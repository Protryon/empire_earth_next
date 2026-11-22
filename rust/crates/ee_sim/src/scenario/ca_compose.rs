//! A class attribute's world operations (`EE_SCENARIO_WORLD_CA_*`, class_attribute.rs) answered here: the upgrade's
//! and class's numbers read and written through the records the C++ classes derive from (`EeUnitsUnitClassUpgrade
//! Fields`, `EeUnitsUnitClassFields`), the calls (the range and velocity setters, the sight off and on, the herds,
//! the pending effect, the capture record) one C++ statement each (`EE_SCENARIO_WK_*`, `SMEClassWorld` in
//! Scenario Manager/EESMEffect.cpp), in the C++'s order. Moved from `SMEClassAttributeWorld` in the scenario editor
//! lane's round 4.

// the records keep the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::scenario::class_attribute::*;
use crate::scenario::fields::EeScenarioEffectFields;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::unit_class_fields::EeUnitsUnitClassFields;

// ---- the statements (handles are the objects' addresses)
/// `SMEPlayer(a0)`; player a0's `GetUpgrade(a1)`; upgrade a0 as its `EeUnitsUnitClassUpgradeFields*`; its
/// `mUnitClass`; class a0 as its `EeUnitsUnitClassFields*`; player a0's `mPlayerID`.
pub const EE_SCENARIO_WK_PLAYER: u32 = 740;
pub const EE_SCENARIO_WK_UPGRADE: u32 = 741;
pub const EE_SCENARIO_WK_UPGRADE_FIELDS: u32 = 742;
pub const EE_SCENARIO_WK_CLASS: u32 = 743;
pub const EE_SCENARIO_WK_CLASS_FIELDS: u32 = 744;
pub const EE_SCENARIO_WK_PLAYER_ID: u32 = 745;
/// TEST ONLY: `EESMCaptureUnitChange(1, the 7 longs at a0, NULL, player a1, upgrade a2, (unsigned int)a3)`.
pub const EE_SCENARIO_WK_CAPTURE: u32 = 746;
/// `new EESMEffect`, effect a0 as its `EeScenarioEffectFields*`, `gWorld.mScenarioManager->AddV134Effect(*a0)`,
/// `delete` a0.
pub const EE_SCENARIO_WK_EFFECT_NEW: u32 = 747;
pub const EE_SCENARIO_WK_EFFECT_FIELDS: u32 = 748;
pub const EE_SCENARIO_WK_EFFECT_ADD: u32 = 749;
pub const EE_SCENARIO_WK_EFFECT_FREE: u32 = 750;
/// Upgrade a0's `SetRange(float a1)`, `SetMaxVelocity(float a1)`.
pub const EE_SCENARIO_WK_SET_RANGE: u32 = 751;
pub const EE_SCENARIO_WK_SET_VELOCITY: u32 = 752;
/// Player a0's `mFirstUnit`; unit a0's `mNext`, `mComplexUnit != NULL`, `mUnitClass`, `mComplexUnit->SightOffHere()`,
/// `SightOnHere()`.
pub const EE_SCENARIO_WK_FIRST_UNIT: u32 = 753;
pub const EE_SCENARIO_WK_NEXT_UNIT: u32 = 754;
pub const EE_SCENARIO_WK_UNIT_COMPLEX: u32 = 755;
pub const EE_SCENARIO_WK_UNIT_CLASS: u32 = 756;
pub const EE_SCENARIO_WK_SIGHT_OFF: u32 = 757;
pub const EE_SCENARIO_WK_SIGHT_ON: u32 = 758;
/// The herd list (the C++'s local vector) cleared; player a0's `GetHerdsOfType(it, a1)` (it appends); its size; herd
/// a0's `mIsActive = a1 != 0`.
pub const EE_SCENARIO_WK_HERDS_CLEAR: u32 = 759;
pub const EE_SCENARIO_WK_HERDS: u32 = 760;
pub const EE_SCENARIO_WK_HERD_COUNT: u32 = 761;
pub const EE_SCENARIO_WK_HERD_AT: u32 = 762;
pub const EE_SCENARIO_WK_HERD_ACTIVE: u32 = 763;

// the attributes (kSMUnitAttribute*)
const SHOCK_ARMOR_PLUS: i64 = 20;
const RELOAD_TIME_PLUS: i64 = 26;
const ATTACK_PLUS: i64 = 27;
const WORK_RATE_PLUS: i64 = 28;
const FLIGHT_TIME_PLUS: i64 = 29;
const LEADERSHIP_PLUS: i64 = 30;
const MAX_OBJECTS_CARRIED_PLUS: i64 = 31;
const REPAIR_PLUS: i64 = 32;
const HIT_POINTS_PLUS: i64 = 33;
const CARRY_AMOUNT_PLUS: i64 = 34;
const HEALING_PLUS: i64 = 35;
const RANGE_PLUS: i64 = 36;
const LINE_OF_SIGHT_PLUS: i64 = 37;
const MAX_VELOCITY_PLUS: i64 = 38;
const POWER_PLUS: i64 = 39;
const ANIMAL_AI_ACTIVE_PLUS: i64 = 40;
const CAMERA_FOCUS_PLUS: i64 = 41;
// kSMEffectObject, kSMObjectActionUnitAttribute
const EFFECT_OBJECT: c_ulong = 1;
const OBJECT_ACTION_UNIT_ATTRIBUTE: c_ulong = 8;

fn bits(v: f32) -> i64 {
    v.to_bits() as i64
}

fn float(v: i64) -> f32 {
    f32::from_bits(v as u32)
}

/// One upgrade number by attribute (`kSMEClassFields`): a `long`, a `float`, an armor, or the animal AI flag.
enum Field {
    Long(fn(&mut EeUnitsUnitClassUpgradeFields) -> &mut c_long),
    Float(fn(&mut EeUnitsUnitClassUpgradeFields) -> &mut f32),
    Armor(usize),
    Flag,
}

fn field(attribute: i64) -> Option<Field> {
    Some(match attribute {
        20..=25 => Field::Armor((attribute - SHOCK_ARMOR_PLUS) as usize),
        RELOAD_TIME_PLUS => Field::Long(|u| &mut u.mReloadTime),
        ATTACK_PLUS => Field::Float(|u| &mut u.mAttack),
        WORK_RATE_PLUS => Field::Long(|u| &mut u.mWorkRate),
        FLIGHT_TIME_PLUS => Field::Long(|u| &mut u.mFlightTime),
        LEADERSHIP_PLUS => Field::Float(|u| &mut u.mLeadership),
        MAX_OBJECTS_CARRIED_PLUS => Field::Long(|u| &mut u.mMaxObjectCarry),
        REPAIR_PLUS => Field::Long(|u| &mut u.mRepairRate),
        HIT_POINTS_PLUS => Field::Long(|u| &mut u.mMaxHitPoints),
        CARRY_AMOUNT_PLUS => Field::Long(|u| &mut u.mCarryAmount),
        HEALING_PLUS => Field::Long(|u| &mut u.mHealingPoints),
        RANGE_PLUS => Field::Float(|u| &mut u.mRange),
        LINE_OF_SIGHT_PLUS => Field::Float(|u| &mut u.mLineOfSight),
        MAX_VELOCITY_PLUS => Field::Float(|u| &mut u.mMaxVelocity),
        POWER_PLUS => Field::Long(|u| &mut u.mPower),
        ANIMAL_AI_ACTIVE_PLUS => Field::Flag,
        CAMERA_FOCUS_PLUS => Field::Float(|u| &mut u.mCameraFocus),
        _ => return None,
    })
}

/// The class's base of attribute `attribute` as the percent reads it (`SMEBits((float)...)`).
fn base(c: &EeUnitsUnitClassFields, attribute: i64) -> f32 {
    match attribute {
        RELOAD_TIME_PLUS => c.mReloadTime as f32,
        ATTACK_PLUS => c.mAttack as f32,
        WORK_RATE_PLUS => c.mWorkRate as f32,
        FLIGHT_TIME_PLUS => c.mFlightTime as f32,
        LEADERSHIP_PLUS => c.mLeadership as f32,
        MAX_OBJECTS_CARRIED_PLUS => c.mMaxObjectCarry as f32,
        REPAIR_PLUS => c.mRepairRate as f32,
        HIT_POINTS_PLUS => c.mHitPoints as f32,
        CARRY_AMOUNT_PLUS => c.mCarryAmount as f32,
        HEALING_PLUS => c.mHealingPoints as f32,
        RANGE_PLUS => c.mRange,
        LINE_OF_SIGHT_PLUS => c.mLineOfSight,
        MAX_VELOCITY_PLUS => c.mMaxVelocity,
        POWER_PLUS => c.mPower as f32,
        // an armor (the only other attribute the percent reads a base for)
        _ => c.mArmor[((attribute - SHOCK_ARMOR_PLUS) as usize).min(5)] as f32,
    }
}

/// The world operation `op` (`EE_SCENARIO_WORLD_CA_*`) composed of the C++ statements `cpp` runs, or `None` for the
/// other operations.
pub fn compose(cpp: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: [i64; 6]) -> Option<i64> {
    if !(EE_SCENARIO_WORLD_CA_UPGRADE..=EE_SCENARIO_WORLD_CA_CAPTURE).contains(&op) {
        return None;
    }
    let mut w = |op: u32, args: &[i64]| -> i64 {
        let mut v = [0i64; 6];
        v[..args.len()].copy_from_slice(args);
        cpp(op, v)
    };
    let player = if a[0] >= 0 { w(EE_SCENARIO_WK_PLAYER, &[a[0]]) } else { 0 };
    let upgrade = if player != 0 && a[1] >= 0 { w(EE_SCENARIO_WK_UPGRADE, &[player, a[1]]) } else { 0 };
    if op == EE_SCENARIO_WORLD_CA_UPGRADE {
        return Some((upgrade != 0) as i64);
    }
    let upgrade_fields = |w: &mut dyn FnMut(u32, &[i64]) -> i64| {
        w(EE_SCENARIO_WK_UPGRADE_FIELDS, &[upgrade]) as usize as *mut EeUnitsUnitClassUpgradeFields
    };
    let class_fields = |w: &mut dyn FnMut(u32, &[i64]) -> i64, class: i64| {
        w(EE_SCENARIO_WK_CLASS_FIELDS, &[class]) as usize as *const EeUnitsUnitClassFields
    };
    if op == EE_SCENARIO_WORLD_CA_CAPTURE {
        // TEST ONLY: SMCU (ee/EEScenarioCapture.h)
        let id = if player != 0 { w(EE_SCENARIO_WK_PLAYER_ID, &[player]) as c_long } else { -1 };
        let unit_type = if upgrade != 0 {
            let class = w(EE_SCENARIO_WK_CLASS, &[upgrade]);
            // SAFETY: the class's live record (the statement's answer)
            unsafe { (*class_fields(&mut w, class)).mUnitType as c_long }
        } else {
            -1
        };
        let args: [c_long; 7] =
            [a[2] as u32 as c_long, (a[2] >> 32) as c_long, id, unit_type, a[3] as c_long, a[4] as c_long, 0];
        w(EE_SCENARIO_WK_CAPTURE, &[args.as_ptr() as usize as i64, player, upgrade, a[5] as u32 as i64]);
        return Some(0);
    }
    if op == EE_SCENARIO_WORLD_CA_PENDING {
        let class = w(EE_SCENARIO_WK_CLASS, &[upgrade]);
        // SAFETY: the class's live record
        let unit_type = unsafe { (*class_fields(&mut w, class)).mUnitType };
        let id = w(EE_SCENARIO_WK_PLAYER_ID, &[player]) as c_long;
        let effect = w(EE_SCENARIO_WK_EFFECT_NEW, &[]);
        let e = w(EE_SCENARIO_WK_EFFECT_FIELDS, &[effect]) as usize as *mut EeScenarioEffectFields;
        // SAFETY: the new effect's record, written before the next C++ call
        unsafe {
            (*e).mAttributeUnit = c_ulong::MAX;
            (*e).mType = EFFECT_OBJECT;
            (*e).mObjectAction = OBJECT_ACTION_UNIT_ATTRIBUTE;
            (*e).mAttributeUnitType = unit_type;
            (*e).mAttributePlayer = id;
            (*e).mObject = 0;
            (*e).mAttribute = a[2] as c_ulong;
            (*e).mUsesPercent = a[3] != 0;
            (*e).mAttributeAmount = a[4] as c_ulong;
            (*e).mAttributeMode = a[5] as c_ulong;
        }
        w(EE_SCENARIO_WK_EFFECT_ADD, &[effect]);
        w(EE_SCENARIO_WK_EFFECT_FREE, &[effect]);
        return Some(0);
    }
    let class = w(EE_SCENARIO_WK_CLASS, &[upgrade]);
    let f = field(a[2]);
    if op == EE_SCENARIO_WORLD_CA_VALUE {
        let u = upgrade_fields(&mut w);
        // SAFETY: the upgrade's live record, read at once
        return Some(unsafe {
            match f {
                Some(Field::Armor(k)) => bits((*u).mArmor[k]),
                Some(Field::Long(get)) => *get(&mut *u) as i64,
                Some(Field::Float(get)) => bits(*get(&mut *u)),
                _ => (*u).mIsInitialized as i64,
            }
        });
    }
    if op == EE_SCENARIO_WORLD_CA_BASE {
        let c = class_fields(&mut w, class);
        // SAFETY: the class's live record, read at once
        return Some(bits(base(unsafe { &*c }, a[2])));
    }
    // EE_SCENARIO_WORLD_CA_SET
    match a[2] {
        RANGE_PLUS => {
            w(EE_SCENARIO_WK_SET_RANGE, &[upgrade, a[3]]);
        }
        MAX_VELOCITY_PLUS => {
            w(EE_SCENARIO_WK_SET_VELOCITY, &[upgrade, a[3]]);
        }
        LINE_OF_SIGHT_PLUS => {
            // the sight leaves the map before the change and comes back after it
            let mut units = Vec::new();
            let mut node = w(EE_SCENARIO_WK_FIRST_UNIT, &[player]);
            while node != 0 {
                if w(EE_SCENARIO_WK_UNIT_COMPLEX, &[node]) != 0 && w(EE_SCENARIO_WK_UNIT_CLASS, &[node]) == class {
                    w(EE_SCENARIO_WK_SIGHT_OFF, &[node]);
                    units.push(node);
                }
                node = w(EE_SCENARIO_WK_NEXT_UNIT, &[node]);
            }
            let u = upgrade_fields(&mut w);
            // SAFETY: the upgrade's live record, written before the next C++ call
            unsafe { (*u).mLineOfSight = float(a[3]) };
            for unit in units {
                w(EE_SCENARIO_WK_SIGHT_ON, &[unit]);
            }
        }
        ANIMAL_AI_ACTIVE_PLUS => {
            let u = upgrade_fields(&mut w);
            // SAFETY: as above
            unsafe { (*u).mIsInitialized = a[3] != 0 };
            w(EE_SCENARIO_WK_HERDS_CLEAR, &[]);
            let c = class_fields(&mut w, class);
            // SAFETY: the class's live record
            let unit_type = unsafe { (*c).mUnitType as c_long };
            w(EE_SCENARIO_WK_HERDS, &[player, unit_type as i64]);
            let mut j = w(EE_SCENARIO_WK_HERD_COUNT, &[]) as c_long - 1;
            while j >= 0 {
                let herd = w(EE_SCENARIO_WK_HERD_AT, &[j as i64]);
                // SAFETY: the upgrade's live record
                let active = unsafe { (*upgrade_fields(&mut w)).mIsInitialized };
                w(EE_SCENARIO_WK_HERD_ACTIVE, &[herd, active as i64]);
                j -= 1;
            }
        }
        _ => {
            let u = upgrade_fields(&mut w);
            // SAFETY: the upgrade's live record, written at once
            unsafe {
                match f {
                    Some(Field::Armor(k)) => (*u).mArmor[k] = float(a[3]),
                    Some(Field::Long(get)) => *get(&mut *u) = a[3] as c_long,
                    Some(Field::Float(get)) => *get(&mut *u) = float(a[3]),
                    _ => {}
                }
            }
        }
    }
    Some(0)
}

#[cfg(test)]
#[path = "ca_compose_tests.rs"]
mod tests;
