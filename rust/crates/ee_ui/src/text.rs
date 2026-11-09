//! Every place `ee_ui` interprets game text: character stepping, spaces, word ends and line ends. Game text is
//! Windows-1252 bytes today, stepped by the running C runtime's multibyte functions as the C++ did; decision D6
//! (UTF-8 throughout) is a later track, and switching it changes this module only: the widget code ([`crate::wrap`])
//! asks [`Mbcs`] and the predicates below, never a byte value. Widths are not here: they come from the caller's font
//! (`GEText`), per word.

use core::ffi::c_uint;

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
}

/// A single-byte code page (Windows-1252, the game's): every byte a character, the C locale's spaces.
pub struct SingleByte;

impl Mbcs for SingleByte {
    fn inc(&self, _s: &[u8], at: usize) -> usize {
        at + 1
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
}
