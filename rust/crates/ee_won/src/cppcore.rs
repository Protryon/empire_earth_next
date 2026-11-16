//! The C++ core every WON object's native calls go through (net round 25). The Rust objects of the queue socket, the
//! UDP manager and its receive and request ops (and, from the socket and op classes' rounds, more) were made with a
//! table of C++ calls per class, each answering on the C++ object: an op's status, kill or completion, a socket's
//! close, the classes' `new`s. Those calls are now Rust's ([`NATIVE_QUEUE`], [`NATIVE_UDP`], [`NATIVE_UDP_OP`]): an
//! op's call is answered on its [`EeWonAsyncOp`], a socket's on its [`EeWonAsyncSocket`], a queue's on its
//! [`EeWonQueueSocket`], and the completions the C++ classes made (`ParamCompletion` over the static callbacks,
//! `QueueOpFastCompletion`, the close timer's) are [`EeWonCompletion`]s. What is left C++'s is one table for the
//! whole library, [`EeWonCoreHooks`] (AsyncOp.cpp): references (`RefCount`'s count and virtual destructor), a C++
//! completion's virtual `Complete`, a C++ object's Rust object, the virtual `Duplicate` of a socket op and the
//! `new`s of the C++ classes (an op, a completion, a socket op, a queue socket, a UDP receive op), the receive op's
//! virtual message check (with the `ReadBuffer` exception), a socket's bind to the local host and a byte buffer's
//! bytes.
//!
//! The table is the same for every object; the first object made with it binds it ([`bind`]) for the adapters,
//! which are plain C functions. The completion bodies hold what the deleted classes' by-value `AsyncOpPtr` and
//! `SmartPtr<RefCount>` parameters held for the call, so the reference counts inside a callback are the original's
//! (`golden_core.txt`).

use std::ffi::{c_long, c_void};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicPtr, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};

use crate::buffer::ee_won_bytes_copy;
use crate::buffer::{ee_won_bytes_data, ee_won_bytes_length, EeWonByteBuffer};
use crate::ipobj::{ee_won_ipaddr_free, EeWonIpAddr};
use crate::opobj::{EeWonAsyncOp, EeWonCompletion, HeldRef};
use crate::qsock::{EeWonQueueSocket, EeWonQueueSocketHooks};
use crate::servconn::{
    ee_won_servconn_connect_callback, ee_won_servconn_keep_alive_callback, EeWonServerConn, EeWonServerConnHooks,
};
use crate::sockobj::{EeWonAsyncSocket, EeWonAsyncSocketHooks};
use crate::sockop::{EeWonSocketOp, EeWonSocketOpBaseHooks};
use crate::sockops::{ee_won_socketop_address, ee_won_socketop_received};
use crate::sockthread::EeWonSocketOpHooks;
use crate::status::*;
use crate::thread::{EeWonContainerHooks, EeWonTimerHooks};
use crate::udpmgr::EeWonUdpManager;
use crate::udpmgr::{ee_won_udpmanager_queue_op, ee_won_udpmanager_recv, ee_won_udpmanager_send, EeWonUdpHooks};
use crate::udpops::{
    ee_won_udprecvop_address, ee_won_udprecvop_recv, ee_won_udprecvop_set_address, ee_won_udprecvop_set_msg,
    ee_won_udprecvop_set_verifier, ee_won_udpsendrecvop_recv_done, EeWonUdpOpHooks, EeWonUdpRecvOp, EeWonUdpSendRecvOp,
};

/// A C++ call on one object.
pub type EeWonCoreFn = Option<unsafe extern "C" fn(object: *mut c_void)>;

/// Which Rust object of a C++ object [`EeWonCoreHooks::object_of`] returns.
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonCoreObject {
    /// an `AsyncOp*`'s [`EeWonAsyncOp`]
    Op = 0,
    /// a `SocketOp*`'s [`EeWonSocketOp`]
    SocketOp = 1,
    /// an `AsyncSocket*`'s [`EeWonAsyncSocket`]
    Socket = 2,
    /// a `QueueSocket*`'s [`EeWonQueueSocket`]
    Queue = 3,
    /// a `UDPManager*`'s [`EeWonUdpManager`]
    UdpManager = 4,
    /// a `UDPManagerRecvOp*`'s [`EeWonUdpRecvOp`]
    UdpRecv = 5,
    /// a `UDPManagerSendRecvOp*`'s [`EeWonUdpSendRecvOp`]
    UdpSendRecv = 6,
    /// a `ServerConnection*`'s [`EeWonServerConn`]
    ServerConn = 7,
    /// a `RoutingOp*`'s [`EeWonRouteOp`](crate::routeop::EeWonRouteOp)
    RouteOp = 8,
    /// a `RoutingConnection*`'s [`EeWonRouteConn`](crate::routeconn::EeWonRouteConn)
    RouteConn = 9,
    /// an `AuthContext*`'s [`EeWonAuthCtx`](crate::authobj::EeWonAuthCtx)
    AuthCtx = 10,
    /// a `RustServerOp*`'s [`EeWonHostOp`](crate::hostop::EeWonHostOp)
    HostOp = 11,
    /// an `HTTPConnection*`'s [`EeWonHttpConn`](crate::httpengine::EeWonHttpConn)
    HttpConn = 12,
    /// an `HTTPConnection*`'s or `HTTPSession*`'s lock (its `CriticalSection`'s platform mutex)
    Lock = 13,
    /// an `HTTPSession*`'s [`EeWonHttpSession`](crate::httpengine::EeWonHttpSession)
    HttpSession = 14,
    /// a `ServerContext*`'s [`ServerList`](crate::servctx::ServerList)
    Context = 15,
}

/// The socket op classes [`EeWonCoreHooks::new_socket_op`] makes.
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonCoreSocketOp {
    /// `new CloseOp(socket)`
    Close = 0,
    /// the queue socket's queued close: a socket op without a socket whose `Continue` succeeds, not run at once
    QueueClose = 1,
    /// `new RecvBytesFromOp(size, socket)`
    RecvFrom = 2,
    /// `new SendBytesToOp(bytes, addr, socket)`
    SendTo = 3,
    /// `new RecvMsgOp(socket)`
    RecvMsg = 4,
    /// `new SendMsgOp(bytes, socket)`
    SendMsg = 5,
    /// `new ConnectOp(addr, socket)`
    Connect = 6,
    /// `new SendBytesOp(bytes, socket)`
    SendBytes = 7,
    /// `new RecvBytesOp(size, socket)`
    RecvBytes = 8,
    /// `new RecvCRMsgOp(socket)`
    RecvCrMsg = 9,
}

/// The C++ calls of the whole library (AsyncOp.cpp's one table). Every object crosses as its `RefCount*`; a `new`
/// object holds no reference yet.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonCoreHooks {
    /// `object->AddRef()`, `object->Release()` (any `RefCount*`)
    pub ref_add: EeWonCoreFn,
    pub ref_release: EeWonCoreFn,
    /// `completion->Complete(op)` (the C++ makes the `AsyncOpPtr`)
    pub completion_call: Option<unsafe extern "C" fn(completion: *mut c_void, op: *mut c_void)>,
    /// the object's Rust object of that kind (null for a null object or one not of the kind's class)
    pub object_of: Option<unsafe extern "C" fn(object: *mut c_void, kind: EeWonCoreObject) -> *mut c_void>,
    /// a socket op's virtual `Duplicate()` (a new op, or null)
    pub socketop_duplicate: Option<unsafe extern "C" fn(op: *mut c_void) -> *mut c_void>,
    /// `new AsyncOp`
    pub new_op: Option<unsafe extern "C" fn() -> *mut c_void>,
    /// `new RustCompletion(body)`: its `Complete` runs the body, its deletion frees it
    pub new_completion: Option<unsafe extern "C" fn(body: *mut EeWonCompletion) -> *mut c_void>,
    /// a new socket op of the class (`socket`, `bytes` and `addr` may be null where the class takes none)
    pub new_socket_op: Option<
        unsafe extern "C" fn(
            kind: EeWonCoreSocketOp,
            socket: *mut c_void,
            bytes: *mut c_void,
            addr: *const EeWonIpAddr,
            size: u32,
        ) -> *mut c_void,
    >,
    /// `new QueueSocket(type)`, or `new BlockingSocket(type)` (the ops' own sockets)
    pub new_queue_socket: Option<unsafe extern "C" fn(kind: i32, blocking: bool) -> *mut c_void>,
    /// `new UDPManagerRecvOp` (from `addr`; any sender for null)
    pub new_udp_recv_op: Option<unsafe extern "C" fn(addr: *const EeWonIpAddr) -> *mut c_void>,
    /// a receive op's virtual `CheckMessage` over the datagram (`*threw` for a `ReadBufferException`)
    pub udp_check:
        Option<unsafe extern "C" fn(op: *mut c_void, data: *const u8, length: usize, threw: *mut bool) -> c_long>,
    /// `UDPManagerSendRecvOp::StaticCheckResponse`, a reply wait's message check (kept as the C++ function's address)
    pub udp_response_check: *mut c_void,
    /// a socket's `Fn2B0D90(port, reuse)`: bound to the local host
    pub socket_bind_local: Option<unsafe extern "C" fn(socket: *mut c_void, port: u16, reuse: bool) -> c_long>,
    /// a `ByteBuffer`'s bytes (its `mBytes`)
    pub bytes_of: Option<unsafe extern "C" fn(buffer: *mut c_void) -> *const EeWonByteBuffer>,
    /// a socket op's virtual `Continue()`
    pub socketop_continue: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    /// a socket op's virtual `Start()`
    pub socketop_start: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    /// `WONAPICore::AddToSocketThread(op)`
    pub socket_thread_add: EeWonCoreFn,
    /// a new `ByteBuffer` adopting Rust's bytes, a reference taken
    pub bytes_adopt: Option<unsafe extern "C" fn(bytes: *mut EeWonByteBuffer) -> *mut c_void>,
    /// `socket->TransformSendMsg(*message)` / `TransformRecvMsg`: `*message` holds a reference, replaced by the
    /// result's
    pub transform: Option<unsafe extern "C" fn(socket: *mut c_void, message: *mut *mut c_void, send: bool) -> c_long>,
    /// a progress chunk: a `RecvChunkOp` of the op with the completion, run in the op's mode
    pub progress: Option<unsafe extern "C" fn(op: *mut c_void, completion: *mut c_void)>,
    /// `socket->Accept(out)` (the virtual `Duplicate` makes the socket): on success `*out` with a reference
    pub accept: Option<unsafe extern "C" fn(socket: *mut c_void, out: *mut *mut c_void) -> c_long>,
    /// the socket's virtual `OpenHook` (0), `CloseHook` (1) or `KillHook` (2)
    pub socket_hook: Option<unsafe extern "C" fn(socket: *mut c_void, which: i32)>,
    /// `new RoutingOp(conn, type)` (a `RoutingDetectFirewallOp` for `RoutingOp_DetectFirewall`)
    pub new_route_op: Option<unsafe extern "C" fn(conn: *mut c_void, ty: i32) -> *mut c_void>,
    /// a routing connection's virtual `RoutingOpCompleteHook(op)` (`which` 0: null returned) or
    /// `GetUnsolicitedDerivedOp(type)` (1: the new op with one reference for the caller, or null)
    pub route_virtual:
        Option<unsafe extern "C" fn(conn: *mut c_void, which: i32, op: *mut c_void, message_type: u8) -> *mut c_void>,
    /// `new HTTPConnection(NULL)`
    pub new_http_conn: Option<unsafe extern "C" fn() -> *mut c_void>,
}

/// `new RustServerOp(op)`: the C++ op of a Rust op on the op base (it takes `op`; no reference yet). ServerOp.cpp
/// sets it at start-up ([`ee_won_core_set_new_host_op`]): a program without the class has none.
pub type EeWonNewHostOp = Option<unsafe extern "C" fn(op: *mut crate::hostop::EeWonHostOp) -> *mut c_void>;
static NEW_HOST_OP: Mutex<EeWonNewHostOp> = Mutex::new(None);

/// Sets the maker of a Rust op's C++ op.
#[no_mangle]
pub extern "C" fn ee_won_core_set_new_host_op(maker: EeWonNewHostOp) {
    *NEW_HOST_OP.lock().unwrap_or_else(|e| e.into_inner()) = maker;
}

/// A new C++ op for the Rust op, which it takes (null without a maker: the Rust op is freed).
pub(crate) fn new_host_op(op: *mut crate::hostop::EeWonHostOp) -> *mut c_void {
    let maker = *NEW_HOST_OP.lock().unwrap_or_else(|e| e.into_inner());
    match maker {
        // SAFETY: the maker's contract: it takes the op
        Some(h) => unsafe { h(op) },
        None => {
            // SAFETY: the op's box, not taken
            unsafe { crate::hostop::ee_won_hostop_free(op) };
            null_mut()
        }
    }
}

// SAFETY: a static table of functions and one function address
unsafe impl Sync for EeWonCoreHooks {}
// SAFETY: as Sync
unsafe impl Send for EeWonCoreHooks {}

static CORE: AtomicPtr<EeWonCoreHooks> = AtomicPtr::new(null_mut());

/// Binds the library's table (the same one every time; a null leaves the binding as it is).
pub fn bind(core: *const EeWonCoreHooks) {
    if !core.is_null() {
        CORE.store(core.cast_mut(), Ordering::Release);
    }
}

pub(crate) fn core() -> Option<&'static EeWonCoreHooks> {
    // SAFETY: a bound table lives for the program
    unsafe { CORE.load(Ordering::Acquire).as_ref() }
}

/// The Rust object of a C++ object (null for none).
pub(crate) fn object_of(object: *mut c_void, kind: EeWonCoreObject) -> *mut c_void {
    if object.is_null() {
        return null_mut();
    }
    // SAFETY: the table's contract: a live C++ object of the kind
    core().and_then(|c| c.object_of).map_or(null_mut(), |h| unsafe { h(object, kind) })
}

pub(crate) fn op<'a>(o: *mut c_void) -> Option<&'a EeWonAsyncOp> {
    // SAFETY: an op's Rust object lives as long as the op, which the caller holds
    unsafe { (object_of(o, EeWonCoreObject::Op) as *const EeWonAsyncOp).as_ref() }
}
pub(crate) fn socket<'a>(s: *mut c_void) -> Option<&'a EeWonAsyncSocket> {
    // SAFETY: as `op`
    unsafe { (object_of(s, EeWonCoreObject::Socket) as *const EeWonAsyncSocket).as_ref() }
}
pub(crate) fn queue<'a>(s: *mut c_void) -> Option<&'a EeWonQueueSocket> {
    // SAFETY: as `op`
    unsafe { (object_of(s, EeWonCoreObject::Queue) as *const EeWonQueueSocket).as_ref() }
}
fn socket_op(o: *mut c_void) -> *const EeWonSocketOp {
    object_of(o, EeWonCoreObject::SocketOp) as *const EeWonSocketOp
}

pub(crate) fn ref_add(o: *mut c_void) {
    if !o.is_null() {
        if let Some(h) = core().and_then(|c| c.ref_add) {
            // SAFETY: the table's contract: a live object
            unsafe { h(o) };
        }
    }
}
pub(crate) fn ref_release(o: *mut c_void) {
    if !o.is_null() {
        if let Some(h) = core().and_then(|c| c.ref_release) {
            // SAFETY: the table's contract: a reference the caller held
            unsafe { h(o) };
        }
    }
}

/// A reference held for a scope (a by-value `SmartPtr`); none for null.
pub(crate) struct Hold(Option<HeldRef>);
pub(crate) fn hold(o: *mut c_void) -> Hold {
    if o.is_null() {
        return Hold(None);
    }
    let c = core();
    Hold(Some(HeldRef::new(o, c.and_then(|c| c.ref_add), c.and_then(|c| c.ref_release))))
}
impl Hold {
    pub(crate) fn get(&self) -> *mut c_void {
        self.0.as_ref().map_or(null_mut(), HeldRef::get)
    }
}

/// A C++ completion running `call` (null without a table).
pub(crate) fn completion(call: impl Fn(*mut c_void) + Send + Sync + 'static) -> *mut c_void {
    EeWonCompletion::make_with(core().and_then(|c| c.new_completion), call)
}

pub(crate) fn new_socket_op(
    kind: EeWonCoreSocketOp,
    socket: *mut c_void,
    bytes: *mut c_void,
    addr: *const EeWonIpAddr,
    size: u32,
) -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.new_socket_op).map_or(null_mut(), |h| unsafe { h(kind, socket, bytes, addr, size) })
}
fn new_queue_socket() -> *mut c_void {
    new_socket(1, false)
}
/// `new QueueSocket(kind)` or `new BlockingSocket(kind)` (no reference yet)
pub(crate) fn new_socket(kind: i32, blocking: bool) -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.new_queue_socket).map_or(null_mut(), |h| unsafe { h(kind, blocking) })
}
/// `new AsyncOp` (no reference yet)
pub(crate) fn new_plain_op() -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.new_op).map_or(null_mut(), |h| unsafe { h() })
}
/// A new `ByteBuffer` adopting the bytes, with one reference.
///
/// # Safety
/// `b` is a new buffer the C++ takes.
pub(crate) unsafe fn bytes_adopt(b: *mut EeWonByteBuffer) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { n_bytes_adopt(b) }
}
fn new_udp_recv_op(addr: *const EeWonIpAddr) -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.new_udp_recv_op).map_or(null_mut(), |h| unsafe { h(addr) })
}
fn bind_local(s: *mut c_void, port: u16) -> c_long {
    // SAFETY: the table's contract
    core().and_then(|c| c.socket_bind_local).map_or(WS_NONE as c_long, |h| unsafe { h(s, port, false) })
}

// ---- an op's, a completion's and a socket's calls

pub(crate) unsafe extern "C" fn n_ref_add(o: *mut c_void) {
    ref_add(o)
}
pub(crate) unsafe extern "C" fn n_ref_release(o: *mut c_void) {
    ref_release(o)
}
pub(crate) unsafe extern "C" fn n_completion_call(c: *mut c_void, o: *mut c_void) {
    if let Some(h) = core().and_then(|c| c.completion_call) {
        // SAFETY: the table's contract: a live completion and op
        unsafe { h(c, o) };
    }
}
pub(crate) unsafe extern "C" fn n_op_status(o: *mut c_void) -> c_long {
    op(o).map_or(WS_NONE, EeWonAsyncOp::status) as c_long
}
pub(crate) unsafe extern "C" fn n_op_killed(o: *mut c_void) -> bool {
    op(o).is_some_and(EeWonAsyncOp::killed)
}
unsafe extern "C" fn n_op_timeout(o: *mut c_void) -> u32 {
    op(o).map_or(0, EeWonAsyncOp::timeout)
}
/// `static_cast<long>(op->GetTimeout())`: the unsigned timeout as the platform's `long`
unsafe extern "C" fn n_op_timeout_long(o: *mut c_void) -> c_long {
    op(o).map_or(0, EeWonAsyncOp::timeout) as c_long
}
unsafe extern "C" fn n_op_mode(o: *mut c_void) -> i32 {
    op(o).map_or(0, EeWonAsyncOp::run_mode)
}
pub(crate) unsafe extern "C" fn n_op_completion(o: *mut c_void) -> *mut c_void {
    op(o).map_or(null_mut(), EeWonAsyncOp::completion)
}
pub(crate) unsafe extern "C" fn n_op_set_completion(o: *mut c_void, c: *mut c_void) {
    if let Some(x) = op(o) {
        // SAFETY: a live completion or null; the op takes its own reference
        unsafe { x.set_completion(c) };
    }
}
unsafe extern "C" fn n_op_set_timeout(o: *mut c_void, t: u32) {
    if let Some(x) = op(o) {
        x.set_timeout(t);
    }
}
/// `RunAsync(t)`: `Run(OP_MODE_ASYNC, t)` with the `long` taken as the unsigned timeout
pub(crate) unsafe extern "C" fn n_op_run_async(o: *mut c_void, t: c_long) {
    if let Some(x) = op(o) {
        x.run(1, t as u32);
    }
}
pub(crate) unsafe extern "C" fn n_op_kill(o: *mut c_void) {
    if let Some(x) = op(o) {
        x.kill(true);
    }
}
pub(crate) unsafe extern "C" fn n_op_finish(o: *mut c_void, s: c_long) {
    if let Some(x) = op(o) {
        x.finish(s as Status);
    }
}
pub(crate) unsafe extern "C" fn n_op_force_finish(o: *mut c_void, s: c_long) {
    if let Some(x) = op(o) {
        x.force_finish(s as Status);
    }
}
unsafe extern "C" fn n_op_time_left(o: *mut c_void) -> u32 {
    op(o).map_or(0, EeWonAsyncOp::time_left)
}
unsafe extern "C" fn n_op_set_socket(o: *mut c_void, s: *mut c_void) {
    let so = socket_op(o);
    if !so.is_null() {
        // SAFETY: the op's live Rust object; the socket is live or null
        unsafe { (*so).set_socket(s) };
    }
}
unsafe extern "C" fn n_op_duplicate(o: *mut c_void) -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.socketop_duplicate).map_or(null_mut(), |h| unsafe { h(o) })
}
unsafe extern "C" fn n_socket_close(s: *mut c_void) {
    if let Some(x) = socket(s) {
        x.close();
    }
}
unsafe extern "C" fn n_socket_kill(s: *mut c_void) {
    if let Some(x) = socket(s) {
        x.kill();
    }
}
unsafe extern "C" fn n_socket_valid(s: *mut c_void) -> bool {
    socket(s).is_some_and(EeWonAsyncSocket::is_valid)
}
unsafe extern "C" fn n_socket_datagram(s: *mut c_void) -> bool {
    socket(s).is_some_and(|x| x.kind() == 1)
}
unsafe extern "C" fn n_socket_set_broadcast(s: *mut c_void, on: bool) {
    if let Some(x) = socket(s) {
        x.set_broadcast(on);
    }
}

// ---- the queue socket's completions and objects

/// `QueueOpFastCompletion`s alive: (the C++ completion, its original completion)
static FAST: Mutex<Vec<(usize, usize)>> = Mutex::new(Vec::new());

struct FastEntry {
    key: AtomicUsize,
    _original: Hold,
}
impl Drop for FastEntry {
    fn drop(&mut self) {
        let key = self.key.load(Ordering::Relaxed);
        if let Ok(mut f) = FAST.lock() {
            f.retain(|e| e.0 != key);
        }
    }
}

/// `StaticOpCallback` (`fast`: through the fast completion) or `StaticCloseTimerCallback`, holding the socket
fn queue_completion(s: *mut c_void, timer: bool, original: *mut c_void) -> *mut c_void {
    let socket = Arc::new(hold(s));
    let call = {
        let socket = socket.clone();
        move |o: *mut c_void| {
            // the callback's by-value parameters
            let _param = hold(socket.get());
            let _op = hold(o);
            if let Some(q) = queue(socket.get()) {
                if timer {
                    q.close_timer_callback(o);
                } else {
                    q.op_callback(o);
                }
            }
        }
    };
    if original.is_null() {
        return completion(call);
    }
    let entry = Arc::new(FastEntry { key: AtomicUsize::new(0), _original: hold(original) });
    let kept = entry.clone();
    let c = completion(move |o| {
        let _entry = &kept;
        call(o)
    });
    entry.key.store(c as usize, Ordering::Relaxed);
    if let Ok(mut f) = FAST.lock() {
        f.push((c as usize, original as usize));
    }
    c
}

unsafe extern "C" fn n_fast_original(c: *mut c_void) -> *mut c_void {
    FAST.lock().ok().and_then(|f| f.iter().find(|e| e.0 == c as usize).map(|e| e.1)).unwrap_or(0) as *mut c_void
}
unsafe extern "C" fn n_new_op_completion(s: *mut c_void) -> *mut c_void {
    queue_completion(s, false, null_mut())
}
unsafe extern "C" fn n_new_fast_completion(s: *mut c_void, original: *mut c_void) -> *mut c_void {
    if original.is_null() {
        // a fast completion without an original (no completion and no default): found, with none to call
        let c = queue_completion(s, false, null_mut());
        if let Ok(mut f) = FAST.lock() {
            f.push((c as usize, 0));
        }
        return c;
    }
    queue_completion(s, false, original)
}
unsafe extern "C" fn n_new_close_timer(s: *mut c_void) -> *mut c_void {
    // SAFETY: the table's contract
    let t = core().and_then(|c| c.new_op).map_or(null_mut(), |h| unsafe { h() });
    let c = queue_completion(s, true, null_mut());
    // SAFETY: the new op and completion (the op takes its reference)
    unsafe { n_op_set_completion(t, c) };
    t
}
unsafe extern "C" fn n_new_queue_close_op(_s: *mut c_void) -> *mut c_void {
    new_socket_op(EeWonCoreSocketOp::QueueClose, null_mut(), null_mut(), std::ptr::null(), 0)
}
unsafe extern "C" fn n_new_close_op(s: *mut c_void) -> *mut c_void {
    new_socket_op(EeWonCoreSocketOp::Close, s, null_mut(), std::ptr::null(), 0)
}

/// The queue socket's calls, answered in Rust.
pub static NATIVE_QUEUE: EeWonQueueSocketHooks = EeWonQueueSocketHooks {
    completion_add_ref: Some(n_ref_add),
    completion_release: Some(n_ref_release),
    completion_call: Some(n_completion_call),
    op_add_ref: Some(n_ref_add),
    op_release: Some(n_ref_release),
    op_status: Some(n_op_status),
    op_killed: Some(n_op_killed),
    op_timeout: Some(n_op_timeout),
    op_completion: Some(n_op_completion),
    op_set_completion: Some(n_op_set_completion),
    op_set_timeout: Some(n_op_set_timeout),
    op_run_async: Some(n_op_run_async),
    op_kill: Some(n_op_kill),
    op_force_finish: Some(n_op_force_finish),
    op_set_socket: Some(n_op_set_socket),
    op_duplicate: Some(n_op_duplicate),
    fast_original: Some(n_fast_original),
    new_op_completion: Some(n_new_op_completion),
    new_fast_completion: Some(n_new_fast_completion),
    new_close_timer: Some(n_new_close_timer),
    new_queue_close_op: Some(n_new_queue_close_op),
    new_close_op: Some(n_new_close_op),
    socket_close: Some(n_socket_close),
    socket_valid: Some(n_socket_valid),
    socket_datagram: Some(n_socket_datagram),
};

// ---- the UDP manager and its ops

/// A received datagram's bytes (a reference held, as `ByteBufferPtr aBuffer = inOp->GetBytes()`) and sender (an owned
/// value), from a `RecvBytesFromOp`.
struct Datagram {
    bytes: Hold,
    from: *mut EeWonIpAddr,
}
impl Datagram {
    fn of(o: *mut c_void) -> Datagram {
        let so = socket_op(o);
        if so.is_null() {
            return Datagram { bytes: Hold(None), from: null_mut() };
        }
        // SAFETY: the op's live Rust object
        let bytes = hold(unsafe { ee_won_socketop_received(so) });
        // SAFETY: as above; the value is ours
        let from = unsafe { ee_won_socketop_address(so) };
        Datagram { bytes, from }
    }
    /// the bytes' data and length (null and 0 without a buffer)
    fn data(&self) -> (*const u8, usize) {
        let b = self.bytes.get();
        let bb = core().and_then(|c| c.bytes_of).map_or(std::ptr::null(), |h| {
            if b.is_null() {
                std::ptr::null()
            } else {
                // SAFETY: the table's contract: a live buffer
                unsafe { h(b) }
            }
        });
        if bb.is_null() {
            return (std::ptr::null(), 0);
        }
        // SAFETY: the buffer's live bytes
        unsafe { (ee_won_bytes_data(bb), ee_won_bytes_length(bb) as usize) }
    }
}
impl Drop for Datagram {
    fn drop(&mut self) {
        if !self.from.is_null() {
            // SAFETY: our value
            unsafe { ee_won_ipaddr_free(self.from) };
        }
    }
}

fn succeeded(o: *mut c_void) -> bool {
    op(o).is_some_and(|x| x.status() == WS_SUCCESS && !x.killed())
}

/// `UDPManager::StaticRecvCallback` (the manager's repeat receive): a successful datagram to the manager
fn manager_completion(m: *mut c_void) -> *mut c_void {
    let manager = hold(m);
    completion(move |o| {
        let _param = hold(manager.get());
        let _op = hold(o);
        if !succeeded(o) {
            return;
        }
        let d = Datagram::of(o);
        let (data, length) = d.data();
        let mo = object_of(manager.get(), EeWonCoreObject::UdpManager) as *const EeWonUdpManager;
        if !mo.is_null() {
            // SAFETY: the manager's live Rust object; the datagram's bytes and sender live for the call
            unsafe { ee_won_udpmanager_recv(mo, d.bytes.get(), data, length, d.from) };
        }
    })
}
/// `UDPManagerRecvOp::StaticRecvCallback` (a receive op's own receive): a successful datagram to the op
fn recv_op_completion(r: *mut c_void) -> *mut c_void {
    let recv = hold(r);
    completion(move |o| {
        let _param = hold(recv.get());
        let _op = hold(o);
        if !succeeded(o) {
            return;
        }
        let d = Datagram::of(o);
        let (data, length) = d.data();
        let ro = object_of(recv.get(), EeWonCoreObject::UdpRecv) as *const EeWonUdpRecvOp;
        if !ro.is_null() {
            // SAFETY: the op's live Rust object; the datagram lives for the call
            unsafe { ee_won_udprecvop_recv(ro, data, length, d.from) };
        }
    })
}
/// `UDPManagerSendRecvOp::StaticRecvCallback` (a request's reply wait): the wait's end to the request
fn wait_completion(r: *mut c_void) -> *mut c_void {
    let request = hold(r);
    completion(move |o| {
        let _param = hold(request.get());
        let _op = hold(o);
        let so = object_of(request.get(), EeWonCoreObject::UdpSendRecv) as *const EeWonUdpSendRecvOp;
        if !so.is_null() {
            // SAFETY: the request's live Rust object
            unsafe { ee_won_udpsendrecvop_recv_done(so, o) };
        }
    })
}

fn udp_recv(o: *mut c_void) -> *const EeWonUdpRecvOp {
    object_of(o, EeWonCoreObject::UdpRecv) as *const EeWonUdpRecvOp
}

unsafe extern "C" fn n_recv_address(o: *mut c_void) -> *mut EeWonIpAddr {
    let r = udp_recv(o);
    // SAFETY: the op's live Rust object (a new value)
    if r.is_null() {
        null_mut()
    } else {
        unsafe { ee_won_udprecvop_address(r) }
    }
}
unsafe extern "C" fn n_recv_set_address(o: *mut c_void, a: *const EeWonIpAddr) {
    let r = udp_recv(o);
    if !r.is_null() {
        // SAFETY: the op's live Rust object; the value is copied
        unsafe { ee_won_udprecvop_set_address(r, a) };
    }
}
unsafe extern "C" fn n_recv_set_msg(o: *mut c_void, b: *mut c_void) {
    let r = udp_recv(o);
    if !r.is_null() {
        // SAFETY: the op's live Rust object; the buffer live or null
        unsafe { ee_won_udprecvop_set_msg(r, b) };
    }
}
unsafe extern "C" fn n_recv_check(o: *mut c_void, data: *const u8, length: usize, threw: *mut bool) -> c_long {
    // SAFETY: the table's contract
    core().and_then(|c| c.udp_check).map_or(0, |h| unsafe { h(o, data, length, threw) })
}
unsafe extern "C" fn n_new_recv_op(a: *const EeWonIpAddr) -> *mut c_void {
    let o = new_udp_recv_op(a);
    ref_add(o);
    o
}
unsafe extern "C" fn n_udp_socket_new() -> *mut c_void {
    let s = new_queue_socket();
    ref_add(s);
    s
}
unsafe extern "C" fn n_udp_socket_bind(s: *mut c_void, port: i32) -> c_long {
    bind_local(s, port as u16)
}
unsafe extern "C" fn n_udp_socket_repeat(s: *mut c_void, m: *mut c_void, size: i32) {
    let Some(q) = queue(s) else { return };
    q.set_repeat_completion(manager_completion(m));
    q.set_repeat_op(new_socket_op(
        EeWonCoreSocketOp::RecvFrom,
        null_mut(),
        null_mut(),
        std::ptr::null(),
        u32::from(size as u16),
    ));
}
unsafe extern "C" fn n_udp_socket_send(s: *mut c_void, b: *mut c_void, a: *const EeWonIpAddr) {
    let Some(q) = queue(s) else { return };
    q.queue_op(new_socket_op(EeWonCoreSocketOp::SendTo, null_mut(), b, a, 0), -1);
}

/// The UDP manager's calls, answered in Rust.
pub static NATIVE_UDP: EeWonUdpHooks = EeWonUdpHooks {
    ref_add: Some(n_ref_add),
    ref_release: Some(n_ref_release),
    op_status: Some(n_op_status),
    op_completion: Some(n_op_completion),
    op_set_completion: Some(n_op_set_completion),
    op_run_async: Some(n_op_run_async),
    op_kill: Some(n_op_kill),
    op_finish: Some(n_op_finish),
    op_force_finish: Some(n_op_force_finish),
    op_time_left: Some(n_op_time_left),
    recv_address: Some(n_recv_address),
    recv_set_address: Some(n_recv_set_address),
    recv_set_msg: Some(n_recv_set_msg),
    recv_check: Some(n_recv_check),
    new_recv_op: Some(n_new_recv_op),
    socket_new: Some(n_udp_socket_new),
    socket_kill: Some(n_socket_kill),
    socket_set_broadcast: Some(n_socket_set_broadcast),
    socket_bind: Some(n_udp_socket_bind),
    socket_repeat: Some(n_udp_socket_repeat),
    socket_send: Some(n_udp_socket_send),
};

unsafe extern "C" fn n_udp_socket_new_bound() -> *mut c_void {
    let s = new_queue_socket();
    ref_add(s);
    bind_local(s, 0);
    s
}
unsafe extern "C" fn n_udp_socket_queue_recv(s: *mut c_void, r: *mut c_void) {
    let recv = hold(new_socket_op(EeWonCoreSocketOp::RecvFrom, null_mut(), null_mut(), std::ptr::null(), 1024));
    // SAFETY: the new op and completion
    unsafe { n_op_set_completion(recv.get(), recv_op_completion(r)) };
    if let Some(q) = queue(s) {
        q.queue_op(recv.get(), -1);
    }
}
unsafe extern "C" fn n_udp_manager_queue(m: *mut c_void, o: *mut c_void, t: u32) {
    let mo = object_of(m, EeWonCoreObject::UdpManager) as *const EeWonUdpManager;
    if !mo.is_null() {
        // SAFETY: the manager's live Rust object
        unsafe { ee_won_udpmanager_queue_op(mo, o, t) };
    }
}
unsafe extern "C" fn n_udp_manager_send(m: *mut c_void, b: *mut c_void, a: *const EeWonIpAddr) {
    let mo = object_of(m, EeWonCoreObject::UdpManager) as *const EeWonUdpManager;
    if !mo.is_null() {
        // SAFETY: the manager's live Rust object; the value is copied
        unsafe { ee_won_udpmanager_send(mo, b, a) };
    }
}
unsafe extern "C" fn n_udp_new_wait(r: *mut c_void, a: *const EeWonIpAddr) -> *mut c_void {
    let w = new_udp_recv_op(a);
    ref_add(w);
    // SAFETY: the new op and completion
    unsafe { n_op_set_completion(w, wait_completion(r)) };
    let wo = udp_recv(w);
    if !wo.is_null() {
        let check = core().map_or(null_mut(), |c| c.udp_response_check);
        // SAFETY: the new op's Rust object
        unsafe { ee_won_udprecvop_set_verifier(wo, check, r) };
    }
    w
}
unsafe extern "C" fn n_udp_recv_object(o: *mut c_void) -> *const EeWonUdpRecvOp {
    udp_recv(o)
}

/// The UDP receive and request ops' calls, answered in Rust.
pub static NATIVE_UDP_OP: EeWonUdpOpHooks = EeWonUdpOpHooks {
    ref_add: Some(n_ref_add),
    ref_release: Some(n_ref_release),
    op_mode: Some(n_op_mode),
    op_timeout: Some(n_op_timeout_long),
    op_status: Some(n_op_status),
    op_finish: Some(n_op_finish),
    op_run_async: Some(n_op_run_async),
    op_kill: Some(n_op_kill),
    recv_check: Some(n_recv_check),
    socket_new_bound: Some(n_udp_socket_new_bound),
    socket_kill: Some(n_socket_kill),
    socket_queue_recv: Some(n_udp_socket_queue_recv),
    socket_send: Some(n_udp_socket_send),
    manager_queue: Some(n_udp_manager_queue),
    manager_send: Some(n_udp_manager_send),
    new_wait: Some(n_udp_new_wait),
    recv_object: Some(n_udp_recv_object),
};

// ---- the timer thread, the op container and the socket thread

unsafe extern "C" fn n_timer_due(_context: *mut c_void, o: *mut c_void) {
    // SAFETY: the timer's op, its reference held until the release
    unsafe {
        n_op_finish(o, WS_TIMED_OUT as c_long);
        n_ref_release(o);
    }
}
unsafe extern "C" fn n_timer_purged(_context: *mut c_void, o: *mut c_void) {
    // SAFETY: as `n_timer_due`
    unsafe {
        n_op_kill(o);
        n_ref_release(o);
    }
}
/// The timer thread's calls, answered in Rust: a due op times out, a purged one is killed, then the timer's reference
/// goes.
pub fn native_timer() -> EeWonTimerHooks {
    EeWonTimerHooks { due: Some(n_timer_due), purged: Some(n_timer_purged), context: null_mut() }
}

unsafe extern "C" fn n_c_add_ref(_context: *mut c_void, o: *mut c_void) {
    ref_add(o)
}
unsafe extern "C" fn n_c_release(_context: *mut c_void, o: *mut c_void) {
    ref_release(o)
}
unsafe extern "C" fn n_c_is_pending(_context: *mut c_void, o: *mut c_void) -> bool {
    op(o).is_some_and(|x| x.status() == WS_PENDING)
}
unsafe extern "C" fn n_c_completion_pending(_context: *mut c_void, o: *mut c_void) -> bool {
    op(o).is_some_and(EeWonAsyncOp::completion_pending)
}
unsafe extern "C" fn n_c_start_run_async(_context: *mut c_void, o: *mut c_void) {
    if let Some(x) = op(o) {
        x.start_run_async();
    }
}
unsafe extern "C" fn n_c_complete(_context: *mut c_void, o: *mut c_void) {
    if let Some(x) = op(o) {
        x.complete();
    }
}
unsafe extern "C" fn n_c_kill(_context: *mut c_void, o: *mut c_void) {
    if let Some(x) = op(o) {
        x.kill(true);
    }
}
/// The op container's calls, answered in Rust.
pub fn native_container() -> EeWonContainerHooks {
    EeWonContainerHooks {
        add_ref: Some(n_c_add_ref),
        release: Some(n_c_release),
        is_pending: Some(n_c_is_pending),
        completion_pending: Some(n_c_completion_pending),
        start_run_async: Some(n_c_start_run_async),
        complete: Some(n_c_complete),
        kill: Some(n_c_kill),
        context: null_mut(),
    }
}

unsafe extern "C" fn n_t_continue(o: *mut c_void) -> c_long {
    // SAFETY: the table's contract: a live socket op
    core().and_then(|c| c.socketop_continue).map_or(WS_NONE as c_long, |h| unsafe { h(o) })
}
unsafe extern "C" fn n_t_set_in_thread(o: *mut c_void, on: bool) {
    let so = socket_op(o);
    if !so.is_null() {
        // SAFETY: the op's live Rust object
        unsafe { (*so).set_in_socket_thread(on) };
    }
}
/// The socket thread's calls, answered in Rust but the virtual `Continue`.
pub static NATIVE_SOCKET_THREAD: EeWonSocketOpHooks = EeWonSocketOpHooks {
    status: Some(n_op_status),
    continue_op: Some(n_t_continue),
    finish: Some(n_op_finish),
    set_in_thread: Some(n_t_set_in_thread),
    add_ref: Some(n_ref_add),
    release: Some(n_ref_release),
    kill: Some(n_op_kill),
};

// ---- the socket op base and the socket

/// A call passed to the core table (the default without one).
macro_rules! pass {
    ($name:ident, $field:ident, ($($a:ident: $t:ty),*) -> $r:ty, $default:expr) => {
        pub(crate) unsafe extern "C" fn $name($($a: $t),*) -> $r {
            // SAFETY: the table's contract
            core().and_then(|c| c.$field).map_or($default, |h| unsafe { h($($a),*) })
        }
    };
    ($name:ident, $field:ident, ($($a:ident: $t:ty),*)) => {
        pub(crate) unsafe extern "C" fn $name($($a: $t),*) {
            if let Some(h) = core().and_then(|c| c.$field) {
                // SAFETY: the table's contract
                unsafe { h($($a),*) };
            }
        }
    };
}
pass!(n_so_start, socketop_start, (o: *mut c_void) -> c_long, WS_NONE as c_long);
pass!(n_so_thread_add, socket_thread_add, (o: *mut c_void));
pass!(n_bytes_of, bytes_of, (b: *mut c_void) -> *const EeWonByteBuffer, std::ptr::null());
pass!(n_bytes_adopt, bytes_adopt, (b: *mut EeWonByteBuffer) -> *mut c_void, null_mut());
pass!(n_transform, transform, (s: *mut c_void, m: *mut *mut c_void, send: bool) -> c_long, WS_NONE as c_long);
// a receive op's target file (the deleted CoreFileOpen / Write / Close / SetTime of AsyncOp.cpp)
unsafe extern "C" fn n_file_open(p: *const u8, n: usize, binary: bool, append: bool) -> *mut c_void {
    let mode = match (binary, append) {
        (true, true) => c"a+b",
        (true, false) => c"wb",
        (false, true) => c"a+",
        (false, false) => c"w",
    };
    // SAFETY: the caller's bytes
    crate::hostnative::plat_fopen(unsafe { crate::ffi::bytes(p, n) }, mode)
}
unsafe extern "C" fn n_file_write(f: *mut c_void, d: *const u8, n: usize) -> usize {
    // SAFETY: the caller's open file and bytes
    unsafe { crate::hostnative::fwrite(f, crate::ffi::bytes(d, n)) }
}
unsafe extern "C" fn n_file_close(f: *mut c_void) -> i32 {
    // SAFETY: the caller's open file, closed once
    unsafe { crate::hostnative::fclose(f) }
}
unsafe extern "C" fn n_file_set_time(p: *const u8, n: usize, t: i64) {
    // SAFETY: the caller's bytes
    crate::hostnative::set_file_time(unsafe { crate::ffi::bytes(p, n) }, t);
}
pass!(n_progress, progress, (o: *mut c_void, c: *mut c_void));
pass!(n_accept, accept, (s: *mut c_void, out: *mut *mut c_void) -> c_long, WS_NONE as c_long);
/// `delete transform`: a crypt transform's session reference given back (`~CryptTransform`), a C++ one deleted
unsafe extern "C" fn n_transform_free(t: *mut c_void) {
    if t.is_null() {
        return;
    }
    // SAFETY: a transform `add_crypt` or `ee_won_core_add_transform` made, freed once
    match *unsafe { Box::from_raw(t as *mut CoreTransform) } {
        // SAFETY: the session's reference the transform held
        CoreTransform::Crypt(session) => unsafe { crate::hostauth::ee_won_session_release(session) },
        // SAFETY: the C++ transform's own deletion
        CoreTransform::Cpp { object, free, .. } => unsafe { free(object) },
    }
}
unsafe extern "C" fn n_socket_object(s: *mut c_void) -> *mut EeWonAsyncSocket {
    object_of(s, EeWonCoreObject::Socket).cast()
}
fn socket_hook(s: *mut c_void, which: i32) {
    if let Some(h) = core().and_then(|c| c.socket_hook) {
        // SAFETY: the table's contract: a live socket
        unsafe { h(s, which) };
    }
}
unsafe extern "C" fn n_open_hook(s: *mut c_void) {
    socket_hook(s, 0)
}
unsafe extern "C" fn n_close_hook(s: *mut c_void) {
    socket_hook(s, 1)
}
unsafe extern "C" fn n_kill_hook(s: *mut c_void) {
    socket_hook(s, 2)
}
/// A socket's message transform: `CryptTransform` (8 bytes in the original, ServerOp.cpp's `Crypt` until net round
/// 29: a session handle it holds a reference to; its `Send` encrypts, its `Recv` decrypts), or a C++ `MsgTransform`
/// (the drivers' own: its virtual `Send` / `Recv` and its deletion through `call` and `free`).
enum CoreTransform {
    Crypt(*mut c_void),
    Cpp {
        object: *mut c_void,
        call: unsafe extern "C" fn(object: *mut c_void, message: *mut c_void, send: bool) -> c_long,
        free: unsafe extern "C" fn(object: *mut c_void),
    },
}

/// `socket->AddMsgTransform(new CryptTransform(session))`: the socket owns the transform.
pub(crate) fn add_crypt(socket_obj: *mut c_void, session: *mut c_void) {
    let Some(s) = socket(socket_obj) else { return };
    // SAFETY: a live session handle (the caller's)
    unsafe { crate::hostauth::ee_won_session_hold(session) };
    s.add_transform(Box::into_raw(Box::new(CoreTransform::Crypt(session))) as *mut c_void);
}

/// `socket->AddMsgTransform(transform)` for a C++ transform: `call(transform, ByteBufferPtr*, send)` runs its
/// `Send` or `Recv`, `free(transform)` deletes it; the socket owns it.
///
/// # Safety
/// `socket` is a live `AsyncSocket*` (its `RefCount*`); `transform` lives until `free`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_core_add_transform(
    socket: *mut c_void,
    transform: *mut c_void,
    call: unsafe extern "C" fn(object: *mut c_void, message: *mut c_void, send: bool) -> c_long,
    free: unsafe extern "C" fn(object: *mut c_void),
) {
    if let Some(s) = self::socket(socket) {
        s.add_transform(Box::into_raw(Box::new(CoreTransform::Cpp { object: transform, call, free })) as *mut c_void);
    }
}

/// The session's `Encrypt` (send) or `Decrypt` (receive) on the message in `*m` (a `ByteBufferPtr`: its one
/// pointer, a reference held), replaced when the session made a new one.
fn transform_call(t: *mut c_void, m: *mut c_void, send: bool) -> c_long {
    // SAFETY: a transform `add_crypt` or `ee_won_core_add_transform` made
    let session = match unsafe { &*(t as *const CoreTransform) } {
        CoreTransform::Crypt(session) => *session,
        // SAFETY: the C++ transform and the socket's message
        CoreTransform::Cpp { object, call, .. } => return unsafe { call(*object, m, send) },
    };
    // SAFETY: `m` the socket's `ByteBufferPtr*`, alive during the call
    let slot = unsafe { &mut *(m as *mut *mut c_void) };
    // SAFETY: the message's live buffer
    let b = unsafe { n_bytes_of(*slot) };
    let (data, len) = if b.is_null() {
        (std::ptr::null(), 0)
    } else {
        // SAFETY: the buffer's bytes, alive while the message is held
        unsafe { (ee_won_bytes_data(b), ee_won_bytes_length(b) as usize) }
    };
    let mut out: Option<Vec<u8>> = None;
    let mut replaced = false;
    // SAFETY: the bytes above; the sink below takes `out`
    let st = unsafe {
        crate::hostauth::ee_won_session_crypt(
            session,
            !send,
            data.cast(),
            len,
            collect_bytes,
            &mut out as *mut Option<Vec<u8>> as *mut c_void,
            &mut replaced,
        )
    };
    if let (true, Some(o)) = (replaced, out) {
        // SAFETY: a new buffer with one reference replaces the old one, whose reference goes back
        let new = unsafe { n_bytes_adopt(ee_won_bytes_copy(o.as_ptr(), o.len() as _)) };
        ref_release(std::mem::replace(slot, new));
    }
    st as c_long
}
unsafe extern "C" fn collect_bytes(context: *mut c_void, data: *const u8, length: usize) {
    // SAFETY: `transform_call`'s `Option<Vec<u8>>`; the session's bytes
    unsafe { *(context as *mut Option<Vec<u8>>) = Some(crate::ffi::bytes(data, length).to_vec()) };
}
unsafe extern "C" fn n_transform_send(t: *mut c_void, m: *mut c_void) -> c_long {
    transform_call(t, m, true)
}
unsafe extern "C" fn n_transform_recv(t: *mut c_void, m: *mut c_void) -> c_long {
    transform_call(t, m, false)
}

/// The socket op base's calls: the op's own answered in Rust, the rest the core table's.
pub static NATIVE_SOCKOP: EeWonSocketOpBaseHooks = EeWonSocketOpBaseHooks {
    start: Some(n_so_start),
    continue_op: Some(n_t_continue),
    finish: Some(n_op_finish),
    mode: Some(n_op_mode),
    add_to_socket_thread: Some(n_so_thread_add),
    socket_add_ref: Some(n_ref_add),
    socket_release: Some(n_ref_release),
    socket_object: Some(n_socket_object),
    ref_add: Some(n_ref_add),
    ref_release: Some(n_ref_release),
    bytes_of: Some(n_bytes_of),
    bytes_adopt: Some(n_bytes_adopt),
    transform: Some(n_transform),
    file_open: Some(n_file_open),
    file_write: Some(n_file_write),
    file_close: Some(n_file_close),
    file_set_time: Some(n_file_set_time),
    progress: Some(n_progress),
    accept: Some(n_accept),
};

/// The socket's calls: its virtual hooks and its transforms', through the core table.
pub static NATIVE_SOCKET: EeWonAsyncSocketHooks = EeWonAsyncSocketHooks {
    open_hook: Some(n_open_hook),
    close_hook: Some(n_close_hook),
    kill_hook: Some(n_kill_hook),
    transform_send: Some(n_transform_send),
    transform_recv: Some(n_transform_recv),
    transform_free: Some(n_transform_free),
};

// ---- the server connection

/// What only the C++ `ServerConnection` answers: the peer authentication op's maker and its virtual `MsgCallback`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonServerConnCpp {
    /// `new PeerAuthOp(*address, context, type, conn, true)` (the op's reference not yet taken)
    pub new_peer_auth: Option<
        unsafe extern "C" fn(conn: *mut c_void, address: *const c_void, context: *mut c_void, auth: i32) -> *mut c_void,
    >,
    /// `conn->MsgCallback(message)` (a `ByteBuffer*`)
    pub msg_callback: Option<unsafe extern "C" fn(conn: *mut c_void, message: *mut c_void)>,
}

static SERVCONN: AtomicPtr<EeWonServerConnCpp> = AtomicPtr::new(null_mut());

/// Binds the server connection's C++ table (the same one every time).
pub fn bind_servconn(cpp: *const EeWonServerConnCpp) {
    if !cpp.is_null() {
        SERVCONN.store(cpp.cast_mut(), Ordering::Release);
    }
}
fn servconn_cpp() -> Option<&'static EeWonServerConnCpp> {
    // SAFETY: a bound table lives for the program
    unsafe { SERVCONN.load(Ordering::Acquire).as_ref() }
}
fn servconn(conn: *mut c_void) -> *const EeWonServerConn {
    object_of(conn, EeWonCoreObject::ServerConn).cast()
}

unsafe extern "C" fn n_op_run_block(o: *mut c_void, t: u32) {
    if let Some(x) = op(o) {
        x.run(2, t);
    }
}
unsafe extern "C" fn n_new_peer_auth(conn: *mut c_void, a: *const c_void, ctx: *mut c_void, auth: i32) -> *mut c_void {
    // SAFETY: the table's contract
    servconn_cpp().and_then(|c| c.new_peer_auth).map_or(null_mut(), |h| unsafe { h(conn, a, ctx, auth) })
}
/// `StaticConnectCallback` / `StaticKeepAliveCallback`, holding the connection
fn conn_completion(conn: *mut c_void, keep_alive: bool) -> *mut c_void {
    let held = hold(conn);
    completion(move |o| {
        let _param = hold(held.get());
        let _op = hold(o);
        let c = servconn(held.get());
        if !c.is_null() {
            // SAFETY: the connection's live Rust object
            unsafe {
                if keep_alive {
                    ee_won_servconn_keep_alive_callback(c, o)
                } else {
                    ee_won_servconn_connect_callback(c, o)
                }
            };
        }
    })
}
/// `ServerConnectionConnectOp::RunHook` (2ac2f0): the connection's connect completion finishes the op with the
/// connect's status while it is pending (`StaticConnectCompletion` 2ac240 / `ConnectCompletion` 2ac2d0, a
/// `ParamCompletion` holding the op), then `ConnectAsync(*address, timeout)`.
///
/// # Safety
/// `conn` is a live `ServerConnection*`, `op` the live op (its `RefCount*`), `address` its `IPAddr*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_servconn_connect_op_run(
    conn: *mut c_void,
    op: *mut c_void,
    address: *const c_void,
    timeout: u32,
) {
    let c = servconn(conn);
    if c.is_null() {
        return;
    }
    let held = hold(op);
    let done = completion(move |o| {
        let _param = hold(held.get());
        let _op = hold(o);
        if let Some(me) = self::op(held.get()) {
            if me.status() == WS_PENDING {
                me.finish(self::op(o).map_or(WS_NONE, EeWonAsyncOp::status));
            }
        }
    });
    // SAFETY: the connection's live Rust object; it takes the completion's reference
    unsafe {
        (*c).set_connect_completion(done);
        (*c).connect_async(address, timeout as i32 as c_long);
    }
}
unsafe extern "C" fn n_route_connect(conn: *mut c_void, o: *mut c_void) {
    // SAFETY: a live op; it takes the completion's reference
    unsafe { n_op_set_completion(o, conn_completion(conn, false)) };
}
unsafe extern "C" fn n_new_keep_alive(conn: *mut c_void) -> *mut c_void {
    // SAFETY: the table's contract
    let t = core().and_then(|c| c.new_op).map_or(null_mut(), |h| unsafe { h() });
    // SAFETY: the new op takes the completion's reference
    unsafe { n_op_set_completion(t, conn_completion(conn, true)) };
    t
}
unsafe extern "C" fn n_start_messages(conn: *mut c_void) {
    let Some(q) = queue(conn) else { return };
    let held = hold(conn);
    // `StaticMsgCallback`: a received message to the virtual MsgCallback
    q.set_repeat_completion(completion(move |o| {
        let _param = hold(held.get());
        let _op = hold(o);
        if op(o).map_or(WS_NONE, EeWonAsyncOp::status) != WS_SUCCESS {
            return;
        }
        let so = socket_op(o);
        // SAFETY: the op's live Rust object; the message is held for the call (`GetMsg()`'s temporary)
        let message =
            hold(if so.is_null() { null_mut() } else { unsafe { crate::sockops::ee_won_socketop_message(so) } });
        if let Some(h) = servconn_cpp().and_then(|c| c.msg_callback) {
            // SAFETY: the table's contract
            unsafe { h(held.get(), message.get()) };
        }
    }));
    q.set_repeat_op(new_socket_op(EeWonCoreSocketOp::RecvMsg, null_mut(), null_mut(), std::ptr::null(), 0));
}
unsafe extern "C" fn n_stop_messages(conn: *mut c_void) {
    if let Some(q) = queue(conn) {
        q.set_repeat_completion(null_mut());
    }
}
unsafe extern "C" fn n_queue_message(conn: *mut c_void, bytes: *const u8, length: usize) {
    // SAFETY: the caller's bytes; a new buffer with one reference (`ByteBufferPtr aMsg`)
    let b = unsafe { n_bytes_adopt(ee_won_bytes_copy(bytes, length as _)) };
    if let Some(q) = queue(conn) {
        q.queue_op(new_socket_op(EeWonCoreSocketOp::SendMsg, null_mut(), b, std::ptr::null(), 0), -1);
    }
    ref_release(b);
}
unsafe extern "C" fn n_length_field_size(conn: *mut c_void) -> u8 {
    socket(conn).map_or(0, EeWonAsyncSocket::length_field_size)
}
unsafe extern "C" fn n_queue_close_hook(conn: *mut c_void) {
    if let Some(q) = queue(conn) {
        q.close_hook();
    }
}
unsafe extern "C" fn n_queue_kill_hook(conn: *mut c_void) {
    if let Some(q) = queue(conn) {
        q.kill_hook();
    }
}

/// The server connection's calls: in Rust but the peer authentication op's maker and `MsgCallback`.
pub static NATIVE_SERVCONN: EeWonServerConnHooks = EeWonServerConnHooks {
    add_ref: Some(n_ref_add),
    release: Some(n_ref_release),
    op_status: Some(n_op_status),
    op_killed: Some(n_op_killed),
    op_kill: Some(n_op_kill),
    op_set_completion: Some(n_op_set_completion),
    op_run_async: Some(n_op_run_async),
    op_run_block: Some(n_op_run_block),
    complete: Some(n_completion_call),
    new_peer_auth: Some(n_new_peer_auth),
    route_connect: Some(n_route_connect),
    new_keep_alive: Some(n_new_keep_alive),
    start_messages: Some(n_start_messages),
    stop_messages: Some(n_stop_messages),
    queue_message: Some(n_queue_message),
    close: Some(n_socket_close),
    length_field_size: Some(n_length_field_size),
    queue_close_hook: Some(n_queue_close_hook),
    queue_kill_hook: Some(n_queue_kill_hook),
};

#[cfg(test)]
#[path = "cppcore_tests.rs"]
pub(crate) mod tests;
