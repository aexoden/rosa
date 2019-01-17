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
