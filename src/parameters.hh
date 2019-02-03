#ifndef ROSA_PARAMETERS_HH
#define ROSA_PARAMETERS_HH

#include <memory>
#include <unordered_map>

#include "cache.hh"
#include "variable.hh"

struct Parameters {
	public:
		const Route route;

		const Encounters encounters;
		const Maps maps;

		const int maximum_extra_steps{256};
		const bool tas_mode = false;

		CacheType cache_type = CacheType::Dynamic;
		std::size_t cache_size{1048576};
		std::string cache_location;
};

#endif // ROSA_PARAMETERS_HH
