//! The unit picker's bodies (was EEUnitPicker.cpp; game core lane round 9): its walks' host (ee_gfx pick_walk.rs's
//! questions of the units, the classes and the picker), Contains, the two pickers' lock and reference, AddUnit's entry,
//! Clear, Reset and the fog factor. Each C++ call or member is a case of EEUnitPicker.cpp's `EEPickerOp` (chained
//! from EERenderObject.cpp's `EECore9OpA`); the methods forward here. Only a game with a window runs these.

use std::ffi::{c_long, c_ulong, c_void};

use ee_base::math::types::{Ge2DPoint, Ge3DPlane, Ge3DPoint, GeTransformation};
use ee_render::ge::physical_walk::EeGePhysicalModels;
use ee_render::ge::rastobj::{EeGeRastColor, EeGeRastFog};
use ee_render::ge::viewport::GeViewport;
use ee_render::gfx::pick::{admits, EeGfxPickAdd, EeGfxPickUnit};
use ee_render::gfx::pick_walk::{
    ee_gfx_pick_box, ee_gfx_pick_find_unit, EeGfxPickBoxUnit, EeGfxPickEntry, EeGfxPickFindUnit, EeGfxPickHost,
    EeGfxPickView,
};

use super::{h, hd, hf, hp, pa, ptr, Owned, O, P};

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
    let unit = find_unit(picker, point as *const Ge2DPoint, prefer_resources);
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
        free_camera: crate::gamecpp::globals::get!(gFreeCamera),
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

/// The walks' host for a picker.
fn host(picker: P) -> EeGfxPickHost {
    EeGfxPickHost {
        context: picker as *mut c_void,
        unit: pick_unit,
        find_unit: pick_find_unit,
        ask: pick_ask,
        group_nonempty: pick_group_nonempty,
        pixel_size: pick_pixel_size,
        carrier: pick_carrier,
        box_unit: pick_box_unit,
        push: pick_push,
    }
}

/// The picker's entries in place (as ee_gfx's records) and their count.
fn entries(picker: P) -> (*mut EeGfxPickEntry, u32) {
    // SAFETY: the picker's count
    (hp(O::PzEntries, picker, &[]) as *mut EeGfxPickEntry, unsafe { *count(picker) } as u32)
}

/// EEUnitPicker::FindUnit: PickUnit's search (the picker's lock held), over the frame's view (ee_gfx pick_walk.rs).
fn find_unit(picker: P, point: *const Ge2DPoint, prefer_resources: c_long) -> P {
    let host = host(picker);
    let view = EeGfxPickView {
        // SAFETY: the picker's viewport (a copy)
        viewport: unsafe { *(hp(O::PzViewport, picker, &[]) as *const GeViewport) },
        orthographic: h(O::PzOrthographic, picker, &[]) != 0,
        show_hidden: h(O::PzShowHidden, picker, &[]) != 0,
        free_camera: h(O::FxFreeCamera, 0, &[]) != 0,
        window_width: h(O::PzWindowWidth, 0, &[]) as u32,
    };
    let (e, n) = entries(picker);
    let models = hp(O::PzModelTable, 0, &[]) as *const EeGePhysicalModels;
    // SAFETY: the picker's entries under its lock, the model table, the caller's point
    unsafe { ee_gfx_pick_find_unit(&host, models, e, n, &view, point, prefer_resources != 0) as P }
}

/// EEUnitPicker::PickBox (PickUnits and PickUnitsInRectangle): under the lock, the ids of the units inside a screen
/// rectangle's frustum (`all`: every pickable unit, else `player`'s of a type) into `out` (a
/// `std::vector<unsigned long>`).
///
/// # Safety
/// `rectangle` is a readable `GERectangle`, `player` a live player or null, `out` a live vector.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_pz_box(
    picker: *mut c_void,
    rectangle: *const c_void,
    all: bool,
    player: *mut c_void,
    kind: c_long,
    out: *mut c_void,
) {
    let picker = picker as P;
    let planes = Owned::new(O::PzPlanesNew, O::PzPlanesDelete);
    let host = host(picker);
    h(O::PkLock, picker, &[]);
    h(O::PzIdsClear, out as P, &[]);
    let (e, n) = entries(picker);
    if n > 0 {
        h(O::PzFrustumPlanes, picker, &[rectangle as usize as i64, pa(planes.0)]);
    }
    let p = if n > 0 { hp(O::PzPlanesData, planes.0, &[]) as *const Ge3DPlane } else { std::ptr::null() };
    let models = hp(O::PzModelTable, 0, &[]) as *const EeGePhysicalModels;
    // SAFETY: the picker's entries under its lock, the planes, the caller's vector
    unsafe { ee_gfx_pick_box(&host, models, e, n, p, all, player, kind as u32, out) };
    h(O::PkUnlock, picker, &[]);
}

/// EEUnitPicker's constructor: its lock, room for `capacity` entries, none yet, the perspective view.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pz_construct(picker: *mut c_void, capacity: c_ulong) {
    let picker = picker as P;
    h(O::PzCreateLock, picker, &[]);
    h(O::PzSetCapacity, picker, &[capacity as i64]);
    h(O::PzNewEntries, picker, &[capacity as i64]);
    // SAFETY: the picker's count
    unsafe { *count(picker) = 0 };
    h(O::PzSetOrthographic, picker, &[0]);
    h(O::PzSetShowHidden, picker, &[0]);
}

/// EEUnitPicker's destructor: its units and entries released, its lock destroyed.
#[no_mangle]
pub extern "C" fn ee_gamecpp_pz_destroy(picker: *mut c_void) {
    ee_gamecpp_picker_clear(picker);
    let picker = picker as P;
    if hp(O::PzEntries, picker, &[]) != 0 {
        h(O::PzDeleteEntries, picker, &[]);
    }
    h(O::PzDestroyLock, picker, &[]);
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
