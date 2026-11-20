//! `UIGetStringCutOffIndex` (was User Interface.cpp): how many characters of a text fit a pixel width, the text
//! measured one character at a time (a tab is four spaces, a carriage return nothing), cut back to just after the
//! last space when one was passed (`break_at_space`) and stopped after a newline (`stop_at_newline`). The in-game
//! chat line cuts its message with it (ee_game chat_line.rs).
//!
//! The quirks are kept: the widths are summed in `f32` and compared with the width as `f32`; a newline's own width
//! counts before the stop; a text whose first character already overflows (or an empty text) answers its whole
//! length.

use core::ffi::c_ulong;

use crate::text::{at_end, is_space_at, starts_with, Mbcs};

/// The characters of `text` (NUL-terminated) that fit `width`. `measure` is the width of one character's bytes (an
/// empty slice for the carriage return and a tab's set-up), as a fresh `GEText` of the font measures it.
pub fn cut_off_index(
    m: &impl Mbcs,
    measure: &mut impl FnMut(&[u8]) -> f32,
    text: &[u8],
    width: c_ulong,
    break_at_space: bool,
    stop_at_newline: bool,
) -> c_ulong {
    let mut total = 0.0f32;
    let mut space_index: c_ulong = 0;
    let mut tabs = 0u32;
    let mut index: c_ulong = 0;
    let mut newline = false;
    let mut found_space = false;
    let mut at = 0usize;
    while !at_end(text, at) {
        let ch: &[u8] = if tabs == 0 {
            let next = m.inc(text, at);
            if stop_at_newline && starts_with(text, at, b'\n') {
                newline = true;
            }
            if break_at_space && is_space_at(m, text, at) {
                found_space = true;
                space_index = index.wrapping_add(1);
            }
            if starts_with(text, at, b'\r') {
                b""
            } else if starts_with(text, at, b'\t') {
                tabs = 4;
                b""
            } else {
                &text[at..next]
            }
        } else {
            tabs -= 1;
            b" "
        };
        total += measure(ch);
        if total > width as f32 {
            if !newline && found_space {
                index = space_index;
            }
            break;
        }
        if newline {
            break;
        }
        if tabs == 0 {
            at = m.inc(text, at);
        }
        index = index.wrapping_add(1);
    }
    if at == 0 && !newline {
        index = characters(m, text);
    }
    index
}

/// The characters before the terminator (`UWideString::GetLength`).
fn characters(m: &impl Mbcs, text: &[u8]) -> c_ulong {
    let mut n: c_ulong = 0;
    let mut at = 0;
    while !at_end(text, at) {
        at = m.inc(text, at);
        n += 1;
    }
    n
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::text::{Platform, SingleByte};

    /// Every character 10 wide but `i` (4) and `W` (14); empty 0.
    fn width_of(ch: &[u8]) -> f32 {
        match ch {
            b"" => 0.0,
            b"i" => 4.0,
            b"W" => 14.0,
            _ => 10.0,
        }
    }

    fn cut(text: &str, width: c_ulong, space: bool, newline: bool) -> c_ulong {
        let mut t = text.as_bytes().to_vec();
        t.push(0);
        cut_off_index(&SingleByte, &mut width_of, &t, width, space, newline)
    }

    #[test]
    fn counts_what_fits_and_cuts_back_to_a_space() {
        assert_eq!(cut("abcdef", 35, false, false), 3);
        assert_eq!(cut("abcdef", 30, false, false), 3, "equal to the width still fits");
        assert_eq!(cut("ab cdef", 55, true, false), 3, "after the space");
        assert_eq!(cut("ab cdef", 55, false, false), 5);
        assert_eq!(cut("abc", 100, true, false), 3, "everything fits");
        assert_eq!(cut("iiiW", 25, false, false), 3);
    }

    #[test]
    fn tabs_are_four_spaces_and_returns_nothing() {
        // the tab's set-up measures nothing, then four spaces: "a\tb" is 10 + 0 + 40 + 10
        assert_eq!(cut("a\tb", 45, false, false), 5, "a, the tab, three spaces");
        assert_eq!(cut("a\tb", 55, false, false), 6);
        assert_eq!(cut("a\tb", 60, false, false), 7);
        assert_eq!(cut("a\r\rb", 15, false, false), 3, "returns are free");
    }

    #[test]
    fn a_newline_stops_after_itself_or_counts_as_a_character() {
        assert_eq!(cut("ab\ncd", 100, false, true), 2, "the newline is not counted");
        assert_eq!(cut("ab\ncd", 100, false, false), 5);
        assert_eq!(cut("ab\ncd", 25, true, true), 2, "the newline flag keeps the width's cut");
    }

    #[test]
    fn a_first_character_too_wide_or_an_empty_text_answers_the_whole_length() {
        assert_eq!(cut("Wxyz", 5, false, false), 4);
        assert_eq!(cut("", 5, false, false), 0);
        assert_eq!(cut("\nxyz", 0, false, true), 0, "but a stop at a first newline is a stop");
    }

    #[test]
    fn utf8_characters_are_measured_whole() {
        let mut seen = Vec::new();
        let t = "aé€b\0".as_bytes();
        let n = cut_off_index(
            &Platform,
            &mut |c: &[u8]| {
                seen.push(c.to_vec());
                10.0
            },
            t,
            25,
            false,
            false,
        );
        assert_eq!(n, 2);
        assert_eq!(seen, vec![b"a".to_vec(), "é".as_bytes().to_vec(), "€".as_bytes().to_vec()]);
        let all = cut_off_index(&Platform, &mut |_: &[u8]| 100.0, t, 5, false, false);
        assert_eq!(all, 4, "characters, not bytes");
    }
}
