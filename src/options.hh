#ifndef ROSA_OPTIONS_HH
#define ROSA_OPTIONS_HH

class Options {
	public:
		std::string route{"paladin"};

		std::string variables{""};

		std::string cache_type{"dynamic"};
		std::string cache_location{""};
		std::string cache_filename{""};

		std::size_t cache_size{1048576};

		bool tas_mode{false};
		bool prefer_fewer_locations{false};

		int seed{0};
		int maximum_steps{0};
		int maximum_step_segments{-1};
};

#endif
