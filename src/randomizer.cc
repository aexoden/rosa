#include "algorithm"

#include "randomizer.hh"

void Randomizer::reset() {
	if (_index < _minimum_variables) {
		_minimum_variables = _index;
	}

	if (_index > _maximum_variables) {
		_maximum_variables = _index;
	}

	_index = 0;
}

std::size_t Randomizer::get_index() {
	return _index;
}

int Randomizer::get_int(int min_value, int max_value) {
	return std::min(_next() + min_value, max_value);
}

int Randomizer::get_set_variable_count() const {
	return static_cast<int>(std::count_if(data.begin(), data.end(), [](int i) { return i > 0; }));
}

std::size_t Randomizer::get_minimum_variables() const {
	return _minimum_variables;
}

std::size_t Randomizer::get_maximum_variables() const {
	return _maximum_variables;
}

void Randomizer::set_implicit_index(std::size_t index) {
	_implicit_index = index;
}

bool Randomizer::is_implicit() const {
	return _index >= _implicit_index;
}

int Randomizer::_next() {
	if (_index == data.size()) {
		data.push_back(0);
	}

	return data[_index++];
}
