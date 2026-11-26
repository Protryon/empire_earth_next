//! The combat world host's compound operations split into the original's statements (the units' lane, round 8; were
//! EECombatGoals.cpp `EEWorldOp`'s multi-statement cases): the ignored targets' entries, the attacked unit's goal and
//! its helpers, the last attacker's release, the line of fire's blocker, the damage record's tables and rules, the
//! hit's classes, the unit effect, the path start, the wall spot, the firing ring, the footprint tiles, the tile
//! visibility, the goal location and the drop model. `EeCombatWorldHost::op` asks [`split`] first; each C++
//! statement is one `EE_COMBAT_W_*` case of `EEWorldOp` (from 100), asked in the original's order; the choices and
//! the marshalling are here. The Rust the cases called (the unit utilities' spots, rings, picks, units near) is
//! called directly.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::combat::damage::{EE_COMBAT_W_DAMAGE_IN, EE_COMBAT_W_HIT_CLASS, EE_COMBAT_W_UNIT_EFFECT};
use crate::combat::leaves::{
    EE_COMBAT_W_DROP_MODEL, EE_COMBAT_W_FIRING_RING, EE_COMBAT_W_FOOTPRINT_TILE, EE_COMBAT_W_GOAL_LOCATION,
    EE_COMBAT_W_PATH_START, EE_COMBAT_W_TILE_SEEN, EE_COMBAT_W_WALL_SPOT,
};
use crate::combat::world::{
    EeCombatWorldArgs, EE_COMBAT_W_ATTACKED_GOAL, EE_COMBAT_W_HELP, EE_COMBAT_W_IGNORED_ENTRY,
    EE_COMBAT_W_IGNORED_ERASE, EE_COMBAT_W_IGNORED_PUSH, EE_COMBAT_W_LINE_BLOCKED, EE_COMBAT_W_RELEASE_ATTACKER,
    EE_COMBAT_W_UNITS_NEAR,
};
use crate::combat::{EeCombatDamageIn, DAMAGE_DIRECT};
use crate::units::uu_tiles::Tile;

// ---- the statements (`EEWorldOp`'s one-statement cases; `a` the arguments)
/// `object->mIgnoredTargets[a[0]].mTileX`
pub const EE_COMBAT_W_IGN_TILE_X: i32 = 100;
/// `.mTileY` of entry `a[0]`
pub const EE_COMBAT_W_IGN_TILE_Y: i32 = 101;
/// `.mFromX` of entry `a[0]`
pub const EE_COMBAT_W_IGN_FROM_X: i32 = 102;
/// `.mFromY` of entry `a[0]`
pub const EE_COMBAT_W_IGN_FROM_Y: i32 = 103;
/// `.mUnitID` of entry `a[0]`
pub const EE_COMBAT_W_IGN_UNIT_ID: i32 = 104;
/// `.mTime` of entry `a[0]`
pub const EE_COMBAT_W_IGN_TIME: i32 = 105;
/// `object->mIgnoredTargets.erase(begin() + a[0], end())`
pub const EE_COMBAT_W_IGN_ERASE_TO_END: i32 = 106;
/// `object->mIgnoredTargets.erase(begin() + a[0], begin() + a[1])`
pub const EE_COMBAT_W_IGN_ERASE_RANGE: i32 = 107;
/// `object->mIgnoredTargets.push_front(EEComplexUnitAttacker())`
pub const EE_COMBAT_W_IGN_PUSH_FRONT: i32 = 108;
/// `object->mIgnoredTargets[0].mTileX = a[0]`
pub const EE_COMBAT_W_IGN_SET_TILE_X: i32 = 109;
/// `[0].mTileY = a[0]`
pub const EE_COMBAT_W_IGN_SET_TILE_Y: i32 = 110;
/// `[0].mFromX = a[0]`
pub const EE_COMBAT_W_IGN_SET_FROM_X: i32 = 111;
/// `[0].mFromY = a[0]`
pub const EE_COMBAT_W_IGN_SET_FROM_Y: i32 = 112;
/// `[0].mUnitID = a[0]`
pub const EE_COMBAT_W_IGN_SET_UNIT_ID: i32 = 113;
/// `[0].mTime = a[0]`
pub const EE_COMBAT_W_IGN_SET_TIME: i32 = 114;
/// `mAttackedGoal = EEUGRust::NewLocation(other's x, y, 0, false, false, false, 0, false, 0)`
pub const EE_COMBAT_W_AG_NEW_LOCATION: i32 = 115;
/// `mAttackedGoal = EEUGRust::NewAttackUnit(object, other, false, false)`
pub const EE_COMBAT_W_AG_NEW_ATTACK: i32 = 116;
/// `object->SetGoal(mAttackedGoal, 1, 0)`
pub const EE_COMBAT_W_AG_SET_GOAL: i32 = 117;
/// the context's `&mNear` (Attacked's units near)
pub const EE_COMBAT_W_NEAR_LIST: i32 = 118;
/// `mHelpGoal = mAttackedGoal->Clone()`
pub const EE_COMBAT_W_HELP_CLONE: i32 = 119;
/// `object->GetClass()->FindAction(mHelpGoal->GetStartAction()) != NULL`
pub const EE_COMBAT_W_HELP_HAS_ACTION: i32 = 120;
/// `object->SetGoal(mHelpGoal, 1, 0)`
pub const EE_COMBAT_W_HELP_SET_GOAL: i32 = 121;
/// `object->StartAction(mHelpGoal->GetStartAction())`
pub const EE_COMBAT_W_HELP_START_ACTION: i32 = 122;
/// `object->GetUnitClass()->ActionChanged(object)`
pub const EE_COMBAT_W_ACTION_CHANGED: i32 = 123;
/// `object->mLastAttacker != NULL`
pub const EE_COMBAT_W_HAS_LAST_ATTACKER: i32 = 124;
/// `object->mLastAttacker->Dereference()`
pub const EE_COMBAT_W_DEREFERENCE_LAST_ATTACKER: i32 = 125;
/// `object->mLastAttacker = NULL`
pub const EE_COMBAT_W_CLEAR_LAST_ATTACKER: i32 = 126;
/// `EEIsLineToUnitBlocked(object, other, a[0] (EEUnit**), a[1] (long*), a[2] (long*))`
pub const EE_COMBAT_W_LINE_BLOCKED_AT: i32 = 127;
/// the bits of `gToHitTable.GetAttackFactor(a[0], other's class, object's player id, other's player id)`
pub const EE_COMBAT_W_DI_ATTACK_FACTOR: i32 = 128;
/// the bits of `gWeaponToHitFactors[other's class's mWeapon][a[0]]`
pub const EE_COMBAT_W_DI_WEAPON_FACTOR: i32 = 129;
/// combat.yaml's `mMinimumDamage`
pub const EE_COMBAT_W_RULE_MINIMUM_DAMAGE: i32 = 130;
/// the bits of `EERuleFloat(mFanaticFactor)`
pub const EE_COMBAT_W_RULE_FANATIC_FACTOR: i32 = 131;
/// the bits of `mElevationStep`
pub const EE_COMBAT_W_RULE_ELEVATION_STEP: i32 = 132;
/// the bits of `mAllyFactor`
pub const EE_COMBAT_W_RULE_ALLY_FACTOR: i32 = 133;
/// the bits of `mHerdAllyFactor`
pub const EE_COMBAT_W_RULE_HERD_ALLY_FACTOR: i32 = 134;
/// `mTrampleDamage`
pub const EE_COMBAT_W_RULE_TRAMPLE_DAMAGE: i32 = 135;
/// the bits of `mTrampleThreshold`
pub const EE_COMBAT_W_RULE_TRAMPLE_THRESHOLD: i32 = 136;
/// the bits of `mDeadFoodShare`
pub const EE_COMBAT_W_RULE_DEAD_FOOD_SHARE: i32 = 137;
/// `object->mUnitClass->mCategory`
pub const EE_COMBAT_W_HC_CATEGORY: i32 = 138;
/// `object->mIsSpecialAttack`
pub const EE_COMBAT_W_HC_SPECIAL_ATTACK: i32 = 139;
/// `object->mLauncherClass`
pub const EE_COMBAT_W_HC_LAUNCHER: i32 = 140;
/// `gUnitClassTable[kUnitTypeCapitalShipSpecialWeapon]`
pub const EE_COMBAT_W_HC_SPECIAL_WEAPON: i32 = 141;
/// `object->mPlayer->GetUpgrade(class a[0]'s mUnitType)`
pub const EE_COMBAT_W_HC_UPGRADE: i32 = 142;
/// the bits of upgrade `a[0]`'s `GetAttack()`
pub const EE_COMBAT_W_HC_UPGRADE_ATTACK: i32 = 143;
/// the bits of upgrade `a[0]`'s `mAreaEffect`
pub const EE_COMBAT_W_HC_UPGRADE_AREA: i32 = 144;
/// the bits of class `a[0]`'s `mAttack`
pub const EE_COMBAT_W_HC_CLASS_ATTACK: i32 = 145;
/// the bits of class `a[0]`'s `mAreaEffect`
pub const EE_COMBAT_W_HC_CLASS_AREA: i32 = 146;
/// class `a[0]`'s `mAmmoType`
pub const EE_COMBAT_W_HC_CLASS_AMMO: i32 = 147;
/// class `a[0]`'s `mWeapon`
pub const EE_COMBAT_W_HC_CLASS_WEAPON: i32 = 148;
/// class `a[0]`'s `mCategory`
pub const EE_COMBAT_W_HC_CLASS_CATEGORY: i32 = 149;
/// the bits of class `a[0]`'s `mFriendlyDamageFactor`
pub const EE_COMBAT_W_HC_CLASS_FRIENDLY: i32 = 150;
/// the host's class's `mShotEffect != NULL`
pub const EE_COMBAT_W_HC_SHOT_EFFECT: i32 = 151;
/// `mEffect = NULL`
pub const EE_COMBAT_W_UE_CLEAR: i32 = 152;
/// `object->GetEffect(3, mEffect)`
pub const EE_COMBAT_W_UE_GET: i32 = 153;
/// `mEffect != NULL`
pub const EE_COMBAT_W_UE_HAS: i32 = 154;
/// `gEffectManager->CreateUnitEffect(object, mEffect, NULL, false, 0, 0)`
pub const EE_COMBAT_W_UE_CREATE: i32 = 155;
/// `object->GetPathStart(the point at a[0], the float at a[1])`
pub const EE_COMBAT_W_PATH_START_AT: i32 = 156;
/// unit `other`'s `GetTileX()`
pub const EE_COMBAT_W_RING_TILE_X: i32 = 157;
/// unit `other`'s `GetTileY()`
pub const EE_COMBAT_W_RING_TILE_Y: i32 = 158;
/// unit `other`'s `GetUnitClass()->GetTileSize()`
pub const EE_COMBAT_W_RING_TILE_SIZE: i32 = 159;
/// `mFootprint[a[0]].mX`
pub const EE_COMBAT_W_FOOTPRINT_X: i32 = 160;
/// `mFootprint[a[0]].mY`
pub const EE_COMBAT_W_FOOTPRINT_Y: i32 = 161;
/// `::gWorld.mMap->GetTileAtLocation(EETileLocation(a[0], a[1]))`
pub const EE_COMBAT_W_TS_TILE_AT_LOCATION: i32 = 162;
/// `::gWorld.mMap->GetTile(a[0], a[1])`
pub const EE_COMBAT_W_TS_TILE: i32 = 163;
/// tile `a[0]`'s `IsSeenBy(player object)`
pub const EE_COMBAT_W_TS_SEEN: i32 = 164;
/// tile `a[0]`'s `GetVisibility(player object's GetPlayerID()) > 1`
pub const EE_COMBAT_W_TS_VISIBLE: i32 = 165;
/// `object->mActiveGoal->GetGoalLocation(the point at a[0])`
pub const EE_COMBAT_W_GOAL_LOCATION_AT: i32 = 166;
/// unit `object`'s `GetModel()->mModel != NULL`
pub const EE_COMBAT_W_DM_HAS_MODEL: i32 = 167;
/// the point at `a[0]` = unit `object`'s `GetModel()->mDefaultPoint`
pub const EE_COMBAT_W_DM_DEFAULT_POINT: i32 = 168;

/// `Attacked`'s call for help radius (0x447aec)
const CALL_FOR_HELP_RADIUS: f32 = 5.0;
/// `kUnitCategorySpaceship`
const CATEGORY_SPACESHIP: i64 = 0x1f;

/// The operations the split asks: the C++ statements and the Rust the cases called.
pub trait CgWorldHost {
    /// one statement (`EE_COMBAT_W_*` from 100) on `object`, `other` with `a`
    fn w(&mut self, code: i32, object: u64, other: u64, a: &mut EeCombatWorldArgs) -> u64;
    /// the unit's location (x, y, z of its `mPhysicalModel.GetLocation()`)
    fn position(&mut self, object: u64) -> [f32; 3];
    /// `ee_units_uu_units_near(center, radius, list)`: the number found
    fn units_near(&mut self, center: [f32; 3], radius: f32, list: u64) -> i32;
    /// `ee_units_uu_spot(which, unit, target, io)`
    fn spot(&mut self, which: i32, unit: u64, target: u64, io: &mut [f32; 3]) -> bool;
    /// `EEGetSortedRingTiles(from, x, y, size, range, corners)` (noted)
    fn sorted_ring_tiles(&mut self, from: [f32; 3], x: i32, y: i32, size: i32, range: i32, corners: i32) -> Vec<Tile>;
    /// `EEPickTileNear(tiles, unit, container, ignore, io)` (noted)
    fn pick_tile_near(&mut self, tiles: &[Tile], unit: u64, container: u64, ignore: bool, io: &mut [f32; 3]) -> bool;
}

fn op<H: CgWorldHost + ?Sized>(h: &mut H, code: i32, object: u64, other: u64, arg0: i64, arg1: i64) -> u64 {
    let mut a: EeCombatWorldArgs = [0; 12];
    a[0] = arg0;
    a[1] = arg1;
    h.w(code, object, other, &mut a)
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

fn from_bits(a: i64) -> f32 {
    f32::from_bits(a as u32)
}

fn point_args(a: &mut [i64], p: [f32; 3]) {
    a[0] = bits(p[0]);
    a[1] = bits(p[1]);
    a[2] = bits(p[2]);
}

/// `code` on `object`, `other` and `a` as `EEWorldOp`'s compound case did it; None for any other code.
pub fn split<H: CgWorldHost + ?Sized>(
    h: &mut H,
    code: i32,
    object: u64,
    other: u64,
    class: u64,
    a: &mut EeCombatWorldArgs,
) -> Option<u64> {
    Some(match code {
        EE_COMBAT_W_IGNORED_ENTRY => {
            let i = a[0];
            for (k, c) in [
                EE_COMBAT_W_IGN_TILE_X,
                EE_COMBAT_W_IGN_TILE_Y,
                EE_COMBAT_W_IGN_FROM_X,
                EE_COMBAT_W_IGN_FROM_Y,
                EE_COMBAT_W_IGN_UNIT_ID,
                EE_COMBAT_W_IGN_TIME,
            ]
            .into_iter()
            .enumerate()
            {
                a[k] = op(h, c, object, 0, i, 0) as i64;
            }
            0
        }
        EE_COMBAT_W_IGNORED_ERASE => {
            if a[1] < 0 {
                op(h, EE_COMBAT_W_IGN_ERASE_TO_END, object, 0, a[0], 0);
            } else {
                op(h, EE_COMBAT_W_IGN_ERASE_RANGE, object, 0, a[0], a[1]);
            }
            0
        }
        EE_COMBAT_W_IGNORED_PUSH => {
            op(h, EE_COMBAT_W_IGN_PUSH_FRONT, object, 0, 0, 0);
            for (k, c) in [
                EE_COMBAT_W_IGN_SET_TILE_X,
                EE_COMBAT_W_IGN_SET_TILE_Y,
                EE_COMBAT_W_IGN_SET_FROM_X,
                EE_COMBAT_W_IGN_SET_FROM_Y,
                EE_COMBAT_W_IGN_SET_UNIT_ID,
                EE_COMBAT_W_IGN_SET_TIME,
            ]
            .into_iter()
            .enumerate()
            {
                op(h, c, object, 0, a[k], 0);
            }
            0
        }
        EE_COMBAT_W_ATTACKED_GOAL => {
            if a[0] != 0 {
                op(h, EE_COMBAT_W_AG_NEW_LOCATION, object, other, 0, 0);
            } else {
                op(h, EE_COMBAT_W_AG_NEW_ATTACK, object, other, 0, 0);
            }
            op(h, EE_COMBAT_W_AG_SET_GOAL, object, other, 0, 0);
            0
        }
        EE_COMBAT_W_UNITS_NEAR => {
            let at = h.position(object);
            let list = op(h, EE_COMBAT_W_NEAR_LIST, object, 0, 0, 0);
            h.units_near(at, CALL_FOR_HELP_RADIUS, list) as u64
        }
        EE_COMBAT_W_HELP => {
            op(h, EE_COMBAT_W_HELP_CLONE, object, 0, 0, 0);
            if op(h, EE_COMBAT_W_HELP_HAS_ACTION, object, 0, 0, 0) != 0 {
                op(h, EE_COMBAT_W_HELP_SET_GOAL, object, 0, 0, 0);
                op(h, EE_COMBAT_W_HELP_START_ACTION, object, 0, 0, 0);
                op(h, EE_COMBAT_W_ACTION_CHANGED, object, 0, 0, 0);
            }
            0
        }
        EE_COMBAT_W_RELEASE_ATTACKER => {
            if op(h, EE_COMBAT_W_HAS_LAST_ATTACKER, object, 0, 0, 0) != 0 {
                op(h, EE_COMBAT_W_DEREFERENCE_LAST_ATTACKER, object, 0, 0, 0);
            }
            op(h, EE_COMBAT_W_CLEAR_LAST_ATTACKER, object, 0, 0, 0);
            0
        }
        EE_COMBAT_W_LINE_BLOCKED => {
            let mut blocker: u64 = 0;
            let (mut x, mut y): (c_long, c_long) = (0, 0);
            let mut b: EeCombatWorldArgs = [0; 12];
            b[0] = &mut blocker as *mut u64 as i64;
            b[1] = &mut x as *mut c_long as i64;
            b[2] = &mut y as *mut c_long as i64;
            let blocked = h.w(EE_COMBAT_W_LINE_BLOCKED_AT, object, other, &mut b);
            a[0] = blocker as i64;
            a[1] = x as i64;
            a[2] = y as i64;
            blocked
        }
        EE_COMBAT_W_DAMAGE_IN => {
            // SAFETY: the caller's record (damage.rs `compute_damage`), live across the call
            let i = unsafe { &mut *(a[0] as usize as *mut EeCombatDamageIn) };
            if i.form != DAMAGE_DIRECT || i.attack > 0.0 {
                i.attack_factor = from_bits(op(h, EE_COMBAT_W_DI_ATTACK_FACTOR, object, other, a[1], 0) as i64);
                i.weapon_factor = from_bits(op(h, EE_COMBAT_W_DI_WEAPON_FACTOR, object, other, a[2], 0) as i64);
            }
            i.minimum_damage = op(h, EE_COMBAT_W_RULE_MINIMUM_DAMAGE, 0, 0, 0, 0) as i32;
            i.fanatic_factor = from_bits(op(h, EE_COMBAT_W_RULE_FANATIC_FACTOR, 0, 0, 0, 0) as i64);
            i.elevation_step = from_bits(op(h, EE_COMBAT_W_RULE_ELEVATION_STEP, 0, 0, 0, 0) as i64);
            i.ally_factor = from_bits(op(h, EE_COMBAT_W_RULE_ALLY_FACTOR, 0, 0, 0, 0) as i64);
            i.herd_ally_factor = from_bits(op(h, EE_COMBAT_W_RULE_HERD_ALLY_FACTOR, 0, 0, 0, 0) as i64);
            i.trample_damage = op(h, EE_COMBAT_W_RULE_TRAMPLE_DAMAGE, 0, 0, 0, 0) as i32;
            i.trample_threshold = from_bits(op(h, EE_COMBAT_W_RULE_TRAMPLE_THRESHOLD, 0, 0, 0, 0) as i64);
            0
        }
        EE_COMBAT_W_HIT_CLASS => {
            // a capital ship's special weapon, else the launcher's or this class; the splash's
            let launcher = op(h, EE_COMBAT_W_HC_LAUNCHER, object, 0, 0, 0);
            let special = op(h, EE_COMBAT_W_HC_CATEGORY, object, 0, 0, 0) as i64 == CATEGORY_SPACESHIP
                && op(h, EE_COMBAT_W_HC_SPECIAL_ATTACK, object, 0, 0, 0) != 0;
            let hit = if special {
                op(h, EE_COMBAT_W_HC_SPECIAL_WEAPON, 0, 0, 0, 0)
            } else if launcher == 0 {
                class
            } else {
                launcher
            };
            let splash = if launcher != 0 { launcher } else { class };
            let upgrade = op(h, EE_COMBAT_W_HC_UPGRADE, object, 0, hit as i64, 0);
            a[0] = if upgrade != 0 {
                op(h, EE_COMBAT_W_HC_UPGRADE_ATTACK, 0, 0, upgrade as i64, 0)
            } else {
                op(h, EE_COMBAT_W_HC_CLASS_ATTACK, 0, 0, hit as i64, 0)
            } as i64;
            a[1] = if upgrade != 0 {
                op(h, EE_COMBAT_W_HC_UPGRADE_AREA, 0, 0, upgrade as i64, 0)
            } else {
                op(h, EE_COMBAT_W_HC_CLASS_AREA, 0, 0, hit as i64, 0)
            } as i64;
            a[2] = op(h, EE_COMBAT_W_HC_CLASS_AMMO, 0, 0, hit as i64, 0) as i64;
            a[3] = op(h, EE_COMBAT_W_HC_CLASS_WEAPON, 0, 0, hit as i64, 0) as i64;
            a[4] = op(h, EE_COMBAT_W_HC_CLASS_CATEGORY, 0, 0, hit as i64, 0) as i64;
            a[5] = op(h, EE_COMBAT_W_HC_CLASS_FRIENDLY, 0, 0, splash as i64, 0) as i64;
            a[6] = op(h, EE_COMBAT_W_HC_CLASS_CATEGORY, 0, 0, splash as i64, 0) as i64;
            a[7] = op(h, EE_COMBAT_W_RULE_DEAD_FOOD_SHARE, 0, 0, 0, 0) as i64;
            a[8] = op(h, EE_COMBAT_W_HC_SHOT_EFFECT, 0, 0, 0, 0) as i64;
            0
        }
        EE_COMBAT_W_UNIT_EFFECT => {
            op(h, EE_COMBAT_W_UE_CLEAR, object, 0, 0, 0);
            if op(h, EE_COMBAT_W_UE_GET, object, 0, 0, 0) == 0 {
                return Some(0);
            }
            if op(h, EE_COMBAT_W_UE_HAS, object, 0, 0, 0) != 0 {
                op(h, EE_COMBAT_W_UE_CREATE, object, 0, 0, 0);
            }
            1
        }
        EE_COMBAT_W_PATH_START => {
            let mut p = [0.0f32; 3];
            let mut index = 0.0f32;
            op(h, EE_COMBAT_W_PATH_START_AT, object, 0, p.as_mut_ptr() as i64, &mut index as *mut f32 as i64);
            point_args(a, p);
            0
        }
        EE_COMBAT_W_WALL_SPOT => {
            let mut p = [from_bits(a[0]), from_bits(a[1]), from_bits(a[2])];
            let found = h.spot(3, object, other, &mut p);
            point_args(a, p);
            u64::from(found)
        }
        EE_COMBAT_W_FIRING_RING => {
            let at = h.position(object);
            let x = op(h, EE_COMBAT_W_RING_TILE_X, 0, other, 0, 0) as c_long as i32;
            let y = op(h, EE_COMBAT_W_RING_TILE_Y, 0, other, 0, 0) as c_long as i32;
            let size = op(h, EE_COMBAT_W_RING_TILE_SIZE, 0, other, 0, 0) as c_long as i32;
            let range = a[0] as c_long as i32;
            let outer = h.sorted_ring_tiles(at, x, y, size, range, 0);
            let mut tiles = h.sorted_ring_tiles(at, x, y, size, range.wrapping_add(1), 0);
            tiles.extend_from_slice(&outer);
            // `std::sort(..., EEUUTileNearerToUnit(object))`
            let distance = |t: &Tile| crate::units::uu::distance_squared_to_tile(at, *t);
            crate::units::msvc_sort::sort_by(&mut tiles, |p, q| distance(p) < distance(q));
            let mut p = [from_bits(a[1]), from_bits(a[2]), from_bits(a[3])];
            let found = h.pick_tile_near(&tiles, object, other, false, &mut p);
            point_args(&mut a[1..], p);
            u64::from(found)
        }
        EE_COMBAT_W_FOOTPRINT_TILE => {
            let i = a[0];
            a[0] = op(h, EE_COMBAT_W_FOOTPRINT_X, 0, 0, i, 0) as i64;
            a[1] = op(h, EE_COMBAT_W_FOOTPRINT_Y, 0, 0, i, 0) as i64;
            0
        }
        EE_COMBAT_W_TILE_SEEN => {
            let tile = if a[3] != 0 {
                op(h, EE_COMBAT_W_TS_TILE_AT_LOCATION, 0, 0, a[0], a[1])
            } else {
                op(h, EE_COMBAT_W_TS_TILE, 0, 0, a[0], a[1])
            };
            let seen = tile != 0
                && if a[2] != 0 {
                    op(h, EE_COMBAT_W_TS_SEEN, object, 0, tile as i64, 0) != 0
                } else {
                    op(h, EE_COMBAT_W_TS_VISIBLE, object, 0, tile as i64, 0) != 0
                };
            u64::from(seen)
        }
        EE_COMBAT_W_GOAL_LOCATION => {
            let mut p = [0.0f32; 3];
            op(h, EE_COMBAT_W_GOAL_LOCATION_AT, object, 0, p.as_mut_ptr() as i64, 0);
            point_args(a, p);
            0
        }
        EE_COMBAT_W_DROP_MODEL => {
            if op(h, EE_COMBAT_W_DM_HAS_MODEL, object, 0, 0, 0) == 0 {
                return Some(0);
            }
            let mut p = [0.0f32; 3];
            op(h, EE_COMBAT_W_DM_DEFAULT_POINT, object, 0, p.as_mut_ptr() as i64, 0);
            point_args(a, p);
            1
        }
        _ => return None,
    })
}

/// The game's [`CgWorldHost`]: `EEWorldOp`'s statements through the host table, the unit utilities' Rust.
#[cfg(not(test))]
pub struct Game<'h>(pub &'h crate::combat::world::EeCombatWorldHost);

#[cfg(not(test))]
impl CgWorldHost for Game<'_> {
    fn w(&mut self, code: i32, object: u64, other: u64, a: &mut EeCombatWorldArgs) -> u64 {
        if let Some(r) = crate::combat::cg_view::world_plain(code, object, other, a) {
            return r;
        }
        // SAFETY: the table's contract: `ctx` is live for the call that handed it over
        unsafe { (self.0.op)(self.0.query.ctx, code, object, other, a) }
    }
    fn position(&mut self, object: u64) -> [f32; 3] {
        use crate::combat::reads::QueryHost;
        let mut q = self.0.query;
        let m = q.location(object);
        [m.x(), m.y(), m.z()]
    }
    fn units_near(&mut self, center: [f32; 3], radius: f32, list: u64) -> i32 {
        // SAFETY: `list` is the host context's live unit vector
        unsafe { crate::units::uu_ffi::ee_units_uu_units_near(&center, radius, list as usize as *mut std::ffi::c_void) }
    }
    fn spot(&mut self, which: i32, unit: u64, target: u64, io: &mut [f32; 3]) -> bool {
        // SAFETY: live units of the host's call; `io` three floats
        unsafe { crate::units::uu_ffi::ee_units_uu_spot(which, unit, target, io) != 0 }
    }
    fn sorted_ring_tiles(&mut self, from: [f32; 3], x: i32, y: i32, size: i32, range: i32, corners: i32) -> Vec<Tile> {
        crate::units::uu_ffi::sorted_ring_tiles(from, x, y, size, range, corners)
    }
    fn pick_tile_near(&mut self, tiles: &[Tile], unit: u64, container: u64, ignore: bool, io: &mut [f32; 3]) -> bool {
        crate::units::uu_ffi::pick_tile_near(tiles, unit, container, ignore, io)
    }
}

#[cfg(test)]
#[path = "cg_world_tests.rs"]
mod tests;
