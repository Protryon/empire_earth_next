//! The string classes' algorithms (LLE lane round 3): `UString`'s searches (`Find`, `FindBackwards`) and splits
//! (`Tokenize`, `NestedTokenize`). The classes, their buffers and their substrings stay C++ (`UString` is held by
//! value in hundreds of records and read inline through `operator const char*`): each method hands its bytes here
//! and builds its result from the indices that come back.
//!
//! A string is passed as its bytes up to its length (`mLength`); the comparisons are the original's `strncmp`, so a
//! NUL inside the length ends a comparison as it did. Arithmetic is the Windows build's: 32-bit `unsigned long`
//! that wraps, and `FindBackwards`' 32-bit `long` index.
//!
//! `golden_us.txt` is the trace of the original class (`tests/string/us_golden.cpp`); `golden.rs` replays it.

#[cfg(test)]
mod golden;

/// `strncmp(a, b, n) == 0` over strings whose bytes past their slices read as the terminator.
fn strneq(a: &[u8], b: &[u8], n: u32) -> bool {
    for k in 0..n as usize {
        let (x, y) = (a.get(k).copied().unwrap_or(0), b.get(k).copied().unwrap_or(0));
        if x != y {
            return false;
        }
        if x == 0 {
            return true;
        }
    }
    true
}

/// `UString::Find`: the first position at or after `start` where `needle` occurs, `text.len()` when none.
pub fn find(text: &[u8], needle: &[u8], start: u32) -> u32 {
    let length = text.len() as u32;
    let needle_length = needle.len() as u32;
    if needle_length <= length.wrapping_sub(start) {
        let mut i = start;
        while i <= length.wrapping_sub(needle_length) {
            // past the terminator the original read outside the string (a start past the end with a needle longer
            // than the string): nothing there is a match
            if i > length {
                break;
            }
            if strneq(&text[i as usize..], needle, needle_length) {
                return i;
            }
            i = i.wrapping_add(1);
        }
    }
    length
}

/// `UString::FindBackwards`: the last position at or before `start` (clamped to the length) where `needle`
/// starts, `text.len()` when none.
pub fn find_backwards(text: &[u8], needle: &[u8], start: u32) -> u32 {
    let length = text.len() as u32;
    let start = start.min(length);
    let mut i = start.wrapping_sub(needle.len() as u32) as i32;
    while i >= 0 {
        if strneq(&text[i as usize..], needle, needle.len() as u32) {
            return i as u32;
        }
        i -= 1;
    }
    length
}

/// Whether the byte at `i` (the terminator at the end) is one of `separators`, as the original asked:
/// `separators.Find(<the byte as a C string>, 0) != separators.GetLength()`.
fn is_separator(separators: &[u8], byte: u8) -> bool {
    let needle: &[u8] = if byte == 0 { &[] } else { std::slice::from_ref(&byte) };
    find(separators, needle, 0) != separators.len() as u32
}

/// `UString::Tokenize`: the tokens between separators as (first, last) index pairs, empty tokens skipped.
pub fn tokenize(text: &[u8], separators: &[u8]) -> Vec<(u32, u32)> {
    let length = text.len() as u32;
    let mut tokens = Vec::new();
    let mut start = 0u32;
    let mut between = true;
    for i in 0..=length {
        let byte = text.get(i as usize).copied().unwrap_or(0);
        if between {
            if !is_separator(separators, byte) {
                between = false;
                start = i;
            }
        } else if is_separator(separators, byte) || i == length {
            tokens.push((start, i.wrapping_sub(1)));
            between = true;
        }
    }
    tokens
}

/// `UString::NestedTokenize`: as [`tokenize`], but separators inside brackets (`open`, `close`; the same byte for
/// quotes) do not split, and a token wholly in brackets loses them.
pub fn nested_tokenize(text: &[u8], separators: &[u8], open: u8, close: u8) -> Vec<(u32, u32)> {
    let length = text.len() as u32;
    let mut tokens = Vec::new();
    let (mut quoted, mut depth, mut start, mut between) = (false, 0u32, 0u32, true);
    let at = |i: u32| text.get(i as usize).copied().unwrap_or(0);
    for i in 0..=length {
        let byte = at(i);
        if between {
            if !is_separator(separators, byte) {
                between = false;
                start = i;
                quoted = byte == open;
                if quoted {
                    depth = 1;
                }
            }
        } else {
            if byte == open && (close != open || depth == 0) {
                depth = depth.wrapping_add(1);
            } else if byte == close && depth > 0 {
                depth -= 1;
            }
            if depth == 0 && (is_separator(separators, byte) || i == length) {
                let mut end = i.wrapping_sub(1);
                if quoted && at(end) == close {
                    start = start.wrapping_add(1);
                    end = end.wrapping_sub(1);
                }
                if start <= end {
                    tokens.push((start, end));
                }
                quoted = false;
                between = true;
            }
        }
    }
    tokens
}

// ------------------------------------------------------------------------------------------- the C interface

/// # Safety
/// `p` points at `n` readable bytes (or `n` is 0).
unsafe fn bytes<'a>(p: *const u8, n: u32) -> &'a [u8] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        std::slice::from_raw_parts(p, n as usize)
    }
}

/// `UString::Find` on `text` (`length` bytes) for `needle` (`needle_length` bytes).
///
/// # Safety
/// Both point at their lengths' readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_find(
    text: *const u8,
    length: u32,
    needle: *const u8,
    needle_length: u32,
    start: u32,
) -> u32 {
    find(bytes(text, length), bytes(needle, needle_length), start)
}

/// `UString::FindBackwards`.
///
/// # Safety
/// As [`ee_ustring_find`].
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_find_backwards(
    text: *const u8,
    length: u32,
    needle: *const u8,
    needle_length: u32,
    start: u32,
) -> u32 {
    find_backwards(bytes(text, length), bytes(needle, needle_length), start)
}

/// Writes the token pairs (first, last) to `out` (room for `length + 1` pairs: 2 x that many u32) -> the count.
unsafe fn write_tokens(tokens: &[(u32, u32)], out: *mut u32) -> u32 {
    for (k, &(first, last)) in tokens.iter().enumerate() {
        *out.add(2 * k) = first;
        *out.add(2 * k + 1) = last;
    }
    tokens.len() as u32
}

/// `UString::Tokenize`: the tokens' (first, last) pairs -> their count (at most `length + 1`).
///
/// # Safety
/// The strings point at their lengths' readable bytes; `out` has room for `2 * (length + 1)` values.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_tokenize(
    text: *const u8,
    length: u32,
    separators: *const u8,
    separators_length: u32,
    out: *mut u32,
) -> u32 {
    write_tokens(&tokenize(bytes(text, length), bytes(separators, separators_length)), out)
}

/// `UString::NestedTokenize` with the bracket pair's two bytes -> the count of pairs written.
///
/// # Safety
/// As [`ee_ustring_tokenize`].
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_nested_tokenize(
    text: *const u8,
    length: u32,
    separators: *const u8,
    separators_length: u32,
    open: u8,
    close: u8,
    out: *mut u32,
) -> u32 {
    write_tokens(&nested_tokenize(bytes(text, length), bytes(separators, separators_length), open, close), out)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn find_and_find_backwards() {
        assert_eq!(find(b"abcabc", b"bc", 0), 1);
        assert_eq!(find(b"abcabc", b"bc", 2), 4);
        assert_eq!(find(b"abcabc", b"x", 0), 6);
        assert_eq!(find(b"abc", b"", 2), 2);
        assert_eq!(find(b"abc", b"a", 9), 3);
        assert_eq!(find_backwards(b"abcabc", b"bc", 6), 4);
        assert_eq!(find_backwards(b"abcabc", b"bc", 3), 1);
        assert_eq!(find_backwards(b"ab", b"abc", 9), 2);
    }

    #[test]
    fn tokenizers() {
        assert_eq!(tokenize(b" a,bc ", b" ,"), [(1, 1), (3, 4)]);
        assert_eq!(tokenize(b"", b" "), []);
        assert_eq!(nested_tokenize(b"a (b c) \"d e\"", b" ", b'(', b')'), [(0, 0), (3, 5), (8, 9), (11, 12)]);
        assert_eq!(nested_tokenize(b"\"d e\" f", b" ", b'"', b'"'), [(1, 3), (6, 6)]);
    }
}
