//! `effect_vortex.rs`'s virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_2d::{effect2d, effect2d_file};
use crate::gfx::effect_confirm::{EeGfxConfirmGoal, EeGfxConfirmUnit};
use crate::gfx::effect_orbit::EeGfxOrbital;
use crate::gfx::effect_vent::EeGfxFrames;
use crate::gfx::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
}

impl Effect2DHost for Host {
    fn base_queue(&mut self) -> i32 {
        0
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        panic!("not the vortex's")
    }
    fn frames(&mut self, _index: u32) -> EeGfxFrames {
        EeGfxFrames::default()
    }
    fn move_to_point(&mut self) {
        self.calls.push("move".into());
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn selection_size(&mut self) -> f32 {
        0.0
    }
    fn confirm_enemy(&mut self) -> bool {
        false
    }
    fn confirm_goal(&mut self, _goal_index: u32, _tile_x: i32, _tile_y: i32) -> EeGfxConfirmGoal {
        EeGfxConfirmGoal::NoUnit
    }
    fn confirm_unit(&mut self) -> EeGfxConfirmUnit {
        EeGfxConfirmUnit::default()
    }
    fn unit_location(&mut self) -> GeTransformation {
        GeTransformation::IDENTITY
    }
    fn terrain_z(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
    }
    fn command_texture(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn object_media(&mut self) -> i32 {
        self.calls.push("media".into());
        0
    }
    fn orbital_count(&mut self) -> u32 {
        2
    }
    fn orbital(&mut self, i: u32) -> EeGfxOrbital {
        EeGfxOrbital { model: std::ptr::null_mut(), speed: 1.0 + i as f32, angle: 0.5 }
    }
}

/// A file of floats.
#[derive(Default)]
struct Mem {
    items: Vec<f32>,
    at: usize,
}

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, _v: &mut u32) -> i32 {
        3
    }
    fn read_i32(&mut self, _v: &mut i32) -> i32 {
        3
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        if self.at >= self.items.len() {
            return 3;
        }
        *v = self.items[self.at];
        self.at += 1;
        0
    }
    fn read_bool(&mut self, _v: &mut bool) -> i32 {
        3
    }
    fn write_u32(&mut self, _v: u32) -> i32 {
        3
    }
    fn write_i32(&mut self, _v: i32) -> i32 {
        3
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        self.items.push(v);
        0
    }
    fn write_bool(&mut self, _v: bool) -> i32 {
        3
    }
}

#[test]
fn vortex_spins_ages_and_fades() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_VORTEX);
    r.sprite.start = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let g = EeGfxGraphicFacts {
        size: 2.0,
        duration: 1000,
        tick_time: 100.0,
        tick_seconds: 0.1,
        param1: 0.005,
        ..Default::default()
    };
    // SAFETY (each call): the record is live and only reached through this pointer during the call
    unsafe {
        effect2d(EeGfxEffect3DOp::CreateAt, &mut r, &g, &mut h, &mut x);
        assert_eq!((r.position, r.sprite.start), (Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, Ge3DPoint::default()));
        effect2d(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
        assert_eq!((r.sprite.angle, r.bounding_radius), (0.1, 2.0));
        for _ in 0..2 {
            assert_eq!(effect2d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
        }
        // 0.1 + 0.5 + 0.5 wraps once
        assert!((r.sprite.angle - 0.1).abs() < 1e-6);
        assert_eq!(r.sprite.age, 0.2);
        effect2d(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x);
        r.sprite.draw_age = 0.95;
        effect2d(EeGfxEffect3DOp::DrawPass5, &mut r, &g, &mut h, &mut x);
    }
    assert_eq!(x.calls[5], "transform 1 2 3.2");
    assert!(x.calls.iter().any(|c| c.starts_with("opacity 0.49") || c.starts_with("opacity 0.50")));
    assert_eq!(x.calls.last().unwrap(), "opacity 1");
    // a load copies the graphic's orbitals; the draw poses them
    let mut m = Mem::default();
    effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut m, 0, 0, &mut h);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_VORTEX);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut m, 0, 0, &mut h), 0);
    assert_eq!((back.orbit.count, back.sprite.age, back.sprite.angle), (2, r.sprite.age, r.sprite.angle));
    let mut x = LogRaster::default();
    // SAFETY: as above
    unsafe {
        effect2d(EeGfxEffect3DOp::DrawPass5, &mut back, &g, &mut h, &mut x);
        effect2d(EeGfxEffect3DOp::Step, &mut back, &g, &mut h, &mut x);
        assert_eq!(orbitals(&mut back).list[1].angle, 0.7);
    }
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("pre ")).count(), 2);
}
