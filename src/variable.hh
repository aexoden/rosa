#ifndef SPOONY_VARIABLE_HH
#define SPOONY_VARIABLE_HH

#include <unordered_map>

enum class VariableType {
	CHOICE,
	STEP
};

struct Variable {
	const VariableType type;
	int value;
	int minimum;
	int maximum;
	int implicit_value = 0;
};

using Variables = std::unordered_map<int, Variable>;

#endif // SPOONY_VARIABLE_HH
