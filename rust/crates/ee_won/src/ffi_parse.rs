//! The C ABI of `parser` for the C++ `StringParser` (WONLobbyParser.h): the C++ object keeps its text pointer, its
//! position and its width; each call passes the text (NUL-terminated, units of 1 byte narrow or `sizeof(wchar_t)`
//! wide), the position to move, and gets the result. The character classes are the C runtime's (`CrtCtype`), so
//! the locale the game sets still decides them.

use std::ffi::{c_char, c_void};

use crate::ffi::EeWonByteSink;
use crate::parser::{self, Ctype, Parser, Text};

/// Receives wide units (`wchar_t` values); valid only during the call.
pub type EeWonWideUnitSink = unsafe extern "C" fn(context: *mut c_void, units: *const u32, count: usize);

struct RawText {
    text: *const c_void,
    unit_size: u32,
}

impl Text for RawText {
    fn char_at(&self, index: i32) -> u8 {
        if self.unit_size == 1 {
            self.wchar_at(index) as u8
        } else {
            let unit = self.wchar_at(index);
            if unit < 256 {
                unit as u8
            } else {
                0xff
            }
        }
    }
    fn wchar_at(&self, index: i32) -> u32 {
        let i = index as isize;
        // SAFETY: the C++ cursor reads only inside its NUL-terminated text, as the original did (the contract of
        // every function below); the pointer may be unaligned for nothing, wchar_t arrays are aligned
        unsafe {
            match self.unit_size {
                1 => u32::from(*(self.text as *const u8).offset(i)),
                2 => u32::from(*(self.text as *const u16).offset(i)),
                _ => *(self.text as *const u32).offset(i),
            }
        }
    }
}

#[cfg(not(test))]
mod crt {
    use std::ffi::c_int;
    extern "C" {
        pub fn isspace(c: c_int) -> c_int;
        pub fn isdigit(c: c_int) -> c_int;
        pub fn isxdigit(c: c_int) -> c_int;
        pub fn isalnum(c: c_int) -> c_int;
        pub fn toupper(c: c_int) -> c_int;
        pub fn tolower(c: c_int) -> c_int;
    }
}

/// The C runtime's ctype functions, in the current locale.
struct CrtCtype;

#[cfg(not(test))]
impl Ctype for CrtCtype {
    fn is_space(&self, c: i32) -> bool {
        // SAFETY: the CRT's ctype functions take any int the original passed them
        unsafe { crt::isspace(c) != 0 }
    }
    fn is_digit(&self, c: i32) -> bool {
        unsafe { crt::isdigit(c) != 0 }
    }
    fn is_xdigit(&self, c: i32) -> bool {
        unsafe { crt::isxdigit(c) != 0 }
    }
    fn is_alnum(&self, c: i32) -> bool {
        unsafe { crt::isalnum(c) != 0 }
    }
    fn to_upper(&self, c: i32) -> i32 {
        unsafe { crt::toupper(c) }
    }
    fn to_lower(&self, c: i32) -> i32 {
        unsafe { crt::tolower(c) }
    }
}

// cargo test links no C++ and needs no locale: the C locale's classes
#[cfg(test)]
impl Ctype for CrtCtype {
    fn is_space(&self, c: i32) -> bool {
        parser::CLocale.is_space(c)
    }
    fn is_digit(&self, c: i32) -> bool {
        parser::CLocale.is_digit(c)
    }
    fn is_xdigit(&self, c: i32) -> bool {
        parser::CLocale.is_xdigit(c)
    }
    fn is_alnum(&self, c: i32) -> bool {
        parser::CLocale.is_alnum(c)
    }
    fn to_upper(&self, c: i32) -> i32 {
        parser::CLocale.to_upper(c)
    }
    fn to_lower(&self, c: i32) -> i32 {
        parser::CLocale.to_lower(c)
    }
}

unsafe fn with<R>(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    f: impl FnOnce(&mut Parser<'_, RawText, CrtCtype>) -> R,
) -> R {
    let raw = RawText { text, unit_size };
    // SAFETY: `pos` is the C++ object's position (each function's contract)
    let mut p = Parser::new(&raw, &CrtCtype, unsafe { *pos });
    let r = f(&mut p);
    unsafe { *pos = p.pos };
    r
}

/// `StringParser::GetChar(offset)` at `pos`.
///
/// # Safety
/// `text` is a NUL-terminated string of `unit_size`-byte units (1, 2 or 4) and `pos + offset` inside it.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_get_char(text: *const c_void, unit_size: u32, pos: i32, offset: i32) -> u8 {
    RawText { text, unit_size }.char_at(pos.wrapping_add(offset))
}

/// `StringParser::GetWChar(offset)` at `pos`.
///
/// # Safety
/// As `ee_won_parser_get_char`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_get_wchar(text: *const c_void, unit_size: u32, pos: i32, offset: i32) -> u32 {
    RawText { text, unit_size }.wchar_at(pos.wrapping_add(offset))
}

/// `StringParser::SkipWhitespace`.
///
/// # Safety
/// `text` is a NUL-terminated string of `unit_size`-byte units, `*pos` inside it.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_skip_whitespace(text: *const c_void, unit_size: u32, pos: *mut i32) {
    unsafe { with(text, unit_size, pos, |p| p.skip_whitespace()) }
}

/// `StringParser::SkipNonWhitespace`.
///
/// # Safety
/// As `ee_won_parser_skip_whitespace`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_skip_non_whitespace(text: *const c_void, unit_size: u32, pos: *mut i32) {
    unsafe { with(text, unit_size, pos, |p| p.skip_non_whitespace()) }
}

/// `StringParser::CheckNextChar(ch)`.
///
/// # Safety
/// As `ee_won_parser_skip_whitespace`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_check_next_char(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    ch: i32,
) -> bool {
    unsafe { with(text, unit_size, pos, |p| p.check_next_char(ch)) }
}

unsafe fn c_bytes<'a>(s: *const c_char) -> &'a [u8] {
    if s.is_null() {
        &[]
    } else {
        // SAFETY: a NUL-terminated string (the callers' contract)
        unsafe { std::ffi::CStr::from_ptr(s) }.to_bytes()
    }
}

/// `StringParser::StartsWith(prefix)`.
///
/// # Safety
/// As `ee_won_parser_skip_whitespace`; `prefix` is NUL-terminated.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_starts_with(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    prefix: *const c_char,
) -> bool {
    let prefix = unsafe { c_bytes(prefix) };
    unsafe { with(text, unit_size, pos, |p| p.starts_with(prefix)) }
}

/// `StringParser::Is(word)`.
///
/// # Safety
/// As `ee_won_parser_starts_with`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_is(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    word: *const c_char,
) -> bool {
    let word = unsafe { c_bytes(word) };
    unsafe { with(text, unit_size, pos, |p| p.is(word)) }
}

/// `StringParser::ReadString(std::string&, stopAtWhitespace)`: the token to `sink` (always true in the C++).
///
/// # Safety
/// As `ee_won_parser_skip_whitespace`; `sink` takes the bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_read_string(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    stop_at_whitespace: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let out = unsafe { with(text, unit_size, pos, |p| p.read_string(stop_at_whitespace)) };
    unsafe { sink(context, out.as_ptr(), out.len()) }
}

/// `StringParser::ReadString(std::wstring&, stopAtWhitespace)`: the token's units to `sink`.
///
/// # Safety
/// As `ee_won_parser_read_string`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_read_wstring(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    stop_at_whitespace: bool,
    sink: EeWonWideUnitSink,
    context: *mut c_void,
) {
    let out = unsafe { with(text, unit_size, pos, |p| p.read_wstring(stop_at_whitespace)) };
    unsafe { sink(context, out.as_ptr(), out.len()) }
}

/// `StringParser::ReadValue(int&)`: true and `*out` when the cursor moved; `*out` untouched otherwise.
///
/// # Safety
/// As `ee_won_parser_skip_whitespace`; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_read_int(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    out: *mut i32,
) -> bool {
    match unsafe { with(text, unit_size, pos, |p| p.read_int()) } {
        Some(v) => {
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// `StringParser::ReadValue(bool&)`: true and `*out` for a boolean word; `*out` untouched otherwise.
///
/// # Safety
/// As `ee_won_parser_read_int`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_read_bool(
    text: *const c_void,
    unit_size: u32,
    pos: *mut i32,
    out: *mut bool,
) -> bool {
    match unsafe { with(text, unit_size, pos, |p| p.read_bool()) } {
        Some(v) => {
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

unsafe fn put_text(s: &[u8], sink: EeWonByteSink, context: *mut c_void) {
    unsafe { sink(context, s.as_ptr(), s.len()) }
}

/// `StringParser::ValueToString` of a signed integer (int, long, short, char, __int64): decimal to `sink`.
///
/// # Safety
/// `sink` takes the bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_format_i64(value: i64, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { put_text(value.to_string().as_bytes(), sink, context) }
}

/// `StringParser::ValueToString` of an unsigned integer (unsigned long, unsigned char, unsigned __int64).
///
/// # Safety
/// As `ee_won_parser_format_i64`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_format_u64(value: u64, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { put_text(value.to_string().as_bytes(), sink, context) }
}

/// `StringParser::ValueToString(const bool&)`: "True" or "False".
///
/// # Safety
/// As `ee_won_parser_format_i64`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_parser_format_bool(value: bool, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { put_text(parser::format_bool(value), sink, context) }
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn collect(context: *mut c_void, bytes: *const u8, length: usize) {
        let v = unsafe { &mut *(context as *mut Vec<u8>) };
        v.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }

    unsafe extern "C" fn collect_wide(context: *mut c_void, units: *const u32, count: usize) {
        let v = unsafe { &mut *(context as *mut Vec<u32>) };
        v.extend_from_slice(unsafe { std::slice::from_raw_parts(units, count) });
    }

    #[test]
    fn raw_text_in_every_unit_size() {
        let narrow = b"  x\0";
        let wide16: Vec<u16> = vec![0x20, 0x263a, 0x41, 0];
        let wide32: Vec<u32> = vec![0x20, 0x263a, 0x41, 0];
        let mut pos = 0;
        unsafe { ee_won_parser_skip_whitespace(narrow.as_ptr().cast(), 1, &mut pos) };
        assert_eq!(pos, 2);
        for (text, size) in [(wide16.as_ptr().cast::<c_void>(), 2), (wide32.as_ptr().cast(), 4)] {
            let mut pos = 0;
            unsafe { ee_won_parser_skip_whitespace(text, size, &mut pos) };
            assert_eq!(pos, 1);
            assert_eq!(unsafe { ee_won_parser_get_char(text, size, pos, 0) }, 0xff);
            assert_eq!(unsafe { ee_won_parser_get_wchar(text, size, pos, 0) }, 0x263a);
            let mut out = Vec::<u32>::new();
            unsafe {
                ee_won_parser_read_wstring(
                    text,
                    size,
                    &mut pos,
                    false,
                    collect_wide,
                    (&mut out as *mut Vec<u32>).cast(),
                )
            };
            assert_eq!(out, vec![0x263a, 0x41]);
            assert_eq!(pos, 3);
        }
    }

    #[test]
    fn reads_and_formats() {
        let text = b"-0x10 yes\0";
        let mut pos = 0;
        let mut v = 7;
        assert!(unsafe { ee_won_parser_read_int(text.as_ptr().cast(), 1, &mut pos, &mut v) });
        assert_eq!((v, pos), (-16, 5));
        let mut b = false;
        assert!(unsafe { ee_won_parser_read_bool(text.as_ptr().cast(), 1, &mut pos, &mut b) });
        assert!(b);
        let mut v = 7;
        assert!(!unsafe { ee_won_parser_read_int(text.as_ptr().cast(), 1, &mut pos, &mut v) });
        assert_eq!(v, 7, "untouched on failure");
        let mut out = Vec::new();
        let ctx = (&mut out as *mut Vec<u8>).cast();
        unsafe {
            ee_won_parser_format_i64(-5, collect, ctx);
            ee_won_parser_format_u64(u64::MAX, collect, ctx);
            ee_won_parser_format_bool(false, collect, ctx);
        }
        assert_eq!(out, b"-518446744073709551615False");
        let mut pos = 0;
        let word = c"ab";
        assert!(!unsafe { ee_won_parser_is(c"abc".as_ptr().cast(), 1, &mut pos, word.as_ptr()) });
        assert_eq!(pos, 2);
    }
}
