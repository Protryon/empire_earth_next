//! The teams' C ABI (rust/sim/app.h): the world's teams (were `EEWorld::mTeams`, a `std::vector<EETeam>`; Rust owns
//! them since round 12 and is their only store since game core round 16), the stances read through the world's
//! callbacks.

use std::ffi::c_void;
use std::sync::Mutex;

use super::teams::{self, Teams};

static TEAMS: Mutex<Teams> = Mutex::new(Vec::new());

fn with_teams<R>(f: impl FnOnce(&mut Teams) -> R) -> R {
    f(&mut TEAMS.lock().unwrap_or_else(|e| e.into_inner()))
}

/// The team count (`mTeams.size()`).
pub fn count() -> usize {
    with_teams(|t| t.len())
}

/// Team `team`'s players (none past the end).
pub fn players(team: usize) -> Vec<u32> {
    with_teams(|t| t.get(team).cloned().unwrap_or_default())
}

/// The teams, copied.
pub fn all() -> Teams {
    with_teams(|t| t.clone())
}

/// `mTeams[team].ContainsPlayer(player)` (false past the end).
#[no_mangle]
pub extern "C" fn ee_app_teams_contains(team: usize, player: u32) -> bool {
    with_teams(|t| t.get(team).is_some_and(|p| p.contains(&player)))
}

/// The world's players as the teams read them.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeAppTeamsWorld {
    pub ctx: *mut c_void,
    /// `mPlayers[i] != NULL`
    pub present: unsafe extern "C" fn(*mut c_void, u32) -> bool,
    /// `mPlayers[i]->mObserverMode`
    pub observer: unsafe extern "C" fn(*mut c_void, u32) -> bool,
    /// `mPlayers[a]->mEnemies[b] != 0`
    pub enemy: unsafe extern "C" fn(*mut c_void, u32, u32) -> bool,
}

/// The callbacks as closures over the caller's world.
///
/// # Safety
/// `w` points to live callbacks for the call's duration.
unsafe fn calls(w: *const EeAppTeamsWorld) -> (impl Fn(u32) -> bool, impl Fn(u32) -> bool, impl Fn(u32, u32) -> bool) {
    // SAFETY: the caller's contract
    let w = unsafe { *w };
    // SAFETY (each): the C++'s callbacks over its own world
    (
        move |i| unsafe { (w.present)(w.ctx, i) },
        move |i| unsafe { (w.observer)(w.ctx, i) },
        move |a, b| unsafe { (w.enemy)(w.ctx, a, b) },
    )
}

/// `EEWorld::AddTeam` of `n` players.
///
/// # Safety
/// `players` holds `n` ids; `w` as [`calls`].
#[no_mangle]
pub unsafe extern "C" fn ee_app_teams_add(players: *const u32, n: usize, w: *const EeAppTeamsWorld) {
    // SAFETY: the caller's contract
    let (players, (present, _, enemy)) =
        unsafe { (if n == 0 { &[][..] } else { std::slice::from_raw_parts(players, n) }, calls(w)) };
    with_teams(|t| teams::add_team(t, players, &present, &enemy));
}

/// `EEWorld::AddAlliance`.
///
/// # Safety
/// As [`calls`].
#[no_mangle]
pub unsafe extern "C" fn ee_app_teams_ally(a: u32, b: u32, w: *const EeAppTeamsWorld) {
    // SAFETY: the caller's contract
    let (_, _, enemy) = unsafe { calls(w) };
    with_teams(|t| teams::add_alliance(t, a, b, &enemy));
}

/// `EEWorld::BreakAlliance`.
///
/// # Safety
/// As [`calls`].
#[no_mangle]
pub unsafe extern "C" fn ee_app_teams_break(a: u32, b: u32, w: *const EeAppTeamsWorld) {
    // SAFETY: the caller's contract
    let (present, _, enemy) = unsafe { calls(w) };
    with_teams(|t| teams::break_alliance(t, a, b, &present, &enemy));
}

/// `EEWorld::RemovePlayerFromTeams`.
///
/// # Safety
/// As [`calls`].
#[no_mangle]
pub unsafe extern "C" fn ee_app_teams_remove(player: u32, w: *const EeAppTeamsWorld) {
    // SAFETY: the caller's contract
    let (present, _, enemy) = unsafe { calls(w) };
    with_teams(|t| teams::remove_player(t, player, &present, &enemy));
}

/// `EEWorld::RebuildTeams`' teams over `count` players.
///
/// # Safety
/// As [`calls`].
#[no_mangle]
pub unsafe extern "C" fn ee_app_teams_rebuild(count: u32, w: *const EeAppTeamsWorld) {
    // SAFETY: the caller's contract
    let (_, observer, enemy) = unsafe { calls(w) };
    let rebuilt = teams::rebuild(count, &observer, &enemy);
    with_teams(|t| *t = rebuilt);
}

/// The teams emptied (a new game, a world cleared).
#[no_mangle]
pub extern "C" fn ee_app_teams_clear() {
    with_teams(|t| t.clear());
}

/// Team `team` (`team` = its count: a new team at the end) set to `n` players (a load's teams, in order).
///
/// # Safety
/// `players` holds `n` ids.
#[no_mangle]
pub unsafe extern "C" fn ee_app_teams_set(team: usize, players: *const u32, n: usize) {
    // SAFETY: the caller's contract
    let players = if n == 0 { Vec::new() } else { unsafe { std::slice::from_raw_parts(players, n) }.to_vec() };
    with_teams(|t| {
        if team >= t.len() {
            t.resize(team + 1, Vec::new());
        }
        t[team] = players;
    });
}

/// The team count (`team` = -1 as usize) or team `team`'s players into `out` (at most `cap`) -> the length.
///
/// # Safety
/// `out` holds `cap` ids (NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_app_teams_get(team: usize, out: *mut u32, cap: usize) -> usize {
    with_teams(|t| {
        if team == usize::MAX {
            return t.len();
        }
        let Some(players) = t.get(team) else { return 0 };
        for (k, p) in players.iter().take(cap).enumerate() {
            // SAFETY: k < cap
            unsafe { *out.add(k) = *p };
        }
        players.len()
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_store_answers_count_players_and_membership() {
        ee_app_teams_clear();
        // SAFETY: local buffers
        unsafe {
            ee_app_teams_set(0, [1u32, 3].as_ptr(), 2);
            ee_app_teams_set(1, [2u32].as_ptr(), 1);
        }
        assert_eq!(count(), 2);
        assert_eq!(players(0), vec![1, 3]);
        assert!(players(5).is_empty());
        assert!(ee_app_teams_contains(0, 3) && !ee_app_teams_contains(1, 3) && !ee_app_teams_contains(9, 1));
        assert_eq!(all(), vec![vec![1, 3], vec![2]]);
        ee_app_teams_clear();
        assert_eq!(count(), 0);
    }
}
