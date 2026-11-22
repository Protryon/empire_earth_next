//! The sea and ambient classes' bodies over their C++ statements (the units' lane, round 9; were
//! EEUnitClassGround.cpp): `AmbientLocationToTile`, `SubmergedNavalGroundLocation`,
//! `SubmergedNavalLocationToTile`, `SubmergedNavalCreateUnitAt`, `SubmergedNavalCreateUnit`,
//! `SubmergedNavalAddToTile`, `SubmergedNavalHasArrived`, `SeaAnimalAdjustPath` and `EEComplexUnit::UpdateWheels`.
//! Each C++ statement one `EE_UNITS_UCG_*` case of `ee_units_ucg_host` (the class host's, `EE_UNITS_UC_*`, reach
//! EEUnitClass.cpp's through it), in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use crate::units::ucg_ground::*;
use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DLine, Ge3DPoint, GeTransformation, GeVector};
use ee_base::math::um::floor;
use std::os::raw::c_long;

// ---- the statements (3000..3049)
/// `memcpy(a, &::gWorld.mLine, sizeof(GE3DLine))` (its start, then its direction)
pub const EE_UNITS_UCG_WORLD_LINE: i32 = 3000;
/// `f[0] = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(f[0], f[1])`
pub const EE_UNITS_UCG_MESH_Z: i32 = 3001;
/// `::gWorld.mMap->GetGroundPoint(f[0..3], f[0..3])`
pub const EE_UNITS_UCG_GROUND_POINT: i32 = 3002;
/// `f[0] = ::gWorld.mMap->mTerrainMesh.GetZ(b, c)`
pub const EE_UNITS_UCG_TERRAIN_Z: i32 = 3003;
/// `class->BaseCreateUnitAt(the transformation at a, b >> 1, b & 1, unit c)`
pub const EE_UNITS_UCG_BASE_CREATE_AT: i32 = 3004;
/// complex unit `a`'s `SetLocation(f[0..3], true)`
pub const EE_UNITS_UCG_SET_LOCATION_OF: i32 = 3005;
/// `f[0] = gPhysics->mRandom.Generate()`
pub const EE_UNITS_UCG_RANDOM_GENERATE: i32 = 3006;
/// `f[0] = gPhysics->GetRandom(f[0], f[1])`
pub const EE_UNITS_UCG_RANDOM_RANGE: i32 = 3007;
/// `unit->mNumWheels`
pub const EE_UNITS_UCG_WHEEL_COUNT: i32 = 3008;
/// `unit->mWheels[b]->Update(unit)`
pub const EE_UNITS_UCG_WHEEL_UPDATE: i32 = 3009;

// ---- the bodies (`ee_units_ucg_sea`'s `what`)
pub const EE_UNITS_UCG_DO_AMBIENT_LOCATION_TO_TILE: i32 = 3020;
pub const EE_UNITS_UCG_DO_SUBMERGED_GROUND_LOCATION: i32 = 3021;
pub const EE_UNITS_UCG_DO_SUBMERGED_LOCATION_TO_TILE: i32 = 3022;
pub const EE_UNITS_UCG_DO_SUBMERGED_CREATE_UNIT_AT: i32 = 3023;
pub const EE_UNITS_UCG_DO_SUBMERGED_CREATE_UNIT: i32 = 3024;
pub const EE_UNITS_UCG_DO_SUBMERGED_ADD_TO_TILE: i32 = 3025;
pub const EE_UNITS_UCG_DO_SUBMERGED_HAS_ARRIVED: i32 = 3026;
pub const EE_UNITS_UCG_DO_UPDATE_WHEELS: i32 = 3027;
pub const EE_UNITS_UCG_DO_NAVAL_CITIZEN_LOCATION_TO_TILE: i32 = 3028;
pub const EE_UNITS_UCG_DO_SPACE_BUILDING_LOCATION_TO_TILE: i32 = 3029;
pub const EE_UNITS_UCG_DO_NAVAL_CITIZEN_CREATE_UNIT: i32 = 3030;
pub const EE_UNITS_UCG_DO_NAVAL_PRIEST_CREATE_UNIT_AT: i32 = 3031;
pub const EE_UNITS_UCG_DO_BUILDING_GROUND_LOCATION: i32 = 3032;

const UNIT_TYPE_AMB_CATTAILS_1: i64 = 0x234;
const UNIT_TYPE_RES_DOLPHIN: i64 = 0x295;
const UNIT_TYPE_DOCK: i64 = 0x24;
const UNIT_TYPE_NAVY_YARD: i64 = 0x41;
/// `kBuildingLift` (a building sits just above the ground)
const BUILDING_LIFT: f32 = 0.001;
const GOAL_LOCATION: i64 = 1;
/// `kSeaAnimalBottomClearance`
const SEA_ANIMAL_BOTTOM_CLEARANCE: f32 = 0.09;
/// `kSeaAnimalMaximumStep`
const SEA_ANIMAL_MAXIMUM_STEP: f32 = 0.48;
/// `kSeaAnimalMinimumDepth`
const SEA_ANIMAL_MINIMUM_DEPTH: f32 = -0.6;
/// `kSeaAnimalMaximumDepthFactor`
const SEA_ANIMAL_MAXIMUM_DEPTH_FACTOR: f32 = 0.81;

/// The unit utilities' Rust these bodies call (as the C++ called it).
pub trait UcgSeaRust {
    /// `ee_units_uu_seabed(0, unit, at)` (its UUTL note is the original call's)
    fn seabed(&mut self, unit: u64, at: [f32; 3]) -> f32;
}

/// `min(a, b)` (windows.h's: `a < b ? a : b`)
fn win_min(a: f32, b: f32) -> f32 {
    if a < b {
        a
    } else {
        b
    }
}

fn world_line<H: UcHost + ?Sized>(s: &mut S<'_, H>) -> Ge3DLine {
    let mut words = [0.0f32; 6];
    s.a(EE_UNITS_UCG_WORLD_LINE, addr(&mut words));
    Ge3DLine {
        start: Ge3DPoint { x: words[0], y: words[1], z: words[2] },
        direction: GeVector { x: words[3], y: words[4], z: words[5] },
    }
}

/// `theLine.SolveForZ(z, t, point)`: the point where it meets `z`, `point` unchanged when it runs parallel
fn solve_z(line: &Ge3DLine, z: f32, point: &mut Ge3DPoint) {
    if let Some((_, p)) = line.solve_for_z_point(z) {
        *point = p;
    }
}

/// The view line through `location` (`gWorld.mLine` started there)
fn line_from<H: UcHost + ?Sized>(s: &mut S<'_, H>, location: [f32; 3]) -> Ge3DLine {
    let mut line = world_line(s);
    line.start = Ge3DPoint { x: location[0], y: location[1], z: location[2] };
    line
}

/// The out tile of a `LocationToTile` body: its `long& outX`, `long& outY`
#[derive(Clone, Copy)]
pub struct OutTile {
    pub x: *mut c_long,
    pub y: *mut c_long,
}

/// # Safety
/// `out`'s two pointers are writable `long`s
unsafe fn write_tile(out: OutTile, point: &Ge3DPoint) {
    // SAFETY: the caller's contract
    unsafe {
        *out.x = floor(point.x) as c_long;
        *out.y = floor(point.y) as c_long;
    }
}

/// `AmbientLocationToTile(location, x, y)`: for cattails, the tile where the view line through `location` meets the
/// water plane; nothing for the others.
///
/// # Safety
/// `out`'s two pointers are writable `long`s
pub unsafe fn ambient_location_to_tile<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    location: [f32; 3],
    out: OutTile,
) {
    let mut s = S { h, class, unit: 0 };
    let mut point = Ge3DPoint::default();
    if s.s(EE_UNITS_UC_THIS_UNIT_TYPE) != UNIT_TYPE_AMB_CATTAILS_1 {
        return;
    }
    let line = line_from(&mut s, location);
    solve_z(&line, 0.0, &mut point);
    // SAFETY: the caller's contract
    unsafe { write_tile(out, &point) };
}

/// `NavalCitizenLocationToTile(location, x, y)` (`z` 0, the water plane) / `SpaceBuildingLocationToTile` (`z` the
/// class's cruise altitude): the tile where the view line through `location` meets that height.
///
/// # Safety
/// `out`'s two pointers are writable `long`s
pub unsafe fn location_to_tile_at<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    location: [f32; 3],
    out: OutTile,
    cruise: bool,
) {
    let mut s = S { h, class, unit: 0 };
    let mut point = Ge3DPoint::default();
    let line = line_from(&mut s, location);
    let z = if cruise { s.float(EE_UNITS_UCG_CRUISE_ALTITUDE_OF, class, 0) } else { 0.0 };
    solve_z(&line, z, &mut point);
    // SAFETY: the caller's contract
    unsafe { write_tile(out, &point) };
}

/// `NavalCitizenCreateUnit(x, y, player, foundation, creator)`: the unit at tile (`x`, `y`) on the water plane.
pub fn naval_citizen_create_unit<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    x: c_long,
    y: c_long,
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = S { h, class, unit: 0 };
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = x as f32;
    t.m[1][3] = y as f32;
    t.m[2][3] = 0.0;
    create_at(&mut s, &mut t, player, foundation, creator)
}

/// `NavalPriestCreateUnitAt(t, player, foundation, creator)`: the unit at `t` on the water plane.
pub fn naval_priest_create_unit_at<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    t: &[[f32; 4]; 4],
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = S { h, class, unit: 0 };
    let mut t = GeTransformation { m: *t };
    t.m[2][3] = 0.0;
    create_at(&mut s, &mut t, player, foundation, creator)
}

/// `BuildingGroundLocation(location)`: docks and navy yards on the water plane, the others just above the ground.
pub fn building_ground_location<H: UcHost + ?Sized>(h: &mut H, class: u64, location: &mut [f32; 3]) {
    let mut s = S { h, class, unit: 0 };
    let unit_type = s.s(EE_UNITS_UC_THIS_UNIT_TYPE);
    if unit_type == UNIT_TYPE_DOCK || unit_type == UNIT_TYPE_NAVY_YARD {
        location[2] = 0.0;
    } else {
        location[2] += BUILDING_LIFT;
    }
}

/// `SubmergedNavalGroundLocation(location)`: a submarine never goes below the sea floor.
pub fn submerged_ground_location<H: UcHost + ?Sized>(h: &mut H, class: u64, location: &mut [f32; 3]) {
    let mut s = S { h, class, unit: 0 };
    let mut f = [location[0], location[1], 0.0, 0.0];
    s.f(EE_UNITS_UCG_MESH_Z, 0, 0, 0, &mut f);
    if f[0] >= location[2] {
        location[2] = f[0];
    }
}

/// `SubmergedNavalLocationToTile(location, x, y)`: the tile where the view line through `location` meets the cruise
/// depth, or the sea floor above it.
///
/// # Safety
/// `out`'s two pointers are writable `long`s
pub unsafe fn submerged_location_to_tile<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    location: [f32; 3],
    out: OutTile,
) {
    let mut s = S { h, class, unit: 0 };
    let mut point = Ge3DPoint::default();
    let z = -s.float(EE_UNITS_UCG_CRUISE_ALTITUDE_OF, class, 0);
    let line = line_from(&mut s, location);
    solve_z(&line, z, &mut point);
    let mut ground = [point.x, point.y, point.z];
    s.point(EE_UNITS_UCG_GROUND_POINT, 0, 0, 0, &mut ground);
    if ground[2] > z {
        solve_z(&line, ground[2], &mut point);
    }
    // SAFETY: the caller's contract
    unsafe { write_tile(out, &point) };
}

/// The unit's seabed placement after a creation: `unit->mComplexUnit->SetLocation(position at the seabed's depth)`
fn to_seabed<H: UcHost + UcgSeaRust + ?Sized>(h: &mut H, class: u64, unit: u64, mut position: [f32; 3]) {
    let complex = S { h: &mut *h, class, unit: 0 }.a(EE_UNITS_UC_COMPLEX_OF, unit) as u64;
    position[2] = h.seabed(complex, position);
    S { h, class, unit: 0 }.point(EE_UNITS_UCG_SET_LOCATION_OF, complex, 0, 0, &mut position);
}

fn create_at<H: UcHost + ?Sized>(s: &mut S<'_, H>, t: &mut GeTransformation, player: i64, foundation: bool, creator: u64) -> u64 {
    s.abc(EE_UNITS_UCG_BASE_CREATE_AT, addr(&mut t.m), player.wrapping_mul(2) | i64::from(foundation), creator as i64)
        as u64
}

/// `SubmergedNavalCreateUnitAt(t, player, foundation, creator)`: the unit at the grounded point of `t`, then on the
/// seabed.
pub fn submerged_create_unit_at<H: UcHost + UcgSeaRust + ?Sized>(
    h: &mut H,
    class: u64,
    t: &[[f32; 4]; 4],
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = S { h: &mut *h, class, unit: 0 };
    let mut t = GeTransformation { m: *t };
    let mut position = [t.m[0][3], t.m[1][3], t.m[2][3]];
    s.point(EE_UNITS_UC_GROUND_LOCATION, 0, 0, 0, &mut position);
    t.m[0][3] = position[0];
    t.m[1][3] = position[1];
    t.m[2][3] = position[2];
    let unit = create_at(&mut s, &mut t, player, foundation, creator);
    to_seabed(h, class, unit, position);
    unit
}

/// `SubmergedNavalCreateUnit(x, y, player, foundation, creator)`: the unit at tile (`x`, `y`)'s grounded height,
/// yaw pi, then on the seabed.
#[allow(clippy::too_many_arguments)]
pub fn submerged_create_unit<H: UcHost + UcgSeaRust + ?Sized>(
    h: &mut H,
    class: u64,
    x: c_long,
    y: c_long,
    player: i64,
    foundation: bool,
    creator: u64,
) -> u64 {
    let mut s = S { h: &mut *h, class, unit: 0 };
    let mut f = [0.0f32; 4];
    s.f(EE_UNITS_UCG_TERRAIN_Z, 0, i64::from(x), i64::from(y), &mut f);
    let mut position = [x as f32, y as f32, f[0]];
    s.point(EE_UNITS_UC_GROUND_LOCATION, 0, 0, 0, &mut position);
    let mut t = GeTransformation::IDENTITY;
    t.m[0][3] = position[0];
    t.m[1][3] = position[1];
    t.m[2][3] = position[2];
    t.set_orientation(RotationOrder::Ypr, 2.0 * 1.570_796_4, 0.0, 0.0);
    t.m[3] = [0.0, 0.0, 0.0, 1.0];
    let unit = create_at(&mut s, &mut t, player, foundation, creator);
    to_seabed(h, class, unit, position);
    unit
}

/// `SubmergedNavalAddToTile(unit)`: the unit's complex unit on its map tile.
pub fn submerged_add_to_tile<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit: 0 };
    let complex = s.a(EE_UNITS_UC_COMPLEX_OF, unit);
    let x = s.a(EE_UNITS_UCG_TILE_X_OF, unit) as c_long;
    let y = s.a(EE_UNITS_UCG_TILE_Y_OF, unit) as c_long;
    let tile = s.abc(EE_UNITS_UC_GET_TILE, 0, i64::from(x), i64::from(y)) as u64;
    if tile != 0 {
        s.ab(EE_UNITS_UCG_TILE_ADD, tile, complex);
    }
}

/// `SubmergedNavalHasArrived(unit, value)`: whether the submarine may stop (sinking, at the path's end tile with
/// its speed reached, or slow enough).
// the original's `!(mCollisionRadius < inValue)` (a NaN arrives)
#[allow(clippy::neg_cmp_op_on_partial_ord)]
pub fn submerged_has_arrived<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, value: f32) -> bool {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UCG_COLLISION_STATE) == 8 {
        return true;
    }
    let count = s.s(EE_UNITS_UCG_WAYPOINT_COUNT) as u32;
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UCG_NEXT_WAYPOINT, 0, 0, 0, &mut position);
    if s.s(EE_UNITS_UC_PRODUCER_TILE_X) as c_long == floor(position[0]) as c_long
        && s.s(EE_UNITS_UC_PRODUCER_TILE_Y) as c_long == floor(position[1]) as c_long
        && (count > 1
            || (s.b(EE_UNITS_UC_QUEUE_LEN, 0) as u32 > 1 && {
                let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
                goal != 0 && s.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_LOCATION
            }))
    {
        return !(s.float(EE_UNITS_UCG_COLLISION_RADIUS, 0, 0) < value);
    }
    value <= 0.05
}

/// `SeaAnimalAdjustPath(unit, path)`: each point's depth, last first: a random depth under the surface over the
/// sea floor, stepped from the last one (a dolphin dives now and then and steps further).
pub fn sea_animal_adjust_path<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, path: &mut [[f32; 3]]) {
    let mut s = S { h, class, unit };
    let mut t = GeTransformation::IDENTITY;
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
    let mut depth = t.m[2][3];
    let mut surfaced = true;
    for i in (0..path.len()).rev() {
        let mut f = [path[i][0], path[i][1], 0.0, 0.0];
        s.f(EE_UNITS_UCG_MESH_Z, 0, 0, 0, &mut f);
        let ground = f[0];
        let mut dive = s.s(EE_UNITS_UC_THIS_UNIT_TYPE) == UNIT_TYPE_RES_DOLPHIN;
        if dive {
            dive = i != 0;
            if dive {
                dive = !surfaced;
                if dive {
                    dive = s.float(EE_UNITS_UCG_RANDOM_GENERATE, 0, 0) < 0.35;
                }
            }
        }
        let target = if !dive {
            let mut f = [0.1, SEA_ANIMAL_MAXIMUM_DEPTH_FACTOR, 0.0, 0.0];
            s.f(EE_UNITS_UCG_RANDOM_RANGE, 0, 0, 0, &mut f);
            win_min(SEA_ANIMAL_MINIMUM_DEPTH, f[0] * ground)
        } else {
            let mut f = [0.45, 0.85, 0.0, 0.0];
            s.f(EE_UNITS_UCG_RANDOM_RANGE, 0, 0, 0, &mut f);
            surfaced = true;
            f[0] * ground
        };
        if surfaced {
            surfaced = false;
        }
        let dolphin = s.s(EE_UNITS_UC_THIS_UNIT_TYPE) == UNIT_TYPE_RES_DOLPHIN;
        let mut f = if dolphin { [0.2, 1.2, 0.0, 0.0] } else { [0.2, SEA_ANIMAL_MAXIMUM_STEP, 0.0, 0.0] };
        s.f(EE_UNITS_UCG_RANDOM_RANGE, 0, 0, 0, &mut f);
        let step = f[0];
        if depth < target {
            depth += step;
        } else {
            depth -= step;
        }
        let limit = if dolphin { -0.1 } else { SEA_ANIMAL_MINIMUM_DEPTH };
        depth = win_min(limit, depth);
        if depth <= ground + SEA_ANIMAL_BOTTOM_CLEARANCE {
            depth = ground + SEA_ANIMAL_BOTTOM_CLEARANCE;
        }
        path[i][2] = depth;
    }
}

/// `EEComplexUnit::UpdateWheels()`: each wheel's update.
pub fn update_wheels<H: UcHost + ?Sized>(h: &mut H, unit: u64) {
    let mut s = S { h, class: 0, unit };
    let count = s.s(EE_UNITS_UCG_WHEEL_COUNT) as u32;
    for i in 0..count {
        s.b(EE_UNITS_UCG_WHEEL_UPDATE, i64::from(i));
    }
}

fn out_tile(a: u64, b: i64) -> OutTile {
    OutTile { x: a as usize as *mut c_long, y: b as usize as *mut c_long }
}

/// Runs body `what` (`EE_UNITS_UCG_DO_*`) of `class` on `unit`: `a` a simple unit (or the creator), `b` and `c`
/// values (a tile, the player and foundation; or `a`, `b` the out tile's `long`s), `f` a point or a transformation;
/// the answer, or 0.
///
/// # Safety
/// `f` is what the body reads and writes (a writable `[f32; 3]` or `[[f32; 4]; 4]`), `a` and `b` the out tile's
/// writable `long`s for the `LocationToTile` bodies.
#[allow(clippy::too_many_arguments)]
pub unsafe fn run<H: UcHost + UcgSeaRust + ?Sized>(
    h: &mut H,
    what: i32,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    f: *mut f32,
) -> i64 {
    // SAFETY: the caller's contract for each body
    unsafe {
        match what {
            EE_UNITS_UCG_DO_AMBIENT_LOCATION_TO_TILE => {
                ambient_location_to_tile(h, class, *f.cast::<[f32; 3]>(), out_tile(a, b))
            }
            EE_UNITS_UCG_DO_SUBMERGED_GROUND_LOCATION => submerged_ground_location(h, class, &mut *f.cast::<[f32; 3]>()),
            EE_UNITS_UCG_DO_SUBMERGED_LOCATION_TO_TILE => {
                submerged_location_to_tile(h, class, *f.cast::<[f32; 3]>(), out_tile(a, b))
            }
            EE_UNITS_UCG_DO_SUBMERGED_CREATE_UNIT_AT => {
                return submerged_create_unit_at(h, class, &*f.cast::<[[f32; 4]; 4]>(), b >> 1, b & 1 != 0, a) as i64
            }
            EE_UNITS_UCG_DO_SUBMERGED_CREATE_UNIT => {
                return submerged_create_unit(h, class, b as i32 as c_long, (b >> 32) as i32 as c_long, c >> 1, c & 1 != 0, a)
                    as i64
            }
            EE_UNITS_UCG_DO_SUBMERGED_ADD_TO_TILE => submerged_add_to_tile(h, class, a),
            EE_UNITS_UCG_DO_SUBMERGED_HAS_ARRIVED => return i64::from(submerged_has_arrived(h, class, unit, *f)),
            EE_UNITS_UCG_DO_UPDATE_WHEELS => update_wheels(h, unit),
            EE_UNITS_UCG_DO_NAVAL_CITIZEN_LOCATION_TO_TILE => {
                location_to_tile_at(h, class, *f.cast::<[f32; 3]>(), out_tile(a, b), false)
            }
            EE_UNITS_UCG_DO_SPACE_BUILDING_LOCATION_TO_TILE => {
                location_to_tile_at(h, class, *f.cast::<[f32; 3]>(), out_tile(a, b), true)
            }
            EE_UNITS_UCG_DO_NAVAL_CITIZEN_CREATE_UNIT => {
                return naval_citizen_create_unit(h, class, b as i32 as c_long, (b >> 32) as i32 as c_long, c >> 1, c & 1 != 0, a)
                    as i64
            }
            EE_UNITS_UCG_DO_NAVAL_PRIEST_CREATE_UNIT_AT => {
                return naval_priest_create_unit_at(h, class, &*f.cast::<[[f32; 4]; 4]>(), b >> 1, b & 1 != 0, a) as i64
            }
            EE_UNITS_UCG_DO_BUILDING_GROUND_LOCATION => building_ground_location(h, class, &mut *f.cast::<[f32; 3]>()),
            _ => {}
        }
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    /// The ground classes' host (`ee_units_ucg_host`, through ucg_ground.rs's; the class host's statements pass
    /// through it).
    struct GameSea;

    impl UcHost for GameSea {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            crate::units::ucg_ground::GameG.uc(op, class, unit, a, b, c, f)
        }
    }

    impl UcgSeaRust for GameSea {
        fn seabed(&mut self, unit: u64, at: [f32; 3]) -> f32 {
            // SAFETY: a live unit and its point
            unsafe { crate::units::uu_ffi::ee_units_uu_seabed(0, unit, &at) }
        }
    }

    /// The sea classes' body `what` (`EE_UNITS_UCG_DO_*` from 3020) of `class` on `unit` (see [`run`]).
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit` (or NULL), `a` a live `EEUnit` (or 0) or the
    /// body's out tile, `f` the body's point or transformation, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucg_sea(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        f: *mut f32,
    ) -> i64 {
        // SAFETY: the caller's contract
        unsafe { run(&mut GameSea, what, class as usize as u64, unit as usize as u64, a, b, c, f) }
    }

    /// `SeaAnimalAdjustPath(unit, path)` of `class` (the waypoints' class `AdjustPath` body 4).
    ///
    /// # Safety
    /// `class` a live `EEUnitClass`, `unit` a live `EEComplexUnit`, on the simulation thread.
    pub unsafe fn sea_animal_path(class: u64, unit: u64, path: &mut [[f32; 3]]) {
        sea_animal_adjust_path(&mut GameSea, class, unit, path);
    }
}

#[cfg(not(test))]
pub use game::sea_animal_path;

#[cfg(test)]
#[path = "ucg_sea_tests.rs"]
mod tests;
