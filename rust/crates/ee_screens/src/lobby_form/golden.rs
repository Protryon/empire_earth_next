//! The lobby against the real C++: a scratch driver (`build/sc7/lobby_driver.cpp`) ran the unmodified
//! `EEUIELobbyEvents.cpp` over fake forms (the lobby's controls, the load screen's), triggers, manager, user interface
//! thread, communications (an `EEServer`'s memory), messages, save files and headers (their outcomes from a hash of
//! the name), rules, generators, engines and strings, the real `UWideString`: one session of 2,000 cases, each
//! perturbing the options, the slots, the controls, the server and the forms, then firing one stored trigger event
//! (each event kind as likely), an activate event, a lobby function the user interface thread calls, the keyboard
//! focus event or the boot question's answer (`golden_lobby_form.txt`). The replay runs [`dispatch`] and the
//! functions over a host that keeps the same world and prints what the driver's fakes print for each host call. The
//! driver relinked over `EEScreens.cpp` and this crate prints the same file, and the same on seeds 2-4 at ten times
//! the size.

use std::collections::BTreeMap;
use std::sync::Mutex;

use super::*;
use crate::menus::EE_SCREENS_TEXT_STRING;

const GOLDEN: &[u8] = include_bytes!("../golden_lobby_form.txt");

static STATE: Mutex<LobbyForm> = Mutex::new(LobbyForm::new());

const NAMES: [&[u8]; 6] = [b"Max", "Zo\u{eb}".as_bytes(), b"Computer", b"", b"Bob", b"Long player name"];

fn event_name(event: u32) -> &'static str {
    match event {
        0 => "null",
        EE_SCREENS_LE_CANCEL_LOAD => "class EEUIELECancelLoad",
        EE_SCREENS_LE_LOAD_SAVED_GAME => "class EEUIELELoadSavedGame",
        EE_SCREENS_LE_OPEN_LOAD_SAVED_GAME => "class EEUIELEOpenLoadSavedGame",
        EE_SCREENS_LE_LOAD_SCENARIO => "class EEUIELELoadScenarioGame",
        EE_SCREENS_LE_OPEN_LOAD_SCENARIO => "class EEUIELEOpenLoadScenarioGame",
        EE_SCREENS_LE_CITIZENS => "class EEUIELEChangeCitizenBonus",
        EE_SCREENS_LE_ENGINE => "class EEUIELEChangeEngine",
        EE_SCREENS_LE_CHAT => "class EEUIELEChat",
        EE_SCREENS_LE_CANCEL => "class EEUIELECancel",
        EE_SCREENS_LE_START => "class EEUIELEStart",
        EE_SCREENS_LE_COLOR_UP => "class EEUIELEIncChangeColor",
        EE_SCREENS_LE_COLOR_DOWN => "class EEUIELEDecChangeColor",
        EE_SCREENS_LE_TEAM => "class EEUIELEChangeTeam",
        EE_SCREENS_LE_PLAYER_TYPE => "class EEUIELEChangePlayerType",
        EE_SCREENS_LE_READY => "class EEUIELEChangeReady",
        EE_SCREENS_LE_VICTORY => "class EEUIELEChangeVictory",
        EE_SCREENS_LE_GAME_TYPE => "class EEUIELEGameType",
        EE_SCREENS_LE_MAP_SIZE => "class EEUIELEGameMapSize",
        EE_SCREENS_LE_MAP_TYPE => "class EEUIELEGameMapType",
        EE_SCREENS_LE_RESOURCES => "class EEUIELEGameResources",
        EE_SCREENS_LE_EPOCH_START => "class EEUIELEGameEpochStart",
        EE_SCREENS_LE_EPOCH_END => "class EEUIELEEpochEnd",
        EE_SCREENS_LE_UNIT_LIMIT => "class EEUIELEGameUnitLimit",
        EE_SCREENS_LE_WONDERS => "class EEUIELEGameWondersForVictory",
        EE_SCREENS_LE_VARIANT => "class EEUIELEGameVariant",
        EE_SCREENS_LE_DIFFICULTY => "class EEUIELEGameDifficulty",
        EE_SCREENS_LE_SPEED => "class EEUIELEGameSpeed",
        EE_SCREENS_LE_RECORD_GAMES => "class EEUIELEGameRecordGames",
        EE_SCREENS_LE_REVEAL_MAP => "class EEUIELEGameRevealMap",
        EE_SCREENS_LE_CUSTOM_CIVS => "class EEUIELEGameTeamIslands",
        EE_SCREENS_LE_LOCK_TEAMS => "class EEUIELEGameLockTeams",
        EE_SCREENS_LE_LOCK_SPEED => "class EEUIELEGameLockSpeed",
        EE_SCREENS_LE_CHEAT_CODES => "class EEUIELEGameCheatCodes",
        EE_SCREENS_LE_CHAT_FOCUS => "class EEUIELEChatKeyboardFocus",
        EE_SCREENS_LE_ACTIVATE_SERVER => "class EEUIELEActivateMultiPlayerServer",
        EE_SCREENS_LE_ACTIVATE_CLIENT => "class EEUIELEActivateMultiPlayerClient",
        EE_SCREENS_LE_ACTIVATE_SINGLE => "class EEUIELEActivateSinglePlayer",
        _ => panic!("event {event:#x}"),
    }
}
fn event_of(name: &str) -> u32 {
    (EE_SCREENS_LE_CANCEL_LOAD..=EE_SCREENS_LE_ACTIVATE_SINGLE).find(|&e| event_name(e) == name).unwrap()
}

/// The driver's name hash (`HK`).
fn hk(text: &[u8], key: u32) -> u32 {
    let mut h = 7u32.wrapping_add(key.wrapping_mul(101));
    for &b in text {
        h = h.wrapping_mul(31).wrapping_add(b as u32);
    }
    h >> 3
}

fn s(b: &[u8]) -> String {
    String::from_utf8_lossy(b).into_owned()
}

#[derive(Clone, Default)]
struct Options {
    r: EeScreensLobbyOptionsRecord,
    map: Vec<u8>,
}

/// The loaded header (the driver's LoadFromFile from the file's hash).
struct Header {
    file: Vec<u8>,
    players: u32,
    checksum: u32,
    options: Options,
    slots: Vec<LobbyPlayer>,
}

fn header_of(f: &[u8]) -> Header {
    let mut o = Options::default();
    o.r.map_size = hk(f, 5) % 5;
    o.r.resources = hk(f, 6) % 6;
    o.r.epoch_start = (hk(f, 7) % 6) as c_long;
    o.r.epoch_end = (hk(f, 8) % 6) as c_long;
    o.r.unit_limit = 50 * (hk(f, 9) % 6);
    o.r.wonders = hk(f, 10) % 4;
    o.r.variant = hk(f, 11) % 8;
    o.r.difficulty = hk(f, 12) % 4;
    o.r.speed = hk(f, 13) % 4;
    o.r.record_games = !hk(f, 14).is_multiple_of(2);
    o.r.custom_civs = !hk(f, 15).is_multiple_of(2);
    o.r.reveal_map = !hk(f, 16).is_multiple_of(2);
    o.r.lock_teams = !hk(f, 17).is_multiple_of(2);
    o.r.lock_speed = !hk(f, 18).is_multiple_of(2);
    o.r.cheat_codes = !hk(f, 19).is_multiple_of(2);
    o.r.victory = !hk(f, 20).is_multiple_of(2);
    let slots = (0..16)
        .map(|j| LobbyPlayer {
            r: EeScreensLobbyPlayerRecord {
                kind: hk(f, 30 + j) % 5,
                loaded_type: (hk(f, 50 + j) % 5) as c_long,
                citizens: 1 + hk(f, 90 + j) % 20,
                color: hk(f, 110 + j) % 16,
                team: hk(f, 130 + j) % 5,
                b36: !hk(f, 150 + j).is_multiple_of(2),
                engine: hk(f, 170 + j) % 4,
                slot: j,
                ..Default::default()
            },
            name: NAMES[(hk(f, 70 + j) % 6) as usize].to_vec(),
        })
        .collect();
    Header { file: f.to_vec(), players: 1 + hk(f, 3) % 9, checksum: hk(f, 4) & 0xffff, options: o, slots }
}

fn ping4(p: &LobbyPlayer) -> i32 {
    (p.r.ping * 4.0).round_ties_even() as i32
}
fn player_text(p: &LobbyPlayer) -> String {
    let r = &p.r;
    format!(
        "{} {} '{}' {} {} {} {} {} {} {} {} {} {}",
        r.kind,
        r.loaded_type,
        s(&p.name),
        ping4(p),
        r.slot,
        r.color,
        r.team,
        r.citizens,
        r.transfer,
        r.is_local as i32,
        r.is_ready as i32,
        r.b36 as i32,
        r.engine
    )
}
fn print_player(what: &str, p: &LobbyPlayer) -> String {
    let r = &p.r;
    format!(
        "  {what} type {} loaded {} name '{}' ping {} slot {} color {} team {} citizens {} transfer {} local {} ready {} \
         b36 {} engine {}",
        r.kind,
        r.loaded_type,
        s(&p.name),
        ping4(p),
        r.slot,
        r.color,
        r.team,
        r.citizens,
        r.transfer,
        r.is_local as i32,
        r.is_ready as i32,
        r.b36 as i32,
        r.engine
    )
}
fn flags(o: &EeScreensLobbyOptionsRecord) -> String {
    [o.record_games, o.custom_civs, o.reveal_map, o.lock_teams, o.lock_speed, o.cheat_codes, o.victory]
        .iter()
        .map(|&b| if b { '1' } else { '0' })
        .collect()
}
fn options_text(o: &Options) -> String {
    let r = &o.r;
    format!(
        "{} {} {} {} {} {} {} {} {} {} {} {} '{}'",
        r.load_type,
        r.multiplayer as i32,
        r.map_size,
        r.resources,
        r.epoch_start,
        r.epoch_end,
        r.unit_limit,
        r.wonders,
        r.variant,
        r.difficulty,
        r.speed,
        flags(r),
        s(&o.map)
    )
}
fn print_options(what: &str, o: &Options) -> String {
    let r = &o.r;
    format!(
        "  {what} load {} mp {} size {} res {} epochs {} {} limit {} wonders {} variant {} difficulty {} speed {} \
         flags {} map '{}'",
        r.load_type,
        r.multiplayer as i32,
        r.map_size,
        r.resources,
        r.epoch_start,
        r.epoch_end,
        r.unit_limit,
        r.wonders,
        r.variant,
        r.difficulty,
        r.speed,
        flags(r),
        s(&o.map)
    )
}

/// The driver's world.
#[derive(Default)]
struct World {
    out: Vec<String>,
    players: Vec<LobbyPlayer>,
    options: Options,
    copy: Options,
    server: bool,
    client: bool,
    visible: bool,
    captured: bool,
    stack: Vec<u32>,
    checks: BTreeMap<u32, bool>,
    check_labels: BTreeMap<u32, Option<Vec<u8>>>,
    selections: BTreeMap<u32, u32>,
    lines: BTreeMap<u32, usize>,
    fg: BTreeMap<u32, (i32, i32, i32)>,
    bitmaps: BTreeMap<u32, bool>,
    list: (u32, bool, Vec<Vec<u8>>),
    edit: Vec<u8>,
    names: (Vec<u8>, Vec<u8>),
    triggers: BTreeMap<(u32, u32), u32>,
    open_event: u32,
    keyboard_event: u32,
    question: Option<LobbyPlayer>,
    boot: Option<LobbyPlayer>,
    levels: (i32, i32),
    /// the server's game state, ping, load responses and load state
    server_state: (u32, bool, u32, u32),
    header: Option<Header>,
    last_file: Vec<u8>,
    missing: Vec<u8>,
    rand: u32,
    /// the forms of the events running (the innermost last), the fired control
    forms: Vec<u32>,
    entity: u32,
}

impl World {
    fn line(&mut self, text: String) {
        self.out.push(text);
    }
    fn event_form(&self) -> u32 {
        *self.forms.last().unwrap()
    }
    /// A control as the fakes print it (the load screen's 400 + its index).
    fn ctl(&self, form: u32, control: u32) -> u32 {
        let f = if form == LE_FORM_EVENT { self.event_form() } else { form };
        if f == LE_FORM_LOAD {
            400 + control
        } else {
            control
        }
    }
    fn dirty(&mut self, c: u32) {
        if (30..62).contains(&c) {
            let (r, g, b) = self.fg.get(&c).copied().unwrap_or((0, 0, 0));
            self.line(format!("  dirty {c} 1 fg {r} {g} {b}"));
        } else {
            self.line(format!("  dirty {c} 1"));
        }
    }
    fn set_trigger(&mut self, c: u32, kind: u32, event: u32) {
        self.line(format!("  trigger {c} {kind} {}", event_name(event)));
        if event == 0 {
            self.triggers.remove(&(c, kind));
        } else {
            self.triggers.insert((c, kind), event);
        }
    }
    fn send(&mut self) {
        self.line("  send 255 255".into());
    }
    fn options_of(&self, source: u32) -> &Options {
        match source {
            EE_SCREENS_LE_SOURCE_HEADER => &self.header.as_ref().unwrap().options,
            EE_SCREENS_LE_SOURCE_COPY => &self.copy,
            _ => &self.options,
        }
    }
    /// EEOpenSaveGameFile, the header, the options check (the start's or the load screen's)
    fn load(&mut self, text: &[u8], start: bool, load_type: u32) -> u32 {
        let parts: Vec<&[u8]> = text.split(|&b| b == 0).collect();
        let (file, directory) = (parts[0], parts[1]);
        let found = !hk(file, 0).is_multiple_of(5);
        self.line(format!("  opensave '{}' '{}' -> {}", s(file), s(directory), (!found) as i32));
        self.last_file = file.to_vec();
        if !start {
            self.header = None;
        }
        if !found {
            return EE_SCREENS_LE_LOAD_NOT_FOUND;
        }
        let read = !hk(file, 1).is_multiple_of(4);
        self.line(format!("  header '{}' 12 34 -> {}", s(file), (!read) as i32));
        if !read {
            return EE_SCREENS_LE_LOAD_READ_ERROR;
        }
        let header = header_of(file);
        let ok = !hk(&self.last_file, 2).is_multiple_of(3);
        let current = if start { self.options.r.load_type } else { load_type };
        self.line(format!(
            "  checksaved {} {} load {current} -> {}",
            start as i32,
            header.players as i32 - 1,
            ok as i32
        ));
        if !start {
            self.header = Some(header);
        }
        if ok {
            EE_SCREENS_LE_LOAD_OK
        } else {
            self.missing = b"m1\nm2\n".to_vec();
            EE_SCREENS_LE_LOAD_MISMATCH
        }
    }
    fn rule(&self, which: u32, arg: u32) -> u32 {
        match which {
            EE_SCREENS_LE_RULE_EPOCHS => 6,
            EE_SCREENS_LE_RULE_UNIT_MINIMUM => 50,
            EE_SCREENS_LE_RULE_UNIT_MAXIMUM => 300,
            EE_SCREENS_LE_RULE_UNIT_STEP => 50,
            EE_SCREENS_LE_RULE_WONDERS_MAXIMUM => 3,
            EE_SCREENS_LE_RULE_MAP_SIZES => 5,
            EE_SCREENS_LE_RULE_MAP_SIZE_NAME => 700 + arg,
            EE_SCREENS_LE_RULE_DIFFICULTIES | EE_SCREENS_LE_RULE_SPEEDS => 4,
            EE_SCREENS_LE_RULE_DIFFICULTY_NAME => 800 + arg,
            EE_SCREENS_LE_RULE_SPEED_NAME => 900 + arg,
            EE_SCREENS_LE_RULE_VARIANTS => 8,
            EE_SCREENS_LE_RULE_VARIANT_SELECTABLE => (arg % 3 != 1) as u32,
            EE_SCREENS_LE_RULE_VARIANT_NAME => 400 + arg,
            EE_SCREENS_LE_RULE_RESOURCE_LEVELS => 6,
            EE_SCREENS_LE_RULE_RESOURCE_LEVEL_NAME | EE_SCREENS_LE_RULE_STARTING_RESOURCES_NAME => 500 + arg,
            EE_SCREENS_LE_RULE_EPOCH_TITLE => 600u32.wrapping_add(arg),
            EE_SCREENS_LE_RULE_GENERATORS | EE_SCREENS_LE_RULE_ENGINES => 4,
            EE_SCREENS_LE_RULE_DEFAULT_COLOUR => [0, 1, 4, 9, 2, 15, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15][arg as usize],
            EE_SCREENS_LE_RULE_STARTING_CITIZENS => 3,
            _ => panic!("rule {which}"),
        }
    }
    fn set_option(&mut self, option: u32, v: u32) {
        let o = &mut self.options.r;
        match option {
            0 => o.load_type = v,
            1 => o.map_size = v,
            3 => o.resources = v,
            4 => o.epoch_start = v as i32 as c_long,
            5 => o.epoch_end = v as i32 as c_long,
            6 => o.unit_limit = v,
            7 => o.wonders = v,
            8 => o.variant = v,
            9 => o.difficulty = v,
            10 => o.speed = v,
            11 => o.record_games = v != 0,
            12 => o.custom_civs = v != 0,
            13 => o.reveal_map = v != 0,
            14 => o.lock_teams = v != 0,
            15 => o.lock_speed = v != 0,
            16 => o.cheat_codes = v != 0,
            17 => o.victory = v != 0,
            _ => panic!("option {option}"),
        }
    }
}

impl LobbyHost for World {
    fn call(&mut self, form: u32, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        match op {
            EE_SCREENS_OP_SHOW => {
                let x = self.ctl(form, a);
                self.line(format!("  mode {x} {b}"));
            }
            EE_SCREENS_OP_COMBO_SELECT => {
                self.line(format!("  sel {a} {}", b as i32));
                self.selections.insert(a, b);
            }
            EE_SCREENS_OP_SET_CHECKBOX => {
                self.line(format!("  set {a} {}", (b != 0) as i32));
                self.checks.insert(a, b != 0);
            }
            EE_SCREENS_OP_CHECKBOX => return self.checks.get(&a).copied().unwrap_or(false) as u32,
            EE_SCREENS_OP_COMBO_CLEAR => {
                self.line(format!("  deleteall {a}"));
                self.lines.insert(a, 0);
            }
            EE_SCREENS_OP_LE_COMBO_DELETE_LINE => {
                self.line(format!("  deleteline {a} {b}"));
                let n = self.lines.entry(a).or_default();
                if (b as usize) < *n {
                    *n -= 1;
                }
            }
            EE_SCREENS_OP_LE_COMBO_LINES => return self.lines.get(&a).copied().unwrap_or(0) as u32,
            EE_SCREENS_OP_TRIGGER => {
                assert_eq!((form, a), (LE_FORM_LOBBY, LE_FORM_LOBBY));
                self.set_trigger(b, c, d);
            }
            EE_SCREENS_OP_TRIGGER_HERE => {
                let x = self.ctl(form, a);
                self.set_trigger(x, b, c);
            }
            EE_SCREENS_OP_LE_TYPE_SELECT => {
                let event = self.triggers.get(&(a, 0x3eb)).copied();
                if event.is_some() {
                    self.set_trigger(a, 0x3eb, 0);
                }
                if b != 0 {
                    self.line(format!("  sel {a} {}", c as i32));
                    self.selections.insert(a, c);
                }
                if let Some(e) = event {
                    self.set_trigger(a, 0x3eb, e);
                }
            }
            EE_SCREENS_OP_LE_HAS_TRIGGER => return self.triggers.contains_key(&(a, b)) as u32,
            EE_SCREENS_OP_LE_PLAYER_COLOUR => {
                let x = self.ctl(form, a);
                self.line(format!("  bgcolor {x} {}", b.min(16)));
            }
            EE_SCREENS_OP_LE_OPACITY => self.line(format!("  opacity {a} 100")),
            EE_SCREENS_OP_LE_BITMAP => {
                self.bitmaps.insert(a, b != 0);
                self.dirty(a);
            }
            EE_SCREENS_OP_LE_NAME_COLOUR => {
                let colour = [(10, 0, 0), (10, 10, 0), (10, 10, 10), (0, 10, 0)][(b & 3) as usize];
                self.fg.insert(a, colour);
            }
            EE_SCREENS_OP_LE_DIRTY => self.dirty(a),
            EE_SCREENS_OP_LE_PING => {
                let ping = f32::from_bits(b);
                self.line(format!("  ping {a} {}", (ping * 4.0).round_ties_even() as i32));
            }
            EE_SCREENS_OP_LE_FORM_VISIBLE => {
                assert_eq!(a, LE_FORM_LOBBY);
                return self.visible as u32;
            }
            EE_SCREENS_OP_CURRENT_FORM_IS => return (self.stack.last() == Some(&a)) as u32,
            EE_SCREENS_OP_GROUP => {
                assert_eq!(a, 1);
                self.line(format!("  group{} 1", if b != 0 { "enable" } else { "disable" }));
            }
            EE_SCREENS_OP_BACKGROUND => {
                let name = if a == 0x89 { "0x89" } else { "0xa2" };
                self.line(format!("  visibility {name} {}", (b != 0) as i32));
            }
            EE_SCREENS_OP_TAB_STOP => {
                let f = self.event_form();
                self.line(format!("  tabstop {f} {a}"));
            }
            EE_SCREENS_OP_LE_KEYBOARD_EVENT => {
                self.line(format!("  keyboardevent 26 {}", event_name(a)));
                self.keyboard_event = a;
            }
            EE_SCREENS_OP_LE_KEYBOARD_CAPTURED => return self.captured as u32,
            EE_SCREENS_OP_LE_BLINKING => self.line(format!("  blinking {a} 1")),
            EE_SCREENS_OP_LE_EDIT_DELETE => self.line(format!("  deletetext {a}")),
            EE_SCREENS_OP_OPEN_EVENT => {
                assert_eq!(a, LE_FORM_LOAD);
                self.line(format!("  openevent 4 {}", event_name(b)));
                self.open_event = b;
            }
            EE_SCREENS_OP_PUSH => {
                self.line(format!("  push {a}"));
                self.stack.push(a);
                if a == LE_FORM_LOAD && self.open_event != 0 {
                    self.forms.push(LE_FORM_LOAD);
                    let e = self.open_event;
                    assert!(dispatch(self, &STATE, e));
                    self.forms.pop();
                }
            }
            EE_SCREENS_OP_POP => {
                self.line("  pop".into());
                self.stack.pop();
            }
            EE_SCREENS_OP_CLOSE_FORM => self.line("  closeform 999".into()),
            EE_SCREENS_OP_ENTITY_INDEX => return self.entity,
            EE_SCREENS_GAME_IS_SERVER => return self.server as u32,
            EE_SCREENS_GAME_LE_IS_CLIENT => return self.client as u32,
            EE_SCREENS_GAME_LE_LOCK => {
                let kind = ["readlock", "readunlock", "writelock", "writeunlock"][a as usize];
                self.line(format!("  {kind}"));
            }
            EE_SCREENS_GAME_LE_PLAYER_INFO => {
                self.line(format!("  playerinfo {a} {b} {} 0", c as i32));
                self.send();
            }
            EE_SCREENS_GAME_LE_SEND_OPTION => {
                self.line(format!("  gameoption {} {} ''", a as i32, b as i32));
                self.send();
            }
            EE_SCREENS_GAME_LE_SET_OPTION => self.set_option(a, b),
            EE_SCREENS_GAME_LE_DIFFICULTY_LEVELS => self.levels = (a as i32, a as i32),
            EE_SCREENS_GAME_LE_DISCONNECT => self.line("  send 36 255 disconnect 0".into()),
            EE_SCREENS_GAME_LE_REMOVE_HOST => self.line("  postaction remove 254".into()),
            EE_SCREENS_GAME_LE_WON_REJOIN => return 1,
            EE_SCREENS_GAME_LE_WON_LEAVE => {}
            EE_SCREENS_GAME_LE_START_SERVER => {
                self.line("  allownew 0".into());
                self.line("  resetxfer".into());
                self.line("  allownew 0".into());
                self.server_state = (1, false, 0, 1);
            }
            EE_SCREENS_GAME_LE_START_LOADING => self.line(format!("  send 93 255 startloading {a} {}", b as i32)),
            EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS => {
                let o = &self.options;
                let text = format!("  lastoptions {} {} {}", o.r.load_type, o.r.map_size, s(&o.map));
                self.line(text);
            }
            EE_SCREENS_GAME_LE_START_PING => self.server_state.1 = true,
            EE_SCREENS_GAME_LE_FULL_PLAYER => {
                let text = print_player("fullplayer", &self.players[a as usize]);
                self.line(text);
                self.send();
            }
            EE_SCREENS_GAME_LE_RULE => return self.rule(a, b),
            EE_SCREENS_GAME_LE_FIND_GENERATOR => {
                return match self.options_of(a).map.as_slice() {
                    [b'M', d @ b'0'..=b'3'] => (d - b'0') as u32,
                    _ => 77 | 0x8000_0000,
                };
            }
            EE_SCREENS_GAME_LE_BOOT_FREE => self.boot = None,
            EE_SCREENS_GAME_LE_BOOT_REFUSE => {
                let text = print_player("refuse", self.boot.as_ref().unwrap());
                self.line(text);
            }
            EE_SCREENS_GAME_LE_XFER => {
                self.line(format!("  abortxfer {b}"));
                let h = self.header.as_ref().unwrap();
                let text = format!("  xfer '{}' {} {} 0", s(&h.file), h.checksum, a + 20);
                self.line(text);
                return 100 + a + 20;
            }
            EE_SCREENS_GAME_LE_HEADER_PLAYERS => return self.header.as_ref().unwrap().players,
            EE_SCREENS_GAME_LE_APPLY_SAVED => {
                let h = self.header.as_ref().unwrap().options.r;
                let o = &mut self.options.r;
                (o.map_size, o.resources, o.epoch_start, o.epoch_end) =
                    (h.map_size, h.resources, h.epoch_start, h.epoch_end);
                (o.unit_limit, o.wonders, o.variant, o.difficulty, o.speed) =
                    (h.unit_limit, h.wonders, h.variant, h.difficulty, h.speed);
                (o.record_games, o.custom_civs, o.reveal_map) = (h.record_games, h.custom_civs, h.reveal_map);
                (o.lock_teams, o.lock_speed, o.cheat_codes, o.victory) =
                    (h.lock_teams, h.lock_speed, h.cheat_codes, h.victory);
                let text = print_options("fulloptions", &self.options);
                self.line(text);
                self.send();
            }
            EE_SCREENS_GAME_LE_RANDOM_MAP_OPTIONS => {
                self.copy = self.options.clone();
                self.copy.r.load_type = 0;
                self.options = self.copy.clone();
            }
            EE_SCREENS_OP_SRAND_TIME => self.rand = 1234,
            EE_SCREENS_OP_RAND => {
                // the MSVC CRT's rand
                self.rand = self.rand.wrapping_mul(214013).wrapping_add(2531011);
                return (self.rand >> 16) & 0x7fff;
            }
            EE_SCREENS_OP_FORM_LIST_SELECTED => {
                assert_eq!((a, b), (LE_FORM_LOAD + 1, LE_LOAD_LIST));
                let (selection, has, lines) = &self.list;
                return if !lines.is_empty() && *has { *selection } else { u32::MAX };
            }
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn text(&mut self, form: u32, which: u32, arg: u32) -> Vec<u8> {
        self.text_checked(form, which, arg).unwrap_or_default()
    }
    fn text_checked(&mut self, _form: u32, which: u32, arg: u32) -> Option<Vec<u8>> {
        Some(match which {
            EE_SCREENS_TEXT_STRING => format!("S{arg}").into_bytes(),
            EE_SCREENS_TEXT_SAVED_EXTENSION => b".ees".to_vec(),
            EE_SCREENS_TEXT_SCENARIO_EXTENSION => b".scn".to_vec(),
            EE_SCREENS_TEXT_LOAD_SAVED_NAME => self.names.0.clone(),
            EE_SCREENS_TEXT_LOAD_SCENARIO_NAME => self.names.1.clone(),
            EE_SCREENS_TEXT_EDIT => {
                assert_eq!(arg, LE_CHAT_BOX);
                self.edit.clone()
            }
            EE_SCREENS_TEXT_LIST_DATA => {
                assert_eq!(arg >> 16, LE_LOAD_LIST);
                self.list.2[(arg & 0xffff) as usize].clone()
            }
            EE_SCREENS_TEXT_LE_MAP_TYPE => self.options_of(arg).map.clone(),
            EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY => format!("DM{arg}").into_bytes(),
            EE_SCREENS_TEXT_LE_MAP_DISPLAY => [b"D", &self.options_of(arg).map[..]].concat(),
            EE_SCREENS_TEXT_LE_GENERATOR_NAME => format!("M{}", arg % 4).into_bytes(),
            EE_SCREENS_TEXT_LE_ENGINE => match arg {
                2 => b"AI one".to_vec(),
                3 => "Zo\u{eb} AI".as_bytes().to_vec(),
                1 => b"S33171".to_vec(),
                _ => b"S33170".to_vec(),
            },
            EE_SCREENS_TEXT_LE_MISSING => self.missing.clone(),
            EE_SCREENS_TEXT_LE_CHECK_LABEL => return self.check_labels.get(&arg).cloned().flatten(),
            _ => panic!("text {which:#x}"),
        })
    }
    fn put_text(&mut self, form: u32, which: u32, arg: u32, text: &[u8]) -> u32 {
        match which {
            EE_SCREENS_TEXT_LABEL => {
                let x = self.ctl(form, arg);
                self.line(format!("  text {x} '{}' 1", s(text)));
            }
            EE_SCREENS_TEXT_COMBO_APPEND => {
                self.line(format!("  append {arg} '{}'", s(text)));
                *self.lines.entry(arg).or_default() += 1;
            }
            EE_SCREENS_TEXT_COMBO_INSERT => {
                self.line(format!("  insertline {} '{}' {}", arg >> 16, s(text), arg & 0xffff));
                *self.lines.entry(arg >> 16).or_default() += 1;
            }
            EE_SCREENS_TEXT_MESSAGE => {
                let t = s(text).replace('\n', "|");
                self.line(format!("  display '{t}' 1 999 0 1"));
            }
            EE_SCREENS_TEXT_LE_MAP_TYPE => self.options.map = text.to_vec(),
            EE_SCREENS_TEXT_LE_SEND_MAP_TYPE => {
                self.line(format!("  gameoption 2 {arg} '{}'", s(text)));
                self.send();
            }
            EE_SCREENS_TEXT_LE_CHAT => self.line(format!("  send 90 255 chat 0 '{}'", s(text))),
            EE_SCREENS_TEXT_LE_CHECK_LOAD => return self.load(text, true, 0),
            EE_SCREENS_TEXT_LE_OPEN_HEADER => return self.load(text, false, arg),
            EE_SCREENS_TEXT_LE_BOOT_QUESTION => {
                let p = self.boot.clone().unwrap();
                let line =
                    format!("  question '{}' 1 slot {} type {} name '{}' 1 1", s(text), p.r.slot, p.r.kind, s(&p.name));
                self.line(line);
                self.question = Some(p);
            }
            EE_SCREENS_TEXT_LOAD_SAVED_NAME => self.names.0 = text.to_vec(),
            EE_SCREENS_TEXT_LOAD_SCENARIO_NAME => self.names.1 = text.to_vec(),
            EE_SCREENS_TEXT_FILE_LIST => {
                assert_eq!(arg, LE_FORM_LOAD << 16 | LE_LOAD_LIST);
                let parts: Vec<&[u8]> = text.split(|&b| b == 0).collect();
                self.line("  volume 'C:'".into());
                self.line(format!("  directory '\\Game\\{}'", s(parts[1])));
                self.line("  adddirectory 1".into());
                self.line("  searchpaths 404 1".into());
                self.line("  createentries 404 1".into());
                if let Some(i) = self.list.2.iter().position(|l| l == parts[2]) {
                    self.line(format!("  listselect 404 {i} 1 0"));
                }
            }
            _ => panic!("put {which:#x}"),
        }
        0
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        format!("S{id}").into_bytes()
    }
    fn selection(&mut self, form: u32, control: u32) -> (bool, u32) {
        assert_ne!(form, LE_FORM_LOAD);
        let v = self.selections.get(&control).copied().unwrap_or(u32::MAX);
        (v != u32::MAX, v)
    }
    fn player(&mut self, source: u32, index: u32) -> LobbyPlayer {
        match source {
            EE_SCREENS_LE_SOURCE_BOOT => self.boot.clone().unwrap(),
            EE_SCREENS_LE_SOURCE_HEADER => self.header.as_ref().unwrap().slots[index as usize].clone(),
            _ => self.players[index as usize].clone(),
        }
    }
    fn put_player(&mut self, source: u32, index: u32, player: &LobbyPlayer) {
        match source {
            EE_SCREENS_LE_SOURCE_BOOT => self.boot = Some(player.clone()),
            _ => self.players[index as usize] = player.clone(),
        }
    }
    fn options(&mut self, source: u32) -> EeScreensLobbyOptionsRecord {
        self.options_of(source).r
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

fn parse_player(w: &[String]) -> LobbyPlayer {
    let n = |i: usize| w[i].parse::<i64>().unwrap();
    LobbyPlayer {
        r: EeScreensLobbyPlayerRecord {
            kind: n(0) as c_ulong,
            loaded_type: n(1) as c_long,
            ping: n(3) as f32 / 4.0,
            slot: n(4) as c_ulong,
            color: n(5) as c_ulong,
            team: n(6) as c_ulong,
            citizens: n(7) as c_ulong,
            transfer: n(8) as c_ulong,
            is_local: n(9) != 0,
            is_ready: n(10) != 0,
            b36: n(11) != 0,
            engine: n(12) as c_ulong,
        },
        name: w[2].as_bytes().to_vec(),
    }
}

fn parse_options(w: &[String], into: &mut Options) {
    let n = |i: usize| w[i].parse::<i64>().unwrap();
    let f: Vec<bool> = w[11].bytes().map(|b| b == b'1').collect();
    let r = &mut into.r;
    r.load_type = n(0) as c_ulong;
    r.multiplayer = n(1) != 0;
    r.map_size = n(2) as c_ulong;
    r.resources = n(3) as c_ulong;
    r.epoch_start = n(4) as c_long;
    r.epoch_end = n(5) as c_long;
    r.unit_limit = n(6) as c_ulong;
    r.wonders = n(7) as c_ulong;
    r.variant = n(8) as c_ulong;
    r.difficulty = n(9) as c_ulong;
    r.speed = n(10) as c_ulong;
    (r.record_games, r.custom_civs, r.reveal_map, r.lock_teams, r.lock_speed, r.cheat_codes, r.victory) =
        (f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
    into.map = w[12].as_bytes().to_vec();
}

/// A case's perturbation applied to the world.
fn perturb(w: &mut World, n: usize, line: &str) {
    let v = words(line);
    match v[0].as_str() {
        "options" => parse_options(&v[1..], &mut w.options),
        "world" => {
            (w.server, w.client, w.visible, w.captured) = (v[1] == "1", v[2] == "1", v[3] == "1", v[4] == "1");
            w.stack = v[6..].iter().map(|x| x.parse().unwrap()).collect();
        }
        "check" => {
            let c: u32 = v[1].parse().unwrap();
            w.checks.insert(c, v[2] == "1");
            if v.len() > 3 {
                w.check_labels.insert(c, (v[3] != "-").then(|| v[3].as_bytes().to_vec()));
            }
        }
        "combo" => {
            let c: u32 = v[1].parse().unwrap();
            w.selections.insert(c, v[2].parse::<i64>().unwrap() as u32);
            w.lines.insert(c, v[3].parse().unwrap());
        }
        "list" => w.list = (v[1].parse().unwrap(), v[2] == "1", v[3..].iter().map(|x| x.as_bytes().to_vec()).collect()),
        "edit" => w.edit = v.get(1).map(|x| x.as_bytes().to_vec()).unwrap_or_default(),
        "names" => w.names = (v[1].as_bytes().to_vec(), v[2].as_bytes().to_vec()),
        p if p.starts_with('p') => w.players[p[1..].parse::<usize>().unwrap()] = parse_player(&v[1..]),
        other => panic!("case {n}: {other}"),
    }
}

/// The case's action.
fn act(w: &mut World, n: usize, action: &[String]) {
    w.forms = vec![LE_FORM_LOBBY];
    match action[0].as_str() {
        "fire" => {
            let (c, t): (u32, u32) = (action[1].parse().unwrap(), action[2].parse().unwrap());
            let event = w.triggers[&(c, t)];
            assert_eq!(event_name(event), action[3..].join(" "), "case {n}");
            if c >= 400 {
                w.forms = vec![LE_FORM_LOAD];
                w.entity = 0;
            } else {
                w.entity = c;
            }
            assert!(dispatch(w, &STATE, event));
        }
        "activate" => assert!(dispatch(w, &STATE, event_of(&action[1..].join(" ")))),
        "updateplayer" => update_player(w, &STATE, action[1].parse().unwrap(), action[2].parse().unwrap()),
        "updateoptions" => update_game_options(w),
        "updateform" => update_form(w, &STATE),
        "transfer" => show_transfer_progress(w, action[1].parse().unwrap(), action[2].parse().unwrap()),
        "focus" => {
            assert_eq!(event_name(w.keyboard_event), action[1..].join(" "));
            let e = w.keyboard_event;
            assert!(dispatch(w, &STATE, e));
        }
        "answer" => {
            w.boot = w.question.take();
            let event = if action[1] == "yes" { EE_SCREENS_LE_BOOT_YES } else { EE_SCREENS_LE_BOOT_NO };
            assert!(dispatch(w, &STATE, event));
        }
        other => panic!("case {n}: action {other}"),
    }
}

#[test]
fn golden_lobby_form() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let mut w = World { players: vec![LobbyPlayer::default(); 16], ..Default::default() };
    w.names = (b"Game 1".to_vec(), b"My Scenario".to_vec());
    let mut kinds = BTreeMap::<String, usize>::new();
    for (n, case) in cases.iter().enumerate() {
        let mut k = 1;
        while case[k].starts_with("  ") {
            perturb(&mut w, n, case[k]);
            k += 1;
        }
        let before: Vec<String> = w.players.iter().map(player_text).collect();
        let before_options = options_text(&w.options);
        let action = words(case[k]);
        let kind = if action[0] == "fire" { action[3..].join(" ") } else { action[0].clone() };
        *kinds.entry(kind).or_default() += 1;
        w.out.clear();
        act(&mut w, n, &action);
        for (i, p) in w.players.clone().iter().enumerate() {
            let t = player_text(p);
            if t != before[i] {
                w.line(format!("  a{i} {t}"));
            }
        }
        let t = options_text(&w.options);
        if t != before_options {
            w.line(format!("  after {t}"));
        }
        let bitmaps: String = (226..=248)
            .map(|i| match w.bitmaps.get(&i) {
                Some(true) => '1',
                Some(false) => '0',
                None => '-',
            })
            .collect();
        let ss = w.server_state;
        let line = format!(
            "  server {} {} {} {} levels {} {} forms {} names '{}' '{}' bitmaps {bitmaps}",
            ss.0,
            ss.1 as i32,
            ss.2,
            ss.3,
            w.levels.0,
            w.levels.1,
            w.stack.len(),
            s(&w.names.0),
            s(&w.names.1)
        );
        w.line(line);
        let expected = &case[k + 1..];
        for (i, e) in expected.iter().enumerate() {
            assert!(i < w.out.len(), "case {n} ({}): missing line {i}: {e}", case[k]);
            assert_eq!(w.out[i], *e, "case {n} ({}) line {i}", case[k]);
        }
        assert_eq!(w.out.len(), expected.len(), "case {n}: extra lines");
    }
    // the 31 control event kinds fired (the open and focus events run otherwise), and the seven other actions ran
    assert_eq!(kinds.len(), 38, "{kinds:?}");
}
