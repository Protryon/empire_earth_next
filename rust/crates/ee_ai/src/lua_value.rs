//! The Lua runtime's value tree (`ee/EECPLua.h`, `EECPLuaValue`): its key order, equality and text, the memory's
//! bytes ("EELM"'s value encoding), the seed of a computer player's RNG and SplitMix64.
//!
//! The tree stays the C++ type callers build (`EECPLuaValue`); the algorithms here read any tree through [`Tree`]:
//! a C++ one through the C++ accessors ([`CppTree`], one call per value), or a Rust [`Value`] (a decoded block, a
//! converted Lua table, the tests).
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"

use std::cmp::Ordering;
use std::ffi::{c_char, c_void};

/// The deepest nesting the memory format, the tree -> Lua push and the Lua -> tree conversion take
/// (`kEECPLuaMaxDepth`).
pub const MAX_DEPTH: usize = 64;

/// A value's type (`EECPLuaValue::Type`).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    Nil = 0,
    Bool = 1,
    Int = 2,
    Str = 3,
    Array = 4,
    Map = 5,
}

impl Kind {
    pub fn from_u8(v: u8) -> Kind {
        match v {
            1 => Kind::Bool,
            2 => Kind::Int,
            3 => Kind::Str,
            4 => Kind::Array,
            5 => Kind::Map,
            _ => Kind::Nil,
        }
    }
}

/// A table key, borrowed. The order (`EECPLuaKey::operator<`): integers ascending, then strings by their bytes (a
/// prefix first).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Key<'a> {
    Int(i64),
    Str(&'a [u8]),
}

impl Ord for Key<'_> {
    fn cmp(&self, other: &Self) -> Ordering {
        match (self, other) {
            (Key::Int(a), Key::Int(b)) => a.cmp(b),
            (Key::Int(_), Key::Str(_)) => Ordering::Less,
            (Key::Str(_), Key::Int(_)) => Ordering::Greater,
            (Key::Str(a), Key::Str(b)) => a.cmp(b),
        }
    }
}

impl PartialOrd for Key<'_> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

/// A table key, owned.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum OwnedKey {
    Int(i64),
    Str(Vec<u8>),
}

impl OwnedKey {
    pub fn as_key(&self) -> Key<'_> {
        match self {
            OwnedKey::Int(v) => Key::Int(*v),
            OwnedKey::Str(s) => Key::Str(s),
        }
    }
}

/// A tree in Rust: what a decode or a conversion from Lua gives, before the C++ copies it into its own.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Value {
    Nil,
    Bool(bool),
    Int(i64),
    Str(Vec<u8>),
    Array(Vec<Value>),
    /// the fields in key order
    Map(Vec<(OwnedKey, Value)>),
}

/// A read-only walk over a tree. `len` is an array's item count or a map's field count; `item(i)` an array's item
/// or a map's value; `key(i)` a map's key.
pub trait Tree<'a>: Copy {
    fn kind(&self) -> Kind;
    fn boolean(&self) -> bool;
    fn int(&self) -> i64;
    fn text(&self) -> &'a [u8];
    fn count(&self) -> usize;
    fn item(&self, i: usize) -> Self;
    fn key(&self, i: usize) -> Key<'a>;
}

impl<'a> Tree<'a> for &'a Value {
    fn kind(&self) -> Kind {
        match self {
            Value::Nil => Kind::Nil,
            Value::Bool(_) => Kind::Bool,
            Value::Int(_) => Kind::Int,
            Value::Str(_) => Kind::Str,
            Value::Array(_) => Kind::Array,
            Value::Map(_) => Kind::Map,
        }
    }
    fn boolean(&self) -> bool {
        matches!(self, Value::Bool(true))
    }
    fn int(&self) -> i64 {
        match self {
            Value::Int(v) => *v,
            _ => 0,
        }
    }
    fn text(&self) -> &'a [u8] {
        match self {
            Value::Str(s) => s,
            _ => &[],
        }
    }
    fn count(&self) -> usize {
        match self {
            Value::Array(a) => a.len(),
            Value::Map(m) => m.len(),
            _ => 0,
        }
    }
    fn item(&self, i: usize) -> Self {
        match self {
            Value::Array(a) => &a[i],
            Value::Map(m) => &m[i].1,
            _ => unreachable!("an item of a scalar"),
        }
    }
    fn key(&self, i: usize) -> Key<'a> {
        match self {
            Value::Map(m) => m[i].0.as_key(),
            _ => unreachable!("a key of a value that is not a map"),
        }
    }
}

// ---- the C++ tree

/// One C++ value as the accessor fills it: the scalars, and the items' and keys' arrays (the vectors' storage, with
/// the element sizes).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiLuaNode {
    pub kind: u8,
    pub boolean: u8,
    pub integer: i64,
    pub text: *const u8,
    pub text_len: usize,
    /// an array's items, or a map's fields
    pub count: usize,
    pub items: *const c_void,
    pub item_stride: usize,
    pub keys: *const c_void,
    pub key_stride: usize,
}

/// A table key across the C ABI (`EECPLuaKey`).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiLuaKey {
    pub is_int: u8,
    pub integer: i64,
    pub text: *const u8,
    pub text_len: usize,
}

impl EeAiLuaKey {
    /// Safety: `text` points to `text_len` bytes valid for 'a (or `text_len` is 0).
    pub unsafe fn get<'a>(&self) -> Key<'a> {
        if self.is_int != 0 {
            Key::Int(self.integer)
        } else {
            Key::Str(bytes(self.text, self.text_len))
        }
    }
    pub fn of(k: Key<'_>) -> EeAiLuaKey {
        match k {
            Key::Int(v) => EeAiLuaKey { is_int: 1, integer: v, text: std::ptr::null(), text_len: 0 },
            Key::Str(s) => EeAiLuaKey { is_int: 0, integer: 0, text: s.as_ptr(), text_len: s.len() },
        }
    }
}

/// Safety: `p` points to `n` readable bytes (or `n` is 0).
pub(crate) unsafe fn bytes<'a>(p: *const u8, n: usize) -> &'a [u8] {
    if n == 0 || p.is_null() {
        &[]
    } else {
        std::slice::from_raw_parts(p, n)
    }
}

/// The C++ side's functions over its tree and strings (`EECPLua.cpp`'s `kEECPLuaTreeOps`): read a value or a key,
/// build a value (set a scalar or an empty container, push an item, add a field: both return the new child), append
/// bytes to a `std::string`, and read a script for the runtime (the text stays the owner's until `read_done`).
#[repr(C)]
pub struct EeAiLuaTreeOps {
    pub node: unsafe extern "C" fn(value: *const c_void, out: *mut EeAiLuaNode),
    pub key: unsafe extern "C" fn(key: *const c_void, out: *mut EeAiLuaKey),
    pub set: unsafe extern "C" fn(value: *mut c_void, kind: u8, boolean: u8, integer: i64, text: *const u8, len: usize),
    pub push: unsafe extern "C" fn(array: *mut c_void) -> *mut c_void,
    pub field: unsafe extern "C" fn(map: *mut c_void, key: *const EeAiLuaKey) -> *mut c_void,
    pub append: unsafe extern "C" fn(string: *mut c_void, data: *const u8, len: usize),
    pub read:
        unsafe extern "C" fn(owner: *mut c_void, path: *const c_char, text: *mut *const u8, len: *mut usize) -> bool,
    pub read_done: unsafe extern "C" fn(owner: *mut c_void),
}

/// A C++ value read through the ops: no destructor (it may live across a Lua error).
#[derive(Clone, Copy)]
pub struct CppTree<'a> {
    ops: &'a EeAiLuaTreeOps,
    node: EeAiLuaNode,
}

impl<'a> CppTree<'a> {
    /// Safety: `value` is a live `EECPLuaValue` that nobody changes for 'a.
    pub unsafe fn new(ops: &'a EeAiLuaTreeOps, value: *const c_void) -> CppTree<'a> {
        let mut node = EeAiLuaNode {
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
        (ops.node)(value, &mut node);
        CppTree { ops, node }
    }
}

impl<'a> Tree<'a> for CppTree<'a> {
    fn kind(&self) -> Kind {
        Kind::from_u8(self.node.kind)
    }
    fn boolean(&self) -> bool {
        self.node.boolean != 0
    }
    fn int(&self) -> i64 {
        self.node.integer
    }
    fn text(&self) -> &'a [u8] {
        unsafe { bytes(self.node.text, self.node.text_len) }
    }
    fn count(&self) -> usize {
        self.node.count
    }
    fn item(&self, i: usize) -> Self {
        unsafe {
            CppTree::new(self.ops, (self.node.items as *const u8).add(i * self.node.item_stride) as *const c_void)
        }
    }
    fn key(&self, i: usize) -> Key<'a> {
        let mut k = EeAiLuaKey { is_int: 1, integer: 0, text: std::ptr::null(), text_len: 0 };
        unsafe {
            (self.ops.key)((self.node.keys as *const u8).add(i * self.node.key_stride) as *const c_void, &mut k);
            k.get()
        }
    }
}

/// Copies a Rust tree into a C++ one (`value` becomes it). Safety: `value` is a live `EECPLuaValue`.
pub unsafe fn build(ops: &EeAiLuaTreeOps, value: *mut c_void, v: &Value) {
    match v {
        Value::Nil => (ops.set)(value, Kind::Nil as u8, 0, 0, std::ptr::null(), 0),
        Value::Bool(b) => (ops.set)(value, Kind::Bool as u8, *b as u8, 0, std::ptr::null(), 0),
        Value::Int(i) => (ops.set)(value, Kind::Int as u8, 0, *i, std::ptr::null(), 0),
        Value::Str(s) => (ops.set)(value, Kind::Str as u8, 0, 0, s.as_ptr(), s.len()),
        Value::Array(a) => {
            (ops.set)(value, Kind::Array as u8, 0, 0, std::ptr::null(), 0);
            for item in a {
                let child = (ops.push)(value);
                build(ops, child, item);
            }
        }
        Value::Map(m) => {
            (ops.set)(value, Kind::Map as u8, 0, 0, std::ptr::null(), 0);
            for (k, item) in m {
                let key = EeAiLuaKey::of(k.as_key());
                let child = (ops.field)(value, &key);
                build(ops, child, item);
            }
        }
    }
}

// ---- equality and text

/// `EECPLuaValue::operator==`: the same type, and the same scalar or the same keys and items.
pub fn equal<'a, 'b, A: Tree<'a>, B: Tree<'b>>(a: A, b: B) -> bool {
    if a.kind() != b.kind() {
        return false;
    }
    match a.kind() {
        Kind::Nil => true,
        Kind::Bool => a.boolean() == b.boolean(),
        Kind::Int => a.int() == b.int(),
        Kind::Str => a.text() == b.text(),
        Kind::Array | Kind::Map => {
            let n = a.count();
            if n != b.count() {
                return false;
            }
            if a.kind() == Kind::Map && (0..n).any(|i| a.key(i) != b.key(i)) {
                return false;
            }
            (0..n).all(|i| equal(a.item(i), b.item(i)))
        }
    }
}

fn quote(text: &[u8], out: &mut Vec<u8>) {
    const HEX: &[u8; 16] = b"0123456789abcdef";
    out.push(b'"');
    for &c in text {
        if c == b'"' || c == b'\\' {
            out.push(b'\\');
            out.push(c);
        } else if !(32..=126).contains(&c) {
            out.extend_from_slice(b"\\x");
            out.push(HEX[(c >> 4) as usize]);
            out.push(HEX[(c & 15) as usize]);
        } else {
            out.push(c);
        }
    }
    out.push(b'"');
}

fn is_identifier(text: &[u8]) -> bool {
    if text.is_empty() || text[0].is_ascii_digit() {
        return false;
    }
    text.iter().all(|&c| c.is_ascii_alphanumeric() || c == b'_')
}

/// `EECPLuaValue::ToText`: nil, true, 12, "text", [1,2], {a=1,[3]=true}.
pub fn to_text<'a, T: Tree<'a>>(v: T, out: &mut Vec<u8>) {
    match v.kind() {
        Kind::Nil => out.extend_from_slice(b"nil"),
        Kind::Bool => out.extend_from_slice(if v.boolean() { b"true" } else { b"false" }),
        Kind::Int => out.extend_from_slice(v.int().to_string().as_bytes()),
        Kind::Str => quote(v.text(), out),
        Kind::Array => {
            out.push(b'[');
            for i in 0..v.count() {
                if i > 0 {
                    out.push(b',');
                }
                to_text(v.item(i), out);
            }
            out.push(b']');
        }
        Kind::Map => {
            out.push(b'{');
            for i in 0..v.count() {
                if i > 0 {
                    out.push(b',');
                }
                match v.key(i) {
                    Key::Int(k) => out.extend_from_slice(format!("[{k}]").as_bytes()),
                    Key::Str(s) if is_identifier(s) => out.extend_from_slice(s),
                    Key::Str(s) => {
                        out.push(b'[');
                        quote(s, out);
                        out.push(b']');
                    }
                }
                out.push(b'=');
                to_text(v.item(i), out);
            }
            out.push(b'}');
        }
    }
}

// ---- the memory's bytes

const TAG_NIL: u8 = 0;
const TAG_FALSE: u8 = 1;
const TAG_TRUE: u8 = 2;
const TAG_INT: u8 = 3;
const TAG_STRING: u8 = 4;
const TAG_TABLE: u8 = 5;

fn put_key(k: Key<'_>, out: &mut Vec<u8>) {
    match k {
        Key::Int(v) => {
            out.push(TAG_INT);
            out.extend_from_slice(&v.to_le_bytes());
        }
        Key::Str(s) => {
            out.push(TAG_STRING);
            out.extend_from_slice(&(s.len() as u32).to_le_bytes());
            out.extend_from_slice(s);
        }
    }
}

/// `EECPLuaEncode` (appends): nil 0x00, false 0x01, true 0x02, int 0x03 + 8 bytes LE, string 0x04 + u32 length +
/// bytes, table 0x05 + u32 count + the pairs in key order (an array: the keys 1..n; nil items and values left out).
pub fn encode<'a, T: Tree<'a>>(v: T, out: &mut Vec<u8>) {
    match v.kind() {
        Kind::Nil => out.push(TAG_NIL),
        Kind::Bool => out.push(if v.boolean() { TAG_TRUE } else { TAG_FALSE }),
        Kind::Int => {
            out.push(TAG_INT);
            out.extend_from_slice(&v.int().to_le_bytes());
        }
        Kind::Str => {
            let s = v.text();
            out.push(TAG_STRING);
            out.extend_from_slice(&(s.len() as u32).to_le_bytes());
            out.extend_from_slice(s);
        }
        Kind::Array | Kind::Map => {
            let n = v.count();
            let count = (0..n).filter(|&i| v.item(i).kind() != Kind::Nil).count() as u32;
            out.push(TAG_TABLE);
            out.extend_from_slice(&count.to_le_bytes());
            for i in 0..n {
                let item = v.item(i);
                if item.kind() == Kind::Nil {
                    continue;
                }
                let key = if v.kind() == Kind::Array { Key::Int(i as i64 + 1) } else { v.key(i) };
                put_key(key, out);
                encode(item, out);
            }
        }
    }
}

fn get_u32(b: &[u8], at: &mut usize) -> Option<u32> {
    if *at > b.len() || b.len() - *at < 4 {
        return None;
    }
    let v = u32::from_le_bytes(b[*at..*at + 4].try_into().unwrap());
    *at += 4;
    Some(v)
}

/// The memory block's u64 (the RNG state) at `at`.
pub fn get_u64(b: &[u8], at: &mut usize) -> Option<u64> {
    if *at > b.len() || b.len() - *at < 8 {
        return None;
    }
    let v = u64::from_le_bytes(b[*at..*at + 8].try_into().unwrap());
    *at += 8;
    Some(v)
}

fn decode_value(b: &[u8], at: &mut usize, depth: usize) -> Result<Value, &'static str> {
    if *at >= b.len() {
        return Err("truncated");
    }
    let tag = b[*at];
    *at += 1;
    match tag {
        TAG_NIL => Ok(Value::Nil),
        TAG_FALSE | TAG_TRUE => Ok(Value::Bool(tag == TAG_TRUE)),
        TAG_INT => get_u64(b, at).map(|v| Value::Int(v as i64)).ok_or("truncated integer"),
        TAG_STRING => {
            let n = get_u32(b, at).ok_or("truncated string")? as usize;
            if b.len() - *at < n {
                return Err("truncated string");
            }
            let s = b[*at..*at + n].to_vec();
            *at += n;
            Ok(Value::Str(s))
        }
        TAG_TABLE => {
            if depth >= MAX_DEPTH {
                return Err("nested too deep");
            }
            let n = get_u32(b, at).ok_or("truncated table")?;
            if ((b.len() - *at) as u64) < n as u64 * 2 {
                return Err("truncated table");
            }
            let mut fields: Vec<(OwnedKey, Value)> = Vec::new();
            let mut array = n > 0;
            for i in 0..n {
                let key = match decode_value(b, at, depth + 1)? {
                    Value::Int(v) => OwnedKey::Int(v),
                    Value::Str(s) => OwnedKey::Str(s),
                    _ => return Err("a table key that is not an integer or a string"),
                };
                if let Some((last, _)) = fields.last() {
                    if last.as_key() >= key.as_key() {
                        return Err("table keys out of order");
                    }
                }
                let v = decode_value(b, at, depth + 1)?;
                if v == Value::Nil {
                    return Err("a nil table value");
                }
                array = array && key == OwnedKey::Int(i as i64 + 1);
                fields.push((key, v));
            }
            if array {
                Ok(Value::Array(fields.into_iter().map(|(_, v)| v).collect()))
            } else {
                Ok(Value::Map(fields))
            }
        }
        _ => Err("unknown tag"),
    }
}

/// `EECPLuaDecode`: one value from `b` at `*at` (advanced past it, or to where the error was found). Rejects
/// anything not canonical: keys out of order, a nil value, nesting deeper than [`MAX_DEPTH`].
pub fn decode(b: &[u8], at: &mut usize) -> Result<Value, &'static str> {
    decode_value(b, at, 0)
}

// ---- the RNG

/// `EECPLuaSeed`: the game's seed and the player id (each player has its own stream).
pub fn seed(game_seed: u32, player: u32) -> u64 {
    ((game_seed as u64) << 32) | player as u64
}

/// SplitMix64 (Steele, Lea, Flood 2014): the state advances by the golden gamma; the output is its mix.
pub fn splitmix(state: &mut u64) -> u64 {
    *state = state.wrapping_add(0x9E37_79B9_7F4A_7C15);
    let mut z = *state;
    z = (z ^ (z >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
    z = (z ^ (z >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
    z ^ (z >> 31)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn text(v: &Value) -> String {
        let mut o = Vec::new();
        to_text(v, &mut o);
        String::from_utf8(o).unwrap()
    }

    fn map(fields: Vec<(OwnedKey, Value)>) -> Value {
        Value::Map(fields)
    }

    #[test]
    fn key_order() {
        assert!(Key::Int(i64::MAX) < Key::Str(b""));
        assert!(Key::Int(-5) < Key::Int(1));
        assert!(Key::Str(b"a") < Key::Str(b"aa"));
        assert!(Key::Str(b"B") < Key::Str(b"a"));
        assert!(Key::Str(b"z") < Key::Str(b"\xff"));
    }

    #[test]
    fn pinned_bytes() {
        // lua_test.cpp's "values" section
        let m = map(vec![
            (OwnedKey::Int(2), Value::Str(b"x".to_vec())),
            (OwnedKey::Str(b"a".to_vec()), Value::Int(1)),
            (OwnedKey::Str(b"b".to_vec()), Value::Array(vec![Value::Bool(true)])),
        ]);
        assert_eq!(text(&m), "{[2]=\"x\",a=1,b=[true]}");
        let mut b = Vec::new();
        encode(&m, &mut b);
        let hex: String = b.iter().map(|c| format!("{c:02x}")).collect();
        assert_eq!(
            hex,
            "0503000000030200000000000000040100000078040100000061030100000000000000040100000062050100000003010000000000000002"
        );
        let mut at = 0;
        let d = decode(&b, &mut at).unwrap();
        assert_eq!(at, b.len());
        assert!(equal(&d, &m));
    }

    #[test]
    fn holes_and_errors() {
        let a = Value::Array(vec![Value::Int(1), Value::Nil, Value::Int(3)]);
        let mut b = Vec::new();
        encode(&a, &mut b);
        let mut at = 0;
        assert_eq!(text(&decode(&b, &mut at).unwrap()), "{[1]=1,[3]=3}");
        let mut at = 0;
        assert_eq!(decode(&[5, 1, 0, 0, 0, 2, 1, 2], &mut at), Err("a table key that is not an integer or a string"));
        let mut at = 0;
        assert_eq!(decode(&[4, 0xff, 0xff, 0xff, 0xff], &mut at), Err("truncated string"));
        assert_eq!(at, 5);
        assert!(!equal(&Value::Array(vec![]), &Value::Map(vec![])));
    }

    #[test]
    fn rng() {
        let mut s = 0;
        assert_eq!(splitmix(&mut s), 0xe220a8397b1dcdaf);
        assert_eq!(seed(7, 3), (7 << 32) | 3);
    }
}
