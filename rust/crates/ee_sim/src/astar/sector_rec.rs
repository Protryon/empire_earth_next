//! The path sectors' and sub sectors' records (`EEAIPathSector`, `EEAIPathSubSector`): every member but the sub
//! sector's tile set, the C++ classes' private bases. A sector holds its sub sectors by id (`std::map<unsigned
//! short, EEAIPathSubSector*>`: an insert of a present id keeps the old one), the continents touching it
//! (`AddNeighbor`: `std::find`, then `push_back`) and its sorted path area ids (`AddPathAreaID`: `binary_search`,
//! `push_back`, `std::sort`; `RemovePathAreaID`: `lower_bound`, erase when equal). A sub sector holds its links
//! (`std::multimap<unsigned short, pair<sub, sub>>`: by id, an insert after the equal ones) and its neighbours. The
//! objects are the C++'s addresses (handles); the continents' neighbours are `continent`'s.
//!
//! RUST_PLAN 2.1: the containers' orders exactly the C++'s.

use std::ffi::c_long;

use crate::astar::area_geom::EeHullRect;
use crate::astar::hull::EeHullPoint;

/// One sub sector of a sector (`mSubSectors`' entry).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarSectorSub {
    pub id: u16,
    /// the `EEAIPathSubSector*`
    pub sub: u64,
}

/// One link of a sub sector (`mLinks`' entry: the other's id, then the two corner sub sectors or 0).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAStarSubSectorLink {
    pub id: u16,
    pub first: u64,
    pub second: u64,
}

/// A sector's lists (opaque to C++).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeAStarSectorLists {
    pub sub_sectors: Vec<EeAStarSectorSub>,
    pub neighbors: Vec<u64>,
    pub path_area_ids: Vec<u16>,
}

/// A sub sector's lists (opaque to C++).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeAStarSubSectorLists {
    pub links: Vec<EeAStarSubSectorLink>,
    pub neighbors: Vec<u64>,
}

/// `EEAIPathSector`'s members.
#[repr(C)]
#[derive(Debug)]
pub struct EeAStarSectorRecord {
    /// the lists (`ee_astar_sector_*`)
    pub lists: *mut EeAStarSectorLists,
    /// `mBounds` (`U2DRectangle<long>`'s layout)
    pub bounds: EeHullRect,
    /// `mCenter` (`U2DPoint<long>`'s layout)
    pub center: EeHullPoint,
    /// `mID`: its index in the manager's sectors
    pub id: u16,
}

/// `EEAIPathSubSector`'s members but the tiles.
#[repr(C)]
#[derive(Debug)]
pub struct EeAStarSubSectorRecord {
    /// the lists (`ee_astar_sub_sector_*`)
    pub lists: *mut EeAStarSubSectorLists,
    /// `mLocation`: the tiles' center
    pub location: EeHullPoint,
    /// `mSector`: the `EEAIPathSector*`
    pub sector: u64,
    /// `mNumTiles`
    pub num_tiles: c_long,
    /// `mContinentID`, `mPathAreaID`, `mID`
    pub continent_id: u16,
    pub path_area_id: u16,
    pub id: u16,
    /// `mKind`: the tile kind
    pub kind: c_long,
    /// `mPlayerID`
    pub player_id: c_long,
    /// `mIsStale`: its path area is being repaired
    pub is_stale: bool,
}

impl EeAStarSectorLists {
    /// `mSubSectors.insert((id, sub))`.
    pub fn insert_sub(&mut self, id: u16, sub: u64) {
        if let Err(at) = self.sub_sectors.binary_search_by(|s| s.id.cmp(&id)) {
            self.sub_sectors.insert(at, EeAStarSectorSub { id, sub });
        }
    }

    /// `AddNeighbor`: unless listed.
    pub fn add_neighbor(&mut self, continent: u64) {
        if !self.neighbors.contains(&continent) {
            self.neighbors.push(continent);
        }
    }

    /// `AddPathAreaID`.
    pub fn add_path_area_id(&mut self, id: u16) {
        if self.path_area_ids.binary_search(&id).is_err() {
            self.path_area_ids.push(id);
            self.path_area_ids.sort_unstable();
        }
    }

    /// `RemovePathAreaID`.
    pub fn remove_path_area_id(&mut self, id: u16) {
        let at = self.path_area_ids.partition_point(|&v| v < id);
        if at < self.path_area_ids.len() && self.path_area_ids[at] == id {
            self.path_area_ids.remove(at);
        }
    }
}

impl EeAStarSubSectorLists {
    /// The links listed under `id` (`lower_bound`..`upper_bound`).
    pub fn links_of(&self, id: u16) -> &[EeAStarSubSectorLink] {
        let lo = self.links.partition_point(|l| l.id < id);
        let hi = self.links.partition_point(|l| l.id <= id);
        &self.links[lo..hi]
    }

    /// `mLinks.insert((id, (first, second)))`: after the links under `id`.
    pub fn insert_link(&mut self, link: EeAStarSubSectorLink) {
        let at = self.links.partition_point(|l| l.id <= link.id);
        self.links.insert(at, link);
    }

    /// `RemoveLinks`: the links under `id` erased.
    pub fn remove_links(&mut self, id: u16) {
        self.links.retain(|l| l.id != id);
    }

    /// `RemoveNeighbor`: every link to `other` (its id) or through it, then `other` from the neighbours (the first);
    /// whether it was a neighbour.
    pub fn remove_neighbor(&mut self, other: u64, other_id: u16) -> bool {
        self.links.retain(|l| !(l.id == other_id || l.first == other || l.second == other));
        match self.neighbors.iter().position(|&n| n == other) {
            Some(at) => {
                self.neighbors.remove(at);
                true
            }
            None => false,
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

unsafe fn sector_lists<'a>(r: *const EeAStarSectorRecord) -> &'a mut EeAStarSectorLists {
    &mut *(*r).lists
}

unsafe fn sub_lists<'a>(r: *const EeAStarSubSectorRecord) -> &'a mut EeAStarSubSectorLists {
    &mut *(*r).lists
}

// ---- the C interface: sectors

/// # Safety
/// `r` points to a record (uninitialised is fine).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_init(r: *mut EeAStarSectorRecord) {
    std::ptr::write(
        r,
        EeAStarSectorRecord {
            lists: Box::into_raw(Box::default()),
            bounds: EeHullRect::default(),
            center: EeHullPoint::default(),
            id: 0,
        },
    );
}

/// # Safety
/// `r` from `ee_astar_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_free(r: *mut EeAStarSectorRecord) {
    let r = &mut *r;
    if !r.lists.is_null() {
        drop(Box::from_raw(r.lists));
        r.lists = std::ptr::null_mut();
    }
}

/// The sector's sub sectors in id order; `count` gets their number. Valid until they change.
///
/// # Safety
/// `r` from `ee_astar_sector_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_subs(
    r: *const EeAStarSectorRecord,
    count: *mut usize,
) -> *const EeAStarSectorSub {
    view(&sector_lists(r).sub_sectors, count)
}

/// `mSubSectors.insert((id, sub))`.
///
/// # Safety
/// `r` from `ee_astar_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_insert_sub(r: *mut EeAStarSectorRecord, id: u16, sub: u64) {
    sector_lists(r).insert_sub(id, sub);
}

/// `mSubSectors.clear()` (the C++ deletes the objects first).
///
/// # Safety
/// `r` from `ee_astar_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_clear_subs(r: *mut EeAStarSectorRecord) {
    sector_lists(r).sub_sectors.clear();
}

/// The continents touching the sector; `count` gets their number.
///
/// # Safety
/// `r` from `ee_astar_sector_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_neighbors(r: *const EeAStarSectorRecord, count: *mut usize) -> *const u64 {
    view(&sector_lists(r).neighbors, count)
}

/// `AddNeighbor` (`unique`), or a plain `push_back` (a saved game's list).
///
/// # Safety
/// `r` from `ee_astar_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_add_neighbor(r: *mut EeAStarSectorRecord, continent: u64, unique: bool) {
    if unique {
        sector_lists(r).add_neighbor(continent);
    } else {
        sector_lists(r).neighbors.push(continent);
    }
}

/// The sector's path area ids; `count` gets their number.
///
/// # Safety
/// `r` from `ee_astar_sector_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_path_area_ids(r: *const EeAStarSectorRecord, count: *mut usize) -> *const u16 {
    view(&sector_lists(r).path_area_ids, count)
}

/// `AddPathAreaID`.
///
/// # Safety
/// `r` from `ee_astar_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_add_path_area_id(r: *mut EeAStarSectorRecord, id: u16) {
    sector_lists(r).add_path_area_id(id);
}

/// `RemovePathAreaID`.
///
/// # Safety
/// `r` from `ee_astar_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_remove_path_area_id(r: *mut EeAStarSectorRecord, id: u16) {
    sector_lists(r).remove_path_area_id(id);
}

/// The path area ids as saved.
///
/// # Safety
/// `r` from `ee_astar_sector_init`; `ids` holds `count` ids (NULL for 0).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sector_set_path_area_ids(r: *mut EeAStarSectorRecord, ids: *const u16, count: usize) {
    let l = &mut sector_lists(r).path_area_ids;
    l.clear();
    if count > 0 {
        l.extend_from_slice(std::slice::from_raw_parts(ids, count));
    }
}

// ---- sub sectors

/// # Safety
/// `r` points to a record (uninitialised is fine).
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_init(r: *mut EeAStarSubSectorRecord) {
    std::ptr::write(
        r,
        EeAStarSubSectorRecord {
            lists: Box::into_raw(Box::default()),
            location: EeHullPoint::default(),
            sector: 0,
            num_tiles: 0,
            continent_id: 0,
            path_area_id: 0,
            id: 0,
            kind: 0,
            player_id: 0,
            is_stale: false,
        },
    );
}

/// # Safety
/// `r` from `ee_astar_sub_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_free(r: *mut EeAStarSubSectorRecord) {
    let r = &mut *r;
    if !r.lists.is_null() {
        drop(Box::from_raw(r.lists));
        r.lists = std::ptr::null_mut();
    }
}

/// Every link in order; `count` gets their number.
///
/// # Safety
/// `r` from `ee_astar_sub_sector_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_links_all(
    r: *const EeAStarSubSectorRecord,
    count: *mut usize,
) -> *const EeAStarSubSectorLink {
    view(&sub_lists(r).links, count)
}

/// The links under `id`; `count` gets their number.
///
/// # Safety
/// `r` from `ee_astar_sub_sector_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_links_of(
    r: *const EeAStarSubSectorRecord,
    id: u16,
    count: *mut usize,
) -> *const EeAStarSubSectorLink {
    view(sub_lists(r).links_of(id), count)
}

/// `mLinks.insert((id, (first, second)))`.
///
/// # Safety
/// `r` from `ee_astar_sub_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_insert_link(
    r: *mut EeAStarSubSectorRecord,
    id: u16,
    first: u64,
    second: u64,
) {
    sub_lists(r).insert_link(EeAStarSubSectorLink { id, first, second });
}

/// `RemoveLinks`.
///
/// # Safety
/// `r` from `ee_astar_sub_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_remove_links(r: *mut EeAStarSubSectorRecord, id: u16) {
    sub_lists(r).remove_links(id);
}

/// `RemoveNeighbor` (`other` the sub sector, `other_id` its id).
///
/// # Safety
/// `r` from `ee_astar_sub_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_remove_neighbor(
    r: *mut EeAStarSubSectorRecord,
    other: u64,
    other_id: u16,
) -> bool {
    sub_lists(r).remove_neighbor(other, other_id)
}

/// The neighbours; `count` gets their number.
///
/// # Safety
/// `r` from `ee_astar_sub_sector_init`; `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_neighbors(
    r: *const EeAStarSubSectorRecord,
    count: *mut usize,
) -> *const u64 {
    view(&sub_lists(r).neighbors, count)
}

/// `mNeighbors.push_back(other)`.
///
/// # Safety
/// `r` from `ee_astar_sub_sector_init`.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_sub_sector_add_neighbor(r: *mut EeAStarSubSectorRecord, other: u64) {
    sub_lists(r).neighbors.push(other);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sector_lists_keep_the_containers_orders() {
        let mut s = EeAStarSectorLists::default();
        s.insert_sub(5, 0x50);
        s.insert_sub(2, 0x20);
        s.insert_sub(5, 0x55);
        assert_eq!(s.sub_sectors, vec![EeAStarSectorSub { id: 2, sub: 0x20 }, EeAStarSectorSub { id: 5, sub: 0x50 }]);
        s.add_neighbor(7);
        s.add_neighbor(3);
        s.add_neighbor(7);
        assert_eq!(s.neighbors, vec![7, 3]);
        for id in [4, 1, 4, 9] {
            s.add_path_area_id(id);
        }
        s.remove_path_area_id(4);
        s.remove_path_area_id(5);
        assert_eq!(s.path_area_ids, vec![1, 9]);
    }

    #[test]
    fn sub_sector_links_are_a_multimap() {
        let mut l = EeAStarSubSectorLists::default();
        l.insert_link(EeAStarSubSectorLink { id: 3, first: 1, second: 0 });
        l.insert_link(EeAStarSubSectorLink { id: 1, first: 2, second: 0 });
        l.insert_link(EeAStarSubSectorLink { id: 3, first: 4, second: 5 });
        assert_eq!(l.links.iter().map(|k| (k.id, k.first)).collect::<Vec<_>>(), vec![(1, 2), (3, 1), (3, 4)]);
        assert_eq!(l.links_of(3).len(), 2);
        assert_eq!(l.links_of(2).len(), 0);
        l.neighbors = vec![9, 8, 9];
        assert!(l.remove_neighbor(9, 1));
        assert_eq!(l.neighbors, vec![8, 9]);
        assert_eq!(l.links.iter().map(|k| (k.id, k.first)).collect::<Vec<_>>(), vec![(3, 1), (3, 4)]);
        assert!(!l.remove_neighbor(5, 7)); // not a neighbour; the links through it go
        assert_eq!(l.links, vec![EeAStarSubSectorLink { id: 3, first: 1, second: 0 }]);
        l.remove_links(3);
        assert!(l.links.is_empty());
        assert!(!l.remove_neighbor(1, 0));
    }
}
