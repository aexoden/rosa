#include <istream>
#include <ostream>
#include <vector>

#include "duration.hh"
#include "encounter.hh"

class Engine;
class Randomizer;

class RouteOutput {
	public:
		RouteOutput(std::istream & input);

		bool is_valid(int current_version) const;

		std::string get_spoony_version() const;
		std::vector<std::pair<std::size_t, int>> get_variables() const;

		int get_maximum_steps() const;

		Milliframes get_frames() const;
		double get_score() const;
		std::size_t get_variable_count() const;

		static std::vector<std::pair<std::size_t, int>> parse_variable_data(const std::string & variable_data);
		static bool write_route(const std::string & filename, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, bool normalize);

	private:
		int _version = 0;

		std::string _spoony_version = "";
		std::vector<std::pair<std::size_t, int>> _variables;

		bool _valid = false;

		int _maximum_steps = 0;

		Milliframes _frames = std::numeric_limits<Milliframes>::max();
		double _score = std::numeric_limits<double>::min();
};
