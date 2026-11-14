//! The op base's socket and op calls answered in Rust (net round 23). ServerOp.cpp's `HostCall` answered them on
//! the C++ objects (`BlockingSocket`, the socket ops, `AsyncOp`), which are shims over Rust objects since the lobby
//! lane's rounds 13-16: the sockets' `EeWonAsyncSocket` and `EeWonQueueSocket`, the ops' `EeWonAsyncOp` and the
//! socket ops' `EeWonSocketOp`. The C++ side now answers one call for all of them, [`EE_WON_HOST_SOCK_CORE`] (an
//! object's Rust object of one kind, by `dynamic_cast`, or null), and these calls run on the Rust objects:
//!
//! - the sockets: bind to the local host, listen, the local port, the length field (read and set), the connected
//!   flag, the last socket error, close and kill;
//! - the blocking calls (`BlockingSocket`'s connect, sends and receives): the socket op is still made by the C++
//!   side (`EE_WON_HOST_NEW_OP`: a C++ class callers and the queue hold), then run blocking, read and released here,
//!   as `BlockingSocket`'s methods did;
//! - the socket ops: their bytes (by the op's class: a datagram's or a receive's bytes, a message, a line), a
//!   datagram's sender, the socket, whether the op is a `RecvMsgOp`, a receive's file, skip and progress settings;
//! - the ops: status, the killed flag, kill, the runs (`Run`, `RunAsync`, `RunBlock`); the queue socket's `QueueOp`.
//!
//! The drivers' stand-ins (bound with `ee_won_hostop_bind`) still see every call; `won_hostsock_test` compares these
//! answers with the original's over real sockets (golden `golden_hostsock.txt`).

use std::ffi::{c_long, c_void};

use crate::hostop::*;
use crate::ipaddr::Addr;
use crate::opobj::{self, EeWonAsyncOp};
use crate::qsock::{self, EeWonQueueSocket};
use crate::sockobj::EeWonAsyncSocket;
use crate::sockop::EeWonSocketOp;

/// One call to the C++ side (the op's `send`).
pub(crate) type SendFn<'a> = &'a dyn Fn(i32, [Obj; 3], [i64; 2], &[u8], [u8; 6]) -> Reply;

fn num(n: i64) -> Option<Reply> {
    Some(Reply { obj: Obj::none(), num: n, addr: [0; 6], bytes: None })
}

/// An object's Rust object of a kind (`EE_WON_HOST_CORE_*`; obj NULL: the op itself), or null.
fn core(send: SendFn, which: i64, o: Obj) -> *mut c_void {
    send(EE_WON_HOST_SOCK_CORE, [o, Obj::none(), Obj::none()], [which, 0], &[], [0; 6]).obj.ptr()
}

fn socket<'a>(send: SendFn, o: Obj) -> Option<&'a EeWonAsyncSocket> {
    let p = core(send, EE_WON_HOST_CORE_SOCKET, o) as *const EeWonAsyncSocket;
    // SAFETY: the C++ socket's own object, alive while the caller holds the socket (as the C++ statement needed)
    (!p.is_null()).then(|| unsafe { &*p })
}

fn op(send: SendFn, o: Obj) -> *const EeWonAsyncOp {
    core(send, EE_WON_HOST_CORE_OP, o) as *const EeWonAsyncOp
}

fn socket_op<'a>(send: SendFn, o: Obj) -> Option<&'a EeWonSocketOp> {
    let p = core(send, EE_WON_HOST_CORE_SOCKET_OP, o) as *const EeWonSocketOp;
    // SAFETY: as `socket`
    (!p.is_null()).then(|| unsafe { &*p })
}

/// `Run(mode, timeout)` on an op's object (0 for none: the C++ dereferenced it).
fn run(o: *const EeWonAsyncOp, mode: i32, timeout: u32) -> i64 {
    if o.is_null() {
        return 0;
    }
    // SAFETY: a live op's object (the caller holds the op)
    i64::from(unsafe { opobj::ee_won_asyncop_run(o, mode, timeout) })
}

fn status(o: *const EeWonAsyncOp) -> i64 {
    if o.is_null() {
        return 0;
    }
    // SAFETY: as `run`
    i64::from(unsafe { opobj::ee_won_asyncop_status(o) })
}

/// What a socket op holds to give back: its bytes by its class (`RecvBytesFromOp::GetBytes`, `RecvMsgOp::GetMsg`,
/// `RecvCRMsgOp::GetMsg`, `RecvBytesOp::GetBytes`), and a datagram's sender.
fn op_bytes(s: Option<&EeWonSocketOp>) -> Option<Vec<u8>> {
    s.and_then(|s| s.result_bytes())
}

/// `BlockingSocket`'s calls: a new socket op of `kind` on the socket (the C++ side's), run blocking, its status, its
/// bytes and sender when it succeeded, released.
fn blocking(
    send: SendFn,
    kind: i64,
    sock: Obj,
    length: i64,
    timeout: i64,
    data: &[u8],
    addr: [u8; 6],
) -> Option<Reply> {
    let made = send(EE_WON_HOST_NEW_OP, [sock, Obj::none(), Obj::none()], [kind, length], data, addr).obj;
    let o = op(send, made);
    let ok = run(o, 2, timeout as u32) == 0;
    let mut r = Reply { obj: Obj::none(), num: status(o), addr: [0; 6], bytes: None };
    if ok {
        let s = socket_op(send, made);
        r.bytes = op_bytes(s);
        if kind == EE_WON_HOST_OP_RECV_BYTES_FROM {
            r.addr = s.and_then(|s| s.datagram_from()).unwrap_or([0; 6]);
        }
    }
    if !made.is_none() {
        send(EE_WON_HOST_RELEASE, [made, Obj::none(), Obj::none()], [0, 0], &[], [0; 6]);
    }
    Some(r)
}

/// The call answered here, or `None` for the C++ side's.
pub(crate) fn answer(code: i32, o: [Obj; 3], n: [i64; 2], data: &[u8], addr: [u8; 6], send: SendFn) -> Option<Reply> {
    match code {
        // the sockets
        EE_WON_HOST_SOCKET_CONNECTED => num(socket(send, o[0]).map_or(0, |s| i64::from(s.is_connected()))),
        EE_WON_HOST_SOCKET_STATUS => num(socket(send, o[0]).map_or(0, |s| s.status())),
        EE_WON_HOST_SOCKET_LFS => num(socket(send, o[0]).map_or(0, |s| i64::from(s.length_field_size()))),
        EE_WON_HOST_SOCKET_SET_LFS => {
            if let Some(s) = socket(send, o[0]) {
                s.set_length_field_size(n[0] as u8);
            }
            num(0)
        }
        EE_WON_HOST_SOCKET_PORT => num(socket(send, o[0]).map_or(0, |s| i64::from(s.local_port()))),
        EE_WON_HOST_SOCKET_LISTEN => num(socket(send, o[0]).map_or(0, |s| s.listen(1000))),
        // `Fn2B0D90(port, reuse)`: `Bind(IPAddr(GetLocalHost(), port), reuse)`
        EE_WON_HOST_SOCKET_BIND => num(socket(send, o[0]).map_or(0, |s| {
            let a = Addr::from_host(crate::ipobj::local_host() as u32, n[0] as u16);
            s.bind(a, n[1] != 0)
        })),
        EE_WON_HOST_CLOSE => {
            if let Some(s) = socket(send, o[0]) {
                s.close();
            }
            num(0)
        }
        EE_WON_HOST_SOCKET_KILL => {
            if let Some(s) = socket(send, o[0]) {
                s.kill();
            }
            num(0)
        }
        // the blocking calls (`BlockingSocket`'s)
        EE_WON_HOST_CONNECT => blocking(send, EE_WON_HOST_OP_CONNECT, o[0], 0, n[0], &[], addr),
        EE_WON_HOST_SEND_BYTES => blocking(send, EE_WON_HOST_OP_SEND_BYTES, o[0], 0, n[0], data, [0; 6]),
        EE_WON_HOST_SEND_MSG => blocking(send, EE_WON_HOST_OP_SEND_MSG, o[0], 0, n[0], data, [0; 6]),
        EE_WON_HOST_SEND_BYTES_TO => blocking(send, EE_WON_HOST_OP_SEND_BYTES_TO, o[0], 0, n[0], data, addr),
        EE_WON_HOST_RECV_BYTES => {
            blocking(send, EE_WON_HOST_OP_RECV_BYTES, o[0], n[0] as u32 as i64, n[1], &[], [0; 6])
        }
        EE_WON_HOST_RECV_MSG => blocking(send, EE_WON_HOST_OP_RECV_MSG, o[0], 0, n[0], &[], [0; 6]),
        EE_WON_HOST_RECV_CR => blocking(send, EE_WON_HOST_OP_RECV_CR_MSG, o[0], 0, n[0], &[], [0; 6]),
        EE_WON_HOST_RECV_BYTES_FROM => {
            blocking(send, EE_WON_HOST_OP_RECV_BYTES_FROM, o[0], n[0] as u16 as i64, n[1], &[], [0; 6])
        }
        // the socket ops
        EE_WON_HOST_OP_BYTES => {
            Some(Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: op_bytes(socket_op(send, o[0])) })
        }
        EE_WON_HOST_OP_ADDR => {
            let a = socket_op(send, o[0]).and_then(|s| s.datagram_from()).unwrap_or([0; 6]);
            Some(Reply { obj: Obj::none(), num: 0, addr: a, bytes: None })
        }
        // the socket, borrowed
        EE_WON_HOST_OP_SOCKET => Some(Reply {
            obj: socket_op(send, o[0]).map_or(Obj::none(), |s| Obj::of(s.socket())),
            num: 0,
            addr: [0; 6],
            bytes: None,
        }),
        EE_WON_HOST_OP_IS_RECV_MSG => num(socket_op(send, o[0]).map_or(0, |s| i64::from(s.is_recv_msg()))),
        EE_WON_HOST_RECV_PROGRESS => {
            if let Some(s) = socket_op(send, o[0]) {
                s.set_progress(o[1].0, n[0] as i32);
            }
            num(0)
        }
        EE_WON_HOST_RECV_FILE => {
            if let Some(s) = socket_op(send, o[0]) {
                s.set_path(data);
                s.set_flag(0, n[0] != 0);
            }
            num(0)
        }
        EE_WON_HOST_RECV_BINARY => {
            if let Some(s) = socket_op(send, o[0]) {
                s.set_flag(1, n[0] != 0);
            }
            num(0)
        }
        EE_WON_HOST_RECV_SKIP => {
            if let Some(s) = socket_op(send, o[0]) {
                s.set_flag(2, true);
            }
            num(0)
        }
        EE_WON_HOST_RECV_MODIFY_TIME => {
            if let Some(s) = socket_op(send, o[0]) {
                s.set_modify_time(n[0]);
            }
            num(0)
        }
        // the ops (obj NULL: the op itself, for the killed flag and kill)
        EE_WON_HOST_OP_STATUS => num(status(op(send, o[0]))),
        EE_WON_HOST_OP_KILLED => {
            let p = op(send, o[0]);
            // SAFETY: as `run`
            num(if p.is_null() { 0 } else { i64::from(unsafe { opobj::ee_won_asyncop_killed(p) }) })
        }
        EE_WON_HOST_KILL => {
            let p = op(send, o[0]);
            if !p.is_null() {
                // SAFETY: as `run`
                unsafe { opobj::ee_won_asyncop_kill(p, n[0] != 0) };
            }
            num(0)
        }
        EE_WON_HOST_RUN => num(run(op(send, o[0]), n[0] as i32, n[1] as u32)),
        EE_WON_HOST_RUN_ASYNC => {
            run(op(send, o[0]), 1, n[0] as u32);
            num(0)
        }
        EE_WON_HOST_RUN_BLOCK => num(i64::from(run(op(send, o[0]), 2, n[0] as u32) == 0)),
        // `QueueOp(op, timeout)` on the queue socket
        EE_WON_HOST_QUEUE => {
            let q = core(send, EE_WON_HOST_CORE_QUEUE, o[0]) as *const EeWonQueueSocket;
            if !q.is_null() {
                // SAFETY: the C++ queue socket's own object (the caller holds the socket); `o[1]` a live `SocketOp*`
                unsafe { qsock::ee_won_queuesocket_queue_op(q, o[1].ptr(), n[0] as c_long) };
            }
            num(0)
        }
        _ => None,
    }
}

#[cfg(test)]
#[path = "hostsock_tests.rs"]
mod tests;
