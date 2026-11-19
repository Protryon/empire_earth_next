//! TEST ONLY: the simulation capture's file (`ee/EESimCapture.h`; `tools/sim_capture.py`). With
//! `EE_SIM_CAPTURE=<absolute file>` a game appends one binary record per call of a captured leaf: a u32 tag, a u32
//! payload size, the payload. Records are written whole under a lock. Off (the default), `active()` is one cached
//! test. This was `EESimCapture.cpp` (the game's leaves round 1): the same filters, in the same order.
//!
//! The filters: `only` (this thread keeps only up to four tags), `exclusive` (every other thread's records are
//! dropped), `close` (no record of any thread after it), `hold` (no record while on).

use std::cell::Cell;
use std::fs::File;
use std::io::{BufWriter, Write};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Mutex, OnceLock};

/// The FNV-1a 64 basis the captures' digests use.
/// cbindgen:ignore
pub const FNV_BASIS: u64 = 0xcbf2_9ce4_8422_2325;
/// The FNV-1a 64 prime.
/// cbindgen:ignore
pub const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;

/// FNV-1a 64 of `bytes`, continuing `hash`.
pub fn fnv(mut hash: u64, bytes: &[u8]) -> u64 {
    for &b in bytes {
        hash = (hash ^ u64::from(b)).wrapping_mul(FNV_PRIME);
    }
    hash
}

static EXCLUSIVE: AtomicBool = AtomicBool::new(false);
static CLOSED: AtomicBool = AtomicBool::new(false);
static HELD: AtomicBool = AtomicBool::new(false);

thread_local! {
    static ONLY_TAGS: Cell<[u32; 4]> = const { Cell::new([0; 4]) };
    static ONLY_COUNT: Cell<u32> = const { Cell::new(0) };
    static EXCLUSIVE_OWNER: Cell<bool> = const { Cell::new(false) };
}

type Sink = Mutex<BufWriter<File>>;

#[cfg(not(test))]
extern "C" fn flush_at_exit() {
    flush();
}

/// The file, opened once at the first captured call (the game changes into its data root first: the path is
/// absolute). Flushed at the process's exit, as the C library flushed its stream.
fn sink() -> Option<&'static Sink> {
    static SINK: OnceLock<Option<Sink>> = OnceLock::new();
    SINK.get_or_init(|| {
        let path = std::env::var_os("EE_SIM_CAPTURE").filter(|p| !p.is_empty())?;
        let file = File::create(path).ok()?;
        #[cfg(not(test))]
        // SAFETY: registers a plain function with the C library's exit list
        unsafe {
            libc::atexit(flush_at_exit);
        }
        Some(Mutex::new(BufWriter::with_capacity(1 << 16, file)))
    })
    .as_ref()
}

fn lock(s: &'static Sink) -> std::sync::MutexGuard<'static, BufWriter<File>> {
    s.lock().unwrap_or_else(|e| e.into_inner())
}

/// `EESimCaptureRecord::Active()`: `EE_SIM_CAPTURE` is set and the file opened.
pub fn active() -> bool {
    sink().is_some()
}

fn dropped() -> bool {
    CLOSED.load(Ordering::SeqCst)
        || HELD.load(Ordering::SeqCst)
        || (EXCLUSIVE.load(Ordering::SeqCst) && !EXCLUSIVE_OWNER.with(Cell::get))
}

fn only_keeps(tag: u32) -> bool {
    let count = ONLY_COUNT.with(Cell::get) as usize;
    count == 0 || ONLY_TAGS.with(Cell::get)[..count].contains(&tag)
}

/// `Only(tags)`: while not empty, this thread's records of any other tag are dropped (up to four tags; a 0 alone
/// clears it, as `Only(0)` did).
pub fn only(tags: &[u32]) {
    let mut t = [0u32; 4];
    let n = tags.len().min(4);
    t[..n].copy_from_slice(&tags[..n]);
    ONLY_TAGS.with(|c| c.set(t));
    ONLY_COUNT.with(|c| c.set(n as u32));
}

/// `Exclusive(on)`: while on, every other thread's records are dropped.
pub fn exclusive(on: bool) {
    EXCLUSIVE_OWNER.with(|c| c.set(on));
    EXCLUSIVE.store(on, Ordering::SeqCst);
}

/// `Close()`: no record of any thread after it.
pub fn close() {
    CLOSED.store(true, Ordering::SeqCst);
}

/// `Hold(on)`: while on, no record of any thread is written.
pub fn hold(on: bool) {
    HELD.store(on, Ordering::SeqCst);
}

/// `Keeps(tag)`: whether a record of `tag` committed now on this thread would be written.
pub fn keeps(tag: u32) -> bool {
    sink().is_some() && !dropped() && only_keeps(tag)
}

/// `Commit()`: appends the record (the tag, the payload's size, the payload) unless a filter drops it.
pub fn commit(tag: u32, payload: &[u8]) {
    let Some(s) = sink() else { return };
    if dropped() || !only_keeps(tag) {
        return;
    }
    let mut f = lock(s);
    let _ = f.write_all(&tag.to_le_bytes());
    let _ = f.write_all(&(payload.len() as u32).to_le_bytes());
    let _ = f.write_all(payload);
}

/// `Flush()`: before the process ends.
pub fn flush() {
    if let Some(s) = sink() {
        let _ = lock(s).flush();
    }
}

/// A record being built (`EESimCaptureRecord`): little-endian fields.
#[derive(Debug, Default, Clone)]
pub struct Record {
    pub tag: u32,
    pub data: Vec<u8>,
}

impl Record {
    pub fn new(tag: u32) -> Record {
        Record { tag, data: Vec::new() }
    }
    pub fn u32(&mut self, v: u32) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    pub fn i32(&mut self, v: i32) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    pub fn f32(&mut self, v: f32) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    pub fn u64(&mut self, v: u64) {
        self.data.extend_from_slice(&v.to_le_bytes());
    }
    pub fn bytes(&mut self, b: &[u8]) {
        self.data.extend_from_slice(b);
    }
    pub fn commit(&self) {
        commit(self.tag, &self.data);
    }
}

// ---- the C interface (ee/EESimCapture.h's inline class forwards here)

/// `EESimCaptureRecord::Active()`
#[no_mangle]
pub extern "C" fn ee_sim_capture_active() -> bool {
    active()
}

/// `EESimCaptureRecord::Flush()`
#[no_mangle]
pub extern "C" fn ee_sim_capture_flush() {
    flush()
}

/// `EESimCaptureRecord::Only(tags, count)` (`tags` may be null when `count` is 0)
///
/// # Safety
/// `tags` is readable for `count` words.
#[no_mangle]
pub unsafe extern "C" fn ee_sim_capture_only(tags: *const u32, count: u32) {
    let n = (count as usize).min(4);
    if n == 0 || tags.is_null() {
        only(&[]);
    } else {
        // SAFETY: the caller's words, readable for count (at most four are read)
        only(unsafe { std::slice::from_raw_parts(tags, n) });
    }
}

/// `EESimCaptureRecord::Exclusive(on)`
#[no_mangle]
pub extern "C" fn ee_sim_capture_exclusive(on: bool) {
    exclusive(on)
}

/// `EESimCaptureRecord::Close()`
#[no_mangle]
pub extern "C" fn ee_sim_capture_close() {
    close()
}

/// `EESimCaptureRecord::Hold(on)`
#[no_mangle]
pub extern "C" fn ee_sim_capture_hold(on: bool) {
    hold(on)
}

/// `EESimCaptureRecord::Keeps(tag)`
#[no_mangle]
pub extern "C" fn ee_sim_capture_keeps(tag: u32) -> bool {
    keeps(tag)
}

/// `EESimCaptureRecord::Commit()`: the record `tag` with `size` bytes of payload.
///
/// # Safety
/// `data` is readable for `size` bytes (or null with `size` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_sim_capture_commit(tag: u32, data: *const u8, size: usize) {
    if data.is_null() || size == 0 {
        commit(tag, &[]);
    } else {
        // SAFETY: the caller's bytes, readable for size
        commit(tag, unsafe { std::slice::from_raw_parts(data, size) });
    }
}

// ---- the hulls' and the areas' geometry records (lle/U2DPolygon.h: HULL, AREA; was EEAIContinentCapture.cpp)
//
// One open record and the scope depth per thread: only the outermost captured call of a thread records.

thread_local! {
    static GEOM_DEPTH: Cell<i64> = const { Cell::new(0) };
    static GEOM_RECORD: std::cell::RefCell<Option<Record>> = const { std::cell::RefCell::new(None) };
}

fn geom(f: impl FnOnce(&mut Record)) {
    GEOM_RECORD.with(|r| {
        if let Some(rec) = r.borrow_mut().as_mut() {
            f(rec);
        }
    });
}

/// The FNV-1a 32 of points (x, y as 32-bit words): a hull's identity in the geometry records.
pub fn geom_points_hash(points: impl IntoIterator<Item = (i32, i32)>) -> u32 {
    let mut hash: u32 = 0x811c_9dc5;
    for (x, y) in points {
        for b in x.to_le_bytes().into_iter().chain(y.to_le_bytes()) {
            hash = (hash ^ u32::from(b)).wrapping_mul(0x0100_0193);
        }
    }
    hash
}

/// `EEGeomCaptureScope`'s constructor: whether this scope is the outer one (the capture is on and no other scope is
/// alive on this thread).
#[no_mangle]
pub extern "C" fn ee_sim_capture_geom_enter() -> bool {
    let depth = GEOM_DEPTH.with(|d| {
        let v = d.get();
        d.set(v + 1);
        v
    });
    depth == 0 && active()
}

/// `EEGeomCaptureScope`'s destructor.
#[no_mangle]
pub extern "C" fn ee_sim_capture_geom_leave() {
    GEOM_DEPTH.with(|d| d.set(d.get() - 1));
}

/// `EEGeomCaptureBegin`: a new record of `tag` (an open one is dropped), its first word `kind`.
#[no_mangle]
pub extern "C" fn ee_sim_capture_geom_begin(tag: u32, kind: u32) {
    let mut rec = Record::new(tag);
    rec.u32(kind);
    GEOM_RECORD.with(|r| *r.borrow_mut() = Some(rec));
}

/// `EEGeomCaptureValue`
#[no_mangle]
pub extern "C" fn ee_sim_capture_geom_value(value: i32) {
    geom(|r| r.i32(value));
}

/// `EEGeomCaptureFloat`
#[no_mangle]
pub extern "C" fn ee_sim_capture_geom_float(value: f32) {
    geom(|r| r.f32(value));
}

/// `EEGeomCapturePoint`
#[no_mangle]
pub extern "C" fn ee_sim_capture_geom_point(x: i32, y: i32) {
    geom(|r| {
        r.i32(x);
        r.i32(y);
    });
}

/// `EEGeomCapturePoints`: the count, then (`whole`) every point or the FNV-1a 32 of the points. `xy` holds `count`
/// points as two C `long`s each (`U2DPoint<long>`).
///
/// # Safety
/// `xy` is readable for `2 * count` longs (or null with `count` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_sim_capture_geom_points(xy: *const std::ffi::c_long, count: usize, whole: bool) {
    let values: &[std::ffi::c_long] = if xy.is_null() || count == 0 {
        &[]
    } else {
        // SAFETY: the caller's points
        unsafe { std::slice::from_raw_parts(xy, count * 2) }
    };
    // (a long as its low 32 bits, as the C++ `(int)` casts)
    let points = (0..values.len() / 2).map(|k| (values[2 * k] as i64 as i32, values[2 * k + 1] as i64 as i32));
    geom(|r| {
        r.i32(count as i32);
        if whole {
            for (x, y) in points {
                r.i32(x);
                r.i32(y);
            }
        } else {
            r.u32(geom_points_hash(points));
        }
    });
}

/// `EEGeomCaptureCommit`: writes the open record and closes it.
#[no_mangle]
pub extern "C" fn ee_sim_capture_geom_commit() {
    if let Some(rec) = GEOM_RECORD.with(|r| r.borrow_mut().take()) {
        rec.commit();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fnv_matches_the_reference_values() {
        assert_eq!(fnv(FNV_BASIS, b""), 0xcbf2_9ce4_8422_2325);
        assert_eq!(fnv(FNV_BASIS, b"a"), 0xaf63_dc4c_8601_ec8c);
        assert_eq!(fnv(FNV_BASIS, b"foobar"), 0x8594_4171_f739_67e8);
    }

    #[test]
    fn only_filters_this_thread() {
        only(&[]);
        assert!(only_keeps(7));
        only(&[1, 2]);
        assert!(only_keeps(2) && !only_keeps(3));
        only(&[1, 2, 3, 4, 5]);
        assert!(only_keeps(4) && !only_keeps(5));
        std::thread::spawn(|| assert!(only_keeps(9))).join().unwrap();
        only(&[]);
        assert!(only_keeps(9));
    }

    #[test]
    fn record_fields_are_little_endian() {
        let mut r = Record::new(0x4b43_5756);
        r.u32(1);
        r.i32(-2);
        r.f32(1.0);
        r.u64(0x0102_0304_0506_0708);
        assert_eq!(
            r.data,
            [1, 0, 0, 0, 0xfe, 0xff, 0xff, 0xff, 0, 0, 0x80, 0x3f, 8, 7, 6, 5, 4, 3, 2, 1]
        );
    }

    #[test]
    fn geometry_records_hash_or_list_points() {
        let pts: [std::ffi::c_long; 4] = [1, 2, -3, 4];
        ee_sim_capture_geom_begin(0x4c4c_5548, 6);
        ee_sim_capture_geom_value(-1);
        // SAFETY: two points
        unsafe { ee_sim_capture_geom_points(pts.as_ptr(), 2, false) };
        // SAFETY: two points
        unsafe { ee_sim_capture_geom_points(pts.as_ptr(), 2, true) };
        ee_sim_capture_geom_point(5, 6);
        let rec = GEOM_RECORD.with(|r| r.borrow_mut().take()).unwrap();
        let mut want = Record::new(0x4c4c_5548);
        want.u32(6);
        want.i32(-1);
        want.i32(2);
        want.u32(geom_points_hash([(1, 2), (-3, 4)]));
        for v in [2, 1, 2, -3, 4, 5, 6] {
            want.i32(v);
        }
        assert_eq!(rec.data, want.data);
        // FNV-1a 32 of no bytes is the basis; of the bytes 1,0,0,0, 2,0,0,0 the reference fold
        assert_eq!(geom_points_hash([]), 0x811c_9dc5);
        let mut h: u32 = 0x811c_9dc5;
        for b in [1u8, 0, 0, 0, 2, 0, 0, 0] {
            h = (h ^ u32::from(b)).wrapping_mul(0x0100_0193);
        }
        assert_eq!(geom_points_hash([(1, 2)]), h);
        // the scopes: only the outermost is outer (never without the capture)
        assert!(!ee_sim_capture_geom_enter() || active());
        assert!(!ee_sim_capture_geom_enter());
        ee_sim_capture_geom_leave();
        ee_sim_capture_geom_leave();
        assert_eq!(GEOM_DEPTH.with(Cell::get), 0);
    }

    #[test]
    fn off_without_the_variable() {
        // the test runner sets no EE_SIM_CAPTURE: nothing is kept, a commit is a no-op
        if std::env::var_os("EE_SIM_CAPTURE").is_none() {
            assert!(!active() && !keeps(1));
            commit(1, b"x");
        }
    }
}
