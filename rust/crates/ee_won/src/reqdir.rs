//! Round 11's ops derived from `ServerRequestOp` in Rust ([`RequestOp`]): WONDir's `GetDirOp`, `GetMultiDirOp`,
//! `GetAliasDirOp`, `FindEntityOp` and `GetServiceOp` (with `GetEntityRequest`, one directory's query and its
//! reply) and WONDB's `GetAccountOp` and `UpdateAccountOp`. Their C++ classes are deleted; a `RustServerOp`
//! (`RustServerOp::NewRequest` over [`crate::reqops::ee_won_reqops_new`]) runs one, and the callers read the
//! results through `ee_won_hostop_entities` (the entity lists, as the C++ `DirEntity` objects they hand on to
//! `ServerContext::AddAddressesFromDir` and the lobby's server list), `ee_won_hostop_num` and `ee_won_hostop_bytes`.
//!
//! - The game's readers: the lobby's room list (`RoutingLogic::RefreshServerList`, a listing), its login's server
//!   query (`InitLogic`, a multi-directory listing: the auth, account, firewall and patch servers), EEWonNet's
//!   Titan server list, the patch ops' version and patch listings (`GetVersionOp`, `FindPatchOp`), GameSpy's
//!   service lookup, the lobby's and EEWonNet's account query and update.
//! - A query's status (`GetEntityRequest::mStatus`, which its constructor left unset) starts at `WS_None` here.
//! - The account ops' profile is changed after the op was made (EEWonNet sets a newsletter after `RunAsync`): the
//!   setter goes through the base's lock of the derived op ([`EE_WON_REQOP_ACCOUNT_NEWSLETTER`]).
//!
//! `golden_reqdir.txt` was printed by `tests/won/won_reqdir_test.cpp` over the original's statements before the
//! port. Reached by nothing in the game but an Internet login (the directory and account servers are gone).

use std::collections::BTreeSet;

use crate::adminop::Member;
use crate::db::{self, EeWonProfile};
use crate::dir::{self, Entity, Reply};
use crate::dirops::{self, op, Object, Query, Request};
use crate::reqop::{hex, Checked, RequestOp, ST_RECV, ST_SUCCESS};
use crate::text::utf16_from_le_bytes;
use crate::wire::Writer;

/// `GetDirOp`: the query's flags, path (UTF-16 LE), data type count and each type
pub const EE_WON_REQOPS_DIR_GET: i32 = 7;
/// `GetMultiDirOp`: the query count, each query as `GetDirOp`'s
pub const EE_WON_REQOPS_DIR_MULTI: i32 = 8;
/// `GetAliasDirOp`: the product name, then the queries as `GetMultiDirOp`'s
pub const EE_WON_REQOPS_DIR_ALIAS: i32 = 9;
/// `FindEntityOp`: is-service, the get flags, the match mode, the find flags, the path, name and display name
/// (UTF-16 LE), has-net-address and the address, the find data object count and each one's type, has-data and
/// data, the data type count and each type
pub const EE_WON_REQOPS_DIR_FIND: i32 = 10;
/// `GetServiceOp`: the flags, the path and name (UTF-16 LE), has-net-address and the address, the data types
pub const EE_WON_REQOPS_DIR_SERVICE: i32 = 11;
/// `GetAccountOp`: the profile (the newsletter count and each id and subscription; the birth month, day and year;
/// the custom action id, has-data and data)
pub const EE_WON_REQOPS_ACCOUNT_GET: i32 = 12;
/// `UpdateAccountOp`: the new password (UTF-16 LE), the email, the profile
pub const EE_WON_REQOPS_ACCOUNT_UPDATE: i32 = 13;

/// the number of queries (number)
pub const EE_WON_REQOP_DIR_LISTS: i32 = 40;
/// a query's status (number; the element: the query's index)
pub const EE_WON_REQOP_DIR_STATUS: i32 = 41;
/// a query's entities (entities; the element: the query's index; the find's own list is 0)
pub const EE_WON_REQOP_DIR_LIST: i32 = 42;
/// the find's and the get service's `GetService()` (entities: none or one)
pub const EE_WON_REQOP_DIR_SERVICE: i32 = 43;
/// the get account's `mEmail` (bytes)
pub const EE_WON_REQOP_ACCOUNT_EMAIL: i32 = 44;
/// the account ops' `mErrorString` (bytes)
pub const EE_WON_REQOP_ACCOUNT_ERROR: i32 = 45;
/// the profile's `GetNewsletterSub(id)` (number; the element: the id); a setter: `AddNewsletterSub` (the number:
/// the id, with the subscription at bit 16)
pub const EE_WON_REQOP_ACCOUNT_NEWSLETTER: i32 = 46;
/// a setter: the profile's `ClearNewsletterSubs()`
pub const EE_WON_REQOP_ACCOUNT_CLEAR_NEWSLETTERS: i32 = 47;

/// `WS_None`: a query's status before its first reply
const ST_NONE: i32 = 100004;
/// `WS_ServerReq_InvalidReplyHeader`
const ST_INVALID_REPLY_HEADER: i32 = 100063;

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
    /// a count then that many byte strings, as a `std::set<std::string>`
    fn types(&mut self) -> BTreeSet<Vec<u8>> {
        let n = self.num().max(0);
        (0..n).map(|_| self.bytes()).collect()
    }
    fn query(&mut self) -> EntityRequest {
        let flags = self.num() as u32;
        let path = self.wide();
        EntityRequest { flags, path, types: self.types(), ..Default::default() }
    }
    fn profile(&mut self) -> EeWonProfile {
        let mut p = EeWonProfile::default();
        for _ in 0..self.num().max(0) {
            let id = self.num() as u16;
            p.newsletters.insert(id, self.num() != 0);
        }
        p.birth_month = self.num() as u8;
        p.birth_day = self.num() as u8;
        p.birth_year = self.num() as u16;
        p.custom_action_id = self.num() as u16;
        p.custom_action_data = self.data();
        p
    }
}

// ---- the entities as the drivers print them (tests/won/won_dir_test.cpp's form)

/// A text's UTF-16 units, `|` around and `.` between.
fn units(t: &[u16]) -> String {
    let u: Vec<String> = t.iter().map(|c| format!("{c:x}")).collect();
    format!("|{}|", u.join("."))
}

/// An entity as the C++ `DirEntity` made from it holds it (the fields not read keep the constructor's values).
pub(crate) fn describe_entity(e: &Entity) -> String {
    let mut s = format!(
        "type {} life {} crc {} created {} cip {} cid {} touched {} tip {} tid {} uniq {} path {} name {} display {} addr {}",
        e.kind as i8,
        e.lifespan,
        e.crc as u32,
        e.created,
        e.create_ip as u32,
        e.create_id as u32,
        e.touched,
        e.touch_ip as u32,
        e.touch_id as u32,
        u8::from(e.req_unique),
        units(&e.path),
        units(&e.name),
        units(&e.display_name),
        if e.fields & dir::read::NET_ADDR != 0 { hex(&e.net_addr) } else { "null".into() }
    );
    for o in &e.data_objects {
        let d = o.data.as_ref().map_or_else(|| "null".to_string(), |d| hex(d));
        s.push_str(&format!(" do {}={d}", hex(&o.data_type)));
    }
    for a in &e.acls {
        s.push_str(&format!(" acl {}/{}/{}/{}", a.kind as i8, a.user as u32, a.community as u32, a.trust as u16));
    }
    s
}

fn describe_list(l: &[Entity]) -> String {
    let mut s = String::from("[");
    for e in l {
        s.push('{');
        s.push_str(&describe_entity(e));
        s.push('}');
    }
    s.push(']');
    s
}

/// What `DirEntityReplyParser::ParseMultiEntityReply` returned (`None`: it threw), the entities read whole appended.
fn parse_multi(reply: &[u8], out: &mut Vec<Entity>) -> Option<i32> {
    let mut sequence = None;
    match dir::parse_multi(reply, out, &mut sequence) {
        Reply::Parsed(s) => Some(s),
        Reply::InvalidHeader => Some(ST_INVALID_REPLY_HEADER),
        Reply::Status(s) => Some(i32::from(s)),
        Reply::PastEnd => None,
    }
}

/// What `ParseSingleEntityReply` returned (`None`: it threw); the entity when read whole.
fn parse_single(reply: &[u8]) -> (Option<i32>, Option<Entity>) {
    let (r, e) = dir::parse_single(reply);
    let s = match r {
        Reply::Parsed(_) => Some(ST_SUCCESS),
        Reply::InvalidHeader => Some(ST_INVALID_REPLY_HEADER),
        Reply::Status(s) => Some(i32::from(s)),
        Reply::PastEnd => None,
    };
    (s, e)
}

// ---- GetEntityRequest, GetDirOp, GetMultiDirOp, GetAliasDirOp

/// `GetEntityRequest`: one directory's query and what its replies listed.
#[derive(Clone, Debug)]
pub struct EntityRequest {
    pub flags: u32,
    pub path: Vec<u16>,
    pub types: BTreeSet<Vec<u8>>,
    pub list: Vec<Entity>,
    pub status: i32,
}

impl Default for EntityRequest {
    fn default() -> EntityRequest {
        EntityRequest {
            flags: 0x0602_0837,
            path: Vec::new(),
            types: BTreeSet::new(),
            list: Vec::new(),
            status: ST_NONE,
        }
    }
}

impl EntityRequest {
    /// `ParseMultiEntityReply`: the entities appended, the status kept (not when the reply was cut)
    fn parse(&mut self, reply: &[u8]) -> Option<i32> {
        let s = parse_multi(reply, &mut self.list)?;
        self.status = s;
        Some(s)
    }
    fn query(&self) -> Query<'_> {
        Query { flags: self.flags, path: self.path.clone(), types: self.types.iter().map(Vec::as_slice).collect() }
    }
    fn describe(&self) -> String {
        format!("s{}{}", self.status, describe_list(&self.list))
    }
}

/// The listing ops: `GetDirOp` (one query), `GetMultiDirOp` and `GetAliasDirOp` (a list of them).
#[derive(Debug, Default)]
pub struct DirListOp {
    /// `EE_WON_REQOPS_DIR_GET`, `_MULTI` or `_ALIAS`
    kind: i32,
    product: Vec<u8>,
    requests: Vec<EntityRequest>,
    /// `mCurRequest` as an index (the end: the list's length)
    cur: usize,
    /// `mNumFailedRequests` (neither `Reset` nor a request clears it)
    failed: i32,
}

impl RequestOp for DirListOp {
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        let mut r = Request { is_service: true, ..Default::default() };
        if self.kind == EE_WON_REQOPS_DIR_GET {
            let q = &self.requests[0];
            r.op = op::GET_DIR;
            r.flags = q.flags;
            r.path = q.path.clone();
            r.types = q.types.iter().map(Vec::as_slice).collect();
        } else {
            r.op = if self.kind == EE_WON_REQOPS_DIR_MULTI { op::GET_MULTI_DIR } else { op::ALIAS_GET };
            r.product = &self.product;
            r.queries = self.requests.iter().map(EntityRequest::query).collect();
        }
        let m = dirops::request(&r, field_size);
        if self.kind != EE_WON_REQOPS_DIR_GET {
            self.cur = 0;
        }
        (ST_RECV, Some(m))
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        if self.kind == EE_WON_REQOPS_DIR_GET {
            return match self.requests[0].parse(response) {
                None => Checked::Cut,
                Some(ST_INVALID_REPLY_HEADER) => Checked::InvalidHeader,
                Some(s) => Checked::Status(s),
            };
        }
        if self.cur == self.requests.len() {
            // an empty list: a new query's parse for the status
            return match EntityRequest::default().parse(response) {
                None => Checked::Cut,
                Some(s) => Checked::Status(s),
            };
        }
        let Some(_) = self.requests[self.cur].parse(response) else { return Checked::Cut };
        if dirops::multi_reply_step(self.requests[self.cur].status, &mut self.failed) {
            self.cur += 1;
        }
        if self.cur == self.requests.len() {
            return Checked::Status(dirops::multi_reply_end(self.failed, self.requests.len()));
        }
        Checked::Status(ST_RECV)
    }

    fn reset(&mut self) {
        for r in &mut self.requests {
            r.list.clear();
        }
    }

    fn describe(&self) -> String {
        if self.kind == EE_WON_REQOPS_DIR_GET {
            return self.requests[0].describe();
        }
        let mut s = format!("c{} f{}", self.cur, self.failed);
        for r in &self.requests {
            s.push(' ');
            s.push_str(&r.describe());
        }
        s
    }

    fn num_at(&self, which: i32, index: i64) -> i64 {
        match which {
            EE_WON_REQOP_DIR_LISTS => self.requests.len() as i64,
            EE_WON_REQOP_DIR_STATUS => {
                usize::try_from(index).ok().and_then(|i| self.requests.get(i)).map_or(0, |r| i64::from(r.status))
            }
            _ => 0,
        }
    }

    fn entities(&self, which: i32, index: i64) -> Vec<Entity> {
        match (which, usize::try_from(index).ok().and_then(|i| self.requests.get(i))) {
            (EE_WON_REQOP_DIR_LIST, Some(r)) => r.list.clone(),
            _ => Vec::new(),
        }
    }
}

// ---- FindEntityOp, GetServiceOp

/// `FindEntityOp` and `GetServiceOp` (`find` false: its get flags, path, name, address and types only).
#[derive(Debug, Default)]
pub struct DirFindOp {
    find: bool,
    is_service: bool,
    flags: u32,
    mode: u8,
    find_flags: u8,
    path: Vec<u16>,
    name: Vec<u16>,
    display_name: Vec<u16>,
    net_addr: Option<Vec<u8>>,
    objects: Vec<(Vec<u8>, Option<Vec<u8>>)>,
    types: BTreeSet<Vec<u8>>,
    /// `mService`
    service: Option<Entity>,
    /// `mDirEntityList` (the find's)
    list: Vec<Entity>,
}

impl RequestOp for DirFindOp {
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        let r = Request {
            op: if self.find { op::FIND } else { op::GET_SERVICE },
            is_service: !self.find || self.is_service,
            flags: self.flags,
            mode: self.mode,
            find_flags: self.find_flags,
            path: self.path.clone(),
            name: self.name.clone(),
            display_name: self.display_name.clone(),
            net_addr: self.net_addr.as_deref(),
            objects: self.objects.iter().map(|(t, d)| Object { data_type: t, data: d.as_deref() }).collect(),
            types: self.types.iter().map(Vec::as_slice).collect(),
            ..Default::default()
        };
        (ST_RECV, Some(dirops::request(&r, field_size)))
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        let status = if !self.find || dirops::find_reads_single(self.is_service, self.find_flags) {
            let (s, e) = parse_single(response);
            if let Some(e) = e {
                self.service = Some(e);
            }
            let Some(s) = s else { return Checked::Cut };
            if self.find && s == ST_SUCCESS {
                self.list.extend(self.service.clone());
            }
            s
        } else {
            let Some(s) = parse_multi(response, &mut self.list) else { return Checked::Cut };
            if s == ST_SUCCESS && self.is_service && !self.list.is_empty() {
                self.service = Some(self.list[0].clone());
            }
            s
        };
        if status == ST_INVALID_REPLY_HEADER {
            Checked::InvalidHeader
        } else {
            Checked::Status(status)
        }
    }

    fn reset(&mut self) {
        self.service = None;
        if self.find {
            self.list.clear();
        }
    }

    fn describe(&self) -> String {
        let v = match &self.service {
            Some(e) => format!("v{{{}}}", describe_entity(e)),
            None => "vn".into(),
        };
        if self.find {
            format!("{v} l{}", describe_list(&self.list))
        } else {
            v
        }
    }

    fn entities(&self, which: i32, _index: i64) -> Vec<Entity> {
        match which {
            EE_WON_REQOP_DIR_SERVICE => self.service.iter().cloned().collect(),
            EE_WON_REQOP_DIR_LIST => self.list.clone(),
            _ => Vec::new(),
        }
    }
}

// ---- GetAccountOp, UpdateAccountOp

/// `GetAccountOp` and `UpdateAccountOp` (`update`: its new password and email).
#[derive(Debug, Default)]
pub struct AccountOp {
    update: bool,
    password: Vec<u16>,
    email: Vec<u8>,
    error: Vec<u8>,
    profile: EeWonProfile,
}

impl RequestOp for AccountOp {
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        let m = if self.update {
            db::update_account_request(field_size, &self.password, &self.email, &self.profile)
        } else {
            db::get_account_request(field_size, &self.profile)
        };
        (ST_RECV, Some(m))
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        let kind = if self.update { db::EE_WON_DB_UPDATE_ACCOUNT } else { db::EE_WON_DB_GET_ACCOUNT };
        let r = db::reply(kind, response);
        // DbAccountReply: the error string and the email the read assigned, then the cut's throw
        if r.read >= 6 {
            let at = r.error_at as usize;
            self.error = response[at..at + r.error_len as usize].to_vec();
        } else if r.error_erased {
            self.error.clear();
        }
        if !self.update {
            if r.email == 2 {
                let at = r.email_at as usize;
                self.email = response[at..at + r.email_len as usize].to_vec();
            } else if r.email == 1 {
                self.email.clear();
            }
        }
        if r.cut {
            return Checked::Cut;
        }
        if r.verdict == db::EE_WON_DB_INVALID_HEADER {
            return Checked::InvalidHeader;
        }
        if self.update || r.email != 2 {
            return Checked::Status(r.result);
        }
        self.profile.unpack(response, r.profile_at as usize);
        Checked::Status(ST_SUCCESS)
    }

    fn describe(&self) -> String {
        let mut w = Writer::new(0, true);
        self.profile.pack(&mut w, true);
        let e = if self.update { String::new() } else { format!(" e{}", hex(&self.email)) };
        format!("x{}{e} p{}", hex(&self.error), hex(w.data()))
    }

    fn bytes(&self, which: i32) -> Option<Vec<u8>> {
        match which {
            EE_WON_REQOP_ACCOUNT_EMAIL => Some(self.email.clone()),
            EE_WON_REQOP_ACCOUNT_ERROR => Some(self.error.clone()),
            _ => None,
        }
    }

    fn num_at(&self, which: i32, index: i64) -> i64 {
        match which {
            EE_WON_REQOP_ACCOUNT_NEWSLETTER => i64::from(self.profile.newsletter(index as u16)),
            _ => 0,
        }
    }

    fn set(&mut self, which: i32, num: i64, _data: &[u8]) {
        match which {
            EE_WON_REQOP_ACCOUNT_NEWSLETTER => {
                self.profile.newsletters.insert(num as u16, num & 0x1_0000 != 0);
            }
            EE_WON_REQOP_ACCOUNT_CLEAR_NEWSLETTERS => self.profile.newsletters.clear(),
            _ => {}
        }
    }
}

/// The op of `kind` (`EE_WON_REQOPS_DIR_*`, `EE_WON_REQOPS_ACCOUNT_*`) with its members in that kind's order;
/// `None` for another kind.
pub(crate) fn new_op(kind: i32, members: &[Member]) -> Option<Box<dyn RequestOp>> {
    let mut m = Members { m: members, at: 0 };
    Some(match kind {
        EE_WON_REQOPS_DIR_GET => Box::new(DirListOp { kind, requests: vec![m.query()], ..Default::default() }),
        EE_WON_REQOPS_DIR_MULTI | EE_WON_REQOPS_DIR_ALIAS => {
            let product = if kind == EE_WON_REQOPS_DIR_ALIAS { m.bytes() } else { Vec::new() };
            let n = m.num().max(0);
            let requests = (0..n).map(|_| m.query()).collect();
            Box::new(DirListOp { kind, product, requests, ..Default::default() })
        }
        EE_WON_REQOPS_DIR_FIND => {
            let is_service = m.num() != 0;
            let (flags, mode, find_flags) = (m.num() as u32, m.num() as u8, m.num() as u8);
            let (path, name, display_name) = (m.wide(), m.wide(), m.wide());
            let net_addr = m.data();
            let objects = (0..m.num().max(0))
                .map(|_| {
                    let t = m.bytes();
                    (t, m.data())
                })
                .collect();
            let types = m.types();
            Box::new(DirFindOp {
                find: true,
                is_service,
                flags,
                mode,
                find_flags,
                path,
                name,
                display_name,
                net_addr,
                objects,
                types,
                ..Default::default()
            })
        }
        EE_WON_REQOPS_DIR_SERVICE => {
            let flags = m.num() as u32;
            let (path, name) = (m.wide(), m.wide());
            let net_addr = m.data();
            Box::new(DirFindOp { flags, path, name, net_addr, types: m.types(), ..Default::default() })
        }
        EE_WON_REQOPS_ACCOUNT_GET => Box::new(AccountOp { profile: m.profile(), ..Default::default() }),
        EE_WON_REQOPS_ACCOUNT_UPDATE => {
            let (password, email) = (m.wide(), m.bytes());
            Box::new(AccountOp { update: true, password, email, profile: m.profile(), ..Default::default() })
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "reqdir_tests.rs"]
mod tests;
