//! The computer player's Lua runtime (`ee/EECPLua.h`, `EECPLuaState`'s body): one sandboxed, deterministic Lua 5.4
//! state per computer player over the vendored Lua (`src/ThirdParty/lua/ee_lua.c`, compiled by the C++ build with
//! its determinism macros), through `lua_api` (`mlua-sys`'s types and constants, `lua54` + `external`: no Lua of its
//! own; the functions declared `extern "C"`, see there).
//!
//! Every Lua API call is the one `EECPLua.cpp` made, in its order, so the state allocates the same blocks (the byte
//! count and the collector's pace are the same) and a script sees the same tables:
//!   - the libraries: base, table, string, math, utf8 opened by `luaL_requiref` in that order; the removed names;
//!     print, tostring, setmetatable, require and string.format replaced; each and ai.random/ai.keys added;
//!   - the count hook every [`HOOK_STEP`] instructions, re-armed to 1 once over the budget so that a pcall cannot
//!     swallow the abort; [`charge`] for the bindings' costs;
//!   - the CRT's realloc and free under a byte cap (the refusal sets `alloc_failed`; Lua collects and retries);
//!   - the error texts and statuses (`next.error` lines quote them).
//!
//! Lua raises errors with longjmp: the functions Lua calls, and every function between a protected call and a
//! raise, hold no value with a destructor (only raw pointers, integers and stack arrays). Values that own memory
//! (the error text, the converter's path) live outside the protected calls. The host is reached from Lua through
//! the allocator's user data, as a raw pointer: no Rust reference to it lives across a Lua call.
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"

use std::ffi::{c_char, c_int, c_void, CStr};
use std::mem::MaybeUninit;
use std::ptr;

use crate::lua_api::*;

use crate::lua_value::{self, EeAiLuaTreeOps, Key, Kind, OwnedKey, Tree, Value, MAX_DEPTH};

/// Instructions between two calls of the count hook (`kEECPLuaHookStep`).
pub(crate) const HOOK_STEP: i64 = 1000;

/// `EECPLuaStatus`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u32)]
pub enum Status {
    Ok = 0,
    NoState,
    NotLoaded,
    NoFile,
    Syntax,
    Runtime,
    Budget,
    Memory,
    BadValue,
}

/// `EECPLuaStatusName`.
pub fn status_name(s: u32) -> &'static CStr {
    match s {
        0 => c"ok",
        1 => c"no Lua state",
        2 => c"no personality loaded",
        3 => c"no such module",
        4 => c"syntax error",
        5 => c"runtime error",
        6 => c"budget exceeded",
        7 => c"out of memory",
        8 => c"bad value",
        _ => c"?",
    }
}

/// A line of script output (print).
pub type WriteFn = unsafe extern "C" fn(context: *mut c_void, text: *const c_char);
/// The extension's setup (`EECPLuaConfig::mOpen`): called with the `ai` table on top; may raise a Lua error.
pub type OpenFn = unsafe extern "C" fn(state: *mut c_void, context: *mut c_void);

/// What the state is made with (`EECPLuaConfig`; the reader is the ops' `read`, with `owner`).
pub struct Config {
    pub seed: u64,
    pub memory_limit: usize,
    pub budget: i64,
    pub has_read: bool,
    pub write: Option<unsafe extern "C" fn(context: *mut c_void, text: *const c_char)>,
    pub write_context: *mut c_void,
    pub check_memory: bool,
    pub open: Option<unsafe extern "C" fn(state: *mut c_void, context: *mut c_void)>,
    pub open_context: *mut c_void,
}

/// One state. Reached through raw pointers only (the allocator's user data while Lua runs).
pub struct Host {
    owner: *mut c_void,
    ops: *const EeAiLuaTreeOps,
    config: Config,
    l: *mut lua_State,
    rng: u64,
    bytes: usize,
    peak: usize,
    used: i64,
    over_budget: bool,
    personality: c_int,
    memory: c_int,
    /// the personality's module name, NUL-terminated (empty: none)
    module: Vec<u8>,
    status: Status,
    error: Vec<u8>,
    no_file: bool,
    syntax: bool,
    alloc_failed: bool,
}

// ---- the jobs a protected call runs

#[derive(Clone, Copy, PartialEq, Eq)]
enum JobKind {
    Setup,
    Require,
    Entry,
    Run,
    Restore,
}

/// The value a job pushes: a C++ tree (through the ops) or a Rust one.
#[derive(Clone, Copy)]
enum JobValue {
    None,
    Cpp(*const c_void),
    Rust(*const Value),
}

/// A protected call's work; it lives on the caller's stack (no destructor).
struct Job {
    kind: JobKind,
    text: *const c_char,
    len: usize,
    chunk: *const c_char,
    value: JobValue,
    reset: bool,
    optional: bool,
    found: bool,
}

impl Job {
    fn new(kind: JobKind) -> Job {
        Job {
            kind,
            text: ptr::null(),
            len: 0,
            chunk: ptr::null(),
            value: JobValue::None,
            reset: false,
            optional: false,
            found: false,
        }
    }
}

const LOADED_KEY: &CStr = c"ee.ai.loaded"; // registry: module name -> its value (false while loading)

/// Safety: `l` is a state this module made.
unsafe fn host(l: *mut lua_State) -> *mut Host {
    let mut ud: *mut c_void = ptr::null_mut();
    lua_getallocf(l, &mut ud);
    ud as *mut Host
}

/// The C++ object that owns the state of `l` (`EECPLuaState::From`). Safety: as [`host`].
pub unsafe fn owner(l: *mut lua_State) -> *mut c_void {
    (*host(l)).owner
}

// ---- the tree -> Lua (inside a protected call: may raise a memory error)

unsafe fn push_key(l: *mut lua_State, k: Key<'_>) {
    match k {
        Key::Int(v) => lua_pushinteger(l, v),
        Key::Str(s) => {
            lua_pushlstring(l, s.as_ptr() as *const c_char, s.len());
        }
    }
}

unsafe fn push_value<'a, T: Tree<'a>>(l: *mut lua_State, v: T, depth: usize) {
    if depth > MAX_DEPTH {
        luaL_error(l, c"the value is nested too deep".as_ptr());
    }
    luaL_checkstack(l, 4, c"the value".as_ptr());
    match v.kind() {
        Kind::Nil => lua_pushnil(l),
        Kind::Bool => lua_pushboolean(l, v.boolean() as c_int),
        Kind::Int => lua_pushinteger(l, v.int()),
        Kind::Str => {
            let s = v.text();
            lua_pushlstring(l, s.as_ptr() as *const c_char, s.len());
        }
        Kind::Array => {
            let n = v.count();
            lua_createtable(l, n as c_int, 0);
            for i in 0..n {
                push_value(l, v.item(i), depth + 1);
                lua_rawseti(l, -2, i as lua_Integer + 1);
            }
        }
        Kind::Map => {
            let n = v.count();
            lua_createtable(l, 0, n as c_int);
            for i in 0..n {
                push_key(l, v.key(i));
                push_value(l, v.item(i), depth + 1);
                lua_rawset(l, -3);
            }
        }
    }
}

unsafe fn push_job_value(h: *mut Host, l: *mut lua_State, v: JobValue) {
    match v {
        JobValue::None => lua_pushnil(l),
        JobValue::Cpp(p) => push_value(l, lua_value::CppTree::new(&*(*h).ops, p), 0),
        JobValue::Rust(p) => push_value(l, &*p, 0),
    }
}

// ---- Lua tables -> the tree (outside a protected call: nothing here raises)

struct Converter {
    l: *mut lua_State,
    /// the tables on the current path (a cycle), compared, never ordered
    open: Vec<*const c_void>,
    path: Vec<u8>,
    error: Vec<u8>,
}

impl Converter {
    fn new(l: *mut lua_State, path: &[u8]) -> Converter {
        Converter { l, open: Vec::new(), path: path.to_vec(), error: Vec::new() }
    }

    fn fail(&mut self, what: &[u8]) -> Option<Value> {
        self.error = if self.path.is_empty() { b"the value".to_vec() } else { self.path.clone() };
        self.error.extend_from_slice(b": ");
        self.error.extend_from_slice(what);
        None
    }

    unsafe fn convert(&mut self, index: c_int) -> Option<Value> {
        let l = self.l;
        match lua_type(l, index) {
            LUA_TNIL => Some(Value::Nil),
            LUA_TBOOLEAN => Some(Value::Bool(lua_toboolean(l, index) != 0)),
            LUA_TNUMBER => {
                if lua_isinteger(l, index) == 0 {
                    return self.fail(b"a float (only integers, strings, booleans and tables)");
                }
                Some(Value::Int(lua_tointeger(l, index)))
            }
            LUA_TSTRING => {
                let mut n = 0usize;
                let s = lua_tolstring(l, index, &mut n);
                Some(Value::Str(lua_value::bytes(s as *const u8, n).to_vec()))
            }
            LUA_TTABLE => self.table(lua_absindex(l, index)),
            t => {
                let mut what = b"a ".to_vec();
                what.extend_from_slice(CStr::from_ptr(lua_typename(l, t)).to_bytes());
                what.extend_from_slice(b" (only integers, strings, booleans and tables)");
                self.fail(&what)
            }
        }
    }

    unsafe fn table(&mut self, index: c_int) -> Option<Value> {
        let l = self.l;
        let table = lua_topointer(l, index);
        if self.open.contains(&table) {
            return self.fail(b"a cycle");
        }
        if self.open.len() >= MAX_DEPTH {
            return self.fail(b"nested too deep");
        }
        if lua_getmetatable(l, index) != 0 {
            lua_pop(l, 1);
            return self.fail(b"a table with a metatable");
        }
        if lua_checkstack(l, 4) == 0 {
            return self.fail(b"no Lua stack left");
        }
        self.open.push(table);
        let mut fields: Vec<(OwnedKey, Value)> = Vec::new();
        let top = lua_gettop(l);
        let path_length = self.path.len();
        let mut ok = true;
        lua_pushnil(l);
        while ok && lua_next(l, index) != 0 {
            let key_type = lua_type(l, -2);
            let key = if key_type == LUA_TNUMBER && lua_isinteger(l, -2) != 0 {
                let k = lua_tointeger(l, -2);
                self.path.extend_from_slice(format!("[{k}]").as_bytes());
                OwnedKey::Int(k)
            } else if key_type == LUA_TSTRING {
                let mut n = 0usize;
                let s = lua_value::bytes(lua_tolstring(l, -2, &mut n) as *const u8, n);
                self.path.push(b'.');
                self.path.extend_from_slice(s);
                OwnedKey::Str(s.to_vec())
            } else {
                self.fail(if key_type == LUA_TNUMBER {
                    b"a float key"
                } else {
                    b"a key that is not an integer or a string"
                });
                ok = false;
                break;
            };
            match self.convert(lua_gettop(l)) {
                Some(v) => fields.push((key, v)),
                None => ok = false,
            }
            self.path.truncate(path_length);
            lua_pop(l, 1);
        }
        lua_settop(l, top);
        self.open.pop();
        if !ok {
            return None;
        }
        fields.sort_by(|a, b| a.0.as_key().cmp(&b.0.as_key()));
        let array =
            !fields.is_empty() && fields.iter().enumerate().all(|(i, (k, _))| *k == OwnedKey::Int(i as i64 + 1));
        Some(if array { Value::Array(fields.into_iter().map(|(_, v)| v).collect()) } else { Value::Map(fields) })
    }

    /// `convert`'s walk without the value (no copies, no path, no sort): false exactly where `convert` fails, in the
    /// same Lua calls. Run after every step under `check_memory`; a failure converts again for the message.
    unsafe fn check(&mut self, index: c_int) -> bool {
        let l = self.l;
        match lua_type(l, index) {
            LUA_TNIL | LUA_TBOOLEAN | LUA_TSTRING => true,
            LUA_TNUMBER => lua_isinteger(l, index) != 0,
            LUA_TTABLE => self.check_table(lua_absindex(l, index)),
            _ => false,
        }
    }

    unsafe fn check_table(&mut self, index: c_int) -> bool {
        let l = self.l;
        let table = lua_topointer(l, index);
        if self.open.contains(&table) || self.open.len() >= MAX_DEPTH {
            return false;
        }
        if lua_getmetatable(l, index) != 0 {
            lua_pop(l, 1);
            return false;
        }
        if lua_checkstack(l, 4) == 0 {
            return false;
        }
        self.open.push(table);
        let top = lua_gettop(l);
        let mut ok = true;
        lua_pushnil(l);
        while ok && lua_next(l, index) != 0 {
            let key_type = lua_type(l, -2);
            if !(key_type == LUA_TNUMBER && lua_isinteger(l, -2) != 0) && key_type != LUA_TSTRING {
                ok = false;
                break;
            }
            ok = self.check(lua_gettop(l));
            lua_pop(l, 1);
        }
        lua_settop(l, top);
        self.open.pop();
        ok
    }
}

// ---- the runtime's Lua functions (no value with a destructor in any of them)

/// tostring without addresses: a table, function or userdata without __tostring is its type's name.
unsafe fn deterministic_tostring(l: *mut lua_State, index: c_int) -> *const c_char {
    let index = lua_absindex(l, index);
    match lua_type(l, index) {
        LUA_TTABLE | LUA_TFUNCTION | LUA_TUSERDATA | LUA_TLIGHTUSERDATA | LUA_TTHREAD => {
            if luaL_getmetafield(l, index, c"__tostring".as_ptr()) == LUA_TNIL {
                lua_pushstring(l, luaL_typename(l, index));
                return lua_tostring(l, -1);
            }
            lua_pop(l, 1);
        }
        _ => {}
    }
    luaL_tolstring(l, index, ptr::null_mut())
}

unsafe extern "C-unwind" fn lua_tostring_fn(l: *mut lua_State) -> c_int {
    luaL_checkany(l, 1);
    deterministic_tostring(l, 1);
    1
}

unsafe extern "C-unwind" fn lua_print(l: *mut lua_State) -> c_int {
    let n = lua_gettop(l);
    let mut b = MaybeUninit::<luaL_Buffer>::uninit();
    let b = b.as_mut_ptr();
    luaL_buffinit(l, b);
    for i in 1..=n {
        if i > 1 {
            luaL_addchar(b, b'\t' as c_char);
        }
        deterministic_tostring(l, i);
        luaL_addvalue(b);
    }
    luaL_pushresult(b);
    write(host(l), lua_tostring(l, -1));
    0
}

unsafe fn write(h: *mut Host, text: *const c_char) {
    if let Some(w) = (*h).config.write {
        w((*h).config.write_context, text);
    }
}

/// string.format, refusing what prints differently per platform: %p (an address), %a (hex floats), %q of a float
/// (hex floats too) and a non-finite float in a float conversion. %s of a table and the like prints its type's name.
/// (The original's `strchr` tests also match the terminating NUL: a NUL counts as a flag and as a float conversion.)
unsafe extern "C-unwind" fn lua_format(l: *mut lua_State) -> c_int {
    const FLAGS: &[u8] = b"-+ #0123456789.\0";
    const FLOATS: &[u8] = b"eEfFgG\0";
    let mut n = 0usize;
    let f = luaL_checklstring(l, 1, &mut n) as *const u8;
    let mut argument: c_int = 1;
    let mut i = 0usize;
    while i < n {
        if *f.add(i) != b'%' {
            i += 1;
            continue;
        }
        i += 1;
        if i < n && *f.add(i) == b'%' {
            i += 1;
            continue;
        }
        while i < n && FLAGS.contains(&*f.add(i)) {
            i += 1;
        }
        if i >= n {
            break;
        }
        let c = *f.add(i);
        argument += 1;
        if c == b'p' {
            return luaL_error(l, c"string.format: %%p prints an address".as_ptr());
        }
        if c == b'a' || c == b'A' {
            return luaL_error(l, c"string.format: %%%c differs between platforms".as_ptr(), c as c_char as c_int);
        }
        if argument > lua_gettop(l) {
            i += 1;
            continue; // the original reports the missing argument
        }
        let is_float = lua_type(l, argument) == LUA_TNUMBER && lua_isinteger(l, argument) == 0;
        if c == b'q' && is_float {
            return luaL_error(l, c"string.format: %%q of a float differs between platforms".as_ptr());
        }
        if FLOATS.contains(&c) && lua_type(l, argument) == LUA_TNUMBER {
            let v = lua_tonumber(l, argument);
            #[allow(clippy::eq_op)]
            if v.is_nan() || v - v != 0.0 {
                return luaL_error(
                    l,
                    c"string.format: %%%c of nan or inf differs between platforms".as_ptr(),
                    c as c_char as c_int,
                );
            }
        }
        if c == b's' && lua_type(l, argument) != LUA_TSTRING && lua_type(l, argument) != LUA_TNUMBER {
            deterministic_tostring(l, argument);
            lua_replace(l, argument);
        }
        i += 1;
    }
    lua_pushvalue(l, lua_upvalueindex(1));
    lua_insert(l, 1);
    lua_call(l, lua_gettop(l) - 1, 1);
    1
}

/// setmetatable, refusing __gc and __mode: finalisers and weak tables run when the collector decides.
unsafe extern "C-unwind" fn lua_setmetatable_fn(l: *mut lua_State) -> c_int {
    if lua_type(l, 2) == LUA_TTABLE {
        for name in [c"__gc", c"__mode"] {
            lua_pushstring(l, name.as_ptr());
            if lua_rawget(l, 2) != LUA_TNIL {
                return luaL_error(
                    l,
                    c"setmetatable: %s is not allowed (the collector's timing is not deterministic)".as_ptr(),
                    name.as_ptr(),
                );
            }
            lua_pop(l, 1);
        }
    }
    lua_pushvalue(l, lua_upvalueindex(1));
    lua_insert(l, 1);
    lua_call(l, lua_gettop(l) - 1, 1);
    1
}

/// A key being sorted, in a Lua userdata (the C++'s `SortEntry`, the same size: the state allocates the same).
#[repr(C)]
#[derive(Clone, Copy)]
struct SortEntry {
    is_string: c_int,
    int: lua_Integer,
    string: *const c_char,
    length: usize,
    index: lua_Integer,
}

fn sort_order(a: &SortEntry, b: &SortEntry) -> std::cmp::Ordering {
    use std::cmp::Ordering;
    if a.is_string != b.is_string {
        return if a.is_string == 0 { Ordering::Less } else { Ordering::Greater };
    }
    if a.is_string == 0 {
        return a.int.cmp(&b.int);
    }
    // Safety: the keys table keeps the strings alive and in place
    let (x, y) = unsafe {
        (lua_value::bytes(a.string as *const u8, a.length), lua_value::bytes(b.string as *const u8, b.length))
    };
    x.cmp(y)
}

/// Pushes an array of t's keys in the total order: integers ascending, then strings by bytes.
unsafe fn push_sorted_keys(l: *mut lua_State, table: c_int) {
    let table = lua_absindex(l, table);
    luaL_checktype(l, table, LUA_TTABLE);
    lua_newtable(l); // the keys as found
    let keys = lua_gettop(l);
    let mut n: lua_Integer = 0;
    lua_pushnil(l);
    while lua_next(l, table) != 0 {
        lua_pop(l, 1);
        let t = lua_type(l, -1);
        if t == LUA_TNUMBER && lua_isinteger(l, -1) == 0 {
            luaL_error(l, c"each: a float key (keys are integers or strings)".as_ptr());
        }
        if t != LUA_TNUMBER && t != LUA_TSTRING {
            luaL_error(l, c"each: a %s key (keys are integers or strings)".as_ptr(), luaL_typename(l, -1));
        }
        lua_pushvalue(l, -1);
        n += 1;
        lua_rawseti(l, keys, n);
    }
    let count = n as usize;
    let e = lua_newuserdatauv(l, count * std::mem::size_of::<SortEntry>(), 0) as *mut SortEntry;
    for i in 0..count {
        lua_rawgeti(l, keys, i as lua_Integer + 1);
        let is_string = lua_type(l, -1) == LUA_TSTRING;
        let mut length = 0usize;
        let string = if is_string { lua_tolstring(l, -1, &mut length) } else { ptr::null() };
        e.add(i).write(SortEntry {
            is_string: is_string as c_int,
            int: if is_string { 0 } else { lua_tointeger(l, -1) },
            string,
            length,
            index: i as lua_Integer + 1,
        });
        lua_pop(l, 1);
    }
    if count > 0 {
        std::slice::from_raw_parts_mut(e, count).sort_unstable_by(sort_order);
    }
    lua_createtable(l, n as c_int, 0);
    for i in 0..count {
        lua_rawgeti(l, keys, (*e.add(i)).index);
        lua_rawseti(l, -2, i as lua_Integer + 1);
    }
    lua_replace(l, keys); // the sorted array in the keys' place
    lua_settop(l, keys);
}

unsafe extern "C-unwind" fn lua_each_next(l: *mut lua_State) -> c_int {
    let mut i = lua_tointeger(l, lua_upvalueindex(3));
    loop {
        i += 1;
        if lua_rawgeti(l, lua_upvalueindex(2), i) == LUA_TNIL {
            return 0;
        }
        lua_pushvalue(l, -1);
        if lua_rawget(l, lua_upvalueindex(1)) != LUA_TNIL {
            // a key removed meanwhile is skipped
            lua_pushinteger(l, i);
            lua_replace(l, lua_upvalueindex(3));
            return 2;
        }
        lua_pop(l, 2);
    }
}

/// for k, v in each(t): t's keys in the total order (the keys are taken when the loop starts).
unsafe extern "C-unwind" fn lua_each(l: *mut lua_State) -> c_int {
    luaL_checktype(l, 1, LUA_TTABLE);
    lua_settop(l, 1);
    push_sorted_keys(l, 1);
    lua_pushinteger(l, 0);
    lua_pushcclosure(l, lua_each_next, 3);
    1
}

unsafe extern "C-unwind" fn lua_keys(l: *mut lua_State) -> c_int {
    luaL_checktype(l, 1, LUA_TTABLE);
    lua_settop(l, 1);
    push_sorted_keys(l, 1);
    1
}

unsafe fn next_random(h: *mut Host) -> u64 {
    lua_value::splitmix(&mut (*h).rng)
}

/// ai.random(n): 1..n; ai.random(lo, hi): lo..hi (uniform: rejection sampling).
unsafe extern "C-unwind" fn lua_random(l: *mut lua_State) -> c_int {
    let (lo, hi) = if lua_gettop(l) >= 2 {
        let lo = luaL_checkinteger(l, 1);
        (lo, luaL_checkinteger(l, 2))
    } else {
        (1, luaL_checkinteger(l, 1))
    };
    if lo > hi {
        luaL_argerror(l, if lua_gettop(l) >= 2 { 2 } else { 1 }, c"interval is empty".as_ptr());
    }
    let h = host(l);
    let range = (hi as u64).wrapping_sub(lo as u64).wrapping_add(1); // 0: the whole 64-bit range
    let mut x = next_random(h);
    if range != 0 {
        let threshold = 0u64.wrapping_sub(range) % range; // 2^64 mod range: the draws below it are biased
        while x < threshold {
            x = next_random(h);
        }
        x %= range;
    }
    lua_pushinteger(l, (lo as u64).wrapping_add(x) as lua_Integer);
    1
}

/// A module name: letters, digits and _ in parts joined by single dots, at most 96 bytes.
pub fn module_name(name: &[u8]) -> bool {
    if name.is_empty() || name.len() > 96 {
        return false;
    }
    let mut start = true;
    for &c in name {
        if c == b'.' {
            if start {
                return false;
            }
            start = true;
        } else if c.is_ascii_alphanumeric() || c == b'_' {
            start = false;
        } else {
            return false;
        }
    }
    !start
}

/// `ai\<name with dots as folders>.lua` and the chunk name `@ai/<name with dots as folders>.lua`, NUL-terminated
/// (a checked name: at most 96 bytes).
pub fn module_paths(name: &[u8]) -> ([u8; 128], [u8; 128]) {
    let mut path = [0u8; 128];
    let mut chunk = [0u8; 128];
    let mut p = 0;
    let mut c = 0;
    for &b in b"ai\\" {
        path[p] = b;
        p += 1;
    }
    for &b in b"@ai/" {
        chunk[c] = b;
        c += 1;
    }
    for &b in name {
        path[p] = if b == b'.' { b'\\' } else { b };
        chunk[c] = if b == b'.' { b'/' } else { b };
        p += 1;
        c += 1;
    }
    for &b in b".lua" {
        path[p] = b;
        chunk[c] = b;
        p += 1;
        c += 1;
    }
    (path, chunk)
}

/// Reads a module through the owner's reader and compiles it (text only) -> the lua_load status, -1 when there is
/// no such file. Compiling does not raise (a protected parse), so the text is released before anything can.
unsafe fn load_module(l: *mut lua_State, path: *const c_char, chunk: *const c_char) -> c_int {
    let h = host(l);
    let ops = &*(*h).ops;
    let mut text: *const u8 = ptr::null();
    let mut len = 0usize;
    if !(*h).config.has_read || !(ops.read)((*h).owner, path, &mut text, &mut len) {
        (ops.read_done)((*h).owner);
        return -1;
    }
    let status = luaL_loadbufferx(l, text as *const c_char, len, chunk, c"t".as_ptr());
    (ops.read_done)((*h).owner);
    status
}

/// require(name): Data/ai/<name with dots as folders>.lua, loaded once per state.
unsafe extern "C-unwind" fn lua_require(l: *mut lua_State) -> c_int {
    let name = luaL_checklstring(l, 1, ptr::null_mut());
    lua_settop(l, 1);
    if !module_name(CStr::from_ptr(name).to_bytes()) {
        (*host(l)).no_file = true;
        return luaL_error(
            l,
            c"require: '%s' is not a module name (letters, digits, _ and dots: Data/ai/a/b.lua is 'a.b')".as_ptr(),
            name,
        );
    }
    lua_getfield(l, LUA_REGISTRYINDEX, LOADED_KEY.as_ptr()); // 2
    lua_getfield(l, 2, name);
    if lua_toboolean(l, -1) != 0 {
        return 1;
    }
    if lua_type(l, -1) == LUA_TBOOLEAN {
        return luaL_error(l, c"require: '%s' requires itself".as_ptr(), name);
    }
    lua_pop(l, 1);
    let (path, chunk) = module_paths(CStr::from_ptr(name).to_bytes());
    let status = load_module(l, path.as_ptr() as *const c_char, chunk.as_ptr() as *const c_char);
    if status == -1 {
        (*host(l)).no_file = true;
        return luaL_error(l, c"require: no module '%s' (Data/%s)".as_ptr(), name, chunk.as_ptr().add(1));
    }
    if status != LUA_OK {
        (*host(l)).syntax = status == LUA_ERRSYNTAX;
        lua_error(l);
    }
    lua_pushboolean(l, 0); // loading
    lua_setfield(l, 2, name);
    lua_pushvalue(l, 1);
    if lua_pcall(l, 1, 1, 0) != LUA_OK {
        lua_pushnil(l);
        lua_setfield(l, 2, name);
        lua_error(l);
    }
    if lua_isnil(l, -1) {
        lua_pop(l, 1);
        lua_pushboolean(l, 1);
    }
    lua_pushvalue(l, -1);
    lua_setfield(l, 2, name);
    1
}

unsafe extern "C-unwind" fn lua_hook(l: *mut lua_State, _ar: *mut lua_Debug) {
    let h = host(l);
    (*h).used += HOOK_STEP;
    over_budget(h, l);
}

/// Over the budget: raises, and keeps raising at every instruction, so that a script's pcall cannot swallow it.
unsafe fn over_budget(h: *mut Host, l: *mut lua_State) {
    if (*h).over_budget || (*h).used > (*h).config.budget {
        if !(*h).over_budget {
            (*h).over_budget = true;
            lua_sethook(l, Some(lua_hook), LUA_MASKCOUNT, 1);
        }
        luaL_error(l, c"instruction budget exceeded (%I instructions)".as_ptr(), (*h).config.budget as lua_Integer);
    }
}

/// `ai.used()`: pushes the call's count so far and its budget -> 2. Safety: `l` is a state of this module, in a call.
pub unsafe fn used_binding(l: *mut lua_State) -> c_int {
    let h = host(l);
    lua_pushinteger(l, (*h).used);
    lua_pushinteger(l, (*h).config.budget);
    2
}

/// A C function's cost in instruction equivalents (`EECPLuaState::Charge`): added to the call's count, which the
/// budget bounds; over the budget it raises as the hook does. Safety: `l` is a state of this module, in a call.
pub unsafe fn charge(l: *mut lua_State, amount: i64) {
    let h = host(l);
    if amount > 0 {
        (*h).used += amount;
    }
    over_budget(h, l);
}

unsafe extern "C" fn lua_allocate(ud: *mut c_void, block: *mut c_void, old: usize, new: usize) -> *mut c_void {
    let h = ud as *mut Host;
    let old = if block.is_null() { 0 } else { old }; // then it is the object's type
    if new == 0 {
        libc::free(block);
        (*h).bytes = (*h).bytes.wrapping_sub(old);
        return ptr::null_mut();
    }
    if new > old && (*h).bytes.wrapping_sub(old).wrapping_add(new) > (*h).config.memory_limit {
        (*h).alloc_failed = true;
        return ptr::null_mut(); // Lua collects and tries once more, then raises "not enough memory"
    }
    let b = libc::realloc(block, new);
    if b.is_null() {
        (*h).alloc_failed = true;
        return ptr::null_mut();
    }
    (*h).bytes = (*h).bytes.wrapping_sub(old).wrapping_add(new);
    if (*h).bytes > (*h).peak {
        (*h).peak = (*h).bytes;
    }
    b
}

unsafe extern "C-unwind" fn lua_panic(l: *mut lua_State) -> c_int {
    let m = lua_tostring(l, -1);
    let text = if m.is_null() { "?".into() } else { CStr::from_ptr(m).to_string_lossy() };
    eprintln!("EECPLua: unprotected Lua error: {text}");
    std::process::abort();
}

/// The message handler: the error as text (no addresses) with a traceback.
unsafe extern "C-unwind" fn lua_message(l: *mut lua_State) -> c_int {
    let m = if lua_type(l, 1) == LUA_TSTRING { lua_tostring(l, 1) } else { deterministic_tostring(l, 1) };
    luaL_traceback(l, l, m, 1);
    1
}

// ---- the protected calls

const BASE_REMOVED: [&CStr; 5] = [c"dofile", c"loadfile", c"load", c"collectgarbage", c"warn"];
const STRING_REMOVED: [&CStr; 4] = [c"dump", c"pack", c"packsize", c"unpack"];
const MATH_REMOVED: [&CStr; 10] =
    [c"random", c"randomseed", c"sin", c"cos", c"tan", c"asin", c"acos", c"atan", c"exp", c"log"];

unsafe fn setup(h: *mut Host, l: *mut lua_State) {
    luaL_requiref(l, c"_G".as_ptr(), luaopen_base, 1);
    luaL_requiref(l, c"table".as_ptr(), luaopen_table, 1);
    luaL_requiref(l, c"string".as_ptr(), luaopen_string, 1);
    luaL_requiref(l, c"math".as_ptr(), luaopen_math, 1);
    luaL_requiref(l, c"utf8".as_ptr(), luaopen_utf8, 1);
    lua_settop(l, 0);
    lua_pushglobaltable(l); // 1
    for n in BASE_REMOVED {
        lua_pushnil(l);
        lua_setfield(l, 1, n.as_ptr());
    }
    lua_pushcfunction(l, lua_print);
    lua_setfield(l, 1, c"print".as_ptr());
    lua_pushcfunction(l, lua_tostring_fn);
    lua_setfield(l, 1, c"tostring".as_ptr());
    lua_getfield(l, 1, c"setmetatable".as_ptr());
    lua_pushcclosure(l, lua_setmetatable_fn, 1);
    lua_setfield(l, 1, c"setmetatable".as_ptr());
    lua_pushcfunction(l, lua_require);
    lua_setfield(l, 1, c"require".as_ptr());
    lua_pushcfunction(l, lua_each);
    lua_setfield(l, 1, c"each".as_ptr());
    lua_newtable(l);
    lua_pushcfunction(l, lua_random);
    lua_setfield(l, -2, c"random".as_ptr());
    lua_pushcfunction(l, lua_keys);
    lua_setfield(l, -2, c"keys".as_ptr());
    if let Some(open) = (*h).config.open {
        open(l as *mut c_void, (*h).config.open_context);
    }
    lua_setfield(l, 1, c"ai".as_ptr());
    lua_getfield(l, 1, c"string".as_ptr()); // 2 (also the strings' __index)
    for n in STRING_REMOVED {
        lua_pushnil(l);
        lua_setfield(l, 2, n.as_ptr());
    }
    lua_getfield(l, 2, c"format".as_ptr());
    lua_pushcclosure(l, lua_format, 1);
    lua_setfield(l, 2, c"format".as_ptr());
    lua_getfield(l, 1, c"math".as_ptr()); // 3
    for n in MATH_REMOVED {
        lua_pushnil(l);
        lua_setfield(l, 3, n.as_ptr());
    }
    lua_settop(l, 0);
    lua_newtable(l);
    lua_setfield(l, LUA_REGISTRYINDEX, LOADED_KEY.as_ptr());
    lua_newtable(l);
    (*h).memory = luaL_ref(l, LUA_REGISTRYINDEX);
}

unsafe extern "C-unwind" fn run_job(l: *mut lua_State) -> c_int {
    let j = lua_touserdata(l, 1) as *mut Job;
    let h = host(l);
    lua_settop(l, 0);
    match (*j).kind {
        JobKind::Setup => {
            setup(h, l);
            0
        }
        JobKind::Require => {
            if (*j).reset {
                lua_newtable(l);
                lua_setfield(l, LUA_REGISTRYINDEX, LOADED_KEY.as_ptr());
            }
            lua_pushcfunction(l, lua_require);
            lua_pushstring(l, (*j).text);
            lua_call(l, 1, 1);
            if !lua_istable(l, 1) {
                return luaL_error(
                    l,
                    c"personality '%s': the script returns a %s, not a table".as_ptr(),
                    (*j).text,
                    luaL_typename(l, 1),
                );
            }
            if lua_getfield(l, 1, c"step".as_ptr()) != LUA_TFUNCTION {
                return luaL_error(l, c"personality '%s': no step function".as_ptr(), (*j).text);
            }
            lua_pop(l, 1);
            luaL_unref(l, LUA_REGISTRYINDEX, (*h).personality);
            (*h).personality = luaL_ref(l, LUA_REGISTRYINDEX);
            0
        }
        JobKind::Entry => {
            lua_rawgeti(l, LUA_REGISTRYINDEX, (*h).personality as lua_Integer);
            if lua_getfield(l, 1, (*j).text) != LUA_TFUNCTION {
                if (*j).optional {
                    (*j).found = false;
                    return 0;
                }
                return luaL_error(l, c"the personality has no %s function".as_ptr(), (*j).text);
            }
            (*j).found = true;
            push_job_value(h, l, (*j).value);
            lua_rawgeti(l, LUA_REGISTRYINDEX, (*h).memory as lua_Integer);
            lua_call(l, 2, 1);
            1
        }
        JobKind::Run => {
            let status = luaL_loadbufferx(l, (*j).text, (*j).len, (*j).chunk, c"t".as_ptr());
            if status != LUA_OK {
                (*h).syntax = status == LUA_ERRSYNTAX;
                lua_error(l);
            }
            lua_call(l, 0, 1);
            1
        }
        JobKind::Restore => {
            push_job_value(h, l, (*j).value);
            luaL_unref(l, LUA_REGISTRYINDEX, (*h).memory);
            (*h).memory = luaL_ref(l, LUA_REGISTRYINDEX);
            0
        }
    }
}

unsafe fn fail(h: *mut Host, status: Status, error: Vec<u8>) -> bool {
    (*h).status = status;
    (*h).error = error;
    false
}

fn joined(a: &[u8], b: &[u8]) -> Vec<u8> {
    let mut v = a.to_vec();
    v.extend_from_slice(b": ");
    v.extend_from_slice(b);
    v
}

fn contains(hay: &[u8], needle: &[u8]) -> bool {
    hay.windows(needle.len()).any(|w| w == needle)
}

/// Runs a job in lua_pcall with the budget; with `result`, converts the job's result into it.
unsafe fn call(h: *mut Host, job: *mut Job, result: Option<&mut Value>, what: &[u8]) -> bool {
    let l = (*h).l;
    (*h).status = Status::Ok;
    (*h).error.clear();
    let base = lua_gettop(l);
    lua_pushcfunction(l, lua_message);
    lua_pushcfunction(l, run_job);
    lua_pushlightuserdata(l, job as *mut c_void);
    (*h).used = 0;
    (*h).over_budget = false;
    (*h).no_file = false;
    (*h).syntax = false;
    (*h).alloc_failed = false;
    lua_sethook(l, Some(lua_hook), LUA_MASKCOUNT, HOOK_STEP as c_int);
    let status = lua_pcall(l, 1, 1, base + 1);
    lua_sethook(l, None, 0, 0);
    let mut ok = true;
    if status != LUA_OK {
        let m = lua_tostring(l, -1);
        let message = joined(what, if m.is_null() { b"error" } else { CStr::from_ptr(m).to_bytes() });
        // a memory error re-raised by require arrives as a runtime error with Lua's memory message
        let memory = status == LUA_ERRMEM || ((*h).alloc_failed && contains(&message, b"not enough memory"));
        let s = if (*h).over_budget {
            Status::Budget
        } else if memory {
            Status::Memory
        } else if (*h).syntax {
            Status::Syntax
        } else if (*h).no_file {
            Status::NoFile
        } else {
            Status::Runtime
        };
        ok = fail(h, s, message);
    } else if let Some(out) = result {
        let mut c = Converter::new(l, b"result");
        match c.convert(-1) {
            Some(v) => *out = v,
            None => ok = fail(h, Status::BadValue, joined(what, &c.error)),
        }
    }
    lua_settop(l, base);
    ok
}

// ---- the state's entry points (`EECPLuaState`'s methods)

/// Makes a state (`EECPLuaState::EECPLuaState`): check [`is_open`]. Safety: `ops` outlives the host; `owner` is
/// what [`owner`] answers.
pub unsafe fn new(config: Config, ops: *const EeAiLuaTreeOps, owner: *mut c_void) -> *mut Host {
    let rng = config.seed;
    let h = Box::into_raw(Box::new(Host {
        owner,
        ops,
        config,
        l: ptr::null_mut(),
        rng,
        bytes: 0,
        peak: 0,
        used: 0,
        over_budget: false,
        personality: LUA_NOREF,
        memory: LUA_NOREF,
        module: Vec::new(),
        status: Status::Ok,
        error: Vec::new(),
        no_file: false,
        syntax: false,
        alloc_failed: false,
    }));
    (*h).l = lua_newstate(lua_allocate, h as *mut c_void);
    if (*h).l.is_null() {
        fail(h, Status::NoState, b"lua_newstate failed (memory limit too small?)".to_vec());
        return h;
    }
    lua_atpanic((*h).l, lua_panic);
    let mut j = Job::new(JobKind::Setup);
    if !call(h, &mut j, None, b"setup") {
        let e = std::mem::take(&mut (*h).error);
        lua_close((*h).l);
        (*h).l = ptr::null_mut();
        fail(h, Status::NoState, e);
    }
    h
}

/// Closes and frees a state. Safety: `h` came from [`new`] and is not used again.
pub unsafe fn free(h: *mut Host) {
    if !(*h).l.is_null() {
        lua_close((*h).l);
    }
    drop(Box::from_raw(h));
}

pub unsafe fn is_open(h: *const Host) -> bool {
    !(*h).l.is_null()
}
pub unsafe fn status(h: *const Host) -> Status {
    (*h).status
}
pub unsafe fn error<'a>(h: *const Host) -> &'a [u8] {
    &(*h).error
}
pub unsafe fn used(h: *const Host) -> i64 {
    (*h).used
}
pub unsafe fn budget(h: *const Host) -> i64 {
    (*h).config.budget
}
pub unsafe fn memory_used(h: *const Host) -> usize {
    (*h).bytes
}
pub unsafe fn memory_peak(h: *const Host) -> usize {
    (*h).peak
}
pub unsafe fn rng_state(h: *const Host) -> u64 {
    (*h).rng
}
pub unsafe fn random(h: *mut Host) -> u64 {
    next_random(h)
}
pub unsafe fn state(h: *const Host) -> *mut lua_State {
    (*h).l
}
pub unsafe fn ops<'a>(h: *const Host) -> &'a EeAiLuaTreeOps {
    &*(*h).ops
}

unsafe fn no_state(h: *mut Host) -> bool {
    fail(h, Status::NoState, b"no Lua state".to_vec())
}

/// `LoadPersonality` (`reload` false) / `Reload` (true): require(module), which must return a table with a step
/// function. Safety: `h` is live; `module` a C string (ignored by a reload).
pub unsafe fn load(h: *mut Host, module: *const c_char, reload: bool) -> bool {
    if (*h).l.is_null() {
        return no_state(h);
    }
    if reload {
        if (*h).module.is_empty() {
            return fail(h, Status::NotLoaded, b"no personality loaded".to_vec());
        }
    } else {
        (*h).module = CStr::from_ptr(module).to_bytes_with_nul().to_vec();
    }
    let text = (*h).module.clone();
    let mut j = Job::new(JobKind::Require);
    j.text = text.as_ptr() as *const c_char;
    j.reset = reload;
    call(h, &mut j, None, &text[..text.len() - 1])
}

fn module_bytes(m: &[u8]) -> &[u8] {
    if m.is_empty() {
        m
    } else {
        &m[..m.len() - 1]
    }
}

/// `Init`/`Step`/`CallOptional`: personality[name](view, memory) with `budget` (0: the config's) -> its result
/// converted into `out` (nil when there is no such function: `found` false, an error unless `optional`).
/// Safety: `h` is live, `name` a C string, the view's tree live during the call.
pub unsafe fn entry(
    h: *mut Host,
    name: *const c_char,
    optional: bool,
    view: JobValueRef,
    budget: i64,
    out: &mut Value,
    found: &mut bool,
) -> bool {
    *out = Value::Nil;
    *found = false;
    if (*h).l.is_null() {
        return no_state(h);
    }
    if (*h).personality == LUA_NOREF || (*h).personality == LUA_REFNIL {
        return fail(h, Status::NotLoaded, b"no personality loaded".to_vec());
    }
    let mut j = Job::new(JobKind::Entry);
    j.text = name;
    j.optional = optional;
    j.value = view.0;
    let mut what = module_bytes(&(*h).module).to_vec();
    what.push(b'.');
    what.extend_from_slice(CStr::from_ptr(name).to_bytes());
    let saved = (*h).config.budget;
    if budget > 0 {
        (*h).config.budget = budget;
    }
    let ok = call(h, &mut j, Some(out), &what);
    (*h).config.budget = saved;
    if !ok {
        return false;
    }
    *found = j.found;
    if !j.found {
        *out = Value::Nil;
    }
    !(*h).config.check_memory || check_memory(h)
}

/// A view for [`entry`]: a C++ tree or a Rust one.
#[derive(Clone, Copy)]
pub struct JobValueRef(JobValue);

impl JobValueRef {
    /// Safety: `p` is a live `EECPLuaValue` for the call.
    pub unsafe fn cpp(p: *const c_void) -> JobValueRef {
        JobValueRef(JobValue::Cpp(p))
    }
    pub fn rust(v: &Value) -> JobValueRef {
        JobValueRef(JobValue::Rust(v))
    }
}

/// `Run`: a chunk of source text (mode "t") with the budget -> its first result. Safety: `h` live, `source` holds
/// `len` bytes, `name` a C string.
pub unsafe fn run(h: *mut Host, source: *const c_char, len: usize, name: *const c_char, out: &mut Value) -> bool {
    *out = Value::Nil;
    if (*h).l.is_null() {
        return no_state(h);
    }
    let mut chunk = b"=".to_vec();
    chunk.extend_from_slice(CStr::from_ptr(name).to_bytes_with_nul());
    let mut j = Job::new(JobKind::Run);
    j.text = source;
    j.len = len;
    j.chunk = chunk.as_ptr() as *const c_char;
    call(h, &mut j, Some(out), CStr::from_ptr(name).to_bytes())
}

/// `GetMemory`: the memory table converted. Safety: `h` live.
pub unsafe fn get_memory(h: *mut Host) -> Option<Value> {
    if (*h).l.is_null() {
        no_state(h);
        return None;
    }
    (*h).status = Status::Ok;
    (*h).error.clear();
    let l = (*h).l;
    let base = lua_gettop(l);
    lua_rawgeti(l, LUA_REGISTRYINDEX, (*h).memory as lua_Integer);
    let mut c = Converter::new(l, b"memory");
    let v = c.convert(-1);
    lua_settop(l, base);
    if v.is_none() {
        fail(h, Status::BadValue, c.error);
    }
    v
}

/// After a step under `check_memory`: `get_memory`'s answer and status without building the value.
unsafe fn check_memory(h: *mut Host) -> bool {
    if !(*h).l.is_null() {
        let l = (*h).l;
        let base = lua_gettop(l);
        lua_rawgeti(l, LUA_REGISTRYINDEX, (*h).memory as lua_Integer);
        let mut c = Converter::new(l, b"");
        let ok = c.check(-1);
        lua_settop(l, base);
        if ok {
            (*h).status = Status::Ok;
            (*h).error.clear();
            return true;
        }
    }
    get_memory(h).is_some()
}

const MEMORY_MAGIC: &[u8; 4] = b"EELM";
const MEMORY_VERSION: u8 = 1;

/// `SaveMemory`: "EELM", version 1, the RNG state as u64 LE, the memory table's bytes. Safety: `h` live.
pub unsafe fn save_memory(h: *mut Host, out: &mut Vec<u8>) -> bool {
    out.clear();
    let Some(memory) = get_memory(h) else { return false };
    out.extend_from_slice(MEMORY_MAGIC);
    out.push(MEMORY_VERSION);
    out.extend_from_slice(&(*h).rng.to_le_bytes());
    lua_value::encode(&memory, out);
    true
}

/// `RestoreMemory`: the memory and the RNG state back from [`save_memory`]'s bytes (a loaded game: after the
/// personality's load, instead of its init). Safety: `h` live.
pub unsafe fn restore_memory(h: *mut Host, b: &[u8]) -> bool {
    if (*h).l.is_null() {
        return no_state(h);
    }
    let mut at = 5;
    let rng = if b.len() < 5 || &b[..4] != MEMORY_MAGIC || b[4] != MEMORY_VERSION {
        None
    } else {
        lua_value::get_u64(b, &mut at)
    };
    let Some(rng) = rng else {
        return fail(h, Status::BadValue, b"memory: not an EELM version 1 block".to_vec());
    };
    let memory = match lua_value::decode(b, &mut at) {
        Ok(v) => v,
        Err(e) => return fail(h, Status::BadValue, joined(b"memory", e.as_bytes())),
    };
    if at != b.len() {
        return fail(h, Status::BadValue, b"memory: bytes after the table".to_vec());
    }
    if !matches!(memory, Value::Map(_) | Value::Array(_)) {
        return fail(h, Status::BadValue, b"memory: not a table".to_vec());
    }
    let mut j = Job::new(JobKind::Restore);
    j.value = JobValue::Rust(&memory);
    if !call(h, &mut j, None, b"restore memory") {
        return false;
    }
    (*h).rng = rng;
    true
}

// The vendored Lua for the tests (build.rs compiles it; the game links the C++ build's)
#[cfg(test)]
#[link(name = "ee_ai_lua_test", kind = "static")]
extern "C" {}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::golden_lua::{gen_value, set, text, Gen, GOLDEN};
    use crate::test_ops::{Reader, OPS};
    use std::cell::RefCell;

    thread_local! {
        static PRINTED: RefCell<Vec<Vec<u8>>> = const { RefCell::new(Vec::new()) };
    }

    unsafe extern "C" fn capture(_: *mut c_void, text: *const c_char) {
        PRINTED.with(|p| p.borrow_mut().push(CStr::from_ptr(text).to_bytes().to_vec()));
    }

    unsafe extern "C-unwind" fn lua_cost(l: *mut lua_State) -> c_int {
        charge(l, luaL_checkinteger(l, 1));
        lua_pushinteger(l, 7);
        1
    }

    // ai.used as the bindings register it (EECPNextUnits.cpp LuaUsed)
    unsafe extern "C-unwind" fn lua_used_now(l: *mut lua_State) -> c_int {
        used_binding(l)
    }

    unsafe extern "C" fn open_test(l: *mut c_void, _: *mut c_void) {
        let l = l as *mut lua_State;
        lua_pushcfunction(l, lua_cost);
        lua_setfield(l, -2, c"cost".as_ptr());
        lua_pushcfunction(l, lua_used_now);
        lua_setfield(l, -2, c"used".as_ptr());
    }

    fn s(t: &str) -> Value {
        Value::Str(t.as_bytes().to_vec())
    }

    fn view(mode: &str) -> Value {
        let mut v = Vec::new();
        set(&mut v, OwnedKey::Str(b"mode".to_vec()), s(mode));
        let key = |t: &str| OwnedKey::Str(t.as_bytes().to_vec());
        match mode {
            "echo" => {
                let mut p = Vec::new();
                set(&mut p, key("name"), Value::Str(b"a\0b".to_vec()));
                set(&mut p, key("big"), Value::Int(i64::MIN));
                set(&mut p, key("flag"), Value::Bool(false));
                let mut l: Vec<Value> = (0..5).map(|i| Value::Int(i * 10)).collect();
                l.push(Value::Nil);
                l.push(Value::Int(7));
                set(&mut p, key("list"), Value::Array(l));
                let mut u = Vec::new();
                set(&mut u, OwnedKey::Int(1007), s("clubman"));
                set(&mut u, OwnedKey::Int(-3), s("tower"));
                set(&mut u, key("nil"), Value::Nil);
                set(&mut p, key("units"), Value::Map(u));
                set(&mut p, key("empty"), Value::Map(Vec::new()));
                set(&mut p, key("emptya"), Value::Array(Vec::new()));
                set(&mut v, key("payload"), Value::Map(p));
            }
            "keys" => {
                let mut p = Vec::new();
                set(&mut p, key("zeta"), Value::Int(1));
                set(&mut p, OwnedKey::Int(40), Value::Int(1));
                set(&mut p, key("alpha"), Value::Int(1));
                set(&mut v, key("payload"), Value::Map(p));
            }
            "big" => {
                let mut g = Gen(77);
                set(&mut v, key("payload"), Value::Array((0..300).map(|_| gen_value(&mut g, 0)).collect()));
            }
            _ => {}
        }
        Value::Map(v)
    }

    fn c_text(b: &[u8]) -> String {
        // as the driver's printf("%s"): up to the first NUL
        let end = b.iter().position(|&c| c == 0).unwrap_or(b.len());
        String::from_utf8_lossy(&b[..end]).into_owned()
    }

    fn report(out: &mut Vec<String>, h: *mut Host, what: &str, ok: bool, result: Option<&Value>) {
        unsafe {
            let mut line = format!(
                "{what} -> {} status={} instr={} mem={} peak={} rng={}",
                if ok { "ok" } else { "FAIL" },
                status_name(status(h) as u32).to_str().unwrap(),
                used(h),
                memory_used(h),
                memory_peak(h),
                rng_state(h)
            );
            if let Some(r) = result {
                line += &format!(" result={}", text(r));
            }
            out.extend(line.split('\n').map(str::to_string));
            if !error(h).is_empty() {
                out.extend(format!("  error: {}", c_text(error(h))).split('\n').map(str::to_string));
            }
        }
        PRINTED.with(|p| {
            for t in p.borrow_mut().drain(..) {
                out.extend(format!("  print: {}", c_text(&t)).split('\n').map(str::to_string));
            }
        });
    }

    fn hex(b: &[u8]) -> String {
        b.iter().map(|c| format!("{c:02x}")).collect()
    }

    /// golden_lua.txt's part B: the driver's commands (chunks.txt) on the Rust state.
    fn part_b(chunks: &str, reader: &mut Reader) -> Vec<String> {
        let mut out = vec!["== part B".to_string()];
        let lines: Vec<&str> = chunks.lines().collect();
        let mut h: *mut Host = ptr::null_mut();
        let mut saved: Vec<u8> = Vec::new();
        let mut i = 0;
        let folder = reader.folder.clone();
        unsafe {
            while i < lines.len() {
                let l = lines[i];
                i += 1;
                if l.is_empty() || l.starts_with('#') {
                    continue;
                }
                let words: Vec<&str> = l.split_whitespace().collect();
                let num = |k: usize| words.get(k).map(|w| w.parse::<i64>().unwrap()).unwrap_or(0);
                match words[0] {
                    "new" => {
                        if !h.is_null() {
                            free(h);
                        }
                        let flags = num(5);
                        reader.folder = if flags & 2 != 0 { None } else { folder.clone() };
                        let config = Config {
                            seed: num(1) as u64,
                            budget: num(2),
                            memory_limit: num(3) as usize,
                            check_memory: num(4) != 0,
                            has_read: flags & 2 == 0,
                            write: if flags & 4 != 0 { None } else { Some(capture) },
                            write_context: ptr::null_mut(),
                            open: if flags & 1 != 0 { Some(open_test) } else { None },
                            open_context: ptr::null_mut(),
                        };
                        h = new(config, &OPS, reader as *mut Reader as *mut c_void);
                        out.push(format!("new {} open={}", &l[4..], is_open(h) as i32));
                        report(&mut out, h, "new", is_open(h), None);
                    }
                    "run" => {
                        let name = std::ffi::CString::new(words[1]).unwrap();
                        let mut src = String::new();
                        while lines[i] != "end" {
                            src += lines[i];
                            src += "\n";
                            i += 1;
                        }
                        i += 1;
                        let mut r = Value::Nil;
                        // EECPLuaState::Run takes a C string (strlen)
                        let len = src.bytes().position(|c| c == 0).unwrap_or(src.len());
                        let ok = run(h, src.as_ptr() as *const c_char, len, name.as_ptr(), &mut r);
                        report(&mut out, h, &format!("run {}", words[1]), ok, Some(&r));
                    }
                    "load" => {
                        let m = std::ffi::CString::new(words[1]).unwrap();
                        let ok = load(h, m.as_ptr(), false);
                        report(&mut out, h, l, ok, None);
                    }
                    "reload" => {
                        let ok = load(h, ptr::null(), true);
                        report(&mut out, h, l, ok, None);
                    }
                    "init" | "step" => {
                        let v = view(words[1]);
                        let mut o = Value::Nil;
                        let mut found = false;
                        let name = if words[0] == "init" { c"init" } else { c"step" };
                        let ok =
                            entry(h, name.as_ptr(), words[0] == "init", JobValueRef::rust(&v), 0, &mut o, &mut found);
                        report(&mut out, h, l, ok, Some(&o));
                    }
                    "opt" => {
                        let v = view(words[2]);
                        let name = std::ffi::CString::new(words[1]).unwrap();
                        let mut o = Value::Nil;
                        let mut found = true;
                        let ok = entry(h, name.as_ptr(), true, JobValueRef::rust(&v), num(3), &mut o, &mut found);
                        report(
                            &mut out,
                            h,
                            &format!("{l}{}", if found { " found" } else { " notfound" }),
                            ok,
                            Some(&o),
                        );
                    }
                    "save" => {
                        let mut b = Vec::new();
                        let ok = save_memory(h, &mut b);
                        if !ok {
                            b.clear();
                        }
                        report(&mut out, h, l, ok, None);
                        out.push(format!("  bytes {} {}", b.len(), hex(&b)));
                        if ok {
                            saved = b;
                        }
                    }
                    "restore" => {
                        let (k, n) = (num(1), num(2) as usize);
                        let mut b = saved.clone();
                        if k == 1 {
                            b.truncate(n);
                        } else if k == 2 {
                            b.push(b'x');
                        } else if k == 3 && n < b.len() {
                            b[n] ^= 0x41;
                        }
                        let ok = restore_memory(h, &b);
                        report(&mut out, h, l, ok, None);
                    }
                    "getmem" => {
                        let m = get_memory(h);
                        let ok = m.is_some();
                        report(&mut out, h, l, ok, Some(m.as_ref().unwrap_or(&Value::Nil)));
                    }
                    "rand" => out.push(format!("rand {}", random(h))),
                    _ => out.push(format!("unknown command {l}")),
                }
            }
            if !h.is_null() {
                free(h);
            }
        }
        out
    }

    #[test]
    fn part_b_as_the_cpp() {
        let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../tests/platform/lua");
        let mut reader = Reader { folder: Some(root), text: Vec::new() };
        let got = part_b(include_str!("golden_lua_chunks.txt"), &mut reader);
        let all: Vec<&str> = GOLDEN.lines().collect();
        let start = all.iter().position(|l| *l == "== part B").unwrap();
        let end = all.iter().position(|l| l.starts_with("status 0 ")).unwrap();
        let want = &all[start..end];
        for (i, (g, w)) in got.iter().zip(want).enumerate() {
            assert_eq!(g, w, "part B line {}", i + 1);
        }
        assert_eq!(got.len(), want.len());
        for (i, l) in all[end..].iter().enumerate() {
            let name = status_name(i as u32).to_str().unwrap();
            assert_eq!(*l, format!("status {i} {name}"));
        }
    }

    #[test]
    fn module_names_and_paths() {
        assert!(module_name(b"lib.util") && module_name(&[b'a'; 96]) && !module_name(&[b'a'; 97]));
        for bad in [&b""[..], b".a", b"a.", b"a..b", b"a/b", b"a\\b", b"C:", b"a-b"] {
            assert!(!module_name(bad));
        }
        let (p, c) = module_paths(b"lib.deep.x");
        assert_eq!(CStr::from_bytes_until_nul(&p).unwrap().to_bytes(), b"ai\\lib\\deep\\x.lua");
        assert_eq!(CStr::from_bytes_until_nul(&c).unwrap().to_bytes(), b"@ai/lib/deep/x.lua");
    }

    #[test]
    fn luaconf_facts() {
        // what EECPLua.cpp's static_asserts check on the C side
        assert_eq!(LUA_REGISTRYINDEX, -1001000);
        assert_eq!(LUAL_BUFFERSIZE, 1024);
        assert_eq!(std::mem::size_of::<luaL_Buffer>(), 4 * 8 + 1024);
        assert_eq!(LUA_MASKCOUNT, 8);
        assert_eq!((LUA_NOREF, LUA_REFNIL), (-2, -1));
        assert_eq!(std::mem::size_of::<SortEntry>(), 40);
        assert_eq!(std::mem::size_of::<lua_Integer>(), 8);
    }
}
