//! `AuthContext`'s C++ calls over the library's core table (net round 26). The auth context's certificate flow was a
//! Rust object since round 21 ([`EeWonAuthCtx`]) with its hooks answered in AuthContext.cpp (`AuthContextFlow`): an
//! op's references, kill, status, runs and finish, the refresh timer and the certificate op the flow makes with their
//! completions, the certificate op's peer data. They are Rust's here ([`native_hooks`]): an op's calls on its
//! [`EeWonAsyncOp`](crate::opobj::EeWonAsyncOp), the completions [`EeWonCompletion`](crate::opobj::EeWonCompletion)s
//! holding the context for their life and the op and context for a call (the original's `ParamCompletion` over
//! `StaticAutoRefreshCallback` / `StaticGetCertCallback`), the peer data on the op's
//! [`EeWonHostOp`](crate::hostop::EeWonHostOp). The C++ is asked only for a new `AsyncOp` (the core table) and a new
//! `GetCertOp` (the maker AuthContext.cpp hands over: the op base's unit is not linked everywhere the core is).

use std::ffi::{c_long, c_ulong, c_void};
use std::ptr::null_mut;
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::authflow::EeWonAuthFlowHooks;
use crate::authobj::{ee_won_authctx_auto_refresh_callback, ee_won_authctx_get_cert_callback, EeWonAuthCtx};
use crate::cppcore::{
    completion, core, hold, n_op_finish, n_op_kill, n_op_killed, n_op_run_async, n_op_set_completion, n_op_status,
    n_ref_add, n_ref_release, object_of, op, EeWonCoreObject,
};
use crate::hostop::{ee_won_hostop_num, EeWonHostOp};
use crate::peerobj::EeWonAuthObj;

/// The auth context's Rust object of the C++ `AuthContext*` (null for none).
fn auth_ctx(ctx: *mut c_void) -> *const EeWonAuthCtx {
    object_of(ctx, EeWonCoreObject::AuthCtx).cast()
}

/// A completion holding the context for its life; its call holds the op and the context (the by-value parameters)
/// and runs `call` on the context's Rust object.
fn ctx_completion(ctx: *mut c_void, call: unsafe extern "C" fn(*const EeWonAuthCtx, *mut c_void)) -> *mut c_void {
    let held = hold(ctx);
    completion(move |o| {
        let _param = hold(held.get());
        let _op = hold(o);
        let c = auth_ctx(held.get());
        if !c.is_null() {
            // SAFETY: the context's live Rust object (the context is held)
            unsafe { call(c, o) };
        }
    })
}

#[allow(clippy::unnecessary_cast)] // `long` is 64-bit on LP64
unsafe extern "C" fn n_run(o: *mut c_void, mode: c_long, timeout: c_ulong) -> c_long {
    op(o).map_or(crate::status::WS_NONE, |x| x.run(mode as i32, timeout as u32)) as c_long
}
/// `new AsyncOp` (one reference) completing to `StaticAutoRefreshCallback` on the context
unsafe extern "C" fn n_new_timer(ctx: *mut c_void) -> *mut c_void {
    // SAFETY: the table's contract
    let t = core().and_then(|c| c.new_op).map_or(null_mut(), |h| unsafe { h() });
    // SAFETY: the new op: one reference for the caller, the completion's taken by the op
    unsafe {
        n_ref_add(t);
        n_op_set_completion(t, ctx_completion(ctx, ee_won_authctx_auto_refresh_callback));
    }
    t
}
/// `new GetCertOp(context)`, the C++ maker (one for the program)
pub type EeWonNewCertOp = unsafe extern "C" fn(context: *mut c_void) -> *mut c_void;
static NEW_CERT_OP: AtomicPtr<c_void> = AtomicPtr::new(null_mut());

fn new_cert_op(ctx: *mut c_void) -> *mut c_void {
    let f = NEW_CERT_OP.load(Ordering::Acquire);
    if f.is_null() {
        return null_mut();
    }
    // SAFETY: a maker stored by ee_won_authctx_new_core
    unsafe { std::mem::transmute::<*mut c_void, EeWonNewCertOp>(f)(ctx) }
}

/// `new GetCertOp(context)` (one reference), completing to `StaticGetCertCallback` when asked
unsafe extern "C" fn n_new_cert_op(ctx: *mut c_void, with_completion: bool) -> *mut c_void {
    let o = new_cert_op(ctx);
    // SAFETY: the new op
    unsafe {
        n_ref_add(o);
        if with_completion {
            n_op_set_completion(o, ctx_completion(ctx, ee_won_authctx_get_cert_callback));
        }
    }
    o
}
/// `GetCertOp::GetPeerData`: a new reference to its peer data, or null
unsafe extern "C" fn n_cert_peer_data(o: *mut c_void) -> *mut EeWonAuthObj {
    let h = object_of(o, EeWonCoreObject::HostOp) as *const EeWonHostOp;
    if h.is_null() {
        return null_mut();
    }
    // SAFETY: the op's live Rust op
    unsafe { ee_won_hostop_num(h, crate::certhost::EE_WON_GETCERT_PEER_DATA, 0) as isize as *mut EeWonAuthObj }
}

/// The auth flow's calls for the C++ `AuthContext*` `ctx`, over the core table.
pub(crate) fn native_hooks(ctx: *mut c_void) -> EeWonAuthFlowHooks {
    EeWonAuthFlowHooks {
        context: ctx,
        add_ref: n_ref_add,
        release: n_ref_release,
        op_kill: n_op_kill,
        op_killed: n_op_killed,
        op_status: n_op_status,
        op_run_async: n_op_run_async,
        op_run: n_run,
        op_finish: n_op_finish,
        new_timer: n_new_timer,
        new_cert_op: n_new_cert_op,
        cert_peer_data: n_cert_peer_data,
    }
}

/// A new auth context object for the C++ `AuthContext*` `ctx` with its login secret file (`len` bytes), its calls
/// over the core table and the certificate op's maker; one reference.
///
/// # Safety
/// `core` is the library's static table; `ctx` outlives the object's use of it; `secret_file` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_authctx_new_core(
    ctx: *mut c_void,
    core: *const crate::cppcore::EeWonCoreHooks,
    new_cert_op: Option<unsafe extern "C" fn(context: *mut c_void) -> *mut c_void>,
    secret_file: *const u8,
    len: usize,
) -> *const EeWonAuthCtx {
    crate::cppcore::bind(core);
    if let Some(f) = new_cert_op {
        NEW_CERT_OP.store(f as *mut c_void, Ordering::Release);
    }
    let hooks = native_hooks(ctx);
    // SAFETY: the caller's bytes
    unsafe { crate::authobj::ee_won_authctx_new(&hooks, secret_file, len) }
}

#[cfg(test)]
#[path = "authcpp_tests.rs"]
mod tests;
