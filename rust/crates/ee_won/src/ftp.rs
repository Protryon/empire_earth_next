//! WONMisc's FTP (`src/Empire Earth/WONAPI/WONMisc/FTPGetOp.cpp`): the URL's host, user, password and path, the
//! remote path's directory and file, the reply's status and its rules, the commands, the passive reply's address
//! and the reply's step through the login, directory, type, passive, restart and retrieve states.
//!
//! `FTPGetOp` stays a C++ object (an `AsyncOp` over two C++ `BlockingSocket`s); it hands its lines and members
//! over and acts on the step. Reached by nothing in the game (the WON patch servers are gone).

use std::ffi::c_void;

use crate::ffi::EeWonByteSink;
use crate::http::{c_str, scan_int};

fn find(s: &[u8], what: &[u8]) -> Option<usize> {
    s.windows(what.len()).position(|w| w == what)
}

/// The URL's parts (ranges in the URL; `None`: not set).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct UrlParts {
    pub user: Option<(usize, usize)>,
    pub password: Option<(usize, usize)>,
    pub host: (usize, usize),
    /// the remote path (after the host's '/')
    pub path: Option<(usize, usize)>,
}

/// `FTPGetOp(url, extract)`: the scheme dropped, then (when asked) a user name before the first ':' and a
/// password before the last '@' up to the first '/' of the rest, then the host up to the next '/'.
pub fn url(s: &[u8], extract: bool) -> UrlParts {
    let mut p = UrlParts::default();
    let mut at = find(s, b"://").map_or(0, |n| n + 3);
    if extract {
        let (user, password, rest) = credentials(s, at);
        p.user = user;
        p.password = password;
        at = rest;
    }
    let rest = &s[at..];
    match rest.iter().position(|&c| c == b'/') {
        None => p.host = (at, s.len()),
        Some(n) => {
            p.host = (at, at + n);
            p.path = Some((at + n + 1, s.len()));
        }
    }
    p
}

/// `ExtractUserNameAndPasswordFromURL` on `s[at..]`: a user name before the first ':', then a password before the
/// last '@' up to the first '/' of the rest; the user's and password's ranges and where the rest starts.
#[allow(clippy::type_complexity)]
pub fn credentials(s: &[u8], mut at: usize) -> (Option<(usize, usize)>, Option<(usize, usize)>, usize) {
    let mut user = None;
    let mut password = None;
    if let Some(n) = s[at..].iter().position(|&c| c == b':') {
        user = Some((at, at + n));
        at += n + 1;
    }
    let rest = &s[at..];
    // rfind('@', slash): up to and including the slash's position (all of it without one)
    let limit = rest.iter().position(|&c| c == b'/').map_or(rest.len(), |n| n + 1);
    if let Some(n) = rest[..limit].iter().rposition(|&c| c == b'@') {
        password = Some((at, at + n));
        at += n + 1;
    }
    (user, password, at)
}

/// `SetRemotePath`: the directory before the last '/' and the file after it (no '/': no directory).
pub fn remote_path(path: &[u8]) -> (&[u8], &[u8]) {
    match path.iter().rposition(|&c| c == b'/') {
        None => (&[], path),
        Some(n) => (&path[..n], &path[n + 1..]),
    }
}

/// `WS_FTP_InvalidResponse`, `WS_FTP_StatusError`, `WS_FTP_InvalidPasvResponse`.
pub const EE_WON_FTP_INVALID_RESPONSE: i32 = 100089;
pub const EE_WON_FTP_STATUS_ERROR: i32 = 100090;
pub const EE_WON_FTP_INVALID_PASV: i32 = 100091;

/// `ExtractStatus`: the status scanned (`None`: the op finishes with `WS_FTP_InvalidResponse`) and whether it
/// is one the transfer goes on with (2xx, 331, 125, 150, 350, 504; else `WS_FTP_StatusError`).
pub fn status(line: &[u8]) -> Option<(i32, bool)> {
    let (v, _) = scan_int(c_str(line), 0)?;
    Some((v, (200..300).contains(&v) || matches!(v, 331 | 125 | 150 | 350 | 504)))
}

/// The request states (`RequestState`).
pub const EE_WON_FTP_START: i32 = 0;
pub const EE_WON_FTP_USER: i32 = 1;
pub const EE_WON_FTP_PASSWORD: i32 = 2;
pub const EE_WON_FTP_CHANGE_DIR: i32 = 3;
pub const EE_WON_FTP_CHANGE_TYPE: i32 = 4;
pub const EE_WON_FTP_PASSIVE: i32 = 5;
pub const EE_WON_FTP_RESTART: i32 = 6;
pub const EE_WON_FTP_RETRIEVE: i32 = 7;

/// What `SendRequest` sends in a state.
pub struct Command<'a> {
    pub user: &'a [u8],
    pub password: &'a [u8],
    pub directory: &'a [u8],
    pub file: &'a [u8],
    pub binary: bool,
    pub local_size: u32,
}

/// `SendRequest`'s line (CR LF ended; an unknown state sends only the CR LF).
pub fn command(state: i32, c: &Command) -> Vec<u8> {
    let mut r = Vec::new();
    match state {
        EE_WON_FTP_USER => {
            r.extend_from_slice(b"USER ");
            r.extend_from_slice(c.user);
        }
        EE_WON_FTP_PASSWORD => {
            r.extend_from_slice(b"PASS ");
            r.extend_from_slice(c.password);
        }
        EE_WON_FTP_CHANGE_DIR => {
            r.extend_from_slice(b"CWD ");
            r.extend_from_slice(c.directory);
        }
        EE_WON_FTP_CHANGE_TYPE => r.extend_from_slice(if c.binary { b"TYPE I" } else { b"TYPE A" }),
        EE_WON_FTP_PASSIVE => r.extend_from_slice(b"PASV"),
        // sprintf("REST %d") of the DWORD size
        EE_WON_FTP_RESTART => r.extend_from_slice(format!("REST {}", c.local_size as i32).as_bytes()),
        EE_WON_FTP_RETRIEVE => {
            r.extend_from_slice(b"RETR ");
            r.extend_from_slice(c.file);
        }
        _ => {}
    }
    r.extend_from_slice(b"\r\n");
    r
}

/// `ParsePasv`: `sscanf(from '(', "(%d,%d,%d,%d,%d,%d)")`: the host (the four numbers' bytes, high first) and
/// the port; `None` when there is no '(' or fewer than six numbers.
pub fn pasv(line: &[u8]) -> Option<(u32, u16)> {
    let s = c_str(line);
    let mut i = s.iter().position(|&c| c == b'(')? + 1;
    let mut n = [0i32; 6];
    for (k, v) in n.iter_mut().enumerate() {
        if k > 0 {
            if s.get(i) != Some(&b',') {
                return None;
            }
            i += 1;
        }
        let (x, end) = scan_int(s, i)?;
        *v = x;
        i = end;
    }
    let host =
        (n[0] as u32).wrapping_shl(24) | (n[1] as u32).wrapping_shl(16) | (n[2] as u32).wrapping_shl(8) | n[3] as u32;
    let port = ((n[4] as u32).wrapping_shl(8) | n[5] as u32) as u16;
    Some((host, port))
}

/// What `HandleReply` does with a line.
pub const EE_WON_FTP_MORE: i32 = 0;
/// finish with `status`
pub const EE_WON_FTP_FINISH: i32 = 1;
/// send the command of `state`
pub const EE_WON_FTP_SEND: i32 = 2;
/// the passive reply: the data address (`host`, `port`), then the data connection, then the command of `state`
pub const EE_WON_FTP_DATA: i32 = 3;
/// receive the content
pub const EE_WON_FTP_RECV: i32 = 4;
/// return false (an unknown state)
pub const EE_WON_FTP_NONE: i32 = 5;

/// A reply's step.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonFtpStep {
    pub action: i32,
    /// the status scanned (assigned to `mFTPStatus` when `scanned`)
    pub scanned: bool,
    pub ftp_status: i32,
    pub status: i32,
    /// the new request state (`SEND`, `DATA`)
    pub state: i32,
    /// `mDoResume` turned off (a 504 to the restart)
    pub no_resume: bool,
    pub host: u32,
    pub port: u16,
}

/// `HandleReply` on a line (the reply op succeeded): its step in `state`, with a directory set or not and a
/// resume asked or not.
pub fn reply(line: &[u8], state: i32, has_directory: bool, resume: bool) -> EeWonFtpStep {
    let mut s = EeWonFtpStep { state, ..Default::default() };
    if line.len() >= 4 && line[3] == b'-' {
        s.action = EE_WON_FTP_MORE;
        return s;
    }
    let Some((v, ok)) = status(line) else {
        s.action = EE_WON_FTP_FINISH;
        s.status = EE_WON_FTP_INVALID_RESPONSE;
        return s;
    };
    s.scanned = true;
    s.ftp_status = v;
    if !ok {
        s.action = EE_WON_FTP_FINISH;
        s.status = EE_WON_FTP_STATUS_ERROR;
        return s;
    }
    let send = |mut s: EeWonFtpStep, state: i32| {
        s.action = EE_WON_FTP_SEND;
        s.state = state;
        s
    };
    match state {
        EE_WON_FTP_START => send(s, EE_WON_FTP_USER),
        EE_WON_FTP_USER | EE_WON_FTP_PASSWORD | EE_WON_FTP_CHANGE_DIR => {
            if state == EE_WON_FTP_USER && v == 331 {
                send(s, EE_WON_FTP_PASSWORD)
            } else if state != EE_WON_FTP_CHANGE_DIR && has_directory {
                send(s, EE_WON_FTP_CHANGE_DIR)
            } else {
                send(s, EE_WON_FTP_CHANGE_TYPE)
            }
        }
        EE_WON_FTP_CHANGE_TYPE => send(s, EE_WON_FTP_PASSIVE),
        EE_WON_FTP_PASSIVE => match pasv(line) {
            None => {
                s.action = EE_WON_FTP_FINISH;
                s.status = EE_WON_FTP_INVALID_PASV;
                s
            }
            Some((host, port)) => {
                s.action = EE_WON_FTP_DATA;
                s.state = if resume { EE_WON_FTP_RESTART } else { EE_WON_FTP_RETRIEVE };
                s.host = host;
                s.port = port;
                s
            }
        },
        EE_WON_FTP_RESTART => {
            s.no_resume = v == 504;
            send(s, EE_WON_FTP_RETRIEVE)
        }
        EE_WON_FTP_RETRIEVE => {
            s.action = EE_WON_FTP_RECV;
            s
        }
        _ => {
            s.action = EE_WON_FTP_NONE;
            s
        }
    }
}

// ---- the C ABI

unsafe fn slice<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// The URL's ranges: [user, password, host, path] as (start, end), -1 for one not set.
///
/// # Safety
/// `text` holds `len` bytes; `out` holds 8 values.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftp_url(text: *const u8, len: usize, extract: bool, out: *mut isize) {
    let p = url(unsafe { slice(text, len) }, extract);
    let out = unsafe { std::slice::from_raw_parts_mut(out, 8) };
    let mut put = |k: usize, r: Option<(usize, usize)>| {
        let (a, b) = r.map_or((-1, -1), |(a, b)| (a as isize, b as isize));
        out[2 * k] = a;
        out[2 * k + 1] = b;
    };
    put(0, p.user);
    put(1, p.password);
    put(2, Some(p.host));
    put(3, p.path);
}

/// `ExtractUserNameAndPasswordFromURL`'s ranges: [user, password] as (start, end), -1 for one not set, then
/// the rest's start.
///
/// # Safety
/// `text` holds `len` bytes; `out` holds 5 values.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftp_credentials(text: *const u8, len: usize, out: *mut isize) {
    let (u, p, rest) = credentials(unsafe { slice(text, len) }, 0);
    let out = unsafe { std::slice::from_raw_parts_mut(out, 5) };
    let r = |x: Option<(usize, usize)>| x.map_or((-1, -1), |(a, b)| (a as isize, b as isize));
    (out[0], out[1]) = r(u);
    (out[2], out[3]) = r(p);
    out[4] = rest as isize;
}

/// `ExtractStatus` of a C string: 0 when no number was scanned, 1 for a status the transfer goes on with, 2 for
/// another (`*out` the status for 1 and 2).
///
/// # Safety
/// `line` is a NUL-terminated string; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftp_status(line: *const u8, out: *mut i32) -> i32 {
    let s = unsafe { std::ffi::CStr::from_ptr(line.cast()) }.to_bytes();
    match status(s) {
        None => 0,
        Some((v, ok)) => {
            unsafe { *out = v };
            if ok {
                1
            } else {
                2
            }
        }
    }
}

/// `ParsePasv` of a C string: false when malformed, else the host and port.
///
/// # Safety
/// `line` is a NUL-terminated string; the outputs are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftp_pasv(line: *const u8, host: *mut u32, port: *mut u16) -> bool {
    let s = unsafe { std::ffi::CStr::from_ptr(line.cast()) }.to_bytes();
    match pasv(s) {
        None => false,
        Some((h, p)) => {
            unsafe {
                *host = h;
                *port = p;
            }
            true
        }
    }
}

/// `SetRemotePath`'s split: the directory's end (-1: none, the file is the whole path).
///
/// # Safety
/// `path` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftp_remote_split(path: *const u8, len: usize) -> isize {
    let p = unsafe { slice(path, len) };
    let (d, f) = remote_path(p);
    if d.is_empty() && f.len() == p.len() {
        -1
    } else {
        d.len() as isize
    }
}

/// `SendRequest`'s line of `state` into `sink`.
///
/// # Safety
/// The byte ranges are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftp_command(
    state: i32,
    user: *const u8,
    user_len: usize,
    password: *const u8,
    password_len: usize,
    directory: *const u8,
    directory_len: usize,
    file: *const u8,
    file_len: usize,
    binary: bool,
    local_size: u32,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let c = unsafe {
        Command {
            user: slice(user, user_len),
            password: slice(password, password_len),
            directory: slice(directory, directory_len),
            file: slice(file, file_len),
            binary,
            local_size,
        }
    };
    let r = command(state, &c);
    unsafe { sink(context, r.as_ptr(), r.len()) }
}

/// `HandleReply`'s step for a line of `len` bytes (its C string read up to a NUL).
///
/// # Safety
/// `line` holds `len` bytes; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ftp_reply(
    line: *const u8,
    len: usize,
    state: i32,
    has_directory: bool,
    resume: bool,
    out: *mut EeWonFtpStep,
) {
    unsafe { *out = reply(slice(line, len), state, has_directory, resume) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn urls() {
        let s = b"ftp://bob:pw@host/dir/file";
        let p = url(s, true);
        assert_eq!(p.user, Some((6, 9)));
        assert_eq!(p.password, Some((10, 12)));
        assert_eq!(p.host, (13, 17));
        assert_eq!(p.path, Some((18, 26)));
        let p = url(b"ftp://bob:pw@host/dir/file", false);
        assert_eq!((p.user, p.host), (None, (6, 17)));
        let p = url(b"host", true);
        assert_eq!((p.user, p.password, p.host, p.path), (None, None, (0, 4), None));
        // the '@' after the first '/' is not the password's
        assert_eq!(url(b"h/a@b", true).password, None);
        assert_eq!(url(b"a@/x", true).password, Some((0, 1)));
        assert_eq!(remote_path(b"a/b/c"), (&b"a/b"[..], &b"c"[..]));
        assert_eq!(remote_path(b"c"), (&b""[..], &b"c"[..]));
    }

    #[test]
    fn statuses_and_commands() {
        assert_eq!(status(b"220 hi"), Some((220, true)));
        assert_eq!(status(b"530 no"), Some((530, false)));
        assert_eq!(status(b"331"), Some((331, true)));
        assert_eq!(status(b"x"), None);
        let c =
            Command { user: b"u", password: b"p", directory: b"d", file: b"f", binary: false, local_size: 0xffffffff };
        assert_eq!(command(EE_WON_FTP_USER, &c), b"USER u\r\n");
        assert_eq!(command(EE_WON_FTP_CHANGE_TYPE, &c), b"TYPE A\r\n");
        assert_eq!(command(EE_WON_FTP_RESTART, &c), b"REST -1\r\n");
        assert_eq!(command(EE_WON_FTP_START, &c), b"\r\n");
        assert_eq!(pasv(b"227 Entering (10,0,0,1,4,1)."), Some((0x0a000001, 0x401)));
        assert_eq!(pasv(b"227 (1,2,3,4,5)"), None);
        assert_eq!(pasv(b"227 ( 1, 2,3,4,5,6"), Some((0x01020304, 0x506)));
        assert_eq!(pasv(b"227 (1 ,2,3,4,5,6)"), None);
        assert_eq!(pasv(b"227"), None);
    }

    #[test]
    fn steps() {
        assert_eq!(reply(b"220-more", 0, false, false).action, EE_WON_FTP_MORE);
        let s = reply(b"331 pw", EE_WON_FTP_USER, true, false);
        assert_eq!((s.action, s.state), (EE_WON_FTP_SEND, EE_WON_FTP_PASSWORD));
        let s = reply(b"230 ok", EE_WON_FTP_USER, true, false);
        assert_eq!(s.state, EE_WON_FTP_CHANGE_DIR);
        let s = reply(b"230 ok", EE_WON_FTP_PASSWORD, false, false);
        assert_eq!(s.state, EE_WON_FTP_CHANGE_TYPE);
        let s = reply(b"250 ok", EE_WON_FTP_CHANGE_DIR, true, false);
        assert_eq!(s.state, EE_WON_FTP_CHANGE_TYPE);
        let s = reply(b"227 (1,2,3,4,0,21)", EE_WON_FTP_PASSIVE, true, true);
        assert_eq!((s.action, s.state, s.port), (EE_WON_FTP_DATA, EE_WON_FTP_RESTART, 21));
        let s = reply(b"504 no", EE_WON_FTP_RESTART, true, true);
        assert!(s.no_resume);
        assert_eq!(reply(b"150 go", EE_WON_FTP_RETRIEVE, true, true).action, EE_WON_FTP_RECV);
        let s = reply(b"550 no", EE_WON_FTP_RETRIEVE, true, true);
        assert_eq!((s.action, s.status, s.scanned), (EE_WON_FTP_FINISH, EE_WON_FTP_STATUS_ERROR, true));
        assert_eq!(reply(b"x", 0, false, false).status, EE_WON_FTP_INVALID_RESPONSE);
        assert_eq!(reply(b"200", 9, false, false).action, EE_WON_FTP_NONE);
    }
}
