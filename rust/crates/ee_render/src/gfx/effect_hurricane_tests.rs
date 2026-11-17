//! `effect_hurricane.rs`'s tests: the hurricane's virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::ge::types::{Ge3DPoint, GeTransformation, GeVector};
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_orbit::{EeGfxAnimation, EeGfxOrbital, EeGfxOrbitals};
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::effect_state::random_seed;
use crate::gfx::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
    orbitals: Vec<EeGfxOrbital>,
    spawns: u32,
    spawn_result: i32,
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
    fn beam_unit(&mut self) -> EeGfxBeamUnit {
        EeGfxBeamUnit::default()
    }
    fn beam_hidden(&mut self) -> bool {
        false
    }
    fn place_linked(&mut self, _point: &Ge3DPoint) {}
    fn orbital_count(&mut self) -> u32 {
        self.orbitals.len() as u32
    }
    fn orbital(&mut self, index: u32) -> EeGfxOrbital {
        self.orbitals[index as usize]
    }
    fn unit_location(&mut self) -> GeTransformation {
        self.calls.push("location".into());
        let mut t = identity();
        t.m[0][3] = 5.0;
        t.m[1][3] = 6.0;
        t.m[2][3] = 7.0;
        t
    }
    fn animation(&mut self) -> EeGfxAnimation {
        EeGfxAnimation { start: 2, frame_time: 100, frame_count: 4 }
    }
    fn animation_sound(&mut self, frame: u32, changed: bool) {
        self.calls.push(format!("sound {frame} {changed}"));
    }
    fn spawn_count(&mut self) -> u32 {
        self.spawns
    }
    fn spawn_at(&mut self, index: u32, frame: u32) -> Ge3DPoint {
        self.calls.push(format!("tag {index} {frame}"));
        Ge3DPoint { x: index as f32, y: frame as f32, z: 1.0 }
    }
    fn spawn(&mut self, point: &Ge3DPoint) -> i32 {
        self.calls.push(format!("spawn {} {} {}", point.x, point.y, point.z));
        self.spawn_result
    }
}

fn host() -> Host {
    let o = |speed| EeGfxOrbital { model: std::ptr::null_mut(), speed, angle: 0.5 };
    Host { orbitals: vec![o(1.0), o(-2.0)], ..Default::default() }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 1.5,
        duration: 1000,
        tick_seconds: 0.5,
        tick_time: 250.0,
        has_unit: true,
        unit_hit_points: 10,
        has_object: true,
        alpha_ref: 0.5,
        int_param1: 400,
        int_param2: 100,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster::default();
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(op, r, g, h, &mut x) };
    (result, x)
}

fn list(r: &EeGfxEffectRecord) -> &EeGfxOrbitals {
    // SAFETY: the record owns its orbitals
    unsafe { &*r.orbit.orbitals }
}

fn made(h: &mut Host) -> EeGfxEffectRecord {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_HURRICANE);
    assert_eq!(run(EeGfxEffect3DOp::Create, &mut r, &facts(), h).0, 0);
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), h).0, 3);
    r
}

#[test]
fn the_hurricane_sets_up_on_its_unit_at_a_tenth_of_its_size() {
    let mut h = host();
    let r = made(&mut h);
    let o = &r.orbit;
    assert_eq!((o.frame, o.time, o.spawn_time, o.draw_frame, o.scale), (2, 0.0, 0.0, 0, 0.1));
    assert_eq!((o.count, list(&r).list[1].angle, r.bounding_radius), (2, 0.5, 1.5));
    assert_eq!(h.calls, ["media", "location", "move"]);
    assert_eq!(r.transformation.m[1][3], 6.0);
}

#[test]
fn the_hurricane_grows_spawns_and_plays_its_animation() {
    let mut h = Host { spawns: 3, spawn_result: 10, ..host() };
    let mut r = made(&mut h);
    h.calls.clear();
    // the spawn timer is due at once: one tag point (one draw), the linked effect's result, a new timer (one draw)
    let g = EeGfxGraphicFacts { has_linked1: true, ..facts() };
    let seed = random_seed();
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 10);
    assert_ne!(seed, random_seed());
    assert_eq!(r.aged.age, 0.25);
    assert_eq!(r.orbit.scale, 0.1f32 + 250.0 / 2000.0);
    assert!(r.orbit.spawn_time >= 350.0 && r.orbit.spawn_time <= 450.0, "{}", r.orbit.spawn_time);
    // 2.5 frames of 100 ms round to 2, frame (2 + 2) % 4; the spawn read the old frame
    assert_eq!((r.orbit.time, r.orbit.frame), (50.0, 0));
    assert_eq!(h.calls.len(), 5);
    assert_eq!(h.calls[..2], ["location", "move"]);
    assert!(h.calls[2].starts_with("tag ") && h.calls[2].ends_with(" 2"), "{}", h.calls[2]);
    assert!(h.calls[3].starts_with("spawn "));
    assert_eq!(h.calls[4], "sound 0 true");
    // not due: no spawn, the timer runs down; without a linked effect nothing is placed
    h.calls.clear();
    let timer = r.orbit.spawn_time;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(r.orbit.spawn_time, timer - 250.0);
    assert!(!h.calls.iter().any(|c| c.starts_with("spawn")));
    // past 30% of its time: full size
    run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    assert_eq!(r.orbit.scale, 1.0);
    // a graphic without tag points of kind 6: the origin, no draw
    let mut none = Host { spawns: 0, ..host() };
    let mut q = made(&mut none);
    none.calls.clear();
    let seed = random_seed();
    run(EeGfxEffect3DOp::Step, &mut q, &g, &mut none);
    assert_eq!(none.calls[2], "spawn 0 0 0");
    // the timer's one draw
    assert_ne!(seed, random_seed());
}

#[test]
fn the_hurricane_shrinks_after_its_unit_dies() {
    let mut h = host();
    let mut r = made(&mut h);
    r.orbit.scale = 0.2;
    h.calls.clear();
    let dead = EeGfxGraphicFacts { unit_hit_points: 0, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &dead, &mut h).0, 0);
    assert_eq!(r.orbit.scale, 0.1);
    assert!(h.calls.is_empty());
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &dead, &mut h).0, 7);
    // under the limit by less than a step: down to a tenth
    r.orbit.scale = 0.15;
    run(EeGfxEffect3DOp::Step, &mut r, &dead, &mut h);
    assert_eq!(r.orbit.scale, 0.1);
}

#[test]
fn the_hurricane_queues_in_four_and_draws_with_clockwise_culling() {
    let mut h = host();
    let mut r = made(&mut h);
    r.orbit.scale = 0.5;
    r.orbit.frame = 3;
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["queue 4"]);
    assert_eq!((r.orbit.draw_frame, r.orbit.draw_scale), (3, 0.5));
    assert_eq!(r.aged.location, Ge3DPoint { x: 5.0, y: 6.0, z: 7.0 });
    assert_eq!(list(&r).angles, [0.5, 0.5]);
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!(result, 0);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "culling 1",
            "zbuffer 3",
            "blend 4 5",
            "shading 0",
            "lighting Unit",
            "transform 5 6 7",
            "frame 3",
            "scale 0.75",
            "pre 0.877545",
            "pre 0.877545",
            "own texture",
            "prepare",
            "draw 1",
            "culling 0"
        ]
    );
    // a failed base queue: nothing kept or queued
    h.calls.clear();
    h.queue_result = 10;
    r.orbit.frame = 1;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 10);
    assert_eq!((h.calls.len(), r.orbit.draw_frame), (0, 3));
    for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass5] {
        let (result, x) = run(op, &mut r, &facts(), &mut h);
        assert_eq!((result, x.calls.len()), (0, 0));
    }
}

/// A file of typed values.
#[derive(Default)]
struct Mem(Vec<u32>, usize);

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        if self.1 >= self.0.len() {
            return 9;
        }
        self.1 += 1;
        *v = self.0[self.1 - 1];
        0
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut u = 0;
        let res = self.read_u32(&mut u);
        *v = u as i32;
        res
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut u = v.to_bits();
        let res = self.read_u32(&mut u);
        *v = f32::from_bits(u);
        res
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut u = 0;
        let res = self.read_u32(&mut u);
        *v = u != 0;
        res
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
fn the_hurricane_saves_and_loads_time_frame_timer_age_and_scale() {
    let mut h = host();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_HURRICANE);
    r.orbit.time = 12.5;
    r.orbit.frame = 3;
    r.orbit.spawn_time = 40.0;
    r.aged.age = 0.5;
    r.orbit.scale = 0.75;
    let mut m = Mem::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 5, &mut r, &mut m, 0, 0, &mut h), 0);
    assert_eq!(m.0.len(), 5);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_HURRICANE);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 5, &mut back, &mut Mem(m.0.clone(), 0), 0, 0, &mut h), 0);
    assert_eq!(h.calls, ["media"]);
    let o = &back.orbit;
    assert_eq!((o.time, o.frame, o.spawn_time, back.aged.age, o.scale), (12.5, 3, 40.0, 0.5, 0.75));
    assert_eq!((o.count, list(&back).list[0].angle), (2, 0.5));
    let mut short = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_HURRICANE);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut short, &mut Mem(m.0[..4].to_vec(), 0), 0, 0, &mut h), 9);
    assert_eq!(short.orbit.time, 12.5);
}
