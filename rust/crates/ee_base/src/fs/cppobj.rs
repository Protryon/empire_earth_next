//! The File System's C++ object calls (LLE round 13): what the moved bodies of `FSChunk` (its persistent loads and
//! saves), `FSPersistentList` (the default objects, the registration) and `FSArchive::FindFile` (the member's new
//! `FSMemoryFile`) make on C++ objects, as a table of one-statement C++ calls that File System.cpp registers once
//! (`ee_fs_cpp_register`), as [`crate::fs::cppfile`]'s file calls are.

#![allow(clippy::unnecessary_cast)] // (c_long/c_ulong: 32 bits on Windows, 64 on LP64)

use std::ffi::{c_long, c_ulong, c_void};
use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};

use crate::fs::archive_obj::{ee_fs_arch_find, ee_fs_arch_name_member, EeFsArchiveObj};
use crate::fs::chunk::EeFsChunk;
use crate::fs::memfile::EeFsMemFile;
use crate::fs::persistent_list::{ee_fs_persistent_find, ee_fs_persistent_register};
use crate::fs::spec::EeFsString;
use crate::fs::status;
use crate::ustring::{ee_ustring_change_case, ee_ustring_free, ee_ustring_init_copy};

/// The C++ calls (File System.cpp): each the one call it names. `p` is an `FSPersistent*`.
#[repr(C)]
pub struct EeFsCppCalls {
    /// `new FSMemoryFile(state, true)` (the memory file owns the state).
    pub new_mem_file: unsafe extern "C" fn(state: *mut EeFsMemFile) -> *mut c_void,
    /// `p->CreateDefault()`.
    pub create_default: unsafe extern "C" fn(p: *mut c_void) -> *mut c_void,
    /// `p->Load(major, minor, file)` on an `FSMemoryFile` over `data` (not owned).
    pub load: unsafe extern "C" fn(p: *mut c_void, major: c_ulong, minor: c_ulong, data: *mut EeFsMemFile) -> c_long,
    /// `p->Save(major, minor, file)` on an `FSMemoryFile` over `data` (not owned).
    pub save: unsafe extern "C" fn(p: *mut c_void, major: c_ulong, minor: c_ulong, data: *mut EeFsMemFile) -> c_long,
    /// `p->GetTagID()`.
    pub tag: unsafe extern "C" fn(p: *mut c_void) -> c_long,
    /// `delete p`.
    pub delete: unsafe extern "C" fn(p: *mut c_void),
}

static CALLS: AtomicPtr<EeFsCppCalls> = AtomicPtr::new(ptr::null_mut());

/// Registers the C++ calls (File System.cpp's static initializer); returns true.
///
/// # Safety
/// `calls` lives for the rest of the process.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_cpp_register(calls: *const EeFsCppCalls) -> bool {
    CALLS.store(calls as *mut EeFsCppCalls, Ordering::Release);
    true
}

fn calls() -> &'static EeFsCppCalls {
    let calls = CALLS.load(Ordering::Acquire);
    assert!(!calls.is_null(), "the C++ File System calls are not registered (ee_fs_cpp_register)");
    // SAFETY: registered once for the process (ee_fs_cpp_register's contract)
    unsafe { &*calls }
}

/// `FSPersistentList::CreateDefaultPersistent`: a new default object of the type registered under the tag, or NULL.
#[no_mangle]
pub extern "C" fn ee_fs_persistent_create_default(tag: c_long) -> *mut c_void {
    let prototype = ee_fs_persistent_find(tag as i32);
    if prototype.is_null() {
        return ptr::null_mut();
    }
    // SAFETY: a registered prototype is a live C++ FSPersistent
    unsafe { (calls().create_default)(prototype) }
}

/// `FSPersistentList::RegisterPersistent`: the prototype registered under its tag (kErrorOK); for a tag already
/// taken the prototype is deleted (kErrorAlreadyExists).
///
/// # Safety
/// `p` is a live C++ `FSPersistent` the registry takes over.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_persistent_register_object(p: *mut c_void) -> c_long {
    let c = calls();
    // SAFETY: the function's contract
    unsafe {
        if ee_fs_persistent_register((c.tag)(p) as i32, p) {
            return status::OK as c_long;
        }
        (c.delete)(p);
    }
    status::NOT_SUSPENDED as c_long // kErrorAlreadyExists
}

/// `FSChunk::LoadPersistentFromChunk`: the object registered for the chunk's tag, created and loaded from the chunk
/// data; kErrorNotSuspended without data, kErrorNotFound for an unregistered tag (`*out` NULL then).
///
/// # Safety
/// `chunk` is a live record; `out` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_load_persistent(chunk: *const EeFsChunk, out: *mut *mut c_void) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let chunk = &*chunk;
        if chunk.data.is_null() {
            return status::NOT_SUSPENDED as c_long;
        }
        *out = ee_fs_persistent_create_default(chunk.tag as c_long);
        if (*out).is_null() {
            return status::NOT_FOUND as c_long;
        }
        (calls().load)(*out, chunk.major as c_ulong, chunk.minor as c_ulong, chunk.data)
    }
}

/// `FSChunk::SavePersistentToChunk`: the object saved into new chunk data (kErrorOK without a save when the chunk
/// has data already); the save's error when it fails (fixed: it used to be reported as success).
///
/// # Safety
/// `chunk` is a live record; `p` a live C++ `FSPersistent`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_save_persistent(
    chunk: *mut EeFsChunk,
    major: c_ulong,
    minor: c_ulong,
    p: *mut c_void,
) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let Some(data) = (*chunk).begin_save() else {
            return status::OK as c_long;
        };
        let c = calls();
        let result = (c.save)(p, major, minor, data);
        if result != status::OK as c_long {
            return result;
        }
        (*chunk).finish_save(major as u32, minor as u32, (c.tag)(p) as i32);
    }
    status::OK as c_long
}

/// `FSArchive::FindFile`: the member of the lower-cased name, decompressed into a new C++ `FSMemoryFile` named after
/// the archive (always made on a hit, even when the decompression failed); a miss returns kErrorNotFound.
///
/// # Safety
/// `a` is a live record, `name` a live `UString`, `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_find_file(
    a: *const EeFsArchiveObj,
    name: *const EeFsString,
    out: *mut *mut c_void,
) -> c_long {
    // SAFETY: the function's contract
    unsafe {
        let mut lower = EeFsString::blank();
        ee_ustring_init_copy(lower.us(), (*name).usc());
        ee_ustring_change_case(lower.us(), false);
        let mut data: *mut EeFsMemFile = ptr::null_mut();
        let result = ee_fs_arch_find(a, lower.string, &mut data);
        if !data.is_null() {
            *out = (calls().new_mem_file)(data);
            ee_fs_arch_name_member(a, (*out).cast(), &lower);
        }
        ee_ustring_free(lower.us());
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fs::persistent_list::ee_fs_persistent_take;
    use std::sync::Mutex;

    static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());

    fn log(s: String) {
        LOG.lock().unwrap().push(s);
    }
    unsafe extern "C" fn new_mem_file(state: *mut EeFsMemFile) -> *mut c_void {
        log(format!("new_mem_file {}", !state.is_null()));
        ptr::null_mut()
    }
    unsafe extern "C" fn create_default(p: *mut c_void) -> *mut c_void {
        log(format!("create_default {}", p as usize));
        (p as usize + 1) as *mut c_void
    }
    unsafe extern "C" fn load(p: *mut c_void, major: c_ulong, minor: c_ulong, data: *mut EeFsMemFile) -> c_long {
        log(format!("load {} {major} {minor} {}", p as usize, !data.is_null()));
        7
    }
    unsafe extern "C" fn save(p: *mut c_void, major: c_ulong, minor: c_ulong, data: *mut EeFsMemFile) -> c_long {
        log(format!("save {} {major} {minor} {}", p as usize, !data.is_null()));
        if p as usize == 99 {
            5
        } else {
            0
        }
    }
    unsafe extern "C" fn tag(p: *mut c_void) -> c_long {
        log(format!("tag {}", p as usize));
        (p as usize / 100) as c_long
    }
    unsafe extern "C" fn delete(p: *mut c_void) {
        log(format!("delete {}", p as usize));
    }
    static TABLE: EeFsCppCalls = EeFsCppCalls { new_mem_file, create_default, load, save, tag, delete };

    fn run(f: impl FnOnce()) -> Vec<String> {
        let _guard = crate::fs::persistent_list::TEST_LOCK.lock().unwrap_or_else(|e| e.into_inner());
        // SAFETY: a static table
        unsafe { ee_fs_cpp_register(&TABLE) };
        LOG.lock().unwrap().clear();
        f();
        std::mem::take(&mut *LOG.lock().unwrap())
    }

    #[test]
    fn registration_takes_a_free_tag_and_deletes_a_duplicate() {
        let calls = run(|| unsafe {
            let (a, b) = (77_700usize as *mut c_void, 77_701usize as *mut c_void);
            assert_eq!(ee_fs_persistent_register_object(a), 0);
            assert_eq!(ee_fs_persistent_register_object(b), 2);
            assert_eq!(ee_fs_persistent_create_default(777), (a as usize + 1) as *mut c_void);
            assert!(ee_fs_persistent_create_default(778).is_null());
            assert_eq!(ee_fs_persistent_take(), a);
        });
        assert_eq!(calls, ["tag 77700", "tag 77701", "delete 77701", "create_default 77700"]);
    }

    #[test]
    fn a_chunk_loads_and_saves_its_persistent() {
        let calls = run(|| unsafe {
            let mut chunk = EeFsChunk::new();
            let mut out = std::ptr::dangling_mut::<c_void>();
            assert_eq!(ee_fs_chunk_load_persistent(&chunk, &mut out), 2);
            assert_eq!(out, std::ptr::dangling_mut::<c_void>());
            // a failed save leaves the chunk without a finished payload
            assert_eq!(ee_fs_chunk_save_persistent(&mut chunk, 3, 4, 99usize as *mut c_void), 5);
            let mut chunk = EeFsChunk::new();
            assert_eq!(ee_fs_chunk_save_persistent(&mut chunk, 3, 4, 88_800usize as *mut c_void), 0);
            assert_eq!((chunk.major, chunk.minor, chunk.tag), (3, 4, 888));
            // the chunk has its data: a second save does nothing
            assert_eq!(ee_fs_chunk_save_persistent(&mut chunk, 5, 6, 88_800usize as *mut c_void), 0);
            // the tag 888 is not registered
            assert_eq!(ee_fs_chunk_load_persistent(&chunk, &mut out), 3);
            assert!(out.is_null());
            assert!(ee_fs_persistent_register(888, 55usize as *mut c_void));
            assert_eq!(ee_fs_chunk_load_persistent(&chunk, &mut out), 7);
            assert_eq!(out as usize, 56);
            assert_eq!(ee_fs_persistent_take(), 55usize as *mut c_void);
            chunk.free();
        });
        assert_eq!(
            calls,
            ["save 99 3 4 true", "save 88800 3 4 true", "tag 88800", "create_default 55", "load 56 3 4 true"]
        );
    }
}
