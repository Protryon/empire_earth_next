//! `effect_decal.rs`'s virtuals over the scripted host and the logging rasterizer.

use super::*;
use crate::gfx::effect_2d::{effect2d, effect2d_file};
use crate::gfx::effect_2d_mock::{Host, Mem};
use crate::gfx::effect_state::EeGfxParticleTick;
use crate::gfx::raster::LogRaster;

#[test]
fn the_decal_sets_up_grows_fades_and_ages() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DECAL);
    r.ambient.transformation.m[0][3] = 4.0;
    r.ambient.transformation.m[1][3] = 5.0;
    r.ambient.transformation.m[2][3] = 6.0;
    r.ambient.size = 2.0;
    r.ambient.age = 0.05;
    r.ambient.rotation = 0.5;
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let g = EeGfxGraphicFacts {
        size: 3.0,
        flag2: true,
        alpha: 0.5,
        param1: 0.75,
        tick_seconds: 0.1,
        duration: 1000,
        ..Default::default()
    };
    // SAFETY (each call): the record is live and only reached through this pointer during the call
    unsafe {
        assert_eq!(effect2d(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!((r.position, r.bounding_radius), (Ge3DPoint { x: 4.0, y: 5.0, z: 6.0 }, 3.0));
        assert_eq!(effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!((r.ambient.draw_age, r.ambient.draw_size), (0.05, (0.05f32 * 10.0) * 2.0));
        effect2d(EeGfxEffect3DOp::DrawPass2, &mut r, &g, &mut h, &mut x);
        assert_eq!(x.calls[0], format!("sprite 1 4 5 6 color 1 1 1 rotation 0.5 size {} 0.5 0.75 0", 1.0f32));
        r.ambient.age = 0.9;
        effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x);
        assert_eq!(r.ambient.draw_size, 2.0);
        effect2d(EeGfxEffect3DOp::DrawPass2, &mut r, &g, &mut h, &mut x);
        assert!(x.calls[1].ends_with(&format!(" {} 0.75 0", 0.5f32 * ((1.0f32 - 0.9) * 5.0))));
        r.ambient.hidden = 1.0;
        effect2d(EeGfxEffect3DOp::DrawPass2, &mut r, &g, &mut h, &mut x);
        assert_eq!(x.calls.len(), 2);
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    }
    assert!(h.calls.contains(&"queue 0".to_string()));
    let mut f = Mem::default();
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut f, 0, 0, &mut h), 0);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DECAL);
    h.media = 9;
    // the media's result is ignored
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 0);
    assert_eq!((back.ambient.size, back.ambient.age, back.ambient.hidden), (2.0, r.ambient.age, 0.0));
}

#[test]
fn the_particle_moves_in_the_queue_and_steps_to_the_move() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_2D);
    r.particle.velocity = Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 };
    r.particle.duration = 1000.0;
    let mut h =
        Host { tick: EeGfxParticleTick { now: 100.0, duration: 1000, ..Default::default() }, ..Default::default() };
    let mut x = LogRaster::default();
    let g = EeGfxGraphicFacts {
        size: 3.0,
        start_size: 1.0,
        param1: 0.8,
        param2: 0.5,
        param5: 0.1,
        param6: 0.2,
        param7: 0.3,
        int_param1: 1,
        ..Default::default()
    };
    // SAFETY (each call): as above
    unsafe {
        assert_eq!(effect2d(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!((r.bounding_radius, r.particle.yaw, r.particle.brightness), (3.0, 0.0, 0.5));
        assert_eq!(effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
        assert!(r.particle.moved);
        assert_eq!(r.particle.draw_position.x, 0.1);
        effect2d(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
        assert_eq!(
            x.calls[0],
            format!(
                "sprite 2 0.1 0 0 color 0.1 0.2 0.3 rotation 0 size {} {} 0.5 0",
                2.0f32 * 0.1 + 1.0,
                (1.0f32 - 0.1) * 0.8
            )
        );
        let g1 = EeGfxGraphicFacts { flag1: true, ..g };
        effect2d(EeGfxEffect3DOp::DrawPass6, &mut r, &g1, &mut h, &mut x);
        assert!(x.calls[1].starts_with("sprite 0 "));
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!(r.position.x, 0.1);
        assert!(!r.particle.moved);
        h.tick.now = 2000.0;
        assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    }
    assert_eq!(h.calls.iter().filter(|c| *c == "move").count(), 1);
    let mut f = Mem::default();
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut f, 0, 0, &mut h), 0);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_2D);
    back.position = Ge3DPoint { x: 9.0, y: 8.0, z: 7.0 };
    h.tick.duration = 1500;
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 1, 0, &mut h), 0);
    assert_eq!((back.particle.draw_position, back.particle.duration), (back.position, 1500.0));
    assert_eq!(back.particle.velocity, r.particle.velocity);
}

#[test]
fn the_damage_controller_draws_and_sets_up_nothing() {
    let mut r = EeGfxEffectRecord::new(crate::gfx::effect_state::EE_GFX_EFFECT_KIND_DAMAGED);
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let g = EeGfxGraphicFacts::default();
    // SAFETY (each call): the record is live and only reached through this pointer during the call
    unsafe {
        assert_eq!(effect2d(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!(effect2d(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x), 0);
        assert_eq!(effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 10);
        assert_eq!(effect2d(EeGfxEffect3DOp::DrawPass2, &mut r, &g, &mut h, &mut x), 0);
    }
    assert!(h.calls.is_empty() && x.calls.is_empty());
}
