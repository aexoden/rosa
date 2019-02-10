#ifndef ROSA_OPTIONS_HH
#define ROSA_OPTIONS_HH

class Options {
	public:
		std::string route{"paladin"};

		std::string variables{""};

		std::string cache_type{"dynamic"};
		std::size_t cache_size{1048576};
		std::string cache_location{""};
		std::string cache_filename{""};

		bool tas_mode{false};
		int seed{0};
		int maximum_steps{0};
		int maximum_step_segments{-1};
};

#endif
