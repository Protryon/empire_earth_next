//! `effect_orbit.rs`'s tests: the orbiting effects' virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::effect_beam::EeGfxBeamUnit;
use crate::effect_rising::{effect3d, effect3d_file};
use crate::effect_state::random_seed;
use crate::raster::LogRaster;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
    orbitals: Vec<EeGfxOrbital>,
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
    fn follow_unit(&mut self) {
        self.calls.push("follow".into());
    }
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
        has_object: true,
        alpha_ref: 0.5,
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

#[test]
fn an_orbital_turns_at_its_speed_wrapped_to_pi() {
    let mut o = EeGfxOrbital { model: std::ptr::null_mut(), speed: 2.0, angle: 3.0 };
    o.update(0.5);
    assert_eq!(o.angle, 4.0 - TWO_PI);
    o.speed = -20.0;
    o.update(1.0);
    assert_eq!(o.angle, ((((4.0 - TWO_PI) - 20.0) + TWO_PI + TWO_PI) + TWO_PI) + TWO_PI);
}

fn projectile(h: &mut Host) -> EeGfxEffectRecord {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE);
    r.shot.path.start = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    r.shot.location = Ge3DPoint { x: 5.0, y: 2.0, z: 1.0 };
    run(EeGfxEffect3DOp::Create, &mut r, &facts(), h);
    r
}

#[test]
fn the_projectile_flies_from_one_point_to_the_other() {
    let mut h = host();
    let seed = random_seed();
    let mut r = projectile(&mut h);
    assert_ne!(seed, random_seed());
    assert_eq!(r.shot.path.direction, GeVector { x: 4.0, y: 0.0, z: -2.0 });
    assert_eq!((r.shot.location, r.bounding_radius), (Ge3DPoint::default(), 1.5));
    assert_eq!((r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]), (1.0, 2.0, 3.0));
    // about one duration: 1000 / (d +- d / 8) / 1000 per ms
    assert!(r.shot.speed > 1.0 / 1125.0 - 1e-9 && r.shot.speed < 1.0 / 875.0 + 1e-9, "{}", r.shot.speed);
    // the set-up copies the graphic's orbitals at random angles
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 3);
    assert_eq!(r.orbit.count, 2);
    let o = list(&r);
    assert_eq!((o.list.len(), o.angles.as_slice()), (2, [0.0, 0.0].as_slice()));
    assert!(o.list.iter().all(|o| o.angle >= 0.0 && o.angle < TWO_PI && o.angle != 0.5));
    // a step turns them and moves along the path
    r.shot.speed = 0.002;
    let before = list(&r).list[0].angle;
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(r.shot.position, 0.5);
    assert_eq!((r.transformation.m[0][3], r.transformation.m[2][3]), (3.0, 2.0));
    let mut turned = EeGfxOrbital { model: std::ptr::null_mut(), speed: 1.0, angle: before };
    turned.update(0.5);
    assert_eq!(list(&r).list[0].angle, turned.angle);
    // the queue keeps the position, the place and the angles
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    assert_eq!((r.shot.draw_position, r.shot.location), (0.5, Ge3DPoint { x: 3.0, y: 2.0, z: 2.0 }));
    assert_eq!(list(&r).angles[0], turned.angle);
    assert_eq!(h.calls, ["move", "queue 3"]);
    // past the end: 7 at 1, unmoved
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 7);
    assert_eq!((r.shot.position, r.transformation.m[0][3]), (1.0, 5.0));
}

#[test]
fn the_projectile_draws_its_orbitals_and_fades_at_the_end() {
    let mut h = host();
    let mut r = projectile(&mut h);
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h);
    r.shot.location = Ge3DPoint { x: 7.0, y: 8.0, z: 9.0 };
    r.shot.draw_position = 0.5;
    let (result, x) = run(EeGfxEffect3DOp::DrawPass5, &mut r, &facts(), &mut h);
    assert_eq!(result, 0);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "zbuffer 2",
            "blend 4 5",
            "lighting Prelit",
            "transform 7 8 9",
            "shading 0",
            "scale 1.5",
            "pre 1",
            "pre 1",
            "own texture",
            "prepare",
            "draw 1"
        ]
    );
    r.shot.draw_position = 0.9;
    let (_, x) = run(EeGfxEffect3DOp::DrawPass5, &mut r, &facts(), &mut h);
    assert_eq!(x.calls[11..], [format!("opacity {}", (1.0f32 - 0.9) * 5.0), "draw 1".into(), "opacity 1".into()]);
    for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass6] {
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
fn the_projectile_saves_and_loads_its_way_and_turns_new_orbitals() {
    let mut h = host();
    let mut r = projectile(&mut h);
    r.shot.position = 0.25;
    let mut m = Mem::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 5, &mut r, &mut m, 0, 0, &mut h), 0);
    assert_eq!(m.0.len(), 8);
    let mut back = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE);
    h.calls.clear();
    let seed = random_seed();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 5, &mut back, &mut Mem(m.0.clone(), 0), 0, 0, &mut h), 0);
    assert_ne!(seed, random_seed());
    assert_eq!(h.calls, ["media"]);
    assert_eq!((back.shot.position, back.shot.speed, back.shot.path), (0.25, r.shot.speed, r.shot.path));
    assert_eq!((back.orbit.count, list(&back).list.len()), (2, 2));
    // a short file: the first error
    let mut short = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_LINEAR_PROJECTILE);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut short, &mut Mem(m.0[..3].to_vec(), 0), 0, 0, &mut h), 9);
    // the record frees its orbitals
    for record in [back, short] {
        // SAFETY: a record of `ee_gfx_effect_record_new`'s kind, freed once
        unsafe { crate::ffi_effect::ee_gfx_effect_record_free(Box::into_raw(Box::new(record))) };
    }
}
