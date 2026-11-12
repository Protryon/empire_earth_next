//! The in-game options menu against the real C++: a scratch driver (`build/sc7/options_driver.cpp`) ran the
//! unmodified `EEUIEMainGameOptionsEvents.cpp` over fake forms (the menu's buttons, the main game form's options
//! button), triggers, the close event, hot keys, manager, user interface thread, communications (server and client),
//! the physics thread, the world, its players and the intro manager: one session of 2,000 cases, each perturbing the
//! state (printed), then opening the menu, firing one stored trigger (an event kind drawn first), the Escape hot key,
//! the form's close event, a queued posted event, the manager's briefing event or a pending question's yes
//! (`golden_main_game_options.txt`). The replay runs [`dispatch`] over a host that keeps the same world and prints
//! what the driver's fakes print for each host call. The driver relinked over `EEScreens.cpp` and this crate prints the
//! same file, and the same on seeds 2-4 at ten times the size.

use std::collections::{BTreeMap, VecDeque};
use std::sync::Mutex;

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_main_game_options.txt");

static STATE: Mutex<MainGameOptions> = Mutex::new(MainGameOptions::new());

fn event_name(event: u32) -> String {
    match event {
        0 => "null",
        EE_SCREENS_GO_RETURN_TO_GAME => "class EEUIEMGOEReturnToGame",
        EE_SCREENS_GO_RESIGN => "class EEUIEMGOEResign",
        EE_SCREENS_GO_RESTART => "class EEUIEMGOERestartGame",
        EE_SCREENS_GO_RETURN_TO_EDITOR => "class EEUIEMGOEReturnToEditor",
        EE_SCREENS_GO_LOAD_SAVED_GAME => "class EEUIEMGOELoadSavedGame",
        EE_SCREENS_GO_SAVE_GAME => "class EEUIEMGOESaveGame",
        EE_SCREENS_GO_PAUSE => "class EEUIEMGOEPauseGame",
        EE_SCREENS_GO_PREVIOUS_MESSAGES => "class EEUIEMGOEPreviousMessages",
        EE_SCREENS_GO_LOAD_SCENARIO | EE_SCREENS_GO_LOAD_SCENARIO_POSTED => "class EEUIEMGOELoadScenario",
        EE_SCREENS_GO_BRIEFING => "class EEUIEMGOEGameBriefing",
        EE_SCREENS_GO_SETTINGS => "class EEUIEMGOEGameSettings",
        EE_SCREENS_GO_EXIT => "class EEUIEMGOEExitGame",
        EE_SCREENS_GO_OPEN => "class EEUIEMGOEOpenOptionsScreen",
        EE_SCREENS_GO_CLOSE => "class EEUIEMGOECloseOptions",
        EE_SCREENS_GO_DISPLAY_INTRO => "class EEUIEMGDisplayScenarioIntro",
        other => return format!("screen {other}"),
    }
    .to_string()
}

/// The driver's world.
#[derive(Default)]
struct World {
    out: Vec<String>,
    server: bool,
    migrating: bool,
    playback: bool,
    cts: u32,
    multiplayer: bool,
    editor: bool,
    paused: bool,
    observer: bool,
    scenario: bool,
    intro: bool,
    close_result: i32,
    open_result: i32,
    music: String,
    flags: (bool, bool),
    saved: u32,
    string_id: u32,
    triggers: BTreeMap<(u32, u32), u32>,
    close_event: u32,
    hot_key: u32,
    posted: VecDeque<u32>,
    yes: u32,
}

impl World {
    fn line(&mut self, text: String) {
        self.out.push(text);
    }
}

impl OptionsHost for World {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        match op {
            EE_SCREENS_OP_TRIGGER_HERE => {
                self.line(format!("  trigger {a} {b} {}", event_name(c)));
                self.triggers.insert((a, b), c);
            }
            EE_SCREENS_OP_SHOW => self.line(format!("  mode {a} {b}")),
            EE_SCREENS_GAME_PAUSE => self.line("  worldpause".into()),
            EE_SCREENS_GAME_GO_OPTIONS_BLINK_OFF => self.line("  blinking 403 0".into()),
            EE_SCREENS_GAME_GO_USER_PAUSED => return self.paused as u32,
            EE_SCREENS_OP_CLOSE_EVENT => {
                assert_eq!(a, 28);
                self.line(format!("  closeevent 28 {}", event_name(b)));
                self.close_event = b;
            }
            EE_SCREENS_OP_GO_REMOVE_HOT_KEYS => {
                self.line("  removehotkeys 28".into());
                self.hot_key = 0;
            }
            EE_SCREENS_OP_GO_HOT_KEY => {
                self.line(format!("  hotkey 28 {a} 32767 32767 32767 {} null null null 0 0", event_name(b)));
                self.hot_key = b;
            }
            EE_SCREENS_GAME_IS_SERVER => return self.server as u32,
            EE_SCREENS_GAME_MULTIPLAYER_SESSION => return self.multiplayer as u32,
            EE_SCREENS_GAME_IN_EDITOR => return self.editor as u32,
            EE_SCREENS_GAME_CTS_PROCESSED => return self.cts,
            EE_SCREENS_OP_GO_STRING_ID => {
                assert_eq!(a, 10);
                self.string_id = b;
            }
            EE_SCREENS_GAME_GO_HIDE_RESIGN => return (self.observer || self.scenario) as u32,
            EE_SCREENS_OP_CLOSE_FORM => {
                self.line(format!("  closeform 999 -> {}", self.close_result));
                return self.close_result as u32;
            }
            EE_SCREENS_GAME_GO_RESIGN => self.line("  local 163 255".into()),
            EE_SCREENS_GAME_GO_RESTART => self.line("  postrestart".into()),
            EE_SCREENS_GAME_GO_TO_EDITOR => {
                self.line("  threadpost 0x402 1".into());
                self.line(format!("  posteditor {}", self.editor as i32));
                self.line("  postpause 1".into());
            }
            EE_SCREENS_OP_POST_EVENT => {
                self.line(format!("  postevent {} 999", event_name(a)));
                self.posted.push_back(a);
            }
            EE_SCREENS_GAME_GO_USER_PAUSE => self.line("  postaction class EEAUserPauseGame".into()),
            EE_SCREENS_GAME_GO_REQUEST_PAUSE => self.line("  local 57 255".into()),
            EE_SCREENS_OP_OPEN_FORM => {
                self.line(format!("  openform {a} -> {}", self.open_result));
                return self.open_result as u32;
            }
            EE_SCREENS_GAME_GO_FORM_EXISTS => {
                assert_eq!(a, 19);
                return self.intro as u32;
            }
            EE_SCREENS_GAME_GO_TO_SCENARIO_LOAD => {
                self.line("  pop".into());
                self.line("  swap 20".into());
                self.line("  serverpause".into());
                self.line("  postpause 1".into());
                self.flags = (false, true);
                self.saved = 1;
            }
            EE_SCREENS_GAME_GO_BRIEFING => self.line("  displayintro".into()),
            EE_SCREENS_OP_OPEN_EVENT => self.line(format!("  openevent {a} {}", event_name(b))),
            EE_SCREENS_GAME_GO_CANNOT_EXIT => return if self.server { self.migrating } else { self.playback } as u32,
            EE_SCREENS_GAME_GO_EXIT => self.line("  exitgame".into()),
            _ => panic!("op {op:#x}"),
        }
        0
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        let t = String::from_utf8_lossy(text).into_owned();
        match which {
            EE_SCREENS_TEXT_GO_BUTTON => self.line(format!("  buttontext {arg} '{t}' font 1")),
            EE_SCREENS_TEXT_GO_QUESTION => {
                self.line(format!("  question '{t}' 1 999 1 0"));
                self.yes = arg;
            }
            crate::menus::EE_SCREENS_TEXT_MESSAGE => self.line(format!("  display '{t}' 1 999 0 1")),
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

#[test]
fn golden_main_game_options() {
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
    let mut kinds = BTreeMap::<String, usize>::new();
    for (n, case) in cases.iter().enumerate() {
        let v = words(case[1]);
        assert_eq!(v[0], "state");
        let b = |i: usize| v[i] == "1";
        (w.server, w.migrating, w.playback) = (b(2), b(4), b(6));
        w.cts = v[8].parse().unwrap();
        (w.multiplayer, w.editor, w.paused, w.observer, w.scenario, w.intro) =
            (b(10), b(12), b(14), b(16), b(18), b(20));
        (w.close_result, w.open_result) = (v[22].parse().unwrap(), v[24].parse().unwrap());
        w.music = v[26].clone();
        w.flags = (b(28), b(29));
        let action = words(case[2]);
        let name = action[1..].join(" ");
        let kind = if action[0] == "fire" { action[3..].join(" ") } else { action[0].clone() };
        *kinds.entry(kind).or_default() += 1;
        w.out.clear();
        let event = match action[0].as_str() {
            "open" => Some(EE_SCREENS_GO_OPEN),
            "hotkey" => Some(w.hot_key),
            "close" => Some(w.close_event),
            "intro" => Some(EE_SCREENS_GO_DISPLAY_INTRO),
            "posted" => {
                let e = w.posted.pop_front().unwrap();
                (!event_name(e).starts_with("screen")).then_some(e)
            }
            "yes" => {
                let yes = std::mem::take(&mut w.yes);
                assert!(dispatch(&mut w, &STATE, yes).is_some());
                None
            }
            "fire" => {
                let key = (action[1].parse().unwrap(), action[2].parse().unwrap());
                assert_eq!(event_name(w.triggers[&key]), action[3..].join(" "), "case {n}");
                Some(w.triggers[&key])
            }
            other => panic!("case {n}: {other}"),
        };
        if let Some(e) = event {
            if action[0] != "fire" {
                assert_eq!(event_name(e), name, "case {n}");
            }
            let result = dispatch(&mut w, &STATE, e).unwrap();
            w.line(format!("  result {result}"));
        }
        let after = format!(
            "  after music '{}' flags {} {} saved {} stringid {}",
            w.music, w.flags.0 as i32, w.flags.1 as i32, w.saved, w.string_id
        );
        w.line(after);
        let expected = &case[3..];
        for (i, e) in expected.iter().enumerate() {
            assert!(i < w.out.len(), "case {n} ({}): missing line {i}: {e}", case[2]);
            assert_eq!(w.out[i], *e, "case {n} ({}) line {i}", case[2]);
        }
        assert_eq!(w.out.len(), expected.len(), "case {n}: extra lines");
    }
    // the 12 control event kinds fired, and the six other actions ran
    assert_eq!(kinds.len(), 18, "{kinds:?}");
}
