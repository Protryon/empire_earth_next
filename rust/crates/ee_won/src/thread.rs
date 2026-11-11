//! WONCommon's threads as Rust objects (round 12): `Thread` (ThreadBase + Thread: the run flags, priority, signal
//! event and platform thread; the C++ class is a shim whose virtual `ThreadFunc` this object's thread calls back),
//! `TimerThread` (wholly here: the deadline queue, its lock, the thread's loop and signal; the ops' `Finish` and
//! `Kill` come back to the C++ as callbacks) and `AsyncOpContainer` (wholly here: the queue, the lock, the signal,
//! the pump and the purge; each op's steps come back as callbacks).
//!
//! The waits and locks are the platform layer's (`ee_plat_sys`: a recursive mutex as the C++ CriticalSection was,
//! auto-reset events whose waits follow the harness clock), so the callbacks run under the same locks, on the same
//! threads and in the same order as the C++ statements they replace (WONCommon's Thread.cpp, TimerThread.cpp and
//! AsyncOpContainer.cpp, deleted; their behaviour is `golden_asyncop.txt`, recorded over them).

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicPtr, AtomicU32, Ordering};

use ee_plat_sys::constants::{
    PLAT_PRIORITY_ABOVE_NORMAL, PLAT_PRIORITY_BELOW_NORMAL, PLAT_PRIORITY_HIGHEST, PLAT_PRIORITY_LOWEST,
    PLAT_PRIORITY_NORMAL, PLAT_WAIT_INFINITE,
};
use ee_plat_sys::thread::imp as plat;

use crate::asyncop::{OpHandle, OpQueue, PumpPlan, QueueResult, TimerQueue};
use crate::ops;

/// `ThreadPriority_Normal` (the enum: Highest, High, Normal, Low, Lowest).
pub const EE_WON_THREAD_PRIORITY_NORMAL: i32 = 2;

/// The platform priority of a `ThreadPriority` (`SetPriorityHook`'s switch; another value is normal).
pub fn platform_priority(priority: i32) -> i32 {
    match priority {
        0 => PLAT_PRIORITY_HIGHEST,
        1 => PLAT_PRIORITY_ABOVE_NORMAL,
        3 => PLAT_PRIORITY_BELOW_NORMAL,
        4 => PLAT_PRIORITY_LOWEST,
        _ => PLAT_PRIORITY_NORMAL,
    }
}

/// A recursive platform lock held for a scope.
struct Locked(*mut plat::PlatMutex);

impl Locked {
    fn new(m: *mut plat::PlatMutex) -> Locked {
        // SAFETY: `m` is the owner's live mutex
        unsafe { plat::PlatMutexLock(m) };
        Locked(m)
    }
}

impl Drop for Locked {
    fn drop(&mut self) {
        // SAFETY: locked in `new`
        unsafe { plat::PlatMutexUnlock(self.0) };
    }
}

// ---- Thread

/// What a thread runs: `run(context)` once (the C++ calls its virtual `ThreadFunc`), then, when the thread was set
/// to auto-delete, `destroy(context)` (the C++ deletes the object, and with it this one).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonThreadBody {
    pub run: Option<unsafe extern "C" fn(context: *mut c_void)>,
    pub destroy: Option<unsafe extern "C" fn(context: *mut c_void)>,
    pub context: *mut c_void,
}

/// `ThreadBase` + `Thread`'s state.
pub struct EeWonThread {
    stopped: AtomicBool,
    running: AtomicBool,
    auto_delete: AtomicBool,
    priority: AtomicI32,
    signal: *mut plat::PlatEvent,
    handle: AtomicPtr<plat::PlatThread>,
    thread_id: AtomicU32,
    body: EeWonThreadBody,
}

// SAFETY: the fields are atomics or platform handles made for use across threads; `body` is the C++ object's
// entry, called only on the thread this object starts
unsafe impl Send for EeWonThread {}
// SAFETY: as Send
unsafe impl Sync for EeWonThread {}

unsafe extern "C" fn thread_main(parameter: *mut c_void) -> u32 {
    // SAFETY: `start` passes the live object; the C++ owner outlives the thread unless it auto-deletes, which
    // happens last
    let t = unsafe { &*(parameter as *const EeWonThread) };
    // SAFETY: plain platform call
    t.thread_id.store(unsafe { plat::PlatCurrentThreadId() }, Ordering::SeqCst);
    if let Some(run) = t.body.run {
        // SAFETY: the body's contract
        unsafe { run(t.body.context) };
    }
    t.running.store(false, Ordering::SeqCst);
    if t.auto_delete.load(Ordering::SeqCst) {
        let (destroy, context) = (t.body.destroy, t.body.context);
        if let Some(destroy) = destroy {
            // SAFETY: the body's contract; `t` is not touched again
            unsafe { destroy(context) };
        }
    }
    0
}

impl EeWonThread {
    /// `ThreadBase()`: stopped, not running, normal priority, an auto-reset signal.
    pub fn new(body: EeWonThreadBody) -> EeWonThread {
        EeWonThread {
            stopped: AtomicBool::new(true),
            running: AtomicBool::new(false),
            auto_delete: AtomicBool::new(false),
            priority: AtomicI32::new(EE_WON_THREAD_PRIORITY_NORMAL),
            // SAFETY: plain platform call
            signal: unsafe { plat::PlatEventCreate(0, 0) },
            handle: AtomicPtr::new(null_mut()),
            thread_id: AtomicU32::new(0),
            body,
        }
    }

    pub fn is_stopped(&self) -> bool {
        self.stopped.load(Ordering::SeqCst)
    }

    pub fn is_running(&self) -> bool {
        self.running.load(Ordering::SeqCst)
    }

    /// `ThreadFunc`'s loops after a stop: `mStopped=false` (WONSound's thread restarts its loop).
    pub fn set_stopped(&self, stopped: bool) {
        self.stopped.store(stopped, Ordering::SeqCst)
    }

    pub fn set_auto_delete(&self, on: bool) {
        self.auto_delete.store(on, Ordering::SeqCst)
    }

    /// `Start`: a thread unless one runs (created suspended to apply a priority other than normal).
    ///
    /// # Safety
    /// `self` stays at its address while the thread runs.
    pub unsafe fn start(&self) {
        if self.running.load(Ordering::SeqCst) {
            return;
        }
        self.stopped.store(false, Ordering::SeqCst);
        self.running.store(true, Ordering::SeqCst);
        let priority = self.priority.load(Ordering::SeqCst);
        let suspended = i32::from(priority != EE_WON_THREAD_PRIORITY_NORMAL);
        let mut id = 0u32;
        // SAFETY: `thread_main` gets this object, alive per the contract
        let handle = unsafe {
            plat::PlatThreadCreate(thread_main, self as *const EeWonThread as *mut c_void, 0, suspended, &mut id)
        };
        self.handle.store(handle, Ordering::SeqCst);
        if suspended != 0 {
            // SAFETY: the new thread's handle
            unsafe {
                plat::PlatThreadSetPriority(handle, platform_priority(priority));
                plat::PlatThreadResume(handle);
            }
        }
    }

    /// `Stop`'s first half: marks the thread stopped; false when it already was (nothing more to do). The C++
    /// then signals it (a virtual call) and waits when asked.
    pub fn mark_stopped(&self) -> bool {
        !self.stopped.swap(true, Ordering::SeqCst)
    }

    /// `Signal`.
    pub fn signal(&self) {
        // SAFETY: the object's event
        unsafe { plat::PlatEventSet(self.signal) };
    }

    /// `mSignalEvent.WaitFor`: true when signalled.
    pub fn wait_signal(&self, milliseconds: u32) -> bool {
        // SAFETY: the object's event
        unsafe { plat::PlatEventWait(self.signal, milliseconds) == 0 }
    }

    /// `InThisThread`.
    pub fn in_this_thread(&self) -> bool {
        // SAFETY: plain platform call
        self.running.load(Ordering::SeqCst)
            && unsafe { plat::PlatCurrentThreadId() } == self.thread_id.load(Ordering::SeqCst)
    }

    /// `WaitForStop`: waits for a running thread to end, unless called from it.
    pub fn wait_for_stop(&self) {
        if self.running.load(Ordering::SeqCst) && !self.in_this_thread() {
            // SAFETY: the thread's handle
            unsafe { plat::PlatThreadWait(self.handle.load(Ordering::SeqCst), PLAT_WAIT_INFINITE) };
        }
    }

    /// `SetPriority`: applied at once to a running thread.
    pub fn set_priority(&self, priority: i32) {
        if self.priority.swap(priority, Ordering::SeqCst) == priority {
            return;
        }
        if self.running.load(Ordering::SeqCst) {
            // SAFETY: the thread's handle
            unsafe { plat::PlatThreadSetPriority(self.handle.load(Ordering::SeqCst), platform_priority(priority)) };
        }
    }
}

impl Drop for EeWonThread {
    fn drop(&mut self) {
        // SAFETY: the object's event and thread handles (the C++ never closed the thread's)
        unsafe {
            plat::PlatEventDestroy(self.signal);
            let h = self.handle.load(Ordering::SeqCst);
            if !h.is_null() {
                plat::PlatThreadClose(h);
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn ee_won_thread_new(body: EeWonThreadBody) -> *mut EeWonThread {
    Box::into_raw(Box::new(EeWonThread::new(body)))
}

/// # Safety
/// `t` came from `ee_won_thread_new`; its thread does not run any more (or never ran).
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_free(t: *mut EeWonThread) {
    if !t.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(t) });
    }
}

/// # Safety
/// `t` is valid and stays at its address while its thread runs.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_start(t: *const EeWonThread) {
    // SAFETY: the caller's contract
    unsafe { (*t).start() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_mark_stopped(t: *const EeWonThread) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).mark_stopped() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_is_stopped(t: *const EeWonThread) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).is_stopped() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_set_stopped(t: *const EeWonThread, stopped: bool) {
    // SAFETY: the caller's contract
    unsafe { (*t).set_stopped(stopped) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_is_running(t: *const EeWonThread) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).is_running() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_set_auto_delete(t: *const EeWonThread, on: bool) {
    // SAFETY: the caller's contract
    unsafe { (*t).set_auto_delete(on) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_signal(t: *const EeWonThread) {
    // SAFETY: the caller's contract
    unsafe { (*t).signal() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_wait_signal(t: *const EeWonThread, milliseconds: u32) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).wait_signal(milliseconds) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_in_this_thread(t: *const EeWonThread) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).in_this_thread() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_wait_for_stop(t: *const EeWonThread) {
    // SAFETY: the caller's contract
    unsafe { (*t).wait_for_stop() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_thread_set_priority(t: *const EeWonThread, priority: i32) {
    // SAFETY: the caller's contract
    unsafe { (*t).set_priority(priority) }
}

// ---- TimerThread

/// An op callback: the op (a C++ `AsyncOp*`) and the context given with the table.
pub type EeWonOpCall = Option<unsafe extern "C" fn(context: *mut c_void, op: *mut c_void)>;

/// The C++ steps the timer thread takes on an op: `due` (it timed out: `Finish(WS_TimedOut)`, then the timer's
/// reference released) and `purged` (the queue let it go: `Kill(true)`, then the reference released).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonTimerHooks {
    pub due: EeWonOpCall,
    pub purged: EeWonOpCall,
    pub context: *mut c_void,
}

struct TimerState {
    queue: TimerQueue,
    /// each queued op's key in `queue` (the original kept the multimap iterator in the op)
    keys: BTreeMap<OpHandle, u64>,
}

/// `TimerThread`: the deadline queue (holding a reference per op, taken by the C++ before `add`), its lock, the
/// signal and the thread that pumps it.
pub struct EeWonTimerThread {
    lock: *mut plat::PlatMutex,
    state: std::cell::UnsafeCell<TimerState>,
    hooks: EeWonTimerHooks,
    thread: EeWonThread,
}

// SAFETY: `state` is touched only under `lock`; the rest are platform handles and atomics
unsafe impl Send for EeWonTimerThread {}
// SAFETY: as Send
unsafe impl Sync for EeWonTimerThread {}

unsafe extern "C" fn timer_thread_run(context: *mut c_void) {
    // SAFETY: the timer thread passes itself
    let t = unsafe { &*(context as *const EeWonTimerThread) };
    t.thread_func();
}

impl EeWonTimerThread {
    /// `TimerThread()`: no timers; the thread not started.
    pub fn new(hooks: EeWonTimerHooks) -> Box<EeWonTimerThread> {
        let mut t = Box::new(EeWonTimerThread {
            // SAFETY: plain platform call
            lock: unsafe { plat::PlatMutexCreate() },
            state: std::cell::UnsafeCell::new(TimerState { queue: TimerQueue::new(), keys: BTreeMap::new() }),
            hooks,
            thread: EeWonThread::new(EeWonThreadBody { run: None, destroy: None, context: null_mut() }),
        });
        let me = &*t as *const EeWonTimerThread as *mut c_void;
        t.thread.body = EeWonThreadBody { run: Some(timer_thread_run), destroy: None, context: me };
        t
    }

    #[allow(clippy::mut_from_ref)]
    fn state(&self, _held: &Locked) -> &mut TimerState {
        // SAFETY: the lock is held (the guard proves it), and the lock is recursive only for the callbacks, which
        // re-enter through `add`/`remove` after the caller's borrow ended
        unsafe { &mut *self.state.get() }
    }

    fn call(&self, hook: EeWonOpCall, op: OpHandle) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract
            unsafe { h(self.hooks.context, op as *mut c_void) };
        }
    }

    /// `AddTimerOp`: the op times out after `timeout` ms; true when it was queued already (its earlier entry is
    /// gone: the C++ releases that entry's reference).
    pub fn add(&self, op: OpHandle, timeout: u32) -> bool {
        let held = Locked::new(self.lock);
        let s = self.state(&held);
        let replaced = match s.keys.remove(&op) {
            Some(k) => s.queue.remove(k).is_some(),
            None => false,
        };
        let key = s.queue.add(ops::tick(), timeout, op);
        s.keys.insert(op, key);
        drop(held);
        self.thread.signal();
        replaced
    }

    /// `RemoveTimerOp`: true when the op was queued (the C++ releases the entry's reference).
    pub fn remove(&self, op: OpHandle) -> bool {
        let held = Locked::new(self.lock);
        let s = self.state(&held);
        match s.keys.remove(&op) {
            Some(k) => s.queue.remove(k).is_some(),
            None => false,
        }
    }

    /// `Pump`: every due op's `due` hook, under the lock, in deadline order.
    pub fn pump(&self) {
        let held = Locked::new(self.lock);
        let due = {
            let s = self.state(&held);
            let due = s.queue.pump(ops::tick());
            for op in &due {
                s.keys.remove(op);
            }
            due
        };
        for op in due {
            self.call(self.hooks.due, op);
        }
    }

    /// `PurgeOps`: every queued op's `purged` hook, under the lock.
    pub fn purge(&self) {
        let held = Locked::new(self.lock);
        let gone = {
            let s = self.state(&held);
            s.keys.clear();
            s.queue.purge()
        };
        for op in gone {
            self.call(self.hooks.purged, op);
        }
    }

    /// `GetWaitTime`.
    pub fn wait_time(&self) -> u32 {
        let held = Locked::new(self.lock);
        self.state(&held).queue.wait_time()
    }

    /// The number of ops queued.
    pub fn len(&self) -> usize {
        let held = Locked::new(self.lock);
        self.state(&held).queue.len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// `ThreadFunc`: waits for the next deadline or a signal and pumps, until stopped.
    fn thread_func(&self) {
        {
            let held = Locked::new(self.lock);
            self.state(&held).queue.set_last_tick(ops::tick());
        }
        loop {
            self.thread.wait_signal(self.wait_time());
            if self.thread.is_stopped() {
                break;
            }
            self.pump();
        }
    }

    pub fn thread(&self) -> &EeWonThread {
        &self.thread
    }

    /// `Stop`: marks the thread stopped, signals it and waits when asked.
    pub fn stop(&self, wait: bool) {
        if !self.thread.mark_stopped() {
            return;
        }
        self.thread.signal();
        if wait {
            self.thread.wait_for_stop();
        }
    }
}

impl Drop for EeWonTimerThread {
    fn drop(&mut self) {
        // `~TimerThread`: every pending op killed first
        self.purge();
        // SAFETY: the object's lock; no thread uses it any more
        unsafe { plat::PlatMutexDestroy(self.lock) };
    }
}

#[no_mangle]
pub extern "C" fn ee_won_timer_thread_new(hooks: EeWonTimerHooks) -> *mut EeWonTimerThread {
    Box::into_raw(EeWonTimerThread::new(hooks))
}

/// # Safety
/// `t` came from `ee_won_timer_thread_new`; its thread does not run any more.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_free(t: *mut EeWonTimerThread) {
    if !t.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(t) });
    }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_start(t: *const EeWonTimerThread) {
    // SAFETY: the caller's contract (the object is boxed: its address is stable)
    unsafe { (*t).thread.start() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_stop(t: *const EeWonTimerThread, wait: bool) {
    // SAFETY: the caller's contract
    unsafe { (*t).stop(wait) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_wait_for_stop(t: *const EeWonTimerThread) {
    // SAFETY: the caller's contract
    unsafe { (*t).thread.wait_for_stop() }
}

/// # Safety
/// `t` is valid; the caller took a reference on `op` for the queue.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_add(t: *const EeWonTimerThread, op: *mut c_void, timeout: u32) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).add(op as OpHandle, timeout) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_remove(t: *const EeWonTimerThread, op: *mut c_void) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).remove(op as OpHandle) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_pump(t: *const EeWonTimerThread) {
    // SAFETY: the caller's contract
    unsafe { (*t).pump() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_purge(t: *const EeWonTimerThread) {
    // SAFETY: the caller's contract
    unsafe { (*t).purge() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_wait_time(t: *const EeWonTimerThread) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*t).wait_time() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_timer_thread_len(t: *const EeWonTimerThread) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*t).len() }
}

// ---- AsyncOpContainer

/// The C++ steps the container takes on an op (`AsyncOp*`): references, its status, its run and its completion.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonContainerHooks {
    pub add_ref: EeWonOpCall,
    pub release: EeWonOpCall,
    /// `mStatus==WS_Pending`
    pub is_pending: Option<unsafe extern "C" fn(context: *mut c_void, op: *mut c_void) -> bool>,
    pub completion_pending: Option<unsafe extern "C" fn(context: *mut c_void, op: *mut c_void) -> bool>,
    pub start_run_async: EeWonOpCall,
    pub complete: EeWonOpCall,
    /// `Kill(true)`
    pub kill: EeWonOpCall,
    pub context: *mut c_void,
}

struct ContainerState {
    queue: OpQueue,
    /// `mCurPumpOp` (0: none), holding a reference
    current: OpHandle,
    purging: bool,
}

/// `AsyncOpContainer`: the queue of ops (a reference each), the op being pumped, the lock and the signal.
pub struct EeWonOpContainer {
    lock: *mut plat::PlatMutex,
    signal: *mut plat::PlatEvent,
    state: std::cell::UnsafeCell<ContainerState>,
    hooks: EeWonContainerHooks,
}

// SAFETY: `state` is touched under `lock` (the purge as the C++ did: by the owner's thread); handles are the
// platform's
unsafe impl Send for EeWonOpContainer {}
// SAFETY: as Send
unsafe impl Sync for EeWonOpContainer {}

impl EeWonOpContainer {
    pub fn new(hooks: EeWonContainerHooks) -> EeWonOpContainer {
        EeWonOpContainer {
            // SAFETY: plain platform calls
            lock: unsafe { plat::PlatMutexCreate() },
            signal: unsafe { plat::PlatEventCreate(0, 0) },
            state: std::cell::UnsafeCell::new(ContainerState { queue: OpQueue::new(), current: 0, purging: false }),
            hooks,
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn state(&self) -> &mut ContainerState {
        // SAFETY: the callers hold the lock, or are the purge (the C++ purged without it); no borrow lives across
        // a hook
        unsafe { &mut *self.state.get() }
    }

    fn call(&self, hook: EeWonOpCall, op: OpHandle) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract
            unsafe { h(self.hooks.context, op as *mut c_void) };
        }
    }

    fn ask(&self, hook: Option<unsafe extern "C" fn(*mut c_void, *mut c_void) -> bool>, op: OpHandle) -> bool {
        // SAFETY: the hooks' contract
        hook.is_some_and(|h| unsafe { h(self.hooks.context, op as *mut c_void) })
    }

    /// `mCurPumpOp = op` (taking over the queue's reference) after releasing the one it held.
    fn set_current(&self, op: OpHandle) {
        let old = std::mem::replace(&mut self.state().current, op);
        if old != 0 {
            self.call(self.hooks.release, old);
        }
    }

    /// `QueueOp`: queues the op (not the one being pumped unless it still runs) with a reference; the first entry
    /// signals.
    pub fn queue(&self, op: OpHandle) {
        let _held = Locked::new(self.lock);
        let current = self.state().current;
        let current_pending = current != 0 && self.ask(self.hooks.is_pending, current);
        let purging = self.state().purging;
        let result = self.state().queue.queue(op, current, current_pending, purging);
        if result == QueueResult::Refused {
            return;
        }
        self.call(self.hooks.add_ref, op);
        if result == QueueResult::QueuedSignal {
            // SAFETY: the object's event
            unsafe { plat::PlatEventSet(self.signal) };
        }
    }

    /// `PurgeOps`: every queued op out, a running one killed, each completed (the last stays the current op, as
    /// the C++ left `mCurPumpOp`).
    pub fn purge(&self) {
        self.state().purging = true;
        while let Some(op) = self.state().queue.pop() {
            self.set_current(op);
            if self.ask(self.hooks.is_pending, op) {
                self.call(self.hooks.kill, op);
            }
            self.call(self.hooks.complete, op);
        }
        self.state().purging = false;
    }

    /// `Pump`: waits up to `wait` ms for work when the plan says so, then runs and completes queued ops until
    /// `max_ops` ran or the time is up.
    pub fn pump(&self, wait: u32, max_ops: i32) {
        let held = Locked::new(self.lock);
        let queued = self.state().queue.len();
        let mut plan = PumpPlan::new(ops::tick(), wait, max_ops, queued);
        if plan.should_wait(queued) {
            // SAFETY: the lock is held once by this call (the pump is not re-entered from an op)
            unsafe { plat::PlatMutexUnlock(self.lock) };
            // SAFETY: the object's event
            unsafe { plat::PlatEventWait(self.signal, wait) };
            // SAFETY: as above
            unsafe { plat::PlatMutexLock(self.lock) };
        }
        while let Some(op) = self.state().queue.pop() {
            self.set_current(op);
            // SAFETY: held above; re-taken below
            unsafe { plat::PlatMutexUnlock(self.lock) };
            if self.ask(self.hooks.is_pending, op) {
                self.call(self.hooks.start_run_async, op);
            }
            if self.ask(self.hooks.completion_pending, op) {
                self.call(self.hooks.complete, op);
            }
            self.set_current(0);
            // SAFETY: as above
            unsafe { plat::PlatMutexLock(self.lock) };
            if !plan.go_on(ops::tick()) {
                break;
            }
        }
        self.set_current(0);
        drop(held);
    }

    /// The number of ops queued.
    pub fn len(&self) -> usize {
        let _held = Locked::new(self.lock);
        self.state().queue.len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// `mCurPumpOp` (null: none).
    pub fn current(&self) -> OpHandle {
        self.state().current
    }
}

impl Drop for EeWonOpContainer {
    fn drop(&mut self) {
        // `~AsyncOpContainer`: every queued op killed and completed, then the current op's reference
        self.purge();
        self.set_current(0);
        // SAFETY: the object's handles; nothing uses them any more
        unsafe {
            plat::PlatEventDestroy(self.signal);
            plat::PlatMutexDestroy(self.lock);
        }
    }
}

#[no_mangle]
pub extern "C" fn ee_won_container_new(hooks: EeWonContainerHooks) -> *mut EeWonOpContainer {
    Box::into_raw(Box::new(EeWonOpContainer::new(hooks)))
}

/// # Safety
/// `c` came from `ee_won_container_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_container_free(c: *mut EeWonOpContainer) {
    if !c.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(c) });
    }
}

/// # Safety
/// `c` is valid; `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_container_queue(c: *const EeWonOpContainer, op: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { (*c).queue(op as OpHandle) }
}

/// # Safety
/// `c` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_container_purge(c: *const EeWonOpContainer) {
    // SAFETY: the caller's contract
    unsafe { (*c).purge() }
}

/// # Safety
/// `c` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_container_pump(c: *const EeWonOpContainer, wait: u32, max_ops: i32) {
    // SAFETY: the caller's contract
    unsafe { (*c).pump(wait, max_ops) }
}

/// # Safety
/// `c` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_container_len(c: *const EeWonOpContainer) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*c).len() }
}

/// # Safety
/// `c` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_container_current(c: *const EeWonOpContainer) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*c).current() as *mut c_void }
}

#[cfg(test)]
#[path = "thread_tests.rs"]
mod tests;
