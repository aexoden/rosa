/*
 * Copyright (c) 2016 Jason Lynch <jason@calindora.com>
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

#ifndef SPOONY_ROUTE_HH
#define SPOONY_ROUTE_HH

#include <map>
#include <memory>
#include <string>

class Map;

class Route
{
	public:
		explicit Route(const std::string & filename, std::map<unsigned int, std::shared_ptr<const Map>> maps);

		std::string name() const;
		unsigned int version() const;

	private:
		void _parse_line(const std::string & line);

		std::string _name;
		unsigned int _version;

		std::map<unsigned int, std::shared_ptr<const Map>> _maps;
};

/*
#include <set>

#include <glibmm/ustring.h>

enum class InstructionType
{
	NOOP,
	CHOICE,
	END,
	NOTE,
	OPTION,
	PATH,
	ROUTE,
	SAVE,
	SEARCH,
	VERSION,
	WAIT
};

class Instruction
{
	public:
		Instruction(const Glib::ustring & line);

		InstructionType type = InstructionType::NOOP;
		Glib::ustring text;

		std::set<int> numbers;

		int number = 0;

		int tiles = 0;

		int required_steps = 0;
		int optional_steps = 0;

		int encounter_rate = 0;
		int encounter_group = 0;

		int transition_count = 0;

		bool take_extra_steps = false;
		bool can_single_step = false;
		bool can_double_step = false;
		bool is_world_map = false;
};

std::vector<std::shared_ptr<const Instruction>> read_instructions(const Glib::RefPtr<Gio::File> & file);*/

#endif // SPOONY_ROUTE_HH
