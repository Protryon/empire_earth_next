//! The op's own calls over a stand-in C++ side: the op's own object and tracker are real Rust objects (with logging
//! hooks), its children real op objects behind stand-in C++ ops, and the side answers only what stays C++'s (an
//! object's Rust object, a new plain op), each logged. The answers against the original's are `won_opcore_test`'s.

use super::*;
use crate::opobj::{ee_won_completion_free, ee_won_completion_run, EeWonAsyncOpHooks};
use crate::ops;
use std::cell::RefCell;
use std::sync::Mutex as StdMutex;

static LOCK: StdMutex<()> = StdMutex::new(());

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    /// the op under test (its callback hook's `RustServerOp::CallbackHook`)
    static HOST: RefCell<usize> = const { RefCell::new(0) };
    /// stand-in C++ ops (ids from 100) and their objects
    static CHILDREN: RefCell<Vec<(usize, usize)>> = const { RefCell::new(Vec::new()) };
}
fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}
fn take() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

unsafe extern "C" fn quiet(_: *mut c_void) {}
unsafe extern "C" fn timer_add(_: *mut c_void, _: u32) {}
unsafe extern "C" fn c_add(c: *mut c_void) {
    log(format!("cref+ {}", name(c)));
}
unsafe extern "C" fn c_release(c: *mut c_void) {
    log(format!("cref- {}", name(c)));
}
unsafe extern "C" fn c_call(c: *mut c_void, op: *mut c_void) {
    log(format!("complete {} {}", name(c), op as usize));
}
unsafe extern "C" fn r_add(o: *mut c_void) {
    log(format!("ref+ {}", o as usize));
}
unsafe extern "C" fn r_release(o: *mut c_void) {
    log(format!("ref- {}", o as usize));
}
thread_local! {
    static BODIES: RefCell<Vec<usize>> = const { RefCell::new(Vec::new()) };
}
/// a completion made by Rust: named `c<n>` in the log
fn name(c: *mut c_void) -> String {
    BODIES
        .with(|b| b.borrow().iter().position(|&x| x == c as usize))
        .map_or((c as usize).to_string(), |i| format!("c{i}"))
}
unsafe extern "C" fn new_completion(body: *mut EeWonCompletion) -> *mut c_void {
    BODIES.with(|b| b.borrow_mut().push(body as usize));
    log(format!("new {}", name(body as *mut c_void)));
    body as *mut c_void
}
unsafe extern "C" fn callback_hook(_op: *mut c_void, child: *mut c_void, param: i32) -> bool {
    let h = HOST.with(|h| *h.borrow());
    unsafe { crate::hostop::ee_won_hostop_callback(h as *const EeWonHostOp, param, child) }
}

static HOOKS: EeWonAsyncOpHooks = EeWonAsyncOpHooks {
    run_hook: Some(quiet),
    cleanup_hook: Some(quiet),
    core_queue: Some(quiet),
    timer_add: Some(timer_add),
    timer_remove: Some(quiet),
    completion_add_ref: Some(c_add),
    completion_release: Some(c_release),
    completion_call: Some(c_call),
    op_add_ref: Some(r_add),
    op_release: Some(r_release),
    op_kill: Some(quiet),
    ref_add: Some(r_add),
    ref_release: Some(r_release),
    new_completion: Some(new_completion),
    callback_hook: Some(callback_hook),
};

/// A plan recording its callbacks.
struct Probe;
impl Plan for Probe {
    fn run(_: &Host<Self>) {}
    fn callback(_: &Host<Self>, id: i32, op: Obj) -> bool {
        log(format!("callback {id} {}", op.0));
        true
    }
    fn cleanup(_: &Host<Self>) {}
    fn setup(&self) -> (u8, [u32; 3]) {
        (2, [20000, 30000, 40000])
    }
    fn describe(&self) -> String {
        String::new()
    }
    fn take_objects(&mut self) -> Vec<Obj> {
        Vec::new()
    }
}

/// The stand-in C++ side: a child's object (`SOCK_CORE` kind OP), a new plain op; the rest logged.
unsafe extern "C" fn side(_host: *mut c_void, c: *mut EeWonHostCall) {
    let c = unsafe { &mut *c };
    match c.code {
        EE_WON_HOST_SOCK_CORE => {
            let o = c.obj[0] as usize;
            c.out_obj = if c.num[0] == EE_WON_HOST_CORE_OP {
                CHILDREN.with(|k| k.borrow().iter().find(|e| e.0 == o).map_or(0, |e| e.1)) as *mut c_void
            } else {
                std::ptr::null_mut()
            };
        }
        EE_WON_HOST_PEER_NEW_LIGHT => {
            let id = new_child();
            log(format!("new op {id}"));
            c.out_obj = id as *mut c_void;
        }
        _ => log(format!("sent {}", c.code)),
    }
}

fn new_child() -> usize {
    CHILDREN.with(|k| {
        let mut k = k.borrow_mut();
        let id = 100 + k.len();
        let o = Box::leak(Box::new(EeWonAsyncOp::new(id as *mut c_void, &HOOKS)));
        k.push((id, o as *const EeWonAsyncOp as usize));
        id
    })
}
fn child(id: usize) -> &'static EeWonAsyncOp {
    let p = CHILDREN.with(|k| k.borrow().iter().find(|e| e.0 == id).map(|e| e.1)).expect("a child");
    unsafe { &*(p as *const EeWonAsyncOp) }
}

/// A natively bound op (C++ op 7) with its own object and tracker.
struct Bound {
    op: *mut EeWonHostOp,
    own: Box<EeWonAsyncOp>,
    tracker: Box<EeWonOpTracker>,
}
fn bound() -> Bound {
    CHILDREN.with(|k| k.borrow_mut().clear());
    BODIES.with(|b| b.borrow_mut().clear());
    let op = EeWonHostOp::boxed(Probe);
    let own = Box::new(EeWonAsyncOp::new(7 as *mut c_void, &HOOKS));
    let tracker = Box::new(EeWonOpTracker::new(&HOOKS));
    unsafe {
        crate::hostop::ee_won_hostop_bind_native(
            op,
            Some(side),
            std::ptr::null_mut(),
            7 as *mut c_void,
            &*own,
            &*tracker,
        )
    };
    HOST.with(|h| *h.borrow_mut() = op as usize);
    take();
    Bound { op, own, tracker }
}
impl Bound {
    fn call(&self, code: i32, o: [usize; 2], n: [i64; 2]) -> Reply {
        let h = unsafe { &*self.op };
        h.call(code, [Obj(o[0]), Obj(o[1]), Obj::none()], n, &[], [0; 6])
    }
    fn num(&self, code: i32) -> i64 {
        self.call(code, [0, 0], [0, 0]).num
    }
}
impl Drop for Bound {
    fn drop(&mut self) {
        unsafe { crate::hostop::ee_won_hostop_free(self.op) };
    }
}

#[test]
fn own_values_and_members() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let b = bound();
    assert_eq!(b.num(EE_WON_HOST_STATUS), b.own.status());
    assert_eq!(b.num(EE_WON_HOST_MODE), 1);
    assert_eq!(b.num(EE_WON_HOST_TIMEOUT), 0xffff_ffff);
    assert_eq!(b.num(EE_WON_HOST_TIME_LEFT), 0);
    // the plan's setup: the length field and limits; no auth; an 8-byte key not used; no socket
    assert_eq!(b.num(EE_WON_HOST_LFS), 2);
    assert_eq!(
        [EE_WON_HOST_CONNECT_TIME, EE_WON_HOST_SEND_TIME, EE_WON_HOST_RECV_TIME].map(|c| b.num(c)),
        [20000, 30000, 40000]
    );
    assert_eq!((b.num(EE_WON_HOST_AUTH_TYPE), b.num(EE_WON_HOST_CLIENT_KEY)), (0, 8));
    assert!(b.call(EE_WON_HOST_OWN_SOCKET, [0, 0], [0, 0]).obj.is_none());
    let h = unsafe { &*b.op };
    h.base_set(EE_WON_HOSTBASE_CLIENT_KEY_SIZE, 16, Obj::none());
    h.base_set(EE_WON_HOSTBASE_USE_CLIENT_KEY, 1, Obj::none());
    assert_eq!(b.num(EE_WON_HOST_CLIENT_KEY), 0x110);
    h.base_set(EE_WON_HOSTBASE_AUTH_TYPE, 3, Obj::none());
    h.base_set(EE_WON_HOSTBASE_LFS, 0x104, Obj::none());
    h.base_set(EE_WON_HOSTBASE_MAX_SEND, 7, Obj::none());
    assert_eq!((b.num(EE_WON_HOST_AUTH_TYPE), b.num(EE_WON_HOST_LFS), b.num(EE_WON_HOST_SEND_TIME)), (3, 4, 7));
    // blocking: the limits capped by the time left (an op never run: none left)
    b.own.set_mode(ops::MODE_BLOCK);
    b.own.set_timeout(0);
    assert_eq!(b.num(EE_WON_HOST_SEND_TIME), 0);
    assert_eq!(h.base_get(EE_WON_HOSTBASE_MAX_SEND), 7, "the limit itself stays");
    // nothing went to the C++ side
    assert!(take().is_empty());
}

#[test]
fn object_members_hold_references() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let b = bound();
    let h = unsafe { &*b.op };
    h.base_set(EE_WON_HOSTBASE_SOCKET, 0, Obj(50));
    h.base_set(EE_WON_HOSTBASE_AUTH_CONTEXT, 0, Obj(60));
    assert_eq!(take(), ["ref+ 50", "ref+ 60"]);
    assert_eq!(b.call(EE_WON_HOST_OWN_SOCKET, [0, 0], [0, 0]).obj, Obj(50));
    assert_eq!(h.base_get(EE_WON_HOSTBASE_AUTH_CONTEXT), 60);
    // replaced: the new one's reference first, then the old one's goes
    h.base_set(EE_WON_HOSTBASE_SOCKET, 0, Obj(54));
    h.base_set(EE_WON_HOSTBASE_AUTH_CONTEXT, 0, Obj::none());
    assert_eq!(take(), ["ref+ 54", "ref- 50", "ref- 60"]);
    // the op freed: the context, then the socket
    h.base_set(EE_WON_HOSTBASE_AUTH_CONTEXT, 0, Obj(64));
    take();
    drop(b);
    assert_eq!(take(), ["ref- 64", "ref- 54"]);
}

#[test]
fn track_kill_all_and_the_tracker_completion() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let b = bound();
    let c = new_child();
    assert_eq!(b.num(EE_WON_HOST_TRACKER_EMPTY), 1);
    b.call(EE_WON_HOST_TRACK, [c, 0], [5, 0]);
    assert_eq!(take(), ["ref+ 7", "new c0", "cref+ c0", "ref+ 100"]);
    assert_eq!(b.num(EE_WON_HOST_TRACKER_EMPTY), 0);
    assert_eq!(b.tracker.at(0), (c as *mut c_void, 5));
    // the child completes while the op is pending: it leaves the tracker, the op's callback with its id
    b.own.run(ops::MODE_ASYNC, 0xffff_ffff);
    take();
    let body = child(c).completion();
    unsafe { ee_won_completion_run(body as *const EeWonCompletion, c as *mut c_void) };
    assert_eq!(take(), ["ref- 100", "callback 5 100"]);
    // KillAll and the base's cleanup: each tracked op killed, untracked
    b.call(EE_WON_HOST_TRACK, [c, 0], [6, 0]);
    take();
    b.call(EE_WON_HOST_KILL_ALL, [0, 0], [0, 0]);
    assert_eq!(take(), ["ref- 100"]);
    b.call(EE_WON_HOST_TRACK, [c, 0], [6, 0]);
    take();
    b.call(EE_WON_HOST_BASE_CLEANUP, [0, 0], [0, 0]);
    assert_eq!(take(), ["ref- 100"]);
    assert_eq!(b.num(EE_WON_HOST_TRACKER_EMPTY), 1);
    // a child without an object (the C++ dereferenced it): nothing
    b.call(EE_WON_HOST_TRACK, [999, 0], [1, 0]);
    assert!(take().is_empty());
    for body in BODIES.with(|x| x.borrow().clone()) {
        unsafe { ee_won_completion_free(body as *mut EeWonCompletion) };
    }
    assert_eq!(take(), ["ref- 7", "ref- 7", "ref- 7"]);
}

#[test]
fn timers_and_completions_call_the_op_back() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let b = bound();
    let t = b.call(EE_WON_HOST_NEW_TIMER, [0, 0], [3, 0]).obj.0;
    assert_eq!(t, 100);
    // a new plain op, the completion holding the op, the timer's completion
    assert_eq!(take(), ["new op 100", "ref+ 7", "new c0", "cref+ c0"]);
    let body = child(t).completion();
    unsafe { ee_won_completion_run(body as *const EeWonCompletion, t as *mut c_void) };
    assert_eq!(take(), ["callback 3 100"], "the op's callback whatever its state");
    unsafe { child(t).set_completion(std::ptr::null_mut()) };
    unsafe { ee_won_completion_free(body as *mut EeWonCompletion) };
    assert_eq!(take(), ["cref- c0", "ref- 7"]);
    // a completion called with the op; references
    b.call(EE_WON_HOST_COMPLETE, [33, 0], [0, 0]);
    b.call(EE_WON_HOST_ADD_REF, [44, 0], [0, 0]);
    b.call(EE_WON_HOST_RELEASE, [44, 0], [0, 0]);
    b.call(EE_WON_HOST_RELEASE, [0, 0], [0, 0]);
    assert_eq!(take(), ["complete 33 7", "ref+ 44", "ref- 44"]);
    // Finish
    b.own.run(ops::MODE_ASYNC, 0xffff_ffff);
    b.call(EE_WON_HOST_FINISH, [0, 0], [777, 0]);
    assert_eq!(b.own.status(), 777);
    assert_eq!(b.num(EE_WON_HOST_STATUS), 777);
    // a repeat on no queue socket: nothing made
    take();
    b.call(EE_WON_HOST_SET_REPEAT, [55, 56], [9, 0]);
    assert!(take().is_empty());
}

#[test]
fn other_objects_without_their_rust_objects() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let b = bound();
    // no Rust op, no list behind the objects: zeros, an empty list, nothing sent but the asks
    assert_eq!(b.call(EE_WON_HOST_HOSTOP_NUM, [70, 0], [1, 0]).num, 0);
    assert!(b.call(EE_WON_HOST_HOSTOP_BYTES, [70, 0], [1, 0]).bytes.is_none());
    assert_eq!(b.call(EE_WON_HOST_PEER_AUTH_CERT, [70, 0], [0, 0]).num, 0);
    assert_eq!(b.call(EE_WON_HOST_HTTP_MODIFIED, [70, 0], [0, 0]).num, 0);
    assert_eq!(b.call(EE_WON_HOST_CTX_ADDRS, [71, 0], [0, 0]).bytes, Some(Vec::new()));
    assert!(take().is_empty());
}

#[test]
fn another_ops_members_and_a_server_list() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let (hs, ps) = (b"example.invalid", b"/x");
    let other = unsafe { crate::httpget::ee_won_httpget_new(hs.as_ptr(), hs.len(), ps.as_ptr(), ps.len()) };
    let list = crate::servctx::ServerList::new();
    list.set_shuffle(false);
    for port in [15101u16, 15102, 15103] {
        let mut a = crate::servctx::Address::default();
        a.sockaddr[2..4].copy_from_slice(&port.to_be_bytes());
        a.sockaddr[4..8].copy_from_slice(&[127, 0, 0, 1]);
        a.valid = true;
        assert!(list.add(&a));
    }
    let (other_at, list_at) = (other as usize, &list as *const _ as usize);
    thread_local! { static CORES: RefCell<(usize, usize)> = const { RefCell::new((0, 0)) }; }
    CORES.with(|c| *c.borrow_mut() = (other_at, list_at));
    unsafe extern "C" fn side2(_h: *mut c_void, c: *mut EeWonHostCall) {
        let c = unsafe { &mut *c };
        let (o, l) = CORES.with(|x| *x.borrow());
        if c.code == EE_WON_HOST_SOCK_CORE {
            c.out_obj = match (c.num[0], c.obj[0] as usize) {
                (EE_WON_HOST_CORE_HOSTOP, 80) => o as *mut c_void,
                (EE_WON_HOST_CORE_CONTEXT, 81) => l as *mut c_void,
                _ => std::ptr::null_mut(),
            };
        }
    }
    let b = bound();
    unsafe { crate::hostop::ee_won_hostop_bind(b.op, Some(side2), std::ptr::null_mut()) };
    let host = b.call(EE_WON_HOST_HOSTOP_BYTES, [80, 0], [crate::httpget::EE_WON_HTTPGET_HOST as i64, 0]);
    assert_eq!(host.bytes.as_deref(), Some(&b"example.invalid"[..]));
    let h = unsafe { &*b.op };
    h.call(
        EE_WON_HOST_HOSTOP_SET,
        [Obj(80), Obj::none(), Obj::none()],
        [crate::httpget::EE_WON_HTTPGET_LOCAL_PATH as i64, 0],
        b"here.bin",
        [0; 6],
    );
    let local = b.call(EE_WON_HOST_HOSTOP_BYTES, [80, 0], [crate::httpget::EE_WON_HTTPGET_LOCAL_PATH as i64, 0]);
    assert_eq!(local.bytes.as_deref(), Some(&b"here.bin"[..]));
    h.call(
        EE_WON_HOST_HOSTOP_SET,
        [Obj(80), Obj::none(), Obj::none()],
        [crate::httpget::EE_WON_HTTPGET_LOCAL_PATH as i64, 0],
        b"",
        [0; 6],
    );
    // no local file: no modify time
    assert_eq!(b.call(EE_WON_HOST_HTTP_MODIFIED, [80, 0], [0, 0]).num, 0);
    let six = |p: u16| [&p.to_be_bytes()[..], &[127, 0, 0, 1]].concat();
    let addrs = b.call(EE_WON_HOST_CTX_ADDRS, [81, 0], [0, 0]).bytes.unwrap_or_default();
    assert_eq!(addrs, [six(15101), six(15102), six(15103)].concat());
    let mut failed = [0u8; 6];
    failed.copy_from_slice(&six(15101));
    h.call(EE_WON_HOST_CTX_FAILED, [Obj(81), Obj::none(), Obj::none()], [0, 0], &[], failed);
    let addrs = b.call(EE_WON_HOST_CTX_ADDRS, [81, 0], [0, 0]).bytes.unwrap_or_default();
    assert_eq!(addrs, [six(15102), six(15103), six(15101)].concat(), "the failed server moved to the end");
    drop(b);
    unsafe { crate::hostop::ee_won_hostop_free(other) };
}

#[test]
fn not_bound_natively_nothing_is_answered() {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let op = EeWonHostOp::boxed(Probe);
    let h = unsafe { &*op };
    // the base still holds the setup (the accessors' values), but the calls go out
    assert_eq!(h.base_get(EE_WON_HOSTBASE_LFS), 2);
    unsafe { crate::hostop::ee_won_hostop_bind(op, Some(side), std::ptr::null_mut()) };
    take();
    h.call(EE_WON_HOST_STATUS, [Obj::none(); 3], [0, 0], &[], [0; 6]);
    assert_eq!(take(), [format!("sent {EE_WON_HOST_STATUS}")]);
    unsafe { crate::hostop::ee_won_hostop_free(op) };
}
