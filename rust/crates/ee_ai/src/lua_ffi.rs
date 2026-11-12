//! The Lua runtime's C ABI (`EECPLua.cpp`): the value tree's order, equality and text, the memory format, the seed
//! and SplitMix64; the state (`EECPLuaState`'s methods over `lua_host`). The C++ trees are read and built through the ops table the C++ passes in (`kEECPLuaTreeOps`);
//! text goes out by the ops' `append` onto the caller's `std::string`.
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"

use std::ffi::{c_char, c_void};

use crate::lua_api::lua_State;

use crate::lua_host::{self, Config, Host, JobValueRef};
use crate::lua_value::{self, build, CppTree, EeAiLuaKey, EeAiLuaTreeOps, Value};

/// `EECPLuaKey::operator<`. Safety: both keys are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_key_less(a: *const EeAiLuaKey, b: *const EeAiLuaKey) -> bool {
    (*a).get() < (*b).get()
}

/// `EECPLuaKey::operator==`. Safety: both keys are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_key_equal(a: *const EeAiLuaKey, b: *const EeAiLuaKey) -> bool {
    (*a).get() == (*b).get()
}

/// `EECPLuaValue::operator==`. Safety: `ops` is the C++ table, `a` and `b` live values.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_value_equal(ops: *const EeAiLuaTreeOps, a: *const c_void, b: *const c_void) -> bool {
    let ops = &*ops;
    lua_value::equal(CppTree::new(ops, a), CppTree::new(ops, b))
}

/// `EECPLuaValue::ToText`, appended to `out` (a `std::string`). Safety: as above.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_value_text(ops: *const EeAiLuaTreeOps, value: *const c_void, out: *mut c_void) {
    let ops = &*ops;
    let mut text = Vec::new();
    lua_value::to_text(CppTree::new(ops, value), &mut text);
    (ops.append)(out, text.as_ptr(), text.len());
}

/// `EECPLuaEncode`: appends the value's bytes to `out` (a `std::string`). Safety: as above.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_encode(ops: *const EeAiLuaTreeOps, value: *const c_void, out: *mut c_void) {
    let ops = &*ops;
    let mut bytes = Vec::new();
    lua_value::encode(CppTree::new(ops, value), &mut bytes);
    (ops.append)(out, bytes.as_ptr(), bytes.len());
}

/// `EECPLuaDecode`: one value from `data[*at..len]`; `*at` advances past it (or to where the error was found). On
/// success `out` (an `EECPLuaValue`) becomes the value; on a failure it stays as it was and the error is appended
/// to `error` (a `std::string`). Safety: `data` holds `len` bytes, `*at` is set, `out` and `error` are live.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_decode(
    ops: *const EeAiLuaTreeOps,
    data: *const u8,
    len: usize,
    at: *mut usize,
    out: *mut c_void,
    error: *mut c_void,
) -> bool {
    let ops = &*ops;
    match lua_value::decode(lua_value::bytes(data, len), &mut *at) {
        Ok(v) => {
            build(ops, out, &v);
            true
        }
        Err(e) => {
            (ops.append)(error, e.as_ptr(), e.len());
            false
        }
    }
}

/// `EECPLuaSeed`.
#[no_mangle]
pub extern "C" fn ee_ai_lua_seed(game_seed: u32, player: u32) -> u64 {
    lua_value::seed(game_seed, player)
}

/// SplitMix64: advances `*state` -> the next output. Safety: `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_splitmix(state: *mut u64) -> u64 {
    lua_value::splitmix(&mut *state)
}

// ---- the state (lua_host.rs): `EECPLuaState`'s methods

/// `EECPLuaConfig` as the state takes it (the reader is the ops' `read` on the owner, when `has_read`).
#[repr(C)]
pub struct EeAiLuaConfig {
    pub seed: u64,
    pub memory_limit: usize,
    pub budget: i64,
    pub has_read: bool,
    pub write: Option<unsafe extern "C" fn(context: *mut c_void, text: *const c_char)>,
    pub write_context: *mut c_void,
    pub check_memory: bool,
    /// `void (*)(lua_State*, void*)` or NULL: may raise a Lua error (a C-unwind function, which cbindgen cannot spell)
    pub open: *mut c_void,
    pub open_context: *mut c_void,
}

/// A new state (`EECPLuaState::EECPLuaState`) -> its handle, never NULL (see `ee_ai_lua_is_open`). Safety: `ops`
/// outlives the state; `owner` is what `ee_ai_lua_owner` answers for it.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_new(
    config: *const EeAiLuaConfig,
    ops: *const EeAiLuaTreeOps,
    owner: *mut c_void,
) -> *mut c_void {
    let c = &*config;
    let config = Config {
        seed: c.seed,
        memory_limit: c.memory_limit,
        budget: c.budget,
        has_read: c.has_read,
        write: c.write,
        write_context: c.write_context,
        check_memory: c.check_memory,
        open: if c.open.is_null() { None } else { Some(std::mem::transmute::<*mut c_void, lua_host::OpenFn>(c.open)) },
        open_context: c.open_context,
    };
    lua_host::new(config, ops, owner) as *mut c_void
}

/// Closes and frees a state. Safety: `host` came from `ee_ai_lua_new`, is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_free(host: *mut c_void) {
    lua_host::free(host as *mut Host)
}

/// Safety: `host` is live (as for every function below taking one).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_is_open(host: *const c_void) -> bool {
    lua_host::is_open(host as *const Host)
}

/// The last call's `EECPLuaStatus`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_status(host: *const c_void) -> u32 {
    lua_host::status(host as *const Host) as u32
}

/// Appends the last call's error text to `out` (a `std::string`).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_error(host: *const c_void, out: *mut c_void) {
    let h = host as *const Host;
    let e = lua_host::error(h);
    (lua_host::ops(h).append)(out, e.as_ptr(), e.len());
}

/// This call's (or the last call's) instructions and charges.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_used(host: *const c_void) -> i64 {
    lua_host::used(host as *const Host)
}

/// The budget of a call (during `CallOptional`, its own).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_budget(host: *const c_void) -> i64 {
    lua_host::budget(host as *const Host)
}

#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_memory_used(host: *const c_void) -> usize {
    lua_host::memory_used(host as *const Host)
}

#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_memory_peak(host: *const c_void) -> usize {
    lua_host::memory_peak(host as *const Host)
}

#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_rng_state(host: *const c_void) -> u64 {
    lua_host::rng_state(host as *const Host)
}

/// The state's next SplitMix64 draw (`ai.random`'s source).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_next_random(host: *mut c_void) -> u64 {
    lua_host::random(host as *mut Host)
}

/// The owner of the state `state` belongs to (`EECPLuaState::From`). Safety: `state` is a `lua_State` of a host.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_owner(state: *mut c_void) -> *mut c_void {
    lua_host::owner(state as *mut lua_State)
}

/// `EECPLuaState::Charge`: may raise a Lua error (longjmp). Safety: `state` is a host's, inside one of its calls.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_lua_charge(state: *mut c_void, amount: i64) {
    lua_host::charge(state as *mut lua_State, amount)
}

/// `LoadPersonality` (`reload` false: `module`) / `Reload` (true).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_load(host: *mut c_void, module: *const c_char, reload: bool) -> bool {
    lua_host::load(host as *mut Host, module, reload)
}

/// `Init`, `Step`, `CallOptional`: personality[name](view, memory) -> `out` (an `EECPLuaValue`: the result, or nil).
/// Safety: `name` a C string, `view` and `out` live values, `found` valid.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_entry(
    host: *mut c_void,
    name: *const c_char,
    optional: bool,
    view: *const c_void,
    budget: i64,
    out: *mut c_void,
    found: *mut bool,
) -> bool {
    let h = host as *mut Host;
    let mut v = Value::Nil;
    let ok = lua_host::entry(h, name, optional, JobValueRef::cpp(view), budget, &mut v, &mut *found);
    build(lua_host::ops(h), out, &v);
    ok
}

/// `Run`: `source` (`len` bytes) as a chunk named `name` -> `out` (its first result, or nil).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_run(
    host: *mut c_void,
    source: *const c_char,
    len: usize,
    name: *const c_char,
    out: *mut c_void,
) -> bool {
    let h = host as *mut Host;
    let mut v = Value::Nil;
    let ok = lua_host::run(h, source, len, name, &mut v);
    build(lua_host::ops(h), out, &v);
    ok
}

/// `GetMemory` -> `out` (the memory, or nil).
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_get_memory(host: *mut c_void, out: *mut c_void) -> bool {
    let h = host as *mut Host;
    let v = lua_host::get_memory(h);
    build(lua_host::ops(h), out, v.as_ref().unwrap_or(&Value::Nil));
    v.is_some()
}

/// `SaveMemory`: appends the block to `out` (a `std::string`, cleared by the caller) when it succeeds.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_save_memory(host: *mut c_void, out: *mut c_void) -> bool {
    let h = host as *mut Host;
    let mut b = Vec::new();
    let ok = lua_host::save_memory(h, &mut b);
    if ok {
        (lua_host::ops(h).append)(out, b.as_ptr(), b.len());
    }
    ok
}

/// `RestoreMemory` from `len` bytes at `data`.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_lua_restore_memory(host: *mut c_void, data: *const u8, len: usize) -> bool {
    lua_host::restore_memory(host as *mut Host, lua_value::bytes(data, len))
}

/// `EECPLuaStatusName`: a static C string.
#[no_mangle]
pub extern "C" fn ee_ai_lua_status_name(status: u32) -> *const c_char {
    lua_host::status_name(status).as_ptr()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::golden_lua::{gen_value, text, Gen};
    use crate::lua_value::{encode, equal, Key, Value};
    use crate::test_ops::{ptr, ptr_mut, OPS};

    #[test]
    fn through_the_ops_as_in_rust() {
        let mut g = Gen(99);
        for _ in 0..300 {
            let v = gen_value(&mut g, 0);
            let w = gen_value(&mut g, 0);
            unsafe {
                assert_eq!(ee_ai_lua_value_equal(&OPS, ptr(&v), ptr(&w)), equal(&v, &w));
                assert!(ee_ai_lua_value_equal(&OPS, ptr(&v), ptr(&v)));
                let mut t = Vec::new();
                ee_ai_lua_value_text(&OPS, ptr(&v), &mut t as *mut Vec<u8> as *mut c_void);
                assert_eq!(String::from_utf8(t).unwrap(), text(&v));
                let mut b = Vec::new();
                ee_ai_lua_encode(&OPS, ptr(&v), &mut b as *mut Vec<u8> as *mut c_void);
                let mut r = Vec::new();
                encode(&v, &mut r);
                assert_eq!(b, r);
                let mut d = Value::Str(b"x".to_vec());
                let mut at = 0;
                let mut e = Vec::new();
                assert!(ee_ai_lua_decode(
                    &OPS,
                    b.as_ptr(),
                    b.len(),
                    &mut at,
                    ptr_mut(&mut d),
                    &mut e as *mut Vec<u8> as *mut c_void
                ));
                let mut again = Vec::new();
                encode(&d, &mut again);
                assert_eq!(again, b);
                let mut at = 0;
                let mut kept = Value::Int(5);
                let cut = &b[..b.len() - 1];
                assert!(!ee_ai_lua_decode(
                    &OPS,
                    cut.as_ptr(),
                    cut.len(),
                    &mut at,
                    ptr_mut(&mut kept),
                    &mut e as *mut Vec<u8> as *mut c_void
                ));
                assert_eq!(kept, Value::Int(5));
                assert!(!e.is_empty());
            }
        }
        let (a, b) = (EeAiLuaKey::of(Key::Int(3)), EeAiLuaKey::of(Key::Str(b"")));
        unsafe {
            assert!(ee_ai_lua_key_less(&a, &b) && !ee_ai_lua_key_less(&b, &a) && ee_ai_lua_key_equal(&a, &a));
        }
        let mut s = 0;
        assert_eq!(unsafe { ee_ai_lua_splitmix(&mut s) }, 0xe220a8397b1dcdaf);
        assert_eq!(ee_ai_lua_seed(7, 3), (7 << 32) | 3);
    }
}
