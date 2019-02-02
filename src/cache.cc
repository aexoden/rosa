#include <boost/format.hpp>

#include "cache.hh"
#include "state.hh"

Cache::Cache() {}

Cache::~Cache() {}

std::pair<int, Milliframes> DynamicCache::get(const State & state) {
	auto keys{state.get_keys()};

	if (_cache.count(keys) == 0) {
		return std::make_pair(-1, Milliframes::max());
	}

	return _cache.at(keys);
}

void DynamicCache::set(const State & state, int value, Milliframes frames) {
	_cache[state.get_keys()] = std::make_pair(value, frames);
}

std::size_t DynamicCache::get_count() const {
	return _cache.size();
}

FixedCache::FixedCache(std::size_t size) : _size(size), _cache{size} {}

#include <iostream>

std::pair<int, Milliframes> FixedCache::get(const State & state) {
	auto keys{state.get_keys()};
	const auto & [test_keys, result] = _cache[_get_index(keys)];

	if (keys != test_keys) {
		return std::make_pair(-1, Milliframes::max());
	}

	return result;
}

void FixedCache::set(const State & state, int value, Milliframes frames) {
	auto keys{state.get_keys()};

	_cache[_get_index(keys)] = std::make_pair(keys, std::make_pair(value, frames));
}

std::size_t FixedCache::get_count() const {
	return _size;
}

std::size_t FixedCache::_get_index(const std::tuple<uint64_t, uint64_t, uint64_t> & keys) {
	return boost::hash<std::tuple<uint64_t, uint64_t, uint64_t>>{}(keys) % _size;
}
