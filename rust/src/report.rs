//! Route output in the C++ version's text layout.
//!
//! This is the format the community tools and website read today. The
//! layout, rounding and odd spots (such as the `FFFFFFFF` variable and the
//! parenthesised look-ahead lines) follow the C++ version exactly.

use std::fmt::Write as _;

use crate::data::GameData;
use crate::model::Route;
use crate::sim::EncounterRecord;
use crate::solve::{EntryKind, Options, Solution};
use crate::time::Milliframes;

/// Formats a solution as the C++ version prints it. `version` fills the
/// `ROSA` line.
#[must_use]
#[expect(clippy::too_many_lines, reason = "one line of output after another")]
pub fn text(solution: &Solution, route: &Route, data: &GameData, options: &Options, version: &str) -> String {
	let mut body = String::new();
	let mut encounter_frames = Milliframes::ZERO;
	let mut total_optional_steps = 0;
	let mut total_extra_steps = 0;
	let mut total_encounters = 0;

	let formation = |record: &EncounterRecord| {
		data.formations
			.get(&record.formation)
			.map_or("", |formation| formation.description.as_str())
	};

	for entry in &solution.entries {
		let description = match &entry.kind {
			EntryKind::Note { text } => text.as_str(),
			EntryKind::Search { label } | EntryKind::Choice { label, .. } => label.as_str(),
			EntryKind::Path { map, .. } => data.map(*map).description.as_str(),
			EntryKind::Delay => "",
		};

		if !description.is_empty() {
			let indented = format!("{}{description}", "  ".repeat(entry.depth));
			let _ = writeln!(
				body,
				"{indented:<78}Seed: {:3}   Index: {:3}",
				entry.step_seed, entry.step_index
			);
		}

		let EntryKind::Path {
			optional_steps,
			extra_steps,
			encounters,
			lookahead,
			..
		} = &entry.kind
		else {
			continue;
		};

		let indent = "  ".repeat(entry.depth + 1);

		if let Some(optional_steps) = optional_steps {
			total_optional_steps += optional_steps;
			let _ = writeln!(body, "{indent}Optional Steps: {optional_steps}");
		}

		total_extra_steps += extra_steps;

		if *extra_steps > 0 {
			let _ = writeln!(body, "{indent}Extra Steps: {extra_steps}");
		}

		// The C++ version logs encounters and look-ahead together, ordered by
		// step; look-ahead steps all come after the path's own.
		for record in encounters {
			let _ = writeln!(
				body,
				"{indent}Step {:3}: {} / {} ({:.3}s)",
				record.step,
				u16::from(record.encounter_index) + 1,
				formation(record),
				record.frames.as_seconds()
			);
			encounter_frames += record.frames;
			total_encounters += 1;
		}

		for record in lookahead {
			let _ = writeln!(
				body,
				"{}(Step {:3}: {} / {})",
				&indent[1..],
				record.step,
				u16::from(record.encounter_index) + 1,
				formation(record)
			);
		}
	}

	let variables: Vec<String> = solution
		.variables
		.iter()
		.map(|(variable, value)| match variable {
			Some(variable) => format!("{variable}:{value}"),
			None => format!("FFFFFFFF:{value}"),
		})
		.collect();

	let total = solution.frames;
	let base = &solution.base;
	let line = |label: &str, value: String| format!("{:<21}{value}\n", format!("{label}:"));
	let seconds = |frames: Milliframes| format!("{:.3}s", frames.as_seconds());

	let mut output = String::new();
	let _ = writeln!(output, "ROUTE\t{}", route.title);
	let _ = writeln!(output, "VERSION\t{}", route.version);
	let _ = writeln!(output, "ROSA\t{version}");
	let _ = writeln!(output, "SEED\t{}", solution.seed);
	let _ = writeln!(output, "MAXSTEP\t{}", options.maximum_steps);
	let _ = writeln!(
		output,
		"MAXSEG\t{}",
		options.maximum_step_segments.map_or(-1, i32::from)
	);
	let _ = writeln!(output, "TASMODE\t{}", u8::from(options.tas_mode));
	let _ = writeln!(
		output,
		"MINIMUM\t{}",
		u8::from(options.maximum_step_segments.is_some() && options.prefer_fewer_locations)
	);
	let _ = writeln!(output, "FRAMES\t{}", total.0);
	let _ = writeln!(output, "VARS\t{}\n", variables.join(" "));
	output += &body;
	output += "\n";
	output += &line("Encounter Time", seconds(encounter_frames));
	output += &line("Other Time", seconds(total - encounter_frames));
	output += &line("Total Time", seconds(total));
	output += "\n";
	output += &line("Base Total Time", seconds(base.frames));
	output += &line("Time Saved", seconds(base.frames - total));
	output += "\n";
	output += &line("Optional Steps", total_optional_steps.to_string());
	output += &line("Extra Steps", total_extra_steps.to_string());
	output += &line("Encounters", total_encounters.to_string());
	output += "\n";
	output += &line("Base Encounters", base.encounters.to_string());
	output += &line(
		"Encounters Saved",
		(i64::try_from(base.encounters).unwrap_or(i64::MAX) - i64::from(total_encounters)).to_string(),
	);
	output += "\n";
	output += &line("Number of Variables", solution.variable_count.to_string());
	output
}
