//! The lobby's containers as Rust objects (round 17): every ordered map and set of LobbyModel (a client list's
//! clients, a server's rooms and address names, the server list, a game list's two maps) and of
//! LobbyPersistentData (users, user ids, ignored names, blocked keys, friends, the two option tables, reconnect
//! ids, LAN names, direct connects), and LobbyOptionSet's ordered values. One object type, [`EeWonLobbyMap`], an
//! ordered vector of entries under the original's comparator for its key kind:
//!
//! - an id (`unsigned short`, `unsigned long`; or `long`, signed), compared as the number;
//! - an `IPAddr` (its Rust value, ipobj.rs), compared as `operator<` compared the six wire bytes;
//! - a `GUIString`'s `wchar_t` units, ignoring case (`GUIStringLessNoCase`: `CompareNoCase`, the CRT's `towupper`)
//!   or exactly (`GUIString::operator<`: `Compare`);
//! - narrow text ignoring case (`StringLessNoCase`: `StringCompareNoCase`, strcase.rs);
//! - LobbyOptionSet's values: unique by `Compare`, in insertion order.
//!
//! As in a `std::map`, a key already present keeps its first spelling. An entry's value is what its container held:
//! a C++ object pointer (a `SmartPtr`'s reference or an owned record; the map's hooks copy it when the map is copied
//! and drop it when the entry goes, the C++ AddRef / Release or new / delete), two numbers and a text. Hooks run after
//! the map's borrow ends (a Release may destroy an object whose destructor reaches another map). The C++ wrappers
//! (`WONLobbyMap.h`) give the classes their old operations over it; positions are indices into the order.
//! The behaviour is `golden_lobbymap.txt`, recorded over the original's std containers and comparators.

use std::cmp::Ordering;
use std::ffi::c_void;

use crate::chat::CrtChat;
use crate::guistring;
use crate::ipobj::EeWonIpAddr;
use crate::strcase;

/// A container's key kind (its comparator).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyKeyKind {
    /// `unsigned short` / `unsigned long` ids
    Unsigned = 0,
    /// `long` ids
    Signed = 1,
    /// `IPAddr`
    Addr = 2,
    /// `GUIString`, `GUIStringLessNoCase`
    NoCase = 3,
    /// `GUIString`, `operator<`
    Text = 4,
    /// `std::string`, `StringLessNoCase`
    NarrowNoCase = 5,
    /// LobbyOptionSet: `GUIString` values unique by `Compare`, in insertion order
    InOrder = 6,
}

/// A key handed over by C++: `num` for the ids (an `unsigned` id zero-extended, a `long` sign-extended), `addr` for
/// an address, `text` / `count` / `unit_size` for text (`count` units of `unit_size` bytes: a `wchar_t`'s size for a
/// `GUIString`, 1 for narrow text).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeWonLobbyKey {
    pub num: u64,
    pub addr: *const EeWonIpAddr,
    pub text: *const c_void,
    pub count: usize,
    pub unit_size: u32,
}

/// What a map does with its entries' objects: `copy` makes the copy's object (a reference added, or a new record),
/// `drop` lets one go (a reference released, or the record deleted). Either may be absent (the entries hold none).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonLobbyMapHooks {
    pub copy: Option<unsafe extern "C" fn(*mut c_void) -> *mut c_void>,
    pub drop: Option<unsafe extern "C" fn(*mut c_void)>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
enum Key {
    Num(u64),
    Addr(EeWonIpAddr),
    Units(Vec<u32>),
    Bytes(Vec<u8>),
}

#[derive(Debug)]
struct Entry {
    key: Key,
    obj: *mut c_void,
    num: [i64; 2],
    text: Vec<u32>,
}

impl Entry {
    fn new(key: Key) -> Entry {
        Entry { key, obj: std::ptr::null_mut(), num: [0; 2], text: Vec::new() }
    }
}

fn ordering(c: i32) -> Ordering {
    c.cmp(&0)
}

/// The kind's comparator.
fn compare(kind: EeWonLobbyKeyKind, a: &Key, b: &Key) -> Ordering {
    use EeWonLobbyKeyKind as K;
    match (kind, a, b) {
        (K::Unsigned, Key::Num(x), Key::Num(y)) => x.cmp(y),
        (K::Signed, Key::Num(x), Key::Num(y)) => (*x as i64).cmp(&(*y as i64)),
        (K::Addr, Key::Addr(x), Key::Addr(y)) => x.compare(y),
        (K::NoCase, Key::Units(x), Key::Units(y)) => ordering(guistring::compare_no_case(x, y, &CrtChat)),
        (K::Text | K::InOrder, Key::Units(x), Key::Units(y)) => ordering(guistring::compare(x, y)),
        (K::NarrowNoCase, Key::Bytes(x), Key::Bytes(y)) => ordering(strcase::compare_no_case(x, y)),
        _ => panic!("lobby map: a key of another kind"),
    }
}

/// An ordered container of the lobby (see the module).
#[derive(Debug)]
pub struct EeWonLobbyMap {
    kind: EeWonLobbyKeyKind,
    hooks: EeWonLobbyMapHooks,
    entries: Vec<Entry>,
}

impl EeWonLobbyMap {
    pub fn new(kind: EeWonLobbyKeyKind, hooks: EeWonLobbyMapHooks) -> EeWonLobbyMap {
        EeWonLobbyMap { kind, hooks, entries: Vec::new() }
    }

    pub fn len(&self) -> usize {
        self.entries.len()
    }
    pub fn is_empty(&self) -> bool {
        self.entries.is_empty()
    }

    /// The entry's position (`Ok`) or where it would go (`Err`); in insertion order, the end.
    fn search(&self, key: &Key) -> Result<usize, usize> {
        if self.kind == EeWonLobbyKeyKind::InOrder {
            return match self.entries.iter().position(|e| compare(self.kind, &e.key, key) == Ordering::Equal) {
                Some(i) => Ok(i),
                None => Err(self.entries.len()),
            };
        }
        self.entries.binary_search_by(|e| compare(self.kind, &e.key, key))
    }

    /// `find`: the position or `None`.
    fn find(&self, key: &Key) -> Option<usize> {
        self.search(key).ok()
    }
    /// `lower_bound`.
    fn lower_bound(&self, key: &Key) -> usize {
        self.entries.partition_point(|e| compare(self.kind, &e.key, key) == Ordering::Less)
    }
    /// `upper_bound`.
    fn upper_bound(&self, key: &Key) -> usize {
        self.entries.partition_point(|e| compare(self.kind, &e.key, key) != Ordering::Greater)
    }
    /// `insert(value_type(key, empty))` (and `operator[]`): the position, and whether it is new.
    fn insert(&mut self, key: Key) -> (usize, bool) {
        match self.search(&key) {
            Ok(i) => (i, false),
            Err(i) => {
                self.entries.insert(i, Entry::new(key));
                (i, true)
            }
        }
    }
    /// `erase(key)`: the object to drop, whether one went.
    #[must_use]
    fn erase(&mut self, key: &Key) -> (bool, Vec<*mut c_void>) {
        match self.find(key) {
            Some(i) => (true, self.erase_at(i)),
            None => (false, Vec::new()),
        }
    }
    #[must_use]
    fn erase_at(&mut self, i: usize) -> Vec<*mut c_void> {
        if i < self.entries.len() {
            vec![self.entries.remove(i).obj]
        } else {
            Vec::new()
        }
    }
    /// `erase(position, end())`.
    #[must_use]
    fn erase_from(&mut self, i: usize) -> Vec<*mut c_void> {
        if i < self.entries.len() {
            self.entries.drain(i..).map(|e| e.obj).collect()
        } else {
            Vec::new()
        }
    }
    #[must_use]
    fn clear(&mut self) -> Vec<*mut c_void> {
        self.erase_from(0)
    }

    /// Lets the objects go through the drop hook (after every borrow of a map ended).
    ///
    /// # Safety
    /// The objects are the map's, taken out.
    unsafe fn release(hooks: EeWonLobbyMapHooks, objs: Vec<*mut c_void>) {
        if let Some(d) = hooks.drop {
            for o in objs.into_iter().filter(|o| !o.is_null()) {
                // SAFETY: the hook's contract
                unsafe { d(o) };
            }
        }
    }
}

// ---- FFI

unsafe fn key_of(kind: EeWonLobbyKeyKind, k: *const EeWonLobbyKey) -> Key {
    // SAFETY: the caller's key
    let k = unsafe { &*k };
    use EeWonLobbyKeyKind as K;
    match kind {
        K::Unsigned | K::Signed => Key::Num(k.num),
        // SAFETY: a live value
        K::Addr => Key::Addr(unsafe { (*k.addr).clone() }),
        K::NarrowNoCase => Key::Bytes(if k.text.is_null() || k.count == 0 {
            Vec::new()
        } else {
            // SAFETY: `count` bytes
            unsafe { std::slice::from_raw_parts(k.text as *const u8, k.count) }.to_vec()
        }),
        // SAFETY: `count` units of `unit_size` bytes
        K::NoCase | K::Text | K::InOrder => Key::Units(unsafe { wide(k.text, k.count, k.unit_size) }),
    }
}

unsafe fn wide(text: *const c_void, count: usize, unit_size: u32) -> Vec<u32> {
    // SAFETY: the caller's contract
    unsafe { crate::chat::units(crate::chat::EeWonChatText { text, length: count }, unit_size) }
}

macro_rules! map {
    ($m:expr) => {
        // SAFETY: a live map (the caller's contract)
        unsafe { &*$m }
    };
}
macro_rules! map_mut {
    ($m:expr) => {
        // SAFETY: a live map, not otherwise borrowed during the call (the caller's contract)
        unsafe { &mut *$m }
    };
}

/// A new, empty container.
#[no_mangle]
pub extern "C" fn ee_won_lobbymap_new(kind: EeWonLobbyKeyKind, hooks: EeWonLobbyMapHooks) -> *mut EeWonLobbyMap {
    Box::into_raw(Box::new(EeWonLobbyMap::new(kind, hooks)))
}
/// Frees the container, dropping its objects.
///
/// # Safety
/// `m` from `ee_won_lobbymap_new` (or null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_free(m: *mut EeWonLobbyMap) {
    if m.is_null() {
        return;
    }
    // SAFETY: the caller's map
    let mut b = unsafe { Box::from_raw(m) };
    let objs = b.clear();
    let hooks = b.hooks;
    drop(b);
    unsafe { EeWonLobbyMap::release(hooks, objs) };
}
/// The number of entries.
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_len(m: *const EeWonLobbyMap) -> usize {
    map!(m).len()
}
/// `clear`.
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_clear(m: *mut EeWonLobbyMap) {
    let (objs, hooks) = {
        let m = map_mut!(m);
        (m.clear(), m.hooks)
    };
    unsafe { EeWonLobbyMap::release(hooks, objs) };
}
/// `find`: the entry's position, or -1.
///
/// # Safety
/// `m` is live, `key` a key of its kind.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_find(m: *const EeWonLobbyMap, key: *const EeWonLobbyKey) -> isize {
    let m = map!(m);
    m.find(&unsafe { key_of(m.kind, key) }).map_or(-1, |i| i as isize)
}
/// `lower_bound`'s position (the length: `end()`).
///
/// # Safety
/// As `ee_won_lobbymap_find`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_lower_bound(m: *const EeWonLobbyMap, key: *const EeWonLobbyKey) -> usize {
    let m = map!(m);
    m.lower_bound(&unsafe { key_of(m.kind, key) })
}
/// `upper_bound`'s position.
///
/// # Safety
/// As `ee_won_lobbymap_find`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_upper_bound(m: *const EeWonLobbyMap, key: *const EeWonLobbyKey) -> usize {
    let m = map!(m);
    m.upper_bound(&unsafe { key_of(m.kind, key) })
}
/// `insert(value_type(key, empty))`: the entry's position; `inserted` (or null) says whether it is new (an empty
/// entry: no object, numbers 0, no text). In insertion order a new value goes at the end.
///
/// # Safety
/// As `ee_won_lobbymap_find`; `inserted` null or writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_insert(
    m: *mut EeWonLobbyMap,
    key: *const EeWonLobbyKey,
    inserted: *mut bool,
) -> usize {
    let m = map_mut!(m);
    let (i, new) = m.insert(unsafe { key_of(m.kind, key) });
    if !inserted.is_null() {
        // SAFETY: writable (the caller's contract)
        unsafe { *inserted = new };
    }
    i
}
/// `erase(key)`: whether an entry went (its object dropped).
///
/// # Safety
/// As `ee_won_lobbymap_find`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_erase(m: *mut EeWonLobbyMap, key: *const EeWonLobbyKey) -> bool {
    let (gone, objs, hooks) = {
        let m = map_mut!(m);
        let key = unsafe { key_of(m.kind, key) };
        let (gone, objs) = m.erase(&key);
        (gone, objs, m.hooks)
    };
    unsafe { EeWonLobbyMap::release(hooks, objs) };
    gone
}
/// `erase(position)` (nothing past the end).
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_erase_at(m: *mut EeWonLobbyMap, i: usize) {
    let (objs, hooks) = {
        let m = map_mut!(m);
        (m.erase_at(i), m.hooks)
    };
    unsafe { EeWonLobbyMap::release(hooks, objs) };
}
/// `erase(position, end())`.
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_erase_from(m: *mut EeWonLobbyMap, i: usize) {
    let (objs, hooks) = {
        let m = map_mut!(m);
        (m.erase_from(i), m.hooks)
    };
    unsafe { EeWonLobbyMap::release(hooks, objs) };
}
/// The container's assignment (`to = from`): `to`'s entries dropped, `from`'s copied (each object through the copy
/// hook).
///
/// # Safety
/// Both live, of the same kind and hooks; they may be the same map.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_assign(to: *mut EeWonLobbyMap, from: *const EeWonLobbyMap) {
    if std::ptr::eq(to, from) {
        return;
    }
    let (keys, objs, hooks) = {
        let f = map!(from);
        let keys: Vec<(Key, [i64; 2], Vec<u32>)> =
            f.entries.iter().map(|e| (e.key.clone(), e.num, e.text.clone())).collect();
        (keys, f.entries.iter().map(|e| e.obj).collect::<Vec<_>>(), f.hooks)
    };
    let copies: Vec<*mut c_void> = objs
        .into_iter()
        .map(|o| match hooks.copy {
            // SAFETY: the hook's contract
            Some(c) if !o.is_null() => unsafe { c(o) },
            _ => o,
        })
        .collect();
    let (old, to_hooks) = {
        let t = map_mut!(to);
        let old = t.clear();
        t.entries = keys.into_iter().zip(copies).map(|((key, num, text), obj)| Entry { key, obj, num, text }).collect();
        (old, t.hooks)
    };
    unsafe { EeWonLobbyMap::release(to_hooks, old) };
}
/// The entry's object (null when none or past the end).
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_obj(m: *const EeWonLobbyMap, i: usize) -> *mut c_void {
    map!(m).entries.get(i).map_or(std::ptr::null_mut(), |e| e.obj)
}
/// Sets the entry's object (the map owns it from then on: a reference the caller added, or a record it made); the
/// old one is dropped.
///
/// # Safety
/// `m` is live, `i` a position.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_set_obj(m: *mut EeWonLobbyMap, i: usize, obj: *mut c_void) {
    let (old, hooks) = {
        let m = map_mut!(m);
        let Some(e) = m.entries.get_mut(i) else {
            return;
        };
        (std::mem::replace(&mut e.obj, obj), m.hooks)
    };
    unsafe { EeWonLobbyMap::release(hooks, vec![old]) };
}
/// One of the entry's two numbers (0 past the end).
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_num(m: *const EeWonLobbyMap, i: usize, slot: usize) -> i64 {
    map!(m).entries.get(i).map_or(0, |e| e.num[slot & 1])
}
/// Sets one of the entry's numbers.
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_set_num(m: *mut EeWonLobbyMap, i: usize, slot: usize, value: i64) {
    if let Some(e) = map_mut!(m).entries.get_mut(i) {
        e.num[slot & 1] = value;
    }
}
/// The entry's text (units as `u32`; `count` gets their number), valid until the map changes.
///
/// # Safety
/// `m` is live, `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_text(m: *const EeWonLobbyMap, i: usize, count: *mut usize) -> *const u32 {
    let t: &[u32] = map!(m).entries.get(i).map_or(&[], |e| &e.text);
    // SAFETY: writable (the caller's contract)
    unsafe { *count = t.len() };
    t.as_ptr()
}
/// Sets the entry's text (`count` units of `unit_size` bytes).
///
/// # Safety
/// `m` is live; `text` holds the units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_set_text(
    m: *mut EeWonLobbyMap,
    i: usize,
    text: *const c_void,
    count: usize,
    unit_size: u32,
) {
    let t = unsafe { wide(text, count, unit_size) };
    if let Some(e) = map_mut!(m).entries.get_mut(i) {
        e.text = t;
    }
}
/// The entry's key as a number (an id map; 0 otherwise or past the end).
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_key_num(m: *const EeWonLobbyMap, i: usize) -> u64 {
    match map!(m).entries.get(i).map(|e| &e.key) {
        Some(Key::Num(n)) => *n,
        _ => 0,
    }
}
/// The entry's key as an address (an address map; null otherwise), valid until the map changes.
///
/// # Safety
/// `m` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_key_addr(m: *const EeWonLobbyMap, i: usize) -> *const EeWonIpAddr {
    match map!(m).entries.get(i).map(|e| &e.key) {
        Some(Key::Addr(a)) => a,
        _ => std::ptr::null(),
    }
}
/// The entry's key as text: `u32` units of a `GUIString` key, or the bytes of a narrow one (as `u8`), and their
/// count; valid until the map changes.
///
/// # Safety
/// `m` is live, `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbymap_key_text(
    m: *const EeWonLobbyMap,
    i: usize,
    count: *mut usize,
) -> *const c_void {
    let (p, n) = match map!(m).entries.get(i).map(|e| &e.key) {
        Some(Key::Units(u)) => (u.as_ptr() as *const c_void, u.len()),
        Some(Key::Bytes(b)) => (b.as_ptr() as *const c_void, b.len()),
        _ => (std::ptr::null(), 0),
    };
    // SAFETY: writable (the caller's contract)
    unsafe { *count = n };
    p
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::ipaddr::Addr;

    // golden_lobbymap.txt's candidates (won_model_test's lists)
    const IDS: [u64; 12] = [0, 1, 2, 3, 7, 10, 0xfffe, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xffffffff];
    const SIGNED: [i64; 10] = [0, 1, -1, 2, -2, 100, -100, 0x7fffffff, -0x7fffffff - 1, 5];
    const HOSTS: [u32; 8] = [0x7f000001, 0x0a000001, 0xc0a80105, 0x0a000001, 0x7f000001, 0, 0xffffffff, 0x01020304];
    const PORTS: [u16; 8] = [8585, 8585, 15100, 15101, 1, 0, 65535, 0x0100];
    const WIDE: [&str; 17] = [
        "Alpha",
        "alpha",
        "ALPHA",
        "Beta",
        "beta2",
        "",
        "Gamma",
        "gAmma",
        "Zed",
        "a",
        "A",
        "_x",
        "[x",
        "zz",
        "Alph",
        "Alpha ",
        "\u{e9}t\u{e9}",
    ];
    const NARROW: [&[u8]; 11] = [
        b"Server.won.net",
        b"server.WON.net",
        b"a",
        b"B",
        b"_c",
        b"[d",
        b"",
        b"Zz",
        b"\xc3\xa9",
        b"server.won.net2",
        b"b",
    ];

    fn kind_of(k: usize) -> EeWonLobbyKeyKind {
        use EeWonLobbyKeyKind as K;
        [K::Unsigned, K::Signed, K::Addr, K::NoCase, K::Text, K::NarrowNoCase][k]
    }
    fn key(kind: EeWonLobbyKeyKind, i: usize) -> Key {
        use EeWonLobbyKeyKind as K;
        match kind {
            K::Unsigned => Key::Num(IDS[i]),
            K::Signed => Key::Num(SIGNED[i] as u64),
            K::Addr => {
                let mut a = EeWonIpAddr::default();
                a.set_host_order(HOSTS[i], PORTS[i]);
                Key::Addr(a)
            }
            K::NoCase | K::Text | K::InOrder => Key::Units(WIDE[i].encode_utf16().map(u32::from).collect()),
            K::NarrowNoCase => Key::Bytes(NARROW[i].to_vec()),
        }
    }
    // the candidate a stored key is (its first spelling)
    fn index_of(kind: EeWonLobbyKeyKind, k: &Key) -> usize {
        let n = match kind {
            EeWonLobbyKeyKind::Unsigned => IDS.len(),
            EeWonLobbyKeyKind::Signed => SIGNED.len(),
            EeWonLobbyKeyKind::Addr => HOSTS.len(),
            EeWonLobbyKeyKind::NarrowNoCase => NARROW.len(),
            _ => WIDE.len(),
        };
        (0..n).find(|&i| key(kind, i) == *k).expect("a candidate")
    }
    fn dump(m: &EeWonLobbyMap) -> String {
        let mut s = format!("{}:", m.len());
        for e in &m.entries {
            s += &format!(" {}={}", index_of(m.kind, &e.key), e.num[0]);
        }
        s
    }
    fn pos(m: &EeWonLobbyMap, i: Option<usize>) -> String {
        match i {
            Some(i) if i < m.len() => i.to_string(),
            _ => "-1".into(),
        }
    }

    /// golden_lobbymap.txt, replayed: each step on the Rust container gives the line the std container gave.
    #[test]
    fn golden() {
        let golden = include_str!("golden_lobbymap.txt");
        let mut maps: std::collections::BTreeMap<String, (EeWonLobbyMap, EeWonLobbyMap)> = Default::default();
        let mut sets: std::collections::BTreeMap<String, EeWonLobbyMap> = Default::default();
        let mut lines = 0;
        for line in golden.lines() {
            let w: Vec<&str> = line.split(' ').collect();
            let got = if w[0] == "map" {
                let kind = kind_of(w[1].parse().unwrap());
                let id = format!("{} {}", w[1], w[2]);
                let (m, copy) = maps.entry(id).or_insert_with(|| {
                    (EeWonLobbyMap::new(kind, Default::default()), EeWonLobbyMap::new(kind, Default::default()))
                });
                let arg = |n: usize| -> usize { w[n].parse().unwrap() };
                let s = match w[3] {
                    "ins" => {
                        let (i, new) = m.insert(key(kind, arg(4)));
                        format!("ins {} -> {} {}", arg(4), i, new as i32)
                    }
                    "set" => {
                        let (i, _) = m.insert(key(kind, arg(4)));
                        m.entries[i].num[0] = w[5].parse().unwrap();
                        format!("set {} {} -> {}", arg(4), w[5], i)
                    }
                    "find" => format!("find {} -> {}", arg(4), pos(m, m.find(&key(kind, arg(4))))),
                    "erase" => {
                        let (gone, _) = m.erase(&key(kind, arg(4)));
                        format!("erase {} -> {}", arg(4), gone as i32)
                    }
                    "lb" => format!("lb {} -> {}", arg(4), pos(m, Some(m.lower_bound(&key(kind, arg(4)))))),
                    "ub" => format!("ub {} -> {}", arg(4), pos(m, Some(m.upper_bound(&key(kind, arg(4)))))),
                    "eraseat" if w[4] == "none" => "eraseat none".into(),
                    "eraseat" => {
                        let _ = m.erase_at(arg(4));
                        format!("eraseat {}", arg(4))
                    }
                    "erasefrom" => {
                        let _ = m.erase_from(arg(4));
                        format!("erasefrom {}", arg(4))
                    }
                    "dump" => "dump".into(),
                    "clear" => {
                        let _ = m.clear();
                        "clear".into()
                    }
                    "copy" => {
                        let _ = copy.clear();
                        copy.entries = m
                            .entries
                            .iter()
                            .map(|e| Entry { key: e.key.clone(), obj: e.obj, num: e.num, text: e.text.clone() })
                            .collect();
                        "copy".into()
                    }
                    "copied" => format!("copied {}", dump(copy)),
                    op => panic!("op {op}"),
                };
                format!("map {} {} {} | {}", w[1], w[2], s, dump(m))
            } else {
                let o = sets
                    .entry(w[1].to_string())
                    .or_insert_with(|| EeWonLobbyMap::new(EeWonLobbyKeyKind::InOrder, Default::default()));
                let s = match w[2] {
                    "add" => {
                        let v: usize = w[3].parse().unwrap();
                        let (_, new) = o.insert(key(EeWonLobbyKeyKind::InOrder, v));
                        format!("add {v} -> {}", new as i32)
                    }
                    "erase" => {
                        let v: usize = w[3].parse().unwrap();
                        let (gone, _) = o.erase(&key(EeWonLobbyKeyKind::InOrder, v));
                        format!("erase {v} -> {}", gone as i32)
                    }
                    _ => {
                        let _ = o.clear();
                        "clear".into()
                    }
                };
                let mut s = format!("opt {} {} |", w[1], s);
                for e in &o.entries {
                    s += &format!(" {}", index_of(EeWonLobbyKeyKind::InOrder, &e.key));
                }
                s
            };
            assert_eq!(got, line, "golden line {}", lines + 1);
            lines += 1;
        }
        assert_eq!(lines, 6480);
    }

    static DROPS: std::sync::Mutex<Vec<usize>> = std::sync::Mutex::new(Vec::new());
    unsafe extern "C" fn copy_hook(o: *mut c_void) -> *mut c_void {
        (o as usize + 1000) as *mut c_void
    }
    unsafe extern "C" fn drop_hook(o: *mut c_void) {
        DROPS.lock().unwrap().push(o as usize);
    }

    /// The objects through the FFI: set, replace, erase, copy, clear and free drop or copy each through the hooks.
    #[test]
    #[allow(clippy::manual_dangling_ptr)] // the objects are tokens the hooks record
    fn objects_through_hooks() {
        let hooks = EeWonLobbyMapHooks { copy: Some(copy_hook), drop: Some(drop_hook) };
        let m = ee_won_lobbymap_new(EeWonLobbyKeyKind::Unsigned, hooks);
        let k =
            |n: u64| EeWonLobbyKey { num: n, addr: std::ptr::null(), text: std::ptr::null(), count: 0, unit_size: 0 };
        unsafe {
            let mut new = false;
            let i = ee_won_lobbymap_insert(m, &k(5), &mut new);
            assert!(new);
            ee_won_lobbymap_set_obj(m, i, 1 as *mut c_void);
            ee_won_lobbymap_set_obj(m, i, 2 as *mut c_void);
            let j = ee_won_lobbymap_insert(m, &k(3), &mut new);
            assert_eq!((j, new), (0, true));
            ee_won_lobbymap_set_obj(m, j, 3 as *mut c_void);
            assert_eq!(ee_won_lobbymap_find(m, &k(5)), 1);
            assert_eq!(ee_won_lobbymap_obj(m, 1), 2 as *mut c_void);
            let c = ee_won_lobbymap_new(EeWonLobbyKeyKind::Unsigned, hooks);
            ee_won_lobbymap_assign(c, m);
            assert_eq!(ee_won_lobbymap_obj(c, 0), 1003 as *mut c_void);
            assert_eq!(ee_won_lobbymap_key_num(c, 1), 5);
            assert!(ee_won_lobbymap_erase(m, &k(3)));
            assert!(!ee_won_lobbymap_erase(m, &k(3)));
            ee_won_lobbymap_free(m);
            ee_won_lobbymap_clear(c);
            ee_won_lobbymap_free(c);
        }
        assert_eq!(*DROPS.lock().unwrap(), vec![1, 3, 2, 1003, 1002]);
    }

    /// Text values and keys, numbers, address keys: what goes in comes out.
    #[test]
    fn values_and_keys() {
        let m = ee_won_lobbymap_new(EeWonLobbyKeyKind::Addr, Default::default());
        let mut a = EeWonIpAddr::default();
        a.set_host_order(0x0a000001, 8585);
        let key = EeWonLobbyKey { num: 0, addr: &a, text: std::ptr::null(), count: 0, unit_size: 0 };
        let text: [u16; 3] = [0x41, 0xe9, 0x42];
        unsafe {
            let i = ee_won_lobbymap_insert(m, &key, std::ptr::null_mut());
            ee_won_lobbymap_set_text(m, i, text.as_ptr() as *const c_void, 3, 2);
            ee_won_lobbymap_set_num(m, i, 1, -7);
            let mut n = 0;
            let t = ee_won_lobbymap_text(m, i, &mut n);
            assert_eq!(std::slice::from_raw_parts(t, n), &[0x41, 0xe9, 0x42]);
            assert_eq!((ee_won_lobbymap_num(m, i, 0), ee_won_lobbymap_num(m, i, 1)), (0, -7));
            assert_eq!(
                (*ee_won_lobbymap_key_addr(m, i)).addr(),
                Addr { address: 0x0a000001u32.to_be(), port: 8585u16.to_be() }
            );
            assert!(ee_won_lobbymap_key_addr(m, 1).is_null());
            ee_won_lobbymap_free(m);
        }
        let n = ee_won_lobbymap_new(EeWonLobbyKeyKind::NarrowNoCase, Default::default());
        let name = b"Server";
        let key = EeWonLobbyKey {
            num: 0,
            addr: std::ptr::null(),
            text: name.as_ptr() as *const c_void,
            count: 6,
            unit_size: 1,
        };
        unsafe {
            ee_won_lobbymap_insert(n, &key, std::ptr::null_mut());
            let upper = b"SERVER";
            let k2 = EeWonLobbyKey { text: upper.as_ptr() as *const c_void, ..key };
            assert_eq!(ee_won_lobbymap_find(n, &k2), 0);
            let mut c = 0;
            let p = ee_won_lobbymap_key_text(n, 0, &mut c) as *const u8;
            assert_eq!(std::slice::from_raw_parts(p, c), b"Server");
            ee_won_lobbymap_free(n);
        }
    }
}
