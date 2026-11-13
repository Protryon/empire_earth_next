//! The map's unit regions (R5b.2, fifteenth round; was `RTSRegionTable<EEComplexUnit*>`, `EEMap::mUnitRegions`,
//! ctor 0x11e05e): the map cut into square regions of `size` tiles, each the list of the complex units on its tiles
//! in the order they entered (a unit is appended when it enters a region and the first copy erased when it leaves).
//! The searches walk a region's units in that order and take the first match, so the order is game state: a save
//! keeps it (`order`, `restore`, save version 0.257).
//!
//! The table is a Rust object the map owns (`EEMap::mUnitRegions` is its pointer); every handle a C++ unit pointer
//! (0 for a part with no complex unit, as the original's `GetComplexUnit()` gave NULL). The callers in C++ read a
//! region as a slice (`ee_combat_regions_units`), valid until the table next changes, as the original's vector was.

use crate::walks::EeCombatHandle;

/// The unit regions of a map.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeCombatUnitRegions {
    /// tiles per region side (the callers divide tile coordinates by it)
    size: i32,
    /// regions per side
    side: u32,
    /// the regions, column-major as the original's array (`x * side + y`)
    regions: Vec<Vec<EeCombatHandle>>,
}

impl EeCombatUnitRegions {
    /// `side * side` empty regions of `size` tiles (the C++ computes `side` as the original's ceiling of the map's
    /// size over `size`).
    pub fn new(side: u32, size: i32) -> Self {
        let count = (side as usize) * (side as usize);
        EeCombatUnitRegions { size, side, regions: vec![Vec::new(); count] }
    }

    pub fn size(&self) -> i32 {
        self.size
    }

    /// The number of regions.
    pub fn count(&self) -> usize {
        self.regions.len()
    }

    /// `GetRegion(x, y)`: none off the grid (the coordinates unsigned, as the original's compare).
    pub fn region(&self, x: u32, y: u32) -> Option<&Vec<EeCombatHandle>> {
        if x < self.side && y < self.side {
            Some(&self.regions[x as usize * self.side as usize + y as usize])
        } else {
            None
        }
    }

    fn region_mut(&mut self, x: u32, y: u32) -> Option<&mut Vec<EeCombatHandle>> {
        if x < self.side && y < self.side {
            Some(&mut self.regions[x as usize * self.side as usize + y as usize])
        } else {
            None
        }
    }

    /// The region at `index` of the array.
    pub fn at(&self, index: usize) -> Option<&Vec<EeCombatHandle>> {
        self.regions.get(index)
    }

    /// `Add`: appends `unit` to region (x, y) when there is one.
    pub fn add(&mut self, x: u32, y: u32, unit: EeCombatHandle) {
        if let Some(r) = self.region_mut(x, y) {
            r.push(unit);
        }
    }

    /// `footprint` of the tiles (x, y) to (x + size - 1, y + size - 1): `unit` added to (`add`) or removed from every
    /// region under them (`EEAddBuildingRegion`, `EERemoveBuildingRegion`).
    pub fn footprint(&mut self, x: i32, y: i32, size: i32, unit: EeCombatHandle, add: bool) {
        let last = size.wrapping_sub(1);
        let (left, top, right, bottom) = self.rect(x, y, x.wrapping_add(last), y.wrapping_add(last));
        for rx in left..=right {
            for ry in top..=bottom {
                if add {
                    self.add(rx as u32, ry as u32, unit);
                } else {
                    self.remove(rx as u32, ry as u32, unit);
                }
            }
        }
    }

    /// `RTSRegion::Remove`: erases the first copy of `unit` from region (x, y) (the others keep their order).
    pub fn remove(&mut self, x: u32, y: u32, unit: EeCombatHandle) {
        if let Some(r) = self.region_mut(x, y) {
            if let Some(i) = r.iter().position(|&u| u == unit) {
                r.remove(i);
            }
        }
    }

    /// `GetRegions(tiles)`: the rectangle's edges in regions (C's truncating division).
    pub fn rect(&self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32) {
        (left / self.size, top / self.size, right / self.size, bottom / self.size)
    }

    /// `GetValues(tiles)`: the units of the regions under the rectangle (its low edges at least 0), the regions
    /// column by column, then walked from the last region and each from its last unit.
    pub fn values(&self, left: i32, top: i32, right: i32, bottom: i32) -> Vec<EeCombatHandle> {
        let (min_x, min_y, max_x, max_y) = self.rect(left, top, right, bottom);
        let (min_x, min_y) = (min_x.max(0), min_y.max(0));
        let mut found: Vec<&Vec<EeCombatHandle>> = Vec::new();
        for x in min_x..=max_x {
            for y in min_y..=max_y {
                if let Some(r) = self.region(x as u32, y as u32) {
                    found.push(r);
                }
            }
        }
        found.iter().rev().flat_map(|r| r.iter().rev().copied()).collect()
    }

    /// `CollectValues(tiles, values, std::less)`: [`Self::values`] sorted by handle (the original's pointer order)
    /// without duplicates.
    pub fn collect(&self, left: i32, top: i32, right: i32, bottom: i32) -> Vec<EeCombatHandle> {
        let mut v = self.values(left, top, right, bottom);
        v.sort_unstable();
        v.dedup();
        v
    }

    /// The saved order: the number of regions holding two units or more, then for each its index, its size and its
    /// units' ids (`id_of`; -1 for none).
    pub fn order(&self, id_of: impl Fn(EeCombatHandle) -> u32) -> Vec<u32> {
        let mut out = vec![0u32];
        for (index, r) in self.regions.iter().enumerate() {
            if r.len() < 2 {
                continue;
            }
            out[0] += 1;
            out.push(index as u32);
            out.push(r.len() as u32);
            out.extend(r.iter().map(|&u| if u != 0 { id_of(u) } else { u32::MAX }));
        }
        out
    }

    /// Puts back a saved region's order: when region `index` (one of the table's) holds the units of `ids` (the same
    /// number, each id's first unit not yet taken), they are put in that order and true is answered; otherwise the
    /// region is left as it is (false).
    pub fn restore(&mut self, index: usize, ids: &[u32], id_of: impl Fn(EeCombatHandle) -> u32) -> bool {
        let region = &mut self.regions[index];
        if region.len() != ids.len() {
            return false;
        }
        let mut taken = vec![false; region.len()];
        let mut units = Vec::with_capacity(region.len());
        for &id in ids {
            let Some(n) = (0..region.len()).find(|&n| !taken[n] && region[n] != 0 && id_of(region[n]) == id) else {
                return false;
            };
            taken[n] = true;
            units.push(region[n]);
        }
        *region = units;
        true
    }
}

/// A unit's id for the saved order (the unit store's `mUnitID`; -1 without a record).
fn unit_id(unit: EeCombatHandle) -> u32 {
    crate::store::unit_of(unit).map_or(u32::MAX, |r| r.id)
}

// ---- the C interface: `table` is a pointer `ee_combat_regions_new` returned and not yet freed (the map's)

/// A new table of `side * side` regions of `size` tiles (`EEMap`'s; freed by `ee_combat_regions_free`).
#[no_mangle]
pub extern "C" fn ee_combat_regions_new(side: u32, size: i32) -> *mut EeCombatUnitRegions {
    Box::into_raw(Box::new(EeCombatUnitRegions::new(side, size)))
}

/// Frees a table (none for null).
///
/// # Safety
/// `table` is null or a live table, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_free(table: *mut EeCombatUnitRegions) {
    if !table.is_null() {
        drop(unsafe { Box::from_raw(table) });
    }
}

/// The tiles per region side.
///
/// # Safety
/// `table` is a live table.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_size(table: *const EeCombatUnitRegions) -> i32 {
    unsafe { &*table }.size()
}

/// Region (x, y)'s units, its count in `len` (null and -1 off the grid); valid until the table next changes.
///
/// # Safety
/// `table` is a live table, `len` a valid pointer.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_units(
    table: *const EeCombatUnitRegions,
    x: u32,
    y: u32,
    len: *mut i32,
) -> *const EeCombatHandle {
    let (units, n) = match unsafe { &*table }.region(x, y) {
        Some(r) => (r.as_ptr(), r.len() as i32),
        None => (std::ptr::null(), -1),
    };
    unsafe { *len = n };
    units
}

/// Region (x, y)'s number of units (-1 off the grid).
///
/// # Safety
/// `table` is a live table.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_len(table: *const EeCombatUnitRegions, x: u32, y: u32) -> i32 {
    unsafe { &*table }.region(x, y).map_or(-1, |r| r.len() as i32)
}

/// Appends `unit` to region (x, y) when there is one.
///
/// # Safety
/// `table` is a live table.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_add(table: *mut EeCombatUnitRegions, x: u32, y: u32, unit: u64) {
    unsafe { &mut *table }.add(x, y, unit);
}

/// Erases the first copy of `unit` from region (x, y).
///
/// # Safety
/// `table` is a live table.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_remove(table: *mut EeCombatUnitRegions, x: u32, y: u32, unit: u64) {
    unsafe { &mut *table }.remove(x, y, unit);
}

/// `unit` added to (`add`) or removed from every region under the `size` by `size` tiles at (x, y).
///
/// # Safety
/// `table` is a live table.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_footprint(
    table: *mut EeCombatUnitRegions,
    x: i32,
    y: i32,
    size: i32,
    unit: u64,
    add: bool,
) {
    unsafe { &mut *table }.footprint(x, y, size, unit, add);
}

/// Copies up to `cap` values into `out` (none when null); answers how many there are.
///
/// # Safety
/// `out` is null or holds `cap` values.
unsafe fn copy_out<T: Copy>(values: &[T], out: *mut T, cap: usize) -> usize {
    if !out.is_null() {
        let n = values.len().min(cap);
        unsafe { std::ptr::copy_nonoverlapping(values.as_ptr(), out, n) };
    }
    values.len()
}

/// `CollectValues`: the units of the regions under the tiles (left, top) to (right, bottom), sorted, no duplicates,
/// into `out` (up to `cap`); answers their number.
///
/// # Safety
/// `table` is a live table; `out` is null or holds `cap` values.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_collect(
    table: *const EeCombatUnitRegions,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
    out: *mut u64,
    cap: usize,
) -> usize {
    let values = unsafe { &*table }.collect(left, top, right, bottom);
    unsafe { copy_out(&values, out, cap) }
}

/// The saved order (see [`EeCombatUnitRegions::order`]; a null table: no region) into `out` (up to `cap`); answers
/// its length.
///
/// # Safety
/// `table` is null or a live table; `out` is null or holds `cap` values.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_order(
    table: *const EeCombatUnitRegions,
    out: *mut u32,
    cap: usize,
) -> usize {
    let order = match unsafe { table.as_ref() } {
        Some(t) => t.order(unit_id),
        None => vec![0],
    };
    unsafe { copy_out(&order, out, cap) }
}

/// Puts back saved region `index`'s order from `ids` (`count` of them): 1 put back, 0 the region holds other
/// units, -1 no table or no such region (the load stops putting back).
///
/// # Safety
/// `table` is null or a live table; `ids` holds `count` values.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_regions_restore(
    table: *mut EeCombatUnitRegions,
    index: u32,
    ids: *const u32,
    count: usize,
) -> i32 {
    let Some(t) = (unsafe { table.as_mut() }) else {
        return -1;
    };
    if index as usize >= t.count() {
        return -1;
    }
    let ids = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(ids, count) } };
    i32::from(t.restore(index as usize, ids, unit_id))
}

#[cfg(test)]
#[path = "regions_tests.rs"]
mod tests;
