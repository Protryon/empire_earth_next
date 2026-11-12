//! The attack, attack-ground and heal goals' trigger tests and steps, and the shots' trigger tests (R5b.2, sixth
//! round): `EEUGAttackUnit`, `EEUGAttackGround`, `EEUGHealUnit::EvaluateCondition` and `Step`;
//! `EEUCProjectile`, `EEUCMissile`, `EEUCTorpedo::EvaluateCondition`.
//!
//! A trigger test reads the unit, its goal and the goal's target. The C++ gathers the plain reads into
//! [`EeCombatConditionIn`] (side-effect free, null-safe: a missing target leaves its fields 0); the other reads (the
//! line of sight, the seen, reach, facing, wall and path tests, the base class's test, the impact tests) are lazy:
//! each is a host call made only where and when the original makes it, and its answer is logged into the record in
//! order. The outcome is the result and an action the C++ applies (the shot's new attack goal).
//!
//! A step is a plan of world operations the C++ host executes as they are issued (the physics action, the facing
//! test, the animation, the shot or the reload timer's random draw, the path): every operation and its answer is
//! logged in order into [`EeCombatStepIn`], so that a capture of the record shows the plan and the C++ order.

use std::ffi::c_void;

/// `EEUGAttackUnit`.
pub const EE_COMBAT_GOAL_ATTACK_UNIT: i32 = 0;
/// `EEUGAttackGround`.
pub const EE_COMBAT_GOAL_ATTACK_GROUND: i32 = 1;
/// `EEUGHealUnit`.
pub const EE_COMBAT_GOAL_HEAL: i32 = 2;
/// `EEUCProjectile` (and `EEUCBomb`, which shares its test).
pub const EE_COMBAT_GOAL_PROJECTILE: i32 = 3;
/// `EEUCMissile`.
pub const EE_COMBAT_GOAL_MISSILE: i32 = 4;
/// `EEUCTorpedo` (its other triggers go to `EEUCMissile`'s test, a separate call: [`EE_COMBAT_READ_BASE`]).
pub const EE_COMBAT_GOAL_TORPEDO: i32 = 5;

// ---- the lazy reads of a trigger test (EeCombatRead::code); the value is 0/1, or an f32's bits where noted

/// `inUnit->GetLineOfSight()` (f32).
pub const EE_COMBAT_READ_LINE_OF_SIGHT: i32 = 1;
/// `EEIsInRangeToHit(inUnit, mTarget)`.
pub const EE_COMBAT_READ_IN_RANGE_TO_HIT: i32 = 2;
/// `inUnit->IsInReach(mTarget)`.
pub const EE_COMBAT_READ_IN_REACH: i32 = 3;
/// `EEIsSeenBy(mTarget, inUnit->mPlayer, false)` (attack); `true` as the last argument (heal).
pub const EE_COMBAT_READ_SEEN: i32 = 4;
/// `FacesTarget(inUnit, mTarget, true)`.
pub const EE_COMBAT_READ_FACES_ARCH: i32 = 5;
/// `FacesTarget(inUnit, mTarget, false)`.
pub const EE_COMBAT_READ_FACES: i32 = 6;
/// `FacesTarget(inUnit, mTarget->mComplexUnit, true)` (the moving test).
pub const EE_COMBAT_READ_FACES_MOVING: i32 = 7;
/// `inUnit->DistanceSquaredToUnit(mTarget)` (f32).
pub const EE_COMBAT_READ_DISTANCE_SQUARED: i32 = 8;
/// `inUnit->IsOnTileBorderingTarget(mTarget)`.
pub const EE_COMBAT_READ_BORDERING: i32 = 9;
/// `EEIsLineToUnitBlocked(inUnit, mTarget, ...)`.
pub const EE_COMBAT_READ_LINE_BLOCKED: i32 = 10;
/// `EEUGAttackUnit::IsGateClosed`.
pub const EE_COMBAT_READ_GATE_CLOSED: i32 = 11;
/// `EEUGAttackUnit::IsStillReachable` (it may make the unit ignore the target).
pub const EE_COMBAT_READ_STILL_REACHABLE: i32 = 12;
/// `EEIsEnemyWithinRange(inUnit)`.
pub const EE_COMBAT_READ_ENEMY_IN_RANGE: i32 = 13;
/// The base class's test of the event (the value is its `long` result): `EEUGLocation::EvaluateCondition` (heal),
/// `EEUnitClass::EvaluateCondition` (projectile, missile), `EEUCMissile::EvaluateCondition` (torpedo).
pub const EE_COMBAT_READ_BASE: i32 = 14;
/// `inUnit->CanReachTile(UMFloor(x), UMFloor(y))` of the attack-ground location.
pub const EE_COMBAT_READ_CAN_REACH_TILE: i32 = 15;
/// `FacesPoint(inUnit, location, true)` of the attack-ground location.
pub const EE_COMBAT_READ_FACES_POINT: i32 = 16;
/// `EEUGHealUnit::IsInRange(inUnit, mTarget)`.
pub const EE_COMBAT_READ_HEAL_IN_RANGE: i32 = 17;
/// `mTarget->IsRepairable()`.
pub const EE_COMBAT_READ_REPAIRABLE: i32 = 18;
/// `FindImpactUnit(inUnit)` found a unit (the C++ keeps it).
pub const EE_COMBAT_READ_IMPACT_UNIT: i32 = 19;
/// `EEUUGetIsUnitClassTargetable(inUnit, <the impact unit>->GetUnitClass())`.
pub const EE_COMBAT_READ_TARGETABLE: i32 = 20;
/// `CheckGroundImpact(inUnit)` (it may set an attack-ground goal).
pub const EE_COMBAT_READ_GROUND_IMPACT: i32 = 21;
/// `CheckWaterImpact(inUnit)` (it may set an attack-ground goal).
pub const EE_COMBAT_READ_WATER_IMPACT: i32 = 22;

/// No action.
pub const EE_COMBAT_CONDITION_NONE: i32 = 0;
/// `inUnit->SetGoal(new EEUGAttackUnit(inUnit, <the impact unit>, false, false), 0, 0)`.
pub const EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT: i32 = 1;

/// One lazy read and its answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatRead {
    /// `EE_COMBAT_READ_*`
    pub code: i32,
    /// 0/1, the `long` result, or an f32's bits
    pub value: u32,
}

/// A trigger test's reads (the capture's CBTC layout). The C++ fills everything above `reads`; the test logs its
/// lazy reads.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatConditionIn {
    /// `EE_COMBAT_GOAL_*`
    pub kind: i32,
    pub event: u32,
    pub unit_id: u32,
    /// the target's id (0 without one)
    pub target_id: u32,
    /// the unit's goal's `mKind` (-1 without one)
    pub goal_kind: i32,
    /// the unit's player `IsHuman`
    pub human: i32,
    /// the unit's `mStats->mRange`
    pub range: f32,
    pub reload_timer: i32,
    /// `mStats->mReloadTime`
    pub reload_time: i32,
    /// `mStats->mCanFireOverWalls`
    pub fire_over_walls: i32,
    /// `mHerd != NULL`
    pub herd: i32,
    /// the unit's class `IsCrusader`
    pub crusader: i32,
    /// the attack goal's `mIsConvert`
    pub convert: i32,
    /// the class's `mIsParatrooperBomber`
    pub paratrooper_bomber: i32,
    /// the unit's `mCloaked` (a missile's lifetime flag)
    pub cloaked: u32,
    /// `mFlightTime` and `mStats->mFlightTime`
    pub flight_time: i32,
    pub flight_time_max: i32,
    /// the class's `GetHealThreshold()`
    pub heal_threshold: f32,
    /// the target: the attack goal's, the heal goal's, or (shots) the attack-unit goal's
    pub has_target: i32,
    pub target_hp: i32,
    pub target_tile: [i32; 2],
    /// the attack goal's last known target tile
    pub goal_tile: [i32; 2],
    /// the target's complex unit is there, its velocity, cloak and container
    pub target_complex: i32,
    pub target_velocity: f32,
    pub target_cloakable: i32,
    /// `GetIsCloaked()`
    pub target_cloaked: i32,
    /// the target class's `mCloakRadius`
    pub target_cloak_radius: i32,
    pub target_container: i32,
    /// the target's player is the unit's
    pub target_same_player: i32,
    /// the unit's player is at war with the target's (`mEnemies[target player] != 0`)
    pub target_enemy: i32,
    /// the target's player is at war with the unit's
    pub target_enemy_of: i32,
    /// the target class's `mFamily`, `mCategory`, `IsLandBuilding()`
    pub target_family: u32,
    pub target_category: u32,
    pub target_land_building: i32,
    /// the target's `mStats->mAttack`, `mStats->mMaxHitPoints`
    pub target_attack: f32,
    pub target_max_hp: i32,
    /// the lazy reads made (may exceed the log's 4)
    pub reads: i32,
    pub log: [EeCombatRead; 4],
}

/// A trigger test's outcome.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatConditionOut {
    /// the test's value (`bool` 0/1; the shots' `long`)
    pub result: i32,
    /// `EE_COMBAT_CONDITION_*`
    pub action: i32,
}

// ---- the step's operations (EeCombatStepOp::op); a read's answer is in `value`

/// `inUnit->SetCurrentPhysicsAction(arg)`.
pub const EE_COMBAT_STEP_PHYSICS_ACTION: i32 = 1;
/// read: `FacesTarget(inUnit, mTarget, false)`.
pub const EE_COMBAT_STEP_FACES: i32 = 2;
/// read: `inUnit->mAnimation`.
pub const EE_COMBAT_STEP_ANIMATION: i32 = 3;
/// `inUnit->SetAnimation(arg, -1, 0)`.
pub const EE_COMBAT_STEP_SET_ANIMATION: i32 = 4;
/// `inUnit->DropPath()`.
pub const EE_COMBAT_STEP_DROP_PATH: i32 = 5;
/// `EEUGAttackUnit::RetargetGatePart(inUnit)`.
pub const EE_COMBAT_STEP_RETARGET_GATE: i32 = 6;
/// read: `EEGetAttackFrameSet(inUnit)`.
pub const EE_COMBAT_STEP_ATTACK_FRAME_SET: i32 = 7;
/// read: `inUnit->GetModel()->mSound1Frames[arg]`.
pub const EE_COMBAT_STEP_SOUND_FRAME: i32 = 8;
/// read: `inUnit->mPhysicalModel.GetFrame()`.
pub const EE_COMBAT_STEP_FRAME: i32 = 9;
/// `inUnit->mUnitClass->FireShot(inUnit)`.
pub const EE_COMBAT_STEP_FIRE_SHOT: i32 = 10;
/// `inUnit->SetRandomReloadTimer(arg)` (a physics random draw).
pub const EE_COMBAT_STEP_RELOAD_TIMER: i32 = 11;
/// `inUnit->SetTargetSearchTimer(arg)`.
pub const EE_COMBAT_STEP_SEARCH_TIMER: i32 = 12;
/// the goal's `SetPathAttempts(arg)`.
pub const EE_COMBAT_STEP_PATH_ATTEMPTS: i32 = 13;
/// read: `inUnit->mIsFiringHigh`.
pub const EE_COMBAT_STEP_FIRING_HIGH: i32 = 14;

/// One operation of a step, its argument and (a read) its answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatStepOp {
    /// `EE_COMBAT_STEP_*`
    pub op: i32,
    pub arg: i32,
    pub value: i32,
}

/// A goal step's reads and its plan (the capture's CBGS layout). The C++ fills everything above `count`; the step
/// logs its operations.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatStepIn {
    /// `EE_COMBAT_GOAL_ATTACK_UNIT`, `_ATTACK_GROUND` or `_HEAL`
    pub kind: i32,
    /// the unit's command's `mType` (0 without one)
    pub command: u32,
    pub unit_id: u32,
    /// the target's id (0 without one; attack ground: 0)
    pub target_id: u32,
    /// `mStats->mReloadTime`, `mStats->mCanFireOverWalls`
    pub reload_time: i32,
    pub fire_over_walls: i32,
    /// the operations issued (may exceed the log's 12)
    pub count: i32,
    pub log: [EeCombatStepOp; 12],
}

// ---- the trigger tests

// the triggers (ee/EEUnitAction.h)
const T_FLIGHT_TIME_ALARM: u32 = 1000;
const T_FLY_OFF_MAP_AFTER_ATTACK: u32 = 1009;
const T_ENEMY_UNIT_DESTROYED: u32 = 2003;
const T_UNIT_IN_WEAPON_RANGE: u32 = 2013;
const T_UNIT_NOT_ON_MAP: u32 = 2022;
const T_ENEMY_UNIT_LEFT_LOS: u32 = 2023;
const T_ENEMY_UNIT_MOVING: u32 = 2025;
const T_ENEMY_UNIT_NO_LONGER_VISIBLE: u32 = 2026;
const T_FACING_ENEMY_UNIT: u32 = 2027;
const T_ENEMY_UNIT_INSIDE_FIRING_ARCH: u32 = 2028;
const T_READY_TO_FIRE_AGAIN: u32 = 2031;
const T_UNIT_ON_TILE_BORDERING_TARGET: u32 = 2032;
const T_ATTACKER_IS_RANGED_UNIT: u32 = 2033;
const T_ENEMY_UNIT_MOVED: u32 = 2034;
const T_CEASE_FIRE: u32 = 2036;
const T_ENEMY_IS_BUILDING: u32 = 2038;
const T_BLOCKED_BY_WALL: u32 = 2042;
const T_TARGET_IS_NOT_DEFENSIVE_BUILDING: u32 = 2046;
const T_LOCATION_INSIDE_FIRING_ARCH: u32 = 2053;
const T_LOCATION_IN_WEAPON_RANGE: u32 = 2054;
const T_TARGET_IS_SPECIAL_ENEMY: u32 = 2055;
const T_ENEMY_UNIT_HAS_NOT_MOVED_ONE_TILE: u32 = 2056;
const T_ENEMY_UNIT_UNDETECTABLE: u32 = 2059;
const T_ENEMY_UNIT_REACHABLE: u32 = 5005;
const T_ENEMY_UNIT_IN_LOS: u32 = 5015;
const T_UNIT_HIT_SPACE: u32 = 5033;
const T_DYING_ANIMATION_FINISHED: u32 = 6000;
const T_RESOURCES_ROTTED_AWAY: u32 = 6001;
const T_UNIT_ROTTED_AWAY: u32 = 6002;
const T_UNIT_FACING_NOT_REQUIRED: u32 = 9014;
const T_PROJECTILE_HIT_UNIT: u32 = 14000;
const T_PROJECTILE_HIT_GROUND: u32 = 14001;
const T_PROJECTILE_HIT_WATER: u32 = 14002;
const T_UNIT_TARGET_NO_LONGER_VALID: u32 = 14003;
const T_NEW_TARGET_SELECTED: u32 = 14004;
const T_TARGET_NEEDS_HEALING: u32 = 18001;
const T_HEALEE_IN_HEALING_RANGE: u32 = 18002;
const T_GOAL_IS_HEAL: u32 = 18003;
const T_FACING_HEALEE: u32 = 18004;
const T_HEALEE_DESTROYED: u32 = 18005;
const T_HEALEE_NO_LONGER_VISIBLE: u32 = 18006;

// the actions a step switches on (ee/EEUnitAction.h)
const A_ATTACK_ENEMY_UNIT: u32 = 2001;
const A_ATTACK_LOCATION: u32 = 2002;
const A_FACE_ENEMY_UNIT: u32 = 2012;
const A_FACE_LOCATION: u32 = 2027;
const A_CHANGE_TARGET_FOR_SPECIAL_ENEMY: u32 = 2031;
const A_TURN_TO_FACE_HEALEE: u32 = 16002;

// kGoalAttackUnit (ee/EEUnitGoal.h), kUnitFamilyBuilding (ee/EEUnitClassIDs.h), the off-map tile, the turning set
const GOAL_ATTACK_UNIT: i32 = 2;
const FAMILY_BUILDING: u32 = 3;
const OFF_MAP: i32 = -10000;
const TURNING_ANIMATION: i32 = 0xd;
const HIGH_FIRING_ANIMATION: i32 = 8;

/// The lazy reads of a trigger test (`EeCombatConditionHost` over the C++, a mock in the tests).
pub trait ConditionHost {
    /// The answer to `EE_COMBAT_READ_*`: 0/1, the `long` result, or an f32's bits.
    fn read(&mut self, code: i32) -> u32;
}

/// The C++ host of a trigger test: `read` answers a lazy read over `context` (the unit, its goal or class, the
/// impact unit the C++ keeps between the impact read and the outcome).
#[repr(C)]
pub struct EeCombatConditionHost {
    pub context: *mut c_void,
    pub read: Option<unsafe extern "C" fn(context: *mut c_void, code: i32) -> u32>,
}

impl ConditionHost for &EeCombatConditionHost {
    fn read(&mut self, code: i32) -> u32 {
        let f = self.read.expect("the condition host has a read");
        // SAFETY: the C++ host answers over its own context for the duration of the test
        unsafe { f(self.context, code) }
    }
}

/// The record and the host: every lazy read goes through here, logged in order.
struct Reads<'a, H: ConditionHost> {
    rec: &'a mut EeCombatConditionIn,
    host: H,
}

impl<H: ConditionHost> Reads<'_, H> {
    fn read(&mut self, code: i32) -> u32 {
        let value = self.host.read(code);
        if let Ok(at) = usize::try_from(self.rec.reads) {
            if let Some(slot) = self.rec.log.get_mut(at) {
                *slot = EeCombatRead { code, value };
            }
        }
        self.rec.reads = self.rec.reads.wrapping_add(1);
        value
    }

    fn flag(&mut self, code: i32) -> bool {
        self.read(code) != 0
    }

    fn float(&mut self, code: i32) -> f32 {
        f32::from_bits(self.read(code))
    }
}

/// A trigger test of `rec.kind` on `rec.event`, its lazy reads made through `host` and logged into `rec`.
pub fn condition(rec: &mut EeCombatConditionIn, host: impl ConditionHost) -> EeCombatConditionOut {
    let mut r = Reads { rec, host };
    match r.rec.kind {
        EE_COMBAT_GOAL_ATTACK_UNIT => goal_out(attack_unit(&mut r)),
        EE_COMBAT_GOAL_ATTACK_GROUND => goal_out(attack_ground(&mut r)),
        EE_COMBAT_GOAL_HEAL => goal_out(heal(&mut r)),
        EE_COMBAT_GOAL_PROJECTILE => shot(&mut r, false),
        EE_COMBAT_GOAL_MISSILE => shot(&mut r, true),
        EE_COMBAT_GOAL_TORPEDO => torpedo(&mut r),
        _ => EeCombatConditionOut::default(),
    }
}

fn goal_out(result: bool) -> EeCombatConditionOut {
    EeCombatConditionOut { result: i32::from(result), action: EE_COMBAT_CONDITION_NONE }
}

/// `EEUnitGoal::EvaluateCondition`.
fn goal_base(event: u32) -> bool {
    event == T_UNIT_FACING_NOT_REQUIRED
}

/// `EEUGAttackUnit::EvaluateCondition`.
fn attack_unit<H: ConditionHost>(r: &mut Reads<'_, H>) -> bool {
    let i = *r.rec;
    match i.event {
        // a living target falls through to the hidden test (the original's switch)
        T_ENEMY_UNIT_DESTROYED => i.target_hp <= 0 || i.target_tile[0] == OFF_MAP || undetectable(&i),
        T_ENEMY_UNIT_UNDETECTABLE => undetectable(&i),
        T_UNIT_NOT_ON_MAP => i.target_tile[0] == OFF_MAP,
        T_UNIT_IN_WEAPON_RANGE => {
            if i.convert != 0 && i.crusader != 0 {
                r.flag(EE_COMBAT_READ_IN_RANGE_TO_HIT)
            } else {
                r.flag(EE_COMBAT_READ_IN_REACH)
            }
        }
        T_ENEMY_UNIT_NO_LONGER_VISIBLE => {
            (i.human != 0 || i.range > r.float(EE_COMBAT_READ_LINE_OF_SIGHT)) && !r.flag(EE_COMBAT_READ_SEEN)
        }
        T_ENEMY_UNIT_INSIDE_FIRING_ARCH => r.flag(EE_COMBAT_READ_FACES_ARCH),
        T_ENEMY_UNIT_LEFT_LOS => {
            let minimum = r.float(EE_COMBAT_READ_LINE_OF_SIGHT);
            if i.range > minimum {
                !r.flag(EE_COMBAT_READ_SEEN)
            } else {
                r.float(EE_COMBAT_READ_DISTANCE_SQUARED) > minimum * minimum
            }
        }
        T_ENEMY_UNIT_MOVING => i.target_complex != 0 && i.target_velocity > 0.0 && !r.flag(EE_COMBAT_READ_FACES_MOVING),
        T_FACING_ENEMY_UNIT => r.flag(EE_COMBAT_READ_FACES),
        T_READY_TO_FIRE_AGAIN => i.reload_timer >= i.reload_time,
        T_UNIT_ON_TILE_BORDERING_TARGET => r.flag(EE_COMBAT_READ_BORDERING),
        T_ATTACKER_IS_RANGED_UNIT => i.range > 1.0,
        T_TARGET_IS_NOT_DEFENSIVE_BUILDING => {
            i.has_target != 0 && i.target_land_building != 0 && i.target_attack == 0.0
        }
        T_BLOCKED_BY_WALL => i.fire_over_walls == 0 && r.flag(EE_COMBAT_READ_LINE_BLOCKED),
        T_ENEMY_IS_BUILDING => i.target_family == FAMILY_BUILDING,
        T_CEASE_FIRE => !(i.target_same_player == 0 && i.target_enemy != 0) && i.herd == 0,
        T_ENEMY_UNIT_MOVED => i.target_tile != i.goal_tile,
        T_TARGET_IS_SPECIAL_ENEMY => r.flag(EE_COMBAT_READ_GATE_CLOSED),
        T_ENEMY_UNIT_HAS_NOT_MOVED_ONE_TILE => i.target_tile == i.goal_tile,
        T_ENEMY_UNIT_REACHABLE => r.flag(EE_COMBAT_READ_STILL_REACHABLE),
        T_ENEMY_UNIT_IN_LOS => r.flag(EE_COMBAT_READ_ENEMY_IN_RANGE),
        event => goal_base(event),
    }
}

/// A cloaked enemy the unit cannot see through (the attack goal's hidden test).
fn undetectable(i: &EeCombatConditionIn) -> bool {
    i.has_target != 0
        && i.target_cloakable != 0
        && i.target_cloaked != 0
        && i.target_same_player == 0
        && i.target_enemy != 0
        && i.target_cloak_radius == 0
}

/// `EEUGAttackGround::EvaluateCondition`.
fn attack_ground<H: ConditionHost>(r: &mut Reads<'_, H>) -> bool {
    let i = *r.rec;
    match i.event {
        T_LOCATION_IN_WEAPON_RANGE => r.flag(EE_COMBAT_READ_CAN_REACH_TILE),
        T_LOCATION_INSIDE_FIRING_ARCH => r.flag(EE_COMBAT_READ_FACES_POINT),
        T_ATTACKER_IS_RANGED_UNIT => i.range > 1.0,
        T_READY_TO_FIRE_AGAIN => i.reload_timer == 0,
        T_FLY_OFF_MAP_AFTER_ATTACK => i.paratrooper_bomber != 0,
        event => goal_base(event),
    }
}

/// `EEUGHealUnit::EvaluateCondition`.
fn heal<H: ConditionHost>(r: &mut Reads<'_, H>) -> bool {
    let i = *r.rec;
    match i.event {
        T_HEALEE_NO_LONGER_VISIBLE => !(i.human == 0 || r.flag(EE_COMBAT_READ_SEEN)),
        T_HEALEE_DESTROYED => i.target_hp <= 0,
        T_FACING_HEALEE => r.flag(EE_COMBAT_READ_FACES),
        T_GOAL_IS_HEAL => true,
        T_HEALEE_IN_HEALING_RANGE => r.flag(EE_COMBAT_READ_HEAL_IN_RANGE),
        T_TARGET_NEEDS_HEALING => {
            // the original divides the hit points as floats (a `long` over a `long` promoted to float)
            r.flag(EE_COMBAT_READ_REPAIRABLE)
                && i.target_hp as f32 / i.target_max_hp as f32 <= i.heal_threshold
                && (i.target_same_player != 0 || i.target_enemy_of == 0)
        }
        _ => r.flag(EE_COMBAT_READ_BASE),
    }
}

/// `EEUCProjectile::EvaluateCondition` (`missile` false) and `EEUCMissile::EvaluateCondition`.
fn shot<H: ConditionHost>(r: &mut Reads<'_, H>, missile: bool) -> EeCombatConditionOut {
    let i = *r.rec;
    let mut action = EE_COMBAT_CONDITION_NONE;
    let flag = match i.event {
        T_PROJECTILE_HIT_UNIT => {
            if r.flag(EE_COMBAT_READ_IMPACT_UNIT) {
                // the missile attacks whatever it hit; the projectile only a unit its class may target
                if missile || r.flag(EE_COMBAT_READ_TARGETABLE) {
                    action = EE_COMBAT_CONDITION_ATTACK_IMPACT_UNIT;
                }
                true
            } else {
                // a missile whose lifetime ran out (mCloaked 0) hits nothing
                missile && i.cloaked == 0
            }
        }
        T_DYING_ANIMATION_FINISHED | T_RESOURCES_ROTTED_AWAY | T_UNIT_ROTTED_AWAY => {
            i.goal_kind == GOAL_ATTACK_UNIT || r.flag(EE_COMBAT_READ_BASE)
        }
        T_FLIGHT_TIME_ALARM if !missile => i.flight_time >= i.flight_time_max,
        T_PROJECTILE_HIT_GROUND => r.flag(EE_COMBAT_READ_GROUND_IMPACT),
        T_UNIT_TARGET_NO_LONGER_VALID => {
            i.goal_kind == GOAL_ATTACK_UNIT && (i.target_hp < 1 || (i.target_complex != 0 && i.target_container != 0))
        }
        T_PROJECTILE_HIT_WATER => r.flag(EE_COMBAT_READ_WATER_IMPACT),
        T_NEW_TARGET_SELECTED => true,
        T_UNIT_HIT_SPACE if missile => false,
        // the base class's test, its `long` returned as it is
        _ => return EeCombatConditionOut { result: r.read(EE_COMBAT_READ_BASE) as i32, action },
    };
    EeCombatConditionOut { result: i32::from(flag), action }
}

/// `EEUCTorpedo::EvaluateCondition` (the other triggers: `EEUCMissile`'s test, a host read).
fn torpedo<H: ConditionHost>(r: &mut Reads<'_, H>) -> EeCombatConditionOut {
    let result = match r.rec.event {
        T_PROJECTILE_HIT_GROUND => i32::from(r.flag(EE_COMBAT_READ_GROUND_IMPACT)),
        T_PROJECTILE_HIT_WATER => 0,
        _ => r.read(EE_COMBAT_READ_BASE) as i32,
    };
    EeCombatConditionOut { result, action: EE_COMBAT_CONDITION_NONE }
}

// ---- the steps

/// The world operations of a step (`EeCombatStepHost` over the C++, a mock in the tests).
pub trait StepHost {
    /// Executes `EE_COMBAT_STEP_*` with its argument; a read returns its answer, an action 0.
    fn op(&mut self, op: i32, arg: i32) -> i32;
}

/// The C++ host of a step: `op` executes an operation on the unit and its goal over `context`.
#[repr(C)]
pub struct EeCombatStepHost {
    pub context: *mut c_void,
    pub op: Option<unsafe extern "C" fn(context: *mut c_void, op: i32, arg: i32) -> i32>,
}

impl StepHost for &EeCombatStepHost {
    fn op(&mut self, op: i32, arg: i32) -> i32 {
        let f = self.op.expect("the step host has an op");
        // SAFETY: the C++ host acts on its own context for the duration of the step
        unsafe { f(self.context, op, arg) }
    }
}

struct Plan<'a, H: StepHost> {
    rec: &'a mut EeCombatStepIn,
    host: H,
}

impl<H: StepHost> Plan<'_, H> {
    fn issue(&mut self, op: i32, arg: i32, read: bool) -> i32 {
        let answer = self.host.op(op, arg);
        let value = if read { answer } else { 0 };
        if let Ok(at) = usize::try_from(self.rec.count) {
            if let Some(slot) = self.rec.log.get_mut(at) {
                *slot = EeCombatStepOp { op, arg, value };
            }
        }
        self.rec.count = self.rec.count.wrapping_add(1);
        value
    }

    fn act(&mut self, op: i32, arg: i32) {
        self.issue(op, arg, false);
    }

    fn read(&mut self, op: i32, arg: i32) -> i32 {
        self.issue(op, arg, true)
    }

    /// The facing branch: turn (animation 0xd) until the unit faces the target, without a path.
    fn face(&mut self) {
        self.act(EE_COMBAT_STEP_PHYSICS_ACTION, 1);
        if self.read(EE_COMBAT_STEP_FACES, 0) == 0 && self.read(EE_COMBAT_STEP_ANIMATION, 0) != TURNING_ANIMATION {
            self.act(EE_COMBAT_STEP_SET_ANIMATION, TURNING_ANIMATION);
        }
        self.act(EE_COMBAT_STEP_DROP_PATH, 0);
    }

    /// The attack branch after the set is chosen: the shot on the set's sound frame (read after the animation is
    /// set), else a random reload timer; the search timer, the path and the path attempts reset.
    fn fire(&mut self, set: i32) {
        let frame = self.read(EE_COMBAT_STEP_SOUND_FRAME, set);
        self.act(EE_COMBAT_STEP_SET_ANIMATION, set);
        if frame == self.read(EE_COMBAT_STEP_FRAME, 0) {
            self.act(EE_COMBAT_STEP_FIRE_SHOT, 0);
        } else {
            let reload = self.rec.reload_time;
            self.act(EE_COMBAT_STEP_RELOAD_TIMER, reload);
        }
        self.act(EE_COMBAT_STEP_SEARCH_TIMER, 0);
        self.act(EE_COMBAT_STEP_DROP_PATH, 0);
        self.act(EE_COMBAT_STEP_PATH_ATTEMPTS, 0);
    }
}

/// A goal's step on `rec.command`: its operations issued to `host` in the original's order and logged into `rec`.
pub fn step(rec: &mut EeCombatStepIn, host: impl StepHost) {
    let mut p = Plan { rec, host };
    let (kind, command) = (p.rec.kind, p.rec.command);
    match (kind, command) {
        (EE_COMBAT_GOAL_ATTACK_UNIT, A_CHANGE_TARGET_FOR_SPECIAL_ENEMY) => p.act(EE_COMBAT_STEP_RETARGET_GATE, 0),
        (EE_COMBAT_GOAL_ATTACK_UNIT, A_FACE_ENEMY_UNIT) | (EE_COMBAT_GOAL_HEAL, A_TURN_TO_FACE_HEALEE) => p.face(),
        (EE_COMBAT_GOAL_ATTACK_UNIT, A_ATTACK_ENEMY_UNIT) => {
            p.act(EE_COMBAT_STEP_PHYSICS_ACTION, 1);
            let set = p.read(EE_COMBAT_STEP_ATTACK_FRAME_SET, 0);
            p.fire(set);
        }
        (EE_COMBAT_GOAL_ATTACK_GROUND, A_FACE_LOCATION) => {
            p.act(EE_COMBAT_STEP_PHYSICS_ACTION, 1);
            p.act(EE_COMBAT_STEP_DROP_PATH, 0);
        }
        (EE_COMBAT_GOAL_ATTACK_GROUND, A_ATTACK_ENEMY_UNIT | A_ATTACK_LOCATION) => {
            p.act(EE_COMBAT_STEP_PHYSICS_ACTION, 1);
            let high = p.rec.fire_over_walls != 0 && p.read(EE_COMBAT_STEP_FIRING_HIGH, 0) != 0;
            p.fire(if high { HIGH_FIRING_ANIMATION } else { 0 });
        }
        _ => {}
    }
}

/// The trigger test of `input.kind`; the lazy reads go to `host` and are logged into `*input`.
///
/// # Safety
/// `input` must point to a valid record and `host` to a valid table whose function may be called with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_condition(
    input: *mut EeCombatConditionIn,
    host: *const EeCombatConditionHost,
) -> EeCombatConditionOut {
    // a copy: no Rust reference to the C++'s record lives across the host's calls (a read may run another test)
    let mut record = unsafe { *input };
    let out = condition(&mut record, unsafe { &*host });
    unsafe { *input = record };
    out
}

/// The step of `input.kind` on `input.command`: its operations go to `host` and are logged into `*input`.
///
/// # Safety
/// As [`ee_combat_condition`].
#[no_mangle]
pub unsafe extern "C" fn ee_combat_goal_step(input: *mut EeCombatStepIn, host: *const EeCombatStepHost) {
    let mut record = unsafe { *input };
    step(&mut record, unsafe { &*host });
    unsafe { *input = record };
}

#[cfg(test)]
#[path = "goals_tests.rs"]
mod logic_tests;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_records_are_the_capture_layout() {
        // every field 4 bytes, no padding (ee/EECombatFormulas.h's capture writes the bytes)
        assert_eq!(std::mem::size_of::<EeCombatRead>(), 8);
        assert_eq!(std::mem::size_of::<EeCombatConditionIn>(), 39 * 4 + 4 * 8);
        assert_eq!(std::mem::size_of::<EeCombatConditionOut>(), 8);
        assert_eq!(std::mem::size_of::<EeCombatStepOp>(), 12);
        assert_eq!(std::mem::size_of::<EeCombatStepIn>(), 7 * 4 + 12 * 12);
    }
}
