#include <iostream>

#include <giomm/datainputstream.h>
#include <glibmm/regex.h>

#include "encounter.hh"

using namespace std::chrono_literals;

Encounter::Encounter(int id, const std::string & description) : _id{id}, _description{description} { }

int Encounter::get_id() const {
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
		std::cerr << "WARNING: Party '" << party << "' not found for encounter " << _id << "... assuming 20 seconds\n";
		return std::chrono::duration_cast<Milliframes>(20s);
	}

	return minimum ? _durations.at(party).minimum : _durations.at(party).average;
}

Encounters::Encounters(const Glib::RefPtr<Gio::File> & file) : _encounters{512}, _encounter_groups{512} {
	auto file_stream = Gio::DataInputStream::create(file->read());
	std::string line;

	auto split_regex = Glib::Regex::create("\t+");

	while (file_stream->read_line(line))
	{
		std::vector<Glib::ustring> tokens = split_regex->split(line);

		if (!tokens.empty())
		{
			if (tokens[0] == "ENCOUNT" && tokens.size() == 6)
			{
				unsigned int id = std::stoul(tokens[1]);

				if (!_encounters[id])
				{
					_encounters[id] = std::make_shared<Encounter>(id, tokens[2]);
				}

				Duration duration = {Milliframes{static_cast<int>(std::stod(tokens[4]) * 1000)}, Milliframes{static_cast<int>(std::stod(tokens[5]) * 1000)}};

				_encounters[id]->add_duration(tokens[3], duration);
			}
			else if (tokens[0] == "GROUP" && tokens.size() == 10)
			{
				unsigned int id = std::stoul(tokens[1]);

				for (int i = 0; i < 8; i++)
				{
					_encounter_groups[id].push_back(std::stoul(tokens[i + 2]));
				}
			}
			else
			{
				std::cerr << "Unrecognized line: " << line << std::endl;
			}
		}
	}
}

std::shared_ptr<const Encounter> Encounters::get_encounter(int id)
{
	return _encounters[id];
}

std::shared_ptr<const Encounter> Encounters::get_encounter_from_group(int group_index, int encounter_index)
{
	auto encounter = _encounters[_encounter_groups[group_index][encounter_index]];

	if (!encounter)
	{
		std::cerr << "WARNING: Attempted to use nonexistent encounter: " << _encounter_groups[group_index][encounter_index] << std::endl;
	}

	return encounter;
}
