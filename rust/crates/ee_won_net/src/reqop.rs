//! `ServerRequestOp`'s derived ops in Rust (round 7): an op whose members, request and reply are Rust's is a
//! [`RequestOp`]; the C++ `ServerRequestOp` holds it as an [`EeWonReqOp`] and its virtual `RunHook`,
//! `GetNextRequest`, `CheckResponse` and `Reset` come here, so such an op has no C++ class of its own. The exchange
//! around it (the server list, the socket, the sessions, peer authentication) stays the C++ base's, whose steps are
//! Rust's already (server.rs).
//!
//! `DBProxyOp`'s statements are [`DbProxy`], the base of the DB proxy ops derived here.
//!
//! Round 8 widened the ABI for the ops of `reqops.rs`: an op may refuse to run for the request's auth type (the
//! contest call without one), ask for the certificate's user id and the tick count before its request and reply
//! ([`ee_won_reqop_set_env`]), turn the base's auth2 flag off, and hand its results out by number
//! ([`ee_won_reqop_bytes`], [`ee_won_reqop_num`]: the narrow accessors its callers read).
//!
//! A reply the original read past its end threw `ReadBufferException` out of `CheckResponse` (the base's exchange
//! catches it): that is [`Checked::Cut`], which the C++ throws after the call returned. An invalid header was
//! `InvalidReplyHeader()` (the base's server error, then the next server): [`Checked::InvalidHeader`], which the C++
//! runs, also after the call.

use std::ffi::c_void;

use crate::db;
use crate::ffi::EeWonByteSink;

/// `WS_Success`
pub(crate) const ST_SUCCESS: i32 = 0;
/// `WS_InvalidMessage`
pub(crate) const ST_INVALID_MESSAGE: i32 = 100006;
/// `WS_ServerReq_Recv`
pub(crate) const ST_RECV: i32 = 100059;

/// What `CheckResponse` did.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Checked {
    /// it returned this status
    Status(i32),
    /// it returned `InvalidReplyHeader()`
    InvalidHeader,
    /// a read past the reply's end threw
    Cut,
    /// it returned `RecvServerDatagram()` (DetectFirewallOp's UDP test, reqhost.rs)
    Datagram,
    /// `SetLastServerError(error); return WS_ServerReq_TryNextServer` (CreateAccountOp's server key, reqlast.rs)
    ServerError(i32),
}

/// An op derived from `ServerRequestOp` in Rust.
pub trait RequestOp: Send {
    /// the `mLengthFieldSize` the op's constructor set
    fn length_field_size(&self) -> u8 {
        4
    }
    /// the `mUseAuth2` the op's constructor set
    fn use_auth2(&self) -> bool {
        true
    }
    /// the status the op's `RunHook` finished with before the base's ran, for the request's `mAuthType`
    fn refuses(&self, _auth_type: i32) -> Option<i32> {
        None
    }
    /// the op's `RunHook` before the base's (the request data of the DB proxy ops)
    fn run_hook(&mut self) {}
    /// whether `GetNextRequest` reads the auth context's certificate (its user id)
    fn needs_user_id(&self) -> bool {
        false
    }
    /// the certificate's user id (when asked for) and `WONTickCount()`, before a request and before a reply
    fn set_env(&mut self, _user_id: u32, _now: u32) {}
    /// `GetNextRequest`: the status and the request (`None` leaves `mRequest`)
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>);
    /// `CheckResponse` over `mResponse`
    fn check_response(&mut self, response: &[u8]) -> Checked;
    /// `Reset` (the next server)
    fn reset(&mut self) {}
    /// the op's members after the exchange, as the drivers print them (`tests/won/won_adminop_test.cpp`)
    fn describe(&self) -> String;
    /// a result's bytes by the op's number (`None`: no such result, or NULL)
    fn bytes(&self, _which: i32) -> Option<Vec<u8>> {
        None
    }
    /// a result's bytes by the op's number and an element (a list's index)
    fn bytes_at(&self, which: i32, _index: i64) -> Option<Vec<u8>> {
        self.bytes(which)
    }
    /// a result's number by the op's number (0: none)
    fn num(&self, _which: i32) -> i64 {
        0
    }
    /// a result's number by the op's number and an element (a list's index, a newsletter's id)
    fn num_at(&self, which: i32, _index: i64) -> i64 {
        self.num(which)
    }
    /// a result list of directory entities by the op's number and an element (reqdir.rs)
    fn entities(&self, _which: i32, _index: i64) -> Vec<crate::dir::Entity> {
        Vec::new()
    }
    /// a setter by the op's number, after the op was made (a value as a number or bytes)
    fn set(&mut self, _which: i32, _num: i64, _data: &[u8]) {}
}

/// A Rust op held by a C++ `ServerRequestOp`.
pub struct EeWonReqOp {
    pub(crate) op: Box<dyn RequestOp>,
}

impl EeWonReqOp {
    pub fn new(op: Box<dyn RequestOp>) -> *mut EeWonReqOp {
        Box::into_raw(Box::new(EeWonReqOp { op }))
    }
}

/// The hex the drivers print (`-` for no bytes).
pub(crate) fn hex(d: &[u8]) -> String {
    if d.is_empty() {
        return "-".into();
    }
    d.iter().map(|b| format!("{b:02x}")).collect()
}

// ---- DBProxyOp

/// `DBProxyOp`'s members and statements.
#[derive(Debug, Default)]
pub struct DbProxy {
    pub message: i16,
    pub sub: i16,
    pub reply_message: i16,
    pub reply_sub: i16,
    /// `mRequestData` (`None`: NULL)
    pub request_data: Option<Vec<u8>>,
    /// `mReceivedData`: the replies' data collected until the last one
    received: Vec<u8>,
    /// `mReplyData` (`None`: NULL)
    pub reply_data: Option<Vec<u8>>,
    pub reply_error: Vec<u8>,
}

impl DbProxy {
    /// `DBProxyOp::GetNextRequest`
    pub fn next_request(&self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        (ST_RECV, Some(db::proxy_request(field_size, self.message, self.sub, self.request_data.as_deref())))
    }

    /// `DBProxyOp::CheckResponse`: collects the replies' data until the last-reply flag, then the server's status
    pub fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = db::reply(db::EE_WON_DB_PROXY, response);
        if r.read >= 3 {
            self.reply_message = r.message;
        }
        if r.read >= 4 {
            self.reply_sub = r.sub;
        }
        if r.read >= 6 {
            let at = r.error_at as usize;
            self.reply_error = response[at..at + r.error_len as usize].to_vec();
        } else if r.error_erased {
            self.reply_error.clear();
        }
        if r.cut {
            return Checked::Cut;
        }
        if r.verdict == db::EE_WON_DB_INVALID_HEADER {
            return Checked::InvalidHeader;
        }
        let at = r.rest_at as usize;
        self.received.extend_from_slice(&response[at..at + r.rest_len as usize]);
        if r.verdict == db::EE_WON_DB_DONE {
            self.reply_data = Some(std::mem::take(&mut self.received));
            return Checked::Status(r.result);
        }
        Checked::Status(ST_RECV)
    }

    /// The derived ops' check of the reply's sub-message type.
    pub fn answers_sub(&self) -> bool {
        i32::from(self.reply_sub) == i32::from(self.sub) + 1
    }

    pub fn reply_data(&self) -> &[u8] {
        self.reply_data.as_deref().unwrap_or(&[])
    }

    pub fn describe(&self) -> String {
        let d = match &self.reply_data {
            None => "n".to_string(),
            Some(d) => format!("x{}", hex(d)),
        };
        format!(
            "t{}/{} e{} d{} r{}",
            self.reply_message,
            self.reply_sub,
            hex(&self.reply_error),
            d,
            self.received.len()
        )
    }
}

// ---- the C ABI (the C++ ServerRequestOp's virtuals)

/// [`Checked`] for the C++: the status is the call's result
pub const EE_WON_REQOP_STATUS: i32 = 0;
/// return `InvalidReplyHeader()`
pub const EE_WON_REQOP_INVALID_HEADER: i32 = 1;
/// throw `ReadBufferException`
pub const EE_WON_REQOP_CUT: i32 = 2;
/// `SetLastServerError(status)`, then the next server
pub const EE_WON_REQOP_SERVER_ERROR: i32 = 3;

/// Frees an op.
///
/// # Safety
/// `op` came from a Rust op constructor (or is NULL) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_free(op: *mut EeWonReqOp) {
    if !op.is_null() {
        drop(unsafe { Box::from_raw(op) });
    }
}

/// The `mLengthFieldSize` the op asks for.
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_length_field_size(op: *const EeWonReqOp) -> u8 {
    unsafe { (*op).op.length_field_size() }
}

/// The `mUseAuth2` the op asks for.
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_use_auth2(op: *const EeWonReqOp) -> bool {
    unsafe { (*op).op.use_auth2() }
}

/// The op's `RunHook` statements before the base's, for the request's `mAuthType`: false when the op finished
/// instead (`*finish` the status to finish with; the base's `RunHook` does not run).
///
/// # Safety
/// `op` is a live op; `finish` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_run_hook(op: *mut EeWonReqOp, auth_type: i32, finish: *mut i32) -> bool {
    let op = unsafe { &mut (*op).op };
    if let Some(s) = op.refuses(auth_type) {
        unsafe { *finish = s };
        return false;
    }
    op.run_hook();
    true
}

/// Whether the op's request reads the certificate's user id (the C++ reads it only then).
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_needs_user_id(op: *const EeWonReqOp) -> bool {
    unsafe { (*op).op.needs_user_id() }
}

/// The certificate's user id and `WONTickCount()`, before `GetNextRequest` and before `CheckResponse`.
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_set_env(op: *mut EeWonReqOp, user_id: u32, now: u32) {
    unsafe { (*op).op.set_env(user_id, now) }
}

/// A result's bytes (the op's `EE_WON_REQOP_*` number) into `sink`: false (and no call) when it has none.
///
/// # Safety
/// `op` is a live op; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_bytes(
    op: *const EeWonReqOp,
    which: i32,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match unsafe { (*op).op.bytes(which) } {
        Some(b) => {
            unsafe { sink(context, b.as_ptr(), b.len()) };
            true
        }
        None => false,
    }
}

/// A result's bytes by number and element (a list's index) into `sink`: false (and no call) when it has none.
///
/// # Safety
/// `op` is a live op; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_bytes_at(
    op: *const EeWonReqOp,
    which: i32,
    index: i64,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match unsafe { (*op).op.bytes_at(which, index) } {
        Some(b) => {
            unsafe { sink(context, b.as_ptr(), b.len()) };
            true
        }
        None => false,
    }
}

/// A result's number by number and element.
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_num_at(op: *const EeWonReqOp, which: i32, index: i64) -> i64 {
    unsafe { (*op).op.num_at(which, index) }
}

/// A setter by number (a number and bytes).
///
/// # Safety
/// `op` is a live op; `data` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_set(op: *mut EeWonReqOp, which: i32, num: i64, data: *const u8, len: usize) {
    let d = unsafe { crate::ffi::bytes(data, len) };
    unsafe { (*op).op.set(which, num, d) }
}

/// A result list of directory entities, each to `sink`; the count.
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_entities(
    op: *const EeWonReqOp,
    which: i32,
    index: i64,
    sink: crate::dir::EeWonDirEntitySink,
    context: *mut c_void,
) -> usize {
    let list = unsafe { (*op).op.entities(which, index) };
    for e in &list {
        // SAFETY: the sink's contract
        unsafe { crate::dir::give(e, sink, context) };
    }
    list.len()
}

/// A result's number (the op's `EE_WON_REQOP_*` number).
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_num(op: *const EeWonReqOp, which: i32) -> i64 {
    unsafe { (*op).op.num(which) }
}

/// `GetNextRequest`: the request into `sink` (when there is one), the status returned.
///
/// # Safety
/// `op` is a live op; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_next_request(
    op: *mut EeWonReqOp,
    field_size: u8,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> i32 {
    let (status, request) = unsafe { (*op).op.next_request(field_size) };
    if let Some(r) = request {
        unsafe { sink(context, r.as_ptr(), r.len()) };
    }
    status
}

/// `CheckResponse` over the reply: the status returned, `*verdict` an `EE_WON_REQOP_*`.
///
/// # Safety
/// `op` is a live op; `data` holds `len` bytes; `verdict` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_check_response(
    op: *mut EeWonReqOp,
    data: *const u8,
    len: usize,
    verdict: *mut i32,
) -> i32 {
    let reply = unsafe { crate::ffi::bytes(data, len) };
    let (v, s) = match unsafe { (*op).op.check_response(reply) } {
        Checked::Status(s) => (EE_WON_REQOP_STATUS, s),
        Checked::InvalidHeader => (EE_WON_REQOP_INVALID_HEADER, 0),
        Checked::Cut | Checked::Datagram => (EE_WON_REQOP_CUT, 0),
        Checked::ServerError(e) => (EE_WON_REQOP_SERVER_ERROR, e),
    };
    unsafe { *verdict = v };
    s
}

/// `Reset`.
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_reset(op: *mut EeWonReqOp) {
    unsafe { (*op).op.reset() }
}

/// The op's members as the drivers print them, into `sink`.
///
/// # Safety
/// `op` is a live op; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqop_describe(op: *const EeWonReqOp, sink: EeWonByteSink, context: *mut c_void) {
    let d = unsafe { (*op).op.describe() };
    unsafe { sink(context, d.as_ptr(), d.len()) }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn proxy_reply(message: i16, sub: i16, status: i16, error: &[u8], flags: u8, data: &[u8]) -> Vec<u8> {
        let mut v = vec![5, 14, 0];
        v.extend_from_slice(&message.to_le_bytes());
        v.extend_from_slice(&sub.to_le_bytes());
        v.extend_from_slice(&status.to_le_bytes());
        v.extend_from_slice(&(error.len() as u16).to_le_bytes());
        v.extend_from_slice(error);
        v.push(flags);
        v.extend_from_slice(data);
        v
    }

    #[test]
    fn proxy_collects_until_the_last_reply() {
        let mut p = DbProxy { message: 1002, sub: 18, ..Default::default() };
        assert_eq!(p.check_response(&proxy_reply(1002, 19, 0, b"", 0, &[1, 2])), Checked::Status(ST_RECV));
        assert!(p.reply_data.is_none());
        assert_eq!(p.check_response(&proxy_reply(1002, 19, -3, b"no", 0x80, &[3])), Checked::Status(-3));
        assert_eq!(p.reply_data.as_deref(), Some(&[1u8, 2, 3][..]));
        assert_eq!((p.reply_message, p.reply_sub, p.reply_error.as_slice()), (1002, 19, &b"no"[..]));
        assert!(p.answers_sub());
        assert_eq!(p.describe(), "t1002/19 e6e6f dx010203 r0");
        // a wrong header: the next server; a reply cut inside the error string erases it
        assert_eq!(p.check_response(&[4, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]), Checked::InvalidHeader);
        let cut = proxy_reply(1, 2, 0, b"abc", 0, &[]);
        assert_eq!(p.check_response(&cut[..cut.len() - 3]), Checked::Cut);
        assert!(p.reply_error.is_empty());
        assert_eq!((p.reply_message, p.reply_sub), (1, 2));
    }

    #[test]
    fn proxy_request_carries_the_types_and_data() {
        let p = DbProxy { message: 6, sub: 1, ..Default::default() };
        let (s, r) = p.next_request(4);
        assert_eq!(s, ST_RECV);
        assert_eq!(r.unwrap(), [11, 0, 0, 0, 5, 14, 0, 6, 0, 1, 0]);
        let p = DbProxy { message: 6, sub: 1, request_data: Some(vec![9]), ..Default::default() };
        assert_eq!(p.next_request(2).1.unwrap(), [10, 0, 5, 14, 0, 6, 0, 1, 0, 9]);
    }
}
