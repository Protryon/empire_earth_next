//! `platform/PlatString.h`: the byte-string and multibyte string functions the game's text is written on. Windows
//! calls the C runtime functions the C++ called (`_stricmp`, `_mbs*`...), so the running locale (`setlocale`,
//! EEUserInterface) and the multibyte code page keep deciding, as they did. POSIX is the single-byte port in `sbcs`:
//! every byte one character, ASCII case folding (PORT.md: an encoding needs the fonts and text input to agree first).
//! PlatCp1252ToWide/PlatWideToCp1252 are not here: they share the font crate's table (ee_plat_ui, cp1252.rs).

use std::ffi::{c_char, c_int, c_uint};

/// The single-byte `_mbs*` semantics (the former src/Platform/Posix/PlatString.cpp): MSVC's in the "C" locale with no
/// lead bytes. Works through raw pointers as the C API does; tested on every system against the C++ goldens.
#[cfg(any(unix, test))]
pub(crate) mod sbcs {
    use std::ffi::{c_char, c_int, c_uint, CStr};
    use std::ptr::{null_mut, write_bytes};

    fn lower(c: u8) -> c_int {
        if c.is_ascii_uppercase() {
            (c + 32) as c_int
        } else {
            c as c_int
        }
    }

    fn upper(c: u8) -> c_int {
        if c.is_ascii_lowercase() {
            (c - 32) as c_int
        } else {
            c as c_int
        }
    }

    unsafe fn strlen(s: *const u8) -> usize {
        CStr::from_ptr(s.cast()).to_bytes().len()
    }

    pub unsafe fn str_icmp(mut left: *const u8, mut right: *const u8) -> c_int {
        // MSVC: both sides lower-cased, compared as unsigned bytes
        loop {
            let (x, y) = (lower(*left), lower(*right));
            if x != y || x == 0 {
                return x - y;
            }
            left = left.add(1);
            right = right.add(1);
        }
    }

    pub unsafe fn str_nicmp(mut left: *const u8, mut right: *const u8, mut count: usize) -> c_int {
        while count > 0 {
            let (x, y) = (lower(*left), lower(*right));
            if x != y || x == 0 {
                return x - y;
            }
            left = left.add(1);
            right = right.add(1);
            count -= 1;
        }
        0
    }

    pub unsafe fn itoa(value: c_int, buffer: *mut c_char, radix: c_int) -> *mut c_char {
        // MSVC _itoa: a minus sign only in radix 10; other radices format the value's 32-bit two's complement
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

    pub unsafe fn mbs_len(s: *const u8) -> usize {
        strlen(s)
    }

    pub unsafe fn mbs_nb_cnt(s: *const u8, characters: usize) -> usize {
        let mut n = 0;
        while n < characters && *s.add(n) != 0 {
            n += 1;
        }
        n
    }

    pub unsafe fn mbs_nc_cnt(s: *const u8, bytes: usize) -> usize {
        mbs_nb_cnt(s, bytes)
    }

    pub unsafe fn mbs_inc(current: *const u8) -> *mut u8 {
        current.add(1).cast_mut()
    }

    pub unsafe fn mbs_ninc(current: *const u8, characters: usize) -> *mut u8 {
        if current.is_null() {
            return null_mut();
        }
        current.add(mbs_nb_cnt(current, characters)).cast_mut()
    }

    pub unsafe fn mbs_dec(start: *const u8, current: *const u8) -> *mut u8 {
        if current <= start {
            return null_mut();
        }
        current.sub(1).cast_mut()
    }

    pub unsafe fn mbs_cmp(mut left: *const u8, mut right: *const u8) -> c_int {
        loop {
            let (x, y) = (*left, *right);
            if x != y {
                return if x < y { -1 } else { 1 };
            }
            if x == 0 {
                return 0;
            }
            left = left.add(1);
            right = right.add(1);
        }
    }

    pub unsafe fn mbs_icmp(mut left: *const u8, mut right: *const u8) -> c_int {
        // _mbsicmp folds to upper case (it differs from _stricmp for the bytes between 'Z' and 'a')
        loop {
            let (x, y) = (upper(*left), upper(*right));
            if x != y {
                return if x < y { -1 } else { 1 };
            }
            if x == 0 {
                return 0;
            }
            left = left.add(1);
            right = right.add(1);
        }
    }

    pub unsafe fn mbs_nb_cmp(mut left: *const u8, mut right: *const u8, mut bytes: usize) -> c_int {
        while bytes > 0 {
            let (x, y) = (*left, *right);
            if x != y {
                return if x < y { -1 } else { 1 };
            }
            if x == 0 {
                return 0;
            }
            left = left.add(1);
            right = right.add(1);
            bytes -= 1;
        }
        0
    }

    pub unsafe fn mbs_ncmp(left: *const u8, right: *const u8, characters: usize) -> c_int {
        mbs_nb_cmp(left, right, characters)
    }

    pub unsafe fn mbs_nb_icmp(mut left: *const u8, mut right: *const u8, mut bytes: usize) -> c_int {
        while bytes > 0 {
            let (x, y) = (upper(*left), upper(*right));
            if x != y {
                return if x < y { -1 } else { 1 };
            }
            if x == 0 {
                return 0;
            }
            left = left.add(1);
            right = right.add(1);
            bytes -= 1;
        }
        0
    }

    pub unsafe fn mbs_ncpy(dest: *mut u8, source: *const u8, characters: usize) -> *mut u8 {
        // as strncpy: pads with zeros when the source is shorter, no terminator when it is not
        let mut i = 0;
        while i < characters && *source.add(i) != 0 {
            *dest.add(i) = *source.add(i);
            i += 1;
        }
        if i < characters {
            write_bytes(dest.add(i), 0, characters - i);
        }
        dest
    }

    pub unsafe fn mbs_nb_cat(dest: *mut u8, source: *const u8, bytes: usize) -> *mut u8 {
        let mut p = dest.add(strlen(dest));
        let mut i = 0;
        while i < bytes && *source.add(i) != 0 {
            *p = *source.add(i);
            p = p.add(1);
            i += 1;
        }
        *p = 0;
        dest
    }

    pub unsafe fn mbs_chr(s: *const u8, character: c_uint) -> *mut u8 {
        if character > 0xFF {
            return null_mut();
        }
        let mut p = s;
        loop {
            if *p == character as u8 {
                return p.cast_mut();
            }
            if *p == 0 {
                return null_mut();
            }
            p = p.add(1);
        }
    }

    pub unsafe fn mbs_lwr(s: *mut u8) -> *mut u8 {
        let mut p = s;
        while *p != 0 {
            *p = lower(*p) as u8;
            p = p.add(1);
        }
        s
    }

    pub unsafe fn mbs_upr(s: *mut u8) -> *mut u8 {
        let mut p = s;
        while *p != 0 {
            *p = upper(*p) as u8;
            p = p.add(1);
        }
        s
    }

    pub fn is_mbc_space(character: c_uint) -> c_int {
        (character == 0x20 || (0x09..=0x0D).contains(&character)) as c_int
    }

    pub fn is_mbc_digit(character: c_uint) -> c_int {
        (0x30..=0x39).contains(&character) as c_int
    }

    pub fn is_mbb_lead(_byte: c_uint) -> c_int {
        0
    }
}

#[cfg(windows)]
mod imp {
    use std::ffi::{c_char, c_int, c_uint};

    extern "C" {
        pub fn _stricmp(left: *const c_char, right: *const c_char) -> c_int;
        pub fn _strnicmp(left: *const c_char, right: *const c_char, count: usize) -> c_int;
        pub fn _itoa(value: c_int, buffer: *mut c_char, radix: c_int) -> *mut c_char;
        pub fn _strdup(text: *const c_char) -> *mut c_char;
        pub fn _mbslen(s: *const u8) -> usize;
        pub fn _mbsnbcnt(s: *const u8, characters: usize) -> usize;
        pub fn _mbsnccnt(s: *const u8, bytes: usize) -> usize;
        pub fn _mbsinc(current: *const u8) -> *mut u8;
        pub fn _mbsninc(current: *const u8, characters: usize) -> *mut u8;
        pub fn _mbsdec(start: *const u8, current: *const u8) -> *mut u8;
        pub fn _mbscmp(left: *const u8, right: *const u8) -> c_int;
        pub fn _mbsicmp(left: *const u8, right: *const u8) -> c_int;
        pub fn _mbsncmp(left: *const u8, right: *const u8, characters: usize) -> c_int;
        pub fn _mbsnbcmp(left: *const u8, right: *const u8, bytes: usize) -> c_int;
        pub fn _mbsnbicmp(left: *const u8, right: *const u8, bytes: usize) -> c_int;
        pub fn _mbsncpy(dest: *mut u8, source: *const u8, characters: usize) -> *mut u8;
        pub fn _mbsnbcat(dest: *mut u8, source: *const u8, bytes: usize) -> *mut u8;
        pub fn _mbschr(s: *const u8, character: c_uint) -> *mut u8;
        pub fn _mbslwr(s: *mut u8) -> *mut u8;
        pub fn _mbsupr(s: *mut u8) -> *mut u8;
        pub fn _ismbcspace(character: c_uint) -> c_int;
        pub fn _ismbcdigit(character: c_uint) -> c_int;
        pub fn _ismbblead(byte: c_uint) -> c_int;
    }

    pub unsafe fn str_icmp(left: *const u8, right: *const u8) -> c_int {
        _stricmp(left.cast(), right.cast())
    }

    pub unsafe fn str_nicmp(left: *const u8, right: *const u8, count: usize) -> c_int {
        _strnicmp(left.cast(), right.cast(), count)
    }
}

#[cfg(unix)]
mod imp {
    pub use super::sbcs::*;
}

#[no_mangle]
pub unsafe extern "C" fn PlatStrICmp(left: *const c_char, right: *const c_char) -> c_int {
    imp::str_icmp(left.cast(), right.cast())
}

#[no_mangle]
pub unsafe extern "C" fn PlatStrNICmp(left: *const c_char, right: *const c_char, count: usize) -> c_int {
    imp::str_nicmp(left.cast(), right.cast(), count)
}

#[no_mangle]
pub unsafe extern "C" fn PlatIToA(value: c_int, buffer: *mut c_char, radix: c_int) -> *mut c_char {
    #[cfg(windows)]
    return imp::_itoa(value, buffer, radix);
    #[cfg(unix)]
    return imp::itoa(value, buffer, radix);
}

#[no_mangle]
pub unsafe extern "C" fn PlatStrDup(text: *const c_char) -> *mut c_char {
    #[cfg(windows)]
    return imp::_strdup(text);
    #[cfg(unix)]
    return if text.is_null() { std::ptr::null_mut() } else { libc::strdup(text) };
}

macro_rules! exported {
    ($($name:ident / $crt:ident / $sbcs:ident ( $($arg:ident : $ty:ty),* ) -> $ret:ty;)*) => {
        $(#[no_mangle]
        pub unsafe extern "C" fn $name($($arg: $ty),*) -> $ret {
            #[cfg(windows)]
            return imp::$crt($($arg),*);
            #[cfg(unix)]
            return imp::$sbcs($($arg),*);
        })*
    };
}

exported! {
    PlatMbsLen / _mbslen / mbs_len (s: *const u8) -> usize;
    PlatMbsNBCnt / _mbsnbcnt / mbs_nb_cnt (s: *const u8, characters: usize) -> usize;
    PlatMbsNCCnt / _mbsnccnt / mbs_nc_cnt (s: *const u8, bytes: usize) -> usize;
    PlatMbsInc / _mbsinc / mbs_inc (current: *const u8) -> *mut u8;
    PlatMbsNInc / _mbsninc / mbs_ninc (current: *const u8, characters: usize) -> *mut u8;
    PlatMbsDec / _mbsdec / mbs_dec (start: *const u8, current: *const u8) -> *mut u8;
    PlatMbsCmp / _mbscmp / mbs_cmp (left: *const u8, right: *const u8) -> c_int;
    PlatMbsICmp / _mbsicmp / mbs_icmp (left: *const u8, right: *const u8) -> c_int;
    PlatMbsNCmp / _mbsncmp / mbs_ncmp (left: *const u8, right: *const u8, characters: usize) -> c_int;
    PlatMbsNBCmp / _mbsnbcmp / mbs_nb_cmp (left: *const u8, right: *const u8, bytes: usize) -> c_int;
    PlatMbsNBICmp / _mbsnbicmp / mbs_nb_icmp (left: *const u8, right: *const u8, bytes: usize) -> c_int;
    PlatMbsNCpy / _mbsncpy / mbs_ncpy (dest: *mut u8, source: *const u8, characters: usize) -> *mut u8;
    PlatMbsNBCat / _mbsnbcat / mbs_nb_cat (dest: *mut u8, source: *const u8, bytes: usize) -> *mut u8;
    PlatMbsChr / _mbschr / mbs_chr (s: *const u8, character: c_uint) -> *mut u8;
    PlatMbsLwr / _mbslwr / mbs_lwr (s: *mut u8) -> *mut u8;
    PlatMbsUpr / _mbsupr / mbs_upr (s: *mut u8) -> *mut u8;
    PlatIsMbcSpace / _ismbcspace / is_mbc_space (character: c_uint) -> c_int;
    PlatIsMbcDigit / _ismbcdigit / is_mbc_digit (character: c_uint) -> c_int;
    PlatIsMbbLead / _ismbblead / is_mbb_lead (byte: c_uint) -> c_int;
}

#[cfg(test)]
mod tests {
    use super::sbcs;
    use super::*;
    use std::ptr::null;

    fn c(text: &[u8]) -> Vec<u8> {
        crate::c_text::terminated(text)
    }

    #[test]
    fn c_api_contract() {
        // the checks of tests/platform/plat_test.cpp "strings", on the exported functions (the system's own)
        let _guard = crate::test_globals();
        unsafe {
            assert_eq!(PlatStrICmp(c"Hello".as_ptr(), c"hELLO".as_ptr()), 0);
            assert!(PlatStrICmp(c"abc".as_ptr(), c"ABD".as_ptr()) < 0);
            assert!(PlatStrICmp(c"b".as_ptr(), c"A".as_ptr()) > 0);
            assert!(PlatStrICmp(c"_".as_ptr(), c"a".as_ptr()) < 0);
            assert_eq!(PlatStrNICmp(c"Data\\Units".as_ptr(), c"DATA\\Other".as_ptr(), 5), 0);
            assert!(PlatStrNICmp(c"Data\\Units".as_ptr(), c"DATA\\Other".as_ptr(), 6) > 0);
            let mut buffer = [0 as c_char; 40];
            let text = |p: *mut c_char| std::ffi::CStr::from_ptr(p).to_bytes().to_vec();
            assert_eq!(text(PlatIToA(-123, buffer.as_mut_ptr(), 10)), b"-123");
            assert_eq!(text(PlatIToA(255, buffer.as_mut_ptr(), 16)), b"ff");
            assert_eq!(text(PlatIToA(-1, buffer.as_mut_ptr(), 16)), b"ffffffff");
            assert_eq!(text(PlatIToA(5, buffer.as_mut_ptr(), 2)), b"101");
            let copy = PlatStrDup(c"copy".as_ptr());
            assert_eq!(text(copy), b"copy");
            libc::free(copy.cast());

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
        }
    }

    /// Golden values captured from the deleted C++ implementation: one row per input (see tools in the commit that
    /// added this file). `golden_sbcs.txt`: the POSIX C++; `golden_crt.txt`: the Win32 C++ under given code pages.
    mod golden {
        use super::*;
        use std::collections::HashMap;

        fn parse_hex(text: &str) -> Vec<u8> {
            (0..text.len() / 2).map(|i| u8::from_str_radix(&text[i * 2..i * 2 + 2], 16).unwrap()).collect()
        }

        /// A golden file: section -> rows of its fields.
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

        /// Runs a function of the byte-string kinds over the rows of one section.
        unsafe fn check_section<F: Fn(&[Vec<u8>], &[String]) -> String>(
            rows: &HashMap<String, Vec<Vec<String>>>,
            name: &str,
            arguments: usize,
            function: F,
        ) -> usize {
            let rows = rows.get(name).unwrap_or_else(|| panic!("no golden section {name}"));
            for row in rows {
                let inputs: Vec<Vec<u8>> = row[..arguments].iter().map(|t| parse_hex(t)).collect();
                let expected = row[row.len() - 1].clone();
                assert_eq!(function(&inputs, row), expected, "{name} {row:?}");
            }
            rows.len()
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

        #[test]
        fn single_byte_goldens_from_the_posix_cpp() {
            let t = table(include_str!("golden_sbcs.txt"));
            let mut n = 0;
            unsafe {
                n += check_section(&t, "stricmp", 2, |i, _| {
                    sbcs::str_icmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr()).to_string()
                });
                n += check_section(&t, "strnicmp", 2, |i, r| {
                    sbcs::str_nicmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "itoa", 0, |_, r| {
                    let mut buffer = [0 as c_char; 64];
                    let value: c_int = r[0].parse().unwrap();
                    let radix: c_int = r[1].parse().unwrap();
                    let out = sbcs::itoa(value, buffer.as_mut_ptr(), radix);
                    String::from_utf8(std::ffi::CStr::from_ptr(out).to_bytes().to_vec()).unwrap()
                });
                n += check_section(&t, "mbslen", 1, |i, _| sbcs::mbs_len(c(&i[0]).as_ptr()).to_string());
                n += check_section(&t, "mbsnbcnt", 1, |i, r| {
                    sbcs::mbs_nb_cnt(c(&i[0]).as_ptr(), digits(r, 1)).to_string()
                });
                n += check_section(&t, "mbsnccnt", 1, |i, r| {
                    sbcs::mbs_nc_cnt(c(&i[0]).as_ptr(), digits(r, 1)).to_string()
                });
                n += check_section(&t, "mbscmp", 2, |i, _| {
                    sbcs::mbs_cmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr()).to_string()
                });
                n += check_section(&t, "mbsicmp", 2, |i, _| {
                    sbcs::mbs_icmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr()).to_string()
                });
                n += check_section(&t, "mbsnbcmp", 2, |i, r| {
                    sbcs::mbs_nb_cmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "mbsncmp", 2, |i, r| {
                    sbcs::mbs_ncmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "mbsnbicmp", 2, |i, r| {
                    sbcs::mbs_nb_icmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
                });
                n += check_section(&t, "mbsninc", 1, |i, r| {
                    let s = c(&i[0]);
                    offset_of(&s, sbcs::mbs_ninc(s.as_ptr(), digits(r, 1)))
                });
                n += check_section(&t, "mbsdec", 1, |i, r| {
                    let s = c(&i[0]);
                    offset_of(&s, sbcs::mbs_dec(s.as_ptr(), s.as_ptr().add(digits(r, 1))))
                });
                n += check_section(&t, "mbschr", 1, |i, r| {
                    let s = c(&i[0]);
                    offset_of(&s, sbcs::mbs_chr(s.as_ptr(), r[1].parse().unwrap()))
                });
                n += check_section(&t, "mbscase", 1, |i, r| {
                    let mut s = c(&i[0]);
                    match r[1].as_str() {
                        "lwr" => sbcs::mbs_lwr(s.as_mut_ptr()),
                        _ => sbcs::mbs_upr(s.as_mut_ptr()),
                    };
                    hex(&s[..s.len() - 1])
                });
                n += check_section(&t, "mbsncpy", 2, |i, r| {
                    let mut dest = vec![0xEEu8; 32];
                    dest[..i[0].len()].copy_from_slice(&i[0]);
                    sbcs::mbs_ncpy(dest.as_mut_ptr(), c(&i[1]).as_ptr(), digits(r, 2));
                    hex(&dest)
                });
                n += check_section(&t, "mbsinc", 1, |i, _| {
                    let s = c(&i[0]);
                    offset_of(&s, sbcs::mbs_inc(s.as_ptr()))
                });
                n += check_section(&t, "mbsnbcat", 2, |i, r| {
                    let mut dest = vec![0xEEu8; 32];
                    dest[..i[0].len()].copy_from_slice(&i[0]);
                    dest[i[0].len()] = 0;
                    sbcs::mbs_nb_cat(dest.as_mut_ptr(), c(&i[1]).as_ptr(), digits(r, 2));
                    hex(&dest)
                });
                n += check_section(&t, "ismbc", 0, |_, r| {
                    let v: c_uint = r[1].parse().unwrap();
                    match r[0].as_str() {
                        "space" => sbcs::is_mbc_space(v).to_string(),
                        "digit" => sbcs::is_mbc_digit(v).to_string(),
                        _ => sbcs::is_mbb_lead(v).to_string(),
                    }
                });
            }
            assert!(n > 500, "{n} golden rows");
        }

        fn hex(data: &[u8]) -> String {
            data.iter().map(|b| format!("{b:02x}")).collect()
        }

        /// The exported Windows functions against the Win32 C++ under given code pages. The C runtime is the
        /// implementation; this pins the binding (names, arguments, result conversions) and the C++'s results.
        #[cfg(windows)]
        #[test]
        fn windows_goldens_from_the_win32_cpp() {
            extern "C" {
                fn _setmbcp(codepage: c_int) -> c_int;
            }
            let _guard = crate::test_globals();
            let mut n = 0;
            for (codepage, data) in
                [(1252, include_str!("golden_crt_1252.txt")), (932, include_str!("golden_crt_932.txt"))]
            {
                let t = table(data);
                unsafe {
                    assert_eq!(_setmbcp(codepage), 0);
                    n += check_crt(&t);
                    assert_eq!(_setmbcp(0), 0); // "C": no lead bytes
                }
            }
            unsafe { _setmbcp(-3) }; // _MB_CP_ANSI, the runtime's startup setting
            assert!(n > 500, "{n} golden rows");
        }

        #[cfg(windows)]
        unsafe fn check_crt(t: &HashMap<String, Vec<Vec<String>>>) -> usize {
            let mut n = 0;
            n += check_section(t, "stricmp", 2, |i, _| {
                PlatStrICmp(c(&i[0]).as_ptr().cast(), c(&i[1]).as_ptr().cast()).to_string()
            });
            n += check_section(t, "strnicmp", 2, |i, r| {
                PlatStrNICmp(c(&i[0]).as_ptr().cast(), c(&i[1]).as_ptr().cast(), digits(r, 2)).to_string()
            });
            n += check_section(t, "itoa", 0, |_, r| {
                let mut buffer = [0 as c_char; 64];
                let out = PlatIToA(r[0].parse().unwrap(), buffer.as_mut_ptr(), r[1].parse().unwrap());
                String::from_utf8(std::ffi::CStr::from_ptr(out).to_bytes().to_vec()).unwrap()
            });
            n += check_section(t, "mbslen", 1, |i, _| PlatMbsLen(c(&i[0]).as_ptr()).to_string());
            n += check_section(t, "mbsnbcnt", 1, |i, r| PlatMbsNBCnt(c(&i[0]).as_ptr(), digits(r, 1)).to_string());
            n += check_section(t, "mbsnccnt", 1, |i, r| PlatMbsNCCnt(c(&i[0]).as_ptr(), digits(r, 1)).to_string());
            n += check_section(t, "mbscmp", 2, |i, _| PlatMbsCmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr()).to_string());
            n += check_section(t, "mbsicmp", 2, |i, _| PlatMbsICmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr()).to_string());
            n += check_section(t, "mbsnbcmp", 2, |i, r| {
                PlatMbsNBCmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
            });
            n += check_section(t, "mbsncmp", 2, |i, r| {
                PlatMbsNCmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
            });
            n += check_section(t, "mbsnbicmp", 2, |i, r| {
                PlatMbsNBICmp(c(&i[0]).as_ptr(), c(&i[1]).as_ptr(), digits(r, 2)).to_string()
            });
            n += check_section(t, "mbsninc", 1, |i, r| {
                let s = c(&i[0]);
                offset_of(&s, PlatMbsNInc(s.as_ptr(), digits(r, 1)))
            });
            n += check_section(t, "mbsdec", 1, |i, r| {
                let s = c(&i[0]);
                offset_of(&s, PlatMbsDec(s.as_ptr(), s.as_ptr().add(digits(r, 1))))
            });
            n += check_section(t, "mbschr", 1, |i, r| {
                let s = c(&i[0]);
                offset_of(&s, PlatMbsChr(s.as_ptr(), r[1].parse().unwrap()))
            });
            n += check_section(t, "mbscase", 1, |i, r| {
                let mut s = c(&i[0]);
                match r[1].as_str() {
                    "lwr" => PlatMbsLwr(s.as_mut_ptr()),
                    _ => PlatMbsUpr(s.as_mut_ptr()),
                };
                hex(&s[..s.len() - 1])
            });
            n += check_section(t, "mbsncpy", 2, |i, r| {
                let mut dest = vec![0xEEu8; 32];
                dest[..i[0].len()].copy_from_slice(&i[0]);
                PlatMbsNCpy(dest.as_mut_ptr(), c(&i[1]).as_ptr(), digits(r, 2));
                hex(&dest)
            });
            n += check_section(t, "mbsinc", 1, |i, _| {
                let s = c(&i[0]);
                offset_of(&s, PlatMbsInc(s.as_ptr()))
            });
            n += check_section(t, "mbsnbcat", 2, |i, r| {
                let mut dest = vec![0xEEu8; 32];
                dest[..i[0].len()].copy_from_slice(&i[0]);
                dest[i[0].len()] = 0;
                PlatMbsNBCat(dest.as_mut_ptr(), c(&i[1]).as_ptr(), digits(r, 2));
                hex(&dest)
            });
            n += check_section(t, "ismbc", 0, |_, r| {
                let v: c_uint = r[1].parse().unwrap();
                match r[0].as_str() {
                    "space" => (PlatIsMbcSpace(v) != 0).to_string(),
                    "digit" => (PlatIsMbcDigit(v) != 0).to_string(),
                    _ => (PlatIsMbbLead(v) != 0).to_string(),
                }
            });
            n
        }
    }

    #[test]
    fn null_tolerance_of_the_defined_cases() {
        unsafe {
            assert!(sbcs::mbs_ninc(null(), 3).is_null());
        }
    }
}
