//! The lobby's chat command parser as a Rust object (round 17): `ChatCommandParser`'s command tables (the names by
//! `WStringLessNoCase`, each id's display name), its settings (the prefix, the name-match threshold, colon emotes,
//! the room flags, the reply target) and what a parse leaves (the error, the command, the target's name, the text,
//! the duration, the invitation), with `HandleInput` / `ParseCommand` applying chat.rs's parse to them. Since R6
//! WONLobby round 17 the whole class: the room's client list and the target client (model shells, one reference
//! each, released target first as the members were), the clients' names read from the model, `InitCommands` over the
//! resource strings, `AddCommands`' list read, `FindClient` and `GetHelp`; the C++ class is inline forwards
//! (WONLobbyParser.h). The behaviour is `golden_chat.txt` (its `state` lines recorded over the C++ class before the
//! first move) and `won_chatparser_test` (ChatCommandParser.cpp's statements as they were, on model shells).

use std::cell::Cell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use ee_won_net::chat::{self, units, CrtChat, EeWonChatText, Input, BROADCAST, EMOTE};
use ee_won_net::strcase;

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
    /// `mClients`: the room's client list (a `LobbyClientList` shell, one reference; 0 none)
    clients: Cell<isize>,
    /// `mTarget`: the target client (a `LobbyClient` shell, one reference; 0 none)
    target: Cell<isize>,
    /// the last text a call returned (`GetHelp`)
    out: Vec<u32>,
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
            clients: Cell::new(0),
            target: Cell::new(0),
            out: Vec::new(),
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
        let x = unsafe { Box::from_raw(p) };
        x.release_members();
        drop(x);
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

// ---- the client list, the target, the commands' names and the help (R6 WONLobby round 17: ChatCommandParser.cpp's
// statements)

use crate::modelobj::{self, EeWonModelCall as MC, EeWonModelField as MF};
use ee_won_gui::guistrobj::{data_units, EE_WON_GSTR_WCHAR};

unsafe extern "C" fn token_sink(ctx: *mut c_void, u: *const u32, n: usize) {
    // SAFETY: the parser's `n` units, into the token being read
    let t = unsafe { &mut *(ctx as *mut Vec<u32>) };
    if n != 0 {
        t.extend_from_slice(unsafe { std::slice::from_raw_parts(u, n) });
    }
}

impl EeWonChatParser {
    /// The room's clients and their names, in the list's order (`ChatClients`: `ClientAt(i)->GetName()`).
    fn clients(&self) -> (Vec<isize>, Vec<Vec<u32>>) {
        let list = self.clients.get();
        if list == 0 {
            return (Vec::new(), Vec::new());
        }
        let n = modelobj::call(list, MC::ListCount, 0, 0, 0).max(0) as usize;
        let mut shells = Vec::with_capacity(n);
        let mut names = Vec::with_capacity(n);
        for i in 0..n {
            let c = modelobj::call(list, MC::ListAt, i as isize, 0, 0);
            shells.push(c);
            // SAFETY: a client's name is a live GUIStringData
            names.push(unsafe { data_units(modelobj::get(c, MF::ClientName) as *const c_void) });
        }
        (shells, names)
    }

    /// `SetClientList`: the list held (`mClients = clients`), then the room flags.
    pub fn set_client_list(&mut self, list: isize, room_flags: i32) {
        modelobj::assign(&self.clients, list);
        self.room_flags = room_flags as u32;
    }

    /// The held client list (`which` 0) or target client, no reference added.
    pub fn object(&self, which: i32) -> isize {
        if which == 0 {
            self.clients.get()
        } else {
            self.target.get()
        }
    }

    /// `FindClient`: by full name, else a unique prefix; the client (0 none) and the match kind.
    pub fn find_client(&self, name: &[u32]) -> (isize, i32) {
        let (shells, names) = self.clients();
        let (matched, found) = chat::find_client(name, &names, &CrtChat);
        (found.map_or(0, |i| shells[i]), matched)
    }

    /// `HandleInput` over the held list: the target cleared, the parse, the target the parse chose held.
    pub fn handle_input_list(&mut self, text: &[u32]) -> bool {
        modelobj::assign(&self.target, 0);
        let (shells, names) = self.clients();
        let has = self.clients.get() != 0;
        let (ok, at) = self.handle_input(text, &names, has);
        if let Some(i) = at {
            modelobj::assign(&self.target, shells[i]);
        }
        ok
    }

    /// `AddCommands`: the names (read as `StringParser::ReadValue(std::list<std::wstring>&)` from the text to its
    /// first NUL), each added; no names, the command's built-in short name.
    pub fn add_commands(&mut self, names: &[u32], id: i32) {
        if names.is_empty() {
            let short: Vec<u32> = chat::command_name(id).to_bytes().iter().map(|&b| u32::from(b)).collect();
            self.add_command(&short, id);
            return;
        }
        let mut text: Vec<u32> = names.iter().copied().take_while(|&u| u != 0).collect();
        text.push(0);
        let at = text.as_ptr() as *const c_void;
        let mut tokens: Vec<Vec<u32>> = Vec::new();
        let mut pos = 0i32;
        loop {
            let mut token = Vec::new();
            // SAFETY: a NUL-terminated text of 4-byte units, the cursor inside it
            unsafe {
                ee_won_net::ffi_parse::ee_won_parser_read_wstring(
                    at,
                    4,
                    &mut pos,
                    false,
                    token_sink,
                    &mut token as *mut Vec<u32> as *mut c_void,
                )
            };
            tokens.push(token);
            // SAFETY: as above
            if !unsafe { ee_won_net::ffi_parse::ee_won_parser_check_next_char(at, 4, &mut pos, i32::from(b',')) } {
                break;
            }
        }
        for t in &tokens {
            self.add_command(t, id);
        }
    }

    /// `InitCommands`: every command's localized names (its resource string; none for the two shutdown commands).
    pub fn init_commands(&mut self) {
        for &(row, command) in INIT_COMMANDS.iter() {
            let names = if row >= 0 { crate::resource::text(row as usize) } else { Vec::new() };
            self.add_commands(&names, command);
        }
    }

    /// `GetHelp(client_id)`: the header, then each help line the client is shown, each with a line break.
    pub fn help(&mut self, client_id: u16) -> &[u32] {
        let (mut admin, mut moderator, mut room_captain, mut game_captain) = (false, false, false, false);
        let list = self.clients.get();
        if list != 0 {
            let c = modelobj::call(list, MC::ListFind, client_id as isize, 0, 0);
            if c != 0 {
                moderator = modelobj::call(c, MC::ClientModerator, 0, 0, 0) != 0;
                let room = ((self.room_flags >> 1) & 1) as isize;
                room_captain = modelobj::call(c, MC::ClientIsCaptain, room, 0, 0) != 0;
                game_captain = modelobj::call(c, MC::ClientIsCaptain, 1, 0, 0) != 0;
                admin = modelobj::call(c, MC::ClientAdmin, 0, 0, 0) != 0;
            }
        }
        let line_break: Vec<u32> = "<n>".bytes().map(u32::from).collect();
        let mut out = crate::resource::text(R::ChatCommandLogic_HelpHeader_String as usize);
        out.extend_from_slice(&line_break);
        for c in chat::help_commands(self.room_flags, moderator, room_captain, game_captain, admin) {
            out.extend(crate::resource::text(ee_won_chat_help_row(c) as usize));
            out.extend_from_slice(&line_break);
        }
        self.out = out;
        &self.out
    }

    /// The destructor's member releases: the target, then the client list (reverse member order).
    fn release_members(&self) {
        modelobj::assign(&self.target, 0);
        modelobj::assign(&self.clients, 0);
    }
}

/// The units of a `wchar_t` text (`count` of them).
unsafe fn wide_text(text: *const c_void, count: usize) -> Vec<u32> {
    // SAFETY: the caller's contract
    unsafe { units_of(text, count, EE_WON_GSTR_WCHAR as u32) }
}

/// `SetClientList(list, room_flags)`.
///
/// # Safety
/// `p` is live; `list` a live `LobbyClientList` shell or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_set_client_list(
    p: *mut EeWonChatParser,
    list: *mut c_void,
    room_flags: i32,
) {
    obj_mut!(p).set_client_list(list as isize, room_flags)
}
/// The held client list (`which` 0) or target (1), no reference added.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_object(p: *const EeWonChatParser, which: i32) -> *mut c_void {
    obj!(p).object(which) as *mut c_void
}
/// `FindClient(name, matched)`: the client (no reference added) or NULL; `*matched` the kind when not NULL.
///
/// # Safety
/// `p` is live; `name` holds `count` `wchar_t`s; `matched` NULL or writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_find_client(
    p: *const EeWonChatParser,
    name: *const c_void,
    count: usize,
    matched: *mut i32,
) -> *mut c_void {
    let n = unsafe { wide_text(name, count) };
    let (c, m) = obj!(p).find_client(&n);
    if !matched.is_null() {
        // SAFETY: writable
        unsafe { *matched = m };
    }
    c as *mut c_void
}
/// `HandleInput(text)` over the held list (the target set).
///
/// # Safety
/// `p` is live; `text` holds `count` `wchar_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_handle_input_list(
    p: *mut EeWonChatParser,
    text: *const c_void,
    count: usize,
) -> bool {
    let t = unsafe { wide_text(text, count) };
    obj_mut!(p).handle_input_list(&t)
}
/// `AddCommands(names, id)`.
///
/// # Safety
/// `p` is live; `names` holds `count` `wchar_t`s.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_add_commands(
    p: *mut EeWonChatParser,
    names: *const c_void,
    count: usize,
    id: i32,
) {
    let n = unsafe { wide_text(names, count) };
    obj_mut!(p).add_commands(&n, id)
}
/// `InitCommands()`.
///
/// # Safety
/// `p` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_init_commands(p: *mut EeWonChatParser) {
    obj_mut!(p).init_commands()
}
/// `GetHelp(client_id)`'s text (`count` gets its units), valid until the parser's next call.
///
/// # Safety
/// `p` is live, `count` writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_help(
    p: *mut EeWonChatParser,
    client_id: u16,
    count: *mut usize,
) -> *const u32 {
    let s = obj_mut!(p).help(client_id);
    // SAFETY: writable
    unsafe { *count = s.len() };
    s.as_ptr()
}

// ---- InitCommands' and GetHelp's tables (R6 WONLobby round 15: ChatCommandParser.cpp's 24 AddCommands calls and its
// help switch)

use crate::resource::EeWonLobbyRes as R;

/// InitCommands' calls in the original's order: (the resource row of the command's names, the command); a row of
/// -1 is the empty list (the command's built-in short name).
const INIT_COMMANDS: [(i32, i32); 24] = [
    (R::ChatCommandLogic_Whisper_String as i32, chat::WHISPER),
    (R::ChatCommandLogic_Block_String as i32, chat::BLOCK),
    (R::ChatCommandLogic_Emote_String as i32, chat::EMOTE),
    (R::ChatCommandLogic_Ignore_String as i32, chat::IGNORE),
    (R::ChatCommandLogic_Away_String as i32, chat::AWAY),
    (R::ChatCommandLogic_Invite_String as i32, chat::INVITE),
    (R::ChatCommandLogic_Uninvite_String as i32, chat::UNINVITE),
    (R::ChatCommandLogic_Reply_String as i32, chat::REPLY),
    (R::ChatCommandLogic_Clear_String as i32, chat::CLEAR),
    (R::ChatCommandLogic_Help_String as i32, chat::HELP),
    (R::ChatCommandLogic_ShowTeam_String as i32, chat::SHOW_TEAM),
    (R::ChatCommandLogic_Moderator_String as i32, chat::MODERATOR),
    (R::ChatCommandLogic_ServerMute_String as i32, chat::SERVER_MUTE),
    (R::ChatCommandLogic_ServerBan_String as i32, chat::SERVER_BAN),
    (R::ChatCommandLogic_Mute_String as i32, chat::MUTE),
    (R::ChatCommandLogic_Ban_String as i32, chat::BAN),
    (R::ChatCommandLogic_Unmute_String as i32, chat::UNMUTE),
    (R::ChatCommandLogic_ServerUnmute_String as i32, chat::SERVER_UNMUTE),
    (R::ChatCommandLogic_Unban_String as i32, chat::UNBAN),
    (R::ChatCommandLogic_ServerUnban_String as i32, chat::SERVER_UNBAN),
    (R::ChatCommandLogic_Alert_String as i32, chat::ALERT),
    (-1, chat::START_SHUTDOWN),
    (-1, chat::ABORT_SHUTDOWN),
    (R::ChatCommandLogic_Warn_String as i32, chat::WARN),
];

/// InitCommands' `i`th call: the names' resource row (-1: none), the command in `*command`; -2 past the end.
///
/// # Safety
/// `command` is writable.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chatparser_init_command(i: usize, command: *mut i32) -> i32 {
    match INIT_COMMANDS.get(i) {
        Some(&(row, c)) => {
            // SAFETY: writable (the contract)
            unsafe { *command = c };
            row
        }
        None => -2,
    }
}

/// A command's help line's resource row (`ChatHelpString`: the warning's for any other command).
#[no_mangle]
pub extern "C" fn ee_won_chat_help_row(command: i32) -> i32 {
    (match command {
        chat::HELP => R::ChatCommandLogic_HelpHelp_String,
        chat::WHISPER => R::ChatCommandLogic_WhisperHelp_String,
        chat::REPLY => R::ChatCommandLogic_ReplyHelp_String,
        chat::EMOTE => R::ChatCommandLogic_EmoteHelp_String,
        chat::IGNORE => R::ChatCommandLogic_IgnoreHelp_String,
        chat::CLEAR => R::ChatCommandLogic_ClearHelp_String,
        chat::SHOW_TEAM => R::ChatCommandLogic_ShowTeamHelp_String,
        chat::BLOCK => R::ChatCommandLogic_BlockHelp_String,
        chat::AWAY => R::ChatCommandLogic_AwayHelp_String,
        chat::INVITE => R::ChatCommandLogic_InviteHelp_String,
        chat::UNINVITE => R::ChatCommandLogic_UninviteHelp_String,
        chat::MUTE => R::ChatCommandLogic_MuteHelp_String,
        chat::UNMUTE => R::ChatCommandLogic_UnmuteHelp_String,
        chat::BAN => R::ChatCommandLogic_BanHelp_String,
        chat::UNBAN => R::ChatCommandLogic_UnbanHelp_String,
        chat::MODERATOR => R::ChatCommandLogic_ModeratorHelp_String,
        chat::SERVER_MUTE => R::ChatCommandLogic_ServerMuteHelp_String,
        chat::SERVER_UNMUTE => R::ChatCommandLogic_ServerUnmuteHelp_String,
        chat::SERVER_BAN => R::ChatCommandLogic_ServerBanHelp_String,
        chat::SERVER_UNBAN => R::ChatCommandLogic_ServerUnbanHelp_String,
        chat::ALERT => R::ChatCommandLogic_AlertHelp_String,
        _ => R::ChatCommandLogic_WarnHelp_String,
    }) as i32
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

    /// `AddCommands` (round 17): the list read as StringParser reads it (quotes, trailing spaces cut, the text to its
    /// first NUL); no names, the built-in short name; with no list held no client is found, no target set, and the
    /// help (the resources unbound) is the line breaks alone.
    #[test]
    fn add_commands_and_no_list() {
        let mut p = EeWonChatParser::new();
        p.add_commands(&w("a, \"b c\" ,d\u{0}e"), 40);
        assert_eq!(p.lookup(&w("A")), Some(40));
        assert_eq!(p.lookup(&w("b c")), Some(40));
        assert_eq!(p.lookup(&w("d")), Some(40));
        assert_eq!(p.lookup(&w("e")), None);
        assert_eq!(p.command_string(40), &w("a")[..]);
        p.add_commands(&[], chat::EMOTE);
        assert_eq!(p.lookup(&w("em")), Some(chat::EMOTE));
        assert_eq!(p.find_client(&w("x")), (0, 0));
        assert!(!p.handle_input_list(&w("hi")));
        assert_eq!(p.number(EeWonChatParserNumber::Error), 6);
        assert_eq!(p.object(1), 0);
        let n = chat::help_commands(0, false, false, false, false).len();
        assert_eq!(p.help(1), &w(&"<n>".repeat(n + 1))[..]);
    }

    /// A text read in place from a block laid out as a C++ `GUIStringData`.
    #[test]
    fn data_units_in_place() {
        use ee_won_gui::guistrobj::{data_units, ee_won_gstr, EeWonGstrOp, EeWonGuiStrBuf, EE_WON_GSTR_WCHAR};
        #[repr(C)]
        struct Data {
            vt: usize,
            rc: usize,
            buf: EeWonGuiStrBuf,
        }
        let mut d = Data { vt: 0, rc: 0, buf: EeWonGuiStrBuf::new() };
        let t: Vec<u32> = w("Bob");
        let ws = EE_WON_GSTR_WCHAR as u32;
        let units: Vec<u8> = if ws == 2 {
            t.iter().flat_map(|&u| (u as u16).to_ne_bytes()).collect()
        } else {
            t.iter().flat_map(|&u| u.to_ne_bytes()).collect()
        };
        unsafe {
            ee_won_gstr(EeWonGstrOp::AssignUnits, &mut d.buf, units.as_ptr() as *const c_void, 3, 0, 0, ws);
            assert_eq!(data_units(&d as *const Data as *const c_void), t);
            assert!(data_units(std::ptr::null()).is_empty());
            ee_won_gstr(EeWonGstrOp::Free, &mut d.buf, std::ptr::null(), 0, 0, 0, ws);
        }
        assert_eq!((d.vt, d.rc), (0, 0));
    }

    /// The tables as ChatCommandParser.cpp had them: every command but none and broadcast named once, in its order.
    #[test]
    fn init_and_help_tables() {
        let mut c = 0;
        assert_eq!(unsafe { ee_won_chatparser_init_command(0, &mut c) }, R::ChatCommandLogic_Whisper_String as i32);
        assert_eq!(c, chat::WHISPER);
        assert_eq!(unsafe { ee_won_chatparser_init_command(21, &mut c) }, -1);
        assert_eq!(c, chat::START_SHUTDOWN);
        assert_eq!(unsafe { ee_won_chatparser_init_command(23, &mut c) }, R::ChatCommandLogic_Warn_String as i32);
        assert_eq!(unsafe { ee_won_chatparser_init_command(24, &mut c) }, -2);
        let mut seen: Vec<i32> = INIT_COMMANDS.iter().map(|&(_, c)| c).collect();
        seen.sort();
        assert_eq!(seen, (2..=25).collect::<Vec<_>>());
        assert_eq!(ee_won_chat_help_row(chat::REPLY), R::ChatCommandLogic_ReplyHelp_String as i32);
        assert_eq!(ee_won_chat_help_row(chat::WARN), R::ChatCommandLogic_WarnHelp_String as i32);
        assert_eq!(ee_won_chat_help_row(chat::START_SHUTDOWN), R::ChatCommandLogic_WarnHelp_String as i32);
    }
}
