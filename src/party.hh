#ifndef ROSA_PARTY_HH
#define ROSA_PARTY_HH

#include <cstdint>
#include <string>
#include <vector>

class Party {
	public:
		explicit Party(std::string party);

		std::pair<uint32_t, uint64_t> get_keys() const;

		bool operator==(const Party & other) const {
			return _key1 == other._key1 && _key2 == other._key2;
		}

		friend std::ostream & operator<<(std::ostream & os, const Party & party);

	private:
		bool _three_front{true};
		bool _has_gp{true};
		bool _on_world_map{true};

		int _level{1};

		uint32_t _key1{0};
		uint64_t _key2{0};

		std::string _party;

		std::vector<std::tuple<int, int>> _characters;
};

namespace std {
	template <>
	struct hash<Party> {
		size_t operator()(const Party & party) const {
			const auto [key1, key2] = party.get_keys();
			return hash<uint32_t>()(key1) * 31 + hash<uint64_t>()(key2);
		}
	};
} // namespace std

#endif // ROSA_PARTY_HH
