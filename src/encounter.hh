#ifndef SPOONY_ENCOUNTER_HH
#define SPOONY_ENCOUNTER_HH

#include <unordered_map>

#include <giomm/file.h>
#include <glibmm/ustring.h>

class Encounter
{
	public:
		Encounter(unsigned int id, const Glib::ustring & description);

		unsigned int get_id() const;

		Glib::ustring get_description() const;

		void add_duration(const std::string & party, double average, double minimum);

		double get_average_duration(const std::string & party) const;
		double get_minimum_duration(const std::string & party) const;

		double get_duration(bool minimum, const std::string & party) const;

	private:
		const unsigned int _id;

		const Glib::ustring _description;

		std::unordered_map<std::string, double> _average_duration;
		std::unordered_map<std::string, double> _minimum_duration;
};

class Encounters
{
	public:
		Encounters(const Glib::RefPtr<Gio::File> & file);

		std::shared_ptr<const Encounter> get_encounter(int id);
		std::shared_ptr<const Encounter> get_encounter_from_group(int group_index, int encounter_index);

	private:
		std::vector<std::shared_ptr<Encounter>> _encounters;
		std::vector<std::vector<unsigned int>> _encounter_groups;
};

#endif // SPOONY_ENCOUNTER_HH
