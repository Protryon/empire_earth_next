//! The path manager's bounded searches and a finished tile search's result (EEPathManager.cpp), the C++ bodies in
//! their order over the manager's host: `TryShortPath` (the short search before a tile search), `FindPath` (the
//! wall manager's and the storm's bounded path), `FindBoundedPath` (`EEUnitClass`'s), `FinishPath` with
//! `FinishFoundPath` and `FinishFailedPath` (the path found so far joined through a short detour), and the
//! splicing between them: `TrimPath`, `AppendWaypoints`, `JoinPaths`, `RunBoundedPath` (the decisions are
//! `path_splice.rs`'s). The PMSP records (TEST ONLY) are written here in the C++'s layout and committed through
//! the host. The bounded machine, its goal and modifiers, the units and the C++ vectors stay the host's.

use crate::astar::path_splice::{
    append_waypoints, bounds_around, detour_join_count, join_paths, short_path_revs, trim_index, um_floor,
    EePathBounds, EePathPoint,
};
use crate::astar::pm::{EePathIo, PathHost, EE_PATH_UNIT_WAYPOINTS};

// ---- the host's operations for the bounded machine (`mBoundedMachine`) and the units
/// `PrepareBoundedMachine(h1 map)` -> the bounded machine
pub const EE_PATH_BM_PREPARE: i32 = 100;
/// the bounded goal's modifier of type `i0` (sea 2, space 0x10; else none)
pub const EE_PATH_BM_MODIFIER: i32 = 101;
/// the bounded goal's `SetModifier(h1)`
pub const EE_PATH_BM_SET_MODIFIER: i32 = 102;
/// the bounded goal's `SetUnit(h1)`
pub const EE_PATH_BM_SET_UNIT: i32 = 103;
/// `SetStart(i0, i1)`, `SetGoal(i2, i3)` and, when `i4`, `mTile = (i2, i3)`
pub const EE_PATH_BM_ENDS: i32 = 104;
/// `SetMaxRevs(i0)`
pub const EE_PATH_BM_SET_MAX_REVS: i32 = 105;
/// `GetMaxRevs()`
pub const EE_PATH_BM_MAX_REVS: i32 = 106;
/// the goal's `mBounds` and the machine's `mSearchBounds` = (top `i0`, left `i1`, bottom `i2`, right `i3`)
pub const EE_PATH_BM_BOUNDS: i32 = 107;
/// `Run()`
pub const EE_PATH_BM_RUN: i32 = 108;
/// `mTile` -> `i0`, `i1`
pub const EE_PATH_BM_TILE: i32 = 109;
/// `Reset()`
pub const EE_PATH_BM_RESET: i32 = 110;
/// `GetPath(*h1)` (a C++ `std::vector<U2DPoint<long> >`), or `GetPath(*h1, *h2)` with the visited tiles
pub const EE_PATH_BM_PATH: i32 = 111;
/// `FindBoundedPath`'s setup: `CancelSearch`, the goal's `mMaxRevs = i0`, start (`i1`, `i2`), goal (`i3`, `i4`),
/// the destination, `mRevLimit = -1`, `SetUnit(h1)`, `mIsBounded`
pub const EE_PATH_BM_BOUNDED_START: i32 = 112;
/// the goal's `ReleaseModifier()`
pub const EE_PATH_BM_RELEASE_MODIFIER: i32 = 113;
/// `RunBoundedPath`'s setup: the goal's `mMaxRevs = i0`, `SetUnit(h1)`, `mAcceptNearest`, start (`i1`, `i2`), goal
/// (`i3`, `i4`), then `Run()` -> `!mSearchFailed`
pub const EE_PATH_BM_RUN_BOUNDED: i32 = 114;
/// the goal's `GetPoints` into the point list `h1`
pub const EE_PATH_BM_GOAL_POINTS: i32 = 115;
/// tile machine `h0`'s points (`GetPoints`, the bounded or a path machine by `mType`) into the point list `h1`
pub const EE_PATH_TM_POINTS: i32 = 116;
/// tile machine `h0`'s goal flags: 1 `mAcceptNearest`, 2 `mTookNearest`
pub const EE_PATH_TM_FLAGS: i32 = 117;
/// tile machine `h0`'s `mType`
pub const EE_PATH_TM_TYPE: i32 = 118;
/// the map `h1`'s size (`mSize`)
pub const EE_PATH_MAP_SIZE: i32 = 119;
/// unit `h0`'s waypoints' locations into the point list `h1`
pub const EE_PATH_UNIT_WAYPOINT_POINTS: i32 = 120;
/// unit `h0`'s location -> the bits of x, y in `i0`, `i1`
pub const EE_PATH_UNIT_LOCATION: i32 = 121;
/// unit `h0`'s tile (`mTileX`, `mTileY`) -> `i0`, `i1`
pub const EE_PATH_UNIT_TILE: i32 = 122;
/// unit `h0` `DropPath()`
pub const EE_PATH_UNIT_DROP_PATH: i32 = 123;
/// unit `h0` `TakePath(the points h1, i0)`
pub const EE_PATH_UNIT_TAKE_PATH: i32 = 124;
/// unit `h0`: `mIsPathDone = true`, `mBoundedPathTiles.clear()`
pub const EE_PATH_UNIT_PATH_DONE: i32 = 125;
/// unit `h0` `SnapLastWaypoint()`, then when `i0` its goal's `PathTaken`
pub const EE_PATH_UNIT_SNAP: i32 = 126;
/// whether the capture is on (`EESimCaptureRecord::Active()`)
pub const EE_PATH_CAPTURE_ACTIVE: i32 = 127;
/// a capture record of tag `i0`: the bytes `h1[..i1]`
pub const EE_PATH_CAPTURE_COMMIT: i32 = 128;
/// the points `h2[..i0]` appended to the C++ vector `h1` (`std::vector<GE3DPoint>`)
pub const EE_PATH_POINTS_OUT: i32 = 129;

/// PMSP and its kinds
const PMSP: i64 = 0x5053_4d50;
const KIND_TRIM: u32 = 1;
const KIND_APPEND: u32 = 2;
const KIND_JOIN: u32 = 3;
const KIND_DETOUR: u32 = 4;
const KIND_SHORT_PATH: u32 = 5;
const KIND_FAILED_BOUNDS: u32 = 6;

fn ask(host: &mut dyn PathHost, op: i32, h: &[u64], i: &[i64]) -> (i64, EePathIo) {
    let mut io = EePathIo::default();
    io.h[..h.len()].copy_from_slice(h);
    io.i[..i.len()].copy_from_slice(i);
    let r = host.call(op, &mut io);
    (r, io)
}

fn op(host: &mut dyn PathHost, code: i32, h: &[u64], i: &[i64]) -> i64 {
    ask(host, code, h, i).0
}

/// A point list from the host (`code`'s list in `h1`), appended to `v` (the C++ `GetPoints` push back).
fn points_into(host: &mut dyn PathHost, code: i32, h0: u64, v: &mut Vec<EePathPoint>) {
    op(host, code, &[h0, v as *mut Vec<EePathPoint> as u64], &[]);
}

fn points(host: &mut dyn PathHost, code: i32, h0: u64) -> Vec<EePathPoint> {
    let mut v: Vec<EePathPoint> = Vec::new();
    points_into(host, code, h0, &mut v);
    v
}

fn take_path(host: &mut dyn PathHost, unit: u64, path: &[EePathPoint], flag: bool) {
    op(
        host,
        EE_PATH_UNIT_TAKE_PATH,
        &[unit, path as *const [EePathPoint] as *const EePathPoint as u64],
        &[path.len() as i64, flag as i64],
    );
}

fn tile2(io: &EePathIo) -> (i32, i32) {
    (io.i[0] as i32, io.i[1] as i32)
}

/// A PMSP record (TEST ONLY): kept only while the capture is on.
struct Record {
    on: bool,
    data: Vec<u8>,
}

impl Record {
    fn new(host: &mut dyn PathHost, on: bool) -> Record {
        Record { on: on && op(host, EE_PATH_CAPTURE_ACTIVE, &[], &[]) != 0, data: Vec::new() }
    }
    fn u32(&mut self, v: u32) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    fn i32(&mut self, v: i32) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    fn f32(&mut self, v: f32) {
        self.data.extend_from_slice(&v.to_bits().to_le_bytes());
    }
    /// `EEPathCapturePoints` / `EEPathCaptureWaypoints`: the count, then x, y, z
    fn points(&mut self, p: &[EePathPoint]) {
        self.u32(p.len() as u32);
        for q in p {
            self.f32(q.x);
            self.f32(q.y);
            self.f32(q.z);
        }
    }
    /// `EEPathCaptureBounds`: top, left, bottom, right
    fn bounds(&mut self, b: &EePathBounds) {
        for v in [b.top, b.left, b.bottom, b.right] {
            self.i32(v);
        }
    }
    fn commit(self, host: &mut dyn PathHost) {
        if self.on {
            op(host, EE_PATH_CAPTURE_COMMIT, &[0, self.data.as_ptr() as u64], &[PMSP, self.data.len() as i64]);
        }
    }
}

fn set_bounds(host: &mut dyn PathHost, b: &EePathBounds) {
    op(host, EE_PATH_BM_BOUNDS, &[], &[b.top as i64, b.left as i64, b.bottom as i64, b.right as i64]);
}

/// `TrimPath`: the path cut from its last point on the unit's tile on (the part walked), the unit's path dropped.
pub fn trim_path(host: &mut dyn PathHost, unit: u64, path: &mut Vec<EePathPoint>) -> bool {
    let waypoints = op(host, EE_PATH_UNIT_WAYPOINTS, &[unit], &[]) as usize;
    let (_, io) = ask(host, EE_PATH_UNIT_LOCATION, &[unit], &[]);
    let (x, y) = (f32::from_bits(io.i[0] as u32), f32::from_bits(io.i[1] as u32));
    let mut rec = Record::new(host, true);
    if rec.on {
        rec.u32(KIND_TRIM);
        rec.u32(waypoints as u32);
        rec.f32(x);
        rec.f32(y);
        rec.points(path);
    }
    let mut result = false;
    if let Some(cut) = trim_index(path, waypoints, x, y) {
        result = true;
        path.truncate(cut);
        op(host, EE_PATH_UNIT_DROP_PATH, &[unit], &[]);
    }
    if rec.on {
        rec.u32(result as u32);
        rec.u32(path.len() as u32);
    }
    rec.commit(host);
    result
}

/// `AppendWaypoints`: unless `TrimPath` cuts the path, the unit's waypoints after it, then its path dropped.
pub fn append_unit_waypoints(host: &mut dyn PathHost, unit: u64, path: &mut Vec<EePathPoint>) {
    if unit == 0 {
        return;
    }
    let mut rec = Record::new(host, true);
    if rec.on {
        rec.u32(KIND_APPEND);
        rec.points(path);
        let w = points(host, EE_PATH_UNIT_WAYPOINT_POINTS, unit);
        rec.points(&w);
    }
    if trim_path(host, unit, path) {
        if rec.on {
            rec.u32(1);
        }
        rec.commit(host);
        return;
    }
    let waypoints = points(host, EE_PATH_UNIT_WAYPOINT_POINTS, unit);
    append_waypoints(path, &waypoints);
    if rec.on {
        rec.u32(0);
        rec.points(path);
    }
    rec.commit(host);
    if !path.is_empty() {
        op(host, EE_PATH_UNIT_DROP_PATH, &[unit], &[]);
    }
}

/// `JoinPaths` (its out path always given here): the path up to its point `count` from the end, then the detour.
fn join(
    host: &mut dyn PathHost,
    path: &[EePathPoint],
    detour: &[EePathPoint],
    out: &mut Vec<EePathPoint>,
    count: usize,
) -> bool {
    let mut rec = Record::new(host, true);
    if rec.on {
        rec.u32(KIND_JOIN);
        rec.points(path);
        rec.points(detour);
        rec.i32(count as i32);
    }
    let mut result = false;
    if !detour.is_empty() {
        if let Some(joined) = join_paths(path, detour, count) {
            out.extend(joined);
            result = true;
        }
    }
    if rec.on {
        rec.u32(result as u32);
        rec.points(out);
    }
    rec.commit(host);
    result
}

/// `FinishPath`: a failed path unless the goal accepted the nearest tile and did not take it.
pub fn finish_path(host: &mut dyn PathHost, machine: u64, unit: u64) {
    let flags = op(host, EE_PATH_TM_FLAGS, &[machine], &[]);
    let (found, blocked) = (flags & 1 != 0, flags & 2 != 0);
    if !found || blocked {
        finish_failed_path(host, machine, unit);
    } else {
        // FinishFoundPath
        let path = points(host, EE_PATH_TM_POINTS, machine);
        take_path(host, unit, &path, false);
        op(host, EE_PATH_UNIT_PATH_DONE, &[unit], &[]);
        op(host, EE_PATH_UNIT_SNAP, &[unit], &[0]);
    }
}

/// `FinishFailedPath`: the points found so far, joined through a short bounded detour (50 revs inside 26 tiles)
/// onto the unit's waypoints when one of the last ten points is reached, else with the waypoints appended.
pub fn finish_failed_path(host: &mut dyn PathHost, machine: u64, unit: u64) {
    let mut path = points(host, EE_PATH_TM_POINTS, machine);
    let mut joined: Vec<EePathPoint> = Vec::new();
    op(host, EE_PATH_UNIT_PATH_DONE, &[unit], &[]);
    let waypoints = op(host, EE_PATH_UNIT_WAYPOINTS, &[unit], &[]) as usize;
    if path.len() > waypoints && waypoints > 0 {
        let from = tile2(&ask(host, EE_PATH_UNIT_TILE, &[unit], &[]).1);
        let p = path[path.len() - waypoints];
        let to = (um_floor(p.x), um_floor(p.y));
        let (dx, dy) = (from.0.wrapping_sub(to.0), from.1.wrapping_sub(to.1));
        if dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) <= 2 {
            trim_path(host, unit, &mut path);
        }
    }
    let waypoints = op(host, EE_PATH_UNIT_WAYPOINTS, &[unit], &[]);
    let detour_wanted = waypoints >= 1 && {
        if path.len() > 4 {
            true
        } else if path.is_empty() {
            false
        } else {
            let first = points(host, EE_PATH_UNIT_WAYPOINT_POINTS, unit)[0];
            let t = tile2(&ask(host, EE_PATH_UNIT_TILE, &[unit], &[]).1);
            um_floor(first.x).wrapping_sub(t.0).wrapping_abs() > 1
                || um_floor(first.y).wrapping_sub(t.1).wrapping_abs() > 1
        }
    };
    if !detour_wanted {
        append_unit_waypoints(host, unit, &mut path);
        take_path(host, unit, &path, true);
    } else {
        let map = op(host, crate::astar::pm::EE_PATH_MAP, &[], &[]) as u64;
        op(host, EE_PATH_BM_PREPARE, &[0, map], &[]);
        let count = path.len().min(10);
        let p = path[path.len() - count];
        let to = (um_floor(p.x), um_floor(p.y));
        let from = tile2(&ask(host, EE_PATH_UNIT_TILE, &[unit], &[]).1);
        let size = op(host, EE_PATH_MAP_SIZE, &[0, map], &[]) as i32;
        let bounds = bounds_around(from, 26, size);
        let mut rec = Record::new(host, true);
        if rec.on {
            rec.u32(KIND_FAILED_BOUNDS);
            rec.i32(from.0);
            rec.i32(from.1);
            rec.i32(size);
            rec.bounds(&bounds);
        }
        rec.commit(host);
        set_bounds(host, &bounds);
        op(host, EE_PATH_BM_SET_MAX_REVS, &[], &[50]);
        let kind = op(host, EE_PATH_TM_TYPE, &[machine], &[]);
        op(host, EE_PATH_BM_MODIFIER, &[], &[kind]);
        let mut done = false;
        // one detour list: the goal's GetPoints appends, so a second call after a failed join doubles it, as in
        // the C++
        let mut detour: Vec<EePathPoint> = Vec::new();
        // RunBoundedPath(50)
        if op(host, EE_PATH_BM_RUN_BOUNDED, &[0, unit], &[50, from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64])
            != 0
        {
            points_into(host, EE_PATH_BM_GOAL_POINTS, 0, &mut detour);
            done = join(host, &path, &detour, &mut joined, count);
        }
        if !done && !path.is_empty() {
            points_into(host, EE_PATH_BM_GOAL_POINTS, 0, &mut detour);
            if !detour.is_empty() {
                let index = detour_join_count(&path, &detour);
                let mut rec = Record::new(host, true);
                if rec.on {
                    rec.u32(KIND_DETOUR);
                    rec.points(&path);
                    rec.points(&detour);
                    rec.i32(index.map_or(-1, |i| i as i32));
                }
                rec.commit(host);
                if let Some(index) = index {
                    done = join(host, &path, &detour, &mut joined, index);
                }
            }
            if !done {
                append_unit_waypoints(host, unit, &mut path);
                take_path(host, unit, &path, true);
            }
        }
        if done {
            op(host, EE_PATH_UNIT_DROP_PATH, &[unit], &[]);
            take_path(host, unit, &joined, true);
        }
        op(host, EE_PATH_BM_RESET, &[], &[]);
    }
    op(host, EE_PATH_UNIT_SNAP, &[unit], &[1]);
}

/// `TryShortPath`: a short bounded search toward `to` (a few steps, more with the queue and the distance), its
/// result given to the unit; `to` becomes the tile reached. The goal's took-nearest flag.
pub fn try_short_path(
    host: &mut dyn PathHost,
    unit: u64,
    map: u64,
    kind: i64,
    from: (i32, i32),
    to: &mut (i32, i32),
    queued: i32,
) -> bool {
    let machine = op(host, EE_PATH_BM_PREPARE, &[0, map], &[]) as u64;
    op(host, EE_PATH_BM_MODIFIER, &[], &[kind]);
    op(host, EE_PATH_BM_SET_UNIT, &[0, unit], &[]);
    // (the original's search node held the unit)
    op(host, crate::astar::pm::EE_PATH_UNIT_REFERENCE, &[unit], &[]);
    op(host, EE_PATH_BM_ENDS, &[], &[from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64, 1]);
    let revs = short_path_revs(queued, from, *to);
    op(host, EE_PATH_BM_SET_MAX_REVS, &[], &[revs as i64]);
    let size = op(host, EE_PATH_MAP_SIZE, &[0, map], &[]) as i32;
    let bounds = bounds_around(from, revs.wrapping_add(1), size);
    let mut rec = Record::new(host, true);
    if rec.on {
        rec.u32(KIND_SHORT_PATH);
        for v in [queued, from.0, from.1, to.0, to.1, size, revs] {
            rec.i32(v);
        }
        rec.bounds(&bounds);
    }
    rec.commit(host);
    set_bounds(host, &bounds);
    op(host, EE_PATH_BM_RUN, &[], &[]);
    finish_path(host, machine, unit);
    let result = op(host, EE_PATH_TM_FLAGS, &[machine], &[]) & 2 != 0;
    *to = tile2(&ask(host, EE_PATH_BM_TILE, &[], &[]).1);
    op(host, EE_PATH_UNIT_PATH_DONE, &[unit], &[]);
    op(host, EE_PATH_BM_RESET, &[], &[]);
    op(host, crate::astar::pm::EE_PATH_UNIT_DEREFERENCE, &[unit], &[]);
    result
}

/// `FindPath(unit, map, ...)`: a bounded search of at most 200 steps (a window around `from` when `bounds` is
/// empty) into the C++ vectors `out_path` and `out_visited` (0: none). `bounds` is bottom, left, right, top.
#[allow(clippy::too_many_arguments)]
pub fn find_path(
    host: &mut dyn PathHost,
    unit: u64,
    map: u64,
    from: (i32, i32),
    to: (i32, i32),
    bounds: [i32; 4],
    out_path: u64,
    kind: i64,
    revs: i64,
    modifier: u64,
    out_visited: u64,
) {
    let clamp = |host: &mut dyn PathHost, t: (i32, i32)| {
        tile2(&ask(host, crate::astar::pm::EE_PATH_CLAMP, &[0, map], &[t.0 as i64, t.1 as i64, 1]).1)
    };
    let (raw_from, _) = (from, to);
    let mut b = bounds;
    let mut revs = revs;
    let from = clamp(host, from);
    let to = clamp(host, to);
    op(host, EE_PATH_BM_PREPARE, &[0, map], &[]);
    if modifier != 0 {
        op(host, EE_PATH_BM_SET_MODIFIER, &[0, modifier], &[]);
    } else {
        op(host, EE_PATH_BM_MODIFIER, &[], &[kind]);
    }
    op(host, EE_PATH_BM_SET_UNIT, &[0, unit], &[]);
    op(host, EE_PATH_BM_ENDS, &[], &[from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64, 1]);
    if revs <= 0 {
        if unit != 0 {
            let (_, io) = ask(
                host,
                crate::astar::pm::EE_PATH_PATH_REVS,
                &[unit],
                &[from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64],
            );
            let r = crate::astar::path_splice::path_revs(io.i[0] as i32, io.i[1] != 0, io.i[2] != 0);
            op(host, EE_PATH_BM_SET_MAX_REVS, &[], &[r as i64]);
        } else {
            op(host, EE_PATH_BM_SET_MAX_REVS, &[], &[200]);
        }
        revs = op(host, EE_PATH_BM_MAX_REVS, &[], &[]);
        if revs > 200 {
            revs = 200;
            op(host, EE_PATH_BM_SET_MAX_REVS, &[], &[revs]);
        }
    } else {
        if revs > 200 {
            revs = 200;
        }
        op(host, EE_PATH_BM_SET_MAX_REVS, &[], &[revs]);
    }
    let revs = revs as i32;
    // the window grows from the unclamped start (bottom, left, right, top)
    if b[1] == b[2] || b[0] == b[3] {
        b[3] = raw_from.1.wrapping_sub(revs).wrapping_sub(1);
        if b[3] <= 0 {
            b[3] = 0;
        }
        b[1] = raw_from.0.wrapping_sub(revs).wrapping_sub(1);
        if b[1] <= 0 {
            b[1] = 0;
        }
        let last = (op(host, EE_PATH_MAP_SIZE, &[0, map], &[]) as i32).wrapping_sub(1);
        b[2] = raw_from.0.wrapping_add(1).wrapping_add(revs);
        if b[2] >= last {
            b[2] = last;
        }
        b[0] = raw_from.1.wrapping_add(1).wrapping_add(revs);
        if b[0] >= last {
            b[0] = last;
        }
    }
    set_bounds(host, &EePathBounds { top: b[3], left: b[1], bottom: b[0], right: b[2] });
    op(host, EE_PATH_BM_RUN, &[], &[]);
    op(host, EE_PATH_BM_PATH, &[0, out_path, out_visited], &[]);
    op(host, EE_PATH_BM_RESET, &[], &[]);
}

/// `FindBoundedPath`: a bounded search within `margin` tiles of `from`; when it reaches `to`, its path (start
/// first). Returns the path and whether it reached `to`.
pub fn find_bounded_path(
    host: &mut dyn PathHost,
    unit: u64,
    map: u64,
    kind: i64,
    from: (i32, i32),
    to: (i32, i32),
    margin: i32,
) -> (bool, Vec<EePathPoint>) {
    let clamp = |host: &mut dyn PathHost, t: (i32, i32)| {
        tile2(&ask(host, crate::astar::pm::EE_PATH_CLAMP, &[0, map], &[t.0 as i64, t.1 as i64, 1]).1)
    };
    let raw_from = from;
    let from = clamp(host, from);
    let to = clamp(host, to);
    op(host, EE_PATH_BM_PREPARE, &[0, map], &[]);
    let machine = op(host, EE_PATH_BM_PREPARE, &[0, map], &[]) as u64;
    let last = (op(host, EE_PATH_MAP_SIZE, &[0, map], &[]) as i32).wrapping_sub(1);
    let low = |v: i32| if v.wrapping_sub(margin) > 0 { v.wrapping_sub(margin) } else { 0 };
    let high = |v: i32| if v.wrapping_add(margin) < last { v.wrapping_add(margin) } else { last };
    let bounds =
        EePathBounds { top: low(raw_from.1), left: low(raw_from.0), right: high(raw_from.0), bottom: high(raw_from.1) };
    set_bounds(host, &bounds);
    op(host, EE_PATH_BM_MODIFIER, &[], &[kind]);
    op(
        host,
        EE_PATH_BM_BOUNDED_START,
        &[0, unit],
        &[margin as i64, from.0 as i64, from.1 as i64, to.0 as i64, to.1 as i64],
    );
    op(host, EE_PATH_BM_RUN, &[], &[]);
    let path = points(host, EE_PATH_TM_POINTS, machine);
    let reached = tile2(&ask(host, EE_PATH_BM_TILE, &[], &[]).1) == to;
    let out: Vec<EePathPoint> = if reached { path.iter().rev().copied().collect() } else { Vec::new() };
    op(host, EE_PATH_BM_RELEASE_MODIFIER, &[], &[]);
    op(host, EE_PATH_BM_RESET, &[], &[]);
    (reached, out)
}

/// The points appended to the C++ vector `out` (`std::vector<GE3DPoint>*`).
pub fn points_out(host: &mut dyn PathHost, out: u64, path: &[EePathPoint]) {
    op(host, EE_PATH_POINTS_OUT, &[0, out, path.as_ptr() as u64], &[path.len() as i64]);
}

/// Appends `count` points to a Rust point list (a host operation's list in `h1`).
///
/// # Safety
/// `list` a live `Vec<EePathPoint>` from this module, `points` readable for `count` points.
#[no_mangle]
pub unsafe extern "C" fn ee_path_points_append(list: *mut std::ffi::c_void, points: *const EePathPoint, count: u32) {
    let v = &mut *(list as *mut Vec<EePathPoint>);
    v.extend_from_slice(std::slice::from_raw_parts(points, count as usize));
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A fake host: the capture on, the records kept; the unit's waypoints and location given.
    struct Fake {
        records: Vec<Vec<u8>>,
        waypoints: Vec<EePathPoint>,
        location: (f32, f32),
        dropped: u32,
    }

    impl PathHost for Fake {
        fn call(&mut self, code: i32, io: &mut EePathIo) -> i64 {
            match code {
                EE_PATH_CAPTURE_ACTIVE => 1,
                EE_PATH_CAPTURE_COMMIT => {
                    let bytes = unsafe { std::slice::from_raw_parts(io.h[1] as *const u8, io.i[1] as usize) };
                    self.records.push(bytes.to_vec());
                    0
                }
                EE_PATH_UNIT_WAYPOINTS => self.waypoints.len() as i64,
                EE_PATH_UNIT_WAYPOINT_POINTS => {
                    let v = unsafe { &mut *(io.h[1] as *mut Vec<EePathPoint>) };
                    v.extend_from_slice(&self.waypoints);
                    0
                }
                EE_PATH_UNIT_LOCATION => {
                    io.i[0] = self.location.0.to_bits() as i64;
                    io.i[1] = self.location.1.to_bits() as i64;
                    0
                }
                EE_PATH_UNIT_DROP_PATH => {
                    self.dropped += 1;
                    self.waypoints.clear();
                    0
                }
                _ => 0,
            }
        }
    }

    fn p(x: f32, y: f32) -> EePathPoint {
        EePathPoint { x, y, z: 0.0 }
    }

    fn word(b: &[u8], k: usize) -> u32 {
        u32::from_le_bytes([b[4 * k], b[4 * k + 1], b[4 * k + 2], b[4 * k + 3]])
    }

    /// TrimPath's record: kind, waypoints, location, the path, then the result and the length; the unit's path
    /// dropped when cut.
    #[test]
    fn trim_records_and_cuts() {
        let mut host = Fake { records: Vec::new(), waypoints: vec![p(9.5, 9.5)], location: (2.7, 2.1), dropped: 0 };
        let mut path = vec![p(1.5, 1.5), p(2.5, 2.5), p(2.2, 2.9), p(5.5, 5.5)];
        assert!(trim_path(&mut host, 1, &mut path));
        assert_eq!(path.len(), 2);
        assert_eq!(host.dropped, 1);
        let r = &host.records[0];
        assert_eq!(word(r, 0), KIND_TRIM);
        assert_eq!(word(r, 1), 1);
        assert_eq!(f32::from_bits(word(r, 2)), 2.7);
        assert_eq!(word(r, 4), 4);
        assert_eq!(r.len(), 4 * (5 + 12 + 2));
        assert_eq!(word(r, 17), 1);
        assert_eq!(word(r, 18), 2);
    }

    /// AppendWaypoints: TrimPath's record first, then its own; the waypoints after the path, the unit's path
    /// dropped.
    #[test]
    fn append_after_no_trim() {
        let mut host =
            Fake { records: Vec::new(), waypoints: vec![p(7.5, 7.5), p(8.5, 8.5)], location: (0.5, 0.5), dropped: 0 };
        let mut path = vec![p(1.5, 1.5)];
        append_unit_waypoints(&mut host, 1, &mut path);
        assert_eq!(path, vec![p(1.5, 1.5), p(8.5, 8.5), p(7.5, 7.5)]);
        assert_eq!(host.dropped, 1);
        assert_eq!(host.records.len(), 2);
        assert_eq!(word(&host.records[0], 0), KIND_TRIM);
        let r = &host.records[1];
        assert_eq!(word(r, 0), KIND_APPEND);
        // kind, the path (1 point), the waypoints (2), the trim flag, the path after (3)
        assert_eq!(r.len(), 4 * (1 + 4 + 7 + 1 + 10));
        assert_eq!(word(r, 12), 0);
    }

    /// JoinPaths' record and result.
    #[test]
    fn join_records() {
        let mut host = Fake { records: Vec::new(), waypoints: Vec::new(), location: (0.0, 0.0), dropped: 0 };
        let path = [p(0.5, 0.5), p(1.5, 1.5), p(2.5, 2.5)];
        let mut out = Vec::new();
        assert!(join(&mut host, &path, &[p(1.9, 1.1), p(4.5, 4.5)], &mut out, 2));
        assert_eq!(out, vec![p(0.5, 0.5), p(1.9, 1.1), p(4.5, 4.5)]);
        let r = &host.records[0];
        assert_eq!(word(r, 0), KIND_JOIN);
        assert_eq!(word(r, 1 + 10 + 7), 2);
        assert_eq!(word(r, 1 + 10 + 7 + 1), 1);
        assert!(!join(&mut host, &path, &[], &mut Vec::new(), 1));
    }
}
