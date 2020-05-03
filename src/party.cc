#include "party.hh"

#include <boost/format.hpp>

#include <iostream>

const int PARTY_LENGTH = 20;

Party::Party(std::string party) : _party{std::move(party)} {
	if (_party.length() == PARTY_LENGTH) {
		_three_front = _party[0] == '3';
		_has_gp = _party[1] == 'G';
		_on_world_map = _party[2] == '+';

		_level = std::stoi(_party.substr(18, 2)); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

		for (std::size_t i = 3; i < 18; i += 3) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			auto character{static_cast<int>(_party[i])};

			if (character == '-') {
				character = 0;
			} else if (character >= 'A' && character <= 'Z') {
				character = character - 'A' + 1;
			} else if (character >= 'a' && character <= 'z') {
				character = character - 'a' + ('Z' - 'A' + 1) + 1;
			} else {
				std::cerr << "WARNING: Invalid character '" << _party[i] << "' in party '" << _party << "'\n";
			}

			int agility{0};

			try {
				agility = std::stoi(_party.substr(i + 1, 2));
			} catch (...) {}

			_characters.emplace_back(character, agility);
		}
	}

	_key1 += _three_front ? 1ULL << 15U : 0; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	_key1 += _has_gp ? 1ULL << 14U : 0; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	_key1 += _on_world_map ? 1ULL << 13U : 0; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	_key1 += static_cast<uint16_t>(_level);

	uint64_t characters{0};
	uint64_t agilities{0};

	for (const auto & [character, agility] : _characters) {
		characters = (characters << 6U) + static_cast<uint64_t>(character); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
		agilities = agilities * 100 + static_cast<uint64_t>(agility); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	}

	_key2 = (characters << 34U) + agilities;// NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
}

auto Party::get_keys() const -> std::pair<uint16_t, uint64_t> {
	return std::make_pair(_key1, _key2);
}

auto operator<<(std::ostream & os, const Party & party) -> std::ostream & {
	os << party._party;
	return os;
}
