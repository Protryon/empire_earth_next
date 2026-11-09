//! A multi-line `UITextLabel`'s word wrapping (was `UITextLabel::ReadWord`, `ReadLine` and the line loop of
//! `SetText`): Windows-1252 (or the running code page's multibyte) bytes cut into lines that fit the text rectangle,
//! by whole words, an over-wide first word cut by width.
//!
//! Text is bytes with a NUL terminator, stepped through the C runtime's multibyte functions ([`Mbcs`]), as the C++
//! did with `PlatMbsInc`/`PlatMbsNInc`/`PlatIsMbcSpace`. Widths come from the caller (`GEText`'s extent of a word;
//! the C++ keeps the text object, whose last measured text is a side effect the label lives with).
//!
//! The quirks are kept: a word stops after 256 characters but counts 257 when text follows, so the line skips the
//! 257th character (the line itself is copied from the source, so only the next line loses it); a line ends after a
//! newline, which the stored line drops.

use core::ffi::{c_long, c_ulong};

use crate::rect::{um_floor, um_floor_unsigned};
use crate::text::{at_end, ends_word, is_line_end};
pub use crate::text::{Mbcs, Platform, SingleByte};

/// `ReadWord`: the next word of `text` (NUL-terminated), up to and including a space, tab or newline, at most 256
/// characters -> (the character count, the word's bytes). The count is 257 when the cap stops a word that goes on.
pub fn read_word(m: &impl Mbcs, text: &[u8]) -> (c_ulong, Vec<u8>) {
    let mut count: c_ulong = 0;
    let mut at = 0;
    let mut word = Vec::new();
    loop {
        if at_end(text, at) {
            break;
        }
        let under = count < 256;
        count += 1;
        if !under {
            break;
        }
        let next = m.inc(text, at);
        let ch = &text[at..next];
        word.extend_from_slice(ch);
        if ends_word(m, ch) {
            break;
        }
        at = next;
    }
    (count, word)
}

/// NUL-terminated copy.
fn terminated(bytes: &[u8]) -> Vec<u8> {
    let mut v = Vec::with_capacity(bytes.len() + 1);
    v.extend_from_slice(bytes);
    v.push(0);
    v
}

/// `ReadLine`: how many characters of `text` (NUL-terminated) fit `width` by whole words; an over-wide first word is
/// cut to the longest start narrower than the width. `measure` is the text's width (`GEText::GetTextExtent`).
pub fn read_line(m: &impl Mbcs, measure: &mut impl FnMut(&[u8]) -> f32, text: &[u8], width: f32) -> c_ulong {
    let mut total: c_ulong = 0;
    let mut length: c_ulong = 0;
    let mut at = 0;
    while !at_end(text, at) {
        let (count, word) = read_word(m, &text[at..]);
        let mut word_length = count as c_long;
        let mut word = terminated(&word);
        total = total.wrapping_add(um_floor(measure(&word[..word.len() - 1])) as c_ulong);
        if total as f32 > width {
            if length == 0 {
                while word_length > 0 {
                    word_length -= 1;
                    let cut = m.ninc(&word, 0, word_length as usize);
                    word.truncate(cut);
                    word.push(0);
                    if (um_floor(measure(&word[..cut])) as c_ulong) < um_floor_unsigned(width) {
                        length = word_length as c_ulong;
                        break;
                    }
                }
            }
            break;
        }
        length = length.wrapping_add(word_length as c_ulong);
        at = m.ninc(text, at, word_length as usize);
        let last = m.ninc(&word, 0, (word_length - 1) as usize);
        if is_line_end(&word, last) {
            break;
        }
    }
    length
}

/// The line loop of `SetText` for a multi-line label: each line [`read_line`] gives, without a final newline, handed
/// to `line` in order. Stops at the end of the text or at a line of no characters (nothing fits).
pub fn split_lines(
    m: &impl Mbcs,
    measure: &mut impl FnMut(&[u8]) -> f32,
    text: &[u8],
    width: f32,
    line: &mut impl FnMut(&[u8]),
) {
    let mut at = 0;
    while !at_end(text, at) {
        let length = read_line(m, measure, &text[at..], width);
        if length == 0 {
            break;
        }
        let end = m.ninc(text, at, length as usize);
        let copy = terminated(&text[at..end]);
        let mut last = m.ninc(&copy, 0, (length - 1) as usize);
        if !is_line_end(&copy, last) {
            last = m.inc(&copy, last);
        }
        line(&copy[..last]);
        at = end;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::text::multi_byte_char;
    use core::ffi::c_uint;

    const GOLDEN: &str = include_str!("golden_label.txt");

    /// The golden driver's fake font: a fixed width per byte.
    fn fake_width(b: u8) -> f32 {
        3.0 + ((b as u32 * 7) % 9) as f32 + 0.25 * (b % 4) as f32
    }

    /// The fake `GEText::ComputeTextExtent` and the driver's FNV-1a over every measured text with its NUL.
    struct Meter {
        count: u32,
        hash: u32,
    }

    impl Meter {
        fn new() -> Self {
            Meter { count: 0, hash: 2166136261 }
        }
        fn measure(&mut self, text: &[u8]) -> f32 {
            let mut w = 0.0f32;
            for &b in text {
                w += fake_width(b);
            }
            for &b in text.iter().chain([0u8].iter()) {
                self.hash = (self.hash ^ b as u32).wrapping_mul(16777619);
            }
            self.count += 1;
            w
        }
        fn take(&mut self) -> String {
            let s = format!("  measures {} {:08x}", self.count, self.hash);
            *self = Meter::new();
            s
        }
    }

    fn hex(b: &[u8]) -> String {
        if b.is_empty() {
            return "-".into();
        }
        b.iter().map(|x| format!("{x:02x}")).collect()
    }

    fn unhex(s: &str) -> Vec<u8> {
        if s == "-" {
            return Vec::new();
        }
        (0..s.len()).step_by(2).map(|i| u8::from_str_radix(&s[i..i + 2], 16).unwrap()).collect()
    }

    fn f(s: &str) -> f32 {
        f32::from_bits(u32::from_str_radix(s, 16).unwrap())
    }

    fn lines_of(m: &mut Meter, text: &[u8], width: f32, into: &mut Vec<Vec<u8>>) {
        let t = terminated(text);
        split_lines(&SingleByte, &mut |w: &[u8]| m.measure(w), &t, width, &mut |l: &[u8]| into.push(l.to_vec()));
    }

    fn listing(lines: &[Vec<u8>]) -> String {
        let mut s = format!("  = {}", lines.len());
        for l in lines {
            s.push(' ');
            s.push_str(&hex(l));
        }
        s
    }

    #[test]
    fn replays_the_cpp_wrapping_line_for_line() {
        let golden: Vec<&str> = GOLDEN.lines().collect();
        let mut out: Vec<String> = Vec::new();
        let mut text: Vec<u8> = Vec::new();
        let mut m = Meter::new();
        let mut i = 0;
        while i < golden.len() {
            let line = golden[i];
            let t: Vec<&str> = line.split(' ').collect();
            match t[0] {
                "label" => out.push(line.to_string()),
                "" if i == 0 => out.push(m.take()), // the constructor's SetText("")
                "text" => {
                    out.push(line.to_string());
                    text = unhex(t[1]);
                    // its words, as the driver walked them
                    let all = terminated(&text);
                    let mut at = 0;
                    while all[at] != 0 {
                        let (count, word) = read_word(&SingleByte, &all[at..]);
                        out.push(format!("word {} -> {count} {}", hex(&all[at..all.len() - 1]), hex(&word)));
                        at = SingleByte.ninc(&all, at, count as usize);
                    }
                }
                "line" => {
                    out.push(line.to_string());
                    let n = read_line(&SingleByte, &mut |w: &[u8]| m.measure(w), &terminated(&text), f(t[1]));
                    out.push(m.take());
                    out.push(format!("  = {n}"));
                }
                "set" => {
                    out.push(line.to_string());
                    let mut lines = Vec::new();
                    lines_of(&mut m, &text, f(t[1]), &mut lines);
                    out.push(m.take());
                    out.push(listing(&lines));
                }
                "append" => {
                    // the driver's last two steps: SetText("first part of it", true), then append
                    let mut lines = Vec::new();
                    lines_of(&mut m, b"first part of it", 60.0, &mut lines);
                    out.push(m.take());
                    out.push(line.to_string());
                    lines_of(&mut m, b"and the second part", 60.0, &mut lines);
                    out.push(m.take());
                    out.push(listing(&lines));
                }
                _ => {}
            }
            i += 1;
        }
        assert_eq!(out.len(), golden.len(), "line count");
        for (k, (a, b)) in out.iter().zip(golden.iter()).enumerate() {
            assert_eq!(a, b, "line {}", k + 1);
        }
    }

    #[test]
    fn a_long_word_counts_257_and_the_next_word_starts_one_later() {
        let mut text = vec![b'w'; 300];
        text.push(0);
        let (count, word) = read_word(&SingleByte, &text);
        assert_eq!((count, word.len()), (257, 256));
        let (count, word) = read_word(&SingleByte, &text[256..]);
        assert_eq!((count, word.len()), (44, 44));
        // with nothing after the 256th character the count is exact
        let mut exact = vec![b'v'; 256];
        exact.push(0);
        assert_eq!(read_word(&SingleByte, &exact).0, 256);
    }

    #[test]
    fn a_word_ends_with_its_separator() {
        let t = b"ab\tcd ef\ngh\0";
        assert_eq!(read_word(&SingleByte, t), (3, b"ab\t".to_vec()));
        assert_eq!(read_word(&SingleByte, &t[3..]), (3, b"cd ".to_vec()));
        assert_eq!(read_word(&SingleByte, &t[6..]), (3, b"ef\n".to_vec()));
        assert_eq!(read_word(&SingleByte, &t[9..]), (2, b"gh".to_vec()));
        assert_eq!(read_word(&SingleByte, b"\0"), (0, Vec::new()));
        // a no-break space (0xa0) is a negative char: never a space
        assert_eq!(read_word(&SingleByte, b"a\xa0b c\0").1, b"a\xa0b ".to_vec());
    }

    #[test]
    fn lines_break_at_newlines_and_drop_them() {
        let mut got = Vec::new();
        let t = terminated(b"one\n\ntwo three");
        split_lines(&SingleByte, &mut |w: &[u8]| w.len() as f32, &t, 100.0, &mut |l: &[u8]| got.push(l.to_vec()));
        assert_eq!(got, [b"one".to_vec(), b"".to_vec(), b"two three".to_vec()]);
    }

    #[test]
    fn an_over_wide_first_word_is_cut_by_width() {
        let t = terminated(b"abcdefgh ij");
        // one unit per byte, 5 wide: "abcd" (4 < 5) fits
        assert_eq!(read_line(&SingleByte, &mut |w: &[u8]| w.len() as f32, &t, 5.0), 4);
        let mut got = Vec::new();
        split_lines(&SingleByte, &mut |w: &[u8]| w.len() as f32, &t, 5.0, &mut |l: &[u8]| got.push(l.to_vec()));
        assert_eq!(got, [b"abcd".to_vec(), b"efgh ".to_vec(), b"ij".to_vec()]); // "efgh " is 5 wide: not over 5
                                                                                // nothing fits a zero width: no lines at all
        got.clear();
        split_lines(&SingleByte, &mut |w: &[u8]| w.len() as f32, &t, 0.0, &mut |l: &[u8]| got.push(l.to_vec()));
        assert!(got.is_empty());
    }

    /// A double-byte code page in the shape of Shift-JIS (lead bytes 0x81-0x9f) for the multibyte paths.
    struct DoubleByte;

    impl Mbcs for DoubleByte {
        fn inc(&self, s: &[u8], at: usize) -> usize {
            if self.is_lead(s[at]) && s[at + 1] != 0 {
                at + 2
            } else {
                at + 1
            }
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
            c == 0x20 || c == 0xffff_8140 // the ideographic space as GetMultiByteChar gives it
        }
        fn is_lead(&self, b: u8) -> bool {
            (0x81..=0x9f).contains(&b)
        }
    }

    #[test]
    fn the_platform_steps_ascii_like_a_single_byte_page() {
        // whatever the test machine's code page, ASCII is single-byte and its spaces are the C locale's
        let t: Vec<u8> = (1u8..0x80).chain([0u8]).collect();
        for at in 0..t.len() - 1 {
            assert_eq!(Platform.inc(&t, at), SingleByte.inc(&t, at));
            assert_eq!(Platform.ninc(&t, at, 3), SingleByte.ninc(&t, at, 3));
            assert_eq!(Platform.is_space(t[at] as c_uint), SingleByte.is_space(t[at] as c_uint), "{at}");
            assert!(!Platform.is_lead(t[at]));
        }
        assert_eq!(Platform.ninc(&t, 120, 50), t.len() - 1);
        let s = terminated(b"The quick brown fox jumps");
        let mut a = Vec::new();
        let mut b = Vec::new();
        split_lines(&Platform, &mut |w: &[u8]| w.len() as f32, &s, 10.0, &mut |l: &[u8]| a.push(l.to_vec()));
        split_lines(&SingleByte, &mut |w: &[u8]| w.len() as f32, &s, 10.0, &mut |l: &[u8]| b.push(l.to_vec()));
        assert_eq!(a, b);
    }

    #[test]
    fn double_byte_characters_stay_whole() {
        assert_eq!(multi_byte_char(&DoubleByte, b"\x81\x40"), (0x81u8 as i8 as i32) << 8 | 0x40);
        let t = b"\x82\xa0\x82\xa2\x81\x40x\0";
        let (count, word) = read_word(&DoubleByte, t);
        assert_eq!((count, word), (3, t[..6].to_vec()));
        let mut got = Vec::new();
        let t = terminated(b"\x82\xa0\x82\xa2\x82\xa4");
        // two bytes, two units, per character; 5 wide: two characters fit
        split_lines(&DoubleByte, &mut |w: &[u8]| w.len() as f32, &t, 5.0, &mut |l: &[u8]| got.push(l.to_vec()));
        assert_eq!(got, [b"\x82\xa0\x82\xa2".to_vec(), b"\x82\xa4".to_vec()]);
    }
}
