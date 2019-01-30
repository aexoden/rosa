#ifndef SPOONY_CACHE_HH
#define SPOONY_CACHE_HH

#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>

#include <boost/functional/hash.hpp>

#include "duration.hh"
#include "state.hh"

class Cache {
	public:
		std::pair<int, Milliframes> get(const State & state);
		void set(const State & state, int value, Milliframes frames);

		int get_count() const;

	private:
		std::unordered_map<std::tuple<uint64_t, uint64_t, uint64_t>, std::pair<int, Milliframes>, boost::hash<std::tuple<uint64_t, uint64_t, uint64_t>>> _cache;
};

#endif // SPOONY_CACHE_HH
