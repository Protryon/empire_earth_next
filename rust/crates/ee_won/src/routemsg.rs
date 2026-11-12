//! The routing server's protocol (`src/Empire Earth/WONAPI/WONRouting`): every request the routing ops build
//! (`SendRequest`, `RoutingGetUserStatusOp::RunHook`, `RoutingGetGroupListReqOp::GetNextRequest`), every reply and
//! notification they read (`HandleReply`, `ParseReplyExceptForStatus`, `ParseExClientData`, `CheckResponse`), the
//! connection's message header and reply dispatch rule (`RoutingConnection::MsgCallback`) and its table of
//! unsolicited messages (`GetUnsolicitedOp`).
//!
//! The ops stay C++ objects (they are `AsyncOp`s on a C++ connection); they hand their fields over for a request
//! and get a reply back as a list of assignments in the order the original made them ([`Event`]), which they
//! apply to their members and records (`include/ee/wonapi/WONRoutingMsg.h`). A reply cut short ends with the
//! assignments made before the original's `ReadBufferException`: a string whose length was read but not its
//! characters is assigned empty (the original erased it before the read failed). Reached by nothing in the game:
//! the routing servers are gone.

use std::ffi::c_void;

use crate::dirops::EeWonDirBytes;
use crate::ffi::EeWonByteSink;
use crate::wire::{PastEnd, Reader, Writer};

// ---- statuses

pub const EE_WON_RT_SUCCESS: i32 = 0;
/// `WS_None`
pub const EE_WON_RT_NONE: i32 = 100004;
/// `WS_InvalidMessage`
pub const EE_WON_RT_INVALID_MESSAGE: i32 = 100006;
/// `WS_MessageUnpackFailure`
pub const EE_WON_RT_UNPACK_FAILURE: i32 = 100012;
/// `WS_RoutingOp_DontWantReply`
pub const EE_WON_RT_DONT_WANT: i32 = 100104;
/// `WS_RoutingOp_NeedMoreReplies`
pub const EE_WON_RT_NEED_MORE: i32 = 100105;

// ---- requests (`EeWonRouteRequest::kind`)

pub const EE_WON_RT_REQ_HEADER: i32 = 0;
pub const EE_WON_RT_REQ_BAN: i32 = 1;
pub const EE_WON_RT_REQ_MUTE: i32 = 2;
pub const EE_WON_RT_REQ_CANCEL_JOIN: i32 = 3;
pub const EE_WON_RT_REQ_LEAVE: i32 = 4;
pub const EE_WON_RT_REQ_SET_GROUP_FLAGS: i32 = 5;
pub const EE_WON_RT_REQ_SERVER_ALERT: i32 = 6;
pub const EE_WON_RT_REQ_START_SHUTDOWN: i32 = 7;
pub const EE_WON_RT_REQ_ABORT_SHUTDOWN: i32 = 8;
pub const EE_WON_RT_REQ_SET_CLIENT_FLAGS: i32 = 9;
pub const EE_WON_RT_REQ_SEND_CHAT: i32 = 10;
pub const EE_WON_RT_REQ_SEND_DATA: i32 = 11;
pub const EE_WON_RT_REQ_BECOME_MODERATOR: i32 = 12;
pub const EE_WON_RT_REQ_ACCEPT_CLIENT: i32 = 13;
pub const EE_WON_RT_REQ_CREATE_DATA: i32 = 14;
pub const EE_WON_RT_REQ_DELETE_DATA: i32 = 15;
pub const EE_WON_RT_REQ_INVITE: i32 = 16;
pub const EE_WON_RT_REQ_MODIFY_DATA: i32 = 17;
pub const EE_WON_RT_REQ_READ_DATA: i32 = 18;
pub const EE_WON_RT_REQ_UNSUBSCRIBE: i32 = 19;
pub const EE_WON_RT_REQ_CREATE_GROUP: i32 = 20;
pub const EE_WON_RT_REQ_JOIN_GROUP: i32 = 21;
pub const EE_WON_RT_REQ_REGISTER: i32 = 22;
pub const EE_WON_RT_REQ_CLIENT_LIST: i32 = 23;
pub const EE_WON_RT_REQ_GROUP_LIST: i32 = 24;
pub const EE_WON_RT_REQ_MEMBERS: i32 = 25;
pub const EE_WON_RT_REQ_BAD_USERS: i32 = 26;
pub const EE_WON_RT_REQ_FIREWALL: i32 = 27;
pub const EE_WON_RT_REQ_SET_GROUP_NAME: i32 = 28;
pub const EE_WON_RT_REQ_SET_GROUP_PASSWORD: i32 = 29;
pub const EE_WON_RT_REQ_SET_GROUP_MAX_PLAYERS: i32 = 30;
pub const EE_WON_RT_REQ_RELINQUISH_CAPTAIN: i32 = 31;
pub const EE_WON_RT_REQ_DISCONNECT: i32 = 32;
pub const EE_WON_RT_REQ_USER_STATUS: i32 = 33;
pub const EE_WON_RT_REQ_GROUP_LIST_REQ: i32 = 34;

/// A request's fields; each kind reads its own (named after the op members they come from).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Request<'a> {
    pub kind: i32,
    /// `mIsDerivedServerOp`: service 8, else 7
    pub derived: bool,
    pub group_id: u16,
    pub client_id: u16,
    pub link_id: u16,
    pub parent_id: u16,
    pub max_players: u16,
    pub offset: u16,
    pub port: u16,
    /// a 16-bit flags member (chat flags, the group list's flags)
    pub flags16: u16,
    /// the first byte member (join flags, data flags, list type, read flag, a header's message type)
    pub byte1: u8,
    /// the second (member flags, the bad-user list's flags)
    pub byte2: u8,
    /// the op's on/off member (banned, muted, leave if joined, accepted, invited, moderator, insert, subscribe)
    pub on: bool,
    pub by_won_id: bool,
    pub is_cdkey: bool,
    pub is_ex: bool,
    /// the op's 32-bit members in request order
    pub longs: [u32; 4],
    pub text1: Vec<u16>,
    pub text2: Vec<u16>,
    pub narrow: &'a [u8],
    pub data: Option<&'a [u8]>,
    /// the recipients (chat, data, alert)
    pub ids: Vec<u16>,
    /// the user status query's WON ids
    pub user_ids: Vec<u32>,
}

fn head(w: &mut Writer, r: &Request, message: u8) {
    w.append_byte(3);
    w.append_byte(if r.derived { 8 } else { 7 });
    w.append_byte(message);
}

fn recipients(w: &mut Writer, ids: &[u16]) {
    w.append_short(ids.len() as i16);
    for &i in ids {
        w.append_short(i as i16);
    }
}

fn ban_target(w: &mut Writer, r: &Request) {
    if r.by_won_id {
        w.append_byte(if r.is_cdkey { 3 } else { 2 });
        w.append_long(r.longs[1] as i32);
    } else {
        w.append_byte(1);
        w.append_short(r.client_id as i16);
    }
}

/// A request's message (the header and body; the C++ write buffer adds its length field when it sends).
pub fn request(r: &Request) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    let s = |v: u16| v as i16;
    match r.kind {
        EE_WON_RT_REQ_HEADER => head(&mut w, r, r.byte1),
        EE_WON_RT_REQ_BAN | EE_WON_RT_REQ_MUTE => {
            head(&mut w, r, if r.kind == EE_WON_RT_REQ_BAN { 3 } else { 46 });
            w.append_short(s(r.group_id));
            w.append_bool(r.on);
            w.append_long(r.longs[0] as i32);
            w.append_wstring(&r.text1, 2);
            ban_target(&mut w, r);
        }
        EE_WON_RT_REQ_CANCEL_JOIN => {
            head(&mut w, r, 5);
            w.append_short(s(r.group_id));
            w.append_bool(r.on);
        }
        EE_WON_RT_REQ_LEAVE => {
            head(&mut w, r, 43);
            w.append_short(s(r.group_id));
        }
        EE_WON_RT_REQ_SET_GROUP_FLAGS => {
            head(&mut w, r, 61);
            w.append_short(s(r.group_id));
            for &l in &r.longs {
                w.append_long(l as i32);
            }
        }
        EE_WON_RT_REQ_SERVER_ALERT => {
            head(&mut w, r, 56);
            w.append_wstring(&r.text1, 2);
            recipients(&mut w, &r.ids);
        }
        EE_WON_RT_REQ_START_SHUTDOWN => {
            head(&mut w, r, 65);
            w.append_wstring(&r.text1, 2);
            w.append_long(r.longs[0] as i32);
        }
        EE_WON_RT_REQ_ABORT_SHUTDOWN => head(&mut w, r, 1),
        EE_WON_RT_REQ_SET_CLIENT_FLAGS => {
            head(&mut w, r, 60);
            for &l in &r.longs {
                w.append_long(l as i32);
            }
        }
        EE_WON_RT_REQ_SEND_CHAT => {
            head(&mut w, r, 54);
            w.append_short(s(r.flags16));
            w.append_wstring(&r.text1, 2);
            recipients(&mut w, &r.ids);
        }
        EE_WON_RT_REQ_SEND_DATA => {
            head(&mut w, r, 55);
            w.append_byte(r.byte1);
            w.append_buffer(r.data, 2);
            recipients(&mut w, &r.ids);
        }
        EE_WON_RT_REQ_BECOME_MODERATOR => {
            head(&mut w, r, 4);
            w.append_bool(r.on);
        }
        EE_WON_RT_REQ_ACCEPT_CLIENT | EE_WON_RT_REQ_INVITE => {
            head(&mut w, r, if r.kind == EE_WON_RT_REQ_ACCEPT_CLIENT { 2 } else { 40 });
            w.append_short(s(r.group_id));
            w.append_short(s(r.client_id));
            w.append_bool(r.on);
            w.append_wstring(&r.text1, 2);
        }
        EE_WON_RT_REQ_CREATE_DATA => {
            head(&mut w, r, 11);
            w.append_byte(r.byte1);
            w.append_short(s(r.link_id));
            w.append_string(r.narrow, 1);
            w.append_wstring(&r.text1, 1);
            w.append_buffer(r.data, 2);
        }
        EE_WON_RT_REQ_DELETE_DATA => {
            head(&mut w, r, 17);
            w.append_short(s(r.link_id));
            w.append_string(r.narrow, 1);
            w.append_wstring(&r.text1, 1);
        }
        EE_WON_RT_REQ_MODIFY_DATA => {
            head(&mut w, r, 45);
            w.append_short(s(r.link_id));
            w.append_string(r.narrow, 1);
            w.append_wstring(&r.text1, 1);
            w.append_short(s(r.offset));
            w.append_bool(r.on);
            w.append_buffer(r.data, 2);
        }
        EE_WON_RT_REQ_READ_DATA | EE_WON_RT_REQ_UNSUBSCRIBE => {
            let t = if r.kind == EE_WON_RT_REQ_UNSUBSCRIBE {
                68
            } else if r.on {
                67
            } else {
                50
            };
            head(&mut w, r, t);
            w.append_short(s(r.link_id));
            w.append_string(r.narrow, 1);
            w.append_byte(r.byte1);
        }
        EE_WON_RT_REQ_CREATE_GROUP => {
            head(&mut w, r, 13);
            w.append_short(s(r.group_id));
            w.append_wstring(&r.text1, 2);
            w.append_wstring(&r.text2, 2);
            w.append_short(s(r.max_players));
            w.append_byte(r.byte1);
            w.append_long(r.longs[0] as i32);
            w.append_long(r.longs[1] as i32);
            w.append_short(s(r.parent_id));
        }
        EE_WON_RT_REQ_JOIN_GROUP => {
            head(&mut w, r, 42);
            w.append_short(s(r.group_id));
            w.append_wstring(&r.text1, 2);
            w.append_wstring(&r.text2, 2);
            w.append_byte(r.byte1);
            w.append_byte(r.byte2);
        }
        EE_WON_RT_REQ_REGISTER => {
            head(&mut w, r, 52);
            w.append_wstring(&r.text1, 2);
            w.append_wstring(&r.text2, 2);
            for &l in &r.longs {
                w.append_long(l as i32);
            }
        }
        EE_WON_RT_REQ_CLIENT_LIST => {
            head(&mut w, r, 24);
            if r.is_ex {
                w.append_bool(true);
            }
        }
        EE_WON_RT_REQ_GROUP_LIST => {
            head(&mut w, r, 26);
            w.append_short(s(r.flags16));
            w.append_short(s(r.parent_id));
        }
        EE_WON_RT_REQ_MEMBERS => {
            head(&mut w, r, 30);
            w.append_short(s(r.group_id));
        }
        EE_WON_RT_REQ_BAD_USERS => {
            head(&mut w, r, 22);
            w.append_byte(r.byte1);
            w.append_short(s(r.group_id));
            w.append_byte(r.byte2);
        }
        EE_WON_RT_REQ_FIREWALL => {
            head(&mut w, r, 18);
            w.append_short(s(r.port));
            w.append_long(r.longs[0] as i32);
        }
        EE_WON_RT_REQ_SET_GROUP_NAME | EE_WON_RT_REQ_SET_GROUP_PASSWORD => {
            head(&mut w, r, if r.kind == EE_WON_RT_REQ_SET_GROUP_NAME { 63 } else { 64 });
            w.append_short(s(r.group_id));
            w.append_wstring(&r.text1, 2);
        }
        EE_WON_RT_REQ_SET_GROUP_MAX_PLAYERS => {
            head(&mut w, r, 62);
            w.append_short(s(r.group_id));
            w.append_short(s(r.max_players));
        }
        EE_WON_RT_REQ_RELINQUISH_CAPTAIN => {
            head(&mut w, r, 53);
            w.append_short(s(r.group_id));
            w.append_short(s(r.client_id));
        }
        EE_WON_RT_REQ_DISCONNECT => head(&mut w, r, 20),
        EE_WON_RT_REQ_USER_STATUS => {
            // a datagram to the routing service itself: never a derived service's header
            w.append_byte(3);
            w.append_byte(7);
            w.append_byte(72);
            w.append_byte(r.user_ids.len() as u8);
            for &id in &r.user_ids {
                w.append_long(id as i32);
            }
        }
        EE_WON_RT_REQ_GROUP_LIST_REQ => {
            w.append_byte(3);
            w.append_byte(7);
            w.append_byte(26);
            // the original casts the flags to a byte before appending a short
            w.append_short(i16::from(r.flags16 as u8));
        }
        _ => {}
    }
    w.data().to_vec()
}

// ---- replies (`ee_won_route_reply`'s kind)

/// A status reply (message 66): every op that only waits for the server's status
pub const EE_WON_RT_STATUS: i32 = 0;
pub const EE_WON_RT_GROUP_LIST: i32 = 1;
/// `RoutingGetGroupListOp::ParseGroupListObj`: a group list body without type or status
pub const EE_WON_RT_GROUP_LIST_BODY: i32 = 2;
pub const EE_WON_RT_READ_DATA: i32 = 3;
pub const EE_WON_RT_CREATE_GROUP: i32 = 4;
pub const EE_WON_RT_JOIN_GROUP: i32 = 5;
pub const EE_WON_RT_REGISTER: i32 = 6;
pub const EE_WON_RT_CLIENT_LIST: i32 = 7;
pub const EE_WON_RT_MEMBERS: i32 = 8;
pub const EE_WON_RT_BAD_USERS: i32 = 9;
pub const EE_WON_RT_FIREWALL: i32 = 10;
pub const EE_WON_RT_DERIVED: i32 = 11;
/// `RoutingGetUserStatusOp::CheckResponse`: the whole datagram
pub const EE_WON_RT_USER_STATUS: i32 = 12;
/// `RoutingGetGroupListReqOp::CheckResponse`: the whole reply
pub const EE_WON_RT_GROUP_LIST_REQ: i32 = 13;
// the notifications (unsolicited ops), in `GetUnsolicitedOp`'s order
pub const EE_WON_RT_N_CLIENT_FLAGS: i32 = 20;
pub const EE_WON_RT_N_CLIENT_JOINED_GROUP: i32 = 21;
pub const EE_WON_RT_N_CLIENT_JOINED_SERVER: i32 = 22;
pub const EE_WON_RT_N_CLIENT_LEFT_GROUP: i32 = 23;
pub const EE_WON_RT_N_CLIENT_LEFT_SERVER: i32 = 24;
pub const EE_WON_RT_N_DATA_CREATED: i32 = 25;
pub const EE_WON_RT_N_DATA_DELETED: i32 = 26;
pub const EE_WON_RT_N_DATA_MODIFIED: i32 = 27;
pub const EE_WON_RT_N_CAPTAIN_CHANGED: i32 = 28;
pub const EE_WON_RT_N_GROUP_CREATED: i32 = 29;
pub const EE_WON_RT_N_GROUP_DELETED: i32 = 30;
pub const EE_WON_RT_N_GROUP_FLAGS: i32 = 31;
pub const EE_WON_RT_N_INVITATION: i32 = 32;
pub const EE_WON_RT_N_JOIN_ATTEMPT: i32 = 33;
pub const EE_WON_RT_N_MAX_PLAYERS: i32 = 34;
pub const EE_WON_RT_N_MEMBER_COUNT: i32 = 35;
pub const EE_WON_RT_N_GROUP_NAME: i32 = 36;
pub const EE_WON_RT_N_OBSERVER_COUNT: i32 = 37;
pub const EE_WON_RT_N_MEMBER_FLAGS: i32 = 38;
pub const EE_WON_RT_N_PEER_CHAT: i32 = 39;
pub const EE_WON_RT_N_PEER_DATA: i32 = 40;
pub const EE_WON_RT_N_SERVER_ALERT: i32 = 41;
pub const EE_WON_RT_N_SHUTDOWN_ABORTED: i32 = 42;
pub const EE_WON_RT_N_SHUTDOWN_STARTED: i32 = 43;
pub const EE_WON_RT_N_BANNED: i32 = 44;
pub const EE_WON_RT_N_MUTED: i32 = 45;

/// `GetUnsolicitedOp`'s table: a routing message type's notification (`None`: no unsolicited op).
pub fn unsolicited(message: u8) -> Option<i32> {
    NOTICES.iter().find(|n| n.1 == message).map(|n| n.0)
}

// ---- the assignments a reply makes (`Event::tag`)

/// the op's numeric member `slot` = `num`
pub const EE_WON_RT_FIELD: u8 = 1;
/// the op's wide string member `slot` = the UTF-16 units at `at` (`len` units)
pub const EE_WON_RT_TEXT: u8 = 2;
/// the op's narrow string member `slot` = the bytes at `at`
pub const EE_WON_RT_NARROW: u8 = 3;
/// the op's buffer member `slot` = a new buffer of the bytes at `at`
pub const EE_WON_RT_BUF: u8 = 4;
/// a new record of kind `slot` (from the op's factory) becomes the current one of its kind
pub const EE_WON_RT_NEW: u8 = 5;
/// the current record's numeric field `slot` = `num`
pub const EE_WON_RT_REC: u8 = 6;
/// the current record's wide string field `slot`
pub const EE_WON_RT_REC_TEXT: u8 = 7;
/// the current record's narrow string field `slot`
pub const EE_WON_RT_REC_NARROW: u8 = 8;
/// the current record's buffer field `slot`
pub const EE_WON_RT_REC_BUF: u8 = 9;
/// the current record goes into the container `slot`
pub const EE_WON_RT_INSERT: u8 = 10;
/// an action `slot` (with `num`)
pub const EE_WON_RT_ACT: u8 = 11;

// record kinds (`EE_WON_RT_NEW`)
/// a client (`GetNewClientInfo`)
pub const EE_WON_RT_KIND_CLIENT: u8 = 0;
/// a member (`GetNewMemberInfo`)
pub const EE_WON_RT_KIND_MEMBER: u8 = 1;
/// a group (`GetNewGroupInfo`)
pub const EE_WON_RT_KIND_GROUP: u8 = 2;
/// a data object (`new RoutingDataObject`)
pub const EE_WON_RT_KIND_OBJECT: u8 = 3;
/// a bad user (a default `RoutingBadUser`)
pub const EE_WON_RT_KIND_BAD_USER: u8 = 4;
/// the current member's client: `member->mClientInfo = GetNewClientInfo()`, now the current client
pub const EE_WON_RT_KIND_MEMBER_CLIENT: u8 = 5;
/// the op's own client: `mClientInfo = GetNewClientInfo()`, now the current client
pub const EE_WON_RT_KIND_OP_CLIENT: u8 = 6;

// record fields (`EE_WON_RT_REC*`)
pub const EE_WON_RT_CLIENT_ID: u8 = 0;
pub const EE_WON_RT_CLIENT_NAME: u8 = 1;
pub const EE_WON_RT_CLIENT_FLAGS: u8 = 2;
pub const EE_WON_RT_CLIENT_USER_ID: u8 = 3;
pub const EE_WON_RT_CLIENT_ADDRESS: u8 = 4;
pub const EE_WON_RT_MEMBER_ID: u8 = 5;
pub const EE_WON_RT_MEMBER_FLAGS: u8 = 6;
pub const EE_WON_RT_GROUP_ID: u8 = 7;
pub const EE_WON_RT_GROUP_NAME: u8 = 8;
pub const EE_WON_RT_GROUP_CAPTAIN: u8 = 9;
pub const EE_WON_RT_GROUP_MAX_CLIENTS: u8 = 10;
pub const EE_WON_RT_GROUP_FLAGS: u8 = 11;
pub const EE_WON_RT_GROUP_ASYNC_FLAGS: u8 = 12;
pub const EE_WON_RT_GROUP_OBSERVERS: u8 = 13;
/// `mMaxPlayers`, which holds the member count
pub const EE_WON_RT_GROUP_MAX_PLAYERS: u8 = 14;
pub const EE_WON_RT_OBJECT_LINK: u8 = 15;
pub const EE_WON_RT_OBJECT_TYPE: u8 = 16;
pub const EE_WON_RT_OBJECT_NAME: u8 = 17;
pub const EE_WON_RT_OBJECT_DATA: u8 = 18;
pub const EE_WON_RT_BAD_WON_ID: u8 = 19;
pub const EE_WON_RT_BAD_NAME: u8 = 20;
pub const EE_WON_RT_BAD_EXPIRATION: u8 = 21;
pub const EE_WON_RT_BAD_MODERATOR_ID: u8 = 22;
pub const EE_WON_RT_BAD_MODERATOR_NAME: u8 = 23;
pub const EE_WON_RT_BAD_COMMENT: u8 = 24;

// containers (`EE_WON_RT_INSERT`)
/// the client map, by the client's id (`map[id] = client`)
pub const EE_WON_RT_INTO_CLIENTS: u8 = 0;
/// the member map, by the member's id
pub const EE_WON_RT_INTO_MEMBERS: u8 = 1;
/// the current group's member map
pub const EE_WON_RT_INTO_GROUP_MEMBERS: u8 = 2;
/// the group map, by the group's id
pub const EE_WON_RT_INTO_GROUPS: u8 = 3;
/// the end of the data object list
pub const EE_WON_RT_INTO_OBJECTS: u8 = 4;
/// the end of the bad user list
pub const EE_WON_RT_INTO_BAD_USERS: u8 = 5;

// actions (`EE_WON_RT_ACT`)
/// `CheckLeaveGroup(num)`
pub const EE_WON_RT_ACT_CHECK_LEAVE: u8 = 0;
/// the group map is cleared
pub const EE_WON_RT_ACT_CLEAR_GROUPS: u8 = 1;
/// the bad user list is cleared
pub const EE_WON_RT_ACT_CLEAR_BAD_USERS: u8 = 2;
/// the op's client map = the client map built so far (an inner client list op's)
pub const EE_WON_RT_ACT_COMMIT_CLIENTS: u8 = 3;
/// the op's member map = the member map built so far (an inner members op's)
pub const EE_WON_RT_ACT_COMMIT_MEMBERS: u8 = 4;
/// `++mReplyCount`
pub const EE_WON_RT_ACT_INC_REPLIES: u8 = 5;
/// `mRecipients.push_back(num)`
pub const EE_WON_RT_ACT_RECIPIENT: u8 = 6;
/// the next queried user's status = `num` (in the status map's order)
pub const EE_WON_RT_ACT_USER_STATUS: u8 = 7;
/// `InvalidReplyHeader()` (a group list request's reply of another kind)
pub const EE_WON_RT_ACT_INVALID_HEADER: u8 = 8;

/// One assignment a reply makes.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonRouteEvent {
    pub tag: u8,
    pub slot: u8,
    pub num: i32,
    /// a string's or buffer's offset in the message
    pub at: u32,
    /// its length: UTF-16 units for a wide string, else bytes
    pub len: u32,
}

/// The op state a reply's read depends on.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonRouteState {
    /// `mGroupId` (join, members)
    pub group_id: u16,
    /// `mJoinFlags` (create, join)
    pub join_flags: u8,
    /// `mRegisterFlags`
    pub register_flags: u32,
    pub reply_count: i32,
    pub replies_needed: i32,
    /// `mWaitingForStatusReply` (firewall)
    pub waiting: bool,
    /// the user status query's map size
    pub user_count: u32,
}

/// How a reply's read ended.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum End {
    /// returned this status
    Status(i32),
    /// the original threw `ReadBufferException` after the assignments
    Cut,
}

/// A reply's assignments and how the read ended (`pos`: the reader's position at the end).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Reply {
    pub events: Vec<EeWonRouteEvent>,
    pub end: End,
    pub pos: usize,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum F {
    Byte,
    Bool,
    Short,
    Long,
    WStr2,
    WStr1,
    Str1,
    Buf2,
}

/// The fixed notifications: (kind, message type, fields in order: field `i` is the op's slot `i`).
const NOTICES: &[(i32, u8, &[F])] = &[
    (EE_WON_RT_N_CLIENT_FLAGS, 6, &[F::Short, F::Long]),
    (EE_WON_RT_N_CLIENT_JOINED_GROUP, 7, &[]),
    (EE_WON_RT_N_CLIENT_JOINED_SERVER, 8, &[]),
    (EE_WON_RT_N_CLIENT_LEFT_GROUP, 9, &[F::Short, F::Short]),
    (EE_WON_RT_N_CLIENT_LEFT_SERVER, 10, &[F::Short]),
    (EE_WON_RT_N_DATA_CREATED, 14, &[F::Short, F::Str1, F::WStr1, F::Buf2]),
    (EE_WON_RT_N_DATA_DELETED, 15, &[F::Short, F::Str1, F::WStr1]),
    (EE_WON_RT_N_DATA_MODIFIED, 16, &[F::Short, F::Str1, F::WStr1, F::Short, F::Bool, F::Buf2]),
    (EE_WON_RT_N_CAPTAIN_CHANGED, 31, &[F::Short, F::Short]),
    (EE_WON_RT_N_GROUP_CREATED, 32, &[F::Short, F::WStr2, F::Short, F::Long, F::Long]),
    (EE_WON_RT_N_GROUP_DELETED, 33, &[F::Short]),
    (EE_WON_RT_N_GROUP_FLAGS, 34, &[F::Short, F::Long, F::Long]),
    (EE_WON_RT_N_INVITATION, 35, &[F::Short, F::Short, F::Bool, F::WStr2]),
    (EE_WON_RT_N_JOIN_ATTEMPT, 36, &[F::Short, F::Short, F::WStr2, F::Byte]),
    (EE_WON_RT_N_MAX_PLAYERS, 37, &[F::Short, F::Short]),
    (EE_WON_RT_N_MEMBER_COUNT, 71, &[F::Short, F::Short]),
    (EE_WON_RT_N_GROUP_NAME, 38, &[F::Short, F::WStr2]),
    (EE_WON_RT_N_OBSERVER_COUNT, 39, &[F::Short, F::Short]),
    (EE_WON_RT_N_MEMBER_FLAGS, 44, &[F::Short, F::Short, F::Byte]),
    (EE_WON_RT_N_PEER_CHAT, 47, &[]),
    (EE_WON_RT_N_PEER_DATA, 48, &[F::Short, F::Buf2]),
    (EE_WON_RT_N_SERVER_ALERT, 57, &[F::WStr2]),
    (EE_WON_RT_N_SHUTDOWN_ABORTED, 58, &[]),
    (EE_WON_RT_N_SHUTDOWN_STARTED, 59, &[F::WStr2, F::Long]),
    (EE_WON_RT_N_BANNED, 69, &[F::Short, F::Bool, F::WStr2, F::Long]),
    (EE_WON_RT_N_MUTED, 70, &[F::Short, F::Bool, F::WStr2, F::Long]),
];

type R<T> = Result<T, PastEnd>;

/// A reply being read: the reader and the assignments so far.
struct P<'a> {
    data: &'a [u8],
    r: Reader<'a>,
    ev: Vec<EeWonRouteEvent>,
}

impl<'a> P<'a> {
    fn push(&mut self, tag: u8, slot: u8, num: i32) {
        self.ev.push(EeWonRouteEvent { tag, slot, num, at: 0, len: 0 });
    }
    fn span(&mut self, tag: u8, slot: u8, at: usize, len: usize) {
        self.ev.push(EeWonRouteEvent { tag, slot, num: 0, at: at as u32, len: len as u32 });
    }
    fn short(&mut self) -> R<i32> {
        Ok(i32::from(self.r.read_short()?))
    }
    fn byte(&mut self) -> R<i32> {
        Ok(i32::from(self.r.read_byte()?))
    }
    fn long(&mut self) -> R<i32> {
        self.r.read_long()
    }
    /// `ReadWString(text, size)` into `tag`/`slot`: erased (assigned empty) once its length is read
    fn wstr(&mut self, tag: u8, slot: u8, size: u8) -> R<()> {
        let n = self.r.read_length(size)?;
        let at = self.r.pos();
        match (n as usize).checked_mul(2).and_then(|b| (b <= self.r.available()).then_some(b)) {
            Some(b) => {
                self.r.read_bytes(b)?;
                self.span(tag, slot, at, n as usize);
                Ok(())
            }
            None => {
                self.span(tag, slot, at, 0);
                Err(PastEnd)
            }
        }
    }
    /// `ReadString(text, size)`: as `wstr`, in bytes
    fn str(&mut self, tag: u8, slot: u8, size: u8) -> R<()> {
        let n = self.r.read_length(size)? as usize;
        let at = self.r.pos();
        if n > self.r.available() {
            self.span(tag, slot, at, 0);
            return Err(PastEnd);
        }
        self.r.read_bytes(n)?;
        self.span(tag, slot, at, n);
        Ok(())
    }
    /// `ReadBuf(size)`: assigned only when read whole
    fn buf(&mut self, tag: u8, slot: u8, size: u8) -> R<()> {
        let n = self.r.read_length(size)? as usize;
        let at = self.r.pos();
        self.r.read_bytes(n)?;
        self.span(tag, slot, at, n);
        Ok(())
    }
    fn field(&mut self, f: F, slot: u8) -> R<()> {
        let tag = EE_WON_RT_FIELD;
        match f {
            F::Byte => {
                let v = self.byte()?;
                self.push(tag, slot, v);
            }
            F::Bool => {
                let v = self.r.read_bool()?;
                self.push(tag, slot, i32::from(v));
            }
            F::Short => {
                let v = self.short()?;
                self.push(tag, slot, v);
            }
            F::Long => {
                let v = self.long()?;
                self.push(tag, slot, v);
            }
            F::WStr2 => self.wstr(EE_WON_RT_TEXT, slot, 2)?,
            F::WStr1 => self.wstr(EE_WON_RT_TEXT, slot, 1)?,
            F::Str1 => self.str(EE_WON_RT_NARROW, slot, 1)?,
            F::Buf2 => self.buf(EE_WON_RT_BUF, slot, 2)?,
        }
        Ok(())
    }
    fn rec(&mut self, field: u8, v: i32) {
        self.push(EE_WON_RT_REC, field, v);
    }
    /// `ReadBytes(length - pos + start)`: the rest of a sized entry (past the end when the entry overran its size)
    fn skip_rest(&mut self, length: u16, start: usize) -> R<()> {
        let used = self.r.pos() - start;
        if used > usize::from(length) {
            return Err(PastEnd);
        }
        self.r.read_bytes(usize::from(length) - used)?;
        Ok(())
    }

    /// `RoutingClientJoinedServerOp::ParseExClientData` into the current client
    fn ex_client_data(&mut self) -> R<()> {
        let count = self.r.read_byte()?;
        for _ in 0..count {
            let kind = self.r.read_byte()?;
            let length = self.r.read_byte()?;
            match kind {
                1 => {
                    let v = self.long()?;
                    self.rec(EE_WON_RT_CLIENT_USER_ID, v);
                }
                2 => {
                    let v = self.long()?;
                    self.rec(EE_WON_RT_CLIENT_ADDRESS, v);
                }
                _ => {
                    self.r.read_bytes(usize::from(length))?;
                }
            }
        }
        Ok(())
    }

    /// `RoutingGetClientListOp::HandleReply`'s body after the status (`ex`: the extended form)
    fn client_list(&mut self, ex: bool) -> R<()> {
        let count = self.short()? as u16;
        for _ in 0..count {
            self.push(EE_WON_RT_NEW, EE_WON_RT_KIND_CLIENT, 0);
            let id = self.short()?;
            self.rec(EE_WON_RT_CLIENT_ID, id);
            self.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_CLIENT_NAME, 2)?;
            let f = self.long()?;
            self.rec(EE_WON_RT_CLIENT_FLAGS, f);
            if ex {
                self.ex_client_data()?;
            }
            self.push(EE_WON_RT_INSERT, EE_WON_RT_INTO_CLIENTS, 0);
        }
        Ok(())
    }

    /// `RoutingGetGroupListOp::ParseReplyExceptForStatus`: the op's slot 0 is `mFlags`
    fn group_list(&mut self) -> R<()> {
        self.push(EE_WON_RT_ACT, EE_WON_RT_ACT_CLEAR_GROUPS, 0);
        let flags = self.short()?;
        self.push(EE_WON_RT_FIELD, 0, flags);
        let flags = flags as u16;
        let count = self.short()? as u16;
        for _ in 0..count {
            let length = self.short()? as u16;
            let start = self.r.pos();
            self.push(EE_WON_RT_NEW, EE_WON_RT_KIND_GROUP, 0);
            let v = self.short()?;
            self.rec(EE_WON_RT_GROUP_ID, v);
            if flags & 1 != 0 {
                self.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_GROUP_NAME, 2)?;
            }
            if flags & 2 != 0 {
                let v = self.short()?;
                self.rec(EE_WON_RT_GROUP_CAPTAIN, v);
            }
            if flags & 4 != 0 {
                let v = self.short()?;
                self.rec(EE_WON_RT_GROUP_MAX_CLIENTS, v);
            }
            if flags & 8 != 0 {
                let v = self.long()?;
                self.rec(EE_WON_RT_GROUP_FLAGS, v);
            }
            if flags & 0x10 != 0 {
                let v = self.long()?;
                self.rec(EE_WON_RT_GROUP_ASYNC_FLAGS, v);
            }
            if flags & 0x40 != 0 {
                let v = self.short()?;
                self.rec(EE_WON_RT_GROUP_OBSERVERS, v);
            }
            // the member count lands in mMaxPlayers; a group without it keeps its factory's 0
            let mut members = 0u16;
            if flags & 0x20 != 0 {
                let v = self.short()?;
                self.rec(EE_WON_RT_GROUP_MAX_PLAYERS, v);
                members = v as u16;
            }
            if flags & 0x80 != 0 {
                for _ in 0..members {
                    self.push(EE_WON_RT_NEW, EE_WON_RT_KIND_MEMBER, 0);
                    let id = self.short()?;
                    self.rec(EE_WON_RT_MEMBER_ID, id);
                    let f = self.byte()?;
                    self.rec(EE_WON_RT_MEMBER_FLAGS, f);
                    self.push(EE_WON_RT_INSERT, EE_WON_RT_INTO_GROUP_MEMBERS, 0);
                }
            }
            self.push(EE_WON_RT_INSERT, EE_WON_RT_INTO_GROUPS, 0);
            self.skip_rest(length, start)?;
        }
        Ok(())
    }

    /// `RoutingGetMembersOfGroupOp::HandleReply` after the type check. `inner`: the join op's temporary
    /// members op (its own fields are dropped; only its members are kept, when its status is a success).
    /// Slots: 0 mHasClientNames, 1 mHasClientFlags, 2 mObserverCount.
    fn members(&mut self, group: u16, inner: bool) -> R<i32> {
        let status = self.short()?;
        let id = self.short()? as u16;
        if id != group {
            return Ok(EE_WON_RT_DONT_WANT);
        }
        if status == 0 {
            let flags = self.r.read_byte()?;
            let names = flags & 1 != 0;
            let cflags = flags & 2 != 0;
            if !inner {
                self.push(EE_WON_RT_FIELD, 0, i32::from(names));
                self.push(EE_WON_RT_FIELD, 1, i32::from(cflags));
            }
            let observers = self.short()?;
            if !inner {
                self.push(EE_WON_RT_FIELD, 2, observers);
            }
            let count = self.short()? as u16;
            for _ in 0..count {
                let length = self.short()? as u16;
                let start = self.r.pos();
                self.push(EE_WON_RT_NEW, EE_WON_RT_KIND_MEMBER, 0);
                let v = self.short()?;
                self.rec(EE_WON_RT_MEMBER_ID, v);
                let f = self.byte()?;
                self.rec(EE_WON_RT_MEMBER_FLAGS, f);
                self.push(EE_WON_RT_INSERT, EE_WON_RT_INTO_MEMBERS, 0);
                if flags != 0 {
                    self.push(EE_WON_RT_NEW, EE_WON_RT_KIND_MEMBER_CLIENT, 0);
                }
                if names {
                    self.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_CLIENT_NAME, 2)?;
                }
                if cflags {
                    let v = self.long()?;
                    self.rec(EE_WON_RT_CLIENT_FLAGS, v);
                }
                self.skip_rest(length, start)?;
            }
        }
        if inner && status == 0 {
            self.push(EE_WON_RT_ACT, EE_WON_RT_ACT_COMMIT_MEMBERS, 0);
        }
        Ok(status)
    }
}

/// A reply of kind `kind` (message type `message`, the reader at `pos` of `data`), read as the op's
/// `HandleReply` (or `CheckResponse`, `ParseGroupListObj`) did.
pub fn reply(kind: i32, message: u8, data: &[u8], pos: usize, st: &EeWonRouteState) -> Reply {
    let mut r = Reader::new(data);
    // the op's reader is past the header (a position past the end leaves nothing to read)
    let _ = r.read_bytes(pos.min(data.len()));
    let mut p = P { data, r, ev: Vec::new() };
    let end = match read(&mut p, kind, message, st) {
        Ok(s) => End::Status(s),
        Err(PastEnd) => End::Cut,
    };
    let pos = p.r.pos();
    let end = match (kind, end) {
        // CheckResponse catches the cut: what was assigned stays
        (EE_WON_RT_USER_STATUS, End::Cut) => End::Status(EE_WON_RT_UNPACK_FAILURE),
        // ParseGroupListObj's catch: false (the request op's caller turns it into an unpack failure)
        (EE_WON_RT_GROUP_LIST_BODY, End::Cut) => End::Status(EE_WON_RT_UNPACK_FAILURE),
        (_, e) => e,
    };
    Reply { events: p.ev, end, pos }
}

fn read(p: &mut P, kind: i32, t: u8, st: &EeWonRouteState) -> R<i32> {
    const DONT: i32 = EE_WON_RT_DONT_WANT;
    match kind {
        EE_WON_RT_STATUS => {
            if t != 66 {
                return Ok(DONT);
            }
            p.short()
        }
        EE_WON_RT_GROUP_LIST => {
            if t != 25 {
                return Ok(DONT);
            }
            let status = p.short()?;
            if status == 0 {
                p.group_list()?;
            }
            Ok(status)
        }
        EE_WON_RT_GROUP_LIST_BODY => {
            p.group_list()?;
            Ok(EE_WON_RT_SUCCESS)
        }
        EE_WON_RT_GROUP_LIST_REQ => {
            let h = p.r.read_byte()?;
            let s = p.r.read_byte()?;
            let ty = p.r.read_byte()?;
            if h != 3 || s != 7 || ty != 25 {
                p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_INVALID_HEADER, 0);
                return Ok(0);
            }
            let status = p.short()?;
            if status != 0 {
                return Ok(status);
            }
            // ParseGroupListObj: a reader of its own over the rest, its cut caught
            let at = p.r.pos();
            let rest = &p.data[at..];
            let mut q = P { data: rest, r: Reader::new(rest), ev: Vec::new() };
            let ok = q.group_list().is_ok();
            for mut e in q.ev {
                if matches!(e.tag, EE_WON_RT_TEXT | EE_WON_RT_NARROW | EE_WON_RT_BUF)
                    || matches!(e.tag, EE_WON_RT_REC_TEXT | EE_WON_RT_REC_NARROW | EE_WON_RT_REC_BUF)
                {
                    e.at += at as u32;
                }
                p.ev.push(e);
            }
            Ok(if ok { EE_WON_RT_SUCCESS } else { EE_WON_RT_UNPACK_FAILURE })
        }
        EE_WON_RT_READ_DATA => {
            if t != 49 {
                return Ok(DONT);
            }
            let status = p.short()?;
            if status == 0 {
                let count = p.short()? as u16;
                for _ in 0..count {
                    p.push(EE_WON_RT_NEW, EE_WON_RT_KIND_OBJECT, 0);
                    let v = p.short()?;
                    p.rec(EE_WON_RT_OBJECT_LINK, v);
                    p.str(EE_WON_RT_REC_NARROW, EE_WON_RT_OBJECT_TYPE, 1)?;
                    p.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_OBJECT_NAME, 1)?;
                    p.buf(EE_WON_RT_REC_BUF, EE_WON_RT_OBJECT_DATA, 2)?;
                    p.push(EE_WON_RT_INSERT, EE_WON_RT_INTO_OBJECTS, 0);
                }
            }
            Ok(status)
        }
        // slots: 0 mGroupId, 1 mGroupName, 2 mParentGroupId
        EE_WON_RT_CREATE_GROUP => {
            if t == 41 {
                if st.join_flags == 0 {
                    return Ok(DONT);
                }
                let status = p.short()?;
                p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_CHECK_LEAVE, status);
                return Ok(status);
            }
            if t != 12 {
                return Ok(DONT);
            }
            let status = p.short()?;
            if status == 0 {
                let v = p.short()?;
                p.push(EE_WON_RT_FIELD, 0, v);
                p.wstr(EE_WON_RT_TEXT, 1, 2)?;
                p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_CHECK_LEAVE, status);
                if st.join_flags != 0 {
                    return Ok(EE_WON_RT_NEED_MORE);
                }
            }
            if p.r.pos() < p.data.len() {
                let v = p.short()?;
                p.push(EE_WON_RT_FIELD, 2, v);
            }
            Ok(status)
        }
        // slot 0: mJoinComment
        EE_WON_RT_JOIN_GROUP => {
            if t == 41 {
                let status = p.short()?;
                let group = p.short()? as u16;
                if group == st.group_id {
                    p.wstr(EE_WON_RT_TEXT, 0, 2)?;
                    if status == -2541 {
                        return Ok(EE_WON_RT_NEED_MORE);
                    }
                    if status != 0 {
                        return Ok(status);
                    }
                    return Ok(if st.join_flags & 1 != 0 { EE_WON_RT_NEED_MORE } else { 0 });
                }
                Ok(DONT)
            } else if t == 29 {
                p.members(st.group_id, true)
            } else {
                Ok(DONT)
            }
        }
        // slots: 0 mClientId, 1 mClientName, 2 mReconnectId
        EE_WON_RT_REGISTER => {
            let status;
            if t != 23 && t != 74 {
                if t != 51 {
                    return Ok(DONT);
                }
                status = p.short()?;
                if status == 0 {
                    let v = p.short()?;
                    p.push(EE_WON_RT_FIELD, 0, v);
                    p.wstr(EE_WON_RT_TEXT, 1, 2)?;
                    let v = p.long()?;
                    p.push(EE_WON_RT_FIELD, 2, v);
                }
            } else {
                if st.register_flags & 0x14 == 0 {
                    return Ok(DONT);
                }
                // the inner client list op's reply (its map assigned whatever its status)
                status = p.short()?;
                if status == 0 {
                    p.client_list(t == 74)?;
                }
                p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_COMMIT_CLIENTS, 0);
            }
            p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_INC_REPLIES, 0);
            if status == 0 && st.reply_count.wrapping_add(1) != st.replies_needed {
                return Ok(EE_WON_RT_NEED_MORE);
            }
            Ok(status)
        }
        // slot 0: mIsEx
        EE_WON_RT_CLIENT_LIST => {
            if t != 23 && t != 74 {
                return Ok(DONT);
            }
            p.push(EE_WON_RT_FIELD, 0, i32::from(t == 74));
            let status = p.short()?;
            if status == 0 {
                p.client_list(t == 74)?;
            }
            Ok(status)
        }
        EE_WON_RT_MEMBERS => {
            if t != 29 {
                return Ok(DONT);
            }
            p.members(st.group_id, false)
        }
        // slots: 0 mListType, 1 mGroupId, 2 mFlags
        EE_WON_RT_BAD_USERS => {
            if t != 21 {
                return Ok(DONT);
            }
            p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_CLEAR_BAD_USERS, 0);
            let status = p.short()?;
            if status == 0 {
                let v = p.r.read_byte()?;
                p.push(EE_WON_RT_FIELD, 0, i32::from(v as i8));
                let v = p.short()?;
                p.push(EE_WON_RT_FIELD, 1, v);
                let flags = p.r.read_byte()?;
                p.push(EE_WON_RT_FIELD, 2, i32::from(flags));
                let count = p.short()? as u16;
                for _ in 0..count {
                    let length = p.short()? as u16;
                    let start = p.r.pos();
                    p.push(EE_WON_RT_NEW, EE_WON_RT_KIND_BAD_USER, 0);
                    let v = p.long()?;
                    p.rec(EE_WON_RT_BAD_WON_ID, v);
                    p.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_BAD_NAME, 2)?;
                    if flags & 1 != 0 {
                        let v = p.long()?;
                        p.rec(EE_WON_RT_BAD_EXPIRATION, v);
                    }
                    if flags & 2 != 0 {
                        let v = p.long()?;
                        p.rec(EE_WON_RT_BAD_MODERATOR_ID, v);
                    }
                    if flags & 4 != 0 {
                        p.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_BAD_MODERATOR_NAME, 2)?;
                    }
                    if flags & 8 != 0 {
                        p.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_BAD_COMMENT, 2)?;
                    }
                    p.push(EE_WON_RT_INSERT, EE_WON_RT_INTO_BAD_USERS, 0);
                    p.skip_rest(length, start)?;
                }
            }
            Ok(status)
        }
        // slots: 0 mWaitingForStatusReply, 1 mListenPort
        EE_WON_RT_FIREWALL => {
            if st.waiting && t == 66 {
                p.push(EE_WON_RT_FIELD, 0, 0);
                let status = p.short()?;
                Ok(if status != 0 { status } else { EE_WON_RT_NEED_MORE })
            } else if t == 19 {
                let status = p.short()?;
                let v = p.short()?;
                p.push(EE_WON_RT_FIELD, 1, v);
                Ok(status)
            } else {
                Ok(DONT)
            }
        }
        // slots: 0 mMsgType, 1 mRecvMsg (the rest of the message)
        EE_WON_RT_DERIVED => {
            p.push(EE_WON_RT_FIELD, 0, i32::from(t));
            let at = p.r.pos();
            let n = p.r.available();
            p.span(EE_WON_RT_BUF, 1, at, n);
            Ok(EE_WON_RT_SUCCESS)
        }
        EE_WON_RT_USER_STATUS => {
            if p.r.read_byte()? != 3 || p.r.read_byte()? != 7 || p.r.read_byte()? != 73 {
                return Ok(EE_WON_RT_NONE);
            }
            let count = p.r.read_byte()?;
            if u32::from(count) != st.user_count {
                return Ok(EE_WON_RT_INVALID_MESSAGE);
            }
            for _ in 0..count {
                let v = p.byte()?;
                p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_USER_STATUS, v);
            }
            Ok(EE_WON_RT_SUCCESS)
        }
        // slots: 0 mGroupId, 1 mClientId, 2 mMemberFlags, 3 mClientName, 4 mClientFlags, 5 mHasClientFlags
        EE_WON_RT_N_CLIENT_JOINED_GROUP => {
            if t != 7 {
                return Ok(DONT);
            }
            let flags = p.r.read_byte()?;
            p.field(F::Short, 0)?;
            p.field(F::Short, 1)?;
            p.field(F::Byte, 2)?;
            if flags & 1 != 0 {
                p.wstr(EE_WON_RT_TEXT, 3, 2)?;
            }
            if flags & 2 != 0 {
                p.field(F::Long, 4)?;
                p.push(EE_WON_RT_FIELD, 5, 1);
            }
            Ok(EE_WON_RT_SUCCESS)
        }
        EE_WON_RT_N_CLIENT_JOINED_SERVER => {
            if t != 8 {
                return Ok(DONT);
            }
            p.push(EE_WON_RT_NEW, EE_WON_RT_KIND_OP_CLIENT, 0);
            let v = p.short()?;
            p.rec(EE_WON_RT_CLIENT_ID, v);
            p.wstr(EE_WON_RT_REC_TEXT, EE_WON_RT_CLIENT_NAME, 2)?;
            let v = p.long()?;
            p.rec(EE_WON_RT_CLIENT_FLAGS, v);
            if p.r.available() > 0 {
                p.ex_client_data()?;
            }
            Ok(EE_WON_RT_SUCCESS)
        }
        // slots: 0 mClientId, 1 mFlags, 2 mText; the recipients pushed
        EE_WON_RT_N_PEER_CHAT => {
            if t != 47 {
                return Ok(DONT);
            }
            p.field(F::Short, 0)?;
            p.field(F::Short, 1)?;
            p.wstr(EE_WON_RT_TEXT, 2, 2)?;
            let count = p.short()? as u16;
            for _ in 0..count {
                let v = p.short()?;
                p.push(EE_WON_RT_ACT, EE_WON_RT_ACT_RECIPIENT, v);
            }
            Ok(EE_WON_RT_SUCCESS)
        }
        _ => {
            let Some(&(_, message, fields)) = NOTICES.iter().find(|n| n.0 == kind) else {
                return Ok(DONT);
            };
            if t != message {
                return Ok(DONT);
            }
            for (i, &f) in fields.iter().enumerate() {
                p.field(f, i as u8)?;
            }
            Ok(EE_WON_RT_SUCCESS)
        }
    }
}

/// `MsgCallback`'s header: the message type, whether a derived service sent it (service other than 7) and the
/// position after it; `None` when the message is shorter than the header.
pub fn header(data: &[u8]) -> Option<(u8, bool, usize)> {
    let mut r = Reader::new(data);
    r.read_byte().ok()?;
    let service = r.read_byte().ok()?;
    let t = r.read_byte().ok()?;
    Some((t, service != 7, r.pos()))
}

/// What `MsgCallback` does with an op's `HandleReply` status: 0 the next op (the position restored), 1 stop
/// (the op waits for more), 2 finish the op with the status and stop.
pub fn dispatch(status: i32) -> i32 {
    match status {
        EE_WON_RT_NEED_MORE => 1,
        EE_WON_RT_DONT_WANT => 0,
        _ => 2,
    }
}

// ---- the C ABI

/// A request's fields from the C++ (strings as UTF-16 LE bytes).
#[repr(C)]
pub struct EeWonRouteRequest {
    pub kind: i32,
    pub derived: bool,
    pub group_id: u16,
    pub client_id: u16,
    pub link_id: u16,
    pub parent_id: u16,
    pub max_players: u16,
    pub offset: u16,
    pub port: u16,
    pub flags16: u16,
    pub byte1: u8,
    pub byte2: u8,
    pub on: bool,
    pub by_won_id: bool,
    pub is_cdkey: bool,
    pub is_ex: bool,
    pub longs: [u32; 4],
    pub text1: EeWonDirBytes,
    pub text2: EeWonDirBytes,
    pub narrow: EeWonDirBytes,
    pub data: EeWonDirBytes,
    pub ids: *const u16,
    pub id_count: usize,
    pub user_ids: *const u32,
    pub user_id_count: usize,
}

/// How a reply's read ended, for the C++.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonRouteResult {
    /// the status the op returns (when not cut)
    pub status: i32,
    /// the original threw `ReadBufferException` after the assignments
    pub cut: bool,
    /// the reader's position at the end
    pub pos: u32,
}

/// Receives a reply's assignments; valid only during the call.
pub type EeWonRouteSink = unsafe extern "C" fn(context: *mut c_void, events: *const EeWonRouteEvent, count: usize);

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the C++ hands `n` entries
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn bytes<'a>(b: &EeWonDirBytes) -> &'a [u8] {
    unsafe { slice(b.data, b.len) }
}

/// A request's message into `sink`.
///
/// # Safety
/// `r` and what it points to are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_route_request(r: *const EeWonRouteRequest, sink: EeWonByteSink, context: *mut c_void) {
    let r = unsafe { &*r };
    let q = unsafe {
        Request {
            kind: r.kind,
            derived: r.derived,
            group_id: r.group_id,
            client_id: r.client_id,
            link_id: r.link_id,
            parent_id: r.parent_id,
            max_players: r.max_players,
            offset: r.offset,
            port: r.port,
            flags16: r.flags16,
            byte1: r.byte1,
            byte2: r.byte2,
            on: r.on,
            by_won_id: r.by_won_id,
            is_cdkey: r.is_cdkey,
            is_ex: r.is_ex,
            longs: r.longs,
            text1: crate::text::utf16_from_le_bytes(bytes(&r.text1)),
            text2: crate::text::utf16_from_le_bytes(bytes(&r.text2)),
            narrow: bytes(&r.narrow),
            data: if r.data.present { Some(bytes(&r.data)) } else { None },
            ids: slice(r.ids, r.id_count).to_vec(),
            user_ids: slice(r.user_ids, r.user_id_count).to_vec(),
        }
    };
    let m = request(&q);
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// A reply's read: its assignments into `sink` (once, all of them), how it ended into `out`.
///
/// # Safety
/// `data` holds `length` bytes; `state` and `out` are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_route_reply(
    kind: i32,
    message: u8,
    data: *const u8,
    length: usize,
    pos: usize,
    state: *const EeWonRouteState,
    sink: EeWonRouteSink,
    context: *mut c_void,
    out: *mut EeWonRouteResult,
) {
    let st = unsafe { *state };
    let r = reply(kind, message, unsafe { slice(data, length) }, pos, &st);
    let (status, cut) = match r.end {
        End::Status(s) => (s, false),
        End::Cut => (0, true),
    };
    unsafe {
        *out = EeWonRouteResult { status, cut, pos: r.pos as u32 };
        sink(context, r.events.as_ptr(), r.events.len());
    }
}

/// `MsgCallback`'s header; false when the message is cut short.
///
/// # Safety
/// `data` holds `length` bytes; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_route_header(
    data: *const u8,
    length: usize,
    message: *mut u8,
    derived: *mut bool,
    pos: *mut u32,
) -> bool {
    match header(unsafe { slice(data, length) }) {
        Some((t, d, p)) => {
            unsafe {
                *message = t;
                *derived = d;
                *pos = p as u32;
            }
            true
        }
        None => false,
    }
}

/// `MsgCallback`'s rule for a `HandleReply` status (see [`dispatch`]).
#[no_mangle]
pub extern "C" fn ee_won_route_dispatch(status: i32) -> i32 {
    dispatch(status)
}

/// `GetUnsolicitedOp`'s notification for a routing message type, or -1.
#[no_mangle]
pub extern "C" fn ee_won_route_unsolicited(message: u8) -> i32 {
    unsolicited(message).unwrap_or(-1)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn st() -> EeWonRouteState {
        EeWonRouteState::default()
    }

    fn le16(v: u16) -> [u8; 2] {
        v.to_le_bytes()
    }

    #[test]
    fn requests_have_the_header_and_fields() {
        let q = Request { kind: EE_WON_RT_REQ_LEAVE, group_id: 0x1234, ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 43, 0x34, 0x12]);
        let q = Request { kind: EE_WON_RT_REQ_LEAVE, derived: true, group_id: 1, ..Default::default() };
        assert_eq!(request(&q), vec![3, 8, 43, 1, 0]);
        let q = Request {
            kind: EE_WON_RT_REQ_BAN,
            group_id: 0xffff,
            on: true,
            longs: [5, 0x01020304, 0, 0],
            text1: vec![0x41],
            by_won_id: true,
            is_cdkey: true,
            ..Default::default()
        };
        assert_eq!(request(&q), vec![3, 7, 3, 0xff, 0xff, 1, 5, 0, 0, 0, 1, 0, 0x41, 0, 3, 4, 3, 2, 1]);
        let q = Request { kind: EE_WON_RT_REQ_MUTE, client_id: 9, ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 46, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 9, 0]);
        let q = Request { kind: EE_WON_RT_REQ_SEND_CHAT, flags16: 1, ids: vec![2, 3], ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 54, 1, 0, 0, 0, 2, 0, 2, 0, 3, 0]);
        let q = Request { kind: EE_WON_RT_REQ_SEND_DATA, data: None, ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 55, 0, 0, 0, 0, 0]);
        let q = Request { kind: EE_WON_RT_REQ_READ_DATA, on: true, narrow: b"ab", byte1: 7, ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 67, 0, 0, 2, b'a', b'b', 7]);
        let q = Request { kind: EE_WON_RT_REQ_CLIENT_LIST, is_ex: true, ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 24, 1]);
        let q = Request { kind: EE_WON_RT_REQ_USER_STATUS, derived: true, user_ids: vec![1, 2], ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 72, 2, 1, 0, 0, 0, 2, 0, 0, 0]);
        let q = Request { kind: EE_WON_RT_REQ_GROUP_LIST_REQ, flags16: 0x1ff, ..Default::default() };
        assert_eq!(request(&q), vec![3, 7, 26, 0xff, 0]);
        let q = Request { kind: EE_WON_RT_REQ_HEADER, byte1: 18, derived: true, ..Default::default() };
        assert_eq!(request(&q), vec![3, 8, 18]);
    }

    #[test]
    fn a_status_reply() {
        let m = [3, 7, 66, 0xfe, 0xff];
        let r = reply(EE_WON_RT_STATUS, 66, &m, 3, &st());
        assert_eq!(r.end, End::Status(-2));
        assert!(r.events.is_empty());
        assert_eq!(reply(EE_WON_RT_STATUS, 65, &m, 3, &st()).end, End::Status(EE_WON_RT_DONT_WANT));
        assert_eq!(reply(EE_WON_RT_STATUS, 66, &m[..4], 3, &st()).end, End::Cut);
    }

    #[test]
    fn a_cut_string_is_erased() {
        // group name changed: id, then a name of 2 units with one present
        let m = [5, 0, 2, 0, 0x41, 0];
        let r = reply(EE_WON_RT_N_GROUP_NAME, 38, &m, 0, &st());
        assert_eq!(r.end, End::Cut);
        assert_eq!(r.events.len(), 2);
        assert_eq!(r.events[0], EeWonRouteEvent { tag: EE_WON_RT_FIELD, slot: 0, num: 5, at: 0, len: 0 });
        assert_eq!(r.events[1], EeWonRouteEvent { tag: EE_WON_RT_TEXT, slot: 1, num: 0, at: 4, len: 0 });
        // its length cut: no assignment
        let r = reply(EE_WON_RT_N_GROUP_NAME, 38, &m[..3], 0, &st());
        assert_eq!(r.events.len(), 1);
        // whole
        let m = [5, 0, 1, 0, 0x41, 0];
        let r = reply(EE_WON_RT_N_GROUP_NAME, 38, &m, 0, &st());
        assert_eq!(r.end, End::Status(0));
        assert_eq!(r.events[1].len, 1);
        assert_eq!(r.pos, 6);
    }

    #[test]
    fn a_group_list_with_members_and_a_skip() {
        let mut m = vec![0, 0]; // status
        m.extend(le16(0x81 | 0x20)); // name, member count, members
        m.extend(le16(1));
        let mut g = vec![];
        g.extend(le16(7)); // id
        g.extend(le16(1));
        g.extend([0x42, 0]);
        g.extend(le16(1)); // one member
        g.extend(le16(9));
        g.push(3);
        g.extend([0xaa, 0xbb]); // two bytes the size skips
        m.extend(le16(g.len() as u16));
        m.extend(g);
        let r = reply(EE_WON_RT_GROUP_LIST, 25, &m, 0, &st());
        assert_eq!(r.end, End::Status(0));
        assert_eq!(r.pos, m.len());
        let tags: Vec<(u8, u8)> = r.events.iter().map(|e| (e.tag, e.slot)).collect();
        assert_eq!(
            tags,
            vec![
                (EE_WON_RT_ACT, EE_WON_RT_ACT_CLEAR_GROUPS),
                (EE_WON_RT_FIELD, 0),
                (EE_WON_RT_NEW, EE_WON_RT_KIND_GROUP),
                (EE_WON_RT_REC, EE_WON_RT_GROUP_ID),
                (EE_WON_RT_REC_TEXT, EE_WON_RT_GROUP_NAME),
                (EE_WON_RT_REC, EE_WON_RT_GROUP_MAX_PLAYERS),
                (EE_WON_RT_NEW, EE_WON_RT_KIND_MEMBER),
                (EE_WON_RT_REC, EE_WON_RT_MEMBER_ID),
                (EE_WON_RT_REC, EE_WON_RT_MEMBER_FLAGS),
                (EE_WON_RT_INSERT, EE_WON_RT_INTO_GROUP_MEMBERS),
                (EE_WON_RT_INSERT, EE_WON_RT_INTO_GROUPS),
            ]
        );
        // an entry that overran its size: past the end
        let mut bad = m.clone();
        bad[6] = 2;
        assert_eq!(reply(EE_WON_RT_GROUP_LIST, 25, &bad, 0, &st()).end, End::Cut);
        // the body form catches the cut
        assert_eq!(reply(EE_WON_RT_GROUP_LIST_BODY, 0, &bad[2..], 0, &st()).end, End::Status(EE_WON_RT_UNPACK_FAILURE));
        // the request op's whole reply: its header, status and the caught body
        let mut req = vec![3, 7, 25];
        req.extend(&bad);
        let r = reply(EE_WON_RT_GROUP_LIST_REQ, 0, &req, 0, &st());
        assert_eq!(r.end, End::Status(EE_WON_RT_UNPACK_FAILURE));
        let r = reply(EE_WON_RT_GROUP_LIST_REQ, 0, &[3, 7, 24], 0, &st());
        assert_eq!(r.events[0].slot, EE_WON_RT_ACT_INVALID_HEADER);
        let mut good = vec![3, 7, 25];
        good.extend(&m);
        let r = reply(EE_WON_RT_GROUP_LIST_REQ, 0, &good, 0, &st());
        assert_eq!(r.end, End::Status(0));
        assert_eq!(r.events[4].at, 3 + 2 + 2 + 2 + 2 + 2 + 2);
    }

    #[test]
    fn create_join_and_register_follow_their_state() {
        let s = EeWonRouteState { join_flags: 1, ..st() };
        let m = [0, 0, 4, 0, 0, 0, 9, 0];
        let r = reply(EE_WON_RT_CREATE_GROUP, 12, &m, 0, &s);
        assert_eq!(r.end, End::Status(EE_WON_RT_NEED_MORE));
        let r = reply(EE_WON_RT_CREATE_GROUP, 12, &m, 0, &st());
        assert_eq!(r.end, End::Status(0));
        assert_eq!(r.events.last().unwrap().num, 9);
        assert_eq!(reply(EE_WON_RT_CREATE_GROUP, 41, &m, 0, &st()).end, End::Status(EE_WON_RT_DONT_WANT));
        // join: another group's status
        let s = EeWonRouteState { group_id: 4, join_flags: 1, ..st() };
        let m = [0, 0, 5, 0, 0, 0];
        assert_eq!(reply(EE_WON_RT_JOIN_GROUP, 41, &m, 0, &s).end, End::Status(EE_WON_RT_DONT_WANT));
        let m = [0, 0, 4, 0, 0, 0];
        assert_eq!(reply(EE_WON_RT_JOIN_GROUP, 41, &m, 0, &s).end, End::Status(EE_WON_RT_NEED_MORE));
        let m = [0x13, 0xf6, 4, 0, 0, 0];
        assert_eq!(reply(EE_WON_RT_JOIN_GROUP, 41, &m, 0, &s).end, End::Status(EE_WON_RT_NEED_MORE));
        // register: a second reply needed
        let s = EeWonRouteState { register_flags: 4, replies_needed: 2, ..st() };
        let m = [0, 0, 1, 0, 0, 0, 1, 2, 3, 4];
        let r = reply(EE_WON_RT_REGISTER, 51, &m, 0, &s);
        assert_eq!(r.end, End::Status(EE_WON_RT_NEED_MORE));
        let s = EeWonRouteState { reply_count: 1, ..s };
        let r = reply(EE_WON_RT_REGISTER, 23, &[0, 0, 0, 0], 0, &s);
        assert_eq!(r.end, End::Status(0));
        let tags: Vec<u8> = r.events.iter().map(|e| e.slot).collect();
        assert_eq!(tags, vec![EE_WON_RT_ACT_COMMIT_CLIENTS, EE_WON_RT_ACT_INC_REPLIES]);
    }

    #[test]
    fn notifications_and_the_tables() {
        assert_eq!(unsolicited(6), Some(EE_WON_RT_N_CLIENT_FLAGS));
        assert_eq!(unsolicited(71), Some(EE_WON_RT_N_MEMBER_COUNT));
        assert_eq!(unsolicited(66), None);
        assert_eq!(NOTICES.len(), 26);
        assert_eq!(header(&[3, 7, 66]), Some((66, false, 3)));
        assert_eq!(header(&[3, 8, 1, 9]), Some((1, true, 3)));
        assert_eq!(header(&[3, 7]), None);
        assert_eq!(dispatch(EE_WON_RT_DONT_WANT), 0);
        assert_eq!(dispatch(EE_WON_RT_NEED_MORE), 1);
        assert_eq!(dispatch(5), 2);
        // client joined server with extended data, its user id read and an unknown kind skipped
        let m = [1, 0, 0, 0, 2, 0, 0, 0, 2, 9, 1, 0xee, 1, 4, 5, 0, 0, 0];
        let r = reply(EE_WON_RT_N_CLIENT_JOINED_SERVER, 8, &m, 0, &st());
        assert_eq!(r.end, End::Status(0));
        assert_eq!(r.events.last().unwrap().num, 5);
        // a chat's recipients
        let m = [1, 0, 2, 0, 0, 0, 2, 0, 7, 0, 8, 0];
        let r = reply(EE_WON_RT_N_PEER_CHAT, 47, &m, 0, &st());
        assert_eq!(r.events.iter().filter(|e| e.tag == EE_WON_RT_ACT).count(), 2);
        // user statuses, cut: what was read stays and the op answers an unpack failure
        let s = EeWonRouteState { user_count: 2, ..st() };
        let r = reply(EE_WON_RT_USER_STATUS, 0, &[3, 7, 73, 2, 1], 0, &s);
        assert_eq!(r.end, End::Status(EE_WON_RT_UNPACK_FAILURE));
        assert_eq!(r.events.len(), 1);
        assert_eq!(
            reply(EE_WON_RT_USER_STATUS, 0, &[3, 7, 73, 1, 1], 0, &s).end,
            End::Status(EE_WON_RT_INVALID_MESSAGE)
        );
        assert_eq!(reply(EE_WON_RT_USER_STATUS, 0, &[3, 7, 72], 0, &s).end, End::Status(EE_WON_RT_NONE));
        // a derived service's message: the rest
        let r = reply(EE_WON_RT_DERIVED, 9, &[3, 8, 9, 1, 2], 3, &st());
        assert_eq!(r.events[1], EeWonRouteEvent { tag: EE_WON_RT_BUF, slot: 1, num: 0, at: 3, len: 2 });
    }
}
