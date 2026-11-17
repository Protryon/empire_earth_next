//! `GetHTTPDocumentOp` (WONMisc/GetHTTPDocumentOp.cpp, HTTPDocumentOps.cpp) on the Rust op base (hostop.rs): the
//! WON system's and the game's message of the day, terms of use or advert, each an `HTTPGetOp` (httpget.rs, a
//! child op made through the host) into its cache file, retried once with the default document on a bad redirect
//! or a 404. The members are the op's (the document type, the language, the product's lower-case name, the HTTP
//! session, per owner the is-new flag, the retry, the download's status, HTTP status and modify time, the cache
//! file) and its steps: `RunHook`, `TryHTTPOp`, `OpFinished`, `CallbackHook`, `DoFinish`.
//!
//! The C++ side makes it with the WON directory (`RustServerOp::NewHttpDocument`); the cache file names, the
//! once-per-process path files `CheckHostAndPath` opened and ignored and the documents' read (`GetDocument`) are
//! here too. The readers (InitLogic, MOTDCtrl, TOUDialog, LobbyEvents, EEWonNet) read the members by number.
//!
//! Behaviour: `golden_hostop.txt`'s `D` sequences, recorded over the original's statements before the C++ class
//! was deleted. Reached by nothing in the game but an Internet login (the WON web servers are gone).
//!
//! The original's undefined corner, defined: `OpFinished` closed a MOTD file it failed to open (`fclose(NULL)`):
//! nothing is closed here.

use crate::hostop::{self, EeWonHostOp, Host, Obj, Plan, SetValue, FOREVER, ST_SUCCESS};
use crate::httpget::{
    EE_WON_HTTPGET_IS_NEW, EE_WON_HTTPGET_LOCAL_PATH, EE_WON_HTTPGET_RANGE_RESUME, EE_WON_HTTPGET_STATUS,
};

/// `HTTPDocType_MOTD`, `_TOU`, `_ADV`
pub const EE_WON_HTTPDOC_MOTD: i32 = 0;
pub const EE_WON_HTTPDOC_TOU: i32 = 1;
pub const EE_WON_HTTPDOC_ADV: i32 = 2;

// Setters (`ee_won_hostop_set`) and results (`ee_won_hostop_num` / `_bytes`, the element: the owner, 0 the system's,
// 1 the game's):
/// `GetDocType()` (number)
pub const EE_WON_HTTPDOC_TYPE: i32 = 1;
/// `SetExtraConfig` (bytes: the language, lower case as `TryHTTPOp` read it)
pub const EE_WON_HTTPDOC_CONFIG: i32 = 2;
/// `SetHTTPSession(session)` (the object)
pub const EE_WON_HTTPDOC_SESSION: i32 = 3;
/// `mIsNew[owner]` (number)
pub const EE_WON_HTTPDOC_IS_NEW: i32 = 4;
/// `GetDocumentStatus(owner)` (number)
pub const EE_WON_HTTPDOC_STATUS: i32 = 5;
/// `mHTTPStatus[owner]` (number)
pub const EE_WON_HTTPDOC_HTTP_STATUS: i32 = 6;
/// `GetModifiedTime(owner)` (number)
pub const EE_WON_HTTPDOC_MODIFIED: i32 = 7;
/// `GetDocumentPath(owner)` (bytes)
pub const EE_WON_HTTPDOC_PATH: i32 = 8;
/// `GetDocument(owner)`: the cache file's bytes as of now, NUL-ended (a MOTD without its flag character)
pub const EE_WON_HTTPDOC_DOCUMENT: i32 = 9;

/// `WS_GetMOTD_SysNotFound`, `WS_GetMOTD_GameNotFound`
const ST_SYS_NOT_FOUND: i32 = 100098;
const ST_GAME_NOT_FOUND: i32 = 100099;
/// `WS_HTTP_InvalidRedirect`
const ST_INVALID_REDIRECT: i32 = 100094;
/// `WS_None`
const ST_NONE: i32 = 100004;

/// The documents' host and base path (`GetHost()`, `GetPath()`)
const HOST: &[u8] = b"www.sierra.com";
const PATH: &[u8] = b"/motd/";

/// The op's members.
#[derive(Default)]
pub(crate) struct HttpDoc {
    doc_type: i32,
    /// `StringToLowerCase(mProduct)`
    product_lower: Vec<u8>,
    /// `StringToLowerCase(mPath)`
    config_lower: Vec<u8>,
    session: Obj,
    is_new: [bool; 2],
    retried: [bool; 2],
    status: [i32; 2],
    http_status: [i32; 2],
    last_modified: [i64; 2],
    documents: [Vec<u8>; 2],
}

type H = Host<HttpDoc>;

/// `DoFinish`
fn do_finish(h: &H) {
    let status = {
        let s = h.st();
        if s.status[0] != ST_SUCCESS && s.doc_type != EE_WON_HTTPDOC_ADV {
            ST_SYS_NOT_FOUND
        } else if s.status[1] != ST_SUCCESS {
            ST_GAME_NOT_FOUND
        } else {
            ST_SUCCESS
        }
    };
    h.finish(status);
}

fn child_num(h: &H, op: Obj, which: i32) -> i64 {
    h.call(hostop::EE_WON_HOST_HOSTOP_NUM, [op, Obj::none(), Obj::none()], [i64::from(which), 0], &[], [0; 6]).num
}

/// `OpFinished(op, owner)`: a download's result (a bad redirect or a 404 retried once); a MOTD's is-new flag
fn op_finished(h: &H, op: Obj, owner: usize) {
    let retried = h.st().retried[owner];
    if !retried && (h.op_status(op) == ST_INVALID_REDIRECT || child_num(h, op, EE_WON_HTTPGET_STATUS) == 404) {
        try_http_op(h, owner, true);
        return;
    }
    let status = h.op_status(op);
    h.st().status[owner] = status;
    let http_status = child_num(h, op, EE_WON_HTTPGET_STATUS) as i32;
    h.st().http_status[owner] = http_status;
    let modified = h.call(hostop::EE_WON_HOST_HTTP_MODIFIED, [op, Obj::none(), Obj::none()], [0, 0], &[], [0; 6]).num;
    h.st().last_modified[owner] = modified;
    if h.op_status(op) != ST_SUCCESS {
        return;
    }
    let is_new = child_num(h, op, EE_WON_HTTPGET_IS_NEW) != 0;
    let (doc_type, path) = {
        let mut s = h.st();
        s.is_new[owner] = is_new;
        (s.doc_type, s.documents[owner].clone())
    };
    if doc_type != EE_WON_HTTPDOC_MOTD {
        return;
    }
    // the first character of the cache file (-2: no file, -1: EOF)
    let first = h.call(hostop::EE_WON_HOST_FILE_FIRST_CHAR, [Obj::none(); 3], [0, 0], &path, [0; 6]).num;
    if first == -2 {
        return;
    }
    if first == -1 {
        h.st().is_new[owner] = false;
        return;
    }
    if !(first as u8).eq_ignore_ascii_case(&b'1') {
        h.st().is_new[owner] = true;
    }
}

/// `TryHTTPOp(owner, forceDefault)`: the owner's document, in the configured language or the default
fn try_http_op(h: &H, owner: usize, force_default: bool) {
    let (target, host_len, local, session) = {
        let mut s = h.st();
        let mut config = s.config_lower.clone();
        if config == b"en" {
            config.clear();
        }
        let base: &[u8] = match s.doc_type {
            EE_WON_HTTPDOC_MOTD => b"motd",
            EE_WON_HTTPDOC_TOU => b"tou",
            EE_WON_HTTPDOC_ADV => b"adv",
            _ => b"",
        };
        let mut document = Vec::new();
        if config.is_empty() || force_default {
            s.retried[owner] = true;
            if !base.is_empty() {
                document.extend_from_slice(base);
                document.extend_from_slice(b".txt");
            }
        } else if !base.is_empty() {
            document.extend_from_slice(base);
            document.push(b'_');
            document.extend_from_slice(&config);
            document.extend_from_slice(b".txt");
        }
        let mut target = HOST.to_vec();
        target.extend_from_slice(PATH);
        if owner == 0 {
            target.extend_from_slice(b"sys/");
        } else {
            target.extend_from_slice(&s.product_lower);
            target.push(b'/');
        }
        target.extend_from_slice(&document);
        (target, HOST.len(), s.documents[owner].clone(), s.session)
    };
    let op = h.call(hostop::EE_WON_HOST_NEW_HTTP_GET, [Obj::none(); 3], [host_len as i64, 0], &target, [0; 6]).obj;
    let set = |which: i32, num: i64, data: &[u8]| {
        h.call(hostop::EE_WON_HOST_HOSTOP_SET, [op, Obj::none(), Obj::none()], [i64::from(which), num], data, [0; 6]);
    };
    set(EE_WON_HTTPGET_RANGE_RESUME, 0, &[]);
    set(EE_WON_HTTPGET_LOCAL_PATH, 0, &local);
    if h.is_async() {
        h.track(op, owner as i32);
        if !session.is_none() {
            h.call(hostop::EE_WON_HOST_SESSION_ADD, [session, op, Obj::none()], [0, 0], &[], [0; 6]);
        } else {
            h.run_async(op, FOREVER);
        }
        h.release(op);
        return;
    }
    let wait = h.time_left();
    h.call(hostop::EE_WON_HOST_RUN, [op, Obj::none(), Obj::none()], [2, i64::from(wait)], &[], [0; 6]);
    op_finished(h, op, owner);
    h.release(op);
}

impl Plan for HttpDoc {
    fn run(h: &H) {
        let doc_type = {
            let mut s = h.st();
            s.is_new = [false; 2];
            s.status = [ST_NONE; 2];
            s.http_status = [0; 2];
            s.last_modified = [0; 2];
            s.retried = [false; 2];
            s.doc_type
        };
        if doc_type == EE_WON_HTTPDOC_ADV {
            try_http_op(h, 1, false);
        } else {
            for owner in 0..2 {
                try_http_op(h, owner, false);
            }
        }
        if !h.is_async() {
            do_finish(h);
        }
    }

    fn callback(h: &H, id: i32, op: Obj) -> bool {
        op_finished(h, op, (id & 1) as usize);
        let empty = h.call(hostop::EE_WON_HOST_TRACKER_EMPTY, [Obj::none(); 3], [0, 0], &[], [0; 6]).num != 0;
        if empty {
            do_finish(h);
        }
        true
    }

    fn cleanup(h: &H) {
        h.base_cleanup();
    }

    fn num(&self, which: i32, index: i64) -> i64 {
        let o = (index & 1) as usize;
        match which {
            EE_WON_HTTPDOC_TYPE => i64::from(self.doc_type),
            EE_WON_HTTPDOC_IS_NEW => i64::from(self.is_new[o]),
            EE_WON_HTTPDOC_STATUS => i64::from(self.status[o]),
            EE_WON_HTTPDOC_HTTP_STATUS => i64::from(self.http_status[o]),
            EE_WON_HTTPDOC_MODIFIED => self.last_modified[o],
            _ => 0,
        }
    }

    fn bytes(&self, which: i32, index: i64) -> Option<Vec<u8>> {
        let path = &self.documents[(index & 1) as usize];
        match which {
            EE_WON_HTTPDOC_PATH => Some(path.clone()),
            // GetDocument(owner): the cache file read now
            EE_WON_HTTPDOC_DOCUMENT => {
                let file = if path.is_empty() { None } else { crate::authlogin::read_file(path, c"r") };
                Some(document(self.doc_type, file))
            }
            _ => None,
        }
    }

    fn set(h: &H, which: i32, v: &SetValue) {
        match which {
            // StringToLowerCase(mPath) as TryHTTPOp read it
            EE_WON_HTTPDOC_CONFIG => h.st().config_lower = v.data.to_ascii_lowercase(),
            EE_WON_HTTPDOC_SESSION => {
                h.add_ref(v.obj);
                h.replace(|s| &mut s.session, v.obj);
            }
            _ => {}
        }
    }

    fn describe(&self) -> String {
        let x = hostop::hex;
        format!(
            "type {} product {} config {} session {} new {}{} retried {}{} st {} {} http {} {} mod {} {} docs {} {}",
            self.doc_type,
            x(&self.product_lower),
            x(&self.config_lower),
            self.session.0,
            u8::from(self.is_new[0]),
            u8::from(self.is_new[1]),
            u8::from(self.retried[0]),
            u8::from(self.retried[1]),
            self.status[0],
            self.status[1],
            self.http_status[0],
            self.http_status[1],
            self.last_modified[0],
            self.last_modified[1],
            x(&self.documents[0]),
            x(&self.documents[1])
        )
    }

    fn take_objects(&mut self) -> Vec<Obj> {
        let s = std::mem::take(&mut self.session);
        if s.is_none() {
            Vec::new()
        } else {
            vec![s]
        }
    }
}

/// `SetDocType`'s cache files of the system's and the game's copy in the WON directory `dir` (none for another
/// type)
pub fn documents(doc_type: i32, product: &[u8], dir: &[u8]) -> [Vec<u8>; 2] {
    let name: &[u8] = match doc_type {
        EE_WON_HTTPDOC_MOTD => b"motd",
        EE_WON_HTTPDOC_TOU => b"tou",
        EE_WON_HTTPDOC_ADV => b"adv",
        _ => return [Vec::new(), Vec::new()],
    };
    let make = |owner: &[u8]| [dir, b"_won", owner, name, b".txt"].concat();
    [make(b"sys"), make(product)]
}

/// `CheckHostAndPath`: once per process the type's `_won*path.txt` override file in the WON directory opened and
/// ignored (the original left it open)
fn check_host_and_path(doc_type: i32, dir: &[u8]) {
    static CHECKED: std::sync::atomic::AtomicBool = std::sync::atomic::AtomicBool::new(false);
    if CHECKED.swap(true, std::sync::atomic::Ordering::SeqCst) {
        return;
    }
    let name: &[u8] = match doc_type {
        EE_WON_HTTPDOC_MOTD => b"_wonmotdpath.txt",
        EE_WON_HTTPDOC_TOU => b"_wontoupath.txt",
        EE_WON_HTTPDOC_ADV => b"_wonadvpath.txt",
        _ => return,
    };
    let _ = crate::authlogin::read_file(&[dir, name].concat(), c"r");
}

/// `GetDocument(owner)`'s bytes from a file's (`None`: it did not open): a MOTD's leading flag character dropped
/// (kept when it is the '<' of HTML), then a NUL; a file that does not open gives the NUL alone
pub fn document(doc_type: i32, file: Option<Vec<u8>>) -> Vec<u8> {
    let Some(mut d) = file else {
        return vec![0];
    };
    if doc_type == EE_WON_HTTPDOC_MOTD && d.first().is_some_and(|&c| c != b'<') {
        d.remove(0);
    }
    d.push(0);
    d
}

/// `new GetHTTPDocumentOp(product, type)`: the document type, the product (its lower case for the paths) and the
/// cache files in the WON directory `dir` (`SetDocType`, `CheckHostAndPath`).
///
/// # Safety
/// The texts hold their lengths' bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_httpdoc_new(
    doc_type: i32,
    product: *const u8,
    product_len: usize,
    dir: *const u8,
    dir_len: usize,
) -> *mut EeWonHostOp {
    // SAFETY: the contract
    let (p, d) = unsafe { (crate::ffi::bytes(product, product_len), crate::ffi::bytes(dir, dir_len)) };
    check_host_and_path(doc_type, d);
    EeWonHostOp::boxed(HttpDoc {
        doc_type,
        product_lower: p.to_ascii_lowercase(),
        documents: documents(doc_type, p, d),
        ..Default::default()
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn op(doc_type: i32) -> H {
        Host::new(HttpDoc {
            doc_type,
            product_lower: b"eeaoc".to_vec(),
            documents: [b"_wonsysmotd.txt".to_vec(), b"_woneeaocmotd.txt".to_vec()],
            ..Default::default()
        })
    }

    #[test]
    fn the_finish_status_reads_both_owners() {
        // an unbound host: a finish does nothing; the status is the members'
        let h = op(EE_WON_HTTPDOC_MOTD);
        h.st().status = [ST_SUCCESS, ST_SUCCESS];
        do_finish(&h);
        let a = h.st();
        assert_eq!((a.status[0], a.status[1]), (0, 0));
        drop(a);
        let h = op(EE_WON_HTTPDOC_ADV);
        h.st().status = [ST_NONE, ST_SUCCESS];
        do_finish(&h);
    }

    #[test]
    fn results_and_setters_by_owner() {
        let h = op(EE_WON_HTTPDOC_TOU);
        let v = SetValue { num: 0, data: b"fr", addr: [0; 6], obj: Obj::none() };
        HttpDoc::set(&h, EE_WON_HTTPDOC_CONFIG, &v);
        {
            let mut s = h.st();
            s.status = [7, 9];
            s.last_modified = [11, 13];
            s.is_new = [false, true];
        }
        let s = h.st();
        assert_eq!(s.config_lower, b"fr");
        assert_eq!((s.num(EE_WON_HTTPDOC_STATUS, 0), s.num(EE_WON_HTTPDOC_STATUS, 1)), (7, 9));
        assert_eq!((s.num(EE_WON_HTTPDOC_MODIFIED, 1), s.num(EE_WON_HTTPDOC_IS_NEW, 1)), (13, 1));
        assert_eq!(s.num(EE_WON_HTTPDOC_TYPE, 0), i64::from(EE_WON_HTTPDOC_TOU));
        assert_eq!(s.bytes(EE_WON_HTTPDOC_PATH, 1).unwrap(), b"_woneeaocmotd.txt");
        assert_eq!(s.bytes(EE_WON_HTTPDOC_TYPE, 0), None);
    }
}

#[cfg(test)]
mod file_tests {
    use super::*;

    #[test]
    fn the_cache_files_are_named_by_owner_and_type() {
        let [a, b] = documents(EE_WON_HTTPDOC_TOU, b"EEAoC", b"C:/won/");
        assert_eq!((a.as_slice(), b.as_slice()), (&b"C:/won/_wonsystou.txt"[..], &b"C:/won/_wonEEAoCtou.txt"[..]));
        assert_eq!(documents(3, b"x", b"d/"), [Vec::new(), Vec::new()]);
    }

    #[test]
    fn a_motd_drops_its_flag_unless_it_is_html() {
        assert_eq!(document(EE_WON_HTTPDOC_MOTD, Some(b"1hello".to_vec())), b"hello\0");
        assert_eq!(document(EE_WON_HTTPDOC_MOTD, Some(b"<b>".to_vec())), b"<b>\0");
        assert_eq!(document(EE_WON_HTTPDOC_MOTD, Some(Vec::new())), b"\0");
        assert_eq!(document(EE_WON_HTTPDOC_TOU, Some(b"1x".to_vec())), b"1x\0");
        assert_eq!(document(EE_WON_HTTPDOC_ADV, None), b"\0");
    }
}
