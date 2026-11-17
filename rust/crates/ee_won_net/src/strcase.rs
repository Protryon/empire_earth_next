//! StringUtil's case functions (WONCommon/StringUtil.cpp) under D6: game text is UTF-8, and the C runtime's
//! character classes are the "C" locale's (the UI thread sets `LC_CTYPE` "C"), so a case map changes only the
//! ASCII letters: a UTF-8 sequence's bytes (all 0x80 and above) and every non-ASCII wide unit pass unchanged, which
//! is what the original's `toupper` / `tolower` / `towupper` / `towlower` did in the game's locale. The compares
//! keep the original's order: narrow text compares its upper-cased bytes as signed `char`s (a UTF-8 byte sorts
//! before ASCII), wide text its upper-cased units as `wchar_t`s; on a common prefix the shorter text is first.

/// ASCII upper / lower case of a byte (the "C" locale's `toupper` / `tolower`).
pub fn upper_byte(b: u8) -> u8 {
    b.to_ascii_uppercase()
}

pub fn lower_byte(b: u8) -> u8 {
    b.to_ascii_lowercase()
}

/// The same for a wide unit (any width: only U+0041-005A / U+0061-007A change).
pub fn upper_unit(u: u32) -> u32 {
    if (0x61..=0x7a).contains(&u) {
        u - 0x20
    } else {
        u
    }
}

pub fn lower_unit(u: u32) -> u32 {
    if (0x41..=0x5a).contains(&u) {
        u + 0x20
    } else {
        u
    }
}

fn order<T: Ord>(a: T, b: T) -> i32 {
    if a < b {
        -1
    } else {
        1
    }
}

/// `StringCompareNoCase`: -1, 0 or 1.
pub fn compare_no_case(a: &[u8], b: &[u8]) -> i32 {
    for (&x, &y) in a.iter().zip(b) {
        let (x, y) = (upper_byte(x) as i8, upper_byte(y) as i8);
        if x != y {
            return order(x, y);
        }
    }
    if a.len() == b.len() {
        0
    } else {
        order(a.len(), b.len())
    }
}

/// `WStringCompareNoCase` over units widened to `i64` by the caller's `wchar_t` (unsigned 16-bit on Windows,
/// signed 32-bit elsewhere).
pub fn wide_compare_no_case(a: &[i64], b: &[i64]) -> i32 {
    for (&x, &y) in a.iter().zip(b) {
        let (x, y) = (upper_wide(x), upper_wide(y));
        if x != y {
            return order(x, y);
        }
    }
    if a.len() == b.len() {
        0
    } else {
        order(a.len(), b.len())
    }
}

fn upper_wide(u: i64) -> i64 {
    if (0x61..=0x7a).contains(&u) {
        u - 0x20
    } else {
        u
    }
}

// ---- the C ABI

/// Upper-cases (`upper`) or lower-cases `length` bytes in place.
///
/// # Safety
/// `bytes` holds `length` writable bytes (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_case_bytes(bytes: *mut u8, length: usize, upper: bool) {
    if bytes.is_null() || length == 0 {
        return;
    }
    // SAFETY: the contract
    let s = unsafe { std::slice::from_raw_parts_mut(bytes, length) };
    for b in s {
        *b = if upper { upper_byte(*b) } else { lower_byte(*b) };
    }
}

/// The same over 16-bit units (Windows' `wchar_t`).
///
/// # Safety
/// `units` holds `count` writable units (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_case_units16(units: *mut u16, count: usize, upper: bool) {
    if units.is_null() || count == 0 {
        return;
    }
    // SAFETY: the contract
    let s = unsafe { std::slice::from_raw_parts_mut(units, count) };
    for u in s {
        let c = u32::from(*u);
        *u = (if upper { upper_unit(c) } else { lower_unit(c) }) as u16;
    }
}

/// The same over 32-bit units (POSIX's `wchar_t`).
///
/// # Safety
/// `units` holds `count` writable units (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_case_units32(units: *mut u32, count: usize, upper: bool) {
    if units.is_null() || count == 0 {
        return;
    }
    // SAFETY: the contract
    let s = unsafe { std::slice::from_raw_parts_mut(units, count) };
    for u in s {
        *u = if upper { upper_unit(*u) } else { lower_unit(*u) };
    }
}

unsafe fn slice<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` items (each function's contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `compare_no_case`.
///
/// # Safety
/// Each holds its length (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_compare_no_case(a: *const u8, a_length: usize, b: *const u8, b_length: usize) -> i32 {
    compare_no_case(unsafe { slice(a, a_length) }, unsafe { slice(b, b_length) })
}

/// `wide_compare_no_case` over 16-bit units (unsigned).
///
/// # Safety
/// Each holds its count (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_compare_no_case16(a: *const u16, a_count: usize, b: *const u16, b_count: usize) -> i32 {
    let a: Vec<i64> = unsafe { slice(a, a_count) }.iter().map(|&u| i64::from(u)).collect();
    let b: Vec<i64> = unsafe { slice(b, b_count) }.iter().map(|&u| i64::from(u)).collect();
    wide_compare_no_case(&a, &b)
}

/// `wide_compare_no_case` over 32-bit units (signed, as POSIX's `wchar_t`).
///
/// # Safety
/// Each holds its count (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_compare_no_case32(a: *const i32, a_count: usize, b: *const i32, b_count: usize) -> i32 {
    let a: Vec<i64> = unsafe { slice(a, a_count) }.iter().map(|&u| i64::from(u)).collect();
    let b: Vec<i64> = unsafe { slice(b, b_count) }.iter().map(|&u| i64::from(u)).collect();
    wide_compare_no_case(&a, &b)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_ascii_changes() {
        let mut s = "aZé☺".as_bytes().to_vec();
        unsafe { ee_won_case_bytes(s.as_mut_ptr(), s.len(), true) };
        assert_eq!(s, "AZé☺".as_bytes());
        unsafe { ee_won_case_bytes(s.as_mut_ptr(), s.len(), false) };
        assert_eq!(s, "azé☺".as_bytes());
        let mut w: Vec<u16> = "aÉé☺z".encode_utf16().collect();
        unsafe { ee_won_case_units16(w.as_mut_ptr(), w.len(), true) };
        assert_eq!(String::from_utf16(&w).unwrap(), "AÉé☺Z");
        let mut w: Vec<u32> = "QÉ".chars().map(u32::from).collect();
        unsafe { ee_won_case_units32(w.as_mut_ptr(), w.len(), false) };
        assert_eq!(w, vec![0x71, 0xc9]);
        for b in 0..=255u8 {
            if b >= 0x80 {
                assert_eq!((upper_byte(b), lower_byte(b)), (b, b));
            }
        }
    }

    #[test]
    fn compares() {
        assert_eq!(compare_no_case(b"abc", b"ABC"), 0);
        assert_eq!(compare_no_case(b"ab", b"ABC"), -1);
        assert_eq!(compare_no_case(b"abd", b"ABC"), 1);
        // a UTF-8 byte is a negative char: before ASCII
        assert_eq!(compare_no_case("é".as_bytes(), b"a"), -1);
        assert_eq!(compare_no_case(b"", b""), 0);
        assert_eq!(wide_compare_no_case(&[0x61, 0xe9], &[0x41, 0xc9]), 1);
        assert_eq!(wide_compare_no_case(&[0x61], &[0x41, 0x41]), -1);
        assert_eq!(wide_compare_no_case(&[0x7a], &[0x5a]), 0);
    }
}
