#ifndef ROSA_OPTIONS_HH
#define ROSA_OPTIONS_HH

#include <string>

#include "cache.hh"

constexpr int CACHE_DEFAULT_SIZE = 1048576;

class Options {
	public:
		std::string route{"paladin"};

		std::string variables{""};

		CacheType cache_type{CacheType::Dynamic};
		std::string cache_location{""};
		std::string cache_filename{""};

		std::size_t cache_size{CACHE_DEFAULT_SIZE};

		bool tas_mode{false};
		bool prefer_fewer_locations{false};

		int seed{0};
		int maximum_steps{0};
		int maximum_step_segments{-1};
};

#endif
