//! The native adapters over a stand-in core table: an op's calls answered on its Rust object, the defaults for an
//! object without one, the fast completions' registry (an original found while its completion lives, gone with it)
//! and the references the completion bodies hold for a call and for their life.

use super::*;
use crate::opobj::{ee_won_completion_free, ee_won_completion_run, EeWonAsyncOpHooks};
use std::sync::Mutex;

pub(crate) static LOCK: Mutex<()> = Mutex::new(());
static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
/// (C++ object, Rust object, kind)
static OBJECTS: Mutex<Vec<(usize, usize, i32)>> = Mutex::new(Vec::new());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}
fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}

unsafe extern "C" fn r_add(o: *mut c_void) {
    log(format!("+{}", o as usize));
}
unsafe extern "C" fn r_release(o: *mut c_void) {
    log(format!("-{}", o as usize));
}
unsafe extern "C" fn c_call(c: *mut c_void, o: *mut c_void) {
    log(format!("call {} {}", c as usize, o as usize));
}
unsafe extern "C" fn object_of_stub(o: *mut c_void, kind: EeWonCoreObject) -> *mut c_void {
    OBJECTS
        .lock()
        .unwrap()
        .iter()
        .find(|e| e.0 == o as usize && e.2 == kind as i32)
        .map_or(null_mut(), |e| e.1 as *mut c_void)
}
/// the completion's C++ object is its body
unsafe extern "C" fn new_completion_stub(body: *mut EeWonCompletion) -> *mut c_void {
    body.cast()
}

static CORE_STUB: EeWonCoreHooks = EeWonCoreHooks {
    ref_add: Some(r_add),
    ref_release: Some(r_release),
    completion_call: Some(c_call),
    object_of: Some(object_of_stub),
    socketop_duplicate: None,
    new_op: None,
    new_completion: Some(new_completion_stub),
    new_socket_op: None,
    new_queue_socket: None,
    new_udp_recv_op: None,
    udp_check: None,
    udp_response_check: std::ptr::null_mut(),
    socket_bind_local: None,
    bytes_of: None,
    socketop_continue: None,
    socketop_start: None,
    socket_thread_add: None,
    bytes_adopt: None,
    transform: None,
    file_open: None,
    file_write: None,
    file_close: None,
    file_set_time: None,
    progress: None,
    accept: None,
    socket_hook: None,
    transform_call: None,
    transform_free: None,
    new_route_op: None,
    route_virtual: None,
    new_http_conn: None,
};

static OP_HOOKS: EeWonAsyncOpHooks = EeWonAsyncOpHooks {
    run_hook: None,
    cleanup_hook: None,
    core_queue: None,
    timer_add: None,
    timer_remove: None,
    completion_add_ref: Some(r_add),
    completion_release: Some(r_release),
    completion_call: None,
    op_add_ref: None,
    op_release: None,
    op_kill: None,
    ref_add: None,
    ref_release: None,
    new_completion: None,
    callback_hook: None,
};

#[test]
fn op_calls_on_the_rust_object() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    bind(&CORE_STUB);
    take();
    let cpp = 0x1000usize as *mut c_void;
    let o = Box::new(EeWonAsyncOp::new(cpp, &OP_HOOKS));
    OBJECTS.lock().unwrap().push((cpp as usize, &*o as *const EeWonAsyncOp as usize, EeWonCoreObject::Op as i32));
    // SAFETY: the stand-in op and its Rust object
    unsafe {
        assert_eq!(n_op_timeout(cpp), u32::MAX);
        n_op_set_timeout(cpp, 250);
        assert_eq!(n_op_timeout(cpp), 250);
        assert_eq!(n_op_timeout_long(cpp), 250);
        assert!(!n_op_killed(cpp));
        assert_eq!(n_op_completion(cpp), null_mut());
        n_op_set_completion(cpp, 0x2000usize as *mut c_void);
        assert_eq!(n_op_completion(cpp), 0x2000usize as *mut c_void);
        assert_eq!(take(), vec!["+8192"]);
        n_op_set_completion(cpp, null_mut());
        assert_eq!(take(), vec!["-8192"]);
        n_op_kill(cpp);
        assert!(n_op_killed(cpp));
        // no Rust object: the defaults, nothing called
        let none = 0x3000usize as *mut c_void;
        assert_eq!(n_op_status(none), WS_NONE as c_long);
        assert!(!n_op_killed(none));
        assert_eq!(n_op_completion(none), null_mut());
        n_op_kill(none);
        n_op_run_async(none, -1);
        assert!(!n_socket_valid(none));
        assert!(!n_socket_datagram(none));
        assert_eq!(n_fast_original(none), null_mut());
        // references go to the table, never for null
        n_ref_add(cpp);
        n_ref_release(null_mut());
        n_completion_call(none, cpp);
    }
    assert_eq!(take(), vec!["+4096", "call 12288 4096"]);
    OBJECTS.lock().unwrap().retain(|e| e.0 != cpp as usize);
}

#[test]
fn fast_completions_and_their_references() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    bind(&CORE_STUB);
    take();
    let socket = 0x10usize as *mut c_void;
    let original = 0x20usize as *mut c_void;
    let op = 0x30usize as *mut c_void;
    // SAFETY: stand-in objects; the completions are their bodies
    unsafe {
        let fast = n_new_fast_completion(socket, original);
        assert_eq!(take(), vec!["+16", "+32"], "the socket and the original held");
        assert_eq!(n_fast_original(fast), original);
        let plain = n_new_op_completion(socket);
        assert_eq!(n_fast_original(plain), null_mut());
        assert_eq!(take(), vec!["+16"]);
        // a call holds its parameters (no queue object: nothing else runs)
        ee_won_completion_run(fast as *const EeWonCompletion, op);
        assert_eq!(take(), vec!["+16", "+48", "-48", "-16"]);
        ee_won_completion_free(fast as *mut EeWonCompletion);
        assert_eq!(n_fast_original(fast), null_mut(), "gone with its completion");
        let mut released = take();
        released.sort();
        assert_eq!(released, vec!["-16", "-32"]);
        ee_won_completion_free(plain as *mut EeWonCompletion);
        assert_eq!(take(), vec!["-16"]);
        // without an original: a plain completion
        let none = n_new_fast_completion(socket, null_mut());
        assert_eq!(n_fast_original(none), null_mut());
        ee_won_completion_free(none as *mut EeWonCompletion);
        assert_eq!(take(), vec!["+16", "-16"]);
    }
}

#[test]
fn udp_completions_hold_their_targets() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    bind(&CORE_STUB);
    take();
    let manager = 0x40usize as *mut c_void;
    let request = 0x50usize as *mut c_void;
    let op = 0x60usize as *mut c_void;
    let m = manager_completion(manager);
    let w = wait_completion(request);
    assert_eq!(take(), vec!["+64", "+80"]);
    // SAFETY: the bodies made above
    unsafe {
        // an op without a Rust object has not succeeded: the manager's body stops after its holds
        ee_won_completion_run(m as *const EeWonCompletion, op);
        assert_eq!(take(), vec!["+64", "+96", "-96", "-64"]);
        ee_won_completion_run(w as *const EeWonCompletion, op);
        assert_eq!(take(), vec!["+80", "+96", "-96", "-80"]);
        ee_won_completion_free(m as *mut EeWonCompletion);
        ee_won_completion_free(w as *mut EeWonCompletion);
    }
    assert_eq!(take(), vec!["-64", "-80"]);
}
