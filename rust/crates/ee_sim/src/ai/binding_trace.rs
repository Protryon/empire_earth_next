//! TEST ONLY: the trace of the AI bindings' calls (`ai.*`, docs/AI_LUA_UNITS.md), the golden of the per-unit
//! interface's port (docs/progress/ai-lua-host.md).
//!
//! `EE_AI_BINDING_TRACE` set in the environment (`full`: the values' text too; anything else: their FNV-1a 64 only)
//! makes each binding call write two lines through the registered output (the C++'s `ESDebugOut`, so EE-SIM's
//! trace file holds them beside the EE-AI lines, which `sim_oracle.py compare` keeps apart: `EE-AIB` is not
//! `EE-AI `):
//!
//! ```text
//! EE-AIB > units a=<hash> [<text of the arguments>]
//! EE-AIB < units n=1 r=<hash> [<text of the results>]
//! ```
//!
//! and each strategy step one line of its view and its orders (`step v=<hash> o=<hash>`). A binding that raises
//! writes only its first line. The text of a value is read without a metamethod, a conversion or an allocation in
//! the Lua state (raw `lua_next`, numbers by `lua_tointegerx`/`lua_tonumberx`, strings by `lua_tolstring` on strings
//! only), so the trace cannot change what the script sees; a `lua_checkstack` grows the stack only when fewer than
//! 12 slots are free, which no binding leaves. Tables are written to a depth of 4, keys in `lua_next`'s order: the
//! order the binding built them in, which the port must keep.
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"

use std::ffi::{c_char, c_int, CStr};
use std::fmt::Write as _;
use std::sync::atomic::{AtomicI32, AtomicUsize, Ordering};

use crate::ai::lua_api::*;

/// The output the C++ registers (`ESDebugOut("%s", text)`).
pub type EeAiBindingTraceOut = unsafe extern "C" fn(text: *const c_char);

static MODE: AtomicI32 = AtomicI32::new(-1);
static OUT: AtomicUsize = AtomicUsize::new(0);

/// 0 off, 1 hashes, 2 hashes and text (`EE_AI_BINDING_TRACE`, read once).
pub fn mode() -> i32 {
    let m = MODE.load(Ordering::Relaxed);
    if m >= 0 {
        return m;
    }
    let m = match std::env::var("EE_AI_BINDING_TRACE") {
        Ok(v) if v == "full" => 2,
        Ok(v) if !v.is_empty() && v != "0" => 1,
        _ => 0,
    };
    MODE.store(m, Ordering::Relaxed);
    m
}

/// Whether the binding trace is on.
#[no_mangle]
pub extern "C" fn ee_ai_binding_trace_on() -> bool {
    mode() > 0
}

/// Registers the trace's output.
#[no_mangle]
pub extern "C" fn ee_ai_binding_trace_set_out(out: EeAiBindingTraceOut) {
    OUT.store(out as usize, Ordering::Relaxed);
}

/// FNV-1a 64.
pub fn fnv64(bytes: &[u8]) -> u64 {
    let mut h: u64 = 0xcbf2_9ce4_8422_2325;
    for &b in bytes {
        h ^= b as u64;
        h = h.wrapping_mul(0x0000_0100_0000_01b3);
    }
    h
}

fn emit(line: &str) {
    let out = OUT.load(Ordering::Relaxed);
    let mut text = Vec::with_capacity(line.len() + 2);
    text.extend_from_slice(line.as_bytes());
    text.extend_from_slice(b"\n\0");
    if out == 0 {
        eprint!("{line}\n");
        return;
    }
    // SAFETY: the registered function takes a NUL-terminated text
    unsafe {
        let f: EeAiBindingTraceOut = std::mem::transmute::<usize, EeAiBindingTraceOut>(out);
        f(text.as_ptr() as *const c_char);
    }
}

fn escape(out: &mut String, bytes: &[u8]) {
    out.push('"');
    for &b in bytes {
        match b {
            b'"' => out.push_str("\\\""),
            b'\\' => out.push_str("\\\\"),
            0x20..=0x7e => out.push(b as char),
            _ => {
                let _ = write!(out, "\\x{b:02x}");
            }
        }
    }
    out.push('"');
}

/// The text of the value at `index` (absolute), as the module doc says.
///
/// # Safety
/// `l` is a live state and `index` a valid absolute index.
pub unsafe fn value_text(l: *mut lua_State, index: c_int, depth: u32, out: &mut String) {
    let t = lua_type(l, index);
    match t {
        LUA_TNIL => out.push_str("nil"),
        LUA_TBOOLEAN => out.push_str(if lua_toboolean(l, index) != 0 { "true" } else { "false" }),
        LUA_TNUMBER => {
            if lua_isinteger(l, index) != 0 {
                let _ = write!(out, "{}", lua_tointegerx(l, index, std::ptr::null_mut()));
            } else {
                let _ = write!(out, "{:?}", lua_tonumberx(l, index, std::ptr::null_mut()));
            }
        }
        LUA_TSTRING => {
            let mut len = 0usize;
            let p = lua_tolstring(l, index, &mut len);
            let bytes = if p.is_null() { &[][..] } else { std::slice::from_raw_parts(p as *const u8, len) };
            escape(out, bytes);
        }
        LUA_TTABLE => {
            if depth >= 4 {
                out.push_str("{...}");
                return;
            }
            if lua_checkstack(l, 12) == 0 {
                out.push_str("{?}");
                return;
            }
            out.push('{');
            let mut first = true;
            lua_pushnil(l);
            while lua_next(l, index) != 0 {
                if !first {
                    out.push(',');
                }
                first = false;
                let top = lua_gettop(l);
                value_text(l, top - 1, depth + 1, out);
                out.push('=');
                value_text(l, top, depth + 1, out);
                lua_pop(l, 1);
            }
            out.push('}');
        }
        _ => {
            let name = CStr::from_ptr(lua_typename(l, t));
            out.push_str(&name.to_string_lossy());
        }
    }
}

/// The text of the stack slots `first..=last` (absolute), comma separated.
unsafe fn slots_text(l: *mut lua_State, first: c_int, last: c_int) -> String {
    let mut s = String::new();
    for i in first..=last {
        if i > first {
            s.push(',');
        }
        value_text(l, i, 0, &mut s);
    }
    s
}

fn line(head: &str, text: &str) -> String {
    let mut s = format!("{head}{:016x}", fnv64(text.as_bytes()));
    if mode() >= 2 {
        s.push(' ');
        s.push_str(text);
    }
    s
}

/// A binding's call begins: its arguments are the stack.
///
/// # Safety
/// `state` is the binding's live state; `name` NUL-terminated.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_binding_trace_enter(state: *mut std::ffi::c_void, name: *const c_char) {
    if mode() == 0 {
        return;
    }
    let l = state as *mut lua_State;
    let text = slots_text(l, 1, lua_gettop(l));
    let name = CStr::from_ptr(name).to_string_lossy();
    emit(&line(&format!("EE-AIB > {name} a="), &text));
}

/// A binding returned `results` values (the stack's top ones).
///
/// # Safety
/// As [`ee_ai_binding_trace_enter`].
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_binding_trace_leave(state: *mut std::ffi::c_void, name: *const c_char, results: c_int) {
    if mode() == 0 {
        return;
    }
    let l = state as *mut lua_State;
    let top = lua_gettop(l);
    let text = slots_text(l, top - results + 1, top);
    let name = CStr::from_ptr(name).to_string_lossy();
    emit(&line(&format!("EE-AIB < {name} n={results} r="), &text));
}

/// A strategy step's view and orders (their `EECPLuaValue` texts).
///
/// # Safety
/// The texts are `len` bytes each (or null with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_binding_trace_step(
    player: i64,
    what: *const c_char,
    view: *const u8,
    view_len: usize,
    orders: *const u8,
    orders_len: usize,
) {
    if mode() == 0 {
        return;
    }
    let bytes = |p: *const u8, n: usize| if p.is_null() { &[][..] } else { std::slice::from_raw_parts(p, n) };
    let v = bytes(view, view_len);
    let o = bytes(orders, orders_len);
    let what = if what.is_null() { "step".into() } else { CStr::from_ptr(what).to_string_lossy() };
    let mut s = format!("EE-AIB {what} p{player} v={:016x} o={:016x}", fnv64(v), fnv64(o));
    if mode() >= 2 {
        s.push(' ');
        s.push_str(&String::from_utf8_lossy(v));
        s.push_str(" -> ");
        s.push_str(&String::from_utf8_lossy(o));
    }
    emit(&s);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fnv_is_the_standard_one() {
        assert_eq!(fnv64(b""), 0xcbf2_9ce4_8422_2325);
        assert_eq!(fnv64(b"a"), 0xaf63_dc4c_8601_ec8c);
    }

    #[test]
    fn escapes_quotes_and_bytes() {
        let mut s = String::new();
        escape(&mut s, b"a\"b\\c\n\xff");
        assert_eq!(s, "\"a\\\"b\\\\c\\x0a\\xff\"");
    }
}
