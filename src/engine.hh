#ifndef SPOONY_ENGINE_HH
#define SPOONY_ENGINE_HH

#include <memory>
#include <vector>

#include "duration.hh"
#include "encounter.hh"
#include "instruction.hh"
#include "parameters.hh"

class LogEntry
{
	public:
		LogEntry(const std::shared_ptr<const Instruction> & instruction, int indent);

		const std::shared_ptr<const Instruction> instruction;
		const int indent = 0;

		int steps = 0;

		bool save_reset = false;
		int new_seed = 0;

		int seed_start = 0;
		int index_start = 0;

		std::string party;

		std::map<int, std::pair<int, std::shared_ptr<const Encounter>>> encounters;
		std::map<int, std::pair<int, std::shared_ptr<const Encounter>>> potential_encounters;
		std::map<int, Milliframes> step_details;
};

class Engine
{
	public:
		Engine(const Parameters & parameters, const std::vector<std::shared_ptr<const Instruction>> & instructions, const Encounters & encounters);

		void reset();
		void run();

		Glib::ustring format_output(const Engine & base_engine) const;
		Milliframes get_frames() const;
		Milliframes get_minimum_frames() const;
		int get_initial_seed() const;
		Glib::ustring get_title() const;
		int get_version() const;
		int get_maximum_steps() const;

		int get_variable_count() const;
		double get_score() const;

		static double frames_to_seconds(double frames);

	private:
		void _cycle();
		std::shared_ptr<const Encounter> _get_encounter();
		void _reset(int seed);
		void _step(int tiles, int steps, bool simulate);
		void _transition(const std::shared_ptr<const Instruction> & instruction);

		Parameters _parameters;

		int _step_seed;
		int _step_index;

		int _encounter_seed;
		int _encounter_index;

		int _encounter_rate;
		int _encounter_group;

		std::vector<std::shared_ptr<const Instruction>> _instructions;
		std::vector<std::shared_ptr<const Instruction>>::size_type _instruction_index = 0;

		Milliframes _frames = 0_mf;
		Milliframes _encounter_frames = 0_mf;
		Milliframes _minimum_frames = 0_mf;
		double _score = 0;

		Encounters _encounters;

		int _encounter_count = 0;

		std::set<int> _encounter_search;
		bool _encounter_search_area;
		std::string _encounter_search_party;

		std::string _party;

		std::vector<LogEntry> _log;

		Glib::ustring _title;
		int _version;

		int _indent = 0;

		bool _full_minimum;
};

#endif // SPOONY_ENGINE_HH
