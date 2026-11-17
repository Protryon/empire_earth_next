//! `effect_additive.rs`'s tests: the additive pass's virtuals over a mock host and the logging rasterizer.

use super::*;
use crate::gfx::effect_beam::EeGfxBeamUnit;
use crate::gfx::effect_rising::{effect3d, effect3d_file};
use crate::gfx::effect_state::EffectFile;
use crate::gfx::raster::{EeGfxModel, LogRaster};
use ee_base::math::types::GeVector;

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
        self.calls.push("texture".into());
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
    fn unit_model_media(&mut self) -> i32 {
        self.calls.push("unit media".into());
        4
    }
    fn unit_scale(&mut self) -> f32 {
        self.calls.push("scale".into());
        2.0
    }
    fn unit_location(&mut self) -> GeTransformation {
        self.calls.push("location".into());
        let mut t = identity();
        t.m[0][3] = 5.0;
        t.m[1][3] = 6.0;
        t.m[2][3] = 7.0;
        t
    }
    fn unit_model_radius(&mut self) -> f32 {
        self.calls.push("radius".into());
        1.5
    }
    fn unit_frame(&mut self) -> EeGfxUnitFrame {
        self.calls.push("frame".into());
        EeGfxUnitFrame { frame: 9, animation: 2 }
    }
    fn unit_model_at(&mut self, index: u32) -> EeGfxModel {
        self.calls.push(format!("model {index}"));
        std::ptr::null_mut()
    }
}

fn facts() -> EeGfxGraphicFacts {
    EeGfxGraphicFacts { has_object: true, alpha_ref: 0.5, has_unit: true, unit_hit_points: 10, ..Default::default() }
}

fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
    let mut x = LogRaster { fog: 1, fog_value: 0.25, ..Default::default() };
    // SAFETY: a live record, not otherwise borrowed during the call
    let result = unsafe { effect3d(op, r, g, h, &mut x) };
    (result, x)
}

#[test]
fn the_pass_follows_its_unit_and_queues_its_frame() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ADDITIVE_PASS);
    assert_eq!(run(EeGfxEffect3DOp::Create, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(r.pass, EeGfxEffectPass::default());
    // the set-up's result is the unit model's media's, the graphic's ignored
    assert_eq!(run(EeGfxEffect3DOp::SetUp, &mut r, &facts(), &mut h).0, 4);
    assert_eq!(h.calls, ["media", "unit media", "scale", "location", "move", "radius"]);
    assert_eq!((r.pass.scale, r.bounding_radius, r.transformation.m[1][3]), (2.0, 3.0, 6.0));
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["frame", "queue 3"]);
    assert_eq!((r.pass.frame, r.pass.model_index, r.pass.draw_scale), (9, 2, 2.0));
    assert_eq!(r.pass.draw_transformation, r.transformation);
    // a refused queue copies nothing
    h.calls.clear();
    h.queue_result = 10;
    r.pass.scale = 8.0;
    assert_eq!(run(EeGfxEffect3DOp::Queue, &mut r, &facts(), &mut h).0, 10);
    assert_eq!((h.calls.len(), r.pass.draw_scale), (0, 2.0));
    // a step follows the unit; a dead unit ends it untouched
    h.calls.clear();
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &facts(), &mut h).0, 0);
    assert_eq!(h.calls, ["scale", "location", "move", "radius"]);
    h.calls.clear();
    let dead = EeGfxGraphicFacts { unit_hit_points: 0, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::Step, &mut r, &dead, &mut h).0, 7);
    assert!(h.calls.is_empty());
    // the repair after a load takes the unit's model and scale
    r.pass.scale = 0.0;
    assert_eq!(run(EeGfxEffect3DOp::Repair, &mut r, &facts(), &mut h).0, 4);
    assert_eq!(h.calls, ["unit media", "scale"]);
    assert_eq!(r.pass.scale, 2.0);
}

#[test]
fn the_draw_redraws_the_unit_model_additively() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ADDITIVE_PASS);
    r.pass.draw_transformation.m[0][3] = 4.0;
    r.pass.frame = 6;
    r.pass.model_index = 3;
    r.pass.draw_scale = 1.25;
    let (result, x) = run(EeGfxEffect3DOp::DrawPass5, &mut r, &facts(), &mut h);
    assert_eq!(result, 0);
    assert_eq!(h.calls, ["model 3", "texture"]);
    assert_eq!(
        x.calls,
        [
            "alpharef 0.5",
            "zbuffer 2",
            "blend 1 1",
            "shading 0",
            "lighting White",
            "fogfactor 4 0 0 1",
            "ambient 0.25 0.25 0.25",
            "texture",
            "frame 6",
            "scale 1.25",
            "transform 4 0 0",
            "prepare",
            "draw 1",
            "fog 1"
        ]
    );
    let g = EeGfxGraphicFacts { has_object: false, ..facts() };
    assert_eq!(run(EeGfxEffect3DOp::DrawPass5, &mut r, &g, &mut h).1.calls[0], "alpharef 0");
    for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass4, EeGfxEffect3DOp::DrawPass6] {
        let (result, x) = run(op, &mut r, &facts(), &mut h);
        assert_eq!((result, x.calls.len()), (0, 0));
    }
}

/// A file that refuses every value (the pass saves and loads none).
struct NoFile;

impl EffectFile for NoFile {
    fn is_open(&mut self) -> bool {
        true
    }
    fn read_u32(&mut self, _v: &mut u32) -> i32 {
        panic!("read")
    }
    fn read_i32(&mut self, _v: &mut i32) -> i32 {
        panic!("read")
    }
    fn read_f32(&mut self, _v: &mut f32) -> i32 {
        panic!("read")
    }
    fn read_bool(&mut self, _v: &mut bool) -> i32 {
        panic!("read")
    }
    fn write_u32(&mut self, _v: u32) -> i32 {
        panic!("write")
    }
    fn write_i32(&mut self, _v: i32) -> i32 {
        panic!("write")
    }
    fn write_f32(&mut self, _v: f32) -> i32 {
        panic!("write")
    }
    fn write_bool(&mut self, _v: bool) -> i32 {
        panic!("write")
    }
}

#[test]
fn save_is_the_base_and_load_the_base_then_the_media() {
    let mut h = Host::default();
    let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_ADDITIVE_PASS);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 0, &mut r, &mut NoFile, 0, 0, &mut h), 0);
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Save, 5, &mut r, &mut NoFile, 0, 0, &mut h), 5);
    assert!(h.calls.is_empty());
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 5, &mut r, &mut NoFile, 0, 0, &mut h), 5);
    assert!(h.calls.is_empty());
    assert_eq!(effect3d_file(EeGfxEffect3DOp::Load, 0, &mut r, &mut NoFile, 0, 0, &mut h), 3);
    assert_eq!(h.calls, ["media"]);
}
