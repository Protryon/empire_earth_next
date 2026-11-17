//! The unit utilities over the world (the unit utilities lane, round 1): the tile and unit searches, the placement
//! tests and the wall lines of `src/Empire Earth/User Interface Thread/EEFOFindNewTargetNearOldTarget.cpp` (the
//! original's "EEUnitUtilities.cpp"), whole. Their pure geometry is [`crate::uu_tiles`].
//!
//! The world is the [`UuHost`]: the map's tiles and their unit lists (walked in node order, as the C++ did), the
//! virtuals a body called (`IsPassableBy`, `CanMoveOnTerrain`, `GetPosition`, `GetSurroundingTiles`), the continent
//! manager, the C++ leaves that stay (`EEGetTileLine` with its CBTL record, `EEIsLineBlocked`) and the few writes
//! (a wall piece's frame set and facing). A unit's plain members are read in place, in the records its object
//! derives from ([`EeUnitsUuUnit`]: `EeUnitsRtsUnitFields`, `EeUnitsUnitFields`, `EeUnitsComplexUnitFields`,
//! `EeUnitsUnitClassFields`), one field at a time and never across a host call.
//!
//! Determinism (RUST_PLAN 2.1): the C++'s statements in order, its float operations in order, the C++ sorts as MSVC
//! sorts (`crate::msvc_sort`: an insertion sort up to 32 tiles, the introsort above), the random draws through the host in
//! the C++'s order and number.

// `c_long` is 32 bits on Windows and 64 on LP64: the casts are needed there. The C++'s negated float comparisons
// (`!(a < b)`) are kept as written: they differ from the flipped comparison for a NaN.
#![allow(clippy::unnecessary_cast, clippy::neg_cmp_op_on_partial_ord)]

use crate::unit_class_fields::EeUnitsUnitClassFields;
use crate::unit_fields::{bool_byte, EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use crate::uu_tiles::{jitter_heading, square_ring, sub_tile_steps, turn_direction, Tile, RING_ORDER_12, RING_ORDER_9};
use ee_render::ge::transform::RotationOrder;
use ee_render::ge::types::{Ge2DPoint, Ge3DPoint, GeTransformation};
use ee_render::ge::um::{floor, um_arctan2};

/// The unit types, categories, families, movement bits and terrains the bodies test (the C++ enums' values).
/// cbindgen:ignore
pub mod k {
    pub const UU_TYPE_FARM: u32 = 0x2a;
    pub const UU_TYPE_GRANARY: u32 = 0x34;
    pub const UU_TYPE_AIRPORT: u32 = 0x42;
    pub const UU_TYPE_PATH_POINT: u32 = 0xdf;
    pub const UU_TYPE_AIRPORT_RUNWAY: u32 = 0xf0;
    pub const UU_TYPE_WAYPOINT: u32 = 0x104;
    pub const UU_TYPE_GRANARY_IMPROVED: u32 = 0x2ba;
    pub const UU_TYPE_ROBOTIC_FARM: u32 = 0x33a;
    pub const UU_CATEGORY_RESOURCE: u32 = 1;
    pub const UU_CATEGORY_LAND_CITIZEN: u32 = 6;
    pub const UU_CATEGORY_NAVAL_CITIZEN: u32 = 0x16;
    pub const UU_CATEGORY_GATE: u32 = 0x19;
    pub const UU_FAMILY_ANIMAL: u32 = 0x11;
    pub const UU_MOVEMENT_AIR: u8 = 1;
    pub const UU_MOVEMENT_SEA: u8 = 4;
    pub const UU_MOVEMENT_SPACE: u8 = 8;
    pub const UU_TERRAIN_SPACE: i32 = 12;
}
use k::*;

/// A unit as the utilities read it: its object (`EEUnit*`, its records' addresses) and the pointer members and
/// virtuals' answers the host reads once (`ee_units_uu_host_unit`). The records are null for a missing part (no
/// complex unit, no class); `obj` 0 is no unit.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsUuUnit {
    pub obj: u64,
    pub rts: *const EeUnitsRtsUnitFields,
    pub unit: *const EeUnitsUnitFields,
    /// `mComplexUnit`'s record
    pub cx: *const EeUnitsComplexUnitFields,
    /// `mUnitClass`'s record
    pub class_fields: *const EeUnitsUnitClassFields,
    pub class_obj: u64,
    pub complex: u64,
    pub player: u64,
    /// `mTile` (`EETile*`)
    pub tile: u64,
    /// the complex unit's `mActiveGroup`
    pub active_group: u64,
    /// `mPlayer->GetPlayerID()` (-1: no player)
    pub player_id: i32,
    /// `mTileNode != NULL`
    pub on_map: u8,
    /// the class's `IsLandBuilding()`, `FliesOver()` (virtuals)
    pub land_building: u8,
    pub flies_over: u8,
    /// the complex unit's location (`mPhysicalModel.GetLocation()`'s x, y, z)
    pub location: [f32; 3],
    /// the complex unit's `mStats->mMaxVelocity`, `mStats->mAttack`
    pub stats_max_velocity: f32,
    pub stats_attack: f32,
    /// the complex unit's `mStats != NULL`; its current command's `mType` (0: none)
    pub has_stats: u8,
    pub command: i32,
}

impl Default for EeUnitsUuUnit {
    fn default() -> Self {
        EeUnitsUuUnit {
            obj: 0,
            rts: std::ptr::null(),
            unit: std::ptr::null(),
            cx: std::ptr::null(),
            class_fields: std::ptr::null(),
            class_obj: 0,
            complex: 0,
            player: 0,
            tile: 0,
            active_group: 0,
            player_id: -1,
            on_map: 0,
            land_building: 0,
            flies_over: 0,
            location: [0.0; 3],
            stats_max_velocity: 0.0,
            stats_attack: 0.0,
            has_stats: 0,
            command: 0,
        }
    }
}

/// A unit class as the utilities read it: its record and its two virtuals' answers (`ee_units_uu_host_class`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeUnitsUuClass {
    pub obj: u64,
    pub fields: *const EeUnitsUnitClassFields,
    pub land_building: u8,
    pub flies_over: u8,
}

impl Default for EeUnitsUuClass {
    fn default() -> Self {
        EeUnitsUuClass { obj: 0, fields: std::ptr::null(), land_building: 0, flies_over: 0 }
    }
}

/// A unit class record's members, read in place.
/// cbindgen:ignore
#[derive(Clone, Copy, Debug)]
pub struct UuClassView(pub *const EeUnitsUnitClassFields, pub u64);

macro_rules! class_field {
    ($name:ident, $field:ident, $ty:ty) => {
        pub fn $name(&self) -> $ty {
            // SAFETY: the host's record of a live class (the db's classes live for the game)
            unsafe { (*self.0).$field as $ty }
        }
    };
}
macro_rules! class_flag {
    ($name:ident, $field:ident) => {
        pub fn $name(&self) -> bool {
            // SAFETY: as `class_field`; the bool member read as a byte
            unsafe { bool_byte(std::ptr::addr_of!((*self.0).$field)) != 0 }
        }
    };
}

impl UuClassView {
    class_field!(unit_type, mUnitType, u32);
    class_field!(category, mCategory, u32);
    class_field!(family, mFamily, u32);
    class_field!(tile_size, mTileSize, i32);
    class_field!(domains, mDomains, u8);
    class_field!(inner, mInnerCollisionSphere, f32);
    class_field!(outer, mOuterCollisionSphere, f32);
    class_field!(max_velocity, mMaxVelocity, f32);
    class_field!(overlap_unit_set, mOverlapUnitSet, i32);
    class_field!(cruise_altitude, mCruiseAltitude, f32);
    class_field!(is_complex, mIsComplex, i32);
    class_field!(air_traffic_type, mAirTrafficType, i32);
    class_field!(garrison_capacity, mGarrisonCapacity, i32);
    class_field!(garrison_bonus, mGarrisonBonus, f32);
    class_flag!(is_wall, mIsWall);
    class_flag!(is_tower, mIsTower);
    class_flag!(is_tree, mIsTree);
    class_flag!(is_decoration, mIsDecoration);
    class_flag!(blocks_tile, mBlocksTile);
    class_flag!(is_collidable, mIsCollidable);
    class_flag!(can_fly, mCanFly);
    class_flag!(builds_on_elevation, mBuildsOnElevation);
    class_flag!(pitches_with_terrain, mPitchesWithTerrain);
    class_flag!(rolls_with_terrain, mRollsWithTerrain);
    /// `mTerrainTypes[terrain]` (the C++ indexes its 17 bools unchecked; another index is false here)
    pub fn terrain(&self, terrain: i32) -> bool {
        if !(0..17).contains(&terrain) {
            return false;
        }
        // SAFETY: as `class_field`, the index in the array
        unsafe { bool_byte(std::ptr::addr_of!((*self.0).mTerrainTypes[terrain as usize])) != 0 }
    }
}

/// A unit's plain members, read in place.
/// cbindgen:ignore
#[derive(Clone, Copy, Debug)]
pub struct UuView(pub EeUnitsUuUnit);

impl UuView {
    pub fn obj(&self) -> u64 {
        self.0.obj
    }
    pub fn class(&self) -> UuClassView {
        UuClassView(self.0.class_fields, self.0.class_obj)
    }
    pub fn id(&self) -> u32 {
        // SAFETY: the host's records of a live unit
        unsafe { (*self.0.rts).mUnitID as u32 }
    }
    pub fn tile_x(&self) -> i32 {
        // SAFETY: as `id`
        unsafe { (*self.0.rts).mTileX as i32 }
    }
    pub fn tile_y(&self) -> i32 {
        // SAFETY: as `id`
        unsafe { (*self.0.rts).mTileY as i32 }
    }
    pub fn hit_points(&self) -> i32 {
        // SAFETY: as `id`
        unsafe { (*self.0.unit).mHitPoints as i32 }
    }
    pub fn has_complex(&self) -> bool {
        !self.0.cx.is_null()
    }
    /// the complex unit's fields (the caller checked `has_complex`)
    fn cx(&self) -> &EeUnitsComplexUnitFields {
        // SAFETY: a non-null record of a live complex unit; the reference ends with the read that takes it
        unsafe { &*self.0.cx }
    }
    pub fn animation(&self) -> u32 {
        self.cx().mAnimation as u32
    }
    pub fn water_state(&self) -> i32 {
        self.cx().mWaterState as i32
    }
    pub fn physics_action(&self) -> u32 {
        self.cx().mCurrentPhysicsAction as u32
    }
    pub fn num_builders(&self) -> i32 {
        self.cx().mNumBuilders as i32
    }
    pub fn is_built(&self) -> bool {
        // SAFETY: as `cx`
        unsafe { bool_byte(std::ptr::addr_of!((*self.0.cx).mIsBuilt)) != 0 }
    }
    pub fn on_map(&self) -> bool {
        self.0.on_map != 0
    }
    pub fn land_building(&self) -> bool {
        self.0.land_building != 0
    }
}

/// The world the utilities ask (the C++ in the game: `ee_units_uu_host_*`, EEFOFindNewTargetNearOldTarget.cpp).
pub trait UuHost {
    /// `gWorld.mMap` (0: none)
    fn map(&mut self) -> u64;
    /// `RTSMap::GetTile(x, y)` of `map` (its `unsigned long` bounds test; 0: off the map)
    fn map_tile(&mut self, map: u64, x: i32, y: i32) -> u64;
    /// the tile's `mTerrainType`, `GetHeight()`
    fn tile_terrain(&mut self, tile: u64) -> i32;
    fn tile_height(&mut self, tile: u64) -> f32;
    /// the tile's unit list in node order (`mUnits`, `mNext`): each node's unit (0 for a NULL one)
    fn tile_units(&mut self, tile: u64, out: &mut Vec<u64>);
    /// `RTSMap::IsEdgeTile(x, y)`
    fn is_edge_tile(&mut self, x: i32, y: i32) -> bool;
    /// `map->GetNumRaisedCorners(x, y)`, `map->mTerrainMesh.GetZ(x, y)`, `GetMeshZ(x, y)`
    fn raised_corners(&mut self, map: u64, x: i32, y: i32) -> i32;
    fn mesh_z(&mut self, map: u64, x: i32, y: i32) -> f32;
    fn mesh_ground(&mut self, x: f32, y: f32) -> f32;
    /// a unit's records and pointer members (`obj` an `EEUnit*`)
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit;
    /// `gUnitClassTable[unit_type]`'s record
    fn class_of_type(&mut self, unit_type: i32) -> EeUnitsUuClass;
    /// a class object's record
    fn class(&mut self, class: u64) -> EeUnitsUuClass;
    /// `obj->GetPosition(out)` (virtual)
    fn position(&mut self, obj: u64) -> [f32; 3];
    /// `obj->IsPassableBy(by)` (virtual; `by` an `EEComplexUnit*` or 0)
    fn passable_by(&mut self, obj: u64, by: u64) -> bool;
    /// `obj->IsFoundation()` (virtual)
    fn is_foundation(&mut self, obj: u64) -> bool;
    /// `class->CanMoveOnTerrain(unit, terrain)`
    fn can_move_on_terrain(&mut self, class: u64, unit: u64, terrain: i32) -> bool;
    /// `class->GetPassableFlags(unit)`
    fn passable_flags(&mut self, class: u64, unit: u64) -> i32;
    /// `target->mUnitClass->GetSurroundingTiles(target, out, 1, false)`, through the vtable when `virtual_call`
    fn surrounding_tiles(&mut self, target: u64, virtual_call: bool) -> Vec<Tile>;
    /// `container->GetClass()->GetContainerContinent(container, unit)` (0: none)
    fn container_continent(&mut self, container: u64, unit: u64) -> u64;
    /// `gContinentManager`'s `GetContinentID(x, y)`, `GetContinentByID`, `GetContinentAt(x, y)`, the continent's
    /// `GetID()`, `AreConnected`, `AreConnectedForUnit`, `GetTileFlag`, `mTileFlags[row * mFlagsWidth + column]`
    fn continent_id(&mut self, x: i32, y: i32) -> u16;
    fn continent_by_id(&mut self, id: u16) -> u64;
    fn continent_at(&mut self, x: i32, y: i32) -> u64;
    fn continent_number(&mut self, continent: u64) -> u16;
    fn are_connected(&mut self, a: u64, b: u64, flags: i32) -> bool;
    fn are_connected_for_unit(&mut self, unit: u64, a: u64, b: u64) -> bool;
    fn continent_tile_flag(&mut self, x: i32, y: i32) -> i32;
    fn continent_link(&mut self, row: u16, column: u16) -> bool;
    /// `EEIsLineBlocked(x1, y1, x2, y2, ...)` (its outputs dropped)
    fn line_blocked(&mut self, x1: i32, y1: i32, x2: i32, y2: i32) -> bool;
    /// `EEGetTileLine(x1, y1, x2, y2, &out, ends, thin)` (the C++ leaf with its CBTL record)
    fn tile_line(&mut self, x1: i32, y1: i32, x2: i32, y2: i32, ends: bool, thin: bool) -> Vec<Tile>;
    /// `group->IsReserved(point)`
    fn reserved(&mut self, group: u64, point: [f32; 3]) -> bool;
    /// `gWorld.mMap->GetGroundPoint(point, out)`, `GetTerrainPitch` / `GetTerrainRoll(transformation)`
    fn ground_point(&mut self, point: [f32; 3]) -> [f32; 3];
    fn terrain_tilt(&mut self, m: &GeTransformation, roll: bool) -> f32;
    /// the player's `mEnemies[id] != 0`, `IsHuman()`
    fn enemy(&mut self, player: u64, other_id: i32) -> bool;
    fn is_human(&mut self, player: u64) -> bool;
    /// `gUnitSetManager.Contains(unit_type, set)`
    fn unit_set_contains(&mut self, unit_type: u32, set: i32) -> bool;
    /// `gWorld.mPlayers[index]`'s unit list (`mFirstUnit`, `mNext`)
    fn player_units(&mut self, index: i32) -> Vec<u64>;
    /// a complex unit's `mPreviousPart` (`next` false) or `mNextPart`
    fn part(&mut self, complex: u64, next: bool) -> u64;
    /// `EEGetFacingQuarter(x, y)`
    fn facing_quarter(&mut self, x: i32, y: i32) -> i32;
    /// `gWorld.GetResourceUsers(unit)`
    fn resource_users(&mut self, unit: u64) -> i32;
    /// `gPhysics->GetRandom(min, max)` (the `long` and the `float` forms)
    fn random_long(&mut self, min: i32, max: i32) -> i32;
    fn random_float(&mut self, min: f32, max: f32) -> f32;
    /// a complex unit's `mPhysicalModel.GetLocation()`; `SetAnimation(animation, -1, 0)`; `SetTransformation(m,
    /// true)`
    fn location(&mut self, complex: u64) -> GeTransformation;
    fn set_animation(&mut self, complex: u64, animation: i32);
    fn set_transformation(&mut self, complex: u64, m: &GeTransformation);
    /// `gWorld.mMap->IsTileFlat(x, y)`; `tile->IsVisibleTo(player)`
    fn is_tile_flat(&mut self, x: i32, y: i32) -> bool;
    fn tile_visible_to(&mut self, tile: u64, player: u64) -> bool;
    /// the player's selection (`mActiveGroup`): `GetNumUnits()`, `GetContinent()`; `GetCitizenPassability()`,
    /// `mSaveVisMap`
    fn group_units(&mut self, player: u64) -> i32;
    fn group_continent(&mut self, player: u64) -> u16;
    fn citizen_passability(&mut self, player: u64) -> i32;
    fn saves_visibility(&mut self, player: u64) -> bool;
    /// a continent's `GetCategory()`; `gContinentManager.AreConnectedByID(a, b, flags)`
    fn continent_category(&mut self, continent: u64) -> u8;
    fn are_connected_by_id(&mut self, a: u16, b: u16, flags: i32) -> bool;
    /// the map's unit regions (ee_combat's table): its region size, the units in region (x, y) (0 off the table)
    fn regions_size(&mut self) -> i32;
    fn regions_len(&mut self, x: i32, y: i32) -> i32;
    /// a complex unit's goal's `mKind` (-1: none), `mStats->GetRange()`
    fn goal_kind(&mut self, unit: u64) -> i32;
    fn stats_range(&mut self, unit: u64) -> f32;
    /// `complex->AddCenterOffset(x, y)`
    fn center_offset(&mut self, complex: u64, x: f32, y: f32) -> (f32, f32);
    /// the terrain mesh's `GetIsValidTile` (`which` 0) / `GetIsSpaceTile` (1) at an unsigned tile
    fn mesh_tile_is(&mut self, which: i32, x: u32, y: u32) -> bool;
    /// `gContinentManager.IsShoreTile(x, y)`, `GetContinent(U2DPoint(x, y), kind)`, the complex unit's
    /// `GetFootprintContinent(kind)`, `InSameArea(tiles, kind)`, `HasShoreTile(tiles)`
    fn shore_tile(&mut self, x: i32, y: i32) -> bool;
    fn continent_of(&mut self, x: i32, y: i32, kind: i32) -> u16;
    fn footprint_continent(&mut self, complex: u64, kind: i32) -> u16;
    fn in_same_area(&mut self, tiles: &[Tile], kind: i32) -> bool;
    fn has_shore_tile(&mut self, tiles: &[Tile]) -> bool;
    /// where a unit dropped onto the target stands: a dead complex unit's model top point (scaled, in its frame),
    /// else `GetPosition`
    fn drop_point(&mut self, target: u64) -> [f32; 3];
    /// `piece->mUnitClass->IsPartLeftOut(piece)`
    fn part_left_out(&mut self, piece: u64) -> bool;
    /// `gContinentManager.GetPathAreaAt(U2DPoint(x, y))`'s id (0: none)
    fn path_area_id(&mut self, x: i32, y: i32) -> u16;
    /// a gate's two ends: the positions of its outermost carrier's last linked carrier and last linked piece
    fn gate_ends(&mut self, gate: u64) -> [[f32; 3]; 2];
}

/// `RTSMap::GetTile` of the world's map.
fn tile<H: UuHost + ?Sized>(h: &mut H, x: i32, y: i32) -> u64 {
    let map = h.map();
    h.map_tile(map, x, y)
}

fn units_of(h: &mut (impl UuHost + ?Sized), tile: u64) -> Vec<u64> {
    let mut units = Vec::new();
    h.tile_units(tile, &mut units);
    units
}

// ---- the rings ----

/// `EEIsCornerBlocked` (0x246edc): whether both tiles hold a wall piece (each list read until its first wall, or a
/// node without a unit).
pub fn is_corner_blocked<H: UuHost + ?Sized>(h: &mut H, x1: i32, y1: i32, x2: i32, y2: i32) -> bool {
    let first_tile = tile(h, x1, y1);
    let second_tile = tile(h, x2, y2);
    if first_tile == 0 || second_tile == 0 {
        return false;
    }
    let scan = |h: &mut H, t: u64| {
        let mut wall = false;
        for u in units_of(h, t) {
            if u == 0 || wall {
                break;
            }
            wall = UuView(h.unit(u)).class().is_wall();
        }
        wall
    };
    let first = scan(h, first_tile);
    let second = scan(h, second_tile);
    first && second
}

/// The four corners of the `EEGetBlockRing` / `EEGetRingCorners` families: each unless closed by two wall pieces
/// (`(char)corners` keeps them all).
fn ring_corner_tiles<H: UuHost + ?Sized>(
    h: &mut H,
    corners: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    out: &mut Vec<Tile>,
) {
    let all = corners as u8 != 0;
    let bottom = y.wrapping_add(height);
    let right = x.wrapping_add(width);
    if all || !is_corner_blocked(h, x - 1, y, x, y - 1) {
        out.push([x - 1, y - 1]);
    }
    if all || !is_corner_blocked(h, x - 1, bottom - 1, x, height + 1) {
        out.push([x - 1, bottom]);
    }
    if all || !is_corner_blocked(h, right, y, right - 1, y - 1) {
        out.push([right, y - 1]);
    }
    if all || !is_corner_blocked(h, right - 1, bottom, right, bottom - 1) {
        out.push([right, bottom]);
    }
}

/// `EEGetRingCorners` (0x247021) / `EEGetCornerTiles` (the square's).
pub fn ring_corners<H: UuHost + ?Sized>(h: &mut H, corners: i32, x: i32, y: i32, width: i32, height: i32) -> Vec<Tile> {
    let mut out = Vec::new();
    ring_corner_tiles(h, corners, x, y, width, height, &mut out);
    out
}

/// `EEGetBlockRing` (0x246a42): the sides of the rectangle (per row its left and right tile, per column its top and
/// bottom one), then its open corners; appended to `out` when `append`.
#[allow(clippy::too_many_arguments)]
pub fn block_ring<H: UuHost + ?Sized>(
    h: &mut H,
    corners: i32,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    out: &mut Vec<Tile>,
    append: bool,
) {
    if !append {
        out.clear();
    }
    let bottom = height.wrapping_add(y);
    let mut ty = y;
    while ty < bottom {
        out.push([x - 1, ty]);
        out.push([width + x, ty]);
        ty += 1;
    }
    let right = width.wrapping_add(x);
    let mut tx = x;
    while tx < right {
        out.push([tx, y - 1]);
        out.push([tx, bottom]);
        tx += 1;
    }
    ring_corner_tiles(h, corners, x, y, width, height, out);
}

/// `EERemoveEdgeTiles` (0x24ba75): drops the tiles off the map or on its edge.
pub fn remove_edge_tiles<H: UuHost + ?Sized>(h: &mut H, tiles: &mut Vec<Tile>) {
    let map = h.map();
    let mut i = 0;
    while i < tiles.len() {
        let [x, y] = tiles[i];
        if h.map_tile(map, x, y) == 0 || h.is_edge_tile(x, y) {
            tiles.remove(i);
        } else {
            i += 1;
        }
    }
}

/// The sides then the open corners of ring `ring` around the `size` square at (`x`, `y`).
fn ring_sides_and_corners<H: UuHost + ?Sized>(
    h: &mut H,
    x: i32,
    y: i32,
    size: i32,
    ring: i32,
    corners: i32,
) -> Vec<Tile> {
    let side = size + ring * 2 - 2;
    let mut tiles = square_ring(x - ring + 1, y - ring + 1, side, side);
    tiles.extend(ring_corners(h, corners, x - ring + 1, y - ring + 1, side, side));
    tiles
}

/// `EEGetRingTiles` (0x24bb36): ring `ring` around the `size` square at (`x`, `y`) (a two- or one-tile square's in
/// the C++'s fixed orders, the one-tile square's with its centre first), off-map and edge tiles dropped; ring 0 is
/// the tile itself.
pub fn ring_tiles<H: UuHost + ?Sized>(h: &mut H, x: i32, y: i32, size: i32, ring: i32, corners: i32) -> Vec<Tile> {
    let mut out = Vec::new();
    if ring > 0 {
        let mut tiles = ring_sides_and_corners(h, x, y, size, ring, corners);
        if tiles.len() == 12 {
            out.extend(RING_ORDER_12.iter().map(|&i| tiles[i]));
        } else if tiles.len() == 8 {
            tiles.push([x, y]);
            out.extend(RING_ORDER_9.iter().map(|&i| tiles[i]));
        } else {
            out = tiles;
        }
    } else {
        out.push([x, y]);
    }
    remove_edge_tiles(h, &mut out);
    out
}

/// `EEGetSortedRingTiles` (0x241299): ring `range` around the square, sorted (MSVC's `std::sort`) by the squared
/// distance from the point's truncated tile; range 0 is the tile itself.
#[allow(clippy::too_many_arguments)]
pub fn sorted_ring_tiles<H: UuHost + ?Sized>(
    h: &mut H,
    from: [f32; 3],
    x: i32,
    y: i32,
    size: i32,
    range: i32,
    corners: i32,
) -> Vec<Tile> {
    if range <= 0 {
        return vec![[x, y]];
    }
    let mut out = ring_sides_and_corners(h, x, y, size, range, corners);
    let center = [crate::uu_tiles::trunc_long(from[0]), crate::uu_tiles::trunc_long(from[1])];
    let distance = |t: &Tile| {
        let dy = center[1].wrapping_sub(t[1]);
        let dx = center[0].wrapping_sub(t[0]);
        dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
    };
    crate::msvc_sort::sort_by(&mut out, |a, b| distance(a) < distance(b));
    out
}

// ---- the units on tiles ----

/// `EEGetUnitsOn` (0x24c433): appends the units of the tiles' lists (in tile order, node order); their number.
pub fn units_on<H: UuHost + ?Sized>(h: &mut H, tiles: &[Tile], out: &mut Vec<u64>) -> i32 {
    let map = h.map();
    let mut count = 0;
    if map != 0 {
        for &[x, y] in tiles {
            let t = h.map_tile(map, x, y);
            if t == 0 {
                continue;
            }
            for u in units_of(h, t) {
                if u != 0 {
                    out.push(u);
                    count += 1;
                }
            }
        }
    }
    count
}

/// `EEGetUnitsOnTiles` (0x24b7e9): appends the units of the tiles' lists, the tiles from the last; when a unit
/// bigger than a tile was among them, the whole list sorted by descending id and its repeats dropped. Its length.
pub fn units_on_tiles<H: UuHost + ?Sized>(h: &mut H, tiles: &[Tile], out: &mut Vec<u64>) -> i32 {
    let map = h.map();
    if map != 0 {
        let mut large = false;
        for &[x, y] in tiles.iter().rev() {
            let t = h.map_tile(map, x, y);
            if t == 0 {
                continue;
            }
            for u in units_of(h, t) {
                if large || UuView(h.unit(u)).class().tile_size() > 1 {
                    large = true;
                }
                out.push(u);
            }
        }
        if large {
            sort_unique_by_id_descending(h, out);
        }
    }
    out.len() as i32
}

/// `EESortUnique(units, EEUUUnitIDGreater, EEUUUnitIDSame)`: by descending id, one unit per id (equal ids are one
/// unit, so the sort's order of ties does not show).
fn sort_unique_by_id_descending<H: UuHost + ?Sized>(h: &mut H, units: &mut Vec<u64>) {
    let mut keyed: Vec<(u32, u64)> = units.iter().map(|&u| (UuView(h.unit(u)).id(), u)).collect();
    crate::msvc_sort::sort_by(&mut keyed, |a, b| a.0 > b.0);
    keyed.dedup_by(|b, a| a.0 == b.0);
    *units = keyed.into_iter().map(|(_, u)| u).collect();
}

/// The tile-list walks of `EEGetUnitsOfTypeOnTiles` (0x24c54a; `player` given) and `EEGetWallUnitsOfTypeOnTiles`
/// (0x24c627): appends the units of a type (and player); their number.
pub fn units_of_type_on_tiles<H: UuHost + ?Sized>(
    h: &mut H,
    tiles: &[Tile],
    player: Option<u64>,
    unit_type: i32,
    out: &mut Vec<u64>,
) -> i32 {
    let map = h.map();
    let mut count = 0;
    if map != 0 {
        for &[x, y] in tiles {
            let t = h.map_tile(map, x, y);
            if t == 0 {
                continue;
            }
            for u in units_of(h, t) {
                if u == 0 {
                    continue;
                }
                let unit = UuView(h.unit(u));
                if unit.class().unit_type() == unit_type as u32 && player.is_none_or(|p| unit.0.player == p) {
                    out.push(u);
                    count += 1;
                }
            }
        }
    }
    count
}

/// `EEGetTileUnitOfType` (0x24c312; `player` none) and `EEGetWallAt` (0x24c4be): the tile's first unit of the type
/// (and player) that is alive or a tree, else 0.
pub fn tile_unit_of_type<H: UuHost + ?Sized>(h: &mut H, at: Tile, unit_type: u32, player: Option<u64>) -> u64 {
    let map = h.map();
    if map == 0 {
        return 0;
    }
    let t = h.map_tile(map, at[0], at[1]);
    if t == 0 {
        return 0;
    }
    for u in units_of(h, t) {
        if u == 0 {
            continue;
        }
        let unit = UuView(h.unit(u));
        let cls = unit.class();
        if (unit.hit_points() <= 0 && !cls.is_tree())
            || player.is_some_and(|p| unit.0.player != p)
            || cls.unit_type() != unit_type
        {
            continue;
        }
        return u;
    }
    0
}

/// `EEGetPlayerUnitsOfType` (0x24c395): appends player `index`'s units of the type (its list's order); their number.
pub fn player_units_of_type<H: UuHost + ?Sized>(h: &mut H, unit_type: i32, index: i32, out: &mut Vec<u64>) -> i32 {
    let mut count = 0;
    for u in h.player_units(index) {
        if UuView(h.unit(u)).class().unit_type() == unit_type as u32 {
            out.push(u);
            count += 1;
        }
    }
    count
}

/// `EEGetFootprintTiles` (0x24b5b3): a gate's pieces' tiles (appended, from the first piece), else (the list
/// cleared) the unit's square, an airport's four tiles long the way its quarter faces. The coordinates are the C++'s
/// `unsigned long`s.
pub fn footprint_tiles<H: UuHost + ?Sized>(h: &mut H, obj: u64, out: &mut Vec<Tile>) {
    let unit = UuView(h.unit(obj));
    let cls = unit.class();
    let y = unit.tile_y() as u32;
    let mut size = cls.tile_size() as u32;
    let x = unit.tile_x() as u32;
    if cls.category() == UU_CATEGORY_GATE {
        let mut piece = unit.0.complex;
        loop {
            let prev = h.part(piece, false);
            if prev == 0 {
                break;
            }
            piece = prev;
        }
        loop {
            let p = UuView(h.unit(piece));
            out.push([p.tile_x(), p.tile_y()]);
            let next = h.part(piece, true);
            if next == 0 {
                break;
            }
            piece = next;
        }
        return;
    }
    let mut height = size;
    let mut start_x = x;
    let mut start_y = y;
    if cls.unit_type() == UU_TYPE_AIRPORT {
        match h.facing_quarter(x as i32, y as i32) {
            0 => height = size.wrapping_mul(4),
            1 => start_y = y.wrapping_sub(size.wrapping_mul(3)),
            2 => size = size.wrapping_mul(4),
            _ => start_x = x.wrapping_sub(size.wrapping_mul(3)),
        }
    }
    out.clear();
    let mut tx = start_x;
    while tx < x.wrapping_add(size) {
        let mut ty = start_y;
        while ty < height.wrapping_add(y) {
            out.push([tx as i32, ty as i32]);
            ty = ty.wrapping_add(1);
        }
        tx = tx.wrapping_add(1);
    }
}

/// `EEGetResourceSpots` (0x24c6fc): appends the tiles around the unit (the ring's corners first, then its sides) that
/// are empty or hold only citizens and units the unit's complex unit may pass that are not foundations.
pub fn resource_spots<H: UuHost + ?Sized>(h: &mut H, obj: u64, out: &mut Vec<Tile>) {
    let map = h.map();
    if map == 0 {
        return;
    }
    let unit = UuView(h.unit(obj));
    let size = unit.class().tile_size() + 2;
    let (x, y) = (unit.tile_x() - 1, unit.tile_y() - 1);
    let mut tiles = ring_corners(h, 0, x, y, size, size);
    tiles.extend(square_ring(x, y, size, size));
    for t in tiles {
        let tile = h.map_tile(map, t[0], t[1]);
        if tile == 0 {
            continue;
        }
        let units = units_of(h, tile);
        let mut free = true;
        for &u in &units {
            if !free {
                break;
            }
            if u == 0 {
                continue;
            }
            let other = UuView(h.unit(u));
            let category = other.class().category();
            if category != UU_CATEGORY_LAND_CITIZEN
                && category != UU_CATEGORY_NAVAL_CITIZEN
                && !(h.passable_by(u, unit.0.complex) && !h.is_foundation(u))
            {
                free = false;
            }
        }
        if free {
            out.push(t);
        }
    }
}

/// `EEGetFarmDropSites` (0x245903): appends the unit's player's granaries on the corners and sides of its footprint
/// (and on it, for a granary); whether `out` holds any.
pub fn farm_drop_sites<H: UuHost + ?Sized>(h: &mut H, obj: u64, out: &mut Vec<u64>) -> bool {
    let unit = UuView(h.unit(obj));
    let cls = unit.class();
    let size = cls.tile_size();
    let (x, y) = (unit.tile_x(), unit.tile_y());
    let mut tiles = Vec::new();
    let mut dx = -size;
    while dx <= size {
        let mut dy = -size;
        while dy <= size {
            let t = cls.unit_type();
            if t == UU_TYPE_GRANARY_IMPROVED || t == UU_TYPE_GRANARY || dx != 0 || dy != 0 {
                tiles.push([x + dx, dy + y]);
            }
            dy += size;
        }
        dx += size;
    }
    let mut units = Vec::new();
    units_on(h, &tiles, &mut units);
    for u in units {
        let other = UuView(h.unit(u));
        let t = other.class().unit_type();
        if (t == UU_TYPE_GRANARY_IMPROVED || t == UU_TYPE_GRANARY) && other.0.player == unit.0.player {
            out.push(u);
        }
    }
    !out.is_empty()
}

/// `EEGetTownCenterLastRing` (0x24ba75's neighbour): the granary class's size and the last ring, max(size, 3)^2 - 1.
fn town_center_last_ring<H: UuHost + ?Sized>(h: &mut H) -> i32 {
    let size = UuClassView(h.class_of_type(UU_TYPE_GRANARY as i32).fields, 0).tile_size();
    let m = size.max(3);
    m * m - 1
}

/// `EEFindFreeFarm` (0x245a19): the unit's player's first farm (or robotic farm) on the slot tiles ring by ring from
/// `ring` (free: a built farm nobody works or an unbuilt one nobody builds, with `free_only`); the units of every
/// ring so far are read again each ring, as the C++'s list grows. The ring reached goes back.
pub fn find_free_farm<H: UuHost + ?Sized>(h: &mut H, obj: u64, free_only: bool, ring: &mut i32) -> u64 {
    let unit = UuView(h.unit(obj));
    let mut r = *ring;
    let last = town_center_last_ring(h);
    let from = [unit.tile_x(), unit.tile_y()];
    let mut units = Vec::new();
    let mut result = 0;
    let mut found = false;
    loop {
        if r > last {
            break;
        }
        let tiles = [crate::uu_tiles::slot_tile(from, r)];
        units_on(h, &tiles, &mut units);
        let mut i = 0;
        while !found && !units.is_empty() && i < units.len() {
            let other = UuView(h.unit(units[i]));
            i += 1;
            if !other.on_map() {
                continue;
            }
            let complex = UuView(h.unit(other.0.complex));
            let t = complex.class().unit_type();
            if (t != UU_TYPE_FARM && t != UU_TYPE_ROBOTIC_FARM)
                || complex.hit_points() <= 0
                || complex.0.player != unit.0.player
            {
                continue;
            }
            if !free_only
                || (t == UU_TYPE_FARM && complex.is_built() && h.resource_users(complex.obj()) == 0)
                || (!complex.is_built() && complex.num_builders() == 0)
            {
                found = true;
                result = complex.obj();
            }
        }
        r += 1;
        if found {
            break;
        }
    }
    *ring = r;
    result
}

// ---- the walls ----

/// `EEExtendWall` (0x24b6db): from `to`, the line's next tiles away from `from` while they hold the player's live
/// unit of the type, each inserted at the front of `tiles`.
pub fn extend_wall<H: UuHost + ?Sized>(
    h: &mut H,
    from: Tile,
    to: Tile,
    player: u64,
    unit_type: u32,
    tiles: &mut Vec<Tile>,
) {
    let dx = to[0] - from[0];
    let dy = to[1] - from[1];
    let mut t = [to[0] + dx, to[1] + dy];
    while tile_unit_of_type(h, t, unit_type, Some(player)) != 0 {
        tiles.insert(0, t);
        t = [t[0] + dx, t[1] + dy];
    }
}

/// `EEGetWallLine` (0x24b794): the tile line between two tiles (ends included, thin), its diagonal corners and
/// repeats dropped; the tile itself when both are one.
pub fn wall_line<H: UuHost + ?Sized>(h: &mut H, from: Tile, to: Tile) -> Vec<Tile> {
    if from == to {
        return vec![from];
    }
    let mut out = h.tile_line(from[0], from[1], to[0], to[1], true, true);
    crate::uu_tiles::drop_diagonal_corners(&mut out);
    crate::uu_tiles::drop_repeated_tiles(&mut out);
    out
}

/// `EEExtendWallLine` (0x247bef): the wall line through `tiles` joined to the player's walls of the type at its ends
/// (a single tile: to the one or two such walls around it), into `out`; the number of tiles extended before it.
pub fn extend_wall_line<H: UuHost + ?Sized>(
    h: &mut H,
    tiles: &[Tile],
    player: u64,
    unit_type: u32,
    out: &mut Vec<Tile>,
) -> i32 {
    let n = tiles.len();
    if n == 0 {
        // (the C++ reads the first tile of an empty list)
        return 0;
    }
    if n >= 2 {
        extend_wall(h, tiles[1], tiles[0], player, unit_type, out);
        let result = out.len() as i32;
        out.extend_from_slice(tiles);
        let mut line = Vec::new();
        extend_wall(h, tiles[n - 2], tiles[n - 1], player, unit_type, &mut line);
        out.extend(line.iter().rev());
        return result;
    }
    let [x, y] = tiles[0];
    let mut around = ring_corners(h, 0, x, y, 1, 1);
    around.extend(square_ring(x, y, 1, 1));
    let mut units = Vec::new();
    units_of_type_on_tiles(h, &around, Some(player), unit_type as i32, &mut units);
    let tile_of = |h: &mut H, u: u64| {
        let unit = UuView(h.unit(u));
        [unit.tile_x(), unit.tile_y()]
    };
    if units.len() == 2 {
        let to = tile_of(h, units[0]);
        extend_wall(h, tiles[0], to, player, unit_type, out);
        out.push(to);
        out.push(tiles[0]);
        let other = tile_of(h, units[1]);
        out.push(other);
        let mut line = Vec::new();
        extend_wall(h, tiles[0], other, player, unit_type, &mut line);
        out.extend(line.iter().rev());
    } else if units.len() == 1 {
        let to = tile_of(h, units[0]);
        extend_wall(h, tiles[0], to, player, unit_type, out);
        out.push(to);
        out.push(tiles[0]);
    } else {
        out.extend_from_slice(tiles);
    }
    0
}

/// `EEPickWallPieces` (0x24cca5): each wall tile's piece (0: flat) from the step to the next tile and the heights
/// (the tile's, its raised corners, the terrain corners), as the C++'s run states pick them.
pub fn pick_wall_pieces<H: UuHost + ?Sized>(h: &mut H, map: u64, tiles: &[Tile]) -> Vec<i32> {
    let count = tiles.len();
    let mut out = vec![0; count];
    let mut prev_height = 0.0f32;
    let mut prev_raised = 0;
    let mut run = 0;
    let mut prev_x = 0;
    let mut prev_y = 0;
    for i in 1..count {
        let [x, y] = tiles[i];
        let mut dx = tiles[i - 1][0] - x;
        let x_up = dx >= 0;
        if !x_up {
            dx = -dx;
        }
        let mut dy = tiles[i - 1][1] - y;
        let y_up = dy >= 0;
        if !y_up {
            dy = -dy;
        }
        let t = h.map_tile(map, x, y);
        let raised = h.raised_corners(map, x, y);
        let height = h.tile_height(t);
        let z = |h: &mut H, zx: i32, zy: i32| h.mesh_z(map, zx, zy);
        let o = i - 1;
        if dx == 0 && dy == 1 {
            if run == 2 {
                out[o] = if prev_height - height >= 0.4 {
                    if prev_raised == 2 {
                        if y_up {
                            5
                        } else {
                            7
                        }
                    } else {
                        0
                    }
                } else if height - prev_height >= 0.4 {
                    if prev_raised == 2 {
                        if y_up {
                            7
                        } else {
                            5
                        }
                    } else {
                        0
                    }
                } else if prev_raised == 2 {
                    let flat = (z(h, prev_x, prev_y) - z(h, x, y)).abs() < 0.4
                        && (z(h, prev_x + 1, prev_y) - z(h, x + 1, y)).abs() < 0.4
                        && (z(h, prev_x, prev_y + 1) - z(h, x, y + 1)).abs() < 0.4
                        && (z(h, prev_x + 1, prev_y + 1) - z(h, x + 1, y + 1)).abs() < 0.4;
                    if flat {
                        2
                    } else if raised != 1
                        || (z(h, prev_x, prev_y) <= z(h, prev_x + 1, prev_y)
                            && z(h, prev_x + 1, prev_y) <= z(h, prev_x, prev_y))
                    {
                        if z(h, prev_x + 1, prev_y + 1) < z(h, prev_x, prev_y) {
                            7
                        } else {
                            5
                        }
                    } else {
                        2
                    }
                } else if prev_raised == 1 {
                    0
                } else {
                    2
                };
            } else {
                run = 2;
            }
        } else if dy == 0 {
            if dx == 1 {
                if run == 4 {
                    out[o] = if prev_height - height >= 0.4 {
                        if prev_raised == 2 {
                            if x_up {
                                8
                            } else {
                                6
                            }
                        } else {
                            0
                        }
                    } else if height - prev_height >= 0.4 {
                        if prev_raised == 2 {
                            if x_up {
                                6
                            } else {
                                8
                            }
                        } else {
                            0
                        }
                    } else if prev_raised == 2 {
                        let flat = (z(h, prev_x, prev_y) - z(h, x, y)).abs() < 0.4
                            && (z(h, prev_x + 1, prev_y) - z(h, x + 1, y)).abs() < 0.4
                            && (z(h, prev_x, prev_y + 1) - z(h, x, y + 1)).abs() < 0.4
                            && (z(h, prev_x + 1, prev_y + 1) - z(h, x + 1, y + 1)).abs() < 0.4;
                        if flat {
                            1
                        } else if raised != 1
                            || (z(h, prev_x, prev_y) <= z(h, prev_x, prev_y + 1)
                                && z(h, prev_x, prev_y + 1) <= z(h, prev_x, prev_y))
                        {
                            if z(h, prev_x + 1, prev_y + 1) < z(h, prev_x, prev_y) {
                                6
                            } else {
                                8
                            }
                        } else {
                            1
                        }
                    } else if prev_raised == 1 {
                        0
                    } else {
                        1
                    };
                }
                run = 4;
            }
        } else if dx == 1 && dy == 1 {
            if y_up == x_up {
                if run == 3 {
                    out[o] = if (prev_height - height).abs() < 0.4 {
                        if prev_raised == 2 {
                            if z(h, prev_x + 1, prev_y + 1) < z(h, prev_x, prev_y) {
                                12
                            } else {
                                10
                            }
                        } else {
                            4
                        }
                    } else if prev_raised != 2 {
                        0
                    } else if z(h, prev_x, prev_y) <= z(h, prev_x + 1, prev_y + 1) {
                        10
                    } else {
                        12
                    };
                }
                run = 3;
            } else if run == 1 {
                out[o] = if (prev_height - height).abs() < 0.4 {
                    if prev_raised == 2 {
                        if z(h, prev_x + 1, prev_y + 1) < z(h, prev_x, prev_y) {
                            9
                        } else {
                            11
                        }
                    } else {
                        3
                    }
                } else if prev_raised != 2 {
                    0
                } else if z(h, prev_x, prev_y) <= z(h, prev_x + 1, prev_y + 1) {
                    11
                } else {
                    9
                };
            } else {
                run = 1;
            }
        }
        prev_raised = raised;
        prev_height = height;
        prev_y = y;
        prev_x = x;
    }
    out
}

/// `EEMakeWallJunctions` (0x243ae1): each wall piece with three or more neighbours (the player's live wall pieces on
/// the eight tiles around it, and the placed tiles within one tile, the C++'s `unsigned long` differences) becomes the
/// plain junction piece (frame set 11) facing south.
pub fn make_wall_junctions<H: UuHost + ?Sized>(h: &mut H, units: &[u64], placed: &[Tile], player: u64, map: u64) {
    for &u in units {
        let unit = UuView(h.unit(u));
        let x = unit.tile_x() as u32;
        let y = unit.tile_y() as u32;
        let mut count = 0u32;
        let mut nx = x.wrapping_sub(1);
        while nx <= x.wrapping_add(1) {
            let mut ny = y.wrapping_sub(1);
            while ny <= y.wrapping_add(1) {
                if !(nx == x && ny == y) {
                    let t = h.map_tile(map, nx as i32, ny as i32);
                    if t != 0 {
                        for other in units_of(h, t) {
                            if other == 0 {
                                continue;
                            }
                            let o = UuView(h.unit(other));
                            if o.0.player == player && o.hit_points() > 0 && o.class().is_wall() {
                                count += 1;
                            }
                        }
                    }
                }
                ny = ny.wrapping_add(1);
            }
            nx = nx.wrapping_add(1);
        }
        for t in placed.iter().rev() {
            let dx = (t[0] as u32).wrapping_sub(x) as f32;
            let dy = (t[1] as u32).wrapping_sub(y) as f32;
            if dx.abs() <= 1.0 && dy.abs() <= 1.0 {
                count += 1;
            }
        }
        if count < 3 {
            continue;
        }
        h.set_animation(u, 0xb);
        let mut m = h.location(u);
        m.set_orientation(RotationOrder::Ypr, 0.0, 0.0, 0.0);
        h.set_transformation(u, &m);
    }
}

// ---- the standing of units on a site ----

/// `EEGetTileUnitStanding` (0x243978): how a placement of `unit_type` for `player` sees a unit on its tile: 0 not at
/// all, 1 a unit that can make way, 2 a blocker.
pub fn tile_unit_standing<H: UuHost + ?Sized>(
    h: &mut H,
    obj: u64,
    player: u64,
    unit_type: i32,
    ignore_owner: bool,
) -> i32 {
    let placed = UuClassView(h.class_of_type(unit_type).fields, 0);
    let unit = UuView(h.unit(obj));
    let owner = unit.0.player;
    let cls = unit.class();
    let t = cls.unit_type();
    if cls.is_decoration() || t == UU_TYPE_WAYPOINT || t == UU_TYPE_PATH_POINT {
        return 0;
    }
    if unit.hit_points() == 0 && cls.category() != UU_CATEGORY_RESOURCE {
        return 0;
    }
    let complex = unit.0.complex != 0;
    if cls.can_fly() && complex && unit.0.location[2] > h.tile_height(unit.0.tile) {
        return 0;
    }
    if unit.land_building()
        && complex
        && h.is_foundation(unit.0.complex)
        && player != 0
        && h.is_human(player)
        && owner != player
    {
        return 0;
    }
    if h.unit_set_contains(t, placed.overlap_unit_set()) {
        return 0;
    }
    if player == 0 {
        return 2;
    }
    let owner_id = unit.0.player_id;
    if (ignore_owner || (owner != player && h.enemy(player, owner_id))) && complex && unit.physics_action() == 0 {
        return 1;
    }
    if (ignore_owner || owner == player || !h.enemy(player, owner_id)) && complex && unit.0.stats_max_velocity > 0.0 {
        return 1;
    }
    if cls.family() == UU_FAMILY_ANIMAL && complex && unit.0.stats_attack == 0.0 {
        return 1;
    }
    2
}

/// `EEGetBlockingUnits` (0x23e530; `standing` 2) and `EEGetMovableTileUnits` (0x23e598; 1): the tile's units of that
/// standing (`out` cleared); whether there are any. A tile off the map holds none.
pub fn tile_units_standing<H: UuHost + ?Sized>(
    h: &mut H,
    player: u64,
    unit_type: i32,
    tile: u64,
    ignore_owner: bool,
    standing: i32,
    out: &mut Vec<u64>,
) -> bool {
    out.clear();
    if tile == 0 {
        return false;
    }
    for u in units_of(h, tile) {
        if u != 0 && tile_unit_standing(h, u, player, unit_type, ignore_owner) == standing {
            out.push(u);
        }
    }
    !out.is_empty()
}

/// `EEIsTileUnblocked` (0x24f86d): not space, and no unit but trees blocks the class there.
pub fn is_tile_unblocked<H: UuHost + ?Sized>(h: &mut H, player: u64, unit_class: i32, tile: u64) -> bool {
    if h.tile_terrain(tile) == UU_TERRAIN_SPACE {
        return false;
    }
    let mut count = 0;
    for u in units_of(h, tile) {
        if u != 0 && !UuView(h.unit(u)).class().is_tree() && tile_unit_standing(h, u, player, unit_class, true) == 2 {
            count += 1;
        }
    }
    count == 0
}

// ---- standing and placing ----

/// `EECanStandOn` (0x241658): whether the complex unit may stand on the tile: an air unit that does not fly over
/// may anywhere; else the tile on the map, off its edge, of a terrain its class moves on; then a flyer or a space
/// unit may; a sea unit needs the tile's water side; and no unit on the tile may block it.
pub fn can_stand_on<H: UuHost + ?Sized>(h: &mut H, at: Tile, obj: u64) -> bool {
    let unit = UuView(h.unit(obj));
    let cls = unit.class();
    let t = tile(h, at[0], at[1]);
    if cls.domains() & UU_MOVEMENT_AIR != 0 && unit.0.flies_over == 0 {
        return true;
    }
    if t == 0 || h.is_edge_tile(at[0], at[1]) {
        return false;
    }
    let terrain = h.tile_terrain(t);
    if !h.can_move_on_terrain(unit.0.class_obj, obj, terrain) {
        return false;
    }
    if unit.0.flies_over != 0 || cls.domains() & UU_MOVEMENT_SPACE != 0 {
        return true;
    }
    if cls.domains() & UU_MOVEMENT_SEA != 0 {
        let flag = h.continent_tile_flag(at[0], at[1]);
        if flag != 0 {
            let ok = if flag < 0 { unit.water_state() == 0 } else { unit.water_state() != 3 };
            if !ok {
                return false;
            }
        }
    }
    for u in units_of(h, t) {
        if !h.passable_by(u, obj) {
            return false;
        }
    }
    true
}

/// `EEFindPlaceOnTile` (0x240ddb): a free spot for a unit (`obj`, or one of `class`) on tile (`to_x`, `to_y`), the
/// spots of [`sub_tile_steps`] tried in turn (entering from (`x`, `y`)); a spot is free of the tile's living units
/// (but farms and buildings) by the two collision spheres, of the units that do not let it pass, and of the unit's
/// active group's reserved spots. The spot found (or the last tried) goes to `out`'s x and y.
#[allow(clippy::too_many_arguments)]
pub fn find_place_on_tile<H: UuHost + ?Sized>(
    h: &mut H,
    obj: u64,
    class: u64,
    x: i32,
    y: i32,
    size: i32,
    to_x: i32,
    to_y: i32,
    square: i32,
    out: &mut [f32; 3],
) -> bool {
    let me = if obj != 0 { Some(UuView(h.unit(obj))) } else { None };
    let class_obj = if class != 0 { class } else { me.map_or(0, |m| m.0.class_obj) };
    let cls = UuClassView(h.class(class_obj).fields, class_obj);
    let own_size = cls.inner();
    let mut clamped = own_size;
    if 1.0 < clamped {
        clamped = 1.0;
    }
    let mut units = Vec::new();
    units_on(h, &[[to_x, to_y]], &mut units);
    let steps = sub_tile_steps(clamped, size, to_x - x, to_y - y, square != 0);
    let mut sx = steps.start_x;
    let mut sy = steps.start_y;
    let mut found = false;
    let mut i = 0;
    while steps.count > i && !found {
        found = true;
        out[0] = (to_x as u32) as f32 + sx;
        out[1] = (to_y as u32) as f32 + sy;
        let mut j = 0;
        while j < units.len() && found {
            let u = units[j];
            j += 1;
            let other = UuView(h.unit(u));
            let oc = other.class();
            if oc.unit_type() == UU_TYPE_FARM || oc.unit_type() == UU_TYPE_ROBOTIC_FARM {
                continue;
            }
            if other.hit_points() > 0
                && !other.land_building()
                && oc.inner() < 1.5
                && obj != u
                && (oc.blocks_tile() || oc.is_collidable())
            {
                let p = h.position(u);
                let mut other_size = oc.inner();
                if oc.is_tree() {
                    let moves = (obj != 0 && h.can_move_on_terrain(me.map_or(0, |m| m.0.class_obj), obj, 0xe))
                        || (class_obj != 0 && cls.terrain(0xe));
                    other_size = if moves { 0.0 } else { 1.0 };
                }
                let distance = Ge2DPoint { x: out[0], y: out[1] }.distance_to_squared(Ge2DPoint { x: p[0], y: p[1] });
                let reach = (other_size + own_size) * 0.5;
                found = !(reach * reach > 0.1 * own_size + distance);
            } else {
                found = !oc.is_tree()
                    && (obj == u
                        || (h.passable_by(u, obj)
                            && (!other.on_map() || !h.is_foundation(other.0.complex))
                            && oc.unit_type() != UU_TYPE_AIRPORT_RUNWAY));
            }
        }
        if found {
            if let Some(m) = me {
                if m.0.active_group != 0 && h.reserved(m.0.active_group, *out) {
                    found = false;
                }
            }
        }
        i += 1;
        if found {
            continue;
        }
        if steps.step_y + sy > 1.0 {
            sy = steps.start_y;
            sx += steps.step_y;
        } else if steps.step_x + sx > 1.0 {
            sx = steps.start_x;
            sy += steps.step_x;
        } else {
            sx += steps.step_x;
            sy += steps.step_y;
        }
        while sx > 1.0 {
            sx -= 1.0;
        }
        while sx < 0.0 {
            sx += 1.0;
        }
        while sy > 1.0 {
            sy -= 1.0;
        }
        while sy < 0.0 {
            sy += 1.0;
        }
    }
    found
}

/// `EEGetTilesAround` (0x24119a): the target's surrounding tiles (its class's, through the vtable when
/// `virtual_call`), or the square ring of `range` around (`x`, `y`), sorted nearer to the unit first (MSVC's
/// `std::sort` by `DistanceSquaredToTile`); the centre tile first for a small or passable target that is no tree
/// nor farm, and with no target.
#[allow(clippy::too_many_arguments)]
pub fn tiles_around<H: UuHost + ?Sized>(
    h: &mut H,
    obj: u64,
    target: u64,
    x: i32,
    y: i32,
    size: i32,
    range: i32,
    corners: i32,
    virtual_call: bool,
) -> Vec<Tile> {
    let mut out = if target != 0 {
        h.surrounding_tiles(target, virtual_call)
    } else {
        let mut ring = Vec::new();
        block_ring(
            h,
            corners,
            x - range + 1,
            y - range + 1,
            size + range * 2 - 2,
            size + range * 2 - 2,
            &mut ring,
            false,
        );
        ring
    };
    let me = UuView(h.unit(obj));
    let complex = UuView(h.unit(me.0.complex));
    let location = complex.0.location;
    let distance = |t: &Tile| distance_squared_to_tile(location, *t);
    crate::msvc_sort::sort_by(&mut out, |a, b| distance(a) < distance(b));
    let first = if target == 0 {
        true
    } else {
        let tc = UuView(h.unit(target)).class();
        (!tc.is_collidable() || tc.inner() < 0.6) && !tc.is_tree() && tc.unit_type() != UU_TYPE_FARM
    };
    if first {
        out.insert(0, [x, y]);
    }
    out
}

/// `EEComplexUnit::DistanceSquaredToTile(tile)`: the squared ground distance from the unit's location to the tile's
/// corner (its coordinates as floats; the x term first).
pub fn distance_squared_to_tile(location: [f32; 3], t: Tile) -> f32 {
    (location[0] - t[0] as f32) * (location[0] - t[0] as f32)
        + (location[1] - t[1] as f32) * (location[1] - t[1] as f32)
}

/// `EEPickTileNear` (0x241504): of `tiles` (in order), the first the unit can stand on, on a continent connected to
/// the container's (or linked for a sea unit), with a free spot entering from the container
/// ([`find_place_on_tile`]) and, unless `ignore_blocked_line`, no wall on the tile line back to it.
pub fn pick_tile_near<H: UuHost + ?Sized>(
    h: &mut H,
    tiles: &[Tile],
    obj: u64,
    container: u64,
    ignore_blocked_line: bool,
    out: &mut [f32; 3],
) -> bool {
    let me = UuView(h.unit(obj));
    let box_ = UuView(h.unit(container));
    let mut continent = 0;
    if box_.on_map() {
        continent = h.container_continent(container, me.0.complex);
    }
    let mut found = false;
    for &t in tiles {
        if found {
            break;
        }
        let stands = me.0.complex == 0 || can_stand_on(h, t, me.0.complex);
        let here = h.continent_at(t[0], t[1]);
        found = stands
            && (here == 0
                || continent == 0
                || continent == here
                || h.are_connected_for_unit(obj, here, continent)
                || (me.class().domains() & UU_MOVEMENT_SEA != 0 && {
                    let row = h.continent_number(here);
                    let column = h.continent_number(continent);
                    h.continent_link(row, column)
                }))
            && find_place_on_tile(
                h,
                me.0.complex,
                0,
                box_.tile_x(),
                box_.tile_y(),
                box_.class().tile_size(),
                t[0],
                t[1],
                0,
                out,
            )
            && (ignore_blocked_line || !h.line_blocked(t[0], t[1], box_.tile_x(), box_.tile_y()));
    }
    found
}

/// `EEGetNearestFootprintTile` (0x246d8d): the tile of the unit's footprint whose centre (on the terrain mesh) is
/// nearest the point, and that squared distance (a one-tile unit: its tile, the ground distance... in 3D from the
/// tile's centre at height 0).
pub fn nearest_footprint_tile<H: UuHost + ?Sized>(h: &mut H, obj: u64, point: [f32; 3]) -> (f32, Tile) {
    let unit = UuView(h.unit(obj));
    let size = unit.class().tile_size();
    let mut best = f32::MAX;
    let mut at = [unit.tile_x(), unit.tile_y()];
    let p = Ge3DPoint { x: point[0], y: point[1], z: point[2] };
    if size > 1 {
        let right = unit.tile_x() + size;
        let bottom = unit.tile_y() + size;
        for tx in unit.tile_x()..right {
            for ty in unit.tile_y()..bottom {
                let cx = tx as f32 + 0.5;
                let cy = ty as f32 + 0.5;
                let c = Ge3DPoint { x: cx, y: cy, z: h.mesh_ground(cx, cy) };
                let d = c.distance_to_squared(p);
                if !(d < best) {
                    continue;
                }
                best = d;
                at = [tx, ty];
            }
        }
    } else {
        let c = Ge3DPoint { x: unit.tile_x() as f32 + 0.5, y: unit.tile_y() as f32 + 0.5, z: 0.0 };
        best = p.distance_to_squared(c);
    }
    (best, at)
}

/// `EEFindPlacement` (0x243561): a free place for a unit (`obj`, or one of `class`) around (`x`, `y`), ring by ring
/// up to `rings` (from the centre with `include_center`), on a terrain its class moves on, off the edge, reachable from
/// the start's continent (or linked for a sea unit or `linked`), with a clear line back to the target (or the centre)
/// when the rings have no corners, and a free spot ([`find_place_on_tile`]). `m` faces it from `point` (`face_point`)
/// or south, on the ground, pitched and rolled with the terrain as the class does. Whether a place was found.
#[allow(clippy::too_many_arguments)]
pub fn find_placement<H: UuHost + ?Sized>(
    h: &mut H,
    point: [f32; 3],
    obj: u64,
    class: u64,
    x: u32,
    y: u32,
    size: i32,
    rings: i32,
    corners: bool,
    include_center: bool,
    face_point: bool,
    linked: bool,
    m: &mut GeTransformation,
    target: u64,
) -> bool {
    let map = h.map();
    let mut kind = 0;
    let class_obj = if class != 0 {
        class
    } else if obj != 0 {
        UuView(h.unit(obj)).0.class_obj
    } else {
        0
    };
    let cls = UuClassView(h.class(class_obj).fields, class_obj);
    if class_obj != 0 {
        kind = h.passable_flags(class_obj, obj);
    }
    let id = h.continent_id(x as i32, y as i32);
    let continent = if id != 0 { h.continent_by_id(id) } else { 0 };
    let mut ring = i32::from(!include_center);
    let mut searching = true;
    let mut location = [0.0f32; 3];
    loop {
        if ring > rings {
            break;
        }
        let tiles = if face_point {
            sorted_ring_tiles(h, point, x as i32, y as i32, size, ring, i32::from(corners))
        } else {
            ring_tiles(h, x as i32, y as i32, size, ring, i32::from(corners))
        };
        let mut i = 0;
        while searching && i < tiles.len() {
            let [tx, ty] = tiles[i];
            i += 1;
            let t = h.map_tile(map, tx, ty);
            let other = h.continent_at(tx, ty);
            if t == 0 {
                continue;
            }
            let terrain = h.tile_terrain(t);
            if !h.can_move_on_terrain(class_obj, obj, terrain) || h.is_edge_tile(tx, ty) || continent == 0 || other == 0
            {
                continue;
            }
            if continent != other && !h.are_connected(continent, other, kind) {
                let open = (cls.domains() & UU_MOVEMENT_SEA != 0 || linked) && {
                    let row = h.continent_number(continent);
                    let column = h.continent_number(other);
                    h.continent_link(row, column)
                };
                if !open {
                    continue;
                }
            }
            if !corners && rings > 1 {
                let (to_x, to_y) = if target != 0 {
                    let (_, at) = nearest_footprint_tile(h, target, [tx as f32 + 0.5, ty as f32 + 0.5, 0.0]);
                    (at[0], at[1])
                } else {
                    (x as i32, y as i32)
                };
                if h.line_blocked(tx, ty, to_x, to_y) {
                    continue;
                }
            }
            let (from_x, from_y) = if include_center { (tx, ty) } else { (x as i32, y as i32) };
            if find_place_on_tile(
                h,
                obj,
                class_obj,
                from_x,
                from_y,
                size,
                tx,
                ty,
                i32::from(include_center),
                &mut location,
            ) {
                searching = false;
            }
        }
        ring += 1;
        if !searching {
            break;
        }
    }
    if !searching {
        let mut yaw = std::f32::consts::PI;
        if face_point {
            yaw = um_arctan2(-(point[0] - location[0]), point[1] - location[1]);
        }
        m.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
        let ground = h.ground_point(location);
        m.m[0][3] = ground[0];
        m.m[1][3] = ground[1];
        m.m[2][3] = ground[2];
        let mut pitch = 0.0;
        let mut roll = 0.0;
        if cls.pitches_with_terrain() {
            pitch = h.terrain_tilt(m, false);
        }
        if cls.rolls_with_terrain() {
            roll = h.terrain_tilt(m, true);
        }
        m.set_orientation(RotationOrder::Ypr, yaw, pitch, roll);
    }
    !searching
}

// ---- the random turns ----

/// `EETurnAway` (0x248f86): the direction turned by a random amount within the spread (two draws: the turn, then the
/// side).
pub fn turn_away<H: UuHost + ?Sized>(h: &mut H, direction: i32, spread: i32) -> i32 {
    let turn = h.random_long(0, spread * 2 + 1) + (4 - spread);
    let mut result = direction;
    let side = h.random_long(0, 1) != 0;
    if turn != 0 {
        result = turn_direction(result, turn % 8, side);
    }
    result
}

/// `EEJitterHeading` (0x24db3f): the angle turned by a random share of a full turn (no draw when the bounds meet).
pub fn jitter<H: UuHost + ?Sized>(h: &mut H, angle: f32, min: f32, max: f32) -> f32 {
    let turn = if min == max { min } else { h.random_float(min, max) };
    jitter_heading(angle, turn)
}

/// `floor` of a coordinate (`UMFloor`), for the FFI's callers.
pub fn floor_tile(x: f32, y: f32) -> Tile {
    [floor(x), floor(y)]
}
