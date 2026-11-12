//! WONSocket's socket ops as Rust objects (round 15): `SendBytesOp`, `SendMsgOp`, `SendBytesToOp`, `RecvBytesOp`,
//! `RecvMsgOp`, `RecvBytesFromOp`, `ConnectOp` and `AcceptOp` keep their state in their op's [`EeWonSocketOp`] (an
//! [`OpKind`]): the bytes to send and the position, the message and its transformed copy, the destination, the
//! receive's write buffer, length, count, file, path and flags, the progress completion, the message's length stage,
//! the datagram buffer, the sender, the connect address and the accepted socket; and every step of SocketOps.cpp and
//! SocketOperationDispatch.cpp (the starts, the continues, `StartRecv`, `ContinueRecv`, `FinishRecv`, `CloseFile`,
//! `StartRecvBody`, `FinishRecvMsg`, `RecvBytesFromOp::RunHook`'s buffer) runs here. The C++ classes are shims.
//!
//! What stays C++ comes back through the op hooks ([`EeWonSocketOpBaseHooks`]): the references of the C++ objects
//! the ops hold (`ByteBuffer`s, the progress completion, the accepted `AsyncSocket`, each a `RefCount*` with a
//! reference held), a `ByteBuffer`'s bytes and a new one adopting Rust's, the socket's message transforms, the
//! target file (`PlatFOpen`, `fwrite`, `fclose`, the modify time), the progress chunk (`RecvChunkOp`, run in the op's
//! mode) and `AsyncSocket::Accept` (the virtual `Duplicate`). No borrow of an op's state lives across a hook. The
//! behaviour is `golden_sockobj.txt`, recorded over the C++ classes.

use std::ffi::c_void;
use std::ptr::null_mut;

use crate::buffer::{EeWonByteBuffer, EeWonWriteBuffer};
use crate::ipaddr::Addr;
use crate::ipobj::{self, EeWonIpAddr};
use crate::netstats::{self, Counter};
use crate::ops::{self, LengthStep, OpTiming};
use crate::socket::{WAIT_READ, WAIT_WRITE};
use crate::sockop::{EeWonSocketOp, EeWonSocketOpBaseHooks};
use crate::status::*;

/// `SendBytesOp`'s state (and `SendMsgOp`'s).
#[derive(Default)]
pub struct SendState {
    /// `mBuffer`: the bytes sent (a `ByteBuffer`, a reference held; 0 for none)
    data: usize,
    /// `mReadBuffer`'s position in them
    pos: u32,
    /// `SendMsgOp::mMessage` (a reference held) and whether the op is one
    message: usize,
    is_msg: bool,
}

/// `SendBytesToOp`'s state.
#[derive(Default)]
pub struct SendToState {
    data: usize,
    to: EeWonIpAddr,
}

/// `RecvBytesOp`'s state (and `RecvMsgOp`'s).
pub struct RecvState {
    write: EeWonWriteBuffer,
    /// `mReceivedData` (a `ByteBuffer`, a reference held)
    received: usize,
    length: u32,
    bytes_read: u32,
    skip: bool,
    /// `mFile` (a `FILE*`)
    file: usize,
    path: Vec<u8>,
    append: bool,
    own_file: bool,
    binary: bool,
    modify_time: i64,
    /// `mProgressCompletion` (a reference held), `mProgressInterval`, `mBytesSinceProgress`
    progress: usize,
    interval: i32,
    since_progress: u32,
    /// `RecvMsgOp`: whether the op is one, `mMessage` (a reference held), `mState` (1: the prefix, 2: the body)
    is_msg: bool,
    message: usize,
    state: i32,
}

/// `RecvBytesFromOp`'s state.
#[derive(Default)]
pub struct RecvFromState {
    max: u16,
    /// `mRawBuffer`: allocated by `RunHook`, handed to the datagram on success
    raw: Option<Vec<u8>>,
    received: usize,
    from: EeWonIpAddr,
}

/// An op's own state.
#[derive(Default)]
pub enum OpKind {
    /// a socket op without state of its own here (`CloseOp`, `RecvCRMsgOp`, the lobby's and the tests' ops)
    #[default]
    Base,
    Send(SendState),
    SendTo(SendToState),
    Recv(Box<RecvState>),
    RecvFrom(RecvFromState),
    Connect(EeWonIpAddr),
    /// `AcceptOp::mAcceptedSocket` (an `AsyncSocket`, a reference held)
    Accept(usize),
}

/// The C++ object references a state holds (released with it).
fn refs(kind: &OpKind) -> Vec<usize> {
    match kind {
        OpKind::Base | OpKind::Connect(_) => vec![],
        OpKind::Send(s) => vec![s.data, s.message],
        OpKind::SendTo(s) => vec![s.data],
        OpKind::Recv(r) => vec![r.received, r.progress, r.message],
        OpKind::RecvFrom(r) => vec![r.received],
        OpKind::Accept(a) => vec![*a],
    }
}

impl EeWonSocketOp {
    #[allow(clippy::mut_from_ref)]
    fn kind(&self) -> &mut OpKind {
        // SAFETY: the op's one runner at a time (its caller or the socket thread, the C++ contract); no borrow lives
        // across a hook
        unsafe { &mut *self.kind_cell().get() }
    }

    fn h(&self) -> &'static EeWonSocketOpBaseHooks {
        self.base_hooks()
    }

    /// A reference to a C++ object (`RefCount*`, 0 for none).
    fn hold(&self, p: usize) -> usize {
        if p != 0 {
            if let Some(h) = self.h().ref_add {
                // SAFETY: the hooks' contract: a live `RefCount*`
                unsafe { h(p as *mut c_void) };
            }
        }
        p
    }

    fn drop_ref(&self, p: usize) {
        if p != 0 {
            if let Some(h) = self.h().ref_release {
                // SAFETY: a reference this op held
                unsafe { h(p as *mut c_void) };
            }
        }
    }

    /// The bytes of a C++ `ByteBuffer` this op holds (none for 0).
    fn bytes_of(&self, p: usize) -> &[u8] {
        if p == 0 {
            return &[];
        }
        let b = self.h().bytes_of.map_or(std::ptr::null(), |h| {
            // SAFETY: the hooks' contract: a live `ByteBuffer*`
            unsafe { h(p as *mut c_void) }
        });
        if b.is_null() {
            &[]
        } else {
            // SAFETY: the buffer lives while this op holds it; its bytes do not change
            unsafe { &*b }.bytes()
        }
    }

    /// A new C++ `ByteBuffer` adopting `bytes`, a reference held.
    fn adopt(&self, bytes: EeWonByteBuffer) -> usize {
        let raw = Box::into_raw(Box::new(bytes));
        match self.h().bytes_adopt {
            // SAFETY: the hooks' contract: the new buffer takes `raw`, a reference taken
            Some(h) => unsafe { h(raw) as usize },
            None => {
                // SAFETY: not handed over
                drop(unsafe { Box::from_raw(raw) });
                0
            }
        }
    }

    /// The socket's transform chain on a message held here (`TransformSendMsg` / `TransformRecvMsg`): the slot gets
    /// the transformed message (its reference) and the status.
    fn transform(&self, slot: &mut usize, send: bool) -> Status {
        let mut p = *slot as *mut c_void;
        let s = self.h().transform.map_or(WS_SUCCESS, |h| {
            // SAFETY: the hooks' contract: the live socket and a message slot holding a reference
            unsafe { h(self.socket(), &mut p, send) as Status }
        });
        *slot = p as usize;
        s
    }

    // ---- the inits (the C++ constructors)

    /// `SendBytesOp(data)` / `SendMsgOp(message)`.
    pub fn init_send(&self, data: usize, message: usize, is_msg: bool) {
        let s = SendState { data: self.hold(data), pos: 0, message: self.hold(message), is_msg };
        self.set_kind(OpKind::Send(s));
    }

    /// `SendBytesToOp(data, address)`.
    pub fn init_send_to(&self, data: usize, to: &EeWonIpAddr) {
        self.set_kind(OpKind::SendTo(SendToState { data: self.hold(data), to: to.clone() }));
    }

    /// `RecvBytesOp(length)` / `RecvMsgOp`: memory, binary, nothing skipped, no modify time.
    pub fn init_recv(&self, length: u32, is_msg: bool) {
        self.set_kind(OpKind::Recv(Box::new(RecvState {
            write: EeWonWriteBuffer::new(0, true),
            received: 0,
            length,
            bytes_read: 0,
            skip: false,
            file: 0,
            path: Vec::new(),
            append: false,
            own_file: false,
            binary: true,
            modify_time: 0,
            progress: 0,
            interval: 0,
            since_progress: 0,
            is_msg,
            message: 0,
            state: 0,
        })));
    }

    /// `RecvBytesFromOp(maxLength)`.
    pub fn init_recv_from(&self, max: u16) {
        self.set_kind(OpKind::RecvFrom(RecvFromState { max, ..RecvFromState::default() }));
    }

    /// `ConnectOp(address)`.
    pub fn init_connect(&self, addr: &EeWonIpAddr) {
        self.set_kind(OpKind::Connect(addr.clone()));
    }

    /// `AcceptOp`.
    pub fn init_accept(&self) {
        self.set_kind(OpKind::Accept(0));
    }

    fn set_kind(&self, kind: OpKind) {
        let old = std::mem::replace(self.kind(), kind);
        self.release_kind(old);
    }

    /// Drops a state's references (the C++ members' destructors).
    pub(crate) fn release_kind(&self, old: OpKind) {
        for p in refs(&old) {
            self.drop_ref(p);
        }
    }

    // ---- the steps

    /// The virtual `Start` of the op's class.
    pub fn op_start(&self, timing: OpTiming) -> Status {
        match self.kind() {
            OpKind::Send(s) if s.is_msg => self.send_msg_start(timing),
            OpKind::Send(s) => {
                s.pos = 0;
                self.op_continue(timing)
            }
            OpKind::Recv(r) if r.is_msg => self.recv_msg_start(timing),
            OpKind::Recv(_) => self.start_recv(timing),
            OpKind::Connect(a) => {
                let a = a.clone();
                let Some(sock) = self.socket_object() else { return WS_ASYNC_SOCKET_INVALID_SOCKET };
                sock.connect_to(&a, ipobj::local_host() as u32)
            }
            _ => self.op_continue(timing),
        }
    }

    /// The virtual `Continue` of the op's class.
    pub fn op_continue(&self, timing: OpTiming) -> Status {
        let Some(sock) = self.socket_object() else { return WS_ASYNC_SOCKET_INVALID_SOCKET };
        let left = || timing.time_left(ops::tick());
        match self.kind() {
            OpKind::Base => WS_SUCCESS,
            OpKind::Send(s) => {
                let (data, mut pos, is_msg) = (s.data, s.pos, s.is_msg);
                let st = ops::send_all(&sock.locked(), self.bytes_of(data), &mut pos, timing);
                if let OpKind::Send(s) = self.kind() {
                    s.pos = pos;
                }
                if is_msg && st == WS_SUCCESS {
                    netstats::add(Counter::MessagesSent, 1);
                }
                st
            }
            OpKind::SendTo(s) => {
                let (data, to) = (s.data, s.to.addr());
                sock.wait(WAIT_WRITE, left());
                if sock.is_connected() {
                    let mut n = 0;
                    sock.send(self.bytes_of(data), &mut n)
                } else {
                    sock.send_to(self.bytes_of(data), to)
                }
            }
            OpKind::Recv(r) => {
                let is_msg = r.is_msg;
                let s = self.continue_recv(timing);
                if !is_msg || s != WS_SUCCESS {
                    return s;
                }
                let state = match self.kind() {
                    OpKind::Recv(r) => r.state,
                    _ => 2,
                };
                if state == 1 {
                    self.start_recv_body(timing)
                } else {
                    self.finish_recv_msg()
                }
            }
            OpKind::RecvFrom(_) => self.recv_from_continue(timing),
            OpKind::Connect(_) => sock.wait_for_connect(left()),
            OpKind::Accept(_) => {
                sock.wait(WAIT_READ, left());
                let mut out = null_mut();
                let s = self.h().accept.map_or(WS_SUCCESS, |h| {
                    // SAFETY: the hooks' contract: the live socket; `out` gets a new socket with a reference
                    unsafe { h(self.socket(), &mut out) as Status }
                });
                if s == WS_SUCCESS {
                    if let OpKind::Accept(a) = self.kind() {
                        let old = std::mem::replace(a, out as usize);
                        self.drop_ref(old);
                    } else {
                        self.drop_ref(out as usize);
                    }
                }
                s
            }
        }
    }

    /// `SendMsgOp::Start`: the message through the socket's send transforms, then sent from its start.
    fn send_msg_start(&self, timing: OpTiming) -> Status {
        let OpKind::Send(s) = self.kind() else { return WS_SUCCESS };
        let (old, message) = (s.data, s.message);
        let mut data = self.hold(message);
        s.data = 0;
        self.drop_ref(old);
        let st = self.transform(&mut data, true);
        if let OpKind::Send(s) = self.kind() {
            s.data = data;
            s.pos = 0;
        }
        if st != WS_SUCCESS {
            return st;
        }
        self.op_continue(timing)
    }

    #[allow(clippy::mut_from_ref)]
    fn recv(&self) -> Option<&mut RecvState> {
        match self.kind() {
            OpKind::Recv(r) => Some(r),
            _ => None,
        }
    }

    /// `RecvBytesOp::StartRecv`: the counts cleared; the target file opened when a path is set
    /// (`WS_FailedToOpenFile`), else memory reserved for the length; then the receive.
    fn start_recv(&self, timing: OpTiming) -> Status {
        let Some(r) = self.recv() else { return WS_SUCCESS };
        r.bytes_read = 0;
        r.since_progress = 0;
        r.own_file = false;
        if r.file == 0 && !r.path.is_empty() {
            let (path, binary, append) = (r.path.clone(), r.binary, r.append);
            let f = self.h().file_open.map_or(null_mut(), |h| {
                // SAFETY: the hooks' contract: the path's bytes
                unsafe { h(path.as_ptr(), path.len(), binary, append) }
            });
            let Some(r) = self.recv() else { return WS_SUCCESS };
            if f.is_null() {
                return WS_FAILED_TO_OPEN_FILE;
            }
            r.file = f as usize;
            r.own_file = true;
        }
        let Some(r) = self.recv() else { return WS_SUCCESS };
        if r.length > 0 && r.file == 0 && !r.skip {
            r.write.reserve(r.length as _);
        }
        self.continue_recv(timing)
    }

    /// `RecvBytesOp::ContinueRecv`: into the write buffer (a length, no file, not skipping) or 1024-byte chunks into
    /// the file or memory, a progress chunk each time more than the interval arrived, until the length (or the
    /// peer's close for none); `WS_TimedOut` when the time is up, else the socket's error.
    fn continue_recv(&self, timing: OpTiming) -> Status {
        let Some(sock) = self.socket_object() else { return WS_ASYNC_SOCKET_INVALID_SOCKET };
        loop {
            sock.wait(WAIT_READ, timing.time_left(ops::tick()));
            let Some(r) = self.recv() else { return WS_SUCCESS };
            let mut read = 0i32;
            let status;
            if r.length > 0 && r.file == 0 && !r.skip {
                let (cap, len) = (r.write.capacity() as usize, r.write.length() as usize);
                let p = r.write.data_ptr();
                let room: &mut [u8] = if p.is_null() || cap <= len {
                    &mut []
                } else {
                    // SAFETY: the writer's allocation holds `cap` bytes from `p`
                    unsafe { std::slice::from_raw_parts_mut(p.add(len), cap - len) }
                };
                status = sock.recv(room, &mut read);
                if read > 0 {
                    r.write.skip_bytes(read as _);
                }
            } else {
                let mut chunk = [0u8; 1024];
                let n = ops::chunk_length(r.length, r.bytes_read).max(0) as usize;
                status = sock.recv(&mut chunk[..n], &mut read);
                if read > 0 && !r.skip {
                    let got = &chunk[..read as usize];
                    if r.file != 0 {
                        let f = r.file;
                        let wrote = self.h().file_write.map_or(0, |h| {
                            // SAFETY: the hooks' contract: the open file and the bytes
                            unsafe { h(f as *mut c_void, got.as_ptr(), got.len()) }
                        });
                        if wrote != got.len() {
                            return self.finish_recv(WS_FAILED_TO_WRITE_TO_FILE);
                        }
                    } else {
                        r.write.append_bytes(got);
                    }
                }
            }
            let Some(r) = self.recv() else { return WS_SUCCESS };
            let read = read.max(0) as u32;
            r.bytes_read = r.bytes_read.wrapping_add(read);
            if read > 0 && r.progress != 0 {
                r.since_progress = r.since_progress.wrapping_add(read);
                // (the original compared the unsigned count with the signed interval as unsigned)
                let interval = r.interval as u32;
                if r.since_progress > interval {
                    let c = r.progress;
                    if let Some(h) = self.h().progress {
                        // SAFETY: the hooks' contract: the live op and its completion (no borrow held)
                        unsafe { h(self.cpp_op(), c as *mut c_void) };
                    }
                    let Some(r) = self.recv() else { return WS_SUCCESS };
                    // (an interval of 0 divided by zero in the original)
                    r.since_progress = r.since_progress.checked_rem(interval).unwrap_or(0);
                }
            }
            let Some(r) = self.recv() else { return WS_SUCCESS };
            if r.length == 0 {
                if status == WS_ASYNC_SOCKET_SHUTDOWN {
                    return self.finish_recv(WS_SUCCESS);
                }
            } else if r.bytes_read == r.length {
                return self.finish_recv(WS_SUCCESS);
            }
            if status == WS_TIMED_OUT {
                if timing.time_left(ops::tick()) == 0 {
                    return WS_TIMED_OUT;
                }
            } else if status != WS_SUCCESS {
                return status;
            }
        }
    }

    /// `RecvBytesOp::FinishRecv`: the received bytes handed over (memory), or the file closed.
    fn finish_recv(&self, status: Status) -> Status {
        let Some(r) = self.recv() else { return status };
        if r.file == 0 {
            let bytes = r.write.to_byte_buffer(true);
            let new = self.adopt(bytes);
            let Some(r) = self.recv() else { return status };
            let old = std::mem::replace(&mut r.received, new);
            self.drop_ref(old);
            return status;
        }
        self.close_file(status)
    }

    /// `RecvBytesOp::CloseFile`: an own file closed (`WS_FailedToWriteToFile` when that fails), the modify time set.
    pub fn close_file(&self, status: Status) -> Status {
        let Some(r) = self.recv() else { return status };
        if r.file == 0 {
            return status;
        }
        let (f, own, time, path) = (r.file, r.own_file, r.modify_time, r.path.clone());
        r.file = 0;
        let mut status = status;
        if own {
            let e = self.h().file_close.map_or(0, |h| {
                // SAFETY: the hooks' contract: an open file, closed once
                unsafe { h(f as *mut c_void) }
            });
            if e != 0 {
                status = WS_FAILED_TO_WRITE_TO_FILE;
            }
        }
        if time != 0 {
            if let Some(h) = self.h().file_set_time {
                // SAFETY: the hooks' contract: the path's bytes
                unsafe { h(path.as_ptr(), path.len(), time) };
            }
        }
        status
    }

    /// `RecvMsgOp::Start`: the socket's 1, 2 or 4-byte length prefix.
    fn recv_msg_start(&self, timing: OpTiming) -> Status {
        let field = self.socket_object().map_or(0, |s| s.length_field_size());
        let Some(r) = self.recv() else { return WS_SUCCESS };
        r.state = 1;
        r.length = field as u32;
        if !ops::valid_length_field(r.length) {
            return WS_RECV_MSG_INVALID_LENGTH_FIELD_SIZE;
        }
        let s = self.start_recv(timing);
        if s == WS_SUCCESS {
            self.start_recv_body(timing)
        } else {
            s
        }
    }

    /// `RecvMsgOp::StartRecvBody`: the prefix read gives the body's length (a 2-byte 0xffff: a 4-byte prefix
    /// follows); the body received.
    fn start_recv_body(&self, timing: OpTiming) -> Status {
        let Some(r) = self.recv() else { return WS_SUCCESS };
        r.state = 2;
        let received = r.received;
        match ops::length_step(self.bytes_of(received)) {
            LengthStep::Widen => {
                if let Some(r) = self.recv() {
                    r.length = 4;
                    r.state = 1;
                }
                let s = self.start_recv(timing);
                if s != WS_SUCCESS {
                    return s;
                }
                self.start_recv_body(timing)
            }
            LengthStep::Body(n) => {
                if let Some(r) = self.recv() {
                    r.state = 2;
                    r.length = n;
                }
                let s = self.start_recv(timing);
                if s == WS_SUCCESS {
                    self.finish_recv_msg()
                } else {
                    s
                }
            }
            LengthStep::Invalid => WS_RECV_MSG_INVALID_MESSAGE_LENGTH,
        }
    }

    /// `RecvMsgOp::FinishRecvMsg`: counted, then the message through the socket's receive transforms.
    fn finish_recv_msg(&self) -> Status {
        netstats::add(Counter::MessagesReceived, 1);
        let Some(r) = self.recv() else { return WS_SUCCESS };
        let (old, received) = (r.message, r.received);
        let mut message = self.hold(received);
        r.message = 0;
        self.drop_ref(old);
        let s = self.transform(&mut message, false);
        match self.recv() {
            Some(r) => r.message = message,
            None => self.drop_ref(message),
        }
        s
    }

    /// `RecvBytesFromOp::RunHook`'s part before the base's: the buffer allocated when there is none, the last
    /// datagram dropped.
    pub fn recv_from_prepare(&self) {
        if let OpKind::RecvFrom(r) = self.kind() {
            if r.raw.is_none() {
                r.raw = Some(vec![0; r.max as usize]);
            }
            let old = std::mem::replace(&mut r.received, 0);
            self.drop_ref(old);
        }
    }

    /// `RecvBytesFromOp::Continue`: one datagram and its sender (the destination when connected); on success the
    /// buffer becomes the datagram.
    fn recv_from_continue(&self, timing: OpTiming) -> Status {
        let Some(sock) = self.socket_object() else { return WS_ASYNC_SOCKET_INVALID_SOCKET };
        sock.wait(WAIT_READ, timing.time_left(ops::tick()));
        let OpKind::RecvFrom(r) = self.kind() else { return WS_SUCCESS };
        let max = r.max as usize;
        let raw = r.raw.get_or_insert_with(|| vec![0; max]);
        let mut n = 0;
        let s;
        if sock.is_connected() {
            r.from = sock.dest_addr();
            s = sock.recv(raw, &mut n);
        } else {
            let mut peer = None;
            s = sock.recv_from(raw, &mut peer, &mut n);
            if let Some(p) = peer {
                let mut a = EeWonIpAddr::default();
                a.set_sockaddr(sockaddr(p));
                r.from = a;
            } else {
                n = 0;
            }
        }
        if s == WS_SUCCESS {
            let raw = r.raw.take().unwrap_or_default();
            let bytes = EeWonByteBuffer::copy(&raw[..(n.max(0) as usize).min(raw.len())]);
            let new = self.adopt(bytes);
            if let OpKind::RecvFrom(r) = self.kind() {
                let old = std::mem::replace(&mut r.received, new);
                self.drop_ref(old);
            }
        }
        s
    }

    // ---- the accessors

    /// The received bytes (`RecvBytesOp::GetBytes`, `RecvBytesFromOp::GetBytes`): a borrowed `ByteBuffer*`.
    pub fn received(&self) -> usize {
        match self.kind() {
            OpKind::Recv(r) => r.received,
            OpKind::RecvFrom(r) => r.received,
            _ => 0,
        }
    }
    /// `RecvMsgOp::GetMsg`.
    pub fn message(&self) -> usize {
        self.recv().map_or(0, |r| r.message)
    }
    /// `ConnectOp::GetAddr`, `RecvBytesFromOp::GetAddr`.
    pub fn address(&self) -> EeWonIpAddr {
        match self.kind() {
            OpKind::Connect(a) => a.clone(),
            OpKind::RecvFrom(r) => r.from.clone(),
            _ => EeWonIpAddr::default(),
        }
    }
    /// `AcceptOp::GetAcceptedSocket`: a borrowed `AsyncSocket*`.
    pub fn accepted(&self) -> usize {
        match self.kind() {
            OpKind::Accept(a) => *a,
            _ => 0,
        }
    }
    /// `mLength` (a receive's), `mMaxLength` (a datagram's).
    pub fn length(&self) -> u32 {
        match self.kind() {
            OpKind::Recv(r) => r.length,
            OpKind::RecvFrom(r) => r.max as u32,
            _ => 0,
        }
    }
    pub fn bytes_read(&self) -> u32 {
        self.recv().map_or(0, |r| r.bytes_read)
    }
    pub fn path(&self) -> Vec<u8> {
        self.recv().map_or_else(Vec::new, |r| r.path.clone())
    }
    /// `SetProgressCompletion`: the completion's reference taken, the old one's dropped.
    pub fn set_progress(&self, completion: usize, interval: i32) {
        let new = self.hold(completion);
        match self.recv() {
            Some(r) => {
                let old = std::mem::replace(&mut r.progress, new);
                r.interval = interval;
                self.drop_ref(old);
            }
            None => self.drop_ref(new),
        }
    }
    /// The file setters: `SetFilePath`, `SetAppendFile`, `SetBinaryFile`, `SetFileModifyTime`, `SetSkipBytes`.
    pub fn set_path(&self, path: &[u8]) {
        if let Some(r) = self.recv() {
            r.path = path.to_vec();
        }
    }
    pub fn set_flag(&self, which: i32, on: bool) {
        if let Some(r) = self.recv() {
            match which {
                FLAG_APPEND => r.append = on,
                FLAG_BINARY => r.binary = on,
                _ => r.skip = on,
            }
        }
    }
    pub fn set_modify_time(&self, time: i64) {
        if let Some(r) = self.recv() {
            r.modify_time = time;
        }
    }
}

const FLAG_APPEND: i32 = 0;
const FLAG_BINARY: i32 = 1;

fn sockaddr(a: Addr) -> ee_plat_sys::constants::PlatSockAddrIn {
    ee_plat_sys::constants::PlatSockAddrIn {
        mFamily: ee_plat_sys::constants::PLAT_AF_INET,
        mPort: a.port,
        mAddress: a.address,
        mZero: [0; 8],
    }
}

// ---- the C ABI (`o` is a live op object from `ee_won_socketop_new`; the object pointers are `RefCount*`s)

macro_rules! op {
    ($o:expr) => {
        // SAFETY: the callers pass a live op object
        unsafe { &*$o }
    };
}

/// `SendBytesOp(data)` (`message` null) / `SendMsgOp(message)` (`is_msg`).
///
/// # Safety
/// `o` is a live op object (so for every call below); `data` and `message` are null or live `ByteBuffer`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_init_send(
    o: *const EeWonSocketOp,
    data: *mut c_void,
    message: *mut c_void,
    is_msg: bool,
) {
    op!(o).init_send(data as usize, message as usize, is_msg)
}
/// `SendBytesToOp(data, address)`.
///
/// # Safety
/// As `ee_won_socketop_init_send`; `to` is a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_init_send_to(
    o: *const EeWonSocketOp,
    data: *mut c_void,
    to: *const EeWonIpAddr,
) {
    // SAFETY: the caller's value
    op!(o).init_send_to(data as usize, unsafe { &*to })
}
/// `RecvBytesOp(length)` / `RecvMsgOp` (`is_msg`, length 0).
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_init_recv(o: *const EeWonSocketOp, length: u32, is_msg: bool) {
    op!(o).init_recv(length, is_msg)
}
/// `RecvBytesFromOp(maxLength)`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_init_recv_from(o: *const EeWonSocketOp, max: u16) {
    op!(o).init_recv_from(max)
}
/// `ConnectOp(address)`.
///
/// # Safety
/// As `ee_won_socketop_init_send`; `addr` is a live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_init_connect(o: *const EeWonSocketOp, addr: *const EeWonIpAddr) {
    // SAFETY: the caller's value
    op!(o).init_connect(unsafe { &*addr })
}
/// `AcceptOp`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_init_accept(o: *const EeWonSocketOp) {
    op!(o).init_accept()
}
/// The op class's `Start`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_start(o: *const EeWonSocketOp, timing: OpTiming) -> std::ffi::c_long {
    op!(o).op_start(timing) as std::ffi::c_long
}
/// The op class's `Continue`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_continue(o: *const EeWonSocketOp, timing: OpTiming) -> std::ffi::c_long {
    op!(o).op_continue(timing) as std::ffi::c_long
}
/// `RecvBytesFromOp::RunHook`'s buffer and reset (the base's run follows).
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_recv_from_prepare(o: *const EeWonSocketOp) {
    op!(o).recv_from_prepare()
}
/// `RecvBytesOp::CloseFile(status)` (its `CleanupHook`'s).
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_close_file(
    o: *const EeWonSocketOp,
    status: std::ffi::c_long,
) -> std::ffi::c_long {
    op!(o).close_file(status as Status) as std::ffi::c_long
}
/// The received bytes: a borrowed `ByteBuffer*` (null for none).
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_received(o: *const EeWonSocketOp) -> *mut c_void {
    op!(o).received() as *mut c_void
}
/// `RecvMsgOp::GetMsg`: a borrowed `ByteBuffer*`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_message(o: *const EeWonSocketOp) -> *mut c_void {
    op!(o).message() as *mut c_void
}
/// `ConnectOp::GetAddr` / `RecvBytesFromOp::GetAddr`: a new value (the C++ `IPAddr` adopts it).
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_address(o: *const EeWonSocketOp) -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(op!(o).address()))
}
/// `AcceptOp::GetAcceptedSocket`: a borrowed `AsyncSocket*`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_accepted(o: *const EeWonSocketOp) -> *mut c_void {
    op!(o).accepted() as *mut c_void
}
/// A receive's length / a datagram op's maximum.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_length(o: *const EeWonSocketOp) -> u32 {
    op!(o).length()
}
/// `RecvBytesOp::GetBytesRead`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_bytes_read(o: *const EeWonSocketOp) -> u32 {
    op!(o).bytes_read()
}
/// `RecvBytesOp::GetFilePath`: the bytes to `sink`.
///
/// # Safety
/// As `ee_won_socketop_init_send`; the sink is called during the call only.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_path(
    o: *const EeWonSocketOp,
    sink: crate::ffi::EeWonByteSink,
    context: *mut c_void,
) {
    let p = op!(o).path();
    // SAFETY: the sink's contract
    unsafe { sink(context, p.as_ptr(), p.len()) }
}
/// `SetFilePath`.
///
/// # Safety
/// As `ee_won_socketop_init_send`; `path` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_path(o: *const EeWonSocketOp, path: *const u8, length: usize) {
    let p = if path.is_null() || length == 0 {
        &[][..]
    } else {
        // SAFETY: the caller's bytes
        unsafe { std::slice::from_raw_parts(path, length) }
    };
    op!(o).set_path(p)
}
/// `SetAppendFile` (0), `SetBinaryFile` (1), `SetSkipBytes` (2).
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_flag(o: *const EeWonSocketOp, which: i32, on: bool) {
    op!(o).set_flag(which, on)
}
/// `SetFileModifyTime`.
///
/// # Safety
/// As `ee_won_socketop_init_send`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_modify_time(o: *const EeWonSocketOp, time: i64) {
    op!(o).set_modify_time(time)
}
/// `SetProgressCompletion(completion, interval)`.
///
/// # Safety
/// As `ee_won_socketop_init_send`; `completion` is null or a live completion.
#[no_mangle]
pub unsafe extern "C" fn ee_won_socketop_set_progress(o: *const EeWonSocketOp, completion: *mut c_void, interval: i32) {
    op!(o).set_progress(completion as usize, interval)
}

#[cfg(test)]
#[path = "sockops_tests.rs"]
mod tests;
