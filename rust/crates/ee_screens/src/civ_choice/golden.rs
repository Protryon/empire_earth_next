//! The civilization choice form against the real C++: a scratch driver (`build/sc10/pdcs_driver.cpp`) ran the
//! unmodified `EEUIEPDCSEvents.cpp` (and its header's in-class bodies) over fake list boxes (the civilization list's
//! lines, selection and selection flag; the statistics list), the label, the civilization manager (up to four
//! civilizations with their names and files), the loader (its result, name and upgrades), the resource strings, the
//! communications (multiplayer, the local input), the user interface thread (posted events, closing the form) and
//! the manager (messages, Pop): one session of 2,000 cases, each drawing that state (printed) and firing one of the
//! form's six events, then (three times in four) running the events it and earlier cases posted, at times with the
//! state redrawn (`golden_civ_choice.txt`). The replay prints what the driver's fakes print for each host call. The
//! driver relinked over `EEScreens.cpp` and this crate prints the same file, and the same on seeds 2-4 at 20,000
//! cases.

use std::collections::VecDeque;

use super::*;
use crate::menus::EE_SCREENS_TEXT_STRING;

const GOLDEN: &[u8] = include_bytes!("../golden_civ_choice.txt");

fn s(text: &[u8]) -> String {
    String::from_utf8(text.to_vec()).unwrap()
}

#[derive(Default)]
struct Style {
    sort: bool,
    auto: bool,
    colour: [u32; 3],
}

#[derive(Default)]
struct World {
    out: Vec<String>,
    civs: Vec<(String, String)>,
    lines: Vec<String>,
    sel: u32,
    has: bool,
    selected: bool,
    load: i64,
    upgrades: Vec<u32>,
    multi: bool,
    player: u32,
    kept: Option<(String, Vec<u32>)>,
    posted: VecDeque<u32>,
    civs_style: Style,
    stats_style: Style,
    stats_points: bool,
    stats_only: bool,
}

const SCREEN_NAMES: [(u32, u32); 7] = [
    (EE_SCREENS_PDCS_OPEN, 8961),
    (EE_SCREENS_PDCS_CLOSE, 8962),
    (EE_SCREENS_PDCS_SELECT, 8963),
    (EE_SCREENS_PDCS_OK, 8964),
    (EE_SCREENS_PDCS_CANCEL, 8965),
    (EE_SCREENS_PDCS_CLEAR, 8966),
    (EE_SCREENS_PDCS_LOAD, 8967),
];
fn screen(code: u32) -> String {
    let low = code & 0xffff;
    assert!(SCREEN_NAMES.iter().any(|&(c, n)| c == low && n == low));
    format!("screen {low}")
}

impl World {
    fn read_state(&mut self, line: &str) {
        // state civs 'n'='f' ... lines 'l' ... sel S has H selected X load L upgrades u... multi M player P
        let mut quoted = Vec::new();
        let mut rest = line;
        let lines_at = line.find(" lines").unwrap();
        while let Some(a) = rest.find('\'') {
            let b = rest[a + 1..].find('\'').unwrap() + a + 1;
            let offset = line.len() - rest.len();
            quoted.push((offset + a, rest[a + 1..b].to_string()));
            rest = &rest[b + 1..];
        }
        self.civs.clear();
        self.lines.clear();
        let civ_quotes: Vec<String> = quoted.iter().filter(|q| q.0 < lines_at).map(|q| q.1.clone()).collect();
        for pair in civ_quotes.chunks(2) {
            self.civs.push((pair[0].clone(), pair[1].clone()));
        }
        for q in quoted.iter().filter(|q| q.0 > lines_at) {
            self.lines.push(q.1.clone());
        }
        let tail = &line[line.find(" sel ").unwrap()..];
        let t: Vec<&str> = tail.split_whitespace().collect();
        self.sel = t[1].parse().unwrap();
        self.has = t[3] == "1";
        self.selected = t[5] == "1";
        self.load = t[7].parse().unwrap();
        let up = t.iter().position(|x| *x == "upgrades").unwrap();
        let multi = t.iter().position(|x| *x == "multi").unwrap();
        self.upgrades = t[up + 1..multi].iter().map(|x| x.parse().unwrap()).collect();
        self.multi = t[multi + 1] == "1";
        self.player = t[multi + 3].parse().unwrap();
    }
    fn current_selection(&mut self) -> bool {
        self.out.push(format!("  currentselection 3 -> {} {}", self.selected as u32, self.sel));
        self.selected
    }
    fn upgrades_text(upgrades: &[u32]) -> String {
        upgrades.iter().map(|u| format!(" {u}")).collect()
    }
}

impl CivChoiceHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        match op {
            EE_SCREENS_OP_TRIGGER_HERE => self.out.push(format!("  trigger {a} {b} {}", screen(c))),
            EE_SCREENS_OP_GO_HOT_KEY => {
                self.out.push(format!("  hotkey {a} 32767 32767 32767 {} null null null 0 0", screen(b)))
            }
            EE_SCREENS_OP_PDCS_STYLE => {
                let style = if a == PDCS_CIVS { &mut self.civs_style } else { &mut self.stats_style };
                style.sort = b & PDCS_STYLE_SORTED != 0;
                style.auto = false;
                style.colour = [c, c, d];
                if b & PDCS_STYLE_STATISTICS != 0 {
                    self.stats_points = false;
                    self.stats_only = false;
                }
            }
            EE_SCREENS_GAME_PDCS_FILL => self.out.push(format!("  fill {a}")),
            EE_SCREENS_OP_TAB_STOP => self.out.push(format!("  tabstop {a}")),
            EE_SCREENS_OP_PDCS_HAS_SELECTION => return self.has as u32,
            EE_SCREENS_GAME_PDCS_PAUSE => self.out.push("  pause".into()),
            EE_SCREENS_OP_LIST_CLEAR | EE_SCREENS_OP_PDCS_CLEAR_STATS => self.out.push("  deleteall 4".into()),
            EE_SCREENS_GAME_PDCS_CIVS => {
                self.out.push(format!("  getcivilizations {}", self.civs.len()));
                return self.civs.len() as u32;
            }
            EE_SCREENS_OP_SHOW => self.out.push(format!("  mode {a} {b}")),
            EE_SCREENS_OP_POST_EVENT => {
                self.out.push(format!("  post {} 0", screen(a)));
                self.posted.push_back(a);
            }
            EE_SCREENS_OP_PDCS_SELECTION => {
                return if self.current_selection() { self.sel.wrapping_add(1) } else { 0 };
            }
            EE_SCREENS_OP_PDCS_CONTROLS => return 1,
            EE_SCREENS_OP_PDCS_SET_UPGRADES => {
                let (_, upgrades) = self.kept.clone().unwrap();
                self.out.push(format!("  setupgrades 4{}", World::upgrades_text(&upgrades)));
            }
            EE_SCREENS_OP_PDCS_HIDE_LABEL => self.out.push("  mode 5 0".into()),
            EE_SCREENS_OP_PDCS_RESET => self.out.push("  resetselection 3".into()),
            EE_SCREENS_GAME_PDCS_REQUEST => {
                let (name, upgrades) = self.kept.clone().unwrap();
                let body = format!(
                    "type 126 player {} name '{name}' string 0 upgrades{}",
                    self.player,
                    World::upgrades_text(&upgrades)
                );
                if self.multi {
                    self.out.push(format!("  localinput 254 {body}"));
                } else {
                    self.out.push(format!("  execute {body}"));
                }
            }
            EE_SCREENS_OP_CLOSE_FORM => self.out.push("  closeform 0".into()),
            EE_SCREENS_OP_POP => self.out.push("  pop".into()),
            other => panic!("unexpected op {other:#x}"),
        }
        0
    }
    fn text(&mut self, which: u32, arg: u32) -> Option<Vec<u8>> {
        match which {
            EE_SCREENS_TEXT_STRING => {
                self.out.push(format!("  string {arg}"));
                Some(format!("S{arg}").into_bytes())
            }
            EE_SCREENS_TEXT_PDCS_LINE => {
                if !self.current_selection() {
                    return None;
                }
                Some(self.lines[self.sel as usize].clone().into_bytes())
            }
            EE_SCREENS_TEXT_PDCS_CIV_NAME => {
                self.out.push(format!("  string {}", 50000 + arg));
                Some(self.civs[arg as usize].0.clone().into_bytes())
            }
            EE_SCREENS_TEXT_PDCS_CIV_FILE => Some(self.civs[arg as usize].1.clone().into_bytes()),
            other => panic!("unexpected text {other:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        match which {
            EE_SCREENS_TEXT_LABEL => self.out.push(format!("  text {arg} '{}' 1", s(text))),
            EE_SCREENS_TEXT_PDCS_LABEL => self.out.push(format!("  text 5 '{}' 1", s(text))),
            EE_SCREENS_TEXT_MESSAGE => self.out.push(format!("  message '{}' 1 0 0 {arg}", s(text))),
            EE_SCREENS_TEXT_PDCS_LOAD => {
                self.out.push("  civdir 0".into());
                self.out.push(format!("  loadciv '{}' 'Civilizations\\' -> {}", s(text), self.load));
                self.kept = Some((format!("Civ of {}", s(text)), self.upgrades.clone()));
                return self.load as u32;
            }
            other => panic!("unexpected put {other:#x}"),
        }
        0
    }
}

#[test]
fn golden_civ_choice() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let state = Mutex::new(CivChoice::new());
    let mut w = World::default();
    let mut posted_runs = 0;
    for (n, case) in cases.iter().enumerate() {
        let event = match case[0].split(' ').nth(2).unwrap() {
            "open" => EE_SCREENS_PDCS_OPEN,
            "close" => EE_SCREENS_PDCS_CLOSE,
            "select" => EE_SCREENS_PDCS_SELECT,
            "ok" => EE_SCREENS_PDCS_OK,
            "cancel" => EE_SCREENS_PDCS_CANCEL,
            "clear" => EE_SCREENS_PDCS_CLEAR,
            other => panic!("no event {other}"),
        };
        w.read_state(case[1]);
        w.out.clear();
        let result = dispatch(&mut w, &state, event).unwrap();
        w.out.push(format!("  result {result}"));
        // the posted events, run where the driver ran them (a state line before a run redraws the state)
        let mut i = 2 + w.out.len();
        while i < case.len() && !case[i].starts_with("  after") {
            if case[i].starts_with("  state") {
                w.read_state(case[i]);
                w.out.push(case[i].to_string());
                i += 1;
                continue;
            }
            assert_eq!(case[i], "  run posted", "case {n}");
            w.out.push(case[i].to_string());
            let code = w.posted.pop_front().unwrap();
            let posted = dispatch(&mut w, &state, code).unwrap();
            w.out.push(format!("  posted result {posted}"));
            posted_runs += 1;
            i = 2 + w.out.len();
        }
        let (c, st) = (&w.civs_style, &w.stats_style);
        w.out.push(format!(
            "  after civs sort {} auto {} colour {:08x} {:08x} {:08x} stats sort {} auto {} points {} upgradesonly {} colour {:08x} {:08x} {:08x}",
            c.sort as u32,
            c.auto as u32,
            c.colour[0],
            c.colour[1],
            c.colour[2],
            st.sort as u32,
            st.auto as u32,
            w.stats_points as u32,
            w.stats_only as u32,
            st.colour[0],
            st.colour[1],
            st.colour[2]
        ));
        assert_eq!(w.out, &case[2..], "case {n} ({})", case[0]);
    }
    assert!(posted_runs > 40);
}
