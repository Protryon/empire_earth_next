//! TEST ONLY: the unit classes' bodies' record (UCBD; ee/EEUnitClassKinds.h `EEUCNote`; the units' lane, round 9:
//! its state and fold in Rust, the notes' words still gathered by the C++ wrappers). The outermost note of a thread
//! folds its words into the physics tick's FNV-1a 64; the record (the tick, the calls, the digest) is written when a
//! later tick's first note ends. Every 20,000 ticks the kinds' counts go to stderr (`EE-UCBD-COUNT`). Nothing is
//! noted unless the capture runs.

use std::cell::Cell;
use std::sync::Mutex;

/// "UCBD"
pub const UCBD: u32 = 0x4442_4355;
const FNV_OFFSET: u64 = 0xcbf2_9ce4_8422_2325;
const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;

thread_local! {
    /// The thread's open notes.
    static DEPTH: Cell<i32> = const { Cell::new(0) };
}

/// The folded notes of the current physics tick.
struct State {
    digest: u64,
    calls: u32,
    tick: u32,
    kinds: [u32; 64],
}

static STATE: Mutex<State> = Mutex::new(State { digest: FNV_OFFSET, calls: 0, tick: 0, kinds: [0; 64] });

/// The world a note asks: whether the capture runs, the physics' ticks done, a record written.
pub trait UcbdWorld {
    fn active(&mut self) -> bool;
    fn ticks(&mut self) -> u32;
    fn record(&mut self, tick: u32, calls: u32, digest: u64);
    fn counts(&mut self, tick: u32, kinds: &[u32; 64]);
}

/// A note opens: whether it records (the thread's outermost, with the capture running).
pub fn begin<W: UcbdWorld + ?Sized>(w: &mut W) -> bool {
    let depth = DEPTH.with(|d| {
        d.set(d.get() + 1);
        d.get()
    });
    depth == 1 && w.active()
}

/// A note closes (`on` its `begin`'s answer): its words folded into the tick's digest (the last tick's record
/// written first when the tick moved on).
pub fn end<W: UcbdWorld + ?Sized>(w: &mut W, on: bool, words: &[u32]) {
    DEPTH.with(|d| d.set(d.get() - 1));
    if !on {
        return;
    }
    let mut state = STATE.lock().unwrap_or_else(|e| e.into_inner());
    let tick = w.ticks();
    if tick != state.tick {
        if state.calls != 0 {
            w.record(state.tick, state.calls, state.digest);
        }
        if tick / 20000 != state.tick / 20000 {
            w.counts(tick, &state.kinds);
        }
        state.digest = FNV_OFFSET;
        state.calls = 0;
        state.tick = tick;
    }
    if let Some(&kind) = words.first() {
        state.kinds[(kind & 63) as usize] += 1;
    }
    for &word in words {
        for b in 0..4 {
            state.digest ^= u64::from((word >> (8 * b)) & 0xff);
            state.digest = state.digest.wrapping_mul(FNV_PRIME);
        }
    }
    state.calls += 1;
}

#[cfg(not(test))]
mod game {
    use super::*;

    struct Game;

    impl UcbdWorld for Game {
        fn active(&mut self) -> bool {
            crate::app::capture_record::active()
        }
        fn ticks(&mut self) -> u32 {
            // the physics' ticks done (the unit utilities' note host, EEFOFindNewTargetNearOldTarget.cpp)
            crate::units::uu_note::host(1, 0, 0, 0, 0)
        }
        fn record(&mut self, tick: u32, calls: u32, digest: u64) {
            let mut r = crate::app::capture_record::Record::new(UCBD);
            r.u32(tick);
            r.u32(calls);
            r.u32(digest as u32);
            r.u32((digest >> 32) as u32);
            r.commit();
        }
        fn counts(&mut self, tick: u32, kinds: &[u32; 64]) {
            let mut line = format!("EE-UCBD-COUNT {tick}");
            for (i, n) in kinds.iter().enumerate().take(50).skip(1) {
                line.push_str(&format!(" {i}:{n}"));
            }
            eprintln!("{line}");
        }
    }

    /// `EEUCNote`'s constructor: whether the note records.
    #[no_mangle]
    pub extern "C" fn ee_units_ucbd_begin() -> u8 {
        u8::from(begin(&mut Game))
    }

    /// `EEUCNote`'s destructor: its words folded (`on` its constructor's answer).
    ///
    /// # Safety
    /// `words` holds `count` words (or is NULL with 0).
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucbd_end(on: u8, words: *const u32, count: usize) {
        let words = if words.is_null() || count == 0 {
            &[][..]
        } else {
            // SAFETY: the caller's contract
            unsafe { std::slice::from_raw_parts(words, count) }
        };
        end(&mut Game, on != 0, words);
    }
}

#[cfg(test)]
#[path = "ucbd_note_tests.rs"]
mod tests;
