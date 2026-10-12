//! Game data: maps and encounter formations.

use std::collections::HashMap;

use serde::{Deserialize, Serialize};

use crate::model::{FormationId, MapId, Party};
use crate::rng::GROUP_SLOTS;
use crate::time::Milliframes;

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct MapInfo {
	pub encounter_rate: u8,
	pub encounter_group: u16,
	pub title: String,
	pub description: String,
}

/// How long a battle with a formation takes for one party.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct BattleDuration {
	pub average: Milliframes,
	pub minimum: Milliframes,
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct Formation {
	pub description: String,
	pub durations: Vec<(Party, BattleDuration)>,
}

impl Formation {
	#[must_use]
	pub fn duration(&self, party: &Party) -> Option<BattleDuration> {
		self.durations.iter().find(|(p, _)| p == party).map(|&(_, d)| d)
	}
}

#[derive(Clone, Debug, Default, PartialEq, Serialize, Deserialize)]
pub struct GameData {
	pub maps: HashMap<MapId, MapInfo>,
	pub formations: HashMap<FormationId, Formation>,
	/// The formation in each slot of each encounter group.
	pub groups: HashMap<u16, [FormationId; GROUP_SLOTS]>,
}

/// Map used when a route names a map the data does not define. It never
/// produces encounters, as in the C++ version.
pub static UNKNOWN_MAP: MapInfo = MapInfo {
	encounter_rate: 0,
	encounter_group: 0,
	title: String::new(),
	description: String::new(),
};

impl GameData {
	#[must_use]
	pub fn map(&self, id: MapId) -> &MapInfo {
		self.maps.get(&id).unwrap_or(&UNKNOWN_MAP)
	}
}
