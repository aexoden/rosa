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

		std::map<int, std::shared_ptr<const Encounter>> encounters;
};

class Engine
{
	public:
		Engine(const Parameters & parameters, const std::vector<std::shared_ptr<const Instruction>> & instructions, const Encounters & encounters);

		void run();

		Glib::ustring format_output(const Engine & base_engine);

	private:
		void _cycle();
		std::shared_ptr<const Encounter> _get_encounter();
		void _reset(int seed);
		void _step(int tiles, int steps);
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

		Encounters _encounters;

		int _encounter_count = 0;

		std::shared_ptr<const Encounter> _encounter_search;

		std::vector<LogEntry> _log;

		Glib::ustring _title;
		int _version;

		int _indent = 0;
};

#endif // SPOONY_ENGINE_HH
