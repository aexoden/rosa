#include <iostream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "instruction.hh"

static int parse_variable(const std::string & s) {
	if (s == "-") {
		return -1;
	}

	try {
		int result{std::stoi(s, nullptr, 16)};
		return result;
	} catch (...) {
		std::cerr << "WARNING: Ignoring invalid variable name: " << s << '\n';
		return -1;
	}
}

Instruction::Instruction(const std::string & line) {
	std::vector<std::string> tokens;
	boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

	if (!tokens.empty()) {
		try {
			if (tokens[0] == "CHOICE" && tokens.size() == 2) {
				type = InstructionType::Choice;
				variable = parse_variable(tokens[1]);
				number = std::stoi(tokens[2]);
			} else if (tokens[0] == "DELAY" && tokens.size() == 2) {
				type = InstructionType::Delay;
				number = std::stoi(tokens[1]);
			} else if (tokens[0] == "END" && tokens.size() == 1) {
				type = InstructionType::End;
			} else if (tokens[0] == "NOTE" && tokens.size() == 2) {
				type = InstructionType::Note;
				text = tokens[1];
			} else if (tokens[0] == "OPTION" && tokens.size() == 2) {
				type = InstructionType::Option;
				text = tokens[1];
			} else if (tokens[0] == "PARTY" && tokens.size() == 2) {
				type = InstructionType::Party;
				text = tokens[1];
			} else if (tokens[0] == "PATH" && tokens.size() == 10) {
				type = InstructionType::Path;
				variable = parse_variable(tokens[1]);
				map = std::stoi(tokens[2], nullptr, 16);
				tiles = std::stoi(tokens[3]);
				required_steps = std::stoi(tokens[4]);
				optional_steps = std::stoi(tokens[5]);
				transition_count = std::stoi(tokens[6]);
				can_single_step = tokens[7] == "+";
				can_double_step = tokens[8] == "+";
				can_step_during_save = tokens[9] == "+";

				if (variable >= 0 && (optional_steps == 0 && !can_single_step && !can_double_step)) {
					std::cerr << "WARNING: Path segment with no extra steps available has assigned variable: " << line << '\n';
				}
			} else if (tokens[0] == "ROUTE" && tokens.size() == 2) {
				type = InstructionType::Route;
				text = tokens[1];
			} else if (tokens[0] == "SAVE" && tokens.size() == 3) {
				type = InstructionType::Save;
				variable = parse_variable(tokens[1]);
				text = tokens[2];
				number = std::stoi(tokens[3]);
			} else if (tokens[0] == "SEARCH" && tokens.size() == 4) {
				type = InstructionType::Search;
				text = tokens[1];

				std::vector<std::string> temp_numbers;
				boost::algorithm::split(temp_numbers, tokens[2], boost::is_any_of("+"), boost::token_compress_on);

				for (const auto & temp_number : temp_numbers) {
					numbers.push_back(std::stoi(temp_number));
				}

				party = tokens[3];
			} else if (tokens[0] == "VERSION" && tokens.size() == 2) {
				type = InstructionType::Version;
				number = std::stoi(tokens[1]);
			} else {
				std::cerr << "WARNING: Unrecognized instruction: " << line << std::endl;
			}
		} catch (...) {
			std::cerr << "WARNING: Malformed line in route definition: " << line << std::endl;
		}
	}
}

Route read_route(std::istream & input) {
	std::string line;
	Route route;

	while (std::getline(input, line)) {
		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

		if (tokens[0] == "WAIT") {
			route[route.size() - 1].end_search = true;
		} else {
			route.emplace_back(line);
		}
	}

	return route;
}
