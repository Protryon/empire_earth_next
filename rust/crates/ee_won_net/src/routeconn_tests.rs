//! `EeWonRouteConn` over scripted C++ ops: the completions an op gets, the list's order, the dispatch (taken,
//! not wanted, waiting for more, a read past the end), unsolicited and derived messages, the close and kill hooks
//! and the references the object holds (every one released at the end).

use super::*;
use crate::routemsg::{EE_WON_RT_DONT_WANT, EE_WON_RT_NEED_MORE};
use std::sync::Mutex;

/// a scripted op or completion
struct Obj {
    name: String,
    refs: i32,
    completion: usize,
    op_type: c_long,
    derived: bool,
    /// what HandleReply answers, in turn (None: a ReadBufferException)
    replies: Vec<Option<i32>>,
}

static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
static LOCK: Mutex<()> = Mutex::new(());
/// the notification ops the hook made, by name
static MADE: Mutex<Vec<usize>> = Mutex::new(Vec::new());
/// whether derived messages make an op
static DERIVED_OPS: Mutex<bool> = Mutex::new(true);

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}
fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}
fn obj<'a>(p: usize) -> &'a mut Obj {
    // SAFETY: the test's leaked objects
    unsafe { &mut *(p as *mut Obj) }
}
fn new_obj(name: &str, op_type: c_long, replies: &[Option<i32>]) -> usize {
    Box::into_raw(Box::new(Obj {
        name: name.into(),
        refs: 0,
        completion: 0,
        op_type,
        derived: false,
        replies: replies.to_vec(),
    })) as usize
}
fn name(p: usize) -> String {
    if p == 0 {
        "null".into()
    } else {
        obj(p).name.clone()
    }
}

unsafe extern "C" fn add_ref(p: *mut c_void) {
    obj(p as usize).refs += 1;
}
unsafe extern "C" fn release(p: *mut c_void) {
    obj(p as usize).refs -= 1;
}
unsafe extern "C" fn op_kill(p: *mut c_void) {
    log(format!("kill {}", name(p as usize)));
}
unsafe extern "C" fn op_completion(p: *mut c_void) -> *mut c_void {
    obj(p as usize).completion as *mut c_void
}
unsafe extern "C" fn op_set_completion(p: *mut c_void, c: *mut c_void) {
    obj(p as usize).completion = c as usize;
}
unsafe extern "C" fn op_type(p: *mut c_void) -> c_long {
    obj(p as usize).op_type
}
unsafe extern "C" fn op_derived(p: *mut c_void) -> bool {
    obj(p as usize).derived
}
unsafe extern "C" fn handle_reply(
    p: *mut c_void,
    t: u8,
    _d: *const u8,
    len: usize,
    pos: usize,
    threw: *mut bool,
) -> c_long {
    let o = obj(p as usize);
    let r = if o.replies.is_empty() { Some(EE_WON_RT_DONT_WANT) } else { o.replies.remove(0) };
    log(format!("{} reply {t} {len} at {pos} -> {r:?}", o.name));
    match r {
        Some(s) => s,
        None => {
            // SAFETY: the object's flag
            unsafe { *threw = true };
            0
        }
    }
}
unsafe extern "C" fn op_end(p: *mut c_void, unsolicited: bool, status: c_long) {
    let how = if unsolicited { "force finish" } else { "finish" };
    log(format!("{how} {} {status} via {}", name(p as usize), name(obj(p as usize).completion)));
}
unsafe extern "C" fn op_complete(p: *mut c_void) {
    log(format!("complete {}", name(p as usize)));
}
unsafe extern "C" fn complete_hook(_c: *mut c_void, p: *mut c_void) {
    log(format!("hook {}", name(p as usize)));
}
unsafe extern "C" fn new_unsolicited(_c: *mut c_void, kind: i32, t: u8) -> *mut c_void {
    if kind == -1 && !*DERIVED_OPS.lock().unwrap() {
        return std::ptr::null_mut();
    }
    let p = new_obj(&format!("notice {kind} for {t}"), if kind == -1 { 60 } else { 10 }, &[Some(0)]);
    obj(p).derived = kind == -1;
    obj(p).refs = 1;
    MADE.lock().unwrap().push(p);
    p as *mut c_void
}
unsafe extern "C" fn server_close_hook(_c: *mut c_void) {
    log("server close hook".into());
}
unsafe extern "C" fn server_kill_hook(_c: *mut c_void) {
    log("server kill hook".into());
}

static HOOKS: EeWonRouteConnHooks = EeWonRouteConnHooks {
    add_ref: Some(add_ref),
    release: Some(release),
    op_kill: Some(op_kill),
    op_completion: Some(op_completion),
    op_set_completion: Some(op_set_completion),
    op_type: Some(op_type),
    op_derived: Some(op_derived),
    handle_reply: Some(handle_reply),
    op_end: Some(op_end),
    op_complete: Some(op_complete),
    complete_hook: Some(complete_hook),
    new_unsolicited: Some(new_unsolicited),
    server_close_hook: Some(server_close_hook),
    server_kill_hook: Some(server_kill_hook),
};

fn with_conn(f: impl FnOnce(&EeWonRouteConn)) {
    let _g = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    take();
    MADE.lock().unwrap().clear();
    *DERIVED_OPS.lock().unwrap() = true;
    // SAFETY: a fresh platform mutex for the test (recursive, as the socket's)
    let lock = unsafe { plat::PlatMutexCreate() };
    let c = EeWonRouteConn::new(std::ptr::null_mut(), &HOOKS, lock);
    f(&c);
    drop(c);
    // SAFETY: created above, no longer used
    unsafe { plat::PlatMutexDestroy(lock) };
}

#[test]
fn ops_get_their_own_their_types_or_the_default_completion() {
    with_conn(|c| {
        let typed = new_obj("typed", 0, &[]);
        let default = new_obj("default", 0, &[]);
        let own = new_obj("own", 0, &[]);
        c.set_completion(35, typed as *mut c_void);
        c.set_completion(-1, default as *mut c_void);
        c.set_completion(61, own as *mut c_void);
        assert_eq!((obj(typed).refs, obj(default).refs, obj(own).refs), (1, 1, 0));
        let a = new_obj("a", 35, &[]);
        let b = new_obj("b", 20, &[]);
        let d = new_obj("d", 99, &[]);
        let e = new_obj("e", 35, &[]);
        obj(e).completion = own;
        for o in [a, b, d, e] {
            c.add_op(o as *mut c_void);
        }
        assert_eq!([a, b, d, e].map(|o| name(obj(o).completion)), ["typed", "default", "default", "own"]);
        assert_eq!(c.st().ops, [a, b, d, e]);
        // the kill hook drops every completion, the close hook kills the ops in order and empties the list
        c.kill_hook();
        assert_eq!((obj(typed).refs, obj(default).refs), (0, 0));
        assert_eq!(take(), ["server kill hook"]);
        c.close_hook();
        assert_eq!(take(), ["kill a", "kill b", "kill d", "kill e", "server close hook"]);
        assert_eq!([a, b, d, e].map(|o| obj(o).refs), [0, 0, 0, 0]);
    });
}

#[test]
fn a_reply_goes_to_the_first_op_that_wants_it() {
    with_conn(|c| {
        let default = new_obj("default", 0, &[]);
        c.set_completion(-1, default as *mut c_void);
        let a = new_obj("a", 20, &[Some(EE_WON_RT_DONT_WANT), Some(EE_WON_RT_NEED_MORE)]);
        let b = new_obj("b", 35, &[Some(-1)]);
        c.add_op(a as *mut c_void);
        c.add_op(b as *mut c_void);
        // an unknown type with no notification: a does not want it, b finishes with -1
        c.msg_callback(&[3, 7, 200, 9]);
        assert_eq!(
            take(),
            [
                "a reply 200 4 at 3 -> Some(100104)",
                "b reply 200 4 at 3 -> Some(-1)",
                "finish b -1 via default",
                "hook b",
                "complete b",
            ]
        );
        assert_eq!(c.st().ops, [a]);
        assert_eq!(obj(b).refs, 0);
        // a waits for more: it stays
        c.msg_callback(&[3, 7, 201]);
        assert_eq!(take(), ["a reply 201 3 at 3 -> Some(100105)"]);
        assert_eq!(c.st().ops, [a]);
        // a short header: nothing
        c.msg_callback(&[3, 7]);
        assert!(take().is_empty());
        // a derived message is not offered to a solicited op (no derived op made here)
        *DERIVED_OPS.lock().unwrap() = false;
        c.msg_callback(&[3, 8, 5]);
        assert!(take().is_empty());
        c.close_hook();
        take();
        assert_eq!(obj(a).refs, 0);
    });
}

#[test]
fn a_notification_goes_first_and_is_forced_to_finish() {
    with_conn(|c| {
        let typed = new_obj("left", 0, &[]);
        c.set_completion(10, typed as *mut c_void);
        let a = new_obj("a", 35, &[]);
        c.add_op(a as *mut c_void);
        c.msg_callback(&[3, 7, 10, 5, 0]);
        let n = MADE.lock().unwrap()[0];
        assert_eq!(name(n), "notice 24 for 10");
        assert_eq!(
            take(),
            [
                "notice 24 for 10 reply 10 5 at 3 -> Some(0)",
                "force finish notice 24 for 10 0 via left",
                "hook notice 24 for 10",
                "complete notice 24 for 10"
            ]
        );
        assert_eq!(obj(n).refs, 0);
        assert_eq!(c.st().ops, [a]);
        // a derived message: the derived op, which only derived ops see
        c.msg_callback(&[3, 8, 9, 1]);
        let d = MADE.lock().unwrap()[1];
        assert_eq!(
            take(),
            [
                "notice -1 for 9 reply 9 4 at 3 -> Some(0)",
                "force finish notice -1 for 9 0 via null",
                "hook notice -1 for 9",
                "complete notice -1 for 9"
            ]
        );
        assert_eq!(obj(d).refs, 0);
        c.close_hook();
        take();
    });
}

#[test]
fn a_read_past_the_end_finishes_the_op_with_the_last_status() {
    with_conn(|c| {
        let a = new_obj("a", 20, &[Some(EE_WON_RT_DONT_WANT)]);
        let b = new_obj("b", 35, &[None]);
        c.add_op(a as *mut c_void);
        c.add_op(b as *mut c_void);
        c.msg_callback(&[3, 7, 66]);
        assert_eq!(
            take(),
            [
                "a reply 66 3 at 3 -> Some(100104)",
                "b reply 66 3 at 3 -> None",
                "finish b 100104 via null",
                "complete b"
            ]
        );
        // the first op's read fails: no status yet, an unpack failure
        let e = new_obj("e", 35, &[None]);
        c.add_op(e as *mut c_void);
        c.close_hook();
        take();
        c.add_op(e as *mut c_void);
        c.msg_callback(&[3, 7, 66]);
        assert_eq!(
            take(),
            [
                "e reply 66 3 at 3 -> None",
                format!("finish e {EE_WON_RT_UNPACK_FAILURE} via null").as_str(),
                "complete e"
            ]
        );
        assert_eq!([a, b, e].map(|o| obj(o).refs), [0, 0, 0]);
    });
}

#[test]
fn the_object_releases_what_it_holds() {
    let mut held = Vec::new();
    with_conn(|c| {
        let d = new_obj("default", 0, &[]);
        let t = new_obj("typed", 0, &[]);
        let a = new_obj("a", 3, &[]);
        c.set_completion(-1, d as *mut c_void);
        c.set_completion(60, t as *mut c_void);
        c.add_op(a as *mut c_void);
        held = vec![d, t, a];
    });
    for h in held {
        assert_eq!(obj(h).refs, 0, "{}", name(h));
    }
}
