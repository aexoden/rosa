#!/usr/bin/env python3

import argparse
import curses
import datetime
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile
import time

from collections import deque

#------------------------------------------------------------------------------
# Constants
#------------------------------------------------------------------------------

COLOR_WHITE = 1
COLOR_BLUE = 2
COLOR_GREEN = 3
COLOR_YELLOW = 4


#------------------------------------------------------------------------------
# Functions
#------------------------------------------------------------------------------

def format_log_entry(msg):
    return f'[{datetime.datetime.now()}] {msg}'


def generate_timings(max_threads):
    print('Generating timings...')

    cmd_args = ['/usr/bin/time', '-f', 'BENCHMARK %e', 'build/rosa', '-r', 'paladin', '-s', '0', '-m', '64']
    timings = []
    base_time = None

    for i in range(max_threads):
        suffix = '' if i == 0 else 's'
        print(f'Testing {i+1} simultaneous thread{suffix}...', end='')
        sys.stdout.flush()
        processes = deque()
        for _ in range(i + 1):
            processes.append(subprocess.Popen(cmd_args, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE))

        samples = []

        while len(processes) > 0:
            process = processes.popleft()
            _, err = process.communicate()
            err = err.decode('utf-8')
            for line in err.split('\n'):
                if line.startswith('BENCHMARK'):
                    t = float(line.strip().split(' ')[1])
                    samples.append(t)

        if i == 0:
            base_time = samples[0]

        timings.append(statistics.mean(samples) / base_time)

        print(f' {statistics.mean(samples):0.3f}s, {timings[i]:0.3f}x')

    return timings


def get_base_time_multiplier(current_processes, timings):
    if timings:
        index = int(current_processes) - 1
        base = timings[index]

        if len(timings) > index + 1:
            delta = timings[index + 1] - base
        else:
            delta = 0

        x = current_processes - 1 - index
        return (x * delta) + base
    else:
        return (((current_processes - 1) ** 1.5) / 116.1895) + 1


def get_time_multiplier(max_processes, current_processes, timings):
    current_multiplier = get_base_time_multiplier(current_processes, timings)
    target_multiplier = get_base_time_multiplier(max_processes, timings)
    return target_multiplier / current_multiplier


def get_seed_coordinates(seed):
    x = (seed % 32) * 4 + 2
    y = (seed // 32) + 1
    return (x, y)


def load_seeds(filename):
    if filename is None:
        return list(range(256))

    with open(filename) as f:
        return [int(x) for x in f.read().split()]


def parse_arguments():
    parser = argparse.ArgumentParser(description='Optimize multiple seeds of a single route in parallel')

    parser.add_argument('route', metavar='ROUTE', type=str, help='name of the route to optimize')

    parser.add_argument('--output-dir', metavar='DIRECTORY', type=str, help='directory to output optimized routes, discarded if not provided')
    parser.add_argument('--seed-list', metavar='FILENAME', type=str, help='list of seeds to process, 0 to 255 if not provided')
    parser.add_argument('--max-steps', type=int, default=256, help='maximum number of steps per segment')
    parser.add_argument('--max-segments', type=int, help='maximum number of segments at which to allow extra steps')

    parser.add_argument('--persistent-cache', action='store_true', help='use a persistent on-disk cache')
    parser.add_argument('--cache-location', metavar='DIRECTORY', type=str, help='custom location for the persistent cache')

    parser.add_argument('--max-memory', type=float, default=1.0, help='target maximum memory to use in gigabytes (NOTE: this is not a hard limit)')
    parser.add_argument('--max-threads', type=int, default=1, help='maximum number of worker threads')
    parser.add_argument('--timings', metavar='FILENAME', type=str, help='filename to use for multithreaded timings, will be created if it does not exist')

    return parser.parse_args()


def format_time(value):
    value = int(value + 0.5)
    hours = value // 3600
    minutes = value // 60 - (hours * 60)
    seconds = value - hours * 3600 - minutes * 60
    return f'{int(hours)}:{int(minutes):02}:{int(seconds):02}'


#------------------------------------------------------------------------------
# Classes
#------------------------------------------------------------------------------

class Process(object):
    def __init__(self, args, seed):
        self._args = args
        self._seed = seed

        if args.output_dir is not None:
            self._output_file = open(os.path.join(args.output_dir, f'{seed:03d}.txt'), 'w')
        else:
            self._output_file = subprocess.DEVNULL

        self._process = None
        self._finalized = False
        self._time = None
        self._memory = None
        self._raw_time = None
        self._process_counts = []

        f = tempfile.NamedTemporaryFile(delete=False)
        self._time_filename = f.name
        f.close()

        self._dispatch()

    @property
    def seed(self):
        return self._seed

    @property
    def time(self):
        return self._time

    @property
    def memory(self):
        return self._memory

    @property
    def raw_time(self):
        return self._raw_time

    @property
    def process_count(self):
        if len(self._process_counts) > 0:
            return statistics.mean(self._process_counts)

        return 1

    @property
    def complete(self):
        if not self._process or self._process.poll() is None:
            return False

        if not self._finalized:
            with open(self._time_filename) as f:
                data = f.read().strip()

                matches = re.search('([0-9.]*)elapsed.*data ([0-9]*)max', data)

                if matches:
                    self._memory = float(matches.group(2)) / 1048576
                    self._time = float(matches.group(1))
                    self._raw_time = data

                os.unlink(self._time_filename)
                self._finalized = True

        return True

    def add_process_count(self, count):
        self._process_counts.append(count)

    def _dispatch(self):
        time_fmt = '%Uuser %Ssystem %eelapsed %PCPU (%Xtext+%Ddata %Mmax)k %Iinputs+%Ooutputs (%Fmajor+%Rminor)pagefaults %Wswaps'
        cmd_args = ['/usr/bin/time', '-o', self._time_filename, '-f', time_fmt, 'build/rosa', '-r', self._args.route, '-s', str(self.seed), '-m', str(self._args.max_steps)]

        if self._args.max_segments is not None:
            cmd_args.extend(['-p', '-n', str(self._args.max_segments)])

        if self._args.persistent_cache:
            cmd_args.extend(['-c', 'persistent'])

        if self._args.cache_location:
            cmd_args.extend(['-l', self._args.cache_location])

        self._process = subprocess.Popen(cmd_args, stdout=self._output_file)


class Optimizer(object):
    def __init__(self, window, args, timings):
        self._window_main = window
        self._args = args
        self._seeds = load_seeds(self._args.seed_list)
        self._timings = timings
        self._curses_init()

    def _curses_init(self):
        self._window_main.clear()
        self._window_seeds = curses.newwin(10, 131, 0, 0)
        self._window_info = curses.newwin(10, curses.COLS - 131, 0, 131)
        self._window_log = curses.newwin(curses.LINES - 10, curses.COLS, 10, 0)

        self._window_seeds.box()
        self._window_info.box()
        self._window_log.box()

        curses.init_pair(COLOR_WHITE, curses.COLOR_WHITE, curses.COLOR_BLACK)
        curses.init_pair(COLOR_BLUE, curses.COLOR_BLUE, curses.COLOR_BLACK)
        curses.init_pair(COLOR_GREEN, curses.COLOR_GREEN, curses.COLOR_BLACK)
        curses.init_pair(COLOR_YELLOW, curses.COLOR_YELLOW, curses.COLOR_BLACK)

        for seed in range(256):
            x, y = get_seed_coordinates(seed)
            color = curses.color_pair(COLOR_WHITE) if seed in self._seeds else curses.color_pair(COLOR_BLUE) | curses.A_DIM
            self._window_seeds.addstr(y, x, '{:3d}'.format(seed), color)

    def run(self):
        log_entries = []
        processes = []
        seeds = self._seeds[:]

        last_dispatch = 0

        memory_values = []
        time_values = []
        adjusted_time_values = []

        while len(seeds) > 0 or len(processes) > 0:
            new_processes = []
            finished = 0

            for process in processes:
                if process.complete:
                    memory_values.append(process.memory)
                    time_values.append(process.time)
                    adjusted_time_values.append((process.time, process.process_count))
                    log_entries.append(format_log_entry(f'Seed {process.seed} Complete: {process.raw_time}'))
                    finished += 1
                else:
                    process.add_process_count(len(processes))
                    new_processes.append(process)

            processes = new_processes

            if len(memory_values) > 0 and max(memory_values) > 0:
                estimated_memory = max(memory_values)
                if len(memory_values) < 32:
                    # The following formula multiplies by 1.15 when x is 1 and by 1 when x is 32.
                    estimated_memory = ((716 - 3 * len(memory_values)) / 620) * max(memory_values)
                ideal_max_processes = max(1, int((self._args.max_memory * 2 - estimated_memory) / estimated_memory))
            else:
                ideal_max_processes = 1

            max_processes = min(self._args.max_threads, ideal_max_processes)

            if len(time_values) > 0:
                if len(time_values) >= 16:
                    index = int(len(time_values) * 0.5)
                else:
                    factor = (31 - len(time_values)) / 30
                    index = min(len(time_values) - 1, int(len(time_values) * factor))

                scaled_time_values = [t * get_time_multiplier(max_processes, p, self._timings) for t, p in adjusted_time_values]
                expected_time = sorted(scaled_time_values)[index]
            else:
                expected_time = 86400

            while len(processes) < max_processes and (time.time() > (last_dispatch + expected_time / ideal_max_processes) or len(processes) == 0) and len(seeds) > 0:
                seed = seeds.pop(0)
                processes.append(Process(self._args, seed))
                last_dispatch = time.time()

            self._window_info.erase()
            self._window_info.box()

            time_estimate = 0

            if len(time_values) > 0:
                time_estimate = len(seeds) * statistics.mean(scaled_time_values) * get_time_multiplier(max_processes, min(len(scaled_time_values), max_processes), self._timings) / max_processes
                time_estimate += statistics.mean(scaled_time_values)
                time_estimate -= time.time() - last_dispatch
                eta = datetime.datetime.now() + datetime.timedelta(seconds=time_estimate)

            info = {
                'Current Threads:': f'{len(processes)} / {max_processes} ({ideal_max_processes})',
                'Since Last Dispatch:': f'{format_time(time.time() - last_dispatch)} / {format_time(expected_time / ideal_max_processes)}',
            }

            if len(memory_values) > 0:
                info['Memory Usage:'] = f'{min(memory_values):0.3f}G - {max(memory_values):0.3f}G ({statistics.mean(memory_values):0.3f}G)'
                info['Time:'] = f'{format_time(min(time_values))} - {format_time(max(time_values))} ({format_time(statistics.mean(time_values))})'
                info['Scaled Time:'] = f'{format_time(min(scaled_time_values))} - {format_time(max(scaled_time_values))} ({format_time(statistics.mean(scaled_time_values))})'
                info['Remaining:'] = f'{format_time(time_estimate)}'
                info['ETA:'] = eta.strftime('%Y-%m-%d %H:%M:%S')
            else:
                info['Memory Usage:'] = 'N/A'
                info['Time:'] = 'N/A'
                info['Remaining:'] = 'N/A'
                info['ETA:'] = 'N/A'

            max_key_length = max([len(key) for key in info])

            for i, (key, value) in enumerate(info.items()):
                self._window_info.addstr(i + 1, 2, f'{key:{max_key_length}} {value}')

            process_seeds = [x.seed for x in processes]
            for seed in range(256):
                x, y = get_seed_coordinates(seed)
                if seed in process_seeds:
                    self._window_seeds.chgat(y, x, 4, curses.color_pair(COLOR_YELLOW))
                elif seed not in seeds and seed in self._seeds:
                    self._window_seeds.chgat(y, x, 4, curses.color_pair(COLOR_GREEN))

            self._window_log.erase()
            self._window_log.box()

            for i, entry in enumerate(log_entries[-(curses.LINES - 10 - 2):]):
                self._window_log.addstr(i + 1, 1, entry)

            self._window_main.refresh()
            self._window_seeds.refresh()
            self._window_info.refresh()
            self._window_log.refresh()
            time.sleep(0.25)

        return log_entries


#-------------------------------------------------------------------------------
# Main Execution
#-------------------------------------------------------------------------------

def main():
    args = parse_arguments()

    timings = None

    if args.timings:
        if os.path.exists(args.timings):
            with open(args.timings) as f:
                timings = json.load(f)

        if not timings or len(timings) < args.max_threads:
            timings = generate_timings(args.max_threads)
            with open(args.timings, 'w') as f:
                json.dump(timings, f)

    for entry in curses.wrapper(run, args, timings):
        print(entry)


def run(stdscr, args, timings):
    optimizer = Optimizer(stdscr, args, timings)
    return optimizer.run()


if __name__ == '__main__':
    main()
