//! The effect manager's bookkeeping as a Rust object (`EEEffectManager`, `src/Empire Earth/Graphics Thread/
//! EEEffectManager.cpp`): the last handle given out (the original's global `sEffectManagerHandle`, saved first),
//! the effects by handle (the original's `std::map<unsigned long, EEEffect*>`, walked in handle order by Save,
//! RepairPointers and ClearEffects), the effect list (the original's intrusive `mEffects` / `EEEffect::mNext`
//! / `mPrevious`, newest first: `Update` steps the effects in its order) and the destruction queue (`DestroyEffect`'s
//! handles, saved). The C++ keeps the effects (opaque pointers here: never dereferenced), their references and every
//! call into them.
//!
//! This is simulation state: the list's order is the order the tick steps the effects in (their random draws), and
//! the handles and the queue are saved. As the original: a handle already in the index keeps its first effect (the
//! list still takes the second), new handles count up from the last one with wrapping, and unlinking an effect that
//! is not in the list does nothing.

use std::collections::{BTreeMap, VecDeque};

/// An effect (`EEEffect*`), compared by address only.
pub type EffectPtr = usize;

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
struct Link {
    prev: EffectPtr,
    next: EffectPtr,
}

/// `EEEffectManager`'s containers.
#[derive(Debug, Default)]
pub struct EffectStore {
    last_handle: u32,
    by_handle: BTreeMap<u32, EffectPtr>,
    links: BTreeMap<EffectPtr, Link>,
    head: EffectPtr,
    destroyed: VecDeque<u32>,
}

impl EffectStore {
    pub fn new() -> EffectStore {
        EffectStore::default()
    }

    pub fn last_handle(&self) -> u32 {
        self.last_handle
    }

    pub fn set_last_handle(&mut self, h: u32) {
        self.last_handle = h;
    }

    /// `AddEffect`: a new handle when asked (else `own`), the effect at the head of the list and in the index under
    /// its handle (unless the handle is there already); returns the handle.
    pub fn add(&mut self, effect: EffectPtr, new_handle: bool, own: u32) -> u32 {
        let handle = if new_handle {
            self.last_handle = self.last_handle.wrapping_add(1);
            self.last_handle
        } else {
            own
        };
        self.unlink(effect);
        let old = self.head;
        if old != 0 {
            if let Some(l) = self.links.get_mut(&old) {
                l.prev = effect;
            }
        }
        self.links.insert(effect, Link { prev: 0, next: old });
        self.head = effect;
        self.by_handle.entry(handle).or_insert(effect);
        handle
    }

    /// `RemoveEffect`'s unlink (the C++ releases the effect).
    pub fn unlink(&mut self, effect: EffectPtr) {
        let Some(l) = self.links.remove(&effect) else { return };
        if l.prev != 0 {
            if let Some(p) = self.links.get_mut(&l.prev) {
                p.next = l.next;
            }
        }
        if l.next != 0 {
            if let Some(n) = self.links.get_mut(&l.next) {
                n.prev = l.prev;
            }
        }
        if self.head == effect {
            self.head = l.next;
        }
    }

    /// The list's first effect (0: none).
    pub fn head(&self) -> EffectPtr {
        self.head
    }

    /// The effect after `effect` in the list (0: none, or `effect` is not in it).
    pub fn next(&self, effect: EffectPtr) -> EffectPtr {
        self.links.get(&effect).map_or(0, |l| l.next)
    }

    /// The list's length.
    pub fn list_len(&self) -> usize {
        self.links.len()
    }

    /// `GetEffect`'s lookup (0: none).
    pub fn find(&self, handle: u32) -> EffectPtr {
        self.by_handle.get(&handle).copied().unwrap_or(0)
    }

    /// The index's entry at `handle`, erased; whether there was one.
    pub fn erase(&mut self, handle: u32) -> bool {
        self.by_handle.remove(&handle).is_some()
    }

    pub fn count(&self) -> usize {
        self.by_handle.len()
    }

    /// The index's first entry at or after `cursor` (a handle, or one past the last as a u64), in handle order.
    pub fn at_or_after(&self, cursor: u64) -> Option<(u32, EffectPtr)> {
        if cursor > u32::MAX as u64 {
            return None;
        }
        self.by_handle.range(cursor as u32..).next().map(|(&h, &e)| (h, e))
    }

    /// `DestroyEffect`: queue a handle.
    pub fn queue_destroy(&mut self, handle: u32) {
        self.destroyed.push_back(handle);
    }

    /// The queue's front handle, taken.
    pub fn take_destroyed(&mut self) -> Option<u32> {
        self.destroyed.pop_front()
    }

    pub fn destroyed_len(&self) -> usize {
        self.destroyed.len()
    }

    pub fn destroyed_at(&self, i: usize) -> u32 {
        self.destroyed.get(i).copied().unwrap_or(0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn list(s: &EffectStore) -> Vec<EffectPtr> {
        let mut v = Vec::new();
        let mut e = s.head();
        while e != 0 {
            v.push(e);
            e = s.next(e);
        }
        v
    }

    #[test]
    fn effects_link_newest_first_and_index_by_handle() {
        let mut s = EffectStore::new();
        assert_eq!(s.add(10, true, 0), 1);
        assert_eq!(s.add(20, true, 0), 2);
        assert_eq!(s.add(30, false, 7), 7);
        assert_eq!(list(&s), vec![30, 20, 10]);
        assert_eq!(s.find(2), 20);
        // a handle already in the index keeps its effect; the list takes the new one
        assert_eq!(s.add(40, false, 2), 2);
        assert_eq!(s.find(2), 20);
        assert_eq!(list(&s), vec![40, 30, 20, 10]);
        s.unlink(30);
        s.unlink(99);
        assert_eq!(list(&s), vec![40, 20, 10]);
        s.unlink(40);
        assert_eq!(s.head(), 20);
        assert_eq!(s.next(10), 0);
        assert_eq!(s.at_or_after(3), Some((7, 30)));
        assert_eq!(s.at_or_after(8), None);
        assert!(s.erase(1) && !s.erase(1));
        assert_eq!(s.count(), 2);
    }

    #[test]
    fn handles_wrap_and_the_queue_is_fifo() {
        let mut s = EffectStore::new();
        s.set_last_handle(u32::MAX);
        assert_eq!(s.add(5, true, 0), 0);
        assert_eq!(s.at_or_after(u32::MAX as u64 + 1), None);
        s.queue_destroy(4);
        s.queue_destroy(9);
        assert_eq!((s.destroyed_len(), s.destroyed_at(1)), (2, 9));
        assert_eq!(s.take_destroyed(), Some(4));
        assert_eq!(s.take_destroyed(), Some(9));
        assert_eq!(s.take_destroyed(), None);
    }
}
