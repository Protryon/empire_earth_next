//! The area effects' running state in a saved game (port, save version 0.256; `EEAEManager.cpp`'s
//! `EEAESaveRunningState` / `EEAELoadRunningState`): per unit that owns or is affected by an effect, its own effects'
//! locations (and the morale from buildings' recount flag and count), then the effects acting on it, each as its
//! owner unit's id and its index in the owner's own list; then the active list the same way.
//!
//! The C++ walks its units into an [`EeAreaEffectRunning`] and saves it through the calamities' I/O callback
//! ([`crate::calamity_record::EeCalamityIoFfi`]: `FSFile`'s own typed reads and writes); a load gives one back, which
//! the C++ puts back on the units at the loaded game's first tick.

// the records keep the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case)]

use crate::tech::calamity_record::{
    read_bool, read_f32, read_long, read_ulong, write_bool, write_f32, write_long, write_ulong, CalamityIo,
    EeCalamityIoFfi, FfiIo,
};
use std::os::raw::{c_long, c_ulong};

/// An effect as its owner unit's id (`-1`: none) and its index in the owner's own list (`-1`: not there).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct EeAreaEffectSavedRef {
    pub mOwner: c_ulong,
    pub mIndex: c_long,
}

impl Default for EeAreaEffectSavedRef {
    fn default() -> Self {
        EeAreaEffectSavedRef { mOwner: c_ulong::MAX, mIndex: -1 }
    }
}

/// One of a unit's own effects: its location and, for a morale from buildings, its recount flag and count.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeAreaEffectSavedOwn {
    pub mX: f32,
    pub mY: f32,
    pub mIsMorale: bool,
    pub mDirty: bool,
    pub mCount: c_long,
}

/// One unit's saved effects.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SavedUnit {
    pub id: c_ulong,
    pub own: Vec<EeAreaEffectSavedOwn>,
    pub affecting: Vec<EeAreaEffectSavedRef>,
}

/// The running state: the units' effects and the active list.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeAreaEffectRunning {
    pub units: Vec<SavedUnit>,
    pub active: Vec<EeAreaEffectSavedRef>,
}

fn write_refs<H: CalamityIo>(h: &mut H, refs: &[EeAreaEffectSavedRef]) {
    write_ulong(h, refs.len() as c_ulong);
    for r in refs {
        write_ulong(h, r.mOwner);
        write_long(h, r.mIndex);
    }
}

/// `EEAESaveRunningState`'s writes (their results unread, as the C++'s).
pub fn save<H: CalamityIo>(h: &mut H, r: &EeAreaEffectRunning) {
    write_ulong(h, r.units.len() as c_ulong);
    for u in &r.units {
        write_ulong(h, u.id);
        write_ulong(h, u.own.len() as c_ulong);
        for o in &u.own {
            write_f32(h, o.mX);
            write_f32(h, o.mY);
            write_bool(h, o.mIsMorale);
            if o.mIsMorale {
                write_bool(h, o.mDirty);
                write_long(h, o.mCount);
            }
        }
        write_refs(h, &u.affecting);
    }
    write_refs(h, &r.active);
}

/// A count and its refs; a ref whose read failed is kept (as read so far), and the reads stop.
fn read_refs<H: CalamityIo>(h: &mut H, out: &mut Vec<EeAreaEffectSavedRef>) -> c_long {
    let mut count: c_ulong = 0;
    let mut result = read_ulong(h, &mut count);
    let mut i: c_ulong = 0;
    while i < count && result == 0 {
        let mut r = EeAreaEffectSavedRef::default();
        result = read_ulong(h, &mut r.mOwner);
        if result == 0 {
            result = read_long(h, &mut r.mIndex);
        }
        out.push(r);
        i += 1;
    }
    result
}

/// `EEAELoadRunningState`'s reads: the first error stops them (the unit or effect being read is kept as read so
/// far); the state is put back only when the result is 0.
pub fn load<H: CalamityIo>(h: &mut H) -> (c_long, EeAreaEffectRunning) {
    let mut r = EeAreaEffectRunning::default();
    let mut count: c_ulong = 0;
    let mut result = read_ulong(h, &mut count);
    let mut u: c_ulong = 0;
    while u < count && result == 0 {
        let mut unit = SavedUnit::default();
        let mut own: c_ulong = 0;
        result = read_ulong(h, &mut unit.id);
        if result == 0 {
            result = read_ulong(h, &mut own);
        }
        let mut i: c_ulong = 0;
        while i < own && result == 0 {
            let mut o = EeAreaEffectSavedOwn::default();
            result = read_f32(h, &mut o.mX);
            if result == 0 {
                result = read_f32(h, &mut o.mY);
            }
            if result == 0 {
                result = read_bool(h, &mut o.mIsMorale);
            }
            if result == 0 && o.mIsMorale {
                result = read_bool(h, &mut o.mDirty);
                if result == 0 {
                    result = read_long(h, &mut o.mCount);
                }
            }
            unit.own.push(o);
            i += 1;
        }
        if result == 0 {
            result = read_refs(h, &mut unit.affecting);
        }
        r.units.push(unit);
        u += 1;
    }
    if result == 0 {
        result = read_refs(h, &mut r.active);
    }
    (result, r)
}

// ---- the C ABI (`EEAEManager.cpp`) ---------------------------------------------------------------------------------

/// A new, empty running state (the C++ fills it, then saves and frees it).
#[no_mangle]
pub extern "C" fn ee_area_effect_running_new() -> *mut EeAreaEffectRunning {
    Box::into_raw(Box::default())
}

/// Frees a running state (NULL: nothing).
///
/// # Safety
/// `r` is NULL or from [`ee_area_effect_running_new`] / [`ee_area_effect_running_load`], freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_free(r: *mut EeAreaEffectRunning) {
    if !r.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(r) });
    }
}

/// A unit appended (its effects follow).
///
/// # Safety
/// `r` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_add_unit(r: *mut EeAreaEffectRunning, id: c_ulong) {
    // SAFETY: the caller's contract
    unsafe { &mut *r }.units.push(SavedUnit { id, ..SavedUnit::default() });
}

/// One of the last unit's own effects appended.
///
/// # Safety
/// `r` valid, with a unit.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_add_own(r: *mut EeAreaEffectRunning, own: EeAreaEffectSavedOwn) {
    // SAFETY: the caller's contract
    if let Some(u) = unsafe { &mut *r }.units.last_mut() {
        u.own.push(own);
    }
}

/// An effect acting on the last unit appended (`active`: to the active list instead).
///
/// # Safety
/// `r` valid (with a unit unless `active`).
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_add_ref(
    r: *mut EeAreaEffectRunning,
    reference: EeAreaEffectSavedRef,
    active: bool,
) {
    // SAFETY: the caller's contract
    let r = unsafe { &mut *r };
    if active {
        r.active.push(reference);
    } else if let Some(u) = r.units.last_mut() {
        u.affecting.push(reference);
    }
}

/// [`save`] of `r` over `io`.
///
/// # Safety
/// `r` valid; `io`'s callback valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_save(r: *const EeAreaEffectRunning, io: EeCalamityIoFfi) {
    // SAFETY: the caller's contract
    save(&mut FfiIo(io), unsafe { &*r });
}

/// [`load`] over `io`: the result into `*result`, the state read (free it).
///
/// # Safety
/// `result` valid; `io`'s callback valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_load(
    io: EeCalamityIoFfi,
    result: *mut c_long,
) -> *mut EeAreaEffectRunning {
    let (code, r) = load(&mut FfiIo(io));
    // SAFETY: the caller's contract
    unsafe { *result = code };
    Box::into_raw(Box::new(r))
}

/// The number of units (`unit` [`usize::MAX`]: of the active list's refs; else the unit's own effects into
/// `*own` and its affecting refs into `*affecting`, its id returned).
///
/// # Safety
/// `r` valid; `unit` below the count; `own` and `affecting` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_unit(
    r: *const EeAreaEffectRunning,
    unit: usize,
    own: *mut usize,
    affecting: *mut usize,
) -> c_ulong {
    // SAFETY: the caller's contract
    let (r, own, affecting) = unsafe { (&*r, &mut *own, &mut *affecting) };
    let u = &r.units[unit];
    *own = u.own.len();
    *affecting = u.affecting.len();
    u.id
}

/// The number of units saved (`active`: of the active list's refs).
///
/// # Safety
/// `r` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_count(r: *const EeAreaEffectRunning, active: bool) -> usize {
    // SAFETY: the caller's contract
    let r = unsafe { &*r };
    if active {
        r.active.len()
    } else {
        r.units.len()
    }
}

/// Unit `unit`'s own effect `index`.
///
/// # Safety
/// `r` valid; the indexes in range.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_own(
    r: *const EeAreaEffectRunning,
    unit: usize,
    index: usize,
) -> EeAreaEffectSavedOwn {
    // SAFETY: the caller's contract
    unsafe { &*r }.units[unit].own[index]
}

/// Unit `unit`'s affecting ref `index` (`unit` [`usize::MAX`]: the active list's).
///
/// # Safety
/// `r` valid; the indexes in range.
#[no_mangle]
pub unsafe extern "C" fn ee_area_effect_running_ref(
    r: *const EeAreaEffectRunning,
    unit: usize,
    index: usize,
) -> EeAreaEffectSavedRef {
    // SAFETY: the caller's contract
    let r = unsafe { &*r };
    if unit == usize::MAX {
        r.active[index]
    } else {
        r.units[unit].affecting[index]
    }
}

#[cfg(test)]
// c_long/c_ulong are i32/u32 on Windows only: the casts are needed elsewhere
#[allow(clippy::unnecessary_cast)]
mod tests {
    use super::*;
    use crate::tech::calamity_record::{
        EE_CALAMITY_IO_READ_BOOL, EE_CALAMITY_IO_READ_FLOAT, EE_CALAMITY_IO_READ_LONG, EE_CALAMITY_IO_READ_ULONG,
        EE_CALAMITY_IO_WRITE_BOOL, EE_CALAMITY_IO_WRITE_FLOAT, EE_CALAMITY_IO_WRITE_LONG, EE_CALAMITY_IO_WRITE_ULONG,
    };
    use std::os::raw::c_void;

    /// An in-memory file in the FSFile encodings (4-byte little-endian numbers, 1-byte bools); reads fail past
    /// `limit` bytes.
    #[derive(Default)]
    struct Mem {
        bytes: Vec<u8>,
        at: usize,
        limit: Option<usize>,
    }

    impl CalamityIo for Mem {
        fn io(&mut self, op: u32, value: *mut c_void) -> c_long {
            // SAFETY: the ops' types as the helpers pass them
            unsafe {
                let size = match op {
                    EE_CALAMITY_IO_READ_BOOL | EE_CALAMITY_IO_WRITE_BOOL => 1,
                    _ => 4,
                };
                match op {
                    EE_CALAMITY_IO_WRITE_ULONG => self.bytes.extend((*(value as *const c_ulong) as u32).to_le_bytes()),
                    EE_CALAMITY_IO_WRITE_LONG => self.bytes.extend((*(value as *const c_long) as i32).to_le_bytes()),
                    EE_CALAMITY_IO_WRITE_FLOAT => self.bytes.extend((*(value as *const f32)).to_le_bytes()),
                    EE_CALAMITY_IO_WRITE_BOOL => self.bytes.push(*(value as *const bool) as u8),
                    _ => {
                        if self.at + size > self.limit.unwrap_or(self.bytes.len()) {
                            return 5;
                        }
                        let b = &self.bytes[self.at..self.at + size];
                        let word = if size == 4 { u32::from_le_bytes([b[0], b[1], b[2], b[3]]) } else { b[0] as u32 };
                        match op {
                            EE_CALAMITY_IO_READ_ULONG => *(value as *mut c_ulong) = word as c_ulong,
                            EE_CALAMITY_IO_READ_LONG => *(value as *mut c_long) = word as i32 as c_long,
                            EE_CALAMITY_IO_READ_FLOAT => *(value as *mut f32) = f32::from_bits(word),
                            EE_CALAMITY_IO_READ_BOOL => *(value as *mut bool) = word != 0,
                            _ => return 9,
                        }
                        self.at += size;
                    }
                }
            }
            0
        }
    }

    fn sample() -> EeAreaEffectRunning {
        EeAreaEffectRunning {
            units: vec![
                SavedUnit {
                    id: 17,
                    own: vec![
                        EeAreaEffectSavedOwn { mX: 1.5, mY: 2.5, ..Default::default() },
                        EeAreaEffectSavedOwn { mX: 3.0, mY: -1.0, mIsMorale: true, mDirty: true, mCount: 4 },
                    ],
                    affecting: vec![EeAreaEffectSavedRef { mOwner: 17, mIndex: 1 }],
                },
                SavedUnit { id: 30, own: vec![], affecting: vec![EeAreaEffectSavedRef::default()] },
            ],
            active: vec![
                EeAreaEffectSavedRef { mOwner: 17, mIndex: 0 },
                EeAreaEffectSavedRef { mOwner: 17, mIndex: 1 },
            ],
        }
    }

    #[test]
    fn the_saved_bytes_and_their_load() {
        let mut m = Mem::default();
        save(&mut m, &sample());
        let mut want = Vec::new();
        for w in [2u32, 17, 2] {
            want.extend(w.to_le_bytes());
        }
        want.extend(1.5f32.to_le_bytes());
        want.extend(2.5f32.to_le_bytes());
        want.push(0);
        want.extend(3.0f32.to_le_bytes());
        want.extend((-1.0f32).to_le_bytes());
        want.extend([1, 1]);
        want.extend(4u32.to_le_bytes());
        for w in [1u32, 17, 1, 30, 0, 1, u32::MAX, u32::MAX, 2, 17, 0, 17, 1] {
            want.extend(w.to_le_bytes());
        }
        assert_eq!(m.bytes, want);
        let (result, back) = load(&mut m);
        assert_eq!((result, back), (0, sample()));
    }

    #[test]
    fn a_failed_read_keeps_what_was_read_and_stops() {
        let mut full = Mem::default();
        save(&mut full, &sample());
        // the second own effect's y cut off: it is kept with x read, and nothing after it is read
        let mut m = Mem { bytes: full.bytes.clone(), limit: Some(12 + 9 + 4), ..Default::default() };
        let (result, r) = load(&mut m);
        assert_eq!(result, 5);
        assert_eq!(r.units.len(), 1);
        assert_eq!(r.units[0].own, [sample().units[0].own[0], EeAreaEffectSavedOwn { mX: 3.0, ..Default::default() }]);
        assert!(r.units[0].affecting.is_empty() && r.active.is_empty());
        // an empty file
        let (result, r) = load(&mut Mem::default());
        assert_eq!((result, r), (5, EeAreaEffectRunning::default()));
    }

    #[test]
    fn the_c_abi_builds_and_reads_a_state() {
        // SAFETY: a state from new, freed once
        unsafe {
            let r = ee_area_effect_running_new();
            ee_area_effect_running_add_ref(r, EeAreaEffectSavedRef { mOwner: 5, mIndex: 0 }, true);
            ee_area_effect_running_add_unit(r, 9);
            ee_area_effect_running_add_own(r, EeAreaEffectSavedOwn { mX: 1.0, ..Default::default() });
            ee_area_effect_running_add_ref(r, EeAreaEffectSavedRef { mOwner: 5, mIndex: 2 }, false);
            let (mut own, mut affecting) = (0usize, 0usize);
            assert_eq!(ee_area_effect_running_count(r, false), 1);
            assert_eq!(ee_area_effect_running_count(r, true), 1);
            assert_eq!(ee_area_effect_running_unit(r, 0, &mut own, &mut affecting), 9);
            assert_eq!((own, affecting), (1, 1));
            assert_eq!(ee_area_effect_running_own(r, 0, 0).mX, 1.0);
            assert_eq!(ee_area_effect_running_ref(r, 0, 0).mIndex, 2);
            assert_eq!(ee_area_effect_running_ref(r, usize::MAX, 0).mOwner, 5);
            ee_area_effect_running_free(r);
            ee_area_effect_running_free(std::ptr::null_mut());
        }
    }
}
