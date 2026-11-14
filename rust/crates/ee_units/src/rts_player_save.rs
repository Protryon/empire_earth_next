//! `RTSPlayer`'s saved numbers as a Rust record with its Load and Save (RUST_PLAN R5b.4, the twenty-ninth round): the
//! block `mNextUniqueID` (+0x458 in the original) .. `mObserverMode` (+0x467) is [`EeUnitsRtsPlayerFields`], which the
//! C++ class derives from (`class RTSPlayer : public EeUnitsRtsPlayerFields`, ee/EEPlayer.h): the members keep their
//! names, C++ types and order. `RTSPlayer::Load` (0x56f539) and `Save` (0x56f6f8) are [`load`] and [`save`] over
//! player_save.rs's host and file primitives (`EESavePrimitive`) and the player's C++ parts: the colour and the name
//! (`GEColor::Load`/`Save`, `UWideString::Load`/`Save`), the shade colour, one unit's load (`LoadUnit`), the pending
//! units and the units' save (`SaveUnits`).
//!
//! Kept quirks: the 0.207 and older saves' "sees all" flag cleared after its read; the hash bucket length read and
//! dropped; a bucket's unit count carried into the next bucket when its read fails (the loop ends there); the pending
//! units added whatever the units' reads returned; the observer flag read only after a clean unit loop.

#![allow(non_snake_case)]
// `c_ulong` is 32 bits on Windows and 64 elsewhere: the casts to the i64 slot are needed there
#![allow(clippy::unnecessary_cast)]

use crate::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo, PlayerSaveHost};
use std::ffi::c_void;
use std::os::raw::c_ulong;

/// `RTSPlayer`'s plain members (ee/EEPlayer.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsRtsPlayerFields {
    /// the next unit id's low 24 bits (saved)
    pub mNextUniqueID: c_ulong,
    /// the player's index (saved)
    pub mPlayerID: c_ulong,
    /// its units (saved)
    pub mUnitCount: c_ulong,
    /// the lobby slot (0xff none; not saved)
    pub mSlot: u8,
    /// sees the whole map (saved; cleared for 0.207 and older)
    pub mSeesAll: bool,
    /// keeps a visibility map (saved)
    pub mSaveVisMap: bool,
    /// an observer (saved)
    pub mObserverMode: bool,
}

// ---- the C++ parts (the callback's `op`; the file's primitives are player_save.rs's)
/// `mColor.Load(inFile)`, the result
pub const EE_UNITS_RTS_PLAYER_SAVE_COLOR_LOAD: i32 = 201;
/// the shade colour from the colour (each channel times 0.6)
pub const EE_UNITS_RTS_PLAYER_SAVE_SHADE: i32 = 202;
/// `mName.Load(inFile)`, the result
pub const EE_UNITS_RTS_PLAYER_SAVE_NAME_LOAD: i32 = 203;
/// `LoadUnit(major, minor, inFile)`, the result
pub const EE_UNITS_RTS_PLAYER_SAVE_LOAD_UNIT: i32 = 204;
/// `AddPendingUnits(true, false)`
pub const EE_UNITS_RTS_PLAYER_SAVE_ADD_PENDING: i32 = 205;
/// `mColor.Save(outFile)`, the result
pub const EE_UNITS_RTS_PLAYER_SAVE_COLOR_SAVE: i32 = 206;
/// `mName.Save(outFile)`, the result
pub const EE_UNITS_RTS_PLAYER_SAVE_NAME_SAVE: i32 = 207;
/// `SaveUnits(major, minor, outFile)`, the result
pub const EE_UNITS_RTS_PLAYER_SAVE_SAVE_UNITS: i32 = 208;

const OK: i32 = 0;
/// The id buckets the save walks (`RTSPlayer::mUnitBuckets`).
const BUCKETS: u32 = 0x80;

/// The record behind its raw pointer: read and written field by field, never by a reference held across a host call.
struct Rec(*mut EeUnitsRtsPlayerFields);

impl Rec {
    fn get(&self) -> EeUnitsRtsPlayerFields {
        // SAFETY: the caller's contract (a live record); copied out
        unsafe { *self.0 }
    }
    fn set(&self, f: impl FnOnce(&mut EeUnitsRtsPlayerFields)) {
        // SAFETY: as get; the reference ends with the closure, before any host call
        unsafe { f(&mut *self.0) }
    }
}

fn read_bool<H: PlayerSaveHost + ?Sized>(
    h: &mut H,
    rec: &Rec,
    field: fn(&mut EeUnitsRtsPlayerFields) -> &mut bool,
) -> i32 {
    let mut v = *field(&mut rec.get());
    let r = h.read_bool(&mut v);
    rec.set(|s| *field(s) = v);
    r
}

fn read_ulong<H: PlayerSaveHost + ?Sized>(
    h: &mut H,
    rec: &Rec,
    field: fn(&mut EeUnitsRtsPlayerFields) -> &mut c_ulong,
) -> i32 {
    let mut v = *field(&mut rec.get());
    let r = h.read_ulong(&mut v);
    rec.set(|s| *field(s) = v);
    r
}

/// `RTSPlayer::Load`: the flags, the ids, the colour (and the shade from it), the name, every bucket's units, the
/// observer flag; the first read error.
pub fn load<H: PlayerSaveHost + ?Sized>(h: &mut H, rec: *mut EeUnitsRtsPlayerFields, major: u32, minor: u32) -> i32 {
    let rec = Rec(rec);
    let mut hash_bucket_length: c_ulong = 0;
    let mut unit_count: c_ulong = 0;
    let mut result = read_bool(h, &rec, |s| &mut s.mSaveVisMap);
    if result == OK {
        result = read_bool(h, &rec, |s| &mut s.mSeesAll);
    }
    if result == OK {
        if major == 0 && minor <= 0xcf {
            rec.set(|s| s.mSeesAll = false);
        }
        result = read_ulong(h, &rec, |s| &mut s.mNextUniqueID);
    }
    if result == OK {
        result = read_ulong(h, &rec, |s| &mut s.mPlayerID);
    }
    if result == OK {
        result = h.op(EE_UNITS_RTS_PLAYER_SAVE_COLOR_LOAD);
    }
    if result == OK {
        h.op(EE_UNITS_RTS_PLAYER_SAVE_SHADE);
        result = h.op(EE_UNITS_RTS_PLAYER_SAVE_NAME_LOAD);
    }
    if result == OK {
        result = h.read_ulong(&mut hash_bucket_length);
    }
    if result != OK {
        return result;
    }
    let mut i = 0;
    while i < BUCKETS && result == OK {
        result = h.read_ulong(&mut unit_count);
        if result == OK {
            let mut j: c_ulong = 0;
            while j < unit_count && result == OK {
                result = h.op(EE_UNITS_RTS_PLAYER_SAVE_LOAD_UNIT);
                j += 1;
            }
        }
        i += 1;
    }
    h.op(EE_UNITS_RTS_PLAYER_SAVE_ADD_PENDING);
    if result == OK {
        result = read_bool(h, &rec, |s| &mut s.mObserverMode);
    }
    result
}

/// `RTSPlayer::Save`: the flags, the ids, the colour, the name, the unit count, the units, the observer flag; the
/// first write error.
pub fn save<H: PlayerSaveHost + ?Sized>(h: &mut H, rec: *mut EeUnitsRtsPlayerFields) -> i32 {
    let s = Rec(rec).get();
    let steps: [&mut dyn FnMut(&mut H) -> i32; 9] = [
        &mut |h| h.write_bool(s.mSaveVisMap),
        &mut |h| h.write_bool(s.mSeesAll),
        &mut |h| h.write_ulong(s.mNextUniqueID),
        &mut |h| h.write_ulong(s.mPlayerID),
        &mut |h| h.op(EE_UNITS_RTS_PLAYER_SAVE_COLOR_SAVE),
        &mut |h| h.op(EE_UNITS_RTS_PLAYER_SAVE_NAME_SAVE),
        &mut |h| h.write_ulong(s.mUnitCount),
        &mut |h| h.op(EE_UNITS_RTS_PLAYER_SAVE_SAVE_UNITS),
        &mut |h| h.write_bool(s.mObserverMode),
    ];
    let mut result = OK;
    for step in steps {
        result = step(h);
        if result != OK {
            return result;
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

/// `RTSPlayer::Load`: the record `rec` (the player's `EeUnitsRtsPlayerFields` base) and its C++ parts through `f`.
///
/// # Safety
/// `rec` points to the player's live record; `f` is valid with `ctx` for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_rts_player_load(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rec: *mut EeUnitsRtsPlayerFields,
    major: u32,
    minor: u32,
) -> i32 {
    load(&mut FfiHost { ctx, f }, rec, major, minor)
}

/// `RTSPlayer::Save`: as [`ee_units_rts_player_load`].
///
/// # Safety
/// As [`ee_units_rts_player_load`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_rts_player_save(
    ctx: *mut c_void,
    f: EeUnitsPlayerSaveFn,
    rec: *mut EeUnitsRtsPlayerFields,
) -> i32 {
    save(&mut FfiHost { ctx, f }, rec)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::player_save::{
        EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
        EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
    };
    use std::collections::VecDeque;

    /// A file of values with a log of the C++ parts; a failing call by count.
    #[derive(Default)]
    struct File {
        data: VecDeque<(i64, u8)>,
        log: Vec<i32>,
        loaded_units: u32,
        fail_read: Option<usize>,
        reads: usize,
    }

    impl PlayerSaveHost for File {
        fn call(&mut self, op: i32, _index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
            self.log.push(op);
            match op {
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_BOOL => {
                    self.data.push_back((io.i, io.b));
                    OK
                }
                EE_UNITS_PLAYER_SAVE_READ_ULONG | EE_UNITS_PLAYER_SAVE_READ_BOOL => {
                    self.reads += 1;
                    if self.fail_read == Some(self.reads) {
                        return 3;
                    }
                    match self.data.pop_front() {
                        Some((i, b)) => {
                            io.i = i;
                            io.b = b;
                            OK
                        }
                        None => 3,
                    }
                }
                EE_UNITS_RTS_PLAYER_SAVE_SAVE_UNITS => {
                    // every bucket's count: two units in bucket 5, none elsewhere
                    for k in 0..BUCKETS {
                        self.data.push_back((if k == 5 { 2 } else { 0 }, 0));
                    }
                    OK
                }
                EE_UNITS_RTS_PLAYER_SAVE_LOAD_UNIT => {
                    self.loaded_units += 1;
                    OK
                }
                _ => OK,
            }
        }
    }

    fn sample() -> EeUnitsRtsPlayerFields {
        EeUnitsRtsPlayerFields {
            mNextUniqueID: 77,
            mPlayerID: 3,
            mUnitCount: 2,
            mSlot: 0xff,
            mSeesAll: true,
            mSaveVisMap: true,
            mObserverMode: true,
        }
    }

    #[test]
    fn save_then_load_round_trips_with_the_parts_in_order() {
        let mut f = File::default();
        let mut rec = sample();
        assert_eq!(save(&mut f, &mut rec), OK);
        assert_eq!(
            f.log,
            vec![
                EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
                EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
                EE_UNITS_RTS_PLAYER_SAVE_COLOR_SAVE,
                EE_UNITS_RTS_PLAYER_SAVE_NAME_SAVE,
                EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
                EE_UNITS_RTS_PLAYER_SAVE_SAVE_UNITS,
                EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
            ]
        );
        // the save writes the unit count where the load reads the hash bucket length
        let mut back = EeUnitsRtsPlayerFields { mSlot: 9, ..Default::default() };
        f.log.clear();
        assert_eq!(load(&mut f, &mut back, 0, 0x118), OK);
        assert_eq!(f.loaded_units, 2);
        assert_eq!(back, EeUnitsRtsPlayerFields { mSlot: 9, mUnitCount: 0, ..sample() });
        let shade = f.log.iter().position(|&o| o == EE_UNITS_RTS_PLAYER_SAVE_SHADE).unwrap();
        assert_eq!(f.log[shade - 1], EE_UNITS_RTS_PLAYER_SAVE_COLOR_LOAD);
        assert_eq!(f.log[shade + 1], EE_UNITS_RTS_PLAYER_SAVE_NAME_LOAD);
        assert_eq!(f.log.iter().filter(|&&o| o == EE_UNITS_RTS_PLAYER_SAVE_ADD_PENDING).count(), 1);
        assert!(f.data.is_empty());
    }

    #[test]
    fn old_saves_clear_sees_all() {
        let mut f = File::default();
        let mut rec = sample();
        save(&mut f, &mut rec);
        let mut back = EeUnitsRtsPlayerFields::default();
        assert_eq!(load(&mut f, &mut back, 0, 0xcf), OK);
        assert!(!back.mSeesAll);
        assert!(back.mSaveVisMap && back.mObserverMode);
    }

    #[test]
    fn a_failed_read_stops_where_the_original_did() {
        // the third read (the next unique id) fails: no colour, no units, no pending units
        let mut f = File::default();
        save(&mut f, &mut sample());
        f.log.clear();
        f.fail_read = Some(3);
        let mut back = EeUnitsRtsPlayerFields::default();
        assert_eq!(load(&mut f, &mut back, 0, 0x118), 3);
        assert!(!f.log.contains(&EE_UNITS_RTS_PLAYER_SAVE_COLOR_LOAD));
        assert!(!f.log.contains(&EE_UNITS_RTS_PLAYER_SAVE_ADD_PENDING));
        // a failed bucket count: the pending units are added, the observer flag is not read
        let mut f = File::default();
        save(&mut f, &mut sample());
        f.log.clear();
        f.fail_read = Some(8);
        let mut back = EeUnitsRtsPlayerFields::default();
        assert_eq!(load(&mut f, &mut back, 0, 0x118), 3);
        assert!(f.log.contains(&EE_UNITS_RTS_PLAYER_SAVE_ADD_PENDING));
        assert!(!back.mObserverMode);
        assert_eq!(f.loaded_units, 0);
    }

    #[test]
    fn a_failed_write_returns_at_once() {
        struct Fails(Vec<i32>);
        impl PlayerSaveHost for Fails {
            fn call(&mut self, op: i32, _: u32, _: &mut EeUnitsPlayerSaveIo) -> i32 {
                self.0.push(op);
                i32::from(op == EE_UNITS_RTS_PLAYER_SAVE_NAME_SAVE) * 5
            }
        }
        let mut f = Fails(Vec::new());
        assert_eq!(save(&mut f, &mut sample()), 5);
        assert_eq!(f.0.last(), Some(&EE_UNITS_RTS_PLAYER_SAVE_NAME_SAVE));
        assert_eq!(f.0.len(), 6);
    }
}
