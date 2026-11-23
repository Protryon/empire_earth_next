//! EEWorld's team methods (were EEWorld.cpp's AddTeam, AddAlliance, RebuildTeams, RemovePlayerFromTeams,
//! BreakAlliance, the teams' world callbacks and EEMirrorTeams; game core lane, round 12, fork D): the teams are
//! ee_sim app teams.rs's; the world's `std::vector<EETeam>` (the victory conditions, the game-over check, the save)
//! mirrors them after every change. `TeamsChanged` (an empty member) is gone.
#![allow(clippy::missing_safety_doc)]

use std::ffi::{c_ulong, c_void};

use ee_sim::app::teams_ffi::{self as teams, EeAppTeamsWorld};

use super::{h, pa, O, P};

/// `mPlayers[i] != NULL` (a player index past the table's 17 is absent).
unsafe extern "C" fn present(_: *mut c_void, player: u32) -> bool {
    player < 17 && h(O::EwPlayerAt, 0, &[i64::from(player)]) != 0
}

unsafe extern "C" fn observer(_: *mut c_void, player: u32) -> bool {
    h(O::C12PlayerObserver, h(O::EwPlayerAt, 0, &[i64::from(player)]) as P, &[]) != 0
}

unsafe extern "C" fn enemy(_: *mut c_void, player: u32, other: u32) -> bool {
    h(O::C12PlayerEnemy, h(O::EwPlayerAt, 0, &[i64::from(player)]) as P, &[i64::from(other)]) != 0
}

/// The world's callbacks of the teams' changes.
fn world() -> EeAppTeamsWorld {
    EeAppTeamsWorld { ctx: std::ptr::null_mut(), present, observer, enemy }
}

/// EEMirrorTeams: the world's teams resized to the Rust teams' count, each team's players copied.
pub fn mirror(w: P) {
    // SAFETY: the count query (no buffer)
    let count = unsafe { teams::ee_app_teams_get(usize::MAX, std::ptr::null_mut(), 0) };
    h(O::C12TeamsResize, w, &[count as i64]);
    for k in 0..count {
        // SAFETY: the size query, then a buffer of that size
        let mut players = vec![0u32; unsafe { teams::ee_app_teams_get(k, std::ptr::null_mut(), 0) }];
        unsafe { teams::ee_app_teams_get(k, players.as_mut_ptr(), players.len()) };
        h(O::C12TeamSetPlayers, w, &[k as i64, pa(players.as_ptr() as P), players.len() as i64]);
    }
}

/// EEWorld::AddTeam: a team of the players, first merging in every team whose players are allied both ways with them.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_add(w: *mut c_void, players: *const c_ulong, n: usize) {
    let players: Vec<u32> = if n == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(players, n) }.iter().map(|p| *p as u32).collect()
    };
    unsafe { teams::ee_app_teams_add(players.as_ptr(), players.len(), &world()) };
    mirror(w as P);
}

/// EEWorld::AddAlliance: two players on one team, merging in every team allied both ways with both.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_ally(w: *mut c_void, a: u32, b: u32) {
    unsafe { teams::ee_app_teams_ally(a, b, &world()) };
    mirror(w as P);
}

/// EEWorld::BreakAlliance.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_break(w: *mut c_void, a: u32, b: u32) {
    unsafe { teams::ee_app_teams_break(a, b, &world()) };
    mirror(w as P);
}

/// EEWorld::RemovePlayerFromTeams.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_remove(w: *mut c_void, player: u32) {
    unsafe { teams::ee_app_teams_remove(player, &world()) };
    mirror(w as P);
}

/// EEWorld::RebuildTeams: the teams from the players' mutual alliances, then Gaia at war with every player.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_rebuild(w: *mut c_void, players: u32) {
    unsafe { teams::ee_app_teams_rebuild(players, &world()) };
    mirror(w as P);
    super::world::ee_gamecpp_world_gaia_stances(w);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_world_callbacks_are_the_ccs() {
        let w = world();
        assert!(w.ctx.is_null());
        // a player past the table's 17 is absent without a host call
        assert!(!unsafe { (w.present)(std::ptr::null_mut(), 17) });
        assert!(!unsafe { (w.present)(std::ptr::null_mut(), 400) });
    }
}
