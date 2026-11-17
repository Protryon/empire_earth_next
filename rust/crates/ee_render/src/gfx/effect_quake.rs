//! The earthquake's crack (kind 0x1a) whole in Rust on `EEE3DRust` (`EEE3DEarthquake`, 0xec6e4..0xed606, deleted): a
//! strip model from one ground point towards another (`quake.rs`'s triangles and texture rows), its front growing at
//! the speed that covers the length in the effect's duration, held 15 s, then closing. The model is made, filled and
//! reshaped through the raster table (`create_model`, `store_mesh`, `set_vertex_point`); the record keeps its handle
//! (`quake.model`, released by the C++ with the effect). The constructor's two points arrive in the record's
//! `shot.path.start` and `shot.location` (`EEE3DRust`'s two-point constructor).
//!
//! Step, Load and Save are simulation-side (the growth and the timers are saved); `Queue` stops a growing crack that
//! reached space (its hold time), as the original did. RUST_PLAN 2.1: the C++'s f32 operations in its order.

use crate::ge::transform::RotationOrder;
use crate::ge::types::{Ge3DPoint, GeTransformation, GeVector};
use crate::ge::um;

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_state::{EeGfxEffectRecord, EffectFile};
use crate::gfx::effects::EeGfxTexCoord;
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::mesh::{EeGfxMeshTriangle, EeGfxMeshVertex, EE_GFX_MESH_COORD, EE_GFX_MESH_POINT};
use crate::gfx::quake::{build_rows, hold_time, queue_rows, segment_polygons, EeGfxQuakeRow, EE_GFX_QUAKE_ROW_U};
use crate::gfx::raster::{EeGfxModel, Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_ONLY};

/// `kEffect3DEarthquake`
pub const EE_GFX_EFFECT_KIND_EARTHQUAKE: i32 = 0x1a;

const OK: i32 = 0;
/// `kHeightEpsilon` (EECPAttackSubGroup.cpp): the reshaped rows' lift over the ground.
const HEIGHT_EPSILON: f32 = 0.01;

/// The crack's state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectQuake {
    /// `mModel` ("earthquake model"), referenced
    pub model: EeGfxModel,
    /// `mDistance`: the front's distance from the start (saved)
    pub distance: f32,
    /// `mSpeed`: the length over the duration
    pub speed: f32,
    /// `mLength` (saved)
    pub length: f32,
    /// `mSegmentCount`: the length rounded up (saved)
    pub segment_count: u32,
    /// `mHoldTime` (saved; 15000 once fully grown)
    pub hold_time: f32,
    /// `mGrowing`, `mClosing` (saved)
    pub growing: bool,
    pub closing: bool,
}

impl Default for EeGfxEffectQuake {
    fn default() -> EeGfxEffectQuake {
        EeGfxEffectQuake {
            model: std::ptr::null_mut(),
            distance: 0.0,
            speed: 0.0,
            length: 0.0,
            segment_count: 0,
            hold_time: 0.0,
            growing: false,
            closing: false,
        }
    }
}

/// `UMCeil`: round to nearest, then up when below.
fn ceil(value: f32) -> i32 {
    let r = um::lrintf(value);
    if (r as f32) < value {
        r.wrapping_add(1)
    } else {
        r
    }
}

/// `BuildModel` (0x4ec8bc): five ground-hugging vertices across per row, eight triangles a segment, the first row on
/// the ground and the others at the origin until the queue moves them; textured with the graphic's first texture.
fn build<H: Effect3DHost, R: Raster>(r: *mut EeGfxEffectRecord, h: &mut H, x: &mut R) -> i32 {
    // SAFETY: the caller's contract; no reference to the record is kept across a host call
    let (t, segments) = unsafe { ((*r).transformation, (*r).quake.segment_count) };
    let vertices = segments.wrapping_add(1).wrapping_mul(5);
    let polygons = segments.wrapping_mul(8);
    let model = x.create_model(polygons, vertices, 0, "earthquake model", 1);
    // SAFETY: as above
    unsafe { (*r).quake.model = model };
    x.model_reference(model);
    x.material_ranges(model, polygons, vertices);
    let mut v = vec![EeGfxMeshVertex::default(); vertices as usize];
    let mut tr = vec![EeGfxMeshTriangle::default(); polygons as usize];
    let mut point = Ge3DPoint::default();
    for (i, vertex) in v.iter_mut().take(5).enumerate() {
        point.x = i as f32 - 2.0;
        let ground = h.ground_point(&t.apply_point(point));
        point.z = ground.z;
        vertex.set = EE_GFX_MESH_POINT | EE_GFX_MESH_COORD;
        vertex.point = point;
        vertex.coord = EeGfxTexCoord { u: EE_GFX_QUAKE_ROW_U[i], v: 0.0 };
    }
    for vertex in v.iter_mut().skip(5) {
        vertex.set |= EE_GFX_MESH_POINT;
        vertex.point = Ge3DPoint::default();
    }
    let mut rows = vec![0.0f32; segments as usize];
    build_rows(&mut rows);
    for s in 0..segments {
        let p = segment_polygons(s);
        for k in 0..8 {
            tr[(s * 8 + k) as usize] =
                EeGfxMeshTriangle { set: 1, v: [p[k as usize * 3], p[k as usize * 3 + 1], p[k as usize * 3 + 2]] };
        }
        for (i, u) in EE_GFX_QUAKE_ROW_U.iter().enumerate() {
            let vertex = &mut v[(s * 5 + 5) as usize + i];
            vertex.set |= EE_GFX_MESH_COORD;
            vertex.coord = EeGfxTexCoord { u: *u, v: rows[s as usize] };
        }
    }
    x.store_mesh(model, &v, &tr);
    x.model_normals(model);
    x.model_bounds(model);
    x.model_transform(model, &t);
    x.model_prepare(model);
    let texture = h.texture_at(0);
    x.model_texture(model, texture);
    OK
}

/// The crack's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn quake<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0x4ec727: from the start towards the end, one segment per unit of length, at the duration's speed
        EeGfxEffect3DOp::Create => {
            let (from, to) = unsafe { ((*r).shot.path.start, (*r).shot.location) };
            let mut direction = GeVector { x: to.x - from.x, y: to.y - from.y, z: 0.0 };
            let q = unsafe { &mut (*r).quake };
            q.distance = 0.0;
            q.length = direction.magnitude();
            q.speed = q.length / g.duration as f32;
            q.segment_count = ceil(q.length) as f32 as u32;
            q.closing = false;
            q.growing = true;
            direction.normalize();
            let yaw = um::um_arctan2(-direction.x, direction.y);
            let mut t = GeTransformation::IDENTITY;
            t.m[0][3] = from.x;
            t.m[1][3] = from.y;
            t.m[2][3] = 0.0;
            t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
            t.m[3] = [0.0, 0.0, 0.0, 1.0];
            let length = q.length;
            unsafe {
                (*r).transformation = t;
            }
            h.move_to();
            unsafe { (*r).bounding_radius = length };
            OK
        }
        // 0x4ed365: the graphic, then the model
        EeGfxEffect3DOp::SetUp => {
            let result = h.load_media();
            if result == OK {
                build(r, h, x)
            } else {
                result
            }
        }
        // 0x4ed4d4: after the base's pointers, the speed (the original computed it in Load) and the model
        EeGfxEffect3DOp::Repair => {
            let q = unsafe { &mut (*r).quake };
            q.speed = q.length / g.duration as f32;
            build(r, h, x)
        }
        // 0x4ecf53: the rows reshaped over the ground up to the front; a growing crack over space stops and holds
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result != OK {
                return result;
            }
            let (t, q) = unsafe { ((*r).transformation, (*r).quake) };
            let mut rows = vec![EeGfxQuakeRow::default(); q.segment_count as usize];
            queue_rows(q.distance, &mut rows);
            let mut coords = vec![EeGfxMeshVertex::default(); q.segment_count.wrapping_add(1).wrapping_mul(5) as usize];
            let mut over_space = false;
            let mut point = Ge3DPoint::default();
            for (s, row) in rows.iter().enumerate() {
                let vertex = s as u32 * 5 + 5;
                point.y = row.y;
                for i in 0..5u32 {
                    point.z = 0.0;
                    point.x = i as f32 - 2.0;
                    let ground = h.ground_point(&t.apply_point(point));
                    point.z = ground.z + HEIGHT_EPSILON;
                    x.set_vertex_point(q.model, vertex + i, &point);
                    if i == 2 && h.space_tile(um::floor_unsigned(ground.x), um::floor_unsigned(ground.y)) {
                        over_space = true;
                    }
                }
                for (i, u) in EE_GFX_QUAKE_ROW_U.iter().enumerate() {
                    let c = &mut coords[vertex as usize + i];
                    c.set = EE_GFX_MESH_COORD;
                    c.coord = EeGfxTexCoord { u: *u, v: row.front_v };
                }
            }
            x.store_mesh(q.model, &coords, &[]);
            if q.growing && over_space {
                let s = unsafe { &mut (*r).quake };
                s.hold_time = hold_time(q.length, q.distance, q.speed);
                s.growing = false;
            }
            x.model_normals(q.model);
            x.model_bounds(q.model);
            h.queue(0);
            OK
        }
        // 0x4ed384: alpha blended, z tested only, under the terrain's light
        EeGfxEffect3DOp::DrawPass2 => {
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::Terrain);
            x.model_draw(unsafe { (*r).quake.model }, 1.0)
        }
        // 0x4ed56c: grow, hold 15 s, close; 7 once closed
        EeGfxEffect3DOp::Step => {
            let q = unsafe { &mut (*r).quake };
            if q.growing {
                q.distance += g.tick_time * q.speed;
                if q.distance > q.length {
                    q.growing = false;
                    q.hold_time = 15000.0;
                }
                OK
            } else if q.closing {
                q.distance -= g.tick_time * q.speed;
                if q.distance < 0.0 {
                    7
                } else {
                    OK
                }
            } else {
                q.hold_time -= g.tick_time;
                if q.hold_time < 0.0 {
                    q.closing = true;
                }
                OK
            }
        }
        _ => 0,
    }
}

/// `Load` (0x4ed424: the graphic reloaded, the crack's state; the speed waits for `Repair`) or `Save` (0x4ed4e9)
/// after the base's, whose result both ignore.
pub fn quake_file<H: Effect3DHost, F: EffectFile>(load: bool, r: &mut EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    let q = &mut r.quake;
    if load {
        h.load_media();
        let mut res = f.read_f32(&mut q.distance);
        if res == OK {
            res = f.read_f32(&mut q.length);
        }
        if res == OK {
            res = f.read_u32(&mut q.segment_count);
        }
        if res == OK {
            res = f.read_f32(&mut q.hold_time);
        }
        if res == OK {
            res = f.read_bool(&mut q.growing);
        }
        if res == OK {
            res = f.read_bool(&mut q.closing);
        }
        res
    } else {
        let mut res = f.write_f32(q.distance);
        if res == OK {
            res = f.write_f32(q.length);
        }
        if res == OK {
            res = f.write_u32(q.segment_count);
        }
        if res == OK {
            res = f.write_f32(q.hold_time);
        }
        if res == OK {
            res = f.write_bool(q.growing);
        }
        if res == OK {
            res = f.write_bool(q.closing);
        }
        res
    }
}

#[cfg(test)]
#[path = "effect_quake_tests.rs"]
mod tests;
