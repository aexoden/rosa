#ifndef ROSA_CACHE_HH
#define ROSA_CACHE_HH

#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>

#include <boost/functional/hash.hpp>

#include "duration.hh"
#include "state.hh"

enum class CacheType {
	Dynamic,
	Fixed
};

class Cache {
	public:
		Cache();
		Cache(const Cache &) = delete;
		Cache & operator=(const Cache &) = delete;

		virtual ~Cache();

		virtual std::pair<int, Milliframes> get(const State & state) = 0;
		virtual void set(const State & state, int value, Milliframes frames) = 0;

		virtual std::size_t get_count() const = 0;
};

class DynamicCache : public Cache {
	public:
		std::pair<int, Milliframes> get(const State & state) override;
		void set(const State & state, int value, Milliframes frames) override;

		std::size_t get_count() const override;

	private:
		std::unordered_map<std::tuple<uint64_t, uint64_t, uint64_t>, std::pair<int, Milliframes>, boost::hash<std::tuple<uint64_t, uint64_t, uint64_t>>> _cache;
};

class FixedCache : public Cache {
	public:
		FixedCache(std::size_t size);

		std::pair<int, Milliframes> get(const State & state) override;
		void set(const State & state, int value, Milliframes frames) override;

		std::size_t get_count() const override;

	private:
		std::size_t _get_index(const std::tuple<uint64_t, uint64_t, uint64_t> & keys);

		std::size_t _size;

		std::vector<std::pair<std::tuple<uint64_t, uint64_t, uint64_t>, std::pair<int, Milliframes>>> _cache;
};

#endif // ROSA_CACHE_HH
