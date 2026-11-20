//! The gather goal's spot search and accessibility test (the units' lane, round 5), moved whole from EEUGRust.cpp
//! (`EEGatherFindSpot` with its sort order `EEGatherTileFarther`, `EEGatherAccessible`; the goal host's
//! `EE_UNITS_GOAL_HOST_GATHER_SPOT` and `EE_UNITS_GOAL_HOST_ACCESSIBLE` cases), and the class's `GetTerrainFlags`
//! over the unit utilities' host. The continent manager is Rust (ee_sim::astar), the unit utilities are (uu.rs), the
//! citizen's `HasGatherPoint` is (citizen.rs): the bodies read the world through them and the views.

use crate::units::msvc_sort::sort_by;

/// `kAITile*` (ee/EEAIKindIDs.h)
const TILE_LAND: usize = 1;
const TILE_WATER: usize = 2;
const TILE_CLIFF: usize = 3;
const TILE_FOREST: usize = 4;
const TILE_SHALLOWS: usize = 7;
const TILE_SHALLOWS_SHORE: usize = 8;
const TILE_SHALLOWS_NEAR_SHORE: usize = 9;
const TILE_SHORE: usize = 10;
const TILE_NEAR_SHORE: usize = 11;
const TILE_GATE: usize = 0xe;
const TILE_SPACE: usize = 0xf;
/// `kMovementAir`
const MOVEMENT_AIR: u8 = 1;

/// The class's numbers `GetTerrainFlags` reads.
#[derive(Clone, Copy, Debug, Default)]
pub struct TerrainClass {
    pub domains: u8,
    pub water_kind: i64,
    pub can_cross_forest: bool,
    pub unit_type: u32,
}

/// `EEUnitClass::GetTerrainFlags(unit, flags)`'s 17 flags (`unit` 0: the class's; `terrain_mask` the unit's stats'
/// `mTerrainMask`). `can` is the class's virtual `CanMoveOnTerrain(unit, terrain)`, asked in the original's order;
/// `gates` whether the gates' unit set holds the class's type.
pub fn terrain_flags(
    c: TerrainClass,
    unit: u64,
    terrain_mask: u64,
    can: &mut dyn FnMut(i32) -> bool,
    gates: &mut dyn FnMut(u32) -> bool,
) -> [bool; 17] {
    let mut f = [false; 17];
    if c.domains & MOVEMENT_AIR == 0 {
        let water = can(0xa);
        f[TILE_LAND] = can(0);
        f[TILE_SHALLOWS_SHORE] = water;
        f[TILE_SHALLOWS_NEAR_SHORE] = water;
        f[TILE_SHALLOWS] = water;
        f[TILE_CLIFF] = can(9);
        f[TILE_WATER] = can(1);
        if can(1) {
            f[TILE_SHORE] = c.water_kind != 1 && c.water_kind != 2;
            f[TILE_SHALLOWS_SHORE] = water && c.water_kind != 1 && c.water_kind != 2;
            f[TILE_NEAR_SHORE] = c.water_kind != 2;
            f[TILE_SHALLOWS_NEAR_SHORE] = water && c.water_kind != 2;
        }
        if unit != 0 {
            f[TILE_FOREST] = (terrain_mask >> 14) & 1 != 0;
        } else if c.can_cross_forest {
            f[TILE_FOREST] = true;
        }
        if gates(c.unit_type) {
            f[TILE_GATE] = true;
        }
    } else {
        f = [true; 17];
    }
    f[TILE_SPACE] = can(0xc);
    f
}

/// What `EEGatherFindSpot` reads of the world.
pub trait SpotWorld {
    /// `EEGetResourceSpots(resource)`: the tiles around the resource
    fn resource_tiles(&mut self, resource: u64) -> Vec<[i32; 2]>;
    /// the gatherer's location (x, y) and tile
    fn unit_place(&mut self, unit: u64) -> ([f32; 2], [i32; 2]);
    /// `GetPathSubSectorAt(resource)` (a sub sector's handle, 0 none)
    fn resource_sub_sector(&mut self, resource: u64) -> u64;
    /// `GetPathSubSectorAt(x, y)`
    fn sub_sector_at(&mut self, x: i32, y: i32) -> u64;
    /// `start->IsNeighbor(other)` (false without a start)
    fn is_neighbor(&mut self, start: u64, other: u64) -> bool;
    /// the sub sector's `GetKind()`
    fn kind(&mut self, sub: u64) -> i64;
    /// the gatherer's class's `GetTerrainFlags(unit)`
    fn terrain_flags(&mut self, unit: u64) -> [bool; 17];
    /// the map tile's height (None: no tile)
    fn tile_height(&mut self, x: i32, y: i32) -> Option<f32>;
    /// `EEFindPlaceOnTile(unit, its class, x, y, 1, x, y, 1, io)`: `io`'s x and y the spot's
    fn place_on_tile(&mut self, unit: u64, x: i32, y: i32, io: &mut [f32; 3]) -> bool;
}

/// `EEGatherFindSpot(unit, resource, out)`: the resource's nearest spot in its path subsector, under the gatherer or
/// in a neighbouring subsector the gatherer can enter, else any spot with room (the waiting area, at the tile's
/// height).
pub fn find_spot(w: &mut dyn SpotWorld, unit: u64, resource: u64) -> Option<[f32; 3]> {
    let mut tiles = w.resource_tiles(resource);
    let (position, unit_tile) = w.unit_place(unit);
    // EEGatherTileFarther: the farther from the gatherer first (the nearest ends last)
    let distance = |t: &[i32; 2]| {
        let (x, y) = (t[0] as f32, t[1] as f32);
        (position[0] - x) * (position[0] - x) + (position[1] - y) * (position[1] - y)
    };
    sort_by(&mut tiles, |a, b| distance(a) > distance(b));
    let start = w.resource_sub_sector(resource);
    let mut flags: Option<[bool; 17]> = None;
    let mut others = Vec::new();
    let mut location = [0.0f32; 3];
    while let Some(&back) = tiles.last() {
        let sub = w.sub_sector_at(back[0], back[1]);
        let mut found = false;
        if sub == start || unit_tile == back {
            found = true;
        } else if w.is_neighbor(start, sub) {
            let f = *flags.get_or_insert_with(|| w.terrain_flags(unit));
            let kind = w.kind(sub);
            if usize::try_from(kind).ok().and_then(|k| f.get(k)).copied().unwrap_or(false) {
                found = true;
            }
        }
        if found {
            if let Some(height) = w.tile_height(back[0], back[1]) {
                if w.place_on_tile(unit, back[0], back[1], &mut location) {
                    return Some([location[0], location[1], height]);
                }
            }
        } else {
            others.push(back);
        }
        tiles.pop();
    }
    for t in others {
        let Some(height) = w.tile_height(t[0], t[1]) else { continue };
        if w.place_on_tile(unit, t[0], t[1], &mut location) {
            return Some([location[0], location[1], height]);
        }
    }
    None
}

/// `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`; `kTerrainDeepWater`
const CATEGORY_LAND_CITIZEN: u64 = 6;
const CATEGORY_NAVAL_CITIZEN: u64 = 0x16;
const TERRAIN_DEEP_WATER: i64 = 1;

/// `EEGatherAccessible(unit, resource)`: a land gatherer's resource on land, a naval one's on deep water, in its
/// continent with a gather point (`same_continent` and `gather_point` asked in that order, only when needed).
pub fn accessible(
    category: u64,
    resource_terrain: Option<i64>,
    same_continent: &mut dyn FnMut() -> bool,
    gather_point: &mut dyn FnMut() -> bool,
) -> bool {
    let Some(terrain) = resource_terrain else { return false };
    let wanted = match category {
        CATEGORY_LAND_CITIZEN => terrain != TERRAIN_DEEP_WATER,
        CATEGORY_NAVAL_CITIZEN => terrain == TERRAIN_DEEP_WATER,
        _ => return false,
    };
    wanted && same_continent() && gather_point()
}

// ------------------------------------------------------------------------------------------------ the game's world

#[cfg(not(test))]
pub use game::{game_accessible, game_find_spot};

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::astar::cm::EeAStarCmTable;
    use crate::astar::cm_io;
    use crate::astar::sector_rec::EeAStarSubSectorRecord;
    use crate::units::uu::UuHost;
    use crate::units::uu_host::Game as UuGame;
    use crate::world::{UnitView, WorldRead, WorldView};

    struct World<'t> {
        t: &'t WorldRead,
    }

    fn cm() -> &'static crate::astar::cm::EeAStarCmRecord {
        crate::astar::world_view::cm().expect("the continent manager's record")
    }

    impl SpotWorld for World<'_> {
        fn resource_tiles(&mut self, resource: u64) -> Vec<[i32; 2]> {
            let mut tiles = Vec::new();
            crate::units::uu_ffi::unit_tiles(1, resource, &mut tiles);
            tiles
        }
        fn unit_place(&mut self, unit: u64) -> ([f32; 2], [i32; 2]) {
            // SAFETY: the goal's live unit
            let u = unsafe { UnitView::of(self.t, unit) }.expect("a live gatherer");
            let l = u.location();
            let (x, y) = u.tile_xy();
            ([l[0], l[1]], [x as i32, y as i32])
        }
        fn resource_sub_sector(&mut self, resource: u64) -> u64 {
            // SAFETY: the registered record, a live resource
            unsafe { crate::astar::cm_queries::ee_astar_cmq_sub_sector_at(cm(), resource) }
        }
        fn sub_sector_at(&mut self, x: i32, y: i32) -> u64 {
            let r = cm();
            #[allow(clippy::unnecessary_cast)] // c_long is 64 bits off Windows
            let (x, y) = (x as std::os::raw::c_long, y as std::os::raw::c_long);
            match cm_io::sub_sector_id_at(r, x, y) {
                0 => 0,
                id => r.tables().tables[EeAStarCmTable::PathSubSectors as usize].get(id as usize).copied().unwrap_or(0),
            }
        }
        fn is_neighbor(&mut self, start: u64, other: u64) -> bool {
            if start == 0 {
                return false;
            }
            let mut n = 0usize;
            // SAFETY: a live sub sector's record (its handle)
            let p = unsafe {
                crate::astar::sector_rec::ee_astar_sub_sector_neighbors(
                    start as usize as *const EeAStarSubSectorRecord,
                    &mut n,
                )
            };
            // SAFETY: the record's list of `n` handles
            n != 0 && unsafe { std::slice::from_raw_parts(p, n) }.contains(&other)
        }
        fn kind(&mut self, sub: u64) -> i64 {
            // SAFETY: a live sub sector
            unsafe { cm_io::sub(sub).kind as i64 }
        }
        fn terrain_flags(&mut self, unit: u64) -> [bool; 17] {
            unit_terrain_flags(self.t, unit)
        }
        fn tile_height(&mut self, x: i32, y: i32) -> Option<f32> {
            WorldView::global(self.t)?.map()?.tile(x.into(), y.into()).map(|t| t.height())
        }
        fn place_on_tile(&mut self, unit: u64, x: i32, y: i32, io: &mut [f32; 3]) -> bool {
            // SAFETY: the goal's live unit; its class
            let class = unsafe { UnitView::of(self.t, unit) }.and_then(|u| u.class()).map_or(0, |c| c.handle());
            // SAFETY: a live point
            unsafe { crate::units::uu_ffi::ee_units_uu_find_place_on_tile(unit, class, x, y, 1, x, y, 1, io) != 0 }
        }
    }

    /// The unit's class's `GetTerrainFlags(unit)`.
    fn unit_terrain_flags(t: &WorldRead, unit: u64) -> [bool; 17] {
        // SAFETY: a live unit
        let u = unsafe { UnitView::of(t, unit) }.expect("a live unit");
        let class = u.class().expect("a unit's class");
        let f = class.fields();
        let c = TerrainClass {
            domains: crate::world_field!(f, mDomains),
            water_kind: crate::world_field!(f, mWaterKind) as i64,
            can_cross_forest: crate::world_field!(f, mCanCrossForest),
            unit_type: class.unit_type() as u32,
        };
        let mask = u.stats().map_or(0, |s| crate::world_field!(s.fields(), mTerrainMask) as u64);
        let h = class.handle();
        terrain_flags(c, unit, mask, &mut |terrain| UuGame.can_move_on_terrain(h, unit, terrain), &mut |ty| {
            // the gates' unit set (`gUnitSetManager.GetSet(0x62)`)
            UuGame.unit_set_contains(ty, 0x62)
        })
    }

    /// The goal host's `GATHER_SPOT`.
    pub fn game_find_spot(unit: u64, resource: u64) -> Option<[f32; 3]> {
        // SAFETY: inside the goal's call: the world is held
        let t = unsafe { WorldRead::begin() };
        find_spot(&mut World { t: &t }, unit, resource)
    }

    /// The goal host's `ACCESSIBLE`.
    pub fn game_accessible(unit: u64, resource: u64) -> bool {
        // SAFETY: as above
        let t = unsafe { WorldRead::begin() };
        // SAFETY: the goal's live unit and resource
        let category = unsafe { UnitView::of(&t, unit) }.and_then(|u| u.class()).map_or(0, |c| c.category());
        // SAFETY: as above
        let terrain = unsafe { UnitView::of(&t, resource) }.map(|r| r.tile().map_or(0, |tile| tile.terrain_type()));
        accessible(
            category,
            terrain,
            // SAFETY: the registered record, two live units
            &mut || unsafe { crate::astar::cm_queries::ee_astar_cmq_in_same_continent(cm(), unit, resource) },
            &mut || crate::units::uu_host::has_gather_point(unit, resource),
        )
    }
}

#[cfg(test)]
#[path = "gather_spot_tests.rs"]
mod tests;
