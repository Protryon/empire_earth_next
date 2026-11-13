//! The attack-unit goal whole in Rust (R5b.2, the twelfth round; was `EEUGAttackUnit` 0x21d2f4..0x21e0c1): its state a
//! record by goal object (goal_objects.rs's table), its bodies over the goals' host (goal_objects.rs's `GoalHost`): the
//! constructor's and destructor's attacker counts and references, `SetTarget`, `RetargetGatePart`, `ReplaceUnit`,
//! `GetDestination`, `Suits`, the checksum part, `Save` and `Load` after the base goal's, `RepairPointers`. The trigger
//! test and the step are goals.rs's; the C++ formats the debug and out-of-synch texts.
//!
//! Units are handles (the `EEUnit*`'s address, 0 for none). Between `Load` and `RepairPointers` the target and attacker
//! hold the saved ids (`USavedID`: the id in the pointer's place), as the original's members did.

use crate::goal_objects::{with_goals, EeCombatGoalArgs, GoalHost};

// ---- the host's operations (goal_objects.rs's numbering goes on)

/// `inUnit->mTileNode != NULL`.
pub const EE_COMBAT_G_ON_TILE: i32 = 1020;
/// `inUnit->mComplexUnit` (a handle).
pub const EE_COMBAT_G_COMPLEX: i32 = 1021;
/// the attacker's flags: `EE_COMBAT_G_HUMAN` (`mPlayer->IsHuman()`), `EE_COMBAT_G_PROJECTILE` (its class's
/// `mIsProjectile`), `EE_COMBAT_G_PRIEST` (its class's `IsPriest()`).
pub const EE_COMBAT_G_ATTACKER_FLAGS: i32 = 1022;
pub const EE_COMBAT_G_HUMAN: i64 = 1;
pub const EE_COMBAT_G_PROJECTILE: i64 = 2;
pub const EE_COMBAT_G_PRIEST: i64 = 4;
/// the complex unit's `mNumAttackers`; with `args[0]` non-zero, adds it first.
pub const EE_COMBAT_G_NUM_ATTACKERS: i32 = 1023;
/// the complex unit's parts: 1 `mNextPart != NULL`, 2 `mPreviousPart != NULL`.
pub const EE_COMBAT_G_PARTS: i32 = 1024;
/// `EEGetAttackPart(unit, args[0])` (a handle).
pub const EE_COMBAT_G_ATTACK_PART: i32 = 1025;
/// `unit->Reference()`, `unit->Dereference()`.
pub const EE_COMBAT_G_REFERENCE: i32 = 1026;
pub const EE_COMBAT_G_DEREFERENCE: i32 = 1027;
/// `mTileX`, `mTileY` into `args[0]`, `args[1]`; `GetTileX()`, `GetTileY()` (the location's tile) into `args[2]`, `args[3]`.
pub const EE_COMBAT_G_TILE: i32 = 1028;
/// the unit's class's `mFamily`.
pub const EE_COMBAT_G_FAMILY: i32 = 1029;
/// the attacker's player's attacks: `args[0]` 1 begins one (the first sets the combat state 2), -1 ends one (the last
/// sets it 1), 2 a loaded one (`args[1]`: the save has the live state; see `repair`).
pub const EE_COMBAT_G_PLAYER_ATTACK: i32 = 1030;
/// SetTarget's checks (`ESVERIFY`: the target class targetable by the attacker, its weapon in the table).
pub const EE_COMBAT_G_VERIFY_TARGET: i32 = 1031;
/// `::gWorld.GetUnit(args[0])` (a handle); with `args[1]` non-zero, `EEUnitCast<EEComplexUnit>` of it.
pub const EE_COMBAT_G_FIND_UNIT: i32 = 1032;
/// `EEFindLoadedGhostUnit(args[0])`.
pub const EE_COMBAT_G_FIND_GHOST: i32 = 1033;
/// `mUnitID` (`GetUnitID()`).
pub const EE_COMBAT_G_ID: i32 = 1034;
/// `mHitPoints`.
pub const EE_COMBAT_G_HIT_POINTS: i32 = 1035;
/// the class's `mCategory`.
pub const EE_COMBAT_G_CATEGORY: i32 = 1036;
/// the complex unit's `mVelocity` (float bits).
pub const EE_COMBAT_G_VELOCITY: i32 = 1037;
/// `EEGetLeadPosition(unit, args[0], p)` (args[1] 0) or `EEGetFiringPosition(unit, args[0], p)` (args[1] 1) on the
/// caller's point `p` (its bits in `args[2..5]`: both may leave parts of it as they were): the point's bits into
/// `args[0..3]`.
pub const EE_COMBAT_G_POSITION: i32 = 1038;
/// Suits' reads: `mPlayer` of the unit (a handle), the class's `mAmmoType`, the class's `CanBeAttacked()`.
pub const EE_COMBAT_G_PLAYER: i32 = 1039;
pub const EE_COMBAT_G_AMMO_TYPE: i32 = 1040;
pub const EE_COMBAT_G_CAN_BE_ATTACKED: i32 = 1041;
/// a player's `mPlayerID`; with `args[0]` set to 1, `mEnemies[args[1]]` instead.
pub const EE_COMBAT_G_PLAYER_READ: i32 = 1042;
/// the file's typed calls (the result answered; a read's value in and out of `args[0]`): `unsigned long`, `long`,
/// `bool`.
pub const EE_COMBAT_G_WRITE_U32: i32 = 1012;
pub const EE_COMBAT_G_READ_U32: i32 = 1013;
pub const EE_COMBAT_G_WRITE_I32: i32 = 1014;
pub const EE_COMBAT_G_READ_I32: i32 = 1015;
pub const EE_COMBAT_G_WRITE_BOOL: i32 = 1016;
pub const EE_COMBAT_G_READ_BOOL: i32 = 1017;

use crate::goal_objects::{EE_COMBAT_G_READ_F32, EE_COMBAT_G_TELL_GROUP, EE_COMBAT_G_WRITE_F32};

/// `kUnitFamilyAnimal` (0x11), `kUnitCategoryGate` (0x19; ee/EEUnitClassIDs.h), the off-map tile.
const FAMILY_ANIMAL: i64 = 0x11;
const CATEGORY_GATE: i64 = 0x19;
const OFF_MAP: i32 = -10000;
const NO_ID: u32 = 0xffff_ffff;

/// An attack-unit goal's own state (the original's members).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatAttackGoal {
    /// `mLocation`: the goal's destination (float bits)
    pub location: [u32; 3],
    /// `mAttacker`, `mTarget` (handles; the saved ids between Load and RepairPointers)
    pub attacker: u64,
    pub target: u64,
    /// `mTargetTileX`, `mTargetTileY`: the target's last known tile
    pub tile: [i32; 2],
    /// `mSidestepCount`
    pub sidestep: u32,
    /// `mIsConvert`
    pub convert: u8,
}

pub fn attack_of(goal: u64) -> EeCombatAttackGoal {
    with_goals(|g| g.attack.get(&goal).copied()).unwrap_or_default()
}

fn put(goal: u64, a: EeCombatAttackGoal) {
    with_goals(|g| g.attack.insert(goal, a));
}

fn op<H: GoalHost>(h: &mut H, code: i32, unit: u64, a0: i64, a1: i64) -> i64 {
    let mut args: EeCombatGoalArgs = [0; 8];
    args[0] = a0;
    args[1] = a1;
    h.op(code, unit, &mut args)
}

fn tile<H: GoalHost>(h: &mut H, unit: u64) -> [i64; 4] {
    let mut args: EeCombatGoalArgs = [0; 8];
    h.op(EE_COMBAT_G_TILE, unit, &mut args);
    [args[0], args[1], args[2], args[3]]
}

/// `EEUGAttackUnit(inAttacker, inTarget, inArg3, inIsConvert)`'s body after the base goal: the AI attacker count on a
/// mapped target, a multi-part target's attack part (not for a priest), the target referenced and its tile, the goal's
/// location the original target's tile centre, the attacker's player's attack (not on an animal).
pub fn create<H: GoalHost>(h: &mut H, goal: u64, attacker: u64, target: u64, convert: bool) {
    let mut a = EeCombatAttackGoal { attacker, convert: u8::from(convert), ..Default::default() };
    let mut chosen = target;
    if op(h, EE_COMBAT_G_ON_TILE, target, 0, 0) != 0 {
        let complex = op(h, EE_COMBAT_G_COMPLEX, target, 0, 0) as u64;
        if attacker != 0 {
            let flags = op(h, EE_COMBAT_G_ATTACKER_FLAGS, attacker, 0, 0);
            if flags & EE_COMBAT_G_HUMAN == 0 && flags & EE_COMBAT_G_PROJECTILE == 0 {
                op(h, EE_COMBAT_G_NUM_ATTACKERS, complex, 1, 0);
            }
        }
        // (the original reads the attacker's class unchecked here)
        if op(h, EE_COMBAT_G_PARTS, complex, 0, 0) == 1
            && op(h, EE_COMBAT_G_ATTACKER_FLAGS, attacker, 0, 0) & EE_COMBAT_G_PRIEST == 0
        {
            chosen = op(h, EE_COMBAT_G_ATTACK_PART, attacker, complex as i64, 0) as u64;
        }
    }
    a.target = chosen;
    if chosen != 0 {
        op(h, EE_COMBAT_G_REFERENCE, chosen, 0, 0);
        let t = tile(h, chosen);
        a.tile = [t[0] as i32, t[1] as i32];
    } else {
        a.tile = [OFF_MAP, OFF_MAP];
    }
    let t = tile(h, target);
    a.location = [(t[2] as f32 + 0.5).to_bits(), (t[3] as f32 + 0.5).to_bits(), 0.0f32.to_bits()];
    if attacker != 0 && chosen != 0 && op(h, EE_COMBAT_G_FAMILY, chosen, 0, 0) != FAMILY_ANIMAL {
        op(h, EE_COMBAT_G_PLAYER_ATTACK, attacker, 1, 0);
    }
    a.sidestep = 0;
    put(goal, a);
}

/// The empty goal of the load (`EEUGAttackUnit(inArg)`: no target, no sidestep; the rest as the load reads it).
pub fn create_empty(goal: u64) {
    put(goal, EeCombatAttackGoal::default());
}

/// `~EEUGAttackUnit`: the player's attack ended (not on an animal), the AI attacker count, the target released. The
/// record goes.
pub fn destroy<H: GoalHost>(h: &mut H, goal: u64) {
    let a = attack_of(goal);
    if a.attacker != 0 && a.target != 0 && op(h, EE_COMBAT_G_FAMILY, a.target, 0, 0) != FAMILY_ANIMAL {
        op(h, EE_COMBAT_G_PLAYER_ATTACK, a.attacker, -1, 0);
    }
    if a.target != 0 {
        if a.attacker != 0 {
            let flags = op(h, EE_COMBAT_G_ATTACKER_FLAGS, a.attacker, 0, 0);
            if flags & EE_COMBAT_G_HUMAN == 0 {
                let complex = op(h, EE_COMBAT_G_COMPLEX, a.target, 0, 0) as u64;
                if complex != 0
                    && flags & EE_COMBAT_G_PROJECTILE == 0
                    && op(h, EE_COMBAT_G_NUM_ATTACKERS, complex, 0, 0) > 0
                {
                    op(h, EE_COMBAT_G_NUM_ATTACKERS, complex, -1, 0);
                }
            }
        }
        op(h, EE_COMBAT_G_DEREFERENCE, a.target, 0, 0);
    }
    with_goals(|g| g.attack.remove(&goal));
}

/// `SetTarget(inTarget)`: the reference moved, the target's tile (-10000 without one).
pub fn set_target<H: GoalHost>(h: &mut H, goal: u64, target: u64) {
    let mut a = attack_of(goal);
    if a.target != 0 {
        op(h, EE_COMBAT_G_DEREFERENCE, a.target, 0, 0);
    }
    a.target = target;
    if target != 0 {
        op(h, EE_COMBAT_G_VERIFY_TARGET, a.attacker, target as i64, 0);
        op(h, EE_COMBAT_G_REFERENCE, target, 0, 0);
        let t = tile(h, target);
        a.tile = [t[0] as i32, t[1] as i32];
    } else {
        a.tile = [OFF_MAP, OFF_MAP];
    }
    put(goal, a);
}

/// `RetargetGatePart`: a gate target becomes the part `EEGetAttackPart` gives.
pub fn retarget_gate_part<H: GoalHost>(h: &mut H, goal: u64) {
    let a = attack_of(goal);
    if op(h, EE_COMBAT_G_CATEGORY, a.target, 0, 0) == CATEGORY_GATE {
        let complex = op(h, EE_COMBAT_G_COMPLEX, a.target, 0, 0);
        let part = op(h, EE_COMBAT_G_ATTACK_PART, a.attacker, complex, 0) as u64;
        set_target(h, goal, part);
    }
}

/// `UpdateTargetTile`: the target's tile now.
pub fn update_target_tile<H: GoalHost>(h: &mut H, goal: u64) {
    let mut a = attack_of(goal);
    let t = tile(h, a.target);
    a.tile = [t[0] as i32, t[1] as i32];
    put(goal, a);
}

/// `ReplaceUnit(old, new)`: the attacker, or a target whose complex unit is the old unit (the reference moved).
pub fn replace_unit<H: GoalHost>(h: &mut H, goal: u64, old: u64, new: u64) {
    let mut a = attack_of(goal);
    if old == a.attacker {
        a.attacker = new;
    } else if a.target != 0 {
        let complex = op(h, EE_COMBAT_G_COMPLEX, a.target, 0, 0) as u64;
        if complex != 0 && complex == old {
            op(h, EE_COMBAT_G_DEREFERENCE, a.target, 0, 0);
            a.target = new;
            op(h, EE_COMBAT_G_REFERENCE, new, 0, 0);
        }
    } else {
        return;
    }
    put(goal, a);
}

/// `GetDestination(inUnit, out)`: a lead position on a moving target, else a firing position (both for the goal's
/// attacker) on the caller's point `inout` (the positions may leave parts of it); kept as the goal's location with the
/// target's tile; the unit's group told. Returns the point's bits.
pub fn destination<H: GoalHost>(h: &mut H, goal: u64, unit: u64, inout: [u32; 3]) -> [u32; 3] {
    let a = attack_of(goal);
    let complex = op(h, EE_COMBAT_G_COMPLEX, a.target, 0, 0) as u64;
    let moving = complex != 0 && f32::from_bits(op(h, EE_COMBAT_G_VELOCITY, complex, 0, 0) as u32) > 0.0;
    let mut args: EeCombatGoalArgs = [0; 8];
    args[0] = a.target as i64;
    args[1] = i64::from(!moving);
    for (slot, b) in args[2..5].iter_mut().zip(inout) {
        *slot = i64::from(b);
    }
    h.op(EE_COMBAT_G_POSITION, a.attacker, &mut args);
    let out = [args[0] as u32, args[1] as u32, args[2] as u32];
    let mut a = attack_of(goal);
    a.location = out;
    put(goal, a);
    update_target_tile(h, goal);
    let mut args: EeCombatGoalArgs = [0; 8];
    for (slot, b) in args.iter_mut().zip(out) {
        *slot = i64::from(b);
    }
    h.op(EE_COMBAT_G_TELL_GROUP, unit, &mut args);
    out
}

/// `Suits(inUnit)`: the target is another player's, an enemy of the unit's player, and (with an ammunition type) can
/// be attacked and is damaged by it (`EEDamageFactor` of its family).
pub fn suits<H: GoalHost>(h: &mut H, goal: u64, unit: u64) -> bool {
    let a = attack_of(goal);
    if a.target == 0 {
        return false;
    }
    let (own, other) = (op(h, EE_COMBAT_G_PLAYER, unit, 0, 0), op(h, EE_COMBAT_G_PLAYER, a.target, 0, 0));
    if other == own {
        return false;
    }
    let other_id = op(h, EE_COMBAT_G_PLAYER_READ, other as u64, 0, 0);
    if op(h, EE_COMBAT_G_PLAYER_READ, own as u64, 1, other_id) == 0 {
        return false;
    }
    let kind = op(h, EE_COMBAT_G_AMMO_TYPE, unit, 0, 0);
    if kind < 0 {
        return true;
    }
    let family = op(h, EE_COMBAT_G_FAMILY, a.target, 0, 0);
    op(h, EE_COMBAT_G_CAN_BE_ATTACKED, a.target, 0, 0) != 0
        && crate::damage_table::ee_combat_damage_factor(family as i32, kind as i32) > 0.0
}

/// The checksum's own part: the location's raw bits, the attacker's and target's ids, the target's last tile.
pub fn checksum<H: GoalHost>(h: &mut H, goal: u64) -> i32 {
    let a = attack_of(goal);
    let mut sum = 0i32;
    for b in a.location {
        sum = sum.wrapping_add(b as i32);
    }
    if a.attacker != 0 {
        sum = sum.wrapping_add(op(h, EE_COMBAT_G_ID, a.attacker, 0, 0) as i32);
    }
    if a.target != 0 {
        sum = sum.wrapping_add(op(h, EE_COMBAT_G_ID, a.target, 0, 0) as i32);
    }
    sum.wrapping_add(a.tile[0].wrapping_add(a.tile[1]))
}

fn write<H: GoalHost>(h: &mut H, code: i32, value: i64) -> i32 {
    op(h, code, 0, value, 0) as i32
}

fn read<H: GoalHost>(h: &mut H, code: i32, value: &mut i64) -> i32 {
    let mut args: EeCombatGoalArgs = [0; 8];
    args[0] = *value;
    let result = h.op(code, 0, &mut args) as i32;
    *value = args[0];
    result
}

/// `Save`'s own part after the base goal's: the target's id while the goal holds it (alive, or dying with the goal
/// ghosts' format `ghosts`), else -1; the attacker's id or -1; the target's last tile; the convert flag; with the live
/// state (`live`), the destination and the sidestep count. The first error is answered.
pub fn save<H: GoalHost>(h: &mut H, goal: u64, live: bool, ghosts: bool) -> i32 {
    let a = attack_of(goal);
    let held = a.target != 0 && ((op(h, EE_COMBAT_G_HIT_POINTS, a.target, 0, 0) as i32) > 0 || ghosts);
    let id = if held { op(h, EE_COMBAT_G_ID, a.target, 0, 0) } else { i64::from(NO_ID) };
    let mut r = write(h, EE_COMBAT_G_WRITE_U32, id);
    if r != 0 {
        return r;
    }
    let id = if a.attacker != 0 { op(h, EE_COMBAT_G_ID, a.attacker, 0, 0) } else { i64::from(NO_ID) };
    r = write(h, EE_COMBAT_G_WRITE_U32, id);
    if r != 0 {
        return r;
    }
    r = write(h, EE_COMBAT_G_WRITE_I32, i64::from(a.tile[0]));
    if r != 0 {
        return r;
    }
    r = write(h, EE_COMBAT_G_WRITE_I32, i64::from(a.tile[1]));
    if r == 0 {
        r = write(h, EE_COMBAT_G_WRITE_BOOL, i64::from(a.convert));
    }
    if r == 0 && live {
        for b in a.location {
            r = write(h, EE_COMBAT_G_WRITE_F32, i64::from(b));
            if r != 0 {
                break;
            }
        }
        if r == 0 {
            r = write(h, EE_COMBAT_G_WRITE_U32, i64::from(a.sidestep));
        }
    }
    r
}

/// `Load`'s own part after the base goal's (version `major`.`minor`): the target's and attacker's saved ids, the
/// target's last tile, the convert flag (from 0.238), with the live state the destination and the sidestep count. A
/// failed read keeps what was read before it; the first error is answered.
pub fn load<H: GoalHost>(h: &mut H, goal: u64, major: u32, minor: u32, live: bool) -> i32 {
    let mut a = attack_of(goal);
    let finish = |a: EeCombatAttackGoal, r: i32| {
        put(goal, a);
        r
    };
    let mut id = 0i64;
    let r = read(h, EE_COMBAT_G_READ_U32, &mut id);
    if r != 0 {
        return finish(a, r);
    }
    a.target = id as u32 as u64;
    let r = read(h, EE_COMBAT_G_READ_U32, &mut id);
    if r != 0 {
        return finish(a, r);
    }
    a.attacker = id as u32 as u64;
    let mut v = i64::from(a.tile[0]);
    let r = read(h, EE_COMBAT_G_READ_I32, &mut v);
    a.tile[0] = v as i32;
    if r != 0 {
        return finish(a, r);
    }
    let mut v = i64::from(a.tile[1]);
    let r = read(h, EE_COMBAT_G_READ_I32, &mut v);
    a.tile[1] = v as i32;
    if r != 0 {
        return finish(a, r);
    }
    let mut r = 0;
    if major == 0 && minor < 0xee {
        a.convert = 0;
    } else {
        let mut v = i64::from(a.convert);
        r = read(h, EE_COMBAT_G_READ_BOOL, &mut v);
        a.convert = u8::from(v != 0);
    }
    if r == 0 && live {
        for slot in a.location.iter_mut() {
            let mut v = i64::from(*slot);
            r = read(h, EE_COMBAT_G_READ_F32, &mut v);
            *slot = v as u32;
            if r != 0 {
                break;
            }
        }
        if r == 0 {
            let mut v = i64::from(a.sidestep);
            r = read(h, EE_COMBAT_G_READ_U32, &mut v);
            a.sidestep = v as u32;
        }
    }
    finish(a, r)
}

/// `RepairPointers`: the saved ids back to units (the target out of the world a ghost the goal held), the target
/// referenced, its tile when none was saved, the attacker's player's attack counted as the running game counts it
/// (`live`: the save has the live state). 3 and both cleared when the target is gone.
pub fn repair<H: GoalHost>(h: &mut H, goal: u64, live: bool) -> i32 {
    let mut a = attack_of(goal);
    let mut result = 3;
    let target_id = a.target as u32;
    if target_id != NO_ID {
        let mut target = op(h, EE_COMBAT_G_FIND_UNIT, 0, i64::from(target_id), 0) as u64;
        if target == 0 {
            target = op(h, EE_COMBAT_G_FIND_GHOST, 0, i64::from(target_id), 0) as u64;
        }
        a.target = target;
        if target != 0 {
            op(h, EE_COMBAT_G_REFERENCE, target, 0, 0);
            result = 0;
            if a.tile == [0, 0] {
                let t = tile(h, target);
                a.tile = [t[0] as i32, t[1] as i32];
            }
            let attacker_id = a.attacker as u32;
            if attacker_id != NO_ID {
                a.attacker = op(h, EE_COMBAT_G_FIND_UNIT, 0, i64::from(attacker_id), 1) as u64;
                if a.attacker != 0 {
                    if !live || op(h, EE_COMBAT_G_FAMILY, target, 0, 0) != FAMILY_ANIMAL {
                        op(h, EE_COMBAT_G_PLAYER_ATTACK, a.attacker, 2, i64::from(live));
                    }
                    result = 0;
                }
            } else {
                a.attacker = 0;
            }
        } else {
            a.attacker = 0;
        }
    } else {
        a.target = 0;
        a.attacker = 0;
    }
    put(goal, a);
    result
}

// ---- the C ABI (the record's fields for the accessors; the bodies are goal_dispatch.rs's)

/// The record of the goal into `out` (zeroes for none).
///
/// # Safety
/// `out` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_attack_get(goal: u64, out: *mut EeCombatAttackGoal) {
    unsafe { *out = attack_of(goal) };
}

/// The record of the goal replaced by `*value` (the C++'s writes of single fields: the attacker, the location, the
/// sidestep count).
///
/// # Safety
/// `value` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_attack_set(goal: u64, value: *const EeCombatAttackGoal) {
    put(goal, unsafe { *value });
}

#[cfg(test)]
#[path = "goal_attack_tests.rs"]
mod tests;
