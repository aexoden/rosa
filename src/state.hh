#ifndef SPOONY_STATE_HH
#define SPOONY_STATE_HH

#include <string>
#include <unordered_map>

#include "map.hh"

struct State {
	int step_seed;
	int step_index{0};

	int encounter_seed{step_seed * 2};
	int encounter_index{0};

	std::size_t index{0};
	std::string party{""};
	std::unordered_map<std::size_t, int> search_targets{};
	std::string search_party{""};
};

#endif // SPOONY_STATE_HH
