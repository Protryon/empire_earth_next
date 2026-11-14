//! The trigger text file of the scenario editor's instructions tab (`EEScenarioManager::ExportText`,
//! `ImportTriggerText`, `ValidateText`, `ImportText` and the three `Parse*`): the scenario's translatable texts as
//! "{tag}" lines, each followed by its text.
//!
//! - Export writes the media strings' four sections, then "{<id>_Effect:<name>}" for every effect with a text (a
//!   media effect's description or message, an object effect's new class name), "{<id>_Condition:<name>}" for every
//!   Chat Message Contains condition with a chat text, and "{<n>_Player Name:<name>}" for every player but nature.
//! - Import reads the file 511 bytes at a time (the original's `fgets` into 0x200 bytes: an entry can start inside
//!   a long line), a line splitting at '{' and '}' into exactly two tokens starting an entry; with validation it
//!   lists every bad entry (`mImportErrors`) and stops, then it applies the entries up to the first bad one.
//!
//! The elements stay the C++ objects of the lists ([`Lists`]); their texts, the players, the file and the error
//! messages (the language's formats) are the host's ([`Text`]). Version 0 files name effects "<id>_<name>".

use std::ffi::{c_void, CStr};
use std::os::raw::{c_char, c_long, c_ulong};

use crate::lists::EeScenarioListHost;
use crate::lists::{self, Element, HostLists, Lists, EE_SCENARIO_LIST_CONDITIONS, EE_SCENARIO_LIST_EFFECTS};
use ee_plat_core::utf8;

/// An element's text by slot: an effect's `mDescription`, `mMessage`, `mNewName`; a condition's `mChatText`.
pub const EE_SCENARIO_TEXT_DESCRIPTION: u32 = 0;
pub const EE_SCENARIO_TEXT_MESSAGE: u32 = 1;
pub const EE_SCENARIO_TEXT_NEW_NAME: u32 = 2;
pub const EE_SCENARIO_TEXT_CHAT: u32 = 3;

/// The import's error messages (`mImportErrors`), each the language's format (`IDS_IS_INVALID_TOKEN`,
/// `IDS_IS_INVALID_EFFECT_ID_FOR_TOKEN`, `IDS_IS_INVALID_EFFECT_TYPE_TO_BE_TOKEN`,
/// `IDS_TEXT_FILE_EFFECT_NAME_DOES_NOT_MATCH_SCENARIOS`, the condition's three, `IDS_INVALID_PLAYER_NUMBER_FOR_THIS_
/// SCENARIO`). The id kinds format (id, line), the others (line, the scenario's name).
pub const EE_SCENARIO_TEXT_ERROR_TOKEN: u32 = 0;
pub const EE_SCENARIO_TEXT_ERROR_EFFECT_ID: u32 = 1;
pub const EE_SCENARIO_TEXT_ERROR_EFFECT_TYPE: u32 = 2;
pub const EE_SCENARIO_TEXT_ERROR_EFFECT_NAME: u32 = 3;
pub const EE_SCENARIO_TEXT_ERROR_CONDITION_ID: u32 = 4;
pub const EE_SCENARIO_TEXT_ERROR_CONDITION_TYPE: u32 = 5;
pub const EE_SCENARIO_TEXT_ERROR_CONDITION_NAME: u32 = 6;
pub const EE_SCENARIO_TEXT_ERROR_PLAYER: u32 = 7;

/// The host's operations (`EESMTextOp`, EEScenarioManager.cpp).
/// An element's text `slot` as a C string.
pub const EE_SCENARIO_TEXT_OP_GET: u32 = 0;
/// An element's text `slot` set to `text`.
pub const EE_SCENARIO_TEXT_OP_SET: u32 = 1;
/// `gWorld.mNumPlayers` (in the answer).
pub const EE_SCENARIO_TEXT_OP_PLAYERS: u32 = 2;
/// Player `value`'s name, NULL when there is no such player.
pub const EE_SCENARIO_TEXT_OP_PLAYER_NAME: u32 = 3;
/// Player `value`'s name set to `text`.
pub const EE_SCENARIO_TEXT_OP_SET_PLAYER_NAME: u32 = 4;
/// `text` written to the file (`fprintf("%s")`): NULL on a write error.
pub const EE_SCENARIO_TEXT_OP_WRITE: u32 = 5;
/// The file's end flag (`feof`): not NULL when set.
pub const EE_SCENARIO_TEXT_OP_EOF: u32 = 6;
/// The next line, 511 bytes at most (`fgets` into 0x200 bytes): NULL at the end.
pub const EE_SCENARIO_TEXT_OP_READ: u32 = 7;
/// `mImportErrors.clear()`.
pub const EE_SCENARIO_TEXT_OP_CLEAR_ERRORS: u32 = 8;
/// Error `slot` (`EE_SCENARIO_TEXT_ERROR_*`) formatted with `value` (an id) or `text2`, and `text` (the line).
pub const EE_SCENARIO_TEXT_OP_ERROR: u32 = 9;
/// The media strings' section `slot` ([`MEDIA_TAGS`]) as a C string.
pub const EE_SCENARIO_TEXT_OP_MEDIA: u32 = 10;
/// The media strings' section `slot` set to `text`.
pub const EE_SCENARIO_TEXT_OP_SET_MEDIA: u32 = 11;

/// The media strings' sections (`EESMMediaStrings`' `mHints`, `mAftermath`, `mHistory`, `mInstructions`) by the
/// number of their tag.
pub const MEDIA_TAGS: [&[u8]; 4] = [b"0_Hints", b"1_After Math", b"2_History", b"3_Instructions"];

/// `kSMEffectObject`, `kSMEffectMedia`, `kSMMediaActionChangeText` .. `kSMMediaActionSendChatMessage`,
/// `kSMObjectActionClassName`, `kSMConditionChatMessageContains` (ee/EEScenarioIDs.h).
const EFFECT_OBJECT: c_ulong = 1;
const EFFECT_MEDIA: c_ulong = 4;
const MEDIA_CHANGE_TEXT: c_ulong = 1;
const MEDIA_SEND_DIALOGUE: c_ulong = 2;
const MEDIA_SEND_CHAT: c_ulong = 3;
const OBJECT_CLASS_NAME: c_ulong = 10;
const CONDITION_CHAT: c_ulong = 7;

/// The C++ side: the elements' texts, the players, the file, the error list, the media strings.
pub trait Text {
    fn text(&mut self, slot: u32, element: Element) -> Vec<u8>;
    fn set_text(&mut self, slot: u32, element: Element, text: &[u8]);
    fn players(&mut self) -> c_ulong;
    fn player_name(&mut self, player: c_ulong) -> Option<Vec<u8>>;
    fn set_player_name(&mut self, player: c_ulong, name: &[u8]);
    /// false on a write error
    fn write(&mut self, text: &[u8]) -> bool;
    fn eof(&mut self) -> bool;
    fn read_line(&mut self) -> Option<Vec<u8>>;
    fn clear_errors(&mut self);
    fn error(&mut self, kind: u32, id: c_long, line: &[u8], name: &[u8]);
    fn media(&mut self, slot: u32) -> Vec<u8>;
    fn set_media(&mut self, slot: u32, text: &[u8]);
}

/// `EESMMediaStrings::Export`: the aftermath, hints, instructions and history sections; 9 on a write error.
fn media_export<T: Text + ?Sized>(t: &mut T) -> c_long {
    for slot in [1, 0, 3, 2] {
        let text = t.media(slot as u32);
        if !t.write(&[b"{", MEDIA_TAGS[slot], b"}\n"].concat()) || !t.write(&[&text[..], b"\n\n"].concat()) {
            return 9;
        }
    }
    0
}

/// `EESMMediaStrings::Validate` and `Import`'s test: the section a line's tag names (the version aside).
fn media_slot(line: &[u8]) -> Option<u32> {
    MEDIA_TAGS.iter().position(|&tag| tag == line).map(|k| k as u32)
}

/// `sprintf("%ld")` of an `unsigned long` id.
fn num(value: c_ulong) -> Vec<u8> {
    (value as c_long).to_string().into_bytes()
}

/// The C runtime's `atoi`: white space, a sign, decimal digits; saturating as the Windows C runtime's.
fn atoi(s: &[u8]) -> i32 {
    let mut i = 0;
    while i < s.len() && matches!(s[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let negative = i < s.len() && s[i] == b'-';
    if i < s.len() && (s[i] == b'-' || s[i] == b'+') {
        i += 1;
    }
    let mut value = 0i64;
    while i < s.len() && s[i].is_ascii_digit() {
        value = (value * 10 + (s[i] - b'0') as i64).min(1 << 40);
        i += 1;
    }
    if negative {
        value = -value;
    }
    value.clamp(i32::MIN as i64, i32::MAX as i64) as i32
}

/// The effect's text the file holds (empty for one with none).
fn effect_text<L: Lists + ?Sized, T: Text + ?Sized>(l: &mut L, t: &mut T, e: Element) -> Vec<u8> {
    let f = l.effect(e);
    let (kind, media, object) = (f.mType, f.mMediaAction, f.mObjectAction);
    if kind == EFFECT_MEDIA {
        match media {
            MEDIA_CHANGE_TEXT => t.text(EE_SCENARIO_TEXT_DESCRIPTION, e),
            MEDIA_SEND_DIALOGUE | MEDIA_SEND_CHAT => t.text(EE_SCENARIO_TEXT_MESSAGE, e),
            _ => Vec::new(),
        }
    } else if kind == EFFECT_OBJECT && object == OBJECT_CLASS_NAME {
        t.text(EE_SCENARIO_TEXT_NEW_NAME, e)
    } else {
        Vec::new()
    }
}

/// One entry: its "{<id>_<kind>:<name>}" line and its text; false on a write error (the text not written then).
fn write_entry<T: Text + ?Sized>(t: &mut T, id: c_ulong, kind: &[u8], name: &[u8], text: &[u8]) -> bool {
    t.write(&[b"{", &num(id)[..], b"_", kind, b":", name, b"}\n"].concat()) && t.write(&[text, b"\n\n"].concat())
}

/// `ExportText`: the media strings, the effects' and conditions' texts, the players' names; 9 on a write error
/// (the players' loop goes on after one).
pub fn export<L: Lists + ?Sized, T: Text + ?Sized>(l: &mut L, t: &mut T) -> c_long {
    let mut result = media_export(t);
    if result != 0 {
        return result;
    }
    let mut i = 0;
    while i < l.count(EE_SCENARIO_LIST_EFFECTS) && result == 0 {
        let e = l.at(EE_SCENARIO_LIST_EFFECTS, i);
        i += 1;
        if e.is_null() {
            continue;
        }
        let text = effect_text(l, t, e);
        if text.is_empty() {
            continue;
        }
        let (id, name) = (l.id(EE_SCENARIO_LIST_EFFECTS, e), l.name(EE_SCENARIO_LIST_EFFECTS, e));
        if !write_entry(t, id, b"Effect", &name, &text) {
            result = 9;
        }
    }
    let mut i = 0;
    while i < l.count(EE_SCENARIO_LIST_CONDITIONS) && result == 0 {
        let c = l.at(EE_SCENARIO_LIST_CONDITIONS, i);
        i += 1;
        if c.is_null() || l.condition(c).mType != CONDITION_CHAT {
            continue;
        }
        let text = t.text(EE_SCENARIO_TEXT_CHAT, c);
        if text.is_empty() {
            continue;
        }
        let (id, name) = (l.id(EE_SCENARIO_LIST_CONDITIONS, c), l.name(EE_SCENARIO_LIST_CONDITIONS, c));
        if !write_entry(t, id, b"Condition", &name, &text) {
            result = 9;
        }
    }
    let mut p: c_ulong = 1;
    while p < t.players() {
        if let Some(name) = t.player_name(p) {
            if !t.write(&[b"{", &num(p)[..], b"_Player Name:", &name, b"}\n"].concat()) {
                result = 9;
            }
        }
        p += 1;
    }
    result
}

/// An entry's text loses up to two final new lines (by character: `theText[GetLength() - 1] == '\n'`).
fn trim(text: &mut Vec<u8>) {
    for _ in 0..2 {
        let n = utf8::char_len(text);
        if n > 0 && text[utf8::bytes_of_chars(text, n - 1)] == b'\n' {
            text.truncate(utf8::bytes_of_chars(text, n - 1));
        }
    }
}

/// The file's entries: each "{tag}" line's tag and the text after it.
pub struct Entries {
    pub lines: Vec<Vec<u8>>,
    pub texts: Vec<Vec<u8>>,
}

/// `ImportTriggerText`'s read: the entries, or 9 for a file whose texts and tags do not pair up. Text before the
/// first entry and lines of three or more tokens are dropped.
pub fn read_entries<T: Text + ?Sized>(t: &mut T) -> Result<Entries, c_long> {
    let mut e = Entries { lines: Vec::new(), texts: Vec::new() };
    let mut text: Vec<u8> = Vec::new();
    let mut in_entry = false;
    let mut result: c_long = 0;
    while !t.eof() && result == 0 {
        match t.read_line() {
            Some(read) => {
                let tokens = ee_ustring::tokenize(&read, b"{}");
                if tokens.len() == 2 {
                    if in_entry {
                        trim(&mut text);
                        e.texts.push(std::mem::take(&mut text));
                        result = if e.texts.len() != e.lines.len() { 9 } else { 0 };
                    }
                    let (first, last) = tokens[0];
                    e.lines.push(read[first as usize..=last as usize].to_vec());
                    text.clear();
                    in_entry = true;
                } else if tokens.len() <= 2 && in_entry {
                    text.extend_from_slice(&read);
                }
            }
            None => {
                if in_entry {
                    trim(&mut text);
                    e.texts.push(text.clone());
                }
                result = if e.texts.len() != e.lines.len() { 9 } else { 0 };
            }
        }
    }
    if result != 0 {
        return Err(result);
    }
    Ok(e)
}

/// `Parse*ID`: "<id><tag><name>": the id (`atoi` of the line's first `index` characters, `index` the tag's byte
/// position) and the name after `skip` characters (None when the tag ends the line: the caller's name kept); None
/// when the tag is not in the line after its first byte.
fn parse(line: &[u8], tag: &[u8], skip: usize) -> Option<(c_long, Option<Vec<u8>>)> {
    let index = ee_ustring::find(line, tag, 0) as usize;
    if index == 0 || index >= line.len() {
        return None;
    }
    let id = atoi(&line[..utf8::bytes_of_chars(line, index)]) as c_long;
    let rest = (index + 1 < line.len()).then(|| line[utf8::bytes_of_chars(line, index + skip)..].to_vec());
    Some((id, rest))
}

/// `ParseEffectID`: "<id>_Effect:<name>", version 0 "<id>_<name>".
fn parse_effect(line: &[u8], kind: c_ulong) -> Option<(c_long, Option<Vec<u8>>)> {
    if kind == 0 {
        parse(line, b"_", 1)
    } else {
        parse(line, b"_Effect:", 8)
    }
}

fn parse_condition(line: &[u8]) -> Option<(c_long, Option<Vec<u8>>)> {
    parse(line, b"_Condition:", 11)
}

fn parse_player(line: &[u8]) -> Option<(c_long, Option<Vec<u8>>)> {
    parse(line, b"_Player Name:", 13)
}

/// The parsed id, the name kept from the last entry when the line has none.
fn take(parsed: (c_long, Option<Vec<u8>>), name: &mut Vec<u8>) -> c_long {
    if let Some(rest) = parsed.1 {
        *name = rest;
    }
    parsed.0
}

/// Whether the effect takes a text (`ExportText`'s kinds).
fn effect_has_text<L: Lists + ?Sized>(l: &mut L, e: Element) -> bool {
    let f = l.effect(e);
    f.mType == EFFECT_MEDIA && matches!(f.mMediaAction, MEDIA_CHANGE_TEXT | MEDIA_SEND_DIALOGUE | MEDIA_SEND_CHAT)
        || f.mType == EFFECT_OBJECT && f.mObjectAction == OBJECT_CLASS_NAME
}

/// `ValidateText`: an error for every entry that is not a media string's, names no element of a kind that takes a
/// text, names it differently, or names no player but nature; 9 when there are any.
pub fn validate<L: Lists + ?Sized, T: Text + ?Sized>(l: &mut L, t: &mut T, lines: &[Vec<u8>], kind: c_ulong) -> c_long {
    let mut name: Vec<u8> = Vec::new();
    let mut errors = false;
    t.clear_errors();
    for line in lines {
        if media_slot(line).is_some() {
            continue;
        }
        let (error, id, scenario_name) = if let Some(p) = parse_effect(line, kind) {
            let id = take(p, &mut name);
            let e = lists::find(l, EE_SCENARIO_LIST_EFFECTS, id);
            if e.is_null() {
                (EE_SCENARIO_TEXT_ERROR_EFFECT_ID, id, Vec::new())
            } else if effect_has_text(l, e) {
                let n = l.name(EE_SCENARIO_LIST_EFFECTS, e);
                if utf8::compare(&name, &n) == 0 {
                    continue;
                }
                (EE_SCENARIO_TEXT_ERROR_EFFECT_NAME, 0, n)
            } else {
                (EE_SCENARIO_TEXT_ERROR_EFFECT_TYPE, 0, Vec::new())
            }
        } else if let Some(p) = parse_condition(line) {
            let id = take(p, &mut name);
            let c = lists::find(l, EE_SCENARIO_LIST_CONDITIONS, id);
            if c.is_null() {
                (EE_SCENARIO_TEXT_ERROR_CONDITION_ID, id, Vec::new())
            } else if l.condition(c).mType == CONDITION_CHAT {
                let n = l.name(EE_SCENARIO_LIST_CONDITIONS, c);
                if utf8::compare(&name, &n) == 0 {
                    continue;
                }
                (EE_SCENARIO_TEXT_ERROR_CONDITION_NAME, 0, n)
            } else {
                (EE_SCENARIO_TEXT_ERROR_CONDITION_TYPE, 0, Vec::new())
            }
        } else if let Some(p) = parse_player(line) {
            let id = take(p, &mut name);
            if id != 0 && (id as c_ulong) < t.players() {
                continue;
            }
            (EE_SCENARIO_TEXT_ERROR_PLAYER, id, Vec::new())
        } else {
            (EE_SCENARIO_TEXT_ERROR_TOKEN, 0, Vec::new())
        };
        t.error(error, id, line, &scenario_name);
        errors = true;
    }
    if errors {
        9
    } else {
        0
    }
}

/// `ImportText`: each entry to the media strings, else the effect, condition or player it names renamed and its
/// text set; stops with 9 at an unknown token or id, or an element whose kind takes no text (an effect is renamed
/// first). A player number out of range is skipped.
pub fn apply<L: Lists + ?Sized, T: Text + ?Sized>(
    l: &mut L,
    t: &mut T,
    lines: &[Vec<u8>],
    texts: &[Vec<u8>],
    kind: c_ulong,
) -> c_long {
    let mut name: Vec<u8> = Vec::new();
    let mut result: c_long = 0;
    let none: Vec<u8> = Vec::new();
    for (i, line) in lines.iter().enumerate() {
        if result != 0 {
            break;
        }
        // a last text the file did not end (no final new line): the original read past its list; empty here
        let text = texts.get(i).unwrap_or(&none);
        if let Some(slot) = media_slot(line) {
            t.set_media(slot, text);
            continue;
        }
        if let Some(p) = parse_effect(line, kind) {
            let id = take(p, &mut name);
            let e = lists::find(l, EE_SCENARIO_LIST_EFFECTS, id);
            result = 0;
            if e.is_null() {
                result = 9;
                continue;
            }
            l.set_name(EE_SCENARIO_LIST_EFFECTS, e, &name);
            let f = l.effect(e);
            let (kind, media, object) = (f.mType, f.mMediaAction, f.mObjectAction);
            if kind == EFFECT_MEDIA {
                match media {
                    MEDIA_CHANGE_TEXT => t.set_text(EE_SCENARIO_TEXT_DESCRIPTION, e, text),
                    MEDIA_SEND_DIALOGUE | MEDIA_SEND_CHAT => t.set_text(EE_SCENARIO_TEXT_MESSAGE, e, text),
                    _ => result = 9,
                }
            } else if kind == EFFECT_OBJECT {
                if object == OBJECT_CLASS_NAME {
                    t.set_text(EE_SCENARIO_TEXT_NEW_NAME, e, text);
                } else {
                    result = 9;
                }
            }
            continue;
        }
        if let Some(p) = parse_condition(line) {
            let id = take(p, &mut name);
            let c = lists::find(l, EE_SCENARIO_LIST_CONDITIONS, id);
            result = 0;
            if !c.is_null() && l.condition(c).mType == CONDITION_CHAT {
                l.set_name(EE_SCENARIO_LIST_CONDITIONS, c, &name);
                t.set_text(EE_SCENARIO_TEXT_CHAT, c, text);
            } else {
                result = 9;
            }
            continue;
        }
        let Some(p) = parse_player(line) else {
            result = 9;
            continue;
        };
        let id = take(p, &mut name) as c_ulong;
        result = 0;
        if id == 0 || id >= t.players() {
            continue;
        }
        if t.player_name(id).is_some() {
            t.set_player_name(id, &name);
        } else {
            result = 9;
        }
    }
    result
}

/// `ImportTriggerText`: the file's entries, validated first on request, then applied.
pub fn import<L: Lists + ?Sized, T: Text + ?Sized>(l: &mut L, t: &mut T, kind: c_ulong, check: bool) -> c_long {
    let entries = match read_entries(t) {
        Ok(e) => e,
        Err(r) => return r,
    };
    if check {
        let r = validate(l, t, &entries.lines, kind);
        if r != 0 {
            return r;
        }
    }
    apply(l, t, &entries.lines, &entries.texts, kind)
}

// ------------------------------------------------------------------------------------------- the C interface

/// The C++ host of the text file (`EESMTextOp` in EEScenarioManager.cpp): the lists' host and one operation
/// callback (`EE_SCENARIO_TEXT_OP_*`: `slot` a text slot or an error kind, `value` a player, an id or the file's
/// version, `text` and `text2` NUL-terminated strings).
#[repr(C)]
pub struct EeScenarioTextHost {
    pub lists: *const EeScenarioListHost,
    pub ctx: *mut c_void,
    pub op: unsafe extern "C" fn(
        ctx: *mut c_void,
        op: u32,
        slot: u32,
        value: c_long,
        element: *mut c_void,
        text: *const c_char,
        text2: *const c_char,
    ) -> *mut c_void,
}

struct HostText<'a>(&'a EeScenarioTextHost);

impl HostText<'_> {
    fn call(&mut self, op: u32, slot: u32, value: c_long, element: Element, text: &[u8], text2: &[u8]) -> *mut c_void {
        let (mut a, mut b) = (text.to_vec(), text2.to_vec());
        a.push(0);
        b.push(0);
        // SAFETY: the host's contract; both strings are NUL-terminated and outlive the call
        unsafe {
            (self.0.op)(self.0.ctx, op, slot, value, element, a.as_ptr() as *const c_char, b.as_ptr() as *const c_char)
        }
    }
    fn c_text(p: *mut c_void) -> Vec<u8> {
        // SAFETY: the host answers a NUL-terminated string valid until its next operation
        unsafe { CStr::from_ptr(p as *const c_char) }.to_bytes().to_vec()
    }
}

impl Text for HostText<'_> {
    fn text(&mut self, slot: u32, element: Element) -> Vec<u8> {
        Self::c_text(self.call(EE_SCENARIO_TEXT_OP_GET, slot, 0, element, b"", b""))
    }
    fn set_text(&mut self, slot: u32, element: Element, text: &[u8]) {
        self.call(EE_SCENARIO_TEXT_OP_SET, slot, 0, element, text, b"");
    }
    fn players(&mut self) -> c_ulong {
        self.call(EE_SCENARIO_TEXT_OP_PLAYERS, 0, 0, std::ptr::null_mut(), b"", b"") as usize as c_ulong
    }
    fn player_name(&mut self, player: c_ulong) -> Option<Vec<u8>> {
        let p = self.call(EE_SCENARIO_TEXT_OP_PLAYER_NAME, 0, player as c_long, std::ptr::null_mut(), b"", b"");
        (!p.is_null()).then(|| Self::c_text(p))
    }
    fn set_player_name(&mut self, player: c_ulong, name: &[u8]) {
        self.call(EE_SCENARIO_TEXT_OP_SET_PLAYER_NAME, 0, player as c_long, std::ptr::null_mut(), name, b"");
    }
    fn write(&mut self, text: &[u8]) -> bool {
        !self.call(EE_SCENARIO_TEXT_OP_WRITE, 0, 0, std::ptr::null_mut(), text, b"").is_null()
    }
    fn eof(&mut self) -> bool {
        !self.call(EE_SCENARIO_TEXT_OP_EOF, 0, 0, std::ptr::null_mut(), b"", b"").is_null()
    }
    fn read_line(&mut self) -> Option<Vec<u8>> {
        let p = self.call(EE_SCENARIO_TEXT_OP_READ, 0, 0, std::ptr::null_mut(), b"", b"");
        (!p.is_null()).then(|| Self::c_text(p))
    }
    fn clear_errors(&mut self) {
        self.call(EE_SCENARIO_TEXT_OP_CLEAR_ERRORS, 0, 0, std::ptr::null_mut(), b"", b"");
    }
    fn error(&mut self, kind: u32, id: c_long, line: &[u8], name: &[u8]) {
        self.call(EE_SCENARIO_TEXT_OP_ERROR, kind, id, std::ptr::null_mut(), line, name);
    }
    fn media(&mut self, slot: u32) -> Vec<u8> {
        Self::c_text(self.call(EE_SCENARIO_TEXT_OP_MEDIA, slot, 0, std::ptr::null_mut(), b"", b""))
    }
    fn set_media(&mut self, slot: u32, text: &[u8]) {
        self.call(EE_SCENARIO_TEXT_OP_SET_MEDIA, slot, 0, std::ptr::null_mut(), text, b"");
    }
}

/// `ExportText` ([`export`]).
///
/// # Safety
/// `host` must be a valid host table over the live manager and an open file.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_text_export(host: *const EeScenarioTextHost) -> c_long {
    // SAFETY: the caller's contract
    let h = unsafe { &*host };
    // SAFETY: the caller's contract (the lists' host is valid with it)
    export(&mut HostLists(unsafe { &*h.lists }), &mut HostText(h))
}

/// `ImportTriggerText` ([`import`]): `kind` the file's version, `validate` the editor's first pass.
///
/// # Safety
/// As [`ee_scenario_text_export`].
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_text_import(
    host: *const EeScenarioTextHost,
    kind: c_ulong,
    validate: bool,
) -> c_long {
    // SAFETY: the caller's contract
    let h = unsafe { &*host };
    // SAFETY: the caller's contract
    import(&mut HostLists(unsafe { &*h.lists }), &mut HostText(h), kind, validate)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fields::{EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioObjectFields};
    use crate::manager::{EeScenarioManagerFields, EeScenarioTimedQueue};

    /// An element: its name, id and numbers.
    #[derive(Default)]
    struct El {
        name: Vec<u8>,
        id: c_ulong,
        condition: EeScenarioConditionFields,
        effect: EeScenarioEffectFields,
    }

    /// The lists.
    #[derive(Default)]
    struct Mock {
        els: Vec<El>,
        conditions: Vec<usize>,
        effects: Vec<usize>,
        manager: EeScenarioManagerFields,
        timed: EeScenarioTimedQueue,
    }

    /// The elements' texts (by element, then slot), the players, a file read as `fgets` reads it (511 bytes or
    /// through a new line) and written, the errors, the media strings.
    #[derive(Default)]
    struct Host {
        texts: Vec<[Vec<u8>; 4]>,
        players: Vec<Option<Vec<u8>>>,
        input: Vec<u8>,
        at: usize,
        end: bool,
        out: Vec<u8>,
        fail_writes_after: Option<usize>,
        writes: usize,
        errors: Vec<(u32, c_long, String, String)>,
        media: [Vec<u8>; 4],
    }

    fn el(n: usize) -> Element {
        (n + 1) as Element
    }
    fn un(e: Element) -> usize {
        e as usize - 1
    }
    fn s(b: &[u8]) -> String {
        String::from_utf8_lossy(b).into_owned()
    }

    impl Mock {
        fn list(&mut self, list: u32) -> &mut Vec<usize> {
            if list == EE_SCENARIO_LIST_CONDITIONS {
                &mut self.conditions
            } else {
                &mut self.effects
            }
        }
    }

    impl Host {
        fn file(&mut self, text: &[u8]) {
            self.input = text.to_vec();
            self.at = 0;
            self.end = false;
        }
    }

    impl Lists for Mock {
        fn count(&mut self, list: u32) -> usize {
            self.list(list).len()
        }
        fn at(&mut self, list: u32, index: usize) -> Element {
            self.list(list).get(index).map_or(std::ptr::null_mut(), |&n| el(n))
        }
        fn set(&mut self, _: u32, _: usize, _: Element) {
            unimplemented!()
        }
        fn insert(&mut self, _: u32, _: usize, _: Element) {
            unimplemented!()
        }
        fn erase(&mut self, _: u32, _: usize) {
            unimplemented!()
        }
        fn push(&mut self, _: u32, _: Element) {
            unimplemented!()
        }
        fn create(&mut self, _: u32) -> Element {
            unimplemented!()
        }
        fn copy(&mut self, _: u32, _: usize, _: Element) {
            unimplemented!()
        }
        fn destroy(&mut self, _: u32, _: Element) {
            unimplemented!()
        }
        fn name(&mut self, _: u32, e: Element) -> Vec<u8> {
            self.els[un(e)].name.clone()
        }
        fn set_name(&mut self, _: u32, e: Element, name: &[u8]) {
            self.els[un(e)].name = name.to_vec();
        }
        fn id(&mut self, _: u32, e: Element) -> c_ulong {
            self.els[un(e)].id
        }
        fn set_id(&mut self, _: u32, _: Element, _: c_ulong) {
            unimplemented!()
        }
        fn condition(&mut self, e: Element) -> &mut EeScenarioConditionFields {
            &mut self.els[un(e)].condition
        }
        fn effect(&mut self, e: Element) -> &mut EeScenarioEffectFields {
            &mut self.els[un(e)].effect
        }
        fn object(&mut self, _: Element) -> &mut EeScenarioObjectFields {
            unimplemented!()
        }
        fn effect_objects(&mut self, _: Element) -> &mut [c_ulong] {
            unimplemented!()
        }
        fn entry_count(&mut self, _: Element, _: bool) -> usize {
            unimplemented!()
        }
        fn entry(&mut self, _: Element, _: bool, _: usize) -> &mut c_ulong {
            unimplemented!()
        }
        fn resource(&mut self, _: bool) -> Vec<u8> {
            unimplemented!()
        }
        fn out_name(&mut self, _: &[u8]) {
            unimplemented!()
        }
        fn manager(&mut self) -> &mut EeScenarioManagerFields {
            &mut self.manager
        }
        fn timed(&mut self) -> &mut EeScenarioTimedQueue {
            &mut self.timed
        }
    }

    impl Text for Host {
        fn text(&mut self, slot: u32, e: Element) -> Vec<u8> {
            self.texts[un(e)][slot as usize].clone()
        }
        fn set_text(&mut self, slot: u32, e: Element, text: &[u8]) {
            self.texts[un(e)][slot as usize] = text.to_vec();
        }
        fn players(&mut self) -> c_ulong {
            self.players.len() as c_ulong
        }
        fn player_name(&mut self, p: c_ulong) -> Option<Vec<u8>> {
            self.players.get(p as usize).cloned().flatten()
        }
        fn set_player_name(&mut self, p: c_ulong, name: &[u8]) {
            self.players[p as usize] = Some(name.to_vec());
        }
        fn write(&mut self, text: &[u8]) -> bool {
            self.writes += 1;
            if self.fail_writes_after.is_some_and(|n| self.writes > n) {
                return false;
            }
            self.out.extend_from_slice(text);
            true
        }
        fn eof(&mut self) -> bool {
            self.end
        }
        fn read_line(&mut self) -> Option<Vec<u8>> {
            if self.at >= self.input.len() {
                self.end = true;
                return None;
            }
            let rest = &self.input[self.at..];
            let mut n = rest.len().min(511);
            if let Some(k) = rest[..n].iter().position(|&b| b == b'\n') {
                n = k + 1;
            }
            self.at += n;
            if self.at >= self.input.len() && rest[n - 1] != b'\n' {
                self.end = true;
            }
            Some(rest[..n].to_vec())
        }
        fn clear_errors(&mut self) {
            self.errors.clear();
        }
        fn error(&mut self, kind: u32, id: c_long, line: &[u8], name: &[u8]) {
            self.errors.push((kind, id, s(line), s(name)));
        }
        fn media(&mut self, slot: u32) -> Vec<u8> {
            self.media[slot as usize].clone()
        }
        fn set_media(&mut self, slot: u32, text: &[u8]) {
            self.media[slot as usize] = text.to_vec();
        }
    }

    #[test]
    fn the_media_tags_name_their_sections() {
        assert_eq!(media_slot(b"0_Hints"), Some(0));
        assert_eq!(media_slot(b"1_After Math"), Some(1));
        assert_eq!(media_slot(b"2_History"), Some(2));
        assert_eq!(media_slot(b"3_Instructions"), Some(3));
        assert_eq!(media_slot(b"0_hints"), None);
        assert_eq!(media_slot(b"0_Hints "), None);
        assert_eq!(media_slot(b""), None);
    }

    #[allow(clippy::too_many_arguments)]
    fn effect(
        l: &mut Mock,
        t: &mut Host,
        id: c_ulong,
        name: &str,
        kind: c_ulong,
        action: c_ulong,
        slot: usize,
        text: &str,
    ) {
        let mut e = El { name: name.as_bytes().to_vec(), id, ..Default::default() };
        e.effect.mType = kind;
        if kind == EFFECT_MEDIA {
            e.effect.mMediaAction = action;
        } else {
            e.effect.mObjectAction = action;
        }
        l.els.push(e);
        l.effects.push(l.els.len() - 1);
        let mut texts: [Vec<u8>; 4] = Default::default();
        texts[slot] = text.as_bytes().to_vec();
        t.texts.push(texts);
    }

    fn condition(l: &mut Mock, t: &mut Host, id: c_ulong, name: &str, kind: c_ulong, chat: &str) {
        let mut e = El { name: name.as_bytes().to_vec(), id, ..Default::default() };
        e.condition.mType = kind;
        l.els.push(e);
        l.conditions.push(l.els.len() - 1);
        let mut texts: [Vec<u8>; 4] = Default::default();
        texts[3] = chat.as_bytes().to_vec();
        t.texts.push(texts);
    }

    /// Effects 0 (none), 5 (change text), 2 (dialogue), 3 (class name), 4 (a media action without a text);
    /// conditions 9 (chat), 6 (chat, no text), 1 (another kind); players nature, 1, none, 3.
    fn scenario() -> (Mock, Host) {
        let mut l = Mock::default();
        let mut t = Host {
            players: vec![Some(b"World".to_vec()), Some(b"Human".to_vec()), None, Some(b"Comp".to_vec())],
            ..Default::default()
        };
        effect(&mut l, &mut t, 0, "Do Nothing", 0, 0, 0, "");
        effect(&mut l, &mut t, 5, "<A>", EFFECT_MEDIA, MEDIA_CHANGE_TEXT, 0, "Briefing \u{fc}\nline 2");
        effect(&mut l, &mut t, 2, "<B>", EFFECT_MEDIA, MEDIA_SEND_DIALOGUE, 1, "Hello \u{65e5}");
        effect(&mut l, &mut t, 3, "<C>", EFFECT_OBJECT, OBJECT_CLASS_NAME, 2, "Class X");
        effect(&mut l, &mut t, 4, "<D>", EFFECT_MEDIA, 5, 1, "not a text kind");
        condition(&mut l, &mut t, 9, "<E>", CONDITION_CHAT, "gg");
        condition(&mut l, &mut t, 6, "<F>", CONDITION_CHAT, "");
        condition(&mut l, &mut t, 1, "<G>", 2, "other kind");
        t.media = [b"hint".to_vec(), b"after".to_vec(), b"".to_vec(), b"do".to_vec()];
        (l, t)
    }

    #[test]
    fn export_writes_the_media_the_texts_and_the_players() {
        let (mut l, mut t) = scenario();
        assert_eq!(export(&mut l, &mut t), 0);
        assert_eq!(
            s(&t.out),
            "{1_After Math}\nafter\n\n{0_Hints}\nhint\n\n{3_Instructions}\ndo\n\n{2_History}\n\n\n\
             {5_Effect:<A>}\nBriefing \u{fc}\nline 2\n\n{2_Effect:<B>}\nHello \u{65e5}\n\n{3_Effect:<C>}\nClass X\n\n\
             {9_Condition:<E>}\ngg\n\n{1_Player Name:Human}\n{3_Player Name:Comp}\n"
        );
    }

    #[test]
    fn export_stops_at_a_write_error_but_the_players_loop_goes_on() {
        let (mut l, mut t) = scenario();
        t.fail_writes_after = Some(9);
        assert_eq!(export(&mut l, &mut t), 9);
        // the media's eight writes and the first effect's line, its text not written; then the two players' lines
        assert_eq!(t.writes, 9 + 1 + 2);
        let (mut l, mut t) = scenario();
        t.fail_writes_after = Some(2);
        assert_eq!(export(&mut l, &mut t), 9);
        assert_eq!(t.writes, 3);
    }

    #[test]
    fn an_export_imports_back_unchanged() {
        let (mut l, mut t) = scenario();
        export(&mut l, &mut t);
        let (mut l2, mut t2) = scenario();
        for texts in t2.texts.iter_mut() {
            *texts = Default::default();
        }
        t2.media = Default::default();
        t2.players[1] = Some(b"x".to_vec());
        t2.file(&t.out);
        assert_eq!(import(&mut l2, &mut t2, 1, true), 0);
        assert!(t2.errors.is_empty());
        assert_eq!(
            t2.texts,
            t.texts
                .iter()
                .enumerate()
                .map(|(i, x)| if i == 4 || i == 7 { Default::default() } else { x.clone() })
                .collect::<Vec<_>>()
        );
        assert_eq!(t2.media, t.media);
        assert_eq!(t2.players, t.players);
    }

    #[test]
    fn a_text_loses_two_final_new_lines_at_most() {
        let (mut l, mut t) = scenario();
        t.file(b"junk before\n{0_Hints}\na\n\n\n\n{2_History}\n\xc3\xbc\n{3_Instructions}\n\n");
        assert_eq!(import(&mut l, &mut t, 1, false), 0);
        assert_eq!(t.media[0], b"a\n\n");
        assert_eq!(t.media[2], "\u{fc}".as_bytes());
        assert_eq!(t.media[3], b"");
    }

    #[test]
    fn long_lines_are_read_511_bytes_at_a_time() {
        let (mut l, mut t) = scenario();
        let (a, b) = ("a".repeat(600), "b".repeat(511));
        t.file(format!("{{2_History}}\n{a}\n{b}{{3_Effect:<C>}}\nafter\n").as_bytes());
        assert_eq!(import(&mut l, &mut t, 1, true), 0);
        assert_eq!(s(&t.media[2]), format!("{a}\n{b}"));
        assert_eq!(s(&t.texts[3][2]), "after");
    }

    #[test]
    fn lines_of_three_tokens_are_dropped_and_two_start_an_entry() {
        let (mut l, mut t) = scenario();
        t.file(b"{0_Hints}\none\na{b}c\n{a}{b}\n{}\n");
        assert_eq!(import(&mut l, &mut t, 1, false), 0);
        // a line of fewer than two tokens is appended whole, braces included
        assert_eq!(t.media[0], b"one\n{}");
        t.file(b"x{y\n");
        assert_eq!(import(&mut l, &mut t, 1, true), 9);
        assert_eq!(t.errors, vec![(EE_SCENARIO_TEXT_ERROR_TOKEN, 0, "x".into(), "".into())]);
    }

    #[test]
    fn validation_lists_every_bad_entry() {
        let (mut l, mut t) = scenario();
        t.file(
            "{3_Effect:Renamed}\nt\n{0_Player Name:Z}\n{4_Player Name:Y}\n{3_Player Name:ok}\n{7_Bogus}\n\
             {99_Effect:x}\n{4_Effect:<D>}\n{99_Condition:x}\n{1_Condition:<G>}\n{9_Condition:Wrong}\n\
             {\u{fc}3_Effect:x}\n{0_Hints}\nfine\n"
                .as_bytes(),
        );
        let before = t.media.clone();
        assert_eq!(import(&mut l, &mut t, 1, true), 9);
        assert_eq!(t.media, before);
        let e = |k, id, line: &str, name: &str| (k, id, line.to_string(), name.to_string());
        assert_eq!(
            t.errors,
            vec![
                e(EE_SCENARIO_TEXT_ERROR_EFFECT_NAME, 0, "3_Effect:Renamed", "<C>"),
                e(EE_SCENARIO_TEXT_ERROR_PLAYER, 0, "0_Player Name:Z", ""),
                e(EE_SCENARIO_TEXT_ERROR_PLAYER, 4, "4_Player Name:Y", ""),
                e(EE_SCENARIO_TEXT_ERROR_TOKEN, 0, "7_Bogus", ""),
                e(EE_SCENARIO_TEXT_ERROR_EFFECT_ID, 99, "99_Effect:x", ""),
                e(EE_SCENARIO_TEXT_ERROR_EFFECT_TYPE, 0, "4_Effect:<D>", ""),
                e(EE_SCENARIO_TEXT_ERROR_CONDITION_ID, 99, "99_Condition:x", ""),
                e(EE_SCENARIO_TEXT_ERROR_CONDITION_TYPE, 0, "1_Condition:<G>", ""),
                e(EE_SCENARIO_TEXT_ERROR_CONDITION_NAME, 0, "9_Condition:Wrong", "<E>"),
                // "ü3": the tag's byte index (3) taken as characters: the id is atoi("ü3_") = 0
                e(EE_SCENARIO_TEXT_ERROR_EFFECT_TYPE, 0, "\u{fc}3_Effect:x", ""),
            ]
        );
    }

    #[test]
    fn applying_stops_at_the_first_bad_entry_an_effect_renamed_first() {
        let (mut l, mut t) = scenario();
        t.file(
            b"{3_Effect:New}\nnew class\n{0_Player Name:Z}\n{9_Player Name:Y}\n{3_Player Name:P3}\n\
              {4_Effect:Media}\nx\n{0_Hints}\nlate\n",
        );
        assert_eq!(import(&mut l, &mut t, 1, false), 9);
        assert_eq!(s(&l.els[3].name), "New");
        assert_eq!(s(&t.texts[3][2]), "new class");
        assert_eq!(t.players[3].as_deref(), Some(&b"P3"[..]));
        // the media effect whose action takes no text: renamed, then the stop
        assert_eq!(s(&l.els[4].name), "Media");
        assert_eq!(t.media[0], b"hint");
        // a missing player in range stops too
        t.file(b"{2_Player Name:P2}\n{0_Hints}\nlate\n");
        assert_eq!(import(&mut l, &mut t, 1, false), 9);
        assert_eq!(t.media[0], b"hint");
    }

    #[test]
    fn version_0_names_effects_by_the_first_underscore() {
        let (mut l, mut t) = scenario();
        let file = b"{5_<A>}\nzero\n{2_Effect:<B>}\nzero two\n{9_Condition:<E>}\nxx\n";
        t.file(file);
        assert_eq!(import(&mut l, &mut t, 0, true), 9);
        assert_eq!(t.errors[0], (EE_SCENARIO_TEXT_ERROR_EFFECT_NAME, 0, "2_Effect:<B>".into(), "<B>".into()));
        assert_eq!(t.errors[1], (EE_SCENARIO_TEXT_ERROR_EFFECT_ID, 9, "9_Condition:<E>".into(), "".into()));
        t.file(file);
        assert_eq!(import(&mut l, &mut t, 0, false), 9);
        assert_eq!(s(&t.texts[1][0]), "zero");
        assert_eq!(s(&l.els[2].name), "Effect:<B>");
        // "<id>_" alone: the name of the entry before is kept
        let (mut l, mut t) = scenario();
        t.file(b"{5_Renamed}\none\n{2_}\ntwo\n");
        assert_eq!(import(&mut l, &mut t, 0, false), 0);
        assert_eq!(s(&l.els[2].name), "Renamed");
        assert_eq!(s(&t.texts[2][1]), "two");
    }

    #[test]
    fn a_file_without_a_final_new_line_gives_its_last_entry_no_text() {
        let (mut l, mut t) = scenario();
        t.file(b"{0_Hints}\none\n{2_History}\nlast");
        assert_eq!(import(&mut l, &mut t, 1, true), 0);
        assert_eq!(t.media[0], b"one");
        assert_eq!(t.media[2], b"");
        t.file(b"");
        assert_eq!(import(&mut l, &mut t, 1, true), 0);
    }

    #[test]
    fn atoi_reads_a_prefix_and_saturates() {
        assert_eq!(atoi(b" -12x"), -12);
        assert_eq!(atoi(b"+7"), 7);
        assert_eq!(atoi(b"99999999999"), i32::MAX);
        assert_eq!(atoi(b"-99999999999"), i32::MIN);
        assert_eq!(atoi(b"\xc3\xbc3"), 0);
    }
}
