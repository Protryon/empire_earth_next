//! `effect_quake.rs`'s virtuals: a crack over a mock host and the logging rasterizer.

use super::*;
use crate::effect_beam::EeGfxBeamUnit;
use crate::effect_rising::{effect3d, effect3d_file};
use crate::raster::LogRaster;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    space: bool,
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
        Ge3DPoint::default()
    }
    fn beam_unit(&mut self) -> EeGfxBeamUnit {
        EeGfxBeamUnit::default()
    }
    fn beam_hidden(&mut self) -> bool {
        false
    }
    fn place_linked(&mut self, _point: &Ge3DPoint) {}
    fn ground_point(&mut self, p: &Ge3DPoint) -> Ge3DPoint {
        Ge3DPoint { x: p.x, y: p.y, z: 0.5 }
    }
    fn space_tile(&mut self, _x: u32, _y: u32) -> bool {
        self.space
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts { duration: 3000, tick_time: 1000.0, ..EeGfxGraphicFacts::default() }
}

fn call(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: a live record, only referenced through the pointer during the call
    unsafe { effect3d(op, r, &facts(), h, x) }
}

/// A crack 2.5 long along y from (10, 20), set up.
fn made() -> (EeGfxEffectRecord, Host, LogRaster) {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_EARTHQUAKE);
    r.shot.path.start = Ge3DPoint { x: 10.0, y: 20.0, z: 3.0 };
    r.shot.location = Ge3DPoint { x: 10.0, y: 22.5, z: 1.0 };
    let mut h = Host::default();
    let mut x = LogRaster::default();
    assert_eq!(call(EeGfxEffect3DOp::Create, &mut r, &mut h, &mut x), 0);
    assert_eq!(call(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut x), 0);
    (r, h, x)
}

#[test]
fn the_crack_has_a_segment_per_unit_of_length() {
    let (r, _, mut x) = made();
    let q = r.quake;
    assert_eq!((q.length, q.segment_count, q.speed), (2.5, 3, 2.5 / 3000.0));
    assert!(q.growing && !q.closing);
    assert_eq!(r.bounding_radius, 2.5);
    // facing +y: the transformation's yaw is 0, placed at the start on z 0
    assert_eq!((r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]), (10.0, 20.0, 0.0));
    let m = x.model(q.model);
    assert_eq!(m.name, "earthquake model");
    assert_eq!((m.points.len(), m.triangles.len()), (20, 24));
    // the first row lies across the start on the ground, the rest wait at the origin
    assert_eq!(m.points[0], Ge3DPoint { x: -2.0, y: 0.0, z: 0.5 });
    assert_eq!(m.points[7], Ge3DPoint::default());
    assert_eq!(m.triangles[8], [10, 5, 11]);
}

#[test]
fn the_queue_reshapes_the_rows_up_to_the_front() {
    let (mut r, mut h, mut x) = made();
    r.quake.distance = 1.5;
    assert_eq!(call(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x), 0);
    let m = x.model(r.quake.model).clone();
    // the first row ends at 1, the others at the front, lifted over the ground
    assert_eq!(m.points[5], Ge3DPoint { x: -2.0, y: 1.0, z: 0.51 });
    assert_eq!(m.points[12].y, 1.5);
    assert_eq!(m.points[17].y, 1.5);
    assert!(r.quake.growing);
    h.space = true;
    call(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x);
    assert!(!r.quake.growing);
    assert_eq!(r.quake.hold_time, (2.5 - 1.5) / (2.5 / 3000.0) * 2.0 + 15000.0);
}

#[test]
fn it_grows_holds_and_closes() {
    let (mut r, mut h, mut x) = made();
    for _ in 0..3 {
        assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x), 0);
    }
    assert!(r.quake.growing);
    call(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x);
    assert!(!r.quake.growing && r.quake.hold_time == 15000.0);
    for _ in 0..16 {
        call(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x);
    }
    assert!(r.quake.closing);
    let mut steps = 0;
    while call(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x) == 0 {
        steps += 1;
    }
    assert_eq!(steps, 3);
}

#[derive(Default)]
struct Mem(Vec<u32>, usize);

impl EffectFile for Mem {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, v: &mut u32) -> i32 {
        let Some(&x) = self.0.get(self.1) else { return 3 };
        self.1 += 1;
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
fn a_saved_crack_loads_and_rebuilds() {
    let (mut r, mut h, mut x) = made();
    r.quake.distance = 1.25;
    let mut f = Mem::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut f, 1, 0, &mut h), 0);
    assert_eq!(f.0.len(), 6);
    let mut l = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_EARTHQUAKE);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut l, &mut f, 1, 0, &mut h), 0);
    assert_eq!((l.quake.distance, l.quake.length, l.quake.segment_count), (1.25, 2.5, 3));
    assert_eq!(call(EeGfxEffect3DOp::Repair, &mut l, &mut h, &mut x), 0);
    assert_eq!(l.quake.speed, r.quake.speed);
    assert!(!l.quake.model.is_null());
}
