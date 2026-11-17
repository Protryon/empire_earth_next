//! RegKey on config.yaml (the port has no registry): a key path is the block "won.<path>" with the path's '\' or
//! '/' separators turned into '.', a value name one key of that block, a DWORD an integer, binary data a string of
//! lower-case hex digits. Since net round 27 the key is a Rust object ([`EeWonRegKey`]: its block, path and open
//! flag, every method on `PlatConfig*`, ee_plat_core's config.rs); the C++ `RegKey` forwards (RegKey.cpp deleted).

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
    if !text.len().is_multiple_of(2) {
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

// ---- RegKey whole (net round 27): the key's block, path and open flag and every method, on ee_plat_core's
// config.yaml (RegKey.cpp deleted; the C++ class forwards to `EeWonRegKey`)

/// A C string of `s` as `std::string::c_str()` gave it: the bytes up to a NUL inside it.
fn c_text(s: &[u8]) -> std::ffi::CString {
    let cut = s.split(|&c| c == 0).next().unwrap_or(&[]);
    std::ffi::CString::new(cut).unwrap_or_default()
}

/// `RegKey`'s members: `mBlock` (the config.yaml block, was the HKEY), `mIsOpen`, `mPath` (the path it was opened
/// with).
#[derive(Debug, Default, Clone, PartialEq, Eq)]
pub struct EeWonRegKey {
    pub block: Vec<u8>,
    pub open: bool,
    pub path: Vec<u8>,
}

impl EeWonRegKey {
    /// `CloseKey`.
    pub fn close(&mut self) {
        self.open = false;
    }

    /// `OpenBlock(parent, path)`: closed, then the path's block under the parent (a block needs no creating: opening
    /// a missing key succeeds as creating it did); an empty path stays closed.
    pub fn open_block(&mut self, parent: &[u8], path: &[u8]) -> bool {
        self.close();
        if !path.is_empty() {
            self.block = config_block(parent, path);
            self.open = true;
            self.path = path.to_vec();
        }
        self.open
    }

    /// `OpenNewKey(root, path, ...)`: every valid root is the block "won".
    pub fn open_new_key(&mut self, root: i32, path: &[u8]) -> bool {
        if !root_valid(root) {
            self.close();
            return false;
        }
        self.open_block(ROOT_BLOCK, path)
    }

    /// `GetToBuf`: the named value's text through a 65,536-byte buffer (None: missing, a block or too long).
    pub fn get_to_buf(&self, name: &[u8]) -> Option<Vec<u8>> {
        let mut buffer = vec![0u8; 65536];
        let (block, key) = (c_text(&self.block), c_text(&config_name(name)));
        // SAFETY: C strings and a writable buffer of its size
        let ok = unsafe {
            ee_base::plat_core::config::PlatConfigGetString(
                block.as_ptr(),
                key.as_ptr(),
                buffer.as_mut_ptr().cast(),
                65536,
            )
        };
        if ok == 0 {
            return None;
        }
        let end = buffer.iter().position(|&c| c == 0).unwrap_or(buffer.len());
        buffer.truncate(end);
        Some(buffer)
    }

    /// `QueryDWORD`: 0 and the value, 2 for a value that is no integer, 1 when missing (the value 0 then).
    pub fn query_dword(&self, name: &[u8]) -> (i32, u32) {
        if !self.open {
            return (1, 0);
        }
        let (block, key) = (c_text(&self.block), c_text(&config_name(name)));
        let mut number = 0i32;
        // SAFETY: C strings and a writable integer
        if unsafe { ee_base::plat_core::config::PlatConfigGetInt(block.as_ptr(), key.as_ptr(), &mut number) } != 0 {
            return (0, number as u32);
        }
        (if self.get_to_buf(name).is_some() { 2 } else { 1 }, 0)
    }

    /// `QueryBinary`: None (1) when closed or missing, else the value (0 and the bytes, 2 a bad value, 4 empty).
    pub fn query_binary(&self, name: &[u8]) -> Option<Binary> {
        if !self.open {
            return None;
        }
        self.get_to_buf(name).map(|text| binary_value(&text))
    }

    fn delete(block: &[u8], key: Option<&[u8]>) -> bool {
        let block = c_text(block);
        let key = key.map(c_text);
        // SAFETY: C strings (or NULL for the whole block)
        unsafe {
            ee_base::plat_core::config::PlatConfigDelete(
                block.as_ptr(),
                key.as_ref().map_or(std::ptr::null(), |k| k.as_ptr()),
            ) != 0
        }
    }

    /// `DeleteSubKey(name)`.
    pub fn delete_subkey(&self, key: &[u8]) -> bool {
        if key.is_empty() || !self.open {
            return false;
        }
        Self::delete(&config_block(&self.block, key), None)
    }

    /// `DeleteSubKey(RegKey&)`: the child closed (only when this parent is open), then deleted by its path.
    pub fn delete_subkey_of(&self, child: &mut EeWonRegKey) -> bool {
        if !self.open {
            return false;
        }
        child.close();
        Self::delete(&config_block(&self.block, &child.path), None)
    }

    /// `SetValue` of a string, a DWORD (32 bits) or binary data (hex text); false when closed.
    pub fn set_value(&self, name: &[u8], value: RegValue<'_>) -> bool {
        if !self.open {
            return false;
        }
        let (block, key) = (c_text(&self.block), c_text(&config_name(name)));
        // SAFETY: C strings
        unsafe {
            match value {
                RegValue::Text(t) => {
                    let t = c_text(t);
                    ee_base::plat_core::config::PlatConfigSetString(block.as_ptr(), key.as_ptr(), t.as_ptr()) != 0
                }
                RegValue::Dword(n) => {
                    ee_base::plat_core::config::PlatConfigSetInt(block.as_ptr(), key.as_ptr(), n as i32) != 0
                }
                RegValue::Binary(b) => {
                    let t = c_text(&binary_text(b));
                    ee_base::plat_core::config::PlatConfigSetString(block.as_ptr(), key.as_ptr(), t.as_ptr()) != 0
                }
            }
        }
    }

    /// `DeleteValue`.
    pub fn delete_value(&self, name: &[u8]) -> bool {
        self.open && Self::delete(&self.block, Some(&config_name(name)))
    }

    /// `OpenSubKey`: this key becomes its subkey.
    pub fn open_subkey(&mut self, sub: &[u8]) -> bool {
        if sub.is_empty() || !self.open {
            return false;
        }
        self.block = config_block(&self.block, sub);
        self.path = sub.to_vec();
        true
    }

    /// `GetSubKey(name, out)`: `out` opened as this key's subkey.
    pub fn get_subkey(&self, name: &[u8], out: &mut EeWonRegKey) -> bool {
        if name.is_empty() || !self.open {
            return false;
        }
        let block = self.block.clone();
        out.open_block(&block, name)
    }

    /// `GetValue(name, out)`: any value as a string (config.yaml keeps no types); None for NotFound.
    pub fn get_value(&self, name: &[u8]) -> Option<Vec<u8>> {
        if !self.open {
            return None;
        }
        self.get_to_buf(name)
    }
}

/// A value `SetValue` writes.
#[derive(Debug, Clone, Copy)]
pub enum RegValue<'a> {
    Text(&'a [u8]),
    Dword(u32),
    Binary(&'a [u8]),
}

/// `RegKey(root, path, create, nonvolatile)`: a new key opened as `OpenNewKey`.
///
/// # Safety
/// `path` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_new(root: i32, path: *const u8, length: usize) -> *mut EeWonRegKey {
    let mut key = EeWonRegKey::default();
    // SAFETY: the caller's bytes
    key.open_new_key(root, unsafe { bytes(path, length) });
    Box::into_raw(Box::new(key))
}

/// # Safety
/// `key` is from `ee_won_regkey_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_free(key: *mut EeWonRegKey) {
    if !key.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(key) });
    }
}

unsafe fn key<'a>(k: *mut EeWonRegKey) -> &'a mut EeWonRegKey {
    // SAFETY: a key from ee_won_regkey_new (contract)
    unsafe { &mut *k }
}

/// What `ee_won_regkey_op` does.
pub const EE_WON_REGKEY_IS_OPEN: i32 = 0;
pub const EE_WON_REGKEY_CLOSE: i32 = 1;
/// `OpenNewKey(root = value, name)`
pub const EE_WON_REGKEY_OPEN_NEW: i32 = 2;
/// `DeleteSubKey(name)`
pub const EE_WON_REGKEY_DELETE_SUBKEY: i32 = 3;
/// `SetValue(name, text)`, `SetValue(name, value)`, `SetValue(name, bytes)`
pub const EE_WON_REGKEY_SET_TEXT: i32 = 4;
pub const EE_WON_REGKEY_SET_DWORD: i32 = 5;
pub const EE_WON_REGKEY_SET_BINARY: i32 = 6;
/// `DeleteValue(name)`
pub const EE_WON_REGKEY_DELETE_VALUE: i32 = 7;
/// `OpenSubKey(name)`
pub const EE_WON_REGKEY_OPEN_SUBKEY: i32 = 8;
/// `GetToBuf(name, out)`, `GetValue(name, out)` (true: found; the text to the sink)
pub const EE_WON_REGKEY_GET_TO_BUF: i32 = 9;
pub const EE_WON_REGKEY_GET_VALUE: i32 = 10;
/// `QueryDWORD(name, out)`: the code; the value to `*num`
pub const EE_WON_REGKEY_QUERY_DWORD: i32 = 11;
/// `QueryBinary(name, out)`: the code; the bytes to the sink for 0
pub const EE_WON_REGKEY_QUERY_BINARY: i32 = 12;
/// `GetLeafName()`: the path to the sink
pub const EE_WON_REGKEY_LEAF_NAME: i32 = 13;
/// the block (`mBlock`) to the sink
pub const EE_WON_REGKEY_BLOCK: i32 = 14;

/// One of RegKey's methods on `key` (`EE_WON_REGKEY_*`): `name` its string argument, `data` its text or bytes,
/// `*num` its number in and out; results as the method returns them (bools as 0/1), texts to the sink.
///
/// # Safety
/// `k` is a live key; `name` and `data` hold their lengths' bytes (NULL for none); `num` is NULL or writable;
/// `sink` receives bytes during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_op(
    k: *mut EeWonRegKey,
    which: i32,
    name: *const u8,
    name_length: usize,
    data: *const u8,
    data_length: usize,
    num: *mut i64,
    sink: Option<unsafe extern "C" fn(context: *mut c_void, bytes: *const u8, length: usize)>,
    context: *mut c_void,
) -> i32 {
    // SAFETY: the contract
    let (k, name, data) = unsafe { (key(k), bytes(name, name_length), bytes(data, data_length)) };
    let n = if num.is_null() { 0 } else { unsafe { *num } };
    let out = |b: &[u8]| {
        if let Some(s) = sink {
            // SAFETY: the sink's contract
            unsafe { give(s, context, b) }
        }
    };
    match which {
        EE_WON_REGKEY_IS_OPEN => i32::from(k.open),
        EE_WON_REGKEY_CLOSE => {
            k.close();
            0
        }
        EE_WON_REGKEY_OPEN_NEW => i32::from(k.open_new_key(n as i32, name)),
        EE_WON_REGKEY_DELETE_SUBKEY => i32::from(k.delete_subkey(name)),
        EE_WON_REGKEY_SET_TEXT => i32::from(k.set_value(name, RegValue::Text(data))),
        EE_WON_REGKEY_SET_DWORD => i32::from(k.set_value(name, RegValue::Dword(n as u32))),
        EE_WON_REGKEY_SET_BINARY => i32::from(k.set_value(name, RegValue::Binary(data))),
        EE_WON_REGKEY_DELETE_VALUE => i32::from(k.delete_value(name)),
        EE_WON_REGKEY_OPEN_SUBKEY => i32::from(k.open_subkey(name)),
        EE_WON_REGKEY_GET_TO_BUF | EE_WON_REGKEY_GET_VALUE => {
            let v = if which == EE_WON_REGKEY_GET_VALUE { k.get_value(name) } else { k.get_to_buf(name) };
            match v {
                Some(t) => {
                    out(&t);
                    1
                }
                None => 0,
            }
        }
        EE_WON_REGKEY_QUERY_DWORD => {
            let (code, value) = k.query_dword(name);
            if !num.is_null() {
                // SAFETY: the contract
                unsafe { *num = i64::from(value) };
            }
            code
        }
        EE_WON_REGKEY_QUERY_BINARY => match k.query_binary(name) {
            None => 1,
            Some(v) => {
                if let Binary::Bytes(b) = &v {
                    out(b);
                }
                v.code()
            }
        },
        EE_WON_REGKEY_LEAF_NAME => {
            out(&k.path);
            0
        }
        EE_WON_REGKEY_BLOCK => {
            out(&k.block);
            0
        }
        _ => 0,
    }
}

/// `GetSubKey(name, child)` or (`delete`) `DeleteSubKey(child)`; the same key as both is the original's statements on
/// one object.
///
/// # Safety
/// `parent` and `child` are live keys (different ones); `name` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_regkey_child(
    parent: *mut EeWonRegKey,
    child: *mut EeWonRegKey,
    delete: bool,
    name: *const u8,
    length: usize,
) -> bool {
    if parent == child {
        // SAFETY: the contract
        let k = unsafe { key(parent) };
        if delete {
            if !k.open {
                return false;
            }
            k.close();
            return EeWonRegKey::delete(&config_block(&k.block, &k.path), None);
        }
        let mut copy = k.clone();
        // SAFETY: the caller's bytes
        let r = k.get_subkey(unsafe { bytes(name, length) }, &mut copy);
        *k = copy;
        return r;
    }
    // SAFETY: the contract
    let (p, c) = unsafe { (key(parent), key(child)) };
    if delete {
        p.delete_subkey_of(c)
    } else {
        // SAFETY: the caller's bytes
        p.get_subkey(unsafe { bytes(name, length) }, c)
    }
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

    #[test]
    fn the_object_opens_closes_and_descends() {
        let mut k = EeWonRegKey::default();
        assert!(!k.open_new_key(7, b"A"));
        assert!(!k.open_new_key(1, b""));
        assert!(k.open_new_key(1, b"SOFTWARE\\Sierra"));
        assert_eq!(k.block, b"won.SOFTWARE.Sierra");
        assert_eq!(k.path, b"SOFTWARE\\Sierra");
        let mut sub = EeWonRegKey::default();
        assert!(!k.get_subkey(b"", &mut sub));
        assert!(k.get_subkey(b"x/y", &mut sub));
        assert_eq!((sub.block.as_slice(), sub.open), (&b"won.SOFTWARE.Sierra.x.y"[..], true));
        assert!(k.open_subkey(b"Deeper"));
        assert_eq!(k.block, b"won.SOFTWARE.Sierra.Deeper");
        k.close();
        // a closed key reads, writes and descends nothing
        assert!(!k.open_subkey(b"More") && k.block == b"won.SOFTWARE.Sierra.Deeper");
        assert_eq!(k.query_dword(b"v"), (1, 0));
        assert!(k.query_binary(b"v").is_none() && k.get_value(b"v").is_none());
        assert!(!k.set_value(b"v", RegValue::Dword(1)) && !k.delete_value(b"v") && !k.delete_subkey(b"x"));
        assert!(!k.delete_subkey_of(&mut sub) && sub.open);
        assert_eq!(c_text(b"ab\0cd").as_bytes(), b"ab");
    }
}
