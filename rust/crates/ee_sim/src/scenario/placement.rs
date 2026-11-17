//! `EESMObject::FindPlacement`, `IsNearTile` and `IsNearUnit`: where a Create effect puts a new unit (a free spot
//! that meets the object's area and nearness filters) and whether an object's units are near a tile or a unit. The
//! tile lists come from the world (`EEGetTilesNearPoint`, the side and corner tiles, an area's tiles) through
//! [`Host::tiles`]; every other world value (footprints, line of sight, reach, the blocking test, the terrain) is one
//! read. The membership tests the original made with `std::find` over vectors are set lookups here (the same
//! answers; no set is ever iterated).

#![allow(clippy::unnecessary_cast)]

use std::collections::HashSet;

use crate::scenario::area::{unpack, EeScenarioAreaRecord};
use crate::scenario::condition::shuffle;
use crate::scenario::fields::EeScenarioObjectFields;
use crate::scenario::ftol;
use crate::scenario::host::*;
use crate::scenario::objeval::{EE_SCENARIO_READ_OBJ_EVALUATE, EE_SCENARIO_READ_UNIT_FLAGS};

/// `gWorld.mMap`'s `mL195610` (the height, low 32 bits) and `mL195614` (the width, high), as `unsigned long`s.
pub const EE_SCENARIO_READ_MAP_SIZE: u32 = 70;
/// `GetArea(a)->ContainsTile(U2DPoint<long>(b, c))` (1/0; the area's own capture record).
pub const EE_SCENARIO_READ_AREA_CONTAINS_TILE: u32 = 71;
/// `GetScenarioObject(a)->mUnits.size()`.
pub const EE_SCENARIO_READ_OBJECT_UNIT_COUNT: u32 = 72;
/// `GetScenarioObject(a)->mUnits[b]`.
pub const EE_SCENARIO_READ_OBJECT_UNIT_AT: u32 = 73;
/// `GetScenarioObject(a)->mSelectedByCheck`.
pub const EE_SCENARIO_READ_OBJECT_SELECTED_BY_CHECK: u32 = 74;
/// Unit `a`'s `mTileX`, `mTileY`, packed.
pub const EE_SCENARIO_READ_UNIT_TILE_OF: u32 = 75;
/// Unit `a`'s `mComplexUnit->GetLineOfSight()`'s bits.
pub const EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT: u32 = 76;
/// Unit `a`'s `mStats->mRange`'s bits.
pub const EE_SCENARIO_READ_UNIT_RANGE: u32 = 77;
/// Unit `a`'s `mUnitClass->mTileSize`.
pub const EE_SCENARIO_READ_UNIT_TILE_SIZE: u32 = 78;
/// `gUnitClassTable[a]->mTileSize`.
pub const EE_SCENARIO_READ_CLASS_TILE_SIZE: u32 = 79;
/// `EEPlayerBlockTakes(NULL, a, b, c, d, d, 0)` (1/0).
pub const EE_SCENARIO_READ_BLOCK_TAKES: u32 = 80;
/// `gWorld.mMap->GetTile(a, b)`: 1 for a tile that is neither deep nor shallow water, else 0 (no tile too).
pub const EE_SCENARIO_READ_MAP_TILE_DRY: u32 = 81;
/// `EEGetFootprintDistance(unit a, b, c, ...)`'s bits (the squared distance to the unit's nearest tile).
pub const EE_SCENARIO_READ_FOOTPRINT_DISTANCE: u32 = 82;
/// Unit `a`'s complex unit's `CanReachTile(b, c)`, 0 without one.
pub const EE_SCENARIO_READ_UNIT_CAN_REACH_TILE: u32 = 83;
/// Unit `a`'s `GetNearestFootprintTile(unit b, &x, &y)`: (x, y) packed.
pub const EE_SCENARIO_READ_UNIT_NEAREST_TILE: u32 = 84;
/// Unit `a`'s complex unit's `IsInReach(unit b)`, 0 without one.
pub const EE_SCENARIO_READ_UNIT_IN_REACH: u32 = 85;
/// `gWorld.mMap->GetTile(a, b) != NULL` (1/0).
pub const EE_SCENARIO_READ_MAP_TILE_EXISTS: u32 = 86;

/// `EEGetTilesNearPoint(GE3DPoint((float)a, (float)b, 0), radius c's bits, list)`.
pub const EE_SCENARIO_TILES_NEAR_POINT: u32 = 0;
/// `EEGetSideTiles(a, b, size c, list)`.
pub const EE_SCENARIO_TILES_SIDE: u32 = 1;
/// `EEGetCornerTiles(0, a, b, size c, list)`.
pub const EE_SCENARIO_TILES_CORNER: u32 = 2;
/// `GetArea(a)->GetTiles(list)` (the area's cache and capture record).
pub const EE_SCENARIO_TILES_AREA: u32 = 3;
/// `gContinentManager.GetContinentTiles(a, list)`.
pub const EE_SCENARIO_TILES_CONTINENT: u32 = 4;
/// `gWorld.GetPlayer(a)->mTowns[b]->GetTiles(list)`.
pub const EE_SCENARIO_TILES_TOWN: u32 = 5;

/// What [`find_placement`] answers: not found, found (the location written), or not found and to be tried again
/// through the C++ entry without the random tries (`FindPlacement(type, location, false)`, its own record).
pub const EE_SCENARIO_PLACEMENT_NONE: u32 = 0;
pub const EE_SCENARIO_PLACEMENT_FOUND: u32 = 1;
pub const EE_SCENARIO_PLACEMENT_RETRY: u32 = 2;

/// The unset tile coordinate (`kNoArea`, a unit off the map).
const EE_SCENARIO_NO_TILE: i32 = -10000;
/// The unit types FindPlacement keeps off open water (0x24 only once its far corner is a candidate).
const EE_SCENARIO_TYPE_COAST_24: u32 = 0x24;
const EE_SCENARIO_TYPE_COAST_41: u32 = 0x41;

fn bits(value: i64) -> f32 {
    f32::from_bits(value as u32)
}

fn unit_exists<H: Host>(h: &mut H, id: i32) -> bool {
    h.read(EE_SCENARIO_READ_UNIT_FLAGS, id, 0) & 1 != 0
}

/// `EESMObject::IsNearUnit(unit, mode, from_unit)` of object `object` (not NULL): whether one of its units, after an
/// `Evaluate(0)`, is within line of sight (mode 0; the unit's own sight when `from_unit`, else the object unit's),
/// within reach (1) or adjacent (2) of unit `unit` (not NULL); false for a unit off the map.
pub fn near_unit<H: Host>(h: &mut H, object: i32, unit: i32, mode: i32, from_unit: bool) -> bool {
    let mut result = false;
    if unpack(h.read(EE_SCENARIO_READ_UNIT_TILE_OF, unit, 0)).0 == EE_SCENARIO_NO_TILE {
        return result;
    }
    h.read(EE_SCENARIO_READ_OBJ_EVALUATE, object, 0);
    let count = h.read(EE_SCENARIO_READ_OBJECT_UNIT_COUNT, object, 0) as i32;
    let mut i = 0;
    while i < count && !result {
        let other = h.read(EE_SCENARIO_READ_OBJECT_UNIT_AT, object, i) as i32;
        i += 1;
        if !unit_exists(h, other)
            || other == unit
            || unpack(h.read(EE_SCENARIO_READ_UNIT_TILE_OF, other, 0)).0 == EE_SCENARIO_NO_TILE
        {
            continue;
        }
        let (other_x, other_y) = unpack(h.read(EE_SCENARIO_READ_UNIT_NEAREST_TILE, unit, other));
        let (x, y) = unpack(h.read(EE_SCENARIO_READ_UNIT_NEAREST_TILE, other, unit));
        let (dx, dy) = (other_x.wrapping_sub(x), other_y.wrapping_sub(y));
        let distance = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as f32;
        match mode {
            0 => {
                let range = bits(h.read(EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT, if from_unit { unit } else { other }, 0));
                result = distance <= range * range;
            }
            1 => {
                let (a, b) = if from_unit { (unit, other) } else { (other, unit) };
                result = h.read(EE_SCENARIO_READ_UNIT_IN_REACH, a, b) != 0;
            }
            2 => result = distance <= 2.0,
            _ => {}
        }
    }
    result
}

/// `EESMObject::IsNearTile(x, y, mode, false)` of object `object` (not NULL): whether one of its units (as listed;
/// no `Evaluate`) has the tile within line of sight (mode 0), can reach it (1) or is adjacent to it (2).
pub fn near_tile<H: Host>(h: &mut H, object: i32, x: i32, y: i32, mode: i32) -> bool {
    let count = h.read(EE_SCENARIO_READ_OBJECT_UNIT_COUNT, object, 0) as i32;
    let mut result = false;
    let mut i = 0;
    while i < count && !result {
        let unit = h.read(EE_SCENARIO_READ_OBJECT_UNIT_AT, object, i) as i32;
        i += 1;
        if !unit_exists(h, unit) {
            continue;
        }
        let distance = bits(h.read4(EE_SCENARIO_READ_FOOTPRINT_DISTANCE, unit, x, y, 0));
        match mode {
            0 => {
                let range = bits(h.read(EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT, unit, 0));
                result = distance <= range * range;
            }
            1 => result = h.read4(EE_SCENARIO_READ_UNIT_CAN_REACH_TILE, unit, x, y, 0) != 0,
            2 => result = distance <= 2.0,
            _ => {}
        }
    }
    result
}

/// The map's tiles as the original walks them: `width` diagonal rows of `height` tiles from (row, row + height),
/// x up and y down; with `pairs`, each tile followed by its neighbour (x + 1, y).
fn map_walk(height: u32, width: u32, pairs: bool, mut visit: impl FnMut((i32, i32))) {
    for row in 0..width {
        let (mut x, mut y) = (row as i32, row.wrapping_add(height) as i32);
        for _ in 0..height {
            visit((x, y));
            if pairs {
                visit((x.wrapping_add(1), y));
            }
            x = x.wrapping_add(1);
            y = y.wrapping_sub(1);
        }
    }
}

fn set_of(tiles: &[(i32, i32)]) -> HashSet<(i32, i32)> {
    tiles.iter().copied().collect()
}

/// `EESMObject::FindPlacement(unit_type, location, try_random)`'s body for object `f` with its area (`area`: the
/// record of `GetArea(f.mArea)`, `None` for no area): a free spot for a unit of the type, written into `location` as
/// the original wrote it (also where it then answers false). Random tries (25 tiles) for a 'not in the area'
/// (with `try_random`) or 'not near' filter, else every tile of the area and near the object's units; the
/// candidates shuffled; the first the type's footprint fits on (a larger unit also needs its far corner among the
/// candidates; types 0x24 and 0x41 a dry tile around them). [`EE_SCENARIO_PLACEMENT_RETRY`] asks the caller for the
/// retry without random tries.
pub fn find_placement<H: Host>(
    h: &mut H,
    f: &EeScenarioObjectFields,
    area: Option<&EeScenarioAreaRecord>,
    unit_type: u32,
    try_random: bool,
    location: &mut [f32; 2],
) -> u32 {
    let object = f.mNearObject as i32;
    let has_object = h.read(EE_SCENARIO_READ_OBJECT_EXISTS, object, 0) != 0;
    let area_id = f.mArea as i32;
    let (height, width) = unpack(h.read(EE_SCENARIO_READ_MAP_SIZE, 0, 0));
    let (height, width) = (height as u32, width as u32);
    let (in_area, near) = (f.mInAreaCheck, f.mNearCheck);
    let mut candidates: Vec<(i32, i32)> = Vec::new();
    let mut found = false;
    let mut tried_random = false;
    if (try_random && in_area == 2 && area.is_some()) || (near == 2 && has_object) {
        tried_random = true;
        if near == 2 && has_object {
            h.read(EE_SCENARIO_READ_OBJ_EVALUATE, object, 0);
        }
        for _ in 0..25 {
            let a = h.read(EE_SCENARIO_READ_RANDOM, 0, width.wrapping_sub(1) as i32) as i32;
            let b = h.read(EE_SCENARIO_READ_RANDOM, 0, height.wrapping_sub(1) as i32) as i32;
            let x = b.wrapping_add(a);
            let y = (height as i32).wrapping_add(b).wrapping_sub(a);
            let mut allowed = true;
            if in_area == 2
                && area.is_some()
                && h.read4(EE_SCENARIO_READ_AREA_CONTAINS_TILE, area_id, ftol(x as f32), ftol(y as f32), 0) != 0
            {
                allowed = false;
            }
            if near == 2 && has_object && near_tile(h, object, x, y, f.mNearMode as i32) {
                allowed = false;
            }
            if allowed {
                candidates.push((x, y));
            }
        }
    } else {
        let mut no_area = false;
        let mut area_tiles: Vec<(i32, i32)> = Vec::new();
        let mut object_tiles: Vec<(i32, i32)> = Vec::new();
        // `theTiles` and `theOtherTiles`: a unit's lists, which a missing or dead unit leaves as they were (the area's
        // own tiles at first), and the loop appends them all the same
        let mut tiles: Vec<(i32, i32)> = Vec::new();
        let mut other: Vec<(i32, i32)> = Vec::new();
        match area {
            Some(a) if in_area == 1 => {
                if a.is_unset() {
                    no_area = true;
                    *location = [EE_SCENARIO_NO_TILE as f32; 2];
                    found = true;
                } else if !a.is_off_map() {
                    area_tiles = h.tiles(EE_SCENARIO_TILES_AREA, area_id, 0, 0);
                }
            }
            Some(a) if in_area == 2 => {
                // every map tile outside the area (each with its neighbour)
                if !a.is_unset() && a.select_mode != 0 {
                    tiles = h.tiles(EE_SCENARIO_TILES_AREA, area_id, 0, 0);
                }
                let inside = set_of(&tiles);
                map_walk(height, width, true, |(x, y)| {
                    let keep = if a.is_unset() {
                        true
                    } else if a.select_mode == 0 {
                        h.read4(EE_SCENARIO_READ_AREA_CONTAINS_TILE, area_id, x, y, 0) == 0
                    } else {
                        !inside.contains(&(x, y))
                    };
                    if keep {
                        area_tiles.push((x, y));
                    }
                });
            }
            _ => {}
        }
        if near != 0
            && has_object
            && ((h.read(EE_SCENARIO_READ_OBJECT_SELECTED_BY_CHECK, object, 0) != 0
                && h.read(EE_SCENARIO_READ_OBJECT_UNIT_COUNT, object, 0) > 0)
                || h.read(EE_SCENARIO_READ_OBJ_EVALUATE, object, 0) != 0)
        {
            let mut i: i64 = 0;
            while i < h.read(EE_SCENARIO_READ_OBJECT_UNIT_COUNT, object, 0) {
                let unit = h.read(EE_SCENARIO_READ_OBJECT_UNIT_AT, object, i as i32) as i32;
                let flags = h.read(EE_SCENARIO_READ_UNIT_FLAGS, unit, 0);
                if flags & 1 != 0 && flags & 2 != 0 {
                    tiles.clear();
                    other.clear();
                    match f.mNearMode {
                        0 | 1 => {
                            let op = if f.mNearMode == 0 {
                                EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT
                            } else {
                                EE_SCENARIO_READ_UNIT_RANGE
                            };
                            let radius = h.read(op, unit, 0);
                            if bits(radius) > 0.0 {
                                let (x, y) = unpack(h.read(EE_SCENARIO_READ_UNIT_TILE_OF, unit, 0));
                                tiles = h.tiles(EE_SCENARIO_TILES_NEAR_POINT, x, y, radius as u32 as i32);
                            }
                        }
                        2 => {
                            let (x, y) = unpack(h.read(EE_SCENARIO_READ_UNIT_TILE_OF, unit, 0));
                            let size = h.read(EE_SCENARIO_READ_UNIT_TILE_SIZE, unit, 0) as i32;
                            tiles = h.tiles(EE_SCENARIO_TILES_SIDE, x, y, size);
                            other = h.tiles(EE_SCENARIO_TILES_CORNER, x, y, size);
                        }
                        _ => {}
                    }
                }
                object_tiles.extend_from_slice(&tiles);
                object_tiles.extend_from_slice(&other);
                i += 1;
            }
            if near == 2 {
                // every map tile not near the units
                let near_tiles = set_of(&object_tiles);
                object_tiles.clear();
                map_walk(height, width, false, |t| {
                    if !near_tiles.contains(&t) {
                        object_tiles.push(t);
                    }
                });
            }
        }
        if in_area == 0 {
            if near != 0 {
                candidates = object_tiles;
            }
        } else if near == 0 {
            candidates = area_tiles;
        } else {
            let near_tiles = set_of(&object_tiles);
            candidates = area_tiles.into_iter().filter(|t| near_tiles.contains(t)).collect();
        }
        if no_area {
            return EE_SCENARIO_PLACEMENT_FOUND;
        }
    }
    let size = h.read(EE_SCENARIO_READ_CLASS_TILE_SIZE, unit_type as i32, 0) as i32;
    shuffle(h, &mut candidates);
    let mut lookup: Option<HashSet<(i32, i32)>> = None;
    for &(x, y) in &candidates {
        if found {
            break;
        }
        if h.read4(EE_SCENARIO_READ_BLOCK_TAKES, unit_type as i32, x, y, size) == 0 {
            continue;
        }
        if size == 1 {
            found = true;
            *location = [x as f32, y as f32];
            continue;
        }
        let corner = (size.wrapping_sub(1).wrapping_add(x), size.wrapping_sub(1).wrapping_add(y));
        if lookup.get_or_insert_with(|| set_of(&candidates)).contains(&corner) {
            found = true;
            *location = [x as f32, y as f32];
        }
        // the 0x41 test does not depend on the corner (nor writes the location)
        if (found && unit_type == EE_SCENARIO_TYPE_COAST_24) || unit_type == EE_SCENARIO_TYPE_COAST_41 {
            found = false;
            'dry: for tx in x.wrapping_sub(1)..=x.wrapping_add(size) {
                for ty in y.wrapping_sub(1)..=y.wrapping_add(size) {
                    if h.read(EE_SCENARIO_READ_MAP_TILE_DRY, tx, ty) != 0 {
                        found = true;
                        break 'dry;
                    }
                }
            }
        }
    }
    if found {
        EE_SCENARIO_PLACEMENT_FOUND
    } else if try_random && tried_random {
        EE_SCENARIO_PLACEMENT_RETRY
    } else {
        EE_SCENARIO_PLACEMENT_NONE
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// A 4 x 3 map (height 4, width 3: x + y from 4 to 10), units by id: (flags, tile, line of sight, range, size,
    /// in reach), object 1's units, the area's tiles, scripted random draws, blocked and wet tiles; a log of the
    /// world calls.
    type Unit = (i64, (i32, i32), f32, f32, i32, bool);

    struct World {
        units: BTreeMap<i32, Unit>,
        object: Vec<i32>,
        selected_by: i64,
        area: Vec<(i32, i32)>,
        draws: Vec<i64>,
        blocked: Vec<(i32, i32)>,
        dry: Vec<(i32, i32)>,
        log: Vec<String>,
    }

    fn world() -> World {
        let mut units = BTreeMap::new();
        units.insert(5, (3, (6, 1), 1.5, 2.0, 1, true));
        units.insert(6, (3, (2, 6), 0.0, 0.0, 2, false));
        units.insert(7, (1, (0, 0), 9.0, 9.0, 1, true)); // dead
        World {
            units,
            object: vec![5],
            selected_by: 0,
            area: vec![(5, 2), (6, 2), (5, 3)],
            draws: vec![],
            blocked: vec![],
            dry: vec![],
            log: vec![],
        }
    }

    fn pack(x: i32, y: i32) -> i64 {
        (x as u32 as i64) | ((y as u32 as i64) << 32)
    }

    impl Host for World {
        fn read4(&mut self, op: u32, a: i32, b: i32, c: i32, d: i32) -> i64 {
            let u = self.units.get(&a).copied().unwrap_or((0, (0, 0), 0.0, 0.0, 1, false));
            match op {
                EE_SCENARIO_READ_OBJECT_EXISTS => (a == 1) as i64,
                EE_SCENARIO_READ_MAP_SIZE => pack(4, 3),
                EE_SCENARIO_READ_OBJ_EVALUATE => {
                    self.log.push(format!("evaluate {a}"));
                    1
                }
                EE_SCENARIO_READ_OBJECT_UNIT_COUNT => self.object.len() as i64,
                EE_SCENARIO_READ_OBJECT_UNIT_AT => self.object[b as usize] as i64,
                EE_SCENARIO_READ_OBJECT_SELECTED_BY_CHECK => self.selected_by,
                EE_SCENARIO_READ_UNIT_FLAGS => u.0,
                EE_SCENARIO_READ_UNIT_TILE_OF => pack(u.1 .0, u.1 .1),
                EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT => u.2.to_bits() as i64,
                EE_SCENARIO_READ_UNIT_RANGE => u.3.to_bits() as i64,
                EE_SCENARIO_READ_UNIT_TILE_SIZE => u.4 as i64,
                EE_SCENARIO_READ_UNIT_IN_REACH => u.5 as i64,
                EE_SCENARIO_READ_UNIT_NEAREST_TILE => pack(u.1 .0, u.1 .1),
                EE_SCENARIO_READ_FOOTPRINT_DISTANCE => {
                    let (dx, dy) = ((u.1 .0 - b) as f32, (u.1 .1 - c) as f32);
                    (dx * dx + dy * dy).to_bits() as i64
                }
                EE_SCENARIO_READ_UNIT_CAN_REACH_TILE => (u.5 && b == u.1 .0) as i64,
                EE_SCENARIO_READ_AREA_CONTAINS_TILE => {
                    self.log.push(format!("contains {b} {c}"));
                    self.area.contains(&(b, c)) as i64
                }
                EE_SCENARIO_READ_RANDOM => {
                    self.log.push(format!("random {a} {b}"));
                    if self.draws.is_empty() {
                        0
                    } else {
                        self.draws.remove(0).min(b as i64)
                    }
                }
                EE_SCENARIO_READ_CLASS_TILE_SIZE => {
                    if a == 0x41 || a == 2 {
                        2
                    } else {
                        1
                    }
                }
                EE_SCENARIO_READ_BLOCK_TAKES => {
                    assert_eq!(d, if a == 0x41 || a == 2 { 2 } else { 1 });
                    !self.blocked.contains(&(b, c)) as i64
                }
                EE_SCENARIO_READ_MAP_TILE_DRY => self.dry.contains(&(a, b)) as i64,
                _ => panic!("read {op}"),
            }
        }
        fn tiles(&mut self, op: u32, a: i32, b: i32, c: i32) -> Vec<(i32, i32)> {
            self.log.push(format!("tiles {op} {a} {b} {c}"));
            match op {
                EE_SCENARIO_TILES_AREA => self.area.clone(),
                EE_SCENARIO_TILES_NEAR_POINT => vec![(a, b), (a + 1, b)],
                EE_SCENARIO_TILES_SIDE => vec![(a - 1, b)],
                EE_SCENARIO_TILES_CORNER => vec![(a - 1, b - 1)],
                _ => panic!("tiles {op}"),
            }
        }
    }

    fn object(in_area: u64, near: u64, mode: u64) -> EeScenarioObjectFields {
        EeScenarioObjectFields {
            mArea: 3,
            mNearObject: 1,
            mInAreaCheck: in_area as _,
            mNearCheck: near as _,
            mNearMode: mode as _,
            ..Default::default()
        }
    }

    fn area(select_mode: u32, anchor: i32) -> EeScenarioAreaRecord {
        EeScenarioAreaRecord { select_mode, anchor_x: anchor, anchor_y: anchor, ..Default::default() }
    }

    fn place(
        w: &mut World,
        f: &EeScenarioObjectFields,
        a: Option<&EeScenarioAreaRecord>,
        t: u32,
        r: bool,
    ) -> (u32, [f32; 2]) {
        let mut at = [-1.0, -1.0];
        let result = find_placement(w, f, a, t, r, &mut at);
        (result, at)
    }

    #[test]
    fn the_map_walk_is_the_originals() {
        let mut v = vec![];
        map_walk(2, 2, false, |t| v.push(t));
        assert_eq!(v, [(0, 2), (1, 1), (1, 3), (2, 2)]);
        let mut v = vec![];
        map_walk(1, 1, true, |t| v.push(t));
        assert_eq!(v, [(0, 1), (1, 1)]);
    }

    #[test]
    fn near_a_unit_by_sight_reach_and_contact() {
        let mut w = world();
        // unit 6 at (2, 6) and object unit 5 at (6, 1): 16 + 25 = 41 apart; sight 1.5 too short
        assert!(!near_unit(&mut w, 1, 6, 0, false));
        assert_eq!(w.log, ["evaluate 1"]);
        w.units.get_mut(&5).unwrap().2 = 6.5;
        assert!(near_unit(&mut w, 1, 6, 0, false));
        // the unit's own sight (0) when from the unit
        assert!(!near_unit(&mut w, 1, 6, 0, true));
        // reach: unit 5 reaches, unit 6 does not
        assert!(near_unit(&mut w, 1, 6, 1, false));
        assert!(!near_unit(&mut w, 1, 6, 1, true));
        assert!(!near_unit(&mut w, 1, 6, 2, false));
        // the unit itself is skipped, an off-map unit is never near
        assert!(!near_unit(&mut w, 1, 5, 2, false));
        w.units.get_mut(&6).unwrap().1 = (EE_SCENARIO_NO_TILE, 0);
        w.log.clear();
        assert!(!near_unit(&mut w, 1, 6, 0, false));
        assert!(w.log.is_empty());
    }

    #[test]
    fn near_a_tile_by_sight_reach_and_contact() {
        let mut w = world();
        w.object = vec![9, 5];
        assert!(near_tile(&mut w, 1, 6, 2, 0));
        assert!(!near_tile(&mut w, 1, 4, 1, 0));
        assert!(near_tile(&mut w, 1, 6, 9, 1));
        assert!(near_tile(&mut w, 1, 7, 2, 2));
        assert!(!near_tile(&mut w, 1, 8, 1, 2));
        assert!(!near_tile(&mut w, 1, 6, 1, 3));
        assert!(w.log.is_empty());
    }

    #[test]
    fn an_unset_area_places_at_no_tile_after_the_nearness_work() {
        let mut w = world();
        let (r, at) = place(&mut w, &object(1, 1, 2), Some(&area(0, EE_SCENARIO_NO_TILE)), 1, true);
        assert_eq!((r, at), (EE_SCENARIO_PLACEMENT_FOUND, [-10000.0, -10000.0]));
        // the object evaluated and its tiles made, no shuffle
        assert_eq!(w.log, ["evaluate 1", "tiles 1 6 1 1", "tiles 2 6 1 1"]);
    }

    #[test]
    fn in_the_area_a_shuffled_free_tile() {
        let mut w = world();
        w.draws = vec![1, 0];
        w.blocked = vec![(6, 2)];
        let (r, at) = place(&mut w, &object(1, 0, 0), Some(&area(1, 4)), 1, true);
        // (5, 2), (6, 2), (5, 3) -> swap 1 with 1, swap 2 with 0: (5, 3), (6, 2), (5, 2)
        assert_eq!((r, at), (EE_SCENARIO_PLACEMENT_FOUND, [5.0, 3.0]));
        assert_eq!(w.log, ["tiles 3 3 0 0", "random 0 1", "random 0 2"]);
        // off the map: no candidates, no retry without random tries
        let mut w = world();
        let (r, at) = place(&mut w, &object(1, 0, 0), Some(&area(1, -4)), 1, true);
        assert_eq!((r, at), (EE_SCENARIO_PLACEMENT_NONE, [-1.0, -1.0]));
    }

    #[test]
    fn outside_the_area_every_map_tile_and_its_neighbour() {
        // select mode 0 asks the area per tile; the map has 12 tiles, 24 visits
        let mut w = world();
        let (r, _) = place(&mut w, &object(2, 0, 0), Some(&area(0, 4)), 1, false);
        assert_eq!(r, EE_SCENARIO_PLACEMENT_FOUND);
        assert_eq!(w.log.iter().filter(|l| l.starts_with("contains")).count(), 24);
        // other modes leave out the area's own tiles: with every tile blocked, nothing (and no retry: not random)
        let mut w = world();
        let mut all = vec![];
        map_walk(4, 3, true, |t| all.push(t));
        w.blocked = all;
        let (r, _) = place(&mut w, &object(2, 0, 0), Some(&area(1, 4)), 1, false);
        assert_eq!(r, EE_SCENARIO_PLACEMENT_NONE);
        assert_eq!(w.log[0], "tiles 3 3 0 0");
    }

    #[test]
    fn near_the_objects_units_with_the_stale_lists_of_a_dead_unit() {
        // area check 2 (not random) leaves the area's tiles in the unit list; dead unit 7 appends them again
        let mut w = world();
        w.object = vec![7, 5];
        w.selected_by = 1;
        let mut all = vec![];
        map_walk(4, 3, true, |t| all.push(t));
        w.blocked = all.clone();
        let (r, _) = place(&mut w, &object(2, 1, 0), Some(&area(1, 4)), 1, false);
        assert_eq!(r, EE_SCENARIO_PLACEMENT_NONE);
        // selected-by objects with units are not evaluated; sight 1.5 > 0: tiles near (6, 1)
        assert!(!w.log.iter().any(|l| l.starts_with("evaluate")));
        assert!(w.log.contains(&"tiles 0 6 1 1069547520".to_string()));
        // range 2.0 for mode 1: the tiles near (6, 1), (7, 1), shuffled (the draw 1 keeps them)
        let mut w = world();
        w.draws = vec![1];
        let (r, at) = place(&mut w, &object(0, 1, 1), None, 1, true);
        assert_eq!((r, at), (EE_SCENARIO_PLACEMENT_FOUND, [6.0, 1.0]));
        assert!(w.log.contains(&"tiles 0 6 1 1073741824".to_string()));
    }

    #[test]
    fn not_near_tries_25_random_tiles_then_asks_for_the_retry() {
        let mut w = world();
        let mut all = vec![];
        map_walk(4, 3, true, |t| all.push(t));
        w.blocked = all;
        w.blocked.push((4, 4));
        let (r, _) = place(&mut w, &object(0, 2, 2), None, 1, true);
        assert_eq!(r, EE_SCENARIO_PLACEMENT_RETRY);
        // 25 tries of two draws (width - 1, height - 1); the draws 0 give (0, 4), not near (6, 1)
        assert_eq!(w.log[0], "evaluate 1");
        // (and the shuffle's draws for k = 2 and 3)
        assert_eq!(w.log.iter().filter(|l| *l == "random 0 2").count(), 26);
        assert_eq!(w.log.iter().filter(|l| *l == "random 0 3").count(), 26);
        // the 25 candidates shuffled: 24 draws (k = 1 .. 24)
        assert_eq!(w.log.iter().filter(|l| l.starts_with("random")).count(), 50 + 24);
        // without try_random the same work answers none
        let mut w2 = world();
        w2.blocked = w.blocked.clone();
        let (r, _) = place(&mut w2, &object(0, 2, 2), None, 1, false);
        assert_eq!(r, EE_SCENARIO_PLACEMENT_NONE);
    }

    #[test]
    fn larger_units_need_their_far_corner_and_the_coastal_types_dry_land() {
        let mut w = world();
        // area (5, 2), (6, 2), (5, 3); size 2: (5, 2)'s corner (6, 3) is no candidate, nothing else either
        let (r, _) = place(&mut w, &object(1, 0, 0), Some(&area(1, 4)), 2, false);
        assert_eq!(r, EE_SCENARIO_PLACEMENT_NONE);
        w.area.push((6, 3));
        let (r, at) = place(&mut w, &object(1, 0, 0), Some(&area(1, 4)), 2, false);
        assert_eq!((r, at), (EE_SCENARIO_PLACEMENT_FOUND, [5.0, 2.0]));
        // 0x41 without dry land: none; with it found, the location the earlier corner's write or none
        let mut w = world();
        let (r, at) = place(&mut w, &object(1, 0, 0), Some(&area(1, 4)), 0x41, false);
        assert_eq!((r, at), (EE_SCENARIO_PLACEMENT_NONE, [-1.0, -1.0]));
        w.dry = vec![(4, 1)];
        let (r, at) = place(&mut w, &object(1, 0, 0), Some(&area(1, 4)), 0x41, false);
        assert_eq!((r, at), (EE_SCENARIO_PLACEMENT_FOUND, [-1.0, -1.0]));
    }
}
