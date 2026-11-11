//! WON's `ConfigParser` (WONConfig/ConfigParser.cpp, ConfigToken.cpp): the reader of the lobby's .cfg files. Its
//! characters (pushback first, 16-bit units in a UTF-16 file, CR LF and a lone CR read as LF, each newline counted
//! in the parser's line and the process-wide line count), comments (`#`, `//`, `/* */`), tokens (narrow and wide,
//! a key's value to the end of its line, skipped), `$name$` expansion, the define scopes and the group defines with
//! their saved states, and the open's UTF-16 sniff are here. The C++ `ConfigParser` keeps the file reader (whose
//! core is `file.rs`), the line number and the Unicode flag (handed in as an [`EeWonConfigIo`]), the parent link,
//! the file names, the error and warning texts and their formatting.
//!
//! - The pushback is 16-bit units, as `std::wstring` on Windows: a pushed-back character is truncated to 16 bits
//!   and read back zero-extended (on POSIX the original kept a 32-bit `wchar_t`; a sign-extended byte such as
//!   0xe9 now reads back as 0xffe9 there too).
//! - The character classes are the C runtime's (`isspace` of any value below 256, the define names' `toupper` of
//!   each signed `char`), so the game's locale keeps its say (`ffi_parse::CrtCtype`).

use std::ffi::{c_char, c_long, c_void};
use std::sync::atomic::{AtomicI32, Ordering};

use crate::ffi::EeWonByteSink;
use crate::ffi_parse::{CrtCtype, EeWonWideUnitSink};
use crate::file::{EeWonFileReader, WON_FILE_OK};
use crate::parser::Ctype;
use crate::text;

/// `ConfigParser::mLineCount`: every newline read by any parser, uncounted when pushed back.
static LINE_COUNT: AtomicI32 = AtomicI32::new(0);

/// Where a parser reads from: its file reader's bytes.
pub trait Source {
    fn try_read_byte(&mut self) -> i32;
    fn end_of_file(&self) -> bool;
    fn is_open(&self) -> bool;
}

impl Source for EeWonFileReader {
    fn try_read_byte(&mut self) -> i32 {
        EeWonFileReader::try_read_byte(self)
    }
    fn end_of_file(&self) -> bool {
        EeWonFileReader::end_of_file(self)
    }
    fn is_open(&self) -> bool {
        EeWonFileReader::is_open(self)
    }
}

/// A parser's members the C++ keeps: the reader, the line number and the Unicode flag.
pub struct Io<'a, S: Source> {
    pub source: &'a mut S,
    pub line: &'a mut i32,
    pub unicode: bool,
}

/// A saved parser position (`ConfigParserState`): the file position, the file, the line and the pushback.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SavedState {
    pub pos: c_long,
    pub path: Vec<u8>,
    pub line: i32,
    pub back: Vec<u16>,
}

#[derive(Default)]
struct Scope {
    defines: Vec<(Vec<u8>, Vec<u8>)>,
    groups: Vec<(Vec<u8>, SavedState)>,
}

/// What a define or group define insertion warns about (the root parser's `AddWarning`, except a group define's
/// spaces, which the parser that was asked warns about).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(i32)]
pub enum DefineResult {
    Ok = 0,
    /// "Not allowed to have spaces in define names."
    Spaces = 1,
    /// "Redefintion of " + the name (the original's spelling)
    Redefinition = 2,
}

/// A token read's end: no token, a token, or one of the three errors `SetError` reports.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum TokenResult {
    False,
    True,
    /// "Expecting end quote"
    EndQuote,
    /// "Expecting closing '$'"
    ClosingDollar,
    /// "Define not found: " + the name
    DefineNotFound(Vec<u8>),
}

/// `StringCompareNoCase`: each signed `char` through `toupper`, then the lengths.
fn compare_no_case(a: &[u8], b: &[u8], ct: &impl Ctype) -> std::cmp::Ordering {
    for (x, y) in a.iter().zip(b) {
        let (x, y) = (ct.to_upper(*x as i8 as i32), ct.to_upper(*y as i8 as i32));
        if x != y {
            return x.cmp(&y);
        }
    }
    a.len().cmp(&b.len())
}

/// The parser's own state: the pushback and the define stack (an empty scope is `None`, as the original's NULL).
#[derive(Default)]
pub struct Lexer {
    back: Vec<u16>,
    stack: Vec<Option<Scope>>,
}

impl Lexer {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn back(&self) -> &[u16] {
        &self.back
    }

    pub fn set_back(&mut self, back: &[u16]) {
        self.back = back.to_vec();
    }

    /// `Reset`'s part: the define stack and the pushback emptied.
    pub fn reset(&mut self) {
        self.stack.clear();
        self.back.clear();
    }

    pub fn end_of_file<S: Source>(&self, io: &Io<S>) -> bool {
        io.source.end_of_file() && self.back.is_empty()
    }

    pub fn get_char<S: Source>(&mut self, io: &mut Io<S>) -> i32 {
        let ch = match self.back.pop() {
            Some(u) => u as i32,
            None if io.unicode => {
                let low = io.source.try_read_byte();
                low | (io.source.try_read_byte() << 8)
            }
            None => io.source.try_read_byte(),
        };
        if ch == 0x0d {
            let next = self.get_char(io);
            if next != 0x0a {
                self.unget_char(io, next);
                LINE_COUNT.fetch_add(1, Ordering::Relaxed);
                *io.line += 1;
            }
            return 0x0a;
        }
        if ch == 0x0a {
            LINE_COUNT.fetch_add(1, Ordering::Relaxed);
            *io.line += 1;
        }
        ch
    }

    /// The next character without consuming it; -1 at the end or with no file open (even with characters pushed
    /// back).
    pub fn peek_char<S: Source>(&mut self, io: &mut Io<S>) -> i32 {
        if io.source.is_open() && !self.end_of_file(io) {
            let ch = self.get_char(io);
            self.unget_char(io, ch);
            return ch;
        }
        -1
    }

    pub fn unget_char<S: Source>(&mut self, io: &mut Io<S>, ch: i32) {
        if ch == -1 {
            return;
        }
        if ch == 0x0a {
            *io.line -= 1;
            LINE_COUNT.fetch_sub(1, Ordering::Relaxed);
        }
        self.back.push(ch as u16);
    }

    /// Pushes a narrow string back (each `char` sign-extended) so that it reads again in order.
    pub fn unget_narrow<S: Source>(&mut self, io: &mut Io<S>, s: &[u8]) {
        for b in s.iter().rev() {
            self.unget_char(io, *b as i8 as i32);
        }
    }

    /// Pushes `wchar_t` values back (as the C++ holds them: a 16-bit unit, or a 32-bit value on POSIX).
    pub fn unget_wide<S: Source>(&mut self, io: &mut Io<S>, s: &[u32]) {
        for u in s.iter().rev() {
            self.unget_char(io, *u as i32);
        }
    }

    pub fn skip_whitespace<S: Source>(&mut self, io: &mut Io<S>, ct: &impl Ctype) {
        while !self.end_of_file(io) {
            let ch = self.get_char(io);
            if (ch as u32) < 256 && ct.is_space(ch) {
                continue;
            }
            if ch == b'#' as i32 {
                self.skip_line(io);
                continue;
            }
            if ch == b'/' as i32 {
                let next = self.get_char(io);
                if next == b'/' as i32 {
                    self.skip_line(io);
                    continue;
                }
                if next == b'*' as i32 {
                    self.skip_block_comment(io);
                    continue;
                }
                self.unget_char(io, next);
            }
            self.unget_char(io, ch);
            return;
        }
    }

    pub fn skip_line<S: Source>(&mut self, io: &mut Io<S>) {
        while !self.end_of_file(io) {
            if self.get_char(io) == 0x0a {
                return;
            }
        }
    }

    pub fn skip_block_comment<S: Source>(&mut self, io: &mut Io<S>) {
        let mut star = false;
        while !self.end_of_file(io) {
            let ch = self.get_char(io);
            if star && ch == b'/' as i32 {
                return;
            }
            star = ch == b'*' as i32;
        }
    }

    // ---- defines (on the root parser: the C++ walks the parents)

    pub fn push_scope(&mut self) {
        self.stack.push(None);
    }

    pub fn pop_scope(&mut self) {
        self.stack.pop();
    }

    fn top_scope(&mut self) -> &mut Scope {
        if self.stack.is_empty() {
            self.stack.push(Some(Scope::default()));
        }
        self.stack.last_mut().unwrap().get_or_insert_with(Scope::default)
    }

    /// `AddDefine`: into the innermost scope; `redefine` replaces the value (the first spelling of the name stays).
    pub fn add_define(&mut self, name: &[u8], value: &[u8], redefine: bool, ct: &impl Ctype) -> DefineResult {
        if name.contains(&b' ') {
            return DefineResult::Spaces;
        }
        let scope = self.top_scope();
        match scope.defines.iter_mut().find(|(n, _)| compare_no_case(n, name, ct).is_eq()) {
            Some((_, v)) if redefine => {
                *v = value.to_vec();
                DefineResult::Ok
            }
            Some(_) => DefineResult::Redefinition,
            None => {
                scope.defines.push((name.to_vec(), value.to_vec()));
                DefineResult::Ok
            }
        }
    }

    /// `GetDefine`: from the innermost scope outwards.
    pub fn get_define(&self, name: &[u8], ct: &impl Ctype) -> Option<&[u8]> {
        self.stack.iter().rev().flatten().find_map(|scope| {
            scope.defines.iter().find(|(n, _)| compare_no_case(n, name, ct).is_eq()).map(|(_, v)| v.as_slice())
        })
    }

    /// `AddGroupDefine`'s insertion: the state is the asking parser's (or the caller's).
    pub fn add_group(&mut self, name: &[u8], state: &SavedState, ct: &impl Ctype) -> DefineResult {
        if name.contains(&b' ') {
            return DefineResult::Spaces;
        }
        let scope = self.top_scope();
        if scope.groups.iter().any(|(n, _)| compare_no_case(n, name, ct).is_eq()) {
            return DefineResult::Redefinition;
        }
        scope.groups.push((name.to_vec(), state.clone()));
        DefineResult::Ok
    }

    pub fn get_group(&self, name: &[u8], ct: &impl Ctype) -> Option<&SavedState> {
        self.stack
            .iter()
            .rev()
            .flatten()
            .find_map(|scope| scope.groups.iter().find(|(n, _)| compare_no_case(n, name, ct).is_eq()).map(|(_, s)| s))
    }
}

/// A token being read: narrow `char`s or `wchar_t` values (`wchar_size` 2 or 4 for a define's text).
pub enum Token<'a> {
    Narrow(&'a mut Vec<u8>),
    Wide(&'a mut Vec<u32>, u32),
}

impl Token<'_> {
    fn clear(&mut self) {
        match self {
            Token::Narrow(t) => t.clear(),
            Token::Wide(t, _) => t.clear(),
        }
    }
    fn is_empty(&self) -> bool {
        match self {
            Token::Narrow(t) => t.is_empty(),
            Token::Wide(t, _) => t.is_empty(),
        }
    }
    fn push(&mut self, ch: i32) {
        match self {
            Token::Narrow(t) => t.push(ch as u8),
            Token::Wide(t, _) => t.push(ch as u16 as u32),
        }
    }
    /// A define's value: its bytes, or `StringToWString` of them (up to a NUL).
    fn push_value(&mut self, value: &[u8]) {
        match self {
            Token::Narrow(t) => t.extend_from_slice(value),
            Token::Wide(t, size) => {
                let end = value.iter().position(|&b| b == 0).unwrap_or(value.len());
                if *size == 2 {
                    t.extend(text::utf16_from_game(&value[..end]).into_iter().map(u32::from));
                } else {
                    t.extend(text::utf32_from_game(&value[..end]));
                }
            }
        }
    }
}

/// `ReadToken`: the next token, or with `to_end_line` the rest of the line (whitespace runs as one space, a
/// backslash before the newline joins the next line), up to `;`, `#`, a comment or an unquoted `}` (a lone `}` is
/// a token); quotes stay in the token. `$name$` expands to the define's value (from `root`'s scopes, or this
/// parser's when `root` is `None`), except after an escape inside quotes; `skip` reads without keeping or
/// expanding. True for a token (always, when skipping).
pub fn read_token<S: Source>(
    lexer: &mut Lexer,
    io: &mut Io<S>,
    ct: &impl Ctype,
    root: Option<&Lexer>,
    token: &mut Token,
    to_end_line: bool,
    skip: bool,
) -> TokenResult {
    if lexer.end_of_file(io) {
        return TokenResult::False;
    }
    token.clear();
    let mut line = *io.line;
    lexer.skip_whitespace(io, ct);
    if to_end_line && line != *io.line {
        return TokenResult::True;
    }
    let space = |c: i32| (c as u32) < 256 && ct.is_space(c);
    let mut quoted = false;
    let mut escaped = false;
    let mut ch = 0i32;
    while !lexer.end_of_file(io) {
        escaped = ch == b'\\' as i32 && !escaped;
        ch = lexer.get_char(io);
        if ch == -1 {
            break;
        }
        if space(ch) {
            if !quoted {
                if !to_end_line || (ch == 0x0a && !escaped) {
                    lexer.unget_char(io, ch);
                    break;
                }
                if !skip {
                    token.push(b' ' as i32);
                }
                line = *io.line;
                lexer.skip_whitespace(io, ct);
                if line != *io.line {
                    break;
                }
                continue;
            }
            if ch == 0x0a {
                return TokenResult::EndQuote;
            }
        } else if ch == b'}' as i32 {
            if !quoted {
                if token.is_empty() && !skip {
                    token.push(b'}' as i32);
                } else {
                    lexer.unget_char(io, ch);
                }
                break;
            }
        } else if ch == b';' as i32 {
            if !quoted {
                break;
            }
        } else if ch == b'/' as i32 {
            if !quoted {
                let next = lexer.peek_char(io);
                if next == b'*' as i32 || next == b'/' as i32 {
                    lexer.unget_char(io, ch);
                    break;
                }
            }
        } else if ch == b'#' as i32 {
            if !quoted {
                lexer.unget_char(io, ch);
                break;
            }
        } else if ch == b'"' as i32 {
            if quoted {
                if !escaped {
                    quoted = false;
                }
            } else {
                quoted = true;
            }
        } else if ch == b'\\' as i32 {
            let next = lexer.get_char(io);
            if next == 0x0a {
                continue;
            }
            lexer.unget_char(io, next);
        } else if ch == b'$' as i32 {
            if skip {
                continue;
            }
            if !quoted || !escaped {
                let mut name = Vec::new();
                while !lexer.end_of_file(io) {
                    let next = lexer.get_char(io);
                    if space(next) {
                        return TokenResult::ClosingDollar;
                    }
                    if next == b'$' as i32 {
                        break;
                    }
                    name.push(next as u8);
                }
                let value = match root {
                    Some(r) => r.get_define(&name, ct),
                    None => lexer.get_define(&name, ct),
                };
                match value {
                    Some(v) => {
                        let v = v.to_vec();
                        token.push_value(&v);
                    }
                    None => return TokenResult::DefineNotFound(name),
                }
                continue;
            }
        }
        if !skip {
            token.push(ch);
        }
    }
    if skip || !token.is_empty() {
        TokenResult::True
    } else {
        TokenResult::False
    }
}

/// `OpenFile`'s part after the names: opens the file as text, and when it starts FF FE ?? 00 reopens it binary past
/// the two bytes (a UTF-16 file); then reads it into memory. The status is the reader's (an exception in C++).
///
/// # Safety
/// `path` is a C string.
pub unsafe fn open_file(reader: &mut EeWonFileReader, unicode: &mut bool, path: *const c_char) -> (bool, i32) {
    unsafe { reader.open_text(path) };
    if reader.is_open() {
        let first = reader.try_read_byte();
        let second = reader.try_read_byte();
        reader.try_read_byte();
        let fourth = reader.try_read_byte();
        if first == 0xff && second == 0xfe && fourth == 0 {
            unsafe { reader.open(path) };
            let status = reader.skip(2);
            if status != WON_FILE_OK {
                return (reader.is_open(), status);
            }
            *unicode = true;
        } else {
            reader.rewind();
        }
    }
    let status = reader.read_into_memory();
    (reader.is_open(), status)
}

// ---- the C ABI (WONConfig/ConfigParser.cpp, ConfigToken.cpp)

/// A parser's members the C++ keeps, for one call.
#[repr(C)]
pub struct EeWonConfigIo {
    pub reader: *mut EeWonFileReader,
    pub line: *mut i32,
    pub unicode: bool,
}

/// The parser's Rust state (opaque): the pushback and the define scopes.
pub struct EeWonConfigLexer(Lexer);

unsafe fn io_of<'a>(io: *mut EeWonConfigIo) -> Io<'a, EeWonFileReader> {
    // SAFETY: the C++ parser's members, valid for the call (each function's contract)
    let io = unsafe { &mut *io };
    Io { source: unsafe { &mut *io.reader }, line: unsafe { &mut *io.line }, unicode: io.unicode }
}

unsafe fn lexer<'a>(l: *mut EeWonConfigLexer) -> &'a mut Lexer {
    // SAFETY: a lexer from ee_won_config_new
    unsafe { &mut (*l).0 }
}

unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: `n` bytes (contract)
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

#[no_mangle]
pub extern "C" fn ee_won_config_new() -> *mut EeWonConfigLexer {
    Box::into_raw(Box::new(EeWonConfigLexer(Lexer::new())))
}

/// # Safety
/// `l` is from `ee_won_config_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_free(l: *mut EeWonConfigLexer) {
    if !l.is_null() {
        drop(unsafe { Box::from_raw(l) });
    }
}

#[no_mangle]
pub extern "C" fn ee_won_config_line_count() -> i32 {
    LINE_COUNT.load(Ordering::Relaxed)
}

#[no_mangle]
pub extern "C" fn ee_won_config_reset_line_count() {
    LINE_COUNT.store(0, Ordering::Relaxed);
}

/// # Safety
/// `l` is a lexer.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_reset(l: *mut EeWonConfigLexer) {
    unsafe { lexer(l) }.reset();
}

/// # Safety
/// `l` is a lexer, `io` the parser's members.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_end_of_file(l: *mut EeWonConfigLexer, io: *mut EeWonConfigIo) -> bool {
    let io = unsafe { io_of(io) };
    unsafe { lexer(l) }.end_of_file(&io)
}

/// # Safety
/// As `ee_won_config_end_of_file`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_get_char(l: *mut EeWonConfigLexer, io: *mut EeWonConfigIo) -> i32 {
    let mut io = unsafe { io_of(io) };
    unsafe { lexer(l) }.get_char(&mut io)
}

/// # Safety
/// As `ee_won_config_end_of_file`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_peek_char(l: *mut EeWonConfigLexer, io: *mut EeWonConfigIo) -> i32 {
    let mut io = unsafe { io_of(io) };
    unsafe { lexer(l) }.peek_char(&mut io)
}

/// # Safety
/// As `ee_won_config_end_of_file`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_unget_char(l: *mut EeWonConfigLexer, io: *mut EeWonConfigIo, ch: i32) {
    let mut io = unsafe { io_of(io) };
    unsafe { lexer(l) }.unget_char(&mut io, ch);
}

/// # Safety
/// As `ee_won_config_end_of_file`; `text` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_unget_narrow(
    l: *mut EeWonConfigLexer,
    io: *mut EeWonConfigIo,
    text: *const u8,
    length: usize,
) {
    let mut io = unsafe { io_of(io) };
    unsafe { lexer(l) }.unget_narrow(&mut io, unsafe { bytes(text, length) });
}

/// # Safety
/// As `ee_won_config_end_of_file`; `units` holds `count` `wchar_t` values widened to 32 bits.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_unget_wide(
    l: *mut EeWonConfigLexer,
    io: *mut EeWonConfigIo,
    units: *const u32,
    count: usize,
) {
    let mut io = unsafe { io_of(io) };
    let units =
        if units.is_null() || count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(units, count) } };
    unsafe { lexer(l) }.unget_wide(&mut io, units);
}

/// # Safety
/// As `ee_won_config_end_of_file`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_skip_whitespace(l: *mut EeWonConfigLexer, io: *mut EeWonConfigIo) {
    let mut io = unsafe { io_of(io) };
    unsafe { lexer(l) }.skip_whitespace(&mut io, &CrtCtype);
}

/// # Safety
/// As `ee_won_config_end_of_file`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_skip_line(l: *mut EeWonConfigLexer, io: *mut EeWonConfigIo) {
    let mut io = unsafe { io_of(io) };
    unsafe { lexer(l) }.skip_line(&mut io);
}

/// # Safety
/// As `ee_won_config_end_of_file`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_skip_block_comment(l: *mut EeWonConfigLexer, io: *mut EeWonConfigIo) {
    let mut io = unsafe { io_of(io) };
    unsafe { lexer(l) }.skip_block_comment(&mut io);
}

/// `ReadToken` (narrow when `wchar_size` is 0, else wide with `wchar_t` of that size): 0 no token, 1 a token, 2
/// "Expecting end quote", 3 "Expecting closing '$'", 4 "Define not found: " + the name (given to `name`). The
/// token as read (the whole token, or the part before an error) goes to `narrow` or `wide` unless the data had
/// ended before the read (then the C++ leaves its token alone). `root` is the root parser's lexer, or NULL when this
/// parser is the root.
///
/// # Safety
/// `l` and a non-NULL `root` are distinct lexers, `io` the parser's members; the sinks take the text.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_config_read_token(
    l: *mut EeWonConfigLexer,
    io: *mut EeWonConfigIo,
    root: *const EeWonConfigLexer,
    wchar_size: u32,
    to_end_line: bool,
    skip: bool,
    narrow: EeWonByteSink,
    wide: EeWonWideUnitSink,
    name: EeWonByteSink,
    context: *mut c_void,
) -> i32 {
    let mut io = unsafe { io_of(io) };
    let root = if root.is_null() || std::ptr::eq(root, l) { None } else { Some(unsafe { &(*root).0 }) };
    let lexer = unsafe { lexer(l) };
    if lexer.end_of_file(&io) {
        return 0;
    }
    let mut n = Vec::new();
    let mut w = Vec::new();
    let mut token = if wchar_size == 0 { Token::Narrow(&mut n) } else { Token::Wide(&mut w, wchar_size) };
    let result = read_token(lexer, &mut io, &CrtCtype, root, &mut token, to_end_line, skip);
    if wchar_size == 0 {
        unsafe { narrow(context, n.as_ptr(), n.len()) }
    } else {
        unsafe { wide(context, w.as_ptr(), w.len()) }
    }
    match result {
        TokenResult::False => 0,
        TokenResult::True => 1,
        TokenResult::EndQuote => 2,
        TokenResult::ClosingDollar => 3,
        TokenResult::DefineNotFound(missing) => {
            unsafe { name(context, missing.as_ptr(), missing.len()) };
            4
        }
    }
}

/// # Safety
/// `root` is a lexer.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_push_scope(root: *mut EeWonConfigLexer) {
    unsafe { lexer(root) }.push_scope();
}

/// # Safety
/// `root` is a lexer.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_pop_scope(root: *mut EeWonConfigLexer) {
    unsafe { lexer(root) }.pop_scope();
}

/// `AddDefine` on the root's lexer: 0, or the warning (1 spaces, 2 redefinition).
///
/// # Safety
/// `root` is a lexer; the texts hold their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_add_define(
    root: *mut EeWonConfigLexer,
    name: *const u8,
    name_len: usize,
    value: *const u8,
    value_len: usize,
    redefine: bool,
) -> i32 {
    unsafe { lexer(root) }.add_define(
        unsafe { bytes(name, name_len) },
        unsafe { bytes(value, value_len) },
        redefine,
        &CrtCtype,
    ) as i32
}

/// `GetDefine` on the root's lexer: the value to the sink, or false.
///
/// # Safety
/// `root` is a lexer; `name` holds `name_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_get_define(
    root: *const EeWonConfigLexer,
    name: *const u8,
    name_len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match unsafe { &(*root).0 }.get_define(unsafe { bytes(name, name_len) }, &CrtCtype) {
        Some(v) => {
            unsafe { sink(context, v.as_ptr(), v.len()) };
            true
        }
        None => false,
    }
}

/// A saved state across the C ABI: the file position, name and line, and the pushback's units.
#[repr(C)]
pub struct EeWonConfigState {
    pub pos: c_long,
    pub path: *const u8,
    pub path_len: usize,
    pub line: i32,
    pub back: *const u16,
    pub back_len: usize,
}

/// Receives a saved state; valid only during the call.
pub type EeWonConfigStateSink = unsafe extern "C" fn(context: *mut c_void, state: *const EeWonConfigState);

/// `AddGroupDefine`'s insertion on the root's lexer: 0, or the warning (1 spaces, 2 redefinition).
///
/// # Safety
/// `root` is a lexer; `name` holds `name_len` bytes, `state` is a state.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_add_group(
    root: *mut EeWonConfigLexer,
    name: *const u8,
    name_len: usize,
    state: *const EeWonConfigState,
) -> i32 {
    let s = unsafe { &*state };
    let saved = SavedState {
        pos: s.pos,
        path: unsafe { bytes(s.path, s.path_len) }.to_vec(),
        line: s.line,
        back: if s.back.is_null() || s.back_len == 0 {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(s.back, s.back_len) }.to_vec()
        },
    };
    unsafe { lexer(root) }.add_group(unsafe { bytes(name, name_len) }, &saved, &CrtCtype) as i32
}

/// `GetGroupDefine` on the root's lexer: the state to the sink, or false.
///
/// # Safety
/// `root` is a lexer; `name` holds `name_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_get_group(
    root: *const EeWonConfigLexer,
    name: *const u8,
    name_len: usize,
    sink: EeWonConfigStateSink,
    context: *mut c_void,
) -> bool {
    match unsafe { &(*root).0 }.get_group(unsafe { bytes(name, name_len) }, &CrtCtype) {
        Some(s) => {
            let state = EeWonConfigState {
                pos: s.pos,
                path: s.path.as_ptr(),
                path_len: s.path.len(),
                line: s.line,
                back: s.back.as_ptr(),
                back_len: s.back.len(),
            };
            unsafe { sink(context, &state) };
            true
        }
        None => false,
    }
}

/// The pushback's units, to the sink (zero-extended).
///
/// # Safety
/// `l` is a lexer.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_get_back(
    l: *const EeWonConfigLexer,
    sink: EeWonWideUnitSink,
    context: *mut c_void,
) {
    let units: Vec<u32> = unsafe { &(*l).0 }.back().iter().map(|&u| u as u32).collect();
    unsafe { sink(context, units.as_ptr(), units.len()) };
}

/// Replaces the pushback (each `wchar_t` truncated to 16 bits).
///
/// # Safety
/// `l` is a lexer; `units` holds `count` values.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_set_back(l: *mut EeWonConfigLexer, units: *const u32, count: usize) {
    let units: Vec<u16> = if units.is_null() || count == 0 {
        Vec::new()
    } else {
        unsafe { std::slice::from_raw_parts(units, count) }.iter().map(|&u| u as u16).collect()
    };
    unsafe { lexer(l) }.set_back(&units);
}

/// `OpenFile`'s open and UTF-16 sniff (the C++ has reset the line, the flag and the pushback and split the path):
/// whether the reader is open; `status` is the reader's.
///
/// # Safety
/// `io` is the parser's members; `path` a C string.
#[no_mangle]
pub unsafe extern "C" fn ee_won_config_open(io: *mut EeWonConfigIo, path: *const c_char, status: &mut i32) -> bool {
    let io = unsafe { &mut *io };
    let (open, s) = unsafe { open_file(&mut *io.reader, &mut io.unicode, path) };
    *status = s;
    open
}

#[cfg(test)]
#[path = "config_golden.rs"]
mod config_golden;
