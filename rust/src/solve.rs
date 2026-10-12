//! The solver: finds the variable values that minimise a route's total time.
//!
//! The route is compiled into a flat program of operations, and the solver
//! searches it depth first. At each decision (extra steps on a path, or the
//! option taken at a choice) it tries every allowed value from the lowest up,
//! keeps the cheapest, and remembers the result for that state in a cache held
//! in RAM. Ties keep the lowest value, as the C++ version does, so results
//! match it exactly.
//!
//! The work differs from the C++ engine in two ways that do not change
//! results. Only decisions are cached, not every instruction, which keeps the
//! cache smaller. And the candidate step counts on a path are walked
//! incrementally: the steps for value `n + 1` continue from where value `n`
//! stopped instead of starting over, so a path costs O(m) steps rather than
//! O(m²).

use std::collections::{BTreeMap, HashMap};
use std::hash::{BuildHasher, Hash, Hasher};

use serde::Serialize;

use crate::data::GameData;
use crate::model::{Choice, FormationId, MAX_SEARCH_TARGETS, Node, Path, Route, Search, VariableId};
use crate::rng::{GROUP_SLOTS, Rng};
use crate::sim::{EncounterRecord, SimError, Simulator, search_assign, search_check, split_steps};
use crate::time::Milliframes;

const FRAMES_PER_TRANSITION: i64 = 82;
const FRAMES_PER_TILE: i64 = 16;

/// Number of steps before the step generator repeats a position. Within a
/// search, the route output lists upcoming encounters up to this many steps
/// into the path.
const LOOKAHEAD_STEPS: u32 = 256;

/// Highest value tried while waiting for a search to finish. The C++ version
/// has no limit and would loop forever on a search that can never finish.
const MAX_VALUE: u32 = u16::MAX as u32;

/// Cost of a route that cannot be finished.
const INFINITE: Milliframes = Milliframes(i64::MAX);

/// Solver settings. These correspond to the C++ command-line options.
#[derive(Clone, Debug, Default)]
pub struct Options {
	/// Most extra steps on any one path (`-m`). Choices are only explored
	/// when this is above zero.
	pub maximum_steps: u32,
	/// Most paths that may take extra steps (`-n`); `None` for no limit.
	pub maximum_step_segments: Option<u16>,
	/// With a segment limit, try every limit from zero up and keep the best
	/// (`-p`), rather than only the given limit.
	pub prefer_fewer_locations: bool,
	/// Use minimum rather than average battle lengths (`-t`).
	pub tas_mode: bool,
	/// Explicit `(minimum, maximum)` bounds for variables (`-v`).
	pub constraints: BTreeMap<VariableId, (u32, u32)>,
}

/// One printed step of the route, in the order the C++ version logs them.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Entry {
	/// Nesting level inside choices and searches.
	pub depth: usize,
	#[serde(flatten)]
	pub kind: EntryKind,
	/// Step generator position on entry.
	pub step_seed: u8,
	pub step_index: u8,
	pub frames: Milliframes,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum EntryKind {
	Note {
		text: String,
	},
	Delay,
	Search {
		label: String,
	},
	Choice {
		variable: Option<VariableId>,
		option: usize,
		label: String,
	},
	Path {
		map: crate::model::MapId,
		variable: Option<VariableId>,
		/// Steps taken, including required ones.
		steps: u32,
		/// Optional steps taken, or `None` where the path has none to take.
		optional_steps: Option<u32>,
		extra_steps: u32,
		encounters: Vec<EncounterRecord>,
		/// Encounters the next steps would bring, listed during a search.
		lookahead: Vec<EncounterRecord>,
	},
}

/// The best route found for one seed.
#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Solution {
	pub seed: u8,
	pub frames: Milliframes,
	/// The segment limit that gave the best result, when one was set.
	pub step_segments: Option<u16>,
	/// The last nonzero value each variable took. `None` collects extra steps
	/// taken on paths without a variable (a search running long), which the
	/// C++ version reports as variable `FFFFFFFF`.
	pub variables: BTreeMap<Option<VariableId>, u32>,
	/// How many variables the C++ version would report.
	pub variable_count: usize,
	pub entries: Vec<Entry>,
	/// The same route with no extra steps and the first option of every
	/// choice.
	pub base: Base,
	/// Number of states held in the cache at the end.
	pub cached_states: usize,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize)]
pub struct Base {
	pub frames: Milliframes,
	pub encounters: usize,
}

impl Solution {
	/// Battles fought and their total length (lookahead excluded).
	#[must_use]
	pub fn encounters(&self) -> (usize, Milliframes) {
		count_encounters(&self.entries)
	}
}

fn count_encounters(entries: &[Entry]) -> (usize, Milliframes) {
	let mut count = 0;
	let mut frames = Milliframes::ZERO;

	for entry in entries {
		if let EntryKind::Path { encounters, .. } = &entry.kind {
			count += encounters.len();
			frames += encounters.iter().map(|e| e.frames).sum();
		}
	}

	(count, frames)
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SolveError {
	Sim(SimError),
	/// A choice variable allows an option the choice does not have.
	OptionOutOfRange {
		variable: VariableId,
		options: usize,
	},
	/// The route has more operations than a cache key can address.
	RouteTooLong,
	/// A search can never finish.
	Unfinishable,
}

impl std::fmt::Display for SolveError {
	fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
		match self {
			Self::Sim(error) => error.fmt(f),
			Self::OptionOutOfRange { variable, options } => {
				write!(f, "choice {variable} allows more options than its {options}")
			}
			Self::RouteTooLong => write!(f, "route is too long"),
			Self::Unfinishable => write!(f, "a search in the route can never finish"),
		}
	}
}

impl std::error::Error for SolveError {}

impl From<SimError> for SolveError {
	fn from(error: SimError) -> Self {
		Self::Sim(error)
	}
}

/// The `(minimum, maximum)` value of every variable, as the C++ engine
/// registers them: the first node using a variable decides whether it counts
/// steps or options, and explicit constraints override both bounds.
#[must_use]
pub fn variable_bounds(route: &Route, options: &Options) -> BTreeMap<VariableId, (u32, u32)> {
	fn visit(nodes: &[Node], maximum_steps: u32, bounds: &mut BTreeMap<VariableId, (u32, u32)>) {
		for node in nodes {
			match node {
				Node::Path(Path {
					variable: Some(variable),
					..
				}) => {
					bounds.entry(*variable).or_insert((0, maximum_steps));
				}
				Node::Choice(choice) => {
					if let Some(variable) = choice.variable {
						let options = u32::try_from(choice.options.len()).unwrap_or(u32::MAX);
						let maximum = if maximum_steps > 0 {
							options.saturating_sub(1)
						} else {
							0
						};
						bounds.entry(variable).or_insert((0, maximum));
					}

					for option in &choice.options {
						visit(&option.body, maximum_steps, bounds);
					}
				}
				_ => {}
			}
		}
	}

	let mut bounds = BTreeMap::new();
	visit(&route.body, options.maximum_steps, &mut bounds);

	for (variable, range) in &options.constraints {
		bounds.insert(*variable, *range);
	}

	bounds
}

/// Finds the best route for one seed.
///
/// # Errors
///
/// Fails if the route refers to data that does not exist, a choice variable
/// allows an option the choice lacks, or a search can never finish.
pub fn solve(route: &Route, data: &GameData, options: &Options, seed: u8) -> Result<Solution, SolveError> {
	let simulator = Simulator::new(route, data, options.tas_mode);
	let bounds = variable_bounds(route, options);
	let program = Program::compile(route, &simulator, &bounds)?;
	let context = Context {
		program: &program,
		durations: &simulator.durations,
		formations: simulator.formation_index.len(),
		limited: options.maximum_step_segments.is_some(),
	};

	let mut cache = Cache::default();
	let initial = State::new(seed);

	// Try each allowed segment limit and keep the best, as the C++ engine
	// does; the cache is shared since the limit is part of each state.
	let limits: Vec<Option<u16>> = match options.maximum_step_segments {
		None => vec![None],
		Some(maximum) if options.prefer_fewer_locations => (0..=maximum).map(Some).collect(),
		Some(maximum) => vec![Some(maximum)],
	};

	let mut best: Option<(Milliframes, Option<u16>)> = None;

	for limit in limits {
		let mut state = initial;

		if let Some(limit) = limit {
			state.remaining = limit;
		}

		let frames = context.solve(&mut cache, 0, state);

		if best.is_none_or(|(best_frames, _)| frames < best_frames) {
			best = Some((frames, limit));
		}
	}

	let (frames, step_segments) = best.expect("at least one segment limit is tried");

	if frames == INFINITE {
		return Err(SolveError::Unfinishable);
	}

	let mut state = initial;

	if let Some(limit) = step_segments {
		state.remaining = limit;
	}

	let trace = context.finalize(&mut cache, state)?;

	let base_bounds = variable_bounds(route, &Options::default())
		.into_keys()
		.map(|variable| (variable, (0, 0)))
		.collect();
	let base_program = Program::compile(route, &simulator, &base_bounds)?;
	let base_context = Context {
		program: &base_program,
		limited: false,
		..context
	};
	let base_trace = base_context.finalize(&mut Cache::default(), state)?;

	let mut variable_count = bounds.len();

	if trace.variables.contains_key(&None) {
		variable_count += 1;
	}

	Ok(Solution {
		seed,
		frames: trace.frames,
		step_segments,
		variables: trace.variables,
		variable_count,
		base: Base {
			frames: base_trace.frames,
			encounters: count_encounters(&base_trace.entries).0,
		},
		entries: trace.entries,
		cached_states: cache.len(),
	})
}

// ---------------------------------------------------------------------------
// Program

#[derive(Clone, Copy, Debug)]
enum Op<'a> {
	Note(&'a str),
	Party(u16),
	Delay(Milliframes),
	Search(usize),
	Path(usize),
	Choice(usize),
	Jump(usize),
	/// End of a choice, after all of its options.
	End,
}

struct PathOp<'a> {
	path: &'a Path,
	rate: u8,
	/// Formation and its battle table column for each slot of the map's
	/// encounter group.
	slots: [(FormationId, usize); GROUP_SLOTS],
	/// Transitions and the required tiles, which every value pays.
	base_frames: Milliframes,
	minimum: u32,
	maximum: u32,
	decision: bool,
}

struct ChoiceOp<'a> {
	choice: &'a Choice,
	/// First operation of each option.
	starts: Vec<usize>,
	minimum: u32,
	maximum: u32,
	decision: bool,
}

struct SearchOp<'a> {
	search: &'a Search,
	party: u16,
}

struct Program<'a> {
	ops: Vec<Op<'a>>,
	paths: Vec<PathOp<'a>>,
	choices: Vec<ChoiceOp<'a>>,
	searches: Vec<SearchOp<'a>>,
}

impl<'a> Program<'a> {
	fn compile(
		route: &'a Route,
		simulator: &Simulator<'a>,
		bounds: &BTreeMap<VariableId, (u32, u32)>,
	) -> Result<Self, SolveError> {
		let mut program = Program {
			ops: Vec::new(),
			paths: Vec::new(),
			choices: Vec::new(),
			searches: Vec::new(),
		};

		program.add(&route.body, simulator, bounds)?;

		if program.ops.len() >= 1 << 16 {
			return Err(SolveError::RouteTooLong);
		}

		Ok(program)
	}

	fn add(
		&mut self,
		nodes: &'a [Node],
		simulator: &Simulator<'a>,
		bounds: &BTreeMap<VariableId, (u32, u32)>,
	) -> Result<(), SolveError> {
		let range = |variable: Option<VariableId>| variable.map_or((0, 0), |v| bounds[&v]);

		for node in nodes {
			match node {
				Node::Note { text } => self.ops.push(Op::Note(text)),
				Node::Party { party } => self.ops.push(Op::Party(simulator.party_index[party])),
				Node::Delay { frames } => self.ops.push(Op::Delay(Milliframes::from_frames(i64::from(*frames)))),
				Node::Search(search) => {
					debug_assert!(search.targets.len() <= MAX_SEARCH_TARGETS);
					self.ops.push(Op::Search(self.searches.len()));
					self.searches.push(SearchOp {
						search,
						party: simulator.party_index[&search.party],
					});
				}
				Node::Path(path) => {
					let map = simulator.resolve_map(path.map)?;
					let (minimum, maximum) = range(path.variable);

					self.ops.push(Op::Path(self.paths.len()));
					self.paths.push(PathOp {
						path,
						rate: map.rate,
						slots: map.slots,
						base_frames: Milliframes::from_frames(
							i64::from(path.transitions) * FRAMES_PER_TRANSITION
								+ i64::from(path.tiles) * FRAMES_PER_TILE,
						),
						minimum,
						maximum,
						decision: maximum > minimum,
					});
				}
				Node::Choice(choice) => {
					let (minimum, maximum) = range(choice.variable);

					if let Some(variable) = choice.variable
						&& usize::try_from(maximum).map_or(true, |m| m >= choice.options.len())
					{
						return Err(SolveError::OptionOutOfRange {
							variable,
							options: choice.options.len(),
						});
					}

					let index = self.choices.len();
					self.ops.push(Op::Choice(index));
					self.choices.push(ChoiceOp {
						choice,
						starts: Vec::new(),
						minimum,
						maximum,
						decision: maximum > minimum,
					});

					let mut starts = Vec::with_capacity(choice.options.len());
					let mut jumps = Vec::with_capacity(choice.options.len());

					for option in &choice.options {
						starts.push(self.ops.len());
						self.add(&option.body, simulator, bounds)?;
						jumps.push(self.ops.len());
						self.ops.push(Op::Jump(0));
					}

					let end = self.ops.len();
					self.ops.push(Op::End);

					for jump in jumps {
						self.ops[jump] = Op::Jump(end);
					}

					self.choices[index].starts = starts;
				}
			}
		}

		Ok(())
	}
}

// ---------------------------------------------------------------------------
// State and cache

const NO_SEARCH: u16 = u16::MAX;

#[derive(Clone, Copy, Debug)]
struct State {
	rng: Rng,
	party: u16,
	/// Paths that may still take extra steps, when that is limited.
	remaining: u16,
	/// The active search, or [`NO_SEARCH`].
	search: u16,
	complete: bool,
	/// Search targets found so far, one bit per target.
	found: u64,
}

impl State {
	fn new(seed: u8) -> Self {
		Self {
			rng: Rng::new(seed),
			party: 0,
			remaining: u16::MAX,
			search: NO_SEARCH,
			complete: false,
			found: 0,
		}
	}

	/// The cache key for this state at operation `pc`.
	///
	/// As in the C++ version, the active search and whether it is complete
	/// are left out: the operation index implies which search is active, and
	/// the targets found imply whether it is complete.
	fn key(&self, pc: usize) -> Key {
		let rng = u64::from(self.rng.step_seed) << 24
			| u64::from(self.rng.step_index) << 16
			| u64::from(self.rng.encounter_seed) << 8
			| u64::from(self.rng.encounter_index);

		Key(
			(pc as u64) << 48 | rng << 16 | u64::from(self.party),
			self.found << 16 | u64::from(self.remaining),
		)
	}
}

#[derive(Clone, Copy, PartialEq, Eq)]
struct Key(u64, u64);

impl Hash for Key {
	fn hash<H: Hasher>(&self, state: &mut H) {
		state.write_u64(fold(
			fold(self.0 ^ 0x243F_6A88_85A3_08D3, 0x1319_8A2E_0370_7344) ^ self.1,
			0xA409_3822_299F_31D0,
		));
	}
}

#[expect(clippy::cast_possible_truncation, reason = "folds the halves of the product")]
fn fold(a: u64, b: u64) -> u64 {
	let product = u128::from(a) * u128::from(b);
	(product as u64) ^ ((product >> 64) as u64)
}

/// Passes through the single `u64` that [`Key`] hashes to.
#[derive(Default)]
struct KeyHasher(u64);

impl Hasher for KeyHasher {
	fn write(&mut self, _bytes: &[u8]) {
		unreachable!("keys hash as one u64");
	}

	fn write_u64(&mut self, value: u64) {
		self.0 = value;
	}

	fn finish(&self) -> u64 {
		self.0
	}
}

#[derive(Clone, Copy, Default)]
struct BuildKeyHasher;

impl BuildHasher for BuildKeyHasher {
	type Hasher = KeyHasher;

	fn build_hasher(&self) -> KeyHasher {
		KeyHasher::default()
	}
}

const FRAME_BITS: u32 = 48;
const FRAME_MASK: u64 = (1 << FRAME_BITS) - 1;

/// Best value and remaining cost for each decision state reached, packed
/// into one `u64` per state.
#[derive(Default)]
struct Cache {
	map: HashMap<Key, u64, BuildKeyHasher>,
}

impl Cache {
	fn get(&self, key: Key) -> Option<(u32, Milliframes)> {
		self.map.get(&key).map(|&packed| {
			let frames = packed & FRAME_MASK;
			let frames = if frames == FRAME_MASK {
				INFINITE
			} else {
				Milliframes(frames.cast_signed())
			};
			((packed >> FRAME_BITS) as u32, frames)
		})
	}

	fn insert(&mut self, key: Key, value: u32, frames: Milliframes) {
		debug_assert!(value <= MAX_VALUE);
		let frames = u64::try_from(frames.0).map_or(FRAME_MASK, |f| f.min(FRAME_MASK));
		self.map.insert(key, u64::from(value) << FRAME_BITS | frames);
	}

	fn len(&self) -> usize {
		self.map.len()
	}
}

// ---------------------------------------------------------------------------
// Search

/// Progress along one path.
#[derive(Clone, Copy)]
struct Walk {
	state: State,
	steps: u32,
	encounters: u32,
	frames: Milliframes,
}

/// Steps and tiles for `value` steps beyond the required ones.
fn extra_steps(path: &Path, value: u32) -> (u32, u32) {
	if value == 0 {
		return (0, 0);
	}

	let mut optional_steps = path.optional_steps.min(value);
	let mut extra_steps = value - optional_steps;

	if extra_steps % 2 == 1 && optional_steps > 0 {
		extra_steps += 1;
		optional_steps -= 1;
	}

	if extra_steps % 2 == 1 && !path.can_single_step {
		extra_steps -= 1;
	}

	let tiles = if path.can_double_step {
		extra_steps
	} else {
		extra_steps * 2
	};

	(optional_steps + extra_steps, tiles + tiles % 2)
}

fn tile_frames(tiles: u32) -> Milliframes {
	Milliframes::from_frames(i64::from(tiles) * FRAMES_PER_TILE)
}

#[derive(Clone, Copy)]
struct Context<'p, 'a> {
	program: &'p Program<'a>,
	durations: &'p [Milliframes],
	formations: usize,
	/// Whether the number of paths with extra steps is limited.
	limited: bool,
}

/// What the logging run of a path records.
struct PathLog {
	encounters: Vec<EncounterRecord>,
	lookahead: Vec<EncounterRecord>,
}

impl Context<'_, '_> {
	fn step(&self, walk: &mut Walk, path: &PathOp<'_>, log: Option<&mut Vec<EncounterRecord>>) {
		walk.steps += 1;

		if !walk.state.rng.step(path.rate) {
			return;
		}

		let (formation, column) = path.slots[walk.state.rng.encounter_slot()];
		let mut battle = self.durations[usize::from(walk.state.party) * self.formations + column];

		if walk.encounters == 0 {
			battle += path.path.first_battle_penalty;
		}

		walk.encounters += 1;
		walk.frames += battle;

		if let Some(log) = log {
			log.push(EncounterRecord {
				step: walk.steps,
				encounter_index: walk.state.rng.encounter_index,
				formation,
				frames: battle,
			});
		}

		if walk.state.search != NO_SEARCH && !walk.state.complete {
			let search = &self.program.searches[usize::from(walk.state.search)];
			search_assign(
				&mut walk.state.found,
				&search.search.targets,
				&search.search.expression,
				formation,
			);

			if search_check(walk.state.found, &search.search.expression) {
				walk.state.complete = true;
				walk.state.party = search.party;
			}
		}

		walk.state.rng.advance_encounter();
	}

	/// Starts a path: transitions, required tiles and required steps.
	fn begin(&self, path: &PathOp<'_>, state: State, mut log: Option<&mut Vec<EncounterRecord>>) -> Walk {
		let mut walk = Walk {
			state,
			steps: 0,
			encounters: 0,
			frames: path.base_frames,
		};

		for _ in 0..path.path.required_steps {
			self.step(&mut walk, path, log.as_deref_mut());
		}

		walk
	}

	/// Finishes a path walked to `value`: checks the search it ends and
	/// counts it against the segment limit. Returns the state after the path,
	/// or `None` if it leaves a search unfinished.
	fn end(&self, path: &PathOp<'_>, value: u32, mut state: State) -> Option<State> {
		if path.path.ends_search {
			if state.search != NO_SEARCH && !state.complete {
				return None;
			}

			// The targets found stay in the state (and the cache key) until
			// the next search starts, as in the C++ version.
			state.search = NO_SEARCH;
		}

		if value > 0 && self.limited && state.remaining > 0 {
			state.remaining -= 1;
		}

		Some(state)
	}

	/// The lowest cost of the route from `pc` on.
	fn solve(&self, cache: &mut Cache, mut pc: usize, mut state: State) -> Milliframes {
		let mut total = Milliframes::ZERO;

		while let Some(op) = self.program.ops.get(pc) {
			match *op {
				Op::Note(_) | Op::End => pc += 1,
				Op::Jump(target) => pc = target,
				Op::Party(party) => {
					state.party = party;
					pc += 1;
				}
				Op::Delay(frames) => {
					total += frames;
					pc += 1;
				}
				Op::Search(index) => {
					state.search = u16::try_from(index).expect("search index fits");
					state.complete = false;
					state.found = 0;
					pc += 1;
				}
				Op::Path(index) => {
					let path = &self.program.paths[index];

					if path.decision {
						return add(total, self.decide_path(cache, pc, path, state).1);
					}

					let Some((_, frames, next)) = self.fixed_path(path, state) else {
						return INFINITE;
					};

					total += frames;
					state = next;
					pc += 1;
				}
				Op::Choice(index) => {
					let choice = &self.program.choices[index];

					if choice.decision {
						return add(total, self.decide_choice(cache, pc, choice, state).1);
					}

					pc = choice.starts[choice.minimum as usize];
				}
			}
		}

		total
	}

	/// Walks a path whose value is fixed, taking more steps if a search it
	/// ends is not finished yet. Returns the value used, the path's cost and
	/// the state after it.
	fn fixed_path(&self, path: &PathOp<'_>, state: State) -> Option<(u32, Milliframes, State)> {
		let mut walk = self.begin(path, state, None);
		let required = path.path.required_steps;

		for value in path.minimum..=MAX_VALUE {
			let (steps, tiles) = extra_steps(path.path, value);

			while walk.steps < required + steps {
				self.step(&mut walk, path, None);
			}

			if let Some(next) = self.end(path, value, walk.state) {
				return Some((value, walk.frames + tile_frames(tiles), next));
			}
		}

		None
	}

	/// Chooses the best number of extra steps for a path at `pc`.
	fn decide_path(&self, cache: &mut Cache, pc: usize, path: &PathOp<'_>, state: State) -> (u32, Milliframes) {
		let key = state.key(pc);

		if let Some(result) = cache.get(key) {
			return result;
		}

		let maximum = if state.remaining == 0 {
			path.minimum
		} else {
			path.maximum
		};
		let required = path.path.required_steps;
		let mut walk = self.begin(path, state, None);
		let mut best = (path.minimum, INFINITE);

		// Values above the maximum are only tried while no value has
		// finished the path's search.
		for value in path.minimum..=MAX_VALUE {
			if value > maximum && best.1 != INFINITE {
				break;
			}

			let (steps, tiles) = extra_steps(path.path, value);

			while walk.steps < required + steps {
				self.step(&mut walk, path, None);
			}

			if let Some(next) = self.end(path, value, walk.state) {
				let frames = add(walk.frames + tile_frames(tiles), self.solve(cache, pc + 1, next));

				if frames < best.1 {
					best = (value, frames);
				}
			}
		}

		cache.insert(key, best.0, best.1);
		best
	}

	/// Chooses the best option of a choice at `pc`.
	fn decide_choice(&self, cache: &mut Cache, pc: usize, choice: &ChoiceOp<'_>, state: State) -> (u32, Milliframes) {
		let key = state.key(pc);

		if let Some(result) = cache.get(key) {
			return result;
		}

		let mut best = (choice.minimum, INFINITE);

		for value in choice.minimum..=choice.maximum {
			let frames = self.solve(cache, choice.starts[value as usize], state);

			if frames < best.1 {
				best = (value, frames);
			}
		}

		cache.insert(key, best.0, best.1);
		best
	}

	/// Replays the decisions the search made, recording every step of the
	/// route.
	#[expect(clippy::too_many_lines, reason = "one arm per operation")]
	fn finalize(&self, cache: &mut Cache, mut state: State) -> Result<Trace, SolveError> {
		let mut trace = Trace {
			frames: Milliframes::ZERO,
			variables: BTreeMap::new(),
			entries: Vec::new(),
		};
		let mut depth = 0usize;
		let mut pc = 0;

		while let Some(op) = self.program.ops.get(pc) {
			let entry = |depth, kind, state: &State, frames| Entry {
				depth,
				kind,
				step_seed: state.rng.step_seed,
				step_index: state.rng.step_index,
				frames,
			};

			match *op {
				Op::Note(text) => {
					trace.entries.push(entry(
						depth,
						EntryKind::Note { text: text.into() },
						&state,
						Milliframes::ZERO,
					));
					pc += 1;
				}
				Op::End => {
					depth = depth.saturating_sub(1);
					pc += 1;
				}
				Op::Jump(target) => pc = target,
				Op::Party(party) => {
					state.party = party;
					pc += 1;
				}
				Op::Delay(frames) => {
					trace.entries.push(entry(depth, EntryKind::Delay, &state, frames));
					trace.frames += frames;
					pc += 1;
				}
				Op::Search(index) => {
					let search = &self.program.searches[index];
					trace.entries.push(entry(
						depth,
						EntryKind::Search {
							label: search.search.label.clone(),
						},
						&state,
						Milliframes::ZERO,
					));
					depth += 1;
					state.search = u16::try_from(index).expect("search index fits");
					state.complete = false;
					state.found = 0;
					pc += 1;
				}
				Op::Choice(index) => {
					let choice = &self.program.choices[index];
					let value = if choice.decision {
						self.decide_choice(cache, pc, choice, state).0
					} else {
						choice.minimum
					};
					let option = value as usize;

					trace.entries.push(entry(
						depth,
						EntryKind::Choice {
							variable: choice.choice.variable,
							option,
							label: choice.choice.options[option].label.clone(),
						},
						&state,
						Milliframes::ZERO,
					));
					depth += 1;

					if value > 0 {
						trace.variables.insert(choice.choice.variable, value);
					}

					pc = choice.starts[option];
				}
				Op::Path(index) => {
					let path = &self.program.paths[index];
					let value = if path.decision {
						self.decide_path(cache, pc, path, state).0
					} else {
						self.fixed_path(path, state).ok_or(SolveError::Unfinishable)?.0
					};

					let (frames, log, steps, next) = self.log_path(path, state, value)?;
					let p = path.path;
					let (optional_steps, extra_steps) = if steps > 0 || p.optional_steps > 0 {
						let (optional, extra) = split_steps(p, steps - p.required_steps);
						((p.optional_steps > 0).then_some(optional), extra)
					} else {
						(None, 0)
					};

					trace.entries.push(entry(
						depth,
						EntryKind::Path {
							map: p.map,
							variable: p.variable,
							steps,
							optional_steps,
							extra_steps,
							encounters: log.encounters,
							lookahead: log.lookahead,
						},
						&state,
						frames,
					));
					trace.frames += frames;

					if p.ends_search {
						depth = depth.saturating_sub(1);
					}

					if value > 0 {
						trace.variables.insert(p.variable, value);
					}

					state = next;
					pc += 1;
				}
			}
		}

		Ok(trace)
	}

	/// Walks a path at a known value, recording its encounters.
	fn log_path(
		&self,
		path: &PathOp<'_>,
		state: State,
		value: u32,
	) -> Result<(Milliframes, PathLog, u32, State), SolveError> {
		let mut log = PathLog {
			encounters: Vec::new(),
			lookahead: Vec::new(),
		};
		let mut walk = self.begin(path, state, Some(&mut log.encounters));
		let (steps, tiles) = extra_steps(path.path, value);

		for _ in 0..steps {
			self.step(&mut walk, path, Some(&mut log.encounters));
		}

		// During a search, the C++ output lists the encounters the following
		// steps would bring, up to one full cycle of the step generator.
		if walk.state.search != NO_SEARCH {
			let lookahead = i64::from(LOOKAHEAD_STEPS) - i64::from(path.path.required_steps) - i64::from(value);
			let mut ahead = walk;

			for _ in 0..lookahead.max(0) {
				self.step(&mut ahead, path, Some(&mut log.lookahead));
			}
		}

		let next = self.end(path, value, walk.state).ok_or(SolveError::Unfinishable)?;
		Ok((walk.frames + tile_frames(tiles), log, walk.steps, next))
	}
}

fn add(a: Milliframes, b: Milliframes) -> Milliframes {
	if a == INFINITE || b == INFINITE {
		INFINITE
	} else {
		a + b
	}
}

struct Trace {
	frames: Milliframes,
	variables: BTreeMap<Option<VariableId>, u32>,
	entries: Vec<Entry>,
}
