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

#include <giomm/datainputstream.h>
#include <glibmm/regex.h>

#include "encounter.hh"

Encounter::Encounter(unsigned int id, const Glib::ustring & description, double average_duration, double minimum_duration) :
	_id{id},
	_description{description},
	_average_duration{average_duration},
	_minimum_duration{minimum_duration}
{ }

unsigned int Encounter::get_id() const
{
	return _id;
}

Glib::ustring Encounter::get_description() const
{
	return _description;
}

double Encounter::get_average_duration() const
{
	return _average_duration;
}

double Encounter::get_minimum_duration() const
{
	return _minimum_duration;
}

Encounters::Encounters(const Glib::RefPtr<Gio::File> & file) :
	_encounters{512},
	_encounter_groups{512}
{
	auto file_stream = Gio::DataInputStream::create(file->read());
	std::string line;

	auto split_regex = Glib::Regex::create("\t+");

	while (file_stream->read_line(line))
	{
		std::vector<Glib::ustring> tokens = split_regex->split(line);

		if (!tokens.empty())
		{
			if (tokens[0] == "ENCOUNT" && tokens.size() == 5)
			{
				unsigned int id = std::stoul(tokens[1]);

				_encounters[id] = std::make_shared<const Encounter>(id, tokens[2], std::stod(tokens[3]), std::stod(tokens[4]));
			}
			else if (tokens[0] == "GROUP" && tokens.size() == 10)
			{
				unsigned int id = std::stoul(tokens[1]);

				for (int i = 0; i < 8; i++)
				{
					_encounter_groups[id].push_back(std::stoul(tokens[i + 2]));
				}
			}
			else
			{
				std::cerr << "Unrecognized line: " << line << std::endl;
			}
		}
	}
}

std::shared_ptr<const Encounter> Encounters::get_encounter(int id)
{
	return _encounters[id];
}

std::shared_ptr<const Encounter> Encounters::get_encounter_from_group(int group_index, int encounter_index)
{
	return _encounters[_encounter_groups[group_index][encounter_index]];
}
