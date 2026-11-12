//! A tile search's storage as bytes, for the saved game (port, save version 0.276: the path manager's searches in
//! progress, `EEPathManager::SaveSearches`). The original saved no search: a loaded game asked each unit's path
//! again from its first tick, so the path came later (or another one) than in the game that was saved.
//!
//! The format is the storage's fields in their declaration order, little-endian: `i32`/`u32` as four bytes, `bool`
//! and `u8` as one, a `Vec` as its length (`u32`) and its elements, an `Option<Vec>` as a presence byte and then the
//! vector. `from_bytes` takes only what `to_bytes` wrote: a short or long buffer, a wrong version or kind, a link
//! out of the arena, or tables of the wrong size give `None`.

use crate::storage::{AStarRect, Node, Storage, StorageKind, NIL};

const VERSION: u32 = 1;

struct Writer(Vec<u8>);

impl Writer {
    fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    fn i32(&mut self, v: i32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    fn bool(&mut self, v: bool) {
        self.0.push(v as u8);
    }
    fn rect(&mut self, r: &AStarRect) {
        self.i32(r.bottom);
        self.i32(r.left);
        self.i32(r.right);
        self.i32(r.top);
    }
    fn ints(&mut self, v: &Option<Vec<i32>>) {
        match v {
            None => self.bool(false),
            Some(v) => {
                self.bool(true);
                self.u32(v.len() as u32);
                for &x in v {
                    self.i32(x);
                }
            }
        }
    }
    fn bytes(&mut self, v: &Option<Vec<u8>>) {
        match v {
            None => self.bool(false),
            Some(v) => {
                self.bool(true);
                self.u32(v.len() as u32);
                self.0.extend_from_slice(v);
            }
        }
    }
}

struct Reader<'a> {
    data: &'a [u8],
    at: usize,
}

impl Reader<'_> {
    fn take(&mut self, n: usize) -> Option<&[u8]> {
        let end = self.at.checked_add(n)?;
        let s = self.data.get(self.at..end)?;
        self.at = end;
        Some(s)
    }
    fn u32(&mut self) -> Option<u32> {
        Some(u32::from_le_bytes(self.take(4)?.try_into().ok()?))
    }
    fn i32(&mut self) -> Option<i32> {
        Some(i32::from_le_bytes(self.take(4)?.try_into().ok()?))
    }
    fn bool(&mut self) -> Option<bool> {
        match self.take(1)?[0] {
            0 => Some(false),
            1 => Some(true),
            _ => None,
        }
    }
    fn len(&mut self) -> Option<usize> {
        let n = self.u32()? as usize;
        // every element takes at least one byte: a count past the end is a bad buffer, not an allocation
        if n > self.data.len() - self.at {
            return None;
        }
        Some(n)
    }
    fn rect(&mut self) -> Option<AStarRect> {
        Some(AStarRect { bottom: self.i32()?, left: self.i32()?, right: self.i32()?, top: self.i32()? })
    }
    fn ints(&mut self) -> Option<Option<Vec<i32>>> {
        if !self.bool()? {
            return Some(None);
        }
        let n = self.len()?;
        let mut v = Vec::with_capacity(n);
        for _ in 0..n {
            v.push(self.i32()?);
        }
        Some(Some(v))
    }
    fn bytes(&mut self) -> Option<Option<Vec<u8>>> {
        if !self.bool()? {
            return Some(None);
        }
        let n = self.len()?;
        Some(Some(self.take(n)?.to_vec()))
    }
}

fn kind_code(kind: StorageKind) -> u32 {
    kind as u32
}

impl Storage {
    /// The storage as bytes (the module's format).
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut w = Writer(Vec::new());
        w.u32(VERSION);
        w.u32(kind_code(self.kind));
        w.u32(self.nodes.len() as u32);
        for n in &self.nodes {
            w.i32(n.parent);
            w.i32(n.next);
            w.i32(n.prev);
            w.u32(n.cost);
            w.i32(n.path_cost);
            w.i32(n.estimate);
            w.i32(n.x);
            w.i32(n.y);
        }
        w.i32(self.free);
        w.rect(&self.bounds);
        w.ints(&self.closed_table);
        w.ints(&self.open_table);
        for v in [
            self.fast_closed,
            self.fast_open,
            self.sorted_open,
            self.sorted_tail,
            self.path,
            self.path_cursor,
            self.current,
        ] {
            w.i32(v);
        }
        w.bytes(&self.flags);
        w.i32(self.width);
        w.i32(self.height);
        w.i32(self.bucket_count);
        w.bool(self.has_bounds);
        w.ints(&self.node_map);
        w.rect(&self.dirty);
        w.0
    }

    /// A storage of `kind` from `to_bytes`' bytes; `None` for any other bytes (see the module's comment).
    pub fn from_bytes(kind: StorageKind, data: &[u8]) -> Option<Storage> {
        let mut r = Reader { data, at: 0 };
        if r.u32()? != VERSION || r.u32()? != kind_code(kind) {
            return None;
        }
        let count = r.len()?;
        let mut nodes = Vec::with_capacity(count);
        for _ in 0..count {
            nodes.push(Node {
                parent: r.i32()?,
                next: r.i32()?,
                prev: r.i32()?,
                cost: r.u32()?,
                path_cost: r.i32()?,
                estimate: r.i32()?,
                x: r.i32()?,
                y: r.i32()?,
            });
        }
        let mut s = Storage::new(kind);
        s.nodes = nodes;
        s.free = r.i32()?;
        s.bounds = r.rect()?;
        s.closed_table = r.ints()?;
        s.open_table = r.ints()?;
        s.fast_closed = r.i32()?;
        s.fast_open = r.i32()?;
        s.sorted_open = r.i32()?;
        s.sorted_tail = r.i32()?;
        s.path = r.i32()?;
        s.path_cursor = r.i32()?;
        s.current = r.i32()?;
        s.flags = r.bytes()?;
        s.width = r.i32()?;
        s.height = r.i32()?;
        s.bucket_count = r.i32()?;
        s.has_bounds = r.bool()?;
        s.node_map = r.ints()?;
        s.dirty = r.rect()?;
        if r.at != data.len() || !s.links_valid() {
            return None;
        }
        Some(s)
    }

    /// Every link (the nodes', the lists', the tables') is `NIL` or inside the arena, and the per-tile tables hold
    /// width x height entries.
    fn links_valid(&self) -> bool {
        let n = self.nodes.len() as i64;
        let ok = |i: i32| i == NIL || (i >= 0 && (i as i64) < n);
        let tiles = (self.width.max(0) as i64) * (self.height.max(0) as i64);
        let table_ok = |t: &Option<Vec<i32>>, len: Option<i64>| match t {
            None => true,
            Some(v) => v.iter().all(|&i| ok(i)) && len.is_none_or(|l| v.len() as i64 == l),
        };
        self.nodes.iter().all(|x| ok(x.parent) && ok(x.next) && ok(x.prev))
            && [
                self.free,
                self.fast_closed,
                self.fast_open,
                self.sorted_open,
                self.sorted_tail,
                self.path,
                self.path_cursor,
                self.current,
            ]
            .iter()
            .all(|&i| ok(i))
            && table_ok(&self.closed_table, None)
            && table_ok(&self.open_table, None)
            && table_ok(&self.node_map, Some(tiles))
            && self.flags.as_ref().is_none_or(|f| f.len() as i64 == tiles)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::storage::GridKind;

    fn filled(kind: StorageKind, grid: GridKind) -> Storage {
        let mut s = Storage::new(kind);
        s.init(12, 9, grid);
        s.nodes = vec![
            Node { parent: NIL, next: 1, prev: NIL, cost: 7, path_cost: 3, estimate: 4, x: 2, y: 5 },
            Node { parent: 0, next: NIL, prev: 0, cost: 9, path_cost: 5, estimate: 4, x: 3, y: 5 },
        ];
        s.sorted_open = 0;
        s.sorted_tail = 1;
        s.current = 1;
        s.bounds = AStarRect { bottom: 8, left: 1, right: 10, top: 2 };
        s.has_bounds = true;
        if let Some(f) = s.flags.as_mut() {
            f[5] = 4;
            f[17] = 8;
        }
        s
    }

    fn same(a: &Storage, b: &Storage) -> bool {
        a.to_bytes() == b.to_bytes()
            && a.nodes == b.nodes
            && a.flags == b.flags
            && a.node_map == b.node_map
            && a.closed_table == b.closed_table
            && a.open_table == b.open_table
            && a.bounds == b.bounds
            && a.dirty == b.dirty
    }

    #[test]
    fn every_kind_round_trips() {
        for (kind, grid) in [
            (StorageKind::Hashed, GridKind::Hashed),
            (StorageKind::Bounded, GridKind::Hashed),
            (StorageKind::Terrain, GridKind::Hashed),
        ] {
            let s = filled(kind, grid);
            let b = s.to_bytes();
            let t = Storage::from_bytes(kind, &b).expect("its own bytes");
            assert!(same(&s, &t), "{kind:?}");
        }
        let empty = Storage::new(StorageKind::Hashed);
        let t = Storage::from_bytes(StorageKind::Hashed, &empty.to_bytes()).unwrap();
        assert!(same(&empty, &t));
    }

    #[test]
    fn the_restored_storage_goes_on_as_the_saved_one() {
        // the open list's next node and the path walk read the same after the round trip
        let s = filled(StorageKind::Hashed, GridKind::Hashed);
        let mut a = s.clone();
        let mut b = Storage::from_bytes(StorageKind::Hashed, &s.to_bytes()).unwrap();
        a.reset_nodes();
        b.reset_nodes();
        assert!(same(&a, &b));
    }

    #[test]
    fn other_bytes_are_refused() {
        let s = filled(StorageKind::Hashed, GridKind::Hashed);
        let b = s.to_bytes();
        assert!(Storage::from_bytes(StorageKind::Bounded, &b).is_none(), "another kind");
        assert!(Storage::from_bytes(StorageKind::Hashed, &b[..b.len() - 1]).is_none(), "short");
        let mut long = b.clone();
        long.push(0);
        assert!(Storage::from_bytes(StorageKind::Hashed, &long).is_none(), "long");
        let mut version = b.clone();
        version[0] = 2;
        assert!(Storage::from_bytes(StorageKind::Hashed, &version).is_none(), "version");
        let mut link = b.clone();
        link[12..16].copy_from_slice(&5i32.to_le_bytes()); // the first node's parent, past the arena
        assert!(Storage::from_bytes(StorageKind::Hashed, &link).is_none(), "link");
        let mut count = b.clone();
        count[8..12].copy_from_slice(&u32::MAX.to_le_bytes()); // a node count past the buffer
        assert!(Storage::from_bytes(StorageKind::Hashed, &count).is_none(), "count");
        assert!(Storage::from_bytes(StorageKind::Hashed, &[]).is_none(), "empty");
    }
}
