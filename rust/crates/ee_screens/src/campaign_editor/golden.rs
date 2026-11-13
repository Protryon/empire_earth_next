//! The campaign editor against the real C++: a scratch driver (`build/sc12/ce_driver.cpp`) ran the unmodified
//! `EEUIECampaignEditorEvents.cpp` over fakes of form 24's controls (edit boxes, labels, list boxes with the toolkit's
//! selection rules, the background file combo box; each control's class drawn per case behind a type-checking
//! `dynamic_cast`), its entity groups, the editor's model (`EECampaignEditorBehavior`: every call printed, its answers
//! drawn), the strings, `EETranslateString`, the manager's messages and questions and the posted events: 2,000 cases of
//! the form's 14 events, the questions' answers (yes or no to a pending question), the messages' closes and the posted
//! saves' runs (`golden_campaign_editor.txt`, seed 5: one that reaches every answer). The replay draws each case's
//! state with the driver's generator, runs [`dispatch`] over a host that prints what the driver's fakes print for each
//! host call, and compares every line. The driver relinked over `EEScreens.cpp` and this crate prints the same file,
//! and the same on seeds 1-4 (2-4 at ten times the size).

use std::collections::BTreeMap;

use super::*;
use ee_plat_core::utf8;

const GOLDEN: &[u8] = include_bytes!("../golden_campaign_editor.txt");

const TEXTS: [&str; 8] = ["", "Rome", "Campaign One", "\u{c9}gypte", "abc", "ABC", "x y", "rome.JPG"];
const IMAGES: [&str; 6] = ["a.jpg", "B.JPG", "Rome.jpg", "\u{e9}t\u{e9}.jpg", "rome.JPG", "S1"];
const SCENARIOS: [&str; 6] = ["Scen1", "Scen2", "Atlantis", "\u{c9}cosse", "", "rome.jpg"];
const EDITS: [u32; 6] = [2, 3, 7, 11, 35, 47];
const LABELS: [u32; 5] = [25, 26, 27, 45, 48];
const LISTS: [u32; 4] = [10, 14, 15, 40];
const COMBO_LIST: u32 = 50;
const SETTERS: [&str; 7] = [
    "SetName",
    "SetAuthorName",
    "SetAuthorEmail",
    "SetBackgroundImage",
    "SetRequiredCiv",
    "SetDescription",
    "SetPassword",
];
const GETTERS: [&str; 6] = ["m.name", "m.author", "m.email", "m.background", "m.civ", "m.description"];

#[derive(Default, Clone)]
struct List {
    data: Vec<Vec<u8>>,
    has: bool,
    sel: u32,
}

#[derive(Default)]
struct Driver {
    seed: u32,
    out: Vec<Vec<u8>>,
    types: [Vec<u32>; 4],
    edits: BTreeMap<u32, Vec<Vec<u8>>>,
    lists: BTreeMap<u32, List>,
    combo: u32,
    extensions: Vec<Vec<u8>>,
    images: Vec<Vec<u8>>,
    answers: BTreeMap<&'static str, bool>,
    new_answer: u32,
    fields: [Vec<u8>; 6],
    model: [Vec<Vec<u8>>; 4],
    error: Vec<u8>,
    entered: bool,
    password: Vec<u8>,
    last_error: Vec<u8>,
    list_ok: bool,
    translated: Vec<u8>,
    questions: Vec<(u32, Option<u32>)>,
    messages: Vec<bool>,
    posted: Vec<u32>,
}

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}
fn quoted(items: &[Vec<u8>], sep: &[u8]) -> Vec<u8> {
    items.iter().flat_map(|s| cat(&[sep, b"'", s, b"'"])).collect()
}

impl Driver {
    fn below(&mut self, n: u32) -> u32 {
        self.seed = self.seed.wrapping_mul(1664525).wrapping_add(1013904223);
        (self.seed >> 8) % n
    }
    fn draw(&mut self, pool: &[&str]) -> Vec<u8> {
        pool[self.below(pool.len() as u32) as usize].as_bytes().to_vec()
    }
    fn line(&mut self, text: &[u8]) {
        self.out.push(cat(&[b"  ", text]));
    }
    fn says(&mut self, text: String) {
        self.line(text.as_bytes());
    }
    fn list(&mut self, control: u32) -> &mut List {
        self.lists.entry(control).or_default()
    }
    fn field(&mut self, which: u32) -> Vec<u8> {
        if which == EE_SCREENS_CE_FIELD_PASSWORD {
            return self.password.clone();
        }
        self.says(GETTERS[which as usize].to_string());
        self.fields[which as usize].clone()
    }
    fn create_entries(&mut self) {
        let ext = quoted(&self.extensions, b" ");
        self.line(&cat(&[b"createentries 44 1 ext", &ext]));
        let images = self.images.clone();
        self.list(COMBO_LIST).data = images;
    }
    fn none_line(&mut self) {
        let list = &mut self.lists.entry(COMBO_LIST).or_default().data;
        list.insert(0, b"S20009".to_vec());
        self.line(b"comboinsert 44 'S20009' 0");
        self.line(b"combosel 44 0");
        self.combo = 0;
    }
    fn draw_list(&mut self, control: u32, pool: &[&str]) {
        let n = self.below(5);
        let mut data = Vec::new();
        for _ in 0..n {
            data.push(self.draw(pool));
        }
        let mut has = self.below(4) != 0;
        let sel = if data.is_empty() { self.below(2) } else { self.below(data.len() as u32) };
        if data.is_empty() && control == COMBO_LIST {
            has = false;
        }
        *self.list(control) = List { data, has, sel };
    }

    /// The driver's `State()`: the case's draws and its state line.
    fn state(&mut self) {
        let mut s = b"state types".to_vec();
        for (k, controls) in [&EDITS[..], &LABELS[..], &LISTS[..], &[44][..]].iter().enumerate() {
            self.types[k].clear();
            for &c in controls.iter() {
                if self.below(12) != 0 {
                    self.types[k].push(c);
                } else {
                    s.extend(format!(" -{c}").bytes());
                }
            }
        }
        s.extend(b" edits");
        for c in EDITS {
            if self.below(3) == 0 {
                let n = 1 + self.below(2);
                let lines = (0..n).map(|_| self.draw(&TEXTS)).collect();
                self.edits.insert(c, lines);
            }
            let lines = self.edits.entry(c).or_insert_with(|| vec![Vec::new()]);
            s.extend(cat(&[b" [", &quoted(lines, b""), b"]"]));
        }
        s.extend(b" lists");
        for c in LISTS {
            if self.below(3) == 0 {
                self.draw_list(c, if c == 10 { &TEXTS } else { &SCENARIOS });
            }
            let l = self.list(c).clone();
            s.extend(cat(&[format!(" [{} {}", l.has as u8, l.sel).as_bytes(), &quoted(&l.data, b" "), b"]"]));
        }
        if self.below(3) == 0 {
            self.draw_list(COMBO_LIST, &IMAGES);
            let n = self.list(COMBO_LIST).data.len() as u32;
            self.combo = if self.below(4) == 0 || n == 0 { u32::MAX } else { self.below(n) };
        }
        let combo = self.list(COMBO_LIST).data.clone();
        s.extend(cat(&[format!(" combo {}", self.combo as i32).as_bytes(), &quoted(&combo, b" ")]));
        let n = self.below(4);
        self.images = (0..n).map(|_| self.draw(&IMAGES)).collect();
        s.extend(cat(&[b" images", &quoted(&self.images, b" ")]));
        s.extend(b" answers");
        for a in ["add", "remove", "move", "delete", "unlocked", "list", "save", "load"] {
            let v = self.below(4) != 0;
            self.answers.insert(a, v);
            s.extend(format!(" {}", v as u8).bytes());
        }
        self.new_answer = if self.below(4) != 0 { 0 } else { self.below(5) };
        s.extend(format!(" new {}", self.new_answer).bytes());
        if self.below(3) == 0 {
            for i in 0..6 {
                self.fields[i] = self.draw(&TEXTS);
            }
        }
        if self.below(3) == 0 {
            for k in 0..4 {
                let n = self.below(4);
                self.model[k] =
                    (0..n).map(|_| if k == 0 { self.draw(&TEXTS) } else { self.draw(&SCENARIOS) }).collect();
            }
        }
        self.error = self.draw(&TEXTS);
        self.entered = self.below(3) == 0;
        if self.below(3) == 0 {
            self.password = self.draw(&TEXTS);
        }
        s.extend(cat(&[b" fields", &quoted(&self.fields, b" ")]));
        for l in &self.model {
            s.extend(cat(&[b" (", &quoted(l, b""), b")"]));
        }
        s.extend(cat(&[
            b" error '",
            &self.error,
            format!("' entered {} password '", self.entered as u8).as_bytes(),
            &self.password,
            format!("' pending {} {} {}", self.questions.len(), self.messages.len(), self.posted.len()).as_bytes(),
        ]));
        self.line(&s);
    }
}

impl CampaignEditorHost for Driver {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        match op {
            EE_SCREENS_OP_CE_IS => return self.types[b as usize].contains(&a) as u32,
            EE_SCREENS_GAME_CE_OPEN => {
                assert_eq!(a, 44);
                self.line(b"volume 'C:'");
                self.line(b"directory '\\Game\\data\\Scenarios\\'");
                self.line(b"adddirectory 1");
                self.line(b"string 20009");
                self.extensions.push(b".jpg".to_vec());
                self.extensions.push(b".JPG".to_vec());
                self.line(b"searchpaths 44 1");
                self.create_entries();
                self.none_line();
                self.line(b"deletepath");
            }
            EE_SCREENS_GAME_CE_RESET => self.line(b"m.reset"),
            EE_SCREENS_GAME_CE_NEW => {
                self.says(format!("m.new -> {}", self.new_answer));
                return self.new_answer;
            }
            EE_SCREENS_GAME_CE_UNLOCKED => {
                let v = self.answers["unlocked"];
                self.says(format!("m.unlocked -> {}", v as u8));
                return v as u32;
            }
            EE_SCREENS_GAME_CE_PASSWORD_ENTERED => return self.entered as u32,
            EE_SCREENS_GAME_CE_SAVE => {
                let v = self.answers["save"];
                self.says(format!("m.save -> {}", v as u8));
                self.last_error.clear();
                if v {
                    let name = self.fields[0].clone();
                    self.model[0].push(name);
                } else {
                    self.last_error = cat(&[b"save error ", &self.error]);
                }
                return v as u32;
            }
            EE_SCREENS_OP_FORM_LIST_SELECTED => {
                assert_eq!(a, 0);
                let l = self.list(b);
                return if !l.data.is_empty() && l.has { l.sel } else { u32::MAX };
            }
            EE_SCREENS_OP_FORM_LIST_COUNT => {
                assert_eq!(a, 24);
                return self.list(b).data.len() as u32;
            }
            EE_SCREENS_OP_CE_LIST_SELECT => {
                self.says(format!("listselect {a} {b} 0 {c}"));
                let l = self.list(a);
                let n = l.data.len() as u32;
                l.sel = if n > 0 && b >= n { n - 1 } else { b };
                l.has = true;
            }
            EE_SCREENS_OP_CE_CLEAR_SELECTION => {
                self.says(format!("clearselection {a}"));
                self.list(a).has = false;
            }
            EE_SCREENS_OP_LIST_CLEAR => {
                self.says(format!("deleteall {a}"));
                *self.list(a) = List::default();
            }
            EE_SCREENS_OP_CE_COMBO_SELECTION => return self.combo,
            EE_SCREENS_OP_CE_COMBO_COUNT => return self.list(COMBO_LIST).data.len() as u32,
            EE_SCREENS_OP_COMBO_SELECT => {
                self.says(format!("combosel {a} {b}"));
                self.combo = b;
            }
            EE_SCREENS_OP_CE_BG_NONE => {
                self.line(b"string 20009");
                self.create_entries();
                self.none_line();
            }
            EE_SCREENS_GAME_CE_LIST_OK => return self.list_ok as u32,
            EE_SCREENS_GAME_CE_CHANGED => {
                let field = self.field(b);
                return (self.edits[&a][0] != field) as u32;
            }
            EE_SCREENS_OP_GROUP => {
                assert_eq!(c, 0);
                self.says(format!("group {a} {}", if b != 0 { "enable" } else { "disable" }));
            }
            EE_SCREENS_OP_POST_EVENT => {
                self.line(b"post null");
                self.posted.push(a);
            }
            EE_SCREENS_OP_CLOSE_FORM => self.line(b"closeform null"),
            EE_SCREENS_GAME_CREATE_EDITOR => self.line(b"createeditor"),
            EE_SCREENS_GAME_START_EDITOR => self.line(b"starteditor"),
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        match which {
            EE_SCREENS_TEXT_CE_FIELD => self.field(arg),
            EE_SCREENS_TEXT_CE_LIST => {
                let name = ["m.campaigns", "m.available", "m.scenarios"];
                if arg == EE_SCREENS_CE_LIST_SCENARIO_LIST {
                    self.list_ok = self.answers["list"];
                    self.says(format!("m.list -> {}", self.list_ok as u8));
                    self.last_error = if self.list_ok { Vec::new() } else { cat(&[b"list error ", &self.error]) };
                } else {
                    self.says(name[arg as usize].to_string());
                }
                self.model[arg as usize].iter().flat_map(|s| cat(&[s, b"\0"])).collect()
            }
            EE_SCREENS_TEXT_CE_ERROR => self.last_error.clone(),
            EE_SCREENS_TEXT_EDIT => self.edits[&arg][0].clone(),
            EE_SCREENS_TEXT_CE_ALL_TEXT => self.edits[&arg].join(&b'|'),
            EE_SCREENS_TEXT_LIST_DATA => {
                let l = self.list(arg >> 16);
                l.data.get((arg & 0xffff) as usize).cloned().unwrap_or_default()
            }
            EE_SCREENS_TEXT_COMBO_DATA => {
                assert_eq!(arg >> 16, 44);
                self.list(COMBO_LIST).data.get((arg & 0xffff) as usize).cloned().unwrap_or_default()
            }
            EE_SCREENS_TEXT_STRING => {
                self.says(format!("string {arg}"));
                format!("S{arg}").into_bytes()
            }
            EE_SCREENS_TEXT_CAMPAIGN_EXTENSION => b".cpn".to_vec(),
            EE_SCREENS_TEXT_CE_TRANSLATED => self.translated.clone(),
            _ => panic!("text {which:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        let answer = |d: &mut Self, name: &'static str, verb: &str, extra: String| -> bool {
            let v = d.answers[name];
            d.line(&cat(&[verb.as_bytes(), b" '", text, format!("'{extra} -> {}", v as u8).as_bytes()]));
            v
        };
        match which {
            EE_SCREENS_TEXT_CE_SET_FIELD => {
                self.line(&cat(&[b"m.", SETTERS[arg as usize].as_bytes(), b" '", text, b"'"]));
                if arg == EE_SCREENS_CE_FIELD_PASSWORD {
                    self.password = text.to_vec();
                    self.entered = true;
                } else {
                    self.fields[arg as usize] = text.to_vec();
                }
            }
            EE_SCREENS_TEXT_CE_ADD => {
                let v = answer(self, "add", "m.add", String::new());
                if v {
                    self.model[2].push(text.to_vec());
                }
                return v as u32;
            }
            EE_SCREENS_TEXT_CE_REMOVE => {
                let v = answer(self, "remove", "m.remove", String::new());
                if v {
                    self.model[2].pop();
                }
                return v as u32;
            }
            EE_SCREENS_TEXT_CE_MOVE => {
                return answer(self, "move", "m.move", format!(" {}", arg as i32)) as u32;
            }
            EE_SCREENS_TEXT_CE_DELETE => {
                let v = answer(self, "delete", "m.delete", String::new());
                self.last_error.clear();
                if v {
                    self.model[0].pop();
                } else {
                    self.last_error = cat(&[b"delete error ", &self.error]);
                }
                return v as u32;
            }
            EE_SCREENS_TEXT_CE_LOAD => {
                let v = answer(self, "load", "m.load", String::new());
                self.last_error = if v { Vec::new() } else { cat(&[b"load error ", &self.error]) };
                return v as u32;
            }
            EE_SCREENS_TEXT_CE_QUESTION => {
                self.line(&cat(&[
                    b"question '",
                    text,
                    format!("' 1 24 1 {}", (arg != EE_SCREENS_CE_ASK_DELETE) as u8).as_bytes(),
                ]));
                self.questions.push(match arg {
                    EE_SCREENS_CE_ASK_OPEN => (EE_SCREENS_CE_SAVE_BEFORE_OPEN, Some(EE_SCREENS_CE_DISCARD_BEFORE_OPEN)),
                    EE_SCREENS_CE_ASK_NEW => (EE_SCREENS_CE_SAVE_BEFORE_NEW, Some(EE_SCREENS_CE_DISCARD_BEFORE_NEW)),
                    EE_SCREENS_CE_ASK_MAIN => (EE_SCREENS_CE_SAVE_BEFORE_MAIN, Some(EE_SCREENS_CE_SHOW_MAIN)),
                    EE_SCREENS_CE_ASK_EDITOR => (EE_SCREENS_CE_SAVE_BEFORE_EDITOR, Some(EE_SCREENS_CE_SHOW_EDITOR)),
                    _ => (EE_SCREENS_CE_DELETE_YES, None),
                });
            }
            EE_SCREENS_TEXT_CE_APPEND => {
                let parts: Vec<Vec<u8>> = text.split(|&b| b == 0).map(|p| p.to_vec()).collect();
                let parts = &parts[..parts.len() - 1];
                self.line(&cat(&[format!("appendarray {arg}").as_bytes(), &quoted(parts, b" ")]));
                self.list(arg).data.extend(parts.iter().cloned());
            }
            EE_SCREENS_TEXT_CE_BG_EQUALS => {
                let mut line = self.list(COMBO_LIST).data[arg as usize].clone();
                let mut other = text.to_vec();
                utf8::map_in_place(&mut line, utf8::fold);
                utf8::map_in_place(&mut other, utf8::fold);
                return (line == other) as u32;
            }
            EE_SCREENS_TEXT_CE_TRANSLATE => {
                self.line(&cat(&[b"translate '", text, b"'"]));
                self.translated = cat(&[b"T:", text]);
            }
            EE_SCREENS_TEXT_EDIT => {
                self.line(&cat(&[format!("edit {arg} '").as_bytes(), text, b"'"]));
                self.edits.insert(arg, vec![text.to_vec()]);
            }
            EE_SCREENS_TEXT_LABEL => self.line(&cat(&[format!("label {arg} '").as_bytes(), text, b"' 1"])),
            EE_SCREENS_TEXT_MESSAGE => {
                self.line(&cat(&[b"message '", text, format!("' 1 null 0 {arg}").as_bytes()]));
                self.messages.push(false);
            }
            EE_SCREENS_TEXT_MESSAGE_NONE_FOUND => {
                self.line(&cat(&[b"message '", text, b"' 1 null 1 1"]));
                self.messages.push(true);
            }
            _ => panic!("put {which:#x}"),
        }
        0
    }
}

const KINDS: [u32; 14] = [
    EE_SCREENS_CE_OPEN,
    EE_SCREENS_CE_CLOSE,
    EE_SCREENS_CE_ADD_SCENARIO,
    EE_SCREENS_CE_REMOVE_SCENARIO,
    EE_SCREENS_CE_MOVE_UP,
    EE_SCREENS_CE_MOVE_DOWN,
    EE_SCREENS_CE_SAVE,
    EE_SCREENS_CE_SELECTED,
    EE_SCREENS_CE_PASSWORD,
    EE_SCREENS_CE_DELETE,
    EE_SCREENS_CE_NEW,
    EE_SCREENS_CE_MAIN_SCREEN,
    EE_SCREENS_CE_EDITOR,
    EE_SCREENS_CE_NAME_CHANGED,
];

#[test]
fn replays_the_cpp_golden() {
    // the driver's text-mode stdout ends its lines CR LF
    let want: Vec<&[u8]> =
        GOLDEN.split(|&b| b == b'\n').map(|l| l.strip_suffix(b"\r").unwrap_or(l)).filter(|l| !l.is_empty()).collect();
    let mut d = Driver { seed: 5, combo: u32::MAX, ..Driver::default() };
    let mut seen = [0usize; 18];
    for n in 0..2000 {
        let kind = if d.below(3) == 0 { 14 + d.below(4) } else { d.below(14) };
        d.out.push(format!("case {n} kind {kind}").into_bytes());
        d.state();
        match kind {
            0..=13 => {
                let r = dispatch(&mut d, KINDS[kind as usize]).unwrap();
                d.says(format!("result {r}"));
            }
            14 | 15 => {
                if !d.questions.is_empty() {
                    let i = d.below(d.questions.len() as u32) as usize;
                    let (yes, no) = d.questions[i];
                    d.questions.clear();
                    let f = if kind == 14 { Some(yes) } else { no };
                    d.says(format!("answer {i} 24 {}", f.is_some() as u8));
                    if let Some(code) = f {
                        dispatch(&mut d, code).unwrap();
                    }
                }
            }
            16 => {
                if !d.messages.is_empty() {
                    let i = d.below(d.messages.len() as u32) as usize;
                    let callback = d.messages.remove(i);
                    d.says(format!("closemessage {i} null {}", callback as u8));
                    if callback {
                        // (EEScreensNoCustomCampaigns: the custom campaign form's close, gUserInterface->CloseForm)
                        d.line(b"closeform null");
                    }
                }
            }
            _ => {
                for code in std::mem::take(&mut d.posted) {
                    let r = dispatch(&mut d, code).unwrap();
                    d.says(format!("posted result {r}"));
                }
            }
        }
        if d.questions.len() > 4 {
            d.questions.remove(0);
        }
        if d.messages.len() > 4 {
            d.messages.remove(0);
        }
        seen[kind as usize] += 1;
    }
    for (i, (got, want)) in d.out.iter().zip(&want).enumerate() {
        assert_eq!(String::from_utf8_lossy(got), String::from_utf8_lossy(want), "line {i}");
    }
    assert_eq!(d.out.len(), want.len());
    assert!(seen.iter().all(|&s| s > 30), "{seen:?}");
}
