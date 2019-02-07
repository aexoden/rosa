#include <iostream>

#include <boost/format.hpp>

#include "cache.hh"
#include "state.hh"

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

FixedCache::FixedCache(std::size_t size) : _size(size), _cache{size} {}

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

std::size_t FixedCache::_get_index(const std::tuple<uint64_t, uint64_t, uint64_t> & keys) {
	return boost::hash<std::tuple<uint64_t, uint64_t, uint64_t>>{}(keys) % _size;
}

PersistentCache::PersistentCache(const std::string & filename) : _env{lmdb::env::create()} {
	_env.set_mapsize(1UL * 1024UL * 1024UL * 1024UL * 1024UL);
	_env.open(filename.c_str(), MDB_NOSYNC, 0664);

	auto txn{lmdb::txn::begin(_env)};
	_dbi = lmdb::dbi::open(txn, nullptr);

	txn.abort();
}

std::pair<int, Milliframes> PersistentCache::get(const State & state) {
	auto txn{lmdb::txn::begin(_env, nullptr, MDB_RDONLY)};
	auto result{std::make_pair(-1, Milliframes::max())};

	auto key{_encode_key(state)};
	std::string_view value;

	if (_dbi.get(txn, key, value)) {
		result = _decode_value(value);
	}

	txn.abort();

	return result;
}

void PersistentCache::set(const State & state, int value, Milliframes frames) {
	auto key{_encode_key(state)};
	auto encoded_value{_encode_value(value, frames)};

	auto txn{lmdb::txn::begin(_env)};
	_dbi.put(txn, key, encoded_value);

	txn.commit();
}

std::string PersistentCache::_encode_key(const State & state) const {
	auto [key1, key2, key3] = state.get_keys();
	std::string result{sizeof(key1) + sizeof(key2) + sizeof(key3), 0, std::string::allocator_type{}};

	std::copy_n(reinterpret_cast<char*>(&key1), sizeof(key1), result.begin()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	std::copy_n(reinterpret_cast<char*>(&key2), sizeof(key2), result.begin() + sizeof(key1)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	std::copy_n(reinterpret_cast<char*>(&key3), sizeof(key3), result.begin() + sizeof(key1) + sizeof(key2)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

	return result;
}

std::string PersistentCache::_encode_value(int value, Milliframes frames) const {
	int64_t value1{static_cast<int64_t>(value)};
	int64_t value2{static_cast<int64_t>(frames.count())};

	std::string result{sizeof(int64_t) * 2, 0, std::string::allocator_type{}};

	std::copy(reinterpret_cast<char*>(&value1), reinterpret_cast<char*>(&value1) + sizeof(value1), result.begin()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)
	std::copy(reinterpret_cast<char*>(&value2), reinterpret_cast<char*>(&value2) + sizeof(value2), result.begin() + sizeof(value1)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)

	return result;
}

std::pair<int, Milliframes> PersistentCache::_decode_value(const std::string_view & data) const {
	int64_t value1{0};
	int64_t value2{0};

	std::copy(data.begin(), data.begin() + sizeof(value1), reinterpret_cast<char*>(&value1)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
	std::copy(data.begin() + sizeof(value1), data.begin() + sizeof(value1) + sizeof(value2), reinterpret_cast<char*>(&value2)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast, cppcoreguidelines-pro-bounds-pointer-arithmetic)

	return std::make_pair(static_cast<int>(value1), Milliframes{value2});
}
