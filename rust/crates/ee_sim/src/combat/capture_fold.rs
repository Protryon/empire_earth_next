//! TEST ONLY: the combat records folded in batches (CBTC, CBQR, CBSO, CBXR, CBXA, CBGO; was C++, EECombatGoals.cpp's
//! `EECombatFolder` and the condition record's fold; the units' behaviour lane, round 2). Each call's bytes go into
//! an FNV-1a 64; every 4096 calls (and at [`ee_combat_capture_fold_flush`]) one record holds the count and the
//! digest's two halves. With the folder's `EE_SIM_CAPTURE_*_FULL` set, each call is a record of its own bytes.
//! Nothing is folded while no capture runs or while the capture's filters drop the tag.

use std::sync::Mutex;

/// The folders, by `ee_combat_capture_fold`'s `which`.
pub const EE_COMBAT_FOLD_CONDITION: u32 = 0;
pub const EE_COMBAT_FOLD_HOST_READS: u32 = 1;
pub const EE_COMBAT_FOLD_SHOTS: u32 = 2;
pub const EE_COMBAT_FOLD_X_READS: u32 = 3;
pub const EE_COMBAT_FOLD_X_ACTIONS: u32 = 4;
pub const EE_COMBAT_FOLD_GOAL_OBJECTS: u32 = 5;

const FNV_START: u64 = 0xcbf2_9ce4_8422_2325;
const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;
const BATCH: u32 = 4096;

const fn tag(t: &[u8; 4]) -> u32 {
    t[0] as u32 | (t[1] as u32) << 8 | (t[2] as u32) << 16 | (t[3] as u32) << 24
}

/// One folder's running fold.
#[derive(Debug)]
pub struct Fold {
    pub count: u32,
    pub hash: u64,
}

impl Fold {
    pub const fn new() -> Self {
        Fold { count: 0, hash: FNV_START }
    }
    /// One call's bytes; the batch's record payload when it fills.
    pub fn add(&mut self, data: &[u8]) -> Option<[u8; 12]> {
        for &b in data {
            self.hash = (self.hash ^ u64::from(b)).wrapping_mul(FNV_PRIME);
        }
        self.count += 1;
        (self.count == BATCH).then(|| self.take())
    }
    /// The record payload (the count, the digest's low and high words); the fold restarts.
    pub fn take(&mut self) -> [u8; 12] {
        let mut r = [0u8; 12];
        r[0..4].copy_from_slice(&self.count.to_le_bytes());
        r[4..8].copy_from_slice(&(self.hash as u32).to_le_bytes());
        r[8..12].copy_from_slice(&((self.hash >> 32) as u32).to_le_bytes());
        *self = Fold::new();
        r
    }
}

impl Default for Fold {
    fn default() -> Self {
        Fold::new()
    }
}

#[cfg_attr(test, allow(dead_code))] // the test binary folds without a capture
struct Folder {
    tag: u32,
    full_name: &'static str,
    state: Mutex<(Option<bool>, Fold)>,
}

impl Folder {
    const fn new(tag: u32, full_name: &'static str) -> Self {
        Folder { tag, full_name, state: Mutex::new((None, Fold::new())) }
    }
}

static FOLDERS: [Folder; 6] = [
    Folder::new(tag(b"CBTC"), "EE_SIM_CAPTURE_CBTC_FULL"),
    Folder::new(tag(b"CBQR"), "EE_SIM_CAPTURE_CBQR_FULL"),
    Folder::new(tag(b"CBSO"), "EE_SIM_CAPTURE_CBSO_FULL"),
    Folder::new(tag(b"CBXR"), "EE_SIM_CAPTURE_CBXR_FULL"),
    Folder::new(tag(b"CBXA"), "EE_SIM_CAPTURE_CBXA_FULL"),
    Folder::new(tag(b"CBGO"), "EE_SIM_CAPTURE_CBGO_FULL"),
];

#[cfg(not(test))]
pub use game::{ee_combat_capture_condition, fold};

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::app::capture::{commit, ee_app_capture_active, ee_app_capture_keeps};

    /// One call of the folder `which` (its record's bytes).
    pub fn fold(which: u32, data: &[u8]) {
        let Some(folder) = FOLDERS.get(which as usize) else { return };
        if !ee_app_capture_active() {
            return;
        }
        let mut s = folder.state.lock().unwrap_or_else(|e| e.into_inner());
        let full = *s.0.get_or_insert_with(|| std::env::var_os(folder.full_name).is_some());
        if full {
            drop(s);
            commit(folder.tag, data);
            return;
        }
        if !ee_app_capture_keeps(folder.tag) {
            return;
        }
        if let Some(record) = s.1.add(data) {
            commit(folder.tag, &record);
        }
    }

    /// TEST ONLY: one call of the folder `which` (`size` bytes at `data`).
    ///
    /// # Safety
    /// `data` points to `size` bytes.
    #[no_mangle]
    pub unsafe extern "C" fn ee_combat_capture_fold(which: u32, data: *const u8, size: usize) {
        if data.is_null() {
            return;
        }
        // SAFETY: the caller's contract
        fold(which, unsafe { std::slice::from_raw_parts(data, size) });
    }

    /// TEST ONLY: the condition record's call (`EECombatCaptureCondition`: the record's inputs, then its outputs).
    #[no_mangle]
    pub extern "C" fn ee_combat_capture_condition(
        input: &crate::combat::goals::EeCombatConditionIn,
        output: &crate::combat::goals::EeCombatConditionOut,
    ) {
        let mut data = Vec::with_capacity(std::mem::size_of_val(input) + std::mem::size_of_val(output));
        // SAFETY: plain repr(C) records read as their bytes (the C++ folded them so)
        unsafe {
            data.extend_from_slice(std::slice::from_raw_parts(
                (input as *const crate::combat::goals::EeCombatConditionIn).cast::<u8>(),
                std::mem::size_of_val(input),
            ));
            data.extend_from_slice(std::slice::from_raw_parts(
                (output as *const crate::combat::goals::EeCombatConditionOut).cast::<u8>(),
                std::mem::size_of_val(output),
            ));
        }
        fold(EE_COMBAT_FOLD_CONDITION, &data);
    }

    /// TEST ONLY: every folder's partial batch as a record (`EECombatCaptureFlush`), in the folders' order.
    #[no_mangle]
    pub extern "C" fn ee_combat_capture_fold_flush() {
        if !ee_app_capture_active() {
            return;
        }
        for folder in &FOLDERS {
            let mut s = folder.state.lock().unwrap_or_else(|e| e.into_inner());
            if s.1.count != 0 {
                let record = s.1.take();
                commit(folder.tag, &record);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_batch_folds_its_calls_bytes() {
        let mut f = Fold::new();
        assert!(f.add(&[1, 2, 3]).is_none());
        let mut h = FNV_START;
        for b in [1u8, 2, 3] {
            h = (h ^ u64::from(b)).wrapping_mul(FNV_PRIME);
        }
        let r = f.take();
        assert_eq!(&r[0..4], &1u32.to_le_bytes());
        assert_eq!(&r[4..8], &(h as u32).to_le_bytes());
        assert_eq!(&r[8..12], &((h >> 32) as u32).to_le_bytes());
        assert_eq!((f.count, f.hash), (0, FNV_START));
    }

    #[test]
    fn the_4096th_call_writes_the_batch() {
        let mut f = Fold::new();
        for _ in 0..BATCH - 1 {
            assert!(f.add(&[0]).is_none());
        }
        let r = f.add(&[0]).unwrap();
        assert_eq!(&r[0..4], &BATCH.to_le_bytes());
        assert_eq!(f.count, 0);
    }

    #[test]
    fn the_tags() {
        assert_eq!(FOLDERS[0].tag, u32::from_le_bytes(*b"CBTC"));
        assert_eq!(FOLDERS[EE_COMBAT_FOLD_X_ACTIONS as usize].tag, u32::from_le_bytes(*b"CBXA"));
    }
}
