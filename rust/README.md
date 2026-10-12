# ROSA in Rust

A Rust port of ROSA, in progress. It currently replays a route with fixed
variable values (extra steps per path, options per choice) and reports the
total time and every encounter. The solver that searches over those values is
the next step.

## Layout

| Module | Role |
|---|---|
| `model` | Format-neutral route model: paths, choices, searches, parties |
| `data` | Game data: maps, encounter groups, battle lengths |
| `legacy` | Importer for the original tab-separated route and data files |
| `rng` | The FF4 step and encounter generator |
| `sim` | Replays a route for one seed and one set of variable values |
| `time` | Integer milliframe arithmetic |

The core never sees the old file format: `legacy` builds `model` and `data`
values, and a future importer for a richer format can do the same.

## Usage

Run from the repository root so the default `data/` directory is found:

```sh
cargo run --release --manifest-path rust/Cargo.toml -- replay -r paladin -s 0 \
    -v "E000202:4 E000400:4 E302500:1 E302600:4 E308000:2"
```

Add `--json` for the full structured result.

## Checking against the C++ version

`cargo test` includes spot checks with values taken from the C++ build. For a
full comparison, build the C++ version into `build/` and the Rust version in
release mode, then from the repository root:

```sh
rust/scripts/compare_cpp.py --steps 4            # every route and seed
rust/scripts/compare_cpp.py --steps 0 --routes paladin --seeds 0-15
```

For each run the script replays the variable values C++ chose and compares
total frames, the chosen variables, every encounter, and the C++ "Base"
figures against a replay with no variables set.

## Behaviour carried over from C++

- A path that ends a search takes as many additional steps as the search
  needs, even beyond its assigned value.
- Search expressions keep the C++ crediting rules, including the `|` quirk
  the main README warns about.
- Battle lengths and penalties are truncated to whole milliframes as in C++.

## Deliberate differences

- Malformed lines are errors with a line number, not warnings.
- `SAVE` is not accepted yet; no bundled route uses it, and the C++ parser
  reads past the end of its fields.
- The "(Step N: …)" look-ahead lines C++ prints during a search are output
  formatting and are not reproduced.
