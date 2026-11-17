//! `FilePushOp` (WONInternal/FilePushOp.cpp) on the Rust op base (hostop.rs): the server's address, the local and
//! remote file names, the compress flag, the modify time, the comment, the block size, the progress completion, the
//! byte counts, the session's encryption key and the TCP socket, and every step: `RunHook` (the file, the socket,
//! peer authentication), `CallbackHook`, `GetEncryptKey`, `SendStreamFileRequest`, `SendFileChunk`, `RecvAck`,
//! `HandleAck`, `RecvFactStatusReply`, `HandleFactStatusReply` and `CleanupHook`. The op's length field size,
//! auth context and type and time limits stay the C++ `ServerOp`'s.
//!
//! Behaviour: `golden_hostop.txt`'s `F` sequences, recorded over the original's statements before the C++ class was
//! deleted. Nothing in the game makes one (no C++ user since the port).
//!
//! The original's undefined corners, defined: a status reply or an acknowledgement that is NULL (a NULL
//! dereference) reads as empty (a cut reply; an acknowledgement 0); a session key that does not encrypt sends the
//! chunk's length field alone.

use crate::blowfish::Blowfish;
use crate::hostop::{
    self, EeWonHostOp, Host, Obj, Plan, SetValue, EE_WON_HOST_OP_RECV_BYTES, EE_WON_HOST_OP_RECV_MSG,
    EE_WON_HOST_OP_SEND_BYTES, EE_WON_HOST_OP_SEND_MSG, FOREVER, ST_SUCCESS,
};
use crate::wire::{Reader, Writer};

/// `WS_FailedToOpenFile`
const ST_FAILED_TO_OPEN_FILE: i32 = 100007;
/// `WS_MessageUnpackFailure`
const ST_UNPACK: i32 = 100012;
/// `WS_AuthRequired`
const ST_AUTH_REQUIRED: i32 = 100013;

const TRACK_AUTH: i32 = 1;
const TRACK_STREAM_REQUEST: i32 = 2;
const TRACK_STATUS_REPLY: i32 = 3;
const TRACK_ACK: i32 = 4;

// Setters (`ee_won_hostop_set`):
/// `SetFile`, `SetRemoteFile`, `SetComment` (the bytes)
pub const EE_WON_FILEPUSH_FILE: i32 = 1;
pub const EE_WON_FILEPUSH_REMOTE_FILE: i32 = 2;
pub const EE_WON_FILEPUSH_COMMENT: i32 = 3;
/// `SetCompress`, `SetBlockTransferSize` (the number)
pub const EE_WON_FILEPUSH_COMPRESS: i32 = 4;
pub const EE_WON_FILEPUSH_BLOCK_SIZE: i32 = 5;
/// `SetModifyTime` (the number: high 32 bits `dwHighDateTime`, low `dwLowDateTime`)
pub const EE_WON_FILEPUSH_MODIFY_TIME: i32 = 6;
/// `SetProgressCompletion` (the object)
pub const EE_WON_FILEPUSH_PROGRESS: i32 = 7;
// Results (`ee_won_hostop_num`):
/// `GetTotalBytesSent`, `GetLastBytesSent`
pub const EE_WON_FILEPUSH_TOTAL_SENT: i32 = 10;
pub const EE_WON_FILEPUSH_LAST_SENT: i32 = 11;

/// The op's members.
pub(crate) struct FilePush {
    connect_addr: [u8; 6],
    file_name: Vec<u8>,
    remote_file_name: Vec<u8>,
    is_compress: bool,
    modify_high: u32,
    modify_low: u32,
    progress: Obj,
    total_sent: u32,
    last_sent: u32,
    key: Blowfish,
    block_size: u32,
    comment: Vec<u8>,
    socket: Obj,
}

impl FilePush {
    /// `FilePushOp(addr)`: no compression, 65,535-byte blocks. The original left the byte counts and the modify
    /// time unset until `RunHook` / `SetModifyTime`: 0 here.
    pub(crate) fn new(addr: [u8; 6]) -> FilePush {
        FilePush {
            connect_addr: addr,
            file_name: Vec::new(),
            remote_file_name: Vec::new(),
            is_compress: false,
            modify_high: 0,
            modify_low: 0,
            progress: Obj::none(),
            total_sent: 0,
            last_sent: 0,
            key: Blowfish::new(),
            block_size: 65535,
            comment: Vec::new(),
            socket: Obj::none(),
        }
    }
}

type H = Host<FilePush>;

/// `CheckError(status)`: true (after `Finish`) for an error.
fn check_error(h: &H, status: i32) -> bool {
    if status != ST_SUCCESS {
        h.finish(status);
        return true;
    }
    false
}

/// `GetEncryptKey(op)`
fn get_encrypt_key(h: &H, op: Obj) -> bool {
    match h.peer_auth_key(op) {
        None => {
            h.finish(ST_AUTH_REQUIRED);
            false
        }
        Some(key) => {
            let mut k = Blowfish::new();
            k.set_key(&key);
            h.st().key = k;
            true
        }
    }
}

/// `DoProgressCompletion`
fn progress(h: &H) {
    let p = h.st().progress;
    if !p.is_none() {
        h.complete(p);
    }
}

/// `HandleFactStatusReply(reply)`
fn handle_status_reply(h: &H, reply: Option<&[u8]>) -> bool {
    let mut r = Reader::new(reply.unwrap_or(&[]));
    let status = (|| {
        r.read_byte()?;
        r.read_short()?;
        r.read_short()?;
        r.read_short()
    })();
    match status {
        Err(_) => {
            h.finish(ST_UNPACK);
            return false;
        }
        Ok(s) => {
            if check_error(h, i32::from(s)) {
                return false;
            }
        }
    }
    if h.st().total_sent > 0 {
        progress(h);
        h.finish(ST_SUCCESS);
        return false;
    }
    true
}

/// `RecvFactStatusReply`
fn recv_status_reply(h: &H) -> bool {
    let socket = h.st().socket;
    if h.is_async() {
        // RecvMsgAsync(TRACK_STATUS_REPLY)
        let op = h.new_op(EE_WON_HOST_OP_RECV_MSG, socket, &[], [0; 6], 0);
        h.track(op, TRACK_STATUS_REPLY);
        h.queue(socket, op, FOREVER);
        h.release(op);
        return false;
    }
    let wait = h.recv_time();
    let (status, reply) = h.recv_msg(socket, wait);
    if check_error(h, status) {
        return false;
    }
    handle_status_reply(h, reply.as_deref())
}

/// `HandleAck(ack)`
fn handle_ack(h: &H, ack: u8) -> bool {
    if ack == 1 {
        progress(h);
        if h.is_async() {
            send_chunk(h);
        }
        return true;
    }
    recv_status_reply(h)
}

/// `RecvAck`
fn recv_ack(h: &H) -> bool {
    let socket = h.st().socket;
    if h.is_async() {
        let op = h.new_op(EE_WON_HOST_OP_RECV_BYTES, socket, &[], [0; 6], 1);
        h.track(op, TRACK_ACK);
        h.queue(socket, op, FOREVER);
        h.release(op);
        return false;
    }
    let (status, byte) = h.recv_bytes(socket, 1, FOREVER);
    if check_error(h, status) {
        return false;
    }
    handle_ack(h, first(byte.as_deref()))
}

fn first(b: Option<&[u8]>) -> u8 {
    b.and_then(|b| b.first().copied()).unwrap_or(0)
}

/// `SendFileChunk`
fn send_chunk(h: &H) -> bool {
    let size = h.st().block_size;
    let plain = h.file_read(size);
    let len = plain.len() as u32;
    let mut w = Writer::new(4, false);
    let socket = {
        let mut s = h.st();
        if len > 0 {
            if let Some(e) = s.key.encrypt(&plain) {
                w.append_bytes(&e);
            }
        }
        s.last_sent = len;
        s.total_sent = s.total_sent.wrapping_add(len);
        s.socket
    };
    let msg = w.to_message();
    if h.is_async() {
        let op = h.new_op(EE_WON_HOST_OP_SEND_BYTES, Obj::none(), &msg, [0; 6], 0);
        h.track(op, 0);
        h.queue(socket, op, FOREVER);
        h.release(op);
    } else {
        let wait = h.send_time();
        let status = h.send_bytes(socket, &msg, wait);
        if check_error(h, status) {
            return false;
        }
    }
    if len > 0 {
        recv_ack(h)
    } else {
        recv_status_reply(h)
    }
}

/// `SendStreamFileRequest`
fn send_request(h: &H) -> bool {
    let lfs = h.lfs();
    let (msg, socket) = {
        let s = h.st();
        let mut w = Writer::new(lfs, true);
        w.append_byte(5);
        w.append_short(5);
        w.append_short(21);
        w.append_string(&s.remote_file_name, 2);
        w.append_bool(s.is_compress);
        w.append_long(s.modify_high as i32);
        w.append_long(s.modify_low as i32);
        w.append_string(&s.comment, 2);
        (w.to_message(), s.socket)
    };
    if h.is_async() {
        // SendMsgAsync(msg, TRACK_STREAM_REQUEST)
        let op = h.new_op(EE_WON_HOST_OP_SEND_MSG, socket, &msg, [0; 6], 0);
        h.track(op, TRACK_STREAM_REQUEST);
        h.queue(socket, op, FOREVER);
        h.release(op);
        recv_status_reply(h);
        recv_ack(h);
        return false;
    }
    let wait = h.send_time();
    let status = h.send_msg(socket, &msg, wait);
    if check_error(h, status) {
        return false;
    }
    if !recv_status_reply(h) {
        return false;
    }
    recv_ack(h)
}

impl Plan for FilePush {
    fn run(h: &H) {
        let name = {
            let mut s = h.st();
            s.total_sent = 0;
            s.last_sent = 0;
            s.file_name.clone()
        };
        if !h.file_open(&name) {
            h.finish(ST_FAILED_TO_OPEN_FILE);
            return;
        }
        let socket = h.new_socket(false);
        h.replace(|s| &mut s.socket, socket);
        let lfs = h.lfs();
        h.set_lfs(socket, lfs);
        let addr = h.st().connect_addr;
        let auth = h.new_peer_auth(addr, socket, true);
        if h.is_async() {
            h.track(auth, TRACK_AUTH);
            h.run_async(auth, FOREVER);
            h.release(auth);
            return;
        }
        let wait = h.time_left();
        if !h.run_block(auth, wait) {
            let st = h.op_status(auth);
            h.finish(st);
            h.release(auth);
            return;
        }
        let ok = get_encrypt_key(h, auth);
        h.release(auth);
        if !ok || !send_request(h) {
            return;
        }
        while send_chunk(h) {}
    }

    fn callback(h: &H, id: i32, op: Obj) -> bool {
        if h.op_status(op) != ST_SUCCESS {
            let st = h.op_status(op);
            h.finish(st);
            return true;
        }
        match id {
            TRACK_AUTH => {
                if get_encrypt_key(h, op) {
                    send_request(h);
                }
                true
            }
            TRACK_STATUS_REPLY => {
                let reply = h.op_bytes(op);
                handle_status_reply(h, reply.as_deref());
                true
            }
            TRACK_ACK => {
                let byte = h.op_bytes(op);
                handle_ack(h, first(byte.as_deref()));
                true
            }
            _ => false,
        }
    }

    fn cleanup(h: &H) {
        h.base_cleanup();
        h.file_close();
    }

    fn num(&self, which: i32, _index: i64) -> i64 {
        match which {
            EE_WON_FILEPUSH_TOTAL_SENT => i64::from(self.total_sent),
            EE_WON_FILEPUSH_LAST_SENT => i64::from(self.last_sent),
            _ => 0,
        }
    }

    fn set(h: &H, which: i32, v: &SetValue) {
        match which {
            EE_WON_FILEPUSH_FILE => h.st().file_name = v.data.to_vec(),
            EE_WON_FILEPUSH_REMOTE_FILE => h.st().remote_file_name = v.data.to_vec(),
            EE_WON_FILEPUSH_COMMENT => h.st().comment = v.data.to_vec(),
            EE_WON_FILEPUSH_COMPRESS => h.st().is_compress = v.num != 0,
            EE_WON_FILEPUSH_BLOCK_SIZE => h.st().block_size = v.num as u32,
            EE_WON_FILEPUSH_MODIFY_TIME => {
                let mut s = h.st();
                s.modify_high = (v.num >> 32) as u32;
                s.modify_low = v.num as u32;
            }
            EE_WON_FILEPUSH_PROGRESS => {
                h.add_ref(v.obj);
                h.replace(|s| &mut s.progress, v.obj);
            }
            _ => {}
        }
    }

    fn describe(&self) -> String {
        format!(
            "addr {} file {} remote {} comp {} mod {:x}:{:x} prog {} total {} last {} key {} bs {} comment {} sock {}",
            hostop::hex(&self.connect_addr),
            hostop::hex(&self.file_name),
            hostop::hex(&self.remote_file_name),
            u8::from(self.is_compress),
            self.modify_high,
            self.modify_low,
            self.progress.0,
            self.total_sent,
            self.last_sent,
            hostop::hex(self.key.key()),
            self.block_size,
            hostop::hex(&self.comment),
            self.socket.0
        )
    }

    fn take_objects(&mut self) -> Vec<Obj> {
        [std::mem::take(&mut self.progress), std::mem::take(&mut self.socket)]
            .into_iter()
            .filter(|o| !o.is_none())
            .collect()
    }
}

/// `new FilePushOp(addr)` (the address's six bytes): the Rust op a `RustServerOp` runs.
///
/// # Safety
/// `addr` points to six bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_filepush_new(addr: *const u8) -> *mut EeWonHostOp {
    let mut a = [0u8; 6];
    // SAFETY: six bytes (the contract)
    a.copy_from_slice(unsafe { std::slice::from_raw_parts(addr, 6) });
    EeWonHostOp::boxed(FilePush::new(a))
}
