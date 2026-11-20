//! The continent manager's objects (the AI's areas, path sectors and sub sectors) as Rust allocations: each kind is
//! one `#[repr(C)]` record made and freed here, and the C++ class of that kind (ee/EEAIContinent.h, ...) is a view of
//! it with no vtable and no constructor (its size checked against the record's in the header). An object's handle
//! (`u64`) is its address, which is also its `EEAIArea*`: the area's record first, its tile set's handle after it,
//! then the kind's record (`EEAIContinent : public EEAIArea, private EeAStarContinentRecord`). The towns and herds
//! are C++ objects whose first base is the same `EEAIArea` (ee/EEAIArea.h), so their handles read the same way.
//!
//! Made with every member as the C++ constructors made it (an area's type `kAIAreaNone`, a path area's unit -1),
//! freed with every list and the tile set (a sector with its sub sectors, as `~EEAIPathSector`).

use crate::astar::area::EeAStarAreaRecord;
use crate::astar::area_kinds::{
    ee_astar_choke_free, ee_astar_choke_init, ee_astar_path_area_free, ee_astar_path_area_init, EeAStarChokeRecord,
    EeAStarForestRecord, EeAStarHillRecord, EeAStarPathAreaRecord, EeAStarRectRecord,
};
use crate::astar::cm::EeAStarCmTable;
use crate::astar::cm_io::EeAStarCmLayout;
use crate::astar::continent::{ee_astar_continent_free, ee_astar_continent_init, EeAStarContinentRecord};
use crate::astar::ffi::EeSparseArray;
use crate::astar::sector_rec::{
    ee_astar_sector_free, ee_astar_sector_init, ee_astar_sub_sector_free, ee_astar_sub_sector_init,
    EeAStarSectorRecord, EeAStarSubSectorRecord,
};
use std::mem::offset_of;

/// `EEAIArea`: the record, then `mTiles`' handle (`U2DSparseArrayPointContainer`, a handle and no vtable).
#[repr(C)]
pub struct EeAStarAreaObj {
    pub area: EeAStarAreaRecord,
    pub tiles: *mut EeSparseArray,
}

/// `EEAIContinent`.
#[repr(C)]
pub struct EeAStarContinentObj {
    pub base: EeAStarAreaObj,
    pub rec: EeAStarContinentRecord,
}

/// `EEAIPathArea`.
#[repr(C)]
pub struct EeAStarPathAreaObj {
    pub base: EeAStarAreaObj,
    pub rec: EeAStarPathAreaRecord,
}

/// `EEAIForest`.
#[repr(C)]
pub struct EeAStarForestObj {
    pub base: EeAStarAreaObj,
    pub rec: EeAStarForestRecord,
}

/// `EEAIHill`.
#[repr(C)]
pub struct EeAStarHillObj {
    pub base: EeAStarAreaObj,
    pub rec: EeAStarHillRecord,
}

/// `EEAIChokePoint`.
#[repr(C)]
pub struct EeAStarChokeObj {
    pub base: EeAStarAreaObj,
    pub rec: EeAStarChokeRecord,
}

/// `EEAIRectangleArea`.
#[repr(C)]
pub struct EeAStarRectObj {
    pub base: EeAStarAreaObj,
    pub rec: EeAStarRectRecord,
}

/// `EEAIPathSector`.
#[repr(C)]
pub struct EeAStarSectorObj {
    pub rec: EeAStarSectorRecord,
}

/// `EEAIPathSubSector`: the record, then `mTiles`' handle.
#[repr(C)]
pub struct EeAStarSubSectorObj {
    pub rec: EeAStarSubSectorRecord,
    pub tiles: *mut EeSparseArray,
}

/// Where each record is in its object (cm_io.rs reads the objects through it).
pub const LAYOUT: EeAStarCmLayout = EeAStarCmLayout {
    area_record: offset_of!(EeAStarAreaObj, area),
    area_tiles: offset_of!(EeAStarAreaObj, tiles),
    continent: offset_of!(EeAStarContinentObj, rec),
    path_area: offset_of!(EeAStarPathAreaObj, rec),
    forest: offset_of!(EeAStarForestObj, rec),
    hill: offset_of!(EeAStarHillObj, rec),
    choke: offset_of!(EeAStarChokeObj, rec),
    rect: offset_of!(EeAStarRectObj, rec),
    sector: offset_of!(EeAStarSectorObj, rec),
    sub_sector: offset_of!(EeAStarSubSectorObj, rec),
    sub_tiles: offset_of!(EeAStarSubSectorObj, tiles),
};

fn area_base() -> EeAStarAreaObj {
    EeAStarAreaObj { area: EeAStarAreaRecord::default(), tiles: ee_base::sparse::ee_sparse_new() }
}

fn boxed<T>(v: T) -> u64 {
    Box::into_raw(Box::new(v)) as u64
}

/// A new object of table `t`'s kind; its handle.
pub fn new_object(t: EeAStarCmTable) -> u64 {
    use EeAStarCmTable as T;
    // SAFETY: each `init` writes the whole record it is given
    unsafe {
        match t {
            T::Continents => {
                let mut o = Box::new(EeAStarContinentObj { base: area_base(), rec: std::mem::zeroed() });
                ee_astar_continent_init(&mut o.rec);
                Box::into_raw(o) as u64
            }
            T::PathAreas => {
                let mut o = Box::new(EeAStarPathAreaObj { base: area_base(), rec: std::mem::zeroed() });
                ee_astar_path_area_init(&mut o.rec);
                Box::into_raw(o) as u64
            }
            T::Forests => boxed(EeAStarForestObj { base: area_base(), rec: EeAStarForestRecord::default() }),
            T::Hills => boxed(EeAStarHillObj { base: area_base(), rec: EeAStarHillRecord::default() }),
            T::ChokePoints => {
                let mut o = Box::new(EeAStarChokeObj { base: area_base(), rec: std::mem::zeroed() });
                ee_astar_choke_init(&mut o.rec);
                Box::into_raw(o) as u64
            }
            T::RectangleAreas => boxed(EeAStarRectObj { base: area_base(), rec: EeAStarRectRecord::default() }),
            T::PathSectors => {
                let mut o = Box::new(EeAStarSectorObj { rec: std::mem::zeroed() });
                ee_astar_sector_init(&mut o.rec);
                Box::into_raw(o) as u64
            }
            T::PathSubSectors => {
                let mut o =
                    Box::new(EeAStarSubSectorObj { rec: std::mem::zeroed(), tiles: ee_base::sparse::ee_sparse_new() });
                ee_astar_sub_sector_init(&mut o.rec);
                Box::into_raw(o) as u64
            }
        }
    }
}

unsafe fn free_area(b: &mut EeAStarAreaObj) {
    b.area.free();
    ee_base::sparse::ee_sparse_free(b.tiles);
    b.tiles = std::ptr::null_mut();
}

/// Frees object `h` of table `t`'s kind (0: nothing): `delete` of its C++ class.
///
/// # Safety
/// `h` is 0 or a live handle from [`new_object`] of the same kind, not used after.
pub unsafe fn delete_object(t: EeAStarCmTable, h: u64) {
    use EeAStarCmTable as T;
    if h == 0 {
        return;
    }
    match t {
        T::Continents => {
            let mut o = Box::from_raw(h as *mut EeAStarContinentObj);
            ee_astar_continent_free(&mut o.rec);
            free_area(&mut o.base);
        }
        T::PathAreas => {
            let mut o = Box::from_raw(h as *mut EeAStarPathAreaObj);
            ee_astar_path_area_free(&mut o.rec);
            free_area(&mut o.base);
        }
        T::Forests => free_area(&mut Box::from_raw(h as *mut EeAStarForestObj).base),
        T::Hills => free_area(&mut Box::from_raw(h as *mut EeAStarHillObj).base),
        T::ChokePoints => {
            let mut o = Box::from_raw(h as *mut EeAStarChokeObj);
            ee_astar_choke_free(&mut o.rec);
            free_area(&mut o.base);
        }
        T::RectangleAreas => free_area(&mut Box::from_raw(h as *mut EeAStarRectObj).base),
        T::PathSectors => {
            // `~EEAIPathSector`: DeleteSubSectors, then the record
            let mut o = Box::from_raw(h as *mut EeAStarSectorObj);
            if !o.rec.lists.is_null() {
                let subs = std::mem::take(&mut (*o.rec.lists).sub_sectors);
                for s in subs {
                    delete_object(T::PathSubSectors, s.sub);
                }
            }
            ee_astar_sector_free(&mut o.rec);
        }
        T::PathSubSectors => {
            let mut o = Box::from_raw(h as *mut EeAStarSubSectorObj);
            ee_astar_sub_sector_free(&mut o.rec);
            ee_base::sparse::ee_sparse_free(o.tiles);
        }
    }
}

/// The objects' layout (the C++ checks its classes against it: EEAIRegisterContinentState).
#[no_mangle]
pub extern "C" fn ee_astar_cm_layout() -> EeAStarCmLayout {
    LAYOUT
}

/// A new object of table `t` (`EeAStarCmTable`); its address (the C++ classes' `New`).
#[no_mangle]
pub extern "C" fn ee_astar_object_new(t: EeAStarCmTable) -> u64 {
    new_object(t)
}

/// Frees object `h` of table `t` (the C++ classes' `Delete`).
///
/// # Safety
/// As [`delete_object`].
#[no_mangle]
pub unsafe extern "C" fn ee_astar_object_delete(t: EeAStarCmTable, h: u64) {
    delete_object(t, h)
}

/// The size of table `t`'s object (the C++ headers check their classes against it at load: `sizeof` must match).
#[no_mangle]
pub extern "C" fn ee_astar_object_size(t: EeAStarCmTable) -> usize {
    use EeAStarCmTable as T;
    match t {
        T::Continents => std::mem::size_of::<EeAStarContinentObj>(),
        T::PathAreas => std::mem::size_of::<EeAStarPathAreaObj>(),
        T::Forests => std::mem::size_of::<EeAStarForestObj>(),
        T::Hills => std::mem::size_of::<EeAStarHillObj>(),
        T::ChokePoints => std::mem::size_of::<EeAStarChokeObj>(),
        T::RectangleAreas => std::mem::size_of::<EeAStarRectObj>(),
        T::PathSectors => std::mem::size_of::<EeAStarSectorObj>(),
        T::PathSubSectors => std::mem::size_of::<EeAStarSubSectorObj>(),
    }
}

/// `EEAIContinent::GetPathAreaAt` (`object` a continent) or `EEAIPathSector::GetPathAreaAt` (`sector`), over the
/// registered manager (0 before it is).
///
/// # Safety
/// `object` a live continent or sector of the registered manager.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_object_path_area_at(
    object: u64,
    sector: bool,
    x: std::ffi::c_long,
    y: std::ffi::c_long,
) -> u64 {
    match crate::astar::world_view::cm() {
        Some(r) => crate::astar::cm_query::ee_astar_cm_object_path_area_at(r, object, sector, x, y),
        None => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const ALL: [EeAStarCmTable; 8] = [
        EeAStarCmTable::Continents,
        EeAStarCmTable::PathSectors,
        EeAStarCmTable::PathSubSectors,
        EeAStarCmTable::Forests,
        EeAStarCmTable::PathAreas,
        EeAStarCmTable::Hills,
        EeAStarCmTable::ChokePoints,
        EeAStarCmTable::RectangleAreas,
    ];

    #[test]
    fn objects_are_made_as_the_constructors_made_them_and_freed() {
        for t in ALL {
            let h = new_object(t);
            assert_ne!(h, 0);
            assert!(ee_astar_object_size(t) >= 8);
            if !matches!(t, EeAStarCmTable::PathSectors | EeAStarCmTable::PathSubSectors) {
                // SAFETY: an area kind's handle starts with the area
                let b = unsafe { &*(h as *const EeAStarAreaObj) };
                assert_eq!(b.area.area_type, 0);
                assert!(!b.tiles.is_null());
            }
            if t == EeAStarCmTable::PathAreas {
                // SAFETY: a path area
                assert_eq!(unsafe { (*(h as *const EeAStarPathAreaObj)).rec.unit_id }, -1);
            }
            // SAFETY: the handle just made
            unsafe { delete_object(t, h) };
        }
    }

    #[test]
    fn a_sector_frees_its_sub_sectors() {
        let s = new_object(EeAStarCmTable::PathSectors);
        let sub = new_object(EeAStarCmTable::PathSubSectors);
        // SAFETY: the sector just made
        unsafe { (*(*(s as *mut EeAStarSectorObj)).rec.lists).insert_sub(3, sub) };
        // SAFETY: as above (the sub sector goes with it)
        unsafe { delete_object(EeAStarCmTable::PathSectors, s) };
        assert_eq!(LAYOUT.area_record, 0);
        assert_eq!(LAYOUT.sector, 0);
        assert_eq!(LAYOUT.sub_sector, 0);
    }
}
