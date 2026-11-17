//! `effect_tracer.rs`'s virtuals: the tracer beam over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::raster::{EeGfxModel, LogRaster};
use ee_base::math::types::GeVector;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    base: i32,
    media: i32,
    target: bool,
    impact: i32,
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
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
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
    fn tracer_ends(&mut self) -> EeGfxTracerEnds {
        self.calls.push("ends".into());
        EeGfxTracerEnds {
            start: Ge3DPoint { x: 1.0, y: 0.0, z: 2.0 },
            end: Ge3DPoint { x: 1.0, y: 8.0, z: 2.0 },
            has_target: self.target,
        }
    }
    fn tracer_impact(&mut self) -> i32 {
        self.calls.push("impact".into());
        self.impact
    }
    fn tracer_hit(&mut self) {
        self.calls.push("hit".into());
    }
    fn tracer_saved_target(&mut self, id: u32) {
        self.calls.push(format!("saved {id}"));
    }
    fn tracer_target_id(&mut self) -> u32 {
        self.calls.push("id".into());
        42
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        param1: 0.01,
        param2: 0.001,
        param3: 0.05,
        param4: 0.3,
        param5: 0.5,
        param6: 0.0,
        param7: 0.25,
        duration: 1000,
        has_object: true,
        alpha_ref: 0.75,
        tick_time: 100.0,
        has_linked1: true,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster { fog: 1, ..Default::default() };
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(op, r, g, h, &mut x) };
    (result, x)
}

#[test]
fn made_it_starts_fresh_and_aims() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACER_BEAM);
    r.tracer.age = 0.75;
    r.tracer.timed = true;
    r.tracer.impact_created = true;
    r.tracer.scroll = 0.5;
    r.tracer.draw_age = 0.5;
    let mut h = Host::default();
    // a fresh beam (age 0) grows from its start: length 0 at its start point
    assert_eq!(run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["ends", "move"]);
    assert_eq!(
        (r.tracer.age, r.tracer.timed, r.tracer.impact_created, r.tracer.scroll, r.tracer.draw_age),
        (0.0, false, false, 0.0, 0.0)
    );
    assert_eq!(r.tracer.length, 0.0);
    assert_eq!([r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]], [1.0, 0.0, 2.0]);
    // grown: the whole beam, placed at its middle
    let mut h = Host::default();
    r.tracer.age = 0.5;
    run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    assert_eq!(r.tracer.length, 8.0);
    assert_eq!([r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]], [1.0, 4.0, 2.0]);
}

#[test]
fn set_up_and_queue() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACER_BEAM);
    let mut h = Host { media: 5, ..Default::default() };
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 5);
    assert_eq!((r.tracer.timed, r.tracer.width, r.bounding_radius), (true, 0.3, 2.0));
    run(EeGfxEffect3DOp::SetUp, &mut r, &EeGfxGraphicFacts { flag1: true, ..facts() }, &mut h);
    assert!(!r.tracer.timed);
    r.tracer.age = 0.25;
    r.tracer.length = 6.0;
    r.tracer.scroll = 0.125;
    let mut h = Host { base: 10, ..Default::default() };
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 10);
    assert_eq!((h.calls.len(), r.tracer.draw_length), (1, 0.0));
    h.base = 0;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["base", "base", "queue 4"]);
    assert_eq!(
        (r.tracer.draw_age, r.tracer.draw_length, r.tracer.draw_scroll, r.tracer.draw_width),
        (0.25, 6.0, 0.125, 0.3)
    );
}

#[test]
fn the_draw_stretches_the_model_and_restores_it() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACER_BEAM);
    r.transformation.m[0][3] = 3.0;
    r.transformation.m[1][3] = 4.0;
    r.tracer.draw_length = 6.0;
    r.tracer.draw_width = 0.5;
    r.tracer.draw_scroll = 0.25;
    let mut h = Host::default();
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!(result, 0);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.75",
            "zbuffer 2",
            "shading 0",
            "blend 4 5",
            "lighting White",
            "own texture",
            "transform 3 4 0",
            "stretch 6 0.5 0.25",
            "prepare",
            "draw 1",
            "fog 1",
            "unstretch"
        ]
    );
    // additive under flag 2, with the fog factor at the beam; without a graphic object the alpha reference is 0
    let g = EeGfxGraphicFacts { flag2: true, has_object: false, ..facts() };
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h);
    assert_eq!(
        &x.calls[..7],
        ["alpharef 0", "zbuffer 2", "shading 0", "blend 1 1", "lighting White", "own texture", "fogfactor 3 4 0 1"]
    );
    for op in
        [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass3, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass5]
    {
        let (result, x) = run(op, &mut r, &facts(), &mut h);
        assert_eq!((result, x.calls.len()), (0, 0));
    }
}

#[test]
fn a_timed_beam_places_its_impact_hits_its_target_and_ends() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACER_BEAM);
    r.tracer.timed = true;
    let mut h = Host { target: true, impact: 10, ..Default::default() };
    let g = EeGfxGraphicFacts { tick_time: 125.0, param5: 0.55, ..facts() };
    let mut results = Vec::new();
    for _ in 0..9 {
        results.push(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0);
    }
    // ages 0.125 .. 0.375: the impact past param 7 (its result the step's); 0.625 hits (past param 5); past 1: 7
    assert_eq!(results, [0, 0, 10, 0, 0, 0, 0, 0, 7]);
    let events: Vec<&str> = h.calls.iter().map(String::as_str).filter(|c| *c == "impact" || *c == "hit").collect();
    assert_eq!(events, ["impact", "hit"]);
    // each step aims first
    assert_eq!(&h.calls[..3], ["ends", "move", "ends"]);
    // without a target unit: no hit
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACER_BEAM);
    r.tracer.timed = true;
    let mut h = Host::default();
    for _ in 0..11 {
        run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    }
    assert!(!h.calls.iter().any(|c| c == "hit"));
}

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
fn load_and_save_through_the_host() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACER_BEAM);
    r.tracer.age = 0.5;
    r.tracer.weapon_tag_point = 2;
    let mut h = Host::default();
    let mut f = Mem::default();
    // the save stops at the base's error
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 4, &mut r, &mut f, 0, 0x10c, &mut h), 4);
    assert!(f.items.is_empty() && h.calls.is_empty());
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut f, 0, 0x10c, &mut h), 0);
    assert_eq!((f.items[0], f.items[1], f.items.len()), (42, 0.5f32.to_bits(), 10));
    // the load ignores the base's result; its target id goes to the host
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACER_BEAM);
    let mut h = Host::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 4, &mut back, &mut f, 0, 0x10c, &mut h), 0);
    assert_eq!(h.calls, ["media", "saved 42"]);
    assert_eq!((back.tracer.age, back.tracer.weapon_tag_point), (0.5, 2));
    // the media's error stops it: no id, the weapon tag point 0
    let mut h = Host { media: 6, ..Default::default() };
    f.at = 0;
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut back, &mut f, 0, 0x10c, &mut h), 6);
    assert_eq!((h.calls.len(), back.tracer.weapon_tag_point, f.at), (1, 0, 0));
}
