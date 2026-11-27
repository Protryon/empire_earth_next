//! The Rust records the C++ game objects derive from, at the world layout's offsets (ee_sim world layout.rs:
//! `EEWorldLayoutRegister` registers them from the build's own `static_cast`s): a unit's `EeUnitsRtsUnitFields`,
//! `EeUnitsUnitFields` and `EeUnitsComplexUnitFields`, a class's `EeUnitsUnitClassFields`, a player's
//! `EeUnitsRtsPlayerFields`, `EeUnitsPlayerSaved` and `EeUnitsPlayerFields` (were EEGameOps.cpp's `UnRts`,
//! `UnFields`, `CxFields`, `UcFields`, `PlRts`, `PlSaved` and `PlFields` cases: one cast each).

use ee_sim::world::layout::{layout, EeWorldLayout};
use ee_sim::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use ee_sim::world::unit_class_fields::EeUnitsUnitClassFields;
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};

use super::P;

/// The record `pick` names in `object` (the object's address plus the record's offset).
fn at<T>(object: P, pick: fn(&EeWorldLayout) -> u64) -> *mut T {
    (object + pick(layout()) as usize) as *mut T
}

/// A unit's `RTSUnit` record.
///
/// # Safety
/// `u` is a live unit (an `RTSUnit*`, `EEUnit*` or `EEComplexUnit*`: the same address).
pub unsafe fn un_rts(u: P) -> *mut EeUnitsRtsUnitFields {
    at(u, |l| l.rts_unit_fields)
}

/// An `EEUnit`'s record.
///
/// # Safety
/// `u` is a live `EEUnit` (or `EEComplexUnit`).
pub unsafe fn un_fields(u: P) -> *mut EeUnitsUnitFields {
    at(u, |l| l.unit_fields)
}

/// An `EEComplexUnit`'s record.
///
/// # Safety
/// `cx` is a live `EEComplexUnit`.
pub unsafe fn cx_fields(cx: P) -> *mut EeUnitsComplexUnitFields {
    at(cx, |l| l.complex_unit_fields)
}

/// An `EEUnitClass`'s record.
///
/// # Safety
/// `uc` is a live unit class.
pub unsafe fn uc_fields(uc: P) -> *mut EeUnitsUnitClassFields {
    at(uc, |l| l.unit_class_fields)
}

/// A player's `RTSPlayer` record.
///
/// # Safety
/// `pl` is a live `EEPlayer`.
pub unsafe fn pl_rts(pl: P) -> *mut EeUnitsRtsPlayerFields {
    at(pl, |l| l.rts_player_fields)
}

/// A player's saved numbers.
///
/// # Safety
/// As [`pl_rts`].
pub unsafe fn pl_saved(pl: P) -> *mut EeUnitsPlayerSaved {
    at(pl, |l| l.player_saved)
}

/// A player's other plain members.
///
/// # Safety
/// As [`pl_rts`].
pub unsafe fn pl_fields(pl: P) -> *mut EeUnitsPlayerFields {
    at(pl, |l| l.player_fields)
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_sim::world::layout::with_test_layout;

    #[test]
    fn the_records_are_the_object_plus_the_layouts_offsets() {
        let _l = with_test_layout(EeWorldLayout {
            rts_unit_fields: 8,
            unit_fields: 0x40,
            complex_unit_fields: 0x100,
            unit_class_fields: 0x18,
            rts_player_fields: 0x20,
            player_saved: 0x200,
            player_fields: 0x300,
            ..Default::default()
        });
        let o: P = 0x1000;
        // SAFETY: only the addresses are computed, nothing is read
        unsafe {
            assert_eq!(un_rts(o) as P, 0x1008);
            assert_eq!(un_fields(o) as P, 0x1040);
            assert_eq!(cx_fields(o) as P, 0x1100);
            assert_eq!(uc_fields(o) as P, 0x1018);
            assert_eq!(pl_rts(o) as P, 0x1020);
            assert_eq!(pl_saved(o) as P, 0x1200);
            assert_eq!(pl_fields(o) as P, 0x1300);
        }
    }
}
