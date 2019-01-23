#ifndef SPOONY_PARAMETERS_HH
#define SPOONY_PARAMETERS_HH

#include <memory>
#include <unordered_map>

#include "variable.hh"

struct Parameters {
	public:
		const bool tas_mode = false;
		const bool step_output = false;

		const int seed = 0;
		const int maximum_extra_steps = 256;

		std::string algorithm = "ils+pair";

		std::shared_ptr<Variables> variables;
};

#endif // SPOONY_PARAMETERS_HH
