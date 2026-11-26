//! EEWorld's team methods (were EEWorld.cpp's AddTeam, AddAlliance, RebuildTeams, RemovePlayerFromTeams,
//! BreakAlliance and the teams' world callbacks; game core lane, round 12, fork D): the teams are ee_sim app
//! teams.rs's. Since round 16 they are the only store: the world's, the game state's and the saved world's
//! `std::vector<EETeam>`s (EETeam, its EEVCWonder victory condition) are gone; the state's and the saved worlds'
//! teams are kept here, the victory condition is civilization.rs's by team index.
#![allow(clippy::missing_safety_doc)]
#![allow(clippy::unnecessary_cast)] // c_ulong is u32 on Windows, u64 on LP64

use std::ffi::{c_ulong, c_void};
use std::sync::Mutex;

use ee_sim::app::teams::Teams;
use ee_sim::app::teams_ffi::{self as teams, EeAppTeamsWorld};

use super::persistent::{Pf, PF_OK};
use super::{h, O, P};

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

/// EEWorld::AddTeam: a team of the players, first merging in every team whose players are allied both ways with them.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_add(_w: *mut c_void, players: *const c_ulong, n: usize) {
    let players: Vec<u32> = if n == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(players, n) }.iter().map(|p| *p as u32).collect()
    };
    unsafe { teams::ee_app_teams_add(players.as_ptr(), players.len(), &world()) };
}

/// EEWorld::AddAlliance: two players on one team, merging in every team allied both ways with both.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_ally(_w: *mut c_void, a: u32, b: u32) {
    unsafe { teams::ee_app_teams_ally(a, b, &world()) };
}

/// EEWorld::BreakAlliance.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_break(_w: *mut c_void, a: u32, b: u32) {
    unsafe { teams::ee_app_teams_break(a, b, &world()) };
}

/// EEWorld::RemovePlayerFromTeams.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_remove(_w: *mut c_void, player: u32) {
    unsafe { teams::ee_app_teams_remove(player, &world()) };
}

/// EEWorld::RebuildTeams: the teams from the players' mutual alliances, then Gaia at war with every player.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_world_teams_rebuild(w: *mut c_void, players: u32) {
    unsafe { teams::ee_app_teams_rebuild(players, &world()) };
    super::world::ee_gamecpp_world_gaia_stances(w);
}

// ---- the game state's teams (were `EEGameState::mTeams`: a loaded game's, handed to the world by StartGame) and the
// saved worlds' (were `EEPWorldHelper::mTeams`, by helper: a save copies the world's, a load reads them)

/// `gGameState.mTeams` (one game state).
static STATE: Mutex<Teams> = Mutex::new(Vec::new());
/// The saved-world helpers' teams (by address; the last [`HELPERS_KEPT`]: a save or a load lives for one call).
static HELPERS: Mutex<Vec<(usize, Teams)>> = Mutex::new(Vec::new());

const HELPERS_KEPT: usize = 8;

fn state<R>(f: impl FnOnce(&mut Teams) -> R) -> R {
    f(&mut STATE.lock().unwrap_or_else(|e| e.into_inner()))
}

/// The helper's teams (none yet: empty).
fn helper<R>(helper: P, f: impl FnOnce(&mut Teams) -> R) -> R {
    let mut all = HELPERS.lock().unwrap_or_else(|e| e.into_inner());
    let k = match all.iter().position(|(h, _)| *h == helper) {
        Some(k) => k,
        None => {
            if all.len() == HELPERS_KEPT {
                all.remove(0);
            }
            all.push((helper, Vec::new()));
            all.len() - 1
        }
    };
    f(&mut all[k].1)
}

/// `EEGameState::Init`'s `mTeams.clear()`.
pub fn state_clear() {
    state(|t| t.clear());
}

/// StartGame's teams of the state, appended to the world's in order -> their count.
pub fn state_to_world() -> usize {
    let teams = state(|t| t.clone());
    for players in &teams {
        // SAFETY: the count (a new team at the end), then the team's players
        unsafe { teams::ee_app_teams_set(teams::count(), players.as_ptr(), players.len()) };
    }
    teams.len()
}

/// A new saved-world helper's (both constructors): no teams.
pub fn helper_new(h: P) {
    helper(h, |t| t.clear());
}

/// `mTeams = inWorld.mTeams` (the save's helper).
pub fn helper_from_world(h: P) {
    let world = teams::all();
    helper(h, |t| *t = world);
}

/// Restore's `ioState.mTeams = mTeams`.
pub fn helper_to_state(h: P) {
    let loaded = helper(h, |t| t.clone());
    state(|t| *t = loaded);
}

/// LoadTeams: the team count and that many teams (`EETeam::Load`: its player count, the players, then the victory
/// condition's old wonder list); the first read error (a failed count resizes nothing; the teams after a failure
/// stay empty, a failed team keeps the players read so far, the rest 0).
pub fn helper_load(h: P, major: c_ulong, minor: c_ulong, file: Pf) -> i64 {
    let mut count: c_ulong = 0;
    let mut r = file.read_ulong(&mut count);
    if r != PF_OK {
        return r;
    }
    let mut loaded: Teams = vec![Vec::new(); count as usize];
    for players in loaded.iter_mut() {
        if r != PF_OK {
            break;
        }
        r = load_team(players, major, minor, file);
    }
    helper(h, |t| *t = loaded);
    r
}

fn load_team(players: &mut Vec<u32>, major: c_ulong, minor: c_ulong, file: Pf) -> i64 {
    let mut n: c_ulong = 0;
    let r = file.read_ulong(&mut n);
    if r != PF_OK {
        return r;
    }
    players.resize(n as usize, 0);
    for p in players.iter_mut() {
        let mut v = *p as c_ulong;
        let r = file.read_ulong(&mut v);
        *p = v as u32;
        if r != PF_OK {
            return r;
        }
    }
    crate::civilization::wonder_load(major, minor, file)
}

/// SaveTeams: the team count and each team (`EETeam::Save`: its count and players); the first write error.
pub fn helper_save(h: P, file: Pf) -> i64 {
    let saved = helper(h, |t| t.clone());
    let mut r = file.write_ulong(saved.len() as c_ulong);
    for players in &saved {
        if r != PF_OK {
            break;
        }
        r = file.write_ulong(players.len() as c_ulong);
        for &p in players {
            if r == PF_OK {
                r = file.write_ulong(p as c_ulong);
            }
        }
    }
    r
}

// ---- the main game form's team reads (were EEUIFMainGame.cpp's shims: screens main_form_calls.rs)

/// `gWorld.mTeams[team].mVictoryCondition->GetWonderCountdown(*value)`.
///
/// # Safety
/// `value` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_screenscpp_team_wonder_countdown(team: u32, value: *mut c_ulong) -> usize {
    crate::civilization::ee_gamecpp_wonder_countdown(team as usize, value) as usize
}

/// `gWorld.GetNumTeams()`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_mf_num_teams() -> i64 {
    teams::count() as i64
}

/// `gWorld.mTeams.size() > 0`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_mf_has_teams() -> bool {
    teams::count() > 0
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

    #[test]
    fn the_helpers_keep_their_own_teams_and_hand_them_to_the_state() {
        teams::ee_app_teams_clear();
        // SAFETY: a local buffer
        unsafe { teams::ee_app_teams_set(0, [1u32, 2].as_ptr(), 2) };
        let (save, load) = (0x1000, 0x2000);
        helper_from_world(save);
        helper_new(load);
        helper(load, |t| *t = vec![vec![3], vec![4, 5]]);
        assert_eq!(helper(save, |t| t.clone()), vec![vec![1, 2]]);
        helper_to_state(load);
        teams::ee_app_teams_clear();
        assert_eq!(state_to_world(), 2);
        assert_eq!(teams::all(), vec![vec![3], vec![4, 5]]);
        state_clear();
        assert_eq!(state_to_world(), 0);
        // a reused address starts empty; old helpers are dropped past the kept count
        helper_new(save);
        assert!(helper(save, |t| t.is_empty()));
        for k in 0..HELPERS_KEPT + 2 {
            helper_new(0x9000 + k);
        }
        assert_eq!(HELPERS.lock().unwrap().len(), HELPERS_KEPT);
        teams::ee_app_teams_clear();
    }
}
