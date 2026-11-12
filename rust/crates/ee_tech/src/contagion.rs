//! The contagion helpers and their functors (`src/Empire Earth/Calamities/EEContagionHelper.cpp`, `EE*Functor.cpp`):
//! when a helper spreads, which units near its held ones it infects (the functor's type test, the allies-only test,
//! the chance's draw and the radius, in the C++'s short-circuit order), which of them it takes, which held units it
//! damages or drops; and the functors' damage arithmetic (`Infect`, `Apply`) and the calamity's discrete updates.
//!
//! The helper's unit list, the units and every world step stay C++: the update asks through a host
//! ([`ContagionHost`]) that `EEContagionHelper.cpp` answers on the helper. Its two timers travel as
//! [`EeContagionState`].

// the C++'s own tests, kept as written
#![allow(clippy::neg_cmp_op_on_partial_ord)]

use ee_ge::um::lrintf;

/// The functor's answers an update reads (all plain getters, read once before it).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeContagionParams {
    /// `GetSpreadTime()`.
    pub spread_time: u32,
    /// `GetChance()`.
    pub chance: f32,
    /// `GetRadius()`.
    pub radius: f32,
    /// `EE_CONTAGION_*` bits.
    pub flags: u32,
}

/// [`EeContagionParams::flags`]: the functor skips the spread (`SkipsSpread()`: it updates its units instead).
pub const EE_CONTAGION_SKIPS_SPREAD: u32 = 1;
/// The functor infects allies only (`InfectsAlliesOnly()`).
pub const EE_CONTAGION_ALLIES_ONLY: u32 = 2;
/// The calamity's class is blocked by immunity (`mIsBlockedByImmunity`).
pub const EE_CONTAGION_BLOCKED_BY_IMMUNITY: u32 = 4;

/// The helper's timers (`mTimeSinceLastSpread`, `mTimeSinceLastUpdate`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeContagionState {
    pub since_spread: f32,
    pub since_update: f32,
}

/// The host calls (`EeContagionCall::op`).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeContagionOp {
    /// The functor's `UpdateUnits(&mUnits)` (a functor that skips the spread).
    UpdateUnits = 1,
    /// The functor's `Update(x, calamity)`.
    FunctorUpdate = 2,
    /// The helper's unit count: value.
    UnitCount = 3,
    /// Held unit `index` is alive: ok.
    UnitAlive = 4,
    /// The functor's `Apply(held unit index, x, calamity)`.
    Apply = 5,
    /// Held unit `index` removed and cured (`RemoveUnit(iterator, true)`).
    Remove = 6,
    /// Held unit `index` is a complex unit: ok.
    HeldComplex = 7,
    /// `GetUnitsInArea(x, held unit index)` into the near list (replacing the last): value its size.
    NearUnits = 8,
    /// Near unit `index` against held unit `index2`: flags `EE_CONTAGION_NEAR_*`, id its unit id.
    NearView = 9,
    /// `gPhysics->mRandom.Generate()`: x.
    Random = 10,
    /// The distance squared from held unit `index2` to near unit `index`: x.
    DistanceSquared = 11,
    /// Near unit `index` added to the new units.
    AddNew = 12,
    /// New unit `index`: flags `EE_CONTAGION_NEW_*`, id its unit id.
    NewView = 13,
    /// New unit `index`'s immunity (effect kind 3): ok when it has one (the host plays its effect).
    Immune = 14,
    /// New unit `index` added and infected (`AddUnit(unit, true)`).
    AddUnit = 15,
    /// The capture's record of a step (TEST ONLY, `EECalamityCaptureContagionEvent(value, index, index2, x, y)`).
    Capture = 16,
}

/// [`EeContagionOp::NearView`]: the list's entry is a unit.
pub const EE_CONTAGION_NEAR_UNIT: u32 = 1;
/// The functor's `CanInfectType` accepts its category and type.
pub const EE_CONTAGION_NEAR_TYPE: u32 = 2;
/// It is the held unit's player's.
pub const EE_CONTAGION_NEAR_SAME_PLAYER: u32 = 4;
/// Its player is no enemy of the held unit's player (`mEnemies[...] == 0`).
pub const EE_CONTAGION_NEAR_NOT_ENEMY: u32 = 8;
/// [`EeContagionOp::NewView`]: the new unit is a complex unit.
pub const EE_CONTAGION_NEW_COMPLEX: u32 = 1;
/// The helper holds it already.
pub const EE_CONTAGION_NEW_HELD: u32 = 2;

/// The capture's step kinds (`EEContagionCaptureOp`, `ee/EETechCapture.h`).
mod capture {
    pub(super) const SPREAD: i32 = 1;
    pub(super) const CANDIDATE: i32 = 3;
    pub(super) const NEW: i32 = 4;
}

/// One host call.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeContagionCall {
    pub op: EeContagionOp,
    pub index: i32,
    pub index2: i32,
    pub value: i32,
    pub x: f32,
    pub y: f32,
}

impl EeContagionCall {
    fn new(op: EeContagionOp) -> Self {
        EeContagionCall { op, index: 0, index2: 0, value: 0, x: 0.0, y: 0.0 }
    }
}

/// The host's answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeContagionReply {
    pub ok: bool,
    pub pad: [u8; 3],
    pub flags: u32,
    pub value: i32,
    pub id: u32,
    pub x: f32,
}

/// The helper and its world, as the C++ answers (a test host in the tests).
pub trait ContagionHost {
    fn call(&mut self, call: &EeContagionCall) -> EeContagionReply;
}

fn ask<H: ContagionHost>(h: &mut H, op: EeContagionOp, index: i32) -> EeContagionReply {
    h.call(&EeContagionCall { index, ..EeContagionCall::new(op) })
}

fn note<H: ContagionHost>(h: &mut H, kind: i32, a: i32, b: i32, x: f32, y: f32) {
    h.call(&EeContagionCall { value: kind, index: a, index2: b, x, y, ..EeContagionCall::new(EeContagionOp::Capture) });
}

/// `EEContagionHelper::Update(elapsed)`: every spread time the helper spreads (or its functor updates its units),
/// then the functor updates and each held unit is damaged while it lives or dropped (cured) once dead.
pub fn update<H: ContagionHost>(h: &mut H, s: &mut EeContagionState, p: &EeContagionParams, elapsed: f32) {
    let spread_time = p.spread_time;
    s.since_spread += elapsed;
    // (the unsigned spread time is compared and subtracted as a float, as the C++'s conversions do)
    let spread = spread_time == 0 || s.since_spread > spread_time as f32;
    if spread {
        s.since_spread -= spread_time as f32;
        if p.flags & EE_CONTAGION_SKIPS_SPREAD == 0 {
            let since_update = s.since_update;
            spread_units(h, p, since_update);
        } else {
            ask(h, EeContagionOp::UpdateUnits, 0);
        }
        s.since_update = 0.0;
    } else {
        s.since_update += elapsed;
    }
    h.call(&EeContagionCall { x: elapsed, ..EeContagionCall::new(EeContagionOp::FunctorUpdate) });
    let mut i = 0;
    while i < ask(h, EeContagionOp::UnitCount, 0).value {
        if ask(h, EeContagionOp::UnitAlive, i).ok {
            h.call(&EeContagionCall { index: i, x: elapsed, ..EeContagionCall::new(EeContagionOp::Apply) });
            i += 1;
        } else {
            ask(h, EeContagionOp::Remove, i);
        }
    }
}

/// What [`infects`] asks for.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum InfectAsk {
    /// A draw (`gPhysics->mRandom.Generate()`).
    Draw,
    /// The distance squared from the held unit to the near unit.
    Distance,
}

/// Whether a near unit is infected (`Spread`'s test, its parts in the C++'s short-circuit order): the functor's
/// type test, the allies-only test, then a draw (only when both passed) against the chance, then the distance (only
/// when the draw passed) against the radius. The draw and the distance come from `ask`.
pub fn infects(
    near: u32,
    allies_only: bool,
    chance: f32,
    radius_squared: f32,
    mut ask: impl FnMut(InfectAsk) -> f32,
) -> InfectTest {
    let type_ok = near & EE_CONTAGION_NEAR_TYPE != 0;
    let ally_ok = type_ok
        && (!allies_only || near & EE_CONTAGION_NEAR_SAME_PLAYER != 0 || near & EE_CONTAGION_NEAR_NOT_ENEMY != 0);
    let mut test = InfectTest { type_ok, ally_ok, draw: -1.0, distance: -1.0, takes: false };
    if ally_ok {
        test.draw = ask(InfectAsk::Draw);
        if test.draw <= chance {
            test.distance = ask(InfectAsk::Distance);
            test.takes = test.distance <= radius_squared;
        }
    }
    test
}

/// [`infects`]'s answer, with what it asked.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct InfectTest {
    pub type_ok: bool,
    pub ally_ok: bool,
    /// The draw, -1 when none was made.
    pub draw: f32,
    /// The distance squared, -1 when it was not asked.
    pub distance: f32,
    pub takes: bool,
}

/// `EEContagionHelper::Spread(elapsed)`: each held complex unit may infect the units around it; the new ones are
/// taken after the walk (an immune one only shows its immunity when the class is blocked by it).
fn spread_units<H: ContagionHost>(h: &mut H, p: &EeContagionParams, elapsed: f32) {
    let chance = p.chance * (elapsed * 0.001);
    let radius = p.radius;
    let radius_squared = radius * radius;
    note(h, capture::SPREAD, 0, 0, elapsed, chance);
    let allies_only = p.flags & EE_CONTAGION_ALLIES_ONLY != 0;
    let mut new_units = 0;
    let held = ask(h, EeContagionOp::UnitCount, 0).value;
    for i in 0..held {
        if !ask(h, EeContagionOp::HeldComplex, i).ok {
            continue;
        }
        let near =
            h.call(&EeContagionCall { index: i, x: radius, ..EeContagionCall::new(EeContagionOp::NearUnits) }).value;
        for j in 0..near {
            let view =
                h.call(&EeContagionCall { index: j, index2: i, ..EeContagionCall::new(EeContagionOp::NearView) });
            if view.flags & EE_CONTAGION_NEAR_UNIT == 0 {
                continue;
            }
            let test = infects(view.flags, allies_only, chance, radius_squared, |what| match what {
                InfectAsk::Draw => ask(h, EeContagionOp::Random, 0).x,
                InfectAsk::Distance => {
                    h.call(&EeContagionCall {
                        index: j,
                        index2: i,
                        ..EeContagionCall::new(EeContagionOp::DistanceSquared)
                    })
                    .x
                }
            });
            let bits = test.type_ok as i32 | (test.ally_ok as i32) << 1 | (test.takes as i32) << 2;
            note(h, capture::CANDIDATE, view.id as i32, bits, test.draw, test.distance);
            if test.takes {
                ask(h, EeContagionOp::AddNew, j);
                new_units += 1;
            }
        }
    }
    for k in 0..new_units {
        let view = ask(h, EeContagionOp::NewView, k);
        if view.flags & EE_CONTAGION_NEW_COMPLEX == 0 || view.flags & EE_CONTAGION_NEW_HELD != 0 {
            note(h, capture::NEW, view.id as i32, 3, 0.0, 0.0);
            continue;
        }
        if p.flags & EE_CONTAGION_BLOCKED_BY_IMMUNITY != 0 && ask(h, EeContagionOp::Immune, k).ok {
            continue;
        }
        ask(h, EeContagionOp::AddUnit, k);
    }
}

/// `EECalamity::GetDiscreteUpdates`: the whole periods of `discrete` in the elapsed time (`UMFloatToLong`, stored
/// unsigned), and the time left (`elapsed -= updates * discrete`, the count converted to a float).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityDiscrete {
    pub updates: u32,
    pub elapsed: f32,
}

pub fn discrete_updates(elapsed: f32, discrete: f32) -> EeCalamityDiscrete {
    let mut out = EeCalamityDiscrete { updates: 0, elapsed };
    if discrete != 0.0 {
        out.updates = lrintf(elapsed / discrete) as u32;
        out.elapsed = elapsed - out.updates as f32 * discrete;
    }
    out
}

/// A functor's damage per update (`mDamage * mUpdates`: a long times an unsigned long, the 32-bit product's bits).
pub fn tick_damage(damage: i32, updates: u32) -> i32 {
    (damage as u32).wrapping_mul(updates) as i32
}

/// `EEFireFunctor::Infect`'s burn: `UMFloatToLong((mBurn * 0.01f) * mMaxHitPoints)`.
pub fn fire_burn(burn: i32, max_hit_points: i32) -> i32 {
    lrintf((burn as f32 * 0.01) * max_hit_points as f32)
}

/// `EELeprosyFunctor::Apply`'s damage: the tick's, never below 1 hit point left (the Windows `min`); 0 when it
/// would be 1 or less (no hit).
pub fn leprosy_damage(damage: i32, updates: u32, hit_points: i32) -> i32 {
    let tick = tick_damage(damage, updates);
    let left = hit_points.wrapping_sub(1);
    let damage = if tick < left { tick } else { left };
    if damage > 1 {
        damage
    } else {
        0
    }
}

/// `EEAreaEffectFunctor::Apply`'s damage: the tick's (converted as the unsigned product) times the unit's attack
/// stat, `UMFloatToLong`; 0 when it is 1 or less (no hit).
pub fn area_damage(damage: i32, updates: u32, attack: f32) -> i32 {
    let value = (damage as u32).wrapping_mul(updates) as f32 * attack;
    let damage = lrintf(value);
    if damage <= 1 {
        0
    } else {
        damage
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    #[derive(Default)]
    struct Host {
        log: Vec<(EeContagionOp, i32, i32, i32, f32, f32)>,
        answers: HashMap<(i32, i32), EeContagionReply>,
        units: i32,
        alive: Vec<bool>,
    }

    impl Host {
        fn answer(&mut self, op: EeContagionOp, index: i32, reply: EeContagionReply) {
            self.answers.insert((op as i32, index), reply);
        }
        fn ops(&self) -> Vec<(EeContagionOp, i32)> {
            self.log.iter().filter(|l| l.0 != EeContagionOp::Capture).map(|l| (l.0, l.1)).collect()
        }
    }

    impl ContagionHost for Host {
        fn call(&mut self, c: &EeContagionCall) -> EeContagionReply {
            self.log.push((c.op, c.index, c.index2, c.value, c.x, c.y));
            match c.op {
                EeContagionOp::UnitCount => EeContagionReply { value: self.units, ..Default::default() },
                EeContagionOp::UnitAlive => EeContagionReply { ok: self.alive[c.index as usize], ..Default::default() },
                EeContagionOp::Remove => {
                    self.alive.remove(c.index as usize);
                    self.units -= 1;
                    EeContagionReply::default()
                }
                _ => self.answers.get(&(c.op as i32, c.index)).copied().unwrap_or_default(),
            }
        }
    }

    #[test]
    fn the_spread_waits_for_its_time_then_drops_the_dead() {
        let p = EeContagionParams { spread_time: 1000, flags: EE_CONTAGION_SKIPS_SPREAD, ..Default::default() };
        let mut h = Host { units: 3, alive: vec![true, false, true], ..Default::default() };
        let mut s = EeContagionState { since_spread: 900.0, since_update: 50.0 };
        update(&mut h, &mut s, &p, 50.0);
        assert_eq!(s, EeContagionState { since_spread: 950.0, since_update: 100.0 });
        assert_eq!(
            h.ops(),
            vec![
                (EeContagionOp::FunctorUpdate, 0),
                (EeContagionOp::UnitCount, 0),
                (EeContagionOp::UnitAlive, 0),
                (EeContagionOp::Apply, 0),
                (EeContagionOp::UnitCount, 0),
                (EeContagionOp::UnitAlive, 1),
                (EeContagionOp::Remove, 1),
                (EeContagionOp::UnitCount, 0),
                (EeContagionOp::UnitAlive, 1),
                (EeContagionOp::Apply, 1),
                (EeContagionOp::UnitCount, 0),
            ]
        );
        // past the spread time: the functor's own update of its units, the update timer reset
        let mut h = Host { units: 0, ..Default::default() };
        update(&mut h, &mut s, &p, 100.0);
        assert_eq!(s, EeContagionState { since_spread: 50.0, since_update: 0.0 });
        assert_eq!(h.ops()[0], (EeContagionOp::UpdateUnits, 0));
        // a spread time of 0 spreads every update
        let p = EeContagionParams { spread_time: 0, ..p };
        let mut h = Host::default();
        update(&mut h, &mut s, &p, 1.0);
        assert_eq!(h.ops()[0], (EeContagionOp::UpdateUnits, 0));
    }

    #[test]
    fn the_test_draws_only_after_the_type_and_the_allies_pass() {
        let answers = |draw: f32, distance: f32| {
            move |what| match what {
                InfectAsk::Draw => draw,
                InfectAsk::Distance => distance,
            }
        };
        let never = |_| -> f32 { panic!("asked") };
        let t = infects(0, false, 0.5, 4.0, never);
        assert!(!t.type_ok && !t.takes);
        let t = infects(EE_CONTAGION_NEAR_TYPE, true, 0.5, 4.0, never);
        assert!(t.type_ok && !t.ally_ok);
        let t = infects(EE_CONTAGION_NEAR_TYPE | EE_CONTAGION_NEAR_NOT_ENEMY, true, 0.5, 4.0, |what| {
            assert_eq!(what, InfectAsk::Draw);
            0.6
        });
        assert_eq!((t.draw, t.distance, t.takes), (0.6, -1.0, false));
        let t = infects(EE_CONTAGION_NEAR_TYPE | EE_CONTAGION_NEAR_SAME_PLAYER, true, 0.5, 4.0, answers(0.5, 4.0));
        assert!(t.takes);
        let t = infects(EE_CONTAGION_NEAR_TYPE, false, 0.5, 4.0, answers(0.5, 4.5));
        assert!(!t.takes);
        // NaN never passes the draw
        let t = infects(EE_CONTAGION_NEAR_TYPE, false, f32::NAN, 4.0, answers(0.1, 0.0));
        assert!(!t.takes && t.distance == -1.0);
    }

    #[test]
    fn a_spread_takes_the_new_units_after_the_walk() {
        let p =
            EeContagionParams { spread_time: 100, chance: 2.0, radius: 3.0, flags: EE_CONTAGION_BLOCKED_BY_IMMUNITY };
        let mut h = Host { units: 2, alive: vec![true, true], ..Default::default() };
        h.answer(EeContagionOp::HeldComplex, 1, EeContagionReply { ok: true, ..Default::default() });
        h.answer(EeContagionOp::NearUnits, 1, EeContagionReply { value: 3, ..Default::default() });
        let near = |flags, id| EeContagionReply { flags, id, ..Default::default() };
        h.answer(EeContagionOp::NearView, 0, near(0, 0));
        h.answer(EeContagionOp::NearView, 1, near(EE_CONTAGION_NEAR_UNIT | EE_CONTAGION_NEAR_TYPE, 11));
        h.answer(EeContagionOp::NearView, 2, near(EE_CONTAGION_NEAR_UNIT | EE_CONTAGION_NEAR_TYPE, 12));
        h.answer(EeContagionOp::Random, 0, EeContagionReply { x: 0.25, ..Default::default() });
        h.answer(EeContagionOp::DistanceSquared, 1, EeContagionReply { x: 9.0, ..Default::default() });
        h.answer(EeContagionOp::DistanceSquared, 2, EeContagionReply { x: 9.5, ..Default::default() });
        h.answer(EeContagionOp::NewView, 0, EeContagionReply { flags: EE_CONTAGION_NEW_COMPLEX, ..Default::default() });
        let mut s = EeContagionState { since_spread: 100.0, since_update: 400.0 };
        update(&mut h, &mut s, &p, 1.0);
        // the chance 2 * 0.4 = 0.8 > 0.25; 9 <= 9 taken, 9.5 not
        let ops = h.ops();
        let spread: Vec<_> = ops.iter().take_while(|o| o.0 != EeContagionOp::FunctorUpdate).copied().collect();
        assert_eq!(
            spread,
            vec![
                (EeContagionOp::UnitCount, 0),
                (EeContagionOp::HeldComplex, 0),
                (EeContagionOp::HeldComplex, 1),
                (EeContagionOp::NearUnits, 1),
                (EeContagionOp::NearView, 0),
                (EeContagionOp::NearView, 1),
                (EeContagionOp::Random, 0),
                (EeContagionOp::DistanceSquared, 1),
                (EeContagionOp::AddNew, 1),
                (EeContagionOp::NearView, 2),
                (EeContagionOp::Random, 0),
                (EeContagionOp::DistanceSquared, 2),
                (EeContagionOp::NewView, 0),
                (EeContagionOp::Immune, 0),
                (EeContagionOp::AddUnit, 0),
            ]
        );
        let first = h.log.iter().find(|l| l.0 == EeContagionOp::Capture).unwrap();
        assert_eq!((first.3, first.4, first.5), (capture::SPREAD, 400.0, 2.0 * (400.0 * 0.001)));
        assert_eq!(s.since_update, 0.0);
    }

    #[test]
    fn the_functors_damage() {
        assert_eq!(discrete_updates(2500.0, 1000.0), EeCalamityDiscrete { updates: 2, elapsed: 500.0 });
        assert_eq!(discrete_updates(2500.0, 0.0), EeCalamityDiscrete { updates: 0, elapsed: 2500.0 });
        // lrintf: 2.5 to even
        assert_eq!(discrete_updates(2500.0, 1000.0).updates, 2);
        assert_eq!(discrete_updates(3500.0, 1000.0), EeCalamityDiscrete { updates: 4, elapsed: -500.0 });
        assert_eq!(tick_damage(7, 3), 21);
        assert_eq!(tick_damage(-2, 3), -6);
        assert_eq!(fire_burn(25, 130), 32); // 32.5 to even
        assert_eq!(fire_burn(30, 130), 39);
        assert_eq!(leprosy_damage(5, 2, 100), 10);
        assert_eq!(leprosy_damage(5, 2, 4), 3);
        assert_eq!(leprosy_damage(5, 2, 2), 0);
        assert_eq!(leprosy_damage(1, 1, 100), 0);
        assert_eq!(area_damage(3, 2, 0.5), 3);
        assert_eq!(area_damage(1, 3, 0.5), 2);
        assert_eq!(area_damage(1, 1, 0.5), 0);
        // a negative damage is the unsigned product's float
        assert_eq!(area_damage(-1, 1, 1.0e-9), 4);
    }
}
