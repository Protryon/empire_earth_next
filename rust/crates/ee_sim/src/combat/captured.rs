//! The captured calls of the combat formulas (`include/ee/EECombatFormulas.h`): the formula, then, with
//! `EE_SIM_CAPTURE` set, one record of the tag, the input record's bytes and the output's (every field 4 bytes, no
//! padding). These were the header's inline wrappers; the C++ callers now make one call.

use crate::app::capture;
use crate::combat::geometry::EeCombatWeaponIn;
use crate::combat::*;

/// The bytes of a plain record.
///
/// # Safety
/// `value` points to `size` readable bytes (or `size` is 0).
unsafe fn bytes<'a>(value: *const u8, size: usize) -> &'a [u8] {
    if size == 0 || value.is_null() {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(value, size) }
    }
}

/// A record's payload: the input's bytes, the chain's (may be empty), the output's.
pub fn payload(input: &[u8], chain: &[u8], output: &[u8]) -> Vec<u8> {
    let mut p = Vec::with_capacity(input.len() + chain.len() + output.len());
    p.extend_from_slice(input);
    p.extend_from_slice(chain);
    p.extend_from_slice(output);
    p
}

fn record<I, O>(name: &[u8; 4], input: &I, output: &O) {
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: plain records of 4-byte fields
    let (i, o) = unsafe {
        (
            bytes((input as *const I).cast(), std::mem::size_of::<I>()),
            bytes((output as *const O).cast(), std::mem::size_of::<O>()),
        )
    };
    capture::commit(u32::from_le_bytes(*name), &payload(i, &[], o));
}

/// `EECombatCapture(tag, input, output)`: one record of two plain values, when the capture is on.
///
/// # Safety
/// `input` points to `input_size` bytes and `output` to `output_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_capture(
    tag: u32,
    input: *const u8,
    input_size: usize,
    output: *const u8,
    output_size: usize,
) {
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller's contract
    let (i, o) = unsafe { (bytes(input, input_size), bytes(output, output_size)) };
    capture::commit(tag, &payload(i, &[], o));
}

/// `EECombatChainCapture(tag, input, chain, output)`: the same with `chain_floats` floats between the two.
///
/// # Safety
/// `input` points to `input_size` bytes, `chain` to `chain_floats` floats (or the count is 0) and `output` to
/// `output_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_chain_capture(
    tag: u32,
    input: *const u8,
    input_size: usize,
    chain: *const f32,
    chain_floats: usize,
    output: *const u8,
    output_size: usize,
) {
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller's contract
    let (i, c, o) = unsafe { (bytes(input, input_size), bytes(chain.cast(), chain_floats * 4), bytes(output, output_size)) };
    capture::commit(tag, &payload(i, c, o));
}

/// `EECombatEffectWeaponPoint`'s end: the weapon point of `input` over `chain` into `out` (three floats), and its
/// CBEW record (the site, the input, the chain, the point).
///
/// # Safety
/// `input` must point to a valid record, `chain` to `input.chain` matrices (or be NULL when it is 0) and `out` to
/// three floats.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_effect_weapon_point(
    site: i32,
    input: *const EeCombatWeaponIn,
    chain: *const [f32; 16],
    out: *mut f32,
) {
    // SAFETY: the caller's contract
    unsafe { crate::combat::geometry::ee_combat_weapon_point(input, chain, out) };
    if !capture::ee_app_capture_active() {
        return;
    }
    // SAFETY: the caller's contract
    let (i, c, o) = unsafe {
        let count = if chain.is_null() || (*input).chain <= 0 { 0 } else { (*input).chain as usize };
        (
            bytes(input.cast(), std::mem::size_of::<EeCombatWeaponIn>()),
            bytes(chain.cast(), count * 64),
            bytes(out.cast(), 12),
        )
    };
    capture::commit(u32::from_le_bytes(*b"CBEW"), &payload(&site.to_le_bytes(), &payload(i, c, &[]), o));
}

/// EEToHitTable::GetAttackFactor, with its CBAF record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_attack_factor_captured(input: *const EeCombatAttackFactorIn) -> f32 {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = attack_factor(input);
    record(b"CBAF", input, &output);
    output
}

/// EEUnitClass::GetElevationFactor, with its CBEL record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_elevation_captured(input: *const EeCombatElevationIn) -> f32 {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = elevation(input);
    record(b"CBEL", input, &output);
    output
}

/// EEComplexUnit::ApplyMorale, with its CBMO record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_morale_captured(input: *const EeCombatMoraleIn) -> i32 {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = morale(input);
    record(b"CBMO", input, &output);
    output
}

/// EEUnit::TakeHit's hit points, with its CBTH record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_hit_points_captured(input: *const EeCombatHitPointsIn) -> EeCombatHitPointsOut {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = hit_points(input);
    record(b"CBTH", input, &output);
    output
}

/// EEUnit::TakeHit's damage effect test, with its CBDE record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_damage_effect_captured(input: *const EeCombatDamageEffectIn) -> i32 {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = damage_effect(input);
    record(b"CBDE", input, &output);
    output
}

/// EEUnitClass::Regenerate, with its CBRG record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regenerate_captured(input: *const EeCombatRegenerateIn) -> EeCombatRegenerateOut {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = regenerate(input);
    record(b"CBRG", input, &output);
    output
}

/// EEUnitClass::HealGarrison's timer, with its CBTM record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_timer_captured(input: *const EeCombatTimerIn) -> EeCombatTimerOut {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = timer(input);
    record(b"CBTM", input, &output);
    output
}

/// a heal, with its CBHL record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_heal_captured(input: *const EeCombatHealIn) -> EeCombatHealOut {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = heal(input);
    record(b"CBHL", input, &output);
    output
}

/// EEGetConversionCost, with its CBCC record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_conversion_cost_captured(input: *const EeCombatConversionCostIn) -> i32 {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = conversion_cost(input);
    record(b"CBCC", input, &output);
    output
}

/// UpdateConversion's clock and chance, with its CBCV record.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_conversion_captured(input: *const EeCombatConversionIn) -> EeCombatConversionOut {
    // SAFETY: the caller's contract
    let input = unsafe { &*input };
    let output = conversion(input);
    record(b"CBCV", input, &output);
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_payload_is_input_chain_output() {
        assert_eq!(payload(&[1, 2], &[], &[3]), vec![1, 2, 3]);
        assert_eq!(payload(&[1], &[7, 8], &[3, 4]), vec![1, 7, 8, 3, 4]);
        assert!(payload(&[], &[], &[]).is_empty());
    }

    #[test]
    fn bytes_of_nothing_are_empty() {
        // SAFETY: no byte is read
        assert!(unsafe { bytes(std::ptr::null(), 4) }.is_empty());
        let v = 0x0403_0201u32;
        // SAFETY: four bytes of v
        assert_eq!(unsafe { bytes((&v as *const u32).cast(), 4) }, &[1, 2, 3, 4]);
    }

    #[test]
    fn without_a_capture_file_nothing_is_written() {
        // no EE_SIM_CAPTURE in the tests: the calls return before reading anything
        let v = [1.0f32, 2.0];
        // SAFETY: valid pointers and sizes
        unsafe {
            ee_combat_capture(1, v.as_ptr().cast(), 8, v.as_ptr().cast(), 4);
            ee_combat_chain_capture(1, v.as_ptr().cast(), 8, v.as_ptr(), 2, v.as_ptr().cast(), 4);
        }
        assert!(!capture::ee_app_capture_active());
    }
}
