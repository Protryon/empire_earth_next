//! The directory server's entity replies (WONDir/DirEntityReplyParser.cpp): a multi-entity reply (one reply of a
//! directory listing, the entities with the fields its get flags include, services and directories each with their
//! own fields) and a single-entity reply (a service found). The C++ makes the `DirEntity` objects from what is read
//! here; a reply cut short keeps the entities read whole before the cut (the C++ then throws
//! `ReadBufferException` as `ReadBuffer` did).
//!
//! Reached by nothing in the game today: the directory servers are gone (the internet login's first request).

use std::ffi::c_void;

use crate::wire::{PastEnd, Reader};

/// The get flags' bits the parsers read (`DIR_GF_*`).
pub mod flags {
    pub const DIRS_ONLY_MASK: u32 = 0x2;
    pub const SERVICES_ONLY_MASK: u32 = 0x5;
    pub const ADD_TYPE: u32 = 0x10;
    pub const ADD_DISPLAY_NAME: u32 = 0x20;
    pub const ADD_CREATED: u32 = 0x40;
    pub const ADD_TOUCHED: u32 = 0x80;
    pub const ADD_LIFESPAN: u32 = 0x100;
    pub const ADD_DO_TYPE: u32 = 0x200;
    pub const ADD_DO_DATA: u32 = 0x400;
    pub const ADD_DATA_OBJECTS: u32 = 0x800;
    pub const ADD_ACLS: u32 = 0x1000;
    pub const ADD_CRC: u32 = 0x2000;
    pub const ADD_UIDS: u32 = 0x4000;
    pub const ADD_IPS: u32 = 0x8000;
    pub const DIR_ADD_PATH: u32 = 0x0001_0000;
    pub const DIR_ADD_NAME: u32 = 0x0002_0000;
    pub const DIR_ADD_REQ_UNIQUE: u32 = 0x0004_0000;
    pub const SERV_ADD_PATH: u32 = 0x0100_0000;
    pub const SERV_ADD_NAME: u32 = 0x0200_0000;
    pub const SERV_ADD_NET_ADDR: u32 = 0x0400_0000;
}

/// Which of an entity's fields a reply carried (`EeWonDirEntity::fields`).
pub mod read {
    pub const TYPE: u32 = 1;
    pub const PATH: u32 = 2;
    pub const NAME: u32 = 4;
    pub const NET_ADDR: u32 = 8;
    pub const REQ_UNIQUE: u32 = 0x10;
    pub const DISPLAY_NAME: u32 = 0x20;
    pub const LIFESPAN: u32 = 0x40;
    pub const CREATED: u32 = 0x80;
    pub const TOUCHED: u32 = 0x100;
    pub const CRC: u32 = 0x200;
    pub const IPS: u32 = 0x400;
    pub const IDS: u32 = 0x800;
}

/// A data object: its type (when the flags carry it) and its data (`None`: not carried).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct DataObject {
    pub data_type: Vec<u8>,
    pub data: Option<Vec<u8>>,
}

/// One permission of an ACL, as read (the C++ converts as the original's locals did).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Acl {
    pub kind: u8,
    pub user: i32,
    pub community: i32,
    pub trust: i16,
}

/// An entity as read: `fields` says which fields the reply carried (the rest keep the `DirEntity` defaults).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Entity {
    pub fields: u32,
    pub kind: u8,
    pub path: Vec<u16>,
    pub name: Vec<u16>,
    pub net_addr: Vec<u8>,
    pub req_unique: bool,
    pub display_name: Vec<u16>,
    pub lifespan: i32,
    pub created: i32,
    pub touched: i32,
    pub crc: i32,
    pub create_ip: i32,
    pub touch_ip: i32,
    pub create_id: i32,
    pub touch_id: i32,
    pub data_objects: Vec<DataObject>,
    pub acls: Vec<Acl>,
}

/// A parse's end.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Reply {
    /// read whole: the status (0, or 100059 when more replies follow a multi-entity reply)
    Parsed(i32),
    /// not a directory reply of this kind (`WS_ServerReq_InvalidReplyHeader`)
    InvalidHeader,
    /// the server's (negative) status
    Status(i16),
    /// the reply ended inside a field (`ReadBufferException`)
    PastEnd,
}

/// `WS_DirServ_MultiGetReply`'s "more replies follow" status.
pub const MORE_REPLIES: i32 = 100059;

fn header(r: &mut Reader, message_type: u16) -> Result<Option<Reply>, PastEnd> {
    let h = r.read_byte()?;
    let service = r.read_short()? as u16;
    let kind = r.read_short()? as u16;
    if h != 5 || service != 2 || kind != message_type {
        return Ok(Some(Reply::InvalidHeader));
    }
    let status = r.read_short()?;
    if status < 0 {
        return Ok(Some(Reply::Status(status)));
    }
    Ok(None)
}

fn read_tail(r: &mut Reader, e: &mut Entity, flags: u32, ips_and_ids: bool) -> Result<(), PastEnd> {
    use self::flags as f;
    if flags & f::ADD_DISPLAY_NAME != 0 {
        e.display_name = r.read_wstring(2)?;
        e.fields |= read::DISPLAY_NAME;
    }
    if flags & f::ADD_LIFESPAN != 0 {
        e.lifespan = r.read_long()?;
        e.fields |= read::LIFESPAN;
    }
    if flags & f::ADD_CREATED != 0 {
        e.created = r.read_long()?;
        e.fields |= read::CREATED;
    }
    if flags & f::ADD_TOUCHED != 0 {
        e.touched = r.read_long()?;
        e.fields |= read::TOUCHED;
    }
    if flags & f::ADD_CRC != 0 {
        e.crc = r.read_long()?;
        e.fields |= read::CRC;
    }
    if ips_and_ids {
        if flags & f::ADD_IPS != 0 {
            e.create_ip = r.read_long()?;
            e.touch_ip = r.read_long()?;
            e.fields |= read::IPS;
        }
        if flags & f::ADD_UIDS != 0 {
            e.create_id = r.read_long()?;
            e.touch_id = r.read_long()?;
            e.fields |= read::IDS;
        }
    }
    if flags & f::ADD_DATA_OBJECTS != 0 {
        let count = r.read_short()? as u16;
        for _ in 0..count {
            let mut o = DataObject::default();
            if flags & f::ADD_DO_TYPE != 0 {
                o.data_type = r.read_string(1)?.to_vec();
            }
            if flags & f::ADD_DO_DATA != 0 {
                let n = r.read_length(2)? as usize;
                o.data = Some(r.read_bytes(n)?.to_vec());
            }
            e.data_objects.push(o);
        }
    }
    if flags & f::ADD_ACLS != 0 {
        let count = r.read_short()? as u16;
        for _ in 0..count {
            let kind = r.read_byte()?;
            let users = r.read_short()? as u16;
            for _ in 0..users {
                let user = r.read_long()?;
                let community = r.read_long()?;
                let trust = r.read_short()?;
                e.acls.push(Acl { kind, user, community, trust });
            }
        }
    }
    Ok(())
}

/// `ParseMultiEntityReply`: the header (5, service 2, type 3), the status, the sequence byte (its top bit marks the
/// last reply), the get flags and the entities. Entities read whole are in `out` even when the reply is cut;
/// `sequence` is the sequence byte once read.
pub fn parse_multi(data: &[u8], out: &mut Vec<Entity>, sequence: &mut Option<u8>) -> Reply {
    use self::flags as f;
    let mut r = Reader::new(data);
    let mut body = || -> Result<Reply, PastEnd> {
        if let Some(reply) = header(&mut r, 3)? {
            return Ok(reply);
        }
        let seq = r.read_byte()?;
        *sequence = Some(seq);
        let more = seq & 0x80 == 0;
        let flags = r.read_long()? as u32;
        let only_dirs = flags & f::DIRS_ONLY_MASK == 0;
        let only_services = flags & f::SERVICES_ONLY_MASK == 0;
        let count = r.read_short()? as u16;
        for _ in 0..count {
            let mut e = Entity::default();
            if flags & f::ADD_TYPE != 0 {
                e.kind = r.read_byte()?;
                e.fields |= read::TYPE;
            }
            // an entity read without its type is the DirEntity default's (0): neither 'S' nor 'D'
            let is_service = (e.fields & read::TYPE != 0 && e.kind == b'S') || only_services;
            let is_dir = (e.fields & read::TYPE != 0 && e.kind == b'D') || only_dirs;
            if is_service {
                if flags & f::SERV_ADD_PATH != 0 {
                    e.path = r.read_wstring(2)?;
                    e.fields |= read::PATH;
                }
                if flags & f::SERV_ADD_NAME != 0 {
                    e.name = r.read_wstring(2)?;
                    e.fields |= read::NAME;
                }
                if flags & f::SERV_ADD_NET_ADDR != 0 {
                    let n = r.read_byte()? as usize;
                    e.net_addr = r.read_bytes(n)?.to_vec();
                    e.fields |= read::NET_ADDR;
                }
            } else if is_dir {
                if flags & f::DIR_ADD_PATH != 0 {
                    e.path = r.read_wstring(2)?;
                    e.fields |= read::PATH;
                }
                if flags & f::DIR_ADD_NAME != 0 {
                    e.name = r.read_wstring(2)?;
                    e.fields |= read::NAME;
                }
                if flags & f::DIR_ADD_REQ_UNIQUE != 0 {
                    e.req_unique = r.read_bool()?;
                    e.fields |= read::REQ_UNIQUE;
                }
            }
            read_tail(&mut r, &mut e, flags, true)?;
            out.push(e);
        }
        Ok(Reply::Parsed(if more { MORE_REPLIES } else { 0 }))
    };
    body().unwrap_or(Reply::PastEnd)
}

/// `ParseSingleEntityReply`: the header (5, service 2, type 2), the status, the get flags and one service entity
/// (its path, name and length-prefixed address; no IPs or ids). `None` unless read whole.
pub fn parse_single(data: &[u8]) -> (Reply, Option<Entity>) {
    use self::flags as f;
    let mut r = Reader::new(data);
    let mut body = || -> Result<(Reply, Option<Entity>), PastEnd> {
        if let Some(reply) = header(&mut r, 2)? {
            return Ok((reply, None));
        }
        let flags = r.read_long()? as u32;
        let mut e = Entity::default();
        if flags & f::ADD_TYPE != 0 {
            e.kind = r.read_byte()?;
            e.fields |= read::TYPE;
        }
        if flags & f::SERV_ADD_PATH != 0 {
            e.path = r.read_wstring(2)?;
            e.fields |= read::PATH;
        }
        if flags & f::SERV_ADD_NAME != 0 {
            e.name = r.read_wstring(2)?;
            e.fields |= read::NAME;
        }
        if flags & f::SERV_ADD_NET_ADDR != 0 {
            let n = r.read_length(1)? as usize;
            e.net_addr = r.read_bytes(n)?.to_vec();
            e.fields |= read::NET_ADDR;
        }
        read_tail(&mut r, &mut e, flags, false)?;
        Ok((Reply::Parsed(0), Some(e)))
    };
    body().unwrap_or((Reply::PastEnd, None))
}

// ---- the C ABI (WONDir/DirEntityReplyParser.cpp)

/// A data object across the C ABI.
#[repr(C)]
pub struct EeWonDirDataObject {
    pub data_type: *const u8,
    pub data_type_len: usize,
    pub has_data: bool,
    pub data: *const u8,
    pub data_len: usize,
}

/// An ACL permission across the C ABI (the raw values).
#[repr(C)]
pub struct EeWonDirAcl {
    pub kind: u8,
    pub user: i32,
    pub community: i32,
    pub trust: i16,
}

/// An entity across the C ABI; valid only during the sink's call. `fields` is the `read` bits.
#[repr(C)]
pub struct EeWonDirEntity {
    pub fields: u32,
    pub kind: u8,
    pub path: *const u16,
    pub path_len: usize,
    pub name: *const u16,
    pub name_len: usize,
    pub display_name: *const u16,
    pub display_name_len: usize,
    pub net_addr: *const u8,
    pub net_addr_len: usize,
    pub req_unique: bool,
    pub lifespan: i32,
    pub created: i32,
    pub touched: i32,
    pub crc: i32,
    pub create_ip: i32,
    pub touch_ip: i32,
    pub create_id: i32,
    pub touch_id: i32,
    pub objects: *const EeWonDirDataObject,
    pub object_count: usize,
    pub acls: *const EeWonDirAcl,
    pub acl_count: usize,
}

/// Receives an entity; valid only during the call.
pub type EeWonDirEntitySink = unsafe extern "C" fn(context: *mut c_void, entity: *const EeWonDirEntity);

/// A parse's end across the C ABI: `kind` 0 parsed (`status` the reply's status), 1 invalid header, 2 the server's
/// negative `status`, 3 cut short; `sequence` when `has_sequence`.
#[repr(C)]
pub struct EeWonDirReply {
    pub kind: i32,
    pub status: i32,
    pub has_sequence: bool,
    pub sequence: u8,
}

unsafe fn give(e: &Entity, sink: EeWonDirEntitySink, context: *mut c_void) {
    let objects: Vec<EeWonDirDataObject> = e
        .data_objects
        .iter()
        .map(|o| EeWonDirDataObject {
            data_type: o.data_type.as_ptr(),
            data_type_len: o.data_type.len(),
            has_data: o.data.is_some(),
            data: o.data.as_ref().map_or(std::ptr::null(), |d| d.as_ptr()),
            data_len: o.data.as_ref().map_or(0, Vec::len),
        })
        .collect();
    let acls: Vec<EeWonDirAcl> = e
        .acls
        .iter()
        .map(|a| EeWonDirAcl { kind: a.kind, user: a.user, community: a.community, trust: a.trust })
        .collect();
    let c = EeWonDirEntity {
        fields: e.fields,
        kind: e.kind,
        path: e.path.as_ptr(),
        path_len: e.path.len(),
        name: e.name.as_ptr(),
        name_len: e.name.len(),
        display_name: e.display_name.as_ptr(),
        display_name_len: e.display_name.len(),
        net_addr: e.net_addr.as_ptr(),
        net_addr_len: e.net_addr.len(),
        req_unique: e.req_unique,
        lifespan: e.lifespan,
        created: e.created,
        touched: e.touched,
        crc: e.crc,
        create_ip: e.create_ip,
        touch_ip: e.touch_ip,
        create_id: e.create_id,
        touch_id: e.touch_id,
        objects: objects.as_ptr(),
        object_count: objects.len(),
        acls: acls.as_ptr(),
        acl_count: acls.len(),
    };
    unsafe { sink(context, &c) };
}

fn reply_of(r: &Reply, sequence: Option<u8>) -> EeWonDirReply {
    let (kind, status) = match r {
        Reply::Parsed(s) => (0, *s),
        Reply::InvalidHeader => (1, 0),
        Reply::Status(s) => (2, *s as i32),
        Reply::PastEnd => (3, 0),
    };
    EeWonDirReply { kind, status, has_sequence: sequence.is_some(), sequence: sequence.unwrap_or(0) }
}

unsafe fn input<'a>(data: *const u8, length: usize) -> &'a [u8] {
    if data.is_null() || length == 0 {
        &[]
    } else {
        // SAFETY: `length` bytes (contract)
        unsafe { std::slice::from_raw_parts(data, length) }
    }
}

/// `ParseMultiEntityReply`: each entity read whole to the sink, in order.
///
/// # Safety
/// `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_parse_multi(
    data: *const u8,
    length: usize,
    sink: EeWonDirEntitySink,
    context: *mut c_void,
) -> EeWonDirReply {
    let mut out = Vec::new();
    let mut sequence = None;
    let r = parse_multi(unsafe { input(data, length) }, &mut out, &mut sequence);
    for e in &out {
        unsafe { give(e, sink, context) };
    }
    reply_of(&r, sequence)
}

/// `ParseSingleEntityReply`: the entity to the sink when read whole.
///
/// # Safety
/// `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dir_parse_single(
    data: *const u8,
    length: usize,
    sink: EeWonDirEntitySink,
    context: *mut c_void,
) -> EeWonDirReply {
    let (r, e) = parse_single(unsafe { input(data, length) });
    if let Some(e) = &e {
        unsafe { give(e, sink, context) };
    }
    reply_of(&r, None)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::wire::Writer;

    fn w(t: &str) -> Vec<u16> {
        t.encode_utf16().collect()
    }

    fn head(kind: u16, status: i16) -> Writer {
        let mut m = Writer::new(4, false);
        m.append_byte(5);
        m.append_short(2);
        m.append_short(kind as i16);
        m.append_short(status);
        m
    }

    #[test]
    fn multi_reply_services_and_dirs() {
        use super::flags as f;
        let fl = f::ADD_TYPE
            | f::SERV_ADD_NAME
            | f::SERV_ADD_NET_ADDR
            | f::DIR_ADD_PATH
            | f::DIR_ADD_REQ_UNIQUE
            | f::ADD_LIFESPAN
            | f::ADD_IPS
            | f::ADD_DATA_OBJECTS
            | f::ADD_DO_TYPE
            | f::ADD_DO_DATA
            | f::ADD_ACLS
            | 0x7;
        let mut m = head(3, 0);
        m.append_byte(0x81);
        m.append_long(fl as i32);
        m.append_short(2);
        // a service
        m.append_byte(b'S');
        m.append_wstring(&w("Auth"), 2);
        m.append_byte(3);
        m.append_bytes(&[1, 2, 3]);
        m.append_long(-7);
        m.append_long(11);
        m.append_long(12);
        m.append_short(1);
        m.append_string(b"t", 1);
        m.append_short(2);
        m.append_bytes(&[9, 8]);
        m.append_short(1);
        m.append_byte(b'a');
        m.append_short(2);
        for v in [1, 2] {
            m.append_long(v);
            m.append_long(-v);
            m.append_short(v as i16);
        }
        // a directory
        m.append_byte(b'D');
        m.append_wstring(&w("/EE"), 2);
        m.append_bool(true);
        m.append_long(5);
        m.append_long(0);
        m.append_long(0);
        m.append_short(0);
        m.append_short(0);
        let mut out = Vec::new();
        let mut seq = None;
        assert_eq!(parse_multi(m.data(), &mut out, &mut seq), Reply::Parsed(0));
        assert_eq!(seq, Some(0x81));
        assert_eq!(out.len(), 2);
        assert_eq!(out[0].name, w("Auth"));
        assert_eq!(out[0].net_addr, vec![1, 2, 3]);
        assert_eq!(out[0].fields & read::PATH, 0);
        assert_eq!(out[0].lifespan, -7);
        assert_eq!((out[0].create_ip, out[0].touch_ip), (11, 12));
        assert_eq!(out[0].data_objects, vec![DataObject { data_type: b"t".to_vec(), data: Some(vec![9, 8]) }]);
        assert_eq!(out[0].acls[1], Acl { kind: b'a', user: 2, community: -2, trust: 2 });
        assert_eq!(out[1].path, w("/EE"));
        assert!(out[1].req_unique);
        // cut inside the second entity: the first stays
        out.clear();
        let cut = &m.data()[..m.data().len() - 3];
        assert_eq!(parse_multi(cut, &mut out, &mut seq), Reply::PastEnd);
        assert_eq!(out.len(), 1);
    }

    #[test]
    fn multi_reply_headers_and_more() {
        let mut out = Vec::new();
        let mut seq = None;
        assert_eq!(parse_multi(head(2, 0).data(), &mut out, &mut seq), Reply::InvalidHeader);
        assert_eq!(parse_multi(head(3, -5).data(), &mut out, &mut seq), Reply::Status(-5));
        assert_eq!(seq, None);
        let mut m = head(3, 0);
        m.append_byte(1);
        m.append_long(0);
        m.append_short(0);
        assert_eq!(parse_multi(m.data(), &mut out, &mut seq), Reply::Parsed(MORE_REPLIES));
        assert_eq!(parse_multi(&[5, 2], &mut out, &mut seq), Reply::PastEnd);
        // no type and dirs-only flags (bit 1 clear): the directory fields
        let mut m = head(3, 0);
        m.append_byte(0x80);
        m.append_long((flags::DIR_ADD_NAME | flags::SERV_ADD_NAME) as i32);
        m.append_short(1);
        m.append_wstring(&w("x"), 2);
        out.clear();
        assert_eq!(parse_multi(m.data(), &mut out, &mut seq), Reply::Parsed(0));
        assert_eq!(out[0].name, w("x"));
    }

    #[test]
    fn single_reply() {
        use super::flags as f;
        let mut m = head(2, 0);
        m.append_long((f::ADD_TYPE | f::SERV_ADD_PATH | f::SERV_ADD_NET_ADDR | f::ADD_IPS | f::ADD_CRC) as i32);
        m.append_byte(b'S');
        m.append_wstring(&w("/p"), 2);
        m.append_byte(2);
        m.append_bytes(&[7, 7]);
        m.append_long(99);
        let (r, e) = parse_single(m.data());
        assert_eq!(r, Reply::Parsed(0));
        let e = e.unwrap();
        assert_eq!(e.crc, 99);
        assert_eq!(e.fields & read::IPS, 0);
        assert_eq!(e.net_addr, vec![7, 7]);
        assert_eq!(parse_single(&m.data()[..m.data().len() - 1]), (Reply::PastEnd, None));
        assert_eq!(parse_single(head(3, 0).data()), (Reply::InvalidHeader, None));
    }
}
