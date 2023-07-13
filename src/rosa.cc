#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/format.hpp>

#include "CLI/CLI.hpp"

#include "encounter.hh"
#include "engine.hh"
#include "instruction.hh"
#include "map.hh"
#include "options.hh"
#include "parameters.hh"
#include "version.hh"

/*
 * Main Function
 */

auto main (int argc, char ** argv) -> int {
	/*
	 * Initialization
	 */

	std::cout << std::fixed << std::setprecision(3);

	Options options;

	/*
	 * Option Parsing
	 */

	CLI::App app{std::string{"Rosa "} + std::string{ROSA_VERSION}};

	std::map<std::string, CacheType> cache_type_map{{"dynamic", CacheType::Dynamic}, {"persistent", CacheType::Persistent}};

	app.add_option("-r,--route", options.route, "Route to process")
		->capture_default_str();
	app.add_option("-s,--seed", options.seed, "Seed to process")
		->capture_default_str();
	app.add_option("-m,--maximum-steps", options.maximum_steps, "Maximum number of extra steps per segment")
		->capture_default_str();
	app.add_option("-n,--maximum-step-segments", options.maximum_step_segments, "Maximum number of segments where extra steps can be taken");
	app.add_option("-v,--variables", options.variables, "Explicitly set variable constraints in the form variable:value[-max_value]");

	app.add_flag("-t,--tas-mode", options.tas_mode, "Use options appropriate for TAS Routing");
	app.add_flag("-p,--prefer-fewer-locations", options.prefer_fewer_locations, "Prefer fewer locations with extra steps when maximum step segments is set.");

	app.add_option("-c,--cache-type", options.cache_type, "The type of cache to use")
		->capture_default_str()
		->transform(CLI::CheckedTransformer(cache_type_map, CLI::ignore_case).description(CLI::detail::generate_map(CLI::detail::smart_deref(cache_type_map), true)));
	app.add_option("-l,--cache-location", options.cache_location, "The location for the cache if using a persistent cache");
	app.add_option("-f,--cache-filename", options.cache_filename, "The filename for the cache if using a persistent cache");
	app.add_option("-x,--cache-size", options.cache_size, "The size of the temporary in-memory cache if using a persistent cache");

	try {
		app.parse(argc, argv);
	} catch (const CLI::ParseError & e) {
		return app.exit(e);
	}

	/*
	 * Base Data
	 */

	std::string route_source_filename{"data/routes/" + options.route + ".txt"};
	std::ifstream route_source_file{route_source_filename, std::ios_base::in};

	if (!route_source_file.is_open()) {
		std::cerr << "ERROR: Failed to open " << route_source_filename << '\n';
		return EXIT_FAILURE;
	}

	auto route{read_route(route_source_file)};

	std::string data_key{"ff2us"};

	for (const auto & instruction : route) {
		if (instruction.type == InstructionType::Data) {
			data_key = instruction.text;
		}
	}

	std::string encounters_filename{"data/encounters/" + data_key + ".txt"};
	std::ifstream encounters_file{encounters_filename, std::ios_base::in};

	if (!encounters_file.is_open()) {
		std::cerr << "ERROR: Failed to open " << encounters_filename << '\n';
		return EXIT_FAILURE;
	}

	Encounters encounters{encounters_file};

	std::string maps_filename{"data/maps/" + data_key + ".txt"};
	std::ifstream maps_file{maps_filename, std::ios_base::in};

	if (!maps_file.is_open()) {
		std::cerr << "ERROR: Failed to open " << maps_filename << '\n';
		return EXIT_FAILURE;
	}

	Maps maps{maps_file};

	auto cache_type{options.cache_type};
	auto cache_location{options.cache_filename};

	if (cache_type == CacheType::Persistent) {
		if (cache_location.empty()) {
			if (options.cache_location.empty()) {
				cache_location = "cache";
			} else {
				cache_location = options.cache_location;
			}

			cache_location += (boost::format("/%s-%03d.mdb") % options.route % options.seed).str();
		}
	}

	/*
	 * Optimization
	 */

	Engine engine{Parameters{route, encounters, maps, options.maximum_steps, options.tas_mode, options.prefer_fewer_locations, options.variables.empty(), options.maximum_step_segments, cache_type, cache_location, options.cache_size}};

	if (!options.variables.empty()) {
		std::vector<std::string> variables;
		boost::algorithm::split(variables, options.variables, boost::is_any_of(" "), boost::token_compress_on);

		try {
			for (const auto & variable : variables) {
				std::vector<std::string> tokens;
				boost::algorithm::split(tokens, variable, boost::is_any_of(":"));

				std::vector<std::string> values;
				boost::algorithm::split(values, tokens[1], boost::is_any_of("-"));

				auto index{std::stoi(tokens[0], nullptr, 16)}; // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
				auto minimum{std::stoi(values[0])};
				auto maximum{minimum};

				if (values.size() > 1) {
					maximum = std::stoi(values[1]);
				}

				engine.set_variable_minimum(index, minimum);
				engine.set_variable_maximum(index, maximum);
			}
		} catch (...) {
			std::cerr << "WARNING: Invalid variable data supplied\n";
		}
	}

	std::cout << engine.optimize(options.seed);

	return EXIT_SUCCESS;
}
