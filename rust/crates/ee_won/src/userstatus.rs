//! `RoutingGetUserStatusOp`'s members (net round 19): the friend status map (`mFriendStatusMap`, a
//! `std::map<unsigned long, unsigned char>`: the queried WON user ids in order, each with the status a server
//! reported, 0 until then), the datagram its `RunHook` sends (routemsg.rs's user status request) and the reply its
//! `CheckResponse` reads (routemsg.rs's user status reply: each status assigned in the map's order, what a reply cut
//! short assigned kept). The C++ class stays the `UDPManagerSendRecvOp` (the lobby lane's object) that sends and
//! waits; RoutingLogic reads the map through copies (`GetFriendStatusMap`, `SetFriendStatusMap`). WONRoutingMsg.h,
//! which the op used for both, is test glue now (`tests/won/won_routemsg_glue.h`).

use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::ffi::{self, EeWonByteSink};
use crate::routemsg::{self, EeWonRouteState, End, Request, EE_WON_RT_REQ_USER_STATUS, EE_WON_RT_USER_STATUS};

/// The op's map (ids as the C++ `unsigned long`s they are: 64-bit on LP64 systems, sent as their low 32 bits).
#[derive(Default)]
pub struct EeWonUserStatus {
    map: BTreeMap<u64, u8>,
}

/// One entry, to the C++ copy.
pub type EeWonUserStatusSink = unsafe extern "C" fn(context: *mut c_void, id: u64, status: u8);

impl EeWonUserStatus {
    /// `AddWONUserId`: the id with status 0, an id already there kept as it is (`std::map::insert`).
    pub fn add(&mut self, id: u64) {
        self.map.entry(id).or_insert(0);
    }

    /// The request datagram (`RouteUserStatusRequest`): every id in order.
    pub fn request(&self) -> Vec<u8> {
        let ids: Vec<u32> = self.map.keys().map(|&k| k as u32).collect();
        routemsg::request(&Request {
            kind: EE_WON_RT_REQ_USER_STATUS,
            derived: false,
            user_ids: ids,
            ..Default::default()
        })
    }

    /// `CheckResponse` over a message read from `pos`: the statuses assigned in the map's order; the status and
    /// the reader's position after.
    pub fn reply(&mut self, data: &[u8], pos: usize) -> (i32, usize) {
        let st = EeWonRouteState { user_count: self.map.len() as u32, ..Default::default() };
        let r = routemsg::reply(EE_WON_RT_USER_STATUS, 0, data, pos, &st);
        for (v, e) in self.map.values_mut().zip(r.events.iter()) {
            *v = e.num as u8;
        }
        let status = match r.end {
            End::Status(s) => s,
            // routemsg.rs turns this read's cut into its status
            End::Cut => routemsg::EE_WON_RT_UNPACK_FAILURE,
        };
        (status, r.pos)
    }
}

/// A new, empty map.
#[no_mangle]
pub extern "C" fn ee_won_userstatus_new() -> *mut EeWonUserStatus {
    Box::into_raw(Box::default())
}

/// Frees it.
///
/// # Safety
/// `s` is NULL or from [`ee_won_userstatus_new`], not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_userstatus_free(s: *mut EeWonUserStatus) {
    if !s.is_null() {
        // SAFETY: the constructor's box
        drop(unsafe { Box::from_raw(s) });
    }
}

/// `AddWONUserId`.
///
/// # Safety
/// `s` is a live map (so for every `ee_won_userstatus_` call taking one).
#[no_mangle]
pub unsafe extern "C" fn ee_won_userstatus_add(s: *mut EeWonUserStatus, id: u64) {
    // SAFETY: the caller's live map
    unsafe { &mut *s }.add(id);
}

/// Empties the map (the start of an assignment of a whole map).
///
/// # Safety
/// As [`ee_won_userstatus_add`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_userstatus_clear(s: *mut EeWonUserStatus) {
    // SAFETY: as above
    unsafe { &mut *s }.map.clear();
}

/// `map[id] = status`.
///
/// # Safety
/// As [`ee_won_userstatus_add`].
#[no_mangle]
pub unsafe extern "C" fn ee_won_userstatus_put(s: *mut EeWonUserStatus, id: u64, status: u8) {
    // SAFETY: as above
    unsafe { &mut *s }.map.insert(id, status);
}

/// Every entry in order to `sink`.
///
/// # Safety
/// As [`ee_won_userstatus_add`]; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_userstatus_each(
    s: *const EeWonUserStatus,
    sink: EeWonUserStatusSink,
    context: *mut c_void,
) {
    // SAFETY: as above
    let entries: Vec<(u64, u8)> = unsafe { &*s }.map.iter().map(|(&k, &v)| (k, v)).collect();
    for (k, v) in entries {
        // SAFETY: the sink's contract
        unsafe { sink(context, k, v) };
    }
}

/// The request datagram to `sink`.
///
/// # Safety
/// As [`ee_won_userstatus_add`]; `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_userstatus_request(
    s: *const EeWonUserStatus,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    // SAFETY: as above
    let b = unsafe { &*s }.request();
    // SAFETY: the sink's contract
    unsafe { ffi::give(sink, context, &b) };
}

/// `CheckResponse`: the message (`len` bytes) read from `pos`; returns the status, `out_pos` the position after.
///
/// # Safety
/// As [`ee_won_userstatus_add`]; `data` holds `len` bytes; `out_pos` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_userstatus_reply(
    s: *mut EeWonUserStatus,
    data: *const u8,
    len: usize,
    pos: usize,
    out_pos: *mut usize,
) -> i32 {
    // SAFETY: as above
    let (status, p) = unsafe { &mut *s }.reply(unsafe { ffi::bytes(data, len) }, pos);
    // SAFETY: the caller's out value
    unsafe { *out_pos = p };
    status
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::routemsg::{EE_WON_RT_INVALID_MESSAGE, EE_WON_RT_SUCCESS, EE_WON_RT_UNPACK_FAILURE};

    fn of(ids: &[u64]) -> EeWonUserStatus {
        let mut s = EeWonUserStatus::default();
        for &i in ids {
            s.add(i);
        }
        s
    }

    #[test]
    fn ids_are_kept_once_in_order_and_sent_as_their_low_32_bits() {
        let s = of(&[5, 2, 900_000, 2]);
        assert_eq!(s.map.keys().copied().collect::<Vec<_>>(), vec![2, 5, 900_000]);
        let r = s.request();
        assert_eq!(&r[..4], &[3, 7, 72, 3]);
        assert_eq!(r.len(), 4 + 12);
        assert_eq!(&r[4..8], &2i32.to_le_bytes());
        let big = of(&[0x1_0000_0001]);
        assert_eq!(&big.request()[4..8], &1i32.to_le_bytes());
    }

    #[test]
    fn a_reply_assigns_in_order_and_a_cut_one_keeps_what_it_read() {
        let mut s = of(&[5, 2, 900_000]);
        assert_eq!(s.reply(&[3, 7, 73, 3, 1, 0, 4], 0), (EE_WON_RT_SUCCESS, 7));
        assert_eq!(s.map.values().copied().collect::<Vec<_>>(), vec![1, 0, 4]);
        let mut s = of(&[1, 2, 3]);
        assert_eq!(s.reply(&[3, 7, 73, 2, 1, 1], 0).0, EE_WON_RT_INVALID_MESSAGE);
        assert_eq!(s.map.values().copied().collect::<Vec<_>>(), vec![0, 0, 0]);
        assert_eq!(s.reply(&[3, 7, 73, 3, 7], 0).0, EE_WON_RT_UNPACK_FAILURE);
        assert_eq!(s.map.values().copied().collect::<Vec<_>>(), vec![7, 0, 0]);
    }
}
