//! The teleport (kind 0x3f) and the time warp (kind 0x44) whole in Rust (`EEE3DTeleport`, `EEE3DTimeWarp`, deleted):
//! a model on a unit that rises or sinks over the effect's duration, launching the effect's two linked line effects
//! from the far end of its span when it is made, and drawn in pass 4 fully transparent with z writes (a depth-only
//! mask). The generic C++ class `EEE3DRust` holds the graphic object and answers the world (`EeGfxEffect3DHost`);
//! the draw goes through the rasterizer table (`raster.rs`). Their age is the aged record's, their step
//! `effect_state::timed_step`, their Load and Save `load_aged`/`save_aged`.
//!
//! The record is reached through its raw pointer field by field: the C++ reads it (the transformation, the radius)
//! during the host calls, so no Rust reference to it lives across one.

use std::ffi::c_void;

use ee_ge::types::{Ge3DPoint, GeVector};

use crate::effect_follow::{follow, load_shield, save_shield, EE_GFX_EFFECT_KIND_SHIELD};
use crate::effect_shot::{is_shot, load_shot, save_shot, shot};
use crate::effect_state::{
    identity, load_aged, save_aged, timed_step, EeGfxEffectRecord, EeGfxTimedTick, EffectFile,
    EE_GFX_EFFECT_KIND_BLAST_WAVE, EE_GFX_EFFECT_KIND_TELEPORT, EE_GFX_EFFECT_KIND_TIME_WARP, EE_GFX_EFFECT_MOVED,
};
use crate::lighting::EeGfxLighting;
use crate::raster::{EeGfxModel, Raster, BLEND_INV_SRC_ALPHA, BLEND_SRC_ALPHA, ZBUFFER_TEST_AND_WRITE};

/// The time warp starts this far below the unit when it rises.
const TIME_WARP_DROP: f32 = 0.2;

/// The calls of `EEE3DRust`'s virtuals.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGfxEffect3DOp {
    /// the constructor on a unit (after `EEEffect3D`'s)
    Create = 0,
    SetUp = 1,
    Queue = 2,
    DrawPass2 = 3,
    DrawPass3 = 4,
    DrawPass4 = 5,
    DrawPass5 = 6,
    DrawPass6 = 7,
    Step = 8,
    /// the constructor at a point (the point in the record's `aged.location`)
    CreateAt = 9,
    /// `RepairPointers` after `EEEffect`'s
    Repair = 10,
    /// `Load` and `Save` after `EEEffect3D`'s (`ee_gfx_effect3d_file`)
    Load = 11,
    Save = 12,
}

/// What the effects read of their graphic effect and the tick, gathered by the C++ for each call.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxGraphicFacts {
    /// `mSize`
    pub size: f32,
    pub param1: f32,
    pub param2: f32,
    pub param3: f32,
    pub param4: f32,
    pub param5: f32,
    pub param6: f32,
    pub flag1: bool,
    /// ms
    pub duration: u32,
    /// the graphic effect has its graphic object, and that object's alpha reference
    pub has_object: bool,
    pub alpha_ref: f32,
    /// `GetTickSeconds`, `GetTickTime`
    pub tick_seconds: f32,
    pub tick_time: f32,
    /// the effect has a unit, and its hit points
    pub has_unit: bool,
    pub unit_hit_points: i32,
}

/// The C++'s answers for an `EEE3DRust`.
#[repr(C)]
pub struct EeGfxEffect3DHost {
    pub context: *mut c_void,
    /// the unit's position (`EEUnit::GetPosition`)
    pub unit_position: extern "C" fn(context: *mut c_void) -> Ge3DPoint,
    /// the graphic effect's first model's height in its bounding box at frame 0
    pub model_height: extern "C" fn(context: *mut c_void) -> f32,
    /// `MoveToTransformation(mTransformation)` (the region)
    pub move_to: extern "C" fn(context: *mut c_void),
    /// `gEffectManager->CreateLineEffect` from `start` along `direction` with linked effect 1 or 2
    pub create_line:
        extern "C" fn(context: *mut c_void, start: *const Ge3DPoint, direction: *const GeVector, linked: u32),
    /// `EEEffect3D::Queue`
    pub base_queue: extern "C" fn(context: *mut c_void) -> i32,
    /// push the effect into render queue `index` under its graphic effect's texture
    pub queue: extern "C" fn(context: *mut c_void, index: u32),
    /// the graphic object is the graphic effect's; `LoadMedia`
    pub load_media: extern "C" fn(context: *mut c_void) -> i32,
    /// the graphic object's model
    pub model: extern "C" fn(context: *mut c_void) -> EeGfxModel,
    /// the graphic object's first texture
    pub texture: extern "C" fn(context: *mut c_void) -> EeGfxModel,
    /// `FollowUnit`: to the unit's first tag point of type 0x12 (`SetTransformation`)
    pub follow_unit: extern "C" fn(context: *mut c_void),
    /// the graphic object's model `index` (`mModels[index]`)
    pub model_at: extern "C" fn(context: *mut c_void, index: u32) -> EeGfxModel,
    /// a tracer's end: its graphic effect's linked effect 1 placed at the effect's position, then the unit's
    /// `UnitDamaged` (each when there is one)
    pub shot_hit: extern "C" fn(context: *mut c_void),
}

/// The world as the effects ask it.
pub trait Effect3DHost {
    fn unit_position(&mut self) -> Ge3DPoint;
    fn model_height(&mut self) -> f32;
    fn move_to(&mut self);
    fn create_line(&mut self, start: &Ge3DPoint, direction: &GeVector, linked: u32);
    fn base_queue(&mut self) -> i32;
    fn queue(&mut self, index: u32);
    fn load_media(&mut self) -> i32;
    fn model(&mut self) -> EeGfxModel;
    fn texture(&mut self) -> EeGfxModel;
    fn follow_unit(&mut self);
    fn model_at(&mut self, index: u32) -> EeGfxModel;
    fn shot_hit(&mut self);
}

impl Effect3DHost for &EeGfxEffect3DHost {
    fn unit_position(&mut self) -> Ge3DPoint {
        (self.unit_position)(self.context)
    }
    fn model_height(&mut self) -> f32 {
        (self.model_height)(self.context)
    }
    fn move_to(&mut self) {
        (self.move_to)(self.context)
    }
    fn create_line(&mut self, start: &Ge3DPoint, direction: &GeVector, linked: u32) {
        (self.create_line)(self.context, start, direction, linked)
    }
    fn base_queue(&mut self) -> i32 {
        (self.base_queue)(self.context)
    }
    fn queue(&mut self, index: u32) {
        (self.queue)(self.context, index)
    }
    fn load_media(&mut self) -> i32 {
        (self.load_media)(self.context)
    }
    fn model(&mut self) -> EeGfxModel {
        (self.model)(self.context)
    }
    fn texture(&mut self) -> EeGfxModel {
        (self.texture)(self.context)
    }
    fn follow_unit(&mut self) {
        (self.follow_unit)(self.context)
    }
    fn model_at(&mut self, index: u32) -> EeGfxModel {
        (self.model_at)(self.context, index)
    }
    fn shot_hit(&mut self) {
        (self.shot_hit)(self.context)
    }
}

/// One virtual of an `EEE3DRust` of the record's kind.
///
/// # Safety
/// `r` is a live record, read and written only through this pointer during the call (the C++ reads it in the host
/// calls).
pub unsafe fn effect3d<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY: the function's contract
    let kind = unsafe { (*r).kind };
    match kind {
        EE_GFX_EFFECT_KIND_TELEPORT | EE_GFX_EFFECT_KIND_TIME_WARP => unsafe { rising(op, r, g, h, x) },
        EE_GFX_EFFECT_KIND_BLAST_WAVE | EE_GFX_EFFECT_KIND_SHIELD => unsafe { follow(op, r, g, h, x) },
        k if is_shot(k) => unsafe { shot(op, r, g, h, x) },
        _ => 0,
    }
}

/// `Load` (`op` Load) or `Save` of an `EEE3DRust` after `EEEffect3D`'s, whose result is `base`; the file versions.
pub fn effect3d_file<H: Effect3DHost, F: EffectFile>(
    op: EeGfxEffect3DOp,
    base: i32,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    h: &mut H,
) -> i32 {
    let load = op == EeGfxEffect3DOp::Load;
    match r.kind {
        EE_GFX_EFFECT_KIND_SHIELD => {
            // the base's and the media's results are ignored
            if load {
                h.load_media();
                load_shield(r, f, major, minor)
            } else {
                save_shield(r, f)
            }
        }
        k if is_shot(k) => {
            // the base's and the media's results are ignored
            if load {
                h.load_media();
                load_shot(r, f)
            } else {
                save_shot(r, f)
            }
        }
        EE_GFX_EFFECT_KIND_BLAST_WAVE if load => {
            // the base's result is ignored; the media's stops the load
            let res = h.load_media();
            if res != 0 {
                res
            } else {
                load_aged(r, f)
            }
        }
        _ if base != 0 => base,
        _ if load => load_aged(r, f),
        _ => save_aged(r, f),
    }
}

/// The teleport's and time warp's virtuals.
///
/// # Safety
/// As `effect3d`.
unsafe fn rising<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept
    let warp = unsafe { (*r).kind } == EE_GFX_EFFECT_KIND_TIME_WARP;
    match op {
        EeGfxEffect3DOp::Create => {
            if warp {
                // the warp's age starts at zero (the original left it unset)
                unsafe { (*r).aged.age = 0.0 };
            }
            let mut point = h.unit_position();
            let mut t = identity();
            t.m[0][3] = point.x;
            t.m[1][3] = point.y;
            t.m[2][3] = point.z;
            let height = h.model_height() * g.size;
            if g.flag1 {
                t.m[2][3] = if warp { point.z - TIME_WARP_DROP } else { point.z };
                point.z = point.z + height;
            } else {
                t.m[2][3] = point.z - height;
            }
            unsafe { (*r).transformation = t };
            h.move_to();
            unsafe { (*r).bounding_radius = g.size };
            let direction = GeVector { x: 0.0, y: 0.0, z: if g.flag1 { -g.param1 } else { g.param1 } };
            h.create_line(&point, &direction, 1);
            h.create_line(&point, &direction, 2);
            0
        }
        EeGfxEffect3DOp::Repair => h.load_media(),
        EeGfxEffect3DOp::SetUp => {
            h.load_media();
            if !warp {
                unsafe { (*r).aged.age = 0.0 };
            }
            unsafe { (*r).bounding_radius = g.size };
            0
        }
        EeGfxEffect3DOp::Queue => {
            let result = h.base_queue();
            if result == 0 {
                unsafe {
                    (*r).aged.draw_age = (*r).aged.age;
                    let t = (*r).transformation;
                    (*r).aged.location = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
                }
                h.queue(2);
            }
            result
        }
        EeGfxEffect3DOp::DrawPass4 => {
            let model = h.model();
            x.set_alpha_ref(if g.has_object { g.alpha_ref } else { 0.0 });
            let fog = x.get_fog();
            let ambient = x.get_ambient();
            x.set_zbuffer(ZBUFFER_TEST_AND_WRITE);
            x.set_blending(BLEND_SRC_ALPHA, BLEND_INV_SRC_ALPHA);
            x.set_shading_source(0);
            x.lighting(EeGfxLighting::White);
            x.set_alpha_testing(0);
            x.model_opacity(model, 0.0);
            let (location, mut t) = unsafe { ((*r).aged.location, (*r).transformation) };
            x.fog_factor(&location, fog);
            t.m[0][3] = location.x;
            t.m[1][3] = location.y;
            t.m[2][3] = location.z;
            x.model_transform(model, &t);
            x.model_scale(model, g.size);
            x.model_prepare(model);
            let result = x.model_draw(model, 1.0);
            x.set_ambient(ambient);
            x.set_fog(fog);
            x.set_alpha_testing(1);
            result
        }
        EeGfxEffect3DOp::Step => {
            let tick = EeGfxTimedTick {
                tick_seconds: g.tick_seconds,
                tick_time: g.tick_time,
                duration: g.duration,
                speed: if warp { g.param1 } else { g.param2 },
                sinking: g.flag1,
            };
            let mut local = unsafe { r.read() };
            let result = timed_step(&mut local, &tick);
            unsafe { r.write(local) };
            if result == EE_GFX_EFFECT_MOVED {
                h.move_to();
                0
            } else {
                result
            }
        }
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::raster::LogRaster;

    #[derive(Default)]
    struct Host {
        calls: Vec<String>,
        queue_result: i32,
    }

    impl Effect3DHost for Host {
        fn unit_position(&mut self) -> Ge3DPoint {
            Ge3DPoint { x: 10.0, y: 20.0, z: 3.0 }
        }
        fn model_height(&mut self) -> f32 {
            2.0
        }
        fn move_to(&mut self) {
            self.calls.push("move".into());
        }
        fn create_line(&mut self, s: &Ge3DPoint, d: &GeVector, linked: u32) {
            self.calls.push(format!("line {} {} {} {} {linked}", s.x, s.y, s.z, d.z));
        }
        fn base_queue(&mut self) -> i32 {
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
        fn follow_unit(&mut self) {
            self.calls.push("follow".into());
        }
        fn model_at(&mut self, _index: u32) -> EeGfxModel {
            std::ptr::null_mut()
        }
        fn shot_hit(&mut self) {
            self.calls.push("hit".into());
        }
    }

    fn facts() -> EeGfxGraphicFacts {
        EeGfxGraphicFacts {
            size: 1.5,
            param1: 4.0,
            param2: 8.0,
            flag1: true,
            duration: 1000,
            has_object: true,
            alpha_ref: 0.25,
            tick_seconds: 0.125,
            tick_time: 125.0,
            ..Default::default()
        }
    }

    fn run(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> (i32, LogRaster) {
        let mut x = LogRaster { fog: 1, ..Default::default() };
        // SAFETY: a live record, not otherwise borrowed during the call
        let result = unsafe { effect3d(op, r, g, h, &mut x) };
        (result, x)
    }

    #[test]
    fn made_on_a_unit_they_launch_two_lines_from_the_far_end() {
        let mut h = Host::default();
        let mut w = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TIME_WARP);
        w.aged.age = 7.0;
        run(EeGfxEffect3DOp::Create, &mut w, &facts(), &mut h);
        // rising: the warp starts 0.2 below the unit, the lines go down from the model's top
        assert_eq!((w.transformation.m[0][3], w.transformation.m[2][3]), (10.0, 3.0 - 0.2));
        assert_eq!((w.aged.age, w.bounding_radius), (0.0, 1.5));
        assert_eq!(h.calls, ["move", "line 10 20 6 -4 1", "line 10 20 6 -4 2"]);
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TELEPORT);
        let mut h = Host::default();
        run(EeGfxEffect3DOp::Create, &mut p, &EeGfxGraphicFacts { flag1: false, ..facts() }, &mut h);
        // sinking: the model starts one height below the unit, the lines go up from the unit
        assert_eq!(p.transformation.m[2][3], 0.0);
        assert_eq!(h.calls[1], "line 10 20 3 4 1");
    }

    #[test]
    fn set_up_queue_and_step() {
        let mut h = Host::default();
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TELEPORT);
        p.aged.age = 5.0;
        run(EeGfxEffect3DOp::SetUp, &mut p, &facts(), &mut h);
        assert_eq!((p.aged.age, p.bounding_radius), (0.0, 1.5));
        p.transformation.m[1][3] = 9.0;
        p.aged.age = 3.0;
        assert_eq!(run(EeGfxEffect3DOp::Queue, &mut p, &facts(), &mut h).0, 0);
        assert_eq!((p.aged.draw_age, p.aged.location.y), (3.0, 9.0));
        h.queue_result = 10;
        assert_eq!(run(EeGfxEffect3DOp::Queue, &mut p, &facts(), &mut h).0, 10);
        assert_eq!(h.calls, ["media", "queue 2"]);
        // the teleport sinks (flag 1) by param 2; the step moves it through the host
        p.aged.age = 0.0;
        assert_eq!(run(EeGfxEffect3DOp::Step, &mut p, &facts(), &mut h).0, 0);
        assert_eq!((p.aged.age, p.transformation.m[2][3]), (125.0, -1.0));
        assert_eq!(h.calls.last().map(String::as_str), Some("move"));
        p.aged.age = 1000.0;
        assert_eq!(run(EeGfxEffect3DOp::Step, &mut p, &facts(), &mut h).0, 7);
        let mut w = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TIME_WARP);
        w.aged.age = 5.0;
        run(EeGfxEffect3DOp::SetUp, &mut w, &facts(), &mut h);
        assert_eq!(w.aged.age, 5.0);
        run(EeGfxEffect3DOp::Step, &mut w, &facts(), &mut h);
        assert_eq!(w.transformation.m[2][3], -0.5);
    }

    #[test]
    fn the_draw_masks_the_depth_and_restores_the_state() {
        let mut h = Host::default();
        let mut p = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_TELEPORT);
        p.aged.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
        let (result, x) = run(EeGfxEffect3DOp::DrawPass4, &mut p, &facts(), &mut h);
        assert_eq!(result, 0);
        assert_eq!(
            x.calls,
            [
                "alpharef 0.25",
                "zbuffer 3",
                "blend 4 5",
                "shading 0",
                "lighting White",
                "alphatest 0",
                "opacity 0",
                "fogfactor 1 2 3 1",
                "transform 1 2 3",
                "scale 1.5",
                "prepare",
                "draw 1",
                "ambient 0 0 0",
                "fog 1",
                "alphatest 1"
            ]
        );
        for op in [EeGfxEffect3DOp::DrawPass2, EeGfxEffect3DOp::DrawPass5, EeGfxEffect3DOp::DrawPass6] {
            let (result, x) = run(op, &mut p, &facts(), &mut h);
            assert_eq!((result, x.calls.len()), (0, 0));
        }
        let (_, x) =
            run(EeGfxEffect3DOp::DrawPass4, &mut p, &EeGfxGraphicFacts { has_object: false, ..facts() }, &mut h);
        assert_eq!(x.calls[0], "alpharef 0");
    }
}
