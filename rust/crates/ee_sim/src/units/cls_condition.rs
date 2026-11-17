//! The unit classes' trigger answers (R5b.4, the thirty-fourth round): `EEUnitClass::EvaluateCondition`'s dispatch by
//! the class's kind and every kind's body (the citizens', priests', buildings', gate's, mine's, ships', helicopters',
//! spaceships' and the fortress's), and the common answer every kind falls back on (`BaseEvaluateCondition`,
//! EEUnitAI.cpp 0x62e47a), over the unit records and one host ([`CondHost`]: the game's is cls_host.rs over
//! `ee_units_cls_cond_host`, EEUnitAI.cpp).
//!
//! Each answer is 0 (false), 1 (true) or 2 (true from an active goal that is not the unit's current goal). The
//! shots' kinds (torpedo, missile, projectile) answer through ee_combat's goals ([`Answer::Combat`]); the landing
//! kinds through cls_air.rs (the host's `landing`).
//!
//! The reads with no record (the goals, the behaviour, the herd, the attackers, the reach and damage tests, the
//! path manager's statements) are the host's operations, each the original's expression or statement; the unit's
//! and its class's plain members are read from their records in place at the moment the original read them, so a
//! statement that changes the unit (an obstacle's wait, a path refresh) is seen by the reads after it. Pinned by
//! USST kind 2 (every trigger's answer, golden from the C++ bodies).
//!
//! The original bodies: the common one 0x62e47a; the citizen's 0x60d10e, the priest's 0x60dd2d, the building's
//! 0x60fb85, the temple's 0x6100c1, the fortress's 0x610357, the gate's 0x611c35, the priest helicopter's 0x613344,
//! the helicopter transport's 0x6137f8, the mine's 0x61726d, the ship's 0x617eb2, the dock's 0x6183f8, the fishing
//! boat's 0x618664, the priest ship's 0x618942, the spaceship's 0x61a59e and 0x61ab3b, the submarine's 0x61b010.

// the original's statements as written (a flag set in a match arm, its float constants' bits)
// `c_long` is 32 bits on Windows and 64 on LP64 (the casts are needed there); the class records the entry points take
// are live objects the C++ hands over (as every unit record read in place)
#![allow(clippy::float_cmp, clippy::unnecessary_cast, clippy::not_unsafe_ptr_arg_deref)]

use crate::units::kinds::*;
use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::units::uu::EeUnitsUuUnit;
use ee_base::math::types::Ge3DPoint;

/// What `EvaluateCondition` answers for a class.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Answer {
    /// the answer (0, 1 or 2)
    Value(i32),
    /// a shot's: ee_combat's goal condition answers (`EE_COMBAT_GOAL_TORPEDO`, the missile's member, the projectile's)
    Combat(CombatKind),
}

/// The shot kinds whose triggers ee_combat answers.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CombatKind {
    Torpedo,
    Missile,
    Projectile,
}

// ---- the host's operations (`ee_units_cls_cond_host`'s `op`; `a` the unit, `b` another unit, class or goal)
/// unit `a`'s `mActiveGoal` (`c` 0) or `mGoalState` (`c` 1)
pub const EE_UNITS_COND_GOAL: i32 = 1;
/// goal `b`'s `mKind`, with `mB1C` as bit 8 and (a move goal) `GetIsForcedMove()` as bit 9
pub const EE_UNITS_COND_GOAL_INFO: i32 = 2;
/// goal `b`'s `EvaluateCondition(unit a, c)`
pub const EE_UNITS_COND_GOAL_CONDITION: i32 = 3;
/// goal `b` started at least `c` ms ago (`gPhysics->mGameTime - GetStartTime()`, unsigned)
pub const EE_UNITS_COND_GOAL_ELAPSED: i32 = 4;
/// attack goal `b`'s `GetAttackTarget()`
pub const EE_UNITS_COND_GOAL_TARGET: i32 = 5;
/// unit `a`'s `mBehavior`: `mCanTargetEnemies` 1, `mCanFlee` 2, `mCanPursuePastInitialContactLOS` 4,
/// `mReturnsToInitialContactLocation` 8
pub const EE_UNITS_COND_BEHAVIOR: i32 = 6;
/// unit `a`'s `mBehavior->CanMoveFreely(a)`
pub const EE_UNITS_COND_MOVE_FREELY: i32 = 7;
/// `EEIsLocationGoalQueued(a)`
pub const EE_UNITS_COND_LOCATION_QUEUED: i32 = 8;
/// unit `a`'s `mLastAttacker`
pub const EE_UNITS_COND_LAST_ATTACKER: i32 = 9;
/// unit `a`'s `GetConditionTarget()`
pub const EE_UNITS_COND_CONDITION_TARGET: i32 = 10;
/// unit `a`'s `CanReachUnit(b)`
pub const EE_UNITS_COND_CAN_REACH: i32 = 11;
/// unit `a`'s `IsInReach(b)`
pub const EE_UNITS_COND_IN_REACH: i32 = 12;
/// class `b`'s `CanBeAttacked()`
pub const EE_UNITS_COND_CAN_BE_ATTACKED: i32 = 13;
/// `EEDamageFactor(c, d)` into `f`
pub const EE_UNITS_COND_DAMAGE_FACTOR: i32 = 14;
/// `EEFindDropLocation(a, &target, &p, NULL)`: the target, the location into `p`
pub const EE_UNITS_COND_DROP_LOCATION: i32 = 15;
/// unit `a`'s `GetLineOfSight()` into `f`
pub const EE_UNITS_COND_LINE_OF_SIGHT: i32 = 16;
/// `EEGetHitDistanceSquared(a, b, -10000, -10000)` into `f`
pub const EE_UNITS_COND_HIT_DISTANCE: i32 = 17;
/// unit `a`'s `DistanceSquared3D(p)` into `f`
pub const EE_UNITS_COND_DISTANCE_3D: i32 = 18;
/// unit `a`'s player's attack response range (its computer player's, else the behaviours table's) into `f`
pub const EE_UNITS_COND_RESPONSE_RANGE: i32 = 19;
/// unit `a`'s `DistanceSquaredTo(b)` into `f`
pub const EE_UNITS_COND_DISTANCE_TO: i32 = 20;
/// `EEIsEnemyWithinRange(a)`
pub const EE_UNITS_COND_ENEMY_IN_RANGE: i32 = 21;
/// unit `a`'s player: a computer player 1, a human player 2
pub const EE_UNITS_COND_OWNER: i32 = 22;
/// unit `a`'s location: off the mesh (`c` 0), on a space tile (`c` 1), farther from its tile than the world's stray
/// distance (`c` 2)
pub const EE_UNITS_COND_OFF_MAP: i32 = 23;
/// unit `a`'s `GetNumCargoUnits()`
pub const EE_UNITS_COND_CARGO: i32 = 24;
/// unit `a`'s model's frame count of its animation
pub const EE_UNITS_COND_FRAME_COUNT: i32 = 25;
/// unit `a`'s `GetMaxVelocity()` into `f`
pub const EE_UNITS_COND_MAX_VELOCITY: i32 = 26;
/// the obstacle trigger's statement on unit `a` (the wait drawn, the count reset)
pub const EE_UNITS_COND_OBSTACLE: i32 = 27;
/// unit `a`'s `ShouldMoveToNextAutoWaypoint()`
pub const EE_UNITS_COND_AUTO_WAYPOINT: i32 = 28;
/// the waypoint triggers' statements on unit `a` (`c`: no more waypoints 0, next retrieved 1, arrived 2, one
/// remaining 3)
pub const EE_UNITS_COND_WAYPOINT_STEP: i32 = 29;
/// unit `a`'s herd (0: none, else 1 | group attack 2 | parental vengeance 4 | group flee 8)
pub const EE_UNITS_COND_HERD: i32 = 30;
/// class `b`'s `AttackNearby(a, goal c)`
pub const EE_UNITS_COND_ATTACK_NEARBY: i32 = 31;
/// unit `a`'s `mInitialContactLocation` into `p`
pub const EE_UNITS_COND_INITIAL_CONTACT: i32 = 32;
/// unit `a`'s `mWaypoints.size()`
pub const EE_UNITS_COND_WAYPOINTS: i32 = 33;
/// unit `a`'s `GetPosition()` into `p` (virtual)
pub const EE_UNITS_COND_POSITION: i32 = 34;
/// `gWorld.mMap->GetGroundPoint(p)`'s z into `f`
pub const EE_UNITS_COND_GROUND_Z: i32 = 35;
/// unit `a`'s tile's `GetHeight()` into `f`, its terrain the answer
pub const EE_UNITS_COND_TILE: i32 = 36;
/// unit `a`'s citizen build queue: its size (`c` 0); the next build started (`c` 1: popped, a build goal to it, the
/// action begun)
pub const EE_UNITS_COND_BUILD_QUEUE: i32 = 37;
/// `EECanConvert(a, b, flag)`
pub const EE_UNITS_COND_CAN_CONVERT: i32 = 38;
/// `EEGetConversionCost(a)`
pub const EE_UNITS_COND_CONVERSION_COST: i32 = 39;
/// unit `a`'s attack goal (`EEAttackUnitCast(GetGoal())`): 0 none; else 1, its target valid for a conversion 2, its
/// `IsConvertAttack()` 4, its target's player is not an enemy 8
pub const EE_UNITS_COND_ATTACK_GOAL: i32 = 40;
/// unit `a`'s class's `IsCrusader()`
pub const EE_UNITS_COND_CRUSADER: i32 = 41;
/// the sunk test of the ships (`DyingAnimationFinished`): unit `a`'s transformed bounding box below the water
pub const EE_UNITS_COND_SUNK: i32 = 42;
/// class `b`'s `SelectUnitToRelease(a)` (`c` 0), `EruptVolcano(a)` (1), `HasSunk(a)` (2), `AreEnemiesNear(a)` (3),
/// `IsPasserNear(a)` (4)
pub const EE_UNITS_COND_MEMBER: i32 = 43;
/// the launch triggers' statements on building `a` (`c`: no launch requests 0, take-off clearance 1)
pub const EE_UNITS_COND_LAUNCH: i32 = 44;
/// unit `a`'s `GetQueuedGoal(c) != NULL`
pub const EE_UNITS_COND_QUEUED_GOAL: i32 = 45;
/// unit `a`'s mine is gone or empty
pub const EE_UNITS_COND_MINE_DEPLETED: i32 = 46;
/// unit `a`'s goal (`GetGoal()`) is an attack goal whose target has hit points
pub const EE_UNITS_COND_SECONDARY_TARGET: i32 = 47;

/// One host operation's values in and out.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeUnitsCondIo {
    pub f: f32,
    pub p: [f32; 3],
}

/// The world the trigger answers ask.
pub trait CondHost {
    /// a unit's records (the unit utilities' read)
    fn unit(&mut self, obj: u64) -> EeUnitsUuUnit;
    /// a class object's record
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields;
    /// unit `a`'s location (`mPhysicalModel.GetLocation()`'s x, y, z)
    fn location(&mut self, unit: u64) -> [f32; 3];
    /// one of the operations above
    fn op(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsCondIo) -> i64;
    /// the landing kinds' answer (cls_air.rs; None: the common one)
    fn landing(&mut self, class: u64, unit: u64, event: u32, animal: bool) -> Option<i32>;
}

// the triggers
const TRIGGER_CAN_I_TARGET_ENEMIES: u32 = 2047;
const TRIGGER_AM_I_UNDER_ATTACK: u32 = 2037;
const TRIGGER_ATTACK_MOVE_ENABLED: u32 = 2029;
const TRIGGER_AMMO_EXHAUSTED: u32 = 2058;
const TRIGGER_RELOADED: u32 = 2044;
const TRIGGER_ATTACKER_IS_NOT_BUILDING: u32 = 2043;
const TRIGGER_ATTACKER_IS_REACHABLE: u32 = 2040;
const TRIGGER_CAN_DAMAGE_ATTACKER: u32 = 2041;
const TRIGGER_CAN_FLEE: u32 = 2045;
const TRIGGER_CAN_I_MOVE_FREELY: u32 = 2050;
const TRIGGER_CAN_I_PURSUE_PAST_INITIAL_CONTACT_LOS: u32 = 2051;
const TRIGGER_ENEMY_INSIDE_INITIAL_CONTACT_LOS: u32 = 2048;
const TRIGGER_ENEMY_WITHIN_MINIMUM_RANGE: u32 = 2035;
const TRIGGER_HAS_VALID_INITIAL_CONTACT_LOCATION: u32 = 2052;
const TRIGGER_IS_ATTACKER_IN_RETALIATION_RANGE: u32 = 2039;
const TRIGGER_RETURNS_TO_INITIAL_CONTACT_LOCATION: u32 = 2049;
const TRIGGER_UNIT_IDLE_OR_ATTACKING_DEFENSELESS_BUILDING: u32 = 2057;
const TRIGGER_ATTACKER_IN_WEAPON_RANGE: u32 = 2060;
const TRIGGER_GOAL_IS_LOCATION: u32 = 5009;
const TRIGGER_SAFE_LOCATION_FOUND: u32 = 11001;
const TRIGGER_TAKING_LIGHT_DAMAGE: u32 = 5000;
const TRIGGER_GOAL_IS_UNIT: u32 = 5010;
const TRIGGER_ENEMY_UNIT_IN_LOS: u32 = 5015;
const TRIGGER_OWNER_IS_COMPUTER: u32 = 5014;
const TRIGGER_OWNER_IS_HUMAN: u32 = 5013;
const TRIGGER_GOAL_IS_PLAYER_INITIATED: u32 = 5016;
const TRIGGER_ONE_SECOND_ELAPSED: u32 = 5024;
const TRIGGER_GOAL_IS_ATTACK_LOCATION: u32 = 5023;
const TRIGGER_UNIT_HAS_GOAL: u32 = 5022;
const TRIGGER_GOAL_IS_CALAMITY: u32 = 5020;
const TRIGGER_GOAL_IS_EXPLORE: u32 = 5017;
const TRIGGER_TWO_SECONDS_ELAPSED: u32 = 5025;
const TRIGGER_LOCATION_OFF_MAP: u32 = 5027;
const TRIGGER_LOCATION_IN_SPACE: u32 = 5031;
const TRIGGER_GOAL_IS_NOT_CALAMITY: u32 = 5030;
const TRIGGER_LOCATION_WAY_OFF_MAP: u32 = 5028;
const TRIGGER_ONE_AND_A_HALF_SECONDS_ELAPSED: u32 = 5029;
const TRIGGER_THREE_SECONDS_ELAPSED: u32 = 5026;
const TRIGGER_CARGO_IS_EMPTY: u32 = 5032;
const TRIGGER_UNIT_HIT_SPACE: u32 = 5033;
const TRIGGER_REACHED_BOTTOM_OF_SPACE: u32 = 5034;
const TRIGGER_DYING_ANIMATION_FINISHED: u32 = 6000;
const TRIGGER_CAN_I_MOVE: u32 = 8012;
const TRIGGER_MOVE_PREPARATION_COMPLETE: u32 = 8000;
const TRIGGER_UNIT_ROTTED_AWAY: u32 = 6002;
const TRIGGER_RESOURCE_DEPLETED: u32 = 4008;
const TRIGGER_RESOURCES_ROTTED_AWAY: u32 = 6001;
const TRIGGER_OBSTACLE_DETECTED: u32 = 9000;
const TRIGGER_SHOULD_MOVE_TO_NEXT_AUTO_WAYPOINT: u32 = 9016;
const TRIGGER_GOAL_IS_PATROL: u32 = 9013;
const TRIGGER_NO_MORE_WAYPOINTS: u32 = 9006;
const TRIGGER_NEXT_WAYPOINT_RETRIEVED: u32 = 9005;
const TRIGGER_NEXT_PATROL_POINT_RETRIEVED: u32 = 8010;
const TRIGGER_ARRIVED_AT_MOVE_WAYPOINT: u32 = 9004;
const TRIGGER_ONE_WAYPOINT_REMAINING: u32 = 9017;
const TRIGGER_IS_ADULT: u32 = 19005;
const TRIGGER_IS_BABY: u32 = 19004;
const TRIGGER_HEAL_SEARCH_TIME_ELAPSED: u32 = 18000;
const TRIGGER_GOAL_IS_NOT_ATTACK: u32 = 12014;
const TRIGGER_HAS_GROUP_ATTACK: u32 = 19006;
const TRIGGER_CAN_I_SEE_ENEMY: u32 = 20002;
const TRIGGER_ENEMY_UNIT_SPOTTED: u32 = 5004;
const TRIGGER_SHOULD_I_RETALIATE: u32 = 20001;
const TRIGGER_SHOULD_I_RUN: u32 = 20000;
const TRIGGER_GOAL_IS_NOT_PLAYER_INITIATED: u32 = 5021;
const TRIGGER_HAS_PARENTAL_VENGANCE: u32 = 19008;
const TRIGGER_HAS_GROUP_FLEE: u32 = 19007;
const TRIGGER_ALWAYS_TRUE: u32 = 5018;
// the citizens'
const TRIGGER_BUILDING_IN_BUILD_QUEUE: u32 = 4024;
const TRIGGER_BUILD_QUEUE_EMPTY: u32 = 4032;
const TRIGGER_SMART_TASK_NOT_ASSIGNED: u32 = 4033;
const TRIGGER_GOAL_IS_REPAIR_SITE: u32 = 5011;
/// the citizens' triggers their active goal answers
const CITIZEN_GOAL_TRIGGERS: [u32; 22] = [
    2001, // BuildingDestroyed
    4001, // BuildingRepaired
    4003, // ResourceCapacityFilled
    4005, // ResourceCapacityEmpty
    4006, // NewResourceFound
    4008, // ResourceDepleted
    4009, // ResourceInRange
    4010, // StorageInRange
    4011, // StorageFound
    4014, // RepairSiteInRange
    4020, // StorageDestroyed
    4021, // BuildSiteClear
    4022, // BuildSiteVisible
    4023, // BuildSiteValid
    4026, // ResourceAccessible
    4027, // WaitingAreaInRange
    4028, // ResourceTargetIsTree
    4029, // OpenPositionAvailable
    4031, // GoalIsBuild
    5007, // GoalIsResource
    5008, // GoalIsStorage
    5019, // GoalIsWaitingArea
];
// the priests'
const TRIGGER_ATTACKER_IS_LEGAL_CONVERSION_TARGET: u32 = 15009;
const TRIGGER_HAS_ENOUGH_MANA_FOR_CONVERSION: u32 = 15008;
const TRIGGER_CONVERT_GOAL_STILL_VALID: u32 = 15006;
const TRIGGER_GOAL_IS_NOT_CONVERSION: u32 = 15005;
const TRIGGER_GOAL_IS_CONVERSION: u32 = 15004;
const TRIGGER_ENEMY_UNIT_CONVERTED: u32 = 15001;
// the buildings'
const TRIGGER_AIRPLANE_AVAILABLE_TO_LAUNCH: u32 = 13000;
const TRIGGER_CHOOSE_RANDOM_VOLCANO_TARGET: u32 = 2030;
const TRIGGER_SECONDARY_TARGET_FOUND: u32 = 1004;
const TRIGGER_CALAMITY_TARGET_STILL_VALID: u32 = 15007;
const TRIGGER_NO_LAUNCH_REQUESTS: u32 = 13003;
const TRIGGER_TAKEOFF_CLEARANCE_GRANTED: u32 = 13001;
const TRIGGER_LAUNCHED_AIRPLANE: u32 = 13002;
const TRIGGER_FIGHTER_HAS_GOAL: u32 = 16000;
// the gate's
const TRIGGER_IS_OPEN: u32 = 17000;
const TRIGGER_IS_CLOSED: u32 = 17001;
const TRIGGER_LOCKED: u32 = 17004;
const TRIGGER_NOT_LOCKED: u32 = 17005;
const TRIGGER_SPECIAL_UNIT_IN_OPENING_RANGE: u32 = 17006;
const TRIGGER_SPECIAL_UNIT_NOT_IN_OPENING_RANGE: u32 = 17007;
const TRIGGER_UNIT_IN_OPENING_RANGE: u32 = 17002;
const TRIGGER_UNIT_NOT_IN_OPENING_RANGE: u32 = 17003;

const GOAL_LOCATION: u32 = 1;
const GOAL_ATTACK_UNIT: u32 = 2;
const GOAL_ATTACK_GROUND: u32 = 3;
const GOAL_BUILD_REPAIR: u32 = 4;
const GOAL_PATROL: u32 = 9;
const GOAL_EXPLORE: u32 = 10;
const GOAL_CAST_CALAMITY: u32 = 11;
const FAMILY_BUILDING: u32 = 3;
const TERRAIN_SPACE: i32 = 12;
const UNIT_TYPE_CAPITOL: u32 = 0x29;
const UNIT_TYPE_SPACE_TURRET: u32 = 0x302;
const UNIT_TYPE_ORBITAL_SPACE_STATION: u32 = 0x31f;
const UNIT_TYPE_SPACE_DOCK: u32 = 0x2d8;
/// the "no heading" marker of `mInitialContactHeading` (EEUnitAI.cpp `kUnitAINoHeading`)
const NO_HEADING: f32 = -10000.0;
/// `kUnitAIDiveDepth`: the bottom of space
const DIVE_DEPTH: f32 = -6.0;
/// `kBuildingSunk`: below it a building is gone; the fortress's the same
const BUILDING_SUNK: f32 = -2.0;
/// `kGateOpen`, `kGateClosed`: the gate's open fraction
const GATE_OPEN: f32 = 1.0;
const GATE_CLOSED: f32 = 0.0;

const MEMBER_SELECT_RELEASE: i64 = 0;
const MEMBER_ERUPT_VOLCANO: i64 = 1;
const MEMBER_HAS_SUNK: i64 = 2;
const MEMBER_ENEMIES_NEAR: i64 = 3;
const MEMBER_PASSER_NEAR: i64 = 4;

/// A goal's kind and flags (`EE_UNITS_COND_GOAL_INFO`).
#[derive(Clone, Copy, Debug)]
struct Goal {
    obj: u64,
    kind: u32,
    b1c: bool,
    forced: bool,
}

/// The unit, its class (the `this` the body ran on) and the event.
struct Ctx {
    class: u64,
    cls: *const EeUnitsUnitClassFields,
    unit: u64,
    event: u32,
}

impl Ctx {
    fn cls(&self) -> &EeUnitsUnitClassFields {
        // SAFETY: a live class's record, read in place
        unsafe { &*self.cls }
    }
}

fn ask<H: CondHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, c: i64) -> i64 {
    h.op(op, a, b, c, 0, &mut EeUnitsCondIo::default())
}

fn ask_f<H: CondHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, c: i64, d: i64) -> f32 {
    let mut io = EeUnitsCondIo::default();
    h.op(op, a, b, c, d, &mut io);
    io.f
}

fn goal<H: CondHost + ?Sized>(h: &mut H, unit: u64, state: bool) -> Option<Goal> {
    let obj = ask(h, EE_UNITS_COND_GOAL, unit, 0, i64::from(state)) as u64;
    if obj == 0 {
        return None;
    }
    let info = ask(h, EE_UNITS_COND_GOAL_INFO, unit, obj, 0);
    Some(Goal { obj, kind: (info & 0xff) as u32, b1c: info & 0x100 != 0, forced: info & 0x200 != 0 })
}

/// A unit's records (the complex unit's, the unit's and its class's), read in place.
struct Rec(EeUnitsUuUnit);

impl Rec {
    fn cx(&self) -> &crate::units::unit_fields::EeUnitsComplexUnitFields {
        // SAFETY: a live complex unit's record, read in place
        unsafe { &*self.0.cx }
    }
    fn hit_points(&self) -> i32 {
        // SAFETY: a live unit's record
        unsafe { (*self.0.unit).mHitPoints as i32 }
    }
    fn resource_amount(&self) -> i32 {
        // SAFETY: a live unit's record
        unsafe { (*self.0.unit).mResourceAmount as i32 }
    }
    fn class(&self) -> &EeUnitsUnitClassFields {
        // SAFETY: a live class's record
        unsafe { &*self.0.class_fields }
    }
    fn flag(&self, field: &bool) -> bool {
        // SAFETY: a bool member of a live record (read as its byte)
        unsafe { crate::units::unit_fields::bool_byte(field) != 0 }
    }
}

fn rec<H: CondHost + ?Sized>(h: &mut H, unit: u64) -> Rec {
    Rec(h.unit(unit))
}

/// The class's kinds (its category's entry).
pub fn class_kinds(cls: &EeUnitsUnitClassFields) -> u64 {
    EE_UNIT_CLASS_KINDS[(cls.mCategory & 0xff) as usize]
}

/// `EEUnitClass::CanAttackMove()` by the kinds.
pub fn can_attack_move(kinds: u64) -> bool {
    if kinds & EE_CLASS_KIND_AIR_ANIMAL != 0 {
        return false;
    }
    kinds
        & (EE_CLASS_KIND_HELICOPTER_TRANSPORT
            | EE_CLASS_KIND_BASIC_PRIEST
            | EE_CLASS_KIND_HELICOPTER
            | EE_CLASS_KIND_LAND
            | EE_CLASS_KIND_LANDABLE
            | EE_CLASS_KIND_NAVAL
            | EE_CLASS_KIND_SPACESHIP
            | EE_CLASS_KIND_SUBMERGED_NAVAL)
        != 0
}

/// `EEUnitClass::EvaluateCondition`: the kinds tested deepest first, as the vtables resolved an override.
pub fn condition<H: CondHost + ?Sized>(h: &mut H, class: u64, unit: u64, event: u32) -> Answer {
    let cls = h.class(class);
    let x = Ctx { class, cls, unit, event };
    let k = class_kinds(x.cls());
    let v = if k & EE_CLASS_KIND_AIR_ANIMAL != 0 {
        landing(h, &x, true)
    } else if k & EE_CLASS_KIND_FORTRESS != 0 {
        fortress(h, &x)
    } else if k & EE_CLASS_KIND_BUILDING_PRIEST != 0 {
        building_priest(h, &x)
    } else if k & EE_CLASS_KIND_GATE != 0 {
        gate(h, &x)
    } else if k & EE_CLASS_KIND_HELICOPTER_PRIEST != 0 {
        helicopter(h, &x, true)
    } else if k & (EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_HELICOPTER) != 0 {
        helicopter(h, &x, false)
    } else if k & EE_CLASS_KIND_MINING_BUILDING != 0 {
        mining_building(h, &x)
    } else if k & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING) != 0 {
        naval_building(h, &x)
    } else if k & EE_CLASS_KIND_NAVAL_CITIZEN != 0 {
        ship(h, &x, citizen)
    } else if k & EE_CLASS_KIND_NAVAL_PRIEST != 0 {
        ship(h, &x, priest)
    } else if k & (EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL) != 0 {
        ship(h, &x, base_of)
    } else if k & EE_CLASS_KIND_SPACE_TRANSPORT != 0 {
        spaceship(h, &x)
    } else if k & EE_CLASS_KIND_TORPEDO != 0 {
        return Answer::Combat(CombatKind::Torpedo);
    } else if k & EE_CLASS_KIND_BASIC_CITIZEN != 0 {
        citizen(h, &x)
    } else if k & EE_CLASS_KIND_BASIC_PRIEST != 0 {
        priest(h, &x)
    } else if k & EE_CLASS_KIND_BUILDING != 0 {
        building(h, &x)
    } else if k & EE_CLASS_KIND_LANDABLE != 0 {
        landing(h, &x, false)
    } else if k & EE_CLASS_KIND_MISSILE != 0 {
        return Answer::Combat(CombatKind::Missile);
    } else if k & EE_CLASS_KIND_PROJECTILE != 0 {
        return Answer::Combat(CombatKind::Projectile);
    } else if k & EE_CLASS_KIND_SPACESHIP != 0 {
        spaceship(h, &x)
    } else if k & EE_CLASS_KIND_SUBMERGED_NAVAL != 0 {
        submerged_naval(h, &x)
    } else {
        base_of(h, &x)
    };
    Answer::Value(v)
}

fn landing<H: CondHost + ?Sized>(h: &mut H, x: &Ctx, animal: bool) -> i32 {
    match h.landing(x.class, x.unit, x.event, animal) {
        Some(v) => v,
        None => base_of(h, x),
    }
}

fn base_of<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    base(h, x.class, x.cls, x.unit, x.event)
}

/// `FortressEvaluateCondition`: sunk below -2 when dying.
fn fortress<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    if x.event != TRIGGER_DYING_ANIMATION_FINISHED {
        return base_of(h, x);
    }
    i32::from(h.location(x.unit)[2] < BUILDING_SUNK)
}

/// `HelicopterPriestEvaluateCondition` (`priest`) and the helicopters' and helicopter transports': dying done below
/// its tile's height, the move prepared with waypoints; the others the priest's or the common answer.
fn helicopter<H: CondHost + ?Sized>(h: &mut H, x: &Ctx, is_priest: bool) -> i32 {
    match x.event {
        TRIGGER_DYING_ANIMATION_FINISHED => {
            let z = h.location(x.unit)[2];
            let mut io = EeUnitsCondIo::default();
            h.op(EE_UNITS_COND_TILE, x.unit, 0, 0, 0, &mut io);
            i32::from(z < io.f)
        }
        TRIGGER_MOVE_PREPARATION_COMPLETE => i32::from(ask(h, EE_UNITS_COND_WAYPOINTS, x.unit, 0, 0) > 0),
        _ if is_priest => priest(h, x),
        _ => base_of(h, x),
    }
}

/// `SpaceshipEvaluateCondition` and the space transport's (`SpaceshipVirtualA0`): dying done at once, the move
/// prepared with waypoints.
fn spaceship<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    match x.event {
        TRIGGER_DYING_ANIMATION_FINISHED => 1,
        TRIGGER_MOVE_PREPARATION_COMPLETE => i32::from(ask(h, EE_UNITS_COND_WAYPOINTS, x.unit, 0, 0) > 0),
        _ => base_of(h, x),
    }
}

/// The ships' (naval, naval transport, naval citizen and priest): dying done once sunk below the water; the others
/// `other`'s.
fn ship<H: CondHost + ?Sized>(h: &mut H, x: &Ctx, other: fn(&mut H, &Ctx) -> i32) -> i32 {
    if x.event != TRIGGER_DYING_ANIMATION_FINISHED {
        return other(h, x);
    }
    i32::from(ask(h, EE_UNITS_COND_SUNK, x.unit, 0, 0) != 0)
}

/// `SubmergedNavalEvaluateCondition`: dying done below the ground.
fn submerged_naval<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    if x.event != TRIGGER_DYING_ANIMATION_FINISHED {
        return base_of(h, x);
    }
    let mut io = EeUnitsCondIo::default();
    h.op(EE_UNITS_COND_POSITION, x.unit, 0, 0, 0, &mut io);
    let z = io.p[2];
    h.op(EE_UNITS_COND_GROUND_Z, x.unit, 0, 0, 0, &mut io);
    i32::from(z < io.f)
}

/// `BasicCitizenEvaluateCondition`.
fn citizen<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    let flag = match x.event {
        TRIGGER_BUILDING_IN_BUILD_QUEUE => {
            ask(h, EE_UNITS_COND_BUILD_QUEUE, x.unit, 0, 0) > 0 && ask(h, EE_UNITS_COND_BUILD_QUEUE, x.unit, 0, 1) != 0
        }
        TRIGGER_BUILD_QUEUE_EMPTY => ask(h, EE_UNITS_COND_BUILD_QUEUE, x.unit, 0, 0) == 0,
        // true while the citizen still has a goal (EEUnitClassPeople.cpp's note on the original's sense)
        TRIGGER_SMART_TASK_NOT_ASSIGNED => goal(h, x.unit, true).is_some(),
        TRIGGER_GOAL_IS_REPAIR_SITE => goal(h, x.unit, false).is_some_and(|g| g.kind == GOAL_BUILD_REPAIR),
        e if CITIZEN_GOAL_TRIGGERS.contains(&e) => return active_goal_answer(h, x.unit, e).unwrap_or(0),
        _ => return base_of(h, x),
    };
    i32::from(flag)
}

/// The active goal's answer: 1, or 2 when it is not the current goal; None when it has none or answers false.
fn active_goal_answer<H: CondHost + ?Sized>(h: &mut H, unit: u64, event: u32) -> Option<i32> {
    let active = ask(h, EE_UNITS_COND_GOAL, unit, 0, 0) as u64;
    if active == 0 || ask(h, EE_UNITS_COND_GOAL_CONDITION, unit, active, i64::from(event)) == 0 {
        return None;
    }
    let state = ask(h, EE_UNITS_COND_GOAL, unit, 0, 1) as u64;
    Some(if active != state { 2 } else { 1 })
}

/// `BasicPriestEvaluateCondition`: the conversion triggers.
fn priest<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    let u = x.unit;
    let flag = match x.event {
        TRIGGER_ATTACKER_IS_LEGAL_CONVERSION_TARGET => {
            let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
            attacker != 0 && ask(h, EE_UNITS_COND_CAN_CONVERT, u, attacker, 0) != 0
        }
        TRIGGER_HAS_ENOUGH_MANA_FOR_CONVERSION => {
            ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) != 0
                && rec(h, u).cx().mPower as i64 >= ask(h, EE_UNITS_COND_CONVERSION_COST, u, 0, 0)
        }
        TRIGGER_CONVERT_GOAL_STILL_VALID => {
            if goal(h, u, true).is_none() {
                return 0;
            }
            ask(h, EE_UNITS_COND_ATTACK_GOAL, u, 0, 0) & 2 != 0
        }
        TRIGGER_GOAL_IS_NOT_CONVERSION => match goal(h, u, true) {
            None => true,
            Some(g) if g.kind != GOAL_ATTACK_UNIT => true,
            Some(_) => {
                ask(h, EE_UNITS_COND_CRUSADER, u, 0, 0) != 0 && ask(h, EE_UNITS_COND_ATTACK_GOAL, u, 0, 0) & 4 == 0
            }
        },
        TRIGGER_GOAL_IS_CONVERSION => {
            if goal(h, u, true).is_none() {
                return 0;
            }
            let g = ask(h, EE_UNITS_COND_ATTACK_GOAL, u, 0, 0);
            g != 0 && (ask(h, EE_UNITS_COND_CRUSADER, u, 0, 0) == 0 || g & 4 != 0)
        }
        TRIGGER_ENEMY_UNIT_CONVERTED => {
            if goal(h, u, true).is_none() {
                return 0;
            }
            ask(h, EE_UNITS_COND_ATTACK_GOAL, u, 0, 0) & 8 != 0
        }
        _ => return base_of(h, x),
    };
    i32::from(flag)
}

fn member<H: CondHost + ?Sized>(h: &mut H, x: &Ctx, which: i64) -> bool {
    ask(h, EE_UNITS_COND_MEMBER, x.unit, x.class, which) != 0
}

/// `BuildingEvaluateCondition`.
fn building<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    let u = x.unit;
    let flag = match x.event {
        TRIGGER_DYING_ANIMATION_FINISHED => {
            let t = rec(h, u).class().mUnitType as u32;
            t == UNIT_TYPE_SPACE_TURRET
                || t == UNIT_TYPE_ORBITAL_SPACE_STATION
                || t == UNIT_TYPE_SPACE_DOCK
                || h.location(u)[2] < BUILDING_SUNK
        }
        TRIGGER_ENEMY_UNIT_SPOTTED => {
            let built = rec(h, u);
            if built.flag(&built.cx().mIsBuilt) {
                return base_of(h, x);
            }
            false
        }
        TRIGGER_AIRPLANE_AVAILABLE_TO_LAUNCH => member(h, x, MEMBER_SELECT_RELEASE),
        TRIGGER_CHOOSE_RANDOM_VOLCANO_TARGET => member(h, x, MEMBER_ERUPT_VOLCANO),
        TRIGGER_SECONDARY_TARGET_FOUND => ask(h, EE_UNITS_COND_SECONDARY_TARGET, u, 0, 0) != 0,
        TRIGGER_CALAMITY_TARGET_STILL_VALID => x.cls().mUnitType as u32 == UNIT_TYPE_CAPITOL,
        TRIGGER_NO_LAUNCH_REQUESTS => ask(h, EE_UNITS_COND_LAUNCH, u, 0, 0) != 0,
        TRIGGER_TAKEOFF_CLEARANCE_GRANTED => ask(h, EE_UNITS_COND_LAUNCH, u, 0, 1) != 0,
        TRIGGER_LAUNCHED_AIRPLANE => true,
        _ => return base_of(h, x),
    };
    i32::from(flag)
}

/// `BuildingPriestEvaluateCondition`: the building's answer, the conversion triggers the priest's (both asked).
fn building_priest<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    let answer = building(h, x);
    match x.event {
        TRIGGER_ENEMY_UNIT_CONVERTED
        | TRIGGER_GOAL_IS_CONVERSION
        | TRIGGER_CONVERT_GOAL_STILL_VALID
        | TRIGGER_HAS_ENOUGH_MANA_FOR_CONVERSION
        | TRIGGER_ATTACKER_IS_LEGAL_CONVERSION_TARGET => priest(h, x),
        _ => answer,
    }
}

/// `GateEvaluateCondition`.
fn gate<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    let r = rec(h, x.unit);
    let flag = match x.event {
        TRIGGER_IS_OPEN => r.cx().mOpenFraction == GATE_OPEN,
        TRIGGER_IS_CLOSED => r.cx().mOpenFraction == GATE_CLOSED,
        TRIGGER_LOCKED => r.flag(&r.cx().mIsLocked),
        TRIGGER_NOT_LOCKED => !r.flag(&r.cx().mIsLocked),
        TRIGGER_SPECIAL_UNIT_IN_OPENING_RANGE => member(h, x, MEMBER_ENEMIES_NEAR),
        TRIGGER_SPECIAL_UNIT_NOT_IN_OPENING_RANGE => !member(h, x, MEMBER_ENEMIES_NEAR),
        TRIGGER_UNIT_IN_OPENING_RANGE => member(h, x, MEMBER_PASSER_NEAR),
        TRIGGER_UNIT_NOT_IN_OPENING_RANGE => !member(h, x, MEMBER_PASSER_NEAR),
        _ => return building(h, x),
    };
    i32::from(flag)
}

/// `MiningBuildingEvaluateCondition`: the resource depleted when its mine is gone or empty.
fn mining_building<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    if x.event != TRIGGER_RESOURCE_DEPLETED {
        return building(h, x);
    }
    i32::from(ask(h, EE_UNITS_COND_MINE_DEPLETED, x.unit, 0, 0) != 0)
}

/// `NavalBuildingEvaluateCondition` (the space buildings' too).
fn naval_building<H: CondHost + ?Sized>(h: &mut H, x: &Ctx) -> i32 {
    let flag = match x.event {
        TRIGGER_FIGHTER_HAS_GOAL => ask(h, EE_UNITS_COND_QUEUED_GOAL, x.unit, 0, 1) != 0,
        TRIGGER_AIRPLANE_AVAILABLE_TO_LAUNCH => member(h, x, MEMBER_SELECT_RELEASE),
        TRIGGER_DYING_ANIMATION_FINISHED => member(h, x, MEMBER_HAS_SUNK),
        _ => return building(h, x),
    };
    i32::from(flag)
}

/// Whether the attacker's class can be hurt by the ammunition (`CanBeAttacked() && EEDamageFactor(family, ammo)`
/// is not 0).
fn can_damage<H: CondHost + ?Sized>(h: &mut H, class: u64, family: u32, ammo: i32) -> bool {
    ask(h, EE_UNITS_COND_CAN_BE_ATTACKED, 0, class, 0) != 0
        && ask_f(h, EE_UNITS_COND_DAMAGE_FACTOR, 0, 0, i64::from(family), i64::from(ammo)) != 0.0
}

fn behavior<H: CondHost + ?Sized>(h: &mut H, unit: u64, bit: i64) -> bool {
    ask(h, EE_UNITS_COND_BEHAVIOR, unit, 0, 0) & bit != 0
}

fn queued<H: CondHost + ?Sized>(h: &mut H, unit: u64) -> bool {
    ask(h, EE_UNITS_COND_LOCATION_QUEUED, unit, 0, 0) != 0
}

const CAN_TARGET: i64 = 1;
const CAN_FLEE: i64 = 2;
const CAN_PURSUE: i64 = 4;
const RETURNS: i64 = 8;

/// `EEUnitClass::BaseEvaluateCondition` (`class`, `cls`: the class it ran on): the active goal answers first (2
/// when it is not the current goal), then the unit's triggers.
pub fn base<H: CondHost + ?Sized>(
    h: &mut H,
    class: u64,
    cls: *const EeUnitsUnitClassFields,
    unit: u64,
    event: u32,
) -> i32 {
    // SAFETY: a live class's record, read in place
    let this = unsafe { &*cls };
    let u = unit;
    let g = goal(h, u, false);
    if let Some(g) = g {
        if ask(h, EE_UNITS_COND_GOAL_CONDITION, u, g.obj, i64::from(event)) != 0 {
            let state = ask(h, EE_UNITS_COND_GOAL, u, 0, 1) as u64;
            return if g.obj != state { 2 } else { 1 };
        }
    }
    let kind = g.map_or(0, |g| g.kind);
    let b1c = g.is_some_and(|g| g.b1c);
    let elapsed = |h: &mut H, ms: i64| g.is_some_and(|g| ask(h, EE_UNITS_COND_GOAL_ELAPSED, u, g.obj, ms) != 0);
    let flag = match event {
        TRIGGER_CAN_I_TARGET_ENEMIES => behavior(h, u, CAN_TARGET) || queued(h, u),
        TRIGGER_AM_I_UNDER_ATTACK => {
            ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) != 0 && ask(h, EE_UNITS_COND_CONDITION_TARGET, u, 0, 0) != 0
        }
        TRIGGER_ATTACK_MOVE_ENABLED => {
            kind == GOAL_LOCATION && can_attack_move(class_kinds(this)) && !g.is_some_and(|g| g.forced)
        }
        TRIGGER_AMMO_EXHAUSTED => {
            let r = rec(h, u);
            r.cx().mAmmunition == 0 && r.flag(&r.class().mUsesAmmunition)
        }
        TRIGGER_RELOADED => rec(h, u).cx().mReloadTimer == 0,
        TRIGGER_ATTACKER_IS_NOT_BUILDING => {
            let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
            attacker != 0 && rec(h, attacker).class().mFamily as u32 != FAMILY_BUILDING
        }
        TRIGGER_ATTACKER_IS_REACHABLE => {
            let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
            attacker != 0 && ask(h, EE_UNITS_COND_CAN_REACH, u, attacker, 0) != 0
        }
        TRIGGER_CAN_DAMAGE_ATTACKER => {
            let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
            if attacker == 0 {
                false
            } else {
                let a = rec(h, attacker);
                let ammo = rec(h, u).class().mAmmoType as i32;
                can_damage(h, a.0.class_obj, a.class().mFamily as u32, ammo)
            }
        }
        TRIGGER_CAN_FLEE => behavior(h, u, CAN_FLEE) && !queued(h, u),
        TRIGGER_CAN_I_MOVE_FREELY => {
            let mut flag = ask(h, EE_UNITS_COND_MOVE_FREELY, u, 0, 0) != 0 || queued(h, u);
            // port (EEUnitAI.cpp's note): a guard closes on the unit that hit it
            if !flag && behavior(h, u, RETURNS) && behavior(h, u, CAN_TARGET) && kind == GOAL_ATTACK_UNIT {
                let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
                if attacker != 0
                    && g.is_some_and(|g| ask(h, EE_UNITS_COND_GOAL_TARGET, u, g.obj, 0) as u64 == attacker)
                    && rec(h, attacker).hit_points() > 0
                {
                    flag = true;
                }
            }
            flag
        }
        TRIGGER_CAN_I_PURSUE_PAST_INITIAL_CONTACT_LOS => behavior(h, u, CAN_PURSUE) || queued(h, u),
        TRIGGER_ENEMY_INSIDE_INITIAL_CONTACT_LOS => {
            if kind != GOAL_ATTACK_UNIT && kind != GOAL_ATTACK_GROUND {
                false
            } else {
                let mut io = EeUnitsCondIo::default();
                h.op(EE_UNITS_COND_DROP_LOCATION, u, 0, 0, 0, &mut io);
                let location = Ge3DPoint { x: io.p[0], y: io.p[1], z: 0.0 };
                let range = ask_f(h, EE_UNITS_COND_LINE_OF_SIGHT, u, 0, 0, 0);
                h.op(EE_UNITS_COND_INITIAL_CONTACT, u, 0, 0, 0, &mut io);
                let contact = Ge3DPoint { x: io.p[0], y: io.p[1], z: io.p[2] };
                contact.distance_to_squared(location) <= range * range
            }
        }
        TRIGGER_ENEMY_WITHIN_MINIMUM_RANGE => {
            if kind != GOAL_ATTACK_UNIT && kind != GOAL_ATTACK_GROUND {
                false
            } else {
                let mut io = EeUnitsCondIo::default();
                let target = h.op(EE_UNITS_COND_DROP_LOCATION, u, 0, 0, 0, &mut io) as u64;
                let range = this.mMinRange;
                let distance = if kind == GOAL_ATTACK_UNIT {
                    ask_f(h, EE_UNITS_COND_HIT_DISTANCE, u, target, 0, 0)
                } else {
                    h.op(EE_UNITS_COND_DISTANCE_3D, u, 0, 0, 0, &mut io);
                    io.f
                };
                distance < range * range
            }
        }
        TRIGGER_HAS_VALID_INITIAL_CONTACT_LOCATION => rec(h, u).cx().mInitialContactHeading != NO_HEADING,
        TRIGGER_IS_ATTACKER_IN_RETALIATION_RANGE => {
            let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
            attacker != 0 && {
                let range = ask_f(h, EE_UNITS_COND_RESPONSE_RANGE, u, 0, 0, 0);
                ask_f(h, EE_UNITS_COND_DISTANCE_TO, u, attacker, 0, 0) < range
            }
        }
        TRIGGER_RETURNS_TO_INITIAL_CONTACT_LOCATION => behavior(h, u, RETURNS) && !queued(h, u),
        TRIGGER_UNIT_IDLE_OR_ATTACKING_DEFENSELESS_BUILDING => idle_or_defenseless(h, u, g),
        TRIGGER_ATTACKER_IN_WEAPON_RANGE => {
            let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
            attacker != 0 && ask(h, EE_UNITS_COND_IN_REACH, u, attacker, 0) != 0 && {
                let r = rec(h, u);
                r.flag(&r.cx().mIsBuilt)
            }
        }
        TRIGGER_GOAL_IS_LOCATION | TRIGGER_SAFE_LOCATION_FOUND => g.is_some() && kind == GOAL_LOCATION,
        TRIGGER_TAKING_LIGHT_DAMAGE => {
            let r = rec(h, u);
            r.cx().mLastHitPoints as i32 > r.hit_points()
        }
        TRIGGER_GOAL_IS_UNIT => g.is_some() && kind == GOAL_ATTACK_UNIT,
        TRIGGER_ENEMY_UNIT_IN_LOS => ask(h, EE_UNITS_COND_ENEMY_IN_RANGE, u, 0, 0) != 0,
        TRIGGER_OWNER_IS_COMPUTER => ask(h, EE_UNITS_COND_OWNER, u, 0, 0) & 1 != 0,
        TRIGGER_OWNER_IS_HUMAN => ask(h, EE_UNITS_COND_OWNER, u, 0, 0) & 2 != 0,
        TRIGGER_GOAL_IS_PLAYER_INITIATED => b1c,
        TRIGGER_ONE_SECOND_ELAPSED => elapsed(h, 1000),
        TRIGGER_GOAL_IS_ATTACK_LOCATION => g.is_some() && kind == GOAL_ATTACK_GROUND,
        TRIGGER_UNIT_HAS_GOAL => g.is_some(),
        TRIGGER_GOAL_IS_CALAMITY => g.is_some() && kind == GOAL_CAST_CALAMITY,
        TRIGGER_GOAL_IS_EXPLORE => g.is_some() && kind == GOAL_EXPLORE,
        TRIGGER_TWO_SECONDS_ELAPSED => elapsed(h, 2000),
        TRIGGER_LOCATION_OFF_MAP => ask(h, EE_UNITS_COND_OFF_MAP, u, 0, 0) != 0,
        TRIGGER_LOCATION_IN_SPACE => ask(h, EE_UNITS_COND_OFF_MAP, u, 0, 1) != 0,
        TRIGGER_GOAL_IS_NOT_CALAMITY => g.is_none() || kind != GOAL_CAST_CALAMITY,
        TRIGGER_LOCATION_WAY_OFF_MAP => ask(h, EE_UNITS_COND_OFF_MAP, u, 0, 2) != 0,
        TRIGGER_ONE_AND_A_HALF_SECONDS_ELAPSED => elapsed(h, 1500),
        TRIGGER_THREE_SECONDS_ELAPSED => elapsed(h, 3000),
        TRIGGER_CARGO_IS_EMPTY => ask(h, EE_UNITS_COND_CARGO, u, 0, 0) == 0,
        TRIGGER_UNIT_HIT_SPACE => {
            let mut io = EeUnitsCondIo::default();
            h.op(EE_UNITS_COND_POSITION, u, 0, 0, 0, &mut io);
            let z = io.p[2];
            h.op(EE_UNITS_COND_TILE, u, 0, 0, 0, &mut io) as i32 == TERRAIN_SPACE && z <= 0.0
        }
        TRIGGER_REACHED_BOTTOM_OF_SPACE => {
            let mut io = EeUnitsCondIo::default();
            h.op(EE_UNITS_COND_POSITION, u, 0, 0, 0, &mut io);
            io.p[2] <= DIVE_DEPTH
        }
        TRIGGER_DYING_ANIMATION_FINISHED => {
            let r = rec(h, u);
            r.flag(&r.cx().mIsAnimationPaused) || ask(h, EE_UNITS_COND_FRAME_COUNT, u, 0, 0) == 1
        }
        TRIGGER_CAN_I_MOVE => ask_f(h, EE_UNITS_COND_MAX_VELOCITY, u, 0, 0, 0) > 0.1,
        TRIGGER_MOVE_PREPARATION_COMPLETE => {
            let r = rec(h, u);
            r.flag(&r.cx().mIsPathDone) && ask(h, EE_UNITS_COND_WAYPOINTS, u, 0, 0) > 0
        }
        TRIGGER_UNIT_ROTTED_AWAY => rec(h, u).cx().mRottingAwayTime == 0,
        TRIGGER_RESOURCE_DEPLETED | TRIGGER_RESOURCES_ROTTED_AWAY => rec(h, u).resource_amount() == 0,
        TRIGGER_OBSTACLE_DETECTED => ask(h, EE_UNITS_COND_OBSTACLE, u, 0, 0) != 0,
        TRIGGER_SHOULD_MOVE_TO_NEXT_AUTO_WAYPOINT => g.is_some() && ask(h, EE_UNITS_COND_AUTO_WAYPOINT, u, 0, 0) != 0,
        TRIGGER_GOAL_IS_PATROL => g.is_some() && kind == GOAL_PATROL,
        TRIGGER_NO_MORE_WAYPOINTS => ask(h, EE_UNITS_COND_WAYPOINT_STEP, u, 0, 0) != 0,
        TRIGGER_NEXT_WAYPOINT_RETRIEVED => ask(h, EE_UNITS_COND_WAYPOINT_STEP, u, 0, 1) != 0,
        TRIGGER_NEXT_PATROL_POINT_RETRIEVED => ask(h, EE_UNITS_COND_WAYPOINTS, u, 0, 0) > 0,
        TRIGGER_ARRIVED_AT_MOVE_WAYPOINT => ask(h, EE_UNITS_COND_WAYPOINT_STEP, u, 0, 2) != 0,
        TRIGGER_ONE_WAYPOINT_REMAINING => ask(h, EE_UNITS_COND_WAYPOINT_STEP, u, 0, 3) != 0,
        TRIGGER_IS_ADULT => rec(h, u).cx().mGrowth >= 1.0,
        TRIGGER_IS_BABY => rec(h, u).cx().mGrowth < 1.0,
        TRIGGER_HEAL_SEARCH_TIME_ELAPSED => this.mHealDelay != 0.0 && rec(h, u).cx().mHealSearchTimer == 0.0,
        TRIGGER_GOAL_IS_NOT_ATTACK => kind != GOAL_ATTACK_UNIT && kind != GOAL_ATTACK_GROUND,
        TRIGGER_HAS_GROUP_ATTACK => ask(h, EE_UNITS_COND_HERD, u, 0, 0) & 2 != 0,
        TRIGGER_CAN_I_SEE_ENEMY => {
            if !behavior(h, u, CAN_TARGET) && !queued(h, u) {
                false
            } else {
                attack_nearby(h, class, u, g)
            }
        }
        TRIGGER_ENEMY_UNIT_SPOTTED => attack_nearby(h, class, u, g),
        TRIGGER_SHOULD_I_RETALIATE => should_retaliate(h, u, g),
        TRIGGER_SHOULD_I_RUN => {
            behavior(h, u, CAN_FLEE)
                && !queued(h, u)
                && ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) != 0
                && ask(h, EE_UNITS_COND_CONDITION_TARGET, u, 0, 0) != 0
                && !b1c
        }
        TRIGGER_GOAL_IS_NOT_PLAYER_INITIATED => !b1c,
        TRIGGER_HAS_PARENTAL_VENGANCE => ask(h, EE_UNITS_COND_HERD, u, 0, 0) & 4 != 0 && rec(h, u).class().mAttack > 0,
        TRIGGER_HAS_GROUP_FLEE => ask(h, EE_UNITS_COND_HERD, u, 0, 0) & 8 != 0,
        TRIGGER_ALWAYS_TRUE => true,
        _ => false,
    };
    i32::from(flag)
}

fn attack_nearby<H: CondHost + ?Sized>(h: &mut H, class: u64, unit: u64, g: Option<Goal>) -> bool {
    ask(h, EE_UNITS_COND_ATTACK_NEARBY, unit, class, g.map_or(0, |g| g.obj as i64)) != 0
}

/// `UnitIdleOrAttackingDefenselessBuilding`: idle, or attacking a building that cannot hurt it; (a port) attacking a
/// unit out of its reach while another hits it.
fn idle_or_defenseless<H: CondHost + ?Sized>(h: &mut H, u: u64, g: Option<Goal>) -> bool {
    let Some(g) = g else { return true };
    if g.kind != GOAL_ATTACK_UNIT {
        return false;
    }
    let target = ask(h, EE_UNITS_COND_GOAL_TARGET, u, g.obj, 0) as u64;
    let t = rec(h, target);
    if t.0.land_building == 0 {
        let attacker = ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) as u64;
        return attacker != 0 && attacker != target && ask(h, EE_UNITS_COND_IN_REACH, u, target, 0) == 0;
    }
    if t.0.stats_attack != 0.0 {
        let own = rec(h, u);
        let ammo = t.class().mAmmoType as i32;
        let family = own.class().mFamily as u32;
        if ask(h, EE_UNITS_COND_CAN_BE_ATTACKED, 0, own.0.class_obj, 0) != 0 {
            return ask_f(h, EE_UNITS_COND_DAMAGE_FACTOR, 0, 0, i64::from(family), i64::from(ammo)) == 0.0;
        }
    }
    true
}

/// `ShouldIRetaliate`.
fn should_retaliate<H: CondHost + ?Sized>(h: &mut H, u: u64, g: Option<Goal>) -> bool {
    if ask(h, EE_UNITS_COND_LAST_ATTACKER, u, 0, 0) == 0 {
        return false;
    }
    let attacker = ask(h, EE_UNITS_COND_CONDITION_TARGET, u, 0, 0) as u64;
    if attacker == 0 || (!behavior(h, u, CAN_TARGET) && !queued(h, u)) || g.is_some_and(|g| g.b1c) {
        return false;
    }
    let a = rec(h, attacker);
    let ammo = rec(h, u).class().mAmmoType as i32;
    if !can_damage(h, a.0.class_obj, a.class().mFamily as u32, ammo) {
        return false;
    }
    if let Some(g) = g {
        if g.kind != GOAL_ATTACK_UNIT {
            return false;
        }
        let target = ask(h, EE_UNITS_COND_GOAL_TARGET, u, g.obj, 0) as u64;
        let t = rec(h, target);
        if t.0.land_building == 0 {
            return false;
        }
        if t.0.stats_attack != 0.0 {
            let own = rec(h, u);
            let ammo = t.class().mAmmoType as i32;
            if can_damage(h, own.0.class_obj, own.class().mFamily as u32, ammo) {
                return false;
            }
        }
    }
    let range = ask_f(h, EE_UNITS_COND_RESPONSE_RANGE, u, 0, 0, 0);
    ask_f(h, EE_UNITS_COND_DISTANCE_TO, u, attacker, 0, 0) < range
        && ask(h, EE_UNITS_COND_CAN_REACH, u, attacker, 0) != 0
}

#[cfg(test)]
#[path = "cls_condition_tests.rs"]
mod tests;
