//! The lobby's chat command parser (`ChatCommandParser`): a line of chat input as a slash command, a colon emote
//! or a broadcast; the command's target by full name or unique prefix, its text, its duration (`10m`, `2h`,
//! `INFINITE`); which help lines a client is shown; the built-in short names. The C++ class keeps its members, the
//! command tables (std::maps, looked up between the two steps of a command) and the client list, and applies what
//! these return. Text is `wchar_t` units (`u32` here, any width); the character classes and case maps are the C
//! runtime's (`towupper`, `iswspace`, `toupper`, `isalpha`, `atoi`), called as the original called them.

use std::ffi::{c_char, c_void, CStr};

use crate::ffi_parse::EeWonWideUnitSink;

/// The C runtime's functions the parser calls.
pub trait ChatCtype {
    /// `towupper` of a `wchar_t` (truncated to `wint_t`).
    fn towupper(&self, c: u32) -> u32;
    /// `iswspace` of a `wchar_t`.
    fn iswspace(&self, c: u32) -> bool;
    /// `toupper` (GUIString::toUpper, units below 256).
    fn toupper(&self, c: u32) -> u32;
    /// `tolower` (GUIString::toLowerCase, units below 256).
    fn tolower(&self, c: u32) -> u32;
    /// `isspace` of a `wchar_t` as an int (GUIString::ToInt).
    fn isspace(&self, c: i32) -> bool;
    /// `isalpha` of a signed `char`.
    fn isalpha(&self, c: i32) -> bool;
    /// `atoi` of narrow text.
    fn atoi(&self, text: &[u8]) -> i32;
}

/// The "C" locale (the tests' and, with the UI thread's `LC_CTYPE` "C", the game's): ASCII only.
pub struct CChat;

impl ChatCtype for CChat {
    fn towupper(&self, c: u32) -> u32 {
        if (0x61..=0x7a).contains(&c) {
            c - 0x20
        } else {
            c
        }
    }
    fn iswspace(&self, c: u32) -> bool {
        matches!(c, 0x09..=0x0d | 0x20)
    }
    fn toupper(&self, c: u32) -> u32 {
        self.towupper(c)
    }
    fn tolower(&self, c: u32) -> u32 {
        if (0x41..=0x5a).contains(&c) {
            c + 0x20
        } else {
            c
        }
    }
    fn isspace(&self, c: i32) -> bool {
        matches!(c, 0x09..=0x0d | 0x20)
    }
    fn isalpha(&self, c: i32) -> bool {
        (0x41..=0x5a).contains(&c) || (0x61..=0x7a).contains(&c)
    }
    fn atoi(&self, text: &[u8]) -> i32 {
        let mut i = 0;
        while i < text.len() && matches!(text[i], 0x09..=0x0d | 0x20) {
            i += 1;
        }
        let negative = i < text.len() && text[i] == b'-';
        if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
            i += 1;
        }
        let mut v: i64 = 0;
        while i < text.len() && text[i].is_ascii_digit() {
            v = (v * 10 + i64::from(text[i] - b'0')).min(1 << 32);
            i += 1;
        }
        let v = if negative { -v } else { v };
        v.clamp(i64::from(i32::MIN), i64::from(i32::MAX)) as i32
    }
}

pub(crate) const NONE: i32 = 0;
#[allow(dead_code)]
pub(crate) const BROADCAST: i32 = 1;
pub(crate) const WHISPER: i32 = 2;
pub(crate) const EMOTE: i32 = 3;
pub(crate) const BLOCK: i32 = 4;
pub(crate) const IGNORE: i32 = 5;
pub(crate) const AWAY: i32 = 6;
pub(crate) const MODERATOR: i32 = 7;
pub(crate) const SERVER_MUTE: i32 = 8;
pub(crate) const SERVER_BAN: i32 = 9;
pub(crate) const MUTE: i32 = 10;
pub(crate) const BAN: i32 = 11;
pub(crate) const UNMUTE: i32 = 12;
pub(crate) const SERVER_UNMUTE: i32 = 13;
pub(crate) const UNBAN: i32 = 14;
pub(crate) const SERVER_UNBAN: i32 = 15;
pub(crate) const INVITE: i32 = 16;
pub(crate) const UNINVITE: i32 = 17;
pub(crate) const REPLY: i32 = 18;
pub(crate) const CLEAR: i32 = 19;
pub(crate) const HELP: i32 = 20;
pub(crate) const SHOW_TEAM: i32 = 21;
pub(crate) const ALERT: i32 = 22;
pub(crate) const START_SHUTDOWN: i32 = 23;
pub(crate) const ABORT_SHUTDOWN: i32 = 24;
pub(crate) const WARN: i32 = 25;

/// `GetCommandName`: a command's built-in short name, registered when the resource gives it none.
pub fn command_name(command: i32) -> &'static CStr {
    match command {
        WHISPER => c"w",
        EMOTE => c"em",
        BLOCK => c"b",
        IGNORE => c"i",
        AWAY => c"afk",
        MODERATOR => c"mod",
        SERVER_MUTE => c"smute",
        SERVER_BAN => c"sban",
        MUTE => c"mute",
        BAN => c"ban",
        UNMUTE => c"unmute",
        SERVER_UNMUTE => c"sunmute",
        UNBAN => c"unban",
        SERVER_UNBAN => c"sunban",
        INVITE => c"invite",
        UNINVITE => c"uninvite",
        REPLY => c"r",
        CLEAR => c"cls",
        HELP => c"?",
        SHOW_TEAM => c"team",
        ALERT => c"alert",
        START_SHUTDOWN => c"startshutdown",
        ABORT_SHUTDOWN => c"abortshutdown",
        WARN => c"warn",
        _ => c"",
    }
}

/// A unit of the text, the terminator (0) past its end as GUIString's `operator[]` reads it.
fn at(text: &[u32], pos: usize) -> u32 {
    text.get(pos).copied().unwrap_or(0)
}

/// `GUIString::find`: the first `ch` at or after `start`.
fn find(text: &[u32], ch: u32, start: usize) -> Option<usize> {
    text.iter().skip(start).position(|&c| c == ch).map(|i| i + start)
}

/// `CompareNames`: (equal ignoring case, `a` a prefix of `b`); a differing character answers (false, false).
pub fn compare_names(a: &[u32], b: &[u32], ct: &impl ChatCtype) -> (bool, bool) {
    if a.iter().zip(b).any(|(&x, &y)| ct.towupper(x) != ct.towupper(y)) {
        return (false, false);
    }
    (a.len() == b.len(), a.len() <= b.len())
}

fn skip_whitespace(text: &[u32], pos: &mut usize, ct: &impl ChatCtype) {
    while *pos < text.len() && ct.iswspace(text[*pos]) {
        *pos += 1;
    }
}

/// `GetToken`: the next word up to a space (only ' ' ends it), or with `quotes` a quoted string; upper-cased
/// with `upper`.
pub fn get_token(text: &[u32], pos: &mut usize, upper: bool, quotes: bool, ct: &impl ChatCtype) -> Vec<u32> {
    skip_whitespace(text, pos, ct);
    if *pos >= text.len() {
        return Vec::new();
    }
    if quotes && text[*pos] == u32::from(b'"') {
        if let Some(end) = find(text, u32::from(b'"'), *pos + 1) {
            let mut token = text[*pos + 1..end].to_vec();
            *pos = end + 1;
            if upper {
                for c in &mut token {
                    if *c < 256 {
                        *c = ct.toupper(*c);
                    }
                }
            }
            return token;
        }
    }
    let mut token = Vec::new();
    while *pos < text.len() {
        let mut ch = text[*pos];
        if upper {
            ch = ct.towupper(ch);
        }
        if ch == u32::from(b' ') {
            return token;
        }
        token.push(ch);
        *pos += 1;
    }
    token
}

/// `GetRestOfLine` (the position is the caller's copy): a quoted string, else the rest without trailing spaces;
/// a rest of one character reads as empty.
pub fn rest_of_line(text: &[u32], mut pos: usize, ct: &impl ChatCtype) -> Vec<u32> {
    skip_whitespace(text, &mut pos, ct);
    if pos >= text.len() {
        return Vec::new();
    }
    if text[pos] == u32::from(b'"') {
        if let Some(end) = find(text, u32::from(b'"'), pos + 1) {
            return text[pos + 1..end].to_vec();
        }
    }
    let mut end = text.len() - 1;
    while end > pos && text[end] == u32::from(b' ') {
        end -= 1;
    }
    if end > pos {
        text[pos..=end].to_vec()
    } else {
        Vec::new()
    }
}

/// `FindClient`'s match: 0 none, 1 ambiguous (two full matches or two prefixes), 2 a unique prefix, 3 a full
/// name; and which client (in the list's order) for 2 and 3.
pub fn find_client(name: &[u32], clients: &[Vec<u32>], ct: &impl ChatCtype) -> (i32, Option<usize>) {
    let mut matched = 0;
    let mut found = None;
    for (i, client) in clients.iter().enumerate() {
        let (equal, prefix) = compare_names(name, client, ct);
        if equal {
            if matched == 3 {
                matched = 1;
                found = None;
                break;
            }
            matched = 3;
            found = Some(i);
        } else if prefix {
            if matched == 0 {
                matched = 2;
                found = Some(i);
            } else if matched == 2 {
                matched = 1;
                found = None;
                break;
            }
        }
    }
    if matched < 2 {
        found = None;
    }
    (matched, found)
}

/// What a parse sets on the parser (`None`: left as it was).
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Outcome {
    pub ok: bool,
    pub error: i32,
    pub command: i32,
    pub target_name: Option<Vec<u32>>,
    /// `Some(None)`: the target set to NULL.
    pub target: Option<Option<usize>>,
    pub text: Option<Vec<u32>>,
    pub duration: Option<u32>,
    pub invitation: Option<bool>,
}

/// `ParseCommand` after the command's lookup: `pos` is past the command word, `command` the id found (`None`:
/// unknown), `reply` the parser's reply target (`mInput`), `clients` the room's names in the list's order.
pub fn parse_command(
    text: &[u32],
    mut pos: usize,
    command: Option<i32>,
    reply: &[u32],
    clients: &[Vec<u32>],
    threshold: i32,
    ct: &impl ChatCtype,
) -> Outcome {
    let mut out = Outcome::default();
    let Some(command) = command else {
        out.command = NONE;
        out.error = 1;
        return out;
    };
    out.command = command;
    let mut need_target = false;
    let mut target_name: Option<Vec<u32>> = None;
    match command {
        REPLY => {
            target_name = Some(reply.to_vec());
            need_target = true;
        }
        IGNORE => {
            let n = rest_of_line(text, pos, ct);
            need_target = !n.is_empty();
            target_name = Some(n);
        }
        BLOCK => {
            target_name = Some(rest_of_line(text, pos, ct));
            need_target = true;
        }
        UNMUTE | SERVER_UNMUTE => {
            let n = rest_of_line(text, pos, ct);
            need_target = !n.is_empty();
            target_name = Some(n);
        }
        WHISPER | SERVER_MUTE | SERVER_BAN | MUTE | BAN | INVITE | UNINVITE | WARN => {
            target_name = Some(get_token(text, &mut pos, false, true, ct));
            need_target = true;
        }
        _ => {}
    }
    if need_target {
        let name = target_name.clone().unwrap_or_default();
        out.target_name = target_name;
        if name.is_empty() {
            out.error = 2;
            return out;
        }
        let (matched, found) = find_client(&name, clients, ct);
        out.target = Some(found);
        if matched < threshold {
            out.error = if matched == 1 { 4 } else { 3 };
            return out;
        }
    } else {
        out.target_name = target_name;
    }
    skip_whitespace(text, &mut pos, ct);
    let rest = || text[pos.min(text.len())..].to_vec();
    match command {
        WHISPER | EMOTE | ALERT | WARN => {
            if pos >= text.len() {
                out.error = 5;
                return out;
            }
            out.text = Some(rest());
        }
        REPLY => {
            let t = rest();
            if !t.is_empty() {
                out.command = WHISPER;
            }
            out.text = Some(t);
        }
        INVITE | UNINVITE => {
            out.text = Some(rest());
            out.invitation = Some(command == INVITE);
        }
        SERVER_MUTE | SERVER_BAN | MUTE | BAN | START_SHUTDOWN => {
            // the token through GUIString's std::string: a unit above 255 is '?'
            let token = get_token(text, &mut pos, true, false, ct);
            let mut duration: Vec<u8> = token.iter().map(|&c| if c < 256 { c as u8 } else { b'?' }).collect();
            if duration != b"INFINITE" && !duration.is_empty() {
                let unit = *duration.last().unwrap_or(&0);
                if ct.isalpha(i32::from(unit as i8)) {
                    duration.pop();
                }
                let mut value = ct.atoi(&duration);
                match unit {
                    b'D' => value = value.wrapping_mul(24).wrapping_mul(60).wrapping_mul(60),
                    b'H' => value = value.wrapping_mul(60).wrapping_mul(60),
                    b'M' => value = value.wrapping_mul(60),
                    b'S' => {}
                    _ => value = 0,
                }
                if value == 0 {
                    out.error = 7;
                    return out;
                }
                out.duration = Some(value as u32);
            }
            out.text = Some(rest_of_line(text, pos, ct));
        }
        UNMUTE | SERVER_UNMUTE => out.text = Some(rest_of_line(text, pos, ct)),
        _ => {}
    }
    out.ok = true;
    out
}

/// `HandleInput`'s kinds of line.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Input {
    /// No client list: error 6.
    NoClients,
    /// A slash command: `ParseCommand`.
    Command,
    /// A colon emote: its text (set even when empty), ok or error 5.
    Emote { text: Vec<u32>, ok: bool },
    /// A broadcast: ok (the text is the line) or error 5.
    Broadcast { ok: bool },
}

/// `HandleInput`'s decision.
pub fn handle_input(text: &[u32], prefix: i32, allow_colon: bool, has_clients: bool, ct: &impl ChatCtype) -> Input {
    if !has_clients {
        return Input::NoClients;
    }
    let first = at(text, 0);
    if first as i32 == prefix {
        return Input::Command;
    }
    if allow_colon && first == u32::from(b':') {
        let rest = text[1.min(text.len())..].to_vec();
        let mut pos = 0;
        skip_whitespace(&rest, &mut pos, ct);
        let ok = pos != rest.len();
        return Input::Emote { text: rest, ok };
    }
    let mut pos = 0;
    skip_whitespace(text, &mut pos, ct);
    Input::Broadcast { ok: pos != text.len() }
}

/// `GetHelp`'s lines after the header: the commands whose help a client is shown (LAN rooms: bit 3 of the room
/// flags; game rooms: bit 1 adds the team help and makes the room captain the game captain).
pub fn help_commands(
    room_flags: u32,
    moderator: bool,
    room_captain: bool,
    game_captain: bool,
    admin: bool,
) -> Vec<i32> {
    let lan = (room_flags >> 3) & 1 != 0;
    let mut out = vec![HELP, WHISPER, REPLY, EMOTE, IGNORE, CLEAR];
    if room_flags & 2 != 0 {
        out.push(SHOW_TEAM);
    }
    if !lan {
        out.extend([BLOCK, AWAY]);
        if game_captain {
            out.extend([INVITE, UNINVITE]);
        }
        if room_captain || moderator {
            out.extend([MUTE, UNMUTE, BAN, UNBAN]);
            if moderator {
                out.extend([MODERATOR, SERVER_MUTE, SERVER_UNMUTE, SERVER_BAN, SERVER_UNBAN, WARN]);
            }
        }
        if admin {
            out.push(ALERT);
        }
    }
    out
}

// ---- the C ABI (ChatCommandParser.cpp)

/// `wchar_t` text handed in: `length` units of `wchar_size` bytes.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeWonChatText {
    pub text: *const c_void,
    pub length: usize,
}

pub(crate) unsafe fn units(t: EeWonChatText, wchar_size: u32) -> Vec<u32> {
    if t.text.is_null() || t.length == 0 {
        return Vec::new();
    }
    // SAFETY: the caller's contract: `length` units of `wchar_size` bytes
    unsafe {
        if wchar_size == 2 {
            std::slice::from_raw_parts(t.text as *const u16, t.length).iter().map(|&u| u32::from(u)).collect()
        } else {
            std::slice::from_raw_parts(t.text as *const u32, t.length).to_vec()
        }
    }
}

unsafe fn names_of(clients: *const EeWonChatText, count: usize, wchar_size: u32) -> Vec<Vec<u32>> {
    if clients.is_null() || count == 0 {
        return Vec::new();
    }
    // SAFETY: `count` texts (the caller's contract)
    unsafe { std::slice::from_raw_parts(clients, count) }.iter().map(|&c| unsafe { units(c, wchar_size) }).collect()
}

/// The parse's result for the C++ to apply; `*_set` false leaves that member alone.
#[repr(C)]
#[derive(Default)]
pub struct EeWonChatOutcome {
    pub ok: bool,
    pub error: i32,
    pub command: i32,
    pub target_set: bool,
    /// The client's index in the names handed in, -1 for NULL.
    pub target: i32,
    pub duration_set: bool,
    pub duration: u32,
    pub invitation_set: bool,
    pub invitation: bool,
}

/// The sink's `which`: the target name.
pub const EE_WON_CHAT_TARGET_NAME: i32 = 0;
/// The sink's `which`: the text.
pub const EE_WON_CHAT_TEXT: i32 = 1;

/// Receives a text the parse set (`which`: `EE_WON_CHAT_TARGET_NAME` or `EE_WON_CHAT_TEXT`).
pub type EeWonChatSink = unsafe extern "C" fn(context: *mut c_void, which: i32, units: *const u32, count: usize);

/// cbindgen:ignore
#[cfg(not(test))]
mod crt {
    use std::ffi::{c_char, c_int};
    #[cfg(windows)]
    pub(crate) type WInt = u16;
    #[cfg(not(windows))]
    pub(crate) type WInt = u32;
    extern "C" {
        pub fn towupper(c: WInt) -> WInt;
        pub fn iswspace(c: WInt) -> c_int;
        pub fn toupper(c: c_int) -> c_int;
        pub fn tolower(c: c_int) -> c_int;
        pub fn isspace(c: c_int) -> c_int;
        pub fn isalpha(c: c_int) -> c_int;
        pub fn atoi(s: *const c_char) -> c_int;
    }
}

/// The C runtime's functions (cargo test: the C locale's).
pub(crate) struct CrtChat;

#[cfg(not(test))]
impl ChatCtype for CrtChat {
    fn towupper(&self, c: u32) -> u32 {
        // SAFETY: the CRT's wide case map takes any wint_t (the original passed the wchar_t)
        u32::from(unsafe { crt::towupper(c as crt::WInt) })
    }
    fn iswspace(&self, c: u32) -> bool {
        unsafe { crt::iswspace(c as crt::WInt) != 0 }
    }
    fn toupper(&self, c: u32) -> u32 {
        unsafe { crt::toupper(c as i32) as u32 }
    }
    fn tolower(&self, c: u32) -> u32 {
        unsafe { crt::tolower(c as i32) as u32 }
    }
    fn isspace(&self, c: i32) -> bool {
        unsafe { crt::isspace(c) != 0 }
    }
    fn isalpha(&self, c: i32) -> bool {
        unsafe { crt::isalpha(c) != 0 }
    }
    fn atoi(&self, text: &[u8]) -> i32 {
        let mut z = text.to_vec();
        z.push(0);
        unsafe { crt::atoi(z.as_ptr() as *const c_char) }
    }
}

#[cfg(test)]
impl ChatCtype for CrtChat {
    fn towupper(&self, c: u32) -> u32 {
        CChat.towupper(c)
    }
    fn iswspace(&self, c: u32) -> bool {
        CChat.iswspace(c)
    }
    fn toupper(&self, c: u32) -> u32 {
        CChat.toupper(c)
    }
    fn tolower(&self, c: u32) -> u32 {
        CChat.tolower(c)
    }
    fn isspace(&self, c: i32) -> bool {
        CChat.isspace(c)
    }
    fn isalpha(&self, c: i32) -> bool {
        CChat.isalpha(c)
    }
    fn atoi(&self, text: &[u8]) -> i32 {
        CChat.atoi(text)
    }
}

/// `GetCommandName`.
#[no_mangle]
pub extern "C" fn ee_won_chat_command_name(command: i32) -> *const c_char {
    command_name(command).as_ptr()
}

/// `ParseCommand`'s first step: the command word (upper-cased, from position 1) to the sink, and the position
/// after it.
///
/// # Safety
/// `text` as `EeWonChatText`; `sink` receives the word during the call.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chat_command_word(
    text: EeWonChatText,
    wchar_size: u32,
    sink: EeWonWideUnitSink,
    context: *mut c_void,
) -> i32 {
    let t = unsafe { units(text, wchar_size) };
    let mut pos = 1;
    let word = get_token(&t, &mut pos, true, false, &CrtChat);
    unsafe { sink(context, word.as_ptr(), word.len()) };
    pos as i32
}

/// `ParseCommand`'s second step (`parse_command`): `command` -1 for an unknown word, `clients` the room's names.
///
/// # Safety
/// The texts as `EeWonChatText`, `clients` `client_count` of them; `out` is valid; `sink` receives the texts set.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chat_parse(
    text: EeWonChatText,
    pos: i32,
    command: i32,
    reply: EeWonChatText,
    clients: *const EeWonChatText,
    client_count: usize,
    threshold: i32,
    wchar_size: u32,
    out: *mut EeWonChatOutcome,
    sink: EeWonChatSink,
    context: *mut c_void,
) {
    let t = unsafe { units(text, wchar_size) };
    let r = unsafe { units(reply, wchar_size) };
    let names = unsafe { names_of(clients, client_count, wchar_size) };
    let o = parse_command(&t, pos.max(0) as usize, (command >= 0).then_some(command), &r, &names, threshold, &CrtChat);
    if let Some(n) = &o.target_name {
        unsafe { sink(context, EE_WON_CHAT_TARGET_NAME, n.as_ptr(), n.len()) };
    }
    if let Some(x) = &o.text {
        unsafe { sink(context, EE_WON_CHAT_TEXT, x.as_ptr(), x.len()) };
    }
    // SAFETY: the caller's outcome
    let out = unsafe { &mut *out };
    *out = EeWonChatOutcome {
        ok: o.ok,
        error: o.error,
        command: o.command,
        target_set: o.target.is_some(),
        target: o.target.flatten().map_or(-1, |i| i as i32),
        duration_set: o.duration.is_some(),
        duration: o.duration.unwrap_or(0),
        invitation_set: o.invitation.is_some(),
        invitation: o.invitation.unwrap_or(false),
    };
}

/// `FindClient` (`find_client`): the match into `*matched`, the client's index or -1.
///
/// # Safety
/// As `ee_won_chat_parse`; `matched` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chat_find_client(
    name: EeWonChatText,
    clients: *const EeWonChatText,
    client_count: usize,
    wchar_size: u32,
    matched: *mut i32,
) -> i32 {
    let n = unsafe { units(name, wchar_size) };
    let names = unsafe { names_of(clients, client_count, wchar_size) };
    let (m, found) = find_client(&n, &names, &CrtChat);
    // SAFETY: the caller's int
    unsafe { *matched = m };
    found.map_or(-1, |i| i as i32)
}

/// `HandleInput`'s decision: 0 no client list, 1 a command, 2 an emote, 3 an emote with nothing in it, 4 a
/// broadcast, 5 an empty broadcast. For an emote the sink gets its text.
///
/// # Safety
/// `text` as `EeWonChatText`; `sink` as `ee_won_chat_parse`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chat_input(
    text: EeWonChatText,
    wchar_size: u32,
    prefix: i32,
    allow_colon: bool,
    has_clients: bool,
    sink: EeWonChatSink,
    context: *mut c_void,
) -> i32 {
    let t = unsafe { units(text, wchar_size) };
    match handle_input(&t, prefix, allow_colon, has_clients, &CrtChat) {
        Input::NoClients => 0,
        Input::Command => 1,
        Input::Emote { text, ok } => {
            unsafe { sink(context, EE_WON_CHAT_TEXT, text.as_ptr(), text.len()) };
            if ok {
                2
            } else {
                3
            }
        }
        Input::Broadcast { ok } => {
            if ok {
                4
            } else {
                5
            }
        }
    }
}

/// `GetHelp`'s commands (`help_commands`) into `out` (room for 32); their count.
///
/// # Safety
/// `out` has room for 32 ids.
#[no_mangle]
pub unsafe extern "C" fn ee_won_chat_help(
    room_flags: u32,
    moderator: bool,
    room_captain: bool,
    game_captain: bool,
    admin: bool,
    out: *mut i32,
) -> usize {
    let ids = help_commands(room_flags, moderator, room_captain, game_captain, admin);
    let n = ids.len().min(32);
    // SAFETY: room for 32
    unsafe { std::ptr::copy_nonoverlapping(ids.as_ptr(), out, n) };
    n
}
