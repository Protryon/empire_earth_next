//! `effect_follow.rs`'s tests: the blast wave's and shield's virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::effect_rising::effect3d;
use crate::effect_state::EeGfxEffectShield;
use crate::raster::{EeGfxModel, LogRaster};
use ee_ge::types::GeVector;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
}

impl Effect3DHost for Host {
    fn unit_position(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn model_height(&mut self) -> f32 {
        0.0
    }
    fn move_to(&mut self) {
        self.calls.push("move".into());
    }
    fn create_line(&mut self, _s: &Ge3DPoint, _d: &GeVector, _l: u32) {}
    fn base_queue(&mut self) -> i32 {
        0
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        self.calls.push("media".into());
        0
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn texture(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn follow_unit(&mut self) {
        self.calls.push("follow".into());
    }
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
        size: 2.0,
        param1: 0.001,
        param2: -0.5,
        param3: 0.5,
        param4: 0.1,
        param5: 0.3,
        param6: 0.9,
        duration: 1000,
        tick_time: 250.0,
        tick_seconds: 0.25,
        has_unit: true,
        unit_hit_points: 50,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster { fog: 1, fog_value: 0.5, ..Default::default() };
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(op, r, g, h, &mut x) };
    (result, x)
}

#[test]
fn the_blast_wave_grows_ages_and_dims() {
    let mut h = Host::default();
    let mut b = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_BLAST_WAVE);
    b.aged.location = Ge3DPoint { x: 4.0, y: 5.0, z: 6.0 };
    run(EeGfxEffect3DOp::CreateAt, &mut b, &facts(), &mut h);
    assert_eq!((b.transformation.m[1][3], b.aged.aging, b.bounding_radius), (5.0, true, 2.0));
    assert_eq!(b.aged.location, Ge3DPoint::default());
    run(EeGfxEffect3DOp::SetUp, &mut b, &facts(), &mut h);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut b, &facts(), &mut h).0, 0);
    assert_eq!(b.aged.age, 0.25);
    assert_eq!(h.calls, ["move", "media", "follow", "follow"]);
    run(EeGfxEffect3DOp::Queue, &mut b, &facts(), &mut h);
    assert_eq!(h.calls.last().map(String::as_str), Some("queue 4"));
    // a quarter through: growing to max(0.1, 1 - fade^2) of the size; full light
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut b, &facts(), &mut h);
    let fade = 0.75f32 * 0.75;
    let scale = (1.0 - fade * fade) * 2.0;
    assert!(x.calls.contains(&format!("scale {scale}")));
    assert!(x.calls.contains(&"ambient 0.5 0.5 0.5".to_string()));
    assert_eq!(x.calls.last().map(String::as_str), Some("fog 1"));
    // shrinking (flag 1) at its end: the smallest scale; past half its light dims
    b.aged.draw_age = 1.0;
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut b, &EeGfxGraphicFacts { flag1: true, ..facts() }, &mut h);
    assert!(x.calls.contains(&"scale 0.1".to_string()) && x.calls.contains(&"ambient 0 0 0".to_string()));
    // an empty draw pass
    assert_eq!(run(EeGfxEffect3DOp::DrawPass5, &mut b, &facts(), &mut h).1.calls.len(), 0);
}

#[test]
fn the_shield_scrolls_flashes_and_fades() {
    let mut h = Host::default();
    let mut s = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHIELD);
    run(EeGfxEffect3DOp::SetUp, &mut s, &facts(), &mut h);
    assert_eq!((s.shield.opacity, s.shield.hit_points, s.bounding_radius), (0.3, 50, 2.0));
    // up by param 1 a ms until past param 3, then down until below param 2
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut s, &facts(), &mut h).0, 0);
    assert_eq!((s.shield.scroll, s.aged.age), (0.25, 0.25));
    run(EeGfxEffect3DOp::Step, &mut s, &facts(), &mut h);
    run(EeGfxEffect3DOp::Step, &mut s, &facts(), &mut h);
    assert!(!s.shield.scrolling_down && s.shield.scroll == 0.75 && s.shield.opacity == 0.3);
    // a hit flashes it to param 6; past param 3 it turns down
    run(EeGfxEffect3DOp::Step, &mut s, &EeGfxGraphicFacts { unit_hit_points: 40, ..facts() }, &mut h);
    assert_eq!((s.shield.opacity, s.shield.hit_points), (0.9, 40));
    assert!(s.shield.scrolling_down && s.shield.scroll == 0.5);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut s, &facts(), &mut h).0, 7);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut s, &EeGfxGraphicFacts { unit_hit_points: 0, ..facts() }, &mut h).0, 7);
    run(EeGfxEffect3DOp::Queue, &mut s, &facts(), &mut h);
    assert_eq!((s.shield.draw_scroll, h.calls.last().map(String::as_str)), (s.shield.scroll, Some("queue 3")));
    s.aged.draw_age = 0.95;
    let (_, x) = run(EeGfxEffect3DOp::DrawPass5, &mut s, &facts(), &mut h);
    assert_eq!(x.calls[4], "lighting Unit");
    assert!(x.calls.contains(&format!("opacity {}", (1.0f32 - 0.95) * 10.0)));
    assert_eq!(x.calls.last().map(String::as_str), Some("restore"));
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
fn the_shield_saves_and_loads_by_version() {
    let mut s = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHIELD);
    s.aged.age = 0.5;
    s.shield =
        EeGfxEffectShield { scroll: 0.25, opacity: 0.75, hit_points: 9, scrolling_down: true, ..Default::default() };
    let mut m = Mem::default();
    assert_eq!(save_shield(&s, &mut m), 0);
    assert_eq!(m.0.len(), 5);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHIELD);
    assert_eq!(load_shield(&mut back, &mut Mem(m.0.clone(), 0), 0, 0xb4), 0);
    assert_eq!((back.aged.age, back.shield), (0.5, s.shield));
    // before 0.180 the direction was not saved
    let mut old = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SHIELD);
    let mut f = Mem(m.0[..4].to_vec(), 0);
    assert_eq!(load_shield(&mut old, &mut f, 0, 0xb3), 0);
    assert!(!old.shield.scrolling_down && f.1 == 4);
}
