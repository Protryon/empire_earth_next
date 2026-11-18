//! TEST ONLY: the unit utilities' record (UUTL, the unit utilities lane, round 1; in Rust from round 3). The
//! outermost call of a noted utility on its thread folds its kind, inputs and outputs (units as ids, floats as bits,
//! tile lists whole) into the physics tick's FNV-1a 64; the record (the tick, the calls, the digest) is written when a
//! later tick's first call comes. Nothing is noted unless the capture runs. Its golden came from the C++ utilities
//! (tools/sim_capture.py), whose notes were these words in this order.
//!
//! A note is opened by each noted entry point (uu_ffi.rs) and by the C++ bodies that are still C++
//! (`ee_units_uu_note_begin`/`_word`/`_end`); every open note counts in the thread's depth, so a body's nested
//! utility calls are never noted, whichever side they are on. The writes a body makes to the world (a build target,
//! a builder starting over, a step aside, a deposit) are events of the thread's recording note
//! (`ee_units_uu_note_event`, from the C++ host), at the point they happen.
//!
//! `EEComputeSoundParameters` (kind 82) is left out: its callers ask it only while a sound node is still playing (the
//! node the sound manager's scavenger thread frees when the mixer is done with it), so its call count follows the
//! audio device's real-time progress, not the game (PORT_LEARNINGS "main merge: the gates on the Lua AI"). Its own
//! nested calls stay unnoted, as before.

use std::cell::{Cell, RefCell};
use std::sync::Mutex;

use crate::units::uu_tiles::Tile;

/// `EEComputeSoundParameters`' kind (never recorded).
const KIND_SOUND_PARAMETERS: u32 = 82;
const FNV_OFFSET: u64 = 0xcbf2_9ce4_8422_2325;
const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;

thread_local! {
    /// The thread's open notes.
    static DEPTH: Cell<i32> = const { Cell::new(0) };
    /// The words of the thread's recording (outermost) note; None when none records.
    static WORDS: RefCell<Option<Vec<u32>>> = const { RefCell::new(None) };
}

/// The folded calls of the current physics tick.
struct State {
    digest: u64,
    calls: u32,
    tick: u32,
    kinds: [u32; 128],
}

static STATE: Mutex<State> = Mutex::new(State { digest: FNV_OFFSET, calls: 0, tick: 0, kinds: [0; 128] });

#[cfg(not(test))]
extern "C" {
    /// `EESimCaptureRecord::Active()` (0), the physics' ticks done (1), the UUTL record of (a tick, b calls, c and d
    /// the digest's halves) written (2).
    fn ee_units_uu_host_note(which: i32, a: u32, b: u32, c: u32, d: u32) -> u32;
}

#[cfg(not(test))]
fn host(which: i32, a: u32, b: u32, c: u32, d: u32) -> u32 {
    // SAFETY: plain C++ reads, and a capture record's write
    unsafe { ee_units_uu_host_note(which, a, b, c, d) }
}

#[cfg(test)]
thread_local! {
    /// The unit tests' world: whether the capture runs, the tick, the records written.
    pub(crate) static TEST_HOST: RefCell<(bool, u32, Vec<[u32; 4]>)> = const { RefCell::new((false, 0, Vec::new())) };
}

#[cfg(test)]
fn host(which: i32, a: u32, b: u32, c: u32, d: u32) -> u32 {
    TEST_HOST.with(|h| {
        let mut h = h.borrow_mut();
        match which {
            0 => u32::from(h.0),
            1 => h.1,
            _ => {
                h.2.push([a, b, c, d]);
                0
            }
        }
    })
}

/// An open note (`EEUUNote`): its words go to the thread's record while it is the recording one.
pub(crate) struct Note {
    on: bool,
    kind: u32,
}

impl Note {
    /// Opens a note of `kind` (its first word): it records when it is the thread's outermost and the capture runs.
    pub(crate) fn begin(kind: u32) -> Note {
        let depth = DEPTH.with(|d| {
            d.set(d.get() + 1);
            d.get()
        });
        let on = depth == 1 && kind != KIND_SOUND_PARAMETERS && host(0, 0, 0, 0, 0) != 0;
        if on {
            WORDS.with(|w| *w.borrow_mut() = Some(Vec::new()));
        }
        let note = Note { on, kind };
        note.i(i64::from(kind));
        note
    }
    /// Whether this note records (its inputs are worth reading).
    pub(crate) fn on(&self) -> bool {
        self.on
    }
    /// A word (a C++ `long`'s low 32 bits).
    pub(crate) fn i(&self, value: i64) -> &Self {
        if self.on {
            push(value as u32);
        }
        self
    }
    /// A bool.
    pub(crate) fn b(&self, value: bool) -> &Self {
        self.i(i64::from(value))
    }
    /// A float's bits.
    pub(crate) fn f(&self, value: f32) -> &Self {
        self.i(i64::from(value.to_bits()))
    }
    /// A point's three floats.
    pub(crate) fn p(&self, p: [f32; 3]) -> &Self {
        self.f(p[0]).f(p[1]).f(p[2])
    }
    /// A matrix's sixteen floats, row by row.
    pub(crate) fn m(&self, m: &[[f32; 4]; 4]) -> &Self {
        for row in m {
            for v in row {
                self.f(*v);
            }
        }
        self
    }
    /// A tile list: its length, then its pairs.
    pub(crate) fn t(&self, tiles: &[Tile]) -> &Self {
        self.i(tiles.len() as i64);
        for t in tiles {
            self.i(i64::from(t[0])).i(i64::from(t[1]));
        }
        self
    }
    /// A point list: its length, then its points.
    pub(crate) fn ps(&self, points: &[[f32; 3]]) -> &Self {
        self.i(points.len() as i64);
        for p in points {
            self.p(*p);
        }
        self
    }
    /// A unit by id (-2: none).
    pub(crate) fn u(&self, unit: u64) -> &Self {
        if self.on {
            self.i(unit_id(unit));
        }
        self
    }
    /// A unit list: its length, then its ids.
    pub(crate) fn us(&self, units: &[u64]) -> &Self {
        self.i(units.len() as i64);
        for u in units {
            self.u(*u);
        }
        self
    }
    /// A player by id (-2: none).
    pub(crate) fn pl(&self, player: u64) -> &Self {
        if self.on {
            self.i(player_id(player));
        }
        self
    }
    /// A class by unit type (-2: none).
    pub(crate) fn c(&self, class: u64) -> &Self {
        if self.on {
            self.i(class_type(class));
        }
        self
    }
    /// The units on each tile of the `size` x `size` footprint at (x, y), each tile's list closed by -3.
    pub(crate) fn foot(&self, x: i64, y: i64, size: i64) -> &Self {
        if self.on {
            for tx in x..x + size {
                for ty in y..y + size {
                    for u in tile_units(tx, ty) {
                        self.u(u);
                    }
                    self.i(-3);
                }
            }
        }
        self
    }
}

impl Drop for Note {
    fn drop(&mut self) {
        DEPTH.with(|d| d.set(d.get() - 1));
        if !self.on {
            return;
        }
        let words = WORDS.with(|w| w.borrow_mut().take()).unwrap_or_default();
        fold(self.kind, &words);
    }
}

fn push(word: u32) {
    WORDS.with(|w| {
        if let Some(words) = w.borrow_mut().as_mut() {
            words.push(word);
        }
    });
}

/// An event of the thread's recording note (a body's write to the world), if one records.
pub(crate) fn event(a: i64, b: i64, c: i64) {
    push(a as u32);
    push(b as u32);
    push(c as u32);
}

/// One recorded call folded into the tick's digest (the previous tick's record written first).
fn fold(kind: u32, words: &[u32]) {
    let mut s = STATE.lock().unwrap_or_else(|e| e.into_inner());
    let tick = host(1, 0, 0, 0, 0);
    if tick != s.tick {
        if s.calls != 0 {
            host(2, s.tick, s.calls, s.digest as u32, (s.digest >> 32) as u32);
        }
        if tick / 20000 != s.tick / 20000 {
            let mut line = format!("EE-UU-COUNT {tick}");
            for k in &s.kinds[1..] {
                line.push_str(&format!(" {k}"));
            }
            eprintln!("{line}");
        }
        s.digest = FNV_OFFSET;
        s.calls = 0;
        s.tick = tick;
    }
    s.kinds[(kind & 127) as usize] += 1;
    let mut digest = s.digest;
    for w in words {
        for b in 0..4 {
            digest ^= u64::from((w >> (8 * b)) & 0xff);
            digest = digest.wrapping_mul(FNV_PRIME);
        }
    }
    s.digest = digest;
    s.calls += 1;
}

#[cfg(not(test))]
fn unit_id(unit: u64) -> i64 {
    // SAFETY: a note's unit is a live unit (or 0) the caller holds
    let t = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: as above
    unsafe { crate::world::UnitView::of(&t, unit) }.map_or(-2, |u| i64::from(u.id()))
}

#[cfg(not(test))]
fn player_id(player: u64) -> i64 {
    // SAFETY: a note's player is a live player (or 0)
    let t = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: as above
    unsafe { crate::world::PlayerView::of(&t, player) }.map_or(-2, |p| i64::from(p.id()))
}

#[cfg(not(test))]
fn class_type(class: u64) -> i64 {
    // SAFETY: a note's class is a live class (or 0)
    let t = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: as above
    unsafe { crate::world::ClassView::of(&t, class) }.map_or(-2, |c| c.unit_type() as i64)
}

/// The units on tile (x, y) of the world's map, in its list's order (none off the map or without one).
#[cfg(not(test))]
fn tile_units(x: i64, y: i64) -> Vec<u64> {
    // SAFETY: the noting thread holds the world
    let t = unsafe { crate::world::WorldRead::begin() };
    let Some(map) = crate::world::WorldView::global(&t).and_then(|w| w.map()) else { return Vec::new() };
    let Some(tile) = map.tile(x, y) else { return Vec::new() };
    tile.units().map(|u| u.map_or(0, |u| u.handle())).collect()
}

// the unit tests' world: handles are ids, every tile empty
#[cfg(test)]
fn unit_id(unit: u64) -> i64 {
    if unit == 0 {
        -2
    } else {
        unit as i64
    }
}
#[cfg(test)]
fn player_id(player: u64) -> i64 {
    unit_id(player)
}
#[cfg(test)]
fn class_type(class: u64) -> i64 {
    unit_id(class)
}
#[cfg(test)]
fn tile_units(_x: i64, _y: i64) -> Vec<u64> {
    Vec::new()
}

// ---- the C++ bodies' notes (EEReplaceUnit, EEConvertUnit, EEUpdateUnitTile, EEGetHelicopterPriestDestination)

/// Opens a note of `kind`; whether it records (its words are worth sending).
#[no_mangle]
pub extern "C" fn ee_units_uu_note_begin(kind: u32) -> u8 {
    let note = Note::begin(kind);
    let on = note.on;
    OPEN.with(|o| o.borrow_mut().push(note));
    u8::from(on)
}

/// A word of the thread's innermost open note (dropped unless it records).
#[no_mangle]
pub extern "C" fn ee_units_uu_note_word(word: u32) {
    OPEN.with(|o| {
        if let Some(n) = o.borrow().last() {
            n.i(i64::from(word));
        }
    });
}

/// Closes the thread's innermost open note (folded when it records).
#[no_mangle]
pub extern "C" fn ee_units_uu_note_end() {
    let note = OPEN.with(|o| o.borrow_mut().pop());
    drop(note);
}

/// An event of the thread's recording note (the C++ host's writes: `UUEvent`).
#[no_mangle]
pub extern "C" fn ee_units_uu_note_event(a: i32, b: i32, c: i32) {
    event(i64::from(a), i64::from(b), i64::from(c));
}

thread_local! {
    /// The notes the C++ opened, innermost last.
    static OPEN: RefCell<Vec<Note>> = const { RefCell::new(Vec::new()) };
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fnv(words: &[u32]) -> u64 {
        let mut d = FNV_OFFSET;
        for w in words {
            for b in w.to_le_bytes() {
                d ^= u64::from(b);
                d = d.wrapping_mul(FNV_PRIME);
            }
        }
        d
    }

    fn reset(active: bool, tick: u32) {
        TEST_HOST.with(|h| *h.borrow_mut() = (active, tick, Vec::new()));
    }

    fn set_tick(tick: u32) {
        TEST_HOST.with(|h| h.borrow_mut().1 = tick);
    }

    fn records() -> Vec<[u32; 4]> {
        TEST_HOST.with(|h| h.borrow().2.clone())
    }

    // One test for the shared state (the tick's digest is the process's, as the C++'s was).
    #[test]
    fn notes_fold_the_outermost_calls_per_tick() {
        reset(true, 7);
        {
            // the previous ticks' state is folded away by the first call of tick 7
            let n = Note::begin(8);
            n.i(3).i(-4).u(9);
            {
                let inner = Note::begin(19);
                assert!(!inner.on(), "a nested note never records");
                inner.i(1);
            }
            event(1, 2, 3);
            n.b(true);
        }
        {
            let n = Note::begin(KIND_SOUND_PARAMETERS);
            assert!(!n.on(), "the sound parameters are never recorded");
            let inner = Note::begin(8);
            assert!(!inner.on());
        }
        let first = [8u32, 3, (-4i32) as u32, 9, 1, 2, 3, 1];
        {
            let n = Note::begin(14);
            n.f(1.5).t(&[[1, 2]]).us(&[0, 5]).pl(0).c(4);
        }
        {
            // the point, matrix and point-list words, and a footprint's empty tiles (-3 each)
            let n = Note::begin(14);
            n.p([1.0, 2.0, 3.0]).m(&[[0.5; 4]; 4]).ps(&[[4.0, 5.0, 6.0]]).foot(0, 0, 2);
        }
        let mut second = vec![14u32, 1.5f32.to_bits(), 1, 1, 2, 2, (-2i32) as u32, 5, (-2i32) as u32, 4];
        second.extend([14, 1.0f32.to_bits(), 2.0f32.to_bits(), 3.0f32.to_bits()]);
        second.extend([0.5f32.to_bits(); 16]);
        second.extend([1, 4.0f32.to_bits(), 5.0f32.to_bits(), 6.0f32.to_bits()]);
        second.extend([(-3i32) as u32; 4]);
        set_tick(8);
        {
            let _n = Note::begin(1);
        }
        let recs = records();
        let last = recs.last().copied().expect("tick 7's record written by tick 8's first call");
        let mut both = first.to_vec();
        both.extend_from_slice(&second);
        let mut d = FNV_OFFSET;
        for w in &both {
            for b in w.to_le_bytes() {
                d ^= u64::from(b);
                d = d.wrapping_mul(FNV_PRIME);
            }
        }
        assert_eq!(fnv(&both), d);
        assert_eq!(last, [7, 3, d as u32, (d >> 32) as u32]);
        // a call with the capture off records nothing and counts in the depth
        reset(false, 9);
        {
            let n = Note::begin(8);
            assert!(!n.on());
        }
        assert!(records().is_empty());
        DEPTH.with(|d| assert_eq!(d.get(), 0));
        // the C++ side's notes share the depth
        reset(true, 9);
        assert_eq!(ee_units_uu_note_begin(85), 1);
        ee_units_uu_note_word(4);
        assert_eq!(ee_units_uu_note_begin(8), 0);
        ee_units_uu_note_end();
        ee_units_uu_note_end();
        DEPTH.with(|d| assert_eq!(d.get(), 0));
    }
}
