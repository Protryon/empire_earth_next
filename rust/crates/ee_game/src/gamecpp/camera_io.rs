//! The camera's host (game core lane, round 11, lane B; was EECameraControl.cpp's `EECameraHost`, `EECameraUnit`
//! and `AddKey`): ee_game app camera_ffi.rs's callback answered in Rust, each C++ call or member a one-statement
//! case of EECameraControl.cpp's `EECore10OpD7` ([`super::EeGameCpp`]'s `Ec` and `Dk` ops). The context is the
//! camera; a unit is resolved from its id each call (NULL for none or one that no longer resolves).
//!
//! The camera runs on the UI thread and the physics thread's update (the tour draws it; the app capture profile
//! compares CAMU).
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_void};
use std::sync::atomic::{AtomicUsize, Ordering};

use ee_base::math::types::Ge3DPoint;

use crate::app::camera::{
    EeAppCameraFields, EE_APP_CAMERA_ALIVE, EE_APP_CAMERA_CINEMATIC, EE_APP_CAMERA_EXISTS, EE_APP_CAMERA_FAR_REAR,
    EE_APP_CAMERA_FREE, EE_APP_CAMERA_HAS_MAP, EE_APP_CAMERA_NO_UNIT, EE_APP_CAMERA_ON_MAP, EE_APP_CAMERA_ORTHOGRAPHIC,
    EE_APP_CAMERA_UNIT_A, EE_APP_CAMERA_UNIT_B,
};
use crate::app::camera_ffi::*;

use super::camera_host::ee_gamecpp_camera_unit_point;
use super::{cptr, h, hd, hf, hp, ptr, O, P};

/// The tile coordinate of a unit off the map.
const OFF_MAP: c_long = -10000;

/// The world's camera (the one `EECameraControl`, gWorld's): the object (the host's context) and its record.
static CAMERA: AtomicUsize = AtomicUsize::new(0);
static FIELDS: AtomicUsize = AtomicUsize::new(0);
/// `EECameraControl()`: the plain members' defaults, no followed unit; the camera is the world's from here on.
///
/// # Safety
/// `camera` is the EECameraControl being constructed and `rec` its record.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_camera_new(camera: *mut c_void, rec: *mut EeAppCameraFields) {
    // SAFETY: the caller's record
    unsafe { ee_app_camera_default(rec) };
    CAMERA.store(camera as usize, Ordering::Relaxed);
    FIELDS.store(rec as usize, Ordering::Relaxed);
    set_unit(EE_APP_CAMERA_UNIT_A, EE_APP_CAMERA_NO_UNIT);
    set_unit(EE_APP_CAMERA_UNIT_B, EE_APP_CAMERA_NO_UNIT);
}

/// The world's camera's record.
pub(crate) fn fields() -> *mut EeAppCameraFields {
    FIELDS.load(Ordering::Relaxed) as *mut EeAppCameraFields
}

fn cam() -> *mut c_void {
    CAMERA.load(Ordering::Relaxed) as *mut c_void
}

/// The followed unit in `slot` (`mUnitA.Get()`): NULL for none or one that no longer resolves.
pub fn unit(slot: u32) -> P {
    crate::rts::units::ee_rts_unit_resolve(ee_app_camera_unit(slot)) as P
}

/// The camera's followed unit's handle in `slot`: its saved id, or none.
fn slot_unit(slot: u32) -> u32 {
    if unit(slot) != 0 {
        ee_app_camera_unit(slot)
    } else {
        EE_APP_CAMERA_NO_UNIT
    }
}

// ---- the world's camera by path (were the hosts' `gWorld.mCameraControl.X()` cases)

/// `ClearUnitA`.
pub fn clear_unit_a() {
    // SAFETY (each below): the world's camera and its host
    unsafe { ee_app_camera_clear_unit_a(fields(), cam(), ee_gamecpp_camera_host) }
}

/// `ClearUnitB`.
pub fn clear_unit_b() {
    unsafe { ee_app_camera_clear_unit_b(fields(), cam(), ee_gamecpp_camera_host) }
}

/// `ResetPath`.
pub fn reset_path() {
    unsafe { ee_app_camera_reset_path(fields(), cam(), ee_gamecpp_camera_host) }
}

/// `SetChanged`.
pub fn set_changed() {
    unsafe { ee_app_camera_set_changed(fields()) }
}

/// `GetZoom`.
pub fn zoom() -> f32 {
    // SAFETY: the world's camera's record
    unsafe { (*fields()).mZoom }
}

/// `SetZoom` (`ResetZoom` is `set_zoom(1.0)`).
pub fn set_zoom(zoom: f32) {
    unsafe { ee_app_camera_set_zoom(fields(), cam(), ee_gamecpp_camera_host, zoom) }
}

/// `SetMode`.
pub fn set_mode(mode: c_long) {
    unsafe { ee_app_camera_set_mode(fields(), cam(), ee_gamecpp_camera_host, mode) }
}

/// `Scroll`.
pub fn scroll(dx: f32, dy: f32) {
    unsafe { ee_app_camera_scroll(fields(), cam(), ee_gamecpp_camera_host, dx, dy) }
}

/// `Update`.
pub fn update(elapsed: f32) {
    unsafe { super::camera_host::ee_gamecpp_camera_update(cam(), elapsed) }
}

/// `GetLocation(mFraction, out)`.
pub fn location() -> Ge3DPoint {
    let mut out = Ge3DPoint::default();
    unsafe { ee_app_camera_point(fields(), 1, (*fields()).mFraction, &mut out) };
    out
}

/// `SetLocation`.
pub fn set_location(location: *const Ge3DPoint, immediate: bool, follow_terrain: bool) {
    unsafe { ee_app_camera_set_location(fields(), cam(), ee_gamecpp_camera_host, location, immediate, follow_terrain) }
}

/// `EECameraID`: a unit's handle id, or none.
fn unit_id(unit: P) -> u32 {
    if unit != 0 {
        // SAFETY: the live unit's record
        unsafe { crate::rts::units::ee_rts_unit_handle_id(crate::gamecpp::records::un_rts(unit)) }
    } else {
        EE_APP_CAMERA_NO_UNIT
    }
}

/// `Follow(unit, flag, duration, turn yaw, turn pitch)` (an EEUnit, or none).
pub fn follow(unit: P, flag: bool, duration: f32, yaw: f32, pitch: f32) {
    // SAFETY: the world's camera and its host
    unsafe { ee_app_camera_follow(fields(), cam(), ee_gamecpp_camera_host, unit_id(unit), flag, duration, yaw, pitch) }
}

/// `Follow(unit, mIsFollowingUnitB, 1, mTurnYaw, mTurnPitch)`: the swapped unit's successor (an EEUnit, or none).
pub fn follow_swapped(unit: P) {
    // SAFETY: the world's camera's record
    let c = unsafe { *fields() };
    follow(unit, c.mIsFollowingUnitB, 1.0, c.mTurnYaw, c.mTurnPitch);
}

// ---- the screens' camera calls (were EEScreens.cpp's, EEUIFMainGame.cpp's and EEUserInterface.cpp's shims)

/// `SetChanged`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_camera_set_changed() {
    set_changed();
}

/// `SetLocation((x, y, z), flag1, flag2)`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_camera_set_location(x: f32, y: f32, z: f32, immediate: bool, follow_terrain: bool) {
    set_location(&Ge3DPoint::new(x, y, z), immediate, follow_terrain);
}

/// `Follow(unit, follow, 1, 0, 0)`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_camera_follow(unit: usize, flag: bool) {
    follow(unit, flag, 1.0, 0.0, 0.0);
}

/// `ClearUnitA`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_camera_clear_unit_a() {
    clear_unit_a();
}

/// `ClearUnitB`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_camera_clear_unit_b() {
    clear_unit_b();
}

/// `Update(seconds)`.
#[no_mangle]
pub extern "C" fn ee_screenscpp_camera_update(seconds: f32) {
    update(seconds);
}

/// A unit's state bits (`EE_APP_CAMERA_EXISTS`, `_ALIVE`, `_ON_MAP`): 0 for none.
pub fn unit_state(exists: bool, hit_points: c_long, tile_x: c_long, tile_y: c_long) -> u32 {
    if !exists {
        return 0;
    }
    EE_APP_CAMERA_EXISTS
        | if hit_points > 0 { EE_APP_CAMERA_ALIVE } else { 0 }
        | if tile_x != OFF_MAP && tile_y != OFF_MAP { EE_APP_CAMERA_ON_MAP } else { 0 }
}

/// The world's switches as the camera's flag bits.
pub fn switches(free: bool, far_rear: bool, cinematic: bool, orthographic: bool, has_map: bool) -> u32 {
    [
        (free, EE_APP_CAMERA_FREE),
        (far_rear, EE_APP_CAMERA_FAR_REAR),
        (cinematic, EE_APP_CAMERA_CINEMATIC),
        (orthographic, EE_APP_CAMERA_ORTHOGRAPHIC),
        (has_map, EE_APP_CAMERA_HAS_MAP),
    ]
    .iter()
    .fold(0, |bits, &(on, bit)| if on { bits | bit } else { bits })
}

fn on(op: O) -> bool {
    h(op, 0, &[]) != 0
}

/// The camera's host (camera_ffi.rs `EeAppCameraFn`): the context is the camera.
///
/// # Safety
/// `context` is a live EECameraControl and `io` the operation's record.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_camera_host(context: *mut c_void, op: i32, io: *mut EeAppCameraIo) -> i32 {
    let _ = context;
    // SAFETY: the caller's record for the call
    let io = unsafe { &mut *io };
    let unit = if (EE_APP_CAMERA_OP_REFERENCE..=EE_APP_CAMERA_OP_HIDDEN).contains(&op) {
        crate::rts::units::ee_rts_unit_resolve(io.id) as P
    } else {
        0
    };
    let complex = if unit != 0 { hp(O::UnComplex, unit, &[]) } else { 0 };
    match op {
        EE_APP_CAMERA_OP_UNIT => io.id = slot_unit(io.slot),
        EE_APP_CAMERA_OP_SET_UNIT => {
            set_unit(io.slot, io.id);
        }
        EE_APP_CAMERA_OP_REFERENCE => {
            h(O::CgUnitReference, unit, &[]);
        }
        EE_APP_CAMERA_OP_DEREFERENCE => {
            h(O::CgUnitDereference, unit, &[]);
        }
        EE_APP_CAMERA_OP_UNIT_STATE => {
            io.id = if unit == 0 {
                0
            } else {
                unit_state(
                    true,
                    h(O::RoUnitHitPointsValue, unit, &[]) as c_long,
                    h(O::EcTileXRaw, unit, &[]) as c_long,
                    h(O::DkTileY, unit, &[]) as c_long,
                )
            };
        }
        EE_APP_CAMERA_OP_CONTAINER => {
            if complex != 0 && hp(O::EcContainer, complex, &[]) != 0 {
                io.id = h(O::EcContainerId, complex, &[]) as u32;
            }
        }
        EE_APP_CAMERA_OP_UNIT_POINT => {
            ee_gamecpp_camera_unit_point(unit as *mut c_void, ptr(&mut io.point) as usize as *mut c_void)
        }
        EE_APP_CAMERA_OP_UNIT_LOCATION => {
            io.flag = complex != 0 && h(O::EcFacesCamera, unit, &[]) == 0;
            if io.flag {
                h(O::EcLocation, complex, &[ptr(&mut io.t)]);
            }
        }
        EE_APP_CAMERA_OP_FOLLOW_RATE => {
            io.flag = complex != 0 && !on(O::RrCinematic);
            if io.flag {
                io.x = hd(O::EcVelocity, complex, &[], &[]) * hd(O::FwWorldSpeed, 0, &[], &[]);
            }
        }
        EE_APP_CAMERA_OP_HIDDEN => {
            io.flag = unit != 0
                && hp(O::UnPlayer, unit, &[]) != hp(O::TkCurrentPlayer, 0, &[])
                && h(O::EcIsSeen, unit, &[]) == 0;
        }
        EE_APP_CAMERA_OP_KEY_COUNT => io.id = keys().len() as u32,
        EE_APP_CAMERA_OP_KEY => {
            if let Some(k) = keys().get(io.id as usize) {
                io.key = *k;
            }
        }
        EE_APP_CAMERA_OP_CLEAR_KEYS => keys().clear(),
        EE_APP_CAMERA_OP_FLAGS => {
            io.id = switches(
                crate::gamecpp::globals::get!(gFreeCamera),
                on(O::EcFarRearClipping),
                on(O::RrCinematic),
                on(O::RoOrthographic),
                on(O::EcHasMap),
            );
        }
        EE_APP_CAMERA_OP_WORLD_TRANSFORMATION => {
            h(O::EcWorldTransformation, 0, &[ptr(&mut io.t)]);
        }
        EE_APP_CAMERA_OP_WORLD_CAMERA_YAW => io.x = hd(O::EcWorldCameraYaw, 0, &[], &[]),
        EE_APP_CAMERA_OP_VALID_TILE => io.flag = hf(O::EcValidTile, 0, &[], &[io.x, io.y]) != 0,
        EE_APP_CAMERA_OP_VALID_POINT => io.flag = hf(O::EcValidPoint, 0, &[], &[io.x, io.y]) != 0,
        EE_APP_CAMERA_OP_MESH_Z => io.z = hd(O::MeshZ, 0, &[], &[io.x, io.y]),
        EE_APP_CAMERA_OP_EDGE_PLANES => {
            h(O::EcFrontPlane, 0, &[ptr(&mut io.planes.front)]);
            h(O::EcBackPlane, 0, &[ptr(&mut io.planes.back)]);
            h(O::EcLeftPlane, 0, &[ptr(&mut io.planes.left)]);
            h(O::EcRightPlane, 0, &[ptr(&mut io.planes.right)]);
        }
        EE_APP_CAMERA_OP_SCREEN_LINE => {
            hf(O::EcScreenLine, 0, &[ptr(&mut io.line)], &[io.x, io.y]);
        }
        EE_APP_CAMERA_OP_LOCATION_ON_LINE => {
            io.flag = h(O::EcLocationOnLine, 0, &[cptr(&io.line), ptr(&mut io.point)]) != 0;
        }
        EE_APP_CAMERA_OP_VIEWPORT => {
            io.x = hd(O::MpViewXCenter, 0, &[], &[]);
            io.y = hd(O::MpViewYCenter, 0, &[], &[]);
            io.z = hd(O::EcViewportHalfHeight, 0, &[], &[]);
        }
        EE_APP_CAMERA_OP_VIEWPORT_CAMERA => {
            io.point.x = hd(O::EcViewportCameraX, 0, &[], &[]);
            io.point.y = hd(O::EcViewportCameraY, 0, &[], &[]);
            io.point.z = hd(O::EcViewportCameraZ, 0, &[], &[]);
        }
        EE_APP_CAMERA_OP_FRONT_CLIPPING => io.x = hd(O::MpViewFrontClip, 0, &[], &[]),
        EE_APP_CAMERA_OP_SET_CLIPPING => {
            hf(O::EcSetFrontClipping, 0, &[], &[io.x]);
            hf(O::EcSetRearClipping, 0, &[], &[io.y]);
        }
        _ => {}
    }
    0
}

/// `AddKey`: a key appended to the camera's path -> 0, or 5 while a path plays (the camera locked).
#[no_mangle]
pub extern "C" fn ee_gamecpp_camera_add_key(camera: *mut c_void, key: *const c_void) -> c_long {
    let _ = camera;
    // SAFETY: the camera's record
    let locked = unsafe { (*fields()).mLocked };
    if locked {
        return 5;
    }
    // SAFETY: the caller's live key (an EECameraKey: this record's layout)
    keys().push(unsafe { *(key as *const crate::app::camera::EeAppCameraKey) });
    0
}

/// The camera's path keys (the C++'s `EECameraControl::mKeys`, a `std::vector<EECameraKey>` at +0x04 of the
/// world's camera, whose standard-library size moved every later member of gWorld). One camera (gWorld's).
static KEYS: std::sync::Mutex<Vec<crate::app::camera::EeAppCameraKey>> = std::sync::Mutex::new(Vec::new());

pub(crate) fn keys() -> std::sync::MutexGuard<'static, Vec<crate::app::camera::EeAppCameraKey>> {
    KEYS.lock().unwrap_or_else(|e| e.into_inner())
}

/// The camera's key count (`mKeys.size()`: the scenario capture's SMCK).
#[no_mangle]
pub extern "C" fn ee_gamecpp_camera_key_count() -> usize {
    keys().len()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_units_state_bits() {
        assert_eq!(unit_state(false, 10, 0, 0), 0);
        assert_eq!(unit_state(true, 10, 3, 4), EE_APP_CAMERA_EXISTS | EE_APP_CAMERA_ALIVE | EE_APP_CAMERA_ON_MAP);
        assert_eq!(unit_state(true, 0, OFF_MAP, 4), EE_APP_CAMERA_EXISTS);
        assert_eq!(unit_state(true, 1, 4, OFF_MAP), EE_APP_CAMERA_EXISTS | EE_APP_CAMERA_ALIVE);
    }

    #[test]
    fn the_switches_bits() {
        assert_eq!(switches(false, false, false, false, false), 0);
        assert_eq!(
            switches(true, false, true, false, true),
            EE_APP_CAMERA_FREE | EE_APP_CAMERA_CINEMATIC | EE_APP_CAMERA_HAS_MAP
        );
        assert_eq!(switches(false, true, false, true, false), EE_APP_CAMERA_FAR_REAR | EE_APP_CAMERA_ORTHOGRAPHIC);
    }
}
