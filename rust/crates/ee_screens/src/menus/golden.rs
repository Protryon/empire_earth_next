//! The menus against the real C++: a scratch driver (`build/sc4/menus_driver.cpp`) ran the unmodified units
//! (`EEUIEMainScreenEvents.cpp`, `EEUIEEmpireEarthEditorsEvents.cpp`, `EEUIESinglePlayerEvents.cpp`) over fake forms,
//! manager, user interface thread, server and messages and the real `UWideString` and name check: 2,000 cases of
//! every event (a third of them the random map) with the typed name, the player name, the flags the events change,
//! the WON connection, the network adapter and the civilization builder's points drawn at random; every toolkit and
//! game call is a trace line (`golden_menus.txt`). The replay runs [`dispatch`] over a host that prints what the
//! driver's fakes print for each call and checks every line up to the case's `result` (the random map's slot dump
//! after it is the C++ host's, which the driver relinked over `EEScreens.cpp` prints the same).
//!
//! The play campaign screen the same way (`build/sc4/campaign_driver.cpp` over the unmodified
//! `EEUIEPlayCampaignEvents.cpp`: 2,000 cases of its eight events, a quarter of them the mouse over a control 2..6,
//! the campaign files loading or not, the Please wait string missing or not, the manager's all-open flag; the state
//! carried from case to case; `golden_campaign.txt`).

use super::*;

const GOLDEN: &[u8] = include_bytes!("../golden_menus.txt");
const CAMPAIGN: &[u8] = include_bytes!("../golden_campaign.txt");

fn cat(parts: &[&[u8]]) -> Vec<u8> {
    parts.concat()
}

/// The driver's fakes, as the host.
#[derive(Default)]
struct Driver {
    out: Vec<Vec<u8>>,
    form: u32,
    typed: Vec<u8>,
    player: Vec<u8>,
    lobby_mp: bool,
    state_mp: bool,
    editor: bool,
    saved: bool,
    won: bool,
    adapter: Vec<u8>,
    points: u64,
    civ_max: u64,
    // the play campaign driver's: the entity's index, the Please wait string missing, the files' salt, the campaign
    // form manager's fields
    entity: u32,
    missing: bool,
    salt: usize,
    manager: [Vec<u8>; 3],
    all_open: bool,
}

fn event_name(event: u32) -> &'static str {
    match event {
        EE_SCREENS_SP_OPEN => "class EEUIESPEOpen",
        crate::settings_form::EE_SCREENS_GS_OUT_OF_GAME_OPEN => "class EEUIEGSEOutOfGameOpen",
        EE_SCREENS_PC_OPEN => "class EEUIEPCEFormOpen",
        EE_SCREENS_PC_MOUSE_OVER => "class EEUIEPCEMouseOver",
        crate::scenario_screens::EE_SCREENS_CS_OPEN => "class EEUIECSSEFormOpen",
        crate::scenario_screens::EE_SCREENS_CS_CLOSE => "class EEUIECSSEFormClose",
        crate::custom_campaign::EE_SCREENS_CC_OPEN => "class EEUIECCCEFormOpen",
        crate::custom_campaign::EE_SCREENS_CC_CLOSE => "class EEUIECCCEFormClose",
        crate::lobby_form::EE_SCREENS_LE_ACTIVATE_SINGLE => "class EEUIELEActivateSinglePlayer",
        EE_SCREENS_CPP_DUMP_MEDIA => "class EEUIEMGDumpLoadedMedia(0,1)",
        EE_SCREENS_CPP_DUMP_MEDIA_ALL => "class EEUIEMGDumpLoadedMedia(1,1)",
        _ => panic!("no name for {event:#x}"),
    }
}

impl Driver {
    fn line(&mut self, text: String) {
        self.out.push(format!("  {text}").into_bytes());
    }
}

impl MenuHost for Driver {
    fn push(&mut self, form: u32) {
        self.line(format!("push {form}"));
    }
    fn pop(&mut self) {
        self.line("pop".into());
    }
    fn switch_to(&mut self, form: u32) {
        self.line(format!("switch {form}"));
    }
    fn set_open_event(&mut self, form: u32, event: u32) {
        self.line(format!("openevent {form} {}", event_name(event)));
    }
    fn show(&mut self, control: u32, mode: u32) {
        self.line(format!("mode {control} {mode}"));
    }
    fn set_label(&mut self, control: u32, text: &[u8]) {
        self.out.push(cat(&[format!("  label {control} '").as_bytes(), text, b"' 1"]));
    }
    fn edit_text(&mut self, control: u32) -> Vec<u8> {
        assert_eq!(control, SP_NAME_BOX);
        self.typed.clone()
    }
    fn set_edit_text(&mut self, control: u32, text: &[u8]) {
        self.typed = text.to_vec();
        self.out.push(cat(&[format!("  edit {control} '").as_bytes(), text, b"'"]));
    }
    fn add_hot_key(&mut self, key: u32, m1: u32, m2: u32, event: u32) {
        let form = self.form;
        self.line(format!("hotkey {form} {key} {m1} {m2} null null {} null 0 0", event_name(event)));
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        format!("S{id}").into_bytes()
    }
    fn message(&mut self, text: &[u8], last: bool) {
        self.out.push(cat(&[b"  display '", text, format!("' 1 999 0 {}", last as u8).as_bytes()]));
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        match which {
            EE_SCREENS_TEXT_VERSION => b"v1.0 build".to_vec(),
            EE_SCREENS_TEXT_PLAYER => self.player.clone(),
            EE_SCREENS_TEXT_CAMPAIGN => {
                ["AOCRoman", "AOCPacific", "AOCAsian", "EELearningCampaign"][arg as usize].as_bytes().to_vec()
            }
            _ => panic!("text {which}"),
        }
    }
    fn set_close_event(&mut self, form: u32, event: u32) {
        self.line(format!("closeevent {form} {}", event_name(event)));
    }
    fn set_trigger(&mut self, form: u32, control: u32, trigger: u32, event: u32) {
        assert_eq!(form, FORM_PLAY_CAMPAIGN);
        self.line(format!("trigger {control} {trigger} {}", event_name(event)));
    }
    fn open_form(&mut self, form: u32) {
        self.line(format!("openform {form}"));
    }
    fn entity_index(&mut self) -> u32 {
        self.entity
    }
    fn string_checked(&mut self, id: u32) -> Option<Vec<u8>> {
        (!(self.missing && id == IDS_PLEASE_WAIT)).then(|| self.string(id))
    }
    fn set_label_lines(&mut self, form: u32, control: u32, text: &[u8]) {
        assert_eq!(form, FORM_PLAY_CAMPAIGN);
        self.line(format!("deletearray {control}"));
        self.out.push(cat(&[format!("  append {control} '").as_bytes(), text, b"'"]));
    }
    fn set_campaign(&mut self, which: u32, text: &[u8]) {
        self.manager[(which - EE_SCREENS_TEXT_CAMPAIGN_NAME) as usize] = text.to_vec();
    }
    fn campaign_info(&mut self, name: &[u8]) -> Option<(Vec<u8>, Vec<u8>)> {
        // the driver's files: one loads when its name's length and the salt are odd together
        let file = cat(&[b"Campaigns\\", name, b".eec"]);
        let ok = (file.len() + self.salt) & 1 != 0;
        self.out.push(cat(&[b"  load '", &file, format!("' 0 -> {}", ok as u8).as_bytes()]));
        ok.then(|| (cat(&[b"desc of ", &file]), format!("civ{}", file.len()).into_bytes()))
    }
    fn set_player_name(&mut self, name: &[u8]) {
        self.player = name.to_vec();
    }
    fn call_with(&mut self, op: u32, arg: u32) {
        match op {
            EE_SCREENS_GAME_SHOW_SINGLE_PLAYER => self.line("showsingle".into()),
            EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN => self.all_open = arg != 0,
            EE_SCREENS_GAME_QUIT => self.line("quit".into()),
            EE_SCREENS_GAME_EXTERNAL_LOBBY => self.line("externallobby".into()),
            EE_SCREENS_GAME_EDITOR_MODE_OFF => self.editor = false,
            EE_SCREENS_GAME_SINGLE => self.state_mp = false,
            EE_SCREENS_GAME_MULTI => self.state_mp = true,
            EE_SCREENS_GAME_DELETE_EDITOR => self.line("deleteeditor".into()),
            EE_SCREENS_GAME_DELETE_WON => {
                if self.won {
                    self.line("wonvirtual 0".into());
                    self.won = false;
                }
            }
            EE_SCREENS_GAME_CREATE_EDITOR => self.line("createeditor".into()),
            EE_SCREENS_GAME_START_EDITOR => self.line("starteditor".into()),
            EE_SCREENS_GAME_CIV_BUILDER => {
                // the host: upgrades cleared, the points back to the maximum, then SetPointsAndMax (the driver's
                // fake: points x 3, maximum x 5)
                let (p, m) = (self.points, self.civ_max);
                self.line(format!("civpoints {p} (had {m} {m})"));
                self.civ_max = p * 5;
                self.line(format!("civbuilder points {} max {} upgrades 0 0", p * 3, p * 5));
            }
            EE_SCREENS_GAME_FORGET_MAIN_FORM => self.saved = false,
            EE_SCREENS_GAME_LOBBY_BEGIN => {
                self.line("writelock".into());
                self.line("lastoptions".into());
            }
            EE_SCREENS_GAME_LOBBY_END => self.line("writeunlock".into()),
            _ => panic!("call {op}"),
        }
    }
    fn lobby_fill(&mut self, slots: &[EeScreensSinglePlayerSlot; 16], _: &[u8], _: &[u8]) {
        assert_eq!(slots, &single_player::random_map_slots());
        self.lobby_mp = false;
        self.line("writeunlock".into());
    }
    fn start_local_server(&mut self, human: &[u8]) {
        let adapter = String::from_utf8(self.adapter.clone()).unwrap();
        self.line(format!("server EEName {adapter}"));
        self.out.push(cat(&[b"  localinput 255 ", human, b" v1.0 build 7 0"]));
    }
}

/// The text between `key '` and the next `' `.
fn quoted<'a>(line: &'a [u8], key: &str) -> &'a [u8] {
    let k = format!("{key} '");
    let at = line.windows(k.len()).position(|w| w == k.as_bytes()).unwrap() + k.len();
    let end = at + line[at..].windows(2).position(|w| w == b"' ").unwrap();
    &line[at..end]
}

/// The word after `key ` (the first one after `after`).
fn word<'a>(line: &'a [u8], key: &str) -> &'a [u8] {
    let k = format!(" {key} ");
    let at = line.windows(k.len()).rposition(|w| w == k.as_bytes()).unwrap() + k.len();
    let end = line[at..].iter().position(|&b| b == b' ').map_or(line.len(), |e| at + e);
    &line[at..end]
}

fn flag(line: &[u8], key: &str) -> bool {
    word(line, key) == b"1"
}

fn code(name: &[u8]) -> u32 {
    match name {
        b"MSEOpen" => EE_SCREENS_MAIN_OPEN,
        b"MSEGameSettings" => EE_SCREENS_MAIN_GAME_SETTINGS,
        b"MSEExitGame" => EE_SCREENS_MAIN_EXIT,
        b"MSESinglePlayer" => EE_SCREENS_MAIN_SINGLE_PLAYER,
        b"MSEMultiPlayer" => EE_SCREENS_MAIN_MULTI_PLAYER,
        b"EEEESubMenuOpen" => EE_SCREENS_EDITORS_OPEN,
        b"EEEEEditor" => EE_SCREENS_EDITORS_CAMPAIGN_EDITOR,
        b"EEEEMainMenu" => EE_SCREENS_EDITORS_MAIN_MENU,
        b"EEEEScenarioEditor" => EE_SCREENS_EDITORS_SCENARIO_EDITOR,
        b"EEEECivBuilder" => EE_SCREENS_EDITORS_CIV_BUILDER,
        b"SPEOpen" => EE_SCREENS_SP_OPEN,
        b"SPECampaign" => EE_SCREENS_SP_CAMPAIGN,
        b"SPECivCampaign" => EE_SCREENS_SP_CIV_CAMPAIGN,
        b"SPELoadGame" => EE_SCREENS_SP_LOAD_GAME,
        b"SPEMainMenu" => EE_SCREENS_SP_MAIN_MENU,
        b"SPERandomMap" => EE_SCREENS_SP_RANDOM_MAP,
        _ => panic!("event {}", String::from_utf8_lossy(name)),
    }
}

#[test]
fn replays_the_menus_golden() {
    // (the driver's stdout is in text mode: CR LF)
    let lines: Vec<&[u8]> =
        GOLDEN.split(|&b| b == b'\n').map(|l| l.strip_suffix(b"\r").unwrap_or(l)).filter(|l| !l.is_empty()).collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with(b"case ")).collect();
    assert_eq!(starts.len(), 2000);
    let mut civ_max = 0;
    let mut seen = std::collections::BTreeSet::new();
    for (n, &a) in starts.iter().enumerate() {
        let head = lines[a];
        let event = code(word(head, "event"));
        seen.insert(event);
        let mut d = Driver {
            form: std::str::from_utf8(word(head, "form")).unwrap().parse().unwrap(),
            typed: quoted(head, "name").to_vec(),
            player: quoted(head, "player").to_vec(),
            lobby_mp: flag(head, "mp"),
            state_mp: flag(head, "state"),
            editor: flag(head, "editor"),
            saved: flag(head, "saved"),
            won: flag(head, "won"),
            adapter: word(head, "adapter").to_vec(),
            points: std::str::from_utf8(word(head, "points")).unwrap().parse().unwrap(),
            civ_max,
            ..Default::default()
        };
        assert!(dispatch(&mut d, &Mutex::default(), event));
        civ_max = d.civ_max;
        let state = format!(
            "' mp {} state {} editor {} saved {} won {} box '",
            d.lobby_mp as u8, d.state_mp as u8, d.editor as u8, d.saved as u8, d.won as u8
        );
        let result = cat(&[b"  result 0 player '", &d.player, state.as_bytes(), &d.typed, b"'"]);
        d.out.push(result);
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let golden = &lines[a + 1..b];
        for (k, x) in d.out.iter().enumerate() {
            assert_eq!(
                String::from_utf8_lossy(x),
                String::from_utf8_lossy(golden.get(k).copied().unwrap_or(b"(none)")),
                "case {n} line {k}"
            );
        }
        // the random map's slots after the result: the C++ host's lobby options
        let rest = &golden[d.out.len()..];
        assert!(rest.iter().all(|l| l.starts_with(b"  slot ")), "case {n}");
        assert_eq!(rest.len(), if event == EE_SCREENS_SP_RANDOM_MAP { 16 } else { 0 }, "case {n}");
    }
    assert_eq!(seen.len(), 16);
}

#[test]
fn replays_the_play_campaign_golden() {
    let lines: Vec<&[u8]> =
        CAMPAIGN.split(|&b| b == b'\n').map(|l| l.strip_suffix(b"\r").unwrap_or(l)).filter(|l| !l.is_empty()).collect();
    let starts: Vec<usize> = (0..lines.len()).filter(|&i| lines[i].starts_with(b"case ")).collect();
    assert_eq!(starts.len(), 2000);
    let state = Mutex::default();
    let mut manager: [Vec<u8>; 3] = Default::default();
    let mut seen = std::collections::BTreeSet::new();
    for (n, &a) in starts.iter().enumerate() {
        let head = lines[a];
        let num = |key: &str| -> usize { std::str::from_utf8(word(head, key)).unwrap().parse().unwrap() };
        let event = match word(head, "event") {
            b"Back" => EE_SCREENS_PC_BACK,
            b"Campaign1" => EE_SCREENS_PC_CAMPAIGN_1,
            b"Campaign2" => EE_SCREENS_PC_CAMPAIGN_2,
            b"Campaign3" => EE_SCREENS_PC_CAMPAIGN_3,
            b"MouseOver" => EE_SCREENS_PC_MOUSE_OVER,
            b"FormOpen" => EE_SCREENS_PC_OPEN,
            b"Custom" => EE_SCREENS_PC_CUSTOM,
            b"Learning" => EE_SCREENS_PC_LEARNING,
            other => panic!("event {}", String::from_utf8_lossy(other)),
        };
        seen.insert(event);
        let mut d = Driver {
            entity: num("entity") as u32,
            missing: num("missing") == 1,
            salt: num("salt"),
            manager: manager.clone(),
            all_open: num("open") == 1,
            ..Default::default()
        };
        assert!(dispatch(&mut d, &state, event));
        let result = cat(&[
            b"  result 0 name '",
            &d.manager[0],
            b"' description '",
            &d.manager[1],
            b"' first '",
            &d.manager[2],
            format!("' open {}", d.all_open as u8).as_bytes(),
        ]);
        d.out.push(result);
        manager = d.manager.clone();
        let b = starts.get(n + 1).copied().unwrap_or(lines.len());
        let golden = &lines[a + 1..b];
        assert_eq!(d.out.len(), golden.len(), "case {n}");
        for (k, (x, y)) in d.out.iter().zip(golden).enumerate() {
            assert_eq!(String::from_utf8_lossy(x), String::from_utf8_lossy(y), "case {n} line {k}");
        }
    }
    assert_eq!(seen.len(), 8);
}
