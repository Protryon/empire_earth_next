//! `UString` and `UWideString` (`wide.rs`) whole in Rust (LLE lane rounds 3-4). Round 3 moved its searches (`Find`, `FindBackwards`) and splits
//! (`Tokenize`, `NestedTokenize`); round 4 its storage (`record.rs`): the C++ class holds an [`EeUString`] by value
//! (the original's buffer, length and capacity, so every record embedding a `UString` keeps its layout and
//! `operator const char*` stays an inline read) and every member forwards here. C++ keeps what touches C++: the
//! `FSFile` virtuals of `Load`/`Save`, the token vectors and `sprintf`'s varargs formatting.
//!
//! A string is searched as its bytes up to its length (`mLength`); the comparisons are the original's `strncmp`, so
//! a NUL inside the length ends a comparison as it did. Arithmetic is the build's `unsigned long` (32 bits on
//! Windows) that wraps, and `FindBackwards`' 32-bit `long` index.
//!
//! Goldens: `golden_us.txt` (the searches, `tests/string/us_golden.cpp`, replayed by `golden.rs`) and
//! `golden_store.txt` (every member, `tests/string/us_store_golden.cpp`, replayed by `golden_store.rs`), both
//! traces of the original class; `golden_wide.txt` (`UWideString`, `tests/string/uws_golden.cpp`, replayed by
//! `golden_wide.rs`).

// `c_ulong` is the build's `unsigned long`: u32 on Windows, u64 on LP64, so its casts are needed on one of them
#![allow(clippy::unnecessary_cast)]

#[cfg(test)]
mod golden;
/// Serialises the golden replays (they set the process-wide legacy flag and default capacities).
#[cfg(test)]
static GOLDEN_LOCK: std::sync::Mutex<()> = std::sync::Mutex::new(());
#[cfg(test)]
mod golden_store;
#[cfg(test)]
mod golden_wide;
mod record;
mod wide;

pub use record::*;
pub use wide::*;
use std::ffi::{c_char, c_ulong};

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

/// A record's text up to its length.
///
/// # Safety
/// `s` is a live record.
unsafe fn record_text<'a>(s: *const EeUString) -> &'a [u8] {
    counted((*s).string, (*s).length)
}

/// # Safety
/// `p` points at `n` readable bytes (or `n` is 0).
unsafe fn counted<'a>(p: *const c_char, n: c_ulong) -> &'a [u8] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        std::slice::from_raw_parts(p.cast(), n as usize)
    }
}

/// `UString::Find` (`backwards`: `FindBackwards`) on `s` for `needle_length` bytes of `needle` (a `UString`'s
/// text and length, or a C string and its `strlen`).
///
/// # Safety
/// `s` is a live record, `needle` holds `needle_length` readable bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_find(
    s: *const EeUString,
    needle: *const c_char,
    needle_length: c_ulong,
    start: c_ulong,
    backwards: bool,
) -> c_ulong {
    let (text, needle) = (record_text(s), counted(needle, needle_length));
    let start = start as u32;
    c_ulong::from(if backwards { find_backwards(text, needle, start) } else { find(text, needle, start) })
}

/// Writes the token pairs (first, last) to `out` (room for `length + 1` pairs: 2 x that many u32) -> the count.
unsafe fn write_tokens(tokens: &[(u32, u32)], out: *mut u32) -> u32 {
    for (k, &(first, last)) in tokens.iter().enumerate() {
        *out.add(2 * k) = first;
        *out.add(2 * k + 1) = last;
    }
    tokens.len() as u32
}

/// `UString::Tokenize` (`brackets` NULL) or `NestedTokenize` (`brackets` the pair's string): the tokens' (first,
/// last) pairs -> their count (at most `length + 1`).
///
/// # Safety
/// `s` and `separators` are live records, `brackets` one or NULL; `out` has room for `2 * (length + 1)` values.
#[no_mangle]
pub unsafe extern "C" fn ee_ustring_tokenize(
    s: *const EeUString,
    separators: *const EeUString,
    brackets: *const EeUString,
    out: *mut u32,
) -> u32 {
    let (text, separators) = (record_text(s), record_text(separators));
    if brackets.is_null() {
        write_tokens(&tokenize(text, separators), out)
    } else {
        let pair = (*brackets).string.cast::<u8>();
        write_tokens(&nested_tokenize(text, separators, *pair, *pair.add(1)), out)
    }
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
