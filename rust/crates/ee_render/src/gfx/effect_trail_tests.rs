//! `effect_trail.rs`'s virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::ge::types::GeTransformation;
use crate::gfx::effect_2d::{effect2d, effect2d_file};
use crate::gfx::effect_confirm::{EeGfxConfirmGoal, EeGfxConfirmUnit};
use crate::gfx::effect_state::set_random_seed;
use crate::gfx::effect_vent::EeGfxFrames;
use crate::gfx::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    frames: EeGfxFrames,
}

impl Effect2DHost for Host {
    fn base_queue(&mut self) -> i32 {
        0
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        panic!("not the trails'")
    }
    fn frames(&mut self, _index: u32) -> EeGfxFrames {
        self.frames
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
    fn wind(&mut self) -> Ge3DPoint {
        Ge3DPoint { x: 0.0, y: 0.0, z: 0.0 }
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        start_size: 1.0,
        duration: 1000,
        tick_seconds: 0.1,
        tick_time: 100.0,
        param1: 10.0,
        param2: 400.0,
        param3: 0.5,
        time1: 50,
        alpha: 0.5,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: the record is live and only reached through this pointer during the call
    unsafe { effect2d(op, r, g, h, x) }
}

/// A file of typed values.
#[derive(Default)]
struct Mem {
    items: Vec<u32>,
    at: usize,
}

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        if self.at >= self.items.len() {
            return 3;
        }
        *v = self.items[self.at];
        self.at += 1;
        0
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = u as i32;
        r
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = f32::from_bits(u);
        r
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = u != 0;
        r
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        self.items.push(v);
        0
    }
    fn write_i32(&mut self, v: i32) -> i32 {
        self.write_u32(v as u32)
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        self.write_u32(v.to_bits())
    }
    fn write_bool(&mut self, v: bool) -> i32 {
        self.write_u32(v as u32)
    }
}

#[test]
fn breath_takes_its_timings_from_the_mouth_and_the_move() {
    set_random_seed(4);
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WINTER_BREATH);
    r.sprite.start = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    r.sprite.flag = true;
    r.sprite.shrinking = true;
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::CreateBetween, &mut r, &g, &mut h, &mut x);
    assert_eq!((r.sprite.duration, r.sprite.speed, r.sprite.slow_down), (550, 10.5, 460.0));
    assert_eq!(r.position, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
    assert_eq!((r.sprite.start, r.sprite.flag, r.sprite.shrinking), (Ge3DPoint::default(), false, false));
    let half = std::f32::consts::FRAC_1_SQRT_2;
    assert!((r.sprite.velocity.y - half).abs() < 1e-6 && (r.sprite.velocity.z + half).abs() < 1e-6);
    // without the tag list: inert
    let mut q = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WINTER_BREATH);
    run(EeGfxEffect3DOp::CreateBetween, &mut q, &g, &mut h, &mut x);
    assert_eq!((q.sprite.duration, q.sprite.speed, q.sprite.fade), (0, 0.0, 0.0));
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut q, &g, &mut h, &mut x), 7);
    // a step slows by the fade; a draw past both delays makes three puffs
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(r.sprite.age, 100.0);
    run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x);
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert_eq!(x.calls.len(), 3);
    let mut m = Mem::default();
    effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut m, 0, 0, &mut h);
    assert_eq!(m.items.len(), 14);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WINTER_BREATH);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut m, 0, 0, &mut h), 0);
    assert_eq!(back.sprite, r.sprite);
}

#[test]
fn projectile_runs_its_line_in_the_duration() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE);
    r.sprite.start = Ge3DPoint { x: 1.0, y: 1.0, z: 0.0 };
    r.sprite.draw_position = Ge3DPoint { x: 4.0, y: 5.0, z: 0.0 };
    let mut h = Host { frames: EeGfxFrames { start: 1, time: 50, count: 3, last: 2 }, ..Default::default() };
    let mut x = LogRaster::default();
    let g = facts();
    run(EeGfxEffect3DOp::CreateBetween, &mut r, &g, &mut h, &mut x);
    assert_eq!((r.position.x, r.sprite.velocity), (1.0, Ge3DPoint { x: 3.0, y: 4.0, z: 0.0 }));
    run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut x);
    // 1000 ms * 0.00001 over a length of 5
    assert_eq!((r.sprite.frame, r.sprite.speed), (1, 0.01 / 5.0));
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!((r.sprite.frame, r.sprite.time), (0, 0.0));
    assert_eq!(r.position.x, 3.0 * r.sprite.age + 1.0);
    let mut m = Mem::default();
    effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut m, 0, 0, &mut h);
    assert_eq!(m.items.len(), 16);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut m, 0, 0, &mut h), 0);
    assert_eq!(
        (back.sprite.start, back.sprite.velocity, back.sprite.age),
        (r.sprite.start, r.sprite.velocity, r.sprite.age)
    );
    for _ in 0..600 {
        if run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x) == 7 {
            return;
        }
    }
    panic!("the projectile never arrived");
}
