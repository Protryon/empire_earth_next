//! `EEAirTrafficControl`'s Load (0x54f75c) and Save (0x54fead) over the Rust state (atc.rs [`EeUnitsAtc`]) and the
//! file's primitives (player_save.rs's host, `EESavePrimitive`).
//!
//! Kept quirks: the airports cleared without their references (a load starts from a new player's empty control);
//! an airport's id stored whatever its read returned; an airport kept once its item count was read even when one
//! of its items failed (the loop ends there); an item's kind carried from the item before when its read fails (the
//! item is dropped); kind 1 a landing, any other a takeoff; the cargo totals read into their old values; the save's
//! item count the landings plus the takeoffs, the landings first. A loaded unit is its saved id (the original's
//! `reinterpret_cast`) until `RepairPointers`.

#![allow(clippy::unnecessary_cast)]

use crate::atc::{EeUnitsAtc, Item, Slot, Unit};
use crate::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use std::ffi::c_void;
use std::os::raw::c_ulong;

const OK: i32 = 0;

/// `EEAirTrafficControl::Load`: the airports with their queued planes (unit ids), then the cargo totals; the first
/// read error.
pub fn load<H: PlayerSaveHost + ?Sized>(a: &mut EeUnitsAtc, h: &mut H) -> i32 {
    let mut item = Item { unit: std::ptr::null_mut(), kind: 0 };
    let mut slots: c_ulong = 0;
    let mut items: c_ulong = 0;
    let mut id: c_ulong = 0;
    a.slots.clear();
    let mut result = h.read_ulong(&mut slots);
    if result != OK {
        return result;
    }
    let mut i: c_ulong = 0;
    while i < slots && result == OK {
        result = h.read_ulong(&mut id);
        let mut slot = Slot::new(id as usize as Unit);
        if result == OK {
            result = h.read_ulong(&mut items);
            if result == OK {
                let mut j: c_ulong = 0;
                while j < items && result == OK {
                    result = h.read_ulong(&mut id);
                    item.unit = id as usize as Unit;
                    if result == OK {
                        result = h.read_ulong(&mut item.kind);
                        if result == OK {
                            if item.kind == 1 {
                                slot.landing.push_back(item);
                            } else {
                                slot.takeoff.push_back(item);
                            }
                        }
                    }
                    j += 1;
                }
                a.slots.push(slot);
            }
        }
        i += 1;
    }
    if result != OK {
        return result;
    }
    result = h.read_long(&mut a.max_cargo);
    if result == OK {
        result = h.read_long(&mut a.used_cargo);
    }
    result
}

/// `EEAirTrafficControl::Save`: the airports (unit id, item count, then each landing's and takeoff's unit id and
/// kind), then the cargo totals; the first write error. `ids` gives a unit's id.
pub fn save<H: PlayerSaveHost + ?Sized>(a: &EeUnitsAtc, h: &mut H, ids: &mut dyn FnMut(Unit) -> c_ulong) -> i32 {
    let mut result = h.write_ulong(a.slots.len() as c_ulong);
    for slot in &a.slots {
        if result != OK {
            break;
        }
        result = h.write_ulong(ids(slot.airport));
        if result != OK {
            break;
        }
        result = h.write_ulong((slot.landing.len() + slot.takeoff.len()) as c_ulong);
        for item in slot.landing.iter().chain(slot.takeoff.iter()) {
            if result != OK {
                break;
            }
            result = h.write_ulong(ids(item.unit));
            if result == OK {
                result = h.write_ulong(item.kind);
            }
        }
    }
    if result == OK {
        result = h.write_long(a.max_cargo);
        if result == OK {
            result = h.write_long(a.used_cargo);
        }
    }
    result
}

// ---- FFI

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

/// `EEAirTrafficControl::Load` into `p` over the file in `f`.
///
/// # Safety
/// `p` is a live state; `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_load(p: *mut EeUnitsAtc, ctx: *mut c_void, f: EeUnitsPlayerSaveFn) -> i32 {
    // SAFETY: the contract; the file's primitives never call back into the control
    load(unsafe { &mut *p }, &mut FfiHost { ctx, f })
}

/// A unit's id (`GetUnitID()`).
pub type EeUnitsAtcUnitIdFn = unsafe extern "C" fn(*mut c_void) -> c_ulong;

/// `EEAirTrafficControl::Save` of `p` over the file in `f`; `unit_id` gives each unit's id.
///
/// # Safety
/// As [`ee_units_atc_load`]; `unit_id` is valid for every unit listed.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_save(
    p: *const EeUnitsAtc,
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    unit_id: EeUnitsAtcUnitIdFn,
) -> i32 {
    // SAFETY: the contract (a copy, so no borrow of the state lives across the C++ calls)
    let a = unsafe { (*p).clone() };
    // SAFETY: the contract
    save(&a, &mut FfiHost { ctx, f }, &mut |u| unsafe { unit_id(u) })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::player_save::{
        EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_LONG,
        EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
    };
    use std::collections::VecDeque;

    /// A file of numbers; a failing read or write by count.
    #[derive(Default)]
    struct File {
        words: VecDeque<i64>,
        reads: usize,
        writes: usize,
        fail_read: Option<usize>,
        fail_write: Option<usize>,
    }

    impl PlayerSaveHost for File {
        fn call(&mut self, op: i32, _: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
            match op {
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => {
                    self.writes += 1;
                    if self.fail_write == Some(self.writes) {
                        return 4;
                    }
                    self.words.push_back(io.i);
                }
                EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_LONG => {
                    self.reads += 1;
                    if self.fail_read == Some(self.reads) {
                        return 2;
                    }
                    match self.words.pop_front() {
                        Some(v) => io.i = v,
                        None => return 2,
                    }
                }
                _ => {}
            }
            OK
        }
    }

    fn u(id: usize) -> Unit {
        id as Unit
    }

    fn slot(airport: usize, landing: &[(usize, c_ulong)], takeoff: &[(usize, c_ulong)]) -> Slot {
        let mut s = Slot::new(u(airport));
        s.landing = landing.iter().map(|&(id, kind)| Item { unit: u(id), kind }).collect();
        s.takeoff = takeoff.iter().map(|&(id, kind)| Item { unit: u(id), kind }).collect();
        s
    }

    fn sample() -> EeUnitsAtc {
        EeUnitsAtc {
            slots: vec![
                slot(0x0300_0010, &[(0x0300_0021, 1), (0x0300_0022, 1)], &[(0x0300_0023, 2)]),
                slot(0x0300_0030, &[], &[]),
            ],
            max_cargo: 40,
            used_cargo: 12,
        }
    }

    /// A test unit is its own id.
    fn ids(unit: Unit) -> c_ulong {
        unit as usize as c_ulong
    }

    #[test]
    fn save_writes_the_original_layout_and_load_reads_it_back() {
        let mut f = File::default();
        assert_eq!(save(&sample(), &mut f, &mut ids), OK);
        let words: Vec<i64> = f.words.iter().copied().collect();
        assert_eq!(
            words,
            vec![2, 0x0300_0010, 3, 0x0300_0021, 1, 0x0300_0022, 1, 0x0300_0023, 2, 0x0300_0030, 0, 40, 12]
        );
        let mut b = EeUnitsAtc { slots: vec![slot(1, &[], &[])], ..Default::default() };
        assert_eq!(load(&mut b, &mut f), OK);
        assert_eq!(b, sample());
    }

    #[test]
    fn a_failed_item_keeps_its_airport_and_ends_the_load() {
        let mut f = File::default();
        save(&sample(), &mut f, &mut ids);
        let words = f.words.clone();
        // reads: slots, id, items, unit 1, kind 1, unit 2 (fails)
        let mut b = EeUnitsAtc::default();
        assert_eq!(load(&mut b, &mut File { words: words.clone(), fail_read: Some(6), ..Default::default() }), 2);
        assert_eq!(b.slots, vec![slot(0x0300_0010, &[(0x0300_0021, 1)], &[])]);
        assert_eq!((b.max_cargo, b.used_cargo), (0, 0));
        // a failed slot count: nothing but the clear
        let mut c = EeUnitsAtc { slots: vec![slot(5, &[], &[])], ..Default::default() };
        assert_eq!(load(&mut c, &mut File { words: words.clone(), fail_read: Some(1), ..Default::default() }), 2);
        assert!(c.slots.is_empty());
        // the used cargo's read fails: the maximum is in, the used total keeps its old value
        let mut d = EeUnitsAtc { used_cargo: 7, ..Default::default() };
        assert_eq!(load(&mut d, &mut File { words, fail_read: Some(13), ..Default::default() }), 2);
        assert_eq!((d.max_cargo, d.used_cargo), (40, 7));
    }

    #[test]
    fn a_failed_kind_read_drops_the_item() {
        // one airport, two items; the second item's kind read (the 7th read) fails
        let words: VecDeque<i64> = [1, 0x10, 2, 0x21, 1, 0x22].into_iter().collect();
        let mut a = EeUnitsAtc::default();
        assert_eq!(load(&mut a, &mut File { words, fail_read: Some(7), ..Default::default() }), 2);
        assert_eq!(a.slots, vec![slot(0x10, &[(0x21, 1)], &[])]);
    }

    #[test]
    fn a_failed_write_stops_the_save() {
        let mut f = File { fail_write: Some(2), ..Default::default() };
        assert_eq!(save(&sample(), &mut f, &mut ids), 4);
        assert_eq!(f.writes, 2);
        assert_eq!(f.words.len(), 1);
    }
}
