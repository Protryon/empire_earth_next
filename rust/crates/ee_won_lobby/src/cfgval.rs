//! WON's typed config values (R6 WON lobby and network round 35): ConfigValueBase and its templates
//! (ConfigValue<T>, SimpleConfigValue<T>, ConfigStringValue<T>, ConfigListValue<T>; the original's ConfigValue.cpp,
//! then WONConfig/ConfigObject.cpp and ee/wonapi/WONConfigValue.h) whole in Rust. A value is bound to a member of a
//! C++ config group (`ConfigObject::KeyAdd`, `RegisterIntValue`, `RegisterIntListListValue`) and the group's Rust
//! object (ee_won_net cfgobject.rs) holds it through its host's value ops (`ValAddRef` .. `ValSetDefault`, which
//! ConfigObject.cpp's host forwards here). Its references, default flag, kind, target and default are this object's.
//!
//! The scalar targets (`bool`, `unsigned short`, `int`) are written in place; the C++ containers (`std::string`,
//! `std::wstring`, `std::list<std::string>`, `std::list<std::list<int> >`) through one-statement host ops
//! ([`EeWonCfgValOp`], ConfigObject.cpp's `CfgValHost`). The parses are ee_won_net's parser (ffi_parse.rs), as the
//! C++ StringParser's were.
//!
//! Reach: the lobby's configs (WONLobby.cfg, the layouts) read the bool, unsigned short, string, wide string and
//! string list kinds through Init and Read in every LAN game; Write, Copy and the int and int-list-list kinds (the
//! drivers' TitleConfig copy) only in won_cfgobject_test, the golden against the deleted classes.

use std::cell::Cell;
use std::ffi::c_void;
use std::sync::atomic::{AtomicUsize, Ordering};

use ee_won_net::cfgobject::EeWonCfgStringParser;
use ee_won_net::ffi::EeWonByteSink;
use ee_won_net::ffi_parse::{
    ee_won_parser_check_next_char, ee_won_parser_format_bool, ee_won_parser_format_i64, ee_won_parser_read_bool,
    ee_won_parser_read_int, ee_won_parser_read_string, ee_won_parser_read_wstring,
};

#[cfg(windows)]
type WChar = u16;
#[cfg(not(windows))]
type WChar = u32;

/// The host's ops: each one C++ statement on a container target `t` (`(op, t, a, b, c) -> result`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgValOp {
    /// `((std::string*)t)->assign((const char*)a, (size_t)b)`.
    StrSet = 1,
    /// `*(std::string*)t = (const char*)a` (a default text).
    StrSetC,
    /// `((std::string*)t)->erase()`.
    StrErase,
    /// `((EeWonByteSink)a)((void*)b, the bytes of *(std::string*)t)`.
    StrOut,
    /// `*(std::string*)t = *(const std::string*)a`.
    StrCopy,
    /// `((std::wstring*)t)->assign((const wchar_t*)a, (size_t)b)`.
    WStrSet,
    /// `*(std::wstring*)t = (const wchar_t*)a` (a default text).
    WStrSetC,
    /// `((std::wstring*)t)->erase()`.
    WStrErase,
    /// `((EeWonCfgValUnits)a)((void*)b, the wchar_t units of *(std::wstring*)t)`.
    WStrOut,
    /// `*(std::wstring*)t = *(const std::wstring*)a`.
    WStrCopy,
    /// `((std::list<std::string>*)t)->clear()`.
    ListClear,
    /// `((std::list<std::string>*)t)->push_back(std::string((const char*)a, (size_t)b))`.
    ListPush,
    /// `((std::list<std::string>*)t)->size()`.
    ListCount,
    /// `((EeWonByteSink)b)((void*)c, the bytes of the list's item a)`.
    ListOut,
    /// `*(std::list<std::string>*)t = *(const std::list<std::string>*)a`.
    ListCopy,
    /// `((std::list<std::list<int> >*)t)->clear()`.
    LlClear,
    /// `((std::list<std::list<int> >*)t)->push_back(std::list<int>())`.
    LlPushGroup,
    /// `((std::list<std::list<int> >*)t)->back().push_back((int)a)`.
    LlPushInt,
    /// `((std::list<std::list<int> >*)t)->size()`.
    LlCount,
    /// The size of the list's group a.
    LlGroupCount,
    /// The int b of the list's group a.
    LlGet,
    /// `*(std::list<std::list<int> >*)t = *(const std::list<std::list<int> >*)a`.
    LlCopy,
}

/// The host: `(op, t, a, b, c) -> result`.
pub type EeWonCfgValHostFn =
    Option<unsafe extern "C" fn(op: EeWonCfgValOp, t: *mut c_void, a: isize, b: isize, c: isize) -> isize>;

/// Receives a wide string's `wchar_t` units; valid only during the call.
pub type EeWonCfgValUnits = unsafe extern "C" fn(context: *mut c_void, units: *const c_void, count: usize);

/// The calls a value takes from its group's host (ConfigObject.cpp's `ValAddRef` .. `ValSetDefault`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCfgValCall {
    /// `v->AddRef()`.
    AddRef = 1,
    /// `v->Release()` (freed at no reference).
    Release,
    /// `InitializeDefaultHook()`.
    Init,
    /// `ReadValue(*(StringParser*)a)` (0 or 1).
    Read,
    /// `WriteValue(s)` into an empty text, given to the sink `a` with the context `b`.
    Write,
    /// `CopyValue((ConfigValueBase*)a)`: the value of the same type copied, then its default flag.
    Copy,
    /// `mIsDefault` (0 or 1).
    IsDefault,
    /// `mIsDefault = a != 0`.
    SetDefault,
}

static HOST: AtomicUsize = AtomicUsize::new(0);

/// Registers the containers' host (ConfigObject.cpp's, at start; a driver's).
#[no_mangle]
pub extern "C" fn ee_won_cfgval_set_host(host: EeWonCfgValHostFn) {
    HOST.store(host.map_or(0, |f| f as usize), Ordering::Release);
}

fn h(op: EeWonCfgValOp, t: *mut c_void, a: isize, b: isize, c: isize) -> isize {
    let p = HOST.load(Ordering::Acquire);
    if p == 0 {
        return 0;
    }
    // SAFETY: a value `ee_won_cfgval_set_host` stored from a host function pointer
    let f = unsafe {
        std::mem::transmute::<usize, unsafe extern "C" fn(EeWonCfgValOp, *mut c_void, isize, isize, isize) -> isize>(p)
    };
    // SAFETY: the host's contract (each op on a live target of its type)
    unsafe { f(op, t, a, b, c) }
}

/// A value's kind: its target (borrowed: the group's member), its default and its flags.
#[derive(Clone, Copy)]
enum Kind {
    /// `SimpleConfigValue<bool>`.
    Bool(*mut bool, bool),
    /// `SimpleConfigValue<unsigned short>`.
    U16(*mut u16, u16),
    /// `SimpleConfigValue<int>`.
    Int(*mut i32, i32),
    /// `ConfigStringValue<char>`: a `std::string`, the default text (borrowed, NULL for none).
    Str(*mut c_void, *const u8),
    /// `ConfigStringValue<wchar_t>`: a `std::wstring`, the default text (borrowed `wchar_t*`, NULL for none).
    WStr(*mut c_void, *const c_void),
    /// `ConfigListValue<std::list<std::string> >`: the default text (borrowed, NULL for none), whether it is wide,
    /// whether a read adds to the list.
    StrList(*mut c_void, *const c_void, bool, bool),
    /// `ConfigListValue<std::list<std::list<int> > >`: as the string list.
    IntListList(*mut c_void, *const c_void, bool, bool),
    /// A Rust-owned `std::string` (a record's member, e.g. lobbyconf.rs's): the default text (NUL-terminated, NULL
    /// for none).
    RStr(*mut Vec<u8>, *const u8),
    /// A Rust-owned `std::wstring`: the default text (NUL-terminated `wchar_t`, NULL for none).
    RWStr(*mut Vec<WChar>, *const WChar),
    /// A Rust-owned `std::list<std::string>`: the default text (narrow, NULL for none), whether a read adds to it.
    RStrList(*mut Vec<Vec<u8>>, *const u8, bool),
}

/// A NUL-terminated text's units (the default texts).
///
/// # Safety
/// `p` is NULL or NUL-terminated.
unsafe fn c_units<T: Copy + Default + PartialEq>(p: *const T) -> Vec<T> {
    let mut out = Vec::new();
    if p.is_null() {
        return out;
    }
    let mut i = 0;
    // SAFETY: the contract
    while unsafe { *p.add(i) } != T::default() {
        out.push(unsafe { *p.add(i) });
        i += 1;
    }
    out
}

/// One value (the C++ `ConfigValueBase*` the groups hold).
pub struct EeWonCfgVal {
    refs: Cell<i32>,
    is_default: Cell<bool>,
    kind: Kind,
}

fn unit(sp: &EeWonCfgStringParser) -> u32 {
    if sp.wide {
        std::mem::size_of::<WChar>() as u32
    } else {
        1
    }
}

unsafe extern "C" fn sink_vec(context: *mut c_void, bytes: *const u8, length: usize) {
    // SAFETY: the context is the caller's Vec<u8>; the bytes hold `length` bytes
    let out = unsafe { &mut *(context as *mut Vec<u8>) };
    if !bytes.is_null() && length > 0 {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(bytes, length) });
    }
}

unsafe extern "C" fn sink_wide(context: *mut c_void, units: *const u32, count: usize) {
    // SAFETY: the context is the caller's Vec<WChar>; `(wchar_t)unit` as StringParser's AppendUnits
    let out = unsafe { &mut *(context as *mut Vec<WChar>) };
    if !units.is_null() {
        out.extend(unsafe { std::slice::from_raw_parts(units, count) }.iter().map(|&u| u as WChar));
    }
}

unsafe extern "C" fn sink_units(context: *mut c_void, units: *const c_void, count: usize) {
    // SAFETY: the context is the caller's Vec<WChar>; the units are `count` wchar_t
    let out = unsafe { &mut *(context as *mut Vec<WChar>) };
    if !units.is_null() && count > 0 {
        out.extend_from_slice(unsafe { std::slice::from_raw_parts(units as *const WChar, count) });
    }
}

fn ctx<T>(v: &mut T) -> isize {
    v as *mut T as isize
}

/// `ReadString(s, false)`: the token's bytes (always read).
fn read_text(sp: &mut EeWonCfgStringParser) -> Vec<u8> {
    let mut out = Vec::new();
    let u = unit(sp);
    // SAFETY: the parser's text holds its units to a NUL
    unsafe { ee_won_parser_read_string(sp.text, u, &mut sp.pos, false, sink_vec, ctx(&mut out) as *mut c_void) };
    out
}

fn next_char(sp: &mut EeWonCfgStringParser, ch: u8) -> bool {
    let u = unit(sp);
    // SAFETY: as `read_text`
    unsafe { ee_won_parser_check_next_char(sp.text, u, &mut sp.pos, ch as i32) }
}

fn read_int(sp: &mut EeWonCfgStringParser) -> Option<i32> {
    let mut v = 0i32;
    let u = unit(sp);
    // SAFETY: as `read_text`
    unsafe { ee_won_parser_read_int(sp.text, u, &mut sp.pos, &mut v) }.then_some(v)
}

/// `StringParser::ReadValue(std::list<std::string>&)` (ReadStrings): an item read, then another after each ',';
/// never fails.
fn read_strings(sp: &mut EeWonCfgStringParser, t: *mut c_void) -> bool {
    read_strings_to(sp, &mut |item| {
        h(EeWonCfgValOp::ListPush, t, item.as_ptr() as isize, item.len() as isize, 0);
    })
}

fn read_strings_to(sp: &mut EeWonCfgStringParser, push: &mut dyn FnMut(Vec<u8>)) -> bool {
    loop {
        push(read_text(sp));
        if !next_char(sp, b',') {
            return true;
        }
    }
}

/// `StringParser::ReadValue(std::list<std::list<int> >&)`: "(1, 2), (3)"; on a failed element or a missing ')' the
/// groups (and elements) read so far stay.
fn read_int_list_list(sp: &mut EeWonCfgStringParser, t: *mut c_void) -> bool {
    while next_char(sp, b'(') {
        h(EeWonCfgValOp::LlPushGroup, t, 0, 0, 0);
        // ReadList: the elements up to a failed one (dropped)
        loop {
            match read_int(sp) {
                Some(v) => {
                    h(EeWonCfgValOp::LlPushInt, t, v as isize, 0, 0);
                }
                None => return false,
            }
            if !next_char(sp, b',') {
                break;
            }
        }
        if !next_char(sp, b')') {
            return false;
        }
        if !next_char(sp, b',') {
            break;
        }
    }
    true
}

fn format_i64(v: i64, out: &mut Vec<u8>) {
    // SAFETY: the sink appends to `out`
    unsafe { ee_won_parser_format_i64(v, sink_vec, ctx(out) as *mut c_void) };
}

/// The parser over a value's default text (`StringParser(mDefStr)`, `SetIsWString(mIsWString)`).
fn default_parser(text: *const c_void, wide: bool) -> EeWonCfgStringParser {
    EeWonCfgStringParser { text, pos: 0, wide }
}

impl EeWonCfgVal {
    fn new(kind: Kind) -> *mut EeWonCfgVal {
        Box::into_raw(Box::new(EeWonCfgVal { refs: Cell::new(0), is_default: Cell::new(false), kind }))
    }

    /// `InitializeDefaultHook`.
    ///
    /// # Safety
    /// The target is live (its group's member).
    unsafe fn init(&self) {
        use EeWonCfgValOp as O;
        match self.kind {
            // SAFETY: the contract (a scalar target in place)
            Kind::Bool(p, d) => unsafe { *p = d },
            Kind::U16(p, d) => unsafe { *p = d },
            Kind::Int(p, d) => unsafe { *p = d },
            Kind::Str(t, d) => {
                if d.is_null() {
                    h(O::StrErase, t, 0, 0, 0);
                } else {
                    h(O::StrSetC, t, d as isize, 0, 0);
                }
            }
            Kind::WStr(t, d) => {
                if d.is_null() {
                    h(O::WStrErase, t, 0, 0, 0);
                } else {
                    h(O::WStrSetC, t, d as isize, 0, 0);
                }
            }
            Kind::StrList(t, d, wide, _) => {
                h(O::ListClear, t, 0, 0, 0);
                if !d.is_null() {
                    read_strings(&mut default_parser(d, wide), t);
                }
            }
            Kind::IntListList(t, d, wide, _) => {
                h(O::LlClear, t, 0, 0, 0);
                if !d.is_null() {
                    read_int_list_list(&mut default_parser(d, wide), t);
                }
            }
            // SAFETY (the Rust targets): the contract
            Kind::RStr(t, d) => unsafe { *t = c_units(d) },
            Kind::RWStr(t, d) => unsafe { *t = c_units(d) },
            Kind::RStrList(t, d, _) => {
                let l = unsafe { &mut *t };
                l.clear();
                if !d.is_null() {
                    read_strings_to(&mut default_parser(d as *const c_void, false), &mut |i| l.push(i));
                }
            }
        }
    }

    /// `ReadValue(parser)`.
    ///
    /// # Safety
    /// As `init`; the parser's text holds its units to a NUL.
    unsafe fn read(&self, sp: &mut EeWonCfgStringParser) -> bool {
        use EeWonCfgValOp as O;
        match self.kind {
            Kind::Bool(p, _) => {
                let u = unit(sp);
                // SAFETY: the contract (true/yes/.. stored on success only)
                unsafe { ee_won_parser_read_bool(sp.text, u, &mut sp.pos, p) }
            }
            // the int stored on success only (the original assigned an uninitialised int after a failed read)
            Kind::U16(p, _) => match read_int(sp) {
                Some(v) => {
                    // SAFETY: the contract
                    unsafe { *p = v as u16 };
                    true
                }
                None => false,
            },
            Kind::Int(p, _) => match read_int(sp) {
                Some(v) => {
                    // SAFETY: the contract
                    unsafe { *p = v };
                    true
                }
                None => false,
            },
            Kind::Str(t, _) => {
                let text = read_text(sp);
                h(O::StrSet, t, text.as_ptr() as isize, text.len() as isize, 0);
                true
            }
            Kind::WStr(t, _) => {
                let mut text: Vec<WChar> = Vec::new();
                let u = unit(sp);
                // SAFETY: as `read_text`
                unsafe {
                    ee_won_parser_read_wstring(sp.text, u, &mut sp.pos, false, sink_wide, ctx(&mut text) as *mut c_void)
                };
                h(O::WStrSet, t, text.as_ptr() as isize, text.len() as isize, 0);
                true
            }
            Kind::StrList(t, _, _, additive) => {
                if !additive {
                    h(O::ListClear, t, 0, 0, 0);
                }
                read_strings(sp, t)
            }
            Kind::IntListList(t, _, _, additive) => {
                if !additive {
                    h(O::LlClear, t, 0, 0, 0);
                }
                read_int_list_list(sp, t)
            }
            Kind::RStr(t, _) => {
                // SAFETY: the contract
                unsafe { *t = read_text(sp) };
                true
            }
            Kind::RWStr(t, _) => {
                let mut text: Vec<WChar> = Vec::new();
                let u = unit(sp);
                // SAFETY: as `read_text`
                unsafe {
                    ee_won_parser_read_wstring(sp.text, u, &mut sp.pos, false, sink_wide, ctx(&mut text) as *mut c_void)
                };
                unsafe { *t = text };
                true
            }
            Kind::RStrList(t, _, additive) => {
                // SAFETY: the contract
                let l = unsafe { &mut *t };
                if !additive {
                    l.clear();
                }
                read_strings_to(sp, &mut |i| l.push(i))
            }
        }
    }

    /// `WriteValue(out)` (StringParser::ValueToString): the value's text appended.
    ///
    /// # Safety
    /// As `init`.
    unsafe fn write(&self, out: &mut Vec<u8>) {
        use EeWonCfgValOp as O;
        match self.kind {
            Kind::Bool(p, _) => {
                // SAFETY: the contract; True / False
                unsafe { ee_won_parser_format_bool(*p, sink_vec, ctx(out) as *mut c_void) }
            }
            // SAFETY (both): the contract
            Kind::U16(p, _) => format_i64(unsafe { *p } as i64, out),
            Kind::Int(p, _) => format_i64(unsafe { *p } as i64, out),
            Kind::Str(t, _) => {
                h(O::StrOut, t, sink_vec as EeWonByteSink as usize as isize, ctx(out), 0);
            }
            Kind::WStr(t, _) => {
                let mut w: Vec<WChar> = Vec::new();
                h(O::WStrOut, t, sink_units as EeWonCfgValUnits as usize as isize, ctx(&mut w), 0);
                // `s += WStringToString(v)`
                out.extend_from_slice(&ee_won_net::titleobj::narrow(&w));
            }
            // ", " between the items
            Kind::StrList(t, ..) => {
                let n = h(O::ListCount, t, 0, 0, 0);
                for i in 0..n {
                    h(O::ListOut, t, i, sink_vec as EeWonByteSink as usize as isize, ctx(out));
                    if i + 1 < n {
                        out.extend_from_slice(b", ");
                    }
                }
            }
            // "(a, b), (c)"
            Kind::IntListList(t, ..) => {
                let n = h(O::LlCount, t, 0, 0, 0);
                for i in 0..n {
                    out.push(b'(');
                    let m = h(O::LlGroupCount, t, i, 0, 0);
                    for j in 0..m {
                        format_i64(h(O::LlGet, t, i, j, 0) as i32 as i64, out);
                        if j + 1 < m {
                            out.extend_from_slice(b", ");
                        }
                    }
                    out.push(b')');
                    if i + 1 < n {
                        out.extend_from_slice(b", ");
                    }
                }
            }
            // SAFETY (the Rust targets): the contract
            Kind::RStr(t, _) => out.extend_from_slice(unsafe { &*t }),
            Kind::RWStr(t, _) => out.extend_from_slice(&ee_won_net::titleobj::narrow(unsafe { &*t })),
            Kind::RStrList(t, ..) => out.extend_from_slice(&unsafe { &*t }.join(&b", "[..])),
        }
    }

    /// `CopyValueHook(source)`: the value copied from one of the same type (`dynamic_cast<ConfigValue<T>*>`).
    ///
    /// # Safety
    /// Both targets are live.
    unsafe fn copy_hook(&self, from: &EeWonCfgVal) -> bool {
        use EeWonCfgValOp as O;
        match (self.kind, from.kind) {
            // SAFETY (the scalars): the contract
            (Kind::Bool(a, _), Kind::Bool(b, _)) => unsafe { *a = *b },
            (Kind::U16(a, _), Kind::U16(b, _)) => unsafe { *a = *b },
            (Kind::Int(a, _), Kind::Int(b, _)) => unsafe { *a = *b },
            (Kind::Str(a, _), Kind::Str(b, _)) => {
                h(O::StrCopy, a, b as isize, 0, 0);
            }
            (Kind::WStr(a, _), Kind::WStr(b, _)) => {
                h(O::WStrCopy, a, b as isize, 0, 0);
            }
            (Kind::StrList(a, ..), Kind::StrList(b, ..)) => {
                h(O::ListCopy, a, b as isize, 0, 0);
            }
            (Kind::IntListList(a, ..), Kind::IntListList(b, ..)) => {
                h(O::LlCopy, a, b as isize, 0, 0);
            }
            // SAFETY (the Rust targets): the contract (a copy of the source first: the two may be one)
            (Kind::RStr(a, _), Kind::RStr(b, _)) => unsafe { *a = (*b).clone() },
            (Kind::RWStr(a, _), Kind::RWStr(b, _)) => unsafe { *a = (*b).clone() },
            (Kind::RStrList(a, ..), Kind::RStrList(b, ..)) => unsafe { *a = (*b).clone() },
            _ => return false,
        }
        true
    }
}

/// `new SimpleConfigValue<bool>(*target, initial)`: a value with no reference.
///
/// # Safety
/// `target` outlives the value (its group's member).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_bool(target: *mut bool, initial: bool) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::Bool(target, initial))
}

/// `new SimpleConfigValue<unsigned short>(*target, initial)`.
///
/// # Safety
/// As `ee_won_cfgval_bool`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_u16(target: *mut u16, initial: u16) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::U16(target, initial))
}

/// `new SimpleConfigValue<int>(*target, initial)`.
///
/// # Safety
/// As `ee_won_cfgval_bool`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_int(target: *mut i32, initial: i32) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::Int(target, initial))
}

/// `new ConfigStringValue<char>(*(std::string*)target, initial)` (`initial` borrowed, NULL for none).
///
/// # Safety
/// As `ee_won_cfgval_bool`; `initial` outlives the value too.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_string(target: *mut c_void, initial: *const u8) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::Str(target, initial))
}

/// `new ConfigStringValue<wchar_t>(*(std::wstring*)target, initial)` (`initial` a borrowed `wchar_t*`, or NULL).
///
/// # Safety
/// As `ee_won_cfgval_string`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_wstring(target: *mut c_void, initial: *const c_void) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::WStr(target, initial))
}

/// `new ConfigListValue<std::list<std::string> >(*target, initial, additive)` (`initial` a borrowed text, narrow or
/// with `wide` a `wchar_t*`; NULL for none).
///
/// # Safety
/// As `ee_won_cfgval_string`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_string_list(
    target: *mut c_void,
    initial: *const c_void,
    wide: bool,
    additive: bool,
) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::StrList(target, initial, wide, additive))
}

/// `new ConfigListValue<std::list<std::list<int> > >(*target, initial, additive)`.
///
/// # Safety
/// As `ee_won_cfgval_string_list`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_int_list_list(
    target: *mut c_void,
    initial: *const c_void,
    wide: bool,
    additive: bool,
) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::IntListList(target, initial, wide, additive))
}

/// `ConfigStringValue<char>` over a Rust-owned text (a record's member; `initial` NUL-terminated, NULL for none).
///
/// # Safety
/// `target` and `initial` outlive the value.
pub unsafe fn rust_string(target: *mut Vec<u8>, initial: *const u8) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::RStr(target, initial))
}

/// `ConfigStringValue<wchar_t>` over a Rust-owned wide text.
///
/// # Safety
/// As `rust_string`.
pub unsafe fn rust_wstring(target: *mut Vec<WChar>, initial: *const WChar) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::RWStr(target, initial))
}

/// `ConfigListValue<std::list<std::string> >` over a Rust-owned list (narrow default text, NULL for none).
///
/// # Safety
/// As `rust_string`.
pub unsafe fn rust_string_list(target: *mut Vec<Vec<u8>>, initial: *const u8, additive: bool) -> *mut EeWonCfgVal {
    EeWonCfgVal::new(Kind::RStrList(target, initial, additive))
}

/// `StringParser(text).ReadValue(list)` into a Rust list (ReadStrings: an item, then another after each ','), the
/// lobby's hash file list (lobbyprv.rs). `text` is a NUL-terminated narrow text.
pub(crate) fn read_string_list(text: *const u8) -> Vec<Vec<u8>> {
    let mut sp = default_parser(text as *const c_void, false);
    let mut list = Vec::new();
    loop {
        list.push(read_text(&mut sp));
        if !next_char(&mut sp, b',') {
            return list;
        }
    }
}

/// A call on a value (`v`: one made above).
///
/// # Safety
/// `v` is a live value; `Read`'s `a` a C++ `StringParser*` (an `EeWonCfgStringParser`); `Write`'s `a` an
/// `EeWonByteSink` and `b` its context; `Copy`'s `a` another live value.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfgval_call(v: *mut EeWonCfgVal, call: EeWonCfgValCall, a: isize, b: isize) -> isize {
    if v.is_null() {
        return 0;
    }
    // SAFETY: the contract (no reference into it lives across a free below)
    let me = unsafe { &*v };
    match call {
        EeWonCfgValCall::AddRef => me.refs.set(me.refs.get() + 1),
        EeWonCfgValCall::Release => {
            me.refs.set(me.refs.get() - 1);
            if me.refs.get() <= 0 {
                // SAFETY: made by `EeWonCfgVal::new`; the last reference
                drop(unsafe { Box::from_raw(v) });
            }
        }
        EeWonCfgValCall::Init => unsafe { me.init() },
        EeWonCfgValCall::Read => {
            // SAFETY: the contract
            return unsafe { me.read(&mut *(a as *mut EeWonCfgStringParser)) } as isize;
        }
        EeWonCfgValCall::Write => {
            let mut out = Vec::new();
            unsafe { me.write(&mut out) };
            // SAFETY: the contract (the sink and its context)
            let sink: EeWonByteSink = unsafe { std::mem::transmute(a) };
            unsafe { sink(b as *mut c_void, out.as_ptr(), out.len()) };
        }
        EeWonCfgValCall::Copy => {
            let from = a as *const EeWonCfgVal;
            if from.is_null() {
                return 0;
            }
            // CopyValue: the default flag only after a copy
            // SAFETY: the contract
            if unsafe { me.copy_hook(&*from) } {
                me.is_default.set(unsafe { (*from).is_default.get() });
            }
        }
        EeWonCfgValCall::IsDefault => return me.is_default.get() as isize,
        EeWonCfgValCall::SetDefault => me.is_default.set(a != 0),
    }
    0
}

#[cfg(test)]
#[path = "cfgval_tests.rs"]
mod tests;
