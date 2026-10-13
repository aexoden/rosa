//! Solves many seeds of one route at once within a memory budget.
//!
//! Each seed runs on its own thread with its own cache. Nearly all the
//! memory a seed uses is its cache, which grows as the solve goes on and is
//! freed when the seed finishes. The farm watches every cache through the
//! solver's progress hook and decides when another seed may start:
//!
//! - Until a seed has finished there is nothing to predict from, so seeds
//!   start one at a time, a fixed interval apart, while the caches fill less
//!   than half the budget.
//! - After that, each seed is expected to grow steadily to the largest peak
//!   seen so far over the median time a seed has taken. A new seed starts
//!   only if, on that forecast, the total stays within the budget until the
//!   running seeds finish. Seeds therefore start staggered, as one would by
//!   hand, and more of them fit than if each reserved its peak from the
//!   start.
//!
//! A forecast can be wrong. If the caches together pass the budget, every
//! seed except the one that started first pauses until enough memory is
//! freed. The first seed always runs, so the farm keeps making progress.

use std::collections::VecDeque;
use std::sync::{Arc, Condvar, Mutex, MutexGuard};
use std::time::{Duration, Instant};

use serde::Serialize;

use crate::data::GameData;
use crate::model::Route;
use crate::solve::{self, CacheStats, Options, ProgressHook, Solution};
use crate::time::Milliframes;

/// How the farm runs.
#[derive(Clone, Debug)]
pub struct Config {
	/// Seeds to solve, in order.
	pub seeds: Vec<u8>,
	/// Seeds already solved, shown as such but not run.
	pub skipped: Vec<u8>,
	/// Most seeds solving at once.
	pub threads: usize,
	/// Most bytes the caches may hold together; `None` for no limit.
	pub memory_budget: Option<usize>,
	/// Time between starts until a seed has finished.
	pub stagger: Duration,
}

/// Where one seed stands.
#[derive(Clone, Debug, Serialize)]
#[serde(tag = "status", rename_all = "snake_case")]
pub enum SeedStatus {
	/// Not part of this run.
	Absent,
	/// Solved before this run started.
	Skipped,
	Pending,
	Running {
		seconds: f64,
		cache: CacheStats,
		peak_bytes: usize,
		paused: bool,
	},
	Done {
		seconds: f64,
		peak_bytes: usize,
		frames: Milliframes,
	},
	Failed {
		error: String,
	},
}

/// The state of the whole farm at one moment.
#[derive(Clone, Debug, Serialize)]
pub struct Snapshot {
	pub seconds: f64,
	pub threads: usize,
	pub memory_budget: Option<usize>,
	/// Bytes held by all running caches.
	pub memory_used: usize,
	/// Expected seconds until every seed is done, once a seed has finished.
	pub remaining_seconds: Option<f64>,
	pub seeds: Vec<SeedStatus>,
	/// Recent events, oldest first.
	pub log: Vec<String>,
	/// Events logged so far, including those no longer in `log`.
	pub logged: u64,
}

impl Snapshot {
	pub fn count(&self, matches: impl Fn(&SeedStatus) -> bool) -> usize {
		self.seeds.iter().filter(|status| matches(status)).count()
	}
}

/// Seeds whose state the farm tracks while they run.
struct Running {
	started: Instant,
	cache: CacheStats,
	peak_bytes: usize,
	paused: bool,
}

enum Slot {
	Absent,
	Skipped,
	Pending,
	Running(Running),
	Done {
		seconds: f64,
		peak_bytes: usize,
		frames: Milliframes,
	},
	Failed(String),
}

struct State {
	started: Instant,
	slots: Vec<Slot>,
	queue: VecDeque<u8>,
	last_start: Option<Instant>,
	log: VecDeque<String>,
	/// Events logged so far, including those dropped from `log`.
	logged: u64,
}

const LOG_LINES: usize = 200;

/// Margin on the largest peak seen, for seeds not yet run.
const PEAK_MARGIN: f64 = 1.05;

impl State {
	fn new(config: &Config) -> Self {
		let mut slots: Vec<Slot> = (0..256).map(|_| Slot::Absent).collect();

		for &seed in &config.skipped {
			slots[usize::from(seed)] = Slot::Skipped;
		}

		for &seed in &config.seeds {
			slots[usize::from(seed)] = Slot::Pending;
		}

		Self {
			started: Instant::now(),
			slots,
			queue: config.seeds.iter().copied().collect(),
			last_start: None,
			log: VecDeque::new(),
			logged: 0,
		}
	}

	fn log(&mut self, message: &str) {
		let elapsed = format_duration(self.started.elapsed().as_secs_f64());
		self.log.push_back(format!("[{elapsed}] {message}"));
		self.logged += 1;

		if self.log.len() > LOG_LINES {
			self.log.pop_front();
		}
	}

	fn running(&self) -> impl Iterator<Item = (u8, &Running)> {
		self.slots.iter().enumerate().filter_map(|(seed, slot)| match slot {
			Slot::Running(running) => Some((u8::try_from(seed).expect("256 slots"), running)),
			_ => None,
		})
	}

	fn memory_used(&self) -> usize {
		self.running().map(|(_, running)| running.cache.bytes).sum()
	}

	/// Largest peak and median time of the finished seeds.
	fn estimates(&self) -> Option<(f64, f64)> {
		let mut done: Vec<(f64, usize)> = self
			.slots
			.iter()
			.filter_map(|slot| match slot {
				Slot::Done {
					seconds, peak_bytes, ..
				} => Some((*seconds, *peak_bytes)),
				_ => None,
			})
			.collect();

		if done.is_empty() {
			return None;
		}

		done.sort_by(|a, b| a.0.total_cmp(&b.0));
		let median = done[done.len() / 2].0.max(1.0);
		#[expect(clippy::cast_precision_loss, reason = "an estimate")]
		let peak = done.iter().map(|&(_, peak)| peak).max().unwrap_or(0) as f64 * PEAK_MARGIN;
		Some((peak, median))
	}

	/// The seed that started first, which never pauses.
	fn eldest(&self) -> Option<u8> {
		self.running()
			.min_by_key(|(_, running)| running.started)
			.map(|(seed, _)| seed)
	}

	fn may_start(&self, config: &Config, now: Instant) -> bool {
		let running = self.running().count();

		if self.queue.is_empty() || running >= config.threads {
			return false;
		}

		let Some(budget) = config.memory_budget else {
			return true;
		};

		if running == 0 {
			return true;
		}

		#[expect(clippy::cast_precision_loss, reason = "an estimate")]
		let budget = budget as f64;

		let Some((peak, median)) = self.estimates() else {
			#[expect(clippy::cast_precision_loss, reason = "an estimate")]
			let used = self.memory_used() as f64;
			let waited = self
				.last_start
				.is_none_or(|last| now.duration_since(last) >= config.stagger);
			return waited && used <= budget / 2.0;
		};

		// Forecast each seed, plus the new one, growing in a straight line
		// to the peak over the median time, and check the total just before
		// each one is due to finish, when it is highest.
		#[expect(clippy::cast_precision_loss, reason = "an estimate")]
		let seeds: Vec<(f64, f64)> = self
			.running()
			.map(|(_, running)| {
				(
					now.duration_since(running.started).as_secs_f64(),
					running.cache.bytes as f64,
				)
			})
			.chain(std::iter::once((0.0, 0.0)))
			.collect();

		let remaining = |elapsed: f64| (median - elapsed).max(median * 0.05);
		let forecast = |at: f64| -> f64 {
			seeds
				.iter()
				.filter(|&&(elapsed, _)| at < remaining(elapsed))
				.map(|&(elapsed, bytes)| bytes.max(peak * ((elapsed + at) / median).min(1.0)))
				.sum()
		};

		seeds
			.iter()
			.map(|&(elapsed, _)| remaining(elapsed) * 0.999)
			.chain(std::iter::once(0.0))
			.all(|at| forecast(at) <= budget)
	}

	fn remaining_seconds(&self, config: &Config) -> Option<f64> {
		let (_, median) = self.estimates()?;
		#[expect(clippy::cast_precision_loss, reason = "an estimate")]
		let pending = self.queue.len() as f64;
		let running: Vec<f64> = self
			.running()
			.map(|(_, running)| (median - running.started.elapsed().as_secs_f64()).max(0.0))
			.collect();
		#[expect(clippy::cast_precision_loss, reason = "an estimate")]
		let parallel = (running.len().max(1) as f64).min(config.threads as f64);
		let current = running.iter().copied().fold(0.0, f64::max);
		Some(current.max(running.iter().sum::<f64>() / parallel) + pending * median / parallel)
	}

	fn snapshot(&self, config: &Config) -> Snapshot {
		let seeds = self
			.slots
			.iter()
			.map(|slot| match slot {
				Slot::Absent => SeedStatus::Absent,
				Slot::Skipped => SeedStatus::Skipped,
				Slot::Pending => SeedStatus::Pending,
				Slot::Running(running) => SeedStatus::Running {
					seconds: running.started.elapsed().as_secs_f64(),
					cache: running.cache,
					peak_bytes: running.peak_bytes,
					paused: running.paused,
				},
				Slot::Done {
					seconds,
					peak_bytes,
					frames,
				} => SeedStatus::Done {
					seconds: *seconds,
					peak_bytes: *peak_bytes,
					frames: *frames,
				},
				Slot::Failed(error) => SeedStatus::Failed { error: error.clone() },
			})
			.collect();

		Snapshot {
			seconds: self.started.elapsed().as_secs_f64(),
			threads: config.threads,
			memory_budget: config.memory_budget,
			memory_used: self.memory_used(),
			remaining_seconds: self.remaining_seconds(config),
			seeds,
			log: self.log.iter().cloned().collect(),
			logged: self.logged,
		}
	}
}

struct Shared {
	config: Config,
	state: Mutex<State>,
	changed: Condvar,
}

impl Shared {
	fn lock(&self) -> MutexGuard<'_, State> {
		self.state.lock().unwrap_or_else(std::sync::PoisonError::into_inner)
	}

	/// Takes the next seed once one may start, or `None` when none are left.
	fn next_seed(&self) -> Option<u8> {
		let mut state = self.lock();

		loop {
			if state.queue.is_empty() {
				return None;
			}

			let now = Instant::now();

			if state.may_start(&self.config, now) {
				let seed = state.queue.pop_front().expect("queue is not empty");
				state.slots[usize::from(seed)] = Slot::Running(Running {
					started: now,
					cache: CacheStats::default(),
					peak_bytes: 0,
					paused: false,
				});
				state.last_start = Some(now);
				state.log(&format!("seed {seed} started"));
				self.changed.notify_all();
				return Some(seed);
			}

			state = self
				.changed
				.wait_timeout(state, Duration::from_secs(1))
				.unwrap_or_else(std::sync::PoisonError::into_inner)
				.0;
		}
	}

	/// Records a seed's cache size, pausing it while the caches are over
	/// budget and it is not the eldest seed.
	fn progress(&self, seed: u8, cache: CacheStats) {
		let mut state = self.lock();
		let mut paused = false;

		loop {
			if let Slot::Running(running) = &mut state.slots[usize::from(seed)] {
				running.cache = cache;
				running.peak_bytes = running.peak_bytes.max(cache.bytes);
				running.paused = paused;
			}

			let over = self
				.config
				.memory_budget
				.is_some_and(|budget| state.memory_used() > budget);

			if !over || state.eldest() == Some(seed) {
				if paused {
					state.log(&format!("seed {seed} resumed"));
					self.changed.notify_all();
				}

				return;
			}

			if !paused {
				paused = true;
				state.log(&format!("seed {seed} paused: caches over budget"));
				self.changed.notify_all();
				continue;
			}

			state = self
				.changed
				.wait_timeout(state, Duration::from_secs(1))
				.unwrap_or_else(std::sync::PoisonError::into_inner)
				.0;
		}
	}

	fn finish(&self, seed: u8, result: Result<(f64, Milliframes), String>) {
		let mut state = self.lock();
		let peak_bytes = match &state.slots[usize::from(seed)] {
			Slot::Running(running) => running.peak_bytes,
			_ => 0,
		};

		state.slots[usize::from(seed)] = match result {
			Ok((seconds, frames)) => {
				state.log(&format!(
					"seed {seed} done in {}, cache peak {:.2} GiB",
					format_duration(seconds),
					solve::gibibytes(peak_bytes)
				));
				Slot::Done {
					seconds,
					peak_bytes,
					frames,
				}
			}
			Err(error) => {
				state.log(&format!("seed {seed} failed: {error}"));
				Slot::Failed(error)
			}
		};

		self.changed.notify_all();
	}
}

/// Solves every seed in `config`, calling `finish` with each solution as it
/// completes and `observe` with a snapshot about once a second.
///
/// `finish` runs on the seed's thread; an error from it marks the seed
/// failed.
pub fn run(
	route: &Route,
	data: &GameData,
	options: &Options,
	config: Config,
	finish: &(dyn Fn(u8, &Solution) -> Result<(), String> + Sync),
	mut observe: impl FnMut(&Snapshot),
) -> Snapshot {
	let threads = config.threads.max(1);
	let shared = Arc::new(Shared {
		state: Mutex::new(State::new(&config)),
		config,
		changed: Condvar::new(),
	});

	std::thread::scope(|scope| {
		let workers: Vec<_> = (0..threads)
			.map(|_| {
				let shared = Arc::clone(&shared);
				scope.spawn(move || {
					while let Some(seed) = shared.next_seed() {
						let hook_shared = Arc::clone(&shared);
						let options = Options {
							progress: Some(ProgressHook::new(move |cache| hook_shared.progress(seed, cache))),
							..options.clone()
						};
						let started = Instant::now();
						let result = solve::solve(route, data, &options, seed)
							.map_err(|error| error.to_string())
							.and_then(|solution| {
								finish(seed, &solution)?;
								Ok((started.elapsed().as_secs_f64(), solution.frames))
							});
						shared.finish(seed, result);
					}
				})
			})
			.collect();

		while workers.iter().any(|worker| !worker.is_finished()) {
			observe(&shared.lock().snapshot(&shared.config));
			std::thread::sleep(Duration::from_secs(1));
		}
	});

	let snapshot = shared.lock().snapshot(&shared.config);
	observe(&snapshot);
	snapshot
}

/// Formats seconds as `H:MM:SS`, or `Dd H:MM:SS` past a day.
#[must_use]
pub fn format_duration(seconds: f64) -> String {
	#[expect(
		clippy::cast_possible_truncation,
		clippy::cast_sign_loss,
		reason = "durations are small and positive"
	)]
	let total = seconds.max(0.0).round() as u64;
	let (days, rest) = (total / 86_400, total % 86_400);
	let clock = format!("{}:{:02}:{:02}", rest / 3600, rest / 60 % 60, rest % 60);

	if days > 0 { format!("{days}d {clock}") } else { clock }
}

/// Draws a snapshot for a terminal at least 132 columns wide, with ANSI
/// colours: a grid of all 256 seeds, a summary, the running seeds and the
/// latest events.
#[must_use]
pub fn render(snapshot: &Snapshot, title: &str, height: usize) -> String {
	use std::fmt::Write;

	const RESET: &str = "\x1b[0m";
	let mut out = String::new();

	let done = snapshot.count(|status| matches!(status, SeedStatus::Done { .. }));
	let skipped = snapshot.count(|status| matches!(status, SeedStatus::Skipped));
	let failed = snapshot.count(|status| matches!(status, SeedStatus::Failed { .. }));
	let pending = snapshot.count(|status| matches!(status, SeedStatus::Pending));
	let running = snapshot.count(|status| matches!(status, SeedStatus::Running { .. }));
	let paused = snapshot.count(|status| matches!(status, SeedStatus::Running { paused: true, .. }));

	let _ = writeln!(
		out,
		"\x1b[1m{title}{RESET}   elapsed {}",
		format_duration(snapshot.seconds)
	);

	let budget = snapshot.memory_budget.map_or_else(
		|| "no limit".to_owned(),
		|budget| format!("{:.2} GiB", solve::gibibytes(budget)),
	);
	let remaining = snapshot
		.remaining_seconds
		.map_or_else(|| "unknown until a seed finishes".to_owned(), format_duration);
	let _ = writeln!(
		out,
		"running {running}/{} ({paused} paused)   cache memory {:.2} GiB of {budget}   done {done}, pending {pending}, failed {failed}, skipped {skipped}   remaining {remaining}",
		snapshot.threads,
		solve::gibibytes(snapshot.memory_used),
	);
	out.push('\n');

	for row in 0..8 {
		for column in 0..32 {
			let seed = row * 32 + column;
			let colour = match &snapshot.seeds[seed] {
				SeedStatus::Absent => "\x1b[2;90m",
				SeedStatus::Skipped => "\x1b[34m",
				SeedStatus::Pending => "\x1b[37m",
				SeedStatus::Running { paused: true, .. } => "\x1b[1;35m",
				SeedStatus::Running { .. } => "\x1b[1;33m",
				SeedStatus::Done { .. } => "\x1b[32m",
				SeedStatus::Failed { .. } => "\x1b[1;31m",
			};
			let _ = write!(out, "{colour}{seed:4}{RESET}");
		}

		out.push('\n');
	}

	let _ = writeln!(
		out,
		"\x1b[37mpending\x1b[0m  \x1b[1;33mrunning\x1b[0m  \x1b[1;35mpaused\x1b[0m  \x1b[32mdone\x1b[0m  \x1b[1;31mfailed\x1b[0m  \x1b[34mskipped\x1b[0m\n"
	);

	let _ = writeln!(
		out,
		"{:>5} {:>12} {:>14} {:>10} {:>10}",
		"seed", "elapsed", "states", "cache GiB", ""
	);
	let mut lines = 13;

	for (seed, status) in snapshot.seeds.iter().enumerate() {
		if let SeedStatus::Running {
			seconds, cache, paused, ..
		} = status
		{
			let _ = writeln!(
				out,
				"{seed:>5} {:>12} {:>14} {:>10.2} {:>10}",
				format_duration(*seconds),
				cache.entries,
				solve::gibibytes(cache.bytes),
				if *paused { "paused" } else { "" },
			);
			lines += 1;
		}
	}

	out.push('\n');
	lines += 1;
	let room = height.saturating_sub(lines + 1).max(3);

	for line in snapshot.log.iter().rev().take(room).rev() {
		let _ = writeln!(out, "{line}");
	}

	out
}

#[cfg(test)]
mod tests {
	use super::*;

	fn config(budget: usize) -> Config {
		Config {
			seeds: (0..8).collect(),
			skipped: Vec::new(),
			threads: 8,
			memory_budget: Some(budget),
			stagger: Duration::from_secs(60),
		}
	}

	fn running(state: &mut State, seed: u8, started: Instant, bytes: usize) {
		state.queue.retain(|&queued| queued != seed);
		state.slots[usize::from(seed)] = Slot::Running(Running {
			started,
			cache: CacheStats {
				bytes,
				..CacheStats::default()
			},
			peak_bytes: bytes,
			paused: false,
		});
	}

	#[test]
	fn staggers_starts_before_any_seed_finishes() {
		let config = config(1000);
		let mut state = State::new(&config);
		let now = Instant::now();
		assert!(state.may_start(&config, now));

		running(&mut state, 0, now, 100);
		state.last_start = Some(now);
		assert!(!state.may_start(&config, now + Duration::from_secs(30)));
		assert!(state.may_start(&config, now + Duration::from_secs(60)));

		running(&mut state, 0, now, 600);
		assert!(!state.may_start(&config, now + Duration::from_secs(60)));
	}

	#[test]
	fn forecasts_from_finished_seeds() {
		let config = config(1000);
		let mut state = State::new(&config);
		let now = Instant::now();
		state.queue.retain(|&seed| seed != 7);
		state.slots[7] = Slot::Done {
			seconds: 100.0,
			peak_bytes: 400,
			frames: Milliframes(0),
		};

		let later = now + Duration::from_secs(90);

		// A seed halfway through, at 200 bytes, will reach 420 by its end;
		// a new seed would be at 210 then. Together they fit.
		running(&mut state, 0, now + Duration::from_secs(40), 200);
		assert!(state.may_start(&config, later));

		// Two seeds near their peaks leave no room for a third.
		running(&mut state, 1, now, 380);
		running(&mut state, 2, now, 380);
		assert!(!state.may_start(&config, later));
	}
}
