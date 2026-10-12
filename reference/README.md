# Reference outputs

Output of the C++ solver at commit f2aeb34 for every active route in
`data/routes`, used to check that changes (or a rewrite) still produce the same
results. The archived routes in `data/routes/archive` use an older format that
the current parser rejects, so they are not included.

## Cases

`cases.tsv` lists the cases. Each one runs a set of routes over a set of seeds
with fixed arguments:

| Case | Routes | Seeds | Arguments |
|---|---|---|---|
| `m0` | all 13 | 0–255 | `-m 0` |
| `m0-tas` | all 13 | 0–255 | `-m 0 -t` |
| `m4` | all 13 | 0–255 | `-m 4` |
| `m4-tas` | all 13 | 0–15 | `-m 4 -t` |
| `paladin-m16` | paladin | 0–255 | `-m 16` |
| `n2` | paladin, nocw | 0–31 | `-m 8 -n 2` |
| `n2-prefer` | paladin, nocw | 0–31 | `-m 8 -n 2 -p` |
| `vars` | paladin | 0–31 | `-m 4 -v "E000202:0 E302500:0-2"` |

`-m` is kept small because memory grows quickly with it; `-m 4` already
exercises the extra-step search on every route.

## Layout

- `results/<case>/<route>.jsonl.xz` holds the parsed result of each run, one
  JSON object per line in seed order. This is what runs are compared against.
- `outputs/<case>/<route>.tar.xz` holds one `<seed>.txt` per seed, the solver's
  raw stdout, kept for reading and for re-parsing with `rebuild`.
- `metrics/<case>.tsv` has one row per run: wall time, peak RSS (KB), exit
  status, and the SHA-256 of the canonical JSON result.

Timings were taken with four runs in parallel on a 4-core cloud container, so
compare them against runs made the same way (`-j 4`), not against single runs.

## Results schema

Checks compare results, not formatted text, so a new solver only has to produce
the same values. It can print the old text format or a single JSON object like
this (key order does not matter):

```json
{
  "route": "Final Fantasy IV Paladin%", "route_version": 20, "seed": 0,
  "maximum_steps": 4, "maximum_step_segments": -1,
  "tas_mode": false, "prefer_fewer_locations": false,
  "milliframes": 41784466,
  "variables": {"E000202": 4, "E000400": 4},
  "log": [
    {"depth": 0, "description": "Overworld (Kaipo)", "seed": 0, "index": 1,
     "optional_steps": null, "extra_steps": 0,
     "encounters": [{"step": 36, "number": 1, "formation": "SandMoth x2, Larva x2", "seconds": 7.009}],
     "lookahead": []}
  ],
  "summary": {"encounter_seconds": 267.8, "other_seconds": 427.463, "total_seconds": 695.263,
              "base_total_seconds": 712.218, "time_saved_seconds": 16.955,
              "optional_steps": 5, "extra_steps": 10, "encounters": 35,
              "base_encounters": 37, "encounters_saved": 2, "variable_count": 45}
}
```

- `variables` lists only variables with a non-zero value, keyed by the route
  file's hex variable id.
- `log` has one entry per printed route step (paths, choices, searches, notes).
  `depth` is the nesting level inside choices and searches, and `seed`/`index`
  are the step RNG state on entry. `optional_steps` is `null` where the step
  has no optional steps. `encounters` are the fights taken; `lookahead` are the
  upcoming encounters printed in parentheses inside a search.
- `seconds` values carry the three decimals the C++ output prints.
- A run that exits non-zero has a `null` result.

## Usage

Run from anywhere; the script switches to the repository root. It needs
Python 3.9+ and GNU time (`/usr/bin/time`, the `time` package) for memory.

```sh
scripts/reference.py check                       # every case, build/rosa
scripts/reference.py -c m4 check -r paladin -s 0-15
scripts/reference.py -b path/to/other/rosa check # a different binary
scripts/reference.py -c m4 capture -r nocw       # regenerate one route of one case
scripts/reference.py capture                     # regenerate everything
```

`check` lists the first differing fields of each mismatching run (for example
`result.log[12].encounters[0].step: 37, expected 36`), a per-case table of
total time and peak memory against the reference, and exits non-zero if any run
differs. The `ROSA` line is ignored, since it holds the build's git revision.
`rebuild` re-parses the stored text outputs, for use after changing the parser.

## Baseline

The calibration run used by `scripts/optimize_route.py`, measured on its own:

| Command | Wall time | Peak RSS |
|---|---|---|
| `build/rosa -r paladin -s 0 -m 64` | 35.1 s | 93 MB |

## Cost

All 10,608 runs exit cleanly, and a second full capture gave identical results,
so the solver is deterministic. Summed wall time and largest peak memory per
case (at `-j 4`):

| Case | Runs | Total time | Peak memory |
|---|---|---|---|
| `m0` | 3,328 | 46 s | 7 MB |
| `m0-tas` | 3,328 | 45 s | 7 MB |
| `m4` | 3,328 | 7.98 h | 1,327 MB |
| `m4-tas` | 208 | 32 min | 1,030 MB |
| `paladin-m16` | 256 | 294 s | 19 MB |
| `n2` | 64 | 3 s | 8 MB |
| `n2-prefer` | 64 | 3 s | 8 MB |
| `vars` | 32 | 6 s | 7 MB |

A full `check` takes two to three hours on four cores, most of it `m4` on
`glitchless` (3.2 h of the 8 h summed, peaking at 1.3 GB). For quick checks,
skip it or limit seeds, for example `-c m4 check -s 0-15`.
