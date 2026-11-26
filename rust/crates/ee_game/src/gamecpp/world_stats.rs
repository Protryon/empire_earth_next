//! EEWorld::RecordStatistics (game core round 10): the post-game history's fields, the players as ee_sim app
//! statistics.rs reads them, its scores, top-ups, order and play times, then the records filled. The C++ calls and
//! members are EEWorld.cpp's EECore10OpE3 cases (p: the world); the history and its records are stats_history.rs's.

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::app::statistics_ffi::{
    ee_app_record_statistics, EeAppStatsHost, EeAppStatsPlayer, EeAppStatsRecord, EeAppStatsWorld,
};

use ee_sim::units::player_stats::{
    EeUnitsPlayerStats, EeUnitsStatsRecordTail, EE_UNITS_STATS_EDITOR, EE_UNITS_STATS_LOADING, EE_UNITS_STATS_SHUTDOWN,
};
use ee_sim::world::player_fields::EeUnitsPlayerSaved;

use crate::app::game_options::{EeAppOptionsFields, EeAppOptionsPlayerFields};

use super::preferences::get_text;
use super::stats_history::{self, StatsRecord};
use super::{global_objects as gobj, h, hd, hp, pa, ptr, rec, Owned, O, P};

/// The record's name suffix (the C++'s `sprintf(" #%ld")`; none for 0).
pub fn world_stats_suffix(suffix: c_long) -> String {
    if suffix != 0 {
        format!(" #{suffix}")
    } else {
        String::new()
    }
}

// the statistics' C++ calls (statistics.rs's host): the physics random draw, a player's statistics Set and resources
unsafe extern "C" fn random(_: *mut c_void, low: f32, high: f32) -> f32 {
    hd(O::PhysRandomRange, 0, &[], &[low, high])
}

unsafe extern "C" fn set(_: *mut c_void, slot: u32, series: c_long, value: c_long) {
    h(O::WsStatsSet, 0, &[slot as i64, series as i64, value as i64]);
}

unsafe extern "C" fn cap(_: *mut c_void, slot: u32, resource: u32) -> c_long {
    h(O::WsStatsCap, 0, &[slot as i64, resource as i64]) as c_long
}

unsafe extern "C" fn set_cap(_: *mut c_void, slot: u32, resource: u32, value: c_long) {
    h(O::WsStatsSetCap, 0, &[slot as i64, resource as i64, value as i64]);
}

/// `EEPlayerStatistics::Record`'s gate: a shutdown, a saved game loading, the scenario editor outside the game
/// screen.
fn record_flags(w: P) -> u32 {
    if ee_base::util::system::ee_util_shutdown_in_progress() {
        EE_UNITS_STATS_SHUTDOWN
    } else if unsafe { *rec::<bool>(O::WLoadingSavedGame, w) } {
        EE_UNITS_STATS_LOADING
    } else if h(O::EoInScenarioEditor, 0, &[]) != 0 && !unsafe { *rec::<bool>(O::WGameScreenActive, w) } {
        EE_UNITS_STATS_EDITOR
    } else {
        0
    }
}

/// The record of the player in the world's slot `slot` (`mPlayers[slot]`), `o` its place's scores and flags.
fn record_of(w: P, o: &EeAppStatsRecord, history_end_time: c_long) -> StatsRecord {
    let pl = hp(O::WPlayer, w, &[o.slot as i64]);
    let statistics = hp(O::EwPlayerStatistics, pl, &[]);
    // SAFETY: the player's saved numbers and colour, copied; its live series
    let (saved, color, series) = unsafe {
        (
            *rec::<EeUnitsPlayerSaved>(O::PlSaved, pl),
            [hd(O::EgColorRed, pl, &[], &[]), hd(O::EgColorGreen, pl, &[], &[]), hd(O::EgColorBlue, pl, &[], &[])],
            (*rec::<EeUnitsPlayerStats>(O::EwStatsSeries, statistics)).clone(),
        )
    };
    let statistics_end_time = h(O::EwStatsEndTime, statistics, &[]) as c_ulong;
    let id = h(O::RoPlayerId, pl, &[]) as c_ulong;
    let options = hp(O::WOptions, w, &[]);
    let mut name = get_text(hp(O::PlName, pl, &[]), true);
    name.extend_from_slice(world_stats_suffix(o.suffix).as_bytes());
    let mut resources = [0; 7];
    for r in 2..7 {
        resources[r] = saved.mResourceCaps[r] as c_ulong;
    }
    StatsRecord {
        statistics: series,
        statistics_end_time,
        name,
        name_suffix: Vec::new(),
        tail: EeUnitsStatsRecordTail {
            color,
            result: saved.mGameResult as c_long,
            // SAFETY: the options' slot of the player, its plain members
            team: if id > 0 {
                unsafe { (*(gobj::opt_slot(options, id as u32 - 1) as *const EeAppOptionsPlayerFields)).mTeam }
            } else {
                0
            },
            end_time: if statistics_end_time != 0 { statistics_end_time } else { history_end_time as c_ulong },
            resources,
            score: o.score as c_ulong,
            economy_score: o.economy_score as c_ulong,
            epoch_score: o.epoch_score as c_ulong,
            military_score: o.military_score as c_ulong,
            tech_score: o.tech_score as c_ulong,
            slot: super::message::prts(pl).mSlot,
            is_local: o.is_local,
            is_human: o.is_human,
        },
    }
}

/// EEWorld::RecordStatistics on `world`.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_record_statistics(world: P) {
    let w = world;
    let options = hp(O::WOptions, w, &[]);
    // SAFETY: the options' plain members, copied
    let fields = unsafe { *(options as *const EeAppOptionsFields) };
    let campaign = h(O::EpWideLength, options + gobj::OPT_CAMPAIGN_NAME, &[]) > 0 && fields.mCampaignChecksum > 0;
    let scenario = fields.mIsScenarioGame || campaign;
    let end_time = h(O::DwGameTime, 0, &[]) as c_long;
    // SAFETY (each): the world's plain members
    let (start_time, victory_enabled, teams_locked) = unsafe {
        (*rec::<c_long>(O::WStartTime, w), *rec::<bool>(O::WVictoryEnabled, w), *rec::<bool>(O::WTeamsLocked, w))
    };
    let tile_count = h(O::WsTiles, w, &[]) as c_long;
    let campaign_name = if campaign { Some(get_text(options + gobj::OPT_CAMPAIGN_NAME, true)) } else { None };
    stats_history::with(|s| {
        s.tail.start_time = start_time;
        s.tail.tile_count = tile_count;
        s.tail.victory_enabled = victory_enabled;
        s.tail.teams_locked = teams_locked;
        s.tail.is_scenario = scenario;
        s.tail.is_campaign = campaign;
        s.tail.end_time = end_time;
        if let Some(name) = campaign_name {
            s.campaign_name = name;
        }
    });
    let default_name = Owned::new(O::StrNew, O::StrFree);
    h(O::StrComputerName, default_name.0, &[]);
    let count = h(O::WsNumPlayers, w, &[]) as usize;
    let mut players: Vec<EeAppStatsPlayer> = (0..count)
        .map(|_| EeAppStatsPlayer {
            id: 0,
            human: false,
            default_name: false,
            observer: false,
            disconnected: false,
            result: 0,
            stats: std::ptr::null_mut(),
        })
        .collect();
    for (i, player) in players.iter_mut().enumerate() {
        h(O::WsPlayer, w, &[i as i64, pa(default_name.0), ptr(player)]);
    }
    let mut stats_world = EeAppStatsWorld {
        current: h(O::WsCurrent, w, &[]) as c_long,
        is_scenario: scenario,
        end_time,
        ..Default::default()
    };
    for r in 0..5 {
        stats_world.base[r] = h(O::WsBase, w, &[r as i64]) as c_long;
        stats_world.level[r] = h(O::WsLevel, w, &[r as i64]) as c_long;
    }
    let host = EeAppStatsHost { ctx: std::ptr::null_mut(), random, set, cap, set_cap };
    let mut order = vec![EeAppStatsRecord::default(); players.len()];
    let mut current: c_long = -1;
    let mut scenario_after = scenario;
    // SAFETY: the players, the host and the order live through the call; the order holds `order.len()` records
    let n = unsafe {
        ee_app_record_statistics(
            &stats_world,
            players.as_ptr(),
            players.len(),
            &host,
            order.as_mut_ptr(),
            order.len(),
            &mut current,
            &mut scenario_after,
        )
    };
    let mut records: Vec<StatsRecord> = order.iter().take(n).map(|o| record_of(w, o, end_time)).collect();
    let (time, flags) = (h(O::DwGameTime, 0, &[]) as c_ulong, record_flags(w));
    for (record, o) in records.iter_mut().zip(&order) {
        if o.has_play_time {
            record.statistics.set(0x18, o.play_time as c_ulong, time, flags);
        }
    }
    stats_history::with(|s| {
        s.records = records;
        if current >= 0 {
            s.tail.current_record_index = current;
        }
        s.tail.is_scenario = scenario_after;
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_suffix_is_a_number_after_a_hash_or_nothing() {
        assert_eq!(world_stats_suffix(0), "");
        assert_eq!(world_stats_suffix(2), " #2");
        assert_eq!(world_stats_suffix(-1), " #-1");
    }
}
