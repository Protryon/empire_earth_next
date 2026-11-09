//! Windows-1252 <-> UTF-8 and <-> wide characters, shared by the window layer's text and the font's codepoints.
//! `PlatCp1252ToWide` / `PlatWideToCp1252` (platform/PlatString.h) are exported here; the UTF-8 pair is the C++
//! `ToCP1252` / `ToUTF8` of the window layer. Text keeps its byte form (docs/RUST_PLAN.md 2.3): nothing is `String`.

use std::ffi::{c_char, CStr};

/// The value that means "up to the terminator" (`PLAT_TEXT_TERMINATED`).
pub const PLAT_TEXT_TERMINATED: u32 = 0xFFFF_FFFF;

/// Windows-1252 0x80..0x9F (0: undefined there).
pub(crate) const CP1252: [u16; 32] = [
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0, 0,
    0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178,
];

/// The Unicode codepoint a byte is drawn as (the font's cmap lookup): undefined bytes become `?`.
pub(crate) fn codepoint_of(c: u8) -> i32 {
    if (0x80..0xA0).contains(&c) {
        let u = CP1252[(c - 0x80) as usize];
        if u != 0 {
            u as i32
        } else {
            b'?' as i32
        }
    } else {
        c as i32
    }
}

/// C wide characters.
#[cfg(windows)]
pub type WChar = u16;
#[cfg(not(windows))]
pub type WChar = i32;

/// A byte's wide character: the table, the undefined five as the C1 controls of the same value (as Windows does).
fn wide_of(byte: u8) -> u32 {
    if (0x80..0xA0).contains(&byte) && CP1252[(byte - 0x80) as usize] != 0 {
        CP1252[(byte - 0x80) as usize] as u32
    } else {
        byte as u32
    }
}

fn byte_of(character: u32) -> u8 {
    if character < 0x80 || (0xA0..=0xFF).contains(&character) {
        return character as u8;
    }
    for (i, &u) in CP1252.iter().enumerate() {
        let c = if u != 0 { u as u32 } else { 0x80 + i as u32 };
        if c == character {
            return 0x80 + i as u8;
        }
    }
    b'?'
}

/// `PlatCp1252ToWide` over slices: writes up to `out.len()` units and a terminator if it fits, -> the units.
pub fn cp1252_to_wide(text: &[u8], out: &mut [WChar]) -> u32 {
    let n = text.len().min(out.len());
    for i in 0..n {
        out[i] = wide_of(text[i]) as WChar;
    }
    if n < out.len() {
        out[n] = 0;
    }
    n as u32
}

/// `PlatWideToCp1252` over slices (`None`: count only).
pub fn wide_to_cp1252(text: &[WChar], mut out: Option<&mut [u8]>) -> u32 {
    let capacity = out.as_ref().map_or(0, |o| o.len()) as u32;
    let mut n = 0u32;
    let mut i = 0usize;
    while i < text.len() {
        let c = text[i] as u32;
        if (0xD800..0xDC00).contains(&c) && i + 1 < text.len() {
            let next = text[i + 1] as u32;
            if (0xDC00..0xE000).contains(&next) {
                i += 1;
            }
        }
        if let Some(o) = out.as_mut() {
            if n >= capacity {
                break;
            }
            o[n as usize] = byte_of(c);
        }
        n += 1;
        i += 1;
    }
    if let Some(o) = out.as_mut() {
        if n < capacity {
            o[n as usize] = 0;
        }
    }
    n
}

/// UTF-8 -> Windows-1252 into `out` (terminated; `out.len()` includes the terminator) -> the bytes written. A byte
/// sequence that is not UTF-8 gives `?`; characters outside Windows-1252 give `?`.
pub fn to_cp1252(utf8: &[u8], out: &mut [u8]) -> usize {
    let mut n = 0usize;
    let mut p = 0usize;
    let at = |i: usize| -> u8 { utf8.get(i).copied().unwrap_or(0) };
    while at(p) != 0 && n + 1 < out.len() {
        let b0 = at(p);
        let length: usize = if b0 < 0x80 {
            1
        } else if b0 >> 5 == 6 {
            2
        } else if b0 >> 4 == 14 {
            3
        } else if b0 >> 3 == 30 {
            4
        } else {
            1
        };
        let mut c: u32 = match length {
            1 => b0 as u32,
            2 => (b0 & 0x1F) as u32,
            3 => (b0 & 0x0F) as u32,
            _ => (b0 & 0x07) as u32,
        };
        let mut used = length;
        for i in 1..length {
            let bi = at(p + i);
            if bi & 0xC0 != 0x80 {
                used = i;
                c = b'?' as u32;
                break;
            }
            c = (c << 6) | (bi & 0x3F) as u32;
        }
        p += used;
        let mut b = b'?';
        if c < 0x80 || (0xA0..=0xFF).contains(&c) {
            b = c as u8;
        } else {
            for (i, &u) in CP1252.iter().enumerate() {
                if u as u32 == c {
                    b = 0x80 + i as u8;
                }
            }
        }
        out[n] = b;
        n += 1;
    }
    if !out.is_empty() {
        out[n] = 0;
    }
    n
}

/// Windows-1252 -> UTF-8, stopping at the first NUL (the C++ takes a C string).
pub fn to_utf8(text: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(text.len());
    for &b in text {
        if b == 0 {
            break;
        }
        let c = if (0x80..0xA0).contains(&b) {
            let u = CP1252[(b - 0x80) as usize];
            if u != 0 {
                u as u32
            } else {
                b'?' as u32
            }
        } else {
            b as u32
        };
        if c < 0x80 {
            out.push(c as u8);
        } else if c < 0x800 {
            out.push(0xC0 | (c >> 6) as u8);
            out.push(0x80 | (c & 0x3F) as u8);
        } else {
            out.push(0xE0 | (c >> 12) as u8);
            out.push(0x80 | ((c >> 6) & 0x3F) as u8);
            out.push(0x80 | (c & 0x3F) as u8);
        }
    }
    out
}

/// The bytes of a C string, empty for NULL.
///
/// # Safety
/// `text` is NULL or a valid NUL-terminated string.
pub unsafe fn c_bytes<'a>(text: *const c_char) -> &'a [u8] {
    if text.is_null() {
        &[]
    } else {
        CStr::from_ptr(text).to_bytes()
    }
}

/// `text` as a NUL-terminated buffer; a NUL inside ends the C string there.
pub fn c_terminated(text: &[u8]) -> Vec<u8> {
    let end = text.iter().position(|&b| b == 0).unwrap_or(text.len());
    let mut out = text[..end].to_vec();
    out.push(0);
    out
}

// ---- the C functions (platform/PlatString.h); `wchar_t` is 16 bits on Windows, 32 elsewhere

/// # Safety
/// `text` may be NULL; otherwise it holds `length` bytes (or is terminated for `PLAT_TEXT_TERMINATED`). `out`, when
/// not NULL, has `capacity` units.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatCp1252ToWide(text: *const c_char, length: u32, out: *mut WChar, capacity: u32) -> u32 {
    let bytes: &[u8] = if text.is_null() {
        &[]
    } else if length == PLAT_TEXT_TERMINATED {
        CStr::from_ptr(text).to_bytes()
    } else {
        std::slice::from_raw_parts(text as *const u8, length as usize)
    };
    if out.is_null() {
        return bytes.len() as u32;
    }
    cp1252_to_wide(bytes, std::slice::from_raw_parts_mut(out, capacity as usize))
}

/// # Safety
/// `text` may be NULL; otherwise it holds `length` units (or is terminated for `PLAT_TEXT_TERMINATED`). `out`, when
/// not NULL, has `capacity` bytes.
#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn PlatWideToCp1252(text: *const WChar, length: u32, out: *mut c_char, capacity: u32) -> u32 {
    let units: &[WChar] = if text.is_null() {
        &[]
    } else if length == PLAT_TEXT_TERMINATED {
        let mut n = 0usize;
        while *text.add(n) != 0 {
            n += 1;
        }
        std::slice::from_raw_parts(text, n)
    } else {
        std::slice::from_raw_parts(text, length as usize)
    };
    if out.is_null() {
        wide_to_cp1252(units, None)
    } else {
        wide_to_cp1252(units, Some(std::slice::from_raw_parts_mut(out as *mut u8, capacity as usize)))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn golden(prefix: &str) -> String {
        let text = include_str!("golden_font.txt");
        text.lines().find(|l| l.starts_with(prefix)).unwrap_or_else(|| panic!("no {prefix}")).to_string()
    }

    #[test]
    fn wide_of_every_byte_is_the_golden_table() {
        let line = golden("wide ");
        let values: Vec<u32> = line.split_whitespace().skip(1).map(|t| u32::from_str_radix(t, 16).unwrap()).collect();
        assert_eq!(values.len(), 256);
        for (i, &v) in values.iter().enumerate() {
            let mut one = [0 as WChar; 1];
            assert_eq!(cp1252_to_wide(&[i as u8], &mut one), 1);
            assert_eq!(one[0] as u32, v, "byte {i:#x}");
        }
    }

    #[test]
    fn narrow_golden_row() {
        let chars: [u32; 17] = [
            0x41, 0xE9, 0x20AC, 0x80, 0x81, 0x8D, 0x8F, 0x90, 0x9D, 0x7F, 0x100, 0x2122, 0xD83D, 0xDE00, 0xD83D, 0x78,
            0xFFFF,
        ];
        let w: Vec<WChar> = chars.iter().map(|&c| c as WChar).collect();
        let mut o = [0u8; 32];
        let n = wide_to_cp1252(&w, Some(&mut o)) as usize;
        let line = golden("narrow ");
        let values: Vec<u8> = line.split_whitespace().skip(2).map(|t| u8::from_str_radix(t, 16).unwrap()).collect();
        assert_eq!(o[..n].to_vec(), values);
        assert_eq!(o[n], 0);
        assert_eq!(wide_to_cp1252(&w, None) as usize, values.len());
        let count: u32 = golden("narrowcount ").split_whitespace().nth(1).unwrap().parse().unwrap();
        assert_eq!(wide_to_cp1252(&w, None), count);
    }

    #[test]
    fn narrow_stops_at_capacity_without_terminating() {
        let w: Vec<WChar> = [0x41u32, 0xE9, 0x20AC].iter().map(|&c| c as WChar).collect();
        let mut o = [0xEEu8; 3];
        let n = wide_to_cp1252(&w, Some(&mut o[..2]));
        let line = golden("narrow2 ");
        let parts: Vec<&str> = line.split_whitespace().collect();
        assert_eq!(n, parts[1].parse::<u32>().unwrap());
        assert_eq!(o[0], u8::from_str_radix(parts[2], 16).unwrap());
        assert_eq!(o[1], u8::from_str_radix(parts[3], 16).unwrap());
        assert_eq!(o[2], 0xEE); // no terminator past the capacity
    }

    #[test]
    fn to_wide_capacity_and_null_out_count() {
        let mut w = [0 as WChar; 2];
        assert_eq!(cp1252_to_wide(b"\x80zz", &mut w), 2);
        assert_eq!(w[0] as u32, 0x20AC);
        assert_eq!(w[1] as u32, b'z' as u32);
        let mut x = [99 as WChar; 3];
        assert_eq!(cp1252_to_wide(b"ab", &mut x), 2);
        assert_eq!(x[2] as u32, 0);
    }

    #[test]
    fn exports_handle_null_and_terminated() {
        unsafe {
            assert_eq!(PlatCp1252ToWide(c"abcd".as_ptr(), PLAT_TEXT_TERMINATED, std::ptr::null_mut(), 0), 4);
            let mut w = [0 as WChar; 8];
            assert_eq!(PlatCp1252ToWide(std::ptr::null(), PLAT_TEXT_TERMINATED, w.as_mut_ptr(), 8), 0);
            let wide: [WChar; 3] = [0x41, 0x20AC, 0];
            assert_eq!(PlatWideToCp1252(wide.as_ptr(), PLAT_TEXT_TERMINATED, std::ptr::null_mut(), 0), 2);
            let mut buf = [0 as c_char; 4];
            assert_eq!(PlatWideToCp1252(wide.as_ptr(), PLAT_TEXT_TERMINATED, buf.as_mut_ptr(), 4), 2);
            assert_eq!(buf[0] as u8, 0x41);
            assert_eq!(buf[1] as u8, 0x80);
            assert_eq!(buf[2], 0);
            assert_eq!(PlatWideToCp1252(std::ptr::null(), PLAT_TEXT_TERMINATED, buf.as_mut_ptr(), 4), 0);
        }
    }

    #[test]
    fn surrogate_pair_is_one_question_mark() {
        let w: Vec<WChar> = [0xD83Du32, 0xDE00, 0x41].iter().map(|&c| c as WChar).collect();
        let mut o = [0u8; 8];
        let n = wide_to_cp1252(&w, Some(&mut o));
        assert_eq!(&o[..n as usize], b"?A");
    }

    #[test]
    fn utf8_to_cp1252_table_and_replacement() {
        let mut o = [0u8; 16];
        assert_eq!(to_cp1252("é€\u{4e2d}".as_bytes(), &mut o), 3);
        assert_eq!(&o[..3], &[0xE9, 0x80, b'?']);
        assert_eq!(o[3], 0);
        // truncated and bad sequences: '?' and go on at the byte that broke it
        let n = to_cp1252(&[0xC3, b'x', 0x80, 0xC3], &mut o);
        assert_eq!(&o[..n], b"?x??");
        // the output limit holds the terminator
        let mut tiny = [9u8; 2];
        assert_eq!(to_cp1252(b"hello", &mut tiny), 1);
        assert_eq!(tiny, [b'h', 0]);
    }

    fn unhex(s: &str) -> Vec<u8> {
        let h = s.strip_prefix('x').unwrap_or(s);
        (0..h.len()).step_by(2).map(|i| u8::from_str_radix(&h[i..i + 2], 16).unwrap()).collect()
    }

    #[test]
    fn utf8_conversions_match_the_cpp_golden_rows() {
        let golden = include_str!("golden_window.txt");
        let mut rows = 0;
        for l in golden.lines() {
            let p: Vec<&str> = l.split_whitespace().collect();
            match p[0] {
                "u8" => {
                    let input = unhex(p[1]);
                    let mut o = [0u8; 64];
                    let n = to_cp1252(&input, &mut o);
                    assert_eq!(n, p[2].parse::<usize>().unwrap(), "{l}");
                    assert_eq!(o[..n].to_vec(), unhex(p[3]), "{l}");
                    rows += 1;
                }
                "cp" => {
                    assert_eq!(to_utf8(&unhex(p[1])), unhex(p[2]), "{l}");
                    rows += 1;
                }
                "u8tiny" => {
                    let mut tiny = [0xEEu8; 3];
                    assert_eq!(to_cp1252(b"abcdef", &mut tiny), p[1].parse::<usize>().unwrap());
                    assert_eq!(tiny[0], u8::from_str_radix(p[2], 16).unwrap());
                    assert_eq!(tiny[1], u8::from_str_radix(p[3], 16).unwrap());
                    assert_eq!(tiny[2], u8::from_str_radix(p[4], 16).unwrap());
                    rows += 1;
                }
                _ => {}
            }
        }
        assert!(rows >= 12);
    }

    #[test]
    fn cp1252_to_utf8_with_undefined_bytes_as_question() {
        assert_eq!(to_utf8(b"A\x80\x81\xE9"), "A€?é".as_bytes());
        assert_eq!(to_utf8(b"ab\0cd"), b"ab");
    }

    #[test]
    fn codepoint_of_undefined_is_question() {
        assert_eq!(codepoint_of(0x80), 0x20AC);
        assert_eq!(codepoint_of(0x81), b'?' as i32);
        assert_eq!(codepoint_of(b'A'), 65);
        assert_eq!(codepoint_of(0xE9), 0xE9);
    }

    #[test]
    fn c_helpers() {
        assert_eq!(unsafe { c_bytes(std::ptr::null()) }, b"");
        assert_eq!(unsafe { c_bytes(c"hi".as_ptr()) }, b"hi");
        assert_eq!(c_terminated(b"ab\0cd"), b"ab\0".to_vec());
    }
}
