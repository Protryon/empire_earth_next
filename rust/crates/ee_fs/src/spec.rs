//! `FSBasicFile` and `FSFileSpec` whole in Rust (LLE lane round 5): a file named by volume, directory and file name,
//! opened through the platform layer's files (`include/platform/PlatFile.h`), searched with its directory search.
//!
//! The C++ classes keep their vtables and members (`UString`s, the access, version and open flag, the handle and the
//! stream: `FSMemoryFile` derives from `FSFileSpec` and `sizeof(FSFileSpec)` is in every embedding class), and each
//! member is a one-line forwarder handing this record (`EeFsSpec`, the object from its vtable pointer on) to a
//! function here. The strings are worked on through `ee_ustring`'s record functions in the original's order (an
//! `operator+` is a fresh record, `operator=` an assignment), so capacities and the bytes past a terminator are the
//! original's.
//!
//! Each function is the original member's steps: the same platform calls in the same order with the same arguments,
//! the same native errors recorded. Two members stay C++ because they call the object's virtuals, which
//! `FSMemoryFile` overrides: `operator<` (Open/Close) and `GetLengthFromClosedFile` (Open/GetLength/Close).

// (c_ulong and c_long are 32 bits on Windows, 64 on LP64: their casts to the platform layer's u32/i32 are real there)
#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_int, c_long, c_ulong, c_void, CStr};
use std::ptr;

use ee_ustring::{
    ee_ustring_append_text, ee_ustring_assign, ee_ustring_assign_text, ee_ustring_at, ee_ustring_concat_text,
    ee_ustring_free, ee_ustring_init_capacity, ee_ustring_substring, EeUString,
};

use crate::status;

/// The engine's `kError*` codes used here past `crate::status`.
/// cbindgen:ignore
mod code {
    pub const NATIVE_ERROR: i32 = 1;
    pub const ALREADY_EXISTS: i32 = 2;
    pub const ALREADY_OPEN: i32 = 5;
    pub const NO_MORE_FILES: i32 = 7;
    pub const BAD_PATH: i32 = 0xe;
    pub const NOT_WRITABLE: i32 = 0xf;
    pub const NOT_READABLE: i32 = 0x10;
}

/// `FSFileAccess`
pub(crate) const FS_ACCESS_READ: c_ulong = 1;
pub(crate) const FS_ACCESS_WRITE: c_ulong = 2;
const FS_ACCESS_APPEND: c_ulong = 4;

/// `PLAT_MAX_PATH`
const MAX_PATH: c_ulong = 260;

// ------------------------------------------------------------------------------------------- the platform layer

const PLAT_FILE_OK: c_int = 0;
const PLAT_FILE_EXISTS: c_int = 1;
const PLAT_FILE_BAD_PATH: c_int = 2;
const PLAT_FILE_NO_MORE: c_int = 3;
const PLAT_ATTR_READONLY: u32 = 0x01;
const PLAT_ATTR_DIRECTORY: u32 = 0x10;
const PLAT_ATTR_NORMAL: u32 = 0x80;
const PLAT_ATTR_INVALID: u32 = 0xFFFF_FFFF;
const PLAT_FILE_READ: u32 = 1;
const PLAT_FILE_WRITE: u32 = 2;
const PLAT_SEEK_BEGIN: c_int = 0;
const PLAT_SEEK_CURRENT: c_int = 1;
const PLAT_SEEK_END: c_int = 2;

/// `PlatFindData`
#[repr(C)]
pub(crate) struct PlatFindData {
    pub(crate) name: [c_char; 260],
    pub(crate) attributes: u32,
}

impl PlatFindData {
    fn zeroed() -> Self {
        PlatFindData { name: [0; 260], attributes: 0 }
    }
}

/// The platform layer (ee_plat_sys) and the C runtime's fclose, by their C names.
/// cbindgen:ignore
mod sys {
    use super::PlatFindData;
    use std::ffi::{c_char, c_int, c_void};
    extern "C" {
        pub fn PlatGetLastError() -> u32;
        pub fn PlatPathGetAttributes(path: *const c_char) -> u32;
        pub fn PlatPathSetAttributes(path: *const c_char, attributes: u32) -> c_int;
        pub fn PlatDeleteFile(path: *const c_char) -> c_int;
        pub fn PlatCopyFile(source: *const c_char, destination: *const c_char, fail_if_exists: c_int) -> c_int;
        pub fn PlatCreateDirectory(path: *const c_char) -> c_int;
        pub fn PlatRemoveDirectory(path: *const c_char) -> c_int;
        pub fn PlatFileOpen(path: *const c_char, access: u32) -> *mut c_void;
        pub fn PlatFileCreateNew(path: *const c_char) -> c_int;
        pub fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void;
        pub fn PlatFileClose(file: *mut c_void) -> c_int;
        pub fn PlatFileRead(file: *mut c_void, data: *mut c_void, length: u32, read: *mut u32) -> c_int;
        pub fn PlatFileWrite(file: *mut c_void, data: *const c_void, length: u32, written: *mut u32) -> c_int;
        pub fn PlatFileGetSize(file: *mut c_void) -> u32;
        pub fn PlatFileSeek(file: *mut c_void, offset: i32, origin: c_int) -> u32;
        pub fn PlatFileTruncate(file: *mut c_void) -> c_int;
        pub fn PlatFindFirst(pattern: *const c_char, out: *mut PlatFindData) -> *mut c_void;
        pub fn PlatFindNext(find: *mut c_void, out: *mut PlatFindData) -> c_int;
        pub fn PlatFindClose(find: *mut c_void);
        pub fn fclose(stream: *mut c_void) -> c_int;
    }
}
use sys::*;

#[cfg(test)]
thread_local! {
    /// The replays' view of the last native error recorded on this thread (`ee_error`'s value is process-wide).
    pub(crate) static LAST_NATIVE: std::cell::Cell<c_ulong> = const { std::cell::Cell::new(0) };
}

/// `ESRecordNativeError(PlatGetLastError())`
unsafe fn record_native() {
    let error = PlatGetLastError() as c_ulong;
    ee_error::ee_error_record_native(error);
    #[cfg(test)]
    if error != 0 {
        LAST_NATIVE.with(|l| l.set(error));
    }
}

// ------------------------------------------------------------------------------------------- the records

/// A `UString` member as the C++ object lays it out: its vtable pointer, then the Rust record (`EeUString`).
#[repr(C)]
pub struct EeFsString {
    pub vtable: *const c_void,
    pub string: *mut c_char,
    pub length: c_ulong,
    pub capacity: c_ulong,
}

const _: () =
    assert!(std::mem::size_of::<EeUString>() == 2 * std::mem::size_of::<c_ulong>() + std::mem::size_of::<usize>());

impl EeFsString {
    /// The `UString` record inside.
    pub fn us(&mut self) -> *mut EeUString {
        ptr::addr_of_mut!(self.string).cast()
    }
    pub fn usc(&self) -> *const EeUString {
        ptr::addr_of!(self.string).cast()
    }
    /// The text up to the terminator.
    pub fn bytes(&self) -> &[u8] {
        // SAFETY: a live string's buffer is NUL-terminated
        unsafe { CStr::from_ptr(self.string).to_bytes() }
    }
    /// A record to initialise (no buffer yet): the target of an `ee_ustring_init*` or a substring.
    pub fn blank() -> Self {
        EeFsString { vtable: ptr::null(), string: ptr::null_mut(), length: 0, capacity: 0 }
    }
    /// A fresh `UString(capacity)` without a vtable (a local of the original's members; never seen by C++).
    pub fn with_capacity(capacity: c_ulong) -> Self {
        let mut s = EeFsString { vtable: ptr::null(), string: ptr::null_mut(), length: 0, capacity: 0 };
        // SAFETY: an uninitialised record to initialise
        unsafe { ee_ustring_init_capacity(s.us(), capacity) };
        s
    }
}

/// `FSFileSpec` (and its base `FSBasicFile`) from the vtable pointer on, as the C++ object lays it out.
#[repr(C)]
pub struct EeFsSpec {
    pub vtable: *const c_void,
    pub volume: EeFsString,
    pub directory: EeFsString,
    pub file: EeFsString,
    /// `mFileAccess`: Open's access, 1 = read
    pub file_access: c_ulong,
    pub version: c_ulong,
    pub is_open: bool,
    /// `mHandle`: the open `PlatFile*`, or a search's `PlatFind*`
    pub handle: *mut c_void,
    /// `mStream`: FOpen's stream
    pub stream: *mut c_void,
}

/// A temporary `FSFileSpec` of the original's (`FSFileSpec theSpec(thePath)`): its strings are freed on drop, it is
/// never opened.
pub struct TempSpec(pub EeFsSpec);

impl TempSpec {
    /// `FSBasicFile()`: three `UString(256)`, read access, version 0, closed (no handle or stream yet).
    pub fn new() -> Self {
        TempSpec(EeFsSpec {
            vtable: ptr::null(),
            volume: EeFsString::with_capacity(256),
            directory: EeFsString::with_capacity(256),
            file: EeFsString::with_capacity(256),
            file_access: FS_ACCESS_READ,
            version: 0,
            is_open: false,
            handle: ptr::null_mut(),
            stream: ptr::null_mut(),
        })
    }
}

impl TempSpec {
    /// `FSFileSpec(const FSFileSpec&)`: `UString()` members (the default capacity) assigned from `from`'s, closed.
    ///
    /// # Safety
    /// `from` is a live record.
    pub unsafe fn copy_of(from: *const EeFsSpec) -> Self {
        let mut t = TempSpec::new();
        for s in [&mut t.0.volume, &mut t.0.directory, &mut t.0.file] {
            ee_ustring_free(s.us());
            ee_ustring::ee_ustring_init(s.us());
        }
        ee_fs_spec_init_copy(&mut t.0, from);
        t
    }
}

impl TempSpec {
    /// The record, its strings no longer freed here (it becomes part of another record).
    pub fn into_inner(self) -> EeFsSpec {
        let s = std::mem::ManuallyDrop::new(self);
        // SAFETY: read once; `s` is never dropped
        unsafe { ptr::read(&s.0) }
    }
}

impl Default for TempSpec {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for TempSpec {
    fn drop(&mut self) {
        // ~FSFileSpec(): an open file or a search closed, then the strings freed
        // SAFETY: the three records are live
        unsafe {
            ee_fs_spec_finish(&mut self.0);
            ee_ustring_free(self.0.volume.us());
            ee_ustring_free(self.0.directory.us());
            ee_ustring_free(self.0.file.us());
        }
    }
}

/// A path built as the original's local `thePath = a + b (+ c)`: only its text is used (a platform call's argument).
fn path(parts: &[&EeFsString]) -> Vec<u8> {
    let mut p = Vec::with_capacity(MAX_PATH as usize + 1);
    for s in parts {
        p.extend_from_slice(s.bytes());
    }
    p.push(0);
    p
}

fn cp(p: &[u8]) -> *const c_char {
    p.as_ptr().cast()
}

/// `a.GetLength() + b.GetLength() (+ c.GetLength()) <= PLAT_MAX_PATH`, in `unsigned long`.
fn fits(parts: &[&EeFsString]) -> bool {
    parts.iter().fold(0 as c_ulong, |n, s| n.wrapping_add(s.length)) <= MAX_PATH
}

/// `out = a + text` (a fresh `operator+` record assigned, then freed).
unsafe fn assign_concat_text(out: &mut EeFsString, a: &EeFsString, text: *const c_char) {
    let mut t = EeFsString::blank();
    ee_ustring_concat_text(t.us(), a.usc(), text);
    ee_ustring_assign(out.us(), t.usc());
    ee_ustring_free(t.us());
}

/// `out = s.GetSubstring(start, length)`
unsafe fn assign_substring(out: &mut EeFsString, s: &EeFsString, start: c_ulong, length: c_ulong) {
    let mut t = EeFsString::blank();
    ee_ustring_substring(t.us(), s.usc(), start, length);
    ee_ustring_assign(out.us(), t.usc());
    ee_ustring_free(t.us());
}

#[cfg(test)]
unsafe fn assign_text(out: &mut EeFsString, text: &[u8]) {
    let mut t = text.to_vec();
    t.push(0);
    ee_ustring_assign_text(out.us(), cp(&t));
}

/// `a == "text"` (strcmp)
fn equals(a: &EeFsString, text: &[u8]) -> bool {
    a.bytes() == text
}

/// The fields every output spec of the original reset: closed, no handle or stream, read access.
fn reset(s: &mut EeFsSpec) {
    s.handle = ptr::null_mut();
    s.stream = ptr::null_mut();
    s.is_open = false;
    s.file_access = FS_ACCESS_READ;
}

// ------------------------------------------------------------------------------------------- FSBasicFile

/// `FSBasicFile()`'s body (the strings are the C++ members' `UString(256)`): read access, version 0, closed.
///
/// # Safety
/// `s` is a live record (its strings constructed).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_basic_init(s: *mut EeFsSpec) {
    let s = &mut *s;
    s.version = 0;
    s.is_open = false;
    s.file_access = FS_ACCESS_READ;
}

/// `FSBasicFile(const FSBasicFile&)`'s body: the names assigned, version 0, closed, read access.
///
/// # Safety
/// Both are live records.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_basic_init_copy(s: *mut EeFsSpec, from: *const EeFsSpec) {
    ee_ustring_assign((*s).volume.us(), (*from).volume.usc());
    ee_ustring_assign((*s).directory.us(), (*from).directory.usc());
    ee_ustring_assign((*s).file.us(), (*from).file.usc());
    (*s).version = 0;
    (*s).is_open = false;
    (*s).file_access = FS_ACCESS_READ;
}

/// `FSBasicFile::Delete`: deletes the file; kErrorAlreadyOpen when open, kErrorBadPath past `PLAT_MAX_PATH`.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_basic_delete(s: *const EeFsSpec) -> c_long {
    let s = &*s;
    if s.is_open {
        return code::ALREADY_OPEN as c_long;
    }
    if !fits(&[&s.volume, &s.directory, &s.file]) {
        return code::BAD_PATH as c_long;
    }
    let p = path(&[&s.volume, &s.directory, &s.file]);
    if PlatDeleteFile(cp(&p)) == 0 {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

// ------------------------------------------------------------------------------------------- FSFileSpec

/// `FSFileSpec()`'s body: the startup volume and directory, no file name, closed, read access.
///
/// # Safety
/// `s` is a live record; `startup_volume`/`startup_directory` the live `gStartupVolume`/`gStartupDirectory`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_init(
    s: *mut EeFsSpec,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
) {
    ee_ustring_assign((*s).volume.us(), (*startup_volume).usc());
    ee_ustring_assign((*s).directory.us(), (*startup_directory).usc());
    reset(&mut *s);
}

/// `FSFileSpec(const FSFileSpec&)`: the base's copy, then no handle or stream.
///
/// # Safety
/// Both are live records.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_init_copy(s: *mut EeFsSpec, from: *const EeFsSpec) {
    ee_fs_basic_init_copy(s, from);
    (*s).handle = ptr::null_mut();
    (*s).stream = ptr::null_mut();
}

/// `FSFileSpec(const UString&)`'s body: a full path split into volume (2 bytes), directory (through the last
/// backslash) and file name; the startup directory when the path has 2 bytes or less or no backslash (the volume
/// then stays the path's first two bytes overwritten by the startup volume, the file name empty).
///
/// # Safety
/// `s` is a live record, `in_path` a live `UString` record, the startup strings as [`ee_fs_spec_init`].
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_init_path(
    s: *mut EeFsSpec,
    in_path: *const EeFsString,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
) {
    let s = &mut *s;
    let p = &*in_path;
    if p.length > 2 {
        assign_substring(&mut s.volume, p, 0, 2);
        if ee_ustring_at(p.usc(), (p.length as c_int).wrapping_sub(1)) == b'\\' as c_char {
            assign_substring(&mut s.directory, p, 2, p.length.wrapping_sub(2));
            ee_ustring_assign_text(s.file.us(), c"".as_ptr());
        } else {
            let index = ee_ustring::ee_ustring_find(p.usc(), c"\\".as_ptr(), 1, c_ulong::MAX, true);
            if index != p.length {
                assign_substring(&mut s.directory, p, 2, index.wrapping_sub(1));
                assign_substring(&mut s.file, p, index.wrapping_add(1), p.length.wrapping_sub(index).wrapping_sub(1));
            } else {
                ee_ustring_assign(s.volume.us(), (*startup_volume).usc());
                ee_ustring_assign(s.directory.us(), (*startup_directory).usc());
            }
        }
    } else {
        ee_ustring_assign(s.volume.us(), (*startup_volume).usc());
        ee_ustring_assign(s.directory.us(), (*startup_directory).usc());
    }
    reset(s);
}

/// `~FSFileSpec()`'s body: an open file closed (this class's Close: the destructor's dispatch), else a search
/// handle closed.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_finish(s: *mut EeFsSpec) {
    if (*s).is_open {
        ee_fs_spec_close(s);
    } else if !(*s).handle.is_null() {
        ee_fs_spec_close_search(s);
    }
}

/// `operator=`: the names copied, closed, no handle or stream, read access (an open file's handle is dropped, as
/// the original dropped it).
///
/// # Safety
/// Both are live records (the same one allowed).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_assign(s: *mut EeFsSpec, from: *const EeFsSpec) {
    ee_ustring_assign((*s).volume.us(), (*from).volume.usc());
    ee_ustring_assign((*s).directory.us(), (*from).directory.usc());
    ee_ustring_assign((*s).file.us(), (*from).file.usc());
    reset(&mut *s);
}

/// `CloseSearchHandle`
///
/// # Safety
/// `s` is a live record whose handle is a search's (or NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_close_search(s: *mut EeFsSpec) {
    PlatFindClose((*s).handle);
    (*s).handle = ptr::null_mut();
}

/// `Create`: an empty new file; kErrorFailed for a name with `:*?"<>|` or only spaces, kErrorAlreadyExists when it
/// exists, kErrorBadPath past `PLAT_MAX_PATH`. The handle is dropped (set NULL) first, as the original did.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_create(s: *mut EeFsSpec) -> c_long {
    let s = &mut *s;
    let name = s.file.bytes();
    let invalid = name.iter().position(|c| b":*?\"<>|".contains(c)).unwrap_or(name.len()) as c_ulong;
    let spaces = name.iter().position(|&c| c != b' ').unwrap_or(name.len()) as c_ulong;
    if invalid != s.file.length || spaces == s.file.length {
        return status::FAILED as c_long;
    }
    if !fits(&[&s.file, &s.volume, &s.directory]) {
        return code::BAD_PATH as c_long;
    }
    let p = path(&[&s.volume, &s.directory, &s.file]);
    s.handle = ptr::null_mut();
    match PlatFileCreateNew(cp(&p)) {
        PLAT_FILE_OK => status::OK as c_long,
        PLAT_FILE_BAD_PATH => status::FAILED as c_long,
        PLAT_FILE_EXISTS => code::ALREADY_EXISTS as c_long,
        _ => {
            record_native();
            code::NATIVE_ERROR as c_long
        }
    }
}

/// `CreateDirectory`: the spec's directory; kErrorAlreadyExists when present.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_create_directory(s: *const EeFsSpec) -> c_long {
    let s = &*s;
    if !fits(&[&s.volume, &s.directory]) {
        return code::BAD_PATH as c_long;
    }
    let p = path(&[&s.volume, &s.directory]);
    match PlatCreateDirectory(cp(&p)) {
        PLAT_FILE_OK => status::OK as c_long,
        PLAT_FILE_EXISTS => code::ALREADY_EXISTS as c_long,
        _ => {
            record_native();
            code::NATIVE_ERROR as c_long
        }
    }
}

/// `Close`: the file handle, else the stdio stream; a native error (still open) when that fails, OK when closed.
///
/// # Safety
/// `s` is a live record (when open without a handle its stream is a live `FILE*`).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_close(s: *mut EeFsSpec) -> c_long {
    let s = &mut *s;
    if !s.is_open {
        return status::OK as c_long;
    }
    if !s.handle.is_null() {
        if PlatFileClose(s.handle) != 0 {
            s.handle = ptr::null_mut();
            s.is_open = false;
            return status::OK as c_long;
        }
    } else if fclose(s.stream) == 0 {
        s.stream = ptr::null_mut();
        s.is_open = false;
        return status::OK as c_long;
    }
    record_native();
    code::NATIVE_ERROR as c_long
}

/// `Copy`: the file copied to `destination`; unless `keep_attributes`, the copy gets the source's attributes less
/// read-only.
///
/// # Safety
/// `s` is a live record, `destination` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_copy(
    s: *const EeFsSpec,
    destination: *const c_char,
    fail_if_exists: bool,
    keep_attributes: bool,
) -> c_long {
    let s = &*s;
    let p = path(&[&s.volume, &s.directory, &s.file]);
    if PlatCopyFile(cp(&p), destination, fail_if_exists as c_int) != 0 {
        if !keep_attributes {
            let attributes = PlatPathGetAttributes(cp(&p));
            if attributes != PLAT_ATTR_INVALID {
                PlatPathSetAttributes(destination, attributes & !PLAT_ATTR_READONLY);
            }
        }
        return status::OK as c_long;
    }
    record_native();
    code::NATIVE_ERROR as c_long
}

/// `DeleteDirectory`: the (empty) directory removed; kErrorFailed for the root `\`.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_delete_directory(s: *const EeFsSpec) -> c_long {
    let s = &*s;
    if !fits(&[&s.volume, &s.directory]) {
        return code::BAD_PATH as c_long;
    }
    if equals(&s.directory, b"\\") {
        return status::FAILED as c_long;
    }
    let p = path(&[&s.volume, &s.directory]);
    if PlatRemoveDirectory(cp(&p)) == 0 {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// `FOpen`: the file as a stdio stream (`PlatFOpen` with `mode`); open meanwhile, closed again on failure.
///
/// # Safety
/// `s` is a live record, `mode` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_fopen(s: *mut EeFsSpec, mode: *const c_char) -> c_long {
    let s = &mut *s;
    s.is_open = true;
    if !fits(&[&s.volume, &s.directory, &s.file]) {
        s.is_open = false;
        return code::BAD_PATH as c_long;
    }
    let p = path(&[&s.volume, &s.directory, &s.file]);
    s.stream = PlatFOpen(cp(&p), mode);
    if s.stream.is_null() {
        s.is_open = false;
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// The search's entry into `out` (the original's two branches: a directory gets the name and a backslash on its
/// directory, a file its name), then closed, no handle or stream, read access.
unsafe fn found(out: &mut EeFsSpec, volume: &EeFsString, directory: &EeFsString, data: &PlatFindData) {
    ee_ustring_assign(out.volume.us(), volume.usc());
    if data.attributes & PLAT_ATTR_DIRECTORY > 0 {
        assign_concat_text(&mut out.directory, directory, data.name.as_ptr());
        ee_ustring_append_text(out.directory.us(), c"\\".as_ptr());
        ee_ustring_assign_text(out.file.us(), c"".as_ptr());
    } else {
        ee_ustring_assign(out.directory.us(), directory.usc());
        ee_ustring_assign_text(out.file.us(), data.name.as_ptr());
    }
}

fn dots(data: &PlatFindData) -> bool {
    // SAFETY: the platform layer NUL-terminates the name
    let name = unsafe { CStr::from_ptr(data.name.as_ptr()) }.to_bytes();
    name == b"." || name == b".."
}

/// `GetFirstFile(pattern, outFile)`: a search of the spec's directory for `pattern`; the first entry that is not
/// `.` or `..` into `out`. kErrorNotFound when nothing matches, kErrorFailed when the spec is not a directory.
///
/// # Safety
/// `s` and `out` are live records (different ones), `pattern` a C string, the startup strings as
/// [`ee_fs_spec_init`].
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_first_file(
    s: *mut EeFsSpec,
    pattern: *const c_char,
    out: *mut EeFsSpec,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
) -> c_long {
    if !ee_fs_spec_is_directory(s) {
        return status::FAILED as c_long;
    }
    let mut p = path(&[&(*s).volume, &(*s).directory]);
    p.pop();
    p.extend_from_slice(CStr::from_ptr(pattern).to_bytes());
    p.push(0);
    // FSFileSpec theSpec(thePath): only its volume and directory are read
    let mut whole = EeFsString::blank();
    ee_ustring::ee_ustring_init_text(whole.us(), cp(&p));
    let mut spec = TempSpec::new();
    ee_fs_spec_init_path(&mut spec.0, &whole, startup_volume, startup_directory);
    ee_ustring_free(whole.us());
    let mut data = PlatFindData::zeroed();
    (*s).handle = PlatFindFirst(cp(&p), &mut data);
    if (*s).handle.is_null() {
        return status::NOT_FOUND as c_long;
    }
    if dots(&data) {
        return ee_fs_spec_next_file(s, out);
    }
    let out = &mut *out;
    found(out, &spec.0.volume, &spec.0.directory, &data);
    reset(out);
    status::OK as c_long
}

/// `GetNextFile`: the search's next entry that is not `.` or `..` into `out`; at the end the search is closed and
/// kErrorNoMoreFiles returned. kErrorFailed without a search or when the spec is not a directory.
///
/// # Safety
/// `s` and `out` are live records (different ones); `s`'s handle is a search's or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_next_file(s: *mut EeFsSpec, out: *mut EeFsSpec) -> c_long {
    if !(ee_fs_spec_is_directory(s) && !(*s).handle.is_null()) {
        return status::FAILED as c_long;
    }
    loop {
        let mut data = PlatFindData::zeroed();
        let next = PlatFindNext((*s).handle, &mut data);
        if next != PLAT_FILE_OK {
            PlatFindClose((*s).handle);
            (*s).handle = ptr::null_mut();
            if next == PLAT_FILE_NO_MORE {
                return code::NO_MORE_FILES as c_long;
            }
            record_native();
            return code::NATIVE_ERROR as c_long;
        }
        if dots(&data) {
            // the original's recursion: the same checks again
            if !(ee_fs_spec_is_directory(s) && !(*s).handle.is_null()) {
                return status::FAILED as c_long;
            }
            continue;
        }
        let (volume, directory) = (&(*s).volume as *const EeFsString, &(*s).directory as *const EeFsString);
        let out = &mut *out;
        found(out, &*volume, &*directory, &data);
        reset(out);
        return status::OK as c_long;
    }
}

/// `GetLength` of the open file; kErrorFailed for a stream, kErrorNotOpen when closed.
///
/// # Safety
/// `s` is a live record, `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_get_length(s: *const EeFsSpec, out: *mut c_ulong) -> c_long {
    let s = &*s;
    if !s.is_open {
        return status::NOT_OPEN as c_long;
    }
    if s.handle.is_null() {
        return status::FAILED as c_long;
    }
    let length = PlatFileGetSize(s.handle);
    if length == 0xFFFF_FFFF {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    *out = length as c_ulong;
    status::OK as c_long
}

/// `GetParentDirectory`: the directory above the spec's (the root stays the root), no file name, closed.
///
/// # Safety
/// `s` and `out` are live records (different ones).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_parent(s: *const EeFsSpec, out: *mut EeFsSpec) {
    let (s, out) = (&*s, &mut *out);
    if equals(&s.directory, b"\\") {
        ee_ustring_assign(out.volume.us(), s.volume.usc());
        ee_ustring_assign(out.directory.us(), s.directory.usc());
    } else {
        let index =
            ee_ustring::ee_ustring_find(s.directory.usc(), c"\\".as_ptr(), 1, s.directory.length.wrapping_sub(2), true);
        ee_ustring_assign(out.volume.us(), s.volume.usc());
        assign_substring(&mut out.directory, &s.directory, 0, index.wrapping_add(1));
    }
    ee_ustring_assign_text(out.file.us(), c"".as_ptr());
    reset(out);
}

/// `GetPosition` in the open file; kErrorFailed for a stream, kErrorNotOpen when closed.
///
/// # Safety
/// `s` is a live record, `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_get_position(s: *const EeFsSpec, out: *mut c_long) -> c_long {
    let s = &*s;
    if !s.is_open {
        return status::NOT_OPEN as c_long;
    }
    if s.handle.is_null() {
        return status::FAILED as c_long;
    }
    let position = PlatFileSeek(s.handle, 0, PLAT_SEEK_CURRENT);
    if position == 0xFFFF_FFFF {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    *out = position as c_long;
    status::OK as c_long
}

/// `IsDirectory`: the spec names an existing directory and no file.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_is_directory(s: *const EeFsSpec) -> bool {
    let s = &*s;
    if s.file.length != 0 || !fits(&[&s.volume, &s.directory]) {
        return false;
    }
    let p = path(&[&s.volume, &s.directory]);
    let attributes = PlatPathGetAttributes(cp(&p));
    attributes != PLAT_ATTR_INVALID && attributes & PLAT_ATTR_DIRECTORY != 0
}

/// `Lock` (read-only on; kErrorAlreadyExists when it was) and `Unlock` (`PLAT_ATTR_NORMAL`; kErrorAlreadyExists
/// when it was not read-only).
unsafe fn lock(s: &EeFsSpec, on: bool) -> c_long {
    if !fits(&[&s.volume, &s.directory, &s.file]) {
        return code::BAD_PATH as c_long;
    }
    let p = path(&[&s.volume, &s.directory, &s.file]);
    let attributes = PlatPathGetAttributes(cp(&p));
    if attributes == PLAT_ATTR_INVALID {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    if (attributes & PLAT_ATTR_READONLY != 0) == on {
        return code::ALREADY_EXISTS as c_long;
    }
    if PlatPathSetAttributes(cp(&p), if on { PLAT_ATTR_READONLY } else { PLAT_ATTR_NORMAL }) == 0 {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// `Lock`
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_lock(s: *const EeFsSpec) -> c_long {
    lock(&*s, true)
}

/// `Unlock`
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_unlock(s: *const EeFsSpec) -> c_long {
    lock(&*s, false)
}

/// `Open`: the existing file for reading (1), writing (2), appending (4, at its end) or both (anything else).
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_open(s: *mut EeFsSpec, access: c_ulong) -> c_long {
    let s = &mut *s;
    s.is_open = true;
    s.file_access = access;
    if !fits(&[&s.volume, &s.directory, &s.file]) {
        s.is_open = false;
        return code::BAD_PATH as c_long;
    }
    let platform = if access == FS_ACCESS_READ {
        PLAT_FILE_READ
    } else if access == FS_ACCESS_WRITE || access == FS_ACCESS_APPEND {
        PLAT_FILE_WRITE
    } else {
        PLAT_FILE_READ | PLAT_FILE_WRITE
    };
    let p = path(&[&s.volume, &s.directory, &s.file]);
    let file = PlatFileOpen(cp(&p), platform);
    s.handle = file;
    if file.is_null() {
        s.is_open = false;
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    if access & FS_ACCESS_APPEND != 0 && PlatFileSeek(file, 0, PLAT_SEEK_END) == 0xFFFF_FFFF {
        PlatFileClose(file);
        s.handle = ptr::null_mut();
        s.is_open = false;
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// `Read(unsigned char*, length, outRead)`: up to `length` bytes; kErrorEndOfFile when none are left.
///
/// # Safety
/// `s` is a live record, `out` writable for `length` bytes, `read` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_read(
    s: *const EeFsSpec,
    out: *mut u8,
    length: c_ulong,
    read: *mut c_ulong,
) -> c_long {
    let s = &*s;
    if !s.is_open {
        return status::NOT_OPEN as c_long;
    }
    if s.handle.is_null() {
        return status::FAILED as c_long;
    }
    if s.file_access & FS_ACCESS_READ == 0 {
        return code::NOT_READABLE as c_long;
    }
    let mut count = 0u32;
    let ok = PlatFileRead(s.handle, out.cast(), length as u32, &mut count);
    *read = count as c_ulong;
    if count == 0 {
        if ok != 0 {
            return status::END_OF_FILE as c_long;
        }
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// The scalar `Read`s: `size` bytes into `out`; kErrorFailed on a short read that reported OK.
///
/// # Safety
/// As [`ee_fs_spec_read`], `out` writable for `size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_read_value(s: *const EeFsSpec, out: *mut u8, size: c_ulong) -> c_long {
    let mut read: c_ulong = 0;
    let result = ee_fs_spec_read(s, out, size, &mut read);
    if read != size && result == status::OK as c_long {
        return status::FAILED as c_long;
    }
    result
}

/// `SetLength`: the open file ends at `length` (the truncate flag is ignored, as in the original).
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_set_length(s: *const EeFsSpec, length: c_ulong) -> c_long {
    let s = &*s;
    if !s.is_open {
        return status::NOT_OPEN as c_long;
    }
    if s.handle.is_null() {
        return status::FAILED as c_long;
    }
    if s.file_access & 6 == 0 {
        return code::NOT_WRITABLE as c_long;
    }
    if PlatFileSeek(s.handle, length as i32, PLAT_SEEK_BEGIN) == 0xFFFF_FFFF || PlatFileTruncate(s.handle) == 0 {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// `SetPosition`: `offset` from the start (0), the current position (2) or the end (anything else); past the end
/// only when `extend` on a file opened for writing (else kErrorBeyondEnd). The target's test wraps in 32 bits.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_set_position(
    s: *const EeFsSpec,
    offset: c_long,
    origin: c_ulong,
    extend: bool,
) -> c_long {
    let s = &*s;
    if !s.is_open {
        return status::NOT_OPEN as c_long;
    }
    if s.handle.is_null() {
        return status::FAILED as c_long;
    }
    let length = PlatFileGetSize(s.handle);
    if length == 0xFFFF_FFFF {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    let method = match origin {
        0 => PLAT_SEEK_BEGIN,
        2 => PLAT_SEEK_CURRENT,
        _ => PLAT_SEEK_END,
    };
    let position = PlatFileSeek(s.handle, 0, method);
    if position == 0xFFFF_FFFF {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    if position.wrapping_add(offset as u32) > length
        && !(extend && s.file_access & (FS_ACCESS_WRITE | FS_ACCESS_APPEND) != 0)
    {
        return status::BEYOND_END as c_long;
    }
    if PlatFileSeek(s.handle, offset as i32, method) == 0xFFFF_FFFF {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// `Write(const unsigned char*, length, outWritten)`: a native error on a short write.
///
/// # Safety
/// `s` is a live record, `data` readable for `length` bytes, `written` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_write(
    s: *const EeFsSpec,
    data: *const u8,
    length: c_ulong,
    written: *mut c_ulong,
) -> c_long {
    let s = &*s;
    if !s.is_open {
        return status::NOT_OPEN as c_long;
    }
    if s.handle.is_null() {
        return status::FAILED as c_long;
    }
    if s.file_access & 6 == 0 {
        return code::NOT_WRITABLE as c_long;
    }
    let mut count = 0u32;
    let ok = PlatFileWrite(s.handle, data.cast(), length as u32, &mut count);
    *written = count as c_ulong;
    if ok == 0 || *written != length {
        record_native();
        return code::NATIVE_ERROR as c_long;
    }
    status::OK as c_long
}

/// The scalar `Write`s: `size` bytes of `data`.
///
/// # Safety
/// As [`ee_fs_spec_write`].
#[no_mangle]
pub unsafe extern "C" fn ee_fs_spec_write_value(s: *const EeFsSpec, data: *const u8, size: c_ulong) -> c_long {
    let mut written: c_ulong = 0;
    ee_fs_spec_write(s, data, size, &mut written)
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    /// A spec as `FSFileSpec()` makes one over the given startup strings (the C++ members' `UString(256)`).
    pub(crate) fn spec(volume: &EeFsString, directory: &EeFsString) -> TempSpec {
        let mut s = TempSpec::new();
        // SAFETY: live records
        unsafe {
            ee_fs_basic_init(&mut s.0);
            ee_fs_spec_init(&mut s.0, volume, directory);
        }
        s
    }

    pub(crate) fn text(s: &str) -> EeFsString {
        let mut r = EeFsString::with_capacity(256);
        // SAFETY: a live record
        unsafe { assign_text(&mut r, s.as_bytes()) };
        r
    }

    #[test]
    fn the_record_has_the_cpp_layout() {
        use std::mem::{offset_of, size_of};
        let word = size_of::<usize>();
        let string = word + size_of::<EeUString>();
        assert_eq!(size_of::<EeFsString>(), string);
        assert_eq!(offset_of!(EeFsSpec, volume), word);
        assert_eq!(offset_of!(EeFsSpec, file_access), word + 3 * string);
        assert_eq!(offset_of!(EeFsSpec, handle) % word, 0);
        assert_eq!(size_of::<EeFsSpec>(), offset_of!(EeFsSpec, stream) + word);
    }

    #[test]
    fn a_path_splits_into_volume_directory_and_file() {
        let (v, d) = (text("C:"), text("\\start\\"));
        for (input, volume, directory, file) in [
            ("C:\\a\\b.txt", "C:", "\\a\\", "b.txt"),
            ("C:\\a\\", "C:", "\\a\\", ""),
            ("D:\\x", "D:", "\\", "x"),
            ("ab", "C:", "\\start\\", ""),
            ("abc.txt", "C:", "\\start\\", ""),
        ] {
            let mut s = spec(&v, &d);
            let p = text(input);
            // SAFETY: live records
            unsafe { ee_fs_spec_init_path(&mut s.0, &p, &v, &d) };
            assert_eq!(s.0.volume.bytes(), volume.as_bytes(), "{input}");
            assert_eq!(s.0.directory.bytes(), directory.as_bytes(), "{input}");
            assert_eq!(s.0.file.bytes(), file.as_bytes(), "{input}");
            assert!(!s.0.is_open && s.0.handle.is_null() && s.0.file_access == FS_ACCESS_READ);
        }
    }

    #[test]
    fn the_parent_of_the_root_is_the_root() {
        let (v, d) = (text("C:"), text("\\a\\b\\"));
        let s = spec(&v, &d);
        let mut out = spec(&v, &d);
        // SAFETY: live records
        unsafe { ee_fs_spec_parent(&s.0, &mut out.0) };
        assert_eq!(out.0.directory.bytes(), b"\\a\\");
        let root = spec(&v, &text("\\"));
        // SAFETY: live records
        unsafe { ee_fs_spec_parent(&root.0, &mut out.0) };
        assert_eq!(out.0.directory.bytes(), b"\\");
    }

    #[test]
    fn a_closed_spec_refuses_io_and_create_checks_the_name() {
        let (v, d) = (text("C:"), text("\\"));
        let mut s = spec(&v, &d);
        let mut n: c_ulong = 0;
        let mut p: c_long = 0;
        let mut b = [0u8; 4];
        // SAFETY: live records and buffers
        unsafe {
            assert_eq!(ee_fs_spec_read(&s.0, b.as_mut_ptr(), 4, &mut n), status::NOT_OPEN as c_long);
            assert_eq!(ee_fs_spec_write(&s.0, b.as_ptr(), 4, &mut n), status::NOT_OPEN as c_long);
            assert_eq!(ee_fs_spec_get_length(&s.0, &mut n), status::NOT_OPEN as c_long);
            assert_eq!(ee_fs_spec_get_position(&s.0, &mut p), status::NOT_OPEN as c_long);
            assert_eq!(ee_fs_spec_close(&mut s.0), status::OK as c_long);
            for bad in ["", "  ", "a?b", "c:d", "e|f"] {
                assign_text(&mut s.0.file, bad.as_bytes());
                assert_eq!(ee_fs_spec_create(&mut s.0), status::FAILED as c_long, "{bad:?}");
            }
            assign_text(&mut s.0.file, &[b'x'; 300]);
            assert_eq!(ee_fs_spec_create(&mut s.0), code::BAD_PATH as c_long);
            assert_eq!(ee_fs_spec_open(&mut s.0, 1), code::BAD_PATH as c_long);
            assert!(!s.0.is_open);
        }
    }

    #[test]
    fn a_file_round_trips_through_a_real_directory() {
        let dir = std::env::temp_dir().join(format!("ee_fs_spec_unit_{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let native = format!("{}{}", dir.display(), std::path::MAIN_SEPARATOR);
        // the game's form (the POSIX layer maps it back): a 2-byte volume, then the directory
        let game = if cfg!(windows) { native.clone() } else { format!("C:{}", native.replace('/', "\\")) };
        let (v, d) = (text(&game[..2]), text(&game[2..]));
        let mut s = spec(&v, &d);
        // SAFETY: live records and buffers
        unsafe {
            assign_text(&mut s.0.file, b"f.bin");
            assert_eq!(ee_fs_spec_create(&mut s.0), status::OK as c_long);
            assert_eq!(ee_fs_spec_create(&mut s.0), code::ALREADY_EXISTS as c_long);
            assert_eq!(ee_fs_spec_open(&mut s.0, 3), status::OK as c_long);
            let v32 = 0x1234_5678u32.to_le_bytes();
            assert_eq!(ee_fs_spec_write_value(&s.0, v32.as_ptr(), 4), status::OK as c_long);
            let mut n: c_ulong = 0;
            assert_eq!(ee_fs_spec_get_length(&s.0, &mut n), status::OK as c_long);
            assert_eq!(n, 4);
            assert_eq!(ee_fs_spec_set_position(&s.0, 5, 0, false), status::BEYOND_END as c_long);
            assert_eq!(ee_fs_spec_set_position(&s.0, 0, 0, false), status::OK as c_long);
            let mut b = [0u8; 4];
            assert_eq!(ee_fs_spec_read_value(&s.0, b.as_mut_ptr(), 4), status::OK as c_long);
            assert_eq!(b, v32);
            assert_eq!(ee_fs_spec_read_value(&s.0, b.as_mut_ptr(), 4), status::END_OF_FILE as c_long);
            assert_eq!(ee_fs_spec_close(&mut s.0), status::OK as c_long);
            // the directory search finds it
            let mut out = spec(&v, &d);
            let mut dirspec = spec(&v, &d);
            assert!(ee_fs_spec_is_directory(&dirspec.0));
            assert_eq!(
                ee_fs_spec_first_file(&mut dirspec.0, c"*.bin".as_ptr(), &mut out.0, &v, &d),
                status::OK as c_long
            );
            assert_eq!(out.0.file.bytes(), b"f.bin");
            assert_eq!(ee_fs_spec_next_file(&mut dirspec.0, &mut out.0), code::NO_MORE_FILES as c_long);
            assert!(dirspec.0.handle.is_null());
            assert_eq!(ee_fs_spec_lock(&s.0), status::OK as c_long);
            assert_eq!(ee_fs_spec_lock(&s.0), code::ALREADY_EXISTS as c_long);
            assert_eq!(ee_fs_spec_unlock(&s.0), status::OK as c_long);
            assert_eq!(ee_fs_basic_delete(&s.0), status::OK as c_long);
        }
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
