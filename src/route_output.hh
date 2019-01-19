#include <vector>

#include <giomm/file.h>
#include <glibmm/regex.h>
#include <glibmm/ustring.h>

#include "encounter.hh"

class Engine;
class Randomizer;

class RouteOutput
{
	public:
		RouteOutput(const Glib::RefPtr<Gio::File> & file);

		bool is_valid(int current_version) const;

		Glib::ustring get_spoony_version() const;
		std::vector<std::pair<std::vector<int>::size_type, int>> get_variables() const;

		int get_maximum_steps() const;

		milliframes get_frames() const;
		double get_score() const;
		int get_variable_count() const;

		static std::vector<std::pair<std::vector<int>::size_type, int>> parse_variable_data(const Glib::ustring & variable_data);
		static bool write_route(const Glib::RefPtr<Gio::File> & file, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, bool normalize);

	private:
		int _version = 0;

		Glib::ustring _spoony_version = "";
		std::vector<std::pair<std::vector<int>::size_type, int>> _variables;

		bool _valid = false;

		int _maximum_steps = 0;

		milliframes _frames = std::numeric_limits<milliframes>::max();
		double _score = std::numeric_limits<double>::min();
};
