#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>

#include "CLI11.hpp"

#include "encounter.hh"
#include "engine.hh"
#include "instruction.hh"
#include "map.hh"
#include "options.hh"
#include "parameters.hh"
#include "version.hh"

/*
 * Main Function
 */

int main (int argc, char ** argv) {
	/*
	 * Initialization
	 */

	std::cout << std::fixed << std::setprecision(3);

	Options options;

	/*
	 * Option Parsing
	 */

	CLI::App app{"Spoony " ROSA_VERSION};

	app.add_option("-r,--route", options.route, "Route to process");
	app.add_option("-s,--seed", options.seed, "Seed to process");
	app.add_option("-m,--maximum-steps", options.maximum_steps, "Maximum number of extra steps per segment");
	app.add_option("-v,--variables", options.variables, "Explicitly set variable constraints in the form variable:value[-max_value]");

	app.add_flag("-t,--tas-mode", options.tas_mode, "Use options appropriate for TAS Routing");

	try {
		app.parse(argc, argv);
	} catch (const CLI::ParseError & e) {
		return app.exit(e);
	}

	/*
	 * Base Data
	 */

	std::string encounters_filename{"data/encounters/ff2us.txt"};
	std::ifstream encounters_file{encounters_filename, std::ios_base::in};

	if (!encounters_file.is_open()) {
		std::cerr << "ERROR: Failed to open " << encounters_filename << '\n';
		return EXIT_FAILURE;
	}

	Encounters encounters{encounters_file};

	std::string maps_filename{"data/maps/ff2us.txt"};
	std::ifstream maps_file{maps_filename, std::ios_base::in};

	if (!maps_file.is_open()) {
		std::cerr << "ERROR: Failed to open " << maps_filename << '\n';
		return EXIT_FAILURE;
	}

	Maps maps{maps_file};

	std::string route_source_filename{"data/routes/" + options.route + ".txt"};
	std::ifstream route_source_file{route_source_filename, std::ios_base::in};

	if (!route_source_file.is_open()) {
		std::cerr << "ERROR: Failed to open " << route_source_filename << '\n';
		return EXIT_FAILURE;
	}

	auto route{read_route(route_source_file)};

	/*
	 * Optimization
	 */

	Engine engine{Parameters{route, encounters, maps, options.maximum_steps, options.tas_mode}};

	if (!options.variables.empty()) {
		std::vector<std::string> variables;
		boost::algorithm::split(variables, options.variables, boost::is_any_of(" "), boost::token_compress_on);

		try {
			for (const auto & variable : variables) {
				std::vector<std::string> tokens;
				boost::algorithm::split(tokens, variable, boost::is_any_of(":"));

				std::vector<std::string> values;
				boost::algorithm::split(values, tokens[1], boost::is_any_of("-"));

				auto index{std::stoi(tokens[0], nullptr, 16)};
				auto minimum{std::stoi(values[0])};
				auto maximum{minimum};

				if (values.size() > 1) {
					maximum = std::stoi(values[1]);
				}

				engine.set_variable_minimum(index, minimum);
				engine.set_variable_maximum(index, maximum);
			}
		} catch (...) {
			std::cerr << "WARNING: Invalid variable data supplied\n";
		}
	}

	std::cout << engine.optimize(options.seed);

	return EXIT_SUCCESS;
}
