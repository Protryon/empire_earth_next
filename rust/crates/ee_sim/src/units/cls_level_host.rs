//! The ground levelling's host in the game ([`LevelHost`] over the unit utilities' world reads and the class
//! bodies' C++ switch, `EEUnitClassMoveOp`) and the entry points the `EEUnitClass` members call (cls_level.rs).
//! Absent from a test binary.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 on LP64: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::cls_host::{ClsGame, EeUnitsClsIo};
use crate::units::cls_level::{self as lv, LevelHost};
use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::uu::UuHost;
use crate::units::uu_host::Game;

/// the mesh `a`'s `GetZ(c, d)` into `io.f`
pub const EE_UNITS_CLS_LV_MESH_Z: i32 = 85;
/// the mesh `a`'s `SetZ(c, d, io.f)`
pub const EE_UNITS_CLS_LV_SET_MESH_Z: i32 = 86;
/// the tile `a`'s `mHeight = io.f`
pub const EE_UNITS_CLS_LV_SET_TILE_HEIGHT: i32 = 87;
/// `&gWorld.mMap->mTerrainMesh`
pub const EE_UNITS_CLS_LV_MAP_MESH: i32 = 88;
/// class `b`'s `CreateUnit(c, d, player io.a, foundation io.b, creator a)->mComplexUnit`
pub const EE_UNITS_CLS_LV_CREATE_UNIT: i32 = 89;
/// unit `a`'s `LinkPart(b)`
pub const EE_UNITS_CLS_LV_LINK_PART: i32 = 90;
/// unit `a`'s `SetFacing(c)`
pub const EE_UNITS_CLS_LV_SET_FACING: i32 = 91;
/// `gCameraFollowing = false`
pub const EE_UNITS_CLS_LV_CAMERA_OFF: i32 = 92;

impl ClsGame {
    fn lv(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: f32) -> (i64, EeUnitsClsIo) {
        let mut io = EeUnitsClsIo { f, ..Default::default() };
        let r = self.call(op, a, b, c, d, &mut io);
        (r, io)
    }
}

impl LevelHost for ClsGame {
    fn map(&mut self) -> u64 {
        self.0.map()
    }
    fn map_tile(&mut self, x: i64, y: i64) -> u64 {
        let map = self.0.map();
        self.0.map_tile(map, x as i32, y as i32)
    }
    fn tile_height(&mut self, tile: u64) -> f32 {
        self.0.tile_height(tile)
    }
    fn set_tile_height(&mut self, tile: u64, height: f32) {
        self.lv(EE_UNITS_CLS_LV_SET_TILE_HEIGHT, tile, 0, 0, 0, height);
    }
    fn mesh_z(&mut self, mesh: u64, x: i64, y: i64) -> f32 {
        self.lv(EE_UNITS_CLS_LV_MESH_Z, mesh, 0, x, y, 0.0).1.f
    }
    fn set_mesh_z(&mut self, mesh: u64, x: i64, y: i64, z: f32) {
        self.lv(EE_UNITS_CLS_LV_SET_MESH_Z, mesh, 0, x, y, z);
    }
    fn map_mesh(&mut self) -> u64 {
        self.lv(EE_UNITS_CLS_LV_MAP_MESH, 0, 0, 0, 0, 0.0).0 as u64
    }
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
        self.0.class(class).fields
    }
    fn land_building(&mut self, class: u64) -> bool {
        self.0.class(class).land_building != 0
    }
    fn class_of_type(&mut self, unit_type: u64) -> u64 {
        self.0.class_of_type(unit_type as i32).obj
    }
    fn unit_tile(&mut self, unit: u64) -> [i64; 2] {
        let r = self.0.unit(unit);
        // SAFETY: the unit's live record
        unsafe { [(*r.rts).mTileX as i64, (*r.rts).mTileY as i64] }
    }
    fn unit_facing(&mut self, unit: u64) -> u64 {
        // SAFETY: the unit's live record
        unsafe { (*self.0.unit(unit).cx).mFacing as u64 }
    }
    fn unit_class(&mut self, unit: u64) -> u64 {
        self.0.unit(unit).class_obj
    }
    fn create_unit(&mut self, class: u64, x: i64, y: i64, player: i64, foundation: bool, creator: u64) -> u64 {
        let mut io = EeUnitsClsIo { a: player, b: i64::from(foundation), ..Default::default() };
        self.call(EE_UNITS_CLS_LV_CREATE_UNIT, creator, class, x, y, &mut io) as u64
    }
    fn link_part(&mut self, unit: u64, other: u64) {
        self.lv(EE_UNITS_CLS_LV_LINK_PART, unit, other, 0, 0, 0.0);
    }
    fn set_facing(&mut self, unit: u64, facing: u64) {
        self.lv(EE_UNITS_CLS_LV_SET_FACING, unit, 0, facing as i64, 0, 0.0);
    }
    fn camera_following_off(&mut self) {
        self.lv(EE_UNITS_CLS_LV_CAMERA_OFF, 0, 0, 0, 0, 0.0);
    }
}

fn game() -> ClsGame {
    ClsGame(Game)
}

/// `EEUnitClass::LevelGround(mesh, x, y, z)`.
///
/// # Safety
/// `z` is a writable float.
#[no_mangle]
pub unsafe extern "C" fn ee_units_lv_level_ground(class: u64, mesh: u64, x: u64, y: u64, z: *mut f32) {
    // SAFETY: the caller's contract (copied: the body calls back into C++)
    let mut value = unsafe { *z };
    lv::level_ground(&mut game(), class, mesh, x, y, &mut value);
    // SAFETY: as above
    unsafe { *z = value };
}

/// `EEUnitClass::LevelRunway*(airport)` by the direction's facing (0 +y, 1 -y, 2 +x, else -x).
#[no_mangle]
pub extern "C" fn ee_units_lv_level_runway(unit: u64, facing: u64) {
    lv::level_runway(&mut game(), unit, facing);
}

/// `EEUnitClass::CreateRunways(airport, player, foundation)`.
#[no_mangle]
pub extern "C" fn ee_units_lv_create_runways(class: u64, unit: u64, player: i64, foundation: u8) {
    lv::create_runways(&mut game(), class, unit, player, foundation != 0);
}
