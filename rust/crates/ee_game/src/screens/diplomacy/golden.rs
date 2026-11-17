//! The diplomacy form against the real C++: a scratch driver (`build/sc9/diplomacy_driver.cpp`) ran the unmodified
//! `EEUIEDiplomacyEvents.cpp` over fake controls (the icons' bitmaps, labels, check boxes, buttons with their texts
//! and fonts), up to 17 players (status, names, civilizations, colours, resources, stances, chat choices, tribute
//! fee), the keyboard's Shift, the communications, the user interface thread and the resource strings: one session
//! of 1,000 cases, each perturbing the players (printed; at times the texts redrawn and check boxes flipped), then
//! opening the form or firing a stored trigger on its control (`golden_diplomacy.txt`). The replay keeps the same
//! controls and prints what the driver's fakes print for each host call. The driver relinked over `EEScreens.cpp`
//! and this crate prints the same file, and the same on seeds 2-4 at 10,000 cases.

use std::collections::BTreeMap;

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_diplomacy.txt");

fn event_of(name: &str) -> u32 {
    match name {
        "class EEUIEDSOpen" => EE_SCREENS_DS_OPEN,
        "class EEUIEDSAlliedCheckBox" => EE_SCREENS_DS_ALLIED,
        "class EEUIEDSCustomChatCheckBox" => EE_SCREENS_DS_CUSTOM_CHAT,
        "class EEUIEDSTributeLClick" => EE_SCREENS_DS_TRIBUTE_RAISE,
        "class EEUIEDSTributeRClick" => EE_SCREENS_DS_TRIBUTE_LOWER,
        "class EEUIEDSOKButton" => EE_SCREENS_DS_OK,
        "class EEUIEDSClearTribute" => EE_SCREENS_DS_CLEAR,
        "class EEUIEDSCancel" => EE_SCREENS_DS_CANCEL,
        other => panic!("no event {other}"),
    }
}
fn name_of(event: u32) -> &'static str {
    match event {
        EE_SCREENS_DS_OPEN => "class EEUIEDSOpen",
        EE_SCREENS_DS_ALLIED => "class EEUIEDSAlliedCheckBox",
        EE_SCREENS_DS_CUSTOM_CHAT => "class EEUIEDSCustomChatCheckBox",
        EE_SCREENS_DS_TRIBUTE_RAISE => "class EEUIEDSTributeLClick",
        EE_SCREENS_DS_TRIBUTE_LOWER => "class EEUIEDSTributeRClick",
        EE_SCREENS_DS_OK => "class EEUIEDSOKButton",
        EE_SCREENS_DS_CLEAR => "class EEUIEDSClearTribute",
        EE_SCREENS_DS_CANCEL => "class EEUIEDSCancel",
        other => panic!("no name for {other:#x}"),
    }
}

/// `atoi` as the UCRT's: leading blanks, a sign, digits; out of range clamps.
fn atoi(text: &[u8]) -> i32 {
    let mut i = 0;
    while i < text.len() && (text[i] == b' ' || (b'\t'..=b'\r').contains(&text[i])) {
        i += 1;
    }
    let negative = i < text.len() && text[i] == b'-';
    if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
        i += 1;
    }
    let mut value: i64 = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        value = (value * 10 + (text[i] - b'0') as i64).min(1 << 40);
        i += 1;
    }
    let value = if negative { -value } else { value };
    value.clamp(i32::MIN as i64, i32::MAX as i64) as i32
}

#[derive(Clone, Default)]
struct Player {
    record: EeScreensDiplomacyPlayer,
    name: Vec<u8>,
    civilization: Vec<u8>,
    colour: [u32; 3],
}

#[derive(Default)]
struct World {
    out: Vec<String>,
    players: Vec<Option<Player>>,
    current: u32,
    locked: bool,
    shift: i64,
    close: i64,
    texts: BTreeMap<u32, Vec<u8>>,
    checks: BTreeMap<u32, bool>,
    fg: BTreeMap<u32, [u32; 3]>,
    entity: u32,
}

fn s(text: &[u8]) -> String {
    String::from_utf8(text.to_vec()).unwrap()
}

impl DiplomacyHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_DS_PLAYERS => return self.players.len() as u32,
            EE_SCREENS_GAME_DS_CURRENT => return self.current,
            EE_SCREENS_GAME_DS_TEAMS_LOCKED => return self.locked as u32,
            EE_SCREENS_OP_DS_UV => {
                self.out.push(format!("  uv {a} {b:08x} {:08x} {c:08x} {:08x} {b:08x} {d:08x} {c:08x} {d:08x}", 0, 0))
            }
            EE_SCREENS_OP_DS_BG => self.out.push(format!("  bg {a} {b:08x} {c:08x} {d:08x}")),
            EE_SCREENS_OP_DS_FG => {
                let colour = self.players[b as usize].as_ref().unwrap().colour;
                self.fg.insert(a, colour);
            }
            EE_SCREENS_OP_DS_DISABLE => self.out.push(format!("  disabled {a} 1")),
            EE_SCREENS_OP_DS_BUTTON_NUMBER | EE_SCREENS_OP_DS_LABEL_NUMBER => {
                return atoi(self.texts.get(&a).map_or(&b""[..], |t| t)) as u32;
            }
            EE_SCREENS_OP_LABEL_NUMBER => {
                let text = (b as i32).to_string().into_bytes();
                self.put_text(EE_SCREENS_TEXT_LABEL, a, &text);
            }
            EE_SCREENS_OP_SET_CHECKBOX => {
                self.out.push(format!("  set {a} {b}"));
                self.checks.insert(a, b != 0);
            }
            EE_SCREENS_OP_CHECKBOX => return *self.checks.get(&a).unwrap_or(&false) as u32,
            EE_SCREENS_OP_SHOW => self.out.push(format!("  mode {a} {b}")),
            EE_SCREENS_OP_TRIGGER_HERE => self.out.push(format!("  trigger {a} {b} {}", name_of(c))),
            EE_SCREENS_OP_ENTITY_INDEX => return self.entity,
            EE_SCREENS_GAME_HK_SHIFT => {
                self.out.push("  keyentity 225".into());
                return self.shift as u32;
            }
            EE_SCREENS_GAME_MG_HAS_PLAYER => {
                return self.players.get(self.current as usize).is_some_and(|p| p.is_some()) as u32
            }
            EE_SCREENS_GAME_DS_RESOURCES => {
                self.out.push(format!("  local 155 255 player {} resource {} amount {c} add {d}", a as i32, b as i32))
            }
            EE_SCREENS_GAME_DS_STANCE => {
                self.out.push(format!("  local 153 255 player {} stance {}", a as i32, b as i32))
            }
            EE_SCREENS_GAME_DS_CHAT => self.out.push(format!("  local 162 255 player {} selected {b}", a as i32)),
            EE_SCREENS_OP_CLOSE_FORM => {
                self.out.push(format!("  closeform 0 -> {}", self.close));
                return self.close as u32;
            }
            other => panic!("unexpected op {other:#x}"),
        }
        0
    }
    fn player(&mut self, index: u32) -> Option<EeScreensDiplomacyPlayer> {
        self.players.get(index as usize).cloned().flatten().map(|p| p.record)
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        let p = self.players[arg as usize].as_ref().unwrap();
        match which {
            EE_SCREENS_TEXT_DS_NAME => p.name.clone(),
            EE_SCREENS_TEXT_DS_CIVILIZATION => p.civilization.clone(),
            other => panic!("unexpected text {other:#x}"),
        }
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        match which {
            EE_SCREENS_TEXT_LABEL => self.out.push(format!("  text {arg} '{}' 1", s(text))),
            EE_SCREENS_TEXT_DS_BUTTON => self.out.push(format!("  btext {arg} '{}' font {}", s(text), arg % 3)),
            other => panic!("unexpected put {other:#x}"),
        }
        self.texts.insert(arg, text.to_vec());
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        format!("S{id}").into_bytes()
    }
    fn pay_tribute(&mut self, to: u32, a: [u32; 5]) {
        self.out.push(format!(
            "  local 68 255 to {} food {} wood {} gold {} stone {} iron {}",
            to as u8, a[0], a[1], a[2], a[3], a[4]
        ));
    }
}

/// The tokens of a line: words, and `'...'` texts (quotes kept off) as one token each, `N='...'` as one.
fn tokens(line: &str) -> Vec<String> {
    let mut out = Vec::new();
    let mut cur = String::new();
    let mut quoted = false;
    for ch in line.chars() {
        match ch {
            '\'' => quoted = !quoted,
            ' ' if !quoted => {
                if !cur.is_empty() {
                    out.push(std::mem::take(&mut cur));
                }
            }
            _ => cur.push(ch),
        }
    }
    if !cur.is_empty() || line.ends_with("''") {
        out.push(cur);
    }
    out
}

fn hex(t: &str) -> u32 {
    u32::from_str_radix(t, 16).unwrap()
}

fn read_player(line: &str) -> (usize, Option<Player>) {
    let t = tokens(line);
    let index: usize = t[1].parse().unwrap();
    if t[2] == "none" {
        return (index, None);
    }
    // player i id I result R observer O slot S human H name N civ C color r g b fee F res 5 enemies 17 chat bits
    // (an empty name or civilization is no token: find the fields by their keys)
    let at = |key: &str| t.iter().position(|x| x == key).unwrap();
    let name_at = at("name");
    let civ_at = at("civ");
    let color_at = at("color");
    let name = if civ_at == name_at + 2 { t[name_at + 1].clone() } else { String::new() };
    let civilization = if color_at == civ_at + 2 { t[civ_at + 1].clone() } else { String::new() };
    let mut record = EeScreensDiplomacyPlayer {
        player_id: t[at("id") + 1].parse().unwrap(),
        game_result: t[at("result") + 1].parse().unwrap(),
        observer: t[at("observer") + 1] == "1",
        slot: t[at("slot") + 1].parse().unwrap(),
        human: t[at("human") + 1] == "1",
        tribute_fee: f32::from_bits(hex(&t[at("fee") + 1])),
        ..Default::default()
    };
    let res = at("res");
    for k in 0..5 {
        record.resources[k] = t[res + 1 + k].parse().unwrap();
    }
    let enemies = at("enemies");
    for q in 0..17 {
        record.enemies[q] = t[enemies + 1 + q].parse().unwrap();
    }
    let chat = t[at("chat") + 1].as_bytes();
    for (q, c) in record.send_chat.iter_mut().enumerate() {
        *c = chat[q] == b'1';
    }
    let colour = [hex(&t[color_at + 1]), hex(&t[color_at + 2]), hex(&t[color_at + 3])];
    (index, Some(Player { record, name: name.into_bytes(), civilization: civilization.into_bytes(), colour }))
}

/// `N='text'` or `N=v` pairs after the first word.
fn pairs(line: &str) -> Vec<(u32, String)> {
    let body = line.trim_start().split_once(' ').map_or("", |x| x.1);
    let mut out = Vec::new();
    let mut rest = body;
    while let Some(eq) = rest.find('=') {
        let key: u32 = rest[..eq].trim().parse().unwrap();
        let after = &rest[eq + 1..];
        if let Some(text) = after.strip_prefix('\'') {
            let end = text.find('\'').unwrap();
            out.push((key, text[..end].to_string()));
            rest = &text[end + 1..];
        } else {
            let end = after.find(' ').unwrap_or(after.len());
            out.push((key, after[..end].to_string()));
            rest = &after[end..];
        }
    }
    out
}

#[test]
fn golden_diplomacy() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 1000);
    let mut w = World::default();
    let mut kinds = std::collections::BTreeSet::new();
    for (n, case) in cases.iter().enumerate() {
        let head: Vec<&str> = case[0].split(' ').collect();
        let (event, entity) = if head[2] == "open" {
            (event_of(&head[3..].join(" ")), 0)
        } else {
            (event_of(&head[5..].join(" ")), head[3].parse().unwrap())
        };
        kinds.insert(event);
        let mut i = 1;
        let st = tokens(case[i]);
        let players: usize = st[2].parse().unwrap();
        w.current = st[4].parse().unwrap();
        w.locked = st[6] == "1";
        w.shift = st[8].parse().unwrap();
        w.close = st[10].parse().unwrap();
        w.players = vec![None; players];
        i += 1;
        while case[i].starts_with("  player ") {
            let (index, p) = read_player(case[i]);
            w.players[index] = p;
            i += 1;
        }
        if case[i].starts_with("  redraw") {
            for (key, value) in pairs(case[i]) {
                w.texts.insert(key, value.into_bytes());
            }
            i += 1;
        }
        assert!(case[i].starts_with("  checks"), "case {n}: {}", case[i]);
        for (key, value) in pairs(case[i]) {
            w.checks.insert(key, value == "1");
        }
        i += 1;
        w.out.clear();
        w.fg.clear();
        w.entity = entity;
        let result = dispatch(&mut w, event).unwrap();
        w.out.push(format!("  result {result}"));
        let mut after = String::from("  after");
        for (control, c) in &w.fg {
            after += &format!(" fg{control}={:08x},{:08x},{:08x}", c[0], c[1], c[2]);
        }
        w.out.push(after);
        let expected = &case[i..];
        for (k, e) in expected.iter().enumerate() {
            assert!(k < w.out.len(), "case {n} ({}): missing line {k}: {e}", case[0]);
            assert_eq!(w.out[k], *e, "case {n} ({}) line {k}", case[0]);
        }
        assert_eq!(w.out.len(), expected.len(), "case {n}: extra lines {:?}", &w.out[expected.len()..]);
    }
    assert_eq!(kinds.len(), 8);
}
