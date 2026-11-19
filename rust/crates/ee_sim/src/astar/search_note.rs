//! TEST ONLY: the A* capture's side of the Rust searches (`ee/EEAIAStarCapture.h`, tags ASRN and ASGR). A search
//! notes every question its machine asks the goal (the event kinds below, after the goal's answer, as the C++
//! machine's hooks did) and, while the capture runs, writes one record per `Run`: the inputs, the outputs and the
//! storage's lists (digested by the C++ run, `EEAIAStarCaptureRun::Nodes`). The view's queries are noted by the C++
//! view itself, in between. Nothing is noted unless a run is being captured on the thread (the C++'s
//! `gEEAIAStarCaptureRun`).

use crate::astar::ffi::EeAStarNodeView;
use std::ffi::c_void;

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

#[cfg(not(test))]
extern "C" {
    fn ee_astar_capture_note(kind: u8, a: i32, b: i32, c: i32, d: i32, e: i32, answer: i32);
    fn ee_astar_capture_begin() -> *mut c_void;
    fn ee_astar_capture_bytes(run: *mut c_void, data: *const u8, length: u32);
    fn ee_astar_capture_nodes(run: *mut c_void, nodes: *const EeAStarNodeView, count: u32);
    fn ee_astar_capture_end(run: *mut c_void, tag: u32);
}

#[cfg(test)]
thread_local! {
    /// The unit tests' capture: the notes made (kind, the five arguments, the answer).
    pub(crate) static TEST_NOTES: std::cell::RefCell<Vec<(u8, [i32; 5], i32)>> =
        const { std::cell::RefCell::new(Vec::new()) };
}

/// `EEAIAStarNote(kind, a .. e, answer)`; returns the answer.
pub(crate) fn note(kind: u8, a: i32, b: i32, c: i32, d: i32, e: i32, answer: i32) -> i32 {
    #[cfg(not(test))]
    // SAFETY: a plain C++ call (a no-op unless a run is captured on this thread)
    unsafe {
        ee_astar_capture_note(kind, a, b, c, d, e, answer)
    };
    #[cfg(test)]
    TEST_NOTES.with(|n| n.borrow_mut().push((kind, [a, b, c, d, e], answer)));
    answer
}

/// A bool's note: the answer as 0 / 1.
pub(crate) fn note_bool(kind: u8, a: i32, b: i32, c: i32, d: i32, e: i32, answer: bool) -> bool {
    note(kind, a, b, c, d, e, i32::from(answer)) != 0
}

/// One run's record (`EEAIAStarCaptureRun`): null while the capture is off.
pub(crate) struct CaptureRun {
    run: *mut c_void,
    data: Vec<u8>,
}

impl CaptureRun {
    /// Opens the run (it becomes the thread's captured run) when the capture is on.
    pub(crate) fn begin() -> CaptureRun {
        #[cfg(not(test))]
        // SAFETY: a plain C++ call
        let run = unsafe { ee_astar_capture_begin() };
        #[cfg(test)]
        let run = std::ptr::null_mut();
        CaptureRun { run, data: Vec::new() }
    }
    pub(crate) fn on(&self) -> bool {
        !self.run.is_null()
    }
    pub(crate) fn bool(&mut self, v: bool) {
        self.data.push(u8::from(v));
    }
    pub(crate) fn i32(&mut self, v: i32) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    /// `EEAIAStarCaptureRun::Node`
    pub(crate) fn node(&mut self, n: &EeAStarNodeView) {
        self.i32(n.x);
        self.i32(n.y);
        self.i32(n.cost as i32);
        self.i32(n.path_cost);
        self.i32(n.estimate);
        self.bool(n.has_parent);
        self.i32(n.parent_x);
        self.i32(n.parent_y);
    }
    /// `EEAIAStarCaptureRun::Nodes`: into the run's storage digest (the record's bytes so far go first)
    pub(crate) fn nodes(&mut self, nodes: &[EeAStarNodeView]) {
        if !self.on() {
            return;
        }
        self.flush();
        #[cfg(not(test))]
        // SAFETY: the run is open; the nodes are readable for their count
        unsafe {
            ee_astar_capture_nodes(self.run, nodes.as_ptr(), nodes.len() as u32)
        };
        #[cfg(test)]
        let _ = nodes;
    }
    fn flush(&mut self) {
        if !self.on() || self.data.is_empty() {
            return;
        }
        #[cfg(not(test))]
        // SAFETY: the run is open; the bytes are readable
        unsafe {
            ee_astar_capture_bytes(self.run, self.data.as_ptr(), self.data.len() as u32)
        };
        self.data.clear();
    }
    /// Writes the record (the events' count and digest, the storage digest) and closes the run.
    pub(crate) fn commit(mut self, tag: u32) {
        if !self.on() {
            return;
        }
        self.flush();
        #[cfg(not(test))]
        // SAFETY: the run is open; this closes it
        unsafe {
            ee_astar_capture_end(self.run, tag)
        };
        #[cfg(test)]
        let _ = tag;
        self.run = std::ptr::null_mut();
    }
}
