#ifndef ROSA_VARIABLE_HH
#define ROSA_VARIABLE_HH

#include <unordered_map>

enum class VariableType {Step, Choice};

struct Variable {
	const VariableType type{VariableType::Step};
	int value{0};
	int minimum{0};
	int maximum{0};
};

using Variables = std::unordered_map<int, Variable>;

#endif // ROSA_VARIABLE_HH
