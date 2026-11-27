//! The heads of the simulation's classes whose record bases follow only a vtable pointer, records and such heads
//! (ee_base::record_head; docs/progress/headers.md "Record bases"): a record's offset in its class is its field's
//! `offset_of!` here, a constant. `EEWorldLayoutRegister` checks every head once at start-up
//! ([`ee_world_record_bases_check`]), so a view may read a record at its head's offset instead of a layout entry.

use ee_base::record_head::{self, EeRecordBase, RecordBase, UReferenceCounterHead, VTablePointer};

use crate::tech::nodes::{EeTechNodeFields, EeTechTreeFields};
use crate::world::map_fields::EeUnitsRtsMapFields;
use crate::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use crate::world::stats_fields::{EeUnitsUnitClassUpgradeFields, EeUnitsUnitClassUpgradeLinks};
use crate::world::unit_class_fields::{EeUnitsUnitClassFields, EeUnitsUnitClassLinks};
use crate::world::unit_fields::EeUnitsRtsUnitFields;

ee_base::record_head! {
    /// `RTSUnit : UReferenceCounter, EeUnitsRtsUnitFields`.
    pub struct RtsUnitHead for "RTSUnit" {
        head { counter: UReferenceCounterHead }
        records { fields: EeUnitsRtsUnitFields }
    }
}

ee_base::record_head! {
    /// `EEUnitClass : UReferenceCounter, EeUnitsUnitClassFields, EeUnitsUnitClassLinks`.
    pub struct UnitClassHead for "EEUnitClass" {
        head { counter: UReferenceCounterHead }
        records { fields: EeUnitsUnitClassFields, links: EeUnitsUnitClassLinks }
    }
}

ee_base::record_head! {
    /// `EEUnitClassUpgrade : EeUnitsUnitClassUpgradeLinks, EeUnitsUnitClassUpgradeFields` (virtual: a vtable first).
    pub struct UnitClassUpgradeHead for "EEUnitClassUpgrade" {
        head { vptr: VTablePointer }
        records { links: EeUnitsUnitClassUpgradeLinks, fields: EeUnitsUnitClassUpgradeFields }
    }
}

ee_base::record_head! {
    /// `EEPlayer : EeUnitsRtsPlayerFields, EeUnitsPlayerSaved, EeUnitsPlayerFields` (virtual: a vtable first).
    pub struct PlayerHead for "EEPlayer" {
        head { vptr: VTablePointer }
        records { rts: EeUnitsRtsPlayerFields, saved: EeUnitsPlayerSaved, fields: EeUnitsPlayerFields }
    }
}

ee_base::record_head! {
    /// `RTSMap : FSRestorable, EeUnitsRtsMapFields` (FSRestorable is its vtable pointer).
    pub struct RtsMapHead for "RTSMap" {
        head { vptr: VTablePointer }
        records { fields: EeUnitsRtsMapFields }
    }
}

ee_base::record_head! {
    /// `EETechTreeNode : EeTechNodeFields` (no vtable).
    pub struct TechNodeHead for "EETechTreeNode" {
        records { fields: EeTechNodeFields }
    }
}

ee_base::record_head! {
    /// `EETechTree : EeTechTreeFields` (no vtable).
    pub struct TechTreeHead for "EETechTree" {
        records { fields: EeTechTreeFields }
    }
}

/// Every head's record bases.
pub const HEADS: &[&[RecordBase]] = &[
    RtsUnitHead::BASES,
    UnitClassHead::BASES,
    UnitClassUpgradeHead::BASES,
    PlayerHead::BASES,
    RtsMapHead::BASES,
    TechNodeHead::BASES,
    TechTreeHead::BASES,
];

/// The C++ hands its compiler's record bases (`EE_RECORD_BASE`, ee/EERecordBase.h) once at start-up: false, with each
/// difference on stderr, when one differs from its head's, a head lacks one or a head was not checked.
///
/// # Safety
/// `bases` points to `count` records with NUL-terminated names.
#[no_mangle]
pub unsafe extern "C" fn ee_world_record_bases_check(bases: *const EeRecordBase, count: usize) -> bool {
    // SAFETY: the caller's contract
    let cpp = unsafe { record_head::from_ffi(bases, count) };
    match record_head::check(&cpp, HEADS) {
        Ok(()) => true,
        Err(e) => {
            eprintln!("EE-RECORD-BASES: {e}");
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_class_without_a_vtable_has_its_first_record_at_zero() {
        assert_eq!(TechNodeHead::BASES[0].offset, 0);
        assert_eq!(TechTreeHead::BASES[0].offset, 0);
        assert_eq!(RtsUnitHead::BASES[0].offset, 16, "after UReferenceCounter's vtable pointer and count");
        assert_eq!(PlayerHead::BASES[0].offset, 8);
    }
}
