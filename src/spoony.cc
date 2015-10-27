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
 */

#include <iomanip>
#include <iostream>

#include <giomm/file.h>
#include <giomm/init.h>
#include <glibmm/init.h>
#include <glibmm/miscutils.h>
#include <glibmm/regex.h>
#include <glibmm/ustring.h>

#include "encounter.hh"
#include "engine.hh"
#include "instruction.hh"
#include "optimizer.hh"
#include "options.hh"
#include "route_output.hh"

/*
 * Main Function
 */

int main (int argc, char ** argv)
{
	/*
	 * Initialization
	 */

	setlocale(LC_ALL, "");

	Glib::init();
	Gio::init();

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
	option_group.add_entry(Options::create_option_entry("maximum-steps", 'm', "Maximum number of extra steps per area"), options.maximum_steps);
	option_group.add_entry(Options::create_option_entry("maximum-iterations", 'i', "Maximum number of iterations to attempt"), options.maximum_iterations);
	option_group.add_entry(Options::create_option_entry("perturbation-strength", 'p', "Strength of perturbations for ILS"), options.perturbation_strength);
	option_group.add_entry(Options::create_option_entry("perturbation-wobble", 'w', "Initial wobble range added to the perturbation for ILS"), options.perturbation_wobble);
	option_group.add_entry(Options::create_option_entry("tas-mode", 't', "Use options appropriate for TAS routing"), options.tas_mode);
	option_group.add_entry(Options::create_option_entry("variables", 'v', "Explicitly set variables in the form index:value"), options.variables);

	Glib::OptionContext option_context;
	option_context.set_main_group(option_group);
	option_context.parse(argc, argv);

	if (options.load_existing_variables)
	{
		options.full_optimization = true;
	}

	/*
	 * Base Data
	 */

	Encounters encounters{Gio::File::create_for_path("data/encounters/ff2us.txt")};

	auto route_source_file = Gio::File::create_for_path(Glib::build_filename("data", "routes", Glib::ustring::compose("%1.txt", options.route)));

	if (!route_source_file->query_exists())
	{
		std::cerr << "Route \"" << options.route << "\" does not exist." << std::endl;
		exit(EXIT_FAILURE);
	}

	auto instructions = read_instructions(route_source_file);
	auto randomizer = std::make_shared<Randomizer>(false);

	Engine base_engine{Parameters{options.tas_mode, options.seed, 0, "none", randomizer}, instructions, encounters};
	base_engine.run();

	auto route_output_directory = Gio::File::create_for_path(Glib::build_filename(options.output_directory, options.route));
	auto route_output_file = Gio::File::create_for_path(Glib::build_filename(options.output_directory, options.route, Glib::ustring::format(std::setfill(L'0'), std::setw(3), options.seed, ".txt")));

	if (!route_output_directory->query_exists())
	{
		route_output_directory->make_directory_with_parents();
	}

	/*
	 * Variable Processing
	 */

	decltype(randomizer->data)::size_type optimization_index = 0;

	auto variables = RouteOutput::parse_variable_data(options.variables);

	RouteOutput route_output_data{route_output_file};

	if (options.load_existing_variables)
	{
		variables = route_output_data.get_variables();
	}

	for (const auto & pair : variables)
	{
		if (pair.first < randomizer->data.size())
		{
			if (!options.full_optimization && pair.first + 1 > optimization_index)
			{
				optimization_index = pair.first + 1;
			}

			randomizer->data[pair.first] = pair.second;
		}
	}

	/*
	 * Optimization
	 */

	double best_frames = route_output_data.is_valid(base_engine.get_version()) ? route_output_data.get_frames() : base_engine.get_frames();
	int best_variable_count = route_output_data.is_valid(base_engine.get_version()) ? route_output_data.get_variable_count() : 0;

	Engine engine{Parameters{options.tas_mode, options.seed, options.maximum_steps, options.algorithm, randomizer}, instructions, encounters};

	for (const auto & algorithm : Glib::Regex::split_simple("\\+", options.algorithm))
	{
		if (algorithm == "bb")
		{
			optimize_bb(optimization_index, best_frames, best_variable_count, options, randomizer, engine, base_engine, route_output_file);
		}
		else if (algorithm == "ils")
		{
			optimize_ils(optimization_index, best_frames, best_variable_count, options, randomizer, engine, base_engine, route_output_file);
		}
		else if (algorithm == "local")
		{
			optimize_local(optimization_index, best_frames, best_variable_count, options, randomizer, engine, base_engine, route_output_file, true);
		}
		else if (algorithm == "pair")
		{
			optimize_local_pair(optimization_index, best_frames, best_variable_count, options, randomizer, engine, base_engine, route_output_file);
		}
		else if (algorithm == "sequential")
		{
			optimize_sequential(optimization_index, best_frames, best_variable_count, options, randomizer, engine, base_engine, route_output_file);
		}
		else if (algorithm == "none")
		{

		}
		else
		{
			std::cerr << "Algorithm \"" << algorithm << "\" is unknown" << std::endl;
		}
	}

	/*
	 * Output
	 */

	randomizer->reset();

	engine.reset();
	engine.run();

	RouteOutput::write_route(route_output_file, randomizer, engine, base_engine, true);

	if (options.output_result)
	{
		std::cout << engine.format_output(base_engine);
	}

	return 0;
}
