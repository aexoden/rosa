#include <boost/format.hpp>

#include "cache.hh"
#include "state.hh"

std::pair<int, Milliframes> Cache::get(const State & state) {
	if (_cache.count(state) == 0) {
		return std::make_pair(-1, std::numeric_limits<Milliframes>::max());
	}

	return _cache.at(state);
}

void Cache::set(const State & state, int value, Milliframes frames) {
	_cache[state] = std::make_pair(value, frames);
}

int Cache::get_count() const {
	return static_cast<int>(_cache.size());
}
