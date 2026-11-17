//! A continent's record (`EEAIContinent`, `ee/EEAIContinent.h`): the continent's own members (the area's are
//! `area`'s), the C++ class's first base: its shore tiles, its choke point indices, its seven id lists (forests,
//! path areas, hills and the four region id lists the path area fill collects), the seven chain counts, the
//! category and the id. The lists' upkeep (`AddPathAreaID`, `RemovePathAreaID`, `AddChokePoint`, `AddForestID`,
//! `AddHillID`, `RemoveForestID`, `AddIDBC`..`AddIDEC`) is here; the C++ keeps the neighbours (pointers to its
//! objects) and reads the lists through views of these vectors.
//!
//! RUST_PLAN 2.1: the sorts are the C++'s `std::sort` over `unsigned short`s and `long`s (a total order: any sort
//! gives the same list), `binary_search`/`lower_bound` over the sorted lists, `std::find` from the front.

use std::ffi::c_long;

use crate::astar::hull::EeHullPoint;

/// The id lists, by `EeAStarContinentList`'s numbers.
pub const EE_ASTAR_CONTINENT_LISTS: usize = 7;

/// Which id list (`EEAIContinent`'s members in their order).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeAStarContinentList {
    /// `mForestIDs` (sorted, duplicates kept)
    Forests = 0,
    /// `mPathAreaIDs` (sorted, unique)
    PathAreas = 1,
    /// `mHillIDs` (sorted, duplicates kept)
    Hills = 2,
    /// `mIDsBC`..`mIDsEC` (unique, in the order added)
    IdsBc = 3,
    IdsCc = 4,
    IdsDc = 5,
    IdsEc = 6,
}

/// The lists (opaque to C++).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeAStarContinentLists {
    shore: Vec<EeHullPoint>,
    choke_points: Vec<c_long>,
    ids: [Vec<u16>; EE_ASTAR_CONTINENT_LISTS],
}

/// `EEAIContinent`'s own members (`ee_astar_continent_init` / `ee_astar_continent_free` in its ctor and dtor).
#[repr(C)]
#[derive(Debug)]
pub struct EeAStarContinentRecord {
    /// the lists (`ee_astar_continent_*`)
    pub lists: *mut EeAStarContinentLists,
    /// `mCounts`: the continental chain numbers by chain kind (`NumberContinentChains`)
    pub counts: [u16; 7],
    /// `mCategory`: land, water, space, cliff, map edge (`kContinentCategory*`)
    pub category: u8,
    /// `mID`: the index in the manager's `mContinents`
    pub id: u16,
}

impl EeAStarContinentLists {
    /// `std::sort` after a `push_back` (`AddChokePoint`, `AddForestID`, `AddHillID`).
    fn push_sorted<T: Ord + Copy>(list: &mut Vec<T>, value: T) {
        list.push(value);
        list.sort_unstable();
    }

    /// The shore tiles.
    pub fn shore(&self) -> &[EeHullPoint] {
        &self.shore
    }

    /// The choke point indices.
    pub fn choke_points(&self) -> &[c_long] {
        &self.choke_points
    }

    /// One id list.
    pub fn ids(&self, list: EeAStarContinentList) -> &[u16] {
        &self.ids[list as usize]
    }

    /// `push_back` of a shore tile (the continent grid's).
    pub fn add_shore_tile(&mut self, tile: EeHullPoint) {
        self.shore.push(tile);
    }

    /// `mShoreTiles.clear()`.
    pub fn clear_shore(&mut self) {
        self.shore.clear();
    }

    /// `AddChokePoint(index)`: `push_back`, then `std::sort`.
    pub fn add_choke_point(&mut self, index: c_long) {
        Self::push_sorted(&mut self.choke_points, index);
    }

    /// `AddForestID` / `AddHillID`: `push_back`, then `std::sort` (duplicates kept); `AddPathAreaID`: unless
    /// `binary_search` finds it; `AddIDBC`..`AddIDEC`: `push_back` unless `std::find` finds it.
    pub fn add_id(&mut self, list: EeAStarContinentList, id: u16) {
        let ids = &mut self.ids[list as usize];
        match list {
            EeAStarContinentList::Forests | EeAStarContinentList::Hills => Self::push_sorted(ids, id),
            EeAStarContinentList::PathAreas => {
                if ids.binary_search(&id).is_err() {
                    Self::push_sorted(ids, id);
                }
            }
            _ => {
                if !ids.contains(&id) {
                    ids.push(id);
                }
            }
        }
    }

    /// `RemovePathAreaID` / `RemoveForestID`: `lower_bound`, erased when equal (the first of equals).
    pub fn remove_id(&mut self, list: EeAStarContinentList, id: u16) {
        let ids = &mut self.ids[list as usize];
        let at = ids.partition_point(|&x| x < id);
        if at < ids.len() && ids[at] == id {
            ids.remove(at);
        }
    }

    /// A saved game's lists, as they were.
    pub fn set(&mut self, shore: &[EeHullPoint], choke_points: &[c_long], ids: [&[u16]; EE_ASTAR_CONTINENT_LISTS]) {
        self.shore = shore.to_vec();
        self.choke_points = choke_points.to_vec();
        for (list, values) in self.ids.iter_mut().zip(ids) {
            *list = values.to_vec();
        }
    }
}

impl EeAStarContinentRecord {
    /// The lists (made on first use).
    pub fn lists_mut(&mut self) -> &mut EeAStarContinentLists {
        if self.lists.is_null() {
            self.lists = Box::into_raw(Box::default());
        }
        // SAFETY: `lists` is this record's own box, alive until `free`
        unsafe { &mut *self.lists }
    }

    /// The lists, read only (`None` before any was written).
    pub fn lists(&self) -> Option<&EeAStarContinentLists> {
        // SAFETY: as in `lists_mut`
        unsafe { self.lists.as_ref() }
    }

    /// The lists freed (the dtor's).
    pub fn free(&mut self) {
        if !self.lists.is_null() {
            // SAFETY: from `Box::into_raw` in `lists_mut`, freed once (set NULL here)
            drop(unsafe { Box::from_raw(self.lists) });
            self.lists = std::ptr::null_mut();
        }
    }
}

// ---- the C interface (EEAIContinent's ctor, dtor, list upkeep and views) ----

/// The record of a new continent: no lists, counts zero, category 0, id 0 (the ctor 0x41d846).
///
/// # Safety
/// `r` points to writable memory for a record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_init(r: *mut EeAStarContinentRecord) {
    // SAFETY: the caller's contract
    unsafe { r.write(EeAStarContinentRecord { lists: std::ptr::null_mut(), counts: [0; 7], category: 0, id: 0 }) };
}

/// The record's lists freed (the dtor).
///
/// # Safety
/// `r` a record from `ee_astar_continent_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_free(r: *mut EeAStarContinentRecord) {
    // SAFETY: the caller's contract
    unsafe { (*r).free() };
}

/// The shore tiles and their count (NULL and 0 without any); valid until the shore next changes.
///
/// # Safety
/// `r` a live record, `count` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_shore(
    r: *const EeAStarContinentRecord,
    count: *mut usize,
) -> *const EeHullPoint {
    // SAFETY: the caller's contract
    view(unsafe { (*r).lists().map(EeAStarContinentLists::shore) }, count)
}

/// The choke point indices and their count; valid until they next change.
///
/// # Safety
/// `r` a live record, `count` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_choke_points(
    r: *const EeAStarContinentRecord,
    count: *mut usize,
) -> *const c_long {
    // SAFETY: the caller's contract
    view(unsafe { (*r).lists().map(EeAStarContinentLists::choke_points) }, count)
}

/// One id list and its count; valid until that list next changes.
///
/// # Safety
/// `r` a live record, `count` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_ids(
    r: *const EeAStarContinentRecord,
    list: EeAStarContinentList,
    count: *mut usize,
) -> *const u16 {
    // SAFETY: the caller's contract
    view(unsafe { (*r).lists().map(|l| l.ids(list)) }, count)
}

/// A shore tile appended.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_add_shore_tile(r: *mut EeAStarContinentRecord, tile: EeHullPoint) {
    // SAFETY: the caller's contract
    unsafe { (*r).lists_mut().add_shore_tile(tile) };
}

/// The shore emptied.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_clear_shore(r: *mut EeAStarContinentRecord) {
    // SAFETY: the caller's contract
    unsafe { (*r).lists_mut().clear_shore() };
}

/// `AddChokePoint(index)`.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_add_choke_point(r: *mut EeAStarContinentRecord, index: c_long) {
    // SAFETY: the caller's contract
    unsafe { (*r).lists_mut().add_choke_point(index) };
}

/// `AddForestID`, `AddPathAreaID`, `AddHillID`, `AddIDBC`..`AddIDEC` by list.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_add_id(
    r: *mut EeAStarContinentRecord,
    list: EeAStarContinentList,
    id: u16,
) {
    // SAFETY: the caller's contract
    unsafe { (*r).lists_mut().add_id(list, id) };
}

/// `RemovePathAreaID` / `RemoveForestID` by list.
///
/// # Safety
/// `r` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_remove_id(
    r: *mut EeAStarContinentRecord,
    list: EeAStarContinentList,
    id: u16,
) {
    // SAFETY: the caller's contract
    unsafe { (*r).lists_mut().remove_id(list, id) };
}

/// The lists of a saved game: the shore, the choke points and the seven id lists (`ids[i]` holds `id_counts[i]`).
///
/// # Safety
/// `r` a live record; each pointer holds its count (NULL when 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_continent_set_lists(
    r: *mut EeAStarContinentRecord,
    shore: *const EeHullPoint,
    shore_count: usize,
    choke_points: *const c_long,
    choke_count: usize,
    ids: *const *const u16,
    id_counts: *const usize,
) {
    let mut lists: [&[u16]; EE_ASTAR_CONTINENT_LISTS] = [&[]; EE_ASTAR_CONTINENT_LISTS];
    for (i, list) in lists.iter_mut().enumerate() {
        // SAFETY: the caller's contract (seven pointers and counts)
        *list = unsafe { slice(*ids.add(i), *id_counts.add(i)) };
    }
    // SAFETY: the caller's contract
    unsafe { (*r).lists_mut().set(slice(shore, shore_count), slice(choke_points, choke_count), lists) };
}

fn view<T>(list: Option<&[T]>, count: *mut usize) -> *const T {
    let list = list.unwrap_or(&[]);
    // SAFETY: the callers' contract (`count` valid)
    unsafe { *count = list.len() };
    if list.is_empty() {
        std::ptr::null()
    } else {
        list.as_ptr()
    }
}

/// # Safety
/// `values` holds `count` elements, or `count` is 0.
unsafe fn slice<'a, T>(values: *const T, count: usize) -> &'a [T] {
    if count == 0 || values.is_null() {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(values, count) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use EeAStarContinentList as L;

    fn record() -> EeAStarContinentRecord {
        let mut r = std::mem::MaybeUninit::<EeAStarContinentRecord>::uninit();
        // SAFETY: init writes the whole record
        unsafe {
            ee_astar_continent_init(r.as_mut_ptr());
            r.assume_init()
        }
    }

    fn ids(r: &EeAStarContinentRecord, list: L) -> Vec<u16> {
        let mut count = 0;
        // SAFETY: a live record
        let p = unsafe { ee_astar_continent_ids(r, list, &mut count) };
        // SAFETY: the view's contract
        unsafe { slice(p, count) }.to_vec()
    }

    #[test]
    fn a_new_continent_has_no_lists() {
        let mut r = record();
        assert!(r.lists.is_null() && r.counts == [0; 7] && r.category == 0 && r.id == 0);
        let mut count = 5;
        // SAFETY: a live record
        assert!(unsafe { ee_astar_continent_shore(&r, &mut count) }.is_null());
        assert_eq!(count, 0);
        assert!(ids(&r, L::Hills).is_empty());
        r.free();
        r.free();
    }

    #[test]
    fn the_sorted_lists_keep_the_cpp_rules() {
        let mut r = record();
        // AddForestID / AddHillID: push_back and sort, duplicates kept
        for id in [5, 2, 5, 9] {
            // SAFETY: a live record
            unsafe { ee_astar_continent_add_id(&mut r, L::Forests, id) };
        }
        assert_eq!(ids(&r, L::Forests), vec![2, 5, 5, 9]);
        // RemoveForestID: the first of equals, nothing when absent
        // SAFETY: a live record
        unsafe {
            ee_astar_continent_remove_id(&mut r, L::Forests, 5);
            ee_astar_continent_remove_id(&mut r, L::Forests, 4);
        }
        assert_eq!(ids(&r, L::Forests), vec![2, 5, 9]);
        // AddPathAreaID: unless present
        for id in [7, 3, 7, 1] {
            // SAFETY: a live record
            unsafe { ee_astar_continent_add_id(&mut r, L::PathAreas, id) };
        }
        assert_eq!(ids(&r, L::PathAreas), vec![1, 3, 7]);
        // AddIDBC..EC: unique, in the order added
        for id in [4, 1, 4, 0] {
            // SAFETY: a live record
            unsafe { ee_astar_continent_add_id(&mut r, L::IdsDc, id) };
        }
        assert_eq!(ids(&r, L::IdsDc), vec![4, 1, 0]);
        assert!(ids(&r, L::IdsBc).is_empty());
        // AddChokePoint: push_back and sort
        for index in [3, -1, 3] {
            // SAFETY: a live record
            unsafe { ee_astar_continent_add_choke_point(&mut r, index) };
        }
        let mut count = 0;
        // SAFETY: a live record and the view's contract
        let chokes = unsafe { slice(ee_astar_continent_choke_points(&r, &mut count), count) }.to_vec();
        assert_eq!(chokes, vec![-1, 3, 3]);
        r.free();
    }

    #[test]
    fn the_shore_and_a_saved_game_s_lists() {
        let mut r = record();
        // SAFETY: a live record
        unsafe {
            ee_astar_continent_add_shore_tile(&mut r, EeHullPoint::new(3, 4));
            ee_astar_continent_add_shore_tile(&mut r, EeHullPoint::new(1, 2));
        }
        assert_eq!(r.lists().unwrap().shore(), &[EeHullPoint::new(3, 4), EeHullPoint::new(1, 2)]);
        // SAFETY: a live record
        unsafe { ee_astar_continent_clear_shore(&mut r) };
        assert!(r.lists().unwrap().shore().is_empty());
        let lists: [Vec<u16>; 7] = [vec![1], vec![2, 3], vec![], vec![4], vec![], vec![5, 6], vec![7]];
        let pointers: Vec<*const u16> =
            lists.iter().map(|l| if l.is_empty() { std::ptr::null() } else { l.as_ptr() }).collect();
        let counts: Vec<usize> = lists.iter().map(Vec::len).collect();
        let shore = [EeHullPoint::new(9, 9)];
        // SAFETY: a live record; seven lists
        unsafe {
            ee_astar_continent_set_lists(
                &mut r,
                shore.as_ptr(),
                1,
                [8 as c_long].as_ptr(),
                1,
                pointers.as_ptr(),
                counts.as_ptr(),
            )
        };
        assert_eq!(r.lists().unwrap().shore(), &shore);
        assert_eq!(r.lists().unwrap().choke_points(), &[8]);
        for (i, list) in
            [L::Forests, L::PathAreas, L::Hills, L::IdsBc, L::IdsCc, L::IdsDc, L::IdsEc].into_iter().enumerate()
        {
            assert_eq!(ids(&r, list), lists[i]);
        }
        r.free();
    }
}
