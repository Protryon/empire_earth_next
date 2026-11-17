//! The routing classes' C++ calls over the library's core table (net round 26). `RoutingConnection`'s op list and
//! reply dispatch were a Rust object since round 16 ([`EeWonRouteConn`]) with 14 hooks answered in the C++; its
//! request helpers (RoutingConnection.cpp, 40 of them) made a `RoutingOp` of a type, set its members and ran it;
//! `RoutingOp::SendRequest` sent the Rust op's message and `HandleReply` read the reply. All of that is here:
//!
//! - [`NATIVE_ROUTECONN`]: the connection's hooks answered on the Rust objects (an op's calls on its
//!   [`EeWonAsyncOp`](crate::opobj::EeWonAsyncOp), its type and reply on its [`EeWonRouteOp`], the server
//!   connection's hooks on its [`EeWonServerConn`](crate::servconn::EeWonServerConn)); the C++ is asked only for a
//!   new `RoutingOp` and the two virtuals a derived connection may override (`RoutingOpCompleteHook`,
//!   `GetUnsolicitedDerivedOp`).
//! - [`ee_won_routeconn_request`]: a request helper by kind (`EE_WON_RTQ_*`), its values in the helper's parameter order,
//!   set on a new op's members as the helper set them, then the op run async.
//! - [`ee_won_routeop_send`]: `RoutingOp::SendRequest`'s base: the message after the connection's length field,
//!   queued on the connection, and the op added to its list when it waits for a reply.
//! - [`ee_won_smart_handle_input`]: `SmartRoutingConnection::HandleInput`: the chat input's action sent as its
//!   request.

use std::ffi::{c_long, c_void};
use std::ptr::null_mut;

use crate::buffer::EeWonWriteBuffer;
use crate::cppcore::{
    core, hold, n_bytes_adopt, n_op_completion, n_op_kill, n_op_run_async, n_op_set_completion, n_ref_add,
    n_ref_release, object_of, op, queue, ref_release, socket, EeWonCoreObject, EeWonCoreSocketOp,
};
use crate::routeconn::{EeWonRouteConn, EeWonRouteConnHooks};
use crate::routeop::*;
use crate::servconn::{ee_won_servconn_close_hook, ee_won_servconn_kill_hook};
use crate::smartroute::*;
use crate::status::WS_PENDING;

/// `CreateGroup(group, name, password, max players, group flags, join flags, async flags)`
pub const EE_WON_RTQ_CREATE_GROUP: i32 = 0;
/// `JoinGroup(group, password, comment, join flags, member flags)`
pub const EE_WON_RTQ_JOIN_GROUP: i32 = 1;
/// `BanClient(group, on, seconds, explanation, client id)`; by WON id; by CD key id
pub const EE_WON_RTQ_BAN: i32 = 2;
pub const EE_WON_RTQ_BAN_WON_ID: i32 = 3;
pub const EE_WON_RTQ_BAN_CDKEY_ID: i32 = 4;
/// `MuteClient(group, on, seconds, explanation, client id)`; by WON id; by CD key id
pub const EE_WON_RTQ_MUTE: i32 = 5;
pub const EE_WON_RTQ_MUTE_WON_ID: i32 = 6;
pub const EE_WON_RTQ_MUTE_CDKEY_ID: i32 = 7;
/// `Register(name, password, reconnect id, async flags, register flags, client flags)`
pub const EE_WON_RTQ_REGISTER: i32 = 8;
/// `GetClientList()`, `GetClientListEx()`
pub const EE_WON_RTQ_CLIENT_LIST: i32 = 9;
pub const EE_WON_RTQ_CLIENT_LIST_EX: i32 = 10;
/// `GetGroupList(flags)`
pub const EE_WON_RTQ_GROUP_LIST: i32 = 11;
/// `GetMembersOfGroup(group)`
pub const EE_WON_RTQ_MEMBERS: i32 = 12;
/// `LeaveGroup(group)`
pub const EE_WON_RTQ_LEAVE: i32 = 13;
/// `CancelJoinGroup(group, leave)`
pub const EE_WON_RTQ_CANCEL_JOIN: i32 = 14;
/// `AcceptClient(group, client, on, explanation)`, `InviteClient(...)`
pub const EE_WON_RTQ_ACCEPT: i32 = 15;
pub const EE_WON_RTQ_INVITE: i32 = 16;
/// `SetGroupFlags(group, mask, flags, async mask, async flags)`
pub const EE_WON_RTQ_SET_GROUP_FLAGS: i32 = 17;
/// `SendServerAlert(text)`; `SendServerAlertWithRecipientList(text, recipients)` (nothing for none)
pub const EE_WON_RTQ_ALERT: i32 = 18;
pub const EE_WON_RTQ_ALERT_TO: i32 = 19;
/// `StartServerShutdown(text, seconds)`, `AbortServerShutdown()`
pub const EE_WON_RTQ_START_SHUTDOWN: i32 = 20;
pub const EE_WON_RTQ_ABORT_SHUTDOWN: i32 = 21;
/// `SetClientFlags(mask, flags, async mask, async flags)` (the mask cut to 16 bits)
pub const EE_WON_RTQ_SET_CLIENT_FLAGS: i32 = 22;
/// `SendChatTo(text, flags, recipients)` (`SendChat`: one recipient)
pub const EE_WON_RTQ_CHAT: i32 = 23;
/// `SendData(data, flags, recipients)`
pub const EE_WON_RTQ_DATA: i32 = 24;
/// `BecomeModerator(on)`
pub const EE_WON_RTQ_MODERATOR: i32 = 25;
/// `CreateDataObject(flags, link, type, name, data)`
pub const EE_WON_RTQ_CREATE_DATA: i32 = 26;
/// `ReplaceDataObject(link, type, name)`: the delete request
pub const EE_WON_RTQ_DELETE_DATA: i32 = 27;
/// `ModifyDataObject(link, type, name, offset, insert, data)`
pub const EE_WON_RTQ_MODIFY_DATA: i32 = 28;
/// `ReadDataObject` / `SubscribeDataObject` / `GetClientCDKeyIdList`: (subscribe, link, type, flags)
pub const EE_WON_RTQ_READ_DATA: i32 = 29;
/// `UnsubscribeDataObject(link, type, flags)`
pub const EE_WON_RTQ_UNSUBSCRIBE_DATA: i32 = 30;
/// `DisconnectClient()`
pub const EE_WON_RTQ_DISCONNECT: i32 = 31;
/// `SetGroupName(group, name)`, `SetGroupMaxPlayers(group, max)`, `RelinquishCaptaincy(group, new captain)`
pub const EE_WON_RTQ_SET_GROUP_NAME: i32 = 32;
pub const EE_WON_RTQ_SET_GROUP_MAX_PLAYERS: i32 = 33;
pub const EE_WON_RTQ_RELINQUISH: i32 = 34;
/// `DetectFirewall(listen port, longest connect wait, listen)` (a `RoutingDetectFirewallOp`)
pub const EE_WON_RTQ_DETECT_FIREWALL: i32 = 35;
/// `GetBadUserList(list type, group, flags)`
pub const EE_WON_RTQ_BAD_USERS: i32 = 36;

// the op types (WONRoutingOpTypes.h)
const T_ABORT_SHUTDOWN: i32 = 1;
const T_ACCEPT: i32 = 2;
const T_BAN: i32 = 3;
const T_MODERATOR: i32 = 4;
const T_CANCEL_JOIN: i32 = 5;
const T_CREATE_DATA: i32 = 11;
const T_CREATE_GROUP: i32 = 12;
const T_DELETE_DATA: i32 = 16;
const T_DETECT_FIREWALL: i32 = 17;
const T_DISCONNECT: i32 = 18;
const T_BAD_USERS: i32 = 19;
const T_CLIENT_LIST: i32 = 20;
const T_GROUP_LIST: i32 = 21;
const T_MEMBERS: i32 = 22;
const T_INVITE: i32 = 33;
const T_JOIN: i32 = 34;
const T_LEAVE: i32 = 35;
const T_MODIFY_DATA: i32 = 37;
const T_MUTE: i32 = 38;
const T_READ_DATA: i32 = 41;
const T_REGISTER: i32 = 42;
const T_RELINQUISH: i32 = 43;
const T_CHAT: i32 = 44;
const T_DATA: i32 = 46;
const T_ALERT: i32 = 47;
const T_SET_CLIENT_FLAGS: i32 = 51;
const T_SET_GROUP_FLAGS: i32 = 52;
const T_SET_GROUP_MAX_PLAYERS: i32 = 53;
const T_SET_GROUP_NAME: i32 = 54;
const T_START_SHUTDOWN: i32 = 56;
const T_UNSUBSCRIBE_DATA: i32 = 57;
const T_DERIVED: i32 = 60;

/// What a helper's parameter sets on the op.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Slot {
    /// a numeric member (`EE_WON_RTF_*`)
    N(i32),
    /// a numeric member cut to 16 bits first (`SetClientFlags`' client mask)
    N16(i32),
    /// a text member (`EE_WON_RTT_*`)
    T(i32),
    /// the data type
    Type,
    /// the data (a buffer or none)
    Data,
    /// the recipients
    Ids,
    /// the client a ban or mute names: by client id (0), WON id (1) or CD key id (2)
    Who(u8),
}

use Slot::*;

/// A request kind's op type, its parameters' slots in order and the members it sets besides.
type Plan = (i32, &'static [Slot], &'static [(i32, i64)]);

fn plan(kind: i32) -> Option<Plan> {
    const G: i32 = EE_WON_RTF_GROUP_ID;
    const MODERATE: [Slot; 4] = [N(G), N(EE_WON_RTF_ON), N(EE_WON_RTF_TIME), T(EE_WON_RTT_COMMENT)];
    const BY_CLIENT: [Slot; 5] = [MODERATE[0], MODERATE[1], MODERATE[2], MODERATE[3], Who(0)];
    const BY_WON: [Slot; 5] = [MODERATE[0], MODERATE[1], MODERATE[2], MODERATE[3], Who(1)];
    const BY_KEY: [Slot; 5] = [MODERATE[0], MODERATE[1], MODERATE[2], MODERATE[3], Who(2)];
    const ANSWER: [Slot; 4] = [N(G), N(EE_WON_RTF_CLIENT_ID), N(EE_WON_RTF_ON), T(EE_WON_RTT_COMMENT)];
    Some(match kind {
        EE_WON_RTQ_CREATE_GROUP => (
            T_CREATE_GROUP,
            &[
                N(G),
                T(EE_WON_RTT_NAME),
                T(EE_WON_RTT_PASSWORD),
                N(EE_WON_RTF_MAX_PLAYERS),
                N(EE_WON_RTF_GROUP_FLAGS),
                N(EE_WON_RTF_JOIN_FLAGS),
                N(EE_WON_RTF_ASYNC_FLAGS),
            ],
            &[],
        ),
        EE_WON_RTQ_JOIN_GROUP => (
            T_JOIN,
            &[
                N(G),
                T(EE_WON_RTT_PASSWORD),
                T(EE_WON_RTT_COMMENT),
                N(EE_WON_RTF_JOIN_FLAGS),
                N(EE_WON_RTF_MEMBER_FLAGS),
            ],
            &[],
        ),
        EE_WON_RTQ_BAN => (T_BAN, &BY_CLIENT, &[]),
        EE_WON_RTQ_BAN_WON_ID => (T_BAN, &BY_WON, &[]),
        EE_WON_RTQ_BAN_CDKEY_ID => (T_BAN, &BY_KEY, &[]),
        EE_WON_RTQ_MUTE => (T_MUTE, &BY_CLIENT, &[]),
        EE_WON_RTQ_MUTE_WON_ID => (T_MUTE, &BY_WON, &[]),
        EE_WON_RTQ_MUTE_CDKEY_ID => (T_MUTE, &BY_KEY, &[]),
        EE_WON_RTQ_REGISTER => (
            T_REGISTER,
            &[
                T(EE_WON_RTT_NAME),
                T(EE_WON_RTT_PASSWORD),
                N(EE_WON_RTF_RECONNECT_ID),
                N(EE_WON_RTF_ASYNC_MSG_FLAGS),
                N(EE_WON_RTF_REGISTER_FLAGS),
                N(EE_WON_RTF_CLIENT_FLAGS),
            ],
            &[],
        ),
        EE_WON_RTQ_CLIENT_LIST => (T_CLIENT_LIST, &[], &[]),
        EE_WON_RTQ_CLIENT_LIST_EX => (T_CLIENT_LIST, &[], &[(EE_WON_RTF_IS_EX, 1)]),
        EE_WON_RTQ_GROUP_LIST => (T_GROUP_LIST, &[N(EE_WON_RTF_FLAGS16)], &[]),
        EE_WON_RTQ_MEMBERS => (T_MEMBERS, &[N(G)], &[]),
        EE_WON_RTQ_LEAVE => (T_LEAVE, &[N(G)], &[]),
        EE_WON_RTQ_CANCEL_JOIN => (T_CANCEL_JOIN, &[N(G), N(EE_WON_RTF_ON)], &[]),
        EE_WON_RTQ_ACCEPT => (T_ACCEPT, &ANSWER, &[]),
        EE_WON_RTQ_INVITE => (T_INVITE, &ANSWER, &[]),
        EE_WON_RTQ_SET_GROUP_FLAGS => (
            T_SET_GROUP_FLAGS,
            &[
                N(G),
                N(EE_WON_RTF_GROUP_MASK),
                N(EE_WON_RTF_GROUP_FLAGS),
                N(EE_WON_RTF_ASYNC_MASK),
                N(EE_WON_RTF_ASYNC_FLAGS),
            ],
            &[],
        ),
        EE_WON_RTQ_ALERT => (T_ALERT, &[T(EE_WON_RTT_TEXT)], &[]),
        EE_WON_RTQ_ALERT_TO => (T_ALERT, &[T(EE_WON_RTT_TEXT), Ids], &[]),
        EE_WON_RTQ_START_SHUTDOWN => (T_START_SHUTDOWN, &[T(EE_WON_RTT_TEXT), N(EE_WON_RTF_TIME)], &[]),
        EE_WON_RTQ_ABORT_SHUTDOWN => (T_ABORT_SHUTDOWN, &[], &[]),
        EE_WON_RTQ_SET_CLIENT_FLAGS => (
            T_SET_CLIENT_FLAGS,
            &[
                N16(EE_WON_RTF_CLIENT_MASK),
                N(EE_WON_RTF_CLIENT_FLAGS),
                N(EE_WON_RTF_ASYNC_MSG_MASK),
                N(EE_WON_RTF_ASYNC_MSG_FLAGS),
            ],
            &[],
        ),
        EE_WON_RTQ_CHAT => (T_CHAT, &[T(EE_WON_RTT_TEXT), N(EE_WON_RTF_FLAGS16), Ids], &[]),
        EE_WON_RTQ_DATA => (T_DATA, &[Data, N(EE_WON_RTF_FLAGS8), Ids], &[]),
        EE_WON_RTQ_MODERATOR => (T_MODERATOR, &[N(EE_WON_RTF_ON)], &[]),
        EE_WON_RTQ_CREATE_DATA => {
            (T_CREATE_DATA, &[N(EE_WON_RTF_FLAGS8), N(EE_WON_RTF_LINK_ID), Type, T(EE_WON_RTT_NAME), Data], &[])
        }
        EE_WON_RTQ_DELETE_DATA => (T_DELETE_DATA, &[N(EE_WON_RTF_LINK_ID), Type, T(EE_WON_RTT_NAME)], &[]),
        EE_WON_RTQ_MODIFY_DATA => (
            T_MODIFY_DATA,
            &[N(EE_WON_RTF_LINK_ID), Type, T(EE_WON_RTT_NAME), N(EE_WON_RTF_OFFSET), N(EE_WON_RTF_ON), Data],
            &[],
        ),
        EE_WON_RTQ_READ_DATA => {
            (T_READ_DATA, &[N(EE_WON_RTF_ON), N(EE_WON_RTF_LINK_ID), Type, N(EE_WON_RTF_FLAGS8)], &[])
        }
        EE_WON_RTQ_UNSUBSCRIBE_DATA => (T_UNSUBSCRIBE_DATA, &[N(EE_WON_RTF_LINK_ID), Type, N(EE_WON_RTF_FLAGS8)], &[]),
        EE_WON_RTQ_DISCONNECT => (T_DISCONNECT, &[], &[]),
        EE_WON_RTQ_SET_GROUP_NAME => (T_SET_GROUP_NAME, &[N(G), T(EE_WON_RTT_NAME)], &[]),
        EE_WON_RTQ_SET_GROUP_MAX_PLAYERS => (T_SET_GROUP_MAX_PLAYERS, &[N(G), N(EE_WON_RTF_MAX_PLAYERS)], &[]),
        EE_WON_RTQ_RELINQUISH => (T_RELINQUISH, &[N(G), N(EE_WON_RTF_CLIENT_ID)], &[]),
        EE_WON_RTQ_DETECT_FIREWALL => {
            (T_DETECT_FIREWALL, &[N(EE_WON_RTF_LISTEN_PORT), N(EE_WON_RTF_MAX_WAIT), N(EE_WON_RTF_DO_LISTEN)], &[])
        }
        EE_WON_RTQ_BAD_USERS => (T_BAD_USERS, &[N(EE_WON_RTF_LIST_TYPE), N(G), N(EE_WON_RTF_FLAGS8)], &[]),
        _ => return None,
    })
}

/// A parameter's value: a number, or bytes (a text's UTF-16 little-endian units, a data type, a buffer, the
/// recipients as 16-bit little-endian ids); a buffer's number says whether there is one.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub(crate) struct Value {
    pub num: i64,
    pub bytes: Vec<u8>,
}

impl Value {
    pub(crate) fn num(n: i64) -> Value {
        Value { num: n, bytes: Vec::new() }
    }
    pub(crate) fn units(u: &[u16]) -> Value {
        Value { num: 0, bytes: u.iter().flat_map(|x| x.to_le_bytes()).collect() }
    }
    fn u16s(&self) -> Vec<u16> {
        self.bytes.as_chunks::<2>().0.iter().map(|c| u16::from_le_bytes(*c)).collect()
    }
}

/// Sets the request's values on a Rust routing op as its helper set them on the C++ op's members.
fn fill(o: &mut RouteOp, slots: &[Slot], extra: &[(i32, i64)], values: &[Value]) {
    let none = Value::default();
    for (i, slot) in slots.iter().enumerate() {
        let v = values.get(i).unwrap_or(&none);
        match *slot {
            N(f) => o.set(f, v.num),
            N16(f) => o.set(f, v.num as u16 as i64),
            T(w) => o.set_text(w, v.u16s()),
            Type => o.data_type = v.bytes.clone(),
            Data => o.data = (v.num != 0).then(|| v.bytes.clone()),
            Ids => o.ids = v.u16s(),
            Who(by) => {
                o.set(if by == 0 { EE_WON_RTF_CLIENT_ID } else { EE_WON_RTF_WON_ID }, v.num);
                o.set(EE_WON_RTF_BY_WON_ID, (by != 0) as i64);
                if by != 0 {
                    o.set(EE_WON_RTF_IS_CDKEY, (by == 2) as i64);
                }
            }
        }
    }
    for &(f, n) in extra {
        o.set(f, n);
    }
}

/// Whether the request is sent: `SendServerAlertWithRecipientList` sends nothing without a recipient.
fn sends(kind: i32, values: &[Value]) -> bool {
    kind != EE_WON_RTQ_ALERT_TO || values.get(1).is_some_and(|v| v.bytes.len() >= 2)
}

/// The Rust op of a C++ `RoutingOp` (null for none).
fn route_op(o: *mut c_void) -> *mut EeWonRouteOp {
    object_of(o, EeWonCoreObject::RouteOp).cast()
}
/// A `RoutingConnection*`'s Rust object (none for another connection).
pub(crate) fn route_conn_of<'a>(conn: *mut c_void) -> Option<&'a EeWonRouteConn> {
    route_conn(conn)
}
fn route_conn<'a>(conn: *mut c_void) -> Option<&'a EeWonRouteConn> {
    // SAFETY: a connection's Rust object lives as long as the connection, which the caller holds
    unsafe { (object_of(conn, EeWonCoreObject::RouteConn) as *const EeWonRouteConn).as_ref() }
}
fn new_route_op(conn: *mut c_void, ty: i32) -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.new_route_op).map_or(null_mut(), |h| unsafe { h(conn, ty) })
}

/// A request helper: a new op of the kind's type on the connection, its completion, its members from the values,
/// run async (a local reference held across the run, as the helpers' `SmartPtr`).
pub(crate) fn request(conn: *mut c_void, kind: i32, values: &[Value], completion: *mut c_void) {
    let Some((ty, slots, extra)) = plan(kind) else { return };
    if !sends(kind, values) {
        return;
    }
    let o = new_route_op(conn, ty);
    if o.is_null() {
        return;
    }
    // SAFETY: the new op; it takes its own reference to the completion
    unsafe { n_op_set_completion(o, completion) };
    let r = route_op(o);
    if !r.is_null() {
        // SAFETY: the new op's Rust object; no C++ call while it is borrowed
        fill(unsafe { &mut (*r).0 }, slots, extra, values);
    }
    let _local = hold(o);
    // SAFETY: the held op
    unsafe { n_op_run_async(o, -1) };
}

/// A request helper of a kind (`EE_WON_RTQ_*`) on the connection `conn` (a `RoutingConnection*`), its `count`
/// values in the helper's parameter order (`num` a number or a buffer's presence; `data` / `len` a text's UTF-16
/// little-endian units, a data type, a buffer's bytes, the recipients as 16-bit little-endian ids), with the
/// completion (or null).
///
/// # Safety
/// `conn` is a live connection; `values` holds `count` values whose bytes are valid for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_request(
    conn: *mut c_void,
    kind: i32,
    values: *const crate::admin::EeWonAdminValue,
    count: usize,
    completion: *mut c_void,
) {
    let raw = if values.is_null() || count == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(values, count) }
    };
    let v: Vec<Value> = raw
        .iter()
        // SAFETY: the caller's bytes
        .map(|x| Value { num: x.num, bytes: unsafe { crate::ffi::bytes(x.data, x.len) }.to_vec() })
        .collect();
    request(conn, kind, &v, completion)
}

/// A new routing op of a type (`RoutingOpType`) on the connection `conn` (borrowed: the C++ op holds its reference;
/// the Rust op sends on it and leaves a group on it).
#[no_mangle]
pub extern "C" fn ee_won_routeop_new_on(ty: i32, conn: *mut c_void) -> *mut EeWonRouteOp {
    Box::into_raw(Box::new(EeWonRouteOp(RouteOp::new(ty), conn as usize)))
}

/// `RoutingOp::SendRequest`'s base: the op's message (when it sends one) after the connection's length field,
/// queued on the connection as a `SendMsgOp`; the op `cpp` added to the connection's list when it waits for a reply.
///
/// # Safety
/// `op` is the live Rust op of the C++ op `cpp` (a `RoutingOp*` as its `RefCount*`).
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeop_send(op: *mut EeWonRouteOp, cpp: *mut c_void) {
    // SAFETY: the caller's contract; the borrow ends before any C++ call
    let (sent, conn) = unsafe { ((*op).0.request(), (*op).1 as *mut c_void) };
    let Some(sent) = sent else { return };
    let size = socket(conn).map_or(0, |s| s.length_field_size());
    let mut w = EeWonWriteBuffer::new(size, true);
    w.append_bytes(&sent.message);
    // SAFETY: a new buffer adopting the bytes, one reference (the original's temporary)
    let b = unsafe { n_bytes_adopt(Box::into_raw(Box::new(w.to_byte_buffer(true)))) };
    if let Some(q) = queue(conn) {
        let o = crate::cppcore::new_socket_op(EeWonCoreSocketOp::SendMsg, null_mut(), b, std::ptr::null(), 0);
        q.queue_op(o, -1);
    }
    ref_release(b);
    if sent.add_op {
        if let Some(c) = route_conn(conn) {
            c.add_op(cpp);
        }
    }
}

// ---- the connection's hooks

unsafe extern "C" fn n_route_type(o: *mut c_void) -> c_long {
    let r = route_op(o);
    // SAFETY: the op's live Rust object
    if r.is_null() {
        0
    } else {
        unsafe { (*r).0.ty as c_long }
    }
}
unsafe extern "C" fn n_route_derived(o: *mut c_void) -> bool {
    // SAFETY: as n_route_type
    unsafe { n_route_type(o) == T_DERIVED as c_long }
}
/// `HandleReply`: the Rust op's read; the create op's leave (`CheckLeaveGroup`) as a leave request on the op's
/// connection; a read cut short sets `threw` after it.
unsafe extern "C" fn n_route_reply(
    o: *mut c_void,
    message_type: u8,
    data: *const u8,
    length: usize,
    pos: usize,
    threw: *mut bool,
) -> i32 {
    let r = route_op(o);
    if r.is_null() {
        return 0;
    }
    let pending = op(o).is_some_and(|x| x.status() == WS_PENDING);
    // SAFETY: the caller's bytes; the op's live Rust object, the borrow over before the leave's C++ calls
    let (read, conn) = unsafe { ((*r).0.reply(message_type, crate::ffi::bytes(data, length), pos, pending), (*r).1) };
    if let Some(group) = read.leave_group {
        request(conn as *mut c_void, EE_WON_RTQ_LEAVE, &[Value::num(group as i64)], null_mut());
    }
    match read.end {
        crate::routemsg::End::Status(s) => s,
        crate::routemsg::End::Cut => {
            // SAFETY: the caller's flag
            unsafe { *threw = true };
            0
        }
    }
}
unsafe extern "C" fn n_route_end(o: *mut c_void, unsolicited: bool, status: c_long) {
    if let Some(x) = op(o) {
        x.set_mode(2);
        if unsolicited {
            x.force_finish(status as _);
        } else {
            x.finish(status as _);
        }
    }
}
unsafe extern "C" fn n_route_complete(o: *mut c_void) {
    if let Some(x) = op(o) {
        x.complete();
    }
}
fn route_virtual(conn: *mut c_void, which: i32, o: *mut c_void, message_type: u8) -> *mut c_void {
    // SAFETY: the table's contract
    core().and_then(|c| c.route_virtual).map_or(null_mut(), |h| unsafe { h(conn, which, o, message_type) })
}
unsafe extern "C" fn n_route_complete_hook(conn: *mut c_void, o: *mut c_void) {
    route_virtual(conn, 0, o, 0);
}
/// `GetUnsolicitedOp`'s new op: a notification of the kind (its op type, routeop.rs's table), or the derived
/// server's (the virtual's); one reference for the caller
unsafe extern "C" fn n_route_unsolicited(conn: *mut c_void, kind: i32, message_type: u8) -> *mut c_void {
    if kind == -1 {
        return route_virtual(conn, 1, null_mut(), message_type);
    }
    let ty = notice_type(kind);
    if ty == 0 {
        return null_mut();
    }
    let o = new_route_op(conn, ty);
    // SAFETY: the new op
    unsafe { n_ref_add(o) };
    o
}
fn servconn(conn: *mut c_void) -> *const crate::servconn::EeWonServerConn {
    object_of(conn, EeWonCoreObject::ServerConn).cast()
}
unsafe extern "C" fn n_route_server_close(conn: *mut c_void) {
    let s = servconn(conn);
    if !s.is_null() {
        // SAFETY: the connection's live Rust object
        unsafe { ee_won_servconn_close_hook(s) };
    }
}
unsafe extern "C" fn n_route_server_kill(conn: *mut c_void) {
    let s = servconn(conn);
    if !s.is_null() {
        // SAFETY: as above
        unsafe { ee_won_servconn_kill_hook(s) };
    }
}

/// The routing connection's calls over the core table.
pub static NATIVE_ROUTECONN: EeWonRouteConnHooks = EeWonRouteConnHooks {
    add_ref: Some(n_ref_add),
    release: Some(n_ref_release),
    op_kill: Some(n_op_kill),
    op_completion: Some(n_op_completion),
    op_set_completion: Some(n_op_set_completion),
    op_type: Some(n_route_type),
    op_derived: Some(n_route_derived),
    handle_reply: Some(n_route_reply),
    op_end: Some(n_route_end),
    op_complete: Some(n_route_complete),
    complete_hook: Some(n_route_complete_hook),
    new_unsolicited: Some(n_route_unsolicited),
    server_close_hook: Some(n_route_server_close),
    server_kill_hook: Some(n_route_server_kill),
};

/// A new routing connection object for the C++ `conn`, on the socket's lock, its calls over the core table.
///
/// # Safety
/// `core` is the library's static table; `conn` and `lock` outlive the object.
#[no_mangle]
pub unsafe extern "C" fn ee_won_routeconn_new_core(
    conn: *mut c_void,
    core: *const crate::cppcore::EeWonCoreHooks,
    lock: *mut c_void,
) -> *mut EeWonRouteConn {
    crate::cppcore::bind(core);
    Box::into_raw(Box::new(EeWonRouteConn::new(conn, &NATIVE_ROUTECONN, lock.cast())))
}

// ---- the smart connection's chat input

/// UTF-32 (or UTF-16) units as UTF-16: a unit past U+FFFF becomes its surrogate pair (as `WideAppendUtf16LE`).
fn utf16(units: &[u32]) -> Vec<u16> {
    let mut out = Vec::with_capacity(units.len());
    for &u in units {
        if u > 0xffff {
            let c = u - 0x10000;
            out.push(0xd800 | ((c >> 10) & 0x3ff) as u16);
            out.push(0xdc00 | (c & 0x3ff) as u16);
        } else {
            out.push(u as u16);
        }
    }
    out
}

/// The request a chat input's action makes, as its helper's kind and values (none for no action).
fn action_request(a: &EeWonSmartAction, text: &[u16]) -> Option<(i32, Vec<Value>)> {
    let n = |x: i64| Value::num(x);
    let t = || Value::units(text);
    let moderate =
        |kind: i32, who: i64| (kind, vec![n(a.group as i64), n(a.on as i64), n(a.seconds as i64), t(), n(who)]);
    Some(match a.kind {
        EE_WON_SMART_ACT_CHAT => (EE_WON_RTQ_CHAT, vec![t(), n(a.flags as u16 as i64), Value::units(&[a.group])]),
        EE_WON_SMART_ACT_CLIENT_FLAGS => {
            (EE_WON_RTQ_SET_CLIENT_FLAGS, vec![n(a.flags as i64), n(a.value as i64), n(0), n(0)])
        }
        EE_WON_SMART_ACT_INVITE => {
            (EE_WON_RTQ_INVITE, vec![n(a.group as i64), n(a.client as i64), n(a.on as i64), t()])
        }
        EE_WON_SMART_ACT_MODERATOR => (EE_WON_RTQ_MODERATOR, vec![n(a.on as i64)]),
        EE_WON_SMART_ACT_MUTE => moderate(EE_WON_RTQ_MUTE, a.client as i64),
        EE_WON_SMART_ACT_BAN => moderate(EE_WON_RTQ_BAN, a.client as i64),
        EE_WON_SMART_ACT_BAN_CDKEY => moderate(EE_WON_RTQ_BAN_CDKEY_ID, a.value as i64),
        _ => return None,
    })
}

/// `SmartRoutingConnection::HandleInput(input, group)`: the model's reading of a chat line (`len` `wchar_t`s), the
/// request it makes sent on the connection `conn`; whether the line was taken. The caller holds the socket's lock.
///
/// # Safety
/// `r` is the connection's model; `conn` the live connection; `text` holds `len` `wchar_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_smart_handle_input(
    r: *mut EeWonSmartRoute,
    conn: *mut c_void,
    text: *const c_void,
    len: usize,
    group: i32,
) -> bool {
    let mut a = EeWonSmartAction::default();
    // SAFETY: the caller's contract
    let ok = unsafe { ee_won_smart_input(r, text, len, group, &mut a) };
    let units = if a.text.is_null() {
        Vec::new()
    } else {
        // SAFETY: the action's text lives in the model until its next input
        utf16(unsafe { std::slice::from_raw_parts(a.text, a.text_len) })
    };
    if let Some((kind, values)) = action_request(&a, &units) {
        request(conn, kind, &values, null_mut());
    }
    ok
}

#[cfg(test)]
#[path = "routecpp_tests.rs"]
mod tests;
