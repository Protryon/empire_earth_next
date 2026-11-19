//! TEST ONLY: the simulation capture's writer (ee/EESimCapture.h; tools/sim_capture.py; was EESimCapture.cpp). With
//! `EE_SIM_CAPTURE=<absolute file>` a game appends one binary record per call of a captured leaf: a u32 tag, a u32
//! payload size, the payload. Records are written whole under a lock, through the C library's stream (`fopen(..,
//! "wb")`, `fwrite`, `fflush`) as the C++ wrote them: the stream's buffer is flushed by `exit` as before.
//!
//! The filters: a thread's "only" tags (a sweep that calls a leaf records only that leaf), the exclusive owner (every
//! other thread's records dropped), closed (no record after the game's end) and held (no record while on).

use std::cell::{Cell, RefCell};
use std::ffi::{c_void, CString};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};

/// The filters' state (the process's and this thread's).
#[derive(Default)]
pub struct Filters {
    pub exclusive: AtomicBool,
    pub closed: AtomicBool,
    pub held: AtomicBool,
}

thread_local! {
    static ONLY: RefCell<Vec<u32>> = const { RefCell::new(Vec::new()) };
    static OWNER: Cell<bool> = const { Cell::new(false) };
}

impl Filters {
    /// Whether a record of `tag` committed now on this thread is written (`only`: this thread's tags, `owner`: this
    /// thread owns the exclusive switch).
    pub fn keeps(&self, tag: u32, only: &[u32], owner: bool) -> bool {
        if self.closed.load(Ordering::SeqCst)
            || self.held.load(Ordering::SeqCst)
            || (self.exclusive.load(Ordering::SeqCst) && !owner)
        {
            return false;
        }
        only.is_empty() || only.contains(&tag)
    }
}

/// A record's bytes: the tag, the payload's size, the payload (little-endian).
pub fn record_bytes(tag: u32, payload: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(8 + payload.len());
    out.extend_from_slice(&tag.to_le_bytes());
    out.extend_from_slice(&(payload.len() as u32).to_le_bytes());
    out.extend_from_slice(payload);
    out
}

/// Up to four tags (the C++ interface's limit; more are dropped).
pub fn only_tags(tags: &[u32]) -> Vec<u32> {
    tags.iter().copied().take(4).collect()
}

/// The C library's stream (kept out of the generated headers).
/// cbindgen:ignore
mod crt {
    use std::ffi::{c_char, c_void};
    extern "C" {
        pub fn fopen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub fn fwrite(data: *const c_void, size: usize, count: usize, file: *mut c_void) -> usize;
        pub fn fflush(file: *mut c_void) -> i32;
    }
}
use crt::{fflush, fopen, fwrite};

struct Stream(*mut c_void);
// SAFETY: the C library's stream is used only under `LOCK`
unsafe impl Send for Stream {}
unsafe impl Sync for Stream {}

static FILTERS: Filters =
    Filters { exclusive: AtomicBool::new(false), closed: AtomicBool::new(false), held: AtomicBool::new(false) };
static LOCK: Mutex<()> = Mutex::new(());

/// The capture file, opened once at the first call (the game changes into its data root first: the path is
/// absolute); None when `EE_SIM_CAPTURE` is unset or empty, or the open failed.
fn stream() -> Option<&'static Stream> {
    static FILE: OnceLock<Option<Stream>> = OnceLock::new();
    FILE.get_or_init(|| {
        let path = std::env::var_os("EE_SIM_CAPTURE")?;
        let path = CString::new(path.to_string_lossy().into_owned()).ok()?;
        if path.as_bytes().is_empty() {
            return None;
        }
        // SAFETY: two C strings
        let f = unsafe { fopen(path.as_ptr(), c"wb".as_ptr()) };
        (!f.is_null()).then_some(Stream(f))
    })
    .as_ref()
}

fn keeps(tag: u32) -> bool {
    stream().is_some() && ONLY.with(|o| FILTERS.keeps(tag, &o.borrow(), OWNER.with(Cell::get)))
}

/// `EESimCaptureRecord::Active()`: `EE_SIM_CAPTURE` is set and the file opened.
#[no_mangle]
pub extern "C" fn ee_app_capture_active() -> bool {
    stream().is_some()
}

/// `Only(tags, count)`: this thread's records of other tags are dropped (count 0 clears it; at most four).
///
/// # Safety
/// `tags` points to `count` tags (or `count` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_only(tags: *const u32, count: u32) {
    let tags = if count == 0 || tags.is_null() {
        Vec::new()
    } else {
        // SAFETY: the caller's contract
        only_tags(unsafe { std::slice::from_raw_parts(tags, count as usize) })
    };
    ONLY.with(|o| *o.borrow_mut() = tags);
}

/// `Exclusive(on)`: while on, every other thread's records are dropped.
#[no_mangle]
pub extern "C" fn ee_app_capture_exclusive(on: bool) {
    OWNER.with(|o| o.set(on));
    FILTERS.exclusive.store(on, Ordering::SeqCst);
}

/// `Close()`: no record of any thread after it.
#[no_mangle]
pub extern "C" fn ee_app_capture_close() {
    FILTERS.closed.store(true, Ordering::SeqCst);
}

/// `Hold(on)`: while on, no record of any thread is written.
#[no_mangle]
pub extern "C" fn ee_app_capture_hold(on: bool) {
    FILTERS.held.store(on, Ordering::SeqCst);
}

/// `Keeps(tag)`: whether a record of `tag` committed now on this thread would be written.
#[no_mangle]
pub extern "C" fn ee_app_capture_keeps(tag: u32) -> bool {
    keeps(tag)
}

/// Appends one record (`Commit`) when the filters keep it.
pub fn commit(tag: u32, payload: &[u8]) {
    let Some(f) = stream() else { return };
    if !keeps(tag) {
        return;
    }
    let bytes = record_bytes(tag, payload);
    let _guard = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the open stream, under the lock
    unsafe { fwrite(bytes.as_ptr().cast(), 1, bytes.len(), f.0) };
}

/// `Commit()`: the record of `tag` with `size` payload bytes.
///
/// # Safety
/// `data` points to `size` bytes (or `size` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_app_capture_commit(tag: u32, data: *const u8, size: usize) {
    let payload = if size == 0 || data.is_null() {
        &[][..]
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(data, size) }
    };
    commit(tag, payload);
}

/// `Flush()`: the stream's buffer written out (EE-SIM's Quit, before the process ends).
#[no_mangle]
pub extern "C" fn ee_app_capture_flush() {
    let Some(f) = stream() else { return };
    let _guard = LOCK.lock().unwrap_or_else(|e| e.into_inner());
    // SAFETY: the open stream, under the lock
    unsafe { fflush(f.0) };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn records_and_filters() {
        assert_eq!(record_bytes(0x4c424243, &[1, 2, 3]), vec![0x43, 0x42, 0x42, 0x4c, 3, 0, 0, 0, 1, 2, 3]);
        assert_eq!(record_bytes(7, &[]), vec![7, 0, 0, 0, 0, 0, 0, 0]);
        assert_eq!(only_tags(&[1, 2, 3, 4, 5]), vec![1, 2, 3, 4]);
        let f = Filters::default();
        assert!(f.keeps(9, &[], false));
        assert!(f.keeps(9, &[3, 9], false));
        assert!(!f.keeps(9, &[3], false));
        f.exclusive.store(true, Ordering::SeqCst);
        assert!(!f.keeps(9, &[], false));
        assert!(f.keeps(9, &[], true));
        f.exclusive.store(false, Ordering::SeqCst);
        f.held.store(true, Ordering::SeqCst);
        assert!(!f.keeps(9, &[], true));
        f.held.store(false, Ordering::SeqCst);
        f.closed.store(true, Ordering::SeqCst);
        assert!(!f.keeps(9, &[], true));
    }
}
