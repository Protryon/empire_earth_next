//! `extern "C"` for the rasterizer's draw paths: the thin forwarders in `Common/DX7Rasterizer Draw*.cpp` call the
//! `ee_dx7r_draw_*` functions; the steps that touch C++ objects come back through the `ee_dx7cpp_*` seam (declared
//! here, defined in `Common/DX7DrawSeam.cpp` and the Draw units). `rast` is a `DX7Rasterizer*`, `screen` the screen
//! index, `state` its pinned `DX7RScreenState*`. The Host holds only the raw `state` pointer; every access is a short
//! field read or write, never held across a call into C++.
#![allow(clippy::missing_safety_doc, clippy::unnecessary_cast)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::draw::*;
use crate::draw2d::{self, Draw2DHost, TextHost, TextInfo, TextRect};
use crate::gx::{yield_now, RawDevice};
use crate::state::ScreenState;
use crate::vertex::Dx7StdColorVertex;

/// cbindgen:ignore
mod cpp {
    use super::*;

    extern "C" {
        pub fn ee_dx7cpp_enter(rast: *mut c_void);
        pub fn ee_dx7cpp_leave(rast: *mut c_void);
        pub fn ee_dx7cpp_set_2d_mode(rast: *mut c_void, screen: c_ulong);
        pub fn ee_dx7cpp_set_3d_mode(rast: *mut c_void, screen: c_ulong);
        pub fn ee_dx7cpp_set_texture(rast: *mut c_void, texture: Handle, screen: c_ulong);
        pub fn ee_dx7cpp_set_shading_color_source(rast: *mut c_void, source: c_ulong, screen: c_ulong);
        pub fn ee_dx7cpp_lock_vb(rast: *mut c_void, count: c_ulong, screen: c_ulong) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_unlock_vb(rast: *mut c_void, screen: c_ulong) -> c_long;
        pub fn ee_dx7cpp_add_triangles(rast: *mut c_void, screen: c_ulong, count: c_ulong);

        pub fn ee_dx7cpp_bitmap_texture(bitmap: Handle, screen: c_ulong) -> Handle;
        pub fn ee_dx7cpp_bitmap_vertices(bitmap: Handle) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_line_texture(lines: Handle, screen: c_ulong) -> Handle;
        pub fn ee_dx7cpp_line_vertices(lines: Handle, index: c_ulong) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_point_texture(points: Handle, screen: c_ulong) -> Handle;
        pub fn ee_dx7cpp_point_vertices(points: Handle, index: c_ulong) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_rhombus_vertices(rhombus: Handle) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_texture_frame(texture: Handle, out_size: *mut f32, out_corner: *mut f32);

        pub fn ee_dx7cpp_text_measure(font: *const c_void, text: *const c_char, length: u32) -> i32;
        pub fn ee_dx7cpp_text_scaled(font: *const c_void, scale: f32) -> *const c_void;
        pub fn ee_dx7cpp_font_height(font: *const c_void) -> i32;
        pub fn ee_dx7cpp_text_draw(
            font: *const c_void,
            text: *const c_char,
            length: u32,
            x: i32,
            y: i32,
            color: u32,
            opaque: i32,
            background: u32,
            clip: *const i32,
            target: *const c_void,
        );
    }
}
use cpp::*;

/// The game's host: one rasterizer, one screen.
pub(crate) struct Host {
    pub rast: *mut c_void,
    pub screen: c_ulong,
    pub state: *mut ScreenState,
}

impl Host {
    fn device(&self) -> RawDevice {
        RawDevice(unsafe { (*self.state).mDevice })
    }

    fn vb(&self) -> *mut c_void {
        unsafe { (*self.state).mVertexBuffer }
    }
}

impl DrawHost for Host {
    fn enter(&mut self) {
        unsafe { ee_dx7cpp_enter(self.rast) };
    }
    fn leave(&mut self) {
        unsafe { ee_dx7cpp_leave(self.rast) };
    }
    fn set_2d_mode(&mut self) {
        unsafe { ee_dx7cpp_set_2d_mode(self.rast, self.screen) };
    }
    fn set_3d_mode(&mut self) {
        unsafe { ee_dx7cpp_set_3d_mode(self.rast, self.screen) };
    }
    fn set_texture(&mut self, texture: Handle) {
        unsafe { ee_dx7cpp_set_texture(self.rast, texture, self.screen) };
    }
    fn set_shading_color_source(&mut self, source: u32) {
        unsafe { ee_dx7cpp_set_shading_color_source(self.rast, source as c_ulong, self.screen) };
    }
    fn lock_vb(&mut self, count: u32) -> *mut Dx7StdColorVertex {
        unsafe { ee_dx7cpp_lock_vb(self.rast, count as c_ulong, self.screen) }
    }
    fn unlock_vb(&mut self) -> LongResult {
        unsafe { ee_dx7cpp_unlock_vb(self.rast, self.screen) }
    }
    fn vb_position(&self) -> u32 {
        unsafe { (*self.state).mVertexBufferPosition as u32 }
    }
    fn advance_vb(&mut self, count: u32) {
        unsafe { (*self.state).mVertexBufferPosition += count as c_ulong };
    }
    fn draw_primitive(&mut self, primitive: u32, start: u32, count: u32) -> i32 {
        self.device().draw_primitive(primitive, self.vb(), start, count)
    }
    fn draw_indexed(&mut self, primitive: u32, start: u32, count: u32, indices: *const u16, n: u32) -> i32 {
        self.device().draw_indexed(primitive, self.vb(), start, count, indices, n)
    }
    fn set_transform(&mut self, kind: u32, matrix: &Matrix) -> LongResult {
        self.device().set_transform(kind, matrix)
    }
    fn set_render_state(&mut self, state: u32, value: u32) -> LongResult {
        self.device().set_render_state_raw(state, value)
    }
    fn set_material(&mut self, material: &Material) {
        self.device().set_material(material as *const Material as *const c_void);
    }
    fn yield_now(&mut self) {
        yield_now();
    }
    fn add_triangles(&mut self, count: u32) {
        unsafe { ee_dx7cpp_add_triangles(self.rast, self.screen, count as c_ulong) };
    }
}

impl Draw2DHost for Host {
    fn bitmap_texture(&mut self, bitmap: Handle) -> Handle {
        unsafe { ee_dx7cpp_bitmap_texture(bitmap, self.screen) }
    }
    fn bitmap_vertices(&mut self, bitmap: Handle) -> *mut Dx7StdColorVertex {
        unsafe { ee_dx7cpp_bitmap_vertices(bitmap) }
    }
    fn line_texture(&mut self, lines: Handle) -> Handle {
        unsafe { ee_dx7cpp_line_texture(lines, self.screen) }
    }
    fn line_vertices(&mut self, lines: Handle, index: u32) -> *mut Dx7StdColorVertex {
        unsafe { ee_dx7cpp_line_vertices(lines, index as c_ulong) }
    }
    fn point_texture(&mut self, points: Handle) -> Handle {
        unsafe { ee_dx7cpp_point_texture(points, self.screen) }
    }
    fn point_vertices(&mut self, points: Handle, index: u32) -> *mut Dx7StdColorVertex {
        unsafe { ee_dx7cpp_point_vertices(points, index as c_ulong) }
    }
    fn rhombus_vertices(&mut self, rhombus: Handle) -> *mut Dx7StdColorVertex {
        unsafe { ee_dx7cpp_rhombus_vertices(rhombus) }
    }
    fn texture_frame(&mut self, texture: Handle) -> (f32, [f32; 2]) {
        let mut size = 0.0;
        let mut corner = [0.0; 2];
        unsafe { ee_dx7cpp_texture_frame(texture, &mut size, corner.as_mut_ptr()) };
        (size, corner)
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_bitmap(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    bitmap: Handle,
) -> c_long {
    draw2d::draw_bitmap(&mut Host { rast, screen, state }, bitmap)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_lines(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    lines: Handle,
    count: c_ulong,
) -> c_long {
    draw2d::draw_lines(&mut Host { rast, screen, state }, lines, count as u32)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_points(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    points: Handle,
    count: c_ulong,
) -> c_long {
    draw2d::draw_points(&mut Host { rast, screen, state }, points, count as u32)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_rhombus(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    rhombus: Handle,
) -> c_long {
    draw2d::draw_rhombus(&mut Host { rast, screen, state }, rhombus)
}

struct TextPlat {
    target: *const c_void,
}

impl TextHost for TextPlat {
    fn measure(&mut self, font: *const c_void, text: *const c_char, length: u32) -> i32 {
        unsafe { ee_dx7cpp_text_measure(font, text, length) }
    }
    fn scaled(&mut self, font: *const c_void, scale: f32) -> *const c_void {
        unsafe { ee_dx7cpp_text_scaled(font, scale) }
    }
    fn font_height(&mut self, font: *const c_void) -> i32 {
        unsafe { ee_dx7cpp_font_height(font) }
    }
    fn draw(
        &mut self,
        font: *const c_void,
        text: *const c_char,
        length: u32,
        x: i32,
        y: i32,
        color: u32,
        opaque: bool,
        background: u32,
        clip: &draw2d::TextClip,
    ) {
        unsafe {
            ee_dx7cpp_text_draw(font, text, length, x, y, color, opaque as i32, background, clip.as_ptr(), self.target)
        };
    }
}

/// `DX7TextBounds`: `clip` may be null.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_text_bounds(info: *const TextInfo, clip: *const TextRect, out: *mut TextRect) {
    let bounds =
        draw2d::text_bounds(&mut TextPlat { target: core::ptr::null() }, unsafe { &*info }, unsafe { clip.as_ref() });
    unsafe { *out = bounds };
}

/// `DX7DrawTextPlat` (the `GXSurface::DrawText` callback). `target` is the `PlatFontTarget*`. Returns false when the
/// font has no OS font.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_text_draw(
    info: *const TextInfo,
    clip: *const TextRect,
    origin_x: c_long,
    origin_y: c_long,
    target_width: i32,
    target_height: i32,
    target_scale: f32,
    target: *const c_void,
) -> bool {
    draw2d::text_draw(
        &mut TextPlat { target },
        unsafe { &*info },
        unsafe { clip.as_ref() },
        origin_x,
        origin_y,
        target_width,
        target_height,
        target_scale,
    )
}
