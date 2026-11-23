//! Two of `EEComplexUnit`'s inline members over its Rust records (units round 10; ee/EEUnit.h): the outermost
//! carrier (the `mPreviousPart` chain) and the velocity setter (`mVelocity`, `mF21C` against the stats' `mBC`). Both
//! read only record fields, so they are Rust's whole; the C++ members are one call each.

use crate::world::layout::layout;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use crate::world::unit_fields::{EeCxxComplexUnit, EeCxxUnitClassUpgrade, EeUnitsComplexUnitFields, EeUnitsUnitFields};

/// The complex record of a complex unit (its address plus the layout's record offset).
fn complex_of(unit: *mut EeCxxComplexUnit, at: u64) -> *mut EeUnitsComplexUnitFields {
    (unit as usize + at as usize) as *mut EeUnitsComplexUnitFields
}

/// The last unit up the chain of carriers from `unit` (each one's `mPreviousPart`), or `unit` when nothing carries
/// it. `at`: the complex record's offset in a unit.
///
/// # Safety
/// `unit` and every unit of its chain are live complex units.
pub unsafe fn outermost_carrier(unit: *mut EeCxxComplexUnit, at: u64) -> *mut EeCxxComplexUnit {
    let mut result = unit;
    // SAFETY: the caller's contract
    let mut carrier = unsafe { (*complex_of(unit, at)).mPreviousPart };
    while !carrier.is_null() {
        result = carrier;
        // SAFETY: a live unit of the chain
        carrier = unsafe { (*complex_of(carrier, at)).mPreviousPart };
    }
    result
}

/// `SetVelocity(value)`: the velocity (written only when it changes) and `mF21C`, at least the stats' `mBC`
/// (the original's `max(value, mBC)`: `value` unless `mBC` is greater).
pub fn set_velocity(cx: &mut EeUnitsComplexUnitFields, bc: f32, value: f32) {
    if value != cx.mVelocity {
        cx.mVelocity = value;
    }
    cx.mF21C = if value > bc { value } else { bc };
}

/// `EEComplexUnit::GetOutermostCarrier`.
///
/// # Safety
/// `unit` and its carriers are live complex units.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cu_outermost_carrier(unit: *mut EeCxxComplexUnit) -> *mut EeCxxComplexUnit {
    // SAFETY: the caller's contract
    unsafe { outermost_carrier(unit, layout().complex_unit_fields) }
}

/// `EEComplexUnit::SetVelocity`: `unit` and `cx` are the unit's two records, its `mStats` a live class upgrade.
///
/// # Safety
/// Both records are the same live complex unit's.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cu_set_velocity(
    unit: *const EeUnitsUnitFields,
    cx: *mut EeUnitsComplexUnitFields,
    value: f32,
) {
    // SAFETY: the caller's contract: the stats object and its record (the layout's offset)
    let bc = unsafe { stats_of((*unit).mStats, layout().stats_fields).mBC };
    // SAFETY: the caller's contract
    set_velocity(unsafe { &mut *cx }, bc, value);
}

/// # Safety
/// `stats` is a live `EEUnitClassUpgrade` and `at` its record's offset.
unsafe fn stats_of<'a>(stats: *mut EeCxxUnitClassUpgrade, at: u64) -> &'a EeUnitsUnitClassUpgradeFields {
    // SAFETY: the caller's contract
    unsafe { &*((stats as usize + at as usize) as *const EeUnitsUnitClassUpgradeFields) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::offset_of;

    #[repr(C)]
    #[derive(Default)]
    struct FakeUnit {
        pad: [u64; 2],
        cx: EeUnitsComplexUnitFields,
    }

    #[test]
    fn the_outermost_carrier_is_the_end_of_the_previous_part_chain() {
        let at = offset_of!(FakeUnit, cx) as u64;
        let (mut a, mut b, mut c) = (FakeUnit::default(), FakeUnit::default(), FakeUnit::default());
        let (pa, pb, pc) = (
            (&mut a as *mut FakeUnit).cast::<EeCxxComplexUnit>(),
            (&mut b as *mut FakeUnit).cast::<EeCxxComplexUnit>(),
            (&mut c as *mut FakeUnit).cast::<EeCxxComplexUnit>(),
        );
        // SAFETY: the fake units live for the test
        unsafe {
            assert_eq!(outermost_carrier(pa, at), pa);
            a.cx.mPreviousPart = pb;
            b.cx.mPreviousPart = pc;
            assert_eq!(outermost_carrier(pa, at), pc);
            assert_eq!(outermost_carrier(pb, at), pc);
        }
    }

    #[test]
    fn the_velocity_keeps_f21c_at_least_the_stats_minimum() {
        let mut cx = EeUnitsComplexUnitFields { mVelocity: 1.0, ..Default::default() };
        set_velocity(&mut cx, 2.0, 3.0);
        assert_eq!((cx.mVelocity, cx.mF21C), (3.0, 3.0));
        set_velocity(&mut cx, 2.0, 0.5);
        assert_eq!((cx.mVelocity, cx.mF21C), (0.5, 2.0));
        // a NaN value: `value > bc` is false, so mF21C takes mBC (as the C++ max macro)
        set_velocity(&mut cx, 2.0, f32::NAN);
        assert!(cx.mVelocity.is_nan());
        assert_eq!(cx.mF21C, 2.0);
    }
}
