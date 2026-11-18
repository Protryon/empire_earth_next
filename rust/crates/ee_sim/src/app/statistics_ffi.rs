//! `RecordStatistics`' C ABI (rust/sim/app.h), called by EEWorld.cpp.

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use super::statistics::{record_statistics, StatsHost, StatsPlayer, StatsWorld};
use crate::units::player_stats::{
    ee_units_stats_compact, ee_units_stats_points, ee_units_stats_value, EeUnitsPlayerStats, EeUnitsStatPoint,
};

/// A player of the world (`mPlayers[slot]`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAppStatsPlayer {
    pub id: c_ulong,
    pub human: bool,
    /// a computer named as the resource's "Computer"
    pub default_name: bool,
    pub observer: bool,
    pub disconnected: bool,
    pub result: c_ulong,
    /// `mSavedGroups.mStats`
    pub stats: *mut EeUnitsPlayerStats,
}

/// The world's part (see `StatsWorld`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAppStatsWorld {
    pub current: c_long,
    pub is_scenario: bool,
    pub end_time: c_long,
    pub base: [c_long; 5],
    pub level: [c_long; 5],
}

/// A record in the history's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeAppStatsRecord {
    pub slot: u32,
    pub score: c_ulong,
    pub economy_score: c_ulong,
    pub epoch_score: c_ulong,
    pub military_score: c_ulong,
    pub tech_score: c_ulong,
    /// the name's suffix number (" #n"); 0 none
    pub suffix: c_long,
    pub is_local: bool,
    pub is_human: bool,
    /// the record's play time (series 0x18) set after its copy
    pub has_play_time: bool,
    pub play_time: c_long,
}

/// The C++ calls: the physics random draw, `mSavedGroups.Set`, `mResourceCaps` read and written.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeAppStatsHost {
    pub ctx: *mut c_void,
    pub random: unsafe extern "C" fn(*mut c_void, f32, f32) -> f32,
    pub set: unsafe extern "C" fn(*mut c_void, u32, c_long, c_long),
    pub cap: unsafe extern "C" fn(*mut c_void, u32, u32) -> c_long,
    pub set_cap: unsafe extern "C" fn(*mut c_void, u32, u32, c_long),
}

struct Host<'a> {
    host: EeAppStatsHost,
    players: &'a [EeAppStatsPlayer],
}

// SAFETY (every call): the C++'s live callbacks and the players' live statistics objects for the call's duration
impl StatsHost for Host<'_> {
    fn random(&mut self, lo: f32, hi: f32) -> f32 {
        unsafe { (self.host.random)(self.host.ctx, lo, hi) }
    }
    fn value(&self, slot: usize, what: u32, series: c_ulong) -> c_ulong {
        unsafe { ee_units_stats_value(self.players[slot].stats, series, what) }
    }
    fn points(&self, slot: usize, series: c_ulong) -> Vec<(c_ulong, c_ulong)> {
        let s = self.players[slot].stats;
        let n = unsafe { ee_units_stats_value(s, series, 3) } as usize;
        let mut out = vec![EeUnitsStatPoint { value: 0, time: 0 }; n];
        if n > 0 {
            unsafe { ee_units_stats_points(s, series, out.as_mut_ptr(), n) };
        }
        out.iter().map(|p| (p.time, p.value)).collect()
    }
    fn compact(&mut self, slot: usize) {
        unsafe { ee_units_stats_compact(self.players[slot].stats) }
    }
    fn set(&mut self, slot: usize, series: c_long, value: c_long) {
        unsafe { (self.host.set)(self.host.ctx, slot as u32, series, value) }
    }
    fn cap(&self, slot: usize, r: usize) -> c_long {
        unsafe { (self.host.cap)(self.host.ctx, slot as u32, r as u32) }
    }
    fn set_cap(&mut self, slot: usize, r: usize, value: c_long) {
        unsafe { (self.host.set_cap)(self.host.ctx, slot as u32, r as u32, value) }
    }
}

/// `EEWorld::RecordStatistics`' decisions over the world's players: the records in their order into `out` (at most
/// `cap`) -> their count; `current_index` the local record's (-1: kept), `is_scenario` the history's flag after.
///
/// # Safety
/// `world`, `players` (`count` of them, each with a live statistics object), `host` and the out values are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_app_record_statistics(
    world: *const EeAppStatsWorld,
    players: *const EeAppStatsPlayer,
    count: usize,
    host: *const EeAppStatsHost,
    out: *mut EeAppStatsRecord,
    cap: usize,
    current_index: *mut c_long,
    is_scenario: *mut bool,
) -> usize {
    // SAFETY: the caller's contract
    let (w, players, host) = unsafe { (*world, std::slice::from_raw_parts(players, count), *host) };
    let world = StatsWorld {
        players: players
            .iter()
            .map(|p| StatsPlayer {
                id: p.id,
                human: p.human,
                default_name: p.default_name,
                observer: p.observer,
                result: p.result,
                disconnected: p.disconnected,
            })
            .collect(),
        current: w.current,
        is_scenario: w.is_scenario,
        end_time: w.end_time,
        base: w.base,
        level: w.level,
    };
    let r = record_statistics(&mut Host { host, players }, &world);
    // SAFETY: the caller's contract
    unsafe {
        *current_index = r.current_index.map_or(-1, |i| i as c_long);
        *is_scenario = r.is_scenario;
    }
    for (k, rec) in r.records.iter().take(cap).enumerate() {
        // SAFETY: k < cap
        unsafe {
            *out.add(k) = EeAppStatsRecord {
                slot: rec.slot as u32,
                score: rec.score,
                economy_score: rec.economy_score,
                epoch_score: rec.epoch_score,
                military_score: rec.military_score,
                tech_score: rec.tech_score,
                suffix: rec.suffix.unwrap_or(0),
                is_local: rec.is_local,
                is_human: rec.is_human,
                has_play_time: rec.play_time.is_some(),
                play_time: rec.play_time.unwrap_or(0),
            }
        };
    }
    r.records.len().min(cap)
}
