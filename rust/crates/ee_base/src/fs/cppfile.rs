//! A C++ `FSFile*` from Rust: the byte and 32-bit primitives of the C++ class (its virtual `Read`/`Write`), through
//! a table of one-statement C++ calls that File System.cpp registers once at start-up (`ee_fs_file_register`).
//! [`CppFile`] is an [`FsFile`] and an [`FsWrite`] over it, so any Rust body can read and write the C++ file it was
//! handed (the memory file's and the chunk's loads and saves, the strings' `Load`/`Save`, the path manager's save).
//!
//! The text calls are the `char*` overloads (`UString::Load`/`Save` used them), the byte calls the `unsigned char*`
//! ones; the C++ classes implement both the same way, but the calls stay the ones the C++ made.

use std::ffi::{c_char, c_long, c_ulong, c_void};
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::fs::file::{FsFile, FsWrite};

/// The C++ `FSFile`'s calls (File System.cpp): each the one virtual call it names; the counts are the C++
/// `unsigned long` out arguments.
#[repr(C)]
pub struct EeFsFileCalls {
    /// `Read(uint32_t&)`.
    pub read_u32: unsafe extern "C" fn(file: *mut c_void, value: *mut u32) -> i32,
    /// `Read(unsigned char*, length, read)`.
    pub read_bytes: unsafe extern "C" fn(file: *mut c_void, out: *mut u8, length: c_ulong, read: *mut c_ulong) -> i32,
    /// `Read(char*, length, read)`.
    pub read_text:
        unsafe extern "C" fn(file: *mut c_void, out: *mut c_char, length: c_ulong, read: *mut c_ulong) -> i32,
    /// `Write(const uint32_t&)`.
    pub write_u32: unsafe extern "C" fn(file: *mut c_void, value: u32) -> i32,
    /// `Write(const unsigned char*, length, written)`.
    pub write_bytes:
        unsafe extern "C" fn(file: *mut c_void, data: *const u8, length: c_ulong, written: *mut c_ulong) -> i32,
    /// `Write(const char*, length, written)`.
    pub write_text:
        unsafe extern "C" fn(file: *mut c_void, data: *const c_char, length: c_ulong, written: *mut c_ulong) -> i32,
    /// `Read(int32_t&)`.
    pub read_i32: unsafe extern "C" fn(file: *mut c_void, value: *mut i32) -> i32,
    /// `Read(float&)`.
    pub read_f32: unsafe extern "C" fn(file: *mut c_void, value: *mut f32) -> i32,
    /// `Write(const int32_t&)`.
    pub write_i32: unsafe extern "C" fn(file: *mut c_void, value: i32) -> i32,
    /// `Write(const float&)`.
    pub write_f32: unsafe extern "C" fn(file: *mut c_void, value: f32) -> i32,
}

static CALLS: AtomicPtr<EeFsFileCalls> = AtomicPtr::new(std::ptr::null_mut());

/// Registers the C++ `FSFile` calls (File System.cpp's static initializer); returns true.
///
/// # Safety
/// `calls` lives for the rest of the process.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_file_register(calls: *const EeFsFileCalls) -> bool {
    CALLS.store(calls as *mut EeFsFileCalls, Ordering::Release);
    true
}

/// A C++ `FSFile*` (any subclass), read and written through its virtuals.
#[derive(Clone, Copy, Debug)]
pub struct CppFile(pub *mut c_void);

impl CppFile {
    fn calls(&self) -> &'static EeFsFileCalls {
        let calls = CALLS.load(Ordering::Acquire);
        assert!(!calls.is_null(), "the C++ FSFile calls are not registered (ee_fs_file_register)");
        // SAFETY: registered once for the process (ee_fs_file_register's contract)
        unsafe { &*calls }
    }
    /// `Read(char*, length, read)`: the status and the count.
    pub fn read_text(&mut self, out: &mut [u8]) -> (i32, c_ulong) {
        let mut read: c_ulong = 0;
        // SAFETY: the file is the live C++ object this was made from; `out` is valid for its length
        let status =
            unsafe { (self.calls().read_text)(self.0, out.as_mut_ptr().cast(), out.len() as c_ulong, &mut read) };
        (status, read)
    }
    /// `Read(int32_t&)`.
    pub fn read_i32(&mut self, value: &mut i32) -> i32 {
        // SAFETY: as `read_text`
        unsafe { (self.calls().read_i32)(self.0, value) }
    }
    /// `Read(float&)`.
    pub fn read_f32(&mut self, value: &mut f32) -> i32 {
        // SAFETY: as `read_text`
        unsafe { (self.calls().read_f32)(self.0, value) }
    }
    /// `Write(const int32_t&)`.
    pub fn write_i32(&mut self, value: i32) -> i32 {
        // SAFETY: as `read_text`
        unsafe { (self.calls().write_i32)(self.0, value) }
    }
    /// `Write(const float&)`.
    pub fn write_f32(&mut self, value: f32) -> i32 {
        // SAFETY: as `read_text`
        unsafe { (self.calls().write_f32)(self.0, value) }
    }
    /// `Write(const char*, length, written)`: the status and the count.
    pub fn write_text(&mut self, data: &[u8]) -> (i32, c_ulong) {
        let mut written: c_ulong = 0;
        // SAFETY: as `read_text`
        let status =
            unsafe { (self.calls().write_text)(self.0, data.as_ptr().cast(), data.len() as c_ulong, &mut written) };
        (status, written)
    }
}

/// `FSFile::Read(long&)`: through a 32-bit copy that starts as the old value and is stored back even when the read
/// fails (`Read(int32_t&)`; 4 bytes on every data model).
///
/// # Safety
/// `file` is a live C++ `FSFile*`; `value` is valid.
#[no_mangle]
#[allow(clippy::unnecessary_cast)] // (c_long/c_ulong: 32 bits on Windows, 64 on LP64)
pub unsafe extern "C" fn ee_fs_read_long(file: *mut c_void, value: *mut c_long) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let mut v = *value as i32;
        let status = CppFile(file).read_i32(&mut v);
        *value = v as c_long;
        status as c_long
    }
}

/// `FSFile::Read(unsigned long&)`: as [`ee_fs_read_long`] through `Read(uint32_t&)`.
///
/// # Safety
/// As [`ee_fs_read_long`].
#[no_mangle]
#[allow(clippy::unnecessary_cast)] // (c_long/c_ulong: 32 bits on Windows, 64 on LP64)
pub unsafe extern "C" fn ee_fs_read_ulong(file: *mut c_void, value: *mut c_ulong) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let mut v = *value as u32;
        let status = CppFile(file).read_u32(&mut v);
        *value = v as c_ulong;
        status as c_long
    }
}

// (`c_ulong` is u32 on Windows, u64 on LP64)
#[allow(clippy::unnecessary_cast)]
impl FsFile for CppFile {
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        // SAFETY: as `read_text`
        unsafe { (self.calls().read_u32)(self.0, value) }
    }
    fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32) {
        let mut read: c_ulong = 0;
        // SAFETY: as `read_text`
        let status = unsafe { (self.calls().read_bytes)(self.0, out.as_mut_ptr(), out.len() as c_ulong, &mut read) };
        (status, read as u32)
    }
    fn read_text(&mut self, out: &mut [u8]) -> (i32, u32) {
        let (status, read) = CppFile::read_text(self, out);
        (status, read as u32)
    }
}

#[allow(clippy::unnecessary_cast)]
impl FsWrite for CppFile {
    fn write_u32(&mut self, value: u32) -> i32 {
        // SAFETY: as `read_text`
        unsafe { (self.calls().write_u32)(self.0, value) }
    }
    fn write_bytes(&mut self, data: &[u8]) -> (i32, u32) {
        let mut written: c_ulong = 0;
        // SAFETY: as `read_text`
        let status = unsafe { (self.calls().write_bytes)(self.0, data.as_ptr(), data.len() as c_ulong, &mut written) };
        (status, written as u32)
    }
    fn write_text(&mut self, data: &[u8]) -> (i32, u32) {
        let (status, written) = CppFile::write_text(self, data);
        (status, written as u32)
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;
    use std::sync::Mutex;

    /// The registered table's file in the tests: a byte vector with a read position, every call logged.
    pub(crate) struct MemFile {
        pub(crate) data: Vec<u8>,
        pub(crate) pos: usize,
        pub(crate) log: Vec<String>,
    }

    unsafe fn mem<'a>(f: *mut c_void) -> &'a mut MemFile {
        unsafe { &mut *(f as *mut MemFile) }
    }
    unsafe extern "C" fn read_u32(f: *mut c_void, v: *mut u32) -> i32 {
        let m = unsafe { mem(f) };
        m.log.push("read_u32".into());
        if m.pos + 4 > m.data.len() {
            return 0x12;
        }
        unsafe { *v = u32::from_le_bytes(m.data[m.pos..m.pos + 4].try_into().unwrap()) };
        m.pos += 4;
        0
    }
    unsafe fn read_any(f: *mut c_void, out: *mut u8, n: c_ulong, r: *mut c_ulong, what: &str) -> i32 {
        let m = unsafe { mem(f) };
        m.log.push(format!("{what} {n}"));
        let k = (m.data.len() - m.pos).min(n as usize);
        unsafe { std::ptr::copy_nonoverlapping(m.data[m.pos..].as_ptr(), out, k) };
        m.pos += k;
        unsafe { *r = k as c_ulong };
        0
    }
    unsafe extern "C" fn read_bytes(f: *mut c_void, out: *mut u8, n: c_ulong, r: *mut c_ulong) -> i32 {
        unsafe { read_any(f, out, n, r, "read_bytes") }
    }
    unsafe extern "C" fn read_text(f: *mut c_void, out: *mut c_char, n: c_ulong, r: *mut c_ulong) -> i32 {
        unsafe { read_any(f, out.cast(), n, r, "read_text") }
    }
    unsafe extern "C" fn write_u32(f: *mut c_void, v: u32) -> i32 {
        let m = unsafe { mem(f) };
        m.log.push(format!("write_u32 {v}"));
        m.data.extend_from_slice(&v.to_le_bytes());
        0
    }
    unsafe fn write_any(f: *mut c_void, d: *const u8, n: c_ulong, w: *mut c_ulong, what: &str) -> i32 {
        let m = unsafe { mem(f) };
        m.log.push(format!("{what} {n}"));
        m.data.extend_from_slice(unsafe { std::slice::from_raw_parts(d, n as usize) });
        unsafe { *w = n };
        0
    }
    unsafe extern "C" fn write_bytes(f: *mut c_void, d: *const u8, n: c_ulong, w: *mut c_ulong) -> i32 {
        unsafe { write_any(f, d, n, w, "write_bytes") }
    }
    unsafe extern "C" fn write_text(f: *mut c_void, d: *const c_char, n: c_ulong, w: *mut c_ulong) -> i32 {
        unsafe { write_any(f, d.cast(), n, w, "write_text") }
    }

    unsafe extern "C" fn read_i32(f: *mut c_void, v: *mut i32) -> i32 {
        let mut u = 0u32;
        let r = unsafe { read_u32(f, &mut u) };
        unsafe { mem(f) }.log.last_mut().unwrap().replace_range(.., "read_i32");
        if r == 0 {
            unsafe { *v = u as i32 };
        }
        r
    }
    unsafe extern "C" fn read_f32(f: *mut c_void, v: *mut f32) -> i32 {
        let mut u = 0u32;
        let r = unsafe { read_u32(f, &mut u) };
        unsafe { mem(f) }.log.last_mut().unwrap().replace_range(.., "read_f32");
        if r == 0 {
            unsafe { *v = f32::from_bits(u) };
        }
        r
    }
    unsafe extern "C" fn write_i32(f: *mut c_void, v: i32) -> i32 {
        let r = unsafe { write_u32(f, v as u32) };
        unsafe { mem(f) }.log.last_mut().unwrap().replace_range(.., &format!("write_i32 {v}"));
        r
    }
    unsafe extern "C" fn write_f32(f: *mut c_void, v: f32) -> i32 {
        let r = unsafe { write_u32(f, v.to_bits()) };
        unsafe { mem(f) }.log.last_mut().unwrap().replace_range(.., &format!("write_f32 {v}"));
        r
    }

    static TABLE: EeFsFileCalls = EeFsFileCalls {
        read_u32,
        read_bytes,
        read_text,
        write_u32,
        write_bytes,
        write_text,
        read_i32,
        read_f32,
        write_i32,
        write_f32,
    };
    static LOCK: Mutex<()> = Mutex::new(());

    /// Registers the test table and runs `f` on a file holding `data` (one test at a time).
    pub(crate) fn with_file<R>(data: &[u8], f: impl FnOnce(CppFile) -> R) -> (R, MemFile) {
        let _guard = LOCK.lock().unwrap_or_else(|e| e.into_inner());
        // SAFETY: a static table
        unsafe { ee_fs_file_register(&TABLE) };
        let mut m = MemFile { data: data.to_vec(), pos: 0, log: Vec::new() };
        let r = f(CppFile(&mut m as *mut MemFile as *mut c_void));
        (r, m)
    }

    #[test]
    fn a_long_is_read_through_a_32_bit_copy_kept_on_a_short_read() {
        let ((), m) = with_file(&[0xfe, 0xff, 0xff, 0xff, 1, 2], |f| {
            let mut v: c_long = 5;
            // SAFETY: a registered file and a valid value
            assert_eq!(unsafe { ee_fs_read_long(f.0, &mut v) }, 0);
            assert_eq!(v, -2);
            let mut u: c_ulong = 9;
            // SAFETY: as above
            assert_eq!(unsafe { ee_fs_read_ulong(f.0, &mut u) }, 0x12);
            assert_eq!(u, 9);
        });
        assert_eq!(m.log, ["read_i32", "read_u32"]);
    }

    #[test]
    fn the_calls_reach_the_registered_table() {
        let ((), m) = with_file(&[7, 0, 0, 0, b'a', b'b', b'c'], |mut f| {
            let mut v = 0;
            assert_eq!(f.read_u32(&mut v), 0);
            assert_eq!(v, 7);
            let mut two = [0u8; 2];
            assert_eq!(FsFile::read_bytes(&mut f, &mut two), (0, 2));
            assert_eq!(&two, b"ab");
            let mut rest = [0u8; 4];
            assert_eq!(CppFile::read_text(&mut f, &mut rest), (0, 1));
            assert_eq!(f.write_u32(9), 0);
            assert_eq!(FsWrite::write_bytes(&mut f, b"xy"), (0, 2));
            assert_eq!(CppFile::write_text(&mut f, b"z"), (0, 1));
        });
        assert_eq!(m.log, ["read_u32", "read_bytes 2", "read_text 4", "write_u32 9", "write_bytes 2", "write_text 1"]);
        assert_eq!(&m.data[7..], &[9, 0, 0, 0, b'x', b'y', b'z']);
    }
}
