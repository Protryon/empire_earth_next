//! WONRouting's `RoutingConnection` as a Rust object (net round 16): the ops waiting for replies (a reference each,
//! in order), the completions by op type (61, a reference each) and the default completion, and the connection's own
//! steps: `AddOp` and `CalcCompletion`, `MsgCallback`'s dispatch of a message to the first op that takes it (a new
//! notification op first, for an unsolicited message), and the close and kill hooks. The request helpers
//! (`LeaveGroup`, `CreateGroup`, ...) stay C++: each one makes a C++ op class. What is C++'s (the ops' references
//! and calls, `HandleReply` with its `ReadBufferException`, the notification ops' constructors, the virtual
//! `GetUnsolicitedDerivedOp` and `RoutingOpCompleteHook`, `ServerConnection`'s hooks) comes back through the
//! [`EeWonRouteConnHooks`].
//!
//! The lock is the socket's (`GetDataLock()`), held where the C++ held it; no borrow of the state lives across a
//! hook. The behaviour is `golden_servconn.txt`'s RoutingConnection part, recorded over the C++ class.

use std::cell::UnsafeCell;
use std::collections::VecDeque;
use std::ffi::{c_long, c_void};

use ee_plat_sys::thread::imp as plat;

use crate::routemsg::{dispatch, header, unsolicited, EE_WON_RT_UNPACK_FAILURE};

/// How many op types have a completion slot (`mCompletions[61]`).
pub const EE_WON_ROUTE_COMPLETIONS: usize = 61;

/// What the routing connection object calls back in the C++. One static table serves every connection. Every
/// object is passed as its `RefCount*`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonRouteConnHooks {
    /// `object->AddRef()`, `->Release()` (an op or a completion)
    pub add_ref: Option<unsafe extern "C" fn(object: *mut c_void)>,
    pub release: Option<unsafe extern "C" fn(object: *mut c_void)>,
    /// an op's `Kill(true)`, `GetCompletion()` (borrowed), `SetCompletion(c)`, the virtual `GetType()`,
    /// `mIsDerivedServerOp`
    pub op_kill: Option<unsafe extern "C" fn(op: *mut c_void)>,
    pub op_completion: Option<unsafe extern "C" fn(op: *mut c_void) -> *mut c_void>,
    pub op_set_completion: Option<unsafe extern "C" fn(op: *mut c_void, completion: *mut c_void)>,
    pub op_type: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    pub op_derived: Option<unsafe extern "C" fn(op: *mut c_void) -> bool>,
    /// `op->HandleReply(type, msg)` over a `ReadBuffer` of the message at `pos`; a `ReadBufferException` sets
    /// `threw` (the status is then not read)
    pub handle_reply: Option<
        unsafe extern "C" fn(
            op: *mut c_void,
            message_type: u8,
            data: *const u8,
            length: usize,
            pos: usize,
            threw: *mut bool,
        ) -> i32,
    >,
    /// the end of a dispatched op: `SetMode(OP_MODE_BLOCK)`, then `ForceFinish(status)` (unsolicited) or
    /// `Finish(status)`
    pub op_end: Option<unsafe extern "C" fn(op: *mut c_void, unsolicited: bool, status: c_long)>,
    /// `op->Complete()`
    pub op_complete: Option<unsafe extern "C" fn(op: *mut c_void)>,
    /// the virtual `RoutingOpCompleteHook(op)`
    pub complete_hook: Option<unsafe extern "C" fn(conn: *mut c_void, op: *mut c_void)>,
    /// a new notification op of the kind (`EE_WON_RT_N_*`) on the connection, or for a derived server's message the
    /// virtual `GetUnsolicitedDerivedOp(type)` (`kind` -1): one reference held for the caller, or null
    pub new_unsolicited: Option<unsafe extern "C" fn(conn: *mut c_void, kind: i32, message_type: u8) -> *mut c_void>,
    /// `ServerConnection::CloseHook()`, `ServerConnection::KillHook()`
    pub server_close_hook: Option<unsafe extern "C" fn(conn: *mut c_void)>,
    pub server_kill_hook: Option<unsafe extern "C" fn(conn: *mut c_void)>,
}

struct State {
    ops: VecDeque<usize>,
    completions: [usize; EE_WON_ROUTE_COMPLETIONS],
    default_completion: usize,
}

/// `RoutingConnection`'s op list, completions and steps.
pub struct EeWonRouteConn {
    /// the C++ `RoutingConnection*` the hooks get
    conn: *mut c_void,
    hooks: &'static EeWonRouteConnHooks,
    /// the socket's lock
    lock: *mut plat::PlatMutex,
    state: UnsafeCell<State>,
}

// SAFETY: the state is touched under `lock` only (or by the owner); no borrow lives across a hook
unsafe impl Send for EeWonRouteConn {}
// SAFETY: as Send
unsafe impl Sync for EeWonRouteConn {}

/// The socket's lock, held for a scope; `leave` releases it early as `AutoCrit::Leave` did.
struct Held(*mut plat::PlatMutex, bool);

impl Held {
    fn new(m: *mut plat::PlatMutex) -> Held {
        // SAFETY: the socket's live mutex
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

/// A local `SmartPtr<RoutingOp>`: assignments take the new reference before dropping the old; released on drop.
struct Local<'a> {
    c: &'a EeWonRouteConn,
    ptr: usize,
}

impl Local<'_> {
    fn set(&mut self, object: usize) {
        self.c.add_ref(object);
        let old = std::mem::replace(&mut self.ptr, object);
        self.c.release(old);
    }
}

impl Drop for Local<'_> {
    fn drop(&mut self) {
        self.c.release(self.ptr);
    }
}

impl EeWonRouteConn {
    /// `RoutingConnection()`: no op waiting, no completion.
    pub fn new(conn: *mut c_void, hooks: &'static EeWonRouteConnHooks, lock: *mut plat::PlatMutex) -> Self {
        EeWonRouteConn {
            conn,
            hooks,
            lock,
            state: UnsafeCell::new(State {
                ops: VecDeque::new(),
                completions: [0; EE_WON_ROUTE_COMPLETIONS],
                default_completion: 0,
            }),
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn st(&self) -> &mut State {
        // SAFETY: the callers hold the lock (or own the object) and drop the borrow before any hook
        unsafe { &mut *self.state.get() }
    }

    fn add_ref(&self, object: usize) {
        if object != 0 {
            if let Some(h) = self.hooks.add_ref {
                // SAFETY: the hooks' contract: a live C++ object
                unsafe { h(object as *mut c_void) };
            }
        }
    }

    fn release(&self, object: usize) {
        if object != 0 {
            if let Some(h) = self.hooks.release {
                // SAFETY: as add_ref
                unsafe { h(object as *mut c_void) };
            }
        }
    }

    fn op_call(&self, hook: Option<unsafe extern "C" fn(*mut c_void)>, op: usize) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract: a live op
            unsafe { h(op as *mut c_void) };
        }
    }

    fn conn_call(&self, hook: Option<unsafe extern "C" fn(*mut c_void)>) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract: the live connection
            unsafe { h(self.conn) };
        }
    }

    /// A completion slot assignment (`mCompletions[type] = c`, `mDefaultCompletion = c` with `type` -1): the new
    /// reference first, then the old one goes. A type past the slots is ignored.
    pub fn set_completion(&self, op_type: i32, completion: *mut c_void) {
        let c = completion as usize;
        let slot = match op_type {
            -1 => &mut self.st().default_completion as *mut usize,
            t if (0..EE_WON_ROUTE_COMPLETIONS as i32).contains(&t) => {
                &mut self.st().completions[t as usize] as *mut usize
            }
            _ => return,
        };
        self.add_ref(c);
        // SAFETY: a slot of the state, no hook between the borrow and the write
        let old = unsafe { std::mem::replace(&mut *slot, c) };
        self.release(old);
    }

    /// `CalcCompletion(op)`: an op without a completion gets its type's, else the default one. (A type past the
    /// slots, which the original read past its array, has none.)
    pub fn calc_completion(&self, op: usize) {
        // SAFETY: the hooks' contract
        let mut c = self.hooks.op_completion.map_or(0, |h| unsafe { h(op as *mut c_void) } as usize);
        if c == 0 {
            // SAFETY: the hooks' contract
            let t = self.hooks.op_type.map_or(-1, |h| unsafe { h(op as *mut c_void) });
            c = usize::try_from(t).ok().and_then(|t| self.st().completions.get(t).copied()).unwrap_or(0);
            if c == 0 {
                c = self.st().default_completion;
            }
        }
        // the C++ held the completion in a local SmartPtr for the call
        self.add_ref(c);
        if let Some(h) = self.hooks.op_set_completion {
            // SAFETY: the hooks' contract (the op takes its own reference)
            unsafe { h(op as *mut c_void, c as *mut c_void) };
        }
        self.release(c);
    }

    /// `AddOp(op)`: its completion resolved, the op waits at the end of the list.
    pub fn add_op(&self, op: *mut c_void) {
        let _held = Held::new(self.lock);
        self.calc_completion(op as usize);
        self.add_ref(op as usize);
        self.st().ops.push_back(op as usize);
    }

    /// `GetUnsolicitedOp(type, derived)`: a notification op for the message type (a derived server's message: the
    /// virtual's op), its completion resolved; one reference held for the caller, or 0.
    fn unsolicited_op(&self, message_type: u8, derived: bool) -> usize {
        let kind = if derived { Some(-1) } else { unsolicited(message_type) };
        let Some(kind) = kind else {
            return 0;
        };
        // SAFETY: the hooks' contract
        let op = self.hooks.new_unsolicited.map_or(0, |h| unsafe { h(self.conn, kind, message_type) } as usize);
        if op != 0 {
            self.calc_completion(op);
        }
        op
    }

    /// The end of a dispatched op: out of the list (index `i`), the lock left, the op finished (forced for an
    /// unsolicited one), the complete hook when `hook`, then its completion.
    fn end_op(&self, held: &mut Held, i: usize, op: usize, unsolicited: bool, status: i32, hook: bool) {
        if let Some(gone) = self.st().ops.remove(i) {
            self.release(gone);
        }
        held.leave();
        if let Some(h) = self.hooks.op_end {
            // SAFETY: the hooks' contract: the op is held by the caller's local
            unsafe { h(op as *mut c_void, unsolicited, status as c_long) };
        }
        if hook {
            if let Some(h) = self.hooks.complete_hook {
                // SAFETY: as above
                unsafe { h(self.conn, op as *mut c_void) };
            }
        }
        self.op_call(self.hooks.op_complete, op);
    }

    /// `MsgCallback(message)`: the header read; an unsolicited message's new op first in the list; the message
    /// offered to each op of its kind (derived or not) in order until one takes it (`dispatch`): an op waiting for
    /// more stops the walk, a finished op leaves the list and completes. A read past the message's end (a
    /// `ReadBufferException`) finishes the op reading it with the last status an op returned (none yet: an unpack
    /// failure, where the original read an unset variable), without the complete hook.
    pub fn msg_callback(&self, data: &[u8]) {
        let mut held = Held::new(self.lock);
        let mut op = Local { c: self, ptr: 0 };
        let Some((message_type, derived, pos)) = header(data) else {
            return;
        };
        let new_op = self.unsolicited_op(message_type, derived);
        // the reference the caller got becomes the local's
        op.ptr = new_op;
        let mut unsolicited = false;
        if new_op != 0 {
            self.add_ref(new_op);
            self.st().ops.push_front(new_op);
            unsolicited = true;
        }
        let mut status: Option<i32> = None;
        let mut i = 0;
        while i < self.st().ops.len() {
            op.set(self.st().ops[i]);
            // SAFETY: the hooks' contract
            let op_derived = self.hooks.op_derived.is_some_and(|h| unsafe { h(op.ptr as *mut c_void) });
            if derived == op_derived {
                let mut threw = false;
                let st = self.hooks.handle_reply.map_or(0, |h| {
                    // SAFETY: the hooks' contract; the message lives for the call
                    unsafe { h(op.ptr as *mut c_void, message_type, data.as_ptr(), data.len(), pos, &mut threw) }
                });
                if threw {
                    let s = status.unwrap_or(EE_WON_RT_UNPACK_FAILURE);
                    self.end_op(&mut held, i, op.ptr, unsolicited, s, false);
                    return;
                }
                status = Some(st);
                match dispatch(st) {
                    1 => break,
                    2 => {
                        self.end_op(&mut held, i, op.ptr, unsolicited, st, true);
                        break;
                    }
                    _ => {}
                }
            }
            i += 1;
        }
    }

    /// `CloseHook`: every waiting op killed (an op a kill adds is killed too), the list emptied, then the server
    /// connection's hook.
    pub fn close_hook(&self) {
        let mut i = 0;
        while i < self.st().ops.len() {
            let op = self.st().ops[i];
            self.op_call(self.hooks.op_kill, op);
            i += 1;
        }
        while let Some(op) = self.st().ops.pop_front() {
            self.release(op);
        }
        self.conn_call(self.hooks.server_close_hook);
    }

    /// `KillHook`: the default and the per-type completions dropped, then the server connection's hook.
    pub fn kill_hook(&self) {
        self.set_completion(-1, std::ptr::null_mut());
        for t in 0..EE_WON_ROUTE_COMPLETIONS as i32 {
            self.set_completion(t, std::ptr::null_mut());
        }
        self.conn_call(self.hooks.server_kill_hook);
    }
}

impl Drop for EeWonRouteConn {
    /// The implicit destructor's member order: the default completion, the per-type ones from the last, the list.
    fn drop(&mut self) {
        let d = std::mem::replace(&mut self.st().default_completion, 0);
        self.release(d);
        for t in (0..EE_WON_ROUTE_COMPLETIONS).rev() {
            let c = std::mem::replace(&mut self.st().completions[t], 0);
            self.release(c);
        }
        while let Some(op) = self.st().ops.pop_front() {
            self.release(op);
        }
    }
}

// ---- the C ABI

/// A new routing connection object for the C++ `conn` (the hooks get it), on the socket's lock `lock`.
///
/// # Safety
/// `hooks` is a static table; `conn` and `lock` outlive the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_new(
    conn: *mut c_void,
    hooks: *const EeWonRouteConnHooks,
    lock: *mut c_void,
) -> *mut EeWonRouteConn {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonRouteConn::new(conn, unsafe { &*hooks }, lock.cast())))
}

/// Frees the routing connection object (its references go).
///
/// # Safety
/// `r` came from `ee_won_routeconn_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_free(r: *mut EeWonRouteConn) {
    if !r.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(r) });
    }
}

macro_rules! r {
    ($r:expr) => {
        // SAFETY: the callers pass a live routing connection object
        unsafe { &*$r }
    };
}

/// `mCompletions[op_type] = completion` (`op_type` -1: `mDefaultCompletion`).
///
/// # Safety
/// `r` is a live routing connection object (so for every `ee_won_routeconn_` call); the objects passed are live
/// C++ objects of the kinds named, or null where the C++ allowed it.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_set_completion(
    r: *const EeWonRouteConn,
    op_type: i32,
    completion: *mut c_void,
) {
    r!(r).set_completion(op_type, completion)
}
/// `CalcCompletion(op)`.
///
/// # Safety
/// As `ee_won_routeconn_set_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_calc_completion(r: *const EeWonRouteConn, op: *mut c_void) {
    r!(r).calc_completion(op as usize)
}
/// `AddOp(op)`.
///
/// # Safety
/// As `ee_won_routeconn_set_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_add_op(r: *const EeWonRouteConn, op: *mut c_void) {
    r!(r).add_op(op)
}
/// `MsgCallback(message)`: the message's bytes.
///
/// # Safety
/// As `ee_won_routeconn_set_completion`; `data` holds `length` bytes (or is null with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_msg_callback(r: *const EeWonRouteConn, data: *const u8, length: usize) {
    let bytes = if data.is_null() || length == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data, length) }
    };
    r!(r).msg_callback(bytes)
}
/// `CloseHook`.
///
/// # Safety
/// As `ee_won_routeconn_set_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_close_hook(r: *const EeWonRouteConn) {
    r!(r).close_hook()
}
/// `KillHook`.
///
/// # Safety
/// As `ee_won_routeconn_set_completion`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_kill_hook(r: *const EeWonRouteConn) {
    r!(r).kill_hook()
}

#[cfg(test)]
#[path = "routeconn_tests.rs"]
mod tests;
