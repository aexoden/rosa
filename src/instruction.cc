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
			type = InstructionType::CHOICE;
			number = std::stoi(tokens[1]);
		}
		else if (tokens[0] == "DELAY" && tokens.size() == 2)
		{
			type = InstructionType::DELAY;
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
		else if (tokens[0] == "PARTY" && tokens.size() == 2)
		{
			type = InstructionType::PARTY;
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
		else if (tokens[0] == "SEARCH" && tokens.size() == 4)
		{
			type = InstructionType::SEARCH;
			text = tokens[1];

			for (const auto & number : Glib::Regex::split_simple("\\+", tokens[2]))
			{
				numbers.insert(std::stoi(number));
			}

			party = tokens[3];
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
}
