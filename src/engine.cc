#include <iomanip>
#include <iostream>
#include <map>

#include <boost/format.hpp>

#include "engine.hh"
#include "version.hh"

Engine::Engine(Parameters parameters, std::vector<std::shared_ptr<const Instruction>> instructions, Encounters encounters, Maps maps) :
		_parameters{std::move(parameters)}, _instructions{std::move(instructions)}, _encounters{std::move(encounters)}, _maps{std::move(maps)} {
	_reset(_parameters.seed);
}

void Engine::reset() {
	_instruction_index = 0;

	_frames = 0_mf;
	_encounter_frames = 0_mf;
	_minimum_frames = 0_mf;
	_score = 0.0;

	_encounter_count = 0;

	_encounter_search.clear();
	_encounter_search_area = false;

	_indent = 0;

	_full_minimum = true;

	_log.clear();
	_reset(_parameters.seed);
}

void Engine::run() {
	while (_instruction_index < _instructions.size()) {
		_cycle();
	}
}

std::string Engine::format_output(const Engine & base_engine) const {
	std::stringstream output;

	int total_optional_steps = 0;
	int total_extra_steps = 0;

	output << boost::format("ROUTE\t%1%\n") % _title;
	output << boost::format("VERSION\t%1%\n") % _version;
	output << boost::format("SPOONY\t%1%\n") % SPOONY_VERSION;
	output << boost::format("SEED\t%1%\n") % _parameters.seed;
	output << boost::format("METHOD\t%1%\n") % _parameters.algorithm;
	output << boost::format("MAXSTEP\t%1%\n") % _parameters.maximum_extra_steps;
	output << boost::format("FRAMES\t%d\n") % _frames.count();
	output << boost::format("SCORE\t%0.20f\n") % _score;

	output << "VARS\t";

	bool output_var = false;

	for (std::size_t i = 0; i < _parameters.randomizer->data.size(); i++) {
		if (_parameters.randomizer->data[i] > 0) {
			output << boost::format("%s%d:%d") % (output_var ? " " : "") % i % _parameters.randomizer->data[i];

			output_var = true;
		}
	}

	output << "\n\n";

	for (auto & entry : _log) {
		std::string indent;
		std::string text{entry.instruction->text};

		if (entry.instruction->map >= 0) {
			text = _maps.get_map(entry.instruction->map).description;
		}

		for (int i = 0; i < entry.indent; i++) {
			indent.append("  ");
		}

		output << boost::format("%-58sSeed: %3d   Index: %3d\n") % (indent + text) % entry.seed_start % entry.index_start;

		int optional_steps = std::min(entry.instruction->optional_steps, entry.steps - entry.instruction->required_steps);
		int extra_steps = entry.steps - entry.instruction->required_steps - optional_steps;

		if (extra_steps % 2 == 1 && optional_steps > 0) {
			extra_steps++;
			optional_steps--;
		}

		total_optional_steps += optional_steps;
		total_extra_steps += extra_steps;

		if (entry.instruction->optional_steps > 0) {
			output << indent << "  Optional Steps: " << optional_steps << '\n';
		}

		if (extra_steps > 0) {
			output << indent << "  Extra Steps: " << extra_steps << '\n';
		}

		if (entry.save_reset) {
			output << indent << "  Save and reset to seed " << entry.new_seed << '\n';
		}

		for (auto & pair : entry.encounters) {
			output << boost::format("%s  Step %3d: %d / %s (%0.3fs)\n") % indent % pair.first % pair.second.first % pair.second.second->get_description() % Seconds(pair.second.second->get_duration(entry.party, _parameters.tas_mode)).count();
		}

		for (auto & pair : entry.potential_encounters) {
			if (entry.encounters.count(pair.first) == 0) {
				output << boost::format("%s (Step %3d: %3d / %s)\n") % indent % pair.first % pair.second.first % pair.second.second->get_description();
			}
		}

		if (_parameters.step_output) {
			for (auto & pair : entry.step_details) 	{
				output << boost::format("%s  :: Step %3d :: %0.3f elapsed :: %0.3f remaining\n") % indent % pair.first % Seconds(pair.second).count() % Seconds(_frames - pair.second).count();
			}
		}
	}

	output << '\n';

	output << boost::format("%-18s %0.3fs\n") % "Encounter Time:" % Seconds(_encounter_frames).count();
	output << boost::format("%-18s %0.3fs\n") % "Other Time:" % Seconds(_frames - _encounter_frames).count();
	output << boost::format("%-18s %0.3fs\n") % "Total Time:" % Seconds(_frames).count();
	output << '\n';
	output << boost::format("%-18s %0.3fs\n") % "Base Total Time:" % Seconds(base_engine._frames).count();
	output << boost::format("%-18s %0.3fs\n") % "Time Saved:" % Seconds(base_engine._frames - _frames).count();
	output << '\n';
	output << boost::format("%-18s %d\n") % "Optional Steps:" % total_optional_steps;
	output << boost::format("%-18s %d\n") % "Extra Steps:" % total_extra_steps;
	output << boost::format("%-18s %d\n") % "Encounters:" % _encounter_count;
	output << '\n';
	output << boost::format("%-18s %d\n") % "Base Encounters:" % base_engine._encounter_count;
	output << boost::format("%-18s %d\n") % "Encounters Saved:" % (base_engine._encounter_count - _encounter_count);

	return output.str();
}

Milliframes Engine::get_frames() const {
	return _frames;
}

Milliframes Engine::get_minimum_frames() const {
	return _minimum_frames;
}

std::string Engine::get_title() const {
	return _title;
}

int Engine::get_version() const {
	return _version;
}

int Engine::get_maximum_steps() const {
	return _parameters.maximum_extra_steps;
}

int Engine::get_variable_count() const {
	return _parameters.randomizer->get_set_variable_count();
}

double Engine::get_score() const {
	return _score;
}

int Engine::get_initial_seed() const {
	return _parameters.seed;
}

void Engine::_cycle() {
	if (_instruction_index == _instructions.size()) {
		return;
	}

	auto instruction = _instructions[_instruction_index];

	switch (instruction->type) {
		case InstructionType::CHOICE: {
			if (_parameters.randomizer->is_implicit()) {
				_full_minimum = false;
			}

			int choice = _parameters.randomizer->get_int(0, instruction->number - 1);

			if (_parameters.maximum_extra_steps == 0) {
				choice = 0;
			}

			while (true) {
				while (_instructions[_instruction_index]->type != InstructionType::OPTION) {
					_instruction_index++;
				}

				if (choice == 0) {
					break;
				}

				_instruction_index++;
				choice--;
			}

			_transition(_instructions[_instruction_index]);
			_indent++;

			break;
		}
		case InstructionType::DELAY:
			_frames += Frames{instruction->number};
			_minimum_frames += Frames{instruction->number};
			break;
		case InstructionType::NOOP:
			for (int i = 0; i < instruction->number; i++) {
				_parameters.randomizer->get_int(0, 1);
			}

			break;
		case InstructionType::NOTE:
			_transition(instruction);
			break;
		case InstructionType::OPTION:
		case InstructionType::END:
			while (_instructions[_instruction_index]->type != InstructionType::END) {
				_instruction_index++;
			}

			_indent--;
			break;
		case InstructionType::PARTY:
			_party = instruction->text;
			break;
		case InstructionType::PATH: {
			_encounter_rate = _maps.get_map(instruction->map).encounter_rate;
			_encounter_group = _maps.get_map(instruction->map).encounter_group;
			_transition(instruction);
			_step(instruction->tiles, instruction->required_steps, false);

			if (instruction->optional_steps > 0 || (instruction->take_extra_steps && (instruction->can_single_step || instruction->can_double_step))) {
				int maximum_extra_steps = 65535;

				if (!instruction->take_extra_steps) {
					maximum_extra_steps = instruction->optional_steps;
				}

				if (_parameters.randomizer->is_implicit()) {
					_full_minimum = false;
				}

				int steps = _parameters.randomizer->get_int(0, maximum_extra_steps);
				int optional_steps = std::min(instruction->optional_steps, steps);
				int extra_steps = steps - optional_steps;
				int tiles = 0;

				if (extra_steps % 2 == 1 && optional_steps > 0) {
					extra_steps++;
					optional_steps--;
				}

				if (extra_steps % 2 == 1 && !instruction->can_single_step) {
					extra_steps--;
				}

				_score -= steps * 1000;
				_score += (_parameters.randomizer->get_index() * static_cast<double>(optional_steps) * 0.5 * 0.001);
				_score += (_parameters.randomizer->get_index() * static_cast<double>(static_cast<int>(extra_steps / 2)) * 0.001);
				_score += (_parameters.randomizer->get_index() * (extra_steps % 2 == 1 ? 0.0000001 : 0));

				if (instruction->can_double_step) {
					tiles = extra_steps;
				} else {
					tiles = extra_steps * 2;
				}

				if (tiles % 2 == 1) {
					tiles++;
				}

				_step(tiles, optional_steps + extra_steps, false);
			}

			break;
		}
		case InstructionType::ROUTE:
			_title = instruction->text;
			break;
		case InstructionType::SAVE: {
			_transition(instruction);

			if (_parameters.randomizer->is_implicit()) {
				_full_minimum = false;
			}

			int value = _parameters.randomizer->get_int(0, 256);

			if (value > 0) {
				int seed = value - 1;
				Milliframes new_frames = 697_f - Frames{instruction->number};

				_reset(seed);

				_frames += new_frames;
				_minimum_frames += new_frames;

				_log.back().save_reset = true;
				_log.back().new_seed = seed;
			}

			break;
		}
		case InstructionType::SEARCH:
			_encounter_search = instruction->numbers;
			_encounter_search_area = true;
			_encounter_search_party = instruction->party;

			_transition(instruction);
			_indent++;
			break;
		case InstructionType::VERSION:
			_version = instruction->number;
			break;
		case InstructionType::WAIT:
			while (!_encounter_search.empty()) {
				_step(2, 2, false);
			}

			_encounter_search_area = false;

			_indent -= 1;

			break;
	}

	_instruction_index++;
}

std::shared_ptr<const Encounter> Engine::_get_encounter() {
	if ((rng_data[static_cast<std::size_t>(_step_index)] + _step_seed) % 256 < _encounter_rate) {
		int value = (rng_data[static_cast<std::size_t>(_encounter_index)] + _encounter_seed) % 256;
		std::size_t i;

		if (value < 43) {
			i = 0;
		} else if (value < 86) {
			i = 1;
		} else if (value < 129) {
			i = 2;
		} else if (value < 172) {
			i = 3;
		} else if (value < 204) {
			i = 4;
		} else if (value < 236) {
			i = 5;
		} else if (value < 252) {
			i = 6;
		} else {
			i = 7;
		}

		return _encounters.get_encounter_from_group(static_cast<std::size_t>(_encounter_group), i);
	}

	return nullptr;
}

void Engine::_reset(int seed) {
	_step_seed = seed;
	_encounter_seed = (seed * 2) % 256;

	_step_index = 0;
	_encounter_index = 0;

	_encounter_rate = 0;
	_encounter_group = 0;
}



void Engine::_step(int tiles, int steps, bool simulate) {
	Milliframes output_frames = _frames;

	if (!simulate) {
		_frames += tiles * 16_f;
		_minimum_frames += tiles * 16_f;
	}

	int log_steps = _log.back().steps;

	int step_index = _step_index;
	int step_seed = _step_seed;

	int encounter_index = _encounter_index;
	int encounter_seed = _encounter_seed;

	for (int i = 0; i < steps; i++) {
		output_frames += 16_f;
		_log.back().steps++;

		_step_index++;

		if (_step_index == 256) {
			_step_index = 0;
			_step_seed += 17;

			if (_step_seed > 255) {
				_step_seed -= 256;
			}
		}

		auto encounter = _get_encounter();

		if (!simulate && encounter && !_encounter_search.empty() && _encounter_search.count(static_cast<int>(encounter->get_id())) > 0) {
			_party = _encounter_search_party;
			_encounter_search.clear();
		}

		if (encounter) {
			if (simulate) {
				_log.back().potential_encounters[_log.back().steps] = std::make_pair(_encounter_index + 1, encounter);
			} else {
				_log.back().encounters[_log.back().steps] = std::make_pair(_encounter_index + 1, encounter);
			}

			Milliframes duration = encounter->get_duration(_party, _parameters.tas_mode);

			if (!simulate) {
				_frames += duration;
				_encounter_frames += duration;
				output_frames += duration;
				_encounter_count++;
			}

			if (!simulate && _full_minimum) {
				_minimum_frames += duration;
			}

			_encounter_index = (_encounter_index + 1) % 256;

			if (_encounter_index == 0) {
				_encounter_seed = (_encounter_seed + 17) % 256;
			}

		}

		if (!simulate && _parameters.step_output) {
			_log.back().step_details[_log.back().steps] = output_frames;
		}
	}

	if (simulate) {
		_log.back().steps = log_steps;

		_step_index = step_index;
		_step_seed = step_seed;

		_encounter_index = encounter_index;
		_encounter_seed = encounter_seed;
	}
}

void Engine::_transition(const std::shared_ptr<const Instruction> & instruction) {
	_frames += instruction->transition_count * 82_f;
	_minimum_frames += instruction->transition_count * 82_f;
	_log.push_back(LogEntry{instruction, _indent});

	_log.back().seed_start = _step_seed;
	_log.back().index_start = _step_index;
	_log.back().party = _party;

	if (_encounter_search_area && instruction->type == InstructionType::PATH) {
		_step(256, 256, true);
	}
}
