//! `effect_icon.rs`'s virtuals: the icon and the goal over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_orbit::{EeGfxAnimation, EeGfxOrbital};
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::raster::{EeGfxModel, LogRaster};
use ee_base::math::types::GeVector;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    base: i32,
    market: bool,
    media: i32,
    orbitals: u32,
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
        self.calls.push("base".into());
        self.base
    }
    fn queue(&mut self, _index: u32) {
        panic!("queues by the preferences' screen")
    }
    fn load_media(&mut self) -> i32 {
        self.calls.push("media".into());
        self.media
    }
    fn model(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn texture(&mut self) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn follow_unit(&mut self) {}
    fn model_at(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("model {index}"));
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
    fn orbital_count(&mut self) -> u32 {
        self.orbitals
    }
    fn orbital(&mut self, index: u32) -> EeGfxOrbital {
        EeGfxOrbital { model: std::ptr::null_mut(), speed: 1.0 + index as f32, angle: 0.5 }
    }
    fn animation(&mut self) -> EeGfxAnimation {
        EeGfxAnimation { start: 2, frame_time: 40, frame_count: 5 }
    }
    fn reload_media(&mut self) -> i32 {
        self.calls.push("reload".into());
        self.media
    }
    fn unit_raised_position(&mut self) -> Ge3DPoint {
        self.calls.push("raised".into());
        Ge3DPoint { x: 10.0, y: 20.0, z: 3.0 }
    }
    fn market_unbuilt(&mut self) -> bool {
        self.market
    }
    fn queue_screen(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn texture_at(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("texture {index}"));
        std::ptr::null_mut()
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        start_size: 1.0,
        param1: 0.5,
        param2: -0.5,
        param3: 1.0,
        param7: 0.25,
        param8: 1.0,
        param9: 0.5,
        param10: 0.0,
        tick_seconds: 0.1,
        tick_time: 100.0,
        speed: 0.1,
        has_object: true,
        alpha_ref: 0.75,
        has_effect: true,
        ..EeGfxGraphicFacts::default()
    }
}

fn run(
    op: EeGfxEffect3DOp,
    r: &mut Box<EeGfxEffectRecord>,
    g: &EeGfxGraphicFacts,
    h: &mut Host,
    x: &mut LogRaster,
) -> i32 {
    // SAFETY: the record is live and only reached through this pointer during the call
    unsafe { effect3d(op, &mut **r, g, h, x) }
}

fn record(kind: i32) -> Box<EeGfxEffectRecord> {
    Box::new(EeGfxEffectRecord::new(kind))
}

#[test]
fn created_on_a_unit_follows_it_and_at_a_point_stays() {
    let (mut h, mut x) = (Host::default(), LogRaster::default());
    let mut on = record(EE_GFX_EFFECT_KIND_ICON);
    assert_eq!(run(EeGfxEffect3DOp::Create, &mut on, &facts(), &mut h, &mut x), 0);
    assert!(on.icon.follow_unit);
    let mut at = record(EE_GFX_EFFECT_KIND_GOAL);
    at.aged.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    run(EeGfxEffect3DOp::CreateAt, &mut at, &facts(), &mut h, &mut x);
    assert!(!at.icon.follow_unit);
    assert_eq!(at.icon.location, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
    assert_eq!(at.aged.location, Ge3DPoint::default());
    assert!(h.calls.is_empty());
    // the draw's transformation starts as the identity (`GETransformation()`)
    assert_eq!(at.icon.draw_transformation, identity());
}

#[test]
fn set_up_restarts_the_animation_copies_the_orbitals_and_places_it() {
    let (mut h, mut x) = (Host { orbitals: 2, media: 5, ..Host::default() }, LogRaster::default());
    let mut r = record(EE_GFX_EFFECT_KIND_ICON);
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h, &mut x);
    r.icon.age = 30.0;
    r.icon.time = 7.0;
    // the media's result is ignored
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h, &mut x), 0);
    assert_eq!(h.calls, ["reload", "raised", "move"]);
    assert_eq!((r.icon.age, r.icon.time, r.icon.frame, r.icon.frame_count, r.icon.frame_time), (0.0, 0.0, 2, 5, 40));
    assert_eq!((r.orbit.count, r.bounding_radius, r.icon.scale), (2, 2.0, 2.0));
    assert_eq!((r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]), (10.5, 19.5, 4.0));
    // a second set-up drops the first copies
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h, &mut x);
    // SAFETY: made by the set-up, owned by the record
    assert_eq!(unsafe { (*r.orbit.orbitals).list.len() }, 2);
    // at a point: its location, no unit asked
    let mut p = record(EE_GFX_EFFECT_KIND_ICON);
    p.aged.location = Ge3DPoint { x: 1.0, y: 1.0, z: 1.0 };
    run(EeGfxEffect3DOp::CreateAt, &mut p, &facts(), &mut h, &mut x);
    h.calls.clear();
    run(EeGfxEffect3DOp::SetUp, &mut p, &facts(), &mut h, &mut x);
    assert_eq!(h.calls, ["reload", "move"]);
    assert_eq!(p.transformation.m[0][3], 1.5);
    // SAFETY: owned by the records
    unsafe {
        crate::gfx::ffi_effect::ee_gfx_effect_record_free(Box::into_raw(r));
        crate::gfx::ffi_effect::ee_gfx_effect_record_free(Box::into_raw(p));
    }
}

#[test]
fn queue_skips_an_unbuilt_market_and_keeps_the_draw_copies() {
    let (mut h, mut x) = (Host { orbitals: 1, ..Host::default() }, LogRaster::default());
    let mut r = record(EE_GFX_EFFECT_KIND_ICON);
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h, &mut x);
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h, &mut x);
    h.calls.clear();
    h.base = 10;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h, &mut x), 10);
    // flag 2: no base queue
    let g = EeGfxGraphicFacts { flag2: true, ..facts() };
    h.market = true;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 10);
    assert_eq!(h.calls, ["base"]);
    h.market = false;
    r.icon.frame = 4;
    r.icon.scroll = 0.25;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(h.calls, ["base", "queue 4"]);
    assert_eq!((r.icon.draw_frame, r.icon.draw_scroll, r.icon.draw_transformation), (4, 0.25, r.transformation));
    // SAFETY: owned by the record
    unsafe {
        assert_eq!(&(*r.orbit.orbitals).angles, &[0.5]);
        crate::gfx::ffi_effect::ee_gfx_effect_record_free(Box::into_raw(r));
    }
}

#[test]
fn step_ages_turns_the_orbitals_and_follows() {
    let (mut h, mut x) = (Host { orbitals: 1, ..Host::default() }, LogRaster::default());
    let mut r = record(EE_GFX_EFFECT_KIND_ICON);
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h, &mut x);
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h, &mut x);
    h.calls.clear();
    let g = EeGfxGraphicFacts { duration: 150, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    assert_eq!(h.calls, ["raised", "move", "raised", "move"]);
    // shrinking from the size by param 7 a step; the radius follows the scale
    assert_eq!((r.icon.scale, r.bounding_radius), (1.5, 1.5));
    // SAFETY: owned by the record
    unsafe {
        let o = &*r.orbit.orbitals;
        assert_eq!(o.list[0].angle, 0.5 + 0.1 + 0.1);
        crate::gfx::ffi_effect::ee_gfx_effect_record_free(Box::into_raw(r));
    }
}

#[test]
fn pass_6_draws_the_tweened_model_and_restores_the_fog() {
    let (mut h, mut x) = (Host { orbitals: 1, ..Host::default() }, LogRaster::default());
    let mut r = record(EE_GFX_EFFECT_KIND_ICON);
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h, &mut x);
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h, &mut x);
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h, &mut x);
    r.icon.draw_frame = 4;
    h.calls.clear();
    let g = EeGfxGraphicFacts { int_param1: 3, int_param2: 1, ..facts() };
    x.calls.clear();
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert_eq!(h.calls, ["model 3", "texture 0"]);
    let calls = x.calls.join("|");
    // one-one blending, white lit and dimmed by the fog, test-only z; the next frame wraps (4 + 1) % 5
    for part in ["blend 1 1", "lighting White", "zbuffer 2", "tween 0 0", "scroll u", "restore u"] {
        assert!(calls.contains(part), "{part} in {calls}");
    }
    assert!(calls.find("draw").unwrap() < calls.find("restore u").unwrap());
    // flag 1: alpha blended, prelit, z writes; no scroll
    x.calls.clear();
    let g = EeGfxGraphicFacts { flag1: true, ..facts() };
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    let calls = x.calls.join("|");
    assert!(calls.contains("blend 4 5") && calls.contains("lighting Prelit") && calls.contains("zbuffer 3"), "{calls}");
    assert!(!calls.contains("scroll u"));
    // no frames: the next is 0, not a fault
    r.icon.frame_count = 0;
    x.calls.clear();
    run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert!(x.calls.join("|").contains("tween 0 "));
    // SAFETY: owned by the record
    unsafe { crate::gfx::ffi_effect::ee_gfx_effect_record_free(Box::into_raw(r)) };
}

#[test]
fn the_goal_labels_its_time_left_and_the_icon_does_not() {
    let (mut h, mut x) = (Host::default(), LogRaster::default());
    let mut goal = record(EE_GFX_EFFECT_KIND_GOAL);
    goal.icon.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    assert_eq!(run(EeGfxEffect3DOp::DrawPass3, &mut goal, &facts(), &mut h, &mut x), 0);
    assert!(x.calls.is_empty());
    goal.icon.time_left = 65_999;
    run(EeGfxEffect3DOp::DrawPass3, &mut goal, &facts(), &mut h, &mut x);
    assert_eq!(x.calls, ["screen 1 2 4.2", "text 10 20 1:05 color 0 0 0", "text 9 19 1:05 color 1 1 1"]);
    x.calls.clear();
    goal.icon.time_left = 600_000;
    run(EeGfxEffect3DOp::DrawPass3, &mut goal, &facts(), &mut h, &mut x);
    assert_eq!(x.calls[1], "text 10 20 10:00 color 0 0 0");
    let mut icon = record(EE_GFX_EFFECT_KIND_ICON);
    icon.icon.time_left = 1000;
    x.calls.clear();
    run(EeGfxEffect3DOp::DrawPass3, &mut icon, &facts(), &mut h, &mut x);
    assert!(x.calls.is_empty());
}

#[test]
fn repair_reloads_and_copies_the_orbitals_again() {
    let (mut h, mut x) = (Host { orbitals: 2, media: 4, ..Host::default() }, LogRaster::default());
    let mut r = record(EE_GFX_EFFECT_KIND_GOAL);
    assert_eq!(run(EeGfxEffect3DOp::Repair, &mut r, &facts(), &mut h, &mut x), 4);
    assert_eq!((r.icon.scale, r.orbit.count, r.icon.frame), (2.0, 2, 2));
    // without a graphic effect the scale stays; the copies are appended as the original's
    r.icon.scale = 0.5;
    run(EeGfxEffect3DOp::Repair, &mut r, &EeGfxGraphicFacts { has_effect: false, ..facts() }, &mut h, &mut x);
    assert_eq!(r.icon.scale, 0.5);
    // SAFETY: owned by the record
    unsafe {
        assert_eq!((*r.orbit.orbitals).list.len(), 4);
        crate::gfx::ffi_effect::ee_gfx_effect_record_free(Box::into_raw(r));
    }
}

#[derive(Default)]
struct Words {
    items: Vec<u32>,
    at: usize,
}

impl EffectFile for Words {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        let Some(&x) = self.items.get(self.at) else { return 3 };
        self.at += 1;
        *v = x;
        0
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut x = 0;
        let res = self.read_u32(&mut x);
        *v = x as i32;
        res
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut x = 0;
        let res = self.read_u32(&mut x);
        *v = f32::from_bits(x);
        res
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut x = 0;
        let res = self.read_u32(&mut x);
        *v = x != 0;
        res
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
fn the_goal_saves_its_time_left_after_the_icon() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_GOAL);
    r.icon.age = 2.5;
    r.icon.follow_unit = true;
    r.icon.time_left = 90_000;
    let mut f = Words::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut f, 0, 0, &mut h), 0);
    assert_eq!(f.items, [2.5f32.to_bits(), 1, 90_000]);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_GOAL);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut back, &mut f, 0, 0, &mut h), 0);
    assert_eq!((back.icon.age, back.icon.follow_unit, back.icon.time_left), (2.5, true, 90_000));
    // the base's error stops both
    let mut none = Words::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 3, &mut r, &mut none, 0, 0, &mut h), 3);
    assert!(none.items.is_empty());
    // the icon writes no time left
    let mut icon = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ICON);
    icon.icon.follow_unit = true;
    let mut f = Words::default();
    effect3d_file(EeGfxEffect3DOp::Save, 0, &mut icon, &mut f, 0, 0, &mut h);
    assert_eq!(f.items.len(), 2);
    assert!(h.calls.is_empty());
}
