//! `effect_volcano.rs`'s virtuals: the volcano over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::raster::{EeGfxModel, LogRaster};
use ee_base::math::types::GeVector;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    media: i32,
}

impl Effect3DHost for Host {
    fn unit_position(&mut self) -> Ge3DPoint {
        Ge3DPoint::default()
    }
    fn model_height(&mut self) -> f32 {
        0.0
    }
    fn move_to(&mut self) {}
    fn create_line(&mut self, _s: &Ge3DPoint, _d: &GeVector, _l: u32) {}
    fn base_queue(&mut self) -> i32 {
        0
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
    fn volcano_build(&mut self, points: *mut Ge3DPoint, heights: *mut f32, count: u32) -> i32 {
        self.calls.push(format!("build {count}"));
        // SAFETY: the record's lists, `count` long
        unsafe {
            for i in 0..count as usize {
                *points.add(i) = Ge3DPoint { x: i as f32, y: 1.0, z: 2.0 };
                *heights.add(i) = 10.0;
            }
        }
        0
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        tick_time: 250.0,
        time1: 1000,
        time2: 300,
        duration: 2000,
        start_size: 0.5,
        has_object: true,
        alpha_ref: 0.25,
        ..Default::default()
    }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: a live record, not otherwise borrowed during the call
    unsafe { effect3d(op, r, &facts(), h, x) }
}

fn made() -> EeGfxEffectRecord {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_VOLCANO);
    r.aged.location = Ge3DPoint { x: 10.0, y: 20.0, z: 0.0 };
    run(EeGfxEffect3DOp::CreateAt, &mut r, &mut Host::default(), &mut LogRaster::default());
    r
}

#[test]
fn made_over_the_six_tiles_around_its_point() {
    let r = made();
    let a = r.volcano.area;
    assert_eq!((a.left, a.top, a.right, a.bottom), (7.0, 23.0, 13.0, 17.0));
    // the centre: the middle, one tile up in y
    assert_eq!((a.center.x, a.center.y), (10.0, 21.0));
    assert!(!r.volcano.grown && !r.volcano.done && r.volcano.mesh.is_null());
}

#[test]
fn set_up_builds_over_the_mesh_lists_unless_the_media_fails() {
    let mut r = made();
    let mut h = Host { media: 3, ..Default::default() };
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut LogRaster::default()), 3);
    assert_eq!(h.calls, ["media"]);
    let mut h = Host::default();
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut LogRaster::default()), 0);
    let n = sizes(6.0).vertices as usize;
    assert_eq!(h.calls, ["media".to_string(), format!("build {n}")]);
    // SAFETY: the record's mesh
    let m = unsafe { &*r.volcano.mesh };
    assert_eq!((m.points.len(), m.heights.len(), m.points[3].x), (n, n, 3.0));
    // the repair builds again
    let mut h = Host::default();
    run(EeGfxEffect3DOp::Repair, &mut r, &mut h, &mut LogRaster::default());
    assert_eq!(h.calls, [format!("build {n}")]);
}

#[test]
fn it_rises_holds_and_sinks() {
    let mut r = made();
    let mut h = Host::default();
    let mut x = LogRaster::default();
    let mut results = Vec::new();
    for _ in 0..20 {
        results.push(run(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x));
    }
    // risen after 4 ticks (1000 ms), then time left 3000 - 250 a tick; sinking below 1000; 7 below 0
    assert_eq!(results.iter().position(|&v| v == 7), Some(16));
    assert_eq!((r.volcano.time_left, r.volcano.growth), (-1000, -1.0));
    let mut s = made();
    for _ in 0..4 {
        run(EeGfxEffect3DOp::Step, &mut s, &mut h, &mut x);
    }
    assert_eq!((s.volcano.growth, s.volcano.grown, s.volcano.time_left, s.volcano.time), (1.0, true, 3000, 1000));
}

#[test]
fn the_queue_morphs_the_mesh_and_the_draw_restores_the_depth() {
    let mut r = made();
    let mut h = Host::default();
    run(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut LogRaster::default());
    let n = sizes(6.0).vertices as usize;
    let mut x = LogRaster { vertices: vec![Ge3DPoint::default(); n], ..Default::default() };
    r.volcano.growth = 0.5;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x), 0);
    // no jitter yet (time 0 <= time 2): z between the terrain's 2 and the cone's 10
    assert_eq!(x.vertices[5], Ge3DPoint { x: 0.0, y: 0.0, z: 6.0 });
    assert_eq!(&x.calls[n..], ["bounds", "normals"]);
    assert_eq!(h.calls.last().map(String::as_str), Some("queue 2"));
    // past time 2: jittered around the terrain point, the timer restarted, the centre at the last vertex's height
    r.volcano.time = 301;
    let mut x = LogRaster { vertices: vec![Ge3DPoint::default(); n], ..Default::default() };
    run(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x);
    assert_eq!((r.volcano.time, r.volcano.area.center.z), (0, 2.0));
    assert!(x.vertices.iter().enumerate().all(|(i, p)| (p.x - i as f32).abs() <= 0.25 && p.z == 6.0));
    // grown: done after this queue, then left alone
    r.volcano.growth = 1.0;
    run(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x);
    assert!(r.volcano.done);
    let mut x = LogRaster::default();
    run(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x);
    assert!(x.calls.is_empty());
    let mut x = LogRaster::default();
    assert_eq!(run(EeGfxEffect3DOp::DrawPass4, &mut r, &mut h, &mut x), 0);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.25",
            "depth 0 1",
            "zbuffer 3",
            "blend 4 5",
            "shading 1",
            "lighting Terrain",
            "draw 1",
            "depth 0 1",
            "zbuffer 3"
        ]
    );
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
fn load_and_save_ignore_the_base() {
    let mut r = made();
    r.volcano.growth = 0.5;
    r.volcano.time_left = 77;
    r.volcano.time = 12;
    r.volcano.grown = true;
    r.volcano.done = true;
    let mut f = Mem::default();
    let mut h = Host::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 5, &mut r, &mut f, 0, 0x10c, &mut h), 0);
    assert_eq!(f.items.len(), 4 + 3 + 4);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_VOLCANO);
    back.volcano.done = true;
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 5, &mut back, &mut f, 0, 0x10c, &mut h), 0);
    assert_eq!(h.calls, ["media"]);
    let mut want = r.volcano;
    want.done = false;
    want.mesh = std::ptr::null_mut();
    assert_eq!(back.volcano, want);
    // a short file: the first error
    let mut short = Mem { items: f.items[..5].to_vec(), at: 0 };
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut back, &mut short, 0, 0x10c, &mut h), 3);
}
