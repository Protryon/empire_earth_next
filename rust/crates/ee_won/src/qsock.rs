//! WONSocket's `QueueSocket` as a Rust object (round 14): the queued ops (a reference each, in order), the op
//! running from the queue and its own completion, the repeating op, the close timer, the default, repeat and close
//! completions (a reference each), the queued-close flag, and every step of `QueueSocket.cpp`: the queue's runs, the
//! repeat op's reruns, `QueueOpFast`, `QueueClose`, the close timer, the finished op's callback (its decisions are
//! [`asyncop::queue_socket_callback`]'s since round 2) and the open, close and kill hooks. The C++ `QueueSocket` is
//! a shim holding an [`EeWonQueueSocket`]; what is C++'s (the ops' and completions' references, calls and classes:
//! `ParamCompletion`, `QueueOpFastCompletion`, `QueueCloseOp`, `CloseOp`, a timer `AsyncOp`) comes back through the
//! [`EeWonQueueSocketHooks`].
//!
//! The lock is the socket's (`mDataCrit`), held where the C++ held it; a reference is dropped under it where a C++
//! `SmartPtr` assignment dropped it. No borrow of the state lives across a hook. The behaviour is
//! `golden_sockobj.txt`'s QueueSocket lines, recorded over the C++ class.

use std::cell::UnsafeCell;
use std::collections::VecDeque;
use std::ffi::{c_long, c_void};
use std::ptr::null_mut;

use ee_plat_sys::thread::imp as plat;

use crate::asyncop::{self, QueueCompletion, QueueNext, QueueOpKind};
use crate::status::*;

/// A C++ call on one object (an op, a completion or the socket).
pub type EeWonQueueFn = Option<unsafe extern "C" fn(object: *mut c_void)>;
/// A C++ call making a new object (its reference not yet taken).
pub type EeWonQueueNewFn = Option<unsafe extern "C" fn(socket: *mut c_void) -> *mut c_void>;

/// What the queue object calls back in the C++. One static table serves every queue socket.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonQueueSocketHooks {
    /// `completion->AddRef()`, `->Release()`, `->Complete(op)`
    pub completion_add_ref: EeWonQueueFn,
    pub completion_release: EeWonQueueFn,
    pub completion_call: Option<unsafe extern "C" fn(completion: *mut c_void, op: *mut c_void)>,
    /// `op->AddRef()`, `->Release()` (any `AsyncOp*`)
    pub op_add_ref: EeWonQueueFn,
    pub op_release: EeWonQueueFn,
    /// `op->GetStatus()`, `->IsKilled()`, `->GetTimeout()`, `->GetCompletion()`
    pub op_status: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    pub op_killed: Option<unsafe extern "C" fn(op: *mut c_void) -> bool>,
    pub op_timeout: Option<unsafe extern "C" fn(op: *mut c_void) -> u32>,
    pub op_completion: Option<unsafe extern "C" fn(op: *mut c_void) -> *mut c_void>,
    /// `op->SetCompletion(c)`, `->SetTimeout(t)`, `->RunAsync(t)`, `->Kill(true)`, `->ForceFinish(s)`,
    /// `->SetRunAsyncImmediately(false)`
    pub op_set_completion: Option<unsafe extern "C" fn(op: *mut c_void, completion: *mut c_void)>,
    pub op_set_timeout: Option<unsafe extern "C" fn(op: *mut c_void, timeout: u32)>,
    pub op_run_async: Option<unsafe extern "C" fn(op: *mut c_void, timeout: c_long)>,
    pub op_kill: EeWonQueueFn,
    pub op_force_finish: Option<unsafe extern "C" fn(op: *mut c_void, status: c_long)>,
    /// a socket op's `SetSocket(socket)` and the virtual `Duplicate()` (a new op, or null)
    pub op_set_socket: Option<unsafe extern "C" fn(op: *mut c_void, socket: *mut c_void)>,
    pub op_duplicate: Option<unsafe extern "C" fn(op: *mut c_void) -> *mut c_void>,
    /// `dynamic_cast<QueueOpFastCompletion*>(completion)`'s original completion, or null
    pub fast_original: Option<unsafe extern "C" fn(completion: *mut c_void) -> *mut c_void>,
    /// new objects: `ParamCompletion(StaticOpCallback, socket)`, `QueueOpFastCompletion(StaticOpCallback,
    /// socket, original)`, a close timer (`AsyncOp` with `ParamCompletion(StaticCloseTimerCallback, socket)`),
    /// `QueueCloseOp`, `CloseOp(socket)`
    pub new_op_completion: EeWonQueueNewFn,
    pub new_fast_completion: Option<unsafe extern "C" fn(socket: *mut c_void, original: *mut c_void) -> *mut c_void>,
    pub new_close_timer: EeWonQueueNewFn,
    pub new_queue_close_op: EeWonQueueNewFn,
    pub new_close_op: EeWonQueueNewFn,
    /// the socket's `Close()`, `IsValid()`, `GetType() == UDP`
    pub socket_close: EeWonQueueFn,
    pub socket_valid: Option<unsafe extern "C" fn(socket: *mut c_void) -> bool>,
    pub socket_datagram: Option<unsafe extern "C" fn(socket: *mut c_void) -> bool>,
}

struct Held(*mut plat::PlatMutex);

impl Held {
    fn new(m: *mut plat::PlatMutex) -> Held {
        // SAFETY: the socket's live mutex
        unsafe { plat::PlatMutexLock(m) };
        Held(m)
    }
}

impl Drop for Held {
    fn drop(&mut self) {
        // SAFETY: locked in `new`
        unsafe { plat::PlatMutexUnlock(self.0) };
    }
}

#[derive(Default)]
struct State {
    default_completion: usize,
    cur_completion: usize,
    repeat_completion: usize,
    close_completion: usize,
    repeat_op: usize,
    queue_op: usize,
    close_timer: usize,
    queue: VecDeque<usize>,
    close_queued: bool,
}

/// Which of the state's references a slot is.
#[derive(Clone, Copy)]
enum Slot {
    Default,
    Cur,
    Repeat,
    Close,
    RepeatOp,
    QueueOp,
    CloseTimer,
}

impl Slot {
    fn is_op(self) -> bool {
        matches!(self, Slot::RepeatOp | Slot::QueueOp | Slot::CloseTimer)
    }
}

/// `QueueSocket`'s state and steps.
pub struct EeWonQueueSocket {
    /// the C++ `QueueSocket*` the hooks get
    socket: *mut c_void,
    hooks: &'static EeWonQueueSocketHooks,
    /// the socket's lock (`mDataCrit`'s mutex)
    lock: *mut plat::PlatMutex,
    state: UnsafeCell<State>,
}

// SAFETY: the state is touched under `lock` only; no borrow lives across a hook
unsafe impl Send for EeWonQueueSocket {}
// SAFETY: as Send
unsafe impl Sync for EeWonQueueSocket {}

/// A reference this code holds for a scope (a C++ local `SmartPtr`): released on drop.
struct Ref<'a> {
    q: &'a EeWonQueueSocket,
    ptr: usize,
    op: bool,
}

impl Ref<'_> {
    fn get(&self) -> *mut c_void {
        self.ptr as *mut c_void
    }
}

impl Drop for Ref<'_> {
    fn drop(&mut self) {
        self.q.release(self.ptr, self.op);
    }
}

impl EeWonQueueSocket {
    /// `QueueSocket(type)`: nothing queued, no close queued.
    pub fn new(socket: *mut c_void, hooks: &'static EeWonQueueSocketHooks, lock: *mut plat::PlatMutex) -> Self {
        EeWonQueueSocket { socket, hooks, lock, state: UnsafeCell::new(State::default()) }
    }

    #[allow(clippy::mut_from_ref)]
    fn st(&self) -> &mut State {
        // SAFETY: the callers hold the lock and drop the borrow before any hook
        unsafe { &mut *self.state.get() }
    }

    fn slot(&self, s: Slot) -> &mut usize {
        let st = self.st();
        match s {
            Slot::Default => &mut st.default_completion,
            Slot::Cur => &mut st.cur_completion,
            Slot::Repeat => &mut st.repeat_completion,
            Slot::Close => &mut st.close_completion,
            Slot::RepeatOp => &mut st.repeat_op,
            Slot::QueueOp => &mut st.queue_op,
            Slot::CloseTimer => &mut st.close_timer,
        }
    }

    fn call(&self, hook: EeWonQueueFn, object: usize) {
        if object != 0 {
            if let Some(h) = hook {
                // SAFETY: the hooks' contract: a live C++ object
                unsafe { h(object as *mut c_void) };
            }
        }
    }

    fn add_ref(&self, object: usize, op: bool) {
        self.call(if op { self.hooks.op_add_ref } else { self.hooks.completion_add_ref }, object);
    }

    fn release(&self, object: usize, op: bool) {
        self.call(if op { self.hooks.op_release } else { self.hooks.completion_release }, object);
    }

    /// A `SmartPtr` assignment to a slot: the new reference first, then the old one goes.
    fn assign(&self, s: Slot, object: usize) {
        self.add_ref(object, s.is_op());
        let old = std::mem::replace(self.slot(s), object);
        self.release(old, s.is_op());
    }

    /// A local copy of a slot (`SmartPtr<...> a = mX;`).
    fn copy(&self, s: Slot) -> Ref<'_> {
        let ptr = *self.slot(s);
        self.add_ref(ptr, s.is_op());
        Ref { q: self, ptr, op: s.is_op() }
    }

    /// A new C++ object held for the scope (`SmartPtr<...> a = new ...;`).
    fn hold(&self, object: *mut c_void, op: bool) -> Ref<'_> {
        self.add_ref(object as usize, op);
        Ref { q: self, ptr: object as usize, op }
    }

    fn op_status(&self, op: usize) -> Status {
        // SAFETY: the hooks' contract
        self.hooks.op_status.map_or(WS_NONE, |h| unsafe { h(op as *mut c_void) } as Status)
    }

    fn op_set_completion(&self, op: usize, completion: usize) {
        if let Some(h) = self.hooks.op_set_completion {
            // SAFETY: the hooks' contract (the op takes its own reference)
            unsafe { h(op as *mut c_void, completion as *mut c_void) };
        }
    }

    fn op_completion(&self, op: usize) -> usize {
        // SAFETY: the hooks' contract
        self.hooks.op_completion.map_or(0, |h| unsafe { h(op as *mut c_void) } as usize)
    }

    fn op_run_async(&self, op: usize, timeout: c_long) {
        if let Some(h) = self.hooks.op_run_async {
            // SAFETY: the hooks' contract
            unsafe { h(op as *mut c_void, timeout) };
        }
    }

    fn op_set_socket(&self, op: usize) {
        if let Some(h) = self.hooks.op_set_socket {
            // SAFETY: the hooks' contract
            unsafe { h(op as *mut c_void, self.socket) };
        }
    }

    fn new_object(&self, hook: EeWonQueueNewFn) -> *mut c_void {
        // SAFETY: the hooks' contract
        hook.map_or(null_mut(), |h| unsafe { h(self.socket) })
    }

    /// A new `ParamCompletion(StaticOpCallback, socket)` set on `op` (which takes its reference).
    fn route_to_socket(&self, op: usize) {
        let c = self.hold(self.new_object(self.hooks.new_op_completion), false);
        self.op_set_completion(op, c.ptr);
    }

    /// `SetRepeatCompletion`, `SetCloseCompletion`, `SetQueueCompletion`.
    pub fn set_repeat_completion(&self, c: *mut c_void) {
        let _held = Held::new(self.lock);
        self.assign(Slot::Repeat, c as usize);
    }
    pub fn set_close_completion(&self, c: *mut c_void) {
        let _held = Held::new(self.lock);
        self.assign(Slot::Close, c as usize);
    }
    pub fn set_queue_completion(&self, c: *mut c_void) {
        let _held = Held::new(self.lock);
        self.assign(Slot::Default, c as usize);
    }

    /// `StartCloseTimer`: the timer restarted; after `timeout` the socket force-closes (0 just cancels it).
    pub fn start_close_timer(&self, timeout: c_long) {
        let _held = Held::new(self.lock);
        self.cancel_close_timer();
        if timeout != 0 {
            let t = self.new_object(self.hooks.new_close_timer);
            self.assign(Slot::CloseTimer, t as usize);
            self.op_run_async(t as usize, timeout);
        }
    }

    /// `CancelCloseTimer`: a running timer is killed (and forgotten first).
    pub fn cancel_close_timer(&self) {
        if *self.slot(Slot::CloseTimer) != 0 {
            let t = self.copy(Slot::CloseTimer);
            self.assign(Slot::CloseTimer, 0);
            self.call(self.hooks.op_kill, t.ptr);
        }
    }

    /// `CloseTimerCallback`: the current timer's expiry closes the socket and reports a killed close.
    pub fn close_timer_callback(&self, timer: *mut c_void) {
        let mut held = Some(Held::new(self.lock));
        if timer as usize != *self.slot(Slot::CloseTimer) || self.op_status(timer as usize) != WS_TIMED_OUT {
            return;
        }
        self.assign(Slot::CloseTimer, 0);
        let close = self.copy(Slot::Close);
        held.take();
        self.call(self.hooks.socket_close, self.socket as usize);
        self.report_close(&close, WS_KILLED);
    }

    /// A new `CloseOp(socket)` with the close completion, force-finished with `status`.
    fn report_close(&self, close: &Ref<'_>, status: Status) {
        if close.ptr == 0 {
            return;
        }
        let op = self.hold(self.new_object(self.hooks.new_close_op), true);
        self.op_set_completion(op.ptr, close.ptr);
        if let Some(h) = self.hooks.op_force_finish {
            // SAFETY: the hooks' contract
            unsafe { h(op.get(), status as c_long) };
        }
    }

    /// `OpCallback`: a finished op goes to its completion, the next queued op starts or the repeat op reruns, and
    /// the socket closes (reporting to the close completion) on a failure or a finished queued close.
    pub fn op_callback(&self, op: *mut c_void) {
        let op = op as usize;
        let mut held = Some(Held::new(self.lock));
        let kind = if op == *self.slot(Slot::QueueOp) {
            QueueOpKind::Queue
        } else if op == *self.slot(Slot::RepeatOp) {
            QueueOpKind::Repeat
        } else {
            QueueOpKind::Other
        };
        // SAFETY: the hooks' contract
        let killed = self.hooks.op_killed.is_some_and(|h| unsafe { h(op as *mut c_void) });
        // SAFETY: the hooks' contract
        let valid = self.hooks.socket_valid.is_some_and(|h| unsafe { h(self.socket) });
        // SAFETY: the hooks' contract
        let datagram = self.hooks.socket_datagram.is_some_and(|h| unsafe { h(self.socket) });
        let st = self.st();
        let d = asyncop::queue_socket_callback(
            kind,
            self.op_status(op),
            killed,
            st.close_queued,
            st.queue.is_empty(),
            valid,
            datagram,
        );
        let completion = match d.completion {
            QueueCompletion::None => Ref { q: self, ptr: 0, op: false },
            QueueCompletion::Current => self.copy(Slot::Cur),
            QueueCompletion::Repeat => self.copy(Slot::Repeat),
            QueueCompletion::Original => {
                let c = self.op_completion(op);
                // SAFETY: the hooks' contract
                let o = self.hooks.fast_original.map_or(null_mut(), |h| unsafe { h(c as *mut c_void) });
                self.hold(o, false)
            }
        };
        if d.clear_current {
            self.assign(Slot::Cur, 0);
            self.assign(Slot::QueueOp, 0);
        }
        match d.next {
            QueueNext::RunQueue => self.run_queue_op(),
            QueueNext::RerunRepeat => {
                let r = *self.slot(Slot::RepeatOp);
                // SAFETY: the hooks' contract
                let dup = self.hooks.op_duplicate.map_or(null_mut(), |h| unsafe { h(r as *mut c_void) });
                self.assign(Slot::RepeatOp, dup as usize);
                self.run_repeat_op();
            }
            QueueNext::DropRepeat => self.assign(Slot::RepeatOp, 0),
            QueueNext::None => {}
        }
        let close = if d.close { self.copy(Slot::Close) } else { Ref { q: self, ptr: 0, op: false } };
        held.take();
        if completion.ptr != 0 {
            if let Some(h) = self.hooks.completion_call {
                // SAFETY: the hooks' contract; the local reference keeps the completion alive
                unsafe { h(completion.get(), op as *mut c_void) };
            }
        }
        if d.close {
            self.call(self.hooks.socket_close, self.socket as usize);
            let s = self.op_status(op);
            self.report_close(&close, s);
        }
    }

    /// `SetRepeatOp`: the op rerun after each finish, started now.
    pub fn set_repeat_op(&self, op: *mut c_void) {
        let _held = Held::new(self.lock);
        self.assign(Slot::RepeatOp, op as usize);
        self.run_repeat_op();
    }

    /// `RunRepeatOp`: unless missing or pending, the repeat op runs async with no timeout, its completion the socket.
    pub fn run_repeat_op(&self) {
        let _held = Held::new(self.lock);
        let r = *self.slot(Slot::RepeatOp);
        if r == 0 || self.op_status(r) == WS_PENDING {
            return;
        }
        self.route_to_socket(r);
        self.op_set_socket(r);
        self.op_run_async(r, -1);
    }

    /// `QueueOp`: the op (with the default completion when it has none) joins the queue; the queue runs if idle.
    pub fn queue_op(&self, op: *mut c_void, timeout: c_long) {
        let _held = Held::new(self.lock);
        let o = op as usize;
        self.op_set_socket(o);
        if let Some(h) = self.hooks.op_set_timeout {
            // SAFETY: the hooks' contract
            unsafe { h(o as *mut c_void, timeout as u32) };
        }
        if self.op_completion(o) == 0 {
            let d = *self.slot(Slot::Default);
            self.op_set_completion(o, d);
        }
        self.add_ref(o, true);
        self.st().queue.push_back(o);
        self.run_queue_op();
    }

    /// `RunQueueOp`: when no queued op runs, the next one starts async, its completion kept and the socket's set.
    pub fn run_queue_op(&self) {
        let _held = Held::new(self.lock);
        if *self.slot(Slot::QueueOp) != 0 {
            return;
        }
        let Some(next) = self.st().queue.pop_front() else {
            return;
        };
        // the queue's reference becomes mQueueOp's (assigned, then the popped list node's released)
        self.assign(Slot::QueueOp, next);
        self.release(next, true);
        let c = self.op_completion(next);
        self.assign(Slot::Cur, c);
        self.route_to_socket(next);
        self.op_set_socket(next);
        // SAFETY: the hooks' contract
        let t = self.hooks.op_timeout.map_or(u32::MAX, |h| unsafe { h(next as *mut c_void) });
        self.op_run_async(next, t as i32 as c_long);
    }

    /// `QueueClose`: a close after the queued ops; the repeat op is killed; a forced-close timer when asked.
    pub fn queue_close(&self, timeout: c_long) {
        let _held = Held::new(self.lock);
        self.st().close_queued = true;
        let r = *self.slot(Slot::RepeatOp);
        self.call(self.hooks.op_kill, r);
        let op = self.hold(self.new_object(self.hooks.new_queue_close_op), true);
        self.queue_op(op.get(), -1);
        if timeout != -1 {
            self.start_close_timer(timeout);
        }
    }

    /// `QueueOpFast`: with a queued op running it queues; else the op runs now through a fast completion and,
    /// when it is still pending, becomes the queued op.
    pub fn queue_op_fast(&self, op: *mut c_void, timeout: u32) {
        let _held = Held::new(self.lock);
        if *self.slot(Slot::QueueOp) != 0 {
            self.queue_op(op, timeout as i32 as c_long);
            return;
        }
        let o = op as usize;
        self.op_set_socket(o);
        let mut original = self.hold(self.op_completion(o) as *mut c_void, false);
        if original.ptr == 0 {
            original = self.copy(Slot::Default);
        }
        // SAFETY: the hooks' contract
        let fast = self.hooks.new_fast_completion.map_or(null_mut(), |h| unsafe { h(self.socket, original.get()) });
        let fast = self.hold(fast, false);
        self.op_set_completion(o, fast.ptr);
        self.op_run_async(o, timeout as i32 as c_long);
        if self.op_status(o) == WS_PENDING {
            self.assign(Slot::QueueOp, o);
            self.assign(Slot::Cur, original.ptr);
        }
    }

    /// `OpenHook`: no close queued.
    pub fn open_hook(&self) {
        self.st().close_queued = false;
    }

    /// `CloseHook`: the repeat op, the running and every queued op and the close timer are killed (the first two
    /// with their own completions back).
    pub fn close_hook(&self) {
        self.st().close_queued = false;
        let r = *self.slot(Slot::RepeatOp);
        if r != 0 {
            let c = *self.slot(Slot::Repeat);
            self.op_set_completion(r, c);
            self.call(self.hooks.op_kill, r);
            self.assign(Slot::RepeatOp, 0);
        }
        let q = *self.slot(Slot::QueueOp);
        if q != 0 {
            let c = *self.slot(Slot::Cur);
            self.op_set_completion(q, c);
            self.call(self.hooks.op_kill, q);
            self.assign(Slot::QueueOp, 0);
            self.assign(Slot::Cur, 0);
        }
        let queued: Vec<usize> = self.st().queue.iter().copied().collect();
        for o in queued {
            self.call(self.hooks.op_kill, o);
        }
        let t = *self.slot(Slot::CloseTimer);
        if t != 0 {
            self.call(self.hooks.op_kill, t);
            self.assign(Slot::CloseTimer, 0);
        }
        let all = std::mem::take(&mut self.st().queue);
        for o in all {
            self.release(o, true);
        }
    }

    /// `KillHook`: every completion dropped (default, current, repeat, close).
    pub fn kill_hook(&self) {
        self.assign(Slot::Default, 0);
        self.assign(Slot::Cur, 0);
        self.assign(Slot::Repeat, 0);
        self.assign(Slot::Close, 0);
    }
}

impl Drop for EeWonQueueSocket {
    fn drop(&mut self) {
        // the implicit destructor's members, in reverse declaration order: the queue, the close timer, the
        // queued and repeat ops, then the close, repeat, current and default completions
        let all = std::mem::take(&mut self.st().queue);
        for o in all {
            self.release(o, true);
        }
        for s in [Slot::CloseTimer, Slot::QueueOp, Slot::RepeatOp, Slot::Close, Slot::Repeat, Slot::Cur, Slot::Default]
        {
            self.assign(s, 0);
        }
    }
}

// ---- the C ABI

/// A new queue object for the C++ `socket` (the hooks get it), on the socket's lock `lock`.
///
/// # Safety
/// `hooks` is a static table; `socket` and `lock` outlive the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_new(
    socket: *mut c_void,
    hooks: *const EeWonQueueSocketHooks,
    lock: *mut c_void,
) -> *mut EeWonQueueSocket {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonQueueSocket::new(socket, unsafe { &*hooks }, lock.cast())))
}

/// Frees the queue object (its references go).
///
/// # Safety
/// `q` came from `ee_won_queuesocket_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_free(q: *mut EeWonQueueSocket) {
    if !q.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(q) });
    }
}

macro_rules! q {
    ($q:expr) => {
        // SAFETY: the callers pass a live queue object
        unsafe { &*$q }
    };
}

/// `SetRepeatCompletion`.
///
/// # Safety
/// `q` is a live queue object (so for every `ee_won_queuesocket_` call); the objects passed are live C++ objects
/// of the kinds named or null.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_set_repeat_completion(q: *const EeWonQueueSocket, c: *mut c_void) {
    q!(q).set_repeat_completion(c)
}
/// `SetCloseCompletion`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_set_close_completion(q: *const EeWonQueueSocket, c: *mut c_void) {
    q!(q).set_close_completion(c)
}
/// `SetQueueCompletion`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_set_queue_completion(q: *const EeWonQueueSocket, c: *mut c_void) {
    q!(q).set_queue_completion(c)
}
/// `StartCloseTimer`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_start_close_timer(q: *const EeWonQueueSocket, timeout: c_long) {
    q!(q).start_close_timer(timeout)
}
/// `CancelCloseTimer`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_cancel_close_timer(q: *const EeWonQueueSocket) {
    q!(q).cancel_close_timer()
}
/// `CloseTimerCallback`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_close_timer_callback(q: *const EeWonQueueSocket, timer: *mut c_void) {
    q!(q).close_timer_callback(timer)
}
/// `OpCallback`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_op_callback(q: *const EeWonQueueSocket, op: *mut c_void) {
    q!(q).op_callback(op)
}
/// `SetRepeatOp`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_set_repeat_op(q: *const EeWonQueueSocket, op: *mut c_void) {
    q!(q).set_repeat_op(op)
}
/// `RunRepeatOp`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_run_repeat_op(q: *const EeWonQueueSocket) {
    q!(q).run_repeat_op()
}
/// `QueueOp`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_queue_op(q: *const EeWonQueueSocket, op: *mut c_void, timeout: c_long) {
    q!(q).queue_op(op, timeout)
}
/// `RunQueueOp`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_run_queue_op(q: *const EeWonQueueSocket) {
    q!(q).run_queue_op()
}
/// `QueueClose`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_queue_close(q: *const EeWonQueueSocket, timeout: c_long) {
    q!(q).queue_close(timeout)
}
/// `QueueOpFast`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_queue_op_fast(q: *const EeWonQueueSocket, op: *mut c_void, timeout: u32) {
    q!(q).queue_op_fast(op, timeout)
}
/// `OpenHook`.
///
/// # Safety
/// As `ee_won_queuesocket_set_repeat_completion`; the caller holds the socket's lock.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_open_hook(q: *const EeWonQueueSocket) {
    q!(q).open_hook()
}
/// `CloseHook`.
///
/// # Safety
/// As `ee_won_queuesocket_open_hook`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_close_hook(q: *const EeWonQueueSocket) {
    q!(q).close_hook()
}
/// `KillHook`.
///
/// # Safety
/// As `ee_won_queuesocket_open_hook`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_queuesocket_kill_hook(q: *const EeWonQueueSocket) {
    q!(q).kill_hook()
}

// ---- LobbyLanSocket

/// What a LAN socket object calls back: the client's references and its back link (`client->mSocket = socket`).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonLanSocketHooks {
    pub client_add_ref: EeWonQueueFn,
    pub client_release: EeWonQueueFn,
    pub client_set_socket: Option<unsafe extern "C" fn(client: *mut c_void, socket: *mut c_void)>,
}

/// `LobbyLanSocket`'s own state (round 14): its client (the C++ `LobbyLanClient`, a reference held) and the time
/// its last message came (`time(NULL)`; the original left it unset until then, here 0).
pub struct EeWonLanSocket {
    socket: *mut c_void,
    hooks: &'static EeWonLanSocketHooks,
    client: std::sync::atomic::AtomicPtr<c_void>,
    last_message_time: std::sync::atomic::AtomicI64,
}

// SAFETY: atomics; the hooks' contract makes the C++ calls thread-safe as the C++ was (the lobby's thread)
unsafe impl Send for EeWonLanSocket {}
// SAFETY: as Send
unsafe impl Sync for EeWonLanSocket {}

impl EeWonLanSocket {
    pub fn new(socket: *mut c_void, hooks: &'static EeWonLanSocketHooks) -> EeWonLanSocket {
        EeWonLanSocket {
            socket,
            hooks,
            client: std::sync::atomic::AtomicPtr::new(null_mut()),
            last_message_time: std::sync::atomic::AtomicI64::new(0),
        }
    }

    pub fn client(&self) -> *mut c_void {
        self.client.load(std::sync::atomic::Ordering::Relaxed)
    }

    fn link(&self, client: *mut c_void, socket: *mut c_void) {
        if !client.is_null() {
            if let Some(h) = self.hooks.client_set_socket {
                // SAFETY: the hooks' contract: a live client
                unsafe { h(client, socket) };
            }
        }
    }

    fn call(&self, hook: EeWonQueueFn, client: *mut c_void) {
        if !client.is_null() {
            if let Some(h) = hook {
                // SAFETY: the hooks' contract: a live client
                unsafe { h(client) };
            }
        }
    }

    /// `SetClient`: the previous client unlinked, the new one held and linked back to this socket.
    pub fn set_client(&self, client: *mut c_void) {
        self.link(self.client(), null_mut());
        self.call(self.hooks.client_add_ref, client);
        let old = self.client.swap(client, std::sync::atomic::Ordering::Relaxed);
        self.call(self.hooks.client_release, old);
        self.link(client, self.socket);
    }

    pub fn last_message_time(&self) -> i64 {
        self.last_message_time.load(std::sync::atomic::Ordering::Relaxed)
    }
    pub fn set_last_message_time(&self, t: i64) {
        self.last_message_time.store(t, std::sync::atomic::Ordering::Relaxed);
    }
}

impl Drop for EeWonLanSocket {
    fn drop(&mut self) {
        // the client's reference goes (no unlink: the original's implicit destructor)
        let old = self.client.swap(null_mut(), std::sync::atomic::Ordering::Relaxed);
        self.call(self.hooks.client_release, old);
    }
}

/// A new LAN socket object for the C++ `socket`.
///
/// # Safety
/// `hooks` is a static table; `socket` outlives the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lansocket_new(
    socket: *mut c_void,
    hooks: *const EeWonLanSocketHooks,
) -> *mut EeWonLanSocket {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonLanSocket::new(socket, unsafe { &*hooks })))
}
/// Frees the LAN socket object.
///
/// # Safety
/// `l` came from `ee_won_lansocket_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lansocket_free(l: *mut EeWonLanSocket) {
    if !l.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(l) });
    }
}
/// `GetClient`.
///
/// # Safety
/// `l` is a live LAN socket object (so for every `ee_won_lansocket_` call).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lansocket_client(l: *const EeWonLanSocket) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { &*l }.client()
}
/// `SetClient`.
///
/// # Safety
/// As `ee_won_lansocket_client`; `client` is null or a live client.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lansocket_set_client(l: *const EeWonLanSocket, client: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { &*l }.set_client(client)
}
/// `mLastMessageTime`.
///
/// # Safety
/// As `ee_won_lansocket_client`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lansocket_last_message_time(l: *const EeWonLanSocket) -> i64 {
    // SAFETY: the caller's contract
    unsafe { &*l }.last_message_time()
}
/// `mLastMessageTime = t`.
///
/// # Safety
/// As `ee_won_lansocket_client`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lansocket_set_last_message_time(l: *const EeWonLanSocket, t: i64) {
    // SAFETY: the caller's contract
    unsafe { &*l }.set_last_message_time(t)
}

#[cfg(test)]
#[path = "qsock_tests.rs"]
mod tests;
