//! TEST ONLY: the A* capture of the Rust searches (tags ASRN and ASGR; was `EEAIAStarCapture.cpp`, moved here in the
//! game's leaves round 1). A search notes every question its machine asks the goal (the event kinds below, after the
//! goal's answer) and, while the capture runs, writes one record per `Run`: the inputs, the outputs, the events' count
//! and FNV-1a 64 digest, the digest of the storage's lists, and with `EE_SIM_CAPTURE_ASTAR_EVENTS=1` the raw events.
//! The C++ view's queries are noted through `ee_astar_capture_note`, in between. Nothing is noted unless a run is
//! being captured on the thread (the innermost run gets the notes).

use crate::astar::ffi::EeAStarNodeView;
use ee_base::sim_capture::{self, fnv, FNV_BASIS};
use std::cell::RefCell;

// the event kinds (`EEAIAStarEventKind`)
pub(crate) const EV_ACCEPT: u8 = b'A';
pub(crate) const EV_COST: u8 = b'C';
pub(crate) const EV_ESTIMATE: u8 = b'E';
pub(crate) const EV_GOAL_NODE: u8 = b'G';
pub(crate) const EV_SEARCH_DONE: u8 = b'S';
pub(crate) const EV_MARK_TILE: u8 = b'M';
pub(crate) const EV_GRAPH_START: u8 = b'o';
pub(crate) const EV_GRAPH_ESTIMATE: u8 = b'e';
pub(crate) const EV_GRAPH_GOAL_NODE: u8 = b'g';
pub(crate) const EV_GRAPH_ENTER: u8 = b'n';
pub(crate) const EV_GRAPH_COST: u8 = b'c';
pub(crate) const EV_GRAPH_SEARCH_DONE: u8 = b's';

/// The records' tags ("ASRN", "ASGR").
pub(crate) const TAG_TILE_RUN: u32 = 0x4e52_5341;
pub(crate) const TAG_GRAPH_RUN: u32 = 0x5247_5341;

/// A captured run's events: the count, the digest and (`EE_SIM_CAPTURE_ASTAR_EVENTS`) the raw events.
struct Events {
    hash: u64,
    count: u32,
    raw: Vec<u8>,
}

thread_local! {
    /// The thread's captured runs, the innermost last.
    static RUNS: RefCell<Vec<Events>> = const { RefCell::new(Vec::new()) };
}

fn keep_events() -> bool {
    static KEEP: std::sync::OnceLock<bool> = std::sync::OnceLock::new();
    *KEEP.get_or_init(|| {
        std::env::var_os("EE_SIM_CAPTURE_ASTAR_EVENTS").is_some_and(|v| v.as_encoded_bytes().first() == Some(&b'1'))
    })
}

/// One event's 25 bytes: the kind, then the six values as little-endian 32-bit words.
fn event_bytes(kind: u8, values: [i32; 6]) -> [u8; 25] {
    let mut b = [0u8; 25];
    b[0] = kind;
    for (i, v) in values.iter().enumerate() {
        b[1 + i * 4..5 + i * 4].copy_from_slice(&v.to_le_bytes());
    }
    b
}

fn record_event(kind: u8, values: [i32; 6]) {
    RUNS.with(|r| {
        if let Some(run) = r.borrow_mut().last_mut() {
            let b = event_bytes(kind, values);
            run.hash = fnv(run.hash, &b);
            run.count = run.count.wrapping_add(1);
            if keep_events() {
                run.raw.extend_from_slice(&b);
            }
        }
    });
}

/// The C++ view's notes (`EEAIAStarView::Note`): a no-op unless a run is captured on this thread.
#[no_mangle]
pub extern "C" fn ee_astar_capture_note(kind: u8, a: i32, b: i32, c: i32, d: i32, e: i32, answer: i32) {
    record_event(kind, [a, b, c, d, e, answer]);
}

#[cfg(test)]
thread_local! {
    /// The unit tests' capture: the notes made (kind, the five arguments, the answer).
    pub(crate) static TEST_NOTES: std::cell::RefCell<Vec<(u8, [i32; 5], i32)>> =
        const { std::cell::RefCell::new(Vec::new()) };
}

/// `EEAIAStarNote(kind, a .. e, answer)`; returns the answer.
pub(crate) fn note(kind: u8, a: i32, b: i32, c: i32, d: i32, e: i32, answer: i32) -> i32 {
    record_event(kind, [a, b, c, d, e, answer]);
    #[cfg(test)]
    TEST_NOTES.with(|n| n.borrow_mut().push((kind, [a, b, c, d, e], answer)));
    answer
}

/// A bool's note: the answer as 0 / 1.
pub(crate) fn note_bool(kind: u8, a: i32, b: i32, c: i32, d: i32, e: i32, answer: bool) -> bool {
    note(kind, a, b, c, d, e, i32::from(answer)) != 0
}

/// One run's record (`EEAIAStarCaptureRun`): off while the capture is off.
pub(crate) struct CaptureRun {
    on: bool,
    data: Vec<u8>,
    storage_hash: u64,
}

impl CaptureRun {
    /// Opens the run (it becomes the thread's captured run) when the capture is on.
    pub(crate) fn begin() -> CaptureRun {
        let on = sim_capture::active();
        if on {
            RUNS.with(|r| r.borrow_mut().push(Events { hash: FNV_BASIS, count: 0, raw: Vec::new() }));
        }
        CaptureRun { on, data: Vec::new(), storage_hash: FNV_BASIS }
    }
    pub(crate) fn on(&self) -> bool {
        self.on
    }
    pub(crate) fn bool(&mut self, v: bool) {
        self.data.push(u8::from(v));
    }
    pub(crate) fn i32(&mut self, v: i32) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    /// `EEAIAStarCaptureRun::Node`: a node's fields into the record
    pub(crate) fn node(&mut self, n: &EeAStarNodeView) {
        Self::node_bytes(&mut self.data, n);
    }
    fn node_bytes(out: &mut Vec<u8>, n: &EeAStarNodeView) {
        for v in [n.x, n.y, n.cost as i32, n.path_cost, n.estimate] {
            out.extend_from_slice(&v.to_le_bytes());
        }
        out.push(u8::from(n.has_parent));
        out.extend_from_slice(&n.parent_x.to_le_bytes());
        out.extend_from_slice(&n.parent_y.to_le_bytes());
    }
    /// `EEAIAStarCaptureRun::Nodes`: the count and each node's fields into the run's storage digest
    pub(crate) fn nodes(&mut self, nodes: &[EeAStarNodeView]) {
        if !self.on {
            return;
        }
        let mut b = Vec::with_capacity(4 + nodes.len() * 25);
        b.extend_from_slice(&(nodes.len() as i32).to_le_bytes());
        for n in nodes {
            Self::node_bytes(&mut b, n);
        }
        self.storage_hash = fnv(self.storage_hash, &b);
    }
    /// Writes the record (the bytes, the events' count and digest, the storage digest, the raw events) and closes
    /// the run.
    pub(crate) fn commit(mut self, tag: u32) {
        if !self.on {
            return;
        }
        let events = RUNS.with(|r| r.borrow_mut().pop());
        let events = events.unwrap_or(Events { hash: FNV_BASIS, count: 0, raw: Vec::new() });
        let mut rec = sim_capture::Record::new(tag);
        rec.bytes(&self.data);
        rec.u32(events.count);
        rec.u64(events.hash);
        rec.u64(self.storage_hash);
        rec.bytes(&events.raw);
        rec.commit();
        self.on = false;
    }
}

#[cfg(test)]
mod capture_tests {
    use super::*;

    #[test]
    fn event_bytes_are_the_kind_then_six_words() {
        let b = event_bytes(b'V', [1, -1, 2, 3, 4, 0x0102_0304]);
        assert_eq!(b[0], b'V');
        assert_eq!(&b[1..5], &[1, 0, 0, 0]);
        assert_eq!(&b[5..9], &[0xff; 4]);
        assert_eq!(&b[21..25], &[4, 3, 2, 1]);
    }

    #[test]
    fn notes_go_to_the_innermost_run_only() {
        RUNS.with(|r| {
            let mut r = r.borrow_mut();
            r.clear();
            r.push(Events { hash: FNV_BASIS, count: 0, raw: Vec::new() });
            r.push(Events { hash: FNV_BASIS, count: 0, raw: Vec::new() });
        });
        ee_astar_capture_note(b'A', 1, 2, 3, 0, 0, 1);
        let (outer, inner) = RUNS.with(|r| {
            let mut r = r.borrow_mut();
            let inner = r.pop().unwrap();
            let outer = r.pop().unwrap();
            (outer, inner)
        });
        assert_eq!((outer.count, outer.hash), (0, FNV_BASIS));
        assert_eq!(inner.count, 1);
        assert_eq!(inner.hash, fnv(FNV_BASIS, &event_bytes(b'A', [1, 2, 3, 0, 0, 1])));
        // no run: nothing is kept
        ee_astar_capture_note(b'A', 1, 2, 3, 0, 0, 1);
        assert!(RUNS.with(|r| r.borrow().is_empty()));
    }

    #[test]
    fn a_run_is_off_without_the_capture() {
        if std::env::var_os("EE_SIM_CAPTURE").is_none() {
            let mut run = CaptureRun::begin();
            assert!(!run.on());
            run.nodes(&[]);
            run.commit(TAG_TILE_RUN);
        }
    }
}
