//! The part of Lua's C API that `lua_host` calls, declared `extern "C"` (cannot unwind) over `mlua-sys`'s types and
//! constants (`lua54`, `external`: the vendored Lua the C++ build compiles).
//!
//! Why not `mlua-sys`'s own functions: it declares them `extern "C-unwind"`. Lua raises an error with longjmp, and the
//! MSVC CRT's longjmp unwinds the stack (an SEH unwind). With `panic = "abort"` (the release profile) every Rust
//! function cannot unwind, so rustc puts an abort pad around each call of a function that may unwind: the first
//! script error crossing a Rust frame aborted the game ("panic in a function that cannot unwind"). Declared
//! `extern "C"`, the calls get no pad in either profile; the functions Lua calls are `extern "C-unwind"` (no pad
//! under `panic = "unwind"`, the tests' profile, either). Every macro of `lua.h`/`lauxlib.h` used is spelt here as
//! the C defines it.
#![allow(non_snake_case, non_camel_case_types, clippy::missing_safety_doc)]

use std::ffi::{c_char, c_int, c_void};

pub use mlua_sys::{
    luaL_Buffer, lua_Debug, lua_Integer, lua_Number, lua_State, lua_upvalueindex, LUAL_BUFFERSIZE, LUA_ERRMEM,
    LUA_ERRSYNTAX, LUA_MASKCOUNT, LUA_NOREF, LUA_OK, LUA_REFNIL, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS, LUA_TBOOLEAN,
    LUA_TFUNCTION, LUA_TLIGHTUSERDATA, LUA_TNIL, LUA_TNUMBER, LUA_TSTRING, LUA_TTABLE, LUA_TTHREAD, LUA_TUSERDATA,
};

/// A C function Lua calls (`lua_CFunction`): it may raise (longjmp).
pub(crate) type lua_CFunction = unsafe extern "C-unwind" fn(L: *mut lua_State) -> c_int;
/// The count hook (`lua_Hook`): it may raise.
pub(crate) type lua_Hook = unsafe extern "C-unwind" fn(L: *mut lua_State, ar: *mut lua_Debug);
/// The allocator (`lua_Alloc`): it never raises.
pub(crate) type lua_Alloc =
    unsafe extern "C" fn(ud: *mut c_void, ptr: *mut c_void, osize: usize, nsize: usize) -> *mut c_void;
type lua_KFunction = unsafe extern "C-unwind" fn(L: *mut lua_State, status: c_int, ctx: isize) -> c_int;

// (Lua's own declarations come from lua.h: lib.rs keeps this module out of the headers)
extern "C" {
    pub fn lua_newstate(f: lua_Alloc, ud: *mut c_void) -> *mut lua_State;
    pub fn lua_close(L: *mut lua_State);
    pub fn lua_atpanic(L: *mut lua_State, panicf: lua_CFunction) -> Option<lua_CFunction>;
    pub fn lua_getallocf(L: *mut lua_State, ud: *mut *mut c_void) -> lua_Alloc;
    pub fn lua_sethook(L: *mut lua_State, func: Option<lua_Hook>, mask: c_int, count: c_int);

    pub fn lua_absindex(L: *mut lua_State, idx: c_int) -> c_int;
    pub fn lua_gettop(L: *mut lua_State) -> c_int;
    pub fn lua_settop(L: *mut lua_State, idx: c_int);
    pub fn lua_pushvalue(L: *mut lua_State, idx: c_int);
    pub fn lua_rotate(L: *mut lua_State, idx: c_int, n: c_int);
    pub fn lua_copy(L: *mut lua_State, fromidx: c_int, toidx: c_int);
    pub fn lua_checkstack(L: *mut lua_State, n: c_int) -> c_int;

    pub fn lua_isinteger(L: *mut lua_State, idx: c_int) -> c_int;
    pub fn lua_type(L: *mut lua_State, idx: c_int) -> c_int;
    pub fn lua_typename(L: *mut lua_State, tp: c_int) -> *const c_char;
    pub fn lua_tonumberx(L: *mut lua_State, idx: c_int, isnum: *mut c_int) -> lua_Number;
    pub fn lua_tointegerx(L: *mut lua_State, idx: c_int, isnum: *mut c_int) -> lua_Integer;
    pub fn lua_toboolean(L: *mut lua_State, idx: c_int) -> c_int;
    pub fn lua_tolstring(L: *mut lua_State, idx: c_int, len: *mut usize) -> *const c_char;
    pub fn lua_touserdata(L: *mut lua_State, idx: c_int) -> *mut c_void;
    pub fn lua_rawlen(L: *mut lua_State, idx: c_int) -> u64;
    pub fn lua_topointer(L: *mut lua_State, idx: c_int) -> *const c_void;

    pub fn lua_pushnil(L: *mut lua_State);
    pub fn lua_pushinteger(L: *mut lua_State, n: lua_Integer);
    pub fn lua_pushlstring(L: *mut lua_State, s: *const c_char, len: usize) -> *const c_char;
    pub fn lua_pushstring(L: *mut lua_State, s: *const c_char) -> *const c_char;
    pub fn lua_pushcclosure(L: *mut lua_State, f: lua_CFunction, n: c_int);
    pub fn lua_pushboolean(L: *mut lua_State, b: c_int);
    pub fn lua_pushlightuserdata(L: *mut lua_State, p: *mut c_void);

    pub fn lua_getfield(L: *mut lua_State, idx: c_int, k: *const c_char) -> c_int;
    pub fn lua_rawget(L: *mut lua_State, idx: c_int) -> c_int;
    pub fn lua_rawgeti(L: *mut lua_State, idx: c_int, n: lua_Integer) -> c_int;
    pub fn lua_createtable(L: *mut lua_State, narr: c_int, nrec: c_int);
    pub fn lua_newuserdatauv(L: *mut lua_State, sz: usize, nuvalue: c_int) -> *mut c_void;
    pub fn lua_getmetatable(L: *mut lua_State, objindex: c_int) -> c_int;
    pub fn lua_setfield(L: *mut lua_State, idx: c_int, k: *const c_char);
    pub fn lua_rawset(L: *mut lua_State, idx: c_int);
    pub fn lua_rawseti(L: *mut lua_State, idx: c_int, n: lua_Integer);

    pub fn lua_callk(L: *mut lua_State, nargs: c_int, nresults: c_int, ctx: isize, k: Option<lua_KFunction>);
    pub fn lua_pcallk(
        L: *mut lua_State,
        nargs: c_int,
        nresults: c_int,
        errfunc: c_int,
        ctx: isize,
        k: Option<lua_KFunction>,
    ) -> c_int;
    #[link_name = "lua_error"]
    fn lua_error_(L: *mut lua_State) -> c_int;
    pub fn lua_next(L: *mut lua_State, idx: c_int) -> c_int;

    pub fn luaL_getmetafield(L: *mut lua_State, obj: c_int, e: *const c_char) -> c_int;
    pub fn luaL_tolstring(L: *mut lua_State, idx: c_int, len: *mut usize) -> *const c_char;
    pub fn luaL_argerror(L: *mut lua_State, arg: c_int, extramsg: *const c_char) -> c_int;
    pub fn luaL_checklstring(L: *mut lua_State, arg: c_int, l: *mut usize) -> *const c_char;
    pub fn luaL_checkinteger(L: *mut lua_State, arg: c_int) -> lua_Integer;
    pub fn luaL_checkstack(L: *mut lua_State, sz: c_int, msg: *const c_char);
    pub fn luaL_checktype(L: *mut lua_State, arg: c_int, t: c_int);
    pub fn luaL_checkany(L: *mut lua_State, arg: c_int);
    pub fn luaL_error(L: *mut lua_State, fmt: *const c_char, ...) -> c_int;
    pub fn luaL_ref(L: *mut lua_State, t: c_int) -> c_int;
    pub fn luaL_unref(L: *mut lua_State, t: c_int, r: c_int);
    pub fn luaL_loadbufferx(
        L: *mut lua_State,
        buff: *const c_char,
        sz: usize,
        name: *const c_char,
        mode: *const c_char,
    ) -> c_int;
    pub fn luaL_traceback(L: *mut lua_State, L1: *mut lua_State, msg: *const c_char, level: c_int);
    pub fn luaL_requiref(L: *mut lua_State, modname: *const c_char, openf: lua_CFunction, glb: c_int);
    pub fn luaL_buffinit(L: *mut lua_State, B: *mut luaL_Buffer);
    pub fn luaL_prepbuffsize(B: *mut luaL_Buffer, sz: usize) -> *mut c_char;
    pub fn luaL_addvalue(B: *mut luaL_Buffer);
    pub fn luaL_pushresult(B: *mut luaL_Buffer);
}

extern "C-unwind" {
    // only passed to luaL_requiref, never called from Rust
    pub fn luaopen_base(L: *mut lua_State) -> c_int;
    pub fn luaopen_table(L: *mut lua_State) -> c_int;
    pub fn luaopen_string(L: *mut lua_State) -> c_int;
    pub fn luaopen_math(L: *mut lua_State) -> c_int;
    pub fn luaopen_utf8(L: *mut lua_State) -> c_int;
}

// ---- lua.h's and lauxlib.h's macros

#[inline(always)]
pub unsafe fn lua_error(L: *mut lua_State) -> ! {
    lua_error_(L);
    unreachable!("lua_error returned")
}
#[inline(always)]
pub unsafe fn lua_call(L: *mut lua_State, n: c_int, r: c_int) {
    lua_callk(L, n, r, 0, None)
}
#[inline(always)]
pub unsafe fn lua_pcall(L: *mut lua_State, n: c_int, r: c_int, f: c_int) -> c_int {
    lua_pcallk(L, n, r, f, 0, None)
}
#[inline(always)]
pub unsafe fn lua_tonumber(L: *mut lua_State, i: c_int) -> lua_Number {
    lua_tonumberx(L, i, std::ptr::null_mut())
}
#[inline(always)]
pub unsafe fn lua_tointeger(L: *mut lua_State, i: c_int) -> lua_Integer {
    lua_tointegerx(L, i, std::ptr::null_mut())
}
#[inline(always)]
pub unsafe fn lua_pop(L: *mut lua_State, n: c_int) {
    lua_settop(L, -n - 1)
}
#[inline(always)]
pub unsafe fn lua_newtable(L: *mut lua_State) {
    lua_createtable(L, 0, 0)
}
#[inline(always)]
pub unsafe fn lua_pushcfunction(L: *mut lua_State, f: lua_CFunction) {
    lua_pushcclosure(L, f, 0)
}
#[inline(always)]
pub unsafe fn lua_istable(L: *mut lua_State, n: c_int) -> bool {
    lua_type(L, n) == LUA_TTABLE
}
#[inline(always)]
pub unsafe fn lua_isnil(L: *mut lua_State, n: c_int) -> bool {
    lua_type(L, n) == LUA_TNIL
}
#[inline(always)]
pub unsafe fn lua_pushglobaltable(L: *mut lua_State) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);
}
#[inline(always)]
pub unsafe fn lua_tostring(L: *mut lua_State, i: c_int) -> *const c_char {
    lua_tolstring(L, i, std::ptr::null_mut())
}
#[inline(always)]
pub unsafe fn lua_insert(L: *mut lua_State, idx: c_int) {
    lua_rotate(L, idx, 1)
}
#[inline(always)]
pub unsafe fn lua_replace(L: *mut lua_State, idx: c_int) {
    lua_copy(L, -1, idx);
    lua_pop(L, 1)
}
#[inline(always)]
pub unsafe fn luaL_typename(L: *mut lua_State, i: c_int) -> *const c_char {
    lua_typename(L, lua_type(L, i))
}
#[inline(always)]
pub unsafe fn luaL_addchar(B: *mut luaL_Buffer, c: c_char) {
    if (*B).n >= (*B).size {
        luaL_prepbuffsize(B, 1);
    }
    *(*B).b.add((*B).n) = c;
    (*B).n += 1;
}
