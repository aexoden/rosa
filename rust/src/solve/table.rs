//! An open-addressing hash table for the solver cache.
//!
//! The standard table keeps a power of two of slots and doubles when 7/8
//! full, so on average it sits a little over 60% full. This one keeps any
//! number of slots, fills to 90% and, once large, grows by a quarter, so it
//! averages about 80% full. A slot holds the entry and one byte giving its distance
//! from its home slot; a key that no state can have marks an empty slot.
//!
//! Collisions are resolved by linear probing with Robin Hood ordering:
//! entries further from their home slot displace nearer ones, which keeps
//! probe sequences short and lets a lookup stop early on a miss.

use super::{Key, Packed};

/// No state has this key: its operation index would be `0xFFFF`, and
/// routes are limited to fewer operations than that.
const EMPTY: Key = Key(u64::MAX, u64::MAX);

/// Fill limit, as a fraction of slots, before the table grows.
const MAX_LOAD_NUMERATOR: usize = 9;
const MAX_LOAD_DENOMINATOR: usize = 10;

/// Growth per resize, as a fraction of the current size.
const GROWTH_NUMERATOR: usize = 5;
const GROWTH_DENOMINATOR: usize = 4;

/// Below this many slots a table doubles instead. Small growth steps save
/// memory but mean more resizes, which costs more time than the memory is
/// worth while the cache is small: 256 tables of this size hold about
/// 190 MB.
const DOUBLING_SLOTS: usize = 1 << 15;

const MIN_SLOTS: usize = 16;

/// Stored distances saturate here; a slot at this value has its distance
/// worked out from its key's hash instead.
const FAR: u8 = u8::MAX;

#[derive(Clone, Copy)]
struct Slot {
	key: Key,
	value: Packed,
	distance: u8,
}

impl Slot {
	fn key(self) -> Key {
		self.key
	}

	/// Reads a slot from its stored bytes. Keys are stored inverted, so
	/// that the empty key is stored as zeros and a new table can come
	/// straight from zeroed memory.
	fn load(bytes: &Bytes) -> Self {
		let word = |range: std::ops::Range<usize>| !u64::from_le_bytes(bytes[range].try_into().expect("eight bytes"));

		Self {
			key: Key(word(0..8), word(8..16)),
			value: Packed {
				frames: u32::from_le_bytes(bytes[16..20].try_into().expect("four bytes")),
				value: u16::from_le_bytes(bytes[20..22].try_into().expect("two bytes")),
			},
			distance: bytes[22],
		}
	}

	fn save(self, bytes: &mut Bytes) {
		let Key(high, low) = self.key;
		let Packed { frames, value } = self.value;
		bytes[0..8].copy_from_slice(&(!high).to_le_bytes());
		bytes[8..16].copy_from_slice(&(!low).to_le_bytes());
		bytes[16..20].copy_from_slice(&frames.to_le_bytes());
		bytes[20..22].copy_from_slice(&value.to_le_bytes());
		bytes[22] = self.distance;
	}
}

type Bytes = [u8; SLOT_BYTES];

pub(super) const SLOT_BYTES: usize = 23;

fn empty_slots(count: usize) -> Vec<Bytes> {
	vec![[0; SLOT_BYTES]; count]
}

#[derive(Default)]
pub(super) struct Table {
	slots: Vec<Bytes>,
	len: usize,
}

impl Table {
	pub(super) fn len(&self) -> usize {
		self.len
	}

	/// Slots allocated, used or not.
	pub(super) fn slots(&self) -> usize {
		self.slots.len()
	}

	/// The slot a key would sit in with no collisions. It rises with the
	/// hash, which [`Self::refill`] relies on.
	#[expect(clippy::cast_possible_truncation, reason = "the result is below the slot count")]
	fn home(&self, hash: u64) -> usize {
		((u128::from(hash) * self.slots.len() as u128) >> 64) as usize
	}

	/// How far the entry in slot `index` sits past its home slot.
	fn distance(&self, index: usize) -> usize {
		let slot = Slot::load(&self.slots[index]);

		if slot.distance < FAR {
			return usize::from(slot.distance);
		}

		let home = self.home(slot.key().hash_value());

		if index >= home {
			index - home
		} else {
			index + self.slots.len() - home
		}
	}

	fn next(&self, index: usize) -> usize {
		if index + 1 == self.slots.len() { 0 } else { index + 1 }
	}

	fn store(&mut self, index: usize, mut slot: Slot, distance: usize) {
		slot.distance = u8::try_from(distance).unwrap_or(FAR);
		slot.save(&mut self.slots[index]);
	}

	pub(super) fn get(&self, key: Key, hash: u64) -> Option<Packed> {
		if self.slots.is_empty() {
			return None;
		}

		let home = self.home(hash);

		// Probe to the end of the table, then on from the start.
		for (distance, bytes) in self.slots[home..].iter().chain(&self.slots[..home]).enumerate() {
			let slot = Slot::load(bytes);

			if slot.key == key {
				return Some(slot.value);
			}

			// Empty slots and entries nearer their home than this key would
			// be both mean the key is absent: a Robin Hood table would have
			// placed it here.
			let nearer = if slot.distance < FAR {
				usize::from(slot.distance) < distance
			} else {
				self.distance((home + distance) % self.slots.len()) < distance
			};

			if slot.key == EMPTY || nearer {
				return None;
			}
		}

		None
	}

	pub(super) fn insert(&mut self, key: Key, hash: u64, value: Packed) {
		if (self.len + 1) * MAX_LOAD_DENOMINATOR > self.slots.len() * MAX_LOAD_NUMERATOR {
			let slots = self.slots.len();
			let grown = if slots < DOUBLING_SLOTS {
				slots * 2
			} else {
				slots * GROWTH_NUMERATOR / GROWTH_DENOMINATOR
			};
			self.resize(grown.max(MIN_SLOTS));
		}

		let slot = Slot {
			key,
			value,
			distance: 0,
		};

		if self.place(slot, hash) {
			self.len += 1;
		}
	}

	/// Puts an entry in the table, replacing any with the same key. Returns
	/// whether the table gained an entry.
	fn place(&mut self, mut entry: Slot, hash: u64) -> bool {
		let mut index = self.home(hash);
		let mut distance = 0;

		loop {
			let slot = Slot::load(&self.slots[index]);

			if slot.key() == EMPTY {
				self.store(index, entry, distance);
				return true;
			}

			if slot.key() == entry.key() {
				self.store(index, entry, distance);
				return false;
			}

			let resident = self.distance(index);

			if resident < distance {
				// Take the slot and carry the nearer entry on. Keys are
				// unique, so it cannot match anything further along.
				self.store(index, entry, distance);
				entry = slot;
				distance = resident;
			}

			index = self.next(index);
			distance += 1;
		}
	}

	fn resize(&mut self, slots: usize) {
		let old = std::mem::replace(&mut self.slots, empty_slots(slots));
		self.len = 0;
		self.refill(&old, |_| true);
	}

	/// Moves the entries of `old` that `keep` accepts into the (empty)
	/// table.
	///
	/// Home slots rise with the hash, so a Robin Hood table holds its
	/// entries in order of home slot, starting after any empty slot. Taking
	/// them in that order, each entry goes in its new home slot or the slot
	/// after the previous entry, whichever is later, and the table fills in
	/// one pass from front to back. Entries that wrap past the end of the
	/// table, and any slot already taken, fall back to an ordinary insert.
	fn refill(&mut self, old: &[Bytes], mut keep: impl FnMut(Key) -> bool) {
		let Some(start) = old.iter().position(|bytes| Slot::load(bytes).key() == EMPTY) else {
			return;
		};

		let size = self.slots.len();
		let mut cursor = 0;
		let mut previous_home = 0;
		let mut wrapped = false;

		for offset in 1..=old.len() {
			let slot = Slot::load(&old[(start + offset) % old.len()]);

			if slot.key() == EMPTY || !keep(slot.key()) {
				continue;
			}

			self.len += 1;
			let hash = slot.key().hash_value();
			let home = self.home(hash);

			if home < previous_home {
				wrapped = true;
			}

			previous_home = home;

			let index = home.max(cursor);

			if wrapped || index >= size || Slot::load(&self.slots[index]).key() != EMPTY {
				wrapped = true;
				self.place(slot, hash);
			} else {
				self.store(index, slot, index - home);
				cursor = index + 1;
			}
		}
	}

	/// Keeps only the entries for which `keep` returns true.
	pub(super) fn retain(&mut self, keep: impl FnMut(Key) -> bool) {
		let kept = self.len;
		let slots = (kept * MAX_LOAD_DENOMINATOR / MAX_LOAD_NUMERATOR + 1).max(MIN_SLOTS);
		let old = std::mem::replace(&mut self.slots, empty_slots(slots));
		self.len = 0;
		self.refill(&old, keep);
	}
}

#[cfg(test)]
mod tests {
	use std::collections::HashMap;

	use super::*;

	fn key(n: u64) -> Key {
		// Spread the keys like real states: low operation indexes, the rest
		// varied.
		Key(
			(n % 300) << 48 | n.wrapping_mul(0x9E37_79B9_7F4A_7C15) >> 16,
			n.rotate_left(17),
		)
	}

	fn packed(n: u64) -> Packed {
		Packed {
			frames: u32::try_from(n * 7).unwrap(),
			value: u16::try_from(n % 1000).unwrap(),
		}
	}

	#[test]
	fn matches_a_standard_map() {
		let mut table = Table::default();
		let mut reference = HashMap::new();

		for n in 0..200_000 {
			let key = key(n);
			table.insert(key, key.hash_value(), packed(n));
			reference.insert(n, packed(n).frames);
		}

		// Replacing keeps the count.
		for n in 0..1000 {
			let key = key(n);
			table.insert(key, key.hash_value(), packed(n + 1));
			reference.insert(n, packed(n + 1).frames);
		}

		assert_eq!(table.len(), reference.len());

		for n in 0..400_000 {
			let key = key(n);
			let found = table.get(key, key.hash_value()).map(|value| value.frames);
			assert_eq!(found, reference.get(&n).copied(), "key {n}");
		}

		table.retain(|key| key.0 >> 48 > 150);
		reference.retain(|&n, _| n % 300 > 150);
		assert_eq!(table.len(), reference.len());

		for n in 0..200_000 {
			let key = key(n);
			let found = table.get(key, key.hash_value()).map(|value| value.frames);
			assert_eq!(found, reference.get(&n).copied(), "key {n} after retain");
		}
	}
}
