//! A graph search's storage as bytes, for the saved game (port, save version 0.278: the path manager's graph
//! searches in progress, `EEPathManager::SaveGraphSearches`). The original saved none: a loaded game dropped each
//! running sub sector search, and its owner (an attack sub group, a unit) asked again at the first tick, so the path
//! came later (or another one) than in the game that was saved.
//!
//! The format is the storage's fields in their declaration order, little-endian, as `storage_save.rs` writes a tile
//! storage. A node's object is a C++ pointer (`AStarGraphableObjectBase*`), meaningless in another process: it is
//! written as one byte, 0 for no object, 1 for a live node's (the loader gets it back from the node's index, the
//! object's id), 2 for a node on the free list (its object is never read again: `new_node` overwrites every field;
//! it loads as no object). `from_bytes` takes only what `to_bytes` wrote: a short or long buffer, a wrong version or
//! kind, a link out of the arena, an index the loader cannot resolve, or arrays of another size than the count give
//! `None`.

use crate::graph::{GraphKind, GraphNode, GraphStorage, Obj, NIL};

const VERSION: u32 = 1;

const OBJ_NONE: u8 = 0;
const OBJ_LIVE: u8 = 1;
const OBJ_FREE: u8 = 2;

struct Writer(Vec<u8>);

impl Writer {
    fn u32(&mut self, v: u32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    fn i32(&mut self, v: i32) {
        self.0.extend_from_slice(&v.to_le_bytes());
    }
    fn u8(&mut self, v: u8) {
        self.0.push(v);
    }
    fn ints(&mut self, v: &Option<Vec<i32>>) {
        match v {
            None => self.u8(0),
            Some(v) => {
                self.u8(1);
                self.u32(v.len() as u32);
                for &x in v {
                    self.i32(x);
                }
            }
        }
    }
    fn bytes(&mut self, v: &Option<Vec<u8>>) {
        match v {
            None => self.u8(0),
            Some(v) => {
                self.u8(1);
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
    fn u8(&mut self) -> Option<u8> {
        Some(self.take(1)?[0])
    }
    fn flag(&mut self) -> Option<bool> {
        match self.u8()? {
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
    fn ints(&mut self) -> Option<Option<Vec<i32>>> {
        if !self.flag()? {
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
        if !self.flag()? {
            return Some(None);
        }
        let n = self.len()?;
        Some(Some(self.take(n)?.to_vec()))
    }
}

impl GraphStorage {
    /// The nodes on the free list (a node there twice, or a cycle, ends the walk).
    fn free_nodes(&self) -> Vec<bool> {
        let mut free = vec![false; self.nodes.len()];
        let mut i = self.free;
        while i != NIL && (i as usize) < self.nodes.len() && !free[i as usize] {
            free[i as usize] = true;
            i = self.nodes[i as usize].next;
        }
        free
    }

    /// Whether every live node's object is the one `object_of` gives for its index (the save's check that the
    /// loader will find the same objects).
    pub fn objects_match(&self, object_of: &mut dyn FnMut(i32) -> Obj) -> bool {
        let free = self.free_nodes();
        self.nodes.iter().zip(free).all(|(n, f)| f || n.object == 0 || object_of(n.index) == n.object)
    }

    /// The storage as bytes (the module's format).
    pub fn to_bytes(&self) -> Vec<u8> {
        let free = self.free_nodes();
        let mut w = Writer(Vec::new());
        w.u32(VERSION);
        w.u32(self.kind as u32);
        w.u32(self.nodes.len() as u32);
        for (n, &f) in self.nodes.iter().zip(&free) {
            w.u8(if f {
                OBJ_FREE
            } else if n.object == 0 {
                OBJ_NONE
            } else {
                OBJ_LIVE
            });
            w.i32(n.parent);
            w.i32(n.next);
            w.i32(n.prev);
            w.i32(n.index);
            w.u32(n.cost);
            w.i32(n.path_cost);
            w.i32(n.estimate);
            w.i32(n.x);
            w.i32(n.y);
        }
        w.i32(self.free);
        w.ints(&self.closed_table);
        w.ints(&self.open_table);
        for v in [
            self.closed_list,
            self.open_list,
            self.sorted_open,
            self.sorted_tail,
            self.path,
            self.path_cursor,
            self.current,
        ] {
            w.i32(v);
        }
        w.bytes(&self.flags);
        w.ints(&self.node_map);
        w.i32(self.count);
        w.i32(self.bucket_count);
        w.i32(self.old_count);
        w.0
    }

    /// A storage of `kind` from `to_bytes`' bytes, each live node's object from `object_of` (its index; 0: none,
    /// which refuses the bytes); `None` for any other bytes (see the module's comment).
    pub fn from_bytes(kind: GraphKind, data: &[u8], object_of: &mut dyn FnMut(i32) -> Obj) -> Option<GraphStorage> {
        let mut r = Reader { data, at: 0 };
        if r.u32()? != VERSION || r.u32()? != kind as u32 {
            return None;
        }
        let count = r.len()?;
        let mut nodes = Vec::with_capacity(count);
        for _ in 0..count {
            let tag = r.u8()?;
            let mut n = GraphNode {
                object: 0,
                parent: r.i32()?,
                next: r.i32()?,
                prev: r.i32()?,
                index: r.i32()?,
                cost: r.u32()?,
                path_cost: r.i32()?,
                estimate: r.i32()?,
                x: r.i32()?,
                y: r.i32()?,
            };
            match tag {
                OBJ_NONE | OBJ_FREE => {}
                OBJ_LIVE => {
                    n.object = object_of(n.index);
                    if n.object == 0 {
                        return None;
                    }
                }
                _ => return None,
            }
            nodes.push(n);
        }
        let mut s = GraphStorage::new(kind);
        s.nodes = nodes;
        s.free = r.i32()?;
        s.closed_table = r.ints()?;
        s.open_table = r.ints()?;
        s.closed_list = r.i32()?;
        s.open_list = r.i32()?;
        s.sorted_open = r.i32()?;
        s.sorted_tail = r.i32()?;
        s.path = r.i32()?;
        s.path_cursor = r.i32()?;
        s.current = r.i32()?;
        s.flags = r.bytes()?;
        s.node_map = r.ints()?;
        s.count = r.i32()?;
        s.bucket_count = r.i32()?;
        s.old_count = r.i32()?;
        if r.at != data.len() || !s.links_valid() {
            return None;
        }
        Some(s)
    }

    /// Every link (the nodes', the lists', the tables') is `NIL` or inside the arena, the hash tables hold
    /// `bucket_count` heads and the per-index arrays as many entries as each other, past every live node's index.
    fn links_valid(&self) -> bool {
        let n = self.nodes.len() as i64;
        let ok = |i: i32| i == NIL || (i >= 0 && (i as i64) < n);
        let table_ok = |t: &Option<Vec<i32>>| {
            t.as_ref().is_none_or(|v| v.len() as i64 == self.bucket_count as i64 && v.iter().all(|&i| ok(i)))
        };
        let size = self.node_map.as_ref().map(|m| m.len());
        let arrays_ok = size == self.flags.as_ref().map(|f| f.len())
            && self.node_map.as_ref().is_none_or(|m| m.iter().all(|&i| ok(i)));
        let free = self.free_nodes();
        let indices_ok = self
            .nodes
            .iter()
            .zip(free)
            .all(|(x, f)| f || size.is_none_or(|s| x.index >= 0 && (x.index as usize) < s));
        self.bucket_count > 0
            && self.nodes.iter().all(|x| ok(x.parent) && ok(x.next) && ok(x.prev))
            && [
                self.free,
                self.closed_list,
                self.open_list,
                self.sorted_open,
                self.sorted_tail,
                self.path,
                self.path_cursor,
                self.current,
            ]
            .iter()
            .all(|&i| ok(i))
            && table_ok(&self.closed_table)
            && table_ok(&self.open_table)
            && arrays_ok
            && indices_ok
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A storage with two live nodes (objects 0x1000 + index), one free node and every table.
    fn filled() -> GraphStorage {
        let mut s = GraphStorage::new(GraphKind::Hashed);
        s.init_begin(6);
        s.prepare_arrays(6);
        s.init_end();
        let a = s.new_node(4, 5, 0x1003, 3);
        let b = s.new_node(6, 5, 0x1005, 5);
        let c = s.new_node(1, 1, 0x1001, 1);
        s.release_node(c);
        s.nodes[b as usize].parent = a;
        s.nodes[b as usize].cost = 17;
        s.sorted_open = b;
        s.sorted_tail = b;
        s.current = a;
        s.add_closed(a);
        s
    }

    fn object_of(index: i32) -> Obj {
        0x1000 + index as usize
    }

    #[test]
    fn a_storage_round_trips_with_its_objects() {
        let s = filled();
        let t = GraphStorage::from_bytes(GraphKind::Hashed, &s.to_bytes(), &mut object_of).expect("its own bytes");
        assert_eq!(t.to_bytes(), s.to_bytes());
        // the live nodes' objects come back from their index; the free node's is dropped
        assert_eq!(t.nodes[0], s.nodes[0]);
        assert_eq!(t.nodes[1], s.nodes[1]);
        assert_eq!(t.nodes[2].object, 0);
        assert_eq!(t.flags, s.flags);
        assert_eq!(t.node_map, s.node_map);
        assert_eq!(t.closed_table, s.closed_table);
        let empty = GraphStorage::new(GraphKind::Terrain);
        let e = GraphStorage::from_bytes(GraphKind::Terrain, &empty.to_bytes(), &mut object_of).unwrap();
        assert_eq!(e.to_bytes(), empty.to_bytes());
    }

    #[test]
    fn the_restored_storage_goes_on_as_the_saved_one() {
        let s = filled();
        let mut a = s.clone();
        let mut b = GraphStorage::from_bytes(GraphKind::Hashed, &s.to_bytes(), &mut object_of).unwrap();
        // the next node either takes comes off the same free list with the same arena index
        assert_eq!(a.new_node(2, 2, 7, 2), b.new_node(2, 2, 7, 2));
        assert_eq!(a.view(0), b.view(0));
        a.reset_nodes();
        b.reset_nodes();
        assert_eq!(a.to_bytes(), b.to_bytes());
    }

    #[test]
    fn objects_match_checks_the_live_nodes_only() {
        let s = filled();
        assert!(s.objects_match(&mut object_of));
        assert!(!s.objects_match(&mut |i| 0x2000 + i as usize));
        // the free node's stale object is not checked
        assert!(s.objects_match(&mut |i| if i == 1 { 0 } else { object_of(i) }));
    }

    #[test]
    fn other_bytes_are_refused() {
        let s = filled();
        let b = s.to_bytes();
        let load = |d: &[u8]| GraphStorage::from_bytes(GraphKind::Hashed, d, &mut object_of);
        assert!(GraphStorage::from_bytes(GraphKind::Terrain, &b, &mut object_of).is_none(), "another kind");
        assert!(load(&b[..b.len() - 1]).is_none(), "short");
        let mut long = b.clone();
        long.push(0);
        assert!(load(&long).is_none(), "long");
        let mut version = b.clone();
        version[0] = 2;
        assert!(load(&version).is_none(), "version");
        let mut link = b.clone();
        link[13..17].copy_from_slice(&9i32.to_le_bytes()); // the first node's parent, past the arena
        assert!(load(&link).is_none(), "link");
        let mut tag = b.clone();
        tag[12] = 7; // the first node's object tag
        assert!(load(&tag).is_none(), "tag");
        assert!(
            GraphStorage::from_bytes(GraphKind::Hashed, &b, &mut |_| 0).is_none(),
            "an object the loader cannot find"
        );
        assert!(load(&[]).is_none(), "empty");
    }
}
