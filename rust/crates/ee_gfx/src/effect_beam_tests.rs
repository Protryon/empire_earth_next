//! `effect_beam.rs`'s tests: the citizen beam's virtuals over a mock host and the logging rasterizer, its Load and
//! Save.

use super::*;
use crate::effect_rising::effect3d;
use crate::raster::{EeGfxModel, LogRaster};
use ee_ge::types::GeVector;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    unit: EeGfxBeamUnit,
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
    fn follow_unit(&mut self) {}
    fn model_at(&mut self, _index: u32) -> EeGfxModel {
        std::ptr::null_mut()
    }
    fn shot_hit(&mut self) {}
    fn beam_start(&mut self) -> Ge3DPoint {
        self.calls.push("start".into());
        Ge3DPoint { x: 1.0, y: 1.0, z: 1.0 }
    }
    fn beam_unit(&mut self) -> EeGfxBeamUnit {
        self.unit
    }
    fn beam_hidden(&mut self) -> bool {
        self.calls.push("hidden?".into());
        self.hidden
    }
    fn place_linked(&mut self, p: &Ge3DPoint) {
        self.calls.push(format!("place {} {} {}", p.x, p.y, p.z));
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        param1: 3.0,
        has_object: true,
        alpha_ref: 0.5,
        int_param1: 4,
        int_param2: 10,
        int_param3: 20,
        game_time_bits: 1234,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster { fog: 1, fog_value: 0.5, ..Default::default() };
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(op, r, g, h, &mut x) };
    (result, x)
}

fn made(h: &mut Host) -> EeGfxEffectRecord {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_CITIZEN_BEAM);
    r.shot.end = Ge3DPoint { x: 1.0, y: 5.0, z: -2.0 };
    r.shot.active = true;
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), h);
    r
}

#[test]
fn made_it_is_off_with_its_end_raised() {
    let mut h = Host::default();
    let mut r = made(&mut h);
    assert_eq!((r.shot.end, r.shot.active), (Ge3DPoint { x: 1.0, y: 5.0, z: 1.0 }, false));
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 0);
    assert_eq!((r.bounding_radius, h.calls.as_slice()), (2.0, &["media".to_string()][..]));
}

#[test]
fn it_turns_on_in_the_animation_window_aiming_once() {
    let mut h = Host { unit: EeGfxBeamUnit { complex: true, animation: 4, frame: 10 }, ..Default::default() };
    let mut r = made(&mut h);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    // from (1, 1, 1) to (1, 5, 1): a quarter out at y 1.25, the middle at 1.5, the line unnormalized
    assert_eq!(h.calls, ["hidden?", "start", "move", "place 1 1.5 1"]);
    assert_eq!((r.shot.path.direction, r.transformation.m[1][3]), (GeVector { x: 0.0, y: 4.0, z: 0.0 }, 1.25));
    // 1234 & 7 = 2: 1 - 0.06
    assert!(r.shot.active);
    assert_eq!(r.shot.scale, (1.0 - 2.0 * 0.03) * 2.0);
    // already on: no new aim
    h.calls.clear();
    run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    assert_eq!(h.calls, ["hidden?"]);
    // out of the window (frame 20, another animation, not complex) it goes off and keeps its scale
    for u in [
        EeGfxBeamUnit { complex: true, animation: 4, frame: 20 },
        EeGfxBeamUnit { complex: true, animation: 3, frame: 12 },
        EeGfxBeamUnit { complex: false, animation: 4, frame: 12 },
    ] {
        h.unit = u;
        r.shot.active = true;
        run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
        assert!(!r.shot.active);
    }
    // a hidden enemy's: off, not aimed
    h.unit = EeGfxBeamUnit { complex: true, animation: 4, frame: 19 };
    h.hidden = true;
    h.calls.clear();
    run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    assert_eq!((r.shot.active, h.calls.as_slice()), (false, &["hidden?".to_string()][..]));
}

#[test]
fn queued_on_it_draws_additively() {
    let mut h = Host::default();
    let mut r = made(&mut h);
    r.transformation.m[0][3] = 7.0;
    r.shot.scale = 1.5;
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h);
    assert_eq!((r.shot.draw_active, r.shot.location.x), (false, 7.0));
    assert_eq!(run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h).1.calls.len(), 0);
    r.shot.active = true;
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h);
    let (_, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "zbuffer 2",
            "shading 0",
            "blend 1 1",
            "lighting White",
            "own texture",
            "fogfactor 7 0 0 1",
            "transform 7 0 0",
            "scale 1.5",
            "ambient 0.5 0.5 0.5",
            "prepare",
            "draw 1",
            "fog 1"
        ]
    );
    assert_eq!(h.calls.last().map(String::as_str), Some("queue 4"));
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
        let r = self.read_u32(&mut u);
        *v = u as i32;
        r
    }
    fn read_f32(&mut self, v: &mut f32) -> i32 {
        let mut u = 0;
        let r = self.read_u32(&mut u);
        *v = f32::from_bits(u);
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
fn it_saves_and_loads_its_end_line_middle_and_flag() {
    let mut h = Host { unit: EeGfxBeamUnit { complex: true, animation: 4, frame: 10 }, ..Default::default() };
    let mut r = made(&mut h);
    run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h);
    let mut m = Mem::default();
    assert_eq!(save_beam(&r, &mut m, 0), 0);
    assert_eq!(m.0.len(), 1 + 3 + 6 + 3 + 1);
    // the base's error stops the save
    assert_eq!(save_beam(&r, &mut Mem::default(), 4), 4);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_CITIZEN_BEAM);
    assert_eq!(load_beam(&mut back, &mut Mem(m.0.clone(), 0), 0), 0);
    let (s, t) = (back.shot, r.shot);
    assert_eq!((s.end, s.path, s.middle, s.active), (t.end, t.path, t.middle, true));
    // the media's error stops the load before any read
    let mut f = Mem(m.0, 0);
    assert_eq!(load_beam(&mut back, &mut f, 3), 3);
    assert_eq!(f.1, 0);
}
