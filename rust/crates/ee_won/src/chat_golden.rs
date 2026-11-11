//! `golden_chat.txt`: the C++ ChatCommandParser over `tests/won/won_chat_test.cpp`'s inputs and settings, recorded
//! before the port. This replays every line through `chat` with the C++ glue modelled: the command tables (each
//! resource name list split at its commas, the first name the display string, a name looked up ignoring case), the
//! members a parse leaves alone (the driver resets the text to "~" and the invitation before each case; the
//! duration carries over), the help text built from `help_commands`.

use crate::chat::*;

const GOLDEN: &str = include_str!("golden_chat.txt");

// (id, the resource's names) in InitCommands' order
const COMMANDS: &[(i32, &str)] = &[
    (WHISPER, "w, whisper, t, tell"),
    (BLOCK, "block"),
    (EMOTE, "e, em, me, emote"),
    (IGNORE, "i, ignore"),
    (AWAY, "away, afk"),
    (INVITE, "invite"),
    (UNINVITE, "uninvite"),
    (REPLY, "r, reply"),
    (CLEAR, "clr, cls, clear"),
    (HELP, "?, h, help"),
    (SHOW_TEAM, "team"),
    (MODERATOR, "mod"),
    (SERVER_MUTE, "smute"),
    (SERVER_BAN, "sban"),
    (MUTE, "mute"),
    (BAN, "ban"),
    (UNMUTE, "unmute"),
    (SERVER_UNMUTE, "sunmute"),
    (UNBAN, "unban"),
    (SERVER_UNBAN, "sunban"),
    (ALERT, ""),
    (START_SHUTDOWN, ""),
    (ABORT_SHUTDOWN, ""),
    (WARN, "warn"),
];

fn help_string(id: i32) -> &'static str {
    match id {
        HELP => "HelpHelp",
        SHOW_TEAM => "ShowTeamHelp",
        WHISPER => "WhisperHelp",
        BLOCK => "BlockHelp",
        IGNORE => "IgnoreHelp",
        REPLY => "ReplyHelp",
        EMOTE => "EmoteHelp",
        AWAY => "AwayHelp",
        CLEAR => "ClearHelp",
        INVITE => "InviteHelp",
        UNINVITE => "UninviteHelp",
        MUTE => "MuteHelp",
        UNMUTE => "UnmuteHelp",
        BAN => "BanHelp",
        UNBAN => "UnbanHelp",
        MODERATOR => "ModeratorHelp",
        SERVER_MUTE => "ServerMuteHelp",
        SERVER_UNMUTE => "ServerUnmuteHelp",
        SERVER_BAN => "ServerBanHelp",
        SERVER_UNBAN => "ServerUnbanHelp",
        ALERT => "AlertHelp",
        WARN => "WarnHelp",
        _ => panic!("no help for {id}"),
    }
}

// (id, name, flags, game captain, chat captain), in id order (the client map's)
const CLIENTS: &[(u16, &str, u32, bool, bool)] = &[
    (1, "Alice", 0, false, false),
    (2, "alice2", 0, false, false),
    (3, "Bob", 1, true, false),
    (4, "Bobby", 0x20, false, true),
    (5, "Carl Jr", 0, false, false),
    (6, "\u{e9}mile", 0x21, true, true),
    (7, "\u{263a}", 0, false, false),
    (8, "Dave", 0, false, false),
    (9, "dave", 0, false, false),
];

const INPUTS: &[&str] = &[
    "",
    "hello",
    "   ",
    " x",
    "/",
    "/ ",
    "/nosuch",
    "/W Alice hi there",
    "/w alice",
    "/w alice ",
    "/w al hi",
    "/w ali hi",
    "/w Bob hi",
    "/w bo hi",
    "/w \"Carl Jr\" yo",
    "/w \"Carl Jr yo",
    "/w Carl",
    "/tell \u{e9}mile salut",
    "/w \u{c9}MILE x",
    "/w \u{263a} ok",
    "/w dave x",
    "/w DAVE x",
    "/w Dav x",
    "/whisper",
    "/e waves",
    "/me",
    "/em  ",
    "/r",
    "/r hey you",
    "/reply   ",
    "/i",
    "/i Bob",
    "/i \"Bob\"",
    "/ignore Bo",
    "/i B",
    "/block Bobby",
    "/block",
    "/block x",
    "/away",
    "/afk now",
    "/invite Bob",
    "/invite Bob come",
    "/uninvite Bob",
    "/cls",
    "/?",
    "/h extra",
    "/team",
    "/mod",
    "/smute Bob",
    "/smute Bob 10",
    "/smute Bob 10m",
    "/smute Bob 2h being rude",
    "/sban Bob 1d",
    "/sban Bob 3s",
    "/sban Bob x",
    "/sban Bob 0s",
    "/sban Bob INFINITE reason",
    "/sban Bob infinite",
    "/mute Bob 5M",
    "/ban Bob -1h",
    "/ban Bob 99999999d",
    "/ban Bob 10\u{e9}",
    "/unmute",
    "/unmute Bob",
    "/unmute Bob  later  ",
    "/sunmute Bob",
    "/unban Bob",
    "/sunban Bob",
    "/alert everyone",
    "/warn",
    "/warn Bob",
    "/warn Bob stop",
    "/startshutdown 5m",
    "/abortshutdown",
    ":smile",
    ": ",
    ":",
    "/W  \"Alice\"   two  spaces  ",
    "/w \"\" x",
    "/w \"Alice\"x",
    "\t/w Alice x",
    "/\tw Alice x",
    "/w\tAlice x",
    "/i \"x",
    "/i ab",
    "/i a ",
    "/r \"quoted\"",
    "/unmute \"Bob\" \"q\"",
    "/unmute B",
];

fn w(s: &str) -> Vec<u32> {
    s.chars().map(u32::from).collect()
}

fn units(t: &[u32]) -> String {
    if t.is_empty() {
        return "-".into();
    }
    t.iter()
        .map(|&c| {
            if c > 0x20 && c < 0x7f && c != u32::from(b'%') && c != u32::from(b'|') {
                char::from_u32(c).unwrap().to_string()
            } else {
                format!("%{c:x};")
            }
        })
        .collect()
}

/// The command tables: every (name, id) in registration order, and each id's display string.
fn tables() -> (Vec<(Vec<u32>, i32)>, Vec<(i32, Vec<u32>)>) {
    let mut names = Vec::new();
    let mut display: Vec<(i32, Vec<u32>)> = Vec::new();
    for &(id, list) in COMMANDS {
        let parts: Vec<String> = list.split(',').map(|p| p.trim().to_string()).filter(|p| !p.is_empty()).collect();
        let parts = if parts.is_empty() { vec![command_name(id).to_str().unwrap().to_string()] } else { parts };
        for p in parts {
            let p = w(&p);
            if !names.iter().any(|(n, _): &(Vec<u32>, i32)| *n == p) {
                names.push((p.clone(), id));
            }
            if !display.iter().any(|(i, _)| *i == id) {
                display.push((id, p));
            }
        }
    }
    (names, display)
}

fn lookup(names: &[(Vec<u32>, i32)], word: &[u32]) -> Option<i32> {
    let up = |t: &[u32]| t.iter().map(|&c| CChat.towupper(c)).collect::<Vec<_>>();
    names.iter().find(|(n, _)| up(n) == up(word)).map(|&(_, id)| id)
}

fn run(pass: &str, threshold: i32, colon: bool, prefix: char, with_clients: bool, out: &mut Vec<String>) {
    let (names, _) = tables();
    let clients: Vec<Vec<u32>> = CLIENTS.iter().map(|c| w(c.1)).collect();
    let reply = w("Bob");
    let mut command = -1; // the driver's start (the constructor leaves it unset)
    let mut duration = 0u32;
    for (i, input) in INPUTS.iter().enumerate() {
        let text = w(input);
        let mut error = 0;
        let mut target: Option<usize> = None;
        let mut target_name: Vec<u32> = Vec::new();
        let mut out_text = w("~");
        let mut invitation = false;
        let ok;
        match handle_input(&text, prefix as i32, colon, with_clients, &CChat) {
            Input::NoClients => {
                error = 6;
                ok = false;
            }
            Input::Emote { text: t, ok: o } => {
                command = EMOTE;
                out_text = t;
                if !o {
                    error = 5;
                }
                ok = o;
            }
            Input::Broadcast { ok: o } => {
                command = BROADCAST;
                if o {
                    out_text = text.clone();
                } else {
                    error = 5;
                }
                ok = o;
            }
            Input::Command => {
                let mut pos = 1;
                let word = get_token(&text, &mut pos, true, false, &CChat);
                let o = parse_command(&text, pos, lookup(&names, &word), &reply, &clients, threshold, &CChat);
                ok = o.ok;
                error = o.error;
                command = o.command;
                if let Some(n) = o.target_name {
                    target_name = n;
                }
                if let Some(t) = o.target {
                    target = t;
                }
                if let Some(t) = o.text {
                    out_text = t;
                }
                if let Some(d) = o.duration {
                    duration = d;
                }
                if let Some(v) = o.invitation {
                    invitation = v;
                }
            }
        }
        let target = target.map_or(-1, |t| i32::from(CLIENTS[t].0));
        out.push(format!(
            "{pass} i{i:02} r={} err={error} cmd={command} name={} target={target} text={} dur={duration} inv={}",
            u8::from(ok),
            units(&target_name),
            units(&out_text),
            u8::from(invitation)
        ));
    }
}

fn run_tables(out: &mut Vec<String>) {
    let (_, display) = tables();
    for id in 0..=26 {
        let s = display.iter().find(|(i, _)| *i == id).map(|(_, s)| s.clone()).unwrap_or_default();
        out.push(format!("command {id} name={} string={}", command_name(id).to_str().unwrap(), units(&s)));
    }
    let help = |flags: u32, client: Option<&(u16, &str, u32, bool, bool)>| {
        let (moderator, room_captain, game_captain, admin) = match client {
            Some(&(_, _, f, game, chat)) => {
                (f & 1 != 0, if (flags >> 1) & 1 != 0 { game } else { chat }, game, f & 0x20 != 0)
            }
            None => (false, false, false, false),
        };
        let mut s = String::from("Header<n>");
        for id in help_commands(flags, moderator, room_captain, game_captain, admin) {
            s += help_string(id);
            s += "<n>";
        }
        s
    };
    for flags in [0u32, 2, 8, 10, 0x0f] {
        for id in 0..=10u16 {
            let client = CLIENTS.iter().find(|c| c.0 == id);
            out.push(format!("help flags={flags} client={id} {}", help(flags, client)));
        }
    }
    out.push(format!("help nolist {}", help(0, None)));
}

#[test]
fn every_input_and_table_matches_the_cpp() {
    let mut got = Vec::new();
    run("t2", 2, false, '/', true, &mut got);
    run("t3", 3, true, '/', true, &mut got);
    run("t1", 1, false, '!', true, &mut got);
    run("none", 2, false, '/', false, &mut got);
    run_tables(&mut got);
    let want: Vec<&str> = GOLDEN.lines().collect();
    assert_eq!(got.len(), want.len());
    for (g, w) in got.iter().zip(&want) {
        assert_eq!(g, w);
    }
}

#[test]
fn durations_and_matches() {
    let c = CChat;
    let names = vec![w("Ann"), w("Anna"), w("bob")];
    assert_eq!(find_client(&w("ann"), &names, &c), (3, Some(0)));
    assert_eq!(find_client(&w("an"), &names, &c), (1, None));
    assert_eq!(find_client(&w("BO"), &names, &c), (2, Some(2)));
    assert_eq!(find_client(&w("x"), &names, &c), (0, None));
    assert_eq!(rest_of_line(&w("  a"), 0, &c), Vec::<u32>::new());
    assert_eq!(rest_of_line(&w("  ab  "), 0, &c), w("ab"));
    assert_eq!(rest_of_line(&w(" \"q r\" s"), 0, &c), w("q r"));
    let o = parse_command(&w("/smute bob 2h x"), 6, Some(SERVER_MUTE), &[], &names, 2, &c);
    // a rest of one character reads as empty (the original's)
    assert_eq!((o.ok, o.duration, o.text), (true, Some(7200), Some(Vec::new())));
    assert_eq!(c.atoi(b" -12x"), -12);
    assert_eq!(c.atoi(b"99999999999"), i32::MAX);
}
