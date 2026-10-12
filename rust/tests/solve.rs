//! Spot checks of the solver against reference results from the C++ version
//! (commit f2aeb34, in `reference/results`).
//!
//! `scripts/reference.py check` runs the full comparison; these few cases
//! cover each kind of option without it.

use std::collections::BTreeMap;
use std::path::Path;

use rosa::legacy;
use rosa::model::VariableId;
use rosa::sim::{Assignments, Simulator};
use rosa::solve::{self, Options, Solution};

fn data_dir() -> &'static Path {
	Path::new(concat!(env!("CARGO_MANIFEST_DIR"), "/../data"))
}

fn variables(text: &str) -> BTreeMap<Option<VariableId>, u32> {
	text.split_whitespace()
		.map(|pair| {
			let (variable, value) = pair.split_once(':').unwrap();
			(
				Some(VariableId(u32::from_str_radix(variable, 16).unwrap())),
				value.parse().unwrap(),
			)
		})
		.collect()
}

fn solve(route: &str, seed: u8, options: &Options) -> Solution {
	let loaded = legacy::load(data_dir(), route).unwrap();
	let solution = solve::solve(&loaded.route, &loaded.data, options, seed).unwrap();

	// Replaying the chosen values must give the same route.
	let assignments: Assignments = solution
		.variables
		.iter()
		.map(|(variable, value)| (variable.unwrap(), *value))
		.collect();
	let outcome = Simulator::new(&loaded.route, &loaded.data, options.tas_mode)
		.replay(seed, &assignments)
		.unwrap();
	assert_eq!(outcome.frames, solution.frames);
	assert_eq!(outcome.encounters, solution.encounters().0);

	solution
}

fn steps(maximum_steps: u32) -> Options {
	Options {
		maximum_steps,
		..Options::default()
	}
}

#[test]
fn paladin_seed_0_at_four_steps() {
	let solution = solve("paladin", 0, &steps(4));
	assert_eq!(solution.frames.0, 41_784_466);
	assert_eq!(
		solution.variables,
		variables("E000202:4 E000400:4 E302500:1 E302600:4 E308000:2")
	);
	assert_eq!(solution.variable_count, 45);
}

#[test]
fn search_runs_past_the_step_limit() {
	let solution = solve("glitchless", 3, &steps(0));
	assert_eq!(solution.frames.0, 246_060_885);
	assert_eq!(solution.variables, variables("E30BC00:290"));
	assert_eq!(solution.base.encounters, 176);
	assert_eq!(solution.variable_count, 242);
}

#[test]
fn tas_mode() {
	let options = Options {
		tas_mode: true,
		..steps(4)
	};
	let solution = solve("no64-excalbur-marathon-recovery-fusoya", 3, &options);
	assert_eq!(solution.frames.0, 45_538_000);
	assert_eq!(
		solution.variables,
		variables("E200009:4 E30B500:1 E30BC00:86 E316402:2 E316700:4 E316A01:4 E316B00:2 E317400:4")
	);
	assert_eq!(solution.base.encounters, 45);
}

#[test]
fn segment_limit_preferring_fewer_locations() {
	let options = Options {
		maximum_step_segments: Some(2),
		prefer_fewer_locations: true,
		..steps(8)
	};
	let solution = solve("nocw", 3, &options);
	assert_eq!(solution.frames.0, 78_881_211);
	assert_eq!(solution.variables, variables("E302500:7 E302600:8"));
	assert_eq!(solution.base.encounters, 69);
}

#[test]
fn variable_constraints() {
	let options = Options {
		constraints: [(VariableId(0xE00_0202), (0, 0)), (VariableId(0xE30_2500), (0, 2))].into(),
		..steps(4)
	};
	let solution = solve("paladin", 3, &options);
	assert_eq!(solution.frames.0, 42_998_231);
	assert_eq!(
		solution.variables,
		variables("E000400:4 E000401:4 E000500:4 E000501:4 E302500:1 E302600:4 E307701:4 E308000:4")
	);
}
