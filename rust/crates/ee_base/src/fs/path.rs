//! `FSPath`'s searches in Rust (LLE lane round 5): a search path of directories and archives. `FindFile` looks in
//! the directories first, in order, then in the archives; `ListFiles` lists the archives' members and the files
//! under each directory (relative to it).
//!
//! The C++ class keeps its two `std::vector`s (of `FSArchive` and `FSFileSpec` objects: `GetArchive` hands out a
//! reference into one) and the vectors' plumbing (`AddArchive`'s close/insert/reopen, the removals); the searches
//! run here over the vectors' element arrays, which are the records `archive_obj.rs` and `spec.rs` work on.

use std::ffi::{c_char, c_long, c_void};

use crate::ustring::{ee_ustring_change_case, ee_ustring_copy};

use crate::fs::archive_obj::*;
use crate::fs::memfile::EeFsMemFile;
use crate::fs::spec::*;
use crate::fs::status;

/// A name handed to `ListFiles`' sink (the C++ inserts it into its `std::set<UString>`).
pub type EeFsPathNameSink = unsafe extern "C" fn(context: *mut c_void, name: *const c_char);

/// `FindFile(name, outFile, outDirectory)`'s search: the directories in order (each searched by a copy of its spec,
/// so no search handle is shared between threads), then the archives (the name lower-cased). A file found in a
/// directory is left in `found` (the C++ copies it into a new `FSFileSpec`) and in `out_directory`; a member found in
/// archive `*out_archive` is handed back in `*out_member` (the C++ wraps and names it) and `out_directory` becomes
/// that archive's file. kErrorNotFound when neither has it.
///
/// # Safety
/// `directories` holds `directory_count` live spec records, `archives` `archive_count` live archive records; `name`
/// is a live `UString` record; `found` and `out_directory` are live spec records; the outputs are writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_find(
    directories: *const EeFsSpec,
    directory_count: usize,
    archives: *const EeFsArchiveObj,
    archive_count: usize,
    name: *const EeFsString,
    found: *mut EeFsSpec,
    out_directory: *mut EeFsSpec,
    out_member: *mut *mut EeFsMemFile,
    out_archive: *mut usize,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
) -> c_long {
    for i in 0..directory_count {
        let mut directory = TempSpec::copy_of(directories.add(i));
        if ee_fs_spec_first_file(&mut directory.0, (*name).string, found, startup_volume, startup_directory)
            == status::OK as c_long
        {
            ee_fs_spec_assign(out_directory, found);
            ee_fs_spec_close_search(&mut directory.0);
            return status::OK as c_long;
        }
    }
    for i in 0..archive_count {
        // FSArchive::FindFile: the name lower-cased, the member found
        let mut lowered = EeFsString::blank();
        crate::ustring::ee_ustring_init_copy(lowered.us(), (*name).usc());
        ee_ustring_change_case(lowered.us(), false);
        let mut member: *mut EeFsMemFile = std::ptr::null_mut();
        let result = ee_fs_arch_find(archives.add(i), lowered.string, &mut member);
        crate::ustring::ee_ustring_free(lowered.us());
        if result == status::OK as c_long {
            ee_fs_spec_assign(out_directory, &(*archives.add(i)).archive_file);
            *out_member = member;
            *out_archive = i;
            return status::OK as c_long;
        }
        if !member.is_null() {
            // a member that failed to decompress (the original leaked its FSMemoryFile)
            drop(Box::from_raw(member));
        }
    }
    status::NOT_FOUND as c_long
}

/// `ListFiles`' walk of the directories: every match of `pattern` in each directory, named relative to it (as a
/// `PLAT_MAX_PATH` string's `Copy` keeps it), handed to `sink`. Returns the last search's result (the end-of-search
/// code when there is a directory), or `previous` (the archives' result) when there is none.
///
/// # Safety
/// `directories` holds `directory_count` live spec records, `pattern` is a C string, `sink` takes `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_path_list_directories(
    directories: *const EeFsSpec,
    directory_count: usize,
    pattern: *const c_char,
    previous: c_long,
    sink: EeFsPathNameSink,
    context: *mut c_void,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
) -> c_long {
    let mut result = previous;
    let mut file = TempSpec::new();
    ee_fs_basic_init(&mut file.0);
    ee_fs_spec_init(&mut file.0, startup_volume, startup_directory);
    let mut name = EeFsString::with_capacity(260);
    for i in 0..directory_count {
        let root = directories.add(i);
        let root_length = (*root).volume.length.wrapping_add((*root).directory.length);
        let mut directory = TempSpec::copy_of(root);
        result = ee_fs_spec_first_file(&mut directory.0, pattern, &mut file.0, startup_volume, startup_directory);
        while result == status::OK as c_long {
            let mut path = Vec::new();
            for s in [&file.0.volume, &file.0.directory, &file.0.file] {
                path.extend_from_slice(s.bytes());
            }
            let mut whole = EeFsString::blank();
            path.push(0);
            crate::ustring::ee_ustring_init_text(whole.us(), path.as_ptr().cast());
            ee_ustring_copy(name.us(), whole.usc(), root_length, whole.length.wrapping_sub(root_length), 0, false);
            crate::ustring::ee_ustring_free(whole.us());
            sink(context, name.string);
            result = ee_fs_spec_next_file(&mut directory.0, &mut file.0);
        }
    }
    crate::ustring::ee_ustring_free(name.us());
    result
}
