#ifndef ROSA_CACHE_HH
#define ROSA_CACHE_HH

#include "duration.hh"
#include "state.hh"

#include "lmdb++.h"

#include <boost/functional/hash.hpp>
#include <tsl/sparse_map.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>

enum class CacheType {
	Dynamic,
	Persistent
};

class Cache {
	public:
		Cache() = default;
		Cache(const Cache &) = delete;
		Cache(const Cache &&) = delete;
		auto operator=(const Cache &) -> Cache & = delete;
		auto operator=(const Cache &&) -> Cache & = delete;

		virtual ~Cache();

		virtual auto get(const State & state) -> std::pair<int, Milliframes> = 0;
		virtual void set(const State & state, int value, Milliframes frames) = 0;

		[[nodiscard]] virtual auto get_size() const -> std::size_t = 0;
};

class DynamicCache : public Cache {
	public:
		auto get(const State & state) -> std::pair<int, Milliframes> override;
		void set(const State & state, int value, Milliframes frames) override;

		[[nodiscard]] auto get_size() const -> std::size_t override;

	private:
		tsl::sparse_map<std::tuple<uint64_t, uint64_t, uint64_t>, std::pair<int, Milliframes>, boost::hash<std::tuple<uint64_t, uint64_t, uint64_t>>> _cache;
};

class PersistentCache : public Cache {
	public:
		explicit PersistentCache(const std::string & filename, std::size_t cache_size);
		PersistentCache(const PersistentCache &) = delete;
		PersistentCache(const PersistentCache &&) = delete;
		auto operator=(const PersistentCache &) -> PersistentCache & = delete;
		auto operator=(const PersistentCache &&) -> PersistentCache && = delete;

		~PersistentCache() override;

		auto get(const State & state) -> std::pair<int, Milliframes> override;
		void set(const State & state, int value, Milliframes frames) override;

		[[nodiscard]] auto get_size() const -> std::size_t override;

	private:
		static auto _encode_key(std::tuple<uint64_t, uint64_t, uint64_t> keys) -> std::string;
		static auto _encode_value(int value, Milliframes frames) -> std::string;
		static auto _decode_value(const std::string_view & data) -> std::pair<int, Milliframes>;

		void _purge_cache() noexcept;

		tsl::sparse_map<std::tuple<uint64_t, uint64_t, uint64_t>, std::pair<int, Milliframes>, boost::hash<std::tuple<uint64_t, uint64_t, uint64_t>>> _cache;

		const std::size_t _cache_size;

		lmdb::env _env;
		lmdb::dbi _dbi;
};

#endif // ROSA_CACHE_HH
