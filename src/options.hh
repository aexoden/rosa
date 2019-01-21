#ifndef SPOONY_OPTIONS_HH
#define SPOONY_OPTIONS_HH

class Options {
	public:
		std::string output_directory = "output/routes";

		bool output_result = false;

		std::string route = "paladin";

		std::string algorithm = "pair";

		std::string variables = "";

		bool full_optimization = false;
		bool load_existing_variables = false;
		bool tas_mode = false;
		bool step_output = false;

		int seed = 43;

		int maximum_steps = 0;

		int maximum_iterations = 1000;

		int maximum_comparisons = 0;
		bool pairwise_shift = false;

		int perturbation_strength = 3;
		int perturbation_wobble = 0;
};

#endif
