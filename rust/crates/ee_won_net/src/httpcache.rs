//! `HTTPCache` and `HTTPCacheEntry` whole in Rust (net round 21; WONMisc/HTTPCache.cpp): the cache's directory
//! (made up front, absolute when asked), its index file (`cachemap.dat`, read and written by cachemap.rs on the Rust
//! file layer: read when the directory is set, written when the cache is destroyed, with the 14-day keep rule and the
//! dropped entries' files deleted), its maps by location and by id, the free id, each entry's numbered file name
//! (`http::cache_file_name`, a stale file at that name deleted), the last access time, and the process's global
//! cache in the WON directory (`_wonHTTPCache`, made at its first use and released at exit as the original's static
//! `SmartPtr` was).
//!
//! The cache and its entries are op base handles (hostauth.rs's tagged `Arc<Item>`): the HTTP get op holds them as
//! numbers, and its four cache calls (`EE_WON_HOST_CACHE_ENTRY`, `_ENTRY_GET`, `_ENTRY_SET`, `_CACHE_UPDATE`) are
//! answered here when the game binds the op (`ee_won_hostop_bind_native`); ServerOp.cpp's `HostCall` answered them
//! on the C++ objects. `GetEntry` returns a borrowed entry in the original; the call answers a new reference, as the
//! C++ `HostCall`'s `Out` did.
//!
//! The behaviour is `golden_httpcache.txt` (tests/won/won_httpcache_test.cpp, recorded over the C++ classes through
//! the op's own call path). The original's undefined corner, defined: a cache made with an empty path never set its
//! next free id (heap garbage); here it starts at 1.
//!
//! Reached only by the lobby's cross-promotion image (`InitLogic::ParseCrossPromotion`, after an Internet login's
//! advert document): never by a LAN game.

use std::collections::BTreeMap;
use std::ffi::{c_char, c_void, CString};
use std::sync::{Arc, Mutex, MutexGuard, Once};

use crate::cachemap::{self, Out};
use crate::file::{EeWonFileReader, EeWonFileWriter};
use crate::hostauth::{self, Item};
use crate::hostop::{self, Obj, Reply};
use crate::http::{cache_file_name, free_id};

/// cbindgen:ignore
mod sys {
    use std::ffi::{c_char, c_int};
    extern "C" {
        // the platform layer (ee_plat_sys)
        pub fn PlatPathGetAttributes(path: *const c_char) -> u32;
        pub fn PlatPathSetAttributes(path: *const c_char, attributes: u32) -> c_int;
        pub fn PlatDeleteFile(path: *const c_char) -> c_int;
        pub fn PlatCreateDirectory(path: *const c_char) -> c_int;
        pub fn PlatGetCurrentDirectory(path: *mut c_char, size: u32) -> u32;
    }
}

const PLAT_ATTR_READONLY: u32 = 0x01;
const PLAT_ATTR_NORMAL: u32 = 0x80;
const PLAT_ATTR_INVALID: u32 = 0xFFFF_FFFF;
const PLAT_MAX_PATH: usize = 260;
/// `gHTTPCacheVersion`
const VERSION: i32 = 1;

/// One entry (`HTTPCacheEntry`).
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub(crate) struct CacheEntry {
    pub(crate) id: i32,
    pub(crate) valid: bool,
    pub(crate) remote: Vec<u8>,
    pub(crate) local_path: Vec<u8>,
    pub(crate) file_name: Vec<u8>,
    pub(crate) file_type: Vec<u8>,
    pub(crate) etag: Vec<u8>,
    pub(crate) expiration: i64,
    pub(crate) last_access: i64,
}

/// The cache's members (`HTTPCache`).
#[derive(Default)]
struct State {
    path: Vec<u8>,
    by_location: BTreeMap<Vec<u8>, Arc<Item>>,
    by_id: BTreeMap<i32, Arc<Item>>,
    next_free_id: i32,
}

/// The cache: its index is written when it is dropped (the original's destructor).
pub(crate) struct HttpCache {
    st: Mutex<State>,
}

/// `time(NULL)`.
fn now() -> i64 {
    match std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH) {
        Ok(d) => d.as_secs() as i64,
        Err(e) => -(e.duration().as_secs() as i64),
    }
}

/// A C string of the bytes up to the first NUL (`std::string::c_str()`).
fn c_path(data: &[u8]) -> CString {
    let end = data.iter().position(|&b| b == 0).unwrap_or(data.len());
    CString::new(&data[..end]).unwrap_or_default()
}

/// `WONFile(path).Delete(true)`: a read-only file made writable first.
fn delete_file(path: &[u8]) {
    let p = c_path(&crate::path::given(path));
    // SAFETY: C strings
    unsafe {
        let attributes = sys::PlatPathGetAttributes(p.as_ptr());
        if attributes != PLAT_ATTR_INVALID && attributes & PLAT_ATTR_READONLY != 0 {
            let a = attributes & !PLAT_ATTR_READONLY;
            sys::PlatPathSetAttributes(p.as_ptr(), if a != 0 { a } else { PLAT_ATTR_NORMAL });
        }
        sys::PlatDeleteFile(p.as_ptr());
    }
}

/// `WONFile(path).CreateAsDirectory(true)` (its result unused): made unless something is there.
fn create_directory(path: &[u8]) {
    let p = c_path(&crate::path::given(path));
    // SAFETY: a C string
    unsafe {
        if sys::PlatPathGetAttributes(p.as_ptr()) == PLAT_ATTR_INVALID {
            sys::PlatCreateDirectory(p.as_ptr());
        }
    }
}

/// `WONFile(path).GetFullPath()`: joined to the current directory when it needs it; empty when the current
/// directory cannot be read or the path leaves its root (the original threw `out_of_range` there).
fn full_path(path: &[u8]) -> Vec<u8> {
    let given = crate::path::given(path);
    let current = if crate::path::needs_current(&given) {
        let mut buf = [0u8; PLAT_MAX_PATH];
        // SAFETY: a buffer of its size
        let n = unsafe { sys::PlatGetCurrentDirectory(buf.as_mut_ptr() as *mut c_char, buf.len() as u32) } as usize;
        if n == 0 || n >= buf.len() {
            return Vec::new();
        }
        Some(buf[..n].to_vec())
    } else {
        None
    };
    crate::path::absolute(&given, current.as_deref()).unwrap_or_default()
}

fn lock<T>(m: &Mutex<T>) -> MutexGuard<'_, T> {
    m.lock().unwrap_or_else(|e| e.into_inner())
}

/// An entry's fields (`Item::CacheEntry`), or None for anything else.
fn entry_of(item: &Item) -> Option<&Mutex<CacheEntry>> {
    match item {
        Item::CacheEntry(e) => Some(e),
        _ => None,
    }
}

impl HttpCache {
    /// `HTTPCache(path, fullPath)`.
    pub(crate) fn new(path: &[u8], full_path: bool) -> HttpCache {
        let c = HttpCache { st: Mutex::new(State { next_free_id: 1, ..State::default() }) };
        c.set_cache_path(path, full_path);
        c
    }

    /// `SetCachePath`: the directory made (from the path as given), the path absolute when asked, a separator
    /// added, the index read.
    fn set_cache_path(&self, path: &[u8], full_path: bool) {
        let mut s = lock(&self.st);
        s.path = if full_path { self::full_path(path) } else { path.to_vec() };
        create_directory(path);
        if s.path.is_empty() {
            return;
        }
        if !matches!(s.path.last(), Some(b'\\' | b'/')) {
            s.path.push(b'/');
        }
        read_cache_map(&mut s);
    }

    /// `GetEntry(location)`: a new entry with the next free id and a numbered file name (a file there deleted);
    /// the entry marked used now. A new handle reference to it.
    pub(crate) fn entry(&self, location: &[u8]) -> Obj {
        let mut s = lock(&self.st);
        let found = s.by_location.get(location).cloned();
        let item = match found {
            Some(item) => item,
            None => {
                let ids: Vec<i32> = s.by_id.keys().copied().collect();
                s.next_free_id = free_id(&ids, s.next_free_id);
                let id = s.next_free_id;
                let file_name = cache_file_name(id, location);
                let mut local_path = s.path.clone();
                local_path.extend_from_slice(&file_name);
                delete_file(&local_path);
                let entry =
                    CacheEntry { id, remote: location.to_vec(), local_path, file_name, ..CacheEntry::default() };
                let item = Arc::new(Item::CacheEntry(Mutex::new(entry)));
                s.by_location.insert(location.to_vec(), item.clone());
                s.by_id.insert(id, item.clone());
                item
            }
        };
        if let Some(e) = entry_of(&item) {
            lock(e).last_access = now();
        }
        hostauth::handle_of(&item)
    }
}

/// `ReadCacheMap`: the maps emptied, the free id 1, then each entry the index holds (a later one replaces an
/// earlier one with its id or location).
fn read_cache_map(s: &mut State) {
    s.by_id.clear();
    s.by_location.clear();
    s.next_free_id = 1;
    let mut path = s.path.clone();
    path.extend_from_slice(b"cachemap.dat");
    let p = c_path(&path);
    let mut r = EeWonFileReader::new();
    // SAFETY: a C string
    if !unsafe { r.open(p.as_ptr()) } {
        return;
    }
    let base = s.path.clone();
    cachemap::read_map(&mut r, VERSION, &mut |e: cachemap::Entry| {
        let mut local_path = base.clone();
        local_path.extend_from_slice(&e.file_name);
        let entry = CacheEntry {
            id: e.id,
            valid: e.valid,
            remote: e.remote.clone(),
            local_path,
            file_name: e.file_name,
            file_type: e.file_type,
            etag: e.etag,
            expiration: i64::from(e.expiration),
            last_access: i64::from(e.last_access),
        };
        let item = Arc::new(Item::CacheEntry(Mutex::new(entry)));
        s.by_id.insert(e.id, item.clone());
        s.by_location.insert(e.remote, item);
    });
    r.close();
}

/// `WriteCacheMap`: the entries in id order; those not kept have their files deleted.
fn write_cache_map(s: &State, now: i64) {
    let mut path = s.path.clone();
    path.extend_from_slice(b"cachemap.dat");
    let p = c_path(&path);
    let mut w = EeWonFileWriter::new();
    // SAFETY: a C string
    if !unsafe { w.open(p.as_ptr()) } {
        return;
    }
    let entries: Vec<CacheEntry> = s.by_id.values().filter_map(|i| entry_of(i)).map(|e| lock(e).clone()).collect();
    let outs: Vec<Out> = entries
        .iter()
        .map(|e| Out {
            id: e.id,
            valid: e.valid,
            expiration: e.expiration,
            last_access: e.last_access,
            strings: [&e.remote, &e.file_name, &e.file_type, &e.etag],
        })
        .collect();
    cachemap::write_map(&mut w, VERSION, now, &outs, &mut |i| delete_file(&entries[i].local_path));
    w.close();
}

impl Drop for HttpCache {
    /// `~HTTPCache`: the index written.
    fn drop(&mut self) {
        let s = self.st.get_mut().unwrap_or_else(|e| e.into_inner());
        write_cache_map(s, now());
    }
}

// ---- the op base's cache calls

fn num(n: i64) -> Option<Reply> {
    Some(Reply { obj: Obj::none(), num: n, addr: [0; 6], bytes: None })
}

/// The HTTP get op's cache calls on handles; None for anything else (sent to the C++ side).
pub(crate) fn answer(code: i32, o: [Obj; 3], n: [i64; 2], data: &[u8]) -> Option<Reply> {
    use hostop::*;
    let item = hostauth::item(o[0])?;
    match (code, &*item) {
        (EE_WON_HOST_CACHE_ENTRY, Item::HttpCache(c)) => {
            Some(Reply { obj: c.entry(data), num: 0, addr: [0; 6], bytes: None })
        }
        (EE_WON_HOST_ENTRY_GET, Item::CacheEntry(e)) => {
            let e = lock(e);
            match n[0] {
                0 => num(i64::from(e.valid)),
                1 => num(e.expiration),
                w => Some(Reply {
                    obj: Obj::none(),
                    num: 0,
                    addr: [0; 6],
                    bytes: Some(if w == 2 { e.local_path.clone() } else { e.etag.clone() }),
                }),
            }
        }
        (EE_WON_HOST_ENTRY_SET, Item::CacheEntry(e)) => {
            let mut e = lock(e);
            match n[0] {
                0 => e.valid = n[1] != 0,
                1 => e.expiration = n[1],
                _ => e.etag = data.to_vec(),
            }
            num(0)
        }
        // `UpdateEntry(entry)`: empty in this version
        (EE_WON_HOST_CACHE_UPDATE, Item::HttpCache(_)) => num(0),
        _ => None,
    }
}

// ---- the global cache

static GLOBAL: Mutex<Option<Arc<Item>>> = Mutex::new(None);
static AT_EXIT: Once = Once::new();

/// cbindgen:ignore
mod crt {
    extern "C" {
        pub fn atexit(f: extern "C" fn()) -> std::ffi::c_int;
    }
}

extern "C" fn release_global_at_exit() {
    ee_won_http_cache_reset_global();
}

// ---- FFI

/// `new HTTPCache(path, fullPath)`: a handle with one reference (released with the op base's `EE_WON_HOST_RELEASE`
/// or [`ee_won_http_cache_release`]).
///
/// # Safety
/// `path` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_cache_new(path: *const u8, len: usize, full_path: bool) -> *mut c_void {
    // SAFETY: the caller's bytes
    let path = unsafe { crate::ffi::bytes(path, len) };
    hostauth::handle(Item::HttpCache(HttpCache::new(path, full_path))).ptr()
}

/// A reference released (the cache's last one writes its index).
///
/// # Safety
/// `cache` is NULL or a handle from [`ee_won_http_cache_new`] the caller holds a reference to.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_cache_release(cache: *mut c_void) {
    hostauth::release(Obj::of(cache));
}

/// `HTTPCache::GetGlobalCache()`: the process's cache in `dir + "_wonHTTPCache"` (the WON directory with its
/// separator), made at the first call; borrowed (the op base's setters take their own reference). Released at exit.
///
/// # Safety
/// `dir` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_cache_global(dir: *const u8, len: usize) -> *mut c_void {
    let mut g = lock(&GLOBAL);
    if g.is_none() {
        // SAFETY: the caller's bytes
        let mut path = unsafe { crate::ffi::bytes(dir, len) }.to_vec();
        path.extend_from_slice(b"_wonHTTPCache");
        *g = Some(Arc::new(Item::HttpCache(HttpCache::new(&path, false))));
        AT_EXIT.call_once(|| {
            // SAFETY: a function that lives as long as the process
            unsafe { crt::atexit(release_global_at_exit) };
        });
    }
    g.as_ref().map_or(std::ptr::null_mut(), |i| (Arc::as_ptr(i) as usize | 1) as *mut c_void)
}

/// `HTTPCache::SetGlobalCache(NULL)`: the global cache's reference released (its index written when it was the
/// last); the next [`ee_won_http_cache_global`] makes a new one.
#[no_mangle]
pub extern "C" fn ee_won_http_cache_reset_global() {
    let old = lock(&GLOBAL).take();
    drop(old);
}

#[cfg(test)]
#[path = "httpcache_tests.rs"]
mod tests;
