//! The calamity classes' `Start` and `Stop` (`src/Empire Earth/Calamities/EECC*.cpp`): what each class sets up when
//! its calamity starts (the contagion helper, its effects and sound, the calamity's height and timers, the units it
//! disables or marks) and what it tears down when it stops. The decisions (which steps run, in which order, under
//! which tests) and the calamity's numbers are here; every world step is a host call ([`CalamityHost`]) the C++
//! answers: the shared ones in `EECalamityUpdate.cpp`, a class's own ([`EeCalamityOp::ClassStep`]) in its unit.
//!
//! The calamity's members travel as [`EeCalamityState`], copied in and out around every host call as for the
//! updates (`calamity_update.rs`).

// the C++'s own tests (`!(a > b)`), kept as written
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use crate::calamity_update::{
    kind, CalamityHost, EeCalamityCall, EeCalamityOp, EeCalamityParams, EeCalamityReply, EeCalamityState,
    EE_CALAMITY_EFFECT1, EE_CALAMITY_SOUND,
};

/// [`EeCalamityParams::flags`]: the morale class counts positive morale (`EECCMorale::mPositive`).
pub const EE_CALAMITY_POSITIVE: u32 = 32;

/// The unit categories and actions the time warp's stop tests and starts (`ee/EEUnitClassIDs.h`,
/// `ee/EEUnitAction.h`).
const CATEGORY_AIRPLANE: i32 = 3;
const CATEGORY_PLANET_SPACESHIP: i32 = 0x22;
/// `kActionCircle`.
pub const ACTION_CIRCLE: i32 = 1008;
/// `kActionIdle`.
pub const ACTION_IDLE: i32 = 5000;

/// The `ESVERIFY_AT` lines of the classes' `DestroyContagionHelper` checks (the C++'s assertion texts).
const VERIFY_FIRE: i32 = 118;
const VERIFY_PLAGUE: i32 = 86;
const VERIFY_LEPROSY: i32 = 112;
const VERIFY_AREA_EFFECT: i32 = 150;
const VERIFY_EXTRA_DAMAGE: i32 = 126;
const VERIFY_MIND_CONTROL: i32 = 122;

fn call<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, op: EeCalamityOp) -> EeCalamityReply {
    h.call(s, &EeCalamityCall::new(op))
}

fn step<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, index: i32) -> EeCalamityReply {
    h.call(s, &EeCalamityCall { index, ..EeCalamityCall::new(EeCalamityOp::ClassStep) })
}

fn step_at<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, index: i32, at: [f32; 3]) -> EeCalamityReply {
    h.call(s, &EeCalamityCall { index, x: at[0], y: at[1], z: at[2], ..EeCalamityCall::new(EeCalamityOp::ClassStep) })
}

fn step_value<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, index: i32, value: i32) -> EeCalamityReply {
    h.call(s, &EeCalamityCall { index, value, ..EeCalamityCall::new(EeCalamityOp::ClassStep) })
}

/// `SetSound(PlayAndReturnReferencedNode(...))` of the class's sound: at its own volume (`at` None), or with the
/// volume, pan and pitch `EEComputeSoundParameters` gives for a location.
fn start_sound<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, at: Option<[f32; 3]>) {
    let (value, at) = match at {
        Some(at) => (1, at),
        None => (0, [0.0; 3]),
    };
    h.call(s, &EeCalamityCall { value, x: at[0], y: at[1], z: at[2], ..EeCalamityCall::new(EeCalamityOp::StartSound) });
}

/// `StopSound` and `StopSecondarySound`.
fn stop_sounds<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, secondary: bool) {
    h.call(s, &EeCalamityCall { index: 0, ..EeCalamityCall::new(EeCalamityOp::StopSound) });
    if secondary {
        h.call(s, &EeCalamityCall { index: 1, ..EeCalamityCall::new(EeCalamityOp::StopSound) });
    }
}

/// The calamity's effect destroyed if it has one (`mEffectHandle != kNoEffectHandle`), the handle cleared.
fn destroy_effect<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState) {
    if s.effect_handle != 0 {
        let handle = s.effect_handle as i32;
        h.call(s, &EeCalamityCall { value: handle, ..EeCalamityCall::new(EeCalamityOp::DestroyEffect) });
        s.effect_handle = 0;
    }
}

/// `ESVERIFY_AT(DestroyContagionHelper(id), line)`.
fn destroy_helper<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, helper: i32, line: i32) {
    h.call(s, &EeCalamityCall { index: helper, value: line, ..EeCalamityCall::new(EeCalamityOp::DestroyHelper) });
}

/// The sight change of the line of sight (`SightOffHere`, `SetSight(GetSight() + delta)`, `SightOnHere`).
fn caster_sight<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, bonus: f32, add: bool) {
    call(h, s, EeCalamityOp::CasterSightOff);
    let sight = call(h, s, EeCalamityOp::CasterSight).x;
    let sight = if add { sight + bonus } else { sight - bonus };
    h.call(s, &EeCalamityCall { x: sight, ..EeCalamityCall::new(EeCalamityOp::CasterSightOn) });
}

/// One calamity's start: the class's `Start(calamity)`; whether it started (one that did not is deleted).
pub fn start<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, p: &EeCalamityParams) -> bool {
    let has = |f: u32| p.flags & f != 0;
    match p.class_enum {
        kind::EARTHQUAKE => {
            // CreateSourceEffect (always true): the quake's area and the source effect from the caster; its travel
            // time delays the quake (0 without a caster)
            if call(h, s, EeCalamityOp::CasterExists).ok {
                let source = step(h, s, 1);
                if source.ok && source.value == 0 {
                    s.float2 = source.x / p.quake_source_speed;
                }
            } else {
                s.float2 = 0.0;
            }
            call(h, s, EeCalamityOp::PlayClassSound);
            s.bool1 = false;
            true
        }
        kind::HURRICANE => {
            let height = p.start_size;
            h.call(s, &EeCalamityCall { x: height, ..EeCalamityCall::new(EeCalamityOp::SetHeight) });
            if has(EE_CALAMITY_EFFECT1) && call(h, s, EeCalamityOp::CasterExists).ok {
                let at = [s.target[0], s.target[1], s.target[2] + height];
                step_at(h, s, 1, at);
            }
            true
        }
        kind::VOLCANO => {
            let at = [s.target[0] - 0.5, s.target[1] - 1.5, s.target[2]];
            // the placed effect, then the highest ground under the volcano
            let maximum = step_at(h, s, 1, at).x;
            let height = maximum + p.start_size;
            h.call(s, &EeCalamityCall { x: height, ..EeCalamityCall::new(EeCalamityOp::SetHeight) });
            call(h, s, EeCalamityOp::PlayClassSound);
            true
        }
        kind::METEOR_STORM => {
            s.float1 = 100.0;
            s.float2 = 0.0;
            true
        }
        kind::SPECIAL_ATTACK | kind::SPECIAL_LOCATION => true,
        kind::CONSCRIPTION => {
            // a living target and a caster's player who can afford the spell: paid, the target marked
            if !call(h, s, EeCalamityOp::TargetExists).ok {
                return true;
            }
            if !(call(h, s, EeCalamityOp::TargetHitPoints).value > 0) {
                return true;
            }
            if !step(h, s, 1).ok {
                return true;
            }
            step(h, s, 2);
            true
        }
        kind::FIRE => {
            if !call(h, s, EeCalamityOp::TargetExists).ok {
                return false;
            }
            if has(EE_CALAMITY_EFFECT1) {
                step(h, s, 1);
            }
            if has(EE_CALAMITY_SOUND) {
                start_sound(h, s, None);
            }
            true
        }
        kind::PLAGUE => {
            step(h, s, 1);
            true
        }
        kind::LEPROSY => {
            let helper = call(h, s, EeCalamityOp::StartHelper);
            if !helper.ok {
                return false;
            }
            s.int1 = helper.value;
            step(h, s, 1);
            start_sound(h, s, None);
            true
        }
        kind::AREA_EFFECT => {
            if !(has(EE_CALAMITY_EFFECT1) && call(h, s, EeCalamityOp::CasterExists).ok) {
                return false;
            }
            // (GetSourceLocation's point is not used: the effect goes from the caster to the target on the ground,
            // at sea level on water terrain 1 and 10)
            let mut at = s.target;
            let tile = h.call(
                s,
                &EeCalamityCall {
                    index: ee_ge::um::floor(at[0]),
                    index2: ee_ge::um::floor(at[1]),
                    ..EeCalamityCall::new(EeCalamityOp::TileTerrain)
                },
            );
            let terrain = if tile.ok { tile.value } else { 0 };
            at[2] = if terrain != 1 && terrain != 10 {
                h.call(s, &EeCalamityCall { x: at[0], y: at[1], ..EeCalamityCall::new(EeCalamityOp::MeshZ) }).x
            } else {
                0.0
            };
            s.effect_handle = step_at(h, s, 1, at).id;
            if has(EE_CALAMITY_SOUND) {
                start_sound(h, s, None);
            }
            true
        }
        kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL => {
            let helper = call(h, s, EeCalamityOp::StartHelper);
            if !helper.ok {
                return false;
            }
            s.int1 = helper.value;
            // the effect's handle is not kept; the sound is placed at the target, or the origin without an effect
            let mut at = [0.0; 3];
            if has(EE_CALAMITY_EFFECT1) && call(h, s, EeCalamityOp::CasterExists).ok {
                at = s.target;
                step_at(h, s, 1, at);
            }
            start_sound(h, s, Some(at));
            true
        }
        kind::CLOAKING => {
            // the cloak's area effect on the caster, its sound; a capitol cloaks its group
            if step(h, s, 1).ok {
                step(h, s, 2);
            }
            step(h, s, 3);
            true
        }
        kind::DROP_OBJECT => {
            let mut at = [0.0; 3];
            if has(EE_CALAMITY_EFFECT1) && call(h, s, EeCalamityOp::CasterExists).ok {
                at = s.target;
                step_at(h, s, 2, at);
            }
            if !has(EE_CALAMITY_SOUND) {
                return true;
            }
            start_sound(h, s, Some(at));
            true
        }
        kind::LINE_OF_SIGHT => {
            if !call(h, s, EeCalamityOp::CasterExists).ok {
                return false;
            }
            caster_sight(h, s, p.sight_bonus, true);
            true
        }
        kind::LOGIC_BOMB => {
            if call(h, s, EeCalamityOp::TargetExists).ok && call(h, s, EeCalamityOp::TargetProtected).ok {
                return false;
            }
            let mut at = [0.0; 3];
            if has(EE_CALAMITY_EFFECT1) && call(h, s, EeCalamityOp::CasterExists).ok {
                let adjusted = step_at(h, s, 1, s.target);
                at = [adjusted.x, adjusted.y, adjusted.z];
                step_at(h, s, 4, at);
            }
            start_sound(h, s, Some(at));
            true
        }
        kind::MORALE => {
            if !call(h, s, EeCalamityOp::TargetExists).ok {
                return false;
            }
            if has(EE_CALAMITY_EFFECT1) && call(h, s, EeCalamityOp::CasterExists).ok {
                s.effect_handle = step(h, s, 1).id;
            }
            start_sound(h, s, None);
            true
        }
        kind::TELEPORT => {
            if !(call(h, s, EeCalamityOp::CasterExists).ok && call(h, s, EeCalamityOp::CasterHitPoints).value > 0) {
                return false;
            }
            step(h, s, 1);
            start_sound(h, s, None);
            true
        }
        kind::TIME_WARP => {
            if !call(h, s, EeCalamityOp::TargetExists).ok {
                return false;
            }
            if !(call(h, s, EeCalamityOp::TargetHitPoints).value > 0) {
                s.int2 = -1;
                return false;
            }
            // disabled and halted, the warp's effect above the unit
            step(h, s, 1);
            if has(EE_CALAMITY_EFFECT1) && call(h, s, EeCalamityOp::CasterExists).ok {
                s.effect_handle = step(h, s, 2).id;
            }
            true
        }
        _ => false,
    }
}

/// One calamity's stop: the class's `Stop(calamity)` (before the calamity is deleted).
pub fn stop<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, p: &EeCalamityParams) {
    let has = |f: u32| p.flags & f != 0;
    match p.class_enum {
        kind::EARTHQUAKE => stop_sounds(h, s, true),
        kind::HURRICANE => {
            stop_sounds(h, s, true);
            if call(h, s, EeCalamityOp::TargetExists).ok {
                call(h, s, EeCalamityOp::KillTarget);
            }
        }
        kind::VOLCANO => {
            stop_sounds(h, s, false);
            if call(h, s, EeCalamityOp::TargetExists).ok {
                call(h, s, EeCalamityOp::KillTarget);
            }
        }
        kind::FIRE | kind::PLAGUE | kind::LEPROSY => {
            let helper = s.int1;
            stop_sounds(h, s, true);
            let line = match p.class_enum {
                kind::FIRE => VERIFY_FIRE,
                kind::PLAGUE => VERIFY_PLAGUE,
                _ => VERIFY_LEPROSY,
            };
            destroy_helper(h, s, helper, line);
        }
        kind::AREA_EFFECT | kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL => {
            let helper = s.int1;
            destroy_effect(h, s);
            stop_sounds(h, s, true);
            let line = match p.class_enum {
                kind::AREA_EFFECT => VERIFY_AREA_EFFECT,
                kind::AE_EXTRA_DAMAGE => VERIFY_EXTRA_DAMAGE,
                _ => VERIFY_MIND_CONTROL,
            };
            destroy_helper(h, s, helper, line);
        }
        kind::CLOAKING => {
            // the cloak's area effect off the caster, its sound, the cloak flag
            step(h, s, 4);
            stop_sounds(h, s, false);
            step(h, s, 5);
        }
        kind::DROP_OBJECT => {
            // the dropped unit, next to a target while the caster lives
            if call(h, s, EeCalamityOp::TargetExists).ok && call(h, s, EeCalamityOp::CasterHitPoints).value > 0 {
                step(h, s, 1);
            }
            destroy_effect(h, s);
            stop_sounds(h, s, true);
        }
        kind::LINE_OF_SIGHT => {
            if !call(h, s, EeCalamityOp::CasterExists).ok || !(call(h, s, EeCalamityOp::CasterHitPoints).value > 0) {
                return;
            }
            caster_sight(h, s, p.sight_bonus, false);
        }
        kind::LOGIC_BOMB => {
            if call(h, s, EeCalamityOp::TargetExists).ok {
                // the target enabled again; a wonder (node kind 4) counts for its player again
                if step(h, s, 2).value == 4 {
                    step(h, s, 3);
                }
            }
            destroy_effect(h, s);
            stop_sounds(h, s, true);
        }
        kind::MORALE => {
            if call(h, s, EeCalamityOp::TargetExists).ok {
                // only a count the update took is given back
                if s.bool1 {
                    step(h, s, if has(EE_CALAMITY_POSITIVE) { 2 } else { 3 });
                }
                destroy_effect(h, s);
            }
            stop_sounds(h, s, true);
        }
        kind::TELEPORT => {
            step(h, s, 2);
            stop_sounds(h, s, true);
        }
        kind::TIME_WARP => {
            if call(h, s, EeCalamityOp::TargetComplexAlive).ok {
                // the unit back: untinted, its orders cleared, enabled, circling (aircraft) or idle
                let category = step(h, s, 3).value;
                let action = if category == CATEGORY_AIRPLANE || category == CATEGORY_PLANET_SPACESHIP {
                    ACTION_CIRCLE
                } else {
                    ACTION_IDLE
                };
                step_value(h, s, 4, action);
                destroy_effect(h, s);
            } else {
                s.int2 = -1;
            }
            stop_sounds(h, s, true);
        }
        _ => {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// A host that logs every call (op, index, value, x, y, z) and answers from a table (op, index).
    #[derive(Default)]
    struct Host {
        log: Vec<(EeCalamityOp, i32, i32, f32, f32, f32)>,
        answers: HashMap<(i32, i32), EeCalamityReply>,
    }

    impl Host {
        fn answer(&mut self, op: EeCalamityOp, index: i32, reply: EeCalamityReply) {
            self.answers.insert((op as i32, index), reply);
        }
        fn ops(&self) -> Vec<(EeCalamityOp, i32)> {
            self.log.iter().map(|l| (l.0, l.1)).collect()
        }
    }

    impl CalamityHost for Host {
        fn call(&mut self, _state: &mut EeCalamityState, c: &EeCalamityCall) -> EeCalamityReply {
            self.log.push((c.op, c.index, c.value, c.x, c.y, c.z));
            self.answers.get(&(c.op as i32, c.index)).copied().unwrap_or_default()
        }
    }

    fn ok() -> EeCalamityReply {
        EeCalamityReply { ok: true, ..Default::default() }
    }

    fn value(v: i32) -> EeCalamityReply {
        EeCalamityReply { value: v, ..Default::default() }
    }

    fn params(class_enum: i32, flags: u32) -> EeCalamityParams {
        EeCalamityParams { class_enum, flags, ..Default::default() }
    }

    use EeCalamityOp as Op;

    #[test]
    fn the_earthquake_waits_for_its_source_effect() {
        let p = EeCalamityParams { quake_source_speed: 4.0, ..params(kind::EARTHQUAKE, 0) };
        let mut h = Host::default();
        h.answer(Op::CasterExists, 0, ok());
        h.answer(Op::ClassStep, 1, EeCalamityReply { ok: true, x: 10.0, ..Default::default() });
        let mut s = EeCalamityState { bool1: true, ..Default::default() };
        assert!(start(&mut h, &mut s, &p));
        assert_eq!((s.float2, s.bool1), (2.5, false));
        assert_eq!(h.ops(), vec![(Op::CasterExists, 0), (Op::ClassStep, 1), (Op::PlayClassSound, 0)]);
        // a failed effect leaves the delay; no caster: no delay
        h.answer(Op::ClassStep, 1, EeCalamityReply { ok: true, value: 3, x: 10.0, ..Default::default() });
        let mut s = EeCalamityState { float2: 7.0, ..Default::default() };
        start(&mut h, &mut s, &p);
        assert_eq!(s.float2, 7.0);
        let mut s = EeCalamityState { float2: 7.0, ..Default::default() };
        assert!(start(&mut Host::default(), &mut s, &p));
        assert_eq!(s.float2, 0.0);
    }

    #[test]
    fn the_hurricane_and_the_volcano_set_their_height() {
        let p = EeCalamityParams { start_size: 2.0, ..params(kind::HURRICANE, EE_CALAMITY_EFFECT1) };
        let mut h = Host::default();
        h.answer(Op::CasterExists, 0, ok());
        let mut s = EeCalamityState { target: [1.0, 2.0, 3.0], ..Default::default() };
        assert!(start(&mut h, &mut s, &p));
        assert_eq!(h.log[0].3, 2.0);
        let effect = h.log.iter().find(|l| l.0 == Op::ClassStep).unwrap();
        assert_eq!((effect.3, effect.4, effect.5), (1.0, 2.0, 5.0));
        // no effect: only the height
        let mut h = Host::default();
        start(&mut h, &mut s, &params(kind::HURRICANE, 0));
        assert_eq!(h.ops(), vec![(Op::SetHeight, 0)]);

        let p = EeCalamityParams { start_size: 1.5, ..params(kind::VOLCANO, 0) };
        let mut h = Host::default();
        h.answer(Op::ClassStep, 1, EeCalamityReply { x: 4.0, ..Default::default() });
        let mut s = EeCalamityState { target: [10.0, 20.0, 0.5], ..Default::default() };
        assert!(start(&mut h, &mut s, &p));
        assert_eq!((h.log[0].3, h.log[0].4, h.log[0].5), (9.5, 18.5, 0.5));
        assert_eq!(h.log[1].0, Op::SetHeight);
        assert_eq!(h.log[1].3, 5.5);
        assert_eq!(h.log[2].0, Op::PlayClassSound);
    }

    #[test]
    fn the_contagions_start_their_helper_or_fail() {
        let p = params(kind::LEPROSY, EE_CALAMITY_SOUND);
        let mut s = EeCalamityState::default();
        assert!(!start(&mut Host::default(), &mut s, &p));
        let mut h = Host::default();
        h.answer(Op::StartHelper, 0, EeCalamityReply { ok: true, value: 12, ..Default::default() });
        assert!(start(&mut h, &mut s, &p));
        assert_eq!(s.int1, 12);
        assert_eq!(h.ops(), vec![(Op::StartHelper, 0), (Op::ClassStep, 1), (Op::StartSound, 0)]);
        // the extra damage: the sound at the target when the effect plays, at the origin without
        let p = params(kind::AE_EXTRA_DAMAGE, EE_CALAMITY_EFFECT1);
        let mut h = Host::default();
        h.answer(Op::StartHelper, 0, EeCalamityReply { ok: true, value: 3, ..Default::default() });
        h.answer(Op::CasterExists, 0, ok());
        let mut s = EeCalamityState { target: [4.0, 5.0, 6.0], ..Default::default() };
        assert!(start(&mut h, &mut s, &p));
        let sound = h.log.last().unwrap();
        assert_eq!((sound.0, sound.2, sound.3, sound.4, sound.5), (Op::StartSound, 1, 4.0, 5.0, 6.0));
        assert_eq!(s.effect_handle, 0);
        let mut h = Host::default();
        h.answer(Op::StartHelper, 0, EeCalamityReply { ok: true, value: 3, ..Default::default() });
        start(&mut h, &mut s, &p);
        let sound = h.log.last().unwrap();
        assert_eq!((sound.2, sound.3, sound.4, sound.5), (1, 0.0, 0.0, 0.0));
        // the fire needs a target
        assert!(!start(&mut Host::default(), &mut s, &params(kind::FIRE, EE_CALAMITY_EFFECT1)));
    }

    #[test]
    fn the_area_effect_sits_on_the_ground_or_the_sea() {
        let p = params(kind::AREA_EFFECT, EE_CALAMITY_EFFECT1 | EE_CALAMITY_SOUND);
        let mut h = Host::default();
        h.answer(Op::CasterExists, 0, ok());
        h.answer(Op::TileTerrain, 3, EeCalamityReply { ok: true, value: 10, ..Default::default() });
        h.answer(Op::MeshZ, 0, EeCalamityReply { x: 7.0, ..Default::default() });
        h.answer(Op::ClassStep, 1, EeCalamityReply { id: 44, ..Default::default() });
        let mut s = EeCalamityState { target: [3.7, 8.2, 1.0], ..Default::default() };
        assert!(start(&mut h, &mut s, &p));
        let tile = h.log.iter().find(|l| l.0 == Op::TileTerrain).unwrap();
        assert_eq!(tile.1, 3);
        assert_eq!(h.log.iter().find(|l| l.0 == Op::ClassStep).unwrap().5, 0.0);
        assert_eq!(s.effect_handle, 44);
        // land: the mesh height
        h.answer(Op::TileTerrain, 3, EeCalamityReply { ok: true, value: 2, ..Default::default() });
        h.log.clear();
        start(&mut h, &mut s, &p);
        assert_eq!(h.log.iter().find(|l| l.0 == Op::ClassStep).unwrap().5, 7.0);
        // no caster: not started
        assert!(!start(&mut Host::default(), &mut s, &p));
    }

    #[test]
    fn the_stops_tear_down_in_order() {
        let mut h = Host::default();
        let mut s = EeCalamityState { int1: 5, effect_handle: 9, ..Default::default() };
        stop(&mut h, &mut s, &params(kind::AE_MIND_CONTROL, 0));
        assert_eq!(
            h.ops(),
            vec![(Op::DestroyEffect, 0), (Op::StopSound, 0), (Op::StopSound, 1), (Op::DestroyHelper, 5)]
        );
        assert_eq!(h.log[0].2, 9);
        assert_eq!(h.log[3].2, VERIFY_MIND_CONTROL);
        assert_eq!(s.effect_handle, 0);
        // the volcano kills its unit, after its one sound
        let mut h = Host::default();
        h.answer(Op::TargetExists, 0, ok());
        stop(&mut h, &mut EeCalamityState::default(), &params(kind::VOLCANO, 0));
        assert_eq!(h.ops(), vec![(Op::StopSound, 0), (Op::TargetExists, 0), (Op::KillTarget, 0)]);
        // the morale gives back only a count it took
        let mut h = Host::default();
        h.answer(Op::TargetExists, 0, ok());
        let mut s = EeCalamityState { bool1: true, ..Default::default() };
        stop(&mut h, &mut s, &params(kind::MORALE, EE_CALAMITY_POSITIVE));
        assert_eq!(h.log[1].1, 2);
        let mut h = Host::default();
        h.answer(Op::TargetExists, 0, ok());
        stop(&mut h, &mut s, &params(kind::MORALE, 0));
        assert_eq!(h.log[1].1, 3);
        s.bool1 = false;
        let mut h = Host::default();
        h.answer(Op::TargetExists, 0, ok());
        stop(&mut h, &mut s, &params(kind::MORALE, 0));
        assert!(!h.ops().contains(&(Op::ClassStep, 3)));
    }

    #[test]
    fn the_time_warp_returns_aircraft_circling() {
        let mut h = Host::default();
        h.answer(Op::TargetComplexAlive, 0, ok());
        h.answer(Op::ClassStep, 3, value(CATEGORY_PLANET_SPACESHIP));
        let mut s = EeCalamityState { effect_handle: 4, ..Default::default() };
        stop(&mut h, &mut s, &params(kind::TIME_WARP, 0));
        let action = h.log.iter().find(|l| l.0 == Op::ClassStep && l.1 == 4).unwrap();
        assert_eq!(action.2, ACTION_CIRCLE);
        assert_eq!(s.effect_handle, 0);
        let mut h = Host::default();
        h.answer(Op::TargetComplexAlive, 0, ok());
        h.answer(Op::ClassStep, 3, value(1));
        stop(&mut h, &mut s, &params(kind::TIME_WARP, 0));
        assert_eq!(h.log.iter().find(|l| l.0 == Op::ClassStep && l.1 == 4).unwrap().2, ACTION_IDLE);
        // a dead target: marked, nothing restored
        let mut h = Host::default();
        stop(&mut h, &mut s, &params(kind::TIME_WARP, 0));
        assert_eq!(s.int2, -1);
        assert_eq!(h.ops(), vec![(Op::TargetComplexAlive, 0), (Op::StopSound, 0), (Op::StopSound, 1)]);
        // the start: a dead target marks the calamity and fails
        let mut h = Host::default();
        h.answer(Op::TargetExists, 0, ok());
        let mut s = EeCalamityState::default();
        assert!(!start(&mut h, &mut s, &params(kind::TIME_WARP, 0)));
        assert_eq!(s.int2, -1);
    }

    #[test]
    fn the_line_of_sight_adds_and_takes_back_its_bonus() {
        let p = EeCalamityParams { sight_bonus: 3.0, ..params(kind::LINE_OF_SIGHT, 0) };
        let mut h = Host::default();
        assert!(!start(&mut h, &mut EeCalamityState::default(), &p));
        h.answer(Op::CasterExists, 0, ok());
        h.answer(Op::CasterSight, 0, EeCalamityReply { x: 8.0, ..Default::default() });
        assert!(start(&mut h, &mut EeCalamityState::default(), &p));
        assert_eq!(h.log.last().unwrap().3, 11.0);
        // a dead caster keeps it
        h.log.clear();
        stop(&mut h, &mut EeCalamityState::default(), &p);
        assert_eq!(h.ops(), vec![(Op::CasterExists, 0), (Op::CasterHitPoints, 0)]);
        h.answer(Op::CasterHitPoints, 0, value(1));
        h.log.clear();
        stop(&mut h, &mut EeCalamityState::default(), &p);
        assert_eq!(h.log.last().unwrap().3, 5.0);
    }

    #[test]
    fn the_logic_bomb_spares_a_protected_target_and_gives_wonders_back() {
        let p = params(kind::LOGIC_BOMB, EE_CALAMITY_EFFECT1);
        let mut h = Host::default();
        h.answer(Op::TargetExists, 0, ok());
        h.answer(Op::TargetProtected, 0, ok());
        assert!(!start(&mut h, &mut EeCalamityState::default(), &p));
        h.answer(Op::TargetProtected, 0, EeCalamityReply::default());
        h.answer(Op::CasterExists, 0, ok());
        h.answer(Op::ClassStep, 1, EeCalamityReply { x: 1.0, y: 2.0, z: 3.0, ..Default::default() });
        h.log.clear();
        assert!(start(&mut h, &mut EeCalamityState::default(), &p));
        let effect = h.log.iter().find(|l| l.0 == Op::ClassStep && l.1 == 4).unwrap();
        assert_eq!((effect.3, effect.4, effect.5), (1.0, 2.0, 3.0));
        let sound = h.log.last().unwrap();
        assert_eq!((sound.0, sound.3), (Op::StartSound, 1.0));
        h.answer(Op::ClassStep, 2, value(4));
        h.log.clear();
        stop(&mut h, &mut EeCalamityState::default(), &p);
        assert!(h.ops().contains(&(Op::ClassStep, 3)));
    }

    #[test]
    fn conscription_pays_only_for_a_living_target() {
        let p = params(kind::CONSCRIPTION, 0);
        let mut h = Host::default();
        h.answer(Op::TargetExists, 0, ok());
        h.answer(Op::TargetHitPoints, 0, value(5));
        assert!(start(&mut h, &mut EeCalamityState::default(), &p));
        assert!(!h.ops().contains(&(Op::ClassStep, 2)));
        h.answer(Op::ClassStep, 1, ok());
        h.log.clear();
        assert!(start(&mut h, &mut EeCalamityState::default(), &p));
        assert!(h.ops().contains(&(Op::ClassStep, 2)));
        h.answer(Op::TargetHitPoints, 0, value(0));
        h.log.clear();
        assert!(start(&mut h, &mut EeCalamityState::default(), &p));
        assert!(!h.ops().contains(&(Op::ClassStep, 1)));
    }
}
