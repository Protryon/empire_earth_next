//! `extern "C"` for the rasterizer's draw paths: the thin forwarders in `Common/DX7Rasterizer Draw*.cpp` call the
//! `ee_dx7r_draw_*` functions with the rasterizer's object (`rastobj.rs`); the rasterizer's own steps (the lock, the
//! modes, the texture, the vertex buffer, the base's visibility tests and viewport) are calls on it, and the steps
//! that read other C++ objects come back through the `ee_dx7cpp_*` seam (declared here, defined in the Draw units).
//! `screen` is the screen index. The Host holds only raw pointers; every access is a short field read or write, never
//! held across a call into C++.
#![allow(clippy::missing_safety_doc, clippy::unnecessary_cast)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::draw::*;
use crate::draw2d::{self, Draw2DHost, TextHost, TextInfo, TextRect};
use crate::draw_model::{self, MaterialInfo, ModelHost, ModelInfo, Plane, ViewportInfo};
use crate::draw_terrain::{
    self, LightData, MaterialMapEntry, MaterialView, TerrainHost, TerrainMeshView, TerrainPolygon, WaterRipple,
};
use crate::gx::{device_light, yield_now, RawDevice, RawGx, RawHost};
use crate::rastobj::{EeDx7RastOp, EeDx7Rasterizer};
use crate::state::ScreenState;
use crate::vertex::Dx7StdColorVertex;
use ee_ge::rastobj::{ee_ge_rast_get, ee_ge_rast_model_visible, ee_ge_rast_pixel_size, ee_ge_rast_ripple, EeGeRastVal};
use ee_ge::types::GeTransformation;
use ee_ge::viewport::GeViewport;

/// cbindgen:ignore
mod cpp {
    use super::*;

    extern "C" {

        pub fn ee_dx7cpp_bitmap_texture(bitmap: Handle, screen: c_ulong) -> Handle;
        pub fn ee_dx7cpp_bitmap_vertices(bitmap: Handle) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_line_texture(lines: Handle, screen: c_ulong) -> Handle;
        pub fn ee_dx7cpp_line_vertices(lines: Handle, index: c_ulong) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_point_texture(points: Handle, screen: c_ulong) -> Handle;
        pub fn ee_dx7cpp_point_vertices(points: Handle, index: c_ulong) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_rhombus_vertices(rhombus: Handle) -> *mut Dx7StdColorVertex;
        pub fn ee_dx7cpp_texture_frame(texture: Handle, out_size: *mut f32, out_corner: *mut f32);

        pub fn ee_dx7cpp_model_info(model: Handle, out: *mut ModelInfo);
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
    pub rast: *mut EeDx7Rasterizer,
    pub screen: c_ulong,
    pub state: *mut ScreenState,
}

impl Host {
    /// The draw host of screen `screen`.
    ///
    /// # Safety
    /// `rast` is live; `screen` is one of its screens.
    unsafe fn new(rast: *mut EeDx7Rasterizer, screen: c_ulong) -> Host {
        // SAFETY: the caller's contract
        Host { rast, screen, state: unsafe { EeDx7Rasterizer::record(rast, screen) } }
    }

    /// The screen's viewport (`GetViewport(screen)`), copied.
    fn viewport(&self) -> GeViewport {
        // SAFETY: the rasterizer is live for the draw call
        unsafe { (*EeDx7Rasterizer::ge_record(self.rast, self.screen)).viewport }
    }

    /// The drawing lock, entered or left.
    fn lock(&mut self, op: EeDx7RastOp) {
        // SAFETY: the rasterizer is live for the draw call; the host's contract
        unsafe {
            let lock = ee_ge_rast_get(EeDx7Rasterizer::base(self.rast), EeGeRastVal::DrawingAccess) as Handle;
            EeDx7Rasterizer::op(self.rast, op, lock, 0, 0);
        }
    }

    fn device(&self) -> RawDevice {
        RawDevice(unsafe { (*self.state).mDevice })
    }

    fn vb(&self) -> *mut c_void {
        unsafe { (*self.state).mVertexBuffer }
    }
}

impl DrawHost for Host {
    fn enter(&mut self) {
        self.lock(EeDx7RastOp::LockEnter);
    }
    fn leave(&mut self) {
        self.lock(EeDx7RastOp::LockLeave);
    }
    fn set_2d_mode(&mut self) {
        // SAFETY: the rasterizer is live for the draw call
        unsafe { EeDx7Rasterizer::set_2d_mode(self.rast, self.screen) };
    }
    fn set_3d_mode(&mut self) {
        // SAFETY: as above
        unsafe { EeDx7Rasterizer::set_3d_mode(self.rast, self.screen) };
    }
    fn set_texture(&mut self, texture: Handle) {
        // SAFETY: as above
        unsafe { EeDx7Rasterizer::set_texture(self.rast, texture, self.screen) };
    }
    fn set_shading_color_source(&mut self, source: u32) {
        // SAFETY: as above
        unsafe { EeDx7Rasterizer::set_shading_color_source(self.rast, source as c_ulong, self.screen) };
    }
    fn lock_vb(&mut self, count: u32) -> *mut Dx7StdColorVertex {
        // SAFETY: the record is live for the draw call (`LockVB`)
        crate::raster::lock_vb(&mut RawGx, &mut RawHost, unsafe { &mut *self.state }, count) as *mut Dx7StdColorVertex
    }
    fn unlock_vb(&mut self) -> LongResult {
        crate::raster::unlock_vb(&mut RawGx, &mut RawHost, self.vb()) as LongResult
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
        // SAFETY: the rasterizer is live for the draw call (`m32C[1] += count`)
        unsafe {
            let g = &mut *EeDx7Rasterizer::ge_record(self.rast, self.screen);
            g.counters[1] = g.counters[1].wrapping_add(count as c_ulong);
        }
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
        let (v, t) = (self.viewport(), GeTransformation { m: *transform });
        let (mut visible, mut clipped) = (false, false);
        // SAFETY: the rasterizer is live for the draw call; the base's contract (`IsPerspectiveModelVisible` /
        // `IsOrthographicModelVisible` on the screen's viewport)
        unsafe {
            let base = EeDx7Rasterizer::base(self.rast);
            ee_ge_rast_model_visible(base, model, scale, &v, &t, orthographic, &mut visible, &mut clipped);
        }
        (visible, clipped)
    }
    fn pixel_size(&mut self, model: Handle, transform: &Transform, scale: f32, orthographic: bool) -> f32 {
        let (v, t) = (self.viewport(), GeTransformation { m: *transform });
        // SAFETY: as above (`GetPixelSize`)
        unsafe { ee_ge_rast_pixel_size(EeDx7Rasterizer::base(self.rast), model, &v, &t, scale, orthographic) }
    }
    fn view_width(&mut self) -> f32 {
        let r = self.viewport().view_rectangle;
        r.right - r.left
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
        if !EeDx7Rasterizer::light_in_use(index as usize) {
            return None;
        }
        let (light_type, diffuse, direction) = device_light(unsafe { (*self.state).mDevice }, index);
        Some(LightData { light_type, diffuse, direction })
    }
    fn um_arctan(&mut self, value: f32) -> f32 {
        ee_ge::um::um_arctan(value)
    }
    fn um_tan(&mut self, value: f32) -> f32 {
        ee_ge::um::um_tan(value)
    }
    fn um_sin(&mut self, value: f32) -> f32 {
        ee_ge::um::um_sin(value)
    }
    fn um_cos(&mut self, value: f32) -> f32 {
        ee_ge::um::um_cos(value)
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_bitmap(rast: *mut EeDx7Rasterizer, screen: c_ulong, bitmap: Handle) -> c_long {
    draw2d::draw_bitmap(&mut unsafe { Host::new(rast, screen) }, bitmap)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_lines(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    lines: Handle,
    count: c_ulong,
) -> c_long {
    draw2d::draw_lines(&mut unsafe { Host::new(rast, screen) }, lines, count as u32)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_points(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    points: Handle,
    count: c_ulong,
) -> c_long {
    draw2d::draw_points(&mut unsafe { Host::new(rast, screen) }, points, count as u32)
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_rhombus(rast: *mut EeDx7Rasterizer, screen: c_ulong, rhombus: Handle) -> c_long {
    draw2d::draw_rhombus(&mut unsafe { Host::new(rast, screen) }, rhombus)
}

/// `Draw(GEPolygonArray&, ...)`: `vertex_type` 1 is coloured.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_polygon_array(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    indices: *const u16,
    count: c_ulong,
    vertex_type: c_ulong,
    vertices: *const c_void,
    vertex_count: c_ulong,
    texture: Handle,
) -> c_long {
    draw_model::draw_polygon_array(
        &mut unsafe { Host::new(rast, screen) },
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
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    model: Handle,
    detail: f32,
    orthographic: bool,
) -> c_long {
    draw_model::draw_model(&mut unsafe { Host::new(rast, screen) }, model, detail, orthographic)
}

/// `DrawModelHierarchy` from outside the recursion (no parent transform).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_model_hierarchy(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    model: Handle,
    detail: f32,
    orthographic: bool,
) -> c_long {
    draw_model::draw_model_hierarchy(&mut unsafe { Host::new(rast, screen) }, model, detail, orthographic, None)
}

/// `Draw3DLine`: `points` is `count` `(x, y, z, advance)` records.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_3d_line(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    points: *const [f32; 4],
    count: usize,
    texture: Handle,
    v: f32,
    orthographic: bool,
) -> c_long {
    let points = if count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(points, count) } };
    draw_model::draw_3d_line(
        &mut unsafe { Host::new(rast, screen) },
        points,
        texture,
        v,
        orthographic,
        &screen_viewport(rast, screen),
    )
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
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *const TerrainMeshView,
    map: *const MaterialMapEntry,
    map_count: usize,
    terrain_materials: *const MaterialView,
    material_count: usize,
    orthographic: bool,
) -> c_long {
    let map = if map_count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(map, map_count) } };
    draw_terrain::draw_terrain(
        &mut unsafe { Host::new(rast, screen) },
        unsafe { &*mesh },
        map,
        unsafe { materials(terrain_materials, material_count) },
        &screen_viewport(rast, screen),
        orthographic,
    )
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_water(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *const TerrainMeshView,
    water_materials: *const MaterialView,
    material_count: usize,
    orthographic: bool,
) -> c_long {
    draw_terrain::draw_water_and_mark(
        &mut unsafe { Host::new(rast, screen) },
        unsafe { &*mesh },
        unsafe { materials(water_materials, material_count) },
        &screen_viewport(rast, screen),
        orthographic,
    )
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_material(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    list: *mut *mut TerrainPolygon,
    length: c_ulong,
) -> c_long {
    draw_terrain::draw_terrain_material(&mut unsafe { Host::new(rast, screen) }, list, length)
}

/// `DrawGridLines`: `lines` is `count` `(x1, y1, x2, y2)` records.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_grid_lines(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *const TerrainMeshView,
    lines: *const [c_long; 4],
    count: usize,
    texture: Handle,
    width: f32,
    orthographic: bool,
) -> c_long {
    let lines = if count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(lines, count) } };
    draw_terrain::draw_grid_lines(
        &mut unsafe { Host::new(rast, screen) },
        unsafe { &*mesh },
        lines,
        texture,
        width,
        &screen_viewport(rast, screen),
        orthographic,
    )
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_grid(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *const TerrainMeshView,
    orthographic: bool,
) -> c_long {
    draw_terrain::draw_terrain_grid(
        &mut unsafe { Host::new(rast, screen) },
        unsafe { &*mesh },
        &screen_viewport(rast, screen),
        orthographic,
    )
}

#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_highlight(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *const TerrainMeshView,
    rect: *const c_long,
    color: *const f32,
    opacity: f32,
    orthographic: bool,
) -> c_long {
    let rect = unsafe { [*rect, *rect.add(1), *rect.add(2), *rect.add(3)] };
    let color = unsafe { [*color, *color.add(1), *color.add(2)] };
    draw_terrain::draw_terrain_highlight(
        &mut unsafe { Host::new(rast, screen) },
        unsafe { &*mesh },
        rect,
        color,
        opacity,
        &screen_viewport(rast, screen),
        orthographic,
    )
}

/// `Light`: the base's computed ambient colour.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_light_terrain(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *const TerrainMeshView,
) {
    // SAFETY: the caller's contract
    let c = unsafe { *ee_ge::rastobj::ee_ge_rast_color(EeDx7Rasterizer::base(rast), true) };
    draw_terrain::light_terrain(&mut unsafe { Host::new(rast, screen) }, unsafe { &*mesh }, [c.red, c.green, c.blue]);
}

/// `TransformToCamera`: `viewport` is the `GEViewport` given; the base's water ripple and the water clock.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_transform_to_camera(
    rast: *mut EeDx7Rasterizer,
    mesh: *const TerrainMeshView,
    terrain_materials: *const MaterialView,
    terrain_count: usize,
    water_materials: *const MaterialView,
    water_count: usize,
    viewport: *const c_void,
    scale: f32,
    water_only: bool,
    orthographic: bool,
) {
    let mut ripple = [0.0f32; 3];
    // SAFETY: the caller's contract
    unsafe { ee_ge_rast_ripple(EeDx7Rasterizer::base(rast), &mut ripple, false) };
    let ripple = WaterRipple { time_rate: ripple[0], water_height: ripple[1], terrain_height: ripple[2] };
    // only the mesh and the UM functions are reached: no device or vertex-buffer call
    let mut host = Host { rast, screen: 0, state: core::ptr::null_mut() };
    let info = viewport_info(unsafe { &*(viewport as *const GeViewport) });
    EeDx7Rasterizer::with_water_time(|water_time| {
        draw_terrain::transform_to_camera(
            &mut host,
            unsafe { &*mesh },
            unsafe { materials(terrain_materials, terrain_count) },
            unsafe { materials(water_materials, water_count) },
            &info,
            scale,
            water_only,
            orthographic,
            ripple,
            water_time,
        )
    });
}

/// `DX7ViewportInfoOf`: a viewport as the 3D draws read it.
fn viewport_info(v: &GeViewport) -> ViewportInfo {
    let plane = |p: &ee_ge::types::Ge3DPlane| Plane {
        point: [p.point.x, p.point.y, p.point.z],
        normal: [p.normal.x, p.normal.y, p.normal.z],
    };
    let o = v.orthographic_rectangle;
    ViewportInfo {
        camera: v.camera.m,
        camera_inverse: v.camera_inverse.m,
        perspective: v.perspective_projection.m,
        orthographic: v.orthographic_projection.m,
        front_clip: v.front_clipping_distance,
        rear_clip: v.rear_clipping_distance,
        half_width: v.half_width,
        half_height: v.half_height,
        field_of_view: v.field_of_view,
        y_clip_plane: v.y_clipping_plane,
        ortho_left: o.left,
        ortho_top: o.top,
        ortho_right: o.right,
        ortho_bottom: o.bottom,
        left_plane: plane(&v.left_plane),
        right_plane: plane(&v.right_plane),
        top_plane: plane(&v.top_plane),
        bottom_plane: plane(&v.bottom_plane),
    }
}

/// The screen's viewport (`GetViewport(screen)`) as the 3D draws read it.
///
/// # Safety
/// `rast` is live; `screen` is one of its screens.
unsafe fn screen_viewport(rast: *mut EeDx7Rasterizer, screen: c_ulong) -> ViewportInfo {
    // SAFETY: the caller's contract
    viewport_info(&unsafe { Host::new(rast, screen) }.viewport())
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
