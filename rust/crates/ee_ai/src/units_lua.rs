//! The per-unit bindings' Lua helpers (`EECPNextUnits.cpp`, docs/AI_LUA_UNITS.md): the argument readers (ids,
//! optional fields, `near`, a class, a layer, the units filter) and the result pushers (a command's result, a
//! record's fields, a sorted id list). The bindings (`ai.units`, `ai.move`, ...) stay C++ over the game's objects
//! and call these.
//!
//! Every Lua API call is the C++'s, in its order (the same blocks allocated, the same metamethods run). A metamethod
//! may raise (a script's `__index`): the functions here are `extern "C-unwind"` over `lua_api`'s `extern "C"`
//! declarations and hold nothing with a destructor, so the error crosses them as it crossed the C++ (no abort pad
//! under the release profile's `panic = "abort"`, see lua_api.rs). Values keep the C++'s `long` (`c_long`: 32 bits on
//! Windows, 64 on the others), truncated where the C++ cast them, so a comparison sees what the C++ saw.
#![allow(clippy::missing_safety_doc)] // each function's contract is its doc's "Safety:"

use std::ffi::{c_char, c_int, c_long, c_void, CStr};

use crate::lua_api::*;

/// The ids one call takes (`kMaxIds`).
pub(crate) const MAX_IDS: usize = 4000;
/// Class names in one filter's `type` list (`kUnitsMaxTypes`).
pub(crate) const MAX_TYPES: usize = 16;

/// A command's refusal reasons (`CmdReason`), by index.
const REASON_NAMES: [&CStr; 11] = [
    c"none",
    c"bad_argument",
    c"not_own",
    c"not_seen",
    c"unexplored",
    c"cannot_place",
    c"cannot_afford",
    c"cannot_do",
    c"limit",
    c"full",
    c"not_in_epoch",
];

/// The C++'s class lookup by name (`EECPNextClassByName`) -> the class, or negative.
pub type EeAiUnitsClassByName = unsafe extern "C" fn(name: *const c_char) -> c_long;

/// An `(sort key, id)` pair of the C++'s `std::vector<std::pair<long long, long>>` (the same layout).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub struct EeAiUnitsSortEntry {
    pub key: i64,
    pub id: c_long,
}

/// `EECPUnitsFilter` (ai.units' and ai.seen's filter) as the C++ copies it in and out.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeAiUnitsFilter {
    pub kind_type: c_long,
    pub types: [c_long; 16], // MAX_TYPES
    pub type_count: c_long,
    pub category: *const c_char,
    pub has_near: bool,
    pub x: c_long,
    pub y: c_long,
    pub r: c_long,
    pub idle: c_int,
    pub built: c_int,
    pub armed: c_int,
    pub held: *const c_char,
    pub action: *const c_char,
    pub player: c_long,
    pub enemy: c_int,
    pub kind: c_long,
    pub limit: c_long,
}

unsafe fn is_integer(l: *mut lua_State, i: c_int) -> bool {
    lua_isinteger(l, i) != 0
}

/// `EECPUnitsClassArg`: an integer (a class id) or a class name at `index` -> the class (negative: none).
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_class_arg(
    state: *mut c_void,
    index: c_int,
    by_name: EeAiUnitsClassByName,
) -> c_long {
    let l = state as *mut lua_State;
    if lua_type(l, index) == LUA_TNUMBER && is_integer(l, index) {
        return lua_tointeger(l, index) as c_long;
    }
    if lua_type(l, index) != LUA_TSTRING {
        return -1;
    }
    by_name(lua_tostring(l, index))
}

/// `EECPUnitsReadIds`: the ids at `index` (an integer, or an array of integers, at most [`MAX_IDS`]) into `out`
/// (room for `MAX_IDS`), their count in `*count` -> the count of bad entries. No error.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_read_ids(
    state: *mut c_void,
    index: c_int,
    out: *mut c_long,
    count: *mut usize,
) -> c_long {
    let l = state as *mut lua_State;
    *count = 0;
    let mut bad: c_long = 0;
    if lua_type(l, index) == LUA_TNUMBER {
        if is_integer(l, index) {
            *out = lua_tointeger(l, index) as c_long;
            *count = 1;
        } else {
            bad += 1;
        }
        return bad;
    }
    if lua_type(l, index) != LUA_TTABLE {
        return 1;
    }
    let n = lua_rawlen(l, index) as lua_Integer;
    let mut i: lua_Integer = 1;
    while i <= n && *count < MAX_IDS {
        lua_rawgeti(l, index, i);
        if is_integer(l, -1) {
            *out.add(*count) = lua_tointeger(l, -1) as c_long;
            *count += 1;
        } else {
            bad += 1;
        }
        lua_pop(l, 1);
        i += 1;
    }
    bad
}

/// `EECPUnitsOptInt`: an integer field of the table at `table` -> found, the value in `*out`.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_opt_int(
    state: *mut c_void,
    table: c_int,
    key: *const c_char,
    out: *mut c_long,
) -> bool {
    let l = state as *mut lua_State;
    if lua_type(l, table) != LUA_TTABLE {
        return false;
    }
    lua_getfield(l, table, key);
    let found = is_integer(l, -1);
    if found {
        *out = lua_tointeger(l, -1) as c_long;
    }
    lua_pop(l, 1);
    found
}

/// `EECPUnitsOptBool`: a field's truth (nil, or no table: `default`).
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_opt_bool(
    state: *mut c_void,
    table: c_int,
    key: *const c_char,
    default: bool,
) -> bool {
    let l = state as *mut lua_State;
    if lua_type(l, table) != LUA_TTABLE {
        return default;
    }
    lua_getfield(l, table, key);
    let v = if lua_isnil(l, -1) { default } else { lua_toboolean(l, -1) != 0 };
    lua_pop(l, 1);
    v
}

/// `EECPUnitsOptString`: a string field -> a pointer into the Lua string (valid while the table holds it), or NULL.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_opt_string(
    state: *mut c_void,
    table: c_int,
    key: *const c_char,
) -> *const c_char {
    let l = state as *mut lua_State;
    if lua_type(l, table) != LUA_TTABLE {
        return std::ptr::null();
    }
    lua_getfield(l, table, key);
    let v = if lua_type(l, -1) == LUA_TSTRING { lua_tostring(l, -1) } else { std::ptr::null() };
    lua_pop(l, 1);
    v
}

/// `EECPUnitsOptNear`: `near = {x, y[, r]}` -> found, the values (r 1,000,000 when it is not an integer).
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_opt_near(
    state: *mut c_void,
    table: c_int,
    x: *mut c_long,
    y: *mut c_long,
    r: *mut c_long,
) -> bool {
    let l = state as *mut lua_State;
    if lua_type(l, table) != LUA_TTABLE {
        return false;
    }
    lua_getfield(l, table, c"near".as_ptr());
    let mut ok = false;
    if lua_type(l, -1) == LUA_TTABLE {
        let t = lua_gettop(l);
        lua_rawgeti(l, t, 1);
        lua_rawgeti(l, t, 2);
        lua_rawgeti(l, t, 3);
        if is_integer(l, -3) && is_integer(l, -2) {
            *x = lua_tointeger(l, -3) as c_long;
            *y = lua_tointeger(l, -2) as c_long;
            *r = if is_integer(l, -1) { lua_tointeger(l, -1) as c_long } else { 1_000_000 };
            ok = true;
        }
        lua_pop(l, 3);
    }
    lua_pop(l, 1);
    ok
}

/// `EECPUnitsOptAmount`: an integer field, 0 when missing or negative (as a `long`).
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_opt_amount(state: *mut c_void, table: c_int, key: *const c_char) -> c_long {
    let l = state as *mut lua_State;
    if lua_type(l, table) != LUA_TTABLE {
        return 0;
    }
    lua_getfield(l, table, key);
    let v = if is_integer(l, -1) { lua_tointeger(l, -1) as c_long } else { 0 };
    lua_pop(l, 1);
    v.max(0)
}

/// `EECPUnitsSetInt`: `t[key] = value` on the table on top.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_set_int(state: *mut c_void, key: *const c_char, value: c_long) {
    let l = state as *mut lua_State;
    lua_pushinteger(l, value as lua_Integer);
    lua_setfield(l, -2, key);
}

/// `EECPUnitsSetStr`.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_set_str(state: *mut c_void, key: *const c_char, value: *const c_char) {
    let l = state as *mut lua_State;
    lua_pushstring(l, value);
    lua_setfield(l, -2, key);
}

/// `EECPUnitsSetBool`.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_set_bool(state: *mut c_void, key: *const c_char, value: bool) {
    let l = state as *mut lua_State;
    lua_pushboolean(l, value as c_int);
    lua_setfield(l, -2, key);
}

/// `EECPUnitsPushSorted`: sorts the pairs in place (by key, then id) and pushes the first `limit` ids (negative:
/// all) as a Lua array.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_push_sorted(
    state: *mut c_void,
    entries: *mut EeAiUnitsSortEntry,
    count: usize,
    limit: c_long,
) {
    let l = state as *mut lua_State;
    if count > 0 {
        std::slice::from_raw_parts_mut(entries, count).sort_unstable();
    }
    let mut n = count as c_long;
    if limit >= 0 && n > limit {
        n = limit;
    }
    lua_createtable(l, n as c_int, 0);
    for k in 0..n.max(0) as usize {
        lua_pushinteger(l, (*entries.add(k)).id as lua_Integer);
        lua_rawseti(l, -2, k as lua_Integer + 1);
    }
}

/// `EECPUnitsResult`: a command's result: "ok"/"refused", the reason's name, the count, the id (nil for 0) -> 4.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_result(
    state: *mut c_void,
    reason: c_long,
    count: c_long,
    id: c_long,
) -> c_int {
    let l = state as *mut lua_State;
    lua_pushstring(l, if reason == 0 { c"ok".as_ptr() } else { c"refused".as_ptr() });
    lua_pushstring(l, REASON_NAMES[reason as usize].as_ptr());
    lua_pushinteger(l, count as lua_Integer);
    if id != 0 {
        lua_pushinteger(l, id as lua_Integer);
    } else {
        lua_pushnil(l);
    }
    4
}

/// `EECPUnitsLayerArg`: the layer an optional string argument names -> 0 threat (also the default: not a string),
/// 1 lost, 2 influence, -1 unknown.
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_layer_arg(state: *mut c_void, index: c_int) -> c_int {
    let l = state as *mut lua_State;
    if lua_type(l, index) != LUA_TSTRING {
        return 0;
    }
    match CStr::from_ptr(lua_tostring(l, index)).to_bytes() {
        b"threat" => 0,
        b"lost" => 1,
        b"influence" => 2,
        _ => -1,
    }
}

unsafe fn opt_flag(l: *mut lua_State, table: c_int, key: &CStr, out: &mut c_int) {
    lua_getfield(l, table, key.as_ptr());
    if !lua_isnil(l, -1) {
        *out = (lua_toboolean(l, -1) != 0) as c_int;
    }
    lua_pop(l, 1);
}

/// `EECPUnitsReadFilter`: the filter table at `table` into `f` (the fields it names; the rest as they were).
#[no_mangle]
pub unsafe extern "C-unwind" fn ee_ai_units_read_filter(
    state: *mut c_void,
    table: c_int,
    f: *mut EeAiUnitsFilter,
    by_name: EeAiUnitsClassByName,
) {
    let l = state as *mut lua_State;
    if lua_type(l, table) != LUA_TTABLE {
        return;
    }
    let f = &mut *f;
    lua_getfield(l, table, c"type".as_ptr());
    if lua_type(l, -1) == LUA_TTABLE {
        // a list of class names (or ids): any of them; the unknown ones are left out (none known: nothing)
        let list = lua_gettop(l);
        let length = lua_rawlen(l, list) as lua_Integer;
        let mut k: lua_Integer = 1;
        while k <= length && (f.type_count as usize) < MAX_TYPES {
            lua_rawgeti(l, list, k);
            let t = ee_ai_units_class_arg(l as *mut c_void, lua_gettop(l), by_name);
            lua_pop(l, 1);
            if t >= 0 {
                f.types[f.type_count as usize] = t;
                f.type_count += 1;
            }
            k += 1;
        }
        f.kind_type = -3;
    } else {
        if !lua_isnil(l, -1) {
            f.kind_type = ee_ai_units_class_arg(l as *mut c_void, lua_gettop(l), by_name);
        }
        if !lua_isnil(l, -1) && f.kind_type < 0 {
            f.kind_type = -2; // an unknown name: matches nothing
        }
    }
    lua_pop(l, 1);
    f.category = ee_ai_units_opt_string(l as *mut c_void, table, c"category".as_ptr());
    f.has_near = ee_ai_units_opt_near(l as *mut c_void, table, &mut f.x, &mut f.y, &mut f.r);
    opt_flag(l, table, c"idle", &mut f.idle);
    opt_flag(l, table, c"built", &mut f.built);
    opt_flag(l, table, c"armed", &mut f.armed);
    opt_flag(l, table, c"enemy", &mut f.enemy);
    f.held = ee_ai_units_opt_string(l as *mut c_void, table, c"held".as_ptr());
    f.action = ee_ai_units_opt_string(l as *mut c_void, table, c"action".as_ptr());
    ee_ai_units_opt_int(l as *mut c_void, table, c"player".as_ptr(), &mut f.player);
    ee_ai_units_opt_int(l as *mut c_void, table, c"limit".as_ptr(), &mut f.limit);
    let kind = ee_ai_units_opt_string(l as *mut c_void, table, c"kind".as_ptr());
    if !kind.is_null() {
        let name = CStr::from_ptr(kind).to_bytes();
        for (k, known) in [&b"food"[..], b"wood", b"stone", b"gold", b"iron"].iter().enumerate() {
            if name == *known {
                f.kind = k as c_long;
            }
        }
        if f.kind < 0 {
            f.kind = 99;
        }
    }
}

#[cfg(test)]
mod tests {
    //! golden_units_lua.txt: what the scratch driver over the unmodified helpers printed (build/ai3_golden, git
    //! c265af97), replayed here on the Rust helpers over the vendored Lua (lua_host's test link).
    use super::*;
    use std::cell::RefCell;
    use std::fmt::Write;

    thread_local! {
        static OUT: RefCell<String> = const { RefCell::new(String::new()) };
        static BYTES: RefCell<usize> = const { RefCell::new(0) };
        static IDS: RefCell<Vec<c_long>> = RefCell::new(vec![0; MAX_IDS]);
    }

    fn out(s: &str) {
        OUT.with(|o| o.borrow_mut().push_str(s));
    }

    fn bytes() -> usize {
        BYTES.with(|b| *b.borrow())
    }

    unsafe extern "C" fn alloc(_: *mut c_void, block: *mut c_void, old: usize, new: usize) -> *mut c_void {
        let old = if block.is_null() { 0 } else { old };
        if new == 0 {
            libc::free(block);
            BYTES.with(|b| *b.borrow_mut() -= old);
            return std::ptr::null_mut();
        }
        let p = libc::realloc(block, new);
        if !p.is_null() {
            BYTES.with(|b| {
                let mut b = b.borrow_mut();
                *b = *b - old + new;
            });
        }
        p
    }

    unsafe extern "C" fn class_by_name(name: *const c_char) -> c_long {
        match CStr::from_ptr(name).to_bytes() {
            b"clubman" => 3,
            b"citizen" => 1,
            b"zero" => 0,
            _ => -1,
        }
    }

    fn text(p: *const c_char) -> String {
        if p.is_null() {
            "(null)".into()
        } else {
            unsafe { CStr::from_ptr(p).to_string_lossy().into_owned() }
        }
    }

    fn number(v: f64) -> String {
        if v.fract() == 0.0 {
            format!("{}", v as i64)
        } else {
            format!("{v}")
        }
    }

    unsafe fn value(l: *mut lua_State, i: c_int) -> String {
        match lua_type(l, i) {
            LUA_TNIL => "nil".into(),
            LUA_TBOOLEAN => (if lua_toboolean(l, i) != 0 { "true" } else { "false" }).into(),
            LUA_TNUMBER if lua_isinteger(l, i) != 0 => lua_tointeger(l, i).to_string(),
            LUA_TNUMBER => number(lua_tonumber(l, i)),
            LUA_TSTRING => format!("\"{}\"", text(lua_tostring(l, i))),
            LUA_TTABLE => {
                let mut s = String::from("{");
                let n = lua_rawlen(l, i) as lua_Integer;
                for k in 1..=n {
                    lua_rawgeti(l, i, k);
                    s += &value(l, lua_gettop(l));
                    s += ",";
                    lua_pop(l, 1);
                }
                let mut keys = Vec::new();
                lua_pushnil(l);
                while lua_next(l, i) != 0 {
                    lua_pop(l, 1);
                    if lua_type(l, -1) == LUA_TSTRING {
                        keys.push(CStr::from_ptr(lua_tostring(l, -1)).to_owned());
                    }
                }
                keys.sort();
                for k in keys {
                    lua_getfield(l, i, k.as_ptr());
                    s += &format!("{}=", k.to_string_lossy());
                    s += &value(l, lua_gettop(l));
                    s += ",";
                    lua_pop(l, 1);
                }
                s + "}"
            }
            t => text(lua_typename(l, t)),
        }
    }

    fn hash(v: &[c_long]) -> u64 {
        let mut h: u64 = 1469598103934665603;
        for &x in v {
            h ^= x as i64 as u64;
            h = h.wrapping_mul(1099511628211);
        }
        h
    }

    unsafe extern "C-unwind" fn run_helper(l: *mut lua_State) -> c_int {
        let h = lua_tointeger(l, lua_upvalueindex(1));
        let mut s = String::new();
        match h {
            0 => {
                let mut n = 0usize;
                let bad =
                    IDS.with(|ids| ee_ai_units_read_ids(l as *mut c_void, 1, ids.borrow_mut().as_mut_ptr(), &mut n));
                let ids: Vec<c_long> = IDS.with(|ids| ids.borrow()[..n].to_vec());
                write!(s, "ids bad={bad} n={n}").unwrap();
                for x in ids.iter().take(6) {
                    write!(s, " {x}").unwrap();
                }
                write!(s, " hash={}", hash(&ids)).unwrap();
            }
            1 => {
                // printed as it goes, as the driver: a raising __index leaves the line cut
                out("optint");
                for k in [c"x", c"limit", c"near", c"player", c"food"] {
                    let mut v: c_long = -77;
                    let f = ee_ai_units_opt_int(l as *mut c_void, 1, k.as_ptr(), &mut v);
                    out(&format!(" {}={}/{v}", k.to_str().unwrap(), f as i32));
                }
            }
            2 => {
                // the driver's printf arguments, evaluated right to left (clang-cl)
                let d = ee_ai_units_opt_bool(l as *mut c_void, 1, c"idle".as_ptr(), false) as i32;
                let c = ee_ai_units_opt_bool(l as *mut c_void, 1, c"idle".as_ptr(), true) as i32;
                let b = ee_ai_units_opt_bool(l as *mut c_void, 1, c"flag".as_ptr(), false) as i32;
                let a = ee_ai_units_opt_bool(l as *mut c_void, 1, c"flag".as_ptr(), true) as i32;
                write!(s, "optbool flag={a}/{b} idle={c}/{d}").unwrap();
            }
            3 => {
                let a = text(ee_ai_units_opt_string(l as *mut c_void, 1, c"name".as_ptr()));
                let b = text(ee_ai_units_opt_string(l as *mut c_void, 1, c"category".as_ptr()));
                write!(s, "optstring name={a} category={b}").unwrap();
            }
            4 => {
                let (mut x, mut y, mut r) = (-1, -1, -1);
                let ok = ee_ai_units_opt_near(l as *mut c_void, 1, &mut x, &mut y, &mut r) as i32;
                write!(s, "optnear {ok} {x} {y} {r}").unwrap();
            }
            5 => {
                let c = ee_ai_units_opt_amount(l as *mut c_void, 1, c"x".as_ptr());
                let b = ee_ai_units_opt_amount(l as *mut c_void, 1, c"gold".as_ptr());
                let a = ee_ai_units_opt_amount(l as *mut c_void, 1, c"food".as_ptr());
                write!(s, "optamount food={a} gold={b} x={c}").unwrap();
            }
            6 => write!(s, "classarg {}", ee_ai_units_class_arg(l as *mut c_void, 1, class_by_name)).unwrap(),
            7 => write!(s, "layer {}", ee_ai_units_layer_arg(l as *mut c_void, 1)).unwrap(),
            _ => {
                let mut f = EeAiUnitsFilter {
                    kind_type: -1,
                    types: [0; MAX_TYPES],
                    type_count: 0,
                    category: std::ptr::null(),
                    has_near: false,
                    x: 0,
                    y: 0,
                    r: 0,
                    idle: -1,
                    built: -1,
                    armed: -1,
                    held: std::ptr::null(),
                    action: std::ptr::null(),
                    player: -1,
                    enemy: -1,
                    kind: -1,
                    limit: -1,
                };
                ee_ai_units_read_filter(l as *mut c_void, 1, &mut f, class_by_name);
                write!(s, "filter type={} types={}:", f.kind_type, f.type_count).unwrap();
                for t in &f.types[..f.type_count as usize] {
                    write!(s, "{t},").unwrap();
                }
                write!(
                    s,
                    " category={} near={} {} {} {} idle={} built={} armed={} held={} action={} player={} enemy={} \
                     kind={} limit={}",
                    text(f.category),
                    f.has_near as i32,
                    f.x,
                    f.y,
                    f.r,
                    f.idle,
                    f.built,
                    f.armed,
                    text(f.held),
                    text(f.action),
                    f.player,
                    f.enemy,
                    f.kind,
                    f.limit
                )
                .unwrap();
            }
        }
        out(&s);
        0
    }

    unsafe fn others(l: *mut lua_State) {
        let top = lua_gettop(l);
        for reason in 0..=10 {
            for id in [0, 5, -2] {
                let n = ee_ai_units_result(l as *mut c_void, reason, reason * 3 - 4, id);
                let mut s = format!("result {reason} {id} n={n}:");
                for k in top + 1..=lua_gettop(l) {
                    s += " ";
                    s += &value(l, k);
                }
                out(&format!("{s} top={} bytes={}\n", lua_gettop(l), bytes()));
                lua_settop(l, top);
            }
        }
        lua_createtable(l, 0, 0);
        ee_ai_units_set_int(l as *mut c_void, c"a".as_ptr(), 5);
        ee_ai_units_set_int(l as *mut c_void, c"b".as_ptr(), -2147483648);
        ee_ai_units_set_str(l as *mut c_void, c"s".as_ptr(), c"text".as_ptr());
        ee_ai_units_set_str(l as *mut c_void, c"e".as_ptr(), c"".as_ptr());
        ee_ai_units_set_bool(l as *mut c_void, c"t".as_ptr(), true);
        ee_ai_units_set_bool(l as *mut c_void, c"f".as_ptr(), false);
        ee_ai_units_set_int(l as *mut c_void, c"a".as_ptr(), 6);
        let v = value(l, lua_gettop(l));
        out(&format!("set {v} top={} bytes={}\n", lua_gettop(l), bytes()));
        lua_settop(l, top);
        let mut g: u64 = 12345;
        for size in [0, 1, 2, 5, 40, 1000] {
            for limit in [-1, 0, 3, 100000] {
                let mut sorted = Vec::new();
                for _ in 0..size {
                    g = g.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                    let key = ((g >> 33) % 7) as i64 - 3;
                    g = g.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                    let id = ((g >> 33) % 50000) as c_long - 100;
                    sorted.push(EeAiUnitsSortEntry { key, id });
                }
                ee_ai_units_push_sorted(l as *mut c_void, sorted.as_mut_ptr(), sorted.len(), limit);
                let n = lua_rawlen(l, -1) as lua_Integer;
                let mut v = Vec::new();
                for k in 1..=n {
                    lua_rawgeti(l, -1, k);
                    v.push(lua_tointeger(l, -1) as c_long);
                    lua_pop(l, 1);
                }
                out(&format!(
                    "sorted {size} {limit}: n={n} hash={} first={} top={} bytes={}\n",
                    hash(&v),
                    v.first().copied().unwrap_or(0),
                    lua_gettop(l),
                    bytes()
                ));
                lua_settop(l, top);
            }
        }
    }

    #[test]
    fn helpers_as_the_cpp() {
        let cases = include_str!("golden_units_lua_cases.lua");
        unsafe {
            let l = lua_newstate(alloc, std::ptr::null_mut());
            luaL_requiref(l, c"_G".as_ptr(), luaopen_base, 1);
            luaL_requiref(l, c"table".as_ptr(), luaopen_table, 1);
            luaL_requiref(l, c"string".as_ptr(), luaopen_string, 1);
            luaL_requiref(l, c"math".as_ptr(), luaopen_math, 1);
            lua_settop(l, 0);
            let st =
                luaL_loadbufferx(l, cases.as_ptr() as *const c_char, cases.len(), c"=cases".as_ptr(), c"t".as_ptr());
            assert_eq!(st, LUA_OK);
            assert_eq!(lua_pcall(l, 0, 1, 0), LUA_OK);
            let all = lua_gettop(l);
            let n = lua_rawlen(l, all) as lua_Integer;
            for c in 1..=n {
                lua_rawgeti(l, all, c);
                let v = value(l, lua_gettop(l));
                out(&format!("case {c}: {v}\n"));
                let at = lua_gettop(l);
                for h in 0..=8 {
                    lua_pushinteger(l, h);
                    lua_pushcclosure(l, run_helper, 1);
                    lua_pushvalue(l, at);
                    out("  ");
                    let st = lua_pcall(l, 1, 0, 0);
                    if st != LUA_OK {
                        out(&format!("ERROR {st} {}", text(lua_tostring(l, -1))));
                        lua_pop(l, 1);
                    }
                    out(&format!(" top={} bytes={}\n", lua_gettop(l), bytes()));
                }
                lua_settop(l, all);
            }
            lua_settop(l, 0);
            others(l);
            out(&format!("end bytes={}\n", bytes()));
            lua_close(l);
        }
        let got = OUT.with(|o| o.borrow().clone());
        let want = include_str!("golden_units_lua.txt");
        for (i, (g, w)) in got.lines().zip(want.lines()).enumerate() {
            assert_eq!(g, w, "line {}", i + 1);
        }
        assert_eq!(got.lines().count(), want.lines().count());
    }
}
