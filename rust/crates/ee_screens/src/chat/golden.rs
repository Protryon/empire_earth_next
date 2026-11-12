//! The chat forms against the real C++: a scratch driver (`build/sc8/chat_driver.cpp`) ran the unmodified
//! `EEUIEChatEvents.cpp` and `EEUIEChatLogEvents.cpp` (its form manager real in both builds) over fake forms, radio
//! buttons, the edit box, the log's label and scroll bar, hot keys, triggers, the keyboard event, the manager, the user
//! interface thread, the communications, the world, the cheat parser and the players: one session of 2,000 cases,
//! each perturbing the state (printed, with the log's entries), then opening or closing the chat form, pressing the
//! mouse inside or outside it, opening the chat log or pressing on it, adding log entries, firing a posted event or a
//! stored hot key, trigger or keyboard event (`golden_chat.txt`). The replay runs [`dispatch`] over a host that keeps
//! the same world and prints what the driver's fakes print for each host call. The driver relinked over
//! `EEScreens.cpp` and this crate prints the same file, and the same on seeds 2-4 at 25,000 cases.

use std::collections::VecDeque;

use super::*;
use crate::menus::EE_SCREENS_TEXT_EDIT;

const GOLDEN: &[u8] = include_bytes!("../golden_chat.txt");

fn event_name(event: u32) -> &'static str {
    match event {
        EE_SCREENS_CH_SEND => "class EEUIECESendMessage",
        EE_SCREENS_CH_KEYBOARD_FOCUS => "class EEUIECEChatKeyboardFocus",
        EE_SCREENS_CH_SEND_TYPE_ALL => "class EEUIECEChangeSendMessageType(0)",
        EE_SCREENS_CH_SEND_TYPE_ALLIES => "class EEUIECEChangeSendMessageType(1)",
        EE_SCREENS_CH_SEND_TYPE_ENEMIES => "class EEUIECEChangeSendMessageType(2)",
        EE_SCREENS_CH_SEND_TYPE_CUSTOM => "class EEUIECEChangeSendMessageType(3)",
        EE_SCREENS_CH_ESCAPE => "class EEUIECEEscape",
        EE_SCREENS_CH_NEXT_TYPE => "class EEUIECEChangeUserChatType(0)",
        EE_SCREENS_CH_PREVIOUS_TYPE => "class EEUIECEChangeUserChatType(1)",
        EE_SCREENS_CH_CLOSE => "class EEUIECECloseChat",
        EE_SCREENS_CH_OPEN => "class EEUIECEOpenChat",
        EE_SCREENS_CH_CANCEL => "class EEUIECECancel",
        EE_SCREENS_CL_OPEN => "class EEUIECLEOpenChatLog",
        EE_SCREENS_CL_CLOSE => "class EEUIECLECloseChatLog",
        EE_SCREENS_CL_MOUSE_PRESSED => "class EEUIECLEMousePressed",
        other => panic!("no name for {other:#x}"),
    }
}

/// The driver's world.
#[derive(Default)]
struct World {
    out: Vec<String>,
    radios: [bool; 6],
    player: bool,
    observer: bool,
    chat_type: u32,
    edit: Vec<u8>,
    cheats: bool,
    code: (i64, i64),
    close: i64,
    inside: bool,
    captured: bool,
    name: Vec<u8>,
    entries: Vec<(Vec<u8>, Vec<u8>)>,
    /// The stored hot keys, triggers and keyboard event: (what, form, code).
    stored: Vec<(String, u32, u32)>,
    posted: VecDeque<u32>,
    lines: u32,
    align: u32,
}

impl World {
    fn line(&mut self, text: String) {
        self.out.extend(text.split('\n').map(str::to_string));
    }
    fn keep(&mut self, what: String, form: u32, event: u32) {
        if let Some(i) = self.stored.iter().position(|s| s.0 == what && s.1 == form) {
            self.stored.remove(i);
        }
        self.stored.push((what, form, event));
    }
}

const RADIO_ORDER: [u32; 4] = [3, 2, 4, 5]; // all, ally, enemy, custom (the driver's kRadios)

impl ChatHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_CH_CHAT_TYPE => self.line(format!("  local 161 255 {a}")),
            EE_SCREENS_OP_CH_SEND_KEYS => {
                self.line(format!("  trigger 1 1009 {}", event_name(a)));
                self.keep("trigger 1 1009".into(), 18, a);
                self.line(format!("  hotkeyobject 18 28 0 0 0 {} 0", event_name(a)));
                self.keep("hotkey 28 object".into(), 18, a);
            }
            EE_SCREENS_OP_CH_HOT_KEY => {
                self.line(format!("  hotkey 18 {a} {b} 32767 32767 {} null null null 0 0", event_name(c)));
                self.keep(format!("hotkey {a} {b}"), 18, c);
            }
            crate::main_game::EE_SCREENS_GAME_MG_HAS_PLAYER => return self.player as u32,
            EE_SCREENS_GAME_CH_OBSERVER => return self.observer as u32,
            EE_SCREENS_GAME_CH_PLAYER_TYPE => return self.chat_type,
            EE_SCREENS_OP_CHECK_RADIO => {
                self.line(format!("  check {a}"));
                for r in RADIO_ORDER {
                    self.radios[r as usize] = r == a;
                }
            }
            EE_SCREENS_OP_RADIO => return self.radios[a as usize] as u32,
            EE_SCREENS_OP_SHOW => self.line(format!("  mode {a} {b}")),
            EE_SCREENS_OP_CH_TABBED_TO => self.line(format!("  tabbedto {a}")),
            EE_SCREENS_OP_TRIGGER_HERE => {
                self.line(format!("  trigger {a} {b} {}", event_name(c)));
                self.keep(format!("trigger {a} {b}"), 18, c);
            }
            EE_SCREENS_OP_CH_KEYBOARD_EVENT => {
                self.line(format!("  keyboardevent 18 {}", event_name(a)));
                self.keep("keyboard".into(), 18, a);
            }
            EE_SCREENS_OP_GO_REMOVE_HOT_KEYS => {
                self.line("  removehotkeys 18".into());
                self.stored.retain(|s| !(s.0.starts_with("hotkey") && s.1 == 18));
            }
            EE_SCREENS_OP_CH_MOUSE_INSIDE => {
                self.line(format!("  contains -> {}", self.inside as i32));
                return self.inside as u32;
            }
            EE_SCREENS_GAME_CH_CLOSE => {
                self.line(format!("  closeform 11 -> {}", self.close));
                return self.close as u32;
            }
            EE_SCREENS_GAME_CH_CHEATS => return self.cheats as u32,
            EE_SCREENS_OP_CH_KEYBOARD_CAPTURED => return self.captured as u32,
            EE_SCREENS_OP_CH_TAB_STOP => self.line(format!("  tabstop 18 {a}")),
            EE_SCREENS_GAME_PAUSE => self.line("  worldpause".into()),
            EE_SCREENS_OP_CL_BEGIN => {
                self.line("  deletearray 401".into());
                self.lines = 0;
                self.align = 0;
            }
            EE_SCREENS_GAME_CL_COUNT => return self.entries.len() as u32,
            EE_SCREENS_OP_CL_CENTER => self.align = 1,
            EE_SCREENS_OP_CL_END => self.line(format!("  startline {}", self.lines as i64 - 1)),
            EE_SCREENS_GAME_CL_UNPAUSE => self.line("  worldunpause".into()),
            EE_SCREENS_OP_POP => self.line("  pop".into()),
            EE_SCREENS_OP_POST_EVENT => {
                self.line(format!("  postevent {} 999", event_name(a)));
                self.posted.push_back(a);
            }
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        match which {
            EE_SCREENS_TEXT_EDIT => {
                assert_eq!(arg, 1);
                self.edit.clone()
            }
            EE_SCREENS_TEXT_CL_NAME => self.entries[arg as usize].0.clone(),
            EE_SCREENS_TEXT_CL_TEXT => self.entries[arg as usize].1.clone(),
            EE_SCREENS_TEXT_CL_PLAYER => self.name.clone(),
            _ => panic!("text {which:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, _arg: u32, text: &[u8]) {
        let t = String::from_utf8_lossy(text).into_owned();
        match which {
            EE_SCREENS_TEXT_CH_CHEAT => {
                self.line(format!("  parsecheat '{t}'"));
                if self.code.0 != 0 {
                    self.line(format!("  local 133 255 {} {}", self.code.0, self.code.1));
                }
            }
            EE_SCREENS_TEXT_CH_CHAT => self.line(format!("  local 125 255 '{t}'")),
            EE_SCREENS_TEXT_CL_APPEND => {
                self.line(format!("  append 401 '{t}' align {}", self.align));
                self.lines += 1;
            }
            _ => panic!("put {which:#x}"),
        }
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        format!("S{id}").into_bytes()
    }
}

/// A line's quoted texts and plain words.
fn words(line: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut rest = line.trim_start();
    while !rest.is_empty() {
        if let Some(r) = rest.strip_prefix('\'') {
            let end = r.find('\'').unwrap();
            out.push(r[..end].to_string());
            rest = r[end + 1..].trim_start();
        } else {
            let end = rest.find(' ').unwrap_or(rest.len());
            out.push(rest[..end].to_string());
            rest = rest[end..].trim_start();
        }
    }
    out
}

fn read_state(w: &mut World, line: &str) {
    let v = words(line);
    assert_eq!(v[0], "state");
    let at = |key: &str| v.iter().position(|x| x == key).unwrap_or_else(|| panic!("no {key}")) + 1;
    let r = at("radios");
    for (i, radio) in RADIO_ORDER.iter().enumerate() {
        w.radios[*radio as usize] = v[r + i] == "1";
    }
    w.player = v[at("player")] == "1";
    w.observer = v[at("observer")] == "1";
    w.chat_type = v[at("chattype")].parse().unwrap();
    w.edit = v[at("edit")].clone().into_bytes();
    w.cheats = v[at("cheats")] == "1";
    let c = at("code");
    w.code = (v[c].parse().unwrap(), v[c + 1].parse().unwrap());
    w.close = v[at("close")].parse().unwrap();
    w.inside = v[at("inside")] == "1";
    w.captured = v[at("captured")] == "1";
    w.name = v[at("name")].clone().into_bytes();
    let e = at("entries");
    let n: usize = v[e].parse().unwrap();
    w.entries =
        (0..n).map(|i| (v[e + 1 + 2 * i].clone().into_bytes(), v[e + 2 + 2 * i].clone().into_bytes())).collect();
}

#[test]
fn golden_chat() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let mut w = World::default();
    let mut kinds = std::collections::BTreeSet::new();
    for (n, case) in cases.iter().enumerate() {
        // (case 0 first makes the log's form manager: the driver's state line asks for it)
        let s = case.iter().position(|l| l.starts_with("  state")).unwrap();
        read_state(&mut w, case[s]);
        let action_line = case[s + 1];
        let action = words(action_line);
        w.out.clear();
        let event = match action[0].as_str() {
            "open" => Some(EE_SCREENS_CH_OPEN),
            "close" => Some(EE_SCREENS_CH_CLOSE),
            "cancel" => Some(EE_SCREENS_CH_CANCEL),
            "logopen" => Some(EE_SCREENS_CL_OPEN),
            "logpressed" => Some(EE_SCREENS_CL_MOUSE_PRESSED),
            "entries" => None,
            "posted" => Some(w.posted.pop_front().unwrap()),
            "fire" => {
                let found = w.stored.iter().find(|st| {
                    let fire = format!(" fire {} {}", st.0, event_name(st.2));
                    fire == action_line
                });
                Some(found.unwrap_or_else(|| panic!("case {n}: {action_line}")).2)
            }
            other => panic!("case {n}: {other}"),
        };
        if let Some(e) = event {
            kinds.insert(e);
            if action[0] != "fire" {
                assert_eq!(event_name(e), action[1..].join(" "), "case {n}");
            }
            let result = dispatch(&mut w, e).unwrap();
            w.line(format!("  result {result}"));
        }
        let r = |i: usize| w.radios[RADIO_ORDER[i] as usize] as i32;
        let after = format!("  after radios {} {} {} {} lines {}", r(0), r(1), r(2), r(3), w.lines);
        w.line(after);
        let expected = &case[s + 2..];
        for (i, e) in expected.iter().enumerate() {
            assert!(i < w.out.len(), "case {n} ({action_line}): missing line {i}: {e}");
            assert_eq!(w.out[i], *e, "case {n} ({action_line}) line {i}");
        }
        assert_eq!(w.out.len(), expected.len(), "case {n}: extra lines {:?}", &w.out[expected.len()..]);
    }
    assert_eq!(kinds.len(), 15);
}
