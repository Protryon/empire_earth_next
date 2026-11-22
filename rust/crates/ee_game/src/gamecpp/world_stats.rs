//! EEWorld::RecordStatistics (game core round 10): the post-game history's fields, the players as ee_sim app
//! statistics.rs reads them, its scores, top-ups, order and play times, then the records filled. The C++ calls and
//! members are EEWorld.cpp's EECore10OpE3 cases (p: the world).

use std::ffi::{c_long, c_ulong, c_void, CString};

use ee_sim::app::statistics_ffi::{
    ee_app_record_statistics, EeAppStatsHost, EeAppStatsPlayer, EeAppStatsRecord, EeAppStatsWorld,
};

use super::{h, hd, pa, ptr, Owned, O, P};

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
    hd(O::WsRandom, 0, &[], &[low, high])
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

/// EEWorld::RecordStatistics on `world`.
#[no_mangle]
pub extern "C" fn ee_gamecpp_world_record_statistics(world: P) {
    let w = world;
    h(O::WsStart, w, &[]);
    h(O::WsTiles, w, &[]);
    h(O::WsVictory, w, &[]);
    h(O::WsTeamsLocked, w, &[]);
    let campaign = h(O::WsCampaignOptions, w, &[]) != 0;
    let scenario = h(O::WsScenarioGame, w, &[]) != 0 || campaign;
    h(O::WsSetScenario, w, &[scenario as i64]);
    h(O::WsSetCampaign, w, &[campaign as i64]);
    let end_time = h(O::WsEndTime, w, &[]) as c_long;
    if campaign {
        h(O::WsCampaignName, w, &[]);
    }
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
    h(O::WsRecordsAssign, w, &[n as i64]);
    for (k, o) in order.iter().take(n).enumerate() {
        let (k, slot) = (k as i64, o.slot as i64);
        let suffix = CString::new(world_stats_suffix(o.suffix)).unwrap_or_default();
        h(O::WsRecScore, w, &[k, o.score as c_ulong as i64]);
        h(O::WsRecEconomy, w, &[k, o.economy_score as c_ulong as i64]);
        h(O::WsRecEpoch, w, &[k, o.epoch_score as c_ulong as i64]);
        h(O::WsRecMilitary, w, &[k, o.military_score as c_ulong as i64]);
        h(O::WsRecTech, w, &[k, o.tech_score as c_ulong as i64]);
        h(O::WsRecColor, w, &[k, slot]);
        h(O::WsRecName, w, &[k, slot, pa(suffix.as_ptr() as P)]);
        h(O::WsRecStatistics, w, &[k, slot]);
        h(O::WsRecSlot, w, &[k, slot]);
        h(O::WsRecResult, w, &[k, slot]);
        h(O::WsRecEndTime, w, &[k, slot]);
        h(O::WsRecTeam, w, &[k, slot]);
        for r in 2..7 {
            h(O::WsRecResource, w, &[k, slot, r]);
        }
        h(O::WsRecLocal, w, &[k, o.is_local as i64]);
        h(O::WsRecHuman, w, &[k, o.is_human as i64]);
    }
    if current >= 0 {
        h(O::WsSetCurrentRecord, w, &[current as i64]);
    }
    for (k, o) in order.iter().take(n).enumerate() {
        if o.has_play_time {
            h(O::WsRecPlayTime, w, &[k as i64, o.play_time as i64]);
        }
    }
    h(O::WsSetScenario, w, &[scenario_after as i64]);
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
