#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "engine.hh"
#include "randomizer.hh"
#include "route_output.hh"
#include "version.hh"

RouteOutput::RouteOutput(std::istream & input) {
	std::string line;

	while (std::getline(input, line)) {
		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

		if (!tokens.empty()) {
			if (tokens[0] == "FRAMES" && tokens.size() == 2) {
				_frames = Milliframes{std::stoi(tokens[1])};
			} else if (tokens[0] == "SCORE" && tokens.size() == 2) {
				_score = std::stod(tokens[1]);
			} else if (tokens[0] == "MAXSTEP" && tokens.size() == 2) {
				_maximum_steps = std::stoi(tokens[1]);
			} else if (tokens[0] == "SPOONY" && tokens.size() == 2) {
				_spoony_version = tokens[1];
			} else if (tokens[0] == "VARS" && tokens.size() == 2) {
				_variables = parse_variable_data(tokens[1]);
			} else if (tokens[0] == "VERSION" && tokens.size() == 2) {
				_version = std::stoi(tokens[1]);
			}
		}

		_valid = true;
	}
}

bool RouteOutput::is_valid(int current_version) const {
	return _valid && current_version <= _version;
}

std::string RouteOutput::get_spoony_version() const {
	return _spoony_version;
}

std::vector<std::pair<std::vector<int>::size_type, int>> RouteOutput::get_variables() const {
	return _variables;
}

int RouteOutput::get_maximum_steps() const {
	return _maximum_steps;
}

Milliframes RouteOutput::get_frames() const {
	return _frames;
}

double RouteOutput::get_score() const {
	return _score;
}

std::size_t RouteOutput::get_variable_count() const {
	return _variables.size();
}

std::vector<std::pair<std::size_t, int>> RouteOutput::parse_variable_data(const std::string & variable_data) {
	std::vector<std::pair<std::size_t, int>> variables;

	if (variable_data.length() > 0) {
		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, variable_data, boost::is_any_of(" "), boost::token_compress_on);

		for (const auto & variable : tokens) {
			std::vector<std::string> subtokens;
			boost::algorithm::split(subtokens, variable, boost::is_any_of(":"), boost::token_compress_on);

			variables.push_back(std::make_pair(std::stoi(subtokens[0]), std::stoi(subtokens[1])));
		}
	}

	return variables;
}

static void normalize_route(const std::shared_ptr<Randomizer> & randomizer, Engine & engine) {
	randomizer->reset();

	engine.reset();
	engine.run();

	Milliframes frames = engine.get_frames();

	for (std::size_t i = 0; i < randomizer->data.size(); i++) {
		for (int value = 0; value <= engine.get_maximum_steps(); value++) {
			randomizer->reset();
			randomizer->data[i] = value;

			engine.reset();
			engine.run();

			if (engine.get_frames() == frames) {
				break;
			}
		}
	}
}

bool RouteOutput::write_route(const std::string & filename, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, bool normalize) {
	std::ifstream route_output_file{filename, std::ios_base::in};
	RouteOutput route_output_data{route_output_file};
	route_output_file.close();

	normalize_route(randomizer, engine);

	Milliframes best_frames = 0_mf;

	if (route_output_data.is_valid(engine.get_version())) {
		bool rewrite_if_equal = false;

		if (SPOONY_VERSION != route_output_data.get_spoony_version() || engine.get_maximum_steps() > route_output_data.get_maximum_steps() || engine.get_score() > route_output_data.get_score()) {
			rewrite_if_equal = true;
		}

		if (engine.get_frames() > route_output_data.get_frames()) {
			return false;
		} else if (engine.get_frames() == route_output_data.get_frames() && !rewrite_if_equal) {
			return false;
		}

		best_frames = route_output_data.get_frames();
	}

	std::vector<int> saved_data{randomizer->data};

	std::time_t t = std::time(nullptr);

	std::cout << "\r                                                                                                                                                                      ";
	std::cout << '\r' << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << ": ";
	std::cout << std::left << std::setw(40) << engine.get_title() << std::right << std::setw(4) << engine.get_initial_seed();

	if (best_frames > 0_mf) {
		std::cout << std::setw(11) << Seconds{best_frames}.count() << " -> " << std::left << std::setw(11) << Seconds{engine.get_frames()}.count();
		std::cout << std::setw(8) << Seconds{best_frames - engine.get_frames()}.count();
	} else {
		std::cout << std::setw(11) << std::setw(11) << "N/A" << " -> " << std::left << std::setw(11) << Seconds{engine.get_frames()}.count();
	}

	std::cout << std::endl;

	std::ofstream output{filename, std::ofstream::out};
	output << engine.format_output(base_engine);

	if (!normalize) {
		randomizer->data = saved_data;
	}

	return true;
}
