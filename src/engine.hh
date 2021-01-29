#ifndef ROSA_ENGINE_HH
#define ROSA_ENGINE_HH

#include "cache.hh"
#include "duration.hh"
#include "encounter.hh"
#include "instruction.hh"
#include "map.hh"
#include "parameters.hh"
#include "state.hh"

#include <vector>

struct LogEntry {
	const State state;

	int steps{0};
	Milliframes frames{0};

	std::string extra_text{""};

	std::vector<std::tuple<int, int, int, Milliframes>> encounters{};
};

using Log = std::vector<LogEntry>;

class Engine {
	public:
		explicit Engine(Parameters parameters);

		void set_variable_minimum(int variable, int value);
		void set_variable_maximum(int variable, int value);

		auto optimize(int seed) -> std::string;

	private:
		auto _optimize(const State & state) -> Milliframes;
		auto _finalize(State state) -> Log;
		auto _generate_output_text(const State & state, const Log & log) -> std::string;

		auto _cycle(State * state, LogEntry * log, int value) -> Milliframes;
		auto _step(State * state, LogEntry * log, int tiles, int steps) -> Milliframes;

		static auto _check_search_complete(State * state, const peg::Ast & expression) -> bool;
		static auto _assign_search_encounter(State * state, std::size_t encounter_id, const peg::Ast & expression) -> bool;

		const Parameters _parameters;

		Variables _variables;

		std::unique_ptr<Cache> _cache;

		std::string _route_title;
		int _route_version{0};

		const std::vector<int> _rng_data{
			0x07, 0xB6, 0xF0, 0x1F, 0x55, 0x5B, 0x37, 0xE3, 0xAE, 0x4F, 0xB2, 0x5E, 0x99, 0xF6, 0x77, 0xCB,
			0x60, 0x8F, 0x43, 0x3E, 0xA7, 0x4C, 0x2D, 0x88, 0xC7, 0x68, 0xD7, 0xD1, 0xC2, 0xF2, 0xC1, 0xDD,
			0xAA, 0x93, 0x16, 0xF7, 0x26, 0x04, 0x36, 0xA1, 0x46, 0x4E, 0x56, 0xBE, 0x6C, 0x6E, 0x80, 0xD5,
			0xB5, 0x8E, 0xA4, 0x9E, 0xE7, 0xCA, 0xCE, 0x21, 0xFF, 0x0F, 0xD4, 0x8C, 0xE6, 0xD3, 0x98, 0x47,
			0xF4, 0x0D, 0x15, 0xED, 0xC4, 0xE4, 0x35, 0x78, 0xBA, 0xDA, 0x27, 0x61, 0xAB, 0xB9, 0xC3, 0x7D,
			0x85, 0xFC, 0x95, 0x6B, 0x30, 0xAD, 0x86, 0x00, 0x8D, 0xCD, 0x7E, 0x9F, 0xE5, 0xEF, 0xDB, 0x59,
			0xEB, 0x05, 0x14, 0xC9, 0x24, 0x2C, 0xA0, 0x3C, 0x44, 0x69, 0x40, 0x71, 0x64, 0x3A, 0x74, 0x7C,
			0x84, 0x13, 0x94, 0x9C, 0x96, 0xAC, 0xB4, 0xBC, 0x03, 0xDE, 0x54, 0xDC, 0xC5, 0xD8, 0x0C, 0xB7,
			0x25, 0x0B, 0x01, 0x1C, 0x23, 0x2B, 0x33, 0x3B, 0x97, 0x1B, 0x62, 0x2F, 0xB0, 0xE0, 0x73, 0xCC,
			0x02, 0x4A, 0xFE, 0x9B, 0xA3, 0x6D, 0x19, 0x38, 0x75, 0xBD, 0x66, 0x87, 0x3F, 0xAF, 0xF3, 0xFB,
			0x83, 0x0A, 0x12, 0x1A, 0x22, 0x53, 0x90, 0xCF, 0x7A, 0x8B, 0x52, 0x5A, 0x49, 0x6A, 0x72, 0x28,
			0x58, 0x8A, 0xBF, 0x0E, 0x06, 0xA2, 0xFD, 0xFA, 0x41, 0x65, 0xD2, 0x4D, 0xE2, 0x5C, 0x1D, 0x45,
			0x1E, 0x09, 0x11, 0xB3, 0x5F, 0x29, 0x79, 0x39, 0x2E, 0x2A, 0x51, 0xD9, 0x5D, 0xA6, 0xEA, 0x31,
			0x81, 0x89, 0x10, 0x67, 0xF5, 0xA9, 0x42, 0x82, 0x70, 0x9D, 0x92, 0x57, 0xE1, 0x3D, 0xF1, 0xF9,
			0xEE, 0x08, 0x91, 0x18, 0x20, 0xB1, 0xA5, 0xBB, 0xC6, 0x48, 0x50, 0x9A, 0xD6, 0x7F, 0x7B, 0xE9,
			0x76, 0xDF, 0x32, 0x6F, 0x34, 0xA8, 0xD0, 0xB8, 0x63, 0xC8, 0xC0, 0xEC, 0x4B, 0xE8, 0x17, 0xF8
		};
};

#endif // ROSA_ENGINE_HH
