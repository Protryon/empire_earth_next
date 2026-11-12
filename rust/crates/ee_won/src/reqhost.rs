//! `ServerRequestOp` on the Rust op base (round 10): the request/reply exchange every Internet service op runs (the
//! server list's failover, the sessions, peer authentication, the expired-session retry: server.rs's steps) with
//! its members in a Rust object and its primitives host calls (hostop.rs's [`Host`]), so an op derived from it is
//! a `RustServerOp` with no C++ class of its own. `ServerRequestOp.cpp`'s statements are [`Env`] (the exchange's
//! primitives), [`ReqPlan`]'s `run`, `cleanup` and `callback` (its virtual hooks) and the datagram path
//! (ReportPatchStatusOp's `SetUDPSocket`).
//!
//! A derived op is a [`HostRequest`]: its `RunHook` prelude, request, reply, `Reset` and results. The ops of
//! rounds 7 and 8 (reqop.rs's [`RequestOp`]) are one through [`Plain`], which asks the host for the certificate's
//! user id and the tick count as `ServerRequestOp::GetNextRequest` and `CheckResponse` did. The derived op sits
//! behind a lock of its own: its steps may make host calls (none that call back into the op), while the
//! exchange's members stay behind the base's lock, taken only for statements that make no call.
//!
//! The behaviour is `golden_hostop.txt`'s `R` sequences: `tests/won/won_hostop_test.cpp`'s original half is
//! ServerRequestOp.cpp's statements as they were (server.rs's exchange over its members and the host calls), with
//! a contest call, a ping and a user id lookup as the derived ops. Reached by nothing in the game but the lobby's
//! version check, add friend and lost password and user name requests (an Internet login; the servers are gone).

use std::sync::{Arc, Mutex, MutexGuard};

use crate::hostop::{
    hex, EeWonHostOp, Host, Obj, Plan, SetValue, EE_WON_HOST_OP_CONNECT, EE_WON_HOST_OP_RECV_MSG,
    EE_WON_HOST_OP_SEND_BYTES_TO, EE_WON_HOST_OP_SEND_MSG, FOREVER,
};
use crate::hostop::{EE_WON_HOST_REQ_CHECK, EE_WON_HOST_REQ_NEXT, EE_WON_HOST_REQ_RESET};
use crate::reqop::{Checked, EeWonReqOp, RequestOp};
use crate::server::{self, st, EeWonServerReqDone, RequestEnv};

/// The current server's address (`GetCurAddr()`, six bytes): `ee_won_hostop_bytes`
pub const EE_WON_REQHOST_CUR_ADDR: i32 = -1;
/// `SetServerContext(obj)` (the op takes its own reference): `ee_won_hostop_set`
pub const EE_WON_REQHOST_CONTEXT: i32 = -2;
/// `SetAddr(addr)`: `ee_won_hostop_set`; `GetAddr()` (the first address, six bytes; none: invalid):
/// `ee_won_hostop_bytes`
pub const EE_WON_REQHOST_ADDR: i32 = -3;
/// `SetUDPSocket(obj)`: the request sent as one datagram on that socket instead (`ee_won_hostop_set`)
pub const EE_WON_REQHOST_UDP_SOCKET: i32 = -4;
/// `SetLastServerError(num)` (an op derived in C++: its `CheckResponse`'s own, and `InvalidReplyHeader`'s):
/// `ee_won_hostop_set`
pub const EE_WON_REQHOST_LAST_ERROR: i32 = -5;

/// `AUTH_TYPE_SESSION`
const AUTH_SESSION: i32 = 1;

/// What a derived op's steps may reach: the host and the exchange's members.
pub(crate) struct Calls<'a> {
    pub(crate) h: &'a Host<ReqPlan>,
}

/// An op derived from `ServerRequestOp` on the base.
pub(crate) trait HostRequest: Send {
    /// the `mLengthFieldSize` the op's constructor set
    fn length_field_size(&self) -> u8;
    /// the `mUseAuth2` the op's constructor set
    fn use_auth2(&self) -> bool;
    /// the `ServerOp` time limits the op's constructor set (`ServerRequestOp::Init`'s 20 seconds)
    fn max_times(&self) -> [u32; 3] {
        [20000; 3]
    }
    /// the op's `RunHook` before the base's: a status finishes the op instead
    fn run_hook(&mut self, c: &Calls) -> Option<i32>;
    /// `GetNextRequest` (a datagram's: no length field): the status and the request (`None` leaves `mRequest`)
    fn next_request(&mut self, c: &Calls, datagram: bool) -> (i32, Option<Vec<u8>>);
    /// `CheckResponse` over `mResponse`
    fn check_response(&mut self, c: &Calls, response: &[u8]) -> Checked;
    /// `Reset` (the next server)
    fn reset(&mut self, c: &Calls);
    /// the op's `CleanupHook` statements before (`after` false) and after the base's
    fn cleanup(&mut self, _c: &Calls, _after: bool) {}
    /// the op's `CallbackHook` for a completion of its own (`None`: the base's)
    fn callback(&mut self, _c: &Calls, _id: i32, _op: Obj) -> Option<Own> {
        None
    }
    /// the op's `RecvServerDatagram` (`CheckResponse`'s [`Checked::Datagram`]): its status, or a receive op to
    /// track with id 1000 and run asynchronously with the wait
    fn datagram(&mut self, _c: &Calls) -> Result<i32, (Obj, u32)> {
        Ok(st::SUCCESS)
    }
    /// the op's members, one line
    fn describe(&self) -> String;
    /// a result by the op's number
    fn num(&self, which: i32, index: i64) -> i64;
    /// a result's bytes by the op's number
    fn bytes(&self, which: i32, index: i64) -> Option<Vec<u8>>;
    /// a setter by the op's number
    fn set(&mut self, _c: &Calls, _which: i32, _value: &SetValue) {}
    /// a result list of directory entities by the op's number
    fn entities(&self, _which: i32, _index: i64) -> Vec<crate::dir::Entity> {
        Vec::new()
    }
    /// every object reference the op holds, taken
    fn take_objects(&mut self) -> Vec<Obj> {
        Vec::new()
    }
}

/// A derived op's own completion (`CallbackHook`), run after its lock is let go.
pub(crate) enum Own {
    /// `Finish(status); return true`
    Finish(i32),
    /// `RecvServerDatagram(); return true`
    Datagram,
}

/// A round 7/8 op (reqop.rs) on the base: `ServerRequestOp`'s own statements around its calls.
pub(crate) struct Plain(pub(crate) Box<dyn RequestOp>);

impl HostRequest for Plain {
    fn length_field_size(&self) -> u8 {
        self.0.length_field_size()
    }
    fn use_auth2(&self) -> bool {
        self.0.use_auth2()
    }
    fn run_hook(&mut self, c: &Calls) -> Option<i32> {
        let auth_type = c.h.auth_type();
        if let Some(s) = self.0.refuses(auth_type) {
            return Some(s);
        }
        self.0.run_hook();
        None
    }
    fn next_request(&mut self, c: &Calls, datagram: bool) -> (i32, Option<Vec<u8>>) {
        let user_id = if self.0.needs_user_id() { c.h.cert_user_id() } else { 0 };
        let now = c.h.tick();
        self.0.set_env(user_id, now);
        let lfs = if datagram { 0 } else { c.h.lfs() };
        self.0.next_request(lfs)
    }
    fn check_response(&mut self, c: &Calls, response: &[u8]) -> Checked {
        let now = c.h.tick();
        self.0.set_env(0, now);
        self.0.check_response(response)
    }
    fn reset(&mut self, _c: &Calls) {
        self.0.reset()
    }
    fn describe(&self) -> String {
        self.0.describe()
    }
    fn num(&self, which: i32, index: i64) -> i64 {
        self.0.num_at(which, index)
    }
    fn bytes(&self, which: i32, _index: i64) -> Option<Vec<u8>> {
        self.0.bytes(which)
    }
    fn set(&mut self, _c: &Calls, which: i32, value: &SetValue) {
        self.0.set(which, value.num, value.data)
    }
    fn entities(&self, which: i32, index: i64) -> Vec<crate::dir::Entity> {
        self.0.entities(which, index)
    }
}

/// An op still derived from `ServerRequestOp` in C++ (the directory, account and routing request ops): its
/// `GetNextRequest`, `CheckResponse` and `Reset` are the C++ class's virtuals, one host call each; its members are
/// the C++ object's.
pub(crate) struct CppReq;

impl HostRequest for CppReq {
    fn length_field_size(&self) -> u8 {
        4
    }
    fn use_auth2(&self) -> bool {
        true
    }
    fn run_hook(&mut self, _c: &Calls) -> Option<i32> {
        None
    }
    fn next_request(&mut self, c: &Calls, _datagram: bool) -> (i32, Option<Vec<u8>>) {
        let r = c.h.call(EE_WON_HOST_REQ_NEXT, [Obj::none(); 3], [0, 0], &[], [0; 6]);
        (r.num as i32, r.bytes)
    }
    fn check_response(&mut self, c: &Calls, response: &[u8]) -> Checked {
        let has = c.h.st().ex.response.is_some();
        let r = c.h.call(EE_WON_HOST_REQ_CHECK, [Obj::none(); 3], [i64::from(has), 0], response, [0; 6]);
        if r.addr[0] == 1 {
            Checked::Cut
        } else {
            Checked::Status(r.num as i32)
        }
    }
    fn reset(&mut self, c: &Calls) {
        c.h.call(EE_WON_HOST_REQ_RESET, [Obj::none(); 3], [0, 0], &[], [0; 6]);
    }
    fn describe(&self) -> String {
        "c++".into()
    }
    fn num(&self, _which: i32, _index: i64) -> i64 {
        0
    }
    fn bytes(&self, _which: i32, _index: i64) -> Option<Vec<u8>> {
        None
    }
}

/// `ServerRequestOp`'s members.
pub(crate) struct Ex {
    pub(crate) addrs: Vec<[u8; 6]>,
    /// `mAddrItr` as an index (the list's begin before any `RunHook` and after the context's copy, where the
    /// original's was indeterminate or dangling; the begin's predecessor is the end, as a list's)
    pub(crate) itr: usize,
    pub(crate) cur_addr: [u8; 6],
    pub(crate) cur_session: Obj,
    pub(crate) used_session: Obj,
    pub(crate) socket: Obj,
    pub(crate) context: Obj,
    pub(crate) udp: Obj,
    /// unset before a `RunHook` in the original (false here)
    pub(crate) reusing: bool,
    pub(crate) expired: bool,
    pub(crate) use_auth2: bool,
    pub(crate) close_connection: bool,
    pub(crate) use_last: bool,
    pub(crate) get_cert_status: i32,
    pub(crate) last_error: i32,
    /// `mRequest` (`None`: NULL)
    pub(crate) request: Option<Vec<u8>>,
    /// `mResponse` (`None`: NULL)
    pub(crate) response: Option<Vec<u8>>,
}

/// The op: the exchange's members and the derived op (behind its own lock).
pub(crate) struct ReqPlan {
    pub(crate) ex: Ex,
    op: Arc<Mutex<Box<dyn HostRequest>>>,
}

impl ReqPlan {
    pub(crate) fn new(op: Box<dyn HostRequest>) -> ReqPlan {
        let use_auth2 = op.use_auth2();
        ReqPlan {
            ex: Ex {
                addrs: Vec::new(),
                itr: 0,
                cur_addr: [0; 6],
                cur_session: Obj::none(),
                used_session: Obj::none(),
                socket: Obj::none(),
                context: Obj::none(),
                udp: Obj::none(),
                reusing: false,
                expired: false,
                use_auth2,
                close_connection: true,
                use_last: false,
                get_cert_status: st::NONE,
                last_error: st::NONE,
                request: None,
                response: None,
            },
            op: Arc::new(Mutex::new(op)),
        }
    }
    fn op(&self) -> MutexGuard<'_, Box<dyn HostRequest>> {
        self.op.lock().unwrap_or_else(|e| e.into_inner())
    }
}

/// The derived op, locked (never while the base's lock is held: its steps may make host calls).
fn derived(h: &Host<ReqPlan>) -> Arc<Mutex<Box<dyn HostRequest>>> {
    h.st().op.clone()
}

macro_rules! with_op {
    ($h:expr, |$o:ident| $body:expr) => {{
        let a = derived($h);
        let mut g = a.lock().unwrap_or_else(|e| e.into_inner());
        let $o = &mut *g;
        $body
    }};
}

/// `SetLastServerError` over the members.
pub(crate) fn set_last_server_error(h: &Host<ReqPlan>, error: i32) {
    let mut s = h.st();
    let (last, use_last) = server::last_server_error(s.ex.last_error, s.ex.use_last, error);
    s.ex.last_error = last;
    s.ex.use_last = use_last;
}

/// `SmartPtr` assignment from another slot's object: the new reference before the old one goes.
fn assign(h: &Host<ReqPlan>, slot: fn(&mut ReqPlan) -> &mut Obj, o: Obj) {
    h.add_ref(o);
    h.replace(slot, o);
}

fn cur_session(p: &mut ReqPlan) -> &mut Obj {
    &mut p.ex.cur_session
}
fn used_session(p: &mut ReqPlan) -> &mut Obj {
    &mut p.ex.used_session
}
fn socket(p: &mut ReqPlan) -> &mut Obj {
    &mut p.ex.socket
}
fn context(p: &mut ReqPlan) -> &mut Obj {
    &mut p.ex.context
}
fn udp(p: &mut ReqPlan) -> &mut Obj {
    &mut p.ex.udp
}

/// `QueueSocketOp(op, timeout)`: tracked with id 1, queued on the socket; the op's reference goes.
fn queue_socket_op(h: &Host<ReqPlan>, op: Obj, timeout: u32) {
    h.track(op, 1);
    let s = h.st().ex.socket;
    h.queue(s, op, timeout);
    h.release(op);
}

fn new_peer_auth(h: &Host<ReqPlan>) -> Obj {
    let (addr, s, auth2) = {
        let p = h.st();
        (p.ex.cur_addr, p.ex.socket, p.ex.use_auth2)
    };
    h.new_peer_auth(addr, s, auth2)
}

/// The exchange's primitives (`ServerRequestOp.cpp`'s), over the host and the members.
struct Env<'a> {
    h: &'a Host<ReqPlan>,
    /// `CallbackHook`'s completed op
    done: Obj,
}

impl RequestEnv for Env<'_> {
    fn status(&mut self) -> i32 {
        self.h.status()
    }
    fn time_left(&mut self) -> u32 {
        self.h.time_left()
    }
    fn is_async(&mut self) -> bool {
        self.h.is_async()
    }
    fn auth_type(&mut self) -> i32 {
        self.h.auth_type()
    }
    fn finish(&mut self, status: i32) {
        self.h.finish(status)
    }
    fn finish_save_session(&mut self, status: i32) {
        let h = self.h;
        h.finish(status);
        // after the finish (its cleanup may have let the session go)
        if h.auth_type() == AUTH_SESSION {
            let (addr, s) = {
                let p = h.st();
                (p.ex.cur_addr, p.ex.cur_session)
            };
            h.put_session(addr, s);
        }
        let s = h.st().ex.cur_session;
        assign(h, used_session, s);
        h.replace(cur_session, Obj::none());
    }
    fn kill(&mut self) {
        self.h.kill(Obj::none(), true)
    }
    fn last_error(&mut self) -> i32 {
        self.h.st().ex.last_error
    }
    fn set_last_error(&mut self, v: i32) {
        self.h.st().ex.last_error = v
    }
    fn use_last(&mut self) -> bool {
        self.h.st().ex.use_last
    }
    fn set_use_last(&mut self, v: bool) {
        self.h.st().ex.use_last = v
    }
    fn reusing(&mut self) -> bool {
        self.h.st().ex.reusing
    }
    fn set_reusing(&mut self, v: bool) {
        self.h.st().ex.reusing = v
    }
    fn expired(&mut self) -> bool {
        self.h.st().ex.expired
    }
    fn set_expired(&mut self, v: bool) {
        self.h.st().ex.expired = v
    }
    fn set_get_cert_status(&mut self, v: i32) {
        self.h.st().ex.get_cert_status = v
    }
    fn prepare_addresses(&mut self) -> bool {
        let h = self.h;
        h.replace(used_session, Obj::none());
        let c = h.st().ex.context;
        if !c.is_none() {
            h.st().ex.addrs.clear();
            let a = h.ctx_addrs(c);
            let mut p = h.st();
            p.ex.addrs = a;
            // the original's iterator dangled here (an empty copy finishes before the rewind): the list's begin
            p.ex.itr = 0;
        }
        !h.st().ex.addrs.is_empty()
    }
    fn rewind_addresses(&mut self) {
        self.h.st().ex.itr = 0
    }
    fn at_begin(&mut self) -> bool {
        self.h.st().ex.itr == 0
    }
    fn at_end(&mut self) -> bool {
        let p = self.h.st();
        p.ex.itr >= p.ex.addrs.len()
    }
    fn has_context(&mut self) -> bool {
        !self.h.st().ex.context.is_none()
    }
    fn has_auth_context(&mut self) -> bool {
        self.h.has_auth()
    }
    fn notify_failed(&mut self) {
        let (c, a) = {
            let p = self.h.st();
            (p.ex.context, p.ex.cur_addr)
        };
        self.h.ctx_failed(c, a)
    }
    fn next_addr(&mut self) {
        let h = self.h;
        let s = h.new_socket(false);
        h.replace(socket, s);
        let lfs = h.lfs();
        h.set_lfs(s, lfs);
        let mut p = h.st();
        let i = p.ex.itr;
        if let Some(a) = p.ex.addrs.get(i).copied() {
            p.ex.cur_addr = a;
        }
        p.ex.itr = i + 1;
    }
    fn retreat_addr(&mut self) {
        // a list's iterator: the begin's predecessor is the end
        let mut p = self.h.st();
        p.ex.itr = if p.ex.itr == 0 { p.ex.addrs.len() } else { p.ex.itr - 1 };
    }
    fn reset_server(&mut self) {
        let h = self.h;
        h.kill_all();
        with_op!(h, |o| o.reset(&Calls { h }));
        let s = h.st().ex.socket;
        if !s.is_none() {
            h.close(s);
            h.replace(socket, Obj::none());
        }
    }
    fn load_session(&mut self) {
        let h = self.h;
        let a = h.st().ex.cur_addr;
        let s = h.get_session(a);
        h.replace(cur_session, s);
    }
    fn clear_session(&mut self) {
        self.h.replace(cur_session, Obj::none())
    }
    fn has_session(&mut self) -> bool {
        !self.h.st().ex.cur_session.is_none()
    }
    fn add_crypt(&mut self) {
        let (s, c) = {
            let p = self.h.st();
            (p.ex.socket, p.ex.cur_session)
        };
        self.h.add_crypt(s, c)
    }
    fn queue_connect(&mut self) {
        let h = self.h;
        let a = h.st().ex.cur_addr;
        let op = h.new_op(EE_WON_HOST_OP_CONNECT, Obj::none(), &[], a, 0);
        let t = h.connect_time();
        queue_socket_op(h, op, t);
    }
    fn connect_block(&mut self) -> i32 {
        let h = self.h;
        let t = h.connect_time();
        let (s, a) = {
            let p = h.st();
            (p.ex.socket, p.ex.cur_addr)
        };
        h.connect(s, a, t)
    }
    fn peer_auth_async(&mut self) {
        let h = self.h;
        let p = new_peer_auth(h);
        h.track(p, 2);
        h.run_async(p, FOREVER);
        h.release(p);
    }
    fn peer_auth_block(&mut self, status: &mut i32, cert: &mut i32) -> bool {
        let h = self.h;
        let p = new_peer_auth(h);
        let t = h.time_left();
        if h.run_block(p, t) {
            let s = h.peer_auth_session(p);
            h.replace(cur_session, s);
            h.release(p);
            return true;
        }
        *status = h.op_status(p);
        *cert = h.peer_auth_cert(p);
        h.release(p);
        false
    }
    fn get_next_request(&mut self) -> i32 {
        next_request(self.h, false)
    }
    fn queue_send(&mut self) {
        let h = self.h;
        let r = h.st().ex.request.clone().unwrap_or_default();
        let op = h.new_op(EE_WON_HOST_OP_SEND_MSG, Obj::none(), &r, [0; 6], 0);
        let t = h.send_time();
        queue_socket_op(h, op, t);
    }
    fn send_block(&mut self) -> i32 {
        let h = self.h;
        let t = h.send_time();
        let (s, r) = {
            let p = h.st();
            (p.ex.socket, p.ex.request.clone().unwrap_or_default())
        };
        h.send_msg(s, &r, t)
    }
    fn queue_recv(&mut self) {
        let h = self.h;
        let op = h.new_op(EE_WON_HOST_OP_RECV_MSG, Obj::none(), &[], [0; 6], 0);
        let t = h.recv_time();
        queue_socket_op(h, op, t);
    }
    fn recv_block(&mut self) -> i32 {
        let h = self.h;
        let t = h.recv_time();
        let s = h.st().ex.socket;
        let (status, b) = h.recv_msg(s, t);
        h.st().ex.response = b;
        status
    }
    fn response(&mut self) -> Vec<u8> {
        self.h.st().ex.response.clone().unwrap_or_default()
    }
    fn check_response(&mut self) -> Option<i32> {
        let h = self.h;
        let r = h.st().ex.response.clone().unwrap_or_default();
        match with_op!(h, |o| o.check_response(&Calls { h }, &r)) {
            Checked::Cut => None,
            Checked::InvalidHeader => {
                set_last_server_error(h, st::INVALID_REPLY_HEADER);
                Some(st::TRY_NEXT_SERVER)
            }
            Checked::Status(s) => Some(s),
            Checked::Datagram => Some(recv_datagram(h)),
        }
    }
    fn take_op_session(&mut self) {
        let s = self.h.peer_auth_session(self.done);
        self.h.replace(cur_session, s);
    }
    fn take_op_response(&mut self) {
        let b = self.h.op_bytes(self.done);
        self.h.st().ex.response = b;
    }
}

/// `RecvServerDatagram()`: the derived op's, its asynchronous receive tracked and run after its lock is let go.
fn recv_datagram(h: &Host<ReqPlan>) -> i32 {
    match with_op!(h, |o| o.datagram(&Calls { h })) {
        Ok(s) => s,
        Err((recv, wait)) => {
            h.track(recv, 1000);
            h.run_async(recv, wait);
            st::EXIT_COMMUNICATION_LOOP
        }
    }
}

/// `GetNextRequest()`: the derived op's, its request kept as `mRequest`.
fn next_request(h: &Host<ReqPlan>, datagram: bool) -> i32 {
    let (status, request) = with_op!(h, |o| o.next_request(&Calls { h }, datagram));
    if let Some(r) = request {
        h.st().ex.request = Some(r);
    }
    status
}

/// `RunDatagram`: the request without a length field to the first address on the datagram socket, then Success.
fn run_datagram(h: &Host<ReqPlan>) {
    let addr = h.st().ex.addrs.first().copied();
    let Some(addr) = addr else {
        h.finish(st::NO_SERVERS_SPECIFIED);
        return;
    };
    with_op!(h, |o| o.reset(&Calls { h }));
    next_request(h, true);
    let (u, r) = {
        let p = h.st();
        (p.ex.udp, p.ex.request.clone().unwrap_or_default())
    };
    let op = h.new_op(EE_WON_HOST_OP_SEND_BYTES_TO, u, &r, addr, 0);
    let is_async = h.is_async();
    let t = h.timeout();
    if is_async {
        h.run_async(op, t);
    } else {
        h.run_block(op, t);
    }
    h.finish(st::SUCCESS);
    h.release(op);
}

impl Plan for ReqPlan {
    fn run(h: &Host<Self>) {
        let refused = with_op!(h, |o| o.run_hook(&Calls { h }));
        if let Some(s) = refused {
            h.finish(s);
            return;
        }
        if !h.st().ex.udp.is_none() {
            run_datagram(h);
            return;
        }
        server::run_hook(&mut Env { h, done: Obj::none() });
    }

    fn callback(h: &Host<Self>, id: i32, op: Obj) -> bool {
        match with_op!(h, |o| o.callback(&Calls { h }, id, op)) {
            Some(Own::Finish(s)) => {
                h.finish(s);
                return true;
            }
            Some(Own::Datagram) => {
                recv_datagram(h);
                return true;
            }
            None => {}
        }
        let killed = h.op_killed(op);
        let status = h.op_status(op);
        let cert_status =
            if id == 2 && !killed && status == st::PEER_AUTH_GET_CERT_FAILURE { h.peer_auth_cert(op) } else { 0 };
        let is_recv = id == 1 && h.op_is_recv_msg(op);
        let done = EeWonServerReqDone { killed, status, param: id, cert_status, is_recv };
        server::callback(&mut Env { h, done: op }, &done)
    }

    fn cleanup(h: &Host<Self>) {
        with_op!(h, |o| o.cleanup(&Calls { h }, false));
        h.base_cleanup();
        let status = h.status();
        let killed = h.op_killed(Obj::none());
        let (c, s, close, a) = {
            let p = h.st();
            (p.ex.context, p.ex.socket, p.ex.close_connection, p.ex.cur_addr)
        };
        let plan = server::cleanup_plan(status, killed, !c.is_none(), !s.is_none(), close);
        if plan & 1 != 0 {
            h.ctx_failed(c, a);
        }
        if plan & 2 != 0 {
            h.close(s);
            h.replace(socket, Obj::none());
        }
        h.replace(cur_session, Obj::none());
        with_op!(h, |o| o.cleanup(&Calls { h }, true));
    }

    fn num(&self, which: i32, index: i64) -> i64 {
        self.op().num(which, index)
    }

    fn bytes(&self, which: i32, index: i64) -> Option<Vec<u8>> {
        if which == EE_WON_REQHOST_CUR_ADDR {
            return Some(self.ex.cur_addr.to_vec());
        }
        if which == EE_WON_REQHOST_ADDR {
            // `GetAddr()`: the first address (none: an invalid one)
            return self.ex.addrs.first().map(|a| a.to_vec());
        }
        self.op().bytes(which, index)
    }

    fn set(h: &Host<Self>, which: i32, value: &SetValue) {
        match which {
            EE_WON_REQHOST_CONTEXT => assign(h, context, value.obj),
            EE_WON_REQHOST_ADDR => {
                h.replace(context, Obj::none());
                let mut p = h.st();
                p.ex.addrs.clear();
                p.ex.addrs.push(value.addr);
            }
            EE_WON_REQHOST_UDP_SOCKET => assign(h, udp, value.obj),
            // during the derived op's own step (its lock held): the members only
            EE_WON_REQHOST_LAST_ERROR => set_last_server_error(h, value.num as i32),
            _ => with_op!(h, |o| o.set(&Calls { h }, which, value)),
        }
    }

    fn setup(&self) -> (u8, [u32; 3]) {
        let o = self.op();
        (o.length_field_size(), o.max_times())
    }

    fn entities(&self, which: i32, index: i64) -> Vec<crate::dir::Entity> {
        self.op().entities(which, index)
    }

    fn describe(&self) -> String {
        let e = &self.ex;
        let mut a = String::new();
        for x in &e.addrs {
            a.push_str(&hex(x));
            a.push(',');
        }
        let opt = |b: &Option<Vec<u8>>| match b {
            None => "null".to_string(),
            Some(b) => hex(b),
        };
        format!(
            "{a} i{} cur {} s{}/{} sock {} udp {} f{}{}{}{} l{} c{} {} {} | {}",
            e.itr.min(e.addrs.len()),
            hex(&e.cur_addr),
            e.cur_session.0,
            e.used_session.0,
            e.socket.0,
            e.udp.0,
            u8::from(e.reusing),
            u8::from(e.expired),
            u8::from(e.use_last),
            u8::from(e.use_auth2),
            e.last_error,
            e.get_cert_status,
            opt(&e.request),
            opt(&e.response),
            self.op().describe()
        )
    }

    fn take_objects(&mut self) -> Vec<Obj> {
        // the derived op's members go first, as its destructor ran before the base's
        let mut v = self.op().take_objects();
        let e = &mut self.ex;
        v.extend(
            [&mut e.cur_session, &mut e.used_session, &mut e.socket, &mut e.context, &mut e.udp]
                .into_iter()
                .map(|o| std::mem::replace(o, Obj::none()))
                .filter(|o| !o.is_none()),
        );
        v
    }
}

/// An op derived from `ServerRequestOp` on the base: the round 7/8 Rust op (`ee_won_reqop_*`'s constructors), taken.
/// Its server context or address are set after the bind (`EE_WON_REQHOST_CONTEXT`, `EE_WON_REQHOST_ADDR`).
///
/// # Safety
/// `op` came from a Rust op constructor and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_request(op: *mut EeWonReqOp) -> *mut EeWonHostOp {
    // SAFETY: the constructors' box, taken
    let b = unsafe { Box::from_raw(op) };
    EeWonHostOp::boxed(ReqPlan::new(Box::new(Plain(b.op))))
}

/// An op derived from `ServerRequestOp` in C++ (`ServerRequestOp`'s constructors): its virtuals answer
/// `EE_WON_HOST_REQ_*`. Its server context or address are set after the bind.
#[no_mangle]
pub extern "C" fn ee_won_hostop_cpp_request() -> *mut EeWonHostOp {
    EeWonHostOp::boxed(ReqPlan::new(Box::new(CppReq)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::reqops::UserIdOp;

    #[test]
    fn a_new_request_starts_at_the_list_begin() {
        let p = ReqPlan::new(Box::new(Plain(Box::new(UserIdOp::default()))));
        assert_eq!(p.setup(), (4, [20000; 3]));
        assert!(p.ex.use_auth2 && p.ex.close_connection);
        assert_eq!((p.ex.last_error, p.ex.get_cert_status), (st::NONE, st::NONE));
        assert_eq!(p.bytes(EE_WON_REQHOST_CUR_ADDR, 0), Some(vec![0; 6]));
    }

    use crate::hostop::{
        EeWonHostCall, EE_WON_HOST_FINISH, EE_WON_HOST_LFS, EE_WON_HOST_MODE, EE_WON_HOST_NEW_SOCKET,
        EE_WON_HOST_RECV_MSG, EE_WON_HOST_SEND_MSG, EE_WON_HOST_STATUS, EE_WON_HOST_TIME_LEFT,
    };
    use std::cell::RefCell;
    use std::ffi::c_void;

    thread_local! {
        /// the calls made, and how many replies the C++ op's CheckResponse threw on first
        static LOG: RefCell<(Vec<String>, i32)> = const { RefCell::new((Vec::new(), 0)) };
    }

    /// A blocking exchange's C++ side: one server, a C++ derived op asking for one reply.
    unsafe extern "C" fn cpp_host(_host: *mut c_void, call: *mut EeWonHostCall) {
        // SAFETY: the op's call
        let c = unsafe { &mut *call };
        let data = unsafe { crate::ffi::bytes(c.data, c.len) }.to_vec();
        let give = |c: &mut EeWonHostCall, b: &[u8]| unsafe { (c.out_sink)(c.out_context, b.as_ptr(), b.len()) };
        LOG.with(|l| {
            let mut l = l.borrow_mut();
            match c.code {
                EE_WON_HOST_STATUS => c.out_num = i64::from(st::PENDING),
                EE_WON_HOST_MODE => c.out_num = 2,
                EE_WON_HOST_LFS => c.out_num = 4,
                EE_WON_HOST_TIME_LEFT => c.out_num = 5000,
                EE_WON_HOST_NEW_SOCKET => c.out_obj = 7 as *mut c_void,
                EE_WON_HOST_REQ_NEXT => {
                    c.out_num = i64::from(st::RECV);
                    give(c, &[1, 2]);
                    l.0.push("next".into());
                }
                EE_WON_HOST_SEND_MSG => l.0.push(format!("send {}", hex(&data))),
                EE_WON_HOST_RECV_MSG => give(c, &[9]),
                EE_WON_HOST_REQ_CHECK => {
                    l.0.push(format!("check {} {}", c.num[0], hex(&data)));
                    if l.1 > 0 {
                        l.1 -= 1;
                        c.out_addr[0] = 1;
                    }
                }
                EE_WON_HOST_REQ_RESET => l.0.push("reset".into()),
                EE_WON_HOST_FINISH => l.0.push(format!("finish {}", c.num[0])),
                _ => {}
            }
        });
    }

    fn run_cpp(throws: i32) -> Vec<String> {
        LOG.with(|l| *l.borrow_mut() = (Vec::new(), throws));
        let op = ee_won_hostop_cpp_request();
        let a = [0u8, 80, 10, 0, 0, 1];
        // SAFETY: the new op
        unsafe {
            crate::hostop::ee_won_hostop_bind(op, Some(cpp_host), std::ptr::null_mut());
            crate::hostop::ee_won_hostop_set(
                op,
                EE_WON_REQHOST_ADDR,
                0,
                std::ptr::null(),
                0,
                a.as_ptr(),
                std::ptr::null_mut(),
            );
            crate::hostop::ee_won_hostop_run(op);
            crate::hostop::ee_won_hostop_free(op);
        }
        LOG.with(|l| l.borrow().0.clone())
    }

    #[test]
    fn a_cpp_derived_op_builds_and_reads_through_its_virtuals() {
        assert_eq!(run_cpp(0), ["reset", "next", "send 0102", "check 1 09", "finish 0"]);
        // a reply its read threw on: an unpack failure, the only server failed alike
        assert_eq!(run_cpp(1), ["reset", "next", "send 0102", "check 1 09", "reset", "finish 100064"]);
    }

    #[test]
    fn a_cpp_derived_ops_server_error_is_the_exchanges() {
        let h = Host::new(ReqPlan::new(Box::new(CppReq)));
        let v = SetValue { num: 100063, data: &[], addr: [0; 6], obj: Obj::none() };
        ReqPlan::set(&h, EE_WON_REQHOST_LAST_ERROR, &v);
        let st_now = {
            let p = h.st();
            (p.ex.last_error, p.ex.use_last)
        };
        assert_eq!(st_now, (100063, true));
        let v = SetValue { num: 100076, ..v };
        ReqPlan::set(&h, EE_WON_REQHOST_LAST_ERROR, &v);
        let st_now = {
            let p = h.st();
            (p.ex.last_error, p.ex.use_last)
        };
        assert_eq!(st_now, (100076, false));
    }
}
