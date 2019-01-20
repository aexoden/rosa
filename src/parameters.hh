#ifndef SPOONY_PARAMETERS_HH
#define SPOONY_PARAMETERS_HH

#include <memory>

#include "randomizer.hh"

struct Parameters {
	public:
		const bool tas_mode = false;
		const bool step_output = false;

		const int seed = 0;
		const int maximum_extra_steps = 256;

		std::string algorithm = "ils+pair";

		const std::shared_ptr<Randomizer> randomizer = nullptr;
};

#endif // SPOONY_PARAMETERS_HH
