#ifndef SPOONY_DURATION_HH
#define SPOONY_DURATION_HH

#include <chrono>

using Seconds = std::chrono::duration<double>;
using Frames = std::chrono::duration<int64_t, std::ratio<655171, 39375000>>;
using Milliframes = std::chrono::duration<int64_t, std::ratio<655171, 39375000000>>;

constexpr Seconds operator ""_s(long double s) {
	return Seconds(s);
}

constexpr Frames operator ""_f(unsigned long long f) {
	return Frames(f);
}

constexpr Milliframes operator ""_mf(unsigned long long mf) {
	return Milliframes(mf);
}

struct Duration {
	Milliframes average;
	Milliframes minimum;
};

#endif // SPOONY_DURATION_HH
