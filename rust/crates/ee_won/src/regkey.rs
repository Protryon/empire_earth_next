//! RegKey on config.yaml (the port has no registry): a key path is the block "won.<path>" with the path's '\' or
//! '/' separators turned into '.', a value name one key of that block, a DWORD an integer, binary data a string of
//! lower-case hex digits. These are the names and the binary values; the C++ `RegKey` keeps its block, path and
//! open flag and calls `PlatConfig*` (ee_plat_core's config.rs) with what these give.

use std::ffi::c_void;

use crate::ffi::{bytes, give, EeWonByteSink};

/// The block every root maps to.
pub const ROOT_BLOCK: &[u8] = b"won";

/// A name as one config.yaml key: '.', ':' and control characters become '_', the blanks around it go, and an
/// empty (or blank) name is the key's unnamed value "(default)".
pub fn config_name(name: &[u8]) -> Vec<u8> {
    let mapped: Vec<u8> = name.iter().map(|&c| if c == b'.' || c == b':' || c < 0x20 { b'_' } else { c }).collect();
    match (mapped.iter().position(|&c| c != b' '), mapped.iter().rposition(|&c| c != b' ')) {
        (Some(a), Some(b)) => mapped[a..=b].to_vec(),
        _ => b"(default)".to_vec(),
    }
}

/// The block of `path` (a registry path) under `parent`: each part between separators, empty parts skipped, as
/// a `config_name`.
pub fn config_block(parent: &[u8], path: &[u8]) -> Vec<u8> {
    let mut block = parent.to_vec();
    for part in path.split(|&c| c == b'\\' || c == b'/') {
        if !part.is_empty() {
            block.push(b'.');
            block.extend(config_name(part));
        }
    }
    block
}

/// `SetValue`'s binary data as text: two lower-case hex digits a byte.
pub fn binary_text(data: &[u8]) -> Vec<u8> {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    data.iter().flat_map(|&b| [HEX[usize::from(b >> 4)], HEX[usize::from(b & 15)]]).collect()
}

/// `QueryBinary`'s answer for a value's text.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Binary {
    /// 0: the bytes.
    Bytes(Vec<u8>),
    /// 2: an odd length or a character that is not a hex digit.
    BadType,
    /// 4: no bytes (the caller's buffer is left alone).
    Empty,
}

impl Binary {
    /// The original's return code.
    pub fn code(&self) -> i32 {
        match self {
            Binary::Bytes(_) => 0,
            Binary::BadType => 2,
            Binary::Empty => 4,
        }
    }
}

fn hex_digit(c: u8) -> Option<u8> {
    match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        b'A'..=b'F' => Some(c - b'A' + 10),
        _ => None,
    }
}

/// A value's text as binary data (either case of hex digit).
pub fn binary_value(text: &[u8]) -> Binary {
    if text.len() % 2 != 0 {
        return Binary::BadType;
    }
    let mut out = Vec::with_capacity(text.len() / 2);
    for pair in text.chunks(2) {
        match (hex_digit(pair[0]), hex_digit(pair[1])) {
            (Some(h), Some(l)) => out.push(h * 16 + l),
            _ => return Binary::BadType,
        }
    }
    if out.is_empty() {
        Binary::Empty
    } else {
        Binary::Bytes(out)
    }
}

/// `OpenNewKey`'s roots: 0 (classes root) to 6 (dyn data), all the same block.
pub fn root_valid(root: i32) -> bool {
    (0..=6).contains(&root)
}

// ---- the C ABI (RegKey.cpp)

/// `config_name`.
///
/// # Safety
/// `name` holds `length` bytes; `sink` receives the result during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_name(name: *const u8, length: usize, sink: EeWonByteSink, context: *mut c_void) {
    unsafe { give(sink, context, &config_name(bytes(name, length))) }
}

/// `config_block`; `parent` NULL for the root block.
///
/// # Safety
/// `parent` (when not NULL) and `path` hold their lengths' bytes; `sink` as `ee_won_regkey_name`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_block(
    parent: *const u8,
    parent_length: usize,
    path: *const u8,
    path_length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let parent = if parent.is_null() { ROOT_BLOCK } else { unsafe { bytes(parent, parent_length) } };
    unsafe { give(sink, context, &config_block(parent, bytes(path, path_length))) }
}

/// `binary_text`.
///
/// # Safety
/// `data` holds `length` bytes; `sink` as `ee_won_regkey_name`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_binary_text(
    data: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    unsafe { give(sink, context, &binary_text(bytes(data, length))) }
}

/// `binary_value`: its code; the sink gets the bytes only for 0.
///
/// # Safety
/// `text` holds `length` bytes; `sink` as `ee_won_regkey_name`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_binary_value(
    text: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> i32 {
    let value = binary_value(unsafe { bytes(text, length) });
    if let Binary::Bytes(b) = &value {
        unsafe { give(sink, context, b) };
    }
    value.code()
}

/// `root_valid`.
#[no_mangle]
pub extern "C" fn ee_won_regkey_root_valid(root: i32) -> bool {
    root_valid(root)
}

#[cfg(test)]
mod tests {
    use super::*;

    const GOLDEN: &str = include_str!("golden_regkey.txt");

    fn hex(s: &[u8]) -> String {
        if s.is_empty() {
            "-".into()
        } else {
            s.iter().map(|b| format!("{b:02x}")).collect()
        }
    }

    fn golden(prefix: &str) -> &'static str {
        GOLDEN.lines().find(|l| l.starts_with(prefix)).unwrap_or_else(|| panic!("no golden line {prefix}"))
    }

    #[test]
    fn the_blocks_match_the_cpp() {
        let block = |p: &[u8]| config_block(ROOT_BLOCK, p);
        let line = |case: &str, b: &[u8], p: &[u8]| format!("{case} open=1 block={} path={}", hex(b), hex(p));
        let lobby = block(b"SOFTWARE\\Sierra\\WONLobby");
        assert_eq!(golden("open "), line("open", &lobby, b"SOFTWARE\\Sierra\\WONLobby"));
        assert_eq!(golden("sub open"), line("sub", &config_block(&lobby, b"Sub\\Key"), b"Sub\\Key"));
        assert_eq!(golden("opened "), line("opened", &config_block(&lobby, b"Deeper/Still"), b"Deeper/Still"));
        let sep = b"\\\\lead\\\\\\trail\\";
        assert_eq!(golden("separators "), line("separators", &block(sep), sep));
        let slashes = b"SOFTWARE/Sierra/WONLobby";
        assert_eq!(golden("slashes open"), line("slashes", &block(slashes), slashes));
        assert_eq!(golden("reopened open"), line("reopened", &block(b"Other"), b"Other"));
        // a key opened at the root, closed by its parent's DeleteSubKey (which deleted parent.WONLobby.Sub)
        assert_eq!(
            golden("child "),
            format!("child open=0 block={} path={}", hex(&block(b"WONLobby\\Sub")), hex(b"WONLobby\\Sub"))
        );
        assert!(root_valid(0) && root_valid(6) && !root_valid(7) && !root_valid(-1));
    }

    #[test]
    fn the_binary_values_match_the_cpp() {
        // the golden's "binary=<code>/<bytes>" for each text the script stored
        for (case, text) in [
            ("hextext ", &b"0a1B"[..]),
            ("oddtext ", b"abc"),
            ("badhex ", b"zz"),
            ("numtext ", b"42"),
            ("binary ", b"00017f80ff"),
            ("empty ", b""),
            ("emptytext ", b""),
        ] {
            let v = binary_value(text);
            let bytes = match &v {
                Binary::Bytes(b) => hex(b),
                _ => "none".into(),
            };
            assert!(golden(case).ends_with(&format!(" binary={}/{bytes}", v.code())), "{case}");
        }
        assert_eq!(binary_text(&[0x00, 0x01, 0x7f, 0x80, 0xff]), b"00017f80ff");
        assert_eq!(binary_text(&[]), b"");
    }

    // tests/won/won_regkey_test.cpp's kNames
    const NAMES: &[&[u8]] = &[
        b"",
        b" ",
        b"Plain",
        b"  padded  ",
        b"dot.ted",
        b"co:lon",
        b"tab\there",
        b"\x01\x1f",
        b"Sierra\\WONLobby",
        b"a/b",
        b"\xc3\xa9t\xc3\xa9",
        b"UseLocalIP",
        b"x . y",
    ];

    #[test]
    fn the_value_names_match_the_cpp() {
        // the file's "<key>: nameNN" lines: name NN was written under that key (the last write of a key shows)
        let mut seen = 0;
        for line in GOLDEN.lines().filter_map(|l| l.strip_prefix("yaml         ")) {
            let Some((key, value)) = line.rsplit_once(": ") else { continue };
            let Some(n) = value.strip_prefix("name").and_then(|n| n.parse::<usize>().ok()) else { continue };
            assert_eq!(config_name(NAMES[n]), key.as_bytes(), "name{n:02}");
            seen += 1;
        }
        assert_eq!(seen, 12); // 00 and 01 share "(default)"
    }

    #[test]
    fn names() {
        assert_eq!(config_name(b""), b"(default)");
        assert_eq!(config_name(b"   "), b"(default)");
        assert_eq!(config_name(b"  padded  "), b"padded");
        assert_eq!(config_name(b"dot.ted"), b"dot_ted");
        assert_eq!(config_name(b"co:lon"), b"co_lon");
        assert_eq!(config_name(b"tab\there"), b"tab_here");
        assert_eq!(config_name(b"\x01\x1f"), b"__");
        assert_eq!(config_name(b"x . y"), b"x _ y");
        assert_eq!(config_name("été".as_bytes()), "été".as_bytes());
        assert_eq!(config_block(b"won", b""), b"won");
        assert_eq!(config_block(b"won", b"a.b\\ c "), b"won.a_b.c");
        assert_eq!(config_block(b"won", b"\\ \\x"), b"won.(default).x");
    }
}
