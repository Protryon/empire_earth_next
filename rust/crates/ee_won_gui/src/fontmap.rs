//! The window's font cache as Rust (R6 WONGUI B round 2): `Window::GetFont`'s `std::map<FontDescriptor,
//! SmartPtr<Font>>`, keyed as `FontDescriptor::operator<` orders (the size, then the style, then the face compared
//! without case, `StringCompareNoCase`). A descriptor met once keeps its entry; an entry whose platform font was
//! NULL is filled at the next lookup, as the original's `insert(..., NULL)` then assignment did.
//!
//! The fonts are C++ objects: the map keeps their pointers, the C++ holding one reference for each (`Window`'s
//! `GetFont` adds it when it stores one, `~Window` releases what `ee_won_fontmap_pop` hands back, in key order).

use std::cmp::Ordering;

use ee_won_net::strcase::compare_no_case;

struct Entry {
    face: Vec<u8>,
    style: i32,
    size: i32,
    font: usize,
}

/// `FontDescriptor::operator<` as an ordering (equal: neither is less).
fn order(face: &[u8], style: i32, size: i32, e: &Entry) -> Ordering {
    size.cmp(&e.size).then(style.cmp(&e.style)).then(compare_no_case(face, &e.face).cmp(&0))
}

/// The window's fonts.
#[derive(Default)]
pub struct EeWonFontMap {
    entries: Vec<Entry>,
}

impl EeWonFontMap {
    fn find(&self, face: &[u8], style: i32, size: i32) -> Result<usize, usize> {
        self.entries.binary_search_by(|e| order(face, style, size, e).reverse())
    }

    /// The font of the descriptor (0 when none is stored); the entry is made when it is new.
    pub fn lookup(&mut self, face: &[u8], style: i32, size: i32) -> usize {
        match self.find(face, style, size) {
            Ok(i) => self.entries[i].font,
            Err(i) => {
                self.entries.insert(i, Entry { face: face.to_vec(), style, size, font: 0 });
                0
            }
        }
    }

    /// Stores the font of the descriptor (made by a lookup); the previous one, which the C++ releases.
    pub fn store(&mut self, face: &[u8], style: i32, size: i32, font: usize) -> usize {
        let i = match self.find(face, style, size) {
            Ok(i) => i,
            Err(i) => {
                self.entries.insert(i, Entry { face: face.to_vec(), style, size, font: 0 });
                i
            }
        };
        std::mem::replace(&mut self.entries[i].font, font)
    }

    /// The first entry's font, removed (0 for a NULL font; -1 when the map is empty).
    pub fn pop(&mut self) -> isize {
        if self.entries.is_empty() {
            return -1;
        }
        self.entries.remove(0).font as isize
    }
}

/// A new, empty font map.
#[no_mangle]
pub extern "C" fn ee_won_fontmap_new() -> *mut EeWonFontMap {
    Box::into_raw(Box::default())
}

/// Frees the map (its fonts already popped and released by the C++).
///
/// # Safety
/// `m` is a map from `ee_won_fontmap_new` (or NULL), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fontmap_free(m: *mut EeWonFontMap) {
    if !m.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(m) });
    }
}

/// `GetFont`'s lookup of the descriptor (`face` its `n` bytes): its font, or 0 (the entry is made).
///
/// # Safety
/// `m` is a live map; `face` points at `n` bytes (or is NULL with `n` 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_fontmap_lookup(
    m: *mut EeWonFontMap,
    face: *const u8,
    n: usize,
    style: i32,
    size: i32,
) -> usize {
    // SAFETY: the caller's contract
    let (m, face) = unsafe { (&mut *m, bytes(face, n)) };
    m.lookup(face, style, size)
}

/// Stores the descriptor's font; returns the one it replaces (for the C++ to release).
///
/// # Safety
/// As `ee_won_fontmap_lookup`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fontmap_store(
    m: *mut EeWonFontMap,
    face: *const u8,
    n: usize,
    style: i32,
    size: i32,
    font: usize,
) -> usize {
    // SAFETY: the caller's contract
    let (m, face) = unsafe { (&mut *m, bytes(face, n)) };
    m.store(face, style, size, font)
}

/// Removes the first entry: its font (0 for NULL), or -1 when the map is empty.
///
/// # Safety
/// `m` is a live map.
#[no_mangle]
pub unsafe extern "C" fn ee_won_fontmap_pop(m: *mut EeWonFontMap) -> isize {
    // SAFETY: the caller's contract
    unsafe { &mut *m }.pop()
}

unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the callers' contract
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_descriptor_is_found_without_case_and_kept_once() {
        let mut m = EeWonFontMap::default();
        assert_eq!(m.lookup(b"Arial", 0, 12), 0);
        assert_eq!(m.store(b"Arial", 0, 12, 0x100), 0);
        assert_eq!(m.lookup(b"ARIAL", 0, 12), 0x100);
        assert_eq!(m.lookup(b"arial", 1, 12), 0);
        assert_eq!(m.lookup(b"arial", 0, 11), 0);
        assert_eq!(m.entries.len(), 3);
        // a NULL platform font leaves the entry to fill at the next lookup
        assert_eq!(m.store(b"arial", 1, 12, 0x200), 0);
        assert_eq!(m.lookup(b"Arial", 1, 12), 0x200);
        assert_eq!(m.entries.len(), 3);
    }

    #[test]
    fn the_entries_are_in_the_descriptors_order() {
        let mut m = EeWonFontMap::default();
        for (f, st, sz, v) in
            [(&b"b"[..], 0, 10, 1), (b"A", 0, 10, 2), (b"a", 1, 9, 3), (b"c", 0, 9, 4), (b"B", 2, 10, 5)]
        {
            m.lookup(f, st, sz);
            m.store(f, st, sz, v);
        }
        // size 9: style 0 "c", style 1 "a"; size 10: style 0 "A", "b", style 2 "B"
        let all: Vec<isize> = std::iter::from_fn(|| Some(m.pop()).filter(|&v| v >= 0)).collect();
        assert_eq!(all, vec![4, 3, 2, 1, 5]);
        assert_eq!(m.pop(), -1);
    }

    #[test]
    fn the_ffi_round_trip() {
        let m = ee_won_fontmap_new();
        // SAFETY: a live map and byte strings
        unsafe {
            assert_eq!(ee_won_fontmap_lookup(m, b"x".as_ptr(), 1, 0, 8), 0);
            assert_eq!(ee_won_fontmap_store(m, b"X".as_ptr(), 1, 0, 8, 7), 0);
            assert_eq!(ee_won_fontmap_lookup(m, std::ptr::null(), 0, 0, 8), 0);
            assert_eq!(ee_won_fontmap_pop(m), 0);
            assert_eq!(ee_won_fontmap_pop(m), 7);
            assert_eq!(ee_won_fontmap_pop(m), -1);
            ee_won_fontmap_free(m);
        }
    }
}
