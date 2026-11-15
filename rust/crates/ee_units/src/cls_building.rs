//! The buildings' and the gate's helpers of `EEUnitClass` (R5b.4, the thirty-third round; EEUnitClassBuildings.cpp
//! until this round): the facings (`EEGetFacingQuarter`, a navy yard's and a dock's side), the gate's pieces' turn
//! and tiles, the level test of a gate line, and the gate's opening tests (`AreEnemiesNear`, `IsPasserNear`).
//!
//! The world is the [`BuildingHost`]: the map's tiles, terrains and mesh, the continent manager, the units near a
//! point (`EEGetUnitsNear`) and a unit's records, read in place ([`EeUnitsUuUnit`]).
//!
//! Determinism (RUST_PLAN 2.1): the C++'s statements and float operations in order, `long` as `i32`, the tile walks'
//! `unsigned long` coordinates wrapping as the C++'s did.

#![allow(clippy::unnecessary_cast, clippy::excessive_precision)] // the C++'s float constants as written

use crate::uu::EeUnitsUuUnit;
use crate::uu_tiles::Tile;
use ee_ge::types::GeVector;
use ee_ge::um::um_arctan2;

/// The world the helpers ask (the game's: cls_host.rs).
pub trait BuildingHost {
    /// a unit's records and pointer members (the unit utilities' read)
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit;
    /// `gWorld.mMap->GetTile(x, y)` (its unsigned bounds test; 0: off the map), its `mTerrainType`,
    /// `GetNumRaisedCorners(x, y)`, `mTerrainMesh.GetZ(x, y)`, `mTerrainMesh.GetIsSpaceMap()`, `mSize`
    fn map_tile(&mut self, x: i32, y: i32) -> u64;
    fn tile_terrain(&mut self, tile: u64) -> i32;
    fn raised_corners(&mut self, x: i32, y: i32) -> i32;
    fn mesh_z(&mut self, x: i32, y: i32) -> f32;
    fn is_space_map(&mut self) -> bool;
    fn map_size(&mut self) -> i32;
    /// `gContinentManager.GetContinentAt(x, y)` (0: none), the continent's `GetHullCenter()`
    fn continent_at(&mut self, x: i32, y: i32) -> u64;
    fn hull_center(&mut self, continent: u64) -> Tile;
    /// `unit->GetPosition(out)` (virtual); `EEGetUnitsNear(point, radius, out)`
    fn position(&mut self, unit: u64) -> [f32; 3];
    fn units_near(&mut self, point: [f32; 3], radius: f32) -> Vec<u64>;
    /// `gUnitSetManager`'s set `set` contains the unit type; the player's `mEnemies[id] != 0`; a complex unit's
    /// `mNextPart` (`next`) or `mPreviousPart`
    fn unit_set_contains(&mut self, unit_type: u32, set: i32) -> bool;
    fn enemy(&mut self, player: u64, other_id: i32) -> bool;
    fn part(&mut self, complex: u64, next: bool) -> u64;
}

/// the quarter of each side (0x4c8ef8) and the steps along the four sides (0x4c8f08)
const SIDE_QUARTERS: [i32; 4] = [0, 2, 1, 3];
const SIDE_STEPS: [[i32; 2]; 4] = [[0, 1], [1, 0], [0, -1], [-1, 0]];
const TERRAIN_DEEP_WATER: i32 = 1;
const TERRAIN_SPACE: i32 = 12;
/// the gate's opening range: half the unit's inner sphere plus 1 (0x447010); the search radius (0x447018)
const GATE_RANGE_BASE: f32 = 1.0;
const GATE_SEARCH_RADIUS: f32 = 2.5;
/// the unit set of the units a gate opens for (or against)
const GATE_SET: i32 = 0x62;
const MOVEMENT_LAND: u8 = 2;

/// `CountTerrainRun`: how many tiles of `terrain` follow (x, y) in steps of (dx, dy), up to `length` (the tiles'
/// coordinates are `unsigned long`s); whether all did.
pub fn terrain_run<H: BuildingHost + ?Sized>(
    h: &mut H,
    x: u32,
    y: u32,
    dx: i32,
    dy: i32,
    length: i32,
    terrain: i32,
) -> (bool, i32) {
    let (mut tx, mut ty) = (x, y);
    let mut i = 0;
    while i < length {
        tx = tx.wrapping_add(dx as u32);
        ty = ty.wrapping_add(dy as u32);
        let tile = h.map_tile(tx as i32, ty as i32);
        if tile == 0 || h.tile_terrain(tile) != terrain {
            break;
        }
        i += 1;
    }
    (i >= length, i)
}

/// `EEGetDockFacing`: the quarter facing the most space within 6 tiles (half weight to the neighbouring sides).
pub fn dock_facing<H: BuildingHost + ?Sized>(h: &mut H, x: u32, y: u32) -> i32 {
    let mut counts = [0i32; 4];
    for (i, step) in SIDE_STEPS.iter().enumerate() {
        counts[i] = terrain_run(h, x, y, step[0], step[1], 6, TERRAIN_SPACE).1;
    }
    let mut result = 0;
    let mut best = -1;
    for i in 0..4 {
        let score = (counts[(i + 3) & 3] >> 1) + (counts[(i + 1) & 3] >> 1) + counts[i];
        if score > best {
            best = score;
            result = SIDE_QUARTERS[i];
        }
    }
    result
}

/// `GetNavyYardFacing`: its map quarter when 5 deep water tiles follow that way, else the next side that has them,
/// else the side with the longest run.
pub fn navy_yard_facing<H: BuildingHost + ?Sized>(h: &mut H, x: u32, y: u32) -> i32 {
    let mut result = facing_quarter(h, x as i32, y as i32);
    let mut best = 0;
    let mut side = 0usize;
    while side < 4 {
        if SIDE_QUARTERS[side] == result {
            break;
        }
        side += 1;
    }
    let mut tries = 0;
    while tries < 4 {
        // (a quarter not in the table leaves the side at 4: the original read past its steps; the quarter is always
        // one of the four)
        let step = SIDE_STEPS[side & 3];
        let (all, count) = terrain_run(h, x, y, step[0], step[1], 5, TERRAIN_DEEP_WATER);
        if all {
            break;
        }
        if count > best {
            best = count;
            result = SIDE_QUARTERS[side & 3];
        }
        side += 1;
        if side >= 4 {
            side = 0;
        }
        tries += 1;
    }
    if tries < 4 {
        result = SIDE_QUARTERS[side & 3];
    }
    result
}

/// `EEGetFacingQuarter`: the map quarter (0..3) a tile faces from the middle of its continent (space maps) or of
/// the map.
pub fn facing_quarter<H: BuildingHost + ?Sized>(h: &mut H, x: i32, y: i32) -> i32 {
    let mut result = 2;
    let (fx, fy) = (x as f32, y as f32);
    let continent = if h.is_space_map() { h.continent_at(x, y) } else { 0 };
    let (center_x, center_y, middle_x, middle_y) = if continent != 0 {
        let c = h.hull_center(continent);
        (c[0], c[1], c[0] as f32, c[1] as f32)
    } else {
        let size = h.map_size();
        let middle = size as f32 * 0.5;
        (size / 2, size / 2, middle, middle)
    };
    if x != center_x || y != center_y {
        let mut direction = GeVector { x: middle_x - fx, y: middle_y - fy, z: 0.0 };
        direction.normalize();
        let angle = um_arctan2(-direction.x, direction.y);
        result = if (-0.78539819..=0.78539819).contains(&angle) {
            0
        } else if (-2.3561945..=-0.78539819).contains(&angle) {
            2
        } else if (0.78539819..=2.3561945).contains(&angle) {
            3
        } else {
            1
        };
    }
    result
}

/// `EETurnGatePart`: gate piece `index`'s animation (9 a post, 10 a diagonal post, 11 else) and the yaw it turns to
/// (None: its turn is kept), for a gate in direction 1..4.
pub fn gate_part_turn(direction: i32, index: i32) -> (i32, Option<f32>) {
    match (index, direction) {
        (4, 4) => (10, Some(2.3561945)),
        (4, 3) => (10, Some(-2.3561945)),
        (4, 2) => (9, Some(std::f32::consts::PI)),
        (4, 1) => (9, Some(1.5707964)),
        (0, 4) => (10, Some(-0.78539819)),
        (0, 3) => (10, Some(0.78539819)),
        (0, 2) => (9, Some(0.0)),
        (0, 1) => (9, Some(-1.5707964)),
        _ => (11, None),
    }
}

/// `EEGetWallGateTiles`: the five tiles of a gate line through a tile (1 along x, 2 along y, 3 and 4 the
/// diagonals; none for another direction).
pub fn wall_gate_tiles(t: Tile, direction: i32) -> Vec<Tile> {
    let (x, y) = (t[0], t[1]);
    let step = match direction {
        1 => [1, 0],
        2 => [0, 1],
        3 => [-1, 1],
        4 => [1, 1],
        _ => return Vec::new(),
    };
    (-2..=2).map(|i| [x + step[0] * i, y + step[1] * i]).collect()
}

/// `EEMap::GetMinimumCornerZ` / `GetMaximumCornerZ` (the nested windows.h macros; -1 off the map).
fn corner_z<H: BuildingHost + ?Sized>(h: &mut H, x: i32, y: i32, highest: bool) -> f32 {
    if h.map_tile(x, y) == 0 {
        return -1.0;
    }
    let z = [
        h.mesh_z(x, y),
        h.mesh_z(x.wrapping_add(1), y),
        h.mesh_z(x, y.wrapping_add(1)),
        h.mesh_z(x.wrapping_add(1), y.wrapping_add(1)),
    ];
    let pick = |a: f32, b: f32| {
        if highest {
            if a > b {
                a
            } else {
                b
            }
        } else if a < b {
            a
        } else {
            b
        }
    };
    pick(z[0], pick(z[1], pick(z[2], z[3])))
}

/// `EEAreTilesLevel`: every tile on the map, flat or a ramp (0 or 2 raised corners), and the heights within 0.6.
pub fn tiles_level<H: BuildingHost + ?Sized>(h: &mut H, tiles: &[Tile]) -> bool {
    let mut lowest = 100.0f32;
    let mut highest = -100.0f32;
    let mut ok = true;
    for t in tiles {
        if !ok {
            break;
        }
        let raised = h.raised_corners(t[0], t[1]);
        if h.map_tile(t[0], t[1]) != 0 {
            let z = corner_z(h, t[0], t[1], false);
            if lowest >= z {
                lowest = z;
            }
            let z = corner_z(h, t[0], t[1], true);
            if highest <= z {
                highest = z;
            }
            ok = raised == 0 || raised == 2;
        } else {
            ok = false;
        }
    }
    ok && highest - lowest <= 0.6
}

/// `DistanceSquaredTo` between two complex units' locations (the y difference first).
fn distance_squared(a: &EeUnitsUuUnit, b: &EeUnitsUuUnit) -> f32 {
    let dy = a.location[1] - b.location[1];
    let dx = a.location[0] - b.location[0];
    dx * dx + dy * dy
}

/// The opening range of a unit (squared: compared with squared distances).
fn opening_range(u: &EeUnitsUuUnit) -> f32 {
    // SAFETY: a live unit's class record
    let range = unsafe { (*u.class_fields).mInnerCollisionSphere } * 0.5 + GATE_RANGE_BASE;
    range * range
}

/// The gate piece and its next two: whether `other` is within `range` of any of them.
fn near_gate<H: BuildingHost + ?Sized>(h: &mut H, pieces: &[u64; 3], other: &EeUnitsUuUnit, range: f32) -> bool {
    for &piece in pieces {
        let p = h.unit(piece);
        if distance_squared(&p, other) <= range {
            return true;
        }
    }
    false
}

/// The gate piece, its next piece and the one after, and the units near it (from the last).
fn gate_search<H: BuildingHost + ?Sized>(h: &mut H, unit: u64) -> ([u64; 3], Vec<u64>) {
    let position = h.position(unit);
    let next = h.part(unit, true);
    let last = h.part(next, true);
    let units = h.units_near(position, GATE_SEARCH_RADIUS);
    ([unit, next, last], units)
}

/// `AreEnemiesNear`: a living enemy unit of the gate set within reach of the gate or its next two pieces.
pub fn enemies_near<H: BuildingHost + ?Sized>(h: &mut H, unit: u64) -> bool {
    let me = h.unit(unit);
    let (pieces, units) = gate_search(h, unit);
    let mut found = false;
    for &other in units.iter().rev() {
        if found {
            break;
        }
        let u = h.unit(other);
        // SAFETY: live records
        let (unit_type, hit_points) = unsafe { ((*u.class_fields).mUnitType as u32, (*u.unit).mHitPoints as i32) };
        if !h.unit_set_contains(unit_type, GATE_SET)
            || !(hit_points > 0)
            || me.player == u.player
            || !h.enemy(u.player, me.player_id)
        {
            continue;
        }
        found = near_gate(h, &pieces, &u, opening_range(&u));
    }
    found
}

/// `IsPasserNear`: a unit that may pass within reach of the gate or its next two pieces: a mobile land unit the
/// owner is not hostile to, or an enemy of the gate set.
pub fn passer_near<H: BuildingHost + ?Sized>(h: &mut H, unit: u64) -> bool {
    let me = h.unit(unit);
    let (pieces, units) = gate_search(h, unit);
    let mut found = false;
    for &other in units.iter().rev() {
        if found {
            break;
        }
        let u = h.unit(other);
        let friend = u.player == me.player || !h.enemy(me.player, u.player_id);
        // SAFETY: live records
        let (hit_points, domains, unit_type) =
            unsafe { ((*u.unit).mHitPoints as i32, (*u.class_fields).mDomains, (*u.class_fields).mUnitType as u32) };
        if hit_points > 0 {
            let passes = if friend {
                if u.stats_max_velocity <= 0.0 {
                    found = false;
                    continue;
                }
                domains & MOVEMENT_LAND != 0
            } else {
                h.unit_set_contains(unit_type, GATE_SET)
            };
            if passes {
                // the complex unit's location (`GetComplexUnit()`: the unit itself for a complex one)
                let c = if u.complex != 0 && u.complex != other { h.unit(u.complex) } else { u };
                if near_gate(h, &pieces, &c, opening_range(&u)) {
                    found = true;
                    continue;
                }
            }
        }
        found = false;
    }
    found
}

#[cfg(test)]
#[path = "cls_building_tests.rs"]
mod tests;
