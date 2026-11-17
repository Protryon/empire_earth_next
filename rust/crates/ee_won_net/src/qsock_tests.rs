//! `EeWonQueueSocket` over scripted C++ objects: the queue's order, the completions it routes, the repeat op's
//! reruns, the close hook's kills and the references it holds (every one released at the end).

use super::*;
use std::sync::Mutex;

/// a scripted op, completion or socket
struct Obj {
    name: String,
    refs: i32,
    status: Status,
    killed: bool,
    completion: usize,
    timeout: u32,
    /// a completion's: the fast completion's original
    original: usize,
}

static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
static OBJS: Mutex<Vec<usize>> = Mutex::new(Vec::new());
static LOCK: Mutex<()> = Mutex::new(());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}
fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}
fn obj<'a>(p: *mut c_void) -> &'a mut Obj {
    // SAFETY: the test's leaked objects
    unsafe { &mut *(p as *mut Obj) }
}
fn new_obj(name: &str) -> *mut c_void {
    let p = Box::into_raw(Box::new(Obj {
        name: name.into(),
        refs: 0,
        status: WS_NONE,
        killed: false,
        completion: 0,
        timeout: u32::MAX,
        original: 0,
    })) as usize;
    OBJS.lock().unwrap().push(p);
    p as *mut c_void
}
fn name(p: usize) -> String {
    if p == 0 {
        "null".into()
    } else {
        obj(p as *mut c_void).name.clone()
    }
}

unsafe extern "C" fn add_ref(p: *mut c_void) {
    obj(p).refs += 1;
}
unsafe extern "C" fn release(p: *mut c_void) {
    obj(p).refs -= 1;
}
unsafe extern "C" fn completion_call(c: *mut c_void, op: *mut c_void) {
    log(format!("{} completes {} {}", obj(c).name, obj(op).name, obj(op).status));
}
unsafe extern "C" fn op_status(op: *mut c_void) -> c_long {
    obj(op).status as c_long
}
unsafe extern "C" fn op_killed(op: *mut c_void) -> bool {
    obj(op).killed
}
unsafe extern "C" fn op_timeout(op: *mut c_void) -> u32 {
    obj(op).timeout
}
unsafe extern "C" fn op_completion(op: *mut c_void) -> *mut c_void {
    obj(op).completion as *mut c_void
}
unsafe extern "C" fn op_set_completion(op: *mut c_void, c: *mut c_void) {
    if !c.is_null() {
        obj(c).refs += 1;
    }
    let old = std::mem::replace(&mut obj(op).completion, c as usize);
    if old != 0 {
        obj(old as *mut c_void).refs -= 1;
    }
}
unsafe extern "C" fn op_set_timeout(op: *mut c_void, t: u32) {
    obj(op).timeout = t;
}
unsafe extern "C" fn op_run_async(op: *mut c_void, t: c_long) {
    log(format!("run {} {} via {}", obj(op).name, t, name(obj(op).completion)));
    obj(op).status = WS_PENDING;
}
unsafe extern "C" fn op_kill(op: *mut c_void) {
    log(format!("kill {} via {}", obj(op).name, name(obj(op).completion)));
    obj(op).killed = true;
    obj(op).status = WS_KILLED;
}
unsafe extern "C" fn op_force_finish(op: *mut c_void, s: c_long) {
    log(format!("force {} {} via {}", obj(op).name, s, name(obj(op).completion)));
}
unsafe extern "C" fn op_set_socket(_op: *mut c_void, _s: *mut c_void) {}
unsafe extern "C" fn op_duplicate(op: *mut c_void) -> *mut c_void {
    let n = format!("{}'", obj(op).name);
    log(format!("dup {n}"));
    new_obj(&n)
}
unsafe extern "C" fn fast_original(c: *mut c_void) -> *mut c_void {
    obj(c).original as *mut c_void
}
unsafe extern "C" fn new_op_completion(_s: *mut c_void) -> *mut c_void {
    new_obj("socket")
}
unsafe extern "C" fn new_fast_completion(_s: *mut c_void, original: *mut c_void) -> *mut c_void {
    let c = new_obj("fast");
    obj(c).original = original as usize;
    if !original.is_null() {
        obj(original).refs += 1;
    }
    c
}
unsafe extern "C" fn new_close_timer(_s: *mut c_void) -> *mut c_void {
    new_obj("timer")
}
unsafe extern "C" fn new_queue_close_op(_s: *mut c_void) -> *mut c_void {
    new_obj("queueclose")
}
unsafe extern "C" fn new_close_op(_s: *mut c_void) -> *mut c_void {
    new_obj("closeop")
}
unsafe extern "C" fn socket_close(_s: *mut c_void) {
    log("socket close".into());
}
unsafe extern "C" fn socket_valid(_s: *mut c_void) -> bool {
    true
}
unsafe extern "C" fn socket_datagram(_s: *mut c_void) -> bool {
    false
}

static HOOKS: EeWonQueueSocketHooks = EeWonQueueSocketHooks {
    completion_add_ref: Some(add_ref),
    completion_release: Some(release),
    completion_call: Some(completion_call),
    op_add_ref: Some(add_ref),
    op_release: Some(release),
    op_status: Some(op_status),
    op_killed: Some(op_killed),
    op_timeout: Some(op_timeout),
    op_completion: Some(op_completion),
    op_set_completion: Some(op_set_completion),
    op_set_timeout: Some(op_set_timeout),
    op_run_async: Some(op_run_async),
    op_kill: Some(op_kill),
    op_force_finish: Some(op_force_finish),
    op_set_socket: Some(op_set_socket),
    op_duplicate: Some(op_duplicate),
    fast_original: Some(fast_original),
    new_op_completion: Some(new_op_completion),
    new_fast_completion: Some(new_fast_completion),
    new_close_timer: Some(new_close_timer),
    new_queue_close_op: Some(new_queue_close_op),
    new_close_op: Some(new_close_op),
    socket_close: Some(socket_close),
    socket_valid: Some(socket_valid),
    socket_datagram: Some(socket_datagram),
};

fn queue() -> EeWonQueueSocket {
    // SAFETY: a plain platform call (leaked with the test)
    let lock = unsafe { plat::PlatMutexCreate() };
    EeWonQueueSocket::new(new_obj("q"), &HOOKS, lock)
}

#[test]
fn the_queue_runs_its_ops_in_order() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    let q = queue();
    let default = new_obj("default");
    q.set_queue_completion(default);
    let a = new_obj("a");
    let b = new_obj("b");
    let own = new_obj("own");
    unsafe { op_set_completion(b, own) };
    q.queue_op(a, 100);
    q.queue_op(b, 200);
    assert_eq!(take(), ["run a 100 via socket"]);
    obj(a).status = WS_SUCCESS;
    q.op_callback(a);
    assert_eq!(take(), ["run b 200 via socket", "default completes a 0"]);
    // the close hook kills the running op with its own completion back
    let c = new_obj("c");
    q.queue_op(c, 300);
    q.close_hook();
    assert_eq!(take(), ["kill b via own", "kill c via default"]);
    // the killed queued op's callback: a close (the socket is not closing by itself here)
    assert_eq!(obj(b).refs, 0);
    drop(q);
    for o in [a, b, c] {
        assert_eq!(obj(o).refs, 0, "{}", obj(o).name);
    }
    OBJS.lock().unwrap().clear();
}

#[test]
fn the_repeat_op_reruns_its_copies_and_a_failure_closes() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    let q = queue();
    let rep = new_obj("repeat");
    let close = new_obj("close");
    q.set_repeat_completion(rep);
    q.set_close_completion(close);
    let r = new_obj("r");
    q.set_repeat_op(r);
    assert_eq!(take(), ["run r -1 via socket"]);
    obj(r).status = WS_SUCCESS;
    q.op_callback(r);
    assert_eq!(take(), ["dup r'", "run r' -1 via socket", "repeat completes r 0"]);
    assert_eq!(obj(r).refs, 0);
    // a failure of the copy drops the repeat op, closes the socket and reports it
    let r2 = *q.slot(Slot::RepeatOp) as *mut c_void;
    obj(r2).status = WS_TIMED_OUT;
    q.op_callback(r2);
    assert_eq!(take(), ["repeat completes r' 100001", "socket close", "force closeop 100001 via close"]);
    assert_eq!(*q.slot(Slot::RepeatOp), 0);
    // a queued close kills the repeat op (a new one here) and queues its own op
    let r3 = new_obj("r3");
    q.set_repeat_op(r3);
    q.queue_close(-1);
    assert_eq!(take(), ["run r3 -1 via socket", "kill r3 via socket", "run queueclose -1 via socket"]);
    // the close timer: started, replaced, expired
    q.start_close_timer(50);
    let t1 = *q.slot(Slot::CloseTimer) as *mut c_void;
    q.start_close_timer(60);
    let t2 = *q.slot(Slot::CloseTimer) as *mut c_void;
    assert_eq!(take(), ["run timer 50 via null", "kill timer via null", "run timer 60 via null"]);
    obj(t1).status = WS_TIMED_OUT;
    q.close_timer_callback(t1);
    assert!(take().is_empty(), "an old timer does nothing");
    obj(t2).status = WS_TIMED_OUT;
    q.close_timer_callback(t2);
    assert_eq!(take(), ["socket close", "force closeop 100002 via close"]);
    assert_eq!(obj(t2).refs, 0);
    q.kill_hook();
    drop(q);
    assert_eq!(obj(rep).refs, 0);
    assert_eq!(obj(close).refs, 2, "the two scripted close ops keep their completion");
    OBJS.lock().unwrap().clear();
}

#[test]
fn a_fast_op_runs_now_and_routes_to_its_original() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    let q = queue();
    let own = new_obj("own");
    let f = new_obj("f");
    unsafe { op_set_completion(f, own) };
    q.queue_op_fast(f, 70);
    assert_eq!(take(), ["run f 70 via fast"]);
    assert_eq!(*q.slot(Slot::QueueOp), f as usize, "pending: it is the queued op");
    obj(f).status = WS_SUCCESS;
    // an op that is neither the queued nor the repeat op goes to its fast completion's original
    let g = new_obj("g");
    unsafe { op_set_completion(g, own) };
    let fast = unsafe { new_fast_completion(null_mut(), own) };
    unsafe { op_set_completion(g, fast) };
    obj(g).status = WS_SUCCESS;
    q.op_callback(g);
    assert_eq!(take(), ["own completes g 0"]);
    drop(q);
    OBJS.lock().unwrap().clear();
}

#[test]
fn a_lan_socket_links_its_client_both_ways() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    static LINKS: Mutex<Vec<(usize, usize)>> = Mutex::new(Vec::new());
    unsafe extern "C" fn link(c: *mut c_void, s: *mut c_void) {
        LINKS.lock().unwrap().push((c as usize, s as usize));
    }
    static LAN: EeWonLanSocketHooks = EeWonLanSocketHooks {
        client_add_ref: Some(add_ref),
        client_release: Some(release),
        client_set_socket: Some(link),
    };
    let socket = new_obj("lan");
    let l = EeWonLanSocket::new(socket, &LAN);
    assert!(l.client().is_null() && l.last_message_time() == 0);
    let a = new_obj("a");
    let b = new_obj("b");
    l.set_client(a);
    l.set_client(b);
    assert_eq!((obj(a).refs, obj(b).refs), (0, 1));
    let (a, b, s) = (a as usize, b as usize, socket as usize);
    assert_eq!(*LINKS.lock().unwrap(), [(a, s), (a, 0), (b, s)]);
    l.set_last_message_time(1234);
    assert_eq!(l.last_message_time(), 1234);
    drop(l);
    assert_eq!(obj(b as *mut c_void).refs, 0);
    assert_eq!(LINKS.lock().unwrap().len(), 3, "no unlink at the destruction");
    OBJS.lock().unwrap().clear();
}
