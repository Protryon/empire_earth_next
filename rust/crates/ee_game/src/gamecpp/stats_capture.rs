//! WSTA, the statistics sweep (game core lane, round 11, lane B; was EEAppCaptureWorld.cpp's
//! `EEAppCaptureStatisticsSweepImpl`), TEST ONLY (tools/sim_capture.py's app profile, after EEMap's sight sweep):
//! EEWorld::RecordStatistics, the end-of-game scores (EEWorld::Clear runs it when a game is left: no capture game
//! reaches it), then one record: the history's fields (but the start time, a wall clock), every record (scores,
//! colour, name and suffix, slot, result, end time, team, resources, flags, statistics), every player's resources
//! and statistics after it (the top-ups of the computers' gathered totals) and the physics random seed. The C++
//! members are one-statement cases of EEGameLog.cpp's `EECore11OpB` ([`super::EeGameCpp`]'s `Ew` ops); the
//! statistics' series are ee_sim units player_stats.rs's.
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_ulong};

use ee_sim::app::capture_record::{self, Record};
use ee_sim::units::player_stats::{ee_units_stats_value, EeUnitsPlayerStats};
use ee_sim::world::player_fields::EeUnitsPlayerSaved;

use super::preferences::get_text;
use super::{h, hp, rec, O, P};

/// `EEStatisticsHistory`'s members from `mStartTime` on (ee/EEStatisticsHistory.h), in order.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGamecppStatsHistoryTail {
    pub mStartTime: c_long,
    pub mCurrentRecordIndex: c_long,
    pub mEndTime: c_long,
    pub mTileCount: c_long,
    pub mTeamsLocked: bool,
    pub mVictoryEnabled: bool,
    pub mIsScenario: bool,
    pub mIsCampaign: bool,
}

/// `EEStatisticsRecord`'s members from `mResult` to `mIsHuman` (ee/EEStatisticsRecord.h), in order.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeGamecppStatsRecordTail {
    pub mResult: c_long,
    pub mTeam: c_ulong,
    pub mEndTime: c_ulong,
    pub mResources: [c_ulong; 7],
    pub mScore: c_ulong,
    pub mEconomyScore: c_ulong,
    pub mEpochScore: c_ulong,
    pub mMilitaryScore: c_ulong,
    pub mTechScore: c_ulong,
    pub mSlot: u8,
    pub mIsLocal: bool,
    pub mIsHuman: bool,
}

/// The series of a player's statistics.
const SERIES: c_ulong = 31;

/// The history's flags: teams locked, victory, scenario, campaign.
pub fn history_flags(h: &EeGamecppStatsHistoryTail) -> u32 {
    u32::from(h.mTeamsLocked)
        | u32::from(h.mVictoryEnabled) << 1
        | u32::from(h.mIsScenario) << 2
        | u32::from(h.mIsCampaign) << 3
}

/// A wide string's bytes, counted (UWideString holds UTF-8 bytes: D6).
fn text(r: &mut Record, member: P) {
    let t = get_text(member, true);
    r.u32(t.len() as u32);
    r.bytes(&t);
}

/// A player's statistics: each series' largest and last value, its count of increases and of points, the end time.
fn statistics(r: &mut Record, s: P) {
    let series = hp(O::EwStatsSeries, s, &[]) as *const EeUnitsPlayerStats;
    for k in 0..SERIES {
        for what in 0..4 {
            // SAFETY: the statistics' live series
            r.u32(unsafe { ee_units_stats_value(series, k, what) } as u32);
        }
    }
    r.u32(h(O::EwStatsEndTime, s, &[]) as c_ulong as u32);
}

/// WSTA (`EEAppCaptureStatisticsSweep`).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_statistics_sweep() {
    if !capture_record::active() || h(O::EcHasMap, 0, &[]) == 0 {
        return;
    }
    h(O::EwRecordStatistics, 0, &[]);
    let history = hp(O::EwHistory, 0, &[]);
    // SAFETY: the history's members, copied
    let t = unsafe { *rec::<EeGamecppStatsHistoryTail>(O::EwHistoryTail, history) };
    let mut r = Record::new(u32::from_le_bytes(*b"WSTA"));
    // (not mStartTime: the wall clock at the game's start)
    r.i32(t.mCurrentRecordIndex as i32);
    r.i32(t.mEndTime as i32);
    r.i32(t.mTileCount as i32);
    r.u32(history_flags(&t));
    text(&mut r, hp(O::EwCampaignName, history, &[]));
    let records = h(O::EwRecordCount, history, &[]);
    r.u32(records as u32);
    for i in 0..records {
        let e = hp(O::EwRecordAt, history, &[i]);
        // SAFETY: the record's members, copied
        let m = unsafe { *rec::<EeGamecppStatsRecordTail>(O::EwRecordTail, e) };
        for score in [m.mScore, m.mEconomyScore, m.mEpochScore, m.mMilitaryScore, m.mTechScore] {
            r.u32(score as u32);
        }
        let color = hp(O::EwRecordColor, e, &[]) as *const u8;
        // SAFETY: the record's colour, its size's bytes
        r.bytes(unsafe { std::slice::from_raw_parts(color, h(O::EwColorSize, 0, &[]) as usize) });
        text(&mut r, hp(O::EwRecordName, e, &[]));
        text(&mut r, hp(O::EwRecordSuffix, e, &[]));
        r.u32(u32::from(m.mSlot));
        r.i32(m.mResult as i32);
        r.u32(m.mEndTime as u32);
        r.u32(m.mTeam as u32);
        for k in 2..7 {
            r.u32(m.mResources[k] as u32);
        }
        r.u32(u32::from(m.mIsLocal) | u32::from(m.mIsHuman) << 1);
        statistics(&mut r, hp(O::EwRecordStatisticsOf, e, &[]));
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
        statistics(&mut r, hp(O::EwPlayerStatistics, p, &[]));
    }
    r.u32(h(O::EwPhysicsSeed, 0, &[]) as c_ulong as u32);
    r.commit();
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_history_flags() {
        let mut t = EeGamecppStatsHistoryTail::default();
        assert_eq!(history_flags(&t), 0);
        t.mTeamsLocked = true;
        t.mIsCampaign = true;
        assert_eq!(history_flags(&t), 9);
        t.mVictoryEnabled = true;
        t.mIsScenario = true;
        assert_eq!(history_flags(&t), 15);
    }
}
