//! The continent state's host questions Rust answers itself (were EEAIContinentSave.cpp's host and
//! EEAIContinentManager.cpp's tile kinds): an area's `Contains` and `RemoveTile` (a forest's with its wood count), the
//! area kinds' `Create`, the map's size, a tile's presence, terrain, AI value, AI kind (`EEAIGetTileKind`), wall
//! (`EEAIGetTileWallID`) and wood weight (`GetResourceWeight`), over the world's views and the objects' records.
//! The rest (a town's or herd's owner, a herd's own `Create`, the region size global, the AI value's write, the
//! random stream) stays the C++'s.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::ffi::c_long;

use crate::astar::cm_io::{self, EeAStarCmIo};
use crate::astar::continent_grid::EePoint;
use crate::astar::hull::EeHullPoint;
use crate::world::{MapView, TileView, WorldRead, WorldView};

const AREA_FOREST: c_long = 3;
const AREA_HERD: c_long = 9;
const RESOURCE_WOOD: i64 = 3;

fn token() -> WorldRead {
    // SAFETY: the simulation thread's reads of the live world
    unsafe { WorldRead::begin() }
}

fn map(t: &WorldRead) -> Option<MapView<'_>> {
    WorldView::global(t).and_then(|w| w.map())
}

fn tile<'t>(t: &'t WorldRead, x: i64, y: i64) -> Option<TileView<'t>> {
    map(t).and_then(|m| m.tile(x, y))
}

/// `EEAIArea::Contains(tile)`: the hull (its HULL record), then the tile set.
///
/// # Safety
/// `h` a live area.
pub unsafe fn contains(h: u64, x: c_long, y: c_long) -> bool {
    crate::astar::area_ops::ee_astar_area_hull_contains(cm_io::area(h), EeHullPoint::new(x, y))
        && ee_base::sparse::ee_sparse_contains(cm_io::area_tiles(h), x as i32, y as i32)
}

/// The influence table's region size (`gWorld.mInfluenceTable->mRegionSize`, 0 without one).
fn region_size() -> c_long {
    crate::astar::modifier_obj::influence_region_size() as c_long
}

/// `EEAIArea::RemoveTile(tile)` of the kind (a forest's count of the tile's wood kind drops).
///
/// # Safety
/// `h` a live area.
pub unsafe fn remove_tile(h: u64, x: c_long, y: c_long) -> bool {
    let removed = crate::astar::area_ops::ee_astar_area_remove_tile_captured(
        cm_io::area(h),
        cm_io::area_tiles(h),
        EeHullPoint::new(x, y),
    );
    if removed && cm_io::area(h).area_type == AREA_FOREST {
        let t = token();
        let kind = crate::astar::world_view::ee_astar_tile_resource_kind(
            tile(&t, x as i64, y as i64).map_or(0, |tl| tl.handle()),
        );
        crate::astar::area_kinds::ee_astar_forest_remove_resource(cm_io::forest(h), kind as c_long);
    }
    removed
}

/// `EEAIGetTileKind(tile)`.
pub fn tile_kind(x: c_long, y: c_long) -> i32 {
    let t = token();
    let Some(tl) = tile(&t, x as i64, y as i64) else { return 0 };
    let r = crate::astar::world_view::cm().expect("the continent manager");
    let c = crate::astar::cm_query::continent_at(r, x, y);
    // SAFETY: the tile's continent (the C++ read it without a test)
    let category = unsafe { cm_io::continent(c).category };
    let flag = tl.ai_value() as i32;
    let units = crate::astar::cm_passes::tile_units(&tl);
    crate::astar::tile_kind::tile_kind(category, tl.terrain_type() as i32, flag, &units)
}

/// `EEAIGetTileWallID(tile)`: -1 off the map.
pub fn wall_id(x: c_long, y: c_long) -> i32 {
    let t = token();
    match tile(&t, x as i64, y as i64) {
        Some(tl) => crate::astar::tile_kind::tile_wall_id(&crate::astar::cm_passes::tile_units(&tl)),
        None => -1,
    }
}

/// `GetResourceWeight(x, y)`: 10 per wood unit on the tile.
pub fn resource_weight(x: c_long, y: c_long) -> i64 {
    let t = token();
    tile(&t, x as i64, y as i64).map_or(0, |tl| {
        10 * tl
            .units()
            .flatten()
            .filter(|u| {
                u.class().is_some_and(|c| {
                    let v: c_long = crate::world_field!(c.fields(), mResourceType);
                    v as i64 == RESOURCE_WOOD
                })
            })
            .count() as i64
    })
}

/// A host question Rust answers (None: the C++'s).
pub(crate) fn answer(op: i32, io: &mut EeAStarCmIo) -> Option<i64> {
    let (x, y) = (io.i[0] as c_long, io.i[1] as c_long);
    // SAFETY: the ops' handles are live areas of the manager or the players
    unsafe {
        Some(match op {
            cm_io::EE_ASTAR_CM_CONTAINS => contains(io.h[0], x, y) as i64,
            cm_io::EE_ASTAR_CM_MAP_SIZE => {
                let t = token();
                map(&t).map_or(-1, |m| m.size())
            }
            cm_io::EE_ASTAR_CM_AI_VALUE => {
                let t = token();
                tile(&t, x as i64, y as i64).map_or(0, |tl| tl.ai_value() as i64)
            }
            crate::astar::cm_query::EE_ASTAR_CM_HAS_TILE => {
                let t = token();
                tile(&t, x as i64, y as i64).is_some() as i64
            }
            crate::astar::cm_query::EE_ASTAR_CM_FOREST_REMOVE_TILE
            | crate::astar::cm_repair::EE_ASTAR_CM_AREA_REMOVE_TILE => remove_tile(io.h[0], x, y) as i64,
            crate::astar::cm_repair::EE_ASTAR_CM_TILE_KIND => tile_kind(x, y) as i64,
            crate::astar::cm_repair::EE_ASTAR_CM_WALL_ID => wall_id(x, y) as i64,
            crate::astar::cm_repair::EE_ASTAR_CM_TERRAIN => {
                let t = token();
                tile(&t, x as i64, y as i64).map_or(-1, |tl| tl.terrain_type())
            }
            crate::astar::cm_repair::EE_ASTAR_CM_AREA_CREATE => {
                if cm_io::area(io.h[0]).area_type == AREA_HERD {
                    return None;
                }
                let p = io.h[1] as usize as *const EeHullPoint;
                let n = io.i[0] as usize;
                let points: Vec<EePoint> = if n == 0 || p.is_null() {
                    Vec::new()
                } else {
                    std::slice::from_raw_parts(p, n).iter().map(|q| EePoint { x: q.x as i32, y: q.y as i32 }).collect()
                };
                let set = cm_io::area_tiles(io.h[0]) as *mut _;
                ee_base::sparse::ee_sparse_insert_list(set, points.as_ptr(), points.len(), true);
                crate::astar::area_ops::ee_astar_area_update_hull_captured(cm_io::area(io.h[0]), set, region_size());
                0
            }
            crate::astar::cm_build::EE_ASTAR_CM_RESOURCE_WEIGHT => resource_weight(x, y),
            _ => return None,
        })
    }
}

/// `EEAIArea::RemoveTile(tile)` / `EEAIForest::RemoveTile(tile)` for the C++ (the kind's slot).
///
/// # Safety
/// `h` a live area.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_area_remove_tile_of(h: u64, x: c_long, y: c_long) -> bool {
    remove_tile(h, x, y)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_host_questions_rust_answers_are_the_tile_and_area_ones() {
        // a town's or herd's owner, the region size global, the AI value's write and the random stream stay C++
        let mut io = EeAStarCmIo::default();
        for op in [
            cm_io::EE_ASTAR_CM_OWNER,
            cm_io::EE_ASTAR_CM_REGION_SIZE,
            cm_io::EE_ASTAR_CM_SET_REGION_SIZE,
            cm_io::EE_ASTAR_CM_SET_AI_VALUE,
        ] {
            assert_eq!(answer(op, &mut io), None);
        }
    }
}
