//! Every place `ee_ui` interprets game text: character stepping, spaces, word ends and line ends. Game text is
//! Windows-1252 bytes today, stepped by the running C runtime's multibyte functions as the C++ did; decision D6
//! (UTF-8 throughout) is a later track, and switching it changes this module only: the widget code ([`crate::wrap`])
//! asks [`Mbcs`] and the predicates below, never a byte value. Widths are not here: they come from the caller's font
//! (`GEText`), per word.

use core::ffi::{c_int, c_uint};

/// The C runtime's multibyte stepping, on NUL-terminated byte slices (`s` includes the terminator).
pub trait Mbcs {
    /// `_mbsinc`: the offset of the character after the one at `at`.
    fn inc(&self, s: &[u8], at: usize) -> usize;
    /// `_mbsninc`: `n` characters on from `at`, stopping at the terminator.
    fn ninc(&self, s: &[u8], at: usize, n: usize) -> usize;
    /// `_ismbcspace`.
    fn is_space(&self, c: c_uint) -> bool;
    /// `_ismbblead`.
    fn is_lead(&self, b: u8) -> bool;
    /// `_ismbcdigit`.
    fn is_digit(&self, c: c_uint) -> bool;
    /// `_mbsicmp` of two NUL-terminated texts: their order with the case folded.
    fn compare_no_case(&self, a: &[u8], b: &[u8]) -> c_int;
}

/// A single-byte code page (Windows-1252, the game's): every byte a character, the C locale's spaces.
pub struct SingleByte;

impl Mbcs for SingleByte {
    fn inc(&self, s: &[u8], at: usize) -> usize {
        (at + 1).min(s.len() - 1)
    }
    fn ninc(&self, s: &[u8], at: usize, n: usize) -> usize {
        let mut at = at;
        for _ in 0..n {
            if s[at] == 0 {
                break;
            }
            at += 1;
        }
        at
    }
    fn is_space(&self, c: c_uint) -> bool {
        c == 0x20 || (0x09..=0x0d).contains(&c)
    }
    fn is_lead(&self, _b: u8) -> bool {
        false
    }
    fn is_digit(&self, c: c_uint) -> bool {
        (0x30..=0x39).contains(&c)
    }
    fn compare_no_case(&self, a: &[u8], b: &[u8]) -> c_int {
        // the UCRT's _mbsicmp in the C locale folds to lower case, as _stricmp does (the list box golden: '_' sorts
        // before the letters; ee_plat_core's POSIX fallback folds to upper)
        for (&x, &y) in a.iter().zip(b) {
            let (x, y) = (x.to_ascii_lowercase(), y.to_ascii_lowercase());
            if x != y {
                return if x < y { -1 } else { 1 };
            }
            if x == 0 {
                return 0;
            }
        }
        0
    }
}

/// The running C runtime's code page, through the platform layer (`PlatMbsInc`, ...; `ee_plat_core`), as the C++
/// stepped. A step past the terminator (a lead byte before the NUL, if the runtime steps over it) stops at the
/// terminator, where the C++ would have read on.
pub struct Platform;

impl Mbcs for Platform {
    fn inc(&self, s: &[u8], at: usize) -> usize {
        // SAFETY: `s` is NUL-terminated and `at` inside it
        let next = unsafe { ee_plat_core::string::PlatMbsInc(s.as_ptr().add(at)).offset_from(s.as_ptr()) };
        (next as usize).min(s.len() - 1)
    }
    fn ninc(&self, s: &[u8], at: usize, n: usize) -> usize {
        // SAFETY: as above; `_mbsninc` stops at the terminator
        let next = unsafe { ee_plat_core::string::PlatMbsNInc(s.as_ptr().add(at), n).offset_from(s.as_ptr()) };
        (next as usize).min(s.len() - 1)
    }
    fn is_space(&self, c: c_uint) -> bool {
        // SAFETY: a plain value
        unsafe { ee_plat_core::string::PlatIsMbcSpace(c) != 0 }
    }
    fn is_lead(&self, b: u8) -> bool {
        // SAFETY: a plain value
        unsafe { ee_plat_core::string::PlatIsMbbLead(b as c_uint) != 0 }
    }
    fn is_digit(&self, c: c_uint) -> bool {
        // SAFETY: a plain value
        unsafe { ee_plat_core::string::PlatIsMbcDigit(c) != 0 }
    }
    fn compare_no_case(&self, a: &[u8], b: &[u8]) -> c_int {
        debug_assert!(a.last() == Some(&0) && b.last() == Some(&0));
        // SAFETY: both NUL-terminated
        unsafe { ee_plat_core::string::PlatMbsICmp(a.as_ptr(), b.as_ptr()) }
    }
}

/// `GetMultiByteChar` (`lle/UIMultiByte.h`): the character at `s[0]` as `_ismbcspace` takes it, from *signed* chars
/// (a byte above 0x7f is negative, as the C++'s `char` was).
pub fn multi_byte_char(m: &impl Mbcs, s: &[u8]) -> i32 {
    let mut c = 0i32;
    let mut s = s;
    if m.is_lead(s[0]) {
        c = (s[0] as i8 as i32) << 8;
        s = &s[1..];
    }
    (s[0] as i8 as i32).wrapping_add(c)
}

/// The character `s[..]` (one character's bytes) ends a word: a newline, a space as the code page sees it, or a tab
/// (`UITextLabel::ReadWord`'s test, which looks at the first byte for the newline and the tab).
pub fn ends_word(m: &impl Mbcs, ch: &[u8]) -> bool {
    ch[0] == b'\n' || m.is_space(multi_byte_char(m, ch) as c_uint) || ch[0] == b'\t'
}

/// The character at `s[at]` is a line end (`ReadLine` and `SetText` compare its first byte with a newline).
pub fn is_line_end(s: &[u8], at: usize) -> bool {
    s[at] == b'\n'
}

/// The terminator of a NUL-terminated text.
pub fn at_end(s: &[u8], at: usize) -> bool {
    s[at] == 0
}

/// The character at `s[at]` starts with the byte `b` (`_mbsnbcmp(s, "\r", 1) == 0` and the like).
pub fn starts_with(s: &[u8], at: usize, b: u8) -> bool {
    s[at] == b
}

/// The character at `s[at..]` is a space as the code page sees it (`_ismbcspace(GetMultiByteChar(s))`).
pub fn is_space_at(m: &impl Mbcs, s: &[u8], at: usize) -> bool {
    m.is_space(multi_byte_char(m, &s[at..]) as c_uint)
}

/// The character at `s[at..]` is a digit (`_ismbcdigit(GetMultiByteChar(s))`: the edit boxes' numbers-only filter).
pub fn is_digit_at(m: &impl Mbcs, s: &[u8], at: usize) -> bool {
    m.is_digit(multi_byte_char(m, &s[at..]) as c_uint)
}

// --- UWideString's view of a text: NUL-terminated bytes counted in characters ---------------------------------------

/// The empty text.
pub fn empty() -> Vec<u8> {
    vec![0]
}

/// A text from its bytes (no terminator in `bytes`).
pub fn from_bytes(bytes: &[u8]) -> Vec<u8> {
    let mut v = bytes.to_vec();
    v.push(0);
    v
}

/// `UWideString`'s `operator==`: `_mbscmp(a, b) == 0`, which compares character by character and so holds exactly
/// when the bytes before the terminators are equal.
pub fn equal(_m: &impl Mbcs, a: &[u8], b: &[u8]) -> bool {
    let end = |s: &[u8]| s.iter().position(|&c| c == 0).unwrap_or(s.len());
    a[..end(a)] == b[..end(b)]
}

/// `GetLength() == 0`: no character before the terminator (a missing terminator counts as the end).
pub fn is_empty(s: &[u8]) -> bool {
    s.first().is_none_or(|&b| b == 0)
}

/// `_mbslen` (`UWideString::GetLength`): the characters before the terminator.
pub fn char_len(m: &impl Mbcs, s: &[u8]) -> usize {
    let (mut at, mut n) = (0, 0);
    while s[at] != 0 {
        at = m.inc(s, at);
        n += 1;
    }
    n
}

/// `_mbsninc(s, n)`: the byte offset of character `n` (the terminator's when the text is shorter).
pub fn offset(m: &impl Mbcs, s: &[u8], n: usize) -> usize {
    m.ninc(s, 0, n)
}

/// `UWideString::CopyString(text, n)`: the first `n` characters (all of a shorter text), terminated.
pub fn prefix(m: &impl Mbcs, s: &[u8], n: usize) -> Vec<u8> {
    from_bytes(&s[..offset(m, s, n)])
}

/// The text from character `n` on (`_mbsninc` into the same buffer), terminated.
pub fn tail<'a>(m: &impl Mbcs, s: &'a [u8], n: usize) -> &'a [u8] {
    &s[offset(m, s, n)..]
}

/// `a += b` (`UWideString::operator+=`): `a` and `b` terminated.
pub fn append(a: &mut Vec<u8>, b: &[u8]) {
    a.pop();
    let end = b.iter().position(|&c| c == 0).unwrap_or(b.len());
    a.extend_from_slice(&b[..end]);
    a.push(0);
}

/// `_mbschr(s, '\n') != NULL`: the text holds a line end (a single-byte character; a double-byte one never matches).
pub fn has_line_end(m: &impl Mbcs, s: &[u8]) -> bool {
    let mut at = 0;
    while s[at] != 0 {
        let next = m.inc(s, at);
        if next == at + 1 && s[at] == b'\n' {
            return true;
        }
        at = next;
    }
    false
}

/// `_mbsnccnt(s, bytes)`: the characters in the first `bytes` bytes.
pub fn chars_in_bytes(m: &impl Mbcs, s: &[u8], bytes: usize) -> usize {
    let (mut at, mut n) = (0, 0);
    while at < bytes && s[at] != 0 {
        at = m.inc(s, at);
        n += 1;
    }
    n
}

/// `UWSNoCaseAlphaSort`: `a` sorts before `b` with the case folded (the list boxes' alphabetical order).
pub fn less_no_case(m: &impl Mbcs, a: &[u8], b: &[u8]) -> bool {
    m.compare_no_case(a, b) < 0
}

/// A password box's text: one `*` per character.
pub fn masked(len: usize) -> Vec<u8> {
    let mut v = vec![b'*'; len];
    v.push(0);
    v
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn word_and_line_ends_on_the_game_page() {
        let m = SingleByte;
        for (ch, end) in
            [(&b" "[..], true), (b"\t", true), (b"\n", true), (b"\r", true), (b"a", false), (b"\xa0", false)]
        {
            assert_eq!(ends_word(&m, ch), end, "{ch:?}");
        }
        assert!(is_line_end(b"a\n\0", 1));
        assert!(!is_line_end(b"a\n\0", 0));
        assert!(at_end(b"a\0", 1));
        assert_eq!(multi_byte_char(&m, b"\xe9"), -23);
    }

    /// A Shift-JIS-shaped page: 0x81..=0x9f lead a two-byte character.
    struct DoubleByte;

    impl Mbcs for DoubleByte {
        fn inc(&self, s: &[u8], at: usize) -> usize {
            let step = if self.is_lead(s[at]) && s[at + 1] != 0 { 2 } else { 1 };
            (at + step).min(s.len() - 1)
        }
        fn ninc(&self, s: &[u8], at: usize, n: usize) -> usize {
            let mut at = at;
            for _ in 0..n {
                if s[at] == 0 {
                    break;
                }
                at = self.inc(s, at);
            }
            at
        }
        fn is_space(&self, c: c_uint) -> bool {
            SingleByte.is_space(c)
        }
        fn is_lead(&self, b: u8) -> bool {
            (0x81..=0x9f).contains(&b)
        }
        fn is_digit(&self, c: c_uint) -> bool {
            SingleByte.is_digit(c)
        }
        fn compare_no_case(&self, a: &[u8], b: &[u8]) -> c_int {
            SingleByte.compare_no_case(a, b)
        }
    }

    #[test]
    fn case_folds_to_lower_like_the_ucrts_mbsicmp() {
        let s = SingleByte;
        assert_eq!(s.compare_no_case(b"abc\0", b"ABC\0"), 0);
        assert!(less_no_case(&s, b"Apple\0", b"banana\0"));
        assert!(!less_no_case(&s, b"banana\0", b"Apple\0"));
        assert!(less_no_case(&s, b"ab\0", b"abc\0"), "a prefix sorts first");
        // '_' (0x5f) sits between 'Z' and 'a': lower-case folding puts it before every letter
        assert!(less_no_case(&s, b"_\0", b"A\0"));
        assert!(!less_no_case(&s, b"z\0", b"_\0"));
        assert_ne!(s.compare_no_case(b"\xe9\0", b"\xc9\0"), 0, "the C locale folds ASCII only");
    }

    #[test]
    fn texts_count_characters_not_bytes() {
        let (s, d) = (SingleByte, DoubleByte);
        let t = b"a\x82\x0ab\0";
        assert_eq!(char_len(&s, t), 4);
        assert_eq!(char_len(&d, t), 3);
        assert!(has_line_end(&s, t), "a single-byte page sees the line end");
        assert!(!has_line_end(&d, t), "a trail byte is not a line end");
        assert_eq!(prefix(&d, t, 2), b"a\x82\x0a\0");
        assert_eq!(prefix(&s, t, 9), t.to_vec(), "a short text is copied whole");
        assert_eq!(tail(&d, t, 2), b"b\0");
        assert_eq!(offset(&s, t, 9), 4);
        assert_eq!(chars_in_bytes(&d, t, 2), 2, "a character started inside the bytes counts");
        assert_eq!(chars_in_bytes(&s, t, 9), 4, "the terminator ends the count");
        let mut a = from_bytes(b"ab");
        append(&mut a, b"cd\0");
        assert_eq!(a, b"abcd\0");
        assert_eq!(masked(3), b"***\0");
        assert_eq!(empty(), b"\0");
        assert!(is_digit_at(&s, b"7\0", 0));
        assert!(!is_digit_at(&s, b"\xb2\0", 0), "signed: a high byte is never a digit");
        assert!(is_space_at(&s, b"x\t\0", 1));
        assert!(starts_with(b"\r\0", 0, b'\r'));
        assert_eq!(SingleByte.inc(b"\0", 0), 0, "no step past the terminator");
    }
}
