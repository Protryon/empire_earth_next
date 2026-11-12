//! `DetectFirewallOp` (WONMisc/DetectFirewallOp.cpp) as a Rust object: the op's members (the listen port, the
//! longest connect wait, the listening socket, the listen, wait-for-connect and UDP flags, the datagram receive op,
//! the public address the server saw) and every step: `RunHook`'s socket, `CallbackHook`'s datagram rule,
//! `CleanupHook`'s close, `GetNextRequest`, `CheckResponse` and `RecvServerDatagram` (the messages and the wait
//! are `miscops.rs`'s).
//!
//! The C++ class stays a `ServerRequestOp` (the base's exchange calls these steps through its virtuals; its
//! `mLengthFieldSize` and `mMaxRecvTime` stay the base's). The socket and receive ops are C++ objects reached through
//! [`EeWonFirewallHooks`]. What can complete the op or call back into it (`Finish`, the base class's hooks,
//! `Track` with `RunAsync`, the virtual `RecvServerDatagram`) is the C++ side's next statement after a call returns:
//! no step here calls back into the op.
//!
//! The behaviour is `golden_fwop.txt` (sequences over the original's statements, recorded first). Reached by
//! nothing in the game (the WON servers are gone; the lobby starts it only for an Internet login).

// c_long is 32 bits on Windows: its casts from and to i32 are needed elsewhere
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_long, c_void};
use std::ptr;

use crate::ffi::EeWonByteSink;
use crate::miscops::{firewall_reply, firewall_request, firewall_wait, FirewallReply};

/// A C++ `RefCount*` (NULL for none).
type Obj = *mut c_void;

const ST_SUCCESS: i32 = 0;
const ST_RECV: i32 = 100059;
const MODE_ASYNC: i32 = 1;

/// The C++ side: `context` is the op; objects are `RefCount*` (a new one carries one reference for the caller).
pub(crate) trait Env {
    fn release(&mut self, o: Obj);
    /// `new BlockingSocket(udp ? 1 : 0)`
    fn new_socket(&mut self, udp: bool) -> Obj;
    /// `socket->Fn2B0D90(port, 0)` (the bind)
    fn bind(&mut self, socket: Obj, port: u16) -> i32;
    /// `socket->Listen(1000)`
    fn listen(&mut self, socket: Obj) -> i32;
    /// `socket->GetLocalPort()`
    fn local_port(&mut self, socket: Obj) -> u16;
    /// `socket->Close()`
    fn close(&mut self, socket: Obj);
    /// `new RecvBytesFromOp(255, socket)` (the socket may be NULL)
    fn new_recv(&mut self, socket: Obj) -> Obj;
    /// `recv->Run(2, wait)` (blocking)
    fn recv_run(&mut self, recv: Obj, wait: u32) -> i32;
    /// `recv->GetAddr().GetHost() == GetCurAddr().GetHost()`
    fn recv_from_current(&mut self, recv: Obj) -> bool;
    /// `TimeLeft()`
    fn time_left(&mut self) -> u32;
    /// `GetMode()`
    fn mode(&mut self) -> i32;
    /// `mRequest = new ByteBuffer(message)`
    fn set_request(&mut self, message: &[u8]);
    /// `mResponse`'s bytes
    fn response(&mut self) -> Vec<u8>;
    /// `return InvalidReplyHeader()`
    fn invalid_header(&mut self) -> i32;
    /// the public address set from a host (a driver logs it; the op keeps it)
    fn set_public(&mut self, host: i32);
}

/// `CallbackHook`'s outcome.
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Callback {
    /// `return ServerRequestOp::CallbackHook(op, param)`
    Base,
    /// `RecvServerDatagram(); return true`
    Datagram,
    /// `Finish(status); return true`
    Finish(i32),
}

/// `CheckResponse`'s outcome (a read past the end is `Err`).
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Check {
    Status(i32),
    /// `return RecvServerDatagram()`
    Datagram,
}

/// `RecvServerDatagram`'s outcome.
#[derive(Debug, PartialEq, Eq)]
pub(crate) enum Datagram {
    /// `Track(mRecvBytesFromOp, 1000); mRecvBytesFromOp->RunAsync(wait); return WS_ServerReq_ExitCommunicationLoop`
    Async(u32),
    Status(i32),
}

/// A read past the reply's end: the C++ throws `ReadBufferException`.
#[derive(Debug, PartialEq, Eq)]
pub(crate) struct Cut;

/// The op's members.
pub(crate) struct FirewallOp {
    pub(crate) listen_port: u16,
    pub(crate) max_wait: u32,
    pub(crate) socket: Obj,
    pub(crate) do_listen: bool,
    pub(crate) wait_for_connect: bool,
    pub(crate) use_udp: bool,
    pub(crate) recv: Obj,
    /// `mPublicAddr`'s host (set with port 0), None while unset
    pub(crate) public: Option<i32>,
}

/// `slot = new X(...)`: `o` carries the reference the slot keeps.
fn adopt<E: Env>(env: &mut E, slot: &mut Obj, o: Obj) {
    let old = std::mem::replace(slot, o);
    if !old.is_null() {
        env.release(old);
    }
}

impl FirewallOp {
    /// `Init`: listen on any port over TCP, wait for the connect, no longest wait.
    pub(crate) fn new() -> FirewallOp {
        FirewallOp {
            listen_port: 0,
            max_wait: 0,
            socket: ptr::null_mut(),
            do_listen: true,
            wait_for_connect: true,
            use_udp: false,
            recv: ptr::null_mut(),
            public: None,
        }
    }

    /// The destructor's releases.
    pub(crate) fn release_all<E: Env>(&mut self, env: &mut E) {
        for o in [&mut self.recv, &mut self.socket] {
            let old = std::mem::replace(o, ptr::null_mut());
            if !old.is_null() {
                env.release(old);
            }
        }
    }

    /// `RunHook` up to `ServerRequestOp::RunHook`: Some(status) to `Finish` with instead.
    pub(crate) fn run_hook<E: Env>(&mut self, env: &mut E) -> Option<i32> {
        if self.do_listen || self.use_udp {
            let s = env.new_socket(self.use_udp);
            adopt(env, &mut self.socket, s);
            let status = env.bind(self.socket, self.listen_port);
            if status != ST_SUCCESS {
                return Some(status);
            }
            if !self.use_udp {
                let status = env.listen(self.socket);
                if status != ST_SUCCESS {
                    return Some(status);
                }
            }
            self.listen_port = env.local_port(self.socket);
        }
        None
    }

    /// `CallbackHook(op, param)` with the completed op's status. Without a receive op (the original dereferenced
    /// NULL) a success finishes.
    pub(crate) fn callback_hook<E: Env>(&mut self, env: &mut E, param: i32, status: i32) -> Callback {
        if param != 1000 {
            return Callback::Base;
        }
        if status == ST_SUCCESS && !self.recv.is_null() && !env.recv_from_current(self.recv) {
            return Callback::Datagram;
        }
        Callback::Finish(status)
    }

    /// `CleanupHook` after `ServerRequestOp::CleanupHook`.
    pub(crate) fn cleanup_hook<E: Env>(&mut self, env: &mut E) {
        if !self.socket.is_null() {
            env.close(self.socket);
            let s = std::mem::replace(&mut self.socket, ptr::null_mut());
            env.release(s);
        }
    }

    /// `GetNextRequest` (`lfs` the base's length field size).
    pub(crate) fn get_next_request<E: Env>(&mut self, env: &mut E, lfs: u8) -> i32 {
        env.set_request(&firewall_request(lfs, self.use_udp, self.listen_port, self.wait_for_connect, self.max_wait));
        ST_RECV
    }

    /// `CheckResponse`.
    pub(crate) fn check_response<E: Env>(&mut self, env: &mut E) -> Result<Check, Cut> {
        match firewall_reply(&env.response()) {
            Err(_) => Err(Cut),
            Ok(FirewallReply::BadHeader) => Ok(Check::Status(env.invalid_header())),
            Ok(FirewallReply::Status(status, host)) => {
                if let Some(h) = host {
                    self.public = Some(h);
                    env.set_public(h);
                }
                if self.use_udp {
                    Ok(Check::Datagram)
                } else {
                    Ok(Check::Status(status as i32))
                }
            }
        }
    }

    /// `RecvServerDatagram`: waits for a datagram from the server's host on the listening socket.
    pub(crate) fn recv_datagram<E: Env>(&mut self, env: &mut E) -> Datagram {
        if env.mode() == MODE_ASYNC {
            let r = env.new_recv(self.socket);
            adopt(env, &mut self.recv, r);
            return Datagram::Async(firewall_wait(env.time_left(), self.max_wait, true));
        }
        loop {
            let r = env.new_recv(self.socket);
            adopt(env, &mut self.recv, r);
            let wait = firewall_wait(env.time_left(), self.max_wait, false);
            let status = env.recv_run(self.recv, wait);
            if status != ST_SUCCESS {
                return Datagram::Status(status);
            }
            if env.recv_from_current(self.recv) {
                return Datagram::Status(ST_SUCCESS);
            }
        }
    }
}

// ---- the C ABI

unsafe extern "C" fn vec_sink(context: *mut c_void, bytes: *const u8, length: usize) {
    let v = unsafe { &mut *(context as *mut Vec<u8>) };
    if length > 0 && !bytes.is_null() {
        v.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

/// The C++ side of a `DetectFirewallOp`: `context` is the op; objects are `RefCount*`. `set_public` is NULL for
/// the real op (the address is this object's).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonFirewallHooks {
    pub context: *mut c_void,
    pub release: unsafe extern "C" fn(*mut c_void),
    pub new_socket: unsafe extern "C" fn(*mut c_void, bool) -> *mut c_void,
    pub bind: unsafe extern "C" fn(*mut c_void, *mut c_void, u16) -> c_long,
    pub listen: unsafe extern "C" fn(*mut c_void, *mut c_void) -> c_long,
    pub local_port: unsafe extern "C" fn(*mut c_void, *mut c_void) -> u16,
    pub close: unsafe extern "C" fn(*mut c_void, *mut c_void),
    pub new_recv: unsafe extern "C" fn(*mut c_void, *mut c_void) -> *mut c_void,
    pub recv_run: unsafe extern "C" fn(*mut c_void, *mut c_void, u32) -> c_long,
    pub recv_from_current: unsafe extern "C" fn(*mut c_void, *mut c_void) -> bool,
    pub time_left: unsafe extern "C" fn(*mut c_void) -> u32,
    pub mode: unsafe extern "C" fn(*mut c_void) -> i32,
    pub set_request: unsafe extern "C" fn(*mut c_void, *const u8, usize),
    pub response: unsafe extern "C" fn(*mut c_void, EeWonByteSink, *mut c_void),
    pub invalid_header: unsafe extern "C" fn(*mut c_void) -> c_long,
    pub set_public: Option<unsafe extern "C" fn(*mut c_void, i32)>,
}

/// `DetectFirewallOp`'s state.
pub struct EeWonFirewallOp {
    op: FirewallOp,
}

struct FfiEnv(EeWonFirewallHooks);

impl Env for FfiEnv {
    fn release(&mut self, o: Obj) {
        unsafe { (self.0.release)(o) }
    }
    fn new_socket(&mut self, udp: bool) -> Obj {
        unsafe { (self.0.new_socket)(self.0.context, udp) }
    }
    fn bind(&mut self, socket: Obj, port: u16) -> i32 {
        unsafe { (self.0.bind)(self.0.context, socket, port) as i32 }
    }
    fn listen(&mut self, socket: Obj) -> i32 {
        unsafe { (self.0.listen)(self.0.context, socket) as i32 }
    }
    fn local_port(&mut self, socket: Obj) -> u16 {
        unsafe { (self.0.local_port)(self.0.context, socket) }
    }
    fn close(&mut self, socket: Obj) {
        unsafe { (self.0.close)(self.0.context, socket) }
    }
    fn new_recv(&mut self, socket: Obj) -> Obj {
        unsafe { (self.0.new_recv)(self.0.context, socket) }
    }
    fn recv_run(&mut self, recv: Obj, wait: u32) -> i32 {
        unsafe { (self.0.recv_run)(self.0.context, recv, wait) as i32 }
    }
    fn recv_from_current(&mut self, recv: Obj) -> bool {
        unsafe { (self.0.recv_from_current)(self.0.context, recv) }
    }
    fn time_left(&mut self) -> u32 {
        unsafe { (self.0.time_left)(self.0.context) }
    }
    fn mode(&mut self) -> i32 {
        unsafe { (self.0.mode)(self.0.context) }
    }
    fn set_request(&mut self, message: &[u8]) {
        unsafe { (self.0.set_request)(self.0.context, message.as_ptr(), message.len()) }
    }
    fn response(&mut self) -> Vec<u8> {
        let mut v = Vec::new();
        unsafe { (self.0.response)(self.0.context, vec_sink, &mut v as *mut Vec<u8> as *mut c_void) };
        v
    }
    fn invalid_header(&mut self) -> i32 {
        unsafe { (self.0.invalid_header)(self.0.context) as i32 }
    }
    fn set_public(&mut self, host: i32) {
        if let Some(f) = self.0.set_public {
            unsafe { f(self.0.context, host) }
        }
    }
}

unsafe fn with<R>(
    op: *mut EeWonFirewallOp,
    hooks: *const EeWonFirewallHooks,
    f: impl FnOnce(&mut FirewallOp, &mut FfiEnv) -> R,
) -> R {
    let mut env = FfiEnv(unsafe { *hooks });
    f(unsafe { &mut (*op).op }, &mut env)
}

unsafe fn get<'a>(op: *const EeWonFirewallOp) -> &'a FirewallOp {
    unsafe { &(*op).op }
}

/// `DetectFirewallOp`'s members after `Init`.
#[no_mangle]
pub extern "C" fn ee_won_fwop_new() -> *mut EeWonFirewallOp {
    Box::into_raw(Box::new(EeWonFirewallOp { op: FirewallOp::new() }))
}

/// The destructor: the socket and the receive op let go.
///
/// # Safety
/// `op` came from `ee_won_fwop_new` (or is NULL); `hooks` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_free(op: *mut EeWonFirewallOp, hooks: *const EeWonFirewallHooks) {
    if op.is_null() {
        return;
    }
    unsafe { with(op, hooks, |o, e| o.release_all(e)) };
    drop(unsafe { Box::from_raw(op) });
}

/// `SetListenPort`.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_set_listen_port(op: *mut EeWonFirewallOp, port: u16) {
    unsafe { (*op).op.listen_port = port }
}

/// `SetDoListen`.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_set_do_listen(op: *mut EeWonFirewallOp, value: bool) {
    unsafe { (*op).op.do_listen = value }
}

/// `SetWaitForConnect`.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_set_wait_for_connect(op: *mut EeWonFirewallOp, value: bool) {
    unsafe { (*op).op.wait_for_connect = value }
}

/// The drivers' setup: the UDP flag and the longest connect wait (no setter sets them).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_setup(op: *mut EeWonFirewallOp, udp: bool, max_wait: u32) {
    unsafe {
        (*op).op.use_udp = udp;
        (*op).op.max_wait = max_wait;
    }
}

/// `RunHook`: true to `Finish(*status)`, false to go on to `ServerRequestOp::RunHook`.
///
/// # Safety
/// `op`, `hooks` and `status` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_run_hook(
    op: *mut EeWonFirewallOp,
    hooks: *const EeWonFirewallHooks,
    status: *mut c_long,
) -> bool {
    match unsafe { with(op, hooks, |o, e| o.run_hook(e)) } {
        Some(s) => {
            unsafe { *status = s as c_long };
            true
        }
        None => false,
    }
}

/// `CallbackHook(op, param)` with `op`'s status: 0 the base's hook, 1 `RecvServerDatagram()` then true, 2
/// `Finish(*finish)` then true.
///
/// # Safety
/// `op`, `hooks` and `finish` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_callback_hook(
    op: *mut EeWonFirewallOp,
    hooks: *const EeWonFirewallHooks,
    param: i32,
    status: c_long,
    finish: *mut c_long,
) -> i32 {
    match unsafe { with(op, hooks, |o, e| o.callback_hook(e, param, status as i32)) } {
        Callback::Base => 0,
        Callback::Datagram => 1,
        Callback::Finish(s) => {
            unsafe { *finish = s as c_long };
            2
        }
    }
}

/// `CleanupHook` after `ServerRequestOp::CleanupHook`.
///
/// # Safety
/// `op` and `hooks` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_cleanup_hook(op: *mut EeWonFirewallOp, hooks: *const EeWonFirewallHooks) {
    unsafe { with(op, hooks, |o, e| o.cleanup_hook(e)) }
}

/// `GetNextRequest` with the base's length field size.
///
/// # Safety
/// `op` and `hooks` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_get_next_request(
    op: *mut EeWonFirewallOp,
    hooks: *const EeWonFirewallHooks,
    length_field_size: u8,
) -> c_long {
    unsafe { with(op, hooks, |o, e| o.get_next_request(e, length_field_size)) as c_long }
}

/// `CheckResponse`: `*kind` -1 cut short (the caller throws `ReadBufferException`), 1 `return
/// RecvServerDatagram()`, 0 return the status.
///
/// # Safety
/// `op`, `hooks` and `kind` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_check_response(
    op: *mut EeWonFirewallOp,
    hooks: *const EeWonFirewallHooks,
    kind: *mut i32,
) -> c_long {
    let (k, s) = match unsafe { with(op, hooks, |o, e| o.check_response(e)) } {
        Err(Cut) => (-1, 0),
        Ok(Check::Datagram) => (1, 0),
        Ok(Check::Status(s)) => (0, s),
    };
    unsafe { *kind = k };
    s as c_long
}

/// `RecvServerDatagram`: true to `Track` the receive op (`ee_won_fwop_recv_op`) as 1000, `RunAsync(*wait)` it and
/// return `WS_ServerReq_ExitCommunicationLoop`; false to return `*status`.
///
/// # Safety
/// `op`, `hooks`, `wait` and `status` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_recv_datagram(
    op: *mut EeWonFirewallOp,
    hooks: *const EeWonFirewallHooks,
    wait: *mut u32,
    status: *mut c_long,
) -> bool {
    match unsafe { with(op, hooks, |o, e| o.recv_datagram(e)) } {
        Datagram::Async(w) => {
            unsafe { *wait = w };
            true
        }
        Datagram::Status(s) => {
            unsafe { *status = s as c_long };
            false
        }
    }
}

/// The receive op (borrowed, or NULL).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_recv_op(op: *const EeWonFirewallOp) -> *mut c_void {
    unsafe { get(op) }.recv
}

/// The listening socket (borrowed, or NULL).
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_listen_socket(op: *const EeWonFirewallOp) -> *mut c_void {
    unsafe { get(op) }.socket
}

/// `GetPublicAddr`'s host into `*host`: false while unset.
///
/// # Safety
/// `op` and `host` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_public_host(op: *const EeWonFirewallOp, host: *mut i32) -> bool {
    match unsafe { get(op) }.public {
        Some(h) => {
            unsafe { *host = h };
            true
        }
        None => false,
    }
}

/// The listen port.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_listen_port(op: *const EeWonFirewallOp) -> u16 {
    unsafe { get(op) }.listen_port
}

/// The longest connect wait.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_max_wait(op: *const EeWonFirewallOp) -> u32 {
    unsafe { get(op) }.max_wait
}

/// The listen flag.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_do_listen(op: *const EeWonFirewallOp) -> bool {
    unsafe { get(op) }.do_listen
}

/// The wait-for-connect flag.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_wait_for_connect(op: *const EeWonFirewallOp) -> bool {
    unsafe { get(op) }.wait_for_connect
}

/// The UDP flag.
///
/// # Safety
/// `op` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fwop_use_udp(op: *const EeWonFirewallOp) -> bool {
    unsafe { get(op) }.use_udp
}

#[cfg(test)]
#[path = "fwop_tests.rs"]
mod tests;
