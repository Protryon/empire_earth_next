//! The C++ network objects' calls for the lobby's Rust logics (R6 WONLobby round 28): what the logics' hosts did on
//! an `AsyncOp`, a socket op, a queue socket or a `ByteBuffer` (each a shim over its Rust object since the net
//! rounds) made on the Rust objects directly, the objects found through the library's core table
//! ([`crate::cppcore`]). Every pointer is the C++ object's `RefCount*`; a null object is a no-op (or 0 / false /
//! null), as are all calls before the table is bound ([`bound`]: the drivers without the C++ classes).

use std::ffi::{c_long, c_void};
use std::ptr::null_mut;

use crate::buffer::EeWonByteBuffer;
use crate::cppcore::{self, EeWonCoreObject, EeWonCoreSocketOp};
use crate::ipobj::EeWonIpAddr;
use crate::sockop::EeWonSocketOp;

/// Whether the C++ classes' table is bound (the game, a driver over the classes).
pub fn bound() -> bool {
    cppcore::core().is_some()
}
/// `o->AddRef()`, `o->Release()`.
pub fn ref_add(o: *mut c_void) {
    cppcore::ref_add(o)
}
pub fn ref_release(o: *mut c_void) {
    cppcore::ref_release(o)
}

/// `op->GetStatus()` (0 for none).
pub fn op_status(o: *mut c_void) -> i32 {
    cppcore::op(o).map_or(0, |op| op.status() as i32)
}
/// `op->IsKilled()`.
pub fn op_killed(o: *mut c_void) -> bool {
    cppcore::op(o).is_some_and(|op| op.killed())
}
/// `op->Succeeded()`: finished without error and not killed.
pub fn op_succeeded(o: *mut c_void) -> bool {
    op_status(o) == 0 && !op_killed(o)
}
/// `op->Kill(finish)`.
pub fn op_kill(o: *mut c_void, finish: bool) {
    if let Some(op) = cppcore::op(o) {
        op.kill(finish)
    }
}
/// `op->Finish(status)`.
pub fn op_finish(o: *mut c_void, status: i32) {
    if let Some(op) = cppcore::op(o) {
        op.finish(status as _)
    }
}
/// `new AsyncOp` with one reference (null before the table is bound).
pub fn new_op() -> *mut c_void {
    let o = cppcore::new_plain_op();
    cppcore::ref_add(o);
    o
}
/// A `RoutingOp`'s numeric member, its first recipient (-1 for none) and whether its data type is `inType`.
pub fn route_num(o: *mut c_void, field: i32) -> i64 {
    cppcore::route_op_of(o).map_or(0, |r| r.num(field))
}
pub fn route_first_recipient(o: *mut c_void) -> isize {
    cppcore::route_op_of(o).and_then(|r| r.ids.first().copied()).map_or(-1, |id| id as isize)
}
pub fn route_data_type_is(o: *mut c_void, ty: &[u8]) -> bool {
    cppcore::route_op_of(o).is_some_and(|r| r.data_type == ty)
}
/// `op->RunAsTimer(timeout)`.
pub fn op_run_as_timer(o: *mut c_void, timeout: u32) {
    if let Some(op) = cppcore::op(o) {
        op.run_as_timer(timeout)
    }
}

fn socket_op<'a>(o: *mut c_void) -> Option<&'a EeWonSocketOp> {
    // SAFETY: a socket op's Rust object lives as long as the op, which the caller holds
    unsafe { (cppcore::object_of(o, EeWonCoreObject::SocketOp) as *const EeWonSocketOp).as_ref() }
}
/// `static_cast<SocketOp*>(op)->GetSocket()` (borrowed).
pub fn socketop_socket(o: *mut c_void) -> *mut c_void {
    socket_op(o).map_or(null_mut(), |s| s.socket())
}
/// A socket op's message (`GetMsg`), received bytes or datagram (`GetBytes`) and accepted socket
/// (`GetAcceptedSocket`): borrowed `RefCount*`s, null for none.
pub fn socketop_message(o: *mut c_void) -> *mut c_void {
    socket_op(o).map_or(0, |s| s.message()) as *mut c_void
}
pub fn socketop_received(o: *mut c_void) -> *mut c_void {
    socket_op(o).map_or(0, |s| s.received()) as *mut c_void
}
pub fn socketop_accepted(o: *mut c_void) -> *mut c_void {
    socket_op(o).map_or(0, |s| s.accepted()) as *mut c_void
}
/// `GetAddr()`: a datagram's sender, a connect's address (a copy).
pub fn socketop_address(o: *mut c_void) -> EeWonIpAddr {
    socket_op(o).map_or_else(EeWonIpAddr::default, |s| s.address())
}
/// A `ByteBuffer`'s bytes (empty for none), borrowed while the caller holds the buffer.
#[allow(clippy::not_unsafe_ptr_arg_deref)] // the C++ object pointers pass through to the table
pub fn buffer_bytes<'a>(b: *mut c_void) -> Option<&'a [u8]> {
    if b.is_null() {
        return None;
    }
    let bb = cppcore::core().and_then(|c| c.bytes_of).map_or(std::ptr::null(), |h| {
        // SAFETY: the table's contract: a live buffer
        unsafe { h(b) }
    });
    // SAFETY: the buffer's bytes live as long as the buffer
    unsafe { bb.as_ref() }.map(EeWonByteBuffer::bytes)
}
/// `new ByteBuffer(bytes)` with one reference (the caller gives it back with [`ref_release`]).
pub fn new_bytes(bytes: &[u8]) -> *mut c_void {
    if !bound() {
        return null_mut();
    }
    // SAFETY: a new buffer the C++ takes
    unsafe { cppcore::bytes_adopt(Box::into_raw(Box::new(EeWonByteBuffer::copy(bytes)))) }
}
/// `new SocketOp(kind, socket, bytes, addr, size)` (no reference yet).
pub fn new_socket_op(
    kind: EeWonCoreSocketOp,
    socket: *mut c_void,
    bytes: *mut c_void,
    addr: Option<&EeWonIpAddr>,
    size: u32,
) -> *mut c_void {
    cppcore::new_socket_op(kind, socket, bytes, addr.map_or(std::ptr::null(), |a| a as *const EeWonIpAddr), size)
}
/// `socket->QueueOp(op, timeout)`, `socket->QueueClose(timeout)` on a queue socket.
pub fn queue_op(socket: *mut c_void, op: *mut c_void, timeout: c_long) {
    if let Some(q) = cppcore::queue(socket) {
        q.queue_op(op, timeout)
    }
}
pub fn queue_close(socket: *mut c_void, timeout: c_long) {
    if let Some(q) = cppcore::queue(socket) {
        q.queue_close(timeout)
    }
}
/// `socket->Listen(backlog)`: the status (`WS_AsyncSocket_InvalidSocket` for none).
pub fn socket_listen(socket: *mut c_void, backlog: i32) -> i32 {
    cppcore::socket(socket).map_or(crate::status::WS_ASYNC_SOCKET_INVALID_SOCKET, |s| s.listen(backlog)) as i32
}
/// `socket->IsConnected()` (false for none).
pub fn socket_connected(socket: *mut c_void) -> bool {
    cppcore::socket(socket).is_some_and(|s| s.is_connected())
}
/// `((RoutingOp*)o)->SetNum(field, value)`.
pub fn route_set_num(o: *mut c_void, field: i32, value: i64) {
    let r = cppcore::object_of(o, EeWonCoreObject::RouteOp) as *mut crate::routeop::EeWonRouteOp;
    if !r.is_null() {
        // SAFETY: the op's live Rust op
        unsafe { crate::routeop::ee_won_routeop_set_num(r, field, value) };
    }
}
/// `((RoutingOp*)o)->mConnection.get()`: the connection the op was made on (its Rust op keeps it).
pub fn route_connection(o: *mut c_void) -> *mut c_void {
    let r = cppcore::object_of(o, EeWonCoreObject::RouteOp) as *const crate::routeop::EeWonRouteOp;
    // SAFETY: the op's live Rust op
    unsafe { r.as_ref() }.map_or(null_mut(), |r| r.1 as *mut c_void)
}
/// `((ServerConnection*)conn)->SetAuth(context, type)`.
pub fn servconn_set_auth(conn: *mut c_void, context: *mut c_void, auth_type: i32) {
    let c = cppcore::object_of(conn, EeWonCoreObject::ServerConn) as *const crate::servconn::EeWonServerConn;
    if !c.is_null() {
        // SAFETY: the connection's live Rust object
        unsafe { crate::servconn::ee_won_servconn_set_auth(c, context, auth_type) };
    }
}
/// `((UDPManagerSendRecvOp*)o)->GetDestAddr()` (none for another op).
pub fn udp_request_address(o: *mut c_void) -> Option<EeWonIpAddr> {
    let r = cppcore::object_of(o, EeWonCoreObject::UdpSendRecv) as *const crate::udpops::EeWonUdpSendRecvOp;
    // SAFETY: the op's live Rust object
    unsafe { r.as_ref() }.map(|r| r.address())
}
/// `new RoutingOp(conn, type)` (no reference yet; null without the table).
pub fn new_route_op(conn: *mut c_void, ty: i32) -> *mut c_void {
    // SAFETY: the table's contract
    cppcore::core().and_then(|c| c.new_route_op).map_or(null_mut(), |h| unsafe { h(conn, ty) })
}
/// `((ServerContext*)context)->NotifyFailed(((RustServerOp*)op)->GetCurAddr())`: the request's current server to
/// the end of the list.
pub fn context_notify_failed(context: *mut c_void, op: *mut c_void) {
    let list = cppcore::object_of(context, EeWonCoreObject::Context) as *const crate::servctx::ServerList;
    let Some(h) = cppcore::host_op_of(op) else { return };
    if list.is_null() {
        return;
    }
    // SAFETY: the op's live Rust op; the new address is freed below; the context's live list
    unsafe {
        let a = crate::hostop::ee_won_hostop_addr(h, crate::reqhost::EE_WON_REQHOST_CUR_ADDR);
        crate::ipobj::ee_won_ctx_ip(list, 1, a);
        crate::ipobj::ee_won_ipaddr_free(a);
    }
}

/// `socket->Kill()`.
pub fn socket_kill(socket: *mut c_void) {
    if let Some(s) = cppcore::socket(socket) {
        s.kill()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A null object: every call a no-op, whether or not a table is bound (the hosts' NULL checks).
    #[test]
    fn null_objects_do_nothing() {
        let p = null_mut();
        assert_eq!(op_status(p), 0);
        assert!(!op_killed(p));
        assert!(op_succeeded(p));
        assert!(socketop_socket(p).is_null());
        assert!(socketop_message(p).is_null());
        assert!(socketop_received(p).is_null());
        assert!(socketop_accepted(p).is_null());
        assert_eq!(socketop_address(p), EeWonIpAddr::default());
        assert!(buffer_bytes(p).is_none());
        queue_op(p, p, -1);
        queue_close(p, 0);
        socket_kill(p);
        op_kill(p, true);
        op_run_as_timer(p, 1);
        ref_add(p);
        ref_release(p);
    }
}
