#include <cstdlib>
#include <filesystem>
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

	CLI::App app{"Spoony " SPOONY_VERSION};

	app.add_option("-a,--algorithm", options.algorithm, "Optimization algorithm");
	app.add_option("-r,--route", options.route, "Route to process");
	app.add_option("-o,--output-directory", options.output_directory, "Directory to output routes to");
	app.add_option("-s,--seed", options.seed, "Seed to process");
	app.add_option("-c,--maximum-comparisons", options.maximum_comparisons, "Maximum number of pairwise comparisons per variable");
	app.add_option("-m,--maximum-steps", options.maximum_steps, "Maximum number of extra steps per area");
	app.add_option("-i,--maximum-iterations", options.maximum_iterations, "Maximum number of iterations to attempt");
	app.add_option("-p,--perturbation-strength", options.perturbation_strength, "Strength of perturbations for ILS");
	app.add_option("-w,--perturbation-wobble", options.perturbation_wobble, "initial wobble range added to the perturbation for ILS");
	app.add_option("-v,--variables", options.variables, "Explicitly set variables in the form index:value");

	app.add_flag("-d,--output-result", options.output_result, "Print the result to the standard output");
	app.add_flag("-f,--full-optimization", options.full_optimization, "Optimize all variables instead of only variables after input data");
	app.add_flag("-l,--load-existing-variables", options.load_existing_variables, "Use the existing variable data in the best route as seed data");
	app.add_flag("-z,--pairwise-shift", options.pairwise_shift, "Shift steps instead of testing all possibilities in local pairwise search");
	app.add_flag("-t,--tas-mode", options.tas_mode, "Use options appropriate for TAS Routing");
	app.add_flag("-x,--step-output", options.step_output, "Output detailed information per step");

	try {
		app.parse(argc, argv);
	} catch (const CLI::ParseError & e) {
		return app.exit(e);
	}

	if (options.load_existing_variables) {
		options.full_optimization = true;
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

	std::cout << engine.optimize(options.seed);

	return EXIT_SUCCESS;
}
