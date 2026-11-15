//! The smoke generators whole in Rust on `EEE3DRust` (`EEE3DSmokeGenerator`, 0xf9871..0xf9b6d with the destruction
//! smoke's 0xebbfd/0xebc56 and the damage smoke's 0xea75c, deleted): kind 0x2b on a unit or at a point, 0x2c the
//! damage smoke on a unit's tag points and 0x1e the destruction smoke at a destroyed unit. Their set-up, tick and files
//! were already Rust (`effect_state.rs`'s `smoke_set_up`, `smoke_step`, `load_smoke`, `save_smoke`); this is the
//! class: the constructors' placing, the queue that draws nothing (10), and the tick's facts gathered from the
//! graphic effect and the host (`smoke_unit`: the unit's place, command and health and, for the damage smoke, its
//! tag points of kind 0 at its frame in its scaled location, read once a step, as the original).
//!
//! Step, Load and Save are simulation-side (the timers are saved, the puffs' random draws come from the effects'
//! generator): RUST_PLAN 2.1.

use ee_ge::types::{Ge3DPoint, GeVector};

use crate::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::effect_state::{
    identity, smoke_set_up, smoke_step, EeGfxEffectRecord, EeGfxSmokeSetUp, EeGfxSmokeTick, SmokeHost,
    EE_GFX_EFFECT_KIND_DAMAGE_SMOKE, EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE, EE_GFX_EFFECT_KIND_SMOKE_GENERATOR,
};

const OK: i32 = 0;

/// What a smoke generator's tick reads of its unit (`smoke_unit`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxSmokeUnit {
    pub has_unit: bool,
    /// the unit's physical model's position
    pub position: Ge3DPoint,
    /// the damage smoke's: its command's `mType` (0 without one); the others': `GetCommandType()`
    pub command: i32,
    /// the damage smoke's: the unit's hit points and its stats' maximum, its tag points of kind 0
    pub hit_points: i32,
    pub max_hit_points: i32,
    pub has_tag_points: bool,
    pub tag_point_count: u32,
}

/// Whether `kind` is a smoke generator's.
pub fn is_smoke(kind: i32) -> bool {
    kind == EE_GFX_EFFECT_KIND_SMOKE_GENERATOR
        || kind == EE_GFX_EFFECT_KIND_DAMAGE_SMOKE
        || kind == EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE
}

/// The tick's and the set-up's world through the effect's host.
struct Smoke<'a, H: Effect3DHost>(&'a mut H);

impl<H: Effect3DHost> SmokeHost for Smoke<'_, H> {
    fn tag_point(&mut self, i: u32) -> Ge3DPoint {
        self.0.smoke_tag_point(i)
    }
    fn place(&mut self, point: Ge3DPoint) -> i32 {
        self.0.smoke_place(&point)
    }
    fn line(&mut self, start: Ge3DPoint, direction: Ge3DPoint) {
        self.0.create_line(&start, &GeVector { x: direction.x, y: direction.y, z: direction.z }, 1);
    }
}

/// The smoke generators' virtuals.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn smoke<H: Effect3DHost>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
) -> i32 {
    // SAFETY (every access below): the function's contract; no reference to the record is kept across a host call
    match op {
        // 0x4f98b4: at a fixed point (the C++ put it in the record's `aged.location`)
        EeGfxEffect3DOp::CreateAt => {
            unsafe {
                let p = (*r).aged.location;
                let mut t = identity();
                t.m[0][3] = p.x;
                t.m[1][3] = p.y;
                t.m[2][3] = p.z;
                (*r).transformation = t;
            }
            h.move_to();
            OK
        }
        // 0x4ebc56: the destruction smoke's lines; the others set up nothing
        EeGfxEffect3DOp::SetUp => {
            if unsafe { (*r).kind } != EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE {
                return OK;
            }
            let s = EeGfxSmokeSetUp { param1: g.param1, param2: g.param2, int_param1: g.int_param1 };
            smoke_set_up(unsafe { &*r }, &s, &mut Smoke(h))
        }
        // 0x4f9a1b: the puffs (the damage smoke's 0xea75c at its unit's tag points)
        EeGfxEffect3DOp::Step => {
            let u = h.smoke_unit();
            let t = EeGfxSmokeTick {
                tick_seconds: g.tick_seconds,
                tick_time: g.tick_time,
                has_unit: u.has_unit,
                unit_position: u.position,
                unit_command: u.command,
                hit_points: u.hit_points,
                max_hit_points: u.max_hit_points,
                duration: g.duration,
                time1: g.time1,
                time2: g.time2,
                param1: g.param1,
                param2: g.param2,
                has_tag_points: u.has_tag_points,
                tag_point_count: u.tag_point_count,
            };
            smoke_step(unsafe { &mut *r }, &t, &mut Smoke(h))
        }
        // nothing to draw (ICF-folded into 0x1020dc)
        EeGfxEffect3DOp::Queue => 10,
        _ => OK,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::effect_beam::EeGfxBeamUnit;
    use crate::effect_rising::effect3d;
    use crate::raster::{EeGfxModel, LogRaster};

    #[derive(Default)]
    struct Host {
        calls: Vec<String>,
        unit: EeGfxSmokeUnit,
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
        fn create_line(&mut self, s: &Ge3DPoint, d: &GeVector, l: u32) {
            self.calls.push(format!("line {} {} {} {l}", s.x, d.x, d.y));
        }
        fn base_queue(&mut self) -> i32 {
            0
        }
        fn queue(&mut self, _index: u32) {}
        fn load_media(&mut self) -> i32 {
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
        fn smoke_unit(&mut self) -> EeGfxSmokeUnit {
            self.unit
        }
        fn smoke_tag_point(&mut self, i: u32) -> Ge3DPoint {
            Ge3DPoint { x: i as f32, y: 0.0, z: 0.0 }
        }
        fn smoke_place(&mut self, p: &Ge3DPoint) -> i32 {
            self.calls.push(format!("place {}", p.z));
            0
        }
    }

    fn call(op: EeGfxEffect3DOp, r: &mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut Host) -> i32 {
        let mut x = LogRaster::default();
        // SAFETY: a live record, only referenced through the pointer during the call
        unsafe { effect3d(op, r, g, h, &mut x) }
    }

    #[test]
    fn a_placed_smoke_stands_at_its_point_and_draws_nothing() {
        let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_GENERATOR);
        r.aged.location = Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 };
        let mut h = Host::default();
        let g = EeGfxGraphicFacts::default();
        assert_eq!(call(EeGfxEffect3DOp::CreateAt, &mut r, &g, &mut h), 0);
        assert_eq!((r.transformation.m[0][3], r.transformation.m[2][3]), (1.0, 3.0));
        assert_eq!(call(EeGfxEffect3DOp::Queue, &mut r, &g, &mut h), 10);
        assert_eq!(call(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h), 0);
        assert_eq!(h.calls, vec!["move"]);
    }

    #[test]
    fn a_smoke_on_a_dead_unit_ends() {
        let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_SMOKE_GENERATOR);
        let mut h =
            Host { unit: EeGfxSmokeUnit { has_unit: true, command: 3, ..Default::default() }, ..Default::default() };
        let g = EeGfxGraphicFacts { tick_seconds: 0.125, duration: 10, ..Default::default() };
        assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h), 0);
        assert_eq!(r.smoke.time, 125.0);
        h.unit.command = crate::effect_state::EE_GFX_EFFECT_ACTION_DEAD;
        assert_eq!(call(EeGfxEffect3DOp::Step, &mut r, &g, &mut h), 7);
    }

    #[test]
    fn the_destruction_smoke_launches_lines() {
        let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_DESTRUCTION_SMOKE);
        r.smoke.radius = 2.0;
        let mut h = Host::default();
        let g = EeGfxGraphicFacts { param1: 1.0, param2: 0.5, int_param1: 2, ..Default::default() };
        assert_eq!(call(EeGfxEffect3DOp::SetUp, &mut r, &g, &mut h), 0);
        assert_eq!(h.calls.iter().filter(|c| c.starts_with("line")).count(), 4);
    }
}
