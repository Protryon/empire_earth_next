//! `effect_shockwave.rs`'s virtuals: the 2D shockwave over a mock host and the logging rasterizer.

use super::*;
use crate::effect_2d::{effect2d, effect2d_file};
use crate::effect_confirm::{EeGfxConfirmGoal, EeGfxConfirmUnit};
use crate::effect_state::EE_GFX_EFFECT_KIND_SHOCKWAVE;
use crate::effect_vent::EeGfxFrames;
use crate::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    base: i32,
    media: i32,
}

impl Effect2DHost for Host {
    fn base_queue(&mut self) -> i32 {
        self.calls.push("base".into());
        self.base
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        panic!("the shockwave loads its media through object_media")
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
        GeTransformation::default()
    }
    fn terrain_z(&mut self, _x: f32, _y: f32) -> f32 {
        0.0
    }
    fn command_texture(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn spawn_point(&mut self, kind: u32) -> Ge3DPoint {
        self.calls.push(format!("spawn {kind}"));
        Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }
    }
    fn object_media(&mut self) -> i32 {
        self.calls.push("media".into());
        self.media
    }
    fn object_texture(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("texture {index}"));
        std::ptr::null_mut()
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 4.0,
        start_size: 1.0,
        duration: 1000,
        tick_seconds: 0.25,
        has_object: true,
        alpha_ref: 0.5,
        has_unit: true,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster { fog: 1, fog_value: 0.5, ..Default::default() };
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect2d(op, r, g, h, &mut x) };
    (result, x)
}

#[test]
fn a_units_wave_stands_at_a_tag_point() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHOCKWAVE);
    r.aged.draw_age = 0.5;
    let mut h = Host { media: 3, ..Default::default() };
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h);
    assert_eq!(r.aged.draw_age, 0.0);
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 3);
    assert_eq!(h.calls, ["spawn 4", "move", "media"]);
    assert_eq!((r.position, r.bounding_radius), (Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, 4.0));
    // at a point: placed by the constructor
    let mut h = Host::default();
    let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHOCKWAVE);
    run(EeGfxEffect3DOp::SetUp, &mut p, &EeGfxGraphicFacts { has_unit: false, ..facts() }, &mut h);
    assert_eq!(h.calls, ["media"]);
}

#[test]
fn queue_and_step() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHOCKWAVE);
    r.aged.age = 0.25;
    let mut h = Host { base: 10, ..Default::default() };
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 10);
    assert_eq!(r.aged.draw_age, 0.0);
    h.base = 0;
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h);
    assert_eq!((r.aged.draw_age, h.calls.last().map(String::as_str)), (0.25, Some("queue 4")));
    let mut results = Vec::new();
    for _ in 0..4 {
        results.push(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0);
    }
    assert_eq!(results, [0, 0, 0, 7]);
}

#[test]
fn the_draw_swells_and_fades() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHOCKWAVE);
    r.position = Ge3DPoint { x: 5.0, y: 6.0, z: 7.0 };
    r.aged.draw_age = 0.75;
    let mut h = Host::default();
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!(result, 0);
    // eased out: 1 - (1/16)^2 of the way from 1 to 4; past half way the light fades: (1 - 0.75) x 0.5 x 2
    let scale = 3.0 * (1.0 - (0.0625f32 * 0.0625)) + 1.0;
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5".to_string(),
            "zbuffer 2".into(),
            "blend 1 1".into(),
            "shading 0".into(),
            "lighting White".into(),
            "texture".into(),
            "transform 5 6 7".into(),
            format!("scale {scale}"),
            "fogfactor 5 6 7 1".into(),
            "ambient 0.25 0.25 0.25".into(),
            "prepare".into(),
            "draw 1".into(),
            "fog 1".into()
        ]
    );
    // young and eased in (flag 1): the fog factor's light, (1/4)^2 of the way
    r.aged.draw_age = 0.25;
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &EeGfxGraphicFacts { flag1: true, ..facts() }, &mut h);
    let fade = 0.75f32 * 0.75;
    assert_eq!(x.calls[7], format!("scale {}", 3.0 * (fade * fade) + 1.0));
    assert_eq!(x.calls[9], "ambient 0.5 0.5 0.5");
}

struct Mem(Vec<u32>, usize);

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        let Some(&x) = self.0.get(self.1) else { return 3 };
        self.1 += 1;
        *v = x;
        0
    }
    fn read_i32(&mut self, _v: &mut i32) -> i32 {
        panic!("read_i32")
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut x = 0;
        let res = self.read_u32(&mut x);
        *v = f32::from_bits(x);
        res
    }
    fn read_bool(&mut self, _v: &mut bool) -> i32 {
        panic!("read_bool")
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        self.0.push(v);
        0
    }
    fn write_i32(&mut self, _v: i32) -> i32 {
        panic!("write_i32")
    }
    fn write_f32(&mut self, v: f32) -> i32 {
        self.write_u32(v.to_bits())
    }
    fn write_bool(&mut self, _v: bool) -> i32 {
        panic!("write_bool")
    }
}

#[test]
fn load_and_save_the_age() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHOCKWAVE);
    r.aged.age = 0.375;
    let mut h = Host::default();
    let mut f = Mem(Vec::new(), 0);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Save, &mut r, &mut f, 0, 0, &mut h), 0);
    assert_eq!(f.0, [0.375f32.to_bits()]);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHOCKWAVE);
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 0);
    assert_eq!((back.aged.age, h.calls.as_slice()), (0.375, ["media".to_string()].as_slice()));
    // a short file: the read's error (the media's result ignored)
    let mut h = Host { media: 5, ..Default::default() };
    assert_eq!(effect2d_file(EeGfxEffect3DOp::Load, &mut back, &mut f, 0, 0, &mut h), 3);
}
