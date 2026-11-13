//! The water splash (kind 0x1b) and the screen flash (kind 0x46) whole in Rust (`EEE3DWaterSplash`,
//! `EEE3DScreenFlash`, deleted): two effects on the aged record (`aged.age` 0..1 over the duration, `aged_step`;
//! `load_aged`/`save_aged`). The splash is placed at a point and drawn in pass 6 growing from its start size, eased,
//! flattening with age and fading over its last tenth; the flash sits at its unit's position (from `SetUp`) and is
//! drawn in pass 6 as the standing plane facing the camera, additively in front of everything (the viewport's depth
//! range 0..0), dimmed by the fog and the square of its age. `EEE3DRust` holds the graphic object and answers the
//! world (`EeGfxEffect3DHost`); the draws go through the rasterizer table (`raster.rs`).
//!
//! The record is reached through its raw pointer field by field: the C++ reads it during the host calls (the region
//! move), so no Rust reference to it lives across one.

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{aged_step, identity, EeGfxEffectRecord, EE_GFX_EFFECT_KIND_SCREEN_FLASH};
use crate::effects::EeGfxColor;
use crate::lighting::EeGfxLighting;
use crate::raster::{
    Raster, BLEND_INV_SRC_ALPHA, BLEND_ONE, BLEND_SRC_ALPHA, COMPARE_LESS, COMPARE_LESS_EQUAL, ZBUFFER_TEST_AND_WRITE,
    ZBUFFER_TEST_ONLY,
};

/// The splash's graphics scale when its eased size is zero.
const SPLASH_MIN_SCALE: f32 = 0.00001;

/// The water splash's and the screen flash's virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub(crate) unsafe fn aged<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept
    let flash = unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_SCREEN_FLASH;
    match op {
        // the flash on its unit: its age starts at zero
        EeGfxEffect3DOp::Create => {
            unsafe { (*r).aged.age = 0.0 };
            0
        }
        // the splash at a point (in `aged.location`)
        EeGfxEffect3DOp::CreateAt => {
            let p = unsafe { (*r).aged.location };
            let mut t = identity();
            t.m[0][3] = p.x;
            t.m[1][3] = p.y;
            t.m[2][3] = p.z;
            unsafe { (*r).transformation = t };
            h.move_to();
            0
        }
        EeGfxEffect3DOp::SetUp => {
            if flash {
                let p = h.complex_position();
                let mut t = identity();
                t.m[0][3] = p.x;
                t.m[1][3] = p.y;
                t.m[2][3] = p.z;
                unsafe { (*r).transformation = t };
                h.move_to();
                unsafe { (*r).bounding_radius = g.size };
            }
            h.load_media()
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe { (*r).aged.draw_age = (*r).aged.age };
                h.queue(4);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass6 if flash => unsafe { draw_flash(r, g, h, x) },
        EeGfxEffect3DOp::DrawPass6 => unsafe { draw_splash(r, g, h, x) },
        EeGfxEffect3DOp::Step => {
            let mut local = unsafe { r.read() };
            let mut tile = [0i32; 2];
            let result = aged_step(&mut local, g.tick_seconds, g.duration, &mut tile);
            unsafe { r.write(local) };
            result
        }
        _ => 0,
    }
}

/// The splash's pass 6.
///
/// # Safety
/// As `aged`.
unsafe fn draw_splash<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    let (t, draw_age) = unsafe { ((*r).transformation, (*r).aged.draw_age) };
    let flatness = 1.0 - draw_age * draw_age;
    let model = h.model();
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
    x.set_shading_source(0);
    x.lighting(EeGfxLighting::Unit);
    let texture = h.texture();
    x.model_texture(model, texture);
    let left = 1.0 - draw_age;
    let eased = 1.0 - left * left;
    unsafe { (*r).aged.draw_age = eased };
    let scale = (g.size - g.start_size) * eased + g.start_size;
    x.model_scale(model, if scale != 0.0 { scale } else { SPLASH_MIN_SCALE });
    x.model_transform_scaled(model, &t, 1.0, 1.0, flatness);
    x.model_opacity(model, if eased >= 0.9 { (1.0 - eased) * 10.0 } else { 1.0 });
    x.model_prepare(model);
    x.model_draw(model, 1.0)
}

/// The flash's pass 6.
///
/// # Safety
/// As `aged`.
unsafe fn draw_flash<H: Effect3DHost, R: Raster>(
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract
    let mut camera = x.camera();
    let model = x.standing_plane();
    x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
    let fog = x.get_fog();
    let (t, draw_age) = unsafe { ((*r).transformation, (*r).aged.draw_age) };
    let location = ee_ge::types::Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
    x.lighting(EeGfxLighting::White);
    let mut value = x.fog_factor(&location, fog);
    value *= 1.0 - draw_age * draw_age;
    x.set_ambient(EeGfxColor { red: value, green: value, blue: value });
    x.set_blending(BLEND_ONE, BLEND_ONE);
    x.set_shading_source(0);
    x.depth_range(0.0, 0.0);
    x.set_zbuffer(ZBUFFER_TEST_ONLY);
    x.set_zcompare(COMPARE_LESS);
    x.model_color(model, EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 });
    x.model_material_opacity(model, 1.0);
    camera.m[0][3] = location.x;
    camera.m[1][3] = location.y;
    camera.m[2][3] = location.z;
    x.model_transform(model, &camera);
    x.model_scale(model, g.size);
    let texture = h.texture();
    x.model_texture(model, texture);
    x.model_prepare(model);
    let result = x.model_draw(model, 1.0);
    x.set_fog(fog);
    x.depth_range(0.0, 1.0);
    x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
    x.set_zcompare(COMPARE_LESS_EQUAL);
    result
}

#[cfg(test)]
#[path = "effect_aged_tests.rs"]
mod tests;
