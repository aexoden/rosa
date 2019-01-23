/*
 * Known TODO:
 *   - Single extra steps at save points are slightly more costly.
 *   - You can take three extra steps in Cecil's room with only a two tile penalty.
 *
 * Code Improvements:
 *   - Calculate encounter counts without forcing the grind fight and see if some seeds could benefit from a save/reset cycle. Given the uncertainty they'd have to be pretty bad.
 *   - Output variable counts when outputting during writes.
 *
 * Route Improvements:
 *   - Variable optimization (combine Ordeals into one variable, improve Toroia, remove dependent variables with choices)
 *   - Do something about indistinguishable seeds (41/42)
 *   - Figure out Damcyan reset seeds.
 */

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
#include "optimizer.hh"
#include "options.hh"
#include "parameters.hh"
#include "route_output.hh"
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

	auto instructions = read_instructions(route_source_file);
	auto variables = std::make_shared<Variables>();

	Engine base_engine{Parameters{options.tas_mode, options.step_output, options.seed, 0, "none", variables}, instructions, encounters, maps};
	base_engine.run();

	auto route_output_directory = options.output_directory + "/" + options.route;
	auto route_output_filename = (boost::format("%s/%s/%03d.txt") % options.output_directory % options.route % options.seed).str();

	std::filesystem::create_directories(std::filesystem::path(route_output_directory));

	/*
	 * Variable Processing
	 */

	/*
	std::size_t optimization_index = 0;


	auto loaded_variables = RouteOutput::parse_variable_data(options.variables);

	std::ifstream route_output_file{route_output_filename, std::ios_base::in};
	RouteOutput route_output_data{route_output_file};
	route_output_file.close();

	if (options.load_existing_variables) {
		loaded_variables = route_output_data.get_variables();
	}

	for (const auto & pair : variables) {
		if (pair.first < randomizer->data.size()) {
			if (!options.full_optimization && pair.first + 1 > optimization_index) {
				optimization_index = pair.first + 1;
			}

			randomizer->data[pair.first] = pair.second;
		}
	}
	*/

	/*
	 * Optimization
	 */

	Engine engine{Parameters{options.tas_mode, options.step_output, options.seed, options.maximum_steps, options.algorithm, variables}, instructions, encounters, maps};

 	engine.reset();
 	engine.run();

	/*
	Milliframes best_frames = base_engine.get_frames();
	double best_score = base_engine.get_score();

	std::vector<std::string> algorithms;
	boost::algorithm::split(algorithms, options.algorithm, boost::is_any_of("+"));


	for (const auto & algorithm : algorithms) {
		if (algorithm == "ils") {
			optimize_ils(optimization_index, &best_frames, &best_score, options, randomizer, &engine, base_engine, route_output_filename);
		} else if (algorithm == "local") {
			optimize_local(optimization_index, &best_frames, &best_score, options, randomizer, &engine, base_engine, route_output_filename, true);
		} else if (algorithm == "pair") {
			optimize_local_pair(optimization_index, &best_frames, &best_score, options, randomizer, &engine, base_engine, route_output_filename, true);
		} else if (algorithm == "sequential") {
			optimize_sequential(optimization_index, &best_frames, &best_score, options, randomizer, &engine, base_engine, route_output_filename);
		} else if (algorithm == "none") {
		} else {
			std::cerr << "Algorithm \"" << algorithm << "\" is unknown" << std::endl;
		}
	}
	*/

	/*
	 * Output
	 */

	engine.reset();
	engine.run();

	std::cout << engine.format_output(base_engine);

	//RouteOutput::write_route(route_output_filename, variables, &engine, base_engine, true);

	return 0;
}
