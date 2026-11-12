//! The load and save screens against the real C++: a scratch driver (`build/sc6/save_load_driver.cpp`) ran the
//! unmodified `EEUIESaveLoadEvents.cpp`, `EEUIESinglePlayerLoadEvents.cpp` and `EEUIEScenarioLoadEvents.cpp` (and the
//! three control-event factories, the form events and `EEUIESPLECancel` as `EEUIManager.cpp` had them) over fake
//! forms, file lists, manager, user interface thread, server, save objects, files, headers and strings, the real
//! `UWideString`: 2,000 cases of the screens' 18 events and a sweep of the factories with the free camera, the current
//! form, the session, the pause, the turns, the server and its cast, the screen, the editor, the saved main form, the
//! question's answer, the files' outcomes (found, opened, valid, a header), the edit box, the last names and the two
//! lists' lines and selections; the saved flag, the save objects, the time, the intro's file and the lobby carried from
//! case to case (`golden_save_load.txt`). The replay runs [`dispatch`] over a host that prints what the driver's fakes
//! print for each host call. The driver relinked over `EEScreens.cpp` and this crate prints the same file, and the same
//! on seeds 2-4 at ten times the size.

use std::sync::Mutex;

use super::*;
use crate::custom_campaign::EE_SCREENS_TEXT_FORM_LIST_DATA;
use crate::menus::{
    EE_SCREENS_OP_OPEN_FORM, EE_SCREENS_OP_POP, EE_SCREENS_OP_PUSH, EE_SCREENS_TEXT_EDIT, EE_SCREENS_TEXT_MESSAGE,
};
use crate::settings_form::{EE_SCREENS_GAME_IS_SERVER, EE_SCREENS_OP_CLOSE_FORM, EE_SCREENS_TEXT_LIST_DATA};
use ee_plat_core::utf8;

const GOLDEN: &[u8] = include_bytes!("../golden_save_load.txt");

/// The replay's saved flag (the game's is the FFI's static).
static STATE: Mutex<SaveLoad> = Mutex::new(SaveLoad { saved: false });

#[derive(Default, Clone)]
struct List {
    selection: u32,
    has: bool,
    lines: Vec<Vec<u8>>,
}

#[derive(Default)]
struct Driver {
    out: Vec<Vec<u8>>,
    // the case's inputs
    free: bool,
    current: u32,
    multi: bool,
    paused: bool,
    cts: u32,
    server: bool,
    cast: bool,
    active: bool,
    editor: bool,
    main: bool,
    answer: u32,
    salt: u32,
    edit: Vec<u8>,
    lists: [List; 2], // controls 3 and 4
    extensions: [Vec<Vec<u8>>; 2],
    // carried from case to case
    saved_name: Vec<u8>,
    scenario_name: Vec<u8>,
    time: u32,
    objects: u32,
    pending: (u32, i32), // the pending save object's number and references
    live: i32,
    intro: Vec<u8>,
    movie: usize,
    load_type: i64,
    lobby_type: u32,
    lobby_map: Vec<u8>,
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

fn hash(salt: u32, text: &[u8]) -> u32 {
    text.iter().fold(salt, |h, &b| h.wrapping_mul(31).wrapping_add(b as u32))
}

fn event_name(event: u32) -> &'static str {
    match event {
        EE_SCREENS_SL_ACTIVATE => "class EEUIESLEActivateForm",
        EE_SCREENS_SL_DELETE | EE_SCREENS_SPL_DELETE | EE_SCREENS_SCL_DELETE => "class EEUIESLEDeleteFile",
        EE_SCREENS_SL_LOAD_GAME => "class EEUIESLELoadFile '.ees' 'Saved Games\\'",
        EE_SCREENS_SL_LOAD_SCENARIO => "class EEUIESLELoadFile '.scn' 'Scenarios\\'",
        EE_SCREENS_SL_SAVE_GAME => "class EEUIESLESaveFile '.ees' ''",
        EE_SCREENS_SL_SAVE_SCENARIO => "class EEUIESLESaveFile '.scn' 'data\\Scenarios\\'",
        EE_SCREENS_SL_SAVE_SCREEN_GAME => "class EEUIESLSaveScreen '.ees' 'data\\Saved Games\\'",
        EE_SCREENS_SL_SAVE_SCREEN_SCENARIO => "class EEUIESLSaveScreen '.scn' 'data\\Scenarios\\'",
        EE_SCREENS_SL_LOAD_SCREEN_GAME => "class EEUIESLLoadScreen '.ees' 'data\\Saved Games\\'",
        EE_SCREENS_SL_LOAD_SCREEN_SCENARIO => "class EEUIESLLoadScreen '.scn' 'data\\Scenarios\\'",
        EE_SCREENS_SL_DISPLAY_NEW_FILE => "class EEUIESLDisplayNewFile",
        EE_SCREENS_SL_CANCEL_SAVE => "class EEUIESLECancelSave",
        EE_SCREENS_SPL_OPEN => "class EEUIESPLEOpen",
        EE_SCREENS_SPL_LOAD_SCREEN => "class EEUIESPLLoadScreen '.ees' 'data\\Saved Games\\'",
        EE_SCREENS_SPL_LOAD => "class EEUIESPLELoadFile '.ees' 'Saved Games\\'",
        EE_SCREENS_SPL_CANCEL => "class EEUIESPLECancel",
        EE_SCREENS_SCL_OPEN => "class EEUIESCLEOpen",
        EE_SCREENS_SCL_LOAD_SCREEN => "class EEUIESCLLoadScreen '.scn' 'data\\Scenarios\\'",
        EE_SCREENS_SCL_LOAD => "class EEUIESCLELoadFile '.scn' 'Scenarios\\'",
        EE_SCREENS_SCL_CANCEL => "class EEUIESCLECancel",
        _ => panic!("event {event:#x}"),
    }
}

/// A put's texts joined by 0 bytes.
fn parts(text: &[u8]) -> Vec<Vec<u8>> {
    text.split(|&b| b == 0).map(|p| p.to_vec()).collect()
}

impl Driver {
    fn line(&mut self, text: &str) {
        self.out.push(format!("  {text}").into_bytes());
    }
    fn bytes(&mut self, parts: &[&[u8]]) {
        self.out.push(cat(&[b"  ", &cat(parts)]));
    }
    fn list(&mut self, control: u32) -> &mut List {
        &mut self.lists[(control - 3) as usize]
    }
    fn time(&mut self) -> u32 {
        self.time += 7;
        self.time
    }
    /// EEOpenSaveGameFile: its result, and whether it made a file
    fn open_save(&mut self, file: &[u8], directory: &[u8]) -> (bool, bool) {
        let h = hash(self.salt, file);
        let ok = !h.is_multiple_of(5);
        self.bytes(&[b"opensave '", file, b"' '", directory, format!("' -> {}", if ok { 0 } else { 1 }).as_bytes()]);
        let made = ok || h % 7 == 1;
        if made {
            self.live += 1;
        }
        (ok, made)
    }
    fn release_pending(&mut self) {
        self.pending.1 -= 1;
        if self.pending.1 == 0 {
            self.line(&format!("release #{}", self.pending.0));
        }
    }
    fn send(&mut self) {
        self.line(&format!("sendall #{} 15000 12", self.pending.0));
        assert!(dispatch(self, &STATE, EE_SCREENS_SL_SAVED));
    }
    fn post(&mut self) {
        self.line(&format!("postaction #{}", self.pending.0));
        self.pending.1 += 1;
        self.release_pending();
        assert!(dispatch(self, &STATE, EE_SCREENS_SL_SAVED));
    }
}

impl SaveLoadHost for Driver {
    fn call(&mut self, op: u32, a: u32, b: u32, _c: u32) -> u32 {
        match op {
            EE_SCREENS_OP_FORM_LIST_SELECTED => {
                let list = self.list(b).clone();
                return if !list.lines.is_empty() && list.has { list.selection } else { u32::MAX };
            }
            EE_SCREENS_GAME_IN_EDITOR => return self.editor as u32,
            EE_SCREENS_GAME_SCREEN_ACTIVE => return self.active as u32,
            EE_SCREENS_GAME_FREE_CAMERA => return self.free as u32,
            EE_SCREENS_GAME_MULTIPLAYER_SESSION => return self.multi as u32,
            EE_SCREENS_GAME_IS_SERVER => return self.server as u32,
            EE_SCREENS_GAME_CTS_PROCESSED => return self.cts,
            EE_SCREENS_GAME_SAVED_MAIN_FORM => return self.main as u32,
            EE_SCREENS_OP_CURRENT_FORM_IS => return (self.current == a) as u32,
            EE_SCREENS_OP_POP => self.line("pop"),
            EE_SCREENS_OP_PUSH => self.line(&format!("push {a}")),
            EE_SCREENS_OP_SWAP_FORM => self.line(&format!("swap {a}")),
            EE_SCREENS_OP_OPEN_FORM => self.line(&format!("openform {a}")),
            EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM => self.line("openform 11"),
            EE_SCREENS_OP_CLOSE_FORM => self.line("closeform 999"),
            EE_SCREENS_GAME_PAUSE_SERVER => {
                if self.cast && !self.paused {
                    self.line("pausegame");
                    self.line("postpause 1");
                }
            }
            EE_SCREENS_OP_POST_EVENT => {
                self.line(&format!("postevent {} 999", event_name(a)));
                assert!(dispatch(self, &STATE, a));
            }
            EE_SCREENS_GAME_SAVE_SEND => self.send(),
            EE_SCREENS_GAME_SAVE_POST => self.post(),
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        match which {
            EE_SCREENS_TEXT_EDIT => {
                assert_eq!(arg, 5);
                self.edit.clone()
            }
            EE_SCREENS_TEXT_LIST_DATA => {
                let list = self.list(arg >> 16).clone();
                list.lines.get((arg & 0xffff) as usize).cloned().unwrap_or_default()
            }
            EE_SCREENS_TEXT_FORM_LIST_DATA => {
                assert_eq!(arg >> 16, 3 << 8 | 4);
                self.lists[1].lines.get((arg & 0xffff) as usize).cloned().unwrap_or_default()
            }
            EE_SCREENS_TEXT_SAVED_EXTENSION => b".ees".to_vec(),
            EE_SCREENS_TEXT_SCENARIO_EXTENSION => b".scn".to_vec(),
            EE_SCREENS_TEXT_LOAD_SAVED_NAME => self.saved_name.clone(),
            EE_SCREENS_TEXT_LOAD_SCENARIO_NAME => self.scenario_name.clone(),
            _ => panic!("text {which:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        let p = parts(text);
        match which {
            EE_SCREENS_TEXT_LOAD_SAVED_NAME => self.saved_name = text.to_vec(),
            EE_SCREENS_TEXT_LOAD_SCENARIO_NAME => self.scenario_name = text.to_vec(),
            EE_SCREENS_TEXT_FILE_LIST => {
                let control = arg & 0xffff;
                assert_eq!(control, if arg >> 16 == 20 { 3 } else { 4 });
                self.line("volume 'C:'");
                self.bytes(&[b"directory '\\Game\\", &p[1], b"'"]);
                self.line("adddirectory 1");
                self.line(&format!("searchpaths {control} 1"));
                self.line(&format!("createentries {control} 1"));
                self.extensions[(control - 3) as usize].push(p[0].clone());
                if let Some(i) = self.list(control).lines.iter().position(|l| *l == p[2]) {
                    self.line(&format!("listselect {control} {i} 1 0"));
                }
            }
            EE_SCREENS_TEXT_QUESTION_DELETE => {
                self.bytes(&[b"question '", text, format!("' 1 1 0 {}", self.answer).as_bytes()]);
                if self.answer == 1 {
                    self.line(&format!("deletefile {arg}"));
                }
            }
            EE_SCREENS_TEXT_SAVE_QUESTION => {
                self.bytes(&[b"question '", text, format!("' 1 1 1 {}", self.answer).as_bytes()]);
                match (self.answer, arg) {
                    (1, 0) => self.send(),
                    (1, _) => self.post(),
                    (2, _) => self.release_pending(),
                    _ => {}
                }
            }
            EE_SCREENS_TEXT_MESSAGE => self.bytes(&[b"display '", text, format!("' 1 999 0 {arg}").as_bytes()]),
            EE_SCREENS_TEXT_LOAD_POST => {
                let (ok, _) = self.open_save(&p[0], &p[1]);
                if !ok {
                    return 0;
                }
                self.bytes(&[b"postload '", &p[0], b"'"]);
                self.live -= 1;
                return 1;
            }
            EE_SCREENS_TEXT_SAVE_MESSAGE => {
                self.objects += 1;
                self.pending = (self.objects, 1);
                let time = self.time();
                self.bytes(&[
                    b"savemessage #",
                    format!("{} '", self.objects).as_bytes(),
                    text,
                    format!("' {time} 0 0 {} 0", self.cts).as_bytes(),
                ]);
            }
            EE_SCREENS_TEXT_SAVE_ACTION => {
                self.objects += 1;
                self.pending = (self.objects, 1);
                self.bytes(&[b"saveaction #", format!("{} '", self.objects).as_bytes(), &p[0], b"' 1 '", &p[1], b"'"]);
            }
            EE_SCREENS_TEXT_FILE_EXISTS => {
                self.bytes(&[b"file '", &p[0], b"'"]);
                self.bytes(&[b"directory '\\Game\\", &p[1], b"'"]);
                let ok = (hash(self.salt, &p[0]) >> 3).is_multiple_of(3);
                self.bytes(&[b"open '", &p[0], format!("' 1 -> {}", if ok { 0 } else { 1 }).as_bytes()]);
                if ok {
                    self.line("close");
                }
                return ok as u32;
            }
            EE_SCREENS_TEXT_LOAD_SAVED_GAME => {
                let (ok, made) = self.open_save(&p[0], &p[1]);
                let h = hash(self.salt, &p[0]);
                if ok {
                    let opened = !(h >> 5).is_multiple_of(4);
                    self.bytes(&[b"fileopen '", &p[0], format!("' 1 -> {}", if opened { 0 } else { 1 }).as_bytes()]);
                    if opened {
                        let valid = !(h >> 7).is_multiple_of(3);
                        self.bytes(&[b"validate '", &p[0], format!("' -> {}", valid as u8).as_bytes()]);
                        if !valid {
                            self.bytes(&[b"display 'invalid ", &p[0], b"' 1 999 0 1"]);
                            self.live -= 1;
                            return 0;
                        }
                        self.line("writelock");
                        self.line("setdefaults");
                        self.lobby_type = 2;
                        self.lobby_map = p[0].clone();
                        self.line("writeunlock");
                        self.saved_name = p[0][..p[0].len() - 4].to_vec();
                        self.live -= 1;
                        self.line("server EEName 0 0 0 1 1");
                        self.line("startthreads");
                        self.load_type = 2;
                        let time = self.time();
                        self.line(&format!("localinput 255 startloading 1 {time}"));
                        return 1;
                    }
                }
                if made {
                    self.live -= 1;
                }
                return 0;
            }
            EE_SCREENS_TEXT_SCENARIO_INTRO => {
                let (ok, _) = self.open_save(&p[0], &p[1]);
                if !ok {
                    return 0;
                }
                let loaded = !(hash(self.salt, &p[0]) >> 9).is_multiple_of(3);
                self.bytes(&[b"header '", &p[0], format!("' 12 34 -> {}", if loaded { 0 } else { 1 }).as_bytes()]);
                if !loaded {
                    self.live -= 1;
                    return 1;
                }
                self.movie = utf8::char_len(&cat(&[b"movie of ", &p[0]]));
                if !self.intro.is_empty() {
                    self.live -= 1;
                }
                self.intro = p[0].clone();
                self.scenario_name = p[0][..p[0].len() - 4].to_vec();
                return 2;
            }
            EE_SCREENS_TEXT_FORM_EDIT => {
                assert_eq!(arg, 3 << 16 | 5);
                self.bytes(&[b"settext 5 '", text, b"'"]);
            }
            _ => panic!("put {which:#x}"),
        }
        0
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.line(&format!("string {id}"));
        format!("S{id}").into_bytes()
    }
}

/// The quoted texts of a part of a header line, in order.
fn quoted(line: &[u8]) -> Vec<Vec<u8>> {
    line.split(|&b| b == b'\'').skip(1).step_by(2).map(|t| t.to_vec()).collect()
}

fn find(line: &[u8], key: &[u8]) -> usize {
    line.windows(key.len()).position(|w| w == key).unwrap()
}

/// A list's part of the header: `SELECTION HAS 'line'...`.
fn parse_list(part: &[u8]) -> List {
    let text = String::from_utf8_lossy(part).into_owned();
    let t: Vec<&str> = text.trim().split(' ').collect();
    List { selection: t[0].parse().unwrap(), has: t[1] == "1", lines: quoted(part) }
}

#[test]
fn replays_the_cpp_golden() {
    // the driver's text-mode stdout ends its lines CR LF
    let lines: Vec<&[u8]> =
        GOLDEN.split(|&b| b == b'\n').map(|l| l.strip_suffix(b"\r").unwrap_or(l)).filter(|l| !l.is_empty()).collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with(b"case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut d = Driver {
        saved_name: b"Game 1".to_vec(),
        scenario_name: b"My Scenario".to_vec(),
        time: 1000,
        ..Default::default()
    };
    *STATE.lock().unwrap() = SaveLoad::default();
    let mut seen = [0usize; 19];
    for (n, &a) in starts.iter().enumerate() {
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let head = lines[a];
        let edit_at = find(head, b" edit '");
        let fields = String::from_utf8_lossy(&head[..edit_at]).into_owned();
        let t: Vec<&str> = fields.split(' ').collect();
        let field = |key: &str| t[t.iter().position(|&s| s == key).unwrap() + 1];
        let flag = |key: &str| field(key) == "1";
        let kind: usize = field("kind").parse().unwrap();
        let scenario = flag("scenario");
        d.free = flag("free");
        d.current = field("current").parse().unwrap();
        d.multi = flag("multi");
        d.paused = flag("paused");
        d.cts = field("cts").parse().unwrap();
        d.server = flag("server");
        d.cast = flag("cast");
        d.active = flag("active");
        d.editor = flag("editor");
        d.main = flag("main");
        d.answer = field("answer").parse().unwrap();
        d.salt = field("salt").parse().unwrap();
        let l3 = find(head, b" list3 ");
        let l4 = find(head, b" list4 ");
        let names = quoted(&head[edit_at..l3]);
        d.edit = names[0].clone();
        d.saved_name = names[1].clone();
        d.scenario_name = names[2].clone();
        d.lists = [parse_list(&head[l3 + 7..l4]), parse_list(&head[l4 + 7..])];
        d.out = vec![head.to_vec()];
        let code = match kind {
            0 => EE_SCREENS_SL_ACTIVATE,
            1 => save_load_event_for(if scenario { 11 } else { 12 }, d.active, d.editor),
            2 => save_load_event_for(if scenario { 13 } else { 17 }, d.active, d.editor),
            3 => save_load_event_for(14, d.active, d.editor),
            4 => {
                if scenario {
                    EE_SCREENS_SL_SAVE_SCREEN_SCENARIO
                } else {
                    EE_SCREENS_SL_SAVE_SCREEN_GAME
                }
            }
            5 => {
                if scenario {
                    EE_SCREENS_SL_LOAD_SCREEN_SCENARIO
                } else {
                    EE_SCREENS_SL_LOAD_SCREEN_GAME
                }
            }
            6 => save_load_event_for(20, d.active, d.editor),
            7 => save_load_event_for(16, d.active, d.editor),
            8 => EE_SCREENS_SPL_OPEN,
            9 => EE_SCREENS_SPL_LOAD_SCREEN,
            10 => sp_load_event_for(if scenario { 41 } else { 42 }),
            11 => sp_load_event_for(43),
            12 => sp_load_event_for(44),
            13 => EE_SCREENS_SCL_OPEN,
            14 => EE_SCREENS_SCL_LOAD_SCREEN,
            15 => scenario_load_event_for(if scenario { 101 } else { 102 }),
            16 => scenario_load_event_for(103),
            17 => scenario_load_event_for(104),
            _ => 0,
        };
        if kind == 18 {
            for id in 0..112u32 {
                let made = [
                    if id == 15 {
                        Some(format!("class EEUIECancel {}", if d.active { 11 } else { 40 }))
                    } else {
                        Some(save_load_event_for(id, d.active, d.editor))
                            .filter(|&c| c != 0)
                            .map(|c| event_name(c).to_string())
                    },
                    Some(scenario_load_event_for(id)).filter(|&c| c != 0).map(|c| event_name(c).to_string()),
                    Some(sp_load_event_for(id)).filter(|&c| c != 0).map(|c| event_name(c).to_string()),
                ];
                for (k, name) in made.iter().enumerate() {
                    if let Some(name) = name {
                        d.line(&format!("factory {k} {id} {name}"));
                    }
                }
                match id {
                    11 => d.line(&format!("formfactory 11 {}", event_name(EE_SCREENS_SPL_OPEN))),
                    12 => d.line(&format!("formfactory 12 {}", event_name(EE_SCREENS_SCL_OPEN))),
                    _ => {}
                }
            }
        } else {
            d.line(&format!("event {}", event_name(code)));
            assert!(dispatch(&mut d, &STATE, code));
        }
        for i in 0..2 {
            for e in std::mem::take(&mut d.extensions[i]) {
                d.bytes(&[format!("extension {} '", i + 3).as_bytes(), &e, b"'"]);
            }
        }
        let result = cat(&[
            b"  result saved '",
            &d.saved_name,
            b"' scen '",
            &d.scenario_name,
            format!("' files {} loadtype {} intro '", d.live, d.load_type).as_bytes(),
            &d.intro,
            format!("' 0 {} lobby {} '", d.movie, d.lobby_type).as_bytes(),
            &d.lobby_map,
            b"' 0",
        ]);
        d.out.push(result);
        let want: Vec<Vec<u8>> = lines[a..b].iter().map(|l| l.to_vec()).collect();
        for (i, (got, want)) in d.out.iter().zip(&want).enumerate() {
            assert_eq!(String::from_utf8_lossy(got), String::from_utf8_lossy(want), "case {n} line {i}");
        }
        assert_eq!(d.out.len(), want.len(), "case {n}");
        seen[kind] += 1;
    }
    assert!(seen.iter().all(|&s| s > 50), "{seen:?}");
}
