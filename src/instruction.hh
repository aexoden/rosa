#ifndef SPOONY_INSTRUCTION_HH
#define SPOONY_INSTRUCTION_HH

#include <set>

#include <glibmm/ustring.h>

enum class InstructionType
{
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

class Instruction
{
	public:
		Instruction(const Glib::ustring & line);

		InstructionType type = InstructionType::NOOP;
		Glib::ustring text;
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

std::vector<std::shared_ptr<const Instruction>> read_instructions(const Glib::RefPtr<Gio::File> & file);

#endif // SPOONY_INSTRUCTION_HH
