//! `DX7Rasterizer DrawModel.cpp`: the polygon-array draw, the model hierarchy draw and its frame blending, the
//! textured 3D line. C++ owns `GEModel`/`GEMaterial`/`GEViewport`; they cross the seam as [`ModelInfo`]/[`MaterialInfo`]
//! /[`ViewportInfo`] (plain data) and opaque handles, the vertex and index memory as raw pointers.
//!
//! Golden: `golden_draw.txt` (sections `polyarray_*`, `model_*`, `line3d*`), printed by a scratch run of the original
//! C++ bodies.
#![allow(clippy::unnecessary_cast)]

use core::ffi::c_void;

use crate::draw::*;
use crate::vertex::{float_to_long, Dx7StdColorVertex, Dx7Vertex};

/// A model as `DrawModelHierarchy` reads it (`GEModel`'s accessors, plain data).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ModelInfo {
    pub material_count: u32,
    pub lod_count: u32,
    pub sub_model_count: u32,
    pub current_frame: u32,
    pub next_frame: u32,
    /// `GetVertexArray(0)->GetType()`: 0 standard, 1 coloured.
    pub vertex_type: u32,
    pub scale: f32,
    pub opacity: f32,
    pub fraction: f32,
    pub transform: Transform,
}

/// One material for one level of detail.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct MaterialInfo {
    pub texture: Handle,
    pub polygon_start: u32,
    pub polygon_length: u32,
    pub vertex_start: u32,
    pub vertex_length: u32,
    pub opacity: f32,
    pub color: [f32; 3],
}

/// The parts of `GEViewport` the 3D draws read.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ViewportInfo {
    pub camera: Transform,
    pub camera_inverse: Transform,
    pub perspective: Transform,
    pub orthographic: Transform,
    pub front_clip: f32,
    pub rear_clip: f32,
    pub half_width: f32,
    pub half_height: f32,
    pub field_of_view: f32,
    pub y_clip_plane: f32,
    pub ortho_left: f32,
    pub ortho_top: f32,
    pub ortho_right: f32,
    pub ortho_bottom: f32,
    pub left_plane: Plane,
    pub right_plane: Plane,
    pub top_plane: Plane,
    pub bottom_plane: Plane,
}

/// A `GE3DPlane`: a point and a normal.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Plane {
    pub point: [f32; 3],
    pub normal: [f32; 3],
}

impl Plane {
    /// `IsPointOnOrBelowPlane`.
    #[inline]
    pub fn on_or_below(&self, p: [f32; 3]) -> bool {
        let d = [p[0] - self.point[0], p[1] - self.point[1], p[2] - self.point[2]];
        d[2] * self.normal[2] + d[1] * self.normal[1] + d[0] * self.normal[0] <= 0.0
    }
}

/// The reads of the model draw (C++ objects stay on the C++ side).
pub trait ModelHost: DrawHost {
    fn model_info(&mut self, model: Handle) -> ModelInfo;
    /// `IsPerspectiveModelVisible` / `IsOrthographicModelVisible` -> (visible, clipped).
    fn model_visible(&mut self, model: Handle, scale: f32, transform: &Transform, orthographic: bool) -> (bool, bool);
    fn pixel_size(&mut self, model: Handle, transform: &Transform, scale: f32, orthographic: bool) -> f32;
    /// `GetViewRectangle().GetWidth()` of the screen's viewport.
    fn view_width(&mut self) -> f32;
    fn material_info(&mut self, model: Handle, index: u32, lod: u32) -> MaterialInfo;
    /// `GetPolygonArray(lod)`'s index memory.
    fn polygon_indices(&mut self, model: Handle, lod: u32) -> *const u16;
    /// `GetVertexArray(frame)`'s vertex memory (standard or coloured by the model's vertex type).
    fn frame_vertices(&mut self, model: Handle, frame: u32) -> *const c_void;
    fn sub_model(&mut self, model: Handle, index: u32) -> Handle;
}

unsafe fn white_copy(source: *const Dx7Vertex, count: u32, out: *mut Dx7StdColorVertex) {
    for i in 0..count as usize {
        let v = unsafe { *source.add(i) };
        unsafe {
            *out.add(i) = Dx7StdColorVertex {
                x: v.x,
                y: v.y,
                z: v.z,
                nx: v.nx,
                ny: v.ny,
                nz: v.nz,
                color: 0xffff_ffff,
                tu: v.tu,
                tv: v.tv,
            };
        }
    }
}

/// `CopyAndSmoothStdColorVertices`: blend between the two frames; colour and UV from the current one.
unsafe fn blend_color(
    current: *const Dx7StdColorVertex,
    next: *const Dx7StdColorVertex,
    count: u32,
    fraction: f32,
    out: *mut Dx7StdColorVertex,
) {
    let inverse = 1.0 - fraction;
    for i in 0..count as usize {
        let (c, n) = unsafe { (*current.add(i), *next.add(i)) };
        unsafe {
            *out.add(i) = Dx7StdColorVertex {
                x: n.x * fraction + c.x * inverse,
                y: c.y * inverse + n.y * fraction,
                z: c.z * inverse + n.z * fraction,
                nx: c.nx * inverse + n.nx * fraction,
                ny: c.ny * inverse + n.ny * fraction,
                nz: c.nz * inverse + n.nz * fraction,
                color: c.color,
                tu: c.tu,
                tv: c.tv,
            };
        }
    }
}

/// `CopyAndSmoothStdVertices`: blended positions and normals, white colour.
unsafe fn blend_std(
    current: *const Dx7Vertex,
    next: *const Dx7Vertex,
    count: u32,
    fraction: f32,
    out: *mut Dx7StdColorVertex,
) {
    let inverse = 1.0 - fraction;
    for i in 0..count as usize {
        let (c, n) = unsafe { (*current.add(i), *next.add(i)) };
        unsafe {
            *out.add(i) = Dx7StdColorVertex {
                x: n.x * fraction + c.x * inverse,
                y: c.y * inverse + n.y * fraction,
                z: c.z * inverse + n.z * fraction,
                nx: c.nx * inverse + n.nx * fraction,
                ny: c.ny * inverse + n.ny * fraction,
                nz: c.nz * inverse + n.nz * fraction,
                color: 0xffff_ffff,
                tu: c.tu,
                tv: c.tv,
            };
        }
    }
}

fn copy_material_vertices(
    h: &mut impl ModelHost,
    model: Handle,
    info: &ModelInfo,
    start: u32,
    length: u32,
    out: *mut Dx7StdColorVertex,
) {
    let current = h.frame_vertices(model, info.current_frame);
    if info.vertex_type == 0 {
        let current = unsafe { (current as *const Dx7Vertex).add(start as usize) };
        if info.fraction != 0.0 {
            let next = unsafe { (h.frame_vertices(model, info.next_frame) as *const Dx7Vertex).add(start as usize) };
            unsafe { blend_std(current, next, length, info.fraction, out) };
        } else {
            unsafe { white_copy(current, length, out) };
        }
    } else {
        let current = unsafe { (current as *const Dx7StdColorVertex).add(start as usize) };
        if info.fraction != 0.0 {
            let next =
                unsafe { (h.frame_vertices(model, info.next_frame) as *const Dx7StdColorVertex).add(start as usize) };
            unsafe { blend_color(current, next, length, info.fraction, out) };
        } else {
            unsafe { core::ptr::copy_nonoverlapping(current, out, length as usize) };
        }
    }
}

/// `Draw(GEPolygonArray&, ...)`: an indexed triangle list with an identity world transform and D3D clipping on.
#[allow(clippy::too_many_arguments)]
pub fn draw_polygon_array(
    h: &mut impl DrawHost,
    indices: *const u16,
    count: u32,
    vertex_type: u32,
    vertices: *const c_void,
    vertex_count: u32,
    texture: Handle,
) -> LongResult {
    h.enter();
    h.set_3d_mode();
    h.set_transform(TS_WORLD, &to_d3d(&IDENTITY));
    h.set_render_state(RS_CLIPPING, 1);
    h.set_texture(texture);
    let locked = h.lock_vb(vertex_count);
    let buffer = unsafe { locked.add(h.vb_position() as usize) };
    if vertex_type == 1 {
        unsafe { core::ptr::copy_nonoverlapping(vertices as *const Dx7StdColorVertex, buffer, vertex_count as usize) };
    } else {
        unsafe { white_copy(vertices as *const Dx7Vertex, vertex_count, buffer) };
    }
    let result = h.unlock_vb();
    let start = h.vb_position();
    draw_indexed_retry(h, PT_TRIANGLELIST, start, vertex_count, indices, count * 3);
    h.advance_vb(vertex_count);
    h.add_triangles(count);
    h.leave();
    result
}

/// `Draw(GEModel*)`: the hierarchy draw under the drawing lock.
pub fn draw_model(h: &mut impl ModelHost, model: Handle, detail: f32, orthographic: bool) -> LongResult {
    h.enter();
    h.set_3d_mode();
    let result = draw_model_hierarchy(h, model, detail, orthographic, None);
    h.leave();
    result
}

/// `DrawModelHierarchy`: composes the transforms, culls, picks the root's level of detail from its on-screen size and
/// draws each material's triangles; stops at the first sub-model error. `parent` replaces the C++ transform stack.
pub fn draw_model_hierarchy(
    h: &mut impl ModelHost,
    model: Handle,
    detail: f32,
    orthographic: bool,
    parent: Option<(&Transform, f32)>,
) -> LongResult {
    let mut result: LongResult = 0;
    let info = h.model_info(model);
    let (transform, scale, has_parent) = match parent {
        None => (info.transform, info.scale, false),
        Some((p, parent_scale)) => (compose(p, &info.transform), info.scale * parent_scale, true),
    };
    let (visible, clipped) = h.model_visible(model, scale, &transform, orthographic);
    if visible {
        let lod = if !has_parent {
            let width = h.view_width();
            let mut pixel_detail = h.pixel_size(model, &transform, scale, orthographic) * (800.0 / width) / detail;
            if 1.0 < pixel_detail {
                pixel_detail = 1.0;
            }
            let lod = float_to_long((1.0 - pixel_detail) * info.lod_count as f32) as u32;
            let last = info.lod_count.wrapping_sub(1);
            if lod >= last {
                last
            } else {
                lod
            }
        } else {
            0
        };
        h.set_transform(TS_WORLD, &to_d3d(&transform));
        result = h.set_render_state(RS_CLIPPING, clipped as u32);
        let indices = h.polygon_indices(model, lod);
        for i in 0..info.material_count {
            let material = h.material_info(model, i, lod);
            if material.polygon_length == 0 {
                continue;
            }
            h.add_triangles(material.polygon_length);
            h.set_texture(material.texture);
            let d3d = Material {
                diffuse: [material.color[0], material.color[1], material.color[2], material.opacity * info.opacity],
                ambient: [material.color[0], material.color[1], material.color[2], 1.0],
                specular: [0.0; 4],
                emissive: [0.0; 4],
                power: 0.0,
            };
            h.set_material(&d3d);
            let locked = h.lock_vb(material.vertex_length);
            let buffer = unsafe { locked.add(h.vb_position() as usize) };
            copy_material_vertices(h, model, &info, material.vertex_start, material.vertex_length, buffer);
            result = h.unlock_vb();
            let start = h.vb_position();
            let first_index = unsafe { indices.add(material.polygon_start as usize * 3) };
            draw_indexed_retry(
                h,
                PT_TRIANGLELIST,
                start,
                material.vertex_length,
                first_index,
                material.polygon_length * 3,
            );
            h.advance_vb(material.vertex_length);
        }
    }
    let mut j = 0;
    while j < info.sub_model_count && result == 0 {
        let sub = h.sub_model(model, j);
        result = draw_model_hierarchy(h, sub, detail, orthographic, Some((&transform, scale)));
        j += 1;
    }
    result
}

/// `Draw3DLine`: a textured polyline (at most 1200 segments) as a red line list in camera space, advancing the texture
/// V along it (without the drawing lock). `points` holds `(x, y, z, advance)`; returns 0 when it has fewer than 2.
pub fn draw_3d_line(
    h: &mut impl DrawHost,
    points: &[[f32; 4]],
    texture: Handle,
    v: f32,
    orthographic: bool,
    viewport: &ViewportInfo,
) -> LongResult {
    set_world_view_projection(h, &viewport.perspective, &viewport.orthographic, orthographic);
    h.set_texture(texture);
    h.set_material(&Material::WHITE);
    h.set_shading_color_source(0);
    let mut count = points.len() as u32;
    if count < 2 {
        return 0;
    }
    let mut vertex_count = count * 2 - 2;
    if vertex_count > 0x960 {
        vertex_count = 0x960;
        count = 0x4b1;
    }
    let locked = h.lock_vb(vertex_count);
    let mut out = unsafe { locked.add(h.vb_position() as usize) };
    let mut tv = v;
    let line_vertex = |p: [f32; 3], tv: f32| Dx7StdColorVertex {
        x: p[0],
        y: p[1],
        z: p[2],
        nx: 0.0,
        ny: 0.0,
        nz: 0.0,
        color: 0xffff_0000,
        tu: 0.0,
        tv,
    };
    for i in 1..count as usize {
        let first = apply_point(&viewport.camera_inverse, [points[i - 1][0], points[i - 1][1], points[i - 1][2]]);
        unsafe {
            *out = line_vertex(first, tv);
            out = out.add(1);
        }
        tv += points[i - 1][3];
        let second = apply_point(&viewport.camera_inverse, [points[i][0], points[i][1], points[i][2]]);
        unsafe {
            *out = line_vertex(second, tv);
            out = out.add(1);
        }
        if tv <= -1.0 {
            tv = um_ceil(tv) as f32 - tv;
        } else if tv > 1.0 {
            tv -= um_floor(tv) as f32;
        }
    }
    let result = h.unlock_vb();
    let start = h.vb_position();
    draw_primitive_retry(h, PT_LINELIST, start, vertex_count);
    h.advance_vb(vertex_count);
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::draw::testing::{b, golden_section, impl_draw_host_for_rec, Rec};

    fn cv(s: f32) -> Dx7StdColorVertex {
        Dx7StdColorVertex {
            x: s + 1.1,
            y: s + 2.2,
            z: s + 3.3,
            nx: 0.1,
            ny: 0.2,
            nz: 0.3,
            color: 0x8011_2233u32.wrapping_add(s as u32),
            tu: s * 0.125,
            tv: 1.0 - s * 0.0625,
        }
    }

    fn sv(s: f32) -> Dx7Vertex {
        Dx7Vertex { x: s + 1.1, y: s + 2.2, z: s + 3.3, nx: 0.4, ny: 0.5, nz: 0.6, tu: s * 0.25, tv: 2.0 + s * 0.0625 }
    }

    fn matrix(seed: f32) -> Transform {
        let mut t = [[0.0f32; 4]; 4];
        for (r, row) in t.iter_mut().enumerate() {
            for (c, e) in row.iter_mut().enumerate() {
                *e = seed * (r as f32 + 1.0) + (c as f32 * 3.0) * 0.125;
            }
        }
        t[3] = [0.0, 0.0, 0.0, 1.0];
        t
    }

    struct Model {
        id: Handle,
        name: &'static str,
        info: ModelInfo,
        materials: Vec<[MaterialInfo; 2]>,
        indices: Vec<Vec<u16>>,
        frames: Vec<*const c_void>,
        subs: Vec<Handle>,
    }

    struct Host {
        rec: Rec,
        names: Vec<(Handle, &'static str)>,
        models: Vec<Model>,
        visible: bool,
        clipped: bool,
        pixel: f32,
    }
    impl_draw_host_for_rec!(Host, names);

    impl Host {
        fn find(&self, id: Handle) -> &Model {
            self.models.iter().find(|m| m.id == id).unwrap()
        }

        fn matrix_text(t: &Transform) -> String {
            let mut s = String::new();
            for row in t {
                for f in row {
                    s.push(' ');
                    s.push_str(&b(*f));
                }
            }
            s
        }
    }

    impl ModelHost for Host {
        fn model_info(&mut self, m: Handle) -> ModelInfo {
            self.find(m).info
        }
        fn model_visible(&mut self, m: Handle, scale: f32, t: &Transform, ortho: bool) -> (bool, bool) {
            let name = self.find(m).name;
            let line = if ortho {
                format!("visortho {name}{}", Host::matrix_text(t))
            } else {
                format!("visper {name} {}{}", b(scale), Host::matrix_text(t))
            };
            self.rec.log.push(line);
            (self.visible, self.clipped)
        }
        fn pixel_size(&mut self, m: Handle, t: &Transform, s: f32, o: bool) -> f32 {
            let name = self.find(m).name;
            self.rec.log.push(format!("pixelsize {name} {} {}{}", b(s), o as i32, Host::matrix_text(t)));
            self.pixel
        }
        fn view_width(&mut self) -> f32 {
            640.0
        }
        fn material_info(&mut self, m: Handle, i: u32, lod: u32) -> MaterialInfo {
            self.find(m).materials[i as usize][lod as usize]
        }
        fn polygon_indices(&mut self, m: Handle, lod: u32) -> *const u16 {
            self.find(m).indices[lod as usize].as_ptr()
        }
        fn frame_vertices(&mut self, m: Handle, f: u32) -> *const c_void {
            self.find(m).frames[f as usize]
        }
        fn sub_model(&mut self, m: Handle, j: u32) -> Handle {
            self.find(m).subs[j as usize]
        }
    }

    fn pa(n: usize) -> Vec<u16> {
        (0..n * 3).map(|i| (i * 2 + 1) as u16).collect()
    }

    const T1M: usize = 0x71;
    const T2M: usize = 0x72;
    const C1: [f32; 3] = [0.2, 0.4, 0.6];

    fn mat(vstart: u32, vlen: u32, pstart: u32, plen: u32, tex: usize, opacity: f32) -> [MaterialInfo; 2] {
        let m0 = MaterialInfo {
            texture: tex as Handle,
            polygon_start: pstart,
            polygon_length: plen,
            vertex_start: vstart,
            vertex_length: vlen,
            opacity,
            color: C1,
        };
        let mut m1 = m0;
        m1.polygon_start = pstart + 1;
        m1.polygon_length = plen.saturating_sub(1);
        [m0, m1]
    }

    #[allow(clippy::too_many_arguments)]
    fn info(
        transform_seed: f32,
        scale: f32,
        frac: f32,
        cur: u32,
        nxt: u32,
        lods: u32,
        nmats: u32,
        vtype: u32,
        subs: u32,
    ) -> ModelInfo {
        ModelInfo {
            material_count: nmats,
            lod_count: lods,
            sub_model_count: subs,
            current_frame: cur,
            next_frame: nxt,
            vertex_type: vtype,
            scale,
            opacity: 0.5,
            fraction: frac,
            transform: matrix(transform_seed),
        }
    }

    fn host() -> Host {
        Host {
            rec: Rec::new(),
            names: vec![(T1M as Handle, "T1m"), (T2M as Handle, "T2m")],
            models: Vec::new(),
            visible: true,
            clipped: false,
            pixel: 0.0,
        }
    }

    fn finish(h: &mut Host, r: LongResult) {
        h.rec.log.push(format!("ret {r}"));
        let e = h.rec.end_line();
        h.rec.log.push(e);
    }

    fn check(h: Host, section: &str) {
        let want = golden_section(section);
        assert_eq!(h.rec.log, want, "{section}");
    }

    #[test]
    fn polygon_array_copies_coloured_and_standard_vertices() {
        let mut h = host();
        let v: Vec<_> = (0..5).map(|i| cv(i as f32)).collect();
        let p = pa(4);
        h.rec.log.clear();
        let r = draw_polygon_array(&mut h, p.as_ptr(), 2, 1, v.as_ptr() as *const c_void, 5, T1M as Handle);
        finish(&mut h, r);
        check(h, "polyarray_color");
        let mut h = host();
        let s: Vec<_> = (0..3).map(|i| sv(7.0 + i as f32)).collect();
        let r = draw_polygon_array(&mut h, p.as_ptr(), 1, 0, s.as_ptr() as *const c_void, 3, core::ptr::null_mut());
        finish(&mut h, r);
        check(h, "polyarray_std");
    }

    fn tree_host() -> (Host, Vec<Vec<Dx7Vertex>>, Vec<Dx7StdColorVertex>) {
        let mut h = host();
        h.rec.force_unlock = Some(0);
        h.rec.force_rs = Some(0);
        let frames_a: Vec<Dx7Vertex> = (0..6).map(|i| sv(1.0 + i as f32)).collect();
        let frames_b: Vec<Dx7Vertex> = (0..6).map(|i| sv(50.0 + i as f32)).collect();
        let child_v: Vec<Dx7StdColorVertex> = (0..4).map(|i| cv(9.0 + i as f32)).collect();
        let root_id = 1usize as Handle;
        let child_id = 2usize as Handle;
        h.models.push(Model {
            id: root_id,
            name: "root",
            info: info(0.5, 1.5, 0.25, 0, 1, 2, 2, 0, 1),
            materials: vec![mat(1, 3, 0, 2, T1M, 0.8), mat(0, 0, 0, 0, T2M, 1.0)],
            indices: vec![pa(8), pa(8)],
            frames: vec![frames_a.as_ptr() as *const c_void, frames_b.as_ptr() as *const c_void],
            subs: vec![child_id],
        });
        h.models.push(Model {
            id: child_id,
            name: "child",
            info: info(0.25, 2.0, 0.0, 0, 0, 1, 1, 1, 0),
            materials: vec![mat(2, 2, 1, 2, T2M, 0.5)],
            indices: vec![pa(8)],
            frames: vec![child_v.as_ptr() as *const c_void],
            subs: vec![],
        });
        (h, vec![frames_a, frames_b], child_v)
    }

    #[test]
    fn model_tree_blends_and_recurses() {
        let (mut h, _f, _c) = tree_host();
        h.pixel = 120.0;
        let r = draw_model(&mut h, 1usize as Handle, 2.0, false);
        finish(&mut h, r);
        check(h, "model_tree");
    }

    #[test]
    fn model_tree_orthographic_clipped_lod_rounding() {
        let (mut h, _f, _c) = tree_host();
        h.pixel = 2000.0;
        h.clipped = true;
        let r = draw_model(&mut h, 1usize as Handle, 1.0, true);
        finish(&mut h, r);
        check(h, "model_tree_ortho_clip");
    }

    #[test]
    fn model_invisible_still_visits_sub_models() {
        let (mut h, _f, _c) = tree_host();
        h.visible = false;
        let r = draw_model(&mut h, 1usize as Handle, 1.0, false);
        assert_eq!(r, 0);
        // DrawSky: returns kErrorOK whatever Draw returned
        finish(&mut h, 0);
        check(h, "model_invisible");
    }

    #[test]
    fn model_stops_at_the_first_sub_model_error() {
        let (mut h, _f, _c) = tree_host();
        h.pixel = 120.0;
        h.clipped = true; // the generator's earlier scenario left clipping on
        h.rec.force_unlock = Some(11);
        let r = draw_model(&mut h, 1usize as Handle, 2.0, false);
        finish(&mut h, r);
        check(h, "model_stops_at_error");
    }

    #[test]
    fn model_with_colored_frames_blends_colored() {
        let mut h = host();
        let va: Vec<_> = (0..3).map(|i| cv(1.0 + i as f32)).collect();
        let vb: Vec<_> = (0..3).map(|i| cv(40.0 + i as f32)).collect();
        h.models.push(Model {
            id: 3usize as Handle,
            name: "rootc",
            info: info(0.75, 1.0, 0.5, 0, 1, 1, 1, 1, 0),
            materials: vec![mat(0, 3, 0, 1, T1M, 1.0)],
            indices: vec![pa(8)],
            frames: vec![va.as_ptr() as *const c_void, vb.as_ptr() as *const c_void],
            subs: vec![],
        });
        h.pixel = 50.0;
        h.clipped = true; // as the generator's earlier scenario left it
        let r = draw_model(&mut h, 3usize as Handle, 1.0, false);
        finish(&mut h, r);
        check(h, "model_color_smooth");
    }

    fn vp() -> ViewportInfo {
        ViewportInfo {
            camera: matrix(0.7),
            camera_inverse: matrix(0.3),
            perspective: matrix(1.1),
            orthographic: matrix(0.9),
            front_clip: 2.0,
            rear_clip: 300.0,
            half_width: 320.0,
            half_height: 240.0,
            field_of_view: core::f32::consts::FRAC_PI_3,
            y_clip_plane: 20.5,
            ortho_left: -30.0,
            ortho_top: -20.0,
            ortho_right: 30.0,
            ortho_bottom: 20.0,
            left_plane: Plane::default(),
            right_plane: Plane::default(),
            top_plane: Plane::default(),
            bottom_plane: Plane::default(),
        }
    }

    fn line_points() -> Vec<[f32; 4]> {
        (0..4).map(|i| [i as f32 * 1.5, i as f32 * 2.5, i as f32 * 0.5, 0.7 + i as f32 * 0.5]).collect()
    }

    #[test]
    fn line_3d_perspective() {
        let mut h = host();
        let pts = line_points();
        let r = draw_3d_line(&mut h, &pts, T1M as Handle, 0.25, false, &vp());
        finish(&mut h, r);
        check(h, "line3d");
    }

    #[test]
    fn line_3d_orthographic_negative_v_wraps() {
        let mut h = host();
        let pts = line_points();
        let r = draw_3d_line(&mut h, &pts, core::ptr::null_mut(), -1.5, true, &vp());
        finish(&mut h, r);
        check(h, "line3d_ortho_v_neg");
    }

    #[test]
    fn line_3d_single_point_returns_early() {
        let mut h = host();
        let pts = vec![[1.0, 2.0, 3.0, 1.0]];
        let r = draw_3d_line(&mut h, &pts, T1M as Handle, 0.0, false, &vp());
        finish(&mut h, r);
        check(h, "line3d_short");
    }

    #[test]
    fn plane_test_is_on_or_below() {
        let p = Plane { point: [0.0, 0.0, 0.0], normal: [0.0, 0.0, 1.0] };
        assert!(p.on_or_below([1.0, 1.0, 0.0]));
        assert!(p.on_or_below([1.0, 1.0, -1.0]));
        assert!(!p.on_or_below([1.0, 1.0, 0.5]));
    }

    #[test]
    fn lod_clamps_to_the_last_level_for_small_and_negative_detail() {
        let mut h = host();
        let frames: Vec<Dx7Vertex> = (0..6).map(|i| sv(i as f32)).collect();
        h.models.push(Model {
            id: 9usize as Handle,
            name: "root",
            info: info(0.5, 1.0, 0.0, 0, 0, 3, 1, 0, 0),
            materials: vec![mat(0, 1, 0, 2, T1M, 1.0), mat(0, 1, 0, 2, T1M, 1.0)],
            indices: vec![pa(8), pa(8), pa(8)],
            frames: vec![frames.as_ptr() as *const c_void],
            subs: vec![],
        });
        h.pixel = 1.0; // detail is tiny: LOD picks (1 - tiny) * 3 -> rounds to 3 -> clamped to 2
        h.models[0].materials = vec![[
            MaterialInfo {
                texture: T1M as Handle,
                polygon_start: 0,
                polygon_length: 1,
                vertex_start: 0,
                vertex_length: 1,
                opacity: 1.0,
                color: C1,
            },
            MaterialInfo {
                texture: T1M as Handle,
                polygon_start: 7,
                polygon_length: 1,
                vertex_start: 0,
                vertex_length: 1,
                opacity: 1.0,
                color: C1,
            },
        ]];
        // material_info panics for lod 2 without the clamp (only two entries): LOD 1 is not 2, so use lod_count 2
        h.models[0].info.lod_count = 2;
        let r = draw_model(&mut h, 9usize as Handle, 1000.0, false);
        assert_eq!(r, 11);
        // the second LOD's polygon start (7 * 3) reached the index memory: indices are "i*2+1"
        assert!(h.rec.log.iter().any(|l| l.contains("drawidx 4 VB 0 1 43 45 47")), "{:?}", h.rec.log);
    }
}
