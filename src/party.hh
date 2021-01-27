#ifndef ROSA_PARTY_HH
#define ROSA_PARTY_HH

#include <cstdint>
#include <string>
#include <vector>

const int HASH_MULTIPLIER = 31;

class Party {
	public:
		explicit Party(std::string party);

		[[nodiscard]] auto get_keys() const -> std::pair<uint16_t, uint64_t>;

		auto operator==(const Party & other) const -> bool {
			return _key1 == other._key1 && _key2 == other._key2;
		}

		friend auto operator<<(std::ostream & os, const Party & party) -> std::ostream &;

	private:
		bool _three_front{true};
		bool _has_gp{true};
		bool _on_world_map{true};

		int _level{1};

		uint16_t _key1{0};
		uint64_t _key2{0};

		std::string _party;

		std::vector<std::tuple<int, int>> _characters;
};

namespace std {
	template <>
	struct hash<Party> {
		auto operator()(const Party & party) const -> size_t {
			const auto [key1, key2] = party.get_keys();
			return hash<uint16_t>()(key1) * HASH_MULTIPLIER + hash<uint64_t>()(key2);
		}
	};
} // namespace std

#endif // ROSA_PARTY_HH
