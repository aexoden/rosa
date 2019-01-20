#ifndef SPOONY_ENCOUNTER_HH
#define SPOONY_ENCOUNTER_HH

#include <memory>
#include <unordered_map>
#include <vector>

#include "duration.hh"

class Encounter {
	public:
		Encounter(int id, const std::string & description);

		int get_id() const;
		std::string get_description() const;

		void add_duration(const std::string & party, const Duration & duration);
		Milliframes get_duration(const std::string & party, bool minimum) const;

	private:
		const int _id;
		const std::string _description;

		std::unordered_map<std::string, Duration> _durations;
};

class Encounters {
	public:
		Encounters(std::istream & file);

		std::shared_ptr<const Encounter> get_encounter(int id);
		std::shared_ptr<const Encounter> get_encounter_from_group(int group_index, int encounter_index);

	private:
		std::vector<std::shared_ptr<Encounter>> _encounters;
		std::vector<std::vector<int>> _encounter_groups;
};

#endif // SPOONY_ENCOUNTER_HH
