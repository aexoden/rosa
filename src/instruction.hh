#ifndef SPOONY_INSTRUCTION_HH
#define SPOONY_INSTRUCTION_HH

#include <istream>
#include <memory>
#include <set>

enum class InstructionType {
	Choice,
	Delay,
	End,
	Note,
	Option,
	Party,
	Path,
	Route,
	Save,
	Search,
	Version
};

class Instruction {
	public:
		explicit Instruction(const std::string & line);

		InstructionType type = InstructionType::Note;
		std::string text;
		std::string party;

		std::vector<int> numbers;

		int variable = -1;

		int number = 0;

		int tiles = 0;

		int required_steps = 0;
		int optional_steps = 0;

		int map = -1;

		int transition_count = 0;

		bool can_single_step = false;
		bool can_double_step = false;
		bool can_step_during_save = false;

		bool end_search = false;
};

using Route = std::vector<Instruction>;

Route read_route(std::istream & input);

#endif // SPOONY_INSTRUCTION_HH
