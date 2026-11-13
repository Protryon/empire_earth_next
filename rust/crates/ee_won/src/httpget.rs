//! `HTTPGetOp` (WONMisc/HTTPGetOp.cpp) on the Rust op base (hostop.rs): an HTTP/1.1 GET through a proxy or not,
//! with redirects (a child op of its own kind), chunked replies, range resumes and the HTTP cache's entry. The
//! members are the op's (the socket, the cache and its entry, the host and paths, the flags, the reply's times,
//! type, lengths and content, the status, the progress completion, the send and receive modes, the redirect count)
//! and every step: `RunHook`, `DoConnect`, `SendRequest`, `AsyncRecvCRMsg`, `ExtractChunkLength`,
//! `RecvChunkLength`, `DoneRecvContent`, `RecvContent`, `CallbackHook`, `FinishRedirect`, `FollowRedirect`,
//! `DoneRecvHeader`, `CleanupHook`, `DoFinish`, `ExtractStatus`, `ExtractRedirect`, `ExtractHeaderLine`. The
//! lines' reading and the request's text are http.rs's.
//!
//! Its users hold a `RustServerOp` (`RustServerOp::NewHttpGet`): `GetHTTPDocumentOp`'s two downloads, the lobby's
//! advert image (`InitLogic`), HTTPEngine's connections (the socket, the send and receive modes, the host) through
//! the setters and results below. The web proxy file (`_wonwebproxy.txt`, read once per process) stays the C++
//! side's (`EE_WON_HOST_HTTP_PROXY`).
//!
//! Behaviour: `golden_hostop.txt`'s `H` sequences, recorded over the original's statements before the C++ class
//! was deleted. Reached by nothing in the game but an Internet login's documents (the WON web servers are gone).
//!
//! The original's undefined corners, defined: a reply line that is NULL (a dereference) reads as an empty line;
//! the members the constructors left unset (`mIfRange`, the chunk and redirect state before a run, the progress
//! size, the HTTP status) start at 0 or false.

use crate::hostop::{
    self, EeWonHostOp, Host, Obj, Plan, SetValue, EE_WON_HOST_OP_CONNECT, EE_WON_HOST_OP_RECV_BYTES,
    EE_WON_HOST_OP_RECV_CR_MSG, EE_WON_HOST_OP_SEND_BYTES, FOREVER, ST_SUCCESS,
};
use crate::http::{self, c_str};

const TRACK_CONNECT: i32 = 1;
const TRACK_RECV_STATUS: i32 = 2;
const TRACK_RECV_HEADER: i32 = 3;
const TRACK_RECV_CONTENT: i32 = 4;
const TRACK_REDIRECT: i32 = 5;
const TRACK_RECV_CHUNK_LENGTH: i32 = 6;
const TRACK_RECV_CHUNK_CRLF: i32 = 7;
const TRACK_RECV_FOOTER: i32 = 8;
const TRACK_SEND_REQUEST: i32 = 9;

/// `WS_HTTP_InvalidHeader` (a malformed status line or chunk length)
const ST_INVALID_HEADER: i32 = 100092;
/// `WS_HTTP_StatusError`
const ST_STATUS_ERROR: i32 = 100093;
/// `WS_HTTP_InvalidRedirect`
const ST_INVALID_REDIRECT: i32 = 100094;
/// `WS_HTTP_TooManyRedirects`
const ST_TOO_MANY_REDIRECTS: i32 = 100095;
/// `WS_HTTP_RedirectNotAllowed`
const ST_NO_REDIRECT: i32 = 100096;
/// `WS_HTTP_SendOnly` (a send-only request's end)
const ST_SEND_ONLY: i32 = 100097;
/// `WS_None`, a socket's status while it is open
const ST_NONE: i32 = 100004;

// Setters (`ee_won_hostop_set`) and results (`ee_won_hostop_num`, `ee_won_hostop_bytes`); a number both reads and
// sets where it is both:
/// `SetLocalPath` / `GetLocalPath()` (bytes)
pub const EE_WON_HTTPGET_LOCAL_PATH: i32 = 1;
/// `SetHTTPCache(cache)` (the object)
pub const EE_WON_HTTPGET_CACHE: i32 = 2;
/// `SetBinaryFile` (number)
pub const EE_WON_HTTPGET_BINARY: i32 = 3;
/// `SetOnlySend()` (0), `SetSendAndRecv()` (1), `SetOnlyRecv()` (2) (number)
pub const EE_WON_HTTPGET_MODE: i32 = 4;
/// `SetDoRangeResume` (number)
pub const EE_WON_HTTPGET_RANGE_RESUME: i32 = 5;
/// `SetCloseConnection` (number)
pub const EE_WON_HTTPGET_CLOSE: i32 = 6;
/// `SetSocket(socket)` (the object; none: NULL) / the socket (`ee_won_hostop_num`: the object, 0 for none; the
/// C++ `GetSocket()` adds the connected test)
pub const EE_WON_HTTPGET_SOCKET: i32 = 7;
/// `SetRecvChunkCompletion(completion, size)` (the object, the number)
pub const EE_WON_HTTPGET_CHUNK: i32 = 8;
/// `mNumRedirects` (number)
pub const EE_WON_HTTPGET_REDIRECTS: i32 = 9;
/// `mLastModified` (number; `GetLastModifiedTime()` is the C++ side's, over the local file's time)
pub const EE_WON_HTTPGET_LAST_MODIFIED: i32 = 10;
/// `GetHost()` (bytes)
pub const EE_WON_HTTPGET_HOST: i32 = 11;
/// `GetHTTPStatus()` (number)
pub const EE_WON_HTTPGET_STATUS: i32 = 12;
/// `ContentIsNew()` (number)
pub const EE_WON_HTTPGET_IS_NEW: i32 = 13;
/// `mContentType` (bytes)
pub const EE_WON_HTTPGET_CONTENT_TYPE: i32 = 14;
/// `mContentLength` (number)
pub const EE_WON_HTTPGET_CONTENT_LENGTH: i32 = 15;
/// `mContent` (bytes; none: NULL)
pub const EE_WON_HTTPGET_CONTENT: i32 = 16;
/// `mRedirectHost`, `mRedirectPath` (bytes)
pub const EE_WON_HTTPGET_REDIRECT_HOST: i32 = 17;
pub const EE_WON_HTTPGET_REDIRECT_PATH: i32 = 18;
/// `GetRemotePath()` (bytes)
pub const EE_WON_HTTPGET_REMOTE_PATH: i32 = 19;
/// `GetOnlySend()`, `GetOnlyRecv()` (number)
pub const EE_WON_HTTPGET_ONLY_SEND: i32 = 20;
pub const EE_WON_HTTPGET_ONLY_RECV: i32 = 21;

/// The op's members.
#[derive(Default)]
pub(crate) struct HttpGet {
    socket: Obj,
    cache: Obj,
    cache_entry: Obj,
    host: Vec<u8>,
    remote_path: Vec<u8>,
    local_path: Vec<u8>,
    content_is_chunked: bool,
    is_first_chunk: bool,
    binary_file: bool,
    close_connection: bool,
    do_close: bool,
    do_range_resume: bool,
    proxy_host: Vec<u8>,
    redirect_host: Vec<u8>,
    redirect_path: Vec<u8>,
    server_time: i64,
    expire_time: i64,
    last_modified: i64,
    local_file_modify_time: i64,
    local_file_size: u64,
    entity_tag: Vec<u8>,
    content_type: Vec<u8>,
    content_length: u32,
    chunk_length: u32,
    content: Option<Vec<u8>>,
    accumulator: Vec<u8>,
    http_status: i32,
    content_is_new: bool,
    chunk_completion: Obj,
    chunk_size: u32,
    only_send: bool,
    only_recv: bool,
    auto_redirect: bool,
    num_redirects: i32,
}

impl HttpGet {
    /// `Init`: binary, closing, resuming, redirecting, sending and receiving.
    fn init() -> HttpGet {
        HttpGet {
            binary_file: true,
            close_connection: true,
            do_range_resume: true,
            auto_redirect: true,
            ..Default::default()
        }
    }

    /// `HTTPGetOp(host, path)`
    pub(crate) fn new(host: &[u8], path: &[u8]) -> HttpGet {
        HttpGet { host: host.to_vec(), remote_path: path.to_vec(), ..HttpGet::init() }
    }

    /// `HTTPGetOp(url)`: the host and path split ("/" when none)
    pub(crate) fn from_url(url: &[u8]) -> HttpGet {
        let ((a, b), path) = http::split_url(url);
        let mut g = HttpGet::init();
        g.host = url[a..b].to_vec();
        g.remote_path = match path {
            Some((p, e)) => url[p..e].to_vec(),
            None => b"/".to_vec(),
        };
        g
    }
}

type H = Host<HttpGet>;

// ---- the host calls of this op (hostop.rs's EE_WON_HOST_HTTP_* and the socket's, the file's and the cache's)

fn socket_connected(h: &H, socket: Obj) -> bool {
    h.call(hostop::EE_WON_HOST_SOCKET_CONNECTED, [socket, Obj::none(), Obj::none()], [0, 0], &[], [0; 6]).num != 0
}
fn recv_line(h: &H, socket: Obj, timeout: u32) -> (i32, Vec<u8>) {
    let r =
        h.call(hostop::EE_WON_HOST_RECV_CR, [socket, Obj::none(), Obj::none()], [i64::from(timeout), 0], &[], [0; 6]);
    // a NULL line (the original dereferenced it) reads as an empty one
    (r.num as i32, r.bytes.unwrap_or_default())
}
fn file_stat(h: &H, path: &[u8], what: i64) -> i64 {
    h.call(hostop::EE_WON_HOST_FILE_STAT, [Obj::none(); 3], [what, 0], path, [0; 6]).num
}
fn now(h: &H) -> i64 {
    h.call(hostop::EE_WON_HOST_TIME, [Obj::none(); 3], [0, 0], &[], [0; 6]).num
}
fn entry_num(h: &H, entry: Obj, what: i64) -> i64 {
    h.call(hostop::EE_WON_HOST_ENTRY_GET, [entry, Obj::none(), Obj::none()], [what, 0], &[], [0; 6]).num
}
fn entry_bytes(h: &H, entry: Obj, what: i64) -> Vec<u8> {
    let r = h.call(hostop::EE_WON_HOST_ENTRY_GET, [entry, Obj::none(), Obj::none()], [what, 0], &[], [0; 6]);
    r.bytes.unwrap_or_default()
}
fn entry_set(h: &H, entry: Obj, what: i64, num: i64, data: &[u8]) {
    h.call(hostop::EE_WON_HOST_ENTRY_SET, [entry, Obj::none(), Obj::none()], [what, num], data, [0; 6]);
}
fn recv_opt(h: &H, code: i32, op: Obj, num: i64) {
    h.call(code, [op, Obj::none(), Obj::none()], [num, 0], &[], [0; 6]);
}
fn child_num(h: &H, op: Obj, which: i32) -> i64 {
    h.call(hostop::EE_WON_HOST_HOSTOP_NUM, [op, Obj::none(), Obj::none()], [i64::from(which), 0], &[], [0; 6]).num
}
fn child_bytes(h: &H, op: Obj, which: i32) -> Option<Vec<u8>> {
    h.call(hostop::EE_WON_HOST_HOSTOP_BYTES, [op, Obj::none(), Obj::none()], [i64::from(which), 0], &[], [0; 6]).bytes
}
fn child_set(h: &H, op: Obj, which: i32, num: i64, data: &[u8], obj: Obj) {
    h.call(hostop::EE_WON_HOST_HOSTOP_SET, [op, obj, Obj::none()], [i64::from(which), num], data, [0; 6]);
}

/// `IsAsync()`: `GetMode()==1`
fn async_mode(h: &H) -> bool {
    h.is_async()
}

/// `CheckStatus(status)`
fn check_status(h: &H, status: i32) -> bool {
    if status == ST_SUCCESS {
        return true;
    }
    h.finish(status);
    false
}

/// `DoFinish`
fn do_finish(h: &H) {
    let status = h.st().http_status;
    h.finish(if status > 304 { ST_STATUS_ERROR } else { ST_SUCCESS });
}

/// `ExtractStatus(line)` (HttpExtractStatus)
fn extract_status(h: &H, line: &[u8]) -> bool {
    let Some(status) = http::status_line(line) else {
        h.finish(ST_INVALID_HEADER);
        return false;
    };
    let close = {
        let mut s = h.st();
        s.http_status = status;
        s.close_connection
    };
    if status > 304 && close {
        h.finish(ST_STATUS_ERROR);
        return false;
    }
    true
}

/// `ExtractRedirect(location)` (HttpExtractRedirect)
fn extract_redirect(h: &H, location: &[u8]) -> bool {
    let Some(((a, b), path)) = http::redirect(location) else {
        h.finish(ST_INVALID_REDIRECT);
        return false;
    };
    let mut s = h.st();
    match path {
        None => {
            s.redirect_host = c_str(&location[a..]).to_vec();
            s.redirect_path = b"/".to_vec();
        }
        Some(p) => {
            s.redirect_host = location[a..b].to_vec();
            s.redirect_path = c_str(&location[p..]).to_vec();
        }
    }
    true
}

/// `ExtractHeaderLine(line)` (HttpExtractHeaderLine)
fn extract_header_line(h: &H, line: &[u8]) -> bool {
    let hd = http::header_line(line);
    if !hd.colon {
        return true;
    }
    let value = line.get(hd.value as usize..).unwrap_or(&[]);
    let mut s = h.st();
    match hd.key {
        0 => s.content_length = hd.number as u32,
        1 => s.content_type = c_str(value).to_vec(),
        2 => s.last_modified = hd.time,
        3 => {
            drop(s);
            return extract_redirect(h, value);
        }
        4 => {
            if hd.flag {
                s.content_is_chunked = true;
                s.is_first_chunk = true;
            }
        }
        5 => {
            if hd.flag {
                s.do_close = true;
            }
        }
        6 => s.server_time = hd.time,
        7 => s.expire_time = hd.time,
        8 => {
            let mut tag = c_str(value).to_vec();
            if hd.tag_len < 0 {
                tag.clear();
            } else {
                tag.resize(hd.tag_len as usize, 0);
            }
            s.entity_tag = tag;
        }
        _ => {}
    }
    true
}

/// `DoConnect`: connects to the proxy or host, port 80, unless connected
fn do_connect(h: &H) -> bool {
    let socket = h.st().socket;
    if socket_connected(h, socket) {
        return true;
    }
    let target = {
        let s = h.st();
        if !s.proxy_host.is_empty() {
            s.proxy_host.clone()
        } else {
            s.host.clone()
        }
    };
    let addr = h.resolve(&target, 80);
    if async_mode(h) {
        let op = h.new_op(EE_WON_HOST_OP_CONNECT, Obj::none(), &[], addr, 0);
        h.track(op, TRACK_CONNECT);
        h.queue(socket, op, FOREVER);
        h.release(op);
        return false;
    }
    let wait = h.time_left();
    let status = h.connect(socket, addr, wait);
    check_status(h, status)
}

/// `SendRequest`: the GET with its resume or validation headers
fn send_request(h: &H) -> bool {
    let local = h.st().local_path.clone();
    let mut file = false;
    if !local.is_empty() && file_stat(h, &local, 0) != 0 {
        let size = file_stat(h, &local, 1) as u64;
        let time = file_stat(h, &local, 2);
        let mut s = h.st();
        s.local_file_size = size;
        s.local_file_modify_time = time;
        file = true;
    }
    let entry = h.st().cache_entry;
    let cache_ok = entry.is_none() || entry_num(h, entry, 0) != 0;
    let (req, socket) = {
        let s = h.st();
        let q = http::Request {
            proxy: !s.proxy_host.is_empty(),
            host: &s.host,
            path: &s.remote_path,
            close: s.close_connection,
            file: file.then_some((s.local_file_size, s.local_file_modify_time)),
            range_resume: s.do_range_resume,
            entity_tag: &s.entity_tag,
            cache_ok,
        };
        (http::request(&q), s.socket)
    };
    if async_mode(h) {
        let op = h.new_op(EE_WON_HOST_OP_SEND_BYTES, Obj::none(), &req, [0; 6], 0);
        h.track(op, TRACK_SEND_REQUEST);
        h.queue(socket, op, FOREVER);
        h.release(op);
        if !h.st().only_send {
            async_recv_line(h, TRACK_RECV_STATUS);
        }
        return false;
    }
    let wait = h.time_left();
    h.send_bytes(socket, &req, wait);
    true
}

/// `AsyncRecvCRMsg(type)`
fn async_recv_line(h: &H, id: i32) {
    let socket = h.st().socket;
    let op = h.new_op(EE_WON_HOST_OP_RECV_CR_MSG, Obj::none(), &[], [0; 6], 0);
    h.track(op, id);
    h.queue(socket, op, FOREVER);
    h.release(op);
}

/// `ExtractChunkLength(line)`: true to read the chunk
fn extract_chunk_length(h: &H, line: &[u8]) -> bool {
    let Some(n) = http::chunk_length(line) else {
        h.finish(ST_INVALID_HEADER);
        return false;
    };
    let entry = {
        let mut s = h.st();
        s.chunk_length = n;
        s.content_length = s.content_length.wrapping_add(n);
        if n != 0 {
            return true;
        }
        s.cache_entry
    };
    if !entry.is_none() {
        entry_set(h, entry, 0, 1, &[]);
    }
    {
        let mut s = h.st();
        if s.local_path.is_empty() {
            s.content = Some(std::mem::take(&mut s.accumulator));
        }
    }
    if async_mode(h) {
        async_recv_line(h, TRACK_RECV_FOOTER);
        return false;
    }
    loop {
        let socket = h.st().socket;
        let wait = h.time_left();
        let (status, footer) = recv_line(h, socket, wait);
        if !check_status(h, status) {
            return false;
        }
        if footer.len() == 1 {
            break;
        }
    }
    do_finish(h);
    false
}

/// `RecvChunkLength`: the CRLF after a chunk, then the next chunk's length line
fn recv_chunk_length(h: &H) -> bool {
    let first = {
        let mut s = h.st();
        if s.is_first_chunk {
            s.content_length = 0;
        }
        s.is_first_chunk
    };
    if async_mode(h) {
        if !first {
            async_recv_line(h, TRACK_RECV_CHUNK_CRLF);
        }
        async_recv_line(h, TRACK_RECV_CHUNK_LENGTH);
        return false;
    }
    let socket = h.st().socket;
    if !first {
        let wait = h.time_left();
        let (status, _) = recv_line(h, socket, wait);
        if !check_status(h, status) {
            return false;
        }
    }
    let wait = h.time_left();
    let (status, line) = recv_line(h, socket, wait);
    if !check_status(h, status) {
        return false;
    }
    extract_chunk_length(h, &line)
}

/// `DoneRecvContent(op)`: true to keep receiving
fn done_recv_content(h: &H, op: Obj) -> bool {
    let local_empty = {
        let mut s = h.st();
        s.is_first_chunk = false;
        s.local_path.is_empty()
    };
    if local_empty && h.op_status(op) == ST_SUCCESS {
        let b = h.op_bytes(op);
        let mut s = h.st();
        if s.content_is_chunked {
            if let Some(b) = b {
                s.accumulator.extend_from_slice(&b);
            }
        } else {
            s.content = b;
        }
    }
    if h.op_status(op) != ST_SUCCESS {
        let status = h.op_status(op);
        h.finish(status);
        return false;
    }
    let (chunked, entry) = {
        let s = h.st();
        (s.content_is_chunked, s.cache_entry)
    };
    if chunked {
        return recv_chunk_length(h);
    }
    if !entry.is_none() {
        entry_set(h, entry, 0, 1, &[]);
    }
    do_finish(h);
    false
}

/// `RecvContent`: the body to memory or the local file (appending for a resume), an error's body skipped
fn recv_content(h: &H) {
    let (status, do_close) = {
        let s = h.st();
        (s.http_status, s.do_close)
    };
    if status == 304 {
        h.st().content_is_new = false;
        do_finish(h);
        return;
    }
    let skip = !(200..300).contains(&status);
    if skip && do_close {
        h.finish(ST_STATUS_ERROR);
        return;
    }
    loop {
        let (length, socket, chunk, chunk_size, entry, local) = {
            let s = h.st();
            let length = if s.content_is_chunked { s.chunk_length } else { s.content_length };
            (length, s.socket, s.chunk_completion, s.chunk_size, s.cache_entry, s.local_path.clone())
        };
        let recv = h.new_op(EE_WON_HOST_OP_RECV_BYTES, socket, &[], [0; 6], length);
        if skip {
            recv_opt(h, hostop::EE_WON_HOST_RECV_SKIP, recv, 1);
        } else {
            if !chunk.is_none() {
                h.recv_progress(recv, chunk, chunk_size);
            }
            if !entry.is_none() {
                entry_set(h, entry, 0, 0, &[]);
            }
            if !local.is_empty() {
                let (append, up_to_date, binary, modified) = {
                    let s = h.st();
                    let append = s.http_status == 206 || (s.content_is_chunked && !s.is_first_chunk);
                    let up = http::up_to_date(
                        append,
                        s.local_file_size,
                        s.content_length,
                        s.local_file_modify_time,
                        s.last_modified,
                        s.do_close,
                    );
                    (append, up, s.binary_file, s.last_modified)
                };
                if up_to_date {
                    do_finish(h);
                    h.st().content_is_new = false;
                    h.release(recv);
                    return;
                }
                recv_opt(h, hostop::EE_WON_HOST_RECV_BINARY, recv, i64::from(binary));
                h.recv_file(recv, &local, append);
                recv_opt(h, hostop::EE_WON_HOST_RECV_MODIFY_TIME, recv, modified);
            }
        }
        if async_mode(h) {
            h.track(recv, TRACK_RECV_CONTENT);
            h.run_async(recv, FOREVER);
            h.release(recv);
            return;
        }
        let wait = h.time_left();
        if h.run_block(recv, wait) {
            let more = done_recv_content(h, recv);
            h.release(recv);
            if !more {
                return;
            }
        } else {
            let status = h.op_status(recv);
            h.finish(status);
            h.release(recv);
            return;
        }
    }
}

/// `FinishRedirect(op)`: the redirected request's results and status
fn finish_redirect(h: &H, op: Obj) {
    let last_modified = child_num(h, op, EE_WON_HTTPGET_LAST_MODIFIED);
    h.st().last_modified = last_modified;
    let content_type = child_bytes(h, op, EE_WON_HTTPGET_CONTENT_TYPE).unwrap_or_default();
    h.st().content_type = content_type;
    let length = child_num(h, op, EE_WON_HTTPGET_CONTENT_LENGTH) as u32;
    h.st().content_length = length;
    let content = child_bytes(h, op, EE_WON_HTTPGET_CONTENT);
    h.st().content = content;
    let local = child_bytes(h, op, EE_WON_HTTPGET_LOCAL_PATH).unwrap_or_default();
    h.st().local_path = local;
    let redirect_host = child_bytes(h, op, EE_WON_HTTPGET_REDIRECT_HOST).unwrap_or_default();
    if !redirect_host.is_empty() {
        let redirect_path = child_bytes(h, op, EE_WON_HTTPGET_REDIRECT_PATH).unwrap_or_default();
        let mut s = h.st();
        s.redirect_host = redirect_host;
        s.redirect_path = redirect_path;
    }
    let status = child_num(h, op, EE_WON_HTTPGET_STATUS) as i32;
    h.st().http_status = status;
    let is_new = child_num(h, op, EE_WON_HTTPGET_IS_NEW) != 0;
    h.st().content_is_new = is_new;
    let redirects = child_num(h, op, EE_WON_HTTPGET_REDIRECTS) as i32;
    h.st().num_redirects = redirects;
    let status = h.op_status(op);
    h.finish(status);
}

/// `FollowRedirect`: the request again at the redirect's target (at most 5 times)
fn follow_redirect(h: &H) {
    h.kill_all();
    let socket = std::mem::take(&mut h.st().socket);
    if !socket.is_none() {
        h.call(hostop::EE_WON_HOST_SOCKET_KILL, [socket, Obj::none(), Obj::none()], [0, 0], &[], [0; 6]);
        h.release(socket);
    }
    let (redirects, auto) = {
        let s = h.st();
        (s.num_redirects, s.auto_redirect)
    };
    if redirects > 5 {
        h.finish(ST_TOO_MANY_REDIRECTS);
        return;
    }
    if !auto {
        h.finish(ST_NO_REDIRECT);
        return;
    }
    let (target, host_len, binary, local, cache, chunk, chunk_size) = {
        let s = h.st();
        let mut t = s.redirect_host.clone();
        t.extend_from_slice(&s.redirect_path);
        (t, s.redirect_host.len(), s.binary_file, s.local_path.clone(), s.cache, s.chunk_completion, s.chunk_size)
    };
    let child = h.call(hostop::EE_WON_HOST_NEW_HTTP_GET, [Obj::none(); 3], [host_len as i64, 0], &target, [0; 6]).obj;
    child_set(h, child, EE_WON_HTTPGET_BINARY, i64::from(binary), &[], Obj::none());
    child_set(h, child, EE_WON_HTTPGET_LOCAL_PATH, 0, &local, Obj::none());
    child_set(h, child, EE_WON_HTTPGET_CACHE, 0, &[], cache);
    child_set(h, child, EE_WON_HTTPGET_REDIRECTS, i64::from(redirects) + 1, &[], Obj::none());
    if !chunk.is_none() {
        child_set(h, child, EE_WON_HTTPGET_CHUNK, i64::from(chunk_size), &[], chunk);
    }
    if async_mode(h) {
        h.track(child, TRACK_REDIRECT);
        h.run_async(child, FOREVER);
        h.release(child);
        return;
    }
    let wait = h.time_left();
    h.run_block(child, wait);
    finish_redirect(h, child);
    h.release(child);
}

/// `DoneRecvHeader`: a 301/302 followed; the cache entry's expiry and tag; then the body
fn done_recv_header(h: &H) {
    let (status, entry, server, expire, tag) = {
        let s = h.st();
        (s.http_status, s.cache_entry, s.server_time, s.expire_time, s.entity_tag.clone())
    };
    if status == 301 || status == 302 {
        follow_redirect(h);
        return;
    }
    if !entry.is_none() {
        if server > 0 && expire > 0 && expire > server {
            let t = now(h).wrapping_add(expire).wrapping_sub(server);
            entry_set(h, entry, 1, t, &[]);
        }
        if !tag.is_empty() {
            entry_set(h, entry, 3, 0, &tag);
        }
    }
    let chunked = h.st().content_is_chunked;
    if chunked && !recv_chunk_length(h) {
        return;
    }
    recv_content(h);
}

/// a reply line of a finished receive (NULL: empty)
fn line_of(h: &H, op: Obj) -> Vec<u8> {
    h.op_bytes(op).unwrap_or_default()
}

impl Plan for HttpGet {
    fn run(h: &H) {
        let only_recv = h.st().only_recv;
        if !only_recv {
            let proxy = h.call(hostop::EE_WON_HOST_HTTP_PROXY, [Obj::none(); 3], [0, 0], &[], [0; 6]).bytes;
            let old_entry = {
                let mut s = h.st();
                if let Some(p) = proxy.filter(|p| !p.is_empty()) {
                    s.proxy_host = p;
                }
                s.last_modified = 0;
                s.server_time = 0;
                s.expire_time = 0;
                s.content_type.clear();
                s.content_length = 0;
                s.content = None;
                s.do_close = s.close_connection;
                s.http_status = 0;
                s.local_file_modify_time = 0;
                s.local_file_size = 0;
                s.num_redirects = 0;
                s.content_is_new = true;
                s.chunk_length = 0;
                s.content_is_chunked = false;
                s.is_first_chunk = false;
                std::mem::take(&mut s.cache_entry)
            };
            h.release(old_entry);
            h.st().accumulator.clear();
            if h.st().socket.is_none() {
                let socket = h.new_socket(false);
                h.replace(|s| &mut s.socket, socket);
            }
            let (cache, key) = {
                let mut s = h.st();
                s.redirect_host.clear();
                s.redirect_path.clear();
                let mut key = s.host.clone();
                key.extend_from_slice(&s.remote_path);
                (s.cache, key)
            };
            if !cache.is_none() {
                let entry = h
                    .call(hostop::EE_WON_HOST_CACHE_ENTRY, [cache, Obj::none(), Obj::none()], [0, 0], &key, [0; 6])
                    .obj;
                h.replace(|s| &mut s.cache_entry, entry);
                if !entry.is_none() {
                    let local = entry_bytes(h, entry, 2);
                    h.st().local_path = local;
                    let tag = entry_bytes(h, entry, 3);
                    h.st().entity_tag = tag;
                    if entry_num(h, entry, 0) != 0 && entry_num(h, entry, 1) > now(h) {
                        h.st().content_is_new = false;
                        h.finish(ST_SUCCESS);
                        return;
                    }
                }
            }
            if !do_connect(h) {
                return;
            }
            if !send_request(h) {
                return;
            }
        }
        if h.st().only_send {
            h.finish(ST_SEND_ONLY);
            return;
        }
        if h.st().only_recv && async_mode(h) {
            async_recv_line(h, TRACK_RECV_STATUS);
            return;
        }
        let socket = h.st().socket;
        let wait = h.time_left();
        let (status, line) = recv_line(h, socket, wait);
        if !check_status(h, status) {
            return;
        }
        if !extract_status(h, &line) {
            return;
        }
        loop {
            let socket = h.st().socket;
            let wait = h.time_left();
            let (status, line) = recv_line(h, socket, wait);
            if !check_status(h, status) {
                return;
            }
            if line.len() == 1 {
                break;
            }
            if !extract_header_line(h, &line) {
                return;
            }
        }
        done_recv_header(h);
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
            TRACK_CONNECT => {
                send_request(h);
            }
            TRACK_SEND_REQUEST => {
                if h.st().only_send {
                    h.finish(ST_SEND_ONLY);
                }
            }
            TRACK_RECV_STATUS => {
                let line = line_of(h, op);
                if extract_status(h, &line) {
                    async_recv_line(h, TRACK_RECV_HEADER);
                }
            }
            TRACK_RECV_HEADER => {
                let line = line_of(h, op);
                if line.len() == 1 {
                    done_recv_header(h);
                } else if extract_header_line(h, &line) {
                    async_recv_line(h, TRACK_RECV_HEADER);
                }
            }
            TRACK_RECV_FOOTER => {
                let line = line_of(h, op);
                if line.len() == 1 {
                    do_finish(h);
                } else {
                    async_recv_line(h, TRACK_RECV_FOOTER);
                }
            }
            TRACK_RECV_CONTENT => {
                done_recv_content(h, op);
            }
            TRACK_REDIRECT => finish_redirect(h, op),
            TRACK_RECV_CHUNK_LENGTH => {
                let line = line_of(h, op);
                if extract_chunk_length(h, &line) {
                    recv_content(h);
                }
            }
            _ => return false,
        }
        true
    }

    fn cleanup(h: &H) {
        h.base_cleanup();
        let socket = h.st().socket;
        if !socket.is_none() {
            let (close, only_send) = {
                let s = h.st();
                (s.do_close, s.only_send)
            };
            let open = (close && !only_send)
                || h.call(hostop::EE_WON_HOST_SOCKET_STATUS, [socket, Obj::none(), Obj::none()], [0, 0], &[], [0; 6])
                    .num
                    != i64::from(ST_NONE);
            if open {
                h.close(socket);
                let s = std::mem::take(&mut h.st().socket);
                h.release(s);
            }
        }
        let (only_send, cache, entry) = {
            let s = h.st();
            (s.only_send, s.cache, s.cache_entry)
        };
        if !only_send && !cache.is_none() && !entry.is_none() {
            h.call(hostop::EE_WON_HOST_CACHE_UPDATE, [cache, entry, Obj::none()], [0, 0], &[], [0; 6]);
        }
    }

    fn num(&self, which: i32, _index: i64) -> i64 {
        match which {
            EE_WON_HTTPGET_SOCKET => self.socket.0 as i64,
            EE_WON_HTTPGET_REDIRECTS => i64::from(self.num_redirects),
            EE_WON_HTTPGET_LAST_MODIFIED => self.last_modified,
            EE_WON_HTTPGET_STATUS => i64::from(self.http_status),
            EE_WON_HTTPGET_IS_NEW => i64::from(self.content_is_new),
            EE_WON_HTTPGET_CONTENT_LENGTH => i64::from(self.content_length),
            EE_WON_HTTPGET_ONLY_SEND => i64::from(self.only_send),
            EE_WON_HTTPGET_ONLY_RECV => i64::from(self.only_recv),
            _ => 0,
        }
    }

    fn bytes(&self, which: i32, _index: i64) -> Option<Vec<u8>> {
        match which {
            EE_WON_HTTPGET_LOCAL_PATH => Some(self.local_path.clone()),
            EE_WON_HTTPGET_HOST => Some(self.host.clone()),
            EE_WON_HTTPGET_REMOTE_PATH => Some(self.remote_path.clone()),
            EE_WON_HTTPGET_CONTENT_TYPE => Some(self.content_type.clone()),
            EE_WON_HTTPGET_CONTENT => self.content.clone(),
            EE_WON_HTTPGET_REDIRECT_HOST => Some(self.redirect_host.clone()),
            EE_WON_HTTPGET_REDIRECT_PATH => Some(self.redirect_path.clone()),
            _ => None,
        }
    }

    fn set(h: &H, which: i32, v: &SetValue) {
        match which {
            EE_WON_HTTPGET_LOCAL_PATH => h.st().local_path = v.data.to_vec(),
            EE_WON_HTTPGET_CACHE => {
                h.add_ref(v.obj);
                h.replace(|s| &mut s.cache, v.obj);
            }
            EE_WON_HTTPGET_BINARY => h.st().binary_file = v.num != 0,
            EE_WON_HTTPGET_MODE => {
                let mut s = h.st();
                s.only_send = v.num == 0;
                s.only_recv = v.num == 2;
            }
            EE_WON_HTTPGET_RANGE_RESUME => h.st().do_range_resume = v.num != 0,
            EE_WON_HTTPGET_CLOSE => h.st().close_connection = v.num != 0,
            EE_WON_HTTPGET_SOCKET => {
                h.add_ref(v.obj);
                h.replace(|s| &mut s.socket, v.obj);
            }
            EE_WON_HTTPGET_CHUNK => {
                h.add_ref(v.obj);
                h.replace(|s| &mut s.chunk_completion, v.obj);
                h.st().chunk_size = v.num as u32;
            }
            EE_WON_HTTPGET_REDIRECTS => h.st().num_redirects = v.num as i32,
            EE_WON_HTTPGET_LAST_MODIFIED => h.st().last_modified = v.num,
            _ => {}
        }
    }

    fn describe(&self) -> String {
        let x = hostop::hex;
        format!(
            "host {} path {} local {} proxy {} redir {}{} tag {} type {} content {} acc {} sock {} cache {} entry {} \
             chunk {}:{} f {}{}{}{}{}{}{}{}{}{}{} times {} {} {} {} size {} len {} clen {} st {} redirects {}",
            x(&self.host),
            x(&self.remote_path),
            x(&self.local_path),
            x(&self.proxy_host),
            x(&self.redirect_host),
            x(&self.redirect_path),
            x(&self.entity_tag),
            x(&self.content_type),
            self.content.as_ref().map_or("null".to_string(), |c| x(c)),
            x(&self.accumulator),
            self.socket.0,
            self.cache.0,
            self.cache_entry.0,
            self.chunk_completion.0,
            self.chunk_size,
            u8::from(self.content_is_chunked),
            u8::from(self.is_first_chunk),
            u8::from(self.binary_file),
            u8::from(self.close_connection),
            u8::from(self.do_close),
            u8::from(self.do_range_resume),
            u8::from(self.content_is_new),
            u8::from(self.only_send),
            u8::from(self.only_recv),
            u8::from(self.auto_redirect),
            0,
            self.server_time,
            self.expire_time,
            self.last_modified,
            self.local_file_modify_time,
            self.local_file_size,
            self.content_length,
            self.chunk_length,
            self.http_status,
            self.num_redirects
        )
    }

    fn take_objects(&mut self) -> Vec<Obj> {
        [
            std::mem::take(&mut self.socket),
            std::mem::take(&mut self.cache),
            std::mem::take(&mut self.cache_entry),
            std::mem::take(&mut self.chunk_completion),
        ]
        .into_iter()
        .filter(|o| !o.is_none())
        .collect()
    }
}

/// `new HTTPGetOp(host, path)`: the Rust op a `RustServerOp` runs.
///
/// # Safety
/// The texts hold their lengths' bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_httpget_new(
    host: *const u8,
    host_len: usize,
    path: *const u8,
    path_len: usize,
) -> *mut EeWonHostOp {
    // SAFETY: the contract
    let (h, p) = unsafe { (crate::ffi::bytes(host, host_len), crate::ffi::bytes(path, path_len)) };
    EeWonHostOp::boxed(HttpGet::new(h, p))
}

/// `new HTTPGetOp(url)`: the Rust op a `RustServerOp` runs.
///
/// # Safety
/// `url` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_httpget_new_url(url: *const u8, len: usize) -> *mut EeWonHostOp {
    // SAFETY: the contract
    EeWonHostOp::boxed(HttpGet::from_url(unsafe { crate::ffi::bytes(url, len) }))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_constructors_split_and_set_the_flags() {
        let g = HttpGet::from_url(b"http://www.sierra.com/motd/sys/motd.txt");
        assert_eq!((g.host.as_slice(), g.remote_path.as_slice()), (&b"www.sierra.com"[..], &b"/motd/sys/motd.txt"[..]));
        let g = HttpGet::from_url(b"onlyhost");
        assert_eq!((g.host.as_slice(), g.remote_path.as_slice()), (&b"onlyhost"[..], &b"/"[..]));
        assert!(g.binary_file && g.close_connection && g.do_range_resume && g.auto_redirect);
        assert!(!g.only_send && !g.only_recv && !g.content_is_new);
    }

    #[test]
    fn header_lines_set_the_members() {
        // an unbound host: its calls answer nothing (a finish does nothing)
        let h = Host::new(HttpGet::new(b"h", b"/"));
        for l in [
            &b"Content-Length: 12\0"[..],
            b"Content-Type: text/plain\0",
            b"Transfer-Encoding: chunked\0",
            b"Connection: close\0",
            b"ETag: \"abc\"x\0",
            b"Location: http://other\0",
            b"no colon\0",
        ] {
            assert!(extract_header_line(&h, l));
        }
        let s = h.st();
        assert_eq!(s.content_length, 12);
        assert_eq!(s.content_type, b"text/plain");
        assert!(s.content_is_chunked && s.is_first_chunk && s.do_close);
        assert_eq!(s.entity_tag, b"\"abc\"");
        assert_eq!((s.redirect_host.as_slice(), s.redirect_path.as_slice()), (&b"other"[..], &b"/"[..]));
        drop(s);
        assert!(!extract_header_line(&h, b"Location: nowhere\0"));
        assert!(extract_status(&h, b"HTTP/1.1 200 OK\0"));
        assert_eq!(h.st().http_status, 200);
        assert!(!extract_status(&h, b"HTTP/1.1 404 Not Found\0"));
        assert!(!extract_status(&h, b"garbage\0"));
    }

    #[test]
    fn setters_and_results_by_number() {
        let h = Host::new(HttpGet::new(b"h", b"/p"));
        let v = |num: i64, data: &'static [u8]| SetValue { num, data, addr: [0; 6], obj: Obj::none() };
        HttpGet::set(&h, EE_WON_HTTPGET_MODE, &v(0, b""));
        assert_eq!(h.st().num(EE_WON_HTTPGET_ONLY_SEND, 0), 1);
        HttpGet::set(&h, EE_WON_HTTPGET_MODE, &v(2, b""));
        // one guard per read: two in one expression deadlock
        let send = h.st().num(EE_WON_HTTPGET_ONLY_SEND, 0);
        let recv = h.st().num(EE_WON_HTTPGET_ONLY_RECV, 0);
        assert_eq!((send, recv), (0, 1));
        HttpGet::set(&h, EE_WON_HTTPGET_LOCAL_PATH, &v(0, b"x.txt"));
        assert_eq!(h.st().bytes(EE_WON_HTTPGET_LOCAL_PATH, 0).unwrap(), b"x.txt");
        assert_eq!(h.st().bytes(EE_WON_HTTPGET_CONTENT, 0), None);
        assert_eq!(h.st().bytes(EE_WON_HTTPGET_REMOTE_PATH, 0).unwrap(), b"/p");
    }
}
