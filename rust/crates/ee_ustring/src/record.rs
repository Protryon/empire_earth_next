//! `UString`'s storage (LLE lane round 4): the record the class holds by value (`EeUString`: the buffer, its
//! length and its capacity, the original's three members in their order, so `sizeof(UString)` and every class
//! embedding one keep their layout) and every member's work on it.
//!
//! Each function is the original member's steps in its order, on raw pointers: a source record is read through its
//! pointer at the point the original read it, so a string copied from itself sees what the original saw (`Copy`
//! from itself with a resize reads the fresh, empty buffer). The buffer is allocated here, zero-filled (the
//! original's bytes past the terminator were the heap's; a `Copy` reads them, so they are now always 0), and its
//! size is always the record's capacity (1 byte for a capacity of 0, which the original allocated as `new
//! char[0]` and then wrote).
//!
//! Fixed where the original wrote outside its buffer (each unreachable from a well-formed string):
//! - `sprintf` that does not fit grows the buffer (the original formatted past it);
//! - a C string assigned to, or appended to, a string of capacity 0 (only a `Copy` resized to `0xFFFFFFFF`
//!   characters makes one) reallocates or appends nothing instead of writing past the 1-byte buffer;
//! - a C string copied (`Copy`) from inside the string's own buffer while it is replaced is read before the old
//!   buffer is freed.

use std::alloc::{alloc_zeroed, dealloc, handle_alloc_error, Layout};
use std::ffi::{c_char, c_int, c_ulong, CStr};
use std::ptr;
use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};

use ee_plat_core::string::{PlatMbsLwr, PlatMbsUpr, PlatStrICmp, PlatStrNICmp};
use ee_plat_core::utf8;

/// A `UString`'s state, held by value in the C++ class (after its vtable pointer).
#[repr(C)]
#[derive(Debug)]
pub struct EeUString {
    /// The buffer (`mString`): `capacity` bytes, at least 1.
    pub string: *mut c_char,
    /// The bytes before the terminator (`mLength`).
    pub length: c_ulong,
    /// The bytes allocated, the terminator included (`mCapacity`).
    pub capacity: c_ulong,
}

/// `UString::sDefaultAllocationLength` (256).
static DEFAULT_ALLOCATION: AtomicUsize = AtomicUsize::new(256);
/// The legacy text load flag (`USetLegacyTextLoad`, decision D6).
static LEGACY_TEXT_LOAD: AtomicBool = AtomicBool::new(false);

fn layout(capacity: c_ulong) -> Layout {
    Layout::from_size_align((capacity as usize).max(1), 1).expect("a string buffer's size")
}

/// `new char[capacity]`, zero-filled.
pub(crate) unsafe fn allocate(capacity: c_ulong) -> *mut c_char {
    let l = layout(capacity);
    let p = alloc_zeroed(l);
    if p.is_null() {
        handle_alloc_error(l);
    }
    p.cast()
}

/// `delete[]` of a buffer of `capacity` bytes.
pub(crate) unsafe fn release(p: *mut c_char, capacity: c_ulong) {
    if !p.is_null() {
        dealloc(p.cast(), layout(capacity));
    }
}

pub(crate) unsafe fn strlen(p: *const c_char) -> c_ulong {
    CStr::from_ptr(p).to_bytes().len() as c_ulong
}

/// `memcpy` (overlap allowed: a string copied into itself).
pub(crate) unsafe fn copy(from: *const c_char, to: *mut c_char, n: c_ulong) {
    ptr::copy(from, to, n as usize);
}

unsafe fn at(s: *mut EeUString, i: c_ulong) -> *mut c_char {
    (*s).string.add(i as usize)
}

/// `mLength = strlen(mString)`.
unsafe fn measure(s: *mut EeUString) {
    (*s).length = strlen((*s).string);
}

/// The C string's bytes (`strcmp`'s view).
pub(crate) unsafe fn text<'a>(p: *const c_char) -> &'a [u8] {
    CStr::from_ptr(p).to_bytes()
}

/// `strcmp`'s sign.
pub fn compare(a: &[u8], b: &[u8]) -> c_int {
    compare_count(a, b, usize::MAX)
}

/// `strncmp`'s sign over C strings' bytes (the terminator compares as 0).
pub fn compare_count(a: &[u8], b: &[u8], n: usize) -> c_int {
    for k in 0..n {
        let (x, y) = (a.get(k).copied().unwrap_or(0), b.get(k).copied().unwrap_or(0));
        if x != y {
            return if x < y { -1 } else { 1 };
        }
        if x == 0 {
            break;
        }
    }
    0
}

// ------------------------------------------------------------------------------------------- the constructors

/// `UString()`: empty, the default capacity.
///
/// # Safety
/// `s` points at a record to initialise (its old contents are ignored).
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_init(s: *mut EeUString) {
    ee_ustring_init_capacity(s, DEFAULT_ALLOCATION.load(Ordering::Relaxed) as c_ulong);
}

/// `UString(unsigned long)`: empty, `capacity` bytes.
///
/// # Safety
/// As [`ee_ustring_init`].
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_init_capacity(s: *mut EeUString, capacity: c_ulong) {
    (*s).capacity = capacity;
    (*s).string = allocate(capacity);
    (*s).length = 0;
    *(*s).string = 0;
}

/// `UString(const UString&)`: the source's length and capacity, its text up to the terminator.
///
/// # Safety
/// As [`ee_ustring_init`]; `source` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_init_copy(s: *mut EeUString, source: *const EeUString) {
    (*s).length = (*source).length;
    (*s).capacity = (*source).capacity;
    (*s).string = allocate((*s).capacity);
    // strcpy, kept inside the buffer
    let n = (strlen((*source).string) + 1).min((*s).capacity.max(1));
    copy((*source).string, (*s).string, n);
}

/// `UString(const char*)`: sized to fit.
///
/// # Safety
/// As [`ee_ustring_init`]; `source` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_init_text(s: *mut EeUString, source: *const c_char) {
    (*s).length = strlen(source);
    (*s).capacity = (*s).length + 1;
    (*s).string = allocate((*s).capacity);
    copy(source, (*s).string, (*s).capacity);
}

/// `~UString()`.
///
/// # Safety
/// `s` is a live record; it is dead afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_free(s: *mut EeUString) {
    release((*s).string, (*s).capacity);
    (*s).string = ptr::null_mut();
}

/// `UString::SetDefaultAllocationLength`.
#[no_mangle]
pub extern "C" fn ee_ustring_set_default_allocation(length: c_ulong) {
    DEFAULT_ALLOCATION.store(length as usize, Ordering::Relaxed);
}

// ------------------------------------------------------------------------------------------- assignment

/// `operator=(const UString&)`: grows to the source's capacity if that is larger, then copies the source's whole
/// buffer (bytes past its terminator too) and measures.
///
/// # Safety
/// Both are live records (the same one allowed).
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_assign(s: *mut EeUString, source: *const EeUString) {
    let mut size = (*source).capacity;
    if size > (*s).capacity {
        release((*s).string, (*s).capacity);
        (*s).string = allocate((*source).capacity);
        (*s).capacity = (*source).capacity;
        size = (*source).capacity;
    }
    if !(*source).string.is_null() {
        copy((*source).string, (*s).string, size);
    }
    if size > 0 {
        *at(s, size - 1) = 0;
        measure(s);
    }
}

/// `operator=(const char*)`: copies into the buffer when the text is shorter than the capacity less one, else
/// replaces the buffer with one that fits exactly.
///
/// # Safety
/// `s` is a live record, `source` a C string (inside the string's own buffer only if it fits).
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_assign_text(s: *mut EeUString, source: *const c_char) {
    let n = strlen(source);
    let mut old = None;
    // (a capacity of 0 has a 1-byte buffer: the original's `capacity - 1` wrapped and it wrote past it)
    if !(n < (*s).capacity.wrapping_sub(1) && (*s).capacity != 0) {
        // the old buffer is freed after the copy: the source may lie in it
        old = Some(((*s).string, (*s).capacity));
        (*s).string = allocate(n + 1);
        (*s).capacity = n + 1;
    }
    copy(source, (*s).string, n + 1);
    *at(s, n) = 0;
    measure(s);
    if let Some(old) = old {
        release(old.0, old.1);
    }
}

// ------------------------------------------------------------------------------------------- reads

/// `operator[]`: the byte at `index`, 0 outside the length.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_at(s: *const EeUString, index: c_int) -> c_char {
    if index >= 0 && index < (*s).length as c_int {
        return *(*s).string.add(index as usize);
    }
    0
}

/// `SizeOf`: the capacity plus the three members.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_size_of(s: *const EeUString) -> c_ulong {
    (*s).capacity.wrapping_add(3 * std::mem::size_of::<c_ulong>() as c_ulong)
}

/// `GetBytesUsedToSave`: a 4-byte length, the text and its terminator.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_bytes_to_save(s: *const EeUString) -> c_ulong {
    (*s).length.wrapping_add(5)
}

/// `strcmp(a, b)`'s sign: the comparison operators and `Equals`.
///
/// # Safety
/// Both are C strings.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_compare(a: *const c_char, b: *const c_char) -> c_int {
    compare(text(a), text(b))
}

/// `strncmp(a, b, count)`'s sign: `Equals` with a count.
///
/// # Safety
/// Both are C strings.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_compare_count(a: *const c_char, b: *const c_char, count: c_ulong) -> c_int {
    compare_count(text(a), text(b), count as usize)
}

/// `EqualsIgnoreCase`: `PlatStrNICmp` over `count` bytes when `counted`, else `PlatStrICmp`.
///
/// # Safety
/// Both are C strings.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_equals_ignore_case(
    a: *const c_char,
    b: *const c_char,
    counted: bool,
    count: c_ulong,
) -> bool {
    if counted {
        PlatStrNICmp(a, b, count as usize) == 0
    } else {
        PlatStrICmp(a, b) == 0
    }
}

/// `ToLower` / `ToUpper` (the platform's in-place case change of UTF-8; the length is not re-measured).
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_change_case(s: *mut EeUString, upper: bool) {
    if upper {
        PlatMbsUpr((*s).string.cast());
    } else {
        PlatMbsLwr((*s).string.cast());
    }
}

// ------------------------------------------------------------------------------------------- edits

/// The two `operator+=`s: appends `count` bytes of `source`, truncated to the space left (never grows).
unsafe fn append(s: *mut EeUString, source: *const c_char, count: impl FnOnce() -> c_ulong) {
    let space = if (*s).capacity == 0 { 0 } else { (*s).capacity.wrapping_sub((*s).length).wrapping_sub(1) };
    if space != 0 {
        let n = count().min(space);
        copy(source, at(s, (*s).length), n);
        *at(s, (*s).length + n) = 0;
        measure(s);
    }
}

/// `operator+=(const UString&)`.
///
/// # Safety
/// Both are live records (the same one allowed).
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_append(s: *mut EeUString, source: *const EeUString) {
    append(s, (*source).string, || (*source).length);
}

/// `operator+=(const char*)`.
///
/// # Safety
/// `s` is a live record, `source` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_append_text(s: *mut EeUString, source: *const c_char) {
    append(s, source, || strlen(source));
}

/// `Compact`: a buffer of exactly the length plus the terminator.
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_compact(s: *mut EeUString) {
    let fresh = allocate((*s).length + 1);
    let old = ((*s).string, (*s).capacity);
    (*s).capacity = (*s).length + 1;
    copy(old.0, fresh, (*s).length);
    *fresh.add((*s).length as usize) = 0;
    release(old.0, old.1);
    (*s).string = fresh;
}

/// `Copy`'s resize: an empty buffer of `count + 1` bytes when `resize` and the capacity is not `count`.
unsafe fn resize_for_copy(s: *mut EeUString, count: c_ulong, resize: bool) -> Option<(*mut c_char, c_ulong)> {
    if resize && (*s).capacity != count {
        let old = ((*s).string, (*s).capacity);
        (*s).capacity = count.wrapping_add(1);
        (*s).string = allocate((*s).capacity);
        (*s).length = 0;
        *(*s).string = 0;
        return Some(old);
    }
    None
}

/// `Copy(const UString&, start, count, destination, resize)`: `count` bytes from `start` of the source's buffer
/// (clamped to its capacity) to `destination`, clamped to the space before the last byte.
///
/// # Safety
/// Both are live records (the same one allowed: it is read through its pointer, after a resize too, as the
/// original read it).
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_copy(
    s: *mut EeUString,
    source: *const EeUString,
    start: c_ulong,
    mut count: c_ulong,
    destination: c_ulong,
    resize: bool,
) {
    if start < (*source).capacity {
        if let Some(old) = resize_for_copy(s, count, resize) {
            release(old.0, old.1);
        }
        if start.wrapping_add(count) >= (*source).capacity {
            count = (*source).capacity - start;
        }
        if destination < (*s).capacity {
            let n = ((*s).capacity - destination - 1).min(count);
            copy((*source).string.add(start as usize), at(s, destination), n);
            *at(s, destination + n) = 0;
            measure(s);
        }
    }
}

/// `Copy(const char*, start, count, destination, resize)`: as [`ee_ustring_copy`] from a C string (clamped to its
/// length); a copy that reaches the last byte grows the buffer by what it needs (the port's fix of the original's
/// terminator written past the buffer).
///
/// # Safety
/// `s` is a live record, `source` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_copy_text(
    s: *mut EeUString,
    source: *const c_char,
    start: c_ulong,
    mut count: c_ulong,
    destination: c_ulong,
    resize: bool,
) {
    let length = strlen(source);
    if start < length {
        // a replaced buffer is freed at the end: the source may lie in it
        let resized = resize_for_copy(s, count, resize);
        let mut grown = None;
        if start.wrapping_add(count) > length {
            count = length - start;
        }
        if destination < (*s).capacity {
            let n = ((*s).capacity - destination).min(count);
            if destination + n >= (*s).capacity {
                let fresh = allocate(destination + n + 1);
                copy((*s).string, fresh, (*s).capacity);
                grown = Some(((*s).string, (*s).capacity));
                (*s).string = fresh;
                (*s).capacity = destination + n + 1;
            }
            copy(source.add(start as usize), at(s, destination), n);
            *at(s, destination + n) = 0;
            measure(s);
        }
        for old in [resized, grown].into_iter().flatten() {
            release(old.0, old.1);
        }
    }
}

// ------------------------------------------------------------------------------------------- new strings

/// `GetSubstring(start, length)`: `out` (uninitialised) becomes `length` bytes from `start`.
///
/// # Safety
/// `out` is a record to initialise, `s` a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_substring(
    out: *mut EeUString,
    s: *const EeUString,
    start: c_ulong,
    length: c_ulong,
) {
    ee_ustring_init_capacity(out, length.wrapping_add(1));
    ee_ustring_copy(out, s, start, length, 0, false);
}

/// `GetIndexedSubstring(first, last)`: `out` (uninitialised) becomes the bytes from `first` through `last`.
///
/// # Safety
/// As [`ee_ustring_substring`].
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_indexed_substring(
    out: *mut EeUString,
    s: *const EeUString,
    first: c_ulong,
    last: c_ulong,
) {
    let length = last.wrapping_sub(first);
    ee_ustring_init_capacity(out, length.wrapping_add(2));
    ee_ustring_copy(out, s, first, length.wrapping_add(1), 0, false);
}

/// `operator+(const UString&, const UString&)`: `out` (uninitialised) becomes `left` then `right`.
///
/// # Safety
/// `out` is a record to initialise, `left` and `right` live records.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_concat(out: *mut EeUString, left: *const EeUString, right: *const EeUString) {
    ee_ustring_init_capacity(out, (*left).length + (*right).length + 1);
    ee_ustring_assign(out, left);
    ee_ustring_append(out, right);
}

/// `operator+(const UString&, const char*)`.
///
/// # Safety
/// As [`ee_ustring_concat`]; `right` is a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_concat_text(out: *mut EeUString, left: *const EeUString, right: *const c_char) {
    ee_ustring_init_capacity(out, (*left).length + strlen(right) + 1);
    ee_ustring_assign(out, left);
    ee_ustring_append_text(out, right);
}

// ------------------------------------------------------------------------------------------- Load and sprintf

/// `USetLegacyTextLoad` (decision D6).
#[no_mangle]
pub extern "C" fn ee_ustring_set_legacy_text_load(legacy: bool) {
    LEGACY_TEXT_LOAD.store(legacy, Ordering::Relaxed);
}

/// `UIsLegacyTextLoad`.
#[no_mangle]
pub extern "C" fn ee_ustring_is_legacy_text_load() -> bool {
    LEGACY_TEXT_LOAD.load(Ordering::Relaxed)
}

/// `Load`'s buffer for a stored length `length` (the length's bytes follow): a fresh buffer of `length + 1`.
///
/// # Safety
/// `s` is a live record; the result holds `length + 1` bytes until the next call on `s`.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_load_buffer(s: *mut EeUString, length: c_ulong) -> *mut c_char {
    release((*s).string, (*s).capacity);
    (*s).capacity = length.wrapping_add(1);
    (*s).string = allocate((*s).capacity);
    (*s).string
}

/// `Load`'s end: with `read` the `length` bytes are in the buffer: terminated, converted from Windows-1252 when
/// they are not UTF-8 or a legacy load runs (`UConvertLoadedText`), measured. Without, the string is emptied (also
/// a failed read of the length).
///
/// # Safety
/// `s` is a live record (after [`ee_ustring_load_buffer`] with `length` when `read`).
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_load_end(s: *mut EeUString, read: bool, length: c_ulong) {
    if !read {
        if !(*s).string.is_null() {
            *(*s).string = 0;
        }
        (*s).length = 0;
        return;
    }
    *at(s, length) = 0;
    convert_loaded(&mut (*s).string, &mut (*s).capacity);
    measure(s);
}

/// `UConvertLoadedText`: a loaded text (up to its terminator) that is not well-formed UTF-8, or any while a legacy
/// load runs, is converted from Windows-1252 into a fresh buffer of exactly its size.
pub(crate) unsafe fn convert_loaded(string: &mut *mut c_char, capacity: &mut c_ulong) {
    let bytes = text(*string);
    if ee_ustring_is_legacy_text_load() || !utf8::is_valid(bytes) {
        let converted = utf8::from_cp1252(bytes);
        let fresh = allocate(converted.len() as c_ulong + 1);
        copy(converted.as_ptr().cast(), fresh, converted.len() as c_ulong);
        release(*string, *capacity);
        *string = fresh;
        *capacity = converted.len() as c_ulong + 1;
    }
}

/// `sprintf(UString&, ...)`'s buffer for a text of `size` bytes with its terminator: the string's own when it fits,
/// else a larger one holding the old bytes (the arguments may point into the old buffer, which stays until
/// [`ee_ustring_format_end`]).
///
/// # Safety
/// `s` is a live record.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_format_buffer(s: *mut EeUString, size: c_ulong) -> *mut c_char {
    if size <= (*s).capacity {
        return (*s).string;
    }
    let fresh = allocate(size);
    copy((*s).string, fresh, (*s).capacity);
    fresh
}

/// `sprintf(UString&, ...)`'s end: adopts `buffer` (of `size` bytes) if it is not the string's own, measures.
///
/// # Safety
/// `s` is a live record, `buffer` what [`ee_ustring_format_buffer`] gave for `size`, now terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_format_end(s: *mut EeUString, buffer: *mut c_char, size: c_ulong) {
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

    pub(crate) fn new_text(t: &[u8]) -> EeUString {
        let c = std::ffi::CString::new(t).unwrap();
        let mut s = MaybeUninit::<EeUString>::uninit();
        unsafe {
            ee_ustring_init_text(s.as_mut_ptr(), c.as_ptr());
            s.assume_init()
        }
    }

    fn bytes(s: &EeUString) -> &[u8] {
        unsafe { std::slice::from_raw_parts(s.string.cast::<u8>(), s.capacity as usize) }
    }

    #[test]
    fn layout_is_the_originals() {
        assert_eq!(std::mem::size_of::<EeUString>(), 8 + 2 * std::mem::size_of::<c_ulong>());
        assert_eq!(std::mem::align_of::<EeUString>(), 8);
    }

    #[test]
    fn append_never_grows_and_copy_reads_stale_bytes() {
        unsafe {
            let mut s = new_text(b"hello");
            ee_ustring_append_text(&mut s, c"xyz".as_ptr());
            assert_eq!((s.length, s.capacity), (5, 6));
            ee_ustring_assign_text(&mut s, c"hi".as_ptr());
            assert_eq!(bytes(&s), b"hi\0lo\0");
            let mut t = new_text(b"");
            ee_ustring_copy(&mut t, &s, 3, 2, 0, true);
            assert_eq!((t.length, t.capacity, bytes(&t)), (2, 3, &b"lo\0"[..]));
            ee_ustring_free(&mut s);
            ee_ustring_free(&mut t);
        }
    }

    #[test]
    fn format_grows_and_keeps_the_arguments() {
        unsafe {
            let mut s = new_text(b"ab");
            let b = ee_ustring_format_buffer(&mut s, 6);
            assert_ne!(b, s.string);
            // the argument (the old text) is still readable
            assert_eq!(text(s.string), b"ab");
            ptr::copy_nonoverlapping(c"ab-ab".as_ptr(), b, 6);
            ee_ustring_format_end(&mut s, b, 6);
            assert_eq!((s.length, s.capacity), (5, 6));
            ee_ustring_free(&mut s);
        }
    }

    #[test]
    fn load_converts_old_text() {
        unsafe {
            let mut s = new_text(b"");
            let b = ee_ustring_load_buffer(&mut s, 3);
            ptr::copy_nonoverlapping(b"caf\xe9".as_ptr().cast(), b, 3);
            ee_ustring_load_end(&mut s, true, 3);
            assert_eq!(text(s.string), b"caf");
            let b = ee_ustring_load_buffer(&mut s, 4);
            ptr::copy_nonoverlapping(b"caf\xe9".as_ptr().cast(), b, 4);
            ee_ustring_load_end(&mut s, true, 4);
            assert_eq!((text(s.string), s.capacity), (&b"caf\xc3\xa9"[..], 6));
            ee_ustring_load_end(&mut s, false, 0);
            assert_eq!((s.length, *s.string), (0, 0));
            ee_ustring_free(&mut s);
        }
    }

    #[test]
    fn case_and_comparisons() {
        unsafe {
            let mut s = new_text("Ab\u{e9}".as_bytes());
            ee_ustring_change_case(&mut s, true);
            assert_eq!(text(s.string), "AB\u{c9}".as_bytes());
            assert!(ee_ustring_equals_ignore_case(s.string, c"ab\xc3\xa9".as_ptr(), false, 0));
            assert!(ee_ustring_equals_ignore_case(s.string, c"aBx".as_ptr(), true, 2));
            assert_eq!(ee_ustring_compare(c"a".as_ptr(), c"b".as_ptr()), -1);
            assert_eq!(ee_ustring_compare(c"\xe9".as_ptr(), c"a".as_ptr()), 1);
            assert_eq!(ee_ustring_compare_count(c"abc".as_ptr(), c"abd".as_ptr(), 2), 0);
            ee_ustring_free(&mut s);
        }
    }
}
