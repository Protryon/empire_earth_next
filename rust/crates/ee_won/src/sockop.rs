//! WONSocket's `SocketOp` base and `RecvCRMsgOp` as Rust objects (round 14). Every socket op's base state lives
//! here: its socket (the C++ `AsyncSocket`, holding a reference, and its [`EeWonAsyncSocket`]), the socket thread
//! flag and the wait flags and event indices; so does `SocketOp::RunHook` (start, then the socket thread or the
//! blocking continue). `RecvCRMsgOp`'s line buffer and its start and continue steps are here as well. The C++
//! classes are shims; the virtual `Start` / `Continue`, `Finish`, the run mode, the socket thread's add and the
//! socket's references come back as the [`EeWonSocketOpBaseHooks`]. The behaviour is `golden_sockobj.txt`, recorded
//! over the C++ classes.

use std::cell::UnsafeCell;
use std::ffi::{c_long, c_void};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicBool, AtomicI32, AtomicPtr, Ordering::Relaxed};

use crate::buffer::EeWonByteBuffer;
use crate::ops::{self, OpTiming};
use crate::sockobj::EeWonAsyncSocket;
use crate::status::*;

/// A C++ call on a socket op (`SocketOp*`).
pub type EeWonSocketOpBaseFn = Option<unsafe extern "C" fn(op: *mut c_void)>;
/// A C++ call on a socket (`AsyncSocket*`).
pub type EeWonSocketRefFn = Option<unsafe extern "C" fn(socket: *mut c_void)>;

/// What the op object calls back in the C++. One static table serves every socket op.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonSocketOpBaseHooks {
    /// the virtual `Start()` and `Continue()`
    pub start: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    pub continue_op: Option<unsafe extern "C" fn(op: *mut c_void) -> c_long>,
    /// `Finish(status)`
    pub finish: Option<unsafe extern "C" fn(op: *mut c_void, status: c_long)>,
    /// `GetMode()`
    pub mode: Option<unsafe extern "C" fn(op: *mut c_void) -> i32>,
    /// `WONAPICore::AddToSocketThread(op)`
    pub add_to_socket_thread: EeWonSocketOpBaseFn,
    /// `socket->AddRef()`, `socket->Release()`
    pub socket_add_ref: EeWonSocketRefFn,
    pub socket_release: EeWonSocketRefFn,
    /// the socket's Rust object (`socket->mSock`)
    pub socket_object: Option<unsafe extern "C" fn(socket: *mut c_void) -> *mut EeWonAsyncSocket>,
}

/// `SocketOp`'s state (and `RecvCRMsgOp`'s line).
pub struct EeWonSocketOp {
    /// the C++ `SocketOp*` the hooks get
    op: *mut c_void,
    hooks: &'static EeWonSocketOpBaseHooks,
    /// `mSocket` (the C++ `AsyncSocket*`, a reference held) and its object
    socket: AtomicPtr<c_void>,
    socket_obj: AtomicPtr<EeWonAsyncSocket>,
    in_socket_thread: AtomicBool,
    wait_for_event: [AtomicBool; 3],
    event_index: [AtomicI32; 3],
    /// `RecvCRMsgOp::mBuffer`: the line so far (the op runs on one thread at a time: its runner or the socket thread)
    line: UnsafeCell<Vec<u8>>,
}

// SAFETY: the fields are atomics; the line is touched by the op's one runner at a time (the C++ contract)
unsafe impl Send for EeWonSocketOp {}
// SAFETY: as Send
unsafe impl Sync for EeWonSocketOp {}

impl EeWonSocketOp {
    /// `SocketOp(socket)`: no socket thread, no waits, the event indices -1; the socket is set by the caller.
    pub fn new(op: *mut c_void, hooks: &'static EeWonSocketOpBaseHooks) -> EeWonSocketOp {
        EeWonSocketOp {
            op,
            hooks,
            socket: AtomicPtr::new(null_mut()),
            socket_obj: AtomicPtr::new(null_mut()),
            in_socket_thread: AtomicBool::new(false),
            wait_for_event: [AtomicBool::new(false), AtomicBool::new(false), AtomicBool::new(false)],
            event_index: [AtomicI32::new(-1), AtomicI32::new(-1), AtomicI32::new(-1)],
            line: UnsafeCell::new(Vec::new()),
        }
    }

    pub fn socket(&self) -> *mut c_void {
        self.socket.load(Relaxed)
    }

    /// The socket's object (none without a socket).
    pub fn socket_object(&self) -> Option<&EeWonAsyncSocket> {
        let s = self.socket_obj.load(Relaxed);
        // SAFETY: the reference this op holds keeps the C++ socket, and with it its object, alive
        if s.is_null() {
            None
        } else {
            Some(unsafe { &*s })
        }
    }

    /// `mSocket = socket`: the new one's reference first, then the old one's goes (a `SmartPtr` assignment).
    ///
    /// # Safety
    /// `socket` is null or a live C++ socket.
    pub unsafe fn set_socket(&self, socket: *mut c_void) {
        let mut obj = null_mut();
        if !socket.is_null() {
            if let Some(h) = self.hooks.socket_add_ref {
                // SAFETY: the caller's live socket
                unsafe { h(socket) };
            }
            if let Some(h) = self.hooks.socket_object {
                // SAFETY: as above
                obj = unsafe { h(socket) };
            }
        }
        self.socket_obj.store(obj, Relaxed);
        let old = self.socket.swap(socket, Relaxed);
        if !old.is_null() {
            if let Some(h) = self.hooks.socket_release {
                // SAFETY: the reference this op held
                unsafe { h(old) };
            }
        }
    }

    pub fn in_socket_thread(&self) -> bool {
        self.in_socket_thread.load(Relaxed)
    }
    pub fn set_in_socket_thread(&self, on: bool) {
        self.in_socket_thread.store(on, Relaxed);
    }
    pub fn wait_for_event(&self, i: usize) -> bool {
        self.wait_for_event[i].load(Relaxed)
    }
    pub fn set_wait_for_event(&self, i: usize, on: bool) {
        self.wait_for_event[i].store(on, Relaxed);
    }
    pub fn event_index(&self, i: usize) -> i32 {
        self.event_index[i].load(Relaxed)
    }
    pub fn set_event_index(&self, i: usize, index: i32) {
        self.event_index[i].store(index, Relaxed);
    }

    fn status_call(&self, hook: Option<unsafe extern "C" fn(*mut c_void) -> c_long>) -> Status {
        // SAFETY: the hooks' contract: `op` is the live C++ op
        hook.map_or(WS_SUCCESS, |h| unsafe { h(self.op) } as Status)
    }

    fn finish(&self, status: Status) {
        if let Some(h) = self.hooks.finish {
            // SAFETY: the hooks' contract
            unsafe { h(self.op, status as c_long) };
        }
    }

    /// `SocketOp::RunHook`: finished with `WS_SocketOp_InvalidSocket` first when there is no socket (and started
    /// anyway, as the original); a start that waits (`WS_TimedOut`) goes to the socket thread (async) or is continued
    /// and finished (blocking); any other start finishes the op.
    pub fn run_hook(&self) {
        if self.socket().is_null() {
            self.finish(WS_SOCKET_OP_INVALID_SOCKET);
        }
        let s = self.status_call(self.hooks.start);
        if s != WS_TIMED_OUT {
            self.finish(s);
            return;
        }
        // SAFETY: the hooks' contract
        let mode = self.hooks.mode.map_or(0, |h| unsafe { h(self.op) });
        if mode == 1 {
            if let Some(h) = self.hooks.add_to_socket_thread {
                // SAFETY: the hooks' contract
                unsafe { h(self.op) };
            }
        } else {
            let s = self.status_call(self.hooks.continue_op);
            self.finish(s);
        }
    }

    #[allow(clippy::mut_from_ref)]
    fn line(&self) -> &mut Vec<u8> {
        // SAFETY: the op's one runner at a time; no borrow lives across a hook
        unsafe { &mut *self.line.get() }
    }

    /// `RecvCRMsgOp::Start`'s buffer reset.
    pub fn line_reset(&self) {
        self.line().clear();
    }

    /// `RecvCRMsgOp::Continue`: one byte at a time until CR-LF (`ops::recv_line`); on success the line, its CR made a
    /// NUL and its LF dropped, is handed over as the message (and the buffer is empty again).
    pub fn line_continue(&self, timing: OpTiming) -> (Status, Option<EeWonByteBuffer>) {
        let Some(sock) = self.socket_object() else {
            return (WS_ASYNC_SOCKET_INVALID_SOCKET, None);
        };
        let line = self.line();
        let mut last = line.last().map_or(-1, |&b| b as i32);
        let s = ops::recv_line(&sock.locked(), &mut last, timing, |b| line.push(b));
        if s != WS_SUCCESS {
            return (s, None);
        }
        let n = line.len();
        line[n - 2] = 0;
        line.truncate(n - 1);
        let message = EeWonByteBuffer::copy(line);
        line.clear();
        (s, Some(message))
    }
}

impl Drop for EeWonSocketOp {
    fn drop(&mut self) {
        // the socket's reference (the C++ SmartPtr member's destructor)
        // SAFETY: null
        unsafe { self.set_socket(null_mut()) };
    }
}

// ---- the C ABI

/// A new op object for the C++ `op`.
///
/// # Safety
/// `hooks` is a static table; `op` outlives the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_new(
    op: *mut c_void,
    hooks: *const EeWonSocketOpBaseHooks,
) -> *mut EeWonSocketOp {
    // SAFETY: the caller's static table
    Box::into_raw(Box::new(EeWonSocketOp::new(op, unsafe { &*hooks })))
}

/// Frees the op object (the socket's reference goes).
///
/// # Safety
/// `o` came from `ee_won_socketop_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_free(o: *mut EeWonSocketOp) {
    if !o.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(o) });
    }
}

macro_rules! op {
    ($o:expr) => {
        // SAFETY: the callers pass a live op object
        unsafe { &*$o }
    };
}

/// `GetSocket`.
///
/// # Safety
/// `o` is a live op object (so for every `ee_won_socketop_` call).
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_socket(o: *const EeWonSocketOp) -> *mut c_void {
    op!(o).socket()
}
/// `SetSocket`.
///
/// # Safety
/// As `ee_won_socketop_socket`; `socket` is null or a live C++ socket.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_socket(o: *const EeWonSocketOp, socket: *mut c_void) {
    let o = op!(o);
    // SAFETY: the caller's contract
    unsafe { o.set_socket(socket) }
}
/// `mInSocketThread`.
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_in_socket_thread(o: *const EeWonSocketOp) -> bool {
    op!(o).in_socket_thread()
}
/// `mInSocketThread = on`.
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_in_socket_thread(o: *const EeWonSocketOp, on: bool) {
    op!(o).set_in_socket_thread(on)
}
/// `mWaitForEvent[i]` (i < 3).
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_wait_for_event(o: *const EeWonSocketOp, i: i32) -> bool {
    op!(o).wait_for_event(i as usize)
}
/// `mWaitForEvent[i] = on`.
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_wait_for_event(o: *const EeWonSocketOp, i: i32, on: bool) {
    op!(o).set_wait_for_event(i as usize, on)
}
/// `mEventIndex[i]`.
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_event_index(o: *const EeWonSocketOp, i: i32) -> i32 {
    op!(o).event_index(i as usize)
}
/// `mEventIndex[i] = index`.
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_event_index(o: *const EeWonSocketOp, i: i32, index: i32) {
    op!(o).set_event_index(i as usize, index)
}
/// `SocketOp::RunHook`.
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_run_hook(o: *const EeWonSocketOp) {
    op!(o).run_hook()
}
/// `RecvCRMsgOp::Start`'s buffer reset.
///
/// # Safety
/// As `ee_won_socketop_socket`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_line_reset(o: *const EeWonSocketOp) {
    op!(o).line_reset()
}
/// `RecvCRMsgOp::Continue`: the status, and on success the message in `*message` (a new `EeWonByteBuffer` the
/// C++ `ByteBuffer` adopts; null otherwise).
///
/// # Safety
/// As `ee_won_socketop_socket`; `message` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_line_continue(
    o: *const EeWonSocketOp,
    timing: OpTiming,
    message: *mut *mut EeWonByteBuffer,
) -> c_long {
    let (s, m) = op!(o).line_continue(timing);
    // SAFETY: the caller's out-parameter
    unsafe { *message = m.map_or(null_mut(), |m| Box::into_raw(Box::new(m))) };
    s as c_long
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ipaddr::Addr;
    use crate::socket::{self, KIND_STREAM};
    use crate::sockobj::{EeWonAsyncSocket, EeWonAsyncSocketHooks};
    use std::sync::Mutex;

    static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
    fn log(s: String) {
        LOG.lock().unwrap().push(s);
    }

    // the "C++ op" is a script: what Start and Continue return and the mode
    struct Script {
        start: Status,
        cont: Status,
        mode: i32,
    }
    unsafe extern "C" fn start(op: *mut c_void) -> c_long {
        let s = unsafe { &*(op as *const Script) };
        log(format!("start {}", s.start));
        s.start as c_long
    }
    unsafe extern "C" fn cont(op: *mut c_void) -> c_long {
        let s = unsafe { &*(op as *const Script) };
        log(format!("continue {}", s.cont));
        s.cont as c_long
    }
    unsafe extern "C" fn finish(_op: *mut c_void, status: c_long) {
        log(format!("finish {status}"));
    }
    unsafe extern "C" fn mode(op: *mut c_void) -> i32 {
        unsafe { &*(op as *const Script) }.mode
    }
    unsafe extern "C" fn add(_op: *mut c_void) {
        log("socket thread".into());
    }
    unsafe extern "C" fn add_ref(s: *mut c_void) {
        log(format!("addref {}", s as usize));
    }
    unsafe extern "C" fn release(s: *mut c_void) {
        log(format!("release {}", s as usize));
    }
    // the "C++ socket" is its object
    unsafe extern "C" fn object(s: *mut c_void) -> *mut EeWonAsyncSocket {
        s.cast()
    }
    static HOOKS: EeWonSocketOpBaseHooks = EeWonSocketOpBaseHooks {
        start: Some(start),
        continue_op: Some(cont),
        finish: Some(finish),
        mode: Some(mode),
        add_to_socket_thread: Some(add),
        socket_add_ref: Some(add_ref),
        socket_release: Some(release),
        socket_object: Some(object),
    };

    fn run(script: Script, socket: usize) -> Vec<String> {
        let s = Box::leak(Box::new(script));
        let o = EeWonSocketOp::new(s as *mut Script as *mut c_void, &HOOKS);
        LOG.lock().unwrap().clear();
        unsafe { o.set_socket(socket as *mut c_void) };
        o.run_hook();
        drop(o);
        std::mem::take(&mut *LOG.lock().unwrap())
    }

    #[test]
    fn run_hook_paths() {
        let _guard = LOCK.lock().unwrap();
        // finished at the start
        assert_eq!(run(Script { start: 0, cont: 0, mode: 2 }, 8), ["addref 8", "start 0", "finish 0", "release 8"]);
        // waiting: blocking continues and finishes, async goes to the socket thread
        assert_eq!(
            run(Script { start: WS_TIMED_OUT, cont: 5, mode: 2 }, 8),
            ["addref 8", "start 100001", "continue 5", "finish 5", "release 8"]
        );
        assert_eq!(
            run(Script { start: WS_TIMED_OUT, cont: 5, mode: 1 }, 8),
            ["addref 8", "start 100001", "socket thread", "release 8"]
        );
        // no socket: finished with InvalidSocket, then started anyway
        assert_eq!(run(Script { start: 7, cont: 0, mode: 2 }, 0), ["finish 100022", "start 7", "finish 7"]);
    }

    static LOCK: Mutex<()> = Mutex::new(());

    #[test]
    fn the_state_and_the_socket_reference() {
        let _guard = LOCK.lock().unwrap();
        let o = EeWonSocketOp::new(null_mut(), &HOOKS);
        assert!(!o.in_socket_thread() && !o.wait_for_event(2) && o.event_index(1) == -1);
        o.set_in_socket_thread(true);
        o.set_wait_for_event(2, true);
        o.set_event_index(1, 4);
        assert!(o.in_socket_thread() && o.wait_for_event(2) && o.event_index(1) == 4);
        LOG.lock().unwrap().clear();
        unsafe {
            o.set_socket(0x10 as *mut c_void);
            o.set_socket(0x20 as *mut c_void);
        }
        drop(o);
        assert_eq!(*LOG.lock().unwrap(), ["addref 16", "addref 32", "release 16", "release 32"]);
        LOG.lock().unwrap().clear();
    }

    #[test]
    fn a_line_over_loopback() {
        let _guard = LOCK.lock().unwrap();
        static QUIET: EeWonAsyncSocketHooks = EeWonAsyncSocketHooks {
            open_hook: None,
            close_hook: None,
            kill_hook: None,
            transform_send: None,
            transform_recv: None,
            transform_free: None,
        };
        socket::start();
        let local = |p| Addr::from_host(0x7f00_0001, p);
        let l = EeWonAsyncSocket::new(null_mut(), &QUIET, KIND_STREAM);
        assert_eq!(l.bind(local(0), true), WS_SUCCESS);
        assert_eq!(l.listen(1), WS_SUCCESS);
        let c = EeWonAsyncSocket::new(null_mut(), &QUIET, KIND_STREAM);
        c.connect_prepare(0);
        c.connect(local(l.local_port()));
        assert_eq!(c.wait_for_connect(5000), WS_SUCCESS);
        l.wait(socket::WAIT_READ, 5000);
        let (h, peer) = l.accept().unwrap();
        let s = Box::leak(Box::new(EeWonAsyncSocket::new(null_mut(), &QUIET, KIND_STREAM)));
        s.adopt(h, peer);
        let mut n = 0;
        assert_eq!(c.send(b"a\nb\r\npar", &mut n), WS_SUCCESS);
        let o = EeWonSocketOp::new(null_mut(), &HOOKS);
        unsafe { o.set_socket(s as *mut EeWonAsyncSocket as *mut c_void) };
        let blocking = OpTiming { run_mode: 2, start: ops::tick(), timeout: 5000 };
        let (st, m) = o.line_continue(blocking);
        assert_eq!((st, m.map(|m| m.bytes().to_vec())), (WS_SUCCESS, Some(b"a\nb\0".to_vec())));
        // a partial line stays for the next continue
        let short = OpTiming { run_mode: 2, start: ops::tick(), timeout: 50 };
        assert_eq!(o.line_continue(short).0, WS_TIMED_OUT);
        assert_eq!(c.send(b"t\r\n", &mut n), WS_SUCCESS);
        let (st, m) = o.line_continue(blocking);
        assert_eq!((st, m.map(|m| m.bytes().to_vec())), (WS_SUCCESS, Some(b"part\0".to_vec())));
        o.line_reset();
        let none = EeWonSocketOp::new(null_mut(), &HOOKS);
        assert_eq!(none.line_continue(blocking).0, WS_ASYNC_SOCKET_INVALID_SOCKET);
        drop((o, none));
        LOG.lock().unwrap().clear();
    }
}
