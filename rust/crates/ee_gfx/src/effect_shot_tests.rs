//! `effect_shot.rs`'s tests: the pulse cannon ball's, tracers' and shells' virtuals over a mock host and the logging
//! rasterizer, and their Load and Save.

use super::*;
use crate::effect_rising::effect3d;
use crate::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
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
    fn model_at(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("model {index}"));
        std::ptr::null_mut()
    }
    fn shot_hit(&mut self) {
        self.calls.push("hit".into());
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        duration: 50000,
        has_object: true,
        alpha_ref: 0.5,
        tick_time: 100.0,
        tick_seconds: 0.1,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster { fog: 1, fog_value: 0.5, ..Default::default() };
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(op, r, g, h, &mut x) };
    (result, x)
}

/// A shot made from (1, 2, 3) to `end`.
fn made(kind: i32, end: Ge3DPoint, h: &mut Host) -> EeGfxEffectRecord {
    let mut r = EeGfxEffectRecord::new(kind);
    r.shot.path.start = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    r.shot.location = end;
    r.shot.position = 9.0;
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), h);
    r
}

#[test]
fn made_it_faces_along_its_path_at_the_duration_rate() {
    let mut h = Host::default();
    let b = made(EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    let s = b.shot;
    assert_eq!(s.path.direction, GeVector { x: 0.0, y: 10.0, z: 0.0 });
    // 50000 * 0.00001 over a length of 10
    assert_eq!(s.speed, 0.5 / 10.0);
    assert_eq!((s.scale, s.glow_scale, s.position, s.location), (1.0, 0.1, 0.0, Ge3DPoint::default()));
    assert_eq!((b.bounding_radius, h.calls.as_slice()), (2.0, &["move".to_string()][..]));
    let t = b.transformation;
    assert_eq!((t.m[0][3], t.m[1][3], t.m[2][3], t.m[3]), (1.0, 2.0, 3.0, [0.0, 0.0, 0.0, 1.0]));
    // the same orientation the engine's SetOrientationYPR gives for the yaw and pitch of the direction
    let mut o = identity();
    o.set_orientation(ee_ge::transform::RotationOrder::Ypr, um_arctan2(-0.0, 1.0), um_arccos(-0.0) - 1.5707964, 0.0);
    for i in 0..3 {
        assert_eq!(t.m[i][..3], o.m[i][..3]);
    }
    // a tracer keeps its scales and its colour
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACERS);
    r.shot.color = EeGfxColor { red: 1.0, green: 0.0, blue: 0.5 };
    r.shot.location = Ge3DPoint { x: 0.0, y: 0.0, z: 4.0 };
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h);
    assert_eq!((r.shot.scale, r.shot.glow_scale, r.shot.color.blue), (0.0, 0.0, 0.5));
    assert_eq!(r.shot.speed, 0.5 / 4.0);
}

#[test]
fn set_up_and_queue() {
    let mut h = Host::default();
    let mut b = made(EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut b, &facts(), &mut h).0, 3);
    b.shot.position = 0.25;
    b.shot.scale = 1.25;
    b.shot.glow_scale = 0.5;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut b, &facts(), &mut h).0, 0);
    let s = b.shot;
    assert_eq!((s.draw_position, s.draw_scale, s.draw_glow_scale), (0.25, 1.25, 0.5));
    assert_eq!(s.location, Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 });
    h.queue_result = 10;
    b.shot.position = 0.75;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut b, &facts(), &mut h).0, 10);
    assert_eq!(b.shot.draw_position, 0.25);
    assert_eq!(h.calls[1..], ["media", "queue 4"]);
}

#[test]
fn the_ball_draws_itself_and_its_glow_additively() {
    let mut h = Host::default();
    let mut b = made(EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    b.shot.location = Ge3DPoint { x: 4.0, y: 5.0, z: 6.0 };
    b.shot.draw_scale = 1.25;
    b.shot.draw_glow_scale = 0.5;
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut b, &facts(), &mut h);
    assert_eq!(result, 0);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "zbuffer 2",
            "blend 1 1",
            "shading 0",
            "lighting White",
            "texture",
            "fogfactor 4 5 6 1",
            "transform 4 5 6",
            "scale 2.5",
            "ambient 0.5 0.5 0.5",
            "prepare",
            "draw 1",
            "transform 4 5 6",
            "scale 1",
            "texture",
            "prepare",
            "draw 1",
            "fog 1"
        ]
    );
    assert_eq!(h.calls.last().map(String::as_str), Some("model 1"));
    for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass5] {
        assert_eq!(run(op, &mut b, &facts(), &mut h).1.calls.len(), 0);
    }
}

#[test]
fn a_tracer_draws_in_colour_or_textured_dimming_past_half_way() {
    let mut h = Host::default();
    let mut r = made(EE_GFX_EFFECT_KIND_TRACERS, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    r.shot.color = EeGfxColor { red: 1.0, green: 0.25, blue: 0.0 };
    r.shot.draw_position = 0.75;
    let g = EeGfxGraphicFacts { flag1: true, has_object: false, ..facts() };
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h);
    assert_eq!(x.calls[0], "alpharef 0");
    assert_eq!(x.calls[5], "color 1 0.25 0");
    // (1 - 0.75) * 0.5 * 2
    assert_eq!(x.calls[8..], ["scale 2", "ambient 0.25 0.25 0.25", "prepare", "draw 1", "fog 1"]);
    r.shot.draw_position = 0.25;
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!((x.calls[5].as_str(), x.calls[9].as_str()), ("texture", "ambient 0.5 0.5 0.5"));
    // only the shells draw in pass 5
    assert_eq!(run(EeGfxEffect3DOp::DrawPass5, &mut r, &facts(), &mut h).1.calls.len(), 0);
}

#[test]
fn a_shell_fades_over_the_second_half_in_pass_5() {
    let mut h = Host::default();
    let mut r = made(EE_GFX_EFFECT_KIND_SHELLS, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    r.shot.location = Ge3DPoint { x: 7.0, y: 8.0, z: 9.0 };
    r.shot.draw_position = 0.75;
    let (_, x) = run(EeGfxEffect3DOp::DrawPass5, &mut r, &facts(), &mut h);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "zbuffer 2",
            "blend 4 5",
            "shading 0",
            "lighting Unit",
            "texture",
            "transform 7 8 9",
            "scale 2",
            "opacity 0.5",
            "prepare",
            "draw 1",
            "opacity 1"
        ]
    );
    r.shot.draw_position = 0.25;
    assert_eq!(run(EeGfxEffect3DOp::DrawPass5, &mut r, &facts(), &mut h).1.calls[8], "opacity 1");
    // and in pass 6 as a tracer
    assert_eq!(run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h).1.calls.len(), 13);
}

#[test]
fn the_ball_pulses_along_its_path_then_explodes() {
    let mut h = Host::default();
    let mut b = made(EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    // 100 ms at 0.05 a ms: 5 per tick is too far; slow it down to a tenth and a half per tick
    b.shot.speed = 0.0015;
    let mut seen = vec![];
    loop {
        let result = run(EeGfxEffect3DOp::Step, &mut b, &facts(), &mut h).0;
        seen.push((b.shot.scale, b.shot.glow_scale));
        if result != 0 {
            assert_eq!(result, 7);
            break;
        }
    }
    // positions 0.15, 0.3, 0.45, ... in f32 sums: 0.6 and 1.2 land just above, 1.5 just below
    assert_eq!(
        seen,
        [
            (1.25, 0.1),
            (1.0, 0.1),
            (1.0, 0.1),
            (1.25, 0.1),
            (1.25, 0.1),
            (1.0, 0.1),
            (2.0, 2.0),
            (4.0, 1.5),
            (16.0, 0.5),
            (32.0, 0.1),
            (32.0, 0.1)
        ]
    );
    // moved along the path while on it
    assert_eq!(h.calls.iter().filter(|c| *c == "move").count(), 1 + 6);
}

#[test]
fn a_tracer_hits_at_the_end() {
    let mut h = Host::default();
    let mut r = made(EE_GFX_EFFECT_KIND_TRACERS, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    r.shot.speed = 0.005;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(r.transformation.m[1][3], 2.0 + 10.0 * 0.5);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 7);
    assert_eq!(h.calls.last().map(String::as_str), Some("hit"));
    assert_eq!(r.transformation.m[1][3], 12.0);
}

/// A file of typed values.
#[derive(Default)]
struct Mem(Vec<u32>, usize);

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        if self.1 == self.0.len() {
            return 5;
        }
        self.1 += 1;
        *v = self.0[self.1 - 1];
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
        if r == 0 {
            *v = f32::from_bits(u);
        }
        r
    }
    fn read_bool(&mut self, v: &mut bool) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = u != 0;
        r
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
fn they_save_and_load_the_path_and_the_balls_scales() {
    let mut h = Host::default();
    let mut b = made(EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    b.shot.position = 0.5;
    b.shot.scale = 4.0;
    b.shot.glow_scale = 1.5;
    let mut m = Mem::default();
    assert_eq!(save_shot(&b, &mut m), 0);
    assert_eq!(m.0.len(), 2 + 6 + 2);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PULSE_CANNON_BALL);
    assert_eq!(load_shot(&mut back, &mut Mem(m.0.clone(), 0)), 0);
    let (s, t) = (back.shot, b.shot);
    assert_eq!((s.position, s.speed, s.path, s.scale, s.glow_scale), (t.position, t.speed, t.path, 4.0, 1.5));
    // a tracer's has no scales; a short file stops at the first error
    let mut r = made(EE_GFX_EFFECT_KIND_TRACERS, Ge3DPoint { x: 1.0, y: 12.0, z: 3.0 }, &mut h);
    r.shot.scale = 4.0;
    let mut m = Mem::default();
    save_shot(&r, &mut m);
    assert_eq!(m.0.len(), 8);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TRACERS);
    assert_eq!(load_shot(&mut back, &mut Mem(m.0[..5].to_vec(), 0)), 5);
    assert_eq!((back.shot.path.start, back.shot.path.direction.x), (r.shot.path.start, 0.0));
    // the media is loaded first through the host, and its result ignored
    let mut f = Mem(m.0.clone(), 0);
    let mut h = Host::default();
    let res = crate::effect_rising::effect3d_file(EeGfxEffect3DOp::Load, 4, &mut back, &mut f, 0, 0, &mut h);
    assert_eq!((res, h.calls.as_slice()), (0, &["media".to_string()][..]));
}
