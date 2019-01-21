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

#include "encounter.hh"
#include "engine.hh"
#include "instruction.hh"
#include "optimizer.hh"
#include "options.hh"
#include "parameters.hh"
#include "route_output.hh"

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

	Glib::OptionGroup option_group{"options", "Options", "Options to control program"};

	option_group.add_entry(Options::create_option_entry("output-directory", 'o', "Directory to output routes to"), options.output_directory);
	option_group.add_entry(Options::create_option_entry("output-result", 'd', "Print the result to the standard output."), options.output_result);
	option_group.add_entry(Options::create_option_entry("route", 'r', "Route to process"), options.route);
	option_group.add_entry(Options::create_option_entry("algorithm", 'a', "Optimization algorithm"), options.algorithm);
	option_group.add_entry(Options::create_option_entry("full-optimization", 'f', "Optimize all variables instead of only variables after input data"), options.full_optimization);
	option_group.add_entry(Options::create_option_entry("load-existing-variables", 'l', "Use the existing variable data in the best route as seed data"), options.load_existing_variables);
	option_group.add_entry(Options::create_option_entry("seed", 's', "Seed to process"), options.seed);
	option_group.add_entry(Options::create_option_entry("maximum-comparisons", 'c', "Maximum number of pairwise comparisons per variable"), options.maximum_comparisons);
	option_group.add_entry(Options::create_option_entry("maximum-steps", 'm', "Maximum number of extra steps per area"), options.maximum_steps);
	option_group.add_entry(Options::create_option_entry("maximum-iterations", 'i', "Maximum number of iterations to attempt"), options.maximum_iterations);
	option_group.add_entry(Options::create_option_entry("pairwise-shift", 'z', "Shift steps instead of testing all possibilities in local pairwise search"), options.pairwise_shift);
	option_group.add_entry(Options::create_option_entry("perturbation-strength", 'p', "Strength of perturbations for ILS"), options.perturbation_strength);
	option_group.add_entry(Options::create_option_entry("perturbation-wobble", 'w', "Initial wobble range added to the perturbation for ILS"), options.perturbation_wobble);
	option_group.add_entry(Options::create_option_entry("tas-mode", 't', "Use options appropriate for TAS routing"), options.tas_mode);
	option_group.add_entry(Options::create_option_entry("variables", 'v', "Explicitly set variables in the form index:value"), options.variables);
	option_group.add_entry(Options::create_option_entry("step-output", 'x', "Output detailed information per step"), options.step_output);

	Glib::OptionContext option_context;
	option_context.set_main_group(option_group);
	option_context.parse(argc, argv);

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

	std::string route_source_filename{"data/routes/" + options.route + ".txt"};
	std::ifstream route_source_file{route_source_filename, std::ios_base::in};

	if (!route_source_file.is_open()) {
		std::cerr << "ERROR: Failed to open " << route_source_filename << '\n';
		return EXIT_FAILURE;
	}

	auto instructions = read_instructions(route_source_file);
	auto randomizer = std::make_shared<Randomizer>();

	Engine base_engine{Parameters{options.tas_mode, options.step_output, options.seed, 0, "none", randomizer}, instructions, encounters};
	base_engine.run();

	auto route_output_directory = options.output_directory + "/" + options.route;
	auto route_output_filename = (boost::format("%s/%s/%03d.txt") % options.output_directory % options.route % options.seed).str();

	std::filesystem::create_directories(std::filesystem::path(route_output_directory));

	/*
	 * Variable Processing
	 */

	std::size_t optimization_index = 0;

	auto variables = RouteOutput::parse_variable_data(options.variables);

	std::ifstream route_output_file{route_output_filename, std::ios_base::in};
	RouteOutput route_output_data{route_output_file};
	route_output_file.close();

	if (options.load_existing_variables) {
		variables = route_output_data.get_variables();
	}

	for (const auto & pair : variables) {
		if (pair.first < randomizer->data.size()) {
			if (!options.full_optimization && pair.first + 1 > optimization_index) {
				optimization_index = pair.first + 1;
			}

			randomizer->data[pair.first] = pair.second;
		}
	}

	/*
	 * Optimization
	 */

	Engine engine{Parameters{options.tas_mode, options.step_output, options.seed, options.maximum_steps, options.algorithm, randomizer}, instructions, encounters};

	randomizer->reset();

 	engine.reset();
 	engine.run();

	Milliframes best_frames = route_output_data.is_valid(base_engine.get_version()) ? route_output_data.get_frames() : base_engine.get_frames();
	double best_score = route_output_data.is_valid(base_engine.get_version()) ? engine.get_score() : base_engine.get_score();

	std::vector<std::string> algorithms;
	boost::algorithm::split(algorithms, options.algorithm, boost::is_any_of("+"));

	for (const auto & algorithm : algorithms) {
		if (algorithm == "bb") {
			optimize_bb(optimization_index, best_frames, best_score, options, randomizer, engine, base_engine, route_output_filename);
		} else if (algorithm == "ils") {
			optimize_ils(optimization_index, best_frames, best_score, options, randomizer, engine, base_engine, route_output_filename);
		} else if (algorithm == "local") {
			optimize_local(optimization_index, best_frames, best_score, options, randomizer, engine, base_engine, route_output_filename, true);
		} else if (algorithm == "pair") {
			optimize_local_pair(optimization_index, best_frames, best_score, options, randomizer, engine, base_engine, route_output_filename, true);
		} else if (algorithm == "sequential") {
			optimize_sequential(optimization_index, best_frames, best_score, options, randomizer, engine, base_engine, route_output_filename);
		} else if (algorithm == "none") {
		} else {
			std::cerr << "Algorithm \"" << algorithm << "\" is unknown" << std::endl;
		}
	}

	/*
	 * Output
	 */

	randomizer->reset();

	engine.reset();
	engine.run();

	if (options.output_result) {
		std::cout << engine.format_output(base_engine);
	}

	RouteOutput::write_route(route_output_filename, randomizer, engine, base_engine, true);

	return 0;
}
