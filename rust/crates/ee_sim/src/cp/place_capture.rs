//! TEST ONLY: PLCE and CPEV, the computer player's placement, resource centres' and own calls' capture records
//! (ee/EECPPlaceCapture.h, tools/sim_capture.py; whole in Rust since the game's leaves round 2, was
//! EECPPlaceCapture.cpp).
//!
//! Every entry point of EECPPlacement, EECPResourceCenter and their free functions opens a PLCE scope; every
//! EEComputerPlayer event a CPEV scope. Each kind keeps its own thread-local depth: only the outermost scope of a
//! thread records. When it closes, one record holds the method, the player id, the words the call added (inputs,
//! outputs, the placement's state, the centres' digest) and, for CPEV, the computer player's state. CPEV records only
//! on the thread that runs the computer players' Update (noted at its first call); the interface thread asks some of
//! them too, at its own times. Off (no capture file), a scope is the depth's count and no allocation.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use std::cell::Cell;
use std::mem::offset_of;
use std::sync::atomic::{AtomicU32, Ordering};

use ee_base::plat_sys::thread::imp::PlatCurrentThreadId;

use super::fields::EeCpFields;
use super::lists::{centers_digest, lists, lists_digest};
use super::placement::{placement_digest, EeCpPlacement};
use crate::app::capture_record::{self, FNV_BASIS};
use crate::world::layout::layout;
use crate::world::view::{PlayerView, UnitView, WorldRead};

thread_local! {
    static PLACE_DEPTH: Cell<i32> = const { Cell::new(0) };
    static EVENT_DEPTH: Cell<i32> = const { Cell::new(0) };
}

/// The computer players' Update thread (0: not noted yet).
static EVENT_THREAD: AtomicU32 = AtomicU32::new(0);

/// `EECaptureTag`: the four characters as a little-endian word.
const fn tag(name: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*name)
}

/// An open scope that records (one exists only while the capture is on and the scope is its thread's outermost).
pub struct EeCpPlaceScope {
    player: u64,
    events: bool,
    method: u32,
    player_id: u32,
    words: Vec<u32>,
}

fn depth(events: bool) -> &'static std::thread::LocalKey<Cell<i32>> {
    if events {
        &EVENT_DEPTH
    } else {
        &PLACE_DEPTH
    }
}

/// Whether a scope opened now records: it is the outermost of its kind, the capture is on and (CPEV) this is the
/// noted thread. Counts the depth either way.
fn enter(events: bool, active: bool, thread: u32) -> bool {
    let outer = depth(events).with(|d| {
        let v = d.get();
        d.set(v + 1);
        v == 0
    });
    let noted = EVENT_THREAD.load(Ordering::Relaxed);
    outer && active && (!events || (noted != 0 && thread == noted))
}

fn leave(events: bool) {
    depth(events).with(|d| d.set(d.get() - 1));
}

/// The record a closed scope commits (its words already appended).
fn record(scope: &EeCpPlaceScope) -> capture_record::Record {
    let mut r = capture_record::Record::new(if scope.events { tag(b"CPEV") } else { tag(b"PLCE") });
    r.u32(scope.method);
    r.u32(scope.player_id);
    r.u32(scope.words.len() as u32);
    for w in &scope.words {
        r.u32(*w);
    }
    r
}

/// FNV-1a 64 of the computer player's record up to its last member (the tail padding after the closing bools is
/// whatever the allocation left).
fn fields_hash(fields: &EeCpFields) -> u64 {
    let len = offset_of!(EeCpFields, mAttackStarted) + std::mem::size_of::<bool>();
    // SAFETY: the record's own bytes, up to and including its last member
    let bytes = unsafe { std::slice::from_raw_parts(fields as *const EeCpFields as *const u8, len) };
    capture_record::fnv(FNV_BASIS, bytes)
}

/// The placement's town centre tile (a `U2DPoint<long>`) and its Rust data, through the layout.
///
/// # Safety
/// `placement` is a live `EECPPlacement`.
unsafe fn placement_parts(placement: u64) -> ([i32; 2], *const EeCpPlacement) {
    let l = layout();
    let at = placement as usize;
    // SAFETY: the caller's contract; the layout's offsets are the build's
    unsafe {
        let tile = (at + l.placement_town as usize) as *const std::ffi::c_long;
        let rust = std::ptr::read((at + l.placement_rust as usize) as *const *const EeCpPlacement);
        ([*tile as i32, *tile.add(1) as i32], rust)
    }
}

/// The player's `mPlacement` (0: none).
///
/// # Safety
/// `player` is a live `EEComputerPlayer`'s `EEPlayer*`.
unsafe fn player_placement(player: u64) -> u64 {
    // SAFETY: the caller's contract
    unsafe { std::ptr::read((player as usize + layout().cp_placement as usize) as *const usize) as u64 }
}

impl EeCpPlaceScope {
    fn word(&mut self, w: u32) {
        self.words.push(w);
    }
    /// The placement's state: the town centre tile, then its data's digest (placement_digest).
    ///
    /// # Safety
    /// `placement` is 0 or a live `EECPPlacement`.
    unsafe fn state(&mut self, placement: u64) {
        if placement == 0 {
            return;
        }
        // SAFETY: the caller's contract
        let (town, rust) = unsafe { placement_parts(placement) };
        self.word(town[0] as u32);
        self.word(town[1] as u32);
        // SAFETY: a live placement's data (EECPPlacement's constructor makes it)
        for w in placement_digest(unsafe { &*rust }) {
            self.word(w);
        }
    }
    /// The computer player's state (CPEV, at the close): the record's hash, the lists' digest, the town centre tile.
    ///
    /// # Safety
    /// `self.player` is 0 or a live `EEComputerPlayer`'s `EEPlayer*`.
    unsafe fn player_state(&mut self) {
        if self.player == 0 {
            return;
        }
        let t = unsafe { WorldRead::begin() };
        // SAFETY: the caller's contract: a computer player
        let Some(p) = (unsafe { PlayerView::of(&t, self.player) }) else { return };
        // SAFETY: as above
        let fields = unsafe { &*p.computer_fields().ptr() };
        let h = fields_hash(fields);
        self.word(h as u32);
        self.word((h >> 32) as u32);
        // SAFETY: a live computer player's lists
        for w in lists_digest(unsafe { lists(fields) }) {
            self.word(w);
        }
        // SAFETY: as above
        let placement = unsafe { player_placement(self.player) };
        if placement != 0 {
            // SAFETY: the player's live placement
            let (town, _) = unsafe { placement_parts(placement) };
            self.word(town[0] as u32);
            self.word(town[1] as u32);
        }
    }
}

/// Opens a scope of `method` for the computer player `player` (its `EEPlayer*`; 0: none), CPEV when `events`.
/// Null when it does not record; close it with [`ee_cp_scope_end`] and the same `events` either way.
///
/// # Safety
/// `player` is 0 or a live `EEComputerPlayer`'s `EEPlayer*`.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_scope_begin(player: u64, method: u32, events: bool) -> *mut EeCpPlaceScope {
    let active = capture_record::active();
    // SAFETY: the platform's thread id
    let thread = if events && active { unsafe { PlatCurrentThreadId() } } else { 0 };
    if !enter(events, active, thread) {
        return std::ptr::null_mut();
    }
    let player_id = if player == 0 {
        0xffff_ffff
    } else {
        let t = unsafe { WorldRead::begin() };
        // SAFETY: the caller's contract
        unsafe { PlayerView::of(&t, player) }.map_or(0xffff_ffff, |p| p.id())
    };
    Box::into_raw(Box::new(EeCpPlaceScope { player, events, method, player_id, words: Vec::new() }))
}

/// Closes a scope: a recording one appends the player's state (CPEV) and commits its record.
///
/// # Safety
/// `scope` is null or [`ee_cp_scope_begin`]'s, not closed yet; `events` is the one it was opened with.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_scope_end(scope: *mut EeCpPlaceScope, events: bool) {
    leave(events);
    if scope.is_null() {
        return;
    }
    // SAFETY: the caller's contract
    let mut s = unsafe { Box::from_raw(scope) };
    if s.events {
        // SAFETY: the scope's player is live for the call it wraps
        unsafe { s.player_state() };
    }
    record(&s).commit();
}

/// A word (an input, an output, a flag).
///
/// # Safety
/// `scope` is null or an open scope.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_scope_word(scope: *mut EeCpPlaceScope, word: u32) {
    // SAFETY: the caller's contract
    if let Some(s) = unsafe { scope.as_mut() } {
        s.word(word);
    }
}

/// A unit's id (`unit` its `RTSUnit*`; 0: none, recorded as 0xfffffffe).
///
/// # Safety
/// `scope` is null or an open scope; `unit` is 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_scope_unit(scope: *mut EeCpPlaceScope, unit: u64) {
    // SAFETY: the caller's contract
    let Some(s) = (unsafe { scope.as_mut() }) else { return };
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let id = unsafe { UnitView::of(&t, unit) }.map_or(0xffff_fffe, |u| u.id());
    s.word(id);
}

/// The placement's state (`placement` an `EECPPlacement*`; 0: nothing).
///
/// # Safety
/// `scope` is null or an open scope; `placement` is 0 or a live placement.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_scope_state(scope: *mut EeCpPlaceScope, placement: u64) {
    // SAFETY: the caller's contract
    if let Some(s) = unsafe { scope.as_mut() } {
        // SAFETY: as above
        unsafe { s.state(placement) };
    }
}

/// Every resource centre of the computer player `player` (its `EEPlayer*`): their count and FNV-1a 64.
///
/// # Safety
/// `scope` is null or an open scope; `player` is 0 or a live computer player.
#[no_mangle]
pub unsafe extern "C" fn ee_cp_scope_centers(scope: *mut EeCpPlaceScope, player: u64) {
    // SAFETY: the caller's contract
    let Some(s) = (unsafe { scope.as_mut() }) else { return };
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let Some(p) = (unsafe { PlayerView::of(&t, player) }) else { return };
    // SAFETY: a computer player's record and its lists
    for w in centers_digest(unsafe { lists(p.computer_fields().ptr()) }) {
        s.word(w);
    }
}

/// Notes the thread that runs the computer players' Update (CPEV records only there): its first call while the
/// capture is on.
#[no_mangle]
pub extern "C" fn ee_cp_capture_thread() {
    if EVENT_THREAD.load(Ordering::Relaxed) == 0 && capture_record::active() {
        // SAFETY: the platform's thread id
        EVENT_THREAD.store(unsafe { PlatCurrentThreadId() }, Ordering::Relaxed);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tags_are_little_endian_words() {
        assert_eq!(tag(b"PLCE"), 0x4543_4c50);
        assert_eq!(tag(b"CPEV"), 0x5645_5043);
    }

    #[test]
    fn only_the_outermost_scope_of_a_kind_records() {
        // the capture off: never records, the depths still count
        assert!(!enter(false, false, 0));
        assert!(!enter(false, true, 0)); // nested
        leave(false);
        leave(false);
        assert!(enter(false, true, 0));
        assert!(!enter(false, true, 0));
        // the other kind keeps its own depth; CPEV needs the noted thread (none here)
        assert!(!enter(true, true, 7));
        leave(true);
        leave(false);
        leave(false);
        PLACE_DEPTH.with(|d| assert_eq!(d.get(), 0));
        EVENT_DEPTH.with(|d| assert_eq!(d.get(), 0));
    }

    #[test]
    fn a_record_holds_method_player_and_words() {
        let s = EeCpPlaceScope { player: 0, events: false, method: 7, player_id: 3, words: vec![1, 0xffff_fffe] };
        let r = record(&s);
        assert_eq!(r.tag, tag(b"PLCE"));
        let words: Vec<u32> = r.data.chunks(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect();
        assert_eq!(words, vec![7, 3, 2, 1, 0xffff_fffe]);
    }

    #[test]
    fn the_fields_hash_covers_the_record_to_its_last_member() {
        let mut f = EeCpFields::default();
        let a = fields_hash(&f);
        f.mAttackStarted = true;
        assert_ne!(fields_hash(&f), a);
    }
}
