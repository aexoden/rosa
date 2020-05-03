#ifndef ROSA_MAP_HH
#define ROSA_MAP_HH

#include <string>
#include <unordered_map>

struct Map {
	int encounter_rate;
	int encounter_group;

	std::string title;
	std::string description;
};

class Maps {
	public:
		explicit Maps(std::istream & input);

		auto get_map(int id) const -> const Map &;

	private:
		const Map _default_map{0, 0, "", ""};

		std::unordered_map<int, const Map> _maps;
};

#endif // ROSA_MAP_HH
