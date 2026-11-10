//! `platform/PlatString.h`: the byte-string and multibyte string functions the game's text is written on, over UTF-8
//! (`utf8.rs`, decision D6) on every system: one implementation, no C runtime locale or code page behind it. The
//! `PlatMbs*` names keep the MSVC `_mbs*` contracts (a "character" is one code point, a stray byte one character of
//! its own); the case-insensitive compares fold like the UCRT's `_stricmp`/`_mbsicmp` did for ASCII (to lower case:
//! `'_'` sorts before the letters) and fold the rest of Unicode's simple mapping with it.

use std::ffi::{c_char, c_int, c_uint, CStr};
use std::ptr::{null_mut, write_bytes};

use crate::utf8;

/// `wchar_t`: UTF-16 units on Windows, UTF-32 elsewhere.
#[cfg(windows)]
pub type WChar = u16;
#[cfg(not(windows))]
pub type WChar = i32;

/// The value that means "up to the terminator" (`PLAT_TEXT_TERMINATED`).
pub const PLAT_TEXT_TERMINATED: u32 = 0xFFFF_FFFF;

/// The bytes of a NUL-terminated text, without the terminator.
unsafe fn bytes<'a>(s: *const u8) -> &'a [u8] {
    CStr::from_ptr(s.cast()).to_bytes()
}

/// The character at `p` and its byte length, reading no further than the terminator (0, 0 at it).
unsafe fn decode_at(p: *const u8) -> (u32, usize) {
    let b0 = *p;
    if b0 == 0 {
        return (0, 0);
    }
    let n = utf8::lead_len(b0);
    if n <= 1 {
        return utf8::decode(std::slice::from_raw_parts(p, 1));
    }
    // a NUL is never a trail byte: reading one byte at a time up to the first that cannot continue stays in bounds
    let mut buf = [0u8; 4];
    buf[0] = b0;
    let mut have = 1;
    while have < n {
        let b = *p.add(have);
        if b & 0xC0 != 0x80 {
            break;
        }
        buf[have] = b;
        have += 1;
    }
    utf8::decode(&buf[..have])
}

/// The bytes of the character at `p` (1 at a stray byte; 1 at the terminator too, as `_mbsinc` stepped).
unsafe fn char_bytes_at(p: *const u8) -> usize {
    decode_at(p).1.max(1)
}

fn sign(v: i32) -> c_int {
    v.signum()
}

// ---- byte strings

#[no_mangle]
pub unsafe extern "C" fn PlatStrICmp(left: *const c_char, right: *const c_char) -> c_int {
    utf8::compare_folded(bytes(left.cast()), bytes(right.cast()))
}

#[no_mangle]
pub unsafe extern "C" fn PlatStrNICmp(left: *const c_char, right: *const c_char, count: usize) -> c_int {
    utf8::compare_folded_bytes(bytes(left.cast()), bytes(right.cast()), count)
}

/// MSVC `_itoa`: a minus sign only in radix 10; other radices format the value's 32-bit two's complement.
#[no_mangle]
pub unsafe extern "C" fn PlatIToA(value: c_int, buffer: *mut c_char, radix: c_int) -> *mut c_char {
    if !(2..=36).contains(&radix) {
        *buffer = 0;
        return buffer;
    }
    let negative = radix == 10 && value < 0;
    let mut magnitude = if negative { (value as u32).wrapping_neg() } else { value as u32 };
    let radix = radix as u32;
    let mut digits = [0u8; 33];
    let mut n = 0;
    loop {
        let d = (magnitude % radix) as u8;
        digits[n] = if d < 10 { b'0' + d } else { b'a' + d - 10 };
        n += 1;
        magnitude /= radix;
        if magnitude == 0 {
            break;
        }
    }
    let mut p = buffer;
    if negative {
        *p = b'-' as c_char;
        p = p.add(1);
    }
    while n > 0 {
        n -= 1;
        *p = digits[n] as c_char;
        p = p.add(1);
    }
    *p = 0;
    buffer
}

#[no_mangle]
pub unsafe extern "C" fn PlatStrDup(text: *const c_char) -> *mut c_char {
    if text.is_null() {
        return null_mut();
    }
    // a malloc'ed copy the caller frees with free()
    let n = bytes(text.cast()).len() + 1;
    let copy = libc::malloc(n).cast::<c_char>();
    if !copy.is_null() {
        std::ptr::copy_nonoverlapping(text, copy, n);
    }
    copy
}

// ---- multibyte strings (UTF-8)

#[no_mangle]
pub unsafe extern "C" fn PlatMbsLen(s: *const u8) -> usize {
    utf8::char_len(bytes(s))
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsNBCnt(s: *const u8, characters: usize) -> usize {
    utf8::bytes_of_chars(bytes(s), characters)
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsNCCnt(s: *const u8, count: usize) -> usize {
    utf8::chars_in_bytes(bytes(s), count)
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsInc(current: *const u8) -> *mut u8 {
    current.add(char_bytes_at(current)).cast_mut()
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsNInc(current: *const u8, characters: usize) -> *mut u8 {
    if current.is_null() {
        return null_mut();
    }
    let mut p = current;
    for _ in 0..characters {
        if *p == 0 {
            break;
        }
        p = p.add(char_bytes_at(p));
    }
    p.cast_mut()
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsDec(start: *const u8, current: *const u8) -> *mut u8 {
    if current <= start {
        return null_mut();
    }
    let at = current.offset_from(start) as usize;
    let s = std::slice::from_raw_parts(start, at);
    start.add(utf8::prev(s, at)).cast_mut()
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsCmp(left: *const u8, right: *const u8) -> c_int {
    sign(utf8::compare(bytes(left), bytes(right)))
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsICmp(left: *const u8, right: *const u8) -> c_int {
    utf8::compare_folded(bytes(left), bytes(right))
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsNCmp(left: *const u8, right: *const u8, characters: usize) -> c_int {
    sign(utf8::compare_chars(bytes(left), bytes(right), characters))
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsNBCmp(left: *const u8, right: *const u8, count: usize) -> c_int {
    let (l, r) = (bytes(left), bytes(right));
    sign(utf8::compare(&l[..count.min(l.len())], &r[..count.min(r.len())]))
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsNBICmp(left: *const u8, right: *const u8, count: usize) -> c_int {
    utf8::compare_folded_bytes(bytes(left), bytes(right), count)
}

/// `_mbsncpy`: the first `characters` characters of `source`; a shorter source is padded with zeros to `characters`
/// bytes; no terminator otherwise.
#[no_mangle]
pub unsafe extern "C" fn PlatMbsNCpy(dest: *mut u8, source: *const u8, characters: usize) -> *mut u8 {
    let src = bytes(source);
    let n = utf8::bytes_of_chars(src, characters);
    std::ptr::copy_nonoverlapping(src.as_ptr(), dest, n);
    if n < characters {
        write_bytes(dest.add(n), 0, characters - n);
    }
    dest
}

/// `_mbsnbcat`: appends at most `count` bytes of `source`, never a partial character, and terminates.
#[no_mangle]
pub unsafe extern "C" fn PlatMbsNBCat(dest: *mut u8, source: *const u8, count: usize) -> *mut u8 {
    let src = bytes(source);
    let mut n = 0;
    while n < src.len() {
        let step = utf8::next(src, n) - n;
        if n + step > count {
            break;
        }
        n += step;
    }
    let p = dest.add(bytes(dest).len());
    std::ptr::copy_nonoverlapping(src.as_ptr(), p, n);
    *p.add(n) = 0;
    dest
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsChr(s: *const u8, character: c_uint) -> *mut u8 {
    let text = bytes(s);
    if character == 0 {
        return s.add(text.len()).cast_mut();
    }
    match utf8::find_char(text, character) {
        Some(at) => s.add(at).cast_mut(),
        None => null_mut(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsLwr(s: *mut u8) -> *mut u8 {
    let n = bytes(s).len();
    utf8::map_in_place(std::slice::from_raw_parts_mut(s, n), utf8::fold);
    s
}

#[no_mangle]
pub unsafe extern "C" fn PlatMbsUpr(s: *mut u8) -> *mut u8 {
    let n = bytes(s).len();
    utf8::map_in_place(std::slice::from_raw_parts_mut(s, n), utf8::upper);
    s
}

#[no_mangle]
pub extern "C" fn PlatIsMbcSpace(character: c_uint) -> c_int {
    c_int::from(utf8::is_space(character))
}

#[no_mangle]
pub extern "C" fn PlatIsMbcDigit(character: c_uint) -> c_int {
    c_int::from(utf8::is_digit(character))
}

#[no_mangle]
pub extern "C" fn PlatIsMbbLead(byte: c_uint) -> c_int {
    c_int::from(byte <= 0xFF && utf8::is_lead(byte as u8))
}

// ---- code points

/// The character at `text` (0 at the terminator or for NULL); `out_length` (may be NULL) gets its bytes (0 there).
#[no_mangle]
pub unsafe extern "C" fn PlatUtf8Decode(text: *const c_char, out_length: *mut u32) -> u32 {
    let (cp, n) = if text.is_null() { (0, 0) } else { decode_at(text.cast()) };
    if !out_length.is_null() {
        *out_length = n as u32;
    }
    cp
}

/// A character's bytes into `out` (room for 4, no terminator) -> their count.
#[no_mangle]
pub unsafe extern "C" fn PlatUtf8Encode(character: u32, out: *mut c_char) -> u32 {
    let mut b = [0u8; 4];
    let n = utf8::encode(character, &mut b);
    std::ptr::copy_nonoverlapping(b.as_ptr(), out.cast(), n);
    n as u32
}

/// The Windows-1252 byte of a character, -1 when the code page has none.
#[no_mangle]
pub extern "C" fn PlatCp1252ByteOf(character: u32) -> c_int {
    utf8::codepoint_to_cp1252(character).map_or(-1, c_int::from)
}

/// The text of `length` units at `text` (`PLAT_TEXT_TERMINATED`: up to the terminator), empty for NULL.
unsafe fn counted<'a, T: Copy + Default + PartialEq>(text: *const T, length: u32) -> &'a [T] {
    if text.is_null() {
        return &[];
    }
    if length == PLAT_TEXT_TERMINATED {
        let mut n = 0usize;
        while *text.add(n) != T::default() {
            n += 1;
        }
        std::slice::from_raw_parts(text, n)
    } else {
        std::slice::from_raw_parts(text, length as usize)
    }
}

/// Copies `units` into `out` (at most `capacity`), terminating when room is left -> the units written; NULL `out`:
/// the units the text needs.
unsafe fn give<T: Copy + Default>(units: &[T], out: *mut T, capacity: u32) -> u32 {
    if out.is_null() {
        return units.len() as u32;
    }
    let n = units.len().min(capacity as usize);
    std::ptr::copy_nonoverlapping(units.as_ptr(), out, n);
    if n < capacity as usize {
        *out.add(n) = T::default();
    }
    n as u32
}

/// UTF-8 -> wide characters (`PlatUtf8ToWide`): a stray byte becomes U+FFFD.
#[no_mangle]
pub unsafe extern "C" fn PlatUtf8ToWide(text: *const c_char, length: u32, out: *mut WChar, capacity: u32) -> u32 {
    let s = counted(text.cast::<u8>(), length);
    #[cfg(windows)]
    let units: Vec<WChar> = utf8::to_utf16(s);
    #[cfg(not(windows))]
    let units: Vec<WChar> = utf8::to_utf32(s).into_iter().map(|c| c as WChar).collect();
    give(&units, out, capacity)
}

/// Wide characters -> UTF-8 (`PlatWideToUtf8`): a lone surrogate (or an out-of-range value) becomes U+FFFD.
#[no_mangle]
pub unsafe extern "C" fn PlatWideToUtf8(text: *const WChar, length: u32, out: *mut c_char, capacity: u32) -> u32 {
    let w = counted(text, length);
    #[cfg(windows)]
    let bytes = utf8::from_utf16(w);
    #[cfg(not(windows))]
    let bytes = utf8::from_utf32(&w.iter().map(|&c| c as u32).collect::<Vec<u32>>());
    give(&bytes, out.cast::<u8>(), capacity)
}

/// Windows-1252 (an old input) -> UTF-8 (`PlatCp1252ToUtf8`).
#[no_mangle]
pub unsafe extern "C" fn PlatCp1252ToUtf8(text: *const c_char, length: u32, out: *mut c_char, capacity: u32) -> u32 {
    let s = counted(text.cast::<u8>(), length);
    give(&utf8::from_cp1252(s), out.cast::<u8>(), capacity)
}

/// Whether `length` bytes of `text` (`PLAT_TEXT_TERMINATED`: up to the terminator) are well-formed UTF-8
/// (`PlatUtf8IsValid`): what tells an old Windows-1252 input from the game's text where no version says.
#[no_mangle]
pub unsafe extern "C" fn PlatUtf8IsValid(text: *const c_char, length: u32) -> c_int {
    c_int::from(utf8::is_valid(counted(text.cast::<u8>(), length)))
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;
    use std::ptr::null;

    fn c(text: &[u8]) -> Vec<u8> {
        crate::c_text::terminated(text)
    }

    #[test]
    fn c_api_contract() {
        // the checks of tests/platform/plat_test.cpp "strings"
        unsafe {
            assert_eq!(PlatStrICmp(c"Hello".as_ptr(), c"hELLO".as_ptr()), 0);
            assert!(PlatStrICmp(c"abc".as_ptr(), c"ABD".as_ptr()) < 0);
            assert!(PlatStrICmp(c"b".as_ptr(), c"A".as_ptr()) > 0);
            assert!(PlatStrICmp(c"_".as_ptr(), c"a".as_ptr()) < 0);
            assert_eq!(PlatStrICmp(c"École".as_ptr(), c"école".as_ptr()), 0);
            assert_eq!(PlatStrNICmp(c"Data\\Units".as_ptr(), c"DATA\\Other".as_ptr(), 5), 0);
            assert!(PlatStrNICmp(c"Data\\Units".as_ptr(), c"DATA\\Other".as_ptr(), 6) > 0);
            let mut buffer = [0 as c_char; 40];
            let text = |p: *mut c_char| CStr::from_ptr(p).to_bytes().to_vec();
            assert_eq!(text(PlatIToA(-123, buffer.as_mut_ptr(), 10)), b"-123");
            assert_eq!(text(PlatIToA(255, buffer.as_mut_ptr(), 16)), b"ff");
            assert_eq!(text(PlatIToA(-1, buffer.as_mut_ptr(), 16)), b"ffffffff");
            assert_eq!(text(PlatIToA(5, buffer.as_mut_ptr(), 2)), b"101");
            assert_eq!(text(PlatIToA(5, buffer.as_mut_ptr(), 1)), b"");
            let copy = PlatStrDup(c"copy".as_ptr());
            assert_eq!(text(copy), b"copy");
            libc::free(copy.cast());
            assert!(PlatStrDup(null()).is_null());

            let s = c"Empire Earth".as_ptr().cast::<u8>();
            assert_eq!(PlatMbsLen(s), 12);
            assert_eq!(PlatMbsNBCnt(s, 6), 6);
            assert_eq!(PlatMbsNBCnt(s, 100), 12);
            assert_eq!(PlatMbsNCCnt(s, 3), 3);
            assert_eq!(PlatMbsInc(s), s.add(1).cast_mut());
            assert_eq!(PlatMbsNInc(s, 7), s.add(7).cast_mut());
            assert_eq!(PlatMbsNInc(s, 99), s.add(12).cast_mut());
            assert_eq!(PlatMbsDec(s, s.add(3)), s.add(2).cast_mut());
            assert!(PlatMbsDec(s, s).is_null());
            assert!(PlatMbsNInc(null(), 3).is_null());
            let u = c"EMPIRE EARTH".as_ptr().cast::<u8>();
            assert!(PlatMbsCmp(s, u) > 0);
            assert_eq!(PlatMbsICmp(s, u), 0);
            assert_eq!(PlatMbsNCmp(s, u, 1), 0);
            assert!(PlatMbsNBCmp(s, u, 2) > 0);
            assert_eq!(PlatMbsNBICmp(s, u, 12), 0);

            let mut text_buf = [b'x'; 32];
            PlatMbsNCpy(text_buf.as_mut_ptr(), s, 20);
            assert_eq!(&text_buf[..21], b"Empire Earth\0\0\0\0\0\0\0\0x");
            PlatMbsNBCat(text_buf.as_mut_ptr(), c": AoC!".as_ptr().cast::<u8>(), 5);
            assert_eq!(&text_buf[..18], b"Empire Earth: AoC\0");
            assert_eq!(PlatMbsChr(text_buf.as_ptr(), b'E' as u32), text_buf.as_mut_ptr());
            assert_eq!(PlatMbsChr(text_buf.as_ptr(), b':' as u32), text_buf.as_mut_ptr().add(12));
            assert!(PlatMbsChr(text_buf.as_ptr(), b'z' as u32).is_null());
            assert_eq!(PlatMbsChr(text_buf.as_ptr(), 0), text_buf.as_mut_ptr().add(17), "the terminator is found");
            PlatMbsUpr(text_buf.as_mut_ptr());
            assert_eq!(&text_buf[..18], b"EMPIRE EARTH: AOC\0");
            PlatMbsLwr(text_buf.as_mut_ptr());
            assert_eq!(&text_buf[..18], b"empire earth: aoc\0");
            assert_ne!(PlatIsMbcSpace(b' ' as u32), 0);
            assert_ne!(PlatIsMbcSpace(b'\t' as u32), 0);
            assert_eq!(PlatIsMbcSpace(b'x' as u32), 0);
            assert_ne!(PlatIsMbcDigit(b'7' as u32), 0);
            assert_eq!(PlatIsMbcDigit(b'a' as u32), 0);
            assert_eq!(PlatIsMbbLead(b'A' as u32), 0);
            assert_ne!(PlatIsMbbLead(0xC3), 0);
            assert_eq!(PlatIsMbbLead(0x1C3), 0);
        }
    }

    #[test]
    fn multi_byte_characters_through_the_c_api() {
        unsafe {
            let t = c("aé€😀".as_bytes());
            let s = t.as_ptr();
            assert_eq!(PlatMbsLen(s), 4);
            assert_eq!(PlatMbsNBCnt(s, 2), 3);
            assert_eq!(PlatMbsNBCnt(s, 3), 6);
            assert_eq!(PlatMbsNCCnt(s, 2), 2);
            assert_eq!(PlatMbsNCCnt(s, 4), 3);
            assert_eq!(PlatMbsInc(s.add(1)), s.add(3).cast_mut());
            assert_eq!(PlatMbsInc(s.add(10)), s.add(11).cast_mut(), "at the terminator: one on, as _mbsinc");
            assert_eq!(PlatMbsNInc(s, 3), s.add(6).cast_mut());
            assert_eq!(PlatMbsDec(s, s.add(10)), s.add(6).cast_mut());
            assert_eq!(PlatMbsDec(s, s.add(6)), s.add(3).cast_mut());
            assert_eq!(PlatMbsDec(s, s.add(3)), s.add(1).cast_mut());
            assert_eq!(PlatMbsDec(s, s.add(2)), s.add(1).cast_mut(), "from inside a sequence: its start");
            let u = c("AÉ€😀".as_bytes());
            assert_eq!(PlatMbsICmp(s, u.as_ptr()), 0);
            assert_eq!(PlatMbsNBICmp(s, u.as_ptr(), 2), 0, "a character started inside the bytes counts whole");
            assert_eq!(PlatMbsNCmp(s, u.as_ptr(), 1), 1);
            assert_eq!(PlatMbsNBCmp(s, c(&[b'a', 0xC3]).as_ptr(), 2), 0);
            assert_eq!(PlatMbsChr(s, 0x20AC), s.add(3).cast_mut());
            assert_eq!(PlatMbsChr(s, 0x1F600), s.add(6).cast_mut());
            assert!(PlatMbsChr(s, 0x82).is_null(), "a trail byte is not a character");

            let mut buf = [0xEEu8; 16];
            PlatMbsNCpy(buf.as_mut_ptr(), s, 2);
            assert_eq!(&buf[..4], &[b'a', 0xC3, 0xA9, 0xEE], "two characters, three bytes, no padding");
            PlatMbsNCpy(buf.as_mut_ptr(), c"a".as_ptr().cast(), 3);
            assert_eq!(&buf[..4], &[b'a', 0, 0, 0xEE], "padded to the count in bytes");
            let mut cat = [0u8; 16];
            PlatMbsNBCat(cat.as_mut_ptr(), s, 4);
            assert_eq!(&cat[..4], &[b'a', 0xC3, 0xA9, 0], "4 bytes would split the euro sign: left out");
            PlatMbsNBCat(cat.as_mut_ptr(), s.add(3), 3);
            assert_eq!(&cat[..7], "aé€\0".as_bytes());

            let mut case = c("Café École ß".as_bytes());
            PlatMbsUpr(case.as_mut_ptr());
            assert_eq!(&case[..case.len() - 1], "CAFÉ ÉCOLE ß".as_bytes());
            PlatMbsLwr(case.as_mut_ptr());
            assert_eq!(&case[..case.len() - 1], "café école ß".as_bytes());
            // stray bytes: one character each, never stepped past the terminator
            let junk = c(&[0xC3, b'x', 0xE2, 0x82]);
            let j = junk.as_ptr();
            assert_eq!(PlatMbsLen(j), 4);
            assert_eq!(PlatMbsInc(j), j.add(1).cast_mut());
            assert_eq!(PlatMbsNInc(j, 9), j.add(4).cast_mut());
            assert_eq!(PlatMbsDec(j, j.add(4)), j.add(3).cast_mut());
            assert_ne!(PlatMbsICmp(j, c(&[0xC4, b'x', 0xE2, 0x82]).as_ptr()), 0);
        }
    }

    #[test]
    fn code_point_and_wide_exports() {
        unsafe {
            let mut n = 0u32;
            assert_eq!(PlatUtf8Decode(c"€x".as_ptr(), &mut n), 0x20AC);
            assert_eq!(n, 3);
            assert_eq!(PlatUtf8Decode(c"".as_ptr(), &mut n), 0);
            assert_eq!(n, 0);
            assert_eq!(PlatUtf8Decode(null(), null_mut()), 0);
            let stray = c(&[0xE2, 0x82]);
            assert_eq!(PlatUtf8Decode(stray.as_ptr().cast(), &mut n), utf8::STRAY_BASE + 0xE2);
            assert_eq!(n, 1);
            let mut b = [0 as c_char; 4];
            assert_eq!(PlatUtf8Encode(0x1F600, b.as_mut_ptr()), 4);
            assert_eq!(b.map(|x| x as u8), [0xF0, 0x9F, 0x98, 0x80]);
            assert_eq!(PlatCp1252ByteOf(0x2019), 0x92);
            assert_eq!(PlatCp1252ByteOf(0x4E2D), -1);

            let mut w = [0 as WChar; 8];
            assert_eq!(PlatUtf8ToWide(c"aé€".as_ptr(), PLAT_TEXT_TERMINATED, w.as_mut_ptr(), 8), 3);
            assert_eq!([w[0] as u32, w[1] as u32, w[2] as u32, w[3] as u32], [0x61, 0xE9, 0x20AC, 0]);
            assert_eq!(PlatUtf8ToWide(c"abcd".as_ptr(), PLAT_TEXT_TERMINATED, null_mut(), 0), 4);
            assert_eq!(PlatUtf8ToWide(c"abcdef".as_ptr(), 6, w.as_mut_ptr(), 3), 3, "no room for a terminator");
            assert_eq!(w[2] as u32, b'c' as u32);
            assert_eq!(PlatUtf8ToWide(null(), PLAT_TEXT_TERMINATED, w.as_mut_ptr(), 8), 0);
            assert_eq!(w[0], 0);
            let mut back = [0 as c_char; 16];
            let wide: [WChar; 4] = [0x41, 0x20AC, 0xE9, 0];
            assert_eq!(PlatWideToUtf8(wide.as_ptr(), PLAT_TEXT_TERMINATED, back.as_mut_ptr(), 16), 6);
            assert_eq!(CStr::from_ptr(back.as_ptr()).to_bytes(), "A€é".as_bytes());
            assert_eq!(PlatWideToUtf8(wide.as_ptr(), 3, null_mut(), 0), 6);
            assert_eq!(PlatWideToUtf8(null(), PLAT_TEXT_TERMINATED, back.as_mut_ptr(), 16), 0);
            // a counted text converts its embedded zeros
            let zeros: [WChar; 3] = [0x41, 0, 0x42];
            assert_eq!(PlatWideToUtf8(zeros.as_ptr(), 3, back.as_mut_ptr(), 16), 3);
            assert_eq!(back[..3].iter().map(|&x| x as u8).collect::<Vec<u8>>(), b"A\0B");
            #[cfg(windows)]
            {
                let pair: [WChar; 3] = [0xD83D, 0xDE00, 0];
                assert_eq!(PlatWideToUtf8(pair.as_ptr(), PLAT_TEXT_TERMINATED, back.as_mut_ptr(), 16), 4);
                let lone: [WChar; 2] = [0xDC00, 0];
                assert_eq!(PlatWideToUtf8(lone.as_ptr(), PLAT_TEXT_TERMINATED, back.as_mut_ptr(), 16), 3);
            }
            assert_eq!(PlatCp1252ToUtf8(c"caf\xe9 \x80".as_ptr(), PLAT_TEXT_TERMINATED, back.as_mut_ptr(), 16), 9);
            assert_eq!(CStr::from_ptr(back.as_ptr()).to_bytes(), "café €".as_bytes());
            assert_eq!(PlatCp1252ToUtf8(c"caf\xe9".as_ptr(), 4, null_mut(), 0), 5);
            assert_eq!(PlatUtf8IsValid(c"caf\xc3\xa9".as_ptr(), PLAT_TEXT_TERMINATED), 1);
            assert_eq!(PlatUtf8IsValid(c"caf\xe9".as_ptr(), PLAT_TEXT_TERMINATED), 0);
            assert_eq!(PlatUtf8IsValid(c"caf\xe9".as_ptr(), 3), 1);
            assert_eq!(PlatUtf8IsValid(null(), PLAT_TEXT_TERMINATED), 1, "nothing is well-formed");
        }
    }

    /// The ASCII rows of the golden file captured from the Win32 C++ under code page 1252 (PORT_LEARNINGS "The
    /// platform leaves in Rust"): what the UCRT answered for ASCII is what the UTF-8 implementation answers.
    mod golden {
        use super::*;

        fn parse_hex(text: &str) -> Vec<u8> {
            (0..text.len() / 2).map(|i| u8::from_str_radix(&text[i * 2..i * 2 + 2], 16).unwrap()).collect()
        }

        fn table(data: &str) -> HashMap<String, Vec<Vec<String>>> {
            let mut out: HashMap<String, Vec<Vec<String>>> = HashMap::new();
            for line in data.lines() {
                let mut fields: Vec<String> = line.split(' ').map(str::to_owned).collect();
                if fields.len() < 2 {
                    continue;
                }
                let section = fields.remove(0);
                out.entry(section).or_default().push(fields);
            }
            out
        }

        /// Runs a function over the rows of one section whose byte inputs are all ASCII.
        unsafe fn check_section<F: Fn(&[Vec<u8>], &[String]) -> String>(
            rows: &HashMap<String, Vec<Vec<String>>>,
            name: &str,
            arguments: usize,
            function: F,
        ) -> usize {
            let rows = rows.get(name).unwrap_or_else(|| panic!("no golden section {name}"));
            let mut n = 0;
            for row in rows {
                let inputs: Vec<Vec<u8>> = row[..arguments].iter().map(|t| parse_hex(t)).collect();
                if inputs.iter().any(|i| !i.is_ascii()) {
                    continue;
                }
                let expected = row[row.len() - 1].clone();
                assert_eq!(function(&inputs, row), expected, "{name} {row:?}");
                n += 1;
            }
            n
        }

        unsafe fn offset_of(buffer: &[u8], p: *const u8) -> String {
            if p.is_null() {
                "null".to_string()
            } else {
                ((p as usize) - (buffer.as_ptr() as usize)).to_string()
            }
        }

        fn digits(row: &[String], index: usize) -> usize {
            row[index].parse().unwrap()
        }

        fn hex(data: &[u8]) -> String {
            data.iter().map(|b| format!("{b:02x}")).collect()
        }

        #[test]
        fn ascii_rows_of_the_win32_goldens() {
            let t = table(include_str!("golden_crt_1252.txt"));
            let mut n = 0;
            unsafe {
                n += check_section(&t, "stricmp", 2, |i, _| {
                    PlatStrICmp(c(&i[0]).as_ptr().cast(), c(&i[1]).as_ptr().cast()).to_string()
                });
                n += check_section(&t, "strnicmp", 2, |i, r| {
                    PlatStrNICmp(c(&i[0]).as_ptr().cast(), c(&i[1]).as_ptr().cast(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "itoa", 0, |_, r| {
                    let mut buffer = [0 as c_char; 64];
                    let out = PlatIToA(r[0].parse().unwrap(), buffer.as_mut_ptr(), r[1].parse().unwrap());
                    String::from_utf8(CStr::from_ptr(out).to_bytes().to_vec()).unwrap()
                });
                n += check_section(&t, "mbslen", 1, |i, _| PlatMbsLen(c(&i[0]).as_ptr()).to_string());
                n += check_section(&t, "mbsnbcnt", 1, |i, r| PlatMbsNBCnt(c(&i[0]).as_ptr(), digits(r, 1)).to_string());
                n += check_section(&t, "mbsnccnt", 1, |i, r| PlatMbsNCCnt(c(&i[0]).as_ptr(), digits(r, 1)).to_string());
                n +=
                    check_section(&t, "mbscmp", 2, |i, _| PlatMbsCmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr()).to_string());
                n += check_section(&t, "mbsicmp", 2, |i, _| {
                    PlatMbsICmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr()).to_string()
                });
                n += check_section(&t, "mbsnbcmp", 2, |i, r| {
                    PlatMbsNBCmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "mbsncmp", 2, |i, r| {
                    PlatMbsNCmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "mbsnbicmp", 2, |i, r| {
                    PlatMbsNBICmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "mbsninc", 1, |i, r| {
                    let s = c(&i[0]);
                    offset_of(&s, PlatMbsNInc(s.as_ptr(), digits(r, 1)))
                });
                n += check_section(&t, "mbsdec", 1, |i, r| {
                    let s = c(&i[0]);
                    offset_of(&s, PlatMbsDec(s.as_ptr(), s.as_ptr().add(digits(r, 1))))
                });
                n += check_section(&t, "mbschr", 1, |i, r| {
                    let s = c(&i[0]);
                    let ch: c_uint = r[1].parse().unwrap();
                    if ch >= 0x80 {
                        return r[2].clone(); // a code page character, not ASCII
                    }
                    offset_of(&s, PlatMbsChr(s.as_ptr(), ch))
                });
                n += check_section(&t, "mbscase", 1, |i, r| {
                    let mut s = c(&i[0]);
                    match r[1].as_str() {
                        "lwr" => PlatMbsLwr(s.as_mut_ptr()),
                        _ => PlatMbsUpr(s.as_mut_ptr()),
                    };
                    hex(&s[..s.len() - 1])
                });
                n += check_section(&t, "mbsncpy", 2, |i, r| {
                    let mut dest = vec![0xEEu8; 32];
                    dest[..i[0].len()].copy_from_slice(&i[0]);
                    PlatMbsNCpy(dest.as_mut_ptr(), c(&i[1]).as_ptr(), digits(r, 2));
                    hex(&dest)
                });
                n += check_section(&t, "mbsinc", 1, |i, _| {
                    let s = c(&i[0]);
                    offset_of(&s, PlatMbsInc(s.as_ptr()))
                });
                n += check_section(&t, "mbsnbcat", 2, |i, r| {
                    let mut dest = vec![0xEEu8; 32];
                    dest[..i[0].len()].copy_from_slice(&i[0]);
                    dest[i[0].len()] = 0;
                    PlatMbsNBCat(dest.as_mut_ptr(), c(&i[1]).as_ptr(), digits(r, 2));
                    hex(&dest)
                });
                n += check_section(&t, "ismbc", 0, |_, r| {
                    let v: c_uint = r[1].parse().unwrap();
                    if v >= 0x80 {
                        return r[2].clone(); // the code page's own lead bytes and spaces: not ASCII
                    }
                    match r[0].as_str() {
                        "space" => (PlatIsMbcSpace(v) != 0).to_string(),
                        "digit" => (PlatIsMbcDigit(v) != 0).to_string(),
                        _ => (PlatIsMbbLead(v) != 0).to_string(),
                    }
                });
            }
            assert_eq!(n, 1280, "the ASCII golden rows");
        }
    }
}
