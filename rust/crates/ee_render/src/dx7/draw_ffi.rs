//! `extern "C"` for the rasterizer's draw paths: the thin forwarders in `Common/DX7Rasterizer Draw*.cpp` call the
//! `ee_dx7r_draw_*` functions with the rasterizer's object (`rastobj.rs`); the rasterizer's own steps (the lock, the
//! modes, the texture, the vertex buffer, the base's visibility tests and viewport) are calls on it. The GE objects a
//! draw reads (a model, its materials, sub-models and arrays, a texture's frame, a text and its font, a vertex
//! array's vertices) are read in place (ee_ge `cppobj.rs`, R4.3 part 17; the C++ `ee_dx7cpp_*` seam is gone) and
//! the text functions are the platform's (`platform/PlatFont.h`). `screen` is the screen index. The Host holds only raw pointers; every access is a short field read or write, never
//! held across a call into C++.
#![allow(clippy::missing_safety_doc, clippy::unnecessary_cast)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use crate::dx7::digest2d;
use crate::dx7::draw::*;
use crate::dx7::draw2d::{self, Draw2DHost, TextHost, TextInfo, TextRect};
use crate::dx7::draw_model::{self, MaterialInfo, ModelHost, ModelInfo, Plane, ViewportInfo};
use crate::dx7::draw_terrain::{
    self, LightData, MaterialMapEntry, MaterialView, RenderVertex, TerrainHost, TerrainMeshView, TerrainPolygon,
    WaterRipple,
};
use crate::dx7::gx::{device_light, yield_now, RawDevice, RawGx, RawHost};
use crate::dx7::rastobj::{EeDx7RastOp, EeDx7Rasterizer};
use crate::dx7::state::ScreenState;
use crate::dx7::vertex::Dx7StdColorVertex;
use crate::ge::bmpobj::{ee_ge_bmp_texture, ee_ge_bmp_vertices};
use crate::ge::cppobj;
use crate::ge::modobj::{
    ee_ge_mod_get, ee_ge_mod_getf, ee_ge_mod_ptr, EeGeModFloat, EeGeModPtr, EeGeModVal, EeGeModel,
};
use crate::ge::prim::{ee_ge_prim_texture, ee_ge_prim_vertices, EeGePrim};
use crate::ge::rastobj::{
    ee_ge_rast_get, ee_ge_rast_model_visible, ee_ge_rast_pixel_size, ee_ge_rast_ripple, EeGeRastVal,
};
use crate::ge::smallobj::{
    ee_ge_font_os, ee_ge_font_value, ee_ge_mat_color, ee_ge_mat_get, ee_ge_mat_opacity, ee_ge_mat_texture,
    ee_ge_text_alignment, ee_ge_text_color, ee_ge_text_font, ee_ge_text_location, ee_ge_text_string,
    ee_ge_text_transparent, EeGeFont, EeGeFontValue, EeGeMat, EeGeMatField, EeGeText,
};
use crate::ge::texobj_tex::{ee_ge_tex_corner, ee_ge_tex_frame_uv_size, ee_ge_tex_get, EeGeTexVal, EeGeTexture};
use crate::ge::tmobj::{
    ee_ge_tm_edge_points, ee_ge_tm_edge_vertices, ee_ge_tm_get, ee_ge_tm_get_f, ee_ge_tm_is_valid, ee_ge_tm_material,
    ee_ge_tm_material_map, ee_ge_tm_point, ee_ge_tm_point_visibility, ee_ge_tm_render_vertices, ee_ge_tm_vertex_color,
    ee_ge_tm_water_color, EeGeTerrainMesh, EeGeTmMaterialView, EeGeTmVal,
};
use crate::ge::viewport::GeViewport;
use core::fmt::Write as _;
use ee_base::math::types::GeTransformation;

/// cbindgen:ignore
mod cpp {
    use super::*;

    /// `PlatFontMetrics` (platform/PlatFont.h).
    #[repr(C)]
    pub struct PlatFontMetrics {
        pub height: i32,
        pub other: [i32; 6],
        pub first_char: u8,
        pub last_char: u8,
    }

    // platform/PlatFont.h (ee_plat_ui)
    extern "C" {
        pub fn PlatFontMeasure(font: *const c_void, text: *const c_char, length: u32) -> i32;
        pub fn PlatFontScaled(font: *const c_void, scale: f32) -> *const c_void;
        pub fn PlatFontGetMetrics(font: *const c_void, out: *mut PlatFontMetrics);
        pub fn PlatFontDraw(
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
        crate::dx7::raster::lock_vb(&mut RawGx, &mut RawHost, unsafe { &mut *self.state }, count)
            as *mut Dx7StdColorVertex
    }
    fn unlock_vb(&mut self) -> LongResult {
        crate::dx7::raster::unlock_vb(&mut RawGx, &mut RawHost, self.vb()) as LongResult
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

/// The Rust object of a C++ `GEModel*`.
///
/// # Safety
/// `model` is a live model.
unsafe fn model_obj(model: Handle) -> *mut EeGeModel {
    // SAFETY: the caller's contract
    unsafe { cppobj::counted(model as usize) }
}

/// Element `index` of one of a model's pointer arrays (`GetMaterial`, `GetPolygonArray`, `GetVertexArray`,
/// `GetSubModel`: the header's inline indexing).
///
/// # Safety
/// `p` is a live model with the array and an `index` in it.
unsafe fn model_array(p: *mut EeGeModel, which: EeGeModPtr, index: u32) -> usize {
    // SAFETY: the caller's contract
    unsafe { *(ee_ge_mod_ptr(p, which) as *const usize).add(index as usize) }
}

/// Element `index` of a `GELine`/`GEPoint` array: each element is one pointer, its Rust object (ee_ge prim.rs).
///
/// # Safety
/// `array` holds more than `index` elements.
unsafe fn prim_at(array: Handle, index: u32) -> *const EeGePrim {
    unsafe { *(array as *const *const EeGePrim).add(index as usize) }
}

// a bitmap is its Rust object (`GEBitmap::GetRust`), a line, point or rhombus a C++ array of them; the vertices are
// the C++ vertex array's
impl Draw2DHost for Host {
    fn bitmap_texture(&mut self, bitmap: Handle) -> Handle {
        unsafe { ee_ge_bmp_texture(bitmap.cast(), self.screen) }
    }
    fn bitmap_vertices(&mut self, bitmap: Handle) -> *mut Dx7StdColorVertex {
        unsafe { cppobj::vertices(ee_ge_bmp_vertices(bitmap.cast()) as usize) as *mut Dx7StdColorVertex }
    }
    fn line_texture(&mut self, lines: Handle) -> Handle {
        unsafe { ee_ge_prim_texture(prim_at(lines, 0), self.screen) }
    }
    fn line_vertices(&mut self, lines: Handle, index: u32) -> *mut Dx7StdColorVertex {
        unsafe { cppobj::vertices(ee_ge_prim_vertices(prim_at(lines, index)) as usize) as *mut Dx7StdColorVertex }
    }
    fn point_texture(&mut self, points: Handle) -> Handle {
        unsafe { ee_ge_prim_texture(prim_at(points, 0), self.screen) }
    }
    fn point_vertices(&mut self, points: Handle, index: u32) -> *mut Dx7StdColorVertex {
        unsafe { cppobj::vertices(ee_ge_prim_vertices(prim_at(points, index)) as usize) as *mut Dx7StdColorVertex }
    }
    fn rhombus_vertices(&mut self, rhombus: Handle) -> *mut Dx7StdColorVertex {
        unsafe { cppobj::vertices(ee_ge_prim_vertices(prim_at(rhombus, 0)) as usize) as *mut Dx7StdColorVertex }
    }
    fn texture_frame(&mut self, texture: Handle) -> (f32, [f32; 2]) {
        // SAFETY: a live texture (`GetFrameUVSize`, `GetFrameTopLeftCorner`)
        unsafe {
            let t: *mut EeGeTexture = cppobj::counted(texture as usize);
            let c = *ee_ge_tex_corner(t);
            (ee_ge_tex_frame_uv_size(t), [c.u, c.v])
        }
    }
}

impl ModelHost for Host {
    fn model_info(&mut self, model: Handle) -> ModelInfo {
        // SAFETY: a live model (the header's accessors, `GetVertexArray(0)->GetType()`)
        unsafe {
            let p = model_obj(model);
            let get = |v| ee_ge_mod_get(p, v) as u32;
            let transform = *(ee_ge_mod_ptr(p, EeGeModPtr::GraphicsTransform) as *const Transform);
            ModelInfo {
                material_count: get(EeGeModVal::MaterialCount),
                lod_count: get(EeGeModVal::LodCount),
                sub_model_count: get(EeGeModVal::SubModelCount),
                current_frame: get(EeGeModVal::CurrentFrame),
                next_frame: get(EeGeModVal::NextFrame),
                vertex_type: cppobj::vertex_type(model_array(p, EeGeModPtr::VertexArrays, 0)) as u32,
                scale: ee_ge_mod_getf(p, EeGeModFloat::GraphicsScale),
                opacity: ee_ge_mod_getf(p, EeGeModFloat::Opacity),
                fraction: ee_ge_mod_getf(p, EeGeModFloat::FractionalFramePosition),
                transform,
            }
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
        // SAFETY: a live model and one of its materials (`GetMaterial(index)` and its accessors)
        unsafe {
            let m: *mut EeGeMat = cppobj::counted(model_array(model_obj(model), EeGeModPtr::Materials, index));
            let get = |f, l| ee_ge_mat_get(m, f, l as c_ulong) as u32;
            let c = ee_ge_mat_color(m);
            MaterialInfo {
                texture: ee_ge_mat_texture(m, self.screen),
                polygon_start: get(EeGeMatField::PolygonStart, lod),
                polygon_length: get(EeGeMatField::PolygonLength, lod),
                vertex_start: get(EeGeMatField::VertexStart, 0),
                vertex_length: get(EeGeMatField::VertexLength, 0),
                opacity: ee_ge_mat_opacity(m),
                color: [*c, *c.add(1), *c.add(2)],
            }
        }
    }
    fn polygon_indices(&mut self, model: Handle, lod: u32) -> *const u16 {
        // SAFETY: a live model (`GetPolygonArray(lod)`'s indices)
        unsafe { cppobj::indices(model_array(model_obj(model), EeGeModPtr::PolygonArrays, lod)) }
    }
    fn frame_vertices(&mut self, model: Handle, frame: u32) -> *const c_void {
        // SAFETY: a live model (`GetVertexArray(frame)`'s vertices)
        unsafe { cppobj::vertices(model_array(model_obj(model), EeGeModPtr::VertexArrays, frame)) }
    }
    fn sub_model(&mut self, model: Handle, index: u32) -> Handle {
        // SAFETY: a live model (`GetSubModel(index)`)
        unsafe { model_array(model_obj(model), EeGeModPtr::SubModels, index) as Handle }
    }
}

impl TerrainHost for Host {
    // the mesh is the Rust mesh (`TerrainMeshView::mesh`): `GetZ`, `GetVertexNormal`, `GetVertexColor`,
    // `GetWaterColor` (one colour for every point) and `GetIsValidTile`
    fn mesh_z(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> f32 {
        unsafe { (*ee_ge_tm_point(tm(mesh), x, y)).z }
    }
    fn mesh_normal(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3] {
        let n = unsafe { *(*ee_ge_tm_point(tm(mesh), x, y)).normal };
        [n.x, n.y, n.z]
    }
    fn mesh_vertex_color(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> [f32; 3] {
        unsafe { *(ee_ge_tm_vertex_color(tm(mesh), x, y) as *const [f32; 3]) }
    }
    fn mesh_water_color(&mut self, mesh: Handle, _x: c_ulong, _y: c_ulong) -> [f32; 3] {
        unsafe { *(ee_ge_tm_water_color(tm(mesh)) as *const [f32; 3]) }
    }
    fn mesh_valid_tile(&mut self, mesh: Handle, x: c_ulong, y: c_ulong) -> bool {
        unsafe { ee_ge_tm_is_valid(tm(mesh), x, y, true) }
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
        ee_base::math::um::um_arctan(value)
    }
    fn um_tan(&mut self, value: f32) -> f32 {
        ee_base::math::um::um_tan(value)
    }
    fn um_sin(&mut self, value: f32) -> f32 {
        ee_base::math::um::um_sin(value)
    }
    fn um_cos(&mut self, value: f32) -> f32 {
        ee_base::math::um::um_cos(value)
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_bitmap(rast: *mut EeDx7Rasterizer, screen: c_ulong, bitmap: Handle) -> c_long {
    let mut h = unsafe { Host::new(rast, screen) };
    let r = draw2d::draw_bitmap(&mut h, bitmap);
    if digest2d::enabled() {
        let texture = h.bitmap_texture(bitmap);
        let mut s = format!("bmp s{screen} r{r}");
        unsafe { digest2d::vertices(&mut s, h.bitmap_vertices(bitmap), 4) };
        digest2d::texture(&mut s, digest_texture(&mut h, texture));
        digest2d::command(&s);
    }
    r
}

/// The digest's view of a texture's frame (none for null).
fn digest_texture(h: &mut Host, texture: Handle) -> Option<(f32, [f32; 2], isize, isize)> {
    if texture.is_null() {
        return None;
    }
    let (size, corner) = h.texture_frame(texture);
    // SAFETY: a live texture
    let (frame, count) = unsafe {
        let t: *mut EeGeTexture = cppobj::counted(texture as usize);
        (ee_ge_tex_get(t, EeGeTexVal::Frame), ee_ge_tex_get(t, EeGeTexVal::FrameCount))
    };
    Some((size, corner, frame, count))
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_lines(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    lines: Handle,
    count: c_ulong,
) -> c_long {
    let mut h = unsafe { Host::new(rast, screen) };
    let r = draw2d::draw_lines(&mut h, lines, count as u32);
    if digest2d::enabled() {
        let texture = h.line_texture(lines);
        let mut s = format!("lines s{screen} r{r} n{count}");
        for i in 0..count as u32 {
            unsafe { digest2d::vertices(&mut s, h.line_vertices(lines, i), 2) };
        }
        digest2d::texture(&mut s, digest_texture(&mut h, texture));
        digest2d::command(&s);
    }
    r
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_points(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    points: Handle,
    count: c_ulong,
) -> c_long {
    let mut h = unsafe { Host::new(rast, screen) };
    let r = draw2d::draw_points(&mut h, points, count as u32);
    if digest2d::enabled() {
        let texture = h.point_texture(points);
        let mut s = format!("points s{screen} r{r} n{count}");
        for i in 0..count as u32 {
            unsafe { digest2d::vertices(&mut s, h.point_vertices(points, i), 1) };
        }
        digest2d::texture(&mut s, digest_texture(&mut h, texture));
        digest2d::command(&s);
    }
    r
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_rhombus(rast: *mut EeDx7Rasterizer, screen: c_ulong, rhombus: Handle) -> c_long {
    let mut h = unsafe { Host::new(rast, screen) };
    let r = draw2d::draw_rhombus(&mut h, rhombus);
    if digest2d::enabled() {
        let mut s = format!("rhombus s{screen} r{r}");
        unsafe { digest2d::vertices(&mut s, h.rhombus_vertices(rhombus), 4) };
        digest2d::command(&s);
    }
    r
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

/// `Draw(GETerrainMesh&)`: `mesh` is the mesh's Rust object (`GETerrainMesh::GetRust`).
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_terrain(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *mut EeGeTerrainMesh,
    orthographic: bool,
) -> c_long {
    // SAFETY: the caller's contract (the views first, as the C++ built them before the draw)
    unsafe {
        let view = mesh_view(mesh);
        let map = material_map(mesh);
        let materials = material_views(mesh, false);
        let viewport = screen_viewport(rast, screen);
        draw_terrain::draw_terrain(&mut Host::new(rast, screen), &view, &map, &materials, &viewport, orthographic)
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_water(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *mut EeGeTerrainMesh,
    orthographic: bool,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        let view = mesh_view(mesh);
        let materials = material_views(mesh, true);
        let viewport = screen_viewport(rast, screen);
        draw_terrain::draw_water_and_mark(&mut Host::new(rast, screen), &view, &materials, &viewport, orthographic)
    }
}

/// `DrawGridLines`: `lines` is `count` `(x1, y1, x2, y2)` records.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_grid_lines(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *mut EeGeTerrainMesh,
    lines: *const [c_long; 4],
    count: usize,
    texture: Handle,
    width: f32,
    orthographic: bool,
) -> c_long {
    let lines = if count == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(lines, count) } };
    // SAFETY: the caller's contract
    unsafe {
        let view = mesh_view(mesh);
        let viewport = screen_viewport(rast, screen);
        draw_terrain::draw_grid_lines(
            &mut Host::new(rast, screen),
            &view,
            lines,
            texture,
            width,
            &viewport,
            orthographic,
        )
    }
}

#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_grid(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *mut EeGeTerrainMesh,
    orthographic: bool,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        let view = mesh_view(mesh);
        let viewport = screen_viewport(rast, screen);
        draw_terrain::draw_terrain_grid(&mut Host::new(rast, screen), &view, &viewport, orthographic)
    }
}

/// `DrawTerrainHighlight`: `color` is a `GEColor`.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_dx7r_draw_terrain_highlight(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *mut EeGeTerrainMesh,
    rect: *const c_long,
    color: *const f32,
    opacity: f32,
    orthographic: bool,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        let view = mesh_view(mesh);
        let rect = [*rect, *rect.add(1), *rect.add(2), *rect.add(3)];
        let color = [*color, *color.add(1), *color.add(2)];
        let viewport = screen_viewport(rast, screen);
        let host = &mut Host::new(rast, screen);
        draw_terrain::draw_terrain_highlight(host, &view, rect, color, opacity, &viewport, orthographic)
    }
}

/// `Light`: the base's computed ambient colour.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_light_terrain(
    rast: *mut EeDx7Rasterizer,
    screen: c_ulong,
    mesh: *mut EeGeTerrainMesh,
) {
    // SAFETY: the caller's contract
    unsafe {
        let view = mesh_view(mesh);
        let c = *crate::ge::rastobj::ee_ge_rast_color(EeDx7Rasterizer::base(rast), true);
        draw_terrain::light_terrain(&mut Host::new(rast, screen), &view, [c.red, c.green, c.blue]);
    }
}

/// `TransformToCamera`: `viewport` is the `GEViewport` given; the base's water ripple and the water clock.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_transform_to_camera(
    rast: *mut EeDx7Rasterizer,
    mesh: *mut EeGeTerrainMesh,
    viewport: *const c_void,
    scale: f32,
    water_only: bool,
    orthographic: bool,
) {
    // SAFETY: the caller's contract
    let (view, terrain, water) = unsafe { (mesh_view(mesh), material_views(mesh, false), material_views(mesh, true)) };
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
            &view,
            &terrain,
            &water,
            &info,
            scale,
            water_only,
            orthographic,
            ripple,
            water_time,
        )
    });
}
// ---- the terrain mesh's views, read off its Rust object (`ee_ge::tmobj`)

// the mesh's visible-frame records are the draws' (`GETMRenderVertex`, `GETMPolygon`)
const _: () = {
    use crate::ge::tmobj::{EeGeTmPolygon, EeGeTmRenderVertex};
    use core::mem::{offset_of, size_of};
    assert!(size_of::<EeGeTmRenderVertex>() == size_of::<RenderVertex>());
    assert!(offset_of!(EeGeTmRenderVertex, color) == offset_of!(RenderVertex, color));
    assert!(offset_of!(EeGeTmRenderVertex, visibility) == offset_of!(RenderVertex, visibility));
    assert!(offset_of!(EeGeTmRenderVertex, fog) == offset_of!(RenderVertex, fog));
    assert!(offset_of!(EeGeTmRenderVertex, clip_flags) == offset_of!(RenderVertex, clip_flags));
    assert!(offset_of!(EeGeTmRenderVertex, linked_visibility) == offset_of!(RenderVertex, linked_visibility));
    assert!(size_of::<EeGeTmPolygon>() == size_of::<TerrainPolygon>());
    assert!(offset_of!(EeGeTmPolygon, alpha) == offset_of!(TerrainPolygon, alpha));
    assert!(offset_of!(EeGeTmPolygon, uv) == offset_of!(TerrainPolygon, uv));
};

/// `DX7TerrainViewOf`: the mesh's visible frame as the terrain draws read it.
///
/// # Safety
/// `mesh` is a live mesh.
unsafe fn mesh_view(mesh: *mut EeGeTerrainMesh) -> TerrainMeshView {
    // SAFETY: the caller's contract
    unsafe {
        TerrainMeshView {
            mesh: mesh as Handle,
            terrain_vertices: ee_ge_tm_render_vertices(mesh, false) as *mut RenderVertex,
            water_vertices: ee_ge_tm_render_vertices(mesh, true) as *mut RenderVertex,
            terrain_visible: ee_ge_tm_point_visibility(mesh, false) as *const bool,
            water_visible: ee_ge_tm_point_visibility(mesh, true) as *const bool,
            visible_tile_rows: ee_ge_tm_get(mesh, EeGeTmVal::VisibleTileRows),
            start_x: ee_ge_tm_get(mesh, EeGeTmVal::BottomLeftX),
            start_y: ee_ge_tm_get(mesh, EeGeTmVal::BottomLeftY),
            width: ee_ge_tm_get(mesh, EeGeTmVal::VisibleTileWidth),
            height: ee_ge_tm_get(mesh, EeGeTmVal::VisibleTileHeight),
            water_percentage: ee_ge_tm_get_f(mesh, EeGeTmVal::VisibleWaterPercentage),
            is_space: ee_ge_tm_get(mesh, EeGeTmVal::SpaceTileCount) > 0,
            edge_world: ee_ge_tm_edge_points(mesh, false) as *const [f32; 3],
            edge_vertices: ee_ge_tm_edge_vertices(mesh, false) as *const *mut RenderVertex,
            edge_count: ee_ge_tm_get(mesh, EeGeTmVal::EdgeVertexCount),
            cliff_world: ee_ge_tm_edge_points(mesh, true) as *const [f32; 3],
            cliff_vertices: ee_ge_tm_edge_vertices(mesh, true) as *const *mut RenderVertex,
            cliff_count: ee_ge_tm_get(mesh, EeGeTmVal::CliffVertexCount),
        }
    }
}

/// `DX7MaterialViewsOf`: the terrain (or with `water`, the water) materials.
///
/// # Safety
/// `mesh` is a live mesh.
unsafe fn material_views(mesh: *mut EeGeTerrainMesh, water: bool) -> Vec<MaterialView> {
    let what = if water { EeGeTmVal::WaterMaterialCount } else { EeGeTmVal::TerrainMaterialCount };
    // SAFETY: the caller's contract
    let count = unsafe { ee_ge_tm_get(mesh, what) };
    (0..count)
        .map(|i| {
            let mut m = EeGeTmMaterialView {
                texture: core::ptr::null_mut(),
                polygons: core::ptr::null_mut(),
                polygon_count: 0,
                indices: core::ptr::null_mut(),
                indices_length: 0,
                strips: core::ptr::null_mut(),
                strips_length: 0,
            };
            // SAFETY: the caller's contract; i is one of the materials
            unsafe { ee_ge_tm_material(mesh, water, i, &mut m) };
            MaterialView {
                texture: m.texture,
                polygons: m.polygons as *const *mut TerrainPolygon,
                polygon_count: m.polygon_count as c_ulong,
                indices: m.indices as *mut *mut TerrainPolygon,
                indices_length: m.indices_length as c_ulong,
                strips: m.strips as *mut *mut TerrainPolygon,
                strips_length: m.strips_length as c_ulong,
            }
        })
        .collect()
}

/// `GetTerrainMaterialMap`: (texture, material index) in map order.
///
/// # Safety
/// `mesh` is a live mesh.
unsafe fn material_map(mesh: *mut EeGeTerrainMesh) -> Vec<MaterialMapEntry> {
    // SAFETY: the caller's contract
    let capacity = unsafe { ee_ge_tm_get(mesh, EeGeTmVal::TerrainMaterialCount) } as usize;
    let mut textures = vec![core::ptr::null_mut(); capacity];
    let mut indices: Vec<c_ulong> = vec![0; capacity];
    // SAFETY: as above; the buffers hold `capacity` entries
    let count = unsafe { ee_ge_tm_material_map(mesh, textures.as_mut_ptr(), indices.as_mut_ptr(), capacity) };
    (0..count.min(capacity)).map(|k| MaterialMapEntry { texture: textures[k], index: indices[k] }).collect()
}

/// The mesh of a view (its `mesh` handle is the Rust mesh).
fn tm(mesh: Handle) -> *mut EeGeTerrainMesh {
    mesh as *mut EeGeTerrainMesh
}

/// `DX7ViewportInfoOf`: a viewport as the 3D draws read it.
fn viewport_info(v: &GeViewport) -> ViewportInfo {
    let plane = |p: &ee_base::math::types::Ge3DPlane| Plane {
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
        unsafe { PlatFontMeasure(font, text, length) }
    }
    fn scaled(&mut self, font: *const c_void, scale: f32) -> *const c_void {
        unsafe { PlatFontScaled(font, scale) }
    }
    fn font_height(&mut self, font: *const c_void) -> i32 {
        let mut m = PlatFontMetrics { height: 0, other: [0; 6], first_char: 0, last_char: 0 };
        unsafe { PlatFontGetMetrics(font, &mut m) };
        m.height
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
        unsafe { PlatFontDraw(font, text, length, x, y, color, opaque as i32, background, clip.as_ptr(), self.target) };
    }
}

/// `DX7TextInfoOf`: one text draw's inputs, read off the `GEText*` and its font. The length is the text's bytes, as
/// the platform's text functions take it (`GetLength` counts characters: under UTF-8, decision D6, it cut the text).
///
/// # Safety
/// `text` is a live `GEText` with a font.
unsafe fn text_info(text: Handle) -> TextInfo {
    // SAFETY: the caller's contract
    unsafe {
        let t: *mut EeGeText = cppobj::plain(text as usize);
        let font: *mut EeGeFont = cppobj::counted(ee_ge_text_font(t) as usize);
        let string = ee_ge_text_string(t) as usize;
        let location = *ee_ge_text_location(t);
        let color = |foreground| {
            let c = ee_ge_text_color(t, foreground);
            [*c, *c.add(1), *c.add(2)]
        };
        TextInfo {
            font: ee_ge_font_os(font),
            text: cppobj::wstr_text(string),
            length: cppobj::wstr_bytes(string) as u32,
            x: location.x,
            y: location.y,
            alignment: ee_ge_text_alignment(t),
            transparent: ee_ge_text_transparent(t),
            shadow_font: ee_ge_font_value(font, EeGeFontValue::Style) & 0x10 != 0,
            foreground: color(true),
            background: color(false),
        }
    }
}

/// `DX7TextBounds` of a `GEText*`: `clip` may be null.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_text_bounds(text: Handle, clip: *const TextRect, out: *mut TextRect) {
    let info = unsafe { text_info(text) };
    let bounds = draw2d::text_bounds(&mut TextPlat { target: core::ptr::null() }, &info, unsafe { clip.as_ref() });
    unsafe { *out = bounds };
    if digest2d::enabled() {
        // SAFETY: the text's bytes (`length` of them) are live for the call
        let bytes = unsafe { core::slice::from_raw_parts(info.text as *const u8, info.length as usize) };
        let mut s = format!(
            "text {:?} a{} t{} sh{}",
            String::from_utf8_lossy(bytes),
            info.alignment,
            info.transparent as u8,
            info.shadow_font as u8
        );
        digest2d::floats(&mut s, &[info.x, info.y]);
        digest2d::floats(&mut s, &info.foreground);
        digest2d::floats(&mut s, &info.background);
        let font = unsafe { ee_ge_text_font(cppobj::plain(text as usize)) };
        let font: *mut EeGeFont = unsafe { cppobj::counted(font as usize) };
        let _ = unsafe {
            write!(
                s,
                " font{} st{}",
                ee_ge_font_value(font, EeGeFontValue::Size),
                ee_ge_font_value(font, EeGeFontValue::Style)
            )
        };
        match unsafe { clip.as_ref() } {
            Some(c) => {
                let _ = write!(s, " clip {} {} {} {}", c.left, c.top, c.right, c.bottom);
            }
            None => s.push_str(" clip -"),
        }
        let _ = write!(s, " -> {} {} {} {}", bounds.left, bounds.top, bounds.right, bounds.bottom);
        digest2d::command(&s);
    }
}

/// `DX7DrawTextPlat` (the `GXSurface::DrawText` callback) of a `GEText*`. `target` is the `PlatFontTarget*`. Returns false when the
/// font has no OS font.
#[no_mangle]
pub unsafe extern "C" fn ee_dx7r_text_draw(
    text: Handle,
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
        &unsafe { text_info(text) },
        unsafe { clip.as_ref() },
        origin_x,
        origin_y,
        target_width,
        target_height,
        target_scale,
    )
}
