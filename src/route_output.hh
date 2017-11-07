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

#include <vector>

#include <giomm/file.h>
#include <glibmm/regex.h>
#include <glibmm/ustring.h>

class Engine;
class Randomizer;

class RouteOutput
{
	public:
		RouteOutput(const Glib::RefPtr<Gio::File> & file);

		bool is_valid(int current_version) const;

		Glib::ustring get_spoony_version() const;
		std::vector<std::pair<std::vector<int>::size_type, int>> get_variables() const;

		int get_maximum_steps() const;

		double get_frames() const;
		int get_variable_count() const;
		double get_score() const;

		static std::vector<std::pair<std::vector<int>::size_type, int>> parse_variable_data(const Glib::ustring & variable_data);
		static bool write_route(const Glib::RefPtr<Gio::File> & file, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, bool normalize);

	private:
		int _version = 0;

		Glib::ustring _spoony_version = "";
		std::vector<std::pair<std::vector<int>::size_type, int>> _variables;

		bool _valid = false;

		int _maximum_steps = 0;

		double _frames = std::numeric_limits<double>::max();
};
