#ifndef ROSA_ENCOUNTER_HH
#define ROSA_ENCOUNTER_HH

#include <memory>
#include <unordered_map>
#include <vector>

#include "duration.hh"
#include "party.hh"

class Encounter {
	public:
		Encounter(std::size_t id, std::string description);

		std::size_t get_id() const;
		std::string get_description() const;

		void add_duration(const Party & party, const Duration & duration);
		Milliframes get_duration(const Party & party, bool minimum) const;

	private:
		const std::size_t _id;
		const std::string _description;

		std::unordered_map<Party, Duration> _durations;
};

class Encounters {
	public:
		explicit Encounters(std::istream & input);

		std::shared_ptr<const Encounter> get_encounter(std::size_t id) const;
		std::shared_ptr<const Encounter> get_encounter_from_group(std::size_t group_index, std::size_t encounter_index) const;

	private:
		std::vector<std::shared_ptr<Encounter>> _encounters;
		std::vector<std::vector<std::size_t>> _encounter_groups;
};

#endif // ROSA_ENCOUNTER_HH
