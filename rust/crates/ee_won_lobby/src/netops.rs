//! The lobby hosts' ops on the network objects, answered on their Rust objects in the game (R6 WONLobby round 28):
//! what LanLogicHost.cpp's cases did on an op, a socket op, a queue socket or a `ByteBuffer` (each the C++ shim of a
//! Rust object: ee_won_net cppnet.rs) is made here, so the C++ cases are gone. Answered only while the C++ classes'
//! table is bound (the game, from WONAPICore's creation on); a driver's scripted host answers them otherwise.

use std::ffi::c_void;
use std::ptr::null_mut;

use ee_won_net::cppcore::EeWonCoreSocketOp;
use ee_won_net::cppnet as net;
use ee_won_net::ipobj::EeWonIpAddr;

use crate::lobbyhost::EeWonLobbyOp as Op;

/// `LanMessageOf(buffer, bytes, length)`: the buffer, or a new one of the bytes (held until the scope's end), or none.
struct Message {
    buffer: *mut c_void,
    own: bool,
}
impl Message {
    fn of(buffer: *mut c_void, bytes: isize, length: isize) -> Message {
        if !buffer.is_null() {
            return Message { buffer, own: false };
        }
        if bytes == 0 {
            return Message { buffer: null_mut(), own: false };
        }
        // SAFETY: the host's contract: the bytes live for the call
        let b = unsafe { std::slice::from_raw_parts(bytes as *const u8, length as usize) };
        Message { buffer: net::new_bytes(b), own: true }
    }
}
impl Drop for Message {
    fn drop(&mut self) {
        if self.own {
            net::ref_release(self.buffer);
        }
    }
}

/// Appends a buffer's bytes to a take op's sink (`ee_won_lobby_sink`).
fn sink(to: *mut c_void, buffer: *mut c_void) {
    if let Some(bytes) = net::buffer_bytes(buffer) {
        if !to.is_null() && !bytes.is_empty() {
            // SAFETY: a take op's sink is the caller's `Vec<u8>`
            unsafe { (*(to as *mut Vec<u8>)).extend_from_slice(bytes) };
        }
    }
}

/// The op's answer, or none for an op the host keeps (or an unbound table).
pub fn lobby_op(op: Op, a: *mut c_void, b: *mut c_void, c: isize, d: isize) -> Option<isize> {
    if !net::bound() {
        return None;
    }
    let r = match op {
        Op::RefHold => {
            net::ref_add(a);
            0
        }
        Op::RefRelease | Op::ClientRelease => {
            net::ref_release(a);
            0
        }
        Op::ClientHold => {
            net::ref_add(a);
            a as isize
        }
        Op::LanOpSocket => net::socketop_socket(a) as isize,
        Op::LanSocketClose => {
            net::queue_close(a, c as _);
            0
        }
        Op::LanSocketSend => {
            let m = Message::of(a, c, d);
            net::queue_op(b, net::new_socket_op(EeWonCoreSocketOp::SendMsg, null_mut(), m.buffer, None, 0), -1);
            0
        }
        Op::LanSendTo => {
            let m = Message::of(null_mut(), c, d);
            // SAFETY: the host's contract: `b` is a live address
            let to = unsafe { (b as *const EeWonIpAddr).as_ref() };
            net::queue_op(a, net::new_socket_op(EeWonCoreSocketOp::SendTo, null_mut(), m.buffer, to, 0), -1);
            0
        }
        Op::LanRecvOk => net::op_succeeded(a) as isize,
        Op::LanRecvMsgTake => {
            let m = net::socketop_message(a);
            sink(b, m);
            m as isize
        }
        Op::LanDiscoveryTake => {
            sink(b, net::socketop_received(a));
            0
        }
        Op::LanAcceptedSocket => match net::op_succeeded(a) {
            true => net::socketop_accepted(a) as isize,
            false => 0,
        },
        Op::LanOpOnSocket => (net::socketop_socket(a) == b && net::op_succeeded(a)) as isize,
        Op::LanRecvFromAddr => {
            if !b.is_null() {
                // SAFETY: the host's contract: `b` is a live address
                unsafe { *(b as *mut EeWonIpAddr) = net::socketop_address(a) };
            }
            0
        }
        Op::LanSocketKill => {
            net::socket_kill(a);
            0
        }
        Op::LanTimerKill => {
            net::op_kill(a, true);
            0
        }
        Op::LanRunTimer => {
            net::op_run_as_timer(a, c as u32);
            0
        }
        _ => return None,
    };
    Some(r)
}

/// The saved game transfer's host ops on the network objects (EELobbyGameHost.cpp's), or none for the host's.
pub fn xfer_op(op: crate::eexfer::EeWonEeXferOp, a: isize) -> Option<isize> {
    use crate::eexfer::EeWonEeXferOp as X;
    if !net::bound() {
        return None;
    }
    let a = a as *mut c_void;
    let r = match op {
        X::Succeeded => net::op_succeeded(a) as isize,
        X::OpSocket => net::socketop_socket(a) as isize,
        X::AcceptedSocket => net::socketop_accepted(a) as isize,
        X::AddRef => {
            net::ref_add(a);
            0
        }
        X::Release => {
            net::ref_release(a);
            0
        }
        X::Kill => {
            net::socket_kill(a);
            0
        }
        X::Listen => net::socket_listen(a, 1000) as isize,
        _ => return None,
    };
    Some(r)
}

/// RoutingLogic's host ops on the network objects, the clock and the time (RoutingLogic.cpp's), or none for the
/// host's.
pub fn route_op(op: crate::routelogic::EeWonRouteLogicOp, a: isize, b: isize) -> Option<isize> {
    use crate::routelogic::EeWonRouteLogicOp as R;
    if !net::bound() {
        return None;
    }
    let p = a as *mut c_void;
    let r = match op {
        R::AddRef => {
            net::ref_add(p);
            0
        }
        R::Release => {
            net::ref_release(p);
            0
        }
        R::OpStatus => net::op_status(p) as isize,
        R::OpKilled => net::op_killed(p) as isize,
        R::OpKill => {
            net::op_kill(p, true);
            0
        }
        R::OpFinish => {
            net::op_finish(p, 0);
            0
        }
        R::AsyncOpNew => net::new_op() as isize,
        R::OpSocket => net::socketop_socket(p) as isize,
        R::RNum => net::route_num(p, b as i32) as isize,
        R::RRecipient => net::route_first_recipient(p),
        // `RO(a)->DataType() == mGameDataType` (the constructor's "G")
        R::RIsGameData => net::route_data_type_is(p, b"G") as isize,
        // `time(NULL)`, `WONTickCount()`
        R::Now => {
            std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map_or(0, |d| d.as_secs() as isize)
        }
        R::Tick => ee_won_net::ops::tick() as isize,
        _ => return None,
    };
    Some(r)
}

// ---- the drivers' entries (tests/won/won_netops_test.cpp: the deleted C++ cases against these): the answer, and
// whether the op is one of these

fn out(r: Option<isize>, handled: *mut bool) -> isize {
    if !handled.is_null() {
        // SAFETY: the caller's flag
        unsafe { *handled = r.is_some() };
    }
    r.unwrap_or(0)
}
/// [`lobby_op`].
///
/// # Safety
/// The objects are live C++ objects of the op's classes (or null); `handled` is null or writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_netops_lobby(
    op: crate::lobbyhost::EeWonLobbyOp,
    a: *mut c_void,
    b: *mut c_void,
    c: isize,
    d: isize,
    handled: *mut bool,
) -> isize {
    out(lobby_op(op, a, b, c, d), handled)
}
/// [`xfer_op`].
///
/// # Safety
/// As `ee_won_netops_lobby`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_netops_xfer(op: crate::eexfer::EeWonEeXferOp, a: isize, handled: *mut bool) -> isize {
    out(xfer_op(op, a), handled)
}
/// [`route_op`].
///
/// # Safety
/// As `ee_won_netops_lobby`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_netops_route(
    op: crate::routelogic::EeWonRouteLogicOp,
    a: isize,
    b: isize,
    handled: *mut bool,
) -> isize {
    out(route_op(op, a, b), handled)
}

/// A take op's sink for a driver (the `Vec<u8>` the logics pass): a new one, its bytes (copied to `out` when it
/// holds `capacity`; the length either way), freed.
#[no_mangle]
pub extern "C" fn ee_won_netops_sink_new() -> *mut c_void {
    Box::into_raw(Box::new(Vec::<u8>::new())) as *mut c_void
}
/// # Safety
/// `sink` came from `ee_won_netops_sink_new`; `out` is null or holds `capacity` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_netops_sink_bytes(sink: *const c_void, out: *mut u8, capacity: usize) -> usize {
    // SAFETY: the caller's contract
    let v = unsafe { &*(sink as *const Vec<u8>) };
    if !out.is_null() && capacity >= v.len() {
        // SAFETY: as above
        unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), out, v.len()) };
    }
    v.len()
}
/// # Safety
/// `sink` came from `ee_won_netops_sink_new` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_netops_sink_free(sink: *mut c_void) {
    // SAFETY: the caller's contract
    drop(unsafe { Box::from_raw(sink as *mut Vec<u8>) });
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Without the C++ classes' table (cargo, the drivers' stand-ins) every op is the host's.
    #[test]
    fn unbound_ops_are_the_hosts() {
        if net::bound() {
            return;
        }
        for op in [Op::RefHold, Op::LanSocketSend, Op::LanRecvOk, Op::LanRunTimer, Op::LanRecvFromAddr] {
            assert_eq!(lobby_op(op, 0x52 as *mut c_void, null_mut(), 0, 0), None);
        }
        use crate::routelogic::EeWonRouteLogicOp as R;
        for op in [R::AddRef, R::OpStatus, R::RNum, R::AsyncOpNew, R::Now] {
            assert_eq!(route_op(op, 0x52, 0), None);
        }
        use crate::eexfer::EeWonEeXferOp as X;
        for op in [X::Succeeded, X::OpSocket, X::AddRef, X::Kill, X::Listen] {
            assert_eq!(xfer_op(op, 0x52), None);
        }
    }

    /// The sink appends a buffer's bytes; none, an empty one or no sink appends nothing.
    #[test]
    fn a_message_without_bytes_or_buffer_is_none() {
        let m = Message::of(null_mut(), 0, 5);
        assert!(m.buffer.is_null() && !m.own);
        let mut v: Vec<u8> = vec![1];
        sink(&mut v as *mut Vec<u8> as *mut c_void, null_mut());
        assert_eq!(v, [1]);
    }
}
