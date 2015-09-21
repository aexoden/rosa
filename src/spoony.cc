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

#include <iostream>

#include <giomm/init.h>
#include <glibmm/keyfile.h>
#include <glibmm/init.h>
#include <glibmm/miscutils.h>
#include <glibmm/optioncontext.h>
#include <glibmm/optionentry.h>
#include <glibmm/optiongroup.h>

#include "encounter.hh"
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

	Glib::ustring route;

	int maximum_steps;

	Glib::OptionGroup option_group{"options", "Options", "Options to configure program"};
	Glib::OptionEntry route_entry = create_option_entry("route", 'r', "Route to process");
	option_group.add_entry(route_entry, route);

	Glib::OptionEntry maximum_steps_entry = create_option_entry("maximum-steps", 's', "Maximum number of extra steps per area");
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

	return 0;
}
