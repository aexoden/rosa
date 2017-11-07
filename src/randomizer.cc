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

#include "algorithm"

#include "randomizer.hh"

Randomizer::Randomizer(bool random) :
	_random(random)
{ }

void Randomizer::reset()
{
	if (static_cast<int>(_index) < _minimum_variables)
	{
		_minimum_variables = _index;
	}

	if (static_cast<int>(_index) > _maximum_variables)
	{
		_maximum_variables = _index;
	}

	_index = 0;
}

int Randomizer::get_index()
{
	return _index;
}

int Randomizer::get_int(int min_value, int max_value)
{
	return std::min(_next() + min_value, max_value);
}

int Randomizer::get_set_variable_count() const
{
	return std::count_if(data.begin(), data.end(), [](int i) { return i > 0; });
}

double Randomizer::get_score() const
{
	double sum = 0;

	for (decltype(data)::size_type i = 0; i != data.size(); i++)
	{
		if (data[i] > 0)
		{
			sum += i * (data[i] / 2);
			
			if (data[i] % 2 == 1)
			{
				sum += static_cast<double>(i) / 1000000.0;
			}
		}
	}

	return sum;
}

int Randomizer::get_minimum_variables() const
{
	return _minimum_variables;
}

int Randomizer::get_maximum_variables() const
{
	return _maximum_variables;
}

void Randomizer::set_implicit_index(int index)
{
	_implicit_index = index;
}

bool Randomizer::is_implicit() const
{
	return _index >= _implicit_index;
}

int Randomizer::_next()
{
	if (_index == data.size())
	{
		if (_random)
		{
			// TODO: Add a random value.
		}
		else
		{
			data.push_back(0);
		}
	}

	return data[_index++];
}
