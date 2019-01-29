#include <iostream>

#include <boost/format.hpp>

#include "engine.hh"
#include "version.hh"

Engine::Engine(Parameters parameters) : _parameters{std::move(parameters)} {
	for (const auto & instruction : _parameters.route) {
		if (instruction.variable >= 0) {
			switch (instruction.type) {
				case InstructionType::Choice:
					_variables.emplace(std::make_pair(instruction.variable, Variable{VariableType::Choice, 0, 0, instruction.number}));
					break;
				case InstructionType::Path:
					_variables.emplace(std::make_pair(instruction.variable, Variable{VariableType::Step, 0, 0, _parameters.maximum_extra_steps}));
					break;
				case InstructionType::Delay:
				case InstructionType::End:
				case InstructionType::Note:
				case InstructionType::Option:
				case InstructionType::Party:
				case InstructionType::Route:
				case InstructionType::Save:
				case InstructionType::Search:
				case InstructionType::Version:
					break;
			}
		}
	}
}

void Engine::set_variable_minimum(int variable, int value) {
	_variables[variable].minimum = value;
}

void Engine::set_variable_maximum(int variable, int value) {
	_variables[variable].maximum = value;
}

void Engine::optimize(int seed) {
	State state{seed};

	_optimize(state);
	_finalize(state);
}

void Engine::_finalize(State state) {
	while (state.index < _parameters.route.size()) {
		auto instruction = _parameters.route[state.index];
		auto [value, frames] = _cache.get(state);

		_cycle(&state, value);

		if (value > 0) {
			_variables[instruction.variable].value = value;
		}
	}
}

std::string Engine::generate_output_text(int seed, const Engine & base_engine) {
	State state{seed};
	std::string output;

	auto mframes{_optimize(state)};

	output += (boost::format("ROUTE\t%s\n") % _route_title).str();
	output += (boost::format("VERSION\t%d\n") % _route_version).str();
	output += (boost::format("SPOONY\t%s\n") % SPOONY_VERSION).str();
	output += (boost::format("SEED\t%d\n") % seed).str();
	output += (boost::format("MAXSTEP\t%d\n") % _parameters.maximum_extra_steps).str();
	output += (boost::format("TASMODE\t%d\n") % (_parameters.tas_mode ? 1 : 0)).str();
	output += (boost::format("FRAMES\t%d\n") % mframes.count()).str();

	std::string variable_output;

	for (const auto & [key, value] : _variables) {
		if (value.value > 0) {
			variable_output += (variable_output.empty() ? "" : " ") + (boost::format("%07X:%d") % key % value.value).str();
		}
	}

	output += (boost::format("VARS\t%s\n\n") % variable_output).str();

	// TODO: Actual output

	output += (boost::format("Total Time: %0.3fs\n") % Seconds(mframes).count()).str();

	return output;
}

Milliframes Engine::_optimize(const State & state) {
	if (state.index == _parameters.route.size()) {
		return 0_mf;
	}

	auto [value, frames] = _cache.get(state);

	if (value >= 0) {
		return frames;
	}

	auto instruction{_parameters.route[state.index]};

	int minimum{0};
	int maximum{0};

	if (instruction.variable > 0) {
		minimum = _variables.at(instruction.variable).minimum;
		maximum = _variables.at(instruction.variable).maximum;
	}

	value = -1;
	frames = Milliframes::max();

	for (int i = minimum; i <= maximum || frames == Milliframes::max(); i++) {
		State work_state{state};
		auto result{_cycle(&work_state, i)};

		if (result < Milliframes::max()) {
			result += _optimize(work_state);
		}

		if (result < frames) {
			value = i;
			frames = result;
		}
	}

	_cache.set(state, value, frames);

	return frames;
}

Milliframes Engine::_cycle(State * state, int value) {
	Milliframes frames{0};
	auto & instruction{_parameters.route[state->index]};

	switch (instruction.type) {
		case InstructionType::Choice:
			while (value >= 0) {
				while (_parameters.route[state->index].type != InstructionType::Option) {
					state->index++;
				}

				value--;
			}

			frames += instruction.transition_count * 82_f;

			break;
		case InstructionType::Delay:
			frames += Frames{instruction.number};
			break;
		case InstructionType::End:
			break;
		case InstructionType::Note:
			break;
		case InstructionType::Option:
			while (_parameters.route[state->index].type != InstructionType::End) {
				state->index++;
			}

			break;
		case InstructionType::Party:
			state->party = instruction.text;
			break;
		case InstructionType::Path:
			frames += instruction.transition_count * 82_f;
			frames += _step(state, instruction.tiles, instruction.required_steps);

			if (value > 0) {
				int optional_steps{std::min(instruction.optional_steps, value)};
				int extra_steps{value - optional_steps};

				if (extra_steps % 2 == 1 && optional_steps > 0) {
					extra_steps++;
					optional_steps--;
				}

				if (extra_steps % 2 == 1 && !instruction.can_single_step) {
					extra_steps--;
				}

				int tiles{instruction.can_double_step ? extra_steps : extra_steps * 2};

				if (tiles % 2 == 1) {
					tiles++;
				}

				frames += _step(state, tiles, optional_steps + extra_steps);
			}

			if (instruction.end_search) {
				if (state->search_targets.size() > 0) {
					return Milliframes::max();
				}

				state->search_party = "";
			}

			break;
		case InstructionType::Route:
			_route_title = instruction.text;
			break;
		case InstructionType::Save:
			// TODO: This needs to be implemented, but it can wait for TAS mode.
			//       A reset is defined as 697 frames minus the number in the instruction.
			break;
		case InstructionType::Search:
			for (const auto & number : instruction.numbers) {
				if (state->search_targets.count(static_cast<std::size_t>(number)) > 0) {
					state->search_targets[static_cast<std::size_t>(number)]++;
				} else {
					state->search_targets[static_cast<std::size_t>(number)] = 1;
				}
			}

			state->search_party = instruction.party;

			break;
		case InstructionType::Version:
			_route_version = instruction.number;
			break;
	}

	state->index++;

	return frames;
}

Milliframes Engine::_step(State * state, int tiles, int steps) {
	const auto & instruction{_parameters.route[state->index]};
	const auto & map{_parameters.maps.get_map(instruction.map)};

	Milliframes frames{tiles * 16_f};

	for (auto i{0}; i < steps; i++) {
		state->step_index = (state->step_index + 1) % 256;

		if (state->step_index == 0) {
			state->step_seed = (state->step_seed + 17) % 256;
		}

		if ((_rng_data[static_cast<std::size_t>(state->step_index)] + state->step_seed) % 256 < map.encounter_rate) {
			auto encounter_rng{(_rng_data[static_cast<std::size_t>(state->encounter_index)] + state->encounter_seed) % 256};
			std::size_t encounter_group_index{7};

			if (encounter_rng < 43) {
				encounter_group_index = 0;
			} else if (encounter_rng < 86) {
				encounter_group_index = 1;
			} else if (encounter_rng < 129) {
				encounter_group_index = 2;
			} else if (encounter_rng < 172) {
				encounter_group_index = 3;
			} else if (encounter_rng < 204) {
				encounter_group_index = 4;
			} else if (encounter_rng < 236) {
				encounter_group_index = 5;
			} else if (encounter_rng < 252) {
				encounter_group_index = 6;
			}

			auto encounter{_parameters.encounters.get_encounter_from_group(static_cast<std::size_t>(map.encounter_group), encounter_group_index)};
			auto encounter_id{encounter->get_id()};

			frames += encounter->get_duration(state->party, _parameters.tas_mode);

			if (!state->search_targets.empty() && state->search_targets.count(encounter_id) > 0) {
				state->search_targets[encounter_id]--;

				if (state->search_targets[encounter_id] == 0) {
					state->search_targets.erase(encounter_id);
				}

				if (state->search_targets.empty()) {
					state->party = state->search_party;
				}
			}

			state->encounter_index = (state->encounter_index + 1) % 256;

			if (state->encounter_index == 0) {
				state->encounter_seed = (state->encounter_seed + 17) % 256;
			}
		}
	}

	return frames;
}

/*
std::string Engine::format_output(const Engine & base_engine) const {
	std::stringstream output;

	int total_optional_steps = 0;
	int total_extra_steps = 0;

	output << boost::format("ROUTE\t%1%\n") % _title;
	output << boost::format("VERSION\t%1%\n") % _version;
	output << boost::format("SPOONY\t%1%\n") % SPOONY_VERSION;
	output << boost::format("SEED\t%1%\n") % _parameters.seed;
	output << boost::format("METHOD\t%1%\n") % _parameters.algorithm;
	output << boost::format("MAXSTEP\t%1%\n") % _parameters.maximum_extra_steps;
	output << boost::format("FRAMES\t%d\n") % _frames.count();
	output << boost::format("SCORE\t%0.20f\n") % _score;

	output << "VARS\t";

	bool output_var = false;

	for (const auto & [key, variable] : *(_parameters.variables)) {
		if (variable.value > 0) {
			output << boost::format("%s%07X:%d") % (output_var ? " " : "") % key % variable.value;
			output_var = true;
		}
	}

	output << "\n\n";

	for (auto & entry : _log) {
		std::string indent;
		std::string text{entry.instruction->text};

		if (entry.instruction->map >= 0) {
			text = _maps.get_map(entry.instruction->map).description;
		}

		for (int i = 0; i < entry.indent; i++) {
			indent.append("  ");
		}

		output << boost::format("%-58sSeed: %3d   Index: %3d\n") % (indent + text) % entry.seed_start % entry.index_start;

		int optional_steps = std::min(entry.instruction->optional_steps, entry.steps - entry.instruction->required_steps);
		int extra_steps = entry.steps - entry.instruction->required_steps - optional_steps;

		if (extra_steps % 2 == 1 && optional_steps > 0) {
			extra_steps++;
			optional_steps--;
		}

		total_optional_steps += optional_steps;
		total_extra_steps += extra_steps;

		if (entry.instruction->optional_steps > 0) {
			output << indent << "  Optional Steps: " << optional_steps << '\n';
		}

		if (extra_steps > 0) {
			output << indent << "  Extra Steps: " << extra_steps << '\n';
		}

		if (entry.save_reset) {
			output << indent << "  Save and reset to seed " << entry.new_seed << '\n';
		}

		for (auto & pair : entry.encounters) {
			output << boost::format("%s  Step %3d: %d / %s (%0.3fs)\n") % indent % pair.first % pair.second.first % pair.second.second->get_description() % Seconds(pair.second.second->get_duration(entry.party, _parameters.tas_mode)).count();
		}

		for (auto & pair : entry.potential_encounters) {
			if (entry.encounters.count(pair.first) == 0) {
				output << boost::format("%s (Step %3d: %3d / %s)\n") % indent % pair.first % pair.second.first % pair.second.second->get_description();
			}
		}

		if (_parameters.step_output) {
			for (auto & pair : entry.step_details) 	{
				output << boost::format("%s  :: Step %3d :: %0.3f elapsed :: %0.3f remaining\n") % indent % pair.first % Seconds(pair.second).count() % Seconds(_frames - pair.second).count();
			}
		}
	}

	output << '\n';

	output << boost::format("%-18s %0.3fs\n") % "Encounter Time:" % Seconds(_encounter_frames).count();
	output << boost::format("%-18s %0.3fs\n") % "Other Time:" % Seconds(_frames - _encounter_frames).count();
	output << boost::format("%-18s %0.3fs\n") % "Total Time:" % Seconds(_frames).count();
	output << '\n';
	output << boost::format("%-18s %0.3fs\n") % "Base Total Time:" % Seconds(base_engine._frames).count();
	output << boost::format("%-18s %0.3fs\n") % "Time Saved:" % Seconds(base_engine._frames - _frames).count();
	output << '\n';
	output << boost::format("%-18s %d\n") % "Optional Steps:" % total_optional_steps;
	output << boost::format("%-18s %d\n") % "Extra Steps:" % total_extra_steps;
	output << boost::format("%-18s %d\n") % "Encounters:" % _encounter_count;
	output << '\n';
	output << boost::format("%-18s %d\n") % "Base Encounters:" % base_engine._encounter_count;
	output << boost::format("%-18s %d\n") % "Encounters Saved:" % (base_engine._encounter_count - _encounter_count);

	return output.str();
}

Milliframes Engine::get_frames() const {
	return _frames;
}

Milliframes Engine::get_minimum_frames() const {
	return _minimum_frames;
}

std::string Engine::get_title() const {
	return _title;
}

int Engine::get_version() const {
	return _version;
}

int Engine::get_maximum_steps() const {
	return _parameters.maximum_extra_steps;
}

double Engine::get_score() const {
	return _score;
}

int Engine::get_initial_seed() const {
	return _parameters.seed;
}


*/
