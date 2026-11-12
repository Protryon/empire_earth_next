//! `FTPGetOp` (WONMisc/FTPGetOp.cpp) on the Rust op base (hostop.rs): the host, the remote path and its directory
//! and file, the local path, the user name and password, the binary and resume flags, the local file's size, the
//! progress completion, the request state, the FTP status, the passive data address, the content and the request
//! and data sockets, and every step: `RunHook` (the resume's size, the sockets, the connect, the blocking replies),
//! `DoConnect`, `RecvLineAsync`, `SendRequest`, `HandleReply`, `DoDataConnection`, `RecvContent`, `CallbackHook`
//! and `CleanupHook`. The URL, the commands and the reply's step are ftp.rs's.
//!
//! Behaviour: `golden_hostop.txt`'s `P` sequences, recorded over the original's statements before the C++ class was
//! deleted. Nothing in the game makes one (no C++ user since the port; the WON patch servers are gone).
//!
//! The original's undefined corners, defined: the members its constructors left unset (the FTP status, the request
//! state, the local file's size, the progress chunk size) start at 0; a reply op with no line (a NULL dereference)
//! reads as an empty line.

use crate::ftp::{
    self, Command, EE_WON_FTP_DATA, EE_WON_FTP_FINISH, EE_WON_FTP_MORE, EE_WON_FTP_RECV, EE_WON_FTP_SEND,
};
use crate::hostop::{
    self, EeWonHostOp, Host, Obj, Plan, SetValue, EE_WON_HOST_OP_CONNECT, EE_WON_HOST_OP_RECV_BYTES,
    EE_WON_HOST_OP_RECV_CR_MSG, EE_WON_HOST_OP_SEND_BYTES, FOREVER, ST_SUCCESS,
};
use crate::ipaddr::Addr;

const TRACK_REQUEST_CONNECT: i32 = 1;
const TRACK_REQUEST: i32 = 2;
const TRACK_DATA_CONNECT: i32 = 3;
const TRACK_RECV_CONTENT: i32 = 4;

// Setters (`ee_won_hostop_set`):
/// `SetLocalPath`, `SetRemotePath`, `SetUserName`, `SetPassword` (the bytes)
pub const EE_WON_FTPGET_LOCAL_PATH: i32 = 1;
pub const EE_WON_FTPGET_REMOTE_PATH: i32 = 2;
pub const EE_WON_FTPGET_USER: i32 = 3;
pub const EE_WON_FTPGET_PASSWORD: i32 = 4;
/// `SetIsBinary`, `SetDoResume` (the number)
pub const EE_WON_FTPGET_BINARY: i32 = 5;
pub const EE_WON_FTPGET_RESUME: i32 = 6;
/// `SetRecvChunkCompletion(completion, size)` (the object, the number)
pub const EE_WON_FTPGET_CHUNK: i32 = 7;
// Results:
/// `GetFTPStatus()` (`ee_won_hostop_num`)
pub const EE_WON_FTPGET_STATUS: i32 = 10;
/// `GetContent()` (`ee_won_hostop_bytes`; none: NULL)
pub const EE_WON_FTPGET_CONTENT: i32 = 11;
/// `GetLocalPath()`, `GetRemotePath()` (`ee_won_hostop_bytes`)
pub const EE_WON_FTPGET_LOCAL: i32 = 12;
pub const EE_WON_FTPGET_REMOTE: i32 = 13;

/// The op's members.
pub(crate) struct FtpGet {
    request_socket: Obj,
    data_socket: Obj,
    host: Vec<u8>,
    remote_path: Vec<u8>,
    local_path: Vec<u8>,
    user: Vec<u8>,
    password: Vec<u8>,
    actual_path: Vec<u8>,
    actual_file: Vec<u8>,
    content: Option<Vec<u8>>,
    ftp_status: i32,
    chunk_completion: Obj,
    chunk_size: u32,
    data_addr: [u8; 6],
    is_binary: bool,
    do_resume: bool,
    local_file_size: u32,
    state: i32,
}

impl FtpGet {
    /// `Init`: binary, no resume, anonymous.
    fn init() -> FtpGet {
        FtpGet {
            request_socket: Obj::none(),
            data_socket: Obj::none(),
            host: Vec::new(),
            remote_path: Vec::new(),
            local_path: Vec::new(),
            user: b"anonymous".to_vec(),
            password: b"anonymous".to_vec(),
            actual_path: Vec::new(),
            actual_file: Vec::new(),
            content: None,
            ftp_status: 0,
            chunk_completion: Obj::none(),
            chunk_size: 0,
            data_addr: [0; 6],
            is_binary: true,
            do_resume: false,
            local_file_size: 0,
            state: 0,
        }
    }

    /// `FTPGetOp(host, remotePath)`
    pub(crate) fn new(host: &[u8], remote_path: &[u8]) -> FtpGet {
        let mut f = FtpGet::init();
        f.host = host.to_vec();
        f.set_remote_path(remote_path);
        f
    }

    /// `FTPGetOp(url, doExtractUserAndPassword)`
    pub(crate) fn from_url(url: &[u8], extract: bool) -> FtpGet {
        let mut f = FtpGet::init();
        let p = ftp::url(url, extract);
        if let Some((a, b)) = p.user {
            f.user = url[a..b].to_vec();
        }
        if let Some((a, b)) = p.password {
            f.password = url[a..b].to_vec();
        }
        f.host = url[p.host.0..p.host.1].to_vec();
        if let Some((a, b)) = p.path {
            f.set_remote_path(&url[a..b]);
        }
        f
    }

    /// `SetRemotePath`
    fn set_remote_path(&mut self, path: &[u8]) {
        self.remote_path = path.to_vec();
        let (d, f) = ftp::remote_path(path);
        self.actual_path = d.to_vec();
        self.actual_file = f.to_vec();
    }
}

type H = Host<FtpGet>;

/// `CheckStatus(status)`
fn check_status(h: &H, status: i32) -> bool {
    if status == ST_SUCCESS {
        return true;
    }
    h.finish(status);
    false
}

/// `DoConnect`
fn do_connect(h: &H) -> bool {
    let (host, socket) = {
        let s = h.st();
        (s.host.clone(), s.request_socket)
    };
    let addr = h.resolve(&host, 21);
    if h.is_async() {
        let op = h.new_op(EE_WON_HOST_OP_CONNECT, Obj::none(), &[], addr, 0);
        h.track(op, TRACK_REQUEST_CONNECT);
        h.queue(socket, op, FOREVER);
        h.release(op);
        return false;
    }
    let wait = h.time_left();
    let status = h.connect(socket, addr, wait);
    check_status(h, status)
}

/// `RecvLineAsync`
fn recv_line_async(h: &H) {
    let socket = h.st().request_socket;
    let op = h.new_op(EE_WON_HOST_OP_RECV_CR_MSG, Obj::none(), &[], [0; 6], 0);
    h.track(op, TRACK_REQUEST);
    h.queue(socket, op, FOREVER);
    h.release(op);
}

/// `SendRequest`
fn send_request(h: &H) -> bool {
    let (req, socket) = {
        let s = h.st();
        let c = Command {
            user: &s.user,
            password: &s.password,
            directory: &s.actual_path,
            file: &s.actual_file,
            binary: s.is_binary,
            local_size: s.local_file_size,
        };
        (ftp::command(s.state, &c), s.request_socket)
    };
    if h.is_async() {
        let op = h.new_op(EE_WON_HOST_OP_SEND_BYTES, Obj::none(), &req, [0; 6], 0);
        h.track(op, 0);
        h.queue(socket, op, FOREVER);
        h.release(op);
        recv_line_async(h);
        return false;
    }
    let wait = h.time_left();
    h.send_bytes(socket, &req, wait);
    true
}

/// `HandleReply(op)` (the reply line's step: `FtpMsg::HandleReply`)
fn handle_reply(h: &H, op: Obj) -> bool {
    let status = h.op_status(op);
    if !check_status(h, status) {
        return false;
    }
    let line = h.op_bytes(op).unwrap_or_default();
    let step = {
        let s = h.st();
        ftp::reply(&line, s.state, !s.actual_path.is_empty(), s.do_resume)
    };
    if step.action == EE_WON_FTP_MORE {
        if h.is_async() {
            recv_line_async(h);
        }
        return true;
    }
    if step.scanned {
        h.st().ftp_status = step.ftp_status;
    }
    match step.action {
        EE_WON_FTP_FINISH => {
            h.finish(step.status);
            false
        }
        EE_WON_FTP_SEND => {
            {
                let mut s = h.st();
                if step.no_resume {
                    s.do_resume = false;
                }
                s.state = step.state;
            }
            send_request(h)
        }
        EE_WON_FTP_DATA => {
            {
                let mut s = h.st();
                s.data_addr = Addr::from_host(step.host, step.port).six_bytes();
                s.state = step.state;
            }
            if !do_data_connection(h) {
                return false;
            }
            send_request(h)
        }
        EE_WON_FTP_RECV => {
            recv_content(h);
            false
        }
        _ => false,
    }
}

/// `DoDataConnection`
fn do_data_connection(h: &H) -> bool {
    let (socket, addr) = {
        let s = h.st();
        (s.data_socket, s.data_addr)
    };
    if h.is_async() {
        let op = h.new_op(EE_WON_HOST_OP_CONNECT, Obj::none(), &[], addr, 0);
        h.track(op, TRACK_DATA_CONNECT);
        h.queue(socket, op, FOREVER);
        h.release(op);
        return false;
    }
    let wait = h.time_left();
    let status = h.connect(socket, addr, wait);
    check_status(h, status)
}

/// `RecvContent`
fn recv_content(h: &H) {
    let (socket, chunk, size, local, resume) = {
        let s = h.st();
        (s.data_socket, s.chunk_completion, s.chunk_size, s.local_path.clone(), s.do_resume)
    };
    let recv = h.new_op(EE_WON_HOST_OP_RECV_BYTES, socket, &[], [0; 6], 0);
    if !chunk.is_none() {
        h.recv_progress(recv, chunk, size);
    }
    if !local.is_empty() {
        h.recv_file(recv, &local, resume);
    }
    if h.is_async() {
        h.track(recv, TRACK_RECV_CONTENT);
        h.run_async(recv, FOREVER);
        h.release(recv);
        return;
    }
    let wait = h.time_left();
    if h.run_block(recv, wait) && local.is_empty() {
        let b = h.op_bytes(recv);
        h.st().content = b;
    }
    let status = h.op_status(recv);
    h.finish(status);
    h.release(recv);
}

impl Plan for FtpGet {
    fn run(h: &H) {
        let (resume, local) = {
            let mut s = h.st();
            s.content = None;
            s.ftp_status = 0;
            (s.do_resume, s.local_path.clone())
        };
        if resume {
            let size = h.file_size(&local);
            let mut s = h.st();
            if size >= 0 {
                s.local_file_size = size as u32;
            } else {
                s.do_resume = false;
            }
        }
        let request = h.new_socket(false);
        h.replace(|s| &mut s.request_socket, request);
        let data = h.new_socket(false);
        h.replace(|s| &mut s.data_socket, data);
        h.st().state = 0;
        if !do_connect(h) {
            return;
        }
        loop {
            let recv = h.new_op(EE_WON_HOST_OP_RECV_CR_MSG, request, &[], [0; 6], 0);
            let wait = h.time_left();
            h.run_block(recv, wait);
            let more = handle_reply(h, recv);
            h.release(recv);
            if !more {
                return;
            }
        }
    }

    fn callback(h: &H, id: i32, op: Obj) -> bool {
        if h.op_killed(op) {
            h.kill(Obj::none(), true);
            return true;
        }
        if h.op_status(op) != ST_SUCCESS {
            let status = h.op_status(op);
            h.finish(status);
            return true;
        }
        match id {
            TRACK_REQUEST_CONNECT => recv_line_async(h),
            TRACK_DATA_CONNECT => {
                send_request(h);
            }
            TRACK_REQUEST => {
                handle_reply(h, op);
            }
            TRACK_RECV_CONTENT => {
                let local_empty = h.st().local_path.is_empty();
                if local_empty && h.op_status(op) == ST_SUCCESS {
                    let b = h.op_bytes(op);
                    h.st().content = b;
                }
                let status = h.op_status(op);
                h.finish(status);
            }
            _ => return false,
        }
        true
    }

    fn cleanup(h: &H) {
        h.base_cleanup();
        for slot in [0, 1] {
            let socket = {
                let mut s = h.st();
                std::mem::take(if slot == 0 { &mut s.request_socket } else { &mut s.data_socket })
            };
            if !socket.is_none() {
                h.close(socket);
                h.release(socket);
            }
        }
    }

    fn num(&self, which: i32, _index: i64) -> i64 {
        if which == EE_WON_FTPGET_STATUS {
            i64::from(self.ftp_status)
        } else {
            0
        }
    }

    fn bytes(&self, which: i32, _index: i64) -> Option<Vec<u8>> {
        match which {
            EE_WON_FTPGET_CONTENT => self.content.clone(),
            EE_WON_FTPGET_LOCAL => Some(self.local_path.clone()),
            EE_WON_FTPGET_REMOTE => Some(self.remote_path.clone()),
            _ => None,
        }
    }

    fn set(h: &H, which: i32, v: &SetValue) {
        match which {
            EE_WON_FTPGET_LOCAL_PATH => h.st().local_path = v.data.to_vec(),
            EE_WON_FTPGET_REMOTE_PATH => h.st().set_remote_path(v.data),
            EE_WON_FTPGET_USER => h.st().user = v.data.to_vec(),
            EE_WON_FTPGET_PASSWORD => h.st().password = v.data.to_vec(),
            EE_WON_FTPGET_BINARY => h.st().is_binary = v.num != 0,
            EE_WON_FTPGET_RESUME => h.st().do_resume = v.num != 0,
            EE_WON_FTPGET_CHUNK => {
                h.add_ref(v.obj);
                h.replace(|s| &mut s.chunk_completion, v.obj);
                h.st().chunk_size = v.num as u32;
            }
            _ => {}
        }
    }

    fn describe(&self) -> String {
        format!(
            "host {} remote {} local {} user {} pass {} path {} file {} content {} st {} chunk {}:{} data {} bin {} \
             resume {} size {} state {} rsock {} dsock {}",
            hostop::hex(&self.host),
            hostop::hex(&self.remote_path),
            hostop::hex(&self.local_path),
            hostop::hex(&self.user),
            hostop::hex(&self.password),
            hostop::hex(&self.actual_path),
            hostop::hex(&self.actual_file),
            self.content.as_ref().map_or("null".to_string(), |c| hostop::hex(c)),
            self.ftp_status,
            self.chunk_completion.0,
            self.chunk_size,
            hostop::hex(&self.data_addr),
            u8::from(self.is_binary),
            u8::from(self.do_resume),
            self.local_file_size,
            self.state,
            self.request_socket.0,
            self.data_socket.0
        )
    }

    fn take_objects(&mut self) -> Vec<Obj> {
        [
            std::mem::take(&mut self.chunk_completion),
            std::mem::take(&mut self.request_socket),
            std::mem::take(&mut self.data_socket),
        ]
        .into_iter()
        .filter(|o| !o.is_none())
        .collect()
    }
}

/// `new FTPGetOp(host, remotePath)`: the Rust op a `RustServerOp` runs.
///
/// # Safety
/// The texts hold their lengths' bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftpget_new(
    host: *const u8,
    host_len: usize,
    path: *const u8,
    path_len: usize,
) -> *mut EeWonHostOp {
    // SAFETY: the contract
    let (h, p) = unsafe { (crate::ffi::bytes(host, host_len), crate::ffi::bytes(path, path_len)) };
    EeWonHostOp::boxed(FtpGet::new(h, p))
}

/// `new FTPGetOp(url, doExtractUserAndPassword)`: the Rust op a `RustServerOp` runs.
///
/// # Safety
/// `url` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftpget_new_url(url: *const u8, len: usize, extract: bool) -> *mut EeWonHostOp {
    // SAFETY: the contract
    EeWonHostOp::boxed(FtpGet::from_url(unsafe { crate::ffi::bytes(url, len) }, extract))
}
