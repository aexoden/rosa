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

#include <ctime>
#include <iomanip>
#include <iostream>

#include <giomm/datainputstream.h>
#include <giomm/init.h>
#include <glibmm/init.h>
#include <glibmm/miscutils.h>
#include <glibmm/optioncontext.h>
#include <glibmm/optionentry.h>
#include <glibmm/optiongroup.h>
#include <glibmm/regex.h>

#include "encounter.hh"
#include "engine.hh"
#include "instruction.hh"

/*
 * Option Handling Functions
 */

class Options
{
	public:
		Glib::ustring output_directory = "output/routes";

		bool output_result = false;

		Glib::ustring route = "paladin";

		Glib::ustring algorithm = "pair";

		Glib::ustring variables = "";

		int seed = 43;
		int maximum_steps = 0;
};

Glib::OptionEntry create_option_entry(const Glib::ustring & long_name, const gchar & short_name, const Glib::ustring & description)
{
	Glib::OptionEntry entry;

	entry.set_long_name(long_name);
	entry.set_short_name(short_name);
	entry.set_description(description);

	return entry;
}

/*
 * Utility Functions
 */

double get_best_frames(const Glib::RefPtr<Gio::File> & file, const Engine & base_engine)
{
	if (file->query_exists())
	{
		auto file_stream = Gio::DataInputStream::create(file->read());
		std::string line;

		auto split_regex = Glib::Regex::create("\t+");

		int version = -1;
		double frames = std::numeric_limits<double>::max();

		while (file_stream->read_line(line))
		{
			std::vector<Glib::ustring> tokens = split_regex->split(line);

			if (!tokens.empty())
			{
				if (tokens[0] == "VERSION" && tokens.size() == 2)
				{
					version = std::stoi(tokens[1]);
				}
				else if (tokens[0] == "FRAMES" && tokens.size() == 2)
				{
					frames = std::stod(tokens[1]);
				}
			}
		}

		if (version >= base_engine.get_version())
		{
			return frames;
		}
	}

	return base_engine.get_frames();
}

void write_best_route(const Glib::RefPtr<Gio::File> & file, double best_frames, const Engine & engine, const Engine & base_engine)
{
	std::cout << "\r                                                                                                                                                ";
	std::cout << "\r" << Glib::DateTime::create_now_local().format("%Y-%m-%d %H:%M:%S") << ": ";
	std::cout << std::left << std::setw(40) << base_engine.get_title() << std::right << std::setw(4) << engine.get_initial_seed();
	std::cout << std::setw(11) << Engine::frames_to_seconds(best_frames) << " -> " << std::left << std::setw(11) << Engine::frames_to_seconds(engine.get_frames());
	std::cout << std::setw(8) << Engine::frames_to_seconds(best_frames - engine.get_frames()) << std::endl;

	auto output_stream = file->replace();

	output_stream->write(engine.format_output(base_engine));
}

/*
 * Optimization Functions
 */

void optimize_pair(int start_index, double best_frames, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const Glib::RefPtr<Gio::File> & output_file)
{
	double search_best_frames = base_engine.get_frames();
	double round_best_frames = base_engine.get_frames();

	int min_variables = randomizer->get_index();
	int max_variables = randomizer->get_index();

	while (true)
	{
		int best_i = -1;
		int best_j = -1;

		int best_i_value = 0;
		int best_j_value = 0;

		for (decltype(randomizer->data)::size_type i = start_index; i < randomizer->data.size(); i++)
		{
			int original_i_value = randomizer->data[i];

			for (decltype(randomizer->data)::size_type j = i + 1; j < randomizer->data.size(); j++)
			{
				int original_j_value = randomizer->data[j];

				std::cout << "\rAlgorithm: " << std::left << std::setw(15) << "Pairwise";
				std::cout << "   Current Indexes: (" << std::right << std::setw(3) << i << ", " << std::setw(3) << j << ")";
				std::cout << "   Variables: (" << std::setw(2) << min_variables << ", " << max_variables << ")";
				std::cout << "   Best: " << std::setw(10) << Engine::frames_to_seconds(best_frames);
				std::cout << "   Previous: " << std::setw(10) << Engine::frames_to_seconds(round_best_frames);
				std::cout << "   Current: " << std::setw(10) << Engine::frames_to_seconds(search_best_frames);
				std::cout << std::flush;

				for (int i_value = 0; i_value <= options.maximum_steps; i_value++)
				{
					for (int j_value = 0; j_value <= options.maximum_steps; j_value++)
					{
						randomizer->reset();
						randomizer->data[i] = i_value;
						randomizer->data[j] = j_value;

						engine.reset();
						engine.run();

						if (engine.get_frames() < best_frames)
						{
							write_best_route(output_file, best_frames, engine, base_engine);
							best_frames = engine.get_frames();
						}

						if (engine.get_frames() < search_best_frames)
						{
							search_best_frames = engine.get_frames();
							best_i = i;
							best_j = j;
							best_i_value = i_value;
							best_j_value = j_value;
						}

						int variables = randomizer->get_index();

						min_variables = std::min(min_variables, variables);
						max_variables = std::max(max_variables, variables);
					}
				}

				randomizer->data[j] = original_j_value;
			}

			randomizer->data[i] = original_i_value;
		}

		if (best_i < 0)
		{
			std::cout << std::endl;
			break;
		}

		std::cout << std::endl << "Updating (" << best_i << ", " << best_j << ") to (" << best_i_value << ", " << best_j_value << ") (" << (best_frames / 60.0988) << "s)" << std::endl;

		randomizer->data[best_i] = best_i_value;
		randomizer->data[best_j] = best_j_value;

		round_best_frames = search_best_frames;
	}
}

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

	option_group.add_entry(create_option_entry("output-directory", 'o', "Directory to output routes to"), options.output_directory);
	option_group.add_entry(create_option_entry("output-result", 'd', "Print the result to the standard output."), options.output_result);
	option_group.add_entry(create_option_entry("route", 'r', "Route to process"), options.route);
	option_group.add_entry(create_option_entry("algorithm", 'a', "Optimization algorithm"), options.algorithm);
	option_group.add_entry(create_option_entry("seed", 's', "Seed to process"), options.seed);
	option_group.add_entry(create_option_entry("maximum-steps", 'm', "Maximum number of extra steps per area"), options.maximum_steps);
	option_group.add_entry(create_option_entry("variables", 'v', "Explicitly set variables in the form index:value"), options.variables);

	Glib::OptionContext option_context;
	option_context.set_main_group(option_group);
	option_context.parse(argc, argv);

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

	Engine base_engine{Parameters{options.seed, 0, randomizer}, instructions, encounters};
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

	for (const auto & variable : Glib::Regex::split_simple(" ", options.variables))
	{
		std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(":", variable);

		decltype(randomizer->data)::size_type index = std::stoi(tokens[0]);
		int value = std::stoi(tokens[1]);

		if (index < randomizer->data.size())
		{
			if (index + 1 > optimization_index)
			{
				optimization_index = index + 1;
			}

			randomizer->data[index] = value;
		}
	}

	/*
	 * Optimization
	 */

	double best_frames = get_best_frames(route_output_file, base_engine);

	Engine engine{Parameters{options.seed, options.maximum_steps, randomizer}, instructions, encounters};

	if (options.algorithm == "pair")
	{
		optimize_pair(optimization_index, best_frames, options, randomizer, engine, base_engine, route_output_file);
	}
	else if (options.algorithm == "none")
	{

	}
	else
	{
		std::cerr << "Algorithm \"" << options.algorithm << "\" is unknown" << std::endl;
		exit(EXIT_FAILURE);
	}

	/*
	 * Output
	 */

	randomizer->reset();

	engine.reset();
	engine.run();

	if (!route_output_file->query_exists())
	{
		write_best_route(route_output_file, best_frames, engine, base_engine);
	}

	if (options.output_result)
	{
		if (options.algorithm != "none")
		{
			std::cout << std::endl;
		}

		std::cout << engine.format_output(base_engine) << std::endl;
	}

	return 0;
}
