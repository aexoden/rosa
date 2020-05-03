#ifndef ROSA_ENCOUNTER_HH
#define ROSA_ENCOUNTER_HH

#include "duration.hh"
#include "party.hh"

#include <memory>
#include <unordered_map>
#include <vector>

class Encounter {
	public:
		Encounter(std::size_t id, std::string description);

		auto get_id() const -> std::size_t;
		auto get_description() const -> std::string;

		void add_duration(const Party & party, const Duration & duration);
		auto get_duration(const Party & party, bool minimum) const -> Milliframes;

	private:
		const std::size_t _id;
		const std::string _description;

		std::unordered_map<Party, Duration> _durations;
};

class Encounters {
	public:
		explicit Encounters(std::istream & input);

		[[nodiscard]] auto get_encounter(std::size_t id) const -> std::shared_ptr<const Encounter>;
		[[nodiscard]] auto get_encounter_from_group(std::size_t group_index, std::size_t encounter_index) const -> std::shared_ptr<const Encounter>;

	private:
		std::vector<std::shared_ptr<Encounter>> _encounters;
		std::vector<std::vector<std::size_t>> _encounter_groups;
};

#endif // ROSA_ENCOUNTER_HH
