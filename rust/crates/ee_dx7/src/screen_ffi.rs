//! `extern "C"` for `DX7Screen` (`Common/DX7Screen.cpp`, the forwarders). `screen` is the `DX7Screen*`, `dd` its
//! `GXDirectDraw*`.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_ulong, c_void};

use crate::api::*;
use crate::gx::{RawGx, RawHost};
use crate::screen::{self, CaptureState, ScreenHost, VideoMode, WindowParams};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_ulong, c_void};

    use crate::api::SurfaceDescription;
    use crate::screen::VideoMode;

    extern "C" {
        pub fn ee_dx7cpp_screen_new_surface(
            screen: *mut c_void,
            gx: *mut c_void,
            d: *const SurfaceDescription,
        ) -> *mut c_void;
        pub fn ee_dx7cpp_screen_push_attached(screen: *mut c_void, parent: *mut c_void, child: *mut c_void);
        pub fn ee_dx7cpp_screen_surface_gx(surface: *mut c_void) -> *mut c_void;
        pub fn ee_dx7cpp_screen_surface_dereference(surface: *mut c_void);
        pub fn ee_dx7cpp_screen_set_filter_caps(screen: *mut c_void, min: bool, mag: bool);
        pub fn ee_dx7cpp_screen_push_mode(screen: *mut c_void, mode: *const VideoMode);
        pub fn ee_dx7cpp_screen_set_current_mode(screen: *mut c_void, index: c_ulong);
        pub fn ee_dx7cpp_screen_create_probe_window(screen: *mut c_void, out: *mut *mut c_void) -> i32;
        pub fn ee_dx7cpp_screen_capture(screen: *mut c_void, window: *mut c_void) -> i32;
        pub fn ee_dx7cpp_screen_release(screen: *mut c_void, window: *mut c_void) -> i32;
        pub fn ee_dx7cpp_screen_delete_window(window: *mut c_void);
        pub fn ee_dx7cpp_screen_dispatch_events();
        pub fn ee_dx7cpp_screen_mode_count(screen: *mut c_void) -> c_ulong;
        pub fn ee_dx7cpp_screen_mode(screen: *mut c_void, index: c_ulong, out: *mut VideoMode);
        pub fn ee_dx7cpp_screen_set_32bit_support(screen: *mut c_void, device: bool, texture: bool);
    }
}

use sys::*;

/// The C++ screen, only as an opaque pointer.
struct CppScreen(H);

impl ScreenHost for CppScreen {
    fn new_surface(&mut self, gx_surface: H, desc: &SurfaceDescription) -> H {
        unsafe { ee_dx7cpp_screen_new_surface(self.0, gx_surface, desc) }
    }
    fn push_attached(&mut self, parent: H, child: H) {
        unsafe { ee_dx7cpp_screen_push_attached(self.0, parent, child) }
    }
    fn surface_gx(&mut self, surface: H) -> H {
        unsafe { ee_dx7cpp_screen_surface_gx(surface) }
    }
    fn surface_dereference(&mut self, surface: H) {
        unsafe { ee_dx7cpp_screen_surface_dereference(surface) }
    }
    fn set_filter_caps(&mut self, min_filter: bool, mag_filter: bool) {
        unsafe { ee_dx7cpp_screen_set_filter_caps(self.0, min_filter, mag_filter) }
    }
    fn push_mode(&mut self, mode: VideoMode) {
        unsafe { ee_dx7cpp_screen_push_mode(self.0, &mode) }
    }
    fn set_current_mode(&mut self, index: u32) {
        unsafe { ee_dx7cpp_screen_set_current_mode(self.0, index as c_ulong) }
    }
    fn create_probe_window(&mut self) -> (i32, H) {
        let mut window: H = NULL;
        let result = unsafe { ee_dx7cpp_screen_create_probe_window(self.0, &mut window) };
        (result, window)
    }
    fn capture_window<G: Gx, O: Host>(&mut self, _gx: &mut G, _host: &mut O, window: H) -> i32 {
        unsafe { ee_dx7cpp_screen_capture(self.0, window) }
    }
    fn release_window<G: Gx, O: Host>(&mut self, _gx: &mut G, _host: &mut O, window: H) -> i32 {
        unsafe { ee_dx7cpp_screen_release(self.0, window) }
    }
    fn delete_window(&mut self, window: H) {
        unsafe { ee_dx7cpp_screen_delete_window(window) }
    }
    fn dispatch_window_events(&mut self) {
        unsafe { ee_dx7cpp_screen_dispatch_events() }
    }
    fn mode_count(&mut self) -> u32 {
        unsafe { ee_dx7cpp_screen_mode_count(self.0) as u32 }
    }
    fn mode(&mut self, index: u32) -> VideoMode {
        let mut mode = VideoMode::default();
        unsafe { ee_dx7cpp_screen_mode(self.0, index as c_ulong, &mut mode) };
        mode
    }
    fn set_32bit_support(&mut self, device: bool, texture: bool) {
        unsafe { ee_dx7cpp_screen_set_32bit_support(self.0, device, texture) }
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7sc_initialize(screen: *mut c_void, dd: *mut c_void) {
    screen::initialize(&mut RawGx, &mut CppScreen(screen), dd);
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7sc_test_32bit_support(screen: *mut c_void, dd: *mut c_void) {
    screen::test_32bit_support(&mut RawGx, &mut RawHost, &mut CppScreen(screen), dd);
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7sc_capture(
    dd: *mut c_void,
    window: *mut c_void,
    to_clipboard: bool,
    attached: bool,
    state: *mut CaptureState,
    full_screen: *mut bool,
) -> i32 {
    let (mut s, mut f) = unsafe { (*state, *full_screen) };
    let r = screen::capture(&mut RawGx, &mut RawHost, dd, window, to_clipboard, attached, &mut s, &mut f);
    unsafe {
        *state = s;
        *full_screen = f;
    }
    r
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7sc_release(
    dd: *mut c_void,
    window: *mut c_void,
    state: *mut CaptureState,
    full_screen: *mut bool,
) -> i32 {
    let (mut s, mut f) = unsafe { (*state, *full_screen) };
    let r = screen::release(&mut RawGx, &mut RawHost, dd, window, &mut s, &mut f);
    unsafe {
        *state = s;
        *full_screen = f;
    }
    r
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7sc_restore_all(dd: *mut c_void) -> i32 {
    screen::restore_all(&mut RawGx, &mut RawHost, dd)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7sc_set_mode(
    dd: *mut c_void,
    full_screen: bool,
    mode: *const VideoMode,
    update: *mut bool,
) -> i32 {
    let mode = unsafe { *mode }; // a copy: no reference across the GX call
    let (result, updated) = screen::set_mode(&mut RawGx, &mut RawHost, dd, full_screen, &mode);
    unsafe { *update = updated };
    result
}

/// `DX7Screen::CreateSurface`: the description is copied in and out (the C++ reference argument's updates).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7sc_create_surface(
    screen: *mut c_void,
    dd: *mut c_void,
    description: *mut SurfaceDescription,
    out: *mut *mut c_void,
) -> i32 {
    let mut local = unsafe { *description };
    let mut surface = NULL;
    let result = screen::create_surface(&mut RawGx, &mut RawHost, &mut CppScreen(screen), dd, &mut local, &mut surface);
    unsafe {
        *description = local;
        *out = surface;
    }
    result
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7sc_window_params(
    sim: bool,
    style: u32,
    width: i32,
    height: i32,
    display_width: i32,
    display_height: i32,
    x: f32,
    y: f32,
    out: *mut WindowParams,
) {
    unsafe { *out = screen::window_params(sim, style, width, height, display_width, display_height, x, y) };
}
