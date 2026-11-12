//! WONSocket's `UDPManagerRecvOp` and `UDPManagerSendRecvOp` as Rust objects (round 16). The receive op waits for one
//! datagram (from an address, or from anyone) that its message check accepts, through a `UDPManager` or on a socket
//! of its own or given to it; the request op sends a datagram and waits for a reply in a receive op of its own,
//! retransmitting on a time-out up to its retry limit. Each object holds what the C++ class's members held (the
//! socket, the manager, the own-socket flag, the address, the received datagram, the check and its context; the
//! manager, the own socket, the request, the address, the reply wait, the retry limit, the attempts and the reply
//! time-out) and runs every step of UDPManager.cpp's two classes: `RunHook`, `CleanupHook`, `QueueRecv`,
//! `RecvCallback` and `SendRequest`. The C++ classes are shims (the lobby subclasses them: PingGameOp,
//! RoutingGetUserStatusOp); the `AsyncOp` they are, the virtual message check, the sockets and the ops queued on them
//! come back through the [`EeWonUdpOpHooks`]. Every object held is a `RefCount*` with a reference.
//!
//! No borrow of the state lives across a hook (a finish runs the op's completion, which may run it again). Like the
//! original, the objects take no lock. The behaviour is `golden_sockobj.txt`'s UDPManager and UDP ops lines,
//! recorded over the C++ classes.

use std::cell::UnsafeCell;
use std::cmp::Ordering;
use std::ffi::{c_long, c_void};

use crate::ipobj::EeWonIpAddr;
use crate::status::*;

/// `WS_BlockMode_NotSupported`
const WS_BLOCK_MODE_NOT_SUPPORTED: Status = 100009;
/// `WS_NoReply`
const WS_NO_REPLY: Status = 100015;
/// `OP_MODE_BLOCK`
const OP_MODE_BLOCK: i32 = 2;

/// A C++ call on one object.
pub type EeWonUdpOpFn = Option<unsafe extern "C" fn(object: *mut c_void)>;

/// What the ops call back in the C++. One static table serves every op.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonUdpOpHooks {
    /// any object's `AddRef()` / `Release()` (`RefCount*`)
    pub ref_add: EeWonUdpOpFn,
    pub ref_release: EeWonUdpOpFn,
    /// an op's `GetMode()`, `GetTimeout()`, `GetStatus()`, `Finish(s)`, `RunAsync(t)`, `Kill(true)`
    pub op_mode: Option<unsafe extern "C" fn(op: *mut c_void) -> i32>,
    pub op_timeout: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    pub op_status: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    pub op_finish: Option<unsafe extern "C" fn(op: *mut c_void, status: c_long)>,
    pub op_run_async: Option<unsafe extern "C" fn(op: *mut c_void, timeout: c_long)>,
    pub op_kill: EeWonUdpOpFn,
    /// a receive op's virtual `CheckMessage` over the datagram (`*threw` for a `ReadBufferException`)
    pub recv_check:
        Option<unsafe extern "C" fn(op: *mut c_void, data: *const u8, length: usize, threw: *mut bool) -> c_long>,
    /// `new QueueSocket(UDP)` bound to any port (`Fn2B0D90(0, false)`), a reference taken; a socket's `Kill()`
    pub socket_new_bound: Option<unsafe extern "C" fn() -> *mut c_void>,
    pub socket_kill: EeWonUdpOpFn,
    /// `QueueRecv`'s op: `QueueOp(new RecvBytesFromOp(1024)` completing to the receive op's `StaticRecvCallback`, `-1)`
    pub socket_queue_recv: Option<unsafe extern "C" fn(socket: *mut c_void, op: *mut c_void)>,
    /// `QueueOp(new SendBytesToOp(bytes, addr), -1)`
    pub socket_send: Option<unsafe extern "C" fn(socket: *mut c_void, bytes: *mut c_void, addr: *const EeWonIpAddr)>,
    /// a manager's `QueueOp(op, timeout)` and `Send(bytes, addr)`
    pub manager_queue: Option<unsafe extern "C" fn(manager: *mut c_void, op: *mut c_void, timeout: u32)>,
    pub manager_send: Option<unsafe extern "C" fn(manager: *mut c_void, bytes: *mut c_void, addr: *const EeWonIpAddr)>,
    /// the reply wait: `new UDPManagerRecvOp(addr)` completing to the request op's `StaticRecvCallback`, its check the
    /// request's `StaticCheckResponse`; a reference taken
    pub new_wait: Option<unsafe extern "C" fn(request: *mut c_void, addr: *const EeWonIpAddr) -> *mut c_void>,
    /// a receive op's Rust object
    pub recv_object: Option<unsafe extern "C" fn(op: *mut c_void) -> *const EeWonUdpRecvOp>,
}

struct Calls {
    hooks: &'static EeWonUdpOpHooks,
}

impl Calls {
    fn call(&self, h: EeWonUdpOpFn, p: usize) {
        if p != 0 {
            if let Some(h) = h {
                // SAFETY: the hooks' contract: a live object
                unsafe { h(p as *mut c_void) };
            }
        }
    }
    fn hold(&self, p: usize) -> usize {
        self.call(self.hooks.ref_add, p);
        p
    }
    fn release(&self, p: usize) {
        self.call(self.hooks.ref_release, p);
    }
    fn mode(&self, op: usize) -> i32 {
        // SAFETY: the hooks' contract: a live op
        self.hooks.op_mode.map_or(1, |h| unsafe { h(op as *mut c_void) })
    }
    fn status(&self, op: usize) -> Status {
        // SAFETY: as above
        self.hooks.op_status.map_or(WS_SUCCESS, |h| unsafe { h(op as *mut c_void) } as Status)
    }
    fn finish(&self, op: usize, status: Status) {
        if let Some(h) = self.hooks.op_finish {
            // SAFETY: as above
            unsafe { h(op as *mut c_void, status as c_long) };
        }
    }
    fn socket_new_bound(&self) -> usize {
        // SAFETY: the hooks' contract: a new socket with a reference
        self.hooks.socket_new_bound.map_or(0, |h| unsafe { h() } as usize)
    }
}

#[derive(Default)]
struct RecvState {
    socket: usize,
    manager: usize,
    own_socket: bool,
    address: EeWonIpAddr,
    msg: usize,
    verifier: usize,
    verifier_context: usize,
}

/// `UDPManagerRecvOp`'s state and steps.
pub struct EeWonUdpRecvOp {
    /// the C++ `UDPManagerRecvOp*` as `RefCount*`
    op: usize,
    calls: Calls,
    state: UnsafeCell<RecvState>,
}

// SAFETY: the state is touched by the thread running the op's calls (the C++ contract); no borrow lives across a hook
unsafe impl Send for EeWonUdpRecvOp {}
// SAFETY: as Send
unsafe impl Sync for EeWonUdpRecvOp {}

impl EeWonUdpRecvOp {
    /// `UDPManagerRecvOp()` / `UDPManagerRecvOp(addr)`: no socket, manager, message or check.
    pub fn new(op: *mut c_void, hooks: &'static EeWonUdpOpHooks, address: Option<&EeWonIpAddr>) -> EeWonUdpRecvOp {
        let state = RecvState { address: address.cloned().unwrap_or_default(), ..RecvState::default() };
        EeWonUdpRecvOp { op: op as usize, calls: Calls { hooks }, state: UnsafeCell::new(state) }
    }

    #[allow(clippy::mut_from_ref)]
    fn st(&self) -> &mut RecvState {
        // SAFETY: the one caller at a time; no borrow lives across a hook
        unsafe { &mut *self.state.get() }
    }
    fn replace(&self, slot: fn(&mut RecvState) -> &mut usize, p: usize) {
        let new = self.calls.hold(p);
        let old = std::mem::replace(slot(self.st()), new);
        self.calls.release(old);
    }

    pub fn set_manager(&self, manager: usize) {
        self.replace(|s| &mut s.manager, manager);
    }
    pub fn manager(&self) -> usize {
        self.st().manager
    }
    /// `SetSocket`: a socket the op does not own.
    pub fn set_socket(&self, socket: usize) {
        self.replace(|s| &mut s.socket, socket);
        self.st().own_socket = false;
    }
    pub fn socket(&self) -> usize {
        self.st().socket
    }
    pub fn address(&self) -> EeWonIpAddr {
        self.st().address.clone()
    }
    pub fn set_address(&self, a: &EeWonIpAddr) {
        self.st().address = a.clone();
    }
    pub fn msg(&self) -> usize {
        self.st().msg
    }
    pub fn set_msg(&self, bytes: usize) {
        self.replace(|s| &mut s.msg, bytes);
    }
    pub fn set_verifier(&self, verifier: usize, context: usize) {
        let st = self.st();
        st.verifier = verifier;
        st.verifier_context = context;
    }
    pub fn verifier(&self) -> (usize, usize) {
        let st = self.st();
        (st.verifier, st.verifier_context)
    }

    /// `QueueRecv`: one datagram receive on the op's socket.
    pub fn queue_recv(&self) {
        let s = self.st().socket;
        if s == 0 {
            return;
        }
        if let Some(h) = self.calls.hooks.socket_queue_recv {
            // SAFETY: the hooks' contract: a live socket and op
            unsafe { h(s as *mut c_void, self.op as *mut c_void) };
        }
    }

    /// `RecvCallback`: a datagram (`data`) from `from` arrived on the op's socket.
    pub fn recv(&self, data: &[u8], from: &EeWonIpAddr) {
        let c = &self.calls;
        if c.status(self.op) != WS_PENDING {
            return;
        }
        let st = self.st();
        if st.address.valid && from.compare(&st.address) != Ordering::Equal {
            self.queue_recv();
            return;
        }
        let mut threw = false;
        let status = c.hooks.recv_check.map_or(WS_SUCCESS, |h| {
            // SAFETY: the hooks' contract: a live op; the bytes outlive the call
            unsafe { h(self.op as *mut c_void, data.as_ptr(), data.len(), &mut threw) as Status }
        });
        if !threw && status != WS_NONE {
            c.finish(self.op, status);
            return;
        }
        self.queue_recv();
    }

    /// `CleanupHook`: an own socket is killed and let go.
    pub fn cleanup(&self) {
        let st = self.st();
        let s = st.socket;
        if s != 0 && st.own_socket {
            self.calls.call(self.calls.hooks.socket_kill, s);
            self.st().socket = 0;
            self.calls.release(s);
        }
    }

    /// `RunHook`: refused blocking; through the manager, or on a socket (bound here when it has none).
    pub fn run(&self) {
        let c = &self.calls;
        if c.mode(self.op) == OP_MODE_BLOCK {
            c.finish(self.op, WS_BLOCK_MODE_NOT_SUPPORTED);
            return;
        }
        let m = self.st().manager;
        if m != 0 {
            // SAFETY: the hooks' contract: a live op
            let timeout = c.hooks.op_timeout.map_or(0, |h| unsafe { h(self.op as *mut c_void) });
            if let Some(h) = c.hooks.manager_queue {
                // SAFETY: the hooks' contract: a live manager and op
                unsafe { h(m as *mut c_void, self.op as *mut c_void, timeout as u32) };
            }
            return;
        }
        if self.st().socket == 0 {
            let s = c.socket_new_bound();
            let old = std::mem::replace(&mut self.st().socket, s);
            c.release(old);
            self.st().own_socket = true;
        }
        self.queue_recv();
    }
}

impl Drop for EeWonUdpRecvOp {
    fn drop(&mut self) {
        // the original's member order backwards: the message, the manager, the socket
        let st = std::mem::take(self.st());
        for p in [st.msg, st.manager, st.socket] {
            self.calls.release(p);
        }
    }
}

struct SendRecvState {
    manager: usize,
    socket: usize,
    outgoing: usize,
    address: EeWonIpAddr,
    recv_op: usize,
    retry_limit: i32,
    attempts: i32,
    timeout: u32,
}

/// `UDPManagerSendRecvOp`'s state and steps.
pub struct EeWonUdpSendRecvOp {
    /// the C++ `UDPManagerSendRecvOp*` as `RefCount*`
    op: usize,
    calls: Calls,
    state: UnsafeCell<SendRecvState>,
}

// SAFETY: as EeWonUdpRecvOp
unsafe impl Send for EeWonUdpSendRecvOp {}
// SAFETY: as Send
unsafe impl Sync for EeWonUdpSendRecvOp {}

impl EeWonUdpSendRecvOp {
    /// `UDPManagerSendRecvOp()`: no retries, a 10-second reply wait (the attempts 0: the original left them unset).
    pub fn new(op: *mut c_void, hooks: &'static EeWonUdpOpHooks) -> EeWonUdpSendRecvOp {
        let state = SendRecvState {
            manager: 0,
            socket: 0,
            outgoing: 0,
            address: EeWonIpAddr::default(),
            recv_op: 0,
            retry_limit: 0,
            attempts: 0,
            timeout: 10000,
        };
        EeWonUdpSendRecvOp { op: op as usize, calls: Calls { hooks }, state: UnsafeCell::new(state) }
    }

    #[allow(clippy::mut_from_ref)]
    fn st(&self) -> &mut SendRecvState {
        // SAFETY: the one caller at a time; no borrow lives across a hook
        unsafe { &mut *self.state.get() }
    }
    fn replace(&self, slot: fn(&mut SendRecvState) -> &mut usize, p: usize) {
        let new = self.calls.hold(p);
        let old = std::mem::replace(slot(self.st()), new);
        self.calls.release(old);
    }

    pub fn set_manager(&self, manager: usize) {
        self.replace(|s| &mut s.manager, manager);
    }
    pub fn manager(&self) -> usize {
        self.st().manager
    }
    pub fn socket(&self) -> usize {
        self.st().socket
    }
    pub fn set_outgoing(&self, bytes: usize) {
        self.replace(|s| &mut s.outgoing, bytes);
    }
    pub fn outgoing(&self) -> usize {
        self.st().outgoing
    }
    pub fn address(&self) -> EeWonIpAddr {
        self.st().address.clone()
    }
    pub fn set_address(&self, a: &EeWonIpAddr) {
        self.st().address = a.clone();
    }
    pub fn recv_op(&self) -> usize {
        self.st().recv_op
    }
    pub fn set_retry_limit(&self, n: i32) {
        self.st().retry_limit = n;
    }
    pub fn retry_limit(&self) -> i32 {
        self.st().retry_limit
    }
    pub fn attempts(&self) -> i32 {
        self.st().attempts
    }
    pub fn set_timeout(&self, t: u32) {
        self.st().timeout = t;
    }
    pub fn timeout(&self) -> u32 {
        self.st().timeout
    }

    /// `SendRequest`: a new reply wait, the request sent through the manager or on the op's socket, the wait run.
    pub fn send_request(&self) {
        let c = &self.calls;
        let h = c.hooks;
        let addr = self.address();
        // SAFETY: the hooks' contract: a new op with a reference
        let wait = h.new_wait.map_or(0, |f| unsafe { f(self.op as *mut c_void, &addr) } as usize);
        let old = std::mem::replace(&mut self.st().recv_op, wait);
        c.release(old);
        // SAFETY: the hooks' contract: the wait's live object (the wait is held)
        let object = h.recv_object.map_or(std::ptr::null(), |f| unsafe { f(wait as *mut c_void) });
        let (m, s, bytes) = (self.st().manager, self.st().socket, self.st().outgoing);
        if m != 0 {
            if let Some(f) = h.manager_send {
                // SAFETY: the hooks' contract: a live manager and bytes
                unsafe { f(m as *mut c_void, bytes as *mut c_void, &addr) };
            }
            if !object.is_null() {
                // SAFETY: as above
                unsafe { &*object }.set_manager(m);
            }
        } else {
            if s != 0 {
                if let Some(f) = h.socket_send {
                    // SAFETY: the hooks' contract: a live socket and bytes
                    unsafe { f(s as *mut c_void, bytes as *mut c_void, &addr) };
                }
            }
            if !object.is_null() {
                // SAFETY: as above
                unsafe { &*object }.set_socket(s);
            }
        }
        if wait != 0 {
            if let Some(f) = h.op_run_async {
                // SAFETY: the hooks' contract: a live op
                unsafe { f(wait as *mut c_void, self.st().timeout as c_long) };
            }
        }
    }

    /// `RunHook`: refused blocking; the attempts start over, a socket of its own without a manager, the request sent.
    pub fn run(&self) {
        let c = &self.calls;
        if c.mode(self.op) == OP_MODE_BLOCK {
            c.finish(self.op, WS_BLOCK_MODE_NOT_SUPPORTED);
            return;
        }
        self.st().attempts = 0;
        if self.st().manager == 0 {
            let s = c.socket_new_bound();
            let old = std::mem::replace(&mut self.st().socket, s);
            c.release(old);
        }
        self.send_request();
    }

    /// `RecvCallback`: the reply wait `wait` ended: its status, or a retransmit on a time-out, or `WS_NoReply`.
    pub fn recv_done(&self, wait: usize) {
        let c = &self.calls;
        if c.status(self.op) != WS_PENDING || wait != self.st().recv_op {
            return;
        }
        let status = c.status(wait);
        if status != WS_TIMED_OUT {
            c.finish(self.op, status);
            return;
        }
        let st = self.st();
        if st.attempts >= st.retry_limit {
            c.finish(self.op, WS_NO_REPLY);
            return;
        }
        st.attempts = st.attempts.wrapping_add(1);
        self.send_request();
    }

    /// `CleanupHook`: the reply wait killed and let go, the socket killed and let go.
    pub fn cleanup(&self) {
        let c = &self.calls;
        let w = self.st().recv_op;
        if w != 0 {
            c.call(c.hooks.op_kill, w);
            let w = std::mem::replace(&mut self.st().recv_op, 0);
            c.release(w);
        }
        let s = self.st().socket;
        if s != 0 {
            c.call(c.hooks.socket_kill, s);
            let s = std::mem::replace(&mut self.st().socket, 0);
            c.release(s);
        }
    }
}

impl Drop for EeWonUdpSendRecvOp {
    fn drop(&mut self) {
        // the original's member order backwards: the reply wait, the request, the socket, the manager
        let st = self.st();
        let all = [st.recv_op, st.outgoing, st.socket, st.manager];
        st.recv_op = 0;
        st.outgoing = 0;
        st.socket = 0;
        st.manager = 0;
        for p in all {
            self.calls.release(p);
        }
    }
}

// ---- the C ABI

macro_rules! obj {
    ($p:expr) => {
        // SAFETY: the caller's live object
        unsafe { &*$p }
    };
}

fn addr_of(a: *const EeWonIpAddr) -> Option<&'static EeWonIpAddr> {
    // SAFETY: the callers' contract: null or a live value for the call
    unsafe { a.as_ref() }
}
fn boxed(a: EeWonIpAddr) -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(a))
}

/// A receive op's object (`addr` null for any sender).
///
/// # Safety
/// `op` is the C++ op as `RefCount*`, `hooks` a static table, `addr` null or a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_new(
    op: *mut c_void,
    hooks: *const EeWonUdpOpHooks,
    addr: *const EeWonIpAddr,
) -> *mut EeWonUdpRecvOp {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonUdpRecvOp::new(op, unsafe { &*hooks }, addr_of(addr))))
}
/// Frees the object, its references given back.
///
/// # Safety
/// `r` from `ee_won_udprecvop_new` (or null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_free(r: *mut EeWonUdpRecvOp) {
    if !r.is_null() {
        // SAFETY: the caller's object
        drop(unsafe { Box::from_raw(r) });
    }
}
/// `SetUDPManager` (a `UDPManager*` as `RefCount*`, or null).
///
/// # Safety
/// `r` is live; `manager` null or live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_set_manager(r: *const EeWonUdpRecvOp, manager: *mut c_void) {
    obj!(r).set_manager(manager as usize)
}
/// The manager (null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_manager(r: *const EeWonUdpRecvOp) -> *mut c_void {
    obj!(r).manager() as *mut c_void
}
/// `SetSocket` (a `QueueSocket*` as `RefCount*`, or null): not the op's own.
///
/// # Safety
/// `r` is live; `socket` null or live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_set_socket(r: *const EeWonUdpRecvOp, socket: *mut c_void) {
    obj!(r).set_socket(socket as usize)
}
/// The socket (null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_socket(r: *const EeWonUdpRecvOp) -> *mut c_void {
    obj!(r).socket() as *mut c_void
}
/// The from address, a new value.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_address(r: *const EeWonUdpRecvOp) -> *mut EeWonIpAddr {
    boxed(obj!(r).address())
}
/// `SetFromAddr`.
///
/// # Safety
/// `r` is live, `addr` a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_set_address(r: *const EeWonUdpRecvOp, addr: *const EeWonIpAddr) {
    // SAFETY: the caller's value
    obj!(r).set_address(unsafe { &*addr })
}
/// The received datagram (a `ByteBuffer*` as `RefCount*`, null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_msg(r: *const EeWonUdpRecvOp) -> *mut c_void {
    obj!(r).msg() as *mut c_void
}
/// `SetMsg`.
///
/// # Safety
/// `r` is live; `bytes` null or live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_set_msg(r: *const EeWonUdpRecvOp, bytes: *mut c_void) {
    obj!(r).set_msg(bytes as usize)
}
/// `SetMessageVerifier`: the C++ check function and its context, kept as they are.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_set_verifier(
    r: *const EeWonUdpRecvOp,
    verifier: *mut c_void,
    context: *mut c_void,
) {
    obj!(r).set_verifier(verifier as usize, context as usize)
}
/// The check function (null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_verifier(r: *const EeWonUdpRecvOp) -> *mut c_void {
    obj!(r).verifier().0 as *mut c_void
}
/// The check's context.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_verifier_context(r: *const EeWonUdpRecvOp) -> *mut c_void {
    obj!(r).verifier().1 as *mut c_void
}
/// `QueueRecv`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_queue_recv(r: *const EeWonUdpRecvOp) {
    obj!(r).queue_recv()
}
/// `RecvCallback`: a datagram from `from` (`data` null for none).
///
/// # Safety
/// `r` is live; `data` null or `length` readable bytes; `from` a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_recv(
    r: *const EeWonUdpRecvOp,
    data: *const u8,
    length: usize,
    from: *const EeWonIpAddr,
) {
    let d = if data.is_null() || length == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's bytes
        unsafe { std::slice::from_raw_parts(data, length) }
    };
    // SAFETY: the caller's value
    obj!(r).recv(d, unsafe { &*from })
}
/// `CleanupHook`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_cleanup(r: *const EeWonUdpRecvOp) {
    obj!(r).cleanup()
}
/// `RunHook`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udprecvop_run(r: *const EeWonUdpRecvOp) {
    obj!(r).run()
}

/// A request op's object.
///
/// # Safety
/// `op` is the C++ op as `RefCount*`, `hooks` a static table.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_new(
    op: *mut c_void,
    hooks: *const EeWonUdpOpHooks,
) -> *mut EeWonUdpSendRecvOp {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonUdpSendRecvOp::new(op, unsafe { &*hooks })))
}
/// Frees the object, its references given back.
///
/// # Safety
/// `r` from `ee_won_udpsendrecvop_new` (or null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_free(r: *mut EeWonUdpSendRecvOp) {
    if !r.is_null() {
        // SAFETY: the caller's object
        drop(unsafe { Box::from_raw(r) });
    }
}
/// `SetUDPManager`.
///
/// # Safety
/// `r` is live; `manager` null or live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_set_manager(r: *const EeWonUdpSendRecvOp, manager: *mut c_void) {
    obj!(r).set_manager(manager as usize)
}
/// The manager (null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_manager(r: *const EeWonUdpSendRecvOp) -> *mut c_void {
    obj!(r).manager() as *mut c_void
}
/// The op's own socket (null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_socket(r: *const EeWonUdpSendRecvOp) -> *mut c_void {
    obj!(r).socket() as *mut c_void
}
/// `SetOutgoing`: the request datagram.
///
/// # Safety
/// `r` is live; `bytes` null or live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_set_outgoing(r: *const EeWonUdpSendRecvOp, bytes: *mut c_void) {
    obj!(r).set_outgoing(bytes as usize)
}
/// The request datagram (null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_outgoing(r: *const EeWonUdpSendRecvOp) -> *mut c_void {
    obj!(r).outgoing() as *mut c_void
}
/// The destination, a new value.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_address(r: *const EeWonUdpSendRecvOp) -> *mut EeWonIpAddr {
    boxed(obj!(r).address())
}
/// `SetDestAddr`.
///
/// # Safety
/// `r` is live, `addr` a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_set_address(r: *const EeWonUdpSendRecvOp, addr: *const EeWonIpAddr) {
    // SAFETY: the caller's value
    obj!(r).set_address(unsafe { &*addr })
}
/// The reply wait (null for none).
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_recv_op(r: *const EeWonUdpSendRecvOp) -> *mut c_void {
    obj!(r).recv_op() as *mut c_void
}
/// `SetNumRetransmits`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_set_retry_limit(r: *const EeWonUdpSendRecvOp, n: i32) {
    obj!(r).set_retry_limit(n)
}
/// The retry limit.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_retry_limit(r: *const EeWonUdpSendRecvOp) -> i32 {
    obj!(r).retry_limit()
}
/// The retransmits so far.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_attempts(r: *const EeWonUdpSendRecvOp) -> i32 {
    obj!(r).attempts()
}
/// `SetMsgTimeout`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_set_timeout(r: *const EeWonUdpSendRecvOp, t: u32) {
    obj!(r).set_timeout(t)
}
/// The reply wait's time-out.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_timeout(r: *const EeWonUdpSendRecvOp) -> u32 {
    obj!(r).timeout()
}
/// `SendRequest`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_send_request(r: *const EeWonUdpSendRecvOp) {
    obj!(r).send_request()
}
/// `RunHook`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_run(r: *const EeWonUdpSendRecvOp) {
    obj!(r).run()
}
/// `RecvCallback` (`wait` the ended op as `RefCount*`).
///
/// # Safety
/// `r` is live; `wait` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_recv_done(r: *const EeWonUdpSendRecvOp, wait: *mut c_void) {
    obj!(r).recv_done(wait as usize)
}
/// `CleanupHook`.
///
/// # Safety
/// `r` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_udpsendrecvop_cleanup(r: *const EeWonUdpSendRecvOp) {
    obj!(r).cleanup()
}

#[cfg(test)]
#[path = "udpops_tests.rs"]
mod tests;
