//! The campaign scenario select form (form 33) and the scenario intro form (form 19) whole: the original
//! `EEUIECampaignSelectScreenEvents.cpp` and the events of `EEUIEScenarioIntroEvents.cpp` (and the intro buttons' empty
//! mouse over and out events of `EEUIManager.cpp`) over a host ([`ScenarioHost`]) whose methods are each one toolkit or
//! game call (`UI Events/EEScreens.cpp`). The campaign and its progress, the scenario files and the intro form's
//! manager (`EEUISCIFormManager`: the media strings and files) stay the game's, behind the host; the one state here is
//! the intro's briefing sound.
//!
//! Every call names its form (the original used the event's form or the manager's by id: the same form in the game;
//! the intro's posted events have no form).

use std::sync::Mutex;

use ee_plat_core::utf8;

use crate::custom_campaign::{
    EE_SCREENS_OP_FORM_LIST_CLEAR, EE_SCREENS_OP_FORM_LIST_COUNT, EE_SCREENS_OP_FORM_LIST_SELECT,
    EE_SCREENS_OP_FORM_LIST_SELECTION, EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN, EE_SCREENS_TEXT_CAMPAIGN_ERROR,
    EE_SCREENS_TEXT_CAMPAIGN_EXTENSION, EE_SCREENS_TEXT_FORM_LIST_APPEND, EE_SCREENS_TEXT_FORM_LIST_DATA,
};
use crate::menus::{
    EE_SCREENS_GAME_SHOW_SINGLE_PLAYER, EE_SCREENS_OP_OPEN_FORM, EE_SCREENS_OP_SHOW, EE_SCREENS_OP_TRIGGER,
    EE_SCREENS_TEXT_CAMPAIGN_LOAD, EE_SCREENS_TEXT_CAMPAIGN_NAME, EE_SCREENS_TEXT_LABEL, EE_SCREENS_TEXT_LABEL_LINES,
};
use crate::save_load::EE_SCREENS_OP_POST_EVENT;
use crate::settings_form::{
    EE_SCREENS_GAME_MASTER_VOLUME, EE_SCREENS_OP_BACKGROUND, EE_SCREENS_OP_CLOSE_FORM, EE_SCREENS_OP_COMBO_CLEAR,
    EE_SCREENS_OP_COMBO_SELECT, EE_SCREENS_OP_GROUP, EE_SCREENS_TEXT_COMBO_APPEND,
};

// ---- the events (EEScreenEvent::mEvent)

/// The campaign scenario select form: its open event (set by the play campaign and custom campaign screens and the
/// post-game summary), its close event (nothing), the custom campaign list's and the scenario list's selection, the
/// difficulty, Start, Back, and the open-error message's callback (no event object).
pub const EE_SCREENS_CS_OPEN: u32 = 0x2101;
pub const EE_SCREENS_CS_CLOSE: u32 = 0x2102;
pub const EE_SCREENS_CS_SELECT_CAMPAIGN: u32 = 0x2103;
pub const EE_SCREENS_CS_SELECT_SCENARIO: u32 = 0x2104;
pub const EE_SCREENS_CS_DIFFICULTY: u32 = 0x2105;
pub const EE_SCREENS_CS_START: u32 = 0x2106;
pub const EE_SCREENS_CS_BACK: u32 = 0x2107;
pub const EE_SCREENS_CS_CLOSE_CALLBACK: u32 = 0x2108;
/// The scenario intro form: its open and close events, Back, the briefing, hints and history pages, the map, the movie
/// (then the sound: the sound-over event, posted), Start, and the buttons' mouse over and out (nothing).
pub const EE_SCREENS_SI_OPEN: u32 = 0x1301;
pub const EE_SCREENS_SI_CLOSE: u32 = 0x1302;
pub const EE_SCREENS_SI_BACK: u32 = 0x1303;
pub const EE_SCREENS_SI_BRIEFING: u32 = 0x1304;
pub const EE_SCREENS_SI_HINTS: u32 = 0x1305;
pub const EE_SCREENS_SI_HISTORY: u32 = 0x1306;
pub const EE_SCREENS_SI_MAP: u32 = 0x1307;
pub const EE_SCREENS_SI_MOVIE: u32 = 0x1308;
pub const EE_SCREENS_SI_SOUND_OVER: u32 = 0x1309;
pub const EE_SCREENS_SI_START: u32 = 0x130a;
pub const EE_SCREENS_SI_MOUSE: u32 = 0x130b;

// ---- the host's ops (ee_screenscpp_call on the named form; the shared ones are menus.rs's, settings_form.rs's,
// custom_campaign.rs's and save_load.rs's)

pub const EE_SCREENS_OP_REMOVE_BACKGROUND: u32 = 0x180; // (id) RemoveBackground(GetBackgroundByID(id)) when there is one
pub const EE_SCREENS_OP_TAB_STOP: u32 = 0x181; // (stop) SetTabStop
pub const EE_SCREENS_OP_LABEL_CLEAR: u32 = 0x182; // (control) a text label's DeleteArray
pub const EE_SCREENS_OP_LABEL_LINE_COUNT: u32 = 0x183; // (control) -> a text label's GetLineCount
pub const EE_SCREENS_OP_LABEL_START_LINE: u32 = 0x184; // (control, line) SetStartingDisplayLine
pub const EE_SCREENS_OP_ADD_TRIGGER: u32 = 0x185; // (code) the manager's AddEventToTrigger(the code's event, form, NULL)
pub const EE_SCREENS_OP_SLEEP: u32 = 0x186; // (ms) PlatSleep
pub const EE_SCREENS_OP_OPEN_MAIN_GAME_FORM: u32 = 0x187; // OpenForm(the manager's main game form)
pub const EE_SCREENS_GAME_SI_FLAGS: u32 = 0x188; // -> the intro manager's flags (SI_FLAG_*)
pub const EE_SCREENS_GAME_SI_SET_FLAG: u32 = 0x189; // (flag, on) sets one
pub const EE_SCREENS_GAME_SI_BLINK_OFF: u32 = 0x18a; // the intro manager's SetBlinking(false)
pub const EE_SCREENS_GAME_SOUND_STOP: u32 = 0x18b; // (id) gSoundManager->Stop(id, false)
pub const EE_SCREENS_GAME_SOUND_LATENCY: u32 = 0x18c; // (ms) gSoundManager->SetLatency
pub const EE_SCREENS_GAME_SI_OPEN_MAP: u32 = 0x18d; // -> the intro manager's OpenMap()
pub const EE_SCREENS_GAME_SI_OPEN_MOVIE: u32 = 0x18e; // -> OpenMovie()
pub const EE_SCREENS_GAME_SI_OPEN_SOUND: u32 = 0x18f; // -> OpenSound()
pub const EE_SCREENS_GAME_SI_PLAY_MOVIE: u32 = 0x190; // (code) GetMovie, PostPlayMovie(it, the code's event)
pub const EE_SCREENS_GAME_SI_PLAY_SOUND: u32 = 0x191; // (id) GetSound, the briefing sound loaded and registered -> its id
pub const EE_SCREENS_GAME_SI_MAP_BACKDROP: u32 = 0x192; // the form's map backdrop (-2) from GetMap, when it changed
pub const EE_SCREENS_GAME_SI_GET_SCENARIO: u32 = 0x193; // -> GetScenario (the host keeps the spec)
pub const EE_SCREENS_GAME_SI_POST_LOAD: u32 = 0x194; // PostLoadSavedGame(that spec)
pub const EE_SCREENS_GAME_SI_START_GAME: u32 = 0x195; // (campaign) the options, a local server, start loading
pub const EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM: u32 = 0x196; // the campaign form manager's UpdateChecksum()
pub const EE_SCREENS_GAME_CAMPAIGN_CHECKSUM: u32 = 0x197; // -> its mChecksum
pub const EE_SCREENS_GAME_CS_MAP_BACKDROP: u32 = 0x198; // the campaign's background image as the form's backdrop
pub const EE_SCREENS_GAME_DIFFICULTY: u32 = 0x199; // -> gDifficultyLevel
pub const EE_SCREENS_GAME_CS_SET_DIFFICULTY: u32 = 0x19a; // (level) the campaign's, the game's and the options'
pub const EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET: u32 = 0x19b; // -> the campaign form manager's mAllScenariosOpen
/// The intro manager's flags.
pub const EE_SCREENS_SI_FLAG_IN_GAME: u32 = 1;
pub const EE_SCREENS_SI_FLAG_LOADING: u32 = 2;
pub const EE_SCREENS_SI_FLAG_CAMPAIGN: u32 = 4;
pub const EE_SCREENS_SI_FLAG_SCROLL: u32 = 8;
/// Texts (ee_screenscpp_text / _put_text).
pub const EE_SCREENS_TEXT_SI_BRIEFING: u32 = 0x50; // the intro manager's GetBriefing()
pub const EE_SCREENS_TEXT_SI_HINTS: u32 = 0x51; // GetHints()
pub const EE_SCREENS_TEXT_SI_HISTORY: u32 = 0x52; // GetHistory()
pub const EE_SCREENS_TEXT_CS_CAMPAIGN_NAME: u32 = 0x53; // the campaign form manager's mCampaignName
pub const EE_SCREENS_TEXT_CS_SCENARIOS: u32 = 0x54; // its campaign's GetScenarios(), each ended by 0
pub const EE_SCREENS_TEXT_CS_COMPLETED: u32 = 0x55; // the campaign progress's completed scenarios (none: not loaded)
pub const EE_SCREENS_TEXT_CS_PROGRESS: u32 = 0x56; // (completed << 16 | scenarios) the in-progress text
pub const EE_SCREENS_TEXT_CS_DESCRIPTION: u32 = 0x57; // the campaign form manager's mDescription
pub const EE_SCREENS_TEXT_CS_ARCHIVE: u32 = 0x58; // put (valid, file): FindFile; a valid one's archive added -> 1
pub const EE_SCREENS_TEXT_CS_SCENARIO_HEADER: u32 = 0x59; // put (name): the campaign scenario's header loads -> 1
pub const EE_SCREENS_TEXT_CS_SCENARIO_NAME: u32 = 0x5a; // put: the campaign form manager's mScenarioName
pub const EE_SCREENS_TEXT_CS_START: u32 = 0x5b; // put (name): the scenario into the intro manager -> 2, 1 bad header
pub const EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN: u32 = 0x5c; // put: DisplayMessage(text, true, NULL, closes, true)

/// The form a host call names.
pub const SC_FORM_INTRO: u32 = 19;
pub const SC_FORM_SELECT: u32 = 33;
const SC_FORM_SCENARIO_LOAD: u32 = 20;
// the controls (ee/EEUIManager.h), triggers (lle/UIControl.h), backgrounds and strings (ee/EEStringIDs.h)
const SC_SI_MAP_BUTTON: u32 = 0;
const SC_SI_MOVIE_BUTTON: u32 = 4;
const SC_SI_START_BUTTON: u32 = 5;
const SC_SI_TEXT: u32 = 7;
const SC_SI_TITLE: u32 = 8;
const SC_CS_SCENARIOS: u32 = 2;
const SC_CS_DESCRIPTION: u32 = 4;
const SC_CS_DIFFICULTY_LABEL: u32 = 5;
const SC_CS_DIFFICULTY: u32 = 6;
const SC_CS_START: u32 = 7;
const SC_CS_BACK: u32 = 8;
const SC_CS_CAMPAIGNS: u32 = 9;
const SC_CS_PROGRESS: u32 = 10;
const SC_TRIGGER_SELECTION_CHANGED: u32 = 0x3eb;
const SC_TRIGGER_BUTTON_UP: u32 = 0x3f0;
const SC_BACKGROUND_TEXT: u32 = 0x49;
const SC_BACKGROUND_81: u32 = 0x81;
const SC_BACKGROUND_MAP: u32 = (-2i32) as u32;
const SC_SHOWN: u32 = 1;
const SC_HIDDEN: u32 = 0;
const SC_IDS_MENU_BRIEFING: u32 = 7075;
const SC_IDS_MENU_HISTORY: u32 = 7076;
const SC_IDS_MENU_HINTS: u32 = 7077;
const SC_IDS_NO_DESCRIPTION: u32 = 7124;
const SC_IDS_FILE_COULDNT_BE_OPENED: u32 = 7152;
const SC_IDS_DIFFICULTIES: [u32; 3] = [30269, 30248, 30270]; // easy, medium, hard
const SC_IDS_CAMPAIGN_COMPLETED: u32 = 31806;
const SC_IDS_SCENARIO_COMPLETED: u32 = 31807;
const SC_NO_SOUND: u32 = u32::MAX;

/// The intro's briefing sound (`sSoundID`: its close and sound-over stop it, its open resets it).
pub struct ScenarioScreens {
    pub sound: u32,
}

impl ScenarioScreens {
    pub const fn new() -> Self {
        ScenarioScreens { sound: 0 }
    }
}

impl Default for ScenarioScreens {
    fn default() -> Self {
        Self::new()
    }
}

/// The toolkit's and the game's calls of the two forms. `form` names the form a call acts on. Texts are the game's
/// UTF-8 bytes.
pub trait ScenarioHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` on `form` with up to four arguments and an answer.
    fn call(&mut self, form: u32, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32;
    fn text(&mut self, form: u32, which: u32, arg: u32) -> Vec<u8>;
    /// A text into the game; the host's answer.
    fn put_text(&mut self, form: u32, which: u32, arg: u32, text: &[u8]) -> u32;
    /// A resource string (whatever `GetString` left when it fails).
    fn string(&mut self, id: u32) -> Vec<u8>;
    /// A combo box's `GetCurrentSelection` (its selection, whatever the result).
    fn combo_selection(&mut self, form: u32, control: u32) -> u32;
}

/// The scenario intro form's event for a control event id (its buttons, their mouse over and out); 0 for another.
pub fn scenario_intro_event_for(id: u32) -> u32 {
    match id {
        94 => EE_SCREENS_SI_BACK,
        95 => EE_SCREENS_SI_BRIEFING,
        96 => EE_SCREENS_SI_HINTS,
        97 => EE_SCREENS_SI_HISTORY,
        98 => EE_SCREENS_SI_MAP,
        99 => EE_SCREENS_SI_MOVIE,
        100 => EE_SCREENS_SI_START,
        124..=137 => EE_SCREENS_SI_MOUSE,
        _ => 0,
    }
}

pub fn is_scenario_event(event: u32) -> bool {
    (EE_SCREENS_CS_OPEN..=EE_SCREENS_CS_CLOSE_CALLBACK).contains(&event)
        || (EE_SCREENS_SI_OPEN..=EE_SCREENS_SI_MOUSE).contains(&event)
}

/// `name.GetIndexedSubstring(0, name.Find(suffix, 0) - 1)` of a `UWideString`: the characters before the first
/// match of `suffix` (compared character by character from each character on; the whole name when none).
pub fn without_suffix<'a>(name: &'a [u8], suffix: &[u8]) -> &'a [u8] {
    let chars = utf8::char_len(name);
    let suffix_chars = utf8::char_len(suffix);
    let mut found = chars;
    if chars >= suffix_chars {
        let last = chars - suffix_chars;
        for i in 0..=last {
            let at = utf8::bytes_of_chars(name, i);
            if name[at..].starts_with(suffix) {
                found = i;
                break;
            }
        }
    }
    &name[..utf8::bytes_of_chars(name, found)]
}

fn list(bytes: &[u8]) -> Vec<Vec<u8>> {
    let count = bytes.iter().filter(|&&b| b == 0).count();
    bytes.split(|&b| b == 0).take(count).map(|s| s.to_vec()).collect()
}

fn flags<H: ScenarioHost>(host: &mut H) -> u32 {
    host.call(SC_FORM_INTRO, EE_SCREENS_GAME_SI_FLAGS, 0, 0, 0, 0)
}

/// The text shown after a finished scenario's name: a space and the completed string.
fn completed_suffix<H: ScenarioHost>(host: &mut H) -> Vec<u8> {
    [b" ".as_slice(), &host.string(SC_IDS_SCENARIO_COMPLETED)].concat()
}

fn close_campaign_message<H: ScenarioHost>(host: &mut H, error: &[u8], name: &[u8]) {
    let text = [error, b"\n\"", name, b"\""].concat();
    host.put_text(SC_FORM_SELECT, EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN, 0, &text);
}

/// The selected scenario list line's scenario name (without the completed text); none for an empty list.
fn selected_scenario<H: ScenarioHost>(host: &mut H) -> Option<Vec<u8>> {
    let selection = host.call(SC_FORM_SELECT, EE_SCREENS_OP_FORM_LIST_SELECTION, SC_FORM_SELECT, SC_CS_SCENARIOS, 0, 0);
    if host.call(SC_FORM_SELECT, EE_SCREENS_OP_FORM_LIST_COUNT, SC_FORM_SELECT, SC_CS_SCENARIOS, 0, 0) == 0 {
        return None;
    }
    let suffix = completed_suffix(host);
    let line = host.text(
        SC_FORM_SELECT,
        EE_SCREENS_TEXT_FORM_LIST_DATA,
        SC_FORM_SELECT << 24 | SC_CS_SCENARIOS << 16 | (selection & 0xffff),
    );
    Some(without_suffix(&line, &suffix).to_vec())
}

fn append_scenario<H: ScenarioHost>(host: &mut H, text: &[u8]) {
    host.put_text(SC_FORM_SELECT, EE_SCREENS_TEXT_FORM_LIST_APPEND, SC_FORM_SELECT << 16 | SC_CS_SCENARIOS, text);
}

/// The current campaign loaded and its open scenarios listed (completed ones marked), the progress text, the map
/// backdrop (`show_map`), the next scenario selected and the difficulty shown; a campaign that does not load or
/// open reports it and closes the form. `built_in` is the campaign's built-in number (its checksum must match).
fn refresh<H: ScenarioHost>(host: &mut H, built_in: u32, show_map: bool) {
    let f = SC_FORM_SELECT;
    host.call(f, EE_SCREENS_GAME_SHOW_SINGLE_PLAYER, 0, 0, 0, 0);
    host.call(f, EE_SCREENS_OP_FORM_LIST_CLEAR, f, SC_CS_SCENARIOS, 0, 0);
    host.call(f, EE_SCREENS_OP_COMBO_CLEAR, SC_CS_DIFFICULTY, 0, 0, 0);
    for id in SC_IDS_DIFFICULTIES {
        let text = host.string(id);
        host.put_text(f, EE_SCREENS_TEXT_COMBO_APPEND, SC_CS_DIFFICULTY, &text);
    }
    host.call(f, EE_SCREENS_OP_REMOVE_BACKGROUND, SC_BACKGROUND_MAP, 0, 0, 0);
    let name = host.text(f, EE_SCREENS_TEXT_CS_CAMPAIGN_NAME, 0);
    let file = [b"Campaigns\\".as_slice(), &name, &host.text(f, EE_SCREENS_TEXT_CAMPAIGN_EXTENSION, 0)].concat();
    if host.put_text(f, EE_SCREENS_TEXT_CAMPAIGN_LOAD, 0, &name) == 0 {
        let error = host.text(f, EE_SCREENS_TEXT_CAMPAIGN_ERROR, 0);
        close_campaign_message(host, &error, &file);
        return;
    }
    let mut valid = true;
    if built_in != 0 {
        host.call(f, EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM, 0, 0, 0, 0);
        if built_in != 1 && built_in != host.call(f, EE_SCREENS_GAME_CAMPAIGN_CHECKSUM, 0, 0, 0, 0) {
            valid = false;
        }
    }
    if host.put_text(f, EE_SCREENS_TEXT_CS_ARCHIVE, valid as u32, &file) == 0 {
        let error = host.string(SC_IDS_FILE_COULDNT_BE_OPENED);
        close_campaign_message(host, &error, &file);
        return;
    }
    if show_map {
        host.call(f, EE_SCREENS_GAME_CS_MAP_BACKDROP, 0, 0, 0, 0);
    }
    let scenarios = list(&host.text(f, EE_SCREENS_TEXT_CS_SCENARIOS, 0));
    host.call(f, EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM, 0, 0, 0, 0);
    let completed = list(&host.text(f, EE_SCREENS_TEXT_CS_COMPLETED, 0));
    let suffix = completed_suffix(host);
    let all_open = host.call(f, EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET, 0, 0, 0, 0) != 0;
    if !all_open {
        // the completed ones (marked), then the first one not completed
        for i in 0..=completed.len() {
            if i >= scenarios.len() {
                continue;
            }
            if i < completed.len() {
                if scenarios.contains(&completed[i]) {
                    append_scenario(host, &[scenarios[i].as_slice(), &suffix].concat());
                }
            } else {
                append_scenario(host, &scenarios[i]);
                break;
            }
        }
        if completed.is_empty() {
            host.call(f, EE_SCREENS_OP_SHOW, SC_CS_PROGRESS, SC_HIDDEN, 0, 0);
        } else {
            let text = if completed.len() == scenarios.len() {
                host.string(SC_IDS_CAMPAIGN_COMPLETED)
            } else {
                host.text(f, EE_SCREENS_TEXT_CS_PROGRESS, (completed.len() as u32) << 16 | scenarios.len() as u32)
            };
            host.call(f, EE_SCREENS_OP_SHOW, SC_CS_PROGRESS, SC_SHOWN, 0, 0);
            host.put_text(f, EE_SCREENS_TEXT_LABEL_LINES, f << 16 | SC_CS_PROGRESS, &text);
        }
    } else {
        for scenario in &scenarios {
            if completed.contains(scenario) {
                append_scenario(host, &[scenario.as_slice(), &suffix].concat());
            } else {
                append_scenario(host, scenario);
            }
        }
        host.call(f, EE_SCREENS_OP_SHOW, SC_CS_PROGRESS, SC_HIDDEN, 0, 0);
    }
    // the next scenario: the first one not completed when all are open, else the last listed one
    let count = host.call(f, EE_SCREENS_OP_FORM_LIST_COUNT, f, SC_CS_SCENARIOS, 0, 0);
    let mut selection = count;
    if host.call(f, EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET, 0, 0, 0, 0) != 0 {
        let suffix = completed_suffix(host);
        for i in 0..count {
            selection = i + 1;
            let line = host.text(f, EE_SCREENS_TEXT_FORM_LIST_DATA, f << 24 | SC_CS_SCENARIOS << 16 | (i & 0xffff));
            if !completed.iter().any(|c| c.as_slice() == without_suffix(&line, &suffix)) {
                break;
            }
        }
    }
    host.call(f, EE_SCREENS_OP_FORM_LIST_SELECT, f, SC_CS_SCENARIOS, selection.saturating_sub(1), 0);
    let level = host.call(f, EE_SCREENS_GAME_DIFFICULTY, 0, 0, 0, 0);
    host.call(f, EE_SCREENS_OP_COMBO_SELECT, SC_CS_DIFFICULTY, level, 0, 0);
}

/// One page of the intro's text (the briefing, hints or history) under its heading.
fn page<H: ScenarioHost>(host: &mut H, title: u32, which: u32, tab_stop: bool) {
    let f = SC_FORM_INTRO;
    let text = host.string(title);
    host.put_text(f, EE_SCREENS_TEXT_LABEL, SC_SI_TITLE, &text);
    host.call(f, EE_SCREENS_OP_SHOW, SC_SI_TITLE, SC_SHOWN, 0, 0);
    host.call(f, EE_SCREENS_OP_BACKGROUND, SC_BACKGROUND_TEXT, 1, 0, 0);
    if tab_stop {
        host.call(f, EE_SCREENS_OP_TAB_STOP, 1, 0, 0, 0);
    }
    let text = host.text(f, which, 0);
    host.put_text(f, EE_SCREENS_TEXT_LABEL_LINES, f << 16 | SC_SI_TEXT, &text);
}

/// One of the two forms' events; false for another code (nothing called). No lock is held across a host call.
pub fn dispatch<H: ScenarioHost>(host: &mut H, state: &Mutex<ScenarioScreens>, event: u32) -> bool {
    let (f, i) = (SC_FORM_SELECT, SC_FORM_INTRO);
    match event {
        EE_SCREENS_CS_OPEN => {
            let name = host.text(f, EE_SCREENS_TEXT_CS_CAMPAIGN_NAME, 0);
            let built_in = host.put_text(f, EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN, 0, &name);
            host.call(f, EE_SCREENS_OP_SHOW, SC_CS_DIFFICULTY, SC_SHOWN, 0, 0);
            host.call(f, EE_SCREENS_OP_SHOW, SC_CS_DIFFICULTY_LABEL, SC_SHOWN, 0, 0);
            for (control, trigger, code) in [
                (SC_CS_CAMPAIGNS, SC_TRIGGER_SELECTION_CHANGED, EE_SCREENS_CS_SELECT_CAMPAIGN),
                (SC_CS_SCENARIOS, SC_TRIGGER_SELECTION_CHANGED, EE_SCREENS_CS_SELECT_SCENARIO),
                (SC_CS_DIFFICULTY, SC_TRIGGER_SELECTION_CHANGED, EE_SCREENS_CS_DIFFICULTY),
                (SC_CS_START, SC_TRIGGER_BUTTON_UP, EE_SCREENS_CS_START),
                (SC_CS_BACK, SC_TRIGGER_BUTTON_UP, EE_SCREENS_CS_BACK),
            ] {
                host.call(f, EE_SCREENS_OP_TRIGGER, f, control, trigger, code);
            }
            refresh(host, built_in, true);
        }
        EE_SCREENS_CS_SELECT_CAMPAIGN => {
            let selection = host.call(f, EE_SCREENS_OP_FORM_LIST_SELECTION, f, SC_CS_CAMPAIGNS, 0, 0);
            let name =
                host.text(f, EE_SCREENS_TEXT_FORM_LIST_DATA, f << 24 | SC_CS_CAMPAIGNS << 16 | (selection & 0xffff));
            host.put_text(f, EE_SCREENS_TEXT_CAMPAIGN_NAME, 0, &name);
            refresh(host, 0, false);
        }
        EE_SCREENS_CS_SELECT_SCENARIO => {
            // the campaign's description for the selected scenario, whose name is remembered
            let Some(name) = selected_scenario(host) else { return true };
            if host.put_text(f, EE_SCREENS_TEXT_CS_SCENARIO_HEADER, 0, &name) != 0 {
                let mut text = host.text(f, EE_SCREENS_TEXT_CS_DESCRIPTION, 0);
                if text.is_empty() {
                    text = host.string(SC_IDS_NO_DESCRIPTION);
                }
                host.put_text(f, EE_SCREENS_TEXT_LABEL_LINES, f << 16 | SC_CS_DESCRIPTION, &text);
                host.put_text(f, EE_SCREENS_TEXT_CS_SCENARIO_NAME, 0, &name);
            }
        }
        EE_SCREENS_CS_DIFFICULTY => {
            let level = host.combo_selection(f, SC_CS_DIFFICULTY);
            host.call(f, EE_SCREENS_GAME_CS_SET_DIFFICULTY, level, 0, 0, 0);
        }
        EE_SCREENS_CS_START => {
            // the selected scenario's header into the intro form, which opens; one that does not open is reported
            let Some(name) = selected_scenario(host) else { return true };
            if host.put_text(f, EE_SCREENS_TEXT_CS_START, 0, &name) == 2 {
                host.call(f, EE_SCREENS_OP_OPEN_FORM, i, 0, 0, 0);
            } else {
                let error = host.string(SC_IDS_FILE_COULDNT_BE_OPENED);
                close_campaign_message(host, &error, &name);
            }
        }
        EE_SCREENS_CS_BACK | EE_SCREENS_CS_CLOSE_CALLBACK => {
            host.call(f, EE_SCREENS_OP_CLOSE_FORM, 0, 0, 0, 0);
        }
        EE_SCREENS_CS_CLOSE | EE_SCREENS_SI_MOUSE => {}
        EE_SCREENS_SI_BACK => {
            // back to the game, the main game form, the campaign or the scenario load screen
            host.call(i, EE_SCREENS_OP_SHOW, SC_SI_TITLE, SC_HIDDEN, 0, 0);
            let flags = flags(host);
            if flags & EE_SCREENS_SI_FLAG_IN_GAME != 0 {
                host.call(i, EE_SCREENS_OP_CLOSE_FORM, 0, 0, 0, 0);
            } else if flags & EE_SCREENS_SI_FLAG_LOADING != 0 {
                host.call(i, EE_SCREENS_OP_OPEN_MAIN_GAME_FORM, 0, 0, 0, 0);
            } else if flags & EE_SCREENS_SI_FLAG_CAMPAIGN != 0 {
                host.call(i, EE_SCREENS_OP_CLOSE_FORM, 0, 0, 0, 0);
            } else {
                host.call(i, EE_SCREENS_OP_OPEN_FORM, SC_FORM_SCENARIO_LOAD, 0, 0, 0);
            }
        }
        EE_SCREENS_SI_BRIEFING => {
            page(host, SC_IDS_MENU_BRIEFING, EE_SCREENS_TEXT_SI_BRIEFING, true);
            if flags(host) & EE_SCREENS_SI_FLAG_SCROLL != 0 {
                let lines = host.call(i, EE_SCREENS_OP_LABEL_LINE_COUNT, SC_SI_TEXT, 0, 0, 0);
                if lines > 0 {
                    host.call(i, EE_SCREENS_OP_LABEL_START_LINE, SC_SI_TEXT, lines - 1, 0, 0);
                }
                host.call(i, EE_SCREENS_GAME_SI_SET_FLAG, EE_SCREENS_SI_FLAG_SCROLL, 0, 0, 0);
            }
        }
        EE_SCREENS_SI_HINTS => page(host, SC_IDS_MENU_HINTS, EE_SCREENS_TEXT_SI_HINTS, false),
        EE_SCREENS_SI_HISTORY => page(host, SC_IDS_MENU_HISTORY, EE_SCREENS_TEXT_SI_HISTORY, false),
        EE_SCREENS_SI_MAP => {
            host.call(i, EE_SCREENS_OP_SHOW, SC_SI_TITLE, SC_HIDDEN, 0, 0);
            host.call(i, EE_SCREENS_OP_LABEL_CLEAR, SC_SI_TEXT, 0, 0, 0);
            host.call(i, EE_SCREENS_OP_BACKGROUND, SC_BACKGROUND_TEXT, 0, 0, 0);
        }
        EE_SCREENS_SI_CLOSE => {
            // the briefing's blinking and sound stopped, the map backdrop dropped
            host.call(i, EE_SCREENS_GAME_SI_BLINK_OFF, 0, 0, 0, 0);
            host.call(i, EE_SCREENS_GAME_SI_SET_FLAG, EE_SCREENS_SI_FLAG_IN_GAME, 0, 0, 0);
            host.call(i, EE_SCREENS_GAME_SI_SET_FLAG, EE_SCREENS_SI_FLAG_LOADING, 0, 0, 0);
            let sound = state.lock().unwrap().sound;
            if sound != SC_NO_SOUND {
                host.call(i, EE_SCREENS_GAME_SOUND_STOP, sound, 0, 0, 0);
                host.call(i, EE_SCREENS_GAME_SOUND_LATENCY, 200, 0, 0, 0);
            }
            host.call(i, EE_SCREENS_OP_REMOVE_BACKGROUND, SC_BACKGROUND_MAP, 0, 0, 0);
            host.call(i, EE_SCREENS_OP_SLEEP, 500, 0, 0, 0);
        }
        EE_SCREENS_SI_MOVIE => {
            // the movie, the sound when it ends; or the sound at once
            if host.call(i, EE_SCREENS_GAME_SI_OPEN_MOVIE, 0, 0, 0, 0) != 0 {
                host.call(i, EE_SCREENS_OP_GROUP, 0, 0, 0, 0);
                host.call(i, EE_SCREENS_GAME_SI_PLAY_MOVIE, EE_SCREENS_SI_SOUND_OVER, 0, 0, 0);
            } else {
                host.call(i, EE_SCREENS_OP_POST_EVENT, EE_SCREENS_SI_SOUND_OVER, 0, 0, 0);
            }
        }
        EE_SCREENS_SI_SOUND_OVER => {
            // the briefing sound once, then the briefing page
            let sound = state.lock().unwrap().sound;
            if sound != SC_NO_SOUND {
                host.call(i, EE_SCREENS_GAME_SOUND_STOP, sound, 0, 0, 0);
            }
            host.call(i, EE_SCREENS_OP_GROUP, 0, 1, 0, 0);
            if host.call(i, EE_SCREENS_GAME_SI_OPEN_SOUND, 0, 0, 0, 0) != 0 {
                let sound = host.call(i, EE_SCREENS_GAME_SI_PLAY_SOUND, sound, 0, 0, 0);
                state.lock().unwrap().sound = sound;
            }
            host.call(i, EE_SCREENS_OP_POST_EVENT, EE_SCREENS_SI_BRIEFING, 0, 0, 0);
        }
        EE_SCREENS_SI_OPEN => {
            // the map backdrop, the buttons that apply, then the movie or the briefing; no media: start at once
            state.lock().unwrap().sound = SC_NO_SOUND;
            host.call(i, EE_SCREENS_GAME_SI_MAP_BACKDROP, 0, 0, 0, 0);
            host.call(i, EE_SCREENS_OP_BACKGROUND, SC_BACKGROUND_81, 0, 0, 0);
            let map = host.call(i, EE_SCREENS_GAME_SI_OPEN_MAP, 0, 0, 0, 0) != 0;
            host.call(i, EE_SCREENS_OP_SHOW, SC_SI_MAP_BUTTON, if map { SC_SHOWN } else { SC_HIDDEN }, 0, 0);
            let movie = host.call(i, EE_SCREENS_GAME_SI_OPEN_MOVIE, 0, 0, 0, 0) != 0
                || host.call(i, EE_SCREENS_GAME_SI_OPEN_SOUND, 0, 0, 0, 0) != 0;
            host.call(i, EE_SCREENS_OP_SHOW, SC_SI_MOVIE_BUTTON, if movie { SC_SHOWN } else { SC_HIDDEN }, 0, 0);
            let flags = flags(host);
            let in_game = flags & EE_SCREENS_SI_FLAG_IN_GAME != 0;
            host.call(i, EE_SCREENS_OP_SHOW, SC_SI_START_BUTTON, if in_game { SC_HIDDEN } else { SC_SHOWN }, 0, 0);
            let next = if !in_game && flags & EE_SCREENS_SI_FLAG_LOADING == 0 {
                EE_SCREENS_SI_MOVIE
            } else {
                if in_game {
                    host.call(i, EE_SCREENS_GAME_SI_SET_FLAG, EE_SCREENS_SI_FLAG_SCROLL, 1, 0, 0);
                }
                EE_SCREENS_SI_BRIEFING
            };
            let start = host.text(i, EE_SCREENS_TEXT_SI_BRIEFING, 0).is_empty()
                && host.text(i, EE_SCREENS_TEXT_SI_HINTS, 0).is_empty()
                && host.text(i, EE_SCREENS_TEXT_SI_HISTORY, 0).is_empty()
                && host.call(i, EE_SCREENS_GAME_SI_OPEN_MAP, 0, 0, 0, 0) == 0
                && host.call(i, EE_SCREENS_GAME_SI_OPEN_MOVIE, 0, 0, 0, 0) == 0
                && host.call(i, EE_SCREENS_GAME_SI_OPEN_SOUND, 0, 0, 0, 0) == 0;
            if start {
                host.call(i, EE_SCREENS_OP_ADD_TRIGGER, EE_SCREENS_SI_START, 0, 0, 0);
            } else {
                host.call(i, EE_SCREENS_OP_POST_EVENT, next, 0, 0, 0);
            }
        }
        EE_SCREENS_SI_START => {
            // the saved game loaded, or a single player game of the scenario (or the campaign's) started
            host.call(i, EE_SCREENS_OP_SHOW, SC_SI_TITLE, SC_HIDDEN, 0, 0);
            if host.call(i, EE_SCREENS_GAME_SI_GET_SCENARIO, 0, 0, 0, 0) != 0 {
                host.call(i, EE_SCREENS_GAME_MASTER_VOLUME, 0, 0, 0, 0);
                let flags = flags(host);
                if flags & EE_SCREENS_SI_FLAG_LOADING != 0 {
                    host.call(i, EE_SCREENS_GAME_SI_POST_LOAD, 0, 0, 0, 0);
                    host.call(i, EE_SCREENS_GAME_SI_SET_FLAG, EE_SCREENS_SI_FLAG_LOADING, 0, 0, 0);
                } else if flags & EE_SCREENS_SI_FLAG_IN_GAME == 0 {
                    host.call(
                        i,
                        EE_SCREENS_GAME_SI_START_GAME,
                        (flags & EE_SCREENS_SI_FLAG_CAMPAIGN != 0) as u32,
                        0,
                        0,
                        0,
                    );
                }
            }
        }
        _ => return false,
    }
    true
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn suffixes() {
        assert_eq!(without_suffix(b"Scenario 1 S31807", b" S31807"), b"Scenario 1");
        assert_eq!(without_suffix(b"Scenario 1", b" S31807"), b"Scenario 1");
        assert_eq!(without_suffix(b" S1", b" S1"), b"");
        assert_eq!(without_suffix("Zo\u{eb} x Zo\u{eb} x".as_bytes(), b" x"), "Zo\u{eb}".as_bytes());
        assert_eq!(without_suffix(b"ab", b"abc"), b"ab");
    }

    #[test]
    fn events() {
        assert_eq!(scenario_intro_event_for(94), EE_SCREENS_SI_BACK);
        assert_eq!(scenario_intro_event_for(100), EE_SCREENS_SI_START);
        assert_eq!(scenario_intro_event_for(101), 0);
        assert_eq!(scenario_intro_event_for(124), EE_SCREENS_SI_MOUSE);
        assert_eq!(scenario_intro_event_for(137), EE_SCREENS_SI_MOUSE);
        assert_eq!(scenario_intro_event_for(138), 0);
        assert!(is_scenario_event(EE_SCREENS_CS_CLOSE_CALLBACK) && is_scenario_event(EE_SCREENS_SI_MOUSE));
        assert!(!is_scenario_event(0x2109) && !is_scenario_event(0x130c));
    }
}
