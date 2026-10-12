#!/usr/bin/env python3
"""Sweep a variable for twin-seed pairs and compare against the twin notes.

For each pair of adjacent seeds listed for a route in doc/twin-seed-notes.md,
solve both seeds once for every value of the variable (E305400 by default)
with `rosa sweep`. Then report, for each value the two seeds could share:

- each seed's loss against its own optimum, in seconds;
- whether the two routes still split before their encounters do (the
  `rosa_helper twins` test).

Run from the repository root after `cargo build --release`:

    rust/scripts/twin_sweep.py -r paladin -m 256 --out work/twins-paladin

Outputs go to OUT: the per-value route files, `values.tsv` (one row per pair
and value) and `summary.md` (one section per pair, next to the note).
"""

import argparse
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

SECONDS_PER_MILLIFRAME = 655171 / 39375000000
HEADER_LINES = 11  # the C++ header, VARS and the blank line after it


def notes_for(route):
    """Pairs and notes for one route from doc/twin-seed-notes.md."""
    pairs = []
    section = None
    for line in Path('doc/twin-seed-notes.md').read_text().splitlines():
        if line.startswith('## '):
            section = line.strip('# ').strip()
        elif section == route and re.match(r'^\d+\t\d+\t', line):
            first, second, note = line.split('\t', 2)
            pairs.append((int(first), int(second), note))
    return pairs


def body(path):
    """Route lines after the header, without the Seed/Index columns."""
    lines = path.read_text().split('\n')[HEADER_LINES:]
    return [re.sub(r' *Seed:.*Index.*$', '', line) for line in lines]


def splits_on_steps(path_1, path_2):
    """True if the routes differ before their encounters do (rosa_helper twins)."""
    for line_1, line_2 in zip(body(path_1), body(path_2)):
        if line_1 != line_2:
            return not re.search('Step.*/', line_1) and not re.search('Step.*/', line_2)
    return False


def sweep(args, seed):
    command = [args.binary, 'sweep', '-r', args.route, '-s', str(seed), '-m', str(args.steps),
               '--variable', args.variable, '--out', str(args.out / 'routes')] + args.extra
    if args.values:
        command += ['--values', args.values]
    output = subprocess.run(command, check=True, capture_output=True, text=True).stdout
    return seed, {int(value): int(frames) for value, frames in (line.split('\t') for line in output.splitlines())}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('-r', '--route', required=True)
    parser.add_argument('-m', '--steps', type=int, required=True)
    parser.add_argument('--variable', default='E305400')
    parser.add_argument('--values', help='values to try, as minimum-maximum (default 0 to the maximum steps)')
    parser.add_argument('--binary', default='rust/target/release/rosa')
    parser.add_argument('--jobs', type=int, default=None)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('extra', nargs='*', help='further solver arguments, after --')
    args = parser.parse_args()

    pairs = notes_for(args.route)
    if not pairs:
        sys.exit(f'no twin notes for route {args.route}')

    seeds = sorted({seed for first, second, _ in pairs for seed in (first, second)})
    with ThreadPoolExecutor(args.jobs) as pool:
        totals = dict(pool.map(lambda seed: sweep(args, seed), seeds))

    def route_file(seed, value):
        return args.out / 'routes' / f'{seed:03}-{value:03}.txt'

    rows = ['first\tsecond\tvalue\tfirst_loss_s\tsecond_loss_s\tsplits_on_steps']
    summary = [f'# Twin sweep: {args.route}, -m {args.steps}, {args.variable}', '']

    for first, second, note in pairs:
        best = {seed: min(totals[seed].values()) for seed in (first, second)}
        own = {seed: min(v for v, f in totals[seed].items() if f == best[seed]) for seed in (first, second)}
        problem = splits_on_steps(route_file(first, own[first]), route_file(second, own[second]))

        summary.append(f'## {first} / {second}')
        summary.append('')
        summary.append(f'- Note: {note}')
        summary.append(f'- Own optimum: {first} at {own[first]}, {second} at {own[second]}; '
                       f'{"split on steps (needs a fix)" if problem else "no split on steps"}')

        options = []
        for value in sorted(set(totals[first]) & set(totals[second])):
            losses = [(totals[seed][value] - best[seed]) * SECONDS_PER_MILLIFRAME for seed in (first, second)]
            splits = splits_on_steps(route_file(first, value), route_file(second, value))
            rows.append(f'{first}\t{second}\t{value}\t{losses[0]:.3f}\t{losses[1]:.3f}\t{int(splits)}')
            if not splits:
                options.append((value, losses))

        options.sort(key=lambda option: (max(option[1]), sum(option[1]), option[0]))
        summary.append('- Shared values that keep the routes together, least worst loss first:')
        for value, losses in options[:8]:
            summary.append(f'  - {value}: {first} loses {losses[0]:.1f} s, {second} loses {losses[1]:.1f} s')
        if not options:
            summary.append('  - none')
        summary.append('')

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / 'values.tsv').write_text('\n'.join(rows) + '\n')
    (args.out / 'summary.md').write_text('\n'.join(summary))
    print(f'wrote {args.out / "summary.md"}')


if __name__ == '__main__':
    main()
