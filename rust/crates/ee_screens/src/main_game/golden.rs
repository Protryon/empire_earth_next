//! The main game form's events against the real C++: a scratch driver (`build/sc8/main_game_driver.cpp`) ran the
//! unmodified `EEUIEMainGameEvents.cpp` over fake forms (the main game form's controls, groups, backgrounds and button
//! bitmaps), the manager, the user interface thread, the communications (server and client), the physics, the world
//! (ghosts, units, camera, game speed), the players' selection, the preferences and the files: one session of 2,000
//! cases, each drawing an event (all 38 of the form's), perturbing the state (printed), firing the event and printing
//! every call and the state after (`golden_main_game.txt`). The replay runs [`dispatch`] over a host that keeps the
//! same world and prints what the driver's fakes print for each host call. The driver relinked over `EEScreens.cpp`
//! and this crate prints the same file, and the same on seeds 2-4 at 25,000 cases.

use super::*;
use crate::lobby_form::EE_SCREENS_GAME_LE_IS_CLIENT;
use crate::main_game_options::EE_SCREENS_GAME_GO_USER_PAUSE;

const GOLDEN: &[u8] = include_bytes!("../golden_main_game.txt");

/// The driver's world.
#[derive(Default)]
struct World {
    out: Vec<String>,
    ui: i64,
    ghosts: u32,
    pointer: u32,
    lock: u32,
    refresh: u32,
    clock: i64,
    hot_keys: u32,
    group: u32,
    fopen: i64,
    open_save: i64,
    dialog: i64,
    civ: i64,
    editor_result: i64,
    local: i64,
    player: bool,
    slot: u32,
    server: bool,
    client: bool,
    cts: u32,
    multiplayer: bool,
    editor: bool,
    time: i64,
    cinematic: bool,
    message: (bool, u32),
    lock_speed: bool,
    speed: [u32; 4],
    hidden: i64,
    cam: i64,
    zoom: i64,
    lead: u32,
    selection: Vec<i64>,
    commands: Option<Vec<u32>>,
    tod: [u32; 6],
    quick: String,
    auto: String,
    bitmaps: [(i64, i64); 9],
    /// The world's units (5 has no complex unit): whether each can hide, its hidden flags.
    can_hide: [bool; 6],
    flags: [u32; 6],
}

impl World {
    fn line(&mut self, text: String) {
        self.out.push(text);
    }
    fn local(&mut self, kind: u32, value: String) -> u32 {
        self.line(format!("  local {kind} 255 {value} -> {}", self.local));
        self.local as u32
    }
    fn file_spec(&mut self) {
        self.line("  filespec".into());
    }
}

impl MainGameHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        match op {
            EE_SCREENS_GAME_MG_UI_STATE => return self.ui as u32,
            EE_SCREENS_GAME_MG_GHOSTS => return self.ghosts,
            EE_SCREENS_GAME_MG_CLEAR_GHOSTS => {
                self.line("  lock enter".into());
                self.line("  clearghosts".into());
                self.line("  lock leave".into());
                self.ghosts = 0;
            }
            EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS => self.line("  gateclear".into()),
            EE_SCREENS_GAME_MG_SET_UI_STATE => {
                self.ui = a as i32 as i64;
                self.line(format!("  uistate {}", self.ui));
            }
            EE_SCREENS_GAME_MG_POINTER => self.pointer = a,
            EE_SCREENS_GAME_MG_CURSOR_LOCK => self.lock = a,
            EE_SCREENS_GAME_MG_REFRESH_GROUP => self.refresh = 1,
            EE_SCREENS_GAME_MG_DUMP_MEDIA => {
                self.file_spec();
                self.line("  filevolume 'V:'".into());
                self.line("  filedir '\\D\\'".into());
                self.line("  filename 'Media Dump.txt'".into());
                if a != 0 {
                    self.line("  filedelete".into());
                }
                self.line("  filecreate".into());
                self.line(format!("  fopen a+ -> {}", self.fopen));
                if self.fopen == 0 {
                    self.line("  chatlog 'Dumped Media Information.' 0".into());
                    self.line(format!("  dumpmedia {b}"));
                    self.line("  fileclose".into());
                }
                self.line("  ~filespec".into());
                return self.fopen as u32;
            }
            EE_SCREENS_GAME_MG_SHOW_DIALOG => {
                self.line(format!("  showdialog -> {}", self.dialog));
                return self.dialog as u32;
            }
            EE_SCREENS_GAME_MG_CURRENT_GROUP => self.group = a,
            EE_SCREENS_GAME_MG_ENTITY_GROUP => self.line(format!("  entitygroup {a}")),
            EE_SCREENS_GAME_MG_REFRESH_FORM => self.line("  refreshform".into()),
            EE_SCREENS_GAME_MG_CIV_BUILDER => {
                self.line(format!("  postcivbuilder -> {}", self.civ));
                return self.civ as u32;
            }
            EE_SCREENS_GAME_MG_HAS_PLAYER => return self.player as u32,
            EE_SCREENS_GAME_MG_SHIFT => {
                self.local(if a != 0 { 136 } else { 137 }, self.time.to_string());
            }
            EE_SCREENS_GAME_MG_AIRPORT_SELECTED => return self.selection.contains(&66) as u32,
            EE_SCREENS_GAME_MG_RALLY => {
                self.local(142, a.to_string());
            }
            EE_SCREENS_GAME_MG_HOT_KEY_MODE => return self.hot_keys,
            EE_SCREENS_GAME_MG_TO_EDITOR => {
                self.call(EE_SCREENS_GAME_MG_CLEAR_GHOSTS, 0, 0);
                self.line("  getprotocol 0".into());
                self.line("  localaddress 3".into());
                self.line(format!("  moveplayer 254 {} 1 '10.0.0.7' 0", self.slot));
                self.line("  threadpost 0x402 1".into());
                self.line(format!("  posteditor {} -> {}", self.editor as i32, self.editor_result));
                self.line("  postpause 1".into());
                return self.editor_result as u32;
            }
            EE_SCREENS_GAME_MG_QUICK_SAVE => {
                let t = self.tod;
                let name = format!(
                    "QuickSave {:4}.{:02}.{:02} - {:02}h.{:02}m.{:02}s.ees",
                    t[0],
                    t[1] + 1,
                    t[2],
                    t[3],
                    t[4],
                    t[5]
                );
                self.line(format!("  prefssave '{name}'"));
                self.line(format!("  loadorsave '{name}' 4242 0 1 {} 0", self.cts));
                self.line("  sendall 15000 12".into());
                self.quick = name;
            }
            EE_SCREENS_GAME_MG_DIALOG_SHOWN => {
                return ((self.message.0 && self.message.1 == 1) || self.cinematic) as u32;
            }
            EE_SCREENS_GAME_MG_CLOCK => return self.clock as u32,
            EE_SCREENS_GAME_MG_SET_CLOCK => self.clock = a as i32 as i64,
            EE_SCREENS_GAME_MG_KILL => return self.local(92, a.to_string()),
            EE_SCREENS_GAME_MG_ATTACK => return self.local(112, format!("2 {a}")),
            EE_SCREENS_GAME_MG_HIDDEN_MODE => return self.hidden as u32,
            EE_SCREENS_GAME_MG_SET_HIDDEN_MODE => self.hidden = a as i64,
            EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED => {
                for u in 0..5 {
                    if self.can_hide[u] {
                        self.flags[u] |= 0x10;
                    }
                }
            }
            EE_SCREENS_GAME_MG_FOLLOW_LEAD => {
                assert_eq!(a, 0);
                self.line("  leadunit 1".into());
                self.line(format!("  getunit {}", self.lead));
                let unit = if self.lead < 8 { self.lead as i64 } else { -1 };
                self.line(format!("  follow {unit} 0 1 0 0"));
                self.line("  camchanged".into());
            }
            EE_SCREENS_GAME_MG_CAMERA_MODE => return self.cam as u32,
            EE_SCREENS_GAME_MG_SET_CAMERA_MODE => {
                self.cam = a as i64;
                self.line(format!("  cammode {a}"));
            }
            EE_SCREENS_GAME_MG_ZOOM_STYLE => self.zoom = self.cam,
            EE_SCREENS_GAME_MG_LOCK_SPEED => return self.lock_speed as u32,
            EE_SCREENS_GAME_MG_GAME_SPEED => return self.speed[0],
            EE_SCREENS_GAME_MG_SPEED_TABLE => return self.speed[1 + a as usize],
            EE_SCREENS_GAME_MG_SEND_SPEED => {
                self.local(95, format!("{a:08x}"));
            }
            EE_SCREENS_GAME_MG_SELECTED_COMMAND => return self.commands.as_ref().map_or(0, |c| c[a as usize]),
            EE_SCREENS_GAME_MG_BEHAVIOR => {
                self.local(158, a.to_string());
            }
            EE_SCREENS_GAME_MG_CLOSE_BEHAVIORS => {
                self.line("  groupdisable 9".into());
                self.line("  background 11 151".into());
                self.line("  visibility 0".into());
            }
            EE_SCREENS_GAME_MG_COPY_BITMAPS => {
                let n = match b {
                    232 => 0, // scout
                    228 => 1, // stand ground
                    233 => 2, // guard
                    _ => 3,   // aggressive (229)
                };
                self.line(format!("  dirty {a} 1"));
                self.line(format!("  dirty {a} 1"));
                self.bitmaps[(a - 0x4e) as usize] = (n, n + 4);
            }
            EE_SCREENS_OP_SHOW => self.line(format!("  mode {a} {b}")),
            EE_SCREENS_OP_OPEN_EVENT => self.line(format!("  openevent {a} screen {b}")),
            EE_SCREENS_OP_SWITCH => self.line(format!("  switch {a}")),
            EE_SCREENS_GAME_GO_USER_PAUSE => {
                assert_eq!(a, 1);
                self.line("  postaction class EEAUserPauseGame".into());
            }
            EE_SCREENS_GAME_IS_SERVER => return self.server as u32,
            EE_SCREENS_GAME_LE_IS_CLIENT => return self.client as u32,
            EE_SCREENS_GAME_CTS_PROCESSED => return self.cts,
            EE_SCREENS_GAME_MULTIPLAYER_SESSION => return self.multiplayer as u32,
            EE_SCREENS_GAME_IN_EDITOR => return self.editor as u32,
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        let t = String::from_utf8_lossy(text).into_owned();
        match which {
            EE_SCREENS_TEXT_MG_CHAT_LOG => self.line(format!("  chatlog '{t}' 0")),
            EE_SCREENS_TEXT_MG_QUICK_LOAD => {
                let name = if arg != 0 { self.auto.clone() } else { self.quick.clone() };
                self.file_spec();
                self.line(format!("  opensave '{name}' 'Saved Games\\' 1 -> {}", self.open_save));
                if self.open_save == 0 {
                    self.line(format!("  filename '{name}'"));
                    self.line(format!("  postload '{name}'"));
                } else {
                    self.line(format!("  display '{t}' 1 999 0 1"));
                }
                self.line("  ~filespec".into());
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

/// The event of a driver's kind name.
fn event_of(name: &str) -> u32 {
    match name {
        "cancel_ui_mode" => EE_SCREENS_MG_CANCEL_UI_MODE,
        "dump_media" => EE_SCREENS_MG_DUMP_MEDIA,
        "dump_media_all" => EE_SCREENS_MG_DUMP_MEDIA_ALL,
        "dump_media_editor" => EE_SCREENS_MG_DUMP_MEDIA_EDITOR,
        "dump_media_editor_all" => EE_SCREENS_MG_DUMP_MEDIA_EDITOR_ALL,
        "end_scenario_dialog" => EE_SCREENS_MG_END_SCENARIO_DIALOG,
        "statistics" => EE_SCREENS_MG_DISPLAY_STATISTICS,
        "civ_builder" => EE_SCREENS_MG_DISPLAY_CIV_BUILDER,
        "chat" => EE_SCREENS_MG_DISPLAY_CHAT,
        "diplomacy" => EE_SCREENS_MG_DISPLAY_DIPLOMACY,
        "options" => EE_SCREENS_MG_DISPLAY_OPTIONS,
        "enter_shift" => EE_SCREENS_MG_ENTER_SHIFT,
        "exit_shift" => EE_SCREENS_MG_EXIT_SHIFT,
        "flare" => EE_SCREENS_MG_FLARE,
        "rally_fighter" => EE_SCREENS_MG_RALLY_FIGHTER,
        "rally_bomber" => EE_SCREENS_MG_RALLY_BOMBER,
        "rally_atomic" => EE_SCREENS_MG_RALLY_ATOMIC,
        "enter_editor" => EE_SCREENS_MG_ENTER_EDITOR,
        "quick_save" => EE_SCREENS_MG_QUICK_SAVE,
        "quick_load" => EE_SCREENS_MG_QUICK_LOAD,
        "auto_load" => EE_SCREENS_MG_AUTO_LOAD,
        "pause" => EE_SCREENS_MG_PAUSE,
        "cancel_update_state" => EE_SCREENS_MG_CANCEL_UPDATE_STATE,
        "clock" => EE_SCREENS_MG_TOGGLE_CLOCK,
        "kill" => EE_SCREENS_MG_KILL,
        "kill_all" => EE_SCREENS_MG_KILL_ALL,
        "attack" => EE_SCREENS_MG_ATTACK,
        "attack_allies" => EE_SCREENS_MG_ATTACK_ALLIES,
        "hidden" => EE_SCREENS_MG_TOGGLE_HIDDEN,
        "follow" => EE_SCREENS_MG_FOLLOW,
        "perspective" => EE_SCREENS_MG_PERSPECTIVE,
        "faster" => EE_SCREENS_MG_FASTER,
        "slower" => EE_SCREENS_MG_SLOWER,
        "scout" => EE_SCREENS_MG_BEHAVIOR_SCOUT,
        "stand_ground" => EE_SCREENS_MG_BEHAVIOR_STAND_GROUND,
        "guard" => EE_SCREENS_MG_BEHAVIOR_GUARD,
        "aggressive" => EE_SCREENS_MG_BEHAVIOR_AGGRESSIVE,
        "previous_messages" => EE_SCREENS_MG_PREVIOUS_MESSAGES,
        other => panic!("kind {other}"),
    }
}

/// The state line into the world (the fields after their keywords).
fn read_state(w: &mut World, line: &str) {
    let v = words(line);
    assert_eq!(v[0], "state");
    let at = |key: &str| v.iter().position(|x| x == key).unwrap_or_else(|| panic!("no {key}")) + 1;
    let n = |key: &str| v[at(key)].parse::<i64>().unwrap();
    let hex = |key: &str| u32::from_str_radix(&v[at(key)], 16).unwrap();
    w.ui = n("ui");
    w.ghosts = n("ghosts") as u32;
    w.pointer = n("pointer") as u32;
    w.lock = n("lock") as u32;
    w.refresh = n("refresh") as u32;
    w.clock = n("clock");
    w.hot_keys = n("hotkeys") as u32;
    w.group = n("group") as u32;
    w.fopen = n("fopen");
    w.open_save = n("opensave");
    w.dialog = n("dialog");
    w.civ = n("civ");
    w.editor_result = n("editorr");
    w.local = n("local");
    w.player = n("player") != 0;
    w.slot = n("slot") as u32;
    w.server = n("server") != 0;
    w.client = n("client") != 0;
    w.cts = n("cts") as u32;
    w.multiplayer = n("mp") != 0;
    w.editor = n("editor") != 0;
    w.time = n("time");
    w.cinematic = n("cinematic") != 0;
    let m = at("message");
    w.message = (v[m] == "1", v[m + 1].parse().unwrap());
    w.lock_speed = n("lockspeed") != 0;
    w.speed = [hex("speed"), hex("step"), hex("lo"), hex("hi")];
    w.hidden = n("hidden");
    w.cam = n("cam");
    w.lead = n("lead") as u32;
    let (s, e) = (at("selection"), at("selected") - 1);
    w.selection = v[s..e].iter().map(|x| x.split(':').nth(1).unwrap().parse().unwrap()).collect();
    let t = at("tod") - 1;
    let sel = at("selected");
    w.commands = (t > sel + 1).then(|| v[sel + 1..t].iter().map(|x| x.parse().unwrap()).collect());
    let tod = at("tod");
    for i in 0..6 {
        w.tod[i] = v[tod + i].parse().unwrap();
    }
    w.quick = v[at("quick")].clone();
    w.auto = v[at("auto")].clone();
    let (can, flags) = (at("can"), at("flags"));
    for u in 0..6 {
        w.can_hide[u] = v[can + u] == "1";
        w.flags[u] = v[flags + u].parse().unwrap();
    }
}

#[test]
fn golden_main_game() {
    let text = String::from_utf8(GOLDEN.to_vec()).unwrap().replace("\r\n", "\n");
    let mut cases: Vec<Vec<&str>> = Vec::new();
    for line in text.lines() {
        if line.starts_with("case ") {
            cases.push(Vec::new());
        }
        cases.last_mut().unwrap().push(line);
    }
    assert_eq!(cases.len(), 2000);
    let mut w = World { bitmaps: [(-1, -1); 9], ..Default::default() };
    let mut kinds = std::collections::BTreeSet::new();
    for (n, case) in cases.iter().enumerate() {
        let kind = words(case[0])[2].clone();
        kinds.insert(kind.clone());
        read_state(&mut w, case[1]);
        w.out.clear();
        let result = dispatch(&mut w, event_of(&kind)).unwrap();
        w.line(format!("  result {result}"));
        let bitmaps: Vec<String> = w.bitmaps.iter().map(|(a, b)| format!("{a}/{b}")).collect();
        let after = format!(
            "  after ui {} ghosts {} pointer {} lock {} refresh {} group {} clock {} hidden {} cam {} zoom {} quick '{}' \
             bitmaps {} flags {}",
            w.ui,
            w.ghosts,
            w.pointer,
            w.lock,
            w.refresh,
            w.group,
            w.clock,
            w.hidden,
            w.cam,
            w.zoom,
            w.quick,
            bitmaps.join(" "),
            w.flags.map(|f| f.to_string()).join(" ")
        );
        w.line(after);
        let expected = &case[2..];
        for (i, e) in expected.iter().enumerate() {
            assert!(i < w.out.len(), "case {n} ({kind}): missing line {i}: {e}");
            assert_eq!(w.out[i], *e, "case {n} ({kind}) line {i}");
        }
        assert_eq!(w.out.len(), expected.len(), "case {n} ({kind}): extra lines {:?}", &w.out[expected.len()..]);
    }
    assert_eq!(kinds.len(), 38);
}
