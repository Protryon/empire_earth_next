//! The auth flow's native calls over a stand-in core table: the refresh timer and the certificate op made with one
//! reference for the caller and their completions holding the context, a completion's call holding the op and the
//! context around the flow's callback, the peer data of an op without a Rust op.

use super::*;
use crate::cppcore::tests::LOCK;
use crate::cppcore::{bind, EeWonCoreHooks};
use crate::opobj::{ee_won_completion_free, ee_won_completion_run, EeWonAsyncOp, EeWonAsyncOpHooks, EeWonCompletion};
use std::sync::Mutex;

static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());
static OPS: Mutex<Vec<(usize, usize)>> = Mutex::new(Vec::new());

fn log(s: String) {
    LOG.lock().unwrap().push(s);
}
fn take() -> Vec<String> {
    std::mem::take(&mut *LOG.lock().unwrap())
}
unsafe extern "C" fn r_add(o: *mut c_void) {
    log(format!("+{:x}", o as usize));
}
unsafe extern "C" fn r_release(o: *mut c_void) {
    log(format!("-{:x}", o as usize));
}
unsafe extern "C" fn object_of_stub(o: *mut c_void, kind: EeWonCoreObject) -> *mut c_void {
    match kind {
        EeWonCoreObject::Op => {
            OPS.lock().unwrap().iter().find(|e| e.0 == o as usize).map_or(null_mut(), |e| e.1 as *mut c_void)
        }
        _ => null_mut(),
    }
}
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
fn made(cpp: usize) -> *mut c_void {
    let o = Box::into_raw(Box::new(EeWonAsyncOp::new(cpp as *mut c_void, &OP_HOOKS)));
    OPS.lock().unwrap().push((cpp, o as usize));
    cpp as *mut c_void
}
unsafe extern "C" fn new_op_stub() -> *mut c_void {
    log("new op".into());
    made(0x300)
}
unsafe extern "C" fn new_cert_op_stub(ctx: *mut c_void) -> *mut c_void {
    log(format!("new cert op {:x}", ctx as usize));
    made(0x400)
}
/// the completion's C++ object is its body
unsafe extern "C" fn new_completion_stub(body: *mut EeWonCompletion) -> *mut c_void {
    body.cast()
}

static AUTH_STUB: EeWonCoreHooks = EeWonCoreHooks {
    ref_add: Some(r_add),
    ref_release: Some(r_release),
    completion_call: None,
    object_of: Some(object_of_stub),
    socketop_duplicate: None,
    new_op: Some(new_op_stub),
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
    progress: None,
    accept: None,
    socket_hook: None,
    new_route_op: None,
    route_virtual: None,
    new_http_conn: None,
};

#[test]
fn timers_and_cert_ops_with_their_completions() {
    let _l = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    bind(&AUTH_STUB);
    NEW_CERT_OP.store(new_cert_op_stub as *mut c_void, Ordering::Release);
    take();
    let ctx = 0x50usize as *mut c_void;
    let h = native_hooks(ctx);
    assert_eq!(h.context, ctx);
    // SAFETY: the stub table
    unsafe {
        let t = (h.new_timer)(ctx);
        assert_eq!(t as usize, 0x300);
        // the caller's reference; the completion holds the context for its life and the op takes its reference
        let c = op(t).unwrap().completion();
        assert_eq!(take(), ["new op", "+300", "+50", format!("+{:x}", c as usize).as_str()]);
        // its call: the context and the op held around the callback (no Rust object: the callback skipped)
        ee_won_completion_run(c.cast(), t);
        assert_eq!(take(), ["+50", "+300", "-300", "-50"]);
        let o = (h.new_cert_op)(ctx, false);
        assert_eq!(take(), ["new cert op 50", "+400"]);
        assert!(op(o).unwrap().completion().is_null());
        let o = (h.new_cert_op)(ctx, true);
        assert!(!op(o).unwrap().completion().is_null());
        take();
        // a completion's end drops the context's reference
        ee_won_completion_free(c.cast());
        assert_eq!(take(), ["-50"]);
        // an op without a Rust op has no peer data
        assert!((h.cert_peer_data)(o).is_null());
        assert_eq!((h.op_status)(null_mut()), crate::status::WS_NONE as c_long);
    }
}
