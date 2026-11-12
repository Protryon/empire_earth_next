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
