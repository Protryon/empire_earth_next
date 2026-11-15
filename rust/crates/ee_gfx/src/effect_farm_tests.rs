//! `effect_farm.rs`'s virtuals: a farm over a mock host and the logging rasterizer.

use super::*;
use crate::effect_beam::EeGfxBeamUnit;
use crate::effect_rising::{effect3d, effect3d_file};
use crate::effect_state::EeGfxEffectRecord;
use crate::raster::LogRaster;

struct Host {
    calls: Vec<String>,
    unit: EeGfxFarmUnit,
    drop_site: Option<(i32, i32)>,
    doppelganger: i32,
    loops: bool,
}

impl Default for Host {
    fn default() -> Host {
        Host {
            calls: Vec::new(),
            unit: EeGfxFarmUnit { tile_x: 10, tile_y: 20, tile_size: 2, built: false, cloaked: 0 },
            drop_site: None,
            doppelganger: -1,
            loops: false,
        }
    }
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
    fn texture_at(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("texture {index}"));
        std::ptr::null_mut()
    }
    fn farm_unit(&mut self) -> EeGfxFarmUnit {
        self.unit
    }
    fn farm_drop_site(&mut self, x: &mut i32, y: &mut i32) -> bool {
        match self.drop_site {
            Some((a, b)) => {
                *x = a;
                *y = b;
                true
            }
            None => false,
        }
    }
    fn farm_set_variant(&mut self, variant: i32) {
        self.unit.cloaked = variant;
    }
    fn farm_loops(&mut self, _index: u32) -> bool {
        self.loops
    }
    fn farm_doppelganger(&mut self) -> i32 {
        self.doppelganger
    }
    fn farm_opacity(&mut self, opacity: f32) -> f32 {
        opacity * 0.5
    }
    fn set_scaled_model(&mut self, model: EeGfxModel) {
        self.calls.push(format!("scaled {}", model as usize));
    }
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

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts {
        size: 0.25,
        param2: 100.0,
        param3: 0.5,
        int_param1: 2,
        tick_time: 125.0,
        unit_hit_points: 10,
        ..EeGfxGraphicFacts::default()
    }
}

fn call(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, h: &mut Host, x: &mut LogRaster) -> i32 {
    // SAFETY: a live record, only referenced through the pointer during the call
    unsafe { effect3d(op, r, &facts(), h, x) }
}

/// A farm made on its unit and set up.
fn made() -> (EeGfxEffectRecord, Host, LogRaster) {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_FARM);
    let mut h = Host::default();
    let mut x = LogRaster::default();
    assert_eq!(call(EeGfxEffect3DOp::Create, &mut r, &mut h, &mut x), 0);
    assert_eq!(call(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut x), 0);
    (r, h, x)
}

#[test]
fn the_field_covers_the_units_tiles() {
    let (r, _, mut x) = made();
    let a = r.farm;
    assert_eq!((a.area.left, a.area.top, a.area.right, a.area.bottom), (10.0, 20.0, 11.0, 21.0));
    assert_eq!((a.area.center.x, a.area.center.y), (11.0, 21.0));
    assert_eq!(a.row_length, 3);
    assert!(a.rebuild);
    // the two copies first, then the ground with its placeholder sub-model
    assert_eq!(x.model(a.ground_copy).name, "graphics farm ground model");
    assert_eq!(x.model(a.crops_copy).name, "graphics farm stalk model");
    assert_eq!(x.model(a.ground).name, "farm ground model");
    assert_eq!(x.models.len(), 4);
    // the copy's placeholders: polygon 0 is the last triple; the 9 vertices invisible
    assert_eq!(x.model(a.crops_copy).triangles[0], [21, 22, 23]);
    // the ground copy's vertices were overwritten by the mesh
    let (ground, copy) = (x.model(a.ground).points.clone(), x.model(a.ground_copy).points.clone());
    assert_eq!(ground, copy);
    assert_eq!(r.bounding_radius, 4.0);
}

#[test]
fn the_variant_comes_from_the_first_drop_site() {
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_FARM);
    let mut h = Host { drop_site: Some((6, 2)), ..Host::default() };
    let mut x = LogRaster::default();
    call(EeGfxEffect3DOp::Create, &mut r, &mut h, &mut x);
    call(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut x);
    // (6 >> 1) & 3 = 3, (2 >> 1) & 3 = 1: variant 0, the ground texture 0
    assert_eq!(h.unit.cloaked, 0);
    h.drop_site = Some((2, 4));
    call(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut x);
    // [1][2] = 0; [(4>>1)&3 = 2]: TILE_VARIANT[1][2] = 0
    h.drop_site = Some((0, 2));
    call(EeGfxEffect3DOp::SetUp, &mut r, &mut h, &mut x);
    assert_eq!(h.unit.cloaked, 1);
    assert!(h.calls.contains(&"texture 10".to_string()));
}

#[test]
fn a_built_farm_grows_and_sways_its_crops() {
    let (mut r, mut h, mut x) = made();
    h.unit.built = true;
    assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x), 0);
    assert!(!r.farm.rebuild);
    assert!(r.farm.has_crops);
    let crops = x.model(r.farm.ground).subs[0];
    // 2 tiles x 3 vertices x 2 rows x 2
    assert_eq!(x.model(crops).points.len(), 24);
    let before = x.model(crops).points.clone();
    // 125 / 100 per tick: past 1 on the first step
    assert_eq!(r.farm.sway_a, 0x3f);
    assert_eq!(r.farm.sway_b, 1);
    let after = x.model(crops).points.clone();
    assert_eq!(before, after);
    call(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x);
    let moved = x.model(crops).points.clone();
    // the tops (the first row of each pair) move, the feet stay
    assert_ne!(moved[0], after[0]);
    assert_eq!(moved[3], after[3]);
    assert_eq!(h.calls.last().unwrap(), &format!("scaled {}", r.farm.ground as usize));
}

#[test]
fn a_dead_farm_ends_without_a_doppelganger() {
    let (mut r, mut h, mut x) = made();
    let g = EeGfxGraphicFacts { unit_hit_points: 0, ..facts() };
    // SAFETY: a live record
    assert_eq!(unsafe { effect3d(EeGfxEffect3DOp::Step, &mut r, &g, &mut h, &mut x) }, 7);
}

#[test]
fn the_queue_refreshes_the_copies_and_the_draw_follows_it() {
    let (mut r, mut h, mut x) = made();
    h.unit.built = true;
    call(EeGfxEffect3DOp::Step, &mut r, &mut h, &mut x);
    h.doppelganger = 0;
    x.calls.clear();
    assert_eq!(call(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x), 0);
    assert!(r.farm.doppelganger && !r.farm.draw_crops && r.farm.draw_shown);
    assert_eq!(r.farm.draw_opacity, 0.5);
    assert!(x.calls.iter().any(|c| c.starts_with("copy") && c.ends_with("false")));
    x.calls.clear();
    call(EeGfxEffect3DOp::DrawPass2, &mut r, &mut h, &mut x);
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("draw")).count(), 1);
    assert!(x.calls.contains(&"lighting DimUnit".to_string()));
    assert_eq!(x.calls.last().unwrap(), "shading 1");
    h.doppelganger = -1;
    call(EeGfxEffect3DOp::Queue, &mut r, &mut h, &mut x);
    x.calls.clear();
    call(EeGfxEffect3DOp::DrawPass2, &mut r, &mut h, &mut x);
    assert_eq!(x.calls.iter().filter(|c| c.starts_with("draw")).count(), 2);
}

#[test]
fn a_saved_farm_loads_and_repairs() {
    let (mut r, mut h, mut x) = made();
    r.farm.bb4 = true;
    let mut f = Mem::default();
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut f, 1, 0, &mut h), 0);
    let mut l = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_FARM);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut l, &mut f, 1, 0, &mut h), 0);
    assert_eq!(l.farm.area, r.farm.area);
    assert!(l.farm.rebuild && !l.farm.bb4);
    h.unit.built = true;
    assert_eq!(call(EeGfxEffect3DOp::Repair, &mut l, &mut h, &mut x), 0);
    assert!(!l.farm.rebuild && l.farm.has_crops);
    assert!(!l.farm.ground.is_null() && !l.farm.crops_copy.is_null());
}
