//! The player's towns and herds as decisions (`EEPlayer`, EEPlayer.cpp; the player lane, round 2): which town or
//! herd a unit joins, which towns merge, which areas a leaving unit empties, and the walks that put every unplaced
//! building and animal into one: `AddUnitToTown`, `AddUnitToHerd`, `RemoveUnitFromTown`, `RemoveUnitFromHerd`,
//! `AssignTowns`, `AssignHerds`, as the C++ bodies in their order over the player's host ([`PlayerHost`], the C++
//! `EEPlayerHost`'s operations 84 on). The towns and herds stay C++ objects (`EETown`, `EEHerd`: their hulls,
//! tiles, patrol groups and the continent manager's areas): the host makes, merges, fills and deletes them.
//!
//! Determinism: the walks are the original's (the player's lists from the back, the units in list order); the one
//! random draw (`AddUnitToHerd`'s pick among several herds without the holder's) is the host's `GetRandom` at the
//! original's place.

#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;

use crate::player::{with, EeUnitsPlayerRefs, Player, PlayerHost};
use crate::player_units::{EE_UNITS_PLAYER_NEXT_UNIT, EE_UNITS_PLAYER_UNIT_INFO};

/// `kind` of the area operations: the towns (`mTowns`) or the herds (`mHerds`)
pub const EE_UNITS_PLAYER_TOWNS: i64 = 0;
pub const EE_UNITS_PLAYER_HERDS: i64 = 1;

// ---- the host's operations (the player's host, after player_diplomacy.rs's)
/// the size of the list `i[0]` (towns or herds)
pub const EE_UNITS_PLAYER_AREA_COUNT: i32 = 84;
/// list `i[0]`'s entry `i[1]` (its address; 0: none)
pub const EE_UNITS_PLAYER_AREA_AT: i32 = 85;
/// whether unit `i[0]` is a town building (`EETown::IsTownUnit`)
pub const EE_UNITS_PLAYER_TOWN_UNIT: i32 = 86;
/// whether unit `i[0]` is a herd animal on the map (`EEHerd::IsAnimalOnMap`)
pub const EE_UNITS_PLAYER_HERD_ANIMAL: i32 = 87;
/// area `i[1]` of list `i[0]` may take unit `i[2]` (a town's `CanTake(unit)`, a herd's `CanTake(unit, true)`)
pub const EE_UNITS_PLAYER_AREA_CAN_TAKE: i32 = 88;
/// herd `i[0]` holds unit `i[1]` (`HasUnit`)
pub const EE_UNITS_PLAYER_HERD_HAS: i32 = 89;
/// area `i[1]` of list `i[0]` adds unit `i[2]` (`AddUnit`: the result)
pub const EE_UNITS_PLAYER_AREA_ADD: i32 = 90;
/// a new area of list `i[0]` for unit `i[1]` (a town with the next town id, a herd of its animal with the next herd
/// id; a new serial, the continent manager's area): listed when it took the unit, else deleted (the result)
pub const EE_UNITS_PLAYER_AREA_NEW: i32 = 91;
/// unit `i[0]`'s holder (`mHolder`; 0: none)
pub const EE_UNITS_PLAYER_HOLDER: i32 = 92;
/// `gPhysics->GetRandom(i[0], i[1])` (a long)
pub const EE_UNITS_PLAYER_RANDOM: i32 = 93;
/// area `i[1]` of list `i[0]`'s `GetNumTiles()`
pub const EE_UNITS_PLAYER_AREA_TILES: i32 = 94;
/// area `i[1]` of list `i[0]` removes unit `i[2]` (`RemoveUnit`)
pub const EE_UNITS_PLAYER_AREA_REMOVE_UNIT: i32 = 95;
/// list `i[0]`'s entry `i[1]` out of the continent manager, the list, and deleted
pub const EE_UNITS_PLAYER_AREA_DROP: i32 = 96;
/// town `i[0]` merges town `i[1]` (`Merge`; `i[1]` out of the continent manager, and when listed out of the list
/// and the ids, its patrol points killed, deleted): the result its patrol group, referenced (0: none)
pub const EE_UNITS_PLAYER_TOWN_MERGE: i32 = 97;
/// town `i[0]` has a patrol group: every patrol goal of the player's units on one of the `i[2]` groups at `i[1]`
/// moves to it, and the groups are dereferenced
pub const EE_UNITS_PLAYER_TOWN_REPATROL: i32 = 98;
/// town `i[0]` contains the tile (`i[1]`, `i[2]`) (`EEAIArea::Contains`)
pub const EE_UNITS_PLAYER_TOWN_CONTAINS: i32 = 99;
/// unit `i[0]`'s `FindAreaTile(true, 1)` into `i[0]`, `i[1]`
pub const EE_UNITS_PLAYER_AREA_TILE: i32 = 100;
/// a land building's tile for `AssignTowns` into `i[0]`, `i[1]`: complex unit `i[0]`'s tile when on the map, else
/// its last map position's floors
pub const EE_UNITS_PLAYER_ASSIGN_TILE: i32 = 101;
/// town `i[0]` without a patrol group gets one (`RefreshPatrolGroup`)
pub const EE_UNITS_PLAYER_TOWN_PATROL: i32 = 102;

/// a tile x off the map
const OFF_MAP: i64 = -10000;

impl<'a, H: PlayerHost + ?Sized> Player<'a, H> {
    fn areas(&mut self, kind: i64) -> i64 {
        self.h.ask(EE_UNITS_PLAYER_AREA_COUNT, &[kind])
    }
    fn area(&mut self, kind: i64, i: i64) -> i64 {
        self.h.ask(EE_UNITS_PLAYER_AREA_AT, &[kind, i])
    }
    fn can_take(&mut self, kind: i64, area: i64, unit: i64) -> bool {
        self.h.ask(EE_UNITS_PLAYER_AREA_CAN_TAKE, &[kind, area, unit]) != 0
    }
    fn herd_has(&mut self, herd: i64, unit: i64) -> bool {
        self.h.ask(EE_UNITS_PLAYER_HERD_HAS, &[herd, unit]) != 0
    }
    fn area_add(&mut self, kind: i64, area: i64, unit: i64) -> bool {
        self.h.ask(EE_UNITS_PLAYER_AREA_ADD, &[kind, area, unit]) != 0
    }
    fn tiles(&mut self, kind: i64, area: i64) -> i64 {
        self.h.ask(EE_UNITS_PLAYER_AREA_TILES, &[kind, area])
    }
    fn tile_of(&mut self, op: i32, unit: i64) -> (i64, i64) {
        let (_, io) = self.h.info(op, &[unit]);
        (io.i[0], io.i[1])
    }
    fn town_contains(&mut self, town: i64, (x, y): (i64, i64)) -> bool {
        self.h.ask(EE_UNITS_PLAYER_TOWN_CONTAINS, &[town, x, y]) != 0
    }
    /// unit `x`'s complex unit (0: none)
    fn complex_of(&mut self, x: i64) -> crate::player_units::EeUnitsPlayerUnit {
        let mut u = crate::player_units::EeUnitsPlayerUnit::default();
        self.h.ask(EE_UNITS_PLAYER_UNIT_INFO, &[x, &mut u as *mut _ as i64]);
        u
    }

    /// `EEPlayer::AddUnitToTown`: a town building joins the only town that can take it, a new town when none can,
    /// or the largest of several after it merged the others (their patrols' units moving to its patrol group).
    pub fn add_unit_to_town(&mut self, unit: i64) -> bool {
        if self.h.ask(EE_UNITS_PLAYER_TOWN_UNIT, &[unit]) == 0 {
            return false;
        }
        self.tile_of(EE_UNITS_PLAYER_AREA_TILE, unit);
        let mut towns = Vec::new();
        let mut i = self.areas(EE_UNITS_PLAYER_TOWNS) - 1;
        while i >= 0 {
            let town = self.area(EE_UNITS_PLAYER_TOWNS, i);
            if self.can_take(EE_UNITS_PLAYER_TOWNS, town, unit) {
                towns.push(town);
            }
            i -= 1;
        }
        match towns.len() {
            0 => self.h.ask(EE_UNITS_PLAYER_AREA_NEW, &[EE_UNITS_PLAYER_TOWNS, unit]) != 0,
            1 => self.area_add(EE_UNITS_PLAYER_TOWNS, towns[0], unit),
            n => {
                let mut town = towns[n - 1];
                let mut size = self.tiles(EE_UNITS_PLAYER_TOWNS, town);
                for &other in towns[..n - 1].iter().rev() {
                    let other_size = self.tiles(EE_UNITS_PLAYER_TOWNS, other);
                    if size < other_size {
                        town = other;
                        size = other_size;
                    }
                }
                let mut old_groups = Vec::new();
                for &other in towns.iter().rev() {
                    if other == town {
                        continue;
                    }
                    let group = self.h.ask(EE_UNITS_PLAYER_TOWN_MERGE, &[town, other]);
                    if group != 0 {
                        old_groups.push(group);
                    }
                }
                let result = self.area_add(EE_UNITS_PLAYER_TOWNS, town, unit);
                if !old_groups.is_empty() {
                    self.h.ask(
                        EE_UNITS_PLAYER_TOWN_REPATROL,
                        &[town, old_groups.as_ptr() as i64, old_groups.len() as i64],
                    );
                }
                result
            }
        }
    }

    /// `EEPlayer::AddUnitToHerd`: a herd animal on the map joins (unless a herd holds it already) a new herd when
    /// none can take it, the only one that can, or of several the one holding its holder, else one at random.
    pub fn add_unit_to_herd(&mut self, unit: i64) -> bool {
        if self.h.ask(EE_UNITS_PLAYER_HERD_ANIMAL, &[unit]) == 0 {
            return false;
        }
        let mut herds = Vec::new();
        let mut i = self.areas(EE_UNITS_PLAYER_HERDS) - 1;
        while i >= 0 {
            let herd = self.area(EE_UNITS_PLAYER_HERDS, i);
            if self.can_take(EE_UNITS_PLAYER_HERDS, herd, unit) {
                herds.push(herd);
            }
            if self.herd_has(herd, unit) {
                return true;
            }
            i -= 1;
        }
        match herds.len() {
            0 => self.h.ask(EE_UNITS_PLAYER_AREA_NEW, &[EE_UNITS_PLAYER_HERDS, unit]) != 0,
            1 => self.area_add(EE_UNITS_PLAYER_HERDS, herds[0], unit),
            n => {
                let mut index: i64 = -1;
                let holder = self.h.ask(EE_UNITS_PLAYER_HOLDER, &[unit]);
                if holder != 0 {
                    index = n as i64 - 1;
                    while index >= 0 && !self.herd_has(herds[index as usize], holder) {
                        index -= 1;
                    }
                }
                if index == -1 {
                    index = self.h.ask(EE_UNITS_PLAYER_RANDOM, &[0, n as i64 - 1]);
                }
                let herd = herds.get(index as usize).copied().unwrap_or(0);
                self.area_add(EE_UNITS_PLAYER_HERDS, herd, unit)
            }
        }
    }

    /// `EEPlayer::RemoveUnitFromHerd`: out of every herd holding it; a herd left without tiles is dropped.
    pub fn remove_unit_from_herd(&mut self, unit: i64) {
        if self.h.ask(EE_UNITS_PLAYER_HERD_ANIMAL, &[unit]) == 0 {
            return;
        }
        let mut i = self.areas(EE_UNITS_PLAYER_HERDS) - 1;
        while i >= 0 {
            let herd = self.area(EE_UNITS_PLAYER_HERDS, i);
            if self.herd_has(herd, unit) {
                self.h.ask(EE_UNITS_PLAYER_AREA_REMOVE_UNIT, &[EE_UNITS_PLAYER_HERDS, herd, unit]);
                if self.tiles(EE_UNITS_PLAYER_HERDS, herd) == 0 {
                    self.h.ask(EE_UNITS_PLAYER_AREA_DROP, &[EE_UNITS_PLAYER_HERDS, i]);
                }
            }
            i -= 1;
        }
    }

    /// `EEPlayer::RemoveUnitFromTown`: a town building out of every town containing its area tile; a town left
    /// without tiles is dropped.
    pub fn remove_unit_from_town(&mut self, unit: i64) {
        if self.h.ask(EE_UNITS_PLAYER_TOWN_UNIT, &[unit]) == 0 {
            return;
        }
        let tile = self.tile_of(EE_UNITS_PLAYER_AREA_TILE, unit);
        let mut i = self.areas(EE_UNITS_PLAYER_TOWNS) - 1;
        while i >= 0 {
            let town = self.area(EE_UNITS_PLAYER_TOWNS, i);
            if self.town_contains(town, tile) {
                self.h.ask(EE_UNITS_PLAYER_AREA_REMOVE_UNIT, &[EE_UNITS_PLAYER_TOWNS, town, unit]);
                if self.tiles(EE_UNITS_PLAYER_TOWNS, town) == 0 {
                    self.h.ask(EE_UNITS_PLAYER_AREA_DROP, &[EE_UNITS_PLAYER_TOWNS, i]);
                }
            }
            i -= 1;
        }
    }

    /// `EEPlayer::AssignHerds`: every herd animal on the map that no herd holds, from the last, into a herd.
    pub fn assign_herds(&mut self) {
        let mut units = Vec::new();
        let mut x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[0]);
        while x != 0 {
            let c = self.complex_of(x).complex as i64;
            if c != 0 && self.h.ask(EE_UNITS_PLAYER_HERD_ANIMAL, &[c]) != 0 {
                let mut found = false;
                let mut i = self.areas(EE_UNITS_PLAYER_HERDS) - 1;
                while i >= 0 && !found {
                    let herd = self.area(EE_UNITS_PLAYER_HERDS, i);
                    found = self.herd_has(herd, c);
                    i -= 1;
                }
                if !found {
                    units.push(c);
                }
            }
            x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[x]);
        }
        for &c in units.iter().rev() {
            self.add_unit_to_herd(c);
        }
    }

    /// `EEPlayer::AssignTowns`: every land building outside a container whose tile no town contains, from the last,
    /// into a town; then every town without a patrol group gets one.
    pub fn assign_towns(&mut self) {
        let mut units = Vec::new();
        let mut x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[0]);
        while x != 0 {
            let u = self.complex_of(x);
            let c = u.complex as i64;
            if c != 0 {
                let cu = self.complex_of(c);
                if cu.land_building && cu.container_category == -1 {
                    let tile = self.tile_of(EE_UNITS_PLAYER_ASSIGN_TILE, x);
                    if tile.0 != OFF_MAP {
                        let mut found = false;
                        let mut i = self.areas(EE_UNITS_PLAYER_TOWNS) - 1;
                        while i >= 0 && !found {
                            let town = self.area(EE_UNITS_PLAYER_TOWNS, i);
                            found = self.town_contains(town, tile);
                            i -= 1;
                        }
                        if !found {
                            units.push(c);
                        }
                    }
                }
            }
            x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[x]);
        }
        for &c in units.iter().rev() {
            self.add_unit_to_town(c);
        }
        let mut k = self.areas(EE_UNITS_PLAYER_TOWNS) - 1;
        while k >= 0 {
            let town = self.area(EE_UNITS_PLAYER_TOWNS, k);
            if town != 0 {
                self.h.ask(EE_UNITS_PLAYER_TOWN_PATROL, &[town]);
            }
            k -= 1;
        }
    }
}

// ---- the C ABI
/// The towns and herds on `unit`: `what` 0 `AddUnitToTown`, 1 `AddUnitToHerd` (the result: whether it was added),
/// 2 `RemoveUnitFromTown`, 3 `RemoveUnitFromHerd`, 4 `AssignTowns`, 5 `AssignHerds`.
///
/// # Safety
/// As player.rs's `with`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_areas(refs: *const EeUnitsPlayerRefs, what: u32, unit: *mut c_void) -> bool {
    let x = unit as i64;
    // SAFETY: the caller's contract
    unsafe {
        with(refs, |p| match what {
            0 => p.add_unit_to_town(x),
            1 => p.add_unit_to_herd(x),
            2 => {
                p.remove_unit_from_town(x);
                false
            }
            3 => {
                p.remove_unit_from_herd(x);
                false
            }
            4 => {
                p.assign_towns();
                false
            }
            _ => {
                p.assign_herds();
                false
            }
        })
    }
}

#[cfg(test)]
#[path = "player_areas_tests.rs"]
mod tests;
