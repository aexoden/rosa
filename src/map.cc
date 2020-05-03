#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>

#include "map.hh"

Maps::Maps(std::istream & input) {
	std::string line;

	while (std::getline(input, line)) {
		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

		if (!tokens.empty() && line[0] != '#') {
			if (tokens[0] == "MAP" && tokens.size() == 6) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				int id{std::stoi(tokens[1], nullptr, 16)}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

				if (_maps.count(id) > 0) {
					std::cerr << "WARNING: Ignoring duplicate map ID in map data: " << boost::format("%04X") % id << '\n';
				} else {
					int encounter_rate{std::stoi(tokens[2])};
					int encounter_group{std::stoi(tokens[3])};

					std::string title{tokens[4]};
					std::string description{tokens[5]}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

					_maps.emplace(id, Map{encounter_rate, encounter_group, title, description});
				}
			} else {
				std::cerr << "WARNING: Unrecognized line in map data: " << line << '\n';
			}
		}
	}
}

auto Maps::get_map(int id) const -> const Map & {
	if (_maps.count(id) == 0) {
		std::cerr << "WARNING: Attempted to retrive non-existant map ID " << boost::format("%04X") % id << '\n';
		return _default_map;
	}

	return _maps.at(id);
}
