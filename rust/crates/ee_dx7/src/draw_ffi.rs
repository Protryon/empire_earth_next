//! `extern "C"` for the rasterizer's draw paths: the thin forwarders in `Common/DX7Rasterizer Draw*.cpp` call the
//! `ee_dx7r_draw_*` functions; the steps that touch C++ objects come back through the `ee_dx7cpp_*` seam (declared
//! here, defined in `Common/DX7DrawSeam.cpp` and the Draw units). `rast` is a `DX7Rasterizer*`, `screen` the screen
//! index, `state` its pinned `DX7RScreenState*`. The Host holds only the raw `state` pointer; every access is a short
//! field read or write, never held across a call into C++.
#![allow(clippy::missing_safety_doc, clippy::unnecessary_cast)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::draw::*;
use crate::draw2d::{self, Draw2DHost, TextHost, TextInfo, TextRect};
use crate::draw_model::{self, MaterialInfo, ModelHost, ModelInfo, ViewportInfo};
use crate::draw_terrain::{
    self, LightData, MaterialMapEntry, MaterialView, TerrainHost, TerrainMeshView, TerrainPolygon, WaterRipple,
};
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

        pub fn ee_dx7cpp_model_info(model: Handle, out: *mut ModelInfo);
        pub fn ee_dx7cpp_model_visible(
            rast: *mut c_void,
            screen: c_ulong,
            model: Handle,
            scale: f32,
            transform: *const Transform,
            orthographic: bool,
            out_visible: *mut bool,
            out_clipped: *mut bool,
        );
        pub fn ee_dx7cpp_pixel_size(
            rast: *mut c_void,
            screen: c_ulong,
            model: Handle,
            transform: *const Transform,
            scale: f32,
            orthographic: bool,
        ) -> f32;
        pub fn ee_dx7cpp_view_width(rast: *mut c_void, screen: c_ulong) -> f32;
        pub fn ee_dx7cpp_material_info(
            model: Handle,
            screen: c_ulong,
            index: c_ulong,
            lod: c_ulong,
            out: *mut MaterialInfo,
        );
        pub fn ee_dx7cpp_polygon_indices(model: Handle, lod: c_ulong) -> *const u16;
        pub fn ee_dx7cpp_frame_vertices(model: Handle, frame: c_ulong, vertex_type: c_ulong) -> *const c_void;
        pub fn ee_dx7cpp_sub_model(model: Handle, index: c_ulong) -> Handle;

        pub fn ee_dx7cpp_mesh_z(mesh: Handle, x: c_ulong, y: c_ulong) -> f32;
        pub fn ee_dx7cpp_mesh_normal(mesh: Handle, x: c_ulong, y: c_ulong, out: *mut f32);
        pub fn ee_dx7cpp_mesh_vertex_color(mesh: Handle, x: c_ulong, y: c_ulong, out: *mut f32);
        pub fn ee_dx7cpp_mesh_water_color(mesh: Handle, x: c_ulong, y: c_ulong, out: *mut f32);
        pub fn ee_dx7cpp_mesh_valid_tile(mesh: Handle, x: c_ulong, y: c_ulong) -> bool;
        pub fn ee_dx7cpp_light(rast: *mut c_void, screen: c_ulong, index: c_ulong, out: *mut LightData) -> bool;
        pub fn ee_dx7cpp_um_arctan(value: f32) -> f32;
        pub fn ee_dx7cpp_um_tan(value: f32) -> f32;
        pub fn ee_dx7cpp_um_sin(value: f32) -> f32;
        pub fn ee_dx7cpp_um_cos(value: f32) -> f32;

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

impl ModelHost for Host {
    fn model_info(&mut self, model: Handle) -> ModelInfo {
        let mut out = core::mem::MaybeUninit::uninit();
        unsafe {
            ee_dx7cpp_model_info(model, out.as_mut_ptr());
            out.assume_init()
        }
    }
    fn model_visible(&mut self, model: Handle, scale: f32, transform: &Transform, orthographic: bool) -> (bool, bool) {
        let (mut visible, mut clipped) = (false, false);
        unsafe {
            ee_dx7cpp_model_visible(
                self.rast,
                self.screen,
                model,
                scale,
                transform,
                orthographic,
                &mut visible,
                &mut clipped,
            )
        };
        (visible, clipped)
    }
    fn pixel_size(&mut self, model: Handle, transform: &Transform, scale: f32, orthographic: bool) -> f32 {
        unsafe { ee_dx7cpp_pixel_size(self.rast, self.screen, model, transform, scale, orthographic) }
    }
    fn view_width(&mut self) -> f32 {
        unsafe { ee_dx7cpp_view_width(self.rast, self.screen) }
    }
    fn material_info(&mut self, model: Handle, index: u32, lod: u32) -> MaterialInfo {
        let mut out = core::mem::MaybeUninit::uninit();
        unsafe {
            ee_dx7cpp_material_info(model, self.screen, index as c_ulong, lod as c_ulong, out.as_mut_ptr());
            out.assume_init()
        }
    }
    fn polygon_indices(&mut self, model: Handle, lod: u32) -> *const u16 {
        unsafe { ee_dx7cpp_polygon_indices(model, lod as c_ulong) }
    }
    fn frame_vertices(&mut self, model: Handle, frame: u32) -> *const c_void {
        let vertex_type = self.model_info(model).vertex_type;
        unsafe { ee_dx7cpp_frame_vertices(model, frame as c_ulong, vertex_type as c_ulong) }
    }
    fn sub_model(&mut self, model: Handle, index: u32) -> Handle {
        unsafe { ee_dx7cpp_sub_model(model, index as c_ulong) }
    }
}

impl TerrainHost for Host {
    fn mesh_z(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> f32 {
        unsafe { ee_dx7cpp_mesh_z(mesh, x, y) }
    }
    fn mesh_normal(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3] {
        let mut out = [0.0; 3];
        unsafe { ee_dx7cpp_mesh_normal(mesh, x, y, out.as_mut_ptr()) };
        out
    }
    fn mesh_vertex_color(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3] {
        let mut out = [0.0; 3];
        unsafe { ee_dx7cpp_mesh_vertex_color(mesh, x, y, out.as_mut_ptr()) };
        out
    }
    fn mesh_water_color(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3] {
        let mut out = [0.0; 3];
        unsafe { ee_dx7cpp_mesh_water_color(mesh, x, y, out.as_mut_ptr()) };
        out
    }
    fn mesh_valid_tile(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> bool {
        unsafe { ee_dx7cpp_mesh_valid_tile(mesh, x, y) }
    }
    fn mark_viewport_changed(&mut self) {
        unsafe { (*self.state).mViewportChanged = true };
    }
    fn light(&mut self, index: u32) -> Option<LightData> {
        let mut out = LightData::default();
        unsafe { ee_dx7cpp_light(self.rast, self.screen, index as c_ulong, &mut out) }.then_some(out)
    }
    fn um_arctan(&mut self, value: f32) -> f32 {
        unsafe { ee_dx7cpp_um_arctan(value) }
    }
    fn um_tan(&mut self, value: f32) -> f32 {
        unsafe { ee_dx7cpp_um_tan(value) }
    }
    fn um_sin(&mut self, value: f32) -> f32 {
        unsafe { ee_dx7cpp_um_sin(value) }
    }
    fn um_cos(&mut self, value: f32) -> f32 {
        unsafe { ee_dx7cpp_um_cos(value) }
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

/// `Draw(GEPolygonArray&, ...)`: `vertex_type` 1 is coloured.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_polygon_array(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    indices: *const u16,
    count: c_ulong,
    vertex_type: c_ulong,
    vertices: *const c_void,
    vertex_count: c_ulong,
    texture: Handle,
) -> c_long {
    draw_model::draw_polygon_array(
        &mut Host { rast, screen, state },
        indices,
        count as u32,
        vertex_type as u32,
        vertices,
        vertex_count as u32,
        texture,
    )
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_model(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    model: Handle,
    detail: f32,
    orthographic: bool,
) -> c_long {
    draw_model::draw_model(&mut Host { rast, screen, state }, model, detail, orthographic)
}

/// `DrawModelHierarchy` from outside the recursion (no parent transform).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_model_hierarchy(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    model: Handle,
    detail: f32,
    orthographic: bool,
) -> c_long {
    draw_model::draw_model_hierarchy(&mut Host { rast, screen, state }, model, detail, orthographic, None)
}

/// `Draw3DLine`: `points` is `count` `(x, y, z, advance)` records.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_3d_line(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    points: *const [f32; 4],
    count: usize,
    texture: Handle,
    v: f32,
    orthographic: bool,
    viewport: *const ViewportInfo,
) -> c_long {
    let points = if count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(points, count) } };
    draw_model::draw_3d_line(&mut Host { rast, screen, state }, points, texture, v, orthographic, unsafe { &*viewport })
}

unsafe fn materials<'a>(materials: *const MaterialView, count: usize) -> &'a [MaterialView] {
    if count == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(materials, count) }
    }
}

/// `Draw(GETerrainMesh&)`: `map` is the terrain material map, in map order.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_terrain(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    mesh: *const TerrainMeshView,
    map: *const MaterialMapEntry,
    map_count: usize,
    terrain_materials: *const MaterialView,
    material_count: usize,
    viewport: *const ViewportInfo,
    orthographic: bool,
) -> c_long {
    let map = if map_count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(map, map_count) } };
    draw_terrain::draw_terrain(
        &mut Host { rast, screen, state },
        unsafe { &*mesh },
        map,
        unsafe { materials(terrain_materials, material_count) },
        unsafe { &*viewport },
        orthographic,
    )
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_water(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    mesh: *const TerrainMeshView,
    water_materials: *const MaterialView,
    material_count: usize,
    viewport: *const ViewportInfo,
    orthographic: bool,
) -> c_long {
    draw_terrain::draw_water_and_mark(
        &mut Host { rast, screen, state },
        unsafe { &*mesh },
        unsafe { materials(water_materials, material_count) },
        unsafe { &*viewport },
        orthographic,
    )
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_material(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    list: *mut *mut TerrainPolygon,
    length: c_ulong,
) -> c_long {
    draw_terrain::draw_terrain_material(&mut Host { rast, screen, state }, list, length)
}

/// `DrawGridLines`: `lines` is `count` `(x1, y1, x2, y2)` records.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_grid_lines(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    mesh: *const TerrainMeshView,
    lines: *const [c_long; 4],
    count: usize,
    texture: Handle,
    width: f32,
    viewport: *const ViewportInfo,
    orthographic: bool,
) -> c_long {
    let lines = if count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(lines, count) } };
    draw_terrain::draw_grid_lines(
        &mut Host { rast, screen, state },
        unsafe { &*mesh },
        lines,
        texture,
        width,
        unsafe { &*viewport },
        orthographic,
    )
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_grid(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    mesh: *const TerrainMeshView,
    viewport: *const ViewportInfo,
    orthographic: bool,
) -> c_long {
    draw_terrain::draw_terrain_grid(
        &mut Host { rast, screen, state },
        unsafe { &*mesh },
        unsafe { &*viewport },
        orthographic,
    )
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_highlight(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    mesh: *const TerrainMeshView,
    rect: *const c_long,
    color: *const f32,
    opacity: f32,
    viewport: *const ViewportInfo,
    orthographic: bool,
) -> c_long {
    let rect = unsafe { [*rect, *rect.add(1), *rect.add(2), *rect.add(3)] };
    let color = unsafe { [*color, *color.add(1), *color.add(2)] };
    draw_terrain::draw_terrain_highlight(
        &mut Host { rast, screen, state },
        unsafe { &*mesh },
        rect,
        color,
        opacity,
        unsafe { &*viewport },
        orthographic,
    )
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_light_terrain(
    rast: *mut c_void,
    screen: c_ulong,
    state: *mut ScreenState,
    mesh: *const TerrainMeshView,
    ambient: *const f32,
) {
    let ambient = unsafe { [*ambient, *ambient.add(1), *ambient.add(2)] };
    draw_terrain::light_terrain(&mut Host { rast, screen, state }, unsafe { &*mesh }, ambient);
}

/// `TransformToCamera`: `water_time` is the C++ `sWaterTime`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_transform_to_camera(
    mesh: *const TerrainMeshView,
    terrain_materials: *const MaterialView,
    terrain_count: usize,
    water_materials: *const MaterialView,
    water_count: usize,
    viewport: *const ViewportInfo,
    scale: f32,
    water_only: bool,
    orthographic: bool,
    ripple: WaterRipple,
    water_time: *mut f32,
) {
    // only the mesh and the UM functions are reached: no device or vertex-buffer call
    let mut host = Host { rast: core::ptr::null_mut(), screen: 0, state: core::ptr::null_mut() };
    draw_terrain::transform_to_camera(
        &mut host,
        unsafe { &*mesh },
        unsafe { materials(terrain_materials, terrain_count) },
        unsafe { materials(water_materials, water_count) },
        unsafe { &*viewport },
        scale,
        water_only,
        orthographic,
        ripple,
        unsafe { &mut *water_time },
    );
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
