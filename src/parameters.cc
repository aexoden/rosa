#include "parameters.hh"

Parameters::Parameters(bool tas_mode, bool step_output, int seed, int maximum_extra_steps, const Glib::ustring & algorithm, const std::shared_ptr<Randomizer> & randomizer) :
	tas_mode(tas_mode),
	step_output(step_output),
	seed(seed),
	maximum_extra_steps(maximum_extra_steps),
	algorithm(algorithm),
	randomizer(randomizer)
{ }
