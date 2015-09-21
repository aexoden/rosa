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

#include <iomanip>
#include <iostream>

#include <giomm/init.h>
#include <glibmm/keyfile.h>
#include <glibmm/init.h>
#include <glibmm/miscutils.h>
#include <glibmm/optioncontext.h>
#include <glibmm/optionentry.h>
#include <glibmm/optiongroup.h>

#include "encounter.hh"
#include "engine.hh"
#include "instruction.hh"

Glib::OptionEntry create_option_entry(const Glib::ustring & long_name, const gchar & short_name, const Glib::ustring & description)
{
	Glib::OptionEntry entry;

	entry.set_long_name(long_name);
	entry.set_short_name(short_name);
	entry.set_description(description);

	return entry;
}

int main (int argc, char ** argv)
{
	setlocale(LC_ALL, "");

	Glib::init();
	Gio::init();

	std::cout << std::fixed << std::setprecision(3);

	Glib::ustring route;
	int seed;

	int maximum_steps = 0;

	Glib::OptionGroup option_group{"options", "Options", "Options to configure program"};
	Glib::OptionEntry route_entry = create_option_entry("route", 'r', "Route to process");
	option_group.add_entry(route_entry, route);

	Glib::OptionEntry seed_entry = create_option_entry("seed", 's', "Seed to process");
	option_group.add_entry(seed_entry, seed);

	Glib::OptionEntry maximum_steps_entry = create_option_entry("maximum-steps", 'm', "Maximum number of extra steps per area");
	option_group.add_entry(maximum_steps_entry, maximum_steps);

	Glib::OptionContext option_context;
	option_context.set_main_group(option_group);
	option_context.parse(argc, argv);

	Encounters encounters{Gio::File::create_for_path("data/encounters/ff2us.txt")};

	auto route_file = Gio::File::create_for_path(Glib::build_filename("data", "routes", Glib::ustring::compose("%1.txt", route)));

	if (!route_file->query_exists())
	{
		std::cerr << "Route " << route << " does not exist." << std::endl;
		exit(EXIT_FAILURE);
	}

	auto instructions = read_instructions(route_file);
	auto randomizer = std::make_shared<Randomizer>(false);

	Engine base_engine{Parameters{seed, 0, randomizer}, instructions, encounters};
	base_engine.run();

	double true_best_frames = base_engine.get_frames();
	double round_best_frames = base_engine.get_frames();
	double best_frames = base_engine.get_frames();

	int min_variables = randomizer->get_index();
	int max_variables = randomizer->get_index();

	while (true)
	{
		int best_i = -1;
		int best_j = -1;
		int best_i_value = 0;
		int best_j_value = 0;

		for (decltype(randomizer->data)::size_type i = 0; i < randomizer->data.size(); i++)
		{
			int original_i_value = randomizer->data[i];

			for (decltype(randomizer->data)::size_type j = i + 1; j < randomizer->data.size(); j++)
			{
				int original_j_value = randomizer->data[j];

				for (int i_value = 0; i_value <= maximum_steps; i_value++)
				{
					for (int j_value = 0; j_value <= maximum_steps; j_value++)
					{
						randomizer->reset();
						randomizer->data[i] = i_value;
						randomizer->data[j] = j_value;

						std::cout << "\r(" << std::right << std::setw(2) << i << "," << std::setw(2) << j << ") -> (" << std::setw(3) << i_value << "," << std::setw(3) << j_value << ")";
						std::cout << "   Variables: (" << std::setw(2) << min_variables << "," << max_variables << ")";
						std::cout << "   Best: " << std::setw(10) << Engine::frames_to_seconds(true_best_frames);
						std::cout << "   Previous: " << std::setw(10) << Engine::frames_to_seconds(round_best_frames);
						std::cout << "   Current: " << std::setw(10) << Engine::frames_to_seconds(best_frames);

						Engine engine{Parameters{seed, maximum_steps, randomizer}, instructions, encounters};
						engine.run();

						if (engine.get_frames() < true_best_frames)
						{
							std::cout << "\r                                                                                                            ";
							std::cout << "\rTIME: " << std::left << std::setw(40) << base_engine.get_title() << std::right << std::setw(4) << seed;
							std::cout << std::setw(11) << Engine::frames_to_seconds(true_best_frames) << " -> " << std::left << std::setw(11) << Engine::frames_to_seconds(engine.get_frames());
							std::cout << std::setw(8) << Engine::frames_to_seconds(true_best_frames - engine.get_frames()) << std::endl;
							true_best_frames = engine.get_frames();
						}

						if (engine.get_frames() < best_frames)
						{
							best_frames = engine.get_frames();
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

		round_best_frames = best_frames;
	}

	randomizer->reset();
	Engine engine{Parameters{seed, maximum_steps, randomizer}, instructions, encounters};
	engine.run();

	std::cout << engine.format_output(base_engine);

	return 0;
}
