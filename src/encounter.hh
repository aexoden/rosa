#ifndef SPOONY_ENCOUNTER_HH
#define SPOONY_ENCOUNTER_HH

#include <chrono>
#include <unordered_map>
#include <vector>

#include <giomm/file.h>
#include <glibmm/ustring.h>

using seconds = std::chrono::duration<double>;
using milliframes = std::chrono::duration<int64_t, std::ratio<655171, 39375000000>>;

constexpr seconds operator ""_s(long double s) {
	return seconds(s);
}

constexpr milliframes operator ""_mf(unsigned long long mf) {
	return milliframes(mf);
}

struct Duration {
	milliframes average;
	milliframes minimum;
};

class Encounter {
	public:
		Encounter(int id, const std::string & description);

		int get_id() const;
		std::string get_description() const;

		void add_duration(const std::string & party, const Duration & duration);
		milliframes get_duration(const std::string & party, bool minimum) const;

	private:
		const int _id;
		const std::string _description;

		std::unordered_map<std::string, Duration> _durations;
};

class Encounters {
	public:
		Encounters(const Glib::RefPtr<Gio::File> & file);

		std::shared_ptr<const Encounter> get_encounter(int id);
		std::shared_ptr<const Encounter> get_encounter_from_group(int group_index, int encounter_index);

	private:
		std::vector<std::shared_ptr<Encounter>> _encounters;
		std::vector<std::vector<unsigned int>> _encounter_groups;
};

#endif // SPOONY_ENCOUNTER_HH
