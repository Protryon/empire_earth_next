//! The unit AI scripts whole in Rust (RUST_PLAN R5b.4, the twentieth round): `Unit AI Scripts\<name>.tai` parsed into
//! actions (states) whose triggers name the next action, the action and trigger name tables, the scripts' cache
//! (`EEUnitAction.cpp` 0x22a509..0x22d95f: `EEGetScriptToken`, `EEStripScript`, `RegisterAll` for both tables,
//! `GetActionID`/`GetTriggerID` and the names, `EELoadScript`, `AddTrigger`, `Link`, `GetScript`, `DeleteScripts`).
//!
//! An action is a Rust object at a stable address (`Box`, kept until `DeleteScripts`) whose first field is its id, the
//! one member C++ reads (`EEUnitAction::mType`: a unit's `mCommand` points at it); its triggers are read by the AI step
//! (ai.rs) straight from here. The script file's bytes come from the C++ (the data path's archive-aware `FindFile`).
//!
//! The parser is the original's, byte for byte: the comment strip and blank folding, the quote-aware tokens, the
//! `anyof(`/`allof(` lists re-read with `", "` after rewinding the cursor, the `true(`/`false(` names cut as
//! `UString::Copy` cut them (start after the parenthesis, `length - 6` characters, ending at the token's end), the
//! includes' names (`#include("X.tai")`: from the 11th character, `length - 16` characters), a `remove()` token
//! ignored, a single-event trigger replacing (or, with no actions, removing) the action's trigger on that event. The
//! name tables keep the first registration of a name and of an id (`std::map::insert`): two triggers share 12007.

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

use crate::ai::EeUnitsTrigger;

include!("script_names.rs");

/// `kActionIdle`
const ACTION_IDLE: i32 = 5000;

/// The action id of a name (0 unknown): the first registration.
pub fn action_id(name: &[u8]) -> i32 {
    lookup_id(&ACTION_NAMES, name)
}

/// The trigger id of a name (0 unknown).
pub fn trigger_id(name: &[u8]) -> i32 {
    lookup_id(&TRIGGER_NAMES, name)
}

fn lookup_id(table: &[(&[u8], u32)], name: &[u8]) -> i32 {
    table.iter().find(|(n, _)| &n[..n.len() - 1] == name).map_or(0, |(_, v)| *v as i32)
}

/// The NUL-terminated name of an action id ("" unknown).
pub fn action_name(id: u32) -> &'static [u8] {
    ACTION_NAMES.iter().find(|(_, v)| *v == id).map_or(b"\0", |(n, _)| n)
}

/// One trigger: its events (all of or any of them), the actions it names (resolved by the link).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Trigger {
    pub conditions: Vec<i64>,
    pub any_of: bool,
    pub all_of: bool,
    pub true_id: i32,
    pub false_id: i32,
    pub true_action: u64,
    pub false_action: u64,
}

/// One action: its id first (C++ reads `mType` there), then its triggers.
#[repr(C)]
#[derive(Debug)]
pub struct Action {
    pub kind: u32,
    pub triggers: Vec<Trigger>,
}

fn new_action(kind: i32) -> u64 {
    Box::into_raw(Box::new(Action { kind: kind as u32, triggers: Vec::new() })) as u64
}

/// # Safety
/// `action` is an action of a live script.
unsafe fn action_mut<'a>(action: u64) -> &'a mut Action {
    // SAFETY: the caller's contract
    unsafe { &mut *(action as *mut Action) }
}

/// A script: its actions by id.
#[derive(Debug, Default)]
pub struct Script {
    pub actions: BTreeMap<i32, u64>,
}

struct Scripts {
    by_name: BTreeMap<Vec<u8>, u64>,
}

static SCRIPTS: Mutex<Scripts> = Mutex::new(Scripts { by_name: BTreeMap::new() });

/// `EEStripScript`: the comments (a slash to the line's end) out, white space folded into single blanks (none
/// leading), a comma before the closing bracket of each `anyof(`/`allof(` list.
pub fn strip(text: &[u8]) -> Vec<u8> {
    let end = text.iter().position(|&c| c == 0).unwrap_or(text.len());
    let text = &text[..end];
    let mut out = Vec::with_capacity(text.len());
    let (mut comment, mut blank, mut in_list) = (false, false, false);
    let mut i = 0;
    while i < text.len() {
        let c = text[i];
        if comment {
            if c == b'\n' {
                comment = false;
            }
            i += 1;
        } else if c == b'/' {
            comment = true;
            i += 1;
        } else if c == b'\r' || c == b'\n' || c == b' ' || c == b'\t' {
            if !blank && !out.is_empty() {
                out.push(b' ');
                blank = true;
            }
            i += 1;
        } else {
            if text[i..].starts_with(b"anyof(") || text[i..].starts_with(b"allof(") {
                in_list = true;
            }
            if c == b')' && in_list {
                out.push(b',');
                in_list = false;
            }
            out.push(c);
            i += 1;
            blank = false;
        }
    }
    out
}

/// `EEGetScriptToken`: the next token up to one of the delimiters outside quotes (the delimiter consumed); `None` at
/// the text's end with an empty token.
pub fn token(delimiters: &[u8], text: &[u8], cursor: &mut usize) -> Option<Vec<u8>> {
    let mut out = Vec::new();
    let mut quoted = false;
    while *cursor < text.len() && text[*cursor] != 0 {
        let c = text[*cursor];
        if delimiters.contains(&c) && !quoted {
            *cursor += 1;
            break;
        }
        if c == b'"' {
            quoted = !quoted;
        }
        out.push(c);
        *cursor += 1;
    }
    (!out.is_empty()).then_some(out)
}

/// `UString::Find`: the first position of `needle`, else the length.
fn find(hay: &[u8], needle: &[u8]) -> usize {
    if needle.len() > hay.len() {
        return hay.len();
    }
    (0..=hay.len() - needle.len()).find(|&i| hay[i..].starts_with(needle)).unwrap_or(hay.len())
}

/// `UString::Copy(token, start, count, 0, false)` as the parser uses it: from `start`, `count` characters (a
/// negative count wraps), ending at the token's end.
fn cut(token: &[u8], start: usize, count: isize) -> Vec<u8> {
    if start > token.len() {
        return Vec::new();
    }
    let end = if count < 0 { token.len() } else { (start + count as usize).min(token.len()) };
    token[start..end.max(start)].to_vec()
}

/// `EEUnitAction::AddTrigger`: a single-event any-of trigger replaces the action's triggers on that one event (or,
/// with no actions, removes them); otherwise it is appended.
pub fn add_trigger(triggers: &mut Vec<Trigger>, t: Trigger) {
    let mut replaced = false;
    if t.any_of && t.conditions.len() == 1 {
        let mut i = 0;
        while i < triggers.len() {
            let e = &triggers[i];
            if !e.any_of || e.conditions.len() != 1 || e.conditions[0] != t.conditions[0] {
                i += 1;
                continue;
            }
            replaced = true;
            if t.true_id == 0 && t.false_id == 0 {
                triggers.remove(i);
            } else {
                triggers[i] = t.clone();
                i += 1;
            }
        }
    }
    if !replaced {
        triggers.push(t);
    }
}

/// Where the parser gets a script's text.
pub trait ScriptSource {
    /// the bytes of `Unit AI Scripts\<name>.tai`, `None` when it is missing or does not open
    fn read(&mut self, name: &[u8]) -> Option<Vec<u8>>;
}

fn action_of(script: &mut Script, id: i32) -> u64 {
    *script.actions.entry(id).or_insert_with(|| new_action(id))
}

/// `EELoadScript`: the file's actions and triggers into the script (the includes first where they stand).
pub fn load(name: &[u8], script: &mut Script, source: &mut dyn ScriptSource) {
    let Some(bytes) = source.read(name) else { return };
    let text = strip(&bytes);
    let mut cursor = 0usize;
    let mut delimiters: &[u8] = b" ";
    let mut state = 0;
    let mut pending: Vec<u8> = Vec::new();
    let mut action = 0u64;
    let mut trigger = Trigger::default();
    let mut include: Vec<u8> = Vec::new();
    let mut action_name: Vec<u8> = Vec::new();
    loop {
        let t = if !pending.is_empty() {
            std::mem::take(&mut pending)
        } else {
            match token(delimiters, &text, &mut cursor) {
                Some(t) => t,
                None => break,
            }
        };
        match state {
            0 => {
                if find(&t, b"#include(") < t.len() {
                    if 10 <= t.len() + 1 {
                        include = cut(&t, 10, t.len() as isize - 16);
                    }
                    load(&include, script, source);
                } else {
                    action = action_of(script, action_id(&t));
                    state = 1;
                }
            }
            1 => {
                if t[0] == b'{' {
                    trigger = Trigger { any_of: true, ..Default::default() };
                    state = 2;
                }
            }
            2 => {
                trigger.any_of = find(&t, b"anyof(") < t.len();
                trigger.all_of = find(&t, b"allof(") < t.len();
                if !trigger.any_of && !trigger.all_of {
                    trigger.any_of = true;
                    trigger.conditions.push(i64::from(trigger_id(&t)));
                    state = 4;
                } else {
                    delimiters = b", ";
                    cursor = cursor.wrapping_sub(t.len() - 5);
                    state = 3;
                }
            }
            3 => {
                if t[0] == b')' {
                    delimiters = b" ";
                    state = 4;
                } else {
                    trigger.conditions.push(i64::from(trigger_id(&t)));
                }
            }
            _ => {
                let at = find(&t, b"true(");
                if at < t.len() {
                    action_name = cut(&t, at + 5, t.len() as isize - 6);
                    trigger.true_id = action_id(&action_name);
                } else {
                    let at = find(&t, b"false(");
                    if at < t.len() {
                        action_name = cut(&t, at + 6, t.len() as isize - 7);
                        trigger.false_id = action_id(&action_name);
                    } else if find(&t, b"remove()") >= t.len() {
                        // SAFETY: the script's own action
                        add_trigger(&mut unsafe { action_mut(action) }.triggers, std::mem::take(&mut trigger));
                        if t[0] == b'}' {
                            state = 0;
                        } else {
                            trigger = Trigger { any_of: true, ..Default::default() };
                            pending = t;
                            state = 2;
                        }
                    }
                }
            }
        }
    }
    let _ = action_name;
}

/// `EEUnitAction::Link` over the script: every trigger's actions resolved, an empty action made for one missing.
pub fn link(script: &mut Script) {
    let keys: Vec<i32> = script.actions.keys().copied().collect();
    for k in keys {
        let a = script.actions[&k];
        // SAFETY: the script's own action
        let count = unsafe { action_mut(a) }.triggers.len();
        for i in 0..count {
            let (tid, fid) = {
                // SAFETY: as above
                let t = &unsafe { action_mut(a) }.triggers[i];
                (t.true_id, t.false_id)
            };
            if tid != 0 {
                let target = action_of(script, tid);
                // SAFETY: as above
                unsafe { action_mut(a) }.triggers[i].true_action = target;
            }
            if fid != 0 {
                let target = action_of(script, fid);
                // SAFETY: as above
                unsafe { action_mut(a) }.triggers[i].false_action = target;
            }
        }
    }
}

/// `EEUnitAction::GetScript`: the named script, loaded, linked and kept on first use (an idle action added when it
/// has none). The handle is the script's address.
pub fn get_script(name: &[u8], source: &mut dyn ScriptSource) -> u64 {
    let mut scripts = SCRIPTS.lock().unwrap_or_else(|e| e.into_inner());
    if let Some(s) = scripts.by_name.get(name) {
        return *s;
    }
    let mut script = Script::default();
    load(name, &mut script, source);
    link(&mut script);
    script.actions.entry(ACTION_IDLE).or_insert_with(|| new_action(ACTION_IDLE));
    let handle = Box::into_raw(Box::new(script)) as u64;
    scripts.by_name.insert(name.to_vec(), handle);
    handle
}

/// `EEUnitClass::FindAction`'s lookup: the script's action of an id (0: none).
pub fn find_action(script: u64, id: i32) -> u64 {
    if script == 0 {
        return 0;
    }
    // SAFETY: a handle from `get_script`, live until `delete_all`
    let s = unsafe { &*(script as *const Script) };
    s.actions.get(&id).copied().unwrap_or(0)
}

/// The trigger `index` of an action as the AI step reads it.
pub fn trigger_view(action: u64, index: u32) -> Option<(EeUnitsTrigger, Vec<i64>)> {
    if action == 0 {
        return None;
    }
    // SAFETY: an action of a live script (a unit's command)
    let a = unsafe { &*(action as *const Action) };
    let t = a.triggers.get(index as usize)?;
    let view = EeUnitsTrigger {
        conditions: std::ptr::null(),
        count: t.conditions.len() as u32,
        all_of: u8::from(t.all_of),
        any_of: u8::from(t.any_of),
        true_id: i64::from(t.true_id),
        false_id: i64::from(t.false_id),
        true_action: t.true_action,
        false_action: t.false_action,
    };
    Some((view, t.conditions.clone()))
}

/// `EEUnitAction::DeleteScripts`: every script and its actions freed.
pub fn delete_all() {
    let mut scripts = SCRIPTS.lock().unwrap_or_else(|e| e.into_inner());
    for (_, handle) in std::mem::take(&mut scripts.by_name) {
        // SAFETY: the handles `get_script` made, freed once
        let script = unsafe { Box::from_raw(handle as *mut Script) };
        for (_, a) in script.actions {
            // SAFETY: as above
            drop(unsafe { Box::from_raw(a as *mut Action) });
        }
    }
}

// ---- the C++ seam

/// The C++ reader of a script's file: `read(ctx, name, len, sink)` pushes the bytes (`ee_units_script_push`) and
/// answers whether the file was found and opened.
#[repr(C)]
pub struct EeUnitsScriptReader {
    pub ctx: *mut c_void,
    pub read: unsafe extern "C" fn(*mut c_void, *const u8, usize, *mut c_void) -> bool,
}

struct FfiSource<'a>(&'a EeUnitsScriptReader);

impl ScriptSource for FfiSource<'_> {
    fn read(&mut self, name: &[u8]) -> Option<Vec<u8>> {
        let mut bytes: Vec<u8> = Vec::new();
        // SAFETY: the reader's contract; the sink is the Vec, live for the call
        let ok = unsafe { (self.0.read)(self.0.ctx, name.as_ptr(), name.len(), (&mut bytes as *mut Vec<u8>).cast()) };
        ok.then_some(bytes)
    }
}

/// The reader appends a file's bytes to the sink it was handed.
///
/// # Safety
/// `sink` is the sink of the read in progress; `data` points to `len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_units_script_push(sink: *mut c_void, data: *const u8, len: usize) {
    // SAFETY: the caller's contract
    let (bytes, data) = unsafe { (&mut *sink.cast::<Vec<u8>>(), std::slice::from_raw_parts(data, len)) };
    bytes.extend_from_slice(data);
}

/// `EEUnitAction::GetScript(name)`: the script's handle.
///
/// # Safety
/// `name` points to `len` bytes; `reader` to a valid reader.
#[no_mangle]
pub unsafe extern "C" fn ee_units_script_get(name: *const u8, len: usize, reader: *const EeUnitsScriptReader) -> u64 {
    // SAFETY: the caller's contract
    let (name, reader) = unsafe { (std::slice::from_raw_parts(name, len), &*reader) };
    get_script(name, &mut FfiSource(reader))
}

/// The script's action of an id (0: none).
#[no_mangle]
pub extern "C" fn ee_units_script_find(script: u64, id: i32) -> u64 {
    find_action(script, id)
}

/// Every script freed (the game's end).
#[no_mangle]
pub extern "C" fn ee_units_script_delete_all() {
    delete_all()
}

/// The action id of a NUL-terminated name (0: unknown).
///
/// # Safety
/// `name` is a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_units_script_action_id(name: *const std::ffi::c_char) -> i32 {
    // SAFETY: the caller's contract
    action_id(unsafe { std::ffi::CStr::from_ptr(name) }.to_bytes())
}

/// The NUL-terminated name of an action id ("" unknown), static.
#[no_mangle]
pub extern "C" fn ee_units_script_action_name(id: u32) -> *const std::ffi::c_char {
    action_name(id).as_ptr().cast()
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Files(Vec<(&'static str, &'static str)>);

    impl ScriptSource for Files {
        fn read(&mut self, name: &[u8]) -> Option<Vec<u8>> {
            self.0.iter().find(|(n, _)| n.as_bytes() == name).map(|(_, t)| t.as_bytes().to_vec())
        }
    }

    fn kind(action: u64) -> u32 {
        if action == 0 {
            0
        } else {
            // SAFETY: a test script's action
            unsafe { &*(action as *const Action) }.kind
        }
    }

    fn triggers(script: &Script, id: i32) -> Vec<Trigger> {
        // SAFETY: a test script's action
        unsafe { &*(script.actions[&id] as *const Action) }.triggers.clone()
    }

    fn free(script: Script) {
        for (_, a) in script.actions {
            // SAFETY: the test's own actions
            drop(unsafe { Box::from_raw(a as *mut Action) });
        }
    }

    #[test]
    fn the_name_tables_keep_the_first_registration() {
        assert_eq!(action_id(b"Idle"), 5000);
        assert_eq!(action_id(b"MoveToRepairSite"), 8017);
        assert_eq!(action_id(b"NoSuchAction"), 0);
        assert_eq!(trigger_id(b"BuildingDestroyed"), 2001);
        assert_eq!(action_name(5000), b"Idle\0");
        assert_eq!(action_name(123_456), b"\0");
        // two triggers share 12007: the first registered keeps the id's name
        assert_eq!(trigger_id(b"GarrisonUpgrade"), 12007);
        assert_eq!(trigger_id(b"LoadableUnitLoaded"), 12007);
        let first = TRIGGER_NAMES.iter().find(|(_, v)| *v == 12007).unwrap().0;
        assert_eq!(first, b"GarrisonUpgrade\0");
    }

    #[test]
    fn every_name_is_a_header_enumerator_comment() {
        let root = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../../../include/ee");
        let text =
            String::from_utf8_lossy(&std::fs::read(root.join("EEUnitAction.h")).expect("the header")).into_owned();
        for (name, id) in ACTION_NAMES.iter().chain(TRIGGER_NAMES.iter()) {
            let n = std::str::from_utf8(&name[..name.len() - 1]).unwrap();
            assert!(text.contains(&format!("Register(\"{n}\", {id})")), "{n} {id}");
        }
    }

    #[test]
    fn strip_and_tokens() {
        let t = strip(b"// head\n\tA\r\n{\n  anyof(X,  Y) true(B) // c\n}\n");
        assert_eq!(t, b"A { anyof(X, Y,) true(B) } ");
        let mut c = 0;
        assert_eq!(token(b" ", b"#include(\"a b.tai\") X", &mut c).unwrap(), b"#include(\"a b.tai\")");
        assert_eq!(token(b" ", b"#include(\"a b.tai\") X", &mut c).unwrap(), b"X");
        assert_eq!(token(b" ", b"#include(\"a b.tai\") X", &mut c), None);
        assert_eq!(cut(b"true(Idle)", 5, 4), b"Idle");
        // a token that does not start with the keyword: the cut runs to the token's end
        assert_eq!(cut(b"xtrue(Idle)", 6, 5), b"Idle)");
    }

    #[test]
    fn a_script_with_includes_lists_replacements_and_removals() {
        let mut files = Files(vec![
            (
                "main",
                "#include(\"base.tai\")\nIdle\n{\n  allof(BuildingDestroyed,AlwaysTrue) true(MoveToRepairSite)\n  \
                 AlwaysTrue false(RepairBuilding)\n}\nRepairBuilding\n{\n  BuildingDestroyed remove()\n  \
                 BuildingRepaired\n}\n",
            ),
            ("base", "RepairBuilding\n{\n  BuildingDestroyed true(Idle)\n  BuildingRepaired true(Idle) \n}\n"),
        ]);
        let mut s = Script::default();
        load(b"main", &mut s, &mut files);
        link(&mut s);
        assert_eq!(s.actions.keys().copied().collect::<Vec<_>>(), [4002, 5000, 8017]);
        let idle = triggers(&s, 5000);
        assert_eq!(idle.len(), 2);
        assert!(idle[0].all_of && !idle[0].any_of);
        assert_eq!(idle[0].conditions, [2001, i64::from(trigger_id(b"AlwaysTrue"))]);
        assert_eq!((idle[0].true_id, kind(idle[0].true_action)), (8017, 8017));
        assert_eq!((idle[1].false_id, kind(idle[1].false_action)), (4002, 4002));
        // the include's BuildingDestroyed trigger: "remove()" is skipped, so the next trigger closes it with no
        // actions and removes it; BuildingRepaired with no actions removes the include's too
        let repair = triggers(&s, 4002);
        assert!(repair.is_empty(), "{repair:?}");
        // the made action (MoveToRepairSite) has no triggers
        assert!(triggers(&s, 8017).is_empty());
        free(s);
    }

    #[test]
    fn a_single_event_trigger_replaces_the_one_before() {
        let one = |c: i64, t: i32| Trigger { conditions: vec![c], any_of: true, true_id: t, ..Default::default() };
        let mut v =
            vec![one(1, 10), Trigger { conditions: vec![1, 2], any_of: true, ..Default::default() }, one(1, 11)];
        add_trigger(&mut v, one(1, 12));
        assert_eq!(v.iter().map(|t| t.true_id).collect::<Vec<_>>(), [12, 0, 12]);
        add_trigger(&mut v, one(1, 0));
        assert_eq!(v.len(), 1);
        add_trigger(&mut v, one(3, 0));
        assert_eq!(v.len(), 2);
    }

    #[test]
    fn the_cache_the_idle_action_and_the_lookup() {
        let mut files = Files(vec![("cached test script", "Idle\n{\n  AlwaysTrue true(Idle)\n}\n")]);
        let h = get_script(b"cached test script", &mut files);
        assert_eq!(get_script(b"cached test script", &mut Files(vec![])), h);
        let idle = find_action(h, 5000);
        assert_eq!(kind(idle), 5000);
        let (view, ids) = trigger_view(idle, 0).unwrap();
        assert_eq!((view.true_action, view.any_of, ids.len()), (idle, 1, 1));
        assert!(trigger_view(idle, 1).is_none());
        assert_eq!(find_action(h, 1), 0);
        // a missing file: only the idle action
        let empty = get_script(b"no such script", &mut Files(vec![]));
        assert_ne!(find_action(empty, 5000), 0);
    }
}
