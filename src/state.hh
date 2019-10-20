#ifndef ROSA_STATE_HH
#define ROSA_STATE_HH

#include <unordered_map>

#include "map.hh"
#include "party.hh"

struct State {
	int step_seed;
	int step_index{0};

	int encounter_seed{(step_seed * 2) % 256};
	int encounter_index{0};

	std::size_t index{0};

	uint16_t remaining_segments{std::numeric_limits<uint16_t>::max()};

	Party party{""};
	Party search_party{""};

	std::unordered_map<std::size_t, int> search_targets{};
	bool search_active{false};

	std::tuple<uint64_t, uint64_t, uint64_t> get_keys() const {
		const auto [party_key1, party_key2] = party.get_keys();

		uint64_t key1{static_cast<uint64_t>(party_key1) << 48u};
		uint64_t key2{static_cast<uint64_t>(index)};
		uint64_t key3{static_cast<uint64_t>(party_key2)};

		key1 += static_cast<uint64_t>(remaining_segments) << 32u;
		key1 += static_cast<uint64_t>(step_seed) << 24u;
		key1 += static_cast<uint64_t>(step_index) << 16u;
		key1 += static_cast<uint64_t>(encounter_seed) << 8u;
		key1 += static_cast<uint64_t>(encounter_index);

		for (const auto & [id, count] : search_targets) {
			key2 = (key2 << 9u) + static_cast<uint64_t>(id);
			key2 = (key2 << 3u) + static_cast<uint64_t>(count);
		}

		return std::make_tuple(key1, key2, key3);
	}

	bool operator==(const State & other) const {
		return get_keys() == other.get_keys();
	}
};

#endif // ROSA_STATE_HH
