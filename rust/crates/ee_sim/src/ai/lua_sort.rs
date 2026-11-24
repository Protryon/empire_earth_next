//! `table.sort` with a fixed pivot choice: Lua 5.5.1's quicksort (`ltablib.c`), statement for statement, with
//! `l_randomizePivot` a constant.
//!
//! Lua's own picks a new "random" pivot seed (`luaL_makeseed`: the time and addresses) after an unbalanced
//! partition of more than 100 elements, which changes the order of elements that compare equal from run to run and
//! from peer to peer. The vendored Lua (mlua-sys, lua-src) is compiled unmodified, so the host installs this in the
//! place of the library's function (`lua_host::setup`). `~0` is the constant ltablib.c itself suggests.
//!
//! As every function Lua calls here: an error raised by the comparison or a metamethod crosses these frames by
//! longjmp, so they hold no value with a destructor.
#![allow(non_snake_case)]

use super::lua_api::*;
use std::ffi::c_int;

type Idx = u32;
const RANDOMIZE_PIVOT: u32 = !0;
/// arrays larger than this may use "randomized" pivots
const RANLIMIT: Idx = 100;

unsafe fn geti(l: *mut lua_State, i: Idx) {
    lua_geti(l, 1, i as lua_Integer);
}

unsafe fn set2(l: *mut lua_State, i: Idx, j: Idx) {
    lua_seti(l, 1, i as lua_Integer);
    lua_seti(l, 1, j as lua_Integer);
}

/// Whether the value at `a` is less than the one at `b` in the sort's order.
unsafe fn sort_comp(l: *mut lua_State, a: c_int, b: c_int) -> bool {
    if lua_isnil(l, 2) {
        lua_compare(l, a, b, LUA_OPLT) != 0
    } else {
        lua_pushvalue(l, 2);
        lua_pushvalue(l, a - 1); // -1 to compensate the function
        lua_pushvalue(l, b - 2); // -2 to compensate the function and a
        lua_call(l, 2, 1);
        let res = lua_toboolean(l, -1) != 0;
        lua_pop(l, 1);
        res
    }
}

/// The pivot P is at the top of the stack; a[lo] <= P == a[up-1] <= a[up] -> i with a[lo..i-1] <= a[i] == P <=
/// a[i+1..up].
unsafe fn partition(l: *mut lua_State, lo: Idx, up: Idx) -> Idx {
    let mut i = lo;
    let mut j = up - 1;
    loop {
        loop {
            i = i.wrapping_add(1);
            geti(l, i);
            if !sort_comp(l, -1, -2) {
                break;
            }
            if i == up - 1 {
                luaL_error(l, c"invalid order function for sorting".as_ptr());
            }
            lua_pop(l, 1);
        }
        loop {
            j = j.wrapping_sub(1);
            geti(l, j);
            if !sort_comp(l, -3, -1) {
                break;
            }
            if j < i {
                luaL_error(l, c"invalid order function for sorting".as_ptr());
            }
            lua_pop(l, 1);
        }
        if j < i {
            lua_pop(l, 1);
            set2(l, up - 1, i);
            return i;
        }
        set2(l, i, j);
    }
}

fn choose_pivot(lo: Idx, up: Idx, rnd: u32) -> Idx {
    let r4 = (up - lo) / 4;
    (rnd ^ lo ^ up) % (r4 * 2) + (lo + r4)
}

unsafe fn auxsort(l: *mut lua_State, mut lo: Idx, mut up: Idx, mut rnd: u32) {
    while lo < up {
        geti(l, lo);
        geti(l, up);
        if sort_comp(l, -1, -2) {
            set2(l, lo, up);
        } else {
            lua_pop(l, 2);
        }
        if up - lo == 1 {
            return;
        }
        let mut p = if up - lo < RANLIMIT || rnd == 0 { (lo + up) / 2 } else { choose_pivot(lo, up, rnd) };
        geti(l, p);
        geti(l, lo);
        if sort_comp(l, -2, -1) {
            set2(l, p, lo);
        } else {
            lua_pop(l, 1);
            geti(l, up);
            if sort_comp(l, -1, -2) {
                set2(l, p, up);
            } else {
                lua_pop(l, 2);
            }
        }
        if up - lo == 2 {
            return;
        }
        geti(l, p);
        lua_pushvalue(l, -1);
        geti(l, up - 1);
        set2(l, p, up - 1);
        p = partition(l, lo, up);
        let n;
        if p - lo < up - p {
            auxsort(l, lo, p - 1, rnd);
            n = p - lo;
            lo = p + 1;
        } else {
            auxsort(l, p + 1, up, rnd);
            n = up - p;
            up = p - 1;
        }
        // (up < lo here when the rest is empty: C's unsigned arithmetic wraps the same way)
        if up.wrapping_sub(lo) / 128 > n {
            rnd = RANDOMIZE_PIVOT;
        }
    }
}

/// ltablib.c's `checktab(L, 1, TAB_RW | TAB_L)`.
unsafe fn check_tab(l: *mut lua_State) {
    if lua_type(l, 1) == LUA_TTABLE {
        return;
    }
    let mut n = 1;
    let mut ok = lua_getmetatable(l, 1) != 0;
    for key in [c"__index", c"__newindex", c"__len"] {
        if !ok {
            break;
        }
        lua_pushstring(l, key.as_ptr());
        n += 1;
        ok = lua_rawget(l, -n) != LUA_TNIL;
    }
    if ok {
        lua_pop(l, n);
    } else {
        luaL_checktype(l, 1, LUA_TTABLE);
    }
}

/// `table.sort(t [, less])`. Safety: Lua calls it.
pub(crate) unsafe extern "C-unwind" fn lua_table_sort(l: *mut lua_State) -> c_int {
    check_tab(l);
    let n = luaL_len(l, 1);
    if n > 1 {
        if n >= c_int::MAX as lua_Integer {
            return luaL_argerror(l, 1, c"array too big".as_ptr());
        }
        if lua_type(l, 2) > LUA_TNIL {
            luaL_checktype(l, 2, LUA_TFUNCTION);
        }
        lua_settop(l, 2);
        auxsort(l, 1, n as Idx, 0);
    }
    0
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::ffi::{c_char, c_void, CStr};

    unsafe extern "C" fn alloc(_: *mut c_void, block: *mut c_void, _: usize, new: usize) -> *mut c_void {
        if new == 0 {
            libc::free(block);
            return std::ptr::null_mut();
        }
        libc::realloc(block, new)
    }

    /// Runs a chunk with `sort` (this module's) and `table.sort` (the library's) -> its result's text.
    unsafe fn run(chunk: &str) -> String {
        let l = lua_newstate(alloc, std::ptr::null_mut(), 0x5eed_5eed);
        luaL_requiref(l, c"_G".as_ptr(), luaopen_base, 1);
        luaL_requiref(l, c"table".as_ptr(), luaopen_table, 1);
        luaL_requiref(l, c"string".as_ptr(), luaopen_string, 1);
        lua_settop(l, 0);
        lua_pushglobaltable(l);
        lua_pushcfunction(l, lua_table_sort);
        lua_setfield(l, 1, c"sort".as_ptr());
        lua_settop(l, 0);
        let st = luaL_loadbufferx(l, chunk.as_ptr() as *const c_char, chunk.len(), c"=t".as_ptr(), c"t".as_ptr());
        assert_eq!(st, LUA_OK, "{}", CStr::from_ptr(lua_tostring(l, -1)).to_string_lossy());
        lua_pcall(l, 0, 1, 0);
        let text = CStr::from_ptr(luaL_tolstring(l, -1, std::ptr::null_mut())).to_string_lossy().into_owned();
        lua_close(l);
        text
    }

    /// Distinct elements have one sorted order, whatever the pivots: the port must give the library's arrays, for
    /// shapes that make the partitions unbalanced too (the library then draws its pivots from the time).
    #[test]
    fn sorts_as_the_library() {
        let chunk = r#"
            local g = 12345
            local function rnd(n) g = (g * 1103515245 + 12345) % 2147483648; return g % n end
            local function shapes(n)
                local up, down, pipe, mixed, seen = {}, {}, {}, {}, {}
                for i = 1, n do
                    up[i] = i; down[i] = n - i; pipe[i] = (i <= n // 2) and 2 * i or 2 * (n - i) + 1
                    local v = rnd(1000000)
                    while seen[v] do v = v + 1 end
                    seen[v] = true; mixed[i] = v
                end
                return {up, down, pipe, mixed}
            end
            local checked = 0
            for _, n in ipairs({0, 1, 2, 3, 4, 5, 17, 99, 100, 101, 128, 300, 1000, 5000}) do
                for _, less in ipairs({false, function(a, b) return a < b end, function(a, b) return a > b end}) do
                    for _, t in ipairs(shapes(n)) do
                        local a, b = {}, {}
                        for i = 1, n do a[i] = t[i]; b[i] = t[i] end
                        if less then sort(a, less); table.sort(b, less) else sort(a); table.sort(b) end
                        for i = 1, n do
                            if a[i] ~= b[i] then return "differs at " .. n .. " " .. i end
                            if i > 1 and less and less(a[i], a[i - 1]) then return "not sorted " .. n end
                        end
                        checked = checked + 1
                    end
                end
            end
            return "ok " .. checked
        "#;
        assert_eq!(unsafe { run(chunk) }, "ok 168");
    }

    /// Equal elements: the same array in every run (a fixed pivot), and the errors of the library's function.
    #[test]
    fn equal_keys_and_errors() {
        let chunk = r#"
            local t = {}
            for i = 1, 3000 do t[i] = {k = (i * 7919) % 5, i = i} end
            sort(t, function(a, b) return a.k < b.k end)
            local h = 0
            for i = 1, #t do h = (h * 31 + t[i].i) % 1000000007 end
            local e1 = select(2, pcall(sort, 5))
            local e2 = select(2, pcall(sort, {3, 2, 1}, 7))
            local e3 = select(2, pcall(sort, {3, "x", 1}))
            local e4 = select(2, pcall(sort, {5, 4, 3, 2, 1, 6, 7, 8}, function() return true end))
            return h .. "|" .. e1 .. "|" .. e2 .. "|" .. e3 .. "|" .. e4
        "#;
        let first = unsafe { run(chunk) };
        assert_eq!(first, unsafe { run(chunk) });
        let parts: Vec<&str> = first.split('|').collect();
        assert_eq!(parts[0], "544556980");
        assert!(parts[1].contains("table expected, got number"), "{first}");
        assert!(parts[2].contains("function expected, got number"), "{first}");
        assert!(parts[3].contains("attempt to compare"), "{first}");
        assert!(parts[4].contains("invalid order function for sorting"), "{first}");
    }
}
