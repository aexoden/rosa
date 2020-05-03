#include "cache.hh"

#include "state.hh"

#include <boost/format.hpp>

#include <filesystem>
#include <iostream>

Cache::~Cache() = default;

auto DynamicCache::get(const State & state) -> std::pair<int, Milliframes> {
	auto keys{state.get_keys()};

	if (_cache.count(keys) == 0) {
		return std::make_pair(-1, Milliframes::max());
	}

	return _cache.at(keys);
}

void DynamicCache::set(const State & state, int value, Milliframes frames) {
	_cache[state.get_keys()] = std::make_pair(value, frames);
}

auto DynamicCache::get_size() const -> std::size_t {
	return _cache.size();
}

PersistentCache::PersistentCache(const std::string & filename, std::size_t cache_size) : _cache_size{cache_size}, _env{lmdb::env::create()} {
	if (std::filesystem::exists(filename)) {
		std::cerr << "Using existing cache database...\n";
	} else {
		std::cerr << "Creating new cache database...\n";
		std::filesystem::create_directories(filename);
	}

	_env.set_mapsize(1UL * 1024UL * 1024UL * 1024UL * 1024UL); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	_env.open(filename.c_str(), MDB_NOSYNC, 0664); // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	auto txn{lmdb::txn::begin(_env)};
	_dbi = lmdb::dbi::open(txn, nullptr);

	txn.abort();
}

PersistentCache::~PersistentCache() {
	_purge_cache();
}

auto PersistentCache::get(const State & state) -> std::pair<int, Milliframes> {
	auto keys{state.get_keys()};

	if (_cache.count(keys) > 0) {
		return _cache.at(keys);
	}

	auto txn{lmdb::txn::begin(_env, nullptr, MDB_RDONLY)};
	auto result{std::make_pair(-1, Milliframes::max())};

	auto key{_encode_key(keys)};
	std::string_view value;

	if (_dbi.get(txn, key, value)) {
		result = _decode_value(value);
	}

	txn.abort();

	return result;
}

void PersistentCache::set(const State & state, int value, Milliframes frames) {
	_cache[state.get_keys()] = std::make_pair(value, frames);

	if (_cache.size() > _cache_size) {
		_purge_cache();
	}
}

auto PersistentCache::get_size() const -> std::size_t {
	return _cache.size();
}

auto PersistentCache::_encode_key(std::tuple<uint64_t, uint64_t, uint64_t> keys) -> std::string {
	auto & [key1, key2, key3] = keys;
	std::string result{sizeof(key1) + sizeof(key2) + sizeof(key3), 0, std::string::allocator_type{}};

	std::copy_n(reinterpret_cast<char*>(&key1), sizeof(key1), result.begin()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	std::copy_n(reinterpret_cast<char*>(&key2), sizeof(key2), result.begin() + sizeof(key1)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	std::copy_n(reinterpret_cast<char*>(&key3), sizeof(key3), result.begin() + sizeof(key1) + sizeof(key2)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

	return result;
}

auto PersistentCache::_encode_value(int value, Milliframes frames) -> std::string {
	int64_t value1{static_cast<int64_t>(value)};
	int64_t value2{static_cast<int64_t>(frames.count())};

	std::string result{sizeof(int64_t) * 2, 0, std::string::allocator_type{}};

	std::copy(reinterpret_cast<char*>(&value1), reinterpret_cast<char*>(&value1) + sizeof(value1), result.begin()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)
	std::copy(reinterpret_cast<char*>(&value2), reinterpret_cast<char*>(&value2) + sizeof(value2), result.begin() + sizeof(value1)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)

	return result;
}

auto PersistentCache::_decode_value(const std::string_view & data) -> std::pair<int, Milliframes> {
	int64_t value1{0};
	int64_t value2{0};

	std::copy(data.begin(), data.begin() + sizeof(value1), reinterpret_cast<char*>(&value1)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	std::copy(data.begin() + sizeof(value1), data.begin() + sizeof(value1) + sizeof(value2), reinterpret_cast<char*>(&value2)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)

	return std::make_pair(static_cast<int>(value1), Milliframes{value2});
}

void PersistentCache::_purge_cache() noexcept {
	try {
		auto txn{lmdb::txn::begin(_env)};

		for (const auto & [keys, value] : _cache) {
			auto key{_encode_key(keys)};
			auto encoded_value{_encode_value(value.first, value.second)};

			_dbi.put(txn, key, encoded_value);
		}

		txn.commit();
		_cache.clear();
	} catch (const lmdb::error &) {
		std::cerr << "WARNING: Error while purging the in-memory cache. Results from this point forward may be incorrect.";
	}
}
