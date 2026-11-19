//! WON's `ConfigParser` whole (WONConfig/ConfigParser.cpp and ConfigToken.cpp, deleted in WON round 21): the
//! parser's every member (the file reader, the lexer's pushback and define scopes, the parent link, the error and
//! warning texts, the flags, the line and the file names) and every method. The C++ class in
//! ee/wonapi/WONLobbyConfig.h is a handle whose methods are inline forwards here; it keeps only the exceptions the
//! original threw (a reader's failed status throws `FileReaderException` in the caller's frame) and the conversion of
//! a `ConfigParserState`.
//!
//! - The error and warning texts are the original's `snprintf`s into 1,024-byte buffers: each piece is cut at 1,023
//!   bytes, and a `%s` argument ends at its first NUL.
//! - Every define and group define lives on the root parser (the topmost parent): the C++ walked `mParent`.

use std::ffi::{c_long, c_void, CString};

use crate::config::{open_file, read_token, DefineResult, Io, Lexer, SavedState, Token, TokenResult};
use crate::ffi::EeWonByteSink;
use crate::ffi_parse::{CrtCtype, EeWonWideUnitSink};
use crate::file::{EeWonFileReader, WON_FILE_OK};

type Parser = EeWonCfgParser;

/// The parser (0x88 bytes in the original; opaque to the C++).
pub struct EeWonCfgParser {
    lexer: Lexer,
    parent: *mut Parser,
    reader: EeWonFileReader,
    abort: Vec<u8>,
    warnings: Vec<u8>,
    warnings_as_errors: bool,
    unicode: bool,
    line: i32,
    file_path: Vec<u8>,
    file_name_and_path: Vec<u8>,
    ignore_path_on_include: bool,
    stopped: bool,
}

impl Default for Parser {
    fn default() -> Self {
        Self::new()
    }
}

impl Drop for Parser {
    fn drop(&mut self) {
        self.close_file();
    }
}

/// A `%s` argument: up to its first NUL.
fn c_str(text: &[u8]) -> &[u8] {
    &text[..text.iter().position(|&b| b == 0).unwrap_or(text.len())]
}

/// What `snprintf` into a 1,024-byte buffer keeps.
fn snprintf_1024(mut text: Vec<u8>) -> Vec<u8> {
    text.truncate(1023);
    text
}

impl Parser {
    /// The constructor: no file, no parent, every flag off.
    pub fn new() -> Self {
        EeWonCfgParser {
            lexer: Lexer::new(),
            parent: std::ptr::null_mut(),
            reader: EeWonFileReader::new(),
            abort: Vec::new(),
            warnings: Vec::new(),
            warnings_as_errors: false,
            unicode: false,
            line: 0,
            file_path: Vec::new(),
            file_name_and_path: Vec::new(),
            ignore_path_on_include: false,
            stopped: false,
        }
    }

    fn io(&mut self) -> (&mut Lexer, Io<'_, EeWonFileReader>) {
        (&mut self.lexer, Io { source: &mut self.reader, line: &mut self.line, unicode: self.unicode })
    }

    /// `Root`: the topmost parent (this parser when it has none).
    fn root(&mut self) -> *mut Parser {
        let mut root: *mut Parser = self;
        // SAFETY: the parents outlive their children (the C++ callers' locals, as in the original)
        while !unsafe { (*root).parent }.is_null() {
            root = unsafe { (*root).parent };
        }
        root
    }

    /// `SetParent`: the parent, and its include-path and warnings-as-errors flags.
    ///
    /// # Safety
    /// `parent` is NULL or a parser that outlives this one.
    pub unsafe fn set_parent(&mut self, parent: *mut Parser) {
        self.parent = parent;
        if !parent.is_null() {
            let p = unsafe { &*parent };
            self.ignore_path_on_include = p.ignore_path_on_include;
            self.warnings_as_errors = p.warnings_as_errors;
        }
    }

    pub fn end_of_file(&mut self) -> bool {
        let (lexer, io) = self.io();
        lexer.end_of_file(&io)
    }

    pub fn get_char(&mut self) -> i32 {
        let (lexer, mut io) = self.io();
        lexer.get_char(&mut io)
    }

    pub fn peek_char(&mut self) -> i32 {
        let (lexer, mut io) = self.io();
        lexer.peek_char(&mut io)
    }

    pub fn unget_char(&mut self, ch: i32) {
        let (lexer, mut io) = self.io();
        lexer.unget_char(&mut io, ch);
    }

    pub fn unget_narrow(&mut self, text: &[u8]) {
        let (lexer, mut io) = self.io();
        lexer.unget_narrow(&mut io, text);
    }

    pub fn unget_wide(&mut self, units: &[u32]) {
        let (lexer, mut io) = self.io();
        lexer.unget_wide(&mut io, units);
    }

    pub fn skip_whitespace(&mut self) {
        let (lexer, mut io) = self.io();
        lexer.skip_whitespace(&mut io, &CrtCtype);
    }

    /// `SetError`: the error and " (file:line)", unless an error is set and `append` is off.
    pub fn set_error(&mut self, error: &[u8], append: bool) {
        if !self.abort.is_empty() && !append {
            return;
        }
        let mut location = b" (".to_vec();
        location.extend_from_slice(c_str(&self.file_name_and_path));
        location.extend_from_slice(format!(":{})", self.line).as_bytes());
        self.abort.extend_from_slice(error);
        self.abort.extend_from_slice(&snprintf_1024(location));
    }

    /// `AddWarning`: a "Warning (file: line) - text" line (also the error when warnings are errors), unless aborted.
    pub fn add_warning(&mut self, warning: &[u8]) {
        if !self.abort.is_empty() {
            return;
        }
        if self.warnings_as_errors {
            self.set_error(warning, false);
        }
        let mut line = b"Warning (".to_vec();
        line.extend_from_slice(c_str(&self.file_name_and_path));
        line.extend_from_slice(format!(": {}) - ", self.line).as_bytes());
        line.extend_from_slice(c_str(warning));
        line.push(b'\n');
        self.warnings.extend_from_slice(&snprintf_1024(line));
    }

    /// `CopyWarnings`: another parser's warnings appended.
    pub fn copy_warnings(&mut self, source: &Parser) {
        let more = source.warnings.clone();
        self.warnings.extend_from_slice(&more);
    }

    /// `GetAbortReason`: the error, first "Parse aborted." when there is none.
    pub fn abort_reason(&mut self) -> &[u8] {
        if self.abort.is_empty() {
            self.set_error(b"Parse aborted.", false);
        }
        &self.abort
    }

    /// `GetState`: the read position, the file, the line and the pushback.
    pub fn state(&self) -> SavedState {
        SavedState {
            pos: self.reader.pos() as c_long,
            path: self.file_name_and_path.clone(),
            line: self.line,
            back: self.lexer.back().to_vec(),
        }
    }

    /// `SetState`: back to a saved state, reopening its file when needed; false when the reopen fails (not rolled
    /// back). The status is the open's or the seek's failure (the C++ throws it), else `WON_FILE_OK`.
    pub fn set_state(&mut self, state: &SavedState) -> (bool, i32) {
        if !self.reader.is_open() || state.path != self.file_name_and_path {
            let (open, status) = self.open_file(&state.path);
            if status != WON_FILE_OK {
                return (open, status);
            }
            if !open {
                return (false, WON_FILE_OK);
            }
        }
        let status = self.reader.set_pos(state.pos as std::ffi::c_ulong);
        if status != WON_FILE_OK {
            return (true, status);
        }
        self.line = state.line;
        self.lexer.set_back(&state.back);
        (true, WON_FILE_OK)
    }

    pub fn close_file(&mut self) {
        self.reader.close();
    }

    /// `OpenFile`: the file (UTF-16 when it starts FF FE), loaded into memory, its directory noted. The status is
    /// the reader's failure (the C++ throws it).
    pub fn open_file(&mut self, path: &[u8]) -> (bool, i32) {
        self.close_file();
        self.line = 1;
        self.unicode = false;
        self.lexer.set_back(&[]);
        self.file_name_and_path = path.to_vec();
        let sep = path.iter().rposition(|&b| b == b'/').or_else(|| path.iter().rposition(|&b| b == b'\\'));
        self.file_path = match sep {
            Some(i) => path[..=i].to_vec(),
            None => Vec::new(),
        };
        let c = CString::new(c_str(path)).unwrap_or_default();
        // SAFETY: a C string
        unsafe { open_file(&mut self.reader, &mut self.unicode, c.as_ptr()) }
    }

    /// `Reset`: the defines, pushback, error and warnings dropped, line 1, not stopped, the file rewound.
    pub fn reset(&mut self) {
        self.lexer.reset();
        self.abort.clear();
        self.warnings.clear();
        self.line = 1;
        self.stopped = false;
        self.reader.rewind();
    }

    pub fn push_define_stack(&mut self) {
        // SAFETY: the root parser (see `root`)
        unsafe { (*self.root()).lexer.push_scope() };
    }

    pub fn pop_define_stack(&mut self) {
        unsafe { (*self.root()).lexer.pop_scope() };
    }

    /// `AddDefine`: into the root's innermost scope; the root warns about spaces or a redefinition ("Redefintion",
    /// the original's spelling). Always true.
    pub fn add_define(&mut self, name: &[u8], value: &[u8], redefine: bool) -> bool {
        let root = unsafe { &mut *self.root() };
        match root.lexer.add_define(name, value, redefine, &CrtCtype) {
            DefineResult::Spaces => root.add_warning(b"Not allowed to have spaces in define names."),
            DefineResult::Redefinition => root.add_warning(&[b"Redefintion of ".as_slice(), name].concat()),
            DefineResult::Ok => {}
        }
        true
    }

    /// `GetDefine`: from the root's innermost scope outwards.
    pub fn get_define(&mut self, name: &[u8]) -> Option<Vec<u8>> {
        let root = unsafe { &*self.root() };
        root.lexer.get_define(name, &CrtCtype).map(<[u8]>::to_vec)
    }

    /// `AddGroupDefine`: a named group at the state (this parser's when `None`) in the root's innermost scope; a
    /// name with spaces warns on this parser, a redefinition on the root. Always true.
    pub fn add_group_define(&mut self, name: &[u8], state: Option<&SavedState>) -> bool {
        let local;
        let state = match state {
            Some(s) => s,
            None => {
                local = self.state();
                &local
            }
        };
        let root = self.root();
        match unsafe { (*root).lexer.add_group(name, state, &CrtCtype) } {
            DefineResult::Spaces => self.add_warning(b"Not allowed to have spaces in define names."),
            DefineResult::Redefinition => {
                unsafe { &mut *root }.add_warning(&[b"Redefintion of ".as_slice(), name].concat())
            }
            DefineResult::Ok => {}
        }
        true
    }

    pub fn get_group_define(&mut self, name: &[u8]) -> Option<SavedState> {
        let root = unsafe { &*self.root() };
        root.lexer.get_group(name, &CrtCtype).cloned()
    }

    /// `ReadToken` (narrow, or wide with `wchar_t` of `wchar_size` bytes): the token in `token`; false at the end
    /// or on an error (which is set). At the end of the data the token is left alone (`None`).
    pub fn read_token(&mut self, token: &mut Token, to_end_line: bool, skip: bool) -> (bool, bool) {
        if self.end_of_file() {
            return (false, false);
        }
        let root = self.root();
        let me: *mut Parser = self;
        let root_lexer = if std::ptr::eq(root, me) { None } else { Some(unsafe { &(*root).lexer }) };
        let result = {
            let (lexer, mut io) = self.io();
            read_token(lexer, &mut io, &CrtCtype, root_lexer, token, to_end_line, skip)
        };
        let ok = match result {
            TokenResult::False => false,
            TokenResult::True => true,
            TokenResult::EndQuote => {
                self.set_error(b"Expecting end quote", false);
                false
            }
            TokenResult::ClosingDollar => {
                self.set_error(b"Expecting closing '$'", false);
                false
            }
            TokenResult::DefineNotFound(missing) => {
                self.set_error(&[b"Define not found: ".as_slice(), &missing].concat(), false);
                false
            }
        };
        (ok, true)
    }
}

// ---- the C ABI (the inline forwards of ee/wonapi/WONLobbyConfig.h)

/// A parser's plain member, for `ee_won_cfgp_field` / `ee_won_cfgp_set_field`.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgpField {
    /// `mLineNum`
    Line = 0,
    /// `mIsUnicode`
    Unicode = 1,
    /// `mStopped` (`IsStopped`; `Stop` sets it)
    Stopped = 2,
    /// `mWarningsAsErrors`
    WarningsAsErrors = 3,
    /// `mIgnorePathOnIncludeFile`
    IgnorePath = 4,
    /// whether the file reader is open (read only)
    Open = 5,
    /// whether `mAbortReason` is empty (read only)
    NoError = 6,
}

/// A parser's text member, for `ee_won_cfgp_text`.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgpText {
    /// `mAbortReason` as it is (no "Parse aborted.")
    Abort = 0,
    /// `mWarnings`
    Warnings = 1,
    /// `mFilePath` (the file's directory, with its separator)
    FilePath = 2,
    /// `mFileNameAndPath`
    FileName = 3,
}

unsafe fn p<'a>(parser: *mut EeWonCfgParser) -> &'a mut Parser {
    // SAFETY: a parser from ee_won_cfgp_new (each function's contract)
    unsafe { &mut *parser }
}

unsafe fn bytes<'a>(text: *const u8, length: usize) -> &'a [u8] {
    if text.is_null() || length == 0 {
        &[]
    } else {
        // SAFETY: `length` bytes (contract)
        unsafe { std::slice::from_raw_parts(text, length) }
    }
}

unsafe fn give(sink: EeWonByteSink, context: *mut c_void, data: &[u8]) {
    unsafe { sink(context, data.as_ptr(), data.len()) };
}

#[no_mangle]
pub extern "C" fn ee_won_cfgp_new() -> *mut EeWonCfgParser {
    Box::into_raw(Box::new(Parser::new()))
}

/// # Safety
/// `parser` is from `ee_won_cfgp_new` (or NULL), not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_free(parser: *mut EeWonCfgParser) {
    if !parser.is_null() {
        drop(unsafe { Box::from_raw(parser) });
    }
}

/// # Safety
/// `parser` is a parser; `parent` NULL or a parser that outlives it.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_set_parent(parser: *mut EeWonCfgParser, parent: *mut EeWonCfgParser) {
    unsafe { p(parser).set_parent(parent) };
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_field(parser: *mut EeWonCfgParser, field: EeWonCfgpField) -> i32 {
    let p = unsafe { p(parser) };
    match field {
        EeWonCfgpField::Line => p.line,
        EeWonCfgpField::Unicode => p.unicode as i32,
        EeWonCfgpField::Stopped => p.stopped as i32,
        EeWonCfgpField::WarningsAsErrors => p.warnings_as_errors as i32,
        EeWonCfgpField::IgnorePath => p.ignore_path_on_include as i32,
        EeWonCfgpField::Open => p.reader.is_open() as i32,
        EeWonCfgpField::NoError => p.abort.is_empty() as i32,
    }
}

/// Sets a plain member (the read-only ones are left alone).
///
/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_set_field(parser: *mut EeWonCfgParser, field: EeWonCfgpField, value: i32) {
    let p = unsafe { p(parser) };
    match field {
        EeWonCfgpField::Line => p.line = value,
        EeWonCfgpField::Unicode => p.unicode = value != 0,
        EeWonCfgpField::Stopped => p.stopped = value != 0,
        EeWonCfgpField::WarningsAsErrors => p.warnings_as_errors = value != 0,
        EeWonCfgpField::IgnorePath => p.ignore_path_on_include = value != 0,
        EeWonCfgpField::Open | EeWonCfgpField::NoError => {}
    }
}

/// A text member to the sink.
///
/// # Safety
/// `parser` is a parser; the sink takes the text.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_text(
    parser: *mut EeWonCfgParser,
    which: EeWonCfgpText,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let p = unsafe { p(parser) };
    let text = match which {
        EeWonCfgpText::Abort => &p.abort,
        EeWonCfgpText::Warnings => &p.warnings,
        EeWonCfgpText::FilePath => &p.file_path,
        EeWonCfgpText::FileName => &p.file_name_and_path,
    };
    unsafe { give(sink, context, text) };
}

/// `GetAbortReason` to the sink.
///
/// # Safety
/// As `ee_won_cfgp_text`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_abort_reason(
    parser: *mut EeWonCfgParser,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let text = unsafe { p(parser) }.abort_reason();
    unsafe { give(sink, context, text) };
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_end_of_file(parser: *mut EeWonCfgParser) -> bool {
    unsafe { p(parser) }.end_of_file()
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_get_char(parser: *mut EeWonCfgParser) -> i32 {
    unsafe { p(parser) }.get_char()
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_peek_char(parser: *mut EeWonCfgParser) -> i32 {
    unsafe { p(parser) }.peek_char()
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_unget_char(parser: *mut EeWonCfgParser, ch: i32) {
    unsafe { p(parser) }.unget_char(ch);
}

/// `UngetStr` of a narrow string (each `char` sign-extended).
///
/// # Safety
/// `parser` is a parser; `text` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_unget_narrow(parser: *mut EeWonCfgParser, text: *const u8, length: usize) {
    unsafe { p(parser) }.unget_narrow(unsafe { bytes(text, length) });
}

/// `UngetStr` of a wide string (`wchar_t` values widened to 32 bits).
///
/// # Safety
/// `parser` is a parser; `units` holds `count` values.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_unget_wide(parser: *mut EeWonCfgParser, units: *const u32, count: usize) {
    let units =
        if units.is_null() || count == 0 { &[][..] } else { unsafe { std::slice::from_raw_parts(units, count) } };
    unsafe { p(parser) }.unget_wide(units);
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_skip_whitespace(parser: *mut EeWonCfgParser) {
    unsafe { p(parser) }.skip_whitespace();
}

/// # Safety
/// `parser` is a parser; `text` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_set_error(
    parser: *mut EeWonCfgParser,
    text: *const u8,
    length: usize,
    append: bool,
) {
    unsafe { p(parser) }.set_error(unsafe { bytes(text, length) }, append);
}

/// # Safety
/// `parser` is a parser; `text` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_add_warning(parser: *mut EeWonCfgParser, text: *const u8, length: usize) {
    unsafe { p(parser) }.add_warning(unsafe { bytes(text, length) });
}

/// # Safety
/// Both are parsers (they may be the same one).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_copy_warnings(parser: *mut EeWonCfgParser, source: *mut EeWonCfgParser) {
    let more = unsafe { &*source }.warnings.clone();
    unsafe { p(parser) }.warnings.extend_from_slice(&more);
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_close_file(parser: *mut EeWonCfgParser) {
    unsafe { p(parser) }.close_file();
}

/// `OpenFile`: whether the file opened; `status` the reader's failure (the C++ throws it) or 0.
///
/// # Safety
/// `parser` is a parser; `path` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_open_file(
    parser: *mut EeWonCfgParser,
    path: *const u8,
    length: usize,
    status: &mut i32,
) -> bool {
    let (open, s) = unsafe { p(parser) }.open_file(unsafe { bytes(path, length) });
    *status = s;
    open
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_reset(parser: *mut EeWonCfgParser) {
    unsafe { p(parser) }.reset();
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_push_define_stack(parser: *mut EeWonCfgParser) {
    unsafe { p(parser) }.push_define_stack();
}

/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_pop_define_stack(parser: *mut EeWonCfgParser) {
    unsafe { p(parser) }.pop_define_stack();
}

/// `AddDefine` (always true).
///
/// # Safety
/// `parser` is a parser; the texts hold their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_add_define(
    parser: *mut EeWonCfgParser,
    name: *const u8,
    name_len: usize,
    value: *const u8,
    value_len: usize,
    redefine: bool,
) -> bool {
    unsafe { p(parser) }.add_define(unsafe { bytes(name, name_len) }, unsafe { bytes(value, value_len) }, redefine)
}

/// `GetDefine`: the value to the sink, or false.
///
/// # Safety
/// `parser` is a parser; `name` holds `name_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_get_define(
    parser: *mut EeWonCfgParser,
    name: *const u8,
    name_len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match unsafe { p(parser) }.get_define(unsafe { bytes(name, name_len) }) {
        Some(v) => {
            unsafe { give(sink, context, &v) };
            true
        }
        None => false,
    }
}

fn saved(s: &crate::config::EeWonConfigState) -> SavedState {
    SavedState {
        pos: s.pos,
        path: unsafe { bytes(s.path, s.path_len) }.to_vec(),
        line: s.line,
        back: if s.back.is_null() || s.back_len == 0 {
            Vec::new()
        } else {
            // SAFETY: `back_len` units (the state's contract)
            unsafe { std::slice::from_raw_parts(s.back, s.back_len) }.to_vec()
        },
    }
}

unsafe fn give_state(sink: crate::config::EeWonConfigStateSink, context: *mut c_void, s: &SavedState) {
    let state = crate::config::EeWonConfigState {
        pos: s.pos,
        path: s.path.as_ptr(),
        path_len: s.path.len(),
        line: s.line,
        back: s.back.as_ptr(),
        back_len: s.back.len(),
    };
    unsafe { sink(context, &state) };
}

/// `GetState` to the sink.
///
/// # Safety
/// `parser` is a parser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_get_state(
    parser: *mut EeWonCfgParser,
    sink: crate::config::EeWonConfigStateSink,
    context: *mut c_void,
) {
    let s = unsafe { p(parser) }.state();
    unsafe { give_state(sink, context, &s) };
}

/// `SetState`: false when the reopen failed; `status` the open's or the seek's failure (the C++ throws it) or 0.
///
/// # Safety
/// `parser` is a parser; `state` a state.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_set_state(
    parser: *mut EeWonCfgParser,
    state: *const crate::config::EeWonConfigState,
    status: &mut i32,
) -> bool {
    let (ok, s) = unsafe { p(parser) }.set_state(&saved(unsafe { &*state }));
    *status = s;
    ok
}

/// `AddGroupDefine` at the state, or at this parser's when `state` is NULL (always true).
///
/// # Safety
/// `parser` is a parser; `name` holds `name_len` bytes; `state` NULL or a state.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_add_group_define(
    parser: *mut EeWonCfgParser,
    name: *const u8,
    name_len: usize,
    state: *const crate::config::EeWonConfigState,
) -> bool {
    let state = if state.is_null() { None } else { Some(saved(unsafe { &*state })) };
    unsafe { p(parser) }.add_group_define(unsafe { bytes(name, name_len) }, state.as_ref())
}

/// `GetGroupDefine`: the state to the sink, or false.
///
/// # Safety
/// `parser` is a parser; `name` holds `name_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_get_group_define(
    parser: *mut EeWonCfgParser,
    name: *const u8,
    name_len: usize,
    sink: crate::config::EeWonConfigStateSink,
    context: *mut c_void,
) -> bool {
    match unsafe { p(parser) }.get_group_define(unsafe { bytes(name, name_len) }) {
        Some(s) => {
            unsafe { give_state(sink, context, &s) };
            true
        }
        None => false,
    }
}

/// `ReadToken`: narrow when `wchar_size` is 0 (to `narrow`), else wide with `wchar_t` of that size (to `wide`);
/// nothing is given at the end of the data. False at the end or on an error (which is set).
///
/// # Safety
/// `parser` is a parser; the sinks take the token.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgp_read_token(
    parser: *mut EeWonCfgParser,
    wchar_size: u32,
    to_end_line: bool,
    skip: bool,
    narrow: EeWonByteSink,
    wide: EeWonWideUnitSink,
    context: *mut c_void,
) -> bool {
    let mut n = Vec::new();
    let mut w = Vec::new();
    let mut token = if wchar_size == 0 { Token::Narrow(&mut n) } else { Token::Wide(&mut w, wchar_size) };
    let (ok, read) = unsafe { p(parser) }.read_token(&mut token, to_end_line, skip);
    if read {
        if wchar_size == 0 {
            unsafe { give(narrow, context, &n) };
        } else {
            unsafe { wide(context, w.as_ptr(), w.len()) };
        }
    }
    ok
}

#[cfg(test)]
#[path = "cfgparser_tests.rs"]
mod tests;
