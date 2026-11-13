//! `effect_aged.rs`'s tests: the water splash's and screen flash's virtuals over a mock host and the logging
//! rasterizer.

use super::*;
use crate::effect_rising::{effect3d, effect3d_file};
use crate::effect_state::{EffectFile, EE_GFX_EFFECT_KIND_WATER_SPLASH};
use crate::raster::{EeGfxModel, LogRaster};
use ee_ge::types::{Ge3DPoint, GeVector};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
}

impl Effect3DHost for Host {
    fn unit_position(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn complex_position(&mut self) -> Ge3DPoint {
        self.calls.push("position".into());
        Ge3DPoint { x: 7.0, y: 8.0, z: 9.0 }
    }
    fn model_height(&mut self) -> f32 {
        0.0
    }
    fn move_to(&mut self) {
        self.calls.push("move".into());
    }
    fn create_line(&mut self, _s: &Ge3DPoint, _d: &GeVector, _l: u32) {}
    fn base_queue(&mut self) -> i32 {
        self.queue_result
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        self.calls.push("media".into());
        3
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn texture(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn follow_unit(&mut self) {}
    fn model_at(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn shot_hit(&mut self) {}
    fn beam_start(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn beam_unit(&mut self) -> crate::effect_beam::EeGfxBeamUnit {
        crate::effect_beam::EeGfxBeamUnit::default()
    }
    fn beam_hidden(&mut self) -> bool {
        false
    }
    fn place_linked(&mut self, _point: &Ge3DPoint) {}
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 3.0,
        start_size: 1.0,
        duration: 1000,
        tick_seconds: 0.25,
        has_object: true,
        alpha_ref: 0.5,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster { fog: 1, fog_value: 0.5, ..Default::default() };
    x.camera.m[0][0] = 2.0;
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(op, r, g, h, &mut x) };
    (result, x)
}

#[test]
fn the_splash_is_placed_queued_and_aged() {
    let mut h = Host::default();
    let mut s = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WATER_SPLASH);
    s.aged.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    run(EeGfxEffect3DOp::CreateAt, &mut s, &facts(), &mut h);
    assert_eq!((s.transformation.m[0][3], s.transformation.m[1][3], s.transformation.m[2][3]), (1.0, 2.0, 3.0));
    // the set-up's result is the media's
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut s, &facts(), &mut h).0, 3);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut s, &facts(), &mut h).0, 0);
    assert_eq!(s.aged.age, 0.25);
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut s, &facts(), &mut h).0, 0);
    assert_eq!(s.aged.draw_age, 0.25);
    h.queue_result = 10;
    s.aged.age = 0.5;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut s, &facts(), &mut h).0, 10);
    assert_eq!(s.aged.draw_age, 0.25);
    assert_eq!(h.calls, ["move", "media", "queue 4"]);
    // it ends past 1 (the flash at 1)
    s.aged.age = 0.75;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut s, &facts(), &mut h).0, 0);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut s, &facts(), &mut h).0, 7);
    let mut f = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SCREEN_FLASH);
    f.aged.age = 0.75;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut f, &facts(), &mut h).0, 7);
}

#[test]
fn the_splash_draw_eases_flattens_and_fades() {
    let mut h = Host::default();
    let mut s = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_WATER_SPLASH);
    s.transformation.m[0][3] = 4.0;
    s.aged.draw_age = 0.5;
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut s, &facts(), &mut h);
    assert_eq!(result, 0);
    // eased 1 - 0.5^2 = 0.75: written back, scale (3 - 1) * 0.75 + 1, flattened to 1 - 0.5^2
    assert_eq!(s.aged.draw_age, 0.75);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "zbuffer 2",
            "blend 4 5",
            "shading 0",
            "lighting Unit",
            "texture",
            "scale 2.5",
            "transform 4 0 0 scaled 1 1 0.75",
            "opacity 1",
            "prepare",
            "draw 1"
        ]
    );
    // the last tenth fades; a zero size draws at the smallest scale
    s.aged.draw_age = 0.8;
    let g = EeGfxGraphicFacts { size: 0.0, start_size: 0.0, has_object: false, ..facts() };
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut s, &g, &mut h);
    let eased = 1.0 - (1.0f32 - 0.8) * (1.0 - 0.8);
    assert_eq!(x.calls[0], "alpharef 0");
    assert_eq!(x.calls[6], "scale 0.00001");
    assert_eq!(x.calls[8], format!("opacity {}", (1.0 - eased) * 10.0));
    for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass5] {
        let (result, x) = run(op, &mut s, &facts(), &mut h);
        assert_eq!((result, x.calls.len()), (0, 0));
    }
}

#[test]
fn the_flash_sits_on_its_unit_and_draws_in_front() {
    let mut h = Host::default();
    let mut f = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SCREEN_FLASH);
    f.aged.age = 0.5;
    run(EeGfxEffect3DOp::Create, &mut f, &facts(), &mut h);
    assert_eq!(f.aged.age, 0.0);
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut f, &facts(), &mut h).0, 3);
    assert_eq!((f.transformation.m[0][3], f.transformation.m[2][3], f.bounding_radius), (7.0, 9.0, 3.0));
    assert_eq!(h.calls, ["position", "move", "media"]);
    f.aged.draw_age = 0.5;
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut f, &facts(), &mut h);
    assert_eq!(result, 0);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "lighting White",
            "fogfactor 7 8 9 1",
            "ambient 0.375 0.375 0.375",
            "blend 1 1",
            "shading 0",
            "depth 0 0",
            "zbuffer 2",
            "zcompare 1",
            "color 1 1 1",
            "material opacity 1",
            "transform 7 8 9",
            "scale 3",
            "texture",
            "prepare",
            "draw 1",
            "fog 1",
            "depth 0 1",
            "zbuffer 3",
            "zcompare 2"
        ]
    );
}

/// A file of typed values.
#[derive(Default)]
struct Mem(Vec<u32>, usize);

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        self.1 += 1;
        *v = self.0[self.1 - 1];
        0
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut u = 0;
        self.read_u32(&mut u);
        *v = u as i32;
        0
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut u = 0;
        self.read_u32(&mut u);
        *v = f32::from_bits(u);
        0
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut u = 0;
        self.read_u32(&mut u);
        *v = u != 0;
        0
    }
    fn write_u32(&mut self, v: u32) -> i32 {
        self.0.push(v);
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
fn both_save_and_load_their_age_ignoring_the_base_and_the_media() {
    for kind in [EE_GFX_EFFECT_KIND_WATER_SPLASH, EE_GFX_EFFECT_KIND_SCREEN_FLASH] {
        let mut h = Host::default();
        let mut r = EeGfxEffectRecord::new(kind);
        r.aged.age = 0.625;
        let mut m = Mem::default();
        assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 5, &mut r, &mut m, 0, 0, &mut h), 0);
        assert_eq!(m.0, [0.625f32.to_bits()]);
        let mut back = EeGfxEffectRecord::new(kind);
        assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 5, &mut back, &mut Mem(m.0, 0), 0, 0, &mut h), 0);
        assert_eq!((back.aged.age, h.calls.len()), (0.625, 1));
    }
}
