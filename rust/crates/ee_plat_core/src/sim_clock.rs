//! The harness clock (`platform/PlatSim.h` `PlatSimSetTimeFactor`): TEST ONLY. With a factor F set, every clock of
//! `platform/PlatTime.h` (`PlatTimeMilliseconds`, `PlatTimeMilliseconds64`, `PlatCounterTicks`) runs F times faster
//! than real time, and every timed wait of the platform layer (`PlatSleep`, the system timers' intervals, the finite
//! timeouts of `PlatEventWait`, `PlatEventWaitAny` and `PlatThreadWait`) lasts 1/F of its real time. The game's own
//! code is unchanged: an accelerated game is the same game on a faster clock (pin the PT rate so the simulation per
//! PT stays the same). This replaces the import hooks of tools/harness/timeshim.c, which patched the exe's WINMM and
//! KERNEL32 import slots: the seam is the platform layer itself, whatever language the callers are in.
//!
//! No factor (the default, and always in EE-AOC.exe without the test fixture): the clocks are the real ones and this
//! module adds nothing to them. A factor set later rebases the clocks, so they never jump: each clock goes on from the
//! value it had at the change. Wall-clock time (`PlatGetLocalTime`, `PlatTimeWallClock100ns`) is never scaled.

use std::sync::atomic::{AtomicPtr, Ordering};

/// The largest factor (as the time shim's); the smallest is 1/1000.
pub const MAX_FACTOR: u32 = 4096;

/// A factor and the point it was set at: real counter ticks `real0`, and the virtual clocks' values there.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Scale {
    pub num: u32,
    pub den: u32,
    /// the real performance counter when the factor was set, and its frequency
    pub real0: i64,
    pub frequency: i64,
    /// the virtual clocks' values at `real0`
    pub counter0: i64,
    pub milliseconds0: u32,
    pub milliseconds64_0: u64,
}

impl Scale {
    /// Virtual counter ticks since the factor was set.
    pub fn elapsed(&self, real_now: i64) -> i64 {
        let real = (real_now - self.real0).max(0) as i128;
        (real * self.num as i128 / self.den as i128) as i64
    }

    fn elapsed_milliseconds(&self, real_now: i64) -> u64 {
        if self.frequency <= 0 {
            return 0;
        }
        (self.elapsed(real_now) as i128 * 1000 / self.frequency as i128) as u64
    }

    pub fn counter(&self, real_now: i64) -> i64 {
        self.counter0 + self.elapsed(real_now)
    }

    pub fn milliseconds(&self, real_now: i64) -> u32 {
        self.milliseconds0.wrapping_add(self.elapsed_milliseconds(real_now) as u32)
    }

    pub fn milliseconds64(&self, real_now: i64) -> u64 {
        self.milliseconds64_0 + self.elapsed_milliseconds(real_now)
    }

    /// The real duration of `virtual_ms` virtual milliseconds, in nanoseconds (rounded up, at least 1).
    pub fn real_nanoseconds(&self, virtual_ms: u32) -> u64 {
        let ns = virtual_ms as u128 * 1_000_000 * self.den as u128;
        (ns.div_ceil(self.num as u128) as u64).max(1)
    }
}

/// A factor's numerator and denominator when valid: both nonzero, 1/1000 <= num/den <= MAX_FACTOR.
pub fn valid_factor(num: u32, den: u32) -> bool {
    num != 0 && den != 0 && num as u64 <= MAX_FACTOR as u64 * den as u64 && den as u64 <= 1000 * num as u64
}

/// The factor parsed from text: "F", "N/D" or a decimal ("1.5") -> (num, den), reduced.
pub fn parse_factor(text: &str) -> Option<(u32, u32)> {
    let text = text.trim();
    let (num, den) = if let Some((n, d)) = text.split_once('/') {
        (n.trim().parse::<u64>().ok()?, d.trim().parse::<u64>().ok()?)
    } else if let Some((whole, fraction)) = text.split_once('.') {
        if fraction.len() > 3 || !fraction.bytes().all(|b| b.is_ascii_digit()) {
            return None;
        }
        let scale = 10u64.pow(fraction.len() as u32);
        let whole = if whole.is_empty() { 0 } else { whole.parse::<u64>().ok()? };
        let fraction = if fraction.is_empty() { 0 } else { fraction.parse::<u64>().ok()? };
        (whole.checked_mul(scale)?.checked_add(fraction)?, scale)
    } else {
        (text.parse::<u64>().ok()?, 1)
    };
    if num == 0 || den == 0 {
        return None;
    }
    let (mut a, mut b) = (num, den);
    while b != 0 {
        (a, b) = (b, a % b);
    }
    let (num, den) = (u32::try_from(num / a).ok()?, u32::try_from(den / a).ok()?);
    valid_factor(num, den).then_some((num, den))
}

/// The scale in force; null: none (real time). Each set leaks one `Scale`: a test sets the factor once or a few
/// times, and readers on other threads may still hold the previous one.
static CURRENT: AtomicPtr<Scale> = AtomicPtr::new(std::ptr::null_mut());

/// The scale in force, None for real time.
pub fn scale() -> Option<&'static Scale> {
    // SAFETY: only `set` stores here, a leaked box that is never freed.
    unsafe { CURRENT.load(Ordering::Acquire).as_ref() }
}

/// The real duration of a finite, nonzero timeout of `virtual_ms` under the scale in force, in nanoseconds; None
/// when there is no scale (wait as always) or the timeout is 0 or infinite (`u32::MAX`), which no factor changes.
pub fn real_timeout_nanoseconds(virtual_ms: u32) -> Option<u64> {
    if virtual_ms == 0 || virtual_ms == u32::MAX {
        return None;
    }
    scale().map(|s| s.real_nanoseconds(virtual_ms))
}

/// Sets the factor from the current real and virtual clocks -> false (and nothing changes) for an invalid factor.
/// `real` is (real counter ticks now, counter frequency); `now` the virtual clocks' values at that point.
pub(crate) fn set(num: u32, den: u32, real: (i64, i64), now: (i64, u32, u64)) -> bool {
    if !valid_factor(num, den) {
        return false;
    }
    let scale = Scale {
        num,
        den,
        real0: real.0,
        frequency: real.1,
        counter0: now.0,
        milliseconds0: now.1,
        milliseconds64_0: now.2,
    };
    CURRENT.store(Box::into_raw(Box::new(scale)), Ordering::Release);
    true
}

/// Back to real time (the tests only: a game never goes back, its clocks would jump).
#[cfg(test)]
pub(crate) fn reset() {
    CURRENT.store(std::ptr::null_mut(), Ordering::Release);
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scale(num: u32, den: u32) -> Scale {
        Scale {
            num,
            den,
            real0: 1_000,
            frequency: 10_000_000,
            counter0: 5_000,
            milliseconds0: u32::MAX - 5,
            milliseconds64_0: 70,
        }
    }

    #[test]
    fn clocks_run_factor_times_faster_from_their_values_at_the_set() {
        let s = scale(32, 1);
        assert_eq!(s.counter(1_000), 5_000);
        assert_eq!(s.milliseconds(1_000), u32::MAX - 5);
        // 10 real ms = 100,000 ticks -> 320 virtual ms
        assert_eq!(s.counter(101_000), 5_000 + 3_200_000);
        assert_eq!(s.milliseconds64(101_000), 70 + 320);
        assert_eq!(s.milliseconds(101_000), 314, "wraps as timeGetTime does");
        // before the set (a racing reader): no negative time
        assert_eq!(s.counter(0), 5_000);
    }

    #[test]
    fn fractional_factors_and_real_durations() {
        let s = scale(3, 2);
        assert_eq!(s.elapsed(1_000 + 20), 30);
        assert_eq!(s.real_nanoseconds(3), 2_000_000);
        assert_eq!(scale(32, 1).real_nanoseconds(1), 31_250);
        assert_eq!(scale(3, 1).real_nanoseconds(1), 333_334, "rounded up");
        assert_eq!(scale(4096, 1).real_nanoseconds(0), 1, "at least 1");
    }

    #[test]
    fn factors_are_checked_and_parsed() {
        assert!(valid_factor(1, 1) && valid_factor(4096, 1) && valid_factor(1, 1000));
        assert!(!valid_factor(0, 1) && !valid_factor(1, 0) && !valid_factor(4097, 1) && !valid_factor(1, 1001));
        assert_eq!(parse_factor("32"), Some((32, 1)));
        assert_eq!(parse_factor(" 6/4 "), Some((3, 2)));
        assert_eq!(parse_factor("1.5"), Some((3, 2)));
        assert_eq!(parse_factor("0.25"), Some((1, 4)));
        assert_eq!(parse_factor("2."), Some((2, 1)));
        for bad in ["", "0", "x", "1/0", "5000", "1.2345", "-2", "1/2000"] {
            assert_eq!(parse_factor(bad), None, "{bad}");
        }
    }

    #[test]
    fn timeouts_scale_only_when_finite_and_nonzero() {
        let _guard = crate::test_globals();
        reset();
        assert_eq!(real_timeout_nanoseconds(100), None);
        assert!(!set(0, 1, (0, 1), (0, 0, 0)));
        assert!(super::scale().is_none(), "an invalid factor changes nothing");
        assert!(set(10, 1, (0, 1_000), (0, 0, 0)));
        assert_eq!(real_timeout_nanoseconds(100), Some(10_000_000));
        assert_eq!(real_timeout_nanoseconds(0), None);
        assert_eq!(real_timeout_nanoseconds(u32::MAX), None);
        reset();
    }
}
