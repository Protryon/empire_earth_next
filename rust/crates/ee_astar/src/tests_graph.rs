//! The graph storage's members and the graph machine over small graphs (no game): the orders, the tie-breaking and
//! the quirks the port keeps. The C++ equality is the capture gate's (tools/sim_capture.py, the ASGR records);
//! these pin the behaviour in `cargo test`.

use crate::graph::*;
use crate::storage::{TILE_CLOSED, TILE_OPEN};

fn hashed(count: i32) -> GraphStorage {
    let mut s = GraphStorage::new(GraphKind::Hashed);
    assert!(s.init(count));
    s
}

fn terrain(count: i32) -> GraphStorage {
    let mut s = GraphStorage::new(GraphKind::Terrain);
    assert!(s.init(count));
    s
}

/// A node for the object `index` (its handle 100 + index) at the tile, opened at the cost.
fn open_node(s: &mut GraphStorage, x: i32, y: i32, index: i32, cost: u32) -> i32 {
    let n = s.new_node(x, y, 100 + index as usize, index);
    s.nodes[n as usize].cost = cost;
    s.add_open(n);
    n
}

fn indices(s: &GraphStorage, list: GraphList) -> Vec<i32> {
    s.list(list).iter().map(|&n| s.node(n).index).collect()
}

#[test]
fn bucket_is_the_cpp_hash_over_27_buckets() {
    let s = hashed(8);
    assert_eq!(s.bucket(0, 0), 0);
    assert_eq!(s.bucket(1, 2), 18);
    assert_eq!(s.bucket(5, 0), 80 % 27);
    assert_eq!(s.bucket(0x0700_0000, 0), ((0x7000_0000i32 >> 24) ^ 0x7000_0000) % 27);
    assert_eq!(s.bucket(-1, 0), 0x0fff_fff0 % 27);
}

#[test]
fn new_node_reuses_the_last_released_node_cleared() {
    let mut s = hashed(4);
    let a = s.new_node(1, 1, 101, 1);
    let b = s.new_node(2, 2, 102, 2);
    s.nodes[a as usize].cost = 7;
    s.nodes[a as usize].parent = b;
    s.release_node(b);
    s.release_node(a);
    let c = s.new_node(3, 3, 103, 3);
    assert_eq!(c, a);
    assert_eq!(
        *s.node(c),
        GraphNode {
            object: 103,
            parent: NIL,
            next: NIL,
            prev: NIL,
            index: 3,
            cost: 0,
            path_cost: 0,
            estimate: 0,
            x: 3,
            y: 3
        }
    );
    assert_eq!(s.new_node(0, 0, 100, 0), b);
    assert_eq!(s.new_node(0, 1, 100, 0), 2);
}

#[test]
fn init_sizes_the_arrays_and_clears_the_lists() {
    let mut s = hashed(3);
    let o = open_node(&mut s, 1, 1, 1, 10);
    let c = s.new_node(2, 2, 102, 2);
    s.add_closed(c);
    assert_eq!(s.flags_at(1), TILE_OPEN);
    assert_eq!(s.flags_at(2), TILE_CLOSED);
    assert!(s.init(5));
    assert_eq!(s.count, 5);
    assert_eq!(s.old_count, 3);
    assert_eq!(s.flags.as_ref().unwrap().len(), 5);
    assert_eq!(s.flags_at(1), 0);
    assert_eq!(s.open_node(1), NIL);
    assert_eq!(s.closed_node(2), NIL);
    assert!(s.list(GraphList::OpenTable).is_empty());
    assert!(s.list(GraphList::Closed).is_empty());
    // the released nodes are reused: Clear splices the closed list, then the open table, onto the free list
    assert_eq!(s.new_node(0, 0, 100, 0), o);
    assert_eq!(s.new_node(0, 0, 100, 0), c);
}

#[test]
fn hashed_open_goes_to_buckets_until_the_sorted_list_holds_nodes() {
    let mut s = hashed(16);
    open_node(&mut s, 1, 1, 1, 30);
    open_node(&mut s, 2, 2, 2, 10);
    open_node(&mut s, 3, 3, 3, 20);
    assert!(indices(&s, GraphList::SortedOpen).is_empty());
    assert_eq!(indices(&s, GraphList::OpenTable), vec![3, 1, 2]); // buckets 51 % 27 = 24, 17, 34 % 27 = 7
    assert_eq!(s.open_node(2), s.list(GraphList::OpenTable)[2]);
    assert_eq!(s.open_node(4), NIL);
    let best = s.remove_best_open();
    assert_eq!(s.node(best).index, 2);
    assert_eq!(s.flags_at(2), 0);
    assert_eq!(s.open_node(2), NIL);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![3, 1]);
    assert!(indices(&s, GraphList::OpenTable).is_empty());
    // a node no dearer than the tail joins the sorted list before the first node costing at least as much
    open_node(&mut s, 4, 4, 4, 20);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![4, 3, 1]);
    // a dearer one goes to its bucket
    open_node(&mut s, 5, 5, 5, 99);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![4, 3, 1]);
    assert_eq!(indices(&s, GraphList::OpenTable), vec![5]);
    assert!(s.has_open());
}

#[test]
fn hashed_best_open_keeps_fifteen_and_ties_in_bucket_order() {
    let mut s = hashed(64);
    // 20 nodes, one per bucket ((0, y): bucket y), costs 100 - y: the 15 cheapest are y 5..19
    for y in 0..20 {
        open_node(&mut s, 0, y, y, 100 - y as u32);
    }
    let first = s.remove_best_open();
    assert_eq!(s.node(first).index, 19);
    assert_eq!(indices(&s, GraphList::SortedOpen), (5..19).rev().collect::<Vec<_>>());
    assert_eq!(s.list(GraphList::OpenTable).len(), 5);
    // the sorted list refills only once it is empty
    for _ in 0..14 {
        s.remove_best_open();
    }
    assert!(indices(&s, GraphList::SortedOpen).is_empty());
    let next = s.remove_best_open();
    assert_eq!(s.node(next).index, 4);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![3, 2, 1, 0]);
    assert!(s.has_open());
    for _ in 0..4 {
        s.remove_best_open();
    }
    assert!(!s.has_open());
    assert_eq!(s.remove_best_open(), NIL);
}

#[test]
fn update_open_refiles_a_node() {
    let mut s = hashed(8);
    open_node(&mut s, 1, 0, 1, 10);
    open_node(&mut s, 2, 0, 2, 20);
    open_node(&mut s, 3, 0, 3, 30);
    let _ = s.remove_best_open(); // index 1; sorted: 2, 3
    let c = s.open_node(3);
    s.nodes[c as usize].cost = 5;
    s.update_open(c);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![3, 2]);
    let b = s.open_node(2);
    s.nodes[b as usize].cost = 50;
    s.update_open(b);
    // dearer than the new tail: back to its bucket
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![3]);
    assert_eq!(indices(&s, GraphList::OpenTable), vec![2]);
    // a bucket node that got cheaper moves into the sorted list before the tail
    s.nodes[b as usize].cost = 4;
    s.update_open(b);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![2, 3]);
    assert!(indices(&s, GraphList::OpenTable).is_empty());
    // the terrain storage's update does nothing
    let mut t = terrain(4);
    let p = open_node(&mut t, 0, 0, 0, 9);
    open_node(&mut t, 1, 0, 1, 1);
    t.nodes[p as usize].cost = 0;
    t.update_open(p);
    assert_eq!(indices(&t, GraphList::FastOpen), vec![1, 0]);
}

#[test]
fn terrain_open_is_a_lifo_list() {
    let mut s = terrain(8);
    open_node(&mut s, 0, 0, 0, 50);
    open_node(&mut s, 1, 0, 1, 10);
    open_node(&mut s, 2, 0, 2, 30);
    assert_eq!(indices(&s, GraphList::FastOpen), vec![2, 1, 0]);
    assert!(indices(&s, GraphList::SortedOpen).is_empty());
    assert_eq!(s.open_node(1), s.list(GraphList::FastOpen)[1]);
    let n = s.remove_best_open();
    assert_eq!(s.node(n).index, 2); // the last pushed, not the cheapest
    assert_eq!(s.flags_at(2), 0);
    assert_eq!(s.open_node(2), NIL);
    assert_eq!(indices(&s, GraphList::FastOpen), vec![1, 0]);
    assert_eq!(s.node(s.list(GraphList::FastOpen)[0]).prev, NIL);
    s.remove_best_open();
    s.remove_best_open();
    assert!(!s.has_open());
    assert_eq!(s.remove_best_open(), NIL);
}

#[test]
fn closed_list_and_nearest_removals() {
    let mut s = hashed(16);
    for (i, (x, y)) in [(1, 1), (5, 5), (9, 9)].iter().enumerate() {
        let n = s.new_node(*x, *y, 100 + i, i as i32);
        s.add_closed(n);
    }
    assert_eq!(indices(&s, GraphList::Closed), vec![2, 1, 0]);
    assert_eq!(s.flags_at(1), TILE_CLOSED);
    assert_eq!(s.closed_node(1), s.list(GraphList::Closed)[1]);
    // nearest by Manhattan distance; the first of equals in list order
    let n = s.remove_nearest_closed(6, 6);
    assert_eq!(s.node(n).index, 1);
    assert_eq!(s.flags_at(1), 0);
    // the node array keeps the stale entry, gated by the flag
    assert_eq!(s.closed_node(1), NIL);
    assert_eq!(indices(&s, GraphList::Closed), vec![2, 0]);
    let n = s.remove_nearest_closed(5, 5); // 2 and 0 are equally far: the list's first
    assert_eq!(s.node(n).index, 2);
    assert_eq!(indices(&s, GraphList::Closed), vec![0]);
    let last = s.list(GraphList::Closed)[0];
    assert_eq!(s.remove_nearest_closed(0, 0), last);
    assert!(s.list(GraphList::Closed).is_empty());
    assert_eq!(s.remove_nearest_closed(0, 0), NIL);
    // the open nearest: the sorted list, then the fast list, then the buckets, the first of equals
    open_node(&mut s, 4, 4, 4, 10);
    open_node(&mut s, 7, 7, 7, 20);
    s.remove_best_open(); // 4 out; the sorted list holds 7
    open_node(&mut s, 8, 8, 8, 99); // bucket
    let a = s.new_node(6, 6, 106, 6);
    s.push(a); // the fast list, as a terrain storage's AddOpen would
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![7]);
    assert_eq!(indices(&s, GraphList::FastOpen), vec![6]);
    assert_eq!(indices(&s, GraphList::OpenTable), vec![8]);
    let n = s.remove_nearest_open(7, 6); // 7 and 6 are 1 away: the sorted list first
    assert_eq!(s.node(n).index, 7);
    assert!(indices(&s, GraphList::SortedOpen).is_empty());
    assert_eq!(s.sorted_tail, NIL);
    let n = s.remove_nearest_open(0, 0);
    assert_eq!(s.node(n).index, 6);
    assert!(indices(&s, GraphList::FastOpen).is_empty());
    let n = s.remove_nearest_open(0, 0);
    assert_eq!(s.node(n).index, 8);
    assert_eq!(s.flags_at(8), 0);
    assert!(!s.has_open());
    assert_eq!(s.remove_nearest_open(0, 0), NIL);
}

#[test]
fn remove_closed_unlinks_from_the_middle_and_the_head() {
    let mut s = hashed(8);
    let nodes: Vec<i32> = (0..3).map(|i| s.new_node(i, 0, 100 + i as usize, i)).collect();
    for &n in &nodes {
        s.add_closed(n);
    }
    s.remove_closed(nodes[1]);
    assert_eq!(indices(&s, GraphList::Closed), vec![2, 0]);
    assert_eq!(s.closed_node(1), NIL);
    s.remove_closed(nodes[2]);
    assert_eq!(indices(&s, GraphList::Closed), vec![0]);
    assert_eq!(s.node(nodes[0]).prev, NIL);
}

#[test]
fn path_walk_follows_the_parents() {
    let mut s = hashed(8);
    let a = s.new_node(0, 0, 100, 0);
    let b = s.new_node(1, 0, 101, 1);
    let c = s.new_node(2, 0, 102, 2);
    s.nodes[b as usize].parent = a;
    s.nodes[c as usize].parent = b;
    assert_eq!(s.first_path_node(), None);
    s.path = c;
    assert_eq!(s.first_path_node(), Some((2, 0, 2)));
    assert_eq!(s.next_path_node(), Some((1, 0, 1)));
    assert_eq!(s.next_path_node(), Some((0, 0, 0)));
    assert_eq!(s.next_path_node(), None);
    // a path of one node: returned, and the cursor ends at once
    s.path = a;
    assert_eq!(s.first_path_node(), Some((0, 0, 0)));
    assert_eq!(s.next_path_node(), None);
    s.reset_nodes();
    assert_eq!(s.path, NIL);
    assert_eq!(s.first_path_node(), None);
}

#[test]
fn list_views_carry_the_parent_and_own_index() {
    let mut s = hashed(8);
    let a = s.new_node(3, 4, 100, 0);
    let b = s.new_node(5, 6, 101, 1);
    s.nodes[b as usize].parent = a;
    s.nodes[b as usize].cost = 9;
    s.nodes[b as usize].path_cost = 7;
    s.nodes[b as usize].estimate = 2;
    assert_eq!(s.view(b), GraphNodeView { x: 5, y: 6, cost: 9, path_cost: 7, estimate: 2, index: 1, object: 101 });
    assert_eq!(s.view(a).object, 100);
}

// ---- the machine ----

/// A graph of objects (handle 100 + id): each its tile and neighbours, in the C++ vector's order.
struct Graph {
    tiles: Vec<(i32, i32)>,
    neighbors: Vec<Vec<usize>>,
}

/// The goal the host plays: the sub sector path goal (`EEAIAStarPathGraphGoal`) or the continental chain flood.
#[derive(Clone, Copy, PartialEq, Eq)]
enum Goal {
    Path,
    Flood,
}

struct Fake {
    graph: Graph,
    goal: Goal,
    start: usize,
    /// the goal object (the path goal's `mTo`; the chain goal's `GetGoalID` is 0)
    to: usize,
    start_tile: (i32, i32),
    goal_tile: (i32, i32),
    /// the step cost into an object by id (the path goal's modifier): 10 unless listed
    costs: Vec<(usize, i32)>,
    /// the objects the goal may not enter
    blocked: Vec<usize>,
    rev_limit: i32,
    max_revs: i32,
    accept_nearest: bool,
    failed: bool,
    took_nearest: bool,
    state: GraphMachineState,
    /// the noted events, as the capture digests them
    events: Vec<String>,
}

impl Fake {
    fn new(graph: Graph, goal: Goal, start: usize, to: usize) -> Fake {
        let start_tile = graph.tiles[start];
        let goal_tile = if goal == Goal::Path { graph.tiles[to] } else { start_tile };
        Fake {
            graph,
            goal,
            start,
            to,
            start_tile,
            goal_tile,
            costs: Vec::new(),
            blocked: Vec::new(),
            rev_limit: -1,
            max_revs: -1,
            accept_nearest: false,
            failed: false,
            took_nearest: false,
            state: GraphMachineState::default(),
            events: Vec::new(),
        }
    }
    fn id(object: Obj) -> usize {
        object - 100
    }
}

impl GraphHost for Fake {
    fn start_object(&mut self) -> Obj {
        self.events.push(format!("o{}", self.start));
        100 + self.start
    }
    fn goal_id(&mut self) -> i32 {
        if self.goal == Goal::Path {
            self.to as i32
        } else {
            0
        }
    }
    fn estimate(&mut self, fx: i32, fy: i32, tx: i32, ty: i32) -> i32 {
        let e = if self.goal == Goal::Path { ((fx - tx).abs() + (fy - ty).abs()) * 10 } else { 1 };
        self.events.push(format!("e{fx},{fy}>{tx},{ty}={e}"));
        e
    }
    fn is_goal_node(&mut self, node: Option<&GraphNodeView>) -> bool {
        let r = match (self.goal, node) {
            (_, None) => true,
            (Goal::Path, Some(n)) => Fake::id(n.object) == self.to,
            (Goal::Flood, Some(_)) => false,
        };
        self.events.push(format!("g{}={}", node.map(|n| n.index).unwrap_or(-1), r as u8));
        r
    }
    fn object_id(&mut self, object: Obj) -> i32 {
        Fake::id(object) as i32
    }
    fn num_neighbors(&mut self, object: Obj) -> u32 {
        self.graph.neighbors[Fake::id(object)].len() as u32
    }
    fn neighbor(&mut self, object: Obj, i: u32) -> Obj {
        100 + self.graph.neighbors[Fake::id(object)][i as usize]
    }
    fn graph_location(&mut self, object: Obj) -> (i32, i32) {
        self.graph.tiles[Fake::id(object)]
    }
    fn can_enter(&mut self, neighbor: Obj, from: Obj) -> bool {
        let r = neighbor == from || !self.blocked.contains(&Fake::id(neighbor));
        self.events.push(format!("n{},{}={}", Fake::id(neighbor), Fake::id(from), r as u8));
        r
    }
    fn step_cost(&mut self, parent: &GraphNodeView, object: Obj) -> i32 {
        let id = Fake::id(object);
        let c = if self.goal == Goal::Flood {
            1
        } else {
            self.costs.iter().find(|(o, _)| *o == id).map(|(_, c)| *c).unwrap_or(10)
        };
        self.events.push(format!("c{},{}={}", parent.index, id, c));
        c
    }
    fn search_done(&mut self, current: Option<&GraphNodeView>, has_open: bool) {
        if self.goal == Goal::Path {
            // EEAIAStarPathGraphGoal::SearchDone
            let mut found = false;
            match current {
                None => {
                    if !has_open {
                        self.failed = true;
                        self.took_nearest = self.accept_nearest;
                    }
                }
                Some(n) => {
                    if (n.x - self.goal_tile.0).abs() <= 1 && (n.y - self.goal_tile.1).abs() <= 1 {
                        self.failed = false;
                        self.took_nearest = self.accept_nearest;
                        found = n.index == self.to as i32;
                    }
                }
            }
            if !self.failed {
                self.failed = !found && self.state.revs >= self.max_revs;
            }
        }
        self.events.push(format!("s{},{}", self.failed as u8, self.took_nearest as u8));
    }
    fn search_failed(&mut self) -> bool {
        self.failed
    }
    fn rev_limit(&mut self) -> i32 {
        self.rev_limit
    }
    fn max_revs(&mut self) -> i32 {
        self.max_revs
    }
    fn start_tile(&mut self) -> (i32, i32) {
        self.start_tile
    }
    fn goal_tile(&mut self) -> (i32, i32) {
        self.goal_tile
    }
    fn state(&mut self) -> GraphMachineState {
        self.state
    }
    fn set_state(&mut self, state: &GraphMachineState) {
        self.state = *state;
    }
}

/// `EEAIAStarGraphMachine::Run` over a fresh search.
fn run(s: &mut GraphStorage, host: &mut Fake, count: i32) {
    if !host.state.found {
        host.state.num_runs = 0;
        host.state.revs = 0;
        s.init(count);
        start(s, host);
    }
    expand(s, host);
}

fn path_indices(s: &mut GraphStorage) -> Vec<i32> {
    let mut out = Vec::new();
    if let Some((_, _, i)) = s.first_path_node() {
        out.push(i);
        while let Some((_, _, i)) = s.next_path_node() {
            out.push(i);
        }
    }
    out
}

/// Four sub sectors in a square: 0 (0,0), 1 (1,0), 2 (0,1), 3 (1,1); the step into 2 costs 30.
fn square() -> Graph {
    Graph {
        tiles: vec![(0, 0), (1, 0), (0, 1), (1, 1)],
        neighbors: vec![vec![1, 2], vec![0, 3], vec![0, 3], vec![1, 2]],
    }
}

#[test]
fn path_search_takes_the_cheap_side_and_notes_in_the_cpp_order() {
    let mut host = Fake::new(square(), Goal::Path, 0, 3);
    host.costs.push((2, 30));
    let mut s = hashed(4);
    run(&mut s, &mut host, 4);
    assert_eq!(
        host.events,
        vec![
            "o0",
            "e0,0>1,1=20",
            // the start node: its neighbours from the last, 2 (cost 30, estimate 10), then 1 (10, 10)
            "g0=0",
            "n2,0=1",
            "c0,2=30",
            "e0,1>1,1=10",
            "n1,0=1",
            "c0,1=10",
            "e1,0>1,1=10",
            // the cheaper node 1 next; 0 is closed and no dearer
            "g1=0",
            "n3,1=1",
            "c1,3=10",
            "e1,1>1,1=0",
            "n0,1=1",
            "c1,0=10",
            // the goal
            "g3=1",
            "s0,0",
        ]
    );
    assert_eq!(
        host.state,
        GraphMachineState { tile_x: 1, tile_y: 1, revs: 3, num_runs: 1, steps: 2, sector: 3, found: false }
    );
    assert!(!host.failed);
    assert_eq!(indices(&s, GraphList::Closed), vec![1, 0]);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![2]);
    assert!(indices(&s, GraphList::OpenTable).is_empty());
    assert_eq!(s.view(s.path).index, 3);
    assert_eq!(s.view(s.path).path_cost, 20);
    assert_eq!(path_indices(&mut s), vec![3, 1, 0]);
    // the path walk is repeatable
    assert_eq!(path_indices(&mut s), vec![3, 1, 0]);
}

#[test]
fn a_cheaper_route_to_an_open_node_refiles_it() {
    // 0 (0,0) -> 1 (5,0) costs 100; 0 -> 2 (1,0) costs 10, 2 -> 1 costs 10: node 1 is opened dear, then re-parented
    let graph = Graph {
        tiles: vec![(0, 0), (5, 0), (1, 0), (6, 0)],
        neighbors: vec![vec![2, 1], vec![0, 2, 3], vec![0, 1], vec![1]],
    };
    let mut host = Fake::new(graph, Goal::Path, 0, 3);
    host.costs.push((1, 100));
    let mut s = hashed(4);
    // the host's cost into 1 is 100 from either parent: make it depend on the parent through the events instead
    run(&mut s, &mut host, 4);
    // start: 1 is added first (cost 100, estimate 10 -> 110), then 2 (10 + 50); 2 is expanded next and reaches 1
    // at 10 + 100 = 110 > 100: no change; the search then goes on through 1 to the goal 3
    assert_eq!(path_indices(&mut s), vec![3, 1, 0]);
    assert_eq!(s.view(s.path).path_cost, 110);
    assert!(host.events.contains(&"c2,1=100".to_string()));
    // now with a cheaper step into 1 from 2 than from 0: the open node 1 is re-parented and re-sorted
    let graph = Graph {
        tiles: vec![(0, 0), (5, 0), (1, 0), (6, 0)],
        neighbors: vec![vec![2, 1], vec![0, 2, 3], vec![0, 1], vec![1]],
    };
    let mut host = Fake::new(graph, Goal::Path, 0, 3);
    host.costs.push((1, 100));
    let mut s = hashed(4);
    s.init(4);
    start(&mut s, &mut host);
    // expand by hand: pop the start, add 1 (100) and 2 (10)
    let a = s.remove_best_open();
    add_node(&mut s, &mut host, a, 101);
    add_node(&mut s, &mut host, a, 102);
    let one = s.open_node(1);
    assert_eq!(s.node(one).path_cost, 100);
    assert_eq!(s.node(one).parent, a);
    // from 2 (path 10) into 1 at a step of 5: 15 < 100, so 1 is re-parented and its cost falls to 15 + 10
    let two = s.open_node(2);
    host.costs.clear();
    host.costs.push((1, 5));
    add_node(&mut s, &mut host, two, 101);
    assert_eq!(s.node(one).parent, two);
    assert_eq!(s.node(one).path_cost, 15);
    assert_eq!(s.node(one).cost, 25);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![]);
    assert_eq!(indices(&s, GraphList::OpenTable), vec![1, 2]); // buckets 26 and 16: re-filed at the head of 26
                                                               // a closed node reached cheaper is removed and reopened (the released node is reused at once)
    assert_eq!(s.remove_nearest_open(1, 0), two);
    s.nodes[two as usize].path_cost = 50;
    s.add_closed(two);
    assert_eq!(s.closed_node(2), two);
    host.costs.clear();
    host.costs.push((2, 1));
    add_node(&mut s, &mut host, one, 102);
    assert_eq!(s.closed_node(2), NIL);
    let two2 = s.open_node(2);
    assert_eq!(two2, two);
    assert_eq!(s.node(two2).path_cost, 16);
    assert_eq!(s.node(two2).parent, one);
    assert_eq!(s.node(two2).cost, 66);
    assert!(indices(&s, GraphList::Closed).is_empty());
    // a closed node that is no dearer stays
    s.nodes[two as usize].path_cost = 5;
    assert_eq!(s.remove_nearest_open(1, 0), two);
    s.add_closed(two);
    add_node(&mut s, &mut host, one, 102);
    assert_eq!(s.closed_node(2), two);
    assert_eq!(s.open_node(2), NIL);
}

#[test]
fn flood_over_a_terrain_storage_closes_every_continent() {
    // four continents in a line; the chain goal floods them all
    let graph = Graph {
        tiles: vec![(0, 0), (1, 0), (2, 0), (3, 0)],
        neighbors: vec![vec![1], vec![0, 2], vec![1, 3], vec![2]],
    };
    let mut host = Fake::new(graph, Goal::Flood, 0, 0);
    let mut s = terrain(4);
    run(&mut s, &mut host, 4);
    assert_eq!(
        host.events,
        vec![
            "o0",
            "e0,0>0,0=1",
            "g0=0",
            "n1,0=1",
            "c0,1=1",
            "e1,0>0,0=1",
            "g1=0",
            "n2,1=1",
            "c1,2=1",
            "e2,0>0,0=1",
            "n0,1=1",
            "c1,0=1",
            "g2=0",
            "n3,2=1",
            "c2,3=1",
            "e3,0>0,0=1",
            "n1,2=1",
            "c2,1=1",
            "g3=0",
            "n2,3=1",
            "c3,2=1",
            "g-1=1",
            "s0,0",
        ]
    );
    assert_eq!(
        host.state,
        GraphMachineState { tile_x: 0, tile_y: 0, revs: 5, num_runs: 1, steps: 4, sector: 0, found: false }
    );
    assert_eq!(indices(&s, GraphList::Closed), vec![3, 2, 1, 0]);
    assert!(indices(&s, GraphList::FastOpen).is_empty());
    assert_eq!(s.current, NIL);
    assert_eq!(s.path, NIL);
    // NumberChain reads the closed and fast open lists' objects
    let objects: Vec<Obj> = s.list(GraphList::Closed).iter().map(|&n| s.node(n).object).collect();
    assert_eq!(objects, vec![103, 102, 101, 100]);
    // a blocked continent and its far side stay out; the search stops at the empty list
    let graph = Graph {
        tiles: vec![(0, 0), (1, 0), (2, 0), (3, 0)],
        neighbors: vec![vec![1], vec![0, 2], vec![1, 3], vec![2]],
    };
    let mut host = Fake::new(graph, Goal::Flood, 0, 0);
    host.blocked.push(2);
    let mut s = terrain(4);
    run(&mut s, &mut host, 4);
    assert_eq!(indices(&s, GraphList::Closed), vec![1, 0]);
    assert_eq!(host.state.revs, 3);
}

#[test]
fn a_rev_limit_suspends_the_search_and_the_next_expand_resumes_it() {
    let mut host = Fake::new(square(), Goal::Path, 0, 3);
    host.costs.push((2, 30));
    host.rev_limit = 1;
    let mut s = hashed(4);
    run(&mut s, &mut host, 4);
    // two nodes closed (0, then 1), then steps (1) >= the limit: suspended with found set, no SearchDone
    assert_eq!(
        host.state,
        GraphMachineState { tile_x: 1, tile_y: 1, revs: 2, num_runs: 1, steps: 1, sector: 3, found: true }
    );
    assert_eq!(s.current, NIL);
    assert_eq!(s.path, NIL);
    assert!(!host.events.iter().any(|e| e.starts_with('s')));
    assert_eq!(indices(&s, GraphList::Closed), vec![1, 0]);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![3, 2]);
    let before = host.events.len();
    run(&mut s, &mut host, 4);
    assert_eq!(&host.events[before..], &["g3=1", "s0,0"]);
    assert_eq!(
        host.state,
        GraphMachineState { tile_x: 1, tile_y: 1, revs: 3, num_runs: 2, steps: 0, sector: 3, found: false }
    );
    assert_eq!(path_indices(&mut s), vec![3, 1, 0]);
}

#[test]
fn a_max_revs_cut_fails_the_path_search_and_restarts_at_the_nearest_node() {
    let mut host = Fake::new(square(), Goal::Path, 0, 3);
    host.costs.push((2, 30));
    host.max_revs = 2;
    let mut s = hashed(4);
    run(&mut s, &mut host, 4);
    // 0 and 1 closed, then revs 2 >= 2: cut with no current node; the goal fails the search (open nodes are left,
    // but revs >= max revs) and the machine restarts at the node nearest to the goal tile
    assert_eq!(
        host.events,
        vec![
            "o0",
            "e0,0>1,1=20",
            "g0=0",
            "n2,0=1",
            "c0,2=30",
            "e0,1>1,1=10",
            "n1,0=1",
            "c0,1=10",
            "e1,0>1,1=10",
            "g1=0",
            "n3,1=1",
            "c1,3=10",
            "e1,1>1,1=0",
            "n0,1=1",
            "c1,0=10",
            "s1,0",
        ]
    );
    assert!(host.failed);
    // the nearest open node is the goal node 3 itself (opened from 1, not yet popped, distance 0); the nearest
    // closed one, 1, is a step away and no cheaper, so it goes back to the closed list. The failed search still
    // hands out the whole path.
    assert_eq!(
        host.state,
        GraphMachineState { tile_x: 1, tile_y: 1, revs: 2, num_runs: 1, steps: 1, sector: 3, found: false }
    );
    assert_eq!(path_indices(&mut s), vec![3, 1, 0]);
    assert_eq!(s.current, s.path);
    assert_eq!(indices(&s, GraphList::Closed), vec![1, 0]);
    assert_eq!(indices(&s, GraphList::SortedOpen), vec![2]);
    assert_eq!(s.flags_at(3), 0);
}

#[test]
fn an_unreachable_goal_fails_and_restarts_at_the_nearest_closed_node() {
    let graph = Graph { tiles: vec![(0, 0), (5, 0), (6, 0)], neighbors: vec![vec![1], vec![0], vec![]] };
    let mut host = Fake::new(graph, Goal::Path, 0, 2);
    host.accept_nearest = true;
    let mut s = hashed(3);
    run(&mut s, &mut host, 3);
    assert_eq!(
        host.events,
        vec![
            "o0",
            "e0,0>6,0=60",
            "g0=0",
            "n1,0=1",
            "c0,1=10",
            "e5,0>6,0=10",
            "g1=0",
            "n0,1=1",
            "c1,0=10",
            "g-1=1",
            "s1,1"
        ]
    );
    assert!(host.failed && host.took_nearest);
    // the nearest closed node to the goal tile, 1 at (5, 0), is the path; it left the closed list
    assert_eq!(
        host.state,
        GraphMachineState { tile_x: 5, tile_y: 0, revs: 3, num_runs: 1, steps: 2, sector: 1, found: false }
    );
    assert_eq!(path_indices(&mut s), vec![1, 0]);
    assert_eq!(indices(&s, GraphList::Closed), vec![0]);
    assert_eq!(s.flags_at(1), 0);
}

#[test]
fn restart_without_any_node_makes_a_start_node_of_index_minus_one() {
    let mut host = Fake::new(square(), Goal::Path, 0, 3);
    let mut s = hashed(4);
    s.init(4);
    host.state.tile_x = 1;
    host.state.tile_y = 1;
    assert!(restart(&mut s, &mut host));
    assert_eq!(host.events, vec!["o0"]);
    let p = s.view(s.path);
    assert_eq!((p.x, p.y, p.index, p.object, p.cost, p.path_cost, p.estimate), (0, 0, -1, 100, 1, 0, 1));
    assert_eq!(s.current, s.path);
    assert!(!host.state.found);
    assert_eq!(path_indices(&mut s), vec![-1]);
    // the node is on no list: a Clear releases it as the current node
    s.clear();
    assert_eq!(s.current, NIL);
    assert_eq!(s.new_node(0, 0, 100, 0), 0);
}

#[test]
fn next_node_prefers_the_open_node_on_a_tie_and_returns_the_other() {
    let mut host = Fake::new(square(), Goal::Path, 0, 3);
    let mut s = hashed(4);
    s.init(4);
    host.state.tile_x = 0;
    host.state.tile_y = 0;
    let o = open_node(&mut s, 1, 0, 1, 20);
    let c = s.new_node(0, 1, 102, 2);
    s.nodes[c as usize].cost = 20;
    s.add_closed(c);
    // equal distances and costs: the open node, the closed one back on its list
    assert_eq!(next_node(&mut s, &mut host), o);
    assert_eq!(indices(&s, GraphList::Closed), vec![2]);
    assert_eq!(s.flags_at(1), 0);
    s.add_open(o);
    s.nodes[o as usize].cost = 30;
    // a dearer open node loses: it goes back to the open lists, the closed node is taken
    assert_eq!(next_node(&mut s, &mut host), c);
    assert_eq!(indices(&s, GraphList::OpenTable), vec![1]);
    assert!(indices(&s, GraphList::Closed).is_empty());
    // only one kind left
    assert_eq!(next_node(&mut s, &mut host), o);
    assert_eq!(next_node(&mut s, &mut host), NIL);
}
