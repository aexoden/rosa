#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include "encounter.hh"

using namespace std::chrono_literals;

Encounter::Encounter(std::size_t id, std::string description) : _id{id}, _description{std::move(description)} { }

std::size_t Encounter::get_id() const {
	return _id;
}

std::string Encounter::get_description() const {
	return _description;
}

void Encounter::add_duration(const std::string & party, const Duration & duration) {
	_durations[party] = duration;
}

Milliframes Encounter::get_duration(const std::string & party, bool minimum) const {
	if (_durations.count(party) == 0) {
		std::cerr << "WARNING: Party '" << party << "' not found for encounter " << _id << "... assuming 30 seconds\n";
		return std::chrono::duration_cast<Milliframes>(30s);
	}

	return minimum ? _durations.at(party).minimum : _durations.at(party).average;
}

Encounters::Encounters(std::istream & input) : _encounters{512}, _encounter_groups{512} {
	std::string line;

	while (std::getline(input, line)) {
		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

		if (!tokens.empty() && line[0] != '#') {
			if (tokens[0] == "ENCOUNT" && tokens.size() == 6) {
				std::size_t id{std::stoul(tokens[1])};

				std::string description{tokens[2]};
				std::string party{tokens[3]};

				Milliframes average_duration{static_cast<int>(std::stod(tokens[4]) * 1000)};
				Milliframes minimum_duration{static_cast<int>(std::stod(tokens[5]) * 1000)};

				if (!_encounters[id]) {
					_encounters[id] = std::make_shared<Encounter>(id, description);
				}

				_encounters[id]->add_duration(party, Duration{average_duration, minimum_duration});
			} else if (tokens[0] == "GROUP" && tokens.size() == 10) {
				std::size_t id{std::stoul(tokens[1])};

				for (std::size_t i = 0; i < 8; i++) {
					_encounter_groups[id].push_back(std::stoul(tokens[i + 2]));
				}
			} else {
				std::cerr << "WARNING: Unrecognized line in encounter data: " << line << '\n';
			}
		}
	}
}

std::shared_ptr<const Encounter> Encounters::get_encounter(std::size_t id) {
	return _encounters[id];
}

std::shared_ptr<const Encounter> Encounters::get_encounter_from_group(std::size_t group_index, std::size_t encounter_index) {
	auto encounter{_encounters[_encounter_groups[group_index][encounter_index]]};

	if (!encounter) {
		std::cerr << "WARNING: Attempted to use nonexistent encounter: " << _encounter_groups[group_index][encounter_index] << std::endl;
	}

	return std::move(encounter);
}
