#include <iostream>
#include <numeric>

#include <boost/format.hpp>
#include <boost/range/adaptor/indexed.hpp>

#include "peglib.h"

#include "engine.hh"
#include "version.hh"

constexpr int SEED_UPDATE_DELTA = 17;
constexpr auto FRAMES_PER_TRANSITION = 82_f;
constexpr auto FRAMES_PER_TILE = 16_f;

auto search_expression_next_token(const std::string & expression, std::size_t & index) -> std::string {
	while (expression.at(index) == ' ') {
		index++;
	}

	std::locale locale;
	std::size_t original_index{index};
	std::size_t length{1};

	while (std::isdigit(expression.at(++index), locale)) {
		length++;
	}

	return expression.substr(original_index, length);
}

Engine::Engine(Parameters parameters) : _parameters{std::move(parameters)} {
	switch (parameters.cache_type) {
		case CacheType::Dynamic:
			_cache = std::make_unique<DynamicCache>();
			break;
		case CacheType::Persistent:
			_cache = std::make_unique<PersistentCache>(_parameters.cache_location, _parameters.cache_size);
			break;
	}

	for (const auto & instruction : _parameters.route) {
		if (instruction.variable >= 0) {
			if (_variables.count(instruction.variable) == 0) {
				switch (instruction.type) {
					case InstructionType::Choice:
						_variables.emplace(std::make_pair(instruction.variable, Variable{VariableType::Choice, 0, 0, _parameters.maximum_extra_steps > 0 ? instruction.number - 1 : 0}));
						break;
					case InstructionType::Path:
						_variables.emplace(std::make_pair(instruction.variable, Variable{VariableType::Step, 0, 0, _parameters.maximum_extra_steps}));
						break;
					case InstructionType::Data:
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

auto Engine::optimize(int seed) -> std::string {
	State state{seed};

	int minimum_step_segments{-1};

	if (_parameters.maximum_step_segments >= 0) {
		if (_parameters.prefer_fewer_locations) {
			minimum_step_segments = 0;
		} else {
			minimum_step_segments = _parameters.maximum_step_segments;
		}
	}

	Milliframes best_result{Milliframes::max()};
	int best_step_segments{-1};

	for (auto i{minimum_step_segments}; i <= _parameters.maximum_step_segments; i++) {
		if (i >= 0) {
			state.remaining_segments = static_cast<uint16_t>(i);
		}

		auto result{_optimize(state)};

		if (result < best_result) {
			best_result = result;
			best_step_segments = i;
		}
	}

	if (best_step_segments >= 0) {
		state.remaining_segments = static_cast<uint16_t>(best_step_segments);
	}

	auto log{_finalize(state)};

	return _generate_output_text(state, log);
}

auto Engine::_finalize(State state) -> Log {
	Log log;

	while (state.index < _parameters.route.size()) {
		auto instruction = _parameters.route[state.index];
		auto [value, frames] = _cache->get(state);

		if (value < 0) {
			std::cerr << "BUG: _finalize() attempted to use uncached state...\n";
		}

		if (_parameters.maximum_extra_steps > 0 && instruction.variable > 0) {
			if (_variables.at(instruction.variable).minimum == _variables.at(instruction.variable).maximum) {
				value = _variables.at(instruction.variable).minimum;
			}
		}

		log.emplace_back(LogEntry{state});
		_cycle(&state, &log[log.size() - 1], value);

		if (value > 0) {
			_variables[instruction.variable].value = value;

			if (instruction.type == InstructionType::Path && state.remaining_segments > 0 && _parameters.maximum_step_segments >= 0) {
				state.remaining_segments--;
			}
		}
	}

	return log;
}

auto Engine::_generate_output_text(const State & state, const Log & log) -> std::string {
	Milliframes total_frames{0_mf};
	Milliframes encounter_frames{0_mf};

	int total_optional_steps{0};
	int total_extra_steps{0};
	int total_encounters{0};

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
			case InstructionType::Data:
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
				encounter_frames += frames;
				total_encounters++;
			}
		}

		total_frames += entry.frames;
		indent_level = new_indent_level;
	}

	std::string output;

	output += (boost::format("ROUTE\t%s\n") % _route_title).str();
	output += (boost::format("VERSION\t%d\n") % _route_version).str();
	output += (boost::format("ROSA\t%s\n") % ROSA_VERSION).str();
	output += (boost::format("SEED\t%d\n") % state.step_seed).str();
	output += (boost::format("MAXSTEP\t%d\n") % _parameters.maximum_extra_steps).str();
	output += (boost::format("MAXSEG\t%d\n") % _parameters.maximum_step_segments).str();
	output += (boost::format("TASMODE\t%d\n") % (_parameters.tas_mode ? 1 : 0)).str();
	output += (boost::format("MINIMUM\t%d\n") % (_parameters.maximum_step_segments >= 0 && _parameters.prefer_fewer_locations ? 1 : 0)).str();
	output += (boost::format("FRAMES\t%d\n") % total_frames.count()).str();

	std::set<int> keys;

	for (const auto & [key, value] : _variables) {
		keys.insert(key);
	}

	std::string variable_output;

	for (const auto & key : keys) {
		if (_variables[key].value > 0) {
			variable_output += (variable_output.empty() ? "" : " ") + (boost::format("%07X:%d") % key % _variables[key].value).str();
		}
	}

	output += (boost::format("VARS\t%s\n\n") % variable_output).str();
	output += raw_output + '\n';
	output += (boost::format("%-21s%0.3fs\n") % "Encounter Time:" % Seconds(encounter_frames).count()).str();
	output += (boost::format("%-21s%0.3fs\n") % "Other Time:" % Seconds(total_frames - encounter_frames).count()).str();
	output += (boost::format("%-21s%0.3fs\n\n") % "Total Time:" % Seconds(total_frames).count()).str();

	Engine base_engine{Parameters{_parameters.route, _parameters.encounters, _parameters.maps, 0, _parameters.tas_mode, false, true, -1, CacheType::Dynamic, ""}};
	auto base_frames{base_engine._optimize(state)};
	auto base_log{base_engine._finalize(state)};

	output += (boost::format("%-21s%0.3fs\n") % "Base Total Time:" % Seconds(base_frames).count()).str();
	output += (boost::format("%-21s%0.3fs\n\n") % "Time Saved:" % Seconds(base_frames - total_frames).count()).str();

	output += (boost::format("%-21s%d\n") % "Optional Steps:" % total_optional_steps).str();
	output += (boost::format("%-21s%d\n") % "Extra Steps:" % total_extra_steps).str();
	output += (boost::format("%-21s%d\n\n") % "Encounters:" % total_encounters).str();

	auto base_encounters{std::accumulate(base_log.begin(), base_log.end(), 0, [](const auto a, const auto & entry) {
		int encounters{0};

		for (const auto & [step, encounter_index, encounter_id, frames] : entry.encounters) {
			if (step <= entry.steps) {
				encounters++;
			}
		}

		return a + encounters;
	})};

	output += (boost::format("%-21s%d\n") % "Base Encounters:" % base_encounters).str();
	output += (boost::format("%-21s%d\n\n") % "Encounters Saved:" % (base_encounters - total_encounters)).str();

	output += (boost::format("%-21s%d\n") % "Number of Variables:" % _variables.size()).str();

	return output;
}

auto Engine::_optimize(const State & state) -> Milliframes {
	if (state.index == _parameters.route.size()) {
		return 0_mf;
	}

	auto [value, frames] = _cache->get(state);
	bool update_cache{value < 0};

	auto instruction{_parameters.route[state.index]};

	int minimum{0};
	int maximum{0};

	if (instruction.variable > 0) {
		minimum = _variables.at(instruction.variable).minimum;
		maximum = _variables.at(instruction.variable).maximum;
	}

	if (instruction.type == InstructionType::Path && state.remaining_segments == 0) {
		maximum = minimum;
	}

	if (value >= 0 && (minimum != maximum || _parameters.always_allow_cache)) {
		return frames;
	}

	value = -1;
	frames = Milliframes::max();

	for (int i = minimum; i <= maximum || frames == Milliframes::max(); i++) {
		State work_state{state};

		if (instruction.type == InstructionType::Path && i > 0 && _parameters.maximum_step_segments >= 0 && work_state.remaining_segments > 0) {
			work_state.remaining_segments--;
		}

		auto result{_cycle(&work_state, nullptr, i)};

		if (result < Milliframes::max()) {
			result += _optimize(work_state);
		}

		if (result < frames) {
			value = i;
			frames = result;
		}
	}

	if (update_cache) {
		_cache->set(state, value, frames);
	}

	return frames;
}

auto Engine::_cycle(State * state, LogEntry * log, int value) -> Milliframes {
	Milliframes frames{0};
	const auto & instruction{_parameters.route[state->index]};

	switch (instruction.type) {
		case InstructionType::Choice:
			while (value >= 0) {
				state->index++;

				int level{0};

				while (level > 0 || _parameters.route[state->index].type != InstructionType::Option) {
					if (_parameters.route[state->index].type == InstructionType::Choice) {
						level++;
					} else if (_parameters.route[state->index].type == InstructionType::End) {
						level--;
					}

					state->index++;
				}

				value--;
			}

			frames += instruction.transition_count * FRAMES_PER_TRANSITION;

			if (log != nullptr) {
				log->extra_text = _parameters.route[state->index].text;
			}

			break;
		case InstructionType::Delay:
			frames += Frames{instruction.number};
			break;
		case InstructionType::End:
		case InstructionType::Note:
			break;
		case InstructionType::Option: {
			int level{0};

			while (level > 0 || _parameters.route[state->index + 1].type != InstructionType::End) {
				if (_parameters.route[state->index + 1].type == InstructionType::Choice) {
					level++;
				} else if (_parameters.route[state->index + 1].type == InstructionType::End) {
					level--;
				}

				state->index++;
			}

			break;
		}
		case InstructionType::Party:
			state->party = Party{instruction.text};
			break;
		case InstructionType::Path: {
			frames += instruction.transition_count * FRAMES_PER_TRANSITION;
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

			if ((log != nullptr) && state->search_active) {
				int extra_steps{UINT8_MAX + 1 - instruction.required_steps - value};

				if (extra_steps > 0) {
					State work_state{*state};
					_step(&work_state, log, 0, extra_steps);
					log->steps -= extra_steps;
				}
			}

			if (instruction.end_search) {
				if (state->search_active) {
					return Milliframes::max();
				}

				state->search_party = Party{""};
				state->search_active = false;
			}

			break;
		}
		case InstructionType::Route:
			_route_title = instruction.text;
			break;
		case InstructionType::Save:
			// TODO(jason@calindora.com): This needs to be implemented, but it
			// can wait for TAS mode. A reset is defined as 697 frames minus the
			// number in the instruction.
			break;
		case InstructionType::Search:
			state->search_targets.clear();

			for (const auto & encounter_id : instruction.numbers) {
				state->search_targets.push_back(static_cast<std::size_t>(encounter_id));
			}

			state->search_expression = instruction.expression;
			state->search_values.fill(false);
			state->search_party = Party{instruction.party};
			state->search_active = true;

			break;
		case InstructionType::Version:
			_route_version = instruction.number;
			break;
		case InstructionType::Data:
			break;
	}

	state->index++;

	if (log != nullptr) {
		log->frames = frames;
	}

	return frames;
}

auto Engine::_step(State * state, LogEntry * log, int tiles, int steps) -> Milliframes {
	const auto & instruction{_parameters.route[state->index]};
	const auto & map{_parameters.maps.get_map(instruction.map)};

	Milliframes frames{tiles * FRAMES_PER_TILE};

	for (auto i{0}; i < steps; i++) {
		state->step_index = (state->step_index + 1) % (UINT8_MAX + 1);

		if (state->step_index == 0) {
			state->step_seed = (state->step_seed + SEED_UPDATE_DELTA) % (UINT8_MAX + 1);
		}

		if ((_rng_data[static_cast<std::size_t>(state->step_index)] + state->step_seed) % (UINT8_MAX + 1) < map.encounter_rate) {
			auto encounter_rng{(_rng_data[static_cast<std::size_t>(state->encounter_index)] + state->encounter_seed) % (UINT8_MAX + 1)};
			std::size_t encounter_group_index{7}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

			if (encounter_rng < 43) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				encounter_group_index = 0;
			} else if (encounter_rng < 86) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				encounter_group_index = 1;
			} else if (encounter_rng < 129) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				encounter_group_index = 2;
			} else if (encounter_rng < 172) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				encounter_group_index = 3;
			} else if (encounter_rng < 204) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				encounter_group_index = 4;
			} else if (encounter_rng < 236) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				encounter_group_index = 5; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			} else if (encounter_rng < 252) { // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				encounter_group_index = 6; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
			}

			auto encounter{_parameters.encounters.get_encounter_from_group(static_cast<std::size_t>(map.encounter_group), encounter_group_index)};
			auto encounter_id{encounter->get_id()};
			auto encounter_frames{encounter->get_duration(state->party, _parameters.tas_mode)};

			frames += encounter_frames;

			if (log != nullptr) {
				auto encounter_step{state->step_index - log->state.step_index};
				auto step_seed_delta{state->step_seed - log->state.step_seed};

				if (step_seed_delta > 0) {
					encounter_step += (step_seed_delta / SEED_UPDATE_DELTA) * (UINT8_MAX + 1);
				} else if (step_seed_delta < 0) {
					encounter_step += ((step_seed_delta + (UINT8_MAX + 1)) / SEED_UPDATE_DELTA) * (UINT8_MAX + 1);
				}

				log->encounters.emplace_back(std::make_tuple(encounter_step, state->encounter_index, encounter_id, encounter_frames));
			}

			if (state->search_active) {
				_assign_search_encounter(state, encounter_id, *state->search_expression);

				if (_check_search_complete(state, *state->search_expression)) {
					state->party = state->search_party;
					state->search_active = false;
				}
			}

			state->encounter_index = (state->encounter_index + 1) % (UINT8_MAX + 1);

			if (state->encounter_index == 0) {
				state->encounter_seed = (state->encounter_seed + SEED_UPDATE_DELTA) % (UINT8_MAX + 1);
			}
		}
	}

	if (log != nullptr) {
		log->steps += steps;
	}

	return frames;
}

auto Engine::_check_search_complete(State * state, const peg::Ast & expression) -> bool {
	using peg::udl::operator""_;

	switch (expression.tag) {
		case "disjunction"_:
			return std::any_of(expression.nodes.begin(), expression.nodes.end(), [state](const auto & node){ return _check_search_complete(state, *node); });
		case "conjunction"_:
		case "sequence"_:
			return std::all_of(expression.nodes.begin(), expression.nodes.end(), [state](const auto & node){ return _check_search_complete(state, *node); });
		case "number"_:
			return state->search_values.at(expression.token_to_number<std::size_t>());
		default:
			std::cerr << "BUG: Unimplemented tag in _check_search_complete. Please report this." << std::endl;
			break;
	}

	return false;
}

auto Engine::_assign_search_encounter(State * state, std::size_t encounter_id, const peg::Ast & expression) -> bool {
	using peg::udl::operator""_;

	switch (expression.tag) {
		case "conjunction"_:
			for (const auto & node : expression.nodes) {
				if (!_check_search_complete(state, *node)) {
					if (_assign_search_encounter(state, encounter_id, *node)) {
						return true;
					}
				}
			}

			break;
		case "disjunction"_:
			for (const auto & node : expression.nodes) {
				_assign_search_encounter(state, encounter_id, *node);
			}

			break;
		case "number"_: {
			auto number = expression.token_to_number<std::size_t>();

			if (!state->search_values.at(number) && state->search_targets.at(number) == encounter_id) {
				state->search_values.at(number) = true;
				return true;
			}

			break;
		}
		case "sequence"_:
			for (const auto & node: expression.nodes) {
				if (!_check_search_complete(state, *node)) {
					return _assign_search_encounter(state, encounter_id, *node);
				}
			}

			break;
		default:
			std::cerr << "BUG: Unimplemented tag in _assign_search_encounter. Please report this." << std::endl;
			break;
	}

	return false;
}
