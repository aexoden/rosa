#ifndef ROSA_STATE_HH
#define ROSA_STATE_HH

#include <unordered_map>

#include "map.hh"
#include "party.hh"

struct State {
	int step_seed; // NOLINT(misc-non-private-member-variables-in-classes)
	int step_index{0}; // NOLINT(misc-non-private-member-variables-in-classes)

	int encounter_seed{(step_seed * 2) % (UINT8_MAX + 1)}; // NOLINT(misc-non-private-member-variables-in-classes)
	int encounter_index{0}; // NOLINT(misc-non-private-member-variables-in-classes)

	std::size_t index{0}; // NOLINT(misc-non-private-member-variables-in-classes)

	uint16_t remaining_segments{std::numeric_limits<uint16_t>::max()}; // NOLINT(misc-non-private-member-variables-in-classes)

	Party party{""}; // NOLINT(misc-non-private-member-variables-in-classes)
	Party search_party{""}; // NOLINT(misc-non-private-member-variables-in-classes)

	std::unordered_map<std::size_t, int> search_targets{}; // NOLINT(misc-non-private-member-variables-in-classes)
	bool search_active{false}; // NOLINT(misc-non-private-member-variables-in-classes)

	auto get_keys() const -> std::tuple<uint64_t, uint64_t, uint64_t> {
		const auto [party_key1, party_key2] = party.get_keys();

		uint64_t key1{static_cast<uint64_t>(party_key1) << 48U}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		uint64_t key2{static_cast<uint64_t>(0)};
		uint64_t key3{static_cast<uint64_t>(party_key2)};

		key1 += static_cast<uint64_t>(remaining_segments) << 32U; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		key1 += static_cast<uint64_t>(step_seed) << 24U; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		key1 += static_cast<uint64_t>(step_index) << 16U; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		key1 += static_cast<uint64_t>(encounter_seed) << 8U; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		key1 += static_cast<uint64_t>(encounter_index);

		for (const auto & [id, count] : search_targets) {
			key2 = (key2 << 9U) + static_cast<uint64_t>(id); // NOLINT: readability-magic-numbers
			key2 = (key2 << 3U) + static_cast<uint64_t>(count); // NOLINT: readability-magic-numbers
		}

		key2 += static_cast<uint64_t>(index) << 48U; // NOLINT: readability-magic-numbers

		return std::make_tuple(key1, key2, key3);
	}

	auto operator==(const State & other) const -> bool {
		return get_keys() == other.get_keys();
	}
};

#endif // ROSA_STATE_HH
