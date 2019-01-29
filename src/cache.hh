#ifndef SPOONY_CACHE_HH
#define SPOONY_CACHE_HH

#include <string>
#include <unordered_map>

#include "duration.hh"
#include "state.hh"

class Cache {
	public:
		std::pair<int, Milliframes> get(const State & state);
		void set(const State & state, int value, Milliframes frames);

		int get_count() const;

	private:
		std::string _get_key(const State & state);

		std::unordered_map<std::string, std::pair<int, Milliframes>> _cache;
};

#endif // SPOONY_CACHE_HH
