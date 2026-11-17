//! The player's diplomacy and shared sight (`EEPlayer`, EEPlayer.cpp; the player lane, round 2): the C++ bodies in
//! their order over the player's host ([`PlayerHost`], the C++ `EEPlayerHost`'s operations 71 on):
//!
//! - `SetDiplomaticStance`: an alliance formed (both players' units and events out of sight, the allies' lists,
//!   the stance, back in sight; the wonder timers restarted; the enemy sight power's sharing ended) or broken (the
//!   mirror, the selection of the other's unit dropped), the local terrain's visibility, the store's diplomacy
//!   table, the world's share bonus and the mini-map;
//! - `AddSightSharer`, `RemoveSightSharer` (the land buildings' sight around the change), `SetSharedLOS`.
//!
//! Players are host arguments by address, 0 for this player. The stances are the records' (`mEnemies`), read
//! fresh at each test (a stance toward itself aliases as the original's reference did).

#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::c_ulong;

use crate::units::player::{with, EeUnitsPlayerRefs, Player, PlayerHost};
use crate::units::player_units::{EE_UNITS_PLAYER_SHARE_BONUS, EE_UNITS_PLAYER_SIGHT_SHARER};

// ---- the host's operations (the player's host, after player_units.rs's)
/// player `i[0]`'s id (`i[0]`), `mSaveVisMap` (`i[1]`) and stance toward player `i[1]` (`i[2]`)
pub const EE_UNITS_PLAYER_VIEW: i32 = 71;
/// every unit of player `i[0]` (`i[2]` 1: its land buildings only) `SightOnHere()` (`i[1]` 1) or `SightOffHere()`
pub const EE_UNITS_PLAYER_UNITS_SIGHT: i32 = 72;
/// player `i[0]`'s events `ShowEvents` (`i[1]` 1) or `HideEvents`
pub const EE_UNITS_PLAYER_EVENTS_SIGHT: i32 = 73;
/// player `i[0]`'s allies: player `i[1]` pushed (`i[2]` 1) or removed (`RemoveAlly`)
pub const EE_UNITS_PLAYER_ALLY: i32 = 74;
/// `EEUnitStorePlayerTable(this)` (the store's diplomacy table)
pub const EE_UNITS_PLAYER_STORE_TABLE: i32 = 75;
/// `ResetWonderTimers(i[0])`
pub const EE_UNITS_PLAYER_RESET_WONDER_TIMERS: i32 = 76;
/// `mSightSharers`: player `i[0]` pushed (`i[1]` 1) or erased
pub const EE_UNITS_PLAYER_SHARERS: i32 = 77;
/// whether the viewed unit (`mViewedUnit`) is player `i[0]`'s
pub const EE_UNITS_PLAYER_VIEWED_OF: i32 = 78;
/// `UpdateSelection(NULL, true)`
pub const EE_UNITS_PLAYER_UNVIEW: i32 = 79;
/// whether player `i[0]` is the local player (`gWorld.GetCurrentPlayer()`)
pub const EE_UNITS_PLAYER_IS_CURRENT: i32 = 80;
/// the local player's `RefreshTerrainVisibility()`
pub const EE_UNITS_PLAYER_REFRESH_TERRAIN: i32 = 81;
/// `gMiniMap->Redraw()` when there is one
pub const EE_UNITS_PLAYER_MINIMAP_REDRAW: i32 = 82;
/// `mWonders`' count of power `i[0]` (the Rust list: player_units.rs)
const POWER_ENEMY_SIGHT: i64 = 5;

/// A player as the stance sees it.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
struct View {
    id: usize,
    save_vis: bool,
    stance: c_ulong,
}

impl<'a, H: PlayerHost + ?Sized> Player<'a, H> {
    fn view(&mut self, player: i64, toward: usize) -> View {
        let (_, io) = self.h.info(EE_UNITS_PLAYER_VIEW, &[player, toward as i64]);
        View { id: io.i[0] as usize, save_vis: io.i[1] != 0, stance: io.i[2] as c_ulong }
    }
    fn sight(&mut self, player: i64, on: bool, land_only: bool) {
        self.h.ask(EE_UNITS_PLAYER_UNITS_SIGHT, &[player, i64::from(on), i64::from(land_only)]);
    }
    fn events(&mut self, player: i64, show: bool) {
        self.h.ask(EE_UNITS_PLAYER_EVENTS_SIGHT, &[player, i64::from(show)]);
    }
    fn ally(&mut self, player: i64, other: i64, add: bool) {
        self.h.ask(EE_UNITS_PLAYER_ALLY, &[player, other, i64::from(add)]);
    }
    fn stance(&self, id: usize) -> c_ulong {
        // SAFETY: the player's record (a field read)
        unsafe { (*self.saved).mEnemies.get(id).copied().unwrap_or(0) }
    }
    fn set_stance(&mut self, id: usize, stance: c_ulong) {
        // SAFETY: the player's record (a store; no reference kept)
        if let Some(s) = unsafe { (*self.saved).mEnemies.get_mut(id) } {
            *s = stance;
        }
        self.h.ask(EE_UNITS_PLAYER_STORE_TABLE, &[]);
    }
    fn enemy_sight_powers(&mut self) -> u32 {
        // SAFETY: the record's wonder state (a read)
        unsafe { (*(*self.fields).mWonderState).powers.iter().filter(|&&p| p == POWER_ENEMY_SIGHT as _).count() as u32 }
    }

    /// `EEPlayer::SetDiplomaticStance(other, stance, map)`.
    pub fn set_diplomatic_stance(&mut self, other: i64, stance: c_ulong, map: bool) {
        let me = self.view(0, 0);
        let o = self.view(other, me.id);
        if self.stance(o.id) == stance {
            return;
        }
        if me.save_vis && o.save_vis {
            if stance == 0 {
                if map {
                    for _ in 0..self.enemy_sight_powers() {
                        self.h.ask(EE_UNITS_PLAYER_SIGHT_SHARER, &[other, 0]);
                    }
                    if self.view(other, me.id).stance == 0 {
                        self.h.ask(EE_UNITS_PLAYER_RESET_WONDER_TIMERS, &[other]);
                        self.sight(0, false, false);
                        self.events(0, false);
                        self.sight(other, false, false);
                        self.events(other, false);
                        self.ally(0, other, true);
                        self.ally(other, 0, true);
                        self.set_stance(o.id, 0);
                        self.sight(0, true, false);
                        self.events(0, true);
                        self.sight(other, true, false);
                        self.events(other, true);
                    }
                } else {
                    self.ally(0, other, true);
                }
            } else if self.stance(o.id) == 0 {
                for _ in 0..self.enemy_sight_powers() {
                    self.h.ask(EE_UNITS_PLAYER_SIGHT_SHARER, &[other, 1]);
                }
                if self.view(other, me.id).stance == 0 {
                    self.sight(0, false, false);
                    self.sight(other, false, false);
                    self.ally(0, other, false);
                    self.ally(other, 0, false);
                    self.set_stance(o.id, stance);
                    self.sight(0, true, false);
                    self.sight(other, true, false);
                    if self.h.ask(EE_UNITS_PLAYER_VIEWED_OF, &[other]) != 0 {
                        self.h.ask(EE_UNITS_PLAYER_UNVIEW, &[]);
                    }
                }
            }
        }
        if self.h.ask(EE_UNITS_PLAYER_IS_CURRENT, &[other]) != 0 || self.h.ask(EE_UNITS_PLAYER_IS_CURRENT, &[0]) != 0 {
            self.h.ask(EE_UNITS_PLAYER_REFRESH_TERRAIN, &[]);
        }
        self.set_stance(o.id, stance);
        self.h.ask(EE_UNITS_PLAYER_SHARE_BONUS, &[1]);
        self.h.ask(EE_UNITS_PLAYER_SHARE_BONUS, &[0]);
        self.h.ask(EE_UNITS_PLAYER_MINIMAP_REDRAW, &[]);
    }

    /// `AddSightSharer` (`add`) / `RemoveSightSharer`: player `other` in or out of the sharers, the land
    /// buildings' sight off before and on after.
    pub fn sight_sharer(&mut self, other: i64, add: bool) {
        self.sight(0, false, true);
        self.h.ask(EE_UNITS_PLAYER_SHARERS, &[other, i64::from(add)]);
        self.sight(0, true, true);
    }

    /// `SetSharedLOS`: on a change, every unit's sight and the events off, the flag, then on again.
    pub fn set_shared_los(&mut self, flag: bool) {
        // SAFETY: the player's record (a field read)
        if unsafe { (*self.saved).mSharedLOS } == flag {
            return;
        }
        self.sight(0, false, false);
        self.events(0, false);
        // SAFETY: as above (a store)
        unsafe { (*self.saved).mSharedLOS = flag };
        self.sight(0, true, false);
        self.events(0, true);
    }
}

// ---- the C ABI
/// The diplomacy on player `other` (an `EEPlayer*`): `what` 0 `SetDiplomaticStance(other, stance, map)` (`flag`:
/// a map), 1 `AddSightSharer(other)`, 2 `RemoveSightSharer(other)`, 3 `SetSharedLOS(flag)`.
///
/// # Safety
/// As player.rs's `with`; the record's wonder state is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_diplomacy(
    refs: *const EeUnitsPlayerRefs,
    what: u32,
    other: *mut c_void,
    stance: c_ulong,
    flag: bool,
) {
    let other = other as i64;
    // SAFETY: the caller's contract
    unsafe {
        with(refs, |p| match what {
            0 => p.set_diplomatic_stance(other, stance, flag),
            1 => p.sight_sharer(other, true),
            2 => p.sight_sharer(other, false),
            _ => p.set_shared_los(flag),
        })
    }
}

#[cfg(test)]
#[path = "player_diplomacy_tests.rs"]
mod tests;
