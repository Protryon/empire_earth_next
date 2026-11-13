//! The citizen beam (kind 0x33) whole in Rust (`EEE3DCitizenBeam`, deleted): a model from the unit's first tag point
//! towards a point (the constructor's, raised by param 1), shown while the unit plays the effect's animation between
//! its frames (int params 1-3) unless it is a cloaked enemy's, flickering its scale with the game time. Aimed when it
//! turns on, placing the linked effect half way. On `EEE3DRust` like `effect_rising.rs`; its state is the shot
//! part's (`effect_shot.rs`: the line, the draw location, the scale, and the beam's end, middle and flags).

use ee_ge::types::Ge3DPoint;
use ee_ge::um::{um_arccos, um_arctan2};

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_shot::{load_line, save_line};
use crate::effect_state::{identity, load_point, save_point, EeGfxEffectRecord, EffectFile};
use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;
use crate::raster::{Raster, BLEND_ONE, ZBUFFER_TEST_ONLY};

/// The citizen beam.
pub const EE_GFX_EFFECT_KIND_CITIZEN_BEAM: i32 = 0x33;

/// `kErrorOK`.
const OK: i32 = 0;

/// The beam's unit as its step reads it (`EEE3DCitizenBeam::Step`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGfxBeamUnit {
    /// the unit is a complex unit
    pub complex: bool,
    /// `mAnimation`, `GetFrame()`
    pub animation: i32,
    pub frame: i32,
}

/// The beam's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn beam<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept
    match op {
        EeGfxEffect3DOp::Create => {
            // the C++ put the target in the end
            unsafe {
                let s = &mut (*r).shot;
                s.end.z = g.param1 + s.end.z;
                s.active = false;
            }
            0
        }
        EeGfxEffect3DOp::SetUp => {
            let result = h.load_media();
            unsafe { (*r).bounding_radius = g.size };
            result
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    let t = (*r).transformation;
                    let s = &mut (*r).shot;
                    s.draw_active = s.active;
                    s.location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                }
                h.queue(4);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass6 => {
            let (mut t, s) = unsafe { ((*r).transformation, (*r).shot) };
            let model = h.model();
            if !s.draw_active {
                return 0;
            }
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let fog = x.get_fog();
            x.set_zbuffer(ZBUFFER_TEST_ONLY);
            x.set_shading_source(0);
            x.set_blending(BLEND_ONE, BLEND_ONE);
            x.lighting(EeGfxLighting::White);
            x.model_own_texture(model, h.texture());
            let location = s.location;
            let value = x.fog_factor(&location, fog);
            t.m[0][3] = location.x;
            t.m[1][3] = location.y;
            t.m[2][3] = location.z;
            x.model_transform(model, &t);
            x.model_scale(model, s.scale);
            x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
            x.model_prepare(model);
            let result = x.model_draw(model, 1.0);
            x.set_fog(fog);
            result
        }
        EeGfxEffect3DOp::Step => {
            let mut active = false;
            let u = h.beam_unit();
            if u.complex && u.animation == g.int_param1 && u.frame >= g.int_param2 && u.frame < g.int_param3 {
                active = true;
                if h.beam_hidden() {
                    active = false;
                } else if !unsafe { (*r).shot.active } {
                    unsafe { aim(r, h) };
                    let middle = unsafe { (*r).shot.middle };
                    h.place_linked(&middle);
                }
            }
            unsafe {
                let s = &mut (*r).shot;
                s.active = active;
                if active {
                    s.scale = (1.0 - (g.game_time_bits & 7) as f32 * 0.03) * g.size;
                }
            }
            0
        }
        _ => 0,
    }
}

/// `Aim`: the line from the unit's first tag point to the end, the transformation a quarter unit out along it
/// facing along it, the middle half a unit out.
///
/// # Safety
/// As `beam`.
unsafe fn aim<H: Effect3DHost>(r: *mut EeGfxEffectRecord, h: &mut H) {
    let start = h.beam_start();
    // SAFETY: the function's contract
    let end = unsafe { (*r).shot.end };
    let mut d = ee_ge::types::GeVector { x: end.x - start.x, y: end.y - start.y, z: end.z - start.z };
    unsafe {
        (*r).shot.path.start = start;
        (*r).shot.path.direction = d;
    }
    d.normalize();
    let yaw = um_arctan2(-d.x, d.y);
    let pitch = um_arccos(-d.z) - 1.5707964;
    let mut t = identity();
    t.m[0][3] = d.x * 0.25 + start.x;
    t.m[1][3] = d.y * 0.25 + start.y;
    t.m[2][3] = d.z * 0.25 + start.z;
    t.set_orientation(ee_ge::transform::RotationOrder::Ypr, yaw, pitch, 0.0);
    unsafe { (*r).transformation = t };
    h.move_to();
    unsafe {
        (*r).shot.middle = Ge3DPoint { x: d.x * 0.5 + start.x, y: d.y * 0.5 + start.y, z: d.z * 0.5 + start.z };
    }
}

/// The beam's own Load after the base's (ignored) and the media load (whose result stops it): the unused float,
/// the end, the line, the middle and the active flag; the first error.
pub fn load_beam<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F, media: i32) -> i32 {
    let s = &mut r.shot;
    let mut res = media;
    if res == OK {
        res = f.read_f32(&mut s.unused);
    }
    if res == OK {
        res = load_point(f, &mut s.end);
    }
    if res == OK {
        res = load_line(f, &mut s.path);
    }
    if res == OK {
        res = load_point(f, &mut s.middle);
    }
    if res == OK {
        res = f.read_bool(&mut s.active);
    }
    res
}

/// The beam's own Save after the base's (whose result stops it).
pub fn save_beam<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F, base: i32) -> i32 {
    let s = &r.shot;
    let mut res = base;
    if res == OK {
        res = f.write_f32(s.unused);
    }
    if res == OK {
        res = save_point(f, &s.end);
    }
    if res == OK {
        res = save_line(f, &s.path);
    }
    if res == OK {
        res = save_point(f, &s.middle);
    }
    if res == OK {
        res = f.write_bool(s.active);
    }
    res
}

#[cfg(test)]
#[path = "effect_beam_tests.rs"]
mod tests;
