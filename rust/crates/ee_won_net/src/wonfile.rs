//! WONFile whole (net round 27): a file path with its parts and stat details cached on first use, as one Rust
//! object (WONCommon/WONFile.cpp deleted; the C++ class holds an `EeWonFile` and forwards). The path logic is
//! path.rs's; the platform calls (attributes, the file's size and write time, delete, the read-only flag, the
//! directory, the current directory) are the platform layer's, as the C++ made them. The cache flags are the
//! original's, including its quirks: `Reset` leaves the name without extension and the stat values, and
//! `NormalizePath` drops only the directory's cache.

use std::ffi::{c_char, c_void, CString};

use crate::ffi::{bytes, give, EeWonByteSink};
use crate::ffi_parse::CrtCtype;
use crate::path;

/// cbindgen:ignore
mod sys {
    use std::ffi::c_char;
    use std::ffi::c_void;
    extern "C" {
        pub fn PlatPathGetAttributes(path: *const c_char) -> u32;
        pub fn PlatPathSetAttributes(path: *const c_char, attributes: u32) -> i32;
        pub fn PlatDeleteFile(path: *const c_char) -> i32;
        pub fn PlatCreateDirectory(path: *const c_char) -> i32;
        pub fn PlatGetCurrentDirectory(path: *mut c_char, size: u32) -> u32;
        pub fn PlatFileOpen(path: *const c_char, access: u32) -> *mut c_void;
        pub fn PlatFileClose(file: *mut c_void) -> i32;
        pub fn PlatFileGetSize(file: *mut c_void) -> u32;
        pub fn PlatFileGetWriteTime(file: *mut c_void, time: *mut u64) -> i32;
    }
}

const ATTR_READONLY: u32 = 0x01;
const ATTR_DIRECTORY: u32 = 0x10;
const ATTR_NORMAL: u32 = 0x80;
const ATTR_INVALID: u32 = 0xFFFF_FFFF;
const FILE_READ: u32 = 0x1;
const MAX_PATH: usize = 260;
/// 1601-01-01 to 1970-01-01 in 100 ns units
const FILETIME_1970: u64 = 116_444_736_000_000_000;

fn c_str(s: &[u8]) -> CString {
    let end = s.iter().position(|&b| b == 0).unwrap_or(s.len());
    CString::new(&s[..end]).unwrap_or_default()
}

/// The current directory (`PlatGetCurrentDirectory` into a `PLAT_MAX_PATH` buffer; None when it fails or fills it).
fn current_directory() -> Option<Vec<u8>> {
    let mut buf = [0u8; MAX_PATH];
    // SAFETY: a writable buffer of its size
    let n = unsafe { sys::PlatGetCurrentDirectory(buf.as_mut_ptr() as *mut c_char, MAX_PATH as u32) } as usize;
    (n != 0 && n < MAX_PATH).then(|| buf[..n].to_vec())
}

/// `WONFile`'s members (0x70 bytes in the original).
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct EeWonFile {
    pub path: Vec<u8>,
    pub directory: Vec<u8>,
    pub name: Vec<u8>,
    pub name_no_extension: Vec<u8>,
    pub extension: Vec<u8>,
    pub full_path: Vec<u8>,
    pub size: u32,
    pub modify_time: i64,
    pub exists: bool,
    pub read_only: bool,
    pub is_dir: bool,
    pub directory_valid: bool,
    pub name_valid: bool,
    pub extension_valid: bool,
    pub stat_valid: bool,
    pub full_path_valid: bool,
}

/// The original threw `std::out_of_range` (a `substr` past the end).
pub use path::OutOfRange;

impl EeWonFile {
    /// `WONFile(path)`.
    pub fn new(p: &[u8]) -> EeWonFile {
        let mut f = EeWonFile::default();
        f.set_path(p);
        f
    }

    /// `Reset`.
    pub fn reset(&mut self) {
        self.path.clear();
        self.directory.clear();
        self.name.clear();
        self.extension.clear();
        self.full_path.clear();
        self.directory_valid = false;
        self.name_valid = false;
        self.extension_valid = false;
        self.stat_valid = false;
        self.full_path_valid = false;
        self.is_dir = false;
    }

    /// `SetFilePath`: reset, then the path with '/' turned into '\'.
    pub fn set_path(&mut self, p: &[u8]) {
        self.reset();
        self.path = path::given(p);
    }

    /// `CalcFileInfo`: existence, read-only and directory flags from the attributes, a file's size and modify time
    /// (seconds since 1970) from the open file.
    pub fn calc_info(&mut self) {
        self.stat_valid = true;
        let p = c_str(&self.path);
        // SAFETY: a C string
        let attributes = unsafe { sys::PlatPathGetAttributes(p.as_ptr()) };
        self.size = 0;
        self.modify_time = 0;
        if attributes == ATTR_INVALID {
            self.exists = false;
            self.read_only = false;
            return;
        }
        self.exists = true;
        self.read_only = attributes & ATTR_READONLY != 0;
        self.is_dir = attributes & ATTR_DIRECTORY != 0;
        if self.is_dir {
            return;
        }
        // SAFETY: a C string; the file closed below
        let file = unsafe { sys::PlatFileOpen(p.as_ptr(), FILE_READ) };
        if file.is_null() {
            return;
        }
        // SAFETY: the open file
        unsafe {
            let size = sys::PlatFileGetSize(file);
            let mut time = 0u64;
            if size != 0xFFFF_FFFF {
                self.size = size;
            }
            if sys::PlatFileGetWriteTime(file, &mut time) != 0 && time >= FILETIME_1970 {
                // `static_cast<long>`
                self.modify_time = ((time - FILETIME_1970) / 10_000_000) as std::ffi::c_long as i64;
            }
            sys::PlatFileClose(file);
        }
    }

    fn stat(&mut self) {
        if !self.stat_valid {
            self.calc_info();
        }
    }

    /// `CalcFileDirectory`: split at the last '\' (the directory left as it was without one).
    pub fn calc_directory(&mut self) {
        let sep = path::directory_split(&self.path);
        if let Some(i) = sep {
            self.directory = self.path[..i].to_vec();
        }
        self.name = self.path[sep.map_or(0, |i| i + 1)..].to_vec();
        self.directory_valid = true;
        self.name_valid = true;
    }

    /// `CalcFullPath` (the current directory joined when the path needs it; nothing set when it cannot be read).
    pub fn calc_full_path(&mut self) -> Result<(), OutOfRange> {
        self.full_path_valid = true;
        let current = if path::needs_current(&self.path) {
            match current_directory() {
                Some(c) => Some(c),
                None => return Ok(()),
            }
        } else {
            None
        };
        self.full_path = path::absolute(&self.path, current.as_deref())?;
        Ok(())
    }

    /// `CalcFileExtension`: the name split at its last dot (the extension left as it was without one).
    pub fn calc_extension(&mut self) {
        self.file_name();
        match path::extension_split(&self.name) {
            Some(i) => {
                self.extension = self.name[i + 1..].to_vec();
                self.name_no_extension = self.name[..i].to_vec();
            }
            None => self.name_no_extension = self.name.clone(),
        }
        self.extension_valid = true;
    }

    pub fn directory(&mut self) -> &[u8] {
        if !self.directory_valid {
            self.calc_directory();
        }
        &self.directory
    }

    pub fn file_name(&mut self) -> &[u8] {
        if !self.name_valid {
            self.directory();
        }
        &self.name
    }

    pub fn extension(&mut self) -> &[u8] {
        if !self.extension_valid {
            self.calc_extension();
        }
        &self.extension
    }

    pub fn full_path(&mut self) -> Result<&[u8], OutOfRange> {
        if !self.full_path_valid {
            self.calc_full_path()?;
        }
        Ok(&self.full_path)
    }

    pub fn size(&mut self) -> u32 {
        self.stat();
        self.size
    }

    pub fn modify_time(&mut self) -> i64 {
        self.stat();
        self.modify_time
    }

    pub fn exists(&mut self) -> bool {
        self.stat();
        self.exists
    }

    pub fn is_dir(&mut self) -> bool {
        self.stat();
        self.is_dir
    }

    /// `SetReadOnly`: only the read-only attribute changed (a plain file's attributes reset to normal).
    pub fn set_read_only(&self, on: bool) -> bool {
        let p = c_str(&self.path);
        // SAFETY: a C string
        let a = unsafe { sys::PlatPathGetAttributes(p.as_ptr()) };
        if a == ATTR_INVALID {
            return false;
        }
        let a = if on { a | ATTR_READONLY } else { a & !ATTR_READONLY };
        // SAFETY: a C string
        unsafe { sys::PlatPathSetAttributes(p.as_ptr(), if a != 0 { a } else { ATTR_NORMAL }) != 0 }
    }

    /// `Delete(evenIfReadOnly)`.
    pub fn delete(&mut self, even_if_read_only: bool) -> bool {
        if even_if_read_only {
            self.stat();
            if self.read_only {
                self.set_read_only(false);
            }
        }
        let p = c_str(&self.path);
        // SAFETY: a C string
        unsafe { sys::PlatDeleteFile(p.as_ptr()) != 0 }
    }

    /// `CreateAsDirectory`: an existing path answers whether it is a directory.
    pub fn create_directory(&mut self) -> bool {
        if self.exists() {
            return self.is_dir();
        }
        let p = c_str(&self.path);
        // SAFETY: a C string
        unsafe { sys::PlatCreateDirectory(p.as_ptr()) == 0 }
    }

    /// `NormalizePath`: false for an empty path; only the directory's cache dropped.
    pub fn normalize(&mut self) -> Result<bool, OutOfRange> {
        match path::normalize(&self.path)? {
            None => Ok(false),
            Some(p) => {
                self.path = p;
                self.directory.clear();
                self.directory_valid = false;
                Ok(true)
            }
        }
    }
}

// ---- the C ABI (WONFile's forwarders in WONLobbyResource.h)

/// `WONFile(path)`; `path` NULL for the working directory (`GetWorkingDirectory`: empty when it cannot be read).
///
/// # Safety
/// `path` (when not NULL) holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_new(path: *const u8, length: usize) -> *mut EeWonFile {
    let p =
        if path.is_null() { current_directory().unwrap_or_default() } else { unsafe { bytes(path, length) }.to_vec() };
    Box::into_raw(Box::new(EeWonFile::new(&p)))
}

/// A copy (the C++ class's copy constructor).
///
/// # Safety
/// `f` is a live file.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_clone(f: *const EeWonFile) -> *mut EeWonFile {
    // SAFETY: the contract
    Box::into_raw(Box::new(unsafe { (*f).clone() }))
}

/// `*to = *from` (the copy assignment).
///
/// # Safety
/// Both are live files.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_assign(to: *mut EeWonFile, from: *const EeWonFile) {
    if !std::ptr::eq(to, from) {
        // SAFETY: the contract
        unsafe { *to = (*from).clone() };
    }
}

/// # Safety
/// `f` is from `ee_won_file_new` / `_clone` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_free(f: *mut EeWonFile) {
    if !f.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(f) });
    }
}

// what `ee_won_file_text` gives
/// `GetFilePath`
pub const EE_WON_FILE_PATH: i32 = 0;
/// `GetFileDirectory`
pub const EE_WON_FILE_DIRECTORY: i32 = 1;
/// `GetFileName`
pub const EE_WON_FILE_NAME: i32 = 2;
/// `GetFileExtension`
pub const EE_WON_FILE_EXTENSION: i32 = 3;
/// `GetFullPath` (2 returned where the original threw)
pub const EE_WON_FILE_FULL_PATH: i32 = 4;
/// `GetRelativePath(other)`: `other`'s path after this one's directory
pub const EE_WON_FILE_RELATIVE: i32 = 5;
/// `mFileNameNoExtension` as it is (no computing)
pub const EE_WON_FILE_NAME_NO_EXTENSION: i32 = 6;

/// One of the file's texts (`EE_WON_FILE_*`) to the sink, computed and cached as the getter did; 1, or 2 where the
/// original threw.
///
/// # Safety
/// `f` is a live file, `other` one for `EE_WON_FILE_RELATIVE`; `sink` receives the text during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_text(
    f: *mut EeWonFile,
    which: i32,
    other: *const EeWonFile,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> i32 {
    // SAFETY: the contract
    let f = unsafe { &mut *f };
    let text: Vec<u8> = match which {
        EE_WON_FILE_DIRECTORY => f.directory().to_vec(),
        EE_WON_FILE_NAME => f.file_name().to_vec(),
        EE_WON_FILE_EXTENSION => f.extension().to_vec(),
        EE_WON_FILE_FULL_PATH => match f.full_path() {
            Ok(p) => p.to_vec(),
            Err(OutOfRange) => return 2,
        },
        EE_WON_FILE_NAME_NO_EXTENSION => f.name_no_extension.clone(),
        EE_WON_FILE_RELATIVE => {
            // SAFETY: the contract (`other` may be `f`: its path read first)
            let sub = unsafe { (*other).path.clone() };
            path::relative(f.directory(), &sub, &CrtCtype)
        }
        _ => f.path.clone(),
    };
    // SAFETY: the sink's contract
    unsafe { give(sink, context, &text) };
    1
}

// what `ee_won_file_op` does
/// `SetFilePath(data)`
pub const EE_WON_FILE_SET_PATH: i32 = 0;
/// `Reset`
pub const EE_WON_FILE_RESET: i32 = 1;
/// `CalcFileInfo`, `CalcFileDirectory`, `CalcFullPath` (2 where it threw), `CalcFileExtension`
pub const EE_WON_FILE_CALC_INFO: i32 = 2;
pub const EE_WON_FILE_CALC_DIRECTORY: i32 = 3;
pub const EE_WON_FILE_CALC_FULL_PATH: i32 = 4;
pub const EE_WON_FILE_CALC_EXTENSION: i32 = 5;
/// `GetSize`, `GetModifyTime`
pub const EE_WON_FILE_SIZE: i32 = 6;
pub const EE_WON_FILE_MODIFY_TIME: i32 = 7;
/// `Exists`, `IsDir`
pub const EE_WON_FILE_EXISTS: i32 = 8;
pub const EE_WON_FILE_IS_DIR: i32 = 9;
/// `SetReadOnly(value)`, `Delete(value)`, `CreateAsDirectory`
pub const EE_WON_FILE_SET_READ_ONLY: i32 = 10;
pub const EE_WON_FILE_DELETE: i32 = 11;
pub const EE_WON_FILE_CREATE_DIRECTORY: i32 = 12;
/// `NormalizePath` (0 false, 1 true, 2 where it threw)
pub const EE_WON_FILE_NORMALIZE: i32 = 13;

/// One of the file's methods (`EE_WON_FILE_*`): its number result (bools 0/1).
///
/// # Safety
/// `f` is a live file; `data` holds `length` bytes (NULL for none).
#[no_mangle]
pub unsafe extern "C" fn ee_won_file_op(
    f: *mut EeWonFile,
    which: i32,
    value: i64,
    data: *const u8,
    length: usize,
) -> i64 {
    // SAFETY: the contract
    let f = unsafe { &mut *f };
    match which {
        EE_WON_FILE_SET_PATH => {
            // SAFETY: the caller's bytes (copied before the reset)
            let p = unsafe { bytes(data, length) }.to_vec();
            f.set_path(&p);
            0
        }
        EE_WON_FILE_RESET => {
            f.reset();
            0
        }
        EE_WON_FILE_CALC_INFO => {
            f.calc_info();
            0
        }
        EE_WON_FILE_CALC_DIRECTORY => {
            f.calc_directory();
            0
        }
        EE_WON_FILE_CALC_FULL_PATH => i64::from(f.calc_full_path().is_err()) * 2,
        EE_WON_FILE_CALC_EXTENSION => {
            f.calc_extension();
            0
        }
        EE_WON_FILE_SIZE => i64::from(f.size()),
        EE_WON_FILE_MODIFY_TIME => f.modify_time(),
        EE_WON_FILE_EXISTS => i64::from(f.exists()),
        EE_WON_FILE_IS_DIR => i64::from(f.is_dir()),
        EE_WON_FILE_SET_READ_ONLY => i64::from(f.set_read_only(value != 0)),
        EE_WON_FILE_DELETE => i64::from(f.delete(value != 0)),
        EE_WON_FILE_CREATE_DIRECTORY => i64::from(f.create_directory()),
        EE_WON_FILE_NORMALIZE => match f.normalize() {
            Ok(b) => i64::from(b),
            Err(OutOfRange) => 2,
        },
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_parts_and_their_caches() {
        let mut f = EeWonFile::new(b"dir/sub/name.tar.gz");
        assert_eq!(f.path, b"dir\\sub\\name.tar.gz");
        assert_eq!(f.extension(), b"gz");
        assert_eq!(f.name_no_extension, b"name.tar");
        assert_eq!(f.directory(), b"dir\\sub");
        assert_eq!(f.file_name(), b"name.tar.gz");
        // no separator: the directory is left as it was
        f.set_path(b"plain");
        assert_eq!(f.directory(), b"");
        assert_eq!(f.name, b"plain");
        assert_eq!(f.extension(), b"");
        assert_eq!(f.name_no_extension, b"plain");
        // Reset keeps the name without extension
        f.reset();
        assert_eq!(f.name_no_extension, b"plain");
        assert!(!f.directory_valid && !f.name_valid && !f.extension_valid);
    }

    #[test]
    fn normalize_drops_only_the_directory() {
        let mut f = EeWonFile::new(b"a\\b\\..\\c.txt");
        f.file_name();
        assert_eq!(f.normalize(), Ok(true));
        assert_eq!(f.path, b"a\\c.txt");
        assert!(!f.directory_valid && f.name_valid);
        assert_eq!(EeWonFile::new(b"").normalize(), Ok(false));
        assert_eq!(EeWonFile::new(b".").normalize(), Err(OutOfRange));
    }

    #[test]
    fn the_stat_of_a_file_a_directory_and_nothing() {
        let d = std::env::temp_dir().join(format!("ee_won_wonfile_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        let dir = d.to_str().unwrap().as_bytes().to_vec();
        let mut df = EeWonFile::new(&dir);
        assert!(!df.exists() && df.size() == 0);
        df.stat_valid = false;
        assert!(df.create_directory());
        df.stat_valid = false;
        assert!(df.exists() && df.is_dir());
        let file = [dir.as_slice(), b"/x.bin"].concat();
        std::fs::write(std::str::from_utf8(&file).unwrap(), b"12345").unwrap();
        let mut f = EeWonFile::new(&file);
        assert!(f.exists() && !f.is_dir() && !f.read_only);
        assert_eq!(f.size(), 5);
        assert!(f.modify_time() > 1_600_000_000);
        assert!(f.set_read_only(true));
        f.calc_info();
        assert!(f.read_only);
        assert!(f.delete(true));
        f.calc_info();
        assert!(!f.exists());
        let _ = std::fs::remove_dir_all(&d);
    }

    #[test]
    fn the_full_and_relative_paths() {
        let mut f = EeWonFile::new(b"C:\\x\\..\\y.txt");
        assert_eq!(f.full_path().unwrap(), b"C:\\y.txt");
        let mut base = EeWonFile::new(b"C:\\data\\file.cfg");
        let sub = EeWonFile::new(b"c:\\DATA\\images\\a.png");
        assert_eq!(path::relative(base.directory(), &sub.path, &CrtCtype), b"images\\a.png");
    }
}
