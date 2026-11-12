//! The classes' `Update` as Rust sequences (RUST_PLAN R5b.4 leaf 6, the twelfth round): which of a unit's per-tick
//! leaves run, in which order, decided over the unit store (the herd, the owner, the tile, the command, the goal), the
//! leaves themselves host calls (the physics step, the turrets, the timers, the animation, the sound, the attack frame,
//! the rotting countdown, the heal work, the regeneration). `EEUCLand::Update` (0x613abf) and `EEUCNaval::Update`
//! (0x61a8fe) so far; the host (EEUCLand.cpp) is one table for every class to come.
//!
//! The record is read again after the host calls that can change what it holds (the command after the animation, the
//! goal after the heal search), and never held across a host call. A unit with no record (a stepped unit killed
//! this tick: RUST_PLAN's tenth round) reads the same fields from the host.
//!
//! Determinism (RUST_PLAN 2.1): no arithmetic; the host calls in the original's order, so the random draws they make
//! keep their order.

use std::ffi::c_void;

use crate::store::{with_store, EeUnitRecord};

// ---- the classes (`ee_units_update`'s `kind`)
pub const EE_UNITS_UPDATE_LAND: i32 = 1;
pub const EE_UNITS_UPDATE_NAVAL: i32 = 2;

// ---- the host's reads (`EeUnitsUpdateHost::read`)
/// `mPhysicalModel.GetFrame()`
pub const EE_UNITS_UPD_R_FRAME: i32 = 1;
/// `EEHerd::IsAnimalOnMap(inUnit)`
pub const EE_UNITS_UPD_R_ANIMAL_ON_MAP: i32 = 2;
/// the class's `mPhysicsModel != NULL`
pub const EE_UNITS_UPD_R_PHYSICS_MODEL: i32 = 3;
/// the class's `mIsMedic`
pub const EE_UNITS_UPD_R_MEDIC: i32 = 4;
/// `mStats->mRegeneration > 0`
pub const EE_UNITS_UPD_R_REGENERATES: i32 = 5;
/// the store's fields for a unit with no record: `mHerd != NULL`, `mPlayer->mPlayerID`, `mTileX`,
/// `mCommand ? mCommand->mType : 0`, `mGoalState ? mGoalState->mKind : -1`, the class's `mUnitType`
pub const EE_UNITS_UPD_R_HERD: i32 = 10;
pub const EE_UNITS_UPD_R_OWNER: i32 = 11;
pub const EE_UNITS_UPD_R_TILE_X: i32 = 12;
pub const EE_UNITS_UPD_R_COMMAND: i32 = 13;
pub const EE_UNITS_UPD_R_GOAL_KIND: i32 = 14;
pub const EE_UNITS_UPD_R_UNIT_TYPE: i32 = 15;

// ---- the host's operations (`EeUnitsUpdateHost::act`)
/// `SetLastHitPoints(mHitPoints)`
pub const EE_UNITS_UPD_LAST_HIT_POINTS: i32 = 20;
/// `mPhysicsModel->Step(inUnit)`, `UpdateTurrets()`, `UpdateWheels()`, `UpdateOrbitals()`
pub const EE_UNITS_UPD_PHYSICS_STEP: i32 = 21;
pub const EE_UNITS_UPD_TURRETS: i32 = 22;
pub const EE_UNITS_UPD_WHEELS: i32 = 23;
pub const EE_UNITS_UPD_ORBITALS: i32 = 24;
/// the class's `UpdateEffects(inUnit)`
pub const EE_UNITS_UPD_EFFECTS: i32 = 25;
/// `UpdateReloadTimer()`, `UpdateTargetSearchTimer()`, the class's `UpdateCamouflageTimer(inUnit)`
pub const EE_UNITS_UPD_RELOAD_TIMER: i32 = 26;
pub const EE_UNITS_UPD_TARGET_SEARCH_TIMER: i32 = 27;
pub const EE_UNITS_UPD_CAMOUFLAGE_TIMER: i32 = 28;
/// the class's `Animate(inUnit)`: its result
pub const EE_UNITS_UPD_ANIMATE: i32 = 29;
/// the class's `RefreshSound(inUnit)`, `RegeneratePower(inUnit)`; `UpdateFanaticismColor()`
pub const EE_UNITS_UPD_REFRESH_SOUND: i32 = 30;
pub const EE_UNITS_UPD_REGENERATE_POWER: i32 = 31;
pub const EE_UNITS_UPD_FANATICISM_COLOR: i32 = 32;
/// the class's `FireOnAttackFrame(inUnit, a: the animation's result, b: the frame)`
pub const EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME: i32 = 33;
/// `UpdateRottingAwayTime()`, `UpdateHealSearchTimer()`, `mGoalState->Work(inUnit)`, the class's
/// `Regenerate(inUnit)`, `UpdateDone(inUnit)`, `UpdateAnimal(inUnit)`, `UpdateResourceUnit(inUnit)`
pub const EE_UNITS_UPD_ROTTING: i32 = 34;
pub const EE_UNITS_UPD_HEAL_SEARCH_TIMER: i32 = 35;
pub const EE_UNITS_UPD_GOAL_WORK: i32 = 36;
pub const EE_UNITS_UPD_REGENERATE: i32 = 37;
pub const EE_UNITS_UPD_DONE: i32 = 38;
pub const EE_UNITS_UPD_ANIMAL: i32 = 39;
pub const EE_UNITS_UPD_RESOURCE_UNIT: i32 = 40;

// the original's constants (ee/EEUnitAction.h, ee/EEUnitGoal.h, ee/EEUnitClassIDs.h)
const ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
const ACTION_ATTACK_LOCATION: i64 = 2002;
const ACTION_WAIT_FOR_RELOAD: i64 = 2021;
const ACTION_FACE_LOCATION: i64 = 2027;
const ACTION_UNIT_ROTTING: i64 = 6002;
const GOAL_HEAL_UNIT: i64 = 13;
const UNIT_TYPE_RES_FISH: i64 = 0x13b;
/// `mTileX` of a unit off the map
const OFF_MAP: i64 = -10000;

/// The C++ side of a unit's `Update` (`ctx`: the host's: the class and the unit).
#[repr(C)]
pub struct EeUnitsUpdateHost {
    pub ctx: *mut c_void,
    pub read: extern "C" fn(ctx: *mut c_void, code: i32) -> i64,
    pub act: extern "C" fn(ctx: *mut c_void, code: i32, a: i64, b: i64) -> i64,
}

/// The calls as a trait (the FFI table; the tests' mock).
pub trait UpdateHost {
    fn read(&mut self, code: i32) -> i64;
    fn act(&mut self, code: i32, a: i64, b: i64) -> i64;
}

impl UpdateHost for &EeUnitsUpdateHost {
    fn read(&mut self, code: i32) -> i64 {
        (self.read)(self.ctx, code)
    }
    fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
        (self.act)(self.ctx, code, a, b)
    }
}

/// A unit's store fields, read at a point of the sequence: the record (`record`, a copy) or the host's.
struct UpdateReads<'a> {
    host: &'a mut dyn UpdateHost,
    record: &'a mut dyn FnMut() -> Option<EeUnitRecord>,
}

impl UpdateReads<'_> {
    fn field(&mut self, code: i32) -> i64 {
        match (self.record)() {
            Some(r) => match code {
                EE_UNITS_UPD_R_HERD => i64::from(r.herd),
                EE_UNITS_UPD_R_OWNER => i64::from(r.owner),
                EE_UNITS_UPD_R_TILE_X => r.tile_x,
                EE_UNITS_UPD_R_COMMAND => r.command,
                EE_UNITS_UPD_R_GOAL_KIND => i64::from(r.goal_kind),
                EE_UNITS_UPD_R_UNIT_TYPE => r.unit_type,
                _ => self.host.read(code),
            },
            None => self.host.read(code),
        }
    }
    fn read(&mut self, code: i32) -> i64 {
        self.host.read(code)
    }
    fn act(&mut self, code: i32) -> i64 {
        self.host.act(code, 0, 0)
    }
    /// the physics step when the class has a model and the unit is on the map (`turrets`: then the turrets too)
    fn step(&mut self, turrets: bool) {
        if self.read(EE_UNITS_UPD_R_PHYSICS_MODEL) != 0 && self.field(EE_UNITS_UPD_R_TILE_X) != OFF_MAP {
            self.act(EE_UNITS_UPD_PHYSICS_STEP);
            if turrets {
                self.act(EE_UNITS_UPD_TURRETS);
            }
        }
    }
    /// the command's attack frame (`face`: the face-location command fires too) or rotting countdown
    fn attack_or_rot(&mut self, attack: i64, frame: i64, face: bool) {
        match self.field(EE_UNITS_UPD_R_COMMAND) {
            ACTION_ATTACK_ENEMY_UNIT | ACTION_ATTACK_LOCATION | ACTION_WAIT_FOR_RELOAD => {
                self.host.act(EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME, attack, frame);
            }
            ACTION_FACE_LOCATION if face => {
                self.host.act(EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME, attack, frame);
            }
            ACTION_UNIT_ROTTING => {
                self.act(EE_UNITS_UPD_ROTTING);
            }
            _ => {}
        }
    }
}

/// `EEUCLand::Update`: a herd member, nature's unit (player 0) or a map animal updates as an animal; else the
/// physics step and turrets, the wheels, effects, the timers, the animation, sound, power, the fanaticism colour, the
/// attack frame or the rotting, a medic's heal search and work, the regeneration, and the class's last call.
fn land(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    if u.field(EE_UNITS_UPD_R_HERD) != 0
        || u.field(EE_UNITS_UPD_R_OWNER) == 0
        || u.read(EE_UNITS_UPD_R_ANIMAL_ON_MAP) != 0
    {
        u.act(EE_UNITS_UPD_ANIMAL);
        return;
    }
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(true);
    u.act(EE_UNITS_UPD_WHEELS);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.act(EE_UNITS_UPD_RELOAD_TIMER);
    u.act(EE_UNITS_UPD_TARGET_SEARCH_TIMER);
    u.act(EE_UNITS_UPD_CAMOUFLAGE_TIMER);
    let attack = u.act(EE_UNITS_UPD_ANIMATE);
    u.act(EE_UNITS_UPD_REFRESH_SOUND);
    u.act(EE_UNITS_UPD_REGENERATE_POWER);
    u.act(EE_UNITS_UPD_FANATICISM_COLOR);
    u.attack_or_rot(attack, frame, true);
    if u.read(EE_UNITS_UPD_R_MEDIC) != 0 {
        u.act(EE_UNITS_UPD_HEAL_SEARCH_TIMER);
        if u.field(EE_UNITS_UPD_R_GOAL_KIND) == GOAL_HEAL_UNIT {
            u.act(EE_UNITS_UPD_GOAL_WORK);
        }
    }
    if u.read(EE_UNITS_UPD_R_REGENERATES) != 0 {
        u.act(EE_UNITS_UPD_REGENERATE);
    }
    u.act(EE_UNITS_UPD_DONE);
}

/// `EEUCNaval::Update`: a fish updates as a resource; else the step and turrets, the orbitals, effects, the timers,
/// the animation, sound, the attack frame or the rotting, and the class's last call.
fn naval(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    if u.field(EE_UNITS_UPD_R_UNIT_TYPE) == UNIT_TYPE_RES_FISH {
        u.act(EE_UNITS_UPD_RESOURCE_UNIT);
        return;
    }
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(true);
    u.act(EE_UNITS_UPD_ORBITALS);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.act(EE_UNITS_UPD_RELOAD_TIMER);
    u.act(EE_UNITS_UPD_TARGET_SEARCH_TIMER);
    u.act(EE_UNITS_UPD_CAMOUFLAGE_TIMER);
    let attack = u.act(EE_UNITS_UPD_ANIMATE);
    u.act(EE_UNITS_UPD_REFRESH_SOUND);
    u.attack_or_rot(attack, frame, false);
    u.act(EE_UNITS_UPD_DONE);
}

/// A class's `Update` (`kind`: `EE_UNITS_UPDATE_*`) over `record` (the unit's record now, or none). False for a kind
/// it does not know (nothing called).
pub fn update(kind: i32, host: &mut impl UpdateHost, record: &mut dyn FnMut() -> Option<EeUnitRecord>) -> bool {
    let mut u = UpdateReads { host, record };
    match kind {
        EE_UNITS_UPDATE_LAND => land(&mut u),
        EE_UNITS_UPDATE_NAVAL => naval(&mut u),
        _ => return false,
    }
    true
}

/// A class's `Update` of the unit `id` (`object`: its address, the record's check) over the store and `host`.
///
/// # Safety
/// `host` points to a valid table for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_update(kind: i32, id: u32, object: u64, host: *const EeUnitsUpdateHost) -> bool {
    // SAFETY: the caller's contract
    let mut h = unsafe { &*host };
    let mut record = || with_store(|s| crate::step::record_of(s, id, object).copied());
    update(kind, &mut h, &mut record)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// A host that logs its operations and answers reads from a map (0 for others; the animation answers 77).
    #[derive(Default)]
    struct Mock {
        reads: BTreeMap<i32, i64>,
        log: Vec<(i32, i64, i64)>,
        /// a command the animation switches to (the record's, read after it)
        after_animate: Option<i64>,
    }

    impl UpdateHost for Mock {
        fn read(&mut self, code: i32) -> i64 {
            *self.reads.get(&code).unwrap_or(&0)
        }
        fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
            self.log.push((code, a, b));
            if code == EE_UNITS_UPD_ANIMATE {
                if let Some(c) = self.after_animate {
                    self.reads.insert(EE_UNITS_UPD_R_COMMAND, c);
                }
                77
            } else {
                0
            }
        }
    }

    fn ops(m: &Mock) -> Vec<i32> {
        m.log.iter().map(|l| l.0).collect()
    }

    fn run(kind: i32, m: &mut Mock) {
        assert!(update(kind, m, &mut || None));
    }

    fn soldier() -> Mock {
        let mut m = Mock::default();
        m.reads.insert(EE_UNITS_UPD_R_OWNER, 2);
        m.reads.insert(EE_UNITS_UPD_R_PHYSICS_MODEL, 1);
        m.reads.insert(EE_UNITS_UPD_R_FRAME, 9);
        m
    }

    #[test]
    fn a_land_unit_runs_its_leaves_in_order() {
        let mut m = soldier();
        run(EE_UNITS_UPDATE_LAND, &mut m);
        assert_eq!(
            ops(&m),
            vec![
                EE_UNITS_UPD_LAST_HIT_POINTS,
                EE_UNITS_UPD_PHYSICS_STEP,
                EE_UNITS_UPD_TURRETS,
                EE_UNITS_UPD_WHEELS,
                EE_UNITS_UPD_EFFECTS,
                EE_UNITS_UPD_RELOAD_TIMER,
                EE_UNITS_UPD_TARGET_SEARCH_TIMER,
                EE_UNITS_UPD_CAMOUFLAGE_TIMER,
                EE_UNITS_UPD_ANIMATE,
                EE_UNITS_UPD_REFRESH_SOUND,
                EE_UNITS_UPD_REGENERATE_POWER,
                EE_UNITS_UPD_FANATICISM_COLOR,
                EE_UNITS_UPD_DONE,
            ]
        );
    }

    #[test]
    fn the_command_after_the_animation_picks_the_attack_frame() {
        let mut m = soldier();
        m.after_animate = Some(ACTION_FACE_LOCATION);
        run(EE_UNITS_UPDATE_LAND, &mut m);
        assert!(m.log.contains(&(EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME, 77, 9)));
        // the naval class does not fire on facing
        let mut m = soldier();
        m.after_animate = Some(ACTION_FACE_LOCATION);
        run(EE_UNITS_UPDATE_NAVAL, &mut m);
        assert!(!ops(&m).contains(&EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME));
        let mut m = soldier();
        m.after_animate = Some(ACTION_UNIT_ROTTING);
        run(EE_UNITS_UPDATE_NAVAL, &mut m);
        assert_eq!(ops(&m)[ops(&m).len() - 2], EE_UNITS_UPD_ROTTING);
    }

    #[test]
    fn animals_medics_and_the_off_map() {
        // a herd member, nature's unit, a map animal: the animal update only
        for (code, value) in [(EE_UNITS_UPD_R_HERD, 1), (EE_UNITS_UPD_R_OWNER, 0), (EE_UNITS_UPD_R_ANIMAL_ON_MAP, 1)] {
            let mut m = soldier();
            m.reads.insert(code, value);
            run(EE_UNITS_UPDATE_LAND, &mut m);
            assert_eq!(ops(&m), vec![EE_UNITS_UPD_ANIMAL]);
        }
        // off the map: no step; a medic healing: the search and the work; regeneration
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_TILE_X, OFF_MAP);
        m.reads.insert(EE_UNITS_UPD_R_MEDIC, 1);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_HEAL_UNIT);
        m.reads.insert(EE_UNITS_UPD_R_REGENERATES, 1);
        run(EE_UNITS_UPDATE_LAND, &mut m);
        let o = ops(&m);
        assert!(!o.contains(&EE_UNITS_UPD_PHYSICS_STEP));
        assert_eq!(
            o[o.len() - 4..].to_vec(),
            vec![EE_UNITS_UPD_HEAL_SEARCH_TIMER, EE_UNITS_UPD_GOAL_WORK, EE_UNITS_UPD_REGENERATE, EE_UNITS_UPD_DONE]
        );
        // a fish
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_RES_FISH);
        run(EE_UNITS_UPDATE_NAVAL, &mut m);
        assert_eq!(ops(&m), vec![EE_UNITS_UPD_RESOURCE_UNIT]);
        assert!(!update(99, &mut soldier(), &mut || None));
    }

    #[test]
    fn the_record_answers_the_store_fields() {
        let mut m = soldier();
        // the record says: in a herd
        let rec = EeUnitRecord { herd: 1, owner: 2, ..Default::default() };
        assert!(update(EE_UNITS_UPDATE_LAND, &mut m, &mut || Some(rec)));
        assert_eq!(ops(&m), vec![EE_UNITS_UPD_ANIMAL]);
    }
}
