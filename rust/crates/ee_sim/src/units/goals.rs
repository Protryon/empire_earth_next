//! The units' goal queues whole in Rust (units behaviour lane, round 3; before it a mirror of the C++ containers):
//! each `EEGoalQueueContainer` (`EEUnitVectorItem`, one of a complex unit's `mGoalQueues`) is the handle of one
//! [`EeUnitsGoalQueue`]: its unit, its goals (goal objects, goal_obj.rs) in queue order and its timer (`mStartTime`,
//! `mTime`, `mStarted`). The C++ container's members are inline calls of the entry points below
//! (ee/EEUnitVectorItem.h); the goals' deletes and copies are goal_obj.rs's. A unit's queues are read through its
//! `mGoalQueues` vector (the layout's `complex_goal_queues`: one queue pointer per container).
//!
//! The container's save and load (`save`, `load`: its `EEGoalQueueContainer` record) and the per-tick update
//! (`update_unit`: the timer's expiry, then the goals' refresh) are Rust over a host ([`GoalQueueHost`]): the C++
//! file's typed reads and writes (ee_base's `CppFile`), the goals' own `Save`/`CreateGoal`+`Load` (goal_obj.rs over
//! the registered goal host) and the unit's refresh (units round 11: EEGoalQueueContainer.cpp deleted).

// `c_long` and `c_ulong` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::units::goal_obj;
use crate::units::rust_goal::{EE_UNITS_GOAL_CLONE, EE_UNITS_GOAL_SET_OWNER, EE_UNITS_GOAL_START, EE_UNITS_GOAL_SUITS};

/// One goal queue: its unit (`mUnit`'s address), its goals' objects in queue order (0: a null entry) and its timer.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeUnitsGoalQueue {
    pub unit: u64,
    pub goals: Vec<u64>,
    pub timer: GoalTimer,
}

/// A queue's timer (`mStartTime`, `mTime`, `mStarted`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GoalTimer {
    pub start: c_ulong,
    pub time: c_ulong,
    pub started: bool,
}

/// The game time (`gPhysics->mGameTime`; 0 without a world).
fn game_time() -> c_ulong {
    #[cfg(not(test))]
    {
        // SAFETY: the queues are used by the simulation holding the world
        let t = unsafe { crate::world::WorldRead::begin() };
        crate::world::WorldView::global(&t).map_or(0, |w| w.game_time() as c_long as c_ulong)
    }
    #[cfg(test)]
    {
        tests::NOW.with(|t| t.get())
    }
}

/// The goal world the queue calls (goal_obj.rs; a fake in the tests).
pub trait GoalWorld {
    /// `inGoal->Clone()`: the copy's handle
    fn clone_goal(&mut self, goal: u64) -> u64;
    /// `inGoal->SetOwner(inUnit)`
    fn set_owner(&mut self, goal: u64, unit: u64);
    /// `delete inGoal`
    fn delete(&mut self, goal: u64);
}

/// The game's goals.
pub struct Goals;

impl GoalWorld for Goals {
    fn clone_goal(&mut self, goal: u64) -> u64 {
        // SAFETY: a live goal of a queue
        unsafe {
            goal_obj::call(EE_UNITS_GOAL_CLONE, goal, 0, u64::from(goal_obj::base(goal).b1c), std::ptr::null_mut())
                as u64
        }
    }
    fn set_owner(&mut self, goal: u64, unit: u64) {
        // SAFETY: as above
        unsafe { goal_obj::call(EE_UNITS_GOAL_SET_OWNER, goal, unit, 0, std::ptr::null_mut()) };
    }
    fn delete(&mut self, goal: u64) {
        // SAFETY: a goal the queue held, no longer listed
        unsafe { goal_obj::goal_delete(goal) };
    }
}

impl EeUnitsGoalQueue {
    /// `Clear()` (0x2057a7): every goal deleted (front to back, the list kept until all are), the timer stopped.
    ///
    /// # Safety
    /// `q` (and `other`) are live queues.
    pub unsafe fn clear(q: *mut EeUnitsGoalQueue, w: &mut dyn GoalWorld) {
        // SAFETY (each access): the live queue; a goal's destroy may read it (never free it)
        let n = unsafe { (*q).goals.len() };
        for i in 0..n {
            let goals = unsafe { &(*q).goals };
            let goal = goals.get(i).copied().unwrap_or(0);
            if goal != 0 {
                w.delete(goal);
            }
        }
        unsafe {
            (*q).goals.clear();
            (*q).timer = GoalTimer::default();
        }
    }

    /// The copy (the copy constructor 0x20569d and `operator=`): the other queue's unit, its goals cloned last to
    /// first (the ids drawn in that order) and owned by the unit, its timer; this queue cleared first.
    ///
    /// # Safety
    /// `q` (and `other`) are live queues.
    pub unsafe fn assign(q: *mut EeUnitsGoalQueue, other: *const EeUnitsGoalQueue, w: &mut dyn GoalWorld) {
        if std::ptr::eq(q, other) {
            return;
        }
        // SAFETY (each access): two live queues
        unsafe { (*q).unit = (*other).unit };
        Self::clear(q, w);
        let n = unsafe { (*other).goals.len() };
        unsafe { (*q).goals = vec![0; n] };
        for i in (0..n).rev() {
            let copy = w.clone_goal(unsafe { (&(*other).goals)[i] });
            unsafe { (&mut (*q).goals)[i] = copy };
            w.set_owner(copy, unsafe { (*q).unit });
        }
        unsafe { (*q).timer = (*other).timer };
    }

    /// `GetTimeLeft()` (0x205807): the time left on the running timer, 0 when stopped.
    pub fn time_left(&self, now: c_ulong) -> c_ulong {
        if self.timer.started {
            self.timer.time.wrapping_sub(now).wrapping_add(self.timer.start)
        } else {
            0
        }
    }

    /// `AddTime(inTime)` (0x205821): the timer started now the first time, then lengthened.
    pub fn add_time(&mut self, time: c_long, now: c_ulong) {
        if !self.timer.started {
            self.timer.started = true;
            self.timer.start = now;
        }
        self.timer.time = self.timer.time.wrapping_add(time as c_ulong);
    }

    /// `Insert(inIndex, inGoal)` (0x205843): at the index (appended past the end), owned by the unit.
    pub fn insert_owned(&mut self, index: usize, goal: u64, w: &mut dyn GoalWorld) {
        if index < self.goals.len() {
            self.goals.insert(index, goal);
        } else {
            self.goals.push(goal);
        }
        w.set_owner(goal, self.unit);
    }

    /// `PopFront()` / `PopBack()`: the goal deleted, then taken off.
    ///
    /// # Safety
    /// `q` (and `other`) are live queues.
    pub unsafe fn pop_delete(q: *mut EeUnitsGoalQueue, front: bool, w: &mut dyn GoalWorld) {
        // SAFETY (each access): the live queue
        let goal = unsafe {
            if front {
                (*q).goals.first().copied()
            } else {
                (*q).goals.last().copied()
            }
        };
        let Some(goal) = goal else { return };
        if goal != 0 {
            w.delete(goal);
        }
        unsafe {
            if front {
                (*q).goals.remove(0);
            } else {
                (*q).goals.pop();
            }
        }
    }
}

/// A unit's goal queues in index order (its `mGoalQueues`' queue pointers; none for another unit).
pub fn unit_queues(unit: u64) -> Vec<*mut EeUnitsGoalQueue> {
    // SAFETY: the callers hold the world
    let t = unsafe { crate::world::WorldRead::begin() };
    // SAFETY: a live unit handle
    unsafe { crate::world::UnitView::of(&t, unit) }
        .map(|u| u.goal_queues().into_iter().map(|q| q as usize as *mut EeUnitsGoalQueue).collect())
        .unwrap_or_default()
}

/// `GetQueueGoals(index).size()` of a unit, when it has the queue.
pub fn queue_len(unit: u64, index: usize) -> Option<usize> {
    #[cfg(test)]
    {
        tests::FAKE_LENS.with(|m| m.borrow().get(&unit).and_then(|v| v.get(index).copied()))
    }
    #[cfg(not(test))]
    {
        // SAFETY: a live queue of the unit
        unit_queues(unit).get(index).map(|q| unsafe { (**q).goals.len() })
    }
}

/// The queue codec's and update's host: typed `FSFile` writes and reads (the C++ `Write`/`Read` overloads), the
/// goals' own save and load, the unit's refresh; a status of 0 is `kErrorOK`.
pub trait GoalQueueHost {
    fn write_u32(&mut self, value: u32) -> i32;
    fn write_u8(&mut self, value: u8) -> i32;
    fn write_bool(&mut self, value: bool) -> i32;
    fn read_u32(&mut self, value: &mut u32) -> i32;
    fn read_u8(&mut self, value: &mut u8) -> i32;
    fn read_bool(&mut self, value: &mut bool) -> i32;
    /// `theGoal->Save(...)` of the goal object `goal`
    fn save_goal(&mut self, goal: u64) -> i32;
    /// `CreateGoal(kind)` and its `Load`: the status and the goal (deleted by the host on a failure: 0)
    fn load_goal(&mut self, kind: u8, goal: &mut u64) -> i32;
    /// the unit's `RefreshGoals()` and `RefreshActiveGoal()`
    fn refresh(&mut self);
}

/// `EEGoalQueueContainer::Save` (0x6059ea): the goal count, each goal's kind (a byte) and its own record, then the
/// timer. The first failing status ends it.
pub fn save(q: &EeUnitsGoalQueue, host: &mut dyn GoalQueueHost) -> i32 {
    let mut result = host.write_u32(q.goals.len() as u32);
    if result != 0 {
        return result;
    }
    for goal in &q.goals {
        if result != 0 {
            return result;
        }
        // SAFETY: a live goal of the queue
        let kind = if *goal != 0 { unsafe { goal_obj::base(*goal).kind } } else { 0 };
        result = host.write_u8(kind);
        if result == 0 {
            result = host.save_goal(*goal);
        }
    }
    if result != 0 {
        return result;
    }
    result = host.write_u32(q.timer.start as u32);
    if result != 0 {
        return result;
    }
    result = host.write_u32(q.timer.time as u32);
    if result == 0 {
        result = host.write_bool(q.timer.started);
    }
    result
}

/// `EEGoalQueueContainer::Load` (0x60588b): the queue cleared, the count, each goal's kind and record (a failed
/// kind read skips to the next turn, which ends the loop), then the timer's three fields (each read gets the field's
/// value in).
///
/// # Safety
/// `q` is a live queue.
pub unsafe fn load(q: *mut EeUnitsGoalQueue, host: &mut dyn GoalQueueHost, w: &mut dyn GoalWorld) -> i32 {
    EeUnitsGoalQueue::clear(q, w);
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
        let mut goal = 0u64;
        result = host.load_goal(kind, &mut goal);
        if result == 0 {
            // SAFETY: the live queue
            unsafe { (*q).goals.push(goal) };
        }
    }
    if result == 0 {
        // SAFETY (each access): the live queue
        let mut timer = unsafe { (*q).timer };
        let mut start = timer.start as u32;
        result = host.read_u32(&mut start);
        timer.start = start as c_ulong;
        if result == 0 {
            let mut time = timer.time as u32;
            result = host.read_u32(&mut time);
            timer.time = time as c_ulong;
            if result == 0 {
                result = host.read_bool(&mut timer.started);
            }
        }
        unsafe { (*q).timer = timer };
    }
    result
}

/// The unit's goal queues' per-tick update (the classes' `Update`'s goal-queue part, `EEGoalQueueContainer::Update`
/// 0x605780 inlined): every started, non-empty queue in index order expires (cleared, its timer stopped) when the
/// game time reaches its start plus its time (wrapping `unsigned long`s), then the unit's goals are refreshed. The
/// queues are read again for each index (the refresh may change them).
pub fn update_unit(unit: u64, now: c_ulong, host: &mut dyn GoalQueueHost, w: &mut dyn GoalWorld) {
    let mut index = 0usize;
    loop {
        let queues = unit_queues(unit);
        let Some(q) = queues.get(index).copied() else { break };
        index += 1;
        // SAFETY: a live queue of the unit
        let (timer, len) = unsafe { ((*q).timer, (*q).goals.len()) };
        if !timer.started || len == 0 {
            continue;
        }
        if now >= timer.start.wrapping_add(timer.time) {
            unsafe { EeUnitsGoalQueue::clear(q, w) };
        }
        host.refresh();
    }
}

/// The goals' own records in a queue's file (`theGoal->Save(...)`, `CreateGoal(kind)` and its `Load`, `delete
/// theGoal`): goal_obj.rs's bodies over the registered goal host, or a fake in the tests.
pub trait GoalRecords {
    /// `EEUnitGoal::CreateGoal(kind)`: an empty goal (0 for an unknown kind)
    fn create(&mut self, kind: u8) -> u64;
    /// `theGoal->Load(major, minor, file)`
    fn load(&mut self, goal: u64) -> i32;
    /// `theGoal->Save(major, minor, file)`
    fn save(&mut self, goal: u64) -> i32;
    /// `delete theGoal`
    fn delete(&mut self, goal: u64);
}

/// `QueueLoadGoal` (the old EEGoalQueueContainer.cpp): `CreateGoal` and its `Load`, the goal kept on success and
/// deleted otherwise (0 then). An unknown kind (no goal: the original read through NULL) fails the load.
pub fn load_goal_record(records: &mut dyn GoalRecords, kind: u8, goal: &mut u64) -> i32 {
    let made = records.create(kind);
    if made == 0 {
        *goal = 0;
        return ee_base::fs::status::FAILED;
    }
    let result = records.load(made);
    *goal = made;
    if result != ee_base::fs::status::OK {
        records.delete(made);
        *goal = 0;
    }
    result
}

/// The C++ goal body's file context (EEUGRust.cpp's `EEGoalContext`: the file, the text, the dump; one pointer
/// each, in that order): what the registered goal host's file cases read.
#[repr(C)]
struct GoalFileContext {
    file: *mut std::ffi::c_void,
    text: *mut std::ffi::c_void,
    dump: *mut std::ffi::c_void,
}

/// The game's goal records over a C++ `FSFile*` and the save's version.
struct FileRecords {
    file: *mut std::ffi::c_void,
    major: c_ulong,
    minor: c_ulong,
}

impl FileRecords {
    fn body(&mut self, op: i32, goal: u64) -> i32 {
        let mut context = GoalFileContext { file: self.file, text: std::ptr::null_mut(), dump: std::ptr::null_mut() };
        // SAFETY: a live goal; the context lives across the call (EEUGRust::Load/Save's `Goal(op, this, major, minor,
        // NULL, &file)`)
        unsafe {
            goal_obj::call_in(op, goal, self.major as u64, self.minor as u64, std::ptr::addr_of_mut!(context).cast())
                as i32
        }
    }
}

impl GoalRecords for FileRecords {
    fn create(&mut self, kind: u8) -> u64 {
        crate::units::goal_calls::ee_units_goal_create(kind) as usize as u64
    }
    fn load(&mut self, goal: u64) -> i32 {
        self.body(crate::units::rust_goal::EE_UNITS_GOAL_LOAD, goal)
    }
    fn save(&mut self, goal: u64) -> i32 {
        self.body(crate::units::rust_goal::EE_UNITS_GOAL_SAVE, goal)
    }
    fn delete(&mut self, goal: u64) {
        // SAFETY: the goal just made and failed, owned by no one
        unsafe { goal_obj::goal_delete(goal) }
    }
}

/// The codec's host over a C++ `FSFile*` (its virtual `Read`/`Write` overloads through ee_base's file calls; a
/// `bool` is its byte) and the goals' records; the update's refresh is the unit's.
struct FileQueueHost<R: GoalRecords> {
    file: ee_base::fs::CppFile,
    records: R,
    unit: u64,
}

impl<R: GoalRecords> GoalQueueHost for FileQueueHost<R> {
    fn write_u32(&mut self, value: u32) -> i32 {
        ee_base::fs::FsWrite::write_u32(&mut self.file, value)
    }
    fn write_u8(&mut self, value: u8) -> i32 {
        self.file.write_u8(value)
    }
    fn write_bool(&mut self, value: bool) -> i32 {
        self.file.write_u8(u8::from(value))
    }
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        ee_base::fs::FsFile::read_u32(&mut self.file, value)
    }
    fn read_u8(&mut self, value: &mut u8) -> i32 {
        self.file.read_u8(value)
    }
    fn read_bool(&mut self, value: &mut bool) -> i32 {
        let mut byte = u8::from(*value);
        let result = self.file.read_u8(&mut byte);
        *value = byte != 0;
        result
    }
    fn save_goal(&mut self, goal: u64) -> i32 {
        self.records.save(goal)
    }
    fn load_goal(&mut self, kind: u8, goal: &mut u64) -> i32 {
        load_goal_record(&mut self.records, kind, goal)
    }
    fn refresh(&mut self) {
        refresh_goals(self.unit);
        refresh_active_goal(self.unit);
    }
}

fn file_host(file: *mut std::ffi::c_void, major: c_ulong, minor: c_ulong, unit: u64) -> FileQueueHost<FileRecords> {
    FileQueueHost { file: ee_base::fs::CppFile(file), records: FileRecords { file, major, minor }, unit }
}

// ---- the container's entry points (ee/EEUnitVectorItem.h's inline members; `q` a live queue throughout)

/// `EEUnitVectorItem(inUnit)`: an empty queue of the unit, its timer stopped.
#[no_mangle]
pub extern "C" fn ee_units_gq_new(unit: u64) -> *mut EeUnitsGoalQueue {
    Box::into_raw(Box::new(EeUnitsGoalQueue { unit, ..Default::default() }))
}

/// `~EEUnitVectorItem()`: the queue cleared (its goals deleted), then freed.
///
/// # Safety
/// `q` is a queue made by [`ee_units_gq_new`] or null, used by no one afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_free(q: *mut EeUnitsGoalQueue) {
    if q.is_null() {
        return;
    }
    EeUnitsGoalQueue::clear(q, &mut Goals);
    // SAFETY: the caller's contract
    drop(unsafe { Box::from_raw(q) });
}

/// The copy (the copy constructor, `operator=`): see [`EeUnitsGoalQueue::assign`].
///
/// # Safety
/// Both are live queues.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_assign(q: *mut EeUnitsGoalQueue, other: *const EeUnitsGoalQueue) {
    EeUnitsGoalQueue::assign(q, other, &mut Goals);
}

/// `Clear()`.
///
/// # Safety
/// `q` is a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_clear(q: *mut EeUnitsGoalQueue) {
    EeUnitsGoalQueue::clear(q, &mut Goals);
}

/// `mUnit`.
///
/// # Safety
/// `q` is a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_unit(q: *const EeUnitsGoalQueue) -> u64 {
    // SAFETY: the caller's contract
    unsafe { (*q).unit }
}

/// `mGoals.size()`.
///
/// # Safety
/// `q` is a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_len(q: *const EeUnitsGoalQueue) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*q).goals.len() }
}

/// `mGoals[index]` (0 past the end).
///
/// # Safety
/// `q` is a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_get(q: *const EeUnitsGoalQueue, index: usize) -> u64 {
    // SAFETY: the caller's contract
    let goals = unsafe { &(*q).goals };
    goals.get(index).copied().unwrap_or(0)
}

/// The deque's edits (no goal deleted or owned): 0 `push_back`, 1 `push_front`, 2 `pop_back`, 3 `pop_front`,
/// 4 `insert(index)`, 5 `erase(index .. index + count)`, 6 `clear`, 7 `resize(index, goal)`, 8 `Set(index, goal)`.
///
/// # Safety
/// `q` is a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_edit(q: *mut EeUnitsGoalQueue, edit: i32, index: usize, count: usize, goal: u64) {
    // SAFETY: the caller's contract
    let goals = unsafe { &mut (*q).goals };
    match edit {
        0 => goals.push(goal),
        1 => goals.insert(0, goal),
        2 => {
            goals.pop();
        }
        3 => {
            if !goals.is_empty() {
                goals.remove(0);
            }
        }
        4 => goals.insert(index.min(goals.len()), goal),
        5 => {
            let end = index.saturating_add(count).min(goals.len());
            goals.drain(index.min(end)..end);
        }
        6 => goals.clear(),
        7 => goals.resize(index, goal),
        8 => {
            if let Some(g) = goals.get_mut(index) {
                *g = goal;
            }
        }
        _ => {}
    }
}

/// The container's goal operations, the unit owning what it adds: 0 `Add` (push_back), 1 `AddFront`, 2
/// `Insert(index)`, 3 `PopBack` (deleted), 4 `PopFront` (deleted).
///
/// # Safety
/// `q` is a live queue, `goal` a live goal for the adds.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_goal_op(q: *mut EeUnitsGoalQueue, op: i32, index: usize, goal: u64) {
    let w = &mut Goals;
    match op {
        // SAFETY (each): the caller's contract
        0..=2 => {
            let at = match op {
                0 => usize::MAX,
                1 => 0,
                _ => index,
            };
            unsafe { (*q).insert_owned(at, goal, w) };
        }
        3 => EeUnitsGoalQueue::pop_delete(q, false, w),
        4 => EeUnitsGoalQueue::pop_delete(q, true, w),
        _ => {}
    }
}

/// The timer: 0 `GetTimeLeft()`, 1 `AddTime(time)` (0 returned), 2 `ResetTimer()`.
///
/// # Safety
/// `q` is a live queue.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_timer(q: *mut EeUnitsGoalQueue, op: i32, time: c_long) -> c_ulong {
    // SAFETY: the caller's contract
    let q = unsafe { &mut *q };
    match op {
        0 => q.time_left(game_time()),
        1 => {
            q.add_time(time, game_time());
            0
        }
        _ => {
            q.timer = GoalTimer::default();
            0
        }
    }
}

/// `EEGoalQueueContainer::Save(major, minor, file)` (`save` over the C++ file): the status.
///
/// # Safety
/// `q` is a live queue, `file` a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_save_file(
    q: *const EeUnitsGoalQueue,
    file: *mut std::ffi::c_void,
    major: c_ulong,
    minor: c_ulong,
) -> c_long {
    // SAFETY: the caller's contract
    let q = unsafe { &*q };
    save(q, &mut file_host(file, major, minor, q.unit)) as c_long
}

/// `EEGoalQueueContainer::Load(major, minor, file)` (`load` over the C++ file): the status.
///
/// # Safety
/// `q` is a live queue, `file` a live C++ `FSFile*`.
#[no_mangle]
pub unsafe extern "C" fn ee_units_gq_load_file(
    q: *mut EeUnitsGoalQueue,
    file: *mut std::ffi::c_void,
    major: c_ulong,
    minor: c_ulong,
) -> c_long {
    // SAFETY: the caller's contract
    let unit = unsafe { (*q).unit };
    load(q, &mut file_host(file, major, minor, unit), &mut Goals) as c_long
}

/// The unit's goal queues' per-tick update (`update_unit`, the refresh the unit's `RefreshGoals` and
/// `RefreshActiveGoal`).
///
/// # Safety
/// `unit` is a live complex unit.
#[no_mangle]
pub unsafe extern "C" fn ee_units_goal_queues_tick(unit: u64) {
    update_unit(unit, game_time(), &mut file_host(std::ptr::null_mut(), 0, 0, unit), &mut Goals);
}

/// The length of a unit's queue `index`; -1 when it has no such queue.
#[no_mangle]
pub extern "C" fn ee_units_goal_queue_len(unit: u64, index: u32) -> i64 {
    queue_len(unit, index as usize).map_or(-1, |n| n as i64)
}

// ---- the unit's goal calls (EEComplexUnit's GetQueuedGoal, RefreshGoals, RefreshActiveGoal, SetGoal,
// DropUnsuitedGoals over its queues and its mGoalState and mActiveGoal)

/// A pointer member of the complex unit `unit` at `offset` set to `value`.
fn set_unit_pointer(unit: u64, offset: u64, value: u64) {
    // SAFETY: a live complex unit's pointer member (the layout's offset); no reference to it is held
    unsafe { *((unit as usize + offset as usize) as *mut usize) = value as usize };
}

fn set_goal_state(unit: u64, goal: u64) {
    set_unit_pointer(unit, crate::world::layout::layout().complex_goal_state, goal);
}

fn set_active_goal(unit: u64, goal: u64) {
    set_unit_pointer(unit, crate::world::layout::layout().complex_active_goal, goal);
}

/// Whether a goal is an automatic waypoint (a location goal with `mIsAutoWaypoint`).
fn is_auto_waypoint(goal: u64) -> bool {
    // SAFETY: a live goal
    goal != 0
        && unsafe { goal_obj::base(goal).kind } == crate::units::location::GOAL_LOCATION
        && crate::units::location::get(goal).auto_waypoint
}

/// `GetQueuedGoal(inIndex)`: the queue's first goal (0: no such queue or none).
pub fn queued_goal(unit: u64, index: usize) -> u64 {
    // SAFETY: a live queue of the unit
    unit_queues(unit).get(index).map_or(0, |q| unsafe { (**q).goals.first().copied().unwrap_or(0) })
}

/// `RefreshGoals()`: the current goal is the main queue's first; it.
pub fn refresh_goals(unit: u64) -> u64 {
    let goal = queued_goal(unit, 0);
    set_goal_state(unit, goal);
    goal
}

/// `RefreshActiveGoal()`: the active goal is the main queue's first goal that is no automatic waypoint.
pub fn refresh_active_goal(unit: u64) {
    let queues = unit_queues(unit);
    // SAFETY: the unit's live main queue (the C++ indexed it unchecked)
    let goals = queues.first().map(|q| unsafe { (**q).goals.clone() }).unwrap_or_default();
    let active = goals.into_iter().find(|g| !is_auto_waypoint(*g)).unwrap_or(0);
    set_active_goal(unit, active);
}

/// `SetGoal(inGoal, inKeepCurrent, inIndex)` (0x1fa2f5): the goal at the queue's front, the front goal kept
/// (restarted at the unit's action) or else dropped with its leading automatic waypoints; on the main queue it is
/// the current goal and, unless an automatic waypoint, the active one.
pub fn set_goal(unit: u64, goal: u64, keep: bool, index: usize, w: &mut dyn GoalWorld) {
    let Some(q) = unit_queues(unit).get(index).copied() else { return };
    // SAFETY (each access): the unit's live queue and goals
    if unsafe { !(*q).goals.is_empty() } {
        if keep {
            let front = unsafe { (&(*q).goals)[0] };
            // SAFETY: the world held by the caller
            let t = unsafe { crate::world::WorldRead::begin() };
            let action = unsafe { crate::world::UnitView::of(&t, unit) }.map_or(0, |u| u.command_type());
            unsafe {
                goal_obj::base(front).action = action as c_ulong;
                goal_obj::base(front).path_attempts = 0;
                goal_obj::call(
                    crate::units::rust_goal::EE_UNITS_GOAL_SET_PATH_ATTEMPTS,
                    front,
                    0,
                    0,
                    std::ptr::null_mut(),
                );
            }
        } else {
            if index < 1 {
                set_active_goal(unit, 0);
            }
            while is_auto_waypoint(queued_goal(unit, index)) {
                unsafe { EeUnitsGoalQueue::pop_delete(q, true, w) };
            }
            if unsafe { !(*q).goals.is_empty() } {
                unsafe { EeUnitsGoalQueue::pop_delete(q, true, w) };
            }
        }
    }
    unsafe { (*q).insert_owned(0, goal, w) };
    // SetIsFiringHigh(false)
    // SAFETY: as above
    let t = unsafe { crate::world::WorldRead::begin() };
    if let Some(u) = unsafe { crate::world::UnitView::of(&t, unit) } {
        let cx = u.complex().ptr() as *mut crate::world::unit_fields::EeUnitsComplexUnitFields;
        // SAFETY: the live unit's record
        unsafe { (*cx).mIsFiringHigh = false };
    }
    if index != 0 {
        return;
    }
    if !is_auto_waypoint(goal) {
        set_active_goal(unit, goal);
    }
    set_goal_state(unit, goal);
}

/// `DropUnsuitedGoals(inIndex)` (0x1fbf66): the queue's front goals that no longer suit the unit deleted, then the
/// current goal and (when any went) the active one refreshed.
pub fn drop_unsuited_goals(unit: u64, index: usize, w: &mut dyn GoalWorld) {
    let Some(q) = unit_queues(unit).get(index).copied() else { return };
    let mut popped = false;
    // SAFETY (each access): the unit's live queue and goals
    while let Some(front) = unsafe { (*q).goals.first().copied() } {
        if unsafe { goal_obj::call(crate::units::rust_goal::EE_UNITS_GOAL_SUITS, front, unit, 0, std::ptr::null_mut()) }
            != 0
        {
            break;
        }
        unsafe { EeUnitsGoalQueue::pop_delete(q, true, w) };
        popped = true;
    }
    refresh_goals(unit);
    if popped {
        refresh_active_goal(unit);
    }
}

/// `GetQueuedGoal(inIndex)`.
#[no_mangle]
pub extern "C" fn ee_units_unit_queued_goal(unit: u64, index: u32) -> u64 {
    queued_goal(unit, index as usize)
}

/// `RefreshGoals()`: the new current goal.
#[no_mangle]
pub extern "C" fn ee_units_unit_refresh_goals(unit: u64) -> u64 {
    refresh_goals(unit)
}

/// `RefreshActiveGoal()`.
#[no_mangle]
pub extern "C" fn ee_units_unit_refresh_active_goal(unit: u64) {
    refresh_active_goal(unit);
}

/// `SetGoal(inGoal, inKeepCurrent, inIndex)`.
#[no_mangle]
pub extern "C" fn ee_units_unit_set_goal(unit: u64, goal: u64, keep: bool, index: i32) {
    if index >= 0 {
        set_goal(unit, goal, keep, index as usize, &mut Goals);
    }
}

/// `DropUnsuitedGoals(inIndex)`.
#[no_mangle]
pub extern "C" fn ee_units_unit_drop_unsuited_goals(unit: u64, index: u32) {
    drop_unsuited_goals(unit, index as usize, &mut Goals);
}

/// `DropFinishedGoals(inStartAll)` (0x1fcb0e): while the current goal is an automatic waypoint, it is deleted off
/// the main queue and the next goal starts when it suits the unit (every next one with `start_all`, else the first
/// that is no automatic waypoint); then the current and active goals refreshed.
pub fn drop_finished_goals(unit: u64, start_all: bool, w: &mut dyn GoalWorld) {
    // SAFETY: the caller holds the world
    let t = unsafe { crate::world::WorldRead::begin() };
    let current = unsafe { crate::world::UnitView::of(&t, unit) }.map_or(0, |u| u.goal_handle());
    if is_auto_waypoint(current) {
        let queues = unit_queues(unit);
        if let Some(q) = queues.first().copied() {
            loop {
                unsafe { EeUnitsGoalQueue::pop_delete(q, true, w) };
                let next = refresh_goals(unit);
                if next == 0 {
                    break;
                }
                let auto = is_auto_waypoint(next);
                // SAFETY (each call): the unit's live goal
                if start_all || !auto {
                    let suits = unsafe { goal_obj::call(EE_UNITS_GOAL_SUITS, next, unit, 0, std::ptr::null_mut()) };
                    if suits != 0 {
                        unsafe { goal_obj::call(EE_UNITS_GOAL_START, next, unit, 0, std::ptr::null_mut()) };
                    }
                }
                if !auto {
                    break;
                }
            }
        }
    }
    refresh_goals(unit);
    refresh_active_goal(unit);
}

/// `DropFinishedGoals(inStartAll)`.
#[no_mangle]
pub extern "C" fn ee_units_unit_drop_finished_goals(unit: u64, start_all: bool) {
    drop_finished_goals(unit, start_all, &mut Goals);
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    thread_local! {
        pub(crate) static NOW: std::cell::Cell<c_ulong> = const { std::cell::Cell::new(0) };
        /// the queue lengths `queue_len` answers in the tests, by unit
        pub(crate) static FAKE_LENS: std::cell::RefCell<std::collections::BTreeMap<u64, Vec<usize>>> =
            const { std::cell::RefCell::new(std::collections::BTreeMap::new()) };
    }

    /// The test units' queue lengths (`queue_len`), none for an empty list.
    pub(crate) fn fake_lens(unit: u64, lens: &[usize]) {
        FAKE_LENS.with(|m| m.borrow_mut().insert(unit, lens.to_vec()));
    }

    /// Goals as numbers: a clone is the goal + 100 (numbered in order), deletes and owners logged.
    #[derive(Default)]
    struct Fake {
        log: Vec<String>,
    }

    impl GoalWorld for Fake {
        fn clone_goal(&mut self, goal: u64) -> u64 {
            self.log.push(format!("clone {goal}"));
            goal + 100
        }
        fn set_owner(&mut self, goal: u64, unit: u64) {
            self.log.push(format!("owner {goal} {unit}"));
        }
        fn delete(&mut self, goal: u64) {
            self.log.push(format!("delete {goal}"));
        }
    }

    fn queue(unit: u64, goals: &[u64]) -> EeUnitsGoalQueue {
        EeUnitsGoalQueue { unit, goals: goals.to_vec(), timer: GoalTimer::default() }
    }

    #[test]
    fn the_copy_clears_then_clones_last_to_first() {
        let mut w = Fake::default();
        let mut q = queue(1, &[5]);
        let mut other = queue(7, &[2, 3]);
        other.timer = GoalTimer { start: 10, time: 20, started: true };
        unsafe { EeUnitsGoalQueue::assign(&mut q, &other, &mut w) };
        assert_eq!(w.log, ["delete 5", "clone 3", "owner 103 7", "clone 2", "owner 102 7"]);
        assert_eq!(q.goals, [102, 103]);
        assert_eq!((q.unit, q.timer), (7, other.timer));
    }

    #[test]
    fn the_timer_starts_once_and_counts_down() {
        let mut q = queue(1, &[]);
        assert_eq!(q.time_left(50), 0);
        q.add_time(60000, 1000);
        q.add_time(5, 2000);
        assert_eq!(q.timer, GoalTimer { start: 1000, time: 60005, started: true });
        assert_eq!(q.time_left(31000), 30005);
    }

    #[test]
    fn inserts_own_and_pops_delete() {
        let mut w = Fake::default();
        let mut q = queue(4, &[1, 2]);
        q.insert_owned(1, 9, &mut w);
        q.insert_owned(10, 8, &mut w);
        assert_eq!(q.goals, [1, 9, 2, 8]);
        unsafe { EeUnitsGoalQueue::pop_delete(&mut q, true, &mut w) };
        unsafe { EeUnitsGoalQueue::pop_delete(&mut q, false, &mut w) };
        assert_eq!(q.goals, [9, 2]);
        assert_eq!(w.log, ["owner 9 4", "owner 8 4", "delete 1", "delete 8"]);
        unsafe { EeUnitsGoalQueue::clear(&mut q, &mut w) };
        assert!(q.goals.is_empty() && !q.timer.started);
    }

    #[test]
    fn the_deque_edits() {
        let q = ee_units_gq_new(3);
        // SAFETY: the queue just made (its goals are plain numbers: freed below without a clear)
        unsafe {
            ee_units_gq_edit(q, 0, 0, 0, 1);
            ee_units_gq_edit(q, 1, 0, 0, 2);
            ee_units_gq_edit(q, 4, 1, 0, 3);
            assert_eq!((*q).goals, [2, 3, 1]);
            ee_units_gq_edit(q, 5, 0, 2, 0);
            ee_units_gq_edit(q, 7, 3, 0, 6);
            ee_units_gq_edit(q, 8, 2, 0, 4);
            assert_eq!((*q).goals, [1, 6, 4]);
            assert_eq!((ee_units_gq_len(q), ee_units_gq_get(q, 1), ee_units_gq_get(q, 9)), (3, 6, 0));
            ee_units_gq_edit(q, 6, 0, 0, 0);
            drop(Box::from_raw(q));
        }
    }

    /// A file of words and the goals' records as their kinds.
    #[derive(Default)]
    struct File {
        words: Vec<u32>,
        at: usize,
    }

    impl GoalQueueHost for File {
        fn write_u32(&mut self, value: u32) -> i32 {
            self.words.push(value);
            0
        }
        fn write_u8(&mut self, value: u8) -> i32 {
            self.words.push(u32::from(value));
            0
        }
        fn write_bool(&mut self, value: bool) -> i32 {
            self.words.push(u32::from(value));
            0
        }
        fn read_u32(&mut self, value: &mut u32) -> i32 {
            *value = self.words[self.at];
            self.at += 1;
            0
        }
        fn read_u8(&mut self, value: &mut u8) -> i32 {
            *value = self.words[self.at] as u8;
            self.at += 1;
            0
        }
        fn read_bool(&mut self, value: &mut bool) -> i32 {
            *value = self.words[self.at] != 0;
            self.at += 1;
            0
        }
        fn save_goal(&mut self, goal: u64) -> i32 {
            self.words.push((goal as u32).wrapping_add(0x5000));
            0
        }
        fn load_goal(&mut self, kind: u8, goal: &mut u64) -> i32 {
            let record = self.words[self.at];
            self.at += 1;
            *goal = u64::from(record.wrapping_sub(0x5000)) + u64::from(kind) * 1000;
            0
        }
        fn refresh(&mut self) {}
    }

    /// Goal records as numbers: a made goal is the kind + 500, a load fails for an odd kind, every call logged.
    #[derive(Default)]
    struct Records {
        log: Vec<String>,
    }

    impl GoalRecords for Records {
        fn create(&mut self, kind: u8) -> u64 {
            self.log.push(format!("create {kind}"));
            if kind == 0xff {
                0
            } else {
                u64::from(kind) + 500
            }
        }
        fn load(&mut self, goal: u64) -> i32 {
            self.log.push(format!("load {goal}"));
            if goal % 2 == 1 {
                0x12
            } else {
                0
            }
        }
        fn save(&mut self, goal: u64) -> i32 {
            self.log.push(format!("save {goal}"));
            0
        }
        fn delete(&mut self, goal: u64) {
            self.log.push(format!("delete {goal}"));
        }
    }

    #[test]
    fn a_goal_record_is_kept_on_a_good_load_and_deleted_on_a_failed_one() {
        let mut r = Records::default();
        let mut goal = 7;
        assert_eq!(load_goal_record(&mut r, 4, &mut goal), 0);
        assert_eq!(goal, 504);
        assert_eq!(load_goal_record(&mut r, 3, &mut goal), 0x12);
        assert_eq!(goal, 0);
        assert_eq!(load_goal_record(&mut r, 0xff, &mut goal), ee_base::fs::status::FAILED);
        assert_eq!(goal, 0);
        assert_eq!(r.log, ["create 4", "load 504", "create 3", "load 503", "delete 503", "create 255"]);
    }

    #[test]
    fn the_record_round_trips() {
        let made =
            |kind| Box::into_raw(Box::new(goal_obj::EeUnitsGoalBase { kind, ..Default::default() })) as usize as u64;
        let mut goals = [made(9), made(1)];
        let mut q = queue(1, &goals);
        q.timer = GoalTimer { start: 3, time: 4, started: true };
        let mut f = File::default();
        assert_eq!(save(&q, &mut f), 0);
        let g = |i: usize| (goals[i] as u32).wrapping_add(0x5000);
        assert_eq!(f.words, [2, 9, g(0), 1, g(1), 3, 4, 1]);
        let mut back = queue(1, &[]);
        let mut w = Fake::default();
        assert_eq!(unsafe { load(&mut back, &mut f, &mut w) }, 0);
        assert_eq!(back.goals, [u64::from(goals[0] as u32) + 9000, u64::from(goals[1] as u32) + 1000]);
        assert_eq!(back.timer, q.timer);
        for goal in goals.iter_mut() {
            // SAFETY: the goals made above
            unsafe { goal_obj::dealloc(*goal) };
        }
    }
}
