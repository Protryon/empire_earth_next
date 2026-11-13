//! The additive pass (kind 0x27) whole in Rust (`EEE3DAdditivePass`, deleted): its unit's own model drawn again
//! over it in pass 5, additively (one-one blending, z test only), with the effect's texture on the model's first
//! material and lit white by the fog factor at its place. It follows its unit's location and scale every step and
//! ends (7) when the unit has no hit points. Its Save is the base's alone; its Load the base's, then the media's.
//! `EEE3DRust` holds the graphic object and the unit's model (`unit_model_media`, unloaded by its destructor) and
//! answers the world (`EeGfxEffect3DHost`); the draw goes through the rasterizer table (`raster.rs`).
//!
//! The record is reached through its raw pointer field by field: the C++ reads it during the host calls (the region
//! move), so no Rust reference to it lives across one.

use ee_ge::types::{Ge3DPoint, GeTransformation};

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{identity, EeGfxEffectRecord};
use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;
use crate::raster::{Raster, BLEND_ONE, ZBUFFER_TEST_ONLY};

/// `kEffect3DAdditivePass`.
pub const EE_GFX_EFFECT_KIND_ADDITIVE_PASS: i32 = 0x27;

/// The unit's physical model's frame and its complex unit's animation (`mAnimation`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGfxUnitFrame {
    pub frame: u32,
    pub animation: u32,
}

/// The additive pass's state (none of it saved).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectPass {
    /// `mDrawTransformation`: `Queue` copies the effect's transformation
    pub draw_transformation: GeTransformation,
    /// `mFrame`, `mModelIndex`: the unit's frame and animation (`Queue` takes them; the draw's model is the unit
    /// model's model of that animation)
    pub frame: u32,
    pub model_index: u32,
    /// `mScale`: the unit's stats' scale, and the draw's copy
    pub scale: f32,
    pub draw_scale: f32,
}

impl Default for EeGfxEffectPass {
    fn default() -> EeGfxEffectPass {
        EeGfxEffectPass { draw_transformation: identity(), frame: 0, model_index: 0, scale: 0.0, draw_scale: 0.0 }
    }
}

/// Follows the unit: its stats' scale, its location (`SetTransformation`) and the radius of the unit's model at it.
///
/// # Safety
/// As `additive`.
unsafe fn follow_unit<H: Effect3DHost>(r: *mut EeGfxEffectRecord, h: &mut H) {
    let scale = h.unit_scale();
    let location = h.unit_location();
    // SAFETY (every access below): the function's contract
    unsafe {
        (*r).pass.scale = scale;
        (*r).transformation = location;
    }
    h.move_to();
    let radius = h.unit_model_radius();
    unsafe { (*r).bounding_radius = radius * (*r).pass.scale };
}

/// The additive pass's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn additive<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept
    match op {
        // the graphic's media (its result ignored), then the unit's model's
        EeGfxEffect3DOp::SetUp => {
            h.load_media();
            let result = h.unit_model_media();
            unsafe { follow_unit(r, h) };
            result
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                let frame = h.unit_frame();
                unsafe {
                    (*r).pass.draw_transformation = (*r).transformation;
                    (*r).pass.frame = frame.frame;
                    (*r).pass.model_index = frame.animation;
                    (*r).pass.draw_scale = (*r).pass.scale;
                }
                h.queue(3);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass5 => unsafe { draw(r, g, h, x) },
        // after a load: the unit's model and scale
        EeGfxEffect3DOp::Repair => {
            let result = h.unit_model_media();
            let scale = h.unit_scale();
            unsafe { (*r).pass.scale = scale };
            result
        }
        EeGfxEffect3DOp::Step => {
            if g.unit_hit_points == 0 {
                7
            } else {
                unsafe { follow_unit(r, h) };
                0
            }
        }
        _ => 0,
    }
}

/// Pass 5: the unit's model of its animation, additively.
///
/// # Safety
/// As `additive`.
unsafe fn draw<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    let p = unsafe { (*r).pass };
    let model = h.unit_model_at(p.model_index);
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    let fog = x.get_fog();
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_blending(BLEND_ONE, BLEND_ONE);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::White);
    let t = p.draw_transformation;
    let position = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
    let light = x.fog_factor(&position, fog);
    x.set_ambient(EeGfxColor { red: light, green: light, blue: light });
    let texture = h.texture();
    x.model_texture(model, texture);
    x.model_frame(model, p.frame);
    x.model_scale(model, p.draw_scale);
    x.model_transform(model, &t);
    x.model_prepare(model);
    let result = x.model_draw(model, 1.0);
    x.set_fog(fog);
    result
}

#[cfg(test)]
#[path = "effect_additive_tests.rs"]
mod tests;
