#ifndef SPOONY_INSTRUCTION_HH
#define SPOONY_INSTRUCTION_HH

#include <istream>
#include <memory>
#include <set>
#include <vector>

enum class InstructionType {
	NOOP,
	CHOICE,
	DELAY,
	END,
	NOTE,
	OPTION,
	PARTY,
	PATH,
	ROUTE,
	SAVE,
	SEARCH,
	VERSION,
	WAIT
};

class Instruction {
	public:
		explicit Instruction(const std::string & line);

		InstructionType type = InstructionType::NOOP;
		std::string text;
		std::string party;

		std::set<int> numbers;

		int number = 0;

		int tiles = 0;

		int required_steps = 0;
		int optional_steps = 0;

		int encounter_rate = 0;
		int encounter_group = 0;

		int transition_count = 0;

		bool take_extra_steps = false;
		bool can_single_step = false;
		bool can_double_step = false;
		bool is_world_map = false;
};

std::vector<std::shared_ptr<const Instruction>> read_instructions(std::istream & input);

#endif // SPOONY_INSTRUCTION_HH
