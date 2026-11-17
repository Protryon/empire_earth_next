//! UTF-8 game text (docs/RUST_PLAN.md D6): the one place that knows how the game's bytes make characters. A
//! character is a code point; a byte that is not part of a well-formed sequence (a stray continuation byte, a lead byte
//! without its trail, an overlong form, a surrogate, a value past U+10FFFF) is one character of its own, reported as
//! `STRAY_BASE + byte` so that no two texts compare equal unless their bytes are, and never a step past a terminator.
//! Case folding is Unicode's simple lower-case mapping (one character to one character), the same on every system.
//!
//! Every function here works on byte slices without a terminator; `string.rs` wraps them for the C API, which works
//! on NUL-terminated text (a NUL is never a continuation byte, so a sequence can be decoded lazily up to it).

/// A stray byte's character: `STRAY_BASE + byte` (U+DC80..U+DCFF, a range no well-formed text decodes to).
pub const STRAY_BASE: u32 = 0xDC00;

/// U+FFFD, what a stray byte is shown as.
pub const REPLACEMENT: u32 = 0xFFFD;

/// The length of the sequence a lead byte starts: 1 for ASCII, 2..4 for a lead, 0 for a byte that leads nothing.
#[inline]
pub fn lead_len(b: u8) -> usize {
    match b {
        0x00..=0x7F => 1,
        0xC2..=0xDF => 2,
        0xE0..=0xEF => 3,
        0xF0..=0xF4 => 4,
        _ => 0,
    }
}

/// The character at the start of `s` and the bytes it takes (`s` not empty). A stray byte is `STRAY_BASE + byte`, 1.
pub fn decode(s: &[u8]) -> (u32, usize) {
    let b0 = s[0];
    let n = lead_len(b0);
    if n == 1 {
        return (b0 as u32, 1);
    }
    if n == 0 || s.len() < n {
        return (STRAY_BASE + b0 as u32, 1);
    }
    let b1 = s[1];
    // the second byte's range depends on the lead (Unicode table 3-7): no overlongs, no surrogates, nothing past U+10FFFF
    let ok1 = match b0 {
        0xE0 => (0xA0..=0xBF).contains(&b1),
        0xED => (0x80..=0x9F).contains(&b1),
        0xF0 => (0x90..=0xBF).contains(&b1),
        0xF4 => (0x80..=0x8F).contains(&b1),
        _ => (0x80..=0xBF).contains(&b1),
    };
    if !ok1 {
        return (STRAY_BASE + b0 as u32, 1);
    }
    let mut cp = match n {
        2 => (b0 & 0x1F) as u32,
        3 => (b0 & 0x0F) as u32,
        _ => (b0 & 0x07) as u32,
    };
    cp = (cp << 6) | (b1 & 0x3F) as u32;
    for &bi in &s[2..n] {
        if bi & 0xC0 != 0x80 {
            return (STRAY_BASE + b0 as u32, 1);
        }
        cp = (cp << 6) | (bi & 0x3F) as u32;
    }
    (cp, n)
}

/// The bytes of a character: a stray byte's character gives that byte back, any other surrogate or a value past
/// U+10FFFF gives U+FFFD. Returns the count written (1..=4).
pub fn encode(cp: u32, out: &mut [u8; 4]) -> usize {
    if (STRAY_BASE + 0x80..=STRAY_BASE + 0xFF).contains(&cp) {
        out[0] = (cp - STRAY_BASE) as u8;
        return 1;
    }
    let cp = if (0xD800..=0xDFFF).contains(&cp) || cp > 0x10FFFF { REPLACEMENT } else { cp };
    if cp < 0x80 {
        out[0] = cp as u8;
        1
    } else if cp < 0x800 {
        out[0] = 0xC0 | (cp >> 6) as u8;
        out[1] = 0x80 | (cp & 0x3F) as u8;
        2
    } else if cp < 0x10000 {
        out[0] = 0xE0 | (cp >> 12) as u8;
        out[1] = 0x80 | ((cp >> 6) & 0x3F) as u8;
        out[2] = 0x80 | (cp & 0x3F) as u8;
        3
    } else {
        out[0] = 0xF0 | (cp >> 18) as u8;
        out[1] = 0x80 | ((cp >> 12) & 0x3F) as u8;
        out[2] = 0x80 | ((cp >> 6) & 0x3F) as u8;
        out[3] = 0x80 | (cp & 0x3F) as u8;
        4
    }
}

/// Appends a character's bytes.
pub fn push(out: &mut Vec<u8>, cp: u32) {
    let mut b = [0u8; 4];
    let n = encode(cp, &mut b);
    out.extend_from_slice(&b[..n]);
}

/// Whether `s` is well-formed UTF-8 throughout (no stray byte).
pub fn is_valid(s: &[u8]) -> bool {
    std::str::from_utf8(s).is_ok()
}

/// The offset after the character at `s[at]` (`s.len()` at most; `at + 1` at a stray byte).
#[inline]
pub fn next(s: &[u8], at: usize) -> usize {
    if at >= s.len() {
        return s.len();
    }
    at + decode(&s[at..]).1
}

/// The start of the character before `s[at]` (`at` > 0): back over up to three continuation bytes to a lead whose
/// sequence reaches `at`; otherwise the byte before `at` is a stray character of its own.
pub fn prev(s: &[u8], at: usize) -> usize {
    debug_assert!(at > 0 && at <= s.len());
    let mut p = at - 1;
    let lowest = at.saturating_sub(4);
    while p > lowest && s[p] & 0xC0 == 0x80 {
        p -= 1;
    }
    if next(s, p) == at {
        p
    } else {
        at - 1
    }
}

/// The characters of `s` as (offset, code point, byte length).
pub fn chars(s: &[u8]) -> impl Iterator<Item = (usize, u32, usize)> + '_ {
    let mut at = 0;
    std::iter::from_fn(move || {
        if at >= s.len() {
            return None;
        }
        let (cp, n) = decode(&s[at..]);
        let here = at;
        at += n;
        Some((here, cp, n))
    })
}

/// The characters in `s`.
pub fn char_len(s: &[u8]) -> usize {
    chars(s).count()
}

/// The bytes of the first `n` characters (all of `s` when it has fewer).
pub fn bytes_of_chars(s: &[u8], n: usize) -> usize {
    let mut at = 0;
    for _ in 0..n {
        if at >= s.len() {
            break;
        }
        at = next(s, at);
    }
    at
}

/// The characters in the first `n` bytes; a character started inside them counts (`_mbsnccnt`).
pub fn chars_in_bytes(s: &[u8], n: usize) -> usize {
    let mut at = 0;
    let mut count = 0;
    while at < n.min(s.len()) {
        at = next(s, at);
        count += 1;
    }
    count
}

/// Unicode's simple lower-case mapping of a character, itself when there is none or it is not one character.
pub fn fold(cp: u32) -> u32 {
    match char::from_u32(cp) {
        Some(c) => {
            let mut l = c.to_lowercase();
            match (l.next(), l.next()) {
                (Some(x), None) => x as u32,
                _ => cp,
            }
        }
        None => cp,
    }
}

/// Unicode's simple upper-case mapping of a character, itself when there is none or it is not one character.
pub fn upper(cp: u32) -> u32 {
    match char::from_u32(cp) {
        Some(c) => {
            let mut u = c.to_uppercase();
            match (u.next(), u.next()) {
                (Some(x), None) => x as u32,
                _ => cp,
            }
        }
        None => cp,
    }
}

/// Compares two texts character by character with the case folded: the difference of the first folded characters
/// that differ (as `_stricmp`), 0 when equal.
pub fn compare_folded(a: &[u8], b: &[u8]) -> i32 {
    compare_folded_bytes(a, b, usize::MAX)
}

/// `compare_folded` over at most `bytes` bytes of each (a character started inside them counts whole).
pub fn compare_folded_bytes(a: &[u8], b: &[u8], bytes: usize) -> i32 {
    let (mut ia, mut ib) = (0, 0);
    while ia < bytes && ib < bytes {
        let x = if ia < a.len() { decode(&a[ia..]) } else { (0, 1) };
        let y = if ib < b.len() { decode(&b[ib..]) } else { (0, 1) };
        let (fx, fy) = (fold(x.0), fold(y.0));
        if fx != fy {
            return fx as i32 - fy as i32;
        }
        if fx == 0 {
            return 0;
        }
        ia += x.1;
        ib += y.1;
    }
    0
}

/// `compare_folded` over at most `n` characters of each.
pub fn compare_folded_chars(a: &[u8], b: &[u8], n: usize) -> i32 {
    let (mut ia, mut ib) = (0, 0);
    for _ in 0..n {
        let x = if ia < a.len() { decode(&a[ia..]) } else { (0, 1) };
        let y = if ib < b.len() { decode(&b[ib..]) } else { (0, 1) };
        let (fx, fy) = (fold(x.0), fold(y.0));
        if fx != fy {
            return fx as i32 - fy as i32;
        }
        if fx == 0 {
            return 0;
        }
        ia += x.1;
        ib += y.1;
    }
    0
}

/// Byte order (`strcmp`'s, which is code point order for well-formed text): -1, 0 or 1.
pub fn compare(a: &[u8], b: &[u8]) -> i32 {
    match a.cmp(b) {
        std::cmp::Ordering::Less => -1,
        std::cmp::Ordering::Equal => 0,
        std::cmp::Ordering::Greater => 1,
    }
}

/// `compare` over at most `n` characters of each (`_mbsncmp`).
pub fn compare_chars(a: &[u8], b: &[u8], n: usize) -> i32 {
    compare(&a[..bytes_of_chars(a, n)], &b[..bytes_of_chars(b, n)])
}

/// The in-place case change of `s` where the mapping keeps a character's byte length (`_mbslwr`/`_mbsupr`): the
/// mapped characters are written over the old, the others stay.
pub fn map_in_place(s: &mut [u8], map: fn(u32) -> u32) {
    let mut at = 0;
    while at < s.len() {
        let (cp, n) = decode(&s[at..]);
        let m = map(cp);
        if m != cp {
            let mut b = [0u8; 4];
            if encode(m, &mut b) == n {
                s[at..at + n].copy_from_slice(&b[..n]);
            }
        }
        at += n;
    }
}

/// The offset of the character `cp` in `s`, if any (`_mbschr`).
pub fn find_char(s: &[u8], cp: u32) -> Option<usize> {
    chars(s).find(|&(_, c, _)| c == cp).map(|(at, _, _)| at)
}

/// `_ismbcspace` of a character: the C locale's white space (the layout breaks words only there).
pub fn is_space(cp: u32) -> bool {
    cp == 0x20 || (0x09..=0x0D).contains(&cp)
}

/// `_ismbcdigit` of a character: an ASCII digit.
pub fn is_digit(cp: u32) -> bool {
    (0x30..=0x39).contains(&cp)
}

/// `_ismbblead` of a byte: it starts a multi-byte sequence.
pub fn is_lead(b: u8) -> bool {
    lead_len(b) > 1
}

/// Text with every stray byte replaced by U+FFFD (what a display shows).
pub fn lossy(s: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(s.len());
    for (at, cp, n) in chars(s) {
        if (STRAY_BASE..STRAY_BASE + 0x100).contains(&cp) {
            push(&mut out, REPLACEMENT);
        } else {
            out.extend_from_slice(&s[at..at + n]);
        }
    }
    out
}

/// The text as `str`: stray bytes as U+FFFD.
pub fn to_string_lossy(s: &[u8]) -> String {
    String::from_utf8(lossy(s)).unwrap_or_default()
}

/// Windows-1252 0x80..0x9F as code points (0: a byte the code page leaves undefined, mapped to the C1 control of
/// the same value, as Windows does).
pub const CP1252_HIGH: [u16; 32] = [
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0, 0,
    0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
];

/// The code point a Windows-1252 byte means (every byte has one: the five undefined bytes are C1 controls).
pub fn cp1252_to_codepoint(b: u8) -> u32 {
    if (0x80..0xA0).contains(&b) {
        let u = CP1252_HIGH[(b - 0x80) as usize];
        if u != 0 {
            return u as u32;
        }
    }
    b as u32
}

/// The Windows-1252 byte of a code point, if the code page has it.
pub fn codepoint_to_cp1252(cp: u32) -> Option<u8> {
    if cp < 0x80 || (0xA0..=0xFF).contains(&cp) {
        return Some(cp as u8);
    }
    for (i, &u) in CP1252_HIGH.iter().enumerate() {
        let c = if u != 0 { u as u32 } else { 0x80 + i as u32 };
        if c == cp {
            return Some(0x80 + i as u8);
        }
    }
    None
}

/// Windows-1252 bytes (an old input) as UTF-8.
pub fn from_cp1252(s: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(s.len() + s.len() / 4);
    for &b in s {
        push(&mut out, cp1252_to_codepoint(b));
    }
    out
}

/// UTF-8 as Windows-1252 bytes: a character the code page lacks (and a stray byte) becomes `?`.
pub fn to_cp1252(s: &[u8]) -> Vec<u8> {
    chars(s).map(|(_, cp, _)| codepoint_to_cp1252(cp).unwrap_or(b'?')).collect()
}

/// The UTF-16 units of UTF-8 text (a stray byte gives U+FFFD).
pub fn to_utf16(s: &[u8]) -> Vec<u16> {
    let mut out = Vec::with_capacity(s.len());
    for (_, cp, _) in chars(s) {
        let cp = if (0xD800..=0xDFFF).contains(&cp) { REPLACEMENT } else { cp };
        if cp >= 0x10000 {
            let v = cp - 0x10000;
            out.push(0xD800 + (v >> 10) as u16);
            out.push(0xDC00 + (v & 0x3FF) as u16);
        } else {
            out.push(cp as u16);
        }
    }
    out
}

/// The UTF-8 bytes of UTF-16 units: a surrogate pair is one character, a lone surrogate U+FFFD.
pub fn from_utf16(units: &[u16]) -> Vec<u8> {
    let mut out = Vec::with_capacity(units.len());
    let mut i = 0;
    while i < units.len() {
        let u = units[i] as u32;
        let cp = if (0xD800..0xDC00).contains(&u)
            && i + 1 < units.len()
            && (0xDC00..0xE000).contains(&(units[i + 1] as u32))
        {
            i += 1;
            0x10000 + ((u - 0xD800) << 10) + (units[i] as u32 - 0xDC00)
        } else {
            u
        };
        push(&mut out, cp);
        i += 1;
    }
    out
}

/// The UTF-32 code points of UTF-8 text (a stray byte gives U+FFFD).
pub fn to_utf32(s: &[u8]) -> Vec<u32> {
    chars(s).map(|(_, cp, _)| if (0xD800..=0xDFFF).contains(&cp) { REPLACEMENT } else { cp }).collect()
}

/// The UTF-8 bytes of UTF-32 code points (a surrogate or an out-of-range value gives U+FFFD).
pub fn from_utf32(text: &[u32]) -> Vec<u8> {
    let mut out = Vec::with_capacity(text.len());
    for &cp in text {
        push(&mut out, cp);
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn decodes_every_length_and_refuses_the_malformed() {
        assert_eq!(decode(b"a"), (0x61, 1));
        assert_eq!(decode("é".as_bytes()), (0xE9, 2));
        assert_eq!(decode("€".as_bytes()), (0x20AC, 3));
        assert_eq!(decode("😀".as_bytes()), (0x1F600, 4));
        assert_eq!(decode(&[0x80]), (STRAY_BASE + 0x80, 1), "a stray continuation byte");
        assert_eq!(decode(&[0xC3]), (STRAY_BASE + 0xC3, 1), "a lead without its trail");
        assert_eq!(decode(&[0xC3, b'x']), (STRAY_BASE + 0xC3, 1), "a lead with the wrong trail");
        assert_eq!(decode(&[0xC0, 0x80]), (STRAY_BASE + 0xC0, 1), "an overlong NUL");
        assert_eq!(decode(&[0xE0, 0x80, 0x80]), (STRAY_BASE + 0xE0, 1), "an overlong 3-byte form");
        assert_eq!(decode(&[0xED, 0xA0, 0x80]), (STRAY_BASE + 0xED, 1), "a surrogate");
        assert_eq!(decode(&[0xF4, 0x90, 0x80, 0x80]), (STRAY_BASE + 0xF4, 1), "past U+10FFFF");
        assert_eq!(decode(&[0xF5]), (STRAY_BASE + 0xF5, 1));
        assert_eq!(decode(&[0xFF]), (STRAY_BASE + 0xFF, 1));
        assert_eq!(decode(&[0xE2, 0x82, 0]), (STRAY_BASE + 0xE2, 1), "a NUL is never a trail byte");
        assert!(is_valid("Café €".as_bytes()));
        assert!(!is_valid(&[b'a', 0xE9]));
    }

    #[test]
    fn encode_round_trips_and_escapes_strays() {
        for cp in [0u32, 0x41, 0x7F, 0x80, 0x7FF, 0x800, 0xFFFF, 0x10000, 0x10FFFF] {
            let mut b = [0u8; 4];
            let n = encode(cp, &mut b);
            assert_eq!(decode(&b[..n]), (cp, n), "{cp:#x}");
        }
        let mut b = [0u8; 4];
        assert_eq!(encode(STRAY_BASE + 0xE9, &mut b), 1);
        assert_eq!(b[0], 0xE9);
        assert_eq!(encode(0xD800, &mut b), 3);
        assert_eq!(decode(&b[..3]), (REPLACEMENT, 3));
        assert_eq!(encode(0x110000, &mut b), 3);
        assert_eq!(decode(&b[..3]), (REPLACEMENT, 3));
        // every byte string round-trips through decode/encode byte for byte
        let junk: Vec<u8> = (0..=255u8).chain([0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xF0, 0x9F]).collect();
        let mut back = Vec::new();
        for (_, cp, _) in chars(&junk) {
            push(&mut back, cp);
        }
        assert_eq!(back, junk);
    }

    #[test]
    fn stepping_counts_characters_not_bytes() {
        let s = "aé€😀\u{80}".as_bytes();
        assert_eq!(char_len(s), 5);
        assert_eq!(next(s, 0), 1);
        assert_eq!(next(s, 1), 3);
        assert_eq!(next(s, 3), 6);
        assert_eq!(next(s, 6), 10);
        assert_eq!(next(s, 10), 12);
        assert_eq!(next(s, 12), 12, "no step past the end");
        assert_eq!(prev(s, 12), 10);
        assert_eq!(prev(s, 10), 6);
        assert_eq!(prev(s, 6), 3);
        assert_eq!(prev(s, 3), 1);
        assert_eq!(prev(s, 1), 0);
        assert_eq!(bytes_of_chars(s, 2), 3);
        assert_eq!(bytes_of_chars(s, 99), s.len());
        assert_eq!(chars_in_bytes(s, 2), 2, "a character started inside the bytes counts");
        assert_eq!(chars_in_bytes(s, 3), 2);
        assert_eq!(chars_in_bytes(s, 99), 5);
        // stray bytes step one at a time, forwards and backwards
        let t = [b'a', 0x80, 0x80, 0xC3];
        assert_eq!(char_len(&t), 4);
        assert_eq!(prev(&t, 4), 3);
        assert_eq!(prev(&t, 3), 2);
        assert_eq!(prev(&t, 2), 1);
        // a continuation run longer than a sequence: only the byte before is taken
        let u = [0xE2, 0x82, 0xAC, 0x80];
        assert_eq!(prev(&u, 4), 3);
        assert_eq!(prev(&u, 3), 0);
        let v: Vec<(usize, u32, usize)> = chars("aé".as_bytes()).collect();
        assert_eq!(v, vec![(0, 0x61, 1), (1, 0xE9, 2)]);
    }

    #[test]
    fn folding_is_unicode_simple_lower_case() {
        assert_eq!(fold(b'A' as u32), b'a' as u32);
        assert_eq!(fold(0xC9), 0xE9, "É -> é");
        assert_eq!(fold(0x178), 0xFF, "Ÿ -> ÿ");
        assert_eq!(fold(0x391), 0x3B1, "Α -> α");
        assert_eq!(fold(0x410), 0x430, "А -> а");
        assert_eq!(fold(0x130), 0x130, "İ lower-cases to two characters: unchanged");
        assert_eq!(fold(0xDF), 0xDF, "ß has no lower-case mapping");
        assert_eq!(fold(b'_' as u32), b'_' as u32);
        assert_eq!(fold(STRAY_BASE + 0xC3), STRAY_BASE + 0xC3);
        assert_eq!(upper(0xE9), 0xC9);
        assert_eq!(upper(0xFF), 0x178);
        assert_eq!(upper(0xDF), 0xDF, "ß upper-cases to SS: unchanged");
        assert_eq!(upper(0x131), b'I' as u32, "ı -> I (shorter: map_in_place leaves it)");
    }

    #[test]
    fn folded_comparison_matches_stricmp_on_ascii_and_folds_latin() {
        assert_eq!(compare_folded(b"Hello", b"hELLO"), 0);
        assert!(compare_folded(b"abc", b"ABD") < 0);
        assert!(compare_folded(b"b", b"A") > 0);
        assert!(compare_folded(b"_", b"a") < 0, "lower-case folding: '_' before the letters");
        assert_eq!(compare_folded(b"", b"a"), -97);
        assert_eq!(compare_folded(b"ab", b"abc"), -99, "a prefix sorts first");
        assert_eq!(compare_folded("É".as_bytes(), "é".as_bytes()), 0);
        assert_eq!(compare_folded("ÉCOLE".as_bytes(), "école".as_bytes()), 0);
        assert!(compare_folded("é".as_bytes(), "z".as_bytes()) > 0, "by code point after folding");
        assert_eq!(compare_folded("ß".as_bytes(), "ss".as_bytes()), 0xDF - 0x73, "no full folding");
        assert_eq!(compare_folded_bytes(b"Data\\Units", b"DATA\\Other", 5), 0);
        assert!(compare_folded_bytes(b"Data\\Units", b"DATA\\Other", 6) > 0);
        assert_eq!(
            compare_folded_bytes("ÉA".as_bytes(), "éB".as_bytes(), 2),
            0,
            "a character started inside counts whole"
        );
        assert_eq!(compare_folded_chars("ÉA".as_bytes(), "éB".as_bytes(), 1), 0);
        assert!(compare_folded_chars("ÉA".as_bytes(), "éB".as_bytes(), 2) < 0);
        assert_eq!(compare(b"a", b"b"), -1);
        assert_eq!(compare("é".as_bytes(), "z".as_bytes()), 1, "byte order is code point order");
        assert_eq!(compare_chars("éa".as_bytes(), "éb".as_bytes(), 1), 0);
        assert_eq!(compare_chars("éa".as_bytes(), "éb".as_bytes(), 2), -1);
        // two different stray bytes are different characters
        assert_ne!(compare_folded(&[0xC3], &[0xC4]), 0);
    }

    #[test]
    fn case_mapping_in_place_keeps_byte_lengths() {
        let mut s = "Café ÉCOLE ß ı ſ Ÿ".as_bytes().to_vec();
        let before = s.len();
        map_in_place(&mut s, upper);
        assert_eq!(s.len(), before);
        assert_eq!(s, "CAFÉ ÉCOLE ß ı ſ Ÿ".as_bytes());
        map_in_place(&mut s, fold);
        assert_eq!(s, "café école ß ı ſ ÿ".as_bytes());
        let mut junk = vec![b'A', 0xC3, b'B', 0x80];
        map_in_place(&mut junk, fold);
        assert_eq!(junk, vec![b'a', 0xC3, b'b', 0x80], "stray bytes stay");
    }

    #[test]
    fn predicates_and_search() {
        assert!(is_space(0x20) && is_space(0x09) && is_space(0x0D) && !is_space(0xA0) && !is_space(b'x' as u32));
        assert!(is_digit(b'7' as u32) && !is_digit(b'a' as u32) && !is_digit(0x0663));
        assert!(!is_lead(b'A') && !is_lead(0x80) && is_lead(0xC3) && is_lead(0xF0) && !is_lead(0xFF));
        let s = "aé€\n".as_bytes();
        assert_eq!(find_char(s, 0x20AC), Some(3));
        assert_eq!(find_char(s, b'\n' as u32), Some(6));
        assert_eq!(find_char(s, 0x82), None, "a trail byte is not a character");
        assert_eq!(find_char(&[0xC3, b'a'], STRAY_BASE + 0xC3), Some(0));
    }

    #[test]
    fn cp1252_table_and_conversions() {
        assert_eq!(cp1252_to_codepoint(0x80), 0x20AC);
        assert_eq!(cp1252_to_codepoint(0x81), 0x81);
        assert_eq!(cp1252_to_codepoint(0x9F), 0x178);
        assert_eq!(cp1252_to_codepoint(0xE9), 0xE9);
        assert_eq!(codepoint_to_cp1252(0x20AC), Some(0x80));
        assert_eq!(codepoint_to_cp1252(0x81), Some(0x81));
        assert_eq!(codepoint_to_cp1252(0x2019), Some(0x92));
        assert_eq!(codepoint_to_cp1252(0x4E2D), None);
        for b in 0..=255u8 {
            assert_eq!(codepoint_to_cp1252(cp1252_to_codepoint(b)), Some(b), "{b:#x} round-trips");
        }
        assert_eq!(from_cp1252(b"caf\xe9 \x80 \x92"), "café € ’".as_bytes());
        assert_eq!(to_cp1252("café € ’ 中".as_bytes()), b"caf\xe9 \x80 \x92 ?");
        assert_eq!(to_cp1252(&[b'a', 0xC3]), b"a?");
        // 128 ASCII bytes, 128 two-byte characters, 17 of the 0x80..0x9F ones three bytes (U+2000 and up)
        assert_eq!(from_cp1252(&(0..=255u8).collect::<Vec<u8>>()).len(), 128 + 128 * 2 + 17);
    }

    #[test]
    fn utf16_and_utf32_conversions() {
        assert_eq!(to_utf16("aé€😀".as_bytes()), vec![0x61, 0xE9, 0x20AC, 0xD83D, 0xDE00]);
        assert_eq!(to_utf16(&[0xC3]), vec![0xFFFD], "a stray byte shows as U+FFFD");
        assert_eq!(from_utf16(&[0x61, 0xE9, 0x20AC, 0xD83D, 0xDE00]), "aé€😀".as_bytes());
        assert_eq!(from_utf16(&[0xDC00, 0x41]), "\u{FFFD}A".as_bytes(), "a lone surrogate");
        assert_eq!(from_utf16(&[0xD83D, 0x41]), "\u{FFFD}A".as_bytes());
        assert_eq!(to_utf32("a😀".as_bytes()), vec![0x61, 0x1F600]);
        assert_eq!(from_utf32(&[0x61, 0x1F600, 0x110000]), "a😀\u{FFFD}".as_bytes());
        assert_eq!(lossy(&[b'a', 0xC3, b'b']), "a\u{FFFD}b".as_bytes());
        assert_eq!(to_string_lossy(&[b'a', 0xFF]), "a\u{FFFD}");
        let text = "Wé😀\u{FFFF}";
        assert_eq!(from_utf16(&to_utf16(text.as_bytes())), text.as_bytes());
    }
}
