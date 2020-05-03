#ifndef ROSA_DURATION_HH
#define ROSA_DURATION_HH

#include <chrono>

const uint64_t SNES_SECONDS_PER_FRAME_NUMERATOR{655171};
const uint64_t SNES_SECONDS_PER_FRAME_DENOMINATOR{39375000};

const uint64_t SNES_SECONDS_PER_MILLIFRAME_NUMERATOR{SNES_SECONDS_PER_FRAME_NUMERATOR};
const uint64_t SNES_SECONDS_PER_MILLIFRAME_DENOMINATOR{SNES_SECONDS_PER_FRAME_DENOMINATOR * 1000};

using Seconds = std::chrono::duration<double>;
using Frames = std::chrono::duration<int64_t, std::ratio<SNES_SECONDS_PER_FRAME_NUMERATOR, SNES_SECONDS_PER_FRAME_DENOMINATOR>>;
using Milliframes = std::chrono::duration<int64_t, std::ratio<SNES_SECONDS_PER_MILLIFRAME_NUMERATOR, SNES_SECONDS_PER_MILLIFRAME_DENOMINATOR>>;

constexpr auto operator ""_s(long double s) -> Seconds {
	return Seconds(s);
}

// NOLINTNEXTLINE(google-runtime-int)
constexpr auto operator ""_f(unsigned long long f) -> Frames {
	return Frames(f);
}

// NOLINTNEXTLINE(google-runtime-int)
constexpr auto operator ""_mf(unsigned long long mf) -> Milliframes {
	return Milliframes(mf);
}

struct Duration {
	Milliframes average;
	Milliframes minimum;
};

#endif // ROSA_DURATION_HH
