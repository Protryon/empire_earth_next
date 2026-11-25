//! WSTA, the statistics sweep (game core lane, round 11, lane B; was EEAppCaptureWorld.cpp's
//! `EEAppCaptureStatisticsSweepImpl`), TEST ONLY (tools/sim_capture.py's app profile, after EEMap's sight sweep):
//! EEWorld::RecordStatistics, the end-of-game scores (EEWorld::Clear runs it when a game is left: no capture game
//! reaches it), then one record: the history's fields (but the start time, a wall clock), every record (scores,
//! colour, name and suffix, slot, result, end time, team, resources, flags, statistics), every player's resources
//! and statistics after it (the top-ups of the computers' gathered totals) and the physics random seed. The
//! history and its records are stats_history.rs's; the players' C++ members are one-statement cases of
//! EEGameLog.cpp's `EECore11OpB` ([`super::EeGameCpp`]'s `Ew` ops); the statistics' series are ee_sim units
//! player_stats.rs's.
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_ulong;

use ee_sim::app::capture_record::{self, Record};
use ee_sim::units::player_stats::{EeUnitsPlayerStats, EeUnitsStatsHistoryTail};
use ee_sim::world::player_fields::EeUnitsPlayerSaved;

use super::{h, hp, rec, O};

/// The series of a player's statistics.
const SERIES: usize = 31;

/// The history's flags: teams locked, victory, scenario, campaign.
pub fn history_flags(h: &EeUnitsStatsHistoryTail) -> u32 {
    u32::from(h.teams_locked)
        | u32::from(h.victory_enabled) << 1
        | u32::from(h.is_scenario) << 2
        | u32::from(h.is_campaign) << 3
}

/// A wide string's bytes, counted (UWideString holds UTF-8 bytes: D6).
fn text(r: &mut Record, t: &[u8]) {
    r.u32(t.len() as u32);
    r.bytes(t);
}

/// A player's statistics: each series' largest and last value, its count of increases and of points, the end time.
fn statistics(r: &mut Record, s: &EeUnitsPlayerStats, end_time: c_ulong) {
    for k in 0..SERIES {
        r.u32(s.max(k) as u32);
        r.u32(s.last(k) as u32);
        r.u32(s.count_increases(k) as u32);
        r.u32(s.series.get(k).map_or(0, |x| x.points.len()) as u32);
    }
    r.u32(end_time as u32);
}

/// WSTA (`EEAppCaptureStatisticsSweep`).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_statistics_sweep() {
    if !capture_record::active() || h(O::EcHasMap, 0, &[]) == 0 {
        return;
    }
    h(O::EwRecordStatistics, 0, &[]);
    let history = super::stats_history::with(|s| s.clone());
    let t = history.tail;
    let mut r = Record::new(u32::from_le_bytes(*b"WSTA"));
    // (not mStartTime: the wall clock at the game's start)
    r.i32(t.current_record_index as i32);
    r.i32(t.end_time as i32);
    r.i32(t.tile_count as i32);
    r.u32(history_flags(&t));
    text(&mut r, &history.campaign_name);
    r.u32(history.records.len() as u32);
    for e in &history.records {
        let m = e.tail;
        for score in [m.score, m.economy_score, m.epoch_score, m.military_score, m.tech_score] {
            r.u32(score as u32);
        }
        for channel in m.color {
            r.bytes(&channel.to_le_bytes());
        }
        text(&mut r, &e.name);
        text(&mut r, &e.name_suffix);
        r.u32(u32::from(m.slot));
        r.i32(m.result as i32);
        r.u32(m.end_time as u32);
        r.u32(m.team as u32);
        for k in 2..7 {
            r.u32(m.resources[k] as u32);
        }
        r.u32(u32::from(m.is_local) | u32::from(m.is_human) << 1);
        statistics(&mut r, &e.statistics, e.statistics_end_time);
    }
    let players = h(O::EwNumPlayers, 0, &[]) as c_ulong;
    r.u32(players as u32);
    for i in 0..players {
        let p = hp(O::EwPlayerAt, 0, &[i as i64]);
        if p == 0 {
            r.u32(0);
            continue;
        }
        r.u32(1);
        // SAFETY: the player's saved numbers, copied
        let caps = unsafe { (*rec::<EeUnitsPlayerSaved>(O::PlSaved, p)).mResourceCaps };
        for k in 2..7 {
            r.i32(caps[k] as i32);
        }
        let s = hp(O::EwPlayerStatistics, p, &[]);
        // SAFETY: the player's live series
        statistics(&mut r, unsafe { &*rec::<EeUnitsPlayerStats>(O::EwStatsSeries, s) }, h(O::EwStatsEndTime, s, &[]) as c_ulong);
    }
    r.u32(h(O::EwPhysicsSeed, 0, &[]) as c_ulong as u32);
    r.commit();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_history_flags() {
        let mut t = EeUnitsStatsHistoryTail::default();
        assert_eq!(history_flags(&t), 0);
        t.teams_locked = true;
        t.is_campaign = true;
        assert_eq!(history_flags(&t), 9);
        t.victory_enabled = true;
        t.is_scenario = true;
        assert_eq!(history_flags(&t), 15);
    }
}
