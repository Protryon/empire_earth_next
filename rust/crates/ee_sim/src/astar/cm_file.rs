//! The continent manager's regions block in a saved game's file (AI Thread cluster round 8; was EEAIContinentSave.cpp's
//! EESaveContinentRegions, EELoadContinentRegions, EEContinentRegionsPending and the block it kept for
//! EERestoreContinentRegions): a 32-bit size, then the bytes of `cm_io::write_regions`. A load keeps the block until
//! the regions are put back at the game's start (`cm_restore.rs`); a failed read keeps none.

use std::ffi::c_void;
use std::sync::Mutex;

use ee_base::fs::{CppFile, FsFile, FsWrite};

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
pub fn save(file: &mut impl FsWrite, bytes: &[u8]) -> i32 {
    let mut result = file.write_u32(bytes.len() as u32);
    if result == ERROR_OK && !bytes.is_empty() {
        result = file.write_bytes(bytes).0;
    }
    result
}

/// Reads a block (its size, then its bytes; a short read is `kErrorBeyondEnd`) -> the result and the bytes (none
/// after a failure).
pub fn load(file: &mut impl FsFile) -> (i32, Vec<u8>) {
    let mut size = 0u32;
    let mut result = file.read_u32(&mut size);
    let mut bytes = Vec::new();
    if result == ERROR_OK && size > 0 {
        bytes.resize(size as usize, 0);
        let (status, read) = file.read_bytes(&mut bytes);
        result = status;
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
/// `r` is the manager's live record; `file` is the save's C++ `FSFile*`, open for writing.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_save_regions(r: *const EeAStarCmRecord, file: *mut c_void) -> i32 {
    // SAFETY: the caller's contract
    save(&mut CppFile(file), &crate::astar::cm_io::write_regions(unsafe { &*r }))
}

/// `EELoadContinentRegions`: the save's block, kept for the start of the game.
///
/// # Safety
/// `file` is the save's C++ `FSFile*`, open for reading.
#[no_mangle]
pub unsafe extern "C" fn ee_astar_cm_load_regions(file: *mut c_void) -> i32 {
    loaded().clear();
    let (result, bytes) = load(&mut CppFile(file));
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

    struct Mem {
        data: Vec<u8>,
        at: usize,
    }

    impl FsFile for Mem {
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            if self.at + 4 > self.data.len() {
                return ERROR_BEYOND_END;
            }
            *v = u32::from_le_bytes(self.data[self.at..self.at + 4].try_into().unwrap());
            self.at += 4;
            ERROR_OK
        }
        fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32) {
            let k = out.len().min(self.data.len() - self.at);
            out[..k].copy_from_slice(&self.data[self.at..self.at + k]);
            self.at += k;
            (ERROR_OK, k as u32)
        }
    }

    impl FsWrite for Mem {
        fn write_u32(&mut self, v: u32) -> i32 {
            self.data.extend_from_slice(&v.to_le_bytes());
            ERROR_OK
        }
        fn write_bytes(&mut self, d: &[u8]) -> (i32, u32) {
            self.data.extend_from_slice(d);
            (ERROR_OK, d.len() as u32)
        }
    }

    #[test]
    fn a_block_reads_back_as_written() {
        let mut m = Mem { data: Vec::new(), at: 0 };
        assert_eq!(save(&mut m, b"regions"), ERROR_OK);
        assert_eq!(m.data, [7, 0, 0, 0, b'r', b'e', b'g', b'i', b'o', b'n', b's']);
        assert_eq!(load(&mut m), (ERROR_OK, b"regions".to_vec()));
        // an empty block is its size alone
        let mut e = Mem { data: Vec::new(), at: 0 };
        save(&mut e, b"");
        assert_eq!(e.data, [0, 0, 0, 0]);
        assert_eq!(load(&mut e), (ERROR_OK, Vec::new()));
    }

    #[test]
    fn a_short_block_keeps_nothing() {
        let mut m = Mem { data: vec![9, 0, 0, 0, 1, 2, 3], at: 0 };
        assert_eq!(load(&mut m), (ERROR_BEYOND_END, Vec::new()));
        let mut none = Mem { data: vec![1, 0], at: 0 };
        assert_eq!(load(&mut none).0, ERROR_BEYOND_END);
    }
}
