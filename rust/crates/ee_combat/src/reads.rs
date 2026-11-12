//! The reads behind the goals' and shots' hosts (R5b.2, seventh round): `EEUnitClass::FindImpactUnit`,
//! `FacesPoint` with `TurretsFacePoint`, `EEComplexUnit::IsInReach`, `EEUGAttackUnit::IsStillReachable` and
//! `IsGateClosed`, `EEIsEnemyWithinRange`.
//!
//! Each body is the original's statements over a host table of the world's reads ([`QueryHost`], the C++'s
//! `EeCombatQueryHost`): every handle is an `EEUnit*` (a player's: `EEPlayer*`), 0 for none; a read is about the
//! object it names and the call's unit (the host's context: the unit whose read this is, its goal). Reads are made
//! where and when the original makes them (several reach C++ code that records captures of its own). The facing test
//! is plain arithmetic over a record ([`EeCombatFacingIn`]) and its turret parts.

use std::ffi::c_void;

use ee_ge::transform::RotationOrder;
use ee_ge::types::{Ge3DLine, Ge3DPoint, GeTransformation, GeVector};
use ee_ge::um::um_arccos;

use crate::geometry::matrix;
use crate::walks::EeCombatHandle;

// ---- the reads (`QueryHost::read`): 0/1, an integer, or an f32's bits where noted

/// `GetTileX()`, `GetTileY()`.
pub const EE_COMBAT_Q_TILE_X: i32 = 1;
pub const EE_COMBAT_Q_TILE_Y: i32 = 2;
/// `GetHitPoints()` (a `long`).
pub const EE_COMBAT_Q_HIT_POINTS: i32 = 3;
/// `GetStats()->mMaxHitPoints`.
pub const EE_COMBAT_Q_MAX_HIT_POINTS: i32 = 4;
/// `GetStats()->GetScale()` (f32).
pub const EE_COMBAT_Q_STATS_SCALE: i32 = 5;
/// a complex unit's `mPhysicalModel.GetModel() != NULL`.
pub const EE_COMBAT_Q_MODEL: i32 = 6;
/// a complex unit's `mIsBuilt`.
pub const EE_COMBAT_Q_BUILT: i32 = 7;
/// `mUnitClass->GetUnitType()`, `->GetCategory()`.
pub const EE_COMBAT_Q_UNIT_TYPE: i32 = 8;
pub const EE_COMBAT_Q_CATEGORY: i32 = 9;
/// the class's flags: `EE_COMBAT_CLASS_*`.
pub const EE_COMBAT_Q_CLASS_FLAGS: i32 = 10;
/// `EEUUGetIsUnitClassTargetable(<arg 1: the call's unit, 0: NULL>, mUnitClass)`.
pub const EE_COMBAT_Q_TARGETABLE: i32 = 11;
/// a player's `GetPlayerID()`.
pub const EE_COMBAT_Q_PLAYER_ID: i32 = 12;
/// a player's `mEnemies[arg]`.
pub const EE_COMBAT_Q_ENEMY: i32 = 13;
/// a complex unit's `mIsFiringHigh`.
pub const EE_COMBAT_Q_FIRING_HIGH: i32 = 14;
/// `GetStats()->mCanFireOverWalls`.
pub const EE_COMBAT_Q_FIRE_OVER_WALLS: i32 = 15;
/// a complex unit's `IsPassableBy(<the call's unit>)`.
pub const EE_COMBAT_Q_PASSABLE: i32 = 16;
/// `gUnitSetManager.Contains(mUnitClass->GetUnitType(), arg)`.
pub const EE_COMBAT_Q_UNIT_SET: i32 = 17;
/// a complex unit's `mPhysicalModel.GetScale()` (f32).
pub const EE_COMBAT_Q_SCALE: i32 = 18;
/// a complex unit's `mCommand ? mCommand->mType : 0`.
pub const EE_COMBAT_Q_COMMAND: i32 = 19;
/// `EEUnitClassCast<EEUCGate>(GetUnitClass())->IsSidePart(mComplexUnit)`.
pub const EE_COMBAT_Q_SIDE_PART: i32 = 20;
/// the call's attack goal's `mPathAttempts`.
pub const EE_COMBAT_Q_PATH_ATTEMPTS: i32 = 21;
/// the call's unit's `CanReachUnit(<the unit>)`.
pub const EE_COMBAT_Q_CAN_REACH_UNIT: i32 = 22;
/// `EEIsAttackReachable(<the call's unit>, <the complex unit>, &tile)`.
pub const EE_COMBAT_Q_ATTACK_REACHABLE: i32 = 23;
/// the call's unit's `IsTargetIgnored(<the complex unit>, true)` (it may forget an old entry).
pub const EE_COMBAT_Q_IGNORED: i32 = 24;
/// a complex unit's `mTargetSearchTimer` (a `long`).
pub const EE_COMBAT_Q_SEARCH_TIMER: i32 = 25;
/// a complex unit's `GetLineOfSight()` (f32).
pub const EE_COMBAT_Q_LINE_OF_SIGHT: i32 = 26;
/// `EEIsSeenBy(<the complex unit, may be 0>, <the call's unit>->GetPlayer(), true)`.
pub const EE_COMBAT_Q_SEEN: i32 = 27;
/// `RTSDummyHead(<the unit>) != NULL` (1 for no unit).
pub const EE_COMBAT_Q_DUMMY_HEAD: i32 = 28;
/// a complex unit's `mIsSpecialAttack`.
pub const EE_COMBAT_Q_SPECIAL_ATTACK: i32 = 29;
/// `GetStats()->GetRange()` (f32).
pub const EE_COMBAT_Q_RANGE: i32 = 30;
/// the class's `mHealRange` (f32).
pub const EE_COMBAT_Q_HEAL_RANGE: i32 = 31;
/// the class's `mProjectileClass->mCategory`, [`EE_COMBAT_Q_NONE`] without one.
pub const EE_COMBAT_Q_PROJECTILE_CATEGORY: i32 = 32;
/// a complex unit's `mCollisionRadius` (f32).
pub const EE_COMBAT_Q_COLLISION_RADIUS: i32 = 33;
/// `EEGetHitDistanceSquared(<the call's unit>, <the unit>, -10000, -10000)` (f32).
pub const EE_COMBAT_Q_HIT_DISTANCE_SQUARED: i32 = 34;
/// the call's unit's `GetModel()->mTagPoints[mAnimation]`'s `GetNumTagPoints()` (0 without a list).
pub const EE_COMBAT_Q_TAG_COUNT: i32 = 35;
/// a complex unit's `mWeaponTagPoint`.
pub const EE_COMBAT_Q_WEAPON_TAG: i32 = 36;
/// `EEIsInRangeToHit(<the call's unit>, <the unit>)`.
pub const EE_COMBAT_Q_IN_RANGE_TO_HIT: i32 = 37;
/// `EECanBallisticHit(<the call's unit>, <the unit>)`.
pub const EE_COMBAT_Q_BALLISTIC_HIT: i32 = 38;
/// `EEIsInContactRange(<the call's unit>, <the unit>, <arg: an f32's bits>)`.
pub const EE_COMBAT_Q_CONTACT_RANGE: i32 = 39;
/// a complex unit's goal is an attack-unit goal (`EEGoalCast<EEUGAttackUnit>(mGoalState) != NULL`).
pub const EE_COMBAT_Q_ATTACK_GOAL: i32 = 40;
/// the number of units in the call's unit's region of `gWorld.mMap->mUnitRegions` ([`EE_COMBAT_Q_NONE`]: none).
pub const EE_COMBAT_Q_REGION_COUNT: i32 = 41;
/// a complex unit's `mInitialContactLocation`'s x, y, z (f32).
pub const EE_COMBAT_Q_CONTACT_X: i32 = 42;
pub const EE_COMBAT_Q_CONTACT_Y: i32 = 43;
pub const EE_COMBAT_Q_CONTACT_Z: i32 = 44;

/// A read's "none" (no projectile class, no region).
pub const EE_COMBAT_Q_NONE: u32 = u32::MAX;

/// `EE_COMBAT_Q_CLASS_FLAGS`: `mIsWall`, `mIsProjectile`, `mIsParatrooperBomber`, `mIsMelee`.
pub const EE_COMBAT_CLASS_WALL: u32 = 1;
pub const EE_COMBAT_CLASS_PROJECTILE: u32 = 2;
pub const EE_COMBAT_CLASS_PARATROOPER_BOMBER: u32 = 4;
pub const EE_COMBAT_CLASS_MELEE: u32 = 8;

// ---- the handles (`QueryHost::handle`)

/// `GetComplexUnit()` (as an `EEUnit*`).
pub const EE_COMBAT_H_COMPLEX: i32 = 1;
/// a complex unit's `mPreviousPart`.
pub const EE_COMBAT_H_PREVIOUS_PART: i32 = 2;
/// a complex unit's `mOwner`.
pub const EE_COMBAT_H_OWNER: i32 = 3;
/// a complex unit's attack-unit goal's `mTarget` (0 without that goal).
pub const EE_COMBAT_H_GOAL_TARGET: i32 = 4;
/// `mPlayer`.
pub const EE_COMBAT_H_PLAYER: i32 = 5;
/// unit `arg` of the call's unit's region.
pub const EE_COMBAT_H_REGION_UNIT: i32 = 6;
/// `static_cast<EEUnit*>(mTileNode->mPrev)`, `->mNext`.
pub const EE_COMBAT_H_TILE_PREV: i32 = 7;
pub const EE_COMBAT_H_TILE_NEXT: i32 = 8;
/// the call's goal's `mTarget` (the attack goal of `IsStillReachable`, `IsGateClosed`).
pub const EE_COMBAT_H_TARGET: i32 = 9;

// ---- the operations (`QueryHost::act`)

/// a complex unit's `mIsFiringHigh = true`.
pub const EE_COMBAT_A_FIRING_HIGH: i32 = 1;
/// a complex unit's `mWeaponTagPoint = arg`.
pub const EE_COMBAT_A_WEAPON_TAG: i32 = 2;
/// the call's unit's `IgnoreTarget(<the complex unit>)`.
pub const EE_COMBAT_A_IGNORE: i32 = 3;

// the constants the bodies compare with (ee/EEUnitClassIDs.h, ee/EEUnitAction.h, EEUnitClass.cpp)
const TYPE_FLAMING_ARROW: u32 = 0x2d5;
const TYPE_SP15_CAPITAL_SHIP: i32 = 0x2da;
const TYPE_ROMAN_LEGIONNAIRE: u32 = 0x2dc;
const TYPE_PILUM: u32 = 0x2e4;
const TYPE_ICBM: u32 = 0x2ee;
const TYPE_METEOR: u32 = 0x325;
const TYPE_CAPITAL_SHIP_YAMATO: i32 = 0x33c;
const TYPE_RES_FISH: u32 = 0x13b;
const CATEGORY_BUILDING: u32 = 2;
const CATEGORY_PROJECTILE: u32 = 9;
const CATEGORY_BOMB: u32 = 0xb;
const CATEGORY_BUILDING19: u32 = 0x13;
const CATEGORY_MINING_BUILDING: u32 = 0x18;
const CATEGORY_GATE: u32 = 0x19;
const CATEGORY_FORTRESS: u32 = 0x1d;
const ACTION_CLOSED: u32 = 15001;
/// the unit set a hit on which (not the target) makes the launcher fire high
const SET_FIRE_HIGH: i32 = 0x2a;
const LARGE_UNIT_SCALE: f32 = 1.5;
const MIN_CONSTRUCTION_SCALE: f32 = 0.1;
const FACING_TOLERANCE: f32 = 0.043633234;
// the C++'s literal 0.5235988f (kept as written)
#[allow(clippy::approx_constant)]
const SPECIAL_ARC: f32 = 0.5235988;
const FULL_ARC: f32 = 6.2831855;
// the C++'s literal 3.1415927f: the same f32
const PI: f32 = std::f32::consts::PI;

/// The world's reads of the bodies here (`EeCombatQueryHost` over the C++, a mock in the tests).
pub trait QueryHost {
    /// `EE_COMBAT_Q_*` of `object` with `arg`.
    fn read(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> u32;
    /// `EE_COMBAT_H_*` of `object` with `arg`.
    fn handle(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> EeCombatHandle;
    /// `EE_COMBAT_A_*` on `object` with `arg`.
    fn act(&mut self, code: i32, object: EeCombatHandle, arg: i32);
    /// a complex unit's `mPhysicalModel.GetLocation()`.
    fn location(&mut self, object: EeCombatHandle) -> GeTransformation;
    /// a complex unit's location's x, y, z (the location's translation).
    fn position(&mut self, object: EeCombatHandle) -> Ge3DPoint {
        let t = self.location(object);
        Ge3DPoint { x: t.x(), y: t.y(), z: t.z() }
    }
    /// the call's unit's `mPhysicalModel.Intersects(&<candidate>->mPhysicalModel, line, segment, lod, 1, 1, scale)`,
    /// the line set only when `segment` (else the default `GE3DLine`).
    fn intersects(&mut self, candidate: EeCombatHandle, line: &Ge3DLine, segment: bool, lod: u32, scale: f32) -> bool;
    /// the call's unit's `GetNearestFootprintTile(<object>, &x, &y)`.
    fn footprint_tile(&mut self, object: EeCombatHandle) -> (i32, i32);
}

fn float(bits: u32) -> f32 {
    f32::from_bits(bits)
}

fn flag<H: QueryHost>(h: &mut H, code: i32, object: EeCombatHandle) -> bool {
    h.read(code, object, 0) != 0
}

fn real<H: QueryHost>(h: &mut H, code: i32, object: EeCombatHandle) -> f32 {
    float(h.read(code, object, 0))
}

/// The first part of a part chain (`while (mPreviousPart) ...`).
fn first_part<H: QueryHost>(h: &mut H, mut unit: EeCombatHandle) -> EeCombatHandle {
    loop {
        let previous = h.handle(EE_COMBAT_H_PREVIOUS_PART, unit, 0);
        if previous == 0 {
            return unit;
        }
        unit = previous;
    }
}

/// The intersection test of a candidate, an unfinished one scaled by its health (a dead one never hit).
fn hits<H: QueryHost>(h: &mut H, candidate: EeCombatHandle, line: &Ge3DLine, segment: bool, lod: u32) -> bool {
    if flag(h, EE_COMBAT_Q_BUILT, candidate) {
        return h.intersects(candidate, line, segment, lod, 1.0);
    }
    // `(float)GetHitPoints() / (float)mMaxHitPoints`
    let hp = h.read(EE_COMBAT_Q_HIT_POINTS, candidate, 0) as i32;
    let max = h.read(EE_COMBAT_Q_MAX_HIT_POINTS, candidate, 0) as i32;
    let mut health = hp as f32 / max as f32;
    if health == 0.0 {
        return false;
    }
    if health < MIN_CONSTRUCTION_SCALE {
        health = MIN_CONSTRUCTION_SCALE;
    }
    h.intersects(candidate, line, segment, lod, health)
}

/// `EEUnitClass::FindImpactUnit(unit)`: the unit a shot hits, its attack target first, then its region's units in
/// storage order (walls, enemies); a hit on a fire-high set unit that is not the target makes the launcher fire high.
pub fn impact_unit<H: QueryHost>(h: &mut H, unit: EeCombatHandle) -> EeCombatHandle {
    let position = h.position(unit);
    let goal = Ge3DPoint {
        x: real(h, EE_COMBAT_Q_CONTACT_X, unit),
        y: real(h, EE_COMBAT_Q_CONTACT_Y, unit),
        z: real(h, EE_COMBAT_Q_CONTACT_Z, unit),
    };
    let direction = GeVector { x: goal.x - position.x, y: goal.y - position.y, z: goal.z - position.z };
    let scale = real(h, EE_COMBAT_Q_SCALE, unit);
    let segment = direction.magnitude_squared() > scale * scale;
    let line = Ge3DLine { start: position, direction };
    let attack = flag(h, EE_COMBAT_Q_ATTACK_GOAL, unit);
    let goal_target = if attack { h.handle(EE_COMBAT_H_GOAL_TARGET, unit, 0) } else { 0 };
    let mut result: EeCombatHandle = 0;
    if attack && goal_target != 0 {
        let target = h.handle(EE_COMBAT_H_COMPLEX, goal_target, 0);
        if target != 0 && flag(h, EE_COMBAT_Q_MODEL, target) {
            // `GETransformation::DistanceSquaredTo` of the two locations: their translations only
            let (from, to) = (h.position(unit), h.position(target));
            let (dz, dy, dx) = (from.z - to.z, from.y - to.y, from.x - to.x);
            let distance = dx * dx + dy * dy + dz * dz;
            let reach = real(h, EE_COMBAT_Q_STATS_SCALE, unit) + real(h, EE_COMBAT_Q_STATS_SCALE, target);
            if distance <= reach * reach {
                let lod = if real(h, EE_COMBAT_Q_STATS_SCALE, target) < LARGE_UNIT_SCALE { 1 } else { 2 };
                if hits(h, target, &line, segment, lod) {
                    result = target;
                }
            }
        }
    }
    if result == 0 {
        let unit_type = h.read(EE_COMBAT_Q_UNIT_TYPE, unit, 0);
        if unit_type == TYPE_FLAMING_ARROW || unit_type == TYPE_PILUM || unit_type == TYPE_ICBM {
            return result;
        }
        let count = h.read(EE_COMBAT_Q_REGION_COUNT, unit, 0);
        if count == EE_COMBAT_Q_NONE {
            return result;
        }
        let mut i = 0u32;
        while i < count && result == 0 {
            let candidate = h.handle(EE_COMBAT_H_REGION_UNIT, unit, i as i32);
            i += 1;
            if !flag(h, EE_COMBAT_Q_TARGETABLE, candidate) || candidate == h.handle(EE_COMBAT_H_OWNER, unit, 0) {
                continue;
            }
            let flags = h.read(EE_COMBAT_Q_CLASS_FLAGS, candidate, 0);
            if (flags & EE_COMBAT_CLASS_PARATROOPER_BOMBER != 0 && {
                let owner = h.handle(EE_COMBAT_H_OWNER, unit, 0);
                h.read(EE_COMBAT_Q_CLASS_FLAGS, owner, 0) & EE_COMBAT_CLASS_PARATROOPER_BOMBER != 0
            }) || !flag(h, EE_COMBAT_Q_MODEL, candidate)
            {
                continue;
            }
            let lod = if real(h, EE_COMBAT_Q_STATS_SCALE, candidate) >= LARGE_UNIT_SCALE { 2 } else { 1 };
            if flags & EE_COMBAT_CLASS_WALL != 0 {
                if flag(h, EE_COMBAT_Q_PASSABLE, candidate) {
                    continue;
                }
                // the target's first part (none without an attack goal or a complex target)
                let goal_carrier = if attack {
                    let target = h.handle(EE_COMBAT_H_GOAL_TARGET, unit, 0);
                    match h.handle(EE_COMBAT_H_COMPLEX, target, 0) {
                        0 => 0,
                        carrier => first_part(h, carrier),
                    }
                } else {
                    0
                };
                let carrier = first_part(h, candidate);
                if goal_carrier != carrier && {
                    let owner = h.handle(EE_COMBAT_H_OWNER, unit, 0);
                    flag(h, EE_COMBAT_Q_FIRE_OVER_WALLS, owner) && flag(h, EE_COMBAT_Q_FIRING_HIGH, owner)
                } {
                    continue;
                }
            } else {
                let player = h.handle(EE_COMBAT_H_PLAYER, candidate, 0);
                let own = h.handle(EE_COMBAT_H_PLAYER, unit, 0);
                if player == own
                    || {
                        let id = h.read(EE_COMBAT_Q_PLAYER_ID, player, 0);
                        h.read(EE_COMBAT_Q_ENEMY, own, id as i32) == 0
                    }
                    || (h.read(EE_COMBAT_Q_HIT_POINTS, candidate, 0) as i32) <= 0
                    || flags & EE_COMBAT_CLASS_PROJECTILE != 0
                {
                    continue;
                }
                let category = h.read(EE_COMBAT_Q_CATEGORY, candidate, 0);
                if (h.read(EE_COMBAT_Q_CATEGORY, unit, 0) != CATEGORY_BOMB
                    && h.read(EE_COMBAT_Q_UNIT_TYPE, unit, 0) != TYPE_METEOR
                    && matches!(
                        category,
                        CATEGORY_BUILDING | CATEGORY_BUILDING19 | CATEGORY_MINING_BUILDING | CATEGORY_FORTRESS
                    ))
                    || h.read(EE_COMBAT_Q_UNIT_TYPE, candidate, 0) == TYPE_RES_FISH
                {
                    continue;
                }
            }
            if hits(h, candidate, &line, segment, lod) {
                result = candidate;
            }
        }
    }
    if result != 0 && attack {
        let owner = h.handle(EE_COMBAT_H_OWNER, unit, 0);
        if owner != 0
            && result != h.handle(EE_COMBAT_H_GOAL_TARGET, unit, 0)
            && h.read(EE_COMBAT_Q_UNIT_SET, result, SET_FIRE_HIGH) != 0
            && !flag(h, EE_COMBAT_Q_FIRING_HIGH, owner)
        {
            h.act(EE_COMBAT_A_FIRING_HIGH, owner, 0);
        }
    }
    result
}

/// `EEComplexUnit::IsInReach(target)` of `unit`: melee contact, the range test, or a ballistic shot from any weapon
/// tag point (it leaves the point that hit, or the first); a legionnaire's special attack uses its heal range.
pub fn in_reach<H: QueryHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) -> bool {
    let mut range = real(h, EE_COMBAT_Q_RANGE, unit);
    let mut special = false;
    if h.read(EE_COMBAT_Q_UNIT_TYPE, unit, 0) == TYPE_ROMAN_LEGIONNAIRE && flag(h, EE_COMBAT_Q_SPECIAL_ATTACK, unit) {
        special = true;
        range = real(h, EE_COMBAT_Q_HEAL_RANGE, unit);
    }
    if range == 0.0 || (h.read(EE_COMBAT_Q_HIT_POINTS, target, 0) as i32) <= 0 {
        return false;
    }
    if h.read(EE_COMBAT_Q_CLASS_FLAGS, unit, 0) & EE_COMBAT_CLASS_MELEE != 0 && !special {
        let distance = h.read(EE_COMBAT_Q_HIT_DISTANCE_SQUARED, target, 0);
        return h.read(EE_COMBAT_Q_CONTACT_RANGE, target, distance as i32) != 0;
    }
    let projectile = h.read(EE_COMBAT_Q_PROJECTILE_CATEGORY, unit, 0);
    if projectile == EE_COMBAT_Q_NONE || (projectile != CATEGORY_PROJECTILE && projectile != CATEGORY_BOMB) {
        return flag(h, EE_COMBAT_Q_IN_RANGE_TO_HIT, target);
    }
    let extra = real(h, EE_COMBAT_Q_COLLISION_RADIUS, unit);
    if !(real(h, EE_COMBAT_Q_HIT_DISTANCE_SQUARED, target) <= (extra + range) * (extra + range)) {
        return false;
    }
    let count = h.read(EE_COMBAT_Q_TAG_COUNT, unit, 0);
    let first = h.read(EE_COMBAT_Q_WEAPON_TAG, unit, 0);
    let mut index = first;
    loop {
        if h.read(EE_COMBAT_Q_WEAPON_TAG, unit, 0) != index {
            h.act(EE_COMBAT_A_WEAPON_TAG, unit, index as i32);
        }
        let result = flag(h, EE_COMBAT_Q_BALLISTIC_HIT, target);
        if count <= 1 {
            return result;
        }
        index = index.wrapping_add(1) % count;
        if result || index == first {
            return result;
        }
    }
}

/// `EEUGAttackUnit::IsStillReachable(unit)`: a melee unit must reach the target, any unit find an attack spot, at
/// most 25 path attempts; if not, the unit starts ignoring the target (unless it already does).
pub fn still_reachable<H: QueryHost>(h: &mut H, unit: EeCombatHandle) -> bool {
    let target = h.handle(EE_COMBAT_H_TARGET, unit, 0);
    let mut result = false;
    if h.read(EE_COMBAT_Q_CLASS_FLAGS, unit, 0) & EE_COMBAT_CLASS_MELEE == 0
        || flag(h, EE_COMBAT_Q_CAN_REACH_UNIT, target)
    {
        let complex = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
        result = flag(h, EE_COMBAT_Q_ATTACK_REACHABLE, complex);
        if result {
            result = h.read(EE_COMBAT_Q_PATH_ATTEMPTS, unit, 0) <= 0x19;
            if result {
                return result;
            }
        }
    }
    let complex = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
    if complex != 0 && !flag(h, EE_COMBAT_Q_IGNORED, complex) {
        h.act(EE_COMBAT_A_IGNORE, complex, 0);
    }
    result
}

/// `EEUGAttackUnit::IsGateClosed(unit)`: the target is a gate's middle part whose gate is not closed (despite the
/// name).
pub fn gate_closed<H: QueryHost>(h: &mut H, unit: EeCombatHandle) -> bool {
    let target = h.handle(EE_COMBAT_H_TARGET, unit, 0);
    if h.read(EE_COMBAT_Q_CATEGORY, target, 0) != CATEGORY_GATE {
        return false;
    }
    if flag(h, EE_COMBAT_Q_SIDE_PART, target) {
        return false;
    }
    let complex = h.handle(EE_COMBAT_H_COMPLEX, target, 0);
    let carrier = first_part(h, complex);
    h.read(EE_COMBAT_Q_COMMAND, carrier, 0) != ACTION_CLOSED
}

/// One side of `EEIsEnemyWithinRange`'s walk: whether `candidate` is no enemy in reach (`seen` is the side's first
/// unit's complex unit: the original tests that one's visibility for every candidate).
fn passed_over<H: QueryHost>(
    h: &mut H,
    candidate: EeCombatHandle,
    seen: EeCombatHandle,
    player: EeCombatHandle,
    tile: (i32, i32),
    reach_squared: f32,
) -> bool {
    let other = h.handle(EE_COMBAT_H_PLAYER, candidate, 0);
    if other == player {
        return true;
    }
    let id = h.read(EE_COMBAT_Q_PLAYER_ID, other, 0);
    if h.read(EE_COMBAT_Q_ENEMY, player, id as i32) == 0
        || h.read(EE_COMBAT_Q_PLAYER_ID, other, 0) == 0
        || h.read(EE_COMBAT_Q_TARGETABLE, candidate, 1) == 0
        || (h.read(EE_COMBAT_Q_HIT_POINTS, candidate, 0) as i32) <= 1
        || !flag(h, EE_COMBAT_Q_SEEN, seen)
    {
        return true;
    }
    let (tx, ty) = h.footprint_tile(candidate);
    let (x, y) = tile;
    let dx = x.wrapping_sub(tx);
    let dy = y.wrapping_sub(ty);
    reach_squared < dy.wrapping_mul(dy).wrapping_add(dx.wrapping_mul(dx)) as f32
}

/// `EEIsEnemyWithinRange(unit)`: an enemy the unit may target within its line of sight, walking the x-sorted tile
/// chain both ways from the unit (only when its search timer has run out).
pub fn enemy_in_range<H: QueryHost>(h: &mut H, unit: EeCombatHandle) -> bool {
    if (h.read(EE_COMBAT_Q_SEARCH_TIMER, unit, 0) as i32) > 0 {
        return false;
    }
    let x = h.read(EE_COMBAT_Q_TILE_X, unit, 0) as i32;
    let y = h.read(EE_COMBAT_Q_TILE_Y, unit, 0) as i32;
    let player = h.handle(EE_COMBAT_H_PLAYER, unit, 0);
    let reach = real(h, EE_COMBAT_Q_LINE_OF_SIGHT, unit);
    let outer = (reach + 4.0) * (reach + 4.0);
    let reach_squared = reach * reach;
    let mut forward = h.handle(EE_COMBAT_H_TILE_PREV, unit, 0);
    let forward_unit = if forward != 0 { h.handle(EE_COMBAT_H_COMPLEX, forward, 0) } else { 0 };
    let mut backward = h.handle(EE_COMBAT_H_TILE_NEXT, unit, 0);
    let backward_unit = if backward != 0 { h.handle(EE_COMBAT_H_COMPLEX, backward, 0) } else { 0 };
    let (mut done, mut back_done) = (false, false);
    while !done || !back_done {
        if !done {
            let near = !flag(h, EE_COMBAT_Q_DUMMY_HEAD, forward) && {
                let dx = x.wrapping_sub(h.read(EE_COMBAT_Q_TILE_X, forward, 0) as i32);
                dx.wrapping_mul(dx) as f32 <= outer
            };
            if !near {
                done = true;
            } else if passed_over(h, forward, forward_unit, player, (x, y), reach_squared) {
                forward = h.handle(EE_COMBAT_H_TILE_PREV, forward, 0);
            } else {
                return true;
            }
        }
        if back_done {
            continue;
        }
        if backward == 0 || {
            let dx = (h.read(EE_COMBAT_Q_TILE_X, backward, 0) as i32).wrapping_sub(x);
            outer < dx.wrapping_mul(dx) as f32
        } {
            back_done = true;
        } else if passed_over(h, backward, backward_unit, player, (x, y), reach_squared) {
            backward = h.handle(EE_COMBAT_H_TILE_NEXT, backward, 0);
        } else {
            return true;
        }
    }
    false
}

// ---- the facing test

/// `FacesPoint`'s reads (plain): the unit's location, the point, the class's arc, offset and broadside flag, the unit's
/// type and special attack, its model's turret part count and its stats' scale.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatFacingIn {
    /// `mPhysicalModel.GetLocation()` (`mMatrix`, row by row)
    pub location: [f32; 16],
    /// the point's x, y
    pub point: [f32; 2],
    pub use_tolerance: i32,
    /// the class (`this`)'s `mFiringArc`, `mFacingOffset`, `mFiresBroadside`
    pub firing_arc: f32,
    pub facing_offset: f32,
    pub fires_broadside: i32,
    /// the unit's `mUnitClass->GetUnitType()`, `mIsSpecialAttack`
    pub unit_type: i32,
    pub special_attack: i32,
    /// `GetModel()->GetNumParts()` (its `mTurretParts.size()`)
    pub turrets: i32,
    /// `mStats->GetScale()`
    pub scale: f32,
}

/// One turret part: its `mPosition`, the model's `GetTurretPartTransformation(i)`, the unit's `mTurretAngles[i]`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatTurret {
    pub position: [f32; 3],
    pub transformation: [f32; 16],
    pub angle: f32,
}

/// `EEUnitClass::FacesPoint`: the unit (or its turrets) faces the point within its firing arc (`use_tolerance`) or
/// the fixed tolerance; a broadside ship also counts its opposite side. `turret(i)` reads turret part `i`.
pub fn faces_point(i: &EeCombatFacingIn, turret: impl FnMut(i32) -> EeCombatTurret) -> bool {
    let location = matrix(&i.location);
    let position = Ge3DPoint { x: location.x(), y: location.y(), z: location.z() };
    let mut tolerance = i.firing_arc;
    if (i.unit_type == TYPE_SP15_CAPITAL_SHIP || i.unit_type == TYPE_CAPITAL_SHIP_YAMATO) && i.special_attack != 0 {
        tolerance = SPECIAL_ARC;
    }
    if i.turrets > 0 {
        return turrets_face_point(i, &location, turret);
    }
    if (i.point[0] == position.x && i.point[1] == position.y) || tolerance == FULL_ARC {
        return true;
    }
    let mut direction = GeVector { x: i.point[0] - position.x, y: i.point[1] - position.y, z: 0.0 };
    direction.normalize();
    let mut rotation = GeTransformation::IDENTITY;
    rotation.set_orientation(RotationOrder::Ypr, i.facing_offset, 0.0, 0.0);
    let rotated = rotation.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
    let mut forward = location.apply_vector(rotated);
    forward.z = 0.0;
    forward.normalize();
    // the C++'s two tests (a NaN stays NaN, as with clamp)
    let dot = forward.dot(direction).clamp(-1.0, 1.0);
    let angle = um_arccos(dot).abs();
    if i.fires_broadside == 0 {
        if i.use_tolerance != 0 {
            return angle <= tolerance * 0.5;
        }
        return angle <= FACING_TOLERANCE;
    }
    let opposite = (angle - PI).abs();
    if i.use_tolerance != 0 {
        let half = tolerance * 0.5;
        return angle <= half || opposite <= half;
    }
    angle <= FACING_TOLERANCE || opposite <= FACING_TOLERANCE
}

/// `EEUnitClass::TurretsFacePoint`: every turret part faces the point within the class's arc (`use_tolerance`, the
/// arc itself, not the special one) or the fixed tolerance.
fn turrets_face_point(
    i: &EeCombatFacingIn,
    location: &GeTransformation,
    mut turret: impl FnMut(i32) -> EeCombatTurret,
) -> bool {
    let mut result = true;
    for index in 0..i.turrets.max(0) {
        if !result {
            return false;
        }
        let part = turret(index);
        let offset =
            Ge3DPoint { x: i.scale * part.position[0], y: i.scale * part.position[1], z: i.scale * part.position[2] };
        let world = location.apply_point(offset);
        let mut direction = GeVector { x: i.point[0] - world.x, y: i.point[1] - world.y, z: 0.0 };
        direction.normalize();
        let mut rotation = GeTransformation::IDENTITY;
        rotation.set_orientation(RotationOrder::Ypr, part.angle, 0.0, 0.0);
        let rotated = rotation.apply_vector(GeVector { x: 0.0, y: 1.0, z: 0.0 });
        let part_facing = matrix(&part.transformation).apply_vector(rotated);
        let mut facing = location.apply_vector(part_facing);
        facing.z = 0.0;
        facing.normalize();
        let mut dot = facing.z * direction.z + facing.y * direction.y + direction.x * facing.x;
        if dot > 1.0 {
            dot = 1.0;
        } else if !(dot >= -1.0) {
            dot = -1.0;
        }
        let mut angle = um_arccos(dot);
        if !(angle >= 0.0) {
            angle = -angle;
        }
        result = if i.use_tolerance != 0 { !(angle > i.firing_arc * 0.5) } else { !(angle > FACING_TOLERANCE) };
    }
    result
}

// ---- the plain reads from the unit store (R5b.4's ee_units store, step 4 of the combat plan)

/// `RTSUnit::mUnitID` of the object (the store's key).
pub const EE_COMBAT_Q_ID: i32 = 45;

/// A host whose plain reads of a unit come from the core lane's unit store (ee_units' `store`) when the unit has a
/// record there (it is in a player's list, and the record is of that object: the store's `object` is the
/// `RTSUnit*`, the `EEUnit*`'s address); the other reads, and these when the store has no record, go to the host.
/// The store is written through at every write of a field it mirrors, so a record equals the object at any point.
pub struct Stored<'a, H: QueryHost> {
    pub host: &'a mut H,
    /// the last object asked about and its id (a body reads one unit's fields in a row)
    last: (EeCombatHandle, u32),
}

impl<'a, H: QueryHost> Stored<'a, H> {
    pub fn new(host: &'a mut H) -> Self {
        Stored { host, last: (0, 0) }
    }

    fn record(&mut self, object: EeCombatHandle) -> Option<UnitRecord> {
        if object == 0 {
            return None;
        }
        if self.last.0 != object {
            self.last = (object, self.host.read(EE_COMBAT_Q_ID, object, 0));
        }
        let id = self.last.1;
        stored_record(id).filter(|r| r.object == object)
    }
}

type UnitRecord = EeCombatStoredUnit;

/// The unit store's fields the reads take (ee_units' `EeUnitRecord`'s, copied by ee_rust's bridge: ee_combat does not
/// depend on ee_units, whose crate imports C++ functions no test binary of this crate has).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatStoredUnit {
    /// the `RTSUnit*` the record is of (the `EEUnit*`'s address)
    pub object: u64,
    /// ee_units' `EE_OBJECT_*` (`EE_COMBAT_STORED_COMPLEX_UNIT`: an `EEComplexUnit`)
    pub object_kind: u8,
    pub built: u8,
    pub tile_x: i64,
    pub tile_y: i64,
    pub hit_points: i64,
    /// -1: no class
    pub unit_type: i64,
    pub category: i64,
    pub command: i64,
    /// the location's x, y, z as float bits
    pub location: [u32; 3],
    /// `EEComplexUnit::mTargetSearchTimer`
    pub target_search_timer: i64,
}

/// ee_units' `EE_OBJECT_COMPLEX_UNIT`.
pub const EE_COMBAT_STORED_COMPLEX_UNIT: u8 = 2;

/// ee_rust defines the bridge (and declares it in its header): cbindgen must not declare it here again.
/// cbindgen:ignore
#[cfg(not(test))]
mod bridge {
    use super::EeCombatStoredUnit;
    extern "C" {
        /// ee_rust's bridge to the core lane's unit store: the record under `id` into `out`; false for none.
        pub fn ee_combat_store_unit(id: u32, out: *mut EeCombatStoredUnit) -> bool;
    }
}

/// The store's record under `id` (in the unit tests their own map).
fn stored_record(id: u32) -> Option<UnitRecord> {
    #[cfg(not(test))]
    {
        let mut out = EeCombatStoredUnit::default();
        // SAFETY: the bridge writes one record into `out`
        unsafe { bridge::ee_combat_store_unit(id, &mut out) }.then_some(out)
    }
    #[cfg(test)]
    {
        tests::stored(id)
    }
}

impl<H: QueryHost> QueryHost for Stored<'_, H> {
    fn read(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> u32 {
        // the store's `long`s are the C++ values; the reads answer their low 32 bits as the host does
        let field = |r: &UnitRecord| -> Option<u32> {
            match code {
                EE_COMBAT_Q_TILE_X => Some(r.tile_x as u32),
                EE_COMBAT_Q_TILE_Y => Some(r.tile_y as u32),
                EE_COMBAT_Q_HIT_POINTS => Some(r.hit_points as u32),
                EE_COMBAT_Q_UNIT_TYPE if r.unit_type != -1 => Some(r.unit_type as u32),
                EE_COMBAT_Q_CATEGORY if r.category != -1 => Some(r.category as u32),
                EE_COMBAT_Q_BUILT if r.object_kind == EE_COMBAT_STORED_COMPLEX_UNIT => Some(u32::from(r.built)),
                EE_COMBAT_Q_COMMAND if r.object_kind == EE_COMBAT_STORED_COMPLEX_UNIT => Some(r.command as u32),
                EE_COMBAT_Q_SEARCH_TIMER if r.object_kind == EE_COMBAT_STORED_COMPLEX_UNIT => {
                    Some(r.target_search_timer as u32)
                }
                _ => None,
            }
        };
        if matches!(
            code,
            EE_COMBAT_Q_TILE_X
                | EE_COMBAT_Q_TILE_Y
                | EE_COMBAT_Q_HIT_POINTS
                | EE_COMBAT_Q_UNIT_TYPE
                | EE_COMBAT_Q_CATEGORY
                | EE_COMBAT_Q_BUILT
                | EE_COMBAT_Q_COMMAND
                | EE_COMBAT_Q_SEARCH_TIMER
        ) {
            if let Some(value) = self.record(object).as_ref().and_then(field) {
                return value;
            }
        }
        self.host.read(code, object, arg)
    }
    fn handle(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> EeCombatHandle {
        self.host.handle(code, object, arg)
    }
    fn act(&mut self, code: i32, object: EeCombatHandle, arg: i32) {
        self.host.act(code, object, arg)
    }
    fn location(&mut self, object: EeCombatHandle) -> GeTransformation {
        self.host.location(object)
    }
    fn position(&mut self, object: EeCombatHandle) -> Ge3DPoint {
        if let Some(r) = self.record(object).filter(|r| r.object_kind == EE_COMBAT_STORED_COMPLEX_UNIT) {
            return Ge3DPoint {
                x: f32::from_bits(r.location[0]),
                y: f32::from_bits(r.location[1]),
                z: f32::from_bits(r.location[2]),
            };
        }
        self.host.position(object)
    }
    fn intersects(&mut self, candidate: EeCombatHandle, line: &Ge3DLine, segment: bool, lod: u32, scale: f32) -> bool {
        self.host.intersects(candidate, line, segment, lod, scale)
    }
    fn footprint_tile(&mut self, object: EeCombatHandle) -> (i32, i32) {
        self.host.footprint_tile(object)
    }
}

// ---- the C ABI

/// The C++ host of the reads (see [`QueryHost`]; `ctx` is the C++'s context: the call's unit and goal).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatQueryHost {
    pub ctx: *mut c_void,
    pub read: unsafe extern "C" fn(ctx: *mut c_void, code: i32, object: u64, arg: i32) -> u32,
    pub handle: unsafe extern "C" fn(ctx: *mut c_void, code: i32, object: u64, arg: i32) -> u64,
    pub act: unsafe extern "C" fn(ctx: *mut c_void, code: i32, object: u64, arg: i32),
    /// the location's `mMatrix` row by row into `out`
    pub location: unsafe extern "C" fn(ctx: *mut c_void, object: u64, out: *mut [f32; 16]),
    /// `line`: start x, y, z, direction x, y, z
    pub intersects: unsafe extern "C" fn(
        ctx: *mut c_void,
        candidate: u64,
        line: *const [f32; 6],
        segment: i32,
        lod: u32,
        scale: f32,
    ) -> i32,
    pub footprint_tile: unsafe extern "C" fn(ctx: *mut c_void, object: u64, out: *mut [i32; 2]),
}

// SAFETY (every call below): the table's contract: `ctx` is live for the call that handed it over, and the callbacks
// are callable with it.
impl QueryHost for EeCombatQueryHost {
    fn read(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> u32 {
        unsafe { (self.read)(self.ctx, code, object, arg) }
    }
    fn handle(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> EeCombatHandle {
        unsafe { (self.handle)(self.ctx, code, object, arg) }
    }
    fn act(&mut self, code: i32, object: EeCombatHandle, arg: i32) {
        unsafe { (self.act)(self.ctx, code, object, arg) }
    }
    fn location(&mut self, object: EeCombatHandle) -> GeTransformation {
        let mut out = [0f32; 16];
        unsafe { (self.location)(self.ctx, object, &mut out) };
        matrix(&out)
    }
    fn intersects(&mut self, candidate: EeCombatHandle, line: &Ge3DLine, segment: bool, lod: u32, scale: f32) -> bool {
        let rows = [line.start.x, line.start.y, line.start.z, line.direction.x, line.direction.y, line.direction.z];
        unsafe { (self.intersects)(self.ctx, candidate, &rows, i32::from(segment), lod, scale) != 0 }
    }
    fn footprint_tile(&mut self, object: EeCombatHandle) -> (i32, i32) {
        let mut out = [0i32; 2];
        unsafe { (self.footprint_tile)(self.ctx, object, &mut out) };
        (out[0], out[1])
    }
}

/// `FindImpactUnit` of `unit` (an `EEUnit*`): the unit hit, 0 for none.
///
/// # Safety
/// `host` must point to a valid table whose callbacks may be called with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_impact_unit(unit: u64, host: *const EeCombatQueryHost) -> u64 {
    let mut h = unsafe { *host };
    impact_unit(&mut Stored::new(&mut h), unit)
}

/// `IsInReach` of `unit` on `target`.
///
/// # Safety
/// As [`ee_combat_impact_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_combat_in_reach(unit: u64, target: u64, host: *const EeCombatQueryHost) -> i32 {
    let mut h = unsafe { *host };
    i32::from(in_reach(&mut Stored::new(&mut h), unit, target))
}

/// `IsStillReachable` of `unit` (its attack goal is the host's).
///
/// # Safety
/// As [`ee_combat_impact_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_combat_still_reachable(unit: u64, host: *const EeCombatQueryHost) -> i32 {
    let mut h = unsafe { *host };
    i32::from(still_reachable(&mut Stored::new(&mut h), unit))
}

/// `IsGateClosed` of `unit` (its attack goal is the host's).
///
/// # Safety
/// As [`ee_combat_impact_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_combat_gate_closed(unit: u64, host: *const EeCombatQueryHost) -> i32 {
    let mut h = unsafe { *host };
    i32::from(gate_closed(&mut Stored::new(&mut h), unit))
}

/// `EEIsEnemyWithinRange` of `unit`.
///
/// # Safety
/// As [`ee_combat_impact_unit`].
#[no_mangle]
pub unsafe extern "C" fn ee_combat_enemy_in_range(unit: u64, host: *const EeCombatQueryHost) -> i32 {
    let mut h = unsafe { *host };
    i32::from(enemy_in_range(&mut Stored::new(&mut h), unit))
}

/// `FacesPoint` over `input`; `turret(ctx, i, out)` reads turret part `i`.
///
/// # Safety
/// `input` must point to a valid record and `turret` be callable with `ctx` for every index below `input.turrets`.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_faces_point(
    input: *const EeCombatFacingIn,
    ctx: *mut c_void,
    turret: unsafe extern "C" fn(ctx: *mut c_void, index: i32, out: *mut EeCombatTurret),
) -> i32 {
    let i = unsafe { *input };
    i32::from(faces_point(&i, |index| {
        let mut out = EeCombatTurret::default();
        // SAFETY: the function's contract
        unsafe { turret(ctx, index, &mut out) };
        out
    }))
}

#[cfg(test)]
#[path = "reads_tests.rs"]
mod tests;
