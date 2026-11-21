//! `FSPath` whole in Rust (LLE round 9): the search path's archives and directories in order, their adding and
//! removing, and every search ([`path_find`], [`path_list_directories`]). The C++ class keeps its vtable and
//! reference count (`UReferenceCounter`) and one member, this object; the archives and directories are C++ objects
//! (`FSArchive`, `FSFileSpec`: `GetArchive` hands out a reference to one) that the C++ calls table creates and
//! deletes, each call one statement ([`EeFsPathCalls`]); everything else on them is Rust already (their records).
//!
//! The original kept two `std::vector`s of the objects by value; this keeps them by pointer, in the same order. A
//! vector's insert or erase copied the objects it moved (closed archives, closed specs): the end states are the same.

use std::ffi::{c_char, c_long, c_ulong, c_void};

use crate::fs::archive_obj::{ee_fs_arch_assign, ee_fs_arch_close, ee_fs_arch_name_member, ee_fs_arch_open, EeFsArchiveObj};
use crate::fs::ffi::ee_fs_archive_list;
use crate::fs::memfile::EeFsMemFile;
use crate::fs::path::{path_find, path_list_directories, EeFsPathNameSink};
use crate::fs::spec::{ee_fs_spec_assign, ee_fs_spec_init, EeFsSpec, EeFsString, TempSpec};
use crate::fs::status;
use crate::ustring::{ee_ustring_change_case, ee_ustring_compare, ee_ustring_free, ee_ustring_init_copy, ee_ustring_init_text};

/// The C++ objects an `FSPath` holds, made and deleted on the C++ side (FSPath.cpp), and the startup volume and
/// directory its searches start from.
#[repr(C)]
pub struct EeFsPathCalls {
    /// `&(*new FSArchive = *archive)`: a copy (the original's `push_back(FSArchive())` and `back() = inArchive`).
    pub archive_new: unsafe extern "C" fn(archive: *const EeFsArchiveObj) -> *mut EeFsArchiveObj,
    /// `delete archive`.
    pub archive_delete: unsafe extern "C" fn(archive: *mut EeFsArchiveObj),
    /// `new FSFileSpec(*spec)`.
    pub spec_new: unsafe extern "C" fn(spec: *const EeFsSpec) -> *mut EeFsSpec,
    /// `delete spec` (any `FSFileSpec`, a memory file too).
    pub spec_delete: unsafe extern "C" fn(spec: *mut EeFsSpec),
    /// `new FSMemoryFile(member, true)` as its `FSFileSpec`.
    pub memory_file_new: unsafe extern "C" fn(member: *mut EeFsMemFile) -> *mut EeFsSpec,
    /// `static_cast<std::set<UString>*>(set)->clear()`.
    pub set_clear: unsafe extern "C" fn(set: *mut c_void),
    /// `static_cast<std::set<UString>*>(set)->insert(UString(name))`.
    pub set_insert: EeFsPathNameSink,
    /// `&gStartupVolume`, `&gStartupDirectory`.
    pub startup_volume: *const EeFsString,
    pub startup_directory: *const EeFsString,
}

/// The search path.
pub struct EeFsPath {
    calls: *const EeFsPathCalls,
    archives: Vec<*mut EeFsArchiveObj>,
    directories: Vec<*mut EeFsSpec>,
}

impl EeFsPath {
    fn calls(&self) -> &EeFsPathCalls {
        // SAFETY: the table is static (the C++'s)
        unsafe { &*self.calls }
    }

    /// `AddArchive(inArchive, inAppend)`: every archive closed, a copy added last (`append`) or first, every archive
    /// opened again.
    ///
    /// # Safety
    /// `archive` is a live archive record.
    pub unsafe fn add_archive(&mut self, archive: *const EeFsArchiveObj, append: bool) {
        for &a in &self.archives {
            // SAFETY: a live archive of this path
            unsafe { ee_fs_arch_close(a) };
        }
        // SAFETY: the caller's contract
        let copy = unsafe { (self.calls().archive_new)(archive) };
        if append {
            self.archives.push(copy);
        } else {
            self.archives.insert(0, copy);
        }
        for &a in &self.archives {
            // SAFETY: as above
            unsafe { ee_fs_arch_open(a) };
        }
    }

    /// `AddDirectory(inDirectory, inAppend)`: a copy added last or first.
    ///
    /// # Safety
    /// `directory` is a live spec record.
    pub unsafe fn add_directory(&mut self, directory: *const EeFsSpec, append: bool) {
        // SAFETY: the caller's contract
        let copy = unsafe { (self.calls().spec_new)(directory) };
        if append {
            self.directories.push(copy);
        } else {
            self.directories.insert(0, copy);
        }
    }

    /// The copy constructor's and `operator=`'s copies of another path's archives and directories, in order.
    ///
    /// # Safety
    /// `other` is live.
    unsafe fn add_all(&mut self, other: *const EeFsPath) {
        // SAFETY: the caller's contract (the lists copied first: `other` may be this path)
        let (archives, directories) = unsafe { ((*other).archives.clone(), (*other).directories.clone()) };
        for a in archives {
            // SAFETY: a live archive of `other`
            unsafe { self.add_archive(a, true) };
        }
        for d in directories {
            // SAFETY: a live spec of `other`
            unsafe { self.add_directory(d, true) };
        }
    }

    /// The archives and directories deleted (`operator=`'s `clear()`s, the destructor).
    fn clear(&mut self) {
        let calls = self.calls;
        for a in self.archives.drain(..) {
            // SAFETY: an archive this path made
            unsafe { ((*calls).archive_delete)(a) };
        }
        for d in self.directories.drain(..) {
            // SAFETY: a spec this path made
            unsafe { ((*calls).spec_delete)(d) };
        }
    }

    /// `FindFile(inName, outFile, outDirectory)`: the directories first, then the archives; a new `FSFileSpec` (or a
    /// memory file named after its archive) in `*out_file`; kErrorNotFound when neither has it.
    ///
    /// # Safety
    /// `name` is a live `UString` record, `out_directory` a live spec record.
    pub unsafe fn find_file(&self, name: *const EeFsString, out_file: &mut *mut EeFsSpec, out_directory: *mut EeFsSpec) -> c_long {
        let calls = self.calls();
        // SAFETY: the startup strings are the C++ globals
        let mut file = unsafe { self.new_spec() };
        let mut member: *mut EeFsMemFile = std::ptr::null_mut();
        let mut archive = 0usize;
        // SAFETY: the caller's contract; the lists hold live records
        let result = unsafe {
            path_find(
                &self.directories,
                &self.archives,
                name,
                &mut file.0,
                out_directory,
                &mut member,
                &mut archive,
                calls.startup_volume,
                calls.startup_directory,
            )
        };
        if result == status::OK as c_long {
            if !member.is_null() {
                // SAFETY: the member found; the archive it came from; a lower-cased copy of the name
                unsafe {
                    let memory_file = (calls.memory_file_new)(member);
                    let mut lowered = EeFsString::blank();
                    ee_ustring_init_copy(lowered.us(), (*name).usc());
                    ee_ustring_change_case(lowered.us(), false);
                    ee_fs_arch_name_member(self.archives[archive], memory_file, &lowered);
                    ee_ustring_free(lowered.us());
                    *out_file = memory_file;
                }
            } else {
                // SAFETY: the spec found
                *out_file = unsafe { (calls.spec_new)(&file.0) };
            }
        }
        result
    }

    /// A local `FSFileSpec()` (the startup volume and directory).
    unsafe fn new_spec(&self) -> TempSpec {
        let mut spec = TempSpec::new();
        // SAFETY: the startup strings are live (the C++ globals)
        unsafe { ee_fs_spec_init(&mut spec.0, self.calls().startup_volume, self.calls().startup_directory) };
        spec
    }

    /// `FindFile(inName)`: whether the path has the file (the found file deleted).
    ///
    /// # Safety
    /// `name` is a live `UString` record.
    pub unsafe fn has_file(&self, name: *const EeFsString) -> c_long {
        // SAFETY: the caller's contract
        unsafe {
            let mut directory = self.new_spec();
            let mut found = std::ptr::null_mut();
            let result = self.find_file(name, &mut found, &mut directory.0);
            if result == status::OK as c_long {
                (self.calls().spec_delete)(found);
            }
            result
        }
    }

    /// `GetFileArchiveStatus(inName, outArchived)`: false when the path lacks the file; else whether it was found in
    /// an archive (its volume, directory and file differ from the directory's it was found in).
    ///
    /// # Safety
    /// `name` is a live `UString` record.
    pub unsafe fn archive_status(&self, name: *const EeFsString, archived: &mut bool) -> bool {
        // SAFETY: the caller's contract
        unsafe {
            let mut directory = self.new_spec();
            let mut found = std::ptr::null_mut();
            if self.find_file(name, &mut found, &mut directory.0) != status::OK as c_long {
                return false;
            }
            let d = &directory.0;
            let f = &*found;
            *archived = !(ee_ustring_compare(f.volume.string, d.volume.string) == 0
                && ee_ustring_compare(f.directory.string, d.directory.string) == 0
                && ee_ustring_compare(f.file.string, d.file.string) == 0);
            (self.calls().spec_delete)(found);
            true
        }
    }

    /// `ListFiles(inPattern, outFiles)`: the set cleared, every archive's matching members, then the files of each
    /// directory; the directories' last search result (the archives' when there is no directory).
    ///
    /// # Safety
    /// `pattern` is a C string, `set` the C++ set.
    pub unsafe fn list_files(&self, pattern: *const c_char, set: *mut c_void) -> c_long {
        let calls = self.calls();
        // SAFETY: the caller's contract; the lists hold live records
        unsafe {
            (calls.set_clear)(set);
            let mut result = status::OK as c_long;
            for &a in &self.archives {
                ee_fs_archive_list((*a).table, pattern, calls.set_insert, set);
                result = status::OK as c_long;
            }
            path_list_directories(
                &self.directories,
                pattern,
                result,
                calls.set_insert,
                set,
                calls.startup_volume,
                calls.startup_directory,
            )
        }
    }
}

/// `FSPath()`.
///
/// # Safety
/// `calls` is static.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_new(calls: *const EeFsPathCalls) -> *mut EeFsPath {
    Box::into_raw(Box::new(EeFsPath { calls, archives: Vec::new(), directories: Vec::new() }))
}

/// `FSPath(inPath)`: a new path with copies of the other's archives (each reopened) and directories.
///
/// # Safety
/// `other` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_new_copy(other: *const EeFsPath) -> *mut EeFsPath {
    // SAFETY: the caller's contract
    unsafe {
        let p = ee_fs_path_new((*other).calls);
        (*p).add_all(other);
        p
    }
}

/// `~FSPath`: the archives and directories deleted.
///
/// # Safety
/// `p` is live and not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_free(p: *mut EeFsPath) {
    // SAFETY: the caller's contract
    let mut p = unsafe { Box::from_raw(p) };
    p.clear();
}

/// `operator=(inPath)`: the archives and directories replaced by copies of the other's.
///
/// # Safety
/// Both are live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_assign(p: *mut EeFsPath, other: *const EeFsPath) {
    // SAFETY: the caller's contract (a self-assignment empties the path, as the original's did)
    unsafe {
        if std::ptr::eq(p, other) {
            (*p).clear();
            return;
        }
        (*p).clear();
        (*p).add_all(other);
    }
}

/// `AddArchive(inArchive, inAppend)`.
///
/// # Safety
/// `p` is live, `archive` a live `FSArchive`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_add_archive(p: *mut EeFsPath, archive: *const EeFsArchiveObj, append: bool) {
    // SAFETY: the caller's contract
    unsafe { (*p).add_archive(archive, append) };
}

/// `AddDirectory(inDirectory, inAppend)`.
///
/// # Safety
/// `p` is live, `directory` a live `FSFileSpec`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_add_directory(p: *mut EeFsPath, directory: *const EeFsSpec, append: bool) {
    // SAFETY: the caller's contract
    unsafe { (*p).add_directory(directory, append) };
}

/// `FindFile(inName, outFile, outDirectory)` (`out_directory` null: a local one, `FindFile(inName, outFile)`).
///
/// # Safety
/// `p` is live, `name` a live `UString`, `out_file` writable, `out_directory` null or a live `FSFileSpec`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_find_file(
    p: *const EeFsPath,
    name: *const EeFsString,
    out_file: *mut *mut EeFsSpec,
    out_directory: *mut EeFsSpec,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        if out_directory.is_null() {
            let mut directory = (*p).new_spec();
            return (*p).find_file(name, &mut *out_file, &mut directory.0);
        }
        (*p).find_file(name, &mut *out_file, out_directory)
    }
}

/// `FindFile(inName)` (`wide`: `inName` is a `UWideString`, searched as a `UString` of its bytes).
///
/// # Safety
/// `p` is live, `name` a live `UString` (or `UWideString`: its text).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_has_file(p: *const EeFsPath, name: *const c_char) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        let mut text = EeFsString::blank();
        ee_ustring_init_text(text.us(), name);
        let result = (*p).has_file(&text);
        ee_ustring_free(text.us());
        result
    }
}

/// `GetFileArchiveStatus(inName, outArchived)` (the name's text: `UString` or `UWideString`).
///
/// # Safety
/// `p` is live, `name` a C string, `archived` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_archive_status(p: *const EeFsPath, name: *const c_char, archived: *mut bool) -> bool {
    // SAFETY: the caller's contract
    unsafe {
        let mut text = EeFsString::blank();
        ee_ustring_init_text(text.us(), name);
        let result = (*p).archive_status(&text, &mut *archived);
        ee_ustring_free(text.us());
        result
    }
}

/// `ListFiles(inPattern, outFiles)`.
///
/// # Safety
/// `p` is live, `pattern` a C string, `set` the C++ `std::set<UString>`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_list_files(p: *const EeFsPath, pattern: *const c_char, set: *mut c_void) -> c_long {
    // SAFETY: the caller's contract
    unsafe { (*p).list_files(pattern, set) }
}

/// `GetArchive(inIndex)`: the archive (null past the end).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_archive(p: *const EeFsPath, index: c_ulong) -> *mut EeFsArchiveObj {
    // SAFETY: the caller's contract
    unsafe { (&(*p).archives).get(index as usize).copied().unwrap_or(std::ptr::null_mut()) }
}

/// `GetArchive(inIndex, outArchive)` / `GetDirectory(inIndex, outDirectory)` (`directory`): the item assigned to
/// `out`; kErrorBeyondEnd past the end.
///
/// # Safety
/// `p` is live; `out` a live `FSArchive` (or `FSFileSpec` for a directory).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_get(p: *const EeFsPath, directory: bool, index: c_ulong, out: *mut c_void) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        let p = &*p;
        if directory {
            match p.directories.get(index as usize) {
                Some(&d) => ee_fs_spec_assign(out.cast(), d),
                None => return status::BEYOND_END as c_long,
            }
        } else {
            match p.archives.get(index as usize) {
                Some(&a) => ee_fs_arch_assign(out.cast(), a),
                None => return status::BEYOND_END as c_long,
            }
        }
        status::OK as c_long
    }
}

/// `GetArchiveCount()` / `GetDirectoryCount()` (`directory`).
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_count(p: *const EeFsPath, directory: bool) -> c_ulong {
    // SAFETY: the caller's contract
    (unsafe { if directory { (&(*p).directories).len() } else { (&(*p).archives).len() } }) as c_ulong
}

/// `RemoveArchive(inIndex)` (the archive closed, then deleted) / `RemoveDirectory(inIndex)` (`directory`);
/// kErrorBeyondEnd past the end.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_remove(p: *mut EeFsPath, directory: bool, index: c_ulong) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        let p = &mut *p;
        let i = index as usize;
        if directory {
            if i >= p.directories.len() {
                return status::BEYOND_END as c_long;
            }
            (p.calls().spec_delete)(p.directories.remove(i));
        } else {
            if i >= p.archives.len() {
                return status::BEYOND_END as c_long;
            }
            ee_fs_arch_close(p.archives[i]);
            (p.calls().archive_delete)(p.archives.remove(i));
        }
        status::OK as c_long
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fs::archive_obj::{ee_fs_arch_finish, ee_fs_arch_init};
    use std::sync::Mutex;

    static LOG: Mutex<Vec<String>> = Mutex::new(Vec::new());

    fn log(s: String) {
        LOG.lock().unwrap_or_else(|e| e.into_inner()).push(s);
    }

    unsafe extern "C" fn archive_new(a: *const EeFsArchiveObj) -> *mut EeFsArchiveObj {
        // a default record, then the source assigned (as the C++ shim)
        let mut b = Box::new(new_archive());
        unsafe { ee_fs_arch_assign(&mut *b, a) };
        log("archive_new".into());
        Box::into_raw(b)
    }
    unsafe extern "C" fn archive_delete(a: *mut EeFsArchiveObj) {
        log("archive_delete".into());
        unsafe { free_archive(*Box::from_raw(a)) };
    }
    unsafe extern "C" fn spec_new(s: *const EeFsSpec) -> *mut EeFsSpec {
        log(format!("spec_new {}", unsafe { String::from_utf8_lossy((*s).file.bytes()) }));
        Box::into_raw(Box::new(unsafe { TempSpec::copy_of(s) }.into_inner()))
    }
    unsafe extern "C" fn spec_delete(s: *mut EeFsSpec) {
        log(format!("spec_delete {}", unsafe { String::from_utf8_lossy((*s).file.bytes()) }));
        drop(TempSpec(*unsafe { Box::from_raw(s) }));
    }
    unsafe extern "C" fn memory_file_new(_: *mut EeFsMemFile) -> *mut EeFsSpec {
        std::ptr::null_mut()
    }
    unsafe extern "C" fn set_clear(_: *mut c_void) {
        log("set_clear".into());
    }
    unsafe extern "C" fn set_insert(_: *mut c_void, _: *const c_char) {}

    /// An `FSArchive()` record: its file spec, then the archive's own fields.
    fn new_archive() -> EeFsArchiveObj {
        let mut a = EeFsArchiveObj {
            vtable: std::ptr::null(),
            archive_file: TempSpec::new().into_inner(),
            table: std::ptr::null_mut(),
            mapping: std::ptr::null_mut(),
            view: std::ptr::null(),
            view_length: 0,
        };
        unsafe { ee_fs_arch_init(&mut a) };
        a
    }
    /// `~FSArchive`: the archive's fields, then its file spec.
    unsafe fn free_archive(mut a: EeFsArchiveObj) {
        unsafe {
            ee_fs_arch_finish(&mut a);
            drop(TempSpec(a.archive_file));
        }
    }

    fn spec(file: &std::ffi::CStr) -> TempSpec {
        let mut s = TempSpec::new();
        unsafe { ee_ustring_init_text(s.0.file.us(), file.as_ptr()) };
        s
    }

    #[test]
    fn directories_and_archives_keep_their_order_and_owners() {
        let _ = std::mem::take(&mut *LOG.lock().unwrap_or_else(|e| e.into_inner()));
        let volume = EeFsString::with_capacity(4);
        let directory = EeFsString::with_capacity(4);
        let calls = Box::leak(Box::new(EeFsPathCalls {
            archive_new,
            archive_delete,
            spec_new,
            spec_delete,
            memory_file_new,
            set_clear,
            set_insert,
            startup_volume: &volume,
            startup_directory: &directory,
        }));
        unsafe {
            let p = ee_fs_path_new(calls);
            let (a, b, c) = (spec(c"a"), spec(c"b"), spec(c"c"));
            ee_fs_path_add_directory(p, &a.0, true);
            ee_fs_path_add_directory(p, &b.0, true);
            ee_fs_path_add_directory(p, &c.0, false);
            assert_eq!(ee_fs_path_count(p, true), 3);
            let mut out = spec(c"");
            assert_eq!(ee_fs_path_get(p, true, 0, &mut out.0 as *mut EeFsSpec as *mut c_void), 0);
            assert_eq!(out.0.file.bytes(), b"c");
            assert_eq!(ee_fs_path_get(p, true, 3, &mut out.0 as *mut EeFsSpec as *mut c_void), status::BEYOND_END as c_long);
            assert_eq!(ee_fs_path_remove(p, true, 1), 0);
            assert_eq!(ee_fs_path_get(p, true, 1, &mut out.0 as *mut EeFsSpec as *mut c_void), 0);
            assert_eq!(out.0.file.bytes(), b"b");
            // an archive (never opened: no file) is copied in and deleted with the path
            let archive = new_archive();
            ee_fs_path_add_archive(p, &archive, true);
            assert_eq!(ee_fs_path_count(p, false), 1);
            assert!(!ee_fs_path_archive(p, 0).is_null());
            assert!(ee_fs_path_archive(p, 1).is_null());
            let q = ee_fs_path_new_copy(p);
            assert_eq!((ee_fs_path_count(q, true), ee_fs_path_count(q, false)), (2, 1));
            ee_fs_path_assign(q, q);
            assert_eq!((ee_fs_path_count(q, true), ee_fs_path_count(q, false)), (0, 0));
            ee_fs_path_free(q);
            ee_fs_path_free(p);
            free_archive(archive);
        }
        let log = LOG.lock().unwrap_or_else(|e| e.into_inner()).clone();
        assert_eq!(&log[..4], ["spec_new a", "spec_new b", "spec_new c", "spec_delete a"]);
        assert_eq!(log.iter().filter(|l| l.starts_with("archive_new")).count(), 2);
        assert_eq!(log.iter().filter(|l| l.starts_with("archive_delete")).count(), 2);
        assert_eq!(log.last().map(String::as_str), Some("spec_delete b"));
    }
}
