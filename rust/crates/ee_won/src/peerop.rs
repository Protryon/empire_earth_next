//! `PeerAuthOp` and `PeerAuthServerOp` (WONAuth/PeerAuthOp.cpp, PeerAuthServerOp.cpp) on the Rust op base
//! (hostop.rs), round 14 of the Internet code: the connect address, the certificate refresh status, the peer data,
//! the handshake object (peerobj.rs's `PeerAuth`, which `PeerAuthClient` / `PeerAuthServer` were), the length field
//! and the session, and every step: `RunHook` (blocking: connect, request, the two challenges, the complete, one
//! retry after a refreshed certificate; asynchronous: `AsyncRun`), `CallbackHook` (the socket ops' completions and
//! the certificate refresh's), `AsyncRetry`, `AsyncSend`, `AsyncConnect`, `AsyncRefreshCert`, `Success` and
//! `CheckStatus`. The C++ classes are `RustServerOp`s with their constructors and accessors.
//!
//! The socket, the socket ops, the auth context's refresh and the session the socket encrypts with stay C++
//! objects, reached by host calls; the members `ServerOp` keeps (the socket, the auth type, the client key size,
//! the time limits) are read through the host. A handshake step that makes a session hands it to a new C++
//! `AuthSession` at once (`EE_WON_HOST_PEER_NEW_SESSION`), which the op keeps for `GetSession()`.
//!
//! Behaviour: `golden_peerop.txt`, recorded by tests/won/won_peerop_test.cpp from the original's statements
//! before the classes moved. Reached by the Internet login's connections (ServerConnection, a request's peer
//! authentication) and by the LAN game's direct connect with the CD key check on (LanLogic: the host's
//! `PeerAuthServerOp` on an accepted socket, the joiner's `PeerAuthOp`), which needs a certificate from the WON
//! servers first; they are gone.
//!
//! The original's undefined corners, defined: the refresh status the constructor left unset is `WS_Success` (0)
//! until a run sets it; a NULL received message is an empty one; a handshake step that
//! answers success with no message where one was to be sent sends an empty message; the server's `Success` with no
//! session finishes without encryption.

use std::sync::Arc;

use crate::authflow::EeWonLightCert;
use crate::hostop::{
    EeWonHostOp, Host, Obj, Plan, EE_WON_HOST_OP_CONNECT, EE_WON_HOST_OP_RECV_MSG, EE_WON_HOST_OP_SEND_MSG, ST_PENDING,
    ST_SUCCESS,
};
use crate::peerauth::{AUTH_TYPE_NONE, AUTH_TYPE_PERSISTENT, AUTH_TYPE_SESSION};
use crate::peerobj::{other_num, unix_now, PeerAuth, PeerData, EE_WON_PEERAUTH_OTHER_PERMITS};

/// `WS_None`
const ST_NONE: i32 = 100004;
/// `WS_PeerAuth_GetCertFailure`
const ST_GET_CERT_FAILURE: i32 = 100053;

/// The client's tracking ids.
const TRACK_SOCKET: i32 = 1;
const TRACK_GET_CERT: i32 = 2;
/// The server's: an intermediate send, a receive, the last send.
const TRACK_SEND: i32 = 0;
const TRACK_RECV: i32 = 1;
const TRACK_LAST_SEND: i32 = 2;

// Results (`ee_won_hostop_num`):
/// `GetSession()` (an `AuthSession*`, borrowed; NULL: none)
pub const EE_WON_PEEROP_SESSION: i32 = 1;
/// `GetGetCertStatus()`
pub const EE_WON_PEEROP_GET_CERT_STATUS: i32 = 2;
/// the server's: the client's certificate's `VerifyPermissions(0, index, 0)`
pub const EE_WON_PEEROP_CLIENT_PERMITS: i32 = 3;

/// The op's members.
pub(crate) struct PeerOp {
    server: bool,
    /// `mConnectAddr`
    addr: [u8; 6],
    /// `mGetCertStatus` (the constructor left it unset)
    get_cert_status: i32,
    /// `mPeerData`
    peer: Option<Arc<PeerData>>,
    /// `mPeerAuthClient` / `mPeerAuthServer`
    auth: PeerAuth,
    /// `mLengthFieldSize`, the socket's at `RunHook`
    lfs: u8,
    /// the `AuthSession` of the handshake's session
    session: Obj,
    /// the certificate refresh's `LightGetCertOp`: its run (named, not held: the tracker holds it) and its member
    light: Option<(Obj, Arc<EeWonLightCert>)>,
}

impl PeerOp {
    fn new(server: bool, addr: [u8; 6], use_auth2: bool) -> PeerOp {
        let mut auth = PeerAuth::new(server);
        if !server {
            auth.set_use_auth2(use_auth2);
        }
        PeerOp {
            server,
            addr,
            get_cert_status: ST_SUCCESS,
            peer: None,
            auth,
            lfs: 0,
            session: Obj::none(),
            light: None,
        }
    }
}

/// `CheckStatus`: finishes with a failure and says so.
fn check(h: &Host<PeerOp>, status: i32) -> bool {
    if status == ST_SUCCESS {
        return false;
    }
    h.finish(status);
    true
}

/// A handshake step's message in (`HandleRecvMsg`): the status and the out message; a session it made becomes the
/// op's `AuthSession`.
fn handle(h: &Host<PeerOp>, msg: Option<Vec<u8>>) -> (i32, Option<Vec<u8>>) {
    let (out, made) = {
        let mut p = h.st();
        let out = p.auth.recv(msg.as_deref().unwrap_or_default(), unix_now());
        (out, p.auth.session.take())
    };
    if let Some(s) = made {
        let o = h.new_auth_session(Box::into_raw(Box::new(s)));
        h.replace(|p| &mut p.session, o);
    }
    out
}

/// `Start`: the client's request, the server's start.
fn start(h: &Host<PeerOp>, auth_type: i32) -> Vec<u8> {
    let mut p = h.st();
    let (peer, lfs) = (p.peer.clone(), p.lfs);
    p.auth.start(peer, auth_type, lfs, unix_now()).unwrap_or_default()
}

fn expired(h: &Host<PeerOp>, extra: i64) -> bool {
    h.st().peer.as_ref().is_none_or(|p| p.cert_expired(unix_now(), extra))
}

fn set_peer(h: &Host<PeerOp>) {
    let peer = h.ctx_peer_data();
    h.st().peer = peer;
}

// ---- the client (PeerAuthOp)

/// `AsyncSend`: a send of the message and a receive, both tracked.
fn async_send(h: &Host<PeerOp>, socket: Obj, msg: &[u8]) {
    let op = h.new_op(EE_WON_HOST_OP_SEND_MSG, socket, msg, [0; 6], 0);
    h.track(op, TRACK_SOCKET);
    let t = h.send_time();
    h.queue(socket, op, t);
    h.release(op);
    let op = h.new_op(EE_WON_HOST_OP_RECV_MSG, socket, &[], [0; 6], 0);
    h.track(op, TRACK_SOCKET);
    let t = h.recv_time();
    h.queue(socket, op, t);
    h.release(op);
}

/// `AsyncConnect`.
fn async_connect(h: &Host<PeerOp>, socket: Obj) {
    let addr = h.st().addr;
    let op = h.new_op(EE_WON_HOST_OP_CONNECT, socket, &[], addr, 0);
    h.track(op, TRACK_SOCKET);
    let t = h.connect_time();
    h.queue(socket, op, t);
    h.release(op);
}

/// `AsyncRefreshCert`.
fn async_refresh_cert(h: &Host<PeerOp>) {
    h.st().get_cert_status = ST_PENDING;
    let light = h.new_light_get_cert();
    let cert = EeWonLightCert::new();
    h.st().light = Some((light, cert.clone()));
    h.track(light, TRACK_GET_CERT);
    h.refresh_async(light, &cert);
    h.release(light);
}

/// `AsyncRetry`.
fn async_retry(h: &Host<PeerOp>, socket: Obj, auth_type: i32) {
    h.st().get_cert_status = -4;
    async_connect(h, socket);
    let msg = start(h, auth_type);
    async_send(h, socket, &msg);
}

/// `AsyncRun`.
fn async_run(h: &Host<PeerOp>, socket: Obj, auth_type: i32) {
    h.st().get_cert_status = ST_NONE;
    if auth_type != AUTH_TYPE_NONE {
        if expired(h, 120) {
            async_refresh_cert(h);
        }
        if expired(h, 30) {
            return;
        }
    }
    async_connect(h, socket);
    if auth_type != AUTH_TYPE_NONE {
        let msg = start(h, auth_type);
        async_send(h, socket, &msg);
    }
}

/// `Success`: the session's encryption on the socket for an encrypting type, then success.
fn success(h: &Host<PeerOp>, socket: Obj, encrypting: bool) {
    let session = h.st().session;
    if encrypting && !session.is_none() {
        h.add_crypt(socket, session);
    }
    h.finish(ST_SUCCESS);
}

fn encrypts(auth_type: i32) -> bool {
    auth_type == AUTH_TYPE_SESSION || auth_type == AUTH_TYPE_PERSISTENT
}

/// `RunHook`'s blocking flow.
fn client_block(h: &Host<PeerOp>, socket: Obj, auth_type: i32) {
    if auth_type != AUTH_TYPE_NONE && expired(h, 30) {
        let t = h.time_left();
        let s = h.refresh_block(t);
        h.st().get_cert_status = s;
        if s != ST_SUCCESS {
            h.finish(ST_GET_CERT_FAILURE);
            return;
        }
        set_peer(h);
    }
    for i in 0..2 {
        let addr = h.st().addr;
        let t = h.connect_time();
        if check(h, h.connect(socket, addr, t)) {
            return;
        }
        if auth_type == AUTH_TYPE_NONE {
            h.finish(ST_SUCCESS);
            return;
        }
        let msg = start(h, auth_type);
        let t = h.send_time();
        if check(h, h.send_msg(socket, &msg, t)) {
            return;
        }
        let t = h.recv_time();
        let (s, recv) = h.recv_msg(socket, t);
        if check(h, s) {
            return;
        }
        let (s, out) = handle(h, recv);
        if i == 0 && (s == -4 || s == -19) {
            h.st().auth.reset();
            let t = h.time_left();
            let s = h.refresh_block(t);
            h.st().get_cert_status = s;
            if s != ST_SUCCESS {
                h.finish(ST_GET_CERT_FAILURE);
                return;
            }
            set_peer(h);
            continue;
        } else if check(h, s) {
            return;
        }
        let t = h.send_time();
        if check(h, h.send_msg(socket, out.as_deref().unwrap_or_default(), t)) {
            return;
        }
        let t = h.recv_time();
        let (s, recv) = h.recv_msg(socket, t);
        if check(h, s) {
            return;
        }
        let (s, _) = handle(h, recv);
        if check(h, s) {
            return;
        }
        success(h, socket, encrypts(auth_type));
        return;
    }
}

fn client_run(h: &Host<PeerOp>) {
    let socket = h.own_socket();
    let lfs = h.socket_lfs(socket);
    let key = h.client_key();
    let auth_type = h.auth_type();
    {
        let mut p = h.st();
        p.lfs = lfs;
        p.auth.set_client_key_size(key as u8, (key >> 8) & 1 != 0);
    }
    if auth_type != AUTH_TYPE_NONE {
        set_peer(h);
    }
    if h.is_async() {
        async_run(h, socket, auth_type);
        return;
    }
    client_block(h, socket, auth_type);
}

fn client_callback(h: &Host<PeerOp>, id: i32, op: Obj) -> bool {
    let socket = h.own_socket();
    let auth_type = h.auth_type();
    match id {
        TRACK_SOCKET => {
            let s = h.op_status(op);
            if s != ST_SUCCESS {
                h.finish(s);
                return true;
            }
            if auth_type == AUTH_TYPE_NONE {
                h.finish(ST_SUCCESS);
                return true;
            }
            if !h.op_is_recv_msg(op) {
                return true;
            }
            let recv = h.op_bytes(op);
            let (s, out) = handle(h, recv);
            if s == -4 || s == -19 {
                let gcs = {
                    let mut p = h.st();
                    p.auth.reset();
                    p.get_cert_status
                };
                match gcs {
                    ST_NONE => async_refresh_cert(h),
                    ST_PENDING => {}
                    ST_SUCCESS => async_retry(h, socket, auth_type),
                    _ => h.finish(ST_GET_CERT_FAILURE),
                }
                return true;
            } else if check(h, s) {
                return true;
            }
            match out {
                None => success(h, socket, encrypts(auth_type)),
                Some(m) => async_send(h, socket, &m),
            }
        }
        TRACK_GET_CERT => {
            let s = h.op_status(op);
            // the completing op's `GetPeerData()`
            let peer = h.st().light.as_ref().filter(|(o, _)| *o == op).and_then(|(_, c)| c.peer());
            let not_started = {
                let mut p = h.st();
                p.get_cert_status = s;
                p.peer = peer;
                p.auth.st.state == 0
            };
            if not_started {
                if s != ST_SUCCESS {
                    h.finish(ST_GET_CERT_FAILURE);
                } else {
                    async_retry(h, socket, auth_type);
                }
            }
        }
        _ => return false,
    }
    true
}

// ---- the server (PeerAuthServerOp)

fn queue_recv(h: &Host<PeerOp>, socket: Obj) {
    let op = h.new_op(EE_WON_HOST_OP_RECV_MSG, socket, &[], [0; 6], 0);
    h.track(op, TRACK_RECV);
    let t = h.recv_time();
    h.queue(socket, op, t);
    h.release(op);
}

/// The server's `Success`: the session's encryption when its type encrypts.
fn server_success(h: &Host<PeerOp>, socket: Obj) {
    let encrypting = encrypts(h.st().auth.st.auth_type);
    success(h, socket, encrypting);
}

fn server_run(h: &Host<PeerOp>) {
    let socket = h.own_socket();
    let lfs = h.socket_lfs(socket);
    h.st().lfs = lfs;
    set_peer(h);
    start(h, 0);
    if h.is_async() {
        queue_recv(h, socket);
        return;
    }
    let t = h.recv_time();
    let (s, recv) = h.recv_msg(socket, t);
    if check(h, s) {
        return;
    }
    let (s, out) = handle(h, recv);
    check(h, s);
    let Some(out) = out else { return };
    let t = h.send_time();
    check(h, h.send_msg(socket, &out, t));
    if h.status() != ST_PENDING {
        return;
    }
    let t = h.recv_time();
    let (s, recv) = h.recv_msg(socket, t);
    if check(h, s) {
        return;
    }
    let (s, out) = handle(h, recv);
    check(h, s);
    let Some(out) = out else { return };
    let t = h.send_time();
    let s = h.send_msg(socket, &out, t);
    if s == ST_SUCCESS {
        server_success(h, socket);
    } else {
        h.finish(s);
    }
}

fn server_callback(h: &Host<PeerOp>, id: i32, op: Obj) -> bool {
    let s = h.op_status(op);
    if s != ST_SUCCESS {
        h.finish(s);
        return true;
    }
    let socket = h.own_socket();
    if id == TRACK_RECV {
        if !h.op_is_recv_msg(op) {
            return true;
        }
        let recv = h.op_bytes(op);
        let (s, out) = handle(h, recv);
        let done = s == ST_SUCCESS && h.st().auth.st.state == 0;
        let send_id = if done { TRACK_LAST_SEND } else { TRACK_SEND };
        if let Some(m) = out {
            let o = h.new_op(EE_WON_HOST_OP_SEND_MSG, socket, &m, [0; 6], 0);
            h.track(o, send_id);
            let t = h.send_time();
            h.queue(socket, o, t);
            h.release(o);
        }
        if s != ST_SUCCESS {
            h.finish(s);
        } else if send_id != TRACK_LAST_SEND {
            queue_recv(h, socket);
        }
    } else if id == TRACK_LAST_SEND {
        server_success(h, socket);
    }
    true
}

impl Plan for PeerOp {
    fn run(h: &Host<Self>) {
        if h.st().server {
            server_run(h)
        } else {
            client_run(h)
        }
    }
    fn callback(h: &Host<Self>, id: i32, op: Obj) -> bool {
        if h.st().server {
            server_callback(h, id, op)
        } else {
            client_callback(h, id, op)
        }
    }
    fn cleanup(h: &Host<Self>) {
        h.base_cleanup();
    }
    fn num(&self, which: i32, index: i64) -> i64 {
        match which {
            EE_WON_PEEROP_SESSION => self.session.0 as i64,
            EE_WON_PEEROP_GET_CERT_STATUS => i64::from(self.get_cert_status),
            EE_WON_PEEROP_CLIENT_PERMITS => other_num(&self.auth, EE_WON_PEERAUTH_OTHER_PERMITS, 0, index, 0),
            _ => 0,
        }
    }
    fn describe(&self) -> String {
        format!(
            "{} state {} gcs {} lfs {} session {}",
            if self.server { "server" } else { "client" },
            self.auth.st.state,
            self.get_cert_status,
            self.lfs,
            u8::from(!self.session.is_none())
        )
    }
    fn take_objects(&mut self) -> Vec<Obj> {
        vec![std::mem::replace(&mut self.session, Obj::none())]
    }
}

/// `new PeerAuthOp(addr, context, type, socket, useAuth2)`'s Rust op (the C++ class sets the context, type and
/// socket).
///
/// # Safety
/// `addr` holds six bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_hostop_peerauth(addr: *const u8, use_auth2: bool) -> *mut EeWonHostOp {
    let mut a = [0u8; 6];
    a.copy_from_slice(unsafe { std::slice::from_raw_parts(addr, 6) });
    EeWonHostOp::boxed(PeerOp::new(false, a, use_auth2))
}

/// `new PeerAuthServerOp(context, socket)`'s Rust op.
#[no_mangle]
pub extern "C" fn ee_won_hostop_peerauth_server() -> *mut EeWonHostOp {
    EeWonHostOp::boxed(PeerOp::new(true, [0; 6], true))
}

#[cfg(test)]
#[path = "peerop_tests.rs"]
mod tests;
