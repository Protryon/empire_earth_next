//! WONCommon's `AsyncOp` and `AsyncOpTracker` as Rust objects (round 13): the op's run state (`mStatus`, `mRunMode`,
//! `mStartTime`, `mTimeout`, `mCompletionPending`, `mKilled`, `mRunAsyncImmediately`), its lock, its completion and
//! its container live here, and so do the run, finish, kill and completion steps (the transitions are
//! [`OpState`]'s, since round 3). The C++ `AsyncOp` is a shim holding an [`EeWonAsyncOp`]: its virtual `RunHook` and
//! `CleanupHook`, the completion's `Complete`, the references and WONAPICore's queue and timer come back to it as
//! the [`EeWonAsyncOpHooks`]. `AsyncOpTracker` (the ops tracked by id, a reference each) is wholly an
//! [`EeWonOpTracker`].
//!
//! The lock is the platform's recursive mutex (the C++ `CriticalSection` was one), held where the C++ held it, so
//! the hooks run under the same locks, on the same threads and in the same order as the statements they replace
//! (`golden_asyncop.txt`, recorded over the C++ class). The fields are atomics: the original read `mStatus`,
//! `mTimeout` and `mCompletion` without the lock in places (`Run`'s result, `StartRunAsync`, `Complete`), which
//! stays so, without a data race.

use std::cell::UnsafeCell;
use std::ffi::{c_long, c_void};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicPtr, AtomicU32, Ordering::Relaxed};

use ee_plat_sys::thread::imp as plat;

use crate::asyncop::{FinishAction, OpState, RunAction, TimerAction};
use crate::ops::{self, OpTiming};
use crate::status::*;
use crate::thread::EeWonOpContainer;

/// A C++ call on an op (`AsyncOp*`).
pub type EeWonAsyncOpFn = Option<unsafe extern "C" fn(op: *mut c_void)>;
/// A C++ call on a completion (`CompletionBase<AsyncOpPtr>*`).
pub type EeWonAsyncOpCompletionFn = Option<unsafe extern "C" fn(completion: *mut c_void)>;

/// What the op object calls back in the C++. One static table serves every op.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonAsyncOpHooks {
    /// the virtual `RunHook`
    pub run_hook: EeWonAsyncOpFn,
    /// the virtual `CleanupHook`
    pub cleanup_hook: EeWonAsyncOpFn,
    /// `WONAPICore::QueueOp` (an op without a container)
    pub core_queue: EeWonAsyncOpFn,
    /// `WONAPICore::AddTimerOp(op, timeout)`
    pub timer_add: Option<unsafe extern "C" fn(op: *mut c_void, timeout: u32)>,
    /// `WONAPICore::RemoveTimerOp(op)`
    pub timer_remove: EeWonAsyncOpFn,
    /// `completion->AddRef()`, `completion->Release()`
    pub completion_add_ref: EeWonAsyncOpCompletionFn,
    pub completion_release: EeWonAsyncOpCompletionFn,
    /// `completion->Complete(op)` (the C++ makes the `AsyncOpPtr`)
    pub completion_call: Option<unsafe extern "C" fn(completion: *mut c_void, op: *mut c_void)>,
    /// `op->AddRef()`, `op->Release()`, `op->Kill(true)` (the tracker's)
    pub op_add_ref: EeWonAsyncOpFn,
    pub op_release: EeWonAsyncOpFn,
    pub op_kill: EeWonAsyncOpFn,
    /// `object->AddRef()`, `object->Release()` on any `RefCount*` (the op base's objects: sockets, contexts)
    pub ref_add: EeWonAsyncOpFn,
    pub ref_release: EeWonAsyncOpFn,
    /// `new RustCompletion(body)`: a C++ completion (no reference taken yet, as a `new` one) whose `Complete` runs
    /// [`ee_won_completion_run`] and whose deletion frees the body ([`ee_won_completion_free`])
    pub new_completion: Option<unsafe extern "C" fn(body: *mut EeWonCompletion) -> *mut c_void>,
    /// the virtual `AsyncOpWithTracker::CallbackHook(child, param)` of `op`
    pub callback_hook: Option<unsafe extern "C" fn(op: *mut c_void, child: *mut c_void, param: i32) -> bool>,
}

/// A completion's call in Rust (round 24): what the C++ `RustCompletion` runs when completed (with the op, an
/// `AsyncOp*`, the caller holding a reference for the call), and what it frees when deleted. The tracker's
/// completion (`TrackerCompletion`, deleted) and the op base's timer and repeat completions (`HostCompletion`,
/// deleted) are these.
pub struct EeWonCompletion {
    call: Box<dyn Fn(*mut c_void) + Send + Sync>,
}

impl EeWonCompletion {
    /// A C++ completion running `call`, made through `hooks` (null when the table has no maker); like a `new`
    /// object it holds no reference: its first holder takes one.
    pub fn make(hooks: &EeWonAsyncOpHooks, call: impl Fn(*mut c_void) + Send + Sync + 'static) -> *mut c_void {
        Self::make_with(hooks.new_completion, call)
    }

    /// As [`make`](Self::make), through the maker given (the library's core table's, cppcore.rs).
    pub fn make_with(
        maker: Option<unsafe extern "C" fn(body: *mut EeWonCompletion) -> *mut c_void>,
        call: impl Fn(*mut c_void) + Send + Sync + 'static,
    ) -> *mut c_void {
        let Some(h) = maker else {
            return null_mut();
        };
        let body = Box::into_raw(Box::new(EeWonCompletion { call: Box::new(call) }));
        // SAFETY: the hooks' contract: the C++ completion owns `body` from here
        unsafe { h(body) }
    }
}

/// A reference held on a C++ object for as long as a completion's body lives (`SmartPtr` members of the deleted
/// completion classes): taken when made, given back when dropped.
pub(crate) struct HeldRef {
    object: usize,
    release: EeWonAsyncOpFn,
}

impl HeldRef {
    pub(crate) fn new(object: *mut c_void, add: EeWonAsyncOpFn, release: EeWonAsyncOpFn) -> HeldRef {
        if let Some(h) = add {
            // SAFETY: the caller's live object
            unsafe { h(object) };
        }
        HeldRef { object: object as usize, release }
    }
    pub(crate) fn get(&self) -> *mut c_void {
        self.object as *mut c_void
    }
}

impl Drop for HeldRef {
    fn drop(&mut self) {
        if let Some(h) = self.release {
            // SAFETY: the reference taken in `new`
            unsafe { h(self.object as *mut c_void) };
        }
    }
}

/// A recursive platform lock held for a scope; `leave` ends it early (`AutoCrit::Leave`).
struct Held(*mut plat::PlatMutex, bool);

impl Held {
    fn new(m: *mut plat::PlatMutex) -> Held {
        // SAFETY: `m` is the owner's live mutex
        unsafe { plat::PlatMutexLock(m) };
        Held(m, true)
    }

    fn leave(&mut self) {
        if self.1 {
            self.1 = false;
            // SAFETY: locked in `new`
            unsafe { plat::PlatMutexUnlock(self.0) };
        }
    }
}

impl Drop for Held {
    fn drop(&mut self) {
        self.leave();
    }
}

/// `AsyncOp`'s state, lock, completion and container.
pub struct EeWonAsyncOp {
    /// the C++ `AsyncOp*` the hooks get
    op: *mut c_void,
    hooks: &'static EeWonAsyncOpHooks,
    lock: *mut plat::PlatMutex,
    status: AtomicI32,
    run_mode: AtomicI32,
    start: AtomicU32,
    timeout: AtomicU32,
    completion_pending: AtomicBool,
    killed: AtomicBool,
    run_async_immediately: AtomicBool,
    /// `mCompletion`, holding a reference
    completion: AtomicPtr<c_void>,
    /// `mContainer` (the C++ `AsyncOpContainer*`, not owned) and its Rust object
    container: AtomicPtr<c_void>,
    container_obj: AtomicPtr<EeWonOpContainer>,
}

// SAFETY: the fields are atomics or set once; the hooks' contract makes the C++ calls thread-safe as the C++ was
unsafe impl Send for EeWonAsyncOp {}
// SAFETY: as Send
unsafe impl Sync for EeWonAsyncOp {}

impl EeWonAsyncOp {
    /// `AsyncOp::AsyncOp`: idle, an infinite timeout, async, no completion or container.
    pub fn new(op: *mut c_void, hooks: &'static EeWonAsyncOpHooks) -> EeWonAsyncOp {
        let s = OpState::new();
        EeWonAsyncOp {
            op,
            hooks,
            // SAFETY: a plain platform call
            lock: unsafe { plat::PlatMutexCreate() },
            status: AtomicI32::new(s.status),
            run_mode: AtomicI32::new(s.run_mode),
            start: AtomicU32::new(s.start),
            timeout: AtomicU32::new(s.timeout),
            completion_pending: AtomicBool::new(s.completion_pending),
            killed: AtomicBool::new(s.killed),
            run_async_immediately: AtomicBool::new(s.run_async_immediately),
            completion: AtomicPtr::new(null_mut()),
            container: AtomicPtr::new(null_mut()),
            container_obj: AtomicPtr::new(null_mut()),
        }
    }

    fn state(&self) -> OpState {
        OpState {
            status: self.status.load(Relaxed),
            run_mode: self.run_mode.load(Relaxed),
            start: self.start.load(Relaxed),
            timeout: self.timeout.load(Relaxed),
            completion_pending: self.completion_pending.load(Relaxed),
            killed: self.killed.load(Relaxed),
            run_async_immediately: self.run_async_immediately.load(Relaxed),
        }
    }

    fn set_state(&self, s: &OpState) {
        self.status.store(s.status, Relaxed);
        self.run_mode.store(s.run_mode, Relaxed);
        self.start.store(s.start, Relaxed);
        self.timeout.store(s.timeout, Relaxed);
        self.completion_pending.store(s.completion_pending, Relaxed);
        self.killed.store(s.killed, Relaxed);
        self.run_async_immediately.store(s.run_async_immediately, Relaxed);
    }

    /// One transition under the lock; the lock is left before the result is acted on.
    fn locked<R>(&self, f: impl FnOnce(&mut OpState) -> R) -> R {
        let _held = Held::new(self.lock);
        let mut s = self.state();
        let r = f(&mut s);
        self.set_state(&s);
        r
    }

    fn call(&self, hook: EeWonAsyncOpFn) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract: `op` is the live C++ op
            unsafe { h(self.op) };
        }
    }

    pub fn status(&self) -> Status {
        self.status.load(Relaxed) as Status
    }
    pub fn run_mode(&self) -> i32 {
        self.run_mode.load(Relaxed)
    }
    pub fn start_time(&self) -> u32 {
        self.start.load(Relaxed)
    }
    pub fn timeout(&self) -> u32 {
        self.timeout.load(Relaxed)
    }
    pub fn completion_pending(&self) -> bool {
        self.completion_pending.load(Relaxed)
    }
    pub fn killed(&self) -> bool {
        self.killed.load(Relaxed)
    }
    pub fn run_async_immediately(&self) -> bool {
        self.run_async_immediately.load(Relaxed)
    }
    pub fn timing(&self) -> OpTiming {
        OpTiming { run_mode: self.run_mode(), start: self.start_time(), timeout: self.timeout() }
    }
    pub fn completion(&self) -> *mut c_void {
        self.completion.load(Relaxed)
    }
    pub fn container(&self) -> *mut c_void {
        self.container.load(Relaxed)
    }
    /// The C++ `AsyncOp*` the hooks get.
    pub fn cpp(&self) -> *mut c_void {
        self.op
    }
    pub fn hooks(&self) -> &'static EeWonAsyncOpHooks {
        self.hooks
    }

    /// `AsyncOpWithTracker::Track(child, param)`: the child's completion becomes the tracker's (holding a reference
    /// to this op; when the child completes while this op is pending and the tracker still has it, the child leaves
    /// the tracker and this op's `CallbackHook(child, param)` runs), then the tracker takes the child; returns it.
    ///
    /// # Safety
    /// `tracker` is this op's tracker; `child` is a live op whose object is `child_obj`.
    pub unsafe fn track(
        &self,
        tracker: *const EeWonOpTracker,
        child: *mut c_void,
        child_obj: &EeWonAsyncOp,
        param: i32,
    ) -> *mut c_void {
        let hooks = self.hooks;
        let parent = HeldRef::new(self.op, hooks.op_add_ref, hooks.op_release);
        let (me, tracker_at) = (self as *const EeWonAsyncOp as usize, tracker as usize);
        let c = EeWonCompletion::make(hooks, move |op| {
            // SAFETY: `parent` holds this op (and so its object and tracker) alive while the completion lives
            let (p, t) = unsafe { (&*(me as *const EeWonAsyncOp), &*(tracker_at as *const EeWonOpTracker)) };
            if p.status() == WS_PENDING && t.remove(op) {
                if let Some(h) = hooks.callback_hook {
                    // SAFETY: the hooks' contract: the live op and its child
                    unsafe { h(parent.get(), op, param) };
                }
            }
        });
        // SAFETY: a new completion (or null), the caller's live child
        unsafe { child_obj.set_completion(c) };
        // SAFETY: the caller's contract
        unsafe { (*tracker).add(child, param as u32) }
    }

    /// `mCompletion = completion`: the new one's reference first, then the old one's goes (a `SmartPtr`
    /// assignment).
    ///
    /// # Safety
    /// `completion` is null or a live completion.
    pub unsafe fn set_completion(&self, completion: *mut c_void) {
        if !completion.is_null() {
            if let Some(h) = self.hooks.completion_add_ref {
                // SAFETY: the caller's live completion
                unsafe { h(completion) };
            }
        }
        let old = self.completion.swap(completion, Relaxed);
        if !old.is_null() {
            if let Some(h) = self.hooks.completion_release {
                // SAFETY: the reference this object held
                unsafe { h(old) };
            }
        }
    }

    /// `mContainer = container` (the C++ object and its Rust container; both null for none).
    pub fn set_container(&self, container: *mut c_void, obj: *mut EeWonOpContainer) {
        self.container.store(container, Relaxed);
        self.container_obj.store(obj, Relaxed);
    }

    /// `SetTimeout`: unless running.
    pub fn set_timeout(&self, timeout: u32) {
        // (the original took no lock here)
        let mut s = self.state();
        s.set_timeout(timeout);
        self.set_state(&s);
    }

    /// `SetMode`: unless running.
    pub fn set_mode(&self, mode: i32) {
        let mut s = self.state();
        s.set_mode(mode);
        self.set_state(&s);
    }

    /// `SetRunAsyncImmediately`.
    pub fn set_run_async_immediately(&self, on: bool) {
        self.run_async_immediately.store(on, Relaxed);
    }

    /// `QueueInOpContainer`: in its container, or WONAPICore's queue when it has none.
    pub fn queue_in_container(&self) {
        let c = self.container_obj.load(Relaxed);
        if !self.container.load(Relaxed).is_null() && !c.is_null() {
            // SAFETY: the container outlives the ops queued in it (the C++ contract)
            unsafe { (*c).queue(self.op as usize) };
        } else {
            self.call(self.hooks.core_queue);
        }
    }

    fn act(&self, action: FinishAction) {
        if action == FinishAction::Queue {
            self.queue_in_container();
        }
    }

    /// `Kill`: marks the op killed and, when asked, finishes it with `WS_Killed`.
    pub fn kill(&self, finish: bool) {
        let a = self.locked(|s| s.kill(finish));
        self.act(a);
    }

    /// `Finish`: a running op finishes; an async one is queued for its completion.
    pub fn finish(&self, status: Status) {
        let a = self.locked(|s| s.finish(status));
        self.act(a);
    }

    /// `ForceFinish`: finishes even a finished op, unless its completion is pending.
    pub fn force_finish(&self, status: Status) {
        let a = self.locked(|s| s.force_finish(status));
        self.act(a);
    }

    /// `Complete`: a pending completion is delivered: the timer entry goes, the cleanup hook runs, then the
    /// completion (a reference held across its call).
    pub fn complete(&self) {
        let mut held = Held::new(self.lock);
        let mut s = self.state();
        let deliver = s.complete();
        self.set_state(&s);
        if !deliver {
            return;
        }
        held.leave();
        self.call(self.hooks.timer_remove);
        self.call(self.hooks.cleanup_hook);
        let c = self.completion.load(Relaxed);
        if c.is_null() {
            return;
        }
        // SAFETY: the hooks' contract; the reference keeps the completion alive across its call
        unsafe {
            if let Some(h) = self.hooks.completion_add_ref {
                h(c);
            }
            if let Some(h) = self.hooks.completion_call {
                h(c, self.op);
            }
            if let Some(h) = self.hooks.completion_release {
                h(c);
            }
        }
    }

    /// `StartRunAsync`: the run hook, then the timeout armed, or a zero timeout times out now.
    pub fn start_run_async(&self) {
        self.call(self.hooks.run_hook);
        match self.state().after_run_hook() {
            TimerAction::Arm => {
                if let Some(h) = self.hooks.timer_add {
                    // SAFETY: the hooks' contract
                    unsafe { h(self.op, self.timeout()) };
                }
            }
            TimerAction::TimeOutNow => self.finish(WS_TIMED_OUT),
            TimerAction::None => {}
        }
    }

    /// `Run(mode, timeout)`: the status after the start (`WS_Pending` / `WS_AwaitingCompletion` when it could not
    /// start; a blocking run's own).
    pub fn run(&self, mode: i32, timeout: u32) -> Status {
        let a = self.locked(|s| s.run(mode, timeout, ops::tick()));
        match a {
            RunAction::Pending => return WS_PENDING,
            RunAction::Awaiting => return WS_AWAITING_COMPLETION,
            RunAction::StartAsync => self.start_run_async(),
            RunAction::Queue => self.queue_in_container(),
            RunAction::Block => {
                self.call(self.hooks.run_hook);
                self.complete();
            }
        }
        self.status()
    }

    /// `RunAsTimer`: an idle op becomes a timer that fires after `timeout`.
    pub fn run_as_timer(&self, timeout: u32) {
        let arm = self.locked(|s| s.run_as_timer(timeout, ops::tick()));
        if arm {
            if let Some(h) = self.hooks.timer_add {
                // SAFETY: the hooks' contract
                unsafe { h(self.op, timeout) };
            }
        }
    }

    /// `TimeLeft`.
    pub fn time_left(&self) -> u32 {
        self.timing().time_left(ops::tick())
    }

    /// `RealTimeLeft`.
    pub fn real_time_left(&self) -> u32 {
        ops::real_time_left(self.start_time(), self.timeout(), ops::tick())
    }
}

impl Drop for EeWonAsyncOp {
    fn drop(&mut self) {
        // `~AsyncOp`: the completion's reference, then the lock
        // SAFETY: null
        unsafe { self.set_completion(null_mut()) };
        // SAFETY: the object's mutex; nothing holds it any more
        unsafe { plat::PlatMutexDestroy(self.lock) };
    }
}

// ---- AsyncOpTracker

/// `AsyncOpTracker`: the ops tracked by id (a reference each), in id order; among equal ids the latest first (the
/// original's hinted multimap insert at the id's first entry); an op tracked once (every tracker was made with
/// duplicate ids allowed, duplicate ops not). The lock is the platform's recursive mutex.
pub struct EeWonOpTracker {
    hooks: &'static EeWonAsyncOpHooks,
    lock: *mut plat::PlatMutex,
    entries: UnsafeCell<Vec<(u32, *mut c_void)>>,
}

// SAFETY: `entries` is touched under `lock` only; no borrow lives across a hook
unsafe impl Send for EeWonOpTracker {}
// SAFETY: as Send
unsafe impl Sync for EeWonOpTracker {}

impl EeWonOpTracker {
    pub fn new(hooks: &'static EeWonAsyncOpHooks) -> EeWonOpTracker {
        EeWonOpTracker {
            hooks,
            // SAFETY: a plain platform call
            lock: unsafe { plat::PlatMutexCreate() },
            entries: UnsafeCell::new(Vec::new()),
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn entries(&self) -> &mut Vec<(u32, *mut c_void)> {
        // SAFETY: the callers hold the lock and drop the borrow before any hook
        unsafe { &mut *self.entries.get() }
    }

    fn call(&self, hook: EeWonAsyncOpFn, op: *mut c_void) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract: `op` is a live op
            unsafe { h(op) };
        }
    }

    /// `AddOp`: tracks `op` under `id` unless it is tracked already; returns `op`.
    pub fn add(&self, op: *mut c_void, id: u32) -> *mut c_void {
        let _held = Held::new(self.lock);
        let e = self.entries();
        if e.iter().any(|&(_, o)| o == op) {
            return op;
        }
        let at = e.partition_point(|&(i, _)| i < id);
        e.insert(at, (id, op));
        self.call(self.hooks.op_add_ref, op);
        op
    }

    /// `RemoveOp`: stops tracking `op`; whether it was tracked.
    pub fn remove(&self, op: *mut c_void) -> bool {
        let _held = Held::new(self.lock);
        let e = self.entries();
        let Some(at) = e.iter().position(|&(_, o)| o == op) else {
            return false;
        };
        e.remove(at);
        self.call(self.hooks.op_release, op);
        true
    }

    /// `KillAll`: each op in turn untracked and killed with its completion (a reference held across the kill).
    pub fn kill_all(&self) {
        let _held = Held::new(self.lock);
        loop {
            let op = {
                let e = self.entries();
                if e.is_empty() {
                    break;
                }
                e.remove(0).1
            };
            self.call(self.hooks.op_kill, op);
            self.call(self.hooks.op_release, op);
        }
    }

    /// The number of ops tracked.
    pub fn len(&self) -> usize {
        let _held = Held::new(self.lock);
        self.entries().len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// The op tracked at `index` in id order (null past the end); its id in `id`.
    pub fn at(&self, index: usize) -> (*mut c_void, u32) {
        let _held = Held::new(self.lock);
        self.entries().get(index).map_or((null_mut(), 0), |&(i, o)| (o, i))
    }
}

impl Drop for EeWonOpTracker {
    fn drop(&mut self) {
        // `~AsyncOpTrackerMaps`: every entry's reference, in id order
        let entries = std::mem::take(self.entries.get_mut());
        for (_, op) in entries {
            self.call(self.hooks.op_release, op);
        }
        // SAFETY: the object's mutex; nothing holds it any more
        unsafe { plat::PlatMutexDestroy(self.lock) };
    }
}

// ---- the C ABI

/// # Safety
/// `hooks` lives for the program; `op` is the C++ op being constructed.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_new(op: *mut c_void, hooks: *const EeWonAsyncOpHooks) -> *mut EeWonAsyncOp {
    // SAFETY: the caller's contract
    Box::into_raw(Box::new(EeWonAsyncOp::new(op, unsafe { &*hooks })))
}

/// # Safety
/// `o` came from `ee_won_asyncop_new` (or is null).
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_free(o: *mut EeWonAsyncOp) {
    if !o.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(o) });
    }
}

/// `AsyncOp`'s run mode.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_run_mode(o: *const EeWonAsyncOp) -> i32 {
    // SAFETY: the caller's contract
    unsafe { (*o).run_mode() }
}

/// `AsyncOp`'s start time.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_start_time(o: *const EeWonAsyncOp) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*o).start_time() }
}

/// `AsyncOp`'s timeout.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_timeout(o: *const EeWonAsyncOp) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*o).timeout() }
}

/// `AsyncOp`'s completion pending.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_completion_pending(o: *const EeWonAsyncOp) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*o).completion_pending() }
}

/// `AsyncOp`'s killed.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_killed(o: *const EeWonAsyncOp) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*o).killed() }
}

/// `AsyncOp`'s run async immediately.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_run_async_immediately(o: *const EeWonAsyncOp) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*o).run_async_immediately() }
}

/// `AsyncOp`'s timing.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_timing(o: *const EeWonAsyncOp) -> OpTiming {
    // SAFETY: the caller's contract
    unsafe { (*o).timing() }
}

/// `AsyncOp`'s completion.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_completion(o: *const EeWonAsyncOp) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*o).completion() }
}

/// `AsyncOp`'s container.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_container(o: *const EeWonAsyncOp) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*o).container() }
}

/// `AsyncOp`'s time left.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_time_left(o: *const EeWonAsyncOp) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*o).time_left() }
}

/// `AsyncOp`'s real time left.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_real_time_left(o: *const EeWonAsyncOp) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*o).real_time_left() }
}

/// `AsyncOp`'s set completion.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_set_completion(o: *const EeWonAsyncOp, completion: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { (*o).set_completion(completion) }
}

/// `AsyncOp`'s set container.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_set_container(
    o: *const EeWonAsyncOp,
    container: *mut c_void,
    obj: *mut EeWonOpContainer,
) {
    // SAFETY: the caller's contract
    unsafe { (*o).set_container(container, obj) }
}

/// `AsyncOp`'s set timeout.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_set_timeout(o: *const EeWonAsyncOp, timeout: u32) {
    // SAFETY: the caller's contract
    unsafe { (*o).set_timeout(timeout) }
}

/// `AsyncOp`'s set mode.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_set_mode(o: *const EeWonAsyncOp, mode: i32) {
    // SAFETY: the caller's contract
    unsafe { (*o).set_mode(mode) }
}

/// `AsyncOp`'s set run async immediately.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_set_run_async_immediately(o: *const EeWonAsyncOp, on: bool) {
    // SAFETY: the caller's contract
    unsafe { (*o).set_run_async_immediately(on) }
}

/// `AsyncOp`'s queue in container.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_queue_in_container(o: *const EeWonAsyncOp) {
    // SAFETY: the caller's contract
    unsafe { (*o).queue_in_container() }
}

/// `AsyncOp`'s kill.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_kill(o: *const EeWonAsyncOp, finish: bool) {
    // SAFETY: the caller's contract
    unsafe { (*o).kill(finish) }
}

/// `AsyncOp`'s complete.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_complete(o: *const EeWonAsyncOp) {
    // SAFETY: the caller's contract
    unsafe { (*o).complete() }
}

/// `AsyncOp`'s start run async.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_start_run_async(o: *const EeWonAsyncOp) {
    // SAFETY: the caller's contract
    unsafe { (*o).start_run_async() }
}

/// `AsyncOp`'s run as timer.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_run_as_timer(o: *const EeWonAsyncOp, timeout: u32) {
    // SAFETY: the caller's contract
    unsafe { (*o).run_as_timer(timeout) }
}

/// `AsyncOp::GetStatus` (a `WONStatus`, the platform's `long`).
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_status(o: *const EeWonAsyncOp) -> c_long {
    // SAFETY: the caller's contract
    unsafe { (*o).status() as c_long }
}

/// `AsyncOp::Finish`.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_finish(o: *const EeWonAsyncOp, status: c_long) {
    // SAFETY: the caller's contract
    unsafe { (*o).finish(status as Status) }
}

/// `AsyncOp::ForceFinish`.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_force_finish(o: *const EeWonAsyncOp, status: c_long) {
    // SAFETY: the caller's contract
    unsafe { (*o).force_finish(status as Status) }
}

/// `AsyncOp::Run`: the status after the start.
///
/// # Safety
/// `o` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_run(o: *const EeWonAsyncOp, mode: i32, timeout: u32) -> c_long {
    // SAFETY: the caller's contract
    unsafe { (*o).run(mode, timeout) as c_long }
}

/// # Safety
/// `hooks` lives for the program.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_tracker_new(hooks: *const EeWonAsyncOpHooks) -> *mut EeWonOpTracker {
    // SAFETY: the caller's contract
    Box::into_raw(Box::new(EeWonOpTracker::new(unsafe { &*hooks })))
}

/// # Safety
/// `t` came from `ee_won_op_tracker_new` (or is null).
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_tracker_free(t: *mut EeWonOpTracker) {
    if !t.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(t) });
    }
}

/// # Safety
/// `t` is valid; `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_tracker_add(t: *const EeWonOpTracker, op: *mut c_void, id: u32) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*t).add(op, id) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_tracker_remove(t: *const EeWonOpTracker, op: *mut c_void) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*t).remove(op) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_tracker_kill_all(t: *const EeWonOpTracker) {
    // SAFETY: the caller's contract
    unsafe { (*t).kill_all() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_tracker_len(t: *const EeWonOpTracker) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*t).len() }
}

/// The op at `index` in id order, null past the end.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_op_tracker_at(t: *const EeWonOpTracker, index: usize) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*t).at(index).0 }
}

/// `AsyncOpWithTracker::Track(child, param)` (see [`EeWonAsyncOp::track`]); returns the child.
///
/// # Safety
/// `o` is the op's object, `tracker` its tracker; `child` is a live op whose object is `child_obj`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_asyncop_track(
    o: *const EeWonAsyncOp,
    tracker: *const EeWonOpTracker,
    child: *mut c_void,
    child_obj: *const EeWonAsyncOp,
    param: i32,
) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*o).track(tracker, child, &*child_obj, param) }
}

/// A `RustCompletion`'s `Complete(op)`: its Rust call.
///
/// # Safety
/// `c` is a live body (a `RustCompletion`'s); `op` the op it completes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_completion_run(c: *const EeWonCompletion, op: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { ((*c).call)(op) }
}

/// A `RustCompletion`'s deletion: its body (and the references the body holds) freed.
///
/// # Safety
/// `c` came from [`EeWonCompletion::make`] (or is null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_completion_free(c: *mut EeWonCompletion) {
    if !c.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(c) });
    }
}

#[cfg(test)]
#[path = "opobj_tests.rs"]
mod tests;
