//! The attack's world steps (R5b.2, eighth round): `EEComplexUnit::IsTargetIgnored`, `IgnoreTarget`, `CanReachUnit`,
//! `IsOnTileBorderingTarget`, `EEIsAttackReachable`, `EEUnitClass::Attacked` and `FireShot`.
//!
//! Each body is the original's statements over [`WorldHost`]: the reads' host ([`QueryHost`], the C++'s
//! `EeCombatQueryHost`, its plain unit fields from the core lane's store through [`Stored`]) and one operation
//! callback (`op`: a code, up to two handles and an argument block) for what touches the world's objects (the
//! ignored targets' list, the continents, the path search, goals, effects, the shot's launch and hit). Reads and
//! operations are made where and when the original makes them; the decisions, the list search and the path window's
//! arithmetic are here. Every handle is an `EEUnit*` (a continent's: an `EEAIContinent*`), 0 for none.

use crate::ceil;
use crate::damage::apply_hit;
use crate::reads::{
    EeCombatQueryHost, QueryHost, Stored, EE_COMBAT_A_WEAPON_TAG, EE_COMBAT_CLASS_PROJECTILE, EE_COMBAT_H_GOAL_TARGET,
    EE_COMBAT_H_PLAYER, EE_COMBAT_Q_CATEGORY, EE_COMBAT_Q_CLASS_FLAGS, EE_COMBAT_Q_ENEMY, EE_COMBAT_Q_FIRE_OVER_WALLS,
    EE_COMBAT_Q_FIRING_HIGH, EE_COMBAT_Q_HEAL_RANGE, EE_COMBAT_Q_HIT_POINTS, EE_COMBAT_Q_ID, EE_COMBAT_Q_LINE_OF_SIGHT,
    EE_COMBAT_Q_NONE, EE_COMBAT_Q_PLAYER_ID, EE_COMBAT_Q_RANGE, EE_COMBAT_Q_SPECIAL_ATTACK, EE_COMBAT_Q_TAG_COUNT,
    EE_COMBAT_Q_TILE_X, EE_COMBAT_Q_TILE_Y, EE_COMBAT_Q_UNIT_TYPE, EE_COMBAT_Q_WEAPON_TAG,
};
use crate::walks::EeCombatHandle;
use ee_ge::um::lrintf;
use std::ffi::c_void;

// ---- the reads (`QueryHost::read`; reads.rs's numbering goes on)

/// `gPhysics->mGameTime`.
pub const EE_COMBAT_Q_GAME_TIME: i32 = 46;
/// `mUnitClass->mTileSize`.
pub const EE_COMBAT_Q_TILE_SIZE: i32 = 47;
/// a complex unit's `mIgnoredTargets.size()`.
pub const EE_COMBAT_Q_IGNORED_COUNT: i32 = 48;
/// `mUnitClass->mMaxVelocity` (f32).
pub const EE_COMBAT_Q_CLASS_MAX_VELOCITY: i32 = 49;
/// `mUnitClass->mDomains`.
pub const EE_COMBAT_Q_DOMAINS: i32 = 50;
/// a complex unit's `mContainer != NULL`.
pub const EE_COMBAT_Q_CONTAINER: i32 = 51;
/// `gWorld.mMap->mSize`.
pub const EE_COMBAT_Q_MAP_SIZE: i32 = 52;
/// `<the call's unit>->DistanceSquaredTo(<the unit>)` (f32).
pub const EE_COMBAT_Q_DISTANCE_SQUARED: i32 = 53;
/// `GetStats()->mMaxVelocity` (f32).
pub const EE_COMBAT_Q_STATS_MAX_VELOCITY: i32 = 54;
/// a complex unit's `mBehavior->mCanTargetEnemies`, `mBehavior->CanMoveFreely(<it>)`.
pub const EE_COMBAT_Q_TARGETS_ENEMIES: i32 = 55;
pub const EE_COMBAT_Q_MOVES_FREELY: i32 = 56;
/// a complex unit's `mGoalState->mKind` ([`EE_COMBAT_Q_NONE`] without a goal), `mGoalState->GetB1C()`.
pub const EE_COMBAT_Q_GOAL_KIND: i32 = 57;
pub const EE_COMBAT_Q_GOAL_B1C: i32 = 58;
/// `GetUnitClass()->IsLandBuilding()`, `GetStats()->mAttack` (f32).
pub const EE_COMBAT_Q_LAND_BUILDING: i32 = 59;
pub const EE_COMBAT_Q_ATTACK: i32 = 60;
/// a complex unit's `mReloadTimer`, `mStats->mReloadTime`, `mAmmunition`.
pub const EE_COMBAT_Q_RELOAD_TIMER: i32 = 61;
pub const EE_COMBAT_Q_RELOAD_TIME: i32 = 62;
pub const EE_COMBAT_Q_AMMUNITION: i32 = 63;
/// the shot's class's (`FireShot`'s `this`) fields: `EE_COMBAT_SHOT_CLASS_*`.
pub const EE_COMBAT_Q_SHOT_CLASS: i32 = 64;
/// `GetModel()->mTurretParts.size()`.
pub const EE_COMBAT_Q_TURRET_PARTS: i32 = 65;
/// the player's epoch's `mHighTechTools` (epochs.yaml: a citizen's shot plays its effect).
pub const EE_COMBAT_Q_HIGH_TECH_TOOLS: i32 = 66;
/// `mUnitClass->mImpactSparksEffect != NULL`.
pub const EE_COMBAT_Q_SPARKS_EFFECT: i32 = 67;

/// [`EE_COMBAT_Q_SHOT_CLASS`]: `mProjectileClass`, `mUsesAmmunition`, `mMuzzleFlashEffect`, `mShotEffect`.
pub const EE_COMBAT_SHOT_CLASS_PROJECTILE: u32 = 1;
pub const EE_COMBAT_SHOT_CLASS_AMMUNITION: u32 = 2;
pub const EE_COMBAT_SHOT_CLASS_MUZZLE_FLASH: u32 = 4;
pub const EE_COMBAT_SHOT_CLASS_SHOT_EFFECT: u32 = 8;

// ---- the operations (`WorldHost::op`: `object`, `other`, the argument block in and out; the answer)

/// `object`'s ignored entry `args[0]` into `args[0..6]` (tile x, y, from x, y, unit id, time).
pub const EE_COMBAT_W_IGNORED_ENTRY: i32 = 1;
/// `object`'s ignored entries `args[0]` to `args[1]` (-1: the end) erased.
pub const EE_COMBAT_W_IGNORED_ERASE: i32 = 2;
/// an entry `args[0..6]` pushed at the front of `object`'s list.
pub const EE_COMBAT_W_IGNORED_PUSH: i32 = 3;
/// `object`'s entry `args[0]`'s time set to `args[1]`.
pub const EE_COMBAT_W_IGNORED_TIME: i32 = 4;
/// `EEIsCornerBlocked(args[0], args[1], args[2], args[3])`.
pub const EE_COMBAT_W_CORNER_BLOCKED: i32 = 5;
/// `gContinentManager.CanGetToUnit(object, other)`.
pub const EE_COMBAT_W_CAN_GET_TO_UNIT: i32 = 6;
/// `gContinentManager.GetContinentAt(object)` (a continent's handle).
pub const EE_COMBAT_W_CONTINENT_AT: i32 = 7;
/// `gContinentManager.mTileFlags[object->mID * mFlagsWidth + other->mID]` (two continents).
pub const EE_COMBAT_W_CONTINENT_LINK: i32 = 8;
/// `gContinentManager.AreConnectedForUnit(args[0] (a unit), object, other)` (two continents).
pub const EE_COMBAT_W_CONNECTED_FOR_UNIT: i32 = 9;
/// `object->IsOnOtherContinentThan(other)`.
pub const EE_COMBAT_W_OTHER_CONTINENT: i32 = 10;
/// `gPathManager.FindPath(object, map, from, to, bounds, path, 0xb, limit, <an attackable-by modifier of object and
/// other>)`, `args`: from x, y, to x, y, bounds top, left, bottom, right, limit; answers the path's length, its first
/// tile in `args[0..2]`.
pub const EE_COMBAT_W_FIND_PATH: i32 = 11;
/// `EEIsSeenByOwnerOf(object, other)`.
pub const EE_COMBAT_W_SEEN_BY_OWNER: i32 = 12;
/// `Attacked`'s goal on `object` against `other` (`args[0]`: a location goal at its x, y, else an attack goal), set
/// (`SetGoal(goal, 1, 0)`) and kept for the helpers.
pub const EE_COMBAT_W_ATTACKED_GOAL: i32 = 13;
/// `EEGetUnitsNear(<object's location>, kCallForHelpRadius, units)` kept; answers their count.
pub const EE_COMBAT_W_UNITS_NEAR: i32 = 14;
/// the kept units' `args[0]`th.
pub const EE_COMBAT_W_NEAR_UNIT: i32 = 15;
/// `object->CanAttackClass(other->GetUnitClass())`.
pub const EE_COMBAT_W_CAN_ATTACK_CLASS: i32 = 16;
/// `object->IsTargetIgnored(other, true)`.
pub const EE_COMBAT_W_IS_IGNORED: i32 = 17;
/// `EEIsAttackReachable(object, other, &tile)`.
pub const EE_COMBAT_W_ATTACK_REACHABLE: i32 = 18;
/// the kept goal's clone on `object`, when its class has the goal's start action: set, started, `ActionChanged`.
pub const EE_COMBAT_W_HELP: i32 = 19;
/// `object->IgnoreTarget(other)`.
pub const EE_COMBAT_W_IGNORE: i32 = 20;
/// `object`'s `mLastAttacker` dereferenced and cleared.
pub const EE_COMBAT_W_RELEASE_ATTACKER: i32 = 21;
/// `object`'s `mLastAttacker`.
pub const EE_COMBAT_W_LAST_ATTACKER: i32 = 22;
/// the class's `LaunchStoredMissile(object)`.
pub const EE_COMBAT_W_LAUNCH_STORED_MISSILE: i32 = 23;
/// `object->SetRandomReloadTimer(args[0])`.
pub const EE_COMBAT_W_RELOAD: i32 = 24;
/// `EEFindDropLocation(object, &target, &point, NULL)`: answers the target, keeps the point.
pub const EE_COMBAT_W_FIND_DROP: i32 = 25;
/// `EEIsLineToUnitBlocked(object, other, &blocker, &x, &y)`: `args[0]` the blocker's handle, `args[1..3]` x, y.
pub const EE_COMBAT_W_LINE_BLOCKED: i32 = 26;
/// the impact sparks of `object`'s class on it; the impact sparks placed on tile `args[0..2]` (+0.5, its height).
pub const EE_COMBAT_W_SPARKS_ON: i32 = 27;
pub const EE_COMBAT_W_SPARKS_AT: i32 = 28;
// 29 was the class's `ApplyHit` (damage.rs `apply_hit` since the tenth round)
/// the class's `LaunchShot(object, other, point, NULL)`, `LaunchFromTurret(object, other, point)`.
pub const EE_COMBAT_W_LAUNCH_SHOT: i32 = 30;
pub const EE_COMBAT_W_LAUNCH_FROM_TURRET: i32 = 31;
/// the class's `mSpecialAttackEffect` between `object` and `other`.
pub const EE_COMBAT_W_SPECIAL_EFFECT: i32 = 32;
/// `object->StartAction(args[0])`.
pub const EE_COMBAT_W_START_ACTION: i32 = 33;
/// the class's `mMuzzleFlashEffect` on `object`.
pub const EE_COMBAT_W_MUZZLE_FLASH: i32 = 34;
/// the class's `mShotEffect` between `object` and `other`, or at the point.
pub const EE_COMBAT_W_SHOT_EFFECT_BETWEEN: i32 = 35;
pub const EE_COMBAT_W_SHOT_EFFECT_AT: i32 = 36;
/// `object->SetAmmunition(args[0])`, `SetFlightTime(args[0])`, `mIsSpecialAttack = false`.
pub const EE_COMBAT_W_SET_AMMUNITION: i32 = 37;
pub const EE_COMBAT_W_SET_FLIGHT_TIME: i32 = 38;
pub const EE_COMBAT_W_SPECIAL_OFF: i32 = 39;

/// The argument block of an operation.
pub type EeCombatWorldArgs = [i64; 12];

// the original's constants (ee/EEUnitClassIDs.h, ee/EEUnitAction.h, ee/EEUnitGoal.h)
const W_TYPE_ROMAN_LEGIONNAIRE: u32 = 0x2dc;
const W_TYPE_SP15_CAPITAL_SHIP: u32 = 0x2da;
const W_TYPE_CAPITAL_SHIP_YAMATO: u32 = 0x33c;
const W_TYPE_CP_MISSILE_BASE: u32 = 0x2ed;
const W_CATEGORY_LAND_CITIZEN: u32 = 6;
const W_CATEGORY_NAVAL_CITIZEN: u32 = 0x16;
const W_MOVEMENT_LAND_OR_SEA: u32 = 6;
const W_GOAL_ATTACK_UNIT: u32 = 2;
const W_ACTION_IDLE: i64 = 5000;
const W_OFF_MAP: i32 = -10000;
const W_IGNORE_TIME: u32 = 30000;
const W_PATH_LIMIT: i32 = 0x23;

/// The world of the bodies here: the reads plus the operations (`EeCombatWorldHost` over the C++, a mock in the tests).
pub trait WorldHost: QueryHost {
    /// `EE_COMBAT_W_*` on `object` and `other` with `args` (read and written).
    fn op(&mut self, code: i32, object: EeCombatHandle, other: EeCombatHandle, args: &mut EeCombatWorldArgs) -> u64;
}

impl<H: WorldHost> WorldHost for Stored<'_, H> {
    fn op(&mut self, code: i32, object: EeCombatHandle, other: EeCombatHandle, args: &mut EeCombatWorldArgs) -> u64 {
        self.host.op(code, object, other, args)
    }
}

fn read<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle) -> u32 {
    h.read(code, object, 0)
}

fn int<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle) -> i32 {
    h.read(code, object, 0) as i32
}

fn real<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle) -> f32 {
    f32::from_bits(h.read(code, object, 0))
}

fn flag<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle) -> bool {
    h.read(code, object, 0) != 0
}

/// An operation without arguments.
fn op<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle, other: EeCombatHandle) -> u64 {
    h.op(code, object, other, &mut [0; 12])
}

/// An operation with one argument.
fn op1<H: WorldHost>(h: &mut H, code: i32, object: EeCombatHandle, arg: i64) -> u64 {
    let mut args = [0; 12];
    args[0] = arg;
    h.op(code, object, 0, &mut args)
}

fn tile<H: WorldHost>(h: &mut H, object: EeCombatHandle) -> (i32, i32) {
    (int(h, EE_COMBAT_Q_TILE_X, object), int(h, EE_COMBAT_Q_TILE_Y, object))
}

// ---- the ignored targets

/// One entry of a unit's `mIgnoredTargets` (an `EEComplexUnitAttacker`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Ignored {
    /// the ignoring unit's tile, the target's ("from") tile
    pub tile: (i32, i32),
    pub from: (i32, i32),
    pub unit_id: u32,
    /// the game time the entry runs out
    pub time: u32,
}

fn ignored_entry<H: WorldHost>(h: &mut H, unit: EeCombatHandle, index: u32) -> Ignored {
    let mut a = [0; 12];
    a[0] = i64::from(index);
    h.op(EE_COMBAT_W_IGNORED_ENTRY, unit, 0, &mut a);
    Ignored {
        tile: (a[0] as i32, a[1] as i32),
        from: (a[2] as i32, a[3] as i32),
        unit_id: a[4] as u32,
        time: a[5] as u32,
    }
}

/// `std::find` of the target's id in the unit's list (the first entry from the front).
fn find_ignored<H: WorldHost>(h: &mut H, unit: EeCombatHandle, id: u32) -> Option<(u32, Ignored)> {
    let count = read(h, EE_COMBAT_Q_IGNORED_COUNT, unit);
    (0..count).map(|i| (i, ignored_entry(h, unit, i))).find(|(_, e)| e.unit_id == id)
}

fn erase_ignored<H: WorldHost>(h: &mut H, unit: EeCombatHandle, from: u32, to: Option<u32>) {
    let mut a = [0; 12];
    a[0] = i64::from(from);
    a[1] = to.map_or(-1, i64::from);
    h.op(EE_COMBAT_W_IGNORED_ERASE, unit, 0, &mut a);
}

/// `EEComplexUnit::IsTargetIgnored(target, check_location)`: whether the unit ignores the target (with
/// `check_location`, only from the tiles both stood on); an expired entry is dropped with every older one behind it,
/// one from other tiles alone.
pub fn is_target_ignored<H: WorldHost>(
    h: &mut H,
    unit: EeCombatHandle,
    target: EeCombatHandle,
    check_location: bool,
) -> bool {
    let id = read(h, EE_COMBAT_Q_ID, target);
    let Some((index, entry)) = find_ignored(h, unit, id) else {
        return false;
    };
    let here = tile(h, unit);
    let from = tile(h, target);
    let now = read(h, EE_COMBAT_Q_GAME_TIME, 0);
    if entry.time <= now {
        erase_ignored(h, unit, index, None);
        return false;
    }
    if !check_location {
        return true;
    }
    if entry.tile == here && entry.from == from {
        return true;
    }
    erase_ignored(h, unit, index, Some(index + 1));
    false
}

/// `EEComplexUnit::IgnoreTarget(target)`: a new entry at the front (both units' tiles, 30 s from now), or the found
/// one's time renewed.
pub fn ignore_target<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) {
    let id = read(h, EE_COMBAT_Q_ID, target);
    match find_ignored(h, unit, id) {
        None => {
            let (x, y) = tile(h, unit);
            let (fx, fy) = tile(h, target);
            let time = read(h, EE_COMBAT_Q_GAME_TIME, 0).wrapping_add(W_IGNORE_TIME);
            let mut a = [0; 12];
            a[..6].copy_from_slice(&[
                i64::from(x),
                i64::from(y),
                i64::from(fx),
                i64::from(fy),
                i64::from(id),
                i64::from(time),
            ]);
            h.op(EE_COMBAT_W_IGNORED_PUSH, unit, 0, &mut a);
        }
        Some((index, _)) => {
            let time = read(h, EE_COMBAT_Q_GAME_TIME, 0).wrapping_add(W_IGNORE_TIME);
            let mut a = [0; 12];
            a[0] = i64::from(index);
            a[1] = i64::from(time);
            h.op(EE_COMBAT_W_IGNORED_TIME, unit, 0, &mut a);
        }
    }
}

// ---- reach

/// `EEComplexUnit::CanReachUnit(target)`: the continent manager's direct test, else the two continents' link flag,
/// else their connection for the target's passable terrain.
pub fn can_reach_unit<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) -> bool {
    if op(h, EE_COMBAT_W_CAN_GET_TO_UNIT, unit, target) != 0 {
        return true;
    }
    let from = op(h, EE_COMBAT_W_CONTINENT_AT, unit, 0);
    let to = op(h, EE_COMBAT_W_CONTINENT_AT, target, 0);
    if from == 0 || to == 0 {
        return false;
    }
    if op(h, EE_COMBAT_W_CONTINENT_LINK, from, to) != 0 {
        return true;
    }
    let mut a = [0; 12];
    a[0] = target as i64;
    h.op(EE_COMBAT_W_CONNECTED_FOR_UNIT, from, to, &mut a) != 0
}

/// `EEComplexUnit::IsOnTileBorderingTarget(target)`: a unit of range 1 stands on the target's tile, on a tile along
/// its sides (`EEGetSideTiles`) or on a corner the walls leave open (`EEGetCornerTiles(0, ...)`).
pub fn on_tile_bordering<H: WorldHost>(h: &mut H, unit: EeCombatHandle, target: EeCombatHandle) -> bool {
    let (x, y) = tile(h, unit);
    let (tx, ty) = tile(h, target);
    let size = int(h, EE_COMBAT_Q_TILE_SIZE, target);
    if real(h, EE_COMBAT_Q_RANGE, unit) != 1.0 {
        return false;
    }
    if x == tx && y == ty {
        return true;
    }
    // EEGetSquareRing(tx, ty, size, size): the left and right columns, then the top and bottom rows
    let bottom = size.wrapping_add(ty);
    let right = tx.wrapping_sub(1).wrapping_add(size);
    let on_side = (ty..bottom).any(|ry| (x == tx.wrapping_sub(1) || x == tx.wrapping_add(size)) && y == ry)
        || (tx..=right).any(|rx| x == rx && (y == ty.wrapping_sub(1) || y == bottom));
    if on_side {
        return true;
    }
    // EEGetRingCorners(0, ...): every corner tested in the original's order (its second test's `inHeight + 1` kept)
    let corner_right = tx.wrapping_add(size);
    let corners = [
        ((tx.wrapping_sub(1), ty, tx, ty.wrapping_sub(1)), (tx.wrapping_sub(1), ty.wrapping_sub(1))),
        ((tx.wrapping_sub(1), bottom.wrapping_sub(1), tx, size.wrapping_add(1)), (tx.wrapping_sub(1), bottom)),
        ((corner_right, ty, corner_right.wrapping_sub(1), ty.wrapping_sub(1)), (corner_right, ty.wrapping_sub(1))),
        ((corner_right.wrapping_sub(1), bottom, corner_right, bottom.wrapping_sub(1)), (corner_right, bottom)),
    ];
    let mut open = [false; 4];
    for (i, ((x1, y1, x2, y2), _)) in corners.iter().enumerate() {
        let mut a = [0; 12];
        a[..4].copy_from_slice(&[i64::from(*x1), i64::from(*y1), i64::from(*x2), i64::from(*y2)]);
        open[i] = h.op(EE_COMBAT_W_CORNER_BLOCKED, 0, 0, &mut a) == 0;
    }
    corners.iter().zip(open).any(|((_, corner), open)| open && *corner == (x, y))
}

/// `EEIsAttackReachable(unit, target, &tile)`: the target is in reach from where the unit stands (a unit that does not
/// move on land or sea always), or a path's first tile (the search bounded by the two tiles grown by twice the
/// unit's sight or range, at most 35 steps) is. `None`: the tile left as it was (the target off the map or on
/// another continent); else the tile written (0, 0 without a search's tile).
pub fn attack_reachable<H: WorldHost>(
    h: &mut H,
    unit: EeCombatHandle,
    target: EeCombatHandle,
) -> (bool, Option<(i32, i32)>) {
    if int(h, EE_COMBAT_Q_TILE_X, target) == W_OFF_MAP || op(h, EE_COMBAT_W_OTHER_CONTINENT, unit, target) != 0 {
        return (false, None);
    }
    let from = tile(h, unit);
    let mut to = tile(h, target);
    let range = real(h, EE_COMBAT_Q_RANGE, unit);
    let mut range = range * range;
    if read(h, EE_COMBAT_Q_UNIT_TYPE, unit) == W_TYPE_ROMAN_LEGIONNAIRE && flag(h, EE_COMBAT_Q_SPECIAL_ATTACK, unit) {
        let heal = real(h, EE_COMBAT_Q_HEAL_RANGE, unit);
        range = heal * heal;
    }
    if int(h, EE_COMBAT_Q_TILE_SIZE, target) > 1 {
        to = h.footprint_tile(target);
    }
    if range < 2.0 {
        range = 2.0;
    }
    let mut out = (0, 0);
    let mut result = false;
    if real(h, EE_COMBAT_Q_CLASS_MAX_VELOCITY, unit) == 0.0
        || read(h, EE_COMBAT_Q_DOMAINS, unit) & W_MOVEMENT_LAND_OR_SEA == 0
        || real(h, EE_COMBAT_Q_DISTANCE_SQUARED, target) <= range
    {
        result = true;
    } else if !flag(h, EE_COMBAT_Q_CONTAINER, unit) && !flag(h, EE_COMBAT_Q_CONTAINER, target) {
        // the bounds: Set(from), Extend(to)
        let (mut left, mut right) = (from.0.min(to.0), from.0.max(to.0));
        let (mut top, mut bottom) = (from.1.min(to.1), from.1.max(to.1));
        let radius = if real(h, EE_COMBAT_Q_LINE_OF_SIGHT, unit) > range {
            ceil(real(h, EE_COMBAT_Q_LINE_OF_SIGHT, unit))
        } else {
            ceil(range)
        };
        let margin = radius.wrapping_mul(2);
        top = top.wrapping_sub(margin);
        if top <= 0 {
            top = 0;
        }
        left = left.wrapping_sub(margin);
        if left <= 0 {
            left = 0;
        }
        let last = int(h, EE_COMBAT_Q_MAP_SIZE, 0).wrapping_sub(1);
        bottom = bottom.wrapping_add(margin).min(last);
        right = right.wrapping_add(margin).min(last);
        let mut limit = distance_squared(from, to);
        if margin > limit {
            limit = margin;
        }
        limit = limit.wrapping_mul(2);
        if limit > W_PATH_LIMIT {
            limit = W_PATH_LIMIT;
        }
        let mut a = [0; 12];
        for (slot, value) in [from.0, from.1, to.0, to.1, top, left, bottom, right, limit].into_iter().enumerate() {
            a[slot] = i64::from(value);
        }
        if h.op(EE_COMBAT_W_FIND_PATH, unit, target, &mut a) != 0 {
            out = (a[0] as i32, a[1] as i32);
            result = distance_squared(out, to) as f32 <= range;
        }
    }
    (result, Some(out))
}

/// `U2DPoint<long>::DistanceToSquared`.
fn distance_squared(a: (i32, i32), b: (i32, i32)) -> i32 {
    let dx = a.0.wrapping_sub(b.0);
    let dy = a.1.wrapping_sub(b.1);
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

// ---- the attacked unit's reaction

/// `EEUnitClass::Attacked(unit)`: the unit turns on its last attacker (or goes to where it stands, when it is unseen
/// and the unit moves); a unit of a player other than nature's (player 0; a citizen only against another player's
/// attacker) calls the idle soldiers near it that may attack the attacker to the same goal, or makes them ignore it when they cannot
/// reach it. The last attacker is released.
pub fn attacked<H: WorldHost>(h: &mut H, unit: EeCombatHandle) {
    let target = op(h, EE_COMBAT_W_LAST_ATTACKER, unit, 0);
    let location =
        op(h, EE_COMBAT_W_SEEN_BY_OWNER, target, unit) == 0 && real(h, EE_COMBAT_Q_STATS_MAX_VELOCITY, unit) != 0.0;
    let mut a = [0; 12];
    a[0] = i64::from(location);
    h.op(EE_COMBAT_W_ATTACKED_GOAL, unit, target, &mut a);
    let player = h.handle(EE_COMBAT_H_PLAYER, unit, 0);
    if read(h, EE_COMBAT_Q_PLAYER_ID, player) != 0 && {
        let category = read(h, EE_COMBAT_Q_CATEGORY, unit);
        (category != W_CATEGORY_LAND_CITIZEN && category != W_CATEGORY_NAVAL_CITIZEN) || {
            let other = h.handle(EE_COMBAT_H_PLAYER, target, 0);
            read(h, EE_COMBAT_Q_PLAYER_ID, other) != 0
        }
    } {
        let count = op(h, EE_COMBAT_W_UNITS_NEAR, unit, 0) as i32;
        for i in (0..count).rev() {
            let near = op1(h, EE_COMBAT_W_NEAR_UNIT, 0, i64::from(i));
            if !helps(h, near, player, target) {
                continue;
            }
            if op(h, EE_COMBAT_W_ATTACK_REACHABLE, near, target) != 0 {
                op(h, EE_COMBAT_W_HELP, near, 0);
            } else {
                op(h, EE_COMBAT_W_IGNORE, near, target);
            }
        }
    }
    op(h, EE_COMBAT_W_RELEASE_ATTACKER, unit, 0);
}

/// `Attacked`'s screen of a unit near: alive, the same player's, free to target enemies and move, no projectile or
/// land citizen, able to attack the attacker and not ignoring it, idle or attacking an unarmed land building.
fn helps<H: WorldHost>(h: &mut H, near: EeCombatHandle, player: EeCombatHandle, target: EeCombatHandle) -> bool {
    if int(h, EE_COMBAT_Q_HIT_POINTS, near) <= 0
        || h.handle(EE_COMBAT_H_PLAYER, near, 0) != player
        || !flag(h, EE_COMBAT_Q_TARGETS_ENEMIES, near)
        || !flag(h, EE_COMBAT_Q_MOVES_FREELY, near)
        || read(h, EE_COMBAT_Q_CLASS_FLAGS, near) & EE_COMBAT_CLASS_PROJECTILE != 0
        || read(h, EE_COMBAT_Q_CATEGORY, near) == W_CATEGORY_LAND_CITIZEN
        || op(h, EE_COMBAT_W_CAN_ATTACK_CLASS, near, target) == 0
        || op(h, EE_COMBAT_W_IS_IGNORED, near, target) != 0
    {
        return false;
    }
    let kind = read(h, EE_COMBAT_Q_GOAL_KIND, near);
    if kind == EE_COMBAT_Q_NONE {
        return true;
    }
    if flag(h, EE_COMBAT_Q_GOAL_B1C, near) || kind != W_GOAL_ATTACK_UNIT {
        return false;
    }
    let goal_target = h.handle(EE_COMBAT_H_GOAL_TARGET, near, 0);
    if !flag(h, EE_COMBAT_Q_LAND_BUILDING, goal_target) {
        return false;
    }
    let goal_target = h.handle(EE_COMBAT_H_GOAL_TARGET, near, 0);
    real(h, EE_COMBAT_Q_ATTACK, goal_target) == 0.0
}

// ---- the shot

fn capital(unit_type: u32) -> bool {
    unit_type == W_TYPE_SP15_CAPITAL_SHIP || unit_type == W_TYPE_CAPITAL_SHIP_YAMATO
}

/// `EEUnitClass::FireShot(unit)` (the class: the host's): one shot of the unit's attack: the reload timer drawn (half
/// again when firing high over walls), the target and drop point found; a direct shot hits the target or what blocks
/// the line to it (a friendly blocker only sparks, open ground sparks), a projectile is launched (from the turrets
/// when the model has them), a capital ship's special attack hits from its last tag point; then the muzzle flash and
/// shot effects, the ammunition and the next weapon tag point.
pub fn fire_shot<H: WorldHost>(h: &mut H, unit: EeCombatHandle) {
    let count = read(h, EE_COMBAT_Q_TAG_COUNT, unit);
    let mut done = false;
    if read(h, EE_COMBAT_Q_UNIT_TYPE, unit) == W_TYPE_CP_MISSILE_BASE {
        op(h, EE_COMBAT_W_LAUNCH_STORED_MISSILE, unit, 0);
        return;
    }
    if int(h, EE_COMBAT_Q_RELOAD_TIMER, unit) == 0 {
        let time = int(h, EE_COMBAT_Q_RELOAD_TIME, unit);
        op1(h, EE_COMBAT_W_RELOAD, unit, i64::from(time));
    }
    if flag(h, EE_COMBAT_Q_FIRE_OVER_WALLS, unit) && flag(h, EE_COMBAT_Q_FIRING_HIGH, unit) {
        // `UMFloatToLong(mReloadTimer * 1.5f)`
        let time = lrintf(int(h, EE_COMBAT_Q_RELOAD_TIMER, unit) as f32 * 1.5);
        op1(h, EE_COMBAT_W_RELOAD, unit, i64::from(time));
    }
    let mut target = op(h, EE_COMBAT_W_FIND_DROP, unit, 0);
    let class = read(h, EE_COMBAT_Q_SHOT_CLASS, 0);
    if class & EE_COMBAT_SHOT_CLASS_PROJECTILE == 0 {
        let mut a = [0; 12];
        if real(h, EE_COMBAT_Q_RANGE, unit) > 1.0
            && target != 0
            && !flag(h, EE_COMBAT_Q_FIRE_OVER_WALLS, unit)
            && h.op(EE_COMBAT_W_LINE_BLOCKED, unit, target, &mut a) != 0
        {
            let blocker = a[0] as u64;
            if blocker != 0 {
                target = blocker;
                let other = h.handle(EE_COMBAT_H_PLAYER, blocker, 0);
                let player = h.handle(EE_COMBAT_H_PLAYER, unit, 0);
                let friendly = other == player || {
                    let id = read(h, EE_COMBAT_Q_PLAYER_ID, other);
                    h.read(EE_COMBAT_Q_ENEMY, player, id as i32) == 0
                };
                if friendly {
                    if flag(h, EE_COMBAT_Q_SPARKS_EFFECT, blocker) {
                        op(h, EE_COMBAT_W_SPARKS_ON, blocker, 0);
                    }
                } else {
                    apply_hit(h, unit, target);
                }
            } else {
                let mut spot = [0; 12];
                spot[..2].copy_from_slice(&a[1..3]);
                h.op(EE_COMBAT_W_SPARKS_AT, 0, 0, &mut spot);
            }
        } else {
            apply_hit(h, unit, target);
        }
    } else {
        let unit_type = read(h, EE_COMBAT_Q_UNIT_TYPE, unit);
        if unit_type != W_TYPE_ROMAN_LEGIONNAIRE && !capital(unit_type) {
            if read(h, EE_COMBAT_Q_TURRET_PARTS, unit) == 0 {
                op(h, EE_COMBAT_W_LAUNCH_SHOT, unit, target);
            } else {
                op(h, EE_COMBAT_W_LAUNCH_FROM_TURRET, unit, target);
            }
        } else if flag(h, EE_COMBAT_Q_SPECIAL_ATTACK, unit) {
            if capital(unit_type) {
                // `mTagPoints[GetAnimation()]->mPoints.size() - 1`
                let last = read(h, EE_COMBAT_Q_TAG_COUNT, unit).wrapping_sub(1);
                if read(h, EE_COMBAT_Q_WEAPON_TAG, unit) != last {
                    h.act(EE_COMBAT_A_WEAPON_TAG, unit, last as i32);
                }
                op(h, EE_COMBAT_W_SPECIAL_EFFECT, unit, target);
                apply_hit(h, unit, target);
                if op(h, EE_COMBAT_W_CAN_ATTACK_CLASS, unit, target) == 0 {
                    op1(h, EE_COMBAT_W_START_ACTION, unit, W_ACTION_IDLE);
                }
            } else {
                op(h, EE_COMBAT_W_LAUNCH_SHOT, unit, target);
            }
            done = true;
        } else {
            apply_hit(h, unit, target);
        }
    }
    if count != 0 {
        if class & EE_COMBAT_SHOT_CLASS_MUZZLE_FLASH != 0 {
            op(h, EE_COMBAT_W_MUZZLE_FLASH, unit, 0);
        }
        if read(h, EE_COMBAT_Q_CATEGORY, unit) == W_CATEGORY_LAND_CITIZEN && !flag(h, EE_COMBAT_Q_HIGH_TECH_TOOLS, unit)
        {
            done = true;
        }
        if class & EE_COMBAT_SHOT_CLASS_SHOT_EFFECT != 0 && !done {
            if target != 0 {
                op(h, EE_COMBAT_W_SHOT_EFFECT_BETWEEN, unit, target);
            } else {
                op(h, EE_COMBAT_W_SHOT_EFFECT_AT, unit, 0);
            }
        }
    }
    if class & EE_COMBAT_SHOT_CLASS_AMMUNITION != 0 {
        let ammunition = read(h, EE_COMBAT_Q_AMMUNITION, unit);
        if ammunition > 1 {
            op1(h, EE_COMBAT_W_SET_AMMUNITION, unit, i64::from(ammunition - 1));
        } else {
            op1(h, EE_COMBAT_W_SET_AMMUNITION, unit, 0);
            op1(h, EE_COMBAT_W_SET_FLIGHT_TIME, unit, 0);
        }
    }
    if flag(h, EE_COMBAT_Q_SPECIAL_ATTACK, unit) && capital(read(h, EE_COMBAT_Q_UNIT_TYPE, unit)) {
        op(h, EE_COMBAT_W_SPECIAL_OFF, unit, 0);
    }
    let mut next = 0;
    if count != 0 {
        next = read(h, EE_COMBAT_Q_WEAPON_TAG, unit).wrapping_add(1) % count;
        if capital(read(h, EE_COMBAT_Q_UNIT_TYPE, unit)) && next == count - 1 {
            next = 0;
        }
    }
    if read(h, EE_COMBAT_Q_WEAPON_TAG, unit) != next {
        h.act(EE_COMBAT_A_WEAPON_TAG, unit, next as i32);
    }
}

// ---- the C ABI

/// The C++ host of the bodies here: the reads' table and the operations (`ctx`: the reads' context).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatWorldHost {
    pub query: EeCombatQueryHost,
    pub op:
        unsafe extern "C" fn(ctx: *mut c_void, code: i32, object: u64, other: u64, args: *mut EeCombatWorldArgs) -> u64,
}

impl QueryHost for EeCombatWorldHost {
    fn read(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> u32 {
        self.query.read(code, object, arg)
    }
    fn handle(&mut self, code: i32, object: EeCombatHandle, arg: i32) -> EeCombatHandle {
        self.query.handle(code, object, arg)
    }
    fn act(&mut self, code: i32, object: EeCombatHandle, arg: i32) {
        self.query.act(code, object, arg)
    }
    fn location(&mut self, object: EeCombatHandle) -> ee_ge::types::GeTransformation {
        self.query.location(object)
    }
    fn intersects(
        &mut self,
        candidate: EeCombatHandle,
        line: &ee_ge::types::Ge3DLine,
        segment: bool,
        lod: u32,
        scale: f32,
    ) -> bool {
        self.query.intersects(candidate, line, segment, lod, scale)
    }
    fn footprint_tile(&mut self, object: EeCombatHandle) -> (i32, i32) {
        self.query.footprint_tile(object)
    }
}

impl WorldHost for EeCombatWorldHost {
    fn op(&mut self, code: i32, object: EeCombatHandle, other: EeCombatHandle, args: &mut EeCombatWorldArgs) -> u64 {
        // SAFETY: the table's contract: `ctx` is live for the call that handed it over
        unsafe { (self.op)(self.query.ctx, code, object, other, args) }
    }
}

/// The functions of [`ee_combat_world`].
pub const EE_COMBAT_WORLD_IS_IGNORED: i32 = 1;
pub const EE_COMBAT_WORLD_IS_IGNORED_ANYWHERE: i32 = 2;
pub const EE_COMBAT_WORLD_IGNORE: i32 = 3;
pub const EE_COMBAT_WORLD_CAN_REACH_UNIT: i32 = 4;
pub const EE_COMBAT_WORLD_BORDERING: i32 = 5;
pub const EE_COMBAT_WORLD_ATTACK_REACHABLE: i32 = 6;
pub const EE_COMBAT_WORLD_ATTACKED: i32 = 7;
pub const EE_COMBAT_WORLD_FIRE_SHOT: i32 = 8;

/// One body of this module (`EE_COMBAT_WORLD_*`) of `unit` on `target` over `host`: the answer (0/1); the attack
/// reach writes its tile into `tile` when it sets one.
///
/// # Safety
/// `host` must point to a valid table whose callbacks may be called with its context, `tile` to two writable values.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_world(
    function: i32,
    unit: u64,
    target: u64,
    host: *const EeCombatWorldHost,
    tile: *mut [i32; 2],
) -> i32 {
    let mut world = unsafe { *host };
    let h = &mut Stored::new(&mut world);
    match function {
        EE_COMBAT_WORLD_IS_IGNORED => i32::from(is_target_ignored(h, unit, target, true)),
        EE_COMBAT_WORLD_IS_IGNORED_ANYWHERE => i32::from(is_target_ignored(h, unit, target, false)),
        EE_COMBAT_WORLD_IGNORE => {
            ignore_target(h, unit, target);
            0
        }
        EE_COMBAT_WORLD_CAN_REACH_UNIT => i32::from(can_reach_unit(h, unit, target)),
        EE_COMBAT_WORLD_BORDERING => i32::from(on_tile_bordering(h, unit, target)),
        EE_COMBAT_WORLD_ATTACK_REACHABLE => {
            let (result, out) = attack_reachable(h, unit, target);
            if let Some((x, y)) = out {
                // SAFETY: the function's contract
                unsafe { *tile = [x, y] };
            }
            i32::from(result)
        }
        EE_COMBAT_WORLD_ATTACKED => {
            attacked(h, unit);
            0
        }
        EE_COMBAT_WORLD_FIRE_SHOT => {
            fire_shot(h, unit);
            0
        }
        _ => 0,
    }
}

#[cfg(test)]
#[path = "world_tests.rs"]
mod tests;
