//! `EEUnitClass`'s bodies over its C++ members (the units' lane, round 6; were EEUnitClass.cpp): `Heal`,
//! `UpdateBuilding`, `UpdateAnimal`, `RefreshSound`, `CreateDeathUnit`. Every C++ statement of the originals is one
//! operation of [`UcHost::uc`] (the game's: `ee_units_uc_host`, EEUnitClass.cpp, one statement a case), asked in
//! the original's order; the choices, loops and arithmetic are here. `class` is the `EEUnitClass*` the body runs on
//! (its virtuals dispatch on it), `unit` the `EEComplexUnit*` it is given, `a` an object, `b`, `c` values, `f`
//! floats in and out.

// ---- the statements (`op`; the answer)
/// `GetGoal()` as a heal goal (`EEHealGoalCast`; 0: none)
pub const EE_UNITS_UC_HEAL_GOAL: i32 = 1;
/// `unit->DropPath()`
pub const EE_UNITS_UC_DROP_PATH: i32 = 2;
/// `EECombatHealInRange(unit, heal goal a's GetHealTarget())`
pub const EE_UNITS_UC_HEAL_IN_RANGE: i32 = 3;
/// `unit->SetAnimation(b, -1, 0)`
pub const EE_UNITS_UC_SET_ANIMATION: i32 = 4;
/// `unit->mUnitClass->mHealEffect`
pub const EE_UNITS_UC_HEAL_EFFECT: i32 = 5;
/// effect `a`'s `mKind`
pub const EE_UNITS_UC_EFFECT_KIND: i32 = 6;
/// `CreateEffectBetweenUnits(unit, heal goal b's GetHealTarget(), effect a, &the handle) != 0`
pub const EE_UNITS_UC_HEAL_BETWEEN: i32 = 7;
/// `CreateUnitEffect(heal goal b's GetHealTarget(), effect a, &the handle, false, 0, 0) != 0`
pub const EE_UNITS_UC_HEAL_ON_TARGET: i32 = 8;
/// `unit->mHealEffect.mValue`
pub const EE_UNITS_UC_HEAL_HANDLE: i32 = 9;
/// `unit->mHealEffect.mValue = b`
pub const EE_UNITS_UC_SET_HEAL_HANDLE: i32 = 10;
/// `unit->mIsDisabled`
pub const EE_UNITS_UC_DISABLED: i32 = 11;
/// `unit->mHitPoints`
pub const EE_UNITS_UC_HIT_POINTS: i32 = 12;
/// `unit->mProductionTime`
pub const EE_UNITS_UC_PRODUCTION_TIME: i32 = 13;
/// `gInCinematic`
pub const EE_UNITS_UC_IN_CINEMATIC: i32 = 14;
/// `gPhysics->mTickTimeMs`
pub const EE_UNITS_UC_TICK_MS: i32 = 15;
/// `unit->mPlayer->mInstantBuild`
pub const EE_UNITS_UC_INSTANT_BUILD: i32 = 16;
/// `unit->mPlayer->mTechnologies->Get(unit->mCurrentResearch)->mData->mMakesUnit`
pub const EE_UNITS_UC_RESEARCH_MAKES_UNIT: i32 = 17;
/// `gUnitClassTable[unit->mPlayer->mTechnologies->Get(unit->mCurrentResearch)->mData->mUnitClass]`
pub const EE_UNITS_UC_RESEARCH_CLASS: i32 = 18;
/// class `a`'s `IsLandBuilding()`
pub const EE_UNITS_UC_IS_LAND_BUILDING: i32 = 19;
/// `class->ProduceUnit(unit, class a, true)`
pub const EE_UNITS_UC_PRODUCE_UNIT: i32 = 20;
/// `unit->mPlayer->mTechnologies->Research(unit->mCurrentResearch, unit)`
pub const EE_UNITS_UC_RESEARCH: i32 = 21;
/// `unit->PlayEffect(100000)`
pub const EE_UNITS_UC_PLAY_NO_EFFECT: i32 = 22;
/// `unit->SetProductionTime(b)`
pub const EE_UNITS_UC_SET_PRODUCTION_TIME: i32 = 23;
/// `unit->UpdateOrbitals()`
pub const EE_UNITS_UC_UPDATE_ORBITALS: i32 = 24;
/// `class->UpdateCamouflageTimer(unit)`
pub const EE_UNITS_UC_UPDATE_CAMOUFLAGE_TIMER: i32 = 25;
/// `class->UpdateEffects(unit)`
pub const EE_UNITS_UC_UPDATE_EFFECTS: i32 = 26;
/// `unit->mCommand != NULL ? unit->mCommand->mType : 0`
pub const EE_UNITS_UC_COMMAND_KIND: i32 = 27;
/// `unit->UpdateRottingAwayTime()`
pub const EE_UNITS_UC_UPDATE_ROTTING: i32 = 28;
/// unit `a`'s `mNextPart` (0: the unit's)
pub const EE_UNITS_UC_NEXT_PART: i32 = 29;
/// unit `a`'s `SetRottingAwayTime(b)`
pub const EE_UNITS_UC_SET_ROTTING: i32 = 30;
/// `unit->ContinueProduction()`
pub const EE_UNITS_UC_CONTINUE_PRODUCTION: i32 = 31;
/// `unit->mPhysicalModel.GetFrame()`
pub const EE_UNITS_UC_FRAME: i32 = 33;
/// `unit->SetLastHitPoints(unit->mHitPoints)`
pub const EE_UNITS_UC_KEEP_LAST_HIT_POINTS: i32 = 34;
/// `class->mPhysicsModel != NULL`
pub const EE_UNITS_UC_HAS_PHYSICS_MODEL: i32 = 35;
/// `unit->GetTileX()`
pub const EE_UNITS_UC_TILE_X: i32 = 36;
/// `class->mPhysicsModel->Step(unit)`
pub const EE_UNITS_UC_PHYSICS_STEP: i32 = 37;
/// `unit->UpdateReloadTimer()`
pub const EE_UNITS_UC_UPDATE_RELOAD_TIMER: i32 = 38;
/// `unit->UpdateTargetSearchTimer()`
pub const EE_UNITS_UC_UPDATE_TARGET_SEARCH_TIMER: i32 = 39;
/// `unit->GetCommandType()`
pub const EE_UNITS_UC_COMMAND_TYPE: i32 = 42;
/// `unit->UpdateResourcesRotting()`
pub const EE_UNITS_UC_UPDATE_RESOURCES_ROTTING: i32 = 44;
/// `class->UpdateDone(unit)`
pub const EE_UNITS_UC_UPDATE_DONE: i32 = 45;
/// `unit->mSound != NULL`
pub const EE_UNITS_UC_HAS_SOUND: i32 = 46;
/// `UIsNone32(gSoundManager->GetInstanceID(unit->mSound))`
pub const EE_UNITS_UC_SOUND_NONE: i32 = 47;
/// `gPhysics->mNumTicksDone`
pub const EE_UNITS_UC_TICKS_DONE: i32 = 48;
/// `unit->mUnitID`
pub const EE_UNITS_UC_UNIT_ID: i32 = 49;
/// `unit->mPhysicalModel.Intersects(gViewport, gOrthographicView)`
pub const EE_UNITS_UC_IN_VIEW: i32 = 50;
/// `gSoundManager->GetSampleID(unit->mSound)`
pub const EE_UNITS_UC_SOUND_SAMPLE: i32 = 51;
/// `unit->GetModel()->mSounds1[unit->GetAnimation()]`
pub const EE_UNITS_UC_MODEL_SOUND1: i32 = 52;
/// `unit->GetModel()->mSounds2[unit->mAnimation]`
pub const EE_UNITS_UC_MODEL_SOUND2: i32 = 53;
/// sound `a`'s `mSampleID`
pub const EE_UNITS_UC_SAMPLE_ID: i32 = 54;
/// `class->GetSoundParameters(unit, sound a, &f[0], &f[1], &f[2])`
pub const EE_UNITS_UC_SOUND_PARAMETERS: i32 = 55;
/// `gSoundManager->SetSoundParameters(unit->mSound, f[0], f[1], f[2])`
pub const EE_UNITS_UC_SET_SOUND_PARAMETERS: i32 = 56;
/// `class->mDeathSpawnClass`
pub const EE_UNITS_UC_DEATH_SPAWN_CLASS: i32 = 57;
/// `unit->mContainer`
pub const EE_UNITS_UC_CONTAINER: i32 = 58;
/// unit `a`'s location's X, Y, Z into `f[0..3]`
pub const EE_UNITS_UC_LOCATION: i32 = 59;
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_UC_UNIT_TILE_X: i32 = 60;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_UC_UNIT_TILE_Y: i32 = 61;
/// `class a->CreateUnit(b, c, unit->mPlayer->GetPlayerID(), false, NULL)->GetComplexUnit()`
pub const EE_UNITS_UC_CREATE_UNIT: i32 = 62;
/// `EEUGRust::NewAttackGround(f[0..3], false)`
pub const EE_UNITS_UC_NEW_ATTACK_GROUND: i32 = 63;
/// complex unit `a`'s `SetVelocity(0.0f)`
pub const EE_UNITS_UC_STOP: i32 = 64;
/// complex unit `a`'s `SetLauncherClass(its GetUnitClass())`
pub const EE_UNITS_UC_OWN_LAUNCHER_CLASS: i32 = 65;
/// complex unit `a`'s `SetOwner(unit)`
pub const EE_UNITS_UC_SET_OWNER: i32 = 66;
/// complex unit `a`'s `SetProjectileStart(f[0..3])`
pub const EE_UNITS_UC_SET_PROJECTILE_START: i32 = 67;
/// complex unit `a`'s `SetGoal(goal b, 0, 0)`
pub const EE_UNITS_UC_SET_GOAL: i32 = 68;
/// complex unit `a`'s `StartAction(b)`
pub const EE_UNITS_UC_START_ACTION: i32 = 69;
/// complex unit `a`'s `mUnitClass->ActionChanged(a)`
pub const EE_UNITS_UC_ACTION_CHANGED: i32 = 70;
/// complex unit `a`'s `mUnitClass->mMuzzleFlashEffect`
pub const EE_UNITS_UC_MUZZLE_FLASH: i32 = 71;
/// `gEffectManager->CreatePlacedEffect(f[0..3], effect a, NULL)`
pub const EE_UNITS_UC_PLACED_EFFECT: i32 = 72;
/// `the handle.mValue = 0` (Heal's local handle, the host's)
pub const EE_UNITS_UC_RESET_HANDLE: i32 = 73;
/// `the handle.mValue`
pub const EE_UNITS_UC_HANDLE: i32 = 74;

// ---- the bodies (`ee_units_uc_body`'s `what`)
pub const EE_UNITS_UC_DO_HEAL: i32 = 1;
pub const EE_UNITS_UC_DO_UPDATE_BUILDING: i32 = 2;
pub const EE_UNITS_UC_DO_UPDATE_ANIMAL: i32 = 3;
pub const EE_UNITS_UC_DO_REFRESH_SOUND: i32 = 4;
pub const EE_UNITS_UC_DO_CREATE_DEATH_UNIT: i32 = 5;

/// The effect kinds `Heal` draws between the healer and its target, and those it puts on the target.
const HEAL_BETWEEN_KINDS: [u32; 5] = [0x14, 0x16, 0x21, 0x26, 0x40];
const HEAL_ON_TARGET_KINDS: [u32; 24] = [
    2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xd, 0xe, 0xf, 0x10, 0x12, 0x1e, 0x23, 0x24, 0x27, 0x28, 0x29, 0x2b, 0x2c, 0x3a, 0x3b,
];
/// `kActionAttackEnemyUnit`, `kActionAttackLocation`, `kActionWaitForReload`, `kActionResourcesRotting`,
/// `kActionUnitRotting`, `kActionAdvance`
const ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
const ACTION_ATTACK_LOCATION: i64 = 2002;
const ACTION_WAIT_FOR_RELOAD: i64 = 2021;
const ACTION_RESOURCES_ROTTING: i64 = 6001;
const ACTION_UNIT_ROTTING: i64 = 6002;
const ACTION_ADVANCE: i64 = 10003;

/// The C++ statements of the bodies.
pub trait UcHost {
    /// one statement (`EE_UNITS_UC_*`) of class `class`'s body on unit `unit`
    #[allow(clippy::too_many_arguments)]
    fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64;
}

/// One body's statements on its class and unit.
struct Body<'h, H: UcHost + ?Sized> {
    h: &'h mut H,
    class: u64,
    unit: u64,
}

impl<H: UcHost + ?Sized> Body<'_, H> {
    fn s(&mut self, op: i32) -> i64 {
        self.h.uc(op, self.class, self.unit, 0, 0, 0, &mut [0.0; 4])
    }
    fn sa(&mut self, op: i32, a: u64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, 0, 0, &mut [0.0; 4])
    }
    fn sb(&mut self, op: i32, b: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, 0, b, 0, &mut [0.0; 4])
    }
    fn sab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.uc(op, self.class, self.unit, a, b, 0, &mut [0.0; 4])
    }
    fn sf(&mut self, op: i32, a: u64, f: &mut [f32; 4]) -> i64 {
        self.h.uc(op, self.class, self.unit, a, 0, 0, f)
    }
}

/// `Heal(unit)`: the healer's path dropped; in range of its heal goal's target, the heal animation and the class's
/// heal effect (between the two or on the target, by the effect's kind), kept as the unit's heal effect.
pub fn heal<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut b = Body { h, class, unit };
    let goal = b.s(EE_UNITS_UC_HEAL_GOAL) as u64;
    b.s(EE_UNITS_UC_DROP_PATH);
    if goal == 0 || b.sa(EE_UNITS_UC_HEAL_IN_RANGE, goal) == 0 {
        return;
    }
    b.sb(EE_UNITS_UC_SET_ANIMATION, 3);
    let effect = b.s(EE_UNITS_UC_HEAL_EFFECT) as u64;
    if effect == 0 {
        return;
    }
    let kind = b.sa(EE_UNITS_UC_EFFECT_KIND, effect) as u32;
    let op = if HEAL_BETWEEN_KINDS.contains(&kind) {
        Some(EE_UNITS_UC_HEAL_BETWEEN)
    } else if HEAL_ON_TARGET_KINDS.contains(&kind) {
        Some(EE_UNITS_UC_HEAL_ON_TARGET)
    } else {
        None
    };
    // the handle the effect manager fills (0 when it makes none)
    b.s(EE_UNITS_UC_RESET_HANDLE);
    if let Some(op) = op {
        if b.sab(op, effect, goal as i64) != 0 {
            return;
        }
    }
    let handle = b.s(EE_UNITS_UC_HANDLE);
    if b.s(EE_UNITS_UC_HEAL_HANDLE) != handle {
        b.sb(EE_UNITS_UC_SET_HEAL_HANDLE, handle);
    }
}

/// `UpdateBuilding(unit)` (EEUCBuilding::Update): production counted down (a finished unit produced, a finished
/// technology researched), the orbitals, camouflage, effects, the rotting parts, the production queue and the
/// garrison's healing.
pub fn update_building<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut b = Body { h, class, unit };
    if b.s(EE_UNITS_UC_DISABLED) != 0 && b.s(EE_UNITS_UC_HIT_POINTS) > 0 {
        return;
    }
    if b.s(EE_UNITS_UC_PRODUCTION_TIME) > 0 && b.s(EE_UNITS_UC_HIT_POINTS) > 0 && b.s(EE_UNITS_UC_IN_CINEMATIC) == 0 {
        let mut step = b.s(EE_UNITS_UC_TICK_MS) as i32;
        if step == 0 {
            step = 1;
        }
        let mut left = (b.s(EE_UNITS_UC_PRODUCTION_TIME) as i32).wrapping_sub(step);
        if b.s(EE_UNITS_UC_INSTANT_BUILD) != 0 && left >= 0 {
            left = -1;
        }
        if left <= 0 {
            left = 0;
            if b.s(EE_UNITS_UC_RESEARCH_MAKES_UNIT) != 0 {
                let made = b.s(EE_UNITS_UC_RESEARCH_CLASS) as u64;
                if b.sa(EE_UNITS_UC_IS_LAND_BUILDING, made) == 0 && b.sa(EE_UNITS_UC_PRODUCE_UNIT, made) == 0 {
                    left = 5000;
                }
            } else {
                b.s(EE_UNITS_UC_RESEARCH);
                b.s(EE_UNITS_UC_PLAY_NO_EFFECT);
            }
        }
        b.sb(EE_UNITS_UC_SET_PRODUCTION_TIME, i64::from(left));
    }
    b.s(EE_UNITS_UC_UPDATE_ORBITALS);
    b.s(EE_UNITS_UC_UPDATE_CAMOUFLAGE_TIMER);
    b.s(EE_UNITS_UC_UPDATE_EFFECTS);
    if b.s(EE_UNITS_UC_COMMAND_KIND) == ACTION_UNIT_ROTTING {
        let time = b.s(EE_UNITS_UC_UPDATE_ROTTING);
        let mut linked = b.sa(EE_UNITS_UC_NEXT_PART, 0) as u64;
        while linked != 0 {
            b.sab(EE_UNITS_UC_SET_ROTTING, linked, time);
            linked = b.sa(EE_UNITS_UC_NEXT_PART, linked) as u64;
        }
    }
    b.s(EE_UNITS_UC_CONTINUE_PRODUCTION);
    crate::units::uc_anim::heal_garrison(b.h, class, unit);
}

/// `UpdateAnimal(unit)`: physics, effects, timers and animation; fires on attack frames and lets its carcass rot.
pub fn update_animal<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut b = Body { h, class, unit };
    let frame = b.s(EE_UNITS_UC_FRAME);
    b.s(EE_UNITS_UC_KEEP_LAST_HIT_POINTS);
    if b.s(EE_UNITS_UC_HAS_PHYSICS_MODEL) != 0 && b.s(EE_UNITS_UC_TILE_X) != -10000 {
        b.s(EE_UNITS_UC_PHYSICS_STEP);
    }
    b.s(EE_UNITS_UC_UPDATE_EFFECTS);
    b.s(EE_UNITS_UC_UPDATE_RELOAD_TIMER);
    b.s(EE_UNITS_UC_UPDATE_TARGET_SEARCH_TIMER);
    let result = crate::units::uc_anim::animate(b.h, class, unit);
    refresh_sound(b.h, class, unit);
    match b.s(EE_UNITS_UC_COMMAND_TYPE) {
        ACTION_ATTACK_ENEMY_UNIT | ACTION_ATTACK_LOCATION | ACTION_WAIT_FOR_RELOAD => {
            crate::units::uc_anim::fire_on_attack_frame(b.h, class, unit, result, frame);
        }
        ACTION_RESOURCES_ROTTING => {
            b.s(EE_UNITS_UC_UPDATE_RESOURCES_ROTTING);
        }
        ACTION_UNIT_ROTTING => {
            b.s(EE_UNITS_UC_UPDATE_ROTTING);
        }
        _ => {}
    }
    b.s(EE_UNITS_UC_UPDATE_DONE);
}

/// `RefreshSound(unit)`: the volume, pan and pitch of the unit's animation sound every third tick (off-screen every
/// 21st), when the sound playing is its animation's.
pub fn refresh_sound<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut b = Body { h, class, unit };
    if b.s(EE_UNITS_UC_HAS_SOUND) == 0 || b.s(EE_UNITS_UC_SOUND_NONE) != 0 {
        return;
    }
    let tick = b.s(EE_UNITS_UC_TICKS_DONE) as u32;
    let id = b.s(EE_UNITS_UC_UNIT_ID) as u32;
    if id % 3 != tick % 3 {
        return;
    }
    if id % 0x15 != tick % 0x15 && b.s(EE_UNITS_UC_IN_VIEW) == 0 {
        return;
    }
    let sample = b.s(EE_UNITS_UC_SOUND_SAMPLE) as u32;
    let mut sound = b.s(EE_UNITS_UC_MODEL_SOUND1) as u64;
    if sound == 0 || sample != b.sa(EE_UNITS_UC_SAMPLE_ID, sound) as u32 {
        sound = b.s(EE_UNITS_UC_MODEL_SOUND2) as u64;
        if sound == 0 || sample != b.sa(EE_UNITS_UC_SAMPLE_ID, sound) as u32 {
            sound = 0;
        }
    }
    if sound == 0 {
        return;
    }
    let mut f = [0.0; 4];
    b.sf(EE_UNITS_UC_SOUND_PARAMETERS, sound, &mut f);
    b.sf(EE_UNITS_UC_SET_SOUND_PARAMETERS, 0, &mut f);
}

/// `CreateDeathUnit(unit)`: the class's death unit spawned on the unit's (or its container's) tile, launched at
/// that point (a ground attack from there, owned by the dying unit), with the muzzle flash.
#[allow(clippy::neg_cmp_op_on_partial_ord)] // the original's `!(z > 0)`: a NaN height is raised too
pub fn create_death_unit<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut b = Body { h, class, unit };
    let spawn = b.s(EE_UNITS_UC_DEATH_SPAWN_CLASS) as u64;
    if spawn == 0 {
        return;
    }
    let container = b.s(EE_UNITS_UC_CONTAINER) as u64;
    let at = if container == 0 { unit } else { container };
    let mut point = [0.0; 4];
    b.sf(EE_UNITS_UC_LOCATION, at, &mut point);
    let (x, y) = (b.sa(EE_UNITS_UC_UNIT_TILE_X, at), b.sa(EE_UNITS_UC_UNIT_TILE_Y, at));
    if !(point[2] > 0.0) {
        point[2] = 0.3;
    }
    let created = b.h.uc(EE_UNITS_UC_CREATE_UNIT, class, unit, spawn, x, y, &mut [0.0; 4]) as u64;
    let goal = b.sf(EE_UNITS_UC_NEW_ATTACK_GROUND, 0, &mut point.clone());
    b.sa(EE_UNITS_UC_STOP, created);
    b.sa(EE_UNITS_UC_OWN_LAUNCHER_CLASS, created);
    b.sa(EE_UNITS_UC_SET_OWNER, created);
    b.sf(EE_UNITS_UC_SET_PROJECTILE_START, created, &mut point.clone());
    b.sab(EE_UNITS_UC_SET_GOAL, created, goal);
    b.sab(EE_UNITS_UC_START_ACTION, created, ACTION_ADVANCE);
    b.sa(EE_UNITS_UC_ACTION_CHANGED, created);
    let flash = b.sa(EE_UNITS_UC_MUZZLE_FLASH, created) as u64;
    if flash != 0 {
        b.sf(EE_UNITS_UC_PLACED_EFFECT, flash, &mut point.clone());
    }
}

/// Runs body `what` of class `class` on unit `unit`.
pub fn run<H: UcHost + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64) {
    match what {
        EE_UNITS_UC_DO_HEAL => heal(h, class, unit),
        EE_UNITS_UC_DO_UPDATE_BUILDING => update_building(h, class, unit),
        EE_UNITS_UC_DO_UPDATE_ANIMAL => update_animal(h, class, unit),
        EE_UNITS_UC_DO_REFRESH_SOUND => refresh_sound(h, class, unit),
        EE_UNITS_UC_DO_CREATE_DEATH_UNIT => create_death_unit(h, class, unit),
        _ => {}
    }
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        /// the class host (EEUnitClass.cpp; uc_class.rs's too)
        pub(crate) fn ee_units_uc_host(
            op: i32,
            class: *mut c_void,
            unit: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            f: *mut [f32; 4],
        ) -> i64;
    }

    /// The game's host (`ee_units_uc_host`).
    pub(crate) struct Game;

    impl UcHost for Game {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::tech_cases::uc(op, unit, a) {
                return v;
            }
            if let Some(v) =
                crate::units::uc_view::uc_view(crate::units::uc_view::Kind::Class, op, class, unit, a, b, c, f)
            {
                return v;
            }
            // SAFETY: the body runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_uc_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    /// `EEUnitClass`'s body `what` (`EE_UNITS_UC_DO_*`) of `class` on `unit`.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uc_body(what: i32, class: *mut c_void, unit: *mut c_void) {
        run(&mut Game, what, class as usize as u64, unit as usize as u64)
    }
}

#[cfg(test)]
#[path = "uc_body_tests.rs"]
mod tests;
