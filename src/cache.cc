#include <iostream>

#include <boost/format.hpp>
#include <leveldb/cache.h>
#include <leveldb/filter_policy.h>

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

PersistentCache::PersistentCache(const std::string & filename) {
	leveldb::Options options;
	options.create_if_missing = true;
	options.max_open_files = 524288;
	options.block_cache = leveldb::NewLRUCache(1024 * 1048576);
	options.filter_policy = leveldb::NewBloomFilterPolicy(10);

	auto status{leveldb::DB::Open(options, filename, &_db)};

	if (!status.ok()) {
		std::cerr << "WARNING: Cache error: " << status.ToString() << '\n';
	}
}

PersistentCache::~PersistentCache() {
	if (_db) {
		delete _db;
	}
}

std::pair<int, Milliframes> PersistentCache::get(const State & state) {
	if (_db) {
		auto key{_encode_key(state)};
		std::string value;

		auto status{_db->Get(leveldb::ReadOptions{}, key, &value)};

		if (status.ok() && !status.IsNotFound()) {
			return _decode_value(value);
		}

		if (!status.ok() && !status.IsNotFound()) {
			std::cerr << "WARNING: Cache error: " << status.ToString() << '\n';
		}
	}

	return std::make_pair(-1, Milliframes::max());
}

void PersistentCache::set(const State & state, int value, Milliframes frames) {
	if (_db) {
		auto key{_encode_key(state)};
		auto encoded_value{_encode_value(value, frames)};

		auto status = _db->Put(leveldb::WriteOptions{}, key, encoded_value);

		if (!status.ok()) {
			std::cerr << "WARNING: Cache error: " << status.ToString() << '\n';
		}

		_states++;
	}
}

std::string PersistentCache::_encode_key(const State & state) const {
	auto [key1, key2, key3] = state.get_keys();
	std::string result{sizeof(key1) + sizeof(key2) + sizeof(key3), 0, std::string::allocator_type{}};

	std::copy_n(reinterpret_cast<char*>(&key1), sizeof(key1), result.begin());
	std::copy_n(reinterpret_cast<char*>(&key2), sizeof(key2), result.begin() + sizeof(key1));
	std::copy_n(reinterpret_cast<char*>(&key3), sizeof(key3), result.begin() + sizeof(key1) + sizeof(key2));

	return result;
}

std::string PersistentCache::_encode_value(int value, Milliframes frames) const {
	int64_t value1{static_cast<int64_t>(value)};
	int64_t value2{static_cast<int64_t>(frames.count())};

	std::string result{sizeof(int64_t) * 2, 0, std::string::allocator_type{}};

	std::copy(reinterpret_cast<char*>(&value1), reinterpret_cast<char*>(&value1) + sizeof(value1), result.begin());
	std::copy(reinterpret_cast<char*>(&value2), reinterpret_cast<char*>(&value2) + sizeof(value2), result.begin() + sizeof(value1));

	return result;
}

std::pair<int, Milliframes> PersistentCache::_decode_value(const std::string & data) const {
	int64_t value1{0};
	int64_t value2{0};

	std::copy(data.begin(), data.begin() + sizeof(value1), reinterpret_cast<char*>(&value1));
	std::copy(data.begin() + sizeof(value1), data.begin() + sizeof(value1) + sizeof(value2), reinterpret_cast<char*>(&value2));

	return std::make_pair(static_cast<int>(value1), Milliframes{value2});
}
