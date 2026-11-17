//! The unit utilities, round 2 (the unit utilities lane): the bodies of `src/Empire Earth/User Interface
//! Thread/EEFOFindNewTargetNearOldTarget.cpp` (the original's "EEUnitUtilities.cpp") that read the units' frames and
//! write to the world: the submarines' depth, a unit's tiles, the selection bounds, the facing radius, the gather
//! reach, the escape point, the buildings' footprint and region registration, the site's step aside, the wall
//! builders' assignment and the wall plan, the walls on a line of fire, the path length, the sound parameters and the
//! deposit of a load.
//!
//! The world is the round-1 [`UuHost`] and the [`UuWorld`] questions and writes below (one C++ function each,
//! `ee_units_uu_host_world`). Determinism (RUST_PLAN 2.1): the C++'s statements and float operations in order, its
//! sorts as MSVC sorts them ([`crate::msvc_sort`]), every C++ leaf with a record of its own (`EEGetTileLine`'s CBTL)
//! called as often and in the same order.

// `c_long` is 32 bits on Windows and 64 on LP64: the casts are needed there. The C++'s negated float comparisons
// (`!(a > b)`) are kept as written: they differ from the flipped comparison for a NaN.
#![allow(clippy::unnecessary_cast, clippy::neg_cmp_op_on_partial_ord)]

use std::collections::BTreeMap;

use crate::units::msvc_sort;
use crate::units::uu::{
    extend_wall_line, footprint_tiles, make_wall_junctions, pick_wall_pieces, tile_unit_of_type, wall_line,
    UuClassView, UuHost, UuView,
};
use crate::units::uu_place::can_place_unit_on_tile;
use crate::units::uu_tiles::{
    ceil, move_in_direction, tile_direction, tiles_near_point, trunc_long, turn_direction, wall_piece, Tile,
};
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge2DPoint, Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um::{floor, floor_unsigned, lrintf, sqrt, um_arccos, um_arctan2, um_cos};
use std::f32::consts::{FRAC_2_PI, FRAC_PI_2, PI, TAU};

/// The values the bodies test (the C++ enums').
/// cbindgen:ignore
pub mod k2 {
    pub const UU_TYPE_FARM: u32 = 0x2a;
    pub const UU_TYPE_TOWN_CENTER: i32 = 0x23;
    pub const UU_TYPE_CAPITOL: i32 = 0x29;
    pub const UU_TYPE_SETTLEMENT: i32 = 0x2b;
    pub const UU_TYPE_GRANARY: i32 = 0x34;
    pub const UU_TYPE_GRANARY_IMPROVED: i32 = 0x2ba;
    pub const UU_CATEGORY_NAVAL_BUILDING: u32 = 0x15;
    pub const UU_CATEGORY_SPACE_BUILDING: u32 = 0x20;
    pub const UU_CATEGORY_GATE: u32 = 0x19;
    pub const UU_FAMILY_ANIMAL: u32 = 0x11;
    pub const UU_MOVEMENT_SEA: u8 = 4;
    pub const UU_GOAL_BUILD_REPAIR: i32 = 4;
    pub const UU_ACTION_GATHER_RESOURCE: i32 = 4004;
    pub const UU_ACTION_WAIT_FOR_OPEN_POSITION: i32 = 4026;
    pub const UU_ACTION_IDLE: i32 = 5000;
    pub const UU_RESOURCE_FOOD: u32 = 2;
    pub const UU_RESOURCE_WOOD: u32 = 3;
    pub const UU_RESOURCE_STONE: u32 = 4;
    pub const UU_RESOURCE_GOLD: u32 = 5;
    pub const UU_RESOURCE_IRON: u32 = 6;
}
use k2::*;

/// The sound falloff factors (`gF564520`, `gF56451C`, `gF564518`: the unit's dynamic initializers, unfolded).
const PAN_DEAD_ZONE: f32 = 0.25;
const EDGE_FALLOFF: f32 = 0.25;
const BEHIND_LIMIT: f32 = 4.0;

/// The camera's view of a sound's location (gathered by the C++ forwarder): whether the current player
/// sees its tile, then (only then) `EEGetCameraPoint`'s screen point and depth, the camera's position
/// (`gWorld.mTransformation`), `gViewport.GetYClippingPlane()` and the camera control's zoom.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUuSoundView {
    pub seen: u8,
    pub screen: [f32; 3],
    pub depth: f32,
    pub camera: [f32; 3],
    pub y_clip: f32,
    pub zoom: f32,
}

/// The world's questions and writes of the round-2 bodies (the C++ in the game: `ee_units_uu_host_world`).
pub trait UuWorld: UuHost {
    /// `complex->GetLineOfSight()` (EEComplexUnit's out-of-line reader)
    fn line_of_sight(&mut self, complex: u64) -> f32;
    /// `EEGetUnitsNear(center, radius, out)`
    fn units_near(&mut self, center: [f32; 3], radius: f32) -> Vec<u64>;
    /// `EEIsInGatherReach(distance, unit, target)` (ee_combat's reach over the classes)
    fn gather_reach(&mut self, distance: f32, unit: u64, target: u64) -> bool;
    /// `tile->mUnits = tile->AddUnit(unit, tile->mUnits)` (`add`) or `tile->RemoveUnit(unit)`
    fn tile_unit(&mut self, tile: u64, unit: u64, add: bool);
    /// `EEClearUnderBuilding`'s step for one unit: a deletable ambient unit other than a path point loses its
    /// resources and is killed (`Kill`, its player's `KillUnit`)
    fn clear_ambient(&mut self, unit: u64);
    /// `ee_combat_regions_footprint(gWorld.mMap->mUnitRegions, x, y, size, complex, add)`; `gLoadingSavedGame`
    fn regions_footprint(&mut self, x: i32, y: i32, size: i32, complex: u64, add: bool);
    fn loading_saved_game(&mut self) -> bool;
    /// `complex->StepAside(player, class, x, y, force)`
    fn step_aside(&mut self, complex: u64, player: u64, class: u64, x: i32, y: i32, force: bool);
    /// the active group's `GetNumUnits()` and `GetUnit(i)` (as its `EEUnit*`)
    fn group_len(&mut self, group: u64) -> i32;
    fn group_unit(&mut self, group: u64, i: i32) -> u64;
    /// `unit->AddBuildTarget(piece)`; a builder starting over (`ClearGoals(0)`, `StartAction(kActionIdle)`,
    /// `mUnitClass->ActionChanged(unit)`)
    fn add_build_target(&mut self, unit: u64, piece: u64);
    fn restart_idle(&mut self, unit: u64);
    /// `player->CanProduce(class->GetTechnology(), 0x20, 1, true, 3, NULL)`
    fn can_produce(&mut self, player: u64, class: u64) -> bool;
    /// `EEPlanWall`'s new piece at (x, y): `CreateUnit`, `EEShapeWall` with the tile's raised corners, then a paid
    /// foundation of one hit point (`foundation`) or a finished piece (with a space wall's energy wall); paid for
    fn make_wall_piece(&mut self, class: u64, player: u64, x: i32, y: i32, corner: i32, foundation: bool) -> u64;
    /// a wall piece's rise above its tile (`EEIsTileTopAbove`: its model box's top in its frame, 1.2 for a gate,
    /// times its stats' scale, times its health while not built)
    fn wall_rise(&mut self, wall: u64) -> f32;
    /// `gWorld.GetMap()->mSize`; `gPathManager.FindPath(unit, map, from, to, bounds, path, 0xb, limit, walls ?
    /// new EEAIBPGModifierWallsDoNotBlock : NULL, NULL)`: the path's first tile (None: an empty path)
    fn map_size(&mut self) -> i32;
    fn find_path(&mut self, unit: u64, from: Tile, to: Tile, bounds: [i32; 4], limit: u32, walls: bool)
        -> Option<Tile>;
    /// `inContainer->CanTake(unit, kind)` (its answer unused); `gWorld.GetGameVariant()->mDepositFactor`;
    /// `unit->mLastResource` (as its `EEUnit*`); `complex->DistanceSquaredToUnit(other)`
    fn can_take(&mut self, container: u64, unit: u64, kind: u32);
    fn deposit_factor(&mut self) -> f32;
    fn last_resource(&mut self, unit: u64) -> u64;
    fn distance_squared_to_unit(&mut self, complex: u64, other: u64) -> f32;
    /// the deposit's bonus effect on the unit (`kGraphicEffectDisbandUnitEmitter`); the container's player's
    /// `AddResource(kind, amount)` (the trace of a computer player's deposit with it); its statistics'
    /// `Reset(series, amount)`
    fn deposit_effect(&mut self, unit: u64);
    fn add_resource(&mut self, container: u64, unit: u64, amount: i32, carried: i32, near: bool, bonus: f32);
    fn statistic(&mut self, container: u64, series: i32, amount: i32);
    /// the scenario editor's unit under the mouse, when it is the player's transport with room for a unit of the
    /// type that may enter it (`EEEditorCanPlace`'s first test)
    fn editor_transport_takes(&mut self, player: u64, unit_type: i32) -> bool;
    /// the complex units of unit region (x, y) (`EEUnitRegion(gWorld.mMap->mUnitRegions, x, y)`, its entries in
    /// order; NULL entries as 0)
    fn region_units(&mut self, x: i32, y: i32) -> Vec<u64>;
}

fn view<H: UuHost + ?Sized>(h: &mut H, obj: u64) -> UuView {
    UuView(h.unit(obj))
}

fn tile_units<H: UuHost + ?Sized>(h: &mut H, tile: u64) -> Vec<u64> {
    let mut out = Vec::new();
    h.tile_units(tile, &mut out);
    out
}

fn vec3(p: [f32; 3]) -> Ge3DPoint {
    Ge3DPoint { x: p[0], y: p[1], z: p[2] }
}

/// A zero test of each component within 0.0001, else the header's `Normalize` (EEGetDopplerPitch's two vectors).
fn zero_or_normalize(v: &mut GeVector) {
    if v.x >= -0.0001 && v.x <= 0.0001 && v.y >= -0.0001 && v.y <= 0.0001 && v.z >= -0.0001 && v.z <= 0.0001 {
        *v = GeVector::default();
    } else {
        v.normalize();
    }
}

// ---- the submarines' depth ----

/// `EEGetSeabedUnder` (0x23efa1): the highest terrain under the unit's tiles (both rows of each tile, the far edge
/// of the last), 0 once a tile is off the mesh. An empty tile list (an unplaced unit) reads past the C++'s vector;
/// here it keeps the ground under the point.
pub fn seabed_under<H: UuWorld + ?Sized>(h: &mut H, unit: u64, x: f32, y: f32) -> f32 {
    let map = h.map();
    let mut z = h.mesh_ground(x, y);
    let mut tiles = Vec::new();
    unit_tiles(h, unit, &mut tiles, 1.0);
    let higher = |h: &mut H, z: f32, tx: i32, ty: i32| {
        let height = h.mesh_z(map, tx, ty);
        if z > height {
            z
        } else {
            height
        }
    };
    for t in &tiles {
        if h.mesh_tile_is(0, t[0] as u32, t[1] as u32) {
            z = higher(h, z, t[0], t[1]);
            z = higher(h, z, t[0], t[1] + 1);
        } else {
            z = 0.0;
        }
    }
    let Some(last) = tiles.last() else { return z };
    if h.mesh_tile_is(0, (last[0] + 1) as u32, last[1] as u32) {
        z = higher(h, z, last[0] + 1, last[1]);
        z = higher(h, z, last[0] + 1, last[1] + 1);
    } else {
        z = 0.0;
    }
    z
}

/// `EEGetSeabedDepth` (0x23f27e): the cruise depth, or the surface (-0.4) when the seabed is as high as the point.
pub fn seabed_depth<H: UuWorld + ?Sized>(h: &mut H, unit: u64, at: [f32; 3]) -> f32 {
    let ground = seabed_under(h, unit, at[0], at[1]);
    let depth = view(h, unit).class().cruise_altitude();
    if ground >= at[2] {
        -0.4
    } else {
        -depth
    }
}

/// `EEAdjustDiveDepth` (0x23f2c0): one step of 0.005 up (to the surface at most) or down (to the cruise depth).
pub fn adjust_dive_depth<H: UuWorld + ?Sized>(h: &mut H, unit: u64, at: [f32; 3]) -> f32 {
    let ground = seabed_under(h, unit, at[0], at[1]);
    let depth = -view(h, unit).class().cruise_altitude();
    if ground >= at[2] {
        let z = at[2] + 0.005;
        if z >= -0.4 {
            -0.4
        } else {
            z
        }
    } else {
        let z = at[2] + -0.005;
        if depth > z {
            depth
        } else {
            z
        }
    }
}

/// `EEGetUnitTiles` (0x248fe4): the tiles a unit covers widened by `margin`: a building's footprint, a ship's line
/// along its heading (`EEGetTileLine` with both ends), else the disc around its position (appended).
pub fn unit_tiles<H: UuHost + ?Sized>(h: &mut H, unit: u64, out: &mut Vec<Tile>, margin: f32) {
    let u = view(h, unit);
    if u.tile_x() == -10000 || u.tile_y() == -10000 {
        return;
    }
    let class = u.class();
    if class.tile_size() > 1 {
        footprint_tiles(h, unit, out);
    } else if u.0.complex != 0 && class.domains() & UU_MOVEMENT_SEA != 0 {
        let mut direction = h.location(u.0.complex).apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
        direction.normalize();
        let scale = margin + class.inner();
        let direction = GeVector { x: scale * direction.x, y: scale * direction.y, z: scale * direction.z };
        let p = h.position(unit);
        let front = [direction.x * 0.5 + p[0], direction.y * 0.5 + p[1]];
        let back = [direction.x * -0.5 + p[0], direction.y * -0.5 + p[1]];
        *out = h.tile_line(floor(front[0]), floor(front[1]), floor(back[0]), floor(back[1]), true, false);
    } else {
        let p = h.position(unit);
        tiles_near_point(p[0], p[1], margin + class.inner(), out);
    }
}

// ---- the selection and the facing ----

/// `EEGetSelectionBounds` (0x241743): the rectangle (left, top, right, bottom) around the units' tiles: the first
/// unit's always, the others' through the terrain filter (1: units that cannot move on terrain 1, 2: those that
/// can, 3: those that can move on terrain 12). An empty list leaves the rectangle.
pub fn selection_bounds<H: UuHost + ?Sized>(h: &mut H, units: &[u64], bounds: &mut [f32; 4], filter: i32) {
    let Some(&first) = units.first() else { return };
    let f = view(h, first);
    let (x, y) = (f.tile_x() as f32, f.tile_y() as f32);
    *bounds = [x, y, x, y];
    for &u in &units[1..] {
        let v = view(h, u);
        let include = match filter {
            1 => !h.can_move_on_terrain(v.0.class_obj, u, 1),
            2 => h.can_move_on_terrain(v.0.class_obj, u, 1),
            3 => h.can_move_on_terrain(v.0.class_obj, u, 0xc),
            _ => true,
        };
        if !include {
            continue;
        }
        let (x, y) = (v.tile_x() as f32, v.tile_y() as f32);
        bounds[0] = if bounds[0] < x { bounds[0] } else { x };
        bounds[2] = if bounds[2] > x { bounds[2] } else { x };
        bounds[1] = if bounds[1] < y { bounds[1] } else { y };
        bounds[3] = if bounds[3] > y { bounds[3] } else { y };
    }
}

/// `EEGetUnitRadius` (0x248d05): the unit's collision radius seen from the other: whole ahead or behind, down to a
/// tenth from the side.
pub fn unit_radius<H: UuHost + ?Sized>(h: &mut H, unit: u64, other: u64) -> f32 {
    let o = h.unit(other).location;
    let u = h.unit(unit);
    let heading = h.location(u.complex).apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    let facing = um_arctan2(-heading.x, heading.y);
    let delta = facing - um_arctan2(o[0] - u.location[0], -(o[1] - u.location[1]));
    let mut angle = if delta < 0.0 { -delta } else { delta };
    if angle > PI {
        angle = TAU - angle;
    }
    if angle > FRAC_PI_2 {
        angle = PI - angle;
    }
    angle = 1.0 - angle * FRAC_2_PI;
    if angle <= 0.1 {
        angle = 0.1;
    }
    UuView(u).class().inner() * angle
}

/// `EEGetAngleFromNorth` (0x242345): the angle between north (an identity frame's forward, flattened) and the
/// direction from `to` to `from`.
#[allow(clippy::manual_clamp)] // the C++'s two tests (the same for a NaN)
pub fn angle_from_north(from: [f32; 3], to: [f32; 3]) -> f32 {
    let mut direction = GeVector { x: from[0] - to[0], y: from[1] - to[1], z: 0.0 };
    direction.normalize();
    let mut heading = GeTransformation::IDENTITY.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    heading.z = 0.0;
    heading.normalize();
    let mut cosine = heading.dot(direction);
    if cosine > 1.0 {
        cosine = 1.0;
    } else if cosine < -1.0 {
        cosine = -1.0;
    }
    let angle = um_arccos(cosine);
    if angle < 0.0 {
        -angle
    } else {
        angle
    }
}

// ---- the gather reach ----

/// `EEIsWithinReach` (0x24745d, `at` None: from the unit's position) and `EEIsWithinReachOf` (0x2475a0, from `at`):
/// the 2D distance to the target's point (a dead complex unit's model top, else its position; an unplaced target's
/// tile centre) against `EEIsInGatherReach`.
pub fn within_reach<H: UuWorld + ?Sized>(h: &mut H, unit: u64, target: u64, at: Option<[f32; 3]>) -> bool {
    let unit_position = if at.is_none() { Some(h.position(unit)) } else { None };
    let t = view(h, target);
    let distance = if t.on_map() {
        let p = h.drop_point(target);
        let from = at.or(unit_position).unwrap_or_default();
        Ge2DPoint { x: p[0], y: p[1] }.distance_to_squared(Ge2DPoint { x: from[0], y: from[1] })
    } else {
        let tile = Ge2DPoint { x: t.tile_x() as f32 + 0.5, y: t.tile_y() as f32 + 0.5 };
        match at {
            Some(l) => Ge2DPoint { x: l[0], y: l[1] }.distance_to_squared(tile),
            None => {
                let l = h.unit(unit).location;
                (l[0] - tile.x) * (l[0] - tile.x) + (l[1] - tile.y) * (l[1] - tile.y)
            }
        }
    };
    h.gather_reach(distance, unit, target)
}

/// `EEIsFootprintVisibleTo` (0x24793c): whether the player sees a tile of the unit's footprint (its own tile for a
/// one-tile unit); false without a map.
pub fn footprint_visible<H: UuHost + ?Sized>(h: &mut H, unit: u64, player: u64) -> bool {
    let map = h.map();
    if map == 0 {
        return false;
    }
    let u = h.unit(unit);
    if UuView(u).class().tile_size() > 1 {
        let mut tiles = Vec::new();
        footprint_tiles(h, unit, &mut tiles);
        let mut result = false;
        for t in tiles {
            if result {
                break;
            }
            let tile = h.map_tile(map, t[0], t[1]);
            result = tile != 0 && h.tile_visible_to(tile, player);
        }
        result
    } else {
        u.tile != 0 && h.tile_visible_to(u.tile, player)
    }
}

// ---- the escape point ----

/// `EEGetEnemiesAround` (0x247125): the living units of the unit's enemies within its line of sight (`EEGetUnitsNear`
/// from its location).
pub fn enemies_around<H: UuWorld + ?Sized>(h: &mut H, unit: u64) -> Vec<u64> {
    let u = h.unit(unit);
    let sight = h.line_of_sight(u.complex);
    let near = h.units_near(u.location, sight);
    let mut out = Vec::new();
    for other in near {
        let o = h.unit(other);
        if UuView(o).hit_points() > 0 && o.player != u.player && h.enemy(u.player, o.player_id) {
            out.push(other);
        }
    }
    out
}

/// `EEGetUnitAngle` (0x242489): the heading from one unit's tile to the other's, kept in [-pi, pi] by a half turn.
pub fn unit_angle<H: UuHost + ?Sized>(h: &mut H, from: u64, to: u64) -> f32 {
    let f = view(h, from);
    let t = view(h, to);
    let dy = t.tile_y() as f32 - f.tile_y() as f32;
    let dx = t.tile_x() as f32 - f.tile_x() as f32;
    let mut angle = um_arctan2(-dx, dy);
    if angle > PI {
        angle -= PI;
    }
    if angle < -PI {
        angle += PI;
    }
    angle
}

/// `EEGetEscapePoint` (0x246254): where a computer player's unit flees its attackers: the middle of the widest gap
/// between them (sorted by angle, MSVC's sort), pushed out to its reach; else (one attacker, or the gap's point off
/// the map) a step of half its sight away from the first attacker. `out` is left alone without attackers.
pub fn escape_point<H: UuWorld + ?Sized>(h: &mut H, unit: u64, out: &mut [f32; 2]) -> bool {
    let complex = h.unit(unit).complex;
    let reach = lrintf(h.line_of_sight(complex));
    let mut attackers: Vec<(u64, f32)> = enemies_around(h, unit).into_iter().map(|a| (a, 0.0)).collect();
    if attackers.is_empty() {
        return false;
    }
    for a in attackers.iter_mut() {
        a.1 = unit_angle(h, unit, a.0);
    }
    let n = attackers.len();
    let mut best = 0usize;
    let angle = if n > 1 {
        msvc_sort::sort_by(&mut attackers, |a, b| a.1 < b.1);
        let mut start = attackers[0].1;
        let mut gap = attackers[1].1 - start;
        for i in 1..n - 1 {
            let from = attackers[i].1;
            let to = attackers[i + 1].1;
            if !(to - from > gap) {
                continue;
            }
            gap = to - from;
            start = from;
            best = i;
        }
        gap * 0.5 + start
    } else {
        -attackers[0].1
    };
    let across = -um_cos(FRAC_PI_2 - angle);
    let along = um_cos(angle);
    let first = attackers[best].0;
    let second = attackers[(best + 1) % n].0;
    if first != second {
        // (the attackers' complex units' locations)
        let fl = h.unit(first).location;
        let sl = h.unit(second).location;
        let direction = GeVector { x: sl[0] - fl[0], y: sl[1] - fl[1], z: 0.0 };
        if direction.x != 0.0 || direction.y != 0.0 {
            let point = [direction.x * 0.5 + fl[0], direction.y * 0.5 + fl[1]];
            let half = direction.magnitude_squared();
            let radius = reach.wrapping_add(1).wrapping_mul(reach.wrapping_add(1)) as f32;
            if half < radius {
                let distance = sqrt(radius - half);
                let offset = [across * distance, along * distance];
                *out = [point[0] + offset[0], point[1] + offset[1]];
            } else {
                *out = point;
            }
            let map = h.map();
            if h.map_tile(map, floor(out[0]), floor(out[1])) != 0 {
                return true;
            }
        }
    }
    let u = view(h, unit);
    let a = view(h, attackers[0].0);
    let direction = turn_direction(tile_direction(u.tile_x(), u.tile_y(), a.tile_x(), a.tile_y()), 4, true);
    let distance = if h.line_of_sight(complex) > 0.0 { h.line_of_sight(complex) * 0.5 } else { 1.0 };
    *out = move_in_direction(u.tile_x() as f32, u.tile_y() as f32, direction, lrintf(distance));
    let map = h.map();
    h.map_tile(map, floor(out[0]), floor(out[1])) != 0
}

// ---- the buildings' footprint and regions ----

/// `EEAddToFootprintTiles` (0x24caef, `add`) / `EERemoveFromFootprintTiles` (0x24d946): the unit in (or out of) the
/// unit list of every tile of its footprint, its own tile first.
pub fn footprint_link<H: UuWorld + ?Sized>(h: &mut H, unit: u64, add: bool) {
    let u = view(h, unit);
    let size = u.class().tile_size();
    let (x, y) = (u.tile_x(), u.tile_y());
    let map = h.map();
    let tile = h.map_tile(map, x, y);
    if tile != 0 {
        h.tile_unit(tile, unit, add);
    }
    let (end_x, end_y) = (x.wrapping_add(size), size.wrapping_add(y));
    let mut tx = x;
    while tx < end_x {
        let mut ty = y;
        while ty < end_y {
            if !(tx == x && ty == y) {
                let tile = h.map_tile(map, tx, ty);
                if tile != 0 {
                    h.tile_unit(tile, unit, add);
                }
            }
            ty += 1;
        }
        tx += 1;
    }
}

/// `EEClearUnderBuilding` (0x2450f1): the deletable ambient units on the square cleared (each tile's list as it
/// stands when the tile is reached).
pub fn clear_under_building<H: UuWorld + ?Sized>(h: &mut H, x: i32, y: i32, size: i32) {
    let map = h.map();
    let mut tx = x;
    while tx < x.wrapping_add(size) {
        let mut ty = y;
        while ty < y.wrapping_add(size) {
            let tile = h.map_tile(map, tx, ty);
            if tile != 0 {
                for u in tile_units(h, tile) {
                    if u != 0 {
                        h.clear_ambient(u);
                    }
                }
            }
            ty += 1;
        }
        tx += 1;
    }
}

/// `EEAddBuildingRegion` (0x24c8bc, `add`) / `EERemoveBuildingRegion` (0x24d779): the fortress in (or out of) the
/// unit regions under its footprint; once entered outside a load, the ambient units under it and its linked pieces
/// are cleared (not for a naval or space building with air traffic).
pub fn building_region<H: UuWorld + ?Sized>(h: &mut H, unit: u64, add: bool) {
    let u = view(h, unit);
    let class = u.class();
    if !u.on_map() {
        return;
    }
    h.regions_footprint(u.tile_x(), u.tile_y(), class.tile_size(), u.0.complex, add);
    if !add || h.loading_saved_game() {
        return;
    }
    let category = class.category();
    if (category == UU_CATEGORY_NAVAL_BUILDING || category == UU_CATEGORY_SPACE_BUILDING)
        && class.air_traffic_type() != 0
    {
        return;
    }
    let mut piece = unit;
    while piece != 0 {
        let p = view(h, piece);
        clear_under_building(h, p.tile_x(), p.tile_y(), p.class().tile_size());
        piece = if p.on_map() { h.part(p.0.complex, true) } else { 0 };
    }
}

/// `EEStepAsideFromSite` (0x24edf6): the units on a site's footprint (the unit's, else the class's square centred
/// on the tile) make way, unless (without `force`) they are another player's that is not an ally or are busy (not
/// idle, waiting or gathering); harmless animals always do. A flier above its tile is left. Removed units and
/// units without stats, class or tile are skipped (the port's guards).
pub fn step_aside_from_site<H: UuWorld + ?Sized>(h: &mut H, unit: u64, class: u64, player: u64, at: Tile, force: bool) {
    let mut tiles = Vec::new();
    if unit != 0 {
        footprint_tiles(h, unit, &mut tiles);
    } else {
        let size = UuClassView(h.class(class).fields, class).tile_size();
        let half = size as f32 * 0.5;
        let left = at[0].wrapping_sub(floor_unsigned(half) as i32);
        let top = at[1].wrapping_sub(floor_unsigned(half) as i32);
        for x in left..left.wrapping_add(size) {
            for y in top..top.wrapping_add(size) {
                tiles.push([x, y]);
            }
        }
    }
    let map = h.map();
    for t in tiles {
        let tile = h.map_tile(map, t[0], t[1]);
        if tile == 0 {
            continue;
        }
        for other in tile_units(h, tile) {
            if other == 0 {
                continue;
            }
            let o = h.unit(other);
            if o.complex == 0 || h.is_foundation(o.complex) {
                continue;
            }
            let c = h.unit(o.complex);
            // SAFETY: the host's record of a live unit
            let valid = unsafe { (*o.rts).mIsValid };
            if !valid || o.tile == 0 || c.has_stats == 0 || c.class_fields.is_null() {
                continue;
            }
            let cc = UuView(c).class();
            if (cc.family() != UU_FAMILY_ANIMAL || c.stats_attack != 0.0) && !force {
                if o.player != player && (o.player == 0 || player == 0 || h.enemy(player, o.player_id)) {
                    continue;
                }
                let command = c.command;
                if command != UU_ACTION_IDLE
                    && command != UU_ACTION_WAIT_FOR_OPEN_POSITION
                    && command != UU_ACTION_GATHER_RESOURCE
                {
                    continue;
                }
            }
            if !cc.can_fly() || c.location[2] <= h.tile_height(c.tile) {
                h.step_aside(o.complex, player, class, t[0], t[1], force);
            }
        }
    }
}

// ---- the walls ----

/// `EEBuildWall` (0x23e630): the wall pieces handed out to the group's builders: spread evenly first (the first
/// builder at the first piece, the last at the last), then every builder steps along (the first forward, the last
/// back, the others alternately further each way) until each piece has one; the builders that had another goal
/// start over (by unit id). An empty group or wall does nothing (the C++ loops forever or reads past the list).
pub fn build_wall<H: UuWorld + ?Sized>(h: &mut H, group: u64, pieces: &[u64]) {
    let n = pieces.len() as i32;
    let units = h.group_len(group);
    if units <= 0 || n == 0 {
        return;
    }
    let spacing = if units > 1 { ceil((n - 2) as f32 / (units - 1) as f32) } else { 0 };
    let mut counts = vec![0i32; n as usize];
    let mut targets = vec![0i32; units as usize];
    let mut steps = vec![0i32; units as usize];
    let mut restart: BTreeMap<u32, u64> = BTreeMap::new();
    let mut assign = |h: &mut H, i: i32, piece: i32, targets: &mut [i32], steps: &mut [i32], counts: &mut [i32]| {
        // a target off the list (only an impossible spread) wraps instead of reading past it
        let piece = piece.rem_euclid(n);
        targets[i as usize] = piece;
        let u = h.group_unit(group, i);
        let kind = h.goal_kind(u);
        if kind != -1 && kind != UU_GOAL_BUILD_REPAIR {
            let id = view(h, u).id();
            restart.insert(id, u);
        }
        h.add_build_target(u, pieces[piece as usize]);
        steps[i as usize] += 1;
        counts[piece as usize] += 1;
    };
    let mut next = 0;
    for i in 0..units {
        let piece = if i == 0 {
            0
        } else if i == units - 1 {
            n - 1
        } else {
            next += spacing;
            if next >= n {
                next = 0;
            }
            next
        };
        assign(h, i, piece, &mut targets, &mut steps, &mut counts);
    }
    while counts.contains(&0) {
        for j in 0..units {
            let old = targets[j as usize];
            let step = steps[j as usize];
            let mut new = if j == 0 {
                old + 1
            } else if j == units - 1 {
                old - 1
            } else if step % 2 == 0 {
                old + step
            } else {
                old - step
            };
            if new < 0 {
                new += n;
            }
            if new >= n {
                new -= n;
            }
            assign(h, j, new, &mut targets, &mut steps, &mut counts);
        }
    }
    for (_, u) in restart {
        h.restart_idle(u);
    }
}

/// `EEShapeWall` (0x24e647): the piece's frame set and facing for its corner (`EEOrientWallPiece`).
pub fn shape_wall<H: UuHost + ?Sized>(h: &mut H, complex: u64, corner: i32) {
    let mut m = h.location(complex);
    let piece = wall_piece(corner);
    if piece.turns != 0 {
        m.set_orientation(RotationOrder::Ypr, piece.yaw, 0.0, 0.0);
    }
    h.set_animation(complex, piece.frame_set);
    h.set_transformation(complex, &m);
}

/// `EEPlanWall` (0x23ebb2): the wall of `class` from `start` to `end` for the player: a new piece on every tile of
/// the line (extended to the walls it joins) that it can afford (as a foundation) and place; the player's existing
/// pieces on the others reshaped, and those with three neighbours made junctions. The new pieces are appended.
#[allow(clippy::too_many_arguments)]
pub fn plan_wall<H: UuWorld + ?Sized>(
    h: &mut H,
    start: [f32; 2],
    end: [f32; 2],
    class: u64,
    player: u64,
    foundation: bool,
    skip_player_checks: bool,
    out: &mut Vec<u64>,
) {
    let unit_type = UuClassView(h.class(class).fields, class).unit_type();
    let line = wall_line(h, [trunc_long(start[0]), trunc_long(start[1])], [trunc_long(end[0]), trunc_long(end[1])]);
    let mut tiles = Vec::new();
    extend_wall_line(h, &line, player, unit_type, &mut tiles);
    let map = h.map();
    let corners = pick_wall_pieces(h, map, &tiles);
    let mut existing = Vec::new();
    let mut placed = Vec::new();
    for (i, &[x, y]) in tiles.iter().enumerate() {
        let mut flag = None;
        if (!foundation || h.can_produce(player, class))
            && can_place_unit_on_tile(h, player, 0, unit_type as i32, x, y, skip_player_checks, false, &mut flag)
        {
            let piece = h.make_wall_piece(class, player, x, y, corners[i], foundation);
            placed.push([x, y]);
            out.push(piece);
        } else if h.map_tile(map, x, y) != 0 {
            // (the C++ reads the tile's raised corners here for EEShapeWall, which does not use them)
            let piece = tile_unit_of_type(h, [x, y], unit_type, Some(player));
            if piece != 0 {
                let p = view(h, piece);
                let complex = p.0.complex;
                if complex != 0 {
                    shape_wall(h, complex, corners[i]);
                }
                let pc = p.class();
                if p.hit_points() > 0
                    && p.0.player == player
                    && pc.is_wall()
                    && pc.category() != UU_CATEGORY_GATE
                    && complex != 0
                    && view(h, complex).animation() != 0xb
                {
                    existing.push(complex);
                }
            }
        }
    }
    make_wall_junctions(h, &existing, &placed, player, map);
}

// ---- the scenario editor's placement ----

/// `EEEditorCanPlace` (0x24f4f2): whether the scenario editor (free camera) may place a unit of the type at the tile:
/// into the player's transport under the mouse; else on a terrain the class allows, flat unless it builds on
/// elevation, without a blocking unit, and (a one-tile path point or complex unit) apart from the units around
/// (`editor_spacing_clear`); a land building or the type 0x167 anywhere on the map; never next to the map's edge.
pub fn editor_can_place<H: UuWorld + ?Sized>(
    h: &mut H,
    player: u64,
    unit_type: i32,
    at: [f32; 3],
    x: i32,
    y: i32,
) -> bool {
    let map = h.map();
    let c = h.class_of_type(unit_type);
    let class = UuClassView(c.fields, c.obj);
    let mut result = unit_type == 0x167;
    let tile = h.map_tile(map, x, y);
    let mut edges = false;
    if tile != 0 && !result {
        if h.editor_transport_takes(player, unit_type) {
            edges = true;
        } else {
            let terrain = h.tile_terrain(tile);
            result = class.terrain(terrain);
            if result {
                if !class.builds_on_elevation() && !h.is_tile_flat(x, y) {
                    result = false;
                } else {
                    let mut units = Vec::new();
                    result = !crate::units::uu::tile_units_standing(h, player, unit_type, tile, false, 2, &mut units);
                }
            }
            if class.tile_size() <= 1 && result {
                if c.land_building != 0 {
                    edges = true;
                } else if class.unit_type() == crate::units::uu::k::UU_TYPE_PATH_POINT || class.is_complex() == 1 {
                    result = editor_spacing_clear(h, class, at, x, y);
                }
            }
        }
    } else if tile != 0 || result {
        result = true;
    }
    if !edges && !result {
        return false;
    }
    h.map_tile(map, x + 1, y) != 0
        && h.map_tile(map, x - 1, y) != 0
        && h.map_tile(map, x, y + 1) != 0
        && h.map_tile(map, x, y - 1) != 0
}

/// `EEIsEditorSpacingClear` (0x24f6f7): whether no unit in the nine unit regions around the tile stands closer to
/// the point than half the two inner radii (units of a set the class may overlap excepted); the last unit asked
/// decides, as in the C++.
pub fn editor_spacing_clear<H: UuWorld + ?Sized>(h: &mut H, class: UuClassView, at: [f32; 3], x: i32, y: i32) -> bool {
    let size = h.regions_size();
    let (rx, ry) = (x / size, y / size);
    let mut result = true;
    let mut ty = ry - 1;
    while result && ty <= ry + 1 {
        let mut tx = rx - 1;
        while result && tx <= rx + 1 {
            for u in h.region_units(tx, ty) {
                if !result {
                    break;
                }
                if u == 0 {
                    continue;
                }
                let p = h.position(u);
                let other = view(h, u).class();
                let near = vec3(at).distance_to(vec3(p)) <= (class.inner() + other.inner()) * 0.5;
                result = !(near && !h.unit_set_contains(other.unit_type(), class.overlap_unit_set()));
            }
            tx += 1;
        }
        ty += 1;
    }
    result
}

// ---- the walls on a line ----

/// `EEIsTileTopAbove` (0x24dbd2): whether the tile's first wall piece (passable pieces skipped) rises above
/// `height`: its rise (`wall_rise`; one tile without the model when `bare` or unplaced) on the tile's height. With
/// `bare`, a tile without a wall is never above. The wall found (0: none) goes back with the answer.
pub fn tile_top_above<H: UuWorld + ?Sized>(h: &mut H, x: i32, y: i32, height: f32, bare: bool) -> (bool, u64) {
    let map = h.map();
    let tile = h.map_tile(map, x, y);
    let mut wall = 0;
    if tile == 0 {
        return (false, wall);
    }
    let mut top = h.tile_height(tile);
    for u in tile_units(h, tile) {
        if wall != 0 {
            break;
        }
        wall = u;
        if u == 0 {
            continue;
        }
        let v = view(h, u);
        if !v.class().is_wall() || h.passable_by(u, 0) {
            wall = 0;
        } else if !bare && v.on_map() {
            top += h.wall_rise(u);
        } else {
            top += 1.0;
        }
    }
    let above = (!bare || wall != 0) && top > height;
    (above, wall)
}

/// `EEIsLineBlocked` (0x24dd2e): whether a wall blocks the tile line (thin, without its ends) from the first tile at
/// its height, a diagonal step between two raised tiles included; the last free tile and the last wall asked
/// (None: never asked) go back.
pub fn line_blocked<H: UuWorld + ?Sized>(
    h: &mut H,
    x1: i32,
    y1: i32,
    x2: i32,
    y2: i32,
) -> (bool, Option<u64>, i32, i32) {
    let map = h.map();
    let from = h.map_tile(map, x1, y1);
    let to = h.map_tile(map, x2, y2);
    let mut last_y = 0;
    let mut last_x = 0;
    let mut blocked = false;
    let mut blocker = None;
    if from == 0 || to == 0 {
        return (blocked, blocker, last_x, last_y);
    }
    let line = h.tile_line(x1, y1, x2, y2, false, true);
    last_y = y1;
    let height = h.tile_height(from);
    let mut above = |h: &mut H, x: i32, y: i32| {
        let (a, w) = tile_top_above(h, x, y, height, true);
        blocker = Some(w);
        a
    };
    let mut cx = x1;
    let mut i = 0;
    loop {
        last_x = cx;
        if !(!line.is_empty() && i < line.len()) || blocked {
            break;
        }
        let [x, y] = line[i];
        let dx = x.wrapping_sub(cx);
        let dy = y.wrapping_sub(last_y);
        if dx.unsigned_abs() == 1 && dy.unsigned_abs() == 1 && above(h, x, last_y) && above(h, cx, y) {
            blocked = true;
        } else {
            blocked = above(h, x, y);
            last_y = y;
        }
        cx = x;
        i += 1;
    }
    if !blocked
        && x2.wrapping_sub(cx).unsigned_abs() == 1
        && y2.wrapping_sub(last_y).unsigned_abs() == 1
        && above(h, x2, last_y)
        && above(h, cx, y2)
    {
        blocked = true;
        last_x = x2;
    }
    (blocked, blocker, last_x, last_y)
}

// ---- the path length ----

/// `EEGetPathLengthTo` (0x24b8ac): the straight distance from the unit's tile to `tile` when a path search (bounded
/// by the two tiles grown by half the larger side, at least one, at most `0x32` steps) reaches it, `tile` then its
/// first path tile; else 10000 and `tile` (-1, -1).
pub fn path_length_to<H: UuWorld + ?Sized>(h: &mut H, unit: u64, tile: &mut Tile, limit: u32, walls: bool) -> f32 {
    let u = view(h, unit);
    let from = [u.tile_x(), u.tile_y()];
    let to = *tile;
    let mut left = from[0];
    let mut right = from[0];
    let mut top = from[1];
    let mut bottom = from[1];
    left = if left < to[0] { left } else { to[0] };
    right = if right > to[0] { right } else { to[0] };
    top = if top < to[1] { top } else { to[1] };
    bottom = if bottom > to[1] { bottom } else { to[1] };
    let width = right.wrapping_sub(left);
    let height = bottom.wrapping_sub(top);
    let larger = if width > height { width } else { height };
    let margin = if larger / 2 > 1 { larger / 2 } else { 1 };
    top = top.wrapping_sub(margin);
    if top <= 0 {
        top = 0;
    }
    left = left.wrapping_sub(margin);
    if left <= 0 {
        left = 0;
    }
    let edge = h.map_size().wrapping_sub(1);
    let b = margin.wrapping_add(bottom);
    bottom = if b < edge { b } else { edge };
    let r = margin.wrapping_add(right);
    right = if r < edge { r } else { edge };
    let limit = if limit > 0x32 { 0x32 } else { limit };
    match h.find_path(unit, from, to, [left, top, right, bottom], limit, walls) {
        Some(first) if first == to => {
            *tile = first;
            let dy = first[1].wrapping_sub(from[1]);
            let dx = first[0].wrapping_sub(from[0]);
            let (dx, dy) = (dx.wrapping_mul(dx), dy.wrapping_mul(dy));
            trunc_long(sqrt(dx as f32 + dy as f32)) as f32
        }
        _ => {
            *tile = [-1, -1];
            10000.0
        }
    }
}

// ---- the sound ----

/// `EEComputeSoundParameters` (0x2449ca): the volume, pan and pitch (always 1) of a sound at `location`, as the
/// camera hears it: silent off a tile the current player sees; panned outside the screen's middle half; faded off
/// screen, behind the camera and past the far distance (the port's: the far fade grows with a zoom past 100%).
pub fn sound_parameters(v: &EeUnitsUuSoundView, volume: f32, location: [f32; 3]) -> [f32; 3] {
    if v.seen == 0 {
        return [0.0, 0.0, 1.0];
    }
    let pan_scale = 1.0 / (1.0 - PAN_DEAD_ZONE);
    let edge_scale = 1.0 / EDGE_FALLOFF;
    let behind_scale = 1.0 / BEHIND_LIMIT;
    let height = v.y_clip;
    let mut scale = 1.0f32;
    let mut vertical = 1.0f32;
    let mut pan = 0.0f32;
    let screen = v.screen;
    let x = if v.depth < 0.0 { -screen[0] } else { screen[0] };
    if x < -PAN_DEAD_ZONE {
        pan = -((-x - PAN_DEAD_ZONE) * pan_scale);
    } else if x > PAN_DEAD_ZONE {
        pan = (x - PAN_DEAD_ZONE) * pan_scale;
    }
    if vec3(v.camera).distance_to_squared(vec3(location)) > 25.0 {
        if screen[0] < -1.0 {
            scale = 1.0 - (-screen[0] - 1.0) * edge_scale;
        } else if screen[0] > 1.0 {
            scale = 1.0 - (screen[0] - 1.0) * edge_scale;
        }
        if -height > screen[1] {
            vertical = 1.0 - (-screen[1] - height) * edge_scale;
        } else if v.y_clip < screen[1] {
            vertical = 1.0 - (screen[1] - height) * edge_scale;
        }
        if vertical < scale {
            scale = vertical;
        }
    }
    if v.depth < 0.0 {
        let behind = -v.depth;
        vertical = if behind > BEHIND_LIMIT { 0.0 } else { 1.0 - behind_scale * behind };
    } else {
        let reach = if v.zoom > 1.0 { v.zoom } else { 1.0 };
        if v.depth >= 27.0 * reach {
            vertical = (35.0 * reach - v.depth) * (0.125 / reach);
        }
    }
    if vertical < scale {
        scale = vertical;
    }
    let p = if pan < 1.0 { pan } else { 1.0 };
    let s = if scale < 1.0 { scale } else { 1.0 };
    [(if s > 0.0 { s } else { 0.0 }) * volume, if p > -1.0 { p } else { -1.0 }, 1.0]
}

/// `EEGetDopplerPitch` (0x242110): the pitch of a moving unit's sound against the camera (`camera`): the relativistic
/// Doppler factor of its speed (`velocity`, in its frame `location`) along the direction to the camera. None (the
/// pitch unchanged) when silent or still.
pub fn doppler_pitch(volume: f32, velocity: f32, location: &GeTransformation, camera: [f32; 3]) -> Option<f32> {
    if !(volume > 0.0) || !(velocity > 0.0) {
        return None;
    }
    let position = [location.m[0][3], location.m[1][3], location.m[2][3]];
    let mut to_camera = GeVector { x: camera[0] - position[0], y: camera[1] - position[1], z: camera[2] - position[2] };
    zero_or_normalize(&mut to_camera);
    let mut heading = location.apply_vector(GeVector { x: 0.0, y: velocity, z: 0.0 });
    zero_or_normalize(&mut heading);
    let cosine = heading.dot(to_camera);
    let ratio = velocity * 0.05;
    let gamma = sqrt(1.0 - velocity * velocity * 0.0025);
    Some(gamma / (1.0 - ratio * ((ratio + cosine) / (1.0 - ratio * cosine))))
}

// ---- the deposit ----

/// `EEDepositLoad` (0x245284): the unit's load into the container's player's stock (the variant's factor; stone,
/// gold and iron from a site near a town building, or food from a farm near a granary, gain its garrison's bonus,
/// with the effect), its statistic series, and the unit's load emptied.
pub fn deposit_load<H: UuWorld + ?Sized>(h: &mut H, unit: u64, container: u64) {
    let u = h.unit(unit);
    // SAFETY: the host's records of a live unit
    let kind = unsafe { (*u.unit).mResourceType as u32 };
    h.can_take(container, unit, kind);
    // SAFETY: as above, read after the C++'s CanTake
    let carried = unsafe { (*u.unit).mResourceAmount as i32 };
    let mut amount = lrintf(h.deposit_factor() * carried as f32);
    let mut near = false;
    let mut bonus = 0.0f32;
    let c = h.unit(container);
    // SAFETY: the container's complex record (a container is a complex unit)
    let level = if c.cx.is_null() { 0 } else { unsafe { (*c.cx).mGarrisonCount as i32 } };
    let class = |h: &mut H, unit_type: i32| {
        let c = h.class_of_type(unit_type);
        UuClassView(c.fields, c.obj)
    };
    // the garrison's first and second levels: max(0, min(level, first)), min(level - first, second - first) unsigned
    let levels = |first: i32, second: i32| {
        let low = if level < first { level } else { first };
        let low = if 0 > low { 0 } else { low };
        let rest = (level.wrapping_sub(low)) as u32;
        let room = second.wrapping_sub(low) as u32;
        (low, if rest < room { rest } else { room })
    };
    let last = if kind == UU_RESOURCE_GOLD
        || kind == UU_RESOURCE_IRON
        || kind == UU_RESOURCE_STONE
        || kind == UU_RESOURCE_FOOD
    {
        h.last_resource(unit)
    } else {
        0
    };
    if (kind == UU_RESOURCE_GOLD || kind == UU_RESOURCE_IRON || kind == UU_RESOURCE_STONE)
        && last != 0
        && h.distance_squared_to_unit(container, last) <= 20.0
    {
        let settlement = class(h, UU_TYPE_SETTLEMENT);
        let town_center = class(h, UU_TYPE_TOWN_CENTER);
        let capitol = class(h, UU_TYPE_CAPITOL);
        let (first, second) = levels(settlement.garrison_capacity(), town_center.garrison_capacity());
        let third = (level as u32).wrapping_sub(second).wrapping_sub(first as u32);
        bonus = third as f32 * capitol.garrison_bonus()
            + town_center.garrison_bonus() * (second as i32) as f32
            + settlement.garrison_bonus() * first as f32;
        near = true;
    } else if kind == UU_RESOURCE_FOOD
        && last != 0
        && view(h, last).class().unit_type() == UU_TYPE_FARM
        && {
            let t = UuView(c).class().unit_type() as i32;
            t == UU_TYPE_GRANARY_IMPROVED || t == UU_TYPE_GRANARY
        }
        && h.distance_squared_to_unit(container, last) <= 20.0
    {
        let granary = class(h, UU_TYPE_GRANARY);
        let improved = class(h, UU_TYPE_GRANARY_IMPROVED);
        let (first, second) = levels(granary.garrison_capacity(), improved.garrison_capacity());
        bonus = improved.garrison_bonus() * (second as i32) as f32 + granary.garrison_bonus() * first as f32;
        near = true;
    }
    if near && bonus > 0.0 {
        amount = lrintf((bonus + 1.0) * amount as f32);
        h.deposit_effect(unit);
    }
    h.add_resource(container, unit, amount, carried, near, bonus);
    let series = match kind {
        UU_RESOURCE_FOOD => 7,
        UU_RESOURCE_WOOD => 8,
        UU_RESOURCE_STONE => 10,
        UU_RESOURCE_GOLD => 9,
        UU_RESOURCE_IRON => 0xb,
        _ => 0,
    };
    if series != 0 {
        h.statistic(container, series, amount);
    }
    // SAFETY: the unit's own record (the C++'s `if (mResourceAmount != 0) mResourceAmount = 0`)
    unsafe {
        let amount = std::ptr::addr_of!((*u.unit).mResourceAmount) as *mut _;
        if *amount != 0 {
            *amount = 0;
        }
    }
}
