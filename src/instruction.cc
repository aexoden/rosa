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

#include <iostream>
#include <vector>

#include <giomm/datainputstream.h>
#include <giomm/file.h>
#include <glibmm/regex.h>

#include "instruction.hh"

Instruction::Instruction(const Glib::ustring & line)
{
	std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("\t+", line);

	if (!tokens.empty())
	{
		if (tokens[0] == "CHOICE" && tokens.size() == 2)
		{
			_type = InstructionType::CHOICE;
			_count = std::stoi(tokens[1]);
		}
		else if (tokens[0] == "END" && tokens.size() == 1)
		{
			_type = InstructionType::END;
		}
		else if (tokens[0] == "NOOP" && tokens.size() == 2)
		{
			_type = InstructionType::NOOP;
			_count = std::stoi(tokens[1]);
		}
		else if (tokens[0] == "NOTE" && tokens.size() == 2)
		{
			_type = InstructionType::NOTE;
			_text = tokens[1];
		}
		else if (tokens[0] == "OPTION" && tokens.size() == 2)
		{
			_type = InstructionType::OPTION;
			_text = tokens[1];
		}
		else if (tokens[0] == "PATH" && tokens.size() == 12)
		{
			_type = InstructionType::PATH;
			_text = tokens[1];
			_tiles = std::stoi(tokens[2]);
			_required_steps = std::stoi(tokens[3]);
			_optional_steps = std::stoi(tokens[4]);
			_encounter_rate = std::stoi(tokens[5]);
			_encounter_group = std::stoi(tokens[6]);
			_transition_count = std::stoi(tokens[7]);
			_take_extra_steps = tokens[8] == "+";
			_can_single_step = tokens[9] == "+";
			_can_double_step = tokens[10] == "+";
			_is_world_map = tokens[11] == "+";
		}
		else if (tokens[0] == "ROUTE" && tokens.size() == 2)
		{
			_type = InstructionType::ROUTE;
			_text = tokens[1];
		}
		else if (tokens[0] == "SEARCH" && tokens.size() == 3)
		{
			_type = InstructionType::SEARCH;
			_text = tokens[1];
			_count = std::stoi(tokens[2]);
		}
		else if (tokens[0] == "VERSION" && tokens.size() == 2)
		{
			_type = InstructionType::VERSION;
			_text = tokens[1];
		}
		else if (tokens[0] == "WAIT" && tokens.size() == 1)
		{
			_type = InstructionType::WAIT;
		}
		else
		{
			std::cerr << "Unrecognized instruction: " << line << std::endl;
		}
	}
}

std::vector<Instruction> read_instructions(const Glib::RefPtr<Gio::File> & file)
{
	std::vector<Instruction> instructions;
	Glib::RefPtr<Gio::DataInputStream> file_stream = Gio::DataInputStream::create(file->read());
	std::string line;

	while (file_stream->read_line(line))
	{
		instructions.push_back(Instruction{line});
	}

	return instructions;
}
