#ifndef ROSA_INSTRUCTION_HH
#define ROSA_INSTRUCTION_HH

#include <istream>
#include <memory>
#include <set>
#include <vector>

#include "peglib.h"

enum class InstructionType {
	Choice,
	Data,
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

		InstructionType type = InstructionType::Note; // NOLINT(misc-non-private-member-variables-in-classes)
		std::string text; // NOLINT(misc-non-private-member-variables-in-classes)
		std::string party; // NOLINT(misc-non-private-member-variables-in-classes)

		std::string expression_string; // NOLINT(misc-non-private-member-variables-in-classes)
		std::shared_ptr<peg::Ast> expression; // NOLINT(misc-non-private-member-variables-in-classes)

		std::vector<int> numbers; // NOLINT(misc-non-private-member-variables-in-classes)

		int variable = -1; // NOLINT(misc-non-private-member-variables-in-classes)

		int number = 0; // NOLINT(misc-non-private-member-variables-in-classes)

		int tiles = 0; // NOLINT(misc-non-private-member-variables-in-classes)

		int required_steps = 0; // NOLINT(misc-non-private-member-variables-in-classes)
		int optional_steps = 0; // NOLINT(misc-non-private-member-variables-in-classes)

		int map = -1; // NOLINT(misc-non-private-member-variables-in-classes)

		int transition_count = 0; // NOLINT(misc-non-private-member-variables-in-classes)

		bool can_single_step = false; // NOLINT(misc-non-private-member-variables-in-classes)
		bool can_double_step = false; // NOLINT(misc-non-private-member-variables-in-classes)
		bool can_step_during_save = false; // NOLINT(misc-non-private-member-variables-in-classes)

		bool end_search = false; // NOLINT(misc-non-private-member-variables-in-classes)
};

using Route = std::vector<Instruction>;

auto read_route(std::istream & input) -> Route;

#endif // ROSA_INSTRUCTION_HH
