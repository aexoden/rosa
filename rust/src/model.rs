//! The format-neutral route model.
//!
//! Nothing here knows about any particular file format. Importers (such as
//! [`crate::legacy`] for the original tab-separated files) build these types,
//! and the simulator and solver only consume them.

use std::fmt;

use serde::{Deserialize, Serialize};

use crate::time::Milliframes;

/// Identifies a decision point (extra steps on a path, or a choice between
/// options). Several nodes may share one variable when they are alternatives
/// of each other, such as the same map segment in two options of a choice.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(transparent)]
pub struct VariableId(pub u32);

impl fmt::Display for VariableId {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{:07X}", self.0)
	}
}

/// A map, identified by its in-game map number.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(transparent)]
pub struct MapId(pub u16);

/// An encounter formation.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(transparent)]
pub struct FormationId(pub u16);

pub const PARTY_SLOTS: usize = 5;

/// One party slot. `character` is 0 for an empty slot.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct Member {
	pub character: u8,
	pub agility: u8,
}

/// The party configuration, which determines how long each battle takes.
///
/// Two parties are the same exactly when every field matches; this mirrors
/// the packed keys the C++ version compares.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct Party {
	pub three_front: bool,
	pub has_gp: bool,
	pub on_world_map: bool,
	pub level: u8,
	pub members: [Member; PARTY_SLOTS],
}

impl Default for Party {
	/// The party in effect before a route sets one.
	fn default() -> Self {
		Self {
			three_front: true,
			has_gp: true,
			on_world_map: true,
			level: 1,
			members: [Member::default(); PARTY_SLOTS],
		}
	}
}

/// A whole route.
#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct Route {
	pub title: String,
	pub version: u32,
	/// Which game data set (maps and encounters) the route uses.
	pub data_set: String,
	pub body: Vec<Node>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Node {
	/// Text shown in the route output; no effect on timing.
	Note {
		text: String,
	},
	/// The party changes from here on.
	Party {
		party: Party,
	},
	/// A fixed delay.
	Delay {
		frames: u32,
	},
	Path(Path),
	Choice(Choice),
	Search(Search),
}

/// Walking through one map.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Path {
	pub variable: Option<VariableId>,
	pub map: MapId,
	pub tiles: u32,
	pub required_steps: u32,
	/// Steps that can be taken without walking any extra tiles.
	pub optional_steps: u32,
	pub transitions: u32,
	pub can_single_step: bool,
	pub can_double_step: bool,
	pub can_step_during_save: bool,
	/// Added to the first encounter on this path.
	pub first_battle_penalty: Milliframes,
	/// The active search must be complete by the end of this path.
	pub ends_search: bool,
}

/// A choice between alternative sub-routes.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Choice {
	pub variable: Option<VariableId>,
	pub options: Vec<ChoiceOption>,
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct ChoiceOption {
	pub label: String,
	pub body: Vec<Node>,
}

/// Walking until certain encounters have been found, after which the party
/// changes. The search must finish by the next path with `ends_search`.
#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Search {
	pub label: String,
	/// The formations the expression refers to, by position.
	pub targets: Vec<FormationId>,
	pub expression: SearchExpr,
	pub party: Party,
}

/// Maximum number of targets one search can track.
pub const MAX_SEARCH_TARGETS: usize = 48;

/// Which encounters satisfy a search.
///
/// Nodes never have exactly one child: a single-element group is replaced by
/// its element. The assignment rules in [`crate::sim`] depend on that.
#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum SearchExpr {
	/// Index into [`Search::targets`].
	Target(usize),
	/// All of these, in order (`a > b`).
	Sequence(Vec<SearchExpr>),
	/// Any one of these (`a | b`).
	Any(Vec<SearchExpr>),
	/// All of these, in any order (`a + b`).
	All(Vec<SearchExpr>),
}

impl SearchExpr {
	/// Builds a group, collapsing a single element into itself.
	#[must_use]
	pub fn group(make: fn(Vec<Self>) -> Self, mut nodes: Vec<Self>) -> Self {
		if nodes.len() == 1 { nodes.remove(0) } else { make(nodes) }
	}
}
