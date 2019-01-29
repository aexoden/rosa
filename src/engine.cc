#include <iostream>
#include <numeric>

#include <boost/format.hpp>

#include "engine.hh"
#include "version.hh"

Engine::Engine(Parameters parameters) : _parameters{std::move(parameters)} {
	for (const auto & instruction : _parameters.route) {
		if (instruction.variable >= 0) {
			if (_variables.count(instruction.variable) == 0) {
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
}

void Engine::set_variable_minimum(int variable, int value) {
	_variables[variable].minimum = value;
}

void Engine::set_variable_maximum(int variable, int value) {
	_variables[variable].maximum = value;
}

std::string Engine::optimize(int seed) {
	State state{seed};

	_optimize(state);
	auto log{_finalize(state)};

	return _generate_output_text(state, log);
}

Log Engine::_finalize(State state) {
	Log log;

	while (state.index < _parameters.route.size()) {
		auto instruction = _parameters.route[state.index];
		auto [value, frames] = _cache.get(state);

		log.emplace_back(LogEntry{state});
		_cycle(&state, &log[log.size() - 1], value);

		if (value > 0) {
			_variables[instruction.variable].value = value;
		}
	}

	return log;
}

std::string Engine::_generate_output_text(const State & state, const Log & log) {
	Milliframes total_frames{0_mf};
	Milliframes encounter_frames{0_mf};

	int total_optional_steps{0};
	int total_extra_steps{0};
	std::size_t total_encounters{0};

	std::string raw_output;
	std::size_t indent_level{0};

	for (const auto & entry : log) {
		auto instruction{_parameters.route[entry.state.index]};
		std::string description;
		std::size_t new_indent_level{indent_level};

		switch (instruction.type) {
			case InstructionType::Path:
				description = _parameters.maps.get_map(instruction.map).description;

				if (instruction.end_search) {
					new_indent_level--;
				}

				break;
			case InstructionType::Choice:
				description = entry.extra_text;
				new_indent_level++;
				break;
			case InstructionType::Search:
				description = instruction.text;
				new_indent_level++;
				break;
			case InstructionType::Note:
				description = instruction.text;
				break;
			case InstructionType::End:
				new_indent_level--;
				break;
			case InstructionType::Delay:
			case InstructionType::Option:
			case InstructionType::Party:
			case InstructionType::Route:
			case InstructionType::Save:
			case InstructionType::Version:
				break;
		}

		if (!description.empty()) {
			raw_output += (boost::format("%-58sSeed: %3d   Index: %3d\n") % (boost::format("%s%s") % std::string(indent_level * 2, ' ') % description).str() % entry.state.step_seed % entry.state.step_index).str();
		}

		if (entry.steps > 0 || instruction.optional_steps > 0) {
			auto steps{entry.steps - instruction.required_steps};
			auto optional_steps{std::min(instruction.optional_steps, steps)};
			auto extra_steps{steps - optional_steps};

			if (extra_steps % 2 == 1 && optional_steps > 0) {
				optional_steps--;
				extra_steps++;
			}

			total_optional_steps += optional_steps;
			total_extra_steps += extra_steps;

			if (instruction.optional_steps > 0) {
				raw_output += (boost::format("%sOptional Steps: %d\n") % std::string((indent_level + 1) * 2, ' ') % optional_steps).str();
			}

			if (extra_steps > 0) {
				raw_output += (boost::format("%sExtra Steps: %d\n") % std::string((indent_level + 1) * 2, ' ') % extra_steps).str();
			}
		}

		for (const auto & [step, encounter_index, encounter_id, frames] : entry.encounters) {
			auto encounter{_parameters.encounters.get_encounter(static_cast<std::size_t>(encounter_id))};
			if (step > entry.steps) {
				raw_output += (boost::format("%s(Step %3d: %d / %s)\n") % std::string((indent_level + 1) * 2 - 1, ' ') % step % (encounter_index + 1) % encounter->get_description()).str();
			} else {
				raw_output += (boost::format("%sStep %3d: %d / %s (%0.3fs)\n") % std::string((indent_level + 1) * 2, ' ') % step % (encounter_index + 1) % encounter->get_description() % Seconds(frames).count()).str();
			}

			encounter_frames += frames;
			total_encounters++;
		}

		total_frames += entry.frames;
		indent_level = new_indent_level;
	}

	std::string output;

	output += (boost::format("ROUTE\t%s\n") % _route_title).str();
	output += (boost::format("VERSION\t%d\n") % _route_version).str();
	output += (boost::format("SPOONY\t%s\n") % SPOONY_VERSION).str();
	output += (boost::format("SEED\t%d\n") % state.step_seed).str();
	output += (boost::format("MAXSTEP\t%d\n") % _parameters.maximum_extra_steps).str();
	output += (boost::format("TASMODE\t%d\n") % (_parameters.tas_mode ? 1 : 0)).str();
	output += (boost::format("FRAMES\t%d\n") % total_frames.count()).str();

	std::string variable_output;

	for (const auto & [key, value] : _variables) {
		if (value.value > 0) {
			variable_output += (variable_output.empty() ? "" : " ") + (boost::format("%07X:%d") % key % value.value).str();
		}
	}

	output += (boost::format("VARS\t%s\n\n") % variable_output).str();
	output += raw_output + '\n';
	output += (boost::format("%-19s%0.3fs\n") % "Encounter Time:" % Seconds(encounter_frames).count()).str();
	output += (boost::format("%-19s%0.3fs\n") % "Other Time:" % Seconds(total_frames - encounter_frames).count()).str();
	output += (boost::format("%-19s%0.3fs\n\n") % "Total Time:" % Seconds(total_frames).count()).str();

	Engine base_engine{Parameters{_parameters.route, _parameters.encounters, _parameters.maps, 0, _parameters.tas_mode}};
	auto base_frames{base_engine._optimize(state)};
	auto base_log{_finalize(state)};

	output += (boost::format("%-19s%0.3fs\n") % "Base Total Time:" % Seconds(base_frames).count()).str();
	output += (boost::format("%-19s%0.3fs\n\n") % "Time Saved:" % Seconds(base_frames - total_frames).count()).str();

	output += (boost::format("%-19s%d\n") % "Optional Steps:" % total_optional_steps).str();
	output += (boost::format("%-19s%d\n") % "Extra Steps:" % total_extra_steps).str();
	output += (boost::format("%-19s%d\n\n") % "Encounters:" % total_encounters).str();

	auto base_encounters{std::accumulate(base_log.begin(), base_log.end(), std::size_t{0}, [](const auto a, const auto & entry) {
		return a + entry.encounters.size();
	})};

	output += (boost::format("%-19s%d\n") % "Base Encounters:" % base_encounters).str();
	output += (boost::format("%-19s%d\n") % "Encounters Saved:" % (base_encounters - total_encounters)).str();

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
		auto result{_cycle(&work_state, nullptr, i)};

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

Milliframes Engine::_cycle(State * state, LogEntry * log, int value) {
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

			if (log) {
				log->extra_text = _parameters.route[state->index].text;
			}

			break;
		case InstructionType::Delay:
			frames += Frames{instruction.number};
			break;
		case InstructionType::End:
			break;
		case InstructionType::Note:
			break;
		case InstructionType::Option:
			while (_parameters.route[state->index + 1].type != InstructionType::End) {
				state->index++;
			}

			break;
		case InstructionType::Party:
			state->party = instruction.text;
			break;
		case InstructionType::Path:
			frames += instruction.transition_count * 82_f;
			frames += _step(state, log, instruction.tiles, instruction.required_steps);

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

				frames += _step(state, log, tiles, optional_steps + extra_steps);
			}

			if (instruction.end_search) {
				if (!state->search_targets.empty()) {
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

	if (log) {
		log->frames = frames;
	}

	return frames;
}

Milliframes Engine::_step(State * state, LogEntry * log, int tiles, int steps) {
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
			auto encounter_frames{encounter->get_duration(state->party, _parameters.tas_mode)};

			frames += encounter_frames;

			if (log) {
				auto encounter_step{(((state->step_seed - log->state.step_seed) % 256) / 17) * 256 + ((state->step_index - log->state.step_index) % 256)};
				log->encounters.emplace_back(std::make_tuple(encounter_step, state->encounter_index, encounter_id, encounter_frames));
			}

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

	if (log) {
		log->steps += steps;
	}

	return frames;
}
