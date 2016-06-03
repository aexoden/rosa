/*
 * Copyright (c) 2016 Jason Lynch <jason@calindora.com>
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

#include <fstream>
#include <iostream>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "map.hh"

Map::Map(unsigned int id, unsigned int encounter_rate, unsigned int encounter_group, const std::string & title, const std::string & description) :
	_id(id),
	_encounter_rate(encounter_rate),
	_encounter_group(encounter_group),
	_title(title),
	_description(description)
{}

unsigned int Map::id() const
{
	return _id;
}

unsigned int Map::encounter_rate() const
{
	return _encounter_rate;
}

unsigned int Map::encounter_group() const
{
	return _encounter_group;
}

std::string Map::title() const
{
	return _title;
}

std::string Map::description() const
{
	return _description;
}

std::map<unsigned int, std::shared_ptr<const Map>> Map::read_maps(const std::string & filename)
{
	std::map<unsigned int, std::shared_ptr<const Map>> maps;

	std::ifstream file{filename};
	std::string line;

	if (file.is_open())
	{
		while (std::getline(file, line))
		{
			if (!line.empty() && line[0] != '#')
			{
				std::vector<std::string> tokens;
				boost::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

				if (tokens[0] == "MAP")
				{
					unsigned int id = static_cast<unsigned int>(std::stoul(tokens[1]));
					unsigned int encounter_rate = static_cast<unsigned int>(std::stoul(tokens[2]));
					unsigned int encounter_group = static_cast<unsigned int>(std::stoul(tokens[3]));

					std::string title = tokens[4];

					if (title == "-")
					{
						title = "";
					}

					maps[id] = std::make_shared<const Map>(id, encounter_rate, encounter_group, title, tokens[5]);
				}
				else
				{
					std::cerr << "WARNING: Unrecognized line in map definition: " << line << std::endl;
				}
			}
		}
	}

	return maps;
}
