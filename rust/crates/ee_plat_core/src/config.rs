//! `platform/PlatConfig.h`: the configuration file config.yaml, a hand-written strict subset of YAML (no serde_yaml).
//! One implementation on every system: the C runtime's stdio for the file (its bytes are the path, as the game's
//! other code opens files), the data root (`path`) for its place. The whole file is kept in memory as a tree of
//! blocks, loaded on first use and rewritten after every change through "<file>.new", which then replaces it.
//!
//! `parse` and `render` are pure and carry the format; `Store` is the tree and its file, and takes the file's path,
//! so tests use their own files; the exported functions at the end use one shared `Store` under a lock.

use std::ffi::{c_char, c_int, CStr};
use std::sync::{Mutex, PoisonError};

use crate::c_text::{bytes, terminated};

// ---- the tree

/// A key: a value, or a block of keys.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub(crate) struct Node {
    name: Vec<u8>,
    /// A value: as the game set it (unquoted).
    text: Vec<u8>,
    block: bool,
    /// A value set as a number or boolean: never quoted.
    plain: bool,
    /// A block's keys in their first-written order.
    children: Vec<Node>,
}

impl Node {
    const fn root() -> Self {
        Self { name: Vec::new(), text: Vec::new(), block: true, plain: false, children: Vec::new() }
    }

    fn named(name: &[u8]) -> Self {
        Self { name: name.to_vec(), ..Self::default() }
    }

    fn find_child(&self, name: &[u8]) -> Option<usize> {
        self.children.iter().position(|child| child.name.eq_ignore_ascii_case(name))
    }

    /// The child called `name`, a new one at the end when there is none.
    fn child_or_new(&mut self, name: &[u8]) -> &mut Node {
        let index = match self.find_child(name) {
            Some(index) => index,
            None => {
                self.children.push(Node::named(name));
                self.children.len() - 1
            }
        };
        &mut self.children[index]
    }

    fn set_value(&mut self, text: Vec<u8>, plain: bool) {
        self.block = false;
        self.children.clear();
        self.text = text;
        self.plain = plain;
    }

    /// The block at `path` ("a.b"; empty for this one); `create` makes the missing blocks (a value in the way is
    /// replaced by a block). None when it is missing or a part of the path is empty.
    fn find_block(&mut self, path: &[u8], create: bool) -> Option<&mut Node> {
        if path.is_empty() {
            return Some(self);
        }
        let end = path.iter().position(|&c| c == b'.').unwrap_or(path.len());
        if end == 0 {
            return None;
        }
        let name = &path[..end];
        if !create && self.find_child(name).is_none() {
            return None;
        }
        let child = self.child_or_new(name);
        if !child.block {
            if !create {
                return None;
            }
            child.block = true;
            child.text.clear();
        }
        let rest = if end < path.len() { &path[end + 1..] } else { &path[end..] };
        child.find_block(rest, create)
    }

    fn find_value(&mut self, path: &[u8], key: &[u8]) -> Option<&Node> {
        let block = self.find_block(path, false)?;
        block.find_child(key).map(|index| &block.children[index]).filter(|node| !node.block)
    }
}

// ---- the text format

const TRUE_WORDS: [&[u8]; 3] = [b"true", b"yes", b"on"];
const FALSE_WORDS: [&[u8]; 3] = [b"false", b"no", b"off"];
const NULL_WORDS: [&[u8]; 2] = [b"null", b"~"];

fn is_word(text: &[u8], words: &[&[u8]]) -> bool {
    words.iter().any(|word| text.eq_ignore_ascii_case(word))
}

fn trim(text: &[u8]) -> &[u8] {
    let blank = |c: &u8| *c == b' ' || *c == b'\t';
    let start = text.iter().position(|c| !blank(c)).unwrap_or(text.len());
    let end = text.iter().rposition(|c| !blank(c)).map_or(start, |last| last + 1);
    &text[start..end]
}

fn find(text: &[u8], pattern: &[u8]) -> Option<usize> {
    text.windows(pattern.len()).position(|window| window == pattern)
}

fn hex_digit(c: u8) -> Option<u8> {
    match c {
        b'0'..=b'9' => Some(c - b'0'),
        b'a'..=b'f' => Some(c - b'a' + 10),
        b'A'..=b'F' => Some(c - b'A' + 10),
        _ => None,
    }
}

/// The text after "key:" -> the value; None for a malformed quoted string. A comment (" #" outside quotes) ends it.
fn parse_value(raw: &[u8]) -> Option<Vec<u8>> {
    let text = trim(raw);
    let mut out = Vec::new();
    if text.is_empty() {
        return Some(out);
    }
    if text[0] == b'"' || text[0] == b'\'' {
        let quote = text[0];
        let mut i = 1;
        while i < text.len() {
            let c = text[i];
            if quote == b'\'' {
                if c == b'\'' {
                    if text.get(i + 1) == Some(&b'\'') {
                        out.push(b'\'');
                        i += 2;
                        continue;
                    }
                    break;
                }
                out.push(c);
                i += 1;
                continue;
            }
            if c == b'"' {
                break;
            }
            if c != b'\\' {
                out.push(c);
                i += 1;
                continue;
            }
            i += 1;
            let escaped = *text.get(i)?;
            match escaped {
                b'n' => out.push(b'\n'),
                b'r' => out.push(b'\r'),
                b't' => out.push(b'\t'),
                b'0' => out.push(0),
                b'x' => {
                    // an "\x" not followed by two hex digits is an x, as C++ wrote it
                    let digits = (i + 2 < text.len())
                        .then(|| Some((hex_digit(text[i + 1])?, hex_digit(text[i + 2])?)))
                        .flatten();
                    if let Some((high, low)) = digits {
                        out.push(high * 16 + low);
                        i += 2;
                    } else {
                        out.push(b'x');
                    }
                }
                other => out.push(other), // \" \\ \/ and any other escaped character: itself
            }
            i += 1;
        }
        if i >= text.len() {
            return None; // no closing quote
        }
        let rest = trim(&text[i + 1..]);
        return (rest.is_empty() || rest[0] == b'#').then_some(out);
    }
    let comment = [find(text, b" #"), find(text, b"\t#")].into_iter().flatten().min();
    let body = comment.map_or(text, |at| trim(&text[..at]));
    if !is_word(body, &NULL_WORDS) {
        out = body.to_vec();
    }
    Some(out)
}

/// Whether a string reads back as itself without quotes.
fn is_plain(value: &[u8]) -> bool {
    let (Some(&first), Some(&last)) = (value.first(), value.last()) else {
        return false;
    };
    if first == b' ' || first == b'\t' || last == b' ' || last == b'\t' {
        return false;
    }
    // the C++ used strchr, which also finds a NUL first byte
    if b"-?:,[]{}#&*!|>'\"%@`\0".contains(&first) || first.is_ascii_digit() || first == b'+' || first == b'.' {
        return false;
    }
    if is_word(value, &TRUE_WORDS) || is_word(value, &FALSE_WORDS) || is_word(value, &NULL_WORDS) {
        return false;
    }
    if find(value, b": ").is_some() || find(value, b" #").is_some() || last == b':' {
        return false;
    }
    value.iter().all(|&c| c >= 0x20 && c != 0x7f)
}

fn quote(value: &[u8]) -> Vec<u8> {
    if is_plain(value) {
        return value.to_vec();
    }
    const HEX: &[u8; 16] = b"0123456789abcdef";
    let mut out = vec![b'"'];
    for &c in value {
        match c {
            b'"' | b'\\' => out.extend_from_slice(&[b'\\', c]),
            b'\n' => out.extend_from_slice(b"\\n"),
            b'\r' => out.extend_from_slice(b"\\r"),
            b'\t' => out.extend_from_slice(b"\\t"),
            0..0x20 | 0x7f => out.extend_from_slice(&[b'\\', b'x', HEX[(c >> 4) as usize], HEX[(c & 15) as usize]]),
            _ => out.push(c),
        }
    }
    out.push(b'"');
    out
}

/// One line -> its indent, key and the text after "key:"; None for a line to skip: blank, comment, a tab indent, a
/// document marker, no "key:".
fn split_line(line: &[u8]) -> Option<(i32, Vec<u8>, &[u8])> {
    let indent = line.iter().position(|&c| c != b' ')?;
    let tail = &line[indent..];
    if tail[0] == b'#' || tail[0] == b'\t' || tail.starts_with(b"---") || tail.starts_with(b"...") {
        return None;
    }
    // the key ends at the first ':' followed by a blank or the end of the line
    let colon = (0..tail.len())
        .find(|&i| tail[i] == b':' && (i + 1 == tail.len() || tail[i + 1] == b' ' || tail[i + 1] == b'\t'))?;
    let mut key = trim(&tail[..colon]).to_vec();
    if key.len() >= 2 && (key[0] == b'"' || key[0] == b'\'') {
        if let Some(unquoted) = parse_value(&key) {
            key = unquoted;
        }
    }
    if key.is_empty() {
        return None;
    }
    Some((indent as i32, key, &tail[colon + 1..]))
}

/// The tree of a file's bytes (a missing file is an empty tree; lines that do not parse are ignored).
pub(crate) fn parse(data: &[u8]) -> Node {
    let mut root = Node::root();
    // the open blocks below the root: their indents and child indices
    let mut open: Vec<(i32, usize)> = Vec::new();
    let mut first = true;
    for line in data.split(|&c| c == b'\n') {
        let line: Vec<u8> = line.iter().copied().filter(|&c| c != b'\r').collect();
        let mut text = line.as_slice();
        if first && text.starts_with(b"\xef\xbb\xbf") {
            text = &text[3..]; // a UTF-8 byte order mark
        }
        first = false;
        let Some((indent, key, rest)) = split_line(text) else {
            continue;
        };
        while open.last().is_some_and(|&(open_indent, _)| open_indent >= indent) {
            open.pop();
        }
        let mut block = &mut root;
        for &(_, index) in &open {
            block = &mut block.children[index];
        }
        let trimmed = trim(rest);
        if trimmed.first().is_none_or(|&c| c == b'#') {
            // "key:" opens a block (a value of that name is replaced)
            let node = block.child_or_new(&key);
            node.block = true;
            node.text.clear();
            let index = block.find_child(&key).expect("just found or added");
            open.push((indent, index));
            continue;
        }
        let Some(value) = parse_value(rest) else {
            continue;
        };
        let plain = trimmed[0] != b'"' && trimmed[0] != b'\'';
        block.child_or_new(&key).set_value(value, plain);
    }
    root
}

const HEADER: &[u8] =
    b"# Empire Earth AoC settings. The game rewrites this file whenever a setting changes (comments are not kept).\n";

fn render_block(out: &mut Vec<u8>, block: &Node, indent: usize) {
    for (index, child) in block.children.iter().enumerate() {
        if child.block && child.children.is_empty() {
            continue; // an empty block: dropped
        }
        if indent == 0 && child.block && index != 0 {
            out.push(b'\n'); // a blank line before every top-level block
        }
        out.resize(out.len() + indent, b' ');
        out.extend_from_slice(&quote(&child.name));
        out.push(b':');
        if child.block {
            out.push(b'\n');
            render_block(out, child, indent + 2);
        } else {
            out.push(b' ');
            if child.plain {
                // as written through %s: a NUL ends it (a raw NUL from a hand-edited file)
                out.extend_from_slice(child.text.split(|&c| c == 0).next().unwrap_or(&[]));
            } else {
                out.extend_from_slice(&quote(&child.text));
            }
            out.push(b'\n');
        }
    }
}

/// The file's bytes for a tree.
pub(crate) fn render(root: &Node) -> Vec<u8> {
    let mut out = HEADER.to_vec();
    render_block(&mut out, root, 0);
    out
}

/// A key the game may set: no separator or path dot, no blanks around it, no control characters, not "...:".
fn valid_key(key: &[u8]) -> bool {
    let (Some(&first), Some(&last)) = (key.first(), key.last()) else {
        return false;
    };
    if first == b' ' || last == b' ' || key.contains(&b'.') {
        return false;
    }
    let colon_end = |i: usize| key[i] == b':' && (i + 1 == key.len() || key[i + 1] == b' ');
    key.iter().all(|&c| c >= 0x20) && !(0..key.len()).any(colon_end)
}

// ---- integers: PlatConfigGetInt reads with strtoll base 0

/// strtoll(text, &end, 0): the value and how many bytes it used; None when there are no digits; `value` saturates and
/// `overflowed` is set past the range, as strtoll does and as errno would say.
fn strtoll_base0(text: &[u8]) -> Option<(i64, usize, bool)> {
    let mut i = 0;
    while text.get(i).is_some_and(|&c| matches!(c, b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r')) {
        i += 1;
    }
    let negative = text.get(i) == Some(&b'-');
    if matches!(text.get(i), Some(b'-' | b'+')) {
        i += 1;
    }
    let mut radix = 10u64;
    if text.get(i) == Some(&b'0') {
        radix = 8;
        // 0x introduces hex only when a hex digit follows; otherwise the "0" stands alone
        if matches!(text.get(i + 1), Some(b'x' | b'X')) && text.get(i + 2).copied().and_then(hex_digit).is_some() {
            radix = 16;
            i += 2;
        }
    }
    let digits_start = i;
    let mut magnitude: u64 = 0;
    let mut overflowed = false;
    while let Some(digit) = text.get(i).copied().and_then(|c| match c {
        b'0'..=b'9' if ((c - b'0') as u64) < radix => Some((c - b'0') as u64),
        b'a'..=b'f' if radix == 16 => Some((c - b'a' + 10) as u64),
        b'A'..=b'F' if radix == 16 => Some((c - b'A' + 10) as u64),
        _ => None,
    }) {
        magnitude = match magnitude.checked_mul(radix).and_then(|m| m.checked_add(digit)) {
            Some(next) => next,
            None => {
                overflowed = true;
                u64::MAX
            }
        };
        i += 1;
    }
    if i == digits_start {
        return None;
    }
    let (value, range_overflow) = if negative {
        match i64::try_from(magnitude) {
            Ok(m) => (-m, false),
            Err(_) if magnitude == 1 << 63 => (i64::MIN, false),
            Err(_) => (i64::MIN, true),
        }
    } else {
        i64::try_from(magnitude).map_or((i64::MAX, true), |m| (m, false))
    };
    Some((value, i, overflowed || range_overflow))
}

/// A value's text as an integer (`PlatConfigGetInt`: true/false as 1/0, decimal, 0x hex, 0 octal; 0x80000000..0xffffffff
/// are the bit pattern).
fn parse_int(text: &[u8]) -> Option<i32> {
    if is_word(text, &TRUE_WORDS) {
        return Some(1);
    }
    if is_word(text, &FALSE_WORDS) {
        return Some(0);
    }
    let (value, used, overflowed) = strtoll_base0(text)?;
    if used != text.len() || overflowed || !(-2_147_483_648..=4_294_967_295).contains(&value) {
        return None;
    }
    Some(value as u32 as i32) // 0x80000000..0xffffffff: the bit pattern
}

// ---- the file

const SEPARATOR: u8 = if cfg!(windows) { b'\\' } else { b'/' };

fn c_string(path: &[u8]) -> Vec<u8> {
    terminated(path)
}

fn read_file(path: &[u8]) -> Option<Vec<u8>> {
    let c_path = c_string(path);
    // SAFETY: both arguments are NUL-terminated.
    let file = unsafe { libc::fopen(c_path.as_ptr().cast(), c"rb".as_ptr()) };
    if file.is_null() {
        return None;
    }
    let mut data = Vec::new();
    let mut buffer = [0u8; 16 * 1024];
    loop {
        // SAFETY: `buffer` is writable for its length; `file` is open.
        let n = unsafe { libc::fread(buffer.as_mut_ptr().cast(), 1, buffer.len(), file) };
        data.extend_from_slice(&buffer[..n]);
        if n == 0 {
            break;
        }
    }
    // SAFETY: `file` is open.
    unsafe { libc::fclose(file) };
    Some(data)
}

fn file_exists(path: &[u8]) -> bool {
    let c_path = c_string(path);
    // SAFETY: both arguments are NUL-terminated.
    let file = unsafe { libc::fopen(c_path.as_ptr().cast(), c"rb".as_ptr()) };
    if !file.is_null() {
        // SAFETY: just opened.
        unsafe { libc::fclose(file) };
    }
    !file.is_null()
}

#[cfg(windows)]
fn replace_file(temporary: &[u8], path: &[u8]) -> bool {
    use windows_sys::Win32::Storage::FileSystem::{MoveFileExA, MOVEFILE_REPLACE_EXISTING};
    let (from, to) = (c_string(temporary), c_string(path));
    // SAFETY: both are NUL-terminated.
    unsafe { MoveFileExA(from.as_ptr(), to.as_ptr(), MOVEFILE_REPLACE_EXISTING) != 0 }
}

#[cfg(unix)]
fn replace_file(temporary: &[u8], path: &[u8]) -> bool {
    let (from, to) = (c_string(temporary), c_string(path));
    // SAFETY: both are NUL-terminated.
    unsafe { libc::rename(from.as_ptr().cast(), to.as_ptr().cast()) == 0 }
}

/// Writes the file through "<file>.new", which then replaces it.
fn write_file(path: &[u8], data: &[u8]) -> bool {
    let mut temporary = path.to_vec();
    temporary.extend_from_slice(b".new");
    let c_temporary = c_string(&temporary);
    // SAFETY: both arguments are NUL-terminated.
    let file = unsafe { libc::fopen(c_temporary.as_ptr().cast(), c"wb".as_ptr()) };
    if file.is_null() {
        return false;
    }
    // SAFETY: `data` is readable for its length and `file` is open until the fclose.
    let written = unsafe { libc::fwrite(data.as_ptr().cast(), 1, data.len(), file) } == data.len();
    let flushed = unsafe { libc::fflush(file) } == 0;
    let closed = unsafe { libc::fclose(file) } == 0;
    if written && flushed && closed && replace_file(&temporary, path) {
        return true;
    }
    // SAFETY: NUL-terminated.
    unsafe { libc::remove(c_temporary.as_ptr().cast()) };
    false
}

fn getenv(name: &CStr) -> Option<Vec<u8>> {
    // SAFETY: a NUL-terminated name; the value is copied before anything can change it.
    let text = unsafe { libc::getenv(name.as_ptr()) };
    if text.is_null() {
        return None;
    }
    // SAFETY: a C string from getenv.
    let value = unsafe { CStr::from_ptr(text) }.to_bytes();
    (!value.is_empty()).then(|| value.to_vec())
}

/// The data root's config.yaml, as a native path.
fn primary_path() -> Vec<u8> {
    const SIZE: usize = 1024;
    let (mut root, mut native) = ([0u8; SIZE], [0u8; SIZE]);
    // SAFETY: both buffers hold SIZE bytes.
    let got = unsafe {
        crate::path::PlatPathGetDataRoot(root.as_mut_ptr().cast(), SIZE as u32) != 0
            && crate::path::PlatPathToNative(root.as_ptr().cast(), native.as_mut_ptr().cast(), SIZE as u32) != 0
    };
    if !got {
        return b"config.yaml".to_vec();
    }
    let length = native.iter().position(|&c| c == 0).unwrap_or(SIZE);
    let mut path = native[..length].to_vec();
    if !path.is_empty() && path.last() != Some(&b'/') && path.last() != Some(&b'\\') {
        path.push(SEPARATOR);
    }
    path.extend_from_slice(b"config.yaml");
    path
}

/// The user's configuration directory (native path, no trailing separator; None when unknown).
fn user_directory() -> Option<Vec<u8>> {
    #[cfg(windows)]
    {
        let mut path = getenv(c"APPDATA")?;
        path.extend_from_slice(b"\\Empire Earth AoC");
        Some(path)
    }
    #[cfg(target_os = "macos")]
    {
        let mut path = getenv(c"HOME")?;
        path.extend_from_slice(b"/Library/Application Support/Empire Earth AoC");
        Some(path)
    }
    #[cfg(all(unix, not(target_os = "macos")))]
    {
        if let Some(config) = getenv(c"XDG_CONFIG_HOME").filter(|c| c.first() == Some(&b'/')) {
            let mut path = config;
            path.extend_from_slice(b"/empire-earth-aoc");
            return Some(path);
        }
        let mut path = getenv(c"HOME")?;
        path.extend_from_slice(b"/.config/empire-earth-aoc");
        Some(path)
    }
}

#[cfg(windows)]
fn create_directory(path: &[u8]) -> bool {
    extern "C" {
        fn _errno() -> *mut c_int;
    }
    let c_path = c_string(path);
    // SAFETY: NUL-terminated; `_errno` returns this thread's errno.
    unsafe { libc::mkdir(c_path.as_ptr().cast()) == 0 || *_errno() == libc::EEXIST }
}

#[cfg(unix)]
fn create_directory(path: &[u8]) -> bool {
    let c_path = c_string(path);
    // SAFETY: NUL-terminated.
    unsafe {
        libc::mkdir(c_path.as_ptr().cast(), 0o700) == 0
            || std::io::Error::last_os_error().raw_os_error() == Some(libc::EEXIST)
    }
}

/// Every directory of `path`, the drive of a Windows path left out.
fn make_directories(path: &[u8]) -> bool {
    for i in 1..=path.len() {
        if i < path.len() && path[i] != b'/' && path[i] != b'\\' {
            continue;
        }
        let prefix = &path[..i];
        if prefix.len() == 2 && prefix[1] == b':' {
            continue; // a Windows drive
        }
        if !create_directory(prefix) {
            return false;
        }
    }
    true
}

fn choose_path() -> Vec<u8> {
    if let Some(path) = getenv(c"EE_CONFIG") {
        return path;
    }
    let primary = primary_path();
    if file_exists(&primary) {
        return primary;
    }
    if let Some(mut user) = user_directory() {
        user.push(SEPARATOR);
        user.extend_from_slice(b"config.yaml");
        if file_exists(&user) {
            return user;
        }
    }
    primary
}

// ---- the store

/// The configuration in memory and the file behind it.
#[derive(Debug)]
pub(crate) struct Store {
    root: Node,
    path: Vec<u8>,
}

impl Store {
    pub(crate) fn load(path: Vec<u8>) -> Self {
        let root = read_file(&path).map_or_else(Node::root, |data| parse(&data));
        Self { root, path }
    }

    /// Rewrites the file; when its directory cannot be written (and EE_CONFIG does not name it), moves to the user's
    /// configuration directory.
    fn save(&mut self) -> bool {
        let data = render(&self.root);
        if write_file(&self.path, &data) {
            return true;
        }
        let Some(mut user) = user_directory() else {
            return false;
        };
        if getenv(c"EE_CONFIG").is_some() || !make_directories(&user) {
            return false;
        }
        user.push(SEPARATOR);
        user.extend_from_slice(b"config.yaml");
        if user == self.path || !write_file(&user, &data) {
            return false;
        }
        self.path = user;
        true
    }

    fn get(&mut self, block: &[u8], key: &[u8]) -> Option<Vec<u8>> {
        self.root.find_value(block, key).map(|node| node.text.clone())
    }

    fn set(&mut self, block: &[u8], key: &[u8], text: Vec<u8>, plain: bool) -> bool {
        if !valid_key(key) {
            return false;
        }
        let Some(parent) = self.root.find_block(block, true) else {
            return false;
        };
        if let Some(node) = parent.find_child(key).map(|index| &parent.children[index]) {
            if !node.block && node.text == text && node.plain == plain {
                return true; // unchanged: no rewrite
            }
        }
        parent.child_or_new(key).set_value(text, plain);
        self.save()
    }

    /// Removes a value, or with no key the whole block.
    fn delete(&mut self, block: &[u8], key: Option<&[u8]>) -> bool {
        let Some(key) = key else {
            if block.is_empty() {
                return false;
            }
            let (parent_path, name) = match block.iter().rposition(|&c| c == b'.') {
                Some(dot) => (&block[..dot], &block[dot + 1..]),
                None => (&block[..0], block),
            };
            let Some(parent) = self.root.find_block(parent_path, false) else {
                return false;
            };
            let Some(index) = parent.find_child(name).filter(|&i| parent.children[i].block) else {
                return false;
            };
            parent.children.remove(index);
            return self.save();
        };
        let Some(parent) = self.root.find_block(block, false) else {
            return false;
        };
        let Some(index) = parent.find_child(key).filter(|&i| !parent.children[i].block) else {
            return false;
        };
        parent.children.remove(index);
        self.save()
    }
}

// ---- the C interface: one store, loaded on first use

static STORE: Mutex<Option<Store>> = Mutex::new(None);

fn with_store<T>(work: impl FnOnce(&mut Store) -> T) -> T {
    let mut guard = STORE.lock().unwrap_or_else(PoisonError::into_inner);
    let store = guard.get_or_insert_with(|| Store::load(choose_path()));
    work(store)
}

/// Copies a value into the caller's buffer: 0 when it does not fit, 1 otherwise.
unsafe fn copy_value(value: &[u8], out: *mut c_char, size: u32) -> c_int {
    if value.len() + 1 > size as usize {
        return 0;
    }
    std::ptr::copy_nonoverlapping(value.as_ptr(), out.cast::<u8>(), value.len());
    *out.add(value.len()) = 0;
    1
}

/// A value as the game reads it: the text up to a NUL inside it, the way a C string stops.
fn as_c_text(value: &[u8]) -> &[u8] {
    value.split(|&c| c == 0).next().unwrap_or(&[])
}

#[no_mangle]
pub unsafe extern "C" fn PlatConfigGetString(
    block: *const c_char,
    key: *const c_char,
    out: *mut c_char,
    size: u32,
) -> c_int {
    if key.is_null() || out.is_null() || size == 0 {
        return 0;
    }
    match with_store(|store| store.get(bytes(block), bytes(key))) {
        Some(value) => copy_value(&value, out, size),
        None => 0,
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatConfigGetInt(block: *const c_char, key: *const c_char, out: *mut i32) -> c_int {
    if out.is_null() || key.is_null() {
        return 0;
    }
    let Some(value) = with_store(|store| store.get(bytes(block), bytes(key))) else {
        return 0;
    };
    // the C++ read it through a 64-byte buffer: a longer value is no integer
    if value.len() + 1 > 64 {
        return 0;
    }
    match parse_int(as_c_text(&value)) {
        Some(number) => {
            *out = number;
            1
        }
        None => 0,
    }
}

#[no_mangle]
pub unsafe extern "C" fn PlatConfigSetString(block: *const c_char, key: *const c_char, value: *const c_char) -> c_int {
    with_store(|store| store.set(bytes(block), bytes(key), bytes(value).to_vec(), false)) as c_int
}

#[no_mangle]
pub unsafe extern "C" fn PlatConfigSetInt(block: *const c_char, key: *const c_char, value: i32) -> c_int {
    with_store(|store| store.set(bytes(block), bytes(key), value.to_string().into_bytes(), true)) as c_int
}

#[no_mangle]
pub unsafe extern "C" fn PlatConfigSetBool(block: *const c_char, key: *const c_char, value: c_int) -> c_int {
    let text = if value != 0 { &b"true"[..] } else { &b"false"[..] };
    with_store(|store| store.set(bytes(block), bytes(key), text.to_vec(), true)) as c_int
}

#[no_mangle]
pub unsafe extern "C" fn PlatConfigDelete(block: *const c_char, key: *const c_char) -> c_int {
    let key = (!key.is_null()).then(|| bytes(key));
    with_store(|store| store.delete(bytes(block), key)) as c_int
}

#[no_mangle]
pub unsafe extern "C" fn PlatConfigGetPath(out: *mut c_char, size: u32) -> u32 {
    with_store(|store| {
        if out.is_null() || store.path.len() + 1 > size as usize {
            return 0;
        }
        std::ptr::copy_nonoverlapping(store.path.as_ptr(), out.cast::<u8>(), store.path.len());
        *out.add(store.path.len()) = 0;
        store.path.len() as u32
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{test_dir, test_globals, test_set_env};

    fn parsed(text: &[u8]) -> Node {
        parse(text)
    }

    fn value(root: &mut Node, block: &[u8], key: &[u8]) -> Option<Vec<u8>> {
        root.find_value(block, key).map(|n| n.text.clone())
    }

    /// The hand-written file of tests/platform/plat_test.cpp (TestConfig), which the C++ read this way.
    const HAND_WRITTEN: &[u8] = b"\xef\xbb\xbf# settings\r\n---\r\nhand:   # a block\r\n    a: 1  # one\r\n\r\n\
          \x20\x20\x20\x20b: 'it''s # here'\r\n    inner:\r\n        c: \"x\\ty\\x41\"\r\n    d: yes\r\n\
          \x20\x20\x20\x20url: http://x#y\r\n    e: ~\r\nother: plain text: no\r\n";

    #[test]
    fn parses_comments_quotes_crlf_and_other_indents() {
        let mut root = parsed(HAND_WRITTEN);
        assert_eq!(value(&mut root, b"hand", b"a").unwrap(), b"1");
        assert_eq!(value(&mut root, b"hand", b"b").unwrap(), b"it's # here");
        assert_eq!(value(&mut root, b"hand.inner", b"c").unwrap(), b"x\tyA");
        assert_eq!(value(&mut root, b"hand", b"d").unwrap(), b"yes");
        assert_eq!(value(&mut root, b"hand", b"url").unwrap(), b"http://x#y");
        assert_eq!(value(&mut root, b"hand", b"e").unwrap(), b"");
        assert_eq!(value(&mut root, b"", b"other").unwrap(), b"plain text: no");
    }

    #[test]
    fn keys_and_blocks_ignore_ascii_case_and_keep_their_first_spelling() {
        let mut root = parsed(b"Display:\n  Width: 1\ndisplay:\n  HEIGHT: 2\n");
        assert_eq!(value(&mut root, b"DISPLAY", b"width").unwrap(), b"1");
        assert_eq!(value(&mut root, b"display", b"height").unwrap(), b"2");
        assert_eq!(root.children.len(), 1);
        assert_eq!(root.children[0].name, b"Display");
    }

    #[test]
    fn a_value_replaces_a_block_of_the_same_name_and_back() {
        let mut root = parsed(b"a:\n  b: 1\na: 5\n");
        assert_eq!(value(&mut root, b"", b"a").unwrap(), b"5");
        assert!(root.find_block(b"a", false).is_none());
        let mut root = parsed(b"a: 5\na:\n  b: 1\n");
        assert_eq!(value(&mut root, b"a", b"b").unwrap(), b"1");
        assert!(value(&mut root, b"", b"a").is_none()); // a block is no value
    }

    #[test]
    fn malformed_lines_are_skipped() {
        let mut root = parsed(b"bad: \"no close\nno colon here\n\ttabbed: 1\n: empty key\nok: 1\n---\n...\n");
        assert!(value(&mut root, b"", b"bad").is_none());
        assert_eq!(value(&mut root, b"", b"ok").unwrap(), b"1");
        assert_eq!(root.children.len(), 1);
    }

    #[test]
    fn escapes_and_null_words() {
        assert_eq!(parse_value(b" \"a\\n\\r\\t\\0\\\\\\\"\\/\\x41\\xZZ\\q\" ").unwrap(), b"a\n\r\t\0\\\"/AxZZq");
        assert_eq!(parse_value(b"'it''s'  # comment").unwrap(), b"it's");
        assert!(parse_value(b"\"x\" tail").is_none());
        assert!(parse_value(b"\"x\\").is_none());
        assert_eq!(parse_value(b"~").unwrap(), b"");
        assert_eq!(parse_value(b"Null").unwrap(), b"");
        assert_eq!(parse_value(b"value # comment").unwrap(), b"value");
        assert_eq!(parse_value(b"a\t#b").unwrap(), b"a");
        assert_eq!(parse_value(b"a#b").unwrap(), b"a#b");
        assert_eq!(parse_value(b"").unwrap(), b"");
        // golden from the C++: the trailing "\x" with only one digit left is an x and the digit
        assert_eq!(parse_value(b"\"\\x4\"").unwrap(), b"x4");
    }

    #[test]
    fn quoting_matches_the_writer() {
        for plain in [&b"Unknown"[..], b"C:\\Games", b"a b", b"x:y", b"tab\xe9"] {
            assert!(is_plain(plain), "{plain:?}");
            assert_eq!(quote(plain), plain);
        }
        for quoted in [
            &b""[..],
            b" lead",
            b"trail ",
            b"-dash",
            b"7up",
            b"+1",
            b".5",
            b"true",
            b"NO",
            b"~",
            b"a: b",
            b"a #b",
            b"end:",
            b"ctl\x01",
            b"\x7f",
        ] {
            assert!(!is_plain(quoted), "{quoted:?}");
        }
        assert_eq!(quote(b"C:\\G \"x\""), b"C:\\G \"x\"");
        assert_eq!(quote(b" a\t"), b"\" a\\t\"");
        assert_eq!(quote(b"\x01\x7f\n"), b"\"\\x01\\x7f\\n\"");
        assert_eq!(quote(b"\"q\\ "), b"\"\\\"q\\\\ \"");
    }

    #[test]
    fn strtoll_matches_the_c_runtime() {
        // the C++ used strtoll; check our reading against the one the C runtime has
        for text in [
            &b"0"[..],
            b"12",
            b"-7",
            b"+5",
            b"010",
            b"0x1F",
            b"0X1f",
            b"0x",
            b"0xZ",
            b"08",
            b"09",
            b"-0x10",
            b" 42",
            b"\t\n 42",
            b"",
            b"-",
            b"+",
            b"- 5",
            b"x",
            b"12x",
            b"0x1g",
            b"4294967295",
            b"4294967296",
            b"-2147483648",
            b"-2147483649",
            b"9223372036854775807",
            b"9223372036854775808",
            b"-9223372036854775808",
            b"-9223372036854775809",
            b"99999999999999999999999",
            b"0777",
            b"0x7fffffffffffffff",
            b"0xffffffffffffffff",
        ] {
            let c_text = terminated(text);
            let mut end: *mut c_char = std::ptr::null_mut();
            // SAFETY: a NUL-terminated string; `end` receives a pointer into it.
            let expected = unsafe { libc::strtoll(c_text.as_ptr().cast(), &mut end, 0) };
            let used = (end as usize) - (c_text.as_ptr() as usize);
            // what PlatConfigGetInt keeps: a whole-string conversion and its value (the C runtimes differ on "0x":
            // glibc stops after the 0, the UCRT reports no conversion; neither is a whole-string conversion)
            let whole_c = (used > 0 && used == text.len()).then_some(expected);
            let ours = strtoll_base0(text).filter(|&(_, bytes, _)| bytes == text.len()).map(|(value, _, _)| value);
            assert_eq!(ours, whole_c, "{text:?}");
        }
    }

    #[test]
    fn integers_read_as_the_game_reads_them() {
        assert_eq!(parse_int(b"1024"), Some(1024));
        assert_eq!(parse_int(b"-7"), Some(-7));
        assert_eq!(parse_int(b"0x10"), Some(16));
        assert_eq!(parse_int(b"010"), Some(8));
        assert_eq!(parse_int(b"true"), Some(1));
        assert_eq!(parse_int(b"NO"), Some(0));
        assert_eq!(parse_int(b"on"), Some(1));
        assert_eq!(parse_int(b"0xffffffff"), Some(-1)); // the bit pattern
        assert_eq!(parse_int(b"4294967295"), Some(-1));
        assert_eq!(parse_int(b"4294967296"), None);
        assert_eq!(parse_int(b"-2147483648"), Some(i32::MIN));
        assert_eq!(parse_int(b"-2147483649"), None);
        assert_eq!(parse_int(b"12 monkeys"), None);
        assert_eq!(parse_int(b""), None);
        assert_eq!(parse_int(b"99999999999999999999"), None);
    }

    #[test]
    fn valid_keys() {
        assert!(valid_key(b"width"));
        assert!(valid_key(b"a:b"));
        assert!(!valid_key(b""));
        assert!(!valid_key(b"a.b"));
        assert!(!valid_key(b" a"));
        assert!(!valid_key(b"a "));
        assert!(!valid_key(b"a\tb"));
        assert!(!valid_key(b"a: b"));
        assert!(!valid_key(b"a:"));
    }

    fn store_in(dir: &std::path::Path) -> Store {
        Store::load(dir.join("config.yaml").to_str().unwrap().as_bytes().to_vec())
    }

    /// The sequence and result of tests/platform/plat_test.cpp TestConfig, which the C++ passed byte for byte.
    #[test]
    fn writes_nested_blocks_in_first_written_order() {
        let dir = test_dir("config_write");
        let mut store = store_in(&dir);
        assert!(store.set(b"display.resolution", b"width", b"1024".to_vec(), true));
        assert!(store.set(b"display", b"negative", b"-7".to_vec(), true));
        assert!(store.set(b"player", b"path", b"C:\\Games\\Empire Earth \"AoC\"".to_vec(), false));
        assert!(store.set(b"player", b"blank", b" padded\t".to_vec(), false));
        assert!(store.set(b"player", b"empty", Vec::new(), false));
        assert!(store.set(b"player", b"number", b"12".to_vec(), false));
        assert!(store.set(b"player", b"name", b"Unknown".to_vec(), false));
        assert!(store.set(b"audio", b"music", b"true".to_vec(), true));
        assert!(store.set(b"", b"top", b"3".to_vec(), true));
        let written = std::fs::read(dir.join("config.yaml")).unwrap();
        let header = "# Empire Earth AoC settings. The game rewrites this file whenever a setting changes (comments are not kept).\n";
        let expected = format!(
            "{header}display:\n  resolution:\n    width: 1024\n  negative: -7\n\nplayer:\n  path: C:\\Games\\Empire Earth \"AoC\"\n  blank: \" padded\\t\"\n  empty: \"\"\n  number: \"12\"\n  name: Unknown\n\naudio:\n  music: true\ntop: 3\n"
        );
        assert_eq!(String::from_utf8(written).unwrap(), expected);

        assert!(store.delete(b"display", Some(b"negative")));
        assert!(!store.delete(b"display", Some(b"negative")));
        assert!(store.delete(b"player", None));
        assert!(store.delete(b"audio", Some(b"music"))); // an empty block is not written
        assert!(store.get(b"player", b"blank").is_none());
        let written = std::fs::read(dir.join("config.yaml")).unwrap();
        assert_eq!(
            String::from_utf8(written).unwrap(),
            format!("{header}display:\n  resolution:\n    width: 1024\ntop: 3\n")
        );
        std::fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn an_unchanged_value_does_not_rewrite() {
        let dir = test_dir("config_unchanged");
        let mut store = store_in(&dir);
        assert!(store.set(b"a", b"k", b"v".to_vec(), false));
        let path = dir.join("config.yaml");
        std::fs::remove_file(&path).unwrap();
        assert!(store.set(b"a", b"k", b"v".to_vec(), false)); // no rewrite: the file stays gone
        assert!(!path.exists());
        assert!(store.set(b"a", b"k", b"w".to_vec(), false));
        assert!(path.exists());
        std::fs::remove_dir_all(dir).unwrap();
    }

    /// A new key whose value happens to equal a fresh node's (empty, not plain) must still be written.
    #[test]
    fn a_new_empty_value_is_written() {
        let dir = test_dir("config_new_empty");
        let mut store = store_in(&dir);
        assert!(store.set(b"a", b"k", Vec::new(), false));
        let written = std::fs::read(dir.join("config.yaml")).unwrap();
        assert!(written.ends_with(b"a:\n  k: \"\"\n"), "{}", String::from_utf8_lossy(&written));
        std::fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn set_rejects_bad_keys_and_bad_block_paths() {
        let dir = test_dir("config_bad");
        let mut store = store_in(&dir);
        assert!(!store.set(b"a..b", b"k", b"v".to_vec(), false));
        assert!(!store.set(b".a", b"k", b"v".to_vec(), false));
        assert!(!store.set(b"a", b"bad.key", b"v".to_vec(), false));
        assert!(!dir.join("config.yaml").exists());
        std::fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn set_replaces_a_block_with_a_value_and_paths_through_values() {
        let dir = test_dir("config_replace");
        let mut store = store_in(&dir);
        assert!(store.set(b"a", b"b", b"1".to_vec(), true));
        assert!(store.set(b"", b"a", b"5".to_vec(), true)); // the block "a" becomes a value
        assert_eq!(store.get(b"", b"a").unwrap(), b"5");
        assert!(store.get(b"a", b"b").is_none());
        assert!(store.set(b"a.c", b"d", b"7".to_vec(), true)); // the value "a" becomes a block again
        assert_eq!(store.get(b"a.c", b"d").unwrap(), b"7");
        std::fs::remove_dir_all(dir).unwrap();
    }

    #[test]
    fn a_file_round_trips_through_load_and_save() {
        let dir = test_dir("config_roundtrip");
        let path = dir.join("config.yaml");
        std::fs::write(&path, HAND_WRITTEN).unwrap();
        let mut store = store_in(&dir);
        assert!(store.set(b"", b"new", b"1".to_vec(), true));
        let again = store_in(&dir);
        assert_eq!(again.root, {
            let mut root = parse(HAND_WRITTEN);
            root.child_or_new(b"new").set_value(b"1".to_vec(), true);
            // a "~" value reads back as a block: "e: " is written with nothing after it (a C++ behaviour kept)
            let e = root.find_block(b"hand", false).unwrap().child_or_new(b"e");
            e.block = true;
            e.plain = false;
            root
        });
        std::fs::remove_dir_all(dir).unwrap();
    }

    /// Golden values captured by running the deleted C++ (src/Platform/Common/PlatConfig.cpp) over the same inputs.
    #[test]
    fn golden_documents() {
        let golden = include_str!("golden_config.txt");
        let mut cases = 0;
        for case in golden.split("\n@@\n").filter(|c| !c.trim().is_empty()) {
            let (input, output) = case.split_once("\n=>\n").expect("a golden case");
            let root = parse(&decode(input));
            assert_eq!(hex(&render(&root)), output.trim(), "case: {input}");
            cases += 1;
        }
        assert!(cases >= 20, "{cases} golden cases");
    }

    /// The golden file's input: `\xHH` escapes for any byte, everything else itself.
    fn decode(text: &str) -> Vec<u8> {
        let bytes = text.trim_end_matches('\n').as_bytes();
        let mut out = Vec::new();
        let mut i = 0;
        while i < bytes.len() {
            if bytes[i] == b'\\' && bytes.get(i + 1) == Some(&b'x') {
                let digit = |c: u8| (c as char).to_digit(16).unwrap() as u8;
                out.push(digit(bytes[i + 2]) * 16 + digit(bytes[i + 3]));
                i += 4;
            } else {
                out.push(bytes[i]);
                i += 1;
            }
        }
        out
    }

    fn hex(data: &[u8]) -> String {
        data.iter().map(|b| format!("{b:02x}")).collect()
    }

    /// The exported functions' whole flow, through EE_CONFIG (tests/platform runs the same against the C API).
    #[test]
    fn c_api_uses_ee_config_and_get_path() {
        let _guard = test_globals();
        let dir = test_dir("config_api");
        let path = dir.join("api.yaml");
        std::fs::write(&path, b"a:\n  b: 7\n").unwrap();
        test_set_env("EE_CONFIG", path.to_str().unwrap());
        // SAFETY: valid NUL-terminated strings and buffers below.
        unsafe {
            *STORE.lock().unwrap() = None;
            let mut out = [0 as c_char; 260];
            let n = PlatConfigGetPath(out.as_mut_ptr(), 260) as usize;
            let got: Vec<u8> = out[..n].iter().map(|&c| c as u8).collect();
            assert_eq!(got, path.to_str().unwrap().as_bytes());
            assert_eq!(PlatConfigGetPath(out.as_mut_ptr(), 1), 0);

            let mut number = 0;
            assert_eq!(PlatConfigGetInt(c"A".as_ptr(), c"B".as_ptr(), &mut number), 1);
            assert_eq!(number, 7);
            assert_eq!(PlatConfigGetString(c"a".as_ptr(), c"b".as_ptr(), out.as_mut_ptr(), 2), 1);
            assert_eq!(PlatConfigGetString(c"a".as_ptr(), c"b".as_ptr(), out.as_mut_ptr(), 1), 0); // too small
            assert_eq!(PlatConfigGetString(c"a".as_ptr(), std::ptr::null(), out.as_mut_ptr(), 10), 0);
            assert_eq!(PlatConfigGetString(std::ptr::null(), c"a".as_ptr(), out.as_mut_ptr(), 10), 0); // a block

            assert_eq!(PlatConfigSetBool(c"x".as_ptr(), c"on".as_ptr(), 1), 1);
            assert_eq!(PlatConfigGetInt(c"x".as_ptr(), c"on".as_ptr(), &mut number), 1);
            assert_eq!(number, 1);
            assert_eq!(PlatConfigSetInt(std::ptr::null(), c"n".as_ptr(), -3), 1);
            assert_eq!(PlatConfigGetInt(c"".as_ptr(), c"N".as_ptr(), &mut number), 1);
            assert_eq!(number, -3);
            assert_eq!(PlatConfigSetString(c"x".as_ptr(), c"s".as_ptr(), c"".as_ptr()), 1);
            assert_eq!(PlatConfigDelete(c"x".as_ptr(), std::ptr::null()), 1);
            assert_eq!(PlatConfigDelete(c"x".as_ptr(), std::ptr::null()), 0);
            assert_eq!(PlatConfigDelete(std::ptr::null(), std::ptr::null()), 0);
            *STORE.lock().unwrap() = None;
        }
        test_set_env("EE_CONFIG", "");
        std::fs::remove_dir_all(dir).unwrap();
    }
}
