//! The C ABI the C++ unit methods call (`include/ee/EECombatFormulas.h`): one record in, the result back by value.
//! The caller owns every record; nothing is kept.

use crate::*;

/// # Safety
/// `input` must point to a valid record.
unsafe fn read<'a, T>(input: *const T) -> &'a T {
    // SAFETY: the caller's contract
    unsafe { &*input }
}

/// `EEToHitTable::GetAttackFactor`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_attack_factor(input: *const EeCombatAttackFactorIn) -> f32 {
    attack_factor(unsafe { read(input) })
}

/// The to-hit factor `ee_combat_damage` uses (the C++ reads the elevation only when it is positive).
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_hit_factor(input: *const EeCombatDamageIn) -> f32 {
    hit_factor(unsafe { read(input) })
}

/// `EEUnitClass::ComputeDamage`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_damage(input: *const EeCombatDamageIn) -> EeCombatDamageOut {
    damage(unsafe { read(input) })
}

/// `EEUnitClass::GetElevationFactor`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_elevation(input: *const EeCombatElevationIn) -> f32 {
    elevation(unsafe { read(input) })
}

/// `EEComplexUnit::ApplyMorale`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_morale(input: *const EeCombatMoraleIn) -> i32 {
    morale(unsafe { read(input) })
}

/// `EEUnit::TakeHit`'s hit points.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_hit_points(input: *const EeCombatHitPointsIn) -> EeCombatHitPointsOut {
    hit_points(unsafe { read(input) })
}

/// `EEUnit::TakeHit`'s damage effect test.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_damage_effect(input: *const EeCombatDamageEffectIn) -> i32 {
    damage_effect(unsafe { read(input) })
}

/// `EEUnitClass::Regenerate`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regenerate(input: *const EeCombatRegenerateIn) -> EeCombatRegenerateOut {
    regenerate(unsafe { read(input) })
}

/// `EEUnitClass::HealGarrison`'s timer.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_timer(input: *const EeCombatTimerIn) -> EeCombatTimerOut {
    timer(unsafe { read(input) })
}

/// A heal (`HealGarrison`, `EEUGHealUnit::Work`).
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_heal(input: *const EeCombatHealIn) -> EeCombatHealOut {
    heal(unsafe { read(input) })
}

/// `EEUGHealUnit::Work`'s clock.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_heal_step(input: *const EeCombatHealStepIn) -> EeCombatHealStepOut {
    heal_step(unsafe { read(input) })
}

/// `EEGetConversionCost`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_conversion_cost(input: *const EeCombatConversionCostIn) -> i32 {
    conversion_cost(unsafe { read(input) })
}

/// `UpdateConversion`'s clock and chance.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_conversion(input: *const EeCombatConversionIn) -> EeCombatConversionOut {
    conversion(unsafe { read(input) })
}

/// `EEGetBallisticPitch`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_ballistic(input: *const EeCombatBallisticIn) -> EeCombatBallisticOut {
    ballistic(unsafe { read(input) })
}

/// `EEPMProjectile::Step0`'s arc.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_arc(input: *const EeCombatArcIn) -> EeCombatArcOut {
    arc(unsafe { read(input) })
}

/// `EEIsInRangeToHit`'s distance test.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_range(input: *const EeCombatRangeIn) -> i32 {
    range(unsafe { read(input) })
}

/// `EECanReach`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_can_reach(input: *const EeCombatReachIn) -> i32 {
    can_reach(unsafe { read(input) })
}

/// `EEIsInWorkReach`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_work_reach(input: *const EeCombatReachIn) -> i32 {
    work_reach(unsafe { read(input) })
}

/// `EEIsInGatherReach`.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_gather_reach(input: *const EeCombatReachIn) -> i32 {
    gather_reach(unsafe { read(input) })
}

/// `EEUGHealUnit::IsInRange`.
#[no_mangle]
pub extern "C" fn ee_combat_within_range(distance_squared: f32, range: f32) -> i32 {
    within_range(distance_squared, range)
}

/// One damage table row.
///
/// # Safety
/// `input` must point to a valid record and `output` to writable room for one.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_damage_row(input: *const EeCombatDamageRowIn, output: *mut EeCombatDamageRowOut) {
    let row = damage_row(unsafe { read(input) });
    // SAFETY: the caller's contract
    unsafe { output.write(row) };
}
