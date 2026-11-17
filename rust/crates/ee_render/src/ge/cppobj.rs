//! The C++ objects of the Graphics Engine's classes, read in place (R4.3, seventeenth part).
//!
//! Every class whose state is a Rust object (`GEModel`, `GEMaterial`, `GEFont`, `GETexture`, `GEBitmap`,
//! `GEFontBitmap`, `GEText`, `GEWindow`, the primitives) keeps one member, that object. The reference-counted ones
//! derive from `UReferenceCounter`: a vtable pointer, the count, then the object; the others are the object alone.
//! A Rust object handed a C++ pointer of one of them reads its Rust object here instead of asking its host, and
//! the hosts lose the operations that only crossed back into Rust. The C++ asserts the layouts
//! (GEBitmap.cpp, "the layouts Rust reads in place").
//!
//! The `UString`s and `UWideString`s the objects own stay C++ objects (the accessors return references to them;
//! the host makes and deletes them): their state is `ee_ustring`'s record after the vtable pointer, so the reads,
//! assignments, comparisons and sizes are `ee_ustring`'s own functions, called here.
//!
//! No reference made here is held across a call into C++.

use std::ffi::{c_char, c_ulong, c_void};

use ee_base::ustring::{EeUString, EeUWideString};

/// A `UReferenceCounter`-derived C++ object whose only member is its Rust object.
#[repr(C)]
pub struct CppCounted {
    /// The C++ vtable (never called from Rust).
    pub vtable: *const c_void,
    /// `mReferenceCount`.
    pub count: c_ulong,
    /// The Rust object (`mRust`).
    pub rust: *mut c_void,
}

/// The Rust object of a reference-counted C++ object (`GEModel*`, `GEMaterial*`, `GEFont*`, `GETexture*`,
/// `GEBitmap*`, `GEFontBitmap*`); NULL for NULL.
///
/// # Safety
/// `cpp` is NULL or a live object of one of those classes.
pub unsafe fn counted<T>(cpp: usize) -> *mut T {
    if cpp == 0 {
        return std::ptr::null_mut();
    }
    // SAFETY: the caller's contract (the layout the C++ asserts)
    unsafe { (*(cpp as *const CppCounted)).rust as *mut T }
}

/// `obj->Reference()` of a reference-counted C++ object (`mReferenceCount++`, the header's inline member).
///
/// # Safety
/// `cpp` is a live reference-counted object.
pub unsafe fn reference(cpp: usize) {
    // SAFETY: the caller's contract
    unsafe {
        let count = &mut (*(cpp as *mut CppCounted)).count;
        *count = count.wrapping_add(1);
    }
}

/// `obj->mReferenceCount`.
///
/// # Safety
/// `cpp` is a live reference-counted object.
pub unsafe fn reference_count(cpp: usize) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { (*(cpp as *const CppCounted)).count }
}

/// The Rust object of a C++ object that is its Rust object alone (`GEText*`, `GEWindow*`, a primitive).
///
/// # Safety
/// `cpp` is NULL or a live object of one of those classes.
pub unsafe fn plain<T>(cpp: usize) -> *mut T {
    if cpp == 0 {
        return std::ptr::null_mut();
    }
    // SAFETY: the caller's contract
    unsafe { *(cpp as *const *mut T) }
}

/// A C++ `GEVertexArray` (or `GEPolygonArray`): its vtable pointer, the GPU's vertices (indices), its Rust array
/// (`vertarr.rs`; NULL for the store gate's subclass).
#[repr(C)]
pub struct CppArray {
    pub vtable: *const c_void,
    pub data: *mut c_void,
    pub rust: *mut c_void,
}

/// A `GEVertexArray*`'s vertices (`GetVertices()`).
///
/// # Safety
/// `array` is a live vertex array.
pub unsafe fn vertices(array: usize) -> *mut c_void {
    // SAFETY: the caller's contract
    unsafe { (*(array as *const CppArray)).data }
}

/// A `GEPolygonArray*`'s indices (`GetIndices()`).
///
/// # Safety
/// `array` is a live polygon array.
pub unsafe fn indices(array: usize) -> *const u16 {
    // SAFETY: the caller's contract
    unsafe { (*(array as *const CppArray)).data as *const u16 }
}

/// A `GEVertexArray*`'s `GetType()` (of its Rust array).
///
/// # Safety
/// `array` is a live vertex array over a Rust array.
pub unsafe fn vertex_type(array: usize) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe {
        crate::ge::vertarr::ee_ge_va_type(
            (*(array as *const CppArray)).rust as *const crate::ge::vertarr::EeGeVertexArray,
        )
    }
}

/// A C++ `UString`: its vtable pointer, then `ee_ustring`'s record.
#[repr(C)]
pub struct CppUString {
    pub vtable: *const c_void,
    pub state: EeUString,
}

/// A C++ `UWideString`: its vtable pointer, then `ee_ustring`'s record.
#[repr(C)]
pub struct CppUWideString {
    pub vtable: *const c_void,
    pub state: EeUWideString,
}

/// `sizeof(UString)` (an element of a `new UString[n]` array).
pub const USTRING_SIZE: usize = std::mem::size_of::<CppUString>();

/// The record of a C++ `UString`.
///
/// # Safety
/// `s` is a live `UString`.
pub unsafe fn ustr<'a>(s: usize) -> &'a mut EeUString {
    // SAFETY: the caller's contract
    unsafe { &mut (*(s as *mut CppUString)).state }
}

/// The record of a C++ `UWideString`.
///
/// # Safety
/// `s` is a live `UWideString`.
pub unsafe fn wstr<'a>(s: usize) -> &'a mut EeUWideString {
    // SAFETY: the caller's contract
    unsafe { &mut (*(s as *mut CppUWideString)).state }
}

/// `*target = *source` (`UString::operator=`).
///
/// # Safety
/// Both are live `UString`s.
pub unsafe fn str_assign(target: usize, source: usize) {
    // SAFETY: the caller's contract
    unsafe { ee_base::ustring::ee_ustring_assign(ustr(target), ustr(source)) }
}

/// `*target = UString(bytes)` (the bytes hold no NUL): a temporary string of the bytes assigned, then freed, as
/// the C++ statement did (the target's capacity follows `operator=`'s rule).
///
/// # Safety
/// `target` is a live `UString`.
pub unsafe fn str_set_bytes(target: usize, bytes: &[u8]) {
    let mut text = bytes.to_vec();
    text.push(0);
    let mut temp = EeUString { string: std::ptr::null_mut(), length: 0, capacity: 0 };
    // SAFETY: the caller's contract; `text` is NUL-terminated; `temp` is a fresh record, freed once
    unsafe {
        ee_base::ustring::ee_ustring_init_text(&mut temp, text.as_ptr() as *const c_char);
        ee_base::ustring::ee_ustring_assign(ustr(target), &temp);
        ee_base::ustring::ee_ustring_free(&mut temp);
    }
}

/// `(const char*)*s`.
///
/// # Safety
/// `s` is a live `UString`.
pub unsafe fn str_text(s: usize) -> *const c_char {
    // SAFETY: the caller's contract
    unsafe { ustr(s).string }
}

/// `s->GetLength()`.
///
/// # Safety
/// `s` is a live `UString`.
pub unsafe fn str_length(s: usize) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { ustr(s).length }
}

/// `s->SizeOf()`.
///
/// # Safety
/// `s` is a live `UString`.
pub unsafe fn str_size_of(s: usize) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { ee_base::ustring::ee_ustring_size_of(ustr(s)) }
}

/// `*a == *b` (`UString::operator==`: `strcmp` of the texts is 0).
///
/// # Safety
/// Both are live `UString`s.
pub unsafe fn str_equal(a: usize, b: usize) -> bool {
    // SAFETY: the caller's contract
    unsafe { ee_base::ustring::ee_ustring_compare(str_text(a), str_text(b)) == 0 }
}

/// `&array[i]` of a `new UString[n]` array.
pub fn str_at(array: usize, i: usize) -> usize {
    array + i * USTRING_SIZE
}

/// `*target = *source` (`UWideString::operator=`).
///
/// # Safety
/// Both are live `UWideString`s.
pub unsafe fn wstr_assign(target: usize, source: usize) {
    // SAFETY: the caller's contract
    unsafe { ee_base::ustring::ee_uwstring_assign(wstr(target), wstr(source)) }
}

/// `(const char*)*s` of a `UWideString`.
///
/// # Safety
/// `s` is a live `UWideString`.
pub unsafe fn wstr_text(s: usize) -> *const c_char {
    // SAFETY: the caller's contract
    unsafe { wstr(s).string }
}

/// `s->GetLength()` of a `UWideString` (characters).
///
/// # Safety
/// `s` is a live `UWideString`.
pub unsafe fn wstr_length(s: usize) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { wstr(s).length }
}

/// `s->GetTCHARLength()` of a `UWideString` (bytes).
///
/// # Safety
/// `s` is a live `UWideString`.
pub unsafe fn wstr_bytes(s: usize) -> c_ulong {
    // SAFETY: the caller's contract
    unsafe { wstr(s).byte_length }
}

/// A C++-layout `UString` made and freed by Rust (the unit tests' hosts: a NULL vtable Rust never calls).
pub mod testing {
    use super::*;

    /// `new UString` as the tests' hosts make it.
    pub fn new_ustring(text: &str) -> usize {
        let s = Box::into_raw(Box::new(CppUString {
            vtable: std::ptr::null(),
            state: EeUString { string: std::ptr::null_mut(), length: 0, capacity: 0 },
        }));
        let mut bytes = text.as_bytes().to_vec();
        bytes.push(0);
        // SAFETY: a fresh record; NUL-terminated text
        unsafe { ee_base::ustring::ee_ustring_init_text(&mut (*s).state, bytes.as_ptr() as *const c_char) };
        s as usize
    }

    /// `new UString` (the default capacity), as the tests' hosts make it.
    pub fn new_ustring_default() -> usize {
        let s = Box::into_raw(Box::new(CppUString {
            vtable: std::ptr::null(),
            state: EeUString { string: std::ptr::null_mut(), length: 0, capacity: 0 },
        }));
        // SAFETY: a fresh record
        unsafe { ee_base::ustring::ee_ustring_init(&mut (*s).state) };
        s as usize
    }

    /// `delete` of a string from [`new_ustring`] or [`new_ustring_default`].
    ///
    /// # Safety
    /// `s` is from [`new_ustring`] and not used again.
    pub unsafe fn delete_ustring(s: usize) {
        // SAFETY: the caller's contract
        unsafe {
            let mut b = Box::from_raw(s as *mut CppUString);
            ee_base::ustring::ee_ustring_free(&mut b.state);
        }
    }

    /// `new UString[n]` (empty strings), as one allocation of `n` C++-layout strings.
    pub fn new_ustring_array(n: usize) -> usize {
        // one string at least: a `new UString[0]` is a distinct address too
        let mut v: Vec<CppUString> = Vec::with_capacity(n.max(1));
        for _ in 0..n.max(1) {
            let mut s = CppUString {
                vtable: std::ptr::null(),
                state: EeUString { string: std::ptr::null_mut(), length: 0, capacity: 0 },
            };
            // SAFETY: a fresh record
            unsafe { ee_base::ustring::ee_ustring_init(&mut s.state) };
            v.push(s);
        }
        let b = v.into_boxed_slice();
        let len = b.len();
        let p = Box::into_raw(b) as *mut CppUString as usize;
        ARRAYS.with(|a| a.borrow_mut().push((p, len)));
        p
    }

    /// `delete[]` of an array from [`new_ustring_array`].
    ///
    /// # Safety
    /// `p` is from [`new_ustring_array`] and not used again.
    pub unsafe fn delete_ustring_array(p: usize) {
        let len = ARRAYS.with(|a| {
            let mut a = a.borrow_mut();
            let at = a.iter().position(|e| e.0 == p).expect("a string array of new_ustring_array");
            a.remove(at).1
        });
        // SAFETY: the caller's contract
        unsafe {
            let mut b = Box::from_raw(std::ptr::slice_from_raw_parts_mut(p as *mut CppUString, len));
            for s in b.iter_mut() {
                ee_base::ustring::ee_ustring_free(&mut s.state);
            }
        }
    }

    /// The text of a C++-layout string.
    ///
    /// # Safety
    /// `s` is a live string.
    pub unsafe fn text(s: usize) -> String {
        // SAFETY: the caller's contract
        unsafe {
            let r = ustr(s);
            String::from_utf8_lossy(std::slice::from_raw_parts(r.string as *const u8, r.length as usize)).into_owned()
        }
    }

    /// `new UWideString(capacity)` as the tests' hosts make it.
    pub fn new_uwstring(characters: c_ulong) -> usize {
        let s = Box::into_raw(Box::new(CppUWideString {
            vtable: std::ptr::null(),
            state: EeUWideString { string: std::ptr::null_mut(), byte_length: 0, length: 0, capacity: 0 },
        }));
        // SAFETY: a fresh record
        unsafe { ee_base::ustring::ee_uwstring_init_characters(&mut (*s).state, characters) };
        s as usize
    }

    /// A `UWideString` holding `text`.
    pub fn new_uwstring_text(text: &str) -> usize {
        let s = new_uwstring(0x100);
        let mut bytes = text.as_bytes().to_vec();
        bytes.push(0);
        // SAFETY: a live record; NUL-terminated text
        unsafe { ee_base::ustring::ee_uwstring_assign_text(wstr(s), bytes.as_ptr() as *const c_char) };
        s
    }

    /// `delete` of a string from [`new_uwstring`].
    ///
    /// # Safety
    /// `s` is from [`new_uwstring`] and not used again.
    pub unsafe fn delete_uwstring(s: usize) {
        // SAFETY: the caller's contract
        unsafe {
            let mut b = Box::from_raw(s as *mut CppUWideString);
            ee_base::ustring::ee_uwstring_free(&mut b.state);
        }
    }

    /// The text of a C++-layout wide string.
    ///
    /// # Safety
    /// `s` is a live string.
    pub unsafe fn wtext(s: usize) -> String {
        // SAFETY: the caller's contract
        unsafe {
            let r = wstr(s);
            String::from_utf8_lossy(std::slice::from_raw_parts(r.string as *const u8, r.byte_length as usize))
                .into_owned()
        }
    }

    thread_local! {
        static ARRAYS: std::cell::RefCell<Vec<(usize, usize)>> = const { std::cell::RefCell::new(Vec::new()) };
    }

    /// A C++-layout reference-counted object around a Rust object (count 1).
    pub fn new_counted(rust: *mut c_void) -> usize {
        Box::into_raw(Box::new(CppCounted { vtable: std::ptr::null(), count: 1, rust })) as usize
    }

    /// Frees an object from [`new_counted`] (not its Rust object).
    ///
    /// # Safety
    /// `cpp` is from [`new_counted`] and not used again.
    pub unsafe fn delete_counted(cpp: usize) {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(cpp as *mut CppCounted) });
    }

    /// A C++-layout plain object (its Rust object alone).
    pub fn new_plain(rust: *mut c_void) -> usize {
        Box::into_raw(Box::new(rust)) as usize
    }

    /// Frees an object from [`new_plain`].
    ///
    /// # Safety
    /// `cpp` is from [`new_plain`] and not used again.
    pub unsafe fn delete_plain(cpp: usize) {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(cpp as *mut *mut c_void) });
    }
}

#[cfg(test)]
mod tests {
    use super::testing::*;
    use super::*;

    #[test]
    fn strings_read_and_write_in_place() {
        let a = new_ustring("alpha");
        let b = new_ustring("beta");
        unsafe {
            assert_eq!(str_length(a), 5);
            assert!(!str_equal(a, b));
            str_assign(a, b);
            assert!(str_equal(a, b));
            assert_eq!(text(a), "beta");
            str_set_bytes(a, b"gamma ray");
            assert_eq!(text(a), "gamma ray");
            assert_eq!(str_length(a), 9);
            assert_eq!(*str_text(a) as u8, b'g');
            assert_eq!(str_size_of(a), ee_base::ustring::ee_ustring_size_of(ustr(a)));
            delete_ustring(a);
            delete_ustring(b);
        }
        let arr = new_ustring_array(3);
        unsafe {
            let s = new_ustring("x");
            str_assign(str_at(arr, 2), s);
            assert_eq!(text(str_at(arr, 2)), "x");
            assert_eq!(text(str_at(arr, 1)), "");
            delete_ustring(s);
            delete_ustring_array(arr);
        }
        let w = new_uwstring_text("h\u{e9}llo");
        let v = new_uwstring(16);
        unsafe {
            assert_eq!(wstr_length(w), 5);
            assert_eq!(wstr_bytes(w), 6);
            wstr_assign(v, w);
            assert_eq!(wtext(v), "h\u{e9}llo");
            assert_eq!(*wstr_text(v) as u8, b'h');
            delete_uwstring(w);
            delete_uwstring(v);
        }
    }

    #[test]
    fn counted_objects_hold_their_rust_object_after_the_count() {
        let mut target = 7u32;
        let cpp = new_counted(&mut target as *mut u32 as *mut c_void);
        unsafe {
            assert_eq!(*counted::<u32>(cpp), 7);
            assert_eq!(reference_count(cpp), 1);
            reference(cpp);
            assert_eq!(reference_count(cpp), 2);
            assert!(counted::<u32>(0).is_null());
            delete_counted(cpp);
        }
        let p = new_plain(&mut target as *mut u32 as *mut c_void);
        unsafe {
            assert_eq!(*plain::<u32>(p), 7);
            assert!(plain::<u32>(0).is_null());
            delete_plain(p);
        }
        assert_eq!(std::mem::size_of::<CppCounted>(), 3 * std::mem::size_of::<usize>());
    }
}
