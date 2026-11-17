//! Round 12's ops derived from `ServerRequestOp` in Rust ([`RequestOp`]), the last two the C++ derived:
//! WONDB's `CreateAccountOp` (its server key, session key and account data) and WONRouting's
//! `RoutingGetGroupListReqOp` (a routing server's room list without a connection). Their C++ classes are deleted,
//! and with them the C++ `ServerRequestOp` they built on; a `RustServerOp` (`RustServerOp::NewCreateAccount`,
//! `NewGroupListReq` over [`crate::reqops::ee_won_reqops_new`]) runs one.
//!
//! - The keys are Rust's objects ([`ElGamal`], [`Blowfish`]): the op makes its 8-byte session key when it is
//!   made, from a generator seeded from the system (the C++ class's `ElGamal` member was made, with its own
//!   generator, before the key; the order is kept, so a driver's fixed seed gives the same bytes on both sides).
//! - A server key that does not load (`SetPublicKey` false) sets the server error `WS_InvalidPublicKey` and asks
//!   for the next server ([`Checked::ServerError`]).
//! - The room list: the original applied each reply's records to its inner `RoutingGetGroupListOp`'s group map
//!   (`RouteGroupListReqReply`, include/ee/wonapi/WONRoutingMsg.h). This op keeps every reply it checked, in
//!   order ([`EE_WON_REQOP_GROUP_REPLIES`], [`EE_WON_REQOP_GROUP_REPLY`]); its one reader
//!   (`RoutingLogic::QueryServerGroupsDone`, through `RouteGroupMapOf`) applies them again, with the same
//!   statements, to a map for `LobbyServer::SyncGroups`. A reply's events that are not a record's set the inner
//!   op's flags, which the next request sends: they set [`GroupListReqOp`]'s here.
//! - A NULL reply (which the original dereferenced) reads as empty.
//!
//! `golden_reqlast.txt` was printed by `tests/won/won_reqlast_test.cpp` over the original's statements before the
//! port (its generator's seed fixed through `ee_won_random_test_seed`). Reached by nothing in the game but an
//! Internet login (the lobby's create account form and its server room counts; the servers are gone).

use crate::adminop::Member;
use crate::blowfish::Blowfish;
use crate::db::{self, EeWonProfile};
use crate::elgamal::ElGamal;
use crate::random::Random;
use crate::reqop::{hex, Checked, RequestOp, ST_RECV};
use crate::routemsg::{self as rt, EeWonRouteEvent};
use crate::text::utf16_from_le_bytes;
use crate::wire::Writer;

/// `CreateAccountOp`: the user name and password (UTF-16 LE), the email, the profile (as the account ops':
/// the newsletter count and each id and subscription; the birth month, day and year; the custom action id,
/// has-data and data)
pub const EE_WON_REQOPS_CREATE_ACCOUNT: i32 = 14;
/// `RoutingGetGroupListReqOp`: the group list flags (its inner op's `mFlags`)
pub const EE_WON_REQOPS_GROUP_LIST: i32 = 15;

/// the create account's `mErrorString` (bytes)
pub const EE_WON_REQOP_CREATE_ERROR: i32 = 48;
/// the group list's replies checked (number)
pub const EE_WON_REQOP_GROUP_REPLIES: i32 = 49;
/// a reply the group list checked (bytes; the element: its index, in order)
pub const EE_WON_REQOP_GROUP_REPLY: i32 = 50;

/// `WS_InvalidPublicKey`
const ST_INVALID_PUBLIC_KEY: i32 = 100011;

/// `CreateAccountOp`'s `mState`
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
enum State {
    #[default]
    GettingPubKey,
    SettingSessionKey,
    CreatingAccount,
}

/// `CreateAccountOp`.
pub struct CreateAccountOp {
    user: Vec<u16>,
    password: Vec<u16>,
    email: Vec<u8>,
    profile: EeWonProfile,
    error: Vec<u8>,
    pub_key: ElGamal,
    sym_key: Blowfish,
    state: State,
}

impl CreateAccountOp {
    /// The constructor and `Init`: no server key (its generator made first), then a new 8-byte session key.
    pub fn new(user: Vec<u16>, password: Vec<u16>, email: Vec<u8>, profile: EeWonProfile) -> CreateAccountOp {
        let key_random = Random::new();
        CreateAccountOp::with_random(user, password, email, profile, key_random, Random::new())
    }

    /// `new` with the server key's generator and the session key's given (the golden replay).
    pub fn with_random(
        user: Vec<u16>,
        password: Vec<u16>,
        email: Vec<u8>,
        profile: EeWonProfile,
        key_random: Random,
        session_random: Random,
    ) -> CreateAccountOp {
        let pub_key = ElGamal::with_random(key_random);
        let mut sym_key = Blowfish::default();
        sym_key.create_with(8, session_random);
        CreateAccountOp { user, password, email, profile, error: Vec::new(), pub_key, sym_key, state: State::default() }
    }
}

impl RequestOp for CreateAccountOp {
    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        match self.state {
            State::GettingPubKey => (ST_RECV, Some(db::create_key_request(field_size))),
            State::SettingSessionKey => {
                let encrypted = self.pub_key.encrypt(self.sym_key.key());
                let m = db::create_session_request(field_size, encrypted.as_deref());
                self.state = State::CreatingAccount;
                (crate::server::st::SEND, Some(m))
            }
            State::CreatingAccount => {
                let plain = db::create_account_data(&self.user, &self.password, &self.email, &self.profile);
                let encrypted = self.sym_key.encrypt(&plain);
                (ST_RECV, Some(db::create_account_request(field_size, encrypted.as_deref())))
            }
        }
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        let kind = if self.state == State::GettingPubKey { db::EE_WON_DB_CREATE_KEY } else { db::EE_WON_DB_CREATE };
        let r = db::reply(kind, response);
        // DbAccountReply: the error string the read assigned, then the cut's throw
        if r.read >= 6 {
            let at = r.error_at as usize;
            self.error = response[at..at + r.error_len as usize].to_vec();
        } else if r.error_erased {
            self.error.clear();
        }
        if r.cut {
            return Checked::Cut;
        }
        if r.verdict == db::EE_WON_DB_INVALID_HEADER {
            return Checked::InvalidHeader;
        }
        if r.verdict == db::EE_WON_DB_PUBLIC_KEY {
            let at = r.key_at as usize;
            if !self.pub_key.set_public_key(&response[at..at + r.key_len as usize]) {
                return Checked::ServerError(ST_INVALID_PUBLIC_KEY);
            }
            self.state = State::SettingSessionKey;
            return Checked::Status(crate::server::st::SEND);
        }
        Checked::Status(r.result)
    }

    fn reset(&mut self) {
        self.state = State::GettingPubKey;
    }

    fn describe(&self) -> String {
        let k = self.pub_key.raw_public_key().unwrap_or_default();
        format!("s{} x{} k{} b{}", self.state as i32, hex(&self.error), hex(&k), hex(self.sym_key.key()))
    }

    fn bytes(&self, which: i32) -> Option<Vec<u8>> {
        (which == EE_WON_REQOP_CREATE_ERROR).then(|| self.error.clone())
    }
}

/// `RoutingGetGroupListReqOp`: the inner op's flags and the replies checked.
#[derive(Debug, Default)]
pub struct GroupListReqOp {
    flags: u16,
    replies: Vec<Vec<u8>>,
}

/// Whether `RouteRecords::Apply` takes the event (a record's, or one of its four actions); the others set the
/// inner op's flags.
fn applied(e: &EeWonRouteEvent) -> bool {
    match e.tag {
        rt::EE_WON_RT_NEW
        | rt::EE_WON_RT_REC
        | rt::EE_WON_RT_REC_TEXT
        | rt::EE_WON_RT_REC_NARROW
        | rt::EE_WON_RT_REC_BUF
        | rt::EE_WON_RT_INSERT => true,
        rt::EE_WON_RT_ACT => matches!(
            e.slot,
            rt::EE_WON_RT_ACT_CLEAR_GROUPS
                | rt::EE_WON_RT_ACT_CLEAR_BAD_USERS
                | rt::EE_WON_RT_ACT_COMMIT_CLIENTS
                | rt::EE_WON_RT_ACT_COMMIT_MEMBERS
        ),
        _ => false,
    }
}

impl RequestOp for GroupListReqOp {
    fn length_field_size(&self) -> u8 {
        2
    }

    fn next_request(&mut self, field_size: u8) -> (i32, Option<Vec<u8>>) {
        let q = rt::Request { kind: rt::EE_WON_RT_REQ_GROUP_LIST_REQ, flags16: self.flags, ..Default::default() };
        let mut w = Writer::new(field_size, true);
        w.append_bytes(&rt::request(&q));
        (ST_RECV, Some(w.to_message()))
    }

    fn check_response(&mut self, response: &[u8]) -> Checked {
        let r = rt::reply(rt::EE_WON_RT_GROUP_LIST_REQ, 0, response, 0, &rt::EeWonRouteState::default());
        self.replies.push(response.to_vec());
        let mut invalid = false;
        for e in &r.events {
            if e.tag == rt::EE_WON_RT_ACT && e.slot == rt::EE_WON_RT_ACT_INVALID_HEADER {
                invalid = true;
            } else if !applied(e) {
                self.flags = e.num as u16;
            }
        }
        match r.end {
            rt::End::Cut => Checked::Cut,
            rt::End::Status(_) if invalid => Checked::InvalidHeader,
            rt::End::Status(s) => Checked::Status(s),
        }
    }

    fn describe(&self) -> String {
        format!("f{} r{}", self.flags, self.replies.len())
    }

    fn num(&self, which: i32) -> i64 {
        if which == EE_WON_REQOP_GROUP_REPLIES {
            self.replies.len() as i64
        } else {
            0
        }
    }

    fn bytes_at(&self, which: i32, index: i64) -> Option<Vec<u8>> {
        if which != EE_WON_REQOP_GROUP_REPLY {
            return None;
        }
        usize::try_from(index).ok().and_then(|i| self.replies.get(i)).cloned()
    }
}

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
    fn wide(&mut self) -> Vec<u16> {
        utf16_from_le_bytes(&self.next().bytes)
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
        let present = self.num() != 0;
        let d = self.next().bytes;
        p.custom_action_data = present.then_some(d);
        p
    }
}

/// `CreateAccountOp` of its members, its generators the system's or (the golden replay) the `n`th and the next
/// system seeds after a fixed base.
fn create_account(m: &mut Members, seeds: Option<(i64, u32)>) -> CreateAccountOp {
    let (user, password) = (m.wide(), m.wide());
    let email = m.next().bytes;
    let profile = m.profile();
    match seeds {
        None => CreateAccountOp::new(user, password, email, profile),
        Some((base, n)) => {
            let key = Random::from_seed(&crate::random::test_seed(base, n));
            let session = Random::from_seed(&crate::random::test_seed(base, n + 1));
            CreateAccountOp::with_random(user, password, email, profile, key, session)
        }
    }
}

/// The op of `kind` (`EE_WON_REQOPS_CREATE_ACCOUNT`, `EE_WON_REQOPS_GROUP_LIST`) with its members in that kind's
/// order; `None` for another kind.
pub(crate) fn new_op(kind: i32, members: &[Member]) -> Option<Box<dyn RequestOp>> {
    let mut m = Members { m: members, at: 0 };
    Some(match kind {
        EE_WON_REQOPS_CREATE_ACCOUNT => Box::new(create_account(&mut m, None)),
        EE_WON_REQOPS_GROUP_LIST => Box::new(GroupListReqOp { flags: m.num() as u16, replies: Vec::new() }),
        _ => return None,
    })
}

#[cfg(test)]
#[path = "reqlast_tests.rs"]
mod tests;
