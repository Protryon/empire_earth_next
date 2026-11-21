//! The placement ghosts' creation (the units' lane, round 7; were EEGhostUnit.cpp): `EEGhostUnit::Create` (a unit's
//! ghost, an airport's runways, a granary's farm ghosts), `CreateGate` (a gate's three parts) and `EEBuildWallGhosts`
//! (one ghost per planned wall tile). Every C++ statement is one operation of `ee_units_ghost_host` (after
//! ghost_body.rs's), asked in the original's order. Only a drawn game reaches these (the placement cursor).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::units::ghost_body::*;

// ---- the statements (`op`, `ghost` the ghost acted on; the answer)
/// `gUnitClassTable[b]`
pub const EE_UNITS_GHOST_CLASS_AT: i32 = 100;
/// `gWorld.ClearGhostUnits()`
pub const EE_UNITS_GHOST_CLEAR_GHOSTS: i32 = 101;
/// `new EEGhostUnit(class a's GetUnitType(), (EEPlayer*)b, c != 0)`
pub const EE_UNITS_GHOST_NEW: i32 = 102;
/// class `a`'s `mMaxAngularVelocity == 0.0f`
pub const EE_UNITS_GHOST_CLASS_FIXED: i32 = 103;
/// `gWorld.AddGhostUnit(ghost)`
pub const EE_UNITS_GHOST_ADD: i32 = 104;
/// class `a`'s `GetUnitType()`
pub const EE_UNITS_GHOST_CLASS_TYPE: i32 = 105;
/// `EEGetFarmType((EEPlayer*)a)`
pub const EE_UNITS_GHOST_FARM_TYPE: i32 = 106;
/// class `a`'s `GetTileSize()`
pub const EE_UNITS_GHOST_CLASS_TILE_SIZE: i32 = 107;
/// `EEGetWallPiecePart(b, &the scratch flag)`
pub const EE_UNITS_GHOST_WALL_PIECE_PART: i32 = 108;
/// the scratch flag
pub const EE_UNITS_GHOST_PIECE_FLAG: i32 = 109;
/// `SetAnimation(b)`
pub const EE_UNITS_GHOST_SET_ANIMATION: i32 = 110;
/// class `a`'s `mCategory`
pub const EE_UNITS_GHOST_CLASS_CATEGORY: i32 = 111;
/// `mUnitSet = class a's mOverlapUnitSet`
pub const EE_UNITS_GHOST_SET_OVERLAP_SET: i32 = 112;
/// `EEUnitClassOfKind<EE_CLASS_KIND_GATE>((EEUnitClass*)a)`
pub const EE_UNITS_GHOST_GATE_CLASS: i32 = 113;
/// `gUnitClassTable[player a's mTechnologies->Get(b)->mData->mUnitClass]`
pub const EE_UNITS_GHOST_TECH_CLASS: i32 = 114;
/// the planned tiles' (the `std::vector<EETileCoordinate>` at a) `size()`
pub const EE_UNITS_GHOST_TILES_SIZE: i32 = 115;
/// the planned tiles' (at a) `[b]`'s x, y (c 0, 1)
pub const EE_UNITS_GHOST_TILES_AT: i32 = 116;
/// `gWorld.GetMap()->GetNumRaisedCorners(b, c)`
pub const EE_UNITS_GHOST_RAISED_CORNERS: i32 = 117;
/// the corners' (the `std::vector<long>` at a) `[b]`
pub const EE_UNITS_GHOST_CORNER_AT: i32 = 118;
/// `EEOrientWallGhost(ghost, b, c)`
pub const EE_UNITS_GHOST_ORIENT_WALL: i32 = 119;
/// `mTransformation.SetX(the float of the bits b)`
pub const EE_UNITS_GHOST_SET_X: i32 = 120;
/// `mTransformation.SetY(the float of the bits b)`
pub const EE_UNITS_GHOST_SET_Y: i32 = 121;
/// `mTransformation.SetZ(the float of the bits b)`
pub const EE_UNITS_GHOST_SET_Z: i32 = 122;

/// `kUnitTypeAirport`, `kUnitTypeAirportRunway`, the granaries, `kUnitTypeRoboticFarm`
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_AIRPORT_RUNWAY: i64 = 0xf0;
const UNIT_TYPE_GRANARY: i64 = 0x34;
const UNIT_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
const UNIT_TYPE_ROBOTIC_FARM: i64 = 0x33a;
/// `kUnitCategoryMiningBuilding`
const CATEGORY_MINING_BUILDING: i64 = 0x18;
/// .rdata 0x446dc0: the four facing yaws
const WALL_ROTATIONS: [f32; 4] = [0.0, std::f32::consts::PI, -std::f32::consts::FRAC_PI_2, std::f32::consts::FRAC_PI_2];
/// the gate's direction for the creation and its three parts
const GATE_DIRECTION: i64 = 2;
const GATE_PARTS: [i64; 3] = [0, 2, 4];
/// the robotic farm piece's corner animation
const ANIMATION_CORNER: i64 = 0xb;

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

/// A new ghost of a class for a player, turned to `rotation` unless the class never turns.
fn new_ghost<H: GhostHost + ?Sized>(h: &mut H, class: u64, player: u64, no_media: bool, rotation: f32) -> u64 {
    let g = h.ghost(EE_UNITS_GHOST_NEW, 0, class, player as i64, i64::from(no_media)) as u64;
    let fixed = h.ghost(EE_UNITS_GHOST_CLASS_FIXED, 0, class, 0, 0) != 0;
    h.ghost(EE_UNITS_GHOST_SET_YAW, g, 0, bits(if fixed { 0.0 } else { rotation }), 0);
    g
}

/// `EEGhostUnit::Create(location, type, player, rotation, is_move)`: the placement ghosts replaced by a unit's
/// ghost, plus 3 runway ghosts for an airport and the farm ghosts around a granary unless it is being moved.
pub fn create<H: GhostHost + ?Sized>(h: &mut H, kind: i64, player: u64, rotation: f32, is_move: bool) {
    let class = h.ghost(EE_UNITS_GHOST_CLASS_AT, 0, 0, kind, 0) as u64;
    let wall_class = h.ghost(EE_UNITS_GHOST_CLASS_AT, 0, 0, UNIT_TYPE_AIRPORT_RUNWAY, 0) as u64;
    h.ghost(EE_UNITS_GHOST_LOCK_ENTER, 0, 0, 0, 0);
    h.ghost(EE_UNITS_GHOST_CLEAR_GHOSTS, 0, 0, 0, 0);
    let mut g = new_ghost(h, class, player, false, rotation);
    h.ghost(EE_UNITS_GHOST_ADD, g, 0, 0, 0);
    if kind == UNIT_TYPE_AIRPORT {
        for _ in 0..3 {
            g = new_ghost(h, wall_class, player, false, rotation);
            h.ghost(EE_UNITS_GHOST_ADD, g, 0, 0, 0);
        }
    }
    let class_type = h.ghost(EE_UNITS_GHOST_CLASS_TYPE, 0, class, 0, 0);
    if (class_type == UNIT_TYPE_GRANARY_IMPROVED || class_type == UNIT_TYPE_GRANARY) && !is_move {
        let farm = h.ghost(EE_UNITS_GHOST_FARM_TYPE, 0, player, 0, 0);
        let piece = h.ghost(EE_UNITS_GHOST_CLASS_AT, 0, 0, farm, 0) as u64;
        let size = h.ghost(EE_UNITS_GHOST_CLASS_TILE_SIZE, 0, class, 0, 0) as c_long;
        let side = size.max(3);
        let count = side * side - 1;
        for i in 0..count {
            g = new_ghost(h, piece, player, false, rotation);
            if h.ghost(EE_UNITS_GHOST_CLASS_TYPE, 0, piece, 0, 0) == UNIT_TYPE_ROBOTIC_FARM {
                let index = if i >= 4 { i + 1 } else { i };
                let part = h.ghost(EE_UNITS_GHOST_WALL_PIECE_PART, 0, 0, i64::from(index), 0);
                if h.ghost(EE_UNITS_GHOST_PIECE_FLAG, 0, 0, 0, 0) != 0 {
                    h.ghost(EE_UNITS_GHOST_SET_ANIMATION, g, 0, ANIMATION_CORNER, 0);
                }
                h.ghost(EE_UNITS_GHOST_SET_YAW, g, 0, bits(WALL_ROTATIONS[part as usize]), 0);
            }
            h.ghost(EE_UNITS_GHOST_ADD, g, 0, 0, 0);
        }
    }
    if h.ghost(EE_UNITS_GHOST_CLASS_CATEGORY, 0, class, 0, 0) == CATEGORY_MINING_BUILDING {
        h.ghost(EE_UNITS_GHOST_SET_OVERLAP_SET, g, class, 0, 0);
    }
    h.ghost(EE_UNITS_GHOST_SET_CHANGED, 0, 0, 0, 0);
    h.ghost(EE_UNITS_GHOST_LOCK_LEAVE, 0, 0, 0, 0);
}

/// `EEGhostUnit::CreateGate(rotation, class, player)`: the placement ghosts replaced by a gate's three parts (left,
/// middle, right), each turned as its part.
pub fn create_gate<H: GhostHost + ?Sized>(h: &mut H, rotation: f32, class: u64, player: u64) {
    let class = h.ghost(EE_UNITS_GHOST_GATE_CLASS, 0, class, 0, 0) as u64;
    h.ghost(EE_UNITS_GHOST_LOCK_ENTER, 0, 0, 0, 0);
    h.ghost(EE_UNITS_GHOST_CLEAR_GHOSTS, 0, 0, 0, 0);
    for part in GATE_PARTS {
        let g = new_ghost(h, class, player, true, rotation);
        h.ghost(EE_UNITS_GHOST_SCRATCH_TRANSFORM_GET, g, 0, 0, 0);
        h.ghost(EE_UNITS_GHOST_TURN_GATE_PART, g, 0, GATE_DIRECTION, part);
        h.ghost(EE_UNITS_GHOST_SCRATCH_TRANSFORM_SET, g, 0, 0, 0);
        h.ghost(EE_UNITS_GHOST_ADD, g, 0, 0, 0);
    }
    h.ghost(EE_UNITS_GHOST_SET_CHANGED, 0, 0, 0, 0);
    h.ghost(EE_UNITS_GHOST_LOCK_LEAVE, 0, 0, 0, 0);
}

/// `EEBuildWallGhosts(tiles, corners, technology, player, rotation)`: the placement ghosts replaced by one wall
/// ghost per planned tile, shaped by its corner and set on the ground; `tiles` and `corners` the vectors' addresses.
pub fn build_wall_ghosts<H: GhostHost + ?Sized>(
    h: &mut H,
    tiles: u64,
    corners: u64,
    technology: i64,
    player: u64,
    rotation: f32,
) {
    let class = h.ghost(EE_UNITS_GHOST_TECH_CLASS, 0, player, technology, 0) as u64;
    h.ghost(EE_UNITS_GHOST_LOCK_ENTER, 0, 0, 0, 0);
    h.ghost(EE_UNITS_GHOST_CLEAR_GHOSTS, 0, 0, 0, 0);
    let mut i = 0;
    while i < h.ghost(EE_UNITS_GHOST_TILES_SIZE, 0, tiles, 0, 0) {
        let x = h.ghost(EE_UNITS_GHOST_TILES_AT, 0, tiles, i, 0);
        let y = h.ghost(EE_UNITS_GHOST_TILES_AT, 0, tiles, i, 1);
        let g = new_ghost(h, class, player, true, rotation);
        let on_map = h.ghost(EE_UNITS_GHOST_TILE_EXISTS, 0, 0, x, y) != 0;
        let raised = h.ghost(EE_UNITS_GHOST_RAISED_CORNERS, 0, 0, x, y);
        let corner = h.ghost(EE_UNITS_GHOST_CORNER_AT, 0, corners, i, 0);
        h.ghost(EE_UNITS_GHOST_ORIENT_WALL, g, 0, corner, raised);
        h.ghost(EE_UNITS_GHOST_SET_TILE_X, g, 0, x, 0);
        h.ghost(EE_UNITS_GHOST_SET_TILE_Y, g, 0, y, 0);
        h.ghost(EE_UNITS_GHOST_ADD, g, 0, 0, 0);
        if !on_map {
            h.ghost(EE_UNITS_GHOST_SET_CAN_PLACE, g, 0, 0, 0);
        } else {
            fill_square(h, g);
        }
        let cx = x as c_long as f32 + 0.5;
        let cy = y as c_long as f32 + 0.5;
        let mut height = 0.0f32;
        if on_map {
            height = f32::from_bits(h.ghost(EE_UNITS_GHOST_MESH_Z, 0, 0, bits(cx), bits(cy)) as u32);
            if height < 0.0 {
                height = 0.0;
            }
        }
        h.ghost(EE_UNITS_GHOST_SET_X, g, 0, bits(cx), 0);
        h.ghost(EE_UNITS_GHOST_SET_Y, g, 0, bits(cy), 0);
        h.ghost(EE_UNITS_GHOST_SET_Z, g, 0, bits(height), 0);
        i += 1;
    }
    h.ghost(EE_UNITS_GHOST_SET_CHANGED, 0, 0, 1, 0);
    h.ghost(EE_UNITS_GHOST_LOCK_LEAVE, 0, 0, 0, 0);
}

/// `EEBuildWallGhosts`' arguments (more than `ee_units_ghost_body` carries).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsGhostWalls {
    pub tiles: *const std::ffi::c_void,
    pub corners: *const std::ffi::c_void,
    pub technology: i64,
    pub player: *mut std::ffi::c_void,
    pub rotation: f32,
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::ghost_body::game::Game;
    use std::os::raw::c_void;

    /// `EEGhostUnit::Create(location, type, player, rotation, is_move)` (the location unused, as the original).
    ///
    /// # Safety
    /// `player` is a live player, on the thread that owns the ghosts.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ghost_create(kind: i64, player: *mut c_void, rotation: f32, is_move: u8) {
        create(&mut Game, kind, player as usize as u64, rotation, is_move != 0);
    }

    /// `EEGhostUnit::CreateGate(rotation, class, player)`.
    ///
    /// # Safety
    /// `class` and `player` are live, on the thread that owns the ghosts.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ghost_create_gate(rotation: f32, class: *mut c_void, player: *mut c_void) {
        create_gate(&mut Game, rotation, class as usize as u64, player as usize as u64);
    }

    /// `EEBuildWallGhosts` over its arguments.
    ///
    /// # Safety
    /// The arguments' vectors and player are live, on the thread that owns the ghosts.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ghost_build_walls(args: *const EeUnitsGhostWalls) {
        // SAFETY: the caller's arguments
        let a = unsafe { *args };
        build_wall_ghosts(
            &mut Game,
            a.tiles as usize as u64,
            a.corners as usize as u64,
            a.technology,
            a.player as usize as u64,
            a.rotation,
        );
    }
}

#[cfg(test)]
#[path = "ghost_make_tests.rs"]
mod tests;
