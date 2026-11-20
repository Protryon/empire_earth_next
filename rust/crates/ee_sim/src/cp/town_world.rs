//! The town host's bodies that call several C++ operations (AI Thread cluster round 7; were EETown.cpp's): a building's
//! facts (`EE_CP_TOWN_UNIT`), `IsTownUnit`, the patrol group's search, making and release, the constructor. Each C++
//! call is one `ee_cp_town_host` case (`EE_CP_TOWN_*` below), in the original's order. Absent from a test binary but
//! [`town_unit_kind`].

// `c_long`/`c_ulong` are 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]

use std::os::raw::{c_long, c_ulong};

/// unit `a`'s `GetTechTreeNode()`'s `mData->mKind` (-1: no node); `FindAreaTile(true, 1)` (x high, y low words)
pub const EE_CP_TOWN_NODE_KIND: i32 = 124;
pub const EE_CP_TOWN_AREA_TILE: i32 = 125;
/// `mPatrolGroup = a`; `mPlayer->AddPatrolGroup(mPatrolGroup)`; `mPatrolGroup`
pub const EE_CP_TOWN_SET_PATROL_GROUP: i32 = 126;
pub const EE_CP_TOWN_ADD_PATROL_GROUP: i32 = 127;
pub const EE_CP_TOWN_PATROL_GROUP: i32 = 128;
/// unit `a`'s `mPlayer->mHumanPlayer != NULL`; `gMapWidth` (low word) and `gMapHeight` (high word)
pub const EE_CP_TOWN_HUMAN_OWNER: i32 = 129;
pub const EE_CP_TOWN_MAP_SIZE: i32 = 130;

/// What `IsTownUnit`'s first test reads of a unit and its class.
#[derive(Clone, Copy, Debug, Default)]
pub(crate) struct TownUnit {
    pub has_tile: bool,
    pub family: c_ulong,
    pub land_building: bool,
    pub category: c_ulong,
    pub max_velocity: f32,
    pub wall: bool,
}

/// `IsTownUnit`'s first test: a placed, non-ambient, immobile land building (or category 0x13), no wall unless the
/// owner is human (`human` asked only for a wall, as the original's `||`).
pub(crate) fn town_unit_kind(u: &TownUnit, human: impl FnOnce() -> bool) -> bool {
    u.has_tile
        && u.family != 0x23
        && (u.land_building || u.category == 0x13)
        && u.max_velocity == 0.0
        && (!u.wall || human())
}

/// A packed tile (x in the high word, y in the low one) as two `long`s.
pub(crate) fn unpack(v: i64) -> (c_long, c_long) {
    ((v >> 32) as i32 as c_long, v as i32 as c_long)
}

#[cfg(not(test))]
pub(crate) use game::*;

#[cfg(not(test))]
mod game {
    use std::ffi::c_void;
    use std::os::raw::{c_long, c_ulong};

    use super::super::town::{self, EeTownFields, TownIo};
    use super::*;
    use crate::units::patrol_group::{ee_units_pg_dereference, ee_units_pg_new, ee_units_pg_reference, ee_units_pg_set_town_patrol};
    use crate::world::{UnitView, WorldRead, WorldView};
    use crate::world_field;

    use super::super::town_game::ee_cp_town_host;

    fn cpp(town: u64, op: i32, a: u64) -> i64 {
        let mut io = TownIo::default();
        // SAFETY: the bodies' live town and objects
        unsafe { ee_cp_town_host(town as *mut c_void, op, a, 0, &mut io) }
    }

    /// The operations below in Rust (`None`: the C++ host's).
    ///
    /// # Safety
    /// `town` the live `EETown*` (its `EEAIArea*`); `a` a live unit as the operation takes it.
    pub(crate) unsafe fn town_op(town: u64, op: i32, a: u64, io: &mut TownIo) -> Option<i64> {
        Some(match op {
            town::EE_CP_TOWN_UNIT => {
                let kind = cpp(town, EE_CP_TOWN_NODE_KIND, a);
                let (x, y) = unpack(cpp(town, EE_CP_TOWN_AREA_TILE, a));
                let t = unsafe { WorldRead::begin() };
                // SAFETY: the caller's contract
                let u = unsafe { UnitView::of(&t, a) }?;
                let c = u.class()?;
                let influence: c_long = world_field!(c.fields(), mInfluence);
                io.v[0] = influence as i64;
                io.v[1] = i64::from(kind == 4);
                io.v[2] = i64::from(world_field!(c.fields(), mIsWall));
                io.v[3] = c.unit_type() as i64;
                io.v[4] = x as i64;
                io.v[5] = y as i64;
                io.v[6] = u.player().map_or(0, |p| p.handle()) as i64;
                0
            }
            town::EE_CP_TOWN_IS_TOWN_UNIT => i64::from(is_town_unit(a)),
            town::EE_CP_TOWN_FIND_PATROL => {
                find_patrol(town);
                0
            }
            town::EE_CP_TOWN_NEW_PATROL => {
                let g = ee_units_pg_new();
                cpp(town, EE_CP_TOWN_SET_PATROL_GROUP, g as u64);
                ee_units_pg_set_town_patrol(g, true);
                cpp(town, EE_CP_TOWN_ADD_PATROL_GROUP, 0);
                0
            }
            town::EE_CP_TOWN_DROP_PATROL => {
                let g = cpp(town, EE_CP_TOWN_PATROL_GROUP, 0) as usize as *mut crate::units::patrol_group::EeUnitsPatrolGroup;
                if !g.is_null() {
                    ee_units_pg_dereference(g);
                    cpp(town, EE_CP_TOWN_SET_PATROL_GROUP, 0);
                }
                0
            }
            _ => return None,
        })
    }

    /// `IsTownUnit`: a town's building (`town_unit_kind`) whose area tile is on the map and on a land continent.
    fn is_town_unit(unit: u64) -> bool {
        let facts = {
            let t = unsafe { WorldRead::begin() };
            // SAFETY: the callers hand live units
            unsafe { UnitView::of(&t, unit) }.and_then(|u| {
                let c = u.class()?;
                let f = c.fields();
                Some(TownUnit {
                    has_tile: u.tile().is_some(),
                    family: c.family() as c_ulong,
                    land_building: c.land_building(),
                    category: c.category() as c_ulong,
                    max_velocity: world_field!(f, mMaxVelocity),
                    wall: world_field!(f, mIsWall),
                })
            })
        };
        let Some(facts) = facts else { return false };
        if !town_unit_kind(&facts, || cpp(0, EE_CP_TOWN_HUMAN_OWNER, unit) != 0) {
            return false;
        }
        let (x, y) = unpack(cpp(0, EE_CP_TOWN_AREA_TILE, unit));
        let size = cpp(0, EE_CP_TOWN_MAP_SIZE, 0);
        let (width, height) = (size as u32 as c_ulong, (size >> 32) as u32 as c_ulong);
        if !((x as c_ulong) < width && (y as c_ulong) < height) {
            return false;
        }
        let Some(r) = crate::astar::world_view::cm() else { return false };
        let id = crate::astar::cm_io::continent_id_at(r, x, y);
        if id == 0 {
            return false;
        }
        let c = r.tables().tables[crate::astar::cm::EeAStarCmTable::Continents as usize].get(id as usize).copied();
        match c {
            // SAFETY: the manager's live continent
            Some(c) if c != 0 => unsafe { crate::astar::cm_io::continent(c) }.category == 1,
            _ => false,
        }
    }

    /// `FindPatrolGroup` (0xac116): an own path point's group on a hull vertex, referenced.
    ///
    /// # Safety
    /// As [`town_op`].
    unsafe fn find_patrol(town: u64) {
        let player = cpp(town, town::EE_CP_TOWN_PLAYER, 0) as u64;
        let hull: Vec<(i64, i64)> = {
            let mut n = 0;
            // SAFETY: the town's area record
            let points = unsafe { crate::astar::area::ee_astar_area_hull_points(crate::astar::cm_io::area(town), &mut n) };
            // SAFETY: its n points
            (0..n).map(|k| unsafe { *points.add(k) }).map(|p| (p.x as i64, p.y as i64)).collect()
        };
        for (x, y) in hull {
            if cpp(town, EE_CP_TOWN_PATROL_GROUP, 0) != 0 {
                break;
            }
            let found: Vec<u64> = {
                let t = unsafe { WorldRead::begin() };
                let Some(tile) = WorldView::global(&t).and_then(|w| w.map()).and_then(|m| m.tile(x, y)) else { continue };
                tile.units()
                    .flatten()
                    .filter(|u| {
                        u.class().is_some_and(|c| c.unit_type() == 0xdf)
                            && u.player().map_or(0, |p| p.handle()) == player
                    })
                    .map(|u| u.complex_unit().map_or(0, |c| c.patrol_group_handle()))
                    .collect()
            };
            for g in found {
                if cpp(town, EE_CP_TOWN_PATROL_GROUP, 0) != 0 {
                    break;
                }
                if g != 0 {
                    cpp(town, EE_CP_TOWN_SET_PATROL_GROUP, g);
                    ee_units_pg_reference(g as usize as *mut crate::units::patrol_group::EeUnitsPatrolGroup);
                }
            }
        }
    }

    /// `EETown(player, id)`'s body after its members (`mPlayer`, `mPatrolGroup` NULL): the id, the area's kind, `Reset`.
    ///
    /// # Safety
    /// `town` the `EETown*` being made, `area` its `EEAIArea*`, `f` its record.
    #[no_mangle]
    pub unsafe extern "C" fn ee_cp_town_construct(town: *mut c_void, area: *mut c_void, f: *mut EeTownFields, id: c_long) {
        // SAFETY: the caller's contract
        unsafe {
            (*f).mTownID = id;
            crate::astar::cm_io::area(area as u64).area_type = 5; // kAIAreaTown
            super::super::town_game::ee_cp_town_reset(town, area, f);
        }
    }

    /// `EETown::IsTownUnit`.
    ///
    /// # Safety
    /// `unit` a live `EEUnit*`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_cp_town_is_town_unit(unit: u64) -> bool {
        is_town_unit(unit)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_town_unit_is_an_immobile_building_and_a_wall_only_for_humans() {
        let b = TownUnit { has_tile: true, family: 1, land_building: true, category: 1, max_velocity: 0.0, wall: false };
        assert!(town_unit_kind(&b, || unreachable!()));
        assert!(town_unit_kind(&TownUnit { land_building: false, category: 0x13, ..b }, || unreachable!()));
        assert!(!town_unit_kind(&TownUnit { land_building: false, ..b }, || unreachable!()));
        assert!(!town_unit_kind(&TownUnit { family: 0x23, ..b }, || unreachable!()));
        assert!(!town_unit_kind(&TownUnit { max_velocity: 1.0, ..b }, || unreachable!()));
        assert!(!town_unit_kind(&TownUnit { has_tile: false, ..b }, || unreachable!()));
        assert!(town_unit_kind(&TownUnit { wall: true, ..b }, || true));
        assert!(!town_unit_kind(&TownUnit { wall: true, ..b }, || false));
    }

    #[test]
    fn a_packed_tile_unpacks_signed() {
        let v = ((-3i64 as u32 as i64) << 32) | (7u32 as i64);
        assert_eq!(unpack(v), (-3, 7));
    }
}
