//! WONCrypt's random generator (`Random.cpp`): an MD5 chain. The state is MD5(previous state || seed); each block
//! of 16 output bytes is MD5(state || counter as 8 little-endian bytes), the counter counting up from 0.
//!
//! The C++ left the counter uninitialised (whatever the memory held); it starts at 0 here. The system seed is
//! 8 bytes of the wall clock, a monotonic clock and a use counter (the Windows build also mixed in the disk's free
//! space): it only needs to differ between calls.

use std::sync::atomic::{AtomicI64, Ordering};
use std::time::{Instant, SystemTime, UNIX_EPOCH};

use crate::md5::Md5;

#[derive(Clone, Debug, Default)]
pub struct Random {
    state: Vec<u8>,
    digest: Md5,
    random_bytes: Vec<u8>,
    used: usize,
    counter: i64,
}

/// The system seed's use counter and the drivers' fixed base (0: the clocks). In the crate's own tests both are per
/// thread, so a replay that fixes the seed draws the same sequence whatever the other tests draw meanwhile.
#[cfg(not(test))]
static USAGE: AtomicI64 = AtomicI64::new(0);
#[cfg(test)]
thread_local! {
    static USAGE: AtomicI64 = const { AtomicI64::new(0) };
    static TEST_SEED: AtomicI64 = const { AtomicI64::new(0) };
}

#[cfg(not(test))]
fn with_usage<R>(f: impl FnOnce(&AtomicI64) -> R) -> R {
    f(&USAGE)
}
#[cfg(test)]
fn with_usage<R>(f: impl FnOnce(&AtomicI64) -> R) -> R {
    USAGE.with(f)
}
#[cfg(not(test))]
fn with_test_seed<R>(f: impl FnOnce(&AtomicI64) -> R) -> R {
    f(&TEST_SEED)
}
#[cfg(test)]
fn with_test_seed<R>(f: impl FnOnce(&AtomicI64) -> R) -> R {
    TEST_SEED.with(f)
}

impl Random {
    /// `Random()`: seeded from the system.
    pub fn new() -> Random {
        Random::from_seed(&system_seed())
    }

    /// `Random(seed)`.
    pub fn from_seed(seed: &[u8]) -> Random {
        let mut r = Random::default();
        r.set_seed(seed);
        r
    }

    /// `setSeed(RawBuffer)`: mixes the seed into the state.
    pub fn set_seed(&mut self, seed: &[u8]) {
        if !self.state.is_empty() {
            self.digest.update(&self.state);
        }
        self.digest.update(seed);
        self.state = self.digest.finish().to_vec();
    }

    /// `setSeed(__int64)`: a zero seed changes nothing.
    pub fn set_seed_i64(&mut self, seed: i64) {
        if seed != 0 {
            self.set_seed(&seed.to_le_bytes());
        }
    }

    /// `nextBytes`: fills the buffer.
    pub fn next_bytes(&mut self, out: &mut [u8]) {
        for b in out.iter_mut() {
            if self.random_bytes.is_empty() || self.used == self.random_bytes.len() {
                self.digest.update(&self.state);
                self.digest.update(&self.counter.to_le_bytes());
                self.counter = self.counter.wrapping_add(1);
                self.random_bytes = self.digest.finish().to_vec();
                self.used = 0;
            }
            *b = self.random_bytes[self.used];
            self.used += 1;
        }
    }

    /// `next(numBits)`: the next `num_bits` (1..=32) bits, big-endian from the bytes.
    pub fn next(&mut self, num_bits: i32) -> u32 {
        let num_bytes = ((num_bits + 7) / 8).max(0) as usize;
        let mut b = vec![0u8; num_bytes];
        self.next_bytes(&mut b);
        let v = b.iter().fold(0u32, |v, &x| v.wrapping_shl(8).wrapping_add(u32::from(x)));
        v.wrapping_shr((num_bytes as i32 * 8 - num_bits) as u32)
    }
}

/// A fixed base for [`system_seed`] (0: the clocks), for the drivers' goldens of ops that make random keys.
#[cfg(not(test))]
static TEST_SEED: AtomicI64 = AtomicI64::new(0);

/// From now on [`system_seed`] gives `seed` plus a use counter, the same sequence in every run (0: the clocks
/// again). For the `tests/won` drivers only: nothing in the game calls it.
#[no_mangle]
pub extern "C" fn ee_won_random_test_seed(seed: i64) {
    with_test_seed(|t| t.store(seed, Ordering::Relaxed));
    with_usage(|u| u.store(0, Ordering::Relaxed));
}

/// The `n`th system seed (from 1) after `ee_won_random_test_seed(fixed)`.
pub fn test_seed(fixed: i64, n: u32) -> [u8; 8] {
    fixed.wrapping_add(i64::from(n)).to_le_bytes()
}

/// `getSeed`: 8 bytes that differ between calls and runs.
pub fn system_seed() -> [u8; 8] {
    let usage = with_usage(|u| u.fetch_add(1, Ordering::Relaxed)) + 1;
    let fixed = with_test_seed(|t| t.load(Ordering::Relaxed));
    if fixed != 0 {
        return test_seed(fixed, usage as u32);
    }
    let wall = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| (d.as_nanos() / 100) as i64).unwrap_or(0);
    thread_local! { static START: Instant = Instant::now(); }
    let ticks = START.with(|s| s.elapsed().as_nanos() as i64);
    let seed = (ticks.rotate_left(17) ^ wall).wrapping_add(usage);
    seed.to_le_bytes()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_chain_is_md5_of_state_and_counter() {
        let mut r = Random::from_seed(b"seed");
        let state = Md5::digest(b"seed");
        let mut first = Vec::from(state);
        first.extend_from_slice(&0i64.to_le_bytes());
        let mut second = Vec::from(state);
        second.extend_from_slice(&1i64.to_le_bytes());
        let mut out = [0u8; 20];
        r.next_bytes(&mut out);
        assert_eq!(&out[..16], &Md5::digest(&first));
        assert_eq!(&out[16..], &Md5::digest(&second)[..4]);
    }

    #[test]
    fn reseeding_chains_the_state() {
        let mut r = Random::from_seed(b"a");
        r.set_seed(b"b");
        let mut joined = Vec::from(Md5::digest(b"a"));
        joined.extend_from_slice(b"b");
        assert_eq!(r.state, Md5::digest(&joined));
        let before = r.state.clone();
        r.set_seed_i64(0);
        assert_eq!(r.state, before);
    }

    #[test]
    fn next_takes_the_top_bits() {
        let mut a = Random::from_seed(b"x");
        let mut b = a.clone();
        let mut bytes = [0u8; 2];
        b.next_bytes(&mut bytes);
        assert_eq!(a.next(12), (u32::from(bytes[0]) << 4) | (u32::from(bytes[1]) >> 4));
        assert!(a.next(1) <= 1);
    }

    #[test]
    fn system_seeds_differ() {
        assert_ne!(system_seed(), system_seed());
    }

    #[test]
    fn a_test_seed_counts_up_from_its_base() {
        assert_eq!(test_seed(77, 1), 78i64.to_le_bytes());
        assert_eq!(test_seed(-1, 2), 1i64.to_le_bytes());
    }
}
