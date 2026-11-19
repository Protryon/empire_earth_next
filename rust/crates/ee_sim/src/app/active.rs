//! The world's active unit lists as one object (`EEWorld`'s `mActivateQueue`, `mDeactivateQueue`, `mActiveUnits`
//! and, through [`crate::units::order`], `mUpdateUnits`): the queues of units to start and stop stepping, the set of
//! the stepped units, and the moves between them (`ActivateUnit`, `DeactivateUnit`, `UpdateActiveUnits`,
//! `RebuildActiveUnits`, `Clear`'s and a load's). Units are their handle ids (`EEUnitHandleID`: the unit's id).
//!
//! The C++ keeps the units' reference counts: every move says which unit gains or loses references, and the C++
//! applies each change when it is reported (a unit whose last reference goes is freed then, as the C++'s loops freed
//! it), with no lock of this object held over the call.
//!
//! Determinism: vectors in the deques' order and a `BTreeSet` (`std::map`'s order by id); nothing hashes.

use std::collections::BTreeSet;
use std::sync::Mutex;

use crate::units::order::{UpdateOrder, ORDER};

/// The queues and the stepped set.
#[derive(Default, Debug, Clone, PartialEq, Eq)]
pub struct ActiveLists {
    /// `mActivateQueue` (each entry holds a reference)
    pub activate: Vec<u32>,
    /// `mDeactivateQueue` (each entry holds a reference)
    pub deactivate: Vec<u32>,
    /// `mActiveUnits`' keys (each holds a reference, as does its entry in the stepped list)
    pub active: BTreeSet<u32>,
}

impl ActiveLists {
    pub const fn new() -> Self {
        ActiveLists { activate: Vec::new(), deactivate: Vec::new(), active: BTreeSet::new() }
    }
}

/// The process's lists.
pub static LISTS: Mutex<ActiveLists> = Mutex::new(ActiveLists::new());

/// The lists and the stepped list an operation works on (the process's: [`Lists::world`]; a test's own).
#[derive(Clone, Copy)]
pub struct Lists<'a> {
    pub lists: &'a Mutex<ActiveLists>,
    pub order: &'a Mutex<UpdateOrder>,
}

impl<'a> Lists<'a> {
    pub fn world() -> Lists<'static> {
        Lists { lists: &LISTS, order: &ORDER }
    }
    /// Runs `f` over the lists (a poisoned lock is taken over: they hold plain ids).
    fn with_lists<R>(&self, f: impl FnOnce(&mut ActiveLists) -> R) -> R {
        f(&mut self.lists.lock().unwrap_or_else(|e| e.into_inner()))
    }
    fn with_order<R>(&self, f: impl FnOnce(&mut UpdateOrder) -> R) -> R {
        f(&mut self.order.lock().unwrap_or_else(|e| e.into_inner()))
    }

    /// `ActivateUnit` after its checks: queued unless already queued -> true when queued (the C++ takes a reference).
    pub fn activate(&self, id: u32) -> bool {
        self.with_lists(|l| {
            if l.activate.contains(&id) {
                return false;
            }
            l.activate.push(id);
            true
        })
    }

    /// `DeactivateUnit` for a unit with an id: queued unless already queued -> true when queued (the C++ takes a
    /// reference; the queue keeps it).
    pub fn deactivate(&self, id: u32) -> bool {
        self.with_lists(|l| {
            if l.deactivate.contains(&id) {
                return false;
            }
            l.deactivate.push(id);
            true
        })
    }

    /// `UpdateActiveUnits` after the born deactivations joined the queue: each queued unit not stepped yet starts
    /// (the stepped list's back, the set; two references), each queued unit stepped stops (two references go); every
    /// queue entry's own reference goes. `refs(id, delta)` applies a unit's change as it is made.
    pub fn update(&self, refs: &mut dyn FnMut(u32, i32)) {
        let queue = self.with_lists(|l| std::mem::take(&mut l.activate));
        for id in queue {
            let start = self.with_lists(|l| l.active.insert(id));
            if start {
                self.with_order(|o| o.push(id));
                refs(id, 2);
            }
            refs(id, -1);
        }
        let queue = self.with_lists(|l| std::mem::take(&mut l.deactivate));
        for id in queue {
            let stepped = self.with_lists(|l| l.active.contains(&id));
            if stepped && self.with_order(|o| o.erase(id)) {
                self.with_lists(|l| l.active.remove(&id));
                refs(id, -2);
            }
            refs(id, -1);
        }
    }

    /// Every list emptied (`RebuildActiveUnits`' and `Clear`'s start): the references go in the C++'s order (the
    /// activation queue's, the deactivation queue's, then `between` (the C++'s own: the unborn deactivations), the
    /// stepped list's, the set's).
    pub fn clear(&self, refs: &mut dyn FnMut(u32, i32), between: &mut dyn FnMut()) {
        let (activate, deactivate) =
            self.with_lists(|l| (std::mem::take(&mut l.activate), std::mem::take(&mut l.deactivate)));
        for id in activate.into_iter().chain(deactivate) {
            refs(id, -1);
        }
        between();
        let stepped = self.with_order(|o| {
            let ids = o.ids().to_vec();
            o.clear();
            ids
        });
        for id in stepped {
            refs(id, -1);
        }
        let set = self.with_lists(|l| std::mem::take(&mut l.active));
        for id in set {
            refs(id, -1);
        }
    }

    /// A unit stepped from now (`RebuildActiveUnits`' walk): the stepped list's back and the set (the C++ takes two
    /// references).
    pub fn start(&self, id: u32) {
        self.with_order(|o| o.push(id));
        self.with_lists(|l| l.active.insert(id));
    }

    /// The list `which` (0 stepped, 1 activation, 2 deactivation) in its order (the save, the dump).
    pub fn list(&self, which: u32) -> Vec<u32> {
        match which {
            0 => self.with_order(|o| o.ids().to_vec()),
            1 => self.with_lists(|l| l.activate.clone()),
            _ => self.with_lists(|l| l.deactivate.clone()),
        }
    }

    /// The list `which`'s id at `index` (no copy of the list: the tick steps by index).
    pub fn at(&self, which: u32, index: usize) -> Option<u32> {
        match which {
            0 => self.with_order(|o| o.ids().get(index).copied()),
            1 => self.with_lists(|l| l.activate.get(index).copied()),
            _ => self.with_lists(|l| l.deactivate.get(index).copied()),
        }
    }

    /// The list `which` replaced (a load's saved order; the references are the load's own moves).
    pub fn set_list(&self, which: u32, ids: &[u32]) {
        match which {
            0 => self.with_order(|o| o.reset(ids)),
            1 => self.with_lists(|l| l.activate = ids.to_vec()),
            _ => self.with_lists(|l| l.deactivate = ids.to_vec()),
        }
    }

    /// A unit in the set or out of it (a load's moves).
    pub fn set_active(&self, id: u32, on: bool) {
        self.with_lists(|l| {
            if on {
                l.active.insert(id);
            } else {
                l.active.remove(&id);
            }
        });
    }
}
