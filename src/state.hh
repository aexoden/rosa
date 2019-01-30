#ifndef SPOONY_STATE_HH
#define SPOONY_STATE_HH

#include <unordered_map>

#include "map.hh"
#include "party.hh"

struct State {
	int step_seed;
	int step_index{0};

	int encounter_seed{(step_seed * 2) % 256};
	int encounter_index{0};

	std::size_t index{0};

	Party party{""};
	Party search_party{""};

	std::unordered_map<std::size_t, int> search_targets{};

	bool operator==(const State & other) const {
		return step_seed == other.step_seed && step_index == other.step_index && encounter_seed == other.encounter_seed && encounter_index == other.encounter_index && index == other.index && party == other.party && search_targets == other.search_targets;
	}
};

namespace std {
	template <>
	struct hash<State>
	{
		std::size_t operator()(const State & state) const
		{
			size_t res = 17;

			res = res * 31 + hash<int>()(state.step_seed);
			res = res * 31 + hash<int>()(state.step_index);
			res = res * 31 + hash<int>()(state.encounter_seed);
			res = res * 31 + hash<int>()(state.encounter_index);
			res = res * 31 + hash<size_t>()(state.index);
			res = res * 31 + hash<Party>()(state.party);
			res = res * 31 + hash<size_t>()(state.search_targets.size());

			return res;

		}
	};
}

#endif // SPOONY_STATE_HH
