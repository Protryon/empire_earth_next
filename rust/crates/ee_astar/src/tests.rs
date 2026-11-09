//! The storage's members and the machine over small worlds (no game): the orders, the tie-breaking and the
//! quirks the port keeps. The C++ equality is the capture gate's (tools/sim_capture.py); these pin the behaviour
//! in `cargo test`.

use crate::ffi::{self, EeAStarCallbacks, EeAStarNodeView};
use crate::machine::{self, Host, MachineState};
use crate::storage::*;
use std::ffi::c_void;

fn hashed(w: i32) -> Storage {
    let mut s = Storage::new(StorageKind::Hashed);
    s.init(w, w, GridKind::Hashed);
    s
}

fn open_node(s: &mut Storage, x: i32, y: i32, cost: u32) -> i32 {
    let n = s.new_node(x, y);
    s.nodes[n as usize].cost = cost;
    s.add_open(n);
    n
}

fn tiles(s: &Storage, list: List) -> Vec<(i32, i32)> {
    s.list(list).iter().map(|&n| (s.node(n).x, s.node(n).y)).collect()
}

#[test]
fn bucket_is_the_cpp_hash() {
    let s = hashed(8);
    assert_eq!(s.bucket(0, 0), 0);
    assert_eq!(s.bucket(1, 2), 18);
    assert_eq!(s.bucket(10, 7), (160 + 7) % 53);
    // the high nibble's fold replaces the hash (0x7000_0000 >> 24 ^ 0x7000_0000), then the C remainder
    assert_eq!(s.bucket(0x0700_0000, 0), ((0x7000_0000i32 >> 24) ^ 0x7000_0000) % 53);
    // a negative hash: its high nibble is set, so the fold applies (an arithmetic shift, as the C++'s long)
    let high = -16i32 & (0xf000_0000u32 as i32);
    assert_eq!(s.bucket(-1, 0), ((high >> 24) ^ high) % 53);
    assert_eq!(s.bucket(-1, 0), 0x0fff_fff0 % 53);
}

#[test]
fn free_list_reuses_the_last_released_node_cleared() {
    let mut s = hashed(4);
    let a = s.new_node(1, 1);
    let b = s.new_node(2, 2);
    s.nodes[a as usize].cost = 7;
    s.nodes[a as usize].parent = b;
    s.release_node(b);
    s.release_node(a);
    let c = s.new_node(3, 3);
    assert_eq!(c, a);
    assert_eq!(*s.node(c), Node { parent: NIL, next: NIL, prev: NIL, cost: 0, path_cost: 0, estimate: 0, x: 3, y: 3 });
    assert_eq!(s.new_node(0, 0), b);
    assert_eq!(s.new_node(0, 1), 2);
}

#[test]
fn hashed_open_goes_to_buckets_until_the_sorted_list_holds_nodes() {
    let mut s = hashed(8);
    // empty sorted list: every node lands in its bucket
    open_node(&mut s, 1, 1, 30);
    open_node(&mut s, 2, 2, 10);
    open_node(&mut s, 3, 3, 20);
    assert_eq!(tiles(&s, List::SortedOpen), vec![]);
    assert_eq!(s.tile_flags(2, 2) & TILE_OPEN, TILE_OPEN);
    // the best removal moves up to 15 cheapest bucket nodes into the sorted list, then takes the cheapest
    let best = s.remove_best_open();
    assert_eq!((s.node(best).x, s.node(best).y), (2, 2));
    assert_eq!(s.tile_flags(2, 2) & TILE_OPEN, 0);
    assert_eq!(tiles(&s, List::SortedOpen), vec![(3, 3), (1, 1)]);
    assert_eq!(tiles(&s, List::OpenTable), vec![]);
    // a node no dearer than the tail joins the sorted list before the first node costing at least as much
    open_node(&mut s, 4, 4, 20);
    assert_eq!(tiles(&s, List::SortedOpen), vec![(4, 4), (3, 3), (1, 1)]);
    // a dearer one goes to its bucket
    open_node(&mut s, 5, 5, 99);
    assert_eq!(tiles(&s, List::SortedOpen), vec![(4, 4), (3, 3), (1, 1)]);
    assert_eq!(tiles(&s, List::OpenTable), vec![(5, 5)]);
    assert_eq!(s.find_open(5, 5), s.list(List::OpenTable)[0]);
    assert_eq!(s.find_open(3, 3), s.list(List::SortedOpen)[1]);
    assert_eq!(s.find_open(6, 6), NIL);
}

#[test]
fn hashed_best_open_keeps_fifteen_and_ties_in_bucket_order() {
    let mut s = hashed(64);
    // 20 nodes, one per bucket ((0, y): bucket y), costs 100 - y: the 15 cheapest are y 5..19
    for y in 0..20 {
        open_node(&mut s, 0, y, 100 - y as u32);
    }
    let first = s.remove_best_open();
    assert_eq!(s.node(first).y, 19);
    let sorted: Vec<i32> = s.list(List::SortedOpen).iter().map(|&n| s.node(n).y).collect();
    assert_eq!(sorted, (5..19).rev().collect::<Vec<_>>());
    assert_eq!(s.list(List::OpenTable).len(), 5);
}

#[test]
fn update_open_refiles_a_node() {
    let mut s = hashed(8);
    let a = open_node(&mut s, 1, 0, 10);
    open_node(&mut s, 2, 0, 20);
    open_node(&mut s, 3, 0, 30);
    let _ = s.remove_best_open(); // (1, 0); sorted: (2,0) (3,0)
    let _ = a;
    let c = s.find_open(3, 0);
    s.nodes[c as usize].cost = 5;
    s.update_open(c);
    assert_eq!(tiles(&s, List::SortedOpen), vec![(3, 0), (2, 0)]);
    let b = s.find_open(2, 0);
    s.nodes[b as usize].cost = 50;
    s.update_open(b);
    // dearer than the new tail: back to its bucket
    assert_eq!(tiles(&s, List::SortedOpen), vec![(3, 0)]);
    assert_eq!(tiles(&s, List::OpenTable), vec![(2, 0)]);
}

#[test]
fn closed_buckets_and_nearest_removals() {
    let mut s = hashed(16);
    for (x, y) in [(1, 1), (5, 5), (9, 2)] {
        let n = s.new_node(x, y);
        s.add_closed(n);
        assert_eq!(s.tile_flags(x, y) & TILE_CLOSED, TILE_CLOSED);
    }
    assert_eq!(s.find_closed(5, 5), 1);
    let near = s.remove_nearest_closed(8, 3);
    assert_eq!((s.node(near).x, s.node(near).y), (9, 2));
    assert_eq!(s.tile_flags(9, 2) & TILE_CLOSED, 0);
    // equal distances: the first one met wins (the buckets are walked last bucket first)
    let near = s.remove_nearest_closed(3, 3);
    assert_eq!((s.node(near).x, s.node(near).y), (5, 5));
    s.remove_closed(s.find_closed(1, 1));
    assert_eq!(tiles(&s, List::ClosedTable), vec![]);
    open_node(&mut s, 7, 7, 3);
    let n = s.remove_nearest_open(0, 0);
    assert_eq!((s.node(n).x, s.node(n).y), (7, 7));
    assert!(!s.has_open());
}

#[test]
fn terrain_open_is_a_stack_with_a_node_map() {
    let mut s = Storage::new(StorageKind::Terrain);
    s.init(4, 4, GridKind::Dirty);
    for x in 0..3 {
        let n = s.new_node(x, 0);
        s.add_open(n);
    }
    assert_eq!(tiles(&s, List::FastOpen), vec![(2, 0), (1, 0), (0, 0)]);
    assert_eq!(s.find_open(1, 0), s.list(List::FastOpen)[1]);
    let n = s.remove_best_open();
    assert_eq!((s.node(n).x, s.node(n).y), (2, 0));
    assert_eq!(s.find_open(2, 0), NIL);
    s.add_closed(n);
    assert_eq!(s.find_closed(2, 0), n);
    s.remove_closed(n);
    assert_eq!(s.find_closed(2, 0), NIL);
    assert!(s.has_open());
}

#[test]
fn bounded_keeps_its_node_map_in_step() {
    let mut s = Storage::new(StorageKind::Bounded);
    s.init(8, 8, GridKind::Bounded);
    let a = open_node(&mut s, 3, 3, 5);
    assert_eq!(s.find_open(3, 3), a);
    let b = s.remove_best_open();
    assert_eq!(b, a);
    assert_eq!(s.find_open(3, 3), NIL);
    s.add_closed(b);
    assert_eq!(tiles(&s, List::FastClosed), vec![(3, 3)]);
    assert_eq!(s.find_closed(3, 3), b);
}

#[test]
fn prepare_grid_clears_only_the_last_bounds_grown_by_two() {
    let mut s = hashed(16);
    s.mark_tile(5, 6, false);
    s.mark_tile(7, 6, true);
    assert_eq!(s.bounds, AStarRect { bottom: 6, left: 5, right: 7, top: 6 });
    assert!(s.has_bounds);
    // a stale flag outside the bounds survives the next Init
    s.flags.as_mut().unwrap()[(15 * 16 + 15) as usize] = TILE_REJECTED;
    s.init(16, 16, GridKind::Hashed);
    assert_eq!(s.tile_flags(5, 6), 0);
    assert_eq!(s.tile_flags(7, 6), 0);
    assert_eq!(s.tile_flags(15, 15), TILE_REJECTED);
    assert!(!s.has_bounds);
    assert_eq!(s.bounds, AStarRect::default());
    // marks off the grid are ignored
    s.mark_tile(16, 0, true);
    assert!(!s.has_bounds);
}

#[test]
fn dirty_prepare_grid_clears_the_dirty_bounds_grown_by_one_but_not_their_last_tile() {
    let mut s = Storage::new(StorageKind::Terrain);
    s.init(8, 8, GridKind::Dirty);
    s.flags.as_mut().unwrap().fill(TILE_REJECTED);
    s.dirty = AStarRect { bottom: 3, left: 2, right: 3, top: 2 };
    s.init(8, 8, GridKind::Dirty);
    // clears the span (1, 1) .. (4, 4), the end exclusive as the C++ count
    let f = |s: &Storage, x, y| s.tile_flags(x, y);
    assert_eq!(f(&s, 1, 0), TILE_REJECTED);
    assert_eq!(f(&s, 1, 1), 0);
    assert_eq!(f(&s, 2, 7), 0);
    assert_eq!(f(&s, 4, 3), 0);
    assert_eq!(f(&s, 4, 4), TILE_REJECTED);
    assert_eq!(s.dirty, AStarRect::default());
    // a resize (the C++ arrays just allocated) clears everything
    s.flags.as_mut().unwrap().fill(TILE_REJECTED);
    s.dirty = AStarRect { bottom: 3, left: 2, right: 3, top: 2 };
    s.init_begin(8, 8);
    s.prepare_grid(8, 8, GridKind::Dirty, true);
    assert!(s.flags.as_ref().unwrap().iter().all(|&f| f == 0));
}

#[test]
fn path_tiles_walk_from_the_found_node_to_the_start() {
    let mut s = hashed(8);
    let a = s.new_node(0, 0);
    let b = s.new_node(1, 0);
    let c = s.new_node(2, 1);
    s.nodes[b as usize].parent = a;
    s.nodes[c as usize].parent = b;
    s.path = c;
    let mut out = vec![s.first_path_tile().unwrap()];
    while let Some(t) = s.next_path_tile() {
        out.push(t);
    }
    assert_eq!(out, vec![(2, 1), (1, 0), (0, 0)]);
    // a one-node path
    s.path = a;
    assert_eq!(s.first_path_tile(), Some((0, 0)));
    assert_eq!(s.next_path_tile(), None);
    s.path = NIL;
    assert_eq!(s.first_path_tile(), None);
}

#[test]
fn clear_returns_every_list_to_the_free_list() {
    let mut s = hashed(8);
    open_node(&mut s, 1, 1, 1);
    open_node(&mut s, 2, 2, 2);
    let c = s.new_node(3, 3);
    s.add_closed(c);
    s.current = s.new_node(4, 4);
    s.reset_nodes();
    assert_eq!(s.nodes.len(), 4);
    let mut free = Vec::new();
    let mut n = s.free;
    while n != NIL {
        free.push(n);
        n = s.node(n).next;
    }
    free.sort();
    assert_eq!(free, vec![0, 1, 2, 3]);
    assert!(s.open_table.is_none() && s.closed_table.is_none());
}

/// A grid world: `#` blocks; the path goal of `EEAIAStarPathfindingGoal` without a modifier (10 straight, 14
/// diagonal, Manhattan x 10), or a flood fill over the open tiles.
struct World {
    rows: Vec<&'static [u8]>,
    flood: bool,
    start: (i32, i32),
    goal: (i32, i32),
    state: MachineState,
    max_revs: i32,
    failed: bool,
    accepts: Vec<(i32, i32, bool)>,
    count: i32,
}

impl World {
    fn new(rows: Vec<&'static [u8]>, flood: bool, start: (i32, i32), goal: (i32, i32)) -> World {
        let n = rows.len() as i32;
        World {
            rows,
            flood,
            start,
            goal,
            state: MachineState { map_width: n, map_height: n, check_corners: !flood, ..Default::default() },
            max_revs: -1,
            failed: false,
            accepts: Vec::new(),
            count: 0,
        }
    }
    fn open(&self, x: i32, y: i32) -> bool {
        x >= 0
            && y >= 0
            && (y as usize) < self.rows.len()
            && (x as usize) < self.rows[0].len()
            && self.rows[y as usize][x as usize] != b'#'
    }
}

impl Host for World {
    fn accept_tile(&mut self, x: i32, y: i32, flag: bool) -> bool {
        self.accepts.push((x, y, flag));
        let ok = self.open(x, y);
        if ok {
            self.count += 1;
        }
        ok
    }
    fn get_cost(&mut self, fx: i32, fy: i32, x: i32, y: i32, _parent: Option<(i32, i32)>) -> i32 {
        if self.flood {
            1
        } else if fx == x || fy == y {
            10
        } else {
            14
        }
    }
    fn estimate(&mut self, fx: i32, fy: i32, tx: i32, ty: i32) -> i32 {
        if self.flood {
            1
        } else {
            ((fx - tx).abs() + (fy - ty).abs()) * 10
        }
    }
    fn is_goal_node(&mut self, node: Option<(i32, i32)>) -> bool {
        match node {
            None => true,
            Some(t) => !self.flood && t == self.goal,
        }
    }
    fn search_done(&mut self, current: Option<(i32, i32)>, has_open: bool) {
        if current.is_none() && !has_open && !self.flood {
            self.failed = true;
        }
    }
    fn search_failed(&mut self) -> bool {
        self.failed
    }
    fn accept_nearest(&mut self) -> bool {
        true
    }
    fn is_step_limit(&mut self, _steps: i32) -> bool {
        false
    }
    fn is_rev_limit(&mut self, revs: i32) -> bool {
        self.max_revs != -1 && revs >= self.max_revs
    }
    fn is_cheaper(&mut self, cost: u32, old: u32) -> bool {
        !self.flood && old > cost
    }
    fn start_tile(&mut self) -> (i32, i32) {
        self.start
    }
    fn goal_tile(&mut self) -> (i32, i32) {
        self.goal
    }
    fn state(&mut self) -> MachineState {
        self.state
    }
    fn set_state(&mut self, state: &MachineState) {
        self.state = *state;
    }
}

fn path(s: &mut Storage) -> Vec<(i32, i32)> {
    let mut out = Vec::new();
    if let Some(t) = s.first_path_tile() {
        out.push(t);
        while let Some(t) = s.next_path_tile() {
            out.push(t);
        }
    }
    out
}

fn run(s: &mut Storage, w: &mut World, grid: GridKind) {
    s.init(w.state.map_width, w.state.map_height, grid);
    w.state.revs = 0;
    w.state.num_runs = 0;
    machine::start(s, w);
    machine::expand(s, w);
}

const MAZE: [&[u8]; 6] = [b"......", b".####.", b".#....", b".#.##.", b"...#..", b"......"];

#[test]
fn machine_finds_the_cheapest_path_around_walls() {
    let mut s = Storage::new(StorageKind::Hashed);
    let mut w = World::new(MAZE.to_vec(), false, (0, 0), (2, 3));
    run(&mut s, &mut w, GridKind::Hashed);
    let p = path(&mut s);
    assert_eq!(p.first(), Some(&(2, 3)));
    assert_eq!(p.last(), Some(&(0, 0)));
    assert!(!w.state.found && !w.failed);
    // every step is a neighbour on an open tile
    for pair in p.windows(2) {
        assert!((pair[0].0 - pair[1].0).abs() <= 1 && (pair[0].1 - pair[1].1).abs() <= 1);
        assert!(w.open(pair[0].0, pair[0].1));
    }
    // the cost: down the left column to (0, 4), then (1, 5)? no: (0,0)->(0,3) 30, (1,4) 14, (2,3) 14 = 58
    let cost: i32 = p.windows(2).map(|q| if q[0].0 == q[1].0 || q[0].1 == q[1].1 { 10 } else { 14 }).sum();
    assert_eq!(cost, 58);
    // TestTile asked the goal once per tile: the cached verdicts answered the rest
    let mut asked: Vec<(i32, i32)> = w.accepts.iter().map(|a| (a.0, a.1)).collect();
    let n = asked.len();
    asked.sort();
    asked.dedup();
    assert_eq!(asked.len(), n);
}

#[test]
fn machine_without_a_way_takes_the_nearest_node() {
    let rows: Vec<&'static [u8]> = vec![b"..#..", b"..#..", b"###..", b".....", b"....."];
    let mut s = Storage::new(StorageKind::Hashed);
    let mut w = World::new(rows, false, (0, 0), (4, 0));
    run(&mut s, &mut w, GridKind::Hashed);
    assert!(w.failed);
    // Restart: the open or closed node nearest the goal tile, the found path from there
    let p = path(&mut s);
    assert_eq!(p.first(), Some(&(1, 0)));
    assert_eq!(p.last(), Some(&(0, 0)));
    assert_eq!((w.state.tile_x, w.state.tile_y), (1, 0));
}

#[test]
fn machine_stops_at_the_rev_limit() {
    let mut s = Storage::new(StorageKind::Hashed);
    let mut w = World::new(MAZE.to_vec(), false, (0, 0), (2, 3));
    w.max_revs = 3;
    run(&mut s, &mut w, GridKind::Hashed);
    assert_eq!(w.state.revs, 3);
    assert_eq!(w.state.steps, 2);
    assert!(!w.state.found);
}

#[test]
fn flood_fill_takes_every_reachable_tile_once() {
    let mut s = Storage::new(StorageKind::Terrain);
    let mut w = World::new(MAZE.to_vec(), true, (0, 0), (6, 6));
    run(&mut s, &mut w, GridKind::Dirty);
    let open_tiles: usize = MAZE.iter().map(|r| r.iter().filter(|&&c| c != b'#').count()).sum();
    let mut all = tiles(&s, List::FastClosed);
    all.extend(tiles(&s, List::FastOpen));
    assert!(tiles(&s, List::FastOpen).is_empty());
    let n = all.len();
    all.sort();
    all.dedup();
    assert_eq!(all.len(), n);
    assert_eq!(n, open_tiles);
    assert_eq!(w.count as usize, open_tiles);
    // the closed list's front is the last tile expanded, the start is at its end
    assert_eq!(*tiles(&s, List::FastClosed).last().unwrap(), (0, 0));
}

#[test]
fn flood_without_corners_uses_the_diagonal_tests_first() {
    // corner checks off: the diagonal tiles are tested before their straight neighbours decide anything
    let rows: Vec<&'static [u8]> = vec![b".#", b"#."];
    let mut s = Storage::new(StorageKind::Terrain);
    let mut w = World::new(rows, true, (0, 0), (2, 2));
    w.state.check_corners = false;
    run(&mut s, &mut w, GridKind::Dirty);
    // (1, 1) is reached through the diagonal
    assert_eq!(tiles(&s, List::FastClosed).len(), 2);
    // with corner checks it is not (both straight neighbours are walls)
    let mut s = Storage::new(StorageKind::Terrain);
    let mut w = World::new(vec![b".#", b"#."], true, (0, 0), (2, 2));
    w.state.check_corners = true;
    run(&mut s, &mut w, GridKind::Dirty);
    assert_eq!(tiles(&s, List::FastClosed).len(), 1);
}

// ---- the C ABI ----

extern "C" fn cb_accept(ctx: *mut c_void, x: i32, y: i32, flag: bool) -> bool {
    unsafe { &mut *(ctx as *mut World) }.accept_tile(x, y, flag)
}
extern "C" fn cb_cost(ctx: *mut c_void, fx: i32, fy: i32, x: i32, y: i32, hp: bool, px: i32, py: i32) -> i32 {
    unsafe { &mut *(ctx as *mut World) }.get_cost(fx, fy, x, y, hp.then_some((px, py)))
}
extern "C" fn cb_estimate(ctx: *mut c_void, fx: i32, fy: i32, tx: i32, ty: i32) -> i32 {
    unsafe { &mut *(ctx as *mut World) }.estimate(fx, fy, tx, ty)
}
extern "C" fn cb_goal(ctx: *mut c_void, has: bool, x: i32, y: i32) -> bool {
    unsafe { &mut *(ctx as *mut World) }.is_goal_node(has.then_some((x, y)))
}
extern "C" fn cb_done(ctx: *mut c_void, has: bool, x: i32, y: i32, open: bool) {
    unsafe { &mut *(ctx as *mut World) }.search_done(has.then_some((x, y)), open)
}
extern "C" fn cb_failed(ctx: *mut c_void) -> bool {
    unsafe { &mut *(ctx as *mut World) }.search_failed()
}
extern "C" fn cb_nearest(ctx: *mut c_void) -> bool {
    unsafe { &mut *(ctx as *mut World) }.accept_nearest()
}
extern "C" fn cb_step(ctx: *mut c_void, n: i32) -> bool {
    unsafe { &mut *(ctx as *mut World) }.is_step_limit(n)
}
extern "C" fn cb_rev(ctx: *mut c_void, n: i32) -> bool {
    unsafe { &mut *(ctx as *mut World) }.is_rev_limit(n)
}
extern "C" fn cb_cheaper(ctx: *mut c_void, c: u32, o: u32) -> bool {
    unsafe { &mut *(ctx as *mut World) }.is_cheaper(c, o)
}
extern "C" fn cb_tiles(ctx: *mut c_void, out: *mut i32) {
    let w = unsafe { &mut *(ctx as *mut World) };
    let t = [w.start.0, w.start.1, w.goal.0, w.goal.1];
    unsafe { std::ptr::copy_nonoverlapping(t.as_ptr(), out, 4) };
}
extern "C" fn cb_get(ctx: *mut c_void, out: *mut MachineState) {
    unsafe { *out = (*(ctx as *mut World)).state };
}
extern "C" fn cb_set(ctx: *mut c_void, s: *const MachineState) {
    unsafe { (*(ctx as *mut World)).state = *s };
}

#[test]
fn c_abi_runs_the_same_search() {
    let mut w = World::new(MAZE.to_vec(), false, (0, 0), (2, 3));
    let cb = EeAStarCallbacks {
        ctx: &mut w as *mut World as *mut c_void,
        accept_tile: cb_accept,
        get_cost: cb_cost,
        estimate: cb_estimate,
        is_goal_node: cb_goal,
        search_done: cb_done,
        search_failed: cb_failed,
        accept_nearest: cb_nearest,
        is_step_limit: cb_step,
        is_rev_limit: cb_rev,
        is_cheaper: cb_cheaper,
        tiles: cb_tiles,
        get_state: cb_get,
        set_state: cb_set,
    };
    unsafe {
        let s = ffi::ee_astar_new(StorageKind::Hashed);
        ffi::ee_astar_init_begin(s, 6, 6);
        ffi::ee_astar_prepare_grid(s, 6, 6, GridKind::Hashed, false);
        ffi::ee_astar_alloc_tables(s);
        ffi::ee_astar_start(s, &cb);
        ffi::ee_astar_expand(s, &cb);
        let mut out = Vec::new();
        let (mut x, mut y) = (0, 0);
        if ffi::ee_astar_first_path_tile(s, &mut x, &mut y) {
            out.push((x, y));
            while ffi::ee_astar_next_path_tile(s, &mut x, &mut y) {
                out.push((x, y));
            }
        }
        assert_eq!(out.first(), Some(&(2, 3)));
        assert_eq!(out.last(), Some(&(0, 0)));
        let mut node = EeAStarNodeView::default();
        assert!(ffi::ee_astar_node(s, 1, &mut node));
        assert_eq!((node.x, node.y), (2, 3));
        assert!(node.has_parent);
        let n = ffi::ee_astar_list(s, List::ClosedTable, std::ptr::null_mut(), 0);
        let mut views = vec![EeAStarNodeView::default(); n as usize];
        assert_eq!(ffi::ee_astar_list(s, List::ClosedTable, views.as_mut_ptr(), n), n);
        assert!(views.iter().any(|v| (v.x, v.y) == (0, 0) && !v.has_parent));
        ffi::ee_astar_free(s);
    }
}

// ---- the flood goals' tile tests (goals.rs) ----

use crate::goals::*;

/// A map of terrains (`.` land, `~` deep water, `#` cliff, `*` space), heights and continents; its edge is the
/// outer ring; it records every query in order.
struct MapView {
    rows: Vec<&'static [u8]>,
    levels: Vec<Vec<i32>>,
    continents: Vec<Vec<u16>>,
    chokes: Vec<Option<u32>>,
    queries: Vec<String>,
}

impl MapView {
    fn new(rows: Vec<&'static [u8]>) -> MapView {
        let n = rows.len();
        MapView {
            rows,
            levels: vec![vec![0; n]; n],
            continents: vec![vec![1; n]; n],
            chokes: vec![None; n * n],
            queries: Vec::new(),
        }
    }
    fn n(&self) -> i32 {
        self.rows.len() as i32
    }
}

impl View for MapView {
    fn has_tile(&mut self, x: i32, y: i32) -> bool {
        self.queries.push(format!("has {x} {y}"));
        x >= 0 && y >= 0 && x < self.n() && y < self.n()
    }
    fn terrain(&mut self, x: i32, y: i32) -> i32 {
        self.queries.push(format!("terrain {x} {y}"));
        match self.rows[y as usize][x as usize] {
            b'~' => TERRAIN_DEEP_WATER,
            b'#' => TERRAIN_CLIFF,
            b'*' => TERRAIN_SPACE,
            _ => 0,
        }
    }
    fn is_edge(&mut self, x: i32, y: i32) -> bool {
        self.queries.push(format!("edge {x} {y}"));
        x == 0 || y == 0 || x == self.n() - 1 || y == self.n() - 1
    }
    fn height_level(&mut self, x: i32, y: i32) -> i32 {
        self.queries.push(format!("level {x} {y}"));
        self.levels[y as usize][x as usize]
    }
    fn continent_id(&mut self, x: i32, y: i32) -> u16 {
        self.queries.push(format!("continent {x} {y}"));
        self.continents[y as usize][x as usize]
    }
    fn choke_tile(&mut self, index: i32) -> Option<u32> {
        self.queries.push(format!("choke {index}"));
        self.chokes[index as usize]
    }
}

const ISLES: [&[u8]; 5] = [b".....", b".~~#.", b".~*..", b".....", b"....."];

#[test]
fn continent_first_tile_picks_the_kind_then_matches_it() {
    let mut v = MapView::new(ISLES.to_vec());
    let mut fill = EeAStarContinentFill::default();
    assert!(continent_accept(&mut fill, &mut v, 1, 1));
    assert_eq!(fill, EeAStarContinentFill { kind: CATEGORY_WATER, tile_count: 1 });
    assert_eq!(v.queries, ["has 1 1", "terrain 1 1", "edge 1 1", "edge 1 1"]);
    assert!(continent_accept(&mut fill, &mut v, 2, 1));
    assert!(!continent_accept(&mut fill, &mut v, 3, 1)); // cliff
    assert!(!continent_accept(&mut fill, &mut v, 9, 9)); // off the map
    assert_eq!(fill.tile_count, 2);
    // a rejected terrain is not asked about the edge
    v.queries.clear();
    assert!(!continent_accept(&mut fill, &mut v, 2, 2));
    assert_eq!(v.queries, ["has 2 2", "terrain 2 2"]);
}

#[test]
fn continent_edge_tiles_make_their_own_continents() {
    let mut v = MapView::new(ISLES.to_vec());
    let mut fill = EeAStarContinentFill::default();
    assert!(continent_accept(&mut fill, &mut v, 0, 0));
    assert_eq!(fill.kind, CATEGORY_MAP_EDGE);
    assert!(continent_accept(&mut fill, &mut v, 4, 2));
    assert!(!continent_accept(&mut fill, &mut v, 3, 3)); // land, not an edge
    let mut land = EeAStarContinentFill::default();
    assert!(continent_accept(&mut land, &mut v, 3, 3));
    assert_eq!(land.kind, CATEGORY_LAND);
    assert!(!continent_accept(&mut land, &mut v, 4, 3)); // land on the edge
    assert!(!continent_accept(&mut land, &mut v, 2, 2)); // space
    let mut space = EeAStarContinentFill::default();
    assert!(continent_accept(&mut space, &mut v, 2, 2));
    assert_eq!(space.kind, CATEGORY_SPACE);
    // an unknown kind matches nothing
    let mut odd = EeAStarContinentFill { kind: 9, tile_count: 0 };
    assert!(!continent_accept(&mut odd, &mut v, 3, 3));
}

#[test]
fn hill_takes_high_tiles_of_its_first_continent() {
    let mut v = MapView::new(ISLES.to_vec());
    v.levels[3][3] = 4;
    v.levels[3][2] = 6;
    v.levels[3][1] = 3;
    v.levels[1][3] = 9; // a cliff
    v.continents[3][2] = 2;
    let mut fill = EeAStarHillFill { continent_id: 0, tile_count: 0, level_sum: 100 };
    assert!(hill_accept(&mut fill, &mut v, 3, 3));
    assert_eq!(fill, EeAStarHillFill { continent_id: 1, tile_count: 1, level_sum: 104 });
    assert!(!hill_accept(&mut fill, &mut v, 2, 3)); // another continent
    assert!(!hill_accept(&mut fill, &mut v, 1, 3)); // too low
    assert!(!hill_accept(&mut fill, &mut v, 3, 1)); // cliff
    assert!(!hill_accept(&mut fill, &mut v, -1, 0));
    assert_eq!(fill.tile_count, 1);
    // the level is asked before the terrain; the continent only for a high, non-cliff tile
    v.queries.clear();
    hill_accept(&mut fill, &mut v, 1, 3);
    assert_eq!(v.queries, ["has 1 3", "level 1 3", "terrain 1 3"]);
}

#[test]
fn choke_takes_flagged_tiles_with_their_serials() {
    let mut v = MapView::new(ISLES.to_vec());
    v.chokes[5 * 2 + 3] = Some(77);
    let mut fill = EeAStarChokeFill { width: 5, height: 5, tile_count: 0 };
    assert_eq!(choke_accept(&mut fill, &mut v, 2, 3), Some(77));
    assert_eq!(fill.tile_count, 1);
    assert_eq!(choke_accept(&mut fill, &mut v, 3, 2), None);
    // off the grid: no query
    v.queries.clear();
    assert_eq!(choke_accept(&mut fill, &mut v, 5, 0), None);
    assert_eq!(choke_accept(&mut fill, &mut v, 0, -1), None);
    assert!(v.queries.is_empty());
}
