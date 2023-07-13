#include "encounter.hh"

#include "state.hh"

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

#include <iostream>

using namespace std::chrono_literals;

const int MAXIMUM_ENCOUNTERS = 512;

Encounter::Encounter(std::size_t id, std::string description) : _id{id}, _description{std::move(description)} { }

auto Encounter::get_id() const -> std::size_t {
	return _id;
}

auto Encounter::get_description() const -> std::string {
	return _description;
}

void Encounter::add_duration(const Party & party, const Duration & duration) {
	_durations[party] = duration;
}

auto Encounter::get_duration(const Party & party, bool minimum) const -> Milliframes {
	if (_durations.count(party) == 0) {
		std::cerr << "WARNING: Party '" << party << "' not found for encounter " << _id << "... assuming 30 seconds\n";
		return std::chrono::duration_cast<Milliframes>(30s);
	}

	return minimum ? _durations.at(party).minimum : _durations.at(party).average;
}

Encounters::Encounters(std::istream & input) : _encounters{MAXIMUM_ENCOUNTERS}, _encounter_groups{MAXIMUM_ENCOUNTERS} {
	std::string line;

	while (std::getline(input, line)) {
		std::vector<std::string> tokens;
		boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);

		if (!tokens.empty() && line[0] != '#') {
			if (tokens[0] == "ENCOUNT" && tokens.size() == 6) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				std::size_t id{std::stoul(tokens[1])};

				std::string description{tokens[2]};
				Party party{tokens[3]};

				Milliframes average_duration{static_cast<int>(std::stod(tokens[4]) * 1000)}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				Milliframes minimum_duration{static_cast<int>(std::stod(tokens[5]) * 1000)}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

				if (!_encounters[id]) {
					_encounters[id] = std::make_shared<Encounter>(id, description);
				}

				_encounters[id]->add_duration(party, Duration{average_duration, minimum_duration});
			} else if (tokens[0] == "GROUP" && tokens.size() == 10) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				std::size_t id{std::stoul(tokens[1])};

				for (std::size_t i = 0; i < tokens.size() - 2; i++) {
					_encounter_groups[id].push_back(std::stoul(tokens[i + 2]));
				}
			} else {
				std::cerr << "WARNING: Unrecognized line in encounter data: " << line << '\n';
			}
		}
	}
}

auto Encounters::get_encounter(std::size_t id) const -> std::shared_ptr<const Encounter> {
	return _encounters[id];
}

auto Encounters::get_encounter_from_group(std::size_t group_index, std::size_t encounter_index) const -> std::shared_ptr<const Encounter> {
	auto encounter{_encounters[_encounter_groups[group_index][encounter_index]]};

	if (!encounter) {
		std::cerr << "WARNING: Attempted to use nonexistent encounter: " << _encounter_groups[group_index][encounter_index] << std::endl;
	}

	return encounter;
}
