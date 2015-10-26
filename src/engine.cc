/*
 * Copyright (c) 2015 Jason Lynch <jason@calindora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <iomanip>
#include <map>

#include "engine.hh"
#include "version.hh"

std::vector<int> FF4_RNG_DATA{
	0x07, 0xB6, 0xF0, 0x1F, 0x55, 0x5B, 0x37, 0xE3, 0xAE, 0x4F, 0xB2, 0x5E, 0x99, 0xF6, 0x77, 0xCB,
	0x60, 0x8F, 0x43, 0x3E, 0xA7, 0x4C, 0x2D, 0x88, 0xC7, 0x68, 0xD7, 0xD1, 0xC2, 0xF2, 0xC1, 0xDD,
	0xAA, 0x93, 0x16, 0xF7, 0x26, 0x04, 0x36, 0xA1, 0x46, 0x4E, 0x56, 0xBE, 0x6C, 0x6E, 0x80, 0xD5,
	0xB5, 0x8E, 0xA4, 0x9E, 0xE7, 0xCA, 0xCE, 0x21, 0xFF, 0x0F, 0xD4, 0x8C, 0xE6, 0xD3, 0x98, 0x47,
	0xF4, 0x0D, 0x15, 0xED, 0xC4, 0xE4, 0x35, 0x78, 0xBA, 0xDA, 0x27, 0x61, 0xAB, 0xB9, 0xC3, 0x7D,
	0x85, 0xFC, 0x95, 0x6B, 0x30, 0xAD, 0x86, 0x00, 0x8D, 0xCD, 0x7E, 0x9F, 0xE5, 0xEF, 0xDB, 0x59,
	0xEB, 0x05, 0x14, 0xC9, 0x24, 0x2C, 0xA0, 0x3C, 0x44, 0x69, 0x40, 0x71, 0x64, 0x3A, 0x74, 0x7C,
	0x84, 0x13, 0x94, 0x9C, 0x96, 0xAC, 0xB4, 0xBC, 0x03, 0xDE, 0x54, 0xDC, 0xC5, 0xD8, 0x0C, 0xB7,
	0x25, 0x0B, 0x01, 0x1C, 0x23, 0x2B, 0x33, 0x3B, 0x97, 0x1B, 0x62, 0x2F, 0xB0, 0xE0, 0x73, 0xCC,
	0x02, 0x4A, 0xFE, 0x9B, 0xA3, 0x6D, 0x19, 0x38, 0x75, 0xBD, 0x66, 0x87, 0x3F, 0xAF, 0xF3, 0xFB,
	0x83, 0x0A, 0x12, 0x1A, 0x22, 0x53, 0x90, 0xCF, 0x7A, 0x8B, 0x52, 0x5A, 0x49, 0x6A, 0x72, 0x28,
	0x58, 0x8A, 0xBF, 0x0E, 0x06, 0xA2, 0xFD, 0xFA, 0x41, 0x65, 0xD2, 0x4D, 0xE2, 0x5C, 0x1D, 0x45,
	0x1E, 0x09, 0x11, 0xB3, 0x5F, 0x29, 0x79, 0x39, 0x2E, 0x2A, 0x51, 0xD9, 0x5D, 0xA6, 0xEA, 0x31,
	0x81, 0x89, 0x10, 0x67, 0xF5, 0xA9, 0x42, 0x82, 0x70, 0x9D, 0x92, 0x57, 0xE1, 0x3D, 0xF1, 0xF9,
	0xEE, 0x08, 0x91, 0x18, 0x20, 0xB1, 0xA5, 0xBB, 0xC6, 0x48, 0x50, 0x9A, 0xD6, 0x7F, 0x7B, 0xE9,
	0x76, 0xDF, 0x32, 0x6F, 0x34, 0xA8, 0xD0, 0xB8, 0x63, 0xC8, 0xC0, 0xEC, 0x4B, 0xE8, 0x17, 0xF8
};

Engine::Engine(const Parameters & parameters, const std::vector<std::shared_ptr<const Instruction>> & instructions, const Encounters & encounters) :
	_parameters{parameters},
	_instructions{instructions},
	_encounters{encounters}
{
	_reset(_parameters.seed);
}

void Engine::reset()
{
	_instruction_index = 0;

	_frames = 0;
	_encounter_frames = 0;
	_minimum_frames = 0;

	_encounter_count = 0;

	_encounter_search.clear();
	_encounter_search_area = false;

	_indent = 0;

	_full_minimum = true;

	_log.clear();
	_reset(_parameters.seed);
}

void Engine::run()
{
	while (_instruction_index < _instructions.size())
	{
		_cycle();
	}
}

Glib::ustring format_label(const Glib::ustring & label)
{
	return Glib::ustring::format(std::left, std::setw(18), label);
}

Glib::ustring format_time(double frames)
{
	return Glib::ustring::format(std::fixed, std::setprecision(3), Engine::frames_to_seconds(frames));
}

Glib::ustring Engine::format_output(const Engine & base_engine) const
{
	Glib::ustring output;

	int total_optional_steps = 0;
	int total_extra_steps = 0;

	output.append(Glib::ustring::compose("ROUTE\t%1\n", _title));
	output.append(Glib::ustring::compose("VERSION\t%1\n", _version));
	output.append(Glib::ustring::compose("SPOONY\t%1\n", SPOONY_VERSION));
	output.append(Glib::ustring::compose("SEED\t%1\n", _parameters.seed));
	output.append(Glib::ustring::compose("METHOD\t%1\n", _parameters.algorithm));
	output.append(Glib::ustring::compose("MAXSTEP\t%1\n", _parameters.maximum_extra_steps));
	output.append(Glib::ustring::compose("FRAMES\t%1\n", Glib::ustring::format(std::setprecision(20), _frames)));

	output.append("VARS\t");

	bool output_var = false;

	for (decltype(_parameters.randomizer->data)::size_type i = 0; i < _parameters.randomizer->data.size(); i++)
	{
		if (_parameters.randomizer->data[i] > 0)
		{
			output.append(Glib::ustring::compose("%1%2:%3", (output_var ? " " : ""), i, _parameters.randomizer->data[i]));

			output_var = true;
		}
	}

	output.append("\n\n");

	for (auto & entry : _log)
	{
		Glib::ustring indent = "";

		for (int i = 0; i < entry.indent; i++)
		{
			indent.append("  ");
		}

		output.append(Glib::ustring::compose("%1%2\n", indent, entry.instruction->text));

		int optional_steps = std::min(entry.instruction->optional_steps, entry.steps - entry.instruction->required_steps);
		int extra_steps = entry.steps - entry.instruction->required_steps - optional_steps;

		if (extra_steps % 2 == 1 && optional_steps > 0)
		{
			extra_steps++;
			optional_steps--;
		}

		total_optional_steps += optional_steps;
		total_extra_steps += extra_steps;

		if (entry.instruction->optional_steps > 0)
		{
			output.append(Glib::ustring::compose("%1  Optional Steps: %2\n", indent, optional_steps));
		}

		if (extra_steps > 0)
		{
			output.append(Glib::ustring::compose("%1  Extra Steps: %2\n", indent, extra_steps));
		}

		if (entry.save_reset)
		{
			output.append(Glib::ustring::compose("%1  Save and reset to seed %2\n", indent, entry.new_seed));
		}

		for (auto & pair : entry.encounters)
		{
			output.append(Glib::ustring::compose("%1  Step %2: %3 / %4 (%5s)\n", indent, Glib::ustring::format(std::setw(3), pair.first), pair.second.first, pair.second.second->get_description(), format_time(pair.second.second->get_average_duration())));
		}

		for (auto & pair : entry.potential_encounters)
		{
			if (entry.encounters.count(pair.first) == 0)
			{
				output.append(Glib::ustring::compose("%1 (Step %2: %3 / %4)\n", indent, Glib::ustring::format(std::setw(3), pair.first), pair.second.first, pair.second.second->get_description(), format_time(pair.second.second->get_average_duration())));
			}
		}
	}

	output.append("\n");

	output.append(Glib::ustring::compose("%1 %2s\n", format_label("Encounter Time:"), format_time(_encounter_frames)));
	output.append(Glib::ustring::compose("%1 %2s\n", format_label("Other Time:"), format_time(_frames - _encounter_frames)));
	output.append(Glib::ustring::compose("%1 %2s\n", format_label("Total Time:"), format_time(_frames)));
	output.append("\n");
	output.append(Glib::ustring::compose("%1 %2s\n", format_label("Base Total Time:"), format_time(base_engine._frames)));
	output.append(Glib::ustring::compose("%1 %2s\n", format_label("Time Saved:"), format_time(base_engine._frames - _frames)));
	output.append("\n");
	output.append(Glib::ustring::compose("%1 %2\n", format_label("Optional Steps:"), total_optional_steps));
	output.append(Glib::ustring::compose("%1 %2\n", format_label("Extra Steps:"), total_extra_steps));
	output.append(Glib::ustring::compose("%1 %2\n", format_label("Encounters:"), _encounter_count));
	output.append("\n");
	output.append(Glib::ustring::compose("%1 %2\n", format_label("Base Encounters:"), base_engine._encounter_count));
	output.append(Glib::ustring::compose("%1 %2\n", format_label("Encounters Saved:"), base_engine._encounter_count - _encounter_count));

	return output;
}

double Engine::get_frames() const
{
	return _frames;
}

double Engine::get_minimum_frames() const
{
	return _minimum_frames;
}

Glib::ustring Engine::get_title() const
{
	return _title;
}

int Engine::get_version() const
{
	return _version;
}

int Engine::get_maximum_steps() const
{
	return _parameters.maximum_extra_steps;
}

int Engine::get_variable_count() const
{
	return _parameters.randomizer->get_set_variable_count();
}

int Engine::get_initial_seed() const
{
	return _parameters.seed;
}

double Engine::frames_to_seconds(double frames)
{
	return frames / 60.0988;
}

void Engine::_cycle()
{
	if (_instruction_index == _instructions.size())
	{
		return;
	}

	auto instruction = _instructions[_instruction_index];

	switch (instruction->type)
	{
		case InstructionType::CHOICE:
		{
			if (_parameters.randomizer->is_implicit())
			{
				_full_minimum = false;
			}

			int choice = _parameters.randomizer->get_int(0, instruction->number - 1);

			if (_parameters.maximum_extra_steps == 0)
			{
				choice = 0;
			}

			while (true)
			{
				while (_instructions[_instruction_index]->type != InstructionType::OPTION)
				{
					_instruction_index++;
				}

				if (choice == 0)
				{
					break;
				}

				_instruction_index++;
				choice--;
			}

			_transition(_instructions[_instruction_index]);
			_indent++;

			break;
		}
		case InstructionType::NOOP:
			for (int i = 0; i < instruction->number; i++)
			{
				_parameters.randomizer->get_int(0, 1);
			}

			break;
		case InstructionType::NOTE:
			_transition(instruction);
			break;
		case InstructionType::OPTION:
		case InstructionType::END:
			while (_instructions[_instruction_index]->type != InstructionType::END)
			{
				_instruction_index++;
			}

			_indent--;
			break;
		case InstructionType::PATH:
			_encounter_rate = instruction->encounter_rate;
			_encounter_group = instruction->encounter_group;
			_transition(instruction);
			_step(instruction->tiles, instruction->required_steps, false);

			if (instruction->optional_steps > 0 || (instruction->take_extra_steps && (instruction->can_single_step || instruction->can_double_step)))
			{
				int maximum_extra_steps = 65535;

				if (!instruction->take_extra_steps)
				{
					maximum_extra_steps = instruction->optional_steps;
				}

				if (_parameters.randomizer->is_implicit())
				{
					_full_minimum = false;
				}

				int steps = _parameters.randomizer->get_int(0, maximum_extra_steps);
				int optional_steps = std::min(instruction->optional_steps, steps);
				int extra_steps = steps - optional_steps;
				int tiles = 0;

				if (extra_steps % 2 == 1 && optional_steps > 0)
				{
					extra_steps++;
					optional_steps--;
				}

				if (extra_steps % 2 == 1 && !instruction->can_single_step)
				{
					extra_steps--;
				}

				if (instruction->can_double_step)
				{
					tiles = extra_steps;
				}
				else
				{
					tiles = extra_steps * 2;
				}

				if (tiles % 2 == 1)
				{
					tiles++;
				}

				_step(tiles, optional_steps + extra_steps, false);
			}

			break;
		case InstructionType::ROUTE:
			_title = instruction->text;
			break;
		case InstructionType::SAVE:
		{
			_transition(instruction);

			if (_parameters.randomizer->is_implicit())
			{
				_full_minimum = false;
			}

			int value = _parameters.randomizer->get_int(0, 256);

			if (value > 0)
			{
				int seed = value - 1;
				int frames = 697 - instruction->number;

				_reset(seed);

				_frames += frames;
				_minimum_frames += frames;

				_log.back().save_reset = true;
				_log.back().new_seed = seed;
			}

			break;
		}
		case InstructionType::SEARCH:
			_encounter_search = instruction->numbers;
			_encounter_search_area = true;

			_transition(instruction);
			_indent++;
			break;
		case InstructionType::VERSION:
			_version = instruction->number;
			break;
		case InstructionType::WAIT:
			while (_encounter_search.size() > 0)
			{
				_step(2, 2, false);
			}

			_encounter_search_area = false;

			_indent -= 1;

			break;
	}

	_instruction_index++;
}

std::shared_ptr<const Encounter> Engine::_get_encounter()
{
	if ((FF4_RNG_DATA[_step_index] + _step_seed) % 256 < _encounter_rate)
	{
		int value = (FF4_RNG_DATA[_encounter_index] + _encounter_seed) % 256;
		int i;

		if (value < 43)
		{
			i = 0;
		}
		else if (value < 86)
		{
			i = 1;
		}
		else if (value < 129)
		{
			i = 2;
		}
		else if (value < 172)
		{
			i = 3;
		}
		else if (value < 204)
		{
			i = 4;
		}
		else if (value < 236)
		{
			i = 5;
		}
		else if (value < 252)
		{
			i = 6;
		}
		else
		{
			i = 7;
		}

		return _encounters.get_encounter_from_group(_encounter_group, i);
	}

	return nullptr;
}

void Engine::_reset(int seed)
{
	_step_seed = seed;
	_encounter_seed = (seed * 2) % 256;

	_step_index = 0;
	_encounter_index = 0;

	_encounter_rate = 0;
	_encounter_group = 0;
}



void Engine::_step(int tiles, int steps, bool simulate)
{
	if (!simulate)
	{
		_frames += tiles * 16;
		_minimum_frames += tiles * 16;
	}

	int log_steps = _log.back().steps;

	int step_index = _step_index;
	int step_seed = _step_seed;

	int encounter_index = _encounter_index;
	int encounter_seed = _encounter_seed;

	for (int i = 0; i < steps; i++)
	{
		_log.back().steps++;

		_step_index++;

		if (_step_index == 256)
		{
			_step_index = 0;
			_step_seed += 17;

			if (_step_seed > 255)
			{
				_step_seed -= 256;
			}
		}

		auto encounter = _get_encounter();

		if (!simulate && encounter && _encounter_search.size() > 0 && _encounter_search.count(encounter->get_id()) > 0)
		{
			_encounter_search.clear();
		}

		if (encounter)
		{
			if (simulate)
			{
				_log.back().potential_encounters[_log.back().steps] = std::make_pair(_encounter_index + 1, encounter);
			}
			else
			{
				_log.back().encounters[_log.back().steps] = std::make_pair(_encounter_index + 1, encounter);
			}

			if (!simulate)
			{
				_frames += encounter->get_average_duration();
				_encounter_frames += encounter->get_average_duration();
				_encounter_count++;
			}

			if (!simulate && _full_minimum)
			{
				_minimum_frames += encounter->get_average_duration();
			}

			_encounter_index = (_encounter_index + 1) % 256;

			if (_encounter_index == 0)
			{
				_encounter_seed = (_encounter_seed + 17) % 256;
			}
		}
	}

	if (simulate)
	{
		_log.back().steps = log_steps;

		_step_index = step_index;
		_step_seed = step_seed;

		_encounter_index = encounter_index;
		_encounter_seed = encounter_seed;
	}
}

void Engine::_transition(const std::shared_ptr<const Instruction> & instruction)
{
	_frames += instruction->transition_count * 82;
	_minimum_frames += instruction->transition_count * 82;
	_log.push_back(LogEntry{instruction, _indent});

	if (_encounter_search_area && instruction->type == InstructionType::PATH)
	{
		_step(256, 256, true);
	}
}

LogEntry::LogEntry(const std::shared_ptr<const Instruction> & instruction, int indent) :
	instruction(instruction),
	indent(indent)
{ }
