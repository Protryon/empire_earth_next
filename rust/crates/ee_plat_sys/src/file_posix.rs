//! PlatFile on POSIX. Every path is a game path, resolved by PlatPath (`native_path`: volumes, separators,
//! case-insensitive lookup). A PlatFile is a file descriptor, a PlatFind a sorted snapshot of the directory's matching
//! entries (NTFS lists a directory in case-insensitive order; readdir has no order at all), a PlatFileMapping an
//! mmap'ed view.

#![allow(clippy::unnecessary_cast)]

use crate::constants::*;
use crate::cstr::try_box;
use crate::last_error::{self, fail, fail_errno, set as set_last_error, *};
use crate::native_path::{to_game_form, to_native};
use crate::text_mode::{translate_mode, CrLfFilter};
use crate::wildcard::{compare_no_case, wildcard_match};
use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::ptr::null_mut;

pub struct PlatFile {
    descriptor: c_int,
}

pub struct PlatFileMapping {
    address: *mut c_void,
    length: usize,
}

pub struct PlatFind {
    entries: Vec<PlatFindData>,
    next: usize,
}

fn is_directory(stat: &libc::stat) -> bool {
    stat.st_mode & libc::S_IFMT == libc::S_IFDIR
}

fn attributes_of(stat: &libc::stat, name: &[u8]) -> u32 {
    let mut attributes = if is_directory(stat) { PLAT_ATTR_DIRECTORY } else { PLAT_ATTR_ARCHIVE };
    if stat.st_mode & libc::S_IWUSR == 0 {
        attributes |= PLAT_ATTR_READONLY;
    }
    if name.first() == Some(&b'.') && name != b"." && name != b".." {
        attributes |= PLAT_ATTR_HIDDEN;
    }
    attributes
}

fn base_name(path: &[u8]) -> &[u8] {
    match path.iter().rposition(|&c| c == b'/') {
        Some(slash) => &path[slash + 1..],
        None => path,
    }
}

fn exists_as_dir(native: &[u8]) -> bool {
    let Ok(path) = CString::new(native) else { return false };
    let mut stat = unsafe { std::mem::zeroed::<libc::stat>() };
    unsafe { libc::stat(path.as_ptr(), &mut stat) == 0 && is_directory(&stat) }
}

fn parent_exists(native: &[u8]) -> bool {
    match native.iter().rposition(|&c| c == b'/') {
        None | Some(0) => true,
        Some(slash) => exists_as_dir(&native[..slash]),
    }
}

/// The last error for a failed open/create of `native` with `errno`, ENOENT split into "file" and "path" as Win32
/// reports them -> 0.
fn fail_path(native: &[u8], errno: i32) -> c_int {
    if errno == libc::ENOENT && !parent_exists(native) {
        return fail(ERROR_PATH_NOT_FOUND);
    }
    set_last_error(last_error::from_errno(errno));
    0
}

/// FILETIME: 100 ns intervals since 1601-01-01 (11644473600 s before the Unix epoch).
pub fn file_time(seconds: i64, nanoseconds: i64) -> u64 {
    ((seconds + 11_644_473_600) as u64) * 10_000_000 + (nanoseconds / 100) as u64
}

fn write_time_of(stat: &libc::stat) -> u64 {
    file_time(stat.st_mtime as i64, stat.st_mtime_nsec as i64)
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathGetAttributes(path: *const c_char) -> u32 {
    let Some(native) = to_native(path) else { return PLAT_ATTR_INVALID };
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::stat(native.as_ptr(), &mut stat) != 0 {
        fail_path(native.as_bytes(), last_error::errno());
        return PLAT_ATTR_INVALID;
    }
    attributes_of(&stat, base_name(native.as_bytes()))
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathSetAttributes(path: *const c_char, attributes: u32) -> i32 {
    let Some(native) = to_native(path) else { return 0 };
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::stat(native.as_ptr(), &mut stat) != 0 {
        return fail_path(native.as_bytes(), last_error::errno());
    }
    // only the read-only bit has a native meaning: it is the owner's write permission
    let mut mode = stat.st_mode & 0o7777;
    if attributes & PLAT_ATTR_READONLY != 0 {
        mode &= !(libc::S_IWUSR | libc::S_IWGRP | libc::S_IWOTH);
    } else {
        mode |= libc::S_IWUSR;
    }
    if libc::chmod(native.as_ptr(), mode) != 0 {
        return fail_errno();
    }
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatDeleteFile(path: *const c_char) -> i32 {
    let Some(native) = to_native(path) else { return 0 };
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::lstat(native.as_ptr(), &mut stat) != 0 {
        return fail_path(native.as_bytes(), last_error::errno());
    }
    if is_directory(&stat) {
        return fail(ERROR_ACCESS_DENIED); // DeleteFile on a directory
    }
    if stat.st_mode & libc::S_IWUSR == 0 {
        return fail(ERROR_ACCESS_DENIED); // Win32 refuses to delete a read-only file
    }
    if libc::unlink(native.as_ptr()) != 0 {
        return fail_errno();
    }
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatCopyFile(source: *const c_char, destination: *const c_char, fail_if_exists: i32) -> i32 {
    let (Some(from), Some(to)) = (to_native(source), to_native(destination)) else { return 0 };
    let input = libc::open(from.as_ptr(), libc::O_RDONLY | libc::O_CLOEXEC);
    if input < 0 {
        return fail_path(from.as_bytes(), last_error::errno());
    }
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::fstat(input, &mut stat) != 0 {
        let error = last_error::errno();
        libc::close(input);
        return fail(last_error::from_errno(error));
    }
    if is_directory(&stat) {
        libc::close(input);
        return fail(ERROR_ACCESS_DENIED);
    }
    let flags = libc::O_WRONLY
        | libc::O_CREAT
        | libc::O_TRUNC
        | libc::O_CLOEXEC
        | if fail_if_exists != 0 { libc::O_EXCL } else { 0 };
    let output = libc::open(to.as_ptr(), flags, (stat.st_mode & 0o777) as libc::c_uint);
    if output < 0 {
        let error = last_error::errno();
        libc::close(input);
        if error == libc::EEXIST {
            return fail(ERROR_FILE_EXISTS);
        }
        return fail_path(to.as_bytes(), error);
    }
    let mut buffer = vec![0u8; 65536];
    loop {
        let read = libc::read(input, buffer.as_mut_ptr().cast(), buffer.len());
        if read == 0 {
            break;
        }
        if read < 0 {
            let error = last_error::errno();
            if error == libc::EINTR {
                continue;
            }
            libc::close(input);
            libc::close(output);
            return fail(last_error::from_errno(error));
        }
        let mut done = 0isize;
        while done < read {
            let written = libc::write(output, buffer.as_ptr().offset(done).cast(), (read - done) as usize);
            if written < 0 {
                let error = last_error::errno();
                if error == libc::EINTR {
                    continue;
                }
                libc::close(input);
                libc::close(output);
                return fail(last_error::from_errno(error));
            }
            done += written;
        }
    }
    // CopyFile keeps the source's write time (and its attributes: the mode was given to open)
    let times = [
        libc::timespec { tv_sec: stat.st_atime, tv_nsec: stat.st_atime_nsec as _ },
        libc::timespec { tv_sec: stat.st_mtime, tv_nsec: stat.st_mtime_nsec as _ },
    ];
    libc::futimens(output, times.as_ptr());
    libc::fchmod(output, stat.st_mode & 0o777);
    libc::close(input);
    if libc::close(output) != 0 {
        return fail_errno();
    }
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatCreateDirectory(path: *const c_char) -> PlatFileResult {
    let Some(native) = to_native(path) else { return PlatFileResult::NativeError };
    if libc::mkdir(native.as_ptr(), 0o777) == 0 {
        return PlatFileResult::Ok;
    }
    let error = last_error::errno();
    if error == libc::EEXIST {
        set_last_error(ERROR_ALREADY_EXISTS);
        return PlatFileResult::Exists;
    }
    fail_path(native.as_bytes(), error);
    PlatFileResult::NativeError
}

#[no_mangle]
pub unsafe extern "C" fn PlatRemoveDirectory(path: *const c_char) -> i32 {
    let Some(native) = to_native(path) else { return 0 };
    if libc::rmdir(native.as_ptr()) != 0 {
        let error = last_error::errno();
        if error == libc::ENOTEMPTY || error == libc::EEXIST {
            return fail(ERROR_DIR_NOT_EMPTY);
        }
        return fail_path(native.as_bytes(), error);
    }
    1
}

/// Copies `text` and its terminator to `out` when it fits (GetCurrentDirectory's rule): -> the length without the
/// terminator; when the buffer is too small, the size it needs.
unsafe fn copy_or_needed(text: &[u8], out: *mut c_char, size: u32) -> u32 {
    if text.len() + 1 > size as usize {
        return text.len() as u32 + 1;
    }
    std::ptr::copy_nonoverlapping(text.as_ptr().cast(), out, text.len());
    *out.add(text.len()) = 0;
    text.len() as u32
}

#[no_mangle]
pub unsafe extern "C" fn PlatGetCurrentDirectory(path: *mut c_char, size: u32) -> u32 {
    let mut buffer = vec![0 as c_char; 4096];
    if libc::getcwd(buffer.as_mut_ptr(), buffer.len()).is_null() {
        fail_errno();
        return 0;
    }
    let Some(game) = to_game_form(CStr::from_ptr(buffer.as_ptr())) else { return 0 };
    copy_or_needed(&game, path, size)
}

#[no_mangle]
pub unsafe extern "C" fn PlatGetTempDirectory(path: *mut c_char, size: u32) -> u32 {
    let mut native: Vec<u8> = match std::env::var_os("TMPDIR") {
        Some(value) if !value.is_empty() => value.into_encoded_bytes(),
        _ => b"/tmp".to_vec(),
    };
    while native.len() > 1 && native.last() == Some(&b'/') {
        native.pop();
    }
    let Ok(wanted) = CString::new(native) else { return 0 };
    let mut real = vec![0 as c_char; 4096];
    let canonical: CString = if libc::realpath(wanted.as_ptr(), real.as_mut_ptr()).is_null() {
        wanted
    } else {
        CStr::from_ptr(real.as_ptr()).to_owned()
    };
    let Some(mut game) = to_game_form(&canonical) else { return 0 };
    if game.last() != Some(&b'\\') {
        game.push(b'\\');
    }
    copy_or_needed(&game, path, size)
}

#[no_mangle]
pub unsafe extern "C" fn PlatCreateTempFile(directory: *const c_char, prefix: *const c_char, path: *mut c_char) -> i32 {
    use std::sync::atomic::{AtomicU32, Ordering};
    static COUNTER: AtomicU32 = AtomicU32::new(0);
    // GetTempFileName(dir, prefix, 0): "<dir>\<first 3 of prefix><4 hex digits>.TMP", created empty
    let mut base: Vec<u8> =
        if directory.is_null() { Vec::new() } else { CStr::from_ptr(directory).to_bytes().to_vec() };
    if !base.is_empty() && base.last() != Some(&b'\\') && base.last() != Some(&b'/') {
        base.push(b'\\');
    }
    let prefix: Vec<u8> =
        if prefix.is_null() { Vec::new() } else { CStr::from_ptr(prefix).to_bytes().iter().take(3).copied().collect() };
    let start =
        (std::process::id().wrapping_mul(7919).wrapping_add(COUNTER.fetch_add(1, Ordering::Relaxed).wrapping_add(1)))
            & 0xFFFF;
    for i in 0..0x10000u32 {
        let unique = start.wrapping_add(i) & 0xFFFF;
        if unique == 0 {
            continue;
        }
        let mut candidate = base.clone();
        candidate.extend_from_slice(&prefix);
        candidate.extend_from_slice(format!("{unique:04X}.TMP").as_bytes());
        if candidate.len() >= PLAT_MAX_PATH {
            return fail(ERROR_FILENAME_EXCED_RANGE);
        }
        let Ok(game) = CString::new(candidate.clone()) else { return 0 };
        let Some(native) = to_native(game.as_ptr()) else { return 0 };
        let file = libc::open(
            native.as_ptr(),
            libc::O_WRONLY | libc::O_CREAT | libc::O_EXCL | libc::O_CLOEXEC,
            0o666 as libc::c_uint,
        );
        if file >= 0 {
            libc::close(file);
            std::ptr::copy_nonoverlapping(candidate.as_ptr().cast(), path, candidate.len());
            *path.add(candidate.len()) = 0;
            return 1;
        }
        let error = last_error::errno();
        if error != libc::EEXIST {
            return fail_path(native.as_bytes(), error);
        }
    }
    fail(ERROR_FILE_EXISTS)
}

// ---- files

#[no_mangle]
pub unsafe extern "C" fn PlatFileOpen(path: *const c_char, access: u32) -> *mut PlatFile {
    let Some(native) = to_native(path) else { return null_mut() };
    let flags = match (access & PLAT_FILE_WRITE != 0, access & PLAT_FILE_READ != 0) {
        (true, true) => libc::O_RDWR,
        (true, false) => libc::O_WRONLY,
        _ => libc::O_RDONLY,
    };
    let file = libc::open(native.as_ptr(), flags | libc::O_CLOEXEC);
    if file < 0 {
        fail_path(native.as_bytes(), last_error::errno());
        return null_mut();
    }
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::fstat(file, &mut stat) == 0 && is_directory(&stat) {
        libc::close(file); // CreateFile without FILE_FLAG_BACKUP_SEMANTICS does not open a directory
        set_last_error(ERROR_ACCESS_DENIED);
        return null_mut();
    }
    let handle = try_box(PlatFile { descriptor: file });
    if handle.is_null() {
        libc::close(file);
        set_last_error(ERROR_NOT_ENOUGH_MEMORY);
    }
    handle
}

// ---- PlatFOpen's text mode: CR LF -> LF through a FILE* wrapper (see text_mode.rs). Writing is unchanged (LF lines,
// which the game reads back alike).

struct TextReader {
    base: *mut libc::FILE,
    filter: CrLfFilter,
    position: i64, // bytes delivered (ftell)
}

unsafe fn read_text(reader: *mut TextReader, data: *mut c_char, size: usize) -> isize {
    let base = (*reader).base;
    let out = std::slice::from_raw_parts_mut(data.cast::<u8>(), size);
    let n = (*reader).filter.read(out, || {
        let c = libc::fgetc(base);
        (c != libc::EOF).then_some(c as u8)
    });
    (*reader).position += n as i64;
    if libc::ferror(base) != 0 && n == 0 {
        -1
    } else {
        n as isize
    }
}

/// Rewind (SEEK_SET 0) and ftell (SEEK_CUR 0) only: a text stream's other offsets have no portable meaning.
unsafe fn seek_text(reader: *mut TextReader, offset: i64, whence: c_int) -> i64 {
    if whence == libc::SEEK_SET && offset == 0 {
        if libc::fseek((*reader).base, 0, libc::SEEK_SET) != 0 {
            return -1;
        }
        (*reader).filter = CrLfFilter::default();
        (*reader).position = 0;
        return 0;
    }
    if whence == libc::SEEK_CUR && offset == 0 {
        return (*reader).position;
    }
    -1
}

unsafe fn close_text(reader: *mut TextReader) -> c_int {
    let result = libc::fclose((*reader).base);
    drop(Box::from_raw(reader));
    result
}

#[cfg(target_vendor = "apple")]
mod text_stream {
    use super::*;

    extern "C" {
        fn funopen(
            cookie: *const c_void,
            read: Option<unsafe extern "C" fn(*mut c_void, *mut c_char, c_int) -> c_int>,
            write: Option<unsafe extern "C" fn(*mut c_void, *const c_char, c_int) -> c_int>,
            seek: Option<unsafe extern "C" fn(*mut c_void, i64, c_int) -> i64>,
            close: Option<unsafe extern "C" fn(*mut c_void) -> c_int>,
        ) -> *mut libc::FILE;
    }

    unsafe extern "C" fn read_fn(cookie: *mut c_void, data: *mut c_char, size: c_int) -> c_int {
        read_text(cookie.cast(), data, size as usize) as c_int
    }
    unsafe extern "C" fn seek_fn(cookie: *mut c_void, offset: i64, whence: c_int) -> i64 {
        seek_text(cookie.cast(), offset, whence)
    }
    unsafe extern "C" fn close_fn(cookie: *mut c_void) -> c_int {
        close_text(cookie.cast())
    }

    pub unsafe fn wrap(reader: *mut TextReader) -> *mut libc::FILE {
        funopen(reader.cast(), Some(read_fn), None, Some(seek_fn), Some(close_fn))
    }
}

#[cfg(not(target_vendor = "apple"))]
mod text_stream {
    use super::*;

    #[repr(C)]
    struct CookieFunctions {
        read: Option<unsafe extern "C" fn(*mut c_void, *mut c_char, usize) -> isize>,
        write: Option<unsafe extern "C" fn(*mut c_void, *const c_char, usize) -> isize>,
        seek: Option<unsafe extern "C" fn(*mut c_void, *mut i64, c_int) -> c_int>,
        close: Option<unsafe extern "C" fn(*mut c_void) -> c_int>,
    }

    extern "C" {
        fn fopencookie(cookie: *mut c_void, mode: *const c_char, functions: CookieFunctions) -> *mut libc::FILE;
    }

    unsafe extern "C" fn read_fn(cookie: *mut c_void, data: *mut c_char, size: usize) -> isize {
        read_text(cookie.cast(), data, size)
    }
    unsafe extern "C" fn seek_fn(cookie: *mut c_void, offset: *mut i64, whence: c_int) -> c_int {
        let result = seek_text(cookie.cast(), *offset, whence);
        if result < 0 {
            return -1;
        }
        *offset = result;
        0
    }
    unsafe extern "C" fn close_fn(cookie: *mut c_void) -> c_int {
        close_text(cookie.cast())
    }

    pub unsafe fn wrap(reader: *mut TextReader) -> *mut libc::FILE {
        let functions =
            CookieFunctions { read: Some(read_fn), write: None, seek: Some(seek_fn), close: Some(close_fn) };
        fopencookie(reader.cast(), c"r".as_ptr(), functions)
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut libc::FILE {
    if mode.is_null() {
        return null_mut();
    }
    let Some(native) = to_native(path) else { return null_mut() };
    let (translated, text_read) = translate_mode(CStr::from_ptr(mode).to_bytes());
    let Ok(translated) = CString::new(translated) else { return null_mut() };
    let stream = libc::fopen(native.as_ptr(), translated.as_ptr());
    if stream.is_null() {
        fail_errno();
        return null_mut();
    }
    if !text_read {
        return stream;
    }
    let reader = Box::into_raw(Box::new(TextReader { base: stream, filter: CrLfFilter::default(), position: 0 }));
    let wrapped = text_stream::wrap(reader);
    if wrapped.is_null() {
        libc::fclose(stream);
        drop(Box::from_raw(reader));
    }
    wrapped
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileCreateNew(path: *const c_char) -> PlatFileResult {
    let Some(native) = to_native(path) else { return PlatFileResult::BadPath };
    let file = libc::open(
        native.as_ptr(),
        libc::O_WRONLY | libc::O_CREAT | libc::O_EXCL | libc::O_CLOEXEC,
        0o666 as libc::c_uint,
    );
    if file >= 0 {
        libc::close(file);
        return PlatFileResult::Ok;
    }
    let error = last_error::errno();
    if error == libc::EEXIST {
        set_last_error(ERROR_FILE_EXISTS);
        return PlatFileResult::Exists;
    }
    if error == libc::ENOENT || error == libc::ENOTDIR || error == libc::ENAMETOOLONG {
        set_last_error(ERROR_PATH_NOT_FOUND);
        return PlatFileResult::BadPath;
    }
    set_last_error(last_error::from_errno(error));
    PlatFileResult::NativeError
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileClose(file: *mut PlatFile) -> i32 {
    if file.is_null() {
        return fail(ERROR_INVALID_HANDLE);
    }
    let result = libc::close((*file).descriptor);
    drop(Box::from_raw(file));
    if result == 0 {
        1
    } else {
        fail_errno()
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileRead(file: *mut PlatFile, data: *mut c_void, length: u32, read: *mut u32) -> i32 {
    *read = 0;
    let mut done = 0u32;
    while done < length {
        let n = libc::read((*file).descriptor, data.cast::<u8>().add(done as usize).cast(), (length - done) as usize);
        if n == 0 {
            break;
        }
        if n < 0 {
            let error = last_error::errno();
            if error == libc::EINTR {
                continue;
            }
            *read = done;
            return fail(last_error::from_errno(error));
        }
        done += n as u32;
    }
    *read = done;
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileWrite(
    file: *mut PlatFile,
    data: *const c_void,
    length: u32,
    written: *mut u32,
) -> i32 {
    *written = 0;
    let mut done = 0u32;
    while done < length {
        let n = libc::write((*file).descriptor, data.cast::<u8>().add(done as usize).cast(), (length - done) as usize);
        if n < 0 {
            let error = last_error::errno();
            if error == libc::EINTR {
                continue;
            }
            *written = done;
            return fail(last_error::from_errno(error));
        }
        done += n as u32;
    }
    *written = done;
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileGetSize(file: *mut PlatFile) -> u32 {
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::fstat((*file).descriptor, &mut stat) != 0 {
        fail_errno();
        return 0xFFFF_FFFF;
    }
    stat.st_size as u32
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileSeek(file: *mut PlatFile, offset: i32, origin: i32) -> u32 {
    let whence = match origin {
        PLAT_SEEK_BEGIN => libc::SEEK_SET,
        PLAT_SEEK_CURRENT => libc::SEEK_CUR,
        _ => libc::SEEK_END,
    };
    let position = libc::lseek((*file).descriptor, offset as libc::off_t, whence);
    if position < 0 {
        let error = last_error::errno();
        if error == libc::EINVAL {
            set_last_error(ERROR_NEGATIVE_SEEK);
        } else {
            set_last_error(last_error::from_errno(error));
        }
        return 0xFFFF_FFFF;
    }
    position as u32
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileTruncate(file: *mut PlatFile) -> i32 {
    let position = libc::lseek((*file).descriptor, 0, libc::SEEK_CUR);
    if position < 0 || libc::ftruncate((*file).descriptor, position) != 0 {
        return fail_errno();
    }
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileGetWriteTime(file: *mut PlatFile, time: *mut u64) -> i32 {
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::fstat((*file).descriptor, &mut stat) != 0 {
        return fail_errno();
    }
    *time = write_time_of(&stat);
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileMap(file: *mut PlatFile, view: *mut *const c_void) -> *mut PlatFileMapping {
    let mut stat: libc::stat = std::mem::zeroed();
    if libc::fstat((*file).descriptor, &mut stat) != 0 {
        fail_errno();
        return null_mut();
    }
    if stat.st_size == 0 {
        set_last_error(ERROR_FILE_INVALID); // CreateFileMapping of an empty file: ERROR_FILE_INVALID
        return null_mut();
    }
    let length = stat.st_size as usize;
    let address = libc::mmap(null_mut(), length, libc::PROT_READ, libc::MAP_PRIVATE, (*file).descriptor, 0);
    if address == libc::MAP_FAILED {
        fail_errno();
        return null_mut();
    }
    let mapping = try_box(PlatFileMapping { address, length });
    if mapping.is_null() {
        libc::munmap(address, length);
        set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return null_mut();
    }
    *view = address;
    mapping
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileUnmap(mapping: *mut PlatFileMapping, _view: *const c_void) {
    if mapping.is_null() {
        return;
    }
    libc::munmap((*mapping).address, (*mapping).length);
    drop(Box::from_raw(mapping));
}

// ---- directory search

#[no_mangle]
pub unsafe extern "C" fn PlatFindFirst(pattern: *const c_char, out: *mut PlatFindData) -> *mut PlatFind {
    if pattern.is_null() || *pattern == 0 {
        set_last_error(ERROR_FILE_NOT_FOUND);
        return null_mut();
    }
    // split the pattern into its directory and its name pattern (the last component)
    let text = CStr::from_ptr(pattern).to_bytes();
    let separator = text.iter().rposition(|&c| c == b'\\' || c == b'/');
    let (directory, name): (Vec<u8>, &[u8]) = match separator {
        Some(at) => (text[..=at].to_vec(), &text[at + 1..]),
        None if text.len() >= 2 && text[1] == b':' => {
            let mut volume = text[..2].to_vec();
            volume.push(b'\\');
            (volume, &text[2..])
        }
        None => (b".".to_vec(), text),
    };
    if name.is_empty() {
        set_last_error(ERROR_FILE_NOT_FOUND); // FindFirstFile("dir\") fails
        return null_mut();
    }
    let Ok(directory_c) = CString::new(directory) else { return null_mut() };
    let Some(native) = to_native(directory_c.as_ptr()) else { return null_mut() };
    let dir = libc::opendir(native.as_ptr());
    if dir.is_null() {
        let error = last_error::errno();
        if error == libc::ENOENT || error == libc::ENOTDIR {
            set_last_error(ERROR_PATH_NOT_FOUND);
        } else {
            set_last_error(last_error::from_errno(error));
        }
        return null_mut();
    }
    let wild = name.iter().any(|&c| c == b'*' || c == b'?');
    let mut entries: Vec<PlatFindData> = Vec::new();
    loop {
        let entry = libc::readdir(dir);
        if entry.is_null() {
            break;
        }
        let entry_name = CStr::from_ptr((*entry).d_name.as_ptr()).to_bytes();
        let matched = if wild { wildcard_match(name, entry_name) } else { compare_no_case(name, entry_name) == 0 };
        if !matched || entry_name.len() >= PLAT_MAX_PATH {
            continue;
        }
        let mut entry_path = native.as_bytes().to_vec();
        entry_path.push(b'/');
        entry_path.extend_from_slice(entry_name);
        let Ok(entry_path) = CString::new(entry_path) else { continue };
        let mut stat: libc::stat = std::mem::zeroed();
        if libc::stat(entry_path.as_ptr(), &mut stat) != 0 {
            continue; // a dangling link
        }
        let mut data = PlatFindData { mName: [0; PLAT_MAX_PATH], mAttributes: attributes_of(&stat, entry_name) };
        data.mName[..entry_name.len()].copy_from_slice(entry_name);
        entries.push(data);
    }
    libc::closedir(dir);
    entries.sort_by(|a, b| {
        let (left, right) = (name_of(a), name_of(b));
        let c = compare_no_case(left, right);
        if c != 0 {
            c.cmp(&0)
        } else {
            left.cmp(right)
        }
    });
    if entries.is_empty() {
        set_last_error(ERROR_FILE_NOT_FOUND);
        return null_mut();
    }
    let find = try_box(PlatFind { entries, next: 1 });
    if find.is_null() {
        set_last_error(ERROR_NOT_ENOUGH_MEMORY);
        return null_mut();
    }
    *out = (&(*find).entries)[0];
    find
}

fn name_of(entry: &PlatFindData) -> &[u8] {
    let end = entry.mName.iter().position(|&c| c == 0).unwrap_or(PLAT_MAX_PATH);
    &entry.mName[..end]
}

#[no_mangle]
pub unsafe extern "C" fn PlatFindNext(find: *mut PlatFind, out: *mut PlatFindData) -> PlatFileResult {
    if find.is_null() {
        set_last_error(ERROR_INVALID_HANDLE);
        return PlatFileResult::NativeError;
    }
    if (*find).next >= (*find).entries.len() {
        set_last_error(ERROR_NO_MORE_FILES);
        return PlatFileResult::NoMore;
    }
    let next = (*find).next;
    *out = (&(*find).entries)[next];
    (*find).next += 1;
    PlatFileResult::Ok
}

#[no_mangle]
pub unsafe extern "C" fn PlatFindClose(find: *mut PlatFind) {
    // touches no error state: a failed PlatFindNext's error survives
    if !find.is_null() {
        drop(Box::from_raw(find));
    }
}
