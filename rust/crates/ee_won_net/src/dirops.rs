//! The directory server's ops (`src/Empire Earth/WONAPI/WONDir`) beyond the entity replies of [`crate::dir`]:
//! every request's bytes (list a directory, several directories, an alias, find, get a service, add, modify,
//! remove, rename, renew an entity, set, modify and clear its data objects), the status replies of the entity
//! changes (with a service's returned address), the multi-directory replies' accounting, `FindEntityOp`'s choice of
//! parser and `GetEntityRequest::GetDirEntityMap`'s grouping of a listing by directory.
//!
//! The entity changes are Rust ops (dirchange.rs); the C++ list, get and find ops keep their members and the
//! `DirEntity` objects, hand the fields over and copy the results back.
//! Reached by nothing in the game today: the directory servers are gone.

use std::ffi::c_void;

use crate::ffi::EeWonByteSink;
use crate::wire::{PastEnd, Reader, Writer};

/// Which request (`EeWonDirRequest::op`).
pub mod op {
    pub(crate) const GET_DIR: i32 = 0;
    pub(crate) const GET_MULTI_DIR: i32 = 1;
    pub(crate) const ALIAS_GET: i32 = 2;
    pub(crate) const GET_SERVICE: i32 = 3;
    pub(crate) const FIND: i32 = 4;
    pub(crate) const ADD: i32 = 5;
    pub(crate) const MODIFY: i32 = 6;
    pub(crate) const REMOVE: i32 = 7;
    pub(crate) const RENAME: i32 = 8;
    pub(crate) const RENEW: i32 = 9;
    pub(crate) const SET_DATA_OBJECTS: i32 = 10;
    pub(crate) const MODIFY_DATA_OBJECTS: i32 = 11;
    pub(crate) const CLEAR_DATA_OBJECTS: i32 = 12;
}

/// A data object: its type and its data (`None`: a NULL buffer, a zero length).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Object<'a> {
    pub data_type: &'a [u8],
    pub data: Option<&'a [u8]>,
}

/// A data object change (`DataObjectMod`).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ObjectMod<'a> {
    pub offset: u16,
    pub insert: bool,
    pub object: Object<'a>,
}

/// One directory's query (`GetEntityRequest`).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Query<'a> {
    pub flags: u32,
    pub path: Vec<u16>,
    pub types: Vec<&'a [u8]>,
}

/// A request's fields; each op reads its own.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Request<'a> {
    pub op: i32,
    pub is_service: bool,
    /// the entity flags (a byte on the wire), or the get flags (`GetDirOp`, `GetServiceOp`, `FindEntityOp`)
    pub flags: u32,
    pub path: Vec<u16>,
    pub name: Vec<u16>,
    pub display_name: Vec<u16>,
    pub net_addr: Option<&'a [u8]>,
    pub new_name: Vec<u16>,
    pub new_display_name: Vec<u16>,
    pub new_net_addr: Option<&'a [u8]>,
    pub lifespan: u32,
    pub is_unique: bool,
    /// the data object mode (set data objects) or the match mode (find)
    pub mode: u8,
    pub find_flags: u8,
    pub objects: Vec<Object<'a>>,
    pub mods: Vec<ObjectMod<'a>>,
    /// the data types (get service, clear data objects, find's types to get, get dir's query)
    pub types: Vec<&'a [u8]>,
    pub queries: Vec<Query<'a>>,
    pub product: &'a [u8],
}

/// `GetEntityRequest::Pack`: a query's flags (a directory-only query of services adds the type), the get dir's
/// short or the multi-get's 0x44, the path and the data types.
pub fn pack_query(w: &mut Writer, q: &Query, multi: bool) {
    let mut flags = q.flags;
    if flags & 2 != 0 && flags & 5 != 0 {
        flags |= 0x10;
    }
    w.append_long(flags as i32);
    if multi {
        w.append_byte(0x44);
    } else {
        w.append_short(0);
    }
    w.append_wstring(&q.path, 2);
    w.append_short(q.types.len() as i16);
    for t in &q.types {
        w.append_string(t, 1);
    }
}

fn head(w: &mut Writer, message: u16) {
    w.append_byte(5);
    w.append_short(2);
    w.append_short(message as i16);
}

fn objects(w: &mut Writer, list: &[Object]) {
    w.append_short(list.len() as i16);
    for o in list {
        w.append_string(o.data_type, 1);
        w.append_buffer(o.data, 2);
    }
}

fn types(w: &mut Writer, list: &[&[u8]]) {
    w.append_short(list.len() as i16);
    for t in list {
        w.append_string(t, 1);
    }
}

fn service_key(w: &mut Writer, r: &Request) {
    w.append_wstring(&r.path, 2);
    if r.is_service {
        w.append_wstring(&r.name, 2);
        w.append_buffer(r.net_addr, 1);
    }
}

/// A request's message behind the op's length field (`GetNextRequest` of each op).
pub fn request(r: &Request, field_size: u8) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    let svc = r.is_service;
    match r.op {
        op::GET_DIR => {
            head(&mut w, 103);
            let q = Query { flags: r.flags, path: r.path.clone(), types: r.types.clone() };
            pack_query(&mut w, &q, false);
        }
        op::GET_MULTI_DIR | op::ALIAS_GET => {
            if r.op == op::GET_MULTI_DIR {
                head(&mut w, 113);
                w.append_short(0);
            } else {
                head(&mut w, 114);
                w.append_short(0);
                w.append_string(r.product, 2);
            }
            w.append_byte(r.queries.len() as u8);
            for q in &r.queries {
                pack_query(&mut w, q, true);
            }
        }
        op::GET_SERVICE => {
            head(&mut w, 105);
            w.append_long(r.flags as i32);
            w.append_wstring(&r.path, 2);
            w.append_wstring(&r.name, 2);
            w.append_buffer(r.net_addr, 1);
            types(&mut w, &r.types);
        }
        op::FIND => {
            head(&mut w, if svc { 109 } else { 107 });
            w.append_long(r.flags as i32);
            w.append_byte(r.mode);
            w.append_byte(r.find_flags);
            w.append_short(0);
            w.append_wstring(&r.path, 2);
            w.append_wstring(&r.name, 2);
            if svc {
                w.append_buffer(r.net_addr, 1);
            }
            w.append_wstring(&r.display_name, 2);
            objects(&mut w, &r.objects);
            types(&mut w, &r.types);
        }
        op::ADD => {
            head(&mut w, if svc { 215 } else { 214 });
            w.append_byte(r.flags as u8);
            w.append_wstring(&r.path, 2);
            w.append_wstring(&r.name, 2);
            if svc {
                w.append_buffer(r.net_addr, 1);
            }
            w.append_wstring(&r.display_name, 2);
            w.append_long(r.lifespan as i32);
            objects(&mut w, &r.objects);
            w.append_short(0);
        }
        op::MODIFY => {
            head(&mut w, if svc { 217 } else { 216 });
            w.append_byte(r.flags as u8);
            service_key(&mut w, r);
            w.append_wstring(&r.new_name, 2);
            if svc {
                w.append_buffer(r.new_net_addr, 1);
            }
            w.append_wstring(&r.new_display_name, 2);
            w.append_long(r.lifespan as i32);
            objects(&mut w, &r.objects);
            w.append_short(0);
        }
        op::REMOVE => {
            head(&mut w, if svc { 209 } else { 208 });
            service_key(&mut w, r);
        }
        op::RENAME => {
            head(&mut w, if svc { 207 } else { 206 });
            w.append_bool(r.is_unique);
            service_key(&mut w, r);
            w.append_wstring(&r.new_display_name, 2);
        }
        op::RENEW => {
            head(&mut w, if svc { 205 } else { 204 });
            service_key(&mut w, r);
            w.append_long(r.lifespan as i32);
        }
        op::SET_DATA_OBJECTS => {
            head(&mut w, if svc { 301 } else { 300 });
            w.append_byte(r.mode);
            service_key(&mut w, r);
            objects(&mut w, &r.objects);
        }
        op::MODIFY_DATA_OBJECTS => {
            head(&mut w, if svc { 305 } else { 304 });
            service_key(&mut w, r);
            w.append_short(r.mods.len() as i16);
            for m in &r.mods {
                w.append_short(m.offset as i16);
                w.append_bool(m.insert);
                w.append_string(m.object.data_type, 1);
                w.append_buffer(m.object.data, 2);
            }
        }
        op::CLEAR_DATA_OBJECTS => {
            head(&mut w, if svc { 303 } else { 302 });
            service_key(&mut w, r);
            types(&mut w, &r.types);
        }
        _ => {}
    }
    w.to_message()
}

/// `AddServiceOp::SetNetAddrPort`: the port in network order.
pub fn port_net_addr(port: u16) -> [u8; 2] {
    port.to_be_bytes()
}

/// An entity change's status reply (`CheckResponse` of add, modify, remove, rename, renew and the data object
/// ops): the header 5, 2, 1, the status; with `read_addr` (a service added or modified with
/// `DIR_UF_SERVRETURNADDR`) and a status not negative, the returned address (`(offset, length)` in the reply).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct StatusReply {
    /// 0 read whole, 1 not a status reply (`InvalidReplyHeader`), 2 cut short (`ReadBufferException`)
    pub end: i32,
    pub status: i16,
    pub addr: Option<(usize, usize)>,
}

pub fn status_reply(reply: &[u8], read_addr: bool) -> StatusReply {
    let mut out = StatusReply::default();
    let mut r = Reader::new(reply);
    let res = (|| {
        let h = r.read_byte()?;
        let s = r.read_short()? as u16;
        let m = r.read_short()? as u16;
        if h != 5 || s != 2 || m != 1 {
            out.end = 1;
            return Ok(());
        }
        let status = r.read_short()?;
        if status >= 0 && read_addr {
            let n = r.read_byte()? as usize;
            let at = r.pos();
            r.read_bytes(n)?;
            out.addr = Some((at, n));
        }
        out.status = status;
        Ok::<(), PastEnd>(())
    })();
    if res.is_err() {
        out.end = 2;
    }
    out
}

/// `GetMultiDirOp` / `GetAliasDirOp::CheckResponse` after the current query parsed a reply with `status`: true
/// when the query is done (it moves to the next; a status other than success counts a failure).
pub fn multi_reply_step(status: i32, failed: &mut i32) -> bool {
    if status == crate::dir::MORE_REPLIES {
        return false;
    }
    if status != 0 {
        *failed += 1;
    }
    true
}

/// The multi-directory ops' final status: success, all failed (-1224) or some failed (-1223).
pub fn multi_reply_end(failed: i32, count: usize) -> i32 {
    if failed == 0 {
        0
    } else if failed == count as i32 {
        -1224
    } else {
        -1223
    }
}

/// `FindEntityOp::CheckResponse`'s parser: a single-entity reply for a service search without
/// `DIR_FF_MATCHALL`, else a multi-entity reply.
pub fn find_reads_single(is_service: bool, find_flags: u8) -> bool {
    is_service && find_flags & 1 == 0
}

/// An entity of a listing for [`group_keys`]: its type, path and name.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Listed {
    pub kind: u8,
    pub path: Vec<u16>,
    pub name: Vec<u16>,
}

/// `GetEntityRequest::GetDirEntityMap`: the directory each entity of a listing belongs to. A directory entry
/// starts its own (its path, or the query's, a slash, its name); an entity with another non-empty path starts that
/// one; the rest stay in the current one (first the query's path, which always has an entry).
pub fn group_keys(query_path: &[u16], entities: &[Listed]) -> Vec<Vec<u16>> {
    let mut current: Vec<u16> = query_path.to_vec();
    let mut key = current.clone();
    let mut out = Vec::with_capacity(entities.len());
    for e in entities {
        if e.kind == b'D' {
            current = if e.path.is_empty() { query_path.to_vec() } else { e.path.clone() };
            if current.last() != Some(&u16::from(b'/')) {
                current.push(u16::from(b'/'));
            }
            current.extend_from_slice(&e.name);
            key = current.clone();
        } else if !e.path.is_empty() && e.path != current {
            current = e.path.clone();
            key = current.clone();
        }
        out.push(key.clone());
    }
    out
}

// ---- the C ABI

/// Bytes handed over by the C++ (`present` false: a NULL buffer).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonDirBytes {
    pub data: *const u8,
    pub len: usize,
    pub present: bool,
}

/// A data object.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonDirObject {
    pub data_type: EeWonDirBytes,
    pub data: EeWonDirBytes,
}

/// A data object change.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonDirMod {
    pub offset: u16,
    pub insert: bool,
    pub object: EeWonDirObject,
}

/// A query: the path as UTF-16 LE bytes.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonDirQuery {
    pub flags: u32,
    pub path: EeWonDirBytes,
    pub types: *const EeWonDirBytes,
    pub type_count: usize,
}

/// A request's fields (strings as UTF-16 LE bytes, the arrays as pointer and count).
#[repr(C)]
pub struct EeWonDirRequest {
    pub op: i32,
    pub is_service: bool,
    pub flags: u32,
    pub path: EeWonDirBytes,
    pub name: EeWonDirBytes,
    pub display_name: EeWonDirBytes,
    pub net_addr: EeWonDirBytes,
    pub new_name: EeWonDirBytes,
    pub new_display_name: EeWonDirBytes,
    pub new_net_addr: EeWonDirBytes,
    pub lifespan: u32,
    pub is_unique: bool,
    pub mode: u8,
    pub find_flags: u8,
    pub objects: *const EeWonDirObject,
    pub object_count: usize,
    pub mods: *const EeWonDirMod,
    pub mod_count: usize,
    pub types: *const EeWonDirBytes,
    pub type_count: usize,
    pub queries: *const EeWonDirQuery,
    pub query_count: usize,
    pub product: EeWonDirBytes,
}

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the C++ hands `n` entries
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

unsafe fn bytes<'a>(b: &EeWonDirBytes) -> &'a [u8] {
    unsafe { slice(b.data, b.len) }
}

unsafe fn buffer<'a>(b: &EeWonDirBytes) -> Option<&'a [u8]> {
    if b.present {
        Some(unsafe { bytes(b) })
    } else {
        None
    }
}

unsafe fn units(b: &EeWonDirBytes) -> Vec<u16> {
    crate::text::utf16_from_le_bytes(unsafe { bytes(b) })
}

unsafe fn object<'a>(o: &EeWonDirObject) -> Object<'a> {
    unsafe { Object { data_type: bytes(&o.data_type), data: buffer(&o.data) } }
}

unsafe fn type_list<'a>(p: *const EeWonDirBytes, n: usize) -> Vec<&'a [u8]> {
    unsafe { slice(p, n).iter().map(|b| bytes(b)).collect() }
}

/// A request's message into `sink`.
///
/// # Safety
/// `r` and everything it points to are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_request(
    r: *const EeWonDirRequest,
    field_size: u8,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let req = unsafe { owned_request(&*r) };
    let m = request(&req, field_size);
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// A request's fields from the C++ (the arrays and bytes borrowed, the texts decoded).
///
/// # Safety
/// `r` and everything it points to are valid while the result lives.
pub(crate) unsafe fn owned_request<'a>(r: &EeWonDirRequest) -> Request<'a> {
    unsafe {
        Request {
            op: r.op,
            is_service: r.is_service,
            flags: r.flags,
            path: units(&r.path),
            name: units(&r.name),
            display_name: units(&r.display_name),
            net_addr: buffer(&r.net_addr),
            new_name: units(&r.new_name),
            new_display_name: units(&r.new_display_name),
            new_net_addr: buffer(&r.new_net_addr),
            lifespan: r.lifespan,
            is_unique: r.is_unique,
            mode: r.mode,
            find_flags: r.find_flags,
            objects: slice(r.objects, r.object_count).iter().map(|o| object(o)).collect(),
            mods: slice(r.mods, r.mod_count)
                .iter()
                .map(|m| ObjectMod { offset: m.offset, insert: m.insert, object: object(&m.object) })
                .collect(),
            types: type_list(r.types, r.type_count),
            queries: slice(r.queries, r.query_count)
                .iter()
                .map(|q| Query { flags: q.flags, path: units(&q.path), types: type_list(q.types, q.type_count) })
                .collect(),
            product: bytes(&r.product),
        }
    }
}

/// `GetEntityRequest::Pack`'s bytes into `sink` (`multi`: the multi-get's form).
///
/// # Safety
/// `q` and what it points to are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_pack_query(
    q: *const EeWonDirQuery,
    multi: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let q = unsafe { &*q };
    let query = unsafe { Query { flags: q.flags, path: units(&q.path), types: type_list(q.types, q.type_count) } };
    let mut w = Writer::new(0, true);
    pack_query(&mut w, &query, multi);
    unsafe { sink(context, w.data().as_ptr(), w.data().len()) }
}

/// `AddServiceOp::SetNetAddrPort`'s two bytes into `sink`.
///
/// # Safety
/// `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_port_net_addr(port: u16, sink: EeWonByteSink, context: *mut c_void) {
    let b = port_net_addr(port);
    unsafe { sink(context, b.as_ptr(), b.len()) }
}

/// A status reply for the C++.
#[repr(C)]
pub struct EeWonDirStatusReply {
    /// 0 read whole, 1 not a status reply, 2 cut short
    pub end: i32,
    pub status: i16,
    /// a returned address: its offset and length in the reply
    pub has_addr: bool,
    pub addr_at: u32,
    pub addr_len: u32,
}

/// An entity change's status reply.
///
/// # Safety
/// `reply` holds `length` bytes; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_status_reply(
    reply: *const u8,
    length: usize,
    read_addr: bool,
    out: *mut EeWonDirStatusReply,
) {
    let s = status_reply(unsafe { slice(reply, length) }, read_addr);
    let (at, len) = s.addr.unwrap_or((0, 0));
    unsafe {
        *out = EeWonDirStatusReply {
            end: s.end,
            status: s.status,
            has_addr: s.addr.is_some(),
            addr_at: at as u32,
            addr_len: len as u32,
        }
    }
}

/// The multi-directory ops' step: true when the current query is done (`*failed` counts a failure).
///
/// # Safety
/// `failed` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_multi_step(status: i32, failed: *mut i32) -> bool {
    unsafe { multi_reply_step(status, &mut *failed) }
}

/// The multi-directory ops' final status.
#[no_mangle]
pub extern "C" fn ee_won_dir_multi_end(failed: i32, count: usize) -> i32 {
    multi_reply_end(failed, count)
}

/// `FindEntityOp::CheckResponse`'s parser choice (true: single-entity).
#[no_mangle]
pub extern "C" fn ee_won_dir_find_single(is_service: bool, find_flags: u8) -> bool {
    find_reads_single(is_service, find_flags)
}

/// An entity for [`ee_won_dir_group_keys`]: its type, path and name as UTF-16 LE bytes.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonDirListed {
    pub kind: u8,
    pub path: EeWonDirBytes,
    pub name: EeWonDirBytes,
}

/// `GetEntityRequest::GetDirEntityMap`'s keys: for each of `count` entities its directory into `sink` (UTF-16
/// LE bytes, one call per entity, in order).
///
/// # Safety
/// `query_path`, `entities` (`count` entries) are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_group_keys(
    query_path: EeWonDirBytes,
    entities: *const EeWonDirListed,
    count: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let path = unsafe { units(&query_path) };
    let list: Vec<Listed> = unsafe { slice(entities, count) }
        .iter()
        .map(|e| unsafe { Listed { kind: e.kind, path: units(&e.path), name: units(&e.name) } })
        .collect();
    for k in group_keys(&path, &list) {
        let b = crate::text::utf16_to_le_bytes(&k);
        unsafe { sink(context, b.as_ptr(), b.len()) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(s: &str) -> Vec<u16> {
        s.encode_utf16().collect()
    }

    #[test]
    fn query_and_requests() {
        let q = Query { flags: 0x2 | 0x1, path: w("/a"), types: vec![b"t"] };
        let mut wr = Writer::new(0, true);
        pack_query(&mut wr, &q, true);
        assert_eq!(wr.data(), &[0x13, 0, 0, 0, 0x44, 2, 0, b'/', 0, b'a', 0, 1, 0, 1, b't']);
        let r = Request { op: op::REMOVE, is_service: true, path: w("p"), name: w("n"), ..Default::default() };
        assert_eq!(request(&r, 0), vec![5, 2, 0, 209, 0, 1, 0, b'p', 0, 1, 0, b'n', 0, 0]);
        let r = Request { op: op::RENEW, lifespan: 0x0102_0304, path: w(""), ..Default::default() };
        assert_eq!(request(&r, 2), vec![13, 0, 5, 2, 0, 204, 0, 0, 0, 4, 3, 2, 1]);
        assert_eq!(port_net_addr(0x3a99), [0x3a, 0x99]);
    }

    #[test]
    fn status_replies() {
        assert_eq!(status_reply(&[5, 2, 0, 1, 0, 0xfe, 0xff], true).status, -2);
        let s = status_reply(&[5, 2, 0, 1, 0, 0, 0, 2, 9, 8], true);
        assert_eq!((s.end, s.status, s.addr), (0, 0, Some((8, 2))));
        assert_eq!(status_reply(&[5, 2, 0, 1, 0, 0, 0, 2, 9], true).end, 2);
        assert_eq!(status_reply(&[5, 2, 0, 1, 0, 0, 0], false).end, 0);
        assert_eq!(status_reply(&[5, 2, 0, 2, 0, 0, 0], false).end, 1);
        assert_eq!(status_reply(&[5, 2], false).end, 2);
    }

    #[test]
    fn multi_and_groups() {
        let mut failed = 0;
        assert!(!multi_reply_step(crate::dir::MORE_REPLIES, &mut failed));
        assert!(multi_reply_step(-1201, &mut failed) && failed == 1);
        assert!(multi_reply_step(0, &mut failed) && failed == 1);
        assert_eq!((multi_reply_end(0, 2), multi_reply_end(1, 2), multi_reply_end(2, 2)), (0, -1223, -1224));
        assert!(find_reads_single(true, 0) && !find_reads_single(true, 1) && !find_reads_single(false, 0));
        let e = |k: u8, p: &str, n: &str| Listed { kind: k, path: w(p), name: w(n) };
        let keys = group_keys(
            &w("/EE"),
            &[e(b'S', "", "s1"), e(b'D', "", "sub"), e(b'S', "", "s2"), e(b'S', "/x/", "s3"), e(b'D', "/y/", "z")],
        );
        let want: Vec<Vec<u16>> = ["/EE", "/EE/sub", "/EE/sub", "/x/", "/y/z"].iter().map(|s| w(s)).collect();
        assert_eq!(keys, want);
    }
}
