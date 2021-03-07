#ifndef ROSA_PARAMETERS_HH
#define ROSA_PARAMETERS_HH

#include "cache.hh"
#include "encounter.hh"
#include "instruction.hh"
#include "variable.hh"

#include <memory>
#include <unordered_map>

struct Parameters {
	public:
		const Route route;

		const Encounters encounters;
		const Maps maps;

		const int maximum_extra_steps{256};
		const bool tas_mode{false};
		const bool prefer_fewer_locations{false};
		const bool always_allow_cache{true};

		const int maximum_step_segments{-1};

		CacheType cache_type = CacheType::Dynamic;
		std::string cache_location;
		const std::size_t cache_size{4294967295};
};

#endif // ROSA_PARAMETERS_HH
