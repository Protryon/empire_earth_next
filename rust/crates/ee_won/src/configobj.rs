//! The grammar of WON's config object model (`ConfigObject`, the lobby's .cfg groups over the `config.rs` lexer):
//! what a token is in a group (its end, a `key:` value or a child group), the group header's sequence
//! (`[name] [: base] {`), the instructions (`GROUPDEFINE`, `IFDEFINED`, `IFNOTDEFINED` and the value instructions
//! `DEFINE`, `REDEFINE`, `INCLUDEGROUP`, `INCLUDEFILE`), which branch of a condition is parsed, the `, optional`
//! suffix and an included file's path. The C++ keeps the objects, their key and group maps, the virtual hooks
//! (`HandleInstruction`, `HandleValueInstruction`, `PreParse`, `PostParse`, overridden by WONGUI's resource and
//! component configs), the lexer calls and the error and warning texts, and asks these at each step.

use std::ffi::c_void;

use crate::ffi::{bytes, give, EeWonByteSink};

/// What an upper-cased token is in a group.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub enum EeWonCfgToken {
    /// `}`: the group's end.
    Close,
    /// `name:`: a key's value follows (the key is the token without its last character).
    Key,
    /// Anything else: a child group or an instruction.
    Group,
}

/// `ParseGroup`'s test of a token. An empty token (the original read `token[-1]`) is a group.
pub fn token_kind(token: &[u8]) -> EeWonCfgToken {
    if token == b"}" {
        EeWonCfgToken::Close
    } else if token.last() == Some(&b':') {
        EeWonCfgToken::Key
    } else {
        EeWonCfgToken::Group
    }
}

/// `ReadGroupStart`'s next step after a token.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub enum EeWonCfgStart {
    /// The token is the group's name (read through a StringParser); read the next token.
    Name,
    /// `:` read: the next token is the base name.
    Colon,
    /// The token is the base name; read the next token.
    Base,
    /// `{`: the header is complete.
    Open,
    /// Not `{` where it was expected: "Expecting '{' but got <token>".
    Error,
}

/// The header's state between tokens (0 at its start).
pub fn group_start_step(state: &mut i32, token: &[u8]) -> EeWonCfgStart {
    let open = token == b"{";
    let colon = token == b":";
    let (next, step) = match *state {
        0 if open => (4, EeWonCfgStart::Open),
        0 if colon => (2, EeWonCfgStart::Colon),
        0 => (1, EeWonCfgStart::Name),
        1 if colon => (2, EeWonCfgStart::Colon),
        1 | 3 if open => (4, EeWonCfgStart::Open),
        2 => (3, EeWonCfgStart::Base),
        _ => (4, EeWonCfgStart::Error),
    };
    *state = next;
    step
}

/// `HandleInstruction`'s and `HandleValueInstruction`'s instructions (upper-cased tokens).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub enum EeWonCfgInstruction {
    /// A registered child group, or unknown.
    Other,
    GroupDefine,
    IfDefined,
    IfNotDefined,
    Define,
    Redefine,
    IncludeGroup,
    IncludeFile,
}

/// A group token's instruction (`HandleInstruction`).
pub fn group_instruction(token: &[u8]) -> EeWonCfgInstruction {
    match token {
        b"GROUPDEFINE" => EeWonCfgInstruction::GroupDefine,
        b"IFDEFINED" => EeWonCfgInstruction::IfDefined,
        b"IFNOTDEFINED" => EeWonCfgInstruction::IfNotDefined,
        _ => EeWonCfgInstruction::Other,
    }
}

/// A key's instruction (`HandleValueInstruction`).
pub fn value_instruction(key: &[u8]) -> EeWonCfgInstruction {
    match key {
        b"DEFINE" => EeWonCfgInstruction::Define,
        b"REDEFINE" => EeWonCfgInstruction::Redefine,
        b"INCLUDEGROUP" => EeWonCfgInstruction::IncludeGroup,
        b"INCLUDEFILE" => EeWonCfgInstruction::IncludeFile,
        _ => EeWonCfgInstruction::Other,
    }
}

/// Whether a condition's first block is parsed (its `else` block is the other): `IFDEFINED` when the define
/// exists and matches (`expected` empty, or equal to its value ignoring case: `matches`), `IFNOTDEFINED` when not.
pub fn condition_parses_first(
    instruction: EeWonCfgInstruction,
    found: bool,
    expected_empty: bool,
    matches: bool,
) -> bool {
    let selected = found && (expected_empty || matches);
    if instruction == EeWonCfgInstruction::IfNotDefined {
        !selected
    } else {
        selected
    }
}

/// The token after a condition's block: an `else` (any case) or not (pushed back).
pub fn is_else(token: &[u8]) -> bool {
    token.eq_ignore_ascii_case(b"else")
}

/// The `, optional` suffix of `INCLUDEGROUP` / `INCLUDEFILE`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(C)]
pub enum EeWonCfgOptional {
    /// No suffix, or a bad one (see the warnings): required.
    Required,
    /// `, optional`.
    Optional,
    /// A comma and no string: "Expecting string: 'optional' after include ..., include is required."
    WarnNoString,
    /// A comma and another word: "Expecting string: optional after include ..., include is required."
    WarnOtherWord,
}

/// The suffix: `comma` whether one followed the name, `read` whether a string followed it, `word_is_optional`
/// whether that string is "optional" ignoring case.
pub fn optional_suffix(comma: bool, read: bool, word_is_optional: bool) -> EeWonCfgOptional {
    match (comma, read, word_is_optional) {
        (false, _, _) => EeWonCfgOptional::Required,
        (true, false, _) => EeWonCfgOptional::WarnNoString,
        (true, true, false) => EeWonCfgOptional::WarnOtherWord,
        (true, true, true) => EeWonCfgOptional::Optional,
    }
}

/// `IncludeFile`'s path: the including file's directory (`base`, as the parser keeps it) and the path given,
/// without its directory when the parser ignores include paths (the last '/', else the last '\').
pub fn include_path(path: &[u8], ignore_path: bool, base: &[u8]) -> Vec<u8> {
    let slash = path.iter().rposition(|&c| c == b'/').or_else(|| path.iter().rposition(|&c| c == b'\\'));
    let file = match slash {
        Some(s) if ignore_path => &path[s + 1..],
        _ => path,
    };
    [base, file].concat()
}

// ---- the C ABI (ConfigObject.cpp)

/// `token_kind`.
///
/// # Safety
/// `token` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_token_kind(token: *const u8, length: usize) -> EeWonCfgToken {
    token_kind(unsafe { bytes(token, length) })
}

/// `group_start_step`.
///
/// # Safety
/// `token` holds `length` bytes; `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_group_start(state: &mut i32, token: *const u8, length: usize) -> EeWonCfgStart {
    group_start_step(state, unsafe { bytes(token, length) })
}

/// `group_instruction` (`value` false) or `value_instruction`.
///
/// # Safety
/// `token` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_instruction(token: *const u8, length: usize, value: bool) -> EeWonCfgInstruction {
    let t = unsafe { bytes(token, length) };
    if value {
        value_instruction(t)
    } else {
        group_instruction(t)
    }
}

/// `condition_parses_first`.
#[no_mangle]
pub extern "C" fn ee_won_cfg_condition(
    instruction: EeWonCfgInstruction,
    found: bool,
    expected_empty: bool,
    matches: bool,
) -> bool {
    condition_parses_first(instruction, found, expected_empty, matches)
}

/// `is_else`.
///
/// # Safety
/// `token` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_is_else(token: *const u8, length: usize) -> bool {
    is_else(unsafe { bytes(token, length) })
}

/// `optional_suffix`.
#[no_mangle]
pub extern "C" fn ee_won_cfg_optional(comma: bool, read: bool, word_is_optional: bool) -> EeWonCfgOptional {
    optional_suffix(comma, read, word_is_optional)
}

/// `include_path`.
///
/// # Safety
/// `path` and `base` hold their lengths' bytes; `sink` receives the path during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_include_path(
    path: *const u8,
    path_length: usize,
    ignore_path: bool,
    base: *const u8,
    base_length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let p = include_path(unsafe { bytes(path, path_length) }, ignore_path, unsafe { bytes(base, base_length) });
    unsafe { give(sink, context, &p) }
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden_cfgobj.txt");

    fn line(case: &str) -> &'static str {
        GOLDEN.lines().find(|l| l.starts_with(&format!("{case} r="))).unwrap()
    }

    fn root(case: &str) -> &'static str {
        GOLDEN.lines().find(|l| l.starts_with(&format!("{case} name="))).unwrap()
    }

    fn run(tokens: &[&str]) -> Vec<EeWonCfgStart> {
        let mut s = 0;
        tokens.iter().map(|t| group_start_step(&mut s, t.as_bytes())).collect()
    }

    #[test]
    fn group_headers() {
        use EeWonCfgStart::*;
        assert_eq!(run(&["{"]), [Open]);
        assert_eq!(run(&["Named", "{"]), [Name, Open]);
        assert_eq!(run(&[":", "Base", "{"]), [Colon, Base, Open]);
        assert_eq!(run(&["Named2", ":", "Base2", "{"]), [Name, Colon, Base, Open]);
        assert_eq!(run(&["Text:"]), [Name]);
        assert_eq!(run(&["Text:", "x"]), [Name, Error]);
        assert_eq!(run(&[":", "}", "x"]), [Colon, Base, Error]);
        // the golden's headers: s07 names and bases, s13 / s14 / s43 the errors' texts
        assert!(root("s07 root.0").contains("name=Named base= "));
        assert!(root("s07 root.1").contains("name= base=Base "));
        assert!(root("s07 root.2").contains("name=Named2 base=Base2 "));
        assert!(line("s13").contains("error=Expecting '{' but got x "));
        assert!(line("s14").contains("error=Expecting '{' but got } "));
        assert!(line("s43").contains("error=Expecting '{' but got Name "));
    }

    #[test]
    fn tokens_instructions_and_conditions() {
        assert_eq!(token_kind(b"}"), EeWonCfgToken::Close);
        assert_eq!(token_kind(b"TEXT:"), EeWonCfgToken::Key);
        assert_eq!(token_kind(b":"), EeWonCfgToken::Key);
        assert_eq!(token_kind(b"CHILD"), EeWonCfgToken::Group);
        assert_eq!(token_kind(b""), EeWonCfgToken::Group);
        assert_eq!(group_instruction(b"IFDEFINED"), EeWonCfgInstruction::IfDefined);
        assert_eq!(group_instruction(b"DEFINE"), EeWonCfgInstruction::Other);
        assert_eq!(value_instruction(b"REDEFINE"), EeWonCfgInstruction::Redefine);
        assert_eq!(value_instruction(b"IFDEFINED"), EeWonCfgInstruction::Other);
        let (d, n) = (EeWonCfgInstruction::IfDefined, EeWonCfgInstruction::IfNotDefined);
        // s18..s24: which branch the golden took
        assert!(condition_parses_first(d, true, true, false) && root("s18 root").contains("Text: defined%0a"));
        assert!(!condition_parses_first(d, false, true, false) && root("s19 root").contains("Text: undefined%0a"));
        assert!(condition_parses_first(d, true, false, true) && root("s21 root").contains("Text: fast%0a"));
        assert!(!condition_parses_first(d, true, false, false) && root("s22 root").contains("Text: other%0a"));
        assert!(condition_parses_first(n, false, true, false) && root("s23 root").contains("Text: notdefined%0a"));
        assert!(!condition_parses_first(n, true, true, false) && root("s24 root").contains("Text: defined%0a"));
        assert!(is_else(b"ELSE") && is_else(b"else") && !is_else(b"elsewhere"));
        assert!(line("s25").contains("error=Expecting '{' "));
    }

    #[test]
    fn suffixes_and_paths() {
        assert_eq!(optional_suffix(false, false, false), EeWonCfgOptional::Required);
        assert_eq!(optional_suffix(true, true, true), EeWonCfgOptional::Optional);
        assert_eq!(optional_suffix(true, true, false), EeWonCfgOptional::WarnOtherWord);
        assert_eq!(optional_suffix(true, false, false), EeWonCfgOptional::WarnNoString);
        assert!(line("s27").contains("Expecting string: optional after include group, include is required."));
        assert!(line("s32").contains("Expecting string: optional after include file, include is required."));
        assert_eq!(include_path(b"dir/x.cfg", false, b"base\\"), b"base\\dir/x.cfg");
        assert_eq!(include_path(b"dir/x.cfg", true, b"base\\"), b"base\\x.cfg");
        assert_eq!(include_path(b"a\\b/c\\x.cfg", true, b""), b"c\\x.cfg");
        assert_eq!(include_path(b"a\\x.cfg", true, b""), b"x.cfg");
        // s29 (the path kept) and s29i (dropped): the file read
        assert!(root("s29 root").contains("Text: from dir%0a"));
        assert!(root("s29i root").contains("Text: included%0a"));
    }
}
