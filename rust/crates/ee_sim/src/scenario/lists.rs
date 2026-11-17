//! The manager's element lists as the scenario editor changes them (`EEScenarioManager`'s `Sort*`, `New*`,
//! `Move*Up`/`Move*Down`, `Delete*`, `Duplicate*`, the name lists and the id searches) and the index upkeep that
//! follows each change: the `Renumber*` (a swap or a delete) and `Remap*` (an old-to-new map) broadcasts over every
//! element that refers to the list, the element classes' own `Renumber*`/`Remap*` and `EERemapID` included.
//!
//! The lists stay the C++ vectors of the C++ objects (the capture, the saves, the trigger round and the editor's
//! screens read them); the logic reaches them through [`Lists`], one operation each, and holds no reference to an
//! element across a call (an element is an opaque pointer, its numbers a record borrowed for one statement).

use std::ffi::{c_void, CStr};
use std::os::raw::{c_char, c_long, c_ulong};

use crate::scenario::condition::choose_player_recorded;
use crate::scenario::fields::{
    self, EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioObjectFields, EE_SCENARIO_RENUMBER_AREAS,
    EE_SCENARIO_RENUMBER_OBJECTS, EE_SCENARIO_RENUMBER_TRIGGERS,
};
use crate::scenario::host::{EeScenarioHost, Host, EE_SCENARIO_READ_CLASS_EXISTS};
use crate::scenario::manager::{EeScenarioManagerFields, EeScenarioTimedQueue};

/// The lists, by the manager's member: `mTriggers`, `mConditions`, `mEffects`, `mObjects`, `mAreas`,
/// `mPendingEffects` (the last only for the index upkeep).
pub const EE_SCENARIO_LIST_TRIGGERS: u32 = 0;
pub const EE_SCENARIO_LIST_CONDITIONS: u32 = 1;
pub const EE_SCENARIO_LIST_EFFECTS: u32 = 2;
pub const EE_SCENARIO_LIST_OBJECTS: u32 = 3;
pub const EE_SCENARIO_LIST_AREAS: u32 = 4;
pub const EE_SCENARIO_LIST_PENDING: u32 = 5;

/// The name lists (`GetTriggerNames`, `GetAllConditionNames`, `GetUserConditionNames`, `GetAllEffectNames`,
/// `GetUserEffectNames`, `GetObjectNames`, `GetUserObjectNames`, `GetAreaNames`, `GetUserAreaNames`).
pub const EE_SCENARIO_NAMES_TRIGGERS: u32 = 0;
pub const EE_SCENARIO_NAMES_CONDITIONS: u32 = 1;
pub const EE_SCENARIO_NAMES_USER_CONDITIONS: u32 = 2;
pub const EE_SCENARIO_NAMES_EFFECTS: u32 = 3;
pub const EE_SCENARIO_NAMES_USER_EFFECTS: u32 = 4;
pub const EE_SCENARIO_NAMES_OBJECTS: u32 = 5;
pub const EE_SCENARIO_NAMES_USER_OBJECTS: u32 = 6;
pub const EE_SCENARIO_NAMES_AREAS: u32 = 7;
pub const EE_SCENARIO_NAMES_USER_AREAS: u32 = 8;

/// The C++ host's operations on the lists (`EESMListOp`, EEScenarioManager.cpp): `index` and `element` as each
/// names them, the answer an element or an integer in the pointer.
/// The list's size.
pub const EE_SCENARIO_LIST_OP_COUNT: u32 = 0;
/// Element `index` (NULL past the end).
pub const EE_SCENARIO_LIST_OP_AT: u32 = 1;
/// `list[index] = element`.
pub const EE_SCENARIO_LIST_OP_SET: u32 = 2;
/// `list.insert(begin + index, element)`.
pub const EE_SCENARIO_LIST_OP_INSERT: u32 = 3;
/// `list.erase(begin + index)`.
pub const EE_SCENARIO_LIST_OP_ERASE: u32 = 4;
/// `list.push_back(element)`.
pub const EE_SCENARIO_LIST_OP_PUSH: u32 = 5;
/// `new` element of the list's class (its constructor's defaults).
pub const EE_SCENARIO_LIST_OP_CREATE: u32 = 6;
/// `element->Copy(*list[index])`.
pub const EE_SCENARIO_LIST_OP_COPY: u32 = 7;
/// `delete element`.
pub const EE_SCENARIO_LIST_OP_DESTROY: u32 = 8;
/// The element's `mName` as a C string (valid until the next operation).
pub const EE_SCENARIO_LIST_OP_NAME: u32 = 9;
/// The element's name set to `text` (an object's through `EESMObject::SetName`, which renames its conditions).
pub const EE_SCENARIO_LIST_OP_SET_NAME: u32 = 10;
/// The element's id (`mID`, an object's `mL64`).
pub const EE_SCENARIO_LIST_OP_ID: u32 = 11;
/// The element's id set to `index`.
pub const EE_SCENARIO_LIST_OP_SET_ID: u32 = 12;
/// The element's numbers (the condition's, effect's, object's or trigger's fields record; NULL for an area).
pub const EE_SCENARIO_LIST_OP_FIELDS: u32 = 13;
/// An effect's `mObjects.size()`.
pub const EE_SCENARIO_LIST_OP_OBJECT_COUNT: u32 = 14;
/// An effect's `mObjects.data()`.
pub const EE_SCENARIO_LIST_OP_OBJECTS: u32 = 15;
/// A trigger's entry count: `list` 0 its condition entries, 1 its effect entries.
pub const EE_SCENARIO_LIST_OP_ENTRY_COUNT: u32 = 16;
/// The index in a trigger's entry: `index` the entry, `list` 0 a condition entry's `mValue`, 1 an effect entry's
/// `mEffectIndex` (a pointer to the `unsigned long`).
pub const EE_SCENARIO_LIST_OP_ENTRY: u32 = 17;
/// `gResource->GetString` of string `index` (`EE_SCENARIO_STRING_*`) as a C string.
pub const EE_SCENARIO_LIST_OP_RESOURCE: u32 = 18;
/// `text` appended to the caller's name list.
pub const EE_SCENARIO_LIST_OP_OUT_NAME: u32 = 19;
/// An area's `SetAnchor(U2DPoint<long>(x, y))`: `x` in `list` (32 bits), `y` in `index`.
pub const EE_SCENARIO_LIST_OP_ANCHOR: u32 = 20;
/// `Prepare`'s media: player `list`'s upgrade of unit class `index` loads its media, kept in `mUnitClassMedia`.
pub const EE_SCENARIO_LIST_OP_MEDIA: u32 = 21;

/// The strings of [`EE_SCENARIO_LIST_OP_RESOURCE`]: `IDS_COPY`, `IDS_EXISTS`, and the built-ins' names
/// (`IDS_ALWAYS_TRUE`, `IDS_DO_NOTHING`, `IDS_NONE`, `IDS_OFF_MAP`, `IDS_PREVIOUS_LOCATION`).
pub const EE_SCENARIO_STRING_COPY: usize = 0;
pub const EE_SCENARIO_STRING_EXISTS: usize = 1;
pub const EE_SCENARIO_STRING_ALWAYS_TRUE: usize = 2;
pub const EE_SCENARIO_STRING_DO_NOTHING: usize = 3;
pub const EE_SCENARIO_STRING_NONE: usize = 4;
pub const EE_SCENARIO_STRING_OFF_MAP: usize = 5;
pub const EE_SCENARIO_STRING_PREVIOUS_LOCATION: usize = 6;

/// `kSMConditionObject`: an object's Exists condition (they stay at the end of the conditions).
const CONDITION_OBJECT: c_ulong = 1;
/// The "deleted" new index of a renumbering (32 bits set, as the indices read from a scenario).
const DELETED: c_ulong = 0xffff_ffff;

/// An element: the C++ object's address (compared, never read here).
pub type Element = *mut c_void;

/// The C++ lists and elements, one operation at a time. Each borrow ends before the next call.
pub trait Lists {
    fn count(&mut self, list: u32) -> usize;
    fn at(&mut self, list: u32, index: usize) -> Element;
    fn set(&mut self, list: u32, index: usize, element: Element);
    fn insert(&mut self, list: u32, index: usize, element: Element);
    fn erase(&mut self, list: u32, index: usize);
    fn push(&mut self, list: u32, element: Element);
    fn create(&mut self, list: u32) -> Element;
    fn copy(&mut self, list: u32, from: usize, to: Element);
    fn destroy(&mut self, list: u32, element: Element);
    fn name(&mut self, list: u32, element: Element) -> Vec<u8>;
    fn set_name(&mut self, list: u32, element: Element, name: &[u8]);
    fn id(&mut self, list: u32, element: Element) -> c_ulong;
    fn set_id(&mut self, list: u32, element: Element, id: c_ulong);
    fn condition(&mut self, element: Element) -> &mut EeScenarioConditionFields;
    fn effect(&mut self, element: Element) -> &mut EeScenarioEffectFields;
    fn object(&mut self, element: Element) -> &mut EeScenarioObjectFields;
    /// An effect's object list.
    fn effect_objects(&mut self, element: Element) -> &mut [c_ulong];
    /// A trigger's condition (`effects` false) or effect entries' count.
    fn entry_count(&mut self, trigger: Element, effects: bool) -> usize;
    /// The index in a trigger's entry.
    fn entry(&mut self, trigger: Element, effects: bool, index: usize) -> &mut c_ulong;
    /// The language's string `which` (`EE_SCENARIO_STRING_*`).
    fn resource(&mut self, which: usize) -> Vec<u8>;
    /// An area's `SetAnchor` (x, y).
    fn set_anchor(&mut self, area: Element, x: c_long, y: c_long);
    /// `Prepare`'s media of unit class `class` for player `player` loaded and kept.
    fn load_media(&mut self, player: i32, class: c_ulong);
    fn out_name(&mut self, name: &[u8]);
    fn manager(&mut self) -> &mut EeScenarioManagerFields;
    fn timed(&mut self) -> &mut EeScenarioTimedQueue;
}

/// `sprintf("%ld")` of an `unsigned long` id or counter.
fn num(value: c_ulong) -> Vec<u8> {
    (value as c_long).to_string().into_bytes()
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

/// The number of characters (`UWideString::GetLength`, ee_plat_core's `PlatMbsLen`).
fn chars(s: &[u8]) -> usize {
    ee_base::plat_core::utf8::char_len(s)
}

/// `inString[index]`: the first byte of character `index` (0 out of range).
fn char_at(s: &[u8], index: usize) -> u8 {
    if index >= chars(s) {
        return 0;
    }
    s[ee_base::plat_core::utf8::bytes_of_chars(s, index)]
}

/// The first `count` characters (`CopyString(s, count)`).
fn first_chars(s: &[u8], count: usize) -> &[u8] {
    &s[..ee_base::plat_core::utf8::bytes_of_chars(s, count)]
}

/// Without its first character (`PlatMbsNInc(s, 1)`).
fn after_first(s: &[u8]) -> &[u8] {
    &s[ee_base::plat_core::utf8::bytes_of_chars(s, 1)..]
}

/// The `Duplicate*` name's stem: a bracketed name ("<name>", over 2 characters) loses its brackets (the closing one
/// only when it is there).
fn stem(name: &[u8]) -> Vec<u8> {
    let mut stem = name.to_vec();
    if chars(&stem) > 2 && char_at(&stem, 0) == b'<' {
        stem = after_first(&stem).to_vec();
        let n = chars(&stem);
        if char_at(&stem, n.wrapping_sub(1)) == b'>' {
            stem = first_chars(&stem, n - 1).to_vec();
        }
    }
    stem
}

/// The first condition that is an object's (its index, the count when none): the user conditions end there.
fn first_object_condition<L: Lists + ?Sized>(l: &mut L) -> usize {
    let n = l.count(EE_SCENARIO_LIST_CONDITIONS);
    (0..n)
        .find(|&i| {
            let e = l.at(EE_SCENARIO_LIST_CONDITIONS, i);
            l.condition(e).mType == CONDITION_OBJECT
        })
        .unwrap_or(n)
}

fn snapshot<L: Lists + ?Sized>(l: &mut L, list: u32) -> Vec<Element> {
    (0..l.count(list)).map(|i| l.at(list, i)).collect()
}

/// The old-to-new map of a reordered list (`EESMIDMap`: from new index `from` on, each element's old place).
fn map_of<L: Lists + ?Sized>(l: &mut L, list: u32, old: &[Element], from: usize) -> Vec<(c_ulong, c_ulong)> {
    let mut map: Vec<(c_ulong, c_ulong)> = Vec::new();
    for new in from..l.count(list) {
        let e = l.at(list, new);
        if let Some(o) = old.iter().position(|&x| x == e) {
            if !map.iter().any(|&(k, _)| k == o as c_ulong) {
                map.push((o as c_ulong, new as c_ulong));
            }
        }
    }
    map.sort_by_key(|&(k, _)| k);
    map
}

/// `EERemapID`: the index through the map, 0 when the map has no entry.
fn remap_id(id: &mut c_ulong, map: &[(c_ulong, c_ulong)]) {
    *id = map.binary_search_by_key(id, |&(k, _)| k).map_or(0, |i| map[i].1);
}

/// `std::stable_sort` of `list[from..to]` by name (`UWideString::operator<`: `PlatMbsCmp`, byte order).
fn sort_range<L: Lists + ?Sized>(l: &mut L, list: u32, from: usize, to: usize) {
    if from >= to {
        return;
    }
    let mut named: Vec<(Vec<u8>, Element)> = (from..to)
        .map(|i| {
            let e = l.at(list, i);
            (l.name(list, e), e)
        })
        .collect();
    named.sort_by(|a, b| a.0.cmp(&b.0));
    for (k, (_, e)) in named.into_iter().enumerate() {
        l.set(list, from + k, e);
    }
}

/// An index change of `list`'s entries passed to every element that refers to the list (`Renumber*`: `old` and
/// `new` swapped, or `old` deleted when `new` is [`DELETED`]).
pub fn renumber<L: Lists + ?Sized>(l: &mut L, list: u32, old: c_ulong, new: c_ulong) {
    match list {
        EE_SCENARIO_LIST_TRIGGERS | EE_SCENARIO_LIST_OBJECTS | EE_SCENARIO_LIST_AREAS => {
            let kind = match list {
                EE_SCENARIO_LIST_TRIGGERS => EE_SCENARIO_RENUMBER_TRIGGERS,
                EE_SCENARIO_LIST_OBJECTS => EE_SCENARIO_RENUMBER_OBJECTS,
                _ => EE_SCENARIO_RENUMBER_AREAS,
            };
            if list != EE_SCENARIO_LIST_AREAS {
                for i in 0..l.count(EE_SCENARIO_LIST_CONDITIONS) {
                    let e = l.at(EE_SCENARIO_LIST_CONDITIONS, i);
                    fields::condition_renumber(l.condition(e), kind, old, new);
                }
            }
            for effects in [EE_SCENARIO_LIST_EFFECTS, EE_SCENARIO_LIST_OBJECTS, EE_SCENARIO_LIST_PENDING] {
                if effects == EE_SCENARIO_LIST_OBJECTS {
                    if list == EE_SCENARIO_LIST_TRIGGERS {
                        continue;
                    }
                    for i in 0..l.count(EE_SCENARIO_LIST_OBJECTS) {
                        let e = l.at(EE_SCENARIO_LIST_OBJECTS, i);
                        fields::object_renumber(l.object(e), kind, old, new);
                    }
                    continue;
                }
                for i in 0..l.count(effects) {
                    let e = l.at(effects, i);
                    fields::effect_renumber(l.effect(e), kind, old, new);
                    if list == EE_SCENARIO_LIST_OBJECTS {
                        for id in l.effect_objects(e) {
                            fields::renumber(id, old, new);
                        }
                    }
                }
            }
        }
        EE_SCENARIO_LIST_CONDITIONS | EE_SCENARIO_LIST_EFFECTS => {
            let effects = list == EE_SCENARIO_LIST_EFFECTS;
            for t in 0..l.count(EE_SCENARIO_LIST_TRIGGERS) {
                let e = l.at(EE_SCENARIO_LIST_TRIGGERS, t);
                for k in 0..l.entry_count(e, effects) {
                    fields::renumber(l.entry(e, effects, k), old, new);
                }
            }
            if effects {
                l.timed().renumber(old, new);
            }
        }
        _ => {}
    }
}

/// An old-to-new map of `list`'s indices applied to every element that refers to the list (`Remap*`).
pub fn remap<L: Lists + ?Sized>(l: &mut L, list: u32, map: &[(c_ulong, c_ulong)]) {
    match list {
        EE_SCENARIO_LIST_TRIGGERS | EE_SCENARIO_LIST_OBJECTS | EE_SCENARIO_LIST_AREAS => {
            if list != EE_SCENARIO_LIST_AREAS {
                for i in 0..l.count(EE_SCENARIO_LIST_CONDITIONS) {
                    let e = l.at(EE_SCENARIO_LIST_CONDITIONS, i);
                    let c = l.condition(e);
                    remap_id(
                        if list == EE_SCENARIO_LIST_TRIGGERS { &mut c.mTrigger } else { &mut c.mObjectIndex },
                        map,
                    );
                }
            }
            for effects in [EE_SCENARIO_LIST_EFFECTS, EE_SCENARIO_LIST_OBJECTS, EE_SCENARIO_LIST_PENDING] {
                if effects == EE_SCENARIO_LIST_OBJECTS {
                    if list == EE_SCENARIO_LIST_TRIGGERS {
                        continue;
                    }
                    for i in 0..l.count(EE_SCENARIO_LIST_OBJECTS) {
                        let e = l.at(EE_SCENARIO_LIST_OBJECTS, i);
                        let o = l.object(e);
                        if list == EE_SCENARIO_LIST_AREAS {
                            remap_id(&mut o.mArea, map);
                        } else {
                            remap_id(&mut o.mParent, map);
                            remap_id(&mut o.mNearObject, map);
                        }
                    }
                    continue;
                }
                for i in 0..l.count(effects) {
                    let e = l.at(effects, i);
                    let f = l.effect(e);
                    match list {
                        EE_SCENARIO_LIST_AREAS => {
                            for id in [&mut f.mTaskArea, &mut f.mTargetArea, &mut f.mPlaceArea, &mut f.mLocationArea] {
                                remap_id(id, map);
                            }
                        }
                        EE_SCENARIO_LIST_OBJECTS => {
                            for id in [
                                &mut f.mObject,
                                &mut f.mLocationObject,
                                &mut f.mTaskObject,
                                &mut f.mPlaceObject,
                                &mut f.mTargetObject,
                            ] {
                                remap_id(id, map);
                            }
                            for id in l.effect_objects(e) {
                                remap_id(id, map);
                            }
                        }
                        _ => remap_id(&mut f.mTrigger, map),
                    }
                }
            }
        }
        EE_SCENARIO_LIST_CONDITIONS | EE_SCENARIO_LIST_EFFECTS => {
            let effects = list == EE_SCENARIO_LIST_EFFECTS;
            for t in 0..l.count(EE_SCENARIO_LIST_TRIGGERS) {
                let e = l.at(EE_SCENARIO_LIST_TRIGGERS, t);
                for k in 0..l.entry_count(e, effects) {
                    remap_id(l.entry(e, effects, k), map);
                }
            }
            if effects {
                let olds: Vec<c_ulong> = map.iter().map(|&(k, _)| k).collect();
                let news: Vec<c_ulong> = map.iter().map(|&(_, v)| v).collect();
                l.timed().remap(&olds, &news);
            }
        }
        _ => {}
    }
}

/// `Sort*`: the user entries by name, then every reference remapped. Areas from 2 (the built-ins first), effects
/// from 1 (Do Nothing first, the timed effects dropped), objects from 1 (None first; then the object conditions by
/// name and the conditions remapped), conditions from 1 to the first object condition, triggers all.
pub fn sort<L: Lists + ?Sized>(l: &mut L, list: u32) {
    let old = snapshot(l, list);
    let n = old.len();
    match list {
        EE_SCENARIO_LIST_AREAS => {
            sort_range(l, list, 2, n);
            let map = map_of(l, list, &old, 0);
            remap(l, list, &map);
        }
        EE_SCENARIO_LIST_CONDITIONS => {
            let end = first_object_condition(l);
            sort_range(l, list, 1, end);
            let map = map_of(l, list, &old, 0);
            remap(l, list, &map);
        }
        EE_SCENARIO_LIST_EFFECTS => {
            l.timed().entries.clear();
            sort_range(l, list, 1, n);
            let map = map_of(l, list, &old, 1);
            remap(l, list, &map);
        }
        EE_SCENARIO_LIST_OBJECTS => {
            let old_conditions = snapshot(l, EE_SCENARIO_LIST_CONDITIONS);
            sort_range(l, list, 1, n);
            let start = first_object_condition(l);
            sort_range(l, EE_SCENARIO_LIST_CONDITIONS, start, old_conditions.len());
            let map = map_of(l, list, &old, 1);
            remap(l, list, &map);
            let map = map_of(l, EE_SCENARIO_LIST_CONDITIONS, &old_conditions, 1);
            remap(l, EE_SCENARIO_LIST_CONDITIONS, &map);
        }
        EE_SCENARIO_LIST_TRIGGERS => {
            sort_range(l, list, 0, n);
            let map = map_of(l, list, &old, 0);
            remap(l, list, &map);
        }
        _ => {}
    }
}

/// The counter of the list's ids (`mNextTriggerID` ...).
fn next_id(m: &mut EeScenarioManagerFields, list: u32) -> &mut c_ulong {
    match list {
        EE_SCENARIO_LIST_TRIGGERS => &mut m.mNextTriggerID,
        EE_SCENARIO_LIST_CONDITIONS => &mut m.mNextConditionID,
        EE_SCENARIO_LIST_EFFECTS => &mut m.mNextEffectID,
        EE_SCENARIO_LIST_OBJECTS => &mut m.mNextObjectID,
        _ => &mut m.mNextAreaID,
    }
}

/// `++counter`.
fn bump<L: Lists + ?Sized>(l: &mut L, list: u32) -> c_ulong {
    let id = next_id(l.manager(), list);
    *id = id.wrapping_add(1);
    *id
}

/// An object's Exists condition, appended (`NewObject` and `DuplicateObject` name it themselves).
fn add_object_condition<L: Lists + ?Sized>(l: &mut L) -> Element {
    let c = l.create(EE_SCENARIO_LIST_CONDITIONS);
    l.push(EE_SCENARIO_LIST_CONDITIONS, c);
    c
}

/// `New*`: a new element named "<KindN>" (N the counter before its increment, the id the counter after); a
/// condition goes before the object conditions (the conditions remapped), an object gets its Exists condition
/// (not the first object, None). Returns the element.
pub fn new<L: Lists + ?Sized>(l: &mut L, list: u32) -> Element {
    let kind: &[u8] = match list {
        EE_SCENARIO_LIST_TRIGGERS => b"Trigger",
        EE_SCENARIO_LIST_CONDITIONS => b"Condition",
        EE_SCENARIO_LIST_EFFECTS => b"Effect",
        EE_SCENARIO_LIST_OBJECTS => {
            let o = l.create(list);
            let id = bump(l, list);
            l.set_id(list, o, id);
            l.push(list, o);
            if l.count(list) <= 1 {
                return o;
            }
            let c = add_object_condition(l);
            let id = bump(l, EE_SCENARIO_LIST_CONDITIONS);
            let exists = l.resource(EE_SCENARIO_STRING_EXISTS);
            let object_name = l.name(list, o);
            l.set_name(
                EE_SCENARIO_LIST_CONDITIONS,
                c,
                &cat(&[b"<", &num(id), b":", &object_name, b" ", &exists, b">"]),
            );
            l.set_id(EE_SCENARIO_LIST_CONDITIONS, c, id);
            let n = l.count(list);
            let f = l.condition(c);
            f.mType = CONDITION_OBJECT;
            f.mObjectIndex = (n - 1) as c_ulong;
            return o;
        }
        _ => b"Area",
    };
    let old = if list == EE_SCENARIO_LIST_CONDITIONS { snapshot(l, list) } else { Vec::new() };
    let at = if list == EE_SCENARIO_LIST_CONDITIONS { first_object_condition(l) } else { 0 };
    let counter = *next_id(l.manager(), list);
    let id = bump(l, list);
    let e = l.create(list);
    l.set_name(list, e, &cat(&[b"<", kind, &num(counter), b">"]));
    l.set_id(list, e, id);
    if list == EE_SCENARIO_LIST_CONDITIONS {
        l.insert(list, at, e);
        let map = map_of(l, list, &old, 0);
        remap(l, list, &map);
    } else {
        l.push(list, e);
    }
    e
}

/// `Move*Up` (`up`) or `Move*Down`: the entry swapped with its neighbour and the references renumbered; nothing for
/// a built-in entry, the first user entry moved up, the last moved down, a condition moved down onto an object
/// condition. An object's Exists condition moves with it. An index past the end does nothing (the original read
/// and wrote past the vector).
pub fn move_entry<L: Lists + ?Sized>(l: &mut L, list: u32, index: c_ulong, up: bool) {
    let n = l.count(list) as c_ulong;
    if index >= n {
        return;
    }
    let other = if up {
        let first = match list {
            EE_SCENARIO_LIST_AREAS => 3,
            EE_SCENARIO_LIST_TRIGGERS => 1,
            _ => 2,
        };
        if index < first {
            return;
        }
        index - 1
    } else {
        let first = match list {
            EE_SCENARIO_LIST_AREAS => 2,
            EE_SCENARIO_LIST_TRIGGERS => 0,
            _ => 1,
        };
        if index < first || index == n - 1 {
            return;
        }
        if list == EE_SCENARIO_LIST_CONDITIONS && index + 1 >= first_object_condition(l) as c_ulong {
            return;
        }
        index + 1
    };
    let (i, j) = (index as usize, other as usize);
    let a = l.at(list, i);
    let b = l.at(list, j);
    l.set(list, i, b);
    l.set(list, j, a);
    if list == EE_SCENARIO_LIST_OBJECTS {
        let count = l.count(EE_SCENARIO_LIST_CONDITIONS);
        let end = if up { count } else { count.saturating_sub(1) };
        for k in 1..end {
            let c = l.at(EE_SCENARIO_LIST_CONDITIONS, k);
            let f = l.condition(c);
            if f.mType != CONDITION_OBJECT || f.mObjectIndex != index {
                continue;
            }
            let k2 = if up { k - 1 } else { k + 1 };
            let d = l.at(EE_SCENARIO_LIST_CONDITIONS, k2);
            l.set(EE_SCENARIO_LIST_CONDITIONS, k, d);
            l.set(EE_SCENARIO_LIST_CONDITIONS, k2, c);
            renumber(l, EE_SCENARIO_LIST_CONDITIONS, k as c_ulong, k2 as c_ulong);
            break;
        }
    }
    renumber(l, list, index, other);
}

/// `Delete*`: the entry erased and freed and the references renumbered (references to it become 0); an effect's
/// timed entries go first, an object's Exists condition after it. False (nothing done) for a built-in entry (the
/// first two areas, Always True, Do Nothing, None) or an index past the end (the original erased past the vector).
pub fn delete<L: Lists + ?Sized>(l: &mut L, list: u32, index: c_ulong) -> bool {
    let first = match list {
        EE_SCENARIO_LIST_AREAS => 2,
        EE_SCENARIO_LIST_TRIGGERS => 0,
        _ => 1,
    };
    if index < first || index >= l.count(list) as c_ulong {
        return false;
    }
    let i = index as usize;
    let e = l.at(list, i);
    if list == EE_SCENARIO_LIST_EFFECTS {
        l.timed().remove_effect(index as c_long);
    }
    l.erase(list, i);
    l.destroy(list, e);
    if list == EE_SCENARIO_LIST_OBJECTS {
        for k in 1..l.count(EE_SCENARIO_LIST_CONDITIONS) {
            let c = l.at(EE_SCENARIO_LIST_CONDITIONS, k);
            let f = l.condition(c);
            if f.mType == CONDITION_OBJECT && f.mObjectIndex == index {
                delete(l, EE_SCENARIO_LIST_CONDITIONS, k as c_ulong);
                break;
            }
        }
    }
    renumber(l, list, index, DELETED);
    true
}

/// `Duplicate*`: a copy of entry `index` named "<stem_Copy>" with the next id, appended (a condition inserted
/// before the object conditions, the conditions remapped; an object named "stem_Copy" when its name was not
/// bracketed, with its own "name Exists" condition). Returns the copy (NULL for an index past the end).
pub fn duplicate<L: Lists + ?Sized>(l: &mut L, list: u32, index: c_ulong) -> Element {
    if index >= l.count(list) as c_ulong {
        return std::ptr::null_mut();
    }
    let i = index as usize;
    let old = if list == EE_SCENARIO_LIST_CONDITIONS { snapshot(l, list) } else { Vec::new() };
    let at = if list == EE_SCENARIO_LIST_CONDITIONS { first_object_condition(l) } else { 0 };
    let source = l.at(list, i);
    let name = l.name(list, source);
    let copy = l.resource(EE_SCENARIO_STRING_COPY);
    let new_name = if list == EE_SCENARIO_LIST_OBJECTS && !(chars(&name) > 2 && char_at(&name, 0) == b'<') {
        cat(&[&name, b"_", &copy])
    } else if list == EE_SCENARIO_LIST_OBJECTS {
        // the original's own strip: a name without its closing bracket keeps its opening one
        let stripped = after_first(&name);
        let n = chars(stripped);
        let stem = if char_at(stripped, n.wrapping_sub(1)) == b'>' {
            first_chars(stripped, n - 1).to_vec()
        } else {
            name.clone()
        };
        cat(&[b"<", &stem, b"_", &copy, b">"])
    } else {
        cat(&[b"<", &stem(&name), b"_", &copy, b">"])
    };
    let e = l.create(list);
    l.copy(list, i, e);
    l.set_name(list, e, &new_name);
    let id = bump(l, list);
    l.set_id(list, e, id);
    match list {
        EE_SCENARIO_LIST_CONDITIONS => {
            l.insert(list, at, e);
            let map = map_of(l, list, &old, 0);
            remap(l, list, &map);
        }
        EE_SCENARIO_LIST_OBJECTS => {
            l.push(list, e);
            let c = add_object_condition(l);
            let exists = l.resource(EE_SCENARIO_STRING_EXISTS);
            let object_name = l.name(list, e);
            l.set_name(EE_SCENARIO_LIST_CONDITIONS, c, &cat(&[&object_name, b" ", &exists]));
            let n = l.count(list);
            let f = l.condition(c);
            f.mType = CONDITION_OBJECT;
            f.mObjectIndex = (n - 1) as c_ulong;
            let id = bump(l, EE_SCENARIO_LIST_CONDITIONS);
            l.set_id(EE_SCENARIO_LIST_CONDITIONS, c, id);
        }
        _ => l.push(list, e),
    }
    e
}

/// `Find*ByID` (conditions, effects, objects by `mL64`): the first element with the id, NULL without one.
pub fn find<L: Lists + ?Sized>(l: &mut L, list: u32, id: c_long) -> Element {
    for i in 0..l.count(list) {
        let e = l.at(list, i);
        if l.id(list, e) == id as c_ulong {
            return e;
        }
    }
    std::ptr::null_mut()
}

/// `EESMObject::SetName`'s second half (the object's name already set): every object condition (from 1) of the
/// object's index renamed "name Exists". An object not in the list has index -1, which matches a condition of index
/// `0xffffffff` (the original's `unsigned long == long`).
pub fn rename_object_conditions<L: Lists + ?Sized>(l: &mut L, object: Element) {
    let index = index_of(l, EE_SCENARIO_LIST_OBJECTS, object) as c_ulong;
    let name = cat(&[&l.name(EE_SCENARIO_LIST_OBJECTS, object), b" Exists"]);
    for i in 1..l.count(EE_SCENARIO_LIST_CONDITIONS) {
        let c = l.at(EE_SCENARIO_LIST_CONDITIONS, i);
        let f = l.condition(c);
        if f.mType == CONDITION_OBJECT && f.mObjectIndex == index {
            l.set_name(EE_SCENARIO_LIST_CONDITIONS, c, &name);
        }
    }
}

/// `EESMObject::SetParent`: the parent set, unless following parents from it (to None, object 0, or past the list)
/// comes back to an object already met, the object itself included: then the parent is 0.
pub fn set_parent<L: Lists + ?Sized>(l: &mut L, object: Element, parent: c_long) {
    l.object(object).mParent = parent as c_ulong;
    let root = l.at(EE_SCENARIO_LIST_OBJECTS, 0);
    let mut visited = vec![object];
    let mut next = parent as c_ulong;
    loop {
        let o = l.at(EE_SCENARIO_LIST_OBJECTS, next as usize);
        if o.is_null() || o == root {
            break;
        }
        if visited.contains(&o) {
            l.object(object).mParent = 0;
            return;
        }
        visited.push(o);
        next = l.object(o).mParent;
    }
    l.object(object).mParent = parent as c_ulong;
}

/// `GetObjectIndex`: the element's place in the list, -1 when it is not there.
pub fn index_of<L: Lists + ?Sized>(l: &mut L, list: u32, element: Element) -> c_long {
    (0..l.count(list)).find(|&i| l.at(list, i) == element).map_or(-1, |i| i as c_long)
}

/// The `Get*Names` lists, each name "[id]"-prefixed while the manager shows debug ids. The user lists leave out the
/// built-ins (and the object conditions); the conditions' list first renames each object condition
/// "<object name Exists>" (English text, as the original).
pub fn names<L: Lists + ?Sized>(l: &mut L, which: u32) {
    let (list, from) = match which {
        EE_SCENARIO_NAMES_TRIGGERS => (EE_SCENARIO_LIST_TRIGGERS, 0),
        EE_SCENARIO_NAMES_CONDITIONS => (EE_SCENARIO_LIST_CONDITIONS, 0),
        EE_SCENARIO_NAMES_USER_CONDITIONS => (EE_SCENARIO_LIST_CONDITIONS, 1),
        EE_SCENARIO_NAMES_EFFECTS => (EE_SCENARIO_LIST_EFFECTS, 0),
        EE_SCENARIO_NAMES_USER_EFFECTS => (EE_SCENARIO_LIST_EFFECTS, 1),
        EE_SCENARIO_NAMES_OBJECTS => (EE_SCENARIO_LIST_OBJECTS, 0),
        EE_SCENARIO_NAMES_USER_OBJECTS => (EE_SCENARIO_LIST_OBJECTS, 1),
        EE_SCENARIO_NAMES_AREAS => (EE_SCENARIO_LIST_AREAS, 0),
        EE_SCENARIO_NAMES_USER_AREAS => (EE_SCENARIO_LIST_AREAS, 2),
        _ => return,
    };
    let debug = l.manager().mShowDebugIDs;
    for i in from..l.count(list) {
        let e = l.at(list, i);
        if list == EE_SCENARIO_LIST_CONDITIONS {
            let f = l.condition(e);
            let (is_object, object) = (f.mType == CONDITION_OBJECT, f.mObjectIndex);
            if is_object && which == EE_SCENARIO_NAMES_USER_CONDITIONS {
                continue;
            }
            if is_object && which == EE_SCENARIO_NAMES_CONDITIONS {
                let o = l.at(EE_SCENARIO_LIST_OBJECTS, object as usize);
                if !o.is_null() {
                    let object_name = l.name(EE_SCENARIO_LIST_OBJECTS, o);
                    l.set_name(EE_SCENARIO_LIST_CONDITIONS, e, &cat(&[b"<", &object_name, b" Exists>"]));
                }
            }
        }
        let prefix = if debug { cat(&[b"[", &num(l.id(list, e)), b"]"]) } else { Vec::new() };
        let name = l.name(list, e);
        l.out_name(&cat(&[&prefix, &name]));
    }
}

/// The C++ host of the lists (`EESMListOp` in EEScenarioManager.cpp): the manager's numbers and timed effects, and
/// one operation callback (`EE_SCENARIO_LIST_OP_*`; `text` a NUL-terminated name for `SET_NAME` and `OUT_NAME`).
#[repr(C)]
pub struct EeScenarioListHost {
    pub ctx: *mut c_void,
    pub manager: *mut EeScenarioManagerFields,
    pub timed: *mut EeScenarioTimedQueue,
    pub op: unsafe extern "C" fn(
        ctx: *mut c_void,
        op: u32,
        list: u32,
        index: usize,
        element: *mut c_void,
        text: *const c_char,
    ) -> *mut c_void,
}

/// [`Lists`] over the C++ host. Every borrow it hands out ends before the next operation.
pub(crate) struct HostLists<'a>(pub(crate) &'a EeScenarioListHost);

impl HostLists<'_> {
    fn call(&mut self, op: u32, list: u32, index: usize, element: Element) -> *mut c_void {
        // SAFETY: the host's contract (an operation on the C++ lists)
        unsafe { (self.0.op)(self.0.ctx, op, list, index, element, std::ptr::null()) }
    }
    fn call_text(&mut self, op: u32, list: u32, element: Element, text: &[u8]) {
        let mut z = text.to_vec();
        z.push(0);
        // SAFETY: the host's contract; `z` is NUL-terminated and outlives the call
        unsafe { (self.0.op)(self.0.ctx, op, list, 0, element, z.as_ptr() as *const c_char) };
    }
    fn c_text(p: *mut c_void) -> Vec<u8> {
        if p.is_null() {
            return Vec::new();
        }
        // SAFETY: the host answers a NUL-terminated string valid until its next operation
        unsafe { CStr::from_ptr(p as *const c_char) }.to_bytes().to_vec()
    }
}

impl Lists for HostLists<'_> {
    fn count(&mut self, list: u32) -> usize {
        self.call(EE_SCENARIO_LIST_OP_COUNT, list, 0, std::ptr::null_mut()) as usize
    }
    fn at(&mut self, list: u32, index: usize) -> Element {
        self.call(EE_SCENARIO_LIST_OP_AT, list, index, std::ptr::null_mut())
    }
    fn set(&mut self, list: u32, index: usize, element: Element) {
        self.call(EE_SCENARIO_LIST_OP_SET, list, index, element);
    }
    fn insert(&mut self, list: u32, index: usize, element: Element) {
        self.call(EE_SCENARIO_LIST_OP_INSERT, list, index, element);
    }
    fn erase(&mut self, list: u32, index: usize) {
        self.call(EE_SCENARIO_LIST_OP_ERASE, list, index, std::ptr::null_mut());
    }
    fn push(&mut self, list: u32, element: Element) {
        self.call(EE_SCENARIO_LIST_OP_PUSH, list, 0, element);
    }
    fn create(&mut self, list: u32) -> Element {
        self.call(EE_SCENARIO_LIST_OP_CREATE, list, 0, std::ptr::null_mut())
    }
    fn copy(&mut self, list: u32, from: usize, to: Element) {
        self.call(EE_SCENARIO_LIST_OP_COPY, list, from, to);
    }
    fn destroy(&mut self, list: u32, element: Element) {
        self.call(EE_SCENARIO_LIST_OP_DESTROY, list, 0, element);
    }
    fn name(&mut self, list: u32, element: Element) -> Vec<u8> {
        let p = self.call(EE_SCENARIO_LIST_OP_NAME, list, 0, element);
        Self::c_text(p)
    }
    fn set_name(&mut self, list: u32, element: Element, name: &[u8]) {
        self.call_text(EE_SCENARIO_LIST_OP_SET_NAME, list, element, name);
    }
    fn id(&mut self, list: u32, element: Element) -> c_ulong {
        self.call(EE_SCENARIO_LIST_OP_ID, list, 0, element) as usize as c_ulong
    }
    fn set_id(&mut self, list: u32, element: Element, id: c_ulong) {
        self.call(EE_SCENARIO_LIST_OP_SET_ID, list, id as usize, element);
    }
    fn condition(&mut self, element: Element) -> &mut EeScenarioConditionFields {
        let p = self.call(EE_SCENARIO_LIST_OP_FIELDS, EE_SCENARIO_LIST_CONDITIONS, 0, element);
        // SAFETY: the condition's fields record, borrowed until the next operation (`&mut self`)
        unsafe { &mut *(p as *mut EeScenarioConditionFields) }
    }
    fn effect(&mut self, element: Element) -> &mut EeScenarioEffectFields {
        let p = self.call(EE_SCENARIO_LIST_OP_FIELDS, EE_SCENARIO_LIST_EFFECTS, 0, element);
        // SAFETY: as `condition`
        unsafe { &mut *(p as *mut EeScenarioEffectFields) }
    }
    fn object(&mut self, element: Element) -> &mut EeScenarioObjectFields {
        let p = self.call(EE_SCENARIO_LIST_OP_FIELDS, EE_SCENARIO_LIST_OBJECTS, 0, element);
        // SAFETY: as `condition`
        unsafe { &mut *(p as *mut EeScenarioObjectFields) }
    }
    fn effect_objects(&mut self, element: Element) -> &mut [c_ulong] {
        let n = self.call(EE_SCENARIO_LIST_OP_OBJECT_COUNT, EE_SCENARIO_LIST_EFFECTS, 0, element) as usize;
        if n == 0 {
            return &mut [];
        }
        let p = self.call(EE_SCENARIO_LIST_OP_OBJECTS, EE_SCENARIO_LIST_EFFECTS, 0, element);
        // SAFETY: the effect's `mObjects` storage, `n` elements, borrowed until the next operation
        unsafe { std::slice::from_raw_parts_mut(p as *mut c_ulong, n) }
    }
    fn entry_count(&mut self, trigger: Element, effects: bool) -> usize {
        self.call(EE_SCENARIO_LIST_OP_ENTRY_COUNT, effects as u32, 0, trigger) as usize
    }
    fn entry(&mut self, trigger: Element, effects: bool, index: usize) -> &mut c_ulong {
        let p = self.call(EE_SCENARIO_LIST_OP_ENTRY, effects as u32, index, trigger);
        // SAFETY: the entry's index member, borrowed until the next operation
        unsafe { &mut *(p as *mut c_ulong) }
    }
    fn resource(&mut self, which: usize) -> Vec<u8> {
        let p = self.call(EE_SCENARIO_LIST_OP_RESOURCE, 0, which, std::ptr::null_mut());
        Self::c_text(p)
    }
    fn set_anchor(&mut self, area: Element, x: c_long, y: c_long) {
        self.call(EE_SCENARIO_LIST_OP_ANCHOR, x as u32, y as isize as usize, area);
    }
    fn load_media(&mut self, player: i32, class: c_ulong) {
        self.call(EE_SCENARIO_LIST_OP_MEDIA, player as u32, class as usize, std::ptr::null_mut());
    }
    fn out_name(&mut self, name: &[u8]) {
        self.call_text(EE_SCENARIO_LIST_OP_OUT_NAME, 0, std::ptr::null_mut(), name);
    }
    fn manager(&mut self) -> &mut EeScenarioManagerFields {
        // SAFETY: the manager's own record, borrowed until the next operation
        unsafe { &mut *self.0.manager }
    }
    fn timed(&mut self) -> &mut EeScenarioTimedQueue {
        // SAFETY: the manager's live queue, borrowed until the next operation
        unsafe { &mut *self.0.timed }
    }
}

/// `kSMConditionAlwaysTrue`, `kSMEffectNone`: the built-in condition's and effect's types.
const CONDITION_ALWAYS_TRUE: c_ulong = 0;
const EFFECT_NONE: c_ulong = 0;

/// `DeleteVectorContents(list)`: every element deleted, then the list emptied.
fn delete_all<L: Lists + ?Sized>(l: &mut L, list: u32) {
    let n = l.count(list);
    for i in 0..n {
        let e = l.at(list, i);
        l.destroy(list, e);
    }
    for i in (0..n).rev() {
        l.erase(list, i);
    }
}

/// A new element of `list` named by string `name` (an object through `SetName`), pushed.
fn builtin<L: Lists + ?Sized>(l: &mut L, list: u32, name: usize) -> Element {
    let e = l.create(list);
    let text = l.resource(name);
    l.set_name(list, e, &text);
    e
}

/// `EEScenarioManager::Reset`'s lists: the triggers emptied; the conditions only Always True, the effects only Do
/// Nothing, the objects only None, the areas only Off Map (anchor -10000) and Previous Location (anchor -1), each
/// list emptied (its elements deleted) just before its built-ins are made.
pub fn reset<L: Lists + ?Sized>(l: &mut L) {
    delete_all(l, EE_SCENARIO_LIST_TRIGGERS);
    delete_all(l, EE_SCENARIO_LIST_CONDITIONS);
    let c = builtin(l, EE_SCENARIO_LIST_CONDITIONS, EE_SCENARIO_STRING_ALWAYS_TRUE);
    l.condition(c).mType = CONDITION_ALWAYS_TRUE;
    l.push(EE_SCENARIO_LIST_CONDITIONS, c);
    delete_all(l, EE_SCENARIO_LIST_EFFECTS);
    let e = builtin(l, EE_SCENARIO_LIST_EFFECTS, EE_SCENARIO_STRING_DO_NOTHING);
    l.effect(e).mType = EFFECT_NONE;
    l.push(EE_SCENARIO_LIST_EFFECTS, e);
    delete_all(l, EE_SCENARIO_LIST_OBJECTS);
    let o = builtin(l, EE_SCENARIO_LIST_OBJECTS, EE_SCENARIO_STRING_NONE);
    l.push(EE_SCENARIO_LIST_OBJECTS, o);
    delete_all(l, EE_SCENARIO_LIST_AREAS);
    for (name, anchor) in [(EE_SCENARIO_STRING_OFF_MAP, -10000), (EE_SCENARIO_STRING_PREVIOUS_LOCATION, -1)] {
        let a = builtin(l, EE_SCENARIO_LIST_AREAS, name);
        l.set_anchor(a, anchor, anchor);
        l.push(EE_SCENARIO_LIST_AREAS, a);
    }
}

/// `kSMEffectObject`, `kSMObjectActionCreate`.
const EFFECT_OBJECT: c_ulong = 1;
const OBJECT_ACTION_CREATE: c_ulong = 0;

/// `EEScenarioManager::Prepare`'s media: for each trigger's effect entries in order, a Create Object effect whose
/// object is not picked on the map, whose owner choice names one player (`ChoosePlayer`, listed) and whose class
/// exists has that player's media of the class loaded.
pub fn prepare_media<L: Lists + ?Sized, H: Host>(l: &mut L, h: &mut H) {
    for t in 0..l.count(EE_SCENARIO_LIST_TRIGGERS) {
        let trigger = l.at(EE_SCENARIO_LIST_TRIGGERS, t);
        for k in 0..l.entry_count(trigger, true) {
            let index = *l.entry(trigger, true, k) as usize;
            let effect = l.at(EE_SCENARIO_LIST_EFFECTS, index);
            if effect.is_null() {
                continue; // the original indexed past the list
            }
            let e = *l.effect(effect);
            if e.mType != EFFECT_OBJECT || e.mObjectAction != OBJECT_ACTION_CREATE {
                continue;
            }
            let object = l.at(EE_SCENARIO_LIST_OBJECTS, e.mObject as usize);
            if object.is_null() {
                continue;
            }
            let o = *l.object(object);
            if o.mIsSelectOnMap {
                continue;
            }
            let (_, players) = choose_player_recorded(h, o.mOwner as i32, false, true);
            if players.len() == 1 && o.mClass != 0 && h.read(EE_SCENARIO_READ_CLASS_EXISTS, o.mClass as i32, 0) != 0 {
                l.load_media(players[0], o.mClass);
            }
        }
    }
}

/// `EEScenarioManager::Prepare`'s media ([`prepare_media`]).
///
/// # Safety
/// `lists` must be a valid host table over the live manager, `host` a valid scenario host, for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_prepare_media(lists: *const EeScenarioListHost, host: *const EeScenarioHost) {
    // SAFETY: the caller's contract
    let mut h = unsafe { std::ptr::read(host) };
    prepare_media(&mut HostLists(unsafe { &*lists }), &mut h);
}

/// `EEScenarioManager::Reset`'s lists ([`reset`]).
///
/// # Safety
/// `host` must be a valid host table over the live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_reset(host: *const EeScenarioListHost) {
    // SAFETY: the caller's contract
    reset(&mut HostLists(unsafe { &*host }));
}

/// `Sort*` of `list` ([`sort`]).
///
/// # Safety
/// `host` must be a valid host table over the live manager.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_sort(host: *const EeScenarioListHost, list: u32) {
    // SAFETY: the caller's contract
    sort(&mut HostLists(unsafe { &*host }), list);
}

/// `New*` of `list` ([`new`]): the new element.
///
/// # Safety
/// As [`ee_scenario_list_sort`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_new(host: *const EeScenarioListHost, list: u32) -> *mut c_void {
    // SAFETY: the caller's contract
    new(&mut HostLists(unsafe { &*host }), list)
}

/// `Move*Up` (`up`) or `Move*Down` ([`move_entry`]).
///
/// # Safety
/// As [`ee_scenario_list_sort`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_move(host: *const EeScenarioListHost, list: u32, index: c_ulong, up: bool) {
    // SAFETY: the caller's contract
    move_entry(&mut HostLists(unsafe { &*host }), list, index, up);
}

/// `Delete*` ([`delete`]): whether it deleted.
///
/// # Safety
/// As [`ee_scenario_list_sort`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_delete(host: *const EeScenarioListHost, list: u32, index: c_ulong) -> bool {
    // SAFETY: the caller's contract
    delete(&mut HostLists(unsafe { &*host }), list, index)
}

/// `Duplicate*` ([`duplicate`]): the copy.
///
/// # Safety
/// As [`ee_scenario_list_sort`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_duplicate(
    host: *const EeScenarioListHost,
    list: u32,
    index: c_ulong,
) -> *mut c_void {
    // SAFETY: the caller's contract
    duplicate(&mut HostLists(unsafe { &*host }), list, index)
}

/// `Find*ByID` ([`find`]).
///
/// # Safety
/// As [`ee_scenario_list_sort`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_find(host: *const EeScenarioListHost, list: u32, id: c_long) -> *mut c_void {
    // SAFETY: the caller's contract
    find(&mut HostLists(unsafe { &*host }), list, id)
}

/// `GetObjectIndex` ([`index_of`]).
///
/// # Safety
/// As [`ee_scenario_list_sort`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_index_of(
    host: *const EeScenarioListHost,
    list: u32,
    element: *mut c_void,
) -> c_long {
    // SAFETY: the caller's contract
    index_of(&mut HostLists(unsafe { &*host }), list, element)
}

/// The `Get*Names` list `which` ([`names`]), each name through `EE_SCENARIO_LIST_OP_OUT_NAME`.
///
/// # Safety
/// As [`ee_scenario_list_sort`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_names(host: *const EeScenarioListHost, which: u32) {
    // SAFETY: the caller's contract
    names(&mut HostLists(unsafe { &*host }), which);
}

/// `EESMObject::SetName`'s conditions ([`rename_object_conditions`]).
///
/// # Safety
/// As [`ee_scenario_list_sort`]; `object` a live scenario object.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_rename_object_conditions(
    host: *const EeScenarioListHost,
    object: *mut c_void,
) {
    // SAFETY: the caller's contract
    rename_object_conditions(&mut HostLists(unsafe { &*host }), object);
}

/// `EESMObject::SetParent` ([`set_parent`]).
///
/// # Safety
/// As [`ee_scenario_list_rename_object_conditions`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_list_set_parent(
    host: *const EeScenarioListHost,
    object: *mut c_void,
    parent: c_long,
) {
    // SAFETY: the caller's contract
    set_parent(&mut HostLists(unsafe { &*host }), object, parent);
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The lists as plain vectors of element numbers (an element's "pointer" is its number + 1).
    #[derive(Default)]
    struct Mock {
        lists: [Vec<usize>; 6],
        names: Vec<Vec<u8>>,
        ids: Vec<c_ulong>,
        conditions: Vec<EeScenarioConditionFields>,
        effects: Vec<EeScenarioEffectFields>,
        objects: Vec<EeScenarioObjectFields>,
        effect_objects: Vec<Vec<c_ulong>>,
        entries: Vec<[Vec<c_ulong>; 2]>,
        destroyed: Vec<usize>,
        out: Vec<Vec<u8>>,
        anchors: Vec<(usize, c_long, c_long)>,
        media: Vec<(i32, c_ulong)>,
        manager: EeScenarioManagerFields,
        timed: EeScenarioTimedQueue,
    }

    fn el(n: usize) -> Element {
        (n + 1) as Element
    }
    fn un(e: Element) -> usize {
        e as usize - 1
    }

    impl Mock {
        fn add(&mut self, list: u32, name: &str) -> usize {
            let n = self.names.len();
            self.names.push(name.as_bytes().to_vec());
            self.ids.push(n as c_ulong + 100);
            self.conditions.push(EeScenarioConditionFields::default());
            self.effects.push(EeScenarioEffectFields::default());
            self.objects.push(EeScenarioObjectFields::default());
            self.effect_objects.push(Vec::new());
            self.entries.push([Vec::new(), Vec::new()]);
            self.lists[list as usize].push(n);
            n
        }
        fn list_names(&self, list: u32) -> Vec<String> {
            self.lists[list as usize].iter().map(|&n| String::from_utf8(self.names[n].clone()).unwrap()).collect()
        }
    }

    impl Lists for Mock {
        fn count(&mut self, list: u32) -> usize {
            self.lists[list as usize].len()
        }
        fn at(&mut self, list: u32, index: usize) -> Element {
            self.lists[list as usize].get(index).map_or(std::ptr::null_mut(), |&n| el(n))
        }
        fn set(&mut self, list: u32, index: usize, element: Element) {
            self.lists[list as usize][index] = un(element);
        }
        fn insert(&mut self, list: u32, index: usize, element: Element) {
            self.lists[list as usize].insert(index, un(element));
        }
        fn erase(&mut self, list: u32, index: usize) {
            self.lists[list as usize].remove(index);
        }
        fn push(&mut self, list: u32, element: Element) {
            self.lists[list as usize].push(un(element));
        }
        fn create(&mut self, list: u32) -> Element {
            let n = self.add(list, "None");
            self.lists[list as usize].pop();
            self.ids[n] = 0;
            el(n)
        }
        fn copy(&mut self, list: u32, from: usize, to: Element) {
            let f = self.lists[list as usize][from];
            let t = un(to);
            self.names[t] = self.names[f].clone();
            self.ids[t] = self.ids[f];
            self.conditions[t] = self.conditions[f];
            self.effects[t] = self.effects[f];
            self.objects[t] = self.objects[f];
            self.effect_objects[t] = self.effect_objects[f].clone();
        }
        fn destroy(&mut self, _list: u32, element: Element) {
            self.destroyed.push(un(element));
        }
        fn name(&mut self, _list: u32, element: Element) -> Vec<u8> {
            self.names[un(element)].clone()
        }
        fn set_name(&mut self, _list: u32, element: Element, name: &[u8]) {
            self.names[un(element)] = name.to_vec();
        }
        fn id(&mut self, _list: u32, element: Element) -> c_ulong {
            self.ids[un(element)]
        }
        fn set_id(&mut self, _list: u32, element: Element, id: c_ulong) {
            self.ids[un(element)] = id;
        }
        fn condition(&mut self, element: Element) -> &mut EeScenarioConditionFields {
            &mut self.conditions[un(element)]
        }
        fn effect(&mut self, element: Element) -> &mut EeScenarioEffectFields {
            &mut self.effects[un(element)]
        }
        fn object(&mut self, element: Element) -> &mut EeScenarioObjectFields {
            &mut self.objects[un(element)]
        }
        fn effect_objects(&mut self, element: Element) -> &mut [c_ulong] {
            &mut self.effect_objects[un(element)]
        }
        fn entry_count(&mut self, trigger: Element, effects: bool) -> usize {
            self.entries[un(trigger)][effects as usize].len()
        }
        fn entry(&mut self, trigger: Element, effects: bool, index: usize) -> &mut c_ulong {
            &mut self.entries[un(trigger)][effects as usize][index]
        }
        fn set_anchor(&mut self, area: Element, x: c_long, y: c_long) {
            self.anchors.push((un(area), x, y));
        }
        fn load_media(&mut self, player: i32, class: c_ulong) {
            self.media.push((player, class));
        }
        fn resource(&mut self, which: usize) -> Vec<u8> {
            let names = ["Copy", "Exists", "Always True", "Do Nothing", "None", "Off Map", "Previous Location"];
            names[which].as_bytes().to_vec()
        }
        fn out_name(&mut self, name: &[u8]) {
            self.out.push(name.to_vec());
        }
        fn manager(&mut self) -> &mut EeScenarioManagerFields {
            &mut self.manager
        }
        fn timed(&mut self) -> &mut EeScenarioTimedQueue {
            &mut self.timed
        }
    }

    const T: u32 = EE_SCENARIO_LIST_TRIGGERS;
    const C: u32 = EE_SCENARIO_LIST_CONDITIONS;
    const E: u32 = EE_SCENARIO_LIST_EFFECTS;
    const O: u32 = EE_SCENARIO_LIST_OBJECTS;
    const A: u32 = EE_SCENARIO_LIST_AREAS;

    /// A scenario's built-ins: two areas, Always True, Do Nothing, the None object.
    fn base() -> Mock {
        let mut m = Mock::default();
        m.add(A, "Off Map");
        m.add(A, "Entire Map");
        m.add(C, "Always True");
        m.add(E, "Do Nothing");
        m.add(O, "None");
        m
    }

    /// A host whose choices name the owner itself (kind 7 + n: player n) and whose classes exist below 50.
    struct Players;

    impl Host for Players {
        fn read4(&mut self, op: u32, a: i32, _b: i32, _c: i32, _d: i32) -> i64 {
            match op {
                crate::scenario::host::EE_SCENARIO_READ_CAPTURE_SEED => -1,
                crate::scenario::host::EE_SCENARIO_READ_NUM_PLAYERS => 4,
                crate::scenario::host::EE_SCENARIO_READ_PLAYER_EXISTS => (0..4).contains(&a) as i64,
                EE_SCENARIO_READ_CLASS_EXISTS => (a < 50) as i64,
                _ => 0,
            }
        }
    }

    #[test]
    fn prepare_loads_the_media_of_one_player_creates() {
        let mut m = base();
        // effects: 0 Do Nothing; 1 create of object 1 (player 3, class 30); 2 create of object 2 (on the map);
        // 3 create of object 3 (class 60, no such class)
        let o1 = m.add(O, "o1");
        m.objects[o1].mOwner = 9;
        m.objects[o1].mClass = 30;
        let o2 = m.add(O, "o2");
        m.objects[o2].mOwner = 9;
        m.objects[o2].mClass = 31;
        m.objects[o2].mIsSelectOnMap = true;
        let o3 = m.add(O, "o3");
        m.objects[o3].mOwner = 9;
        m.objects[o3].mClass = 60;
        for object in 1..4 {
            let e = m.add(E, "create");
            m.effects[e].mType = EFFECT_OBJECT;
            m.effects[e].mObjectAction = OBJECT_ACTION_CREATE;
            m.effects[e].mObject = object;
        }
        let t = m.add(T, "t");
        m.entries[t][1] = vec![0, 1, 2, 3, 1];
        prepare_media(&mut m, &mut Players);
        assert_eq!(m.media, [(3, 30), (3, 30)]);
    }

    #[test]
    fn reset_leaves_the_built_ins() {
        let mut m = base();
        m.add(T, "t");
        m.add(C, "c");
        let before = m.names.len();
        reset(&mut m);
        // the seven elements deleted, list by list
        assert_eq!(m.destroyed, [5, 2, 6, 3, 4, 0, 1]);
        assert!(m.list_names(T).is_empty());
        assert_eq!(m.list_names(C), ["Always True"]);
        assert_eq!(m.list_names(E), ["Do Nothing"]);
        assert_eq!(m.list_names(O), ["None"]);
        assert_eq!(m.list_names(A), ["Off Map", "Previous Location"]);
        assert_eq!(m.anchors, [(before + 3, -10000, -10000), (before + 4, -1, -1)]);
        assert_eq!(m.conditions[before].mType, CONDITION_ALWAYS_TRUE);
    }

    #[test]
    fn new_names_ids_and_the_object_condition() {
        let mut m = base();
        m.manager.mNextAreaID = 4;
        new(&mut m, A);
        assert_eq!(m.list_names(A)[2], "<Area4>");
        let a = un(m.at(A, 2));
        assert_eq!(m.ids[a], 5);
        assert_eq!(m.manager.mNextAreaID, 5);
        new(&mut m, O);
        assert_eq!(m.manager.mNextObjectID, 1);
        assert_eq!(m.list_names(C), ["Always True", "<1:None Exists>"]);
        let c = m.at(C, 1);
        assert_eq!((m.conditions[un(c)].mType, m.conditions[un(c)].mObjectIndex), (1, 1));
        // a user condition goes before the object conditions; the triggers' entries follow
        let t = m.add(T, "<Trigger0>");
        m.entries[t][0] = vec![0, 1];
        new(&mut m, C);
        assert_eq!(m.list_names(C), ["Always True", "<Condition1>", "<1:None Exists>"]);
        assert_eq!(m.entries[t][0], vec![0, 2]);
    }

    #[test]
    fn the_first_object_gets_no_condition() {
        let mut m = Mock::default();
        new(&mut m, O);
        assert_eq!(m.count(C), 0);
        assert_eq!(m.manager.mNextObjectID, 1);
    }

    #[test]
    fn sort_keeps_the_built_ins_and_remaps() {
        let mut m = base();
        let e1 = m.add(E, "<b>");
        let e2 = m.add(E, "<a>");
        let t = m.add(T, "t");
        m.entries[t][1] = vec![0, 1, 2];
        m.effects[e1].mTrigger = 0;
        m.timed.add(1, 5, false);
        sort(&mut m, E);
        assert_eq!(m.list_names(E), ["Do Nothing", "<a>", "<b>"]);
        assert_eq!(m.entries[t][1], vec![0, 2, 1]);
        assert!(m.timed.entries.is_empty());
        let _ = e2;
        // areas: from 2; byte order ("<Area1>" before "<Area1_Copy>" before "<Area2>")
        m.add(A, "<Area2>");
        m.add(A, "<Area1_Copy>");
        m.add(A, "<Area1>");
        m.effects[e1].mTaskArea = 4;
        sort(&mut m, A);
        assert_eq!(m.list_names(A)[2..], ["<Area1>", "<Area1_Copy>", "<Area2>"]);
        assert_eq!(m.effects[e1].mTaskArea, 2);
    }

    #[test]
    fn sort_objects_sorts_their_conditions_too() {
        let mut m = base();
        new(&mut m, O);
        new(&mut m, O);
        let (o1, o2) = (un(m.at(O, 1)), un(m.at(O, 2)));
        m.names[o1] = b"z".to_vec();
        m.names[o2] = b"a".to_vec();
        let (c1, c2) = (un(m.at(C, 1)), un(m.at(C, 2)));
        m.names[c1] = b"z Exists".to_vec();
        m.names[c2] = b"a Exists".to_vec();
        sort(&mut m, O);
        assert_eq!(m.list_names(O), ["None", "a", "z"]);
        assert_eq!(m.list_names(C), ["Always True", "a Exists", "z Exists"]);
        assert_eq!(m.conditions[c2].mObjectIndex, 1);
        assert_eq!(m.conditions[c1].mObjectIndex, 2);
    }

    #[test]
    fn moves_respect_the_built_ins_and_carry_the_object_condition() {
        let mut m = base();
        m.add(A, "a");
        m.add(A, "b");
        move_entry(&mut m, A, 2, true);
        move_entry(&mut m, A, 1, false);
        move_entry(&mut m, A, 3, false);
        assert_eq!(m.list_names(A)[2..], ["a", "b"]);
        move_entry(&mut m, A, 2, false);
        assert_eq!(m.list_names(A)[2..], ["b", "a"]);
        new(&mut m, O);
        new(&mut m, O);
        let c1 = un(m.at(C, 1));
        move_entry(&mut m, O, 1, false);
        assert_eq!(m.conditions[c1].mObjectIndex, 2);
        assert_eq!(un(m.at(C, 2)), c1);
        move_entry(&mut m, O, 2, true);
        assert_eq!(m.conditions[c1].mObjectIndex, 1);
        assert_eq!(un(m.at(C, 1)), c1);
        // a condition never moves down onto an object condition
        new(&mut m, C);
        let before = m.list_names(C);
        move_entry(&mut m, C, 1, false);
        assert_eq!(m.list_names(C), before);
        move_entry(&mut m, O, 9, false);
    }

    #[test]
    fn delete_renumbers_and_drops_the_object_condition() {
        let mut m = base();
        new(&mut m, O);
        new(&mut m, O);
        let e = m.add(E, "e");
        m.effects[e].mObject = 2;
        m.effect_objects[e] = vec![1, 2];
        m.timed.add(1, 5, false);
        assert!(!delete(&mut m, O, 0));
        assert!(delete(&mut m, O, 1));
        assert_eq!(m.count(O), 2);
        assert_eq!(m.count(C), 2);
        assert_eq!(m.effects[e].mObject, 1);
        assert_eq!(m.effect_objects[e], vec![0, 1]);
        assert_eq!(m.destroyed.len(), 2);
        assert!(delete(&mut m, E, 1));
        assert!(m.timed.entries.is_empty());
        assert!(!delete(&mut m, A, 1));
        assert!(!delete(&mut m, A, 7));
    }

    #[test]
    fn duplicate_names() {
        let mut m = base();
        m.add(A, "<Area3>");
        duplicate(&mut m, A, 2);
        assert_eq!(m.list_names(A)[3], "<Area3_Copy>");
        m.add(T, "<xy");
        duplicate(&mut m, T, 0);
        assert_eq!(m.list_names(T)[1], "<xy_Copy>");
        m.add(T, "<x");
        duplicate(&mut m, T, 2);
        assert_eq!(m.list_names(T)[3], "<<x_Copy>");
        // objects: unbracketed names get no brackets; an unclosed one keeps its opening bracket
        duplicate(&mut m, O, 0);
        assert_eq!(m.list_names(O)[1], "None_Copy");
        assert_eq!(m.list_names(C)[1], "None_Copy Exists");
        let o = m.add(O, "<ab");
        duplicate(&mut m, O, 2);
        assert_eq!(m.list_names(O)[3], "<<ab_Copy>");
        let _ = o;
        // two characters are not stripped; multi-byte names count characters
        m.add(E, "<>");
        duplicate(&mut m, E, 1);
        assert_eq!(m.list_names(E)[2], "<<>_Copy>");
        m.add(E, "<é>");
        duplicate(&mut m, E, 3);
        assert_eq!(m.list_names(E)[4], "<é_Copy>");
    }

    #[test]
    fn names_with_debug_ids_and_the_object_conditions_renamed() {
        let mut m = base();
        new(&mut m, O);
        new(&mut m, C);
        names(&mut m, EE_SCENARIO_NAMES_USER_CONDITIONS);
        assert_eq!(m.out, vec![b"<Condition1>".to_vec()]);
        m.out.clear();
        m.manager.mShowDebugIDs = true;
        names(&mut m, EE_SCENARIO_NAMES_CONDITIONS);
        let out: Vec<String> = m.out.iter().map(|s| String::from_utf8(s.clone()).unwrap()).collect();
        assert_eq!(out, ["[102]Always True", "[2]<Condition1>", "[1]<None Exists>"]);
        m.out.clear();
        names(&mut m, EE_SCENARIO_NAMES_USER_AREAS);
        assert!(m.out.is_empty());
    }

    #[test]
    fn parents_never_loop_and_conditions_follow_a_rename() {
        let mut m = base();
        new(&mut m, O);
        new(&mut m, O);
        new(&mut m, O);
        let (a, b, c) = (m.at(O, 1), m.at(O, 2), m.at(O, 3));
        set_parent(&mut m, a, 2);
        set_parent(&mut m, b, 3);
        assert_eq!(m.objects[un(b)].mParent, 3);
        set_parent(&mut m, c, 1);
        assert_eq!(m.objects[un(c)].mParent, 0);
        set_parent(&mut m, c, 0);
        assert_eq!(m.objects[un(c)].mParent, 0);
        set_parent(&mut m, a, 3);
        assert_eq!(m.objects[un(a)].mParent, 3);
        set_parent(&mut m, a, 1);
        assert_eq!(m.objects[un(a)].mParent, 0);
        set_parent(&mut m, a, 77);
        assert_eq!(m.objects[un(a)].mParent, 77);
        m.names[un(b)] = b"bee".to_vec();
        rename_object_conditions(&mut m, b);
        assert_eq!(m.list_names(C)[2], "bee Exists");
        assert_eq!(m.list_names(C)[1], "<1:None Exists>");
    }

    #[test]
    fn find_and_index_of() {
        let mut m = base();
        let o = m.add(O, "x");
        m.ids[o] = 7;
        assert_eq!(find(&mut m, O, 7), el(o));
        assert!(find(&mut m, O, 8).is_null());
        assert_eq!(index_of(&mut m, O, el(o)), 1);
        assert_eq!(index_of(&mut m, O, el(99)), -1);
    }

    #[test]
    fn remap_id_defaults_to_zero() {
        let map = [(1, 3), (2, 1), (3, 2)];
        let mut id: c_ulong = 2;
        remap_id(&mut id, &map);
        assert_eq!(id, 1);
        id = 9;
        remap_id(&mut id, &map);
        assert_eq!(id, 0);
    }
}
