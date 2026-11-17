//! The energy wall's virtuals (`effect_wall.rs` `wall`, `wall_file`) over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::effect_state::{identity, random_seed};
use crate::gfx::raster::{EeGfxModel, LogRaster};
use ee_base::math::types::{GeTransformation, GeVector};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
    gate: bool,
    open: Option<f32>,
    hidden: bool,
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
        self.queue_result
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
    fn complex_position(&mut self) -> Ge3DPoint {
        self.calls.push("position".into());
        Ge3DPoint { x: 10.5, y: 10.5, z: 1.0 }
    }
    fn unit_location(&mut self) -> GeTransformation {
        self.calls.push("location".into());
        let mut t = identity();
        t.m[0][0] = 2.0;
        t
    }
    fn unit_hidden(&mut self) -> bool {
        self.calls.push("hidden".into());
        self.hidden
    }
    fn wall_gate_posts(&mut self) -> Option<(Ge3DPoint, Ge3DPoint)> {
        self.calls.push("posts".into());
        self.gate.then_some((Ge3DPoint { x: 10.5, y: 10.5, z: 1.0 }, Ge3DPoint { x: 20.5, y: 10.5, z: 2.0 }))
    }
    fn wall_piece(&mut self, x: i32, y: i32) -> Option<f32> {
        self.calls.push(format!("piece {x} {y}"));
        match (x, y) {
            (9, 10) | (19, 10) => Some(2.0),
            _ => None,
        }
    }
    fn gate_open(&mut self) -> Option<f32> {
        self.calls.push("open".into());
        self.open
    }
    fn player_color(&mut self) -> EeGfxColor {
        self.calls.push("color".into());
        EeGfxColor { red: 0.5, green: 0.25, blue: 1.0 }
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        param1: 0.75,
        param2: 0.2,
        param3: 0.6,
        param4: 3.0,
        param5: 0.5,
        param6: 0.5,
        tick_time: 120.0,
        game_time_bits: 13,
        alpha_ref: 0.125,
        has_object: true,
        has_unit: true,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, Vec<String>) {
    let mut x = LogRaster::default();
    // SAFETY: a live record, no other reference during the call
    let res = unsafe { effect3d(op, r, g, h, &mut x) };
    (res, x.calls)
}

#[test]
fn a_wall_is_placed_on_its_unit_and_finds_its_segments_when_made() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    assert_eq!(run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls[..5], ["position", "posts", "location", "position", "move"]);
    assert_eq!(h.calls[5..], ["piece 9 10", "piece 9 9", "piece 10 9", "piece 11 9"]);
    let w = r.wall;
    assert_eq!((w.placed, w.gate, w.segments, w.timer), (true, false, 0b10, 0));
    assert_eq!(w.points[0], Ge3DPoint { x: 10.5, y: 10.5, z: 1.5 });
    assert_eq!(w.points[1], Ge3DPoint { x: 9.5, y: 10.5, z: 2.5 });
    // the location's orientation at the unit's place
    assert_eq!(r.transformation.m[0][0], 2.0);
    assert_eq!((r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]), (10.5, 10.5, 1.0));
    assert_eq!(r.bounding_radius, 2.828);
}

#[test]
fn a_gates_posts_are_its_end_parts() {
    let mut h = Host { gate: true, ..Host::default() };
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h);
    let w = r.wall;
    assert!(w.gate);
    assert_eq!((w.points[0].z, w.points[9]), (1.5, Ge3DPoint { x: 20.5, y: 10.5, z: 2.5 }));
    assert_eq!(w.segments, 0b10_0010);
    assert_eq!(h.calls.iter().filter(|c| c.starts_with("piece")).count(), 8);
}

#[test]
fn the_wall_queues_its_alpha_colour_and_frame() {
    let mut h = Host { open: Some(0.25), ..Host::default() };
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    // not placed: nothing
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h), (0, vec![]));
    assert!(h.calls.is_empty());
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h);
    h.calls.clear();
    h.queue_result = 10;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 10);
    assert_eq!(h.calls, ["base"]);
    h.queue_result = 0;
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["base", "open", "hidden", "color", "queue 4"]);
    let w = r.wall;
    assert_eq!((w.alpha, w.gate_alpha), (0.6, 0.75 * (0.6 - 0.2) + 0.2));
    assert_eq!(w.color, EeGfxColor { red: 0.5, green: 0.25, blue: 1.0 });
    // frame 13: (5, 3) x 0.14
    assert_eq!((w.offset_u, w.offset_v), (5.0 * 0.14, 3.0 * 0.14));
    // seen through a doppelganger: frame 0 once
    r.wall.seen = true;
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h);
    assert_eq!((r.wall.offset_u, r.wall.offset_v, r.wall.seen), (0.0, 0.0, false));
    // a cloaked enemy's: invisible
    h.hidden = true;
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h);
    assert_eq!((r.wall.alpha, r.wall.gate_alpha, r.wall.offset_u), (0.0, 0.0, 5.0 * 0.14));
    // a free camera: the segments again and a random frame
    h.calls.clear();
    let seed = random_seed();
    run(EeGfxEffect3DOp::Queue, &mut r, &EeGfxGraphicFacts { free_camera: true, ..facts() }, &mut h);
    assert!(h.calls.iter().any(|c| c.starts_with("piece")));
    assert_ne!(seed, random_seed());
}

#[test]
fn the_wall_draws_a_strip_to_each_segment_and_across_a_gate() {
    let mut h = Host { gate: true, open: Some(1.0), ..Host::default() };
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h);
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h);
    let (res, calls) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!(res, 0);
    assert_eq!(calls.len(), 3);
    assert!(calls[0].starts_with("strip 10.5 10.5 1.5 to 9.5 10.5 2.5 width 0.75 color 0.5 0.25 1"));
    assert!(calls[0].ends_with("size (3.0, 0.5) 0.6 1 0.125"));
    assert!(calls[1].starts_with("strip 10.5 10.5 1.5 to 20.5 10.5 2.5"));
    assert!(calls[1].ends_with("0.2 1 0.125"));
    assert!(calls[2].starts_with("strip 20.5 10.5 2.5 to 19.5 10.5 2.5"));
    r.wall.gate = false;
    assert_eq!(run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h).1.len(), 1);
    for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass5] {
        assert_eq!(run(op, &mut r, &facts(), &mut h), (0, vec![]));
    }
}

#[test]
fn an_unplaced_wall_is_placed_at_its_step_and_searches_every_500_ms() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["media"]);
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert!(r.wall.placed);
    assert_eq!(h.calls.iter().filter(|c| c.starts_with("piece")).count(), 4);
    assert_eq!(r.wall.timer, 500);
    h.calls.clear();
    for _ in 0..4 {
        run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    }
    assert!(h.calls.is_empty());
    run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    assert_eq!(h.calls.len(), 4);
}

#[derive(Default)]
struct Words(Vec<u32>, usize);

impl EffectFile for Words {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        let Some(&w) = self.0.get(self.1) else { return 3 };
        self.1 += 1;
        *v = w;
        0
    }
    fn read_i32(&mut self, v: &mut i32) -> i32 {
        let mut w = 0;
        let res = self.read_u32(&mut w);
        *v = w as i32;
        res
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut w = 0;
        let res = self.read_u32(&mut w);
        *v = f32::from_bits(w);
        res
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut w = 0;
        let res = self.read_u32(&mut w);
        *v = w != 0;
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
fn the_wall_saves_after_the_base_and_loads_by_version() {
    let mut h = Host { gate: true, ..Host::default() };
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h);
    let mut m = Words::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 4, &mut r, &mut m, 0, 268, &mut h), 4);
    assert!(m.0.is_empty());
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut m, 0, 268, &mut h), 0);
    assert_eq!(m.0.len(), 34);
    h.calls.clear();
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    // the base's result is ignored
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 4, &mut back, &mut Words(m.0.clone(), 0), 0, 268, &mut h), 0);
    assert_eq!(h.calls, ["media"]);
    assert_eq!((back.wall.points, back.wall.segments, back.wall.placed), (r.wall.points, r.wall.segments, true));
    // before 0.250: the old reads, unplaced
    let mut old = vec![0u32; 7];
    old.extend([1, 1]);
    old.extend([0u32; 54]);
    old.extend([0b11, 250]);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WALL);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut back, &mut Words(old, 0), 0, 249, &mut h), 0);
    assert_eq!((back.wall.gate, back.wall.placed, back.wall.segments, back.wall.timer), (true, false, 0b11, 250));
}
