//! PlatFile on Win32: the ANSI file functions the engine called itself before, with the same arguments. A `PlatFile*`
//! is the file HANDLE, a `PlatFind*` the search HANDLE, a `PlatFileMapping*` the mapping HANDLE.

use crate::constants::*;
use crate::cstr::{cstr_bytes, strncpy};
use std::ffi::{c_char, c_void};
use std::ptr::{null, null_mut};
use windows_sys::Win32::Foundation::{
    CloseHandle, GetLastError, SetLastError, ERROR_ALREADY_EXISTS, ERROR_FILE_EXISTS, ERROR_INVALID_NAME,
    ERROR_NO_MORE_FILES, ERROR_PATH_NOT_FOUND, FILETIME, GENERIC_READ, GENERIC_WRITE, HANDLE, INVALID_HANDLE_VALUE,
};
use windows_sys::Win32::Storage::FileSystem::{
    CopyFileA, CreateDirectoryA, CreateFileA, DeleteFileA, FindClose, FindFirstFileA, FindNextFileA,
    GetFileAttributesA, GetFileSize, GetFileTime, GetTempFileNameA, GetTempPathA, ReadFile, RemoveDirectoryA,
    SetEndOfFile, SetFileAttributesA, SetFilePointer, WriteFile, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, FILE_BEGIN,
    FILE_CURRENT, FILE_END, FILE_FLAG_SEQUENTIAL_SCAN, FILE_SHARE_READ, OPEN_EXISTING, WIN32_FIND_DATAA,
};
use windows_sys::Win32::System::Environment::GetCurrentDirectoryA;
use windows_sys::Win32::System::Memory::{
    CreateFileMappingA, MapViewOfFile, UnmapViewOfFile, FILE_MAP_READ, MEMORY_MAPPED_VIEW_ADDRESS, PAGE_READONLY,
    SEC_COMMIT,
};

/// Opaque: the file HANDLE.
pub struct PlatFile {
    _opaque: u8,
}
/// Opaque: the mapping HANDLE.
pub struct PlatFileMapping {
    _opaque: u8,
}
/// Opaque: the search HANDLE.
pub struct PlatFind {
    _opaque: u8,
}

fn handle(file: *mut PlatFile) -> HANDLE {
    file.cast()
}

fn result_of(success: i32) -> i32 {
    (success != 0) as i32
}

extern "C" {
    fn fopen(path: *const c_char, mode: *const c_char) -> *mut c_void;
}

unsafe fn copy_find_data(data: &WIN32_FIND_DATAA, out: *mut PlatFindData) {
    let name = cstr_bytes(data.cFileName.as_ptr().cast());
    strncpy(&mut (&mut (*out).mName)[..PLAT_MAX_PATH - 1], name);
    (*out).mName[PLAT_MAX_PATH - 1] = 0;
    (*out).mAttributes = data.dwFileAttributes;
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathGetAttributes(path: *const c_char) -> u32 {
    GetFileAttributesA(path.cast())
}

#[no_mangle]
pub unsafe extern "C" fn PlatPathSetAttributes(path: *const c_char, attributes: u32) -> i32 {
    result_of(SetFileAttributesA(path.cast(), attributes))
}

#[no_mangle]
pub unsafe extern "C" fn PlatDeleteFile(path: *const c_char) -> i32 {
    result_of(DeleteFileA(path.cast()))
}

#[no_mangle]
pub unsafe extern "C" fn PlatCopyFile(source: *const c_char, destination: *const c_char, fail_if_exists: i32) -> i32 {
    result_of(CopyFileA(source.cast(), destination.cast(), (fail_if_exists != 0) as i32))
}

#[no_mangle]
pub unsafe extern "C" fn PlatCreateDirectory(path: *const c_char) -> PlatFileResult {
    if CreateDirectoryA(path.cast(), null()) != 0 {
        return PlatFileResult::Ok;
    }
    if GetLastError() == ERROR_ALREADY_EXISTS {
        PlatFileResult::Exists
    } else {
        PlatFileResult::NativeError
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatRemoveDirectory(path: *const c_char) -> i32 {
    result_of(RemoveDirectoryA(path.cast()))
}

#[no_mangle]
pub unsafe extern "C" fn PlatGetCurrentDirectory(path: *mut c_char, size: u32) -> u32 {
    GetCurrentDirectoryA(size, path.cast())
}

#[no_mangle]
pub unsafe extern "C" fn PlatGetTempDirectory(path: *mut c_char, size: u32) -> u32 {
    GetTempPathA(size, path.cast())
}

#[no_mangle]
pub unsafe extern "C" fn PlatCreateTempFile(directory: *const c_char, prefix: *const c_char, path: *mut c_char) -> i32 {
    (GetTempFileNameA(directory.cast(), prefix.cast(), 0, path.cast()) != 0) as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileOpen(path: *const c_char, access: u32) -> *mut PlatFile {
    let mut desired = 0u32;
    if access & PLAT_FILE_READ != 0 {
        desired |= GENERIC_READ;
    }
    if access & PLAT_FILE_WRITE != 0 {
        desired |= GENERIC_WRITE;
    }
    let file = CreateFileA(
        path.cast(),
        desired,
        FILE_SHARE_READ,
        null(),
        OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN,
        null_mut(),
    );
    if file == INVALID_HANDLE_VALUE {
        null_mut()
    } else {
        file.cast()
    }
}

/// fopen unchanged: a text mode's CR LF is the CRT's.
#[no_mangle]
pub unsafe extern "C" fn PlatFOpen(path: *const c_char, mode: *const c_char) -> *mut c_void {
    fopen(path, mode)
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileCreateNew(path: *const c_char) -> PlatFileResult {
    let file = CreateFileA(path.cast(), 0, 0, null(), CREATE_NEW, FILE_ATTRIBUTE_NORMAL, null_mut());
    if file != INVALID_HANDLE_VALUE {
        CloseHandle(file);
        return PlatFileResult::Ok;
    }
    match GetLastError() {
        ERROR_PATH_NOT_FOUND | ERROR_INVALID_NAME => PlatFileResult::BadPath,
        ERROR_FILE_EXISTS => PlatFileResult::Exists,
        _ => PlatFileResult::NativeError,
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileClose(file: *mut PlatFile) -> i32 {
    result_of(CloseHandle(handle(file)))
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileRead(file: *mut PlatFile, data: *mut c_void, length: u32, read: *mut u32) -> i32 {
    let mut done = 0u32;
    let succeeded = ReadFile(handle(file), data.cast(), length, &mut done, null_mut());
    *read = done;
    (succeeded == 1) as i32
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileWrite(
    file: *mut PlatFile,
    data: *const c_void,
    length: u32,
    written: *mut u32,
) -> i32 {
    let mut done = 0u32;
    let succeeded = WriteFile(handle(file), data.cast(), length, &mut done, null_mut());
    *written = done;
    result_of(succeeded)
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileGetSize(file: *mut PlatFile) -> u32 {
    GetFileSize(handle(file), null_mut())
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileSeek(file: *mut PlatFile, offset: i32, origin: i32) -> u32 {
    let method = match origin {
        PLAT_SEEK_BEGIN => FILE_BEGIN,
        PLAT_SEEK_CURRENT => FILE_CURRENT,
        _ => FILE_END,
    };
    SetFilePointer(handle(file), offset, null_mut(), method)
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileTruncate(file: *mut PlatFile) -> i32 {
    result_of(SetEndOfFile(handle(file)))
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileGetWriteTime(file: *mut PlatFile, time: *mut u64) -> i32 {
    let mut written: FILETIME = std::mem::zeroed();
    if GetFileTime(handle(file), null_mut(), null_mut(), &mut written) == 0 {
        return 0;
    }
    *time = ((written.dwHighDateTime as u64) << 32) | written.dwLowDateTime as u64;
    1
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileMap(file: *mut PlatFile, view: *mut *const c_void) -> *mut PlatFileMapping {
    let mapping = CreateFileMappingA(handle(file), null(), PAGE_READONLY | SEC_COMMIT, 0, 0, null());
    if mapping.is_null() {
        return null_mut();
    }
    let address = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if address.Value.is_null() {
        let error = GetLastError();
        CloseHandle(mapping);
        SetLastError(error);
        return null_mut();
    }
    *view = address.Value;
    mapping.cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatFileUnmap(mapping: *mut PlatFileMapping, view: *const c_void) {
    UnmapViewOfFile(MEMORY_MAPPED_VIEW_ADDRESS { Value: view.cast_mut() });
    CloseHandle(mapping.cast());
}

#[no_mangle]
pub unsafe extern "C" fn PlatFindFirst(pattern: *const c_char, out: *mut PlatFindData) -> *mut PlatFind {
    let mut data: WIN32_FIND_DATAA = std::mem::zeroed();
    let find = FindFirstFileA(pattern.cast(), &mut data);
    if find == INVALID_HANDLE_VALUE {
        return null_mut();
    }
    copy_find_data(&data, out);
    find.cast()
}

#[no_mangle]
pub unsafe extern "C" fn PlatFindNext(find: *mut PlatFind, out: *mut PlatFindData) -> PlatFileResult {
    let mut data: WIN32_FIND_DATAA = std::mem::zeroed();
    if FindNextFileA(find.cast(), &mut data) != 0 {
        copy_find_data(&data, out);
        return PlatFileResult::Ok;
    }
    if GetLastError() == ERROR_NO_MORE_FILES {
        PlatFileResult::NoMore
    } else {
        PlatFileResult::NativeError
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatFindClose(find: *mut PlatFind) {
    let error = GetLastError(); // a failed PlatFindNext's error survives the close
    FindClose(find.cast());
    SetLastError(error);
}
