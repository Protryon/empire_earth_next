//! `FSArchive` whole in Rust (LLE lane round 5): an archive file (`.ssa`: a header, a file table, a tag table and the
//! files, `archive.rs`) opened and mapped, its members found and decompressed, built from a directory walk or a
//! list of files, extracted.
//!
//! The C++ class keeps its vtable (a destructor only) and its members (`FSFileSpec mArchiveFile`, the tables, the
//! mapping); each member forwards this record (`EeFsArchiveObj`, the object from its vtable pointer on) here. The
//! archive file is a by-value `FSFileSpec`, so its calls are `spec.rs`'s directly (the original's calls on it
//! dispatched to `FSFileSpec` too). The memory files the original built for compression are Rust's `EeFsMemFile`s;
//! a member found for a caller is handed back as one for the C++ to wrap in an `FSMemoryFile`.
//!
//! What stays C++: `GetTag`, `SetTag` and `ListFiles` (they fill a `UWideString` and a `std::set`) and `FindFile`'s
//! `new FSMemoryFile` (the names it gives the member are set here, `ee_fs_arch_name_member`).

#![allow(clippy::unnecessary_cast)]

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};
use std::io::Write as _;
use std::ptr;

use crate::ustring::{
    ee_ustring_assign, ee_ustring_change_case, ee_ustring_concat, ee_ustring_concat_text, ee_ustring_copy,
    ee_ustring_find, ee_ustring_free, ee_ustring_init_copy,
};

use crate::fs::archive::{Archive, FatEntry};
use crate::fs::file::{FsFile, FsWrite};
use crate::fs::memfile::EeFsMemFile;
use crate::fs::spec::*;
use crate::fs::status;

/// cbindgen:ignore
mod code {
    pub const NATIVE_ERROR: i32 = 1;
    pub const ALREADY_EXISTS: i32 = 2;
}

/// cbindgen:ignore
mod sys {
    use std::ffi::c_void;
    extern "C" {
        pub fn PlatFileMap(file: *mut c_void, view: *mut *const c_void) -> *mut c_void;
        pub fn PlatFileUnmap(mapping: *mut c_void, view: *const c_void);
    }
}

const FILE: &CStr = c"C:\\EEX\\Low-Level Engine\\File System\\FSArchive.cpp";

/// `ESVERIFY_AT(condition, line)`
fn verify(condition: bool, line: c_ulong, expression: &CStr) {
    if !condition {
        // SAFETY: C strings
        unsafe { crate::error::ee_error_verify(FILE.as_ptr(), line, expression.as_ptr()) };
    }
}

/// `FSArchive` from its vtable pointer on, as the C++ object lays it out.
#[repr(C)]
pub struct EeFsArchiveObj {
    pub vtable: *const c_void,
    /// `mArchiveFile` (an `FSFileSpec` by value)
    pub archive_file: EeFsSpec,
    /// `mTable`: the file and tag tables
    pub table: *mut Archive,
    /// `mMapping`: `PlatFileMap`'s mapping
    pub mapping: *mut c_void,
    /// `mView`
    pub view: *const u8,
    /// `mViewLength`
    pub view_length: c_ulong,
}

/// The archive file as an `FSFile` for the tables' readers and writers (the original's `FSBuildFileOps` over
/// `mArchiveFile`: `Read(uint32_t&)`, `Read(unsigned char*, ...)` and the writes, all `FSFileSpec`'s).
struct SpecFile(*mut EeFsSpec);

impl FsFile for SpecFile {
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        // SAFETY: a live record for the call
        unsafe { ee_fs_spec_read_value(self.0, (value as *mut u32).cast(), 4) as i32 }
    }
    fn read_bytes(&mut self, out: &mut [u8]) -> (i32, u32) {
        let mut read: c_ulong = 0;
        // SAFETY: as above
        let r = unsafe { ee_fs_spec_read(self.0, out.as_mut_ptr(), out.len() as c_ulong, &mut read) };
        (r as i32, read as u32)
    }
}

impl FsWrite for SpecFile {
    fn write_u32(&mut self, value: u32) -> i32 {
        // SAFETY: as above
        unsafe { ee_fs_spec_write_value(self.0, value.to_le_bytes().as_ptr(), 4) as i32 }
    }
    fn write_bytes(&mut self, data: &[u8]) -> (i32, u32) {
        let mut written: c_ulong = 0;
        // SAFETY: as above
        let r = unsafe { ee_fs_spec_write(self.0, data.as_ptr(), data.len() as c_ulong, &mut written) };
        (r as i32, written as u32)
    }
}

/// A `UString` local of the original's (no vtable), freed on drop.
struct Local(EeFsString);

impl Local {
    fn capacity(capacity: c_ulong) -> Local {
        Local(EeFsString::with_capacity(capacity))
    }
    /// `a + b` (a fresh record)
    unsafe fn concat(a: *const EeFsString, b: *const EeFsString) -> Local {
        let mut t = EeFsString::blank();
        ee_ustring_concat(t.us(), (*a).usc(), (*b).usc());
        Local(t)
    }
    /// `a + "text"`
    unsafe fn concat_text(a: *const EeFsString, b: *const c_char) -> Local {
        let mut t = EeFsString::blank();
        ee_ustring_concat_text(t.us(), (*a).usc(), b);
        Local(t)
    }
    unsafe fn copy_of(a: *const EeFsString) -> Local {
        let mut t = EeFsString::blank();
        ee_ustring_init_copy(t.us(), (*a).usc());
        Local(t)
    }
    fn text(text: &[u8]) -> Local {
        let mut t = text.to_vec();
        t.push(0);
        let mut s = EeFsString::blank();
        // SAFETY: a record to initialise, a C string
        unsafe { crate::ustring::ee_ustring_init_text(s.us(), t.as_ptr().cast()) };
        Local(s)
    }
}

impl Drop for Local {
    fn drop(&mut self) {
        // SAFETY: a live record
        unsafe { ee_ustring_free(self.0.us()) };
    }
}

/// The startup strings `FSFileSpec`'s constructors read.
#[derive(Clone, Copy)]
struct Startup(*const EeFsString, *const EeFsString);

impl Startup {
    /// `FSFileSpec()`
    unsafe fn spec(self) -> TempSpec {
        let mut s = TempSpec::new();
        ee_fs_basic_init(&mut s.0);
        ee_fs_spec_init(&mut s.0, self.0, self.1);
        s
    }
    /// `FSFileSpec(const UString&)`
    unsafe fn spec_of(self, path: &Local) -> TempSpec {
        let mut s = TempSpec::new();
        ee_fs_basic_init(&mut s.0);
        ee_fs_spec_init_path(&mut s.0, &path.0, self.0, self.1);
        s
    }
}

/// The file table in order: (name, entry).
unsafe fn fat(a: *const EeFsArchiveObj) -> Vec<(Vec<u8>, FatEntry)> {
    let mut out = Vec::new();
    (*(*a).table).for_each_fat(|name, entry| out.push((name.to_vec(), entry)));
    out
}

unsafe fn view<'a>(a: *const EeFsArchiveObj) -> &'a [u8] {
    if (*a).view.is_null() {
        &[]
    } else {
        std::slice::from_raw_parts((*a).view, (*a).view_length as usize)
    }
}

// ------------------------------------------------------------------------------------------- object life

/// `FSArchive()`'s body: empty tables, nothing mapped (the archive file is the C++ member's `FSFileSpec()`).
///
/// # Safety
/// `a` is a live record (its archive file constructed).
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_init(a: *mut EeFsArchiveObj) {
    (*a).table = Box::into_raw(Box::new(Archive::new()));
    (*a).mapping = ptr::null_mut();
    (*a).view = ptr::null();
    (*a).view_length = 0;
}

/// `FSArchive(const FSFileSpec&)`'s body: as [`ee_fs_arch_init`], then the archive file assigned.
///
/// # Safety
/// As [`ee_fs_arch_init`]; `file` is a live spec.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_init_file(a: *mut EeFsArchiveObj, file: *const EeFsSpec) {
    ee_fs_arch_init(a);
    ee_fs_spec_assign(&mut (*a).archive_file, file);
}

/// The copy constructor's body (the archive file is the C++ member's copy): the tables copied, the view shared
/// (only closed archives are copied: `FSPath::AddArchive`).
///
/// # Safety
/// Both are live records.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_init_copy(a: *mut EeFsArchiveObj, from: *const EeFsArchiveObj) {
    (*a).table = Box::into_raw(Box::new((*(*from).table).clone()));
    (*a).mapping = (*from).mapping;
    (*a).view = (*from).view;
    (*a).view_length = (*from).view_length;
}

/// `~FSArchive()`'s body: closed, the tables freed (the archive file's destructor is the C++ member's).
///
/// # Safety
/// `a` is a live record; its tables are dead afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_finish(a: *mut EeFsArchiveObj) {
    ee_fs_arch_close(a);
    drop(Box::from_raw((*a).table));
    (*a).table = ptr::null_mut();
}

/// `operator=`: closed, then the other archive's file name (not its contents).
///
/// # Safety
/// Both are live records.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_assign(a: *mut EeFsArchiveObj, from: *const EeFsArchiveObj) {
    ee_fs_arch_close(a);
    ee_fs_spec_assign(&mut (*a).archive_file, &(*from).archive_file);
}

/// `SetArchive`: closed, then pointed at another archive file.
///
/// # Safety
/// `a` is a live record, `file` a live spec.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_set_archive(a: *mut EeFsArchiveObj, file: *const EeFsSpec) -> c_long {
    ee_fs_arch_close(a);
    ee_fs_spec_assign(&mut (*a).archive_file, file);
    status::OK as c_long
}

/// `Reset`: the view unmapped, the archive file closed, the file table emptied.
unsafe fn reset(a: *mut EeFsArchiveObj) {
    if !(*a).mapping.is_null() {
        sys::PlatFileUnmap((*a).mapping, (*a).view.cast());
        (*a).mapping = ptr::null_mut();
    }
    (*a).view = ptr::null();
    (*a).view_length = 0;
    ee_fs_spec_close(&mut (*a).archive_file);
    (*(*a).table).clear_fat();
}

/// `Close`: reset and the tags dropped.
///
/// # Safety
/// `a` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_close(a: *mut EeFsArchiveObj) -> c_long {
    reset(a);
    (*(*a).table).clear_tags();
    status::OK as c_long
}

/// `Open`: the archive file opened for reading, its header, file table and tags read, the file mapped; the file
/// closed again on any failure. kErrorAlreadyExists when open.
///
/// # Safety
/// `a` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_open(a: *mut EeFsArchiveObj) -> c_long {
    if (*a).archive_file.is_open {
        return code::ALREADY_EXISTS as c_long;
    }
    let file = ptr::addr_of_mut!((*a).archive_file);
    let mut result = ee_fs_spec_open(file, FS_ACCESS_READ) as i32;
    if result != status::OK {
        return result as c_long;
    }
    let table = &mut *(*a).table;
    result = table.read_header(&mut SpecFile(file));
    if result == status::OK {
        result = table.read_fat(&mut SpecFile(file));
    }
    if result == status::OK {
        result = table.read_tags(&mut SpecFile(file));
    }
    if result == status::OK {
        let mut view: *const c_void = ptr::null();
        let mut length: c_ulong = 0;
        ee_fs_spec_get_length(file, &mut length);
        (*a).mapping = sys::PlatFileMap((*file).handle, &mut view);
        if !(*a).mapping.is_null() {
            (*a).view = view.cast();
            (*a).view_length = length;
        } else {
            // the mapping or the view failed (PlatFileMap closes a mapping without a view)
            record_native();
            result = code::NATIVE_ERROR;
        }
    }
    if result != status::OK {
        ee_fs_spec_close(file);
    }
    result as c_long
}

// ------------------------------------------------------------------------------------------- members

/// `FindFile`'s search after the facade lower-cased the name: on a hit `*out` receives the member as a new memory
/// file (always, even when its decompression failed) and the status is returned; a miss returns kErrorNotFound.
///
/// # Safety
/// `a` is a live record, `name` a C string, `out` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_find(
    a: *const EeFsArchiveObj,
    name: *const c_char,
    out: *mut *mut EeFsMemFile,
) -> c_long {
    let Some(entry) = (*(*a).table).fat_find(CStr::from_ptr(name).to_bytes()) else {
        return status::NOT_FOUND as c_long;
    };
    let (result, file) = Archive::read_member(view(a), entry);
    *out = Box::into_raw(Box::new(file));
    result as c_long
}

/// `FindFile`'s names for the member found (`SetVolume`, `SetDirectory`, `SetFile` on the new `FSMemoryFile`): the
/// archive's volume, its directory and file name as a directory, the lower-cased name.
///
/// # Safety
/// `a` is a live record, `member` the found member's live spec record, `name` the lower-cased name's live record.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_name_member(
    a: *const EeFsArchiveObj,
    member: *mut EeFsSpec,
    name: *const EeFsString,
) {
    let file = &(*a).archive_file;
    ee_ustring_assign((*member).volume.us(), file.volume.usc());
    let directory = Local::concat(&file.directory, &file.file);
    let directory = Local::concat_text(&directory.0, c"\\".as_ptr());
    ee_ustring_assign((*member).directory.us(), directory.0.usc());
    ee_ustring_assign((*member).file.us(), (*name).usc());
}

/// `ReadIntoMemoryFile`: the whole open file into the memory file.
unsafe fn read_into(file: *mut EeFsSpec, memory: &mut EeFsMemFile) -> i32 {
    let mut length: c_ulong = 0;
    ee_fs_spec_get_length(file, &mut length);
    let mut buffer = vec![0u8; length as usize];
    let mut read: c_ulong = 0;
    ee_fs_spec_read(file, buffer.as_mut_ptr(), length, &mut read);
    memory.write(&buffer[..(read as usize).min(buffer.len())]).0
}

/// `FSMemoryFile::Compress` with its assertion
fn compress(memory: &mut EeFsMemFile, level: c_ulong) -> (i32, u32) {
    let (result, length) = memory.compress(level as u32);
    if result == 0x15 {
        verify(false, 140, c"NULL == \"PKWARE implode() error!\"");
    }
    (result, length)
}

/// `ComputeCompressedFileLength`: the file's compressed length (0 when it cannot be read).
unsafe fn compressed_length(file: *const EeFsSpec, level: c_ulong) -> c_ulong {
    let mut copy = TempSpec::copy_of(file);
    let mut memory = EeFsMemFile::new(1000, true, 1);
    let mut length: u32 = 0;
    let result = memory.open();
    if result != status::OK && result != code::ALREADY_EXISTS {
        memory.close();
        return 0;
    }
    if ee_fs_spec_open(&mut copy.0, FS_ACCESS_READ) != status::OK as c_long {
        memory.close();
        return 0;
    }
    if read_into(&mut copy.0, &mut memory) == status::OK {
        let (r, l) = compress(&mut memory, level);
        length = l;
        verify(r == status::OK, 268, c"theResult == keseOK");
    }
    ee_fs_spec_close(&mut copy.0);
    memory.close();
    length as c_ulong
}

/// `ComputeFATFilename`: the file's path relative to the root, into `out` (`Copy` without a resize).
unsafe fn fat_name(root: *const EeFsSpec, file: *const EeFsSpec, out: &mut EeFsString) {
    let root_path = Local::concat(&(*root).volume, &(*root).directory);
    let name = Local::concat(&(*file).volume, &(*file).directory);
    let name = Local::concat(&name.0, &(*file).file);
    let mut offset = ee_ustring_find(name.0.usc(), root_path.0.string, root_path.0.length, 0, false);
    verify(offset == 0, 296, c"theOffset == 0");
    offset = offset.wrapping_add(root_path.0.length);
    verify(offset < name.0.length, 298, c"theOffset < theFilename.GetLength()");
    ee_ustring_copy(out.us(), name.0.usc(), offset, name.0.length.wrapping_sub(offset), 0, false);
}

/// The table entry for one file: its compressed length under its lower-cased relative name.
unsafe fn add_to_fat(
    a: *mut EeFsArchiveObj,
    root: *const EeFsSpec,
    file: *const EeFsSpec,
    level: c_ulong,
    name: &mut Local,
    line: c_ulong,
) {
    let length = compressed_length(file, level);
    fat_name(root, file, &mut name.0);
    let mut key = Local::copy_of(&name.0);
    ee_ustring_change_case(key.0.us(), false);
    verify(key.0.length == name.0.length, line, c"theKey.GetLength() == theFilename.GetLength()");
    (*(*a).table).fat_insert(key.0.bytes(), FatEntry { start: 0, end: 0, length: length as i32 });
}

/// `BuildFAT(root, directory, pattern, level, recursive)`: every file under the directory matching the pattern
/// (recursing when asked) added with its compressed length.
unsafe fn build_fat_walk(
    a: *mut EeFsArchiveObj,
    root: *const EeFsSpec,
    directory: *const EeFsSpec,
    pattern: *const c_char,
    level: c_ulong,
    recursive: bool,
    startup: Startup,
) -> i32 {
    let mut name = Local::capacity(260);
    let mut dir = TempSpec::copy_of(directory);
    let mut file = startup.spec();
    if !ee_fs_spec_is_directory(&dir.0) {
        return status::FAILED;
    }
    let mut r = ee_fs_spec_first_file(&mut dir.0, pattern, &mut file.0, startup.0, startup.1);
    while r == status::OK as c_long {
        if !ee_fs_spec_is_directory(&file.0) {
            add_to_fat(a, root, &file.0, level, &mut name, 137);
        }
        r = ee_fs_spec_next_file(&mut dir.0, &mut file.0);
    }
    if recursive {
        let mut r = ee_fs_spec_first_file(&mut dir.0, c"*.*".as_ptr(), &mut file.0, startup.0, startup.1);
        while r == status::OK as c_long {
            if ee_fs_spec_is_directory(&file.0) {
                build_fat_walk(a, root, &file.0, pattern, level, recursive, startup);
            }
            r = ee_fs_spec_next_file(&mut dir.0, &mut file.0);
        }
    }
    status::OK
}

/// `WriteFATFiles`: every listed file read, compressed and written at its planned offset.
unsafe fn write_fat_files(a: *mut EeFsArchiveObj, root: *const EeFsSpec, level: c_ulong) -> i32 {
    let entries = fat(a);
    let mut file = TempSpec::copy_of(root);
    let archive_file = ptr::addr_of_mut!((*a).archive_file);
    let mut result = status::OK;
    for (name, entry) in &entries {
        if result != status::OK {
            break;
        }
        let mut position: c_long = 0;
        ee_fs_spec_get_position(archive_file, &mut position);
        verify(position as i32 == entry.start, 1435, c"theFilePosition == theEntry.mStart");
        let name = Local::text(name);
        ee_ustring_assign(file.0.file.us(), name.0.usc());
        let mut memory = EeFsMemFile::new(1000, true, 1);
        result = memory.open();
        if result == status::OK || result == code::ALREADY_EXISTS {
            result = ee_fs_spec_open(&mut file.0, FS_ACCESS_READ) as i32;
            if result == status::OK {
                result = read_into(&mut file.0, &mut memory);
                if result == status::OK {
                    let (r, size) = compress(&mut memory, level);
                    result = r;
                    if result == status::OK {
                        verify(size as i32 == entry.length, 1459, c"(int32)theCompressedSize == theEntry.mLength");
                        let mut wrote: c_ulong = 0;
                        let data = memory.data().unwrap_or(ptr::null());
                        result = ee_fs_spec_write(archive_file, data, size as c_ulong, &mut wrote) as i32;
                        verify(wrote == size as c_ulong, 1463, c"theAmountWrote == theCompressedSize");
                        ee_fs_spec_get_position(archive_file, &mut position);
                        if entry.length > 0 {
                            verify(position as i32 == entry.end + 1, 1468, c"theFilePosition == theEntry.mEnd + 1");
                        } else {
                            verify(position as i32 == entry.end, 1472, c"theFilePosition == theEntry.mEnd");
                        }
                    }
                }
                ee_fs_spec_close(&mut file.0);
            }
        }
        memory.close();
    }
    result
}

/// `CheckFATAfterCreate`: the reopened archive's offsets are consistent and every member reads back.
unsafe fn check_fat(a: *const EeFsArchiveObj) {
    let entries = fat(a);
    for (i, (_, file)) in entries.iter().enumerate() {
        if file.length == 0 {
            verify(file.end - file.start == file.length, 452, c"theFile.mEnd - theFile.mStart == theFile.mLength");
        } else {
            verify(
                file.end - file.start + 1 == file.length,
                456,
                c"theFile.mEnd - theFile.mStart + 1 == theFile.mLength",
            );
        }
        if let Some((_, next)) = entries.get(i + 1) {
            verify(
                file.start + file.length == next.start,
                462,
                c"theFile.mStart + theFile.mLength == theFAT[i + 1].mFile.mStart",
            );
            if file.length == 0 {
                verify(file.end == next.start, 465, c"theFile.mEnd == theFAT[i + 1].mFile.mStart");
            } else {
                verify(file.end + 1 == next.start, 469, c"theFile.mEnd + 1 == theFAT[i + 1].mFile.mStart");
            }
        }
        let _member = Archive::read_member(view(a), FatEntry { start: file.start, end: 0, length: file.length });
    }
}

/// The common end of both `Create`s: offsets planned, the archive file recreated and written, reopened and checked.
unsafe fn finish_create(a: *mut EeFsArchiveObj, root: *const EeFsSpec, level: c_ulong) -> i32 {
    let table = &mut *(*a).table;
    table.set_file_offsets(table.tags_size().wrapping_add(table.fat_size()).wrapping_add(12));
    let file = ptr::addr_of_mut!((*a).archive_file);
    ee_fs_basic_delete(file);
    ee_fs_spec_create(file);
    let result = ee_fs_spec_open(file, FS_ACCESS_WRITE) as i32;
    if result != status::OK {
        return result;
    }
    let table = &*(*a).table;
    if table.write_header(&mut SpecFile(file)) == status::OK
        && table.write_fat(&mut SpecFile(file)) == status::OK
        && table.write_tags(&mut SpecFile(file)) == status::OK
    {
        write_fat_files(a, root, level);
    }
    ee_fs_arch_close(a);
    ee_fs_arch_open(a);
    check_fat(a);
    ee_fs_arch_close(a) as i32
}

/// `Create(root, pattern, level, recursive)`: the archive built from the files under the root matching the pattern.
///
/// # Safety
/// `a` is a live record, `root` a live spec, `pattern` a C string, the startup strings live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_create_walk(
    a: *mut EeFsArchiveObj,
    root: *const EeFsSpec,
    pattern: *const c_char,
    level: c_ulong,
    recursive: bool,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
) -> c_long {
    reset(a);
    let startup = Startup(startup_volume, startup_directory);
    let result = build_fat_walk(a, root, root, pattern, level, recursive, startup);
    if result != status::OK {
        return result as c_long;
    }
    finish_create(a, root, level) as c_long
}

/// `Create(root, files, level)`: the archive built from the listed files under the root.
///
/// # Safety
/// `a` is a live record, `root` a live spec, `files` `count` live `UString` records.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_create_list(
    a: *mut EeFsArchiveObj,
    root: *const EeFsSpec,
    files: *const *const EeFsString,
    count: usize,
    level: c_ulong,
) -> c_long {
    reset(a);
    let mut name = Local::capacity(260);
    let mut file = TempSpec::copy_of(root);
    for i in 0..count {
        ee_ustring_assign(file.0.file.us(), (**files.add(i)).usc());
        add_to_fat(a, root, &file.0, level, &mut name, 213);
    }
    finish_create(a, root, level) as c_long
}

/// `ExtractAll`: every member written below the destination (its directory made first), with the original's
/// progress lines on stdout; stops at the first failure.
///
/// # Safety
/// `a` is a live record, `destination` a live spec, the startup strings live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_arch_extract_all(
    a: *mut EeFsArchiveObj,
    destination: *const EeFsSpec,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
) -> c_long {
    let mut out = TextMode(std::io::stdout().lock());
    let result = extract_all_to(a, destination, startup_volume, startup_directory, &mut out);
    let _ = out.flush();
    result
}

/// The C runtime's text-mode stdout the original printed to: on Windows a line ends in CR LF.
struct TextMode<W: std::io::Write>(W);

impl<W: std::io::Write> std::io::Write for TextMode<W> {
    fn write(&mut self, data: &[u8]) -> std::io::Result<usize> {
        if cfg!(windows) {
            for part in data.split_inclusive(|&b| b == b'\n') {
                if let Some(text) = part.strip_suffix(b"\n") {
                    self.0.write_all(text)?;
                    self.0.write_all(b"\r\n")?;
                } else {
                    self.0.write_all(part)?;
                }
            }
        } else {
            self.0.write_all(data)?;
        }
        Ok(data.len())
    }
    fn flush(&mut self) -> std::io::Result<()> {
        self.0.flush()
    }
}

/// [`ee_fs_arch_extract_all`] with its progress lines to `out`.
///
/// # Safety
/// As [`ee_fs_arch_extract_all`].
pub unsafe fn extract_all_to(
    a: *mut EeFsArchiveObj,
    destination: *const EeFsSpec,
    startup_volume: *const EeFsString,
    startup_directory: *const EeFsString,
    out: &mut dyn std::io::Write,
) -> c_long {
    let startup = Startup(startup_volume, startup_directory);
    let mut result = status::OK;
    let (volume, directory) = (&(*destination).volume, &(*destination).directory);
    for (name, entry) in fat(a) {
        if result != status::OK {
            break;
        }
        let shown = |s: &[u8]| String::from_utf8_lossy(s).into_owned();
        let _ = write!(out, "Extracting file {}{}{}...", shown(volume.bytes()), shown(directory.bytes()), shown(&name));
        let name = Local::text(&name);
        let path = Local::concat(volume, directory);
        let path = Local::concat(&path.0, &name.0);
        let mut dir = startup.spec_of(&path);
        ee_ustring_assign((*ptr::addr_of_mut!(dir.0.file)).us(), Local::text(b"").0.usc());
        result = ee_fs_spec_create_directory(&dir.0) as i32;
        if result == code::ALREADY_EXISTS {
            result = status::OK;
        }
        if result != status::OK {
            let _ = writeln!(
                out,
                "extraction faild: unable to create directory {}{}",
                shown(dir.0.volume.bytes()),
                shown(dir.0.directory.bytes())
            );
            continue;
        }
        let (r, member) = Archive::read_member(view(a), FatEntry { start: entry.start, end: 0, length: entry.length });
        result = r;
        if result != status::OK {
            let _ = writeln!(out, "extraction failed: unable to read from archive");
            continue;
        }
        let mut output = startup.spec_of(&path);
        result = ee_fs_spec_create(&mut output.0) as i32;
        if result == code::ALREADY_EXISTS {
            result = status::OK;
        }
        if result != status::OK {
            let _ = writeln!(out, "extraction failed: unable to create file");
            continue;
        }
        result = ee_fs_spec_open(&mut output.0, FS_ACCESS_WRITE) as i32;
        if result != status::OK {
            let _ = writeln!(out, "extraction failed: unable to open file");
            continue;
        }
        let length = member.length() as c_ulong;
        let mut written: c_ulong = 0;
        result = ee_fs_spec_write(&output.0, member.data().unwrap_or(ptr::null()), length, &mut written) as i32;
        if result == status::OK {
            if written == length {
                let _ = writeln!(out, "success!");
            } else {
                let _ = writeln!(out, "extraction failed: unable to write all data to file");
                result = status::FAILED;
            }
        } else {
            let _ = writeln!(out, "extraction failed: unable to write to file");
        }
        ee_fs_spec_close(&mut output.0);
    }
    result as c_long
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fs::spec::tests::text;

    fn archive(file: &EeFsSpec) -> Box<EeFsArchiveObj> {
        let mut a = Box::new(EeFsArchiveObj {
            vtable: ptr::null(),
            archive_file: TempSpec::new().into_inner(),
            table: ptr::null_mut(),
            mapping: ptr::null_mut(),
            view: ptr::null(),
            view_length: 0,
        });
        // SAFETY: live records
        unsafe { ee_fs_arch_init_file(&mut *a, file) };
        a
    }

    #[test]
    fn an_archive_round_trips_through_a_real_directory() {
        let dir = std::env::temp_dir().join(format!("ee_fs_arch_unit_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("in").join("Sub")).unwrap();
        std::fs::write(dir.join("in").join("a.txt"), b"hello hello hello hello hello").unwrap();
        std::fs::write(dir.join("in").join("Sub").join("B.dat"), vec![7u8; 5000]).unwrap();
        std::fs::write(dir.join("in").join("empty.txt"), b"").unwrap();
        let native = format!("{}{}", dir.display(), std::path::MAIN_SEPARATOR);
        let game = if cfg!(windows) { native.clone() } else { format!("C:{}", native.replace('/', "\\")) };
        let (v, d) = (text(&game[..2]), text(&game[2..]));
        let (vin, din) = (text(&game[..2]), text(&format!("{}in\\", &game[2..])));
        // SAFETY: live records
        unsafe {
            let startup = Startup(&v, &d);
            let mut file = startup.spec();
            ee_ustring_assign(file.0.file.us(), text("t.ssa").usc());
            let mut root = startup.spec();
            ee_fs_spec_init(&mut root.0, &vin, &din);
            let mut a = archive(&file.0);
            (*a.table).set_tag(b"Name", b"value");
            assert_eq!(ee_fs_arch_create_walk(&mut *a, &root.0, c"*.*".as_ptr(), 0x1000, true, &v, &d), 0);
            assert_eq!(ee_fs_arch_open(&mut *a), 0);
            assert_eq!(ee_fs_arch_open(&mut *a), code::ALREADY_EXISTS as c_long);
            assert_eq!((*a.table).get_tag(b"Name"), Some(&b"value"[..]));
            let names: Vec<Vec<u8>> = fat(&*a).into_iter().map(|(n, _)| n).collect();
            assert_eq!(names, vec![b"a.txt".to_vec(), b"empty.txt".to_vec(), b"sub\\b.dat".to_vec()]);
            let mut member = ptr::null_mut();
            assert_eq!(ee_fs_arch_find(&*a, c"sub\\b.dat".as_ptr(), &mut member), 0);
            assert_eq!((*member).contents(), &vec![7u8; 5000][..]);
            drop(Box::from_raw(member));
            assert_eq!(ee_fs_arch_find(&*a, c"none".as_ptr(), &mut member), status::NOT_FOUND as c_long);
            let mut out = startup.spec();
            ee_fs_spec_init(&mut out.0, &v, &text(&format!("{}out\\", &game[2..])));
            ee_fs_spec_create_directory(&out.0);
            ee_fs_spec_create_directory(
                &{
                    let mut s = startup.spec();
                    ee_fs_spec_init(&mut s.0, &v, &text(&format!("{}out\\sub\\", &game[2..])));
                    s
                }
                .0,
            );
            assert_eq!(ee_fs_arch_extract_all(&mut *a, &out.0, &v, &d), 0);
            assert_eq!(std::fs::read(dir.join("out").join("sub").join("b.dat")).unwrap(), vec![7u8; 5000]);
            ee_fs_arch_finish(&mut *a);
            ee_fs_spec_finish(&mut a.archive_file);
            ee_ustring_free(a.archive_file.volume.us());
            ee_ustring_free(a.archive_file.directory.us());
            ee_ustring_free(a.archive_file.file.us());
        }
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
