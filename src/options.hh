#ifndef SPOONY_OPTIONS_HH
#define SPOONY_OPTIONS_HH

#include <glibmm/optioncontext.h>
#include <glibmm/optionentry.h>
#include <glibmm/optiongroup.h>

class Options {
	public:
		static Glib::OptionEntry create_option_entry(const Glib::ustring & long_name, const gchar & short_name, const Glib::ustring & description);

		Glib::ustring output_directory = "output/routes";

		bool output_result = false;

		Glib::ustring route = "paladin";

		Glib::ustring algorithm = "pair";

		Glib::ustring variables = "";

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
