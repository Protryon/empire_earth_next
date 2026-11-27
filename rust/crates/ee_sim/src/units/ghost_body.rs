//! `EEGhostUnit`'s bodies, the placement previews (the units' lane, round 7; were EEGhostUnit.cpp): `FillSquare` and
//! its dock, rim, mining and volcano variants, `GetFootprintOffset`, `EEPreviewPlacement` and `PreviewGate`. Every
//! C++ statement of the originals is one operation of [`GhostHost::ghost`] (the game's: `ee_units_ghost_host`,
//! EEGhostUnit.cpp, one statement a case), asked in the original's order; the loops, the footprint arithmetic and
//! the choices are here. `ghost` is the `EEGhostUnit*` an operation acts on (0 for the world's), `a` an object or a
//! value, `b`, `c` values (floats as their bits). Only a drawn game reaches these (the placement cursor).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

// ---- the statements (`op`; the answer)
/// `mUnitClass->GetUnitType()`
pub const EE_UNITS_GHOST_UNIT_TYPE: i32 = 1;
/// `mUnitClass->mCategory`
pub const EE_UNITS_GHOST_CATEGORY: i32 = 2;
/// `mUnitClass->GetTileSize()`
pub const EE_UNITS_GHOST_TILE_SIZE: i32 = 3;
/// `mTileX`
pub const EE_UNITS_GHOST_TILE_X: i32 = 4;
/// `mTileY`
pub const EE_UNITS_GHOST_TILE_Y: i32 = 5;
/// `mTileX = b`
pub const EE_UNITS_GHOST_SET_TILE_X: i32 = 6;
/// `mTileY = b`
pub const EE_UNITS_GHOST_SET_TILE_Y: i32 = 7;
/// `UMCeil(the float of the bits b)` (its bits)
pub const EE_UNITS_GHOST_UM_CEIL: i32 = 8;
/// the fills' scratch position `= GE3DPoint(mTransformation)` (their `thePosition`; not the preview's)
pub const EE_UNITS_GHOST_POSITION_FROM_TRANSFORM: i32 = 9;
/// `EECanPlace(mPlayer, mUnitClass->GetUnitType(), &the fills' scratch position, b, c, 0, 0)`
pub const EE_UNITS_GHOST_CAN_PLACE_AT: i32 = 10;
/// `gWorld.GetMap()->GetTile(b, c) != NULL`
pub const EE_UNITS_GHOST_TILE_EXISTS: i32 = 11;
/// `gWorld.GetMap()->GetTile(b, c)->mTerrainType`
pub const EE_UNITS_GHOST_TILE_TERRAIN: i32 = 12;
/// `gWorld.GetMap()->mTerrainMesh.GetZ(b, c)` (its bits)
pub const EE_UNITS_GHOST_TERRAIN_Z: i32 = 13;
/// `mHeights[b] = the float of the bits c`
pub const EE_UNITS_GHOST_SET_HEIGHT: i32 = 14;
/// `mFootprint[b][c] = a != 0`
pub const EE_UNITS_GHOST_SET_FOOTPRINT: i32 = 15;
/// `EEIsLandTerrain(b)`
pub const EE_UNITS_GHOST_IS_LAND_TERRAIN: i32 = 16;
/// `EEIsSpaceDockSiteOpen(mTileX, mTileY, mPlayer)`
pub const EE_UNITS_GHOST_SPACE_DOCK_SITE_OPEN: i32 = 17;
/// `EEGetDockFacing(mTileX, mTileY)`
pub const EE_UNITS_GHOST_DOCK_FACING: i32 = 18;
/// `EEGetSpaceDockYaw(mTileX, mTileY, mUnitClass->GetTileSize())` (its bits)
pub const EE_UNITS_GHOST_SPACE_DOCK_YAW: i32 = 19;
/// `mCanPlace = b != 0`
pub const EE_UNITS_GHOST_SET_CAN_PLACE: i32 = 20;
/// `mCanPlace`
pub const EE_UNITS_GHOST_CAN_PLACE: i32 = 21;
/// `mTransformation.SetOrientationYPR(the float of the bits b, 0.0f, 0.0f)`
pub const EE_UNITS_GHOST_SET_YAW: i32 = 22;
/// `gWorld.GetMap()->GetTile(b, c)` (the tile's first unit node: its `mUnits`; 0 without the tile)
pub const EE_UNITS_GHOST_TILE_FIRST_NODE: i32 = 23;
/// node `a`'s `mNext`
pub const EE_UNITS_GHOST_NODE_NEXT: i32 = 24;
/// node `a`'s unit is of the ghost's overlap set (`mUnit != NULL && gUnitSetManager.Contains(...type, mUnitSet)`)
pub const EE_UNITS_GHOST_NODE_IN_SET: i32 = 25;
/// `ee_units_uu_is_tile_unblocked(mPlayer, the type, gWorld.GetMap()->GetTile(b, c))`
pub const EE_UNITS_GHOST_TILE_UNBLOCKED: i32 = 26;
/// `gWorld.GetMap()->IsEdgeTile(b, c)`
pub const EE_UNITS_GHOST_IS_EDGE_TILE: i32 = 27;
/// `gAEManager.HasAreaEffect2(3, b, c, mPlayer->mPlayerID)`
pub const EE_UNITS_GHOST_HAS_AREA_EFFECT_3: i32 = 28;
/// `UpdateFootprint()` / `FillRimSquare()` / `FillDockSquare()` / `FillVolcanoSquare()` / `FillSquare()` as
/// virtual-free calls are this module's own; none here
/// `gWorld.mGhostUnitsLock->Enter()`
pub const EE_UNITS_GHOST_LOCK_ENTER: i32 = 30;
/// `gWorld.mGhostUnitsLock->Leave()`
pub const EE_UNITS_GHOST_LOCK_LEAVE: i32 = 31;
/// `gViewport.GetViewRectangle().Contains(*(const GE2DPoint*)a)`
pub const EE_UNITS_GHOST_VIEW_CONTAINS: i32 = 32;
/// `gWorld.GetNumGhostUnits()`
pub const EE_UNITS_GHOST_NUM_GHOSTS: i32 = 33;
/// `gGhostUnitsChanged = b != 0`
pub const EE_UNITS_GHOST_SET_CHANGED: i32 = 34;
/// `gWorld.mGhostUnits[b]`
pub const EE_UNITS_GHOST_GHOST_AT: i32 = 35;
/// `EEPointUnitOrTile(*(const GE2DPoint*)a, the scratch tile, the preview's scratch position, true)`
pub const EE_UNITS_GHOST_POINT_UNIT_OR_TILE: i32 = 36;
/// the preview's scratch position's x, y, z (b 0, 1, 2; its bits)
pub const EE_UNITS_GHOST_POSITION: i32 = 37;
/// the preview's scratch position's x, y, z (b 0, 1, 2) `= the float of the bits c`
pub const EE_UNITS_GHOST_SET_POSITION_PART: i32 = 38;
/// `GetModel()`
pub const EE_UNITS_GHOST_GET_MODEL: i32 = 39;
/// `EEUnitClassOfKind<EE_CLASS_KIND_BUILDING>(gUnitClassTable[kUnitTypeAirport])->GetTileSize()`
pub const EE_UNITS_GHOST_AIRPORT_TILE_SIZE: i32 = 40;
/// `EEGetFacingQuarter(b, c)`
pub const EE_UNITS_GHOST_FACING_QUARTER: i32 = 41;
/// `EEGhostUnit::SetFarmSlotTile(this, b)`
pub const EE_UNITS_GHOST_SET_FARM_SLOT: i32 = 42;
/// `mUnitClass->IsLandBuilding()`
pub const EE_UNITS_GHOST_IS_LAND_BUILDING: i32 = 43;
/// `mUnitClass->mIsComplex`
pub const EE_UNITS_GHOST_IS_COMPLEX: i32 = 44;
/// `mTransformation.SetPosition(the preview's scratch position)`
pub const EE_UNITS_GHOST_SET_POSITION: i32 = 45;
/// `mUnitClass->LocationToTile(the preview's scratch position, mTileX, mTileY)`
pub const EE_UNITS_GHOST_LOCATION_TO_TILE: i32 = 46;
/// `gUnitPicker.PickUnit(*(const GE2DPoint*)a, 0)`
pub const EE_UNITS_GHOST_PICK_UNIT: i32 = 47;
/// unit `a`'s `mUnitClass->mIsWall`
pub const EE_UNITS_GHOST_UNIT_IS_WALL: i32 = 48;
/// `EEPointUnitOrTileXY(*(const GE2DPoint*)a, the scratch tile, the scratch x, the scratch y, true)`
pub const EE_UNITS_GHOST_POINT_TILE_XY: i32 = 49;
/// the scratch x, y (b 0, 1)
pub const EE_UNITS_GHOST_SCRATCH_XY: i32 = 50;
/// unit `a`'s `mTileX`, `mTileY` (b 0, 1)
pub const EE_UNITS_GHOST_UNIT_TILE: i32 = 51;
/// unit `a`'s `Dereference()`
pub const EE_UNITS_GHOST_UNIT_DEREFERENCE: i32 = 52;
/// node `a`'s `mUnit`
pub const EE_UNITS_GHOST_NODE_UNIT: i32 = 53;
/// unit `a`'s `mComplexUnit != NULL && mComplexUnit->mIsTinted`
pub const EE_UNITS_GHOST_UNIT_TINTED: i32 = 54;
/// unit `a`'s `mComplexUnit->mGateDirection`
pub const EE_UNITS_GHOST_UNIT_GATE_DIRECTION: i32 = 55;
/// `EEGetWallGateTiles(U2DPoint<long>(b, c), a, the scratch points)`
pub const EE_UNITS_GHOST_GATE_TILES: i32 = 56;
/// the scratch points' `[b]`'s x, y (c 0, 1)
pub const EE_UNITS_GHOST_SCRATCH_POINT: i32 = 57;
/// the scratch transformation `= mTransformation`
pub const EE_UNITS_GHOST_SCRATCH_TRANSFORM_GET: i32 = 58;
/// `SetAnimation(EETurnGatePart(the scratch transformation, b, c))`
pub const EE_UNITS_GHOST_TURN_GATE_PART: i32 = 59;
/// `mTransformation = the scratch transformation`
pub const EE_UNITS_GHOST_SCRATCH_TRANSFORM_SET: i32 = 60;
/// `gWorld.GetMap()->mTerrainMesh.GetMeshZ(the floats of the bits b, c)` (its bits)
pub const EE_UNITS_GHOST_MESH_Z: i32 = 61;
/// `mTransformation.SetPosition(the floats of the bits b, c, a)`
pub const EE_UNITS_GHOST_SET_POSITION_XYZ: i32 = 62;
/// `mFootprint[0][0] = mCanPlace`
pub const EE_UNITS_GHOST_FOOTPRINT_FROM_CAN_PLACE: i32 = 63;
/// the scratch points' `clear()` (the original's fresh local)
pub const EE_UNITS_GHOST_SCRATCH_POINTS_CLEAR: i32 = 64;
/// the preview's scratch position `= GE3DPoint()` (the original's fresh local)
pub const EE_UNITS_GHOST_POSITION_RESET: i32 = 65;

/// `kUnitCategoryMiningBuilding`
const CATEGORY_MINING_BUILDING: i64 = 0x18;
/// the unit types with their own fills and previews
const UNIT_TYPE_DOCK: i64 = 0x24;
const UNIT_TYPE_FARM: i64 = 0x2a;
const UNIT_TYPE_NAVY_YARD: i64 = 0x41;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_AIRPORT_RUNWAY: i64 = 0xf0;
const UNIT_TYPE_VOLCANO: i64 = 0x16e;
const UNIT_TYPE_SPACE_DOCK: i64 = 0x2d8;
const UNIT_TYPE_SPACE_TURRET: i64 = 0x302;
const UNIT_TYPE_ORBITAL_SPACE_STATION: i64 = 0x31f;
const UNIT_TYPE_ROBOTIC_FARM: i64 = 0x33a;
/// `kTerrainDeepWater`, `kTerrainShallowWater`, `kTerrainSpace`
const TERRAIN_DEEP_WATER: i64 = 1;
const TERRAIN_SHALLOW_WATER: i64 = 10;
const TERRAIN_SPACE: i64 = 12;
/// .rdata 0x446dc0: the four facing yaws (docks, robotic farm pieces)
const WALL_ROTATIONS: [f32; 4] = [0.0, std::f32::consts::PI, -std::f32::consts::FRAC_PI_2, std::f32::consts::FRAC_PI_2];
/// the footprint's row length (`mHeights` is 12 x 12)
const ROW: i64 = 12;

/// The C++ statements of the bodies.
pub trait GhostHost {
    /// one statement (`EE_UNITS_GHOST_*`) on the ghost `ghost` (0: none)
    fn ghost(&mut self, op: i32, ghost: u64, a: u64, b: i64, c: i64) -> i64;
}

struct S<'h, H: GhostHost + ?Sized> {
    h: &'h mut H,
    g: u64,
}

impl<H: GhostHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.ghost(op, self.g, 0, 0, 0)
    }
    fn qa(&mut self, op: i32, a: u64) -> i64 {
        self.h.ghost(op, self.g, a, 0, 0)
    }
    fn qb(&mut self, op: i32, b: i64) -> i64 {
        self.h.ghost(op, self.g, 0, b, 0)
    }
    fn qab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.ghost(op, self.g, a, b, 0)
    }
    fn qbc(&mut self, op: i32, b: i64, c: i64) -> i64 {
        self.h.ghost(op, self.g, 0, b, c)
    }
    fn qabc(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.h.ghost(op, self.g, a, b, c)
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
    /// the ghost operated on from here on
    fn on(&mut self, g: u64) {
        self.g = g;
    }
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

fn float(b: i64) -> f32 {
    f32::from_bits(b as u32)
}

/// `(long)value`: the C++ truncating conversion
fn trunc(value: f32) -> i64 {
    value as c_long as i64
}

/// The height of tile (x, y) for the footprint: 0 for water (and space when `space`), else the terrain's.
fn set_height<H: GhostHost + ?Sized>(s: &mut S<'_, H>, index: i64, x: i64, y: i64, space: bool) {
    let terrain = if s.qbc(EE_UNITS_GHOST_TILE_EXISTS, x, y) != 0 {
        Some(s.qbc(EE_UNITS_GHOST_TILE_TERRAIN, x, y))
    } else {
        None
    };
    let water = match terrain {
        None => true,
        Some(t) => t == TERRAIN_DEEP_WATER || t == TERRAIN_SHALLOW_WATER || (space && t == TERRAIN_SPACE),
    };
    let z = if water { 0.0 } else { float(s.qbc(EE_UNITS_GHOST_TERRAIN_Z, x, y)) };
    s.qbc(EE_UNITS_GHOST_SET_HEIGHT, index, bits(z));
}

/// `GetFootprintOffset()`: the half-width of the footprint square around the ghost's tile (1 for a one-tile unit).
pub fn footprint_offset<H: GhostHost + ?Sized>(h: &mut H, ghost: u64) -> f32 {
    let s = &mut S { h, g: ghost };
    footprint_offset_s(s)
}

fn footprint_offset_s<H: GhostHost + ?Sized>(s: &mut S<'_, H>) -> f32 {
    let size = s.q(EE_UNITS_GHOST_TILE_SIZE);
    if size <= 1 {
        1.0
    } else if size & 1 != 0 {
        float(s.qb(EE_UNITS_GHOST_UM_CEIL, bits(size as c_long as f32 * 0.5)))
    } else {
        size as c_long as f32 * 0.5 + 0.5
    }
}

/// The footprint square around the ghost's tile: (left, bottom, right, top).
fn square<H: GhostHost + ?Sized>(s: &mut S<'_, H>) -> (i64, i64, i64, i64) {
    let offset = footprint_offset_s(s);
    let x = s.q(EE_UNITS_GHOST_TILE_X) as c_long as f32;
    let y = s.q(EE_UNITS_GHOST_TILE_Y) as c_long as f32;
    (trunc(x - offset), trunc(y - offset), trunc(x + offset), trunc(y + offset))
}

/// The inner cells of the square set to `value`.
fn fill_inner<H: GhostHost + ?Sized>(s: &mut S<'_, H>, (left, bottom, right, top): (i64, i64, i64, i64), value: bool) {
    for i in left + 1..right {
        for j in bottom + 1..top {
            s.qabc(EE_UNITS_GHOST_SET_FOOTPRINT, u64::from(value), i - left - 1, j - bottom - 1);
        }
    }
}

/// `FillDockSquare()`: a space building's footprint and heights: placeable when its space dock site is open or
/// every inner cell is, with 2+ land rim tiles; then turned to the space dock yaw or the dock facing.
pub fn fill_dock_square<H: GhostHost + ?Sized>(h: &mut H, ghost: u64) {
    let s = &mut S { h, g: ghost };
    s.q(EE_UNITS_GHOST_POSITION_FROM_TRANSFORM);
    let sq = square(s);
    let (left, bottom, right, top) = sq;
    let mut yaw = 0.0f32;
    let mut border = 0;
    let mut valid = true;
    let space_dock = s.q(EE_UNITS_GHOST_UNIT_TYPE) == UNIT_TYPE_SPACE_DOCK;
    if space_dock {
        valid = s.yes(EE_UNITS_GHOST_SPACE_DOCK_SITE_OPEN);
        if valid {
            border = 2;
            fill_inner(s, sq, true);
        }
    } else {
        for x in left..=right {
            for y in bottom..=top {
                let exists = s.qbc(EE_UNITS_GHOST_TILE_EXISTS, x, y) != 0;
                set_height(s, (x - left) * ROW + y - bottom, x, y, true);
                if x == left || y == bottom || x == right || y == top {
                    if exists {
                        let terrain = s.qbc(EE_UNITS_GHOST_TILE_TERRAIN, x, y);
                        if s.qb(EE_UNITS_GHOST_IS_LAND_TERRAIN, terrain) != 0 {
                            border += 1;
                        }
                    }
                } else {
                    let mut cell = false;
                    if exists {
                        cell = s.qbc(EE_UNITS_GHOST_CAN_PLACE_AT, x, y) != 0;
                    }
                    s.qabc(EE_UNITS_GHOST_SET_FOOTPRINT, u64::from(cell), x - left - 1, y - bottom - 1);
                    if !cell {
                        valid = false;
                    }
                }
            }
        }
    }
    if valid {
        if s.q(EE_UNITS_GHOST_UNIT_TYPE) != UNIT_TYPE_SPACE_DOCK {
            yaw = WALL_ROTATIONS[s.q(EE_UNITS_GHOST_DOCK_FACING) as usize];
        } else {
            yaw = float(s.q(EE_UNITS_GHOST_SPACE_DOCK_YAW));
        }
    }
    if border < 2 {
        fill_inner(s, sq, false);
    }
    let can = valid && border >= 2;
    s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, i64::from(can));
    if s.yes(EE_UNITS_GHOST_CAN_PLACE) {
        s.qb(EE_UNITS_GHOST_SET_YAW, bits(yaw));
    }
}

/// `FillRimSquare()`: a dock's footprint and heights: placeable when every inner cell is and at least 3 rim tiles
/// are land.
pub fn fill_rim_square<H: GhostHost + ?Sized>(h: &mut H, ghost: u64) {
    let s = &mut S { h, g: ghost };
    s.q(EE_UNITS_GHOST_POSITION_FROM_TRANSFORM);
    let sq = square(s);
    let (left, bottom, right, top) = sq;
    let mut border = 0;
    let mut valid = true;
    for x in left..=right {
        for y in bottom..=top {
            let exists = s.qbc(EE_UNITS_GHOST_TILE_EXISTS, x, y) != 0;
            set_height(s, (x - left) * ROW + y - bottom, x, y, false);
            if x != left && y != bottom && x != right && y != top {
                let cell = exists && s.qbc(EE_UNITS_GHOST_CAN_PLACE_AT, x, y) != 0;
                s.qabc(EE_UNITS_GHOST_SET_FOOTPRINT, u64::from(cell), x - left - 1, y - bottom - 1);
                if !cell {
                    valid = false;
                }
            } else if exists {
                let terrain = s.qbc(EE_UNITS_GHOST_TILE_TERRAIN, x, y);
                if s.qb(EE_UNITS_GHOST_IS_LAND_TERRAIN, terrain) != 0 {
                    border += 1;
                }
            }
        }
    }
    if border < 3 {
        fill_inner(s, sq, false);
    }
    s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, i64::from(valid && border >= 3));
}

/// `UpdateFootprint()`: a mining building's whole footprint placeable when a unit of its overlap set stands on
/// tile (x - 1, y - 1).
pub fn update_footprint<H: GhostHost + ?Sized>(h: &mut H, ghost: u64) {
    let s = &mut S { h, g: ghost };
    let sq = square(s);
    s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, 0);
    let x = s.q(EE_UNITS_GHOST_TILE_X) - 1;
    let y = s.q(EE_UNITS_GHOST_TILE_Y) - 1;
    let mut node = s.qbc(EE_UNITS_GHOST_TILE_FIRST_NODE, x, y) as u64;
    while node != 0 && !s.yes(EE_UNITS_GHOST_CAN_PLACE) {
        if s.qa(EE_UNITS_GHOST_NODE_IN_SET, node) != 0 {
            s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, 1);
        }
        node = s.qa(EE_UNITS_GHOST_NODE_NEXT, node) as u64;
    }
    let can = s.yes(EE_UNITS_GHOST_CAN_PLACE);
    fill_inner(s, sq, can);
}

/// The plain square of a size (left, bottom, size): a one-tile unit's at its tile, else centred.
fn plain_square<H: GhostHost + ?Sized>(s: &mut S<'_, H>) -> (i64, i64, i64) {
    let size = s.q(EE_UNITS_GHOST_TILE_SIZE);
    if size <= 1 {
        (s.q(EE_UNITS_GHOST_TILE_X), s.q(EE_UNITS_GHOST_TILE_Y), 1)
    } else {
        let half = trunc(size as c_long as f32 * 0.5);
        (s.q(EE_UNITS_GHOST_TILE_X) - half, s.q(EE_UNITS_GHOST_TILE_Y) - half, size)
    }
}

/// `FillVolcanoSquare()`: a volcano's footprint and heights: each cell unblocked, off the map edge and free of
/// area effect 3.
pub fn fill_volcano_square<H: GhostHost + ?Sized>(h: &mut H, ghost: u64) {
    let s = &mut S { h, g: ghost };
    s.q(EE_UNITS_GHOST_POSITION_FROM_TRANSFORM);
    let (left, bottom, size) = plain_square(s);
    let (right, top) = (left + size, bottom + size);
    let mut valid = true;
    for x in left..=right {
        for y in bottom..=top {
            let cell = if s.qbc(EE_UNITS_GHOST_TILE_EXISTS, x, y) != 0 {
                let mut cell = s.qbc(EE_UNITS_GHOST_TILE_UNBLOCKED, x, y) != 0;
                set_height(s, (x - left) * ROW + y - bottom, x, y, false);
                if cell {
                    cell = s.qbc(EE_UNITS_GHOST_IS_EDGE_TILE, x, y) == 0;
                    if cell {
                        cell = s.qbc(EE_UNITS_GHOST_HAS_AREA_EFFECT_3, x, y) == 0;
                    }
                }
                cell
            } else {
                s.qbc(EE_UNITS_GHOST_SET_HEIGHT, (x - left) * ROW + y - bottom, bits(0.0));
                false
            };
            s.qabc(EE_UNITS_GHOST_SET_FOOTPRINT, u64::from(cell), x - left, y - bottom);
            if x < right && y < top && !cell {
                valid = false;
            }
        }
    }
    s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, i64::from(valid));
}

/// `FillSquare()`: the ghost's footprint and heights and whether it can be placed (mining, dock, space and volcano
/// buildings by their own fills).
pub fn fill_square<H: GhostHost + ?Sized>(h: &mut H, ghost: u64) {
    let s = &mut S { h, g: ghost };
    s.q(EE_UNITS_GHOST_POSITION_FROM_TRANSFORM);
    let kind = s.q(EE_UNITS_GHOST_UNIT_TYPE);
    if s.q(EE_UNITS_GHOST_CATEGORY) == CATEGORY_MINING_BUILDING {
        return update_footprint(s.h, ghost);
    }
    if kind == UNIT_TYPE_DOCK || kind == UNIT_TYPE_NAVY_YARD {
        return fill_rim_square(s.h, ghost);
    }
    if kind == UNIT_TYPE_SPACE_DOCK || kind == UNIT_TYPE_SPACE_TURRET || kind == UNIT_TYPE_ORBITAL_SPACE_STATION {
        return fill_dock_square(s.h, ghost);
    }
    if kind == UNIT_TYPE_VOLCANO {
        return fill_volcano_square(s.h, ghost);
    }
    let (left, bottom, size) = plain_square(s);
    let (top, right) = (bottom + size, left + size);
    let mut valid = true;
    for x in left..=right {
        for y in bottom..=top {
            let index = (x - left) * ROW + y - bottom;
            let cell = if s.qbc(EE_UNITS_GHOST_TILE_EXISTS, x, y) != 0 {
                let cell = s.qbc(EE_UNITS_GHOST_CAN_PLACE_AT, x, y) != 0;
                set_height(s, index, x, y, false);
                cell
            } else {
                s.qbc(EE_UNITS_GHOST_SET_HEIGHT, index, bits(0.0));
                false
            };
            s.qabc(EE_UNITS_GHOST_SET_FOOTPRINT, u64::from(cell), x - left, y - bottom);
            if x < right && y < top && !cell {
                valid = false;
            }
        }
    }
    s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, i64::from(valid));
}

/// `EEPreviewPlacement(location)`: the placement ghosts moved under the cursor and each rechecked; an airport turns
/// to its facing quarter with its runways behind it, farm ghosts take their slots. `location` is the
/// `const GE2DPoint*`.
pub fn preview_placement<H: GhostHost + ?Sized>(h: &mut H, location: u64) {
    let s = &mut S { h, g: 0 };
    let mut direction = 0i64;
    s.q(EE_UNITS_GHOST_POSITION_RESET);
    s.q(EE_UNITS_GHOST_LOCK_ENTER);
    if s.qa(EE_UNITS_GHOST_VIEW_CONTAINS, location) == 0 || s.q(EE_UNITS_GHOST_NUM_GHOSTS) == 0 {
        s.qb(EE_UNITS_GHOST_SET_CHANGED, 0);
    } else {
        s.qb(EE_UNITS_GHOST_SET_CHANGED, 1);
        let on_map = s.qa(EE_UNITS_GHOST_POINT_UNIT_OR_TILE, location) != 0;
        let x = trunc(float(s.qb(EE_UNITS_GHOST_POSITION, 0)));
        let y = trunc(float(s.qb(EE_UNITS_GHOST_POSITION, 1)));
        let mut i = 0i64;
        while i < s.q(EE_UNITS_GHOST_NUM_GHOSTS) {
            let ghost = s.qb(EE_UNITS_GHOST_GHOST_AT, i) as u64;
            s.on(ghost);
            s.q(EE_UNITS_GHOST_GET_MODEL);
            s.qb(EE_UNITS_GHOST_SET_TILE_X, x);
            s.qb(EE_UNITS_GHOST_SET_TILE_Y, y);
            let kind = s.q(EE_UNITS_GHOST_UNIT_TYPE);
            if kind == UNIT_TYPE_AIRPORT {
                let size = s.q(EE_UNITS_GHOST_AIRPORT_TILE_SIZE);
                direction = s.qbc(EE_UNITS_GHOST_FACING_QUARTER, x - size / 2, y - size / 2);
                if (0..4).contains(&direction) {
                    s.qb(EE_UNITS_GHOST_SET_YAW, bits(WALL_ROTATIONS[direction as usize]));
                }
            } else if kind == UNIT_TYPE_AIRPORT_RUNWAY {
                let offset = (s.q(EE_UNITS_GHOST_TILE_SIZE) as c_long as u32).wrapping_mul(i as u32) as f32;
                match direction {
                    0 => s.qb(EE_UNITS_GHOST_SET_TILE_Y, y + trunc(offset)),
                    1 => s.qb(EE_UNITS_GHOST_SET_TILE_Y, y - trunc(offset)),
                    2 => s.qb(EE_UNITS_GHOST_SET_TILE_X, x + trunc(offset)),
                    3 => s.qb(EE_UNITS_GHOST_SET_TILE_X, x - trunc(offset)),
                    _ => 0,
                };
            } else if (kind == UNIT_TYPE_FARM || kind == UNIT_TYPE_ROBOTIC_FARM) && s.q(EE_UNITS_GHOST_NUM_GHOSTS) != 1
            {
                s.qb(EE_UNITS_GHOST_SET_FARM_SLOT, i);
            }
            if (s.q(EE_UNITS_GHOST_TILE_SIZE) > 1
                || s.yes(EE_UNITS_GHOST_IS_LAND_BUILDING)
                || (s.q(EE_UNITS_GHOST_UNIT_TYPE) != UNIT_TYPE_PATH_POINT && s.q(EE_UNITS_GHOST_IS_COMPLEX) != 1))
                && on_map
            {
                let tx = s.q(EE_UNITS_GHOST_TILE_X) as c_long as f32;
                let ty = s.q(EE_UNITS_GHOST_TILE_Y) as c_long as f32;
                let (px, py) = if s.q(EE_UNITS_GHOST_TILE_SIZE) & 1 == 0 { (tx, ty) } else { (tx + 0.5, ty + 0.5) };
                s.qbc(EE_UNITS_GHOST_SET_POSITION_PART, 0, bits(px));
                s.qbc(EE_UNITS_GHOST_SET_POSITION_PART, 1, bits(py));
                let (gx, gy) = (s.q(EE_UNITS_GHOST_TILE_X), s.q(EE_UNITS_GHOST_TILE_Y));
                if s.qbc(EE_UNITS_GHOST_TILE_EXISTS, gx, gy) != 0 {
                    let px = s.qb(EE_UNITS_GHOST_POSITION, 0);
                    let py = s.qb(EE_UNITS_GHOST_POSITION, 1);
                    let mut z = float(s.qbc(EE_UNITS_GHOST_MESH_Z, px, py));
                    s.qbc(EE_UNITS_GHOST_SET_POSITION_PART, 2, bits(z));
                    if float(s.qb(EE_UNITS_GHOST_POSITION, 2)) < 0.0 {
                        z = 0.0;
                        s.qbc(EE_UNITS_GHOST_SET_POSITION_PART, 2, bits(z));
                    }
                }
            }
            s.q(EE_UNITS_GHOST_SET_POSITION);
            if !on_map {
                s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, 0);
            } else {
                if s.q(EE_UNITS_GHOST_UNIT_TYPE) == UNIT_TYPE_AIRPORT_RUNWAY {
                    for j in 0..i {
                        s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, 1);
                        let other = s.qb(EE_UNITS_GHOST_GHOST_AT, j) as u64;
                        if s.h.ghost(EE_UNITS_GHOST_CAN_PLACE, other, 0, 0, 0) == 0 {
                            s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, 0);
                        }
                        if s.yes(EE_UNITS_GHOST_CAN_PLACE) {
                            fill_square(s.h, ghost);
                        }
                    }
                } else {
                    fill_square(s.h, ghost);
                }
                if !s.yes(EE_UNITS_GHOST_CAN_PLACE) && s.q(EE_UNITS_GHOST_UNIT_TYPE) == UNIT_TYPE_AIRPORT_RUNWAY {
                    for k in 0..i {
                        let other = s.qb(EE_UNITS_GHOST_GHOST_AT, k) as u64;
                        s.h.ghost(EE_UNITS_GHOST_SET_CAN_PLACE, other, 0, 0, 0);
                    }
                }
            }
            s.q(EE_UNITS_GHOST_LOCATION_TO_TILE);
            let (gx, gy) = (s.q(EE_UNITS_GHOST_TILE_X), s.q(EE_UNITS_GHOST_TILE_Y));
            if s.qbc(EE_UNITS_GHOST_TILE_EXISTS, gx, gy) == 0 {
                s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, 0);
            }
            s.on(0);
            i += 1;
        }
    }
    s.q(EE_UNITS_GHOST_LOCK_LEAVE);
}

/// One gate part placed on its tile (`index` of the gate's five tiles, `part` the turn's): the turn, the tile, the
/// position on the ground (over water by the tile `water_at`, or its own tile: the original tests the cursor's tile
/// for the middle part) and the 2 x 2 heights, placeable only over a marked wall.
fn place_gate_part<H: GhostHost + ?Sized>(
    s: &mut S<'_, H>,
    ghost: u64,
    direction: i64,
    part: i64,
    index: i64,
    can: bool,
    water_at: Option<(i64, i64)>,
) {
    s.on(ghost);
    s.q(EE_UNITS_GHOST_SCRATCH_TRANSFORM_GET);
    s.qbc(EE_UNITS_GHOST_TURN_GATE_PART, direction, part);
    s.q(EE_UNITS_GHOST_SCRATCH_TRANSFORM_SET);
    let tx = s.qbc(EE_UNITS_GHOST_SCRATCH_POINT, index, 0);
    s.qb(EE_UNITS_GHOST_SET_TILE_X, tx);
    let ty = s.qbc(EE_UNITS_GHOST_SCRATCH_POINT, index, 1);
    s.qb(EE_UNITS_GHOST_SET_TILE_Y, ty);
    let (sx, sy) = (s.q(EE_UNITS_GHOST_TILE_X), s.q(EE_UNITS_GHOST_TILE_Y));
    let (fx, fy) = (sx as c_long as f32 + 0.5, sy as c_long as f32 + 0.5);
    let (wx, wy) = water_at.unwrap_or((sx, sy));
    let water = s.qbc(EE_UNITS_GHOST_TILE_EXISTS, wx, wy) == 0 || {
        let t = s.qbc(EE_UNITS_GHOST_TILE_TERRAIN, wx, wy);
        t == TERRAIN_DEEP_WATER || t == TERRAIN_SHALLOW_WATER
    };
    let z = if water { 0.0 } else { float(s.qbc(EE_UNITS_GHOST_MESH_Z, bits(fx), bits(fy))) };
    s.qabc(EE_UNITS_GHOST_SET_POSITION_XYZ, bits(z) as u64, bits(fx), bits(fy));
    for x in sx..=sx + 1 {
        for y in sy..=sy + 1 {
            if s.qbc(EE_UNITS_GHOST_TILE_EXISTS, x, y) == 0 {
                continue;
            }
            set_height(s, (x - sx) * ROW + y - sy, x, y, false);
        }
    }
    s.qb(EE_UNITS_GHOST_SET_CAN_PLACE, i64::from(can));
    s.q(EE_UNITS_GHOST_FOOTPRINT_FROM_CAN_PLACE);
}

/// `PreviewGate(location, direction)`: the gate ghost's three parts moved onto the wall tiles under the cursor;
/// placeable only over a marked wall piece, whose gate direction it takes (`direction` in and out).
pub fn preview_gate<H: GhostHost + ?Sized>(h: &mut H, location: u64, direction: &mut i64) {
    let s = &mut S { h, g: 0 };
    let mut new_gate = true;
    s.q(EE_UNITS_GHOST_LOCK_ENTER);
    if s.qa(EE_UNITS_GHOST_VIEW_CONTAINS, location) == 0 {
        s.qb(EE_UNITS_GHOST_SET_CHANGED, 0);
    } else {
        s.qb(EE_UNITS_GHOST_SET_CHANGED, 1);
        let unit = s.qa(EE_UNITS_GHOST_PICK_UNIT, location) as u64;
        let (x, y);
        if unit == 0 || s.qa(EE_UNITS_GHOST_UNIT_IS_WALL, unit) == 0 {
            s.qa(EE_UNITS_GHOST_POINT_TILE_XY, location);
            x = s.qb(EE_UNITS_GHOST_SCRATCH_XY, 0);
            y = s.qb(EE_UNITS_GHOST_SCRATCH_XY, 1);
        } else {
            x = s.qab(EE_UNITS_GHOST_UNIT_TILE, unit, 0);
            y = s.qab(EE_UNITS_GHOST_UNIT_TILE, unit, 1);
        }
        if unit != 0 {
            s.qa(EE_UNITS_GHOST_UNIT_DEREFERENCE, unit);
        }
        let left = s.qb(EE_UNITS_GHOST_GHOST_AT, 0) as u64;
        let middle = s.qb(EE_UNITS_GHOST_GHOST_AT, 1) as u64;
        let right = s.qb(EE_UNITS_GHOST_GHOST_AT, 2) as u64;
        if s.qbc(EE_UNITS_GHOST_TILE_EXISTS, x, y) != 0 {
            let mut node = s.qbc(EE_UNITS_GHOST_TILE_FIRST_NODE, x, y) as u64;
            let mut existing = if node != 0 { s.qa(EE_UNITS_GHOST_NODE_UNIT, node) as u64 } else { 0 };
            while existing != 0 && s.qa(EE_UNITS_GHOST_UNIT_IS_WALL, existing) == 0 && node != 0 {
                node = s.qa(EE_UNITS_GHOST_NODE_NEXT, node) as u64;
                if node != 0 {
                    existing = s.qa(EE_UNITS_GHOST_NODE_UNIT, node) as u64;
                }
            }
            if existing != 0
                && s.qa(EE_UNITS_GHOST_UNIT_IS_WALL, existing) != 0
                && s.qa(EE_UNITS_GHOST_UNIT_TINTED, existing) != 0
            {
                new_gate = false;
                *direction = s.qa(EE_UNITS_GHOST_UNIT_GATE_DIRECTION, existing);
            }
            s.q(EE_UNITS_GHOST_SCRATCH_POINTS_CLEAR);
            s.qabc(EE_UNITS_GHOST_GATE_TILES, *direction as u64, x, y);
            let d = *direction;
            place_gate_part(s, middle, d, 2, 2, !new_gate, Some((x, y)));
            place_gate_part(s, left, d, 0, 0, !new_gate, None);
            place_gate_part(s, right, d, 4, 4, !new_gate, None);
            s.on(0);
        }
    }
    s.q(EE_UNITS_GHOST_LOCK_LEAVE);
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        pub(crate) fn ee_units_ghost_host(op: i32, ghost: *mut c_void, a: u64, b: i64, c: i64) -> i64;
    }

    /// the game's host (ghost_make.rs's extern functions use it too)
    pub(crate) struct Game;

    impl GhostHost for Game {
        fn ghost(&mut self, op: i32, ghost: u64, a: u64, b: i64, c: i64) -> i64 {
            if let Some(v) = crate::units::ghost_scratch_view::ghost_scratch_view(op, ghost, a, b, c) {
                return v;
            }
            if let Some(v) = crate::units::tech_cases::ghost(op, a, b) {
                return v;
            }
            if let Some(v) = crate::units::ghost_view::ghost_view(op, ghost, a, b, c) {
                return v;
            }
            // SAFETY: the ghosts are the world's, under its ghost lock or the caller's own
            unsafe { ee_units_ghost_host(op, ghost as usize as *mut c_void, a, b, c) }
        }
    }

    /// `EEGhostUnit`'s body `what` (`EE_UNITS_GHOST_DO_*`) on `ghost` (`a`: the location, a `const GE2DPoint*`;
    /// `b`: the gate direction, a `long*`); the footprint offset's bits for `EE_UNITS_GHOST_DO_FOOTPRINT_OFFSET`.
    ///
    /// # Safety
    /// `ghost` is a live `EEGhostUnit` (or null for the previews), `a` and `b` live as the body needs them.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ghost_body(
        what: i32,
        ghost: *mut c_void,
        a: *const c_void,
        b: *mut c_long,
    ) -> i64 {
        let g = ghost as usize as u64;
        match what {
            EE_UNITS_GHOST_DO_FILL_SQUARE => fill_square(&mut Game, g),
            EE_UNITS_GHOST_DO_FILL_DOCK_SQUARE => fill_dock_square(&mut Game, g),
            EE_UNITS_GHOST_DO_FILL_RIM_SQUARE => fill_rim_square(&mut Game, g),
            EE_UNITS_GHOST_DO_UPDATE_FOOTPRINT => update_footprint(&mut Game, g),
            EE_UNITS_GHOST_DO_FILL_VOLCANO_SQUARE => fill_volcano_square(&mut Game, g),
            EE_UNITS_GHOST_DO_FOOTPRINT_OFFSET => return bits(footprint_offset(&mut Game, g)),
            EE_UNITS_GHOST_DO_PREVIEW_PLACEMENT => preview_placement(&mut Game, a as usize as u64),
            EE_UNITS_GHOST_DO_PREVIEW_GATE => {
                // SAFETY: the caller's live direction
                let mut d = i64::from(unsafe { *b });
                preview_gate(&mut Game, a as usize as u64, &mut d);
                // SAFETY: as above
                unsafe { *b = d as c_long };
            }
            _ => {}
        }
        0
    }
}

// ---- the bodies (`ee_units_ghost_body`'s `what`)
pub const EE_UNITS_GHOST_DO_FILL_SQUARE: i32 = 1;
pub const EE_UNITS_GHOST_DO_FILL_DOCK_SQUARE: i32 = 2;
pub const EE_UNITS_GHOST_DO_FILL_RIM_SQUARE: i32 = 3;
pub const EE_UNITS_GHOST_DO_UPDATE_FOOTPRINT: i32 = 4;
pub const EE_UNITS_GHOST_DO_FILL_VOLCANO_SQUARE: i32 = 5;
pub const EE_UNITS_GHOST_DO_FOOTPRINT_OFFSET: i32 = 6;
pub const EE_UNITS_GHOST_DO_PREVIEW_PLACEMENT: i32 = 7;
pub const EE_UNITS_GHOST_DO_PREVIEW_GATE: i32 = 8;

#[cfg(test)]
#[path = "ghost_body_tests.rs"]
mod tests;
