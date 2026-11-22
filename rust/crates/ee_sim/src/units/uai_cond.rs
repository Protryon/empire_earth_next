//! The trigger host's compound answers over their C++ statements (the units' lane, round 8; were EEUnitAI.cpp
//! `ee_units_cls_cond_host`'s multi-statement cases and `EEUnitClass::AttackNearby`): the behaviour flags, the drop
//! location, the response range, the owner, the off-map tests, the obstacle wait, the herd, `AttackNearby`, the ground
//! height, the build queue, the attack goal's flags, the class members and the launch. Every C++ statement of the
//! originals is one `EE_UNITS_UAI_*` case of `ee_units_cls_cond_host` (EEUnitAI.cpp), asked in the original's order;
//! `a` is the unit, `b` another object, `c`, `d` values, `io` the floats.
//!
//! Determinism: the obstacle wait's `UMFloatToLong` is `lrintf` over the original's `float` products; the random
//! draw is the host's one `GetRandom(0.75f, 1.25f)`.

use crate::units::cls_condition::*;
use ee_base::math::um::{floor, floor_unsigned, lrintf};

// ---- the statements (`op`; the answer), from 100
/// goal `b`'s `mKind`
pub const EE_UNITS_UAI_GOAL_KIND: i32 = 100;
/// goal `b`'s `GetIsForcedMove()`
pub const EE_UNITS_UAI_GOAL_FORCED_MOVE: i32 = 101;
/// goal `b`'s `mB1C`
pub const EE_UNITS_UAI_GOAL_B1C: i32 = 102;
/// `mTargetSearchTimer`
pub const EE_UNITS_UAI_SEARCH_TIMER: i32 = 103;
/// `io.f = mInitialContactHeading`
pub const EE_UNITS_UAI_CONTACT_HEADING: i32 = 104;
/// `mBehavior->mReturnsToInitialContactLocation`
pub const EE_UNITS_UAI_RETURNS_TO_CONTACT: i32 = 105;
/// `io.p = mInitialContactLocation`
pub const EE_UNITS_UAI_CONTACT_LOCATION: i32 = 106;
/// `EEFindTargetNear(unit, c, d, io.f)`
pub const EE_UNITS_UAI_FIND_TARGET_NEAR: i32 = 107;
/// `EEFindNearbyTarget(unit)`
pub const EE_UNITS_UAI_FIND_NEARBY_TARGET: i32 = 108;
/// `mGoalState`
pub const EE_UNITS_UAI_GOAL_STATE: i32 = 109;
/// unit `b`'s `mUnitClass->IsLandBuilding()`
pub const EE_UNITS_UAI_IS_LAND_BUILDING: i32 = 110;
/// `io.f = ` unit `b`'s `mStats->mAttack`
pub const EE_UNITS_UAI_ATTACK_OF: i32 = 111;
/// `mUnitClass->mCategory`
pub const EE_UNITS_UAI_CATEGORY: i32 = 112;
/// `EEUnitCast<EEComplexUnit>(unit b)`
pub const EE_UNITS_UAI_AS_COMPLEX: i32 = 113;
/// `EECanConvert(unit, complex unit b, the flag)`
pub const EE_UNITS_UAI_CAN_CONVERT: i32 = 114;
/// the flag the last `EE_UNITS_UAI_CAN_CONVERT` (or `EE_UNITS_COND_CAN_CONVERT`) wrote
pub const EE_UNITS_UAI_CONVERT_FLAG: i32 = 115;
/// `EEUGRust::NewAttackUnit(unit, unit b, false, false)`
pub const EE_UNITS_UAI_NEW_ATTACK: i32 = 116;
/// `SetGoal(goal b, c != 0, 0)`
pub const EE_UNITS_UAI_SET_GOAL: i32 = 117;
/// `mPlayer->NoticeForeignUnit(unit b)`
pub const EE_UNITS_UAI_NOTICE_FOREIGN: i32 = 118;
/// `SetTargetSearchTimer(mUnitClass->mTargetSearchInterval)`
pub const EE_UNITS_UAI_RESTART_SEARCH: i32 = 119;
/// `mBehavior->mCanTargetEnemies`
pub const EE_UNITS_UAI_CAN_TARGET_ENEMIES: i32 = 120;
/// `mBehavior->mCanFlee`
pub const EE_UNITS_UAI_CAN_FLEE: i32 = 121;
/// `mBehavior->mCanPursuePastInitialContactLOS`
pub const EE_UNITS_UAI_CAN_PURSUE: i32 = 122;
/// `EECombatFindDropLocation(unit, NULL, the point)`
pub const EE_UNITS_UAI_FIND_DROP: i32 = 123;
/// `io.p = ` the point
pub const EE_UNITS_UAI_POINT: i32 = 124;
/// the point `= GE3DPoint(0, 0, 0)`
pub const EE_UNITS_UAI_CLEAR_POINT: i32 = 125;
/// `mPlayer->mComputerPlayer != NULL`
pub const EE_UNITS_UAI_HAS_COMPUTER: i32 = 126;
/// `io.f = mPlayer->mComputerPlayer->mMaxAttackResponseRange`
pub const EE_UNITS_UAI_CP_RESPONSE_RANGE: i32 = 127;
/// `io.f = gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue`
pub const EE_UNITS_UAI_DEFAULT_RESPONSE_RANGE: i32 = 128;
/// `mPlayer->mHumanPlayer != NULL`
pub const EE_UNITS_UAI_HAS_HUMAN: i32 = 129;
/// `gWorld.mMap->mTerrainMesh.GetIsValidTile(c, d)` (as `unsigned long`s)
pub const EE_UNITS_UAI_VALID_TILE: i32 = 131;
/// the map tile (c, d) exists and is space
pub const EE_UNITS_UAI_SPACE_TILE: i32 = 132;
/// the location's distance to the tile's centre at `mHeight` beyond `kWorldTileStrayDistance`
pub const EE_UNITS_UAI_STRAY: i32 = 133;
/// `mBlockingUnit != NULL`
pub const EE_UNITS_UAI_HAS_BLOCKING: i32 = 134;
/// `io.f = gPhysics->GetRandom(io.p[0], io.p[1])`
pub const EE_UNITS_UAI_RANDOM: i32 = 135;
/// `gUnitAIWaitScale`
pub const EE_UNITS_UAI_WAIT_SCALE: i32 = 136;
/// `mActiveGoal`
pub const EE_UNITS_UAI_ACTIVE_GOAL: i32 = 137;
/// `mUnitClass->mIsMelee`
pub const EE_UNITS_UAI_IS_MELEE: i32 = 138;
/// `io.f = mVelocity`
pub const EE_UNITS_UAI_VELOCITY: i32 = 139;
/// `io.f = GetCollisionResolutionCount()`
pub const EE_UNITS_UAI_COLLISION_COUNT: i32 = 140;
/// `mCollisionResolutionCount = 0.0f`
pub const EE_UNITS_UAI_CLEAR_COLLISION_COUNT: i32 = 141;
/// `mHerd`
pub const EE_UNITS_UAI_HERD: i32 = 142;
/// herd `b`'s `GetAnimal()->mHasGroupAttack`
pub const EE_UNITS_UAI_HERD_GROUP_ATTACK: i32 = 143;
/// herd `b`'s `GetAnimal()->mHasParentalVengeance`
pub const EE_UNITS_UAI_HERD_PARENTAL: i32 = 144;
/// herd `b`'s `GetAnimal()->mHasGroupFlee`
pub const EE_UNITS_UAI_HERD_GROUP_FLEE: i32 = 145;
/// `gWorld.mMap->GetGroundPoint(io.p, the point)`
pub const EE_UNITS_UAI_GROUND_POINT: i32 = 146;
/// `io.f = ` the point's z
pub const EE_UNITS_UAI_POINT_Z: i32 = 147;
/// `mCitizenBuildQueue.size()`
pub const EE_UNITS_UAI_CITIZEN_QUEUE_SIZE: i32 = 148;
/// `PopBuildTarget()`
pub const EE_UNITS_UAI_POP_BUILD_TARGET: i32 = 149;
/// `EEUGRust::NewBuildRepair(GetPlayer(), unit b, true)`
pub const EE_UNITS_UAI_NEW_BUILD_REPAIR: i32 = 150;
/// `StartAction(c)`
pub const EE_UNITS_UAI_START_ACTION: i32 = 151;
/// `EEAttackUnitCast(GetGoal())`
pub const EE_UNITS_UAI_ATTACK_GOAL: i32 = 152;
/// goal `b`'s `GetAttackTarget()`
pub const EE_UNITS_UAI_ATTACK_TARGET: i32 = 153;
/// unit `b`'s `mTileNode != NULL`
pub const EE_UNITS_UAI_ON_MAP: i32 = 154;
/// unit `b`'s `GetComplexUnit()->mIsDisabled`
pub const EE_UNITS_UAI_DISABLED: i32 = 155;
/// unit `b`'s `GetComplexUnit()->IsProtected()`
pub const EE_UNITS_UAI_PROTECTED: i32 = 156;
/// unit `b`'s `GetPlayer() == GetPlayer()`
pub const EE_UNITS_UAI_SAME_PLAYER: i32 = 157;
/// `GetPlayer()->mEnemies[` unit `b`'s `GetPlayer()->GetPlayerID()]`
pub const EE_UNITS_UAI_STANCE: i32 = 158;
/// goal `b`'s `IsConvertAttack()`
pub const EE_UNITS_UAI_CONVERT_ATTACK: i32 = 159;
/// class `b`'s `SelectUnitToRelease(unit)`
pub const EE_UNITS_UAI_SELECT_TO_RELEASE: i32 = 160;
/// class `b`'s `EruptVolcano(unit)`
pub const EE_UNITS_UAI_ERUPT_VOLCANO: i32 = 161;
/// class `b`'s `HasSunk(unit)`
pub const EE_UNITS_UAI_HAS_SUNK: i32 = 162;
/// class `b`'s `AreEnemiesNear(unit)`
pub const EE_UNITS_UAI_ENEMIES_NEAR: i32 = 163;
/// class `b`'s `IsPasserNear(unit)`
pub const EE_UNITS_UAI_PASSER_NEAR: i32 = 164;
/// `ee_units_atc_queue_length(` the unit's air traffic `, unit)`
pub const EE_UNITS_UAI_TRAFFIC_QUEUE_LENGTH: i32 = 165;
/// `mExitingUnitID`
pub const EE_UNITS_UAI_EXITING_ID: i32 = 166;
/// `FindPassenger(c)`
pub const EE_UNITS_UAI_FIND_PASSENGER: i32 = 167;
/// complex unit `b`'s `mIsClearanceGranted`
pub const EE_UNITS_UAI_CLEARANCE: i32 = 168;
/// `SetExitingUnitID(c)`
pub const EE_UNITS_UAI_SET_EXITING: i32 = 169;
/// `gWorld.GetUnit(c)`
pub const EE_UNITS_UAI_WORLD_UNIT: i32 = 170;
/// unit `b`'s `mComplexUnit`
pub const EE_UNITS_UAI_COMPLEX_OF: i32 = 171;
/// `ee_units_atc_remove_plane(` the unit's air traffic `, unit, complex unit b)`
pub const EE_UNITS_UAI_REMOVE_PLANE: i32 = 172;

/// `kUnitAINoHeading`
const NO_HEADING: f32 = -10000.0;
/// `kUnitAIShortWaitFactor`, `kUnitAILongWaitFactor`
const SHORT_WAIT: f32 = 0.5;
const LONG_WAIT: f32 = 1.5;
const GOAL_LOCATION: i64 = 1;
const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_BUILD_REPAIR: i64 = 4;
const GOAL_PATROL: i64 = 9;
const GOAL_EXPLORE: i64 = 10;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
const CATEGORY_BUILDING_PRIEST: i64 = 0x1e;
const ACTION_MOVE_TO_REPAIR_SITE: i64 = 8017;

/// The C++ statements and the unit's location.
pub trait UaiHost {
    /// one statement (`EE_UNITS_UAI_*`) of unit `a`
    fn uai(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, io: &mut EeUnitsCondIo) -> i64;
    /// unit `a`'s location (`mPhysicalModel.GetLocation()`'s x, y, z)
    fn uai_location(&mut self, unit: u64) -> [f32; 3];
}

struct S<'h, H: UaiHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
    io: EeUnitsCondIo,
}

impl<H: UaiHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.uai(op, self.unit, 0, 0, 0, &mut self.io)
    }
    fn qb(&mut self, op: i32, b: u64) -> i64 {
        self.h.uai(op, self.unit, b, 0, 0, &mut self.io)
    }
    fn qc(&mut self, op: i32, c: i64) -> i64 {
        self.h.uai(op, self.unit, 0, c, 0, &mut self.io)
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
    fn yes_b(&mut self, op: i32, b: u64) -> bool {
        self.qb(op, b) != 0
    }
    fn f(&mut self, op: i32) -> f32 {
        self.q(op);
        self.io.f
    }
    fn f_b(&mut self, op: i32, b: u64) -> f32 {
        self.qb(op, b);
        self.io.f
    }
}

/// `EEUnitClass::AttackNearby(unit, goal)`: when the search timer ran out or the unit attack-moves, a target near
/// the initial contact or seen, an attack goal on it (ahead of a move, patrol or explore goal); the search timer
/// restarted. Returns whether it gave the goal.
pub fn attack_nearby<H: UaiHost + ?Sized>(h: &mut H, unit: u64, goal: u64) -> bool {
    let s = &mut S { h, unit, io: EeUnitsCondIo::default() };
    let kind = if goal != 0 { s.qb(EE_UNITS_UAI_GOAL_KIND, goal) } else { 0 };
    let mut result = false;
    let mut idle = goal != 0;
    if idle {
        idle = kind == GOAL_LOCATION;
        if idle {
            idle = !s.yes_b(EE_UNITS_UAI_GOAL_FORCED_MOVE, goal);
        }
    }
    if s.q(EE_UNITS_UAI_SEARCH_TIMER) <= 0 || idle {
        let target = if s.f(EE_UNITS_UAI_CONTACT_HEADING) != NO_HEADING
            && s.yes(EE_UNITS_UAI_RETURNS_TO_CONTACT)
            && (goal == 0 || !s.yes_b(EE_UNITS_UAI_GOAL_B1C, goal))
        {
            s.q(EE_UNITS_UAI_CONTACT_LOCATION);
            let (x, y) = (floor(s.io.p[0]), floor(s.io.p[1]));
            s.io.f = 1.0;
            s.h.uai(EE_UNITS_UAI_FIND_TARGET_NEAR, unit, 0, i64::from(x), i64::from(y), &mut s.io) as u64
        } else {
            s.q(EE_UNITS_UAI_FIND_NEARBY_TARGET) as u64
        };
        if target != 0 {
            let state = s.q(EE_UNITS_UAI_GOAL_STATE) as u64;
            if state == 0
                || s.qb(EE_UNITS_UAI_GOAL_KIND, state) != GOAL_ATTACK_UNIT
                || s.yes_b(EE_UNITS_UAI_GOAL_B1C, state)
                || s.qb(EE_UNITS_UAI_IS_LAND_BUILDING, target) != 1
                || s.f_b(EE_UNITS_UAI_ATTACK_OF, target) != 0.0
            {
                let mut attack = true;
                if s.q(EE_UNITS_UAI_CATEGORY) == CATEGORY_BUILDING_PRIEST {
                    let other = s.qb(EE_UNITS_UAI_AS_COMPLEX, target) as u64;
                    if other != 0 && (!s.yes_b(EE_UNITS_UAI_CAN_CONVERT, other) || s.yes(EE_UNITS_UAI_CONVERT_FLAG)) {
                        attack = false;
                    }
                }
                if attack {
                    let new_goal = s.qb(EE_UNITS_UAI_NEW_ATTACK, target) as u64;
                    let keep = goal != 0 && matches!(kind, GOAL_LOCATION | GOAL_PATROL | GOAL_EXPLORE);
                    s.h.uai(EE_UNITS_UAI_SET_GOAL, unit, new_goal, i64::from(keep), 0, &mut s.io);
                    result = true;
                    s.qb(EE_UNITS_UAI_NOTICE_FOREIGN, target);
                }
            }
        }
        s.q(EE_UNITS_UAI_RESTART_SEARCH);
    }
    result
}

/// `EE_UNITS_COND_OBSTACLE`: blocked, or the collision count past a random wait (shorter for a melee attacker,
/// longer for a citizen going to build); the count cleared when it is.
fn obstacle<H: UaiHost + ?Sized>(s: &mut S<'_, H>) -> i64 {
    if s.yes(EE_UNITS_UAI_HAS_BLOCKING) {
        return 1;
    }
    let goal = s.q(EE_UNITS_UAI_ACTIVE_GOAL) as u64;
    s.io.p[0] = 0.75;
    s.io.p[1] = 1.25;
    let random = s.f(EE_UNITS_UAI_RANDOM);
    let scale = s.q(EE_UNITS_UAI_WAIT_SCALE) as i32;
    let mut wait = lrintf(random * scale as f32) as u32;
    if s.yes(EE_UNITS_UAI_IS_MELEE)
        && s.f(EE_UNITS_UAI_VELOCITY) > 0.0
        && goal != 0
        && s.qb(EE_UNITS_UAI_GOAL_KIND, goal) == GOAL_ATTACK_UNIT
    {
        wait = lrintf(wait as f32 * SHORT_WAIT) as u32;
    } else if matches!(s.q(EE_UNITS_UAI_CATEGORY), CATEGORY_LAND_CITIZEN | CATEGORY_NAVAL_CITIZEN)
        && s.f(EE_UNITS_UAI_VELOCITY) > 0.0
        && goal != 0
        && s.qb(EE_UNITS_UAI_GOAL_KIND, goal) == GOAL_BUILD_REPAIR
    {
        wait = lrintf(wait as f32 * LONG_WAIT) as u32;
    }
    if s.f(EE_UNITS_UAI_COLLISION_COUNT) <= wait as f32 {
        return 0;
    }
    s.q(EE_UNITS_UAI_CLEAR_COLLISION_COUNT);
    1
}

/// `EE_UNITS_COND_LAUNCH`: `c` 0, the plane may go (the queue empty or no exiting unit); else whether the exiting
/// unit has clearance (a vanished one is dropped from the queue)
fn launch<H: UaiHost + ?Sized>(s: &mut S<'_, H>, c: i64) -> i64 {
    if c == 0 {
        return i64::from(s.q(EE_UNITS_UAI_TRAFFIC_QUEUE_LENGTH) == 0 || s.q(EE_UNITS_UAI_EXITING_ID) == -1);
    }
    let id = s.q(EE_UNITS_UAI_EXITING_ID);
    if id == -1 {
        return 0;
    }
    let cargo = s.qc(EE_UNITS_UAI_FIND_PASSENGER, id) as u64;
    if cargo != 0 {
        return s.qb(EE_UNITS_UAI_CLEARANCE, cargo);
    }
    s.qc(EE_UNITS_UAI_SET_EXITING, -1);
    let other = s.qc(EE_UNITS_UAI_WORLD_UNIT, id) as u64;
    if other != 0 {
        let complex = s.qb(EE_UNITS_UAI_COMPLEX_OF, other) as u64;
        if complex != 0 {
            s.qb(EE_UNITS_UAI_REMOVE_PLANE, complex);
        }
    }
    0
}

/// The compound answers: `Some` for the operations this module answers (`EE_UNITS_COND_*`), None for the others.
pub fn compound<H: UaiHost + ?Sized>(
    h: &mut H,
    op: i32,
    a: u64,
    b: u64,
    c: i64,
    io: &mut EeUnitsCondIo,
) -> Option<i64> {
    let s = &mut S { h, unit: a, io: *io };
    let v = match op {
        EE_UNITS_COND_BEHAVIOR => {
            i64::from(s.yes(EE_UNITS_UAI_CAN_TARGET_ENEMIES))
                | i64::from(s.yes(EE_UNITS_UAI_CAN_FLEE)) << 1
                | i64::from(s.yes(EE_UNITS_UAI_CAN_PURSUE)) << 2
                | i64::from(s.yes(EE_UNITS_UAI_RETURNS_TO_CONTACT)) << 3
        }
        EE_UNITS_COND_DROP_LOCATION => {
            s.q(EE_UNITS_UAI_CLEAR_POINT);
            let target = s.q(EE_UNITS_UAI_FIND_DROP);
            s.q(EE_UNITS_UAI_POINT);
            target
        }
        EE_UNITS_COND_RESPONSE_RANGE => {
            if s.yes(EE_UNITS_UAI_HAS_COMPUTER) {
                s.q(EE_UNITS_UAI_CP_RESPONSE_RANGE);
            } else {
                s.q(EE_UNITS_UAI_DEFAULT_RESPONSE_RANGE);
            }
            0
        }
        EE_UNITS_COND_OWNER => {
            i64::from(s.yes(EE_UNITS_UAI_HAS_COMPUTER)) | i64::from(s.yes(EE_UNITS_UAI_HAS_HUMAN)) << 1
        }
        EE_UNITS_COND_OFF_MAP => {
            if c == 0 || c == 1 {
                let l = s.h.uai_location(a);
                let (x, y) = (i64::from(floor_unsigned(l[0])), i64::from(floor_unsigned(l[1])));
                if c == 0 {
                    i64::from(s.h.uai(EE_UNITS_UAI_VALID_TILE, a, 0, x, y, &mut s.io) == 0)
                } else {
                    s.h.uai(EE_UNITS_UAI_SPACE_TILE, a, 0, x, y, &mut s.io)
                }
            } else {
                s.q(EE_UNITS_UAI_STRAY)
            }
        }
        EE_UNITS_COND_OBSTACLE => obstacle(s),
        EE_UNITS_COND_HERD => {
            let herd = s.q(EE_UNITS_UAI_HERD) as u64;
            if herd == 0 {
                0
            } else {
                1 | i64::from(s.yes_b(EE_UNITS_UAI_HERD_GROUP_ATTACK, herd)) << 1
                    | i64::from(s.yes_b(EE_UNITS_UAI_HERD_PARENTAL, herd)) << 2
                    | i64::from(s.yes_b(EE_UNITS_UAI_HERD_GROUP_FLEE, herd)) << 3
            }
        }
        EE_UNITS_COND_ATTACK_NEARBY => i64::from(attack_nearby(s.h, a, c as u64)),
        EE_UNITS_COND_GROUND_Z => {
            s.q(EE_UNITS_UAI_CLEAR_POINT);
            s.q(EE_UNITS_UAI_GROUND_POINT);
            s.q(EE_UNITS_UAI_POINT_Z);
            0
        }
        EE_UNITS_COND_BUILD_QUEUE => {
            if c == 0 {
                s.q(EE_UNITS_UAI_CITIZEN_QUEUE_SIZE)
            } else {
                let site = s.q(EE_UNITS_UAI_POP_BUILD_TARGET) as u64;
                if site == 0 {
                    0
                } else {
                    let goal = s.qb(EE_UNITS_UAI_NEW_BUILD_REPAIR, site) as u64;
                    s.h.uai(EE_UNITS_UAI_SET_GOAL, a, goal, 0, 0, &mut s.io);
                    s.qc(EE_UNITS_UAI_START_ACTION, ACTION_MOVE_TO_REPAIR_SITE);
                    1
                }
            }
        }
        EE_UNITS_COND_ATTACK_GOAL => {
            let goal = s.q(EE_UNITS_UAI_ATTACK_GOAL) as u64;
            if goal == 0 {
                0
            } else {
                let t = s.qb(EE_UNITS_UAI_ATTACK_TARGET, goal) as u64;
                let valid = t != 0
                    && (!s.yes_b(EE_UNITS_UAI_ON_MAP, t)
                        || !s.yes_b(EE_UNITS_UAI_DISABLED, t)
                        || s.yes_b(EE_UNITS_UAI_PROTECTED, t));
                let friendly = t != 0 && (s.yes_b(EE_UNITS_UAI_SAME_PLAYER, t) || s.qb(EE_UNITS_UAI_STANCE, t) == 0);
                1 | i64::from(valid) << 1
                    | i64::from(s.yes_b(EE_UNITS_UAI_CONVERT_ATTACK, goal)) << 2
                    | i64::from(friendly) << 3
            }
        }
        EE_UNITS_COND_MEMBER => {
            let member = match c {
                0 => EE_UNITS_UAI_SELECT_TO_RELEASE,
                1 => EE_UNITS_UAI_ERUPT_VOLCANO,
                2 => EE_UNITS_UAI_HAS_SUNK,
                3 => EE_UNITS_UAI_ENEMIES_NEAR,
                _ => EE_UNITS_UAI_PASSER_NEAR,
            };
            s.qb(member, b)
        }
        EE_UNITS_COND_LAUNCH => launch(s, c),
        _ => return None,
    };
    *io = s.io;
    Some(v)
}

#[cfg(test)]
#[path = "uai_cond_tests.rs"]
mod tests;
