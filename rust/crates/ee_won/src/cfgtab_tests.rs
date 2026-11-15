use super::*;
use crate::chat::CChat;

/// One host event as the driver logged it (tests/won/won_cfgtab_test.cpp `--print`).
#[derive(Debug, Clone, PartialEq)]
enum Ev {
    Read(i32, bool),
    Comma(bool),
    End(bool),
    Text(i32, Vec<u32>),
    Push(i32),
    Apply(i32, bool),
    Fail(i32, Vec<u8>),
    Clear,
}

fn unhex(s: &str, width: usize) -> Vec<u32> {
    if s == "-" {
        return vec![];
    }
    (0..s.len() / width).map(|i| u32::from_str_radix(&s[i * width..(i + 1) * width], 16).unwrap()).collect()
}

fn parse_ev(e: &str) -> Ev {
    let b = |v: &str| v == "1";
    let (head, rest) = e.split_at(1);
    match head {
        "r" => {
            let (k, v) = rest.split_once('=').unwrap();
            Ev::Read(k.parse().unwrap(), b(v))
        }
        "c" => Ev::Comma(b(&rest[1..])),
        "e" => Ev::End(b(&rest[1..])),
        "t" => {
            let (s, v) = rest.split_once('=').unwrap();
            Ev::Text(s.parse().unwrap(), unhex(v, 4))
        }
        "p" => Ev::Push(rest.parse().unwrap()),
        "a" => {
            let (s, v) = rest.split_once('=').unwrap();
            Ev::Apply(s.parse().unwrap(), b(v))
        }
        "f" => {
            let (s, m) = rest.split_once(':').unwrap();
            Ev::Fail(s.parse().unwrap(), unhex(m, 4).into_iter().map(|u| u as u8).collect())
        }
        "x" => Ev::Clear,
        _ => panic!("event {e}"),
    }
}

/// A host that answers from a recorded run and checks every request against it.
struct Replay {
    evs: Vec<Ev>,
    at: usize,
}

impl Replay {
    fn next(&mut self) -> Ev {
        let e = self.evs.get(self.at).cloned().unwrap_or_else(|| panic!("no event left at {}", self.at));
        self.at += 1;
        e
    }
    fn text(&mut self, slot: i32) -> Vec<u32> {
        match self.next() {
            Ev::Text(s, t) if s == slot => t,
            e => panic!("expected text {slot}, got {e:?}"),
        }
    }
}

fn name_of(t: i32, units: &[u32]) -> i32 {
    name_index(t, |n| {
        let narrow = crate::guistring::widen(n, 4);
        crate::guistring::compare_no_case_narrow(units, 0, &narrow, &CChat) == 0
    })
}

impl CfgHost for Replay {
    fn read(&mut self, kind: EeWonCfgRead) -> bool {
        match self.next() {
            Ev::Read(k, r) if k == kind as i32 => r,
            e => panic!("expected read {kind:?}, got {e:?}"),
        }
    }
    fn comma(&mut self) -> bool {
        match self.next() {
            Ev::Comma(r) => r,
            e => panic!("expected comma, got {e:?}"),
        }
    }
    fn end(&mut self) -> bool {
        match self.next() {
            Ev::End(r) => r,
            e => panic!("expected end, got {e:?}"),
        }
    }
    fn name(&mut self, t: i32, slot: i32) -> i32 {
        let u = self.text(slot);
        name_of(t, &u)
    }
    fn empty(&mut self, slot: i32) -> bool {
        self.text(slot).is_empty()
    }
    fn push(&mut self, v: i32) {
        assert_eq!(self.next(), Ev::Push(v));
    }
    fn apply(&mut self, setter: i32) -> bool {
        match self.next() {
            Ev::Apply(s, r) if s == setter => r,
            e => panic!("expected apply {setter}, got {e:?}"),
        }
    }
    fn fail(&mut self, msg: &[u8], slot: i32) {
        assert_eq!(self.next(), Ev::Fail(slot, msg.to_vec()));
    }
    fn clear(&mut self) {
        assert_eq!(self.next(), Ev::Clear);
    }
}

fn replay_line(line: &str) {
    let f: Vec<&str> = line.split('\t').collect();
    let table: i32 = f[0].parse().unwrap();
    let key: Vec<u8> = unhex(f[1], 4).into_iter().map(|u| u as u8).collect();
    let want: i32 = f[2].parse().unwrap();
    let evs: Vec<Ev> = if f[3] == "-" { vec![] } else { f[3].split(' ').map(parse_ev).collect() };
    let mut h = Replay { evs, at: 0 };
    let got = match program(table, &key) {
        None => -1,
        Some(p) => {
            if run(table, p, &mut h) {
                0
            } else {
                1
            }
        }
    };
    assert_eq!(got, want, "{line}");
    assert_eq!(h.at, h.evs.len(), "events left over: {line}");
}

#[test]
fn golden_replay() {
    let g = include_str!("golden_cfgtab.txt");
    let mut n = 0;
    for line in g.lines().filter(|l| !l.is_empty()) {
        replay_line(line);
        n += 1;
    }
    assert!(n > 1000, "{n} lines");
}

/// A scripted host for the unit tests: reads succeed unless listed, end-of-string answers in turn, texts given.
#[derive(Default)]
struct Script {
    log: Vec<String>,
    ends: Vec<bool>,
    texts: Vec<&'static str>,
    throw_read: Option<usize>,
    reads: usize,
    fail_apply: bool,
}

impl CfgHost for Script {
    fn read(&mut self, kind: EeWonCfgRead) -> bool {
        self.reads += 1;
        self.log.push(format!("r{kind:?}"));
        self.throw_read != Some(self.reads)
    }
    fn comma(&mut self) -> bool {
        self.log.push("c".into());
        true
    }
    fn end(&mut self) -> bool {
        self.log.push("e".into());
        if self.ends.is_empty() {
            true
        } else {
            self.ends.remove(0)
        }
    }
    fn name(&mut self, t: i32, _slot: i32) -> i32 {
        let s = self.texts.remove(0);
        let u: Vec<u32> = s.chars().map(u32::from).collect();
        name_of(t, &u)
    }
    fn empty(&mut self, _slot: i32) -> bool {
        if self.texts.first().is_some_and(|s| s.is_empty()) {
            self.texts.remove(0);
            true
        } else {
            false
        }
    }
    fn push(&mut self, v: i32) {
        self.log.push(format!("p{v}"));
    }
    fn apply(&mut self, setter: i32) -> bool {
        self.log.push(format!("a{}:{}", setter >> 8, setter & 0xff));
        !self.fail_apply
    }
    fn fail(&mut self, msg: &[u8], slot: i32) {
        self.log.push(format!("f{slot}:{}", String::from_utf8_lossy(msg)));
    }
    fn clear(&mut self) {
        self.log.push("x".into());
    }
}

fn go(table: i32, key: &str, h: &mut Script) -> Result<(), ()> {
    if run(table, program(table, key.as_bytes()).expect("a key of the table"), h) {
        Ok(())
    } else {
        Err(())
    }
}

#[test]
fn unknown_keys_and_tables() {
    assert!(program(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, b"TEXT").is_some());
    assert!(program(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, b"text").is_none());
    assert!(program(EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, b"MODIFY").is_none());
    // a key table the reader does not run (the base ComponentConfig's)
    assert!(program(EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY, b"MODIFY").is_none());
    assert!(!ee_won_cfg_tab_has(EE_WON_CFG_KEYS_COMPONENTCONFIG_VIRTUAL24_KEY));
    assert!(ee_won_cfg_tab_has(EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION));
}

#[test]
fn every_handler_holds_a_program_per_key() {
    for (t, progs) in PROGRAMS {
        assert_eq!(progs.len(), names(*t).len(), "table {t}");
        for p in progs.iter() {
            for op in p.iter() {
                if let Je(x) | J(x) | Jz(x) = op {
                    assert!(*x == END || usize::from(*x) <= p.len(), "table {t}: jump {x}");
                }
            }
        }
    }
    assert_eq!(PROGRAMS.len(), 45);
}

#[test]
fn optional_second_argument() {
    // InputBox CURSORBLINKRATE: on, then off only when more follows
    let t = EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION;
    let mut h = Script { ends: vec![true], ..Default::default() };
    go(t, "CURSORBLINKRATE", &mut h).unwrap();
    assert_eq!(h.log, ["x", "rInt", "e", "a34:14"]);
    let mut h = Script { ends: vec![false], ..Default::default() };
    go(t, "CURSORBLINKRATE", &mut h).unwrap();
    assert_eq!(h.log, ["x", "rInt", "e", "c", "rInt", "a34:14"]);
}

#[test]
fn text_items_ask_twice_at_the_end() {
    // while(!EndOfString()) { add; if(!EndOfString()) EnsureComma(); }
    let t = EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY;
    let mut h = Script { ends: vec![false, false, false, true, true], ..Default::default() };
    go(t, "TEXTITEM", &mut h).unwrap();
    assert_eq!(h.log, ["x", "e", "x", "rStr", "a5:4", "e", "c", "e", "x", "rStr", "a5:4", "e", "e"]);
}

#[test]
fn flags_until_an_empty_flag_and_unknown_names_throw() {
    let t = EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION;
    let mut h = Script { texts: vec!["radio", "WantDefault", "noactivate", ""], ..Default::default() };
    go(t, "BUTTONFLAGS", &mut h).unwrap();
    assert_eq!(
        h.log,
        ["x", "x", "rFlag", "p8", "a4:0", "x", "rFlag", "p1024", "a4:0", "x", "rFlag", "p512", "a4:0", "x", "rFlag"]
    );
    let mut h = Script { texts: vec!["Bogus"], ..Default::default() };
    assert!(go(t, "BUTTONFLAGS", &mut h).is_err());
    assert_eq!(h.log.last().unwrap(), "f0:Unknown button flag");
}

#[test]
fn a_throwing_read_stops_the_program() {
    let t = EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY;
    let mut h = Script { throw_read: Some(2), ..Default::default() };
    assert!(go(t, "BORDERS", &mut h).is_err());
    // the left border was set before the top's read threw
    assert_eq!(h.log, ["x", "rInt", "a81:0", "c", "rInt"]);
}

#[test]
fn arrow_direction_ignores_other_words() {
    let t = EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY;
    let mut h = Script { texts: vec!["sideways"], ..Default::default() };
    go(t, "DIRECTION", &mut h).unwrap();
    assert_eq!(h.log, ["x", "rStr"]);
    let mut h = Script { texts: vec!["left"], ..Default::default() };
    go(t, "DIRECTION", &mut h).unwrap();
    assert_eq!(h.log, ["x", "rStr", "p0", "a40:0"]);
}

#[test]
fn enum_values_are_the_originals() {
    let q = |s: &'static str| {
        let mut h = Script { texts: vec![s], ..Default::default() };
        go(EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION, "MAXQUADRANT", &mut h).unwrap();
        h.log[2].clone()
    };
    assert_eq!(q("Left"), "p1");
    assert_eq!(q("BOTTOMRIGHT"), "p12");
    let mut h = Script { texts: vec!["center"], ..Default::default() };
    go(EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION, "ADDINVISIBLESPLITTER", &mut h).unwrap();
    assert_eq!(h.log, ["x", "rStr", "p271", "e", "a69:2"]);
    let mut h = Script { texts: vec!["CheckedOver"], ends: vec![true], ..Default::default() };
    go(EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, "BUTTONSTATE", &mut h).unwrap();
    assert_eq!(h.log, ["x", "rStr", "c", "p259", "rImage", "e", "a80:15"]);
    let mut h = Script { texts: vec!["Nope"], ..Default::default() };
    assert!(go(EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION, "LISTSORTTYPE", &mut h).is_err());
    assert_eq!(h.log.last().unwrap(), "f-1:Invalid list sort type.");
}

#[test]
fn a_throwing_setter_stops_the_program() {
    // WONFrameComponentConfig BORDERS: the left border's setter throws; nothing more is read
    let t = EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY;
    let mut h = Script { fail_apply: true, ..Default::default() };
    assert!(go(t, "BORDERS", &mut h).is_err());
    assert_eq!(h.log, ["x", "rInt", "a81:0"]);
}

#[test]
fn optional_sizes_take_their_comma() {
    // ADDINVISIBLESPLITTER type, size: the comma before the size
    let mut h = Script { texts: vec!["UpDown"], ends: vec![false], ..Default::default() };
    go(EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION, "ADDINVISIBLESPLITTER", &mut h).unwrap();
    assert_eq!(h.log, ["x", "rStr", "p10", "e", "c", "rInt", "a69:2"]);
}

#[test]
fn every_state_and_flag_value() {
    let states = [
        "Normal",
        "Pressed",
        "Disabled",
        "Over",
        "Default",
        "CheckedNormal",
        "CheckedPressed",
        "CheckedDisabled",
        "CheckedOver",
        "CheckedDefault",
    ];
    let want = [0, 1, 2, 3, 4, 0x100, 0x101, 0x102, 0x103, 0x104];
    for (s, w) in states.iter().zip(want) {
        let mut h = Script { texts: vec![s], ends: vec![true], ..Default::default() };
        go(EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, "BUTTONSTATE", &mut h).unwrap();
        assert_eq!(h.log[3], format!("p{w}"), "{s}");
    }
    let flags = ["WordWrap", "AllowSelection", "ScrollOnBottom", "ShowPartialLines", "ScrollFullLine", "DelayWordWrap"];
    for (i, f) in flags.iter().enumerate() {
        let mut h = Script { texts: vec![f, ""], ..Default::default() };
        go(EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, "TEXTFLAGS", &mut h).unwrap();
        assert_eq!(h.log[3], format!("p{}", 1 << i), "{f}");
    }
}
