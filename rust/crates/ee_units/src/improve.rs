//! A unit class's improvement levels in Rust (RUST_PLAN R5b.4, the twentieth round): `EEUnitClassImprovement`'s
//! arithmetic (0x23a5bf..0x23b61a): whether an improvement applies, the class's base value and one level's step, the
//! integer and float step sums, a level change on the upgrade's stats, the reapplication of the levels, and the
//! upgrade button's gain. The levels (16 saved words and the points spent) stay the C++ object's members; the class's
//! fields and the upgrade's stats come and go as plain records ([`EeUnitsImproveClass`], [`EeUnitsImproveStats`]),
//! made in Rust from the class's and the upgrade's own records (round 28: [`improve_class`], [`improve_stats`]); a
//! level change writes back only the stat it moved (range with line of sight through [`set_range`], speed through
//! `stats::set_max_velocity`), as the C++'s switch did.
//!
//! Determinism (RUST_PLAN 2.1): the original's `f32` sums one step at a time, its truncating casts (`(long)` of the
//! step and of the gain's rounded values, in range), the step factor 0.1 when the table leaves it 0.

use crate::stats::EeUnitsUnitClassUpgradeFields;
use crate::unit_class_fields::EeUnitsUnitClassFields;
use std::os::raw::c_long;

/// The improvement kinds (ee/EEImprovementIDs.h): 0 area effect, 1 attack, 2..7 the armours, 8 flight time or power,
/// 9 hit points, 10 leadership, 11 range, 12 repair, 14 speed, 15 transport; 13 unused.
pub const EE_UNITS_IMPROVEMENT_AREA_EFFECT: i32 = 0;
pub const EE_UNITS_IMPROVEMENT_ATTACK: i32 = 1;
pub const EE_UNITS_IMPROVEMENT_FLIGHT_TIME_OR_POWER: i32 = 8;
pub const EE_UNITS_IMPROVEMENT_HIT_POINTS: i32 = 9;
pub const EE_UNITS_IMPROVEMENT_LEADERSHIP: i32 = 10;
pub const EE_UNITS_IMPROVEMENT_RANGE: i32 = 11;
pub const EE_UNITS_IMPROVEMENT_REPAIR: i32 = 12;
pub const EE_UNITS_IMPROVEMENT_SPEED: i32 = 14;
pub const EE_UNITS_IMPROVEMENT_TRANSPORT: i32 = 15;

/// The armour index (`mArmor[k]`) of the armour kinds 2..7: arrow, gun, laser, missile, pierce, shock.
const ARMOR_INDEX: [usize; 6] = [1, 3, 4, 5, 2, 0];

/// `kRoundUp` (0x449ad0): added before the gain's float values are truncated
const ROUND_UP: f32 = 0.5001;

/// A unit class's fields the improvements read.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsImproveClass {
    pub area_effect: f32,
    pub attack: i32,
    pub armor: [i32; 6],
    pub flight_time: i32,
    pub power: i32,
    pub hit_points: i32,
    pub leadership: i32,
    pub range: f32,
    pub repair_rate: i32,
    pub max_velocity: f32,
    pub carry_amount: i32,
    pub transport_capacity: i32,
    /// the class's `mUpgradesAreaEffect`, `mUpgradesArrowArmor` .. `mUpgradesShockArmor` (by improvement 2..7),
    /// `mUpgradesRepair`
    pub upgrades_area_effect: bool,
    pub upgrades_armor: [bool; 6],
    pub upgrades_repair: bool,
    /// the class has an improvement table (`mImprovements`), and its entries' step factors
    pub has_table: bool,
    pub step_factors: [f32; 16],
}

/// An upgrade's stats a level change moves.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsImproveStats {
    pub area_effect: f32,
    pub attack: f32,
    pub armor: [f32; 6],
    pub flight_time: i32,
    pub power: i32,
    pub max_hit_points: i32,
    pub leadership: f32,
    pub range: f32,
    pub line_of_sight: f32,
    pub repair_rate: i32,
    pub max_velocity: f32,
    pub carry_amount: i32,
    /// the upgrade's own class has a flight time (a flight-time-or-power change moves the flight time)
    pub upgrade_has_flight_time: bool,
}

/// `CanUpgrade`: an improvement applies (its upgrade flag, or a base stat above 0, range above 1).
pub fn can_upgrade(c: &EeUnitsImproveClass, improvement: i32) -> bool {
    match improvement {
        0 => c.upgrades_area_effect,
        1 => c.attack > 0,
        2..=7 => c.upgrades_armor[(improvement - 2) as usize],
        8 => c.flight_time > 0 || c.power > 0,
        9 => c.hit_points > 0,
        10 => c.leadership > 0,
        11 => c.range > 1.0,
        12 => c.upgrades_repair,
        14 => c.max_velocity > 0.0,
        15 => c.transport_capacity > 0,
        _ => false,
    }
}

/// `GetBaseValue`: the class's base stat (flight time when it has one, else power; 0 for unknown kinds).
pub fn base_value(c: &EeUnitsImproveClass, improvement: i32) -> f32 {
    match improvement {
        0 => c.area_effect,
        1 => c.attack as f32,
        2..=7 => c.armor[ARMOR_INDEX[(improvement - 2) as usize]] as f32,
        8 => (if c.flight_time > 0 { c.flight_time } else { c.power }) as f32,
        9 => c.hit_points as f32,
        10 => c.leadership as f32,
        11 => c.range,
        12 => c.repair_rate as f32,
        14 => c.max_velocity,
        15 => c.carry_amount as f32,
        _ => 0.0,
    }
}

/// `GetCost`: one level's points (1 for the armours, else 2).
pub fn cost(improvement: i32) -> i32 {
    if (2..=7).contains(&(improvement as u32 as i64)) {
        1
    } else {
        2
    }
}

/// `GetStep`: the table's factor (0.1 when 0; 0 for repair) times the base value.
pub fn step(c: &EeUnitsImproveClass, improvement: i32) -> f32 {
    let mut factor = 0.0f32;
    if improvement != EE_UNITS_IMPROVEMENT_REPAIR {
        factor = c.step_factors[(improvement & 15) as usize];
        if factor == 0.0 {
            factor = 0.1;
        }
    }
    base_value(c, improvement) * factor
}

/// `AddIntSteps`: an integer stat moved `count` steps (the step truncated, at least `minimum`).
pub fn add_int_steps(
    c: &EeUnitsImproveClass,
    improvement: i32,
    value: i32,
    minimum: i32,
    count: i32,
    negative: bool,
) -> i32 {
    let mut s = step(c, improvement) as i32;
    if s < minimum {
        s = minimum;
    }
    s = s.wrapping_mul(if negative { -1 } else { 1 });
    if count > 0 {
        value.wrapping_add(s.wrapping_mul(count))
    } else {
        value
    }
}

/// `AddFloatSteps`: a float stat moved `count` steps, one addition at a time (the step at least `minimum`).
pub fn add_float_steps(
    c: &EeUnitsImproveClass,
    improvement: i32,
    value: f32,
    minimum: f32,
    count: i32,
    negative: bool,
) -> f32 {
    let mut s = step(c, improvement);
    if s < minimum {
        s = minimum;
    }
    s *= if !negative { 1.0 } else { -1.0 };
    let mut v = value;
    let mut i = count;
    while i > 0 {
        v += s;
        i -= 1;
    }
    v
}

/// The float improvements' stat, minimum step and display scale (area effect in tenths, speed in twentieths).
fn float_stat(s: &EeUnitsImproveStats, improvement: i32) -> Option<(f32, f32, f32)> {
    Some(match improvement {
        0 => (s.area_effect, 0.1, 10.0),
        1 => (s.attack, 1.0, 1.0),
        2..=7 => (s.armor[ARMOR_INDEX[(improvement - 2) as usize]], 1.0, 1.0),
        10 => (s.leadership, 1.0, 1.0),
        11 => (s.range, 1.0, 1.0),
        14 => (s.max_velocity, 0.05, 20.0),
        _ => return None,
    })
}

/// `ChangeLevel`: the level and the points spent moved by `delta`, the upgrade's stat as many steps (range also moves
/// line of sight; hit points at least 1); nothing without the class's table. True when the stats changed.
pub fn change_level(
    c: &EeUnitsImproveClass,
    s: &mut EeUnitsImproveStats,
    levels: &mut [i32; 16],
    total_cost: &mut i32,
    improvement: i32,
    delta: i32,
) -> bool {
    let points = cost(improvement);
    if !c.has_table {
        return false;
    }
    let k = (improvement & 15) as usize;
    levels[k] = levels[k].wrapping_add(delta);
    *total_cost = total_cost.wrapping_add(points.wrapping_mul(delta));
    let negative = delta < 0;
    let count = delta.wrapping_abs();
    if let Some((value, minimum, _)) = float_stat(s, improvement) {
        let v = add_float_steps(c, improvement, value, minimum, count, negative);
        match improvement {
            0 => s.area_effect = v,
            1 => s.attack = v,
            2..=7 => s.armor[ARMOR_INDEX[(improvement - 2) as usize]] = v,
            10 => s.leadership = v,
            11 => {
                s.range = v;
                s.line_of_sight = add_float_steps(c, improvement, s.line_of_sight, 1.0, count, negative);
            }
            _ => s.max_velocity = v,
        }
        return true;
    }
    match improvement {
        8 => {
            if s.upgrade_has_flight_time {
                s.flight_time = add_int_steps(c, improvement, s.flight_time, 1, count, negative);
            } else {
                s.power = add_int_steps(c, improvement, s.power, 1, count, negative);
            }
        }
        9 => s.max_hit_points = add_int_steps(c, improvement, s.max_hit_points, 1, count, negative).max(1),
        12 => s.repair_rate = add_int_steps(c, improvement, s.repair_rate, 1, count, negative),
        15 => s.carry_amount = add_int_steps(c, improvement, s.carry_amount, 1, count, negative),
        _ => return false,
    }
    true
}

/// `GetGainText`'s number: the stat gain of the improvement's step to level `level + 1` (flight time in seconds,
/// area effect in tenths, speed in twentieths; 0 without the class's table).
pub fn gain(c: &EeUnitsImproveClass, s: &EeUnitsImproveStats, levels: &[i32; 16], level: i32, improvement: i32) -> i32 {
    let mut count = level.wrapping_sub(levels[(improvement & 15) as usize]).wrapping_add(1);
    let negative = count < 0;
    if negative {
        count = count.wrapping_neg();
    }
    if !c.has_table {
        return 0;
    }
    if let Some((value, minimum, scale)) = float_stat(s, improvement) {
        let at = |n: i32| (add_float_steps(c, improvement, value, minimum, n, negative) * scale + ROUND_UP) as i32;
        return at(count).wrapping_sub(at(count - 1));
    }
    let int = |v: i32, n: i32| add_int_steps(c, improvement, v, 1, n, negative);
    match improvement {
        8 => {
            if c.flight_time > 0 {
                int(s.flight_time, count) / 1000 + int(s.flight_time, count - 1) / -1000
            } else {
                int(s.power, count).wrapping_sub(int(s.power, count - 1))
            }
        }
        9 => int(s.max_hit_points, count).wrapping_sub(int(s.max_hit_points, count - 1)),
        12 => int(s.repair_rate, count).wrapping_sub(int(s.repair_rate, count - 1)),
        15 => int(s.carry_amount, count).wrapping_sub(int(s.carry_amount, count - 1)),
        _ => 0,
    }
}

// ---- the C++ seam

/// `GetBaseValue`
///
/// # Safety
/// `c` points to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_improve_base_value(c: *const EeUnitsImproveClass, improvement: i32) -> f32 {
    // SAFETY: the caller's contract
    base_value(unsafe { &*c }, improvement)
}

/// `GetCost`
#[no_mangle]
pub extern "C" fn ee_units_improve_cost(improvement: i32) -> i32 {
    cost(improvement)
}

// ---- the records in place (round 28): the class's record and the upgrade's (stats.rs) read and written by the
// C++'s `ChangeLevel`, `GetGainText` and `CanUpgrade` through the calls below; `SetRange` with them

/// `kUnitCategoryBomb`, `kUnitTypeX04RomanLegionnaire` (ee/EEUnitClassIDs.h)
const CATEGORY_BOMB: u64 = 0xb;
const UNIT_TYPE_X04_ROMAN_LEGIONNAIRE: u64 = 0x2dc;

/// A `bool` member's value read as its byte.
fn flag(b: &bool) -> bool {
    // SAFETY: a member of a live record
    unsafe { crate::unit_fields::bool_byte(b) != 0 }
}

/// The class's fields the improvements read ([`EeUnitsImproveClass`], the C++'s old `ClassRecord`): its record and
/// its improvement table's step factors (None: no table).
#[allow(clippy::unnecessary_cast)] // `long` is i32 on Windows, i64 elsewhere
pub fn improve_class(c: &EeUnitsUnitClassFields, step_factors: Option<&[f32; 16]>) -> EeUnitsImproveClass {
    EeUnitsImproveClass {
        area_effect: c.mAreaEffect,
        attack: c.mAttack as i32,
        armor: c.mArmor.map(|a| a as i32),
        flight_time: c.mFlightTime as i32,
        power: c.mPower as i32,
        hit_points: c.mHitPoints as i32,
        leadership: c.mLeadership as i32,
        range: c.mRange,
        repair_rate: c.mRepairRate as i32,
        max_velocity: c.mMaxVelocity,
        carry_amount: c.mCarryAmount as i32,
        transport_capacity: c.mTransportCapacity as i32,
        upgrades_area_effect: flag(&c.mUpgradesAreaEffect),
        // by improvement 2..7: arrow, gun, laser, missile, pierce, shock
        upgrades_armor: [
            flag(&c.mUpgradesArrowArmor),
            flag(&c.mUpgradesGunArmor),
            flag(&c.mUpgradesLaserArmor),
            flag(&c.mUpgradesMissileArmor),
            flag(&c.mUpgradesPierceArmor),
            flag(&c.mUpgradesShockArmor),
        ],
        upgrades_repair: flag(&c.mUpgradesRepair),
        has_table: step_factors.is_some(),
        step_factors: step_factors.copied().unwrap_or_default(),
    }
}

/// The upgrade's stats a level change moves ([`EeUnitsImproveStats`], the C++'s old `StatsRecord`); `own` is the
/// upgrade's own class (`mUnitClass`).
#[allow(clippy::unnecessary_cast)] // `long` is i32 on Windows, i64 elsewhere
pub fn improve_stats(u: &EeUnitsUnitClassUpgradeFields, own: &EeUnitsUnitClassFields) -> EeUnitsImproveStats {
    EeUnitsImproveStats {
        area_effect: u.mAreaEffect,
        attack: u.mAttack,
        armor: u.mArmor,
        flight_time: u.mFlightTime as i32,
        power: u.mPower as i32,
        max_hit_points: u.mMaxHitPoints as i32,
        leadership: u.mLeadership,
        range: u.mRange,
        line_of_sight: u.mLineOfSight,
        repair_rate: u.mRepairRate as i32,
        max_velocity: u.mMaxVelocity,
        carry_amount: u.mCarryAmount as i32,
        upgrade_has_flight_time: own.mFlightTime > 0,
    }
}

/// `EEUnitClassUpgrade::SetRange`: the range and the projectile's launch speed to reach it at 45 degrees (a bomb:
/// its top speed; a Roman Legionnaire aims for its heal range; no projectile: 0). `own` is the upgrade's class,
/// `projectile` its `mProjectileClass`.
pub fn set_range(
    u: &mut EeUnitsUnitClassUpgradeFields,
    own: &EeUnitsUnitClassFields,
    projectile: Option<&EeUnitsUnitClassFields>,
    value: f32,
) {
    // the original's 0.78539819f
    let cosine = ee_ge::um::um_cos(std::f32::consts::FRAC_PI_4);
    u.mRange = value;
    u.mLaunchSpeed = match projectile {
        Some(p) if u64::from(p.mCategory) == CATEGORY_BOMB => p.mMaxVelocity,
        Some(p) => {
            let reach =
                if u64::from(own.mUnitType) == UNIT_TYPE_X04_ROMAN_LEGIONNAIRE { own.mHealRange } else { value };
            (p.mAcceleration * reach / (cosine * cosine * 2.0f32)).sqrt()
        }
        None => 0.0,
    };
}

/// `ChangeLevel` over the records: the level and points spent moved, and the stat the change moved written back to
/// the upgrade (range through [`set_range`] with line of sight, speed through `set_max_velocity`), as the C++'s
/// switch did. `class` is the class the call names, `own` the upgrade's (`mUnitClass`), `projectile` its projectile.
#[allow(clippy::too_many_arguments)]
pub fn change_level_records(
    class: &EeUnitsUnitClassFields,
    step_factors: Option<&[f32; 16]>,
    u: &mut EeUnitsUnitClassUpgradeFields,
    own: &EeUnitsUnitClassFields,
    projectile: Option<&EeUnitsUnitClassFields>,
    levels: &mut [i32; 16],
    total_cost: &mut i32,
    improvement: i32,
    delta: i32,
) {
    let c = improve_class(class, step_factors);
    let mut s = improve_stats(u, own);
    if !change_level(&c, &mut s, levels, total_cost, improvement, delta) {
        return;
    }
    match improvement {
        0 => u.mAreaEffect = s.area_effect,
        1 => u.mAttack = s.attack,
        2..=7 => u.mArmor = s.armor,
        8 => {
            if s.upgrade_has_flight_time {
                u.mFlightTime = s.flight_time as c_long;
            } else {
                u.mPower = s.power as c_long;
            }
        }
        9 => u.mMaxHitPoints = s.max_hit_points as c_long,
        10 => u.mLeadership = s.leadership,
        11 => {
            set_range(u, own, projectile, s.range);
            u.mLineOfSight = s.line_of_sight;
        }
        12 => u.mRepairRate = s.repair_rate as c_long,
        14 => crate::stats::set_max_velocity(u, own, s.max_velocity),
        15 => u.mCarryAmount = s.carry_amount as c_long,
        _ => {}
    }
}

/// `CanUpgrade` over the class's record.
///
/// # Safety
/// `class` points to a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_improve_can_upgrade_class(
    class: *const EeUnitsUnitClassFields,
    improvement: i32,
) -> bool {
    // SAFETY: the caller's contract
    can_upgrade(&improve_class(unsafe { &*class }, None), improvement)
}

/// `SetRange` ([`set_range`]).
///
/// # Safety
/// `upgrade` and `own` point to live records, `projectile` to one or null; no other reference to them is held.
#[no_mangle]
pub unsafe extern "C" fn ee_units_upgrade_set_range(
    upgrade: *mut EeUnitsUnitClassUpgradeFields,
    own: *const EeUnitsUnitClassFields,
    projectile: *const EeUnitsUnitClassFields,
    value: f32,
) {
    // SAFETY: the caller's contract
    unsafe { set_range(&mut *upgrade, &*own, projectile.as_ref(), value) }
}

/// `ChangeLevel` over the records ([`change_level_records`]); `step_factors` the class's table's 16 (null: none).
///
/// # Safety
/// The pointers name live records (`projectile`, `step_factors` may be null); `levels` points to 16 words.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_improve_change_level_records(
    class: *const EeUnitsUnitClassFields,
    step_factors: *const f32,
    upgrade: *mut EeUnitsUnitClassUpgradeFields,
    own: *const EeUnitsUnitClassFields,
    projectile: *const EeUnitsUnitClassFields,
    levels: *mut i32,
    total_cost: *mut i32,
    improvement: i32,
    delta: i32,
) {
    // SAFETY: the caller's contract
    unsafe {
        change_level_records(
            &*class,
            step_factors.cast::<[f32; 16]>().as_ref(),
            &mut *upgrade,
            &*own,
            projectile.as_ref(),
            &mut *levels.cast::<[i32; 16]>(),
            &mut *total_cost,
            improvement,
            delta,
        )
    }
}

/// `GetGainText`'s number over the records (the class is the upgrade's own).
///
/// # Safety
/// As [`ee_units_improve_change_level_records`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_improve_gain_records(
    class: *const EeUnitsUnitClassFields,
    step_factors: *const f32,
    upgrade: *const EeUnitsUnitClassUpgradeFields,
    levels: *const i32,
    level: i32,
    improvement: i32,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe {
        let class = &*class;
        let c = improve_class(class, step_factors.cast::<[f32; 16]>().as_ref());
        let s = improve_stats(&*upgrade, class);
        gain(&c, &s, &*levels.cast::<[i32; 16]>(), level, improvement)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn class() -> EeUnitsImproveClass {
        let mut c = EeUnitsImproveClass {
            area_effect: 0.5,
            attack: 12,
            armor: [3, 4, 5, 6, 7, 8],
            hit_points: 100,
            leadership: 0,
            range: 6.0,
            repair_rate: 5,
            max_velocity: 1.25,
            carry_amount: 10,
            transport_capacity: 0,
            flight_time: 0,
            power: 40,
            has_table: true,
            ..Default::default()
        };
        c.step_factors[1] = 0.25;
        c.upgrades_armor[0] = true;
        c
    }

    #[test]
    fn applicability_base_values_costs_and_steps() {
        let c = class();
        assert!(can_upgrade(&c, 1) && can_upgrade(&c, 2) && !can_upgrade(&c, 3));
        assert!(can_upgrade(&c, 8) && can_upgrade(&c, 11) && !can_upgrade(&c, 10) && !can_upgrade(&c, 13));
        // the armours: arrow 1, gun 3, laser 4, missile 5, pierce 2, shock 0
        assert_eq!((base_value(&c, 2), base_value(&c, 6), base_value(&c, 7)), (4.0, 5.0, 3.0));
        assert_eq!(base_value(&c, 8), 40.0);
        assert_eq!((cost(2), cost(7), cost(1), cost(-1)), (1, 1, 2, 2));
        assert_eq!(step(&c, 1), 3.0);
        assert_eq!(step(&c, 9), 100.0 * 0.1);
        assert_eq!(step(&c, 12), 0.0);
    }

    #[test]
    fn level_changes_move_the_stats() {
        let c = class();
        let mut s = EeUnitsImproveStats {
            attack: 12.0,
            max_hit_points: 100,
            range: 6.0,
            line_of_sight: 8.0,
            repair_rate: 5,
            max_velocity: 1.25,
            power: 40,
            ..Default::default()
        };
        let mut levels = [1i32; 16];
        let mut total = 0;
        assert!(change_level(&c, &mut s, &mut levels, &mut total, 1, 2));
        assert_eq!((s.attack, levels[1], total), (18.0, 3, 4));
        // range: at least 1 a step, line of sight with it
        change_level(&c, &mut s, &mut levels, &mut total, 11, 1);
        assert_eq!((s.range, s.line_of_sight), (7.0, 9.0));
        // repair's step is 0: at least 1
        change_level(&c, &mut s, &mut levels, &mut total, 12, 1);
        assert_eq!(s.repair_rate, 6);
        // hit points down stay at least 1
        change_level(&c, &mut s, &mut levels, &mut total, 9, -20);
        assert_eq!(s.max_hit_points, 1);
        change_level(&c, &mut s, &mut levels, &mut total, 8, 1);
        assert_eq!(s.power, 44);
        // speed: 1.25 * 0.1 = 0.125 a step, one f32 addition at a time
        change_level(&c, &mut s, &mut levels, &mut total, 14, 3);
        assert_eq!(s.max_velocity, 1.25f32 + 0.125 + 0.125 + 0.125);
        // no table: nothing moves
        let mut none = c;
        none.has_table = false;
        let before = s;
        assert!(!change_level(&none, &mut s, &mut levels, &mut total, 1, 1));
        assert_eq!(s, before);
    }

    #[test]
    fn the_gain_of_the_next_level() {
        let c = class();
        let s =
            EeUnitsImproveStats { attack: 12.0, area_effect: 0.5, max_velocity: 1.25, power: 40, ..Default::default() };
        let levels = [1i32; 16];
        assert_eq!(gain(&c, &s, &levels, 1, 1), 3);
        // area effect in tenths: a step of 0.1 at least (0.05 computed)
        assert_eq!(gain(&c, &s, &levels, 1, 0), 1);
        // speed in twentieths: 0.125 a step: (1.25 + 0.125) * 20 = 27.5 -> 28, 1.25 * 20 = 25 -> 25
        assert_eq!(gain(&c, &s, &levels, 1, 14), 3);
        assert_eq!(gain(&c, &s, &levels, 1, 8), 4);
        let mut none = c;
        none.has_table = false;
        assert_eq!(gain(&none, &s, &levels, 1, 1), 0);
    }

    fn class_record() -> Box<EeUnitsUnitClassFields> {
        let mut c = Box::<EeUnitsUnitClassFields>::default();
        c.mAttack = 12;
        c.mArmor = [3, 4, 5, 6, 7, 8];
        c.mHitPoints = 100;
        c.mRange = 6.0;
        c.mMaxVelocity = 2.0;
        c.mAcceleration = 4.0;
        c.mAngularAcceleration = 1.0;
        c.mMaxAngularVelocity = 1.0;
        c.mTurretMaxAngularVelocity = 1.0;
        c.mUpgradesGunArmor = true;
        c
    }

    #[test]
    fn the_records_give_the_old_records() {
        let c = class_record();
        let r = improve_class(&c, None);
        assert_eq!((r.attack, r.hit_points, r.range, r.has_table), (12, 100, 6.0, false));
        assert_eq!(r.upgrades_armor, [false, true, false, false, false, false]);
        assert!(can_upgrade(&r, 3) && !can_upgrade(&r, 2) && can_upgrade(&r, 11));
        let factors = [0.25f32; 16];
        assert!(improve_class(&c, Some(&factors)).has_table);
        let mut u = EeUnitsUnitClassUpgradeFields::default();
        crate::stats::init(&mut u, &c);
        let s = improve_stats(&u, &c);
        assert_eq!((s.attack, s.max_hit_points, s.upgrade_has_flight_time), (12.0, 100, false));
    }

    #[test]
    fn set_range_aims_the_projectile() {
        let own = class_record();
        let mut u = EeUnitsUnitClassUpgradeFields::default();
        set_range(&mut u, &own, None, 7.0);
        assert_eq!((u.mRange, u.mLaunchSpeed), (7.0, 0.0));
        let mut p = class_record();
        p.mAcceleration = 9.8;
        set_range(&mut u, &own, Some(&p), 7.0);
        let cosine = ee_ge::um::um_cos(std::f32::consts::FRAC_PI_4);
        assert_eq!(u.mLaunchSpeed, (9.8f32 * 7.0 / (cosine * cosine * 2.0)).sqrt());
        // the legionnaire aims for its heal range; a bomb falls at its top speed
        let mut legion = class_record();
        legion.mUnitType = 0x2dc;
        legion.mHealRange = 3.0;
        set_range(&mut u, &legion, Some(&p), 7.0);
        assert_eq!((u.mRange, u.mLaunchSpeed), (7.0, (9.8f32 * 3.0 / (cosine * cosine * 2.0)).sqrt()));
        p.mCategory = 0xb;
        set_range(&mut u, &own, Some(&p), 7.0);
        assert_eq!(u.mLaunchSpeed, 2.0);
    }

    #[test]
    fn change_level_writes_the_moved_stat_back() {
        let c = class_record();
        let factors = [0.5f32; 16];
        let mut u = EeUnitsUnitClassUpgradeFields::default();
        crate::stats::init(&mut u, &c);
        let mut levels = [1i32; 16];
        let mut total = 0;
        // no table: nothing moves, not even the level
        change_level_records(&c, None, &mut u, &c, None, &mut levels, &mut total, 9, 1);
        assert_eq!((levels[9], u.mMaxHitPoints), (1, 100));
        change_level_records(&c, Some(&factors), &mut u, &c, None, &mut levels, &mut total, 9, 2);
        assert_eq!((levels[9], total, u.mMaxHitPoints), (3, 4, 200));
        // speed through SetMaxVelocity: the acceleration scaled too
        change_level_records(&c, Some(&factors), &mut u, &c, None, &mut levels, &mut total, 14, 1);
        assert_eq!((u.mMaxVelocity, u.mAcceleration), (3.0, 4.0 * 1.5));
        // range through SetRange, with line of sight (Init's SetRange is the C++'s)
        set_range(&mut u, &c, None, c.mRange);
        let los = u.mLineOfSight;
        change_level_records(&c, Some(&factors), &mut u, &c, None, &mut levels, &mut total, 11, 1);
        assert_eq!((u.mRange, u.mLineOfSight, u.mLaunchSpeed), (9.0, los + 3.0, 0.0));
        // an armour: the six written back, one moved
        change_level_records(&c, Some(&factors), &mut u, &c, None, &mut levels, &mut total, 3, 1);
        assert_eq!(u.mArmor[3], 6.0 + 3.0);
        assert_eq!(u.mArmor[0], 3.0);
    }
}
