//! `HTTPSession`'s and `HTTPConnection`'s C++ calls over the library's core table (net round 26). The session's map
//! and the connection's state machine were Rust objects since round 20 ([`EeWonHttpSession`], [`EeWonHttpConn`])
//! with their hooks answered in HTTPEngine.cpp (`HTTPEngineHooks`). They are Rust's here: the locks on the C++
//! object's platform mutex, an op's calls on its [`EeWonAsyncOp`](crate::opobj::EeWonAsyncOp) and its
//! [`EeWonHostOp`](crate::hostop::EeWonHostOp) (the HTTP get op's members), a socket's on its
//! [`EeWonAsyncSocket`](crate::sockobj::EeWonAsyncSocket), the connection's completion an
//! [`EeWonCompletion`](crate::opobj::EeWonCompletion) holding the connection (the original's `ParamCompletion` over
//! `StaticOpCallback`), a connection's calls on its Rust object. The C++ is asked only for a new `HTTPConnection`.

use std::ffi::{c_int, c_long, c_void};
use std::ptr::null_mut;

use ee_plat_sys::thread::imp as plat;

use crate::cppcore::{
    completion, core, hold, n_completion_call, n_op_completion, n_op_force_finish, n_op_kill, n_op_set_completion,
    n_op_status, n_ref_add, n_ref_release, object_of, op, ref_add, socket, EeWonCoreObject,
};
use crate::ffi::EeWonByteSink;
use crate::hostop::{ee_won_hostop_bytes, ee_won_hostop_num, ee_won_hostop_set, EeWonHostOp};
use crate::httpengine::*;
use crate::httpget::{
    EE_WON_HTTPGET_CLOSE, EE_WON_HTTPGET_HOST, EE_WON_HTTPGET_MODE, EE_WON_HTTPGET_RANGE_RESUME, EE_WON_HTTPGET_SOCKET,
};

fn lock_of(o: *mut c_void) -> *mut plat::PlatMutex {
    object_of(o, EeWonCoreObject::Lock).cast()
}
unsafe extern "C" fn n_lock(o: *mut c_void) {
    let m = lock_of(o);
    if !m.is_null() {
        // SAFETY: the object's live mutex
        unsafe { plat::PlatMutexLock(m) };
    }
}
unsafe extern "C" fn n_unlock(o: *mut c_void) {
    let m = lock_of(o);
    if !m.is_null() {
        // SAFETY: locked by n_lock
        unsafe { plat::PlatMutexUnlock(m) };
    }
}
fn host_op(o: *mut c_void) -> *const EeWonHostOp {
    object_of(o, EeWonCoreObject::HostOp).cast()
}
fn set_num(o: *mut c_void, which: i32, num: i64, obj: *mut c_void) {
    let h = host_op(o);
    if !h.is_null() {
        // SAFETY: the op's live Rust op
        unsafe { ee_won_hostop_set(h, which, num, std::ptr::null(), 0, std::ptr::null(), obj) };
    }
}
fn conn_of(c: *mut c_void) -> *const EeWonHttpConn {
    object_of(c, EeWonCoreObject::HttpConn).cast()
}

/// `GetCompletion()`: a reference, or null
unsafe extern "C" fn n_completion_ref(o: *mut c_void) -> *mut c_void {
    // SAFETY: the op
    let c = unsafe { n_op_completion(o) };
    ref_add(c);
    c
}
/// `SetCompletion(new ParamCompletion(StaticOpCallback, connection))`
unsafe extern "C" fn n_set_conn_completion(o: *mut c_void, conn: *mut c_void) {
    let held = hold(conn);
    let c = completion(move |x| {
        let _param = hold(held.get());
        let _op = hold(x);
        let impl_ = conn_of(held.get());
        if !impl_.is_null() {
            // SAFETY: the connection's live Rust object (the connection is held)
            unsafe { ee_won_http_conn_op_callback(impl_, x) };
        }
    });
    // SAFETY: the op takes the completion's reference
    unsafe { n_op_set_completion(o, c) };
}
/// `SetCloseConnection(false)`, `SetDoRangeResume(false)`, `SetSocket(socket)`
unsafe extern "C" fn n_prepare(o: *mut c_void, s: *mut c_void) {
    set_num(o, EE_WON_HTTPGET_CLOSE, 0, null_mut());
    set_num(o, EE_WON_HTTPGET_RANGE_RESUME, 0, null_mut());
    set_num(o, EE_WON_HTTPGET_SOCKET, 0, s);
}
/// `RunAsync(GetTimeout())`
unsafe extern "C" fn n_run_async(o: *mut c_void) {
    if let Some(x) = op(o) {
        x.run(1, x.timeout());
    }
}
/// the op's socket when connected: a reference, or null
unsafe extern "C" fn n_op_socket(o: *mut c_void) -> *mut c_void {
    let h = host_op(o);
    if h.is_null() {
        return null_mut();
    }
    // SAFETY: the op's live Rust op
    let s = unsafe { ee_won_hostop_num(h, EE_WON_HTTPGET_SOCKET, 0) } as isize as *mut c_void;
    if s.is_null() || !socket(s).is_some_and(|x| x.is_connected()) {
        return null_mut();
    }
    ref_add(s);
    s
}
/// 0 send only, 1 send and receive, 2 receive only
unsafe extern "C" fn n_set_mode(o: *mut c_void, m: c_int) {
    set_num(
        o,
        EE_WON_HTTPGET_MODE,
        if m == 2 {
            2
        } else if m == 0 {
            0
        } else {
            1
        },
        null_mut(),
    );
}
unsafe extern "C" fn n_socket_kill(s: *mut c_void) {
    if let Some(x) = socket(s) {
        x.kill();
    }
}
/// `time(NULL)`
unsafe extern "C" fn n_now(_: *mut c_void) -> i64 {
    std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_secs() as i64)
}
/// the op's `mHost` (empty for none) into the sink
unsafe extern "C" fn n_host(o: *mut c_void, sink: EeWonByteSink, context: *mut c_void) {
    let h = host_op(o);
    // SAFETY: the op's live Rust op; the sink's contract
    if h.is_null() || !unsafe { ee_won_hostop_bytes(h, EE_WON_HTTPGET_HOST, 0, sink, context) } {
        // SAFETY: as above
        unsafe { sink(context, [].as_ptr(), 0) };
    }
}
/// `new HTTPConnection(NULL)`: a reference
unsafe extern "C" fn n_new_conn(_: *mut c_void) -> *mut c_void {
    // SAFETY: the table's contract
    let c = core().and_then(|x| x.new_http_conn).map_or(null_mut(), |h| unsafe { h() });
    ref_add(c);
    c
}
unsafe extern "C" fn n_conn_queue(c: *mut c_void, o: *mut c_void) {
    let x = conn_of(c);
    if !x.is_null() {
        // SAFETY: the connection's live Rust object
        unsafe { ee_won_http_conn_queue_op(x, o) };
    }
}
unsafe extern "C" fn n_conn_kill(c: *mut c_void) {
    let x = conn_of(c);
    if !x.is_null() {
        // SAFETY: as above
        unsafe { ee_won_http_conn_kill(x) };
    }
}
unsafe extern "C" fn n_conn_num_ops(c: *mut c_void) -> c_int {
    let x = conn_of(c);
    // SAFETY: as above
    if x.is_null() {
        0
    } else {
        unsafe { ee_won_http_conn_num_ops(x) }
    }
}
unsafe extern "C" fn n_status(o: *mut c_void) -> c_long {
    // SAFETY: the op
    unsafe { n_op_status(o) }
}

/// The connection's calls for the C++ `HTTPConnection*` `conn`, over the core table.
pub(crate) fn conn_hooks(conn: *mut c_void) -> EeWonHttpConnHooks {
    EeWonHttpConnHooks {
        context: conn,
        lock: n_lock,
        unlock: n_unlock,
        add_ref: n_ref_add,
        release: n_ref_release,
        op_status: n_status,
        op_kill: n_op_kill,
        op_force_finish: n_op_force_finish,
        op_completion: n_completion_ref,
        op_set_completion: n_op_set_completion,
        op_set_conn_completion: n_set_conn_completion,
        op_prepare: n_prepare,
        op_run_async: n_run_async,
        op_socket: n_op_socket,
        op_set_mode: n_set_mode,
        complete: n_completion_call,
        socket_kill: n_socket_kill,
        now: n_now,
    }
}

/// The session's calls for the C++ `HTTPSession*` `session`, over the core table.
pub(crate) fn session_hooks(session: *mut c_void) -> EeWonHttpSessionHooks {
    EeWonHttpSessionHooks {
        context: session,
        lock: n_lock,
        unlock: n_unlock,
        add_ref: n_ref_add,
        release: n_ref_release,
        op_host: n_host,
        new_conn: n_new_conn,
        conn_queue_op: n_conn_queue,
        conn_kill: n_conn_kill,
        conn_num_ops: n_conn_num_ops,
    }
}

/// A new connection object for the C++ `HTTPConnection*` `conn`, its calls over the core table.
///
/// # Safety
/// `core` is the library's static table; `conn` outlives the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_new_core(
    conn: *mut c_void,
    core: *const crate::cppcore::EeWonCoreHooks,
) -> *mut EeWonHttpConn {
    crate::cppcore::bind(core);
    let h = conn_hooks(conn);
    // SAFETY: a table on the stack, copied by the constructor
    unsafe { ee_won_http_conn_new(&h) }
}

/// A new session object for the C++ `HTTPSession*` `session`, its calls over the core table.
///
/// # Safety
/// As `ee_won_http_conn_new_core`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_session_new_core(
    session: *mut c_void,
    core: *const crate::cppcore::EeWonCoreHooks,
) -> *mut EeWonHttpSession {
    crate::cppcore::bind(core);
    let h = session_hooks(session);
    // SAFETY: as above
    unsafe { ee_won_http_session_new(&h) }
}

#[cfg(test)]
#[path = "httpcpp_tests.rs"]
mod tests;
