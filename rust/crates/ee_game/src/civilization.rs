//! A player's civilization and the wonder victory (game core lane, round 10, lane D; was EECivilization.cpp's and
//! EEVCWonder.cpp's bodies): the civilization's defaults, its save and load (save 0.268's chunk, unchanged), its
//! purchases and the order a loaded civilization's upgrades apply in (ee_sim tech civilization.rs's), and the team's
//! wonder count and countdown. Each C++ call or member is a one-statement case of EECivilization.cpp's
//! `EECore10OpD5` or EEVCWonder.cpp's `EECore10OpD6` ([`crate::gamecpp::EeGameCpp`]'s `Dv`, `Dw` ops). Its own
//! module since the player/tech lane's round 1: ee/EECivilization.h, which every includer of ee/EEPlayer.h reads,
//! includes its small header (rust/game/civilization.h), not gamecpp.h.
//!
//! Simulation and save code (RUST_PLAN 2.1): the reads and writes are the C++'s overloads in its order; the points
//! are `unsigned long` and wrap as the C++'s did; the countdown is the C++'s unsigned arithmetic.
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::tech::civilization::{apply_all, reapply, EE_TECH_CIV_DEFERRED_INDEX};
use ee_sim::units::player_lists::{player_op, ListOp, EE_UNITS_LISTS_WONDERS};

use ee_sim::world::player_fields::EeUnitsRtsPlayerFields;

use crate::gamecpp::persistent::Pf;
use crate::gamecpp::{h, hp, pa, rec, Owned, O, P};

/// The upgrade's attribute index bits (ee_sim tech civ.rs `LEVEL_MASK`).
const CIV_LEVEL_MASK: u32 = 0xffff;
/// `kErrorOK`; Reapply's and ApplyDeferred's failure.
const OK: i64 = 0;
const CIV_FAILED: c_long = 0xd;

fn member<'a, T>(op: O, cv: P) -> &'a mut T {
    // SAFETY: the op answers the address of the civilization's member of type T
    unsafe { &mut *rec(op, cv) }
}

fn available<'a>(cv: P) -> &'a mut c_ulong {
    member(O::DvAvailable, cv)
}

fn maximum<'a>(cv: P) -> &'a mut c_ulong {
    member(O::DvMaximum, cv)
}

fn no_points_added<'a>(cv: P) -> &'a mut bool {
    member(O::DvNoPointsAdded, cv)
}

fn unchanged<'a>(cv: P) -> &'a mut bool {
    member(O::DvUnchanged, cv)
}

/// A C++ `std::vector<EECivUpgrade>`'s upgrades, copied.
fn upgrades(list: P) -> Vec<u32> {
    (0..h(O::DvListCount, list, &[])).map(|i| h(O::DvListAt, list, &[i]) as u32).collect()
}

fn purchased(cv: P) -> P {
    hp(O::DvPurchased, cv, &[])
}

/// `gCivilizationManager.ApplyUpgrade(upgrade, player)`.
fn apply(upgrade: u32, player: c_long) -> bool {
    h(O::DvApplyUpgrade, 0, &[upgrade as i64, player as i64]) != 0
}

/// `EECivilization()`: unnamed, no points or upgrades, unchanged.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_init(civilization: *mut c_void) {
    let cv = civilization as P;
    h(O::DvNameEmpty, cv, &[]);
    *available(cv) = 0;
    *maximum(cv) = 0;
    *no_points_added(cv) = true;
    *unchanged(cv) = true;
    h(O::DvDeferredClear, cv, &[]);
}

/// `AddPoints`: to both the available and the maximum points.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_add_points(civilization: *mut c_void, amount: c_long) {
    let cv = civilization as P;
    *available(cv) = available(cv).wrapping_add(amount as c_ulong);
    *maximum(cv) = maximum(cv).wrapping_add(amount as c_ulong);
    *no_points_added(cv) = false;
}

/// `Load`: the purchased upgrades, the points, the flags and the name (before 0.216 a narrow one); the first read
/// error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_load(
    civilization: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
) -> c_long {
    let (cv, f) = (civilization as P, Pf(file as P));
    let mut count: c_ulong = 0;
    let mut r = f.read_ulong(&mut count);
    if r != OK {
        return r as c_long;
    }
    let mut i: c_ulong = 0;
    while i < count && r == OK {
        let mut value: c_ulong = 0;
        r = f.read_ulong(&mut value);
        if r == OK {
            h(O::DvPurchasedPush, cv, &[value as u32 as i64]);
        }
        i += 1;
    }
    if r != OK {
        return r as c_long;
    }
    r = f.read_ulong(available(cv));
    if r != OK {
        return r as c_long;
    }
    r = f.read_ulong(maximum(cv));
    if r != OK {
        return r as c_long;
    }
    r = f.read_bool(no_points_added(cv));
    if r != OK {
        return r as c_long;
    }
    r = f.read_bool(unchanged(cv));
    if r == OK {
        if major == 0 && minor < 0xd8 {
            let old = Owned::new(O::UStrNew, O::UStrFree);
            r = h(O::DvUStrLoad, old.0, &[pa(f.0)]);
            h(O::DvNameNarrow, cv, &[pa(old.0)]);
        } else {
            r = h(O::DvNameLoad, cv, &[pa(f.0)]);
        }
    }
    r as c_long
}

/// `Save`: as `Load`, stopping at the first failed upgrade write; the first write error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_save(civilization: *mut c_void, file: *mut c_void) -> c_long {
    let (cv, f) = (civilization as P, Pf(file as P));
    let list = upgrades(purchased(cv));
    let mut r = f.write_ulong(list.len() as c_ulong);
    if r != OK {
        return r as c_long;
    }
    for &u in &list {
        r = f.write_ulong(u as c_ulong);
        if r != OK {
            return r as c_long;
        }
    }
    r = f.write_ulong(*available(cv));
    if r != OK {
        return r as c_long;
    }
    r = f.write_ulong(*maximum(cv));
    if r != OK {
        return r as c_long;
    }
    r = f.write_bool(*no_points_added(cv));
    if r != OK {
        return r as c_long;
    }
    r = f.write_bool(*unchanged(cv));
    if r == OK {
        r = h(O::DvNameSave, cv, &[pa(f.0)]);
    }
    r as c_long
}

/// A player's index (`mPlayerID`).
fn player_id(player: *mut c_void) -> c_long {
    // SAFETY: a live player's record
    unsafe { (*rec::<EeUnitsRtsPlayerFields>(O::PlRts, player as P)).mPlayerID as c_long }
}

/// `gGameOptions.mAllowCustomCivs` (the lobby's options: off, every upgrade is affordable).
fn allow_custom() -> bool {
    // SAFETY: the lobby's options record
    unsafe { (*rec::<crate::app::game_options::EeAppOptionsFields>(O::LobbyOptions, 0)).mAllowCustomCivs }
}

/// `CanAfford`: whether the upgrade is affordable (always, with custom civilizations off) and its cost into
/// `out_cost` (when not null), counted against `list` (null: the purchased upgrades): ee_sim tech civilization.rs.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_can_afford(
    civilization: *mut c_void,
    upgrade: u32,
    out_cost: *mut c_ulong,
    list: *mut c_void,
) -> bool {
    let cv = civilization as P;
    let list = upgrades(if list.is_null() { purchased(cv) } else { list as P });
    // SAFETY: the copied list; the caller's cost or null
    unsafe {
        ee_sim::tech::civilization::ee_tech_civ_can_afford(
            upgrade,
            list.as_ptr(),
            list.len(),
            *available(cv),
            allow_custom(),
            out_cost,
        )
    }
}

/// `CanAffordAll`: every upgrade of the list affordable in turn.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_can_afford_all(civilization: *mut c_void, list: *mut c_void) -> bool {
    let cv = civilization as P;
    let (bought, list) = (upgrades(purchased(cv)), upgrades(list as P));
    // SAFETY: the copied lists
    unsafe {
        ee_sim::tech::civilization::ee_tech_civ_can_afford_all(
            bought.as_ptr(),
            bought.len(),
            list.as_ptr(),
            list.len(),
            *available(cv),
            allow_custom(),
        )
    }
}

/// `Buy`: an upgrade applied to a player, first paid for and recorded when buying; whether it applied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_buy(civilization: *mut c_void, upgrade: u32, player: c_long, purchase: bool) -> bool {
    let cv = civilization as P;
    let mut cost: c_ulong = 0;
    if purchase && !ee_gamecpp_civ_can_afford(civilization, upgrade, &mut cost, std::ptr::null_mut()) {
        return false;
    }
    if purchase {
        *available(cv) = available(cv).wrapping_sub(cost);
        h(O::DvPurchasedPush, cv, &[upgrade as i64]);
    }
    apply(upgrade, player);
    *unchanged(cv) = false;
    true
}

/// `AddUpgrades`: a list's upgrades bought in turn for a player, stopping at the first that fails.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_add_upgrades(
    civilization: *mut c_void,
    list: *mut c_void,
    player: c_long,
    purchase: bool,
) {
    let list = list as P;
    let mut i = 0;
    let mut ok = true;
    while ok && i < h(O::DvListCount, list, &[]) {
        ok = ee_gamecpp_civ_buy(civilization, h(O::DvListAt, list, &[i]) as u32, player, purchase);
        i += 1;
    }
}

/// `ApplyLevel21ToWorld`: the purchased upgrades of the deferred kind applied to player 0.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_apply_level21(civilization: *mut c_void) {
    for u in upgrades(purchased(civilization as P)) {
        if u & CIV_LEVEL_MASK == EE_TECH_CIV_DEFERRED_INDEX {
            apply(u, 0);
        }
    }
}

/// `Reapply`: after a load the purchased upgrades applied to the player but those of the deferred kind, which wait;
/// 13 when one fails.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_reapply(civilization: *mut c_void, player: *mut c_void) -> c_long {
    let (cv, player) = (civilization as P, player_id(player));
    let (applied, deferred) = reapply(&upgrades(purchased(cv)), |u| apply(u, player));
    for u in deferred {
        h(O::DvDeferredPush, cv, &[u as i64]);
    }
    if applied {
        0
    } else {
        CIV_FAILED
    }
}

/// `ApplyDeferred`: the deferred upgrades applied to the player and forgotten; 13 when one fails.
#[no_mangle]
pub extern "C" fn ee_gamecpp_civ_apply_deferred(civilization: *mut c_void, player: *mut c_void) -> c_long {
    let (cv, player) = (civilization as P, player_id(player));
    if !apply_all(&upgrades(hp(O::DvDeferred, cv, &[])), |u| apply(u, player)) {
        return CIV_FAILED;
    }
    h(O::DvDeferredClear, cv, &[]);
    0
}

// ---- the wonder victory (p: an EEVictoryCondition*)

/// The team's players' ids.
fn team_players(vc: P) -> Vec<c_ulong> {
    (0..h(O::DwTeamCount, vc, &[])).map(|i| h(O::DwTeamPlayer, vc, &[i]) as c_ulong).collect()
}

/// `HasEnoughWonders`: the team's players together own as many wonders as the game needs (never when none).
#[no_mangle]
pub extern "C" fn ee_gamecpp_wonder_enough(condition: *mut c_void) -> bool {
    let vc = condition as P;
    let needed = h(O::DwNeeded, 0, &[]) as c_ulong;
    if needed == 0 {
        return false;
    }
    let mut count: c_ulong = 0;
    for id in team_players(vc) {
        count = count.wrapping_add(wonder_count(hp(O::WorldPlayer, 0, &[id as i64])));
    }
    count >= needed
}

/// The player's `GetNumWonderUnits()` (its wonders' list: ee_sim player_lists.rs).
fn wonder_count(pl: P) -> c_ulong {
    player_op(pl as u64, EE_UNITS_LISTS_WONDERS, ListOp::Len) as c_ulong
}

/// The countdown's winning entry: the standing wonders (time, player) sorted, the one completing the count.
pub fn winning_entry(mut entries: Vec<(c_ulong, c_ulong)>, needed: c_ulong) -> (c_ulong, c_ulong) {
    entries.sort();
    entries[needed as usize - 1]
}

/// The seconds left: the victory time less the seconds since the winning wonder (the C++'s unsigned arithmetic),
/// at least 0.
pub fn seconds_left(victory_time: c_long, game_time: c_ulong, built: c_ulong) -> c_ulong {
    let left = (victory_time as c_ulong).wrapping_sub(game_time.wrapping_sub(built) / 1000) as c_long;
    if left <= 0 {
        0
    } else {
        left as c_ulong
    }
}

/// `GetWonderCountdown`: the player whose wonder completes the team's count and the seconds left (NULL and 0 when
/// the team lacks the wonders).
#[no_mangle]
pub extern "C" fn ee_gamecpp_wonder_countdown(condition: *mut c_void, out: *mut c_ulong) -> *mut c_void {
    let vc = condition as P;
    let needed = h(O::DwNeeded, 0, &[]) as c_ulong;
    let (winner, value) = if ee_gamecpp_wonder_enough(condition) {
        let mut entries = Vec::new();
        for id in team_players(vc) {
            let pl = hp(O::WorldPlayer, 0, &[id as i64]);
            let mut j: c_ulong = 0;
            while j < wonder_count(pl) && j < needed {
                entries.push((h(O::DwWonderTime, pl, &[j as i64]) as c_long as c_ulong, id));
                j += 1;
            }
        }
        let (time, player) = winning_entry(entries, needed);
        let left = seconds_left(h(O::DwVictoryTime, 0, &[]) as c_long, h(O::DwGameTime, 0, &[]) as c_ulong, time);
        (hp(O::WorldPlayer, 0, &[player as i64]), left)
    } else {
        (0, 0)
    };
    // SAFETY: the caller's out value
    unsafe { *out = value };
    winner as *mut c_void
}

/// `EEVCWonder::GetVictoryState`: 1 (won) when the team's countdown has run out, outside cinematics.
#[no_mangle]
pub extern "C" fn ee_gamecpp_wonder_state(condition: *mut c_void) -> c_long {
    if h(O::DwInCinematic, 0, &[]) != 0 {
        return 0;
    }
    let mut value: c_ulong = 0;
    (!ee_gamecpp_wonder_countdown(condition, &mut value).is_null() && value == 0) as c_long
}

/// `EEVCWonder::Load`: the wonder list of saves before 0.225, read and dropped; the first read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_wonder_load(major: c_ulong, minor: c_ulong, file: *mut c_void) -> c_long {
    let f = Pf(file as P);
    let mut r = OK;
    if major == 0 && minor < 0xe1 {
        let mut count: c_ulong = 0;
        r = f.read_ulong(&mut count);
        for _ in 0..count {
            if r == OK {
                let mut old: c_ulong = 0;
                r = f.read_ulong(&mut old);
            }
        }
    }
    r as c_long
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_winning_wonder_is_the_counts_by_time_then_player() {
        let entries = vec![(5000, 3), (1000, 2), (1000, 1), (9000, 2)];
        assert_eq!(winning_entry(entries.clone(), 1), (1000, 1));
        assert_eq!(winning_entry(entries.clone(), 2), (1000, 2));
        assert_eq!(winning_entry(entries, 3), (5000, 3));
    }

    #[test]
    fn the_countdown_is_unsigned_and_stops_at_zero() {
        assert_eq!(seconds_left(600, 70_000, 10_000), 540);
        assert_eq!(seconds_left(600, 700_000, 10_000), 0);
        assert_eq!(seconds_left(0, 1_000, 1_000), 0);
    }
}
