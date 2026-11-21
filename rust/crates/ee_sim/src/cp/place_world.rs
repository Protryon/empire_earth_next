//! The placement's and the centres' host operations answered over the world views and the continent manager's records
//! (AI Thread cluster round 7; were `ee_cp_place_host`'s and `ee_cp_center_host`'s multi-statement cases,
//! EECPPlacement.cpp and EECPResourceCenter.cpp): a unit's facts, the player's and nature's units, the towns, a tech
//! node's kind, the tiles, the continents' shore, hull and choke points, the sort, the morale effect, the houses in a
//! rectangle, `EEComplexUnit::DistanceSquaredToUnit`. Asked before the C++ hosts by every caller
//! ([`super::place_game::host_call`]); the C++ calls left are one host case each, in the original's order. Absent
//! from a test binary but [`distance_squared_to`] and [`sort_order`].

// `c_long` is 32 bits on Windows and 64 elsewhere
#![allow(clippy::unnecessary_cast, clippy::useless_conversion)]

use std::os::raw::c_long;

/// the town `a`'s `EeTownFields` (`static_cast<EeTownFields*>(town)`)
pub const EE_CP_PLACE_TOWN_FIELDS: i32 = 60;
/// unit `a`'s morale-from-buildings effect (0: none); the effect `b`'s `GetRadius()` into `f0`, `MarkDirty()`,
/// `Recount(0)`, `GetLevel()`, `mTemplate->mMaxLevel`
pub const EE_CP_CENTER_MORALE_EFFECT: i32 = 116;
pub const EE_CP_CENTER_EFFECT_RADIUS: i32 = 117;
pub const EE_CP_CENTER_EFFECT_DIRTY: i32 = 118;
pub const EE_CP_CENTER_EFFECT_RECOUNT: i32 = 119;
pub const EE_CP_CENTER_EFFECT_LEVEL: i32 = 120;
pub const EE_CP_CENTER_EFFECT_MAX_LEVEL: i32 = 121;
/// the player's `mUnitClassUpgrades[v0]`
pub const EE_CP_CENTER_UPGRADE: i32 = 122;

/// `EEComplexUnit::DistanceSquaredToUnit` (0x...: EECPResourceCenter.cpp's): from this unit's location to the other's
/// (its complex unit's location, else its tile), `dx * dx + dy * dy` in f32 as the original.
pub fn distance_squared_to(from: [f32; 3], to: Result<[f32; 3], (c_long, c_long)>) -> f32 {
    let (dy, dx) = match to {
        Ok(p) => (from[1] - p[1], from[0] - p[0]),
        Err((x, y)) => (from[1] - y as f32, from[0] - x as f32),
    };
    dx * dx + dy * dy
}

/// `EE_CP_PLACE_SORT`: the original's `std::sort` of (key, index) pairs by the key (MSVC's, the same order of ties);
/// the indices in their sorted order.
pub fn sort_order(keys: &[i64]) -> Vec<u32> {
    let mut pairs: Vec<(i64, u32)> = keys.iter().enumerate().map(|(i, &k)| (k, i as u32)).collect();
    crate::units::msvc_sort::sort_by(&mut pairs, |a, b| a.0 < b.0);
    pairs.into_iter().map(|p| p.1).collect()
}

#[cfg(not(test))]
pub(crate) use game::*;

#[cfg(not(test))]
mod game {
    use std::os::raw::c_long;

    use super::super::place_game::{self as pg, EeCpPlaceCtx, EeCpPlaceIo};
    use super::*;
    use crate::astar::cm_io;
    use crate::world::{PlayerView, TileView, UnitView, WorldRead, WorldView};
    use crate::world_field;

    fn push(io: &EeCpPlaceIo, a: i64, b: i64) {
        // SAFETY: the operation's list
        unsafe { pg::ee_cp_place_push(io.list, a, b) }
    }

    fn input_pairs(io: &EeCpPlaceIo) -> Vec<(i64, i64)> {
        // SAFETY: the operation's `input_len` pairs
        (0..io.input_len).map(|i| unsafe { (*io.input.add(2 * i), *io.input.add(2 * i + 1)) }).collect()
    }

    fn map_tile(x: i64, y: i64) -> Option<u64> {
        let t = unsafe { WorldRead::begin() };
        WorldView::global(&t).and_then(|w| w.map()).and_then(|m| m.tile(x, y)).map(|t| t.handle())
    }

    fn cpp(ctx: *const EeCpPlaceCtx, op: i32, a: u64, b: u64, io: &mut EeCpPlaceIo) -> i64 {
        // SAFETY: the C++ hosts' one-statement cases on the live objects
        unsafe { pg::host_cpp(ctx, op, a, b, io) }
    }

    /// The placement's and centres' operations in Rust (`None`: the C++ host's).
    ///
    /// # Safety
    /// `ctx` a call's context; `a`, `b` live objects as the operation takes them.
    pub(crate) unsafe fn place_op(
        ctx: *const EeCpPlaceCtx,
        op: i32,
        a: u64,
        b: u64,
        io: &mut EeCpPlaceIo,
    ) -> Option<i64> {
        // SAFETY: the caller's contract
        let player = unsafe { (*ctx).player };
        let (v0, v1) = (io.v[0], io.v[1]);
        Some(match op {
            pg::EE_CP_PLACE_TECH_KIND => {
                let t = WorldRead::begin();
                let kind = PlayerView::of(&t, player).and_then(|p| p.tech_tree()).and_then(|tr| tr.node(v0)).map(|n| {
                    n.data().map_or(0, |d| {
                        let k: c_long = world_field!(d, mKind);
                        k as i64
                    })
                });
                match kind {
                    Some(k) => {
                        io.v[0] = k;
                        1
                    }
                    None => 0,
                }
            }
            pg::EE_CP_PLACE_OWN_UNITS => {
                let t = WorldRead::begin();
                let units: Vec<u64> =
                    PlayerView::of(&t, player).map_or(Vec::new(), |p| p.units().map(|u| u.handle()).collect());
                for u in units {
                    push(io, u as i64, 0);
                }
                0
            }
            pg::EE_CP_PLACE_NATURE_UNITS => {
                let t = WorldRead::begin();
                let Some(p) = WorldView::global(&t).and_then(|w| w.player(0)) else { return Some(0) };
                let units: Vec<u64> = p.units().map(|u| u.handle()).collect();
                for u in units {
                    push(io, u as i64, 0);
                }
                1
            }
            pg::EE_CP_PLACE_TOWNS => {
                let towns: Vec<u64> = {
                    let t = WorldRead::begin();
                    PlayerView::of(&t, player)
                        .map_or(Vec::new(), |p| (0..p.towns_len()).map(|i| p.town_handle(i)).collect())
                };
                for town in towns {
                    if town == 0 {
                        push(io, -1, 0);
                        continue;
                    }
                    let center = cm_io::area(town).hull_center;
                    let fields = cpp(ctx, EE_CP_PLACE_TOWN_FIELDS, town, 0, &mut pg::place_io()) as usize
                        as *const super::super::town::EeTownFields;
                    let continent = (*fields).mContinent;
                    push(io, i64::from(continent), ((center.x as u32 as u64) << 32 | center.y as u32 as u64) as i64);
                }
                0
            }
            pg::EE_CP_PLACE_UNIT_INFO => {
                let drop_site = {
                    let t = WorldRead::begin();
                    UnitView::of(&t, a).and_then(|u| u.class()).map(|c| c.handle())
                };
                let drop_site = drop_site.is_some_and(|c| {
                    cpp(ctx, super::super::player_game::EE_CP_PLAYER_CLASS_IS_DROP_SITE, c, 0, &mut pg::place_io()) != 0
                });
                let t = WorldRead::begin();
                let Some(u) = UnitView::of(&t, a) else { return Some(0) };
                let i = &mut io.unit;
                let (x, y) = u.tile_xy();
                i.tile = [x as i32, y as i32];
                i.id = u.id();
                let class = u.class();
                i.has_class = u8::from(class.is_some());
                if let Some(c) = class {
                    let f = c.fields();
                    i.unit_type = c.unit_type() as i32;
                    i.tile_size = world_field!(f, mTileSize) as i32;
                    i.category = c.category() as i32;
                    i.family = c.family() as i32;
                    i.technology = c.technology() as i32;
                    i.land_building = u8::from(c.land_building());
                    i.wall = u8::from(world_field!(f, mIsWall));
                    i.drop_site = u8::from(drop_site);
                }
                i.hit_points = u.hit_points() as i32;
                i.resource_amount = u.resource_amount() as i32;
                i.resource_type = u.resource_type() as i32;
                i.valid = u8::from(u.valid());
                i.has_tile = u8::from(u.tile().is_some());
                i.has_tile_node = u8::from(u.has_tile_node());
                i.player = u.player().map_or(0, |p| p.handle());
                i.complex = u.complex_unit_handle();
                if let Some(c) = u.complex_unit() {
                    let (cx, cy) = c.tile_xy();
                    i.complex_tile = [cx as i32, cy as i32];
                    i.complex_hit_points = c.hit_points() as i32;
                    i.complex_built = u8::from(c.is_built());
                    i.facing = c.facing() as u32;
                    i.previous_part = c.previous_part_handle();
                    i.next_part = c.next_part_handle();
                }
                0
            }
            pg::EE_CP_PLACE_TILE_TERRAIN | pg::EE_CP_PLACE_MAP_TILE_TERRAIN => {
                let Some(tile) = map_tile(v0, v1) else { return Some(-1) };
                let t = WorldRead::begin();
                TileView::of(&t, tile).map_or(-1, |t| t.terrain_type())
            }
            pg::EE_CP_PLACE_TILE_FREE => {
                let Some(tile) = map_tile(v0, v1) else { return Some(-1) };
                let t = WorldRead::begin();
                i64::from(TileView::of(&t, tile).is_some_and(|t| t.units().next().is_none()))
            }
            pg::EE_CP_PLACE_SHORE_NEXT_TO => {
                extern "C" fn add(list: *mut std::ffi::c_void, x: c_long, y: c_long) {
                    // SAFETY: the vector below
                    unsafe { (*(list as *mut Vec<(i64, i64)>)).push((x as i64, y as i64)) }
                }
                let mut tiles = input_pairs(io);
                crate::astar::area_shore::ee_astar_continent_shore_next_to(
                    a,
                    v0 as u16,
                    &mut tiles as *mut Vec<(i64, i64)> as *mut std::ffi::c_void,
                    add,
                );
                for (x, y) in tiles {
                    push(io, x, y);
                }
                0
            }
            pg::EE_CP_PLACE_SHORE_TILES => {
                let mut n = 0;
                let tiles = crate::astar::continent::ee_astar_continent_shore(cm_io::continent(a), &mut n);
                let tiles: Vec<(i64, i64)> = (0..n).map(|k| *tiles.add(k)).map(|p| (p.x as i64, p.y as i64)).collect();
                for (x, y) in tiles {
                    push(io, x, y);
                }
                0
            }
            pg::EE_CP_PLACE_HULL_CENTER => {
                let c = cm_io::area(a).hull_center;
                io.v[0] = c.x as i64;
                io.v[1] = c.y as i64;
                0
            }
            pg::EE_CP_PLACE_CHOKE_POINTS => {
                extern "C" fn add(list: *mut std::ffi::c_void, choke: u64) {
                    // SAFETY: the vector below
                    unsafe { (*(list as *mut Vec<u64>)).push(choke) }
                }
                let Some(r) = crate::astar::world_view::cm() else { return Some(0) };
                let mut chokes: Vec<u64> = Vec::new();
                crate::astar::cm_queries::ee_astar_cmq_choke_points(
                    r,
                    v0 as u16,
                    true,
                    std::ptr::null(),
                    0,
                    &mut chokes as *mut Vec<u64> as *mut std::ffi::c_void,
                    add,
                );
                for c in chokes {
                    push(io, c as i64, 0);
                }
                0
            }
            pg::EE_CP_PLACE_HULL_BOUNDS => {
                let b = cm_io::area(a).hull_bounds;
                io.v[0] = b.left as i64;
                io.v[1] = b.top as i64;
                io.v[2] = b.right as i64;
                io.v[3] = b.bottom as i64;
                0
            }
            pg::EE_CP_PLACE_CHOKE_TILES => {
                let mut n = 0;
                let points = ee_base::sparse::ee_sparse_points(cm_io::area_tiles(a), false, 1, &mut n);
                let points: Vec<(i64, i64)> =
                    (0..n).map(|k| *points.add(k)).map(|p| (p.x as i64, p.y as i64)).collect();
                for (x, y) in points {
                    push(io, x, y);
                }
                0
            }
            pg::EE_CP_PLACE_SORT => {
                // SAFETY: the operation's keys and its order buffer of as many
                let keys = if io.input_len == 0 { &[][..] } else { std::slice::from_raw_parts(io.input, io.input_len) };
                for (i, k) in sort_order(keys).into_iter().enumerate() {
                    *io.order.add(i) = k;
                }
                0
            }
            pg::EE_CP_PLACE_UNITS_IN_RECT => units_in_rect(player, io),
            pg::EE_CP_CENTER_DISTANCE_TO_UNIT => {
                io.f[0] = distance_squared_to_unit(a, b);
                0
            }
            pg::EE_CP_CENTER_MORALE_RADIUS => {
                let e = cpp(ctx, EE_CP_CENTER_MORALE_EFFECT, a, 0, &mut pg::place_io()) as u64;
                if e == 0 {
                    return Some(0);
                }
                let mut q = pg::place_io();
                cpp(ctx, EE_CP_CENTER_EFFECT_RADIUS, 0, e, &mut q);
                io.f[0] = q.f[0];
                1
            }
            pg::EE_CP_CENTER_MORALE_RECOUNT => {
                let e = cpp(ctx, EE_CP_CENTER_MORALE_EFFECT, a, 0, &mut pg::place_io()) as u64;
                if e == 0 {
                    return Some(0);
                }
                cpp(ctx, EE_CP_CENTER_EFFECT_DIRTY, 0, e, &mut pg::place_io());
                cpp(ctx, EE_CP_CENTER_EFFECT_RECOUNT, 0, e, &mut pg::place_io());
                io.v[0] = cpp(ctx, EE_CP_CENTER_EFFECT_LEVEL, 0, e, &mut pg::place_io());
                io.v[1] = cpp(ctx, EE_CP_CENTER_EFFECT_MAX_LEVEL, 0, e, &mut pg::place_io());
                let mut q = pg::place_io();
                cpp(ctx, EE_CP_CENTER_EFFECT_RADIUS, 0, e, &mut q);
                io.f[0] = q.f[0];
                1
            }
            pg::EE_CP_CENTER_EFFECT_TEMPLATES => {
                let mut q = pg::place_io();
                q.v[0] = v0;
                let upgrade = cpp(ctx, EE_CP_CENTER_UPGRADE, 0, 0, &mut q) as u64;
                let t = WorldRead::begin();
                let templates: [c_long; 4] = crate::world::StatsView::of(&t, upgrade)
                    .map_or([0; 4], |s| world_field!(s.fields(), mAreaEffectTemplates));
                for (k, &id) in templates.iter().enumerate() {
                    io.v[k] = i64::from(id as u32);
                }
                0
            }
            _ => return None,
        })
    }

    /// `EE_CP_PLACE_UNITS_IN_RECT` (the centres' house filters: `v4` 1 any living house of the player, else an unbuilt
    /// one): `EEGetUnitsInRectangle(rect, units, filter, true)` on the input units.
    fn units_in_rect(player: u64, io: &mut EeCpPlaceIo) -> i64 {
        let (left, top, right, bottom) = (io.v[0] as i32, io.v[1] as i32, io.v[2] as i32, io.v[3] as i32);
        let live = io.v[4] == 1;
        let mut units: Vec<u64> = input_pairs(io).into_iter().map(|p| p.0 as u64).collect();
        let keep = |u: u64| {
            let t = unsafe { WorldRead::begin() };
            // SAFETY: a region's live unit
            unsafe { UnitView::of(&t, u) }.is_some_and(|v| {
                let (x, y) = v.tile_xy();
                v.player().map_or(0, |p| p.handle()) == player
                    && v.class().is_some_and(|c| c.unit_type() == 0x25)
                    && (live || !v.is_built())
                    && v.hit_points() > 0
                    && x >= i64::from(left)
                    && x <= i64::from(right)
                    && y >= i64::from(top)
                    && y <= i64::from(bottom)
            })
        };
        crate::units::uu_ffi::collect_units((left, top, right, bottom), keep, &mut units);
        for u in units {
            push(io, u as i64, 0);
        }
        0
    }

    /// `EEComplexUnit::DistanceSquaredToUnit(other)` of the complex unit `unit`.
    pub(crate) fn distance_squared_to_unit(unit: u64, other: u64) -> f32 {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: the callers hand live units
        let from = unsafe { UnitView::of(&t, unit) }.map_or([0.0; 3], |u| u.location());
        // SAFETY: as above
        let to = unsafe { UnitView::of(&t, other) }.map_or(Err((0, 0)), |o| match o.complex_unit() {
            Some(c) => Ok(c.location()),
            None => {
                let (x, y) = o.tile_xy();
                Err((x as c_long, y as c_long))
            }
        });
        distance_squared_to(from, to)
    }

    /// `EEComplexUnit::DistanceSquaredToUnit`.
    ///
    /// # Safety
    /// `unit` a live complex unit, `other` a live unit.
    #[no_mangle]
    pub unsafe extern "C" fn ee_cp_distance_squared_to_unit(unit: u64, other: u64) -> f32 {
        distance_squared_to_unit(unit, other)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_distance_is_squared_in_the_ground_plane() {
        assert_eq!(distance_squared_to([1.0, 2.0, 9.0], Ok([4.0, 6.0, 0.0])), 25.0);
        assert_eq!(distance_squared_to([1.5, 2.0, 0.0], Err((1, 3))), 1.25);
    }

    #[test]
    fn the_sort_keeps_msvc_ties_and_orders_by_key() {
        assert_eq!(sort_order(&[3, 1, 2]), vec![1, 2, 0]);
        assert_eq!(sort_order(&[]), Vec::<u32>::new());
        let order = sort_order(&[5, 5, 1]);
        assert_eq!(order[0], 2);
    }
}
