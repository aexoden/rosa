/*
 * Copyright (c) 2015 Jason Lynch <jason@calindora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SPOONY_OPTIONS_HH
#define SPOONY_OPTIONS_HH

#include <glibmm/optioncontext.h>
#include <glibmm/optionentry.h>
#include <glibmm/optiongroup.h>

class Options
{
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
