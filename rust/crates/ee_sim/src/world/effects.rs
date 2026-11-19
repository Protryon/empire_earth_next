//! The graphic effects as the simulation asks for them: a narrow interface over the effect manager, which lives in
//! ee_render (ee_gfx ffi_manager.rs) where ee_sim may not reach.
//!
//! The units, their goals and the calamities make, find and destroy effects (`gEffectManager->CreateUnitEffect`,
//! `GetEffect`, `DestroyEffect`, ...: 60 calls in their C++). A body that moves into ee_sim makes those calls through
//! [`effects`]: the [`Effects`] the game registered at start-up (ee_game's `ee_rust_init` registers ee_render's,
//! `ee_render::gfx::sim_effects`, which works on the game's current manager), or [`NoEffects`] before that and in
//! tests that register none. The interface speaks in handles ([`EffectHandle`], `EEEffectHandle`'s u32), never in
//! effect pointers, so no reference into the renderer's objects lives in simulation code.
//!
//! The effects never feed back into the simulation's state except through these answers (the result of an entry
//! point, whether an effect lives and its kind), as in the C++.

use std::sync::OnceLock;

use ee_base::math::types::{Ge3DLine, Ge3DPoint, GeTransformation};

/// An effect's handle (`EEEffectHandle`: the effect record's u32; 0 is none, `kNoEffectHandle`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct EffectHandle(pub u32);

impl EffectHandle {
    /// `kNoEffectHandle`
    pub const NONE: EffectHandle = EffectHandle(0);

    pub fn is_none(self) -> bool {
        self.0 == 0
    }
}

/// Where an entry point puts the effect: one variant per `EEEffectManager::Create*` method, with its arguments.
/// Units are their addresses (`EEUnit*`), as the C++ passes them.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum EffectPlace<'a> {
    /// `CreateUnitEffect`: on a unit (the light glow keeps its unit; the particle emitter's tag point; the command
    /// confirmation's goal queue)
    Unit { unit: u64, keeps_unit: bool, tag_point: i32, goal_index: u32 },
    /// `CreateEffectBetweenUnits`: from a unit to a unit
    BetweenUnits { from: u64, to: u64 },
    /// `CreateEffect`: from a unit to a point
    UnitToPoint { unit: u64, at: Ge3DPoint, goal_index: u32 },
    /// `CreatePlacedEffect`: at a point
    Placed { at: Ge3DPoint },
    /// `CreateTransformationEffect`: at a transformation
    Transformation(GeTransformation),
    /// `CreateLineEffect`: along a line
    Line(Ge3DLine),
    /// `CreateEffectBetween`: between two points
    Between { from: Ge3DPoint, to: Ge3DPoint },
    /// `CreateWeatherEffect`: over tiles (x, y)
    Weather { tiles: &'a [[i32; 2]] },
}

/// One entry point's call: the graphic effect (`EEGraphicEffect*`, its address) and where it goes.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EffectRequest<'a> {
    pub graphic_effect: u64,
    pub place: EffectPlace<'a>,
}

/// What a `Create*` method answers: its result (10 for a kind it does not make or one without the living units it
/// needs, else the set-up's) and the new effect's handle ([`EffectHandle::NONE`] when it made none).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EffectCreated {
    pub result: i32,
    pub handle: EffectHandle,
}

/// The effect manager's entry points and the effect reads the simulation's code makes, on the game's current
/// manager (`gEffectManager`).
pub trait Effects: Sync {
    /// `Create*`: the entry point for the request's place.
    fn create(&self, request: &EffectRequest) -> EffectCreated;
    /// `DestroyEffect`: the handle queued for the manager's next update (nothing for an unknown handle).
    fn destroy(&self, handle: EffectHandle);
    /// `GetEffect(handle)->GetKind()`: the living effect's kind, `None` when no effect has the handle.
    fn kind(&self, handle: EffectHandle) -> Option<i32>;
    /// `GetEffect(handle)->SetLocalTo(player)`: shown only to the player and not saved; false when no effect has
    /// the handle.
    fn set_local_to(&self, handle: EffectHandle, player: i32) -> bool;
    /// The damage controller's finished flag (`mRecord->damaged.finished = true`: the fire put out); false when no
    /// effect has the handle.
    fn finish_damaged(&self, handle: EffectHandle) -> bool;

    /// `GetEffect(handle) != NULL`
    fn exists(&self, handle: EffectHandle) -> bool {
        self.kind(handle).is_some()
    }
    /// `EEGraphicEffect::Get(index)`: the graphic effects table's entry (an `EEGraphicEffect*`; 0 past the table and
    /// before it is loaded)
    fn graphic_effect(&self, index: u32) -> u64 {
        let _ = index;
        0
    }
}

/// The effects before any are registered (and in the tests that register none): every entry point makes nothing
/// (result 10, as the manager answers for a kind it does not make) and no handle names an effect.
#[derive(Clone, Copy, Debug, Default)]
pub struct NoEffects;

/// `EEEffectManager`'s result for an entry point that makes nothing.
pub const EFFECT_NOT_MADE: i32 = 10;

impl Effects for NoEffects {
    fn create(&self, _: &EffectRequest) -> EffectCreated {
        EffectCreated { result: EFFECT_NOT_MADE, handle: EffectHandle::NONE }
    }
    fn destroy(&self, _: EffectHandle) {}
    fn kind(&self, _: EffectHandle) -> Option<i32> {
        None
    }
    fn set_local_to(&self, _: EffectHandle, _: i32) -> bool {
        false
    }
    fn finish_damaged(&self, _: EffectHandle) -> bool {
        false
    }
}

static EFFECTS: OnceLock<&'static dyn Effects> = OnceLock::new();

/// Registers the game's effects (once, at start-up; a second registration is ignored and answers false).
pub fn register(effects: &'static dyn Effects) -> bool {
    EFFECTS.set(effects).is_ok()
}

/// The registered effects, else [`NoEffects`].
pub fn effects() -> &'static dyn Effects {
    static NONE: NoEffects = NoEffects;
    EFFECTS.get().copied().unwrap_or(&NONE)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;
    use std::sync::Mutex;

    /// A fake manager: handles from 1, each effect's kind (the graphic effect's address here), its local player and
    /// finished flag; the destroyed handles; every call logged.
    #[derive(Default)]
    struct Fake {
        state: Mutex<FakeState>,
    }

    #[derive(Default)]
    struct FakeState {
        next: u32,
        effects: BTreeMap<u32, (i32, Option<i32>, bool)>,
        destroyed: Vec<u32>,
        log: Vec<String>,
    }

    impl Effects for Fake {
        fn create(&self, request: &EffectRequest) -> EffectCreated {
            let mut s = self.state.lock().unwrap();
            s.log.push(format!("{:?}", request.place));
            if request.graphic_effect == 0 {
                return EffectCreated { result: EFFECT_NOT_MADE, handle: EffectHandle::NONE };
            }
            s.next += 1;
            let h = s.next;
            s.effects.insert(h, (request.graphic_effect as i32, None, false));
            EffectCreated { result: 0, handle: EffectHandle(h) }
        }
        fn destroy(&self, handle: EffectHandle) {
            let mut s = self.state.lock().unwrap();
            if s.effects.remove(&handle.0).is_some() {
                s.destroyed.push(handle.0);
            }
        }
        fn kind(&self, handle: EffectHandle) -> Option<i32> {
            self.state.lock().unwrap().effects.get(&handle.0).map(|e| e.0)
        }
        fn set_local_to(&self, handle: EffectHandle, player: i32) -> bool {
            let mut s = self.state.lock().unwrap();
            s.effects.get_mut(&handle.0).map(|e| e.1 = Some(player)).is_some()
        }
        fn finish_damaged(&self, handle: EffectHandle) -> bool {
            let mut s = self.state.lock().unwrap();
            s.effects.get_mut(&handle.0).map(|e| e.2 = true).is_some()
        }
    }

    // a unit body's use: the fire put out (EEContagionManager's): finished when it is the damage controller, else
    // destroyed; the handle cleared
    fn put_out(e: &dyn Effects, fire: &mut EffectHandle, damaged_kind: i32) {
        if fire.is_none() {
            return;
        }
        match e.kind(*fire) {
            Some(k) if k == damaged_kind => {
                e.finish_damaged(*fire);
            }
            Some(_) => e.destroy(*fire),
            None => {}
        }
        *fire = EffectHandle::NONE;
    }

    #[test]
    fn no_effects_make_nothing() {
        let e = NoEffects;
        assert_eq!(e.graphic_effect(15), 0);
        let r = e.create(&EffectRequest { graphic_effect: 5, place: EffectPlace::Placed { at: Ge3DPoint::default() } });
        assert_eq!(r, EffectCreated { result: EFFECT_NOT_MADE, handle: EffectHandle::NONE });
        assert_eq!(e.kind(EffectHandle(1)), None);
        assert!(!e.exists(EffectHandle(1)));
        assert!(!e.set_local_to(EffectHandle(1), 2));
        assert!(!e.finish_damaged(EffectHandle(1)));
        e.destroy(EffectHandle(1));
    }

    #[test]
    fn a_fake_answers_by_handle() {
        let f = Fake::default();
        let tiles = [[1, 2], [3, 4]];
        let places = [
            EffectPlace::Unit { unit: 0x10, keeps_unit: true, tag_point: 2, goal_index: 1 },
            EffectPlace::BetweenUnits { from: 0x10, to: 0x20 },
            EffectPlace::UnitToPoint { unit: 0x10, at: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, goal_index: 0 },
            EffectPlace::Transformation(GeTransformation::default()),
            EffectPlace::Line(Ge3DLine::default()),
            EffectPlace::Between { from: Ge3DPoint::default(), to: Ge3DPoint { x: 1.0, y: 0.0, z: 0.0 } },
            EffectPlace::Weather { tiles: &tiles },
        ];
        let handles: Vec<EffectHandle> =
            places.iter().map(|&place| f.create(&EffectRequest { graphic_effect: 0x30, place }).handle).collect();
        assert_eq!(handles, (1..=7).map(EffectHandle).collect::<Vec<_>>());
        assert_eq!(f.state.lock().unwrap().log.len(), 7);
        assert!(f.state.lock().unwrap().log[6].contains("[1, 2], [3, 4]"));
        // no graphic effect: nothing made
        let none =
            f.create(&EffectRequest { graphic_effect: 0, place: EffectPlace::Placed { at: Ge3DPoint::default() } });
        assert_eq!(none, EffectCreated { result: EFFECT_NOT_MADE, handle: EffectHandle::NONE });
        assert!(f.set_local_to(handles[0], 3));
        assert_eq!(f.state.lock().unwrap().effects[&1].1, Some(3));
        assert_eq!(f.kind(handles[1]), Some(0x30));
        assert!(!f.exists(EffectHandle(99)));
    }

    #[test]
    fn a_body_over_the_interface() {
        let f = Fake::default();
        let damaged =
            f.create(&EffectRequest { graphic_effect: 0x3e, place: EffectPlace::Placed { at: Ge3DPoint::default() } });
        let burning =
            f.create(&EffectRequest { graphic_effect: 0x40, place: EffectPlace::Placed { at: Ge3DPoint::default() } });
        let mut fire = damaged.handle;
        put_out(&f, &mut fire, 0x3e);
        assert_eq!(fire, EffectHandle::NONE);
        assert!(f.state.lock().unwrap().effects[&damaged.handle.0].2);
        let mut fire = burning.handle;
        put_out(&f, &mut fire, 0x3e);
        assert_eq!(f.state.lock().unwrap().destroyed, vec![burning.handle.0]);
        assert!(!f.exists(burning.handle));
        // a gone effect: nothing, the handle cleared
        let mut fire = burning.handle;
        put_out(&f, &mut fire, 0x3e);
        assert!(fire.is_none());
    }

    #[test]
    fn registered_once() {
        static FAKE: OnceLock<Fake> = OnceLock::new();
        let fake: &'static Fake = FAKE.get_or_init(Fake::default);
        // the first registration wins (this crate's tests register no other), a second is ignored
        assert!(register(fake));
        assert!(!register(&NoEffects));
        let r = effects()
            .create(&EffectRequest { graphic_effect: 7, place: EffectPlace::Placed { at: Ge3DPoint::default() } });
        assert_eq!(r.result, 0);
        assert_eq!(effects().kind(r.handle), Some(7));
    }
}
