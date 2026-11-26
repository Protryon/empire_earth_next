//! WONGUI's StringLocalize::GetStr in Rust (R6 WON round 34): the lobby's formatted texts (`LobbyFormatString`,
//! `LobbyFormatString2`, `LobbyFormatString3`) fill `<s1>`..`<s9>` tags in a resource string. The original's
//! StringLocalize.cpp: each `<sN>` tag replaced by substitution N (a missing one dropped), `<<` a literal `<`, any
//! other tag dropped up to its `>`; a tag with no `>` keeps the text after its `<`.
//!
//! The C++ keeps the three formatting functions (inline in WONLobbyGUI.h and WONLobbyCore.h), each one call here; the
//! texts cross as GUIStrings read in place and the result is a new text (guistrobj.rs, R6 WON GUI round 39). The class
//! StringLocalize, its header and StringLocalize.cpp are gone.

use std::ffi::c_void;

use crate::guistring::find_char;

/// The unit at `i`, the terminator (0) at and past the end (the original read `mText[i]`, the terminator at the
/// length).
fn at(s: &[u32], i: i32) -> u32 {
    if i < 0 {
        return 0;
    }
    s.get(i as usize).copied().unwrap_or(0)
}

/// `GetStr(text, subs, n)` over units: `subs[i]` the substitution for `<s{i+1}>` (None where the caller gave NULL).
pub fn get_str(text: &[u32], subs: &[Option<Vec<u32>>]) -> Vec<u32> {
    let (lt, gt) = (i32::from(b'<'), i32::from(b'>'));
    let length = text.len() as i32;
    let mut out = Vec::new();
    let mut start = 0i32;
    loop {
        let bracket = find_char(text, lt, start);
        if bracket < 0 {
            break;
        }
        out.extend_from_slice(&text[start as usize..bracket as usize]);
        let next = at(text, bracket + 1);
        if next == b'<' as u32 {
            out.push(b'<' as u32);
            start = bracket + 2;
            continue;
        } else if bracket + 1 < length {
            let id = at(text, bracket + 2) as i32 - i32::from(b'0') - 1;
            // tolower(next) == 's' (the C locale's)
            if (next == b's' as u32 || next == b'S' as u32) && id >= 0 && (id as usize) < subs.len() {
                if let Some(sub) = &subs[id as usize] {
                    out.extend_from_slice(sub);
                }
            }
        }
        start = bracket + 1;
        let close = find_char(text, gt, start);
        if close < 0 {
            break;
        }
        start = close + 1;
    }
    if start < length {
        out.extend_from_slice(&text[start as usize..]);
    }
    out
}

/// `StringLocalize::GetStr(*text, {first, second, third}, count)`: a new text (no reference) with the tags filled; a
/// NULL substitution (or one past `count`) is dropped.
///
/// # Safety
/// `text` and the non-NULL substitutions are live `GUIString`s; `count` is at most 3.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_localize(
    text: *const c_void,
    first: *const c_void,
    second: *const c_void,
    third: *const c_void,
    count: i32,
) -> *mut crate::guistrobj::EeWonGstr {
    use crate::guistrobj::gstring_units;
    // SAFETY: the contract
    let t = unsafe { gstring_units(text) };
    let n = count.clamp(0, 3) as usize;
    let subs: Vec<Option<Vec<u32>>> = [first, second, third][..n]
        .iter()
        // SAFETY: the contract
        .map(|&p| if p.is_null() { None } else { Some(unsafe { gstring_units(p) }) })
        .collect();
    crate::guistrobj::gstr_from_units(&get_str(&t, &subs))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(s: &str) -> Vec<u32> {
        s.chars().map(|c| c as u32).collect()
    }
    fn s(v: &[u32]) -> String {
        v.iter().map(|&c| char::from_u32(c).unwrap()).collect()
    }

    #[test]
    fn tags_are_filled_dropped_and_escaped_as_the_original() {
        let subs = [Some(w("one")), None, Some(w("three"))];
        assert_eq!(s(&get_str(&w("a <s1> b <S3> c <s2>!"), &subs)), "a one b three c !");
        assert_eq!(s(&get_str(&w("<<s1> <x>y <s4>"), &subs)), "<s1> y ");
        assert_eq!(s(&get_str(&w("open <s1 never"), &subs)), "open ones1 never");
        assert_eq!(s(&get_str(&w("end<"), &subs)), "end");
        assert_eq!(s(&get_str(&w("end<<"), &subs)), "end<");
        assert_eq!(s(&get_str(&w("<s"), &subs)), "s");
        assert_eq!(s(&get_str(&w("plain"), &[])), "plain");
        assert!(get_str(&[], &subs).is_empty());
    }
}
