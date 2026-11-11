//! WONServer (`src/Empire Earth/WONAPI/WONServer`): the request/reply exchange every Internet service op runs
//! (`ServerRequestOp`: the server list's failover, the expired-session retry, the last-error rule, the blocking loop
//! and the asynchronous completions), its time limits (`ServerOp`), the ping requests and replies (`PingOp`,
//! `MultiPingOp`'s batches, failures and order), the server list's shuffle (`ServerContext`) and the persistent
//! connection's keep-alive (`ServerConnection`).
//!
//! The C++ keeps the objects: the address list and its iterator, the sockets, the sessions, the peer auth ops, the
//! tracker and the completions. The exchange runs here over [`RequestEnv`], whose methods are the C++ primitives
//! (`ee_won_server_req_*` hand them over as [`EeWonServerReqHooks`]); the state the exchange keeps (the last server
//! error, the reuse and expiry flags, the certificate status) stays in the C++ members, read and written through
//! the hooks' pointers at each step, so a subclass's `SetLastServerError` between steps sees the same values.
//!
//! Reached by nothing in the game today: every op here talks to the WON servers, which are gone.

use std::ffi::c_void;

use crate::wire::{Reader, Writer};

/// The statuses the exchange names (`WONStatus.h`).
/// cbindgen:ignore
pub mod st {
    pub const SUCCESS: i32 = 0;
    pub const WSAEMSGSIZE: i32 = 10040;
    pub const PENDING: i32 = 100000;
    pub const TIMED_OUT: i32 = 100001;
    pub const NONE: i32 = 100004;
    pub const PEER_AUTH_GET_CERT_FAILURE: i32 = 100053;
    pub const FAILED_ALL_SERVERS: i32 = 100054;
    pub const NO_SERVERS_SPECIFIED: i32 = 100055;
    pub const NEED_AUTH_CONTEXT: i32 = 100056;
    pub const TRY_NEXT_SERVER: i32 = 100057;
    pub const SEND: i32 = 100058;
    pub const RECV: i32 = 100059;
    pub const EXIT_COMMUNICATION_LOOP: i32 = 100060;
    pub const SESSION_EXPIRED: i32 = 100061;
    pub const GET_CERT_FAILURE: i32 = 100062;
    pub const INVALID_REPLY_HEADER: i32 = 100063;
    pub const UNPACK_FAILURE: i32 = 100064;
}

/// `AuthType`'s values the exchange tests.
const AUTH_TYPE_NONE: i32 = 0;
const AUTH_TYPE_SESSION: i32 = 1;

/// The C++ primitives of one `ServerRequestOp` and the state the exchange keeps in its members.
pub trait RequestEnv {
    // the op
    fn status(&mut self) -> i32;
    fn time_left(&mut self) -> u32;
    fn is_async(&mut self) -> bool;
    fn auth_type(&mut self) -> i32;
    fn finish(&mut self, status: i32);
    /// `FinishSaveSession`: finish, keep a session-type session in the auth context, remember it as used.
    fn finish_save_session(&mut self, status: i32);
    /// `Kill(true)`.
    fn kill(&mut self);
    // the members
    fn last_error(&mut self) -> i32;
    fn set_last_error(&mut self, v: i32);
    fn use_last(&mut self) -> bool;
    fn set_use_last(&mut self, v: bool);
    fn reusing(&mut self) -> bool;
    fn set_reusing(&mut self, v: bool);
    fn expired(&mut self) -> bool;
    fn set_expired(&mut self, v: bool);
    fn set_get_cert_status(&mut self, v: i32);
    // the address list
    /// `RunHook`'s start: drop the used session, copy the context's addresses; false when the list is empty.
    fn prepare_addresses(&mut self) -> bool;
    /// `mAddrItr = mAddrList.begin()`.
    fn rewind_addresses(&mut self);
    fn at_begin(&mut self) -> bool;
    fn at_end(&mut self) -> bool;
    fn has_context(&mut self) -> bool;
    fn has_auth_context(&mut self) -> bool;
    /// The context's `NotifyFailed(mCurAddr)`.
    fn notify_failed(&mut self);
    /// A new socket with the op's length field, `mCurAddr = *mAddrItr`, `++mAddrItr`.
    fn next_addr(&mut self);
    /// `--mAddrItr`.
    fn retreat_addr(&mut self);
    /// `mTracker.KillAll()`, `Reset()`, close and drop the socket.
    fn reset_server(&mut self);
    // the session
    /// `mCurSession = mAuthContext->GetSession(mCurAddr)`.
    fn load_session(&mut self);
    fn clear_session(&mut self);
    fn has_session(&mut self) -> bool;
    /// The socket's `AddMsgTransform(new CryptTransform(mCurSession))`.
    fn add_crypt(&mut self);
    // the steps
    fn queue_connect(&mut self);
    fn connect_block(&mut self) -> i32;
    /// A tracked peer auth op run asynchronously.
    fn peer_auth_async(&mut self);
    /// A peer auth op run blocking: true when it succeeded (the C++ then took its session); else its status and
    /// its certificate status.
    fn peer_auth_block(&mut self, status: &mut i32, cert: &mut i32) -> bool;
    fn get_next_request(&mut self) -> i32;
    fn queue_send(&mut self);
    fn send_block(&mut self) -> i32;
    fn queue_recv(&mut self);
    fn recv_block(&mut self) -> i32;
    /// `mResponse`'s bytes.
    fn response(&mut self) -> Vec<u8>;
    /// `CheckResponse()`; `None` when it threw `ReadBufferException`.
    fn check_response(&mut self) -> Option<i32>;
    /// The completed peer auth op's session into `mCurSession`.
    fn take_op_session(&mut self);
    /// The completed receive op's message into `mResponse`.
    fn take_op_response(&mut self);
}

/// `SetLastServerError`: the final status is the last server error only while every server failed the same way.
pub fn set_last_server_error<E: RequestEnv + ?Sized>(e: &mut E, error: i32) {
    let last = e.last_error();
    if last == st::NONE {
        e.set_use_last(true);
    } else if error != last {
        e.set_use_last(false);
    }
    e.set_last_error(error);
}

/// `SetLastServerError` on plain values: (last error, use it for the final status).
pub fn last_server_error(last: i32, use_last: bool, error: i32) -> (i32, bool) {
    let use_last = if last == st::NONE {
        true
    } else if error != last {
        false
    } else {
        use_last
    };
    (error, use_last)
}

/// `CheckExpiredSession`'s reply test: a reused session's reply of at least 10 bytes starting with service 1,
/// message 15 (the server's "session expired").
pub fn is_expired_reply(reply: &[u8]) -> bool {
    if reply.len() < 10 {
        return false;
    }
    let mut r = Reader::new(reply);
    match (r.read_long(), r.read_long()) {
        (Ok(service), Ok(message)) => service == 1 && message == 15,
        _ => false,
    }
}

/// `CheckExpiredSession`: true (and the same server again) when a reused session was rejected as expired.
pub fn check_expired_session<E: RequestEnv + ?Sized>(e: &mut E) -> bool {
    if !e.reusing() {
        return false;
    }
    if !is_expired_reply(&e.response()) {
        return false;
    }
    e.set_expired(true);
    e.retreat_addr();
    true
}

/// `TryConnect`: a cached session encrypts the socket; asynchronous: queue the connect and the first request.
pub fn try_connect<E: RequestEnv + ?Sized>(e: &mut E) -> bool {
    if e.has_session() {
        e.add_crypt();
        e.set_reusing(true);
    }
    if e.is_async() {
        e.queue_connect();
        send(e);
        return true;
    }
    let status = e.connect_block();
    if status == st::SUCCESS {
        return true;
    }
    set_last_server_error(e, status);
    false
}

/// `TryPeerToPeerAuth`: true when started (asynchronous) or done; a certificate failure finishes the op.
pub fn try_peer_auth<E: RequestEnv + ?Sized>(e: &mut E) -> bool {
    if e.is_async() {
        e.peer_auth_async();
        return true;
    }
    let (mut status, mut cert) = (0, 0);
    if e.peer_auth_block(&mut status, &mut cert) {
        return true;
    }
    if status == st::PEER_AUTH_GET_CERT_FAILURE {
        e.set_get_cert_status(cert);
        e.finish(st::GET_CERT_FAILURE);
    } else {
        set_last_server_error(e, status);
    }
    false
}

/// `TryNextServer`: the next address, connected or authenticated; finishes when none is left (the last server
/// error when every server failed alike, else `FailedAllServers`) or the time ran out.
pub fn try_next_server<E: RequestEnv + ?Sized>(e: &mut E) -> bool {
    if e.status() != st::PENDING {
        return false;
    }
    loop {
        e.reset_server();
        if !e.at_begin() && !e.expired() && e.has_context() {
            e.notify_failed();
        }
        if e.at_end() {
            if e.use_last() {
                let last = e.last_error();
                e.finish(last);
            } else {
                e.finish(st::FAILED_ALL_SERVERS);
            }
            return false;
        }
        e.next_addr();
        let was_reusing = e.reusing();
        e.set_reusing(false);
        e.set_expired(false);
        if !was_reusing && e.auth_type() == AUTH_TYPE_SESSION {
            e.load_session();
        } else {
            e.clear_session();
        }
        if e.has_session() || e.auth_type() == AUTH_TYPE_NONE {
            if try_connect(e) {
                return true;
            }
        } else {
            if try_peer_auth(e) {
                return true;
            }
            if e.status() != st::PENDING {
                return false;
            }
        }
        if e.time_left() == 0 {
            e.finish(st::TIMED_OUT);
            return false;
        }
    }
}

/// `Send`: the requests `GetNextRequest` builds until one wants a reply; returns the next step.
pub fn send<E: RequestEnv + ?Sized>(e: &mut E) -> i32 {
    loop {
        let request = e.get_next_request();
        if request != st::SEND && request != st::RECV {
            return request;
        }
        if e.is_async() {
            e.queue_send();
        } else {
            let status = e.send_block();
            if status != st::SUCCESS {
                set_last_server_error(e, status);
                return st::TRY_NEXT_SERVER;
            }
        }
        if request != st::RECV {
            continue;
        }
        if e.is_async() {
            e.queue_recv();
        }
        return request;
    }
}

/// `Recv` (blocking): replies checked until the exchange moves on; returns the next step.
pub fn recv<E: RequestEnv + ?Sized>(e: &mut E) -> i32 {
    loop {
        let status = e.recv_block();
        if status != st::SUCCESS {
            set_last_server_error(e, status);
            return st::TRY_NEXT_SERVER;
        }
        if check_expired_session(e) {
            return st::SESSION_EXPIRED;
        }
        match e.check_response() {
            None => {
                set_last_server_error(e, st::UNPACK_FAILURE);
                return st::TRY_NEXT_SERVER;
            }
            Some(status) => {
                if status != st::RECV {
                    return status;
                }
            }
        }
    }
}

/// `RunHook`: asynchronous, the first server; blocking, each server until one completes.
pub fn run_hook<E: RequestEnv + ?Sized>(e: &mut E) {
    e.set_last_error(st::NONE);
    e.set_get_cert_status(st::NONE);
    e.set_use_last(false);
    if !e.prepare_addresses() {
        e.finish(st::NO_SERVERS_SPECIFIED);
        return;
    }
    e.rewind_addresses();
    e.set_reusing(false);
    e.set_expired(false);
    if e.auth_type() != AUTH_TYPE_NONE && !e.has_auth_context() {
        e.finish(st::NEED_AUTH_CONTEXT);
        return;
    }
    if e.is_async() {
        try_next_server(e);
        return;
    }
    while try_next_server(e) {
        loop {
            let mut status = send(e);
            while status == st::SEND {
                status = send(e);
            }
            if status == st::TRY_NEXT_SERVER {
                break;
            }
            status = recv(e);
            while status == st::RECV {
                status = recv(e);
            }
            if status == st::TRY_NEXT_SERVER || status == st::SESSION_EXPIRED {
                break;
            }
            if status == st::SEND {
                continue;
            }
            e.finish_save_session(status);
            return;
        }
        if e.time_left() == 0 {
            e.finish(st::TIMED_OUT);
            return;
        }
    }
}

/// A completed op the exchange tracks (`CallbackHook`'s argument): track id 2 the peer auth, 1 a socket op.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonServerReqDone {
    pub killed: bool,
    pub status: i32,
    pub param: i32,
    /// the peer auth op's certificate status (param 2)
    pub cert_status: i32,
    /// a receive op (`dynamic_cast<RecvMsgOp*>`)
    pub is_recv: bool,
}

/// `CallbackHook`: the asynchronous exchange on its peer auth and socket completions; false for unknown ops.
pub fn callback<E: RequestEnv + ?Sized>(e: &mut E, done: &EeWonServerReqDone) -> bool {
    if done.killed {
        e.kill();
        return true;
    }
    if done.status != st::SUCCESS {
        if done.param == 2 && done.status == st::PEER_AUTH_GET_CERT_FAILURE {
            e.set_get_cert_status(done.cert_status);
            e.finish(st::GET_CERT_FAILURE);
            return true;
        }
        set_last_server_error(e, done.status);
        try_next_server(e);
        return true;
    }
    match done.param {
        2 => {
            e.take_op_session();
            send(e);
        }
        1 => {
            if done.is_recv {
                e.take_op_response();
                if check_expired_session(e) {
                    try_next_server(e);
                    return true;
                }
                match e.check_response() {
                    None => {
                        set_last_server_error(e, st::UNPACK_FAILURE);
                        try_next_server(e);
                    }
                    Some(status) => {
                        if status == st::TRY_NEXT_SERVER {
                            try_next_server(e);
                        } else if status == st::SEND {
                            send(e);
                        } else if status == st::RECV {
                            e.queue_recv();
                        } else if status != st::EXIT_COMMUNICATION_LOOP {
                            e.finish_save_session(status);
                        }
                    }
                }
            }
        }
        _ => return false,
    }
    true
}

/// `CleanupHook`'s decisions: bit 1 the context marks the server failed (timed out or killed), bit 2 the socket
/// closes (close after use, a failure or a kill).
pub fn cleanup_plan(status: i32, killed: bool, has_context: bool, has_socket: bool, close_connection: bool) -> u32 {
    let mut plan = 0;
    if status != st::SUCCESS && (status == st::TIMED_OUT || killed) && has_context {
        plan |= 1;
    }
    if has_socket && (close_connection || status != st::SUCCESS || killed) {
        plan |= 2;
    }
    plan
}

/// `ServerOp::ConnectTime` / `SendTime` / `RecvTime`: the step's limit, capped by the time left when blocking.
pub fn step_time(max: u32, is_async: bool, time_left: u32) -> u32 {
    if !is_async && time_left < max {
        time_left
    } else {
        max
    }
}

// ---- the ping requests and replies

/// `PingOp::GetNextRequest`: a mini message (3), the common service (1), the ping (5), the start tick, the
/// extended flag, behind the op's length field.
pub fn ping_request(field_size: u8, start_tick: u32, extended: bool) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    w.append_byte(3);
    w.append_byte(1);
    w.append_byte(5);
    w.append_long(start_tick as i32);
    w.append_bool(extended);
    w.to_message()
}

/// `MultiPingOp::GetRequest`: the ping with its id and no extended flag, no length field (a datagram).
pub fn multi_ping_request(ping_id: i32) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_byte(3);
    w.append_byte(1);
    w.append_byte(5);
    w.append_long(ping_id);
    w.append_bool(false);
    w.to_message()
}

/// `ServerConnection::SendNoOp`: the common service's no-op (3, 1, 7).
pub fn no_op(field_size: u8) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    w.append_byte(3);
    w.append_byte(1);
    w.append_byte(7);
    w.to_message()
}

/// `ServerConnection::StartKeepAlive`'s timer: the period in seconds as milliseconds (32-bit), none at 0.
pub fn keep_alive_ms(period_seconds: u32) -> Option<u32> {
    if period_seconds == 0 {
        None
    } else {
        Some(period_seconds.wrapping_mul(1000))
    }
}

/// `PingOp::CheckResponse` as read: `fields` is how many of lag, app name, logical name, version, image, pid,
/// ports, auth flag, registration data, server data were read (in that order; the C++ assigns those). The six
/// strings (app name, logical name, image, ports, registration data, server data) are `(offset, length)` in the
/// reply.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct PingReply {
    /// 0 read whole, 1 not a ping reply (`InvalidReplyHeader`), 2 cut short (`ReadBufferException`)
    pub end: i32,
    pub fields: u32,
    pub lag: u32,
    pub version: i32,
    pub pid: i32,
    pub is_auth: bool,
    pub text: [(usize, usize); 6],
}

impl PingReply {
    /// String `k` of the six (app name ... server data).
    pub fn text<'a>(&self, reply: &'a [u8], k: usize) -> &'a [u8] {
        let (at, n) = self.text[k];
        &reply[at..at + n]
    }
}

/// `PingOp::CheckResponse`: the header 3, 1, 6, the lag from the echoed tick, and the extended fields when more
/// bytes follow (the registration and server data when more follow still).
pub fn ping_reply(reply: &[u8], now: u32) -> PingReply {
    let mut out = PingReply::default();
    let mut r = Reader::new(reply);
    let res = (|| {
        let h = r.read_byte()?;
        let s = r.read_byte()?;
        let m = r.read_byte()?;
        if h != 3 || s != 1 || m != 6 {
            out.end = 1;
            return Ok(());
        }
        out.lag = now.wrapping_sub(r.read_long()? as u32);
        out.fields = 1;
        let string = |r: &mut Reader, out: &mut PingReply, k: usize| {
            let n = r.read_length(2)? as usize;
            let at = r.pos();
            r.read_bytes(n)?;
            out.text[k] = (at, n);
            out.fields += 1;
            Ok::<(), crate::wire::PastEnd>(())
        };
        if r.available() > 0 {
            string(&mut r, &mut out, 0)?;
            string(&mut r, &mut out, 1)?;
            out.version = r.read_long()?;
            out.fields = 4;
            string(&mut r, &mut out, 2)?;
            out.pid = r.read_long()?;
            out.fields = 6;
            string(&mut r, &mut out, 3)?;
            out.is_auth = r.read_bool()?;
            out.fields = 8;
            if r.available() > 0 {
                string(&mut r, &mut out, 4)?;
                string(&mut r, &mut out, 5)?;
            }
        }
        Ok::<(), crate::wire::PastEnd>(())
    })();
    if res.is_err() {
        out.end = 2;
    }
    out
}

/// `MultiPingOp::HandleResponse`: done when the reply is a ping reply (3, 1, 6) echoing the id; else (or cut
/// short) an invalid reply.
pub fn multi_ping_reply(reply: &[u8], ping_id: i32) -> bool {
    let mut r = Reader::new(reply);
    let read = (|| {
        let h = r.read_byte()?;
        let s = r.read_byte()?;
        let m = r.read_byte()?;
        if h != 3 || s != 1 || m != 6 {
            return Ok(false);
        }
        Ok::<bool, crate::wire::PastEnd>(r.read_long()? == ping_id)
    })();
    read.unwrap_or(false)
}

/// `MultiPingOp::HandleResponsePrv`'s status rule: 0 stop (the op finishes or the batch fails with it), 1 an
/// oversized datagram (ignored), 2 a reply to match.
pub fn multi_ping_recv_status(status: i32) -> i32 {
    if status != st::SUCCESS && status != st::WSAEMSGSIZE {
        0
    } else if status == st::SUCCESS {
        2
    } else {
        1
    }
}

/// `MultiPingOp::RecvCallback`'s next step once a datagram was handled: 0 wait (pings in flight), 1 finish
/// (nothing left), 2 the next batch.
pub fn multi_ping_next(in_flight: usize, waiting: usize) -> i32 {
    if in_flight != 0 {
        0
    } else if waiting == 0 {
        1
    } else {
        2
    }
}

/// `MultiPingOp::FinishBatch`: an unanswered server's failures (after this one) end its pings at the maximum.
pub fn multi_ping_gives_up(failures: i32, max_failures: u8) -> bool {
    failures >= i32::from(max_failures)
}

/// `MultiPingOp::ResponseTimeLeft`: the batch's wait left, capped by the op's time left (32-bit tick wrap).
pub fn multi_ping_response_time_left(now: u32, start: u32, max_ping: u32, time_left: u32) -> u32 {
    let elapsed = now.wrapping_sub(start);
    if elapsed > max_ping {
        return 0;
    }
    let left = max_ping - elapsed;
    if left < time_left {
        left
    } else {
        time_left
    }
}

/// `MultiPingOp::RunHook`'s blocking loop: a batch's status other than success or a time-out finishes the op.
pub fn multi_ping_batch_continues(status: i32) -> bool {
    status == st::SUCCESS || status == st::TIMED_OUT
}

/// `MultiPingOp::CleanupHook`'s order: by ping time as an unsigned tick count (no reply, -1, last), equal times
/// in list order (the multimap's insertion order).
pub fn multi_ping_order(ping_times: &[i32]) -> Vec<usize> {
    let mut order: Vec<usize> = (0..ping_times.len()).collect();
    order.sort_by_key(|&i| ping_times[i] as u32);
    order
}

/// `MultiPingOp::GetServerContext`: a server joins the context when it answered or the unanswered are kept.
pub fn multi_ping_keeps(ping_time: i32, discard_unanswered: bool) -> bool {
    ping_time != -1 || !discard_unanswered
}

/// `ServerContext::CopyAddresses`' one-time shuffle (the original's `random_shuffle` over `rand()`): the new
/// order of `n` addresses, `draw` giving each `rand()`.
pub fn shuffle_order(n: usize, mut draw: impl FnMut() -> i32) -> Vec<usize> {
    let mut v: Vec<usize> = (0..n).collect();
    for i in 1..n {
        let j = (draw() as u32 as usize) % (i + 1);
        v.swap(i, j);
    }
    v
}

// ---- the C ABI

/// The C++ primitives of one `ServerRequestOp` (`ctx` is the C++ glue's) and pointers to the members the exchange
/// keeps. Each hook is called with `ctx`.
#[repr(C)]
pub struct EeWonServerReqHooks {
    pub ctx: *mut c_void,
    pub last_error: *mut i32,
    pub get_cert_status: *mut i32,
    pub use_last: *mut bool,
    pub reusing: *mut bool,
    pub expired: *mut bool,
    pub status: unsafe extern "C" fn(*mut c_void) -> i32,
    pub time_left: unsafe extern "C" fn(*mut c_void) -> u32,
    pub is_async: unsafe extern "C" fn(*mut c_void) -> bool,
    pub auth_type: unsafe extern "C" fn(*mut c_void) -> i32,
    pub finish: unsafe extern "C" fn(*mut c_void, i32),
    pub finish_save_session: unsafe extern "C" fn(*mut c_void, i32),
    pub kill: unsafe extern "C" fn(*mut c_void),
    pub prepare_addresses: unsafe extern "C" fn(*mut c_void) -> bool,
    pub rewind_addresses: unsafe extern "C" fn(*mut c_void),
    pub at_begin: unsafe extern "C" fn(*mut c_void) -> bool,
    pub at_end: unsafe extern "C" fn(*mut c_void) -> bool,
    pub has_context: unsafe extern "C" fn(*mut c_void) -> bool,
    pub has_auth_context: unsafe extern "C" fn(*mut c_void) -> bool,
    pub notify_failed: unsafe extern "C" fn(*mut c_void),
    pub next_addr: unsafe extern "C" fn(*mut c_void),
    pub retreat_addr: unsafe extern "C" fn(*mut c_void),
    pub reset_server: unsafe extern "C" fn(*mut c_void),
    pub load_session: unsafe extern "C" fn(*mut c_void),
    pub clear_session: unsafe extern "C" fn(*mut c_void),
    pub has_session: unsafe extern "C" fn(*mut c_void) -> bool,
    pub add_crypt: unsafe extern "C" fn(*mut c_void),
    pub queue_connect: unsafe extern "C" fn(*mut c_void),
    pub connect_block: unsafe extern "C" fn(*mut c_void) -> i32,
    pub peer_auth_async: unsafe extern "C" fn(*mut c_void),
    pub peer_auth_block: unsafe extern "C" fn(*mut c_void, *mut i32, *mut i32) -> bool,
    pub get_next_request: unsafe extern "C" fn(*mut c_void) -> i32,
    pub queue_send: unsafe extern "C" fn(*mut c_void),
    pub send_block: unsafe extern "C" fn(*mut c_void) -> i32,
    pub queue_recv: unsafe extern "C" fn(*mut c_void),
    pub recv_block: unsafe extern "C" fn(*mut c_void) -> i32,
    /// `mResponse`'s bytes (NULL and 0: none)
    pub response: unsafe extern "C" fn(*mut c_void, *mut *const u8, *mut usize),
    /// `CheckResponse()`; `*threw` set when it threw `ReadBufferException`
    pub check_response: unsafe extern "C" fn(*mut c_void, *mut bool) -> i32,
    pub take_op_session: unsafe extern "C" fn(*mut c_void),
    pub take_op_response: unsafe extern "C" fn(*mut c_void),
}

struct Hooked<'a>(&'a EeWonServerReqHooks);

// SAFETY (every method): the C++ hands valid function pointers, a context they accept and member pointers that
// stay valid for the call (`ee_won_server_req_*`'s contract); no Rust reference is held across a hook.
impl RequestEnv for Hooked<'_> {
    fn status(&mut self) -> i32 {
        unsafe { (self.0.status)(self.0.ctx) }
    }
    fn time_left(&mut self) -> u32 {
        unsafe { (self.0.time_left)(self.0.ctx) }
    }
    fn is_async(&mut self) -> bool {
        unsafe { (self.0.is_async)(self.0.ctx) }
    }
    fn auth_type(&mut self) -> i32 {
        unsafe { (self.0.auth_type)(self.0.ctx) }
    }
    fn finish(&mut self, status: i32) {
        unsafe { (self.0.finish)(self.0.ctx, status) }
    }
    fn finish_save_session(&mut self, status: i32) {
        unsafe { (self.0.finish_save_session)(self.0.ctx, status) }
    }
    fn kill(&mut self) {
        unsafe { (self.0.kill)(self.0.ctx) }
    }
    fn last_error(&mut self) -> i32 {
        unsafe { self.0.last_error.read() }
    }
    fn set_last_error(&mut self, v: i32) {
        unsafe { self.0.last_error.write(v) }
    }
    fn use_last(&mut self) -> bool {
        unsafe { self.0.use_last.read() }
    }
    fn set_use_last(&mut self, v: bool) {
        unsafe { self.0.use_last.write(v) }
    }
    fn reusing(&mut self) -> bool {
        unsafe { self.0.reusing.read() }
    }
    fn set_reusing(&mut self, v: bool) {
        unsafe { self.0.reusing.write(v) }
    }
    fn expired(&mut self) -> bool {
        unsafe { self.0.expired.read() }
    }
    fn set_expired(&mut self, v: bool) {
        unsafe { self.0.expired.write(v) }
    }
    fn set_get_cert_status(&mut self, v: i32) {
        unsafe { self.0.get_cert_status.write(v) }
    }
    fn prepare_addresses(&mut self) -> bool {
        unsafe { (self.0.prepare_addresses)(self.0.ctx) }
    }
    fn rewind_addresses(&mut self) {
        unsafe { (self.0.rewind_addresses)(self.0.ctx) }
    }
    fn at_begin(&mut self) -> bool {
        unsafe { (self.0.at_begin)(self.0.ctx) }
    }
    fn at_end(&mut self) -> bool {
        unsafe { (self.0.at_end)(self.0.ctx) }
    }
    fn has_context(&mut self) -> bool {
        unsafe { (self.0.has_context)(self.0.ctx) }
    }
    fn has_auth_context(&mut self) -> bool {
        unsafe { (self.0.has_auth_context)(self.0.ctx) }
    }
    fn notify_failed(&mut self) {
        unsafe { (self.0.notify_failed)(self.0.ctx) }
    }
    fn next_addr(&mut self) {
        unsafe { (self.0.next_addr)(self.0.ctx) }
    }
    fn retreat_addr(&mut self) {
        unsafe { (self.0.retreat_addr)(self.0.ctx) }
    }
    fn reset_server(&mut self) {
        unsafe { (self.0.reset_server)(self.0.ctx) }
    }
    fn load_session(&mut self) {
        unsafe { (self.0.load_session)(self.0.ctx) }
    }
    fn clear_session(&mut self) {
        unsafe { (self.0.clear_session)(self.0.ctx) }
    }
    fn has_session(&mut self) -> bool {
        unsafe { (self.0.has_session)(self.0.ctx) }
    }
    fn add_crypt(&mut self) {
        unsafe { (self.0.add_crypt)(self.0.ctx) }
    }
    fn queue_connect(&mut self) {
        unsafe { (self.0.queue_connect)(self.0.ctx) }
    }
    fn connect_block(&mut self) -> i32 {
        unsafe { (self.0.connect_block)(self.0.ctx) }
    }
    fn peer_auth_async(&mut self) {
        unsafe { (self.0.peer_auth_async)(self.0.ctx) }
    }
    fn peer_auth_block(&mut self, status: &mut i32, cert: &mut i32) -> bool {
        let (mut s, mut c) = (0, 0);
        let ok = unsafe { (self.0.peer_auth_block)(self.0.ctx, &mut s, &mut c) };
        *status = s;
        *cert = c;
        ok
    }
    fn get_next_request(&mut self) -> i32 {
        unsafe { (self.0.get_next_request)(self.0.ctx) }
    }
    fn queue_send(&mut self) {
        unsafe { (self.0.queue_send)(self.0.ctx) }
    }
    fn send_block(&mut self) -> i32 {
        unsafe { (self.0.send_block)(self.0.ctx) }
    }
    fn queue_recv(&mut self) {
        unsafe { (self.0.queue_recv)(self.0.ctx) }
    }
    fn recv_block(&mut self) -> i32 {
        unsafe { (self.0.recv_block)(self.0.ctx) }
    }
    fn response(&mut self) -> Vec<u8> {
        let mut data: *const u8 = std::ptr::null();
        let mut len = 0usize;
        unsafe { (self.0.response)(self.0.ctx, &mut data, &mut len) };
        if data.is_null() || len == 0 {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(data, len) }.to_vec()
        }
    }
    fn check_response(&mut self) -> Option<i32> {
        let mut threw = false;
        let status = unsafe { (self.0.check_response)(self.0.ctx, &mut threw) };
        if threw {
            None
        } else {
            Some(status)
        }
    }
    fn take_op_session(&mut self) {
        unsafe { (self.0.take_op_session)(self.0.ctx) }
    }
    fn take_op_response(&mut self) {
        unsafe { (self.0.take_op_response)(self.0.ctx) }
    }
}

/// `ServerRequestOp::RunHook`.
///
/// # Safety
/// `hooks` points to valid hooks whose member pointers stay valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_run(hooks: *const EeWonServerReqHooks) {
    run_hook(&mut Hooked(unsafe { &*hooks }))
}

/// `ServerRequestOp::TryNextServer`.
///
/// # Safety
/// As [`ee_won_server_req_run`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_try_next(hooks: *const EeWonServerReqHooks) -> bool {
    try_next_server(&mut Hooked(unsafe { &*hooks }))
}

/// `ServerRequestOp::TryConnect`.
///
/// # Safety
/// As [`ee_won_server_req_run`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_try_connect(hooks: *const EeWonServerReqHooks) -> bool {
    try_connect(&mut Hooked(unsafe { &*hooks }))
}

/// `ServerRequestOp::TryPeerToPeerAuth`.
///
/// # Safety
/// As [`ee_won_server_req_run`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_try_peer_auth(hooks: *const EeWonServerReqHooks) -> bool {
    try_peer_auth(&mut Hooked(unsafe { &*hooks }))
}

/// `ServerRequestOp::Send`.
///
/// # Safety
/// As [`ee_won_server_req_run`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_send(hooks: *const EeWonServerReqHooks) -> i32 {
    send(&mut Hooked(unsafe { &*hooks }))
}

/// `ServerRequestOp::Recv`.
///
/// # Safety
/// As [`ee_won_server_req_run`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_recv(hooks: *const EeWonServerReqHooks) -> i32 {
    recv(&mut Hooked(unsafe { &*hooks }))
}

/// `ServerRequestOp::CheckExpiredSession`.
///
/// # Safety
/// As [`ee_won_server_req_run`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_check_expired(hooks: *const EeWonServerReqHooks) -> bool {
    check_expired_session(&mut Hooked(unsafe { &*hooks }))
}

/// `ServerRequestOp::CallbackHook`.
///
/// # Safety
/// As [`ee_won_server_req_run`]; `done` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_callback(
    hooks: *const EeWonServerReqHooks,
    done: *const EeWonServerReqDone,
) -> bool {
    callback(&mut Hooked(unsafe { &*hooks }), unsafe { &*done })
}

/// `ServerRequestOp::SetLastServerError` on the members.
///
/// # Safety
/// The pointers are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_req_last_error(last: *mut i32, use_last: *mut bool, error: i32) {
    let (l, u) = unsafe { last_server_error(*last, *use_last, error) };
    unsafe {
        *last = l;
        *use_last = u;
    }
}

/// `ServerRequestOp::CleanupHook`'s plan (bit 1 notify the context, bit 2 close the socket).
#[no_mangle]
pub extern "C" fn ee_won_server_req_cleanup_plan(
    status: i32,
    killed: bool,
    has_context: bool,
    has_socket: bool,
    close_connection: bool,
) -> u32 {
    cleanup_plan(status, killed, has_context, has_socket, close_connection)
}

/// `ServerOp`'s step limits.
#[no_mangle]
pub extern "C" fn ee_won_server_step_time(max: u32, is_async: bool, time_left: u32) -> u32 {
    step_time(max, is_async, time_left)
}

use crate::ffi::EeWonByteSink;

/// `PingOp::GetNextRequest`'s message into `sink`.
///
/// # Safety
/// `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_ping_request(
    field_size: u8,
    start_tick: u32,
    extended: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let m = ping_request(field_size, start_tick, extended);
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// `MultiPingOp::GetRequest`'s datagram into `sink`.
///
/// # Safety
/// As [`ee_won_server_ping_request`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_multi_ping_request(ping_id: i32, sink: EeWonByteSink, context: *mut c_void) {
    let m = multi_ping_request(ping_id);
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// `ServerConnection::SendNoOp`'s message into `sink`.
///
/// # Safety
/// As [`ee_won_server_ping_request`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_no_op(field_size: u8, sink: EeWonByteSink, context: *mut c_void) {
    let m = no_op(field_size);
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// `ServerConnection::StartKeepAlive`'s timer in milliseconds (`*ms`); false: no timer.
///
/// # Safety
/// `ms` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_keep_alive_ms(period_seconds: u32, ms: *mut u32) -> bool {
    match keep_alive_ms(period_seconds) {
        Some(v) => {
            unsafe { *ms = v };
            true
        }
        None => false,
    }
}

/// A ping reply's fields for the C++ (`PingOp::CheckResponse`).
#[repr(C)]
pub struct EeWonPingReply {
    /// 0 read whole, 1 not a ping reply, 2 cut short
    pub end: i32,
    /// how many fields were read, in order: lag, app name, logical name, version, image, pid, ports, auth flag,
    /// registration data, server data
    pub fields: u32,
    pub lag: u32,
    pub version: i32,
    pub pid: i32,
    pub is_auth: bool,
    /// app name, logical name, image, ports, registration data, server data: offsets and lengths in the reply
    pub text_at: [u32; 6],
    pub text_len: [u32; 6],
}

/// `PingOp::CheckResponse`'s read.
///
/// # Safety
/// `reply` holds `length` bytes; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_ping_reply(reply: *const u8, length: usize, now: u32, out: *mut EeWonPingReply) {
    let bytes: &[u8] =
        if reply.is_null() || length == 0 { &[] } else { unsafe { std::slice::from_raw_parts(reply, length) } };
    let p = ping_reply(bytes, now);
    unsafe {
        *out = EeWonPingReply {
            end: p.end,
            fields: p.fields,
            lag: p.lag,
            version: p.version,
            pid: p.pid,
            is_auth: p.is_auth,
            text_at: p.text.map(|t| t.0 as u32),
            text_len: p.text.map(|t| t.1 as u32),
        }
    }
}

/// `MultiPingOp::HandleResponse`: true when done (a ping reply with the id).
///
/// # Safety
/// `reply` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_multi_ping_reply(reply: *const u8, length: usize, ping_id: i32) -> bool {
    let bytes: &[u8] =
        if reply.is_null() || length == 0 { &[] } else { unsafe { std::slice::from_raw_parts(reply, length) } };
    multi_ping_reply(bytes, ping_id)
}

/// `MultiPingOp::HandleResponsePrv`'s status rule (0 stop, 1 ignore, 2 match the reply).
#[no_mangle]
pub extern "C" fn ee_won_server_multi_ping_recv_status(status: i32) -> i32 {
    multi_ping_recv_status(status)
}

/// `MultiPingOp::RecvCallback`'s next step (0 wait, 1 finish, 2 next batch).
#[no_mangle]
pub extern "C" fn ee_won_server_multi_ping_next(in_flight: usize, waiting: usize) -> i32 {
    multi_ping_next(in_flight, waiting)
}

/// `MultiPingOp::FinishBatch`'s give-up rule.
#[no_mangle]
pub extern "C" fn ee_won_server_multi_ping_gives_up(failures: i32, max_failures: u8) -> bool {
    multi_ping_gives_up(failures, max_failures)
}

/// `MultiPingOp::ResponseTimeLeft`.
#[no_mangle]
pub extern "C" fn ee_won_server_multi_ping_time_left(now: u32, start: u32, max_ping: u32, time_left: u32) -> u32 {
    multi_ping_response_time_left(now, start, max_ping, time_left)
}

/// `MultiPingOp::RunHook`'s blocking loop rule.
#[no_mangle]
pub extern "C" fn ee_won_server_multi_ping_continues(status: i32) -> bool {
    multi_ping_batch_continues(status)
}

/// `MultiPingOp::CleanupHook`'s order: `order` receives `count` indexes.
///
/// # Safety
/// `ping_times` and `order` hold `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_multi_ping_order(ping_times: *const i32, count: usize, order: *mut usize) {
    if count == 0 {
        return;
    }
    let t = unsafe { std::slice::from_raw_parts(ping_times, count) };
    let o = multi_ping_order(t);
    unsafe { std::slice::from_raw_parts_mut(order, count) }.copy_from_slice(&o);
}

/// `MultiPingOp::GetServerContext`'s rule.
#[no_mangle]
pub extern "C" fn ee_won_server_multi_ping_keeps(ping_time: i32, discard_unanswered: bool) -> bool {
    multi_ping_keeps(ping_time, discard_unanswered)
}

/// `rand()` for [`ee_won_server_shuffle`].
pub type EeWonDraw = unsafe extern "C" fn() -> i32;

/// `ServerContext::CopyAddresses`' shuffle: `order` receives `count` indexes, `draw` is `rand`.
///
/// # Safety
/// `order` holds `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_won_server_shuffle(count: usize, draw: EeWonDraw, order: *mut usize) {
    if count == 0 {
        return;
    }
    let o = shuffle_order(count, || unsafe { draw() });
    unsafe { std::slice::from_raw_parts_mut(order, count) }.copy_from_slice(&o);
}

#[cfg(test)]
mod tests {
    use super::*;

    const RESPONSES: [&[u8]; 6] = [
        b"",
        b"\x01\0\0\0\x0f\0\0\0\0\0",
        b"\x01\0\0\0\x0e\0\0\0\0\0",
        b"\x01\0\0\0\x0f\0\0\0\0",
        b"\x02\0\0\0\x0f\0\0\0\0\0\0\0",
        b"\x01\0\0\0\x0f\0\0\0\xff\xff\xff\xff",
    ];
    const THROW: i32 = 0x7fff_ffff;

    /// One golden scenario (tests/won/won_server_test.cpp's `Describe`).
    #[derive(Default)]
    struct Scenario {
        addrs: i32,
        is_async: bool,
        auth_type: i32,
        has_context: bool,
        has_auth_context: bool,
        last: i32,
        use_last: bool,
        reusing: bool,
        expired: bool,
        lists: std::collections::BTreeMap<char, Vec<i64>>,
        done: Vec<EeWonServerReqDone>,
    }

    fn parse(line: &str) -> Scenario {
        let mut s = Scenario::default();
        for tok in line.split(' ') {
            let (k, v) = tok.split_at(1);
            let k = k.chars().next().unwrap();
            let n = |v: &str| v.parse::<i64>().unwrap();
            match k {
                'n' => s.addrs = n(v) as i32,
                'a' => s.is_async = n(v) != 0,
                't' => s.auth_type = n(v) as i32,
                'c' => s.has_context = n(v) != 0,
                'x' => s.has_auth_context = n(v) != 0,
                'l' => s.last = n(v) as i32,
                'u' => s.use_last = n(v) != 0,
                'r' => s.reusing = n(v) != 0,
                'e' => s.expired = n(v) != 0,
                'd' => {
                    let f: Vec<i64> = v.split(':').map(n).collect();
                    s.done.push(EeWonServerReqDone {
                        killed: f[0] != 0,
                        status: f[1] as i32,
                        param: f[2] as i32,
                        cert_status: f[3] as i32,
                        is_recv: f[4] != 0,
                    });
                }
                _ => {
                    let list = if v.is_empty() { Vec::new() } else { v.split(',').map(n).collect() };
                    s.lists.insert(k, list);
                }
            }
        }
        s
    }

    /// The driver's `Env`, step for step.
    struct Fake<'a> {
        s: &'a Scenario,
        at: std::collections::BTreeMap<char, usize>,
        status: i32,
        idx: i32,
        session: bool,
        response: usize,
        calls: i32,
        last: i32,
        cert: i32,
        use_last: bool,
        reusing: bool,
        expired: bool,
        log: Vec<String>,
    }

    impl<'a> Fake<'a> {
        fn new(s: &'a Scenario) -> Fake<'a> {
            Fake {
                s,
                at: Default::default(),
                status: st::PENDING,
                idx: 0,
                session: false,
                response: 0,
                calls: 0,
                last: s.last,
                cert: 0,
                use_last: s.use_last,
                reusing: s.reusing,
                expired: s.expired,
                log: Vec::new(),
            }
        }
        fn log(&mut self, t: String) {
            self.log.push(t);
            self.calls += 1;
        }
        fn over(&self) -> bool {
            self.calls > 300
        }
        fn pop(&mut self, k: char, default: i64) -> i64 {
            let i = self.at.entry(k).or_insert(0);
            match self.s.lists.get(&k).and_then(|l| l.get(*i)) {
                Some(&v) => {
                    *i += 1;
                    v
                }
                None => default,
            }
        }
        fn state(&self) -> String {
            format!(
                "= st{} l{} u{} r{} e{} k{}",
                self.status,
                self.last,
                u8::from(self.use_last),
                u8::from(self.reusing),
                u8::from(self.expired),
                self.cert
            )
        }
    }

    impl RequestEnv for Fake<'_> {
        fn status(&mut self) -> i32 {
            self.status
        }
        fn time_left(&mut self) -> u32 {
            let v = if self.over() { 0 } else { self.pop('T', 0) as u32 };
            self.log(format!("t{v}"));
            v
        }
        fn is_async(&mut self) -> bool {
            self.s.is_async
        }
        fn auth_type(&mut self) -> i32 {
            self.s.auth_type
        }
        fn finish(&mut self, status: i32) {
            self.log(format!("F{status}"));
            self.status = status;
        }
        fn finish_save_session(&mut self, status: i32) {
            self.log(format!("V{status}"));
            self.status = status;
        }
        fn kill(&mut self) {
            self.log("K".into());
            self.status = 100002;
        }
        fn last_error(&mut self) -> i32 {
            self.last
        }
        fn set_last_error(&mut self, v: i32) {
            self.last = v;
        }
        fn use_last(&mut self) -> bool {
            self.use_last
        }
        fn set_use_last(&mut self, v: bool) {
            self.use_last = v;
        }
        fn reusing(&mut self) -> bool {
            self.reusing
        }
        fn set_reusing(&mut self, v: bool) {
            self.reusing = v;
        }
        fn expired(&mut self) -> bool {
            self.expired
        }
        fn set_expired(&mut self, v: bool) {
            self.expired = v;
        }
        fn set_get_cert_status(&mut self, v: i32) {
            self.cert = v;
        }
        fn prepare_addresses(&mut self) -> bool {
            self.log("p".into());
            self.s.addrs > 0
        }
        fn rewind_addresses(&mut self) {
            self.log("w".into());
            self.idx = 0;
        }
        fn at_begin(&mut self) -> bool {
            self.idx == 0
        }
        fn at_end(&mut self) -> bool {
            self.idx >= self.s.addrs || self.over()
        }
        fn has_context(&mut self) -> bool {
            self.s.has_context
        }
        fn has_auth_context(&mut self) -> bool {
            self.s.has_auth_context
        }
        fn notify_failed(&mut self) {
            self.log(format!("n{}", self.idx));
        }
        fn next_addr(&mut self) {
            self.log(format!("a{}", self.idx));
            self.idx += 1;
        }
        fn retreat_addr(&mut self) {
            self.log("r".into());
            if self.idx > 0 {
                self.idx -= 1;
            }
        }
        fn reset_server(&mut self) {
            self.log("z".into());
        }
        fn load_session(&mut self) {
            self.session = self.pop('S', 0) != 0;
            self.log(format!("s{}", u8::from(self.session)));
        }
        fn clear_session(&mut self) {
            self.log("c".into());
            self.session = false;
        }
        fn has_session(&mut self) -> bool {
            self.session
        }
        fn add_crypt(&mut self) {
            self.log("e".into());
        }
        fn queue_connect(&mut self) {
            self.log("q".into());
        }
        fn connect_block(&mut self) -> i32 {
            let v = self.pop('C', 0) as i32;
            self.log(format!("C{v}"));
            v
        }
        fn peer_auth_async(&mut self) {
            self.log("pa".into());
        }
        fn peer_auth_block(&mut self, status: &mut i32, cert: &mut i32) -> bool {
            let i = self.at.get(&'P').copied().unwrap_or(0);
            let ok = self.pop('P', 0) != 0;
            *status = self.s.lists[&'Q'].get(i).map_or(5, |&v| v as i32);
            *cert = self.s.lists[&'Z'].get(i).map_or(0, |&v| v as i32);
            self.log(format!("P{}", u8::from(ok)));
            if ok {
                self.session = true;
            }
            ok
        }
        fn get_next_request(&mut self) -> i32 {
            let v = if self.over() { 0 } else { self.pop('G', 0) as i32 };
            self.log(format!("G{v}"));
            v
        }
        fn queue_send(&mut self) {
            self.log("qs".into());
        }
        fn send_block(&mut self) -> i32 {
            let v = self.pop('D', 0) as i32;
            self.log(format!("D{v}"));
            v
        }
        fn queue_recv(&mut self) {
            self.log("qr".into());
        }
        fn recv_block(&mut self) -> i32 {
            let v = if self.over() { st::TIMED_OUT } else { self.pop('R', st::TIMED_OUT as i64) as i32 };
            self.log(format!("R{v}"));
            if v == 0 {
                self.response = self.pop('B', 0) as usize;
            }
            v
        }
        fn response(&mut self) -> Vec<u8> {
            RESPONSES[self.response].to_vec()
        }
        fn check_response(&mut self) -> Option<i32> {
            let v = self.pop('K', 0) as i32;
            if v == THROW {
                self.log("Kx".into());
                None
            } else {
                self.log(format!("K{v}"));
                Some(v)
            }
        }
        fn take_op_session(&mut self) {
            self.log("o".into());
            self.session = true;
        }
        fn take_op_response(&mut self) {
            self.response = self.pop('B', 0) as usize;
            self.log(format!("b{}", self.response));
        }
    }

    /// The driver's `log + State(e)` (State starts with a space).
    fn trace(log: &[String], state: String) -> String {
        format!("{} {}", log.join(" "), state)
    }

    fn run(s: &Scenario) -> String {
        let mut f = Fake::new(s);
        run_hook(&mut f);
        let mut out = trace(&f.log, f.state());
        for d in &s.done {
            f.log.clear();
            f.calls = 0;
            let r = callback(&mut f, d);
            out += &format!(" | {} {}", u8::from(r), trace(&f.log, f.state()));
        }
        out
    }

    /// The original's traces (tests/won/won_server_test.cpp `--print 500`, recorded over the original's
    /// statements) replayed over the Rust exchange.
    #[test]
    fn golden_scenarios() {
        let golden = include_str!("golden_server.txt");
        let mut lines = golden.lines();
        let mut count = 0;
        while let Some(desc) = lines.next() {
            let want = lines.next().unwrap().strip_prefix("> ").unwrap();
            let got = run(&parse(desc));
            assert_eq!(got, want, "scenario {desc}");
            count += 1;
        }
        assert_eq!(count, 500);
    }

    #[test]
    fn last_error_rule() {
        assert_eq!(last_server_error(st::NONE, false, 5), (5, true));
        assert_eq!(last_server_error(5, true, 5), (5, true));
        assert_eq!(last_server_error(5, true, 6), (6, false));
        assert_eq!(last_server_error(6, false, 6), (6, false));
    }

    #[test]
    fn requests_and_replies() {
        assert_eq!(ping_request(4, 0x0102_0304, true), vec![12, 0, 0, 0, 3, 1, 5, 4, 3, 2, 1, 1]);
        assert_eq!(multi_ping_request(-1), vec![3, 1, 5, 0xff, 0xff, 0xff, 0xff, 0]);
        assert_eq!(no_op(2), vec![5, 0, 3, 1, 7]);
        assert!(is_expired_reply(RESPONSES[1]) && is_expired_reply(RESPONSES[5]));
        assert!(!is_expired_reply(RESPONSES[2]) && !is_expired_reply(RESPONSES[3]) && !is_expired_reply(RESPONSES[4]));
        let mut w = Writer::new(0, true);
        for b in [3u8, 1, 6] {
            w.append_byte(b);
        }
        w.append_long(100);
        let short = w.to_message();
        let p = ping_reply(&short, 150);
        assert_eq!((p.end, p.fields, p.lag), (0, 1, 50));
        w.append_string(b"app", 2);
        w.append_string(b"log", 2);
        w.append_long(7);
        let cut = w.to_message();
        let p = ping_reply(&cut, 99);
        assert_eq!((p.end, p.fields, p.lag, p.version), (2, 4, u32::MAX, 7));
        assert_eq!((p.text(&cut, 0), p.text(&cut, 1)), (&b"app"[..], &b"log"[..]));
        assert_eq!(ping_reply(&[3, 1, 7, 0, 0, 0, 0], 0).end, 1);
        assert_eq!(ping_reply(&[3, 1], 0).end, 2);
        assert!(multi_ping_reply(&[3, 1, 6, 9, 0, 0, 0], 9));
        assert!(!multi_ping_reply(&[3, 1, 6, 9, 0, 0], 9));
        assert!(!multi_ping_reply(&[3, 1, 6, 9, 0, 0, 0], 8));
    }

    #[test]
    fn multi_ping_rules() {
        assert_eq!(multi_ping_order(&[-1, 5, 0, 5, -2]), vec![2, 1, 3, 4, 0]);
        assert_eq!(multi_ping_response_time_left(10, 0, 3000, 100), 100);
        assert_eq!(multi_ping_response_time_left(2990, 0, 3000, 100), 10);
        assert_eq!(multi_ping_response_time_left(3001, 0, 3000, 100), 0);
        assert_eq!(multi_ping_response_time_left(5, u32::MAX, 3000, 9999), 2994);
        assert_eq!((multi_ping_next(1, 0), multi_ping_next(0, 0), multi_ping_next(0, 3)), (0, 1, 2));
        assert!(multi_ping_gives_up(3, 3) && !multi_ping_gives_up(2, 3));
        assert_eq!(multi_ping_recv_status(st::WSAEMSGSIZE), 1);
        assert!(multi_ping_batch_continues(st::TIMED_OUT) && !multi_ping_batch_continues(5));
        assert!(!multi_ping_keeps(-1, true) && multi_ping_keeps(-1, false));
        let draws = [7, 0, 5];
        let mut i = 0;
        let order = shuffle_order(4, || {
            i += 1;
            draws[i - 1]
        });
        assert_eq!(order, vec![2, 3, 0, 1]);
        assert_eq!(step_time(20000, false, 5), 5);
        assert_eq!(step_time(20000, true, 5), 20000);
        assert_eq!(keep_alive_ms(300), Some(300000));
        assert_eq!(keep_alive_ms(0), None);
        assert_eq!(cleanup_plan(st::TIMED_OUT, false, true, true, false), 3);
        assert_eq!(cleanup_plan(st::SUCCESS, false, true, true, false), 0);
    }
}
