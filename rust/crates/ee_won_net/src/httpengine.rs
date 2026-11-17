//! WONMisc's HTTPEngine (HTTPEngine.cpp) as Rust objects: `HTTPConnection`'s state (the shared socket, the send
//! and receive queues, the two running ops and their saved completions, the pipelining flag and counts, the retry
//! mode, the last use) and every step (`QueueOp`, `RunOp`, `OpCallback`'s pipelining and retry rules, `Kill`,
//! `GetNumOps`); `HTTPSession`'s connection map (by host, case-insensitively: `StringLessNoCase`) with `AddOp`,
//! `KillAll` and `GetNumOps`.
//!
//! The C++ classes are shims (`RefCount`s: a connection is the parameter of its ops' completions). The objects
//! held (the HTTPGetOps, the completions, the socket, the connections) are C++ `RefCount`s reached through the
//! hooks, and the locks are the C++ `CriticalSection`s, taken through the hooks where the original took them (a
//! connection's `OpCallback` runs mostly unlocked, as the original's did). No borrow of the state lives across a
//! hook: a kill, a run or a completion can come back into the connection or the session inside the call. The
//! behaviour is `golden_httpengine.txt`, recorded over the original's statements.
//!
//! Reached by the internet lobby's InitLogic (its HTTP documents: the MOTD, the terms of use, the ads; the servers
//! are gone): no LAN game, tour or EE-SIM run.

use std::collections::{BTreeMap, VecDeque};
use std::ffi::{c_int, c_long, c_void};
use std::sync::{Mutex, MutexGuard};

use crate::ffi::EeWonByteSink;
use crate::strcase;

/// `WS_Success`, `WS_Pending`, `WS_Killed`, `WS_HTTP_SendSuccess`
const WS_SUCCESS: c_long = 0;
const WS_PENDING: c_long = 100000;
const WS_KILLED: c_long = 100002;
const WS_HTTP_SEND_SUCCESS: c_long = 100097;

/// A `RefCount*` (an `HTTPGetOp`, a completion, a `BlockingSocket`, an `HTTPConnection`).
type Obj = *mut c_void;

/// The connection's C++ side: `context` is the `HTTPConnection`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonHttpConnHooks {
    pub context: *mut c_void,
    /// `mDataCrit.Enter()` / `Leave()`
    pub lock: unsafe extern "C" fn(*mut c_void),
    pub unlock: unsafe extern "C" fn(*mut c_void),
    pub add_ref: unsafe extern "C" fn(*mut c_void),
    pub release: unsafe extern "C" fn(*mut c_void),
    /// the op's `GetStatus`
    pub op_status: unsafe extern "C" fn(*mut c_void) -> c_long,
    /// `Kill(true)`
    pub op_kill: unsafe extern "C" fn(*mut c_void),
    /// `ForceFinish`
    pub op_force_finish: unsafe extern "C" fn(*mut c_void, c_long),
    /// `GetCompletion`: a reference, or NULL
    pub op_completion: unsafe extern "C" fn(*mut c_void) -> *mut c_void,
    /// `SetCompletion` (NULL clears it)
    pub op_set_completion: unsafe extern "C" fn(*mut c_void, *mut c_void),
    /// `SetCompletion(new ParamCompletion(StaticOpCallback, connection))`
    pub op_set_conn_completion: unsafe extern "C" fn(*mut c_void, *mut c_void),
    /// `SetCloseConnection(false)`, `SetDoRangeResume(false)`, `SetSocket(socket)` (NULL for none)
    pub op_prepare: unsafe extern "C" fn(*mut c_void, *mut c_void),
    /// `RunAsync(GetTimeout())`
    pub op_run_async: unsafe extern "C" fn(*mut c_void),
    /// `GetSocket`: a reference, or NULL
    pub op_socket: unsafe extern "C" fn(*mut c_void) -> *mut c_void,
    /// the op's send / receive flags: 0 send only, 1 send and receive, 2 receive only (`SetOnlyRecv`)
    pub op_set_mode: unsafe extern "C" fn(*mut c_void, c_int),
    /// `completion->Complete(AsyncOpPtr(op))`
    pub complete: unsafe extern "C" fn(*mut c_void, *mut c_void),
    /// the socket's `Kill`
    pub socket_kill: unsafe extern "C" fn(*mut c_void),
    /// `time(NULL)`
    pub now: unsafe extern "C" fn(*mut c_void) -> i64,
}

#[derive(Default)]
struct Conn {
    /// `mSocket`
    socket: Option<Obj>,
    /// `mNumPipelineFailures`, `mNumPipelineSuccesses`
    failures: i32,
    successes: i32,
    /// `mDoPipeline`
    do_pipeline: bool,
    /// `mLastUseTime`
    last_use: i64,
    /// `mOpQueue`: a reference each
    queue: [VecDeque<Obj>; 2],
    /// `mRetryMode`
    retry: bool,
    /// `mCurOp` (not held)
    cur: [Option<Obj>; 2],
    /// `mCurOpCompletion`
    cur_completion: [Option<Obj>; 2],
}

/// `HTTPConnection`.
pub struct EeWonHttpConn {
    hooks: EeWonHttpConnHooks,
    state: Mutex<Conn>,
}

/// The C++ lock, held until dropped.
struct Locked<'a>(&'a EeWonHttpConnHooks);
impl Drop for Locked<'_> {
    fn drop(&mut self) {
        // SAFETY: entered in `EeWonHttpConn::enter`
        unsafe { (self.0.unlock)(self.0.context) }
    }
}

impl EeWonHttpConn {
    pub(crate) fn new(hooks: EeWonHttpConnHooks) -> Self {
        EeWonHttpConn { hooks, state: Mutex::new(Conn { do_pipeline: true, ..Conn::default() }) }
    }

    fn st(&self) -> MutexGuard<'_, Conn> {
        self.state.lock().unwrap_or_else(|e| e.into_inner())
    }

    fn enter(&self) -> Locked<'_> {
        // SAFETY: the connection's lock
        unsafe { (self.hooks.lock)(self.hooks.context) };
        Locked(&self.hooks)
    }

    fn release(&self, o: Option<Obj>) {
        if let Some(p) = o {
            // SAFETY: a reference this object held
            unsafe { (self.hooks.release)(p) }
        }
    }

    #[cfg(test)]
    pub(crate) fn last_use(&self) -> i64 {
        self.st().last_use
    }

    /// `Kill`: the counts and flags reset; per queue, the running op killed (its completion may come back inside)
    /// and forgotten with its saved completion, then every queued op killed (and force-finished as killed when it
    /// was not pending) and the queue cleared; the socket killed and dropped.
    pub(crate) fn kill(&self) {
        let _l = self.enter();
        {
            let mut s = self.st();
            s.retry = false;
            s.failures = 0;
            s.successes = 0;
            s.do_pipeline = true;
        }
        for id in 0..2 {
            let cur = self.st().cur[id];
            if let Some(op) = cur {
                // SAFETY: the running op (held by its run)
                unsafe { (self.hooks.op_kill)(op) };
                let old = {
                    let mut s = self.st();
                    s.cur[id] = None;
                    s.cur_completion[id].take()
                };
                self.release(old);
            }
            // the walk reaches ops queued from inside a kill, as the original's list iterator did
            let mut i = 0;
            loop {
                let Some(op) = self.st().queue[id].get(i).copied() else { break };
                // SAFETY: a queued op (held by the queue)
                unsafe {
                    let was_not_pending = (self.hooks.op_status)(op) != WS_PENDING;
                    (self.hooks.op_kill)(op);
                    if was_not_pending {
                        (self.hooks.op_force_finish)(op, WS_KILLED);
                    }
                }
                i += 1;
            }
            let q = std::mem::take(&mut self.st().queue[id]);
            for op in q {
                self.release(Some(op));
            }
        }
        let socket = self.st().socket;
        if let Some(sock) = socket {
            // SAFETY: the held socket
            unsafe { (self.hooks.socket_kill)(sock) };
            let old = self.st().socket.take();
            self.release(old);
        }
    }

    /// `QueueOp(op)`: send only on the send queue while pipelining, else send and receive on the receive queue.
    pub(crate) fn queue_op(&self, op: Obj) {
        let _l = self.enter();
        let pipeline = self.st().do_pipeline;
        // SAFETY: a live op
        unsafe { (self.hooks.op_set_mode)(op, if pipeline { 0 } else { 1 }) };
        self.queue_op_id(op, if pipeline { 0 } else { 1 });
    }

    /// `QueueOp(op, id)`: appended (a reference); run when the queue was empty.
    pub(crate) fn queue_op_id(&self, op: Obj, id: usize) {
        let _l = self.enter();
        // SAFETY: a live op (the queue's reference)
        unsafe { (self.hooks.add_ref)(op) };
        let alone = {
            let mut s = self.st();
            s.queue[id].push_back(op);
            s.queue[id].len() == 1
        };
        if alone {
            self.run_op(id);
        }
    }

    /// `RunOp`: unless retrying, the queue's front op becomes the running one (when none runs): its completion
    /// saved and replaced by the connection's, the socket handed over, run with its timeout.
    pub(crate) fn run_op(&self, id: usize) {
        let _l = self.enter();
        let op = {
            let mut s = self.st();
            if s.retry || s.queue[id].is_empty() || s.cur[id].is_some() {
                return;
            }
            let op = s.queue[id].pop_front().unwrap_or(std::ptr::null_mut());
            s.cur[id] = Some(op);
            op
        };
        // SAFETY: the op popped (its reference is this call's until the end)
        unsafe {
            let completion = (self.hooks.op_completion)(op);
            let completion = (!completion.is_null()).then_some(completion);
            let old = std::mem::replace(&mut self.st().cur_completion[id], completion);
            self.release(old);
            (self.hooks.op_set_conn_completion)(op, self.hooks.context);
            let socket = self.st().socket.unwrap_or(std::ptr::null_mut());
            (self.hooks.op_prepare)(op, socket);
            (self.hooks.op_run_async)(op);
        }
        self.release(Some(op));
    }

    /// `OpCallback`: a running op ended. Its saved completion is put back. Outside retry mode the connection takes
    /// the op's socket (a receive with a socket counts toward pipelining's successes); no socket while pipelining
    /// with more on the line starts the retry mode, killing the running ops. In retry mode a received op completes
    /// and the rest are queued again; when nothing runs, the queues are queued again (pipelining off after three
    /// failures) and the next op runs. Otherwise a sent op with a socket moves to the receive queue, any other
    /// completes; then the queue's next op runs.
    pub(crate) fn op_callback(&self, op: Obj) {
        let found = {
            let mut s = self.st();
            match (0..2).find(|&i| s.cur[i] == Some(op)) {
                Some(id) => {
                    s.cur[id] = None;
                    Some((id, s.cur_completion[id].take()))
                }
                None => None,
            }
        };
        let Some((id, completion)) = found else { return };
        let comp = completion.unwrap_or(std::ptr::null_mut());
        // SAFETY: the completing op, alive for its completion; the saved completion this call holds
        unsafe { (self.hooks.op_set_completion)(op, comp) };

        if !self.st().retry {
            // SAFETY: the completing op
            let socket = unsafe { (self.hooks.op_socket)(op) };
            let socket = (!socket.is_null()).then_some(socket);
            let old = std::mem::replace(&mut self.st().socket, socket);
            self.release(old);
            let drop_line = {
                let mut s = self.st();
                if id == 1 && s.socket.is_some() {
                    s.successes += 1;
                    if s.successes > 2 {
                        s.failures = 0;
                    }
                }
                s.socket.is_none()
                    && s.do_pipeline
                    && (!s.queue[1].is_empty() || s.cur[1].is_some() || s.cur[0].is_some())
            };
            if drop_line {
                let _l = self.enter();
                self.st().retry = true;
                for i in 0..2 {
                    let cur = self.st().cur[i];
                    if let Some(c) = cur {
                        // SAFETY: a running op
                        unsafe { (self.hooks.op_kill)(c) };
                    }
                }
            }
        }

        if self.st().retry {
            // SAFETY: the completing op
            if id != 0 && unsafe { (self.hooks.op_status)(op) } == WS_SUCCESS {
                if let Some(c) = completion {
                    // SAFETY: the saved completion, held here
                    unsafe { (self.hooks.complete)(c, op) };
                }
            } else {
                self.queue_op(op);
            }
            let idle = {
                let s = self.st();
                s.cur[0].is_none() && s.cur[1].is_none()
            };
            if idle {
                let _l = self.enter();
                let last = {
                    let mut s = self.st();
                    let mut last = 1;
                    if s.do_pipeline && {
                        s.failures += 1;
                        s.failures > 2
                    } {
                        s.do_pipeline = false;
                        last = 0;
                    }
                    last
                };
                for q in (last..=1).rev() {
                    let ops = std::mem::take(&mut self.st().queue[q]);
                    for &o in &ops {
                        self.queue_op(o);
                    }
                    for o in ops {
                        self.release(Some(o));
                    }
                }
                let pipeline = {
                    let mut s = self.st();
                    s.retry = false;
                    s.do_pipeline
                };
                self.run_op(if pipeline { 0 } else { 1 });
            }
            self.release(completion);
            return;
        }
        let has_socket = self.st().socket.is_some();
        // SAFETY: the completing op
        if id == 0 && has_socket && unsafe { (self.hooks.op_status)(op) } == WS_HTTP_SEND_SUCCESS {
            // SAFETY: the completing op
            unsafe { (self.hooks.op_set_mode)(op, 2) };
            self.queue_op_id(op, 1);
        } else {
            if let Some(c) = completion {
                // SAFETY: the saved completion, held here
                unsafe { (self.hooks.complete)(c, op) };
            }
            // SAFETY: the context
            let now = unsafe { (self.hooks.now)(self.hooks.context) };
            self.st().last_use = now;
        }
        self.run_op(id);
        self.release(completion);
    }

    /// `GetNumOps`: the queued ops and the running ones (no lock, as the original).
    pub(crate) fn num_ops(&self) -> c_int {
        let s = self.st();
        let mut n = 0;
        for i in 0..2 {
            n += s.queue[i].len() as c_int;
            if s.cur[i].is_some() {
                n += 1;
            }
        }
        n
    }
}

impl Drop for EeWonHttpConn {
    /// The members in the original's reverse order: the saved completions, the queues, the socket.
    fn drop(&mut self) {
        let s = std::mem::take(&mut *self.st());
        for c in s.cur_completion.into_iter().rev() {
            self.release(c);
        }
        for q in s.queue.into_iter().rev() {
            for op in q {
                self.release(Some(op));
            }
        }
        self.release(s.socket);
    }
}

// ---- HTTPSession

/// The session's C++ side: `context` is the `HTTPSession`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonHttpSessionHooks {
    pub context: *mut c_void,
    pub lock: unsafe extern "C" fn(*mut c_void),
    pub unlock: unsafe extern "C" fn(*mut c_void),
    pub add_ref: unsafe extern "C" fn(*mut c_void),
    pub release: unsafe extern "C" fn(*mut c_void),
    /// the op's `mHost`, into the sink
    pub op_host: unsafe extern "C" fn(*mut c_void, EeWonByteSink, *mut c_void),
    /// `new HTTPConnection(NULL)`: a reference
    pub new_conn: unsafe extern "C" fn(*mut c_void) -> *mut c_void,
    /// the connection's `QueueOp(op)`
    pub conn_queue_op: unsafe extern "C" fn(*mut c_void, *mut c_void),
    /// the connection's `Kill`
    pub conn_kill: unsafe extern "C" fn(*mut c_void),
    /// the connection's `GetNumOps`
    pub conn_num_ops: unsafe extern "C" fn(*mut c_void) -> c_int,
}

/// `HTTPSession`: the connections by host, `StringLessNoCase`'s order (the upper-cased bytes, signed, then the
/// length; a host equal but for case finds the first one's connection).
pub struct EeWonHttpSession {
    hooks: EeWonHttpSessionHooks,
    conns: Mutex<BTreeMap<Vec<i8>, Option<Obj>>>,
}

fn host_key(host: &[u8]) -> Vec<i8> {
    host.iter().map(|&b| strcase::upper_byte(b) as i8).collect()
}

unsafe extern "C" fn vec_sink(context: *mut c_void, bytes: *const u8, length: usize) {
    // SAFETY: the Vec handed to the hook
    let v = unsafe { &mut *context.cast::<Vec<u8>>() };
    if length > 0 {
        // SAFETY: the hook's bytes
        v.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

struct SessionLocked<'a>(&'a EeWonHttpSessionHooks);
impl Drop for SessionLocked<'_> {
    fn drop(&mut self) {
        // SAFETY: entered in `EeWonHttpSession::enter`
        unsafe { (self.0.unlock)(self.0.context) }
    }
}

impl EeWonHttpSession {
    pub(crate) fn new(hooks: EeWonHttpSessionHooks) -> Self {
        EeWonHttpSession { hooks, conns: Mutex::new(BTreeMap::new()) }
    }

    fn map(&self) -> MutexGuard<'_, BTreeMap<Vec<i8>, Option<Obj>>> {
        self.conns.lock().unwrap_or_else(|e| e.into_inner())
    }

    fn enter(&self) -> SessionLocked<'_> {
        // SAFETY: the session's lock
        unsafe { (self.hooks.lock)(self.hooks.context) };
        SessionLocked(&self.hooks)
    }

    /// `AddOp`: the op queued on its host's connection, made on first use.
    pub(crate) fn add_op(&self, op: Obj) {
        let _l = self.enter();
        let mut host = Vec::new();
        // SAFETY: a live op
        unsafe { (self.hooks.op_host)(op, vec_sink, (&mut host as *mut Vec<u8>).cast()) };
        let key = host_key(&host);
        let stored = *self.map().entry(key.clone()).or_insert(None);
        let conn = match stored {
            Some(c) => c,
            None => {
                // SAFETY: the context
                let c = unsafe { (self.hooks.new_conn)(self.hooks.context) };
                let old = self.map().insert(key, Some(c)).flatten();
                if let Some(o) = old {
                    // SAFETY: a reference the map held
                    unsafe { (self.hooks.release)(o) };
                }
                c
            }
        };
        // SAFETY: the connection (a reference for the call, as the original's local SmartPtr)
        unsafe {
            (self.hooks.add_ref)(conn);
            (self.hooks.conn_queue_op)(conn, op);
            (self.hooks.release)(conn);
        }
    }

    /// The connections in map order, walked past inserts made inside a call.
    fn each(&self, mut f: impl FnMut(Obj)) {
        use std::ops::Bound::{Excluded, Unbounded};
        let mut at = self.map().iter().next().map(|(k, v)| (k.clone(), *v));
        while let Some((key, conn)) = at {
            if let Some(c) = conn {
                f(c);
            }
            let m = self.map();
            at = m.range::<Vec<i8>, _>((Excluded(&key), Unbounded)).next().map(|(k, v)| (k.clone(), *v));
        }
    }

    /// `KillAll`: every connection killed, in host order.
    pub(crate) fn kill_all(&self) {
        let _l = self.enter();
        // SAFETY: a connection the map holds
        self.each(|c| unsafe { (self.hooks.conn_kill)(c) });
    }

    /// `GetNumOps`: the connections' ops summed.
    pub(crate) fn num_ops(&self) -> c_int {
        let _l = self.enter();
        let mut n: c_int = 0;
        // SAFETY: a connection the map holds
        self.each(|c| n += unsafe { (self.hooks.conn_num_ops)(c) });
        n
    }
}

impl Drop for EeWonHttpSession {
    fn drop(&mut self) {
        let m = std::mem::take(&mut *self.map());
        for c in m.into_values().flatten() {
            // SAFETY: a reference the map held
            unsafe { (self.hooks.release)(c) };
        }
    }
}

// ---- the C ABI (HTTPEngine.cpp)

macro_rules! conn {
    ($c:expr) => {
        // SAFETY: a live connection (contract)
        unsafe { &*$c }
    };
}

/// # Safety
/// `hooks` points to a hook table whose context outlives the connection.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_new(hooks: *const EeWonHttpConnHooks) -> *mut EeWonHttpConn {
    // SAFETY: the caller's table
    Box::into_raw(Box::new(EeWonHttpConn::new(unsafe { *hooks })))
}

/// # Safety
/// `c` is from `ee_won_http_conn_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_free(c: *mut EeWonHttpConn) {
    if !c.is_null() {
        // SAFETY: the caller's connection
        drop(unsafe { Box::from_raw(c) });
    }
}

/// # Safety
/// `c` a connection.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_kill(c: *const EeWonHttpConn) {
    conn!(c).kill()
}

/// # Safety
/// `c` a connection, `op` an `HTTPGetOp`'s `RefCount*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_queue_op(c: *const EeWonHttpConn, op: *mut c_void) {
    conn!(c).queue_op(op)
}

/// # Safety
/// `c` a connection, `op` an `HTTPGetOp`'s `RefCount*`, `id` 0 or 1.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_queue_op_id(c: *const EeWonHttpConn, op: *mut c_void, id: c_int) {
    conn!(c).queue_op_id(op, (id != 0) as usize)
}

/// # Safety
/// `c` a connection, `id` 0 or 1.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_run_op(c: *const EeWonHttpConn, id: c_int) {
    conn!(c).run_op((id != 0) as usize)
}

/// # Safety
/// `c` a connection, `op` the completing op's `RefCount*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_op_callback(c: *const EeWonHttpConn, op: *mut c_void) {
    conn!(c).op_callback(op)
}

/// # Safety
/// `c` a connection.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_conn_num_ops(c: *const EeWonHttpConn) -> c_int {
    conn!(c).num_ops()
}

/// # Safety
/// `hooks` points to a hook table whose context outlives the session.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_session_new(hooks: *const EeWonHttpSessionHooks) -> *mut EeWonHttpSession {
    // SAFETY: the caller's table
    Box::into_raw(Box::new(EeWonHttpSession::new(unsafe { *hooks })))
}

/// # Safety
/// `s` is from `ee_won_http_session_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_session_free(s: *mut EeWonHttpSession) {
    if !s.is_null() {
        // SAFETY: the caller's session
        drop(unsafe { Box::from_raw(s) });
    }
}

/// # Safety
/// `s` a session, `op` an `HTTPGetOp`'s `RefCount*`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_session_add_op(s: *const EeWonHttpSession, op: *mut c_void) {
    // SAFETY: a live session (contract)
    unsafe { &*s }.add_op(op)
}

/// # Safety
/// `s` a session.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_session_kill_all(s: *const EeWonHttpSession) {
    // SAFETY: a live session (contract)
    unsafe { &*s }.kill_all()
}

/// # Safety
/// `s` a session.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_session_num_ops(s: *const EeWonHttpSession) -> c_int {
    // SAFETY: a live session (contract)
    unsafe { &*s }.num_ops()
}

#[cfg(test)]
#[path = "httpengine_tests.rs"]
mod tests;
