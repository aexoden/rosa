# ROSA in Rust

A Rust port of ROSA, in progress. It finds the fastest route for a seed, as the
C++ version does, with the same results, and can also replay a route with
fixed variable values (extra steps per path, options per choice).

## Layout

| Module | Role |
|---|---|
| `model` | Format-neutral route model: paths, choices, searches, parties |
| `data` | Game data: maps, encounter groups, battle lengths |
| `legacy` | Importer for the original tab-separated route and data files |
| `rng` | The FF4 step and encounter generator |
| `sim` | Replays a route for one seed and one set of variable values |
| `solve` | Finds the best variable values for one seed, with an in-memory cache |
| `report` | Prints a solution in the C++ version's text format |
| `farm` | Solves many seeds at once within a memory budget, with a dashboard |
| `time` | Integer milliframe arithmetic |

The core never sees the old file format: `legacy` builds `model` and `data`
values, and a future importer for a richer format can do the same.

## Usage

Run from the repository root so the default `data/` directory is found. With
no subcommand, `rosa` solves a seed and takes the same options as the C++
version (`-r`, `-s`, `-m`, `-n`, `-p`, `-t`, `-v`), printing the same text:

```sh
cargo build --release --manifest-path rust/Cargo.toml
rust/target/release/rosa -r paladin -s 0 -m 4
```

`replay` runs fixed values instead:

```sh
rust/target/release/rosa replay -r paladin -s 0 \
    -v "E000202:4 E000400:4 E302500:1 E302600:4 E308000:2"
```

Add `--json` to either for the full structured result.

`--progress SECONDS` prints the cache size to standard error at that interval
while solving, and once at the end:

```
30 s: 12582912 states, 21168128 slots, 0.45 GiB
...
cache: 41502403 states, 65404928 slots, 1.40 GiB
```

The cache is nearly all of the memory a run uses, so the final line is a good
guide to peak memory per seed. The `cache` field of `--json` output holds the
same figures.

`sweep` solves one seed once per value of a variable, as separate `-v` runs
would, and writes each route to `OUT/SEED-VALUE.txt`. Cache entries after the
variable's last use are shared between values, so sweeping `E305400` (the
first decision in its routes) costs about one solve:

```sh
rust/target/release/rosa sweep -r paladin -s 30 -m 256 --variable E305400 --out work/sweep
```

`rust/scripts/twin_sweep.py -r ROUTE -m STEPS --out DIR` runs that for every
twin pair in `doc/twin-seed-notes.md` and summarises each seed's loss per
shared value, and whether the two routes still split on steps. The C++ cache options
(`-c`, `-l`, `-f`, `-x`) are not ported; the cache always lives in memory.

`farm` replaces `scripts/optimize_route.py` for production runs. It solves a
set of seeds in one process, one thread per seed, and writes each route to
`OUT/SEED.txt` (seeds whose file exists are skipped, so an interrupted farm
resumes where it stopped):

```sh
rust/target/release/rosa farm -r no64-rosa -m 128 --memory 450 --out work/no64-rosa --status work/no64-rosa.json
```

Memory, not CPU, is what limits how many seeds can run, so `--memory` sets a
budget for all caches together and the farm schedules against it:

- Until a seed finishes, it starts one seed per `--stagger` seconds (default
  300) while the caches use at most half the budget.
- After that, it forecasts each running seed's cache as growing linearly to
  the largest finished peak (plus 5%) over the median finished time, and starts
  another seed only if the forecast total stays within budget now and just
  before each running seed finishes.
- If the caches still go over budget, every seed except the eldest pauses until
  they fit again. Pausing does not change any result.

On a terminal it draws a live dashboard: a grid of all 256 seeds, memory used
against the budget, an estimate of the time remaining, the running seeds' cache
sizes, and a log. Otherwise it prints the log lines. `--status` writes the same
state as JSON every 5 seconds for other tools to read. `--threads` caps the
seeds running at once (default: the number of CPUs).

## How the solver works

The route is compiled into a flat list of operations. The solver searches it
depth first: at each path with a variable it tries every number of extra steps
from the lowest allowed up, and at each choice every option, keeping the
cheapest and remembering it for that state. Ties keep the lowest value, as in
C++. The cache key holds the same state as the C++ one (operation, generator
position, party, search progress and remaining step segments).

Two changes make it faster without changing results. Only decisions are
cached, not every instruction. And candidate step counts on a path are walked
incrementally, each continuing from the previous one, so a path costs O(m)
steps instead of O(m²).

The cache is split into 256 hash tables of its own design (`src/solve/table.rs`).
Each slot takes 23 bytes: the 22-byte entry and one byte for its distance from
its home slot. The tables fill to 90% and grow by a quarter, so they average
about 80% full; the standard library's table doubles at 7/8 full and averages
a little over 60%. The tables are given unequal shares of the states, so they
resize at different times and memory grows smoothly with no spike.

## Checking against the C++ version

`cargo test` includes spot checks with values taken from the C++ build. The
solver prints the C++ text format, so the reference set checks it directly:

```sh
scripts/reference.py -b rust/target/release/rosa -c m4 check -s 0-15   # quick
scripts/reference.py -b rust/target/release/rosa check                 # everything
```

For the replay simulator alone, build the C++ version into `build/` and the Rust version in
release mode, then from the repository root:

```sh
rust/scripts/compare_cpp.py --steps 4            # every route and seed
rust/scripts/compare_cpp.py --steps 0 --routes paladin --seeds 0-15
```

For each run the script replays the variable values C++ chose and compares
total frames, the chosen variables, every encounter, and the C++ "Base"
figures against a replay with no variables set.

## Behaviour carried over from C++

Until the port is verified, the Rust version matches the C++ results exactly,
oddities included (see `notes/cpp-bug-decisions.md` in the project files):

- A path that ends a search takes as many additional steps as the search
  needs, even beyond its assigned value.
- Search expressions keep the C++ crediting rules, including the `|` quirk
  the main README warns about.
- Battle lengths and penalties are truncated to whole milliframes through
  `f64`, as in C++, so a few values come out one milliframe low.
- Consecutive tabs act as one separator, and variable `0` means "no
  variable".
- `SAVE` lines are accepted and ignored, as the C++ engine ignores them.
- Extra steps a search forces on a path with no variable are reported as
  variable `FFFFFFFF`, and the cache key leaves out the active search, as in
  C++ (the operation implies it).

## Deliberate differences

- Malformed lines are errors with a line number, not warnings.
- `replay` does not print the "(Step N: …)" look-ahead lines C++ shows during
  a search; the solver's output does.
- A search that can never finish is an error rather than an endless loop.
