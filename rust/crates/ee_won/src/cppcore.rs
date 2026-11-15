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

use crate::buffer::{ee_won_bytes_data, ee_won_bytes_length, EeWonByteBuffer};
use crate::ipobj::{ee_won_ipaddr_free, EeWonIpAddr};
use crate::opobj::{EeWonAsyncOp, EeWonCompletion, HeldRef};
use crate::qsock::{EeWonQueueSocket, EeWonQueueSocketHooks};
use crate::sockobj::EeWonAsyncSocket;
use crate::sockop::EeWonSocketOp;
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
    /// the object's Rust object of that kind (null for a null object)
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
            size: u16,
        ) -> *mut c_void,
    >,
    /// `new QueueSocket(type)`
    pub new_queue_socket: Option<unsafe extern "C" fn(kind: i32) -> *mut c_void>,
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

fn core() -> Option<&'static EeWonCoreHooks> {
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

fn op<'a>(o: *mut c_void) -> Option<&'a EeWonAsyncOp> {
    // SAFETY: an op's Rust object lives as long as the op, which the caller holds
    unsafe { (object_of(o, EeWonCoreObject::Op) as *const EeWonAsyncOp).as_ref() }
}
fn socket<'a>(s: *mut c_void) -> Option<&'a EeWonAsyncSocket> {
    // SAFETY: as `op`
    unsafe { (object_of(s, EeWonCoreObject::Socket) as *const EeWonAsyncSocket).as_ref() }
}
fn queue<'a>(s: *mut c_void) -> Option<&'a EeWonQueueSocket> {
    // SAFETY: as `op`
    unsafe { (object_of(s, EeWonCoreObject::Queue) as *const EeWonQueueSocket).as_ref() }
}
fn socket_op(o: *mut c_void) -> *const EeWonSocketOp {
    object_of(o, EeWonCoreObject::SocketOp) as *const EeWonSocketOp
}

fn ref_add(o: *mut c_void) {
    if !o.is_null() {
        if let Some(h) = core().and_then(|c| c.ref_add) {
            // SAFETY: the table's contract: a live object
            unsafe { h(o) };
        }
    }
}
fn ref_release(o: *mut c_void) {
    if !o.is_null() {
        if let Some(h) = core().and_then(|c| c.ref_release) {
            // SAFETY: the table's contract: a reference the caller held
            unsafe { h(o) };
        }
    }
}

/// A reference held for a scope (a by-value `SmartPtr`); none for null.
struct Hold(Option<HeldRef>);
fn hold(o: *mut c_void) -> Hold {
    if o.is_null() {
        return Hold(None);
    }
    let c = core();
    Hold(Some(HeldRef::new(o, c.and_then(|c| c.ref_add), c.and_then(|c| c.ref_release))))
}
impl Hold {
    fn get(&self) -> *mut c_void {
        self.0.as_ref().map_or(null_mut(), HeldRef::get)
    }
}

/// A C++ completion running `call` (null without a table).
fn completion(call: impl Fn(*mut c_void) + Send + Sync + 'static) -> *mut c_void {
    EeWonCompletion::make_with(core().and_then(|c| c.new_completion), call)
}

fn new_socket_op(
    kind: EeWonCoreSocketOp,
    socket: *mut c_void,
    bytes: *mut c_void,
    addr: *const EeWonIpAddr,
    size: u16,
) -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.new_socket_op).map_or(null_mut(), |h| unsafe { h(kind, socket, bytes, addr, size) })
}
fn new_queue_socket() -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.new_queue_socket).map_or(null_mut(), |h| unsafe { h(1) })
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

unsafe extern "C" fn n_ref_add(o: *mut c_void) {
    ref_add(o)
}
unsafe extern "C" fn n_ref_release(o: *mut c_void) {
    ref_release(o)
}
unsafe extern "C" fn n_completion_call(c: *mut c_void, o: *mut c_void) {
    if let Some(h) = core().and_then(|c| c.completion_call) {
        // SAFETY: the table's contract: a live completion and op
        unsafe { h(c, o) };
    }
}
unsafe extern "C" fn n_op_status(o: *mut c_void) -> c_long {
    op(o).map_or(WS_NONE, EeWonAsyncOp::status) as c_long
}
unsafe extern "C" fn n_op_killed(o: *mut c_void) -> bool {
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
unsafe extern "C" fn n_op_completion(o: *mut c_void) -> *mut c_void {
    op(o).map_or(null_mut(), EeWonAsyncOp::completion)
}
unsafe extern "C" fn n_op_set_completion(o: *mut c_void, c: *mut c_void) {
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
unsafe extern "C" fn n_op_run_async(o: *mut c_void, t: c_long) {
    if let Some(x) = op(o) {
        x.run(1, t as u32);
    }
}
unsafe extern "C" fn n_op_kill(o: *mut c_void) {
    if let Some(x) = op(o) {
        x.kill(true);
    }
}
unsafe extern "C" fn n_op_finish(o: *mut c_void, s: c_long) {
    if let Some(x) = op(o) {
        x.finish(s as Status);
    }
}
unsafe extern "C" fn n_op_force_finish(o: *mut c_void, s: c_long) {
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
    q.set_repeat_op(new_socket_op(EeWonCoreSocketOp::RecvFrom, null_mut(), null_mut(), std::ptr::null(), size as u16));
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

#[cfg(test)]
#[path = "cppcore_tests.rs"]
mod tests;
