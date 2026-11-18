//! The other area kinds' records: `EEAIPathArea`, `EEAIChokePoint`, `EEAIForest`, `EEAIHill` and
//! `EEAIRectangleArea`'s own members (the area's are `area`'s), each the C++ class's private base like
//! `EEAIContinent`'s record (`continent`). The lists are Rust behind opaque handles: a path area's sorted sector
//! ids (`AddSectorID`: `binary_search`, then `push_back` and `std::sort`), a choke point's area serials
//! (`AddAreaSerials` appends). The forest's resource kind (`UpdateResourceKind`) and its counts' upkeep when a tile
//! goes (`RemoveTile`) are here.
//!
//! RUST_PLAN 2.1: `long` is `c_long`, the C++'s; the orders are the C++'s.

use std::ffi::{c_long, c_ulong};

use crate::astar::area_geom::EeHullRect;

/// A list of ids (opaque to C++).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeAStarIdList(pub Vec<u16>);

/// A list of area serials (opaque to C++).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeAStarSerialList(pub Vec<c_ulong>);

/// `EEAIPathArea`'s members.
#[repr(C)]
#[derive(Debug)]
pub struct EeAStarPathAreaRecord {
    /// `mSectorIDs`: the path sectors it is in (sorted, unique)
    pub sector_ids: *mut EeAStarIdList,
    /// `mContinentID`
    pub continent_id: u16,
    /// `mID`: the index in the manager's path areas
    pub id: u16,
    /// `mKind`: the tile kind (`EEAIGetTileKind`)
    pub kind: c_long,
    /// `mUnitID`: the wall or gate of an obstacle area, -1
    pub unit_id: c_long,
    /// the region ids by kind
    pub surface_region: u16,
    pub water_body: u16,
    pub land_mass: u16,
    pub space_region: u16,
}

/// `EEAIChokePoint`'s members.
#[repr(C)]
#[derive(Debug)]
pub struct EeAStarChokeRecord {
    /// `mAreaSerials`: the areas it joins
    pub area_serials: *mut EeAStarSerialList,
    /// `mIndex`: its index in the manager's choke points
    pub index: c_long,
    /// `mContinentID`
    pub continent_id: u16,
}

/// `EEAIForest`'s members.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarForestRecord {
    /// `mResourceKind`: the highest kind (1..3) with trees
    pub resource_kind: c_long,
    /// the trees by kind 1, 2, 3
    pub num_resources1: c_long,
    pub num_resources2: c_long,
    pub num_resources3: c_long,
    /// `mL78` (only zeroed)
    pub l78: c_long,
    /// `mNextRenewal`: the game time of the next regrowth
    pub next_renewal: c_ulong,
    /// `mContinentID`, `mID`
    pub continent_id: u16,
    pub id: u16,
}

/// `EEAIHill`'s members.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarHillRecord {
    pub continent_id: u16,
    pub id: u16,
}

/// `EEAIRectangleArea`'s members.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarRectRecord {
    /// `mRect`: the scenario area's tiles
    pub rect: EeHullRect,
    /// `mScenarioAreaID`
    pub scenario_area_id: c_long,
    /// the tiles by continent category (`UpdateContinents`)
    pub num_land_tiles: c_long,
    pub num_water_tiles: c_long,
    pub num_space_tiles: c_long,
    /// `mContinentID`: the continent with most of its tiles
    pub continent_id: u16,
    /// `mID`
    pub id: u16,
}

impl EeAStarPathAreaRecord {
    pub fn sector_ids(&self) -> &[u16] {
        if self.sector_ids.is_null() {
            &[]
        } else {
            // SAFETY: made by init, freed by free
            unsafe { &(*self.sector_ids).0 }
        }
    }

    /// `AddSectorID`: unless present (`binary_search`), `push_back` + `std::sort`.
    pub fn add_sector_id(&mut self, id: u16) {
        // SAFETY: made by init
        let list = unsafe { &mut (*self.sector_ids).0 };
        if list.binary_search(&id).is_err() {
            list.push(id);
            list.sort_unstable();
        }
    }
}

impl EeAStarForestRecord {
    /// `UpdateResourceKind`: the highest kind with a positive count; unchanged when none has any.
    pub fn update_resource_kind(&mut self) {
        for (kind, count) in [(1, self.num_resources1), (2, self.num_resources2), (3, self.num_resources3)] {
            if count > 0 {
                self.resource_kind = kind;
            }
        }
    }

    /// `RemoveTile`'s count: the removed tile's tree kind (1..3; else none) loses one.
    pub fn remove_resource(&mut self, kind: c_long) {
        match kind {
            1 => self.num_resources1 = self.num_resources1.wrapping_sub(1),
            2 => self.num_resources2 = self.num_resources2.wrapping_sub(1),
            3 => self.num_resources3 = self.num_resources3.wrapping_sub(1),
            _ => {}
        }
    }
}

fn view<T>(list: &[T], count: *mut usize) -> *const T {
    // SAFETY: the callers' contract (count writable)
    unsafe { *count = list.len() };
    if list.is_empty() {
        std::ptr::null()
    } else {
        list.as_ptr()
    }
}

// ---- the C interface

/// # Safety
/// `r` points to a record (uninitialised is fine).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_area_init(r: *mut EeAStarPathAreaRecord) {
    std::ptr::write(
        r,
        EeAStarPathAreaRecord {
            sector_ids: Box::into_raw(Box::default()),
            continent_id: 0,
            id: 0,
            kind: 0,
            unit_id: -1,
            surface_region: 0,
            water_body: 0,
            land_mass: 0,
            space_region: 0,
        },
    );
}

/// # Safety
/// `r` from `ee_astar_path_area_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_area_free(r: *mut EeAStarPathAreaRecord) {
    let r = &mut *r;
    if !r.sector_ids.is_null() {
        drop(Box::from_raw(r.sector_ids));
        r.sector_ids = std::ptr::null_mut();
    }
}

/// A path area's sector ids; `count` gets their number. Valid until they change.
///
/// # Safety
/// `r` from `ee_astar_path_area_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_area_sector_ids(
    r: *const EeAStarPathAreaRecord,
    count: *mut usize,
) -> *const u16 {
    view((*r).sector_ids(), count)
}

/// `AddSectorID`.
///
/// # Safety
/// `r` from `ee_astar_path_area_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_area_add_sector_id(r: *mut EeAStarPathAreaRecord, id: u16) {
    (*r).add_sector_id(id);
}

/// The sector ids as they were saved.
///
/// # Safety
/// `r` from `ee_astar_path_area_init`; `ids` holds `count` ids (NULL for 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_path_area_set_sector_ids(
    r: *mut EeAStarPathAreaRecord,
    ids: *const u16,
    count: usize,
) {
    let list = &mut (*(*r).sector_ids).0;
    list.clear();
    if count > 0 {
        list.extend_from_slice(std::slice::from_raw_parts(ids, count));
    }
}

/// # Safety
/// `r` points to a record (uninitialised is fine).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_choke_init(r: *mut EeAStarChokeRecord) {
    std::ptr::write(r, EeAStarChokeRecord { area_serials: Box::into_raw(Box::default()), index: 0, continent_id: 0 });
}

/// # Safety
/// `r` from `ee_astar_choke_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_choke_free(r: *mut EeAStarChokeRecord) {
    let r = &mut *r;
    if !r.area_serials.is_null() {
        drop(Box::from_raw(r.area_serials));
        r.area_serials = std::ptr::null_mut();
    }
}

/// A choke point's area serials; `count` gets their number. Valid until they change.
///
/// # Safety
/// `r` from `ee_astar_choke_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_choke_area_serials(
    r: *const EeAStarChokeRecord,
    count: *mut usize,
) -> *const c_ulong {
    view(&(*(*r).area_serials).0, count)
}

/// `AddAreaSerials` (appended); `clear` empties the list first (a saved game's).
///
/// # Safety
/// `r` from `ee_astar_choke_init`; `serials` holds `count` serials (NULL for 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_choke_add_area_serials(
    r: *mut EeAStarChokeRecord,
    serials: *const c_ulong,
    count: usize,
    clear: bool,
) {
    let list = &mut (*(*r).area_serials).0;
    if clear {
        list.clear();
    }
    if count > 0 {
        list.extend_from_slice(std::slice::from_raw_parts(serials, count));
    }
}

/// `UpdateResourceKind`.
///
/// # Safety
/// `r` a forest's record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_forest_update_resource_kind(r: *mut EeAStarForestRecord) {
    (*r).update_resource_kind();
}

/// `RemoveTile`'s count upkeep for a tile of tree kind `kind`.
///
/// # Safety
/// `r` a forest's record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_forest_remove_resource(r: *mut EeAStarForestRecord, kind: c_long) {
    (*r).remove_resource(kind);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn path_area_sector_ids_sorted_unique() {
        let mut r = std::mem::MaybeUninit::<EeAStarPathAreaRecord>::uninit();
        // SAFETY: init writes it whole
        let r = unsafe {
            ee_astar_path_area_init(r.as_mut_ptr());
            &mut *r.as_mut_ptr()
        };
        assert_eq!(r.unit_id, -1);
        for id in [5, 2, 5, 9, 2] {
            r.add_sector_id(id);
        }
        assert_eq!(r.sector_ids(), &[2, 5, 9]);
        // SAFETY: an initialised record
        unsafe {
            ee_astar_path_area_set_sector_ids(r, [7u16, 1].as_ptr(), 2);
            let mut n = 0;
            let p = ee_astar_path_area_sector_ids(r, &mut n);
            assert_eq!(std::slice::from_raw_parts(p, n), &[7, 1]);
            ee_astar_path_area_free(r);
        }
    }

    #[test]
    fn choke_serials_append() {
        let mut r = std::mem::MaybeUninit::<EeAStarChokeRecord>::uninit();
        // SAFETY: init writes it whole
        unsafe {
            ee_astar_choke_init(r.as_mut_ptr());
            let r = &mut *r.as_mut_ptr();
            ee_astar_choke_add_area_serials(r, [4 as c_ulong, 3].as_ptr(), 2, false);
            ee_astar_choke_add_area_serials(r, [4 as c_ulong].as_ptr(), 1, false);
            let mut n = 0;
            let p = ee_astar_choke_area_serials(r, &mut n);
            assert_eq!(std::slice::from_raw_parts(p, n), &[4, 3, 4]);
            ee_astar_choke_add_area_serials(r, std::ptr::null(), 0, true);
            ee_astar_choke_area_serials(r, &mut n);
            assert_eq!(n, 0);
            ee_astar_choke_free(r);
        }
    }

    #[test]
    fn forest_resource_kind() {
        let mut f =
            EeAStarForestRecord { num_resources1: 2, num_resources2: 0, num_resources3: 1, ..Default::default() };
        f.update_resource_kind();
        assert_eq!(f.resource_kind, 3);
        f.remove_resource(3);
        f.remove_resource(7);
        assert_eq!(f.num_resources3, 0);
        f.update_resource_kind();
        assert_eq!(f.resource_kind, 1);
        let mut g = EeAStarForestRecord { resource_kind: 2, ..Default::default() };
        g.update_resource_kind();
        assert_eq!(g.resource_kind, 2);
    }
}
