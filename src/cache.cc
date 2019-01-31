#include <boost/format.hpp>

#include "cache.hh"
#include "state.hh"

std::pair<int, Milliframes> Cache::get(const State & state) {
	auto keys{state.get_keys()};

	if (_cache.count(keys) == 0) {
		return std::make_pair(-1, Milliframes::max());
	}

	return _cache.at(keys);
}

void Cache::set(const State & state, int value, Milliframes frames) {
	_cache[state.get_keys()] = std::make_pair(value, frames);
}

int Cache::get_count() const {
	return static_cast<int>(_cache.size());
}
