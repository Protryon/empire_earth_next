//! Tests only: an ops table (`EeAiLuaTreeOps`) over Rust [`Value`]s, playing the C++ tree's part, and a script
//! reader over a folder (tests/platform/lua, as luatest).

use std::ffi::{c_char, c_void, CStr};
use std::mem::{offset_of, size_of};

use crate::golden_lua::set;
use crate::lua_value::{EeAiLuaKey, EeAiLuaNode, EeAiLuaTreeOps, Kind, OwnedKey, Value};

unsafe extern "C" fn node(value: *const c_void, out: *mut EeAiLuaNode) {
    let v = &*(value as *const Value);
    let o = &mut *out;
    *o = EeAiLuaNode {
        kind: 0,
        boolean: 0,
        integer: 0,
        text: std::ptr::null(),
        text_len: 0,
        count: 0,
        items: std::ptr::null(),
        item_stride: 0,
        keys: std::ptr::null(),
        key_stride: 0,
    };
    match v {
        Value::Nil => {}
        Value::Bool(b) => {
            o.kind = Kind::Bool as u8;
            o.boolean = *b as u8;
        }
        Value::Int(i) => {
            o.kind = Kind::Int as u8;
            o.integer = *i;
        }
        Value::Str(s) => {
            o.kind = Kind::Str as u8;
            o.text = s.as_ptr();
            o.text_len = s.len();
        }
        Value::Array(a) => {
            o.kind = Kind::Array as u8;
            o.count = a.len();
            o.items = a.as_ptr() as *const c_void;
            o.item_stride = size_of::<Value>();
        }
        Value::Map(m) => {
            type Field = (OwnedKey, Value);
            o.kind = Kind::Map as u8;
            o.count = m.len();
            let base = m.as_ptr() as *const u8;
            o.items = base.add(offset_of!(Field, 1)) as *const c_void;
            o.keys = base.add(offset_of!(Field, 0)) as *const c_void;
            o.item_stride = size_of::<Field>();
            o.key_stride = size_of::<Field>();
        }
    }
}

unsafe extern "C" fn key(k: *const c_void, out: *mut EeAiLuaKey) {
    *out = EeAiLuaKey::of((*(k as *const OwnedKey)).as_key());
}

unsafe extern "C" fn set_value(value: *mut c_void, kind: u8, boolean: u8, integer: i64, text: *const u8, len: usize) {
    *(value as *mut Value) = match Kind::from_u8(kind) {
        Kind::Nil => Value::Nil,
        Kind::Bool => Value::Bool(boolean != 0),
        Kind::Int => Value::Int(integer),
        Kind::Str => Value::Str(crate::lua_value::bytes(text, len).to_vec()),
        Kind::Array => Value::Array(Vec::new()),
        Kind::Map => Value::Map(Vec::new()),
    };
}

unsafe extern "C" fn push(array: *mut c_void) -> *mut c_void {
    match &mut *(array as *mut Value) {
        Value::Array(a) => {
            a.push(Value::Nil);
            a.last_mut().unwrap() as *mut Value as *mut c_void
        }
        _ => panic!("push on a value that is not an array"),
    }
}

unsafe extern "C" fn field(map: *mut c_void, k: *const EeAiLuaKey) -> *mut c_void {
    let key = match (*k).get() {
        crate::lua_value::Key::Int(i) => OwnedKey::Int(i),
        crate::lua_value::Key::Str(s) => OwnedKey::Str(s.to_vec()),
    };
    match &mut *(map as *mut Value) {
        Value::Map(m) => {
            set(m, key.clone(), Value::Nil);
            let i = m.iter().position(|(x, _)| *x == key).unwrap();
            &mut m[i].1 as *mut Value as *mut c_void
        }
        _ => panic!("a field of a value that is not a map"),
    }
}

unsafe extern "C" fn append(string: *mut c_void, data: *const u8, len: usize) {
    (*(string as *mut Vec<u8>)).extend_from_slice(crate::lua_value::bytes(data, len));
}

/// A reader's owner: the folder playing Data (None: no reader) and the text being loaded.
pub struct Reader {
    pub folder: Option<std::path::PathBuf>,
    pub text: Vec<u8>,
}

unsafe extern "C" fn read(owner: *mut c_void, path: *const c_char, text: *mut *const u8, len: *mut usize) -> bool {
    let r = &mut *(owner as *mut Reader);
    r.text.clear();
    let Some(folder) = &r.folder else { return false };
    let rel = CStr::from_ptr(path).to_str().unwrap().replace('\\', "/");
    match std::fs::read(folder.join(rel)) {
        Ok(b) => {
            r.text = b;
            *text = r.text.as_ptr();
            *len = r.text.len();
            true
        }
        Err(_) => false,
    }
}

unsafe extern "C" fn read_done(owner: *mut c_void) {
    (*(owner as *mut Reader)).text = Vec::new();
}

pub static OPS: EeAiLuaTreeOps = EeAiLuaTreeOps { node, key, set: set_value, push, field, append, read, read_done };

pub fn ptr(v: &Value) -> *const c_void {
    v as *const Value as *const c_void
}

pub fn ptr_mut(v: &mut Value) -> *mut c_void {
    v as *mut Value as *mut c_void
}
