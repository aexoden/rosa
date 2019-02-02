#ifndef ROSA_OPTIONS_HH
#define ROSA_OPTIONS_HH

class Options {
	public:
		std::string route{"paladin"};

		std::string variables{""};

		std::string cache_type{"dynamic"};
		std::size_t cache_size{1000000};

		bool tas_mode{false};
		int seed{0};
		int maximum_steps{0};
};

#endif
