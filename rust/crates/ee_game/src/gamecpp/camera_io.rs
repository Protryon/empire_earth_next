//! The camera's host (game core lane, round 11, lane B; was EECameraControl.cpp's `EECameraHost`, `EECameraUnit`
//! and `AddKey`): ee_game app camera_ffi.rs's callback answered in Rust, each C++ call or member a one-statement
//! case of EECameraControl.cpp's `EECore10OpD7` ([`super::EeGameCpp`]'s `Ec` and `Dk` ops). The context is the
//! camera; a unit is resolved from its id each call (NULL for none or one that no longer resolves).
//!
//! The camera runs on the UI thread and the physics thread's update (the tour draws it; the app capture profile
//! compares CAMU).
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_void};

use crate::app::camera::{
    EeAppCameraFields, EE_APP_CAMERA_ALIVE, EE_APP_CAMERA_CINEMATIC, EE_APP_CAMERA_EXISTS, EE_APP_CAMERA_FAR_REAR,
    EE_APP_CAMERA_FREE, EE_APP_CAMERA_HAS_MAP, EE_APP_CAMERA_NO_UNIT, EE_APP_CAMERA_ON_MAP, EE_APP_CAMERA_ORTHOGRAPHIC,
    EE_APP_CAMERA_UNIT_A,
};
use crate::app::camera_ffi::*;

use super::camera_host::ee_gamecpp_camera_unit_point;
use super::{cptr, h, hd, hf, hp, ptr, O, P};

/// The tile coordinate of a unit off the map.
const OFF_MAP: c_long = -10000;

/// The camera's followed unit's handle in `slot`: its saved id, or none.
fn slot_unit(c: P, slot: u32) -> u32 {
    let (get, saved) =
        if slot == EE_APP_CAMERA_UNIT_A { (O::EcUnitAGet, O::DkUnitASaved) } else { (O::EcUnitBGet, O::DkUnitBSaved) };
    if hp(get, c, &[]) != 0 {
        h(saved, c, &[]) as u32
    } else {
        EE_APP_CAMERA_NO_UNIT
    }
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
    let c = context as P;
    // SAFETY: the caller's record for the call
    let io = unsafe { &mut *io };
    let unit = if (EE_APP_CAMERA_OP_REFERENCE..=EE_APP_CAMERA_OP_HIDDEN).contains(&op) {
        hp(O::EcResolve, 0, &[i64::from(io.id)])
    } else {
        0
    };
    let complex = if unit != 0 { hp(O::UnComplex, unit, &[]) } else { 0 };
    match op {
        EE_APP_CAMERA_OP_UNIT => io.id = slot_unit(c, io.slot),
        EE_APP_CAMERA_OP_SET_UNIT => {
            h(if io.slot == EE_APP_CAMERA_UNIT_A { O::EcSetUnitA } else { O::EcSetUnitB }, c, &[i64::from(io.id)]);
        }
        EE_APP_CAMERA_OP_REFERENCE => {
            h(O::EcReference, unit, &[]);
        }
        EE_APP_CAMERA_OP_DEREFERENCE => {
            h(O::EcDereference, unit, &[]);
        }
        EE_APP_CAMERA_OP_UNIT_STATE => {
            io.id = if unit == 0 {
                0
            } else {
                unit_state(
                    true,
                    h(O::EcHitPoints, unit, &[]) as c_long,
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
            io.flag = complex != 0 && !on(O::EcInCinematic);
            if io.flag {
                io.x = hd(O::EcVelocity, complex, &[], &[]) * hd(O::EcGameSpeed, 0, &[], &[]);
            }
        }
        EE_APP_CAMERA_OP_HIDDEN => {
            io.flag = unit != 0
                && hp(O::UnPlayer, unit, &[]) != hp(O::EcCurrentPlayer, 0, &[])
                && h(O::EcIsSeen, unit, &[]) == 0;
        }
        EE_APP_CAMERA_OP_KEY_COUNT => io.id = h(O::DkKeyCount, c, &[]) as u32,
        EE_APP_CAMERA_OP_KEY => {
            h(O::DkKeyAt, c, &[i64::from(io.id), ptr(&mut io.key)]);
        }
        EE_APP_CAMERA_OP_CLEAR_KEYS => {
            h(O::EcClearKeys, c, &[]);
        }
        EE_APP_CAMERA_OP_FLAGS => {
            io.id = switches(
                crate::gamecpp::globals::get!(gFreeCamera),
                on(O::EcFarRearClipping),
                on(O::EcInCinematic),
                on(O::EcOrthographic),
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
            io.x = hd(O::EcViewportXCenter, 0, &[], &[]);
            io.y = hd(O::EcViewportYCenter, 0, &[], &[]);
            io.z = hd(O::EcViewportHalfHeight, 0, &[], &[]);
        }
        EE_APP_CAMERA_OP_VIEWPORT_CAMERA => {
            io.point.x = hd(O::EcViewportCameraX, 0, &[], &[]);
            io.point.y = hd(O::EcViewportCameraY, 0, &[], &[]);
            io.point.z = hd(O::EcViewportCameraZ, 0, &[], &[]);
        }
        EE_APP_CAMERA_OP_FRONT_CLIPPING => io.x = hd(O::DkFrontClipping, 0, &[], &[]),
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
    let c = camera as P;
    // SAFETY: the camera's record, read before the C++ call
    let locked = unsafe { (*(hp(O::DkFields, c, &[]) as *const EeAppCameraFields)).mLocked };
    if locked {
        return 5;
    }
    h(O::EcPushKey, c, &[key as usize as i64]);
    0
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
