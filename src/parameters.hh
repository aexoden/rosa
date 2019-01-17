#ifndef SPOONY_PARAMETERS_HH
#define SPOONY_PARAMETERS_HH

#include <memory>

#include <glibmm/ustring.h>

#include "randomizer.hh"

class Parameters
{
	public:
		Parameters(bool tas_mode, bool step_output, int seed, int maximum_extra_steps, const Glib::ustring & algorithm, const std::shared_ptr<Randomizer> & randomizer);

		const bool tas_mode;
		const bool step_output;

		const int seed;
		const int maximum_extra_steps;

		Glib::ustring algorithm;

		const std::shared_ptr<Randomizer> randomizer;
};

#endif // SPOONY_PARAMETERS_HH
