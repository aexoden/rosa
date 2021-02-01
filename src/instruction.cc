#include <iostream>
#include <locale>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "instruction.hh"

static auto parse_variable(const std::string & s) -> int {
	if (s == "-") {
		return -1;
	}

	try {
		int result{std::stoi(s, nullptr, 16)}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		return result;
	} catch (...) {
		std::cerr << "WARNING: Ignoring invalid variable name: " << s << '\n';
		return -1;
	}
}

Instruction::Instruction(const std::string & line) :
		expression_string(std::make_shared<std::string>()) {
	std::vector<std::string> tokens;
	boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

	if (!tokens.empty()) {
		try {
			if (tokens[0] == "CHOICE" && tokens.size() == 3) {
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
			} else if (tokens[0] == "PATH" && tokens.size() == 10) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				type = InstructionType::Path;
				variable = parse_variable(tokens[1]);
				map = std::stoi(tokens[2], nullptr, 16); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				tiles = std::stoi(tokens[3]);
				required_steps = std::stoi(tokens[4]);
				optional_steps = std::stoi(tokens[5]); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				transition_count = std::stoi(tokens[6]); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				can_single_step = tokens[7] == "+"; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				can_double_step = tokens[8] == "+"; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				can_step_during_save = tokens[9] == "+"; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

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

				std::locale locale;
				std::string current_number;
				size_t current_index{0};

				for (const auto & c : tokens[2]) {
					if (isdigit(c, locale)) {
						current_number.push_back(c);
					} else {
						if (current_number.length() > 0) {
							numbers.push_back(std::stoi(current_number));
							expression_string->push_back('0' + current_index);

							current_number = "";
							current_index++;
						}

						expression_string->push_back(c);
					}
				}

				if (current_number.length() > 0) {
					numbers.push_back(std::stoi(current_number));
					expression_string->push_back('0' + current_index);
				}

				peg::parser parser(R"(
					sequence <- disjunction ('>' disjunction)*
					disjunction <- conjunction ('|' conjunction)*
					conjunction <- unit ('+' unit)*
					unit <- number / '(' sequence ')'
					number <- < [0-9]+ >
					%whitespace <- [ ]*
				)");

				parser.enable_ast();

				if (parser.parse(*expression_string, expression)) {
					expression = parser.optimize_ast(expression);
				} else {
					std::cerr << "WARNING: Invalid expression in search: " << expression_string << std::endl;
				}

				party = tokens[3];
			} else if (tokens[0] == "VERSION" && tokens.size() == 2) {
				type = InstructionType::Version;
				number = std::stoi(tokens[1]);
			} else if (tokens[0] == "DATA" && tokens.size() == 2) {
				type = InstructionType::Data;
				text = tokens[1];
			} else {
				std::cerr << "WARNING: Unrecognized instruction: " << line << std::endl;
			}
		} catch (...) {
			std::cerr << "WARNING: Malformed line in route definition: " << line << std::endl;
		}
	}
}

auto read_route(std::istream & input) -> Route {
	std::string line;
	Route route;

	while (std::getline(input, line)) {
		if (line.length() > 0 && line.at(0) != '#') {
			std::vector<std::string> tokens;
			boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

			if (tokens[0] == "WAIT") {
				route[route.size() - 1].end_search = true;
			} else {
				route.emplace_back(line);
			}
		}
	}

	return route;
}
