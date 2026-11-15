//! The op base's own calls answered in Rust (net round 24): the op itself, `ServerOp`'s members, the tracker, the
//! completions the op makes, references, and the Rust objects behind other C++ objects it names.
//!
//! ServerOp.cpp's `HostCall` answered these on the C++ op: `GetStatus`, `GetMode`, `TimeLeft`, `GetTimeout` and
//! `Finish` (the op's `EeWonAsyncOp` since round 13), `ServerOp`'s members (`mSocket`, `mAuthContext`, `mAuthType`,
//! `mLengthFieldSize`, `mClientKeySize`, `mUseClientKey`, the three time limits: moved here, [`ServerBase`]; the C++
//! class keeps only accessors over [`ee_won_hostop_base`] / [`ee_won_hostop_base_get`]), `Track`, `mTracker.KillAll()`,
//! `AsyncOpWithTracker::CleanupHook`, `mTracker.IsEmpty()` (the op's `EeWonOpTracker`, the tracked ops' completion
//! Rust's: [`EeWonAsyncOp::track`]), the timers' and repeating receives' completions (`HostCompletion`, deleted: an
//! [`EeWonCompletion`] calling the op back), a completion called, `AddRef` / `Release`, and on other objects' Rust
//! objects: another Rust op's numbers, bytes and setters, a peer authentication op's certificate status, the HTTP
//! get's modify time, a server context's addresses and failed server.
//!
//! The op learns its own C++ objects when it is bound natively ([`ee_won_hostop_bind_native`]); the drivers'
//! stand-ins (bound with `ee_won_hostop_bind`) still see every call. `won_opcore_test` compares these answers with
//! the original's over real sockets (golden `golden_opcore.txt`).

use std::ffi::c_void;
use std::sync::Mutex;

use crate::hostop::*;
use crate::opobj::{EeWonAsyncOp, EeWonCompletion, EeWonOpTracker, HeldRef};
use crate::qsock::EeWonQueueSocket;
use crate::servctx::{Address, ServerList};
use crate::status::*;

// `ServerOp`'s members by number (`ee_won_hostop_base`, `ee_won_hostop_base_get`):
/// `mSocket` (an object: a `BlockingSocket*`, a reference held)
pub const EE_WON_HOSTBASE_SOCKET: i32 = 0;
/// `mAuthContext` (an object: an `AuthContext*`, a reference held)
pub const EE_WON_HOSTBASE_AUTH_CONTEXT: i32 = 1;
/// `mAuthType`
pub const EE_WON_HOSTBASE_AUTH_TYPE: i32 = 2;
/// `mLengthFieldSize`
pub const EE_WON_HOSTBASE_LFS: i32 = 3;
/// `mClientKeySize`
pub const EE_WON_HOSTBASE_CLIENT_KEY_SIZE: i32 = 4;
/// `mUseClientKey`
pub const EE_WON_HOSTBASE_USE_CLIENT_KEY: i32 = 5;
/// `mMaxConnectTime`, `mMaxSendTime`, `mMaxRecvTime`
pub const EE_WON_HOSTBASE_MAX_CONNECT: i32 = 6;
pub const EE_WON_HOSTBASE_MAX_SEND: i32 = 7;
pub const EE_WON_HOSTBASE_MAX_RECV: i32 = 8;
/// `ConnectTime()`, `SendTime()`, `RecvTime()` (read only: the limits, capped by the time left when blocking)
pub const EE_WON_HOSTBASE_CONNECT_TIME: i32 = 9;
pub const EE_WON_HOSTBASE_SEND_TIME: i32 = 10;
pub const EE_WON_HOSTBASE_RECV_TIME: i32 = 11;

/// `ServerOp`'s members (0x78..0x94 in the original).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) struct ServerBase {
    pub(crate) socket: Obj,
    pub(crate) auth_context: Obj,
    pub(crate) auth_type: i32,
    pub(crate) lfs: u8,
    pub(crate) client_key_size: u8,
    pub(crate) use_client_key: bool,
    /// connect, send, receive
    pub(crate) times: [u32; 3],
}

impl ServerBase {
    /// `RustServerOp`'s constructor over `ServerOp()`: the op's length field and limits, no auth, an 8-byte client
    /// key not used, no socket.
    pub(crate) fn new(setup: (u8, [u32; 3])) -> ServerBase {
        ServerBase {
            socket: Obj::none(),
            auth_context: Obj::none(),
            auth_type: 0,
            lfs: setup.0,
            client_key_size: 8,
            use_client_key: false,
            times: setup.1,
        }
    }
}

/// The op's own C++ objects (a natively bound op's): the `AsyncOp*`, its `EeWonAsyncOp` and its tracker.
#[derive(Clone, Copy, Debug, Default)]
pub(crate) struct Own {
    pub(crate) op: usize,
    pub(crate) obj: usize,
    pub(crate) tracker: usize,
}

/// The state a natively bound op adds to its `Host`.
#[derive(Default)]
pub(crate) struct Native {
    pub(crate) own: Mutex<Own>,
    pub(crate) base: Mutex<Option<ServerBase>>,
}

fn num(n: i64) -> Option<Reply> {
    Some(Reply { obj: Obj::none(), num: n, addr: [0; 6], bytes: None })
}
fn obj(o: Obj) -> Option<Reply> {
    Some(Reply { obj: o, num: 0, addr: [0; 6], bytes: None })
}

/// `ServerOp::ConnectTime` / `SendTime` / `RecvTime`: the limit, capped by the time left when blocking.
fn step(op: &EeWonAsyncOp, max: u32) -> u32 {
    if op.run_mode() == MODE_ASYNC {
        max
    } else {
        crate::server::step_time(max, false, op.time_left())
    }
}

impl<P: Plan> Host<P> {
    pub(crate) fn own(&self) -> Own {
        *self.native.own.lock().unwrap_or_else(|e| e.into_inner())
    }

    fn own_op(&self) -> Option<&EeWonAsyncOp> {
        let o = self.own().obj;
        // SAFETY: the op's own object, alive while the op is (the C++ op owns it and this host)
        (o != 0).then(|| unsafe { &*(o as *const EeWonAsyncOp) })
    }

    /// `ServerOp`'s members (after the op released them: none, the constructor's values).
    pub(crate) fn base(&self) -> ServerBase {
        let b = *self.native.base.lock().unwrap_or_else(|e| e.into_inner());
        b.unwrap_or_else(|| ServerBase::new((4, [60000; 3])))
    }

    fn with_base<R>(&self, f: impl FnOnce(&mut ServerBase) -> R) -> Option<R> {
        self.native.base.lock().unwrap_or_else(|e| e.into_inner()).as_mut().map(f)
    }

    /// A member by number (`EE_WON_HOSTBASE_*`).
    pub(crate) fn base_get(&self, which: i32) -> i64 {
        let b = self.base();
        let op = self.own_op();
        match which {
            EE_WON_HOSTBASE_SOCKET => b.socket.0 as i64,
            EE_WON_HOSTBASE_AUTH_CONTEXT => b.auth_context.0 as i64,
            EE_WON_HOSTBASE_AUTH_TYPE => i64::from(b.auth_type),
            EE_WON_HOSTBASE_LFS => i64::from(b.lfs),
            EE_WON_HOSTBASE_CLIENT_KEY_SIZE => i64::from(b.client_key_size),
            EE_WON_HOSTBASE_USE_CLIENT_KEY => i64::from(b.use_client_key),
            EE_WON_HOSTBASE_MAX_CONNECT..=EE_WON_HOSTBASE_MAX_RECV => {
                i64::from(b.times[(which - EE_WON_HOSTBASE_MAX_CONNECT) as usize])
            }
            EE_WON_HOSTBASE_CONNECT_TIME..=EE_WON_HOSTBASE_RECV_TIME => {
                let max = b.times[(which - EE_WON_HOSTBASE_CONNECT_TIME) as usize];
                i64::from(op.map_or(max, |o| step(o, max)))
            }
            _ => 0,
        }
    }

    /// Sets a member by number: an object member takes a reference to the new object and drops the old one's (a
    /// `SmartPtr` assignment), the references through the op's hooks.
    pub(crate) fn base_set(&self, which: i32, n: i64, o: Obj) {
        match which {
            EE_WON_HOSTBASE_SOCKET | EE_WON_HOSTBASE_AUTH_CONTEXT => {
                if !o.is_none() {
                    self.add_ref(o);
                }
                // (a released op keeps nothing: the new object's reference goes back)
                let old = self
                    .with_base(|b| {
                        let slot = if which == EE_WON_HOSTBASE_SOCKET { &mut b.socket } else { &mut b.auth_context };
                        std::mem::replace(slot, o)
                    })
                    .unwrap_or(o);
                self.release(old);
            }
            EE_WON_HOSTBASE_AUTH_TYPE => {
                self.with_base(|b| b.auth_type = n as i32);
            }
            EE_WON_HOSTBASE_LFS => {
                self.with_base(|b| b.lfs = n as u8);
            }
            EE_WON_HOSTBASE_CLIENT_KEY_SIZE => {
                self.with_base(|b| b.client_key_size = n as u8);
            }
            EE_WON_HOSTBASE_USE_CLIENT_KEY => {
                self.with_base(|b| b.use_client_key = n != 0);
            }
            EE_WON_HOSTBASE_MAX_CONNECT..=EE_WON_HOSTBASE_MAX_RECV => {
                self.with_base(|b| b.times[(which - EE_WON_HOSTBASE_MAX_CONNECT) as usize] = n as u32);
            }
            _ => {}
        }
    }

    /// The object members' references, given back when the op goes (after the plan's objects, `mAuthContext` then
    /// `mSocket`: the members' destruction order).
    pub(crate) fn release_base(&self) {
        let b = { self.native.base.lock().unwrap_or_else(|e| e.into_inner()).take() };
        if let Some(b) = b {
            self.release(b.auth_context);
            self.release(b.socket);
        }
    }

    /// A completion calling this op back with `id` (the deleted `HostCompletion`: it holds a reference to the op).
    fn host_completion(&self, id: i32) -> *mut c_void {
        let (own, Some(me)) = (self.own(), self.own_op()) else {
            return std::ptr::null_mut();
        };
        let hooks = me.hooks();
        let held = HeldRef::new(own.op as *mut c_void, hooks.op_add_ref, hooks.op_release);
        let host = self as *const Host<P> as usize;
        EeWonCompletion::make(hooks, move |op| {
            let _ = &held;
            // SAFETY: `held` keeps the C++ op, which owns this host, alive while the completion lives
            P::callback(unsafe { &*(host as *const Host<P>) }, id, Obj::of(op));
        })
    }

    /// `AddRef` / `Release` of a C++ object (a `RefCount*`: asked after the Rust handles' answers), or `None`.
    pub(crate) fn answer_refs(&self, code: i32, o: [Obj; 3]) -> Option<Reply> {
        if code != EE_WON_HOST_ADD_REF && code != EE_WON_HOST_RELEASE {
            return None;
        }
        let me = self.own_op()?;
        let h = if code == EE_WON_HOST_ADD_REF { me.hooks().ref_add } else { me.hooks().ref_release };
        if let (Some(h), false) = (h, o[0].is_none()) {
            // SAFETY: the caller's live object (a release may delete it)
            unsafe { h(o[0].ptr()) };
        }
        num(0)
    }

    /// The call answered here, or `None` for the others (`send` makes a call to the C++ side).
    pub(crate) fn answer_own(&self, code: i32, o: [Obj; 3], n: [i64; 2], data: &[u8], addr: [u8; 6]) -> Option<Reply> {
        let send = |c: i32, x: [Obj; 3], m: [i64; 2]| self.send_raw(c, x, m, &[], [0; 6]);
        let core =
            |which: i64, x: Obj| send(EE_WON_HOST_SOCK_CORE, [x, Obj::none(), Obj::none()], [which, 0]).obj.ptr();
        let me = self.own_op()?;
        let tracker = self.own().tracker as *const EeWonOpTracker;
        let base = || self.base();
        match code {
            // the op itself
            EE_WON_HOST_STATUS => num(me.status()),
            EE_WON_HOST_MODE => num(i64::from(me.run_mode())),
            EE_WON_HOST_TIME_LEFT => num(i64::from(me.time_left())),
            EE_WON_HOST_TIMEOUT => num(i64::from(me.timeout())),
            EE_WON_HOST_FINISH => {
                me.finish(n[0] as Status);
                num(0)
            }
            // ServerOp's members
            EE_WON_HOST_LFS => num(i64::from(base().lfs)),
            EE_WON_HOST_CONNECT_TIME => num(self.base_get(EE_WON_HOSTBASE_CONNECT_TIME)),
            EE_WON_HOST_SEND_TIME => num(self.base_get(EE_WON_HOSTBASE_SEND_TIME)),
            EE_WON_HOST_RECV_TIME => num(self.base_get(EE_WON_HOSTBASE_RECV_TIME)),
            EE_WON_HOST_AUTH_TYPE => num(i64::from(base().auth_type)),
            EE_WON_HOST_CLIENT_KEY => {
                let b = base();
                num(i64::from(b.client_key_size) | if b.use_client_key { 0x100 } else { 0 })
            }
            EE_WON_HOST_OWN_SOCKET => obj(base().socket),
            // the op's own auth context's object (`mAuthContext`; none: NULL)
            EE_WON_HOST_AUTH_CORE if o[0].is_none() => {
                let ctx = base().auth_context;
                obj(if ctx.is_none() { Obj::none() } else { send(EE_WON_HOST_AUTH_CORE, [ctx, o[1], o[2]], n).obj })
            }
            // the tracker
            EE_WON_HOST_TRACK => {
                let child = core(EE_WON_HOST_CORE_OP, o[0]) as *const EeWonAsyncOp;
                if !child.is_null() && !tracker.is_null() {
                    // SAFETY: the op's tracker; the caller's live child and its object
                    unsafe { me.track(tracker, o[0].ptr(), &*child, n[0] as i32) };
                }
                num(0)
            }
            EE_WON_HOST_KILL_ALL | EE_WON_HOST_BASE_CLEANUP => {
                if !tracker.is_null() {
                    // SAFETY: the op's tracker, alive with the op
                    unsafe { (*tracker).kill_all() };
                }
                num(0)
            }
            // SAFETY: as above
            EE_WON_HOST_TRACKER_EMPTY => num(i64::from(tracker.is_null() || unsafe { (*tracker).is_empty() })),
            // the completions this op makes
            EE_WON_HOST_NEW_TIMER => {
                let t = send(EE_WON_HOST_PEER_NEW_LIGHT, [Obj::none(); 3], [0, 0]).obj;
                let t_obj = core(EE_WON_HOST_CORE_OP, t) as *const EeWonAsyncOp;
                if !t_obj.is_null() {
                    let c = self.host_completion(n[0] as i32);
                    // SAFETY: the new op's object; a new completion
                    unsafe { (*t_obj).set_completion(c) };
                }
                obj(t)
            }
            EE_WON_HOST_SET_REPEAT => {
                let q = core(EE_WON_HOST_CORE_QUEUE, o[0]) as *const EeWonQueueSocket;
                if !q.is_null() {
                    let c = self.host_completion(n[0] as i32);
                    // SAFETY: the C++ queue socket's own object (the caller holds the socket); `o[1]` a live op
                    unsafe {
                        (*q).set_repeat_completion(c);
                        (*q).set_repeat_op(o[1].ptr());
                    }
                }
                num(0)
            }
            EE_WON_HOST_COMPLETE => {
                if let Some(h) = me.hooks().completion_call {
                    // SAFETY: the caller's live completion; this op
                    unsafe { h(o[0].ptr(), me.cpp()) };
                }
                num(0)
            }
            // another Rust op
            EE_WON_HOST_HOSTOP_NUM
            | EE_WON_HOST_HOSTOP_BYTES
            | EE_WON_HOST_HOSTOP_SET
            | EE_WON_HOST_PEER_AUTH_CERT
            | EE_WON_HOST_HTTP_MODIFIED => {
                let h = core(EE_WON_HOST_CORE_HOSTOP, o[0]) as *const EeWonHostOp;
                if h.is_null() {
                    return num(0);
                }
                // SAFETY: the C++ op's own Rust op (the caller holds the op)
                let h = unsafe { &*h };
                match code {
                    EE_WON_HOST_HOSTOP_NUM => num(h.num(n[0] as i32, n[1])),
                    EE_WON_HOST_HOSTOP_BYTES => {
                        Some(Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: h.bytes(n[0] as i32, n[1]) })
                    }
                    EE_WON_HOST_HOSTOP_SET => {
                        h.set(n[0] as i32, &SetValue { num: n[1], data, addr: [0; 6], obj: o[1] });
                        num(0)
                    }
                    EE_WON_HOST_PEER_AUTH_CERT => {
                        num(i64::from(h.num(crate::peerop::EE_WON_PEEROP_GET_CERT_STATUS, 0) as i32))
                    }
                    _ => num(http_modified(h)),
                }
            }
            // a server context's list
            EE_WON_HOST_CTX_ADDRS | EE_WON_HOST_CTX_FAILED => {
                let l = core(EE_WON_HOST_CORE_CONTEXT, o[0]) as *const ServerList;
                if l.is_null() {
                    return if code == EE_WON_HOST_CTX_ADDRS {
                        Some(Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: Some(Vec::new()) })
                    } else {
                        num(0)
                    };
                }
                // SAFETY: the C++ context's own list (the caller holds the context)
                let l = unsafe { &*l };
                if code == EE_WON_HOST_CTX_FAILED {
                    let mut a = Address::default();
                    a.sockaddr[2..8].copy_from_slice(&addr);
                    l.notify_failed(&a);
                    return num(0);
                }
                let list = l.copy(crate::hostnative::crt_srand_and_rand);
                let bytes: Vec<u8> = list.iter().flat_map(|a| a.key()).collect();
                Some(Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: Some(bytes) })
            }
            _ => None,
        }
    }
}

/// `RustServerOp::GetHttpLastModifiedTime`: the reply's time, else the local file's (kept for next time), else 0.
fn http_modified(h: &EeWonHostOp) -> i64 {
    use crate::httpget::{EE_WON_HTTPGET_LAST_MODIFIED, EE_WON_HTTPGET_LOCAL_PATH};
    let t = h.num(EE_WON_HTTPGET_LAST_MODIFIED, 0);
    if t != 0 {
        return t;
    }
    let local = h.bytes(EE_WON_HTTPGET_LOCAL_PATH, 0).unwrap_or_default();
    if local.is_empty() {
        return 0;
    }
    // `WONFile(aLocal).GetModifyTime()` (`time_t`)
    let t = crate::hostnative::won_file_modify_time(&local);
    h.set(EE_WON_HTTPGET_LAST_MODIFIED, &SetValue { num: t, data: &[], addr: [0; 6], obj: Obj::none() });
    t
}

/// The op's member by number (`ServerOp`'s accessors).
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_base_get(op: *const EeWonHostOp, which: i32) -> i64 {
    // SAFETY: the caller's contract
    unsafe { (*op).base_get(which) }
}

/// Sets the op's member by number (an object member: `obj`, a reference taken; others: `num`).
///
/// # Safety
/// `op` is a live, natively bound op; `obj` NULL or a live `RefCount*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_base(op: *const EeWonHostOp, which: i32, num: i64, obj: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe { (*op).base_set(which, num, Obj::of(obj)) }
}

#[cfg(test)]
#[path = "hostbase_tests.rs"]
mod tests;
