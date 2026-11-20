//! The path manager's host operations that read the world (`EEPathManagerHost` and `EEPathBoundedHost` of
//! EEPathManager.cpp, AI Thread cluster round 6): the units' ids, tiles, players and classes through the world
//! views (`crate::world`), the continent manager's sub sectors from its record, the searches' views, the blocked
//! tiles of a sea walker (`EEAIPathBlockerCollector`), the capture's records. The operations that change a unit, its
//! goals or a request stay the C++ host's (`raw`).
//!
//! [`answer`] is asked before the C++ host for every operation of the manager and its searches (pm.rs `FnHost`);
//! `None` hands the operation to the C++.

use std::ffi::{c_long, c_ulong, c_void};

use crate::astar::cm::{EeAStarCmRecord, EeAStarCmTable};
use crate::astar::continent_grid::EePoint;
use crate::astar::graph_search::{
    EE_PATH_GRAPH_ENDS, EE_PATH_GRAPH_OBJECT, EE_PATH_GRAPH_REVS_WORLD, EE_PATH_SS_INFO, EE_PATH_SS_IS_NEIGHBOR,
    EE_PATH_SS_NEIGHBOR,
};
use crate::astar::path_search::{
    EE_PATH_BLOCKED_TILES, EE_PATH_GOAL_UNIT, EE_PATH_UNIT_TILES, EE_PATH_VIEW_FREE, EE_PATH_VIEW_NEW,
};
use crate::astar::pm::*;
use crate::astar::pm_bounded::{
    EE_PATH_CAPTURE_ACTIVE, EE_PATH_CAPTURE_COMMIT, EE_PATH_MAP_SIZE, EE_PATH_UNIT_LOCATION, EE_PATH_UNIT_TILE,
};
use crate::world::{UnitView, WorldRead, WorldView};

/// `kMovementSea`
const MOVEMENT_SEA: u8 = 4;
/// `kUnitCategory*`: the walkers below the water, a naval building
const CATEGORY_SUBMERGED_NAVAL: u64 = 0xa;
const CATEGORY_NAVAL_BUILDING: u64 = 0x15;
const CATEGORY_SEA_ANIMAL: u64 = 0x1b;
/// `EEUnitClass::IsTransport`'s categories
const TRANSPORT_CATEGORIES: [u64; 5] = [0x10, 0x11, 0x12, 0x21, 0x1d];

fn token() -> WorldRead {
    // SAFETY: the manager's calls run on the simulation thread, which holds the world (the C++ host read it so)
    unsafe { WorldRead::begin() }
}

fn unit<'t>(t: &'t WorldRead, h: u64) -> Option<UnitView<'t>> {
    // SAFETY: the host's unit handles are live `EEComplexUnit*`s
    unsafe { UnitView::of(t, h) }
}

fn cm_table(t: EeAStarCmTable) -> &'static [u64] {
    match crate::astar::world_view::cm() {
        Some(r) => &r.tables().tables[t as usize],
        None => &[],
    }
}

fn cm_record() -> Option<&'static EeAStarCmRecord> {
    crate::astar::world_view::cm()
}

/// `U2DPoint<long>::DistanceTo`: the root of the squared distance, truncated.
pub(crate) fn tile_distance(from: (i64, i64), to: (i64, i64)) -> i64 {
    let dx = (from.0 - to.0) as i32;
    let dy = (from.1 - to.1) as i32;
    let d = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy));
    (d as f32).sqrt() as i64
}

/// The submerged walkers' kind (`IsAirCategory`).
fn submerged(category: u64) -> bool {
    category == CATEGORY_SUBMERGED_NAVAL || category == CATEGORY_SEA_ANIMAL
}

/// A contact of a sea walker as `EEAIPathBlockerCollector` tests it.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub(crate) struct Contact {
    pub handle: u64,
    pub velocity: f32,
    pub domains: u8,
    pub category: u64,
    pub active_group: u64,
    pub air_traffic_type: i64,
    pub sphere: f32,
    pub push_immune_until: u64,
}

/// `EEAIPathBlockerCollector::Add` up to its tile pass: whether the contact's tiles are looked at for `walker`
/// (its handle, group, naval kind).
pub(crate) fn blocks(c: &Contact, walker: u64, walker_group: u64, naval: bool) -> bool {
    if c.velocity != 0.0 || c.domains & MOVEMENT_SEA == 0 || c.handle == walker {
        return false;
    }
    if submerged(c.category) != naval {
        return false;
    }
    walker_group == 0
        || walker_group != c.active_group
        || (c.category == CATEGORY_NAVAL_BUILDING && c.air_traffic_type != 0)
}

/// `AddTiles`' size test: at least as big as the walker, or immune to pushes now.
pub(crate) fn big_enough(c: &Contact, walker_sphere: f32, game_time: u64) -> bool {
    c.sphere >= walker_sphere || c.push_immune_until > game_time
}

/// `AddTiles`' tile pass: the contact's tiles, last first, but the goal and the walker's own blocked tiles.
pub(crate) fn new_blocked(tiles: &[(i64, i64)], goal: (i64, i64), blocked: &[(i64, i64)], out: &mut Vec<(i64, i64)>) {
    for &t in tiles.iter().rev() {
        if t != goal && !blocked.contains(&t) {
            out.push(t);
        }
    }
}

fn contact_of(u: &UnitView<'_>) -> Contact {
    let c = u.class();
    let f = c.map(|c| c.fields());
    Contact {
        handle: u.handle(),
        velocity: crate::world_field!(u.complex(), mVelocity),
        domains: f.map_or(0, |f| crate::world_field!(f, mDomains)),
        category: c.map_or(0, |c| c.category()),
        active_group: u.active_group_handle(),
        air_traffic_type: f.map_or(0, |f| {
            let v: c_long = crate::world_field!(f, mAirTrafficType);
            v as i64
        }),
        sphere: f.map_or(0.0, |f| crate::world_field!(f, mInnerCollisionSphere)),
        push_immune_until: {
            let v: c_ulong = crate::world_field!(u.complex(), mPushImmuneUntil);
            v as u64
        },
    }
}

/// `EEGetUnitTiles(unit, tiles, margin)` with an empty list, its UUNT note as the C++ call made it.
#[cfg(not(test))]
fn unit_tiles(unit: u64, margin: f32) -> Vec<(i64, i64)> {
    use crate::units::uu_note::Note;
    let n = Note::begin(53);
    let mut tiles: Vec<crate::units::uu_tiles::Tile> = Vec::new();
    n.u(unit).f(margin).t(&tiles);
    crate::units::uu_world::unit_tiles(&mut crate::units::uu_host::Game, unit, &mut tiles, margin);
    n.t(&tiles);
    tiles.iter().map(|t| (i64::from(t[0]), i64::from(t[1]))).collect()
}

#[cfg(test)]
fn unit_tiles(_unit: u64, _margin: f32) -> Vec<(i64, i64)> {
    Vec::new()
}

fn push_tiles(list: u64, tiles: &[(i64, i64)]) {
    // SAFETY: the operation's list is a live `Vec<EePoint>` (ee_astar_tiles_push's)
    let v = unsafe { &mut *(list as *mut Vec<EePoint>) };
    v.extend(tiles.iter().map(|&(x, y)| EePoint { x: x as i32, y: y as i32 }));
}

/// `CollectBlockedTiles`: the walker's bounded path tiles and, for a sea walker, its stopped contacts' tiles near
/// the goal (`i0`, `i1`; `i2` the wide margin).
fn blocked_tiles(t: &WorldRead, walker: &UnitView<'_>, io: &EePathIo) -> Vec<(i64, i64)> {
    let mut blocked = walker.bounded_path_tiles();
    let wc = walker.class();
    let domains: u8 = wc.map_or(0, |c| crate::world_field!(c.fields(), mDomains));
    if domains & MOVEMENT_SEA == 0 {
        return blocked;
    }
    let naval = submerged(wc.map_or(0, |c| c.category()));
    let sphere: f32 = wc.map_or(0.0, |c| crate::world_field!(c.fields(), mInnerCollisionSphere));
    let goal = (io.i[0], io.i[1]);
    let margin = if io.i[2] == 0 { 1.0 } else { 2.0 };
    let game_time = WorldView::global(t).map_or(0, |w| w.game_time()) as c_long as c_ulong as u64;
    let group = walker.active_group_handle();
    let mut found = Vec::new();
    for h in walker.contacts().1 {
        let Some(u) = unit(t, h) else { continue };
        let c = contact_of(&u);
        if blocks(&c, walker.handle(), group, naval) && big_enough(&c, sphere, game_time) {
            new_blocked(&unit_tiles(h, margin), goal, &blocked, &mut found);
        }
    }
    blocked.extend(found);
    blocked
}

fn can_move_on_terrain(u: &UnitView<'_>, terrain: i32) -> bool {
    let Some(c) = u.class() else { return false };
    let stats = u.stats().map(|s| s.fields());
    // SAFETY: live records of the unit's class and stats
    unsafe { crate::units::cls_answers::can_move_on_terrain(&*c.fields().ptr(), stats.map(|s| &*s.ptr()), terrain) }
}

/// The manager's `GetContinentAt(x, y)`: the continent object, 0 for none.
fn continent_at(x: i64, y: i64) -> u64 {
    let Some(r) = cm_record() else { return 0 };
    match crate::astar::cm_io::continent_id_at(r, x as c_long, y as c_long) {
        0 => 0,
        id => cm_table(EeAStarCmTable::Continents).get(id as usize).copied().unwrap_or(0),
    }
}

/// The manager's `GetPathSubSectorAt(x, y)`: 0 for none.
fn sub_sector_at(x: i64, y: i64) -> u64 {
    let Some(r) = cm_record() else { return 0 };
    match crate::astar::cm_io::sub_sector_id_at(r, x as c_long, y as c_long) {
        0 => 0,
        id => cm_table(EeAStarCmTable::PathSubSectors).get(id as usize).copied().unwrap_or(0),
    }
}

/// `EEGraphObjectOf` / `EESubSectorOf`: sub sector `id`, 0 for none.
fn sub_sector(id: i64) -> u64 {
    let table = cm_table(EeAStarCmTable::PathSubSectors);
    if id < 0 || id as usize >= table.len() {
        0
    } else {
        table[id as usize]
    }
}

/// The sub sector's sector's center.
fn sector_center(sub: u64) -> (i64, i64) {
    // SAFETY: a listed sub sector and its sector are live objects
    let c = unsafe { crate::astar::cm_io::sector(crate::astar::cm_io::sub(sub).sector).center };
    (c.x as i64, c.y as i64)
}

/// The unit id of a handle (`EEUnitHandleID`): reported once when the unit has none.
fn unit_id(t: &WorldRead, h: u64) -> i64 {
    let id = unit(t, h).map_or(u32::MAX, |u| u.id());
    if id == u32::MAX {
        static REPORTED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);
        if !REPORTED.swap(true, std::sync::atomic::Ordering::Relaxed) {
            eprintln!("EE-HANDLE: a handle to a unit without an id ({h:#x}): it resolves to nothing");
        }
    }
    i64::from(id)
}

#[cfg(not(test))]
fn resolve(id: u32) -> u64 {
    if id == u32::MAX {
        0
    } else {
        crate::units::ffi::ee_units_resolve(id) as u64
    }
}

#[cfg(test)]
fn resolve(_id: u32) -> u64 {
    0
}

#[cfg(not(test))]
fn reference(unit: u64, add: bool) {
    // SAFETY: a live unit (the C++ host's Reference / Dereference)
    unsafe { crate::astar::modifier_obj::ee_astar_mod_host_unit_reference(unit, add) }
}

#[cfg(test)]
fn reference(_unit: u64, _add: bool) {}

/// One host operation answered in Rust, else `None` (the C++ host's). `raw` is the C++ host, for the parts an
/// answer asks of it (a tile moved onto the map).
pub fn answer(op: i32, io: &mut EePathIo, raw: &mut dyn FnMut(i32, &mut EePathIo) -> i64) -> Option<i64> {
    let t = token();
    let h = io.h[0];
    let r = match op {
        EE_PATH_UNIT_ID => unit_id(&t, h),
        EE_PATH_UNIT_RESOLVE => resolve(io.i[0] as u32) as i64,
        EE_PATH_UNIT_REFERENCE | EE_PATH_UNIT_DEREFERENCE => {
            reference(h, op == EE_PATH_UNIT_REFERENCE);
            0
        }
        EE_PATH_GAME_TIME => WorldView::global(&t).map_or(0, |w| i64::from(w.game_time() as u32)),
        EE_PATH_SUB_SECTORS => cm_table(EeAStarCmTable::PathSubSectors).len() as i64,
        EE_PATH_SUB_SECTOR => i64::from(sub_sector(io.i[0]) != 0),
        EE_PATH_MAP => WorldView::global(&t).and_then(|w| w.map()).map_or(0, |m| m.handle() as i64),
        EE_PATH_HAS_MAP => i64::from(WorldView::global(&t).and_then(|w| w.map()).is_some()),
        EE_PATH_LOADING => i64::from(WorldView::global(&t).is_some_and(|w| w.loading_saved_game())),
        EE_PATH_PATH_REVS => {
            let u = unit(&t, h)?;
            let (from, to) = ((io.i[0], io.i[1]), (io.i[2], io.i[3]));
            let same = continent_at(from.0, from.1) == continent_at(to.0, to.1);
            io.i[0] = tile_distance(from, to);
            io.i[1] = i64::from(can_move_on_terrain(&u, 0) && can_move_on_terrain(&u, 1));
            io.i[2] = i64::from(same);
            0
        }
        EE_PATH_UNIT_PLAYER => unit(&t, h)?.player().map_or(0, |p| i64::from(p.id())),
        EE_PATH_UNIT_PATH_TYPE => {
            let u = unit(&t, h)?;
            if can_move_on_terrain(&u, 1) {
                2
            } else if can_move_on_terrain(&u, 0xc) {
                0x10
            } else {
                1
            }
        }
        EE_PATH_REGION_DELAY => {
            let speed = WorldView::global(&t).map_or(0.0, |w| w.game_speed());
            i64::from(ee_base::math::um::lrintf(speed * 10000.0) as u32)
        }
        EE_PATH_VIEW_NEW => {
            let view = Box::into_raw(Box::new(crate::astar::world_view::EeAStarWorldView {
                map: io.h[1] as *mut c_void,
                quiet: false,
            }));
            // SAFETY: `h2` is the search's callbacks slot; the view lives until EE_PATH_VIEW_FREE
            unsafe {
                *(io.h[2] as *mut crate::astar::ffi::EeAStarViewCallbacks) =
                    crate::astar::world_view::ee_astar_world_view_callbacks(view)
            };
            view as i64
        }
        EE_PATH_VIEW_FREE => {
            if h != 0 {
                // SAFETY: made by EE_PATH_VIEW_NEW
                drop(unsafe { Box::from_raw(h as *mut crate::astar::world_view::EeAStarWorldView) });
            }
            0
        }
        EE_PATH_GOAL_UNIT => {
            let u = unit(&t, h)?;
            let (x, y) = u.tile_xy();
            let on = cm_record().is_some_and(|r| (x as c_ulong) < r.width && (y as c_ulong) < r.height);
            io.i[0] = if on {
                cm_record().map_or(0, |r| i64::from(crate::astar::cm_io::continent_id_at(r, x as c_long, y as c_long)))
            } else {
                0
            };
            let computer = u.player().is_some_and(|p| p.has_computer_player());
            let transport = u.class().is_some_and(|c| TRANSPORT_CATEGORIES.contains(&c.category()));
            io.i[1] = i64::from(!(computer && !transport));
            0
        }
        EE_PATH_BLOCKED_TILES => {
            let u = unit(&t, h)?;
            let tiles = blocked_tiles(&t, &u, io);
            push_tiles(io.h[1], &tiles);
            0
        }
        EE_PATH_UNIT_TILES => {
            let u = unit(&t, h)?;
            let sea = u.class().is_some_and(|c| {
                let d: u8 = crate::world_field!(c.fields(), mDomains);
                d & MOVEMENT_SEA != 0
            });
            if sea {
                push_tiles(io.h[1], &unit_tiles(h, 0.0));
            }
            0
        }
        EE_PATH_SS_INFO => {
            // SAFETY: a listed sub sector is a live object
            let s = unsafe { crate::astar::cm_io::sub(h) };
            // SAFETY: its lists live with it
            let lists = unsafe { &*s.lists };
            let c = sector_center(h);
            io.i[0] = i64::from(s.id);
            io.i[1] = s.kind as i64;
            io.i[2] = lists.neighbors.len() as i64;
            io.i[3] = c.0;
            io.i[4] = c.1;
            io.i[5] = lists.links.len() as i64;
            io.i[6] = s.sector as i64;
            0
        }
        EE_PATH_SS_NEIGHBOR => {
            // SAFETY: as above
            let lists = unsafe { &*crate::astar::cm_io::sub(h).lists };
            lists.neighbors[io.i[0] as usize] as i64
        }
        EE_PATH_SS_IS_NEIGHBOR => {
            // SAFETY: as above
            let lists = unsafe { &*crate::astar::cm_io::sub(h).lists };
            i64::from(lists.neighbors.contains(&io.h[1]))
        }
        EE_PATH_GRAPH_OBJECT => sub_sector(io.i[0]) as i64,
        crate::astar::graph_search::EE_PATH_REQUEST_FLAGS => {
            // the goal's flags = the request's (`EECPASGFlags::operator=`), from the request's record
            let mut r = RequestRecord::default();
            let mut c = EePathIo::default();
            c.h[0] = h;
            c.h[1] = &mut r as *mut RequestRecord as u64;
            raw(EE_PATH_REQUEST_SAVE, &mut c);
            // SAFETY: `h1` is the search's live goal record
            let goal = unsafe { &mut *(io.h[1] as *mut crate::astar::graph_search::EeAStarGraphGoalRec) };
            goal.flags_player = r.r.flags_player;
            goal.path_flags = r.r.flags;
            0
        }
        EE_PATH_GRAPH_ENDS => {
            for k in 0..2 {
                let mut c = EePathIo::default();
                c.i[0] = io.i[2 * k];
                c.i[1] = io.i[2 * k + 1];
                raw(EE_PATH_CLAMP, &mut c);
                let (x, y) = (c.i[0], c.i[1]);
                io.i[2 * k] = x;
                io.i[2 * k + 1] = y;
                let s = sub_sector_at(x, y);
                io.h[k] = s;
                let center = sector_center(s);
                io.i[4 + 2 * k] = center.0;
                io.i[5 + 2 * k] = center.1;
                // SAFETY: as above
                io.i[8 + k] = i64::from(unsafe { crate::astar::cm_io::sub(s).id });
            }
            0
        }
        EE_PATH_GRAPH_REVS_WORLD => {
            let (from, to) = ((io.i[0], io.i[1]), (io.i[2], io.i[3]));
            let s = sub_sector_at(to.0, to.1);
            io.i[0] = tile_distance(from, to);
            // SAFETY: as above
            io.i[1] = unsafe { crate::astar::cm_io::sub(s).kind } as i64;
            io.i[2] = match unit(&t, h) {
                Some(u)
                    if u.player().is_some_and(|p| p.has_computer_player())
                        && crate::world_field!(u.complex(), mIsCitizen) =>
                {
                    i64::from(u.id())
                }
                _ => i64::from(u32::MAX),
            };
            0
        }
        EE_PATH_MAP_SIZE => {
            // SAFETY: the operation's map is the live map of the search
            unsafe { crate::world::MapView::of(&t, io.h[1]) }.map_or(0, |m| m.size())
        }
        EE_PATH_UNIT_LOCATION => {
            let p = unit(&t, h)?.location();
            io.i[0] = i64::from(p[0].to_bits());
            io.i[1] = i64::from(p[1].to_bits());
            0
        }
        EE_PATH_UNIT_TILE => {
            let (x, y) = unit(&t, h)?.tile_xy();
            io.i[0] = x;
            io.i[1] = y;
            0
        }
        EE_PATH_CAPTURE_ACTIVE => i64::from(crate::app::capture_record::active()),
        EE_PATH_CAPTURE_COMMIT => {
            // SAFETY: `h1` holds the record's `i1` bytes
            let bytes = unsafe { std::slice::from_raw_parts(io.h[1] as *const u8, io.i[1] as usize) };
            crate::app::capture_record::commit(io.i[0] as u32, bytes);
            0
        }
        _ => return None,
    };
    Some(r)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_distance_is_the_truncated_root() {
        assert_eq!(tile_distance((0, 0), (3, 4)), 5);
        assert_eq!(tile_distance((10, 10), (11, 12)), 2);
        assert_eq!(tile_distance((5, 5), (5, 5)), 0);
        assert_eq!(tile_distance((0, 0), (-7, 0)), 7);
    }

    fn sea(handle: u64) -> Contact {
        Contact { handle, domains: MOVEMENT_SEA, category: 1, sphere: 1.0, ..Default::default() }
    }

    #[test]
    fn a_blocker_is_a_stopped_sea_unit_of_the_walkers_kind_outside_its_group() {
        let c = sea(2);
        assert!(blocks(&c, 1, 0, false));
        assert!(!blocks(&c, 2, 0, false), "the walker itself");
        assert!(!blocks(&Contact { velocity: 0.5, ..c }, 1, 0, false), "moving");
        assert!(!blocks(&Contact { domains: 1, ..c }, 1, 0, false), "not at sea");
        assert!(!blocks(&c, 1, 0, true), "a submerged walker");
        assert!(blocks(&Contact { category: CATEGORY_SEA_ANIMAL, ..c }, 1, 0, true));
        let grouped = Contact { active_group: 9, ..c };
        assert!(!blocks(&grouped, 1, 9, false), "the walker's group");
        assert!(blocks(&grouped, 1, 8, false));
        let dock = Contact { category: CATEGORY_NAVAL_BUILDING, air_traffic_type: 1, ..grouped };
        assert!(blocks(&dock, 1, 9, false), "a carrier dock always");
    }

    #[test]
    fn small_blockers_count_only_while_push_immune() {
        let c = sea(2);
        assert!(big_enough(&c, 1.0, 100));
        assert!(!big_enough(&c, 2.0, 100));
        assert!(big_enough(&Contact { push_immune_until: 101, ..c }, 2.0, 100));
        assert!(!big_enough(&Contact { push_immune_until: 100, ..c }, 2.0, 100));
    }

    #[test]
    fn the_new_blocked_tiles_are_last_first_without_the_goal_or_known_ones() {
        let mut out = vec![(9, 9)];
        new_blocked(&[(1, 1), (2, 2), (3, 3), (4, 4)], (3, 3), &[(1, 1)], &mut out);
        assert_eq!(out, vec![(9, 9), (4, 4), (2, 2)]);
    }
}
