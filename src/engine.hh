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

#ifndef SPOONY_ENGINE_HH
#define SPOONY_ENGINE_HH

#include <memory>
#include <vector>

#include "encounter.hh"
#include "instruction.hh"
#include "parameters.hh"

class LogEntry
{
	public:
		LogEntry(const std::shared_ptr<const Instruction> & instruction, int indent);

		const std::shared_ptr<const Instruction> instruction;
		const int indent = 0;

		int steps = 0;

		bool save_reset = false;
		int new_seed = 0;

		int seed_start = 0;
		int index_start = 0;

		std::string party;

		std::map<int, std::pair<int, std::shared_ptr<const Encounter>>> encounters;
		std::map<int, std::pair<int, std::shared_ptr<const Encounter>>> potential_encounters;
		std::map<int, double> step_details;
};

class Engine
{
	public:
		Engine(const Parameters & parameters, const std::vector<std::shared_ptr<const Instruction>> & instructions, const Encounters & encounters);

		void reset();
		void run();

		Glib::ustring format_output(const Engine & base_engine) const;
		double get_frames() const;
		double get_minimum_frames() const;
		int get_initial_seed() const;
		Glib::ustring get_title() const;
		int get_version() const;
		int get_maximum_steps() const;

		int get_variable_count() const;
		int get_score() const;

		static double frames_to_seconds(double frames);

	private:
		void _cycle();
		std::shared_ptr<const Encounter> _get_encounter();
		void _reset(int seed);
		void _step(int tiles, int steps, bool simulate);
		void _transition(const std::shared_ptr<const Instruction> & instruction);

		Parameters _parameters;

		int _step_seed;
		int _step_index;

		int _encounter_seed;
		int _encounter_index;

		int _encounter_rate;
		int _encounter_group;

		std::vector<std::shared_ptr<const Instruction>> _instructions;
		std::vector<std::shared_ptr<const Instruction>>::size_type _instruction_index = 0;

		double _frames = 0;
		double _encounter_frames = 0;
		double _minimum_frames = 0;

		Encounters _encounters;

		int _encounter_count = 0;

		std::set<int> _encounter_search;
		bool _encounter_search_area;
		std::string _encounter_search_party;

		std::string _party;

		std::vector<LogEntry> _log;

		Glib::ustring _title;
		int _version;

		int _indent = 0;

		bool _full_minimum;
};

#endif // SPOONY_ENGINE_HH
