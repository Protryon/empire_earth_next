//! `UWideString`'s storage and members (LLE lane round 4): the record the class holds by value (`EeUWideString`:
//! the buffer, its byte length, its character length and its capacity, the original's four members in their
//! order) and every member's work on it, on the platform's UTF-8 multibyte functions (`PlatMbs*`, decision D6) as
//! the original called them.
//!
//! As for `UString` (`record.rs`), each function is the original's steps in order on raw pointers, the buffer is
//! zero-filled and sized by the capacity, and a replaced buffer is freed after the copy that may read it. The
//! original's quirks stay: `operator=` and `CopyString` shrink a much larger buffer to whole default-sized blocks,
//! `operator+=` tests against the free space plus 2, `GetIndexedSubstring` truncates its result without measuring
//! it again (its lengths are the whole tail's), `Import` measures the input's characters.
//!
//! Changed: `Tokenize` holds a character in 5 bytes (the original's 3 overflowed on a 4-byte UTF-8 character);
//! `sprintf` that does not fit grows the buffer (the original formatted past it).

use std::ffi::{c_char, c_long, c_ulong};
use std::ptr;
use std::sync::atomic::{AtomicUsize, Ordering};

use crate::plat_core::string::{PlatMbsInc, PlatMbsLen, PlatMbsNBCnt, PlatMbsNCmp, PlatMbsNCpy, PlatMbsNInc};

use crate::ustring::record::{allocate, convert_loaded, copy, release, strlen};

/// A `UWideString`'s state, held by value in the C++ class (after its vtable pointer).
#[repr(C)]
#[derive(Debug)]
pub struct EeUWideString {
    /// The buffer (`mString`): `capacity` bytes, at least 1.
    pub string: *mut c_char,
    /// The bytes of the characters (`mByteLength`, `GetTCHARLength`).
    pub byte_length: c_ulong,
    /// The characters (`mLength`, `GetLength`).
    pub length: c_ulong,
    /// The bytes allocated (`mCapacity`).
    pub capacity: c_ulong,
}

/// `UWideString::sDefaultAllocationLength` (256): the default capacity and the block a shrink rounds to.
static DEFAULT_ALLOCATION: AtomicUsize = AtomicUsize::new(256);

fn default_allocation() -> c_ulong {
    DEFAULT_ALLOCATION.load(Ordering::Relaxed) as c_ulong
}

unsafe fn mbslen(p: *const c_char) -> c_ulong {
    PlatMbsLen(p.cast()) as c_ulong
}

unsafe fn nbcnt(p: *const c_char, characters: c_ulong) -> c_ulong {
    PlatMbsNBCnt(p.cast(), characters as usize) as c_ulong
}

unsafe fn ninc(p: *const c_char, characters: c_ulong) -> *mut c_char {
    PlatMbsNInc(p.cast(), characters as usize).cast()
}

/// `strcpy` (overlap allowed).
unsafe fn strcpy(to: *mut c_char, from: *const c_char) {
    copy(from, to, strlen(from) + 1);
}

/// `strcat` (the source measured before the copy).
unsafe fn strcat(to: *mut c_char, from: *const c_char) {
    strcpy(to.add(strlen(to) as usize), from);
}

/// The lengths of what the buffer holds now.
unsafe fn measure(s: *mut EeUWideString) {
    (*s).length = mbslen((*s).string);
    (*s).byte_length = nbcnt((*s).string, (*s).length);
}

/// The buffer for `size` bytes, as `operator=` and `CopyString` size it: a larger size takes a buffer of exactly
/// that; a smaller one shrinks the buffer to whole default-sized blocks when it is a block or more too big. Returns
/// the replaced buffer (to free after the copy) when there is one.
unsafe fn fit(s: *mut EeUWideString, size: c_ulong) -> Option<(*mut c_char, c_ulong)> {
    if size == (*s).capacity {
        return None;
    }
    let old = ((*s).string, (*s).capacity);
    if size > (*s).capacity {
        (*s).capacity = size;
    } else {
        let d = default_allocation();
        let blocks = (size.wrapping_sub(1)) / d + 1;
        if (((*s).capacity.wrapping_sub(1) / d).wrapping_sub(blocks).wrapping_add(1) as c_long) <= 0 {
            return None;
        }
        (*s).capacity = d.wrapping_mul(blocks);
    }
    (*s).string = allocate((*s).capacity);
    Some(old)
}

unsafe fn free_old(old: Option<(*mut c_char, c_ulong)>) {
    if let Some((p, capacity)) = old {
        release(p, capacity);
    }
}

// ------------------------------------------------------------------------------------------- the constructors

/// `UWideString()`: empty, the default capacity.
///
/// # Safety
/// `s` points at a record to initialise.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_init(s: *mut EeUWideString) {
    (*s).capacity = default_allocation();
    (*s).string = allocate((*s).capacity);
    (*s).length = 0;
    (*s).byte_length = 0;
}

/// `UWideString(const UWideString&)`: the source's lengths and capacity, its text.
///
/// # Safety
/// As [`ee_uwstring_init`]; `source` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_init_copy(s: *mut EeUWideString, source: *const EeUWideString) {
    (*s).length = (*source).length;
    (*s).byte_length = (*source).byte_length;
    (*s).capacity = (*source).capacity;
    (*s).string = allocate((*s).capacity);
    // strcpy, kept inside the buffer
    copy((*source).string, (*s).string, (strlen((*source).string) + 1).min((*s).capacity.max(1)));
}

/// `UWideString(const char*)`: room for two bytes a character and the terminator, or the bytes and the terminator
/// when they need more (port, decision D6).
///
/// # Safety
/// As [`ee_uwstring_init`]; `source` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_init_text(s: *mut EeUWideString, source: *const c_char) {
    (*s).length = mbslen(source);
    (*s).byte_length = nbcnt(source, (*s).length);
    (*s).capacity = (*s).length.wrapping_mul(2).wrapping_add(2);
    if (*s).capacity < (*s).byte_length + 1 {
        (*s).capacity = (*s).byte_length + 1;
    }
    (*s).string = allocate((*s).capacity);
    strcpy((*s).string, source);
}

/// `UWideString(unsigned long)`: empty, room for `characters` characters of two bytes and the terminator.
///
/// # Safety
/// As [`ee_uwstring_init`].
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_init_characters(s: *mut EeUWideString, characters: c_ulong) {
    (*s).capacity = characters.wrapping_mul(2).wrapping_add(2);
    (*s).string = allocate((*s).capacity);
    (*s).length = 0;
    (*s).byte_length = 0;
}

/// `~UWideString()`.
///
/// # Safety
/// `s` is a live record; it is dead afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_free(s: *mut EeUWideString) {
    release((*s).string, (*s).capacity);
    (*s).string = ptr::null_mut();
}

/// `UWideString::SetDefaultAllocationLength`.
#[no_mangle]
pub extern "C" fn ee_uwstring_set_default_allocation(length: c_ulong) {
    DEFAULT_ALLOCATION.store(length as usize, Ordering::Relaxed);
}

/// `Clear`: empty, the buffer kept.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_clear(s: *mut EeUWideString) {
    *(*s).string = 0;
    (*s).length = 0;
    (*s).byte_length = 0;
}

/// `Reallocate`: an empty buffer of `size + 1` bytes.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_reallocate(s: *mut EeUWideString, size: c_ulong) {
    release((*s).string, (*s).capacity);
    (*s).length = 0;
    (*s).byte_length = 0;
    (*s).capacity = size.wrapping_add(1);
    (*s).string = allocate((*s).capacity);
}

// ------------------------------------------------------------------------------------------- assignment

/// `operator=(const UWideString&)`: an empty source empties the string (the buffer kept); otherwise the buffer is
/// fitted to the source's capacity and the text copied. Assigning a string to itself does nothing (port: the
/// original's strcpy onto itself).
///
/// # Safety
/// Both are live records.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_assign(s: *mut EeUWideString, source: *const EeUWideString) {
    if ptr::eq(s, source) {
        return;
    }
    if (*source).length == 0 {
        ee_uwstring_clear(s);
        return;
    }
    let old = fit(s, (*source).capacity);
    if !(*source).string.is_null() {
        strcpy((*s).string, (*source).string);
    }
    free_old(old);
    measure(s);
}

/// `operator=(const char*)`: the buffer fitted to the text's bytes plus 2, the text copied.
///
/// # Safety
/// `s` is a live record, `source` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_assign_text(s: *mut EeUWideString, source: *const c_char) {
    let old = fit(s, nbcnt(source, mbslen(source)) + 2);
    strcpy((*s).string, source);
    free_old(old);
    measure(s);
}

/// `CopyString(const UWideString&, count)`: the first `count` characters of the source (an empty source empties
/// the string), the buffer fitted as `operator=` fits it.
///
/// # Safety
/// Both are live records; `count` is at most the source's characters.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_copy(s: *mut EeUWideString, source: *const EeUWideString, count: c_ulong) {
    if (*source).length == 0 {
        ee_uwstring_clear(s);
        return;
    }
    let old = fit(s, (*source).capacity);
    copy_characters(s, (*source).string, count, old);
}

/// `CopyString(const char*, count)`: the first `count` characters of the text, the buffer fitted to their bytes
/// plus 1.
///
/// # Safety
/// `s` is a live record, `source` a C string of at least `count` characters.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_copy_text(s: *mut EeUWideString, source: *const c_char, count: c_ulong) {
    let old = fit(s, nbcnt(source, count) + 1);
    copy_characters(s, source, count, old);
}

unsafe fn copy_characters(
    s: *mut EeUWideString,
    source: *const c_char,
    count: c_ulong,
    old: Option<(*mut c_char, c_ulong)>,
) {
    // the source may be the string's own buffer (`CopyString` of itself): copied out first
    let mut text = crate::ustring::record::text(source).to_vec();
    text.push(0);
    PlatMbsNCpy((*s).string.cast(), text.as_ptr(), count as usize);
    *ninc((*s).string, count) = 0;
    free_old(old);
    measure(s);
}

/// The two `operator+=`s: appends the text when it fits the free space plus 2 (the original's test), else moves to
/// a buffer of exactly the bytes, the text's and 2.
///
/// # Safety
/// `s` is a live record, `source` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_append(s: *mut EeUWideString, source: *const c_char) {
    let length = mbslen((*s).string);
    let added = nbcnt(source, mbslen(source));
    let size = nbcnt((*s).string, length) + added + 2;
    if size < (*s).capacity.wrapping_sub(nbcnt((*s).string, mbslen((*s).string))).wrapping_add(2) {
        strcat((*s).string, source);
    } else {
        let old = ((*s).string, (*s).capacity);
        (*s).capacity = size;
        let fresh = allocate(size);
        strcpy(fresh, old.0);
        strcat(fresh, source);
        (*s).string = fresh;
        release(old.0, old.1);
    }
    measure(s);
}

/// `InsertString`: the text at character `position`, the old tail after it unless `replace`; always into a new
/// buffer, of the needed size when that is more than the capacity or the free space plus 2.
///
/// # Safety
/// `s` is a live record, `source` a C string (it may lie in the string's own buffer).
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_insert(
    s: *mut EeUWideString,
    source: *const c_char,
    position: c_ulong,
    replace: bool,
) {
    let this = (*s).string;
    let added = nbcnt(source, mbslen(source));
    let size = if replace { nbcnt(this, position) + added + 2 } else { nbcnt(this, mbslen(this)) + added + 2 };
    let available = (*s).capacity.wrapping_sub(nbcnt(this, mbslen(this))).wrapping_add(2);
    let old_capacity = (*s).capacity;
    if size > available || size > (*s).capacity {
        (*s).capacity = size;
    }
    let fresh = allocate((*s).capacity);
    PlatMbsNCpy(fresh.cast(), this.cast(), position as usize);
    let insert = ninc(fresh, position);
    strcpy(insert, source);
    if !replace {
        strcat(insert, ninc(this, position));
    }
    release(this, old_capacity);
    (*s).string = fresh;
    measure(s);
}

// ------------------------------------------------------------------------------------------- reads

/// `operator[]`: the first byte of character `index`, 0 outside the length.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_at(s: *const EeUWideString, index: i32) -> c_char {
    if index >= 0 && index < (*s).length as i32 {
        return *ninc((*s).string, index as c_ulong);
    }
    0
}

/// `Find`: the first character position at or after `start` where `needle` occurs, the length when none.
/// `needle_length` is the needle's `mLength` (`UWideString` overload), or `None` for its measured characters
/// (`const char*`).
unsafe fn find(
    s: *const EeUWideString,
    needle: *const c_char,
    needle_length: Option<c_ulong>,
    start: c_ulong,
) -> c_ulong {
    let characters = mbslen(needle);
    let compared = needle_length.unwrap_or(characters);
    let mut index = start;
    let last = (*s).length.wrapping_sub(compared) as c_long;
    let last = if last > 0 { last } else { 0 } as c_ulong;
    let mut result = (*s).length;
    if (*s).length >= characters {
        let mut position = ninc((*s).string, start).cast::<u8>();
        while index <= last {
            if PlatMbsNCmp(position, needle.cast(), compared as usize) == 0 {
                result = index;
                break;
            }
            index += 1;
            position = PlatMbsInc(position);
        }
    }
    result
}

/// `Find(const UWideString&, start)`.
///
/// # Safety
/// Both are live records whose lengths are their texts' (not a `GetIndexedSubstring` result).
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_find(
    s: *const EeUWideString,
    needle: *const EeUWideString,
    start: c_ulong,
) -> c_ulong {
    find(s, (*needle).string, Some((*needle).length), start)
}

/// `Find(const char*, start)`.
///
/// # Safety
/// As [`ee_uwstring_find`]; `needle` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_find_text(
    s: *const EeUWideString,
    needle: *const c_char,
    start: c_ulong,
) -> c_ulong {
    find(s, needle, None, start)
}

/// `GetBytesUsedToSave`: a 4-byte size, the bytes of the characters and the terminator.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_bytes_to_save(s: *const EeUWideString) -> c_ulong {
    nbcnt((*s).string, (*s).length) + 5
}

/// `GetExportSize`: a 4-byte size, the bytes and a 2-byte terminator.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_export_size(s: *const EeUWideString) -> c_ulong {
    strlen((*s).string) + 6
}

/// `Export`: the size (bytes + 2, 4 bytes little-endian), the bytes, a 2-byte zero.
///
/// # Safety
/// `s` is a live record, `out` has room for [`ee_uwstring_export_size`] bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_export(s: *const EeUWideString, out: *mut u8) {
    let length = strlen((*s).string);
    let size = (length as u32).wrapping_add(2);
    ptr::copy_nonoverlapping(size.to_le_bytes().as_ptr(), out, 4);
    copy((*s).string, out.add(4).cast(), length);
    ptr::write_bytes(out.add(4 + length as usize), 0, 2);
}

/// `Import`: a size and that many bytes (`Export`'s form); the buffer grows (emptied) when the size is more than
/// the capacity; the lengths are the input text's. Returns the bytes consumed.
///
/// # Safety
/// `s` is a live record; `data` holds a size and that many bytes, with a terminator among them.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_import(s: *mut EeUWideString, data: *const u8) -> c_ulong {
    let mut size = [0u8; 4];
    ptr::copy_nonoverlapping(data, size.as_mut_ptr(), 4);
    let size = u32::from_le_bytes(size) as c_ulong;
    let text = data.add(4).cast::<c_char>();
    let length = mbslen(text);
    if (*s).capacity < size {
        ee_uwstring_reallocate(s, size);
    }
    copy(text, (*s).string, size);
    (*s).length = length;
    (*s).byte_length = nbcnt((*s).string, length);
    size + 4
}

/// `GetChecksum`'s value: the bytes (signed) weighted by position plus 2 over the character length, times it.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_checksum(s: *const EeUWideString) -> c_ulong {
    let mut sum: c_ulong = 0;
    for i in 0..(*s).length {
        let byte = *(*s).string.add(i as usize) as i8 as i64 as c_ulong;
        sum = sum.wrapping_add(byte.wrapping_mul(i.wrapping_add(2)));
    }
    (*s).length.wrapping_mul(sum)
}

// ------------------------------------------------------------------------------------------- new strings

/// `GetIndexedSubstring(first, last)`: `out` (uninitialised) becomes characters `first` through `last`; its
/// lengths stay the whole tail's from `first` (the original did not measure the cut).
///
/// # Safety
/// `out` is a record to initialise, `s` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_indexed_substring(
    out: *mut EeUWideString,
    s: *const EeUWideString,
    first: c_ulong,
    last: c_ulong,
) {
    ee_uwstring_init(out);
    ee_uwstring_assign_text(out, ninc((*s).string, first));
    *ninc((*out).string, last.wrapping_sub(first).wrapping_add(1)) = 0;
}

/// `operator+`: `out` (uninitialised) is made with room for `characters` characters (the left's length plus the
/// right's length or bytes, plus 1), then becomes `left` and has `right` appended.
///
/// # Safety
/// `out` is a record to initialise, `left` a live record, `right` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_concat(
    out: *mut EeUWideString,
    characters: c_ulong,
    left: *const EeUWideString,
    right: *const c_char,
) {
    ee_uwstring_init_characters(out, characters);
    ee_uwstring_assign(out, left);
    ee_uwstring_append(out, right);
}

/// `Tokenize`: the tokens between separator characters as (first, last) character pairs (empty ones skipped), each
/// character looked up in `separators` with `Find` as the original did. Returns the count (at most `length + 1`).
///
/// # Safety
/// Both are live records whose lengths are their texts'; `out` has room for `2 * (length + 1)` values.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_tokenize(
    s: *const EeUWideString,
    separators: *const EeUWideString,
    out: *mut u32,
) -> u32 {
    let not_found = (*separators).length;
    let (mut start, mut between, mut count) = (0, true, 0usize);
    // one character and a terminator (the original's 3 bytes overflowed on a 4-byte character)
    let mut character = [0 as c_char; 5];
    let mut position = (*s).string.cast::<u8>();
    for i in 0..=(*s).length {
        PlatMbsNCpy(character.as_mut_ptr().cast(), position, 1);
        *ninc(character.as_ptr(), 1) = 0;
        let found = ee_uwstring_find_text(separators, character.as_ptr(), 0) != not_found;
        if between {
            if !found {
                between = false;
                start = i;
            }
        } else if found || i == (*s).length {
            *out.add(2 * count) = start as u32;
            *out.add(2 * count + 1) = i.wrapping_sub(1) as u32;
            count += 1;
            between = true;
        }
        position = PlatMbsInc(position);
    }
    count as u32
}

// ------------------------------------------------------------------------------------------- Load and sprintf

/// `Load`'s buffer for a stored size: grown to `size + 1` when smaller, `size + 1` bytes zeroed.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_load_buffer(s: *mut EeUWideString, size: c_ulong) -> *mut c_char {
    if (*s).capacity < size.wrapping_add(1) {
        release((*s).string, (*s).capacity);
        (*s).capacity = size.wrapping_add(1);
        (*s).string = allocate((*s).capacity);
    }
    ptr::write_bytes((*s).string, 0, size as usize + 1);
    (*s).string
}

/// `Load`'s end after the bytes were read: terminated at `size`, converted from Windows-1252 when they are not
/// UTF-8 or a legacy load runs, measured.
///
/// # Safety
/// `s` is a live record after [`ee_uwstring_load_buffer`] with `size`.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_load_end(s: *mut EeUWideString, size: c_ulong) {
    *(*s).string.add(size as usize) = 0;
    convert_loaded(&mut (*s).string, &mut (*s).capacity);
    measure(s);
}

/// `sprintf(UWideString&, ...)`'s buffer for `size` bytes: the string's own when it fits, else a larger one with
/// the old bytes (the arguments may point into the old buffer).
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_format_buffer(s: *mut EeUWideString, size: c_ulong) -> *mut c_char {
    if size <= (*s).capacity {
        return (*s).string;
    }
    let fresh = allocate(size);
    copy((*s).string, fresh, (*s).capacity);
    fresh
}

/// `sprintf(UWideString&, ...)`'s end: adopts `buffer` (of `size` bytes) if it is not the string's own, measures.
///
/// # Safety
/// `s` is a live record, `buffer` what [`ee_uwstring_format_buffer`] gave for `size`, now terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_uwstring_format_end(s: *mut EeUWideString, buffer: *mut c_char, size: c_ulong) {
    if buffer != (*s).string {
        release((*s).string, (*s).capacity);
        (*s).string = buffer;
        (*s).capacity = size;
    }
    measure(s);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::MaybeUninit;

    fn new_text(t: &[u8]) -> EeUWideString {
        let c = std::ffi::CString::new(t).unwrap();
        let mut s = MaybeUninit::<EeUWideString>::uninit();
        unsafe {
            ee_uwstring_init_text(s.as_mut_ptr(), c.as_ptr());
            s.assume_init()
        }
    }

    fn text(s: &EeUWideString) -> &[u8] {
        unsafe { crate::ustring::record::text(s.string) }
    }

    #[test]
    fn layout_is_the_originals() {
        // the class's members after the vtable pointer: 8 + 3 x 4 bytes, padded to 24 on Windows (sizeof 32 with the
        // vtable pointer, the original's too)
        assert_eq!(std::mem::size_of::<EeUWideString>(), (8 + 3 * std::mem::size_of::<c_ulong>()).next_multiple_of(8));
        assert_eq!(std::mem::align_of::<EeUWideString>(), 8);
    }

    #[test]
    fn lengths_are_characters_and_bytes() {
        let mut s = new_text("a\u{e9}\u{20ac}".as_bytes());
        assert_eq!((s.length, s.byte_length, s.capacity), (3, 6, 8));
        unsafe {
            ee_uwstring_append(&mut s, c"xy".as_ptr());
            assert_eq!((s.length, s.byte_length), (5, 8));
            assert_eq!(ee_uwstring_find_text(&s, c"x".as_ptr(), 0), 3);
            assert_eq!(ee_uwstring_at(&s, 2), 0xe2u8 as c_char);
            ee_uwstring_free(&mut s);
        }
    }

    #[test]
    fn tokenize_takes_four_byte_characters() {
        // U+1F600 is 4 bytes: the original's 3-byte character buffer overflowed on it
        let mut s = new_text("a\u{1F600}b c".as_bytes());
        let mut sep = new_text(b" ");
        let mut pairs = [0u32; 10];
        unsafe {
            assert_eq!(ee_uwstring_tokenize(&s, &sep, pairs.as_mut_ptr()), 2);
            assert_eq!(&pairs[..4], &[0, 2, 4, 4]);
            let mut sep4 = new_text("\u{1F600}".as_bytes());
            assert_eq!(ee_uwstring_tokenize(&s, &sep4, pairs.as_mut_ptr()), 2);
            assert_eq!(&pairs[..4], &[0, 0, 2, 4]);
            ee_uwstring_free(&mut sep4);
            ee_uwstring_free(&mut s);
            ee_uwstring_free(&mut sep);
        }
    }

    #[test]
    fn indexed_substring_keeps_the_tails_lengths() {
        let mut s = new_text(b"abcdef");
        let mut out = MaybeUninit::<EeUWideString>::uninit();
        unsafe {
            ee_uwstring_indexed_substring(out.as_mut_ptr(), &s, 1, 2);
            let mut out = out.assume_init();
            assert_eq!((text(&out), out.length), (&b"bc"[..], 5));
            ee_uwstring_free(&mut out);
            ee_uwstring_free(&mut s);
        }
    }

    #[test]
    fn export_import_round_trip() {
        let mut s = new_text("x\u{e9}".as_bytes());
        let mut data = vec![0u8; unsafe { ee_uwstring_export_size(&s) } as usize];
        unsafe {
            ee_uwstring_export(&s, data.as_mut_ptr());
            assert_eq!(data, [5, 0, 0, 0, b'x', 0xc3, 0xa9, 0, 0]);
            let mut t = new_text(b"");
            assert_eq!(ee_uwstring_import(&mut t, data.as_ptr()), 9);
            assert_eq!((text(&t), t.length, t.capacity), ("x\u{e9}".as_bytes(), 2, 6));
            ee_uwstring_free(&mut t);
            ee_uwstring_free(&mut s);
        }
    }
}
