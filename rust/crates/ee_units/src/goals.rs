//! The units' goal queues as goal ids (RUST_PLAN R5b.4, the fifteenth round): each `EEGoalQueueContainer`
//! (`EEUnitVectorItem`, one of a complex unit's `mGoalQueues`) is mirrored as the list of its goals' ids
//! (`EEUnitGoal::mID`, the save's goal ids) by the container's address, with its unit. The container's deque is a
//! write-through holder (`EEGoalDeque`): every change reports the whole list (queues are short), the destructor drops
//! it. A unit's queues are its containers in address order (`mGoalQueues` is one vector: the address order is the
//! index order). The capture's USCK check compares every listed complex unit's queues with the mirror.
//!
//! Reads: `queue_len(unit, index)` (`GetQueueGoals(index).size()`), `queue_ids(unit, index)`.

use std::collections::BTreeMap;
use std::sync::Mutex;

/// One container's goals: its unit (`mUnit`'s address) and the goals' ids in queue order (0: a null entry).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct GoalQueue {
    pub unit: u64,
    pub ids: Vec<u32>,
}

/// The mirrors by container address.
#[derive(Default)]
pub struct GoalQueues {
    queues: BTreeMap<u64, GoalQueue>,
}

impl GoalQueues {
    pub const fn new() -> Self {
        GoalQueues { queues: BTreeMap::new() }
    }

    /// A container's deque changed: its whole list.
    pub fn set(&mut self, container: u64, unit: u64, ids: &[u32]) {
        let q = self.queues.entry(container).or_default();
        q.unit = unit;
        q.ids.clear();
        q.ids.extend_from_slice(ids);
    }

    /// A container's destructor. True when it had a mirror.
    pub fn drop_queue(&mut self, container: u64) -> bool {
        self.queues.remove(&container).is_some()
    }

    pub fn get(&self, container: u64) -> Option<&GoalQueue> {
        self.queues.get(&container)
    }

    pub fn len(&self) -> usize {
        self.queues.len()
    }

    pub fn is_empty(&self) -> bool {
        self.queues.is_empty()
    }

    /// A unit's queues in index order (its containers by address).
    pub fn of_unit(&self, unit: u64) -> impl Iterator<Item = (u64, &GoalQueue)> {
        self.queues.iter().filter(move |(_, q)| q.unit == unit).map(|(c, q)| (*c, q))
    }

    /// The length of a unit's queue `index` (`GetQueueGoals(index).size()`), when the mirror has it.
    pub fn queue_len(&self, unit: u64, index: usize) -> Option<usize> {
        self.of_unit(unit).nth(index).map(|(_, q)| q.ids.len())
    }

    /// TEST ONLY: a unit's containers (in its vector's order) and their goals' ids against the mirror: the
    /// containers missing, differing in unit or ids, plus the unit's mirrors no container named. 0: equal.
    pub fn check_unit(&self, unit: u64, containers: &[(u64, &[u32])]) -> u32 {
        let mut bad = 0u32;
        for (c, ids) in containers {
            match self.queues.get(c) {
                Some(q) if q.unit == unit && q.ids == *ids => {}
                _ => bad += 1,
            }
        }
        let named = containers.iter().filter(|(c, _)| self.queues.get(c).is_some_and(|q| q.unit == unit)).count();
        bad + self.of_unit(unit).count().saturating_sub(named) as u32
    }
}

/// The process's mirrors (written under the world's lock; the mutex keeps another thread's read whole).
pub static GOAL_QUEUES: Mutex<GoalQueues> = Mutex::new(GoalQueues::new());

/// Runs `f` over the process's mirrors (a poisoned lock is taken over: plain values only).
pub fn with_goal_queues<R>(f: impl FnOnce(&mut GoalQueues) -> R) -> R {
    let mut guard = GOAL_QUEUES.lock().unwrap_or_else(|e| e.into_inner());
    f(&mut guard)
}

/// `GetQueueGoals(index).size()` of a unit (its object's address), when the mirror has the queue.
pub fn queue_len(unit: u64, index: usize) -> Option<usize> {
    with_goal_queues(|g| g.queue_len(unit, index))
}

/// The goal ids of a unit's queue `index`, when the mirror has it.
pub fn queue_ids(unit: u64, index: usize) -> Option<Vec<u32>> {
    with_goal_queues(|g| g.of_unit(unit).nth(index).map(|(_, q)| q.ids.clone()))
}

fn slice<'a, T>(ptr: *const T, count: usize) -> &'a [T] {
    if count == 0 || ptr.is_null() {
        &[]
    } else {
        // SAFETY: the callers' contracts (`count` valid elements at `ptr`)
        unsafe { std::slice::from_raw_parts(ptr, count) }
    }
}

/// A container's deque changed: its unit and its goals' ids (`count` at `ids`).
///
/// # Safety
/// `ids` points to `count` words (or is null with `count` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queue_set(container: u64, unit: u64, ids: *const u32, count: usize) {
    let ids = slice(ids, count);
    with_goal_queues(|g| g.set(container, unit, ids));
}

/// A container's destructor.
#[no_mangle]
pub extern "C" fn ee_units_goal_queue_drop(container: u64) {
    with_goal_queues(|g| g.drop_queue(container));
}

/// The length of a unit's queue `index`; -1 when the mirror has no such queue.
#[no_mangle]
pub extern "C" fn ee_units_goal_queue_len(unit: u64, index: u32) -> i64 {
    queue_len(unit, index as usize).map_or(-1, |n| n as i64)
}

/// TEST ONLY: a unit's `count` containers (their addresses, each with `lens[i]` goal ids, all of them in order at
/// `ids`) against the mirror: the mismatches (`GoalQueues::check_unit`).
///
/// # Safety
/// `containers` and `lens` point to `count` elements, `ids` to the sum of `lens` (each null when empty).
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queue_check_unit(
    unit: u64,
    containers: *const u64,
    lens: *const u32,
    count: usize,
    ids: *const u32,
    ids_count: usize,
) -> u32 {
    let containers = slice(containers, count);
    let lens = slice(lens, count);
    let ids = slice(ids, ids_count);
    let mut at = 0usize;
    let mut list = Vec::with_capacity(count);
    for (c, n) in containers.iter().zip(lens) {
        let n = *n as usize;
        if at + n > ids.len() {
            return u32::MAX;
        }
        list.push((*c, &ids[at..at + n]));
        at += n;
    }
    with_goal_queues(|g| g.check_unit(unit, &list))
}

/// TEST ONLY: the mirrors held (the containers alive).
#[no_mangle]
pub extern "C" fn ee_units_goal_queue_count() -> u32 {
    with_goal_queues(|g| g.len() as u32)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_units_queues_are_its_containers_by_address() {
        let mut g = GoalQueues::new();
        // a unit with two queues (one vector: 0x1000 then 0x1028), another unit's queue between them by key
        g.set(0x1028, 0xaa, &[7, 8]);
        g.set(0x1000, 0xaa, &[5]);
        g.set(0x1010, 0xbb, &[9, 9, 9]);
        assert_eq!(g.queue_len(0xaa, 0), Some(1));
        assert_eq!(g.queue_len(0xaa, 1), Some(2));
        assert_eq!(g.queue_len(0xaa, 2), None);
        assert_eq!(g.queue_len(0xbb, 0), Some(3));
        // a change replaces the list; the destructor drops it
        g.set(0x1000, 0xaa, &[]);
        assert_eq!(g.queue_len(0xaa, 0), Some(0));
        assert!(g.drop_queue(0x1028));
        assert!(!g.drop_queue(0x1028));
        assert_eq!(g.queue_len(0xaa, 1), None);
        assert_eq!(g.len(), 2);
    }

    #[test]
    fn the_check_names_missing_stale_and_unnamed_queues() {
        let mut g = GoalQueues::new();
        g.set(0x1000, 0xaa, &[5]);
        g.set(0x1028, 0xaa, &[7, 8]);
        let same: [(u64, &[u32]); 2] = [(0x1000, &[5]), (0x1028, &[7, 8])];
        assert_eq!(g.check_unit(0xaa, &same), 0);
        // a stale list, a missing container, an unnamed one
        assert_eq!(g.check_unit(0xaa, &[(0x1000, &[5]), (0x1028, &[8, 7])]), 1);
        assert_eq!(g.check_unit(0xaa, &[(0x1000, &[5]), (0x1028, &[7, 8]), (0x1050, &[])]), 1);
        assert_eq!(g.check_unit(0xaa, &[(0x1000, &[5])]), 1);
        // another unit's container is not this unit's
        assert_eq!(g.check_unit(0xbb, &[(0x1000, &[5])]), 1);
    }

    #[test]
    fn the_process_mirror_is_reachable() {
        let ids = [3u32, 4];
        // SAFETY: valid pointers to locals
        unsafe { ee_units_goal_queue_set(0xfeed_0000_1000, 0xfeed_0000_0001, ids.as_ptr(), ids.len()) };
        assert_eq!(ee_units_goal_queue_len(0xfeed_0000_0001, 0), 2);
        assert_eq!(queue_ids(0xfeed_0000_0001, 0), Some(vec![3, 4]));
        let containers = [0xfeed_0000_1000u64];
        let lens = [2u32];
        // SAFETY: valid pointers to locals
        let bad = unsafe {
            ee_units_goal_queue_check_unit(0xfeed_0000_0001, containers.as_ptr(), lens.as_ptr(), 1, ids.as_ptr(), 2)
        };
        assert_eq!(bad, 0);
        ee_units_goal_queue_drop(0xfeed_0000_1000);
        assert_eq!(ee_units_goal_queue_len(0xfeed_0000_0001, 0), -1);
    }
}
