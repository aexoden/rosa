#!/usr/bin/env python3

import argparse
import json
import os
import re
import statistics


#-------------------------------------------------------------------------------
# Functions
#-------------------------------------------------------------------------------

def get_seed_filename(directory, seed):
    return os.path.join(directory, f'{seed:03d}.txt')


#-------------------------------------------------------------------------------
# Subcommands
#-------------------------------------------------------------------------------

def subcommand_checkvars(args):
    choice = 0
    level = 0
    variables = {}

    with open(args.filename) as f:
        for line in f:
            if line.startswith('CHOICE'):
                level += 1
                choice += 1
            elif line.startswith('END'):
                level -= 1

            matches = re.search('(PATH|CHOICE)\t([A-F][0-9A-F]{6})', line)

            if matches:
                var = matches.group(2)

                if var not in variables:
                    variables[var] = []

                variables[var].append((level, choice))

    bad_vars = {}
    warning_vars = {}

    for var, levels in variables.items():
        if len(levels) > 1:
            if min([x[0] for x in levels]) == 0:
                bad_vars[var] = len(levels)
            elif len(set([x[1] for x in levels])) > 1:
                warning_vars[var] = len(levels)

    if len(bad_vars) > 0:
        print('Bad Variables')
        print('-------------')

        for var, count in bad_vars.items():
            print(f'  {count} {var}')

        if len(warning_vars) > 0:
            print()

    if len(warning_vars) > 0:
        print('Potentially Bad Variables')
        print('-------------------------')

        for var, count in warning_vars.items():
            print(f'  {count} {var}')


def subcommand_identification(args):
    formations = {}
    base = {}

    for seed in range(256):
        filename = os.path.join(args.directory, f'{seed:03d}.txt')

        if not os.path.exists(filename):
            print('WARNING: {filename} does not exist')
            continue

        with open(filename) as f:
            current_seed = seed
            index = 0
            steps = 0
            formations[seed] = []

            for line in f:
                matches = re.search(r'(?P<map>.*)Seed: *(?P<seed>[0-9]*) *Index: *(?P<index>[0-9]*)', line)

                if matches:
                    new_seed = int(matches.group('seed'))
                    new_index = int(matches.group('index'))

                    if current_seed == new_seed:
                        steps += new_index - index
                    else:
                        steps += 256 + new_index - index

                    if seed == 0:
                        base[steps] = matches.group('map').strip()

                    current_seed = new_seed
                    index = new_index

                matches = re.search(r' *Step *(?P<step>[0-9]*): [0-9]* / (?P<formation>.*) \(', line)

                if matches:
                    formations[seed].append((steps + int(matches.group('step')), matches.group('formation')))

    all_divergence = []

    for seed in range(256):
        for i, (step, formation) in enumerate(formations[seed]):
            other_seed = (seed + 1) % 256
            other_step, other_formation = formations[other_seed][i]

            if step != other_step or formation != other_formation:
                divergence = min(step, other_step)
                all_divergence.append(divergence)
                break

    if args.encounters is not None:
        output_data = []

        for seed in range(256):
            output_data.append({})
            for step, formation in formations[seed]:
                if step <= args.encounters:
                    output_data[seed][step] = formation

        print(json.dumps(output_data, sort_keys=True, indent=4, separators=(',', ': ')))
    else:
        remaining = set(range(256))

        for step, map_name in base.items():
            print(map_name)
            for seed in list(remaining):
                divergence = all_divergence[seed]

                if divergence < step:
                    print(f'  {seed} and {(seed + 1) % 256}')
                    remaining.remove(seed)

            if len(remaining) == 0:
                break

        print()
        print(f'50% of pairs diverge by step {sorted(all_divergence)[127]}')
        print(f'75% of pairs diverge by step {sorted(all_divergence)[191]}')
        print(f'80% of pairs diverge by step {sorted(all_divergence)[204]}')
        print(f'85% of pairs diverge by step {sorted(all_divergence)[217]}')
        print(f'90% of pairs diverge by step {sorted(all_divergence)[230]}')
        print(f'95% of pairs diverge by step {sorted(all_divergence)[243]}')
        print(f'100% of pairs diverge by step {max(all_divergence)}')


def subcommand_range(args):
    frames = {}

    for seed in range(256):
        filename = os.path.join(args.directory, f'{seed:03d}.txt')

        if not os.path.exists(filename):
            continue

        with open(filename) as f:
            for line in f:
                if line.startswith('FRAMES'):
                    frames[seed] = int(line.strip().split('\t')[1])

    range_averages = {}

    for start in range(256):
        range_frames = []
        seed = start

        for i in range(args.size):
            seed = (start + i) % 256
            range_frames.append(frames[seed])

        average = statistics.mean(range_frames)
        range_averages[start] = average

    for start, average in sorted(range_averages.items(), key=lambda x: x[1]):
        print(f'{start:3d} - {(start + args.size - 1) % 256:3d}: {average * 655171 / 39375000000:12.3f}')


def subcommand_rate(args):
    selected_counts = None
    selected_times = None
    total_counts = []
    total_times = []

    for seed in range(256):
        filename = os.path.join(args.directory, f'{seed:03d}.txt')

        if not os.path.exists(filename):
            continue

        with open(filename) as f:
            counts = []
            times = []
            current_count = 0
            current_time = 0

            for line in f:
                matches = re.search(r'Step.*: (?P<index>[0-9]+) / .*[^0-9.](?P<time>[0-9.]*)s\)', line.strip())

                if matches:
                    current_count += 1
                    current_time += float(matches.group('time'))

                matches = re.search(r' +Extra Steps: (?P<steps>[0-9]+)', line.rstrip())

                if matches:
                    current_time += int(matches.group('steps')) * 16 * 655171 / 39375000

                if line.startswith('Paladin'):
                    counts.append(current_count)
                    times.append(current_time)
                    current_count = 0
                    current_time = 0
                elif line.startswith('Castle of Dwarves W'):
                    counts.append(current_count)
                    times.append(current_time)
                    current_count = 0
                    current_time = 0
                elif line.startswith('Return to Moon'):
                    counts.append(current_count)
                    times.append(current_time)
                    current_count = 0
                    current_time = 0

            counts.append(current_count)
            times.append(current_time)

            if seed == args.seed:
                selected_counts = counts[:]
                selected_times = times[:]

            while len(total_counts) < len(counts):
                total_counts.append([])
                total_times.append([])

            for index, count in enumerate(counts):
                total_counts[index].append(count)

            for index, time in enumerate(times):
                total_times[index].append(time)

    for index, counts in enumerate(total_counts):
        median = statistics.median(counts)
        selected = (selected_counts[index] - statistics.mean(counts)) / statistics.stdev(counts)
        print(f'Group {index} {min(counts):8d} {median:8.0f} {max(counts):8d}   {selected_counts[index]:8d} {selected:8.3f}')

    print()

    for index, times in enumerate(total_times):
        median = statistics.median(times)
        selected = (selected_times[index] - statistics.mean(times)) / statistics.stdev(times)
        print(f'Group {index} {min(times):8.2f} {median:8.2f} {max(times):8.2f}   {selected_times[index]:8.2f} {selected:8.3f}')


def subcommand_twins(args):
    for seed_1 in range(256):
        seed_2 = (seed_1 + 1) % 256

        filename_1 = get_seed_filename(args.directory, seed_1)
        filename_2 = get_seed_filename(args.directory, seed_2)

        try:
            with open(filename_1) as f:
                lines_1 = f.readlines()
        except FileNotFoundError:
            print(f'WARNING: {filename_1} does not exist')
            continue

        if len(lines_1) == 0:
            print(f'WARNING: {filename_1} is empty')
            continue

        try:
            with open(filename_2) as f:
                lines_2 = f.readlines()
        except FileNotFoundError:
            print(f'WARNING: {filename_2} does not exist')
            continue

        if len(lines_2) == 0:
            continue

        for index in range(11, len(lines_1)):
            matches = re.search('(?P<line>.*)Seed:.*Index.*', lines_1[index])

            if matches:
                line_1 = matches.group('line')
            else:
                line_1 = lines_1[index]

            matches = re.search('(?P<line>.*)Seed:.*Index.*', lines_2[index])

            if matches:
                line_2 = matches.group('line')
            else:
                line_2 = lines_2[index]

            if line_1 != line_2:
                if not re.search('Step.*/', line_1) and not re.search('Step.*/', line_2):
                    print(f'{seed_1} and {seed_2} differ in steps')

                break


def subcommand_varfrequency(args):
    variables = {}

    with open(args.filename) as f:
        for line in f:
            tokens = line.split('\t')
            if tokens[0] in ['PATH', 'CHOICE'] and tokens[1] != '-':
                variables[tokens[1]] = 0

    for seed in range(256):
        with open(os.path.join(args.directory, f'{seed:03d}.txt')) as f:
            for line in f:
                tokens = line.split('\t')
                if tokens[0] == 'VARS':
                    all_vars = tokens[1].split(' ')

                    for var in all_vars:
                        var, value = var.split(':')

                        if int(value) > 0:
                            variables[var] += 1

    for variable, count in sorted(variables.items(), key=lambda x: x[1], reverse=True):
        print(variable, count)



#-------------------------------------------------------------------------------
# Main Execution
#-------------------------------------------------------------------------------

def main():
    subcommands = {
        'checkvars': {
            'function': subcommand_checkvars,
            'help': 'checks variables in a route for potential errors',
            'arguments': {
                'filename': {'metavar': 'FILENAME', 'type': str, 'help': 'filename of the route to check'}
            }
        },
        'identification': {
            'function': subcommand_identification,
            'help': 'extracts data useful for identifying which seed a run is on',
            'arguments': {
                'directory': {'metavar': 'DIRECTORY', 'type': str, 'help': 'directory containing the routes to process'},
                '--encounters': {'metavar': 'LIMIT', 'type': int, 'help': 'if specified, prints encounters for each seed up to the given step limit'},
            }
        },
        'range': {
            'function': subcommand_range,
            'help': 'finds the best range of seeds of a given size',
            'arguments': {
                'directory': {'metavar': 'DIRECTORY', 'type': str, 'help': 'directory containing the routes to check'},
                'size': {'type': int, 'help': 'size of the range'}
            }
        },
        'rate': {
            'function': subcommand_rate,
            'help': 'rates a seed in comparison to others of the same route (very non-generic)',
            'arguments': {
                'directory': {'metavar': 'DIRECTORY', 'type': str, 'help': 'directory containing the generated routes'},
                'seed': {'type': int, 'help': 'seed to rate'}
            }
        },
        'twins': {
            'function': subcommand_twins,
            'help': 'checks generated routes for problematic twin seeds',
            'arguments': {
                'directory': {'metavar': 'DIRECTORY', 'type': str, 'help': 'directory to check for problematic twin seeds'}
            }
        },
        'varfrequency': {
            'function': subcommand_varfrequency,
            'help': 'finds unused variables for a given route',
            'arguments': {
                'filename': {'metavar': 'FILENAME', 'type': str, 'help': 'filename for the route description'},
                'directory': {'metavar': 'DIRECTORY', 'type': str, 'help': 'directory containing the generated routes to check'}
            }
        },
    }

    parser = argparse.ArgumentParser(description='Various helper utilities to work with routes')
    subparsers = parser.add_subparsers(title='subcommands', description='valid subcommands', help='additional help')

    for subcommand, data in subcommands.items():
        subparser = subparsers.add_parser(subcommand, help=data['help'])
        subparser.set_defaults(func=data['function'])

        for argument, properties in data['arguments'].items():
            subparser.add_argument(argument, **properties)

    args = parser.parse_args()
    args.func(args)


if __name__ == '__main__':
    main()
