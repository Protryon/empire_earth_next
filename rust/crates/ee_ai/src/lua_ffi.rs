//! The Lua runtime's C ABI (`EECPLua.cpp`): the value tree's order, equality and text, the memory format, the seed
//! and SplitMix64. The C++ trees are read and built through the ops table the C++ passes in (`kEECPLuaTreeOps`);
//! text goes out by the ops' `append` onto the caller's `std::string`.
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"

use std::ffi::c_void;

use crate::lua_value::{self, build, CppTree, EeAiLuaKey, EeAiLuaTreeOps};

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
