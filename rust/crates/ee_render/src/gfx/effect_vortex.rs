//! The 2D vortex (kind 0x31, `EEE2DVortex`) whole in Rust on `EEE2DRust` (R4.4, twentieth round), its class deleted:
//! the graphic object's own model spun at a point, its orbitals (copied from the graphic only by a load, as the
//! original's: a Rust list the record owns, `effect_orbit.rs`'s) posed at the angles the draw keeps (all 0, as the
//! original's never set).

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::{Ge3DPoint, GeTransformation};

use crate::gfx::effect_2d::Effect2DHost;
use crate::gfx::effect_orbit::EeGfxOrbitals;
use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts};
use crate::gfx::effect_sprite::EeGfxEffectSprite;
use crate::gfx::effect_state::{EeGfxEffectRecord, EffectFile, OK};
use crate::gfx::lighting::EeGfxLighting;
use crate::gfx::raster::{Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE};

/// `kEffect2DVortex`.
pub const EE_GFX_EFFECT_KIND_VORTEX: i32 = 0x31;

/// The record's sprite state.
///
/// # Safety
/// `r` is a live record; the reference does not live across a host call.
unsafe fn sp<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxEffectSprite {
    // SAFETY: the function's contract
    unsafe { &mut (*r).sprite }
}

/// The record's orbitals (made on first use).
///
/// # Safety
/// As `sp`.
unsafe fn orbitals<'a>(r: *mut EeGfxEffectRecord) -> &'a mut EeGfxOrbitals {
    // SAFETY: the function's contract; the list is the record's own Box, freed with it
    unsafe {
        if (*r).orbit.orbitals.is_null() {
            (*r).orbit.orbitals = Box::into_raw(Box::default());
        }
        &mut *(*r).orbit.orbitals
    }
}

/// The vortex's virtuals.
///
/// # Safety
/// As `effect_2d::effect2d`.
pub unsafe fn vortex<H: Effect2DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record lives across a host call
    match op {
        // 0xe777f at the point (the C++ put it in `start`): age 0, frame 0, no orbitals
        EeGfxEffect3DOp::CreateAt => {
            unsafe {
                let p = sp(r).start;
                sp(r).start = Ge3DPoint::default();
                (*r).position = p;
            }
            h.move_to_point();
            0
        }
        // 0xe790f: the media at age 0 and spin 0.1; the radius
        EeGfxEffect3DOp::SetUp => {
            let res = h.object_media();
            unsafe {
                let s = sp(r);
                s.age = 0.0;
                s.angle = 0.1;
                s.draw_angle = 0.1;
                (*r).bounding_radius = g.size;
            }
            res
        }
        // 0xe787e: the frame, age, spin and position for the draw, render queue 3
        EeGfxEffect3DOp::Queue => {
            let res = h.base_queue();
            if res == 0 {
                unsafe {
                    let s = sp(r);
                    s.draw_frame = s.frame;
                    s.draw_age = s.age;
                    s.draw_angle = s.angle;
                    s.draw_position = (*r).position;
                }
                h.queue(3);
            }
            res
        }
        // 0xe7949: the graphic's model 0.2 above the point with its orbitals posed, fading over the last tenth of life
        EeGfxEffect3DOp::DrawPass5 => {
            let model = h.object_model();
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::Unit);
            let s = unsafe { *sp(r) };
            let mut t = GeTransformation::IDENTITY;
            t.m[0][3] = s.draw_position.x;
            t.m[1][3] = s.draw_position.y;
            t.m[2][3] = s.draw_position.z + 0.2;
            x.model_transform(model, &t);
            x.model_frame(model, s.draw_frame);
            x.model_scale(model, g.size);
            let count = unsafe { (*r).orbit.count };
            for i in 0..count.max(0) as usize {
                let (orbital, angle) = unsafe {
                    let o = orbitals(r);
                    (o.list[i].model, o.angles[i])
                };
                let mut turn = GeTransformation::IDENTITY;
                turn.set_orientation(RotationOrder::Ypr, angle, 0.0, 0.0);
                x.model_pre_transform(orbital, &turn);
            }
            x.model_prepare(model);
            let texture = h.object_texture(0);
            x.model_texture(model, texture);
            if s.draw_age >= 0.9 {
                x.model_opacity(model, (1.0 - s.draw_age) * 10.0);
            }
            let res = x.model_draw(model, 1.0);
            x.model_opacity(model, 1.0);
            res
        }
        // 0xe7c57: ages and spins; follows a unit with a complex unit; the orbitals turn; 7 once over
        EeGfxEffect3DOp::Step => {
            let age = unsafe {
                let s = sp(r);
                s.age += g.tick_time / g.duration as f32;
                s.angle += g.param1 * g.tick_time;
                if s.angle > 1.0 {
                    s.angle -= 1.0;
                }
                s.age
            };
            if !(age <= 1.0) {
                return 7;
            }
            if g.has_unit && h.has_complex() {
                let l = h.unit_location();
                unsafe { (*r).position = Ge3DPoint { x: l.m[0][3], y: l.m[1][3], z: l.m[2][3] } };
                h.move_to_point();
            }
            let count = unsafe { (*r).orbit.count };
            for i in 0..count.max(0) as usize {
                unsafe { orbitals(r).list[i].update(g.tick_seconds) };
            }
            0
        }
        _ => 0,
    }
}

/// `Load` (0xe7b50: the media, the graphic's orbitals copied, the age and spin) or `Save` (0xe7c1c) after
/// `EEEffect2D`'s.
pub fn vortex_file<H: Effect2DHost, F: EffectFile>(load: bool, r: &mut EeGfxEffectRecord, f: &mut F, h: &mut H) -> i32 {
    if load {
        h.object_media();
        let count = h.orbital_count();
        r.orbit.count = count as i32;
        let rp: *mut EeGfxEffectRecord = r;
        // SAFETY: `r` is live and the list its own; the host's orbital calls do not reach the record
        let o = unsafe { orbitals(rp) };
        o.angles.resize(count as usize, 0.0);
        for i in 0..count {
            o.list.push(h.orbital(i));
        }
        let res = f.read_f32(&mut r.sprite.age);
        if res == OK {
            f.read_f32(&mut r.sprite.angle)
        } else {
            res
        }
    } else {
        let res = f.write_f32(r.sprite.age);
        if res == OK {
            f.write_f32(r.sprite.angle)
        } else {
            res
        }
    }
}

#[cfg(test)]
#[path = "effect_vortex_tests.rs"]
mod tests;
