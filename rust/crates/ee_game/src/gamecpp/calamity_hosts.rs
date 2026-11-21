//! The compound operations of the Rust contagion manager's and the calamity record's C++ hosts (game core lane,
//! round 9; were cases of EEContagionManager.cpp's `ee_contagion_world` and EECalamity.cpp's `EECalamityIoCall`): a
//! unit's reference, its calamity flags, the units near a unit, a unit's immunity, a calamity's index, the saved class
//! and tile. Each C++ call or member is a case of the lane B chain (EEContagionManager.cpp's `EECore9OpB5`); the host
//! cases call these (`ee_gamecpp_contagion_*`, `ee_gamecpp_calamity_io_*`).
//!
//! Simulation code (RUST_PLAN 2.1): the near area's floors are `UMFloor`'s.

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::Ge3DPoint;
use ee_base::math::um;
use ee_sim::tech::contagion_store::{ee_contagion_unit_list_push, EeContagionUnitList};
use ee_sim::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields};

use super::{cptr, h, hp, ptr, rec, Owned, O, P};

/// `kContagionNew` (static_assert'd in EEContagionManager.cpp).
const CG_CONTAGION_NEW: i64 = 4;

/// EE_CONTAGION_WORLD_REFERENCE: the unit referenced (`add`) or dereferenced.
#[no_mangle]
pub extern "C" fn ee_gamecpp_contagion_reference(unit: u64, add: u64) -> i64 {
    h(if add != 0 { O::CgUnitReference } else { O::CgUnitDereference }, unit as P, &[]);
    0
}

/// EE_CONTAGION_WORLD_MARK: the calamity's class flags set on (`set`) or cleared from the unit's complex unit.
#[no_mangle]
pub extern "C" fn ee_gamecpp_contagion_mark(unit: u64, calamity: u64, set: u64) -> i64 {
    let cx = hp(O::UnComplex, unit as P, &[]);
    let flags = h(O::CgClassFlags, calamity as P, &[]) as c_ulong;
    if cx != 0 {
        // SAFETY: the complex unit's record
        let f = unsafe { &mut (*rec::<EeUnitsComplexUnitFields>(O::CxFields, cx)).mCalamityFlags };
        *f = if set != 0 { *f | flags } else { *f & !flags };
    }
    0
}

/// The tile area a radius around a point covers (`U2DRectangle<long>`: bottom, left, right, top).
fn near_area(at: Ge3DPoint, radius: f32) -> [c_long; 4] {
    let left = um::floor(at.x - radius) as c_long;
    let top = um::floor(at.y - radius) as c_long;
    let right = um::floor(at.x + radius) as c_long;
    let bottom = um::floor(at.y + radius) as c_long;
    [bottom, left, right, top]
}

/// EE_CONTAGION_WORLD_NEAR (GetUnitsInArea): the units in the square of the radius around the unit, unless it is
/// inside a carrier.
#[no_mangle]
pub extern "C" fn ee_gamecpp_contagion_near(unit: u64, radius: f32, list: *mut c_void) -> i64 {
    let cx = unit as P;
    let mut at = Ge3DPoint::default();
    h(O::UnPosition, cx, &[ptr(&mut at)]);
    if hp(O::CxContainer, cx, &[]) != 0 {
        return 0;
    }
    let area = near_area(at, radius);
    let units = Owned::new(O::CgUnitsNew, O::CgUnitsDelete);
    h(O::CgUnitsInArea, units.0, &[cptr(&area)]);
    let count = h(O::CgUnitsCount, units.0, &[]);
    for i in 0..count {
        let near = hp(O::CgUnitsAt, units.0, &[i]);
        // SAFETY: the Rust's list
        unsafe { ee_contagion_unit_list_push(list as *mut EeContagionUnitList, near as u64) };
    }
    0
}

/// EE_CONTAGION_WORLD_IMMUNE: the unit is immune (its effect 3 played on it, CONT's record first).
#[no_mangle]
pub extern "C" fn ee_gamecpp_contagion_immune(unit: u64) -> i64 {
    let cx = hp(O::UnComplex, unit as P, &[]);
    let mut effect: P = 0;
    if h(O::CxGetEffect, cx, &[3, ptr(&mut effect)]) == 0 {
        return 0;
    }
    // SAFETY: the unit's record
    let id = unsafe { (*rec::<EeUnitsRtsUnitFields>(O::UnRts, unit as P)).mUnitID };
    h(O::CgCaptureContagion, 0, &[CG_CONTAGION_NEW, id as c_long as i64, 2]); // CONT (TEST ONLY)
    if effect != 0 {
        h(O::EffUnit, cx, &[effect as i64, 0, 0, 0, 0]);
    }
    1
}

/// EE_CONTAGION_WORLD_CALAMITY_INDEX: the calamity's index in the manager (-1: none).
#[no_mangle]
pub extern "C" fn ee_gamecpp_contagion_calamity_index(calamity: u64) -> i64 {
    let mut index: c_long = 0;
    if h(O::CgCalamityIndex, calamity as P, &[ptr(&mut index)]) != 0 {
        index as i64
    } else {
        -1
    }
}

/// EE_CALAMITY_IO_CLASS: the class saved by id, or read by id and looked up.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_io_class(calamity: *mut c_void, file: *mut c_void, loading: bool) -> c_long {
    let (c, f) = (calamity as P, file as P);
    if !loading {
        return h(O::CgWriteClassId, c, &[f as i64]) as c_long;
    }
    let mut id: c_long = 0;
    let result = h(O::CgFileReadLong, f, &[ptr(&mut id)]) as c_long;
    h(O::CgSetClass, c, &[id as i64]);
    result
}

/// EE_CALAMITY_IO_TILE_X / _Y: the place holder's coordinate saved as a long, or read into it.
#[no_mangle]
pub extern "C" fn ee_gamecpp_calamity_io_tile(
    calamity: *mut c_void,
    file: *mut c_void,
    loading: bool,
    y: bool,
) -> c_long {
    let tile = hp(O::CgTileAddr, calamity as P, &[y as i64]) as *mut c_ulong;
    if !loading {
        // SAFETY: the calamity's member
        return h(O::CgFileWriteLong, file as P, &[unsafe { *tile } as c_long as i64]) as c_long;
    }
    let mut value: c_long = 0;
    let result = h(O::CgFileReadLong, file as P, &[ptr(&mut value)]) as c_long;
    // SAFETY: as above
    unsafe { *tile = value as c_ulong };
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_near_area_floors_each_side() {
        assert_eq!(near_area(Ge3DPoint::new(10.5, 20.25, 0.0), 2.0), [22, 8, 12, 18]);
        assert_eq!(near_area(Ge3DPoint::new(0.5, 0.5, 0.0), 1.0), [1, -1, 1, -1]);
    }
}
