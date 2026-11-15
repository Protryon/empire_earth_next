//! The op base's calls that need no C++ object, answered in Rust (net round 19): the clock (`WONTickCount`,
//! `time`), the CRT's random numbers (`rand`, `srand(time(NULL))`), an address resolved (`IPAddr::
//! SetWithDefaultPort`), the op's own file (the `FILE*` `RustServerOp::mFile` was: open, read, close), the files an
//! op measures (`stat`'s size, `WONFile`'s existence, size and modify time, the first character) and the web proxy
//! host (`HTTPGetOp::ReadProxyHostFile`, read once per process when the first HTTP get op is made). ServerOp.cpp's
//! `HostCall` answered these with one statement each; the game's ops are bound with [`ee_won_hostop_bind_native`]
//! (hostop.rs) and never send them to the C++ side. The drivers' stand-ins (bound with `ee_won_hostop_bind`) still
//! see every call, so the goldens of the ops' statements are unchanged; `won_serverop_test` compares these answers
//! with the original's (golden `golden_serverop.txt`).
//!
//! The file functions are the platform layer's (`PlatFOpen`: Windows-form paths and the CRT's text mode on every
//! system) and the CRT's stdio, as the C++ called them; `rand` and `srand` are the CRT's, so the sequence is the one
//! the game's other `rand` callers share.

use std::ffi::{c_uint, c_void, CString};
use std::sync::Mutex;

use crate::hostop::Obj;
use crate::hostop::{
    Reply, EE_WON_HOST_FILE_CLOSE, EE_WON_HOST_FILE_FIRST_CHAR, EE_WON_HOST_FILE_OPEN, EE_WON_HOST_FILE_READ,
    EE_WON_HOST_FILE_SIZE, EE_WON_HOST_FILE_STAT, EE_WON_HOST_HTTP_PROXY, EE_WON_HOST_RAND, EE_WON_HOST_RESOLVE,
    EE_WON_HOST_SRAND, EE_WON_HOST_TICK, EE_WON_HOST_TIME,
};

/// cbindgen:ignore
mod sys {
    use std::ffi::{c_char, c_int, c_uint, c_void};
    extern "C" {
        // the platform layer (ee_plat_sys, ee_plat_core)
        pub fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub fn PlatPathGetAttributes(path: *const c_char) -> u32;
        pub fn PlatFileOpen(path: *const c_char, access: u32) -> *mut c_void;
        pub fn PlatFileClose(file: *mut c_void) -> i32;
        pub fn PlatFileGetSize(file: *mut c_void) -> u32;
        pub fn PlatFileGetWriteTime(file: *mut c_void, time: *mut u64) -> i32;
        pub fn PlatTimeMilliseconds64() -> u64;
        // the CRT
        pub fn fclose(file: *mut c_void) -> c_int;
        pub fn fread(data: *mut c_void, size: usize, count: usize, file: *mut c_void) -> usize;
        pub fn fgetc(file: *mut c_void) -> c_int;
        pub fn rand() -> c_int;
        pub fn srand(seed: c_uint);
        #[cfg(windows)]
        pub fn _stat64i32(path: *const c_char, out: *mut c_void) -> c_int;
    }
}

const PLAT_ATTR_DIRECTORY: u32 = 0x10;
const PLAT_ATTR_INVALID: u32 = 0xFFFF_FFFF;
const PLAT_FILE_READ: u32 = 0x1;
/// 1601-01-01 to 1970-01-01 in 100 ns units (WONFile's modify time)
const FILETIME_1970: u64 = 116_444_736_000_000_000;

/// A C string of the bytes up to the first NUL (`std::string(data, len).c_str()`).
fn c_path(data: &[u8]) -> CString {
    let end = data.iter().position(|&b| b == 0).unwrap_or(data.len());
    // no NUL left in the slice
    CString::new(&data[..end]).unwrap_or_default()
}

/// `time(NULL)`.
fn now() -> i64 {
    match std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH) {
        Ok(d) => d.as_secs() as i64,
        Err(e) => -(e.duration().as_secs() as i64),
    }
}

fn num(n: i64) -> Option<Reply> {
    Some(Reply { obj: Obj::none(), num: n, addr: [0; 6], bytes: None })
}

fn bytes(b: Vec<u8>) -> Option<Reply> {
    Some(Reply { obj: Obj::none(), num: 0, addr: [0; 6], bytes: Some(b) })
}

/// `stat(path).st_size`, or -1 when it fails.
fn stat_size(path: &CString) -> i64 {
    #[cfg(windows)]
    {
        // `struct _stat64i32` (48 bytes): the size, an `_off_t`, at +20
        let mut buf = [0u64; 8];
        // SAFETY: a C string; the buffer is larger than the CRT's struct
        if unsafe { sys::_stat64i32(path.as_ptr(), buf.as_mut_ptr().cast()) } != 0 {
            return -1;
        }
        let raw: &[u8; 64] = bytemuck_bytes(&buf);
        i64::from(i32::from_le_bytes([raw[20], raw[21], raw[22], raw[23]]))
    }
    #[cfg(not(windows))]
    {
        use std::os::unix::ffi::OsStrExt;
        match std::fs::metadata(std::ffi::OsStr::from_bytes(path.as_bytes())) {
            Ok(m) => m.len() as i64,
            Err(_) => -1,
        }
    }
}

#[cfg(windows)]
fn bytemuck_bytes(buf: &[u64; 8]) -> &[u8; 64] {
    // SAFETY: the same 64 bytes, u8 has no alignment
    unsafe { &*(buf as *const [u64; 8] as *const [u8; 64]) }
}

/// `WONFile(path)`'s existence (0), size (1) or modify time (2) (`CalcFileInfo`).
fn won_file(path: &[u8], which: i64) -> i64 {
    let p = c_path(&crate::path::given(path));
    // SAFETY: a C string
    let attributes = unsafe { sys::PlatPathGetAttributes(p.as_ptr()) };
    if attributes == PLAT_ATTR_INVALID {
        return 0;
    }
    if which == 0 {
        return 1;
    }
    let (mut size, mut modify) = (0u32, 0i64);
    if attributes & PLAT_ATTR_DIRECTORY == 0 {
        // SAFETY: a C string; the file closed below
        let file = unsafe { sys::PlatFileOpen(p.as_ptr(), PLAT_FILE_READ) };
        if !file.is_null() {
            // SAFETY: the open file
            unsafe {
                let s = sys::PlatFileGetSize(file);
                let mut time = 0u64;
                if s != 0xFFFF_FFFF {
                    size = s;
                }
                if sys::PlatFileGetWriteTime(file, &mut time) != 0 && time >= FILETIME_1970 {
                    // `static_cast<long>`
                    modify = ((time - FILETIME_1970) / 10_000_000) as std::ffi::c_long as i64;
                }
                sys::PlatFileClose(file);
            }
        }
    }
    if which == 1 {
        i64::from(size)
    } else {
        modify
    }
}

/// `WONFile(path).GetModifyTime()` (0: no file).
pub(crate) fn won_file_modify_time(path: &[u8]) -> i64 {
    won_file(path, 2)
}

/// `ServerContext`'s generator (its `CopyAddresses`' shuffle): `srand(time(NULL))` when seeding (0), else `rand()`,
/// the CRT's.
pub(crate) fn crt_srand_and_rand(seed: bool) -> i32 {
    if seed {
        // SAFETY: the CRT's
        unsafe { sys::srand(now() as c_uint) };
        0
    } else {
        // SAFETY: the CRT's
        unsafe { sys::rand() }
    }
}

/// The first character of a text-mode open: -2 for no file, -1 for an empty one.
fn first_char(path: &[u8]) -> i64 {
    let p = c_path(path);
    // SAFETY: C strings; the file closed below
    let file = unsafe { sys::PlatFOpen(p.as_ptr(), c"r".as_ptr()) };
    if file.is_null() {
        return -2;
    }
    // SAFETY: the open file
    let c = unsafe { sys::fgetc(file) };
    // SAFETY: as above
    unsafe { sys::fclose(file) };
    if c < 0 {
        -1
    } else {
        i64::from(c)
    }
}

/// The web proxy host (`HTTPGetOp::mStaticProxyHost`) and whether its file was read.
static PROXY: Mutex<(bool, Vec<u8>)> = Mutex::new((false, Vec::new()));

/// `HTTPGetOp::ReadProxyHostFile(false)` as the constructors ran it: `_wonwebproxy.txt` in `dir` (the WON
/// directory, with its separator), its first line (`fgets` of up to 499 characters, cut at the newline), once per
/// process.
pub(crate) fn read_proxy_file(dir: &[u8]) {
    let mut proxy = PROXY.lock().unwrap_or_else(|e| e.into_inner());
    if proxy.0 {
        return;
    }
    proxy.0 = true;
    let mut name = dir.to_vec();
    name.extend_from_slice(b"_wonwebproxy.txt");
    let p = c_path(&name);
    // SAFETY: C strings; the file closed below
    let file = unsafe { sys::PlatFOpen(p.as_ptr(), c"r".as_ptr()) };
    if file.is_null() {
        return;
    }
    let mut line = Vec::new();
    while line.len() < 499 {
        // SAFETY: the open file
        let c = unsafe { sys::fgetc(file) };
        if c < 0 {
            break;
        }
        line.push(c as u8);
        if c == i32::from(b'\n') {
            break;
        }
    }
    // SAFETY: as above
    unsafe { sys::fclose(file) };
    // the buffer read as a C string, then cut at its newline
    if let Some(end) = line.iter().position(|&b| b == 0) {
        line.truncate(end);
    }
    if let Some(end) = line.iter().position(|&b| b == b'\n') {
        line.truncate(end);
    }
    proxy.1 = line;
}

/// The op's file (a `FILE*`, 0: none), closed when the op goes.
#[derive(Default)]
pub(crate) struct OpFile(Mutex<usize>);

impl OpFile {
    fn take(&self) -> usize {
        std::mem::take(&mut *self.0.lock().unwrap_or_else(|e| e.into_inner()))
    }
    fn close(&self) {
        let f = self.take();
        if f != 0 {
            // SAFETY: the file this op opened
            unsafe { sys::fclose(f as *mut c_void) };
        }
    }
    fn open(&self, path: &[u8]) -> bool {
        self.close();
        let p = c_path(path);
        // SAFETY: C strings
        let f = unsafe { sys::PlatFOpen(p.as_ptr(), c"rb".as_ptr()) } as usize;
        *self.0.lock().unwrap_or_else(|e| e.into_inner()) = f;
        f != 0
    }
    fn read(&self, length: i64) -> Vec<u8> {
        let f = *self.0.lock().unwrap_or_else(|e| e.into_inner());
        if f == 0 {
            return Vec::new();
        }
        let mut buf = vec![0u8; length.max(0) as usize];
        // SAFETY: the open file and a buffer of the length
        let n = unsafe { sys::fread(buf.as_mut_ptr().cast(), 1, buf.len(), f as *mut c_void) };
        buf.truncate(n);
        buf
    }
}

impl Drop for OpFile {
    fn drop(&mut self) {
        self.close();
    }
}

/// The answer to a call that needs no C++ object, or None for the C++ side's calls.
pub(crate) fn answer(file: &OpFile, code: i32, n: [i64; 2], data: &[u8]) -> Option<Reply> {
    match code {
        // `(DWORD)WONTickCount()`
        // SAFETY: the platform clock
        EE_WON_HOST_TICK => num(i64::from(unsafe { sys::PlatTimeMilliseconds64() } as u32)),
        // SAFETY: the CRT's
        EE_WON_HOST_RAND => num(i64::from(unsafe { sys::rand() })),
        EE_WON_HOST_SRAND => {
            // SAFETY: the CRT's
            unsafe { sys::srand(now() as c_uint) };
            num(0)
        }
        EE_WON_HOST_TIME => num(now()),
        EE_WON_HOST_RESOLVE => {
            let mut a = crate::ipobj::EeWonIpAddr::default();
            let ok = a.set_with_default_port(data, n[0] as u16);
            let mut six = [0u8; 6];
            six[..2].copy_from_slice(&a.sa.mPort.to_ne_bytes());
            six[2..].copy_from_slice(&a.sa.mAddress.to_ne_bytes());
            Some(Reply { obj: Obj::none(), num: i64::from(ok), addr: six, bytes: None })
        }
        EE_WON_HOST_FILE_OPEN => num(i64::from(file.open(data))),
        EE_WON_HOST_FILE_READ => bytes(file.read(n[0])),
        EE_WON_HOST_FILE_CLOSE => {
            file.close();
            num(0)
        }
        EE_WON_HOST_FILE_SIZE => num(stat_size(&c_path(data))),
        EE_WON_HOST_FILE_STAT => num(won_file(data, n[0].clamp(0, 2))),
        EE_WON_HOST_FILE_FIRST_CHAR => num(first_char(data)),
        EE_WON_HOST_HTTP_PROXY => bytes(PROXY.lock().unwrap_or_else(|e| e.into_inner()).1.clone()),
        _ => None,
    }
}

/// `HTTPGetOp`'s constructors' proxy file read (`dir`: `WONAPICore::GetDefaultFileDirectory()`, `len` bytes).
///
/// # Safety
/// `dir` holds `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_http_proxy_read(dir: *const u8, len: usize) {
    // SAFETY: the caller's bytes
    read_proxy_file(unsafe { crate::ffi::bytes(dir, len) });
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(name: &str) -> std::path::PathBuf {
        let d = std::env::temp_dir().join(format!("ee_won_hostnative_{}_{name}", std::process::id()));
        let _ = std::fs::create_dir_all(&d);
        d
    }

    #[test]
    fn the_op_file_reads_binary_and_closes_on_reopen() {
        let d = scratch("file");
        let a = d.join("a.bin");
        std::fs::write(&a, b"ab\r\ncd").unwrap();
        let f = OpFile::default();
        assert!(answer(&f, EE_WON_HOST_FILE_READ, [4, 0], &[]).unwrap().bytes == Some(Vec::new()));
        let p = a.to_str().unwrap().as_bytes().to_vec();
        assert_eq!(answer(&f, EE_WON_HOST_FILE_OPEN, [0, 0], &p).unwrap().num, 1);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_READ, [3, 0], &[]).unwrap().bytes.unwrap(), b"ab\r");
        assert_eq!(answer(&f, EE_WON_HOST_FILE_READ, [30, 0], &[]).unwrap().bytes.unwrap(), b"\ncd");
        assert_eq!(answer(&f, EE_WON_HOST_FILE_READ, [3, 0], &[]).unwrap().bytes.unwrap(), b"");
        assert_eq!(answer(&f, EE_WON_HOST_FILE_OPEN, [0, 0], b"no such file here").unwrap().num, 0);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_READ, [3, 0], &[]).unwrap().bytes.unwrap(), b"");
        answer(&f, EE_WON_HOST_FILE_CLOSE, [0, 0], &[]);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_SIZE, [0, 0], &p).unwrap().num, 6);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_SIZE, [0, 0], b"no such file here").unwrap().num, -1);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_STAT, [0, 0], &p).unwrap().num, 1);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_STAT, [1, 0], &p).unwrap().num, 6);
        assert!(answer(&f, EE_WON_HOST_FILE_STAT, [2, 0], &p).unwrap().num > 1_600_000_000);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_STAT, [1, 0], b"no such file here").unwrap().num, 0);
        assert_eq!(answer(&f, EE_WON_HOST_FILE_FIRST_CHAR, [0, 0], &p).unwrap().num, i64::from(b'a'));
        assert_eq!(answer(&f, EE_WON_HOST_FILE_FIRST_CHAR, [0, 0], b"no such file here").unwrap().num, -2);
        let _ = std::fs::remove_dir_all(&d);
    }

    #[test]
    fn the_proxy_file_is_read_once_its_first_line() {
        let d = scratch("proxy");
        std::fs::write(d.join("_wonwebproxy.txt"), b"host:81\r\nsecond\n").unwrap();
        let mut dir = d.to_str().unwrap().as_bytes().to_vec();
        dir.push(std::path::MAIN_SEPARATOR as u8);
        read_proxy_file(&dir);
        std::fs::write(d.join("_wonwebproxy.txt"), b"changed\n").unwrap();
        read_proxy_file(&dir);
        let f = OpFile::default();
        assert_eq!(answer(&f, EE_WON_HOST_HTTP_PROXY, [0, 0], &[]).unwrap().bytes.unwrap(), b"host:81");
        let _ = std::fs::remove_dir_all(&d);
    }

    #[test]
    fn addresses_resolve_without_the_cpp_side() {
        let f = OpFile::default();
        let r = answer(&f, EE_WON_HOST_RESOLVE, [80, 0], b"127.0.0.1:15101").unwrap();
        assert_eq!(r.num, 1);
        assert_eq!(r.addr, [0x3a, 0xfd, 127, 0, 0, 1]);
        let r = answer(&f, EE_WON_HOST_RESOLVE, [0x1234, 0], b"10.1.2.3").unwrap();
        assert_eq!(r.addr, [0x12, 0x34, 10, 1, 2, 3]);
        assert!(answer(&f, EE_WON_HOST_KILL_ALL_CODE, [0, 0], &[]).is_none());
    }

    const EE_WON_HOST_KILL_ALL_CODE: i32 = crate::hostop::EE_WON_HOST_KILL_ALL;

    #[test]
    fn rand_is_the_crts_sequence() {
        let f = OpFile::default();
        // SAFETY: the CRT's
        unsafe { sys::srand(4242) };
        let a = answer(&f, EE_WON_HOST_RAND, [0, 0], &[]).unwrap().num;
        // SAFETY: as above
        unsafe { sys::srand(4242) };
        // SAFETY: as above
        assert_eq!(a, i64::from(unsafe { sys::rand() }));
        let t = answer(&f, EE_WON_HOST_TIME, [0, 0], &[]).unwrap().num;
        assert!((t - now()).abs() <= 1);
    }
}
