#!/usr/bin/env python3
"""Capture and check reference results for the rosa solver.

The reference set pins down the solver's behavior so a rewrite can be verified
against it. Cases are defined in reference/cases.tsv. Each case runs one or more
routes over a range of seeds with fixed arguments.

  scripts/reference.py capture    Run every case and write the reference set.
  scripts/reference.py check      Re-run cases and compare against the references.
  scripts/reference.py rebuild    Regenerate results/ and hashes from outputs/.

Comparison is on parsed results (frames, chosen variables, the per-segment
encounter log and the summary totals), not on the formatted text. A solver under
test may print either the C++ text format or a single JSON object in the
results schema described in reference/README.md.
"""

import argparse
import concurrent.futures
import hashlib
import io
import json
import lzma
import os
import re
import shlex
import subprocess
import sys
import tarfile
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REFERENCE = ROOT / 'reference'
TIME = '/usr/bin/time'
METRICS_HEADER = ['route', 'seed', 'wall_seconds', 'peak_rss_kb', 'exit_status', 'sha256']

HEADER_FIELDS = {
	'ROUTE': ('route', str),
	'VERSION': ('route_version', int),
	'SEED': ('seed', int),
	'MAXSTEP': ('maximum_steps', int),
	'MAXSEG': ('maximum_step_segments', int),
	'TASMODE': ('tas_mode', lambda value: value == '1'),
	'MINIMUM': ('prefer_fewer_locations', lambda value: value == '1'),
	'FRAMES': ('milliframes', int),
}

SUMMARY_FIELDS = {
	'Encounter Time': ('encounter_seconds', float),
	'Other Time': ('other_seconds', float),
	'Total Time': ('total_seconds', float),
	'Base Total Time': ('base_total_seconds', float),
	'Time Saved': ('time_saved_seconds', float),
	'Optional Steps': ('optional_steps', int),
	'Extra Steps': ('extra_steps', int),
	'Encounters': ('encounters', int),
	'Base Encounters': ('base_encounters', int),
	'Encounters Saved': ('encounters_saved', int),
	'Number of Variables': ('variable_count', int),
}

ENTRY_PATTERN = re.compile(r'^( *)(.*?) *Seed: +(\d+)   Index: +(\d+)$')
ENCOUNTER_PATTERN = re.compile(r'^ *Step +(\d+): (\d+) / (.*) \((\d+\.\d+)s\)$')
LOOKAHEAD_PATTERN = re.compile(r'^ *\(Step +(\d+): (\d+) / (.*)\)$')
STEPS_PATTERN = re.compile(r'^ *(Optional|Extra) Steps: (\d+)$')
SUMMARY_PATTERN = re.compile(r'^([A-Za-z ]+): +(-?\d+(?:\.\d+)?)s?$')


class ParseError(Exception):
	pass


def parse_text(text):
	"""Parse the C++ solver's text output into the results schema."""
	result = {'variables': {}, 'log': [], 'summary': {}}
	lines = text.split('\n')
	position = 0

	while position < len(lines) and lines[position]:
		key, _, value = lines[position].partition('\t')
		if key == 'VARS':
			for pair in value.split():
				name, number = pair.split(':')
				result['variables'][name] = int(number)
		elif key in HEADER_FIELDS:
			field, convert = HEADER_FIELDS[key]
			result[field] = convert(value)
		elif key != 'ROSA':
			raise ParseError(f'unknown header line: {lines[position]!r}')
		position += 1

	entry = None
	for line in lines[position:]:
		if not line:
			continue
		if match := ENTRY_PATTERN.match(line):
			entry = {
				'depth': len(match[1]) // 2,
				'description': match[2],
				'seed': int(match[3]),
				'index': int(match[4]),
				'optional_steps': None,
				'extra_steps': 0,
				'encounters': [],
				'lookahead': [],
			}
			result['log'].append(entry)
		elif match := ENCOUNTER_PATTERN.match(line):
			entry['encounters'].append({'step': int(match[1]), 'number': int(match[2]), 'formation': match[3], 'seconds': float(match[4])})
		elif match := LOOKAHEAD_PATTERN.match(line):
			entry['lookahead'].append({'step': int(match[1]), 'number': int(match[2]), 'formation': match[3]})
		elif (match := STEPS_PATTERN.match(line)) and line.startswith('  '):
			entry['optional_steps' if match[1] == 'Optional' else 'extra_steps'] = int(match[2])
		elif (match := SUMMARY_PATTERN.match(line)) and match[1] in SUMMARY_FIELDS:
			field, convert = SUMMARY_FIELDS[match[1]]
			result['summary'][field] = convert(match[2])
		else:
			raise ParseError(f'unrecognized line: {line!r}')

	if 'milliframes' not in result or len(result['summary']) != len(SUMMARY_FIELDS):
		raise ParseError('incomplete output')
	return result


def parse_output(text):
	"""Parse solver output, which may be the C++ text format or JSON."""
	if text.lstrip().startswith('{'):
		return json.loads(text)
	return parse_text(text)


def canonical(result):
	return json.dumps(result, sort_keys=True, separators=(',', ':'))


def digest(result):
	return hashlib.sha256(canonical(result).encode()).hexdigest()


def differences(expected, actual, path=''):
	"""Yield a description of each place two results differ."""
	if isinstance(expected, dict) and isinstance(actual, dict):
		for key in sorted(set(expected) | set(actual)):
			if key not in actual:
				yield f'{path}.{key}: missing'
			elif key not in expected:
				yield f'{path}.{key}: unexpected {actual[key]!r}'
			else:
				yield from differences(expected[key], actual[key], f'{path}.{key}')
	elif isinstance(expected, list) and isinstance(actual, list):
		for index, (left, right) in enumerate(zip(expected, actual)):
			yield from differences(left, right, f'{path}[{index}]')
		if len(expected) != len(actual):
			yield f'{path}: length {len(actual)}, expected {len(expected)}'
	elif expected != actual:
		yield f'{path}: {actual!r}, expected {expected!r}'


def active_routes():
	return sorted(path.stem for path in (ROOT / 'data' / 'routes').glob('*.txt'))


def parse_seeds(text):
	seeds = []
	for part in text.split(','):
		if '-' in part:
			low, high = part.split('-')
			seeds.extend(range(int(low), int(high) + 1))
		else:
			seeds.append(int(part))
	return seeds


def load_cases(names=None):
	cases = []
	for line in (REFERENCE / 'cases.tsv').read_text().splitlines():
		if not line.strip() or line.startswith('#'):
			continue
		name, routes, seeds, args = line.split('\t')
		if names and name not in names:
			continue
		routes = active_routes() if routes == '*' else routes.split(',')
		cases.append({'name': name, 'routes': routes, 'seeds': parse_seeds(seeds), 'args': shlex.split(args)})
	return cases


def run(binary, route, seed, args):
	"""Run one solve, returning (output, wall seconds, peak RSS in KB, exit status).

	Peak RSS comes from GNU time. Measuring it with wait4() from this process
	would be wrong: Linux carries the parent's high-water mark into the child at
	fork and keeps it across exec, so every run would report at least this
	script's own memory use.
	"""
	with tempfile.NamedTemporaryFile('r') as rss_file:
		command = [TIME, '-f', '%M', '-o', rss_file.name, str(binary), '-r', route, '-s', str(seed)] + args
		start = time.monotonic()
		process = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
		seconds = time.monotonic() - start
		lines = rss_file.read().split()
	return process.stdout.decode(), seconds, int(lines[-1]) if lines else 0, process.returncode


def jobs_for(cases, routes=None, seeds=None):
	for case in cases:
		for route in case['routes']:
			if routes and route not in routes:
				continue
			for seed in case['seeds']:
				if seeds is None or seed in seeds:
					yield case, route, seed


def execute(binary, jobs, workers):
	jobs = list(jobs)
	results = {}
	interactive = sys.stderr.isatty()
	with concurrent.futures.ThreadPoolExecutor(workers) as pool:
		futures = {pool.submit(run, binary, route, seed, case['args']): (case['name'], route, seed) for case, route, seed in jobs}
		for done, future in enumerate(concurrent.futures.as_completed(futures), 1):
			results[futures[future]] = future.result()
			if interactive or done == len(jobs):
				print(f'\r{done}/{len(jobs)} runs', end='' if interactive else '\n', file=sys.stderr, flush=True)
	if interactive:
		print(file=sys.stderr)
	return results


def outputs_path(name, route):
	return REFERENCE / 'outputs' / name / f'{route}.tar.xz'


def results_path(name, route):
	return REFERENCE / 'results' / name / f'{route}.jsonl.xz'


def metrics_path(name):
	return REFERENCE / 'metrics' / f'{name}.tsv'


def write_route(case, route, runs):
	"""Write outputs, results and metrics for one route of a case. runs maps seed to (output, seconds, rss, status)."""
	rows = []
	outputs_path(case['name'], route).parent.mkdir(parents=True, exist_ok=True)
	results_path(case['name'], route).parent.mkdir(parents=True, exist_ok=True)
	with tarfile.open(outputs_path(case['name'], route), 'w:xz', preset=9) as archive, lzma.open(results_path(case['name'], route), 'wt', preset=9) as results:
		for seed in case['seeds']:
			output, seconds, rss, status = runs[seed]
			data = output.encode()
			info = tarfile.TarInfo(f'{seed:03}.txt')
			info.size = len(data)
			archive.addfile(info, io.BytesIO(data))
			result = parse_output(output) if status == 0 else None
			results.write(canonical(result) + '\n')
			rows.append([route, seed, seconds if isinstance(seconds, str) else f'{seconds:.3f}', rss, status, digest(result)])

	# Keep the other routes' rows so routes can be captured one at a time.
	existing = load_metrics(case['name']) if metrics_path(case['name']).exists() else {}
	for (other_route, seed), row in existing.items():
		if other_route != route and other_route in case['routes']:
			rows.append([other_route, seed, row['seconds'], row['rss'], row['status'], row['sha256']])
	rows.sort(key=lambda row: (case['routes'].index(row[0]), case['seeds'].index(row[1])))

	args = ' '.join(shlex.quote(arg) for arg in case['args'])
	lines = [f'# args: {args}', '\t'.join(METRICS_HEADER)] + ['\t'.join(str(value) for value in row) for row in rows]
	metrics_path(case['name']).parent.mkdir(parents=True, exist_ok=True)
	metrics_path(case['name']).write_text('\n'.join(lines) + '\n')


def load_metrics(name):
	metrics = {}
	for line in metrics_path(name).read_text().splitlines():
		if line.startswith('#') or line.startswith('route\t'):
			continue
		route, seed, seconds, rss, status, sha = line.split('\t')
		metrics[(route, int(seed))] = {'seconds': seconds, 'rss': int(rss), 'status': int(status), 'sha256': sha}
	return metrics


def load_results(name, route):
	with lzma.open(results_path(name, route), 'rt') as results:
		return [json.loads(line) for line in results]


def capture(arguments):
	for case in load_cases(arguments.case):
		for route in case['routes']:
			if arguments.route and route not in arguments.route:
				continue
			print(f'{case["name"]} {route}', file=sys.stderr)
			runs = execute(arguments.binary, jobs_for([case], [route]), arguments.jobs)
			write_route(case, route, {seed: run for (_, _, seed), run in runs.items()})


def rebuild(arguments):
	for case in load_cases(arguments.case):
		metrics = load_metrics(case['name'])
		for route in case['routes']:
			runs = {}
			with tarfile.open(outputs_path(case['name'], route)) as archive:
				for seed in case['seeds']:
					output = archive.extractfile(f'{seed:03}.txt').read().decode()
					row = metrics[(route, seed)]
					runs[seed] = (output, row['seconds'], row['rss'], row['status'])
			write_route(case, route, runs)


def check(arguments):
	cases = load_cases(arguments.case)
	seeds = set(parse_seeds(arguments.seeds)) if arguments.seeds else None
	runs = execute(arguments.binary, jobs_for(cases, arguments.route, seeds), arguments.jobs)

	failures = 0
	totals = {}
	metrics = {case['name']: load_metrics(case['name']) for case in cases}
	for (name, route, seed), (output, seconds, rss, status) in sorted(runs.items()):
		expected = metrics[name][(route, seed)]
		total = totals.setdefault(name, [0, 0.0, 0.0, 0, 0])
		total[0] += 1
		total[1] += seconds
		total[2] += float(expected['seconds'])
		total[3] = max(total[3], rss)
		total[4] = max(total[4], expected['rss'])

		try:
			result = parse_output(output) if status == 0 else None
		except (ParseError, ValueError) as error:
			failures += 1
			print(f'MISMATCH {name} {route} seed {seed}: cannot parse output: {error}')
			continue
		if status == expected['status'] and digest(result) == expected['sha256']:
			continue

		failures += 1
		print(f'MISMATCH {name} {route} seed {seed} (exit {status}, expected {expected["status"]})')
		if arguments.detail:
			reference = load_results(name, route)[[case for case in cases if case['name'] == name][0]['seeds'].index(seed)]
			found = list(differences(reference, result, 'result'))
			for line in found[:arguments.detail]:
				print(f'  {line}')
			if len(found) > arguments.detail:
				print(f'  ... {len(found) - arguments.detail} more differences')

	print(f'{"case":<16} {"runs":>5} {"time":>10} {"ref time":>10} {"peak MB":>8} {"ref MB":>8}')
	for name, (count, seconds, reference_seconds, rss, reference_rss) in totals.items():
		print(f'{name:<16} {count:>5} {seconds:>9.1f}s {reference_seconds:>9.1f}s {rss // 1024:>8} {reference_rss // 1024:>8}')
	print(f'{len(runs) - failures}/{len(runs)} runs match')
	return 1 if failures else 0


def main():
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument('-b', '--binary', type=Path, default=ROOT / 'build' / 'rosa', help='solver binary to run (default: build/rosa)')
	parser.add_argument('-j', '--jobs', type=int, default=os.cpu_count(), help='parallel runs (default: CPU count)')
	parser.add_argument('-c', '--case', action='append', help='only this case (repeatable)')
	subparsers = parser.add_subparsers(dest='command', required=True)
	capture_parser = subparsers.add_parser('capture', help='run cases and overwrite their reference data, one route at a time')
	capture_parser.add_argument('-r', '--route', action='append', help='only this route (repeatable)')
	subparsers.add_parser('rebuild', help='regenerate results/ and hashes from outputs/ without re-running')
	check_parser = subparsers.add_parser('check', help='re-run cases and compare against the reference data')
	check_parser.add_argument('-r', '--route', action='append', help='only this route (repeatable)')
	check_parser.add_argument('-s', '--seeds', help='only these seeds, e.g. 0-15,40')
	check_parser.add_argument('-d', '--detail', type=int, default=10, help='differences to print per mismatch (0 for none)')
	arguments = parser.parse_args()
	arguments.binary = arguments.binary.resolve()
	if arguments.command != 'rebuild' and not os.access(TIME, os.X_OK):
		parser.error(f'{TIME} (GNU time) is needed to measure peak memory; install the "time" package')
	return {'capture': capture, 'rebuild': rebuild, 'check': check}[arguments.command](arguments)


if __name__ == '__main__':
	sys.exit(main())
