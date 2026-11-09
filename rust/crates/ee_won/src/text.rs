//! WON's text, the one place it is converted (docs/RUST_PLAN.md D6).
//!
//! On the wire and in WON's files a wide string is UTF-16 code units, little-endian, two bytes per unit. In memory
//! the C++ keeps `wchar_t`: one UTF-16 unit on Windows, one code point (UTF-32) on Linux and macOS. Units from the
//! wire need not be valid UTF-16 (a lone surrogate stays one unit), so they stay `u16` until something displays them;
//! `to_display` is that edge and the only lossy step.

/// The UTF-16 units of UTF-32 code points: a code point past U+FFFF becomes a surrogate pair; anything else
/// (including a value past U+10FFFF) is truncated to its low 16 bits, as `static_cast<uint16_t>` did.
pub fn utf16_from_utf32(text: &[u32]) -> Vec<u16> {
    let mut units = Vec::with_capacity(text.len());
    for &c in text {
        if c > 0xffff && c <= 0x10ffff {
            let v = c - 0x10000;
            units.push(0xd800 + (v >> 10) as u16);
            units.push(0xdc00 + (v & 0x3ff) as u16);
        } else {
            units.push(c as u16);
        }
    }
    units
}

/// Appends one UTF-16 unit to UTF-32 text: a low surrogate right after a high one joins it into one code point;
/// every other unit (a lone surrogate too) is one code point of its own.
pub fn push_utf16_unit(text: &mut Vec<u32>, unit: u16) {
    if (0xdc00..0xe000).contains(&unit) {
        if let Some(last) = text.last_mut() {
            if (0xd800..0xdc00).contains(last) {
                *last = 0x10000 + ((*last - 0xd800) << 10) + (u32::from(unit) - 0xdc00);
                return;
            }
        }
    }
    text.push(u32::from(unit));
}

/// UTF-32 text from UTF-16 units (`push_utf16_unit` for each).
pub fn utf32_from_utf16(units: &[u16]) -> Vec<u32> {
    let mut text = Vec::with_capacity(units.len());
    for &u in units {
        push_utf16_unit(&mut text, u);
    }
    text
}

/// The wire form of UTF-16 units: two bytes each, little-endian.
pub fn utf16_to_le_bytes(units: &[u16]) -> Vec<u8> {
    units.iter().flat_map(|u| u.to_le_bytes()).collect()
}

/// UTF-16 units from their wire form; a trailing odd byte is not a unit and is ignored.
pub fn utf16_from_le_bytes(bytes: &[u8]) -> Vec<u16> {
    bytes.as_chunks::<2>().0.iter().map(|&b| u16::from_le_bytes(b)).collect()
}

/// Text for a display: UTF-8, with every unpaired surrogate replaced by U+FFFD.
pub fn to_display(units: &[u16]) -> String {
    String::from_utf16_lossy(units)
}

/// UTF-16 units of UTF-8 text (text typed or read as UTF-8 going onto the wire).
pub fn from_utf8(text: &str) -> Vec<u16> {
    text.encode_utf16().collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn astral_code_points_become_surrogate_pairs() {
        assert_eq!(utf16_from_utf32(&[0x41, 0xe9, 0x20ac, 0x1f600]), vec![0x41, 0xe9, 0x20ac, 0xd83d, 0xde00]);
        assert_eq!(utf16_from_utf32(&[0x10ffff]), vec![0xdbff, 0xdfff]);
        // past the last code point: truncated, as the C++ cast did
        assert_eq!(utf16_from_utf32(&[0x110041]), vec![0x0041]);
    }

    #[test]
    fn surrogate_pairs_join_and_lone_ones_stay() {
        assert_eq!(utf32_from_utf16(&[0x41, 0xd83d, 0xde00]), vec![0x41, 0x1f600]);
        assert_eq!(utf32_from_utf16(&[0xdc00]), vec![0xdc00]);
        assert_eq!(utf32_from_utf16(&[0xd83d, 0x41]), vec![0xd83d, 0x41]);
        // a low surrogate after a joined pair does not join again
        assert_eq!(utf32_from_utf16(&[0xd83d, 0xde00, 0xde00]), vec![0x1f600, 0xde00]);
        let mut t = vec![0xd800];
        push_utf16_unit(&mut t, 0xdc00);
        assert_eq!(t, vec![0x10000]);
    }

    #[test]
    fn wire_bytes_are_little_endian_units() {
        assert_eq!(utf16_to_le_bytes(&[0x20ac, 0x41]), vec![0xac, 0x20, 0x41, 0x00]);
        assert_eq!(utf16_from_le_bytes(&[0xac, 0x20, 0x41, 0x00, 0x99]), vec![0x20ac, 0x41]);
        assert!(utf16_from_le_bytes(&[]).is_empty());
    }

    #[test]
    fn display_and_utf8_edges() {
        assert_eq!(to_display(&[0x48, 0xe9, 0xd83d, 0xde00]), "H\u{e9}\u{1f600}");
        assert_eq!(to_display(&[0xdc00, 0x41]), "\u{fffd}A");
        assert_eq!(from_utf8("\u{e9}\u{1f600}"), vec![0xe9, 0xd83d, 0xde00]);
    }

    #[test]
    fn round_trip_through_both_widths() {
        let text: Vec<u32> = vec![0x57, 0xe9, 0x1f600, 0xffff, 0];
        assert_eq!(utf32_from_utf16(&utf16_from_utf32(&text)), text);
    }
}
