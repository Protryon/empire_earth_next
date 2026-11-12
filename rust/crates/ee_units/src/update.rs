//! The classes' `Update` as Rust sequences (RUST_PLAN R5b.4 leaf 6, the twelfth and thirteenth rounds): which of a
//! unit's per-tick leaves run, in which order, decided over the unit store (the herd, the owner, the tile, the command,
//! the goal, the hit points, the family, the flight time), the leaves themselves host calls (the physics step, the
//! turrets, the timers, the animation, the sound, the attack frame, the rotting countdown, the heal work, the
//! regeneration, the citizens' and priests' updates, the buildings'), and the small arithmetic between them in Rust
//! (the flight time and flight state, the gate's opening, the mine's work timer, the robotic farm's graze time, the
//! fade out in space). Every class with a body of its own but the shots (combat's: `EEUCProjectile`, `EEUCMissile`,
//! `EEUCTorpedo`, `EEUCBomb`), `EEUCBasicCitizen` (one call) and `EEUCBuildingPriest` (one test); the host
//! (EEUnitUpdate.cpp) is one table for all of them. The classes whose `Update` only called a base's are gone (the
//! base's is their virtual).
//!
//! The record is read again after the host calls that can change what it holds (the command after the animation, the
//! goal after the heal search), and never held across a host call. A unit with no record (a stepped unit killed
//! this tick: RUST_PLAN's tenth round) reads the same fields from the host.
//!
//! Determinism (RUST_PLAN 2.1): the host calls in the original's order, so the random draws they make keep their
//! order; the integers as the Windows build's 32-bit `long`s (wrapping), the floats as the original's `f32`
//! operations in order, `UMFloatToLong` as `lrintf`.

use std::ffi::c_void;

use crate::physics::lrintf;
use crate::store::{with_store, EeUnitRecord};

// ---- the classes (`ee_units_update`'s `kind`)
pub const EE_UNITS_UPDATE_LAND: i32 = 1;
pub const EE_UNITS_UPDATE_NAVAL: i32 = 2;
pub const EE_UNITS_UPDATE_LAND_CITIZEN: i32 = 3;
pub const EE_UNITS_UPDATE_NAVAL_CITIZEN: i32 = 4;
/// `EEUCBasicPriest` (the land, naval and helicopter priests')
pub const EE_UNITS_UPDATE_BASIC_PRIEST: i32 = 5;
pub const EE_UNITS_UPDATE_SPACESHIP: i32 = 6;
/// `EEUCSubmergedNaval` (and the sea animal's)
pub const EE_UNITS_UPDATE_SUBMERGED_NAVAL: i32 = 7;
pub const EE_UNITS_UPDATE_HELICOPTER: i32 = 8;
/// `EEUCTransport` (and the naval and space transports')
pub const EE_UNITS_UPDATE_TRANSPORT: i32 = 9;
pub const EE_UNITS_UPDATE_LAND_TRANSPORT: i32 = 10;
pub const EE_UNITS_UPDATE_HELICOPTER_TRANSPORT: i32 = 11;
pub const EE_UNITS_UPDATE_FORTRESS: i32 = 12;
/// `EEUCLandable` (and the airplane's and planet spaceship's)
pub const EE_UNITS_UPDATE_LANDABLE: i32 = 13;
pub const EE_UNITS_UPDATE_AIR_ANIMAL: i32 = 14;
pub const EE_UNITS_UPDATE_BUILDING: i32 = 15;
pub const EE_UNITS_UPDATE_GATE: i32 = 16;
pub const EE_UNITS_UPDATE_MINING_BUILDING: i32 = 17;
pub const EE_UNITS_UPDATE_NAVAL_BUILDING: i32 = 18;
pub const EE_UNITS_UPDATE_SPACE_BUILDING: i32 = 19;
pub const EE_UNITS_UPDATE_AMBIENT: i32 = 20;

// ---- the host's reads (`EeUnitsUpdateHost::read`)
/// `mPhysicalModel.GetFrame()`
pub const EE_UNITS_UPD_R_FRAME: i32 = 1;
/// `EEHerd::IsAnimalOnMap(inUnit)`
pub const EE_UNITS_UPD_R_ANIMAL_ON_MAP: i32 = 2;
/// the class's `mPhysicsModel != NULL`
pub const EE_UNITS_UPD_R_PHYSICS_MODEL: i32 = 3;
/// the class's `mIsMedic`
pub const EE_UNITS_UPD_R_MEDIC: i32 = 4;
/// `mStats != NULL && mStats->mRegeneration > 0`
pub const EE_UNITS_UPD_R_REGENERATES: i32 = 5;
/// the store's fields for a unit with no record: `mHerd != NULL`, `mPlayer->mPlayerID`, `mTileX`,
/// `mCommand ? mCommand->mType : 0`, `mGoalState ? mGoalState->mKind : -1`, the class's `mUnitType`, `mHitPoints`,
/// the class's `mFamily`, `mIsBuilt`, `mFlightTime`
pub const EE_UNITS_UPD_R_HERD: i32 = 10;
pub const EE_UNITS_UPD_R_OWNER: i32 = 11;
pub const EE_UNITS_UPD_R_TILE_X: i32 = 12;
pub const EE_UNITS_UPD_R_COMMAND: i32 = 13;
pub const EE_UNITS_UPD_R_GOAL_KIND: i32 = 14;
pub const EE_UNITS_UPD_R_UNIT_TYPE: i32 = 15;
pub const EE_UNITS_UPD_R_HIT_POINTS: i32 = 16;
pub const EE_UNITS_UPD_R_FAMILY: i32 = 17;
pub const EE_UNITS_UPD_R_BUILT: i32 = 18;
pub const EE_UNITS_UPD_R_FLIGHT_TIME: i32 = 19;
/// `mConversionStartTime`, `gWorld.mInCinematic`, `gPhysics->mTickTimeMs`, `mCurrentPhysicsOrder`,
/// `mCurrentPhysicsAction`
pub const EE_UNITS_UPD_R_CONVERSION_START: i32 = 100;
pub const EE_UNITS_UPD_R_CINEMATIC: i32 = 101;
pub const EE_UNITS_UPD_R_TICK_MS: i32 = 102;
pub const EE_UNITS_UPD_R_PHYSICS_ORDER: i32 = 103;
pub const EE_UNITS_UPD_R_PHYSICS_ACTION: i32 = 104;
/// `GetPosition`'s z (the float's bits)
pub const EE_UNITS_UPD_R_POSITION_Z: i32 = 105;
/// the calamity manager's `IsCalamityActive(0x17)`; the class's `mAttack == 0.0f`, `IsPriest()`; `mStats->mPower`
pub const EE_UNITS_UPD_R_CALAMITY_CAMOUFLAGE: i32 = 106;
pub const EE_UNITS_UPD_R_CLASS_UNARMED: i32 = 107;
pub const EE_UNITS_UPD_R_IS_PRIEST: i32 = 108;
pub const EE_UNITS_UPD_R_STATS_POWER: i32 = 109;
/// `gPhysics->mGameTime`, `mGrazeTime`, `mStats->mGatherTimes[2]` and `[6]`, the game variant's `mWorkTimeFactor`
/// (the float's bits), `mWorkTimer`, `mMine != NULL`, `mMine->mResourceType`
pub const EE_UNITS_UPD_R_GAME_TIME: i32 = 110;
pub const EE_UNITS_UPD_R_GRAZE_TIME: i32 = 111;
pub const EE_UNITS_UPD_R_GATHER_TIME_2: i32 = 112;
pub const EE_UNITS_UPD_R_GATHER_TIME_6: i32 = 113;
pub const EE_UNITS_UPD_R_WORK_TIME_FACTOR: i32 = 114;
pub const EE_UNITS_UPD_R_WORK_TIMER: i32 = 115;
pub const EE_UNITS_UPD_R_MINE: i32 = 116;
pub const EE_UNITS_UPD_R_MINE_RESOURCE_TYPE: i32 = 117;
/// `mPreviousPart == NULL`, `mNextPart != NULL`, `mOpenFraction` (the float's bits)
pub const EE_UNITS_UPD_R_FIRST_PART: i32 = 118;
pub const EE_UNITS_UPD_R_HAS_NEXT_PART: i32 = 119;
pub const EE_UNITS_UPD_R_OPEN_FRACTION: i32 = 120;

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
/// `UpdateRottingAwayTime()` (its result), `UpdateHealSearchTimer()`, `mGoalState->Work(inUnit)`, the class's
/// `Regenerate(inUnit)`, `UpdateDone(inUnit)`, `UpdateAnimal(inUnit)`, the naval class's `UpdateResourceUnit(inUnit)`
pub const EE_UNITS_UPD_ROTTING: i32 = 34;
pub const EE_UNITS_UPD_HEAL_SEARCH_TIMER: i32 = 35;
pub const EE_UNITS_UPD_GOAL_WORK: i32 = 36;
pub const EE_UNITS_UPD_REGENERATE: i32 = 37;
pub const EE_UNITS_UPD_DONE: i32 = 38;
pub const EE_UNITS_UPD_ANIMAL: i32 = 39;
pub const EE_UNITS_UPD_RESOURCE_UNIT: i32 = 40;
/// `ContinueProduction()`, the citizen class's `UpdateCitizen(inUnit)`
pub const EE_UNITS_UPD_CONTINUE_PRODUCTION: i32 = 41;
pub const EE_UNITS_UPD_UPDATE_CITIZEN: i32 = 42;
/// the priest class's `UpdateConversion(inUnit)`, `mConversionStartTime = 0`, `ReleaseConversionEffect()`, the
/// priest class's `HideConversionEffects(inUnit)`
pub const EE_UNITS_UPD_UPDATE_CONVERSION: i32 = 43;
pub const EE_UNITS_UPD_CLEAR_CONVERSION_START: i32 = 44;
pub const EE_UNITS_UPD_RELEASE_CONVERSION_EFFECT: i32 = 45;
pub const EE_UNITS_UPD_HIDE_CONVERSION_EFFECTS: i32 = 46;
/// `UpdateResourcesRotting()`
pub const EE_UNITS_UPD_RESOURCES_ROTTING: i32 = 47;
/// `SetFlightTime(a)`, the landable class's `GetNextFlightState(inUnit)` (its result), `SetCurrentPhysicsOrder(a)`
pub const EE_UNITS_UPD_SET_FLIGHT_TIME: i32 = 48;
pub const EE_UNITS_UPD_NEXT_FLIGHT_STATE: i32 = 49;
pub const EE_UNITS_UPD_SET_PHYSICS_ORDER: i32 = 50;
/// the landable's goal queues: each started queue with goals `Update()`, then `RefreshGoals()`, `RefreshActiveGoal()`
pub const EE_UNITS_UPD_GOAL_QUEUES: i32 = 51;
/// `SetOpacityChain(a: the float's bits)`
pub const EE_UNITS_UPD_OPACITY_CHAIN: i32 = 52;
/// the class's `UpdateBuilding(inUnit)`, the building class's `UpdateArmed(inUnit)`
pub const EE_UNITS_UPD_UPDATE_BUILDING: i32 = 53;
pub const EE_UNITS_UPD_UPDATE_ARMED: i32 = 54;
/// `mPlayer->AddResource(a, b)`, `mPlayer->mSavedGroups.Reset(7, 1)`, `SetGrazeTime(a)`
pub const EE_UNITS_UPD_ADD_RESOURCE: i32 = 55;
pub const EE_UNITS_UPD_SAVED_GROUPS_RESET: i32 = 56;
pub const EE_UNITS_UPD_SET_GRAZE_TIME: i32 = 57;
/// `SetOpenFraction(a: the float's bits)` on the unit, the class's `GetLastLinkedCarrier(inUnit)` and
/// `GetLastLinked(inUnit)`
pub const EE_UNITS_UPD_SET_OPEN_FRACTION: i32 = 58;
/// `SetWorkTimer(a)`, `mMine->DepleteResource(1)`
pub const EE_UNITS_UPD_SET_WORK_TIMER: i32 = 59;
pub const EE_UNITS_UPD_DEPLETE_MINE: i32 = 60;
/// every next part's `SetRottingAwayTime(a)`
pub const EE_UNITS_UPD_SET_PARTS_ROTTING: i32 = 61;

// the original's constants (ee/EEUnitAction.h, ee/EEUnitGoal.h, ee/EEUnitClassIDs.h, ee/EEUCGate.cpp, EEPlayer.h)
const ACTION_CIRCLE: i64 = 1008;
const ACTION_WAIT_FOR_LANDING_CLEARANCE: i64 = 1009;
const ACTION_CIRCLE_AIRTRAFFIC_CONTROLLER: i64 = 1022;
const ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
const ACTION_ATTACK_LOCATION: i64 = 2002;
const ACTION_CONVERT_ENEMY_UNIT: i64 = 2004;
const ACTION_FACE_ENEMY_UNIT: i64 = 2012;
const ACTION_WAIT_FOR_RELOAD: i64 = 2021;
const ACTION_FACE_LOCATION: i64 = 2027;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const ACTION_IDLE: i64 = 5000;
const ACTION_RESOURCES_ROTTING: i64 = 6001;
const ACTION_UNIT_ROTTING: i64 = 6002;
const ACTION_FADING_OUT_IN_SPACE: i64 = 6009;
const ACTION_OPENING: i64 = 15002;
const ACTION_CLOSING: i64 = 15003;
/// `kGoalHealUnit` (ee/EEUnitGoal.h; the twelfth round's 13 was wrong: a land medic never worked its heal goal)
const GOAL_HEAL_UNIT: i64 = 15;
const UNIT_TYPE_HOUSE: i64 = 0x25;
const UNIT_TYPE_CAPITOL: i64 = 0x29;
const UNIT_TYPE_RES_FISH: i64 = 0x13b;
const UNIT_TYPE_CP_MISSILE_BASE: i64 = 0x2ed;
const UNIT_TYPE_TELEPORTER: i64 = 0x313;
const UNIT_TYPE_ROBOTIC_FARM: i64 = 0x33a;
const FAMILY_WALLS: i64 = 0x2c;
const RESOURCE_FOOD: i64 = 2;
/// the flight state 5 (`mCurrentPhysicsOrder`), which the flight state machine enters and leaves at once
const FLIGHT_STATE_5: u32 = 5;
/// `kGateOpen`, `kGateClosed`, `kGateStep`
const GATE_OPEN: f32 = 1.0;
const GATE_CLOSED: f32 = 0.0;
const GATE_STEP: f32 = 0.2;
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
                EE_UNITS_UPD_R_HIT_POINTS => r.hit_points,
                EE_UNITS_UPD_R_FAMILY => r.family,
                EE_UNITS_UPD_R_BUILT => i64::from(r.built),
                EE_UNITS_UPD_R_FLIGHT_TIME => r.flight_time,
                _ => self.host.read(code),
            },
            None => self.host.read(code),
        }
    }
    fn read(&mut self, code: i32) -> i64 {
        self.host.read(code)
    }
    fn on(&mut self, code: i32) -> bool {
        self.host.read(code) != 0
    }
    fn act(&mut self, code: i32) -> i64 {
        self.host.act(code, 0, 0)
    }
    fn act_with(&mut self, code: i32, a: i64) -> i64 {
        self.host.act(code, a, 0)
    }
    fn command(&mut self) -> i64 {
        self.field(EE_UNITS_UPD_R_COMMAND)
    }
    /// the class has a physics model and the unit is on the map
    fn can_step(&mut self) -> bool {
        self.on(EE_UNITS_UPD_R_PHYSICS_MODEL) && self.field(EE_UNITS_UPD_R_TILE_X) != OFF_MAP
    }
    /// the physics step when the unit can step (`turrets`: then the turrets too)
    fn step(&mut self, turrets: bool) {
        if self.can_step() {
            self.act(EE_UNITS_UPD_PHYSICS_STEP);
            if turrets {
                self.act(EE_UNITS_UPD_TURRETS);
            }
        }
    }
    /// the reload, target search and camouflage timers
    fn timers(&mut self) {
        self.act(EE_UNITS_UPD_RELOAD_TIMER);
        self.act(EE_UNITS_UPD_TARGET_SEARCH_TIMER);
        self.act(EE_UNITS_UPD_CAMOUFLAGE_TIMER);
    }
    /// the animation (its result) and the sound
    fn animate(&mut self) -> i64 {
        let attack = self.act(EE_UNITS_UPD_ANIMATE);
        self.act(EE_UNITS_UPD_REFRESH_SOUND);
        attack
    }
    /// the command's attack frame (`face`: the face-location command fires too) or rotting countdown
    fn attack_or_rot(&mut self, attack: i64, frame: i64, face: bool) {
        match self.command() {
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
    /// the rotting countdown under the rotting command
    fn rot(&mut self) {
        if self.command() == ACTION_UNIT_ROTTING {
            self.act(EE_UNITS_UPD_ROTTING);
        }
    }
    /// the regeneration when the stats regenerate
    fn regenerate(&mut self) {
        if self.on(EE_UNITS_UPD_R_REGENERATES) {
            self.act(EE_UNITS_UPD_REGENERATE);
        }
    }
    /// a medic's heal search and its heal goal's work
    fn medic(&mut self) {
        if self.on(EE_UNITS_UPD_R_MEDIC) {
            self.act(EE_UNITS_UPD_HEAL_SEARCH_TIMER);
            if self.field(EE_UNITS_UPD_R_GOAL_KIND) == GOAL_HEAL_UNIT {
                self.act(EE_UNITS_UPD_GOAL_WORK);
            }
        }
    }
}

/// The flight time's count down by the tick (`theTime - mTickTimeMs`, at least 0), as 32-bit `long`s.
fn flight_left(time: i32, tick_ms: i32) -> i32 {
    let left = time.wrapping_sub(tick_ms);
    if left < 0 {
        0
    } else {
        left
    }
}

/// `EEUCLandable::UpdateFlightState` (`face_check`: the landable's, whose facing an enemy holds the state; the air
/// animal's inline copy has no such test): the next flight state over the circling commands, state 5 and the second
/// the flight time crossed.
fn flight_state(u: &mut UpdateReads<'_>, time: i32, left: i32, face_check: bool) {
    let command = u.command();
    if command == ACTION_CIRCLE
        || command == ACTION_WAIT_FOR_LANDING_CLEARANCE
        || command == ACTION_CIRCLE_AIRTRAFFIC_CONTROLLER
    {
        return;
    }
    let state = u.read(EE_UNITS_UPD_R_PHYSICS_ORDER) as u32;
    let next = u.act(EE_UNITS_UPD_NEXT_FLIGHT_STATE) as u32;
    if state == FLIGHT_STATE_5 {
        if next != FLIGHT_STATE_5 {
            u.act_with(EE_UNITS_UPD_SET_PHYSICS_ORDER, i64::from(next));
        }
    } else if next == FLIGHT_STATE_5 {
        u.act_with(EE_UNITS_UPD_SET_PHYSICS_ORDER, i64::from(FLIGHT_STATE_5));
    } else if state != next {
        let now = time / 1000;
        let last = left / 1000;
        if (!face_check || u.command() != ACTION_FACE_ENEMY_UNIT) && (now > last || last == 0) {
            u.act_with(EE_UNITS_UPD_SET_PHYSICS_ORDER, i64::from(next));
        }
    }
}

/// `EEUCLand::Update`: a herd member, nature's unit (player 0) or a map animal updates as an animal; else the
/// physics step and turrets, the wheels, effects, the timers, the animation, sound, power, the fanaticism colour, the
/// attack frame or the rotting, a medic's heal search and work, the regeneration, and the class's last call.
fn land(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    if u.field(EE_UNITS_UPD_R_HERD) != 0 || u.field(EE_UNITS_UPD_R_OWNER) == 0 || u.on(EE_UNITS_UPD_R_ANIMAL_ON_MAP) {
        u.act(EE_UNITS_UPD_ANIMAL);
        return;
    }
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(true);
    u.act(EE_UNITS_UPD_WHEELS);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.timers();
    let attack = u.animate();
    u.act(EE_UNITS_UPD_REGENERATE_POWER);
    u.act(EE_UNITS_UPD_FANATICISM_COLOR);
    u.attack_or_rot(attack, frame, true);
    u.medic();
    u.regenerate();
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
    u.timers();
    let attack = u.animate();
    u.attack_or_rot(attack, frame, false);
    u.act(EE_UNITS_UPD_DONE);
}

/// `EEUCLandCitizen::Update` (0x616c48): the step, effects, the timers, the animation, sound, the attack frame or the
/// rotting, the production and the citizen's update.
fn land_citizen(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(false);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.timers();
    let attack = u.animate();
    u.attack_or_rot(attack, frame, false);
    u.act(EE_UNITS_UPD_CONTINUE_PRODUCTION);
    u.act(EE_UNITS_UPD_UPDATE_CITIZEN);
}

/// `EEUCNavalCitizen::Update` (0x618767): the step, effects, the camouflage, the animation, sound, the rotting and
/// the citizen's update.
fn naval_citizen(u: &mut UpdateReads<'_>) {
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(false);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.act(EE_UNITS_UPD_CAMOUFLAGE_TIMER);
    u.animate();
    u.rot();
    u.act(EE_UNITS_UPD_UPDATE_CITIZEN);
}

/// `EEUCBasicPriest::Update` (0x60df05): the step with the turrets and orbitals, effects, the timers, the animation,
/// sound, a medic's heal, the regeneration, power; then the attack frame, the conversion (which ends the update), the
/// resources' rotting (likewise) or the unit's; else the conversion's start time, effect and effects cleared.
fn basic_priest(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    if u.can_step() {
        u.act(EE_UNITS_UPD_PHYSICS_STEP);
        u.act(EE_UNITS_UPD_TURRETS);
        u.act(EE_UNITS_UPD_ORBITALS);
    }
    u.act(EE_UNITS_UPD_EFFECTS);
    u.timers();
    let attack = u.animate();
    u.medic();
    u.regenerate();
    u.act(EE_UNITS_UPD_DONE);
    u.act(EE_UNITS_UPD_REGENERATE_POWER);
    match u.command() {
        ACTION_ATTACK_ENEMY_UNIT | ACTION_ATTACK_LOCATION | ACTION_WAIT_FOR_RELOAD => {
            u.host.act(EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME, attack, frame);
        }
        ACTION_CONVERT_ENEMY_UNIT => {
            u.act(EE_UNITS_UPD_UPDATE_CONVERSION);
            return;
        }
        ACTION_RESOURCES_ROTTING => return,
        ACTION_UNIT_ROTTING => {
            u.act(EE_UNITS_UPD_ROTTING);
        }
        _ => {}
    }
    if u.read(EE_UNITS_UPD_R_CONVERSION_START) != 0 {
        u.act(EE_UNITS_UPD_CLEAR_CONVERSION_START);
    }
    u.act(EE_UNITS_UPD_RELEASE_CONVERSION_EFFECT);
    u.act(EE_UNITS_UPD_HIDE_CONVERSION_EFFECTS);
}

/// `EEUCSpaceship::Update` (0x61a5dd): the step and turrets, the orbitals, the timers, the animation, sound, power,
/// the attack frame or the rotting, the regeneration and the class's last call.
fn spaceship(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(true);
    u.act(EE_UNITS_UPD_ORBITALS);
    u.timers();
    let attack = u.animate();
    u.act(EE_UNITS_UPD_REGENERATE_POWER);
    u.attack_or_rot(attack, frame, false);
    u.regenerate();
    u.act(EE_UNITS_UPD_DONE);
}

/// `EEUCSubmergedNaval::Update` (0x61b085): the step, effects, the timers, the animation, sound, the attack frame or
/// the rotting and the class's last call.
fn submerged_naval(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(false);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.timers();
    let attack = u.animate();
    u.attack_or_rot(attack, frame, false);
    u.act(EE_UNITS_UPD_DONE);
}

/// `EEUCHelicopter::Update` (0x612deb): the step, the orbitals, the timers, the animation, sound, the attack frame or
/// the rotting, the regeneration and the class's last call.
fn helicopter(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(false);
    u.act(EE_UNITS_UPD_ORBITALS);
    u.timers();
    let attack = u.animate();
    u.attack_or_rot(attack, frame, false);
    u.regenerate();
    u.act(EE_UNITS_UPD_DONE);
}

/// `EEUCTransport::Update` (0x61c92c): the step, effects, the camouflage, the animation, sound, the rotting and the
/// regeneration.
fn transport(u: &mut UpdateReads<'_>) {
    u.act(EE_UNITS_UPD_LAST_HIT_POINTS);
    u.step(false);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.act(EE_UNITS_UPD_CAMOUFLAGE_TIMER);
    u.animate();
    u.rot();
    u.regenerate();
}

/// `EEUCLandable::Update` (0x616a11): the flight time down by the tick (not in a cinematic or for nature's units) and
/// the flight state, unless idle; the goal queues; the step, the orbitals, effects, the timers, the animation, sound;
/// the attack frame, the rotting or the fade out below the ground in space; the class's last call.
fn landable(u: &mut UpdateReads<'_>) {
    let frame = u.read(EE_UNITS_UPD_R_FRAME);
    if u.command() != ACTION_IDLE {
        let time = u.field(EE_UNITS_UPD_R_FLIGHT_TIME) as i32;
        let left = flight_left(time, u.read(EE_UNITS_UPD_R_TICK_MS) as i32);
        if !u.on(EE_UNITS_UPD_R_CINEMATIC) && u.field(EE_UNITS_UPD_R_OWNER) != 0 {
            u.act_with(EE_UNITS_UPD_SET_FLIGHT_TIME, i64::from(left));
        }
        flight_state(u, time, left, true);
    }
    u.act(EE_UNITS_UPD_GOAL_QUEUES);
    u.step(false);
    u.act(EE_UNITS_UPD_ORBITALS);
    u.act(EE_UNITS_UPD_EFFECTS);
    u.timers();
    let attack = u.animate();
    match u.command() {
        ACTION_ATTACK_ENEMY_UNIT | ACTION_ATTACK_LOCATION | ACTION_WAIT_FOR_RELOAD => {
            u.host.act(EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME, attack, frame);
        }
        ACTION_UNIT_ROTTING => {
            u.act(EE_UNITS_UPD_ROTTING);
        }
        ACTION_FADING_OUT_IN_SPACE => {
            let z = f32::from_bits(u.read(EE_UNITS_UPD_R_POSITION_Z) as u32);
            if z < 0.0 {
                let opacity = 1.0f32 - z * -0.16666667f32;
                u.act_with(EE_UNITS_UPD_OPACITY_CHAIN, i64::from(opacity.to_bits()));
            }
        }
        _ => {}
    }
    u.act(EE_UNITS_UPD_DONE);
}

/// `EEUCAirAnimal::Update` (0x60aff5): the flight time down by the tick (not in a cinematic) and the flight state,
/// unless idle; the step, the animation, sound; the resources' or the unit's rotting.
fn air_animal(u: &mut UpdateReads<'_>) {
    if u.command() != ACTION_IDLE {
        let time = u.field(EE_UNITS_UPD_R_FLIGHT_TIME) as i32;
        let left = flight_left(time, u.read(EE_UNITS_UPD_R_TICK_MS) as i32);
        if !u.on(EE_UNITS_UPD_R_CINEMATIC) {
            u.act_with(EE_UNITS_UPD_SET_FLIGHT_TIME, i64::from(left));
        }
        flight_state(u, time, left, false);
    }
    u.step(false);
    u.animate();
    match u.command() {
        ACTION_RESOURCES_ROTTING => {
            u.act(EE_UNITS_UPD_RESOURCES_ROTTING);
        }
        ACTION_UNIT_ROTTING => {
            u.act(EE_UNITS_UPD_ROTTING);
        }
        _ => {}
    }
}

/// `EEUCBuilding::Update` (0x60fd9d): a standing wall or house only runs its camouflage under the calamity 0x17; an
/// unarmed building (or the missile base) steps while its physics action is 2, a capitol with power regenerates it,
/// then the building's update; an armed one the armed update. A built robotic farm then harvests one food when its
/// graze time has come, and the next in its gather time scaled by the work time factor.
fn building(u: &mut UpdateReads<'_>) {
    let unit_type = u.field(EE_UNITS_UPD_R_UNIT_TYPE);
    if (u.field(EE_UNITS_UPD_R_FAMILY) == FAMILY_WALLS || unit_type == UNIT_TYPE_HOUSE)
        && u.field(EE_UNITS_UPD_R_HIT_POINTS) > 0
    {
        if u.on(EE_UNITS_UPD_R_CALAMITY_CAMOUFLAGE) {
            u.act(EE_UNITS_UPD_CAMOUFLAGE_TIMER);
        }
        return;
    }
    if (u.on(EE_UNITS_UPD_R_CLASS_UNARMED) && !u.on(EE_UNITS_UPD_R_IS_PRIEST)) || unit_type == UNIT_TYPE_CP_MISSILE_BASE
    {
        if u.read(EE_UNITS_UPD_R_PHYSICS_ACTION) == 2 && u.can_step() {
            u.act(EE_UNITS_UPD_PHYSICS_STEP);
        }
        if unit_type == UNIT_TYPE_CAPITOL && u.read(EE_UNITS_UPD_R_STATS_POWER) > 0 {
            u.act(EE_UNITS_UPD_DONE);
            u.act(EE_UNITS_UPD_REGENERATE_POWER);
        }
        u.act(EE_UNITS_UPD_UPDATE_BUILDING);
    } else {
        u.act(EE_UNITS_UPD_UPDATE_ARMED);
    }
    if u.field(EE_UNITS_UPD_R_UNIT_TYPE) != UNIT_TYPE_ROBOTIC_FARM
        || (u.read(EE_UNITS_UPD_R_GAME_TIME) as u32) < (u.read(EE_UNITS_UPD_R_GRAZE_TIME) as u32)
        || u.field(EE_UNITS_UPD_R_BUILT) == 0
    {
        return;
    }
    u.host.act(EE_UNITS_UPD_ADD_RESOURCE, RESOURCE_FOOD, 1);
    u.act(EE_UNITS_UPD_SAVED_GROUPS_RESET);
    let time = u.read(EE_UNITS_UPD_R_GAME_TIME) as i32;
    let delay = u.read(EE_UNITS_UPD_R_GATHER_TIME_6) as i32;
    let scale = f32::from_bits(u.read(EE_UNITS_UPD_R_WORK_TIME_FACTOR) as u32);
    let graze = time.wrapping_add(lrintf(scale * delay as f32)) as u32;
    u.act_with(EE_UNITS_UPD_SET_GRAZE_TIME, i64::from(graze));
}

/// `EEUCGate::Update` (0x611dc9): a gate's first part with a next one opens or closes a step (clamped), on itself and
/// its last linked carrier and part; then the building's update.
fn gate(u: &mut UpdateReads<'_>) {
    let command = u.command();
    if u.on(EE_UNITS_UPD_R_FIRST_PART)
        && u.on(EE_UNITS_UPD_R_HAS_NEXT_PART)
        && (command == ACTION_OPENING || command == ACTION_CLOSING)
    {
        let mut open = f32::from_bits(u.read(EE_UNITS_UPD_R_OPEN_FRACTION) as u32);
        if command == ACTION_OPENING {
            open += GATE_STEP;
            if GATE_OPEN < open {
                open = GATE_OPEN;
            }
        } else {
            open -= GATE_STEP;
            if GATE_CLOSED > open {
                open = GATE_CLOSED;
            }
        }
        u.act_with(EE_UNITS_UPD_SET_OPEN_FRACTION, i64::from(open.to_bits()));
    }
    building(u);
}

/// `EEUCMiningBuilding::Update` (0x6172a5): the building's update; then, while gathering, the work timer down by the
/// tick (at least 1 ms, at least 0), and when it was 0 with a mine, one resource out of the mine into the player's and
/// the timer again.
fn mining_building(u: &mut UpdateReads<'_>) {
    building(u);
    if u.command() != ACTION_GATHER_RESOURCE {
        return;
    }
    let time = u.read(EE_UNITS_UPD_R_WORK_TIMER) as i32;
    let mine = u.on(EE_UNITS_UPD_R_MINE);
    let mut elapsed = u.read(EE_UNITS_UPD_R_TICK_MS) as u32;
    if time > 0 {
        if elapsed == 0 {
            elapsed = 1;
        }
        let mut remaining = time.wrapping_sub(elapsed as i32);
        if remaining < 0 {
            remaining = 0;
        }
        u.act_with(EE_UNITS_UPD_SET_WORK_TIMER, i64::from(remaining));
    }
    if time != 0 || !mine {
        return;
    }
    u.act(EE_UNITS_UPD_DEPLETE_MINE);
    let resource = u.read(EE_UNITS_UPD_R_MINE_RESOURCE_TYPE);
    u.host.act(EE_UNITS_UPD_ADD_RESOURCE, resource, 1);
    let gather = u.read(EE_UNITS_UPD_R_GATHER_TIME_2);
    u.act_with(EE_UNITS_UPD_SET_WORK_TIMER, gather);
}

/// `EEUCNavalBuilding::Update` (0x61845a) and `EEUCSpaceBuilding::Update` (0x61a1cb, `space`: with the
/// regeneration): the step, sound, then the building's update.
fn floating_building(u: &mut UpdateReads<'_>, space: bool) {
    if u.can_step() {
        u.act(EE_UNITS_UPD_PHYSICS_STEP);
    }
    u.act(EE_UNITS_UPD_REFRESH_SOUND);
    if space {
        u.regenerate();
    }
    building(u);
}

/// `EEUCAmbient::Update` (0x60bc8e): a rotting ambient unit counts down and hands the time to its next parts.
fn ambient(u: &mut UpdateReads<'_>) {
    if u.command() != ACTION_UNIT_ROTTING {
        return;
    }
    let time = u.act(EE_UNITS_UPD_ROTTING);
    u.act_with(EE_UNITS_UPD_SET_PARTS_ROTTING, time);
}

/// A class's `Update` (`kind`: `EE_UNITS_UPDATE_*`) over `record` (the unit's record now, or none). False for a kind
/// it does not know (nothing called).
pub fn update(kind: i32, host: &mut impl UpdateHost, record: &mut dyn FnMut() -> Option<EeUnitRecord>) -> bool {
    let mut reads = UpdateReads { host, record };
    let u = &mut reads;
    match kind {
        EE_UNITS_UPDATE_LAND => land(u),
        EE_UNITS_UPDATE_NAVAL => naval(u),
        EE_UNITS_UPDATE_LAND_CITIZEN => land_citizen(u),
        EE_UNITS_UPDATE_NAVAL_CITIZEN => naval_citizen(u),
        EE_UNITS_UPDATE_BASIC_PRIEST => basic_priest(u),
        EE_UNITS_UPDATE_SPACESHIP => spaceship(u),
        EE_UNITS_UPDATE_SUBMERGED_NAVAL => submerged_naval(u),
        EE_UNITS_UPDATE_HELICOPTER => helicopter(u),
        EE_UNITS_UPDATE_TRANSPORT => transport(u),
        EE_UNITS_UPDATE_LAND_TRANSPORT => {
            transport(u);
            u.act(EE_UNITS_UPD_WHEELS);
        }
        EE_UNITS_UPDATE_HELICOPTER_TRANSPORT => {
            transport(u);
            u.act(EE_UNITS_UPD_ORBITALS);
        }
        EE_UNITS_UPDATE_FORTRESS => {
            // a fortress is a land transport, but its update is the transport's (no wheels)
            if u.field(EE_UNITS_UPD_R_UNIT_TYPE) == UNIT_TYPE_TELEPORTER {
                u.act(EE_UNITS_UPD_REGENERATE_POWER);
            }
            transport(u);
        }
        EE_UNITS_UPDATE_LANDABLE => landable(u),
        EE_UNITS_UPDATE_AIR_ANIMAL => air_animal(u),
        EE_UNITS_UPDATE_BUILDING => building(u),
        EE_UNITS_UPDATE_GATE => gate(u),
        EE_UNITS_UPDATE_MINING_BUILDING => mining_building(u),
        EE_UNITS_UPDATE_NAVAL_BUILDING => floating_building(u, false),
        EE_UNITS_UPDATE_SPACE_BUILDING => floating_building(u, true),
        EE_UNITS_UPDATE_AMBIENT => ambient(u),
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

    /// A host that logs its operations and answers reads from a map (0 for others; the animation answers 77, the
    /// rotting 1234, the next flight state `next_state`).
    #[derive(Default)]
    struct Mock {
        reads: BTreeMap<i32, i64>,
        log: Vec<(i32, i64, i64)>,
        /// a command the animation switches to (the record's, read after it)
        after_animate: Option<i64>,
        next_state: i64,
    }

    impl UpdateHost for Mock {
        fn read(&mut self, code: i32) -> i64 {
            *self.reads.get(&code).unwrap_or(&0)
        }
        fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
            self.log.push((code, a, b));
            match code {
                EE_UNITS_UPD_ANIMATE => {
                    if let Some(c) = self.after_animate {
                        self.reads.insert(EE_UNITS_UPD_R_COMMAND, c);
                    }
                    77
                }
                EE_UNITS_UPD_ROTTING => 1234,
                EE_UNITS_UPD_NEXT_FLIGHT_STATE => self.next_state,
                _ => 0,
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
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, 15); // kGoalHealUnit
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
        // a wall's family and hit points from the record
        let mut m = soldier();
        let rec = EeUnitRecord { family: FAMILY_WALLS, hit_points: 5, ..Default::default() };
        assert!(update(EE_UNITS_UPDATE_BUILDING, &mut m, &mut || Some(rec)));
        assert!(m.log.is_empty());
    }

    #[test]
    fn citizens_and_transports() {
        let mut m = soldier();
        m.after_animate = Some(ACTION_ATTACK_LOCATION);
        run(EE_UNITS_UPDATE_LAND_CITIZEN, &mut m);
        assert_eq!(
            ops(&m),
            vec![
                EE_UNITS_UPD_LAST_HIT_POINTS,
                EE_UNITS_UPD_PHYSICS_STEP,
                EE_UNITS_UPD_EFFECTS,
                EE_UNITS_UPD_RELOAD_TIMER,
                EE_UNITS_UPD_TARGET_SEARCH_TIMER,
                EE_UNITS_UPD_CAMOUFLAGE_TIMER,
                EE_UNITS_UPD_ANIMATE,
                EE_UNITS_UPD_REFRESH_SOUND,
                EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME,
                EE_UNITS_UPD_CONTINUE_PRODUCTION,
                EE_UNITS_UPD_UPDATE_CITIZEN,
            ]
        );
        // a naval citizen never fires; it rots
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_ATTACK_ENEMY_UNIT);
        run(EE_UNITS_UPDATE_NAVAL_CITIZEN, &mut m);
        assert!(!ops(&m).contains(&EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME));
        assert_eq!(*ops(&m).last().unwrap(), EE_UNITS_UPD_UPDATE_CITIZEN);
        // the transports: the land one's wheels, the helicopter's orbitals, the fortress's teleporter power and no
        // wheels
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_REGENERATES, 1);
        run(EE_UNITS_UPDATE_LAND_TRANSPORT, &mut m);
        let o = ops(&m);
        assert_eq!(o[o.len() - 2..].to_vec(), vec![EE_UNITS_UPD_REGENERATE, EE_UNITS_UPD_WHEELS]);
        let mut m = soldier();
        run(EE_UNITS_UPDATE_HELICOPTER_TRANSPORT, &mut m);
        assert_eq!(*ops(&m).last().unwrap(), EE_UNITS_UPD_ORBITALS);
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_TELEPORTER);
        run(EE_UNITS_UPDATE_FORTRESS, &mut m);
        let o = ops(&m);
        assert_eq!(o[0], EE_UNITS_UPD_REGENERATE_POWER);
        assert!(!o.contains(&EE_UNITS_UPD_WHEELS));
    }

    #[test]
    fn a_priest_converts_or_clears_its_conversion() {
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_CONVERT_ENEMY_UNIT);
        run(EE_UNITS_UPDATE_BASIC_PRIEST, &mut m);
        let o = ops(&m);
        assert_eq!(o[1..4].to_vec(), vec![EE_UNITS_UPD_PHYSICS_STEP, EE_UNITS_UPD_TURRETS, EE_UNITS_UPD_ORBITALS]);
        assert_eq!(
            o[o.len() - 3..].to_vec(),
            vec![EE_UNITS_UPD_DONE, EE_UNITS_UPD_REGENERATE_POWER, EE_UNITS_UPD_UPDATE_CONVERSION]
        );
        // the resources rotting: nothing after the power
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_RESOURCES_ROTTING);
        run(EE_UNITS_UPDATE_BASIC_PRIEST, &mut m);
        assert_eq!(*ops(&m).last().unwrap(), EE_UNITS_UPD_REGENERATE_POWER);
        // otherwise: the start time cleared only when set, then the effects
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_CONVERSION_START, 5);
        run(EE_UNITS_UPDATE_BASIC_PRIEST, &mut m);
        let o = ops(&m);
        assert_eq!(
            o[o.len() - 3..].to_vec(),
            vec![
                EE_UNITS_UPD_CLEAR_CONVERSION_START,
                EE_UNITS_UPD_RELEASE_CONVERSION_EFFECT,
                EE_UNITS_UPD_HIDE_CONVERSION_EFFECTS
            ]
        );
        let mut m = soldier();
        run(EE_UNITS_UPDATE_BASIC_PRIEST, &mut m);
        assert!(!ops(&m).contains(&EE_UNITS_UPD_CLEAR_CONVERSION_START));
    }

    #[test]
    fn the_flight_time_and_state() {
        assert_eq!(flight_left(500, 30), 470);
        assert_eq!(flight_left(10, 30), 0);
        assert_eq!(flight_left(i32::MIN, 1), i32::MAX);
        // a flying plane: the time written, the state changed when a second was crossed
        let flying = |time: i64, state: i64, next: i64| {
            let mut m = soldier();
            m.reads.insert(EE_UNITS_UPD_R_COMMAND, 1001);
            m.reads.insert(EE_UNITS_UPD_R_FLIGHT_TIME, time);
            m.reads.insert(EE_UNITS_UPD_R_TICK_MS, 100);
            m.reads.insert(EE_UNITS_UPD_R_PHYSICS_ORDER, state);
            m.next_state = next;
            m
        };
        let mut m = flying(2050, 1, 2);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert_eq!(m.log[0], (EE_UNITS_UPD_SET_FLIGHT_TIME, 1950, 0));
        assert!(m.log.contains(&(EE_UNITS_UPD_SET_PHYSICS_ORDER, 2, 0)));
        // the same second: no change
        let mut m = flying(2550, 1, 2);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert!(!ops(&m).contains(&EE_UNITS_UPD_SET_PHYSICS_ORDER));
        // the last second: a change
        let mut m = flying(550, 1, 2);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert!(m.log.contains(&(EE_UNITS_UPD_SET_PHYSICS_ORDER, 2, 0)));
        // state 5 to another state at once, and to state 5 at once
        let mut m = flying(2550, 5, 3);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert!(m.log.contains(&(EE_UNITS_UPD_SET_PHYSICS_ORDER, 3, 0)));
        let mut m = flying(2550, 3, 5);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert!(m.log.contains(&(EE_UNITS_UPD_SET_PHYSICS_ORDER, 5, 0)));
        // facing an enemy holds the landable's state, not the air animal's
        let mut m = flying(2050, 1, 2);
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_FACE_ENEMY_UNIT);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert!(!ops(&m).contains(&EE_UNITS_UPD_SET_PHYSICS_ORDER));
        let mut m = flying(2050, 1, 2);
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_FACE_ENEMY_UNIT);
        run(EE_UNITS_UPDATE_AIR_ANIMAL, &mut m);
        assert!(ops(&m).contains(&EE_UNITS_UPD_SET_PHYSICS_ORDER));
        // circling: no state asked; nature's landable keeps its time, the air animal does not; idle: nothing
        let mut m = flying(2050, 1, 2);
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_CIRCLE);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert!(!ops(&m).contains(&EE_UNITS_UPD_NEXT_FLIGHT_STATE));
        let mut m = flying(2050, 1, 2);
        m.reads.insert(EE_UNITS_UPD_R_OWNER, 0);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert!(!ops(&m).contains(&EE_UNITS_UPD_SET_FLIGHT_TIME));
        let mut m = flying(2050, 1, 2);
        m.reads.insert(EE_UNITS_UPD_R_OWNER, 0);
        run(EE_UNITS_UPDATE_AIR_ANIMAL, &mut m);
        assert!(ops(&m).contains(&EE_UNITS_UPD_SET_FLIGHT_TIME));
        let mut m = flying(2050, 1, 2);
        m.reads.insert(EE_UNITS_UPD_R_CINEMATIC, 1);
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_IDLE);
        run(EE_UNITS_UPDATE_LANDABLE, &mut m);
        assert_eq!(ops(&m)[0], EE_UNITS_UPD_GOAL_QUEUES);
    }

    #[test]
    fn fading_out_in_space() {
        let fade = |z: f32| {
            let mut m = soldier();
            m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_FADING_OUT_IN_SPACE);
            m.reads.insert(EE_UNITS_UPD_R_POSITION_Z, i64::from(z.to_bits()));
            run(EE_UNITS_UPDATE_LANDABLE, &mut m);
            m.log.iter().find(|l| l.0 == EE_UNITS_UPD_OPACITY_CHAIN).map(|l| f32::from_bits(l.1 as u32))
        };
        assert_eq!(fade(1.0), None);
        assert_eq!(fade(0.0), None);
        assert_eq!(fade(-3.0), Some(1.0f32 - -3.0f32 * -0.16666667f32));
        assert_eq!(fade(-3.0).unwrap().to_bits(), 0.5f32.to_bits());
    }

    fn building_mock() -> Mock {
        let mut m = Mock::default();
        m.reads.insert(EE_UNITS_UPD_R_OWNER, 2);
        m.reads.insert(EE_UNITS_UPD_R_HIT_POINTS, 100);
        m
    }

    #[test]
    fn buildings() {
        // a standing wall: the camouflage under the calamity only
        let mut m = building_mock();
        m.reads.insert(EE_UNITS_UPD_R_FAMILY, FAMILY_WALLS);
        run(EE_UNITS_UPDATE_BUILDING, &mut m);
        assert!(m.log.is_empty());
        m.reads.insert(EE_UNITS_UPD_R_CALAMITY_CAMOUFLAGE, 1);
        run(EE_UNITS_UPDATE_BUILDING, &mut m);
        assert_eq!(ops(&m), vec![EE_UNITS_UPD_CAMOUFLAGE_TIMER]);
        // a dead house updates as a building; an armed one as armed; a priest building as armed
        let mut m = building_mock();
        m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_HOUSE);
        m.reads.insert(EE_UNITS_UPD_R_HIT_POINTS, 0);
        run(EE_UNITS_UPDATE_BUILDING, &mut m);
        assert_eq!(ops(&m), vec![EE_UNITS_UPD_UPDATE_ARMED]);
        let mut m = building_mock();
        m.reads.insert(EE_UNITS_UPD_R_CLASS_UNARMED, 1);
        m.reads.insert(EE_UNITS_UPD_R_IS_PRIEST, 1);
        run(EE_UNITS_UPDATE_BUILDING, &mut m);
        assert_eq!(ops(&m), vec![EE_UNITS_UPD_UPDATE_ARMED]);
        // an unarmed capitol with power, stepping
        let mut m = building_mock();
        m.reads.insert(EE_UNITS_UPD_R_CLASS_UNARMED, 1);
        m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_CAPITOL);
        m.reads.insert(EE_UNITS_UPD_R_STATS_POWER, 3);
        m.reads.insert(EE_UNITS_UPD_R_PHYSICS_ACTION, 2);
        m.reads.insert(EE_UNITS_UPD_R_PHYSICS_MODEL, 1);
        run(EE_UNITS_UPDATE_BUILDING, &mut m);
        assert_eq!(
            ops(&m),
            vec![
                EE_UNITS_UPD_PHYSICS_STEP,
                EE_UNITS_UPD_DONE,
                EE_UNITS_UPD_REGENERATE_POWER,
                EE_UNITS_UPD_UPDATE_BUILDING
            ]
        );
        // the missile base is unarmed by its type
        let mut m = building_mock();
        m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_CP_MISSILE_BASE);
        run(EE_UNITS_UPDATE_BUILDING, &mut m);
        assert_eq!(ops(&m), vec![EE_UNITS_UPD_UPDATE_BUILDING]);
    }

    #[test]
    fn the_robotic_farm_harvests() {
        let farm = |game: i64, graze: i64, built: i64| {
            let mut m = building_mock();
            m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_ROBOTIC_FARM);
            m.reads.insert(EE_UNITS_UPD_R_CLASS_UNARMED, 1);
            m.reads.insert(EE_UNITS_UPD_R_GAME_TIME, game);
            m.reads.insert(EE_UNITS_UPD_R_GRAZE_TIME, graze);
            m.reads.insert(EE_UNITS_UPD_R_BUILT, built);
            m.reads.insert(EE_UNITS_UPD_R_GATHER_TIME_6, 3001);
            m.reads.insert(EE_UNITS_UPD_R_WORK_TIME_FACTOR, i64::from(0.5f32.to_bits()));
            run(EE_UNITS_UPDATE_BUILDING, &mut m);
            m
        };
        let m = farm(10_000, 10_000, 1);
        // 0.5 * 3001 = 1500.5: to even, 1500
        assert_eq!(
            m.log[1..].to_vec(),
            vec![
                (EE_UNITS_UPD_ADD_RESOURCE, RESOURCE_FOOD, 1),
                (EE_UNITS_UPD_SAVED_GROUPS_RESET, 0, 0),
                (EE_UNITS_UPD_SET_GRAZE_TIME, 11_500, 0)
            ]
        );
        assert_eq!(farm(9_999, 10_000, 1).log.len(), 1);
        assert_eq!(farm(10_000, 10_000, 0).log.len(), 1);
        // the times compare unsigned
        assert_eq!(farm(-1, 10_000, 1).log.len(), 4);
    }

    #[test]
    fn the_gate_opens_and_closes() {
        let gate_at = |command: i64, open: f32, first: i64, next: i64| {
            let mut m = building_mock();
            m.reads.insert(EE_UNITS_UPD_R_COMMAND, command);
            m.reads.insert(EE_UNITS_UPD_R_OPEN_FRACTION, i64::from(open.to_bits()));
            m.reads.insert(EE_UNITS_UPD_R_FIRST_PART, first);
            m.reads.insert(EE_UNITS_UPD_R_HAS_NEXT_PART, next);
            m.reads.insert(EE_UNITS_UPD_R_FAMILY, FAMILY_WALLS);
            run(EE_UNITS_UPDATE_GATE, &mut m);
            m.log.iter().find(|l| l.0 == EE_UNITS_UPD_SET_OPEN_FRACTION).map(|l| f32::from_bits(l.1 as u32))
        };
        assert_eq!(gate_at(ACTION_OPENING, 0.4, 1, 1), Some(0.4f32 + 0.2f32));
        assert_eq!(gate_at(ACTION_OPENING, 0.9, 1, 1), Some(1.0));
        assert_eq!(gate_at(ACTION_CLOSING, 0.1, 1, 1), Some(0.0));
        assert_eq!(gate_at(ACTION_CLOSING, 0.5, 1, 1), Some(0.5f32 - 0.2f32));
        assert_eq!(gate_at(ACTION_CLOSING, 0.5, 0, 1), None);
        assert_eq!(gate_at(ACTION_CLOSING, 0.5, 1, 0), None);
        assert_eq!(gate_at(ACTION_IDLE, 0.5, 1, 1), None);
    }

    #[test]
    fn the_mine_works() {
        let mine = |timer: i64, tick: i64, has_mine: i64| {
            let mut m = building_mock();
            m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_GATHER_RESOURCE);
            m.reads.insert(EE_UNITS_UPD_R_WORK_TIMER, timer);
            m.reads.insert(EE_UNITS_UPD_R_TICK_MS, tick);
            m.reads.insert(EE_UNITS_UPD_R_MINE, has_mine);
            m.reads.insert(EE_UNITS_UPD_R_MINE_RESOURCE_TYPE, 5);
            m.reads.insert(EE_UNITS_UPD_R_GATHER_TIME_2, 4000);
            run(EE_UNITS_UPDATE_MINING_BUILDING, &mut m);
            m.log[1..].to_vec()
        };
        assert_eq!(mine(500, 100, 1), vec![(EE_UNITS_UPD_SET_WORK_TIMER, 400, 0)]);
        assert_eq!(mine(50, 100, 1), vec![(EE_UNITS_UPD_SET_WORK_TIMER, 0, 0)]);
        assert_eq!(mine(5, 0, 1), vec![(EE_UNITS_UPD_SET_WORK_TIMER, 4, 0)]);
        assert_eq!(
            mine(0, 100, 1),
            vec![
                (EE_UNITS_UPD_DEPLETE_MINE, 0, 0),
                (EE_UNITS_UPD_ADD_RESOURCE, 5, 1),
                (EE_UNITS_UPD_SET_WORK_TIMER, 4000, 0)
            ]
        );
        assert_eq!(mine(0, 100, 0), vec![]);
        // a negative timer stays
        assert_eq!(mine(-5, 100, 1), vec![]);
    }

    #[test]
    fn floating_buildings_and_ambients() {
        let mut m = building_mock();
        m.reads.insert(EE_UNITS_UPD_R_PHYSICS_MODEL, 1);
        m.reads.insert(EE_UNITS_UPD_R_REGENERATES, 1);
        run(EE_UNITS_UPDATE_SPACE_BUILDING, &mut m);
        assert_eq!(
            ops(&m),
            vec![
                EE_UNITS_UPD_PHYSICS_STEP,
                EE_UNITS_UPD_REFRESH_SOUND,
                EE_UNITS_UPD_REGENERATE,
                EE_UNITS_UPD_UPDATE_ARMED
            ]
        );
        let mut m = building_mock();
        m.reads.insert(EE_UNITS_UPD_R_REGENERATES, 1);
        run(EE_UNITS_UPDATE_NAVAL_BUILDING, &mut m);
        assert_eq!(ops(&m), vec![EE_UNITS_UPD_REFRESH_SOUND, EE_UNITS_UPD_UPDATE_ARMED]);
        let mut m = building_mock();
        run(EE_UNITS_UPDATE_AMBIENT, &mut m);
        assert!(m.log.is_empty());
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_UNIT_ROTTING);
        run(EE_UNITS_UPDATE_AMBIENT, &mut m);
        assert_eq!(m.log, vec![(EE_UNITS_UPD_ROTTING, 0, 0), (EE_UNITS_UPD_SET_PARTS_ROTTING, 1234, 0)]);
    }

    #[test]
    fn spaceships_submarines_and_helicopters() {
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_REGENERATES, 1);
        run(EE_UNITS_UPDATE_SPACESHIP, &mut m);
        let o = ops(&m);
        assert_eq!(o[1..4].to_vec(), vec![EE_UNITS_UPD_PHYSICS_STEP, EE_UNITS_UPD_TURRETS, EE_UNITS_UPD_ORBITALS]);
        assert!(!o.contains(&EE_UNITS_UPD_EFFECTS));
        assert_eq!(
            o[o.len() - 3..].to_vec(),
            vec![EE_UNITS_UPD_REGENERATE_POWER, EE_UNITS_UPD_REGENERATE, EE_UNITS_UPD_DONE]
        );
        let mut m = soldier();
        run(EE_UNITS_UPDATE_SUBMERGED_NAVAL, &mut m);
        let o = ops(&m);
        assert_eq!(o[1..3].to_vec(), vec![EE_UNITS_UPD_PHYSICS_STEP, EE_UNITS_UPD_EFFECTS]);
        let mut m = soldier();
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, ACTION_WAIT_FOR_RELOAD);
        run(EE_UNITS_UPDATE_HELICOPTER, &mut m);
        let o = ops(&m);
        assert_eq!(o[1..3].to_vec(), vec![EE_UNITS_UPD_PHYSICS_STEP, EE_UNITS_UPD_ORBITALS]);
        assert!(m.log.contains(&(EE_UNITS_UPD_FIRE_ON_ATTACK_FRAME, 77, 9)));
    }
}
