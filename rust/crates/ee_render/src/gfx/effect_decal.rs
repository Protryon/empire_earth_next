//! The battlefield decal (kind 0x32, `EEE2DBattlefieldAmbient`) and the pooled 2D particle (kind 0x2e,
//! `EEE2DParticle`) whole in Rust on `EEE2DRust` (R4.4, twenty-first round), their classes deleted. Their state was
//! already the record's (`effect_state.rs`: the decal's placement on the terrain, tick, load and save; the particle's
//! set-up, move, tick, load and save); this adds their virtuals: the queues, the draws (the decal's ground sprite
//! fading over its last fifth, the particle's glow or blended sprite growing and fading), the set-ups and the files.
//! The C++ keeps the decal's placement helpers (`EEPlaceImpactDecal`, `EEPlaceTrackDecal`), its constructor's
//! terrain walk, and the particle's pool.

use ee_base::math::types::Ge3DPoint;

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_state::{
    ambient_step, load_ambient, load_particle, particle_set_up, particle_step, particle_update, save_ambient,
    save_particle, EeGfxEffectRecord, EeGfxParticleSetUp, EffectFile,
};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::ffi_effect::Ground;
use crate::gfx::raster::{Raster, EE_GFX_SPRITE_BLENDED, EE_GFX_SPRITE_GLOW, EE_GFX_SPRITE_GROUND};

/// `kEffect2DBattlefieldAmbient`, `kEffect2DParticle`.
pub const EE_GFX_EFFECT_KIND_DECAL: i32 = 0x32;
pub use crate::gfx::effect_state::EE_GFX_EFFECT_KIND_PARTICLE_2D;

/// The alpha reference of the graphic effect's object (0 without one).
fn alpha_ref(g: &EeGfxGraphicFacts) -> f32 {
    if g.has_object {
        g.alpha_ref
    } else {
        0.0
    }
}

/// The decal's and the particle's virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn decal<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY: the function's contract
    if unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_PARTICLE_2D {
        return unsafe { particle(op, r, g, h, x) };
    }
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    match op {
        // 0xdeb58: the media, the effect at its transformation's position, the effect's size as its radius
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            unsafe {
                let t = (*r).ambient.transformation;
                (*r).position = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
            }
            h.move_to_point();
            unsafe { (*r).bounding_radius = g.size };
            res
        }
        // 0xdeab2: the age and size for the draw (flag 2: growing over the first tenth of life), render queue 0
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let a = &mut (*r).ambient;
                    a.draw_age = a.age;
                    a.draw_size = if g.flag2 && a.age < 0.1 { (a.age * 10.0) * a.size } else { a.size };
                }
                h.queue(0);
            }
            res
        }
        // 0xdebba: flat on the ground, fading over the last fifth of life; nothing while hidden
        EeGfxEffect3DOp::DrawPass2 => {
            let a = unsafe { (*r).ambient };
            if a.hidden != 0.0 {
                return 0;
            }
            let fade = if a.draw_age >= 0.8 { (1.0 - a.draw_age) * 5.0 } else { 1.0 };
            let t = a.transformation;
            let p = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
            let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
            x.sprite(EE_GFX_SPRITE_GROUND, &p, white, a.rotation, a.draw_size, g.alpha * fade, g.param1, alpha_ref(g));
            0
        }
        // 0xdee2b: aged by the tick over the duration; 7 once over
        EeGfxEffect3DOp::Step => unsafe { ambient_step(&mut *r, g.tick_seconds, g.duration) },
        _ => 0,
    }
}

/// The particle's virtuals.
///
/// # Safety
/// As `decal`.
unsafe fn particle<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    match op {
        // 0xe4b47: the size as the radius, age 0, a random turn (int param 1 of 0) and brightness, the media
        EeGfxEffect3DOp::SetUp => {
            let s = EeGfxParticleSetUp { int_param1: g.int_param1, param2: g.param2, param3: g.param3 };
            unsafe {
                (*r).bounding_radius = g.size;
                particle_set_up(&mut *r, &s);
            }
            h.object_media()
        }
        // 0xe4833: moved by the game time since the last move (`Update`), then render queue 4; 7 once it dies
        EeGfxEffect3DOp::Queue => {
            let mut res = h.base_queue();
            if res == 0 {
                let tick = h.particle_tick();
                let ground = h.particle_ground();
                res = unsafe { particle_update(&mut *r, &tick, &mut Ground(&ground)) };
                if res == 0 {
                    h.queue(4);
                }
            }
            res
        }
        // 0xe5078: a sprite in the effect's colour from the start size to the size, fading with age (flag 1: glowing,
        // else blended)
        EeGfxEffect3DOp::DrawPass6 => {
            let p = unsafe { (*r).particle };
            let color = EeGfxColor { red: g.param5, green: g.param6, blue: g.param7 };
            let size = (g.size - g.start_size) * p.draw_age + g.start_size;
            let kind = if g.flag1 { EE_GFX_SPRITE_GLOW } else { EE_GFX_SPRITE_BLENDED };
            x.sprite(
                kind,
                &p.draw_position,
                color,
                p.yaw,
                size,
                (1.0 - p.draw_age) * g.param1,
                p.brightness,
                alpha_ref(g),
            );
            0
        }
        // 0xe520d: 7 once its duration is over; else at the position the move left
        EeGfxEffect3DOp::Step => {
            let now = h.particle_tick().now;
            let mut moved = false;
            let res = unsafe { particle_step(&mut *r, now, &mut moved) };
            if moved {
                unsafe { (*r).position = (*r).particle.draw_position };
                h.move_to_point();
            }
            res
        }
        _ => 0,
    }
}

/// `Load` (`load`) or `Save` of a decal or particle after `EEEffect2D`'s (whose result each ignores): a load's media
/// (its result ignored), then the record's own reads or writes; a loaded particle is drawn where it is, for its
/// graphic effect's duration.
pub fn decal_file<H: Effect2DHost, F: EffectFile>(
    load: bool,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    h: &mut H,
) -> i32 {
    if load {
        h.object_media();
    }
    if r.kind == EE_GFX_EFFECT_KIND_PARTICLE_2D {
        if !load {
            return save_particle(r, f);
        }
        let mut loaded = false;
        let res = load_particle(r, f, major, minor, &mut loaded);
        if loaded {
            r.particle.draw_position = r.position;
            r.particle.duration = h.particle_tick().duration as i32 as f32;
        }
        return res;
    }
    if load {
        load_ambient(r, f)
    } else {
        save_ambient(r, f)
    }
}

#[cfg(test)]
#[path = "effect_decal_tests.rs"]
mod tests;
