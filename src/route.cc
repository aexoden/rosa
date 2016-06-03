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

#include <fstream>
#include <iostream>
#include <string>

#include <boost/algorithm/string.hpp>

#include "map.hh"
#include "route.hh"

Route::Route(const std::string & filename, std::map<unsigned int, std::shared_ptr<const Map>> maps) :
	_maps(maps)
{
	std::ifstream file{filename};
	std::string line;

	if (file.is_open())
	{
		while (std::getline(file, line))
		{
			_parse_line(line);
		}
	}

	file.close();
}

std::string Route::name() const
{
	return _name;
}

unsigned int Route::version() const
{
	return _version;
}

void Route::_parse_line(const std::string & line)
{
	if (!line.empty() && line[0] != '#')
	{
		std::vector<std::string> tokens;
		boost::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

		if (tokens[0] == "ROUTE")
		{
			_name = tokens[1];
		}
		else if (tokens[0] == "VERSION")
		{
			_version = static_cast<unsigned int>(std::stoul(tokens[1]));
		}
		else
		{
			std::cerr << "WARNING: Unrecognized line in route definition: " << line << std::endl;
		}
	}
}

/*
#include <iostream>
#include <vector>

#include <giomm/datainputstream.h>
#include <giomm/file.h>
#include <glibmm/regex.h>

#include "instruction.hh"


std::vector<std::shared_ptr<const Instruction>> read_instructions(const Glib::RefPtr<Gio::File> & file)
{
	std::vector<std::shared_ptr<const Instruction>> instructions;
	Glib::RefPtr<Gio::DataInputStream> file_stream = Gio::DataInputStream::create(file->read());
	std::string line;

	while (file_stream->read_line(line))
	{
		instructions.push_back(std::make_shared<const Instruction>(line));
	}

	return instructions;

Instruction::Instruction(const Glib::ustring & line)
{
	std::vector<Glib::ustring> tokens = Glib::Regex::split_simple("\t+", line);

	if (!tokens.empty())
	{
		if (tokens[0] == "CHOICE" && tokens.size() == 2)
		{
			type = InstructionType::CHOICE;
			number = std::stoi(tokens[1]);
		}
		else if (tokens[0] == "END" && tokens.size() == 1)
		{
			type = InstructionType::END;
		}
		else if (tokens[0] == "NOOP" && tokens.size() == 2)
		{
			type = InstructionType::NOOP;
			number = std::stoi(tokens[1]);
		}
		else if (tokens[0] == "NOTE" && tokens.size() == 2)
		{
			type = InstructionType::NOTE;
			text = tokens[1];
		}
		else if (tokens[0] == "OPTION" && tokens.size() == 2)
		{
			type = InstructionType::OPTION;
			text = tokens[1];
		}
		else if (tokens[0] == "PATH" && tokens.size() == 12)
		{
			type = InstructionType::PATH;
			text = tokens[1];
			tiles = std::stoi(tokens[2]);
			required_steps = std::stoi(tokens[3]);
			optional_steps = std::stoi(tokens[4]);
			encounter_rate = std::stoi(tokens[5]);
			encounter_group = std::stoi(tokens[6]);
			transition_count = std::stoi(tokens[7]);
			take_extra_steps = tokens[8] == "+";
			can_single_step = tokens[9] == "+";
			can_double_step = tokens[10] == "+";
			is_world_map = tokens[11] == "+";
		}
		else if (tokens[0] == "ROUTE" && tokens.size() == 2)
		{
			type = InstructionType::ROUTE;
			text = tokens[1];
		}
		else if (tokens[0] == "SAVE" && tokens.size() == 3)
		{
			type = InstructionType::SAVE;
			text = tokens[1];
			number = std::stoi(tokens[2]);
		}
		else if (tokens[0] == "SEARCH" && tokens.size() == 3)
		{
			type = InstructionType::SEARCH;
			text = tokens[1];

			for (const auto & number : Glib::Regex::split_simple("\\+", tokens[2]))
			{
				numbers.insert(std::stoi(number));
			}
		}
		else if (tokens[0] == "VERSION" && tokens.size() == 2)
		{
			type = InstructionType::VERSION;
			number = std::stoi(tokens[1]);
		}
		else if (tokens[0] == "WAIT" && tokens.size() == 1)
		{
			type = InstructionType::WAIT;
		}
		else
		{
			std::cerr << "Unrecognized instruction: " << line << std::endl;
		}
	}
}

}*/
