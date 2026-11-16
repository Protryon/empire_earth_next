//! A player's statistics (`EEPlayerStatistics`, EEPlayerStatistics.cpp 0x56d346..0x56dadc): the 31 series of
//! (value, time) points, each with its largest value and whether it keeps its history, as one Rust object the C++
//! class holds (`mStats`; its name and end time stay C++ members). Every body is here: the point pushed by `Set`
//! (the C++ passes its gate and the game time), `Reset`/`Lower` (raise or lower the last value), the readers
//! (`GetValue`, `GetLastValue`, `CountIncreases`, a series' points for `GetSeries`), `Clear`, the copy, the epoch
//! series' compaction and the saved game's Load and Save over the player save's primitives.
//!
//! Kept: `Set` replaces the last point unless the series keeps its history (12..16 after a clear), the maximum
//! compared unsigned, a value pushed as the C++ `long`'s bits; `Lower` only when the last value is at least the
//! amount (unsigned); the load's history flag read and dropped, a count past 31 read into a scratch series (the
//! original wrote past its array), its points resized before they are read (a failed read leaves zero points).
//! Defined where the original read past a vector: the last value of a series with no point (a load of zero points)
//! is 0.
//!
//! Determinism: integer arithmetic wrapping as the C++ `unsigned long`'s, no float, no random draw.

#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::player_save::{
    EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_ULONG,
    EE_UNITS_PLAYER_SAVE_WRITE_BOOL, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};

/// The series of a player's statistics.
pub const EE_UNITS_STATS_SERIES: usize = 31;
/// The series that keep their history after a clear.
const KEPT: [usize; 5] = [12, 13, 14, 15, 16];
/// The series the scenario editor still records (outside the game screen).
const EDITOR: [usize; 6] = [12, 13, 14, 15, 16, 19];

/// `Set`'s gate (the C++ reads the world): bit 0 a shutdown in progress, bit 1 a saved game loading, bit 2 the
/// scenario editor without the game screen.
pub const EE_UNITS_STATS_SHUTDOWN: u32 = 1;
pub const EE_UNITS_STATS_LOADING: u32 = 2;
pub const EE_UNITS_STATS_EDITOR: u32 = 4;

/// One point (`EEStatisticsPoint`: the value, the game time).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsStatPoint {
    pub value: c_ulong,
    pub time: c_ulong,
}

/// One series (`EEStatisticsSeries`).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Series {
    pub points: Vec<EeUnitsStatPoint>,
    /// `mMaxValue`
    pub max: c_ulong,
    /// `mKeepHistory`
    pub keep: bool,
}

impl Default for Series {
    /// `Reset`: one zero point, no maximum, no history.
    fn default() -> Self {
        Series { points: vec![EeUnitsStatPoint::default()], max: 0, keep: false }
    }
}

/// The 31 series (the C++ `mStats`).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct EeUnitsPlayerStats {
    pub series: Vec<Series>,
}

impl Default for EeUnitsPlayerStats {
    fn default() -> Self {
        let mut s = EeUnitsPlayerStats { series: Vec::new() };
        s.clear();
        s
    }
}

impl EeUnitsPlayerStats {
    /// `Clear`'s series: each reset, 12..16 keeping their history.
    pub fn clear(&mut self) {
        self.series = vec![Series::default(); EE_UNITS_STATS_SERIES];
        for s in KEPT {
            self.series[s].keep = true;
        }
    }

    /// `Set(index, value)` at game time `time` under the gate `flags`.
    pub fn set(&mut self, index: usize, value: c_ulong, time: c_ulong, flags: u32) {
        if flags & EE_UNITS_STATS_SHUTDOWN != 0 {
            return;
        }
        let record = flags & EE_UNITS_STATS_EDITOR == 0 || EDITOR.contains(&index);
        if flags & EE_UNITS_STATS_LOADING != 0 || !record {
            return;
        }
        let Some(s) = self.series.get_mut(index) else { return };
        if !s.keep {
            s.points.pop();
        }
        if value > s.max {
            s.max = value;
        }
        s.points.push(EeUnitsStatPoint { value, time });
    }

    /// `GetLastValue`: the last point's value, 0 without one.
    pub fn last(&self, index: usize) -> c_ulong {
        self.series.get(index).and_then(|s| s.points.last()).map_or(0, |p| p.value)
    }

    /// `Reset(series, amount)`: the last value raised by `amount`.
    pub fn raise(&mut self, index: usize, amount: c_ulong, time: c_ulong, flags: u32) {
        let value = self.last(index).wrapping_add(amount);
        self.set(index, value, time, flags);
    }

    /// `Lower(series, amount)`: the last value lowered by `amount` unless it is smaller.
    pub fn lower(&mut self, index: usize, amount: c_ulong, time: c_ulong, flags: u32) {
        let value = self.last(index);
        if value >= amount {
            self.set(index, value - amount, time, flags);
        }
    }

    /// `GetValue`: the series' maximum.
    pub fn max(&self, index: usize) -> c_ulong {
        self.series.get(index).map_or(0, |s| s.max)
    }

    /// `CountIncreases`: the points whose next point is larger.
    pub fn count_increases(&self, index: usize) -> c_ulong {
        let Some(s) = self.series.get(index) else { return 0 };
        s.points.windows(2).filter(|w| w[0].value < w[1].value).count() as c_ulong
    }

    /// `CompactEpochHistory`: the epoch series (12) without its repeats and zeros; a point is kept when it is not 0
    /// and differs from the next one, the last one when it differs from the kept series' last value; a kept point
    /// at time 0 replaces a kept one at time 0 once there are two.
    pub fn compact_epochs(&mut self) {
        let old = &self.series[12];
        if old.points.len() <= 1 {
            return;
        }
        let mut new = Series { points: vec![EeUnitsStatPoint::default()], max: 0, keep: old.keep };
        let n = old.points.len();
        for j in 0..n {
            let point = old.points[j];
            if j == n - 1 {
                if new.points.last().is_some_and(|p| p.value == point.value) {
                    continue;
                }
            } else {
                let next = old.points[j + 1];
                if point.value == 0 || next.value == point.value {
                    continue;
                }
            }
            if new.points.len() > 1 && new.points.last().is_some_and(|p| p.time == point.time && p.time == 0) {
                new.points.pop();
            }
            new.points.push(point);
            if point.value > new.max {
                new.max = point.value;
            }
        }
        self.series[12] = new;
    }
}

/// `Load` after the name: the end time, the count, each series' maximum, flag (dropped) and points.
pub fn load<F: FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32>(
    stats: &mut EeUnitsPlayerStats,
    end_time: &mut c_ulong,
    mut prim: F,
) -> i32 {
    let read = |value: &mut c_ulong, prim: &mut F| {
        let mut io = EeUnitsPlayerSaveIo { i: *value as i64, ..Default::default() };
        let r = prim(EE_UNITS_PLAYER_SAVE_READ_ULONG, &mut io);
        *value = io.i as c_ulong;
        r
    };
    let mut result = read(end_time, &mut prim);
    let mut count: c_ulong = 0;
    if result == 0 {
        result = read(&mut count, &mut prim);
    }
    let mut points: c_ulong = 0;
    let mut scratch = Series::default();
    let mut i: c_ulong = 0;
    while i < count && result == 0 {
        let s = stats.series.get_mut(i as usize).unwrap_or(&mut scratch);
        result = read(&mut s.max, &mut prim);
        if result == 0 {
            result = prim(EE_UNITS_PLAYER_SAVE_READ_BOOL, &mut EeUnitsPlayerSaveIo::default());
        }
        if result == 0 {
            result = read(&mut points, &mut prim);
        }
        if result == 0 {
            s.points.resize(points as usize, EeUnitsStatPoint::default());
        }
        let mut p: c_ulong = 0;
        while p < points && result == 0 {
            let s = stats.series.get_mut(i as usize).unwrap_or(&mut scratch);
            let mut point = s.points.get(p as usize).copied().unwrap_or_default();
            result = read(&mut point.value, &mut prim);
            if result == 0 {
                result = read(&mut point.time, &mut prim);
            }
            if let Some(slot) = s.points.get_mut(p as usize) {
                *slot = point;
            }
            p += 1;
        }
        i += 1;
    }
    result
}

/// `Save` after the name: the end time, 31, each series' maximum, flag and points.
pub fn save<F: FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32>(
    stats: &EeUnitsPlayerStats,
    end_time: c_ulong,
    mut prim: F,
) -> i32 {
    let ulong = |value: c_ulong, prim: &mut F| {
        prim(EE_UNITS_PLAYER_SAVE_WRITE_ULONG, &mut EeUnitsPlayerSaveIo { i: value as i64, ..Default::default() })
    };
    let mut result = ulong(end_time, &mut prim);
    if result == 0 {
        result = ulong(EE_UNITS_STATS_SERIES as c_ulong, &mut prim);
    }
    for s in &stats.series {
        if result != 0 {
            break;
        }
        result = ulong(s.max, &mut prim);
        if result == 0 {
            result = prim(
                EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
                &mut EeUnitsPlayerSaveIo { b: u8::from(s.keep), ..Default::default() },
            );
        }
        if result == 0 {
            result = ulong(s.points.len() as c_ulong, &mut prim);
        }
        for p in &s.points {
            if result != 0 {
                break;
            }
            result = ulong(p.value, &mut prim);
            if result == 0 {
                result = ulong(p.time, &mut prim);
            }
        }
    }
    result
}

/// The C++ callback of the save's primitives (`EESavePrimitive` on the context's file).
fn prim(ctx: *mut c_void, f: EeUnitsPlayerSaveFn) -> impl FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32 {
    // SAFETY: the C++ callback's contract
    move |op, io| unsafe { f(ctx, op, 0, io) }
}

fn index(series: c_long) -> usize {
    usize::try_from(series).unwrap_or(usize::MAX)
}

/// A new player's statistics (cleared).
#[no_mangle]
pub extern "C" fn ee_units_stats_new() -> *mut EeUnitsPlayerStats {
    Box::into_raw(Box::default())
}

/// # Safety
/// `s` came from [`ee_units_stats_new`] (or is null) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_free(s: *mut EeUnitsPlayerStats) {
    if !s.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(s) });
    }
}

/// `operator=`'s series: `dst` becomes a copy of `src`.
///
/// # Safety
/// Both are live statistics.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_copy(dst: *mut EeUnitsPlayerStats, src: *const EeUnitsPlayerStats) {
    if !std::ptr::eq(dst, src) {
        // SAFETY: the caller's contract
        unsafe { (*dst).clone_from(&*src) };
    }
}

/// `Clear`'s series.
///
/// # Safety
/// `s` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_clear(s: *mut EeUnitsPlayerStats) {
    // SAFETY: the caller's contract
    unsafe { (*s).clear() };
}

/// `Set(series, value)` (`op` 0), `Reset(series, value)` (1: raised by it) or `Lower(series, value)` (2) at `time`
/// under `flags` (`EE_UNITS_STATS_*`).
///
/// # Safety
/// `s` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_set(
    s: *mut EeUnitsPlayerStats,
    op: u32,
    series: c_long,
    value: c_long,
    time: c_ulong,
    flags: u32,
) {
    // SAFETY: the caller's contract
    let s = unsafe { &mut *s };
    let i = index(series);
    match op {
        0 => s.set(i, value as c_ulong, time, flags),
        1 => s.raise(i, value as c_ulong, time, flags),
        _ => s.lower(i, value as c_ulong, time, flags),
    }
}

/// A series' number: `what` 0 `GetValue` (its maximum), 1 `GetLastValue`, 2 `CountIncreases`, 3 its points' count,
/// 4 whether it keeps its history.
///
/// # Safety
/// `s` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_value(s: *const EeUnitsPlayerStats, series: c_ulong, what: u32) -> c_ulong {
    // SAFETY: the caller's contract
    let s = unsafe { &*s };
    let i = series as usize;
    match what {
        0 => s.max(i),
        1 => s.last(i),
        2 => s.count_increases(i),
        3 => s.series.get(i).map_or(0, |x| x.points.len() as c_ulong),
        _ => c_ulong::from(s.series.get(i).is_some_and(|x| x.keep)),
    }
}

/// A series' points (`GetSeries`): up to `cap` copied to `out`; the count copied.
///
/// # Safety
/// `s` is live, `out` holds `cap` points.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_points(
    s: *const EeUnitsPlayerStats,
    series: c_ulong,
    out: *mut EeUnitsStatPoint,
    cap: usize,
) -> usize {
    // SAFETY: the caller's contract
    let s = unsafe { &*s };
    let Some(x) = s.series.get(series as usize) else { return 0 };
    let n = x.points.len().min(cap);
    if n > 0 {
        // SAFETY: the caller's contract (out holds cap >= n points)
        unsafe { std::ptr::copy_nonoverlapping(x.points.as_ptr(), out, n) };
    }
    n
}

/// `CompactEpochHistory`.
///
/// # Safety
/// `s` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_compact(s: *mut EeUnitsPlayerStats) {
    // SAFETY: the caller's contract
    unsafe { (*s).compact_epochs() };
}

/// `Load` after the name (see [`load`]) through `f` on `ctx`.
///
/// # Safety
/// `s` and `end_time` are live; `f` handles the primitives with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_load(
    s: *mut EeUnitsPlayerStats,
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    end_time: *mut c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe { load(&mut *s, &mut *end_time, prim(ctx, f)) }
}

/// `Save` after the name (see [`save`]) through `f` on `ctx`.
///
/// # Safety
/// `s` is live; `f` handles the primitives with `ctx`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_stats_save(
    s: *const EeUnitsPlayerStats,
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    end_time: c_ulong,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe { save(&*s, end_time, prim(ctx, f)) }
}

#[cfg(test)]
#[path = "player_stats_tests.rs"]
mod tests;
