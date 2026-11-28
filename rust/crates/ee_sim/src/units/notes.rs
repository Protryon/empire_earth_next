//! TEST ONLY: the unit step's notes (was C++, EEUnitStore.cpp; the units' behaviour lane, round 2): the step
//! leaves' digest (USTM, R5b.4 leaf 6) and the state digest (USST: the AI step, the conditions, the physics models'
//! arithmetic, the update host's and `ActionChanged` host's operations), folded while the simulation capture runs and
//! written once a tick by [`ee_units_note_step_commit`]. Each note is its kind, the unit's id and its words, as
//! 32-bit words into an FNV-1a 64; the update and action operations are gathered per unit and noted as one call
//! when the unit's step (kind 12) or its outermost `ActionChanged` (kind 13) returns.

use std::sync::Mutex;

/// `kUnitStateUpdate`
const NOTE_KIND_UPDATE: u32 = 12;
/// `kUnitStateActionChanged`
const NOTE_KIND_ACTION_CHANGED: u32 = 13;

const FNV_START: u64 = 0xcbf2_9ce4_8422_2325;
const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;

/// The digests and the gathered operations.
#[derive(Debug)]
pub(crate) struct Notes {
    step: u64,
    step_calls: u32,
    state: u64,
    state_calls: u32,
    update: Vec<u32>,
    action: Vec<u32>,
    action_depth: u32,
    action_id: u32,
}

impl Default for Notes {
    fn default() -> Self {
        Notes {
            step: FNV_START,
            step_calls: 0,
            state: FNV_START,
            state_calls: 0,
            update: Vec::new(),
            action: Vec::new(),
            action_depth: 0,
            action_id: 0,
        }
    }
}

fn fold(hash: &mut u64, word: u32) {
    for b in word.to_le_bytes() {
        *hash ^= u64::from(b);
        *hash = hash.wrapping_mul(FNV_PRIME);
    }
}

impl Notes {
    /// `EEUnitStepNote`: one leaf call into the step digest.
    pub fn step(&mut self, kind: u32, id: u32, words: &[u32]) {
        fold(&mut self.step, kind);
        fold(&mut self.step, id);
        for &w in words {
            fold(&mut self.step, w);
        }
        self.step_calls += 1;
    }
    /// `EEUnitStateNote`: one call into the state digest.
    pub fn state(&mut self, kind: u32, id: u32, words: &[u32]) {
        fold(&mut self.state, kind);
        fold(&mut self.state, id);
        for &w in words {
            fold(&mut self.state, w);
        }
        self.state_calls += 1;
    }
    /// `EEUnitUpdateNote`: an update host operation of the unit stepping now.
    pub fn update(&mut self, op: i32, a: i64, b: i64) {
        self.update.extend_from_slice(&[op as u32, a as u32, b as u32]);
    }
    /// `EEUnitUpdateCommit`: the stepped unit's operations as one state call (kind 12).
    pub fn update_commit(&mut self, id: u32) {
        if self.update.is_empty() {
            return;
        }
        let words = std::mem::take(&mut self.update);
        self.state(NOTE_KIND_UPDATE, id, &words);
    }
    /// `EEUnitActionNote`: an `ActionChanged` host operation.
    pub fn action(&mut self, op: i32, a: i64, b: i64) {
        self.action.extend_from_slice(&[op as u32, a as u32, b as u32]);
    }
    /// `EEUnitActionScope`'s constructor: the outermost call names the unit.
    pub fn action_begin(&mut self, id: u32) {
        if self.action_depth == 0 {
            self.action_id = id;
        }
        self.action_depth += 1;
    }
    /// `EEUnitActionScope`'s destructor: the outermost call's operations as one state call (kind 13).
    pub fn action_end(&mut self) {
        self.action_depth = self.action_depth.saturating_sub(1);
        if self.action_depth != 0 || self.action.is_empty() {
            return;
        }
        let words = std::mem::take(&mut self.action);
        let id = self.action_id;
        self.state(NOTE_KIND_ACTION_CHANGED, id, &words);
    }
    /// The tick's USTM and USST payloads (the tick, the calls, the digest's two halves); both digests restart.
    pub fn commit(&mut self, ticks: u32) -> ([u8; 16], [u8; 16]) {
        let rec = |calls: u32, digest: u64| {
            let mut r = [0u8; 16];
            r[0..4].copy_from_slice(&ticks.to_le_bytes());
            r[4..8].copy_from_slice(&calls.to_le_bytes());
            r[8..12].copy_from_slice(&(digest as u32).to_le_bytes());
            r[12..16].copy_from_slice(&((digest >> 32) as u32).to_le_bytes());
            r
        };
        let out = (rec(self.step_calls, self.step), rec(self.state_calls, self.state));
        self.step = FNV_START;
        self.step_calls = 0;
        self.state = FNV_START;
        self.state_calls = 0;
        out
    }
}

static NOTES: Mutex<Option<Notes>> = Mutex::new(None);

fn with<R>(f: impl FnOnce(&mut Notes) -> R) -> R {
    let mut g = NOTES.lock().unwrap_or_else(|e| e.into_inner());
    f(g.get_or_insert_with(Notes::default))
}

/// Whether the notes are taken (the simulation capture runs: `EEUnitStepNoting`).
pub fn noting() -> bool {
    crate::app::capture::ee_app_capture_active()
}

/// A step leaf's note (nothing while no capture runs).
pub fn step(kind: u32, id: u32, words: &[u32]) {
    if noting() {
        with(|n| n.step(kind, id, words));
    }
}

/// A state note (nothing while no capture runs).
pub fn state(kind: u32, id: u32, words: &[u32]) {
    if noting() {
        with(|n| n.state(kind, id, words));
    }
}

/// An update host operation's note.
pub fn update(op: i32, a: i64, b: i64) {
    if noting() {
        with(|n| n.update(op, a, b));
    }
}

/// An `ActionChanged` host operation's note.
pub fn action(op: i32, a: i64, b: i64) {
    if noting() {
        with(|n| n.action(op, a, b));
    }
}

/// The update or action note of a host operation (`code` at or past `first_action`: an action's; the C++ host's
/// split).
pub fn host_op(code: i32, first_action: i32, a: i64, b: i64) {
    if code >= first_action {
        action(code, a, b);
    } else {
        update(code, a, b);
    }
}

/// `EEUnitStepNote(kind, id, words, count)`
///
/// # Safety
/// `words` points to `count` words (or `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_note_step(kind: u32, id: u32, words: *const u32, count: u32) {
    if !noting() {
        return;
    }
    // SAFETY: the caller's contract
    let w = if count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(words, count as usize) } };
    step(kind, id, w);
}

/// `EEUnitUpdateCommit(id)`: after a unit's step
#[no_mangle]
pub extern "C" fn ee_units_note_update_commit(id: u32) {
    with(|n| n.update_commit(id));
}

/// `EEUnitActionScope(id)`: an `ActionChanged` begins
#[no_mangle]
pub extern "C" fn ee_units_note_action_begin(id: u32) {
    with(|n| n.action_begin(id));
}

/// `~EEUnitActionScope`: the `ActionChanged` returns
#[no_mangle]
pub extern "C" fn ee_units_note_action_end() {
    with(|n| n.action_end());
}

/// `EEUnitStepCommit()`: the tick's USTM and USST records (`ticks`: the physics' ticks done). A leaf's store read
/// that differed from the members stops EE-SIM (exit 3).
#[no_mangle]
pub extern "C" fn ee_units_note_step_commit(ticks: u32) {
    if !noting() {
        return;
    }
    let mismatches = crate::units::step::with_reads(|r| r.mismatches);
    if mismatches != 0 {
        eprintln!(
            "EE-UNIT-STEP: a store read (records differing from the members) of unit 0x00000000: Rust {mismatches}, \
             the original 0"
        );
        crate::app::capture::ee_app_capture_flush();
        std::process::exit(3);
    }
    let (ustm, usst) = with(|n| n.commit(ticks));
    crate::app::capture::commit(0x4d54_5355, &ustm); // "USTM"
    crate::app::capture::commit(0x5453_5355, &usst); // "USST"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_note_folds_its_words_as_the_cpp_did() {
        let mut n = Notes::default();
        n.step(3, 0x0100_0002, &[7]);
        let mut h = FNV_START;
        for w in [3u32, 0x0100_0002, 7] {
            for b in w.to_le_bytes() {
                h = (h ^ u64::from(b)).wrapping_mul(FNV_PRIME);
            }
        }
        let (ustm, usst) = n.commit(9);
        assert_eq!(&ustm[0..8], &[9, 0, 0, 0, 1, 0, 0, 0]);
        assert_eq!(u32::from_le_bytes(ustm[8..12].try_into().unwrap()), h as u32);
        assert_eq!(u32::from_le_bytes(ustm[12..16].try_into().unwrap()), (h >> 32) as u32);
        // the state digest had no call: its start value
        assert_eq!(u32::from_le_bytes(usst[4..8].try_into().unwrap()), 0);
        assert_eq!(u32::from_le_bytes(usst[8..12].try_into().unwrap()), FNV_START as u32);
        // and both restart
        let (again, _) = n.commit(10);
        assert_eq!(u32::from_le_bytes(again[8..12].try_into().unwrap()), FNV_START as u32);
    }

    #[test]
    fn the_outermost_action_scope_notes_once() {
        let mut n = Notes::default();
        n.action_begin(5);
        n.action(1, 2, 3);
        n.action_begin(6);
        n.action(4, -1, 0);
        n.action_end();
        assert_eq!(n.state_calls, 0);
        n.action_end();
        assert_eq!(n.state_calls, 1);
        let mut m = Notes::default();
        m.state(NOTE_KIND_ACTION_CHANGED, 5, &[1, 2, 3, 4, u32::MAX, 0]);
        assert_eq!(n.state, m.state);
        // an empty scope notes nothing
        n.action_begin(7);
        n.action_end();
        assert_eq!(n.state_calls, 1);
    }

    #[test]
    fn a_step_commits_its_updates() {
        let mut n = Notes::default();
        n.update_commit(1);
        assert_eq!(n.state_calls, 0);
        n.update(12, 5, 6);
        n.update_commit(2);
        let mut m = Notes::default();
        m.state(NOTE_KIND_UPDATE, 2, &[12, 5, 6]);
        assert_eq!((n.state, n.state_calls), (m.state, 1));
    }
}
