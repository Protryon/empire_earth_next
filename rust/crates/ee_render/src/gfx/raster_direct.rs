//! The raster table's model, material, vertex array and texture operations made on the Graphics Engine's Rust
//! objects directly (the graphics + LLE cluster's third round): the C++ table's lambdas (EERenderObject.cpp's
//! `gEERustRaster`) called the C++ shells' members, which forward to these same objects (`GEModel`, `GEMaterial`,
//! `GEVertexArray`, `GETexture`: cppobj.rs reads their Rust objects in place). Each function is the lambda's
//! statements over those forwarders, in their order. The rasterizer's state, the managers, the screen and the camera
//! stay the table's.

#![allow(clippy::unnecessary_cast)] // c_ulong and c_long are u64 and i64 on LP64

use std::ffi::{c_ulong, c_void};
use std::sync::Mutex;

use ee_base::math::types::{Ge3DPoint, GeTransformation};

#[cfg(not(test))]
use crate::dx7::rastobj::{
    ee_dx7r_card_flag, ee_dx7r_set, ee_dx7r_set_blending, ee_dx7r_set_color, ee_dx7r_set_culling, ee_dx7r_set_float,
    ee_dx7r_set_fog_range, EeDx7RastColor, EeDx7RastSet, EeDx7Rasterizer,
};
use crate::ge::cppobj;
use crate::ge::modobj::{
    ee_ge_mod_compute_bounding_volumes, ee_ge_mod_compute_normals, ee_ge_mod_dereference, ee_ge_mod_get,
    ee_ge_mod_prepare_transforms, ee_ge_mod_ptr, ee_ge_mod_set_current_frame, ee_ge_mod_set_fractional,
    ee_ge_mod_set_graphics_scale, ee_ge_mod_set_next_frame, ee_ge_mod_set_opacity, ee_ge_mod_set_texture,
    ee_ge_mod_set_transform, EeGeModPtr, EeGeModVal, EeGeModXf, EeGeModel,
};
use crate::ge::rastobj::{
    ee_ge_rast_color, ee_ge_rast_get, ee_ge_rast_screen, ee_ge_rast_screen_set, EeGeRastColor, EeGeRastScreen,
    EeGeRastState, EeGeRastVal, EeGeRasterizer,
};
use crate::ge::smallobj::{
    ee_ge_mat_color, ee_ge_mat_get, ee_ge_mat_set, ee_ge_mat_set_opacity, ee_ge_mat_set_texture, EeGeMat, EeGeMatField,
};
use crate::ge::texobj_tex::{
    ee_ge_tex_dereference, ee_ge_tex_frame_texture, ee_ge_tex_get, ee_ge_tex_set_frame, ee_ge_tex_set_stage,
    EeGeTexStageField, EeGeTexVal, EeGeTexture,
};
use crate::ge::vertarr::{ee_ge_va_get, ee_ge_va_set, ee_ge_va_size, EeGeVaField, EeGeVertexArray};
use crate::gfx::beam::{self, EeGfxBeamVertex};
use crate::gfx::effects::{EeGfxColor, EeGfxTexCoord};
use crate::gfx::raster::{EeGfxFogAttributes, EeGfxModel};
use crate::gfx::tags::{ee_gfx_tags_set_color, EeGfxGraphicTags};

fn model(m: EeGfxModel) -> *mut EeGeModel {
    // SAFETY: a live GEModel (the drawers')
    unsafe { cppobj::counted::<EeGeModel>(m as usize) }
}

fn ptr(m: EeGfxModel, field: EeGeModPtr) -> *mut c_void {
    // SAFETY: a live model
    unsafe { ee_ge_mod_ptr(model(m), field) }
}

fn val(m: EeGfxModel, field: EeGeModVal) -> usize {
    // SAFETY: a live model
    unsafe { ee_ge_mod_get(model(m), field) as usize }
}

/// `GetMaterial(i)`'s Rust object.
fn material(m: EeGfxModel, i: usize) -> *mut EeGeMat {
    // SAFETY: the model's materials (the C++ indexed them alike)
    unsafe { cppobj::counted::<EeGeMat>(*(ptr(m, EeGeModPtr::Materials) as *const usize).add(i)) }
}

/// `GetSubModel(i)`.
pub(crate) fn sub_model(m: EeGfxModel, i: u32) -> EeGfxModel {
    // SAFETY: the model's sub-models
    unsafe { *(ptr(m, EeGeModPtr::SubModels) as *const EeGfxModel).add(i as usize) }
}

/// `GetVertexArray(0)`'s Rust object (the game's arrays are the base class: cppobj.rs `CppArray`).
fn vertices(m: EeGfxModel) -> *mut EeGeVertexArray {
    // SAFETY: the model's vertex arrays; a live array
    unsafe { (*(*(ptr(m, EeGeModPtr::VertexArrays) as *const *const cppobj::CppArray))).rust as *mut EeGeVertexArray }
}

fn texture(t: EeGfxModel) -> *mut EeGeTexture {
    // SAFETY: a live GETexture
    unsafe { cppobj::counted::<EeGeTexture>(t as usize) }
}

fn internal0(m: EeGfxModel) -> GeTransformation {
    // SAFETY: the model's per-frame transformations
    unsafe { *(ptr(m, EeGeModPtr::InternalTransforms) as *const GeTransformation) }
}

fn set_transform(m: EeGfxModel, which: EeGeModXf, frame: c_ulong, t: &GeTransformation) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_set_transform(model(m), which, frame, t) }
}

/// The vertex array's size (`GetSize()`).
pub(crate) fn vertex_count(m: EeGfxModel) -> u32 {
    // SAFETY: a live array
    unsafe { ee_ge_va_size(vertices(m), false) as u32 }
}

fn get<const N: usize>(a: *mut EeGeVertexArray, field: EeGeVaField, i: u32) -> [f32; N] {
    let mut out = [0.0; N];
    // SAFETY: a live array; room for the field's floats
    unsafe { ee_ge_va_get(a, field, i, out.as_mut_ptr()) };
    out
}

fn set<const N: usize>(a: *mut EeGeVertexArray, field: EeGeVaField, i: u32, value: [f32; N]) {
    // SAFETY: a live array
    unsafe { ee_ge_va_set(a, field, i, value.as_ptr()) }
}

/// `GetVertexArray(0)->GetPoint(point, index)`.
pub(crate) fn vertex_point(m: EeGfxModel, index: u32) -> Ge3DPoint {
    let [x, y, z] = get(vertices(m), EeGeVaField::Point, index);
    Ge3DPoint { x, y, z }
}

/// `GetVertexArray(0)->SetPoint(point, index)`.
pub(crate) fn set_vertex_point(m: EeGfxModel, index: u32, p: &Ge3DPoint) {
    set(vertices(m), EeGeVaField::Point, index, [p.x, p.y, p.z]);
}

pub(crate) fn opacity(m: EeGfxModel, opacity: f32) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_set_opacity(model(m), opacity) }
}

pub(crate) fn transform(m: EeGfxModel, t: &GeTransformation) {
    set_transform(m, EeGeModXf::Post, 0, t);
}

pub(crate) fn pre_transform(m: EeGfxModel, t: &GeTransformation) {
    set_transform(m, EeGeModXf::Pre, 0, t);
}

/// The post-model transformation `t` scaled by (x, y, z) (`GETransformation::Scale`).
pub(crate) fn transform_scaled(m: EeGfxModel, t: &GeTransformation, x: f32, y: f32, z: f32) {
    let mut scaled = *t;
    scaled.scale(x, y, z);
    set_transform(m, EeGeModXf::Post, 0, &scaled);
}

pub(crate) fn scale(m: EeGfxModel, scale: f32) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_set_graphics_scale(model(m), scale) }
}

pub(crate) fn prepare(m: EeGfxModel) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_prepare_transforms(model(m)) }
}

pub(crate) fn frame(m: EeGfxModel, frame: u32) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_set_current_frame(model(m), frame as c_ulong) }
}

/// `SetGraphicsNextFrame(next)`, then `SetFractionalFramePosition(fraction)`.
pub(crate) fn tween(m: EeGfxModel, next: u32, fraction: f32) {
    // SAFETY: a live model
    unsafe {
        ee_ge_mod_set_next_frame(model(m), next as c_ulong);
        ee_ge_mod_set_fractional(model(m), fraction);
    }
}

pub(crate) fn bounds(m: EeGfxModel) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_compute_bounding_volumes(model(m)) }
}

pub(crate) fn normals(m: EeGfxModel) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_compute_normals(model(m)) }
}

pub(crate) fn reference(m: EeGfxModel) {
    // SAFETY: a live reference-counted object
    unsafe { cppobj::reference(m as usize) }
}

pub(crate) fn dereference(m: EeGfxModel) {
    // SAFETY: a live model (`GEModel::Dereference`)
    unsafe { ee_ge_mod_dereference(model(m)) }
}

/// `GetBoundingRadius(0)`.
pub(crate) fn bounding_radius(m: EeGfxModel) -> f32 {
    // SAFETY: the model's per-frame radii
    unsafe { *(ptr(m, EeGeModPtr::BoundingRadii) as *const f32) }
}

/// `GetMaterial(0)->SetTexture(texture, screen)`.
pub(crate) fn material_texture(m: EeGfxModel, t: EeGfxModel, screen: u32) {
    // SAFETY: a live material
    unsafe { ee_ge_mat_set_texture(material(m, 0), t, screen as c_ulong) }
}

/// `SetTexture(texture, screen)` (every material's).
pub(crate) fn own_texture(m: EeGfxModel, t: EeGfxModel, screen: u32) {
    // SAFETY: a live model
    unsafe { ee_ge_mod_set_texture(model(m), t, screen as c_ulong) }
}

/// `GetMaterial(0)->SetColor(color)`.
pub(crate) fn color(m: EeGfxModel, c: EeGfxColor) {
    // SAFETY: the material's colour (three floats)
    unsafe { *(ee_ge_mat_color(material(m, 0)) as *mut [f32; 3]) = [c.red, c.green, c.blue] }
}

/// `GetMaterial(0)->SetOpacity(opacity)`.
pub(crate) fn material_opacity(m: EeGfxModel, opacity: f32) {
    // SAFETY: a live material
    unsafe { ee_ge_mat_set_opacity(material(m, 0), opacity) }
}

/// Material 0's ranges: polygons from 0 (LOD 0), vertices from 0.
pub(crate) fn material_ranges(m: EeGfxModel, polygons: u32, vertices: u32) {
    let mat = material(m, 0);
    // SAFETY: a live material
    unsafe {
        ee_ge_mat_set(mat, EeGeMatField::PolygonStart, 0, 0);
        ee_ge_mat_set(mat, EeGeMatField::PolygonLength, 0, polygons as c_ulong);
        ee_ge_mat_set(mat, EeGeMatField::VertexStart, 0, 0);
        ee_ge_mat_set(mat, EeGeMatField::VertexLength, 0, vertices as c_ulong);
    }
}

/// Vertex array 0's points (and normals) copied from `from` to `to`, by `from`'s size.
pub(crate) fn copy_vertices(from: EeGfxModel, to: EeGfxModel, with_normals: bool) {
    let (a, b) = (vertices(from), vertices(to));
    // SAFETY: a live array
    let n = unsafe { ee_ge_va_size(a, false) } as u32;
    for i in 0..n {
        set::<3>(b, EeGeVaField::Point, i, get(a, EeGeVaField::Point, i));
        if with_normals {
            set::<3>(b, EeGeVaField::Normal, i, get(a, EeGeVaField::Normal, i));
        }
    }
}

/// What the texture-coordinate scrolls changed (`model_scroll_v` / `_u`), put back by the restores (the graphics
/// thread's, as the C++'s statics).
static KEPT_V: Mutex<Vec<f32>> = Mutex::new(Vec::new());
static KEPT_U: Mutex<Vec<f32>> = Mutex::new(Vec::new());

/// Each vertex's coordinate `which` (0 u, 1 v) moved by `delta`, the old values kept.
fn scroll(m: EeGfxModel, which: usize, delta: f32, kept: Option<&Mutex<Vec<f32>>>) {
    scroll_array(vertices(m), which, delta, kept);
}

fn scroll_array(a: *mut EeGeVertexArray, which: usize, delta: f32, kept: Option<&Mutex<Vec<f32>>>) {
    let mut kept = kept.map(|k| k.lock().unwrap());
    if let Some(k) = kept.as_mut() {
        k.clear();
    }
    // SAFETY: a live array
    let n = unsafe { ee_ge_va_size(a, false) } as u32;
    for i in 0..n {
        let mut uv: [f32; 2] = get(a, EeGeVaField::Uv, i);
        if let Some(k) = kept.as_mut() {
            k.push(uv[which]);
        }
        uv[which] += delta;
        set(a, EeGeVaField::Uv, i, uv);
    }
}

fn restore(m: EeGfxModel, which: usize, kept: &Mutex<Vec<f32>>) {
    restore_array(vertices(m), which, kept);
}

fn restore_array(a: *mut EeGeVertexArray, which: usize, kept: &Mutex<Vec<f32>>) {
    let kept = kept.lock().unwrap();
    // SAFETY: a live array
    let n = unsafe { ee_ge_va_size(a, false) } as u32;
    for i in 0..n {
        let mut uv: [f32; 2] = get(a, EeGeVaField::Uv, i);
        uv[which] = kept[i as usize];
        set(a, EeGeVaField::Uv, i, uv);
    }
}

pub(crate) fn scroll_v(m: EeGfxModel, delta: f32) {
    scroll(m, 1, delta, Some(&KEPT_V));
}

pub(crate) fn restore_v(m: EeGfxModel) {
    restore(m, 1, &KEPT_V);
}

pub(crate) fn scroll_u(m: EeGfxModel, delta: f32) {
    scroll(m, 0, delta, Some(&KEPT_U));
}

pub(crate) fn restore_u(m: EeGfxModel) {
    restore(m, 0, &KEPT_U);
}

/// The v coordinates moved by `delta`, nothing kept.
pub(crate) fn shift_v(m: EeGfxModel, delta: f32) {
    scroll(m, 1, delta, None);
}

/// The tracer beam's vertices and its sub-models' internal ys before the stretch.
static KEPT_BEAM: Mutex<(Vec<EeGfxBeamVertex>, Vec<f32>)> = Mutex::new((Vec::new(), Vec::new()));

fn set_beam_vertices(a: *mut EeGeVertexArray, values: &[EeGfxBeamVertex]) {
    for (i, v) in values.iter().enumerate() {
        set(a, EeGeVaField::Point, i as u32, [v.point.x, v.point.y, v.point.z]);
        set(a, EeGeVaField::Uv, i as u32, [v.coord.u, v.coord.v]);
    }
}

/// EEE3DTracerBeam::DrawPass6's stretch (0xfca9b): vertex array 0 kept and stretched (beam.rs), the sub-models'
/// internal ys kept and stretched, the bounding volumes.
pub(crate) fn stretch(m: EeGfxModel, length: f32, width: f32, scroll: f32) {
    let a = vertices(m);
    let mut kept = KEPT_BEAM.lock().unwrap();
    // SAFETY: a live array
    let n = unsafe { ee_ge_va_size(a, false) } as u32;
    kept.0 = (0..n)
        .map(|i| {
            let [x, y, z] = get(a, EeGeVaField::Point, i);
            let [u, v] = get(a, EeGeVaField::Uv, i);
            EeGfxBeamVertex { point: Ge3DPoint { x, y, z }, coord: EeGfxTexCoord { u, v } }
        })
        .collect();
    let mut stretched = kept.0.clone();
    beam::stretch(&mut stretched, length, width, scroll);
    set_beam_vertices(a, &stretched);
    kept.1.clear();
    for j in 0..val(m, EeGeModVal::SubModelCount) as u32 {
        let sub = sub_model(m, j);
        let mut t = internal0(sub);
        kept.1.push(t.y());
        t.m[1][3] = beam::stretch_y(t.y(), length);
        set_transform(sub, EeGeModXf::Internal, 0, &t);
    }
    drop(kept);
    bounds(m);
}

/// The stretch put back: the vertices, the sub-models' internal ys.
pub(crate) fn unstretch(m: EeGfxModel) {
    let kept = KEPT_BEAM.lock().unwrap();
    set_beam_vertices(vertices(m), &kept.0);
    for j in 0..val(m, EeGeModVal::SubModelCount) as u32 {
        let sub = sub_model(m, j);
        let mut t = internal0(sub);
        t.m[1][3] = kept.1[j as usize];
        set_transform(sub, EeGeModXf::Internal, 0, &t);
    }
}

/// A gate side's sub-model 0 placed against its internal offset (EEROUnits::DrawUnitModel): its post-model
/// transformation's position minus the internal one's times `scale`; the transformation set.
pub(crate) fn sub_model_offset(m: EeGfxModel, scale: f32) -> GeTransformation {
    let sub = sub_model(m, 0);
    // SAFETY: the sub-model's post transformation
    let mut t = unsafe { *(ptr(sub, EeGeModPtr::PostTransform) as *const GeTransformation) };
    let i = internal0(sub);
    // (the C++'s `* -1.0f`: a negation, bit for bit)
    t.m[0][3] = -(i.x() * scale);
    t.m[1][3] = -(i.y() * scale);
    t.m[2][3] = -(i.z() * scale);
    set_transform(sub, EeGeModXf::Post, 0, &t);
    t
}

fn frame_count(t: EeGfxModel) -> c_ulong {
    // SAFETY: a live texture
    unsafe { ee_ge_tex_get(texture(t), EeGeTexVal::FrameCount) as c_ulong }
}

fn frame_texture(t: EeGfxModel, frame: c_ulong) -> EeGfxModel {
    // SAFETY: a live texture
    unsafe { ee_ge_tex_frame_texture(texture(t), frame) }
}

fn texture_index(mat: *mut EeGeMat) -> c_ulong {
    // SAFETY: a live material
    unsafe { ee_ge_mat_get(mat, EeGeMatField::TextureIndex, 0) }
}

/// A unit model's frame textures (EEROUnits::DrawUnitModel 0x515cc7: past the frame count, frame 0): for each
/// material after the first, its frame of `texture`, then every sub-model's materials' frames (those in range).
pub(crate) fn frame_textures(m: EeGfxModel, t: EeGfxModel, screen: u32) {
    for i in 1..val(m, EeGeModVal::MaterialCount) {
        if t.is_null() {
            continue;
        }
        let first = material(m, i);
        let index = texture_index(first);
        let frame = if index < frame_count(t) { frame_texture(t, index) } else { frame_texture(t, 0) };
        // SAFETY: a live material
        unsafe { ee_ge_mat_set_texture(first, frame, screen as c_ulong) };
        for s in 0..val(m, EeGeModVal::SubModelCount) as u32 {
            let sub = sub_model(m, s);
            for j in 0..val(sub, EeGeModVal::MaterialCount) {
                let other = material(sub, j);
                let index = texture_index(other);
                if index < frame_count(t) {
                    // SAFETY: a live material
                    unsafe { ee_ge_mat_set_texture(other, frame_texture(t, index), screen as c_ulong) };
                }
            }
        }
    }
}

pub(crate) fn texture_frame(t: EeGfxModel, frame: u32) {
    // SAFETY: a live texture
    unsafe { ee_ge_tex_set_frame(texture(t), frame as c_ulong) }
}

pub(crate) fn texture_wrap(t: EeGfxModel) {
    // SAFETY: a live texture
    unsafe { ee_ge_tex_set_stage(texture(t), EeGeTexStageField::TextureAddressingMode, 2) }
}

pub(crate) fn texture_dereference(t: EeGfxModel) {
    // SAFETY: a live texture
    unsafe { ee_ge_tex_dereference(texture(t)) }
}

/// `SetGroupColor(color, part)` of a graphic object (`EEGraphicObject*`): its tag store's colour groups (tags.rs).
pub(crate) fn group_color(object: EeGfxModel, c: EeGfxColor, part: u32) {
    if object.is_null() {
        return;
    }
    // SAFETY: a live graphic object's `mTags` (the registered layout's)
    unsafe {
        let tags = std::ptr::read_unaligned(
            (object as usize + crate::gfx::effect_reads::layout().gobj_tags as usize) as *const *const EeGfxGraphicTags,
        );
        ee_gfx_tags_set_color(tags, part as c_ulong, [c.red, c.green, c.blue].as_ptr());
    }
}

// ---- the rasterizer's state (the DX7 rasterizer's overrides and the base's inline getters: the game's only
// rasterizer is the DX7 one, its two Rust objects given by the table's `rasterizer`)

/// The current rasterizer's Rust objects and the screen.
#[derive(Clone, Copy)]
pub(crate) struct Rast {
    pub(crate) dx7: *mut EeDx7Rasterizer,
    pub(crate) ge: *mut EeGeRasterizer,
    pub(crate) screen: c_ulong,
}

impl Rast {
    fn screen_state(self) -> &'static EeGeRastScreen {
        // SAFETY: the live rasterizer's screen record (read before the next state call)
        unsafe { &*ee_ge_rast_screen(self.ge, self.screen) }
    }
    fn set(self, which: EeDx7RastSet, value: u32) {
        // SAFETY: the live rasterizer
        unsafe { ee_dx7r_set(self.dx7, which, value as c_ulong, self.screen) }
    }
    pub(crate) fn set_fog(self, state: u32) {
        self.set(EeDx7RastSet::Fog, state)
    }
    pub(crate) fn set_zbuffer(self, mode: u32) {
        self.set(EeDx7RastSet::ZBuffer, mode)
    }
    pub(crate) fn set_zcompare(self, mode: u32) {
        self.set(EeDx7RastSet::ZCompare, mode)
    }
    pub(crate) fn set_shading_source(self, source: u32) {
        self.set(EeDx7RastSet::ShadingColorSource, source)
    }
    pub(crate) fn set_alpha_testing(self, state: u32) {
        self.set(EeDx7RastSet::AlphaTesting, state)
    }
    pub(crate) fn set_ambient(self, c: EeGfxColor) {
        self.set_color(EeDx7RastColor::Ambient, c)
    }
    pub(crate) fn set_fog_color(self, c: EeGfxColor) {
        self.set_color(EeDx7RastColor::Fog, c)
    }
    pub(crate) fn set_alpha_ref(self, value: f32) {
        // SAFETY: as above
        unsafe { ee_dx7r_set_float(self.dx7, false, value, self.screen) }
    }
    pub(crate) fn set_ambient_intensity(self, value: f32) {
        // SAFETY: as above
        unsafe { ee_dx7r_set_float(self.dx7, true, value, self.screen) }
    }
    fn set_color(self, which: EeDx7RastColor, c: EeGfxColor) {
        let color = EeGeRastColor { red: c.red, green: c.green, blue: c.blue };
        // SAFETY: as above
        unsafe { ee_dx7r_set_color(self.dx7, which, &color, self.screen) }
    }
    pub(crate) fn set_blending(self, source: u32, destination: u32) {
        // SAFETY: as above
        unsafe { ee_dx7r_set_blending(self.dx7, source as c_ulong, destination as c_ulong, self.screen) }
    }
    pub(crate) fn set_culling(self, mode: u32) {
        // SAFETY: as above
        unsafe { ee_dx7r_set_culling(self.dx7, mode as c_ulong) }
    }
    pub(crate) fn set_fog_range(self, start: f32, end: f32) {
        // SAFETY: as above
        unsafe { ee_dx7r_set_fog_range(self.dx7, start, end, self.screen) }
    }
    /// `SetFogType`: the base's (the DX7 rasterizer does not override it).
    pub(crate) fn set_fog_type(self, kind: u32) {
        // SAFETY: as above
        unsafe { ee_ge_rast_screen_set(self.ge, self.screen, EeGeRastState::FogType, kind as isize) }
    }
    pub(crate) fn can_modulate_alpha(self) -> bool {
        // SAFETY: as above
        unsafe { ee_dx7r_card_flag(self.dx7, self.screen, false) }
    }
    pub(crate) fn ambient(self) -> EeGfxColor {
        // SAFETY: the rasterizer's ambient colour
        let c = unsafe { *ee_ge_rast_color(self.ge, false) };
        EeGfxColor { red: c.red, green: c.green, blue: c.blue }
    }
    pub(crate) fn culling(self) -> u32 {
        // SAFETY: the live rasterizer
        unsafe { ee_ge_rast_get(self.ge, EeGeRastVal::Culling) as u32 }
    }
    pub(crate) fn fog(self) -> u32 {
        self.screen_state().fog_state as u32
    }
    pub(crate) fn zbuffer(self) -> u32 {
        self.screen_state().z_buffer as u32
    }
    pub(crate) fn zcompare(self) -> u32 {
        self.screen_state().z_compare as u32
    }
    pub(crate) fn alpha_testing(self) -> u32 {
        self.screen_state().alpha_testing as u32
    }
    pub(crate) fn alpha_ref(self) -> f32 {
        self.screen_state().alpha_testing_ref_value
    }
    pub(crate) fn blending(self) -> (u32, u32) {
        let s = self.screen_state();
        (s.blend_source as u32, s.blend_destination as u32)
    }
    pub(crate) fn fog_attributes(self) -> EeGfxFogAttributes {
        let f = self.screen_state().fog;
        EeGfxFogAttributes {
            kind: f.kind as u32,
            color: EeGfxColor { red: f.color.red, green: f.color.green, blue: f.color.blue },
            start: f.start,
            end: f.end,
            scale: f.scale,
        }
    }
}

// the test builds have no DX7 rasterizer (rastobj.rs imports the C++): its calls are never made there
#[cfg(test)]
use no_dx7::*;
#[cfg(test)]
mod no_dx7 {
    use super::*;
    pub(crate) struct EeDx7Rasterizer;
    #[derive(Clone, Copy)]
    pub(crate) enum EeDx7RastSet {
        AlphaTesting,
        Fog,
        ShadingColorSource,
        ZBuffer,
        ZCompare,
    }
    #[derive(Clone, Copy)]
    pub(crate) enum EeDx7RastColor {
        Ambient,
        Fog,
    }
    pub(crate) unsafe fn ee_dx7r_set(_: *mut EeDx7Rasterizer, _: EeDx7RastSet, _: c_ulong, _: c_ulong) {
        unreachable!()
    }
    pub(crate) unsafe fn ee_dx7r_set_float(_: *mut EeDx7Rasterizer, _: bool, _: f32, _: c_ulong) {
        unreachable!()
    }
    pub(crate) unsafe fn ee_dx7r_set_color(
        _: *mut EeDx7Rasterizer,
        _: EeDx7RastColor,
        _: *const EeGeRastColor,
        _: c_ulong,
    ) {
        unreachable!()
    }
    pub(crate) unsafe fn ee_dx7r_set_blending(_: *mut EeDx7Rasterizer, _: c_ulong, _: c_ulong, _: c_ulong) {
        unreachable!()
    }
    pub(crate) unsafe fn ee_dx7r_set_culling(_: *mut EeDx7Rasterizer, _: c_ulong) {
        unreachable!()
    }
    pub(crate) unsafe fn ee_dx7r_set_fog_range(_: *mut EeDx7Rasterizer, _: f32, _: f32, _: c_ulong) {
        unreachable!()
    }
    pub(crate) unsafe fn ee_dx7r_card_flag(_: *mut EeDx7Rasterizer, _: c_ulong, _: bool) -> bool {
        unreachable!()
    }
}

#[cfg(test)]
#[path = "raster_direct_tests.rs"]
mod tests;
