use std::path::PathBuf;
use std::process::ExitCode;

use clap::{Parser, Subcommand};

use rosa::legacy;
use rosa::model::VariableId;
use rosa::sim::{Assignments, Event, Simulator};

#[derive(Parser)]
#[command(version, about)]
struct Cli {
	#[command(subcommand)]
	command: Command,
}

#[derive(Subcommand)]
enum Command {
	/// Replay a route with fixed variable values and report its time.
	Replay {
		/// Route to load from DATA_DIR/routes/ROUTE.txt.
		#[arg(short, long, default_value = "paladin")]
		route: String,

		/// Starting seed.
		#[arg(short, long, default_value_t = 0)]
		seed: u8,

		/// Variable values, as space-separated `variable:value` pairs with
		/// hexadecimal variable names (the VARS line of a C++ route).
		#[arg(short, long, default_value = "")]
		variables: String,

		/// Use minimum rather than average battle lengths.
		#[arg(short, long)]
		tas_mode: bool,

		/// Directory containing routes/, maps/ and encounters/.
		#[arg(long, default_value = "data")]
		data_dir: PathBuf,

		/// Print the full result as JSON.
		#[arg(long)]
		json: bool,
	},
}

fn parse_assignments(text: &str) -> Result<Assignments, String> {
	text.split_whitespace()
		.map(|pair| {
			let (variable, value) = pair.split_once(':').ok_or_else(|| format!("expected variable:value, found {pair:?}"))?;
			let variable = u32::from_str_radix(variable, 16).map_err(|_| format!("invalid variable {variable:?}"))?;
			let value = value.parse().map_err(|_| format!("invalid value in {pair:?}"))?;
			Ok((VariableId(variable), value))
		})
		.collect()
}

fn main() -> ExitCode {
	let Command::Replay { route, seed, variables, tas_mode, data_dir, json } = Cli::parse().command;

	let result = (|| -> Result<(), Box<dyn std::error::Error>> {
		let assignments = parse_assignments(&variables)?;
		let loaded = legacy::load(&data_dir, &route)?;
		let simulator = Simulator::new(&loaded.route, &loaded.data, tas_mode);
		let outcome = simulator.replay(seed, &assignments)?;

		if json {
			println!("{}", serde_json::to_string_pretty(&outcome)?);
			return Ok(());
		}

		let variables: Vec<String> = outcome.variables.iter().map(|(variable, value)| format!("{variable}:{value}")).collect();

		println!("Route:      {} (version {})", loaded.route.title, loaded.route.version);
		println!("Seed:       {seed}");
		println!("Variables:  {}", variables.join(" "));
		println!("Frames:     {}", outcome.frames.0);
		println!("Encounters: {} ({})", outcome.encounters, outcome.encounter_frames);
		println!("Total time: {}", outcome.frames);
		println!();

		for event in &outcome.events {
			match event {
				Event::Path { map, step_seed, step_index, extra_steps, optional_steps, encounters, .. } => {
					let description = &loaded.data.map(*map).description;
					println!("{description:<60} Seed: {step_seed:3}   Index: {step_index:3}");

					if optional_steps + extra_steps > 0 {
						println!("  Optional steps: {optional_steps}, extra steps: {extra_steps}");
					}

					for encounter in encounters {
						let formation = &loaded.data.formations[&encounter.formation].description;
						println!("  Step {:3}: {} / {formation} ({})", encounter.step, u16::from(encounter.encounter_index) + 1, encounter.frames);
					}
				}
				Event::Choice { label, .. } => println!("{label}"),
				Event::Delay { .. } => {}
			}
		}

		Ok(())
	})();

	match result {
		Ok(()) => ExitCode::SUCCESS,
		Err(error) => {
			eprintln!("error: {error}");
			ExitCode::FAILURE
		}
	}
}
