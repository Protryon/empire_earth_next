//! `extern "C"` for the rasterizers' Rust parts. `state` is the C++ `DX7RScreenState*` (the pinned record); `dd`
//! the screen's `GXDirectDraw*`; `target` a `GXSurface*`.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_void};

use crate::api::*;
use crate::gx::{direct_draw_enumerate, RawGx, RawHost};
use crate::raster::{self, EnumerationHost, Flavor, FrameStats, LightParams, RasterHost};
use crate::screen::VideoMode;
use crate::state::ScreenState;

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_char, c_ulong, c_void};

    use crate::api::SurfaceDescription;
    use crate::screen::VideoMode;

    extern "C" {
        pub fn ee_dx7cpp_rast_is_full_screen(ctx: *mut c_void) -> bool;
        pub fn ee_dx7cpp_rast_current_mode(ctx: *mut c_void, out: *mut VideoMode);
        pub fn ee_dx7cpp_rast_mode_count(ctx: *mut c_void) -> c_ulong;
        pub fn ee_dx7cpp_rast_mode(ctx: *mut c_void, index: c_ulong, out: *mut VideoMode);
        pub fn ee_dx7cpp_rast_create_surface(
            ctx: *mut c_void,
            d: *mut SurfaceDescription,
            out: *mut *mut c_void,
        ) -> i32;
        pub fn ee_dx7cpp_rast_attached_surface(surface: *mut c_void) -> *mut c_void;
        pub fn ee_dx7cpp_rast_attach_surface(surface: *mut c_void, attached: *mut c_void) -> i32;
        pub fn ee_dx7cpp_rast_dereference(surface: *mut c_void);
        pub fn ee_dx7cpp_rast_activate_the_screen(
            ctx: *mut c_void,
            primary: *mut c_void,
            secondary: *mut c_void,
        ) -> i32;
        pub fn ee_dx7cpp_rast_commit_activation(ctx: *mut c_void, primary: *mut c_void, secondary: *mut c_void);
        pub fn ee_dx7cpp_rast_apply_defaults(ctx: *mut c_void);
        pub fn ee_dx7cpp_rast_verify(ok: bool, line: u32);
        pub fn ee_dx7cpp_rast_new_screen(
            r: *mut c_void,
            dd: *mut c_void,
            name: *const c_char,
            attached: bool,
            primary: bool,
        ) -> *mut c_void;
        pub fn ee_dx7cpp_rast_screen_is_valid(r: *mut c_void, screen: *mut c_void) -> bool;
        pub fn ee_dx7cpp_rast_add_valid_screen(r: *mut c_void, screen: *mut c_void);
        pub fn ee_dx7cpp_rast_delete_screen(screen: *mut c_void);
        pub fn ee_dx7cpp_rast_set_active(r: *mut c_void);
        pub fn ee_dx7cpp_rast_flip(primary: *mut c_void, wait: bool) -> i32;
    }
}

use sys::*;

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
    // no reference to the C++ record across the host calls: a copy, written back
    let mut local = unsafe { *state };
    let result =
        raster::activate_the_screen(&mut RawGx, &mut RawHost, &mut local, dd, target, width, height, flavor(tnl));
    unsafe { *state = local };
    result
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_create_vertex_buffers(state: *mut ScreenState, tnl: bool) -> i32 {
    let mut local = unsafe { *state };
    let result = raster::create_vertex_buffers(&mut RawGx, &mut RawHost, &mut local, flavor(tnl));
    unsafe { *state = local };
    result
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_is_screen_valid(dd: *mut c_void, tnl: bool) -> bool {
    raster::is_screen_valid(&mut RawGx, &mut RawHost, dd, flavor(tnl))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_begin_scene(device: *mut c_void) -> i32 {
    raster::begin_scene(&mut RawGx, &mut RawHost, device)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_end_scene(device: *mut c_void) -> i32 {
    raster::end_scene(&mut RawGx, &mut RawHost, device)
}

unsafe fn floats4(p: *const f32) -> [f32; 4] {
    unsafe { [*p, *p.add(1), *p.add(2), *p.add(3)] }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_clear(device: *mut c_void, rect: *const f32, rgb: *const f32) -> i32 {
    let color = unsafe { [*rgb, *rgb.add(1), *rgb.add(2)] };
    raster::clear(&mut RawGx, &mut RawHost, device, unsafe { floats4(rect) }, color)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_clear_z(device: *mut c_void, rect: *const f32, z: f32) -> i32 {
    raster::clear_z(&mut RawGx, &mut RawHost, device, unsafe { floats4(rect) }, z)
}

/// `LockVB`: `out` receives the vertex pointer; the result is always OK, as in the C++.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_lock_vb(state: *mut ScreenState, count: u32, out: *mut *mut c_void) -> i32 {
    let mut local = unsafe { *state };
    let pointer = raster::lock_vb(&mut RawGx, &mut RawHost, &mut local, count);
    unsafe {
        *state = local;
        *out = pointer;
    }
    K_OK
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_unlock_vb(buffer: *mut c_void) -> i32 {
    raster::unlock_vb(&mut RawGx, &mut RawHost, buffer)
}

/// `in_use` is `gLightsInUse`, 8 slots.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_add_light(
    device: *mut c_void,
    in_use: *mut core::ffi::c_ulong,
    params: *const LightParams,
    out_index: *mut u32,
) -> i32 {
    let in_use = unsafe { &mut *(in_use as *mut [core::ffi::c_ulong; raster::LIGHT_SLOTS]) };
    let (result, index) = raster::add_light(&mut RawGx, device, in_use, unsafe { &*params });
    unsafe { *out_index = index };
    result
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_update_light(
    device: *mut c_void,
    in_use: *const core::ffi::c_ulong,
    index: u32,
    params: *const LightParams,
) -> i32 {
    let in_use = unsafe { &*(in_use as *const [core::ffi::c_ulong; raster::LIGHT_SLOTS]) };
    raster::update_light(&mut RawGx, device, in_use, index, unsafe { &*params })
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_remove_light(device: *mut c_void, in_use: *mut core::ffi::c_ulong, index: u32) -> i32 {
    let in_use = unsafe { &mut *(in_use as *mut [core::ffi::c_ulong; raster::LIGHT_SLOTS]) };
    raster::remove_light(&mut RawGx, device, in_use, index)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_remove_all_lights(device: *mut c_void, in_use: *mut core::ffi::c_ulong) -> i32 {
    let in_use = unsafe { &mut *(in_use as *mut [core::ffi::c_ulong; raster::LIGHT_SLOTS]) };
    raster::remove_all_lights(&mut RawGx, device, in_use)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_2d_mode(state: *mut ScreenState) -> i32 {
    raster::set_2d_mode(&mut RawGx, unsafe { &mut *state })
}

unsafe fn matrix(p: *const f32) -> [[f32; 4]; 4] {
    let mut out = [[0.0; 4]; 4];
    for (r, row) in out.iter_mut().enumerate() {
        for (c, value) in row.iter_mut().enumerate() {
            *value = unsafe { *p.add(r * 4 + c) };
        }
    }
    out
}

/// `view`/`projection`: the engine transformations as 16 elements, row by row.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_3d_mode(state: *mut ScreenState, view: *const f32, projection: *const f32) -> i32 {
    raster::set_3d_mode(&mut RawGx, unsafe { &mut *state }, unsafe { matrix(view) }, unsafe { matrix(projection) })
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_render_target(
    state: *mut ScreenState,
    target: *mut c_void,
    width: u32,
    height: u32,
) {
    raster::set_render_target(&mut RawGx, unsafe { &mut *state }, target, width, height);
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_set_viewport(state: *mut ScreenState, rect: *const f32, near: f32, far: f32) {
    raster::set_viewport(&mut RawGx, unsafe { &mut *state }, unsafe { floats4(rect) }, near, far);
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_update_frame_stats(stats: *mut FrameStats, time: u32) {
    raster::update_frame_stats(unsafe { &mut *stats }, time);
}

/// `ShowScene`'s presentation; `primary_surface` is the primary's `GESurface*` for the flip.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_show_scene(
    full_screen: bool,
    primary: *mut c_void,
    secondary: *mut c_void,
    secondary_width: u32,
    secondary_height: u32,
    origin_x: f32,
    origin_y: f32,
    primary_surface: *mut c_void,
    wait: bool,
) -> i32 {
    raster::show_scene(
        &mut RawGx,
        &mut RawHost,
        full_screen,
        primary,
        secondary,
        secondary_width,
        secondary_height,
        [origin_x, origin_y],
        || unsafe { ee_dx7cpp_rast_flip(primary_surface, wait) },
    )
}

/// The rasterizer's activation context (`DX7ActivateContext` in DX7Rasterizer.cpp).
struct CppRaster(*mut c_void);

impl RasterHost for CppRaster {
    fn is_full_screen(&mut self) -> bool {
        unsafe { ee_dx7cpp_rast_is_full_screen(self.0) }
    }
    fn current_mode(&mut self) -> VideoMode {
        let mut m = VideoMode::default();
        unsafe { ee_dx7cpp_rast_current_mode(self.0, &mut m) };
        m
    }
    fn mode_count(&mut self) -> u32 {
        unsafe { ee_dx7cpp_rast_mode_count(self.0) as u32 }
    }
    fn mode(&mut self, index: u32) -> VideoMode {
        let mut m = VideoMode::default();
        unsafe { ee_dx7cpp_rast_mode(self.0, index as core::ffi::c_ulong, &mut m) };
        m
    }
    fn create_surface(&mut self, description: &mut SurfaceDescription) -> (i32, H) {
        let mut out = NULL;
        let r = unsafe { ee_dx7cpp_rast_create_surface(self.0, description, &mut out) };
        (r, out)
    }
    fn attached_surface(&mut self, surface: H) -> H {
        unsafe { ee_dx7cpp_rast_attached_surface(surface) }
    }
    fn attach_surface(&mut self, surface: H, attached: H) -> i32 {
        unsafe { ee_dx7cpp_rast_attach_surface(surface, attached) }
    }
    fn dereference(&mut self, surface: H) {
        unsafe { ee_dx7cpp_rast_dereference(surface) }
    }
    fn activate_the_screen(&mut self, primary: H, secondary: H) -> i32 {
        unsafe { ee_dx7cpp_rast_activate_the_screen(self.0, primary, secondary) }
    }
    fn commit_activation(&mut self, primary: H, secondary: H) {
        unsafe { ee_dx7cpp_rast_commit_activation(self.0, primary, secondary) }
    }
    fn apply_defaults(&mut self) {
        unsafe { ee_dx7cpp_rast_apply_defaults(self.0) }
    }
    fn verify(&mut self, ok: bool, line: u32) {
        unsafe { ee_dx7cpp_rast_verify(ok, line) }
    }
}

/// `ActivateScreen`: `context` is a `DX7ActivateContext*`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_activate_screen(
    context: *mut c_void,
    window_width: u32,
    window_height: u32,
    back_buffers: u32,
    z_buffer: bool,
    z_format: u32,
) -> i32 {
    raster::activate_screen(&mut CppRaster(context), window_width, window_height, back_buffers, z_buffer, z_format)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_activate_rendering_surface(context: *mut c_void, width: u32, height: u32) -> i32 {
    raster::activate_rendering_surface(&mut CppRaster(context), width, height)
}

/// `ActivateRasterizer`'s enumeration host: `rasterizer` is the `DX7Rasterizer*`.
struct CppEnumeration(*mut c_void);

impl EnumerationHost for CppEnumeration {
    fn new_screen(&mut self, dd: H, description: &[u8], attached: bool, primary: bool) -> H {
        let mut name = description.to_vec();
        name.push(0);
        unsafe { ee_dx7cpp_rast_new_screen(self.0, dd, name.as_ptr() as *const c_char, attached, primary) }
    }
    fn screen_is_valid(&mut self, screen: H) -> bool {
        unsafe { ee_dx7cpp_rast_screen_is_valid(self.0, screen) }
    }
    fn add_valid_screen(&mut self, screen: H) {
        unsafe { ee_dx7cpp_rast_add_valid_screen(self.0, screen) }
    }
    fn delete_screen(&mut self, screen: H) {
        unsafe { ee_dx7cpp_rast_delete_screen(screen) }
    }
    fn set_active(&mut self) {
        unsafe { ee_dx7cpp_rast_set_active(self.0) }
    }
}

unsafe extern "C" fn device_enumerated(
    guid: *mut c_void,
    description: *mut c_char,
    _name: *mut c_char,
    context: *mut c_void,
    monitor: *mut c_void,
) -> i32 {
    let bytes = unsafe { core::ffi::CStr::from_ptr(description) }.to_bytes().to_vec(); // no reference across the C++ calls
    raster::on_device_enumerated(
        &mut RawGx,
        &mut RawHost,
        &mut CppEnumeration(context),
        guid.is_null(),
        monitor.is_null(),
        &bytes,
    );
    1 // TRUE: keep enumerating
}

/// `DX7Rasterizer::ActivateRasterizer`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_activate_rasterizer(rasterizer: *mut c_void) -> i32 {
    let status = unsafe { direct_draw_enumerate(device_enumerated, rasterizer) };
    raster::finish_activation(&mut RawHost, &mut CppEnumeration(rasterizer), status)
}

/// `GetVideoMemory`.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_get_video_memory(dd: *mut c_void, total: *mut u32, free: *mut u32) -> i32 {
    let (result, t, f) = raster::video_memory(&mut RawGx, dd);
    unsafe {
        *total = t;
        *free = f;
    }
    result
}
