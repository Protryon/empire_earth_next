//! The plain members of the placement ghost and the complex doppelganger as Rust records the C++ classes derive from
//! (units round 10): `class EEGhostUnit : public EeUnitsGhostFields` (ee/EEGhostUnit.h) and `class
//! EEComplexDoppelganger : public EEDoppelganger, public EeUnitsComplexDopFields` (ee/EEComplexDoppelganger.h). The
//! fields keep their C++ names, types (`c_long`/`c_ulong` are the C++ `long`/`unsigned long`) and order, so no C++
//! reader changed; the transformations stay C++ members. The pointers are typed `class X*` in C++ (rust/cbindgen.toml's
//! renames, as for the unit records: ee_sim world unit_fields.rs).

#![allow(non_snake_case)]

use crate::world::unit_fields::EeCxxUnitClass;
use std::os::raw::{c_long, c_ulong};

/// `EEGhostUnit`'s plain members (after its vtable; its `mTransformation` follows the record).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsGhostFields {
    /// the footprint's ground heights (Reset zeroes them)
    pub mHeights: [f32; 144],
    pub mTileX: c_long,
    pub mTileY: c_long,
    pub mFrame: c_long,
    /// 7 after Reset
    pub mAnimation: c_long,
    pub mPlayer: *mut EeCxxPlayer,
    /// `gUnitClassTable[inUnitClass]`
    pub mUnitClass: *mut EeCxxUnitClass,
    pub mUnitSet: c_long,
    /// per footprint tile: placeable (UpdateFootprint fills it with mCanPlace; Reset clears it)
    pub mFootprint: [[bool; 12]; 12],
    pub mCanPlace: bool,
    pub mNoMedia: bool,
    pub mU332: i16,
}

/// `EEComplexDoppelganger`'s plain members (its `mTransformation`, the unit's location, follows the record).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsComplexDopFields {
    pub mModel: *mut EeCxxGraphicObject,
    /// the unit's animation
    pub mAnimation: c_ulong,
    /// the unit's model scale
    pub mScale: f32,
    /// the hit points, bit 31 the built flag
    pub mPackedHitPoints: c_long,
    pub mTileX: c_long,
    pub mTileY: c_long,
    /// the unit's model frame
    pub mFrame: c_ulong,
    pub mIsFoundation: bool,
}

impl Default for EeUnitsGhostFields {
    fn default() -> Self {
        // SAFETY: every field is a number, a bool, an array of them or a raw pointer: all zero is each one's default
        unsafe { std::mem::zeroed() }
    }
}

impl Default for EeUnitsComplexDopFields {
    fn default() -> Self {
        // SAFETY: as above
        unsafe { std::mem::zeroed() }
    }
}

/// The C++ classes these records point to (opaque; `class EEPlayer`, `class EEGraphicObject` in the generated header).
pub struct EeCxxPlayer {
    _opaque: [u8; 0],
}
pub struct EeCxxGraphicObject {
    _opaque: [u8; 0],
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::{offset_of, size_of};

    #[test]
    fn the_records_have_the_c_layout_of_their_members() {
        let long = size_of::<c_long>();
        let ptr = size_of::<usize>();
        assert_eq!(offset_of!(EeUnitsGhostFields, mTileX), 144 * 4);
        assert_eq!(offset_of!(EeUnitsGhostFields, mPlayer), (144 * 4 + 4 * long).next_multiple_of(ptr));
        let footprint = offset_of!(EeUnitsGhostFields, mFootprint);
        assert_eq!(offset_of!(EeUnitsGhostFields, mCanPlace), footprint + 144);
        assert_eq!(offset_of!(EeUnitsGhostFields, mU332), footprint + 146);
        assert_eq!(offset_of!(EeUnitsComplexDopFields, mAnimation), ptr);
        assert!(offset_of!(EeUnitsComplexDopFields, mIsFoundation) > offset_of!(EeUnitsComplexDopFields, mFrame));
    }

    #[test]
    fn the_defaults_are_zero_and_null() {
        let (g, d) = (EeUnitsGhostFields::default(), EeUnitsComplexDopFields::default());
        assert!(g.mPlayer.is_null() && g.mUnitClass.is_null() && d.mModel.is_null());
        assert!(g.mHeights.iter().all(|h| *h == 0.0) && !g.mFootprint[11][11] && d.mPackedHitPoints == 0);
    }
}
