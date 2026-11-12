//! The two ops over stand-ins: each op, socket, manager and datagram is a counted object with a name; an op has a
//! status, a mode and a check (a status per first byte, or a throw on 'X'). The run paths, the receive's address and
//! check rules, the retransmits, the clean-ups, and every reference given back.

use super::*;
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

struct Obj {
    name: String,
    refs: AtomicI32,
    status: Mutex<Status>,
    mode: i32,
    accept: Vec<(u8, Status)>,
    /// a reply wait's Rust object
    object: Mutex<usize>,
}
static ALL: Mutex<Vec<usize>> = Mutex::new(Vec::new());
fn new_obj(name: &str, mode: i32, accept: Vec<(u8, Status)>) -> usize {
    let o = Box::leak(Box::new(Obj {
        name: name.to_string(),
        refs: AtomicI32::new(0),
        status: Mutex::new(WS_NONE),
        mode,
        accept,
        object: Mutex::new(0),
    }));
    let p = o as *const Obj as usize;
    ALL.lock().unwrap().push(p);
    p
}
fn o(p: usize) -> &'static Obj {
    unsafe { &*(p as *const Obj) }
}
fn p(x: *mut c_void) -> &'static Obj {
    o(x as usize)
}
fn name(x: usize) -> String {
    if x == 0 {
        "-".into()
    } else {
        o(x).name.clone()
    }
}

unsafe extern "C" fn ref_add(x: *mut c_void) {
    p(x).refs.fetch_add(1, SeqCst);
}
unsafe extern "C" fn ref_release(x: *mut c_void) {
    p(x).refs.fetch_sub(1, SeqCst);
}
unsafe extern "C" fn op_mode(x: *mut c_void) -> i32 {
    p(x).mode
}
unsafe extern "C" fn op_timeout(_x: *mut c_void) -> c_long {
    500
}
unsafe extern "C" fn op_status(x: *mut c_void) -> c_long {
    *p(x).status.lock().unwrap() as c_long
}
unsafe extern "C" fn op_finish(x: *mut c_void, s: c_long) {
    log(format!("{} finish {s}", p(x).name));
    *p(x).status.lock().unwrap() = s as Status;
}
unsafe extern "C" fn op_run_async(x: *mut c_void, t: c_long) {
    log(format!("{} run {t}", p(x).name));
    *p(x).status.lock().unwrap() = WS_PENDING;
}
unsafe extern "C" fn op_kill(x: *mut c_void) {
    log(format!("{} kill", p(x).name));
    *p(x).status.lock().unwrap() = WS_KILLED;
}
unsafe extern "C" fn recv_check(x: *mut c_void, d: *const u8, n: usize, threw: *mut bool) -> c_long {
    let first = if n == 0 { 0 } else { unsafe { *d } };
    log(format!("{} checks {}", p(x).name, first as char));
    if first == b'X' {
        unsafe { *threw = true };
        return 0;
    }
    p(x).accept.iter().find(|e| e.0 == first).map_or(WS_NONE, |e| e.1) as c_long
}
static SOCKETS: AtomicI32 = AtomicI32::new(0);
unsafe extern "C" fn socket_new_bound() -> *mut c_void {
    let n = SOCKETS.fetch_add(1, SeqCst);
    let x = new_obj(&format!("sock{n}"), 1, vec![]);
    o(x).refs.fetch_add(1, SeqCst);
    log(format!("new {}", o(x).name));
    x as *mut c_void
}
unsafe extern "C" fn socket_kill(x: *mut c_void) {
    log(format!("{} kill", p(x).name));
}
unsafe extern "C" fn socket_queue_recv(s: *mut c_void, op: *mut c_void) {
    log(format!("{} recv for {}", p(s).name, p(op).name));
}
unsafe extern "C" fn socket_send(s: *mut c_void, b: *mut c_void, a: *const EeWonIpAddr) {
    log(format!("{} send {} to {}", p(s).name, name(b as usize), unsafe { &*a }.port()));
}
unsafe extern "C" fn manager_queue(m: *mut c_void, op: *mut c_void, t: u32) {
    log(format!("{} queue {} {t}", p(m).name, p(op).name));
}
unsafe extern "C" fn manager_send(m: *mut c_void, b: *mut c_void, a: *const EeWonIpAddr) {
    log(format!("{} send {} to {}", p(m).name, name(b as usize), unsafe { &*a }.port()));
}
static WAITS: AtomicI32 = AtomicI32::new(0);
unsafe extern "C" fn new_wait(r: *mut c_void, a: *const EeWonIpAddr) -> *mut c_void {
    let n = WAITS.fetch_add(1, SeqCst);
    let x = new_obj(&format!("wait{n}"), 1, vec![]);
    o(x).refs.fetch_add(1, SeqCst);
    let w = Box::into_raw(Box::new(EeWonUdpRecvOp::new(x as *mut c_void, &HOOKS, Some(unsafe { &*a }))));
    *o(x).object.lock().unwrap() = w as usize;
    log(format!("new {} for {} from {}", o(x).name, p(r).name, unsafe { &*a }.port()));
    x as *mut c_void
}
unsafe extern "C" fn recv_object(x: *mut c_void) -> *const EeWonUdpRecvOp {
    *p(x).object.lock().unwrap() as *const EeWonUdpRecvOp
}

static HOOKS: EeWonUdpOpHooks = EeWonUdpOpHooks {
    ref_add: Some(ref_add),
    ref_release: Some(ref_release),
    op_mode: Some(op_mode),
    op_timeout: Some(op_timeout),
    op_status: Some(op_status),
    op_finish: Some(op_finish),
    op_run_async: Some(op_run_async),
    op_kill: Some(op_kill),
    recv_check: Some(recv_check),
    socket_new_bound: Some(socket_new_bound),
    socket_kill: Some(socket_kill),
    socket_queue_recv: Some(socket_queue_recv),
    socket_send: Some(socket_send),
    manager_queue: Some(manager_queue),
    manager_send: Some(manager_send),
    new_wait: Some(new_wait),
    recv_object: Some(recv_object),
};

fn at(port: u16) -> EeWonIpAddr {
    let mut a = EeWonIpAddr::default();
    a.set_host_order(0x7f00_0001, port);
    a
}

/// frees the reply waits' objects, then checks that nothing holds a reference
fn all_given_back() {
    for x in ALL.lock().unwrap().drain(..) {
        let w = std::mem::take(&mut *o(x).object.lock().unwrap());
        if w != 0 {
            drop(unsafe { Box::from_raw(w as *mut EeWonUdpRecvOp) });
        }
    }
}

#[test]
fn the_receive_op_runs_on_a_manager_its_own_socket_or_a_given_one() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    SOCKETS.store(0, SeqCst);
    // blocking: refused
    let blk = new_obj("blk", OP_MODE_BLOCK, vec![]);
    let r = EeWonUdpRecvOp::new(blk as *mut c_void, &HOOKS, None);
    r.run();
    assert_eq!(take(), ["blk finish 100009"]);
    drop(r);
    // through a manager, with the op's time-out
    let op = new_obj("op", 1, vec![(b'A', WS_SUCCESS), (b'R', 100059)]);
    let m = new_obj("mgr", 1, vec![]);
    let r = EeWonUdpRecvOp::new(op as *mut c_void, &HOOKS, Some(&at(9)));
    r.set_manager(m);
    assert_eq!(o(m).refs.load(SeqCst), 1);
    r.run();
    assert_eq!(take(), ["mgr queue op 500"]);
    assert_eq!(r.socket(), 0);
    r.set_manager(0);
    assert_eq!(o(m).refs.load(SeqCst), 0);
    // its own socket, made once
    r.run();
    r.run();
    assert_eq!(take(), ["new sock0", "sock0 recv for op", "sock0 recv for op"]);
    let s0 = r.socket();
    *o(op).status.lock().unwrap() = WS_PENDING;
    // another sender: received again; a refusal and a throw: again; an accepted one: finished, no message kept
    r.recv(b"A", &at(8));
    r.recv(b"Q", &at(9));
    r.recv(b"X", &at(9));
    r.recv(b"R", &at(9));
    assert_eq!(
        take(),
        [
            "sock0 recv for op",
            "op checks Q",
            "sock0 recv for op",
            "op checks X",
            "sock0 recv for op",
            "op checks R",
            "op finish 100059"
        ]
    );
    // not pending: ignored
    r.recv(b"A", &at(9));
    assert_eq!(take(), Vec::<String>::new());
    assert_eq!(r.msg(), 0);
    // the own socket is killed and let go
    r.cleanup();
    assert_eq!(take(), ["sock0 kill"]);
    assert_eq!(o(s0).refs.load(SeqCst), 0);
    r.cleanup();
    assert_eq!(take(), Vec::<String>::new());
    // a given socket stays; an address-less op takes any sender
    let given = new_obj("given", 1, vec![]);
    let any = new_obj("any", 1, vec![(b'A', WS_SUCCESS)]);
    let r2 = EeWonUdpRecvOp::new(any as *mut c_void, &HOOKS, None);
    r2.set_socket(given);
    r2.run();
    *o(any).status.lock().unwrap() = WS_PENDING;
    r2.recv(b"A", &at(3));
    r2.cleanup();
    assert_eq!(take(), ["given recv for any", "any checks A", "any finish 0"]);
    assert_eq!(o(given).refs.load(SeqCst), 1);
    // the message and the check are kept as given
    let d = new_obj("dgram", 1, vec![]);
    r2.set_msg(d);
    r2.set_verifier(11, 12);
    assert_eq!((r2.msg(), r2.verifier()), (d, (11, 12)));
    r2.set_address(&at(4));
    assert_eq!(r2.address().port(), 4);
    drop(r);
    drop(r2);
    for x in [blk, op, m, given, any, d] {
        assert_eq!(o(x).refs.load(SeqCst), 0, "{}", o(x).name);
    }
    all_given_back();
}

#[test]
fn the_request_retransmits_until_its_limit() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    SOCKETS.store(0, SeqCst);
    WAITS.store(0, SeqCst);
    let req = new_obj("req", 1, vec![]);
    let bytes = new_obj("bytes", 1, vec![]);
    let r = EeWonUdpSendRecvOp::new(req as *mut c_void, &HOOKS);
    assert_eq!((r.retry_limit(), r.attempts(), r.timeout()), (0, 0, 10000));
    r.set_address(&at(7));
    r.set_outgoing(bytes);
    r.set_retry_limit(2);
    r.set_timeout(60);
    // on its own socket: sent there, the wait given the socket
    r.run();
    assert_eq!(take(), ["new sock0", "new wait0 for req from 7", "sock0 send bytes to 7", "wait0 run 60"]);
    let w0 = r.recv_op();
    let obj0 = *o(w0).object.lock().unwrap() as *const EeWonUdpRecvOp;
    assert_eq!(unsafe { &*obj0 }.socket(), r.socket());
    *o(req).status.lock().unwrap() = WS_PENDING;
    // a stale wait's end is ignored; a time-out retransmits (twice), then WS_NoReply
    r.recv_done(bytes);
    *o(w0).status.lock().unwrap() = WS_TIMED_OUT;
    r.recv_done(w0);
    let w1 = r.recv_op();
    *o(w1).status.lock().unwrap() = WS_TIMED_OUT;
    r.recv_done(w1);
    let w2 = r.recv_op();
    *o(w2).status.lock().unwrap() = WS_TIMED_OUT;
    r.recv_done(w2);
    assert_eq!(
        take(),
        [
            "new wait1 for req from 7",
            "sock0 send bytes to 7",
            "wait1 run 60",
            "new wait2 for req from 7",
            "sock0 send bytes to 7",
            "wait2 run 60",
            "req finish 100015"
        ]
    );
    assert_eq!(r.attempts(), 2);
    r.cleanup();
    assert_eq!(take(), ["wait2 kill", "sock0 kill"]);
    assert_eq!((r.recv_op(), r.socket()), (0, 0));
    // through a manager: the attempts start over; the wait's status ends the request
    let m = new_obj("mgr", 1, vec![]);
    r.set_manager(m);
    r.run();
    let w3 = r.recv_op();
    let obj3 = *o(w3).object.lock().unwrap() as *const EeWonUdpRecvOp;
    assert_eq!(unsafe { &*obj3 }.manager(), m);
    *o(req).status.lock().unwrap() = WS_PENDING;
    *o(w3).status.lock().unwrap() = WS_KILLED;
    r.recv_done(w3);
    assert_eq!(take(), ["new wait3 for req from 7", "mgr send bytes to 7", "wait3 run 60", "req finish 100002"]);
    assert_eq!(r.attempts(), 0);
    // blocking: refused
    let blk = new_obj("blk", OP_MODE_BLOCK, vec![]);
    let b = EeWonUdpSendRecvOp::new(blk as *mut c_void, &HOOKS);
    b.run();
    assert_eq!(take(), ["blk finish 100009"]);
    drop(b);
    drop(r);
    // the waits' C++ references: the request let them go; their objects hold the socket and manager they were given
    all_given_back();
    for x in [req, bytes, m, blk] {
        assert_eq!(o(x).refs.load(SeqCst), 0, "{}", o(x).name);
    }
}
