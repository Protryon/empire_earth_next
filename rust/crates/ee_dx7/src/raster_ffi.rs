//! `extern "C"` for the rasterizers' Rust parts. `state` is the C++ `DX7RScreenState*` (the pinned record); `dd`
//! the screen's `GXDirectDraw*`; `target` a `GXSurface*`.
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::gx::{RawGx, RawHost};
use crate::raster::{self, Flavor};
use crate::state::ScreenState;

fn flavor(tnl: bool) -> Flavor {
    if tnl {
        Flavor::Tnl
    } else {
        Flavor::Hal
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_activate_the_screen(
    state: *mut ScreenState,
    dd: *mut c_void,
    target: *mut c_void,
    width: u32,
    height: u32,
    tnl: bool,
) -> i32 {
    let state = unsafe { &mut *state };
    raster::activate_the_screen(&mut RawGx, &mut RawHost, state, dd, target, width, height, flavor(tnl))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_create_vertex_buffers(state: *mut ScreenState, tnl: bool) -> i32 {
    let state = unsafe { &mut *state };
    raster::create_vertex_buffers(&mut RawGx, &mut RawHost, state, flavor(tnl))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_is_screen_valid(dd: *mut c_void, tnl: bool) -> bool {
    raster::is_screen_valid(&mut RawGx, &mut RawHost, dd, flavor(tnl))
}
