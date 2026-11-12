//! The Rust op base (round 9): an op whose members and every step are Rust's, over the C++ objects it drives (its
//! sockets, the socket ops, timers, child ops, the file it reads), through ONE narrow C interface. The C++ side is
//! one class, `RustServerOp` (a `ServerOp`, WONRouting.h): its virtual `RunHook`, `CleanupHook` and `CallbackHook`
//! and the completions of the timers and repeating receives it made come here ([`ee_won_hostop_run`],
//! [`ee_won_hostop_cleanup`], [`ee_won_hostop_callback`]), and everything the op does to a C++ object is one
//! [`EeWonHostCall`] through one function the class binds ([`ee_won_hostop_bind`]). So an op ported here has no
//! C++ class, no hook table and no shim of its own: `MultiPingOp` (multiping.rs) and `FilePushOp` (filepush.rs)
//! are the first.
//!
//! The calls are primitives of the original's statements (`new RecvBytesFromOp(n)`, `socket->QueueOp(op, t)`,
//! `Track(op, id)`, `op->RunAsync(t)`, `Finish(status)`, `TimeLeft()`, a blocking `SendBytesTo`, ...), made in the
//! original's order, so a driver logging them over stand-ins compares the Rust op with the original's statements
//! call for call (`tests/won/won_hostop_test.cpp`, golden `golden_hostop.txt`).
//!
//! Objects are C++ `RefCount*`. A call that makes one returns it with a reference the op owns, released with
//! [`Host::release`]. A call can re-enter the op (a queued op completing at once, `Finish` delivering, a socket's
//! close killing its ops): the op's state is behind a mutex taken only for statements that make no call, never
//! across one (tests assert it), so re-entry and the socket threads' completions find it free.

use std::ffi::c_void;
use std::sync::{Mutex, MutexGuard};

use crate::ffi::{self, EeWonByteSink};

/// `WS_Success`
pub(crate) const ST_SUCCESS: i32 = 0;
/// `WS_Pending`
pub(crate) const ST_PENDING: i32 = 100000;
/// `OP_MODE_ASYNC`
pub(crate) const MODE_ASYNC: i32 = 1;
/// An infinite wait (`(DWORD)-1`)
pub(crate) const FOREVER: u32 = 0xffff_ffff;

// The calls (`EeWonHostCall::code`). Queries of the op itself:
/// `GetStatus()` -> out_num
pub const EE_WON_HOST_STATUS: i32 = 1;
/// `GetMode()` -> out_num
pub const EE_WON_HOST_MODE: i32 = 2;
/// `TimeLeft()` -> out_num
pub const EE_WON_HOST_TIME_LEFT: i32 = 3;
/// `WONTickCount()` -> out_num
pub const EE_WON_HOST_TICK: i32 = 4;
/// `rand()` -> out_num
pub const EE_WON_HOST_RAND: i32 = 5;
/// `srand(time(NULL))`
pub const EE_WON_HOST_SRAND: i32 = 6;
/// `ServerOp::mLengthFieldSize` -> out_num
pub const EE_WON_HOST_LFS: i32 = 7;
/// `ServerOp::SendTime()` -> out_num
pub const EE_WON_HOST_SEND_TIME: i32 = 8;
/// `ServerOp::RecvTime()` -> out_num
pub const EE_WON_HOST_RECV_TIME: i32 = 9;
// New objects (one reference the op owns):
/// `new BlockingSocket(num0)` (0 TCP, 1 UDP) -> out_obj
pub const EE_WON_HOST_NEW_SOCKET: i32 = 10;
/// a new socket op of kind num0 (`EE_WON_HOST_OP_*`) on socket obj0 (NULL: the queue's) with data, addr and
/// length num1 -> out_obj
pub const EE_WON_HOST_NEW_OP: i32 = 11;
/// `new AsyncOp` whose completion calls the op back with id num0 (`ee_won_hostop_callback`) -> out_obj
pub const EE_WON_HOST_NEW_TIMER: i32 = 12;
/// `new PeerAuthOp(addr, mAuthContext, mAuthType, obj0, num0)` with `CopyMaxTimes(this)` -> out_obj
pub const EE_WON_HOST_NEW_PEER_AUTH: i32 = 13;
// Reads of an object obj0:
/// `op->GetStatus()` -> out_num
pub const EE_WON_HOST_OP_STATUS: i32 = 20;
/// a receive op's bytes (`GetBytes` / `GetMsg`; none: NULL) -> the sink
pub const EE_WON_HOST_OP_BYTES: i32 = 21;
/// a datagram receive's sender (`GetAddr`) -> out_addr
pub const EE_WON_HOST_OP_ADDR: i32 = 22;
/// a socket op's socket (`GetSocket`) -> out_obj, borrowed
pub const EE_WON_HOST_OP_SOCKET: i32 = 23;
/// a peer auth op's session key (`GetSession()->GetKey()`; no session: none) -> the sink
pub const EE_WON_HOST_PEER_AUTH_KEY: i32 = 24;
// Immediate socket calls:
/// `socket->Fn2B0D90(num0, num1)` (the bind) -> out_num
pub const EE_WON_HOST_SOCKET_BIND: i32 = 30;
/// `socket->SetLengthFieldSize(num0)`
pub const EE_WON_HOST_SOCKET_SET_LFS: i32 = 31;
// Blocking socket calls (status -> out_num):
/// `socket->SendBytesTo(data, addr, num0)`
pub const EE_WON_HOST_SEND_BYTES_TO: i32 = 40;
/// `socket->RecvBytesFrom(bytes, num0, addr, num1)` -> the sink, out_addr
pub const EE_WON_HOST_RECV_BYTES_FROM: i32 = 41;
/// `socket->SendBytes(data, num0)`
pub const EE_WON_HOST_SEND_BYTES: i32 = 42;
/// `socket->SendMsg(data, num0)`
pub const EE_WON_HOST_SEND_MSG: i32 = 43;
/// `socket->RecvBytes(bytes, num0, num1)` -> the sink
pub const EE_WON_HOST_RECV_BYTES: i32 = 44;
/// `socket->RecvMsg(bytes, num0)` -> the sink
pub const EE_WON_HOST_RECV_MSG: i32 = 45;
/// `op->RunBlock(num0)` -> out_num (0/1)
pub const EE_WON_HOST_RUN_BLOCK: i32 = 46;
// The op's file (one per op):
/// `mFile.Attach(PlatFOpen(data, "rb"))` -> out_num (0: none)
pub const EE_WON_HOST_FILE_OPEN: i32 = 50;
/// `fread(buffer, 1, num0, file)` -> the sink
pub const EE_WON_HOST_FILE_READ: i32 = 51;
/// `mFile.Detach()`
pub const EE_WON_HOST_FILE_CLOSE: i32 = 52;
// Calls that may call back into the op:
/// `Finish(num0)`
pub const EE_WON_HOST_FINISH: i32 = 60;
/// `obj0->Kill(num0)`
pub const EE_WON_HOST_KILL: i32 = 61;
/// `Track(obj0, num0)`
pub const EE_WON_HOST_TRACK: i32 = 62;
/// `obj0->QueueOp(obj1, num0)` (the socket, the op)
pub const EE_WON_HOST_QUEUE: i32 = 63;
/// `obj0->RunAsync(num0)`
pub const EE_WON_HOST_RUN_ASYNC: i32 = 64;
/// `obj0->Close()` (a socket)
pub const EE_WON_HOST_CLOSE: i32 = 65;
/// `obj0->SetRepeatCompletion(...)` calling the op back with id num0, then `obj0->SetRepeatOp(obj1)`
pub const EE_WON_HOST_SET_REPEAT: i32 = 66;
/// `obj0->Complete(this)` (a completion)
pub const EE_WON_HOST_COMPLETE: i32 = 67;
/// `AsyncOpWithTracker::CleanupHook()`
pub const EE_WON_HOST_BASE_CLEANUP: i32 = 68;
/// `obj0->Release()`
pub const EE_WON_HOST_RELEASE: i32 = 69;
/// `obj0->AddRef()`
pub const EE_WON_HOST_ADD_REF: i32 = 70;

// Socket op kinds (`EE_WON_HOST_NEW_OP`'s num0):
/// `ConnectOp(addr)`
pub const EE_WON_HOST_OP_CONNECT: i64 = 0;
/// `SendBytesOp(data)`
pub const EE_WON_HOST_OP_SEND_BYTES: i64 = 1;
/// `SendBytesToOp(data, addr)`
pub const EE_WON_HOST_OP_SEND_BYTES_TO: i64 = 2;
/// `SendMsgOp(data)`
pub const EE_WON_HOST_OP_SEND_MSG: i64 = 3;
/// `RecvBytesOp(num1)`
pub const EE_WON_HOST_OP_RECV_BYTES: i64 = 4;
/// `RecvMsgOp()`
pub const EE_WON_HOST_OP_RECV_MSG: i64 = 5;
/// `RecvCRMsgOp()`
pub const EE_WON_HOST_OP_RECV_CR_MSG: i64 = 6;
/// `RecvBytesFromOp(num1)`
pub const EE_WON_HOST_OP_RECV_BYTES_FROM: i64 = 7;

/// One call of the op on the C++ side: the code, its inputs, its outputs. Bytes come back through `out_sink`
/// (not called: NULL).
#[repr(C)]
pub struct EeWonHostCall {
    pub code: i32,
    pub obj: [*mut c_void; 2],
    pub num: [i64; 2],
    pub data: *const u8,
    pub len: usize,
    /// an address's six bytes (`IPAddr::GetSixByte`)
    pub addr: [u8; 6],
    pub out_obj: *mut c_void,
    pub out_num: i64,
    pub out_addr: [u8; 6],
    pub out_sink: EeWonByteSink,
    pub out_context: *mut c_void,
}

/// The C++ side's one function: `host` is the `RustServerOp` (or a driver's stand-in).
pub type EeWonHostFn = Option<unsafe extern "C" fn(host: *mut c_void, call: *mut EeWonHostCall)>;

/// A C++ `RefCount*` (0: none). A number, so the op's state is `Send`.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) struct HostObj(pub(crate) usize);

pub(crate) use self::HostObj as Obj;

// (no associated consts: cbindgen would export the type)
impl HostObj {
    pub(crate) const fn none() -> HostObj {
        HostObj(0)
    }
    pub(crate) fn is_none(self) -> bool {
        self.0 == 0
    }
    pub(crate) fn ptr(self) -> *mut c_void {
        self.0 as *mut c_void
    }
    pub(crate) fn of(p: *mut c_void) -> HostObj {
        HostObj(p as usize)
    }
}

/// A call's answers.
pub(crate) struct Reply {
    pub(crate) obj: Obj,
    pub(crate) num: i64,
    pub(crate) addr: [u8; 6],
    pub(crate) bytes: Option<Vec<u8>>,
}

unsafe extern "C" fn collect(context: *mut c_void, data: *const u8, length: usize) {
    // SAFETY: `context` is the `Option<Vec<u8>>` of `Host::invoke`, alive during the call; the C++ side's bytes
    let out = unsafe { &mut *(context as *mut Option<Vec<u8>>) };
    *out = Some(unsafe { ffi::bytes(data, length) }.to_vec());
}

/// An op's plan: its members and its steps, each an associated function over the [`Host`] (which holds the
/// members behind its lock).
pub(crate) trait Plan: Send + Sized + 'static {
    /// `RunHook`
    fn run(h: &Host<Self>);
    /// a tracked op finished (`CallbackHook(op, id)`), or a completion this op made was called with `op`
    fn callback(h: &Host<Self>, id: i32, op: Obj) -> bool;
    /// `CleanupHook`
    fn cleanup(h: &Host<Self>);
    /// a result by number (`which` the op's `EE_WON_*` constant, `index` an element)
    fn num(&self, _which: i32, _index: i64) -> i64 {
        0
    }
    /// a result's bytes by number
    fn bytes(&self, _which: i32, _index: i64) -> Option<Vec<u8>> {
        None
    }
    /// a setter by number (the value as a number, bytes, an address or an object)
    fn set(_h: &Host<Self>, _which: i32, _value: &SetValue) {}
    /// the members, one line (the drivers' and goldens' form)
    fn describe(&self) -> String;
    /// every object reference held, taken (the destructor releases them)
    fn take_objects(&mut self) -> Vec<Obj>;
}

/// A setter's value.
pub(crate) struct SetValue<'a> {
    pub(crate) num: i64,
    pub(crate) data: &'a [u8],
    pub(crate) addr: [u8; 6],
    pub(crate) obj: Obj,
}

/// The op: its plan's members behind a lock, and the C++ side's function.
pub(crate) struct Host<P> {
    st: Mutex<P>,
    bind: Mutex<(EeWonHostFn, usize)>,
}

impl<P: Plan> Host<P> {
    pub(crate) fn new(plan: P) -> Host<P> {
        Host { st: Mutex::new(plan), bind: Mutex::new((None, 0)) }
    }

    /// The members, for statements that make no call.
    pub(crate) fn st(&self) -> MutexGuard<'_, P> {
        self.st.lock().unwrap_or_else(|e| e.into_inner())
    }

    fn invoke(&self, code: i32, obj: [Obj; 2], num: [i64; 2], data: &[u8], addr: [u8; 6]) -> Reply {
        // a call may re-enter the op: never under its lock
        #[cfg(test)]
        assert!(self.st.try_lock().is_ok(), "a call under the op's lock (code {code})");
        let (f, host) = *self.bind.lock().unwrap_or_else(|e| e.into_inner());
        let mut out: Option<Vec<u8>> = None;
        let mut c = EeWonHostCall {
            code,
            obj: [obj[0].ptr(), obj[1].ptr()],
            num,
            data: if data.is_empty() { std::ptr::null() } else { data.as_ptr() },
            len: data.len(),
            addr,
            out_obj: std::ptr::null_mut(),
            out_num: 0,
            out_addr: [0; 6],
            out_sink: collect,
            out_context: &mut out as *mut Option<Vec<u8>> as *mut c_void,
        };
        if let Some(f) = f {
            // SAFETY: the bound function's contract: it answers the call during it
            unsafe { f(host as *mut c_void, &mut c) };
        }
        Reply { obj: Obj::of(c.out_obj), num: c.out_num, addr: c.out_addr, bytes: out }
    }

    fn num_of(&self, code: i32, obj: Obj, num: [i64; 2]) -> i64 {
        self.invoke(code, [obj, Obj::none()], num, &[], [0; 6]).num
    }

    // ---- the op itself
    pub(crate) fn status(&self) -> i32 {
        self.num_of(EE_WON_HOST_STATUS, Obj::none(), [0, 0]) as i32
    }
    pub(crate) fn is_async(&self) -> bool {
        self.num_of(EE_WON_HOST_MODE, Obj::none(), [0, 0]) as i32 == MODE_ASYNC
    }
    pub(crate) fn time_left(&self) -> u32 {
        self.num_of(EE_WON_HOST_TIME_LEFT, Obj::none(), [0, 0]) as u32
    }
    pub(crate) fn tick(&self) -> u32 {
        self.num_of(EE_WON_HOST_TICK, Obj::none(), [0, 0]) as u32
    }
    pub(crate) fn rand(&self) -> i32 {
        self.num_of(EE_WON_HOST_RAND, Obj::none(), [0, 0]) as i32
    }
    pub(crate) fn srand(&self) {
        self.num_of(EE_WON_HOST_SRAND, Obj::none(), [0, 0]);
    }
    pub(crate) fn lfs(&self) -> u8 {
        self.num_of(EE_WON_HOST_LFS, Obj::none(), [0, 0]) as u8
    }
    pub(crate) fn send_time(&self) -> u32 {
        self.num_of(EE_WON_HOST_SEND_TIME, Obj::none(), [0, 0]) as u32
    }
    pub(crate) fn recv_time(&self) -> u32 {
        self.num_of(EE_WON_HOST_RECV_TIME, Obj::none(), [0, 0]) as u32
    }
    pub(crate) fn finish(&self, status: i32) {
        self.num_of(EE_WON_HOST_FINISH, Obj::none(), [i64::from(status), 0]);
    }
    pub(crate) fn base_cleanup(&self) {
        self.num_of(EE_WON_HOST_BASE_CLEANUP, Obj::none(), [0, 0]);
    }

    // ---- objects
    pub(crate) fn new_socket(&self, udp: bool) -> Obj {
        self.invoke(EE_WON_HOST_NEW_SOCKET, [Obj::none(); 2], [i64::from(udp), 0], &[], [0; 6]).obj
    }
    pub(crate) fn new_op(&self, kind: i64, socket: Obj, data: &[u8], addr: [u8; 6], length: u32) -> Obj {
        self.invoke(EE_WON_HOST_NEW_OP, [socket, Obj::none()], [kind, i64::from(length)], data, addr).obj
    }
    pub(crate) fn new_timer(&self, id: i32) -> Obj {
        self.invoke(EE_WON_HOST_NEW_TIMER, [Obj::none(); 2], [i64::from(id), 0], &[], [0; 6]).obj
    }
    pub(crate) fn new_peer_auth(&self, addr: [u8; 6], socket: Obj, use_auth2: bool) -> Obj {
        self.invoke(EE_WON_HOST_NEW_PEER_AUTH, [socket, Obj::none()], [i64::from(use_auth2), 0], &[], addr).obj
    }
    pub(crate) fn release(&self, o: Obj) {
        if !o.is_none() {
            self.num_of(EE_WON_HOST_RELEASE, o, [0, 0]);
        }
    }
    pub(crate) fn add_ref(&self, o: Obj) {
        if !o.is_none() {
            self.num_of(EE_WON_HOST_ADD_REF, o, [0, 0]);
        }
    }

    // ---- reads of an object
    pub(crate) fn op_status(&self, op: Obj) -> i32 {
        self.num_of(EE_WON_HOST_OP_STATUS, op, [0, 0]) as i32
    }
    pub(crate) fn op_bytes(&self, op: Obj) -> Option<Vec<u8>> {
        self.invoke(EE_WON_HOST_OP_BYTES, [op, Obj::none()], [0, 0], &[], [0; 6]).bytes
    }
    pub(crate) fn op_addr(&self, op: Obj) -> [u8; 6] {
        self.invoke(EE_WON_HOST_OP_ADDR, [op, Obj::none()], [0, 0], &[], [0; 6]).addr
    }
    pub(crate) fn op_socket(&self, op: Obj) -> Obj {
        self.invoke(EE_WON_HOST_OP_SOCKET, [op, Obj::none()], [0, 0], &[], [0; 6]).obj
    }
    pub(crate) fn peer_auth_key(&self, op: Obj) -> Option<Vec<u8>> {
        self.invoke(EE_WON_HOST_PEER_AUTH_KEY, [op, Obj::none()], [0, 0], &[], [0; 6]).bytes
    }

    // ---- sockets
    pub(crate) fn bind(&self, socket: Obj, port: u16, reuse: bool) -> i32 {
        self.num_of(EE_WON_HOST_SOCKET_BIND, socket, [i64::from(port), i64::from(reuse)]) as i32
    }
    pub(crate) fn set_lfs(&self, socket: Obj, size: u8) {
        self.num_of(EE_WON_HOST_SOCKET_SET_LFS, socket, [i64::from(size), 0]);
    }
    pub(crate) fn send_bytes_to(&self, socket: Obj, data: &[u8], addr: [u8; 6], timeout: u32) -> i32 {
        self.invoke(EE_WON_HOST_SEND_BYTES_TO, [socket, Obj::none()], [i64::from(timeout), 0], data, addr).num as i32
    }
    pub(crate) fn recv_bytes_from(&self, socket: Obj, size: u16, timeout: u32) -> (i32, Option<Vec<u8>>, [u8; 6]) {
        let r = self.invoke(
            EE_WON_HOST_RECV_BYTES_FROM,
            [socket, Obj::none()],
            [i64::from(size), i64::from(timeout)],
            &[],
            [0; 6],
        );
        (r.num as i32, r.bytes, r.addr)
    }
    pub(crate) fn send_bytes(&self, socket: Obj, data: &[u8], timeout: u32) -> i32 {
        self.invoke(EE_WON_HOST_SEND_BYTES, [socket, Obj::none()], [i64::from(timeout), 0], data, [0; 6]).num as i32
    }
    pub(crate) fn send_msg(&self, socket: Obj, data: &[u8], timeout: u32) -> i32 {
        self.invoke(EE_WON_HOST_SEND_MSG, [socket, Obj::none()], [i64::from(timeout), 0], data, [0; 6]).num as i32
    }
    pub(crate) fn recv_bytes(&self, socket: Obj, length: u32, timeout: u32) -> (i32, Option<Vec<u8>>) {
        let r = self.invoke(
            EE_WON_HOST_RECV_BYTES,
            [socket, Obj::none()],
            [i64::from(length), i64::from(timeout)],
            &[],
            [0; 6],
        );
        (r.num as i32, r.bytes)
    }
    pub(crate) fn recv_msg(&self, socket: Obj, timeout: u32) -> (i32, Option<Vec<u8>>) {
        let r = self.invoke(EE_WON_HOST_RECV_MSG, [socket, Obj::none()], [i64::from(timeout), 0], &[], [0; 6]);
        (r.num as i32, r.bytes)
    }
    pub(crate) fn run_block(&self, op: Obj, timeout: u32) -> bool {
        self.num_of(EE_WON_HOST_RUN_BLOCK, op, [i64::from(timeout), 0]) != 0
    }

    // ---- the file
    pub(crate) fn file_open(&self, path: &[u8]) -> bool {
        self.invoke(EE_WON_HOST_FILE_OPEN, [Obj::none(); 2], [0, 0], path, [0; 6]).num != 0
    }
    pub(crate) fn file_read(&self, length: u32) -> Vec<u8> {
        self.invoke(EE_WON_HOST_FILE_READ, [Obj::none(); 2], [i64::from(length), 0], &[], [0; 6])
            .bytes
            .unwrap_or_default()
    }
    pub(crate) fn file_close(&self) {
        self.num_of(EE_WON_HOST_FILE_CLOSE, Obj::none(), [0, 0]);
    }

    // ---- calls that may call back
    pub(crate) fn kill(&self, op: Obj, finish: bool) {
        self.num_of(EE_WON_HOST_KILL, op, [i64::from(finish), 0]);
    }
    pub(crate) fn track(&self, op: Obj, id: i32) {
        self.num_of(EE_WON_HOST_TRACK, op, [i64::from(id), 0]);
    }
    pub(crate) fn queue(&self, socket: Obj, op: Obj, timeout: u32) {
        self.invoke(EE_WON_HOST_QUEUE, [socket, op], [i64::from(timeout), 0], &[], [0; 6]);
    }
    pub(crate) fn run_async(&self, op: Obj, timeout: u32) {
        self.num_of(EE_WON_HOST_RUN_ASYNC, op, [i64::from(timeout), 0]);
    }
    pub(crate) fn close(&self, socket: Obj) {
        self.num_of(EE_WON_HOST_CLOSE, socket, [0, 0]);
    }
    pub(crate) fn set_repeat(&self, socket: Obj, op: Obj, id: i32) {
        self.invoke(EE_WON_HOST_SET_REPEAT, [socket, op], [i64::from(id), 0], &[], [0; 6]);
    }
    pub(crate) fn complete(&self, completion: Obj) {
        self.num_of(EE_WON_HOST_COMPLETE, completion, [0, 0]);
    }

    /// `slot = o` for an object slot of the members (`o` carries the reference the slot keeps; the old one goes
    /// after, outside the lock).
    pub(crate) fn replace(&self, slot: impl FnOnce(&mut P) -> &mut Obj, o: Obj) {
        let old = std::mem::replace(slot(&mut self.st()), o);
        self.release(old);
    }
}

/// The type-erased op the C++ holds.
trait HostDyn: Send + Sync {
    fn bind(&self, f: EeWonHostFn, host: *mut c_void);
    fn run(&self);
    fn callback(&self, id: i32, op: Obj) -> bool;
    fn cleanup(&self);
    fn num(&self, which: i32, index: i64) -> i64;
    fn bytes(&self, which: i32, index: i64) -> Option<Vec<u8>>;
    fn set(&self, which: i32, value: &SetValue);
    fn describe(&self) -> String;
    fn release_all(&self);
}

impl<P: Plan> HostDyn for Host<P> {
    fn bind(&self, f: EeWonHostFn, host: *mut c_void) {
        *self.bind.lock().unwrap_or_else(|e| e.into_inner()) = (f, host as usize);
    }
    fn run(&self) {
        P::run(self)
    }
    fn callback(&self, id: i32, op: Obj) -> bool {
        P::callback(self, id, op)
    }
    fn cleanup(&self) {
        P::cleanup(self)
    }
    fn num(&self, which: i32, index: i64) -> i64 {
        self.st().num(which, index)
    }
    fn bytes(&self, which: i32, index: i64) -> Option<Vec<u8>> {
        self.st().bytes(which, index)
    }
    fn set(&self, which: i32, value: &SetValue) {
        P::set(self, which, value)
    }
    fn describe(&self) -> String {
        self.st().describe()
    }
    fn release_all(&self) {
        let objs = self.st().take_objects();
        for o in objs {
            self.release(o);
        }
    }
}

/// An op of the Rust op base, as the C++ `RustServerOp` holds it.
pub struct EeWonHostOp {
    op: Box<dyn HostDyn>,
}

impl EeWonHostOp {
    pub(crate) fn boxed<P: Plan>(plan: P) -> *mut EeWonHostOp {
        Box::into_raw(Box::new(EeWonHostOp { op: Box::new(Host::new(plan)) }))
    }
}

/// Binds the C++ side's function (`host` its first argument) before any other call.
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_bind(op: *const EeWonHostOp, f: EeWonHostFn, host: *mut c_void) {
    // SAFETY: the caller's live op
    unsafe { (*op).op.bind(f, host) }
}

/// `RunHook`.
///
/// # Safety
/// `op` is a live, bound op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_run(op: *const EeWonHostOp) {
    // SAFETY: as above
    unsafe { (*op).op.run() }
}

/// `CallbackHook(done, id)` for a tracked op, or a completion this op made (a timer's, a repeating receive's) with
/// its id: whether the op knew the id.
///
/// # Safety
/// `op` is a live, bound op; `done` the finished C++ op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_callback(op: *const EeWonHostOp, id: i32, done: *mut c_void) -> bool {
    // SAFETY: as above
    unsafe { (*op).op.callback(id, Obj::of(done)) }
}

/// `CleanupHook`.
///
/// # Safety
/// `op` is a live, bound op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_cleanup(op: *const EeWonHostOp) {
    // SAFETY: as above
    unsafe { (*op).op.cleanup() }
}

/// A result by number (the op's `EE_WON_*` constants).
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_num(op: *const EeWonHostOp, which: i32, index: i64) -> i64 {
    // SAFETY: as above
    unsafe { (*op).op.num(which, index) }
}

/// A result's bytes by number, to `sink`: false (the sink not called) for none.
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_bytes(
    op: *const EeWonHostOp,
    which: i32,
    index: i64,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    // SAFETY: as above
    match unsafe { (*op).op.bytes(which, index) } {
        Some(b) => {
            // SAFETY: the sink's contract
            unsafe { ffi::give(sink, context, &b) };
            true
        }
        None => false,
    }
}

/// A setter by number: the value as a number, bytes (`data`, `len`), an address's six bytes (`addr`, NULL: none) or
/// an object (the op takes its own reference).
///
/// # Safety
/// `op` is a live, bound op; `data` holds `len` bytes; `addr` is NULL or six bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_set(
    op: *const EeWonHostOp,
    which: i32,
    num: i64,
    data: *const u8,
    len: usize,
    addr: *const u8,
    obj: *mut c_void,
) {
    let mut a = [0u8; 6];
    if !addr.is_null() {
        // SAFETY: six bytes (the contract)
        a.copy_from_slice(unsafe { std::slice::from_raw_parts(addr, 6) });
    }
    // SAFETY: as above
    let data = unsafe { ffi::bytes(data, len) };
    let v = SetValue { num, data, addr: a, obj: Obj::of(obj) };
    // SAFETY: as above
    unsafe { (*op).op.set(which, &v) }
}

/// The members, one line (the drivers' form).
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_describe(op: *const EeWonHostOp, sink: EeWonByteSink, context: *mut c_void) {
    // SAFETY: as above
    let d = unsafe { (*op).op.describe() };
    // SAFETY: the sink's contract
    unsafe { ffi::give(sink, context, d.as_bytes()) }
}

/// Frees the op, releasing every object it holds through the bound function first.
///
/// # Safety
/// `op` is NULL or an op from one of the constructors, not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_free(op: *mut EeWonHostOp) {
    if op.is_null() {
        return;
    }
    // SAFETY: the constructors' box
    let b = unsafe { Box::from_raw(op) };
    b.op.release_all();
}

/// Six bytes as hex (the drivers' form).
pub(crate) fn hex(d: &[u8]) -> String {
    if d.is_empty() {
        return "-".into();
    }
    d.iter().map(|b| format!("{b:02x}")).collect()
}

#[cfg(test)]
#[path = "hostop_tests.rs"]
mod tests;
