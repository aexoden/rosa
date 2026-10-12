//! Time units.
//!
//! All solver arithmetic is done in integer milliframes (1/1000 of an SNES
//! frame) so that results are exact and reproducible.

use std::fmt;
use std::iter::Sum;
use std::ops::{Add, AddAssign, Sub};

use serde::{Deserialize, Serialize};

/// Length of one SNES frame in seconds, as a fraction.
const SECONDS_PER_FRAME_NUMERATOR: i64 = 655_171;
const SECONDS_PER_FRAME_DENOMINATOR: i64 = 39_375_000;

pub const MILLIFRAMES_PER_FRAME: i64 = 1000;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize)]
#[serde(transparent)]
pub struct Milliframes(pub i64);

impl Milliframes {
	pub const ZERO: Self = Self(0);

	#[must_use]
	pub const fn from_frames(frames: i64) -> Self {
		Self(frames * MILLIFRAMES_PER_FRAME)
	}

	/// Converts a whole number of seconds, truncating toward zero.
	#[must_use]
	pub const fn from_seconds(seconds: i64) -> Self {
		Self(seconds * SECONDS_PER_FRAME_DENOMINATOR * MILLIFRAMES_PER_FRAME / SECONDS_PER_FRAME_NUMERATOR)
	}

	/// Converts a fractional frame count, truncating toward zero to whole
	/// milliframes. This deliberately goes through `f64` so that values read
	/// from data files round exactly as the C++ version does.
	#[must_use]
	#[expect(clippy::cast_possible_truncation, reason = "matches the C++ conversion")]
	pub fn from_fractional_frames(frames: f64) -> Self {
		Self(i64::from((frames * 1000.0) as i32))
	}

	#[must_use]
	#[expect(clippy::cast_precision_loss, reason = "display only")]
	pub fn as_seconds(self) -> f64 {
		self.0 as f64 * SECONDS_PER_FRAME_NUMERATOR as f64 / (SECONDS_PER_FRAME_DENOMINATOR * MILLIFRAMES_PER_FRAME) as f64
	}
}

impl Add for Milliframes {
	type Output = Self;

	fn add(self, rhs: Self) -> Self {
		Self(self.0 + rhs.0)
	}
}

impl AddAssign for Milliframes {
	fn add_assign(&mut self, rhs: Self) {
		self.0 += rhs.0;
	}
}

impl Sub for Milliframes {
	type Output = Self;

	fn sub(self, rhs: Self) -> Self {
		Self(self.0 - rhs.0)
	}
}

impl Sum for Milliframes {
	fn sum<I: Iterator<Item = Self>>(iter: I) -> Self {
		iter.fold(Self::ZERO, Add::add)
	}
}

impl fmt::Display for Milliframes {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{:.3}s", self.as_seconds())
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn thirty_seconds_matches_cpp_duration_cast() {
		// std::chrono::duration_cast<Milliframes>(30s) in the C++ version.
		assert_eq!(Milliframes::from_seconds(30), Milliframes(1_802_965));
	}

	#[test]
	fn fractional_frames_truncate_toward_zero() {
		assert_eq!(Milliframes::from_fractional_frames(375.945), Milliframes(375_945));
		assert_eq!(Milliframes::from_fractional_frames(-0.080), Milliframes(-80));
		assert_eq!(Milliframes::from_fractional_frames(6.984), Milliframes(6984));
	}
}
