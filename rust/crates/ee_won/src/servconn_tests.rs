//! `EeWonServerConn` over scripted C++ objects: the connects, the connect callback's choice, the keep-alive's
//! timers and no-ops, the close and kill hooks (with a kill that calls back into the object) and the references it
//! holds (every one released at the end).

use super::*;
use std::sync::Mutex;

/// a scripted op, completion or auth context
struct Obj {
    name: String,
    refs: i32,
    status: Status,
    killed: bool,
    completion: usize,
}

static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
static LOCK: Mutex<()> = Mutex::new(());
/// the connection the scripted kill calls back into (a timer's or a peer op's completion)
static CONN: Mutex<usize> = Mutex::new(0);
/// what a peer op's run ends with
static RUN_STATUS: Mutex<Status> = Mutex::new(WS_SUCCESS);

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
    Box::into_raw(Box::new(Obj { name: name.into(), refs: 0, status: WS_PENDING, killed: false, completion: 0 }))
        as *mut c_void
}
fn name(p: usize) -> String {
    if p == 0 {
        "null".into()
    } else {
        obj(p as *mut c_void).name.clone()
    }
}
fn conn() -> &'static EeWonServerConn {
    // SAFETY: set by the test for its scope
    unsafe { &*(*CONN.lock().unwrap() as *const EeWonServerConn) }
}

unsafe extern "C" fn add_ref(p: *mut c_void) {
    obj(p).refs += 1;
}
unsafe extern "C" fn release(p: *mut c_void) {
    obj(p).refs -= 1;
}
unsafe extern "C" fn op_status(p: *mut c_void) -> c_long {
    obj(p).status as c_long
}
unsafe extern "C" fn op_killed(p: *mut c_void) -> bool {
    obj(p).killed
}
/// a kill finishes a pending op killed and calls its completion: a timer's or a peer op's reaches the connection
unsafe extern "C" fn op_kill(p: *mut c_void) {
    log(format!("kill {}", name(p as usize)));
    let o = obj(p);
    if o.status != WS_PENDING {
        return;
    }
    o.status = WS_KILLED;
    o.killed = true;
    let c = o.completion;
    if c == 0 {
        return;
    }
    match name(c).as_str() {
        "keepalive cb" => conn().keep_alive_callback(p as usize),
        "connect cb" => conn().connect_callback(p as usize),
        other => log(format!("{other} completes {}", name(p as usize))),
    }
}
unsafe extern "C" fn op_set_completion(op: *mut c_void, c: *mut c_void) {
    log(format!("{} completion {}", name(op as usize), name(c as usize)));
    obj(op).completion = c as usize;
}
unsafe extern "C" fn op_run_async(op: *mut c_void, timeout: c_long) {
    log(format!("run async {} {timeout}", name(op as usize)));
}
unsafe extern "C" fn op_run_block(op: *mut c_void, timeout: u32) {
    log(format!("run block {} {timeout}", name(op as usize)));
    obj(op).status = *RUN_STATUS.lock().unwrap();
}
unsafe extern "C" fn complete(c: *mut c_void, op: *mut c_void) {
    log(format!("{} completes {} {}", name(c as usize), name(op as usize), obj(op).status));
}
unsafe extern "C" fn new_peer_auth(
    _conn: *mut c_void,
    address: *const c_void,
    context: *mut c_void,
    auth: i32,
) -> *mut c_void {
    // SAFETY: the test passes a &str's bytes as the address
    let a = unsafe { *(address as *const &str) };
    log(format!("new peer {a} context {} auth {auth}", name(context as usize)));
    new_obj(&format!("peer {a}"))
}
unsafe extern "C" fn route_connect(_conn: *mut c_void, op: *mut c_void) {
    let cb = new_obj("connect cb");
    unsafe { op_set_completion(op, cb) };
}
unsafe extern "C" fn new_keep_alive(_conn: *mut c_void) -> *mut c_void {
    let t = new_obj("timer");
    obj(t).completion = new_obj("keepalive cb") as usize;
    t
}
unsafe extern "C" fn start_messages(_conn: *mut c_void) {
    log("start messages".into());
}
unsafe extern "C" fn stop_messages(_conn: *mut c_void) {
    log("stop messages".into());
}
unsafe extern "C" fn queue_message(_conn: *mut c_void, bytes: *const u8, length: usize) {
    // SAFETY: the object's bytes for the call
    let b = unsafe { std::slice::from_raw_parts(bytes, length) };
    log(format!("queue {b:?}"));
}
unsafe extern "C" fn close(_conn: *mut c_void) {
    log("close".into());
    // the socket's close runs the close hook, as AsyncSocket::Close does
    conn().close_hook();
}
unsafe extern "C" fn length_field_size(_conn: *mut c_void) -> u8 {
    2
}
unsafe extern "C" fn queue_close_hook(_conn: *mut c_void) {
    log("queue close hook".into());
}
unsafe extern "C" fn queue_kill_hook(_conn: *mut c_void) {
    log("queue kill hook".into());
}

static HOOKS: EeWonServerConnHooks = EeWonServerConnHooks {
    add_ref: Some(add_ref),
    release: Some(release),
    op_status: Some(op_status),
    op_killed: Some(op_killed),
    op_kill: Some(op_kill),
    op_set_completion: Some(op_set_completion),
    op_run_async: Some(op_run_async),
    op_run_block: Some(op_run_block),
    complete: Some(complete),
    new_peer_auth: Some(new_peer_auth),
    route_connect: Some(route_connect),
    new_keep_alive: Some(new_keep_alive),
    start_messages: Some(start_messages),
    stop_messages: Some(stop_messages),
    queue_message: Some(queue_message),
    close: Some(close),
    length_field_size: Some(length_field_size),
    queue_close_hook: Some(queue_close_hook),
    queue_kill_hook: Some(queue_kill_hook),
};

fn with_conn(f: impl FnOnce(&EeWonServerConn)) {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    *RUN_STATUS.lock().unwrap() = WS_SUCCESS;
    // SAFETY: a fresh platform mutex for the test (recursive, as the socket's)
    let lock = unsafe { plat::PlatMutexCreate() };
    let c = Box::new(EeWonServerConn::new(null_mut(), &HOOKS, lock));
    *CONN.lock().unwrap() = &*c as *const EeWonServerConn as usize;
    f(&c);
    drop(c);
    *CONN.lock().unwrap() = 0;
    // SAFETY: created above, no longer used
    unsafe { plat::PlatMutexDestroy(lock) };
}

#[test]
fn a_blocking_connect_completes_receives_and_keeps_alive() {
    with_conn(|c| {
        let ctx = new_obj("ctx");
        let done = new_obj("done");
        c.set_auth(ctx, 2);
        c.set_connect_completion(done);
        c.set_keep_alive_period(2);
        assert_eq!(obj(ctx).refs, 1);
        let addr = "a";
        let st = c.connect_block(&addr as *const &str as *const c_void, 5000);
        assert_eq!(st, WS_SUCCESS);
        assert_eq!(
            take(),
            [
                "close",
                "stop messages",
                "queue close hook",
                "new peer a context ctx auth 2",
                "run block peer a 5000",
                "done completes peer a 0",
                "start messages",
                "run async timer 2000",
            ]
        );
        // the peer op went from the slot (no completion was set on it: a blocking connect calls back itself)
        assert_eq!(c.st().peer_op, 0);
        let timer = c.st().keep_alive_op as *mut c_void;
        assert_eq!(obj(timer).refs, 1);
        // the timer fires: a no-op in the socket's 2-byte length field, the next timer
        obj(timer).status = WS_SUCCESS;
        c.keep_alive_callback(timer as usize);
        assert_eq!(take(), ["queue [5, 0, 3, 1, 7]", "run async timer 2000"]);
        assert_eq!(obj(timer).refs, 0);
        // an old timer's callback is ignored
        c.keep_alive_callback(timer as usize);
        assert!(take().is_empty());
        // a period of 0: the next restart makes no timer (the running one killed)
        c.set_keep_alive_period(0);
        c.start_keep_alive();
        assert_eq!(take(), ["kill timer"]);
        assert_eq!(c.st().keep_alive_op, 0);
    });
}

#[test]
fn a_failed_connect_completes_without_receiving() {
    with_conn(|c| {
        let done = new_obj("done");
        c.set_connect_completion(done);
        *RUN_STATUS.lock().unwrap() = WS_TIMED_OUT;
        let addr = "b";
        assert_eq!(c.connect_block(&addr as *const &str as *const c_void, 10), WS_TIMED_OUT);
        let t = take();
        assert_eq!(t[3..], ["new peer b context null auth 0", "run block peer b 10", "done completes peer b 100001"]);
        assert_eq!(c.st().keep_alive_op, 0);
    });
}

#[test]
fn an_async_connect_routes_its_completion_and_only_the_current_one_counts() {
    with_conn(|c| {
        let done = new_obj("done");
        c.set_connect_completion(done);
        let a = "a";
        let op = c.connect_async(&a as *const &str as *const c_void, 4000);
        assert_eq!(name(op as usize), "peer a");
        assert_eq!(obj(op).refs, 1);
        assert_eq!(
            take()[3..],
            ["new peer a context null auth 0", "peer a completion connect cb", "run async peer a 4000"]
        );
        // a callback for another op is ignored
        let other = new_obj("other");
        c.connect_callback(other as usize);
        assert!(take().is_empty());
        // a second connect closes: the pending one gets the connect completion and is killed
        let b = "b";
        let op2 = c.connect_async(&b as *const &str as *const c_void, 4000);
        assert_eq!(
            take(),
            [
                "close",
                "peer a completion done",
                "kill peer a",
                "done completes peer a",
                "stop messages",
                "queue close hook",
                "new peer b context null auth 0",
                "peer b completion connect cb",
                "run async peer b 4000",
            ]
        );
        assert_eq!(obj(op).refs, 0);
        // its success: the completion, the messages, the 300-second keep-alive
        obj(op2).status = WS_SUCCESS;
        c.connect_callback(op2 as usize);
        assert_eq!(take(), ["done completes peer b 0", "start messages", "run async timer 300000"]);
        assert_eq!(obj(op2).refs, 0);
    });
}

#[test]
fn the_close_hook_kills_the_timer_through_its_callback_and_the_kill_hook_drops_the_rest() {
    with_conn(|c| {
        let ctx = new_obj("ctx");
        let done = new_obj("done");
        c.set_auth(ctx, 1);
        c.set_connect_completion(done);
        c.start_keep_alive();
        let timer = c.st().keep_alive_op as *mut c_void;
        take();
        // the kill completes the timer: its callback drops it from the slot (killed: no no-op)
        c.close_hook();
        assert_eq!(take(), ["kill timer", "stop messages", "queue close hook"]);
        assert_eq!(obj(timer).refs, 0);
        assert!(obj(timer).killed);
        c.kill_hook();
        assert_eq!(take(), ["queue kill hook"]);
        assert_eq!(obj(ctx).refs, 0);
        assert_eq!(obj(done).refs, 0);
        // a finished peer op at the close is handed the completion but not killed
        let a = "a";
        c.set_connect_completion(done);
        let op = c.connect_async(&a as *const &str as *const c_void, 1);
        obj(op).status = WS_TIMED_OUT;
        take();
        c.close_hook();
        assert_eq!(take(), ["peer a completion done", "stop messages", "queue close hook"]);
        assert_eq!(obj(op).refs, 0);
    });
}

#[test]
fn the_object_releases_what_it_holds() {
    let mut held = Vec::new();
    with_conn(|c| {
        let ctx = new_obj("ctx");
        let done = new_obj("done");
        c.set_auth(ctx, 1);
        c.set_connect_completion(done);
        let a = "a";
        let op = c.connect_async(&a as *const &str as *const c_void, 1);
        c.start_keep_alive();
        held = vec![ctx, done, op, c.st().keep_alive_op as *mut c_void];
        for &h in &held {
            assert_eq!(obj(h).refs, 1, "{}", name(h as usize));
        }
    });
    for h in held {
        assert_eq!(obj(h).refs, 0, "{}", name(h as usize));
    }
}
