//! `EEGroup`'s Load (0x6064f4) and Save (0x606e61) as Rust sequences (RUST_PLAN R5b.4, the thirtieth round) over
//! player_save.rs's host: the file's primitives (`EESavePrimitive`) and the group's C++ parts (its formation, a
//! reference-counted object made by kind, and its unit vector, ids until `RepairPointers`) by operation.
//!
//! Kept: the formation kind 8 keeps the current formation; a kind the factory cannot make keeps it too; the ids read
//! until the first failure (the failed one pushed as well); the save's first failed write returned at once. One
//! difference: a failed kind read leaves the kind 8 (keep the formation) where the original used an uninitialised
//! word, and a failed id read pushes 0 where it pushed one.

#![allow(clippy::unnecessary_cast)]

use crate::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use std::ffi::c_void;
use std::os::raw::c_ulong;

/// `EEFormation::Create(io.i)`; when made, the old formation dereferenced and the new one the group's
pub const EE_UNITS_GROUP_SAVE_FORMATION_SET: i32 = 320;
/// `mUnits.push_back(id io.i)` (the id in the pointer, as the original)
pub const EE_UNITS_GROUP_SAVE_PUSH_ID: i32 = 321;
/// the formation's `mType` into `io.i`, 8 without one
pub const EE_UNITS_GROUP_SAVE_KIND: i32 = 322;
/// `mUnits.size()` into `io.i`
pub const EE_UNITS_GROUP_SAVE_COUNT: i32 = 323;
/// `GetUnitAt(index)->mUnitID` into `io.i`
pub const EE_UNITS_GROUP_SAVE_UNIT_ID: i32 = 324;

/// The formation kind that keeps the group's formation (and that a group without one saves).
const KEEP_FORMATION: c_ulong = 8;
const OK: i32 = 0;

/// `EEGroup::Load`: the formation kind, then the unit ids; the count's or the first id's read error.
pub fn load<H: PlayerSaveHost + ?Sized>(h: &mut H) -> i32 {
    let mut kind: c_ulong = KEEP_FORMATION;
    h.read_ulong(&mut kind);
    if kind != KEEP_FORMATION {
        h.op_with(EE_UNITS_GROUP_SAVE_FORMATION_SET, kind as i64);
    }
    let mut count: c_ulong = 0;
    let mut result = h.read_ulong(&mut count);
    if result == OK {
        let mut i: c_ulong = 0;
        while i < count && result == OK {
            let mut id: c_ulong = 0;
            result = h.read_ulong(&mut id);
            h.op_with(EE_UNITS_GROUP_SAVE_PUSH_ID, id as i64);
            i += 1;
        }
    }
    result
}

/// `EEGroup::Save`: the formation kind (8 for none), the unit count and the units' ids; the first write error.
pub fn save<H: PlayerSaveHost + ?Sized>(h: &mut H) -> i32 {
    let kind = h.query(EE_UNITS_GROUP_SAVE_KIND, 0).i as c_ulong;
    let count = h.query(EE_UNITS_GROUP_SAVE_COUNT, 0).i as c_ulong;
    let mut result = h.write_ulong(kind);
    if result != OK {
        return result;
    }
    result = h.write_ulong(count);
    if result != OK {
        return result;
    }
    let mut i: c_ulong = 0;
    while i < count && result == OK {
        let id = h.query(EE_UNITS_GROUP_SAVE_UNIT_ID, i as u32).i as c_ulong;
        result = h.write_ulong(id);
        i += 1;
    }
    result
}

struct FfiHost {
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
}

impl PlayerSaveHost for FfiHost {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        // SAFETY: the C++ callback with its own context; io lives for the call
        unsafe { (self.f)(self.ctx, op, index, io) }
    }
}

/// `EEGroup::Load` over the group's parts and the file in `f`.
///
/// # Safety
/// `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_load(ctx: *mut c_void, f: EeUnitsPlayerSaveFn) -> i32 {
    load(&mut FfiHost { ctx, f })
}

/// `EEGroup::Save`: as [`ee_units_group_load`].
///
/// # Safety
/// As [`ee_units_group_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_group_save(ctx: *mut c_void, f: EeUnitsPlayerSaveFn) -> i32 {
    save(&mut FfiHost { ctx, f })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::player_save::{EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG};
    use std::collections::VecDeque;

    #[derive(Default)]
    struct Group {
        formation: Option<i64>,
        units: Vec<i64>,
        file: VecDeque<i64>,
        writes: usize,
        fail_read: Option<usize>,
        fail_write: Option<usize>,
        reads: usize,
    }

    impl PlayerSaveHost for Group {
        fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
            match op {
                EE_UNITS_PLAYER_SAVE_READ_ULONG => {
                    self.reads += 1;
                    if self.fail_read == Some(self.reads) {
                        return 2;
                    }
                    match self.file.pop_front() {
                        Some(v) => io.i = v,
                        None => return 2,
                    }
                }
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG => {
                    self.writes += 1;
                    if self.fail_write == Some(self.writes) {
                        return 4;
                    }
                    self.file.push_back(io.i);
                }
                // the factory makes kinds 0..7
                EE_UNITS_GROUP_SAVE_FORMATION_SET => {
                    if (0..8).contains(&io.i) {
                        self.formation = Some(io.i);
                    }
                }
                EE_UNITS_GROUP_SAVE_PUSH_ID => self.units.push(io.i),
                EE_UNITS_GROUP_SAVE_KIND => io.i = self.formation.unwrap_or(8),
                EE_UNITS_GROUP_SAVE_COUNT => io.i = self.units.len() as i64,
                EE_UNITS_GROUP_SAVE_UNIT_ID => io.i = self.units[index as usize],
                _ => panic!("op {op}"),
            }
            OK
        }
    }

    #[test]
    fn save_and_load_the_kind_and_the_ids() {
        let mut g = Group { formation: Some(3), units: vec![0x0300_0010, 0x0300_0011], ..Default::default() };
        assert_eq!(save(&mut g), OK);
        assert_eq!(g.file.iter().copied().collect::<Vec<_>>(), vec![3, 2, 0x0300_0010, 0x0300_0011]);
        let mut l = Group { formation: Some(1), units: vec![7], file: g.file.clone(), ..Default::default() };
        assert_eq!(load(&mut l), OK);
        assert_eq!(l.formation, Some(3));
        assert_eq!(l.units, vec![7, 0x0300_0010, 0x0300_0011]);
        // no formation: 8, which keeps the loading group's
        let mut n = Group::default();
        save(&mut n);
        assert_eq!(n.file.iter().copied().collect::<Vec<_>>(), vec![8, 0]);
        let mut k = Group { formation: Some(5), file: n.file.clone(), ..Default::default() };
        assert_eq!(load(&mut k), OK);
        assert_eq!(k.formation, Some(5));
        // a kind the factory cannot make keeps the formation too
        let mut b = Group { formation: Some(5), file: [12, 0].into_iter().collect(), ..Default::default() };
        assert_eq!(load(&mut b), OK);
        assert_eq!(b.formation, Some(5));
    }

    #[test]
    fn failures_stop_where_the_original_did() {
        // the second id's read fails: pushed as 0, the error returned
        let mut l = Group { file: [8, 3, 21, 22, 23].into_iter().collect(), fail_read: Some(4), ..Default::default() };
        assert_eq!(load(&mut l), 2);
        assert_eq!(l.units, vec![21, 0]);
        // the count's write fails: nothing after it
        let mut s = Group { units: vec![1, 2], fail_write: Some(2), ..Default::default() };
        assert_eq!(save(&mut s), 4);
        assert_eq!(s.file.len(), 1);
    }
}
