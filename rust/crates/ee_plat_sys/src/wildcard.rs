//! ASCII case-insensitive compares for the directory search: `PlatPosix::CompareNoCase` / `WildcardMatch` of the C++
//! (NTFS orders names close to an upper-case ordinal compare). Used on POSIX only; tested on every system.

/// ASCII case-insensitive compare, as `strcmp` over `toupper`ed bytes.
pub fn compare_no_case(left: &[u8], right: &[u8]) -> i32 {
    let mut i = 0;
    loop {
        let a = left.get(i).copied().unwrap_or(0).to_ascii_uppercase() as i32;
        let b = right.get(i).copied().unwrap_or(0).to_ascii_uppercase() as i32;
        if a != b || a == 0 {
            return a - b;
        }
        i += 1;
    }
}

/// Case-insensitive match of a name against a Windows pattern (`*` and `?`; `*.*` matches everything).
pub fn wildcard_match(pattern: &[u8], name: &[u8]) -> bool {
    if pattern == b"*.*" || pattern == b"*" {
        return true;
    }
    let at = |s: &[u8], i: usize| s.get(i).copied().unwrap_or(0);
    let mut star: Option<usize> = None;
    let mut resume = 0;
    let (mut p, mut n) = (0, 0);
    while n < name.len() {
        let c = at(pattern, p);
        if c == b'*' {
            star = Some(p);
            p += 1;
            resume = n;
        } else if c == b'?' || c.eq_ignore_ascii_case(&name[n]) {
            p += 1;
            n += 1;
        } else if let Some(s) = star {
            p = s + 1;
            resume += 1;
            n = resume;
        } else {
            return false;
        }
    }
    // the name is used up: what is left of the pattern must match nothing (stars; "?" and a final "." match the end,
    // as Win32's matcher does: "name.*" matches "name", "a?" matches "a")
    loop {
        let c = at(pattern, p);
        if c == b'*' || c == b'?' || (c == b'.' && (at(pattern, p + 1) == b'*' || at(pattern, p + 1) == 0)) {
            p += 1;
        } else {
            return c == 0;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    // Golden results of PlatPosix::WildcardMatch (src/Platform/Posix/PlatPath.cpp).
    #[test]
    fn wildcard_golden() {
        for (pattern, name, expected) in [
            ("*.*", "anything", true),
            ("*", "", true),
            ("*.dat", "A.DAT", true),
            ("*.DAT", "unit.dat", true),
            ("*.dat", "unit.txt", false),
            ("a?c", "abc", true),
            ("a?c", "ac", false),
            ("a?", "a", true),
            ("name.*", "name", true),
            ("name.*", "name.txt", true),
            ("name.", "name", true),
            ("na*e", "name", true),
            ("na*e", "nam", false),
            ("*a*b", "xxaxxb", true),
            ("*a*b", "xxaxxc", false),
            ("?", "", true),
            ("", "x", false),
            ("x", "", false),
            ("*.none", "a.dat", false),
        ] {
            assert_eq!(wildcard_match(pattern.as_bytes(), name.as_bytes()), expected, "{pattern} vs {name}");
        }
    }

    #[test]
    fn compare_orders_case_insensitively() {
        assert_eq!(compare_no_case(b"Hello", b"hELLO"), 0);
        assert!(compare_no_case(b"abc", b"ABD") < 0);
        assert!(compare_no_case(b"b", b"A") > 0);
        assert!(compare_no_case(b"ab", b"abc") < 0);
        assert!(compare_no_case(b"abc", b"ab") > 0);
        // upper-casing: '_' (0x5f) sorts after 'A'..'Z' (0x41..0x5a)
        assert!(compare_no_case(b"_", b"a") > 0);
    }
}
