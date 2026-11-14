//! `EEAirTrafficControl`'s Load (0x54f75c) and Save (0x54fead) as Rust sequences (RUST_PLAN R5b.4, the twenty-ninth
//! round) over player_save.rs's host and file primitives (`EESavePrimitive`) and the class's C++ parts: its airports
//! (`std::vector<EEATCAirport>`, each with a landing and a takeoff `std::deque<EEATCItem>`), built and read by
//! operation, and its two cargo totals.
//!
//! Kept quirks: an airport's id stored whatever its read returned; an airport kept once its item count was read even
//! when one of its items failed (the loop ends there); an item's kind carried from the item before when its read
//! fails (the item is dropped); kind 1 a landing, any other a takeoff; the save's item count the landings plus the
//! takeoffs, the landings first.

#![allow(clippy::unnecessary_cast)]

use crate::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

// ---- the C++ parts (the callback's `op`; the file's primitives are player_save.rs's)
/// `mSlots.clear()`
pub const EE_UNITS_ATC_SAVE_CLEAR: i32 = 301;
/// a new airport (the original's local `theSlot`): its airport the id `io.i`, its queues empty
pub const EE_UNITS_ATC_SAVE_SLOT_BEGIN: i32 = 302;
/// the item `io.i` (a unit id) of kind `index` into the new airport's landings (kind 1) or takeoffs
pub const EE_UNITS_ATC_SAVE_ITEM_PUSH: i32 = 303;
/// `mSlots.push_back(theSlot)`
pub const EE_UNITS_ATC_SAVE_SLOT_PUSH: i32 = 304;
/// the cargo total `index` (0 `mMaxCargo`, 1 `mUsedCargo`) into `io.i`
pub const EE_UNITS_ATC_SAVE_CARGO: i32 = 305;
/// the cargo total `index` = `io.i`
pub const EE_UNITS_ATC_SAVE_SET_CARGO: i32 = 306;
/// `mSlots.size()` into `io.i`
pub const EE_UNITS_ATC_SAVE_SLOT_COUNT: i32 = 307;
/// airport `index`'s unit id into `io.i`
pub const EE_UNITS_ATC_SAVE_SLOT_AIRPORT: i32 = 308;
/// airport `index`'s queue `io.b` (0 the landings, 1 the takeoffs): its length into `io.i`
pub const EE_UNITS_ATC_SAVE_QUEUE_COUNT: i32 = 309;
/// airport `index`'s queue `io.b`, item `io.i`: its unit's id into `io.i`, its kind into `io.f`'s bits
pub const EE_UNITS_ATC_SAVE_ITEM: i32 = 310;

const OK: i32 = 0;

fn query<H: PlayerSaveHost + ?Sized>(h: &mut H, op: i32, index: u32, b: u8, i: i64) -> EeUnitsPlayerSaveIo {
    let mut io = EeUnitsPlayerSaveIo { i, b, ..Default::default() };
    h.call(op, index, &mut io);
    io
}

/// `EEAirTrafficControl::Load`: the airports with their queued planes (unit ids, `RepairPointers` turns them back),
/// then the cargo totals; the first read error.
pub fn load<H: PlayerSaveHost + ?Sized>(h: &mut H) -> i32 {
    let mut unit: c_ulong = 0;
    let mut kind: c_ulong = 0;
    let mut slots: c_ulong = 0;
    let mut items: c_ulong = 0;
    let mut id: c_ulong = 0;
    h.op(EE_UNITS_ATC_SAVE_CLEAR);
    let mut result = h.read_ulong(&mut slots);
    if result != OK {
        return result;
    }
    let mut i: c_ulong = 0;
    while i < slots && result == OK {
        result = h.read_ulong(&mut id);
        h.op_with(EE_UNITS_ATC_SAVE_SLOT_BEGIN, id as i64);
        if result == OK {
            result = h.read_ulong(&mut items);
            if result == OK {
                let mut j: c_ulong = 0;
                while j < items && result == OK {
                    result = h.read_ulong(&mut unit);
                    if result == OK {
                        result = h.read_ulong(&mut kind);
                        if result == OK {
                            h.call(
                                EE_UNITS_ATC_SAVE_ITEM_PUSH,
                                kind as u32,
                                &mut EeUnitsPlayerSaveIo { i: unit as i64, ..Default::default() },
                            );
                        }
                    }
                    j += 1;
                }
                h.op(EE_UNITS_ATC_SAVE_SLOT_PUSH);
            }
        }
        i += 1;
    }
    if result != OK {
        return result;
    }
    for which in 0..2 {
        let mut total = query(h, EE_UNITS_ATC_SAVE_CARGO, which, 0, 0).i as c_long;
        result = h.read_long(&mut total);
        h.call(EE_UNITS_ATC_SAVE_SET_CARGO, which, &mut EeUnitsPlayerSaveIo { i: total as i64, ..Default::default() });
        if result != OK {
            break;
        }
    }
    result
}

/// `EEAirTrafficControl::Save`: the airports (unit id, item count, then each landing's and takeoff's unit id and
/// kind), then the cargo totals; the first write error.
pub fn save<H: PlayerSaveHost + ?Sized>(h: &mut H) -> i32 {
    let slots = query(h, EE_UNITS_ATC_SAVE_SLOT_COUNT, 0, 0, 0).i as c_ulong;
    let mut result = h.write_ulong(slots);
    let mut i: c_ulong = 0;
    while i < slots && result == OK {
        let slot = i as u32;
        let airport = query(h, EE_UNITS_ATC_SAVE_SLOT_AIRPORT, slot, 0, 0).i as c_ulong;
        result = h.write_ulong(airport);
        if result != OK {
            break;
        }
        let landings = query(h, EE_UNITS_ATC_SAVE_QUEUE_COUNT, slot, 0, 0).i as c_ulong;
        let takeoffs = query(h, EE_UNITS_ATC_SAVE_QUEUE_COUNT, slot, 1, 0).i as c_ulong;
        result = h.write_ulong(landings.wrapping_add(takeoffs));
        for (queue, count) in [(0u8, landings), (1u8, takeoffs)] {
            let mut j: c_ulong = 0;
            while j < count && result == OK {
                let item = query(h, EE_UNITS_ATC_SAVE_ITEM, slot, queue, j as i64);
                result = h.write_ulong(item.i as c_ulong);
                if result == OK {
                    result = h.write_ulong(item.f.to_bits() as c_ulong);
                }
                j += 1;
            }
        }
        i += 1;
    }
    if result == OK {
        let max = query(h, EE_UNITS_ATC_SAVE_CARGO, 0, 0, 0).i as c_long;
        result = h.write_long(max);
        if result == OK {
            let used = query(h, EE_UNITS_ATC_SAVE_CARGO, 1, 0, 0).i as c_long;
            result = h.write_long(used);
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

/// `EEAirTrafficControl::Load` over the C++ parts in `f`.
///
/// # Safety
/// `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_load(ctx: *mut c_void, f: EeUnitsPlayerSaveFn) -> i32 {
    load(&mut FfiHost { ctx, f })
}

/// `EEAirTrafficControl::Save`: as [`ee_units_atc_load`].
///
/// # Safety
/// As [`ee_units_atc_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_atc_save(ctx: *mut c_void, f: EeUnitsPlayerSaveFn) -> i32 {
    save(&mut FfiHost { ctx, f })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::player_save::{
        EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_LONG,
        EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
    };
    use std::collections::VecDeque;

    /// An airport: its id, its landings and takeoffs as (unit, kind).
    type Slot = (i64, Vec<(i64, u32)>, Vec<(i64, u32)>);

    /// The class (its airports and cargo) over a file of numbers; a failing read by count.
    #[derive(Default)]
    struct Atc {
        slots: Vec<Slot>,
        building: Option<Slot>,
        cargo: [i64; 2],
        file: VecDeque<i64>,
        reads: usize,
        fail_read: Option<usize>,
    }

    impl PlayerSaveHost for Atc {
        fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
            match op {
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => self.file.push_back(io.i),
                EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_LONG => {
                    self.reads += 1;
                    if self.fail_read == Some(self.reads) {
                        return 2;
                    }
                    match self.file.pop_front() {
                        Some(v) => io.i = v,
                        None => return 2,
                    }
                }
                EE_UNITS_ATC_SAVE_CLEAR => self.slots.clear(),
                EE_UNITS_ATC_SAVE_SLOT_BEGIN => self.building = Some((io.i, Vec::new(), Vec::new())),
                EE_UNITS_ATC_SAVE_ITEM_PUSH => {
                    let slot = self.building.as_mut().unwrap();
                    if index == 1 {
                        slot.1.push((io.i, index));
                    } else {
                        slot.2.push((io.i, index));
                    }
                }
                EE_UNITS_ATC_SAVE_SLOT_PUSH => self.slots.push(self.building.clone().unwrap()),
                EE_UNITS_ATC_SAVE_CARGO => io.i = self.cargo[index as usize],
                EE_UNITS_ATC_SAVE_SET_CARGO => self.cargo[index as usize] = io.i,
                EE_UNITS_ATC_SAVE_SLOT_COUNT => io.i = self.slots.len() as i64,
                EE_UNITS_ATC_SAVE_SLOT_AIRPORT => io.i = self.slots[index as usize].0,
                EE_UNITS_ATC_SAVE_QUEUE_COUNT => {
                    let s = &self.slots[index as usize];
                    io.i = if io.b == 0 { s.1.len() } else { s.2.len() } as i64;
                }
                EE_UNITS_ATC_SAVE_ITEM => {
                    let s = &self.slots[index as usize];
                    let (unit, kind) = if io.b == 0 { s.1[io.i as usize] } else { s.2[io.i as usize] };
                    io.i = unit;
                    io.f = f32::from_bits(kind);
                }
                _ => {}
            }
            OK
        }
    }

    fn sample() -> Atc {
        Atc {
            slots: vec![
                (0x0300_0010, vec![(0x0300_0021, 1), (0x0300_0022, 1)], vec![(0x0300_0023, 2)]),
                (0x0300_0030, vec![], vec![]),
            ],
            cargo: [40, 12],
            ..Default::default()
        }
    }

    #[test]
    fn save_writes_the_original_layout_and_load_reads_it_back() {
        let mut a = sample();
        assert_eq!(save(&mut a), OK);
        let words: Vec<i64> = a.file.iter().copied().collect();
        assert_eq!(
            words,
            vec![2, 0x0300_0010, 3, 0x0300_0021, 1, 0x0300_0022, 1, 0x0300_0023, 2, 0x0300_0030, 0, 40, 12]
        );
        let mut b = Atc { file: a.file.clone(), slots: vec![(1, vec![], vec![])], ..Default::default() };
        assert_eq!(load(&mut b), OK);
        assert_eq!(b.slots, sample().slots);
        assert_eq!(b.cargo, [40, 12]);
    }

    #[test]
    fn a_failed_item_keeps_its_airport_and_ends_the_load() {
        let mut a = sample();
        save(&mut a);
        // reads: slots, id, items, unit 1, kind 1, unit 2 (fails)
        let mut b = Atc { file: a.file.clone(), fail_read: Some(6), ..Default::default() };
        assert_eq!(load(&mut b), 2);
        assert_eq!(b.slots, vec![(0x0300_0010, vec![(0x0300_0021, 1)], vec![])]);
        assert_eq!(b.cargo, [0, 0]);
        // a failed slot count: nothing but the clear
        let mut c =
            Atc { file: a.file.clone(), fail_read: Some(1), slots: vec![(5, vec![], vec![])], ..Default::default() };
        assert_eq!(load(&mut c), 2);
        assert!(c.slots.is_empty());
        // the used cargo's read fails: the maximum is in, the used total keeps its old value
        let mut d = Atc { file: a.file.clone(), fail_read: Some(13), cargo: [0, 7], ..Default::default() };
        assert_eq!(load(&mut d), 2);
        assert_eq!(d.cargo, [40, 7]);
    }

    #[test]
    fn a_failed_write_stops_the_save() {
        struct Fail(usize);
        impl PlayerSaveHost for Fail {
            fn call(&mut self, op: i32, _: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
                match op {
                    EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => {
                        self.0 += 1;
                        i32::from(self.0 == 2) * 4
                    }
                    EE_UNITS_ATC_SAVE_SLOT_COUNT => {
                        io.i = 3;
                        OK
                    }
                    _ => OK,
                }
            }
        }
        let mut f = Fail(0);
        assert_eq!(save(&mut f), 4);
        assert_eq!(f.0, 2);
    }
}
