//! The op base's calls on C++ objects answered over the library's core table (net round 29): what ServerOp.cpp's
//! `RustServerOp::HostCall` answered last, its nine cases. A natively bound op without a C++ function (the game's:
//! `ee_won_hostop_bind_native` with no `HostCall`) asks these here instead of sending them:
//!
//! - the objects only the C++ can make: `new BlockingSocket(type)`, the socket op classes
//!   (`EE_WON_HOST_NEW_OP`), a plain `AsyncOp` (the light certificate op, a timer), each with the reference the Rust
//!   op owns (`Out`'s `AddRef`);
//! - the Rust ops' C++ ops: `new PeerAuthOp(addr, mAuthContext, mAuthType, socket, auth2)` with the op's time limits
//!   (`CopyMaxTimes`) and `NewHttpGet(host, path)`, made as a `RustServerOp` over the Rust op (`new_host_op`);
//! - the session's transform on a socket (`AddMsgTransform(new CryptTransform(session))`: Rust's,
//!   cppcore.rs) and an HTTP session's `AddOp`;
//! - a C++ object's Rust object: an auth context's (`EE_WON_HOST_AUTH_CORE`) and, by kind, a socket's, a queue
//!   socket's, an op's, a socket op's, a Rust op's or a server context's list (`EE_WON_HOST_SOCK_CORE`: null when
//!   the object is not of the kind's class, as the `dynamic_cast`s answered).
//!
//! The drivers' stand-ins (bound with a function) still see every call; `won_opcore_test`, `won_hostsock_test` and
//! the other drivers over ServerOp.cpp compare these answers with the ones recorded over `HostCall`.

use std::ffi::c_void;
use std::ptr::null_mut;

use crate::cppcore::{self, EeWonCoreObject, EeWonCoreSocketOp};
use crate::hostbase::{
    EE_WON_HOSTBASE_AUTH_CONTEXT, EE_WON_HOSTBASE_AUTH_TYPE, EE_WON_HOSTBASE_MAX_CONNECT, EE_WON_HOSTBASE_MAX_RECV,
    EE_WON_HOSTBASE_SOCKET,
};
use crate::hostop::*;

fn obj(o: *mut c_void) -> Option<Reply> {
    Some(Reply { obj: Obj::of(o), num: 0, addr: [0; 6], bytes: None })
}

/// A new C++ object with the reference the Rust op owns.
fn owned(o: *mut c_void) -> Option<Reply> {
    cppcore::ref_add(o);
    obj(o)
}

/// `new PeerAuthOp(addr, context, type, socket, auth2)`: the Rust op (peerop.rs) and its C++ op, its `ServerOp`
/// members set as the constructor set them (`SetAuthType(type, context)`, `SetSocket(socket)`); no reference yet
/// (null without the C++ op's maker).
pub(crate) fn new_peer_auth(addr: [u8; 6], context: Obj, auth_type: i32, socket: Obj, auth2: bool) -> *mut c_void {
    // SAFETY: six bytes
    let op = unsafe { crate::peerop::ee_won_hostop_peerauth(addr.as_ptr(), auth2) };
    let cpp = cppcore::new_host_op(op);
    if cpp.is_null() {
        return null_mut();
    }
    // SAFETY: the new C++ op owns `op` (bound natively by its constructor)
    let o = unsafe { &*op };
    o.base_set(EE_WON_HOSTBASE_AUTH_TYPE, i64::from(auth_type), Obj::none());
    o.base_set(EE_WON_HOSTBASE_AUTH_CONTEXT, 0, context);
    o.base_set(EE_WON_HOSTBASE_SOCKET, 0, socket);
    cpp
}

/// `EE_WON_HOST_NEW_OP`'s class, from the op's kind.
fn socket_op_kind(kind: i64) -> EeWonCoreSocketOp {
    match kind {
        EE_WON_HOST_OP_CONNECT => EeWonCoreSocketOp::Connect,
        EE_WON_HOST_OP_SEND_BYTES => EeWonCoreSocketOp::SendBytes,
        EE_WON_HOST_OP_SEND_BYTES_TO => EeWonCoreSocketOp::SendTo,
        EE_WON_HOST_OP_SEND_MSG => EeWonCoreSocketOp::SendMsg,
        EE_WON_HOST_OP_RECV_BYTES => EeWonCoreSocketOp::RecvBytes,
        EE_WON_HOST_OP_RECV_MSG => EeWonCoreSocketOp::RecvMsg,
        EE_WON_HOST_OP_RECV_CR_MSG => EeWonCoreSocketOp::RecvCrMsg,
        _ => EeWonCoreSocketOp::RecvFrom,
    }
}

/// A new socket op of the kind on the socket: the bytes as a new `ByteBuffer` and the address as an `IPAddr` (each
/// a temporary the op copies or holds), the length (`RecvBytesOp`) or size (`RecvBytesFromOp`, 16 bits).
fn new_socket_op(kind: i64, socket: Obj, data: &[u8], addr: [u8; 6], length: i64) -> *mut c_void {
    let k = socket_op_kind(kind);
    let bytes = match k {
        EeWonCoreSocketOp::SendBytes | EeWonCoreSocketOp::SendTo | EeWonCoreSocketOp::SendMsg => {
            // SAFETY: the bytes; a new buffer with one reference
            unsafe { cppcore::bytes_adopt(crate::buffer::ee_won_bytes_copy(data.as_ptr(), data.len() as _)) }
        }
        _ => null_mut(),
    };
    let ip = match k {
        EeWonCoreSocketOp::Connect | EeWonCoreSocketOp::SendTo => {
            let a = crate::ipobj::ee_won_ipaddr_new();
            // SAFETY: a new address; six bytes
            unsafe { crate::ipobj::ee_won_ipaddr_set_six(a, addr.as_ptr()) };
            a
        }
        _ => null_mut(),
    };
    let size = match k {
        EeWonCoreSocketOp::RecvBytes => length as u32,
        _ => u32::from(length as u16),
    };
    let op = cppcore::new_socket_op(k, socket.ptr(), bytes, ip, size);
    cppcore::ref_release(bytes);
    // SAFETY: the address made above (the op copied it)
    unsafe { crate::ipobj::ee_won_ipaddr_free(ip) };
    op
}

/// `EE_WON_HOST_SOCK_CORE`'s kind.
fn core_kind(kind: i64) -> EeWonCoreObject {
    match kind {
        EE_WON_HOST_CORE_SOCKET => EeWonCoreObject::Socket,
        EE_WON_HOST_CORE_QUEUE => EeWonCoreObject::Queue,
        EE_WON_HOST_CORE_OP => EeWonCoreObject::Op,
        EE_WON_HOST_CORE_HOSTOP => EeWonCoreObject::HostOp,
        EE_WON_HOST_CORE_CONTEXT => EeWonCoreObject::Context,
        _ => EeWonCoreObject::SocketOp,
    }
}

impl<P: Plan> Host<P> {
    /// The call answered over the core table, or `None` for the others.
    pub(crate) fn answer_cpp(&self, code: i32, o: [Obj; 3], n: [i64; 2], data: &[u8], addr: [u8; 6]) -> Option<Reply> {
        match code {
            EE_WON_HOST_NEW_SOCKET => owned(cppcore::new_socket(i32::from(n[0] != 0), true)),
            EE_WON_HOST_NEW_OP => owned(new_socket_op(n[0], o[0], data, addr, n[1])),
            EE_WON_HOST_PEER_NEW_LIGHT => owned(cppcore::new_plain_op()),
            EE_WON_HOST_NEW_PEER_AUTH => {
                let b = self.base();
                let op = new_peer_auth(addr, b.auth_context, b.auth_type, o[0], n[0] != 0);
                let h = cppcore::object_of(op, EeWonCoreObject::HostOp) as *const EeWonHostOp;
                if !h.is_null() {
                    // `CopyMaxTimes(this)`
                    for which in EE_WON_HOSTBASE_MAX_CONNECT..=EE_WON_HOSTBASE_MAX_RECV {
                        // SAFETY: the new op's own Rust op
                        unsafe { (*h).base_set(which, i64::from(b.times[(which - EE_WON_HOSTBASE_MAX_CONNECT) as usize]), Obj::none()) };
                    }
                }
                owned(op)
            }
            EE_WON_HOST_NEW_HTTP_GET => {
                let cut = (n[0].max(0) as usize).min(data.len());
                let (host, path) = data.split_at(cut);
                // SAFETY: the two parts
                let op = unsafe { crate::httpget::ee_won_httpget_open(host.as_ptr(), host.len(), path.as_ptr(), path.len()) };
                owned(cppcore::new_host_op(op))
            }
            EE_WON_HOST_ADD_CRYPT => {
                cppcore::add_crypt(o[0].ptr(), o[1].ptr());
                obj(null_mut())
            }
            EE_WON_HOST_SESSION_ADD => {
                let s = cppcore::object_of(o[0].ptr(), EeWonCoreObject::HttpSession);
                if !s.is_null() {
                    // SAFETY: the C++ session's live Rust object; the caller's op
                    unsafe { crate::httpengine::ee_won_http_session_add_op(s.cast(), o[1].ptr()) };
                }
                obj(null_mut())
            }
            EE_WON_HOST_AUTH_CORE => obj(cppcore::object_of(o[0].ptr(), EeWonCoreObject::AuthCtx)),
            EE_WON_HOST_SOCK_CORE => {
                let x = if o[0].is_none() { Obj(self.own().op) } else { o[0] };
                obj(cppcore::object_of(x.ptr(), core_kind(n[0])))
            }
            _ => None,
        }
    }
}
