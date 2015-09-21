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

#ifndef SPOONY_ENCOUNTER_HH
#define SPOONY_ENCOUNTER_HH

#include <giomm/file.h>
#include <glibmm/ustring.h>

class Encounter
{
	public:
		Encounter(unsigned int id, const Glib::ustring & description, double average_duration, double minimum_duration);

		unsigned int get_id() const;

		Glib::ustring get_description() const;

		double get_average_duration() const;
		double get_minimum_duration() const;

	private:
		const unsigned int _id;

		const Glib::ustring _description;

		const double _average_duration;
		const double _minimum_duration;
};

class Encounters
{
	public:
		Encounters(const Glib::RefPtr<Gio::File> & file);

		std::shared_ptr<const Encounter> get_encounter(int id);
		std::shared_ptr<const Encounter> get_encounter_from_group(int group_index, int encounter_index);

	private:
		std::vector<std::shared_ptr<const Encounter>> _encounters;
		std::vector<std::vector<unsigned int>> _encounter_groups;
};

#endif // SPOONY_ENCOUNTER_HH
