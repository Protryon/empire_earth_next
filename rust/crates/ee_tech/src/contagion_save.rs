//! The contagion helpers' and their manager's saved form and checksums (`src/Empire Earth/Calamities/
//! EEContagionHelper.cpp`, `EEContagionManager.cpp`).
//!
//! A helper's numbers are the record [`EeContagionHelperFields`] `EEContagionHelper` derives from; its units, unit
//! ids, functor and calamity stay C++ members. `Load`/`Save` of both classes run over one I/O callback
//! (`EE_CONTAGION_IO_*`): the numbers through the `FSFile` overload the original called, the C++ lists by part.

// the record keeps the C++ member names; c_long/c_ulong are 32 bits on Windows: the casts are needed elsewhere
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// A contagion helper's numbers (`EEContagionHelper` derives from it).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeContagionHelperFields {
    pub mHelperID: c_long,
    pub mTimeSinceLastSpread: f32,
    pub mTimeSinceLastUpdate: f32,
}

/// `FSFile::Read(unsigned long&)` into `*value` (a `c_ulong`; the old value in, the read value out).
pub const EE_CONTAGION_IO_READ_ULONG: u32 = 0;
/// `FSFile::Read(long&)` into `*value` (a `c_long`).
pub const EE_CONTAGION_IO_READ_LONG: u32 = 1;
/// `FSFile::Read(float&)` into `*value` (an `f32`).
pub const EE_CONTAGION_IO_READ_FLOAT: u32 = 2;
/// `FSFile::Write(const unsigned long&)` of `*value`.
pub const EE_CONTAGION_IO_WRITE_ULONG: u32 = 3;
/// `FSFile::Write(const long&)` of `*value`.
pub const EE_CONTAGION_IO_WRITE_LONG: u32 = 4;
/// `FSFile::Write(const float&)` of `*value`.
pub const EE_CONTAGION_IO_WRITE_FLOAT: u32 = 5;
/// The helper: `mUnitIDs.push_back(*value)` (a `c_ulong`).
pub const EE_CONTAGION_IO_PUSH_UNIT_ID: u32 = 6;
/// The helper: `mUnits.size()` into `*value` (a `c_ulong`).
pub const EE_CONTAGION_IO_UNIT_COUNT: u32 = 7;
/// The helper: unit `*value`'s `mUnitID` into `*value` (a `c_ulong`, the index in, the id out).
pub const EE_CONTAGION_IO_UNIT_ID: u32 = 8;
/// The manager: delete every helper and clear the calamity indexes.
pub const EE_CONTAGION_IO_CLEAR: u32 = 9;
/// The manager: a new helper loaded by its own `Load` (its result ignored) and kept with the calamity index
/// `*value` (a `c_ulong`).
pub const EE_CONTAGION_IO_LOAD_HELPER: u32 = 10;
/// The manager: `mHelpers.size()` into `*value` (a `c_ulong`).
pub const EE_CONTAGION_IO_HELPER_COUNT: u32 = 11;
/// The manager: helper `*value`'s calamity index among the active calamities into `*value` (a `c_ulong`, the
/// index in); the result 0 when it has one, else not 0.
pub const EE_CONTAGION_IO_HELPER_CALAMITY: u32 = 12;
/// The manager: helper `*value`'s own `Save` (the index in; its result).
pub const EE_CONTAGION_IO_SAVE_HELPER: u32 = 13;

/// The C++'s file and object: `op` on `value` (the op's type), the `FSFile` result (0 is `kErrorOK`).
pub trait ContagionIo {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long;
}

fn ulong<H: ContagionIo>(h: &mut H, op: u32, value: &mut c_ulong) -> c_long {
    h.io(op, value as *mut c_ulong as *mut c_void)
}
fn long<H: ContagionIo>(h: &mut H, op: u32, value: &mut c_long) -> c_long {
    h.io(op, value as *mut c_long as *mut c_void)
}
fn float<H: ContagionIo>(h: &mut H, op: u32, value: &mut f32) -> c_long {
    h.io(op, value as *mut f32 as *mut c_void)
}

/// Whether a save of this version has no update timer (major 0, minor below 0xe7 = 0.231).
pub fn without_update_timer(major: c_ulong, minor: c_ulong) -> bool {
    major == 0 && minor < 0xe7
}

/// `EEContagionHelper::Load`: the unit count and ids (each read pushed, the loop left at the first error), the
/// helper id, the spread timer and, from 0.231 on, the update timer; the first error.
pub fn helper_load<H: ContagionIo>(
    h: &mut H,
    f: &mut EeContagionHelperFields,
    major: c_ulong,
    minor: c_ulong,
) -> c_long {
    let mut count: c_ulong = 0;
    let mut id: c_ulong = 0;
    let mut result = ulong(h, EE_CONTAGION_IO_READ_ULONG, &mut count);
    if result != 0 {
        return result;
    }
    let mut i: c_ulong = 0;
    while i < count && result == 0 {
        result = ulong(h, EE_CONTAGION_IO_READ_ULONG, &mut id);
        ulong(h, EE_CONTAGION_IO_PUSH_UNIT_ID, &mut id);
        i += 1;
    }
    if result != 0 {
        return result;
    }
    result = long(h, EE_CONTAGION_IO_READ_LONG, &mut f.mHelperID);
    if result != 0 {
        return result;
    }
    result = float(h, EE_CONTAGION_IO_READ_FLOAT, &mut f.mTimeSinceLastSpread);
    if result == 0 && !without_update_timer(major, minor) {
        result = float(h, EE_CONTAGION_IO_READ_FLOAT, &mut f.mTimeSinceLastUpdate);
    }
    result
}

/// `EEContagionHelper::Save`: the unit count and ids (stopping at the first error), the helper id, both timers.
pub fn helper_save<H: ContagionIo>(h: &mut H, f: &EeContagionHelperFields) -> c_long {
    let mut count: c_ulong = 0;
    ulong(h, EE_CONTAGION_IO_UNIT_COUNT, &mut count);
    let mut result = ulong(h, EE_CONTAGION_IO_WRITE_ULONG, &mut count.clone());
    if result != 0 {
        return result;
    }
    let mut i: c_ulong = 0;
    while i < count && result == 0 {
        let mut id = i;
        ulong(h, EE_CONTAGION_IO_UNIT_ID, &mut id);
        result = ulong(h, EE_CONTAGION_IO_WRITE_ULONG, &mut id);
        i += 1;
    }
    if result != 0 {
        return result;
    }
    result = long(h, EE_CONTAGION_IO_WRITE_LONG, &mut f.mHelperID.clone());
    if result != 0 {
        return result;
    }
    result = float(h, EE_CONTAGION_IO_WRITE_FLOAT, &mut f.mTimeSinceLastSpread.clone());
    if result == 0 {
        result = float(h, EE_CONTAGION_IO_WRITE_FLOAT, &mut f.mTimeSinceLastUpdate.clone());
    }
    result
}

/// `EEContagionHelper::GetOutOfSynchChecksum`: its units' ids (`unit_ids`, their sum as the C++'s `long`), its
/// calamity's checksum, its id and its spread timer's bits.
pub fn helper_checksum(f: &EeContagionHelperFields, unit_ids: c_long, calamity: c_long) -> c_long {
    let theChecksum = unit_ids.wrapping_add(calamity);
    f.mHelperID.wrapping_add(f.mTimeSinceLastSpread.to_bits() as i32 as c_long).wrapping_add(theChecksum)
}

/// `EEContagionManager::Load`: the helpers deleted, then each helper's calamity index and the helper (read while
/// every read succeeded), then the last helper id when `counter` (0.273 on; `next_id` 0 otherwise); the first error.
pub fn manager_load<H: ContagionIo>(h: &mut H, next_id: &mut c_long, counter: bool) -> c_long {
    let mut count: c_ulong = 0;
    h.io(EE_CONTAGION_IO_CLEAR, std::ptr::null_mut());
    let mut result = ulong(h, EE_CONTAGION_IO_READ_ULONG, &mut count);
    if result == 0 {
        let mut i: c_ulong = 0;
        while i < count && result == 0 {
            let mut index: c_ulong = 0;
            result = ulong(h, EE_CONTAGION_IO_READ_ULONG, &mut index);
            if result == 0 {
                ulong(h, EE_CONTAGION_IO_LOAD_HELPER, &mut index);
            }
            i += 1;
        }
    }
    *next_id = 0;
    if result == 0 && counter {
        result = long(h, EE_CONTAGION_IO_READ_LONG, next_id);
    }
    result
}

/// `EEContagionManager::Save`: the helper count, each helper's calamity index and the helper (9 for a helper whose
/// calamity is not active; stopping at the first error), the last helper id when `counter`; the first error.
pub fn manager_save<H: ContagionIo>(h: &mut H, next_id: c_long, counter: bool) -> c_long {
    let mut count: c_ulong = 0;
    ulong(h, EE_CONTAGION_IO_HELPER_COUNT, &mut count);
    let mut result = ulong(h, EE_CONTAGION_IO_WRITE_ULONG, &mut count.clone());
    if result == 0 {
        let mut i: c_ulong = 0;
        while i < count && result == 0 {
            let mut index = i;
            if ulong(h, EE_CONTAGION_IO_HELPER_CALAMITY, &mut index) == 0 {
                result = ulong(h, EE_CONTAGION_IO_WRITE_ULONG, &mut index);
                if result == 0 {
                    let mut at = i;
                    result = ulong(h, EE_CONTAGION_IO_SAVE_HELPER, &mut at);
                }
            } else {
                result = 9;
            }
            i += 1;
        }
    }
    if result == 0 && counter {
        result = long(h, EE_CONTAGION_IO_WRITE_LONG, &mut next_id.clone());
    }
    result
}

/// The C++'s I/O callback: `call(ctx, op, value)`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeContagionIoFfi {
    pub ctx: *mut c_void,
    pub call: Option<unsafe extern "C" fn(ctx: *mut c_void, op: u32, value: *mut c_void) -> c_long>,
}

struct FfiIo(EeContagionIoFfi);

impl ContagionIo for FfiIo {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long {
        match self.0.call {
            // SAFETY: the C++ callback's contract: `value` points at the op's type or is null
            Some(call) => unsafe { call(self.0.ctx, op, value) },
            None => 1,
        }
    }
}

// ---- the C ABI (`EEContagionHelper.cpp`, `EEContagionManager.cpp`) ------------------------------------------------
// SAFETY (every function below): the caller passes valid, aligned pointers; the record is copied in and out.

/// [`helper_load`] into `*fields`.
///
/// # Safety
/// `fields` valid for reads and writes; `io`'s callback valid, not touching `*fields`.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_load(
    fields: *mut EeContagionHelperFields,
    major: c_ulong,
    minor: c_ulong,
    io: EeContagionIoFfi,
) -> c_long {
    // SAFETY: the caller's contract
    let mut f = unsafe { *fields };
    let result = helper_load(&mut FfiIo(io), &mut f, major, minor);
    // SAFETY: as above
    unsafe { *fields = f };
    result
}

/// [`helper_save`] of `*fields`.
///
/// # Safety
/// `fields` valid for reads; `io`'s callback valid.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_save(
    fields: *const EeContagionHelperFields,
    io: EeContagionIoFfi,
) -> c_long {
    // SAFETY: the caller's contract
    let f = unsafe { *fields };
    helper_save(&mut FfiIo(io), &f)
}

/// [`helper_checksum`].
///
/// # Safety
/// `fields` valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_helper_checksum(
    fields: *const EeContagionHelperFields,
    unit_ids: c_long,
    calamity: c_long,
) -> c_long {
    // SAFETY: the caller's contract
    let f = unsafe { *fields };
    helper_checksum(&f, unit_ids, calamity)
}

/// [`manager_load`]; the last helper id into `*next_id`.
///
/// # Safety
/// `next_id` valid for writes; `io`'s callback valid, not touching `*next_id`.
#[no_mangle]
pub unsafe extern "C" fn ee_contagion_manager_load(
    next_id: *mut c_long,
    counter: bool,
    io: EeContagionIoFfi,
) -> c_long {
    let mut id: c_long = 0;
    let result = manager_load(&mut FfiIo(io), &mut id, counter);
    // SAFETY: the caller's contract
    unsafe { *next_id = id };
    result
}

/// [`manager_save`].
#[no_mangle]
pub extern "C" fn ee_contagion_manager_save(next_id: c_long, counter: bool, io: EeContagionIoFfi) -> c_long {
    manager_save(&mut FfiIo(io), next_id, counter)
}

#[cfg(test)]
#[path = "contagion_save_tests.rs"]
mod tests;
