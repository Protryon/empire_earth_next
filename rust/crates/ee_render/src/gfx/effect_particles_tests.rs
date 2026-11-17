//! `effect_particles.rs`'s virtuals: a particle, an emitter and a marching line over a mock host and the logging
//! rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::effect_state::{EeGfxParticleTick, RANDOM_TESTS};
use crate::gfx::raster::{EeGfxModel, LogRaster};
use ee_base::math::types::GeVector;

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    base: i32,
    unit: EeGfxEmitterUnit,
    ids: [i32; 2],
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
        self.base
    }
    fn queue(&mut self, index: u32) {
        self.calls.push(format!("queue {index}"));
    }
    fn load_media(&mut self) -> i32 {
        self.calls.push("media".into());
        0
    }
    fn model(&mut self) -> EeGfxModel {
        5 as EeGfxModel
    }
    fn texture(&mut self) -> EeGfxModel {
        6 as EeGfxModel
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
    fn particle_tick(&mut self, p: &Ge3DPoint) -> EeGfxParticleTick {
        EeGfxParticleTick { now: 500.0, position: *p, duration: 1000, ..Default::default() }
    }
    fn effect_duration(&mut self) -> u32 {
        750
    }
    fn emitter_place(&mut self) {
        self.calls.push("place".into());
    }
    fn emitter_unit(&mut self) -> EeGfxEmitterUnit {
        self.unit
    }
    fn emitter_sound(&mut self, p: &Ge3DPoint, first: bool) {
        self.calls.push(format!("sound {} {first}", p.x));
    }
    fn emitter_line(&mut self, _s: &Ge3DPoint, _d: &Ge3DPoint) -> i32 {
        self.calls.push("line".into());
        0
    }
    fn effect_queue(&mut self) -> i32 {
        self.base
    }
    fn march_media(&mut self) {
        self.calls.push("march media".into());
    }
    fn march_ends(&mut self, from: &Ge3DPoint, to: &Ge3DPoint) -> [Ge3DPoint; 2] {
        [*from, *to]
    }
    fn march_set_unit(&mut self, which: u32, id: u32) {
        self.calls.push(format!("unit {which} {id}"));
    }
    fn march_unit_id(&mut self, which: u32) -> i32 {
        self.ids[which as usize]
    }
}

fn call(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: a live record, only referenced through the pointer during the call
    unsafe { effect3d(op, r, g, h, x) }
}

#[test]
fn a_particle_starts_at_its_point_with_the_clock_and_duration() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
    r.aged.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
    let mut h = Host::default();
    let g = EeGfxGraphicFacts { duration: 900, ..Default::default() };
    call(EeGfxEffect3DOp::CreateAt, &mut r, &g, &mut h, &mut LogRaster::default());
    assert_eq!(r.particle.draw_position, r.aged.location);
    assert_eq!((r.particle.last_time, r.particle.duration), (500.0, 900.0));
    assert_eq!(r.transformation.m[2][3], 3.0);
    assert_eq!(h.calls, vec!["move"]);
}

#[test]
fn a_particle_sets_up_its_radius_and_media() {
    let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
    let mut h = Host::default();
    let g = EeGfxGraphicFacts { size: 2.5, int_param1: 1, param2: 0.5, ..Default::default() };
    assert_eq!(call(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h, &mut LogRaster::default()), 0);
    assert_eq!(r.bounding_radius, 2.5);
    assert_eq!((r.particle.yaw, r.particle.pitch), (0.0, 0.0));
    assert_eq!(h.calls, vec!["media"]);
}

#[test]
fn a_particle_draws_fading_or_additive_and_restores_the_model() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
    r.particle.draw_age = 0.5;
    r.particle.brightness = 0.8;
    let mut h = Host::default();
    let g = EeGfxGraphicFacts {
        size: 3.0,
        start_size: 1.0,
        param1: 0.5,
        has_object: true,
        alpha_ref: 0.1,
        ..Default::default()
    };
    let mut x = LogRaster { fog: 1, fog_value: 0.5, ..Default::default() };
    call(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert!(x.calls.contains(&"opacity 0.25".to_string()), "{:?}", x.calls);
    assert!(x.calls.contains(&"scale 2".to_string()), "{:?}", x.calls);
    assert_eq!(x.calls.last().unwrap(), "fog 1");
    let g = EeGfxGraphicFacts { flag1: true, ..g };
    let mut x = LogRaster { fog: 1, fog_value: 0.5, ..Default::default() };
    call(EeGfxEffect3DOp::DrawPass6, &mut r, &g, &mut h, &mut x);
    assert!(x.calls.contains(&"blend 1 1".to_string()), "{:?}", x.calls);
    assert!(x.calls.contains(&"ambient 0.2 0.2 0.2".to_string()), "{:?}", x.calls);
}

#[test]
fn a_particle_loads_its_position_and_duration_after_its_state() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
    r.particle.age = 12.0;
    r.transformation.m[0][3] = 4.0;
    let mut h = Host::default();
    let mut f = Mem::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 3, &mut r, &mut f, 1, 0, &mut h), 0);
    let mut l = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_PARTICLE_3D);
    l.transformation.m[0][3] = 4.0;
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 3, &mut l, &mut f, 1, 0, &mut h), 0);
    assert_eq!((l.particle.age, l.particle.draw_position.x, l.particle.duration), (12.0, 4.0, 750.0));
    assert_eq!(h.calls, vec!["media"]);
}

#[test]
fn an_emitter_draws_nothing_and_moves_itself_without_a_unit() {
    let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_EMITTER);
    r.aged.location = Ge3DPoint { x: 1.0, y: 1.0, z: 1.0 };
    let mut h = Host::default();
    let g = EeGfxGraphicFacts { duration: 1000, tick_seconds: 0.25, param8: 0.0, int_param3: 2, ..Default::default() };
    let mut x = LogRaster::default();
    call(EeGfxEffect3DOp::CreateAt, &mut r, &g, &mut h, &mut x);
    assert_eq!(call(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 10);
    h.calls.clear();
    assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(h.calls, vec!["move", "sound 1 true", "line", "line"]);
    assert!(r.emitter.stepped);
    h.calls.clear();
    for _ in 0..4 {
        call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x);
    }
    assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
    assert!(h.calls.contains(&"sound 1 false".to_string()));
}

#[test]
fn an_emitter_ends_with_its_dead_unit() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_EMITTER);
    let mut h = Host {
        unit: EeGfxEmitterUnit { has_unit: true, complex: true, dead: true, ..Default::default() },
        ..Default::default()
    };
    let g = EeGfxGraphicFacts { duration: 1000, flag1: true, flag2: true, ..Default::default() };
    call(EeGfxEffect3DOp::Create, &mut r, &g, &mut h, &mut LogRaster::default());
    assert_eq!(h.calls, vec!["place"]);
    assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut LogRaster::default()), 7);
}

#[test]
fn a_marching_line_draws_a_fading_screen_line_and_ages() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MARCHING_LINE);
    r.march.from = Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 };
    r.march.to = Ge3DPoint { x: 2.0, y: 0.0, z: 0.0 };
    let mut h = Host::default();
    let g = EeGfxGraphicFacts { duration: 300, tick_time: 100.0, ..Default::default() };
    let mut x = LogRaster::default();
    assert_eq!(call(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(r.march.draw_to.x, 2.0);
    call(EeGfxEffect3DOp::DrawPass4, &mut r, &g, &mut h, &mut x);
    assert!(x.calls.iter().any(|c| c.starts_with("line ")), "{:?}", x.calls);
    assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 0);
    assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x), 7);
}

#[test]
fn a_marching_line_saves_its_units_ids() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MARCHING_LINE);
    r.march.time = 5.0;
    let mut h = Host { ids: [-1, 9], ..Default::default() };
    let mut f = Mem::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut f, 1, 0, &mut h), 0);
    assert_eq!(f.items, vec![5.0f32.to_bits(), 0, 0, 1, 9]);
    let mut l = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_MARCHING_LINE);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut l, &mut f, 1, 0, &mut h), 0);
    assert_eq!(l.march.time, 5.0);
    assert_eq!(h.calls, vec!["unit 1 9"]);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 3, &mut l, &mut f, 1, 0, &mut h), 3);
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
