//! WONServer's `ServerConnection` as a Rust object (net round 16): the auth context and auth type a connect
//! authenticates with, the peer authentication op running a connect, the connect completion, the keep-alive timer and
//! its period (a reference each where the C++ held a `SmartPtr`), and every step of `ServerConnection.cpp`: the
//! blocking and asynchronous connects, the connect callback (the repeat receive and the keep-alive start on success),
//! the keep-alive's timer and no-op message, and the close and kill hooks. The C++ `ServerConnection` is a shim (its
//! subclasses, `RoutingConnection` and the lobby's, keep their virtual `MsgCallback`); what is C++'s (the objects'
//! references and calls, `PeerAuthOp`, the timer `AsyncOp`, the `ParamCompletion`s, the queue socket's own hooks and
//! queue) comes back through the [`EeWonServerConnHooks`].
//!
//! The lock is the socket's (`GetDataLock()`), held where the C++ held it; a reference is dropped under it where a
//! C++ `SmartPtr` assignment dropped it. No borrow of the state lives across a hook: a hook may call back into this
//! object (a kill completes an op whose completion reaches `connect_callback`, `keep_alive_callback` or
//! `set_connect_completion`). The behaviour is `golden_servconn.txt`, recorded over the C++ class.

use std::cell::UnsafeCell;
use std::ffi::{c_long, c_void};
use std::ptr::null_mut;

use ee_plat_sys::thread::imp as plat;

use crate::server::{keep_alive_ms, no_op};
use crate::status::*;

/// What the connection object calls back in the C++. One static table serves every connection. Every object is
/// passed as its `RefCount*`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonServerConnHooks {
    /// `object->AddRef()`, `->Release()` (an auth context, an op or a completion)
    pub add_ref: Option<unsafe extern "C" fn(object: *mut c_void)>,
    pub release: Option<unsafe extern "C" fn(object: *mut c_void)>,
    /// an op's `GetStatus()`, `IsKilled()`, `Kill(true)`, `SetCompletion(c)`, `RunAsync(t)`, `RunBlock(t)`
    pub op_status: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    pub op_killed: Option<unsafe extern "C" fn(op: *mut c_void) -> bool>,
    pub op_kill: Option<unsafe extern "C" fn(op: *mut c_void)>,
    pub op_set_completion: Option<unsafe extern "C" fn(op: *mut c_void, completion: *mut c_void)>,
    pub op_run_async: Option<unsafe extern "C" fn(op: *mut c_void, timeout: c_long)>,
    pub op_run_block: Option<unsafe extern "C" fn(op: *mut c_void, timeout: u32)>,
    /// `completion->Complete(AsyncOpPtr(op))`
    pub complete: Option<unsafe extern "C" fn(completion: *mut c_void, op: *mut c_void)>,
    /// `new PeerAuthOp(*address, context, type, conn, true)` (the op's reference not yet taken)
    pub new_peer_auth: Option<
        unsafe extern "C" fn(conn: *mut c_void, address: *const c_void, context: *mut c_void, auth: i32) -> *mut c_void,
    >,
    /// `op->SetCompletion(new ParamCompletion(StaticConnectCallback, conn))`
    pub route_connect: Option<unsafe extern "C" fn(conn: *mut c_void, op: *mut c_void)>,
    /// `new AsyncOp` with its completion `ParamCompletion(StaticKeepAliveCallback, conn)` (reference not taken)
    pub new_keep_alive: Option<unsafe extern "C" fn(conn: *mut c_void) -> *mut c_void>,
    /// `SetRepeatCompletion(new ParamCompletion(StaticMsgCallback, conn))`, then `SetRepeatOp(new RecvMsgOp)`
    pub start_messages: Option<unsafe extern "C" fn(conn: *mut c_void)>,
    /// `SetRepeatCompletion(NULL)`
    pub stop_messages: Option<unsafe extern "C" fn(conn: *mut c_void)>,
    /// `QueueOp(new SendMsgOp(new ByteBuffer(bytes)), -1)`
    pub queue_message: Option<unsafe extern "C" fn(conn: *mut c_void, bytes: *const u8, length: usize)>,
    /// the socket's `Close()`, `GetLengthFieldSize()`, and `QueueSocket::CloseHook()`, `QueueSocket::KillHook()`
    pub close: Option<unsafe extern "C" fn(conn: *mut c_void)>,
    pub length_field_size: Option<unsafe extern "C" fn(conn: *mut c_void) -> u8>,
    pub queue_close_hook: Option<unsafe extern "C" fn(conn: *mut c_void)>,
    pub queue_kill_hook: Option<unsafe extern "C" fn(conn: *mut c_void)>,
}

/// `AuthType`'s `AUTH_TYPE_NONE`, the constructor's.
const AUTH_NONE: i32 = 0;
/// The constructor's keep-alive period, in seconds.
const DEFAULT_KEEP_ALIVE: u32 = 300;

struct State {
    auth_context: usize,
    auth_type: i32,
    peer_op: usize,
    connect_completion: usize,
    keep_alive_op: usize,
    keep_alive_period: u32,
}

#[derive(Clone, Copy)]
enum Slot {
    AuthContext,
    PeerOp,
    ConnectCompletion,
    KeepAliveOp,
}

/// `ServerConnection`'s state and steps.
pub struct EeWonServerConn {
    /// the C++ `ServerConnection*` the hooks get
    conn: *mut c_void,
    hooks: &'static EeWonServerConnHooks,
    /// the socket's lock
    lock: *mut plat::PlatMutex,
    state: UnsafeCell<State>,
}

// SAFETY: the state is touched under `lock` only; no borrow lives across a hook
unsafe impl Send for EeWonServerConn {}
// SAFETY: as Send
unsafe impl Sync for EeWonServerConn {}

/// The socket's lock, held for a scope (an `AutoCrit`); `leave` releases it early as `AutoCrit::Leave` did.
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

/// A reference held for a scope (a C++ local `SmartPtr`): released on drop.
struct Ref<'a> {
    c: &'a EeWonServerConn,
    ptr: usize,
}

impl Drop for Ref<'_> {
    fn drop(&mut self) {
        self.c.release(self.ptr);
    }
}

impl EeWonServerConn {
    /// `ServerConnection()`: no auth (`AUTH_TYPE_NONE`), a 300-second keep-alive.
    pub fn new(conn: *mut c_void, hooks: &'static EeWonServerConnHooks, lock: *mut plat::PlatMutex) -> Self {
        EeWonServerConn {
            conn,
            hooks,
            lock,
            state: UnsafeCell::new(State {
                auth_context: 0,
                auth_type: AUTH_NONE,
                peer_op: 0,
                connect_completion: 0,
                keep_alive_op: 0,
                keep_alive_period: DEFAULT_KEEP_ALIVE,
            }),
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn st(&self) -> &mut State {
        // SAFETY: the callers hold the lock (or own the object) and drop the borrow before any hook
        unsafe { &mut *self.state.get() }
    }

    fn slot(&self, s: Slot) -> &mut usize {
        let st = self.st();
        match s {
            Slot::AuthContext => &mut st.auth_context,
            Slot::PeerOp => &mut st.peer_op,
            Slot::ConnectCompletion => &mut st.connect_completion,
            Slot::KeepAliveOp => &mut st.keep_alive_op,
        }
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

    /// A `SmartPtr` assignment to a slot: the new reference first, then the old one goes.
    fn assign(&self, s: Slot, object: usize) {
        self.add_ref(object);
        let old = std::mem::replace(self.slot(s), object);
        self.release(old);
    }

    /// A local copy of a slot (`SmartPtr<...> a = mX;`).
    fn copy(&self, s: Slot) -> Ref<'_> {
        let ptr = *self.slot(s);
        self.add_ref(ptr);
        Ref { c: self, ptr }
    }

    fn conn_call(&self, hook: Option<unsafe extern "C" fn(*mut c_void)>) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract: the live connection
            unsafe { h(self.conn) };
        }
    }

    fn op_call(&self, hook: Option<unsafe extern "C" fn(*mut c_void)>, op: usize) {
        if let Some(h) = hook {
            // SAFETY: the hooks' contract: a live op
            unsafe { h(op as *mut c_void) };
        }
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

    fn op_run_async(&self, op: usize, timeout: c_long) {
        if let Some(h) = self.hooks.op_run_async {
            // SAFETY: the hooks' contract
            unsafe { h(op as *mut c_void, timeout) };
        }
    }

    /// `StartKeepAlive`: the running timer killed and dropped, a new one for the period (none at 0).
    pub fn start_keep_alive(&self) {
        let _held = Held::new(self.lock);
        let timer = *self.slot(Slot::KeepAliveOp);
        if timer != 0 {
            // the kill may complete the timer (keep_alive_callback drops it from the slot): read the slot again
            self.op_call(self.hooks.op_kill, timer);
            self.assign(Slot::KeepAliveOp, 0);
        }
        if let Some(ms) = keep_alive_ms(self.st().keep_alive_period) {
            // SAFETY: the hooks' contract
            let timer = self.hooks.new_keep_alive.map_or(null_mut(), |h| unsafe { h(self.conn) }) as usize;
            self.assign(Slot::KeepAliveOp, timer);
            self.op_run_async(timer, ms as c_long);
        }
    }

    /// `SetAuth(context, type)`.
    pub fn set_auth(&self, context: *mut c_void, auth_type: i32) {
        let _held = Held::new(self.lock);
        self.assign(Slot::AuthContext, context as usize);
        self.st().auth_type = auth_type;
    }

    /// `ConnectCallback(op)`: a connect that is not the current one is ignored; the connect completion gets it, and
    /// on success the messages are received (the repeat op) and the keep-alive starts.
    pub fn connect_callback(&self, op: usize) {
        let mut held = Held::new(self.lock);
        if op != *self.slot(Slot::PeerOp) {
            return;
        }
        let completion = self.copy(Slot::ConnectCompletion);
        self.assign(Slot::PeerOp, 0);
        held.leave();
        if completion.ptr != 0 {
            if let Some(h) = self.hooks.complete {
                // SAFETY: the hooks' contract: a live completion and op
                unsafe { h(completion.ptr as *mut c_void, op as *mut c_void) };
            }
        }
        if self.op_status(op) == WS_SUCCESS {
            self.conn_call(self.hooks.start_messages);
            self.start_keep_alive();
        }
    }

    /// `KeepAliveCallback(op)`: the current timer, not killed, sends a no-op and starts the next one.
    pub fn keep_alive_callback(&self, op: usize) {
        let _held = Held::new(self.lock);
        if op != *self.slot(Slot::KeepAliveOp) {
            return;
        }
        self.assign(Slot::KeepAliveOp, 0);
        // SAFETY: the hooks' contract
        if self.hooks.op_killed.is_some_and(|h| unsafe { h(op as *mut c_void) }) {
            return;
        }
        self.send_no_op();
        self.start_keep_alive();
    }

    /// `SendNoOp`: the no-op message in the socket's length field size, queued.
    pub fn send_no_op(&self) {
        // SAFETY: the hooks' contract
        let size = self.hooks.length_field_size.map_or(4, |h| unsafe { h(self.conn) });
        let bytes = no_op(size);
        if let Some(h) = self.hooks.queue_message {
            // SAFETY: the hooks' contract; the bytes live for the call
            unsafe { h(self.conn, bytes.as_ptr(), bytes.len()) };
        }
    }

    /// `SetConnectCompletion(c)`.
    pub fn set_connect_completion(&self, completion: *mut c_void) {
        let _held = Held::new(self.lock);
        self.assign(Slot::ConnectCompletion, completion as usize);
    }

    /// `SetKeepAlivePeriod(seconds)`.
    pub fn set_keep_alive_period(&self, seconds: u32) {
        let _held = Held::new(self.lock);
        self.st().keep_alive_period = seconds;
    }

    /// `CloseHook`: the timer killed, a pending connect handed the connect completion and killed, the message
    /// receive's completion dropped, then the queue socket's hook.
    pub fn close_hook(&self) {
        let timer = *self.slot(Slot::KeepAliveOp);
        if timer != 0 {
            self.op_call(self.hooks.op_kill, timer);
            self.assign(Slot::KeepAliveOp, 0);
        }
        let peer = *self.slot(Slot::PeerOp);
        if peer != 0 {
            self.op_set_completion(peer, *self.slot(Slot::ConnectCompletion));
            if self.op_status(peer) == WS_PENDING {
                self.op_call(self.hooks.op_kill, peer);
            }
            self.assign(Slot::PeerOp, 0);
        }
        self.conn_call(self.hooks.stop_messages);
        self.conn_call(self.hooks.queue_close_hook);
    }

    /// `KillHook`: the queue socket's hook, then the connect completion and the auth context dropped.
    pub fn kill_hook(&self) {
        self.conn_call(self.hooks.queue_kill_hook);
        self.assign(Slot::ConnectCompletion, 0);
        self.assign(Slot::AuthContext, 0);
    }

    /// A new peer authentication op for `address` with the connection's auth, held in the slot.
    fn new_peer_op(&self, address: *const c_void) -> usize {
        let st = self.st();
        let (context, auth) = (st.auth_context, st.auth_type);
        // SAFETY: the hooks' contract
        let op = self
            .hooks
            .new_peer_auth
            .map_or(null_mut(), |h| unsafe { h(self.conn, address, context as *mut c_void, auth) })
            as usize;
        self.assign(Slot::PeerOp, op);
        op
    }

    /// `ConnectAsync(address, timeout)`: the socket closed, a new peer authentication run asynchronously; returns it
    /// (borrowed: the connection's reference).
    pub fn connect_async(&self, address: *const c_void, timeout: c_long) -> *mut c_void {
        let _held = Held::new(self.lock);
        self.conn_call(self.hooks.close);
        let op = self.new_peer_op(address);
        if let Some(h) = self.hooks.route_connect {
            // SAFETY: the hooks' contract: the slot's live op
            unsafe { h(self.conn, op as *mut c_void) };
        }
        self.op_run_async(op, timeout);
        *self.slot(Slot::PeerOp) as *mut c_void
    }

    /// `ConnectBlock(address, timeout)`: the socket closed, a new peer authentication run blocking, then the connect
    /// callback; returns its status.
    pub fn connect_block(&self, address: *const c_void, timeout: u32) -> Status {
        let mut held = Held::new(self.lock);
        self.conn_call(self.hooks.close);
        self.new_peer_op(address);
        let op = self.copy(Slot::PeerOp);
        held.leave();
        if let Some(h) = self.hooks.op_run_block {
            // SAFETY: the hooks' contract
            unsafe { h(op.ptr as *mut c_void, timeout) };
        }
        self.connect_callback(op.ptr);
        self.op_status(op.ptr)
    }
}

impl Drop for EeWonServerConn {
    /// The implicit destructor's member order: the timer, the connect completion, the peer op, the auth context.
    fn drop(&mut self) {
        for s in [Slot::KeepAliveOp, Slot::ConnectCompletion, Slot::PeerOp, Slot::AuthContext] {
            let old = std::mem::replace(self.slot(s), 0);
            self.release(old);
        }
    }
}

// ---- the C ABI

/// A new connection object for the C++ `conn` (the hooks get it), on the socket's lock `lock`.
///
/// # Safety
/// `hooks` is a static table; `conn` and `lock` outlive the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_new(
    conn: *mut c_void,
    hooks: *const EeWonServerConnHooks,
    lock: *mut c_void,
) -> *mut EeWonServerConn {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonServerConn::new(conn, unsafe { &*hooks }, lock.cast())))
}

/// Frees the connection object (its references go).
///
/// # Safety
/// `c` came from `ee_won_servconn_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_free(c: *mut EeWonServerConn) {
    if !c.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(c) });
    }
}

macro_rules! c {
    ($c:expr) => {
        // SAFETY: the callers pass a live connection object
        unsafe { &*$c }
    };
}

/// `StartKeepAlive`.
///
/// # Safety
/// `c` is a live connection object (so for every `ee_won_servconn_` call); the objects passed are live C++ objects
/// of the kinds named, or null where the C++ allowed it.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_start_keep_alive(c: *const EeWonServerConn) {
    c!(c).start_keep_alive()
}
/// `SetAuth(context, type)`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_set_auth(c: *const EeWonServerConn, context: *mut c_void, auth_type: i32) {
    c!(c).set_auth(context, auth_type)
}
/// `ConnectCallback(op)`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_connect_callback(c: *const EeWonServerConn, op: *mut c_void) {
    c!(c).connect_callback(op as usize)
}
/// `KeepAliveCallback(op)`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_keep_alive_callback(c: *const EeWonServerConn, op: *mut c_void) {
    c!(c).keep_alive_callback(op as usize)
}
/// `SendNoOp`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_send_no_op(c: *const EeWonServerConn) {
    c!(c).send_no_op()
}
/// `SetConnectCompletion(c)`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_set_connect_completion(c: *const EeWonServerConn, completion: *mut c_void) {
    c!(c).set_connect_completion(completion)
}
/// `SetKeepAlivePeriod(seconds)`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_set_keep_alive_period(c: *const EeWonServerConn, seconds: u32) {
    c!(c).set_keep_alive_period(seconds)
}
/// `CloseHook`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_close_hook(c: *const EeWonServerConn) {
    c!(c).close_hook()
}
/// `KillHook`.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_kill_hook(c: *const EeWonServerConn) {
    c!(c).kill_hook()
}
/// `ConnectAsync(address, timeout)`: the peer authentication op (borrowed). `address` is the C++ `IPAddr*`, handed
/// to `new_peer_auth` unread.
///
/// # Safety
/// As `ee_won_servconn_start_keep_alive`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_connect_async(
    c: *const EeWonServerConn,
    address: *const c_void,
    timeout: c_long,
) -> *mut c_void {
    c!(c).connect_async(address, timeout)
}
/// `ConnectBlock(address, timeout)`: the status.
///
/// # Safety
/// As `ee_won_servconn_connect_async`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_connect_block(
    c: *const EeWonServerConn,
    address: *const c_void,
    timeout: u32,
) -> c_long {
    c!(c).connect_block(address, timeout) as c_long
}

#[cfg(test)]
#[path = "servconn_tests.rs"]
mod tests;
