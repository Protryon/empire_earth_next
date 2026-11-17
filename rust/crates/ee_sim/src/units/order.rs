//! The units stepped every tick, in their order (RUST_PLAN R5b.4 leaf 6, the tick's first store field):
//! `EEWorld::mUpdateUnits` as a list of unit ids.
//!
//! The C++ deque stays the list the tick walks (`EEPhysics::UpdateUnits`: from the last to the first); this list
//! follows each of its changes (`EEWorld::UpdateActiveUnits`' appends at the back and erases in place,
//! `RebuildActiveUnits`' and `Clear`'s clears and appends, a load's reorder through `reset`), and the unit store's
//! check (USCK) compares the two at every tick. A Rust tick will walk this list, not the store's id order: one
//! random stream serves every unit in this order (RUST_PLAN 2.1).
//!
//! Determinism: a `Vec` in the deque's order; nothing here draws, sorts or hashes.

use std::sync::Mutex;

/// The ids in the deque's order.
#[derive(Default, Debug, Clone, PartialEq, Eq)]
pub struct UpdateOrder {
    ids: Vec<u32>,
}

impl UpdateOrder {
    pub const fn new() -> Self {
        UpdateOrder { ids: Vec::new() }
    }

    /// `mUpdateUnits.push_back`.
    pub fn push(&mut self, id: u32) {
        self.ids.push(id);
    }

    /// `mUpdateUnits.erase(std::find(..., unit))`: the first entry of `id`, the others keeping their order. False
    /// when there is none (the C++ erases only what its find found).
    pub fn erase(&mut self, id: u32) -> bool {
        match self.ids.iter().position(|&i| i == id) {
            Some(at) => {
                self.ids.remove(at);
                true
            }
            None => false,
        }
    }

    /// `mUpdateUnits.clear()`.
    pub fn clear(&mut self) {
        self.ids.clear();
    }

    /// The whole list replaced (a load's saved order, EEPWorldHelper's `RestoreUnitOrder`).
    pub fn reset(&mut self, ids: &[u32]) {
        self.ids.clear();
        self.ids.extend_from_slice(ids);
    }

    pub fn ids(&self) -> &[u32] {
        &self.ids
    }

    pub fn len(&self) -> usize {
        self.ids.len()
    }

    pub fn is_empty(&self) -> bool {
        self.ids.is_empty()
    }

    /// TEST ONLY: the deque's ids (in its order) against this list: 0 when they are the same, else the first index
    /// where they differ plus 1 (the shorter list's length plus 1 when one is the other's start).
    pub fn check(&self, deque: &[u32]) -> u32 {
        match self.ids.iter().zip(deque).position(|(a, b)| a != b) {
            Some(at) => at as u32 + 1,
            None if self.ids.len() != deque.len() => self.ids.len().min(deque.len()) as u32 + 1,
            None => 0,
        }
    }
}

/// The process's list. Written under the world's write lock (the physics tick, the loads, the editor); the mutex
/// keeps a read from another thread whole.
pub static ORDER: Mutex<UpdateOrder> = Mutex::new(UpdateOrder::new());

/// Runs `f` over the process's list (a poisoned lock is taken over: the list holds plain ids only).
pub fn with_order<R>(f: impl FnOnce(&mut UpdateOrder) -> R) -> R {
    let mut guard = ORDER.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn push_and_erase_keep_the_deques_order() {
        let mut o = UpdateOrder::new();
        for id in [5, 3, 9, 3, 7] {
            o.push(id);
        }
        // the first 3 goes, the second stays where it is
        assert!(o.erase(3));
        assert_eq!(o.ids(), &[5, 9, 3, 7]);
        assert!(!o.erase(42));
        assert!(o.erase(7));
        assert_eq!(o.ids(), &[5, 9, 3]);
        o.clear();
        assert!(o.is_empty());
        o.reset(&[1, 2, 3]);
        assert_eq!((o.len(), o.ids()), (3, &[1u32, 2, 3][..]));
    }

    #[test]
    fn check_names_the_first_difference() {
        let mut o = UpdateOrder::new();
        o.reset(&[10, 11, 12]);
        assert_eq!(o.check(&[10, 11, 12]), 0);
        assert_eq!(o.check(&[10, 12, 11]), 2);
        assert_eq!(o.check(&[10, 11]), 3);
        assert_eq!(o.check(&[10, 11, 12, 13]), 4);
        assert_eq!(o.check(&[]), 1);
        assert_eq!(UpdateOrder::new().check(&[]), 0);
    }

    #[test]
    fn the_process_list_is_reachable() {
        // the only test of this crate that touches ORDER
        with_order(|o| o.reset(&[0xfe00_0001, 0xfe00_0002]));
        assert!(with_order(|o| o.erase(0xfe00_0001)));
        assert_eq!(with_order(|o| o.ids().to_vec()), vec![0xfe00_0002]);
        with_order(|o| o.clear());
    }
}
