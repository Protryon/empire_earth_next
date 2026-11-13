//! WONRouting's routing ops as one Rust object (`src/Empire Earth/WONAPI/WONRouting`): every request op
//! (`RoutingCreateGroupOp`, `RoutingJoinGroupOp`, `RoutingRegisterClientOp`, the list ops, the data object ops, the
//! moderation ops, the status-only requests) and every notification op (`RoutingClientJoinedGroupOp` ... the
//! derived server's op): their members, the records their replies build (clients, members, groups, data objects,
//! bad users), the request each sends (`SendRequest`: the message, whether it waits for a reply) and the reply each
//! reads (`HandleReply`: the members and records assigned in the original's order, its status, a cut short read,
//! `RoutingCreateGroupOp::CheckLeaveGroup`'s leave).
//!
//! The C++ `RoutingOp` is a shim holding one [`EeWonRouteOp`] (it stays the `AsyncOp` the connection's list and
//! the completions hold); the readers (the lobby's `RoutingLogic`, `EEWonNet`, the smart connection) read the
//! members through accessors by field number (`EE_WON_RTF_*`) and the records as copies. The message formats are
//! `routemsg.rs`'s; this module applies their assignments, as `WONRoutingMsg.h`'s templates did. Reached by nothing
//! in the game but an Internet login: the routing servers are gone.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;
use std::rc::Rc;

use crate::ffi::EeWonByteSink;
use crate::hostop::EeWonHostOp;
use crate::reqlast::{EE_WON_REQOP_GROUP_REPLIES, EE_WON_REQOP_GROUP_REPLY};
use crate::routemsg::{self as rt, EeWonRouteEvent, EeWonRouteState, End, Request};

// ---- the op types (`RoutingOpType`, WONRoutingOpTypes.h)

pub(crate) const T_ABORT_SHUTDOWN: i32 = 1;
pub(crate) const T_ACCEPT_CLIENT: i32 = 2;
pub(crate) const T_BAN_CLIENT: i32 = 3;
pub(crate) const T_BECOME_MODERATOR: i32 = 4;
pub(crate) const T_CANCEL_JOIN: i32 = 5;
pub(crate) const T_CLIENT_FLAGS_CHANGED: i32 = 6;
pub(crate) const T_CLIENT_JOINED_GROUP: i32 = 7;
pub(crate) const T_CLIENT_JOINED_SERVER: i32 = 8;
pub(crate) const T_CLIENT_LEFT_GROUP: i32 = 9;
pub(crate) const T_CLIENT_LEFT_SERVER: i32 = 10;
pub(crate) const T_CREATE_DATA: i32 = 11;
pub(crate) const T_CREATE_GROUP: i32 = 12;
pub(crate) const T_DATA_CREATED: i32 = 13;
pub(crate) const T_DATA_DELETED: i32 = 14;
pub(crate) const T_DATA_MODIFIED: i32 = 15;
pub(crate) const T_DELETE_DATA: i32 = 16;
pub(crate) const T_DETECT_FIREWALL: i32 = 17;
pub(crate) const T_DISCONNECT: i32 = 18;
pub(crate) const T_BAD_USERS: i32 = 19;
pub(crate) const T_CLIENT_LIST: i32 = 20;
pub(crate) const T_GROUP_LIST: i32 = 21;
pub(crate) const T_MEMBERS: i32 = 22;
pub(crate) const T_CAPTAIN_CHANGED: i32 = 23;
pub(crate) const T_GROUP_CREATED: i32 = 24;
pub(crate) const T_GROUP_DELETED: i32 = 25;
pub(crate) const T_GROUP_FLAGS_CHANGED: i32 = 26;
pub(crate) const T_INVITATION: i32 = 27;
pub(crate) const T_JOIN_ATTEMPT: i32 = 28;
pub(crate) const T_MAX_PLAYERS_CHANGED: i32 = 29;
pub(crate) const T_MEMBER_COUNT: i32 = 30;
pub(crate) const T_GROUP_NAME_CHANGED: i32 = 31;
pub(crate) const T_OBSERVER_COUNT: i32 = 32;
pub(crate) const T_INVITE_CLIENT: i32 = 33;
pub(crate) const T_JOIN_GROUP: i32 = 34;
pub(crate) const T_LEAVE_GROUP: i32 = 35;
pub(crate) const T_MEMBER_FLAGS_CHANGED: i32 = 36;
pub(crate) const T_MODIFY_DATA: i32 = 37;
pub(crate) const T_MUTE_CLIENT: i32 = 38;
pub(crate) const T_PEER_CHAT: i32 = 39;
pub(crate) const T_PEER_DATA: i32 = 40;
pub(crate) const T_READ_DATA: i32 = 41;
pub(crate) const T_REGISTER: i32 = 42;
pub(crate) const T_RELINQUISH: i32 = 43;
pub(crate) const T_SEND_CHAT: i32 = 44;
pub(crate) const T_SEND_DATA: i32 = 46;
pub(crate) const T_SEND_ALERT: i32 = 47;
pub(crate) const T_SERVER_ALERT: i32 = 48;
pub(crate) const T_SHUTDOWN_ABORTED: i32 = 49;
pub(crate) const T_SHUTDOWN_STARTED: i32 = 50;
pub(crate) const T_SET_CLIENT_FLAGS: i32 = 51;
pub(crate) const T_SET_GROUP_FLAGS: i32 = 52;
pub(crate) const T_SET_MAX_PLAYERS: i32 = 53;
pub(crate) const T_SET_GROUP_NAME: i32 = 54;
pub(crate) const T_SET_GROUP_PASSWORD: i32 = 55;
pub(crate) const T_START_SHUTDOWN: i32 = 56;
pub(crate) const T_UNSUBSCRIBE: i32 = 57;
pub(crate) const T_BANNED: i32 = 58;
pub(crate) const T_MUTED: i32 = 59;
pub(crate) const T_DERIVED: i32 = 60;

// ---- the numeric members (`ee_won_routeop_num`'s field), each with the width of the members it stands for

/// `mGroupId` (u16)
pub const EE_WON_RTF_GROUP_ID: i32 = 0;
/// `mClientId`; the relinquish op's new captain (u16)
pub const EE_WON_RTF_CLIENT_ID: i32 = 1;
/// `mParentGroupId` (u16)
pub const EE_WON_RTF_PARENT_ID: i32 = 2;
/// `mMaxPlayers` (u16)
pub const EE_WON_RTF_MAX_PLAYERS: i32 = 3;
/// the group created notice's `mMaxClients` (u16)
pub const EE_WON_RTF_MAX_CLIENTS: i32 = 4;
/// `mLinkId` (u16)
pub const EE_WON_RTF_LINK_ID: i32 = 5;
/// `mOffset` (u16)
pub const EE_WON_RTF_OFFSET: i32 = 6;
/// `mCaptainId` (u16)
pub const EE_WON_RTF_CAPTAIN_ID: i32 = 7;
/// `mObserverCount` (u16)
pub const EE_WON_RTF_OBSERVERS: i32 = 8;
/// `mMemberCount` (u16)
pub const EE_WON_RTF_MEMBER_COUNT: i32 = 9;
/// a 16-bit `mFlags`: chat, peer chat, the group list (u16)
pub const EE_WON_RTF_FLAGS16: i32 = 10;
/// the firewall op's listen port (u16)
pub const EE_WON_RTF_LISTEN_PORT: i32 = 11;
/// an 8-bit `mFlags` / `mFlag`: data, read, unsubscribe, create data, bad users, join attempt (u8)
pub const EE_WON_RTF_FLAGS8: i32 = 12;
/// `mJoinFlags` (u8)
pub const EE_WON_RTF_JOIN_FLAGS: i32 = 13;
/// `mMemberFlags` (u8)
pub const EE_WON_RTF_MEMBER_FLAGS: i32 = 14;
/// the derived op's `mMsgType` (u8)
pub const EE_WON_RTF_MSG_TYPE: i32 = 15;
/// `mGroupFlags`, the group created notice's `mFlags` (u32)
pub const EE_WON_RTF_GROUP_FLAGS: i32 = 16;
/// `mAsyncFlags` (u32)
pub const EE_WON_RTF_ASYNC_FLAGS: i32 = 17;
/// `mGroupFlagMask` (u32)
pub const EE_WON_RTF_GROUP_MASK: i32 = 18;
/// `mAsyncFlagMask` (u32)
pub const EE_WON_RTF_ASYNC_MASK: i32 = 19;
/// `mClientFlags` (u32)
pub const EE_WON_RTF_CLIENT_FLAGS: i32 = 20;
/// `mClientFlagMask` (u32)
pub const EE_WON_RTF_CLIENT_MASK: i32 = 21;
/// `mAsyncMessageFlags` (u32)
pub const EE_WON_RTF_ASYNC_MSG_FLAGS: i32 = 22;
/// `mAsyncMessageFlagMask` (u32)
pub const EE_WON_RTF_ASYNC_MSG_MASK: i32 = 23;
/// `mRegisterFlags` (u32)
pub const EE_WON_RTF_REGISTER_FLAGS: i32 = 24;
/// `mReconnectId` (u32)
pub const EE_WON_RTF_RECONNECT_ID: i32 = 25;
/// `mWONId` (u32)
pub const EE_WON_RTF_WON_ID: i32 = 26;
/// a duration: `mBanTime`, `mMuteTime`, `mSecondsUntilShutdown`, `mSeconds`, `mDuration` (u32)
pub const EE_WON_RTF_TIME: i32 = 27;
/// the firewall op's longest connect wait (u32)
pub const EE_WON_RTF_MAX_WAIT: i32 = 28;
/// the op's on/off member: banned, muted, accepted, invited, moderator, insert, subscribe, leave if joined (bool)
pub const EE_WON_RTF_ON: i32 = 29;
/// `mBanByWONId` / `mMuteByWONId` (bool)
pub const EE_WON_RTF_BY_WON_ID: i32 = 30;
/// `mWONIdIsCDKeyId` (bool)
pub const EE_WON_RTF_IS_CDKEY: i32 = 31;
/// `mIsEx` (bool)
pub const EE_WON_RTF_IS_EX: i32 = 32;
/// `mLeaveGroupOnCancel` (bool)
pub const EE_WON_RTF_LEAVE_ON_CANCEL: i32 = 33;
/// `mHasClientNames` (bool)
pub const EE_WON_RTF_HAS_NAMES: i32 = 34;
/// `mHasClientFlags` (bool)
pub const EE_WON_RTF_HAS_CLIENT_FLAGS: i32 = 35;
/// the firewall op's `mWaitingForStatusReply` (bool)
pub const EE_WON_RTF_WAITING: i32 = 36;
/// the firewall op's `mDoListen` (bool)
pub const EE_WON_RTF_DO_LISTEN: i32 = 37;
/// `mListType` (a signed 32-bit enum)
pub const EE_WON_RTF_LIST_TYPE: i32 = 38;
/// `mReplyCount` (i32)
pub const EE_WON_RTF_REPLY_COUNT: i32 = 39;
/// `mNumRepliesNeeded` (i32)
pub const EE_WON_RTF_REPLIES_NEEDED: i32 = 40;
const FIELDS: usize = 41;

// ---- the texts (`ee_won_routeop_text`'s which)

/// a message text: chat, alerts, shutdown notices
pub const EE_WON_RTT_TEXT: i32 = 0;
/// a name: the client's, the group's, a data object's
pub const EE_WON_RTT_NAME: i32 = 1;
/// a password: the server's, the group's
pub const EE_WON_RTT_PASSWORD: i32 = 2;
/// a comment: ban, mute, accept, invite, join, invitation, join attempt, banned, muted
pub const EE_WON_RTT_COMMENT: i32 = 3;
const TEXTS: usize = 4;

#[derive(Clone, Copy)]
enum Width {
    U8,
    U16,
    U32,
    Bool,
    I32,
}

fn width(field: i32) -> Width {
    match field {
        0..=11 => Width::U16,
        12..=15 => Width::U8,
        16..=28 => Width::U32,
        29..=37 => Width::Bool,
        _ => Width::I32,
    }
}

/// A value as a member of the field's width holds it (`(T)value`).
fn fit(field: i32, v: i64) -> i64 {
    match width(field) {
        Width::U8 => i64::from(v as u8),
        Width::U16 => i64::from(v as u16),
        Width::U32 => i64::from(v as u32),
        Width::Bool => i64::from(v != 0),
        Width::I32 => i64::from(v as i32),
    }
}

// ---- the records

/// `RoutingClientInfo`
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Client {
    pub id: u16,
    pub name: Vec<u16>,
    pub flags: u32,
    pub user_id: u32,
    /// `mAddress`: the CD key id
    pub address: u32,
    pub trust: u16,
}

/// `RoutingMemberInfo`
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Member {
    pub id: u16,
    pub flags: u8,
    pub client: Option<Shared<Client>>,
}

/// `RoutingGroupInfo`
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Group {
    pub id: u16,
    pub name: Vec<u16>,
    pub parent: u16,
    pub captain: u16,
    pub max_clients: u16,
    pub flags: u32,
    pub async_flags: u32,
    pub observers: u16,
    /// +0x2a: the group list's member count
    pub max_players: u16,
    pub members: BTreeMap<u16, Shared<Member>>,
}

impl Default for Group {
    fn default() -> Group {
        Group {
            id: 0,
            name: Vec::new(),
            parent: 0xffff,
            captain: 0,
            max_clients: 0,
            flags: 0,
            async_flags: 0,
            observers: 0,
            max_players: 0,
            members: BTreeMap::new(),
        }
    }
}

/// `RoutingDataObject`
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Object {
    pub link: u16,
    pub data_type: Vec<u8>,
    pub name: Vec<u16>,
    /// `mData`: none until a reply assigns it
    pub data: Option<Vec<u8>>,
}

/// `RoutingBadUser`
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct BadUser {
    pub won_id: u32,
    pub name: Vec<u16>,
    pub expiration: u32,
    pub moderator_id: u32,
    pub moderator_name: Vec<u16>,
    pub comment: Vec<u16>,
}

/// A record shared as the original's `SmartPtr`s shared it (a member's client after the member went in its map).
pub type Shared<T> = Rc<RefCell<T>>;

fn shared<T>(v: T) -> Shared<T> {
    Rc::new(RefCell::new(v))
}

// ---- the op

/// A routing op: its type, members and records.
#[derive(Clone, Debug)]
pub struct RouteOp {
    pub ty: i32,
    nums: [i64; FIELDS],
    texts: [Vec<u16>; TEXTS],
    /// `mDataType`
    pub data_type: Vec<u8>,
    /// `mData` / `mRecvMsg`: none for a null buffer
    pub data: Option<Vec<u8>>,
    /// the recipients (`mRecipients`, `mRecipientList`)
    pub ids: Vec<u16>,
    /// `mClientMap`
    pub clients: BTreeMap<u16, Shared<Client>>,
    /// `mMembers`, the join op's `mMemberMap`
    pub members: BTreeMap<u16, Shared<Member>>,
    /// `mGroupMap`
    pub groups: BTreeMap<u16, Shared<Group>>,
    /// `mDataObjects`
    pub objects: Vec<Shared<Object>>,
    /// `mBadUserList`
    pub bad_users: Vec<BadUser>,
    /// the client joined server notice's `mClientInfo`
    pub op_client: Option<Shared<Client>>,
}

/// What a request does: its message, and whether the op waits in the connection's list for a reply.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Sent {
    pub message: Vec<u8>,
    pub add_op: bool,
}

/// How a reply's read ended.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Read {
    /// `End::Status` or a cut short read (the original's `ReadBufferException`)
    pub end: End,
    /// `CheckLeaveGroup` decided to leave this group (a `RoutingLeaveGroupOp` run async)
    pub leave_group: Option<u16>,
}

/// The containers a reply's records go into.
#[derive(Default)]
struct Recs {
    client: Option<Shared<Client>>,
    member: Option<Shared<Member>>,
    group: Option<Shared<Group>>,
    object: Option<Shared<Object>>,
    bad: BadUser,
    /// the register op's (and join op's) inner op's map, copied in on its commit
    temp_clients: BTreeMap<u16, Shared<Client>>,
    temp_members: BTreeMap<u16, Shared<Member>>,
    temp: bool,
}

fn units(data: &[u8], e: &EeWonRouteEvent) -> Vec<u16> {
    let at = e.at as usize;
    let end = at + 2 * e.len as usize;
    data.get(at..end).map(crate::text::utf16_from_le_bytes).unwrap_or_default()
}

fn span(data: &[u8], e: &EeWonRouteEvent) -> Vec<u8> {
    let at = e.at as usize;
    data.get(at..at + e.len as usize).map(<[u8]>::to_vec).unwrap_or_default()
}

/// The notification kind (routemsg's `EE_WON_RT_N_*`) an unsolicited message makes, as its op type.
pub fn notice_type(kind: i32) -> i32 {
    match kind {
        rt::EE_WON_RT_N_CLIENT_FLAGS => T_CLIENT_FLAGS_CHANGED,
        rt::EE_WON_RT_N_CLIENT_JOINED_GROUP => T_CLIENT_JOINED_GROUP,
        rt::EE_WON_RT_N_CLIENT_JOINED_SERVER => T_CLIENT_JOINED_SERVER,
        rt::EE_WON_RT_N_CLIENT_LEFT_GROUP => T_CLIENT_LEFT_GROUP,
        rt::EE_WON_RT_N_CLIENT_LEFT_SERVER => T_CLIENT_LEFT_SERVER,
        rt::EE_WON_RT_N_DATA_CREATED => T_DATA_CREATED,
        rt::EE_WON_RT_N_DATA_DELETED => T_DATA_DELETED,
        rt::EE_WON_RT_N_DATA_MODIFIED => T_DATA_MODIFIED,
        rt::EE_WON_RT_N_CAPTAIN_CHANGED => T_CAPTAIN_CHANGED,
        rt::EE_WON_RT_N_GROUP_CREATED => T_GROUP_CREATED,
        rt::EE_WON_RT_N_GROUP_DELETED => T_GROUP_DELETED,
        rt::EE_WON_RT_N_GROUP_FLAGS => T_GROUP_FLAGS_CHANGED,
        rt::EE_WON_RT_N_INVITATION => T_INVITATION,
        rt::EE_WON_RT_N_JOIN_ATTEMPT => T_JOIN_ATTEMPT,
        rt::EE_WON_RT_N_MAX_PLAYERS => T_MAX_PLAYERS_CHANGED,
        rt::EE_WON_RT_N_MEMBER_COUNT => T_MEMBER_COUNT,
        rt::EE_WON_RT_N_GROUP_NAME => T_GROUP_NAME_CHANGED,
        rt::EE_WON_RT_N_OBSERVER_COUNT => T_OBSERVER_COUNT,
        rt::EE_WON_RT_N_MEMBER_FLAGS => T_MEMBER_FLAGS_CHANGED,
        rt::EE_WON_RT_N_PEER_CHAT => T_PEER_CHAT,
        rt::EE_WON_RT_N_PEER_DATA => T_PEER_DATA,
        rt::EE_WON_RT_N_SERVER_ALERT => T_SERVER_ALERT,
        rt::EE_WON_RT_N_SHUTDOWN_ABORTED => T_SHUTDOWN_ABORTED,
        rt::EE_WON_RT_N_SHUTDOWN_STARTED => T_SHUTDOWN_STARTED,
        rt::EE_WON_RT_N_BANNED => T_BANNED,
        rt::EE_WON_RT_N_MUTED => T_MUTED,
        _ => 0,
    }
}

/// The reply kind (routemsg's) an op type reads, `None` for an op that wants no reply (the base `HandleReply`).
fn reply_kind(ty: i32) -> Option<i32> {
    Some(match ty {
        T_ABORT_SHUTDOWN | T_ACCEPT_CLIENT | T_BAN_CLIENT | T_BECOME_MODERATOR | T_CANCEL_JOIN | T_CREATE_DATA
        | T_DELETE_DATA | T_INVITE_CLIENT | T_LEAVE_GROUP | T_MODIFY_DATA | T_MUTE_CLIENT | T_RELINQUISH
        | T_SEND_CHAT | T_SEND_DATA | T_SEND_ALERT | T_SET_CLIENT_FLAGS | T_SET_GROUP_FLAGS | T_SET_MAX_PLAYERS
        | T_SET_GROUP_NAME | T_SET_GROUP_PASSWORD | T_START_SHUTDOWN | T_UNSUBSCRIBE => rt::EE_WON_RT_STATUS,
        T_CREATE_GROUP => rt::EE_WON_RT_CREATE_GROUP,
        T_JOIN_GROUP => rt::EE_WON_RT_JOIN_GROUP,
        T_REGISTER => rt::EE_WON_RT_REGISTER,
        T_CLIENT_LIST => rt::EE_WON_RT_CLIENT_LIST,
        T_GROUP_LIST => rt::EE_WON_RT_GROUP_LIST,
        T_MEMBERS => rt::EE_WON_RT_MEMBERS,
        T_BAD_USERS => rt::EE_WON_RT_BAD_USERS,
        T_READ_DATA => rt::EE_WON_RT_READ_DATA,
        T_DETECT_FIREWALL => rt::EE_WON_RT_FIREWALL,
        T_DERIVED => rt::EE_WON_RT_DERIVED,
        T_CLIENT_FLAGS_CHANGED => rt::EE_WON_RT_N_CLIENT_FLAGS,
        T_CLIENT_JOINED_GROUP => rt::EE_WON_RT_N_CLIENT_JOINED_GROUP,
        T_CLIENT_JOINED_SERVER => rt::EE_WON_RT_N_CLIENT_JOINED_SERVER,
        T_CLIENT_LEFT_GROUP => rt::EE_WON_RT_N_CLIENT_LEFT_GROUP,
        T_CLIENT_LEFT_SERVER => rt::EE_WON_RT_N_CLIENT_LEFT_SERVER,
        T_DATA_CREATED => rt::EE_WON_RT_N_DATA_CREATED,
        T_DATA_DELETED => rt::EE_WON_RT_N_DATA_DELETED,
        T_DATA_MODIFIED => rt::EE_WON_RT_N_DATA_MODIFIED,
        T_CAPTAIN_CHANGED => rt::EE_WON_RT_N_CAPTAIN_CHANGED,
        T_GROUP_CREATED => rt::EE_WON_RT_N_GROUP_CREATED,
        T_GROUP_DELETED => rt::EE_WON_RT_N_GROUP_DELETED,
        T_GROUP_FLAGS_CHANGED => rt::EE_WON_RT_N_GROUP_FLAGS,
        T_INVITATION => rt::EE_WON_RT_N_INVITATION,
        T_JOIN_ATTEMPT => rt::EE_WON_RT_N_JOIN_ATTEMPT,
        T_MAX_PLAYERS_CHANGED => rt::EE_WON_RT_N_MAX_PLAYERS,
        T_MEMBER_COUNT => rt::EE_WON_RT_N_MEMBER_COUNT,
        T_GROUP_NAME_CHANGED => rt::EE_WON_RT_N_GROUP_NAME,
        T_OBSERVER_COUNT => rt::EE_WON_RT_N_OBSERVER_COUNT,
        T_MEMBER_FLAGS_CHANGED => rt::EE_WON_RT_N_MEMBER_FLAGS,
        T_PEER_CHAT => rt::EE_WON_RT_N_PEER_CHAT,
        T_PEER_DATA => rt::EE_WON_RT_N_PEER_DATA,
        T_SERVER_ALERT => rt::EE_WON_RT_N_SERVER_ALERT,
        T_SHUTDOWN_ABORTED => rt::EE_WON_RT_N_SHUTDOWN_ABORTED,
        T_SHUTDOWN_STARTED => rt::EE_WON_RT_N_SHUTDOWN_STARTED,
        T_BANNED => rt::EE_WON_RT_N_BANNED,
        T_MUTED => rt::EE_WON_RT_N_MUTED,
        _ => return None,
    })
}

impl RouteOp {
    /// A new op of a type with its constructor's members (the original's uninitialized ones are 0).
    pub fn new(ty: i32) -> RouteOp {
        let mut op = RouteOp {
            ty,
            nums: [0; FIELDS],
            texts: Default::default(),
            data_type: Vec::new(),
            data: None,
            ids: Vec::new(),
            clients: BTreeMap::new(),
            members: BTreeMap::new(),
            groups: BTreeMap::new(),
            objects: Vec::new(),
            bad_users: Vec::new(),
            op_client: None,
        };
        match ty {
            // the ban and mute ops: the server-wide group, banned / muted, by WON id
            T_BAN_CLIENT | T_MUTE_CLIENT => {
                op.set(EE_WON_RTF_GROUP_ID, 0xffff);
                op.set(EE_WON_RTF_ON, 1);
                op.set(EE_WON_RTF_BY_WON_ID, 1);
            }
            T_CREATE_GROUP => {
                op.set(EE_WON_RTF_GROUP_ID, 0xffff);
                op.set(EE_WON_RTF_PARENT_ID, 0xffff);
            }
            T_GROUP_LIST => op.set(EE_WON_RTF_PARENT_ID, 0xffff),
            T_CANCEL_JOIN => op.set(EE_WON_RTF_ON, 1),
            T_RELINQUISH => op.set(EE_WON_RTF_CLIENT_ID, 0xffff),
            T_DETECT_FIREWALL => op.set(EE_WON_RTF_DO_LISTEN, 1),
            _ => {}
        }
        op
    }

    /// A numeric member (0 for a field out of range).
    pub fn num(&self, field: i32) -> i64 {
        usize::try_from(field).ok().and_then(|f| self.nums.get(f)).copied().unwrap_or(0)
    }

    /// Assigns a numeric member, cut to its width as the original's assignment cut it.
    pub fn set(&mut self, field: i32, v: i64) {
        if let Some(slot) = usize::try_from(field).ok().and_then(|f| self.nums.get_mut(f)) {
            *slot = fit(field, v);
        }
    }

    pub fn text(&self, which: i32) -> &[u16] {
        usize::try_from(which).ok().and_then(|w| self.texts.get(w)).map(Vec::as_slice).unwrap_or(&[])
    }

    pub fn set_text(&mut self, which: i32, units: Vec<u16>) {
        if let Some(t) = usize::try_from(which).ok().and_then(|w| self.texts.get_mut(w)) {
            *t = units;
        }
    }

    fn n16(&self, f: i32) -> u16 {
        self.num(f) as u16
    }
    fn n8(&self, f: i32) -> u8 {
        self.num(f) as u8
    }
    fn n32(&self, f: i32) -> u32 {
        self.num(f) as u32
    }
    fn on(&self, f: i32) -> bool {
        self.num(f) != 0
    }

    /// `SendRequest`: what the op sends (with the members it resets first), or none for an op that sends nothing
    /// (the notifications, the derived op).
    pub fn request(&mut self) -> Option<Sent> {
        const G: i32 = EE_WON_RTF_GROUP_ID;
        let mut q = Request { derived: self.ty == T_DERIVED, ..Request::default() };
        let mut add_op = true;
        let longs = |a: u32, b: u32, c: u32, d: u32| [a, b, c, d];
        match self.ty {
            T_ABORT_SHUTDOWN => q.kind = rt::EE_WON_RT_REQ_ABORT_SHUTDOWN,
            T_ACCEPT_CLIENT | T_INVITE_CLIENT => {
                q.kind =
                    if self.ty == T_ACCEPT_CLIENT { rt::EE_WON_RT_REQ_ACCEPT_CLIENT } else { rt::EE_WON_RT_REQ_INVITE };
                q.group_id = self.n16(G);
                q.client_id = self.n16(EE_WON_RTF_CLIENT_ID);
                q.on = self.on(EE_WON_RTF_ON);
                q.text1 = self.text(EE_WON_RTT_COMMENT).to_vec();
            }
            T_BAN_CLIENT | T_MUTE_CLIENT => {
                q.kind = if self.ty == T_BAN_CLIENT { rt::EE_WON_RT_REQ_BAN } else { rt::EE_WON_RT_REQ_MUTE };
                q.group_id = self.n16(G);
                q.on = self.on(EE_WON_RTF_ON);
                q.longs[0] = self.n32(EE_WON_RTF_TIME);
                q.text1 = self.text(EE_WON_RTT_COMMENT).to_vec();
                q.by_won_id = self.on(EE_WON_RTF_BY_WON_ID);
                q.is_cdkey = self.on(EE_WON_RTF_IS_CDKEY);
                q.longs[1] = self.n32(EE_WON_RTF_WON_ID);
                q.client_id = self.n16(EE_WON_RTF_CLIENT_ID);
            }
            T_BECOME_MODERATOR => {
                q.kind = rt::EE_WON_RT_REQ_BECOME_MODERATOR;
                q.on = self.on(EE_WON_RTF_ON);
            }
            T_CANCEL_JOIN => {
                q.kind = rt::EE_WON_RT_REQ_CANCEL_JOIN;
                q.group_id = self.n16(G);
                q.on = self.on(EE_WON_RTF_ON);
            }
            T_CREATE_DATA => {
                q.kind = rt::EE_WON_RT_REQ_CREATE_DATA;
                q.byte1 = self.n8(EE_WON_RTF_FLAGS8);
                q.link_id = self.n16(EE_WON_RTF_LINK_ID);
                q.text1 = self.text(EE_WON_RTT_NAME).to_vec();
            }
            T_CREATE_GROUP => {
                q.kind = rt::EE_WON_RT_REQ_CREATE_GROUP;
                q.group_id = self.n16(G);
                q.text1 = self.text(EE_WON_RTT_NAME).to_vec();
                q.text2 = self.text(EE_WON_RTT_PASSWORD).to_vec();
                q.max_players = self.n16(EE_WON_RTF_MAX_PLAYERS);
                q.byte1 = self.n8(EE_WON_RTF_JOIN_FLAGS);
                q.longs[0] = self.n32(EE_WON_RTF_GROUP_FLAGS);
                q.longs[1] = self.n32(EE_WON_RTF_ASYNC_FLAGS);
                q.parent_id = self.n16(EE_WON_RTF_PARENT_ID);
            }
            T_DELETE_DATA => {
                q.kind = rt::EE_WON_RT_REQ_DELETE_DATA;
                q.link_id = self.n16(EE_WON_RTF_LINK_ID);
                q.text1 = self.text(EE_WON_RTT_NAME).to_vec();
            }
            T_DETECT_FIREWALL => {
                q.kind = rt::EE_WON_RT_REQ_FIREWALL;
                q.port = self.n16(EE_WON_RTF_LISTEN_PORT);
                q.longs[0] = self.n32(EE_WON_RTF_MAX_WAIT);
            }
            T_DISCONNECT => {
                q.kind = rt::EE_WON_RT_REQ_DISCONNECT;
                add_op = false;
            }
            T_BAD_USERS => {
                q.kind = rt::EE_WON_RT_REQ_BAD_USERS;
                q.byte1 = self.num(EE_WON_RTF_LIST_TYPE) as u8;
                q.group_id = self.n16(G);
                q.byte2 = self.n8(EE_WON_RTF_FLAGS8);
            }
            T_CLIENT_LIST => {
                self.clients.clear();
                q.kind = rt::EE_WON_RT_REQ_CLIENT_LIST;
                q.is_ex = self.on(EE_WON_RTF_IS_EX);
            }
            T_GROUP_LIST => {
                self.groups.clear();
                q.kind = rt::EE_WON_RT_REQ_GROUP_LIST;
                q.flags16 = self.n16(EE_WON_RTF_FLAGS16);
                q.parent_id = self.n16(EE_WON_RTF_PARENT_ID);
            }
            T_MEMBERS => {
                self.members.clear();
                q.kind = rt::EE_WON_RT_REQ_MEMBERS;
                q.group_id = self.n16(G);
            }
            T_JOIN_GROUP => {
                q.kind = rt::EE_WON_RT_REQ_JOIN_GROUP;
                q.group_id = self.n16(G);
                q.text1 = self.text(EE_WON_RTT_PASSWORD).to_vec();
                q.text2 = self.text(EE_WON_RTT_COMMENT).to_vec();
                q.byte1 = self.n8(EE_WON_RTF_JOIN_FLAGS);
                q.byte2 = self.n8(EE_WON_RTF_MEMBER_FLAGS);
            }
            T_LEAVE_GROUP => {
                q.kind = rt::EE_WON_RT_REQ_LEAVE;
                q.group_id = self.n16(G);
            }
            T_MODIFY_DATA => {
                q.kind = rt::EE_WON_RT_REQ_MODIFY_DATA;
                q.link_id = self.n16(EE_WON_RTF_LINK_ID);
                q.text1 = self.text(EE_WON_RTT_NAME).to_vec();
                q.offset = self.n16(EE_WON_RTF_OFFSET);
                q.on = self.on(EE_WON_RTF_ON);
            }
            T_READ_DATA => {
                self.objects.clear();
                q.kind = rt::EE_WON_RT_REQ_READ_DATA;
                q.on = self.on(EE_WON_RTF_ON);
                q.link_id = self.n16(EE_WON_RTF_LINK_ID);
                q.byte1 = self.n8(EE_WON_RTF_FLAGS8);
            }
            T_REGISTER => {
                self.set(EE_WON_RTF_REPLY_COUNT, 0);
                let needed = if self.n32(EE_WON_RTF_REGISTER_FLAGS) & 4 != 0 { 2 } else { 1 };
                self.set(EE_WON_RTF_REPLIES_NEEDED, needed);
                q.kind = rt::EE_WON_RT_REQ_REGISTER;
                q.text1 = self.text(EE_WON_RTT_NAME).to_vec();
                q.text2 = self.text(EE_WON_RTT_PASSWORD).to_vec();
                q.longs = longs(
                    self.n32(EE_WON_RTF_RECONNECT_ID),
                    self.n32(EE_WON_RTF_ASYNC_MSG_FLAGS),
                    self.n32(EE_WON_RTF_REGISTER_FLAGS),
                    self.n32(EE_WON_RTF_CLIENT_FLAGS),
                );
            }
            T_RELINQUISH => {
                q.kind = rt::EE_WON_RT_REQ_RELINQUISH_CAPTAIN;
                q.group_id = self.n16(G);
                q.client_id = self.n16(EE_WON_RTF_CLIENT_ID);
            }
            T_SEND_CHAT => {
                q.kind = rt::EE_WON_RT_REQ_SEND_CHAT;
                q.flags16 = self.n16(EE_WON_RTF_FLAGS16);
                q.text1 = self.text(EE_WON_RTT_TEXT).to_vec();
                q.ids = self.ids.clone();
                add_op = q.flags16 & 1 != 0;
            }
            T_SEND_DATA => {
                q.kind = rt::EE_WON_RT_REQ_SEND_DATA;
                q.byte1 = self.n8(EE_WON_RTF_FLAGS8);
                q.ids = self.ids.clone();
                add_op = q.byte1 & 1 != 0;
            }
            T_SEND_ALERT => {
                q.kind = rt::EE_WON_RT_REQ_SERVER_ALERT;
                q.text1 = self.text(EE_WON_RTT_TEXT).to_vec();
                q.ids = self.ids.clone();
            }
            T_SET_CLIENT_FLAGS => {
                q.kind = rt::EE_WON_RT_REQ_SET_CLIENT_FLAGS;
                q.longs = longs(
                    self.n32(EE_WON_RTF_ASYNC_MSG_MASK),
                    self.n32(EE_WON_RTF_ASYNC_MSG_FLAGS),
                    self.n32(EE_WON_RTF_CLIENT_MASK),
                    self.n32(EE_WON_RTF_CLIENT_FLAGS),
                );
            }
            T_SET_GROUP_FLAGS => {
                q.kind = rt::EE_WON_RT_REQ_SET_GROUP_FLAGS;
                q.group_id = self.n16(G);
                q.longs = longs(
                    self.n32(EE_WON_RTF_GROUP_MASK),
                    self.n32(EE_WON_RTF_GROUP_FLAGS),
                    self.n32(EE_WON_RTF_ASYNC_MASK),
                    self.n32(EE_WON_RTF_ASYNC_FLAGS),
                );
            }
            T_SET_MAX_PLAYERS => {
                q.kind = rt::EE_WON_RT_REQ_SET_GROUP_MAX_PLAYERS;
                q.group_id = self.n16(G);
                q.max_players = self.n16(EE_WON_RTF_MAX_PLAYERS);
            }
            T_SET_GROUP_NAME | T_SET_GROUP_PASSWORD => {
                let (kind, which) = if self.ty == T_SET_GROUP_NAME {
                    (rt::EE_WON_RT_REQ_SET_GROUP_NAME, EE_WON_RTT_NAME)
                } else {
                    (rt::EE_WON_RT_REQ_SET_GROUP_PASSWORD, EE_WON_RTT_PASSWORD)
                };
                q.kind = kind;
                q.group_id = self.n16(G);
                q.text1 = self.text(which).to_vec();
            }
            T_START_SHUTDOWN => {
                q.kind = rt::EE_WON_RT_REQ_START_SHUTDOWN;
                q.text1 = self.text(EE_WON_RTT_TEXT).to_vec();
                q.longs[0] = self.n32(EE_WON_RTF_TIME);
            }
            T_UNSUBSCRIBE => {
                q.kind = rt::EE_WON_RT_REQ_UNSUBSCRIBE;
                q.link_id = self.n16(EE_WON_RTF_LINK_ID);
                q.byte1 = self.n8(EE_WON_RTF_FLAGS8);
            }
            _ => return None,
        }
        q.narrow = &self.data_type;
        // the data object and data requests carry the op's buffer
        q.data = match self.ty {
            T_CREATE_DATA | T_MODIFY_DATA | T_SEND_DATA => self.data.as_deref(),
            _ => None,
        };
        Some(Sent { message: rt::request(&q), add_op })
    }

    /// The state a reply's read depends on.
    fn state(&self) -> EeWonRouteState {
        EeWonRouteState {
            group_id: self.n16(EE_WON_RTF_GROUP_ID),
            join_flags: self.n8(EE_WON_RTF_JOIN_FLAGS),
            register_flags: self.n32(EE_WON_RTF_REGISTER_FLAGS),
            reply_count: self.num(EE_WON_RTF_REPLY_COUNT) as i32,
            replies_needed: self.num(EE_WON_RTF_REPLIES_NEEDED) as i32,
            waiting: self.on(EE_WON_RTF_WAITING),
            user_count: 0,
        }
    }

    /// `HandleReply(type, msg)` with the reader at `pos` of `data`; `pending`: the op's status is still pending
    /// (`CheckLeaveGroup` reads it).
    pub fn reply(&mut self, message: u8, data: &[u8], pos: usize, pending: bool) -> Read {
        let Some(kind) = reply_kind(self.ty) else {
            return Read { end: End::Status(rt::EE_WON_RT_DONT_WANT), leave_group: None };
        };
        let r = rt::reply(kind, message, data, pos, &self.state());
        let mut recs = Recs { temp: matches!(self.ty, T_REGISTER | T_JOIN_GROUP), ..Recs::default() };
        let mut leave_group = None;
        for e in &r.events {
            if self.record(&mut recs, e, data) {
                continue;
            }
            if e.tag == rt::EE_WON_RT_ACT && self.ty == T_CREATE_GROUP {
                // CheckLeaveGroup((short)status)
                if e.num as i16 == 0 && !pending && self.on(EE_WON_RTF_LEAVE_ON_CANCEL) {
                    self.set(EE_WON_RTF_LEAVE_ON_CANCEL, 0);
                    leave_group = Some(self.n16(EE_WON_RTF_GROUP_ID));
                }
                continue;
            }
            self.field(e, data);
        }
        Read { end: r.end, leave_group }
    }

    /// `ParseGroupListObj`: a group list body read into the group map; false when it ran short (what was read
    /// stays).
    pub fn parse_group_list(&mut self, data: &[u8]) -> bool {
        let r = rt::reply(rt::EE_WON_RT_GROUP_LIST_BODY, 0, data, 0, &EeWonRouteState::default());
        self.apply_group_list(&r.events, data);
        r.end == End::Status(rt::EE_WON_RT_SUCCESS)
    }

    /// A group list request op's replies, read again in order into this (connection-less) group list op as its
    /// `CheckResponse` read each one (a reply cut short keeps what it read; an invalid header reads nothing more).
    pub fn read_group_list_replies(&mut self, replies: &[Vec<u8>]) {
        for reply in replies {
            let r = rt::reply(rt::EE_WON_RT_GROUP_LIST_REQ, 0, reply, 0, &EeWonRouteState::default());
            self.apply_group_list(&r.events, reply);
        }
    }

    fn apply_group_list(&mut self, events: &[EeWonRouteEvent], data: &[u8]) {
        let mut recs = Recs::default();
        for e in events {
            if e.tag == rt::EE_WON_RT_ACT && e.slot == rt::EE_WON_RT_ACT_INVALID_HEADER {
                continue;
            }
            if !self.record(&mut recs, e, data) {
                self.set(EE_WON_RTF_FLAGS16, i64::from(e.num));
            }
        }
    }

    /// A record's event (`RouteRecords::Apply`); false for the op's own field or action.
    fn record(&mut self, r: &mut Recs, e: &EeWonRouteEvent, data: &[u8]) -> bool {
        let v = e.num;
        match e.tag {
            rt::EE_WON_RT_NEW => {
                match e.slot {
                    rt::EE_WON_RT_KIND_CLIENT => r.client = Some(shared(Client::default())),
                    rt::EE_WON_RT_KIND_MEMBER => r.member = Some(shared(Member::default())),
                    rt::EE_WON_RT_KIND_GROUP => r.group = Some(shared(Group::default())),
                    rt::EE_WON_RT_KIND_OBJECT => r.object = Some(shared(Object::default())),
                    rt::EE_WON_RT_KIND_BAD_USER => r.bad = BadUser::default(),
                    rt::EE_WON_RT_KIND_MEMBER_CLIENT => {
                        let c = shared(Client::default());
                        if let Some(m) = &r.member {
                            m.borrow_mut().client = Some(c.clone());
                        }
                        r.client = Some(c);
                    }
                    rt::EE_WON_RT_KIND_OP_CLIENT => {
                        let c = shared(Client::default());
                        self.op_client = Some(c.clone());
                        r.client = Some(c);
                    }
                    _ => {}
                }
                true
            }
            rt::EE_WON_RT_REC => {
                let c = |r: &Recs, f: &dyn Fn(&mut Client)| {
                    if let Some(c) = &r.client {
                        f(&mut c.borrow_mut());
                    }
                };
                let g = |r: &Recs, f: &dyn Fn(&mut Group)| {
                    if let Some(g) = &r.group {
                        f(&mut g.borrow_mut());
                    }
                };
                match e.slot {
                    rt::EE_WON_RT_CLIENT_ID => c(r, &|c| c.id = v as u16),
                    rt::EE_WON_RT_CLIENT_FLAGS => c(r, &|c| c.flags = v as u32),
                    rt::EE_WON_RT_CLIENT_USER_ID => c(r, &|c| c.user_id = v as u32),
                    rt::EE_WON_RT_CLIENT_ADDRESS => c(r, &|c| c.address = v as u32),
                    rt::EE_WON_RT_MEMBER_ID => {
                        if let Some(m) = &r.member {
                            m.borrow_mut().id = v as u16;
                        }
                    }
                    rt::EE_WON_RT_MEMBER_FLAGS => {
                        if let Some(m) = &r.member {
                            m.borrow_mut().flags = v as u8;
                        }
                    }
                    rt::EE_WON_RT_GROUP_ID => g(r, &|g| g.id = v as u16),
                    rt::EE_WON_RT_GROUP_CAPTAIN => g(r, &|g| g.captain = v as u16),
                    rt::EE_WON_RT_GROUP_MAX_CLIENTS => g(r, &|g| g.max_clients = v as u16),
                    rt::EE_WON_RT_GROUP_FLAGS => g(r, &|g| g.flags = v as u32),
                    rt::EE_WON_RT_GROUP_ASYNC_FLAGS => g(r, &|g| g.async_flags = v as u32),
                    rt::EE_WON_RT_GROUP_OBSERVERS => g(r, &|g| g.observers = v as u16),
                    rt::EE_WON_RT_GROUP_MAX_PLAYERS => g(r, &|g| g.max_players = v as u16),
                    rt::EE_WON_RT_OBJECT_LINK => {
                        if let Some(o) = &r.object {
                            o.borrow_mut().link = v as u16;
                        }
                    }
                    rt::EE_WON_RT_BAD_WON_ID => r.bad.won_id = v as u32,
                    rt::EE_WON_RT_BAD_EXPIRATION => r.bad.expiration = v as u32,
                    rt::EE_WON_RT_BAD_MODERATOR_ID => r.bad.moderator_id = v as u32,
                    _ => {}
                }
                true
            }
            rt::EE_WON_RT_REC_TEXT => {
                let t = units(data, e);
                match e.slot {
                    rt::EE_WON_RT_CLIENT_NAME => {
                        if let Some(c) = &r.client {
                            c.borrow_mut().name = t;
                        }
                    }
                    rt::EE_WON_RT_GROUP_NAME => {
                        if let Some(g) = &r.group {
                            g.borrow_mut().name = t;
                        }
                    }
                    rt::EE_WON_RT_OBJECT_NAME => {
                        if let Some(o) = &r.object {
                            o.borrow_mut().name = t;
                        }
                    }
                    rt::EE_WON_RT_BAD_NAME => r.bad.name = t,
                    rt::EE_WON_RT_BAD_MODERATOR_NAME => r.bad.moderator_name = t,
                    rt::EE_WON_RT_BAD_COMMENT => r.bad.comment = t,
                    _ => {}
                }
                true
            }
            rt::EE_WON_RT_REC_NARROW => {
                if e.slot == rt::EE_WON_RT_OBJECT_TYPE {
                    if let Some(o) = &r.object {
                        o.borrow_mut().data_type = span(data, e);
                    }
                }
                true
            }
            rt::EE_WON_RT_REC_BUF => {
                if e.slot == rt::EE_WON_RT_OBJECT_DATA {
                    if let Some(o) = &r.object {
                        o.borrow_mut().data = Some(span(data, e));
                    }
                }
                true
            }
            rt::EE_WON_RT_INSERT => {
                match e.slot {
                    rt::EE_WON_RT_INTO_CLIENTS => {
                        if let Some(c) = &r.client {
                            let id = c.borrow().id;
                            let target = if r.temp { &mut r.temp_clients } else { &mut self.clients };
                            target.insert(id, c.clone());
                        }
                    }
                    rt::EE_WON_RT_INTO_MEMBERS => {
                        if let Some(m) = &r.member {
                            let id = m.borrow().id;
                            let target = if r.temp { &mut r.temp_members } else { &mut self.members };
                            target.insert(id, m.clone());
                        }
                    }
                    rt::EE_WON_RT_INTO_GROUP_MEMBERS => {
                        if let (Some(g), Some(m)) = (&r.group, &r.member) {
                            let id = m.borrow().id;
                            g.borrow_mut().members.insert(id, m.clone());
                        }
                    }
                    rt::EE_WON_RT_INTO_GROUPS => {
                        if let Some(g) = &r.group {
                            let id = g.borrow().id;
                            self.groups.insert(id, g.clone());
                        }
                    }
                    rt::EE_WON_RT_INTO_OBJECTS => {
                        if let Some(o) = &r.object {
                            self.objects.push(o.clone());
                        }
                    }
                    rt::EE_WON_RT_INTO_BAD_USERS => self.bad_users.push(r.bad.clone()),
                    _ => {}
                }
                true
            }
            rt::EE_WON_RT_ACT => match e.slot {
                rt::EE_WON_RT_ACT_CLEAR_GROUPS => {
                    self.groups.clear();
                    true
                }
                rt::EE_WON_RT_ACT_CLEAR_BAD_USERS => {
                    self.bad_users.clear();
                    true
                }
                rt::EE_WON_RT_ACT_COMMIT_CLIENTS => {
                    self.clients = r.temp_clients.clone();
                    true
                }
                rt::EE_WON_RT_ACT_COMMIT_MEMBERS => {
                    self.members = r.temp_members.clone();
                    true
                }
                _ => false,
            },
            _ => false,
        }
    }

    /// The op's own member an event assigns (the templates' slot tables, per op type).
    fn field(&mut self, e: &EeWonRouteEvent, data: &[u8]) {
        const C: i32 = EE_WON_RTF_CLIENT_ID;
        const G: i32 = EE_WON_RTF_GROUP_ID;
        let s = e.slot;
        let v = i64::from(e.num);
        let text = |op: &mut RouteOp, which: i32| op.set_text(which, units(data, e));
        match self.ty {
            T_CLIENT_FLAGS_CHANGED => self.set(if s == 0 { C } else { EE_WON_RTF_CLIENT_FLAGS }, v),
            T_CLIENT_JOINED_GROUP => match s {
                0 => self.set(G, v),
                1 => self.set(C, v),
                2 => self.set(EE_WON_RTF_MEMBER_FLAGS, v),
                3 => text(self, EE_WON_RTT_NAME),
                4 => self.set(EE_WON_RTF_CLIENT_FLAGS, v),
                5 => self.set(EE_WON_RTF_HAS_CLIENT_FLAGS, v),
                _ => {}
            },
            T_CLIENT_LEFT_GROUP => self.set(if s == 0 { G } else { C }, v),
            T_CAPTAIN_CHANGED => self.set(if s == 0 { G } else { EE_WON_RTF_CAPTAIN_ID }, v),
            T_MAX_PLAYERS_CHANGED => self.set(if s == 0 { G } else { EE_WON_RTF_MAX_PLAYERS }, v),
            T_MEMBER_COUNT => self.set(if s == 0 { G } else { EE_WON_RTF_MEMBER_COUNT }, v),
            T_OBSERVER_COUNT => self.set(if s == 0 { G } else { EE_WON_RTF_OBSERVERS }, v),
            T_CLIENT_LEFT_SERVER => self.set(C, v),
            T_GROUP_DELETED => self.set(G, v),
            T_DATA_CREATED | T_DATA_DELETED | T_DATA_MODIFIED => {
                if e.tag == rt::EE_WON_RT_BUF {
                    self.data = Some(span(data, e));
                } else {
                    match s {
                        0 => self.set(EE_WON_RTF_LINK_ID, v),
                        1 => self.data_type = span(data, e),
                        2 => text(self, EE_WON_RTT_NAME),
                        3 => self.set(EE_WON_RTF_OFFSET, v),
                        _ => self.set(EE_WON_RTF_ON, v),
                    }
                }
            }
            T_GROUP_CREATED => match s {
                0 => self.set(G, v),
                1 => text(self, EE_WON_RTT_NAME),
                2 => self.set(EE_WON_RTF_MAX_CLIENTS, v),
                3 => self.set(EE_WON_RTF_GROUP_FLAGS, v),
                4 => self.set(EE_WON_RTF_ASYNC_FLAGS, v),
                _ => {}
            },
            T_GROUP_FLAGS_CHANGED => match s {
                0 => self.set(G, v),
                1 => self.set(EE_WON_RTF_GROUP_FLAGS, v),
                2 => self.set(EE_WON_RTF_ASYNC_FLAGS, v),
                _ => {}
            },
            T_INVITATION => match s {
                0 => self.set(G, v),
                1 => self.set(C, v),
                2 => self.set(EE_WON_RTF_ON, v),
                3 => text(self, EE_WON_RTT_COMMENT),
                _ => {}
            },
            T_JOIN_ATTEMPT => match s {
                0 => self.set(C, v),
                1 => self.set(G, v),
                2 => text(self, EE_WON_RTT_COMMENT),
                3 => self.set(EE_WON_RTF_FLAGS8, v),
                _ => {}
            },
            T_GROUP_NAME_CHANGED => {
                if s == 0 {
                    self.set(G, v)
                } else {
                    text(self, EE_WON_RTT_NAME)
                }
            }
            T_MEMBER_FLAGS_CHANGED => match s {
                0 => self.set(G, v),
                1 => self.set(C, v),
                2 => self.set(EE_WON_RTF_MEMBER_FLAGS, v),
                _ => {}
            },
            T_PEER_CHAT => {
                if e.tag == rt::EE_WON_RT_ACT {
                    self.ids.push(e.num as u16);
                } else if s == 0 {
                    self.set(C, v);
                } else if s == 1 {
                    self.set(EE_WON_RTF_FLAGS16, v);
                } else {
                    text(self, EE_WON_RTT_TEXT);
                }
            }
            T_PEER_DATA => {
                if s == 0 {
                    self.set(C, v)
                } else {
                    self.data = Some(span(data, e))
                }
            }
            T_SERVER_ALERT => text(self, EE_WON_RTT_TEXT),
            T_SHUTDOWN_STARTED => {
                if s == 0 {
                    text(self, EE_WON_RTT_TEXT)
                } else {
                    self.set(EE_WON_RTF_TIME, v)
                }
            }
            T_BANNED | T_MUTED => match s {
                0 => self.set(G, v),
                1 => self.set(EE_WON_RTF_ON, v),
                2 => text(self, EE_WON_RTT_COMMENT),
                3 => self.set(EE_WON_RTF_TIME, v),
                _ => {}
            },
            T_CREATE_GROUP => match s {
                0 => self.set(G, v),
                1 => text(self, EE_WON_RTT_NAME),
                _ => self.set(EE_WON_RTF_PARENT_ID, v),
            },
            T_JOIN_GROUP => text(self, EE_WON_RTT_COMMENT),
            T_REGISTER => {
                if e.tag == rt::EE_WON_RT_ACT {
                    let n = self.num(EE_WON_RTF_REPLY_COUNT);
                    self.set(EE_WON_RTF_REPLY_COUNT, n + 1);
                } else {
                    match s {
                        0 => self.set(C, v),
                        1 => text(self, EE_WON_RTT_NAME),
                        _ => self.set(EE_WON_RTF_RECONNECT_ID, v),
                    }
                }
            }
            T_CLIENT_LIST => self.set(EE_WON_RTF_IS_EX, v),
            T_GROUP_LIST => self.set(EE_WON_RTF_FLAGS16, v),
            T_MEMBERS => match s {
                0 => self.set(EE_WON_RTF_HAS_NAMES, v),
                1 => self.set(EE_WON_RTF_HAS_CLIENT_FLAGS, v),
                _ => self.set(EE_WON_RTF_OBSERVERS, v),
            },
            T_BAD_USERS => match s {
                0 => self.set(EE_WON_RTF_LIST_TYPE, v),
                1 => self.set(G, v),
                _ => self.set(EE_WON_RTF_FLAGS8, v),
            },
            T_DETECT_FIREWALL => self.set(if s == 0 { EE_WON_RTF_WAITING } else { EE_WON_RTF_LISTEN_PORT }, v),
            T_DERIVED => {
                if s == 0 {
                    self.set(EE_WON_RTF_MSG_TYPE, v)
                } else {
                    self.data = Some(span(data, e))
                }
            }
            _ => {}
        }
    }
}

// ---- the C ABI

/// A routing op, as the C++ `RoutingOp` holds it.
pub struct EeWonRouteOp(pub RouteOp);

/// A record copied out (`ee_won_routeop_records`): the fields of its kind are set; texts are UTF-16 units and the
/// pointers are valid only during the sink's call.
#[repr(C)]
pub struct EeWonRouteRecord {
    /// `EE_WON_RTR_*`
    pub kind: i32,
    /// its map's key
    pub key: u16,
    pub id: u16,
    pub flags: u32,
    pub user_id: u32,
    pub address: u32,
    pub trust: u16,
    pub parent: u16,
    pub captain: u16,
    pub max_clients: u16,
    pub async_flags: u32,
    pub observers: u16,
    pub max_players: u16,
    pub member_flags: u8,
    pub name: *const u16,
    pub name_len: usize,
    pub data_type: *const u8,
    pub data_type_len: usize,
    pub data: *const u8,
    pub data_len: usize,
    pub has_data: bool,
    pub expiration: u32,
    pub moderator_id: u32,
    pub moderator_name: *const u16,
    pub moderator_name_len: usize,
    pub comment: *const u16,
    pub comment_len: usize,
}

/// A client of `mClientMap`
pub const EE_WON_RTR_CLIENT: i32 = 0;
/// a member of `mMembers` / `mMemberMap`
pub const EE_WON_RTR_MEMBER: i32 = 1;
/// the last member's client (`mClientInfo`)
pub const EE_WON_RTR_MEMBER_CLIENT: i32 = 2;
/// a group of `mGroupMap`
pub const EE_WON_RTR_GROUP: i32 = 3;
/// a member of the last group
pub const EE_WON_RTR_GROUP_MEMBER: i32 = 4;
/// a data object of `mDataObjects`
pub const EE_WON_RTR_OBJECT: i32 = 5;
/// a bad user of `mBadUserList`
pub const EE_WON_RTR_BAD_USER: i32 = 6;
/// the client joined server notice's client
pub const EE_WON_RTR_OP_CLIENT: i32 = 7;

/// Receives one record; valid only during the call.
pub type EeWonRouteRecordSink = unsafe extern "C" fn(context: *mut c_void, record: *const EeWonRouteRecord);

const EMPTY_RECORD: EeWonRouteRecord = EeWonRouteRecord {
    kind: 0,
    key: 0,
    id: 0,
    flags: 0,
    user_id: 0,
    address: 0,
    trust: 0,
    parent: 0,
    captain: 0,
    max_clients: 0,
    async_flags: 0,
    observers: 0,
    max_players: 0,
    member_flags: 0,
    name: std::ptr::null(),
    name_len: 0,
    data_type: std::ptr::null(),
    data_type_len: 0,
    data: std::ptr::null(),
    data_len: 0,
    has_data: false,
    expiration: 0,
    moderator_id: 0,
    moderator_name: std::ptr::null(),
    moderator_name_len: 0,
    comment: std::ptr::null(),
    comment_len: 0,
};

fn client_record(kind: i32, key: u16, c: &Client) -> EeWonRouteRecord {
    EeWonRouteRecord {
        kind,
        key,
        id: c.id,
        flags: c.flags,
        user_id: c.user_id,
        address: c.address,
        trust: c.trust,
        name: c.name.as_ptr(),
        name_len: c.name.len(),
        ..EMPTY_RECORD
    }
}

unsafe fn op_ref<'a>(op: *const EeWonRouteOp) -> &'a RouteOp {
    // SAFETY: a live op from ee_won_routeop_new (contract)
    unsafe { &(*op).0 }
}

unsafe fn op_mut<'a>(op: *mut EeWonRouteOp) -> &'a mut RouteOp {
    // SAFETY: as above
    unsafe { &mut (*op).0 }
}

/// A new op of a type (`RoutingOpType`) with its constructor's members.
#[no_mangle]
pub extern "C" fn ee_won_routeop_new(ty: i32) -> *mut EeWonRouteOp {
    Box::into_raw(Box::new(EeWonRouteOp(RouteOp::new(ty))))
}

/// Frees an op.
///
/// # Safety
/// `op` is from `ee_won_routeop_new` (or null), not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_free(op: *mut EeWonRouteOp) {
    if !op.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(op) });
    }
}

/// The op's type (`GetType`).
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_type(op: *const EeWonRouteOp) -> i32 {
    unsafe { op_ref(op) }.ty
}

/// The op type a notification kind (routemsg's `EE_WON_RT_N_*`) makes (0 for none).
#[no_mangle]
pub extern "C" fn ee_won_routeop_notice_type(kind: i32) -> i32 {
    notice_type(kind)
}

/// A numeric member (`EE_WON_RTF_*`).
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_num(op: *const EeWonRouteOp, field: i32) -> i64 {
    unsafe { op_ref(op) }.num(field)
}

/// Assigns a numeric member (cut to its width).
///
/// # Safety
/// `op` is a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_set_num(op: *mut EeWonRouteOp, field: i32, value: i64) {
    unsafe { op_mut(op) }.set(field, value);
}

/// A text member (`EE_WON_RTT_*`) as UTF-16 units into `sink`.
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_text(
    op: *const EeWonRouteOp,
    which: i32,
    sink: crate::ffi::EeWonUnitSink,
    context: *mut c_void,
) {
    let t = unsafe { op_ref(op) }.text(which);
    unsafe { sink(context, t.as_ptr(), t.len()) }
}

/// Assigns a text member from `count` UTF-16 units.
///
/// # Safety
/// `op` is a live op; `units` holds `count` units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_set_text(op: *mut EeWonRouteOp, which: i32, units: *const u16, count: usize) {
    let t = if units.is_null() || count == 0 {
        Vec::new()
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(units, count) }.to_vec()
    };
    unsafe { op_mut(op) }.set_text(which, t);
}

/// `mDataType` into `sink`.
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_data_type(op: *const EeWonRouteOp, sink: EeWonByteSink, context: *mut c_void) {
    let o = unsafe { op_ref(op) };
    unsafe { crate::ffi::give(sink, context, &o.data_type) }
}

/// Assigns `mDataType`.
///
/// # Safety
/// `op` is a live op; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_set_data_type(op: *mut EeWonRouteOp, data: *const u8, length: usize) {
    let d = unsafe { crate::ffi::bytes(data, length) }.to_vec();
    unsafe { op_mut(op) }.data_type = d;
}

/// `mData` into `sink`; false (the sink not called) for a null buffer.
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_data(
    op: *const EeWonRouteOp,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match &unsafe { op_ref(op) }.data {
        Some(d) => {
            unsafe { crate::ffi::give(sink, context, d) };
            true
        }
        None => false,
    }
}

/// Assigns `mData` (`present` false: a null buffer).
///
/// # Safety
/// `op` is a live op; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_set_data(op: *mut EeWonRouteOp, present: bool, data: *const u8, length: usize) {
    let d = present.then(|| unsafe { crate::ffi::bytes(data, length) }.to_vec());
    unsafe { op_mut(op) }.data = d;
}

/// The recipients: copies up to `capacity` into `out`, returns how many there are.
///
/// # Safety
/// `op` is a live op; `out` holds `capacity` entries (or is null with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_ids(op: *const EeWonRouteOp, out: *mut u16, capacity: usize) -> usize {
    let ids = &unsafe { op_ref(op) }.ids;
    for (i, &id) in ids.iter().take(capacity).enumerate() {
        // SAFETY: `i` < capacity
        unsafe { *out.add(i) = id };
    }
    ids.len()
}

/// Assigns the recipients.
///
/// # Safety
/// `op` is a live op; `ids` holds `count` entries.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_set_ids(op: *mut EeWonRouteOp, ids: *const u16, count: usize) {
    let v = if ids.is_null() || count == 0 {
        Vec::new()
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(ids, count) }.to_vec()
    };
    unsafe { op_mut(op) }.ids = v;
}

/// `SendRequest`: the message into `sink` (not called when the op sends nothing); returns 0 for nothing sent, 1
/// sent, 3 sent and the op waits for its reply (`AddOp`).
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_request(
    op: *mut EeWonRouteOp,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> i32 {
    match unsafe { op_mut(op) }.request() {
        Some(s) => {
            unsafe { crate::ffi::give(sink, context, &s.message) };
            if s.add_op {
                3
            } else {
                1
            }
        }
        None => 0,
    }
}

/// How `ee_won_routeop_reply` ended.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonRouteOpRead {
    /// the status `HandleReply` returns (when not cut)
    pub status: i32,
    /// the original threw `ReadBufferException` after its assignments
    pub cut: bool,
    /// `CheckLeaveGroup` leaves `leave_group` (a `RoutingLeaveGroupOp` run async)
    pub leave: bool,
    pub leave_group: u16,
}

/// `HandleReply(message, msg)`, the reader at `pos` of `data` (`length` bytes); `pending`: the op's status is
/// pending.
///
/// # Safety
/// `op` is a live op; `data` holds `length` bytes; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_reply(
    op: *mut EeWonRouteOp,
    message: u8,
    data: *const u8,
    length: usize,
    pos: usize,
    pending: bool,
    out: *mut EeWonRouteOpRead,
) {
    let d = unsafe { crate::ffi::bytes(data, length) };
    let r = unsafe { op_mut(op) }.reply(message, d, pos, pending);
    let (status, cut) = match r.end {
        End::Status(s) => (s, false),
        End::Cut => (0, true),
    };
    unsafe {
        *out =
            EeWonRouteOpRead { status, cut, leave: r.leave_group.is_some(), leave_group: r.leave_group.unwrap_or(0) };
    }
}

/// `ParseGroupListObj`: a group list body (`length` bytes) into the op's group map; false when it ran short.
///
/// # Safety
/// `op` is a live op; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_parse_group_list(
    op: *mut EeWonRouteOp,
    data: *const u8,
    length: usize,
) -> bool {
    let d = unsafe { crate::ffi::bytes(data, length) };
    unsafe { op_mut(op) }.parse_group_list(d)
}

/// A group list request op's (`RustServerOp::NewGroupListReq`, reqlast.rs) checked replies read again into this
/// connection-less group list op (`RouteGroupListOf`).
///
/// # Safety
/// `op` is a live op; `host` is a live host op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_read_group_list_of(op: *mut EeWonRouteOp, host: *const EeWonHostOp) {
    let h = unsafe { &*host };
    let n = h.num(EE_WON_REQOP_GROUP_REPLIES, 0);
    let replies: Vec<Vec<u8>> = (0..n).map(|k| h.bytes(EE_WON_REQOP_GROUP_REPLY, k).unwrap_or_default()).collect();
    unsafe { op_mut(op) }.read_group_list_replies(&replies);
}

/// The op's records of one container, each into `sink` in the container's order: `EE_WON_RTR_CLIENT` (the client
/// map), `EE_WON_RTR_MEMBER` (the member map; each member with a client is followed by its
/// `EE_WON_RTR_MEMBER_CLIENT`), `EE_WON_RTR_GROUP` (the group map; each group followed by its
/// `EE_WON_RTR_GROUP_MEMBER`s), `EE_WON_RTR_OBJECT`, `EE_WON_RTR_BAD_USER`, `EE_WON_RTR_OP_CLIENT` (none when
/// the op has no client). The sink must not call back into the op.
///
/// # Safety
/// `op` is a live op; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_records(
    op: *const EeWonRouteOp,
    which: i32,
    sink: EeWonRouteRecordSink,
    context: *mut c_void,
) {
    let o = unsafe { op_ref(op) };
    let give = |r: &EeWonRouteRecord| unsafe { sink(context, r) };
    let member = |kind: i32, key: u16, m: &Member| {
        give(&EeWonRouteRecord { kind, key, id: m.id, member_flags: m.flags, ..EMPTY_RECORD });
        if let Some(c) = &m.client {
            let c = c.borrow();
            give(&client_record(EE_WON_RTR_MEMBER_CLIENT, c.id, &c));
        }
    };
    match which {
        EE_WON_RTR_CLIENT => {
            for (&k, c) in &o.clients {
                give(&client_record(EE_WON_RTR_CLIENT, k, &c.borrow()));
            }
        }
        EE_WON_RTR_MEMBER => {
            for (&k, m) in &o.members {
                member(EE_WON_RTR_MEMBER, k, &m.borrow());
            }
        }
        EE_WON_RTR_GROUP => {
            for (&k, g) in &o.groups {
                let g = g.borrow();
                give(&EeWonRouteRecord {
                    kind: EE_WON_RTR_GROUP,
                    key: k,
                    id: g.id,
                    flags: g.flags,
                    parent: g.parent,
                    captain: g.captain,
                    max_clients: g.max_clients,
                    async_flags: g.async_flags,
                    observers: g.observers,
                    max_players: g.max_players,
                    name: g.name.as_ptr(),
                    name_len: g.name.len(),
                    ..EMPTY_RECORD
                });
                for (&mk, m) in &g.members {
                    member(EE_WON_RTR_GROUP_MEMBER, mk, &m.borrow());
                }
            }
        }
        EE_WON_RTR_OBJECT => {
            for ob in &o.objects {
                let ob = ob.borrow();
                give(&EeWonRouteRecord {
                    kind: EE_WON_RTR_OBJECT,
                    key: ob.link,
                    id: ob.link,
                    name: ob.name.as_ptr(),
                    name_len: ob.name.len(),
                    data_type: ob.data_type.as_ptr(),
                    data_type_len: ob.data_type.len(),
                    data: ob.data.as_ref().map_or(std::ptr::null(), |d| d.as_ptr()),
                    data_len: ob.data.as_ref().map_or(0, Vec::len),
                    has_data: ob.data.is_some(),
                    ..EMPTY_RECORD
                });
            }
        }
        EE_WON_RTR_BAD_USER => {
            for b in &o.bad_users {
                give(&EeWonRouteRecord {
                    kind: EE_WON_RTR_BAD_USER,
                    name: b.name.as_ptr(),
                    name_len: b.name.len(),
                    user_id: b.won_id,
                    expiration: b.expiration,
                    moderator_id: b.moderator_id,
                    moderator_name: b.moderator_name.as_ptr(),
                    moderator_name_len: b.moderator_name.len(),
                    comment: b.comment.as_ptr(),
                    comment_len: b.comment.len(),
                    ..EMPTY_RECORD
                });
            }
        }
        EE_WON_RTR_OP_CLIENT => {
            if let Some(c) = &o.op_client {
                let c = c.borrow();
                give(&client_record(EE_WON_RTR_OP_CLIENT, c.id, &c));
            }
        }
        _ => {}
    }
}

#[cfg(test)]
#[path = "routeop_tests.rs"]
mod tests;
