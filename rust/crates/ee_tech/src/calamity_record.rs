//! One active calamity's numbers and their saved form (`src/Empire Earth/Calamities/EECalamity.cpp`).
//!
//! [`EeCalamityFields`] is the record `EECalamity` derives from: its plain members with their C++ names and types.
//! The rest stays C++ members (the rectangle, the target and shake locations, the effect generations' vector, the
//! remembered tile, the effect handle, the units, the class and the sound nodes). `Reset`'s numbers, the out-of-sync
//! checksum and `Load`/`Save` are here; `Load`/`Save` run over one I/O callback (`EE_CALAMITY_IO_*`): the numbers
//! through the `FSFile` overload the original called (`long`/`unsigned long` through their 4-byte adapters, `float`,
//! `bool`), the C++ members' parts by operation.
//!
//! The record is copied in, worked on, and copied out by the C++ around the call: no reference into C++ memory lives
//! across a call back into it.

// the record keeps the C++ member names; c_long/c_ulong are 32 bits on Windows: the casts are needed elsewhere
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

/// An active calamity's plain members (`EECalamity` derives from it).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityFields {
    /// 1.0 after `Reset`.
    pub mEpicenterFactor: f32,
    pub mFloatPlaceHolder1: f32,
    pub mFloatPlaceHolder2: f32,
    pub mHeight: f32,
    pub mTimeAlive: f32,
    pub mDiscreteUpdateTime: f32,
    pub mTimeElapsedSinceLastUpdate: f32,
    /// The area effect classes keep their contagion helper id here.
    pub mIntPlaceHolder1: c_long,
    pub mIntPlaceHolder2: c_long,
    pub mTargetUnitID: c_ulong,
    /// The caster unit's id.
    pub mOriginatingUnitID: c_ulong,
    pub mPlayerNumber: c_ulong,
    pub mBoolPlaceHolder1: bool,
    pub mBoolPlaceHolder2: bool,
    pub mInitialized: bool,
    pub mHasTargetLocation: bool,
    pub mHasTargetUnit: bool,
}

/// `Reset`'s numbers (the C++ releases the units and sounds, clears the class, the effect generations and the effect
/// handle).
pub fn reset(f: &mut EeCalamityFields) {
    f.mEpicenterFactor = 1.0;
    f.mHasTargetLocation = false;
    f.mHasTargetUnit = false;
    f.mOriginatingUnitID = !0;
    f.mTimeAlive = 0.0;
    f.mTargetUnitID = !0;
    f.mDiscreteUpdateTime = 0.0;
    f.mInitialized = false;
    f.mIntPlaceHolder1 = 0;
    f.mTimeElapsedSinceLastUpdate = 0.0;
    f.mIntPlaceHolder2 = 0;
    f.mBoolPlaceHolder1 = false;
    f.mBoolPlaceHolder2 = false;
    f.mHeight = 0.0;
    f.mPlayerNumber = 0;
    f.mFloatPlaceHolder1 = 0.0;
    f.mFloatPlaceHolder2 = 0.0;
}

/// A float's bits as the C++'s `*reinterpret_cast<int32_t*>` read them, widened to `long`.
fn bits(value: f32) -> c_long {
    value.to_bits() as i32 as c_long
}

/// `GetOutOfSynchChecksum`: the target location's and the float members' bit patterns, the tile, ids, placeholders,
/// player and flags, summed as the C++'s `long` (wrapping; the tile's `unsigned long`s converted).
pub fn checksum(f: &EeCalamityFields, target: [f32; 3], tile: [c_ulong; 2]) -> c_long {
    let flags = [f.mBoolPlaceHolder1, f.mBoolPlaceHolder2, f.mInitialized, f.mHasTargetLocation, f.mHasTargetUnit];
    let mut sum = bits(target[0]).wrapping_add(bits(target[1]));
    let mut rest = bits(target[2])
        .wrapping_add(tile[0] as c_long)
        .wrapping_add(tile[1] as c_long)
        .wrapping_add(f.mIntPlaceHolder1)
        .wrapping_add(f.mIntPlaceHolder2)
        .wrapping_add(f.mTargetUnitID as c_long)
        .wrapping_add(f.mOriginatingUnitID as c_long)
        .wrapping_add(f.mPlayerNumber as c_long);
    for flag in flags {
        rest = rest.wrapping_add(flag as c_long);
    }
    sum = sum.wrapping_add(rest);
    for value in
        [f.mFloatPlaceHolder1, f.mFloatPlaceHolder2, f.mHeight, f.mDiscreteUpdateTime, f.mTimeElapsedSinceLastUpdate]
    {
        sum = sum.wrapping_add(bits(value));
    }
    sum
}

/// `FSFile::Read(unsigned long&)` into `*value` (a `c_ulong`; the old value in, the read value out).
pub const EE_CALAMITY_IO_READ_ULONG: u32 = 0;
/// `FSFile::Read(long&)` into `*value` (a `c_long`).
pub const EE_CALAMITY_IO_READ_LONG: u32 = 1;
/// `FSFile::Read(float&)` into `*value` (an `f32`).
pub const EE_CALAMITY_IO_READ_FLOAT: u32 = 2;
/// `FSFile::Read(bool&)` into `*value` (a `bool`).
pub const EE_CALAMITY_IO_READ_BOOL: u32 = 3;
/// `FSFile::Write(const unsigned long&)` of `*value`.
pub const EE_CALAMITY_IO_WRITE_ULONG: u32 = 4;
/// `FSFile::Write(const long&)` of `*value`.
pub const EE_CALAMITY_IO_WRITE_LONG: u32 = 5;
/// `FSFile::Write(const float&)` of `*value`.
pub const EE_CALAMITY_IO_WRITE_FLOAT: u32 = 6;
/// `FSFile::Write(const bool&)` of `*value`.
pub const EE_CALAMITY_IO_WRITE_BOOL: u32 = 7;
/// `mRectangle.Load` / `Save` (`value` unused).
pub const EE_CALAMITY_IO_RECTANGLE: u32 = 8;
/// `mTargetLocation.Load` / `Save`.
pub const EE_CALAMITY_IO_TARGET_LOCATION: u32 = 9;
/// `mShakeLocation.Load` / `Save`.
pub const EE_CALAMITY_IO_SHAKE_LOCATION: u32 = 10;
/// `FSLoadVector` / `FSSaveVector` of `mEffectGeneration`.
pub const EE_CALAMITY_IO_EFFECT_GENERATION: u32 = 11;
/// Load: a `long` class id read and the class it names set (`GetCalamityClass(unsigned long)`); save: the class's
/// `mID` written as an `unsigned long`.
pub const EE_CALAMITY_IO_CLASS: u32 = 12;
/// Save: the target unit's id (-1 without one) into `*value` (a `c_ulong`).
pub const EE_CALAMITY_IO_TARGET_ID: u32 = 13;
/// Save: the caster unit's id (-1 without one) into `*value` (a `c_ulong`).
pub const EE_CALAMITY_IO_CASTER_ID: u32 = 14;
/// Load: a `long` read into the remembered tile's x; save: the tile's x written as a `long`.
pub const EE_CALAMITY_IO_TILE_X: u32 = 15;
/// The same for its y.
pub const EE_CALAMITY_IO_TILE_Y: u32 = 16;
/// `mEffectHandle.mValue` read or written (a `uint32_t` through `FSFile`'s own overload).
pub const EE_CALAMITY_IO_EFFECT_HANDLE: u32 = 17;

/// The C++'s file and calamity: `op` on `value` (the op's type), the `FSFile` result (0 is `kErrorOK`).
pub trait CalamityIo {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long;
}

fn read_ulong<H: CalamityIo>(h: &mut H, field: &mut c_ulong) -> c_long {
    h.io(EE_CALAMITY_IO_READ_ULONG, field as *mut c_ulong as *mut c_void)
}
fn read_long<H: CalamityIo>(h: &mut H, field: &mut c_long) -> c_long {
    h.io(EE_CALAMITY_IO_READ_LONG, field as *mut c_long as *mut c_void)
}
fn read_f32<H: CalamityIo>(h: &mut H, field: &mut f32) -> c_long {
    h.io(EE_CALAMITY_IO_READ_FLOAT, field as *mut f32 as *mut c_void)
}
fn read_bool<H: CalamityIo>(h: &mut H, field: &mut bool) -> c_long {
    // the byte crosses as 0 or 1 (the C++ reads into a `bool`)
    h.io(EE_CALAMITY_IO_READ_BOOL, field as *mut bool as *mut c_void)
}
fn part<H: CalamityIo>(h: &mut H, op: u32) -> c_long {
    h.io(op, std::ptr::null_mut())
}
fn write_ulong<H: CalamityIo>(h: &mut H, mut value: c_ulong) -> c_long {
    h.io(EE_CALAMITY_IO_WRITE_ULONG, &mut value as *mut c_ulong as *mut c_void)
}
fn write_long<H: CalamityIo>(h: &mut H, mut value: c_long) -> c_long {
    h.io(EE_CALAMITY_IO_WRITE_LONG, &mut value as *mut c_long as *mut c_void)
}
fn write_f32<H: CalamityIo>(h: &mut H, mut value: f32) -> c_long {
    h.io(EE_CALAMITY_IO_WRITE_FLOAT, &mut value as *mut f32 as *mut c_void)
}
fn write_bool<H: CalamityIo>(h: &mut H, mut value: bool) -> c_long {
    h.io(EE_CALAMITY_IO_WRITE_BOOL, &mut value as *mut bool as *mut c_void)
}

/// One step of `Load`: each runs while the steps before it read without an error.
enum LoadStep {
    Part(u32),
    ULong(fn(&mut EeCalamityFields) -> &mut c_ulong),
    Long(fn(&mut EeCalamityFields) -> &mut c_long),
    Float(fn(&mut EeCalamityFields) -> &mut f32),
    Bool(fn(&mut EeCalamityFields) -> &mut bool),
}

/// `Load`: the rectangle, target and shake locations, effect generations, target id, class, the numbers, the tile,
/// the effect handle and the flags, in the original's order; each read only while the ones before it succeeded; the
/// first error (0 when every read succeeded).
pub fn load<H: CalamityIo>(h: &mut H, f: &mut EeCalamityFields) -> c_long {
    use LoadStep::*;
    let steps: [LoadStep; 25] = [
        Part(EE_CALAMITY_IO_RECTANGLE),
        Part(EE_CALAMITY_IO_TARGET_LOCATION),
        Part(EE_CALAMITY_IO_SHAKE_LOCATION),
        Part(EE_CALAMITY_IO_EFFECT_GENERATION),
        ULong(|f| &mut f.mTargetUnitID),
        Part(EE_CALAMITY_IO_CLASS),
        Float(|f| &mut f.mEpicenterFactor),
        Float(|f| &mut f.mFloatPlaceHolder1),
        Float(|f| &mut f.mFloatPlaceHolder2),
        Float(|f| &mut f.mHeight),
        Long(|f| &mut f.mIntPlaceHolder1),
        Long(|f| &mut f.mIntPlaceHolder2),
        Float(|f| &mut f.mTimeAlive),
        ULong(|f| &mut f.mOriginatingUnitID),
        ULong(|f| &mut f.mPlayerNumber),
        Float(|f| &mut f.mDiscreteUpdateTime),
        Float(|f| &mut f.mTimeElapsedSinceLastUpdate),
        Part(EE_CALAMITY_IO_TILE_X),
        Part(EE_CALAMITY_IO_TILE_Y),
        Part(EE_CALAMITY_IO_EFFECT_HANDLE),
        Bool(|f| &mut f.mBoolPlaceHolder1),
        Bool(|f| &mut f.mBoolPlaceHolder2),
        Bool(|f| &mut f.mInitialized),
        Bool(|f| &mut f.mHasTargetLocation),
        Bool(|f| &mut f.mHasTargetUnit),
    ];
    let mut result = 0;
    for step in steps {
        result = match step {
            Part(op) => part(h, op),
            ULong(field) => read_ulong(h, field(f)),
            Long(field) => read_long(h, field(f)),
            Float(field) => read_f32(h, field(f)),
            Bool(field) => read_bool(h, field(f)),
        };
        if result != 0 {
            break;
        }
    }
    result
}

/// `Save`: the same order, the target and caster as their ids (-1 without one), the class as its `mID`; stops at
/// the first error and returns it.
pub fn save<H: CalamityIo>(h: &mut H, f: &EeCalamityFields) -> c_long {
    let mut result = 0;
    let mut id: c_ulong = 0;
    for op in [
        EE_CALAMITY_IO_RECTANGLE,
        EE_CALAMITY_IO_TARGET_LOCATION,
        EE_CALAMITY_IO_SHAKE_LOCATION,
        EE_CALAMITY_IO_EFFECT_GENERATION,
    ] {
        result = part(h, op);
        if result != 0 {
            return result;
        }
    }
    h.io(EE_CALAMITY_IO_TARGET_ID, &mut id as *mut c_ulong as *mut c_void);
    let steps: [&dyn Fn(&mut H) -> c_long; 21] = [
        &|h| write_ulong(h, id),
        &|h| part(h, EE_CALAMITY_IO_CLASS),
        &|h| write_f32(h, f.mEpicenterFactor),
        &|h| write_f32(h, f.mFloatPlaceHolder1),
        &|h| write_f32(h, f.mFloatPlaceHolder2),
        &|h| write_f32(h, f.mHeight),
        &|h| write_long(h, f.mIntPlaceHolder1),
        &|h| write_long(h, f.mIntPlaceHolder2),
        &|h| write_f32(h, f.mTimeAlive),
        &|h| {
            let mut caster: c_ulong = 0;
            h.io(EE_CALAMITY_IO_CASTER_ID, &mut caster as *mut c_ulong as *mut c_void);
            write_ulong(h, caster)
        },
        &|h| write_ulong(h, f.mPlayerNumber),
        &|h| write_f32(h, f.mDiscreteUpdateTime),
        &|h| write_f32(h, f.mTimeElapsedSinceLastUpdate),
        &|h| part(h, EE_CALAMITY_IO_TILE_X),
        &|h| part(h, EE_CALAMITY_IO_TILE_Y),
        &|h| part(h, EE_CALAMITY_IO_EFFECT_HANDLE),
        &|h| write_bool(h, f.mBoolPlaceHolder1),
        &|h| write_bool(h, f.mBoolPlaceHolder2),
        &|h| write_bool(h, f.mInitialized),
        &|h| write_bool(h, f.mHasTargetLocation),
        &|h| write_bool(h, f.mHasTargetUnit),
    ];
    for step in steps {
        result = step(h);
        if result != 0 {
            break;
        }
    }
    result
}

/// The C++'s I/O callback: `call(ctx, op, value)`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCalamityIoFfi {
    pub ctx: *mut c_void,
    pub call: Option<unsafe extern "C" fn(ctx: *mut c_void, op: u32, value: *mut c_void) -> c_long>,
}

struct FfiIo(EeCalamityIoFfi);

impl CalamityIo for FfiIo {
    fn io(&mut self, op: u32, value: *mut c_void) -> c_long {
        match self.0.call {
            // SAFETY: the C++ callback's contract: `value` points at the op's type or is null for a part
            Some(call) => unsafe { call(self.0.ctx, op, value) },
            None => 1,
        }
    }
}

// ---- the C ABI (`EECalamity.cpp`) ----------------------------------------------------------------------------------
// SAFETY (every function below): the caller passes valid, aligned pointers; the record is copied in and out.

/// [`reset`] of `*fields`.
///
/// # Safety
/// `fields` valid for reads and writes.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_record_reset(fields: *mut EeCalamityFields) {
    // SAFETY: the caller's contract
    let mut f = unsafe { *fields };
    reset(&mut f);
    // SAFETY: as above
    unsafe { *fields = f };
}

/// [`checksum`].
///
/// # Safety
/// `fields` valid for reads.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_record_checksum(
    fields: *const EeCalamityFields,
    target_x: f32,
    target_y: f32,
    target_z: f32,
    tile_x: c_ulong,
    tile_y: c_ulong,
) -> c_long {
    // SAFETY: the caller's contract
    let f = unsafe { *fields };
    checksum(&f, [target_x, target_y, target_z], [tile_x, tile_y])
}

/// [`load`] into `*fields` over `io`; the first error.
///
/// # Safety
/// `fields` valid for reads and writes; `io`'s callback valid, not touching `*fields`.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_record_load(fields: *mut EeCalamityFields, io: EeCalamityIoFfi) -> c_long {
    // SAFETY: the caller's contract
    let mut f = unsafe { *fields };
    let result = load(&mut FfiIo(io), &mut f);
    // SAFETY: as above
    unsafe { *fields = f };
    result
}

/// [`save`] of `*fields` over `io`; the first error.
///
/// # Safety
/// `fields` valid for reads; `io`'s callback valid.
#[no_mangle]
pub unsafe extern "C" fn ee_calamity_record_save(fields: *const EeCalamityFields, io: EeCalamityIoFfi) -> c_long {
    // SAFETY: the caller's contract
    let f = unsafe { *fields };
    save(&mut FfiIo(io), &f)
}

#[cfg(test)]
#[path = "calamity_record_tests.rs"]
mod tests;
