//! The C ABI of `FSMemoryFile`, `FSChunk` and `FSArchive` (`include/rust/ee_rust.h`, called by the facades in
//! `src/Low-Level Engine/File System`). Plain forwards: no logic beyond turning pointers into references.

use std::ffi::{c_char, c_void, CStr};
use std::slice;

use crate::archive::{Archive, FatEntry};
use crate::chunk::EeFsChunk;
use crate::file::EeFsFileOps;
use crate::memfile::EeFsMemFile;
use crate::status;

unsafe fn bytes<'a>(data: *const u8, length: usize) -> &'a [u8] {
    if length == 0 {
        &[]
    } else {
        // SAFETY: the caller passes a valid buffer of `length` bytes (each function's contract)
        unsafe { slice::from_raw_parts(data, length) }
    }
}

unsafe fn c_bytes<'a>(text: *const c_char) -> &'a [u8] {
    // SAFETY: the caller passes a NUL-terminated string (each function's contract)
    unsafe { CStr::from_ptr(text) }.to_bytes()
}

// ---- FSMemoryFile ----

/// Creates `FSMemoryFile(inBlockSize, inGrowByDoubling, inBlockCount)`.
#[no_mangle]
pub extern "C" fn ee_fs_mem_new(block_size: usize, grow_by_doubling: bool, block_count: usize) -> *mut EeFsMemFile {
    Box::into_raw(Box::new(EeFsMemFile::new(block_size, grow_by_doubling, block_count)))
}

/// Creates a memory file holding a copy of `data` (`FSMemoryFile(inData, inLength, ...)`; the caller frees its buffer).
///
/// # Safety
/// `data` is valid for `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_new_over(
    data: *const u8,
    length: usize,
    block_size: usize,
    grow_by_doubling: bool,
) -> *mut EeFsMemFile {
    // SAFETY: the function's contract
    let buffer = unsafe { bytes(data, length) }.to_vec().into_boxed_slice();
    Box::into_raw(Box::new(EeFsMemFile::over(buffer, block_size, grow_by_doubling)))
}

/// Frees a memory file (the facade's `~FSMemoryFile`).
///
/// # Safety
/// `file` is null or came from this crate and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_free(file: *mut EeFsMemFile) {
    if !file.is_null() {
        // SAFETY: the function's contract
        drop(unsafe { Box::from_raw(file) });
    }
}

/// `Close`'s data part (the facade closes the stdio stream).
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_close(file: *mut EeFsMemFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { (*file).close() }
}

/// `Compact`.
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_compact(file: *mut EeFsMemFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { (*file).compact() }
}

/// `Compress`.
///
/// # Safety
/// `file` is live, `out_length` valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_compress(file: *mut EeFsMemFile, level: u32, out_length: *mut u32) -> i32 {
    // SAFETY: the function's contract
    let (result, length) = unsafe { (*file).compress(level) };
    unsafe { *out_length = length };
    result
}

/// `Decompress`.
///
/// # Safety
/// `file` is live, `out_length` valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_decompress(file: *mut EeFsMemFile, out_length: *mut u32) -> i32 {
    // SAFETY: the function's contract
    let (result, length) = unsafe { (*file).decompress() };
    unsafe { *out_length = length };
    result
}

/// `Delete`.
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_delete(file: *mut EeFsMemFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { (*file).delete() }
}

/// `GetLength`.
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_length(file: *const EeFsMemFile) -> u32 {
    // SAFETY: the function's contract
    unsafe { (*file).length() as u32 }
}

/// `GetPosition`.
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_position(file: *const EeFsMemFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { (*file).position() as i32 }
}

/// `Open`.
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_open(file: *mut EeFsMemFile) -> i32 {
    // SAFETY: the function's contract
    unsafe { (*file).open() }
}

/// `GetData`; null when the file has no buffer.
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_data(file: *const EeFsMemFile) -> *const u8 {
    // SAFETY: the function's contract
    unsafe { (*file).data() }.unwrap_or(std::ptr::null())
}

/// `Read`: `*out_read` is written unless the file has no buffer (`kErrorNotOpen`, the C++ left it untouched).
///
/// # Safety
/// `file` is live, `out` valid for `length` bytes, `out_read` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_read(
    file: *mut EeFsMemFile,
    out: *mut u8,
    length: usize,
    out_read: *mut u32,
) -> i32 {
    let mut empty = [];
    let slice = if length == 0 {
        &mut empty[..]
    } else {
        // SAFETY: the function's contract
        unsafe { slice::from_raw_parts_mut(out, length) }
    };
    // SAFETY: the function's contract
    let (result, read) = unsafe { (*file).read(slice) };
    if let Some(read) = read {
        // SAFETY: the function's contract
        unsafe { *out_read = read };
    }
    result
}

/// `Write`.
///
/// # Safety
/// `file` is live, `data` valid for `length` bytes, `out_written` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_write(
    file: *mut EeFsMemFile,
    data: *const u8,
    length: usize,
    out_written: *mut u32,
) -> i32 {
    // SAFETY: the function's contract
    let (result, written) = unsafe { (*file).write(bytes(data, length)) };
    // SAFETY: the function's contract
    unsafe { *out_written = written };
    result
}

/// `SetPosition`.
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_set_position(file: *mut EeFsMemFile, offset: i64, origin: u32) -> i32 {
    // SAFETY: the function's contract
    unsafe { (*file).set_position(offset, origin) }
}

/// Whether the file is open (`mIsOpen`, read by the facade).
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_is_open(file: *const EeFsMemFile) -> bool {
    // SAFETY: the function's contract
    unsafe { (*file).is_open() }
}

/// Sets the open flag (`FOpen`, which owns the temporary file, drives it).
///
/// # Safety
/// `file` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_set_open(file: *mut EeFsMemFile, open: bool) {
    // SAFETY: the function's contract
    unsafe { (*file).set_open(open) };
}

/// `Load(inFile, inLength)`.
///
/// # Safety
/// `file` is live, `ops` valid and its callbacks live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_load(file: *mut EeFsMemFile, ops: *const EeFsFileOps, length: usize) -> i32 {
    // SAFETY: the function's contract
    let (file, ops) = unsafe { (&mut *file, &mut *(ops as *mut EeFsFileOps)) };
    file.load(ops, length)
}

/// `Load(inFile)`.
///
/// # Safety
/// As `ee_fs_mem_load`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_load_prefixed(file: *mut EeFsMemFile, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (file, ops) = unsafe { (&mut *file, &mut *(ops as *mut EeFsFileOps)) };
    file.load_prefixed(ops)
}

/// `Save(outFile, inLength)`.
///
/// # Safety
/// As `ee_fs_mem_load`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_save(file: *const EeFsMemFile, ops: *const EeFsFileOps, length: usize) -> i32 {
    // SAFETY: the function's contract
    let (file, ops) = unsafe { (&*file, &mut *(ops as *mut EeFsFileOps)) };
    file.save(ops, length)
}

/// `Save(outFile)`.
///
/// # Safety
/// As `ee_fs_mem_load`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_mem_save_prefixed(file: *mut EeFsMemFile, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (file, ops) = unsafe { (&mut *file, &mut *(ops as *mut EeFsFileOps)) };
    file.save_prefixed(ops)
}

// ---- FSChunk ----

/// `FSChunk()`: fills the embedded state.
///
/// # Safety
/// `chunk` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_init(chunk: *mut EeFsChunk) {
    // SAFETY: the function's contract
    unsafe { chunk.write(EeFsChunk::new()) };
}

/// `~FSChunk`: frees the payload.
///
/// # Safety
/// `chunk` is valid; its `data` is null or live and not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_free(chunk: *mut EeFsChunk) {
    // SAFETY: the function's contract
    unsafe { (*chunk).free() };
}

/// `GetChunkSize`.
///
/// # Safety
/// `chunk` and `out_size` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_size(chunk: *const EeFsChunk, out_size: *mut u32) -> bool {
    // SAFETY: the function's contract
    match unsafe { (*chunk).size() } {
        Some(size) => {
            // SAFETY: as above
            unsafe { *out_size = size };
            true
        }
        None => false,
    }
}

/// `LoadChunkHeader`.
///
/// # Safety
/// `chunk` is valid; `ops` valid and its callbacks live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_load_header(chunk: *mut EeFsChunk, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (chunk, ops) = unsafe { (&mut *chunk, &mut *(ops as *mut EeFsFileOps)) };
    chunk.load_header(ops)
}

/// `LoadChunkFromFile`.
///
/// # Safety
/// As `ee_fs_chunk_load_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_load_from_file(chunk: *mut EeFsChunk, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (chunk, ops) = unsafe { (&mut *chunk, &mut *(ops as *mut EeFsFileOps)) };
    chunk.load_from_file(ops)
}

/// `SaveChunkToFile`.
///
/// # Safety
/// As `ee_fs_chunk_load_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_save_to_file(chunk: *mut EeFsChunk, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (chunk, ops) = unsafe { (&mut *chunk, &mut *(ops as *mut EeFsFileOps)) };
    chunk.save_to_file(ops)
}

/// `SkipChunk`.
///
/// # Safety
/// As `ee_fs_chunk_load_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_skip(chunk: *mut EeFsChunk, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (chunk, ops) = unsafe { (&mut *chunk, &mut *(ops as *mut EeFsFileOps)) };
    chunk.skip(ops)
}

/// `SavePersistentToChunk`'s first half: a new payload, or null when the chunk has one already.
///
/// # Safety
/// `chunk` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_begin_save(chunk: *mut EeFsChunk) -> *mut EeFsMemFile {
    // SAFETY: the function's contract
    unsafe { (*chunk).begin_save() }.unwrap_or(std::ptr::null_mut())
}

/// `SavePersistentToChunk`'s second half.
///
/// # Safety
/// `chunk` is valid and `ee_fs_chunk_begin_save` returned non-null for it.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_chunk_finish_save(chunk: *mut EeFsChunk, major: u32, minor: u32, tag: i32) {
    // SAFETY: the function's contract
    unsafe { (*chunk).finish_save(major, minor, tag) };
}

// ---- FSArchive ----

/// Creates an empty `FSArchive` table set.
#[no_mangle]
pub extern "C" fn ee_fs_archive_new() -> *mut Archive {
    Box::into_raw(Box::new(Archive::new()))
}

/// Copies an archive's tables (the C++ implicit copy constructor copied them).
///
/// # Safety
/// `archive` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_clone(archive: *const Archive) -> *mut Archive {
    // SAFETY: the function's contract
    Box::into_raw(Box::new(unsafe { (*archive).clone() }))
}

/// Frees an archive's tables.
///
/// # Safety
/// `archive` is null or came from this crate and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_free(archive: *mut Archive) {
    if !archive.is_null() {
        // SAFETY: the function's contract
        drop(unsafe { Box::from_raw(archive) });
    }
}

/// Empties the file table.
///
/// # Safety
/// `archive` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_clear_fat(archive: *mut Archive) {
    // SAFETY: the function's contract
    unsafe { (*archive).clear_fat() };
}

/// Empties the tag table.
///
/// # Safety
/// `archive` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_clear_tags(archive: *mut Archive) {
    // SAFETY: the function's contract
    unsafe { (*archive).clear_tags() };
}

/// `std::map::insert` of a FAT entry.
///
/// # Safety
/// `archive` is valid; `name` NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_fat_insert(archive: *mut Archive, name: *const c_char, length: i32) {
    // SAFETY: the function's contract
    unsafe { (*archive).fat_insert(c_bytes(name), FatEntry { start: 0, end: 0, length }) };
}

/// `ComputeFATSize`.
///
/// # Safety
/// `archive` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_fat_size(archive: *const Archive) -> u32 {
    // SAFETY: the function's contract
    unsafe { (*archive).fat_size() }
}

/// `ComputeTagsSize`.
///
/// # Safety
/// `archive` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_tags_size(archive: *const Archive) -> u32 {
    // SAFETY: the function's contract
    unsafe { (*archive).tags_size() }
}

/// `SetFATFileOffsets`.
///
/// # Safety
/// `archive` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_set_offsets(archive: *mut Archive, offset: u32) {
    // SAFETY: the function's contract
    unsafe { (*archive).set_file_offsets(offset) };
}

/// `WriteHeader`.
///
/// # Safety
/// `archive` is valid; `ops` valid and live for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_write_header(archive: *const Archive, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (archive, ops) = unsafe { (&*archive, &mut *(ops as *mut EeFsFileOps)) };
    archive.write_header(ops)
}

/// `ReadHeader`.
///
/// # Safety
/// As `ee_fs_archive_write_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_read_header(archive: *const Archive, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (archive, ops) = unsafe { (&*archive, &mut *(ops as *mut EeFsFileOps)) };
    archive.read_header(ops)
}

/// `WriteFAT`.
///
/// # Safety
/// As `ee_fs_archive_write_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_write_fat(archive: *const Archive, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (archive, ops) = unsafe { (&*archive, &mut *(ops as *mut EeFsFileOps)) };
    archive.write_fat(ops)
}

/// `ReadFAT`.
///
/// # Safety
/// As `ee_fs_archive_write_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_read_fat(archive: *mut Archive, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (archive, ops) = unsafe { (&mut *archive, &mut *(ops as *mut EeFsFileOps)) };
    archive.read_fat(ops)
}

/// `WriteTags`.
///
/// # Safety
/// As `ee_fs_archive_write_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_write_tags(archive: *const Archive, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (archive, ops) = unsafe { (&*archive, &mut *(ops as *mut EeFsFileOps)) };
    archive.write_tags(ops)
}

/// `ReadTags`.
///
/// # Safety
/// As `ee_fs_archive_write_header`.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_read_tags(archive: *mut Archive, ops: *const EeFsFileOps) -> i32 {
    // SAFETY: the function's contract
    let (archive, ops) = unsafe { (&mut *archive, &mut *(ops as *mut EeFsFileOps)) };
    archive.read_tags(ops)
}

/// `SetTag` (always `kErrorOK` in C++).
///
/// # Safety
/// `archive` is valid; `name` and `value` NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_set_tag(archive: *mut Archive, name: *const c_char, value: *const c_char) {
    // SAFETY: the function's contract
    unsafe { (*archive).set_tag(c_bytes(name), c_bytes(value)) };
}

/// Receives a NUL-terminated byte string, valid during the call. `context` is the caller's.
pub type EeFsTextSink = unsafe extern "C" fn(context: *mut c_void, text: *const c_char);

/// `GetTag`: gives the value to `sink` and returns `kErrorOK`, or `kErrorNotFound`.
///
/// # Safety
/// `archive` is valid; `name` NUL-terminated; `sink` callable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_get_tag(
    archive: *const Archive,
    name: *const c_char,
    sink: EeFsTextSink,
    context: *mut c_void,
) -> i32 {
    // SAFETY: the function's contract
    match unsafe { (*archive).get_tag(c_bytes(name)) } {
        Some(value) => {
            give_text(sink, context, value);
            status::OK
        }
        None => status::NOT_FOUND,
    }
}

fn give_text(sink: EeFsTextSink, context: *mut c_void, text: &[u8]) {
    let mut owned = Vec::with_capacity(text.len() + 1);
    owned.extend_from_slice(text);
    owned.push(0);
    // SAFETY: the sink's contract: `owned` is valid during the call
    unsafe { sink(context, owned.as_ptr().cast()) };
}

/// `ListFiles`' selection: each matching name goes to `sink` in order.
///
/// # Safety
/// `archive` is valid; `pattern` NUL-terminated; `sink` callable.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_list(
    archive: *const Archive,
    pattern: *const c_char,
    sink: EeFsTextSink,
    context: *mut c_void,
) {
    // SAFETY: the function's contract
    for name in unsafe { (*archive).list_matching(c_bytes(pattern)) } {
        give_text(sink, context, &name);
    }
}

/// Visits a FAT entry; `name` is valid during the call.
pub type EeFsFatVisitor =
    unsafe extern "C" fn(context: *mut c_void, name: *const c_char, start: i32, end: i32, length: i32);

/// Visits the FAT in order (the `Create` check loop, `ExtractAll`).
///
/// # Safety
/// `archive` is valid; `visit` callable; `visit` must not touch this archive.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_for_each_fat(
    archive: *const Archive,
    visit: EeFsFatVisitor,
    context: *mut c_void,
) {
    // SAFETY: the function's contract
    unsafe {
        (*archive).for_each_fat(|name, entry| {
            let mut owned = name.to_vec();
            owned.push(0);
            visit(context, owned.as_ptr().cast(), entry.start, entry.end, entry.length);
        });
    }
}

/// `FindFile` after the facade lower-cased the name: on a hit `*out_file` receives a new memory file (always, even for
/// a failed decompression, as the C++ `ReadFATFile`) and the status is returned; a miss leaves it and returns
/// `kErrorNotFound`.
///
/// # Safety
/// `archive` is valid, `name` NUL-terminated, `view` valid for `view_length` bytes, `out_file` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_find_file(
    archive: *const Archive,
    name: *const c_char,
    view: *const u8,
    view_length: usize,
    out_file: *mut *mut EeFsMemFile,
) -> i32 {
    // SAFETY: the function's contract
    let (archive, name) = unsafe { (&*archive, c_bytes(name)) };
    let Some(entry) = archive.fat_find(name) else {
        return status::NOT_FOUND;
    };
    // SAFETY: the function's contract
    let (result, file) = Archive::read_member(unsafe { bytes(view, view_length) }, entry);
    // SAFETY: the function's contract
    unsafe { *out_file = Box::into_raw(Box::new(file)) };
    result
}

/// `ReadFATFile` for an entry from `ee_fs_archive_for_each_fat`: a new memory file, as `ee_fs_archive_find_file`.
///
/// # Safety
/// `view` valid for `view_length` bytes, `out_file` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_fs_archive_read_member(
    view: *const u8,
    view_length: usize,
    start: i32,
    length: i32,
    out_file: *mut *mut EeFsMemFile,
) -> i32 {
    // SAFETY: the function's contract
    let (result, file) = Archive::read_member(unsafe { bytes(view, view_length) }, FatEntry { start, end: 0, length });
    // SAFETY: the function's contract
    unsafe { *out_file = Box::into_raw(Box::new(file)) };
    result
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::CString;

    #[test]
    fn mem_round_trip_through_the_c_abi() {
        let file = ee_fs_mem_new(8, true, 1);
        unsafe {
            let mut count = 0;
            assert_eq!(ee_fs_mem_write(file, [1u8, 2, 3].as_ptr(), 3, &mut count), status::OK);
            assert_eq!(count, 3);
            assert_eq!(ee_fs_mem_length(file), 3);
            assert_eq!(ee_fs_mem_set_position(file, 1, 0), status::OK);
            let mut out = [0u8; 2];
            assert_eq!(ee_fs_mem_read(file, out.as_mut_ptr(), 2, &mut count), status::OK);
            assert_eq!(out, [2, 3]);
            assert_eq!(ee_fs_mem_delete(file), status::OK);
            assert!(ee_fs_mem_data(file).is_null());
            let mut untouched = 77;
            assert_eq!(ee_fs_mem_read(file, out.as_mut_ptr(), 2, &mut untouched), status::NOT_OPEN);
            assert_eq!(untouched, 77);
            ee_fs_mem_free(file);
        }
    }

    #[test]
    fn archive_tags_through_the_c_abi() {
        let archive = ee_fs_archive_new();
        let (k, v) = (CString::new("k").unwrap(), CString::new("v").unwrap());
        unsafe {
            ee_fs_archive_set_tag(archive, k.as_ptr(), v.as_ptr());
            let clone = ee_fs_archive_clone(archive);
            let mut got = String::new();
            unsafe extern "C" fn take(ctx: *mut c_void, text: *const c_char) {
                *(ctx as *mut String) = CStr::from_ptr(text).to_str().unwrap().to_string();
            }
            assert_eq!(ee_fs_archive_get_tag(clone, k.as_ptr(), take, (&mut got as *mut String).cast()), status::OK);
            assert_eq!(got, "v");
            ee_fs_archive_free(clone);
            ee_fs_archive_free(archive);
        }
    }

    #[test]
    fn chunk_state_lifecycle_through_the_c_abi() {
        let mut chunk = EeFsChunk::new();
        unsafe {
            ee_fs_chunk_init(&mut chunk);
            let mut size = 9;
            assert!(!ee_fs_chunk_size(&chunk, &mut size));
            assert_eq!(size, 9);
            assert!(!ee_fs_chunk_begin_save(&mut chunk).is_null());
            assert!(ee_fs_chunk_begin_save(&mut chunk).is_null());
            ee_fs_chunk_finish_save(&mut chunk, 1, 2, 3);
            ee_fs_chunk_free(&mut chunk);
            assert!(chunk.data.is_null());
        }
    }
}
