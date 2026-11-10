//! The AI's graph A* searches: the node storage (`EEAIAStarGraphStorage.cpp`, `EEAIAStarGraphTerrainStorage.h`) and
//! the search machine (`EEAIAStarGraphMachine<S, G, M>`'s `Start`, `Expand`, `Restart`, `NextNode` and `AddNode`,
//! `ee/EEAIAStarMachine.h`), line by line. They search a graph of objects (the path sub sectors, the continents)
//! instead of tiles: a node names its object (an opaque handle, never dereferenced here), the object's index (its
//! id, which indexes the per-node flag and node arrays) and the object's tile (the hash key, the distance of the
//! nearest-node removals).
//!
//! The goal stays the caller's: every question the C++ machine asked its goal `G` and its objects `M` is a
//! [`GraphHost`] call in the same order and number. The machine's own members (`mTile`, `mRevs`, `mNumRuns`,
//! `mSteps`, `mSector`, `mFound`) stay the C++ machine's, read and written through the host every time and never
//! cached across a call into the goal.
//!
//! Nodes live in an arena and link by index (`NIL` for the C++ NULL); the free list is per storage (the C++'s was one
//! static list shared by every storage: which free node a search reuses is not observable, since `NewNode` clears
//! every field).
//!
//! The two storages differ in three members (the machine calls the most derived one of its storage `S`):
//!
//! | member | `Hashed` (`EEAIAStarGraphStorage`: the sub sector paths) | `Terrain` (the continental chains) |
//! |---|---|---|
//! | `add_open` | sorted list or open bucket | front of the fast open list (`Push`) |
//! | `update_open` | re-files the node | nothing (the fast list is unsorted) |
//! | `remove_best_open` | sorted list (refilled from the buckets) | front of the fast open list (`Pop`) |

use crate::storage::{TILE_CLOSED, TILE_OPEN};

/// The C++ NULL of a node link.
pub(crate) const NIL: i32 = -1;

/// The graph storage's hash bucket count (`mBucketCount`, 0x1b).
pub(crate) const BUCKETS: i32 = 0x1b;

/// A graph object as the storage keeps it: the C++ pointer's bits (`AStarGraphableObjectBase*`), opaque here.
pub type Obj = usize;

/// `EEAIAStarGraphNode`: the links are arena indices.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GraphNode {
    pub object: Obj,
    pub parent: i32,
    pub next: i32,
    pub prev: i32,
    /// the object's index (`mIndex`: its id; the flag and node arrays' index)
    pub index: i32,
    pub cost: u32,
    pub path_cost: i32,
    pub estimate: i32,
    pub x: i32,
    pub y: i32,
}

/// A node as the goals see one (`IsGoalNode`, `GetStepCost`, `SearchDone`): its tile, costs, index and object. The
/// C++ goals read `mObject`, `mIndex` and `mTile` of the nodes the machine hands them, never the links.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GraphNodeView {
    pub x: i32,
    pub y: i32,
    pub cost: u32,
    pub path_cost: i32,
    pub estimate: i32,
    pub index: i32,
    pub object: Obj,
}

/// Which members the storage's machine calls (see the module table).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GraphKind {
    /// `EEAIAStarGraphStorage`
    Hashed = 0,
    /// `EEAIAStarGraphTerrainStorage`
    Terrain = 1,
}

/// The storage's lists, in the capture's order (`EEAIAStarGraphStorage::CaptureNodes`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GraphList {
    /// `mClosedList`
    Closed = 0,
    /// `mClosedTable`, every bucket, the last first
    ClosedTable = 1,
    /// `mSortedOpen`
    SortedOpen = 2,
    /// `mOpenTable`, every bucket, the last first
    OpenTable = 3,
    /// `mOpenList` (`Push` / `Pop`)
    FastOpen = 4,
}

/// `EEAIAStarGraphMachineBase`'s search members.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GraphMachineState {
    pub tile_x: i32,
    pub tile_y: i32,
    pub revs: i32,
    pub num_runs: i32,
    pub steps: i32,
    pub sector: i32,
    pub found: bool,
}

/// The storage of one graph machine.
#[derive(Clone, Debug)]
pub struct GraphStorage {
    pub kind: GraphKind,
    pub nodes: Vec<GraphNode>,
    pub free: i32,
    pub closed_table: Option<Vec<i32>>,
    pub open_table: Option<Vec<i32>>,
    /// `mClosedList`
    pub closed_list: i32,
    /// `mOpenList` (the fast open list)
    pub open_list: i32,
    pub sorted_open: i32,
    pub sorted_tail: i32,
    pub path: i32,
    pub path_cursor: i32,
    pub current: i32,
    /// `mFlags`: one byte per index (`TILE_OPEN`, `TILE_CLOSED`)
    pub flags: Option<Vec<u8>>,
    /// `mNodes`: one node per index (`NIL`: none)
    pub node_map: Option<Vec<i32>>,
    /// `mCount`: the index count
    pub count: i32,
    pub bucket_count: i32,
    /// `mOldCount`: the count the arrays were sized for
    pub old_count: i32,
}

impl GraphStorage {
    /// 0x160b7: an empty storage with no lists or tables and 27 hash buckets.
    pub fn new(kind: GraphKind) -> GraphStorage {
        GraphStorage {
            kind,
            nodes: Vec::new(),
            free: NIL,
            closed_table: None,
            open_table: None,
            closed_list: NIL,
            open_list: NIL,
            sorted_open: NIL,
            sorted_tail: NIL,
            path: NIL,
            path_cursor: NIL,
            current: NIL,
            flags: None,
            node_map: None,
            count: 0,
            bucket_count: BUCKETS,
            old_count: 0,
        }
    }

    pub fn node(&self, i: i32) -> &GraphNode {
        &self.nodes[i as usize]
    }
    fn n(&mut self, i: i32) -> &mut GraphNode {
        &mut self.nodes[i as usize]
    }
    fn flag(&mut self, index: i32) -> &mut u8 {
        &mut self.flags.as_mut().expect("the flags")[index as usize]
    }
    pub fn flags_at(&self, index: i32) -> u8 {
        self.flags.as_ref().expect("the flags")[index as usize]
    }
    fn set_map(&mut self, index: i32, node: i32) {
        self.node_map.as_mut().expect("the node map")[index as usize] = node;
    }
    fn map(&self, index: i32) -> i32 {
        self.node_map.as_ref().expect("the node map")[index as usize]
    }
    fn table(&self, closed: bool) -> &[i32] {
        if closed { self.closed_table.as_deref() } else { self.open_table.as_deref() }.expect("the hash tables")
    }
    fn bucket_head(&self, closed: bool, b: i32) -> i32 {
        self.table(closed)[b as usize]
    }
    fn set_bucket_head(&mut self, closed: bool, b: i32, node: i32) {
        let t = if closed { self.closed_table.as_mut() } else { self.open_table.as_mut() }.expect("the hash tables");
        t[b as usize] = node;
    }

    /// A node as the goals see it.
    pub fn view(&self, node: i32) -> GraphNodeView {
        let n = self.node(node);
        GraphNodeView {
            x: n.x,
            y: n.y,
            cost: n.cost,
            path_cost: n.path_cost,
            estimate: n.estimate,
            index: n.index,
            object: n.object,
        }
    }

    /// 0x16233 `GetBucket`: the hash of a tile (the high nibble's fold replaces the hash, as in the C++).
    pub fn bucket(&self, x: i32, y: i32) -> i32 {
        let mut h = x.wrapping_shl(4).wrapping_add(y);
        let high = h & (0xf000_0000u32 as i32);
        if high != 0 {
            h = (high >> 24) ^ high;
        }
        h % self.bucket_count
    }

    // ---- the node pool (0x16070, 0x1616b, 0x162b0) ----

    /// 0x1616b `ReleaseNode`: the node onto the free list.
    pub fn release_node(&mut self, node: i32) {
        let free = self.free;
        self.n(node).next = free;
        self.free = node;
    }

    /// 0x162b0 `NewNode`: a cleared node for the tile, object and index, from the free list or new.
    pub fn new_node(&mut self, x: i32, y: i32, object: Obj, index: i32) -> i32 {
        let node = if self.free != NIL {
            let node = self.free;
            self.free = self.node(node).next;
            node
        } else {
            self.nodes.push(GraphNode::default());
            (self.nodes.len() - 1) as i32
        };
        *self.n(node) =
            GraphNode { object, parent: NIL, next: NIL, prev: NIL, index, cost: 0, path_cost: 0, estimate: 0, x, y };
        node
    }

    fn last_of(&self, mut node: i32) -> i32 {
        while self.node(node).next != NIL {
            node = self.node(node).next;
        }
        node
    }
    fn splice_free(&mut self, head: i32) {
        let last = self.last_of(head);
        let free = self.free;
        self.n(last).next = free;
        self.free = head;
    }

    // ---- the lists ----

    /// 0x1613f `AddClosed`: at the head of the closed list, flagged closed and in the node array.
    pub fn add_closed(&mut self, node: i32) {
        let head = self.closed_list;
        if head != NIL {
            self.n(head).prev = node;
        }
        self.n(node).next = head;
        self.closed_list = node;
        let index = self.node(node).index;
        *self.flag(index) |= TILE_CLOSED;
        self.set_map(index, node);
    }

    /// `AddOpen` of the variant.
    pub fn add_open(&mut self, node: i32) {
        match self.kind {
            GraphKind::Hashed => self.hashed_add_open(node),
            GraphKind::Terrain => self.push(node),
        }
    }

    /// 0x1618a: into the sorted open list when it costs no more than its tail, else onto its hash bucket; flagged
    /// open and in the node array.
    fn hashed_add_open(&mut self, node: i32) {
        let mut inserted = false;
        let cost = self.node(node).cost;
        if self.sorted_tail != NIL && cost <= self.node(self.sorted_tail).cost {
            let mut at = self.sorted_open;
            while at != NIL {
                if !(cost <= self.node(at).cost) {
                    at = self.node(at).next;
                    continue;
                }
                let prev = self.node(at).prev;
                if prev != NIL {
                    self.n(prev).next = node;
                    self.n(node).next = at;
                    self.n(node).prev = prev;
                    self.n(at).prev = node;
                } else {
                    self.n(node).next = at;
                    self.n(at).prev = node;
                    self.sorted_open = node;
                }
                if self.node(at).next == NIL {
                    self.sorted_tail = at;
                }
                inserted = true;
                break;
            }
        }
        if !inserted {
            let (x, y) = (self.node(node).x, self.node(node).y);
            let b = self.bucket(x, y);
            let head = self.bucket_head(false, b);
            if head != NIL {
                self.n(head).prev = node;
            }
            self.n(node).next = head;
            self.set_bucket_head(false, b, node);
        }
        let index = self.node(node).index;
        *self.flag(index) |= TILE_OPEN;
        self.set_map(index, node);
    }

    /// 0x16221 `ResetNodes`: every node back to the free list, no path.
    pub fn reset_nodes(&mut self) {
        self.clear();
        self.path_cursor = NIL;
        self.path = NIL;
    }

    /// 0x16258 `GetFirstPathNode`: the found path's first node's tile and index (`None`: no path, the C++'s zeros),
    /// and the cursor on the next.
    pub fn first_path_node(&mut self) -> Option<(i32, i32, i32)> {
        if self.path == NIL {
            return None;
        }
        self.path_cursor = self.path;
        if self.node(self.path).parent != NIL {
            self.next_path_node()
        } else {
            let p = *self.node(self.path);
            self.path_cursor = NIL;
            Some((p.x, p.y, p.index))
        }
    }

    /// 0x1634f `GetNextPathNode`: the cursor's tile and index, the cursor onto its parent (`None` at the end).
    pub fn next_path_node(&mut self) -> Option<(i32, i32, i32)> {
        if self.path_cursor != NIL {
            let p = *self.node(self.path_cursor);
            self.path_cursor = p.parent;
            return Some((p.x, p.y, p.index));
        }
        None
    }

    /// 0x16394 `Clear`: the current node and every list onto the free list, the hash tables freed.
    pub fn clear(&mut self) {
        if self.current != NIL {
            let c = self.current;
            self.release_node(c);
            self.current = NIL;
        }
        if self.closed_table.is_some() {
            for i in (0..self.bucket_count).rev() {
                let head = self.bucket_head(true, i);
                if head == NIL {
                    continue;
                }
                self.splice_free(head);
                self.set_bucket_head(true, i, NIL);
            }
            self.closed_table = None;
        }
        if self.closed_list != NIL {
            let head = self.closed_list;
            self.splice_free(head);
            self.closed_list = NIL;
        }
        if self.sorted_open != NIL {
            let head = self.sorted_open;
            self.splice_free(head);
            self.sorted_open = NIL;
            self.sorted_tail = NIL;
        }
        if self.open_table.is_some() {
            for i in (0..self.bucket_count).rev() {
                let head = self.bucket_head(false, i);
                if head == NIL {
                    continue;
                }
                self.splice_free(head);
                self.set_bucket_head(false, i, NIL);
            }
            self.open_table = None;
        }
        if self.open_list != NIL {
            let head = self.open_list;
            self.splice_free(head);
            self.open_list = NIL;
        }
    }

    /// 0x16514 `Init`: the storage for a graph of `count` nodes (the three steps the C++ class calls in turn, its
    /// `PrepareArrays` being a virtual slot).
    pub fn init(&mut self, count: i32) -> bool {
        self.init_begin(count);
        self.prepare_arrays(count);
        self.init_end();
        true
    }

    /// `Init`'s first lines: the counts.
    pub fn init_begin(&mut self, count: i32) {
        self.old_count = self.count;
        self.count = count;
    }

    /// `Init`'s last lines: `Clear`, then empty hash tables.
    pub fn init_end(&mut self) {
        self.clear();
        self.closed_table = Some(vec![NIL; self.bucket_count as usize]);
        self.open_table = Some(vec![NIL; self.bucket_count as usize]);
    }

    /// 0x16af6 `PrepareArrays`: the flag and node arrays sized to `count` and cleared (the C++ reallocates them when
    /// the count changed and clears them either way: the same arrays).
    pub fn prepare_arrays(&mut self, count: i32) {
        let n = count.max(0) as usize;
        self.flags = Some(vec![0; n]);
        self.node_map = Some(vec![NIL; n]);
    }

    /// `UpdateOpen` of the variant.
    pub fn update_open(&mut self, node: i32) {
        match self.kind {
            GraphKind::Hashed => self.hashed_update_open(node),
            GraphKind::Terrain => {}
        }
    }

    /// 0x1656d: re-files an open node after its cost changed: unlinked, then into the sorted open list when it is
    /// no dearer than the tail, else at the head of its hash bucket.
    fn hashed_update_open(&mut self, node: i32) {
        let (x, y) = (self.node(node).x, self.node(node).y);
        let b = self.bucket(x, y);
        let cost = self.node(node).cost;
        let (prev, next) = (self.node(node).prev, self.node(node).next);
        if prev != NIL {
            self.n(prev).next = next;
            if next != NIL {
                self.n(next).prev = prev;
            }
        } else if self.bucket_head(false, b) == node {
            let head = self.node(self.bucket_head(false, b)).next;
            self.set_bucket_head(false, b, head);
            if head != NIL {
                self.n(head).prev = NIL;
            }
        } else if self.sorted_open == node {
            self.sorted_open = self.node(self.sorted_open).next;
            if self.sorted_open != NIL {
                let head = self.sorted_open;
                self.n(head).prev = NIL;
            }
        }
        if self.sorted_tail == node {
            self.sorted_tail = self.node(node).prev;
        }
        self.n(node).prev = NIL;
        self.n(node).next = NIL;
        if self.sorted_open != NIL && self.sorted_tail != NIL && cost <= self.node(self.sorted_tail).cost {
            let mut at = self.sorted_open;
            while at != NIL {
                if cost <= self.node(at).cost {
                    break;
                }
                at = self.node(at).next;
            }
            if at != NIL {
                let prev = self.node(at).prev;
                if prev != NIL {
                    self.n(node).prev = prev;
                    self.n(node).next = at;
                    self.n(prev).next = node;
                    self.n(at).prev = node;
                } else {
                    let head = self.sorted_open;
                    self.n(head).prev = node;
                    self.n(node).next = head;
                    self.sorted_open = node;
                }
                if self.node(node).next == NIL {
                    self.sorted_tail = node;
                }
                return;
            }
        }
        let head = self.bucket_head(false, b);
        if head != NIL {
            self.n(head).prev = node;
        }
        self.n(node).next = head;
        self.set_bucket_head(false, b, node);
    }

    fn nearest_in(&self, mut node: i32, x: i32, y: i32, best: &mut i32, best_distance: &mut i32) {
        while node != NIL {
            let n = self.node(node);
            let distance = (n.x.wrapping_sub(x)).wrapping_abs().wrapping_add((n.y.wrapping_sub(y)).wrapping_abs());
            if *best_distance == -1 || distance < *best_distance {
                *best = node;
                *best_distance = distance;
            }
            node = n.next;
        }
    }

    /// 0x1665f `RemoveNearestClosed`: the closed node nearest to the tile (Manhattan; the first of equals in list
    /// order: the closed list, then the buckets, the last first), unlinked and its closed flag cleared.
    pub fn remove_nearest_closed(&mut self, x: i32, y: i32) -> i32 {
        let mut best_distance = -1;
        let mut result = NIL;
        self.nearest_in(self.closed_list, x, y, &mut result, &mut best_distance);
        for i in (0..self.bucket_count).rev() {
            self.nearest_in(self.bucket_head(true, i), x, y, &mut result, &mut best_distance);
        }
        if result != NIL {
            let (prev, next) = (self.node(result).prev, self.node(result).next);
            if prev != NIL {
                self.n(prev).next = next;
                if next != NIL {
                    self.n(next).prev = prev;
                }
            } else {
                let (rx, ry) = (self.node(result).x, self.node(result).y);
                let b = self.bucket(rx, ry);
                if result == self.bucket_head(true, b) {
                    self.set_bucket_head(true, b, next);
                } else {
                    self.closed_list = next;
                }
                if next != NIL {
                    self.n(next).prev = NIL;
                }
            }
            self.n(result).next = NIL;
            self.n(result).prev = NIL;
            let index = self.node(result).index;
            *self.flag(index) &= !TILE_CLOSED;
        }
        result
    }

    /// 0x16771 `RemoveNearestOpen`: the open node nearest to the tile (the sorted list, the fast list, then the
    /// buckets, the last first), unlinked from whichever list holds it and its open flag cleared.
    pub fn remove_nearest_open(&mut self, x: i32, y: i32) -> i32 {
        let mut best_distance = -1;
        let mut result = NIL;
        self.nearest_in(self.sorted_open, x, y, &mut result, &mut best_distance);
        self.nearest_in(self.open_list, x, y, &mut result, &mut best_distance);
        for i in (0..self.bucket_count).rev() {
            self.nearest_in(self.bucket_head(false, i), x, y, &mut result, &mut best_distance);
        }
        if result != NIL {
            let (prev, next) = (self.node(result).prev, self.node(result).next);
            if prev != NIL {
                self.n(prev).next = next;
                if next != NIL {
                    self.n(next).prev = prev;
                }
            } else {
                let (rx, ry) = (self.node(result).x, self.node(result).y);
                let b = self.bucket(rx, ry);
                if self.bucket_head(false, b) == result {
                    if next != NIL {
                        self.n(next).prev = NIL;
                    }
                    self.set_bucket_head(false, b, next);
                } else if self.open_list == result {
                    if next != NIL {
                        self.n(next).prev = NIL;
                    }
                    self.open_list = next;
                } else if self.sorted_open == result {
                    if next != NIL {
                        self.n(next).prev = NIL;
                    }
                    self.sorted_open = next;
                    // the C++ sets the head from the node's (null) prev when it was the tail too
                    if self.sorted_tail == result {
                        self.sorted_open = self.node(result).prev;
                    }
                }
            }
            if self.sorted_tail == result {
                self.sorted_tail = self.node(result).prev;
            }
            self.n(result).next = NIL;
            self.n(result).prev = NIL;
            let index = self.node(result).index;
            *self.flag(index) &= !TILE_OPEN;
        }
        result
    }

    /// `RemoveBestOpen` of the variant.
    pub fn remove_best_open(&mut self) -> i32 {
        match self.kind {
            GraphKind::Hashed => self.hashed_remove_best_open(),
            GraphKind::Terrain => self.pop(),
        }
    }

    /// 0x16905: the cheapest open node; when the sorted list is empty, first up to 15 of the cheapest bucketed
    /// nodes move into it.
    fn hashed_remove_best_open(&mut self) -> i32 {
        let mut result = NIL;
        if self.sorted_open == NIL {
            let mut best = [NIL; 15];
            let mut worst: i32 = -1;
            let mut worst_cost: u32 = 0;
            let mut seen = 0i32;
            let mut filled = 0i32;
            for i in (0..self.bucket_count).rev() {
                let mut node = self.bucket_head(false, i);
                while node != NIL {
                    let cost = self.node(node).cost;
                    if seen >= 15 {
                        if cost < worst_cost {
                            best[worst as usize] = node;
                            worst = 0;
                            for j in 1..15 {
                                if self.node(best[j]).cost > self.node(best[worst as usize]).cost {
                                    worst = j as i32;
                                }
                            }
                            worst_cost = self.node(best[worst as usize]).cost;
                            seen += 1;
                        }
                    } else {
                        if worst != -1 {
                            if cost >= self.node(best[worst as usize]).cost {
                                worst = filled;
                                worst_cost = cost;
                            }
                            best[filled as usize] = node;
                            filled += 1;
                        } else {
                            worst_cost = cost;
                            worst = 0;
                            best[0] = node;
                            filled = 1;
                        }
                        seen += 1;
                    }
                    node = self.node(node).next;
                }
            }
            if worst != -1 {
                for i in (0..filled.min(15)).rev() {
                    let node = best[i as usize];
                    let (prev, next) = (self.node(node).prev, self.node(node).next);
                    if prev != NIL {
                        self.n(prev).next = next;
                        if next != NIL {
                            self.n(next).prev = prev;
                        }
                        self.n(node).prev = NIL;
                        self.n(node).next = NIL;
                    } else {
                        let (x, y) = (self.node(node).x, self.node(node).y);
                        let b = self.bucket(x, y);
                        self.set_bucket_head(false, b, next);
                        if next != NIL {
                            self.n(next).prev = NIL;
                        }
                        self.n(node).next = NIL;
                    }
                    self.insert_sorted(node);
                }
            }
        }
        if self.sorted_open != NIL {
            result = self.sorted_open;
            self.sorted_open = self.node(result).next;
            if self.sorted_open != NIL {
                let head = self.sorted_open;
                self.n(head).prev = NIL;
            }
            if self.sorted_open == NIL {
                self.sorted_tail = NIL;
            }
            self.n(result).next = NIL;
            self.n(result).prev = NIL;
            let index = self.node(result).index;
            *self.flag(index) &= !TILE_OPEN;
            self.set_map(index, NIL);
        }
        result
    }

    /// 0x16aa5 `RemoveClosed`: unlinked from the closed list, its closed flag and node array entry cleared.
    pub fn remove_closed(&mut self, node: i32) {
        let (prev, next) = (self.node(node).prev, self.node(node).next);
        if prev != NIL {
            self.n(prev).next = next;
            if next != NIL {
                self.n(next).prev = prev;
            }
        } else {
            self.closed_list = next;
            if next != NIL {
                self.n(next).prev = NIL;
            }
        }
        self.n(node).next = NIL;
        self.n(node).prev = NIL;
        let index = self.node(node).index;
        *self.flag(index) &= !TILE_CLOSED;
        self.set_map(index, NIL);
    }

    /// 0x16b5b `InsertSorted`: before the first sorted node that costs at least as much.
    pub fn insert_sorted(&mut self, node: i32) {
        let mut prev = NIL;
        let mut at = self.sorted_open;
        let cost = self.node(node).cost;
        if at != NIL {
            while at != NIL {
                if cost <= self.node(at).cost {
                    break;
                }
                prev = at;
                at = self.node(at).next;
            }
            if at != NIL {
                let at_prev = self.node(at).prev;
                if at_prev != NIL {
                    self.n(at_prev).next = node;
                    self.n(node).next = at;
                    self.n(node).prev = at_prev;
                    self.n(at).prev = node;
                } else {
                    self.n(node).next = at;
                    self.n(at).prev = node;
                    self.sorted_open = node;
                }
            } else {
                self.n(node).prev = prev;
                self.n(prev).next = node;
                self.sorted_tail = node;
            }
        } else {
            self.sorted_open = node;
            self.sorted_tail = node;
        }
    }

    /// 0x16bb5 `Push`: onto the fast open list, flagged open and in the node array.
    pub fn push(&mut self, node: i32) {
        let index = self.node(node).index;
        *self.flag(index) |= TILE_OPEN;
        let head = self.open_list;
        if head != NIL {
            self.n(head).prev = node;
        }
        self.n(node).next = head;
        self.open_list = node;
        self.set_map(index, node);
    }

    /// 0x16be1 `Pop`: the head of the fast open list (`NIL` when empty), its open flag and node array entry cleared.
    pub fn pop(&mut self) -> i32 {
        let node = self.open_list;
        if node != NIL {
            self.open_list = self.node(node).next;
            if self.open_list != NIL {
                let head = self.open_list;
                self.n(head).prev = NIL;
            }
            self.n(node).next = NIL;
            self.n(node).prev = NIL;
            let index = self.node(node).index;
            *self.flag(index) &= !TILE_OPEN;
            self.set_map(index, NIL);
        }
        node
    }

    /// `GetOpenNode`: the index's node when it is open, else `NIL`.
    pub fn open_node(&self, index: i32) -> i32 {
        if self.flags_at(index) & TILE_OPEN != 0 {
            self.map(index)
        } else {
            NIL
        }
    }

    /// `GetClosedNode`: the index's node when it is closed, else `NIL`.
    pub fn closed_node(&self, index: i32) -> i32 {
        if self.flags_at(index) & TILE_CLOSED != 0 {
            self.map(index)
        } else {
            NIL
        }
    }

    /// Whether an open node is left (`EEAIAStarPathGraphGoal::SearchDone`'s test: the fast list, the sorted list,
    /// then every open bucket).
    pub fn has_open(&self) -> bool {
        if self.open_list != NIL || self.sorted_open != NIL {
            return true;
        }
        match &self.open_table {
            Some(t) => t.iter().any(|&h| h != NIL),
            None => false,
        }
    }

    /// The nodes of a list in its order.
    pub fn list(&self, which: GraphList) -> Vec<i32> {
        let mut out = Vec::new();
        let mut walk = |mut node: i32| {
            while node != NIL {
                out.push(node);
                node = self.node(node).next;
            }
        };
        match which {
            GraphList::Closed => walk(self.closed_list),
            GraphList::SortedOpen => walk(self.sorted_open),
            GraphList::FastOpen => walk(self.open_list),
            GraphList::ClosedTable | GraphList::OpenTable => {
                let table = if which == GraphList::ClosedTable { &self.closed_table } else { &self.open_table };
                if let Some(t) = table {
                    for i in (0..self.bucket_count).rev() {
                        walk(t[i as usize]);
                    }
                }
            }
        }
        out
    }
}

/// The goal and the graph objects, as the C++ machine sees them.
pub trait GraphHost {
    /// `G::GetStartObject()`
    fn start_object(&mut self) -> Obj;
    /// `G::GetGoalID()`
    fn goal_id(&mut self) -> i32;
    /// `G::Estimate(from, to)`
    fn estimate(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> i32;
    /// `G::IsGoalNode(node)` (`None`: the C++ NULL)
    fn is_goal_node(&mut self, node: Option<&GraphNodeView>) -> bool;
    /// `M::mID`
    fn object_id(&mut self, object: Obj) -> i32;
    /// `M::GetNumNeighbors()` (`mNeighbors.size()`)
    fn num_neighbors(&mut self, object: Obj) -> u32;
    /// `M::mNeighbors[i]`
    fn neighbor(&mut self, object: Obj, i: u32) -> Obj;
    /// `M::GetGraphLocation()`
    fn graph_location(&mut self, object: Obj) -> (i32, i32);
    /// `G::CanEnter(neighbor, from)`
    fn can_enter(&mut self, neighbor: Obj, from: Obj) -> bool;
    /// `G::GetStepCost(parent, object)`
    fn step_cost(&mut self, parent: &GraphNodeView, object: Obj) -> i32;
    /// `G::SearchDone(machine)`, given the current node and whether an open node is left (what the path goal reads)
    fn search_done(&mut self, current: Option<&GraphNodeView>, has_open: bool);
    /// `G::mSearchFailed`
    fn search_failed(&mut self) -> bool;
    /// `G::mRevLimit`, `G::mMaxRevs`
    fn rev_limit(&mut self) -> i32;
    fn max_revs(&mut self) -> i32;
    /// `G::mStartTile`, `G::mGoalTile`
    fn start_tile(&mut self) -> (i32, i32);
    fn goal_tile(&mut self) -> (i32, i32);
    /// The machine's members.
    fn state(&mut self) -> GraphMachineState;
    fn set_state(&mut self, state: &GraphMachineState);
}

fn with<H: GraphHost + ?Sized>(host: &mut H, f: impl FnOnce(&mut GraphMachineState)) {
    let mut s = host.state();
    f(&mut s);
    host.set_state(&s);
}

/// 0x27434 `Start`: the goal tile and sector as the machine's, the start node from the goal's start object.
pub fn start<H: GraphHost + ?Sized>(st: &mut GraphStorage, host: &mut H) -> bool {
    let goal = host.goal_tile();
    let sector = host.goal_id();
    with(host, |s| {
        s.tile_x = goal.0;
        s.tile_y = goal.1;
        s.sector = sector;
    });
    let object = host.start_object();
    let id = host.object_id(object);
    let (sx, sy) = host.start_tile();
    let node = st.new_node(sx, sy, object, id);
    st.current = node;
    st.nodes[node as usize].path_cost = 0;
    let state = host.state();
    let estimate = host.estimate(sx, sy, state.tile_x, state.tile_y);
    let n = &mut st.nodes[node as usize];
    n.estimate = estimate;
    n.cost = (n.estimate.wrapping_add(n.path_cost)) as u32;
    st.add_open(node);
    true
}

/// 0x274a3 `Expand`: pops the best open node until the goal, a limit or an empty open set, adding the neighbours
/// the goal lets it enter.
pub fn expand<H: GraphHost + ?Sized>(st: &mut GraphStorage, host: &mut H) {
    with(host, |s| {
        s.num_runs = s.num_runs.wrapping_add(1);
        s.revs = s.revs.wrapping_add(1);
        s.steps = 0;
    });
    loop {
        st.current = st.remove_best_open();
        let cur = st.current;
        let view = if cur == NIL { None } else { Some(st.view(cur)) };
        if host.is_goal_node(view.as_ref()) {
            with(host, |s| s.found = false);
            break;
        }
        let object = st.node(cur).object;
        let count = host.num_neighbors(object);
        if count > 0 {
            // `theObject->mID != -1` per neighbour: a plain read of the object, asked once here
            let id = host.object_id(object);
            for i in (0..count).rev() {
                let neighbor = host.neighbor(object, i);
                if id != -1 && host.can_enter(neighbor, object) {
                    add_node(st, host, cur, neighbor);
                }
            }
        }
        st.add_closed(cur);
        let rev_limit = host.rev_limit();
        if rev_limit != -1 && host.state().steps >= rev_limit {
            st.current = NIL;
            with(host, |s| s.found = true);
            break;
        }
        let max_revs = host.max_revs();
        if max_revs != -1 && host.state().revs >= max_revs {
            st.current = NIL;
            with(host, |s| s.found = false);
            break;
        }
        with(host, |s| {
            s.steps = s.steps.wrapping_add(1);
            s.revs = s.revs.wrapping_add(1);
        });
    }
    if !host.state().found {
        let cur = st.current;
        let view = if cur == NIL { None } else { Some(st.view(cur)) };
        let has_open = st.has_open();
        host.search_done(view.as_ref(), has_open);
    }
    if host.search_failed() {
        if st.current != NIL {
            let c = st.current;
            st.add_closed(c);
            st.current = NIL;
        }
        restart(st, host);
    } else if !host.state().found && st.current != NIL {
        st.path = st.current;
    } else {
        st.current = NIL;
    }
}

/// 0x28a34 `Restart`: the nearest remaining node as the path and the machine's tile and sector, else a new start
/// node (index -1, cost 1) as the path.
pub fn restart<H: GraphHost + ?Sized>(st: &mut GraphStorage, host: &mut H) -> bool {
    st.path = NIL;
    let node = next_node(st, host);
    if node != NIL {
        let n = *st.node(node);
        with(host, |s| {
            s.tile_x = n.x;
            s.tile_y = n.y;
            s.sector = n.index;
        });
        st.path = node;
        st.current = node;
    } else {
        let object = host.start_object();
        let (sx, sy) = host.start_tile();
        let node = st.new_node(sx, sy, object, -1);
        st.current = node;
        let n = &mut st.nodes[node as usize];
        n.path_cost = 0;
        n.estimate = 1;
        n.cost = 1;
        st.path = node;
    }
    with(host, |s| s.found = false);
    true
}

/// 0x2945f `NextNode`: the nearer of the nearest open and closed nodes to the machine's tile (the open one on a
/// tie); the other goes back to its list.
pub fn next_node<H: GraphHost + ?Sized>(st: &mut GraphStorage, host: &mut H) -> i32 {
    let s = host.state();
    let mut open = st.remove_nearest_open(s.tile_x, s.tile_y);
    let closed = st.remove_nearest_closed(s.tile_x, s.tile_y);
    if open != NIL {
        if closed != NIL {
            if st.node(open).cost <= st.node(closed).cost {
                st.add_closed(closed);
            } else {
                st.add_open(open);
                open = closed;
            }
        }
    } else {
        open = closed;
    }
    open
}

/// 0x28aa7 `AddNode`: a new open node for the object unless it is known at no more cost.
pub fn add_node<H: GraphHost + ?Sized>(st: &mut GraphStorage, host: &mut H, parent: i32, object: Obj) {
    let parent_view = st.view(parent);
    let step = host.step_cost(&parent_view, object);
    // `unsigned long theCost = inParent->mPathCost + theStep`
    let cost = parent_view.path_cost.wrapping_add(step) as u32;
    let (x, y) = host.graph_location(object);
    let index = host.object_id(object);
    let open = st.open_node(index);
    let closed = st.closed_node(index);
    // `theOpen->mPathCost <= theCost`: a long against an unsigned long, compared unsigned
    if open != NIL && st.node(open).path_cost as u32 <= cost {
        return;
    }
    if closed != NIL {
        if st.node(closed).path_cost as u32 <= cost {
            return;
        }
        st.remove_closed(closed);
        st.release_node(closed);
    }
    if open == NIL {
        let node = st.new_node(x, y, object, index);
        st.nodes[node as usize].path_cost = cost as i32;
        st.nodes[node as usize].parent = parent;
        let s = host.state();
        let estimate = host.estimate(x, y, s.tile_x, s.tile_y);
        let n = &mut st.nodes[node as usize];
        n.estimate = estimate;
        n.cost = (n.estimate as u32).wrapping_add(cost);
        st.add_open(node);
    } else {
        let n = &mut st.nodes[open as usize];
        n.parent = parent;
        let total = (n.estimate as u32).wrapping_add(cost);
        n.path_cost = cost as i32;
        if n.cost != total {
            n.cost = total;
            st.update_open(open);
        }
    }
}
