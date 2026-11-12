//! The manager over stand-in ops: each op has a name, a status, an address, a check (a status per first byte, or a
//! throw) and a count; the socket and the completions are counted objects. The dispatch's order, the clean-up's
//! cursor, the close, and every reference given back.

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
    addr: Mutex<EeWonIpAddr>,
    completion: Mutex<usize>,
    /// first byte -> the check's answer; 'X' throws
    accept: Vec<(u8, Status)>,
    msg: Mutex<usize>,
}
fn new_obj(name: &str, addr: Option<&EeWonIpAddr>, accept: Vec<(u8, Status)>) -> usize {
    let o = Box::leak(Box::new(Obj {
        name: name.to_string(),
        refs: AtomicI32::new(0),
        status: Mutex::new(WS_SUCCESS),
        addr: Mutex::new(addr.cloned().unwrap_or_default()),
        completion: Mutex::new(0),
        accept,
        msg: Mutex::new(0),
    }));
    o as *const Obj as usize
}
fn o(p: usize) -> &'static Obj {
    unsafe { &*(p as *const Obj) }
}
fn p(x: *mut c_void) -> &'static Obj {
    o(x as usize)
}

unsafe extern "C" fn ref_add(x: *mut c_void) {
    p(x).refs.fetch_add(1, SeqCst);
}
unsafe extern "C" fn ref_release(x: *mut c_void) {
    p(x).refs.fetch_sub(1, SeqCst);
}
unsafe extern "C" fn op_status(x: *mut c_void) -> c_long {
    *p(x).status.lock().unwrap() as c_long
}
unsafe extern "C" fn op_completion(x: *mut c_void) -> *mut c_void {
    *p(x).completion.lock().unwrap() as *mut c_void
}
unsafe extern "C" fn op_set_completion(x: *mut c_void, c: *mut c_void) {
    *p(x).completion.lock().unwrap() = c as usize;
}
unsafe extern "C" fn op_run_async(x: *mut c_void, t: c_long) {
    log(format!("{} run {t}", p(x).name));
    *p(x).status.lock().unwrap() = WS_PENDING;
}
unsafe extern "C" fn op_kill(x: *mut c_void) {
    log(format!("{} kill", p(x).name));
    *p(x).status.lock().unwrap() = WS_KILLED;
}
fn done(x: *mut c_void, s: c_long, how: &str) {
    let c = *p(x).completion.lock().unwrap();
    let m = *p(x).msg.lock().unwrap();
    log(format!(
        "{} {how} {s} msg {} from {} completion {}",
        p(x).name,
        if m == 0 { "-".into() } else { o(m).name.clone() },
        p(x).addr.lock().unwrap().port(),
        if c == 0 { "-".into() } else { o(c).name.clone() }
    ));
    *p(x).status.lock().unwrap() = s as Status;
}
unsafe extern "C" fn op_finish(x: *mut c_void, s: c_long) {
    done(x, s, "finish");
}
unsafe extern "C" fn op_force_finish(x: *mut c_void, s: c_long) {
    done(x, s, "force");
}
unsafe extern "C" fn op_time_left(_x: *mut c_void) -> u32 {
    77
}
unsafe extern "C" fn recv_address(x: *mut c_void) -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(p(x).addr.lock().unwrap().clone()))
}
unsafe extern "C" fn recv_set_address(x: *mut c_void, a: *const EeWonIpAddr) {
    *p(x).addr.lock().unwrap() = unsafe { &*a }.clone();
}
unsafe extern "C" fn recv_set_msg(x: *mut c_void, b: *mut c_void) {
    *p(x).msg.lock().unwrap() = b as usize;
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
static NEW_OPS: AtomicI32 = AtomicI32::new(0);
unsafe extern "C" fn new_recv_op(a: *const EeWonIpAddr) -> *mut c_void {
    let n = NEW_OPS.fetch_add(1, SeqCst);
    let x = new_obj(&format!("new{n}"), if a.is_null() { None } else { Some(unsafe { &*a }) }, vec![]);
    o(x).refs.fetch_add(1, SeqCst);
    x as *mut c_void
}
static SOCKETS: Mutex<Vec<usize>> = Mutex::new(Vec::new());
unsafe extern "C" fn socket_new() -> *mut c_void {
    let x = new_obj("socket", None, vec![]);
    o(x).refs.fetch_add(1, SeqCst);
    SOCKETS.lock().unwrap().push(x);
    x as *mut c_void
}
unsafe extern "C" fn socket_kill(_x: *mut c_void) {
    log("socket kill".into());
}
unsafe extern "C" fn socket_set_broadcast(_x: *mut c_void, on: bool) {
    log(format!("broadcast {on}"));
}
unsafe extern "C" fn socket_bind(_x: *mut c_void, port: i32) -> c_long {
    log(format!("bind {port}"));
    if port == 1 {
        10048
    } else {
        0
    }
}
unsafe extern "C" fn socket_repeat(_x: *mut c_void, m: *mut c_void, size: i32) {
    log(format!("repeat {} {size}", m as usize));
}
unsafe extern "C" fn socket_send(_x: *mut c_void, b: *mut c_void, a: *const EeWonIpAddr) {
    log(format!("send {} to {}", p(b).name, unsafe { &*a }.port()));
}

static HOOKS: EeWonUdpHooks = EeWonUdpHooks {
    ref_add: Some(ref_add),
    ref_release: Some(ref_release),
    op_status: Some(op_status),
    op_completion: Some(op_completion),
    op_set_completion: Some(op_set_completion),
    op_run_async: Some(op_run_async),
    op_kill: Some(op_kill),
    op_finish: Some(op_finish),
    op_force_finish: Some(op_force_finish),
    op_time_left: Some(op_time_left),
    recv_address: Some(recv_address),
    recv_set_address: Some(recv_set_address),
    recv_set_msg: Some(recv_set_msg),
    recv_check: Some(recv_check),
    new_recv_op: Some(new_recv_op),
    socket_new: Some(socket_new),
    socket_kill: Some(socket_kill),
    socket_set_broadcast: Some(socket_set_broadcast),
    socket_bind: Some(socket_bind),
    socket_repeat: Some(socket_repeat),
    socket_send: Some(socket_send),
};

fn at(port: u16) -> EeWonIpAddr {
    let mut a = EeWonIpAddr::default();
    a.set_host_order(0x7f00_0001, port);
    a
}

#[test]
fn the_dispatch_by_sender_then_any_then_auto() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    NEW_OPS.store(0, SeqCst);
    let m = EeWonUdpManager::new(5 as *mut c_void, &HOOKS);
    let def = new_obj("default", None, vec![]);
    m.set_default_completion(def);
    // unbound: run and killed
    let early = new_obj("early", None, vec![]);
    m.queue_op(early, 1000);
    assert_eq!(take(), ["early run 1000", "early kill"]);
    assert_eq!(m.bind(0, false), WS_SUCCESS);
    assert_eq!(take(), ["bind 0", "repeat 5 1024"]);
    m.set_auto_recv(true);
    // two ops for port 9 (the later after the earlier), one for port 8, one for any sender
    let a1 = new_obj("a1", Some(&at(9)), vec![(b'A', WS_SUCCESS), (b'R', WS_SERVER_REQ_RECV)]);
    let a2 = new_obj("a2", Some(&at(9)), vec![(b'B', 7)]);
    let b = new_obj("b", Some(&at(8)), vec![(b'A', WS_SUCCESS)]);
    let any = new_obj("any", None, vec![(b'Q', WS_SUCCESS)]);
    for x in [a1, a2, b, any] {
        m.queue_op(x, 60000);
    }
    assert_eq!(take(), ["a1 run 60000", "a2 run 60000", "b run 60000", "any run 60000"]);
    assert_eq!(*o(a1).completion.lock().unwrap(), def);
    let d = new_obj("dgram", None, vec![]);
    // to the second op for port 9
    m.recv(d, b"B", &at(9));
    assert_eq!(take(), ["a1 checks B", "a2 checks B", "a2 finish 7 msg dgram from 9 completion default"]);
    // a throwing check passes on; nobody takes it: the auto-receive op, a new one armed
    m.recv(d, b"X", &at(9));
    assert_eq!(take(), ["a1 checks X", "any checks X", "new0 force 0 msg dgram from 9 completion default"]);
    // WS_ServerReq_Recv: a new op for the sender, which takes it (no check of its own: the stand-in's none)
    m.recv(d, b"R", &at(9));
    assert_eq!(
        take(),
        [
            "a1 checks R",
            "new2 run 77",
            "new2 checks R",
            "any checks R",
            "new1 force 0 msg dgram from 9 completion default"
        ]
    );
    // the any op takes a datagram from anyone and keeps the sender
    m.recv(d, b"Q", &at(3));
    assert_eq!(take(), ["any checks Q", "any finish 0 msg dgram from 3 completion default"]);
    // a finished op met on the way is dropped
    *o(a1).status.lock().unwrap() = WS_SUCCESS;
    m.recv(d, b"A", &at(9));
    assert_eq!(take(), ["new2 checks A", "new3 force 0 msg dgram from 9 completion default"]);
    assert_eq!(o(a1).refs.load(SeqCst), 0);
    m.send(d, &at(4));
    assert_eq!(take(), ["send dgram to 4"]);
    // close: the socket, the auto op and the waiting ops killed
    m.close();
    // (the map in the wire order: the port first)
    assert_eq!(take(), ["socket kill", "new4 kill", "b kill", "new2 kill"]);
    assert_eq!(m.socket(), 0);
    m.close();
    assert_eq!(take(), Vec::<String>::new());
    // a refused bind keeps the socket, no repeat
    assert_eq!(m.bind(1, true), 10048);
    assert_eq!(take(), ["broadcast true", "bind 1"]);
    drop(m);
    for x in [def, early, a2, b, any] {
        assert_eq!(o(x).refs.load(SeqCst), 0, "{}", o(x).name);
    }
    for s in SOCKETS.lock().unwrap().drain(..) {
        assert_eq!(o(s).refs.load(SeqCst), 0);
    }
}

#[test]
fn the_clean_up_walks_ten_at_a_time_from_its_cursor() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    let m = EeWonUdpManager::new(std::ptr::null_mut(), &HOOKS);
    m.bind(0, false);
    let ops: Vec<usize> = (0..25).map(|i| new_obj(&format!("o{i}"), None, vec![])).collect();
    for &x in &ops {
        m.queue_op(x, 10);
    }
    take();
    // all finish; the cursor sat at the end (the list's first clean-up found it there and moved it to the start)
    for &x in &ops {
        *o(x).status.lock().unwrap() = WS_SUCCESS;
    }
    let live = || ops.iter().filter(|&&x| o(x).refs.load(SeqCst) > 0).count();
    // each call drops at most 10 from where the last one stopped; reaching the end it goes back to the start and
    // stops (the queueing's clean-ups left the cursor 5 before the end)
    let mut left = vec![];
    for _ in 0..5 {
        m.clean_ops();
        left.push(live());
    }
    assert_eq!(left, [20, 10, 0, 0, 0]);
    // at the end: a call moves the cursor back to the start and stops; the next one drops the op
    let x = new_obj("late", None, vec![]);
    m.queue_op(x, 10);
    *o(x).status.lock().unwrap() = WS_SUCCESS;
    m.clean_ops();
    assert_eq!(o(x).refs.load(SeqCst), 1);
    m.clean_ops();
    assert_eq!(o(x).refs.load(SeqCst), 0);
    m.close();
    drop(m);
    SOCKETS.lock().unwrap().clear();
}
