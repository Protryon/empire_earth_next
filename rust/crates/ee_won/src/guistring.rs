//! WONGUI's GUIStringData (every lobby text): the comparisons (exact, ignoring case, against narrow text), the
//! searches (a character, a substring, either case, any of a set, from the end), the in-place case maps, the
//! decimal read and the crossing to and from the game's narrow text. The C++ class keeps the buffer, its length and
//! capacity; these are its algorithms over the units (`wchar_t` values, any width). A read at the length is the
//! terminator (0), as the original read it; the character classes and case maps are the C runtime's
//! (`chat::ChatCtype`).
//!
//! Narrow text is the game's text, UTF-8 (decision D6): the narrow constructors decode it, `GetString` encodes, and
//! the comparisons with narrow text decode it first (`widen`, `narrow`; `text` is the one conversion). The original
//! widened each byte and narrowed a unit below 256 to its byte (Latin-1) and anything else to '?'.

use std::ffi::c_void;

use crate::chat::{units, ChatCtype, CrtChat, EeWonChatText};
use crate::ffi::{EeWonByteSink, EeWonWideSink};

fn at(t: &[u32], i: i64) -> u32 {
    if i < 0 {
        0
    } else {
        t.get(i as usize).copied().unwrap_or(0)
    }
}

/// `Compare`: the first differing units' difference, else -1/0/1 by length.
pub fn compare(a: &[u32], b: &[u32]) -> i32 {
    for (&x, &y) in a.iter().zip(b) {
        if x != y {
            return (x as i32).wrapping_sub(y as i32);
        }
    }
    a.len().cmp(&b.len()) as i32
}

/// `CompareNoCase`: as `compare` through `towupper`.
pub fn compare_no_case(a: &[u32], b: &[u32], ct: &impl ChatCtype) -> i32 {
    for (&x, &y) in a.iter().zip(b) {
        let (x, y) = (ct.towupper(x) as i32, ct.towupper(y) as i32);
        if x != y {
            return x.wrapping_sub(y);
        }
    }
    a.len().cmp(&b.len()) as i32
}

fn unit(n: &[u32], i: usize) -> u32 {
    n.get(i).copied().unwrap_or(0)
}

/// `wchar_t` units of the game's narrow text (UTF-8; a stray byte is U+FFFD): UTF-16 units for a `wchar_size` of
/// 2, code points otherwise.
pub fn widen(narrow: &[u8], wchar_size: u32) -> Vec<u32> {
    if wchar_size == 2 {
        crate::text::utf16_from_game(narrow).into_iter().map(u32::from).collect()
    } else {
        crate::text::utf32_from_game(narrow)
    }
}

/// `GetString`: the units as the game's narrow text (UTF-8; a lone surrogate is U+FFFD).
pub fn narrow(a: &[u32], wchar_size: u32) -> Vec<u8> {
    if wchar_size == 2 {
        crate::text::game_from_utf16(&a.iter().map(|&c| c as u16).collect::<Vec<_>>())
    } else {
        crate::text::game_from_utf32(a)
    }
}

/// `CompareNoCase(const char*, offset)`: the text from `offset` against narrow text (to its NUL) as `widen` gives
/// it: 1 when the narrow text ends first, the first difference, else 0 when both end, -1.
pub fn compare_no_case_narrow(a: &[u32], offset: i32, narrow: &[u32], ct: &impl ChatCtype) -> i32 {
    let count = a.len() as i64 - i64::from(offset);
    let mut i = 0usize;
    while (i as i64) < count {
        let ca = ct.towupper(at(a, i64::from(offset) + i as i64)) as i32;
        let cb = ct.towupper(unit(narrow, i)) as i32;
        if cb == 0 {
            return 1;
        }
        if ca != cb {
            return ca.wrapping_sub(cb);
        }
        i += 1;
    }
    if unit(narrow, i) == 0 {
        0
    } else {
        -1
    }
}

/// `compare(const char*, offset)`: as `compare_no_case_narrow` without the case map, except that the end answers 0
/// only when the narrow text continues with '0' (the original's bug), else -1.
pub fn compare_narrow(a: &[u32], offset: i32, narrow: &[u32]) -> i32 {
    let count = a.len() as i64 - i64::from(offset);
    let mut i = 0usize;
    while (i as i64) < count {
        let c1 = at(a, i64::from(offset) + i as i64) as i32;
        let c2 = unit(narrow, i) as i32;
        if c2 == 0 {
            return 1;
        }
        if c1 != c2 {
            return c1.wrapping_sub(c2);
        }
        i += 1;
    }
    if unit(narrow, i) == u32::from(b'0') {
        0
    } else {
        -1
    }
}

/// `ToInt`: from a 16-bit offset (it wraps), spaces skipped (the CRT's `isspace`), an optional '-', the digits
/// in a wrapping int.
pub fn to_int(a: &[u32], offset: u16, ct: &impl ChatCtype) -> i32 {
    let mut o = offset;
    while ct.isspace(at(a, i64::from(o)) as i32) {
        o = o.wrapping_add(1);
    }
    let negative = at(a, i64::from(o)) == u32::from(b'-');
    if negative {
        o = o.wrapping_add(1);
    }
    let mut value: i32 = 0;
    loop {
        let digit = (at(a, i64::from(o)) as i32).wrapping_sub(0x30);
        o = o.wrapping_add(1);
        if !(0..=9).contains(&digit) {
            break;
        }
        value = value.wrapping_mul(10).wrapping_add(digit);
    }
    if negative {
        value.wrapping_neg()
    } else {
        value
    }
}

/// `find(int, start)`: the first `ch` at or after `start`, or -1.
pub fn find_char(a: &[u32], ch: i32, start: i32) -> i32 {
    let mut i = i64::from(start);
    while i < a.len() as i64 {
        if at(a, i) as i32 == ch {
            return i as i32;
        }
        i += 1;
    }
    -1
}

fn find_with(a: &[u32], needle: &[u32], start: i32, same: impl Fn(u32, u32) -> bool) -> i32 {
    let length = a.len() as i64;
    let mut candidate = i64::from(start);
    while candidate < length {
        let (mut pos, mut matched) = (candidate, 0usize);
        while pos < length && matched < needle.len() && same(at(a, pos), needle[matched]) {
            pos += 1;
            matched += 1;
        }
        if matched == needle.len() {
            return (pos - needle.len() as i64) as i32;
        }
        candidate += 1;
    }
    -1
}

/// `find(const GUIStringData*, start)`: the first match at or after `start`, or -1 (an empty needle matches at
/// `start` while it is below the length).
pub fn find(a: &[u32], needle: &[u32], start: i32) -> i32 {
    find_with(a, needle, start, |x, y| x == y)
}

/// `findNoCase`: as `find` through `towupper`.
pub fn find_no_case(a: &[u32], needle: &[u32], start: i32, ct: &impl ChatCtype) -> i32 {
    find_with(a, needle, start, |x, y| ct.towupper(x) == ct.towupper(y))
}

/// `rFind(int, start)`: the last `ch` at or before `start` (-1: from the end), or -1.
pub fn rfind_char(a: &[u32], ch: i32, start: i32) -> i32 {
    let mut i = if start == -1 { a.len() as i64 - 1 } else { i64::from(start) };
    while i >= 0 {
        if at(a, i) as i32 == ch {
            return i as i32;
        }
        i -= 1;
    }
    -1
}

/// `findOneOf`: the first position of any of `set`'s units at or after `start`, or -1.
pub fn find_one_of(a: &[u32], set: &[u32], start: i32) -> i32 {
    set.iter().map(|&c| find_char(a, c as i32, start)).filter(|&f| f != -1).min().unwrap_or(-1)
}

/// `rFind(const GUIStringData*, start)`: `std::wstring::rfind` (a negative start is `npos`), -1 for none.
pub fn rfind(a: &[u32], needle: &[u32], start: i32) -> i32 {
    if needle.len() > a.len() {
        return -1;
    }
    let last = a.len() - needle.len();
    let from = if start < 0 { last } else { (start as usize).min(last) };
    (0..=from).rev().find(|&i| a[i..i + needle.len()] == *needle).map_or(-1, |i| i as i32)
}

/// `toUpper` / `toLowerCase`: units below 256 through the CRT's `toupper` / `tolower`.
pub fn change_case(a: &mut [u32], upper: bool, ct: &impl ChatCtype) {
    for c in a.iter_mut().filter(|c| **c < 256) {
        *c = if upper { ct.toupper(*c) } else { ct.tolower(*c) };
    }
}

// ---- the C ABI (GUIStringData.cpp, GUIStringDataSearch.cpp)

unsafe fn text(t: *const c_void, length: u16, wchar_size: u32) -> Vec<u32> {
    unsafe { units(EeWonChatText { text: t, length: usize::from(length) }, wchar_size) }
}

unsafe fn narrow_text<'a>(s: *const u8) -> &'a [u8] {
    if s.is_null() {
        &[]
    } else {
        // SAFETY: a NUL-terminated string (the caller's contract)
        unsafe { std::ffi::CStr::from_ptr(s.cast()) }.to_bytes()
    }
}

/// The search or compare `ee_won_guistring_op` runs.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiStringOp {
    Compare,
    CompareNoCase,
    Find,
    FindNoCase,
    FindOneOf,
    RFind,
}

/// A comparison or search of `a` (`a_length` units) with `b`; `start` for the searches.
///
/// # Safety
/// `a` and `b` hold their lengths' units of `wchar_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_guistring_op(
    op: EeWonGuiStringOp,
    a: *const c_void,
    a_length: u16,
    b: *const c_void,
    b_length: u16,
    start: i32,
    wchar_size: u32,
) -> i32 {
    let (x, y) = unsafe { (text(a, a_length, wchar_size), text(b, b_length, wchar_size)) };
    match op {
        EeWonGuiStringOp::Compare => compare(&x, &y),
        EeWonGuiStringOp::CompareNoCase => compare_no_case(&x, &y, &CrtChat),
        EeWonGuiStringOp::Find => find(&x, &y, start),
        EeWonGuiStringOp::FindNoCase => find_no_case(&x, &y, start, &CrtChat),
        EeWonGuiStringOp::FindOneOf => find_one_of(&x, &y, start),
        EeWonGuiStringOp::RFind => rfind(&x, &y, start),
    }
}

/// `find(int, start)` (`reverse` false) or `rFind(int, start)`.
///
/// # Safety
/// `a` holds `a_length` units of `wchar_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_guistring_find_char(
    a: *const c_void,
    a_length: u16,
    ch: i32,
    start: i32,
    reverse: bool,
    wchar_size: u32,
) -> i32 {
    let x = unsafe { text(a, a_length, wchar_size) };
    if reverse {
        rfind_char(&x, ch, start)
    } else {
        find_char(&x, ch, start)
    }
}

/// `compare(const char*, offset)` (`no_case` false) or `CompareNoCase(const char*, offset)`.
///
/// # Safety
/// `a` as `ee_won_guistring_find_char`; `narrow` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_won_guistring_compare_narrow(
    a: *const c_void,
    a_length: u16,
    offset: i32,
    narrow: *const u8,
    no_case: bool,
    wchar_size: u32,
) -> i32 {
    let (x, n) = unsafe { (text(a, a_length, wchar_size), widen(narrow_text(narrow), wchar_size)) };
    if no_case {
        compare_no_case_narrow(&x, offset, &n, &CrtChat)
    } else {
        compare_narrow(&x, offset, &n)
    }
}

/// `ToInt`.
///
/// # Safety
/// `a` as `ee_won_guistring_find_char`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_guistring_to_int(a: *const c_void, a_length: u16, offset: u16, wchar_size: u32) -> i32 {
    to_int(&unsafe { text(a, a_length, wchar_size) }, offset, &CrtChat)
}

/// `toUpper` / `toLowerCase` in place.
///
/// # Safety
/// `a` holds `a_length` writable units of `wchar_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_guistring_change_case(a: *mut c_void, a_length: u16, upper: bool, wchar_size: u32) {
    let mut x = unsafe { text(a, a_length, wchar_size) };
    change_case(&mut x, upper, &CrtChat);
    for (i, &c) in x.iter().enumerate() {
        // SAFETY: `a_length` writable units
        unsafe {
            if wchar_size == 2 {
                *(a as *mut u16).add(i) = c as u16;
            } else {
                *(a as *mut u32).add(i) = c;
            }
        }
    }
}

/// `GetString`: the text as UTF-8, given to the sink once.
///
/// # Safety
/// `a` as `ee_won_guistring_find_char`; the sink's contract (`EeWonByteSink`).
#[no_mangle]
pub unsafe extern "C" fn ee_won_guistring_narrow(
    a: *const c_void,
    a_length: u16,
    wchar_size: u32,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let n = narrow(&unsafe { text(a, a_length, wchar_size) }, wchar_size);
    unsafe { crate::ffi::give(sink, context, &n) }
}

/// The narrow constructors' and `AssignStr`'s text: `length` bytes of UTF-8 as `wchar_t` units, given to the sink
/// once (`count` units of `wchar_size` bytes; none for empty text).
///
/// # Safety
/// `narrow` holds `length` bytes (or is NULL with 0); the sink's contract (`EeWonWideSink`).
#[no_mangle]
pub unsafe extern "C" fn ee_won_guistring_widen(
    narrow: *const u8,
    length: usize,
    wchar_size: u32,
    sink: EeWonWideSink,
    context: *mut c_void,
) {
    let bytes: &[u8] = if narrow.is_null() || length == 0 {
        &[]
    } else {
        // SAFETY: `length` bytes (the contract)
        unsafe { std::slice::from_raw_parts(narrow, length) }
    };
    let w = widen(bytes, wchar_size);
    if wchar_size == 2 {
        let u: Vec<u16> = w.iter().map(|&c| c as u16).collect();
        unsafe { sink(context, u.as_ptr().cast(), u.len()) }
    } else {
        unsafe { sink(context, w.as_ptr().cast(), w.len()) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::chat::CChat;

    const GOLDEN: &str = include_str!("golden_guistring.txt");

    // tests/won/won_guistring_test.cpp's tables
    const TEXTS: &[&str] = &[
        "",
        "a",
        "abc",
        "ABC",
        "abd",
        "ab",
        "abcabc",
        "  -42x",
        "12",
        "-",
        " 7 ",
        "\u{e9}t\u{e9}",
        "\u{c9}T\u{c9}",
        "\u{263a} smile",
        "a_b",
        "a-b c",
        "0",
        "\t\u{b}99",
        "x\u{ff}y",
        "aBcAbC",
    ];
    const NARROW: &[&[u8]] = &[
        b"",
        b"a",
        b"abc",
        b"ABC",
        b"ab0",
        b"b",
        b"\xe9t",
        b"\xc9T",
        b"abcd",
        b"0",
        b"\xc3\xa9t",
        b"\xc3\x89T",
        b"\xe2\x98\xba",
        b"x\xf0\x9f\x98\x80y",
    ];
    const CHARS: &[u32] = &[0x61, 0x62, 0x63, 0x20, 0x263a, 0xe9, 0];

    fn w(s: &str) -> Vec<u32> {
        s.chars().map(u32::from).collect()
    }

    fn units(t: &[u32]) -> String {
        if t.is_empty() {
            return "-".into();
        }
        t.iter()
            .map(|&c| {
                if c > 0x20 && c < 0x7f && c != 0x25 {
                    char::from_u32(c).unwrap().to_string()
                } else {
                    format!("%{c:x};")
                }
            })
            .collect()
    }

    fn hex(b: &[u8]) -> String {
        if b.is_empty() {
            "-".into()
        } else {
            b.iter().map(|x| format!("{x:02x}")).collect()
        }
    }

    fn lines() -> Vec<String> {
        let ct = CChat;
        let mut out = Vec::new();
        for (i, t) in TEXTS.iter().enumerate() {
            let a = w(t);
            let case = format!("c t{i:02}");
            let (mut up, mut low) = (a.clone(), a.clone());
            change_case(&mut up, true, &ct);
            change_case(&mut low, false, &ct);
            let ints: Vec<String> = (0..=a.len()).map(|o| to_int(&a, o as u16, &ct).to_string()).collect();
            out.push(format!(
                "{case} {} upper={} lower={} narrow={} int={}",
                units(&a),
                units(&up),
                units(&low),
                hex(&narrow(&a, 2)),
                ints.join(",")
            ));
            let mut l = format!("{case} find");
            let n = a.len() as i32;
            for &c in CHARS {
                for s in 0..=n + 1 {
                    l += &format!(" {}", find_char(&a, c as i32, s));
                }
                for s in -1..n + 1 {
                    l += &format!(" r{}", rfind_char(&a, c as i32, s));
                }
                l += " |";
            }
            out.push(l);
            for (j, u) in TEXTS.iter().enumerate() {
                let b = w(u);
                let mut l = format!("{case} u{j} cmp={} nocase={} find", compare(&a, &b), compare_no_case(&a, &b, &ct));
                for s in 0..=n {
                    l += &format!(" {}/{}/{}", find(&a, &b, s), find_no_case(&a, &b, s, &ct), find_one_of(&a, &b, s));
                }
                l += " rfind";
                for s in -1..=n {
                    l += &format!(" {}", rfind(&a, &b, s));
                }
                out.push(l);
            }
            for (j, nar) in NARROW.iter().enumerate() {
                let nar = widen(nar, 2);
                let mut l = format!("{case} n{j}");
                for o in 0..=n + 1 {
                    l += &format!(" {}/{}", compare_narrow(&a, o, &nar), compare_no_case_narrow(&a, o, &nar, &ct));
                }
                out.push(l);
            }
        }
        // the narrow constructors (whole, std::string, the first half of the bytes) and back
        for (j, nar) in NARROW.iter().enumerate() {
            let (whole, half) = (widen(nar, 2), widen(&nar[..nar.len() / 2], 2));
            out.push(format!(
                "c from n{j} {} back={} half={} back={}",
                units(&whole),
                hex(&narrow(&whole, 2)),
                units(&half),
                hex(&narrow(&half, 2))
            ));
        }
        out
    }

    #[test]
    fn the_c_locale_pass_matches_the_cpp() {
        let got = lines();
        let want: Vec<&str> = GOLDEN.lines().filter(|l| l.starts_with("c ")).collect();
        assert_eq!(got.len(), want.len());
        for (g, w) in got.iter().zip(&want) {
            assert_eq!(g, w);
        }
    }

    #[test]
    fn edges() {
        assert_eq!(to_int(&w("-"), 0, &CChat), 0);
        assert_eq!(to_int(&w("99999999999"), 0, &CChat), 99999999999i64 as i32);
        assert_eq!(rfind(&w("abab"), &w("ab"), -1), 2);
        assert_eq!(rfind(&w("abab"), &w("ab"), 1), 0);
        assert_eq!(compare_narrow(&w("ab"), 0, &widen(b"ab0", 2)), 0);
        assert_eq!(compare_narrow(&w("ab"), 0, &widen(b"ab", 2)), -1);
    }

    #[test]
    fn narrow_text_is_utf8() {
        assert_eq!(widen(b"\xc3\xa9t", 2), vec![0xe9, 0x74]);
        assert_eq!(widen(b"\xe9t", 2), vec![0xfffd, 0x74]);
        assert_eq!(widen(b"\xf0\x9f\x98\x80", 2), vec![0xd83d, 0xde00]);
        assert_eq!(widen(b"\xf0\x9f\x98\x80", 4), vec![0x1f600]);
        assert_eq!(narrow(&[0xe9, 0x263a], 2), b"\xc3\xa9\xe2\x98\xba");
        assert_eq!(narrow(&[0xd83d, 0xde00], 2), b"\xf0\x9f\x98\x80");
        assert_eq!(narrow(&[0x1f600], 4), b"\xf0\x9f\x98\x80");
        assert_eq!(narrow(&[0xd800], 2), b"\xef\xbf\xbd");
        assert_eq!(compare_no_case_narrow(&w("\u{e9}t"), 0, &widen(b"\xc3\xa9T", 2), &CChat), 0);
    }
}
