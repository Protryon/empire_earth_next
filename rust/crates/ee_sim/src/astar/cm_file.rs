//! The continent manager's regions block in a saved game's file (AI Thread cluster round 8; was EEAIContinentSave.cpp's
//! EESaveContinentRegions, EELoadContinentRegions, EEContinentRegionsPending and the block it kept for
//! EERestoreContinentRegions): a 32-bit size, then the bytes of `cm_io::write_regions`. A load keeps the block until
//! the regions are put back at the game's start (`cm_restore.rs`); a failed read keeps none.

use std::sync::Mutex;

use ee_base::fs::EeFsFileOps;

use crate::astar::cm::EeAStarCmRecord;

/// `kErrorOK`, `kErrorBeyondEnd` (lle/LowLevelEngineCore.h)
const ERROR_OK: i32 = 0;
const ERROR_BEYOND_END: i32 = 0xa;

/// The block of the save being loaded (empty: none).
static LOADED: Mutex<Vec<u8>> = Mutex::new(Vec::new());

fn loaded() -> std::sync::MutexGuard<'static, Vec<u8>> {
    LOADED.lock().unwrap_or_else(|e| e.into_inner())
}

/// Writes the block: its size, then its bytes (none for an empty one) -> the file's result.
///
/// # Safety
/// `ops` serves a file open for writing.
pub unsafe fn save(ops: &EeFsFileOps, bytes: &[u8]) -> i32 {
    // SAFETY: the caller's contract
    let mut result = unsafe { (ops.write_u32)(ops.context, bytes.len() as u32) };
    if result == ERROR_OK && !bytes.is_empty() {
        let mut written = 0u32;
        // SAFETY: as above
        result = unsafe { (ops.write_bytes)(ops.context, bytes.as_ptr(), bytes.len(), &mut written) };
    }
    result
}

/// Reads a block (its size, then its bytes; a short read is `kErrorBeyondEnd`) -> the result and the bytes (none
/// after a failure).
///
/// # Safety
/// `ops` serves a file open for reading.
pub unsafe fn load(ops: &EeFsFileOps) -> (i32, Vec<u8>) {
    let mut size = 0u32;
    // SAFETY: the caller's contract
    let mut result = unsafe { (ops.read_u32)(ops.context, &mut size) };
    let mut bytes = Vec::new();
    if result == ERROR_OK && size > 0 {
        bytes.resize(size as usize, 0);
        let mut read = 0u32;
        // SAFETY: as above; room for `size` bytes
        result = unsafe { (ops.read_bytes)(ops.context, bytes.as_mut_ptr(), bytes.len(), &mut read) };
        if result == ERROR_OK && read != size {
            result = ERROR_BEYOND_END;
        }
    }
    if result != ERROR_OK {
        bytes.clear();
    }
    (result, bytes)
}

/// `EESaveContinentRegions`: the manager's regions into the save.
///
/// # Safety
/// `r` is the manager's live record; `ops` serves the save's file.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_save_regions(r: *const EeAStarCmRecord, ops: EeFsFileOps) -> i32 {
    // SAFETY: the caller's contract
    unsafe { save(&ops, &crate::astar::cm_io::write_regions(&*r)) }
}

/// `EELoadContinentRegions`: the save's block, kept for the start of the game.
///
/// # Safety
/// `ops` serves the save's file.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_load_regions(ops: EeFsFileOps) -> i32 {
    loaded().clear();
    // SAFETY: the caller's contract
    let (result, bytes) = unsafe { load(&ops) };
    *loaded() = bytes;
    result
}

/// `EEContinentRegionsPending`: a loaded block waits.
#[no_mangle]
pub extern "C" fn ee_astar_cm_regions_pending() -> bool {
    !loaded().is_empty()
}

/// The loaded block, taken (none left).
pub fn take_loaded() -> Vec<u8> {
    std::mem::take(&mut *loaded())
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::c_void;

    struct Mem {
        data: Vec<u8>,
        at: usize,
    }

    unsafe extern "C" fn read_u32(c: *mut c_void, v: *mut u32) -> i32 {
        let m = unsafe { &mut *(c as *mut Mem) };
        if m.at + 4 > m.data.len() {
            return ERROR_BEYOND_END;
        }
        unsafe { *v = u32::from_le_bytes(m.data[m.at..m.at + 4].try_into().unwrap()) };
        m.at += 4;
        ERROR_OK
    }
    unsafe extern "C" fn read_bytes(c: *mut c_void, out: *mut u8, n: usize, read: *mut u32) -> i32 {
        let m = unsafe { &mut *(c as *mut Mem) };
        let k = n.min(m.data.len() - m.at);
        unsafe { std::ptr::copy_nonoverlapping(m.data[m.at..].as_ptr(), out, k) };
        m.at += k;
        unsafe { *read = k as u32 };
        ERROR_OK
    }
    unsafe extern "C" fn write_u32(c: *mut c_void, v: u32) -> i32 {
        unsafe { &mut *(c as *mut Mem) }.data.extend_from_slice(&v.to_le_bytes());
        ERROR_OK
    }
    unsafe extern "C" fn write_bytes(c: *mut c_void, d: *const u8, n: usize, w: *mut u32) -> i32 {
        unsafe { &mut *(c as *mut Mem) }.data.extend_from_slice(unsafe { std::slice::from_raw_parts(d, n) });
        unsafe { *w = n as u32 };
        ERROR_OK
    }

    fn ops(m: &mut Mem) -> EeFsFileOps {
        EeFsFileOps { context: (m as *mut Mem).cast(), read_u32, read_bytes, write_u32, write_bytes }
    }

    #[test]
    fn a_block_reads_back_as_written() {
        let mut m = Mem { data: Vec::new(), at: 0 };
        assert_eq!(unsafe { save(&ops(&mut m), b"regions") }, ERROR_OK);
        assert_eq!(m.data, [7, 0, 0, 0, b'r', b'e', b'g', b'i', b'o', b'n', b's']);
        assert_eq!(unsafe { load(&ops(&mut m)) }, (ERROR_OK, b"regions".to_vec()));
        // an empty block is its size alone
        let mut e = Mem { data: Vec::new(), at: 0 };
        unsafe { save(&ops(&mut e), b"") };
        assert_eq!(e.data, [0, 0, 0, 0]);
        assert_eq!(unsafe { load(&ops(&mut e)) }, (ERROR_OK, Vec::new()));
    }

    #[test]
    fn a_short_block_keeps_nothing() {
        let mut m = Mem { data: vec![9, 0, 0, 0, 1, 2, 3], at: 0 };
        assert_eq!(unsafe { load(&ops(&mut m)) }, (ERROR_BEYOND_END, Vec::new()));
        let mut none = Mem { data: vec![1, 0], at: 0 };
        assert_eq!(unsafe { load(&ops(&mut none)) }.0, ERROR_BEYOND_END);
    }
}
