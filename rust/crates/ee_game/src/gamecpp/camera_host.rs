//! The camera's C++ remainder (game core lane, round 10, lane D; was EECameraControl.cpp's `GetUnitPoint` and
//! `Update`'s tail, and EEAppCapture.cpp's CAMU record): the point a camera focuses on for a unit, the frame's
//! update (ee_game app camera.rs's over the camera's host) and its view, orthographic size, field of view, clipping
//! distances and mini map applied, and the TEST ONLY record of the camera's whole state at the update's start and
//! end. Each C++ call or member is a one-statement case of EECameraControl.cpp's `EECore10OpD7` ([`super::EeGameCpp`]'s
//! `Dk` ops).
//!
//! The camera runs on the UI thread (the tour exercises it; the app capture profile compares CAMU).
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_void};

use ee_base::math::types::Ge3DPoint;
use ee_sim::app::capture_record::{self, Record};

use crate::app::camera::{EeAppCameraFields, EeAppCameraKey, EE_APP_CAMERA_UPDATE_DONE, EE_APP_CAMERA_UPDATE_ORTHO};
use crate::app::camera_ffi::{ee_app_camera_set_clipping_distances, ee_app_camera_update, EeAppCameraFn};

use super::{cptr, h, hd, hf, hp, ptr, O, P};

fn fields(c: P) -> *mut EeAppCameraFields {
    hp(O::DkFields, c, &[]) as *mut EeAppCameraFields
}

/// `GetUnitPoint`: a unit's model centre at its camera-focus height (scaled, at its location), else its tile's
/// centre just above the terrain.
#[no_mangle]
pub extern "C" fn ee_gamecpp_camera_unit_point(unit: *mut c_void, out: *mut c_void) {
    let u = unit as P;
    let cx = hp(O::UnComplex, u, &[]);
    if cx != 0 {
        let scale = hd(O::DkModelScale, cx, &[], &[]);
        let mut offset = Ge3DPoint::default();
        h(O::DkModelCenter, cx, &[ptr(&mut offset)]);
        offset.x = offset.x * scale;
        offset.y = offset.y * scale;
        offset.z = hd(O::DkModelHeight, cx, &[], &[]) * hd(O::DkCameraFocus, cx, &[], &[]) * scale;
        h(O::DkLocationApply, cx, &[cptr(&offset), out as usize as i64]);
    } else {
        let x = h(O::DkTileX, u, &[]) as c_long as f32 + 0.5;
        let y = 0.5 + h(O::DkTileY, u, &[]) as c_long as f32;
        let z = hd(O::MeshZ, 0, &[], &[x, y]) + 0.3;
        // SAFETY: the caller's GE3DPoint
        unsafe { *(out as *mut Ge3DPoint) = Ge3DPoint::new(x, y, z) };
    }
}

/// `Update`: the camera's frame (ee_game app camera.rs over the host, camera_io.rs); when it moved, the view to the
/// world and the viewport (the orthographic size, the field of view, the clipping distances) and the mini map's
/// view; CAMU before and after (TEST ONLY).
///
/// # Safety
/// `camera` is a live EECameraControl.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_camera_update(camera: *mut c_void, elapsed: f32) {
    let c = camera as P;
    let call: EeAppCameraFn = super::camera_io::ee_gamecpp_camera_host;
    ee_gamecpp_capture_camera(camera, 0, elapsed);
    let mut size = 0.0f32;
    // SAFETY: the function's contract
    let action = unsafe { ee_app_camera_update(fields(c), camera, call, elapsed, &mut size) };
    if action != EE_APP_CAMERA_UPDATE_DONE {
        if action == EE_APP_CAMERA_UPDATE_ORTHO {
            hf(O::DkSetOrtho, 0, &[], &[-size, -size, size, size]);
        }
        h(O::DkSetWorldCamera, c, &[]);
        h(O::DkSetFieldOfView, 0, &[]);
        // SAFETY: as above
        unsafe { ee_app_camera_set_clipping_distances(fields(c), camera, call) };
        if crate::gamecpp::globals::have!(gMiniMap) {
            h(O::DkMiniMapRefresh, 0, &[]);
        }
        h(O::DkClearB529066, 0, &[]);
        h(O::DkClearFollowing, 0, &[]);
        h(O::DkClearB529068, 0, &[]);
    }
    ee_gamecpp_capture_camera(camera, 1, elapsed);
}

/// A plain value's floats, in order.
fn floats<T>(r: &mut Record, v: &T) {
    // SAFETY: T is a repr(C) value of f32s only
    let all = unsafe { std::slice::from_raw_parts(v as *const T as *const f32, std::mem::size_of::<T>() / 4) };
    for &f in all {
        r.f32(f);
    }
}

/// The record's flags: changed, following unit B, locked.
pub fn camera_flags(c: &EeAppCameraFields) -> u32 {
    (c.mChanged as u32) | (c.mIsFollowingUnitB as u32) << 1 | (c.mLocked as u32) << 2
}

/// CAMU (TEST ONLY): the camera's whole state (the poses, the view, the target, the followed units' ids, the move's
/// numbers, the mode and flags, every key of the path) and the viewport's clipping distances, at each update's start
/// (phase 0, with its elapsed time) and end (1).
#[no_mangle]
pub extern "C" fn ee_gamecpp_capture_camera(camera: *mut c_void, phase: u32, elapsed: f32) {
    if !capture_record::active() {
        return;
    }
    let cam = camera as P;
    // SAFETY: the camera's record, copied
    let c = unsafe { *fields(cam) };
    let mut r = Record::new(u32::from_le_bytes(*b"CAMU"));
    r.u32(phase);
    r.f32(elapsed);
    floats(&mut r, &c.mQ014);
    floats(&mut r, &c.mQ024);
    floats(&mut r, &c.mStartOffset);
    floats(&mut r, &c.mStartFocus);
    floats(&mut r, &c.mQ04C);
    floats(&mut r, &c.mQ05C);
    floats(&mut r, &c.mEndOffset);
    floats(&mut r, &c.mEndFocus);
    floats(&mut r, &c.mView);
    floats(&mut r, &c.mTargetPoint);
    r.u32(h(O::DkUnitASaved, cam, &[]) as u32);
    r.u32(h(O::DkUnitBSaved, cam, &[]) as u32);
    for f in [c.mFraction, c.mRate, c.mZoom, c.mWait, c.mWaited, c.mTurnYaw, c.mTurnPitch] {
        r.f32(f);
    }
    r.i32(c.mMode as i32);
    r.i32(c.mPathIndex as i32);
    r.u32(camera_flags(&c));
    let keys = h(O::DkKeyCount, cam, &[]);
    r.u32(keys as u32);
    for i in 0..keys {
        let mut k = EeAppCameraKey::default();
        h(O::DkKeyAt, cam, &[i, ptr(&mut k)]);
        floats(&mut r, &k.mT00);
        floats(&mut r, &k.mT40);
        floats(&mut r, &k.mOffset);
        floats(&mut r, &k.mFocus);
        r.f32(k.mWait);
        r.f32(k.mRate);
        r.u32(k.mChanged as u32);
    }
    r.f32(hd(O::DkFrontClipping, 0, &[], &[]));
    r.f32(hd(O::DkRearClipping, 0, &[], &[]));
    r.commit();
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::math::types::GeTransformation;

    #[test]
    fn the_floats_are_the_values_in_order() {
        let mut r = Record::new(0);
        floats(&mut r, &Ge3DPoint::new(1.0, 2.0, 3.0));
        assert_eq!(r.data.len(), 12);
        assert_eq!(&r.data[4..8], &2.0f32.to_le_bytes());
        let mut r = Record::new(0);
        floats(&mut r, &GeTransformation::IDENTITY);
        assert_eq!(r.data.len(), 64);
    }
}
