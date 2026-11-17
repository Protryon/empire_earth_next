//! `effect_nuclear.rs`'s tests: the nuclear explosion's and the energy wave's virtuals over a mock host and the
//! logging rasterizer.

use super::*;
use crate::ge::types::GeVector;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::raster::{EeGfxModel, LogRaster};

#[derive(Default)]
struct Host {
    calls: Vec<String>,
    queue_result: i32,
    tile: EeGfxUnitTile,
    hidden: bool,
    space: Vec<(u32, u32)>,
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
        self.calls.push("texture 0".into());
        std::ptr::null_mut()
    }
    fn follow_unit(&mut self) {}
    fn model_at(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("model {index}"));
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
        Ge3DPoint { x: 10.25, y: 20.5, z: 3.0 }
    }
    fn unit_tile(&mut self) -> EeGfxUnitTile {
        self.tile
    }
    fn terrain_z(&mut self, x: f32, y: f32) -> f32 {
        self.calls.push(format!("terrain {x} {y}"));
        if x > 15.0 {
            -2.0
        } else {
            4.0
        }
    }
    fn texture_at(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("texture {index}"));
        std::ptr::null_mut()
    }
    fn unit_hidden(&mut self) -> bool {
        self.hidden
    }
    fn space_tile(&mut self, x: u32, y: u32) -> bool {
        self.calls.push(format!("tile {x} {y}"));
        self.space.contains(&(x, y))
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 2.0,
        start_size: 8.0,
        duration: 1000,
        param1: 2000.0,
        param2: 500.0,
        param3: 0.7,
        param4: 0.9,
        param5: 1.5,
        tick_seconds: 0.25,
        tick_time: 250.0,
        has_unit: true,
        unit_hit_points: 10,
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

#[test]
fn the_explosion_stands_on_the_ground_at_its_unit_or_its_tile() {
    let mut h = Host { tile: EeGfxUnitTile { on_map: true, size: 2.0, x: 30.0, y: 40.0 }, ..Default::default() };
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION);
    r.blast.flash = 0.5;
    assert_eq!(run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h).0, 0);
    assert_eq!((r.blast.flash, r.blast.rise), (0.0, 0.0));
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 3);
    assert_eq!(h.calls, ["position", "terrain 10.25 20.5", "move", "media"]);
    assert_eq!((r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]), (10.25, 20.5, 4.0));
    assert_eq!(r.bounding_radius, 2.0);
    // off the map: its tile's centre, never under 0
    h.tile.on_map = false;
    h.calls.clear();
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h);
    assert_eq!(h.calls[0], "terrain 31 41");
    assert_eq!((r.transformation.m[0][3], r.transformation.m[1][3], r.transformation.m[2][3]), (31.0, 41.0, 0.0));
}

#[test]
fn the_explosion_flashes_rises_and_ends_risen() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION);
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert_eq!((r.blast.flash, r.blast.rise, r.blast.scroll), (0.25, 0.125, 0.5));
    // the queue takes the scroll and clears it
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    assert_eq!((r.blast.draw_flash, r.blast.draw_rise, r.blast.draw_scroll, r.blast.scroll), (0.25, 0.125, 0.5, 0.0));
    assert_eq!(h.calls, ["queue 4"]);
    // the flash stops at 1; past a full rise: 7
    r.blast.flash = 0.9;
    r.blast.rise = 0.9;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 7);
    assert_eq!((r.blast.flash, r.blast.rise), (1.0, 0.9 + 0.125));
    // a failed base queue keeps the scroll
    h.queue_result = 10;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 10);
    assert_eq!(r.blast.scroll, 0.5);
}

#[test]
fn the_explosion_draws_its_cloud_stem_and_flash_and_restores_the_state() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION);
    r.transformation.m[0][3] = 1.0;
    r.transformation.m[1][3] = 2.0;
    r.transformation.m[2][3] = 3.0;
    r.blast.draw_rise = 0.5;
    r.blast.draw_scroll = 0.2;
    r.blast.draw_flash = 0.5;
    let mut x = LogRaster { fog: 1, fog_value: 0.8, ..Default::default() };
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h, &mut x) };
    assert_eq!(result, 0);
    assert_eq!(h.calls, ["model 11", "model 9", "texture 9", "texture 0"]);
    let opacity = 1.0f32 - 0.25;
    let lift = (1.0f32 - 0.25) * 8.0 * 0.5;
    let light = (1.0f32 - 0.25) * 0.8;
    let want: Vec<String> = [
        "alpharef 0.5".to_string(),
        "zbuffer 3".into(),
        "blend 4 5".into(),
        "shading 0".into(),
        "lighting Unit".into(),
        "texture".into(),
        "texture".into(),
        "transform 1 2 3".into(),
        "scale 8".into(),
        "transform 1 2 3".into(),
        format!("scale {}", lift + 8.0),
        format!("opacity {opacity}"),
        format!("opacity {opacity}"),
        format!("shift v {}", opacity * 0.2),
        format!("shift v {}", -(opacity * 0.2)),
        "prepare".into(),
        "draw 1".into(),
        "prepare".into(),
        "draw 1".into(),
        "lighting White".into(),
        "fogfactor 1 2 3 1".into(),
        format!("ambient {light} {light} {light}"),
        "blend 1 1".into(),
        "shading 0".into(),
        "depth 0 0".into(),
        "zbuffer 2".into(),
        "zcompare 1".into(),
        "color 1 1 1".into(),
        "material opacity 1".into(),
        "transform 1 2 3".into(),
        "scale 2".into(),
        "texture".into(),
        "prepare".into(),
        "draw 1".into(),
        "fog 1".into(),
        "depth 0 1".into(),
        "zbuffer 3".into(),
        "zcompare 2".into(),
    ]
    .into();
    assert_eq!(x.calls, want);
}

#[test]
fn the_energy_wave_cycles_its_phase_for_its_duration() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WAVE);
    assert_eq!(run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 3);
    assert_eq!(h.calls, ["position", "move", "media"]);
    assert_eq!((r.transformation.m[0][3], r.transformation.m[2][3], r.bounding_radius), (10.25, 3.0, 2.0));
    let g = EeGfxGraphicFacts { param1: 200.0, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 0);
    // 250 / 200 wraps once
    assert_eq!((r.blast.phase, r.aged.age), (1.25 - 1.0, 0.25));
    r.aged.age = 0.75;
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &g, &mut h).0, 7);
}

#[test]
fn the_energy_wave_takes_its_corners_and_draws_its_ground_quad() {
    let mut h = Host { space: vec![(11, 21)], ..Default::default() };
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WAVE);
    run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h);
    r.blast.phase = 0.5;
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    // corners at distance 1: (-1, 1) (1, 1) (1, -1) (-1, -1) around (10.25, 20.5)
    assert_eq!(h.calls, ["tile 9 21", "tile 11 21", "tile 11 19", "tile 9 19", "queue 4"]);
    assert_eq!((r.blast.draw_phase, r.blast.heights, r.blast.hidden), (0.5, [0.7, 0.0, 0.7, 0.7], false));
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!(result, 0);
    assert_eq!(x.calls, ["quad 10.25 20.5 4.5 color 1 1 1 yaw 0 size 2 heights [0.7, 0.0, 0.7, 0.7] 0.9 0.5"]);
    // a cloaked enemy's: nothing drawn
    h.hidden = true;
    run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h);
    let (result, x) = run(EeGfxEffect3DOp::DrawPass6, &mut r, &facts(), &mut h);
    assert_eq!((result, x.calls.len()), (0, 0));
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
fn both_save_and_load_their_two_values() {
    let mut h = Host::default();
    let mut n = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_NUCLEAR_EXPLOSION);
    n.blast.flash = 0.5;
    n.blast.rise = 0.25;
    let mut w = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ENERGY_WAVE);
    w.aged.age = 0.75;
    w.blast.phase = 0.125;
    for r in [&mut n, &mut w] {
        let mut m = Mem::default();
        assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 5, r, &mut m, 0, 0, &mut h), 0);
        assert_eq!(m.0.len(), 2);
        let mut back = EeGfxEffectRecord::new(r.kind);
        h.calls.clear();
        assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 5, &mut back, &mut Mem(m.0.clone(), 0), 0, 0, &mut h), 0);
        assert_eq!(h.calls, ["media"]);
        assert_eq!(
            (back.blast.flash, back.blast.rise, back.blast.phase, back.aged.age),
            (r.blast.flash, r.blast.rise, r.blast.phase, r.aged.age)
        );
        // one word short: the explosion's first error, the wave's phase read's
        let mut short = EeGfxEffectRecord::new(r.kind);
        assert_eq!(
            effect3d_file(EeGfxEffect3DOp::Load, 0, &mut short, &mut Mem(m.0[..1].to_vec(), 0), 0, 0, &mut h),
            9
        );
    }
}
