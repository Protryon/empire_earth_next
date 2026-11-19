//! TEST ONLY: the step leaves' and the unit step's state records (USTM, USST; `ee/EEUnitStore.h`), moved here from
//! EEUnitStore.cpp in the game's leaves round 1. A note adds one call of a leaf (its kind, the unit's id, its 32-bit
//! words) to the tick's digest (USTM's for the step leaves, USST's for the state's); `ee_units_step_commit` writes
//! both records at the end of `EEPhysics::UpdateUnits` (the tick, the calls, the FNV-1a 64's low and high words).
//! A class Update's leaf calls (USST kind 12) and a class ActionChanged's operations (kind 13, the outermost call's)
//! gather as one note each. Nothing is noted unless the capture runs.

use crate::app::capture_record::{self as sim_capture, FNV_BASIS, FNV_PRIME};
use std::sync::Mutex;

/// USST kind 12: a class Update's calls (`kUnitStateUpdate`).
const STATE_UPDATE: u32 = 12;
/// USST kind 13: a class ActionChanged's operations (`kUnitStateActionChanged`).
const STATE_ACTION_CHANGED: u32 = 13;

/// One digest: the calls and their FNV-1a 64 over little-endian words.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Digest {
    hash: u64,
    calls: u32,
}

impl Digest {
    const fn new() -> Digest {
        Digest { hash: FNV_BASIS, calls: 0 }
    }
    fn word(&mut self, w: u32) {
        for b in w.to_le_bytes() {
            self.hash = (self.hash ^ u64::from(b)).wrapping_mul(FNV_PRIME);
        }
    }
    fn note(&mut self, kind: u32, id: u32, words: &[u32]) {
        self.word(kind);
        self.word(id);
        for &w in words {
            self.word(w);
        }
        self.calls = self.calls.wrapping_add(1);
    }
}

struct Notes {
    step: Digest,
    state: Digest,
    update: Vec<u32>,
    action: Vec<u32>,
    action_depth: u32,
    action_id: u32,
}

static NOTES: Mutex<Notes> = Mutex::new(Notes {
    step: Digest::new(),
    state: Digest::new(),
    update: Vec::new(),
    action: Vec::new(),
    action_depth: 0,
    action_id: 0,
});

fn with<R>(f: impl FnOnce(&mut Notes) -> R) -> R {
    f(&mut NOTES.lock().unwrap_or_else(|e| e.into_inner()))
}

/// # Safety
/// `words` readable for `count` (or null with 0).
unsafe fn slice<'a>(words: *const u32, count: u32) -> &'a [u32] {
    if words.is_null() || count == 0 {
        &[]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(words, count as usize) }
    }
}

/// `EEUnitStepNote`: one step leaf's call into USTM's digest.
///
/// # Safety
/// `words` readable for `count` words.
#[no_mangle]
pub unsafe extern "C" fn ee_units_step_note(kind: u32, id: u32, words: *const u32, count: u32) {
    if !sim_capture::active() {
        return;
    }
    // SAFETY: the caller's contract
    let w = unsafe { slice(words, count) };
    with(|n| n.step.note(kind, id, w));
}

/// `EEUnitStateNote`: one call into USST's digest.
///
/// # Safety
/// `words` readable for `count` words.
#[no_mangle]
pub unsafe extern "C" fn ee_units_state_note(kind: u32, id: u32, words: *const u32, count: u32) {
    if !sim_capture::active() {
        return;
    }
    // SAFETY: the caller's contract
    let w = unsafe { slice(words, count) };
    with(|n| n.state.note(kind, id, w));
}

/// `EEUnitUpdateNote`: one leaf call of a class Update (three words: the operation, its two arguments' low words).
#[no_mangle]
pub extern "C" fn ee_units_update_note(operation: i32, a: i64, b: i64) {
    if !sim_capture::active() {
        return;
    }
    with(|n| n.update.extend_from_slice(&[operation as u32, a as u32, b as u32]));
}

/// `EEUnitUpdateCommit`: the unit's Update calls as one USST note (kind 12).
#[no_mangle]
pub extern "C" fn ee_units_update_commit(id: u32) {
    with(|n| {
        if n.update.is_empty() {
            return;
        }
        let words = std::mem::take(&mut n.update);
        if sim_capture::active() {
            n.state.note(STATE_UPDATE, id, &words);
        }
    });
}

/// `EEUnitActionNote`: one operation of a class ActionChanged.
#[no_mangle]
pub extern "C" fn ee_units_action_note(operation: i32, a: i64, b: i64) {
    if !sim_capture::active() {
        return;
    }
    with(|n| n.action.extend_from_slice(&[operation as u32, a as u32, b as u32]));
}

/// `EEUnitActionScope`'s constructor: the outermost call names the unit.
#[no_mangle]
pub extern "C" fn ee_units_action_enter(id: u32) {
    with(|n| {
        if n.action_depth == 0 {
            n.action_id = id;
        }
        n.action_depth = n.action_depth.wrapping_add(1);
    });
}

/// `EEUnitActionScope`'s destructor: the outermost call writes the operations as one USST note (kind 13).
#[no_mangle]
pub extern "C" fn ee_units_action_leave() {
    with(|n| {
        n.action_depth = n.action_depth.wrapping_sub(1);
        if n.action_depth != 0 || n.action.is_empty() {
            return;
        }
        let words = std::mem::take(&mut n.action);
        if sim_capture::active() {
            let id = n.action_id;
            n.state.note(STATE_ACTION_CHANGED, id, &words);
        }
    });
}

/// The two records of a tick (USTM then USST), and the digests started again.
fn commit_records(ticks: u32) {
    let (step, state) = with(|n| {
        let d = (n.step, n.state);
        n.step = Digest::new();
        n.state = Digest::new();
        d
    });
    for (tag, d) in [(0x4d54_5355u32, step), (0x5453_5355u32, state)] {
        let mut r = sim_capture::Record::new(tag);
        for v in [ticks, d.calls, d.hash as u32, (d.hash >> 32) as u32] {
            r.u32(v);
        }
        r.commit();
    }
}

/// `EEUnitStepCommit` (the end of `EEPhysics::UpdateUnits`, `ticks` the physics' ticks done): the leaves' store
/// reads that differed from the members stop EE-SIM (exit 3, a USCK mismatch); then the USTM and USST records.
#[no_mangle]
pub extern "C" fn ee_units_step_commit(ticks: u32) {
    if !sim_capture::active() {
        return;
    }
    let (mut mismatches, mut unrecorded) = (0u32, 0u32);
    // SAFETY: two writable words
    unsafe { super::step::ee_units_step_reads(&mut mismatches, &mut unrecorded) };
    if mismatches != 0 {
        eprintln!(
            "EE-UNIT-STEP: a store read (records differing from the members) of unit 0x00000000: Rust {mismatches}, the original 0"
        );
        sim_capture::flush();
        std::process::exit(3);
    }
    commit_records(ticks);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_note_folds_kind_id_and_words() {
        let mut d = Digest::new();
        d.note(1, 0x1234, &[5, 6]);
        let mut h = FNV_BASIS;
        for w in [1u32, 0x1234, 5, 6] {
            for b in w.to_le_bytes() {
                h = (h ^ u64::from(b)).wrapping_mul(FNV_PRIME);
            }
        }
        assert_eq!(d, Digest { hash: h, calls: 1 });
    }

    #[test]
    fn the_outermost_action_scope_names_the_unit() {
        ee_units_action_enter(7);
        ee_units_action_enter(9);
        ee_units_action_leave();
        let (depth, id) = with(|n| (n.action_depth, n.action_id));
        assert_eq!((depth, id), (1, 7));
        ee_units_action_leave();
        assert_eq!(with(|n| n.action_depth), 0);
    }

    #[test]
    fn nothing_is_noted_without_the_capture() {
        if std::env::var_os("EE_SIM_CAPTURE").is_none() {
            ee_units_update_note(1, 2, 3);
            // SAFETY: two words
            unsafe { ee_units_step_note(1, 2, [3u32, 4].as_ptr(), 2) };
            assert!(with(|n| n.update.is_empty() && n.step.calls == 0));
        }
    }
}
