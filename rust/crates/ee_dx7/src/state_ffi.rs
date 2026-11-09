//! `extern "C"` for the state setters, called by `DX7Rasterizer`'s C++ bodies: each returns whether the C++ base class
//! must record the new value. `device` is a `GXDevice*`, `state` the C++ `DX7RScreenState*` (the pinned record).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_ulong, c_void};

use crate::gx::RawDevice;
use crate::state::{self, ScreenState, StageView};

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_alpha_testing(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_alpha_testing(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_alpha_testing_compare_state(
    device: *mut c_void,
    in_state: c_ulong,
    current: c_ulong,
) -> bool {
    state::set_alpha_testing_compare_state(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_alpha_testing_ref_value(device: *mut c_void, value: f32, current: f32) -> bool {
    state::set_alpha_testing_ref_value(&mut RawDevice(device), value, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_anti_aliasing(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_anti_aliasing(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_color_keying(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_color_keying(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_dithering(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_dithering(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_draw_style(device: *mut c_void, in_style: c_ulong, current: c_ulong) -> bool {
    state::set_draw_style(&mut RawDevice(device), in_style, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_fog(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_fog(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_lighting(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_lighting(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_shading(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_shading(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_shading_color_source(
    device: *mut c_void,
    in_source: c_ulong,
    current: c_ulong,
) -> bool {
    state::set_shading_color_source(&mut RawDevice(device), in_source, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_specular(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_specular(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_z_compare(device: *mut c_void, in_state: c_ulong, current: c_ulong) -> bool {
    state::set_z_compare(&mut RawDevice(device), in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_blending(
    state: *mut ScreenState,
    in_source: c_ulong,
    in_destination: c_ulong,
    current_source: c_ulong,
    current_destination: c_ulong,
) -> bool {
    let state = unsafe { &mut *state };
    let mut device = RawDevice(state.mDevice);
    state::set_blending(state, &mut device, in_source, in_destination, current_source, current_destination)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_z_buffer(state: *mut ScreenState, in_state: c_ulong, current: c_ulong) -> bool {
    let state = unsafe { &mut *state };
    let mut device = RawDevice(state.mDevice);
    state::set_z_buffer(state, &mut device, in_state, current)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_fog_color(device: *mut c_void, rgb: *const f32, current_equal: bool) -> bool {
    let rgb = unsafe { [*rgb, *rgb.add(1), *rgb.add(2)] };
    state::set_fog_color(&mut RawDevice(device), rgb, current_equal)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_fog_range(
    device: *mut c_void,
    start: f32,
    end: f32,
    current_start: f32,
    current_end: f32,
) {
    state::set_fog_range(&mut RawDevice(device), start, end, current_start, current_end);
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_cull_mode(device: *mut c_void, in_state: c_ulong) -> i32 {
    state::set_cull_mode(&mut RawDevice(device), in_state)
}

/// `SetAmbientColor`/`SetAmbientIntensity`: `computed` is `mComputedAmbientColor` (r,g,b); the value sent to the device.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_ambient(device: *mut c_void, computed: *const f32) -> i32 {
    let rgb = unsafe { [*computed, *computed.add(1), *computed.add(2)] };
    state::set_ambient(&mut RawDevice(device), rgb)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_debug_color(rgb: *const f32) -> u32 {
    state::debug_color(unsafe { [*rgb, *rgb.add(1), *rgb.add(2)] })
}

/// `SetTexture`'s NULL path.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_clear_texture(state: *mut ScreenState) {
    let state = unsafe { &mut *state };
    let mut device = RawDevice(state.mDevice);
    state::clear_texture(state, &mut device);
}

/// `SetTexture`'s stage-0 diff and stage 1 off (after the C++ reference counting).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_apply_texture_stage(state: *mut ScreenState, view: *const StageView) {
    let state = unsafe { &mut *state };
    let mut device = RawDevice(state.mDevice);
    state::apply_texture_stage(state, &mut device, unsafe { &*view });
}
