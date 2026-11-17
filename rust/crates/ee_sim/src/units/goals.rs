//! The units' goal queues as goal ids (RUST_PLAN R5b.4, the fifteenth round): each `EEGoalQueueContainer`
//! (`EEUnitVectorItem`, one of a complex unit's `mGoalQueues`) is mirrored as the list of its goals' ids
//! (`EEUnitGoal::mID`, the save's goal ids) by the container's address, with its unit. The container's deque is a
//! write-through holder (`EEGoalDeque`): every change reports the whole list (queues are short), the destructor drops
//! it. A unit's queues are its containers in address order (`mGoalQueues` is one vector: the address order is the
//! index order). The capture's USCK check compares every listed complex unit's queues with the mirror.
//!
//! Reads: `queue_len(unit, index)` (`GetQueueGoals(index).size()`), `queue_ids(unit, index)`.
//!
//! The sixteenth round: the mirror holds each queue's goals (their objects and kinds beside the ids) and its timer
//! (`mStartTime`, `mTime`, `mStarted`), so the queue's save and load (`save`, `load`: the container's
//! `EEGoalQueueContainer` record) and its per-tick update (`update_unit`: the timer's expiry, then the goals'
//! refresh) are Rust over a host table (`EeGoalQueueHost`) of typed `FSFile` reads and writes and the goals' own
//! `Save`/`CreateGoal`+`Load` (the goal classes stay C++).

use std::collections::BTreeMap;
use std::sync::Mutex;

/// One container's goals: its unit (`mUnit`'s address) and the goals' ids in queue order (0: a null entry); the
/// goals' objects and kinds in the same order; the timer.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct GoalQueue {
    pub unit: u64,
    pub ids: Vec<u32>,
    pub goals: Vec<u64>,
    pub kinds: Vec<u8>,
    pub timer: EeGoalQueueTimer,
}

/// A queue's timer (`mStartTime`, `mTime`, `mStarted`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGoalQueueTimer {
    pub start: u32,
    pub time: u32,
    pub started: u8,
}

/// One goal of a queue's report: its id, kind and object.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeGoalQueueEntry {
    pub id: u32,
    pub kind: u8,
    pub goal: u64,
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

    /// A container's report: its goals (ids, kinds, objects) and its timer.
    pub fn set_full(&mut self, container: u64, unit: u64, entries: &[EeGoalQueueEntry], timer: EeGoalQueueTimer) {
        let q = self.queues.entry(container).or_default();
        q.unit = unit;
        q.ids = entries.iter().map(|e| e.id).collect();
        q.kinds = entries.iter().map(|e| e.kind).collect();
        q.goals = entries.iter().map(|e| e.goal).collect();
        q.timer = timer;
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
        let timers: Vec<Option<EeGoalQueueTimer>> = vec![None; containers.len()];
        self.check_unit_timers(unit, containers, &timers)
    }

    /// `check_unit` with each container's timer too (None: not compared).
    pub fn check_unit_timers(
        &self,
        unit: u64,
        containers: &[(u64, &[u32])],
        timers: &[Option<EeGoalQueueTimer>],
    ) -> u32 {
        let mut bad = 0u32;
        for ((c, ids), timer) in containers.iter().zip(timers) {
            match self.queues.get(c) {
                Some(q) if q.unit == unit && q.ids == *ids && timer.is_none_or(|t| t == q.timer) => {}
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

/// The queue codec's and update's host: typed `FSFile` writes and reads (the C++ `Write`/`Read` overloads), the
/// goals' own save and load, the container's fields; a status of 0 is `kErrorOK`.
pub trait GoalQueueHost {
    fn write_u32(&mut self, value: u32) -> i32;
    fn write_u8(&mut self, value: u8) -> i32;
    fn write_bool(&mut self, value: bool) -> i32;
    fn read_u32(&mut self, value: &mut u32) -> i32;
    fn read_u8(&mut self, value: &mut u8) -> i32;
    /// `theGoal->Save(...)` of the goal object `goal`
    fn save_goal(&mut self, goal: u64) -> i32;
    /// `CreateGoal(kind)`, its `Load`, then the container's `push_back` on success (else the goal deleted): the
    /// load's status
    fn load_goal(&mut self, kind: u8) -> i32;
    /// the container's `Clear()`
    fn clear(&mut self);
    /// `Read` into the container's `mStartTime`, `mTime`, `mStarted` (0, 1, 2): the status
    fn read_timer(&mut self, field: u32) -> i32;
    /// the per-tick update's: queue `index`'s `Clear()` and `ResetTimer()`; the unit's `RefreshGoals()` and
    /// `RefreshActiveGoal()`
    fn expire(&mut self, index: u32);
    fn refresh(&mut self);
}

/// `EEGoalQueueContainer::Save` (0x6059ea) from the mirror of `container`: the goal count, each goal's kind (a byte)
/// and its own record, then the timer. The first failing status ends it. None: no mirror.
pub fn save(container: u64, host: &mut dyn GoalQueueHost) -> Option<i32> {
    let q = with_goal_queues(|g| g.get(container).cloned())?;
    let mut result = host.write_u32(q.goals.len() as u32);
    if result != 0 {
        return Some(result);
    }
    for (goal, kind) in q.goals.iter().zip(&q.kinds) {
        if result != 0 {
            return Some(result);
        }
        result = host.write_u8(*kind);
        if result == 0 {
            result = host.save_goal(*goal);
        }
    }
    if result != 0 {
        return Some(result);
    }
    result = host.write_u32(q.timer.start);
    if result != 0 {
        return Some(result);
    }
    result = host.write_u32(q.timer.time);
    if result == 0 {
        result = host.write_bool(q.timer.started != 0);
    }
    Some(result)
}

/// `EEGoalQueueContainer::Load` (0x60588b): the container cleared, the count, each goal's kind and record (a
/// failed kind read skips to the next turn, which ends the loop), then the timer's three fields.
pub fn load(host: &mut dyn GoalQueueHost) -> i32 {
    host.clear();
    let mut count = 0u32;
    let mut result = host.read_u32(&mut count);
    if result != 0 {
        return result;
    }
    for _ in 0..count {
        if result != 0 {
            return result;
        }
        let mut kind = 0u8;
        result = host.read_u8(&mut kind);
        if result != 0 {
            continue;
        }
        result = host.load_goal(kind);
    }
    if result == 0 {
        result = host.read_timer(0);
        if result == 0 {
            result = host.read_timer(1);
            if result == 0 {
                result = host.read_timer(2);
            }
        }
    }
    result
}

/// The unit's goal queues' per-tick update (the classes' `Update`'s goal-queue part, `EEGoalQueueContainer::Update`
/// 0x605780 inlined): every started, non-empty queue in index order expires when the game time reaches its start
/// plus its time (32-bit, wrapping: `unsigned long`s), then the unit's goals are refreshed. The mirror is read again
/// for each queue (the refresh may change them). `count` is `GetNumGoalQueues()`.
pub fn update_unit(unit: u64, count: u32, now: u32, host: &mut dyn GoalQueueHost) {
    for index in 0..count {
        let q = with_goal_queues(|g| g.of_unit(unit).nth(index as usize).map(|(_, q)| (q.timer, q.ids.len())));
        let Some((timer, len)) = q else { continue };
        if timer.started == 0 || len == 0 {
            continue;
        }
        if now >= timer.start.wrapping_add(timer.time) {
            host.expire(index);
        }
        host.refresh();
    }
}

/// The C++ host of the codec and the update (`EEGoalQueueContainer.cpp`'s table).
#[repr(C)]
pub struct EeGoalQueueHost {
    pub context: *mut std::ffi::c_void,
    pub write_u32: unsafe extern "C" fn(context: *mut std::ffi::c_void, value: u32) -> i32,
    pub write_u8: unsafe extern "C" fn(context: *mut std::ffi::c_void, value: u8) -> i32,
    pub write_bool: unsafe extern "C" fn(context: *mut std::ffi::c_void, value: bool) -> i32,
    pub read_u32: unsafe extern "C" fn(context: *mut std::ffi::c_void, value: *mut u32) -> i32,
    pub read_u8: unsafe extern "C" fn(context: *mut std::ffi::c_void, value: *mut u8) -> i32,
    pub save_goal: unsafe extern "C" fn(context: *mut std::ffi::c_void, goal: u64) -> i32,
    pub load_goal: unsafe extern "C" fn(context: *mut std::ffi::c_void, kind: u8) -> i32,
    pub clear: unsafe extern "C" fn(context: *mut std::ffi::c_void),
    pub read_timer: unsafe extern "C" fn(context: *mut std::ffi::c_void, field: u32) -> i32,
    pub expire: unsafe extern "C" fn(context: *mut std::ffi::c_void, index: u32),
    pub refresh: unsafe extern "C" fn(context: *mut std::ffi::c_void),
}

impl GoalQueueHost for &EeGoalQueueHost {
    // SAFETY (every call): the table's contract: `context` is live for the FFI call that handed it out
    fn write_u32(&mut self, value: u32) -> i32 {
        unsafe { (self.write_u32)(self.context, value) }
    }
    fn write_u8(&mut self, value: u8) -> i32 {
        unsafe { (self.write_u8)(self.context, value) }
    }
    fn write_bool(&mut self, value: bool) -> i32 {
        unsafe { (self.write_bool)(self.context, value) }
    }
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        unsafe { (self.read_u32)(self.context, value) }
    }
    fn read_u8(&mut self, value: &mut u8) -> i32 {
        unsafe { (self.read_u8)(self.context, value) }
    }
    fn save_goal(&mut self, goal: u64) -> i32 {
        unsafe { (self.save_goal)(self.context, goal) }
    }
    fn load_goal(&mut self, kind: u8) -> i32 {
        unsafe { (self.load_goal)(self.context, kind) }
    }
    fn clear(&mut self) {
        unsafe { (self.clear)(self.context) }
    }
    fn read_timer(&mut self, field: u32) -> i32 {
        unsafe { (self.read_timer)(self.context, field) }
    }
    fn expire(&mut self, index: u32) {
        unsafe { (self.expire)(self.context, index) }
    }
    fn refresh(&mut self) {
        unsafe { (self.refresh)(self.context) }
    }
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

/// A container's report: `count` goals at `entries` (id, kind, object) and its timer.
///
/// # Safety
/// `entries` points to `count` entries (or is null with `count` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queue_report(
    container: u64,
    unit: u64,
    entries: *const EeGoalQueueEntry,
    count: usize,
    timer: EeGoalQueueTimer,
) {
    let entries = slice(entries, count);
    with_goal_queues(|g| g.set_full(container, unit, entries, timer));
}

/// The container's save from its mirror (`save`): the status; -1 when there is no mirror.
///
/// # Safety
/// `host` points to a valid table for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queue_save(container: u64, host: *const EeGoalQueueHost) -> i32 {
    // SAFETY: the caller's contract
    let mut h = unsafe { &*host };
    save(container, &mut h).unwrap_or(-1)
}

/// The container's load (`load`): the status.
///
/// # Safety
/// `host` points to a valid table for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queue_load(host: *const EeGoalQueueHost) -> i32 {
    // SAFETY: the caller's contract
    let mut h = unsafe { &*host };
    load(&mut h)
}

/// The unit's goal queues' per-tick update (`update_unit`).
///
/// # Safety
/// `host` points to a valid table for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queues_update(unit: u64, count: u32, now: u32, host: *const EeGoalQueueHost) {
    // SAFETY: the caller's contract
    let mut h = unsafe { &*host };
    update_unit(unit, count, now, &mut h);
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
/// `ids`, and its timer) against the mirror: the mismatches (`GoalQueues::check_unit_timers`).
///
/// # Safety
/// `containers`, `lens` and `timers` point to `count` elements, `ids` to the sum of `lens` (each null when empty).
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queue_check_unit(
    unit: u64,
    containers: *const u64,
    lens: *const u32,
    timers: *const EeGoalQueueTimer,
    count: usize,
    ids: *const u32,
    ids_count: usize,
) -> u32 {
    let containers = slice(containers, count);
    let lens = slice(lens, count);
    let timers: Vec<Option<EeGoalQueueTimer>> = slice(timers, count).iter().map(|t| Some(*t)).collect();
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
    if timers.len() != list.len() {
        return u32::MAX;
    }
    with_goal_queues(|g| g.check_unit_timers(unit, &list, &timers))
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
            ee_units_goal_queue_check_unit(
                0xfeed_0000_0001,
                containers.as_ptr(),
                lens.as_ptr(),
                [EeGoalQueueTimer::default()].as_ptr(),
                1,
                ids.as_ptr(),
                2,
            )
        };
        assert_eq!(bad, 0);
        ee_units_goal_queue_drop(0xfeed_0000_1000);
        assert_eq!(ee_units_goal_queue_len(0xfeed_0000_0001, 0), -1);
    }

    /// A host that logs its calls and answers from scripted values.
    #[derive(Default)]
    struct Io {
        log: Vec<String>,
        reads_u32: Vec<(i32, u32)>,
        reads_u8: Vec<(i32, u8)>,
        fail_write_at: Option<usize>,
        writes: usize,
        load_results: Vec<i32>,
        timer_results: Vec<i32>,
    }

    impl GoalQueueHost for Io {
        fn write_u32(&mut self, value: u32) -> i32 {
            self.log.push(format!("w32 {value}"));
            self.writes += 1;
            if Some(self.writes) == self.fail_write_at {
                7
            } else {
                0
            }
        }
        fn write_u8(&mut self, value: u8) -> i32 {
            self.log.push(format!("w8 {value}"));
            self.writes += 1;
            if Some(self.writes) == self.fail_write_at {
                7
            } else {
                0
            }
        }
        fn write_bool(&mut self, value: bool) -> i32 {
            self.log.push(format!("wb {value}"));
            0
        }
        fn read_u32(&mut self, value: &mut u32) -> i32 {
            let (r, v) = self.reads_u32.remove(0);
            *value = v;
            self.log.push(format!("r32 {v}"));
            r
        }
        fn read_u8(&mut self, value: &mut u8) -> i32 {
            let (r, v) = self.reads_u8.remove(0);
            *value = v;
            self.log.push(format!("r8 {v}"));
            r
        }
        fn save_goal(&mut self, goal: u64) -> i32 {
            self.log.push(format!("save {goal:x}"));
            0
        }
        fn load_goal(&mut self, kind: u8) -> i32 {
            self.log.push(format!("load {kind}"));
            if self.load_results.is_empty() {
                0
            } else {
                self.load_results.remove(0)
            }
        }
        fn clear(&mut self) {
            self.log.push("clear".into());
        }
        fn read_timer(&mut self, field: u32) -> i32 {
            self.log.push(format!("timer {field}"));
            if self.timer_results.is_empty() {
                0
            } else {
                self.timer_results.remove(0)
            }
        }
        fn expire(&mut self, index: u32) {
            self.log.push(format!("expire {index}"));
        }
        fn refresh(&mut self) {
            self.log.push("refresh".into());
        }
    }

    fn entry(id: u32, kind: u8, goal: u64) -> EeGoalQueueEntry {
        EeGoalQueueEntry { id, kind, goal }
    }

    #[test]
    fn the_save_writes_the_mirror() {
        let c = 0x5a5e_0000_1000u64;
        let timer = EeGoalQueueTimer { start: 100, time: 60_000, started: 1 };
        with_goal_queues(|g| g.set_full(c, 1, &[entry(5, 1, 0xa0), entry(6, 9, 0xb0)], timer));
        let mut io = Io::default();
        assert_eq!(save(c, &mut io), Some(0));
        assert_eq!(io.log, ["w32 2", "w8 1", "save a0", "w8 9", "save b0", "w32 100", "w32 60000", "wb true"]);
        // a failing kind write: no goal save, the loop's next turn returns it
        let mut io = Io { fail_write_at: Some(2), ..Default::default() };
        assert_eq!(save(c, &mut io), Some(7));
        assert_eq!(io.log, ["w32 2", "w8 1"]);
        assert_eq!(save(0x5a5e_dead, &mut Io::default()), None);
        with_goal_queues(|g| g.drop_queue(c));
    }

    #[test]
    fn the_load_reads_the_record() {
        let mut io = Io { reads_u32: vec![(0, 2)], reads_u8: vec![(0, 1), (0, 9)], ..Default::default() };
        assert_eq!(load(&mut io), 0);
        assert_eq!(io.log, ["clear", "r32 2", "r8 1", "load 1", "r8 9", "load 9", "timer 0", "timer 1", "timer 2"]);
        // a failed goal load ends the loop and skips the timer
        let mut io =
            Io { reads_u32: vec![(0, 3)], reads_u8: vec![(0, 1)], load_results: vec![4], ..Default::default() };
        assert_eq!(load(&mut io), 4);
        assert_eq!(io.log, ["clear", "r32 3", "r8 1", "load 1"]);
        // a failed kind read: the next turn returns its status
        let mut io = Io { reads_u32: vec![(0, 2)], reads_u8: vec![(3, 0)], ..Default::default() };
        assert_eq!(load(&mut io), 3);
        assert_eq!(io.log, ["clear", "r32 2", "r8 0"]);
        // the timer's reads stop at the first failure
        let mut io = Io { reads_u32: vec![(0, 0)], timer_results: vec![0, 5], ..Default::default() };
        assert_eq!(load(&mut io), 5);
        assert_eq!(io.log, ["clear", "r32 0", "timer 0", "timer 1"]);
    }

    #[test]
    fn the_update_expires_started_queues() {
        let unit = 0x0bd0_0000_0001u64;
        let started = |start: u32, time: u32| EeGoalQueueTimer { start, time, started: 1 };
        with_goal_queues(|g| {
            // queue 0: started, not expired; 1: not started; 2: started and empty; 3: expired (wrapping sum)
            g.set_full(0x0bd0_1000, unit, &[entry(1, 1, 1)], started(1000, 5000));
            g.set_full(0x0bd0_1028, unit, &[entry(2, 1, 2)], EeGoalQueueTimer::default());
            g.set_full(0x0bd0_1050, unit, &[], started(0, 0));
            g.set_full(0x0bd0_1078, unit, &[entry(3, 1, 3)], started(u32::MAX - 10, 20));
        });
        let mut io = Io::default();
        update_unit(unit, 4, 5999, &mut io);
        assert_eq!(io.log, ["refresh", "expire 3", "refresh"]);
        let mut io = Io::default();
        update_unit(unit, 4, 6000, &mut io);
        assert_eq!(io.log, ["expire 0", "refresh", "expire 3", "refresh"]);
        // a count past the mirror's queues reads nothing more
        let mut io = Io::default();
        update_unit(unit, 6, 0, &mut io);
        assert_eq!(io.log, ["refresh", "refresh"]);
        with_goal_queues(|g| {
            for c in [0x0bd0_1000, 0x0bd0_1028, 0x0bd0_1050, 0x0bd0_1078] {
                g.drop_queue(c);
            }
        });
    }

    #[test]
    fn the_check_compares_the_timers() {
        let mut g = GoalQueues::new();
        let t = EeGoalQueueTimer { start: 1, time: 2, started: 1 };
        g.set_full(0x10, 0xaa, &[entry(5, 1, 0x50)], t);
        let list: [(u64, &[u32]); 1] = [(0x10, &[5])];
        assert_eq!(g.check_unit_timers(0xaa, &list, &[Some(t)]), 0);
        assert_eq!(g.check_unit_timers(0xaa, &list, &[Some(EeGoalQueueTimer { time: 3, ..t })]), 1);
    }
}
