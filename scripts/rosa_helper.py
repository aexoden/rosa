#!/usr/bin/env python3

import argparse
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

        try:
            with open(filename_2) as f:
                lines_2 = f.readlines()
        except FileNotFoundError:
            print(f'WARNING: {filename_2} does not exist')
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
