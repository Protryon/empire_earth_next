//! The directory's entities whole in Rust (R6 WON round 34): `DirEntity`, its data objects and ACLs, and the
//! `DirEntityList` the ops' replies make (WONDir/DirEntity.cpp, DirEntityReplyParser.cpp's `DirMakeEntity`), with
//! what the C++ did over a list: `GetNetAddrAsIP` (2f0400), `ServerContext::AddAddressesFromDir` (2acab0) and
//! `AtomicClearAndAddAddressesFromDir`, and MultiPingOp's `AddServers`. The C++ `DirList` (ee/EEWonNet.h) owns one
//! list and reads it through these calls; the lobby's server list reads the entities itself (ee_won_lobby dirread.rs).
//!
//! An entity is the reply's [`Entity`]: the members a reply did not carry keep `DirEntity`'s defaults (zero, empty).
//! Reached by nothing in the game today: the directory servers are gone (the internet login's first request).

use std::ffi::c_void;
use std::ptr::null_mut;

use crate::dir::{Acl, DataObject, EeWonDirEntity, Entity};
use crate::hostop::EeWonHostOp;
use crate::ipobj::EeWonIpAddr;
use crate::servctx::{Address, ServerList};

/// A directory listing: the entities in the reply's order.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeWonDirList {
    pub entities: Vec<Entity>,
}

/// `GetNetAddrAsIP`: the first six bytes of the network address as an address (an empty, invalid one when shorter).
pub fn net_ip(e: &Entity) -> EeWonIpAddr {
    let mut a = EeWonIpAddr::default();
    if e.net_addr.len() >= 6 {
        let mut six = [0u8; 6];
        six.copy_from_slice(&e.net_addr[..6]);
        a.set_six(&six);
    }
    a
}

/// `IPAddr::ToRaw` as the server list keeps it.
pub fn address_of(a: &EeWonIpAddr) -> Address {
    let mut sockaddr = [0u8; 16];
    sockaddr[0..2].copy_from_slice(&a.sa.mFamily.to_ne_bytes());
    sockaddr[2..4].copy_from_slice(&a.sa.mPort.to_ne_bytes());
    sockaddr[4..8].copy_from_slice(&a.sa.mAddress.to_ne_bytes());
    sockaddr[8..16].copy_from_slice(&a.sa.mZero);
    Address { sockaddr, valid: a.valid, remember_host: a.remember_host, host: a.host.clone() }
}

/// The entity's UTF-16 name as the C++'s `wchar_t` units (`unit` bytes each), as `WonWideSink` assigns it.
fn wide(units: &[u16], unit: u32) -> Vec<u32> {
    if unit == 2 {
        units.iter().map(|&u| u as u32).collect()
    } else {
        crate::text::utf32_from_utf16(units)
    }
}

/// The name filter's test: a service (not a directory, `mType != 'D'`) whose name equals a non-empty filter.
pub fn passes(e: &Entity, filter: &[u32], unit: u32) -> bool {
    e.kind != b'D' && (filter.is_empty() || wide(&e.name, unit) == filter)
}

/// `ServerContext::DirAddresses`: each passing entity's address, valid or not.
pub fn dir_addresses(l: &EeWonDirList, filter: &[u32], unit: u32) -> Vec<Address> {
    l.entities.iter().filter(|e| passes(e, filter, unit)).map(|e| address_of(&net_ip(e))).collect()
}

/// `AddAddressesFromDir`: each address added in order; whether any joined.
pub fn add_from_dir(ctx: &ServerList, l: &EeWonDirList, filter: &[u32], unit: u32) -> bool {
    let mut added = false;
    for a in dir_addresses(l, filter, unit) {
        if ctx.add(&a) {
            added = true;
        }
    }
    added
}

/// `DirMakeEntity`: the record's carried members (its `fields` bits), its data objects and ACLs.
///
/// # Safety
/// `e`'s pointers hold their counts.
pub unsafe fn entity_of(e: &EeWonDirEntity) -> Entity {
    unsafe fn s<'a, T>(p: *const T, n: usize) -> &'a [T] {
        if p.is_null() || n == 0 {
            &[]
        } else {
            // SAFETY: the record's contract
            unsafe { std::slice::from_raw_parts(p, n) }
        }
    }
    let f = e.fields;
    let mut o = Entity { fields: f, ..Entity::default() };
    if f & 0x1 != 0 {
        o.kind = e.kind;
    }
    if f & 0x2 != 0 {
        o.path = unsafe { s(e.path, e.path_len) }.to_vec();
    }
    if f & 0x4 != 0 {
        o.name = unsafe { s(e.name, e.name_len) }.to_vec();
    }
    if f & 0x8 != 0 {
        o.net_addr = unsafe { s(e.net_addr, e.net_addr_len) }.to_vec();
    }
    if f & 0x10 != 0 {
        o.req_unique = e.req_unique;
    }
    if f & 0x20 != 0 {
        o.display_name = unsafe { s(e.display_name, e.display_name_len) }.to_vec();
    }
    if f & 0x40 != 0 {
        o.lifespan = e.lifespan;
    }
    if f & 0x80 != 0 {
        o.created = e.created;
    }
    if f & 0x100 != 0 {
        o.touched = e.touched;
    }
    if f & 0x200 != 0 {
        o.crc = e.crc;
    }
    if f & 0x400 != 0 {
        o.create_ip = e.create_ip;
        o.touch_ip = e.touch_ip;
    }
    if f & 0x800 != 0 {
        o.create_id = e.create_id;
        o.touch_id = e.touch_id;
    }
    for d in unsafe { s(e.objects, e.object_count) } {
        o.data_objects.push(DataObject {
            data_type: unsafe { s(d.data_type, d.data_type_len) }.to_vec(),
            data: d.has_data.then(|| unsafe { s(d.data, d.data_len) }.to_vec()),
        });
    }
    for a in unsafe { s(e.acls, e.acl_count) } {
        o.acls.push(Acl { kind: a.kind, user: a.user, community: a.community, trust: a.trust });
    }
    o
}

// ---- the C ABI (ee/EEWonNet.h's DirList, ServerContext's and the multi-ping's directory calls)

/// Which text of an entity `ee_won_dirlist_text` gives.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonDirText {
    /// `mName`
    Name,
    /// `mDisplayName`
    DisplayName,
    /// `mPath`
    Path,
}

/// Receives UTF-16 units (the C++'s `WonWideSink`).
pub type EeWonDirUnitsSink = unsafe extern "C" fn(context: *mut c_void, units: *const u16, count: usize);

unsafe fn list<'a>(l: *const EeWonDirList) -> Option<&'a EeWonDirList> {
    // SAFETY: NULL or a live list (each call's contract)
    unsafe { l.as_ref() }
}
unsafe fn at<'a>(l: *const EeWonDirList, i: usize) -> Option<&'a Entity> {
    unsafe { list(l) }.and_then(|l| l.entities.get(i))
}
unsafe fn filter_of(filter: *const c_void, count: usize, unit: u32) -> Vec<u32> {
    if filter.is_null() || count == 0 {
        return Vec::new();
    }
    // SAFETY: `count` units of `unit` bytes (the contract)
    unsafe {
        match unit {
            4 => std::slice::from_raw_parts(filter as *const u32, count).to_vec(),
            _ => std::slice::from_raw_parts(filter as *const u16, count).iter().map(|&u| u as u32).collect(),
        }
    }
}

/// An empty list (the C++ frees it with `ee_won_dirlist_free`).
#[no_mangle]
pub extern "C" fn ee_won_dirlist_new() -> *mut EeWonDirList {
    Box::into_raw(Box::default())
}
/// # Safety
/// `l` is NULL or a list from `ee_won_dirlist_new` / `_of_op`, not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_free(l: *mut EeWonDirList) {
    if !l.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(l) });
    }
}
/// `RustServerOp::GetEntities(which, index)`: a new list of what the op read (empty for none).
///
/// # Safety
/// `op` is NULL or a live op.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_of_op(op: *const EeWonHostOp, which: i32, index: i64) -> *mut EeWonDirList {
    let entities = if op.is_null() { Vec::new() } else { unsafe { (*op).entities(which, index) } };
    Box::into_raw(Box::new(EeWonDirList { entities }))
}
/// `push_back(new DirEntity(e))` (DirEntityReplyParser::TakeEntity).
///
/// # Safety
/// `l` is a live list; `e` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_push(l: *mut EeWonDirList, e: *const EeWonDirEntity) {
    // SAFETY: the contract
    unsafe { (*l).entities.push(entity_of(&*e)) }
}
/// `size()` (0 for NULL).
///
/// # Safety
/// `l` is NULL or a live list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_len(l: *const EeWonDirList) -> usize {
    unsafe { list(l) }.map_or(0, |l| l.entities.len())
}
/// `clear()`.
///
/// # Safety
/// `l` is a live list.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_clear(l: *mut EeWonDirList) {
    unsafe { (*l).entities.clear() }
}
/// `dst.push_back(src[i])` (EEWonNet's server list keeps a listing's entity).
///
/// # Safety
/// Live lists; `i` is in `src`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_keep(dst: *mut EeWonDirList, src: *const EeWonDirList, i: usize) {
    if let Some(e) = unsafe { at(src, i) }.cloned() {
        unsafe { (*dst).entities.push(e) }
    }
}
/// The entity's `mType`.
///
/// # Safety
/// As `ee_won_dirlist_len`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_kind(l: *const EeWonDirList, i: usize) -> u8 {
    unsafe { at(l, i) }.map_or(0, |e| e.kind)
}
/// The entity's text (its UTF-16 units) to `sink`.
///
/// # Safety
/// As `ee_won_dirlist_len`; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_text(
    l: *const EeWonDirList,
    i: usize,
    which: EeWonDirText,
    sink: EeWonDirUnitsSink,
    context: *mut c_void,
) {
    let empty = Vec::new();
    let t = unsafe { at(l, i) }.map_or(&empty, |e| match which {
        EeWonDirText::Name => &e.name,
        EeWonDirText::DisplayName => &e.display_name,
        EeWonDirText::Path => &e.path,
    });
    unsafe { sink(context, t.as_ptr(), t.len()) }
}
/// `GetNetAddrAsIP()`: a new value the C++ `IPAddr` adopts.
///
/// # Safety
/// As `ee_won_dirlist_len`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dirlist_addr(l: *const EeWonDirList, i: usize) -> *mut EeWonIpAddr {
    Box::into_raw(Box::new(unsafe { at(l, i) }.map_or_else(EeWonIpAddr::default, net_ip)))
}
/// `ServerContext::AddAddressesFromDir(list, filter)`: the filter is `count` `wchar_t` units of `unit` bytes (NULL
/// or empty: none).
///
/// # Safety
/// `c` is a live server list; `l` NULL or a live list; `filter` holds `count` units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_add_from_dir(
    c: *const ServerList,
    l: *const EeWonDirList,
    filter: *const c_void,
    count: usize,
    unit: u32,
) -> bool {
    let f = unsafe { filter_of(filter, count, unit) };
    unsafe { list(l) }.is_some_and(|l| add_from_dir(unsafe { &*c }, l, &f, unit))
}
/// `ServerContext::AtomicClearAndAddAddressesFromDir(list, filter)`: cleared and the addresses added under one lock.
///
/// # Safety
/// As `ee_won_ctx_add_from_dir`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_ctx_replace_from_dir(
    c: *const ServerList,
    l: *const EeWonDirList,
    filter: *const c_void,
    count: usize,
    unit: u32,
) -> bool {
    let f = unsafe { filter_of(filter, count, unit) };
    let addrs = unsafe { list(l) }.map_or_else(Vec::new, |l| dir_addresses(l, &f, unit));
    unsafe { &*c }.replace(&addrs)
}
/// MultiPingOp's `AddServers(list, filter)`: each passing entity with a valid address, its six bytes, as
/// `EE_WON_MULTIPING_ADD` (no entity object: the element's `EE_WON_MULTIPING_ENTITY` is 0).
///
/// # Safety
/// `op` is a live multi-ping op; as `ee_won_ctx_add_from_dir`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_multiping_add_servers(
    op: *const EeWonHostOp,
    l: *const EeWonDirList,
    filter: *const c_void,
    count: usize,
    unit: u32,
) {
    let f = unsafe { filter_of(filter, count, unit) };
    let Some(l) = (unsafe { list(l) }) else { return };
    for e in l.entities.iter().filter(|e| passes(e, &f, unit)) {
        let a = net_ip(e);
        if a.valid {
            let six = a.six();
            unsafe {
                crate::hostop::ee_won_hostop_set(
                    op,
                    crate::multiping::EE_WON_MULTIPING_ADD,
                    0,
                    std::ptr::null(),
                    0,
                    six.as_ptr(),
                    null_mut(),
                )
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ent(kind: u8, name: &str, addr: &[u8]) -> Entity {
        Entity { kind, name: name.encode_utf16().collect(), net_addr: addr.to_vec(), ..Entity::default() }
    }
    fn w(t: &str) -> Vec<u32> {
        t.chars().map(|c| c as u32).collect()
    }

    #[test]
    fn net_ip_takes_six_bytes_or_none() {
        assert!(!net_ip(&ent(0, "a", &[1, 2, 3, 4, 5])).valid);
        let a = net_ip(&ent(0, "a", &[0x1f, 0x90, 10, 0, 0, 1, 99]));
        assert!(a.valid);
        assert_eq!(a.six(), [0x1f, 0x90, 10, 0, 0, 1]);
    }

    #[test]
    fn filter_skips_directories_and_other_names() {
        let e = ent(b'S', "Routing", &[]);
        assert!(passes(&e, &[], 2));
        assert!(passes(&e, &w("Routing"), 2));
        assert!(!passes(&e, &w("routing"), 2));
        assert!(!passes(&ent(b'D', "Routing", &[]), &[], 2));
    }

    #[test]
    fn add_from_dir_adds_valid_once() {
        let l = EeWonDirList {
            entities: vec![
                ent(b'S', "a", &[0, 1, 10, 0, 0, 1]),
                ent(b'S', "b", &[0, 1, 10, 0, 0, 1]),
                ent(b'S', "c", &[0, 1]),
                ent(b'D', "d", &[0, 2, 10, 0, 0, 2]),
            ],
        };
        let ctx = ServerList::new();
        assert!(add_from_dir(&ctx, &l, &[], 2));
        assert!(!add_from_dir(&ctx, &l, &[], 2));
        assert_eq!(dir_addresses(&l, &[], 2).len(), 3);
        assert!(ctx.replace(&dir_addresses(&l, &w("b"), 2)));
    }

    #[test]
    fn entity_of_keeps_only_carried_fields() {
        let name: Vec<u16> = "n".encode_utf16().collect();
        let objs = [crate::dir::EeWonDirDataObject {
            data_type: b"_C".as_ptr(),
            data_type_len: 2,
            has_data: false,
            data: std::ptr::null(),
            data_len: 0,
        }];
        let r = EeWonDirEntity {
            fields: 0x4,
            kind: 7,
            path: std::ptr::null(),
            path_len: 0,
            name: name.as_ptr(),
            name_len: 1,
            display_name: std::ptr::null(),
            display_name_len: 0,
            net_addr: std::ptr::null(),
            net_addr_len: 0,
            req_unique: true,
            lifespan: 5,
            created: 0,
            touched: 0,
            crc: 0,
            create_ip: 0,
            touch_ip: 0,
            create_id: 0,
            touch_id: 0,
            objects: objs.as_ptr(),
            object_count: 1,
            acls: std::ptr::null(),
            acl_count: 0,
        };
        let e = unsafe { entity_of(&r) };
        assert_eq!(e.kind, 0);
        assert_eq!(e.name, name);
        assert!(!e.req_unique);
        assert_eq!(e.lifespan, 0);
        assert_eq!(e.data_objects, vec![DataObject { data_type: b"_C".to_vec(), data: None }]);
    }

    #[test]
    fn the_c_abi_lists() {
        let l = ee_won_dirlist_new();
        unsafe {
            (*l).entities.push(ent(b'S', "x", &[0, 5, 1, 2, 3, 4]));
            assert_eq!(ee_won_dirlist_len(l), 1);
            let k = ee_won_dirlist_new();
            ee_won_dirlist_keep(k, l, 0);
            ee_won_dirlist_keep(k, l, 3);
            assert_eq!(ee_won_dirlist_len(k), 1);
            assert_eq!(ee_won_dirlist_kind(k, 0), b'S');
            let a = ee_won_dirlist_addr(k, 0);
            assert!((*a).valid);
            drop(Box::from_raw(a));
            ee_won_dirlist_clear(k);
            assert_eq!(ee_won_dirlist_len(k), 0);
            ee_won_dirlist_free(k);
            ee_won_dirlist_free(l);
            assert_eq!(ee_won_dirlist_len(std::ptr::null()), 0);
        }
    }
}
