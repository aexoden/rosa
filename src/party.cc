#include "party.hh"

#include <boost/format.hpp>

#include <iostream>

Party::Party(std::string party) : _party{std::move(party)} {
	if (_party.length() == 20) {
		_three_front = _party[0] == '3';
		_has_gp = _party[1] == 'G';
		_on_world_map = _party[2] == '+';

		_level = std::stoi(_party.substr(18, 2));

		for (std::size_t i = 3; i < 18; i += 3) {
			auto character{static_cast<int>(_party[i])};;

			if (character == 0x2D) {
				character = 0;
			} else if (character >= 0x41 && character <= 0x5A) {
				character = character - 0x41 + 1;
			} else if (character >= 0x61 && character <= 0x7A) {
				character = character - 0x61 + 27;
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

	_key1 += _three_front ? 1ull << 15u : 0;
	_key1 += _has_gp ? 1ull << 14u : 0;
	_key1 += _on_world_map ? 1ull << 13u : 0;

	_key1 += static_cast<uint16_t>(_level);

	uint64_t characters{0};
	uint64_t agilities{0};

	for (const auto & [character, agility] : _characters) {
		characters = (characters << 6u) + static_cast<uint64_t>(character);
		agilities = agilities * 100 + static_cast<uint64_t>(agility);
	}

	_key2 = (characters << 34u) + agilities;
}

std::pair<uint16_t, uint64_t> Party::get_keys() const {
	return std::make_pair(_key1, _key2);
}

std::ostream & operator<<(std::ostream & os, const Party & party) {
	os << party._party;
	return os;
}
