//! TEST ONLY: the physics models' step record (PMST; the physics models' lane, round 1, moved from
//! EEPhysicsModel.cpp's `PMStepNote` in the world lane, round 2). Every `Step` call folds the unit's id, its model's
//! index, its physics action before the step and the state the models write after it (the location's matrix, the
//! velocity, the angular velocity, the push, the roll, the bob phase, the collision time, state and blocking unit, the
//! physics action, the tile, the arrival flag, the animation, its pause and time, the hit points, the contact
//! location, the push immunity, the orientation, the height; floats as bits) into the tick's FNV-1a 64; the record
//! (the tick, the calls, the digest) is written when the next tick's first step comes. Nothing is noted unless the
//! capture runs. Its golden came from the C++ models (tools/sim_capture.py).
//!
//! The steps per model so far (the capture's coverage) go to stderr every 4000 ticks:
//! "EE-PM-COUNT <tick> <land> <airplane> .. <spaceship> deck <the deck walks>".

use std::sync::Mutex;

use crate::app::capture_record::{FNV_BASIS, FNV_PRIME};

/// "PMST"
pub const PM_STEP_TAG: u32 = 0x5453_4d50;

/// The fold's state (the C++'s file statics).
#[derive(Debug, Clone)]
pub struct PmStepNotes {
    digest: u64,
    calls: u32,
    tick: u32,
    kinds: [u32; 12],
    deck: u32,
}

impl Default for PmStepNotes {
    fn default() -> Self {
        PmStepNotes { digest: FNV_BASIS, calls: 0, tick: 0, kinds: [0; 12], deck: 0 }
    }
}

/// What a note hands the writer: the finished tick's record words and the coverage line.
#[derive(Debug, Default, PartialEq)]
pub struct PmStepOut {
    pub record: Option<[u32; 4]>,
    pub count_line: Option<String>,
}

impl PmStepNotes {
    fn fnv(&mut self, word: u32) {
        for i in 0..4 {
            self.digest ^= u64::from((word >> (8 * i)) & 0xff);
            self.digest = self.digest.wrapping_mul(FNV_PRIME);
        }
    }

    fn commit(&mut self) -> Option<[u32; 4]> {
        let record =
            (self.calls != 0).then_some([self.tick, self.calls, self.digest as u32, (self.digest >> 32) as u32]);
        self.digest = FNV_BASIS;
        self.calls = 0;
        record
    }

    /// A deck walk's placement (OrientOnCarrier's route).
    pub fn deck_walk(&mut self) {
        self.deck = self.deck.wrapping_add(1);
    }

    /// The step of model `kind` at tick `ticks`: `unit_id`, the action before the step, then the state's words.
    pub fn note(&mut self, ticks: u32, kind: u32, unit_id: u32, action: u32, state: &[u32]) -> PmStepOut {
        let mut out = PmStepOut::default();
        if ticks != self.tick {
            out.record = self.commit();
            self.tick = ticks;
            if self.tick.is_multiple_of(4000) {
                let mut line = format!("EE-PM-COUNT {}", self.tick);
                for k in self.kinds {
                    line.push_str(&format!(" {k}"));
                }
                line.push_str(&format!(" deck {}", self.deck));
                out.count_line = Some(line);
            }
        }
        let model = if kind < 12 { kind } else { 0xff };
        if model < 12 {
            self.kinds[model as usize] = self.kinds[model as usize].wrapping_add(1);
        }
        self.fnv(unit_id);
        self.fnv(model);
        self.fnv(action);
        for &w in state {
            self.fnv(w);
        }
        self.calls = self.calls.wrapping_add(1);
        out
    }
}

static NOTES: Mutex<Option<PmStepNotes>> = Mutex::new(None);

/// The process' notes.
pub fn with_notes<R>(f: impl FnOnce(&mut PmStepNotes) -> R) -> R {
    let mut n = NOTES.lock().unwrap_or_else(|e| e.into_inner());
    f(n.get_or_insert_with(PmStepNotes::default))
}

/// A step's note: the finished tick's record written, the coverage line printed.
pub fn note(ticks: u32, kind: u32, unit_id: u32, action: u32, state: &[u32]) {
    let out = with_notes(|n| n.note(ticks, kind, unit_id, action, state));
    if let Some(words) = out.record {
        let mut r = crate::app::capture_record::Record::new(PM_STEP_TAG);
        for w in words {
            r.u32(w);
        }
        r.commit();
    }
    if let Some(line) = out.count_line {
        eprintln!("{line}");
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fnv_words(words: &[u32]) -> u64 {
        let mut bytes = Vec::new();
        for w in words {
            bytes.extend_from_slice(&w.to_le_bytes());
        }
        crate::app::capture_record::fnv(FNV_BASIS, &bytes)
    }

    #[test]
    fn a_tick_folds_its_steps_and_the_next_tick_writes_the_record() {
        let mut n = PmStepNotes::default();
        // the first tick (1): no record before it
        assert_eq!(n.note(1, 0, 7, 2, &[10, 11]), PmStepOut::default());
        assert_eq!(n.note(1, 6, 8, 0, &[]), PmStepOut::default());
        let out = n.note(2, 0, 7, 2, &[10, 11]);
        let digest = fnv_words(&[7, 0, 2, 10, 11, 8, 6, 0]);
        assert_eq!(out.record, Some([1, 2, digest as u32, (digest >> 32) as u32]));
        assert_eq!(out.count_line, None);
    }

    #[test]
    fn the_coverage_line_every_4000_ticks() {
        let mut n = PmStepNotes::default();
        n.note(3999, 1, 1, 0, &[]);
        n.note(3999, 1, 1, 0, &[]);
        n.deck_walk();
        let out = n.note(4000, 11, 1, 0, &[]);
        assert_eq!(out.count_line.as_deref(), Some("EE-PM-COUNT 4000 0 2 0 0 0 0 0 0 0 0 0 0 deck 1"));
        // a kind past the models folds 0xff and counts nowhere
        n.note(4000, 40, 1, 0, &[]);
        assert_eq!(n.kinds[11], 1);
    }
}
