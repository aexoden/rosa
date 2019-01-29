#include <boost/format.hpp>

#include "cache.hh"
#include "state.hh"

std::pair<int, Milliframes> Cache::get(const State & state) {
	auto key{_get_key(state)};

	if (_cache.count(key) == 0) {
		return std::make_pair(-1, std::numeric_limits<Milliframes>::max());
	}

	return _cache.at(key);
}

void Cache::set(const State & state, int value, Milliframes frames) {
	_cache[_get_key(state)] = std::make_pair(value, frames);
}

int Cache::get_count() const {
	return static_cast<int>(_cache.size());
}

std::string Cache::_get_key(const State & state) {
	std::string targets;

	for (const auto & [target, count] : state.search_targets) {
		targets += (boost::format("%s%03X%02X") % targets % target % count).str();
	}

	return (boost::format("%03X%02X%02X%02X%02X%s%s") % state.index % state.step_seed % state.step_index % state.encounter_seed % state.encounter_index % state.party % targets).str();
}
