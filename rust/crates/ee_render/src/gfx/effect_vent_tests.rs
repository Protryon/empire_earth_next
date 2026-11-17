//! `effect_vent.rs`'s tests: the damage vent's virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::effect_state::EffectFile;
use crate::gfx::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
    unit: EeGfxVentUnit,
    count: u32,
    frames: EeGfxFrames,
}

impl Effect3DHost for Host {
    fn unit_position(&mut self) -> Ge3DPoint {
        self.calls.push("position".into());
        Ge3DPoint { x: 10.0, y: 20.0, z: 3.0 }
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
        self.calls.push("model".into());
        std::ptr::null_mut()
    }
    fn texture(&mut self) -> EeGfxModel {
        self.calls.push("texture".into());
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
    fn place_linked(&mut self, p: &Ge3DPoint) {
        self.calls.push(format!("place {} {} {}", p.x, p.y, p.z));
    }
    fn unit_location(&mut self) -> GeTransformation {
        self.calls.push("location".into());
        let mut t = identity();
        t.m[0][3] = 100.0;
        t
    }
    fn vent_lists(&mut self) {
        self.calls.push("lists".into());
    }
    fn vent_unit(&mut self) -> EeGfxVentUnit {
        self.unit
    }
    fn vent_count(&mut self) -> u32 {
        self.calls.push("count".into());
        self.count
    }
    fn vent_offsets(&mut self, tag: i32) -> (Ge3DPoint, Ge3DPoint) {
        self.calls.push(format!("offsets {tag}"));
        (Ge3DPoint { x: 0.0, y: 0.0, z: 0.0 }, Ge3DPoint { x: 0.0, y: 0.0, z: 1.0 })
    }
    fn vent_points(&mut self, tag: i32) -> EeGfxVentPoints {
        self.calls.push(format!("points {tag}"));
        EeGfxVentPoints {
            from: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
            to: Ge3DPoint { x: 1.0, y: 2.0, z: 5.0 },
            translation: GeVector::new(0.5, 0.25, 0.0),
        }
    }
    fn frames(&mut self, index: u32) -> EeGfxFrames {
        self.calls.push(format!("frames {index}"));
        self.frames
    }
    fn unit_class_scale(&mut self) -> f32 {
        self.calls.push("class scale".into());
        1.5
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        start_size: 1.0,
        param2: 0.5,
        param3: 1.0,
        param4: 1.0,
        param5: 10.0,
        param6: 100.0,
        param7: 100.0,
        param8: 0.0,
        tick_time: 50.0,
        has_unit: true,
        unit_hit_points: 25,
        has_linked1: true,
        has_object: true,
        alpha_ref: 0.25,
        ..Default::default()
    }
}

fn host() -> Host {
    Host {
        unit: EeGfxVentUnit { dead: false, max_hit_points: 100.0 },
        count: 4,
        frames: EeGfxFrames { start: 0, time: 40, count: 3, last: 2 },
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, Vec<String>) {
    let mut x = LogRaster::default();
    // SAFETY: a live record, no other reference during the call
    let res = unsafe { effect3d(op, r, g, h, &mut x) };
    (res, x.calls)
}

fn jets(r: &mut EeGfxEffectRecord) -> Vec<EeGfxVentPart> {
    // SAFETY: a live record
    unsafe { parts(r) }.clone()
}

#[test]
fn the_vent_ends_without_a_unit_or_once_dead_or_healed() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_VENT);
    let mut h = host();
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { has_unit: false, ..facts() }, &mut h).0, 7);
    assert!(h.calls.is_empty());
    h.unit.dead = true;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 7);
    h.unit.dead = false;
    // 50 of 100: not below param 2
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { unit_hit_points: 50, ..facts() }, &mut h).0, 7);
    assert_eq!(h.calls, ["lists", "lists"]);
    assert!(jets(&mut r).is_empty());
}

#[test]
fn the_vent_adds_a_jet_every_two_seconds_up_to_the_damage() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_VENT);
    let mut h = host();
    let g = EeGfxGraphicFacts { has_linked1: false, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 0);
    // follows the unit: its position, no orientation
    assert_eq!((r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]), (10.0, 20.0, 3.0));
    assert_eq!(r.transformation.m[0][0], 1.0);
    assert_eq!(r.vent.next_count_time, 2000.0);
    let first = jets(&mut r);
    assert_eq!(first.len(), 1);
    let tag = first[0].tag;
    assert!((0..4).contains(&tag));
    assert_eq!(first[0].speed, 1.0);
    assert_eq!(
        h.calls[..6],
        ["lists", "position", "move", "count", format!("offsets {tag}").as_str(), "frames 0"].map(String::from)
    );
    // grew by param 5 x 50 ms / 1000 towards (0.5 - 0.25) / 0.5 x (2 - 1) + 1 = 1.5
    assert_eq!(first[0].length, 0.5);
    // the jet stands at the location moved along it (length 0.5 up) and by the vent's offset
    assert_eq!(first[0].transformation.m[0][3], 100.5);
    assert_eq!(first[0].transformation.m[1][3], 0.25);
    assert_eq!(first[0].transformation.m[2][3], 0.5);
    // its first frame came at once (no time left): frame 1, 40 ms
    assert_eq!((first[0].frame, first[0].time), (1, 40.0));
    // the next steps until the 2 s are up add none
    for _ in 0..39 {
        run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h);
    }
    assert_eq!(jets(&mut r).len(), 1);
    assert_eq!(jets(&mut r)[0].length, 1.5);
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h);
    let both = jets(&mut r);
    assert_eq!(both.len(), 2);
    assert_ne!(both[0].tag, both[1].tag);
    // 0.25 of 0.5 below: floor(0.5 x 4 / 0.5) + 1 = 3 wanted; at 45 hit points 1 wanted: one jet dropped at once
    let g45 = EeGfxGraphicFacts { unit_hit_points: 45, ..g };
    for _ in 0..40 {
        run(EeGfxEffect3DOp::Step, &mut r, &g45, &mut h);
    }
    assert_eq!(jets(&mut r).len(), 1);
}

#[test]
fn the_jets_animate_and_place_their_sub_effects() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_VENT);
    let mut h = host();
    let g = facts();
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h);
    // the first sub-effect at once at the vent (no spread), the next after 100 ms
    assert_eq!(h.calls.iter().filter(|c| c.starts_with("place")).collect::<Vec<_>>(), ["place 1 2 3"]);
    h.calls.clear();
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h);
    assert!(!h.calls.iter().any(|c| c.starts_with("place")));
    run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h);
    assert!(h.calls.iter().any(|c| c.starts_with("place")));
    // the frames wrap at the count
    let frames: Vec<u32> = (0..6)
        .map(|_| {
            run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h);
            jets(&mut r)[0].frame
        })
        .collect();
    assert!(frames.contains(&0) && frames.iter().all(|&f| f < 3));
    // flag 1: aimed again every step
    h.calls.clear();
    run(EeGfxEffect3DOp::Step, &mut r, &EeGfxGraphicFacts { flag1: true, ..g }, &mut h);
    assert!(h.calls.iter().any(|c| c.starts_with("offsets")));
}

#[test]
fn the_vent_queues_and_draws_its_jets() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_VENT);
    let mut h = host();
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 3);
    assert_eq!(r.bounding_radius, 3.5);
    run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    h.queue_result = 10;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 10);
    assert_eq!(r.vent.draw_count, 0);
    h.queue_result = 0;
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["queue 4"]);
    let jet = jets(&mut r)[0];
    assert_eq!((jet.draw_length, jet.draw_frame, r.vent.draw_count), (jet.length, jet.frame, 1));
    let (_, calls) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!(calls[..6], ["alpharef 0.25", "zbuffer 2", "shading 0", "blend 4 5", "lighting White", "own texture"]);
    assert!(calls.iter().any(|c| c.starts_with("frame")));
    assert_eq!(calls.last().unwrap(), "fog 0");
    let (_, additive) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &EeGfxGraphicFacts { flag2: true, ..facts() }, &mut h);
    assert!(additive.contains(&"blend 1 1".to_string()));
    assert!(additive.iter().any(|c| c.starts_with("fogfactor")));
    for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass5] {
        assert_eq!(run(op, &mut r, &facts(), &mut h), (0, vec![]));
    }
}

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

#[test]
fn the_vent_loads_its_media_and_saves_the_base() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DAMAGE_VENT);
    let mut h = host();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 5, &mut r, &mut NoFile, 0, 268, &mut h), 3);
    assert_eq!(h.calls, ["media"]);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 5, &mut r, &mut NoFile, 0, 268, &mut h), 5);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut NoFile, 0, 268, &mut h), 0);
}
