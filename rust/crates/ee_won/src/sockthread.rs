//! WONSocket's `SocketThreadSimple` as a Rust object (round 13): the ops it continues (a reference each, in the
//! ops' address order as the original's `std::set` of `SmartPtr`s), its lock, `DoOp`, the pump, the purge, and the
//! adds and removes. The C++ class is a shim: a `Thread` whose `ThreadFunc` pumps this object and then waits on the
//! thread's signal for the time [`EeWonSocketThread::pump`] returns. The ops' steps come back to the C++ as the
//! [`EeWonSocketOpHooks`] (their status, the virtual `Continue`, `Finish`, `Kill(true)`, the references and the op's
//! `mInSocketThread`). The behaviour is `golden_asyncop.txt`'s socket thread lines, recorded over the C++ class.

use std::cell::UnsafeCell;
use std::ffi::{c_long, c_void};

use ee_plat_sys::thread::imp as plat;

use crate::asyncop::{socket_op_finishes, socket_pump_wait, OpHandle, OpSet};
use crate::status::*;

/// A C++ call on a socket op (`SocketOp*`).
pub type EeWonSocketOpFn = Option<unsafe extern "C" fn(op: *mut c_void)>;

/// What the socket thread calls back in the C++. One static table serves every socket thread.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonSocketOpHooks {
    /// `op->GetStatus()`
    pub status: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    /// the virtual `op->Continue()`
    pub continue_op: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    /// `op->Finish(status)`
    pub finish: Option<unsafe extern "C" fn(op: *mut c_void, status: c_long)>,
    /// `op->mInSocketThread = on`
    pub set_in_thread: Option<unsafe extern "C" fn(op: *mut c_void, on: bool)>,
    /// `op->AddRef()`, `op->Release()`, `op->Kill(true)`
    pub add_ref: EeWonSocketOpFn,
    pub release: EeWonSocketOpFn,
    pub kill: EeWonSocketOpFn,
}

impl EeWonSocketOpHooks {
    fn status(&self, op: OpHandle) -> Status {
        // SAFETY: the hooks' contract: `op` is a live op
        self.status.map_or(WS_NONE, |h| unsafe { h(op as *mut c_void) } as Status)
    }

    fn call(&self, hook: EeWonSocketOpFn, op: OpHandle) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract
            unsafe { h(op as *mut c_void) };
        }
    }

    fn set_in_thread(&self, op: OpHandle, on: bool) {
        if let Some(h) = self.set_in_thread {
            // SAFETY: the hooks' contract
            unsafe { h(op as *mut c_void, on) };
        }
    }

    /// `SocketThread::DoOp`: a pending op is continued and finished unless it still waits; true once it is done.
    pub fn do_op(&self, op: OpHandle) -> bool {
        if self.status(op) == WS_PENDING {
            // SAFETY: the hooks' contract
            let s = self.continue_op.map_or(WS_SUCCESS, |h| unsafe { h(op as *mut c_void) } as Status);
            if socket_op_finishes(s) {
                self.set_in_thread(op, false);
                if let Some(h) = self.finish {
                    // SAFETY: the hooks' contract
                    unsafe { h(op as *mut c_void, s as c_long) };
                }
            }
        }
        if self.status(op) != WS_PENDING {
            self.set_in_thread(op, false);
            return true;
        }
        false
    }
}

/// A recursive platform lock held for a scope.
struct Held(*mut plat::PlatMutex);

impl Held {
    fn new(m: *mut plat::PlatMutex) -> Held {
        // SAFETY: `m` is the owner's live mutex
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

/// `SocketThreadSimple`'s op set and lock (the base's `mDataCrit`).
pub struct EeWonSocketThread {
    hooks: &'static EeWonSocketOpHooks,
    lock: *mut plat::PlatMutex,
    ops: UnsafeCell<OpSet>,
}

// SAFETY: `ops` is touched under `lock`; no borrow lives across a hook
unsafe impl Send for EeWonSocketThread {}
// SAFETY: as Send
unsafe impl Sync for EeWonSocketThread {}

impl EeWonSocketThread {
    pub fn new(hooks: &'static EeWonSocketOpHooks) -> EeWonSocketThread {
        EeWonSocketThread {
            hooks,
            // SAFETY: a plain platform call
            lock: unsafe { plat::PlatMutexCreate() },
            ops: UnsafeCell::new(OpSet::new()),
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn ops(&self) -> &mut OpSet {
        // SAFETY: the callers hold the lock and drop the borrow before any hook
        unsafe { &mut *self.ops.get() }
    }

    /// `AddSocketOp`: held with a reference (once), marked in the thread.
    pub fn add(&self, op: OpHandle) {
        let _held = Held::new(self.lock);
        if self.ops().insert(op) {
            self.hooks.call(self.hooks.add_ref, op);
        }
        self.hooks.set_in_thread(op, true);
    }

    /// `RemoveSocketOp`: no longer held nor marked.
    pub fn remove(&self, op: OpHandle) {
        let _held = Held::new(self.lock);
        let held = self.ops().remove(op);
        self.hooks.set_in_thread(op, false);
        if held {
            self.hooks.call(self.hooks.release, op);
        }
    }

    /// `Pump`'s work: every held op continued once in order, the finished ones dropped; the time to wait on the
    /// thread's signal after it (at most 20 ms; 0: none).
    pub fn pump(&self, wait: u32) -> u32 {
        {
            let _held = Held::new(self.lock);
            let snapshot = self.ops().snapshot();
            for op in snapshot {
                if self.hooks.do_op(op) && self.ops().remove(op) {
                    self.hooks.call(self.hooks.release, op);
                }
            }
        }
        socket_pump_wait(wait)
    }

    /// `PurgeOps`: every op out of the set, then killed with its completion and released.
    pub fn purge(&self) {
        let ops = {
            let _held = Held::new(self.lock);
            self.ops().take_all()
        };
        for op in ops {
            self.hooks.call(self.hooks.kill, op);
            self.hooks.call(self.hooks.release, op);
        }
    }

    /// The number of ops held.
    pub fn len(&self) -> usize {
        let _held = Held::new(self.lock);
        self.ops().len()
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

impl Drop for EeWonSocketThread {
    fn drop(&mut self) {
        // `~SocketThreadSimple`: the purge, then the lock
        self.purge();
        // SAFETY: the object's mutex; nothing holds it any more
        unsafe { plat::PlatMutexDestroy(self.lock) };
    }
}

/// # Safety
/// `hooks` lives for the program.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_new(hooks: *const EeWonSocketOpHooks) -> *mut EeWonSocketThread {
    // SAFETY: the caller's contract
    Box::into_raw(Box::new(EeWonSocketThread::new(unsafe { &*hooks })))
}

/// # Safety
/// `t` came from `ee_won_socket_thread_new` (or is null).
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_free(t: *mut EeWonSocketThread) {
    if !t.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(t) });
    }
}

/// # Safety
/// `t` is valid; `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_add(t: *const EeWonSocketThread, op: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { (*t).add(op as OpHandle) }
}

/// # Safety
/// `t` is valid; `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_remove(t: *const EeWonSocketThread, op: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { (*t).remove(op as OpHandle) }
}

/// The pump's work; the time to wait on the thread's signal after it.
///
/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_pump(t: *const EeWonSocketThread, wait: u32) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*t).pump(wait) }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_purge(t: *const EeWonSocketThread) {
    // SAFETY: the caller's contract
    unsafe { (*t).purge() }
}

/// # Safety
/// `t` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_len(t: *const EeWonSocketThread) -> usize {
    // SAFETY: the caller's contract
    unsafe { (*t).len() }
}

/// `SocketThread::DoOp` (a static of the C++ class).
///
/// # Safety
/// `hooks` is valid; `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socket_thread_do_op(hooks: *const EeWonSocketOpHooks, op: *mut c_void) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*hooks).do_op(op as OpHandle) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;
    use std::collections::BTreeMap;

    thread_local! {
        static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
        /// each op's status and the statuses its Continue returns
        static OPS: RefCell<BTreeMap<usize, (Status, Vec<Status>)>> = const { RefCell::new(BTreeMap::new()) };
    }

    fn log(s: String) {
        LOG.with(|l| l.borrow_mut().push(s));
    }
    fn take() -> Vec<String> {
        LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
    }
    fn op(id: usize, status: Status, script: &[Status]) {
        OPS.with(|o| o.borrow_mut().insert(id, (status, script.to_vec())));
    }

    unsafe extern "C" fn status(op: *mut c_void) -> c_long {
        OPS.with(|o| o.borrow()[&(op as usize)].0) as c_long
    }
    unsafe extern "C" fn continue_op(op: *mut c_void) -> c_long {
        let s = OPS.with(|o| {
            let mut o = o.borrow_mut();
            let e = o.get_mut(&(op as usize)).unwrap();
            if e.1.is_empty() {
                WS_SUCCESS
            } else {
                e.1.remove(0)
            }
        });
        log(format!("continue {} {}", op as usize, s));
        s as c_long
    }
    unsafe extern "C" fn finish(op: *mut c_void, s: c_long) {
        OPS.with(|o| o.borrow_mut().get_mut(&(op as usize)).unwrap().0 = s as Status);
        log(format!("finish {} {}", op as usize, s));
    }
    unsafe extern "C" fn set_in_thread(op: *mut c_void, on: bool) {
        log(format!("in {} {}", op as usize, on as u8));
    }
    unsafe extern "C" fn add_ref(op: *mut c_void) {
        log(format!("ref+ {}", op as usize));
    }
    unsafe extern "C" fn release(op: *mut c_void) {
        log(format!("ref- {}", op as usize));
    }
    unsafe extern "C" fn kill(op: *mut c_void) {
        log(format!("kill {}", op as usize));
    }

    static HOOKS: EeWonSocketOpHooks = EeWonSocketOpHooks {
        status: Some(status),
        continue_op: Some(continue_op),
        finish: Some(finish),
        set_in_thread: Some(set_in_thread),
        add_ref: Some(add_ref),
        release: Some(release),
        kill: Some(kill),
    };

    #[test]
    fn pump_continues_in_address_order_and_drops_the_finished() {
        let t = EeWonSocketThread::new(&HOOKS);
        op(20, WS_PENDING, &[WS_TIMED_OUT, WS_SUCCESS]);
        op(10, WS_PENDING, &[WS_TIMED_OUT]);
        op(30, WS_SUCCESS, &[]);
        t.add(20);
        t.add(20);
        t.add(10);
        t.add(30);
        assert_eq!(take(), ["ref+ 20", "in 20 1", "in 20 1", "ref+ 10", "in 10 1", "ref+ 30", "in 30 1"]);
        assert_eq!(t.pump(1000), 20);
        assert_eq!(take(), ["continue 10 100001", "continue 20 100001", "in 30 0", "ref- 30"]);
        assert_eq!(t.pump(5), 5);
        assert_eq!(
            take(),
            [
                "continue 10 0",
                "in 10 0",
                "finish 10 0",
                "in 10 0",
                "ref- 10",
                "continue 20 0",
                "in 20 0",
                "finish 20 0",
                "in 20 0",
                "ref- 20"
            ]
        );
        assert!(t.is_empty());
    }

    #[test]
    fn remove_purge_and_drop() {
        let t = EeWonSocketThread::new(&HOOKS);
        op(1, WS_PENDING, &[]);
        op(2, WS_PENDING, &[]);
        t.add(1);
        t.add(2);
        take();
        t.remove(1);
        t.remove(1);
        assert_eq!(take(), ["in 1 0", "ref- 1", "in 1 0"]);
        t.purge();
        assert_eq!(take(), ["kill 2", "ref- 2"]);
        t.add(1);
        take();
        drop(t);
        assert_eq!(take(), ["kill 1", "ref- 1"]);
    }
}
