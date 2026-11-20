//! The socket ops over 127.0.0.1 with a stand-in for the C++ side: reference-counted objects for the byte buffers,
//! completions and sockets, a tagging transform, the target file through `std::fs`, the progress chunk reading the
//! op's count, `Accept` into a new socket object. Every reference an op takes is given back when it goes.

use super::*;
use crate::socket::{self as sk, KIND_DATAGRAM, KIND_STREAM};
use crate::sockobj::{EeWonAsyncSocket, EeWonAsyncSocketHooks};
use std::io::Write as _;
use std::sync::atomic::{AtomicI32, Ordering::SeqCst};
use std::sync::Mutex;

static LOCK: Mutex<()> = Mutex::new(());
static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
fn log(s: String) {
    LOG.lock().unwrap().push(s);
}
fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}

/// A stand-in C++ object: its count and what it is.
struct Obj {
    refs: AtomicI32,
    bytes: EeWonByteBuffer,
    socket: Option<&'static EeWonAsyncSocket>,
}
fn obj(bytes: &[u8]) -> &'static Obj {
    Box::leak(Box::new(Obj { refs: AtomicI32::new(0), bytes: EeWonByteBuffer::copy(bytes), socket: None }))
}
fn refs(o: usize) -> i32 {
    unsafe { &*(o as *const Obj) }.refs.load(SeqCst)
}
fn bytes(o: usize) -> Vec<u8> {
    unsafe { &*(o as *const Obj) }.bytes.bytes().to_vec()
}

unsafe extern "C" fn ref_add(p: *mut c_void) {
    unsafe { &*(p as *const Obj) }.refs.fetch_add(1, SeqCst);
}
unsafe extern "C" fn ref_release(p: *mut c_void) {
    unsafe { &*(p as *const Obj) }.refs.fetch_sub(1, SeqCst);
}
unsafe extern "C" fn bytes_of(p: *mut c_void) -> *const EeWonByteBuffer {
    &unsafe { &*(p as *const Obj) }.bytes
}
unsafe extern "C" fn bytes_adopt(b: *mut EeWonByteBuffer) -> *mut c_void {
    let b = unsafe { Box::from_raw(b) };
    let o = Box::leak(Box::new(Obj { refs: AtomicI32::new(1), bytes: *b, socket: None }));
    o as *mut Obj as *mut c_void
}
/// a transform on every socket: a 't' appended on send, checked and dropped on receive
unsafe extern "C" fn transform(_socket: *mut c_void, message: *mut *mut c_void, send: bool) -> std::ffi::c_long {
    let old = unsafe { *message } as usize;
    let mut b = bytes(old);
    if send {
        b.push(b't');
    } else if b.pop() != Some(b't') {
        return WS_RECV_MSG_INVALID_MESSAGE_LENGTH as std::ffi::c_long;
    }
    let new = obj(&b);
    new.refs.fetch_add(1, SeqCst);
    unsafe {
        ref_release(old as *mut c_void);
        *message = new as *const Obj as *mut c_void;
    }
    0
}
unsafe extern "C" fn file_open(path: *const u8, len: usize, binary: bool, append: bool) -> *mut c_void {
    let p = std::str::from_utf8(unsafe { std::slice::from_raw_parts(path, len) }).unwrap().to_string();
    log(format!("open binary {binary} append {append}"));
    let f = std::fs::OpenOptions::new().create(true).append(append).write(true).truncate(!append).open(p);
    match f {
        Ok(f) => Box::into_raw(Box::new(f)).cast(),
        Err(_) => null_mut(),
    }
}
unsafe extern "C" fn file_write(f: *mut c_void, data: *const u8, len: usize) -> usize {
    let f = unsafe { &mut *(f as *mut std::fs::File) };
    f.write(unsafe { std::slice::from_raw_parts(data, len) }).unwrap_or(0)
}
unsafe extern "C" fn file_close(f: *mut c_void) -> i32 {
    drop(unsafe { Box::from_raw(f as *mut std::fs::File) });
    log("close".into());
    0
}
unsafe extern "C" fn file_set_time(_path: *const u8, _len: usize, time: i64) {
    log(format!("time {time}"));
}
/// the "C++ op" is a cell holding the op object: the chunk reads the bytes so far
unsafe extern "C" fn progress(op: *mut c_void, completion: *mut c_void) {
    let o = unsafe { *(op as *const *const EeWonSocketOp) };
    log(format!("chunk {} completion refs {}", unsafe { &*o }.bytes_read(), refs(completion as usize)));
}
unsafe extern "C" fn accept(socket: *mut c_void, out: *mut *mut c_void) -> std::ffi::c_long {
    let s = unsafe { &*(socket as *const Obj) }.socket.unwrap();
    match s.accept() {
        Ok((h, peer)) => {
            let n = new_socket(KIND_STREAM);
            unsafe { &*(n as *const Obj) }.socket.unwrap().adopt(h, peer);
            unsafe { ref_add(n as *mut c_void) };
            unsafe { *out = n as *mut c_void };
            0
        }
        Err(e) => e as std::ffi::c_long,
    }
}
unsafe extern "C" fn socket_object(s: *mut c_void) -> *mut EeWonAsyncSocket {
    unsafe { &*(s as *const Obj) }.socket.unwrap() as *const EeWonAsyncSocket as *mut EeWonAsyncSocket
}

static HOOKS: EeWonSocketOpBaseHooks = EeWonSocketOpBaseHooks {
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
    transform: Some(transform),
    file_open: Some(file_open),
    file_write: Some(file_write),
    file_close: Some(file_close),
    file_set_time: Some(file_set_time),
    progress: Some(progress),
    accept: Some(accept),
};

static QUIET: EeWonAsyncSocketHooks = EeWonAsyncSocketHooks {
    open_hook: None,
    close_hook: None,
    kill_hook: None,
    transform_send: None,
    transform_recv: None,
    transform_free: None,
};

/// A socket stand-in (an object holding a leaked socket object).
fn new_socket(kind: i32) -> usize {
    let s: &'static EeWonAsyncSocket = Box::leak(Box::new(EeWonAsyncSocket::new(null_mut(), &QUIET, kind)));
    let o = Box::leak(Box::new(Obj { refs: AtomicI32::new(0), bytes: EeWonByteBuffer::copy(b""), socket: Some(s) }));
    o as *const Obj as usize
}
fn sock(o: usize) -> &'static EeWonAsyncSocket {
    unsafe { &*(o as *const Obj) }.socket.unwrap()
}

/// An op on a socket; the "C++ op" cell points at it.
fn new_op(socket: usize) -> Box<EeWonSocketOp> {
    let cell: &'static mut *const EeWonSocketOp = Box::leak(Box::new(std::ptr::null()));
    let cell_ptr = cell as *mut *const EeWonSocketOp;
    let o = Box::new(EeWonSocketOp::new(cell_ptr.cast(), &HOOKS));
    unsafe {
        *cell_ptr = &*o;
        o.set_socket(socket as *mut c_void);
    }
    o
}

fn blocking(timeout: u32) -> OpTiming {
    OpTiming { run_mode: ops::MODE_BLOCK, start: ops::tick(), timeout }
}

fn local(port: u16) -> EeWonIpAddr {
    let mut a = EeWonIpAddr::default();
    a.set_host_order(0x7f00_0001, port);
    a
}

/// A listener, a client connected to it through a ConnectOp and the socket an AcceptOp accepted.
fn pair() -> (usize, usize, usize) {
    sk::start();
    let l = new_socket(KIND_STREAM);
    assert_eq!(sock(l).bind(local(0).addr(), true), WS_SUCCESS);
    assert_eq!(sock(l).listen(5), WS_SUCCESS);
    let c = new_socket(KIND_STREAM);
    let conn = new_op(c);
    conn.init_connect(&local(sock(l).local_port()));
    let s = conn.op_start(blocking(5000));
    assert!(s == WS_SUCCESS || s == WS_TIMED_OUT);
    assert_eq!(conn.op_continue(blocking(5000)), WS_SUCCESS);
    assert_eq!(conn.address(), local(sock(l).local_port()));
    assert_eq!(sock(c).dest_addr(), local(sock(l).local_port()));
    let acc = new_op(l);
    acc.init_accept();
    assert_eq!(acc.op_start(blocking(5000)), WS_SUCCESS);
    let a = acc.accepted();
    assert_ne!(a, 0);
    assert_eq!(refs(a), 1);
    unsafe { ref_add(a as *mut c_void) };
    drop(acc);
    assert_eq!(refs(a), 1);
    assert_eq!(sock(a).dest_addr().port(), sock(c).local_port());
    (l, c, a)
}

#[test]
fn bytes_messages_and_files_over_a_stream() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    let (_l, c, s) = pair();
    // bytes, with a progress chunk once more than 3 arrived
    let data = obj(b"hello world").as_ptr();
    let send = new_op(c);
    send.init_send(data, 0, false);
    assert_eq!(refs(data), 1);
    assert_eq!(send.op_start(blocking(5000)), WS_SUCCESS);
    let recv = new_op(s);
    recv.init_recv(11, false);
    let done = obj(b"").as_ptr();
    recv.set_progress(done, 3);
    assert_eq!(recv.op_start(blocking(5000)), WS_SUCCESS);
    assert_eq!(bytes(recv.received()), b"hello world");
    assert_eq!(recv.bytes_read(), 11);
    assert_eq!(take(), ["chunk 11 completion refs 1"]);
    drop((send, recv));
    assert_eq!((refs(data), refs(done)), (0, 0));

    // a message (a 4-byte prefix counting itself) through the transforms both ways
    let m = obj(&[6, 0, 0, 0, b'h', b'i']).as_ptr();
    let send = new_op(c);
    send.init_send(0, m, true);
    assert_eq!(send.op_start(blocking(5000)), WS_SUCCESS);
    let recv = new_op(s);
    recv.init_recv(0, true);
    // the prefix said 6 bytes but 7 came (the send transform's tag): the receive takes 2 for the body, then the
    // receive transform drops a tag that is not there
    assert_eq!(recv.op_start(blocking(5000)), WS_RECV_MSG_INVALID_MESSAGE_LENGTH);
    let mut rest = [0u8; 8];
    let mut n = 0;
    sock(s).recv(&mut rest, &mut n);
    assert_eq!(&rest[..n as usize], b"t");
    let m2 = obj(&[7, 0, 0, 0, b'h', b'i']).as_ptr();
    let send2 = new_op(c);
    send2.init_send(0, m2, true);
    assert_eq!(send2.op_start(blocking(5000)), WS_SUCCESS);
    let recv2 = new_op(s);
    recv2.init_recv(0, true);
    assert_eq!(recv2.op_start(blocking(5000)), WS_SUCCESS);
    assert_eq!(bytes(recv2.message()), b"hi");
    assert_eq!(bytes(recv2.received()), b"hit");
    // an invalid length field size
    sock(s).set_length_field_size(3);
    let recv3 = new_op(s);
    recv3.init_recv(0, true);
    assert_eq!(recv3.op_start(blocking(50)), WS_RECV_MSG_INVALID_LENGTH_FIELD_SIZE);
    sock(s).set_length_field_size(4);
    drop((send, recv, send2, recv2, recv3));
    assert_eq!((refs(m), refs(m2)), (0, 0));

    // into a file, the modify time set at the close
    let dir = std::env::temp_dir().join(format!("ee_won_sockops_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let path = dir.join("recv.bin");
    let raw = obj(b"filedata").as_ptr();
    let send = new_op(c);
    send.init_send(raw, 0, false);
    assert_eq!(send.op_start(blocking(5000)), WS_SUCCESS);
    let recv = new_op(s);
    recv.init_recv(8, false);
    recv.set_path(path.to_str().unwrap().as_bytes());
    recv.set_modify_time(1_000_000_000);
    assert_eq!(recv.op_start(blocking(5000)), WS_SUCCESS);
    assert_eq!(recv.received(), 0);
    assert_eq!(recv.path(), path.to_str().unwrap().as_bytes());
    assert_eq!(take(), ["open binary true append false", "close", "time 1000000000"]);
    assert_eq!(std::fs::read(&path).unwrap(), b"filedata");
    // a path that cannot be opened
    let bad = new_op(s);
    bad.init_recv(8, false);
    bad.set_path(dir.join("no_dir").join("x").to_str().unwrap().as_bytes());
    assert_eq!(bad.op_start(blocking(50)), WS_FAILED_TO_OPEN_FILE);
    take();
    // skipping keeps nothing; a time-out keeps the partial bytes for a re-run
    let tail = obj(b"skipme").as_ptr();
    let send2 = new_op(c);
    send2.init_send(tail, 0, false);
    assert_eq!(send2.op_start(blocking(5000)), WS_SUCCESS);
    let skip = new_op(s);
    skip.init_recv(6, false);
    skip.set_flag(2, true);
    assert_eq!(skip.op_start(blocking(5000)), WS_SUCCESS);
    assert_eq!(bytes(skip.received()), b"");
    let short = new_op(s);
    short.init_recv(4, false);
    assert_eq!(short.op_start(blocking(30)), WS_TIMED_OUT);
    std::fs::remove_dir_all(&dir).ok();
    drop((send, recv, bad, send2, skip, short));
    assert_eq!((refs(raw), refs(tail)), (0, 0));
}

#[test]
fn datagrams_and_their_senders() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    sk::start();
    let u = new_socket(KIND_DATAGRAM);
    let v = new_socket(KIND_DATAGRAM);
    assert_eq!(sock(u).bind(local(0).addr(), false), WS_SUCCESS);
    assert_eq!(sock(v).bind(local(0).addr(), false), WS_SUCCESS);
    let data = obj(b"dgram").as_ptr();
    let to = new_op(u);
    to.init_send_to(data, &local(sock(v).local_port()));
    assert_eq!(to.op_start(blocking(5000)), WS_SUCCESS);
    let from = new_op(v);
    from.init_recv_from(512);
    from.recv_from_prepare();
    assert_eq!(from.op_start(blocking(5000)), WS_SUCCESS);
    assert_eq!(bytes(from.received()), b"dgram");
    assert_eq!(from.address().port(), sock(u).local_port());
    assert_eq!(from.address().host(), 0x7f00_0001);
    assert_eq!(from.length(), 512);
    // nothing waiting: the datagram dropped at the next run, the sender kept
    from.recv_from_prepare();
    assert_eq!(from.op_start(blocking(0)), WS_TIMED_OUT);
    assert_eq!(from.received(), 0);
    assert_eq!(from.address().port(), sock(u).local_port());
    // connected: a send, and the destination as the sender
    let mut d = local(sock(v).local_port());
    assert_eq!(sock(u).connect_to(&d, 0), WS_SUCCESS);
    d.set_port(sock(u).local_port());
    assert_eq!(sock(v).connect_to(&d, 0), WS_SUCCESS);
    let to2 = new_op(u);
    to2.init_send_to(data, &local(1));
    assert_eq!(to2.op_start(blocking(5000)), WS_SUCCESS);
    from.recv_from_prepare();
    assert_eq!(from.op_start(blocking(5000)), WS_SUCCESS);
    assert_eq!(bytes(from.received()), b"dgram");
    assert_eq!(from.address(), d);
    drop((to, from, to2));
    assert_eq!(refs(data), 0);
}

#[test]
fn an_op_without_a_socket_and_a_refused_connect() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    sk::start();
    let o = new_op(0);
    o.init_recv(4, false);
    assert_eq!(o.op_start(blocking(10)), WS_ASYNC_SOCKET_INVALID_SOCKET);
    let c = new_socket(KIND_STREAM);
    let conn = new_op(c);
    conn.init_connect(&EeWonIpAddr::default());
    assert_eq!(conn.op_start(blocking(10)), WS_INVALID_ADDRESS);
    let acc = new_op(c);
    acc.init_accept();
    assert_ne!(acc.op_start(blocking(0)), WS_SUCCESS);
    assert_eq!(acc.accepted(), 0);
}

trait AsPtr {
    fn as_ptr(&'static self) -> usize;
}
impl AsPtr for Obj {
    fn as_ptr(&'static self) -> usize {
        self as *const Obj as usize
    }
}

/// The kinds (R6 WONLobby round 28): each original constructor's wait flags and members, the class back as its
/// kind, the queued close's `Continue` without a socket, `CloseOp`'s closing its socket, and no duplicate for a
/// class that had none.
#[test]
fn kinds_are_the_original_constructors() {
    let _guard = LOCK.lock().unwrap();
    type K = EeWonCoreSocketOp;
    let data = obj(b"abc") as *const Obj as usize;
    let to = local(9);
    // (kind, waits for read, write, connect)
    let cases = [
        (K::Close, [false, false, false]),
        (K::QueueClose, [false, false, false]),
        (K::RecvFrom, [true, false, false]),
        (K::SendTo, [false, true, false]),
        (K::RecvMsg, [true, false, false]),
        (K::SendMsg, [false, true, false]),
        (K::Connect, [false, true, true]),
        (K::SendBytes, [false, true, false]),
        (K::RecvBytes, [true, false, false]),
        (K::RecvCrMsg, [true, false, false]),
        (K::Accept, [true, false, false]),
    ];
    for (kind, waits) in cases {
        let o = new_op(0);
        assert_eq!(o.class_kind(), -1);
        o.init_kind(kind, data, Some(&to), 77);
        assert_eq!(o.class_kind(), kind as i32, "{kind:?}");
        assert_eq!([o.wait_for_event(0), o.wait_for_event(1), o.wait_for_event(2)], waits, "{kind:?}");
        match kind {
            K::RecvFrom => assert_eq!(o.length(), 77),
            K::RecvBytes => assert_eq!(o.length(), 77),
            K::Connect => assert_eq!(o.address(), to),
            _ => {}
        }
        // the bytes' reference is held by the send kinds only
        let held = matches!(kind, K::SendTo | K::SendMsg | K::SendBytes);
        assert_eq!(refs(data), held as i32, "{kind:?}");
        // no C++ table here: the duplicate's new op is not made
        assert!(o.duplicate().is_null());
        drop(o);
        assert_eq!(refs(data), 0, "{kind:?}");
    }
    // the queued close succeeds with no socket; any other kind without one is InvalidSocket
    let q = new_op(0);
    q.init_kind(K::QueueClose, 0, None, 0);
    assert_eq!(q.op_continue(blocking(0)), WS_SUCCESS);
    let c = new_op(0);
    c.init_kind(K::Close, 0, None, 0);
    assert_eq!(c.op_continue(blocking(0)), WS_ASYNC_SOCKET_INVALID_SOCKET);
    // CloseOp's Continue closes its socket
    sk::start();
    let s = new_socket(KIND_DATAGRAM);
    assert_eq!(sock(s).open(), WS_SUCCESS);
    assert!(sock(s).is_open());
    let c = new_op(s);
    c.init_kind(K::Close, 0, None, 0);
    assert_eq!(c.op_start(blocking(0)), WS_SUCCESS);
    assert!(!sock(s).is_open());
}
