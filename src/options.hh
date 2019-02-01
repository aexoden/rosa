#ifndef ROSA_OPTIONS_HH
#define ROSA_OPTIONS_HH

class Options {
	public:
		std::string route{"paladin"};

		std::string variables{""};

		bool tas_mode{false};
		int seed{0};
		int maximum_steps{0};
};

#endif
