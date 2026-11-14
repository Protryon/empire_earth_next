//! The socket and op calls over a stand-in C++ side: its objects are real socket and socket op objects (sockets on
//! 127.0.0.1), and its one answer is `EE_WON_HOST_SOCK_CORE` (by the object's kind) plus the calls a blocking call
//! sends (`NEW_OP`, `RELEASE`), each logged. The answers against the original's are `won_hostsock_test`'s.

use super::*;
use crate::buffer::EeWonByteBuffer;
use crate::ipobj::EeWonIpAddr;
use crate::ops::{self, OpTiming};
use crate::socket::{self as sk, KIND_STREAM};
use crate::sockobj::EeWonAsyncSocketHooks;
use crate::sockop::EeWonSocketOpBaseHooks;
use crate::status::*;
use std::cell::RefCell;
use std::ptr::null_mut;
use std::sync::atomic::{AtomicI32, Ordering::SeqCst};
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());

/// A stand-in C++ object: a socket, a socket op or a byte buffer, with its count.
enum Kind {
    Socket(EeWonAsyncSocket),
    Op(Box<EeWonSocketOp>),
    Bytes(EeWonByteBuffer),
}
struct Cpp {
    refs: AtomicI32,
    kind: Kind,
}
fn leak(kind: Kind) -> usize {
    Box::leak(Box::new(Cpp { refs: AtomicI32::new(0), kind })) as *const Cpp as usize
}
fn cpp(p: usize) -> &'static Cpp {
    unsafe { &*(p as *const Cpp) }
}
fn refs(p: usize) -> i32 {
    cpp(p).refs.load(SeqCst)
}

unsafe extern "C" fn ref_add(p: *mut c_void) {
    cpp(p as usize).refs.fetch_add(1, SeqCst);
}
unsafe extern "C" fn ref_release(p: *mut c_void) {
    cpp(p as usize).refs.fetch_sub(1, SeqCst);
}
unsafe extern "C" fn bytes_of(p: *mut c_void) -> *const EeWonByteBuffer {
    match &cpp(p as usize).kind {
        Kind::Bytes(b) => b,
        _ => std::ptr::null(),
    }
}
unsafe extern "C" fn bytes_adopt(b: *mut EeWonByteBuffer) -> *mut c_void {
    let b = unsafe { Box::from_raw(b) };
    let p = leak(Kind::Bytes(*b));
    cpp(p).refs.store(1, SeqCst);
    p as *mut c_void
}
unsafe extern "C" fn socket_object(p: *mut c_void) -> *mut EeWonAsyncSocket {
    match &cpp(p as usize).kind {
        Kind::Socket(s) => s as *const EeWonAsyncSocket as *mut EeWonAsyncSocket,
        _ => null_mut(),
    }
}

static OP_HOOKS: EeWonSocketOpBaseHooks = EeWonSocketOpBaseHooks {
    start: None,
    continue_op: None,
    finish: None,
    mode: None,
    add_to_socket_thread: None,
    socket_add_ref: Some(ref_add),
    socket_release: Some(ref_release),
    socket_object: Some(socket_object),
    ref_add: Some(ref_add),
    ref_release: Some(ref_release),
    bytes_of: Some(bytes_of),
    bytes_adopt: Some(bytes_adopt),
    transform: None,
    file_open: None,
    file_write: None,
    file_close: None,
    file_set_time: None,
    progress: None,
    accept: None,
};
static QUIET: EeWonAsyncSocketHooks = EeWonAsyncSocketHooks {
    open_hook: None,
    close_hook: None,
    kill_hook: None,
    transform_send: None,
    transform_recv: None,
    transform_free: None,
};

fn new_socket() -> usize {
    leak(Kind::Socket(EeWonAsyncSocket::new(null_mut(), &QUIET, KIND_STREAM)))
}
fn sock(p: usize) -> &'static EeWonAsyncSocket {
    match &cpp(p).kind {
        Kind::Socket(s) => s,
        _ => panic!("not a socket"),
    }
}
/// A socket op on a socket (the "C++ op" is the stand-in object).
fn new_op(socket: usize) -> usize {
    let raw = Box::into_raw(Box::new(Cpp { refs: AtomicI32::new(0), kind: Kind::Bytes(EeWonByteBuffer::copy(b"")) }));
    let o = Box::new(EeWonSocketOp::new(raw as *mut c_void, &OP_HOOKS));
    unsafe { o.set_socket(socket as *mut c_void) };
    // the stand-in becomes the op (its object lives in it); no reference to it exists yet
    unsafe { (*raw).kind = Kind::Op(o) };
    raw as usize
}
fn op(p: usize) -> &'static EeWonSocketOp {
    match &cpp(p).kind {
        Kind::Op(o) => o,
        _ => panic!("not an op"),
    }
}

/// The stand-in C++ side: the core of an object by kind, a new op (the one `NEXT_OP` holds), a release; each logged.
struct Side {
    log: RefCell<Vec<String>>,
    next_op: usize,
}
impl Side {
    fn send(&self, c: i32, o: [Obj; 3], n: [i64; 2], d: &[u8], a: [u8; 6]) -> Reply {
        let mut r = Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: None };
        match c {
            EE_WON_HOST_SOCK_CORE => {
                self.log.borrow_mut().push(format!("core {} {}", n[0], o[0].0 != 0));
                if o[0].0 != 0 {
                    let p: *mut c_void = match (&cpp(o[0].0).kind, n[0]) {
                        (Kind::Socket(s), EE_WON_HOST_CORE_SOCKET) => s as *const EeWonAsyncSocket as *mut c_void,
                        (Kind::Op(s), EE_WON_HOST_CORE_SOCKET_OP) => &**s as *const EeWonSocketOp as *mut c_void,
                        _ => null_mut(),
                    };
                    r.obj = Obj::of(p);
                }
            }
            EE_WON_HOST_NEW_OP => {
                self.log.borrow_mut().push(format!("new op {} {} data {:?} addr {:?}", n[0], n[1], d, a));
                r.obj = Obj(self.next_op);
            }
            _ => self.log.borrow_mut().push(format!("call {c} {}", o[0].0 == self.next_op)),
        }
        r
    }
    fn take(&self) -> Vec<String> {
        std::mem::take(&mut *self.log.borrow_mut())
    }
}
fn ask(side: &Side, code: i32, o: [Obj; 3], n: [i64; 2], data: &[u8]) -> Option<Reply> {
    let send = |c: i32, o: [Obj; 3], n: [i64; 2], d: &[u8], a: [u8; 6]| side.send(c, o, n, d, a);
    answer(code, o, n, data, [0; 6], &send)
}
fn obj(p: usize) -> [Obj; 3] {
    [Obj(p), Obj::none(), Obj::none()]
}
fn blocking(timeout: u32) -> OpTiming {
    OpTiming { run_mode: ops::MODE_BLOCK, start: ops::tick(), timeout }
}

#[test]
fn sockets_answer_on_their_objects() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    sk::start();
    let side = Side { log: RefCell::new(Vec::new()), next_op: 0 };
    let l = new_socket();
    let num = |code, p, n0, n1| ask(&side, code, obj(p), [n0, n1], &[]).unwrap().num;
    assert_eq!(num(EE_WON_HOST_SOCKET_PORT, l, 0, 0), 0);
    assert_eq!(num(EE_WON_HOST_SOCKET_LFS, l, 0, 0), 4);
    num(EE_WON_HOST_SOCKET_SET_LFS, l, 0x102, 0);
    assert_eq!(num(EE_WON_HOST_SOCKET_LFS, l, 0, 0), 2, "the size is the low byte");
    assert_eq!(num(EE_WON_HOST_SOCKET_BIND, l, 0, 1), 0);
    let port = num(EE_WON_HOST_SOCKET_PORT, l, 0, 0);
    assert_ne!(port, 0);
    assert_eq!(num(EE_WON_HOST_SOCKET_LISTEN, l, 0, 0), 0);
    assert_eq!(num(EE_WON_HOST_SOCKET_CONNECTED, l, 0, 0), 0);
    assert_eq!(num(EE_WON_HOST_SOCKET_STATUS, l, 0, 0), sock(l).status());
    num(EE_WON_HOST_CLOSE, l, 0, 0);
    assert!(!sock(l).is_valid());
    assert_eq!(num(EE_WON_HOST_SOCKET_PORT, l, 0, 0), 0);
    num(EE_WON_HOST_SOCKET_KILL, l, 0, 0);
    // each call asked the socket's object once
    assert_eq!(side.take().iter().filter(|s| *s == "core 0 true").count(), 12);
    // no object (the C++ dereferenced it): nothing done, zeros
    for code in [EE_WON_HOST_SOCKET_CONNECTED, EE_WON_HOST_SOCKET_STATUS, EE_WON_HOST_SOCKET_LFS, EE_WON_HOST_CLOSE] {
        assert_eq!(ask(&side, code, obj(0), [0, 0], &[]).unwrap().num, 0);
    }
    assert_eq!(side.take(), vec!["core 0 false"; 4]);
    // an op is not a socket
    let o = new_op(0);
    assert_eq!(num(EE_WON_HOST_SOCKET_LFS, o, 0, 0), 0);
    // a code that is not this module's
    assert!(ask(&side, EE_WON_HOST_TICK, obj(l), [0, 0], &[]).is_none());
    assert!(ask(&side, EE_WON_HOST_NEW_OP, obj(l), [0, 0], &[]).is_none());
}

#[test]
fn socket_ops_give_their_class_results() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let side = Side { log: RefCell::new(Vec::new()), next_op: 0 };
    let s = new_socket();
    let m = new_op(s);
    op(m).init_recv(0, true);
    let b = new_op(s);
    op(b).init_recv(7, false);
    let f = new_op(0);
    op(f).init_recv_from(512);
    let ask1 = |code, p| ask(&side, code, obj(p), [0, 0], &[]).unwrap();
    assert_eq!(ask1(EE_WON_HOST_OP_IS_RECV_MSG, m).num, 1);
    assert_eq!(ask1(EE_WON_HOST_OP_IS_RECV_MSG, b).num, 0);
    assert_eq!(ask1(EE_WON_HOST_OP_IS_RECV_MSG, s).num, 0, "a socket is no op");
    // nothing received yet: no bytes; a datagram op's sender is given (zero), another class's none
    assert_eq!(ask1(EE_WON_HOST_OP_BYTES, m).bytes, None);
    assert_eq!(ask1(EE_WON_HOST_OP_BYTES, f).bytes, None);
    assert_eq!(ask1(EE_WON_HOST_OP_ADDR, f).addr, EeWonIpAddr::default().addr().six_bytes());
    assert_eq!(ask1(EE_WON_HOST_OP_ADDR, m).addr, [0; 6]);
    // the socket, borrowed
    assert_eq!(ask1(EE_WON_HOST_OP_SOCKET, m).obj, Obj(s));
    assert_eq!(ask1(EE_WON_HOST_OP_SOCKET, f).obj, Obj::none());
    assert_eq!(ask1(EE_WON_HOST_OP_SOCKET, s).obj, Obj::none());
    // the receive's settings
    ask(&side, EE_WON_HOST_RECV_FILE, obj(b), [1, 0], b"x/y.bin").unwrap();
    assert_eq!(op(b).path(), b"x/y.bin");
    let c = leak(Kind::Bytes(EeWonByteBuffer::copy(b"")));
    ask(&side, EE_WON_HOST_RECV_PROGRESS, [Obj(b), Obj(c), Obj::none()], [3, 0], &[]).unwrap();
    assert_eq!(refs(c), 1, "the progress completion is held");
    // a class without the settings takes no reference
    ask(&side, EE_WON_HOST_RECV_PROGRESS, [Obj(f), Obj(c), Obj::none()], [3, 0], &[]).unwrap();
    assert_eq!(refs(c), 1);
}

#[test]
fn a_line_op_keeps_its_message_and_blocking_calls_read_it() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    sk::start();
    // a connected pair over 127.0.0.1
    let l = new_socket();
    let mut here = EeWonIpAddr::default();
    here.set_host_order(0x7f00_0001, 0);
    assert_eq!(sock(l).bind(here.addr(), true), WS_SUCCESS);
    assert_eq!(sock(l).listen(5), WS_SUCCESS);
    here.set_port(sock(l).local_port());
    let c = new_socket();
    let st = sock(c).connect_to(&here, 0);
    assert!(st == WS_SUCCESS || st == WS_TIMED_OUT);
    assert_eq!(sock(c).wait_for_connect(5000), WS_SUCCESS);
    sock(l).wait(crate::socket::WAIT_READ, 5000);
    let (h, peer) = sock(l).accept().expect("accepted");
    let s = new_socket();
    sock(s).adopt(h, peer);

    let line = new_op(s);
    op(line).init_line();
    let mut n = 0;
    assert_eq!(sock(c).send(b"one\r\ntwo\r\n", &mut n), WS_SUCCESS);
    assert_eq!(op(line).op_start(blocking(5000)), WS_SUCCESS);
    let first = op(line).message();
    assert_ne!(first, 0);
    assert_eq!(refs(first), 1);
    // the line's CR is a NUL, its LF dropped
    assert_eq!(op(line).result_bytes(), Some(b"one\0".to_vec()));
    // a new start drops the last message first
    assert_eq!(op(line).op_start(blocking(5000)), WS_SUCCESS);
    assert_eq!(refs(first), 0);
    assert_eq!(op(line).result_bytes(), Some(b"two\0".to_vec()));
    // nothing more: a time-out keeps no message
    assert_eq!(op(line).op_start(blocking(50)), WS_TIMED_OUT);
    assert_eq!(op(line).message(), 0);

    // a blocking call: the op is the C++ side's, read here when it succeeded (no op object: the run answers 0),
    // then released
    assert_eq!(sock(c).send(b"three\r\n", &mut n), WS_SUCCESS);
    assert_eq!(op(line).op_start(blocking(5000)), WS_SUCCESS);
    let side = Side { log: RefCell::new(Vec::new()), next_op: line };
    let r = ask(&side, EE_WON_HOST_RECV_CR, obj(s), [2000, 0], &[]).unwrap();
    assert_eq!(r.bytes, Some(b"three\0".to_vec()));
    assert_eq!(
        side.take(),
        vec![
            "new op 6 0 data [] addr [0, 0, 0, 0, 0, 0]".to_string(),
            "core 2 true".into(),
            "core 3 true".into(),
            format!("call {EE_WON_HOST_RELEASE} true"),
        ]
    );
    // the kinds and lengths each call makes its op with
    let calls: [(i32, [i64; 2], &str); 4] = [
        (EE_WON_HOST_RECV_BYTES, [0x1_0000_0005, 9], "new op 4 5 data [] addr [0, 0, 0, 0, 0, 0]"),
        (EE_WON_HOST_RECV_BYTES_FROM, [0x1_0200, 9], "new op 7 512 data [] addr [0, 0, 0, 0, 0, 0]"),
        (EE_WON_HOST_SEND_MSG, [9, 0], "new op 3 0 data [104, 105] addr [0, 0, 0, 0, 0, 0]"),
        (EE_WON_HOST_RECV_MSG, [9, 0], "new op 5 0 data [] addr [0, 0, 0, 0, 0, 0]"),
    ];
    for (code, n, want) in calls {
        ask(&side, code, obj(s), n, b"hi").unwrap();
        assert_eq!(side.take()[0], want);
    }
}
