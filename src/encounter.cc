#include <iostream>

#include <giomm/datainputstream.h>
#include <glibmm/regex.h>

#include "encounter.hh"

Encounter::Encounter(unsigned int id, const Glib::ustring & description) :
	_id{id},
	_description{description}
{ }

unsigned int Encounter::get_id() const
{
	return _id;
}

Glib::ustring Encounter::get_description() const
{
	return _description;
}

void Encounter::add_duration(const std::string & party, double average, double minimum)
{
	_average_duration[party] = average;
	_minimum_duration[party] = minimum;
}

double Encounter::get_average_duration(const std::string & party) const
{
	if (_average_duration.count(party) > 0)
	{
		return _average_duration.at(party);
	}
	else
	{
		std::cerr << "WARNING: Party '" << party << "' not found for encounter " << _id << std::endl;
		return 10000;
	}
}

double Encounter::get_minimum_duration(const std::string & party) const
{
	if (_minimum_duration.count(party) > 0)
	{
		return _minimum_duration.at(party);
	}
	else
	{
		std::cerr << "WARNING: Party '" << party << "' not found for encounter " << _id << std::endl;
		return 10000;
	}
}

double Encounter::get_duration(bool minimum, const std::string & party) const
{
	if (minimum)
	{
		return get_minimum_duration(party);
	}
	else
	{
		return get_average_duration(party);
	}
}

Encounters::Encounters(const Glib::RefPtr<Gio::File> & file) :
	_encounters{512},
	_encounter_groups{512}
{
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

				_encounters[id]->add_duration(tokens[3], std::stod(tokens[4]), std::stod(tokens[5]));
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
