use std::collections::BTreeMap;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

use clap::{Args, Parser, Subcommand};

use rosa::model::VariableId;
use rosa::sim::{Assignments, Event, Simulator};
use rosa::solve::{self, Options};
use rosa::{legacy, report};

/// Step route optimizer for Final Fantasy IV.
///
/// With no subcommand, finds the best route for one seed and prints it in
/// the C++ version's format. The options match the C++ version's.
#[derive(Parser)]
#[command(version, about, args_conflicts_with_subcommands = true)]
struct Cli {
	#[command(subcommand)]
	command: Option<Command>,

	#[command(flatten)]
	solve: SolveArgs,
}

#[derive(Args)]
struct SolveArgs {
	/// Route to load from `DATA_DIR/routes/ROUTE.txt`.
	#[arg(short, long, default_value = "paladin")]
	route: String,

	/// Seed to solve.
	#[arg(short, long, default_value_t = 0)]
	seed: u8,

	/// Maximum number of extra steps per path.
	#[arg(short, long, default_value_t = 0)]
	maximum_steps: u32,

	/// Maximum number of paths that may take extra steps (negative for no
	/// limit).
	#[arg(short = 'n', long, default_value_t = -1, allow_negative_numbers = true)]
	maximum_step_segments: i32,

	/// Variable constraints, as space-separated `variable:value` or
	/// `variable:minimum-maximum` with hexadecimal variable names.
	#[arg(short, long, default_value = "")]
	variables: String,

	/// Use minimum rather than average battle lengths.
	#[arg(short, long)]
	tas_mode: bool,

	/// With a segment limit, prefer fewer paths with extra steps.
	#[arg(short, long)]
	prefer_fewer_locations: bool,

	/// Directory containing routes/, maps/ and encounters/.
	#[arg(long, default_value = "data")]
	data_dir: PathBuf,

	/// Print the full result as JSON.
	#[arg(long)]
	json: bool,

	/// Print the cache size to standard error every SECONDS while solving,
	/// and once at the end.
	#[arg(long, value_name = "SECONDS")]
	progress: Option<u64>,
}

#[derive(Subcommand)]
enum Command {
	/// Solve one seed once per value of a variable, writing each result in
	/// the C++ format to `OUT/SEED-VALUE.txt`.
	Sweep {
		#[command(flatten)]
		solve: SolveArgs,

		/// The variable to sweep (hexadecimal, as in `-v`).
		#[arg(long)]
		variable: String,

		/// Values to try, as `minimum-maximum` (default: 0 to the maximum
		/// steps).
		#[arg(long)]
		values: Option<String>,

		/// Output directory.
		#[arg(long)]
		out: PathBuf,
	},
	/// Replay a route with fixed variable values and report its time.
	Replay {
		/// Route to load from `DATA_DIR/routes/ROUTE.txt`.
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

/// Parses `-v` constraints such as `E000202:0 E302500:0-2`.
fn parse_constraints(text: &str) -> Result<BTreeMap<VariableId, (u32, u32)>, String> {
	text.split_whitespace()
		.map(|pair| {
			let invalid = || format!("expected variable:value or variable:minimum-maximum, found {pair:?}");
			let (variable, range) = pair.split_once(':').ok_or_else(invalid)?;
			let variable = u32::from_str_radix(variable, 16).map_err(|_| invalid())?;
			let (minimum, maximum) = range.split_once('-').unwrap_or((range, range));
			let minimum = minimum.parse().map_err(|_| invalid())?;
			let maximum = maximum.parse().map_err(|_| invalid())?;
			Ok((VariableId(variable), (minimum, maximum)))
		})
		.collect()
}

fn options(args: &SolveArgs) -> Result<Options, String> {
	Ok(Options {
		maximum_steps: args.maximum_steps,
		maximum_step_segments: u16::try_from(args.maximum_step_segments).ok(),
		prefer_fewer_locations: args.prefer_fewer_locations,
		tas_mode: args.tas_mode,
		constraints: parse_constraints(&args.variables)?,
		progress: args.progress.map(std::time::Duration::from_secs),
	})
}

/// Reports the final cache size when `--progress` is set.
fn report_cache(args: &SolveArgs, cache: solve::CacheStats) {
	if args.progress.is_some() {
		eprintln!(
			"cache: {} states, {} slots, {:.2} GiB",
			cache.entries,
			cache.slots,
			solve::gibibytes(cache.bytes)
		);
	}
}

const VERSION: &str = concat!("rust-", env!("CARGO_PKG_VERSION"));

fn run_sweep(
	args: &SolveArgs,
	variable: &str,
	values: Option<&str>,
	out: &Path,
) -> Result<(), Box<dyn std::error::Error>> {
	let options = options(args)?;
	let variable = VariableId(u32::from_str_radix(variable, 16).map_err(|_| format!("invalid variable {variable:?}"))?);
	let values = match values {
		Some(text) => {
			let (low, high) = text.split_once('-').unwrap_or((text, text));
			low.parse()?..=high.parse()?
		}
		None => 0..=options.maximum_steps,
	};
	let loaded = legacy::load(&args.data_dir, &args.route)?;
	std::fs::create_dir_all(out)?;

	let mut largest = solve::CacheStats::default();

	for (value, solution) in solve::sweep(&loaded.route, &loaded.data, &options, args.seed, variable, values)? {
		if solution.cache.bytes > largest.bytes {
			largest = solution.cache;
		}

		let mut fixed = options.clone();
		fixed.constraints.insert(variable, (value, value));
		let text = report::text(&solution, &loaded.route, &loaded.data, &fixed, VERSION);
		std::fs::write(out.join(format!("{:03}-{value:03}.txt", args.seed)), text)?;
		println!("{value}\t{}", solution.frames.0);
	}

	report_cache(args, largest);

	Ok(())
}

fn run_solve(args: &SolveArgs) -> Result<(), Box<dyn std::error::Error>> {
	let options = options(args)?;
	let loaded = legacy::load(&args.data_dir, &args.route)?;
	let solution = solve::solve(&loaded.route, &loaded.data, &options, args.seed)?;
	report_cache(args, solution.cache);

	if args.json {
		println!("{}", serde_json::to_string_pretty(&solution)?);
	} else {
		let version = concat!("rust-", env!("CARGO_PKG_VERSION"));
		print!(
			"{}",
			report::text(&solution, &loaded.route, &loaded.data, &options, version)
		);
	}

	Ok(())
}

fn parse_assignments(text: &str) -> Result<Assignments, String> {
	text.split_whitespace()
		.map(|pair| {
			let (variable, value) = pair
				.split_once(':')
				.ok_or_else(|| format!("expected variable:value, found {pair:?}"))?;
			let variable = u32::from_str_radix(variable, 16).map_err(|_| format!("invalid variable {variable:?}"))?;
			let value = value.parse().map_err(|_| format!("invalid value in {pair:?}"))?;
			Ok((VariableId(variable), value))
		})
		.collect()
}

fn main() -> ExitCode {
	let cli = Cli::parse();

	let result = match cli.command {
		None => run_solve(&cli.solve),
		Some(Command::Sweep {
			solve,
			variable,
			values,
			out,
		}) => run_sweep(&solve, &variable, values.as_deref(), &out),
		Some(Command::Replay {
			route,
			seed,
			variables,
			tas_mode,
			data_dir,
			json,
		}) => run_replay(&route, seed, &variables, tas_mode, &data_dir, json),
	};

	match result {
		Ok(()) => ExitCode::SUCCESS,
		Err(error) => {
			eprintln!("error: {error}");
			ExitCode::FAILURE
		}
	}
}

fn run_replay(
	route: &str,
	seed: u8,
	variables: &str,
	tas_mode: bool,
	data_dir: &Path,
	json: bool,
) -> Result<(), Box<dyn std::error::Error>> {
	let assignments = parse_assignments(variables)?;
	let loaded = legacy::load(data_dir, route)?;
	let simulator = Simulator::new(&loaded.route, &loaded.data, tas_mode);
	let outcome = simulator.replay(seed, &assignments)?;

	if json {
		println!("{}", serde_json::to_string_pretty(&outcome)?);
		return Ok(());
	}

	let variables: Vec<String> = outcome
		.variables
		.iter()
		.map(|(variable, value)| format!("{variable}:{value}"))
		.collect();

	println!("Route:      {} (version {})", loaded.route.title, loaded.route.version);
	println!("Seed:       {seed}");
	println!("Variables:  {}", variables.join(" "));
	println!("Frames:     {}", outcome.frames.0);
	println!("Encounters: {} ({})", outcome.encounters, outcome.encounter_frames);
	println!("Total time: {}", outcome.frames);
	println!();

	for event in &outcome.events {
		match event {
			Event::Path {
				map,
				step_seed,
				step_index,
				extra_steps,
				optional_steps,
				encounters,
				..
			} => {
				let description = &loaded.data.map(*map).description;
				println!("{description:<60} Seed: {step_seed:3}   Index: {step_index:3}");

				if optional_steps + extra_steps > 0 {
					println!("  Optional steps: {optional_steps}, extra steps: {extra_steps}");
				}

				for encounter in encounters {
					let formation = &loaded.data.formations[&encounter.formation].description;
					println!(
						"  Step {:3}: {} / {formation} ({})",
						encounter.step,
						u16::from(encounter.encounter_index) + 1,
						encounter.frames
					);
				}
			}
			Event::Choice { label, .. } => println!("{label}"),
			Event::Delay { .. } => {}
		}
	}

	Ok(())
}
