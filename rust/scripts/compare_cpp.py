#!/usr/bin/env python3
"""Check the Rust replay simulator against the C++ solver.

For each route and seed, run the C++ solver, then replay the variable values it
chose with the Rust simulator and compare the results: exact total frames, the
chosen variables, and every encounter (step, encounter number, formation and
duration). The C++ "Base" figures are checked against a replay with no
variables set.

Run from the repository root after building both versions:

    rust/scripts/compare_cpp.py --steps 4 --seeds 0-255
"""

import argparse
import json
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ENCOUNTER = re.compile(r"^\s*Step\s+(\d+): (\d+) / (.*) \((\d+\.\d{3})s\)$")


def parse_cpp(text):
    fields = dict(line.split("\t", 1) for line in text.splitlines() if re.match(r"^[A-Z]+\t", line))
    encounters = [m.groups() for m in map(ENCOUNTER.match, text.splitlines()) if m]
    summary = dict(re.findall(r"^(Base Total Time|Base Encounters):\s+(\S+?)s?$", text, re.M))
    variables = {}

    for pair in fields.get("VARS", "").split():
        name, value = pair.split(":")
        if len(name) <= 7:  # FFFFFFFF is the C++ "no variable" slot
            variables[name] = int(value)

    return {
        "frames": int(fields["FRAMES"]),
        "variables": variables,
        "encounters": [(int(s), int(i), d, t) for s, i, d, t in encounters],
        "base_time": summary["Base Total Time"],
        "base_encounters": int(summary["Base Encounters"]),
    }


def seconds(milliframes):
    return f"{milliframes * 655171 / 39375000000:.3f}"


def replay(args, route, seed, variables, formations):
    command = [args.rust, "replay", "-r", route, "-s", str(seed), "--json", "-v", " ".join(f"{k}:{v}" for k, v in variables.items())]
    if args.tas_mode:
        command.append("-t")
    result = json.loads(subprocess.run(command, check=True, capture_output=True, text=True).stdout)
    encounters = [
        (e["step"], e["encounter_index"] + 1, formations[e["formation"]], seconds(e["frames"]))
        for event in result["events"]
        if event["type"] == "path"
        for e in event["encounters"]
    ]
    return result, encounters


def check(args, route, seed, formations):
    command = [args.cpp, "-r", route, "-s", str(seed), "-m", str(args.steps)]
    if args.tas_mode:
        command.append("-t")
    cpp = parse_cpp(subprocess.run(command, check=True, capture_output=True, text=True).stdout)

    problems = []
    result, encounters = replay(args, route, seed, cpp["variables"], formations)
    variables = {f"{int(k):07X}": v for k, v in result["variables"].items()}

    if result["frames"] != cpp["frames"]:
        problems.append(f"frames {result['frames']} != {cpp['frames']}")
    if variables != cpp["variables"]:
        problems.append(f"variables {variables} != {cpp['variables']}")
    if encounters != cpp["encounters"]:
        problems.append("encounter log differs")

    base, _ = replay(args, route, seed, {}, formations)
    if seconds(base["frames"]) != cpp["base_time"] or base["encounters"] != cpp["base_encounters"]:
        problems.append(f"base {seconds(base['frames'])}s/{base['encounters']} != {cpp['base_time']}s/{cpp['base_encounters']}")

    return route, seed, problems


def formations_for(route):
    data_set = "ff2us"
    for line in Path(f"data/routes/{route}.txt").read_text().splitlines():
        if line.startswith("DATA\t"):
            data_set = line.split("\t")[1]
    names = {}
    for line in Path(f"data/encounters/{data_set}.txt").read_text().splitlines():
        tokens = [t for t in line.split("\t") if t]
        if tokens and tokens[0] == "ENCOUNT":
            names.setdefault(int(tokens[1]), tokens[2])
    return names


def seed_range(text):
    start, _, end = text.partition("-")
    return range(int(start), int(end or start) + 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--cpp", default="build/rosa")
    parser.add_argument("--rust", default="rust/target/release/rosa")
    parser.add_argument("--routes", nargs="*", default=sorted(p.stem for p in Path("data/routes").glob("*.txt")))
    parser.add_argument("--seeds", type=seed_range, default=range(256))
    parser.add_argument("--steps", type=int, default=4)
    parser.add_argument("--tas-mode", action="store_true")
    parser.add_argument("--jobs", type=int, default=None)
    args = parser.parse_args()

    jobs = [(route, seed, formations_for(route)) for route in args.routes for seed in args.seeds]
    failures = 0

    with ThreadPoolExecutor(args.jobs) as pool:
        for route, seed, problems in pool.map(lambda job: check(args, *job), jobs):
            if problems:
                failures += 1
                print(f"MISMATCH {route} seed {seed}: {'; '.join(problems)}")

    print(f"{len(jobs) - failures}/{len(jobs)} runs match")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
