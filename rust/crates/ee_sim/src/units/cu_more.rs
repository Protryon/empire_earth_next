//! More of `EEComplexUnit`'s bodies over its C++ statements (the units' lane, round 8; were EEComplexUnit.cpp):
//! `QueueResearch` with its TQRS record, `ResizeTurretAngles`, `AddCenterOffset`, `GetEffectHeight`,
//! `PlaceOnGround`, `FaceToward`, `FaceTowardSquare` and `FaceTarget`. Every C++ statement is one `EE_UNITS_CUM_*`
//! case of `ee_units_cu_host` (EEComplexUnit.cpp; from 1000), asked in the original's order; the transformations,
//! points and vectors the originals kept as locals are the host's file statics, set where they were declared.
//!
//! Determinism: the yaws are `UMArcTan2` (ee_base `um_arctan2`) of the original's `float` differences, the floors
//! `UMFloor`, the heights the original's `float` products in order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::app::capture_record::Record;
use crate::units::cu_body::CuHost;
use ee_base::math::um::{floor, um_arctan2};

// ---- the statements (`op`; `a` an object, `b`, `c` values, `f` floats)
/// `mPlayer->mPlayerID`, `mUnitID`, `GetUnitClass()->GetUnitType()`, `mBuildQueue.size()`
pub const EE_UNITS_CUM_PLAYER_ID: i32 = 1000;
pub const EE_UNITS_CUM_UNIT_ID: i32 = 1001;
pub const EE_UNITS_CUM_UNIT_TYPE: i32 = 1002;
pub const EE_UNITS_CUM_BUILD_QUEUE_SIZE: i32 = 1003;
/// `QueueResearchUncaptured(b, c != 0)`
pub const EE_UNITS_CUM_QUEUE_UNCAPTURED: i32 = 1004;
/// `delete[] mTurretAngles`
pub const EE_UNITS_CUM_TURRETS_DELETE: i32 = 1005;
/// `mTurretAngles = NULL`
pub const EE_UNITS_CUM_TURRETS_NULL: i32 = 1006;
/// `mTurretAngles = new float[b]`
pub const EE_UNITS_CUM_TURRETS_NEW: i32 = 1007;
/// `mTurretAngles[b] = 0.0f`
pub const EE_UNITS_CUM_TURRET_ZERO: i32 = 1008;
/// `GetUnitClass()->GetTileSize()`
pub const EE_UNITS_CUM_TILE_SIZE: i32 = 1009;
/// `mPhysicalModel.GetLocation().Apply(` the model's bounding box (frame: the current if `b`, else 0) top right
/// back`, ` the point `= GE3DPoint(0, 0, 0))`
pub const EE_UNITS_CUM_EFFECT_TOP: i32 = 1010;
/// `f[0..3] = ` the point
pub const EE_UNITS_CUM_POINT_OUT: i32 = 1011;
/// `f[0] = GetStats()->GetScale()`
pub const EE_UNITS_CUM_STATS_SCALE: i32 = 1012;
/// `mIsBuilt`, `mHitPoints`, `GetStats()->mMaxHitPoints`
pub const EE_UNITS_CUM_IS_BUILT: i32 = 1013;
pub const EE_UNITS_CUM_HIT_POINTS: i32 = 1014;
pub const EE_UNITS_CUM_MAX_HIT_POINTS: i32 = 1015;
/// the transformation `= mPhysicalModel.GetLocation()`
pub const EE_UNITS_CUM_LOAD_LOCATION: i32 = 1016;
/// `f[0..3] = ` the transformation's x, y, z
pub const EE_UNITS_CUM_LOCATION_OUT: i32 = 1017;
/// the transformation's `SetOrientationYPR(f[0], f[1], f[2])`
pub const EE_UNITS_CUM_SET_YPR: i32 = 1018;
/// `SetTransformation(` the transformation`, true)`
pub const EE_UNITS_CUM_APPLY_TRANSFORMATION: i32 = 1019;
/// `gWorld.mMap->GetGroundPoint(GE3DPoint(f[0..3]), ` the point `= GE3DPoint(0, 0, 0))`
pub const EE_UNITS_CUM_GROUND_POINT: i32 = 1020;
/// `GetUnitClass()->GroundLocation(` the point `)`
pub const EE_UNITS_CUM_CLASS_GROUND: i32 = 1021;
/// the transformation's `SetX`, `SetY`, `SetZ` of the point
pub const EE_UNITS_CUM_POSITION_AT_POINT: i32 = 1022;
/// the transformation's `Apply(GEVector(0, 1, 0), ` the vector `= GEVector(0, 0, 0))`; `f[0..3] = ` the vector
pub const EE_UNITS_CUM_APPLY_UP: i32 = 1023;
pub const EE_UNITS_CUM_VECTOR_OUT: i32 = 1024;
/// `GetUnitClass()->mPitchesWithTerrain`, `->mRollsWithTerrain`
pub const EE_UNITS_CUM_PITCHES: i32 = 1025;
pub const EE_UNITS_CUM_ROLLS: i32 = 1026;
/// `f[0] = gWorld.mMap->GetTerrainPitch(` the transformation `)`, `GetTerrainRoll`
pub const EE_UNITS_CUM_TERRAIN_PITCH: i32 = 1027;
pub const EE_UNITS_CUM_TERRAIN_ROLL: i32 = 1028;
/// `mPhysicalModel.SetLocation(` the transformation `)`
pub const EE_UNITS_CUM_SET_LOCATION: i32 = 1029;
/// unit `a`'s `mUnitClass->mTileSize`, `GetTileX()`, `GetTileY()`
pub const EE_UNITS_CUM_TILE_SIZE_OF: i32 = 1030;
pub const EE_UNITS_CUM_TILE_X_OF: i32 = 1031;
pub const EE_UNITS_CUM_TILE_Y_OF: i32 = 1032;
/// unit `a`'s `GetComplexUnit() != NULL && GetModel()->mModel != NULL`
pub const EE_UNITS_CUM_HAS_MODEL: i32 = 1033;
/// `f[0..3] = ` unit `a`'s `GetModel()->mTopPoint`
pub const EE_UNITS_CUM_TOP_POINT_OF: i32 = 1034;
/// `f[0] = ` unit `a`'s `GetComplexUnit()->mPhysicalModel.GetScale()`
pub const EE_UNITS_CUM_SCALE_OF: i32 = 1035;
/// unit `a`'s `GetComplexUnit()->mPhysicalModel.GetLocation().Apply(GE3DPoint(f[0..3]), ` the point `)`
pub const EE_UNITS_CUM_APPLY_OF: i32 = 1036;
/// unit `a`'s `GetPosition(` the point `)`
pub const EE_UNITS_CUM_POSITION_OF: i32 = 1037;

// ---- the bodies (`ee_units_cu_more`'s `what`)
pub const EE_UNITS_CUM_DO_QUEUE_RESEARCH: i32 = 1;
pub const EE_UNITS_CUM_DO_RESIZE_TURRETS: i32 = 2;
pub const EE_UNITS_CUM_DO_CENTER_OFFSET: i32 = 3;
pub const EE_UNITS_CUM_DO_EFFECT_HEIGHT: i32 = 4;
pub const EE_UNITS_CUM_DO_PLACE_ON_GROUND: i32 = 5;
pub const EE_UNITS_CUM_DO_FACE_TOWARD: i32 = 6;
pub const EE_UNITS_CUM_DO_FACE_SQUARE: i32 = 7;
pub const EE_UNITS_CUM_DO_FACE_TARGET: i32 = 8;

/// "TQRS", as `EECaptureTag` builds it (the first letter in the low byte)
pub const TQRS: u32 = u32::from_le_bytes(*b"TQRS");

/// The world the bodies reach beside the statements.
pub trait CuMoreHost: CuHost {
    /// `EESimCaptureRecord::Active()`
    fn capture_active(&mut self) -> bool;
    /// a record's commit
    fn commit(&mut self, r: &Record);
}

fn q<H: CuMoreHost + ?Sized>(h: &mut H, op: i32, unit: u64) -> i64 {
    h.cu(op, unit, 0, 0, 0, &mut [0.0; 4])
}

fn qa<H: CuMoreHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64) -> i64 {
    h.cu(op, unit, a, 0, 0, &mut [0.0; 4])
}

fn qb<H: CuMoreHost + ?Sized>(h: &mut H, op: i32, unit: u64, b: i64) -> i64 {
    h.cu(op, unit, 0, b, 0, &mut [0.0; 4])
}

/// a statement's floats out
fn floats<H: CuMoreHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64) -> [f32; 4] {
    let mut f = [0.0; 4];
    h.cu(op, unit, a, 0, 0, &mut f);
    f
}

/// a statement with floats in
fn with<H: CuMoreHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64, f: [f32; 4]) -> i64 {
    let mut f = f;
    h.cu(op, unit, a, 0, 0, &mut f)
}

/// `QueueResearch(technology, front)`: the queue's body; a capture records TQRS (the player, unit, class type,
/// technology, front flag and the queue's size before and after).
pub fn queue_research<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, technology: c_long, front: bool) {
    if !h.capture_active() {
        h.cu(EE_UNITS_CUM_QUEUE_UNCAPTURED, unit, 0, i64::from(technology), i64::from(front), &mut [0.0; 4]);
        return;
    }
    let mut r = Record::new(TQRS);
    r.i32(q(h, EE_UNITS_CUM_PLAYER_ID, unit) as i32);
    r.i32(q(h, EE_UNITS_CUM_UNIT_ID, unit) as i32);
    r.i32(q(h, EE_UNITS_CUM_UNIT_TYPE, unit) as i32);
    r.i32(technology as i32);
    r.u32(u32::from(front));
    r.u32(q(h, EE_UNITS_CUM_BUILD_QUEUE_SIZE, unit) as u32);
    h.cu(EE_UNITS_CUM_QUEUE_UNCAPTURED, unit, 0, i64::from(technology), i64::from(front), &mut [0.0; 4]);
    r.u32(q(h, EE_UNITS_CUM_BUILD_QUEUE_SIZE, unit) as u32);
    h.commit(&r);
}

/// `ResizeTurretAngles(count)`: a new array of `count` zeros (none for 0).
pub fn resize_turret_angles<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, count: u32) {
    q(h, EE_UNITS_CUM_TURRETS_DELETE, unit);
    if count == 0 {
        q(h, EE_UNITS_CUM_TURRETS_NULL, unit);
    } else {
        qb(h, EE_UNITS_CUM_TURRETS_NEW, unit, i64::from(count));
        for i in 0..count {
            qb(h, EE_UNITS_CUM_TURRET_ZERO, unit, i64::from(i));
        }
    }
}

/// `AddCenterOffset(x, y)`: a tile position to the centre of the unit's footprint.
pub fn add_center_offset<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, x: f32, y: f32) -> (f32, f32) {
    let size = q(h, EE_UNITS_CUM_TILE_SIZE, unit) as c_long;
    if size <= 1 {
        (x + 0.5, y + 0.5)
    } else {
        let x = x + size as f32 * 0.5;
        let size = q(h, EE_UNITS_CUM_TILE_SIZE, unit) as c_long;
        (x, y + size as f32 * 0.5)
    }
}

/// `GetEffectHeight(current_frame)`: the model's top for effects, scaled by build progress while unbuilt.
pub fn effect_height<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, current_frame: bool) -> f32 {
    qb(h, EE_UNITS_CUM_EFFECT_TOP, unit, i64::from(current_frame));
    let z = floats(h, EE_UNITS_CUM_POINT_OUT, unit, 0)[2];
    let scale = floats(h, EE_UNITS_CUM_STATS_SCALE, unit, 0)[0];
    let mut height = scale * z;
    if q(h, EE_UNITS_CUM_IS_BUILT, unit) == 0 {
        let hit_points = q(h, EE_UNITS_CUM_HIT_POINTS, unit) as c_long;
        let max = q(h, EE_UNITS_CUM_MAX_HIT_POINTS, unit) as c_long;
        height *= hit_points as f32 / max as f32;
    }
    height
}

/// `PlaceOnGround(point)`: on the ground at the point, yawed as it was and pitched and rolled with the terrain when
/// its class does.
pub fn place_on_ground<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, point: [f32; 3]) {
    q(h, EE_UNITS_CUM_LOAD_LOCATION, unit);
    with(h, EE_UNITS_CUM_GROUND_POINT, unit, 0, [point[0], point[1], point[2], 0.0]);
    q(h, EE_UNITS_CUM_CLASS_GROUND, unit);
    q(h, EE_UNITS_CUM_POSITION_AT_POINT, unit);
    q(h, EE_UNITS_CUM_APPLY_UP, unit);
    let up = floats(h, EE_UNITS_CUM_VECTOR_OUT, unit, 0);
    let yaw = um_arctan2(-up[0], up[1]);
    let mut pitch = 0.0;
    let mut roll = 0.0;
    if q(h, EE_UNITS_CUM_PITCHES, unit) != 0 {
        pitch = floats(h, EE_UNITS_CUM_TERRAIN_PITCH, unit, 0)[0];
    }
    if q(h, EE_UNITS_CUM_ROLLS, unit) != 0 {
        roll = floats(h, EE_UNITS_CUM_TERRAIN_ROLL, unit, 0)[0];
    }
    with(h, EE_UNITS_CUM_SET_YPR, unit, 0, [yaw, pitch, roll, 0.0]);
    q(h, EE_UNITS_CUM_SET_LOCATION, unit);
}

/// `FaceToward(point)`: level, facing the point.
pub fn face_toward<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, point: [f32; 3]) {
    q(h, EE_UNITS_CUM_LOAD_LOCATION, unit);
    let l = floats(h, EE_UNITS_CUM_LOCATION_OUT, unit, 0);
    let dy = point[1] - l[1];
    let yaw = um_arctan2(-(point[0] - l[0]), dy);
    with(h, EE_UNITS_CUM_SET_YPR, unit, 0, [yaw, 0.0, 0.0, 0.0]);
    q(h, EE_UNITS_CUM_APPLY_TRANSFORMATION, unit);
}

/// `FaceTowardSquare(x, y, size)`: level, facing the centre of the square of tiles.
pub fn face_toward_square<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, x: c_long, y: c_long, size: c_long) {
    q(h, EE_UNITS_CUM_LOAD_LOCATION, unit);
    let half = size as f32 * 0.5;
    let tx = floor(x as f32 + half) as c_long;
    let ty = floor(y as f32 + half) as c_long;
    let l = floats(h, EE_UNITS_CUM_LOCATION_OUT, unit, 0);
    let yaw = um_arctan2(-((tx as f32 + 0.5) - l[0]), (ty as f32 + 0.5) - l[1]);
    with(h, EE_UNITS_CUM_SET_YPR, unit, 0, [yaw, 0.0, 0.0, 0.0]);
    q(h, EE_UNITS_CUM_APPLY_TRANSFORMATION, unit);
}

/// `FaceTarget(target)`: a large target's footprint centre, else its scaled top point (or its position).
pub fn face_target<H: CuMoreHost + ?Sized>(h: &mut H, unit: u64, target: u64) {
    let size = qa(h, EE_UNITS_CUM_TILE_SIZE_OF, unit, target) as c_long;
    if size > 1 {
        let x = qa(h, EE_UNITS_CUM_TILE_X_OF, unit, target) as c_long;
        let y = qa(h, EE_UNITS_CUM_TILE_Y_OF, unit, target) as c_long;
        face_toward_square(h, unit, x, y, size);
        return;
    }
    if qa(h, EE_UNITS_CUM_HAS_MODEL, unit, target) != 0 {
        let top = floats(h, EE_UNITS_CUM_TOP_POINT_OF, unit, target);
        let scale = floats(h, EE_UNITS_CUM_SCALE_OF, unit, target)[0];
        with(h, EE_UNITS_CUM_APPLY_OF, unit, target, [top[0] * scale, top[1] * scale, top[2], 0.0]);
    } else {
        qa(h, EE_UNITS_CUM_POSITION_OF, unit, target);
    }
    let p = floats(h, EE_UNITS_CUM_POINT_OUT, unit, 0);
    face_toward(h, unit, [p[0], p[1], p[2]]);
}

/// Runs body `what` on `unit` (`f` the floats in and out).
pub fn run<H: CuMoreHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: i64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
    match what {
        EE_UNITS_CUM_DO_QUEUE_RESEARCH => queue_research(h, unit, a as c_long, b != 0),
        EE_UNITS_CUM_DO_RESIZE_TURRETS => resize_turret_angles(h, unit, a as u32),
        EE_UNITS_CUM_DO_CENTER_OFFSET => {
            let (x, y) = add_center_offset(h, unit, f[0], f[1]);
            f[0] = x;
            f[1] = y;
        }
        EE_UNITS_CUM_DO_EFFECT_HEIGHT => f[0] = effect_height(h, unit, a != 0),
        EE_UNITS_CUM_DO_PLACE_ON_GROUND => place_on_ground(h, unit, [f[0], f[1], f[2]]),
        EE_UNITS_CUM_DO_FACE_TOWARD => face_toward(h, unit, [f[0], f[1], f[2]]),
        EE_UNITS_CUM_DO_FACE_SQUARE => face_toward_square(h, unit, a as c_long, b as c_long, c as c_long),
        EE_UNITS_CUM_DO_FACE_TARGET => face_target(h, unit, a as u64),
        _ => {}
    }
    0
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use crate::units::cu_body::game::Game;
    use std::os::raw::c_void;

    impl CuMoreHost for Game {
        fn capture_active(&mut self) -> bool {
            crate::app::capture_record::active()
        }
        fn commit(&mut self, r: &Record) {
            r.commit();
        }
    }

    /// `EEComplexUnit`'s body `what` (`EE_UNITS_CUM_DO_*`) on `unit`: `x`, `y`, `z` the floats in; the first float
    /// out returned and written to `out_x` (the second to `out_y`) when given.
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`, the outs null or writable, on the simulation thread.
    #[allow(clippy::too_many_arguments)]
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_more(
        what: i32,
        unit: *mut c_void,
        a: i64,
        b: i64,
        c: i64,
        x: f32,
        y: f32,
        z: f32,
        out_x: *mut f32,
        out_y: *mut f32,
    ) -> f32 {
        let mut f = [x, y, z, 0.0];
        run(&mut Game, what, unit as usize as u64, a, b, c, &mut f);
        // SAFETY: the caller's outs (null: none)
        unsafe {
            if !out_x.is_null() {
                *out_x = f[0];
            }
            if !out_y.is_null() {
                *out_y = f[1];
            }
        }
        f[0]
    }
}

#[cfg(test)]
#[path = "cu_more_tests.rs"]
mod tests;
