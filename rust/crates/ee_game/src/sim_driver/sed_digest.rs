//! TEST ONLY: the scripted scenario editor's digests (was EESimEditor.cpp's): the save's chunks and a saved file's,
//! the world's, the scenario lists' state and texts, the area screen, the trigger screens' controls, the form's
//! controls, the players, the terrain tab, the tech lists, the form's own state, the trigger-only save's report, the
//! open trigger and object. Each reads the C++ objects through the editor host ([`super::sed_host`]); the lines and
//! their formats are the C++'s.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use crate::gamecpp as gc;
use std::ffi::{c_long, c_ulong};

use ee_render::gx::cformat::format_g;
use ee_sim::scenario::fields::{EeScenarioConditionFields, EeScenarioEffectFields};
use ee_sim::scenario::trigger_members::{
    ee_scenario_trigger_member, EE_SCENARIO_TRM_SET, EE_SCENARIO_TRM_SET_OPERATOR,
};

use super::digest::{hex, Hash};
use super::sed_host::{self as x, h, ht, num, sel, EeSimDriverSed as O};

/// `EESMTrigger::SetCondition(index, not, condition)` (list 0) / `SetEffect(index, delay, effect)` (list 1).
fn trigger_set(trigger: usize, list: i64, a: [i64; 3]) {
    let second = if list == 0 { (a[1] != 0) as i64 } else { a[1] as c_long as c_ulong as i64 };
    ee_scenario_trigger_member(
        trigger as *mut _,
        EE_SCENARIO_TRM_SET,
        list,
        a[0] as c_ulong as i64,
        second,
        a[2] as c_ulong as i64,
    );
}

/// `EESMTrigger::SetConditionOperator(index, operator)` (list 0) / `SetEffectOperator(index, type)` (list 1).
fn trigger_set_operator(trigger: usize, list: i64, a: [i64; 2]) {
    ee_scenario_trigger_member(
        trigger as *mut _,
        EE_SCENARIO_TRM_SET_OPERATOR,
        list,
        a[0] as c_ulong as i64,
        a[1] as c_long as i64,
        0,
    );
}

/// The save's chunks that differ by run though the editor writes none of them: the checksum (1, over the players),
/// the players (5: per player a few words the computer players' AI thread and timers write), the multiplayer helper
/// (8: the server's times) and the environment manager (12: one byte of its state).
pub fn chunk_by_run(tag: i64) -> bool {
    matches!(tag, 1 | 5 | 8 | 12)
}

/// A chunk's line after "chunk ": its tag, its result, its length and hash (or "differs by run").
pub fn chunk_text(tag: i64, result: i64, data: &[u8]) -> String {
    if chunk_by_run(tag) {
        return format!("{tag} result={result} (differs by run)");
    }
    let mut hash = Hash::default();
    hash.bytes(data);
    format!("{tag} result={result} bytes={} {}", data.len(), hex(hash.value))
}

/// `%g` of a float (the C++ passed every float to printf as a double).
pub fn g(v: f32) -> String {
    format_g(v as f64)
}

/// `std::to_string(float)`: `%f`.
pub fn f6(v: f32) -> String {
    format!("{:.6}", v as f64)
}

/// A rectangle's four sides as "%g,%g,%g,%g" (left, top, right, bottom).
pub fn rect_text(r: [f32; 4]) -> String {
    format!("{},{},{},{}", g(r[0]), g(r[1]), g(r[2]), g(r[3]))
}

/// `Colour`: "%g,%g,%g".
pub fn colour(c: [f32; 3]) -> String {
    format!("{},{},{}", g(c[0]), g(c[1]), g(c[2]))
}

/// A line count's selection: the index, or "none".
fn selection_text(s: Option<c_ulong>) -> String {
    s.map_or_else(|| "none".to_string(), |v| v.to_string())
}

// ------------------------------------------------------------------------------------------------ the save

/// The save's chunk objects (EESaveLoad.cpp's BuildSaveGameChunks), each deleted after its line.
fn chunk_list() -> (usize, i64) {
    let list = h(O::ChunksNew, 0, &[]) as usize;
    h(O::ChunksBuild, list, &[]);
    (list, h(O::ChunksCount, list, &[]))
}

/// The save's body: the game's save chunks (each written as WriteSaveGameChunks writes it) -> one line per chunk:
/// its tag, length and hash. The header (the date, the time) is not written.
pub fn save_digest(out: &mut Vec<String>) {
    let (list, count) = chunk_list();
    for i in 0..count {
        let persistent = h(O::ChunksAt, list, &[i]) as usize;
        let chunk = h(O::ChunkNew, 0, &[]) as usize;
        let file = h(O::MemNew, 0, &[0x400, 1, 1000]) as usize;
        let mut result = h(O::ChunkSavePersistent, chunk, &[persistent as i64]);
        if result == 0 {
            result = h(O::ChunkSaveToFile, chunk, &[file as i64]);
        }
        let data = x::mem_bytes(file, 0, x::mem_length(file));
        out.push(format!("  chunk {}", chunk_text(h(O::PersistentTag, persistent, &[]), result, &data)));
        h(O::PersistentFree, persistent, &[]);
        h(O::MemFree, file, &[]);
        h(O::ChunkFree, chunk, &[]);
    }
    h(O::ChunksFree, list, &[]);
}

/// A saved file's chunks: the file read whole, its header loaded, then each chunk as written (tags in the order of
/// the save's list).
pub fn file_digest(name: &str, out: &mut Vec<String>) {
    let spec = x::scenario_spec(name);
    if h(O::SpecOpenRead, spec, &[]) != 0 {
        out.push("  missing".into());
        h(O::SpecFree, spec, &[]);
        return;
    }
    let memory = h(O::MemNew, 0, &[1000, 1, 1]) as usize;
    let header = h(O::HeaderNew, 0, &[]) as usize;
    let result = h(O::MemLoad, memory, &[spec as i64]);
    h(O::SpecClose, spec, &[]);
    let done = |out: &mut Vec<String>, line: &str| {
        out.push(line.into());
        gc::h(gc::O::SsDeleteHeader, header, &[]);
        h(O::MemFree, memory, &[]);
        h(O::SpecFree, spec, &[]);
    };
    if result != 0 || h(O::HeaderLoad, header, &[memory as i64]) != 0 {
        done(out, "  unreadable");
        return;
    }
    let (list, count) = chunk_list();
    let length = x::mem_length(memory);
    for i in 0..count {
        let persistent = h(O::ChunksAt, list, &[i]) as usize;
        let start = x::mem_position(memory);
        let chunk = h(O::ChunkNew, 0, &[]) as usize;
        let read = if (start as c_ulong) < length { h(O::ChunkLoadFromFile, chunk, &[memory as i64]) } else { 9 };
        let end = x::mem_position(memory);
        let data = x::mem_bytes(memory, start as usize, end.wrapping_sub(start) as c_ulong);
        out.push(format!("  chunk {}", chunk_text(h(O::PersistentTag, persistent, &[]), read, &data)));
        h(O::PersistentFree, persistent, &[]);
        h(O::ChunkFree, chunk, &[]);
    }
    h(O::ChunksFree, list, &[]);
    let end = x::mem_position(memory);
    done(out, &format!("  rest {}", length.wrapping_sub(end as c_ulong)));
}

/// The world as the editor left it: the map's tiles and mesh, every player's units, the editor's selection and paint
/// area, the save's body. Under the world's write lock (a step).
pub fn digest(label: &str, out: &mut Vec<String>) {
    out.push(format!("digest {label}"));
    if !super::digest::world_digest(h(O::MapMesh, 0, &[]) as usize, out) {
        return;
    }
    let e = x::editor();
    let ids: String =
        (0..h(O::SelectedCount, e, &[])).map(|i| format!(" {}", h(O::SelectedUnitId, e, &[i]) as c_ulong)).collect();
    out.push(format!("  selection{ids}"));
    out.push(format!("  paint area {}", rect_text(x::rect(h(O::BrushRect, x::brush_group(), &[]) as usize))));
    save_digest(out);
}

// ------------------------------------------------------------------------------------------------ the scenario lists

/// The scenario manager's list in `kind` (triggers, conditions, effects, pending effects, objects, areas): its
/// elements.
pub fn sm_list(count: O, at: O) -> Vec<usize> {
    let m = x::sm();
    (0..h(count, m, &[])).map(|i| h(at, m, &[i]) as usize).collect()
}

fn effect_fields(e: usize) -> &'static mut EeScenarioEffectFields {
    // SAFETY: the effect's numbers (its base class) under the world's write lock (a step)
    unsafe { &mut *(h(O::EffectFields, e, &[]) as *mut EeScenarioEffectFields) }
}

fn condition_fields(c: usize) -> &'static mut EeScenarioConditionFields {
    // SAFETY: as effect_fields
    unsafe { &mut *(h(O::ConditionFields, c, &[]) as *mut EeScenarioConditionFields) }
}

fn object_fields(o: usize) -> &'static mut ee_sim::scenario::fields::EeScenarioObjectFields {
    // SAFETY: as effect_fields
    unsafe { &mut *(h(O::ObjectFields, o, &[]) as *mut ee_sim::scenario::fields::EeScenarioObjectFields) }
}

fn trigger_fields(t: usize) -> &'static mut ee_sim::scenario::fields::EeScenarioTriggerFields {
    // SAFETY: as effect_fields
    unsafe { &mut *(h(O::TriggerFields, t, &[]) as *mut ee_sim::scenario::fields::EeScenarioTriggerFields) }
}

fn effect_objects(e: usize) -> Vec<c_ulong> {
    (0..h(O::EffectObjectCount, e, &[])).map(|k| h(O::EffectObjectAt, e, &[k]) as c_ulong).collect()
}

/// The trigger tab's lists, every element's name, id and index references, the trigger entries, the timed effects
/// and the name lists the screens show.
pub fn scenario_list_state(out: &mut Vec<String>) {
    let m = x::sm();
    for (i, &t) in sm_list(O::SmTriggerCount, O::SmTriggerAt).iter().enumerate() {
        let mut s = format!("  trigger {i} {} id {} c", ht(O::TriggerName, t, 0), num(trigger_fields(t).mID));
        for k in 0..h(O::TriggerConditionCount, t, &[]) {
            s += &format!(" {}", num(h(O::WrapperValue, h(O::TriggerConditionAt, t, &[k]) as usize, &[]) as c_ulong));
        }
        s += " e";
        for k in 0..h(O::TriggerEffectCount, t, &[]) {
            s +=
                &format!(" {}", num(h(O::WrapperEffectIndex, h(O::TriggerEffectAt, t, &[k]) as usize, &[]) as c_ulong));
        }
        out.push(s);
    }
    for (i, &c) in sm_list(O::SmConditionCount, O::SmConditionAt).iter().enumerate() {
        let f = condition_fields(c);
        out.push(format!(
            "  condition {i} {} id {} type {} object {} trigger {}",
            ht(O::ConditionName, c, 0),
            num(f.mID),
            num(f.mType),
            num(f.mObjectIndex),
            num(f.mTrigger)
        ));
    }
    let effects = sm_list(O::SmEffectCount, O::SmEffectAt);
    let mut all = effects.clone();
    all.extend(sm_list(O::SmPendingCount, O::SmPendingAt));
    for (i, &e) in all.iter().enumerate() {
        let f = effect_fields(e);
        let mut s = format!(
            "{}{i} {} id {} areas {} {} {} {} objects {} {} {} {} {} trigger {} list",
            if i < effects.len() { "  effect " } else { "  pending " },
            ht(O::EffectName, e, 0),
            num(f.mID),
            num(f.mTaskArea),
            num(f.mTargetArea),
            num(f.mPlaceArea),
            num(f.mLocationArea),
            num(f.mObject),
            num(f.mLocationObject),
            num(f.mTaskObject),
            num(f.mPlaceObject),
            num(f.mTargetObject),
            num(f.mTrigger)
        );
        for o in effect_objects(e) {
            s += &format!(" {}", num(o));
        }
        out.push(s);
    }
    for (i, &o) in sm_list(O::SmObjectCount, O::SmObjectAt).iter().enumerate() {
        let f = object_fields(o);
        out.push(format!(
            "  object {i} {} id {} area {} parent {} near {}",
            ht(O::ObjectName, o, 0),
            num(f.mL64),
            num(f.mArea),
            num(f.mParent),
            num(f.mNearObject)
        ));
    }
    for (i, &a) in sm_list(O::SmAreaCount, O::SmAreaAt).iter().enumerate() {
        out.push(format!("  area {i} {} id {}", ht(O::AreaName, a, 0), num(h(O::AreaId, a, &[]) as c_ulong)));
    }
    let mut s = "  timed".to_string();
    for i in 0..h(O::SmTimedCount, m, &[]) {
        s += &format!(
            " {}@{}",
            num(h(O::SmTimedEffect, m, &[i]) as c_ulong),
            num(h(O::SmTimedTime, m, &[i]) as c_ulong)
        );
    }
    out.push(s);
    out.push(format!(
        "  next {} {} {} {} {}",
        num(h(O::SmNextTriggerId, m, &[]) as c_ulong),
        num(h(O::SmNextConditionId, m, &[]) as c_ulong),
        num(h(O::SmNextEffectId, m, &[]) as c_ulong),
        num(h(O::SmNextObjectId, m, &[]) as c_ulong),
        num(h(O::SmNextAreaId, m, &[]) as c_ulong)
    ));
    const LABELS: [&str; 9] = [
        "triggers",
        "conditions",
        "user conditions",
        "effects",
        "user effects",
        "objects",
        "user objects",
        "areas",
        "user areas",
    ];
    for debug in 0..2 {
        let old = h(O::SmShowDebugIds, m, &[]);
        h(O::SmSetShowDebugIds, m, &[debug]);
        // EE_SCENARIO_NAMES_TRIGGERS .. EE_SCENARIO_NAMES_USER_AREAS
        let names: Vec<Vec<String>> = (0..9).map(x::sm_names).collect();
        h(O::SmSetShowDebugIds, m, &[old]);
        for (k, list) in names.iter().enumerate() {
            let mut s = format!("  {}", LABELS[k]);
            for n in list {
                s += &format!(" |{n}");
            }
            out.push(s);
        }
    }
    let mut s = "  find".to_string();
    for id in 0..8i64 {
        let c = h(O::SmFindCondition, m, &[id]) as usize;
        let e = h(O::SmFindEffect, m, &[id]) as usize;
        let o = h(O::SmFindObject, m, &[id]) as usize;
        let name = |p: usize, op: O| if p != 0 { ht(op, p, 0) } else { "-".to_string() };
        s += &format!(
            " {id}:{}/{}/{}/{}",
            name(c, O::ConditionName),
            name(e, O::EffectName),
            name(o, O::ObjectName),
            if o != 0 { h(O::SmObjectIndex, m, &[o as i64]) as c_long } else { -2 }
        );
    }
    out.push(s);
}

/// Every effect's and condition's name and texts, every player's name.
pub fn text_elements(out: &mut Vec<String>) {
    for e in sm_list(O::SmEffectCount, O::SmEffectAt) {
        out.push(format!(
            "  effect {} {} |{}|{}|{}|",
            num(effect_fields(e).mID),
            ht(O::EffectName, e, 0),
            ht(O::EffectDescription, e, 0),
            ht(O::EffectMessage, e, 0),
            ht(O::EffectNewName, e, 0)
        ));
    }
    for c in sm_list(O::SmConditionCount, O::SmConditionAt) {
        out.push(format!(
            "  condition {} {} |{}|",
            num(condition_fields(c).mID),
            ht(O::ConditionName, c, 0),
            ht(O::ConditionChatText, c, 0)
        ));
    }
    let mut s = "  players".to_string();
    for p in 0..gc::h(gc::O::MmNumPlayers, 0, &[]) {
        let player = gc::h(gc::O::TbWorldPlayer, 0, &[p]) as usize;
        s += &format!(" |{}", if player != 0 { ht(O::PlayerName, player, 0) } else { "-".into() });
    }
    out.push(s);
}

/// The import files of `sm import` (each ends with a new line: a last line without one leaves the original's text
/// list a line short and ImportText reads past it); 4 is made by [`text_file_content`].
pub const TEXT_FILES: [&[u8]; 4] = [
    // 0: valid (version 1): the four media strings, the texts of three effects and a condition, two player names
    b"{1_After Math}\nAftermath text\nline two\n\n{0_Hints}\nHints \xc3\xbc\n\n{3_Instructions}\nDo this.\n\n\n\n\
{2_History}\nLong ago\n\n{5_Effect:<Effect0_Copy>}\nNew briefing\n{2_Effect:<Effect1>}\nNew dialogue\n\n\
{9_Condition:<Condition3_Copy>}\nnew chat\n\n{1_Player Name:Alice}\n{2_Player Name:Bob \xc3\xbc}\n",
    // 1: errors of every kind (validated: listed; not validated: applied up to the first)
    b"preamble before any entry\n{3_Effect:Renamed}\nRenamed class\n\n{0_Hints}\nHints two\n{3_Player Name:Carol}\n\
{0_Player Name:Zero}\n{99_Player Name:Nobody}\n{6_Condition:<Condition5>}\nchat 6\n{4_Effect:<Effect3>}\nchat 4\n\
{7_Bogus}\nbogus\n{99_Effect:x}\ny\n{0_Effect:Do Nothing}\nz\n{99_Condition:x}\ny\n{5_Condition:<Condition4>}\n\
t\n{6_Condition:Wrong}\nt\n{\xc3\xbc3_Effect:x}\nt\nx{y\na{b}c\n{a}{b}\n{}\n{2_History}\nhistory two\n",
    // 2: version 0 entries ("<id>_<name>"; a condition entry reads as an effect's)
    b"{5_<Effect0_Copy>}\nkind zero\n\n{2_Effect:<Effect1>}\nkind zero two\n{9_Condition:<Condition3_Copy>}\nxx\n",
    // 3: empty
    b"",
];

/// Import file `index`'s bytes (4: long lines: fgets reads 511 bytes at a time, so an entry can start inside a
/// line).
pub fn text_file_content(index: u64) -> Vec<u8> {
    if let Some(f) = TEXT_FILES.get(index as usize) {
        return f.to_vec();
    }
    let (a, b, c) = ("a".repeat(600), "b".repeat(511), "c".repeat(509));
    format!("{{2_History}}\n{a}\n{b}{{3_Effect:<Effect2>}}\nafter\n{c}\u{fc}\u{fc}\n{{0_Hints}}\n{b}\n\n").into_bytes()
}

/// `sm text | sm export | sm import FILE KIND VALIDATE`
pub fn scenario_text_step(w: &[String], out: &mut Vec<String>) {
    let m = x::sm();
    let op = w[1].as_str();
    if op == "text" {
        // sm text: types and texts on the effects and conditions the script left
        const MEDIA: c_ulong = 4; // kSMEffectMedia
        const OBJECT: c_ulong = 1; // kSMEffectObject
        const CHANGE_TEXT: c_ulong = 1; // kSMMediaActionChangeText
        let effects: [(c_ulong, c_ulong, &str); 4] = [
            (MEDIA, CHANGE_TEXT, "Briefing \u{fc}\nsecond line"),
            (MEDIA, 2, "Hello \u{65e5}\u{672c}"), // kSMMediaActionSendDialogue
            (OBJECT, 10, "Class X"),              // kSMObjectActionClassName
            (MEDIA, 3, "chat msg"),               // kSMMediaActionSendChatMessage
        ];
        let list = sm_list(O::SmEffectCount, O::SmEffectAt);
        for i in 1..list.len().min(5) {
            let e = list[i];
            let f = effect_fields(e);
            let (kind, action, text) = effects[i - 1];
            f.mType = kind;
            if f.mType == MEDIA {
                f.mMediaAction = action;
            } else {
                f.mObjectAction = action;
            }
            x::hs(O::EffectSetDescription, e, "desc only");
            x::hs(O::EffectSetMessage, e, "message only");
            x::hs(O::EffectSetNewName, e, "new name only");
            if f.mType == OBJECT {
                x::hs(O::EffectSetNewName, e, text);
            } else if action == CHANGE_TEXT {
                x::hs(O::EffectSetDescription, e, text);
            } else {
                x::hs(O::EffectSetMessage, e, text);
            }
        }
        let chats = ["gg", "multi\nline", "not exported", ""];
        let list = sm_list(O::SmConditionCount, O::SmConditionAt);
        for i in 1..list.len().min(5) {
            let c = list[i];
            condition_fields(c).mType = if i == 3 { 2 } else { 7 }; // kSMConditionChatMessageContains
            x::hs(O::ConditionSetChatText, c, chats[i - 1]);
        }
        text_elements(out);
        return;
    }
    if op == "export" {
        // sm export: ExportText's result and the file's lines
        let spec = x::text_spec(9);
        let result = if h(O::SpecFOpenWrite, spec, &[]) == 0 { h(O::SmExportText, m, &[spec as i64]) } else { -1 };
        h(O::SpecClose, spec, &[]);
        out.push(format!("  export {result}"));
        if h(O::SpecFOpenRead, spec, &[]) == 0 {
            while let Some(mut line) = x::spec_gets(spec, 0x400) {
                while matches!(line.last(), Some(b'\n' | b'\r')) {
                    line.pop();
                }
                out.push(format!("  | {}", String::from_utf8_lossy(&line)));
            }
            h(O::SpecClose, spec, &[]);
        }
        h(O::SpecFree, spec, &[]);
        return;
    }
    // sm import FILE KIND VALIDATE: ImportTriggerText over file FILE; its result, the errors, the texts
    let index = super::options::c_atol(&w[2]) as c_ulong;
    let spec = x::text_spec(index as i64);
    if h(O::SpecFOpenWrite, spec, &[]) == 0 {
        x::spec_puts(spec, &text_file_content(index as u64));
        h(O::SpecClose, spec, &[]);
    }
    h(O::SmClearImportErrors, m, &[]);
    let mut result = -1;
    if h(O::SpecFOpenRead, spec, &[]) == 0 {
        let kind = super::options::c_atol(&w[3]) as c_ulong;
        let validate = super::options::c_atol(&w[4]) != 0;
        result = h(O::SmImportText, m, &[spec as i64, kind as i64, validate as i64]);
    }
    h(O::SpecClose, spec, &[]);
    h(O::SpecFree, spec, &[]);
    out.push(format!("  import {} {} {} result {result}", w[2], w[3], w[4]));
    for k in 0..h(O::SmImportErrorCount, m, &[]) {
        out.push(format!("  error {}", ht(O::SmImportError, m, k)));
    }
    text_elements(out);
}

/// `sm OP LIST [INDEX]`: LIST 0 triggers, 1 conditions, 2 effects, 3 objects, 4 areas; OP new, sort, up, down,
/// delete, dup; `sm fill`: every element's index references set from its place (each list's size modulo), two
/// trigger entries of each kind, a timed entry per effect and a pending copy of effect 1; `sm state`: the state.
pub fn scenario_list_step(w: &[String], out: &mut Vec<String>) {
    let m = x::sm();
    let op = w.get(1).map_or("", |s| s.as_str());
    let k = if w.len() > 2 { super::options::c_atol(&w[2]) } else { 0 };
    let i = if w.len() > 3 { super::options::c_atol(&w[3]) as c_ulong } else { 0 };
    if op == "state" {
        scenario_list_state(out);
        return;
    }
    if op == "text" || op == "export" || op == "import" {
        scenario_text_step(w, out);
        return;
    }
    if op == "fill" {
        let triggers = sm_list(O::SmTriggerCount, O::SmTriggerAt);
        let conditions = sm_list(O::SmConditionCount, O::SmConditionAt);
        let effects = sm_list(O::SmEffectCount, O::SmEffectAt);
        let objects = sm_list(O::SmObjectCount, O::SmObjectAt);
        let (nt, nc, ne) = (triggers.len() as c_ulong, conditions.len() as c_ulong, effects.len() as c_ulong);
        let (no, na) = (objects.len() as c_ulong, h(O::SmAreaCount, m, &[]) as c_ulong);
        for (j, &t) in triggers.iter().enumerate() {
            let j = j as c_ulong;
            trigger_set(t, 0, [0, 0, ((j + 1) % nc) as i64]);
            trigger_set_operator(t, 0, [0, 1]);
            trigger_set(t, 0, [1, 1, ((j + 2) % nc) as i64]);
            trigger_set(t, 1, [0, 0, ((j + 1) % ne) as i64]);
            trigger_set_operator(t, 1, [0, 1]);
            trigger_set(t, 1, [1, 5, ((j + 2) % ne) as i64]);
        }
        for (j, &c) in conditions.iter().enumerate() {
            let j = j as c_ulong;
            let f = condition_fields(c);
            if f.mType != 1 {
                // kSMConditionObject
                f.mObjectIndex = (j + 1) % no;
            }
            f.mTrigger = if nt != 0 { (j + 1) % nt } else { 0 };
        }
        for (j, &e) in effects.iter().enumerate() {
            let j = j as c_ulong;
            let f = effect_fields(e);
            f.mTaskArea = j % na;
            f.mTargetArea = (j + 1) % na;
            f.mPlaceArea = (j + 2) % na;
            f.mLocationArea = (j + 3) % na;
            f.mObject = j % no;
            f.mLocationObject = (j + 1) % no;
            f.mTaskObject = (j + 2) % no;
            f.mPlaceObject = (j + 3) % no;
            f.mTargetObject = (j + 4) % no;
            f.mTrigger = if nt != 0 { (j + 1) % nt } else { 0 };
            h(O::EffectClearObjects, e, &[]);
            let mut o = 0;
            while o < no {
                h(O::EffectPushObject, e, &[((o + j) % no) as i64]);
                o += 2;
            }
            h(O::SmAddTimedEffect, m, &[j as i64, (1000 * (ne - j)) as i64]);
        }
        for (j, &o) in objects.iter().enumerate() {
            let j = j as c_ulong;
            let f = object_fields(o);
            f.mArea = (j + 2) % na;
            f.mParent = (j + 1) % no;
            f.mNearObject = (j + 2) % no;
        }
        if ne > 1 {
            h(O::SmAddPendingCopy, m, &[effects[1] as i64]);
        }
        return;
    }
    out.push(format!("  {op} {k} {i}"));
    match op {
        "new" => {
            h(O::SmNewEntry, m, &[k as i64]);
        }
        "sort" => {
            h(O::SmSortList, m, &[k as i64]);
        }
        "up" | "down" => {
            h(O::SmMoveEntry, m, &[k as i64, i as i64, (op == "up") as i64]);
        }
        "delete" => out
            .push(format!("  deleted {}", if h(O::SmDeleteEntry, m, &[k as i64, i as i64]) != 0 { "1" } else { "0" })),
        "dup" => {
            h(O::SmDuplicateEntry, m, &[k as i64, i as i64]);
        }
        _ => {}
    }
}

// ------------------------------------------------------------------------------------------------ the screens

/// The area screen: its state and centre, the brush group's highlight and paint area, every scenario area's name,
/// text, select mode, rectangle and anchor.
pub fn area_digest(out: &mut Vec<String>) {
    let s = x::trigger_sub(4); // kSETriggerScreenArea
    let mut r = [0f32; 4];
    let mut p = [0f32; 2];
    let mut middle = [0f32; 2];
    // SAFETY: the area screen's Rust object (or null)
    let flags = unsafe { x::ee_screens_sear_state(s, r.as_mut_ptr(), p.as_mut_ptr()) };
    let center = unsafe { x::ee_screens_sear_center(s, middle.as_mut_ptr()) };
    let b = x::brush_group();
    let c = if center { format!("{},{}", f6(middle[0]), f6(middle[1])) } else { "none".into() };
    out.push(format!(
        "  areascreen flags={flags} rect={} point={},{} center={c} highlight={} paint={}",
        rect_text(r),
        g(p[0]),
        g(p[1]),
        h(O::BrushHighlight, b, &[]),
        rect_text(x::rect(h(O::BrushRect, b, &[]) as usize))
    ));
    for (i, &a) in sm_list(O::SmAreaCount, O::SmAreaAt).iter().enumerate() {
        out.push(format!(
            "  area {i} mode={} rect={} {} anchor={} {},{} '{}' '{}'",
            h(O::AreaSelectMode, a, &[]) as c_ulong,
            h(O::AreaHasRectangle, a, &[]),
            rect_text(x::rect(h(O::AreaRect, a, &[]) as usize)),
            h(O::AreaHasAnchor, a, &[]),
            h(O::AreaAnchorX, a, &[]) as c_long,
            h(O::AreaAnchorY, a, &[]) as c_long,
            ht(O::AreaName, a, 0),
            ht(O::AreaText, a, 0)
        ));
    }
}

/// One control: its display mode.
fn control_text(name: &str, index: usize, control: usize) -> String {
    format!("  {name}{index} mode={}", x::display_mode(control))
}

/// The condition's or effect's controls (display modes, labels, combos, sliders).
pub fn controls_digest(condition: bool, out: &mut Vec<String>) {
    let (labels, sliders, checks, radios, combos, scenario, lists) = if condition {
        let t = x::condition_controls();
        (
            t.labels.to_vec(),
            vec![t.slider],
            t.checks.to_vec(),
            t.radios.to_vec(),
            t.combos.to_vec(),
            t.scenario.to_vec(),
            vec![],
        )
    } else {
        let t = x::effect_controls();
        (
            t.labels.to_vec(),
            t.sliders.to_vec(),
            t.checks.to_vec(),
            t.radios.to_vec(),
            t.combos.to_vec(),
            t.scenario.to_vec(),
            t.lists.to_vec(),
        )
    };
    out.push(if condition { "  condition controls" } else { "  effect controls" }.into());
    for (i, &l) in labels.iter().enumerate() {
        let first = if h(O::LabelLineCount, l, &[]) > 0 { ht(O::LabelText, l, 0) } else { String::new() };
        out.push(format!("{} '{first}'", control_text("label", i, l)));
    }
    for (i, &s) in sliders.iter().enumerate() {
        out.push(format!("{} value={}", control_text("slider", i, s), h(O::SliderValue, s, &[]) as c_long));
    }
    for (i, &c) in checks.iter().enumerate() {
        out.push(format!("{} checked={}", control_text("check", i, c), h(O::CheckIsChecked, c, &[])));
    }
    for (i, &r) in radios.iter().enumerate() {
        out.push(control_text("radio", i, r));
    }
    for (i, &c) in combos.iter().enumerate() {
        out.push(format!("{} lines={}", control_text("combo", i, c), x::line_count(h(O::ComboList, c, &[]) as usize)));
    }
    for (i, &c) in scenario.iter().enumerate() {
        out.push(format!(
            "{} kind={} lines={} selection={}",
            control_text("scenario", i, c),
            x::shell_op(c, crate::screens::shells::EE_SCREENS_SKOP_MODE, 0, 0),
            x::line_count(h(O::ComboList, c, &[]) as usize),
            selection_text(sel(O::ComboSelection, c))
        ));
    }
    for (i, &l) in lists.iter().enumerate() {
        let mut s = format!("{} lines={}", control_text("list", i, l), x::line_count(l));
        for k in 0..x::line_count(l) {
            s += &format!("{}'{}'", if k == 0 { " " } else { ", " }, ht(O::ListData, l, k as i64));
        }
        out.push(s);
    }
}

/// A control's class: a shell's is the class its kind replaces.
pub fn type_name(control: usize) -> String {
    let shell = h(O::ShellOf, control, &[]) as usize;
    if shell != 0 {
        return ht(O::ShellKindName, shell, 0);
    }
    strip_class(ht(O::ControlTypeName, control, 0))
}

/// `typeid(...).name()` without Microsoft's "class " prefix.
pub fn strip_class(name: String) -> String {
    name.strip_prefix("class ").map(str::to_string).unwrap_or(name)
}

fn ge_text_text(text: usize) -> String {
    if text == 0 {
        return " fg=none".into();
    }
    let font = h(O::TextFont, text, &[]) as usize;
    format!(
        " fg={} tr={} fh={} '{}'",
        colour(x::color(h(O::TextFgColor, text, &[]) as usize)),
        h(O::TextIsTransparent, text, &[]),
        if font != 0 { h(O::FontHeight, font, &[]) as c_ulong } else { 0 },
        ht(O::TextText, text, 0)
    )
}

fn label_text(label: usize) -> String {
    if label == 0 {
        return " label=none".into();
    }
    let n = h(O::LabelLineCount, label, &[]) as c_ulong;
    let mut s = format!("{} lines={n}", ge_text_text(h(O::LabelGeText, label, &[]) as usize));
    for k in 0..n.min(4) {
        s += &format!("{}'{}'", if k == 0 { " " } else { "|" }, ht(O::LabelText, label, k as i64));
    }
    s
}

fn list_text(list: usize) -> String {
    if list == 0 {
        return " list=none".into();
    }
    let n = x::line_count(list);
    let mut s = format!(" n={n} sel={}", selection_text(sel(O::ListSelection, list)));
    for k in 0..n.min(6) {
        s += &format!("{}'{}'", if k == 0 { " " } else { "|" }, ht(O::ListData, list, k as i64));
    }
    s
}

/// The form's controls: every control with an index in [first, last] or in the entity group `group` (-1: any): its
/// class, id, groups, rectangle, display mode, background, border, help string, text colour and font height, texts,
/// check state, selection and lines, slider value and range, and the trigger events by id.
pub fn form_digest(first: c_ulong, last: c_ulong, group: c_long, out: &mut Vec<String>) {
    let form = x::editor();
    let mut groups: Vec<(usize, String)> = Vec::new();
    for gi in 0..h(O::FormGroupCount, form, &[]) {
        let gr = h(O::FormGroup, form, &[gi]) as usize;
        if gr == 0 {
            continue;
        }
        let enabled = h(O::GroupIsEnabled, gr, &[]) != 0;
        for e in 0..h(O::GroupEntityCount, gr, &[]) {
            let entity = h(O::GroupEntity, gr, &[e]) as usize;
            let list = match groups.iter_mut().find(|(c, _)| *c == entity) {
                Some((_, l)) => l,
                None => {
                    groups.push((entity, String::new()));
                    &mut groups.last_mut().unwrap().1
                }
            };
            *list += &format!("{}{gi}{}", if list.is_empty() { "" } else { "," }, if enabled { "" } else { "d" });
        }
    }
    let count = h(O::FormControlCount, form, &[]) as c_ulong;
    let mut i = first;
    while i <= last && i < count {
        let c = h(O::FormControl, form, &[i as i64]) as usize;
        i += 1;
        let i = i - 1;
        if c == 0 {
            continue;
        }
        let group_list = groups.iter().find(|(k, _)| *k == c).map_or_else(|| "-".to_string(), |(_, l)| l.clone());
        if group >= 0
            && !format!(",{group_list},").contains(&format!(",{group},"))
            && !format!(",{group_list},").contains(&format!(",{group}d,"))
        {
            continue;
        }
        if h(O::ControlId, c, &[]) == 9 && h(O::IsTextLabel, c, &[]) != 0 {
            // the load text: its text (and the rectangle fitted to it) is the load thread's last, a timing
            out.push(format!("  c{i} the load text g={group_list}"));
            continue;
        }
        let mut bg = [0f32; 3];
        h(O::ControlBgColor, c, &[bg.as_mut_ptr() as i64]);
        let mut line = format!(
            "  c{i} {} id=0x{:x} ix={} g={group_list} r={} m={} bg={},{} b={} h={}",
            type_name(c),
            h(O::ControlId, c, &[]) as c_ulong,
            h(O::ControlIndex, c, &[]) as c_ulong,
            rect_text(x::rect(h(O::ControlRect, c, &[]) as usize)),
            x::display_mode(c),
            format_g(x::hd(O::ControlBgOpacity, c, 0)),
            colour(bg),
            h(O::ControlBorder, c, &[]) as c_ulong,
            h(O::ControlStringTableId, c, &[]) as c_ulong
        );
        let checked = |op: O| if h(op, c, &[]) != 0 { "1" } else { "0" };
        if h(O::IsRadio, c, &[]) != 0 {
            line += &format!(
                " k={}{} btn{}",
                checked(O::RadioIsChecked),
                label_text(h(O::RadioTextLabel, c, &[]) as usize),
                ge_text_text(h(O::ButtonGeText, c, &[]) as usize)
            );
        } else if h(O::IsCheck, c, &[]) != 0 {
            line += &format!(" k={}{}", checked(O::CheckIsChecked), label_text(h(O::CheckTextLabel, c, &[]) as usize));
        } else if h(O::IsButton, c, &[]) != 0 {
            line += &ge_text_text(h(O::ButtonGeText, c, &[]) as usize);
        } else if h(O::IsCombo, c, &[]) != 0 {
            line += &format!(
                " tb={}{}{}",
                h(O::ComboTextBoxType, c, &[]) as c_long,
                label_text(h(O::ComboTextLabel, c, &[]) as usize),
                list_text(h(O::ComboList, c, &[]) as usize)
            );
        } else if h(O::IsList, c, &[]) != 0 {
            line += &format!("{}{}", ge_text_text(h(O::ListGeText, c, &[]) as usize), list_text(c));
        } else if h(O::IsEdit, c, &[]) != 0 {
            let n = h(O::EditLineCount, c, &[]) as c_ulong;
            line += &format!(
                "{} lines={n} '{}'",
                ge_text_text(h(O::EditGeText, c, &[]) as usize),
                if n > 0 { ht(O::EditText, c, 0) } else { String::new() }
            );
        } else if h(O::IsSlider, c, &[]) != 0 {
            line += &format!(" v={} max={}", h(O::SliderValue, c, &[]) as c_long, h(O::SliderMax, c, &[]) as c_long);
        } else if i > 0 && {
            let before = h(O::FormControl, form, &[i as i64 - 1]) as usize;
            before != 0 && type_name(before) == "EEUIFMGAttribute"
        } {
            // a unit attribute's value (the unit tab's panel): its bonus follows the tech tree the load thread
            // reloads, shown on a later frame (a timing)
            line += " (an attribute value)";
        } else if h(O::IsTextLabel, c, &[]) != 0 {
            line += &label_text(c);
        }
        // kControlTriggerRangeChanged .. kControlTriggerButtonUp
        for t in x::TRIGGER_RANGE_CHANGED..=x::TRIGGER_BUTTON_UP {
            let e = x::trigger_event(c, t);
            if e != 0 {
                line += &format!(" e{}={}", t - x::TRIGGER_RANGE_CHANGED, strip_class(ht(O::EventTypeName, e, 0)));
            }
        }
        out.push(line);
    }
}

/// The players as the player tab writes them: names, colours, civilizations, epochs, resource caps, shared line of
/// sight, game options, the computer players' settings, stances; the world's game options.
pub fn players_digest(out: &mut Vec<String>) {
    let n = h(O::WorldGetNumPlayers, 0, &[]) as c_ulong;
    for p in 0..n {
        let q = gc::h(gc::O::TbWorldPlayer, 0, &[p as i64]) as usize;
        if q == 0 {
            out.push(format!("  player {p} none"));
            continue;
        }
        let t = gc::h(gc::O::PlTechTree, q, &[]) as usize;
        let caps: Vec<String> = (2..7).map(|i| (h(O::PlayerResourceCap, q, &[i]) as c_long).to_string()).collect();
        let enemies: String = (0..n).map(|j| (h(O::PlayerEnemy, q, &[j as i64]) as c_long).to_string()).collect();
        let mut line = format!(
            "  player {p} '{}' colour={} civ='{}' epochs={},{},{} caps={} los={} upgrades={} points={} enemies={enemies}",
            ht(O::PlayerName, q, 0),
            colour(x::color(h(O::PlayerColor, q, &[]) as usize)),
            ht(O::PlayerCivilizationFile, q, 0),
            h(O::TreeCurrentEpoch, t, &[]) as c_long,
            h(O::TreeStartEpoch, t, &[]) as c_long,
            h(O::TreeEndEpoch, t, &[]) as c_long,
            caps.join(","),
            h(O::PlayerSharedLos, q, &[]),
            h(O::PlayerPurchasedUpgrades, q, &[]) as u64,
            format_g(x::hd(O::PlayerAvailablePoints, q, 0))
        );
        if p > 0 {
            line += &format!(
                " options={},{}",
                h(O::OptionsPlayerColor, 0, &[p as i64 - 1]) as c_ulong,
                h(O::OptionsPlayerB36, 0, &[p as i64 - 1])
            );
        }
        let c = gc::h(gc::O::PlComputerPlayer, q, &[]) as usize;
        if c != 0 {
            const FLAGS: [O; 8] = [
                O::ComputerActive,
                O::ComputerIsCheating,
                O::ComputerCanAttack,
                O::ComputerCanDefend,
                O::ComputerCanBuild,
                O::ComputerCanExplore,
                O::ComputerIgnoreOthers,
                O::ComputerCanGather,
            ];
            let flags: String = FLAGS.iter().map(|&k| h(k, c, &[]).to_string()).collect();
            line += &format!(" ai={flags} citizens={}", format_g(x::hd(O::ComputerCitizens, c, 0)));
        }
        out.push(line);
    }
    let wv = |k: usize| {
        const VALUES: [O; 13] = [
            O::WorldMaxPlayers,
            O::WorldPopulationLimit,
            O::WorldWondersToWin,
            O::WorldVictoryEnabled,
            O::WorldTeamsLocked,
            O::WorldCheatsEnabled,
            O::WorldImprovementsAllowed,
            O::WorldUsingCarbon,
            O::OptionsUnitLimit,
            O::OptionsWondersForVictory,
            O::OptionsVictory,
            O::OptionsLockTeams,
            O::OptionsCheatCodes,
        ];
        h(VALUES[k], 0, &[])
    };
    out.push(format!(
        "  world max={} population={} wonders={} victory={} teams={} cheats={} improvements={} carbon={} \
         options={},{},{},{},{}",
        wv(0) as c_long,
        wv(1) as c_long,
        wv(2) as c_long,
        wv(3),
        wv(4),
        wv(5),
        wv(6),
        wv(7),
        wv(8) as c_long,
        wv(9) as c_long,
        wv(10),
        wv(11),
        wv(12)
    ));
}

/// The editor's load thread drained: every message posted to it so far handled, as
/// EEDataPreLoading::WaitForMessageQueue (its 0x40a sets mMessageQueueDone); the digests that read what it loads
/// (epochs, tech trees, the load text) run after it, so they do not depend on its timing.
pub fn sync_load_thread() {
    let thread = x::slot(43); // kSlotLoadThread
    if h(O::LoadThreadPostDone, thread, &[]) == 0 {
        h(O::LoadThreadWaitDone, thread, &[]);
        h(O::LoadThreadClearDone, thread, &[]);
    }
}

/// The terrain tab: its terrains' and colour types' table ids in order, the selected terrain, the type list (each
/// line's name, preview and colour set), the colour list (each set's size and first colour), their selections, the
/// preview bitmap's texture and corner colours, the paint radios, the controls' display modes, the paint area's
/// centring, the brush group's size, highlight and rectangle.
pub fn terrain_digest(out: &mut Vec<String>) {
    use crate::screens::editor_terrain::*;
    let t = x::tab(2); // kEditorTabTerrain
                       // SAFETY: the terrain tab's Rust object
    let get = |what: u32, k: u32| unsafe { x::ee_screens_setn_get(t, what, k) } as c_long;
    let mut s =
        format!("  terrain selected={} terrains", get(EE_SCREENS_SETN_GET_SELECTED, 0) as u32 as c_ulong as c_long);
    for k in 0..get(EE_SCREENS_SETN_GET_TERRAIN_COUNT, 0) as u32 {
        s += &format!(" {}", get(EE_SCREENS_SETN_GET_TERRAIN_ID, k));
    }
    s += " colors";
    for k in 0..get(EE_SCREENS_SETN_GET_COLOR_COUNT, 0) as u32 {
        s += &format!(" {}", get(EE_SCREENS_SETN_GET_COLOR_ID, k));
    }
    out.push(s);
    let types = x::control_by_id(0xcc); // kSETerrainTerrainType
    let n = h(O::TerrainTypeCount, types, &[]) as c_ulong;
    let mut s = format!("  types n={n} sel={}:", selection_text(sel(O::TerrainTypeSelection, types)));
    for k in 0..n {
        s += &format!(
            " '{}'{}{}",
            ht(O::TerrainTypeName, types, k as i64),
            if h(O::TerrainTypeTexture, types, &[k as i64]) != 0 { "t" } else { "-" },
            h(O::TerrainTypeColorSet, types, &[k as i64]) as c_long
        );
    }
    out.push(s);
    let colors = x::control_by_id(0xcf); // kSETerrainTerrainColorType
    let n = h(O::TerrainColorCount, colors, &[]) as c_ulong;
    let mut s = format!("  colorsets n={n} sel={}:", selection_text(sel(O::TerrainColorSelection, colors)));
    for k in 0..n {
        let size = h(O::TerrainColorSetSize, colors, &[k as i64]) as u64;
        s += &format!(" {size}");
        if size != 0 {
            s += &format!("/{}", colour(x::color(h(O::TerrainColorSetFirst, colors, &[k as i64]) as usize)));
        }
    }
    out.push(s);
    let bitmap = h(O::BitmapGeBitmap, x::control_by_id(0xc9), &[]) as usize; // kSETerrainBitmap
    let mut corners = [[0f32; 3]; 4];
    h(O::GeBitmapCorners, bitmap, &[corners.as_mut_ptr() as i64]);
    let mut s = format!(
        "  preview texture={} corners {} {} {} {} modes",
        if h(O::GeBitmapTexture, bitmap, &[]) != 0 { "1" } else { "0" },
        colour(corners[0]),
        colour(corners[1]),
        colour(corners[2]),
        colour(corners[3])
    );
    for id in [0xcb, 0xcc, 0xce, 0xcf, 0xc9] {
        s += &format!(" {}", x::display_mode(x::control_by_id(id)));
    }
    s += &format!(
        " radios {}{}",
        h(O::RadioIsChecked, x::control_by_id(0xc8), &[]),
        h(O::RadioIsChecked, x::control_by_id(0xca), &[])
    );
    let centered =
        x::shell_op(x::control_by_id(205), crate::screens::shells_paint::EE_SCREENS_SKOP_PAINT_CENTERED, 0, 0);
    s += &format!(" centered={}", if centered != 0 { "1" } else { "0" });
    let b = x::brush_group();
    out.push(format!(
        "{s} brush size={} shown={} centered={} rect={}",
        h(O::BrushSize, b, &[]) as c_ulong,
        h(O::BrushHighlight, b, &[]),
        h(O::BrushCentered, b, &[]),
        rect_text(x::rect(h(O::BrushRect, b, &[]) as usize))
    ));
}

/// The player tab's two tech lists: each one's techs (ids, in order), lines, selection and the lines CopyData marks.
pub fn tech_digest(out: &mut Vec<String>) {
    use crate::screens::shells_tech::*;
    for id in [0x154i64, 0x155] {
        // kSERBUnitTypesTechList, kSERBUnitTypesOtherTechList
        let l = x::control_by_id(id);
        h(O::ListCopyData, l, &[]);
        let count = x::shell_op(l, EE_SCREENS_SKOP_TECH_COUNT, 0, 0) as c_long;
        let mut s = format!(
            "  techs {id} n={count} lines={} sel={} marked",
            x::line_count(l),
            selection_text(sel(O::ListSelection, l))
        );
        for d in 0..x::shell_op(l, EE_SCREENS_SKOP_TECH_MARKED_COUNT, 0, 0) as c_long {
            s += &format!(" {}", x::shell_op(l, EE_SCREENS_SKOP_TECH_MARKED_AT, d as i64, 0));
        }
        s += " ids";
        for k in 0..count {
            s += &format!(" {}", x::shell_op(l, EE_SCREENS_SKOP_TECH_AT, k as i64, 0));
        }
        out.push(s);
        let mut s = "  lines".to_string();
        for k in 0..x::line_count(l) {
            s += &format!("{}'{}'", if k == 0 { " " } else { "|" }, ht(O::ListData, l, k as i64));
        }
        out.push(s);
    }
}

/// The editor form's own state: its tab and control group, the selection and its cargo count, its flags, the mini
/// map's, help label's and load text's display modes (the load text's text after the load thread is idle), the
/// brush highlight, the saved view and panel rectangles, the edge scroll's edges.
pub fn form_state(out: &mut Vec<String>) {
    use crate::screens::editor_form::*;
    sync_load_thread();
    let e = x::editor();
    let mut units = String::new();
    for i in 0..h(O::SelectedCount, e, &[]) {
        let u = h(O::SelectedUnit, e, &[i]) as usize;
        units += if i == 0 { "" } else { "," };
        if u == 0 {
            units += "null";
        } else {
            units += &(h(O::UnitId, u, &[]) as c_ulong).to_string();
            units += if h(O::UnitIsEditorSelected, u, &[]) != 0 { "" } else { "!" };
        }
    }
    let s = |slot: u32| x::slot(slot);
    let r = |slot: u32| -> [f32; 4] { [0, 1, 2, 3].map(|k| f32::from_bits(s(slot + k) as u32)) };
    let load_text = s(EE_SCREENS_SEFM_LOAD_TEXT);
    let b01 = |v: usize| if v != 0 { 1 } else { 0 };
    let mut line = format!(
        "  form tab={} eg={} on={} cargo={} sel=[{units}] b={}{} sw={} sus={} hid={} exit={} mini={} help={} load={} \
         hl={} view={} panel={} edges={}",
        s(EE_SCREENS_SEFM_CURRENT_TAB) as c_long,
        s(EE_SCREENS_SEFM_ENTITY_GROUP) as c_long,
        b01(s(EE_SCREENS_SEFM_ENTITY_GROUP_ENABLED)),
        s(EE_SCREENS_SEFM_NUM_CARGO_UNITS) as c_long,
        b01(s(EE_SCREENS_SEFM_B2B1)),
        b01(s(EE_SCREENS_SEFM_B2B2)),
        b01(s(EE_SCREENS_SEFM_SWITCHING)),
        b01(s(EE_SCREENS_SEFM_SUSPENDED)),
        b01(s(52)), // kSlotUIHidden: GetIsUIHidden
        b01(s(EE_SCREENS_SEFM_EXIT_AFTER_SAVE)),
        x::display_mode(s(EE_SCREENS_SEFM_MINI_MAP)),
        x::display_mode(s(EE_SCREENS_SEFM_HELP_LABEL)),
        x::display_mode(load_text),
        h(O::BrushHighlight, x::brush_group(), &[]),
        rect_text(r(EE_SCREENS_SEFM_SAVED_VIEW)),
        rect_text(r(EE_SCREENS_SEFM_PANEL)),
        rect_text(x::rect(h(O::EdgeScrollEdges, s(1), &[]) as usize)) // kSlotEdgeScroll
    );
    if x::display_mode(load_text) == 1 {
        // kDisplayModeShown
        line += &label_text(load_text);
    }
    out.push(line);
    let mut groups = "  groups".to_string();
    for gi in 0..h(O::FormGroupCount, e, &[]) {
        let gr = h(O::FormGroup, e, &[gi]) as usize;
        groups += if gr != 0 && h(O::GroupIsEnabled, gr, &[]) != 0 { " 1" } else { " 0" };
    }
    out.push(groups);
}

/// The trigger-only save's report: a fixed set of changes first with `setup` (a trigger off, looping, timed and
/// described; a NOT and an OR; a wrapper past the conditions and one past the effects; a looping trigger's effect
/// naming the class; an effect of no type; an object in an effect's list, one a parent, an area an object's and one
/// an effect's), then the save's result and ScenarioTriggers.txt's lines.
pub fn trigger_report(setup: bool, out: &mut Vec<String>) {
    let triggers = sm_list(O::SmTriggerCount, O::SmTriggerAt);
    let effects = sm_list(O::SmEffectCount, O::SmEffectAt);
    let conditions = sm_list(O::SmConditionCount, O::SmConditionAt);
    if setup && triggers.len() >= 2 && effects.len() >= 3 && conditions.len() >= 2 {
        let t = triggers[0];
        h(O::TriggerSetIsOn, t, &[0]);
        h(O::TriggerSetIsLooping, t, &[1]);
        h(O::TriggerSetConditionDuration, t, &[7]);
        x::hs(O::TriggerSetName, t, "Report One");
        x::hs(O::TriggerSetText, t, "A described trigger");
        trigger_set(t, 0, [0, 1, 1]);
        trigger_set_operator(t, 0, [0, 2]); // kSMTriggerOperatorOr
        trigger_set(t, 0, [1, 0, conditions.len() as i64 + 3]);
        trigger_set(t, 1, [0, 4, 1]);
        trigger_set_operator(t, 1, [0, 1]); // kSMTriggerOperatorAnd
        trigger_set(t, 1, [1, 0, effects.len() as i64 + 2]);
        effect_fields(effects[1]).mType = 1; // kSMEffectObject
        effect_fields(effects[1]).mObjectAction = 10; // kSMObjectActionClassName
        effect_fields(effects[2]).mType = 0; // kSMEffectNone
        let u = triggers[1];
        h(O::TriggerSetIsLooping, u, &[1]);
        trigger_set(u, 1, [0, 0, 2]);
        let objects = sm_list(O::SmObjectCount, O::SmObjectAt);
        if objects.len() >= 4 && h(O::SmAreaCount, x::sm(), &[]) >= 4 {
            h(O::EffectPushObject, effects[1], &[1]);
            object_fields(objects[3]).mParent = 2;
            object_fields(objects[1]).mArea = 2;
            effect_fields(effects[2]).mTaskArea = 3;
        }
    }
    out.push(format!("  result {}", h(O::SaveTriggersOnly, x::editor(), &[]) as c_long));
    let path = ht(O::StartupPath, 0, 0) + "ScenarioTriggers.txt";
    let Ok(bytes) = std::fs::read(&path) else {
        out.push("  no report".into());
        return;
    };
    let mut line: Vec<u8> = Vec::new();
    for &c in &bytes {
        if c == b'\n' {
            out.push(format!("  | {}", String::from_utf8_lossy(&line)));
            line.clear();
        } else if c == b'\r' {
            line.extend_from_slice(b"<CR>");
        } else {
            line.push(c);
        }
    }
    if !line.is_empty() {
        out.push(format!("  | {}<no LF>", String::from_utf8_lossy(&line)));
    }
}

/// A trigger tab sub-screen's selection (`ee_screens_sets_sub`'s).
fn sub_selection(k: u32) -> (i32, bool) {
    let mut selection = 0i32;
    let mut has = false;
    // SAFETY: the trigger tab's Rust object
    unsafe { x::ee_screens_sets_sub(x::trigger_tab(), k, &mut selection, &mut has) };
    (selection, has)
}

/// The trigger tab's open trigger: the sub-screen's selection, then the trigger's flags, timer, conditions (not,
/// condition, operator) and effects (delay, effect, operator).
pub fn trigger_digest(out: &mut Vec<String>) {
    let (selection, has) = sub_selection(0); // kSETriggerScreenTrigger
    let t = h(O::SmGetTrigger, x::sm(), &[selection as i64]) as usize;
    out.push(format!("  trigger selection={selection} has={}", has as i32));
    if t == 0 {
        out.push("  trigger none".into());
        return;
    }
    let mut conditions = String::new();
    for i in 0..h(O::TriggerGetConditionCount, t, &[]) {
        let c = h(O::TriggerGetConditionAt, t, &[i]) as usize;
        conditions += &format!(
            "{}{}{}/{}",
            if i == 0 { "" } else { "," },
            if h(O::WrapperIsNot, c, &[]) != 0 { "!" } else { "" },
            h(O::WrapperValue, c, &[]) as c_ulong,
            h(O::WrapperOperator, c, &[]) as c_long
        );
    }
    let mut effects = String::new();
    for i in 0..h(O::TriggerGetEffectCount, t, &[]) {
        let e = h(O::TriggerGetEffectAt, t, &[i]) as usize;
        effects += &format!(
            "{}{}:{}/{}",
            if i == 0 { "" } else { "," },
            h(O::WrapperDelay, e, &[]) as c_ulong,
            h(O::WrapperEffectIndex, e, &[]) as c_ulong,
            h(O::WrapperEffectOperator, e, &[]) as c_long
        );
    }
    let f = trigger_fields(t);
    out.push(format!(
        "  trigger '{}' text='{}' on={} duration={} looping={} cinematic={} conditions={conditions} effects={effects}",
        ht(O::TriggerName, t, 0),
        ht(O::TriggerText, t, 0),
        f.mIsOn as i32,
        f.mConditionDuration,
        f.mIsLooping as i32,
        f.mIsInCinematic as i32
    ));
}

/// The trigger tab's open object: the sub-screen's selection, then the scenario object's fields.
pub fn object_digest(out: &mut Vec<String>) {
    let (selection, has) = sub_selection(3); // kSETriggerScreenObject
    let o = h(O::SmGetObject, x::sm(), &[selection as i64 + 1]) as usize;
    out.push(format!("  object selection={selection} has={}", has as i32));
    if o == 0 {
        out.push("  object none".into());
        return;
    }
    let units: Vec<String> =
        (0..h(O::ObjectUnitCount, o, &[])).map(|i| (h(O::ObjectUnitAt, o, &[i]) as c_ulong).to_string()).collect();
    let f = object_fields(o);
    out.push(format!(
        "  object '{}' map={} units={} count={},{} dynamic={} owner={} family={} class={} parent={} \
         checks={},{},{},{},{},{} area={} near={},{} attribute={},{},{},{} visible={} state={} selected={}",
        ht(O::ObjectName, o, 0),
        f.mIsSelectOnMap as i32,
        units.join(","),
        f.mMinCount,
        f.mMaxCount,
        f.mIsDynamic as i32,
        f.mOwner,
        f.mFamily,
        f.mClass,
        f.mParent,
        f.mInAreaCheck,
        f.mNearCheck,
        f.mHasAttributeCheck,
        f.mVisibleByCheck,
        f.mHasStateCheck,
        f.mSelectedByCheck,
        f.mArea,
        f.mNearMode,
        f.mNearObject,
        f.mAttribute,
        f.mAttributeMin,
        f.mAttributeMax,
        f.mIsAttributePercent as i32,
        f.mVisibleByPlayer,
        f.mState,
        f.mSelectedByPlayer
    ));
}

/// Every condition: its numbers (EeScenarioConditionFields' words, then its six flags) and texts.
pub fn condition_state(out: &mut Vec<String>) {
    for (i, &c) in sm_list(O::SmConditionCount, O::SmConditionAt).iter().enumerate() {
        let f = *condition_fields(c);
        let words = std::mem::offset_of!(EeScenarioConditionFields, mIsTriggerOn);
        // SAFETY: the fields' bytes (a plain repr(C) struct)
        let bytes = unsafe {
            std::slice::from_raw_parts(&f as *const _ as *const u8, std::mem::size_of::<EeScenarioConditionFields>())
        };
        let mut s = format!("  condition {} '{}' n", num(i as c_ulong), ht(O::ConditionName, c, 0));
        s += &word_list(&bytes[..words]);
        s += " b";
        for &b in &bytes[words..words + 6] {
            s += if b != 0 { "1" } else { "0" };
        }
        s += &format!(" text '{}' chat '{}'", ht(O::ConditionText, c, 0), ht(O::ConditionChatText, c, 0));
        out.push(s);
    }
}

/// Every effect: its numbers (EeScenarioEffectFields' words), texts and objects.
pub fn effect_state(out: &mut Vec<String>) {
    for (i, &e) in sm_list(O::SmEffectCount, O::SmEffectAt).iter().enumerate() {
        let f = *effect_fields(e);
        // SAFETY: as condition_state
        let bytes = unsafe {
            std::slice::from_raw_parts(&f as *const _ as *const u8, std::mem::size_of::<EeScenarioEffectFields>())
        };
        let mut s = format!("  effect {} '{}' n", num(i as c_ulong), ht(O::EffectName, e, 0));
        s += &word_list(bytes);
        s += &format!(
            " new '{}' msg '{}' file '{}' desc '{}' text '{}' objects",
            ht(O::EffectNewName, e, 0),
            ht(O::EffectMessage, e, 0),
            ht(O::EffectFileName, e, 0),
            ht(O::EffectDescription, e, 0),
            ht(O::EffectText, e, 0)
        );
        for o in effect_objects(e) {
            s += &format!(" {}", num(o));
        }
        out.push(s);
    }
}

/// " %lx" per whole 32-bit word of `bytes`.
pub fn word_list(bytes: &[u8]) -> String {
    bytes.as_chunks::<4>().0.iter().map(|w| format!(" {:x}", u32::from_le_bytes(*w))).collect()
}

/// The first `limit` bytes of `text`, cut back to a whole UTF-8 character.
pub fn head_of(text: &[u8], limit: usize) -> &[u8] {
    let mut n = text.len().min(limit);
    while n > 0 && n < text.len() && (text[n] & 0xC0) == 0x80 {
        n -= 1;
    }
    &text[..n]
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn chunk_lines() {
        assert_eq!(chunk_text(5, 0, b"x"), "5 result=0 (differs by run)");
        assert_eq!(chunk_text(2, 0, b""), "2 result=0 bytes=0 14650fb0739d0383");
        assert!(chunk_by_run(12) && !chunk_by_run(2));
    }

    #[test]
    fn formats() {
        assert_eq!(g(0.5), "0.5");
        assert_eq!(rect_text([0.0, 1.5, 100.0, 2.0]), "0,1.5,100,2");
        assert_eq!(colour([1.0, 0.25, 0.0]), "1,0.25,0");
        assert_eq!(f6(1.5), "1.500000");
        assert_eq!(strip_class("class UIButton".into()), "UIButton");
        assert_eq!(strip_class("UIButton".into()), "UIButton");
        assert_eq!(word_list(&[1, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 9]), " 1 ffffffff");
    }

    #[test]
    fn heads_and_files() {
        assert_eq!(head_of("ab\u{fc}".as_bytes(), 3), b"ab");
        assert_eq!(head_of(b"abc", 60), b"abc");
        let long = text_file_content(4);
        assert!(long.starts_with(b"{2_History}\n") && long.ends_with(b"\n\n"));
        assert_eq!(long.len(), 12 + 601 + 511 + 27 + 509 + 5 + 10 + 513);
        assert!(text_file_content(3).is_empty());
        assert!(text_file_content(0).ends_with(b"Bob \xc3\xbc}\n"));
    }
}
