//! Spot checks against results from the C++ version (commit f2aeb34).
//!
//! `scripts/compare_cpp.py` runs the full comparison against a C++ build;
//! these few cases keep the essentials checked without one.

use std::path::Path;

use rosa::legacy;
use rosa::model::VariableId;
use rosa::sim::{Assignments, Simulator};

fn data_dir() -> &'static Path {
	Path::new(concat!(env!("CARGO_MANIFEST_DIR"), "/../data"))
}

fn assignments(text: &str) -> Assignments {
	text.split_whitespace()
		.map(|pair| {
			let (variable, value) = pair.split_once(':').unwrap();
			(
				VariableId(u32::from_str_radix(variable, 16).unwrap()),
				value.parse().unwrap(),
			)
		})
		.collect()
}

fn replay(route: &str, seed: u8, variables: &str, tas_mode: bool) -> (i64, Assignments) {
	let loaded = legacy::load(data_dir(), route).unwrap();
	let outcome = Simulator::new(&loaded.route, &loaded.data, tas_mode)
		.replay(seed, &assignments(variables))
		.unwrap();
	(outcome.frames.0, outcome.variables)
}

#[test]
fn every_bundled_route_imports() {
	for entry in data_dir().join("routes").read_dir().unwrap() {
		let path = entry.unwrap().path();

		if path.extension().is_some_and(|e| e == "txt") {
			let name = path.file_stem().unwrap().to_str().unwrap();
			legacy::load(data_dir(), name).unwrap_or_else(|e| panic!("{name}: {e}"));
		}
	}
}

#[test]
fn paladin_seed_0_at_four_steps() {
	let variables = "E000202:4 E000400:4 E302500:1 E302600:4 E308000:2";
	let (frames, chosen) = replay("paladin", 0, variables, false);
	assert_eq!(frames, 41_784_466);
	assert_eq!(chosen, assignments(variables));
}

#[test]
fn search_extends_past_assigned_steps() {
	// With no extra steps allowed, C++ still walks until the Rydia grind
	// search completes, and reports the steps it took.
	let (frames, chosen) = replay("glitchless", 5, "", false);
	assert_eq!(frames, 240_735_689);
	assert_eq!(chosen, assignments("E307000:84"));
}

#[test]
fn tas_mode_with_grind_search() {
	let variables = "E200007:2 E30BC00:24 E317B00:4";
	let (frames, chosen) = replay("no64-excalbur-marathon-recovery-fusoya", 9, variables, true);
	assert_eq!(frames, 43_391_000);
	assert_eq!(chosen, assignments(variables));
}

#[test]
fn choices_select_options() {
	let variables = "C303C00:1 C308F00:1 E000700:4 E000701:1 E000703:1 E302400:4 E302600:3 E302700:3 E303C01:4 \
		E303D00:4 E308700:1 E308702:1 E309200:4";
	let (frames, chosen) = replay("nocw", 200, variables, false);
	assert_eq!(frames, 77_814_378);
	assert_eq!(chosen, assignments(variables));
}
