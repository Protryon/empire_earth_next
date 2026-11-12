//! Round 8's ops derived from `ServerRequestOp` in Rust ([`RequestOp`]): WONDB's `ContestCallOp` and
//! `EventReportOp`, WONMisc's `ReportPatchStatusOp` (a `DBProxyOp`, [`DbProxy`]) and WONServer's `PingOp`. Their
//! C++ classes (`ContestCallOp.cpp`, `EventReportOp.cpp`, `ReportPatchStatusOp.cpp`, `PingOp.cpp` and their
//! headers) are deleted; a C++ `ServerRequestOp` runs one made by [`ee_won_reqops_new`] from its members, and its
//! callers read the results through [`crate::reqop::ee_won_reqop_bytes`] / [`crate::reqop::ee_won_reqop_num`].
//!
//! - The contest call refuses to run without an auth type (`WS_ServerReq_NeedAuthContext`), turns the base's auth2
//!   off, sends with a 2-byte length field and reads the certificate's user id at each request.
//! - The patch status report's datagram path (`SetUDPSocket`) is the C++ base's: with a UDP socket it sends the
//!   request built at length field 0 as one datagram and finishes (`ServerRequestOp::RunDatagram`).
//! - The ping reads `WONTickCount()` at its request and its replies (the lag); its `Reset` clears every member
//!   but the lag. The original left the lag, version, pid and auth flag unset until a reply or a `Reset`; they
//!   start at 0 here.
//!
//! `golden_reqops.txt` was printed by `tests/won/won_reqops_test.cpp` over the original's statements before the
//! port. Reached by nothing in the game (no C++ made these ops; their servers are gone).

use crate::adminop::Member;
use crate::db::{self, Attachment, Detail, Event};
use crate::patch;
use crate::reqop::{hex, Checked, DbProxy, EeWonReqOp, RequestOp, ST_RECV, ST_SUCCESS};
use crate::server;
use crate::text::utf16_from_le_bytes;

/// `ContestCallOp`: the procedure number, whether there is request data, the data
pub const EE_WON_REQOPS_CONTEST: i32 = 1;
/// `EventReportOp`: the activity type; has-server, server type, logical name (UTF-16 LE), net address; has-client,
/// name (UTF-16 LE), net address; has-user, user id, name (UTF-16 LE); the detail count, each detail's type, data
/// type, number, text (UTF-16 LE); the attachment count, each one's description (UTF-16 LE), content type,
/// has-data, data
pub const EE_WON_REQOPS_EVENT: i32 = 2;
/// `ReportPatchStatusOp`: product, config, from, to, net address, the patch status
pub const EE_WON_REQOPS_PATCH_STATUS: i32 = 3;
/// `PingOp`: the length field size, the extended flag
pub const EE_WON_REQOPS_PING: i32 = 4;

/// the contest's `GetReplyData()` (bytes; none while NULL)
pub const EE_WON_REQOP_CONTEST_REPLY: i32 = 1;
/// the patch status report's `mReplyData` (bytes; none while NULL)
pub const EE_WON_REQOP_PROXY_REPLY: i32 = 2;
/// the ping's `mAppName`, `mLogicalName`, `mImage`, `mPorts`, `mRegData`, `mServData` (bytes, `+0` .. `+5`)
pub const EE_WON_REQOP_PING_TEXT: i32 = 10;
/// the ping's `mLag` (number)
pub const EE_WON_REQOP_PING_LAG: i32 = 20;
/// the ping's `mVersion` (number)
pub const EE_WON_REQOP_PING_VERSION: i32 = 21;
/// the ping's `mPID` (number)
pub const EE_WON_REQOP_PING_PID: i32 = 22;
/// the ping's `mIsAuth` (number)
pub const EE_WON_REQOP_PING_IS_AUTH: i32 = 23;

/// `WS_ServerReq_NeedAuthContext`
const ST_NEED_AUTH_CONTEXT: i32 = 100056;

/// The members, read in order (missing ones: 0 and no bytes).
struct Members<'a> {
    m: &'a [Member],
    at: usize,
}

impl Members<'_> {
    fn next(&mut self) -> Member {
        let v = self.m.get(self.at).cloned().unwrap_or_default();
        self.at += 1;
        v
    }
    fn num(&mut self) -> i64 {
        self.next().num
    }
    fn bytes(&mut self) -> Vec<u8> {
        self.next().bytes
    }
    fn wide(&mut self) -> Vec<u16> {
        utf16_from_le_bytes(&self.next().bytes)
    }
    /// a has-flag then the bytes: `None` for NULL
    fn data(&mut self) -> Option<Vec<u8>> {
        let present = self.num() != 0;
        let d = self.bytes();
        present.then_some(d)
    }
}

fn data_hex(d: &Option<Vec<u8>>) -> String {
    match d {
        None => "n".into(),
        Some(d) => format!("x{}", hex(d)),
    }
}

// ---- ContestCallOp

/// `ContestCallOp`
#[derive(Debug, Default)]
pub struct ContestOp {
    pub proc_num: u16,
    pub request_data: Option<Vec<u8>>,
    pub reply_data: Option<Vec<u8>>,
    user_id: u32,
}

impl RequestOp for ContestOp {
    fn length_field_size(&self) -> u8 {
        2
    }
    fn use_auth2(&self) -> bool {
        false
    }
    fn refuses(&self, auth_type: i32) -> Option<i32> {
        (auth_type == 0).then_some(ST_NEED_AUTH_CONTEXT)
    }
    fn needs_user_id(&self) -> bool {
        true
    }
    fn set_env(&mut self, user_id: u32, _now: u32) {
        self.user_id = user_id;
    }
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        let r = db::contest_request(field_size, self.user_id, self.proc_num as i16, self.request_data.as_deref());
        (ST_RECV, Some(r))
    }
    fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = db::reply(db::EE_WON_DB_CONTEST, response);
        if r.cut {
            return Checked::Cut;
        }
        if r.verdict == db::EE_WON_DB_INVALID_HEADER {
            return Checked::InvalidHeader;
        }
        let at = r.rest_at as usize;
        self.reply_data = Some(response[at..at + r.rest_len as usize].to_vec());
        Checked::Status(r.result)
    }
    fn describe(&self) -> String {
        data_hex(&self.reply_data)
    }
    fn bytes(&self, which: i32) -> Option<Vec<u8>> {
        (which == EE_WON_REQOP_CONTEST_REPLY).then(|| self.reply_data.clone()).flatten()
    }
}

// ---- EventReportOp

/// An attachment as the op keeps it.
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct EventAttachment {
    pub description: Vec<u16>,
    pub content_type: u8,
    pub data: Option<Vec<u8>>,
}

/// `EventReportOp`
#[derive(Debug, Default)]
pub struct EventOp {
    pub activity_type: u16,
    pub server: Option<(u16, Vec<u16>, Vec<u8>)>,
    pub client: Option<(Vec<u16>, Vec<u8>)>,
    pub user: Option<(u32, Vec<u16>)>,
    pub details: Vec<Detail>,
    pub attachments: Vec<EventAttachment>,
}

impl EventOp {
    fn from_members(m: &mut Members) -> EventOp {
        let mut e = EventOp { activity_type: m.num() as u16, ..Default::default() };
        let has = m.num() != 0;
        let (t, name, addr) = (m.num() as u16, m.wide(), m.bytes());
        if has {
            e.server = Some((t, name, addr));
        }
        let has = m.num() != 0;
        let (name, addr) = (m.wide(), m.bytes());
        if has {
            e.client = Some((name, addr));
        }
        let has = m.num() != 0;
        let (id, name) = (m.num() as u32, m.wide());
        if has {
            e.user = Some((id, name));
        }
        for _ in 0..m.num().max(0) {
            let (detail_type, data_type, number, text) = (m.num() as u16, m.num() as i32, m.num() as u32, m.wide());
            e.details.push(Detail { detail_type, data_type, number, text });
        }
        for _ in 0..m.num().max(0) {
            let (description, content_type, data) = (m.wide(), m.num() as u8, m.data());
            e.attachments.push(EventAttachment { description, content_type, data });
        }
        e
    }
}

impl RequestOp for EventOp {
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        let e = Event {
            activity_type: self.activity_type,
            server: self.server.as_ref().map(|(t, n, a)| (*t, n.clone(), a.as_slice())),
            client: self.client.as_ref().map(|(n, a)| (n.clone(), a.as_slice())),
            user: self.user.clone(),
            details: self.details.clone(),
            attachments: self
                .attachments
                .iter()
                .map(|a| Attachment {
                    description: a.description.clone(),
                    content_type: a.content_type,
                    data: a.data.as_deref(),
                })
                .collect(),
        };
        (ST_RECV, Some(db::event_request(field_size, &e)))
    }
    fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = db::reply(db::EE_WON_DB_EVENT, response);
        if r.cut {
            return Checked::Cut;
        }
        if r.verdict == db::EE_WON_DB_INVALID_HEADER {
            return Checked::InvalidHeader;
        }
        Checked::Status(r.result)
    }
    fn describe(&self) -> String {
        "-".into()
    }
}

// ---- ReportPatchStatusOp

/// `ReportPatchStatusOp` (`mMsgType` 19)
#[derive(Debug, Default)]
pub struct PatchStatusOp {
    /// product, config, from, to, net address (the patch's URL)
    pub strings: [Vec<u8>; 5],
    pub status: u8,
    pub proxy: DbProxy,
}

const PATCH_STATUS_MSG: i16 = 19;

impl RequestOp for PatchStatusOp {
    fn run_hook(&mut self) {
        self.proxy.message = 1004;
        self.proxy.sub = PATCH_STATUS_MSG;
        let s: Vec<&[u8]> = self.strings.iter().map(|s| s.as_slice()).collect();
        self.proxy.request_data = Some(patch::strings_and_byte(&s, self.status));
    }
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        self.proxy.next_request(field_size)
    }
    fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = self.proxy.check_response(response);
        if r != Checked::Status(ST_SUCCESS) {
            return r;
        }
        if !self.proxy.answers_sub() {
            return Checked::InvalidHeader;
        }
        Checked::Status(ST_SUCCESS)
    }
    fn describe(&self) -> String {
        self.proxy.describe()
    }
    fn bytes(&self, which: i32) -> Option<Vec<u8>> {
        (which == EE_WON_REQOP_PROXY_REPLY).then(|| self.proxy.reply_data.clone()).flatten()
    }
}

// ---- PingOp

/// `PingOp`
#[derive(Debug, Default)]
pub struct PingOp {
    pub field_size: u8,
    pub extended: bool,
    pub lag: u32,
    /// app name, logical name, image, ports, registration data, server data
    pub texts: [Vec<u8>; 6],
    pub version: i64,
    pub pid: i32,
    pub is_auth: bool,
    now: u32,
}

impl RequestOp for PingOp {
    fn length_field_size(&self) -> u8 {
        self.field_size
    }
    fn set_env(&mut self, _user_id: u32, now: u32) {
        self.now = now;
    }
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        (ST_RECV, Some(server::ping_request(field_size, self.now, self.extended)))
    }
    fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = server::ping_reply(response, self.now);
        if r.end == 1 {
            return Checked::InvalidHeader;
        }
        // the fields read before a cut are assigned, then the cut throws
        if r.fields >= 1 {
            self.lag = r.lag;
        }
        const TEXT_FIELD: [u32; 6] = [2, 3, 5, 7, 9, 10];
        for (k, &f) in TEXT_FIELD.iter().enumerate() {
            if r.fields >= f {
                self.texts[k] = r.text(response, k).to_vec();
            }
        }
        if r.fields >= 4 {
            self.version = i64::from(r.version);
        }
        if r.fields >= 6 {
            self.pid = r.pid;
        }
        if r.fields >= 8 {
            self.is_auth = r.is_auth;
        }
        if r.end == 2 {
            return Checked::Cut;
        }
        Checked::Status(ST_SUCCESS)
    }
    fn reset(&mut self) {
        self.texts.iter_mut().for_each(Vec::clear);
        self.version = 0;
        self.pid = 0;
        self.is_auth = false;
    }
    fn describe(&self) -> String {
        let t = &self.texts;
        format!(
            "l{} a{} n{} i{} p{} r{} s{} v{} d{} u{}",
            self.lag,
            hex(&t[0]),
            hex(&t[1]),
            hex(&t[2]),
            hex(&t[3]),
            hex(&t[4]),
            hex(&t[5]),
            self.version,
            self.pid,
            u8::from(self.is_auth)
        )
    }
    fn bytes(&self, which: i32) -> Option<Vec<u8>> {
        let k = which.checked_sub(EE_WON_REQOP_PING_TEXT)?;
        self.texts.get(usize::try_from(k).ok()?).cloned()
    }
    fn num(&self, which: i32) -> i64 {
        match which {
            EE_WON_REQOP_PING_LAG => i64::from(self.lag),
            EE_WON_REQOP_PING_VERSION => self.version,
            EE_WON_REQOP_PING_PID => i64::from(self.pid),
            EE_WON_REQOP_PING_IS_AUTH => i64::from(self.is_auth),
            _ => 0,
        }
    }
}

/// The op of `kind` (`EE_WON_REQOPS_*`) with its members in that kind's order; `None` for another kind.
pub fn new_op(kind: i32, members: &[Member]) -> Option<Box<dyn RequestOp>> {
    let mut m = Members { m: members, at: 0 };
    Some(match kind {
        EE_WON_REQOPS_CONTEST => {
            let proc_num = m.num() as u16;
            Box::new(ContestOp { proc_num, request_data: m.data(), ..Default::default() })
        }
        EE_WON_REQOPS_EVENT => Box::new(EventOp::from_members(&mut m)),
        EE_WON_REQOPS_PATCH_STATUS => {
            let strings = [m.bytes(), m.bytes(), m.bytes(), m.bytes(), m.bytes()];
            Box::new(PatchStatusOp { strings, status: m.num() as u8, proxy: DbProxy::default() })
        }
        EE_WON_REQOPS_PING => {
            let field_size = m.num() as u8;
            Box::new(PingOp { field_size, extended: m.num() != 0, ..Default::default() })
        }
        _ => return None,
    })
}

// ---- the C ABI

/// An op of `kind` (`EE_WON_REQOPS_*`) with its members, for a C++ `ServerRequestOp` to run; NULL for an unknown
/// kind.
///
/// # Safety
/// `values` holds `count` values whose byte ranges are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqops_new(
    kind: i32,
    values: *const crate::admin::EeWonAdminValue,
    count: usize,
) -> *mut EeWonReqOp {
    let members: Vec<Member> = if values.is_null() || count == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(values, count) }
            .iter()
            .map(|v| Member { num: v.num, bytes: unsafe { crate::ffi::bytes(v.data, v.len) }.to_vec() })
            .collect()
    };
    match new_op(kind, &members) {
        Some(op) => EeWonReqOp::new(op),
        None => std::ptr::null_mut(),
    }
}

#[cfg(test)]
#[path = "reqops_tests.rs"]
mod tests;
