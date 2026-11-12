//! The lobby's chat command parser as a Rust object (round 17): `ChatCommandParser`'s command tables (the names by
//! `WStringLessNoCase`, each id's display name), its settings (the prefix, the name-match threshold, colon emotes,
//! the room flags, the reply target) and what a parse leaves (the error, the command, the target's name, the text,
//! the duration, the invitation), with `HandleInput` / `ParseCommand` applying chat.rs's parse to them. The C++ class
//! keeps the room's client list and the target client (references to C++ objects): it hands the clients' names in
//! and sets the target from the position the parse returns. The behaviour is `golden_chat.txt` (its `state` lines
//! recorded over the C++ class before the move).

use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::chat::{self, units, CrtChat, EeWonChatText, Input, BROADCAST, EMOTE};
use crate::strcase;

/// A number of the parser.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonChatParserNumber {
    Error = 0,
    Command = 1,
    Prefix = 2,
    RoomFlags = 3,
    Duration = 4,
    Invitation = 5,
    MatchThreshold = 6,
    AllowColonEmote = 7,
}

/// A text of the parser.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonChatParserText {
    /// `mInput`: the reply target's name
    Input = 0,
    TargetName = 1,
    Text = 2,
}

fn wide(u: &[u32]) -> Vec<i64> {
    u.iter().map(|&x| i64::from(x)).collect()
}

/// `ChatCommandParser`'s state (see the module).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct EeWonChatParser {
    /// `mCommands`: name -> id, in `WStringCompareNoCase` order (a name present keeps its first spelling)
    commands: Vec<(Vec<u32>, i32)>,
    /// `mCommandStrings`: id -> the first name added for it
    strings: BTreeMap<i32, Vec<u32>>,
    threshold: i32,
    allow_colon: bool,
    error: i32,
    command: i32,
    target_name: Vec<u32>,
    text: Vec<u32>,
    invitation: bool,
    duration: u32,
    prefix: i32,
    room_flags: u32,
    input: Vec<u32>,
}

impl Default for EeWonChatParser {
    fn default() -> Self {
        Self::new()
    }
}

impl EeWonChatParser {
    /// The constructor: '/' prefix, a threshold of 2, no colon emotes, no room flags. The original left the error,
    /// the command and the invitation unset: 0, 0 and false here.
    pub fn new() -> EeWonChatParser {
        EeWonChatParser {
            commands: Vec::new(),
            strings: BTreeMap::new(),
            threshold: 2,
            allow_colon: false,
            error: 0,
            command: 0,
            target_name: Vec::new(),
            text: Vec::new(),
            invitation: false,
            duration: 0,
            prefix: i32::from(b'/'),
            room_flags: 0,
            input: Vec::new(),
        }
    }

    fn search(&self, name: &[u32]) -> Result<usize, usize> {
        let key = wide(name);
        self.commands.binary_search_by(|(k, _)| strcase::wide_compare_no_case(&wide(k), &key).cmp(&0))
    }

    /// `AddCommand`: `mCommands[name] = id` (an empty name ignored); the first name added for an id is its string.
    pub fn add_command(&mut self, name: &[u32], id: i32) {
        if name.is_empty() {
            return;
        }
        match self.search(name) {
            Ok(i) => self.commands[i].1 = id,
            Err(i) => self.commands.insert(i, (name.to_vec(), id)),
        }
        self.strings.entry(id).or_insert_with(|| name.to_vec());
    }

    /// `GetCommandString`: the id's string, else the first name (in the table's order) with that id, else empty.
    pub fn command_string(&self, id: i32) -> &[u32] {
        if let Some(s) = self.strings.get(&id) {
            return s;
        }
        self.commands.iter().find(|(_, c)| *c == id).map_or(&[], |(k, _)| k)
    }

    /// The command id of a command word (`mCommands.find`), if any.
    pub fn lookup(&self, word: &[u32]) -> Option<i32> {
        self.search(word).ok().map(|i| self.commands[i].1)
    }

    pub fn number(&self, n: EeWonChatParserNumber) -> i64 {
        use EeWonChatParserNumber as N;
        match n {
            N::Error => i64::from(self.error),
            N::Command => i64::from(self.command),
            N::Prefix => i64::from(self.prefix),
            N::RoomFlags => i64::from(self.room_flags),
            N::Duration => i64::from(self.duration),
            N::Invitation => i64::from(self.invitation),
            N::MatchThreshold => i64::from(self.threshold),
            N::AllowColonEmote => i64::from(self.allow_colon),
        }
    }
    pub fn set_number(&mut self, n: EeWonChatParserNumber, v: i64) {
        use EeWonChatParserNumber as N;
        match n {
            N::Error => self.error = v as i32,
            N::Command => self.command = v as i32,
            N::Prefix => self.prefix = v as i32,
            N::RoomFlags => self.room_flags = v as u32,
            N::Duration => self.duration = v as u32,
            N::Invitation => self.invitation = v != 0,
            N::MatchThreshold => self.threshold = v as i32,
            N::AllowColonEmote => self.allow_colon = v != 0,
        }
    }
    pub fn text(&self, t: EeWonChatParserText) -> &[u32] {
        match t {
            EeWonChatParserText::Input => &self.input,
            EeWonChatParserText::TargetName => &self.target_name,
            EeWonChatParserText::Text => &self.text,
        }
    }
    pub fn set_text(&mut self, t: EeWonChatParserText, v: Vec<u32>) {
        match t {
            EeWonChatParserText::Input => self.input = v,
            EeWonChatParserText::TargetName => self.target_name = v,
            EeWonChatParserText::Text => self.text = v,
        }
    }

    /// `ParseCommand`: the command word, its id, chat.rs's parse applied; the target (`Some(None)`: cleared,
    /// `Some(Some(i))`: the i-th client) when the parse set it.
    fn parse_command(&mut self, text: &[u32], clients: &[Vec<u32>]) -> (bool, Option<Option<usize>>) {
        let mut pos = 1;
        let word = chat::get_token(text, &mut pos, true, false, &CrtChat);
        let id = self.lookup(&word);
        let o = chat::parse_command(text, pos, id, &self.input, clients, self.threshold, &CrtChat);
        self.command = o.command;
        if o.error != 0 {
            self.error = o.error;
        }
        if let Some(n) = o.target_name {
            self.target_name = n;
        }
        if let Some(t) = o.text {
            self.text = t;
        }
        if let Some(d) = o.duration {
            self.duration = d;
        }
        if let Some(i) = o.invitation {
            self.invitation = i;
        }
        (o.ok, o.target)
    }

    /// `HandleInput`: the error, target and target name reset, then a command, an emote or a broadcast; the result
    /// and the target to set (always set: cleared first, then whatever the parse chose).
    pub fn handle_input(&mut self, text: &[u32], clients: &[Vec<u32>], has_clients: bool) -> (bool, Option<usize>) {
        self.error = 0;
        self.target_name.clear();
        match chat::handle_input(text, self.prefix, self.allow_colon, has_clients, &CrtChat) {
            Input::NoClients => {
                self.error = 6;
                (false, None)
            }
            Input::Command => {
                let (ok, target) = self.parse_command(text, clients);
                (ok, target.flatten())
            }
            Input::Emote { text, ok } => {
                self.command = EMOTE;
                self.text = text;
                if !ok {
                    self.error = 5;
                }
                (ok, None)
            }
            Input::Broadcast { ok } => {
                self.command = BROADCAST;
                if ok {
                    self.text = text.to_vec();
                } else {
                    self.error = 5;
                }
                (ok, None)
            }
        }
    }
}

// ---- FFI (ChatCommandParser.cpp)

unsafe fn units_of(text: *const c_void, count: usize, unit_size: u32) -> Vec<u32> {
    // SAFETY: the caller's contract
    unsafe { units(EeWonChatText { text, length: count }, unit_size) }
}

macro_rules! obj {
    ($p:expr) => {
        // SAFETY: a live parser (the caller's contract)
        unsafe { &*$p }
    };
}
macro_rules! obj_mut {
    ($p:expr) => {
        // SAFETY: a live parser, not otherwise borrowed (the caller's contract)
        unsafe { &mut *$p }
    };
}

/// A new parser (the constructor's state).
#[no_mangle]
pub extern "C" fn ee_won_chatparser_new() -> *mut EeWonChatParser {
    Box::into_raw(Box::new(EeWonChatParser::new()))
}
/// Frees the parser.
///
/// # Safety
/// `p` from `ee_won_chatparser_new` (or null), not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_free(p: *mut EeWonChatParser) {
    if !p.is_null() {
        // SAFETY: the caller's object
        drop(unsafe { Box::from_raw(p) });
    }
}
/// A number.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_number(p: *const EeWonChatParser, n: EeWonChatParserNumber) -> i64 {
    obj!(p).number(n)
}
/// Sets a number.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_set_number(p: *mut EeWonChatParser, n: EeWonChatParserNumber, v: i64) {
    obj_mut!(p).set_number(n, v)
}
/// A text (`count` gets its units), valid until the parser changes.
///
/// # Safety
/// `p` is live, `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_text(
    p: *const EeWonChatParser,
    t: EeWonChatParserText,
    count: *mut usize,
) -> *const u32 {
    let s = obj!(p).text(t);
    // SAFETY: writable
    unsafe { *count = s.len() };
    s.as_ptr()
}
/// Sets a text (`count` units of `unit_size` bytes).
///
/// # Safety
/// `p` is live; `text` holds the units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_set_text(
    p: *mut EeWonChatParser,
    t: EeWonChatParserText,
    text: *const c_void,
    count: usize,
    unit_size: u32,
) {
    let v = unsafe { units_of(text, count, unit_size) };
    obj_mut!(p).set_text(t, v)
}
/// `AddCommand`.
///
/// # Safety
/// As `ee_won_chatparser_set_text`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_add_command(
    p: *mut EeWonChatParser,
    name: *const c_void,
    count: usize,
    unit_size: u32,
    id: i32,
) {
    let v = unsafe { units_of(name, count, unit_size) };
    obj_mut!(p).add_command(&v, id)
}
/// `GetCommandString`'s text (`count` gets its units), valid until the parser changes.
///
/// # Safety
/// `p` is live, `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_command_string(
    p: *const EeWonChatParser,
    id: i32,
    count: *mut usize,
) -> *const u32 {
    let s = obj!(p).command_string(id);
    // SAFETY: writable
    unsafe { *count = s.len() };
    s.as_ptr()
}
/// `HandleInput` over the room's client names (`clients`, `client_count`; `has_clients` whether the parser has a
/// list): the result; `target` gets the target client's position, or -1 for none.
///
/// # Safety
/// `p` is live; the texts as `EeWonChatText` of `wchar_size` units; `target` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_handle_input(
    p: *mut EeWonChatParser,
    text: EeWonChatText,
    clients: *const EeWonChatText,
    client_count: usize,
    has_clients: bool,
    wchar_size: u32,
    target: *mut i32,
) -> bool {
    let t = unsafe { units(text, wchar_size) };
    let names: Vec<Vec<u32>> = if clients.is_null() || client_count == 0 {
        Vec::new()
    } else {
        // SAFETY: `client_count` texts
        unsafe { std::slice::from_raw_parts(clients, client_count) }
            .iter()
            .map(|&c| unsafe { units(c, wchar_size) })
            .collect()
    };
    let (ok, at) = obj_mut!(p).handle_input(&t, &names, has_clients);
    // SAFETY: writable
    unsafe { *target = at.map_or(-1, |i| i as i32) };
    ok
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(s: &str) -> Vec<u32> {
        s.encode_utf16().map(u32::from).collect()
    }

    /// The tables: a name present keeps its spelling and takes the new id; an id's string is its first name.
    #[test]
    fn tables() {
        let mut p = EeWonChatParser::new();
        p.add_command(&w("w"), 2);
        p.add_command(&w("whisper"), 2);
        p.add_command(&w("W"), 99);
        p.add_command(&w(""), 5);
        assert_eq!(p.lookup(&w("W")), Some(99));
        assert_eq!(p.lookup(&w("WHISPER")), Some(2));
        assert_eq!(p.command_string(2), &w("w")[..]);
        assert_eq!(p.command_string(99), &w("W")[..]);
        assert_eq!(p.command_string(5), &[] as &[u32]);
    }

    /// A broadcast keeps the line, an empty one only the error; no list, error 6; an emote with colons on.
    #[test]
    fn inputs() {
        let mut p = EeWonChatParser::new();
        p.set_text(EeWonChatParserText::Text, w("~"));
        assert_eq!(p.handle_input(&w("hi"), &[], true), (true, None));
        assert_eq!((p.number(EeWonChatParserNumber::Command), p.text(EeWonChatParserText::Text)), (1, &w("hi")[..]));
        assert_eq!(p.handle_input(&w("  "), &[], true), (false, None));
        assert_eq!((p.number(EeWonChatParserNumber::Error), p.text(EeWonChatParserText::Text)), (5, &w("hi")[..]));
        assert_eq!(p.handle_input(&w("hi"), &[], false), (false, None));
        assert_eq!(p.number(EeWonChatParserNumber::Error), 6);
        p.set_number(EeWonChatParserNumber::AllowColonEmote, 1);
        assert_eq!(p.handle_input(&w(":wave"), &[], true), (true, None));
        assert_eq!((p.number(EeWonChatParserNumber::Command), p.text(EeWonChatParserText::Text)), (3, &w("wave")[..]));
    }
}
