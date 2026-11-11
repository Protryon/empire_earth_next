//! A string list box's type-to-find (`User Interface/UILBString.cpp`, `UILBString::UpdateTranslatedKey`): the next
//! line after the selection whose first byte is the typed key's, in either case, wrapping to the top once.
//!
//! The comparison is the C++'s: `UWideString::operator[](0)` is a line's first byte (0 for an empty string), the
//! key's lower and upper case are `PlatMbsLwr`/`PlatMbsUpr` of the whole key (UTF-8, decision D6), so a key whose
//! first character is not ASCII matches a line on its lead byte only.

use core::ffi::{c_long, c_ulong};

use ee_plat_core::utf8;

/// The key's lower- and upper-case first bytes (`UWideString(inKey)` after `ToLower`/`ToUpper`, then `[0]`).
pub fn key_bytes(key: &[u8]) -> (u8, u8) {
    let n = key.iter().position(|&b| b == 0).unwrap_or(key.len());
    let mut lower = key[..n].to_vec();
    let mut upper = lower.clone();
    utf8::map_in_place(&mut lower, utf8::fold);
    utf8::map_in_place(&mut upper, utf8::upper);
    (lower.first().copied().unwrap_or(0), upper.first().copied().unwrap_or(0))
}

/// The list as `UpdateTranslatedKey` asks about it.
pub trait FindHost {
    /// The virtual `GetNumLines()`, asked where the C++ asked it.
    fn lines(&mut self) -> usize;
    /// Line `i`'s first byte (`theLine[0]`), `None` when it is empty (`GetLength() == 0`).
    fn first(&mut self, i: usize) -> Option<u8>;
}

/// The line to select for a typed key (`selection` is `mCurrentSelection`, -1 for none). The search starts after
/// the selection (at the top past the end) and wraps to the top once when it did not start there.
pub fn find(key: &[u8], selection: c_ulong, h: &mut impl FindHost) -> Option<usize> {
    let (lower, upper) = key_bytes(key);
    let mut start = selection.wrapping_add(1) as c_long;
    if start >= h.lines() as c_long {
        start = 0;
    }
    let mut i = start;
    while (i as c_ulong as usize) < h.lines() {
        if let Some(b) = h.first(i as usize) {
            if b == lower || b == upper {
                break;
            }
        }
        if start > 0 && i as c_ulong as usize == h.lines().wrapping_sub(1) {
            start = 0;
            i = -1;
        }
        i += 1;
    }
    ((i as c_ulong as usize) < h.lines()).then_some(i as usize)
}

#[cfg(test)]
mod tests;
