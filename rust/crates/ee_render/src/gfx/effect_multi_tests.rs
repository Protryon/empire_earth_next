//! `effect_multi.rs`'s tests: the multi-effect's virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::effect_state::EffectFile;
use crate::gfx::raster::{EeGfxModel, LogRaster};
use ee_base::math::types::{Ge3DPoint, GeVector};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
}

impl Effect3DHost for Host {
    fn unit_position(&mut self) -> Ge3DPoint {
        self.calls.push("position".into());
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
        self.calls.push("base".into());
        0
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
    fn beam_unit(&mut self) -> EeGfxBeamUnit {
        EeGfxBeamUnit::default()
    }
    fn beam_hidden(&mut self) -> bool {
        false
    }
    fn place_linked(&mut self, _point: &Ge3DPoint) {}
    fn create_linked(&mut self, index: u32) -> i32 {
        self.calls.push(format!("linked {index}"));
        10
    }
}

/// A file that fails every call (nothing may be read or written).
struct NoFile;

impl EffectFile for NoFile {
    fn is_open(&mut self) -> bool {
        panic!("open")
    }
    fn read_u32(&mut self, _v: &mut u32) -> i32 {
        panic!("read")
    }
    fn read_i32(&mut self, _v: &mut i32) -> i32 {
        panic!("read")
    }
    fn read_f32(&mut self, _v: &mut f32) -> i32 {
        panic!("read")
    }
    fn read_bool(&mut self, _v: &mut bool) -> i32 {
        panic!("read")
    }
    fn write_u32(&mut self, _v: u32) -> i32 {
        panic!("write")
    }
    fn write_i32(&mut self, _v: i32) -> i32 {
        panic!("write")
    }
    fn write_f32(&mut self, _v: f32) -> i32 {
        panic!("write")
    }
    fn write_bool(&mut self, _v: bool) -> i32 {
        panic!("write")
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, Vec<String>) {
    let mut x = LogRaster::default();
    // SAFETY: a live record, no other reference during the call
    let res = unsafe { effect3d(op, r, g, h, &mut x) };
    (res, x.calls)
}

#[test]
fn the_multi_effect_starts_its_linked_effects_through_every_entry_point() {
    for op in [EeGfxEffect3DOp::Create, EeGfxEffect3DOp::CreateAt, EeGfxEffect3DOp::CreateLine] {
        for third in [false, true] {
            let mut h = Host::default();
            let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MULTI_EFFECT);
            let g = EeGfxGraphicFacts { has_linked3: third, has_linked1: false, ..Default::default() };
            let (res, draws) = run(op, &mut r, &g, &mut h);
            // the creations' results are ignored
            assert_eq!(res, 0);
            assert!(draws.is_empty());
            let want: Vec<String> =
                if third { vec!["linked 1", "linked 2", "linked 3"] } else { vec!["linked 1", "linked 2"] }
                    .into_iter()
                    .map(String::from)
                    .collect();
            assert_eq!(h.calls, want);
            assert_eq!(r, EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MULTI_EFFECT));
        }
    }
}

#[test]
fn the_multi_effect_is_never_queued_or_drawn_and_ends_at_its_first_step() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MULTI_EFFECT);
    let g = EeGfxGraphicFacts { has_linked3: true, ..Default::default() };
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h).0, 0);
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h).0, 10);
    for op in [
        EeGfxEffect3DOp::DrawPass2,
        EeGfxEffect3DOp::DrawPass3,
        EeGfxEffect3DOp::DrawPass4,
        EeGfxEffect3DOp::DrawPass5,
        EeGfxEffect3DOp::DrawPass6,
        EeGfxEffect3DOp::Repair,
    ] {
        assert_eq!(run(op, &mut r, &g, &mut h), (0, vec![]));
    }
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 7);
    assert!(h.calls.is_empty());
    assert_eq!(r, EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MULTI_EFFECT));
}

#[test]
fn the_multi_effect_loads_and_saves_only_the_base() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MULTI_EFFECT);
    for base in [0, 5] {
        assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, base, &mut r, &mut NoFile, 0, 268, &mut h), base);
        assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, base, &mut r, &mut NoFile, 0, 268, &mut h), base);
    }
    assert!(h.calls.is_empty());
}
