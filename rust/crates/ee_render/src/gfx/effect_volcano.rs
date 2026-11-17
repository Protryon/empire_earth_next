//! The volcano (kind 0x20) whole in Rust on `EEE3DRust` (`EEE3DVolcano`, 0xfe553..0x101c34, deleted): a mesh over the
//! 6 x 6 area around its point that rises from the terrain into a cone over the effect's time 1 (jittered near the
//! centre every time 2 while it rises), holds for the duration and sinks again. Its mesh's terrain points and cone
//! heights are a Rust list the record owns (`volcano.mesh`, freed with the record); the C++ host's `volcano_build`
//! keeps `BuildModel` (the image's surface, the model's creation and material, `volcano.rs`'s mesh stored, the
//! placement, the radius and the texture). The queue's morph reads and writes the model's vertices through the
//! raster table (`vertex_count`, `vertex_point`, `set_vertex_point`, `model_bounds`, `model_normals`); its jitter
//! draws from one generator all volcanoes share (the original's function-static `URandom`, seed 1).
//!
//! Step, Load and Save are simulation-side state (the growth and timers are saved): RUST_PLAN 2.1, the C++'s f32
//! operations in its order.

use std::sync::Mutex;

use crate::ge::types::Ge3DPoint;
use ee_base::random::{float_to_long, URandom};

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_state::{load_point, save_point, EeGfxEffectRecord, EffectFile};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE};
use crate::gfx::volcano::{sizes, EeGfxVolcanoArea};

/// `kEffect3DVolcano`
pub const EE_GFX_EFFECT_KIND_VOLCANO: i32 = 0x20;

const OK: i32 = 0;

/// The mesh's terrain points (`mPoints`) and cone heights (`mHeights`).
#[derive(Debug, Default)]
pub struct EeGfxVolcanoMesh {
    pub points: Vec<Ge3DPoint>,
    pub heights: Vec<f32>,
}

/// The volcano's state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectVolcano {
    /// `mArea` and `mCenter` (saved)
    pub area: EeGfxVolcanoArea,
    /// `mGrowth`: 0 on the terrain .. 1 the cone (saved)
    pub growth: f32,
    /// `mTimeLeft` (saved)
    pub time_left: i32,
    /// `mTime`: ms since the last jitter (saved)
    pub time: i32,
    /// `mGrown` (saved)
    pub grown: bool,
    /// `mDone`: the mesh is at its growth
    pub done: bool,
    /// the mesh (made on first use; owned by the record)
    pub mesh: *mut EeGfxVolcanoMesh,
}

impl Default for EeGfxEffectVolcano {
    fn default() -> EeGfxEffectVolcano {
        EeGfxEffectVolcano {
            area: EeGfxVolcanoArea::default(),
            growth: 0.0,
            time_left: 0,
            time: 0,
            grown: false,
            done: false,
            mesh: std::ptr::null_mut(),
        }
    }
}

/// The record's mesh (made on first use).
///
/// # Safety
/// `r` is a live record; no other reference to its mesh lives.
unsafe fn mesh<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxVolcanoMesh {
    // SAFETY: the function's contract; the mesh is the record's own Box
    unsafe {
        if (*r).volcano.mesh.is_null() {
            (*r).volcano.mesh = Box::into_raw(Box::default());
        }
        &mut *(*r).volcano.mesh
    }
}

/// The original's `static URandom sRandom` in `Queue` (seed 0, so 1).
static JITTER: Mutex<URandom> = Mutex::new(URandom { seed: 1 });

/// `BuildModel` (0xfe79d): the mesh's lists sized for the area (as `resize`, keeping what they held), then the host's.
///
/// # Safety
/// As `effect_rising::effect3d`.
unsafe fn build<H: Effect3DHost>(r: *mut EeGfxEffectRecord, h: &mut H) -> i32 {
    // SAFETY (every access below): the function's contract; the lists are not touched by the host but through the
    // pointers it is handed for the call
    let n = sizes(unsafe { (*r).volcano.area.right - (*r).volcano.area.left }).vertices.max(0) as usize;
    let m = unsafe { mesh(r) };
    m.points.resize(n, Ge3DPoint::default());
    m.heights.resize(n, 0.0);
    let (points, heights) = (m.points.as_mut_ptr(), m.heights.as_mut_ptr());
    h.volcano_build(points, heights, n as u32)
}

/// The volcano's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn volcano<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0xfe596: the 6 x 6 area around the point, its centre one tile up in y
        EeGfxEffect3DOp::CreateAt => {
            unsafe {
                let p = (*r).aged.location;
                let v = &mut (*r).volcano;
                v.area.left = p.x - 3.0;
                v.area.top = p.y + 3.0;
                v.area.right = p.x + 3.0;
                v.area.bottom = p.y - 3.0;
                v.area.center.x = (v.area.right - v.area.left) * 0.5 + v.area.left;
                v.area.center.y = (v.area.bottom - v.area.top) * 0.5 + v.area.top + 1.0;
                v.growth = 0.0;
                v.grown = false;
                v.done = false;
                v.time = 0;
                v.time_left = 0;
            }
            0
        }
        // 0x500a25: the media, then the model
        EeGfxEffect3DOp::SetUp => {
            let result = h.load_media();
            if result == OK {
                unsafe { build(r, h) }
            } else {
                result
            }
        }
        // 0x500be2: after the base's pointers (`EEE3DRust::RepairPointers`), the model again
        EeGfxEffect3DOp::Repair => unsafe { build(r, h) },
        // 0x50068a: the mesh morphed to the growth (jittered every time 2 while rising), render queue 2
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == OK {
                if !unsafe { (*r).volcano.done } {
                    let model = h.volcano_model();
                    let m = unsafe { mesh(r) };
                    let v = unsafe { &mut (*r).volcano };
                    let growth = v.growth;
                    if v.time > g.time2 {
                        v.time = 0;
                        let mut point = Ge3DPoint { x: v.area.center.x, y: v.area.top - 0.2, z: v.area.center.z };
                        let amplitude = g.start_size;
                        let scale = 1.0 / v.area.center.distance_to_squared(point);
                        let mut random = JITTER.lock().unwrap_or_else(|e| e.into_inner());
                        let mut i = 0;
                        while i < x.vertex_count(model) {
                            point = x.vertex_point(model, i);
                            let p = m.points[i as usize];
                            v.area.center.z = p.z;
                            let mut factor = 1.0 - v.area.center.distance_to_squared(p) * scale;
                            // the original's tests (a NaN factor stays NaN; `clamp` would too, but keep its form)
                            #[allow(clippy::manual_clamp)]
                            if factor >= 1.0 {
                                factor = 1.0;
                            } else if factor <= 0.0 {
                                factor = 0.0;
                            }
                            point.x = (random.generate() * 2.0 - 1.0) * (1.0 - growth) * factor * amplitude + p.x;
                            point.y = (random.generate() * 2.0 - 1.0) * (1.0 - growth) * factor * amplitude + p.y;
                            point.z = (1.0 - growth) * p.z + m.heights[i as usize] * growth;
                            x.set_vertex_point(model, i, &point);
                            i += 1;
                        }
                    } else {
                        let mut i = 0;
                        while i < x.vertex_count(model) {
                            let mut point = x.vertex_point(model, i);
                            point.z = (1.0 - growth) * m.points[i as usize].z + growth * m.heights[i as usize];
                            x.set_vertex_point(model, i, &point);
                            i += 1;
                        }
                    }
                    x.model_bounds(model);
                    x.model_normals(model);
                    if growth == 1.0 {
                        v.done = true;
                    }
                }
                h.queue(2);
            }
            result
        }
        // 0x500a47: vertex colours, z written, at depth range 0..1 (left there for the draws that follow)
        EeGfxEffect3DOp::DrawPass4 => {
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            x.depth_range(0.0, 1.0);
            x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(1);
            x.lighting(EeGfxLighting::Terrain);
            let model = h.volcano_model();
            let result = x.model_draw(model, 1.0);
            x.depth_range(0.0, 1.0);
            x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
            result
        }
        // 0x500c7e: rise over time 1, hold for the duration, sink over time 1; 7 when the time is out
        EeGfxEffect3DOp::Step => {
            let v = unsafe { &mut (*r).volcano };
            let mut elapsed = float_to_long(g.tick_time);
            if elapsed <= 0 {
                elapsed = 1;
            }
            let mut result = OK;
            if !v.grown {
                v.growth += g.tick_time / g.time1 as f32;
                v.time = v.time.wrapping_add(elapsed);
                if v.growth >= 1.0 {
                    v.growth = 1.0;
                    v.time_left = g.duration.wrapping_add(g.time1) as i32;
                    v.grown = true;
                }
            } else {
                v.time_left = v.time_left.wrapping_sub(elapsed);
                if v.time_left < g.time1 as i32 {
                    v.done = false;
                    v.growth = v.time_left as f32 / g.time1 as f32;
                }
                if v.time_left < 0 {
                    result = 7;
                }
            }
            result
        }
        _ => 0,
    }
}

/// `GERectangle::Load` of the area's edges.
pub(crate) fn load_area<F: EffectFile>(f: &mut F, a: &mut EeGfxVolcanoArea) -> i32 {
    if !f.is_open() {
        return 4; // kErrorNotOpen
    }
    let mut res = f.read_f32(&mut a.left);
    if res == OK {
        res = f.read_f32(&mut a.top);
        if res == OK {
            res = f.read_f32(&mut a.right);
            if res == OK {
                res = f.read_f32(&mut a.bottom);
            }
        }
    }
    res
}

/// `GERectangle::Save` of the area's edges.
pub(crate) fn save_area<F: EffectFile>(f: &mut F, a: &EeGfxVolcanoArea) -> i32 {
    if !f.is_open() {
        return 4; // kErrorNotOpen
    }
    let mut res = f.write_f32(a.left);
    if res == OK {
        res = f.write_f32(a.top);
        if res == OK {
            res = f.write_f32(a.right);
            if res == OK {
                res = f.write_f32(a.bottom);
            }
        }
    }
    res
}

/// `Load` (0x500b46) and `Save` (0x500bf7) after `EEEffect3D`'s, whose result both ignore: the area, centre, growth,
/// time left, rise timer and grown flag; the first error. The load loads the media (its result ignored) and marks
/// the mesh for reshaping.
pub fn volcano_file<H: Effect3DHost, F: EffectFile>(
    load: bool,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    h: &mut H,
) -> i32 {
    let v = &mut r.volcano;
    if load {
        h.load_media();
        v.done = false;
        let mut res = load_area(f, &mut v.area);
        if res != OK {
            return res;
        }
        res = load_point(f, &mut v.area.center);
        if res != OK {
            return res;
        }
        res = f.read_f32(&mut v.growth);
        if res != OK {
            return res;
        }
        res = f.read_i32(&mut v.time_left);
        if res != OK {
            return res;
        }
        res = f.read_i32(&mut v.time);
        if res == OK {
            res = f.read_bool(&mut v.grown);
        }
        res
    } else {
        let mut res = save_area(f, &v.area);
        if res != OK {
            return res;
        }
        res = save_point(f, &v.area.center);
        if res != OK {
            return res;
        }
        res = f.write_f32(v.growth);
        if res != OK {
            return res;
        }
        res = f.write_i32(v.time_left);
        if res != OK {
            return res;
        }
        res = f.write_i32(v.time);
        if res == OK {
            res = f.write_bool(v.grown);
        }
        res
    }
}

#[cfg(test)]
#[path = "effect_volcano_tests.rs"]
mod tests;
