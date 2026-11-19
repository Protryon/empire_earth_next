//! The database's unit sets whole (`EEUnitSet`, `EEUnitSetManager`, `gUnitSetManager`; db\dbunitset; was
//! EEUnitSet.cpp and EEUnitSetManager.cpp, the game's leaves round 2). A set holds unit families and unit types and
//! may name an included and an excluded set; `Contains` is the test the effects, the calamities, the civilization
//! builder, the tech tree and the scenario editor ask. The C++ classes are inline readers (ee/EEUnitSetManager.h): an
//! `EEUnitSet*` is this module's set, boxed (its address is stable until the table is cleared).

use std::collections::BTreeSet;
use std::ffi::c_char;
use std::sync::RwLock;

/// One record of dbunitset (`EEUnitSetRecord`, 184 bytes: tools/eedata reads its layout here, overlay.py).
#[repr(C)]
#[derive(Clone, Copy)]
#[allow(non_snake_case)]
pub struct DbUnitSet {
    pub mName: [u8; 0x68],
    /// 0 or less: no set at this index
    pub mID: i32,
    /// unit families (all eight are added; the valid ones kept)
    pub mFamilies: [u32; 8],
    /// unit types (only the positive ones are added)
    pub mUnitTypes: [i32; 8],
    /// the index of the included set, of the excluded set (0: none)
    pub mIncludedSet: i32,
    pub mExcludedSet: i32,
    pub mShowInEditor: u8,
}

const _: () = assert!(std::mem::size_of::<DbUnitSet>() == 184);

/// A unit set.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeUnitSet {
    pub families: BTreeSet<u32>,
    pub types: BTreeSet<u32>,
    /// the name's bytes and a NUL (`GetName`)
    pub name: Vec<u8>,
    /// indexes into the table (0: none)
    pub included: usize,
    pub excluded: usize,
    pub has_families: bool,
    pub show_in_editor: bool,
    /// its own index in the table
    pub index: usize,
}

impl EeUnitSet {
    /// A set from its record (`family_count`: the rows of dbfamily; a family must be below it).
    pub fn from_record(r: &DbUnitSet, family_count: u32) -> EeUnitSet {
        let mut s = EeUnitSet::default();
        let len = r.mName.iter().position(|&b| b == 0).unwrap_or(r.mName.len());
        s.name = r.mName[..len].to_vec();
        s.name.push(0);
        s.show_in_editor = r.mShowInEditor != 0;
        for &f in &r.mFamilies {
            // AddFamily: 1 .. the family count - 1
            if f > 0 && f < family_count {
                s.families.insert(f);
                s.has_families = true;
            }
        }
        for &t in &r.mUnitTypes {
            if t > 0 {
                s.types.insert(t as u32);
            }
        }
        s
    }

    /// `IsEmpty`: no families and no unit types.
    pub fn is_empty(&self) -> bool {
        self.families.is_empty() && self.types.is_empty()
    }
}

/// The table (`gUnitSetManager.mSets`; None: an unused record's empty slot).
pub type UnitSets = Vec<Option<Box<EeUnitSet>>>;

/// `EEUnitSet::Contains(type)` of set `index`: a family set holds the types of its families but its listed ones, a
/// type set its listed types; an included set adds its own, an excluded set's are never in it; a set with nothing
/// (no families, types or included set) holds every type. `family` is the unit type's class's family (None: no
/// class, as the C++'s NULL class).
pub fn contains(sets: &UnitSets, index: usize, unit_type: u32, family: &dyn Fn(u32) -> Option<u32>) -> bool {
    let Some(Some(s)) = sets.get(index) else { return false };
    let mut allowed;
    if s.families.is_empty() && s.types.is_empty() && s.included == 0 {
        allowed = true;
    } else {
        allowed = false;
        if let Some(f) = family(unit_type) {
            let in_family = s.families.contains(&f);
            let listed = s.types.contains(&unit_type);
            allowed = if s.has_families { in_family && !listed } else { listed };
        }
        if !allowed && s.included != 0 && contains(sets, s.included, unit_type, family) {
            allowed = true;
        }
    }
    allowed && !(s.excluded != 0 && contains(sets, s.excluded, unit_type, family))
}

/// `Load`'s links: each set's included and excluded set by its record's indexes (in the table, else none).
pub fn link(sets: &mut UnitSets, links: &[(i32, i32)]) {
    let n = sets.len() as i32;
    for (i, &(inc, exc)) in links.iter().enumerate() {
        if let Some(Some(s)) = sets.get_mut(i) {
            if inc > 0 && inc < n {
                s.included = inc as usize;
            }
            if exc > 0 && exc < n {
                s.excluded = exc as usize;
            }
        }
    }
}

struct Table {
    sets: UnitSets,
    links: Vec<(i32, i32)>,
}

static TABLE: RwLock<Table> = RwLock::new(Table { sets: Vec::new(), links: Vec::new() });

fn read() -> std::sync::RwLockReadGuard<'static, Table> {
    TABLE.read().unwrap_or_else(|e| e.into_inner())
}

fn write() -> std::sync::RwLockWriteGuard<'static, Table> {
    TABLE.write().unwrap_or_else(|e| e.into_inner())
}

/// The family of unit type `t`'s class (`gUnitClassTable[t]->mFamily`), through the world views.
#[cfg(not(test))]
fn family_of(t: u32) -> Option<u32> {
    use crate::world::view::{ClassView, WorldRead};
    // SAFETY: the unit classes are the database's, live while the sets are asked
    let token = unsafe { WorldRead::begin() };
    ClassView::by_index(&token, t as usize).map(|c| c.family() as u32)
}

#[cfg(test)]
fn family_of(t: u32) -> Option<u32> {
    // the tests' classes: unit type t of family t / 10
    Some(t / 10)
}

/// `EEUnitClassCount()` (`gUnitClassTable.size()`).
#[cfg(not(test))]
fn class_count() -> u32 {
    use crate::world::view::{ClassView, WorldRead};
    // SAFETY: as in `family_of`
    let token = unsafe { WorldRead::begin() };
    ClassView::count(&token) as u32
}

#[cfg(test)]
fn class_count() -> u32 {
    20
}

/// The size of a dbunitset record.
#[no_mangle]
pub extern "C" fn ee_unit_sets_record_size() -> usize {
    std::mem::size_of::<DbUnitSet>()
}

/// Every set deleted (the manager's dtor; a load's start).
#[no_mangle]
pub extern "C" fn ee_unit_sets_clear() {
    let mut t = write();
    t.sets.clear();
    t.links.clear();
}

/// Record `index`'s set (an empty slot for an unused record), appended; linked by [`ee_unit_sets_link`].
///
/// # Safety
/// `bytes` holds a whole record.
#[no_mangle]
pub unsafe extern "C" fn ee_unit_sets_load(bytes: *const u8) {
    // SAFETY: the caller's guarantee; the record is plain bytes
    let r: DbUnitSet = unsafe { std::ptr::read_unaligned(bytes as *const DbUnitSet) };
    let families = crate::combat::damage_table::ee_combat_damage_table_rows();
    let mut t = write();
    if r.mID > 0 {
        let mut set = EeUnitSet::from_record(&r, families);
        set.index = t.sets.len();
        t.sets.push(Some(Box::new(set)));
        t.links.push((r.mIncludedSet, r.mExcludedSet));
    } else {
        t.sets.push(None);
        t.links.push((0, 0));
    }
}

/// `Load`'s end: the sets' included and excluded sets.
#[no_mangle]
pub extern "C" fn ee_unit_sets_link() {
    let mut t = write();
    let links = std::mem::take(&mut t.links);
    link(&mut t.sets, &links);
}

/// The set count (`GetSize`).
#[no_mangle]
pub extern "C" fn ee_unit_sets_count() -> usize {
    read().sets.len()
}

/// Set `index` (`GetSet`: NULL past the end or for an unused record).
#[no_mangle]
pub extern "C" fn ee_unit_sets_get(index: usize) -> *const EeUnitSet {
    match read().sets.get(index) {
        Some(Some(s)) => &**s as *const EeUnitSet,
        _ => std::ptr::null(),
    }
}

/// The index of `set` (one of the table's).
///
/// # Safety
/// `set` is a live set of the table.
unsafe fn index_of(set: *const EeUnitSet) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*set).index }
}

/// `EEUnitSet::Contains(type)`.
///
/// # Safety
/// `set` is one of [`ee_unit_sets_get`]'s, the table not cleared since.
#[no_mangle]
pub unsafe extern "C" fn ee_unit_set_contains(set: *const EeUnitSet, unit_type: u32) -> bool {
    let t = read();
    // SAFETY: the caller's contract
    let i = unsafe { index_of(set) };
    contains(&t.sets, i, unit_type, &family_of)
}

/// `EEUnitSetManager::Contains(type, set)`: false for no set at `index`.
#[no_mangle]
pub extern "C" fn ee_unit_sets_contains(index: usize, unit_type: u32) -> bool {
    let t = read();
    contains(&t.sets, index, unit_type, &family_of)
}

/// `IsEmpty`.
///
/// # Safety
/// As [`ee_unit_set_contains`].
#[no_mangle]
pub unsafe extern "C" fn ee_unit_set_is_empty(set: *const EeUnitSet) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*set).is_empty() }
}

/// `mShowInEditor`.
///
/// # Safety
/// As [`ee_unit_set_contains`].
#[no_mangle]
pub unsafe extern "C" fn ee_unit_set_show_in_editor(set: *const EeUnitSet) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*set).show_in_editor }
}

/// `GetName`: the name's bytes, NUL-terminated (valid until the table is cleared).
///
/// # Safety
/// As [`ee_unit_set_contains`].
#[no_mangle]
pub unsafe extern "C" fn ee_unit_set_name(set: *const EeUnitSet) -> *const c_char {
    // SAFETY: the caller's contract
    unsafe { (*set).name.as_ptr() as *const c_char }
}

/// `ForEach`'s walk: the first unit type above `after` (1 .. `EEUnitClassCount()` - 1) the set holds, 0 when none.
///
/// # Safety
/// As [`ee_unit_set_contains`].
#[no_mangle]
pub unsafe extern "C" fn ee_unit_set_next(set: *const EeUnitSet, after: u32) -> u32 {
    let count = class_count();
    let t = read();
    // SAFETY: the caller's contract
    let i = unsafe { index_of(set) };
    (after + 1..count).find(|&u| contains(&t.sets, i, u, &family_of)).unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn record(id: i32, families: &[u32], types: &[i32], inc: i32, exc: i32) -> DbUnitSet {
        // SAFETY: a plain record
        let mut r: DbUnitSet = unsafe { std::mem::zeroed() };
        r.mID = id;
        r.mName[..3].copy_from_slice(b"set");
        r.mFamilies[..families.len()].copy_from_slice(families);
        r.mUnitTypes[..types.len()].copy_from_slice(types);
        r.mIncludedSet = inc;
        r.mExcludedSet = exc;
        r
    }

    #[test]
    fn families_types_and_the_linked_sets() {
        let fam = |t: u32| (t != 0).then_some(t / 10);
        let mut sets: UnitSets = vec![
            None,
            // 1: families 2 and 3 but type 25; family 70 is past the 60 families
            Some(Box::new(EeUnitSet::from_record(&record(1, &[2, 3, 70], &[25], 0, 0), 60))),
            // 2: types 5 and 6, plus set 1, minus set 3
            Some(Box::new(EeUnitSet::from_record(&record(2, &[], &[5, 6, -1], 1, 3), 60))),
            // 3: type 31
            Some(Box::new(EeUnitSet::from_record(&record(3, &[], &[31], 0, 0), 60))),
            // 4: nothing: every type
            Some(Box::new(EeUnitSet::from_record(&record(4, &[], &[], 0, 0), 60))),
        ];
        link(&mut sets, &[(0, 0), (0, 0), (1, 3), (0, 0), (0, 9)]);
        let has = |i: usize, t: u32| contains(&sets, i, t, &fam);
        assert!(has(1, 21) && has(1, 39) && !has(1, 25) && !has(1, 71));
        assert!(has(2, 5) && has(2, 6) && !has(2, 7));
        assert!(has(2, 21), "through the included set");
        assert!(!has(2, 31) && !has(2, 25));
        assert!(has(2, 30) && !has(2, 31), "set 3 excludes 31 only");
        assert!(has(4, 999) && !has(0, 1) && !has(9, 1));
        assert!(!sets[1].as_ref().unwrap().families.contains(&70));
        assert_eq!(sets[1].as_ref().unwrap().name, b"set\0");
        assert!(sets[4].as_ref().unwrap().is_empty());
        assert_eq!(sets[4].as_ref().unwrap().excluded, 0, "a link past the table is none");
    }

    #[test]
    fn the_table_loads_links_and_walks() {
        // (the only test of the table)
        ee_unit_sets_clear();
        for r in [record(1, &[], &[12, 14], 0, 0), record(0, &[], &[], 0, 0), record(3, &[], &[13], 0, 0)] {
            // SAFETY: one record's bytes
            unsafe { ee_unit_sets_load(&r as *const DbUnitSet as *const u8) };
        }
        ee_unit_sets_link();
        assert_eq!(ee_unit_sets_count(), 3);
        assert!(ee_unit_sets_get(1).is_null() && ee_unit_sets_get(7).is_null());
        let s = ee_unit_sets_get(0);
        // SAFETY: a set of the table
        unsafe {
            assert!(ee_unit_set_contains(s, 12) && !ee_unit_set_contains(s, 13));
            assert_eq!(ee_unit_set_next(s, 0), 12);
            assert_eq!(ee_unit_set_next(s, 12), 14);
            assert_eq!(ee_unit_set_next(s, 14), 0);
            assert_eq!(std::ffi::CStr::from_ptr(ee_unit_set_name(s)).to_bytes(), b"set");
        }
        assert!(!ee_unit_sets_contains(1, 12), "an unused slot holds nothing");
        assert_eq!(ee_unit_sets_record_size(), 184);
        ee_unit_sets_clear();
    }
}
