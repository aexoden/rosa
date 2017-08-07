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
#include <numeric>

#include <giomm/datainputstream.h>

#include "engine.hh"
#include "randomizer.hh"
#include "route_output.hh"
#include "version.hh"

RouteOutput::RouteOutput(const Glib::RefPtr<Gio::File> & file)
{
	if (file->query_exists())
	{
		auto file_stream = Gio::DataInputStream::create(file->read());
		std::string line;

		auto split_regex = Glib::Regex::create("\t+");

		while (file_stream->read_line(line))
		{
			std::vector<Glib::ustring> tokens = split_regex->split(line);

			if (!tokens.empty())
			{
				if (tokens[0] == "FRAMES" && tokens.size() == 2)
				{
					_frames = std::stod(tokens[1]);
				}
				else if (tokens[0] == "MAXSTEP" && tokens.size() == 2)
				{
					_maximum_steps = std::stoi(tokens[1]);
				}
				else if (tokens[0] == "SPOONY" && tokens.size() == 2)
				{
					_spoony_version = tokens[1];
				}
				else if (tokens[0] == "VARS" && tokens.size() == 2)
				{
					_variables = parse_variable_data(tokens[1]);
				}
				else if (tokens[0] == "VERSION" && tokens.size() == 2)
				{
					_version = std::stoi(tokens[1]);
				}
			}
		}

		_valid = true;
	}
}

bool RouteOutput::is_valid(int current_version) const
{
	return _valid && current_version <= _version;
}

Glib::ustring RouteOutput::get_spoony_version() const
{
	return _spoony_version;
}

std::vector<std::pair<std::vector<int>::size_type, int>> RouteOutput::get_variables() const
{
	return _variables;
}

int RouteOutput::get_maximum_steps() const
{
	return _maximum_steps;
}

double RouteOutput::get_frames() const
{
	return _frames;
}

int RouteOutput::get_variable_count() const
{
	return _variables.size();
}

int RouteOutput::get_score() const
{
	auto lambda = [&](int & a, decltype(*(_variables.begin())) b) { return a + b.first * b.second; };
	return std::accumulate(_variables.begin(), _variables.end(), 0, lambda);
}

std::vector<std::pair<std::vector<int>::size_type, int>> RouteOutput::parse_variable_data(const Glib::ustring & variable_data)
{
	std::vector<std::pair<std::vector<int>::size_type, int>> variables;

	for (const auto & variable : Glib::Regex::split_simple(" ", variable_data))
	{
		std::vector<Glib::ustring> tokens = Glib::Regex::split_simple(":", variable);

		variables.push_back(std::make_pair(std::stoi(tokens[0]), std::stoi(tokens[1])));
	}

	return variables;
}

static void normalize_route(const std::shared_ptr<Randomizer> & randomizer, Engine & engine)
{
	randomizer->reset();

	engine.reset();
	engine.run();

	double frames = engine.get_frames();

	for (decltype(randomizer->data)::size_type i = 0; i < randomizer->data.size(); i++)
	{
		for (int value = 0; value <= engine.get_maximum_steps(); value++)
		{
			randomizer->reset();
			randomizer->data[i] = value;

			engine.reset();
			engine.run();

			if (engine.get_frames() == frames)
			{
				break;
			}
		}
	}
}

bool RouteOutput::write_route(const Glib::RefPtr<Gio::File> & file, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, bool normalize)
{
	RouteOutput route_output_data{file};

	normalize_route(randomizer, engine);

	double best_frames = 0.0;

	if (route_output_data.is_valid(engine.get_version()))
	{
		bool rewrite_if_equal = false;

		if (SPOONY_VERSION != route_output_data.get_spoony_version() || engine.get_maximum_steps() > route_output_data.get_maximum_steps() || engine.get_score() > route_output_data.get_score())
		{
			rewrite_if_equal = true;
		}

		if (engine.get_frames() > route_output_data.get_frames())
		{
			return false;
		}
		else if (engine.get_frames() == route_output_data.get_frames() && !rewrite_if_equal)
		{
			return false;
		}

		best_frames = route_output_data.get_frames();
	}

	std::vector<int> saved_data{randomizer->data};

	std::cout << "\r                                                                                                                                                                      ";
	std::cout << "\r" << Glib::DateTime::create_now_local().format("%Y-%m-%d %H:%M:%S") << ": ";
	std::cout << std::left << std::setw(40) << engine.get_title() << std::right << std::setw(4) << engine.get_initial_seed();

	if (best_frames > 0)
	{
		std::cout << std::setw(11) << Engine::frames_to_seconds(best_frames) << " -> " << std::left << std::setw(11) << Engine::frames_to_seconds(engine.get_frames());
		std::cout << std::setw(8) << Engine::frames_to_seconds(best_frames - engine.get_frames());
	}
	else
	{
		std::cout << std::setw(11) << std::setw(11) << "N/A" << " -> " << std::left << std::setw(11) << Engine::frames_to_seconds(engine.get_frames());
	}

	std::cout << std::endl;

	auto output_stream = file->replace();
	output_stream->write(engine.format_output(base_engine));

	if (!normalize)
	{
		randomizer->data = saved_data;
	}

	return true;
}
