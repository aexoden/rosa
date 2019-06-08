#ifndef ROSA_CACHE_HH
#define ROSA_CACHE_HH

#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>

#include <boost/functional/hash.hpp>
#include <tsl/sparse_map.h>
#include "lmdb++.h"

#include "duration.hh"
#include "state.hh"

enum class CacheType {
	Dynamic,
	Persistent
};

class Cache {
	public:
		Cache() = default;
		Cache(const Cache &) = delete;
		Cache & operator=(const Cache &) = delete;

		virtual ~Cache();

		virtual std::pair<int, Milliframes> get(const State & state) = 0;
		virtual void set(const State & state, int value, Milliframes frames) = 0;
};

class DynamicCache : public Cache {
	public:
		std::pair<int, Milliframes> get(const State & state) override;
		void set(const State & state, int value, Milliframes frames) override;

	private:
		tsl::sparse_map<std::tuple<uint64_t, uint64_t, uint64_t>, std::pair<int, Milliframes>, boost::hash<std::tuple<uint64_t, uint64_t, uint64_t>>> _cache;
};

class PersistentCache : public Cache {
	public:
		explicit PersistentCache(const std::string & filename);

		std::pair<int, Milliframes> get(const State & state) override;
		void set(const State & state, int value, Milliframes frames) override;

	private:
		std::string _encode_key(const State & state) const;
		std::string _encode_value(int value, Milliframes frames) const;
		std::pair<int, Milliframes> _decode_value(const std::string_view & data) const;

		lmdb::env _env;
		lmdb::dbi _dbi;
};

#endif // ROSA_CACHE_HH
