//! The unit picker's bodies (was EEUnitPicker.cpp; game core lane round 9): its walks' host (ee_gfx pick_walk.rs's
//! questions of the units, the classes and the picker), Contains, the two pickers' lock and reference, AddUnit's entry,
//! Clear, Reset and the fog factor. Each C++ call or member is a case of EEUnitPicker.cpp's `EEPickerOp` (chained
//! from EERenderObject.cpp's `EECore9OpA`); the methods forward here. Only a game with a window runs these.

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::{Ge3DPoint, GeTransformation};
use ee_render::ge::rastobj::{EeGeRastColor, EeGeRastFog};
use ee_render::gfx::pick::{admits, EeGfxPickAdd, EeGfxPickUnit};
use ee_render::gfx::pick_walk::{EeGfxPickBoxUnit, EeGfxPickFindUnit, EeGfxPickHost};

use super::{h, hd, hf, hp, pa, ptr, O, P};

/// The picker's entry count (`mCount`).
fn count(picker: P) -> *mut c_ulong {
    hp(O::PkCount, picker, &[]) as *mut c_ulong
}

/// EEUnitPicker::Contains: whether the picker holds a unit (by id, from the last entry).
#[no_mangle]
pub extern "C" fn ee_gamecpp_picker_contains(picker: *mut c_void, unit: *mut c_void) -> bool {
    let (picker, unit) = (picker as P, unit as P);
    let mut found = false;
    // SAFETY: the picker's count
    let n = unsafe { *count(picker) } as c_long;
    if n > 0 {
        let mut i = n - 1;
        while i >= 0 && !found {
            found = h(O::PkEntryId, picker, &[i as i64]) == h(O::RoUnitId, unit, &[]);
            i -= 1;
        }
    }
    found
}

/// EEUnitPicker::PickUnit (`reference`: the unit referenced) and PickUnitUnreferenced: the unit under a screen point,
/// searched under the picker's lock.
///
/// # Safety
/// `point` is a readable screen point (two floats).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_picker_pick(
    picker: *mut c_void,
    point: *const f32,
    prefer_resources: c_long,
    reference: bool,
) -> *mut c_void {
    let picker = picker as P;
    h(O::PkLock, picker, &[]);
    let unit = hp(O::PkFindUnit, picker, &[point as usize as i64, prefer_resources as i64]);
    if reference && unit != 0 {
        h(O::PkUnitReference, unit, &[]);
    }
    h(O::PkUnlock, picker, &[]);
    unit as *mut c_void
}

/// EEUnitPicker::Clear: the entries' models released, the picker emptied.
#[no_mangle]
pub extern "C" fn ee_gamecpp_picker_clear(picker: *mut c_void) {
    let picker = picker as P;
    let n = count(picker);
    // SAFETY: the picker's count
    for i in 0..unsafe { *n } {
        h(O::PkEntryReleaseModel, picker, &[i as i64]);
    }
    // SAFETY: as above
    unsafe { *n = 0 };
}

/// EEUnitPicker::Reset: emptied, then the frame's viewport, projection and see-through mode taken.
#[no_mangle]
pub extern "C" fn ee_gamecpp_picker_reset(picker: *mut c_void) {
    ee_gamecpp_picker_clear(picker);
    h(O::PkTakeView, picker as P, &[]);
}

/// EEUnitPicker::AddUnit: a unit at a transformation when there is room and it is big enough on screen (ee_gfx
/// pick.rs admits): its id, and its complex unit's physical model at the transformation, or its graphic's first model
/// at its stats' scale.
///
/// # Safety
/// `transformation` is a readable transformation (sixteen floats).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_picker_add_unit(
    picker: *mut c_void,
    unit: *mut c_void,
    transformation: *const f32,
    pixel_size: f32,
) {
    let (picker, unit) = (picker as P, unit as P);
    let complex = hp(O::UnGetComplex, unit, &[]);
    let n = count(picker);
    // SAFETY: the picker's count
    let index = unsafe { *n };
    let add = EeGfxPickAdd {
        count: index as u32,
        capacity: h(O::PkCapacity, picker, &[]) as u32,
        pixel_size,
        unit_type: h(O::PkClassUnitType, unit, &[]) as u32,
        category: h(O::PkClassCategory, unit, &[]) as u32,
        projectile: h(O::PkClassProjectile, unit, &[]) != 0,
        free_camera: h(O::FreeCamera, 0, &[]) != 0,
    };
    if !admits(&add) {
        return;
    }
    let entry = hp(O::PkEntry, picker, &[index as i64]);
    let t = transformation as usize as i64;
    h(O::PkEntrySetId, entry, &[h(O::RoUnitId, unit, &[])]);
    if complex != 0 {
        h(O::PkEntryAssignPhysical, entry, &[pa(complex)]);
        h(O::PkEntrySetLocation, entry, &[t]);
    } else {
        h(O::PkEntrySetFrame0, entry, &[]);
        h(O::PkEntrySetModel, entry, &[pa(hp(O::RoGobjModel, hp(O::UnGetModel, unit, &[]), &[0]))]);
        let scale = hd(O::PkStatsScale, unit, &[], &[]);
        h(O::PkEntrySetLocation, entry, &[t]);
        hf(O::PkEntrySetScale, entry, &[], &[scale]);
    }
    // SAFETY: as above
    unsafe { *n += 1 };
}

/// GetFogFactor: a point's fog visibility (linear fog only, which it also turns off; else 1).
pub fn fog_factor(location: Ge3DPoint, fog: c_ulong) -> f32 {
    if fog != 1 {
        return 1.0;
    }
    // GERFogAttributes is ee_render's record (LLE round 9)
    let mut attributes = EeGeRastFog {
        color: EeGeRastColor { red: 0.0, green: 0.0, blue: 0.0 },
        kind: 0,
        density: 0.0,
        start: 0.0,
        end: 0.0,
        scale: 0.0,
    };
    h(O::PkFogAttributes, 0, &[ptr(&mut attributes)]);
    h(O::PkRastFogOff, 0, &[]);
    // SAFETY: the main viewport's camera inverse
    let ci = unsafe { *(hp(O::PkMainCameraInverse, 0, &[]) as *const GeTransformation) };
    let distance = location.y * ci.m[1][1] + location.z * ci.m[1][2] + location.x * ci.m[1][0] + ci.m[1][3];
    if distance <= attributes.start {
        1.0
    } else if distance <= attributes.end {
        1.0 - (distance - attributes.start) * attributes.scale
    } else {
        0.0
    }
}

// ---- the walks' host (ee_gfx pick_walk.rs EeGfxPickHost; the context: the picker)

extern "C" fn pick_unit(picker: *mut c_void, index: u32) -> *mut c_void {
    hp(O::PkEntryUnit, picker as P, &[index as i64]) as *mut c_void
}

/// the facts GetUnitPickPriority reads
fn facts(unit: P) -> EeGfxPickUnit {
    EeGfxPickUnit {
        complex: hp(O::UnGetComplex, unit, &[]) != 0,
        gaia: hp(O::UnPlayer, unit, &[]) == hp(O::WorldPlayer, 0, &[0]),
        wall: h(O::PkClassIsWall, unit, &[]) != 0,
        hit_points: h(O::RoUnitHitPointsValue, unit, &[]) as i32,
        family: h(O::PkClassFamily, unit, &[]) as u32,
        category: h(O::PkClassCategory, unit, &[]) as u32,
        resource_type: h(O::PkUnitResourceType, unit, &[]) as u32,
        resource_amount: h(O::RoUnitResourceAmount, unit, &[]) as i32,
    }
}

extern "C" fn pick_find_unit(_: *mut c_void, unit: *mut c_void) -> EeGfxPickFindUnit {
    let unit = unit as P;
    EeGfxPickFindUnit { facts: facts(unit), tile_x: h(O::PkUnitTileX, unit, &[]) as i32 }
}

extern "C" fn pick_ask(_: *mut c_void, unit: *mut c_void, question: u32) -> bool {
    let unit = unit as P;
    if question == 0 {
        return h(O::UcIsLandBuilding, hp(O::UnClass, unit, &[]), &[]) != 0;
    }
    h(O::PkCurrentPlayerAllied, unit, &[]) != 0
}

extern "C" fn pick_group_nonempty(_: *mut c_void) -> bool {
    h(O::PkGroupNonEmpty, 0, &[]) != 0
}

extern "C" fn pick_pixel_size(picker: *mut c_void, index: u32) -> f32 {
    hd(O::PkPixelSize, picker as P, &[index as i64], &[])
}

/// a part's outermost carrier (the unit itself without a complex unit)
fn outer(unit: P) -> P {
    let mut carrier = hp(O::UnGetComplex, unit, &[]);
    if carrier == 0 {
        return unit;
    }
    while hp(O::CxPreviousPart, carrier, &[]) != 0 {
        carrier = hp(O::CxPreviousPart, carrier, &[]);
    }
    carrier
}

extern "C" fn pick_carrier(_: *mut c_void, unit: *mut c_void) -> *mut c_void {
    outer(unit as P) as *mut c_void
}

extern "C" fn pick_box_unit(_: *mut c_void, unit: *mut c_void) -> EeGfxPickBoxUnit {
    let unit = unit as P;
    let o = outer(unit);
    EeGfxPickBoxUnit {
        complex: hp(O::UnGetComplex, unit, &[]) != 0,
        pickable: h(O::PkClassPickable, unit, &[]) != 0,
        alive: h(O::RoUnitHitPointsValue, unit, &[]) > 0,
        category: h(O::PkClassCategory, unit, &[]) as u32,
        unit_type: h(O::PkClassUnitType, unit, &[]) as u32,
        player: hp(O::UnPlayer, unit, &[]) as *const c_void,
        outer_id: h(O::PkUnitIdField, o, &[]) as u32,
        outer_land_building: h(O::UcIsLandBuilding, hp(O::UnClass, o, &[]), &[]) != 0,
    }
}

extern "C" fn pick_push(_: *mut c_void, out: *mut c_void, id: u32) {
    h(O::PkPushId, out as P, &[id as i64]);
}

/// The walks' host for a picker (EEUnitPicker.cpp's FindUnit and PickBox pass it to ee_gfx pick_walk.rs).
///
/// # Safety
/// `out` is a writable `EeGfxPickHost`.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_picker_host(picker: *mut c_void, out: *mut c_void) {
    let host = EeGfxPickHost {
        context: picker,
        unit: pick_unit,
        find_unit: pick_find_unit,
        ask: pick_ask,
        group_nonempty: pick_group_nonempty,
        pixel_size: pick_pixel_size,
        carrier: pick_carrier,
        box_unit: pick_box_unit,
        push: pick_push,
    };
    // SAFETY: the function's contract
    unsafe { *(out as *mut EeGfxPickHost) = host };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn no_fog_is_full_visibility() {
        assert_eq!(fog_factor(Ge3DPoint::new(1.0, 2.0, 3.0), 0), 1.0);
        assert_eq!(fog_factor(Ge3DPoint::new(1.0, 2.0, 3.0), 2), 1.0);
    }
}
