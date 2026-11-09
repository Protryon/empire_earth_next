//! The node storage of the tile searches (`EEAIAStarStorage.cpp`, `EEAIAStarFastStorage.cpp`,
//! `EEAIAStarTerrainStorage.cpp`, `EEAIAStarBoundedStorage.cpp` and the flood storages' `PrepareGrid`), a line-by-line
//! port. Nodes live in an arena and link by index (`NIL` for the C++ NULL); the free list is per storage (the C++'s
//! was one static list shared by every storage: which free node a search reuses is not observable, since `NewNode`
//! clears every field).
//!
//! The variants differ in a few members (the machine calls the most derived one of its storage `S`):
//!
//! | member | `Hashed` (`EEAIAStarStorage`) | `Bounded` | `Terrain` (the flood fills) |
//! |---|---|---|---|
//! | `add_open` | sorted list or open bucket | `Hashed`'s + node map | front of the fast open list + node map |
//! | `remove_best_open` | sorted list (refilled from the buckets) | `Hashed`'s + node map | front of the fast open list |
//! | `add_closed` / `remove_closed` | closed bucket | fast closed list + node map | fast closed list + node map |
//! | `find_open` / `find_closed` | the list / bucket walk | node map | node map |
//!
//! `update_open`, the nearest-node removals and `insert_sorted` are the base's in every variant.

/// The C++ NULL of a node link.
pub(crate) const NIL: i32 = -1;

/// `EEAIKindIDs.h`'s per-tile flags (`EEAIAStarTileFlag`).
pub(crate) const TILE_ACCEPTED: u8 = 1;
pub(crate) const TILE_REJECTED: u8 = 2;
pub(crate) const TILE_OPEN: u8 = 4;
pub(crate) const TILE_CLOSED: u8 = 8;

/// The storage's hash bucket count (`mBucketCount`, 0x35).
pub(crate) const BUCKETS: i32 = 0x35;

/// `EEAIAStarNode`: the links are arena indices.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Node {
    pub parent: i32,
    pub next: i32,
    pub prev: i32,
    pub cost: u32,
    pub path_cost: i32,
    pub estimate: i32,
    pub x: i32,
    pub y: i32,
}

/// `U2DRectangle<long>` (`EEAIAStarBounds`): the member order of the C++ (bottom, left, right, top).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct AStarRect {
    pub bottom: i32,
    pub left: i32,
    pub right: i32,
    pub top: i32,
}

impl AStarRect {
    pub fn clear(&mut self) {
        *self = AStarRect::default();
    }
    pub fn extend(&mut self, x: i32, y: i32) {
        self.left = self.left.min(x);
        self.right = self.right.max(x);
        self.top = self.top.min(y);
        self.bottom = self.bottom.max(y);
    }
}

/// Which members the storage's machine calls (see the module table).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StorageKind {
    Hashed = 0,
    Bounded = 1,
    Terrain = 2,
}

/// Which `PrepareGrid` the storage has (its `+0x10` slot).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GridKind {
    /// `EEAIAStarStorage`: the flags; the last search's bounds grown by two when they are valid
    Hashed = 0,
    /// `EEAIAStarFastStorage`: the flags and the node map, all of them
    Fast = 1,
    /// `EEAIAStarBoundedStorage`: the flags and the node map; the last search's bounds grown by two
    Bounded = 2,
    /// the flood storages (choke point, continent, hill, path area, path sector): the flags and the node map; the
    /// dirty bounds grown by one when they are valid and the grid was not just allocated
    Dirty = 3,
}

/// The storage of one machine.
#[derive(Clone, Debug)]
pub struct Storage {
    pub kind: StorageKind,
    pub nodes: Vec<Node>,
    pub free: i32,
    pub bounds: AStarRect,
    pub closed_table: Option<Vec<i32>>,
    pub open_table: Option<Vec<i32>>,
    pub fast_closed: i32,
    pub fast_open: i32,
    pub sorted_open: i32,
    pub sorted_tail: i32,
    pub path: i32,
    pub path_cursor: i32,
    pub current: i32,
    pub flags: Option<Vec<u8>>,
    pub width: i32,
    pub height: i32,
    pub bucket_count: i32,
    pub has_bounds: bool,
    /// `EEAIAStarFastStorage::mNodes` (`None`: not allocated)
    pub node_map: Option<Vec<i32>>,
    /// `EEAIAStarTerrainStorage::mDirtyBounds`
    pub dirty: AStarRect,
}

impl Storage {
    /// The constructors: `EEAIAStarStorage` (no grid, no lists, 53 buckets), then for the terrain storages
    /// `EEAIAStarTerrainStorage::Reset` (the dirty bounds over the whole, still empty, grid).
    pub fn new(kind: StorageKind) -> Storage {
        let mut s = Storage {
            kind,
            nodes: Vec::new(),
            free: NIL,
            bounds: AStarRect::default(),
            closed_table: None,
            open_table: None,
            fast_closed: NIL,
            fast_open: NIL,
            sorted_open: NIL,
            sorted_tail: NIL,
            path: NIL,
            path_cursor: NIL,
            current: NIL,
            flags: None,
            width: 0,
            height: 0,
            bucket_count: BUCKETS,
            has_bounds: false,
            node_map: None,
            dirty: AStarRect::default(),
        };
        if kind == StorageKind::Terrain {
            s.terrain_reset();
        }
        s
    }

    /// `EEAIAStarTerrainStorage::Reset`
    pub fn terrain_reset(&mut self) {
        self.dirty.top = 0;
        self.dirty.left = 0;
        self.dirty.bottom = self.height - 1;
        self.dirty.right = self.width - 1;
    }

    pub fn node(&self, i: i32) -> &Node {
        &self.nodes[i as usize]
    }
    fn n(&mut self, i: i32) -> &mut Node {
        &mut self.nodes[i as usize]
    }
    fn index(&self, x: i32, y: i32) -> usize {
        (self.width.wrapping_mul(x).wrapping_add(y)) as usize
    }
    fn flag(&mut self, x: i32, y: i32) -> &mut u8 {
        let i = self.index(x, y);
        &mut self.flags.as_mut().expect("the tile flags")[i]
    }
    pub fn tile_flags(&self, x: i32, y: i32) -> u8 {
        self.flags.as_ref().expect("the tile flags")[self.index(x, y)]
    }
    fn set_map(&mut self, x: i32, y: i32, node: i32) {
        let i = self.index(x, y);
        self.node_map.as_mut().expect("the node map")[i] = node;
    }
    fn map(&self, x: i32, y: i32) -> i32 {
        self.node_map.as_ref().expect("the node map")[self.index(x, y)]
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

    /// The inline hash of a tile (`GetBucket`; PJW-like, the high nibble's fold replaces the hash, as in the C++).
    pub fn bucket(&self, x: i32, y: i32) -> i32 {
        let mut h = x.wrapping_shl(4).wrapping_add(y);
        let high = h & (0xf000_0000u32 as i32);
        if high != 0 {
            h = (high >> 24) ^ high;
        }
        h % self.bucket_count
    }

    // ---- the node pool (0x1b991, 0x1ba9b, 0x1bc68) ----

    /// `ReleaseNode`: the node onto the free list.
    pub fn release_node(&mut self, node: i32) {
        let free = self.free;
        self.n(node).next = free;
        self.free = node;
    }

    /// `NewNode`: a cleared node for the tile, from the free list or new.
    pub fn new_node(&mut self, x: i32, y: i32) -> i32 {
        let node = if self.free != NIL {
            let node = self.free;
            self.free = self.node(node).next;
            node
        } else {
            self.nodes.push(Node::default());
            (self.nodes.len() - 1) as i32
        };
        *self.n(node) = Node { parent: NIL, next: NIL, prev: NIL, cost: 0, path_cost: 0, estimate: 0, x, y };
        node
    }

    /// `FreeNodePool` has nothing to free here: the arena goes with the storage. The free nodes stay reusable.
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

    // ---- EEAIAStarStorage ----

    /// `AddClosed` of the variant.
    pub fn add_closed(&mut self, node: i32) {
        match self.kind {
            StorageKind::Hashed => self.hashed_add_closed(node),
            StorageKind::Bounded | StorageKind::Terrain => self.fast_add_closed(node),
        }
    }

    /// 0x1ba43
    fn hashed_add_closed(&mut self, node: i32) {
        let (x, y) = (self.node(node).x, self.node(node).y);
        let b = self.bucket(x, y);
        let head = self.bucket_head(true, b);
        if head != NIL {
            self.n(head).prev = node;
        }
        self.n(node).next = head;
        self.set_bucket_head(true, b, node);
        *self.flag(x, y) |= TILE_CLOSED;
    }

    /// `AddOpen` of the variant.
    pub fn add_open(&mut self, node: i32) {
        match self.kind {
            StorageKind::Hashed => self.hashed_add_open(node),
            StorageKind::Bounded => {
                self.hashed_add_open(node);
                let (x, y) = (self.node(node).x, self.node(node).y);
                self.set_map(x, y, node);
            }
            StorageKind::Terrain => self.terrain_add_open(node),
        }
    }

    /// 0x1baae: into the sorted list when it costs no more than its tail, else onto its open bucket
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
        let (x, y) = (self.node(node).x, self.node(node).y);
        if !inserted {
            let b = self.bucket(x, y);
            let head = self.bucket_head(false, b);
            if head != NIL {
                self.n(head).prev = node;
            }
            self.n(node).next = head;
            self.set_bucket_head(false, b, node);
        }
        *self.flag(x, y) |= TILE_OPEN;
    }

    /// 0x1bb52 `ResetNodes`
    pub fn reset_nodes(&mut self) {
        self.clear();
        self.path_cursor = NIL;
        self.path = NIL;
    }

    /// `FindClosed` of the variant.
    pub fn find_closed(&self, x: i32, y: i32) -> i32 {
        if self.kind != StorageKind::Hashed {
            return if self.tile_flags(x, y) & TILE_CLOSED != 0 { self.map(x, y) } else { NIL };
        }
        // 0x1bb64
        let mut node = NIL;
        if self.tile_flags(x, y) & TILE_CLOSED != 0 {
            node = self.bucket_head(true, self.bucket(x, y));
            while node != NIL {
                if self.node(node).x == x && self.node(node).y == y {
                    break;
                }
                node = self.node(node).next;
            }
        }
        node
    }

    /// `FindOpen` of the variant.
    pub fn find_open(&self, x: i32, y: i32) -> i32 {
        if self.kind != StorageKind::Hashed {
            return if self.tile_flags(x, y) & TILE_OPEN != 0 { self.map(x, y) } else { NIL };
        }
        // 0x1bbb9
        let mut node = NIL;
        if self.tile_flags(x, y) & TILE_OPEN != 0 {
            node = self.sorted_open;
            while node != NIL {
                if self.node(node).x == x && self.node(node).y == y {
                    break;
                }
                node = self.node(node).next;
            }
            if node == NIL {
                node = self.bucket_head(false, self.bucket(x, y));
                while node != NIL {
                    if self.node(node).x == x && self.node(node).y == y {
                        break;
                    }
                    node = self.node(node).next;
                }
            }
        }
        node
    }

    /// 0x1bc2a `GetFirstPathTile`
    pub fn first_path_tile(&mut self) -> Option<(i32, i32)> {
        if self.path == NIL {
            return None;
        }
        self.path_cursor = self.path;
        if self.node(self.path).parent != NIL {
            self.next_path_tile()
        } else {
            let p = *self.node(self.path);
            self.path_cursor = NIL;
            Some((p.x, p.y))
        }
    }

    /// 0x1bcc3 `GetNextPathTile` (`None` stands for false and (0, 0))
    pub fn next_path_tile(&mut self) -> Option<(i32, i32)> {
        if self.path_cursor != NIL {
            let p = *self.node(self.path_cursor);
            self.path_cursor = p.parent;
            return Some((p.x, p.y));
        }
        None
    }

    /// 0x1bcfe `Clear`: the current node and every list onto the free list, the hash tables freed
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
        if self.fast_closed != NIL {
            let head = self.fast_closed;
            self.splice_free(head);
            self.fast_closed = NIL;
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
        if self.fast_open == NIL {
            return;
        }
        let head = self.fast_open;
        self.splice_free(head);
        self.fast_open = NIL;
    }

    /// The first part of 0x1be27 `Init`: the size, then `Clear` (the caller runs its `PrepareGrid`, then
    /// `alloc_tables`).
    pub fn init_begin(&mut self, width: i32, height: i32) {
        self.width = width;
        self.height = height;
        self.clear();
    }

    /// The last part of `Init`: empty hash tables.
    pub fn alloc_tables(&mut self) {
        self.closed_table = Some(vec![NIL; self.bucket_count as usize]);
        self.open_table = Some(vec![NIL; self.bucket_count as usize]);
    }

    /// `Init` with this storage's own `PrepareGrid` (the C++ machines call the virtual slot; this is for tests and
    /// for storages whose slot has no C++ arrays).
    pub fn init(&mut self, width: i32, height: i32, grid: GridKind) {
        self.init_begin(width, height);
        self.prepare_grid(width, height, grid, false);
        self.alloc_tables();
    }

    /// 0x1be86 `UpdateOpen`
    pub fn update_open(&mut self, node: i32) {
        let (x, y, cost) = (self.node(node).x, self.node(node).y, self.node(node).cost);
        let b = self.bucket(x, y);
        let (prev, next) = (self.node(node).prev, self.node(node).next);
        if prev != NIL {
            self.n(prev).next = next;
            if next != NIL {
                self.n(next).prev = prev;
            }
        } else if self.bucket_head(false, b) == node {
            let head = self.node(node).next;
            self.set_bucket_head(false, b, head);
            if head != NIL {
                self.n(head).prev = NIL;
            }
        } else if self.sorted_open == node {
            self.sorted_open = self.node(node).next;
            if self.sorted_open != NIL {
                let s = self.sorted_open;
                self.n(s).prev = NIL;
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
                    let s = self.sorted_open;
                    self.n(s).prev = node;
                    self.n(node).next = s;
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

    fn manhattan(&self, node: i32, x: i32, y: i32) -> i32 {
        let n = self.node(node);
        n.x.wrapping_sub(x).wrapping_abs().wrapping_add(n.y.wrapping_sub(y).wrapping_abs())
    }

    /// 0x1bf8b `RemoveNearestClosed`
    pub fn remove_nearest_closed(&mut self, x: i32, y: i32) -> i32 {
        let mut best = -1;
        let mut result = NIL;
        let mut node = self.fast_closed;
        while node != NIL {
            let d = self.manhattan(node, x, y);
            if best == -1 || d < best {
                result = node;
                best = d;
            }
            node = self.node(node).next;
        }
        for i in (0..self.bucket_count).rev() {
            let mut node = self.bucket_head(true, i);
            while node != NIL {
                let d = self.manhattan(node, x, y);
                if best == -1 || d < best {
                    result = node;
                    best = d;
                }
                node = self.node(node).next;
            }
        }
        if result != NIL {
            let (prev, next) = (self.node(result).prev, self.node(result).next);
            if prev != NIL {
                self.n(prev).next = next;
                if next != NIL {
                    self.n(next).prev = prev;
                }
            } else {
                let b = self.bucket(self.node(result).x, self.node(result).y);
                if result == self.bucket_head(true, b) {
                    self.set_bucket_head(true, b, next);
                } else {
                    self.fast_closed = next;
                }
                if next != NIL {
                    self.n(next).prev = NIL;
                }
            }
            self.n(result).next = NIL;
            self.n(result).prev = NIL;
            let (rx, ry) = (self.node(result).x, self.node(result).y);
            *self.flag(rx, ry) &= !TILE_CLOSED;
        }
        result
    }

    /// 0x1c0b6 `RemoveNearestOpen`
    pub fn remove_nearest_open(&mut self, x: i32, y: i32) -> i32 {
        let mut best = -1;
        let mut result = NIL;
        for head in [self.sorted_open, self.fast_open] {
            let mut node = head;
            while node != NIL {
                let d = self.manhattan(node, x, y);
                if best == -1 || d < best {
                    result = node;
                    best = d;
                }
                node = self.node(node).next;
            }
        }
        for i in (0..self.bucket_count).rev() {
            let mut node = self.bucket_head(false, i);
            while node != NIL {
                let d = self.manhattan(node, x, y);
                if best == -1 || d < best {
                    result = node;
                    best = d;
                }
                node = self.node(node).next;
            }
        }
        if result != NIL {
            let (prev, next) = (self.node(result).prev, self.node(result).next);
            if prev != NIL {
                self.n(prev).next = next;
                if next != NIL {
                    self.n(next).prev = prev;
                }
            } else {
                let b = self.bucket(self.node(result).x, self.node(result).y);
                if self.bucket_head(false, b) == result {
                    if next != NIL {
                        self.n(next).prev = NIL;
                    }
                    self.set_bucket_head(false, b, next);
                } else if self.fast_open == result {
                    if next != NIL {
                        self.n(next).prev = NIL;
                    }
                    self.fast_open = next;
                } else if self.sorted_open == result {
                    if next != NIL {
                        self.n(next).prev = NIL;
                    }
                    self.sorted_open = next;
                    if self.sorted_tail == result {
                        // as the C++: the head's prev, which is NULL here
                        self.sorted_open = self.node(result).prev;
                    }
                }
            }
            if self.sorted_tail == result {
                self.sorted_tail = self.node(result).prev;
            }
            self.n(result).next = NIL;
            self.n(result).prev = NIL;
            let (rx, ry) = (self.node(result).x, self.node(result).y);
            *self.flag(rx, ry) &= !TILE_OPEN;
        }
        result
    }

    /// `RemoveBestOpen` of the variant.
    pub fn remove_best_open(&mut self) -> i32 {
        match self.kind {
            StorageKind::Hashed => self.hashed_remove_best_open(),
            StorageKind::Bounded => {
                let node = self.hashed_remove_best_open();
                if node != NIL {
                    let (x, y) = (self.node(node).x, self.node(node).y);
                    self.set_map(x, y, NIL);
                }
                node
            }
            StorageKind::Terrain => self.terrain_remove_best_open(),
        }
    }

    /// 0x1c268: the cheapest open node; an empty sorted list first takes the 15 cheapest bucket nodes
    fn hashed_remove_best_open(&mut self) -> i32 {
        let mut result = NIL;
        if self.sorted_open == NIL {
            let mut best = [NIL; 15];
            let mut worst: i32 = -1;
            let mut worst_cost: u32 = 0;
            let mut seen: i32 = 0;
            let mut filled: i32 = 0;
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
                        let b = self.bucket(self.node(node).x, self.node(node).y);
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
                let s = self.sorted_open;
                self.n(s).prev = NIL;
            }
            if self.sorted_open == NIL {
                self.sorted_tail = NIL;
            }
            self.n(result).next = NIL;
            self.n(result).prev = NIL;
            let (x, y) = (self.node(result).x, self.node(result).y);
            *self.flag(x, y) &= !TILE_OPEN;
        }
        result
    }

    /// `RemoveClosed` of the variant.
    pub fn remove_closed(&mut self, node: i32) {
        match self.kind {
            StorageKind::Hashed => self.hashed_remove_closed(node),
            StorageKind::Bounded | StorageKind::Terrain => self.fast_remove_closed(node),
        }
    }

    /// 0x1c420
    fn hashed_remove_closed(&mut self, node: i32) {
        let (x, y) = (self.node(node).x, self.node(node).y);
        let b = self.bucket(x, y);
        let (prev, next) = (self.node(node).prev, self.node(node).next);
        if prev != NIL {
            self.n(prev).next = next;
            if next != NIL {
                self.n(next).prev = prev;
            }
        } else {
            self.set_bucket_head(true, b, next);
            if next != NIL {
                self.n(next).prev = NIL;
            }
        }
        self.n(node).next = NIL;
        self.n(node).prev = NIL;
        *self.flag(x, y) &= !TILE_CLOSED;
    }

    /// `PrepareGrid` of the given slot, for the storage's own arrays (the flags, the node map). `resized` is the
    /// flood storages' "the C++ arrays were just allocated" (their own grid's allocation decides it).
    pub fn prepare_grid(&mut self, width: i32, height: i32, grid: GridKind, resized: bool) {
        let size = (width as usize).wrapping_mul(height as usize);
        match grid {
            GridKind::Hashed => {
                // 0x1c491
                let bounds = self.bounds;
                self.width = width;
                self.height = height;
                if self.flags.as_ref().map(|f| f.len()) != Some(size) {
                    self.flags = Some(vec![0; size]);
                }
                let flags = self.flags.as_mut().expect("the tile flags");
                if self.has_bounds && bounds.left <= bounds.right && bounds.top <= bounds.bottom {
                    let first_y = if bounds.top - 2 >= 0 { bounds.top - 2 } else { 0 };
                    let first_x = if bounds.left - 2 >= 0 { bounds.left - 2 } else { 0 };
                    let last_x = (self.width - 1).min(bounds.right + 2);
                    let last_y = (self.height - 1).min(bounds.bottom + 2);
                    let start = self.width * first_x + first_y;
                    let count = self.width * last_x - start + last_y;
                    fill(flags, start, count, 0);
                } else {
                    flags.fill(0);
                }
                self.has_bounds = false;
                self.bounds.clear();
            }
            GridKind::Fast => {
                // 0x15d1c
                self.width = width;
                self.height = height;
                self.alloc_grid(size);
                self.node_map.as_mut().expect("the node map").fill(NIL);
                self.flags.as_mut().expect("the tile flags").fill(0);
            }
            GridKind::Bounded => {
                // 0x10c0e
                let bounds = self.bounds;
                self.width = width;
                self.height = height;
                self.alloc_grid(size);
                if self.has_bounds && bounds.left <= bounds.right && bounds.top <= bounds.bottom {
                    let first_column = if bounds.top - 2 < 0 { 0 } else { bounds.top - 2 };
                    let first_row = if bounds.left - 2 < 0 { 0 } else { bounds.left - 2 };
                    let last_row = (self.width - 1).min(bounds.right + 2);
                    let last_column = (self.height - 1).min(bounds.bottom + 2);
                    let start = self.width * first_row + first_column;
                    let count = self.width * last_row - start + last_column;
                    fill(self.node_map.as_mut().expect("the node map"), start, count, NIL);
                    fill(self.flags.as_mut().expect("the tile flags"), start, count, 0);
                } else {
                    self.node_map.as_mut().expect("the node map").fill(NIL);
                    self.flags.as_mut().expect("the tile flags").fill(0);
                }
                self.has_bounds = false;
                self.bounds.clear();
            }
            GridKind::Dirty => {
                // 0x1b829 / 0x15a99 / 0x17239 / 0x1545a
                self.width = width;
                self.height = height;
                let resized = self.alloc_grid(size) || resized;
                let d = &mut self.dirty;
                if !resized && d.left <= d.right && d.top <= d.bottom {
                    d.top -= 1;
                    if d.top < 0 {
                        d.top = 0;
                    }
                    d.left -= 1;
                    if d.left < 0 {
                        d.left = 0;
                    }
                    d.right = (width - 1).min(d.right + 1);
                    d.bottom = (height - 1).min(d.bottom + 1);
                    let start = d.left * width + d.top;
                    let count = width * d.right + d.bottom - start;
                    fill(self.node_map.as_mut().expect("the node map"), start, count, NIL);
                    fill(self.flags.as_mut().expect("the tile flags"), start, count, 0);
                } else {
                    self.node_map.as_mut().expect("the node map").fill(NIL);
                    self.flags.as_mut().expect("the tile flags").fill(0);
                }
                self.has_bounds = false;
                self.dirty.clear();
            }
        }
    }

    /// The flags and the node map at `size` tiles; true when they were (re)allocated.
    fn alloc_grid(&mut self, size: usize) -> bool {
        let ok =
            self.flags.as_ref().map(|f| f.len()) == Some(size) && self.node_map.as_ref().map(|m| m.len()) == Some(size);
        if !ok {
            self.flags = Some(vec![0; size]);
            self.node_map = Some(vec![NIL; size]);
        }
        !ok
    }

    /// 0x1c586 `MarkTile`: an on-grid tile accepted or rejected in advance, inside the bounds
    pub fn mark_tile(&mut self, x: u32, y: u32, accept: bool) {
        if !(x < self.width as u32) || !(y < self.height as u32) {
            return;
        }
        let f = self.flag(x as i32, y as i32);
        *f |= if accept { TILE_ACCEPTED } else { TILE_REJECTED };
        self.extend_bounds(x as i32, y as i32);
    }

    /// 0x1c5bd `InsertSorted`
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
                let p = self.node(at).prev;
                if p != NIL {
                    self.n(p).next = node;
                    self.n(node).next = at;
                    self.n(node).prev = p;
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

    /// 0x1c617 `ExtendBounds`: started at the tile while all four edges are 0
    pub fn extend_bounds(&mut self, x: i32, y: i32) {
        let b = &mut self.bounds;
        if b.top == 0 && b.left == 0 && b.right == 0 && b.bottom == 0 {
            b.top = y;
            b.left = x;
            b.bottom = y;
            b.right = x;
            self.has_bounds = true;
        } else {
            b.extend(x, y);
        }
    }

    // ---- EEAIAStarFastStorage ----

    /// 0x15c83
    fn fast_add_closed(&mut self, node: i32) {
        let head = self.fast_closed;
        if head != NIL {
            self.n(head).prev = node;
        }
        self.n(node).next = head;
        self.fast_closed = node;
        let (x, y) = (self.node(node).x, self.node(node).y);
        *self.flag(x, y) |= TILE_CLOSED;
        self.set_map(x, y, node);
    }

    /// 0x15cbd
    fn fast_remove_closed(&mut self, node: i32) {
        let (prev, next) = (self.node(node).prev, self.node(node).next);
        if prev != NIL {
            self.n(prev).next = next;
            if next != NIL {
                self.n(next).prev = prev;
            }
        } else {
            self.fast_closed = next;
            if next != NIL {
                self.n(next).prev = NIL;
            }
        }
        self.n(node).next = NIL;
        self.n(node).prev = NIL;
        let (x, y) = (self.node(node).x, self.node(node).y);
        *self.flag(x, y) &= !TILE_CLOSED;
        self.set_map(x, y, NIL);
    }

    // ---- EEAIAStarTerrainStorage ----

    /// 0x1c6b4
    fn terrain_add_open(&mut self, node: i32) {
        let (x, y) = (self.node(node).x, self.node(node).y);
        *self.flag(x, y) |= TILE_OPEN;
        let head = self.fast_open;
        if head != NIL {
            self.n(head).prev = node;
        }
        self.n(node).next = head;
        self.fast_open = node;
        self.set_map(x, y, node);
    }

    /// 0x1c705: the most recently opened node
    fn terrain_remove_best_open(&mut self) -> i32 {
        let mut node = NIL;
        if self.fast_open != NIL {
            node = self.fast_open;
            self.fast_open = self.node(node).next;
            self.n(node).next = NIL;
            self.n(node).prev = NIL;
            let (x, y) = (self.node(node).x, self.node(node).y);
            *self.flag(x, y) &= !TILE_OPEN;
            self.set_map(x, y, NIL);
        }
        node
    }

    // ---- what the goals read ----

    /// Whether any open node is left (the path goals' `SearchDone`).
    pub fn has_open(&self) -> bool {
        if self.fast_open != NIL || self.sorted_open != NIL {
            return true;
        }
        match self.open_table.as_deref() {
            Some(t) => (0..self.bucket_count).rev().any(|i| t[i as usize] != NIL),
            None => false,
        }
    }

    /// The nodes of a list in its order (`List`).
    pub fn list(&self, which: List) -> Vec<i32> {
        let mut out = Vec::new();
        let walk = |mut node: i32, out: &mut Vec<i32>| {
            while node != NIL {
                out.push(node);
                node = self.node(node).next;
            }
        };
        match which {
            List::SortedOpen => walk(self.sorted_open, &mut out),
            List::FastOpen => walk(self.fast_open, &mut out),
            List::FastClosed => walk(self.fast_closed, &mut out),
            List::OpenTable | List::ClosedTable => {
                if let Some(t) =
                    if which == List::OpenTable { self.open_table.as_deref() } else { self.closed_table.as_deref() }
                {
                    for i in (0..self.bucket_count).rev() {
                        walk(t[i as usize], &mut out);
                    }
                }
            }
        }
        out
    }
}

/// The storage's lists, for the goals' walks and the capture.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum List {
    SortedOpen = 0,
    FastOpen = 1,
    FastClosed = 2,
    /// every open bucket, the last bucket first
    OpenTable = 3,
    /// every closed bucket, the last bucket first
    ClosedTable = 4,
}

/// `memset(base + start, value, count)` (the C++ counts in elements; a count past the end or below 0 would be the
/// C++'s overrun: a panic here)
fn fill<T: Copy>(data: &mut [T], start: i32, count: i32, value: T) {
    if count <= 0 {
        assert!(count == 0, "PrepareGrid: a negative clear count");
        return;
    }
    data[start as usize..(start + count) as usize].fill(value);
}
