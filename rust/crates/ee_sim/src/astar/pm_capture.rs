//! TEST ONLY (docs/RUST_PLAN.md R5b.1, tools/sim_capture.py): the path manager's record PMRQ (was
//! `EEPathRequestCapture` of EEPathManager.cpp; AI Thread cluster round 6), one per outermost call of its entry
//! points (the units', the AI's, the storm's, the physics tick's): a u32 kind, the inputs, the result, then the
//! queued searches after the call (per search: the queue, unit, goal tile, passes, request id, the machine's type,
//! revs, steps, found, step and rev limits) and every unit and request whose search left the queues during the call
//! with what it got (a unit's waypoints, path-done flag, request count, bounded tiles, goals; a request's path,
//! length and done flag). An Update that leaves nothing and finds nothing queued writes no record.
//!
//! The record reads only the manager's queues and the objects, so it is the same over any implementation of them.
//! Its scopes are a thread's stack: the manager's calls open theirs in `ee_path_manager_call`, the two bounded
//! searches the C++ keeps open theirs through `ee_path_capture_open` / `_close`; only the outermost records.

use std::cell::RefCell;
use std::ffi::{c_long, c_void};

use crate::app::capture_record::Record;
use crate::astar::path_splice::EePathPoint;
use crate::astar::pm::*;
use crate::astar::pm_bounded::EE_PATH_UNIT_WAYPOINT_POINTS;
use crate::world::{UnitView, WorldRead};

/// "PMRQ"
const TAG: u32 = 0x5152_4d50;
/// The entry points' kinds.
const KIND_PATH: u32 = 1;
const KIND_UNIT_PATH: u32 = 2;
const KIND_COMPLETE_PATH: u32 = 5;
const KIND_COMPLETE_AUTO_PATH: u32 = 6;
const KIND_CANCEL_PATHS: u32 = 7;
const KIND_CANCEL_REQUEST: u32 = 8;
const KIND_UPDATE: u32 = 9;
const KIND_PENDING: u32 = 10;
const KIND_REGION_UPDATES: u32 = 11;
const KIND_IS_PATH_DONE: u32 = 12;
const KIND_HAS_AUTO_PATH: u32 = 13;
const KIND_GET_PATH_GOAL: u32 = 14;
const KIND_RESET: u32 = 15;

/// A unit's goals as the record reads them (the C++ host's `EE_PATH_UNIT_CAPTURE`): the first queue's length into
/// the answer; `i0` whether the goal is a location goal, `i1`, `i2` its location's x and y (f32 bits).
pub const EE_PATH_UNIT_CAPTURE: i32 = 70;

/// A queued search as the record writes it.
#[derive(Clone, Copy, Debug, Default)]
struct Entry {
    words: [i32; 12],
    request: u64,
}

impl Entry {
    /// `operator==`: the queue, unit, tile and request.
    fn same(&self, o: &Entry) -> bool {
        self.words[..4] == o.words[..4] && self.request == o.request
    }
}

/// One open scope.
struct Scope {
    on: bool,
    kind: u32,
    rec: Record,
    before: Vec<Entry>,
}

thread_local! {
    static SCOPES: RefCell<Vec<Scope>> = const { RefCell::new(Vec::new()) };
}

fn token() -> WorldRead {
    // SAFETY: the manager's calls run on the thread that holds the world
    unsafe { WorldRead::begin() }
}

fn with_top(f: impl FnOnce(&mut Record)) {
    SCOPES.with(|s| {
        if let Some(top) = s.borrow_mut().last_mut() {
            if top.on {
                f(&mut top.rec);
            }
        }
    });
}

fn top_on() -> bool {
    SCOPES.with(|s| s.borrow().last().is_some_and(|t| t.on))
}

fn request_record(host: &mut dyn PathHost, request: u64) -> RequestRecord {
    let mut r = RequestRecord::default();
    let mut io = EePathIo::default();
    io.h[0] = request;
    io.h[1] = &mut r as *mut RequestRecord as u64;
    host.call(EE_PATH_REQUEST_SAVE, &mut io);
    r
}

fn entries(m: &PathManager, host: &mut dyn PathHost) -> Vec<Entry> {
    let mut out = Vec::new();
    for q in m.queued() {
        let mut w = [0i32; 12];
        w[0] = q.queue as i32;
        w[1] = q.unit as i32;
        w[2] = q.x;
        w[3] = q.y;
        w[4] = q.passes;
        w[5] = if q.request != 0 { request_record(host, q.request).r.id } else { -1 };
        if q.queue == 0 && q.machine != 0 {
            let mut i = crate::astar::path_search::EeAStarPathSearchInfo::default();
            // SAFETY: a queued tile search's machine
            unsafe { crate::astar::path_search::ee_astar_path_search_info(q.machine as *const c_void, false, &mut i) };
            w[6..12].copy_from_slice(&[i.machine_type, i.revs, i.steps, i32::from(i.found), i.max_revs, i.rev_limit]);
        } else if q.queue == 1 && q.machine != 0 {
            let mut i = crate::astar::graph_search::EeAStarGraphSearchInfo::default();
            // SAFETY: a queued graph search's machine
            unsafe { crate::astar::graph_search::ee_astar_graph_search_info(q.machine as *const c_void, &mut i) };
            w[6..12].copy_from_slice(&[i.machine_type, i.revs, i.steps, i32::from(i.found), i.max_revs, i.rev_limit]);
        }
        out.push(Entry { words: w, request: q.request });
    }
    out
}

/// `EEPathCaptureUnitID`: the unit's `mUnitID`, all ones for none.
fn unit_id(unit: u64) -> u32 {
    let t = token();
    // SAFETY: 0 or a live unit
    unsafe { UnitView::of(&t, unit) }.map_or(u32::MAX, |u| u.id())
}

/// `Unit(id)`: the unit's path state as it is now.
fn write_unit(rec: &mut Record, host: &mut dyn PathHost, id: u32) {
    let unit = if id == u32::MAX {
        0
    } else {
        let mut io = EePathIo::default();
        io.i[0] = i64::from(id);
        host.call(EE_PATH_UNIT_RESOLVE, &mut io) as u64
    };
    rec.u32(if unit != 0 { id } else { u32::MAX });
    if unit == 0 {
        return;
    }
    let mut points: Vec<EePathPoint> = Vec::new();
    let mut io = EePathIo::default();
    io.h[0] = unit;
    io.h[1] = &mut points as *mut Vec<EePathPoint> as u64;
    host.call(EE_PATH_UNIT_WAYPOINT_POINTS, &mut io);
    rec.u32(points.len() as u32);
    for p in &points {
        rec.f32(p.x);
        rec.f32(p.y);
        rec.f32(p.z);
    }
    let t = token();
    // SAFETY: a resolved live unit
    let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return };
    let c = u.complex();
    rec.u32(u32::from(crate::world_field!(c, mIsPathDone)));
    let requests: c_long = crate::world_field!(c, mNumPathRequests);
    rec.i32(requests as i32);
    rec.u32(u.bounded_path_tiles().len() as u32);
    let mut g = EePathIo::default();
    g.h[0] = unit;
    let goals = host.call(EE_PATH_UNIT_CAPTURE, &mut g);
    rec.u32(goals as u32);
    rec.i32(u.goal().map_or(-1, |(kind, _)| i32::from(kind)));
    let (x, y) = if g.i[0] != 0 { (f32::from_bits(g.i[1] as u32), f32::from_bits(g.i[2] as u32)) } else { (0.0, 0.0) };
    rec.f32(x);
    rec.f32(y);
}

/// `Request(request)`: its id, path, length and done flag (-1 alone for none).
fn write_request(rec: &mut Record, host: &mut dyn PathHost, request: u64) {
    if request == 0 {
        rec.i32(-1);
        return;
    }
    let r = request_record(host, request);
    rec.i32(r.r.id);
    rec.u32(r.path.len() as u32);
    for t in &r.path {
        rec.i32(t[0]);
        rec.i32(t[1]);
    }
    rec.i32(r.r.length);
    rec.u32(u32::from(r.r.done));
}

fn open(m: &PathManager, host: &mut dyn PathHost, kind: u32) -> bool {
    let outer = SCOPES.with(|s| s.borrow().is_empty());
    let on = outer && crate::app::capture_record::active();
    let mut scope = Scope { on, kind, rec: Record::new(TAG), before: Vec::new() };
    if on {
        scope.rec.u32(kind);
        scope.before = entries(m, host);
        for e in &scope.before {
            if e.request != 0 {
                // read after the call, whoever else let go of it
                let mut io = EePathIo::default();
                io.h[0] = e.request;
                host.call(EE_PATH_REQUEST_REFERENCE, &mut io);
            }
        }
    }
    SCOPES.with(|s| s.borrow_mut().push(scope));
    on
}

fn close(m: &PathManager, host: &mut dyn PathHost) {
    let Some(mut s) = SCOPES.with(|s| s.borrow_mut().pop()) else { return };
    if !s.on {
        return;
    }
    let after = entries(m, host);
    let left: Vec<Entry> = s.before.iter().filter(|b| !after.iter().any(|a| a.same(b))).copied().collect();
    if s.kind != KIND_UPDATE || !s.before.is_empty() || !after.is_empty() {
        s.rec.u32(0x2d2d_2d2d);
        s.rec.u32(after.len() as u32);
        for e in &after {
            for w in e.words {
                s.rec.i32(w);
            }
        }
        s.rec.u32(left.len() as u32);
        for e in &left {
            for w in e.words {
                s.rec.i32(w);
            }
            write_unit(&mut s.rec, host, e.words[1] as u32);
            if e.request != 0 {
                write_request(&mut s.rec, host, e.request);
            }
        }
        s.rec.commit();
    }
    for e in &s.before {
        if e.request != 0 {
            let mut io = EePathIo::default();
            io.h[0] = e.request;
            host.call(EE_PATH_REQUEST_DEREFERENCE, &mut io);
        }
    }
}

/// `EECPASGFlags`' layout.
#[repr(C)]
struct Flags {
    player: c_long,
    flags: [bool; 17],
}

/// The scope of `ee_path_manager_call(what)` with its inputs; false (no scope opened, none to close) for a call the
/// record does not cover.
pub(crate) fn open_call(m: &PathManager, host: &mut dyn PathHost, what: i32, io: &EePathIo) -> bool {
    let (h, i) = (io.h, io.i);
    let kind = match what {
        1 => KIND_UPDATE,
        2 => KIND_RESET,
        5 => KIND_PENDING,
        6 => KIND_REGION_UPDATES,
        7 => KIND_CANCEL_PATHS,
        8 => KIND_CANCEL_REQUEST,
        9 => KIND_COMPLETE_PATH,
        10 => KIND_COMPLETE_AUTO_PATH,
        11 => KIND_HAS_AUTO_PATH,
        12 => KIND_IS_PATH_DONE,
        13 => KIND_GET_PATH_GOAL,
        14 => KIND_PATH,
        15 => KIND_UNIT_PATH,
        _ => return false,
    };
    if !open(m, host, kind) {
        return true;
    }
    let tile = |r: &mut Record, a: usize| {
        r.i32(i[a] as i32);
        r.i32(i[a + 1] as i32);
    };
    let mut rec = SCOPES.with(|s| std::mem::take(&mut s.borrow_mut().last_mut().expect("open").rec));
    match kind {
        KIND_PENDING => {
            rec.u32(m.pending.len() as u32);
            for p in &m.pending {
                rec.u32(p.unit);
                rec.i32(p.x);
                rec.i32(p.y);
            }
        }
        KIND_REGION_UPDATES => {
            rec.u32((i[0] != 0) as u32);
            rec.u32((i[1] != 0) as u32);
            rec.u32(u32::from(m.updating_regions));
            rec.u32(m.next_region_time);
        }
        KIND_CANCEL_PATHS => {
            rec.u32(unit_id(h[0]));
            rec.u32((i[0] != 0) as u32);
        }
        KIND_CANCEL_REQUEST | KIND_COMPLETE_AUTO_PATH => write_request(&mut rec, host, h[0]),
        KIND_COMPLETE_PATH => {
            let id = unit_id(h[0]);
            rec.u32(id);
            write_unit(&mut rec, host, id);
        }
        KIND_HAS_AUTO_PATH | KIND_IS_PATH_DONE | KIND_GET_PATH_GOAL => rec.u32(unit_id(h[0])),
        KIND_PATH => {
            rec.u32(i[0] as u32);
            tile(&mut rec, 1);
            tile(&mut rec, 3);
            // SAFETY: the call's flags (an `EECPASGFlags*`)
            let f = unsafe { &*(h[0] as *const Flags) };
            rec.i32(i64::from(f.player) as i32);
            for b in f.flags {
                rec.u32(u32::from(b));
            }
            rec.i32(i[5] as i32);
            rec.i32(i[6] as i32);
            rec.u32(u32::from(h[1] != 0));
            rec.u32((i[7] != 0) as u32);
        }
        KIND_UNIT_PATH => {
            let id = unit_id(h[0]);
            rec.u32(id);
            tile(&mut rec, 0);
            tile(&mut rec, 2);
            rec.u32((i[4] != 0) as u32);
            rec.i32(i[5] as i32);
            rec.u32((i[6] != 0) as u32);
            rec.u32(u32::from(h[1] != 0));
            rec.u32((i[7] != 0) as u32);
            write_unit(&mut rec, host, id);
        }
        _ => {}
    }
    SCOPES.with(|s| s.borrow_mut().last_mut().expect("open").rec = rec);
    true
}

/// The end of the scope `open_call` opened: the call's results, then the queues.
pub(crate) fn close_call(m: &PathManager, host: &mut dyn PathHost, what: i32, io: &EePathIo, result: i64) {
    match what {
        11 | 12 => with_top(|r| r.u32(u32::from(result != 0))),
        13 => with_top(|r| {
            r.u32(u32::from(result != 0));
            r.i32(io.i[0] as i32);
            r.i32(io.i[1] as i32);
        }),
        14 if top_on() => {
            let mut rec = SCOPES.with(|s| std::mem::take(&mut s.borrow_mut().last_mut().expect("open").rec));
            write_request(&mut rec, host, result as u64);
            SCOPES.with(|s| s.borrow_mut().last_mut().expect("open").rec = rec);
        }
        _ => {}
    }
    close(m, host);
}

/// Opens the record of a call the C++ makes (`kind`: 3 `FindPath`, 4 `FindBoundedPath`); whether it records.
///
/// # Safety
/// `p` a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_path_capture_open(p: *mut EePathManager, kind: u32) -> bool {
    with_manager(p, |m, host| open(m, host, kind))
}

/// The open record's next word (a u32, i32 or f32's bits).
#[no_mangle]
pub extern "C" fn ee_path_capture_word(bits: u32) {
    with_top(|r| r.u32(bits));
}

/// Closes the record `ee_path_capture_open` opened.
///
/// # Safety
/// `p` a live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_path_capture_close(p: *mut EePathManager) {
    with_manager(p, |m, host| close(m, host));
}

#[cfg(test)]
mod tests {
    use super::*;

    struct NoHost;
    impl PathHost for NoHost {
        fn call(&mut self, _op: i32, _io: &mut EePathIo) -> i64 {
            0
        }
    }

    #[test]
    fn entries_match_by_queue_unit_tile_and_request() {
        let a = Entry { words: [0, 7, 3, 4, 1, 9, 2, 5, 6, 1, 8, 8], request: 0x10 };
        let mut b = a;
        b.words[4..].copy_from_slice(&[9, 9, 9, 9, 9, 9, 9, 9]);
        assert!(a.same(&b), "the search's progress is not its identity");
        b.request = 0x20;
        assert!(!a.same(&b));
        let mut c = a;
        c.words[2] = 5;
        assert!(!a.same(&c));
    }

    #[test]
    fn a_scope_without_the_capture_records_nothing_and_nests() {
        let m = PathManager::default();
        let mut host = NoHost;
        assert!(!open(&m, &mut host, KIND_UPDATE));
        assert!(!open(&m, &mut host, KIND_RESET), "an inner scope is off");
        assert!(!top_on());
        ee_path_capture_word(1);
        close(&m, &mut host);
        close(&m, &mut host);
        assert!(SCOPES.with(|s| s.borrow().is_empty()));
        assert!(!open_call(&m, &mut host, 3, &EePathIo::default()), "Clear has no record");
        open(&m, &mut host, 3);
        assert!(!open_call(&m, &mut host, 27, &EePathIo::default()), "the bounded search's own call opens none");
        assert_eq!(SCOPES.with(|s| s.borrow().len()), 1, "and leaves the C++ scope open");
        close(&m, &mut host);
    }
}
