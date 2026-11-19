//! WON's `ConfigObject` whole (WONConfig/ConfigObject.cpp, deleted in WON round 21): a config group's children,
//! child iteration, registered key values and key groups, and every method: the group header, the parse of a
//! group (its key values, child groups, instructions, `PreParse` / `PostParse`, the caught exceptions), skipping,
//! child groups, the include of a file or a group define, the conditions, the defines, the values' queries and copy.
//! The grammar's decisions are `configobj`'s, the parser `cfgparser`'s.
//!
//! The C++ class (ee/wonapi/WONConfigObject.h) keeps its vtable (WONGUI's resource and component configs, the
//! lobby's config override the hooks; TitleConfig is titleobj.rs's Rust owner and host), its name, base name and parent (read by the subclasses) and
//! the typed values (`ConfigValueBase`, bound to the subclasses' members): those are host ops. A host op that runs
//! a virtual catches what the original's frames let through: a `ConfigObjectException` (caught by the innermost
//! `ParseGroup`, here as [`Exc::Config`]) and a `FileReaderException` (never caught: [`Exc::File`] goes back to the
//! C++ entry, which throws it again). No Rust reference to an object lives across a host op (a hook re-enters).

use std::ffi::c_void;

use crate::cfgparser::EeWonCfgParser;
use crate::config::Token;
use crate::configobj::{
    condition_parses_first, group_instruction, group_start_step, include_path, is_else, optional_suffix, token_kind,
    value_instruction, EeWonCfgInstruction, EeWonCfgOptional, EeWonCfgStart, EeWonCfgToken,
};
use crate::file::status_text;
use crate::strcase::compare_no_case;

#[cfg(windows)]
type WChar = u16;
#[cfg(not(windows))]
type WChar = u32;

/// The C++ `StringParser` (ee/wonapi/WONLobbyParser.h, checked by its static_asserts): the text (narrow or
/// `wchar_t`), the cursor and the unit flag.
#[repr(C)]
pub struct EeWonCfgStringParser {
    pub text: *const c_void,
    pub pos: i32,
    pub wide: bool,
}

/// The host's ops. `o` is a C++ `ConfigObject*`, `v` a `ConfigValueBase*`, `p` a C++ `ConfigParser*`; texts are
/// a pointer and a length; `x` is the C++ entry's exception record. A virtual's op answers -1 when it threw (the
/// record filled).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgoOp {
    /// `o->AddRef()`
    ObjAddRef,
    /// `o->Release()`
    ObjRelease,
    /// `new ConfigObject` (no reference)
    ObjNew,
    /// a group's factory `a()` (no reference)
    ObjFactory,
    /// `o`'s Rust object
    ObjRust,
    /// `o->PreParse(p)`; a: p, x
    ObjPreParse,
    /// `o->PostParse(p)`; a: p, x
    ObjPostParse,
    /// `o->HandleInstruction(p, text)`; a: p, b c: the instruction, x
    ObjHandleInstruction,
    /// `o->HandleValueInstruction(p, key, *sp)`; a: p, b c: the key, d: the `StringParser`, x
    ObjHandleValueInstruction,
    /// `o->Reset(a)`, x
    ObjReset,
    /// `o->mParent = a`
    ObjSetParent,
    /// `o->mName` (a 0) or `o->mBaseName` (a 1) set to the text b c
    ObjSetName,
    /// `v->AddRef()`
    ValAddRef,
    /// `v->Release()`
    ValRelease,
    /// `v->InitializeDefaultHook()`
    ValInit,
    /// `v->ReadValue(*sp)`; a: the `StringParser`
    ValRead,
    /// `v->WriteValue(s)`: s to the sink a with context b
    ValWrite,
    /// `v->CopyValue(a)`
    ValCopy,
    /// `v->mIsDefault`
    ValIsDefault,
    /// `v->mIsDefault = a`
    ValSetDefault,
    /// `*(SmartPtr<ConfigObject>*)a = (ConfigObject*)b`
    GroupSet,
    /// `new ConfigParser` (a C++ handle over a new Rust parser)
    ParserNew,
    /// `delete p` (a)
    ParserFree,
    /// the handle `a`'s Rust parser
    ParserRust,
    /// the exception record x filled: a the kind (1 ConfigObjectException, 2 FileReaderException), b its flag, c d
    /// its text
    SetExc,
    /// the record x's text to the sink a with context b, then the record cleared; the answer its flag
    TakeExc,
}

/// The host: `(op, o, a, b, c, d, x)`.
pub type EeWonCfgoHost = unsafe extern "C" fn(
    op: EeWonCfgoOp,
    o: *mut c_void,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    x: *mut c_void,
) -> isize;

/// What a host op or a parse let through.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Exc {
    /// `ConfigObjectException(text, flag)`, recorded by the host.
    Config,
    /// `FileReaderException`, recorded by the host (or a reader status this module recorded).
    File,
}

type R<T> = Result<T, Exc>;

struct Group {
    factory: isize,
    group_ptr: isize,
    object: *mut c_void,
}

/// `ChildList::iterator`: a node, or the end (which a later `push_back` does not move).
#[derive(Clone, Copy, PartialEq, Eq)]
enum Itr {
    At(usize),
    End,
}

/// The config group (0x54 bytes in the original; opaque to the C++).
pub struct EeWonCfgObject {
    owner: *mut c_void,
    host: EeWonCfgoHost,
    children: Vec<*mut c_void>,
    itr: Itr,
    keys: Vec<(Vec<u8>, *mut c_void)>,
    groups: Vec<(Vec<u8>, Group)>,
}

/// One C++ entry's context: the host and its exception record.
#[derive(Clone, Copy)]
pub(crate) struct Cx {
    pub(crate) host: EeWonCfgoHost,
    pub(crate) x: *mut c_void,
}

unsafe extern "C" fn sink_vec(context: *mut c_void, bytes: *const u8, length: usize) {
    let out = unsafe { &mut *(context as *mut Vec<u8>) };
    if !bytes.is_null() && length > 0 {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

fn find<T>(map: &[(Vec<u8>, T)], key: &[u8]) -> Option<usize> {
    map.iter().position(|(k, _)| compare_no_case(k, key) == 0)
}

/// The position a new key takes (the map's order: `StringLessNoCase`), or `None` when the key is there.
fn insert_at<T>(map: &[(Vec<u8>, T)], key: &[u8]) -> Option<usize> {
    let mut at = map.len();
    for (i, (k, _)) in map.iter().enumerate() {
        let c = compare_no_case(k, key);
        if c == 0 {
            return None;
        }
        if c > 0 {
            at = i;
            break;
        }
    }
    Some(at)
}

/// A `%s`-free C string's bytes (up to the first NUL), as `std::string(const char*)` reads them.
fn up_to_nul(b: &[u8]) -> &[u8] {
    &b[..b.iter().position(|&c| c == 0).unwrap_or(b.len())]
}

impl Cx {
    pub(crate) fn op(&self, op: EeWonCfgoOp, o: *mut c_void, a: isize, b: isize, c: isize, d: isize) -> isize {
        // SAFETY: the C++ host (its contract: every op on live objects)
        unsafe { (self.host)(op, o, a, b, c, d, self.x) }
    }
    pub(crate) fn hook(&self, op: EeWonCfgoOp, o: *mut c_void, a: isize, b: isize, c: isize, d: isize) -> R<bool> {
        match self.op(op, o, a, b, c, d) {
            -1 => Err(Exc::Config),
            -2 => Err(Exc::File),
            r => Ok(r != 0),
        }
    }
    fn rust(&self, o: *mut c_void) -> *mut EeWonCfgObject {
        self.op(EeWonCfgoOp::ObjRust, o, 0, 0, 0, 0) as *mut EeWonCfgObject
    }
    pub(crate) fn parser<'a>(self, p: *mut c_void) -> &'a mut EeWonCfgParser {
        // SAFETY: a live C++ parser's Rust parser; used only between host ops
        unsafe {
            &mut *(self.op(EeWonCfgoOp::ParserRust, std::ptr::null_mut(), p as isize, 0, 0, 0) as *mut EeWonCfgParser)
        }
    }
    fn set_name(&self, o: *mut c_void, which: isize, text: &[u8]) {
        self.op(EeWonCfgoOp::ObjSetName, o, which, text.as_ptr() as isize, text.len() as isize, 0);
    }
    fn file_exc(&self, status: i32) -> Exc {
        let text = status_text(status).to_bytes();
        self.op(EeWonCfgoOp::SetExc, std::ptr::null_mut(), 2, 0, text.as_ptr() as isize, text.len() as isize);
        Exc::File
    }
    fn val_write(&self, v: *mut c_void, out: &mut Vec<u8>) {
        self.op(
            EeWonCfgoOp::ValWrite,
            v,
            sink_vec as unsafe extern "C" fn(*mut c_void, *const u8, usize) as usize as isize,
            out as *mut Vec<u8> as isize,
            0,
            0,
        );
    }
}

/// A token read into a `wchar_t` buffer (NUL-terminated) for a `StringParser`.
fn wide_buffer(units: &[u32]) -> Vec<WChar> {
    let mut w: Vec<WChar> = units.iter().map(|&u| u as WChar).collect();
    w.push(0);
    w
}

impl EeWonCfgObject {
    fn me<'a>(o: *mut EeWonCfgObject) -> &'a mut EeWonCfgObject {
        // SAFETY: a live object, borrowed only between host ops (see the module's note)
        unsafe { &mut *o }
    }
}

/// `Clear`: the children dropped (front to back), the iteration restarted.
fn clear(cx: Cx, o: *mut EeWonCfgObject) {
    let children = std::mem::take(&mut EeWonCfgObject::me(o).children);
    EeWonCfgObject::me(o).itr = Itr::End;
    for c in children {
        cx.op(EeWonCfgoOp::ObjRelease, c, 0, 0, 0, 0);
    }
    rewind(o);
}

fn rewind(o: *mut EeWonCfgObject) {
    let me = EeWonCfgObject::me(o);
    me.itr = if me.children.is_empty() { Itr::End } else { Itr::At(0) };
}

fn add_child(cx: Cx, o: *mut EeWonCfgObject, child: *mut c_void) {
    let owner = EeWonCfgObject::me(o).owner;
    cx.op(EeWonCfgoOp::ObjSetParent, child, owner as isize, 0, 0, 0);
    cx.op(EeWonCfgoOp::ObjAddRef, child, 0, 0, 0, 0);
    EeWonCfgObject::me(o).children.push(child);
}

/// The base `Reset`: the children dropped, the names too when asked, every key back to its default, every group
/// pointer cleared.
pub(crate) fn reset(cx: Cx, o: *mut EeWonCfgObject, reset_name: bool) {
    clear(cx, o);
    let owner = EeWonCfgObject::me(o).owner;
    if reset_name {
        cx.set_name(owner, 0, b"");
        cx.set_name(owner, 1, b"");
    }
    let values: Vec<*mut c_void> = EeWonCfgObject::me(o).keys.iter().map(|(_, v)| *v).collect();
    for v in values {
        cx.op(EeWonCfgoOp::ValInit, v, 0, 0, 0, 0);
        cx.op(EeWonCfgoOp::ValSetDefault, v, 1, 0, 0, 0);
    }
    let ptrs: Vec<isize> = EeWonCfgObject::me(o).groups.iter().map(|(_, g)| g.group_ptr).collect();
    for p in ptrs {
        if p != 0 {
            cx.op(EeWonCfgoOp::GroupSet, std::ptr::null_mut(), p, 0, 0, 0);
        }
    }
}

/// `QueryValue`: the value's text appended, with "  [DEF]" while it is the default.
fn query_value(cx: Cx, v: *mut c_void, out: &mut Vec<u8>) {
    cx.val_write(v, out);
    if cx.op(EeWonCfgoOp::ValIsDefault, v, 0, 0, 0, 0) != 0 {
        out.extend_from_slice(b"  [DEF]");
    }
}

/// Where `ReadGroupStart` puts the name and the base name: the object's own members or the caller's locals.
enum Names<'a> {
    Own(*mut c_void),
    Local(&'a mut Option<Vec<u8>>, &'a mut Option<Vec<u8>>),
}

fn read_string(text: &[u8], stop: bool) -> Vec<u8> {
    let mut t = up_to_nul(text).to_vec();
    t.push(0);
    let mut pos = 0i32;
    let mut out = Vec::new();
    unsafe {
        crate::ffi_parse::ee_won_parser_read_string(
            t.as_ptr() as *const c_void,
            1,
            &mut pos,
            stop,
            sink_vec,
            &mut out as *mut Vec<u8> as *mut c_void,
        )
    };
    out
}

/// `ReadGroupStart`: "[name] [: base] {"; a parse error unless it ends in '{'.
fn read_group_start(cx: Cx, p: *mut c_void, mut names: Names) -> bool {
    let mut state = 0i32;
    let mut token = Vec::new();
    loop {
        let (ok, _) = cx.parser(p).read_token(&mut Token::Narrow(&mut token), false, false);
        if !ok {
            return false;
        }
        match group_start_step(&mut state, &token) {
            step @ (EeWonCfgStart::Name | EeWonCfgStart::Base) => {
                let base = step == EeWonCfgStart::Base;
                let read = read_string(&token, false);
                match &mut names {
                    Names::Own(owner) => cx.set_name(*owner, base as isize, &read),
                    Names::Local(name, inherited) => {
                        if base {
                            **inherited = Some(read)
                        } else {
                            **name = Some(read)
                        }
                    }
                }
            }
            EeWonCfgStart::Colon => {}
            EeWonCfgStart::Open => return true,
            EeWonCfgStart::Error => {
                cx.parser(p).set_error(&[b"Expecting '{' but got ".as_slice(), &token].concat(), false);
                return false;
            }
        }
    }
}

struct Scope(Cx, *mut c_void);
impl Drop for Scope {
    fn drop(&mut self) {
        self.0.parser(self.1).pop_define_stack();
    }
}
fn scope(cx: Cx, p: *mut c_void) -> Scope {
    cx.parser(p).push_define_stack();
    Scope(cx, p)
}

/// `ParseTopLevel`: the file opened (when given), the group reset and its whole contents parsed.
pub(crate) fn parse_top_level(cx: Cx, o: *mut EeWonCfgObject, p: *mut c_void, path: Option<&[u8]>) -> R<bool> {
    if let Some(path) = path {
        let (open, status) = cx.parser(p).open_file(path);
        if status != 0 {
            return Err(cx.file_exc(status));
        }
        if !open {
            cx.parser(p).set_error(&[b"File not found: ".as_slice(), path].concat(), false);
            return Ok(false);
        }
    }
    let _scope = scope(cx, p);
    let owner = EeWonCfgObject::me(o).owner;
    cx.hook(EeWonCfgoOp::ObjReset, owner, 1, 0, 0, 0)?;
    parse_group(cx, o, p, false, false, true, true)
}

/// `ParseGroup`: key values and child groups up to the closing brace (or the end), unknown ones skipped with a
/// warning; a `ConfigObjectException` from a hook becomes the error (its flag set) or a warning.
fn parse_group(
    cx: Cx,
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    check: bool,
    skip: bool,
    pre_post: bool,
    read_start: bool,
) -> R<bool> {
    let owner = EeWonCfgObject::me(o).owner;
    let body = || -> R<Option<bool>> {
        if check && read_start && !read_group_start(cx, p, Names::Own(owner)) {
            return Ok(Some(false));
        }
        if pre_post && !skip && !cx.hook(EeWonCfgoOp::ObjPreParse, owner, p as isize, 0, 0, 0)? {
            return Ok(Some(false));
        }
        loop {
            {
                let parser = cx.parser(p);
                if !parser.has_no_error() || parser.is_stopped() {
                    break;
                }
            }
            let mut token = Vec::new();
            let (ok, _) = cx.parser(p).read_token(&mut Token::Narrow(&mut token), false, false);
            if !ok {
                if check {
                    cx.parser(p).set_error(b"Expecting '}'", false);
                    return Ok(Some(false));
                }
                break;
            }
            unsafe { crate::strcase::ee_won_case_bytes(token.as_mut_ptr(), token.len(), true) };
            match token_kind(&token) {
                EeWonCfgToken::Close => {
                    if !check {
                        cx.parser(p).set_error(b"Unexpected '}'", false);
                        return Ok(Some(false));
                    }
                    break;
                }
                EeWonCfgToken::Key => {
                    token.pop();
                    let mut value = Vec::new();
                    let mut wide = Vec::new();
                    let mut narrow_buf;
                    let wide_buf;
                    let sp = if cx.parser(p).is_unicode() {
                        let mut t = Token::Wide(&mut wide, std::mem::size_of::<WChar>() as u32);
                        if !cx.parser(p).read_token(&mut t, true, skip).0 {
                            return Ok(Some(false));
                        }
                        wide_buf = wide_buffer(&wide);
                        EeWonCfgStringParser { text: wide_buf.as_ptr() as *const c_void, pos: 0, wide: true }
                    } else {
                        if !cx.parser(p).read_token(&mut Token::Narrow(&mut value), true, skip).0 {
                            return Ok(Some(false));
                        }
                        narrow_buf = up_to_nul(&value).to_vec();
                        narrow_buf.push(0);
                        EeWonCfgStringParser { text: narrow_buf.as_ptr() as *const c_void, pos: 0, wide: false }
                    };
                    let mut sp = sp;
                    if !skip
                        && !cx.hook(
                            EeWonCfgoOp::ObjHandleValueInstruction,
                            owner,
                            p as isize,
                            token.as_ptr() as isize,
                            token.len() as isize,
                            &mut sp as *mut EeWonCfgStringParser as isize,
                        )?
                    {
                        let w = [b"Unexpected key/val: ".as_slice(), &token, b" ", &value].concat();
                        cx.parser(p).add_warning(&w);
                    }
                }
                EeWonCfgToken::Group => {
                    if !skip {
                        if !cx.hook(
                            EeWonCfgoOp::ObjHandleInstruction,
                            owner,
                            p as isize,
                            token.as_ptr() as isize,
                            token.len() as isize,
                            0,
                        )? {
                            cx.parser(p).add_warning(&[b"Unexpected child group: ".as_slice(), &token].concat());
                            skip_group(cx, p, true, true)?;
                        }
                    } else {
                        skip_group(cx, p, true, true)?;
                    }
                }
            }
        }
        if pre_post && !skip && !cx.hook(EeWonCfgoOp::ObjPostParse, owner, p as isize, 0, 0, 0)? {
            return Ok(Some(false));
        }
        Ok(None)
    };
    match body() {
        Ok(Some(r)) => return Ok(r),
        Ok(None) => {}
        Err(Exc::Config) => {
            // the host recorded the exception's text and flag in the record; this frame catches it
            let mut text = Vec::new();
            let flag = take_config_exc(cx, &mut text);
            if flag {
                cx.parser(p).set_error(&text, false);
            } else {
                cx.parser(p).add_warning(&text);
            }
        }
        Err(e) => return Err(e),
    }
    Ok(cx.parser(p).has_no_error())
}

/// The recorded `ConfigObjectException` taken out of the record (its flag returned).
fn take_config_exc(cx: Cx, text: &mut Vec<u8>) -> bool {
    cx.op(
        EeWonCfgoOp::TakeExc,
        std::ptr::null_mut(),
        sink_vec as unsafe extern "C" fn(*mut c_void, *const u8, usize) as usize as isize,
        text as *mut Vec<u8> as isize,
        0,
        0,
    ) != 0
}

/// `SkipGroup`: a group read past on a new plain object.
fn skip_group(cx: Cx, p: *mut c_void, check: bool, read_start: bool) -> R<bool> {
    let child = cx.op(EeWonCfgoOp::ObjNew, std::ptr::null_mut(), 0, 0, 0, 0) as *mut c_void;
    cx.op(EeWonCfgoOp::ObjAddRef, child, 0, 0, 0, 0);
    let r = parse_group(cx, cx.rust(child), p, check, true, false, read_start);
    cx.op(EeWonCfgoOp::ObjRelease, child, 0, 0, 0, 0);
    r
}

/// `ParseChild`: the child parsed in its own define scope, then added when asked.
pub(crate) fn parse_child(
    cx: Cx,
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    child: *mut c_void,
    add: bool,
    read_start: bool,
) -> R<bool> {
    let _scope = scope(cx, p);
    let owner = EeWonCfgObject::me(o).owner;
    cx.op(EeWonCfgoOp::ObjSetParent, child, owner as isize, 0, 0, 0);
    cx.hook(EeWonCfgoOp::ObjReset, child, read_start as isize, 0, 0, 0)?;
    if !parse_group(cx, cx.rust(child), p, true, false, true, read_start)? {
        return Ok(false);
    }
    if add {
        add_child(cx, o, child);
    }
    Ok(true)
}

/// `HandleChildGroup`: a registered group (new from its factory, or the fixed object) parsed, then stored in its
/// pointer or added.
fn handle_child_group(cx: Cx, o: *mut EeWonCfgObject, p: *mut c_void, at: usize) -> R<bool> {
    let _scope = scope(cx, p);
    let (factory, group_ptr, object) = {
        let g = &EeWonCfgObject::me(o).groups[at].1;
        (g.factory, g.group_ptr, g.object)
    };
    let child = if factory != 0 {
        cx.op(EeWonCfgoOp::ObjFactory, std::ptr::null_mut(), factory, 0, 0, 0) as *mut c_void
    } else {
        object
    };
    cx.op(EeWonCfgoOp::ObjAddRef, child, 0, 0, 0, 0);
    let r = (|| -> R<bool> {
        cx.hook(EeWonCfgoOp::ObjReset, child, 1, 0, 0, 0)?;
        let owner = EeWonCfgObject::me(o).owner;
        cx.op(EeWonCfgoOp::ObjSetParent, child, owner as isize, 0, 0, 0);
        if !parse_group(cx, cx.rust(child), p, true, false, true, true)? {
            return Ok(false);
        }
        if group_ptr != 0 {
            cx.op(EeWonCfgoOp::GroupSet, std::ptr::null_mut(), group_ptr, child as isize, 0, 0);
        } else if object.is_null() {
            add_child(cx, o, child);
        }
        Ok(true)
    })();
    cx.op(EeWonCfgoOp::ObjRelease, child, 0, 0, 0, 0);
    r
}

/// `IncludeFile`: another file (relative to this one) parsed into this group by a child parser; a missing
/// required file is an error.
fn include_file(cx: Cx, o: *mut EeWonCfgObject, p: *mut c_void, path: &[u8], required: bool) -> R<bool> {
    let file = {
        let parser = cx.parser(p);
        include_path(path, parser.ignore_path_on_include(), parser.file_path())
    };
    let included = cx.op(EeWonCfgoOp::ParserNew, std::ptr::null_mut(), 0, 0, 0, 0) as *mut c_void;
    let r = (|| -> R<bool> {
        let parent: *mut EeWonCfgParser = cx.parser(p);
        unsafe { cx.parser(included).set_parent(parent) };
        let (open, status) = cx.parser(included).open_file(&file);
        if status != 0 {
            return Err(cx.file_exc(status));
        }
        if open {
            let result = parse_group(cx, o, included, false, false, false, true)?;
            let more = cx.parser(included).warnings().to_vec();
            cx.parser(p).append_warnings(&more);
            if !result {
                let reason = cx.parser(included).abort_reason().to_vec();
                cx.parser(p).set_error(&reason, false);
                return Ok(false);
            }
            if cx.parser(included).is_stopped() {
                cx.parser(p).stop();
            }
        } else if required {
            cx.parser(p).set_error(&[b"File not found: ".as_slice(), &file].concat(), false);
            return Ok(false);
        }
        Ok(true)
    })();
    cx.op(EeWonCfgoOp::ParserFree, std::ptr::null_mut(), included as isize, 0, 0, 0);
    r
}

/// `IncludeGroup`: a GROUPDEFINE's group parsed into this group, the parser's position restored afterwards.
fn include_group(cx: Cx, o: *mut EeWonCfgObject, p: *mut c_void, name: &[u8], required: bool) -> R<bool> {
    let defined = match cx.parser(p).get_group_define(name) {
        Some(s) => s,
        None => {
            if required {
                cx.parser(p).add_warning(&[b"IncludeGroup not found: ".as_slice(), name].concat());
            }
            return Ok(true);
        }
    };
    let saved = cx.parser(p).state();
    let (_, status) = cx.parser(p).set_state(&defined);
    if status != 0 {
        return Err(cx.file_exc(status));
    }
    let result = parse_group(cx, o, p, true, false, false, false)?;
    let (_, status) = cx.parser(p).set_state(&saved);
    if status != 0 {
        return Err(cx.file_exc(status));
    }
    if !cx.parser(p).has_no_error() {
        cx.parser(p).set_error(b"IncludeGroup Error", true);
    }
    Ok(result)
}

/// The base `HandleInstruction`: GROUPDEFINE, IFDEFINED / IFNOTDEFINED .. else, a registered child group; false
/// when unknown.
pub(crate) fn handle_instruction(cx: Cx, o: *mut EeWonCfgObject, p: *mut c_void, instruction: &[u8]) -> R<bool> {
    let kind = group_instruction(instruction);
    if kind == EeWonCfgInstruction::GroupDefine {
        let (mut name, mut inherited) = (None, None);
        if !read_group_start(cx, p, Names::Local(&mut name, &mut inherited)) {
            return Ok(false);
        }
        cx.parser(p).add_group_define(name.as_deref().unwrap_or(&[]), None);
        return skip_group(cx, p, true, false);
    }
    if kind == EeWonCfgInstruction::IfDefined || kind == EeWonCfgInstruction::IfNotDefined {
        let (mut name, mut expected) = (None, None);
        if !read_group_start(cx, p, Names::Local(&mut name, &mut expected)) {
            return Ok(true);
        }
        let (name, expected) = (name.unwrap_or_default(), expected.unwrap_or_default());
        let actual = cx.parser(p).get_define(&name);
        let found = actual.is_some();
        let matches = found && compare_no_case(&expected, actual.as_deref().unwrap_or(&[])) == 0;
        let first = condition_parses_first(kind, found, expected.is_empty(), matches);
        if first {
            parse_group(cx, o, p, true, false, false, false)?;
        } else {
            skip_group(cx, p, true, false)?;
        }
        let mut token = Vec::new();
        cx.parser(p).read_token(&mut Token::Narrow(&mut token), false, false);
        if !is_else(&token) {
            cx.parser(p).unget_narrow(&token);
            return Ok(true);
        }
        cx.parser(p).read_token(&mut Token::Narrow(&mut token), false, false);
        if token != b"{" {
            cx.parser(p).set_error(b"Expecting '{'", false);
            return Ok(true);
        }
        if first {
            skip_group(cx, p, true, false)?;
        } else {
            parse_group(cx, o, p, true, false, false, false)?;
        }
        return Ok(true);
    }
    match find(&EeWonCfgObject::me(o).groups, instruction) {
        Some(at) => handle_child_group(cx, o, p, at),
        None => Ok(false),
    }
}

fn sp_unit(sp: &EeWonCfgStringParser) -> u32 {
    if sp.wide {
        std::mem::size_of::<WChar>() as u32
    } else {
        1
    }
}

fn sp_read_string(sp: &mut EeWonCfgStringParser, stop: bool) -> Vec<u8> {
    let mut out = Vec::new();
    unsafe {
        crate::ffi_parse::ee_won_parser_read_string(
            sp.text,
            sp_unit(sp),
            &mut sp.pos,
            stop,
            sink_vec,
            &mut out as *mut Vec<u8> as *mut c_void,
        )
    };
    out
}

/// The text from the cursor to the NUL: its bytes, or `WStringToString` of its `wchar_t`s.
fn sp_rest(sp: &EeWonCfgStringParser) -> Vec<u8> {
    if sp.wide {
        let mut units = Vec::new();
        let base = sp.text as *const WChar;
        let mut i = sp.pos as isize;
        loop {
            let u = unsafe { *base.offset(i) };
            if u == 0 {
                break;
            }
            units.push(u);
            i += 1;
        }
        let mut out = Vec::new();
        if !units.is_empty() {
            unsafe {
                crate::ffi::ee_won_utf8_from_wide(
                    units.as_ptr() as *const c_void,
                    std::mem::size_of::<WChar>() as u32,
                    units.len(),
                    sink_vec,
                    &mut out as *mut Vec<u8> as *mut c_void,
                )
            };
        }
        out
    } else {
        let base = sp.text as *const u8;
        let mut out = Vec::new();
        let mut i = sp.pos as isize;
        loop {
            let b = unsafe { *base.offset(i) };
            if b == 0 {
                break;
            }
            out.push(b);
            i += 1;
        }
        out
    }
}

/// The base `HandleValueInstruction`: DEFINE / REDEFINE, INCLUDEGROUP, INCLUDEFILE and the registered keys; false
/// for an unknown key.
pub(crate) fn handle_value_instruction(
    cx: Cx,
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    instruction: &[u8],
    sp: *mut EeWonCfgStringParser,
) -> R<bool> {
    let kind = value_instruction(instruction);
    // SAFETY: the C++ StringParser of the call
    let value = unsafe { &mut *sp };
    if kind == EeWonCfgInstruction::Define || kind == EeWonCfgInstruction::Redefine {
        let name = sp_read_string(value, true);
        unsafe { crate::ffi_parse::ee_won_parser_skip_whitespace(value.text, sp_unit(value), &mut value.pos) };
        let rest = sp_rest(value);
        cx.parser(p).add_define(&name, &rest, kind == EeWonCfgInstruction::Redefine);
        return Ok(true);
    }
    if kind == EeWonCfgInstruction::IncludeGroup || kind == EeWonCfgInstruction::IncludeFile {
        let group = kind == EeWonCfgInstruction::IncludeGroup;
        let target = sp_read_string(value, false);
        let comma = unsafe {
            crate::ffi_parse::ee_won_parser_check_next_char(value.text, sp_unit(value), &mut value.pos, b',' as i32)
        };
        let (read, is_optional) = if comma {
            let optional = sp_read_string(value, false);
            (true, compare_no_case(&optional, b"optional") == 0)
        } else {
            (false, false)
        };
        let mut required = true;
        match optional_suffix(comma, read, is_optional) {
            EeWonCfgOptional::Optional => required = false,
            EeWonCfgOptional::WarnNoString => cx.parser(p).add_warning(if group {
                b"Expecting string: 'optional' after include group, include is required.".as_slice()
            } else {
                b"Expecting string: 'optional' after include file, include is required.".as_slice()
            }),
            EeWonCfgOptional::WarnOtherWord => cx.parser(p).add_warning(if group {
                b"Expecting string: optional after include group, include is required.".as_slice()
            } else {
                b"Expecting string: optional after include file, include is required.".as_slice()
            }),
            EeWonCfgOptional::Required => {}
        }
        if group {
            include_group(cx, o, p, &target, required)?;
        } else {
            include_file(cx, o, p, &target, required)?;
        }
        return Ok(true);
    }
    let (key, v) = match find(&EeWonCfgObject::me(o).keys, instruction) {
        Some(at) => {
            let (k, v) = &EeWonCfgObject::me(o).keys[at];
            (k.clone(), *v)
        }
        None => return Ok(false),
    };
    // HandleKeyVal: no longer the default; a failed read warns
    cx.op(EeWonCfgoOp::ValSetDefault, v, 0, 0, 0, 0);
    if cx.op(EeWonCfgoOp::ValRead, v, sp as isize, 0, 0, 0) == 0 {
        cx.parser(p).add_warning(&[b"Failed to parse value for key: ".as_slice(), &key].concat());
    }
    Ok(true)
}

// ---- the C ABI (the inline forwards of ee/wonapi/WONConfigObject.h). `x` is the C++ entry's exception record:
// a function that answers through it returns -1 (ConfigObjectException) or -2 (FileReaderException) after filling
// it, else its result.

fn ret(r: R<bool>) -> i32 {
    match r {
        Ok(b) => b as i32,
        Err(Exc::Config) => -1,
        Err(Exc::File) => -2,
    }
}

unsafe fn obj<'a>(o: *mut EeWonCfgObject) -> &'a mut EeWonCfgObject {
    unsafe { &mut *o }
}

unsafe fn text<'a>(t: *const u8, n: usize) -> &'a [u8] {
    if t.is_null() || n == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(t, n) }
    }
}

fn cx_of(o: *mut EeWonCfgObject, x: *mut c_void) -> Cx {
    Cx { host: unsafe { obj(o) }.host, x }
}

/// A new group for the C++ object `owner`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_cfgo_new(owner: *mut c_void, host: EeWonCfgoHost) -> *mut EeWonCfgObject {
    Box::into_raw(Box::new(EeWonCfgObject {
        owner,
        host,
        children: Vec::new(),
        itr: Itr::End,
        keys: Vec::new(),
        groups: Vec::new(),
    }))
}

/// The destructor: the group objects, the values and the children released.
///
/// # Safety
/// `o` is from `ee_won_cfgo_new`, not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_free(o: *mut EeWonCfgObject) {
    if o.is_null() {
        return;
    }
    let me = unsafe { Box::from_raw(o) };
    let cx = Cx { host: me.host, x: std::ptr::null_mut() };
    for (_, g) in &me.groups {
        if !g.object.is_null() {
            cx.op(EeWonCfgoOp::ObjRelease, g.object, 0, 0, 0, 0);
        }
    }
    for (_, v) in &me.keys {
        cx.op(EeWonCfgoOp::ValRelease, *v, 0, 0, 0, 0);
    }
    for c in &me.children {
        cx.op(EeWonCfgoOp::ObjRelease, *c, 0, 0, 0, 0);
    }
}

/// # Safety
/// `o` is a group.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_clear(o: *mut EeWonCfgObject) {
    clear(cx_of(o, std::ptr::null_mut()), o);
}

/// # Safety
/// `o` is a group.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_rewind(o: *mut EeWonCfgObject) {
    rewind(o);
}

/// `AddChild`: the child held, this group its parent.
///
/// # Safety
/// `o` is a group, `child` a C++ ConfigObject.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_add_child(o: *mut EeWonCfgObject, child: *mut c_void) {
    add_child(cx_of(o, std::ptr::null_mut()), o, child);
}

/// `NextChild`: the iteration's next child (not held), or NULL at the end.
///
/// # Safety
/// `o` is a group.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_next_child(o: *mut EeWonCfgObject) -> *mut c_void {
    let me = unsafe { obj(o) };
    match me.itr {
        Itr::End => std::ptr::null_mut(),
        Itr::At(i) => {
            me.itr = if i + 1 < me.children.len() { Itr::At(i + 1) } else { Itr::End };
            me.children[i]
        }
    }
}

/// The number of children (`mChildList.size()`).
///
/// # Safety
/// `o` is a group.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_child_count(o: *mut EeWonCfgObject) -> usize {
    unsafe { obj(o) }.children.len()
}

/// `RegisterKeyValue`: the value under the key, held; an existing registration is kept (the new value released).
///
/// # Safety
/// `o` is a group; `key` holds `n` bytes; `v` a C++ ConfigValueBase.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_register_key_value(
    o: *mut EeWonCfgObject,
    key: *const u8,
    n: usize,
    v: *mut c_void,
) {
    let cx = cx_of(o, std::ptr::null_mut());
    let key = unsafe { text(key, n) };
    cx.op(EeWonCfgoOp::ValAddRef, v, 0, 0, 0, 0);
    match insert_at(&unsafe { obj(o) }.keys, key) {
        Some(at) => unsafe { obj(o) }.keys.insert(at, (key.to_vec(), v)),
        None => {
            cx.op(EeWonCfgoOp::ValRelease, v, 0, 0, 0, 0);
        }
    }
}

/// `RegisterKeyGroup`: a factory and group pointer (`object` NULL) or a fixed object (held) under the key; an
/// existing registration is kept (a new object released).
///
/// # Safety
/// `o` is a group; `key` holds `n` bytes; `object` NULL or a C++ ConfigObject.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_register_key_group(
    o: *mut EeWonCfgObject,
    key: *const u8,
    n: usize,
    factory: isize,
    group_ptr: isize,
    object: *mut c_void,
) {
    let cx = cx_of(o, std::ptr::null_mut());
    let key = unsafe { text(key, n) };
    if !object.is_null() {
        cx.op(EeWonCfgoOp::ObjAddRef, object, 0, 0, 0, 0);
    }
    match insert_at(&unsafe { obj(o) }.groups, key) {
        Some(at) => unsafe { obj(o) }.groups.insert(at, (key.to_vec(), Group { factory, group_ptr, object })),
        None => {
            if !object.is_null() {
                cx.op(EeWonCfgoOp::ObjRelease, object, 0, 0, 0, 0);
            }
        }
    }
}

/// `CopyKeyValsFrom`: each key both groups have (in their order, ignoring case) copied from `from`.
///
/// # Safety
/// Both are groups.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_copy_key_vals_from(o: *mut EeWonCfgObject, from: *mut EeWonCfgObject) {
    let cx = cx_of(o, std::ptr::null_mut());
    let a: Vec<(Vec<u8>, *mut c_void)> = unsafe { obj(o) }.keys.clone();
    let b: Vec<(Vec<u8>, *mut c_void)> = unsafe { obj(from) }.keys.clone();
    let (mut i, mut j) = (0, 0);
    while i < a.len() && j < b.len() {
        match compare_no_case(&a[i].0, &b[j].0) {
            c if c < 0 => i += 1,
            c if c > 0 => j += 1,
            _ => {
                cx.op(EeWonCfgoOp::ValCopy, a[i].1, b[j].1 as isize, 0, 0, 0);
                i += 1;
                j += 1;
            }
        }
    }
}

/// The base `Reset`.
///
/// # Safety
/// `o` is a group.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_reset(o: *mut EeWonCfgObject, reset_name: bool) {
    reset(cx_of(o, std::ptr::null_mut()), o, reset_name);
}

/// The base `GetKeyValue`: the value's text to the sink (to append), or false for an unknown key.
///
/// # Safety
/// `o` is a group; `key` holds `n` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_get_key_value(
    o: *mut EeWonCfgObject,
    key: *const u8,
    n: usize,
    sink: crate::ffi::EeWonByteSink,
    context: *mut c_void,
) -> bool {
    let cx = cx_of(o, std::ptr::null_mut());
    let v = match find(&unsafe { obj(o) }.keys, unsafe { text(key, n) }) {
        Some(at) => unsafe { obj(o) }.keys[at].1,
        None => return false,
    };
    let mut out = Vec::new();
    query_value(cx, v, &mut out);
    unsafe { sink(context, out.as_ptr(), out.len()) };
    true
}

/// The base `GetAllKeyValues`: one "key: value" line per key, to the sink (to append).
///
/// # Safety
/// `o` is a group.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_get_all_key_values(
    o: *mut EeWonCfgObject,
    sink: crate::ffi::EeWonByteSink,
    context: *mut c_void,
) {
    let cx = cx_of(o, std::ptr::null_mut());
    let keys: Vec<(Vec<u8>, *mut c_void)> = unsafe { obj(o) }.keys.clone();
    let mut out = Vec::new();
    for (k, v) in keys {
        out.extend_from_slice(&k);
        out.extend_from_slice(b": ");
        query_value(cx, v, &mut out);
        out.push(b'\n');
    }
    unsafe { sink(context, out.as_ptr(), out.len()) };
}

/// The base `SetKeyValue`: 0 parsed, 2 a parse error, 1 an unknown key.
///
/// # Safety
/// `o` is a group; `key` holds `n` bytes; `sp` a C++ StringParser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_set_key_value(
    o: *mut EeWonCfgObject,
    key: *const u8,
    n: usize,
    sp: *mut EeWonCfgStringParser,
) -> i32 {
    let cx = cx_of(o, std::ptr::null_mut());
    let v = match find(&unsafe { obj(o) }.keys, unsafe { text(key, n) }) {
        Some(at) => unsafe { obj(o) }.keys[at].1,
        None => return 1,
    };
    if cx.op(EeWonCfgoOp::ValRead, v, sp as isize, 0, 0, 0) != 0 {
        cx.op(EeWonCfgoOp::ValSetDefault, v, 0, 0, 0, 0);
        0
    } else {
        2
    }
}

/// `ReadGroupStart` into the caller's strings (to the sinks, each only when read).
///
/// # Safety
/// `o` is a group, `p` a C++ ConfigParser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_read_group_start(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    name: crate::ffi::EeWonByteSink,
    name_context: *mut c_void,
    base: crate::ffi::EeWonByteSink,
    base_context: *mut c_void,
) -> bool {
    let cx = cx_of(o, std::ptr::null_mut());
    let (mut n, mut b) = (None, None);
    let r = read_group_start(cx, p, Names::Local(&mut n, &mut b));
    if let Some(n) = n {
        unsafe { name(name_context, n.as_ptr(), n.len()) };
    }
    if let Some(b) = b {
        unsafe { base(base_context, b.as_ptr(), b.len()) };
    }
    r
}

/// `ParseTopLevel` (`path` NULL for none).
///
/// # Safety
/// `o` is a group, `p` a C++ ConfigParser, `x` the record; `path` NULL or holds `n` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_parse_top_level(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    path: *const u8,
    n: usize,
    x: *mut c_void,
) -> i32 {
    let path = if path.is_null() { None } else { Some(unsafe { text(path, n) }) };
    ret(parse_top_level(cx_of(o, x), o, p, path))
}

/// `ParseGroup`.
///
/// # Safety
/// As `ee_won_cfgo_parse_top_level`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_parse_group(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    check: bool,
    skip: bool,
    pre_post: bool,
    read_start: bool,
    x: *mut c_void,
) -> i32 {
    ret(parse_group(cx_of(o, x), o, p, check, skip, pre_post, read_start))
}

/// `SkipGroup`.
///
/// # Safety
/// As `ee_won_cfgo_parse_top_level`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_skip_group(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    check: bool,
    read_start: bool,
    x: *mut c_void,
) -> i32 {
    ret(skip_group(cx_of(o, x), p, check, read_start))
}

/// `ParseChild`.
///
/// # Safety
/// As `ee_won_cfgo_parse_top_level`; `child` a C++ ConfigObject.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_parse_child(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    child: *mut c_void,
    add: bool,
    read_start: bool,
    x: *mut c_void,
) -> i32 {
    ret(parse_child(cx_of(o, x), o, p, child, add, read_start))
}

/// `IncludeFile` (`group` false) or `IncludeGroup` (`group` true).
///
/// # Safety
/// As `ee_won_cfgo_parse_top_level`; `name` holds `n` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_include(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    group: bool,
    name: *const u8,
    n: usize,
    required: bool,
    x: *mut c_void,
) -> i32 {
    let name = unsafe { text(name, n) };
    let cx = cx_of(o, x);
    ret(if group { include_group(cx, o, p, name, required) } else { include_file(cx, o, p, name, required) })
}

/// The base `HandleInstruction`.
///
/// # Safety
/// As `ee_won_cfgo_parse_top_level`; `instruction` holds `n` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_handle_instruction(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    instruction: *const u8,
    n: usize,
    x: *mut c_void,
) -> i32 {
    let instruction = unsafe { text(instruction, n) }.to_vec();
    ret(handle_instruction(cx_of(o, x), o, p, &instruction))
}

/// The base `HandleValueInstruction`.
///
/// # Safety
/// As `ee_won_cfgo_handle_instruction`; `sp` a C++ StringParser.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgo_handle_value_instruction(
    o: *mut EeWonCfgObject,
    p: *mut c_void,
    instruction: *const u8,
    n: usize,
    sp: *mut EeWonCfgStringParser,
    x: *mut c_void,
) -> i32 {
    let instruction = unsafe { text(instruction, n) }.to_vec();
    ret(handle_value_instruction(cx_of(o, x), o, p, &instruction, sp))
}

#[cfg(test)]
#[path = "cfgobject_tests.rs"]
mod tests;
