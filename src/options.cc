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

#include <sstream>

#include <tclap/CmdLine.h>

#include "options.hh"
#include "version.hh"

template <typename T>
NumberConstraint<T>::NumberConstraint(T min, T max) :
	_min(min),
	_max(max)
{}

template <typename T>
std::string NumberConstraint<T>::description() const
{
	std::ostringstream string_stream;
	string_stream << _min << ".." << _max;
	return string_stream.str();
}

template <typename T>
std::string NumberConstraint<T>::shortID() const
{
	return description();
}

template <typename T>
bool NumberConstraint<T>::check(const T & value) const
{
	return (value >= _min) && (value <= _max);
}

Options::Options() {}

void Options::parse(int argc, char ** argv)
{
	TCLAP::CmdLine command_line{"Generates step routes for Final Fantasy IV", ' ', SPOONY_VERSION};

	TCLAP::ValueArg<decltype(_seed)> option_seed{"s", "seed", "Encounter seed", true, 0, &_seed_constraint, command_line};
	TCLAP::ValueArg<decltype(_route)> option_route{"r", "route", "Route definition file", true, "", "filename", command_line};

	command_line.parse(argc, argv);

	_seed = option_seed.getValue();
}
