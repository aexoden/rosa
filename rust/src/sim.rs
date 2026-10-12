//! Replays a route with fixed variable values.
//!
//! Given a seed and a value for every variable (extra steps on a path, or the
//! option taken at a choice), this walks the route once and reports the total
//! time and every encounter. The solver will search over these values; this
//! module is the single source of truth for what one assignment costs.

use std::collections::{BTreeMap, HashMap};

use serde::Serialize;

use crate::data::GameData;
use crate::model::{FormationId, MapId, Node, Party, Path, Route, Search, SearchExpr, VariableId};
use crate::rng::{GROUP_SLOTS, Rng};
use crate::time::Milliframes;

const FRAMES_PER_TRANSITION: i64 = 82;
const FRAMES_PER_TILE: i64 = 16;

/// Upper bound on extra steps tried while waiting for a search to finish.
const MAX_SEARCH_STEPS: u32 = 1 << 16;

/// Battle length used when the data has no timing for the current party.
const UNKNOWN_BATTLE: Milliframes = Milliframes::from_seconds(30);

/// The value chosen for each variable. Missing variables take value 0.
pub type Assignments = BTreeMap<VariableId, u32>;

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize)]
pub struct EncounterRecord {
	/// Step within the path on which the encounter happened, counting from 1.
	pub step: u32,
	/// Position of the encounter generator (0-based) before this encounter.
	pub encounter_index: u8,
	pub formation: FormationId,
	pub frames: Milliframes,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Event {
	Path {
		map: MapId,
		variable: Option<VariableId>,
		/// Generator position on entering the path.
		step_seed: u8,
		step_index: u8,
		/// Steps actually taken, including required ones.
		steps: u32,
		optional_steps: u32,
		extra_steps: u32,
		frames: Milliframes,
		encounters: Vec<EncounterRecord>,
	},
	Choice {
		variable: Option<VariableId>,
		option: usize,
		label: String,
	},
	Delay {
		frames: Milliframes,
	},
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Outcome {
	pub seed: u8,
	pub frames: Milliframes,
	pub encounter_frames: Milliframes,
	pub encounters: usize,
	/// Every variable that ended up with a nonzero value. A shared variable
	/// reports the last nonzero value it took.
	pub variables: Assignments,
	pub events: Vec<Event>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SimError {
	/// A search could not be completed within [`MAX_SEARCH_STEPS`].
	SearchNeverCompletes { map: MapId },
	/// A choice was given an option number it does not have.
	NoSuchOption { variable: Option<VariableId>, value: u32 },
	/// The data has no formation for a slot of an encounter group.
	MissingFormation { group: u16, slot: usize },
}

impl std::fmt::Display for SimError {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		match self {
			Self::SearchNeverCompletes { map } => write!(f, "search never completes by the end of map {:04X}", map.0),
			Self::NoSuchOption { variable: Some(variable), value } => write!(f, "choice {variable} has no option {value}"),
			Self::NoSuchOption { variable: None, value } => write!(f, "choice has no option {value}"),
			Self::MissingFormation { group, slot } => write!(f, "encounter group {group} slot {slot} names an unknown formation"),
		}
	}
}

impl std::error::Error for SimError {}

/// Precomputed lookup tables for one route and data set.
///
/// Parties are interned to small indices so that simulation state is a
/// small `Copy` value and battle lengths are a single table lookup.
pub struct Simulator<'a> {
	route: &'a Route,
	data: &'a GameData,
	parties: Vec<Party>,
	party_index: HashMap<Party, u16>,
	formation_index: HashMap<FormationId, usize>,
	/// `durations[party * formations + formation]`
	durations: Vec<Milliframes>,
}

#[derive(Clone, Copy)]
struct ActiveSearch<'a> {
	search: &'a Search,
	party: u16,
	found: u64,
	complete: bool,
}

#[derive(Clone, Copy)]
struct State<'a> {
	rng: Rng,
	party: u16,
	search: Option<ActiveSearch<'a>>,
}

/// A map's encounter rate and the formation in each slot, resolved once per
/// path rather than once per step.
struct ResolvedMap {
	rate: u8,
	slots: [(FormationId, usize); GROUP_SLOTS],
}

impl<'a> Simulator<'a> {
	/// Builds lookup tables. `tas_mode` uses minimum rather than average
	/// battle lengths.
	#[must_use]
	pub fn new(route: &'a Route, data: &'a GameData, tas_mode: bool) -> Self {
		let mut parties = vec![Party::default()];
		collect_parties(&route.body, &mut parties);

		let party_index = parties.iter().enumerate().map(|(i, p)| (*p, u16::try_from(i).expect("too many parties"))).collect();

		let mut formation_ids: Vec<FormationId> = data.formations.keys().copied().collect();
		formation_ids.sort_unstable();
		let formation_index: HashMap<_, _> = formation_ids.iter().enumerate().map(|(i, id)| (*id, i)).collect();

		let mut durations = Vec::with_capacity(parties.len() * formation_ids.len());

		for party in &parties {
			for id in &formation_ids {
				let duration = data.formations[id].duration(party);
				durations.push(duration.map_or(UNKNOWN_BATTLE, |d| if tas_mode { d.minimum } else { d.average }));
			}
		}

		Self { route, data, parties, party_index, formation_index, durations }
	}

	/// Every party the route uses, in order of first appearance.
	#[must_use]
	pub fn parties(&self) -> &[Party] {
		&self.parties
	}

	fn resolve_map(&self, id: MapId) -> Result<ResolvedMap, SimError> {
		let map = self.data.map(id);
		let mut slots = [(FormationId(0), 0); GROUP_SLOTS];

		if map.encounter_rate > 0 {
			let group = self.data.groups.get(&map.encounter_group);

			for (slot, target) in slots.iter_mut().enumerate() {
				let formation = group.map(|g| g[slot]);
				let index = formation.and_then(|f| self.formation_index.get(&f));

				match (formation, index) {
					(Some(formation), Some(&index)) => *target = (formation, index),
					_ => return Err(SimError::MissingFormation { group: map.encounter_group, slot }),
				}
			}
		}

		Ok(ResolvedMap { rate: map.encounter_rate, slots })
	}

	/// Replays the route from the given seed.
	///
	/// A path that ends a search and would otherwise leave it unfinished takes
	/// as many additional steps as it needs, as the C++ solver does.
	///
	/// # Errors
	///
	/// Fails if an assignment names a missing option or a search can never
	/// finish.
	pub fn replay(&self, seed: u8, assignments: &Assignments) -> Result<Outcome, SimError> {
		let mut run = Run {
			sim: self,
			assignments,
			state: State { rng: Rng::new(seed), party: 0, search: None },
			outcome: Outcome {
				seed,
				frames: Milliframes::ZERO,
				encounter_frames: Milliframes::ZERO,
				encounters: 0,
				variables: Assignments::new(),
				events: Vec::new(),
			},
		};

		run.nodes(&self.route.body)?;
		Ok(run.outcome)
	}
}

fn collect_parties(nodes: &[Node], parties: &mut Vec<Party>) {
	fn add(parties: &mut Vec<Party>, party: &Party) {
		if !parties.contains(party) {
			parties.push(*party);
		}
	}

	for node in nodes {
		match node {
			Node::Party { party } => add(parties, party),
			Node::Search(search) => add(parties, &search.party),
			Node::Choice(choice) => {
				for option in &choice.options {
					collect_parties(&option.body, parties);
				}
			}
			Node::Note { .. } | Node::Delay { .. } | Node::Path(_) => {}
		}
	}
}

struct Run<'s, 'a> {
	sim: &'s Simulator<'a>,
	assignments: &'s Assignments,
	state: State<'a>,
	outcome: Outcome,
}

impl<'a> Run<'_, 'a> {
	fn value(&mut self, variable: Option<VariableId>) -> u32 {
		variable.and_then(|v| self.assignments.get(&v).copied()).unwrap_or(0)
	}

	fn record(&mut self, variable: Option<VariableId>, value: u32) {
		if let (Some(variable), 1..) = (variable, value) {
			self.outcome.variables.insert(variable, value);
		}
	}

	fn nodes(&mut self, nodes: &'a [Node]) -> Result<(), SimError> {
		for node in nodes {
			match node {
				Node::Note { .. } => {}
				Node::Party { party } => self.state.party = self.sim.party_index[party],
				Node::Delay { frames } => {
					let frames = Milliframes::from_frames(i64::from(*frames));
					self.outcome.frames += frames;
					self.outcome.events.push(Event::Delay { frames });
				}
				Node::Search(search) => {
					self.state.search =
						Some(ActiveSearch { search, party: self.sim.party_index[&search.party], found: 0, complete: false });
				}
				Node::Choice(choice) => {
					let value = self.value(choice.variable);
					let index = usize::try_from(value).unwrap_or(usize::MAX);
					let option = choice.options.get(index).ok_or(SimError::NoSuchOption { variable: choice.variable, value })?;

					self.record(choice.variable, value);
					self.outcome.events.push(Event::Choice { variable: choice.variable, option: index, label: option.label.clone() });
					self.nodes(&option.body)?;
				}
				Node::Path(path) => self.path(path)?,
			}
		}

		Ok(())
	}

	fn path(&mut self, path: &'a Path) -> Result<(), SimError> {
		let map = self.sim.resolve_map(path.map)?;
		let start = self.state;
		let mut value = self.value(path.variable);

		loop {
			let mut encounters = Vec::new();
			let (frames, steps) = self.walk_path(path, &map, value, &mut encounters);
			let search_failed = path.ends_search && self.state.search.is_some_and(|s| !s.complete);

			if search_failed {
				if value >= MAX_SEARCH_STEPS {
					return Err(SimError::SearchNeverCompletes { map: path.map });
				}

				self.state = start;
				value += 1;
				continue;
			}

			if path.ends_search {
				self.state.search = None;
			}

			let (optional_steps, extra_steps) = split_steps(path, steps - path.required_steps);
			let encounter_frames: Milliframes = encounters.iter().map(|e| e.frames).sum();

			self.record(path.variable, value);
			self.outcome.frames += frames;
			self.outcome.encounter_frames += encounter_frames;
			self.outcome.encounters += encounters.len();
			self.outcome.events.push(Event::Path {
				map: path.map,
				variable: path.variable,
				step_seed: start.rng.step_seed,
				step_index: start.rng.step_index,
				steps,
				optional_steps,
				extra_steps,
				frames,
				encounters,
			});

			return Ok(());
		}
	}

	/// Walks a path with `value` extra steps. Returns the frames spent and the
	/// number of steps taken.
	fn walk_path(&mut self, path: &Path, map: &ResolvedMap, value: u32, encounters: &mut Vec<EncounterRecord>) -> (Milliframes, u32) {
		let mut frames = Milliframes::from_frames(i64::from(path.transitions) * FRAMES_PER_TRANSITION);
		frames += self.walk(path, map, path.tiles, path.required_steps, 0, encounters);
		let mut steps = path.required_steps;

		if value > 0 {
			let mut optional_steps = path.optional_steps.min(value);
			let mut extra_steps = value - optional_steps;

			if extra_steps % 2 == 1 && optional_steps > 0 {
				extra_steps += 1;
				optional_steps -= 1;
			}

			if extra_steps % 2 == 1 && !path.can_single_step {
				extra_steps -= 1;
			}

			let tiles = if path.can_double_step { extra_steps } else { extra_steps * 2 };
			let tiles = tiles + tiles % 2;

			frames += self.walk(path, map, tiles, optional_steps + extra_steps, path.required_steps, encounters);
			steps += optional_steps + extra_steps;
		}

		(frames, steps)
	}

	/// Takes `steps` steps over `tiles` tiles. `step_offset` is the number of
	/// steps already taken on this path.
	fn walk(&mut self, path: &Path, map: &ResolvedMap, tiles: u32, steps: u32, step_offset: u32, encounters: &mut Vec<EncounterRecord>) -> Milliframes {
		let mut frames = Milliframes::from_frames(i64::from(tiles) * FRAMES_PER_TILE);

		for step in 1..=steps {
			if !self.state.rng.step(map.rate) {
				continue;
			}

			let (formation, formation_index) = map.slots[self.state.rng.encounter_slot()];
			let mut battle = self.sim.durations[usize::from(self.state.party) * self.sim.formation_index.len() + formation_index];

			if encounters.is_empty() {
				battle += path.first_battle_penalty;
			}

			frames += battle;
			encounters.push(EncounterRecord {
				step: step_offset + step,
				encounter_index: self.state.rng.encounter_index,
				formation,
				frames: battle,
			});

			if let Some(search) = &mut self.state.search
				&& !search.complete
			{
				let expression = &search.search.expression;
				assign(search, expression, formation);

				if check(search.found, expression) {
					search.complete = true;
					self.state.party = search.party;
				}
			}

			self.state.rng.advance_encounter();
		}

		frames
	}
}

/// Splits steps beyond the required ones into optional and extra steps, as
/// the route output reports them.
fn split_steps(path: &Path, steps: u32) -> (u32, u32) {
	let mut optional_steps = path.optional_steps.min(steps);
	let mut extra_steps = steps - optional_steps;

	if extra_steps % 2 == 1 && optional_steps > 0 {
		optional_steps -= 1;
		extra_steps += 1;
	}

	(optional_steps, extra_steps)
}

fn check(found: u64, expression: &SearchExpr) -> bool {
	match expression {
		SearchExpr::Target(index) => found & (1 << index) != 0,
		SearchExpr::Any(nodes) => nodes.iter().any(|node| check(found, node)),
		SearchExpr::All(nodes) | SearchExpr::Sequence(nodes) => nodes.iter().all(|node| check(found, node)),
	}
}

/// Credits an encounter to the search. These rules reproduce the C++
/// behaviour exactly, including its quirks: under `|`, every alternative is
/// credited and the result is always "not consumed", so an enclosing `+` may
/// credit the same encounter again.
fn assign(search: &mut ActiveSearch<'_>, expression: &SearchExpr, formation: FormationId) -> bool {
	match expression {
		SearchExpr::Target(index) => {
			let bit = 1 << index;

			if search.found & bit == 0 && search.search.targets[*index] == formation {
				search.found |= bit;
				return true;
			}

			false
		}
		SearchExpr::All(nodes) => {
			nodes.iter().any(|node| !check(search.found, node) && assign(search, node, formation))
		}
		SearchExpr::Any(nodes) => {
			for node in nodes {
				assign(search, node, formation);
			}

			false
		}
		SearchExpr::Sequence(nodes) => match nodes.iter().find(|node| !check(search.found, node)) {
			Some(node) => assign(search, node, formation),
			None => false,
		},
	}
}
