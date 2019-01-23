#ifndef SPOONY_MAP_HH
#define SPOONY_MAP_HH

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

		const Map & get_map(int id) const;

	private:
		const Map _default_map{0, 0, "", ""};

		std::unordered_map<int, const Map> _maps;
};

#endif // SPOONY_MAP_HH
