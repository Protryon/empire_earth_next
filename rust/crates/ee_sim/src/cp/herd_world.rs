//! The herd host's bodies that call several C++ operations (AI Thread cluster round 7; were `ee_cp_herd_host`'s
//! multi-statement cases in EEHerd.cpp): the unit's record from the world views, the formations, `CanStandOn`,
//! `GiveGoal` and the goals, `GiveBirth`'s newborn, the attacker's and holder's release, the units in a rectangle
//! with the herd's filter, the area's hull bounds and test. Each C++ call is one `ee_cp_herd_host` case
//! (`EE_CP_HERD_*` below), in the original's order. Absent from a test binary but [`herd_filter`].

// `c_long`/`c_ulong` are 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]

use std::os::raw::{c_long, c_ulong};


// ---- the herd host's one-statement operations the bodies below ask (EEHerd.cpp)
/// unit `a`'s `mWaypoints.size()`; `mStats->mModel` (its `EEMedia`)
pub const EE_CP_HERD_UNIT_WAYPOINTS: i32 = 64;
pub const EE_CP_HERD_UNIT_MODEL: i32 = 65;
/// `EEFormation::Create(6)`; the formation `b`'s `mCenter = (f0, f1, f2)`, `SetCenter(..)`, `SetMinRadius(f0)`,
/// `mMinRadius = f0`, `Place` of the `input` units (the units after it into `list2`, the slots into `list`),
/// `Dereference()`
pub const EE_CP_HERD_FORM_NEW: i32 = 66;
pub const EE_CP_HERD_FORM_CENTER: i32 = 67;
pub const EE_CP_HERD_FORM_SET_CENTER: i32 = 68;
pub const EE_CP_HERD_FORM_SET_MIN_RADIUS: i32 = 69;
pub const EE_CP_HERD_FORM_MIN_RADIUS: i32 = 70;
pub const EE_CP_HERD_FORM_PLACE: i32 = 71;
pub const EE_CP_HERD_FORM_DEREF: i32 = 72;
/// the tile unit `b`'s `IsPassableBy(a)`; unit `a`'s class `CanMoveOnTerrain(a, v0)`; `GetTileAtLocation((v0, v1))`
pub const EE_CP_HERD_PASSABLE_BY: i32 = 73;
pub const EE_CP_HERD_CAN_MOVE_ON: i32 = 74;
pub const EE_CP_HERD_TILE_AT: i32 = 75;
/// unit `a`'s `mLastAttacker = NULL`; `mHolder->ReleaseHeld(a)`; `mHolder = NULL`
pub const EE_CP_HERD_ATTACKER_NULL: i32 = 76;
pub const EE_CP_HERD_RELEASE_HELD: i32 = 77;
pub const EE_CP_HERD_HOLDER_NULL: i32 = 78;
/// unit `a`'s goal queue 0's goal `v0`; whether the goal `b` is a location goal; its `GetLocation()` into `f0..f2`
pub const EE_CP_HERD_GOAL_AT: i32 = 79;
pub const EE_CP_HERD_GOAL_IS_LOCATION: i32 = 80;
pub const EE_CP_HERD_GOAL_POINT: i32 = 81;
/// the goals: `NewLocation(f0, f1, f2, v0, v1, v2, f3, false, false)`, `NewLocation(&(f0, f1, f2), v0, v1, v2, f3,
/// false, false)`, `NewAttackUnit(a, b, true, false)`
pub const EE_CP_HERD_NEW_LOCATION: i32 = 82;
pub const EE_CP_HERD_NEW_LOCATION_AT: i32 = 83;
pub const EE_CP_HERD_NEW_ATTACK: i32 = 84;
/// unit `a`'s `SetGoal(b, v0, 0)`, `StartAction(v0)`, `mUnitClass->ActionChanged(a)`; `delete b`
pub const EE_CP_HERD_SET_GOAL: i32 = 85;
pub const EE_CP_HERD_START_ACTION: i32 = 86;
pub const EE_CP_HERD_ACTION_CHANGED: i32 = 87;
pub const EE_CP_HERD_DELETE_GOAL: i32 = 88;
/// animal `a`'s `GetUnitClass()`, `GetNewbornGrowth()` into `f0`; class `a`'s `CreateUnit(v0, v1, v2, 0, NULL)`;
/// `dynamic_cast<EEComplexUnit*>` of unit `a`
pub const EE_CP_HERD_ANIMAL_CLASS: i32 = 89;
pub const EE_CP_HERD_NEWBORN_GROWTH: i32 = 90;
pub const EE_CP_HERD_CREATE_UNIT: i32 = 91;
pub const EE_CP_HERD_TO_COMPLEX: i32 = 92;
/// unit `a`'s `SetTargetSearchTimer(v0)`, `HoldUnit(b)`
pub const EE_CP_HERD_SET_TARGET_SEARCH: i32 = 93;
pub const EE_CP_HERD_HOLD_UNIT: i32 = 94;
/// `gPathManager.CancelAutoPathRequest(a)`; the request `a`'s `Dereference()`
pub const EE_CP_HERD_CANCEL_REQUEST: i32 = 95;
pub const EE_CP_HERD_REQUEST_DEREF: i32 = 96;

/// cbindgen:ignore
#[cfg_attr(test, allow(dead_code))]
mod k {
    use std::os::raw::c_ulong;
    pub(super) const MOVEMENT_SEA: u8 = 4;
    pub(super) const TYPE_FISHING_BOAT_STONE: c_ulong = 0x6c;
    pub(super) const TYPE_FISHING_BOAT_BRONZE: c_ulong = 0x6d;
    pub(super) const TYPE_FISHING_BOAT_MODERN: c_ulong = 0xd3;
    pub(super) const CATEGORY_SUBMERGED_NAVAL: c_ulong = 0xa;
    pub(super) const CATEGORY_SEA_ANIMAL: c_ulong = 0x1b;
    pub(super) const ACTION_INITIAL_ATTACK_STATE: i64 = 2016;
    pub(super) const ACTION_PREPARE_TO_MOVE: i64 = 8000;
}
use k::*;

/// What the herd's unit filter reads of a unit.
#[derive(Clone, Copy, Debug, Default)]
pub(crate) struct FilterUnit {
    pub on_map: bool,
    pub domains: u8,
    pub unit_type: c_ulong,
    pub category: c_ulong,
    pub player_id: u32,
    pub hit_points: c_long,
}

/// The filter of Chase/GetPrey (`EEHerdUnitFilter`): a living sea-going unit on the map, on `continent` (0: any),
/// either a fishing boat (`huntable`) or a non-gaia unit that is no submarine or sea animal, moving when `moving`
/// (more than one waypoint). `unit_continent` and `waypoints` are asked only where the original's `&&` asked them.
pub(crate) fn herd_filter(
    huntable: bool,
    moving: bool,
    continent: u16,
    u: &FilterUnit,
    unit_continent: impl FnOnce() -> u16,
    waypoints: impl FnOnce() -> i64,
) -> bool {
    if u.domains & MOVEMENT_SEA == 0 || !u.on_map {
        return false;
    }
    let kind = if huntable {
        u.unit_type >= TYPE_FISHING_BOAT_STONE
            && (u.unit_type <= TYPE_FISHING_BOAT_BRONZE || u.unit_type == TYPE_FISHING_BOAT_MODERN)
    } else {
        u.category != CATEGORY_SUBMERGED_NAVAL && u.category != CATEGORY_SEA_ANIMAL && u.player_id != 0
    };
    kind && (continent == 0 || unit_continent() == continent) && (!moving || waypoints() > 1) && u.hit_points > 0
}

#[cfg(not(test))]
pub(crate) use game::*;

#[cfg(not(test))]
mod game {
    use std::ffi::c_void;
    use std::os::raw::{c_long, c_ulong};

    use super::super::herd::{self, EeHerdIo};
    use super::*;
    use crate::world::{ClassView, TileView, UnitView, WorldRead, WorldView};
    use crate::world_field;

    use super::super::herd_game::ee_cp_herd_host;

    /// The C++ host's operation `op` (its one-statement cases).
    ///
    /// # Safety
    /// `herd` the live `EEHerd*`; the operation's objects live.
    unsafe fn cpp(herd: u64, op: i32, a: u64, b: u64, io: &mut EeHerdIo) -> i64 {
        // SAFETY: the caller's contract
        unsafe { ee_cp_herd_host(herd as *mut c_void, op, a, b, io) }
    }

    fn cpp0(herd: u64, op: i32, a: u64, b: u64) -> i64 {
        // SAFETY: the bodies' live objects
        unsafe { cpp(herd, op, a, b, &mut EeHerdIo::default()) }
    }

    fn with_v(v: &[i64]) -> EeHerdIo {
        let mut io = EeHerdIo::default();
        io.v[..v.len()].copy_from_slice(v);
        io
    }

    fn with_f(f: &[f32]) -> EeHerdIo {
        let mut io = EeHerdIo::default();
        io.f[..f.len()].copy_from_slice(f);
        io
    }

    fn unit<R>(handle: u64, f: impl FnOnce(UnitView<'_>) -> R) -> Option<R> {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: the bodies hand live units
        unsafe { UnitView::of(&t, handle) }.map(f)
    }

    fn holder_of(handle: u64) -> u64 {
        unit(handle, |u| u.holder_handle()).unwrap_or(0)
    }

    /// The operations below in Rust (`None`: the C++ host's).
    ///
    /// # Safety
    /// `herd` the live `EEHerd*` (its `EEAIArea*`); `a`, `b` live objects as the operation takes them.
    pub(crate) unsafe fn herd_op(herd: u64, op: i32, a: u64, b: u64, io: &mut EeHerdIo) -> Option<i64> {
        use herd::*;
        Some(match op {
            EE_CP_HERD_UNIT => {
                unit_record(herd, a, io);
                0
            }
            EE_CP_HERD_FORMATION => {
                formation(herd, io);
                0
            }
            EE_CP_HERD_STAND => i64::from(stand(herd, a, io)),
            EE_CP_HERD_CLEAR_ATTACKER => {
                let attacker = unit(a, |u| u.last_attacker_handle()).unwrap_or(0);
                if attacker != 0 {
                    cpp0(herd, EE_CP_HERD_DEREF, attacker, 0);
                }
                cpp0(herd, EE_CP_HERD_ATTACKER_NULL, a, 0);
                0
            }
            EE_CP_HERD_RELEASE_HOLDER => {
                cpp0(herd, EE_CP_HERD_RELEASE_HELD, a, 0);
                let holder = holder_of(a);
                if holder != 0 {
                    cpp0(herd, EE_CP_HERD_DEREF, holder, 0);
                }
                cpp0(herd, EE_CP_HERD_HOLDER_NULL, a, 0);
                0
            }
            EE_CP_HERD_GOAL_LOCATION => {
                let goal = cpp(herd, EE_CP_HERD_GOAL_AT, a, 0, &mut with_v(&[io.v[0]])) as u64;
                if cpp0(herd, EE_CP_HERD_GOAL_IS_LOCATION, 0, goal) == 0 {
                    return Some(0);
                }
                let mut q = EeHerdIo::default();
                cpp(herd, EE_CP_HERD_GOAL_POINT, 0, goal, &mut q);
                io.f[0] = q.f[0];
                io.f[1] = q.f[1];
                1
            }
            EE_CP_HERD_GOAL_ATTACK => {
                let goal = cpp0(herd, EE_CP_HERD_NEW_ATTACK, a, b) as u64;
                i64::from(give_goal(herd, a, goal, false, ACTION_INITIAL_ATTACK_STATE))
            }
            EE_CP_HERD_GOAL_MOVE => {
                let mut q = EeHerdIo { v: io.v, f: io.f, ..Default::default() };
                let op = if io.v[4] != 0 { EE_CP_HERD_NEW_LOCATION } else { EE_CP_HERD_NEW_LOCATION_AT };
                let goal = cpp(herd, op, a, 0, &mut q) as u64;
                i64::from(give_goal(herd, a, goal, io.v[3] != 0, ACTION_PREPARE_TO_MOVE))
            }
            EE_CP_HERD_BIRTH => {
                birth(herd, a, b);
                0
            }
            EE_CP_HERD_CANCEL_PATH => {
                cpp0(herd, EE_CP_HERD_CANCEL_REQUEST, a, 0);
                cpp0(herd, EE_CP_HERD_REQUEST_DEREF, a, 0);
                0
            }
            EE_CP_HERD_UNITS_IN_RECT => {
                units_in_rect(herd, io);
                0
            }
            _ => return area_op(herd, op, io),
        })
    }

    /// An area's operations of the random point (`ee_cp_area_host`'s) over the area's Rust record: its hull's
    /// bounds and test, the map. `None` for the C++ host's (the draws, the base point, the map's tile).
    ///
    /// # Safety
    /// `area` a live `EEAIArea*`.
    pub(crate) unsafe fn area_op(area: u64, op: i32, io: &mut EeHerdIo) -> Option<i64> {
        use crate::astar::cm_io;
        use crate::astar::hull::EeHullPoint;
        use herd::*;
        Some(match op {
            EE_CP_HERD_HULL_BOUNDS => {
                // SAFETY: the caller's contract
                let b = unsafe { cm_io::area(area) }.hull_bounds;
                io.v[0] = b.bottom as i64;
                io.v[1] = b.left as i64;
                io.v[2] = b.right as i64;
                io.v[3] = b.top as i64;
                0
            }
            EE_CP_HERD_HULL_CONTAINS => {
                let p = EeHullPoint::new(io.v[0] as c_long, io.v[1] as c_long);
                // SAFETY: as above
                i64::from(unsafe { crate::astar::area_ops::ee_astar_area_hull_contains(cm_io::area(area), p) })
            }
            EE_CP_HERD_HAS_MAP => {
                let t = unsafe { WorldRead::begin() };
                i64::from(WorldView::global(&t).is_some_and(|w| w.map().is_some()))
            }
            _ => return None,
        })
    }

    /// `EE_CP_HERD_UNIT`: the unit's record, from the world views (its waypoints and model from the C++).
    unsafe fn unit_record(herd: u64, handle: u64, io: &mut EeHerdIo) {
        let waypoints = cpp0(herd, EE_CP_HERD_UNIT_WAYPOINTS, handle, 0);
        let model = cpp0(herd, EE_CP_HERD_UNIT_MODEL, handle, 0) as u64;
        let r = &mut io.unit;
        unit(handle, |u| {
            let rts = u.rts();
            let c = u.complex();
            r.unit_id = world_field!(rts, mUnitID);
            r.handle = r.unit_id as u32;
            r.tile_x = world_field!(rts, mTileX);
            r.tile_y = world_field!(rts, mTileY);
            r.hit_points = world_field!(u.fields(), mHitPoints);
            r.growth = world_field!(c, mGrowth);
            r.graze_time = world_field!(c, mGrazeTime);
            r.grow_time = world_field!(c, mGrowTime);
            r.command = u.command_type() as c_long;
            r.waypoints = waypoints;
            r.held = u.held_unit_handles().len() as i64;
            r.holder = u.holder_handle();
            r.herd = u.herd_handle();
            r.player = u.player().map_or(0, |p| p.handle());
            r.last_attacker = u.last_attacker_handle();
            r.model = model;
            if let Some(k) = u.class() {
                let f = k.fields();
                r.attack = world_field!(f, mAttack);
                r.ammo_type = world_field!(f, mAmmoType);
                r.family = world_field!(f, mFamily);
                r.animal_id = world_field!(f, mAnimalID);
                r.can_be_attacked = !world_field!(f, mIsFlare);
            }
            r.disabled = world_field!(c, mIsDisabled);
            r.initialized = u.stats().is_some_and(|s| world_field!(s.fields(), mIsInitialized));
            r.has_goal = u.goal_handle() != 0;
            r.regrowing = world_field!(c, mIsRegrowing);
            r.b34f = world_field!(c, mB34F);
        });
    }

    /// `EE_CP_HERD_FORMATION`: an inward circle placed over the `input` units (Chase, MoveTo, Surround).
    unsafe fn formation(herd: u64, io: &mut EeHerdIo) {
        let f = cpp0(herd, EE_CP_HERD_FORM_NEW, 0, 0) as u64;
        let center = [io.f[0], io.f[1], io.f[2]];
        let set = |op: i32, x: &[f32]| cpp(herd, op, 0, f, &mut with_f(x));
        let random_radius = || {
            let mut q = with_f(&[1.25, 2.75]);
            cpp(herd, herd::EE_CP_HERD_RANDOM_FLOAT, 0, 0, &mut q);
            q.f[0]
        };
        let (input, input_len, list, list2) = (io.input, io.input_len, io.list, io.list2);
        let place = || {
            let mut q = EeHerdIo { input, input_len, list, list2, ..Default::default() };
            cpp(herd, EE_CP_HERD_FORM_PLACE, 0, f, &mut q);
        };
        match io.v[0] {
            0 => {
                // Chase (the original never released this formation)
                set(EE_CP_HERD_FORM_CENTER, &center);
                let r = random_radius();
                set(EE_CP_HERD_FORM_SET_MIN_RADIUS, &[r]);
                place();
            }
            1 => {
                // MoveTo
                if io.v[1] != 0 {
                    set(EE_CP_HERD_FORM_CENTER, &center);
                    set(EE_CP_HERD_FORM_MIN_RADIUS, &[io.f[3]]);
                    place();
                } else {
                    // nothing placed: the units as they came, no slots
                    for i in 0..input_len {
                        // SAFETY: the operation's input units
                        herd::ee_cp_herd_push(list2, unsafe { *input.add(i) } as i64, 0, 0, 0);
                    }
                }
                set(EE_CP_HERD_FORM_DEREF, &[]);
            }
            _ => {
                // Surround
                set(EE_CP_HERD_FORM_SET_CENTER, &center);
                let r = random_radius();
                set(EE_CP_HERD_FORM_SET_MIN_RADIUS, &[r]);
                place();
                set(EE_CP_HERD_FORM_DEREF, &[]);
            }
        }
    }

    /// `CanStandOn` (0xa1e75): walkable terrain, passable units and no other herd member (the `input` units) there.
    unsafe fn stand(herd: u64, handle: u64, io: &mut EeHerdIo) -> bool {
        let tile = cpp(herd, EE_CP_HERD_TILE_AT, 0, 0, &mut with_v(&[io.v[0], io.v[1]])) as u64;
        if tile == 0 {
            return false;
        }
        let (terrain, nodes) = {
            let t = unsafe { WorldRead::begin() };
            // SAFETY: the map's live tile
            let Some(v) = (unsafe { TileView::of(&t, tile) }) else { return false };
            (v.terrain_type(), v.units().map(|u| u.map_or(0, |u| u.handle())).collect::<Vec<u64>>())
        };
        let mut result = cpp(herd, EE_CP_HERD_CAN_MOVE_ON, handle, 0, &mut with_v(&[terrain])) != 0;
        // SAFETY: the operation's input units
        let members: &[u64] =
            if io.input_len == 0 { &[] } else { unsafe { std::slice::from_raw_parts(io.input, io.input_len) } };
        for node in nodes {
            if !result {
                break;
            }
            let owner = unit(node, |u| u.complex_unit_handle()).unwrap_or(0);
            result = cpp0(herd, EE_CP_HERD_PASSABLE_BY, handle, node) != 0;
            if result && owner != 0 && holder_of(handle) != owner && members.contains(&owner) && holder_of(owner) != handle {
                result = false;
            }
        }
        result
    }

    /// `GiveGoal` (0xa31b4): a live, enabled, placed animal takes the goal and starts the action; else the goal is
    /// deleted.
    unsafe fn give_goal(herd: u64, handle: u64, goal: u64, keep: bool, action: i64) -> bool {
        let ok = unit(handle, |u| {
            let hp: c_long = world_field!(u.fields(), mHitPoints);
            let x: c_long = world_field!(u.rts(), mTileX);
            hp > 0 && !world_field!(u.complex(), mIsDisabled) && x != -10000
        })
        .unwrap_or(false);
        if ok {
            cpp(herd, EE_CP_HERD_SET_GOAL, handle, goal, &mut with_v(&[i64::from(keep)]));
            cpp(herd, EE_CP_HERD_START_ACTION, handle, 0, &mut with_v(&[action]));
            cpp0(herd, EE_CP_HERD_ACTION_CHANGED, handle, 0);
            return true;
        }
        if goal != 0 {
            cpp0(herd, EE_CP_HERD_DELETE_GOAL, 0, goal);
        }
        false
    }

    /// `GiveBirth`'s newborn (0xa3d9c): the animal's class made at the parent's tile for its player, its growth,
    /// grow time and search timer, held by the parent.
    unsafe fn birth(herd: u64, parent: u64, animal: u64) {
        let class = cpp0(herd, EE_CP_HERD_ANIMAL_CLASS, animal, 0) as u64;
        let (x, y, player) = unit(parent, |u| {
            let (x, y) = u.tile_xy();
            (x, y, u.player().map_or(0, |p| i64::from(p.id())))
        })
        .unwrap_or((0, 0, 0));
        let new = cpp(herd, EE_CP_HERD_CREATE_UNIT, class, 0, &mut with_v(&[x, y, player])) as u64;
        let young = cpp0(herd, EE_CP_HERD_TO_COMPLEX, new, 0) as u64;
        let mut q = EeHerdIo::default();
        cpp(herd, EE_CP_HERD_NEWBORN_GROWTH, animal, 0, &mut q);
        cpp(herd, herd::EE_CP_HERD_SET_GROWTH, young, 0, &mut q);
        let (now, search) = {
            let t = unsafe { WorldRead::begin() };
            let now = WorldView::global(&t).map_or(0, |w| w.game_time()) as c_ulong;
            // SAFETY: the animal's live class
            let search: c_long =
                unsafe { ClassView::of(&t, class) }.map_or(0, |c| world_field!(c.fields(), mTargetSearchInterval));
            (now, search)
        };
        cpp(herd, herd::EE_CP_HERD_SET_GROW_TIME, young, 0, &mut with_v(&[now.wrapping_add(4000) as i64]));
        cpp(herd, EE_CP_HERD_SET_TARGET_SEARCH, young, 0, &mut with_v(&[search as i64]));
        cpp0(herd, EE_CP_HERD_HOLD_UNIT, parent, young);
    }

    /// `EE_CP_HERD_UNITS_IN_RECT`: `EEGetUnitsInRectangle(rect, units, filter, false)` (not made unique) over the
    /// map's unit regions, each region's units from its last.
    unsafe fn units_in_rect(herd: u64, io: &mut EeHerdIo) {
        let (bottom, left, right, top) = (io.v[0] as i32, io.v[1] as i32, io.v[2] as i32, io.v[3] as i32);
        let continent = io.v[4] as u16;
        let (huntable, moving) = (io.v[5] & 1 != 0, io.v[5] & 2 != 0);
        let mut dims = EeHerdIo::default();
        cpp(herd, herd::EE_CP_HERD_MAP_SIZE, 0, 0, &mut dims);
        let (width, height) = (dims.v[0] as c_ulong, dims.v[1] as c_ulong);
        let candidates: Vec<u64> = {
            let t = unsafe { WorldRead::begin() };
            let Some(map) = WorldView::global(&t).and_then(|w| w.map()) else { return };
            // SAFETY: the map's live regions table
            let Some(regions) = (unsafe { map.unit_regions().as_ref() }) else { return };
            let (l, tp, r, b) = regions.rect(left, top, right, bottom);
            let mut out = Vec::new();
            for x in l..=r {
                for y in tp..=b {
                    if let Some(region) = regions.region(x as u32, y as u32) {
                        out.extend(region.iter().rev().copied());
                    }
                }
            }
            out
        };
        for u in candidates {
            let facts = unit(u, |v| {
                let (x, y) = v.tile_xy();
                let c = v.class();
                let f = FilterUnit {
                    on_map: (x as c_long as c_ulong) < width && (y as c_long as c_ulong) < height,
                    domains: c.map_or(0, |c| world_field!(c.fields(), mDomains)),
                    unit_type: c.map_or(0, |c| c.unit_type()) as c_ulong,
                    category: c.map_or(0, |c| c.category()) as c_ulong,
                    player_id: v.player().map_or(0, |p| p.id()),
                    hit_points: v.hit_points() as c_long,
                };
                (x, y, f)
            });
            let Some((x, y, f)) = facts else { continue };
            if x < i64::from(left) || x > i64::from(right) || y < i64::from(top) || y > i64::from(bottom) {
                continue;
            }
            let unit_continent = || {
                crate::astar::world_view::cm().map_or(0, |r| {
                    if (x as c_ulong) < r.width && (y as c_ulong) < r.height {
                        crate::astar::cm_io::continent_id_at(r, x as c_long, y as c_long)
                    } else {
                        0
                    }
                })
            };
            let waypoints = || cpp0(herd, EE_CP_HERD_UNIT_WAYPOINTS, u, 0);
            if herd_filter(huntable, moving, continent, &f, unit_continent, waypoints) {
                herd::ee_cp_herd_push(io.list, u as i64, 0, 0, 0);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn boat() -> FilterUnit {
        FilterUnit { on_map: true, domains: 4, unit_type: 0x6c, category: 1, player_id: 2, hit_points: 5 }
    }

    #[test]
    fn the_filter_takes_living_sea_units_of_its_kind() {
        let b = boat();
        assert!(herd_filter(true, false, 0, &b, || unreachable!(), || unreachable!()));
        assert!(herd_filter(true, false, 0, &FilterUnit { unit_type: 0xd3, ..b }, || 0, || 0));
        assert!(!herd_filter(true, false, 0, &FilterUnit { unit_type: 0x6e, ..b }, || 0, || 0));
        assert!(!herd_filter(true, false, 0, &FilterUnit { domains: 1, ..b }, || 0, || 0));
        assert!(!herd_filter(true, false, 0, &FilterUnit { on_map: false, ..b }, || 0, || 0));
        assert!(!herd_filter(true, false, 0, &FilterUnit { hit_points: 0, ..b }, || 0, || 0));
        // not huntable: any non-gaia ship but submarines and sea animals
        assert!(herd_filter(false, false, 0, &FilterUnit { unit_type: 9, ..b }, || 0, || 0));
        assert!(!herd_filter(false, false, 0, &FilterUnit { player_id: 0, ..b }, || 0, || 0));
        assert!(!herd_filter(false, false, 0, &FilterUnit { category: 0x1b, ..b }, || 0, || 0));
    }

    #[test]
    fn the_filter_asks_the_continent_and_the_waypoints_only_when_needed() {
        let b = boat();
        assert!(herd_filter(true, true, 3, &b, || 3, || 2));
        assert!(!herd_filter(true, true, 3, &b, || 4, || unreachable!()));
        assert!(!herd_filter(true, true, 0, &b, || unreachable!(), || 1));
    }
}
