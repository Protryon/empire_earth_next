//! The out-of-game menus, whole: the main menu (form 12), the editors' sub-menu (form 9: the scenario and campaign
//! editors, the civilization builder), the single player screen (form 6) and the play campaign screen (form 8: the
//! three campaigns, the learning campaign, a custom campaign). Their events are this module's: the
//! toolkit makes an `EEScreenEvent` (`ee/EEScreenEvent.h`) for each of their controls and form-open events, carrying
//! one of the codes below, and its `ProcessEvent` comes here ([`dispatch`]). What stays C++ is the registration (the
//! control-event factory asks [`event_for_control`]) and the [`MenuHost`] calls: the toolkit's (push, pop, a form's
//! open event, a control's display mode and text, a hot key, a resource string, a message box) and the game's (quit,
//! the external lobby, the editors, the civilization builder, the lobby's options, the local server).
//!
//! The pattern for every form that moves whole (docs/RUST_PLAN.md "R6 game screens", step 2): the form's events are
//! codes `form << 8 | n`; its state is a Rust record ([`Menus`]: the campaigns' descriptions and first scenarios;
//! the player name is the game's, behind the host), locked only around its own reads and writes, never across a
//! host call (a push can run another form's open event at once); one `dispatch` over a host trait whose methods are
//! the narrowest toolkit and game calls, each a trace line in the golden driver's fakes, so the replay below is the
//! driver's trace.
//!
//! Codes past 0xffff name events that are still C++ classes (another form's open event, the main game's media dump
//! hot keys); the host builds those.

use core::ffi::c_ulong;
use std::sync::Mutex;

use crate::single_player::{self, EeScreensSinglePlayerSlot};

// ---- the events (EEScreenEvent::mEvent)

/// The main menu's open event (form event 1).
pub const EE_SCREENS_MAIN_OPEN: u32 = 0x0c01;
pub const EE_SCREENS_MAIN_SINGLE_PLAYER: u32 = 0x0c02;
pub const EE_SCREENS_MAIN_MULTI_PLAYER: u32 = 0x0c03;
pub const EE_SCREENS_MAIN_EXIT: u32 = 0x0c04;
/// The "Learning Scenarios" button, which opens the game settings.
pub const EE_SCREENS_MAIN_GAME_SETTINGS: u32 = 0x0c05;
/// The test button: does nothing.
pub const EE_SCREENS_MAIN_TEST: u32 = 0x0c06;
/// The main menu's editors button: opens the sub-menu (form 9).
pub const EE_SCREENS_EDITORS_OPEN: u32 = 0x0901;
pub const EE_SCREENS_EDITORS_SCENARIO_EDITOR: u32 = 0x0902;
pub const EE_SCREENS_EDITORS_CAMPAIGN_EDITOR: u32 = 0x0903;
pub const EE_SCREENS_EDITORS_MAIN_MENU: u32 = 0x0904;
pub const EE_SCREENS_EDITORS_CIV_BUILDER: u32 = 0x0905;
/// The single player screen's open event (set by the main menu's Single Player).
pub const EE_SCREENS_SP_OPEN: u32 = 0x0601;
pub const EE_SCREENS_SP_CAMPAIGN: u32 = 0x0602;
pub const EE_SCREENS_SP_CIV_CAMPAIGN: u32 = 0x0603;
/// The "Game Settings" control event of the single player screen (the original's Learning Scenarios): does nothing.
pub const EE_SCREENS_SP_LEARNING: u32 = 0x0604;
/// Load Game (also the play campaign screen's Play Saved Game).
pub const EE_SCREENS_SP_LOAD_GAME: u32 = 0x0605;
pub const EE_SCREENS_SP_MAIN_MENU: u32 = 0x0606;
pub const EE_SCREENS_SP_RANDOM_MAP: u32 = 0x0607;
/// The play campaign screen: Back, the three campaigns, the mouse over their buttons, its open event (set by the
/// single player screen's Campaign), Custom Campaign, the learning campaign.
pub const EE_SCREENS_PC_BACK: u32 = 0x0801;
pub const EE_SCREENS_PC_CAMPAIGN_1: u32 = 0x0802;
pub const EE_SCREENS_PC_CAMPAIGN_2: u32 = 0x0803;
pub const EE_SCREENS_PC_CAMPAIGN_3: u32 = 0x0804;
pub const EE_SCREENS_PC_MOUSE_OVER: u32 = 0x0805;
pub const EE_SCREENS_PC_OPEN: u32 = 0x0806;
pub const EE_SCREENS_PC_CUSTOM: u32 = 0x0807;
pub const EE_SCREENS_PC_LEARNING: u32 = 0x0808;

/// The C++ events the menus hang on other forms (built by the host).
pub const EE_SCREENS_CPP_SETTINGS_OPEN: u32 = 0x1_0001; // EEUIEGSEOutOfGameOpen
pub const EE_SCREENS_CPP_LOBBY_SINGLE: u32 = 0x1_0003; // EEUIELEActivateSinglePlayer
pub const EE_SCREENS_CPP_DUMP_MEDIA: u32 = 0x1_0004; // EEUIEMGDumpLoadedMedia(false)
pub const EE_SCREENS_CPP_DUMP_MEDIA_ALL: u32 = 0x1_0005; // EEUIEMGDumpLoadedMedia(true)
pub const EE_SCREENS_CPP_SCENARIOS_OPEN: u32 = 0x1_0006; // EEUIECSSEFormOpen
pub const EE_SCREENS_CPP_SCENARIOS_CLOSE: u32 = 0x1_0007; // EEUIECSSEFormClose
pub const EE_SCREENS_CPP_CUSTOM_OPEN: u32 = 0x1_0008; // EEUIECCCEFormOpen
pub const EE_SCREENS_CPP_CUSTOM_CLOSE: u32 = 0x1_0009; // EEUIECCCEFormClose

// ---- the game's calls (MenuHost::call, call_with)

pub const EE_SCREENS_GAME_QUIT: u32 = 1; // gUserInterface->PostQuit()
pub const EE_SCREENS_GAME_EXTERNAL_LOBBY: u32 = 2; // EEStartExternalLobby()
pub const EE_SCREENS_GAME_EDITOR_MODE_OFF: u32 = 3; // the manager's mInScenarioEditor = false
pub const EE_SCREENS_GAME_SINGLE: u32 = 4; // gGameState.mGameOptions.mIsMultiplayer = false
pub const EE_SCREENS_GAME_MULTI: u32 = 5; // ... = true
pub const EE_SCREENS_GAME_DELETE_EDITOR: u32 = 6; // the manager's DeleteScenarioEditor()
pub const EE_SCREENS_GAME_DELETE_WON: u32 = 7; // delete gWonNet (when there is one), gWonNet = NULL
pub const EE_SCREENS_GAME_CREATE_EDITOR: u32 = 8; // the manager's CreateScenarioEditor()
pub const EE_SCREENS_GAME_START_EDITOR: u32 = 9; // the manager's StartScenarioEditor()
pub const EE_SCREENS_GAME_CIV_BUILDER: u32 = 10; // a fresh custom civilization into OpenCivilizationBuilder
pub const EE_SCREENS_GAME_FORGET_MAIN_FORM: u32 = 11; // gSavedMainGameForm = NULL
pub const EE_SCREENS_GAME_LOBBY_BEGIN: u32 = 12; // the lobby options' write lock, gGameOptions = the last game's
pub const EE_SCREENS_GAME_LOBBY_END: u32 = 13; // the write unlock
pub const EE_SCREENS_GAME_SHOW_SINGLE_PLAYER: u32 = 14; // EEShowSinglePlayerScreen()
pub const EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN: u32 = 15; // (open) the campaign form manager's mAllScenariosOpen

/// The game's texts (MenuHost::text).
pub const EE_SCREENS_TEXT_VERSION: u32 = 1; // gGameVersion
pub const EE_SCREENS_TEXT_PLAYER: u32 = 2; // gPlayerName (put: sets it)
pub const EE_SCREENS_TEXT_CAMPAIGN: u32 = 8; // (0..3) gCampaignAOCRoman, ..Pacific, ..Asian, gCampaignEELearning

// ---- the host's C calls (menus_ffi.rs; EEScreens.cpp): ee_screenscpp_call's toolkit ops (the game's are above),
// ee_screenscpp_text's and ee_screenscpp_put_text's texts

pub const EE_SCREENS_OP_PUSH: u32 = 0x101; // (form)
pub const EE_SCREENS_OP_POP: u32 = 0x102;
pub const EE_SCREENS_OP_SWITCH: u32 = 0x103; // (form)
pub const EE_SCREENS_OP_OPEN_EVENT: u32 = 0x104; // (form, event)
pub const EE_SCREENS_OP_SHOW: u32 = 0x105; // (control, mode)
pub const EE_SCREENS_OP_HOT_KEY: u32 = 0x106; // (key, modifier 1, modifier 2, event)
pub const EE_SCREENS_OP_CLOSE_EVENT: u32 = 0x107; // (form, event)
pub const EE_SCREENS_OP_TRIGGER: u32 = 0x108; // (form, control, trigger, event): SetTriggerEvent
pub const EE_SCREENS_OP_OPEN_FORM: u32 = 0x109; // (form): gUserInterface->OpenForm
pub const EE_SCREENS_OP_ENTITY_INDEX: u32 = 0x10a; // -> the event's entity's control index
pub const EE_SCREENS_TEXT_EDIT: u32 = 3; // (control) an edit box's first line; put: its text
pub const EE_SCREENS_TEXT_STRING: u32 = 4; // (id) a resource string
pub const EE_SCREENS_TEXT_LABEL: u32 = 5; // put (control): a text label's text
pub const EE_SCREENS_TEXT_MESSAGE: u32 = 6; // put (the last flag): the message box
pub const EE_SCREENS_TEXT_SERVER: u32 = 7; // put: the local server for the human of this name
pub const EE_SCREENS_TEXT_STRING_CHECKED: u32 = 9; // (id) a resource string; usize::MAX when it is missing
pub const EE_SCREENS_TEXT_LABEL_LINES: u32 = 10; // put (form << 16 | control): DeleteArray, then AppendText
pub const EE_SCREENS_TEXT_CAMPAIGN_NAME: u32 = 11; // put: the campaign form manager's name
pub const EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION: u32 = 12; // put: its description
pub const EE_SCREENS_TEXT_CAMPAIGN_FIRST: u32 = 13; // put: its first scenario
pub const EE_SCREENS_TEXT_CAMPAIGN_LOAD: u32 = 14; // put (a name): its file loaded -> 1
pub const EE_SCREENS_TEXT_CAMPAIGN_LOADED: u32 = 15; // the loaded campaign's description
pub const EE_SCREENS_TEXT_CAMPAIGN_CIV: u32 = 16; // the loaded campaign's required civilization (its first scenario)

// the forms, controls, keys and strings the menus name (ee/EEUIManager.h, lle/ISKeyCode.h, ee/EEStringIDs.h)
const FORM_GAME_SETTINGS: u32 = 5;
const FORM_SINGLE_PLAYER: u32 = 6;
const FORM_PLAY_CAMPAIGN: u32 = 8;
const FORM_GAME_TOOLS: u32 = 9;
const FORM_SINGLE_PLAYER_LOAD: u32 = 13;
const FORM_SCENARIO_LOAD: u32 = 20;
const FORM_CAMPAIGN_EDITOR: u32 = 24;
const FORM_LOBBY: u32 = 26;
const FORM_CAMPAIGN_SELECT: u32 = 33;
const FORM_CUSTOM_CAMPAIGN: u32 = 34;
const PC_INTRO_LABEL: u32 = 2;
const PC_CAMPAIGN_BUTTONS: [u32; 3] = [3, 4, 5];
const TRIGGER_MOUSE_OVER: u32 = 0x3f2;
const IDS_NO_DESCRIPTION_PROVIDED: u32 = 7124;
const IDS_PLEASE_WAIT: u32 = 31842;
const MAIN_BUILD_LABEL: u32 = 5;
const MAIN_COPYRIGHT_LABEL: u32 = 8;
const SP_NAME_BOX: u32 = 8;
const DISPLAY_SHOWN: u32 = 1;
const KEY_D: u32 = 0x20;
const KEY_CONTROL: u32 = 0xe0;
const KEY_SHIFT: u32 = 0xe1;
const KEY_NONE: u32 = 0x7fff;
const IDS_PLAYER_NAME_IS_INVALID: u32 = 10004;
const IDS_SE_HUMAN: u32 = 20552;
const IDS_COMPUTER: u32 = 30241;

/// The toolkit's and the game's calls the menus make. Texts are the game's UTF-8 bytes.
pub trait MenuHost {
    /// The form's manager pushes form `form`.
    fn push(&mut self, form: u32);
    fn pop(&mut self);
    /// `gUserInterface->SwitchToForm(form)`.
    fn switch_to(&mut self, form: u32);
    /// Form `form`'s open event becomes `event` (a screen event's code or an `EE_SCREENS_CPP_*`).
    fn set_open_event(&mut self, form: u32, event: u32);
    /// Control `control` of the event's form: `SetDisplayMode(mode)`.
    fn show(&mut self, control: u32, mode: u32);
    /// A text label's text.
    fn set_label(&mut self, control: u32, text: &[u8]);
    /// An edit box's first line, and setting its text.
    fn edit_text(&mut self, control: u32) -> Vec<u8>;
    fn set_edit_text(&mut self, control: u32, text: &[u8]);
    /// The event's form: `AddHotKey(key, m1, m2, none, NULL, NULL, event, NULL, 0, false)`.
    fn add_hot_key(&mut self, key: u32, m1: u32, m2: u32, event: u32);
    /// Form `form`'s close event becomes `event`.
    fn set_close_event(&mut self, form: u32, event: u32);
    /// Control `control` of form `form`: `SetTriggerEvent(trigger, event)`.
    fn set_trigger(&mut self, form: u32, control: u32, trigger: u32, event: u32);
    /// `gUserInterface->OpenForm(form)`.
    fn open_form(&mut self, form: u32);
    /// The control index of the event's entity.
    fn entity_index(&mut self) -> u32;
    /// A resource string (whatever `GetString` left when it fails), and one that may be missing.
    fn string(&mut self, id: u32) -> Vec<u8>;
    fn string_checked(&mut self, id: u32) -> Option<Vec<u8>>;
    /// The message box (`DisplayMessage(text, true, NULL, 0, last)`).
    fn message(&mut self, text: &[u8], last: bool);
    /// A text label of form `form`: its lines deleted, then `text` appended.
    fn set_label_lines(&mut self, form: u32, control: u32, text: &[u8]);
    /// `EE_SCREENS_TEXT_*` (`arg`: the campaign).
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// The campaign form manager's name, description or first scenario (`EE_SCREENS_TEXT_CAMPAIGN_*`).
    fn set_campaign(&mut self, which: u32, text: &[u8]);
    /// The campaign `name`'s file: its description and first scenario when it loads.
    fn campaign_info(&mut self, name: &[u8]) -> Option<(Vec<u8>, Vec<u8>)>;
    fn set_player_name(&mut self, name: &[u8]);
    /// `EE_SCREENS_GAME_*`.
    fn call(&mut self, op: u32) {
        self.call_with(op, 0)
    }
    fn call_with(&mut self, op: u32, arg: u32);
    /// The random map's lobby options (inside `LOBBY_BEGIN`): not multiplayer, the slots from the table with the
    /// human's and the computer's names; then the write unlock.
    fn lobby_fill(&mut self, slots: &[EeScreensSinglePlayerSlot; 16], human: &[u8], computer: &[u8]);
    /// A new local server (the old communications deleted), its threads, and the human's connection request.
    fn start_local_server(&mut self, human: &[u8]);
}

/// The menus' event for a control event id (`dbuicontrolevents`), 0 for another form's: the original's factories
/// `EECreateMainScreenEvent`, `EECreateSinglePlayerEvent` and `EECreatePlayCampaignEvent`.
pub fn event_for_control(id: c_ulong) -> u32 {
    match id {
        28 => EE_SCREENS_MAIN_SINGLE_PLAYER,
        0x1d => EE_SCREENS_EDITORS_SCENARIO_EDITOR,
        30 => EE_SCREENS_MAIN_MULTI_PLAYER,
        31 => EE_SCREENS_MAIN_EXIT,
        32 => EE_SCREENS_MAIN_GAME_SETTINGS,
        33 => EE_SCREENS_MAIN_TEST,
        35 => EE_SCREENS_SP_CAMPAIGN,
        36 => EE_SCREENS_SP_CIV_CAMPAIGN,
        37 => EE_SCREENS_SP_LEARNING,
        38 => EE_SCREENS_SP_LOAD_GAME,
        39 => EE_SCREENS_SP_MAIN_MENU,
        40 => EE_SCREENS_SP_RANDOM_MAP,
        85 => EE_SCREENS_PC_BACK,
        86 => EE_SCREENS_PC_CAMPAIGN_1,
        87 => EE_SCREENS_PC_CAMPAIGN_2,
        88 => EE_SCREENS_PC_CAMPAIGN_3,
        90 => EE_SCREENS_SP_LOAD_GAME,
        92 => EE_SCREENS_PC_CUSTOM,
        93 => EE_SCREENS_PC_LEARNING,
        186 => EE_SCREENS_EDITORS_OPEN,
        187 => EE_SCREENS_EDITORS_SCENARIO_EDITOR,
        188 => EE_SCREENS_EDITORS_MAIN_MENU,
        189 => EE_SCREENS_EDITORS_CAMPAIGN_EDITOR,
        240 => EE_SCREENS_EDITORS_CIV_BUILDER,
        _ => 0,
    }
}

/// Whether `event` is one of the menus'.
pub fn is_menu_event(event: u32) -> bool {
    matches!(event >> 8, 0x0c | 0x09 | 0x06 | 0x08) && (event & 0xff) != 0
}

/// The menus' state: the campaigns' descriptions and first scenarios (Roman, Pacific, Asian, learning), as the play
/// campaign screen loaded them (the original's file statics; its unused demo campaign's are gone).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Menus {
    pub descriptions: [Vec<u8>; 4],
    pub first_scenarios: [Vec<u8>; 4],
}

/// Each character's first byte, as `EEIsValidName` reads a `UWideString` (`operator[]` over `PlatMbsNInc`).
fn first_bytes(text: &[u8]) -> Vec<u8> {
    ee_plat_core::utf8::chars(text).map(|(at, _, _)| text[at]).collect()
}

/// The single player screen's typed name into the player name, unless the box is empty (`SPESavePlayerName`).
fn save_player_name<H: MenuHost>(host: &mut H) {
    let name = host.edit_text(SP_NAME_BOX);
    if !name.is_empty() {
        host.set_player_name(&name);
    }
}

/// Opening a form from the menus outside the scenario editor, single or multiplayer.
fn leave_editor<H: MenuHost>(host: &mut H, multi: bool) {
    host.call(EE_SCREENS_GAME_EDITOR_MODE_OFF);
    host.call(if multi { EE_SCREENS_GAME_MULTI } else { EE_SCREENS_GAME_SINGLE });
}

/// The random map: you against one computer in the lobby on a new local server; an empty name is the default
/// human's, an invalid one shows a message and changes nothing but the lobby's options (the last game's) and the
/// player name.
fn random_map<H: MenuHost>(host: &mut H) {
    host.call(EE_SCREENS_GAME_LOBBY_BEGIN);
    let typed = host.edit_text(SP_NAME_BOX);
    let name = if typed.is_empty() {
        host.string(IDS_SE_HUMAN)
    } else {
        host.set_player_name(&typed);
        typed
    };
    if single_player::valid_name(&first_bytes(&name)) {
        let computer = host.string(IDS_COMPUTER);
        host.lobby_fill(&single_player::random_map_slots(), &name, &computer);
        host.set_open_event(FORM_LOBBY, EE_SCREENS_CPP_LOBBY_SINGLE);
        host.start_local_server(&name);
    } else {
        let text = host.string(IDS_PLAYER_NAME_IS_INVALID);
        host.message(&text, true);
        host.call(EE_SCREENS_GAME_LOBBY_END);
    }
}

fn lock(state: &Mutex<Menus>) -> std::sync::MutexGuard<'_, Menus> {
    state.lock().unwrap_or_else(|e| e.into_inner())
}

/// A campaign's description and first scenario from its file (`LoadCampaignInfo`); a campaign that does not load
/// gets the no-description text and keeps its first scenario.
fn load_campaign_info<H: MenuHost>(host: &mut H, state: &Mutex<Menus>, campaign: usize) {
    let name = host.text(EE_SCREENS_TEXT_CAMPAIGN, campaign as u32);
    match host.campaign_info(&name) {
        Some((description, first)) => {
            let mut s = lock(state);
            s.descriptions[campaign] = description;
            s.first_scenarios[campaign] = first;
        }
        None => {
            let text = host.string(IDS_NO_DESCRIPTION_PROVIDED);
            lock(state).descriptions[campaign] = text;
        }
    }
}

/// Opens a campaign's scenario choice form with its name, description and first scenario (every scenario open for
/// the learning campaign), with Please wait while it opens (`StartCampaign`).
fn start_campaign<H: MenuHost>(host: &mut H, state: &Mutex<Menus>, campaign: usize, learning: bool) {
    let name = host.text(EE_SCREENS_TEXT_CAMPAIGN, campaign as u32);
    let (description, first) = {
        let s = lock(state);
        (s.descriptions[campaign].clone(), s.first_scenarios[campaign].clone())
    };
    host.set_campaign(EE_SCREENS_TEXT_CAMPAIGN_NAME, &name);
    host.set_campaign(EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION, &description);
    host.call_with(EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN, learning as u32);
    host.set_campaign(EE_SCREENS_TEXT_CAMPAIGN_FIRST, &first);
    host.set_open_event(FORM_CAMPAIGN_SELECT, EE_SCREENS_CPP_SCENARIOS_OPEN);
    host.set_close_event(FORM_CAMPAIGN_SELECT, EE_SCREENS_CPP_SCENARIOS_CLOSE);
    match host.string_checked(IDS_PLEASE_WAIT) {
        Some(text) => {
            host.message(&text, false);
            host.open_form(FORM_CAMPAIGN_SELECT);
        }
        None => host.switch_to(FORM_CAMPAIGN_SELECT),
    }
}

/// One of the menus' events over their state; false for another code (nothing called).
pub fn dispatch<H: MenuHost>(host: &mut H, state: &Mutex<Menus>, event: u32) -> bool {
    match event {
        EE_SCREENS_MAIN_OPEN => {
            host.show(MAIN_COPYRIGHT_LABEL, DISPLAY_SHOWN);
            let version = host.text(EE_SCREENS_TEXT_VERSION, 0);
            host.set_label(MAIN_BUILD_LABEL, &version);
            host.add_hot_key(KEY_D, KEY_CONTROL, KEY_NONE, EE_SCREENS_CPP_DUMP_MEDIA);
            host.add_hot_key(KEY_D, KEY_CONTROL, KEY_SHIFT, EE_SCREENS_CPP_DUMP_MEDIA_ALL);
            host.call(EE_SCREENS_GAME_DELETE_EDITOR);
            host.call(EE_SCREENS_GAME_DELETE_WON);
        }
        EE_SCREENS_MAIN_GAME_SETTINGS => {
            host.set_open_event(FORM_GAME_SETTINGS, EE_SCREENS_CPP_SETTINGS_OPEN);
            host.push(FORM_GAME_SETTINGS);
        }
        EE_SCREENS_MAIN_EXIT => host.call(EE_SCREENS_GAME_QUIT),
        EE_SCREENS_MAIN_SINGLE_PLAYER => {
            leave_editor(host, false);
            host.set_open_event(FORM_SINGLE_PLAYER, EE_SCREENS_SP_OPEN);
            host.push(FORM_SINGLE_PLAYER);
        }
        EE_SCREENS_MAIN_MULTI_PLAYER => {
            // the original's multiplayer form branch could never run (EEIsMultiplayerGame(false) is always true)
            leave_editor(host, true);
            host.call(EE_SCREENS_GAME_EXTERNAL_LOBBY);
        }
        EE_SCREENS_MAIN_TEST | EE_SCREENS_SP_LEARNING => {}
        EE_SCREENS_EDITORS_OPEN => {
            leave_editor(host, false);
            host.push(FORM_GAME_TOOLS);
        }
        EE_SCREENS_EDITORS_CAMPAIGN_EDITOR => {
            leave_editor(host, false);
            host.push(FORM_CAMPAIGN_EDITOR);
        }
        EE_SCREENS_EDITORS_MAIN_MENU => host.pop(),
        EE_SCREENS_EDITORS_SCENARIO_EDITOR => {
            host.call(EE_SCREENS_GAME_CREATE_EDITOR);
            host.call(EE_SCREENS_GAME_START_EDITOR);
        }
        EE_SCREENS_EDITORS_CIV_BUILDER => host.call(EE_SCREENS_GAME_CIV_BUILDER),
        EE_SCREENS_SP_OPEN => {
            let name = host.text(EE_SCREENS_TEXT_PLAYER, 0);
            host.set_edit_text(SP_NAME_BOX, &name);
        }
        EE_SCREENS_SP_CAMPAIGN => {
            save_player_name(host);
            host.set_open_event(FORM_PLAY_CAMPAIGN, EE_SCREENS_PC_OPEN);
            host.switch_to(FORM_PLAY_CAMPAIGN);
        }
        EE_SCREENS_SP_CIV_CAMPAIGN => {
            host.call(EE_SCREENS_GAME_FORGET_MAIN_FORM);
            save_player_name(host);
            host.push(FORM_SCENARIO_LOAD);
        }
        EE_SCREENS_SP_LOAD_GAME => host.push(FORM_SINGLE_PLAYER_LOAD),
        EE_SCREENS_SP_MAIN_MENU => {
            save_player_name(host);
            host.pop();
        }
        EE_SCREENS_SP_RANDOM_MAP => random_map(host),
        EE_SCREENS_PC_BACK => {
            host.pop();
            host.call(EE_SCREENS_GAME_SHOW_SINGLE_PLAYER);
        }
        EE_SCREENS_PC_CAMPAIGN_1 | EE_SCREENS_PC_CAMPAIGN_2 | EE_SCREENS_PC_CAMPAIGN_3 => {
            start_campaign(host, state, (event - EE_SCREENS_PC_CAMPAIGN_1) as usize, false)
        }
        EE_SCREENS_PC_MOUSE_OVER => {
            // the description of the campaign whose button the mouse is over (another control's: none)
            let index = host.entity_index();
            let text = match PC_CAMPAIGN_BUTTONS.iter().position(|&b| b == index) {
                Some(campaign) => lock(state).descriptions[campaign].clone(),
                None => Vec::new(),
            };
            host.set_label_lines(FORM_PLAY_CAMPAIGN, PC_INTRO_LABEL, &text);
        }
        EE_SCREENS_PC_OPEN => {
            for campaign in 0..3 {
                load_campaign_info(host, state, campaign);
            }
            for button in PC_CAMPAIGN_BUTTONS {
                host.set_trigger(FORM_PLAY_CAMPAIGN, button, TRIGGER_MOUSE_OVER, EE_SCREENS_PC_MOUSE_OVER);
            }
        }
        EE_SCREENS_PC_CUSTOM => {
            host.set_open_event(FORM_CUSTOM_CAMPAIGN, EE_SCREENS_CPP_CUSTOM_OPEN);
            host.set_close_event(FORM_CUSTOM_CAMPAIGN, EE_SCREENS_CPP_CUSTOM_CLOSE);
            host.switch_to(FORM_CUSTOM_CAMPAIGN);
        }
        EE_SCREENS_PC_LEARNING => {
            load_campaign_info(host, state, 3);
            start_campaign(host, state, 3, true);
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

    /// A host that records its calls.
    #[derive(Default)]
    struct Rec {
        calls: Vec<String>,
        box_text: Vec<u8>,
        entity: u32,
        missing: bool,
    }
    impl MenuHost for Rec {
        fn push(&mut self, form: u32) {
            self.calls.push(format!("push {form}"));
        }
        fn pop(&mut self) {
            self.calls.push("pop".into());
        }
        fn switch_to(&mut self, form: u32) {
            self.calls.push(format!("switch {form}"));
        }
        fn set_open_event(&mut self, form: u32, event: u32) {
            self.calls.push(format!("open {form} {event:#x}"));
        }
        fn show(&mut self, control: u32, mode: u32) {
            self.calls.push(format!("show {control} {mode}"));
        }
        fn set_label(&mut self, control: u32, text: &[u8]) {
            self.calls.push(format!("label {control} {}", String::from_utf8_lossy(text)));
        }
        fn edit_text(&mut self, _: u32) -> Vec<u8> {
            self.box_text.clone()
        }
        fn set_edit_text(&mut self, control: u32, text: &[u8]) {
            self.calls.push(format!("edit {control} {}", String::from_utf8_lossy(text)));
        }
        fn add_hot_key(&mut self, key: u32, m1: u32, m2: u32, event: u32) {
            self.calls.push(format!("hotkey {key:#x} {m1:#x} {m2:#x} {event:#x}"));
        }
        fn set_close_event(&mut self, form: u32, event: u32) {
            self.calls.push(format!("close {form} {event:#x}"));
        }
        fn set_trigger(&mut self, form: u32, control: u32, trigger: u32, event: u32) {
            self.calls.push(format!("trigger {form} {control} {trigger:#x} {event:#x}"));
        }
        fn open_form(&mut self, form: u32) {
            self.calls.push(format!("openform {form}"));
        }
        fn entity_index(&mut self) -> u32 {
            self.entity
        }
        fn string(&mut self, id: u32) -> Vec<u8> {
            format!("S{id}").into_bytes()
        }
        fn string_checked(&mut self, id: u32) -> Option<Vec<u8>> {
            (!self.missing).then(|| self.string(id))
        }
        fn message(&mut self, text: &[u8], last: bool) {
            self.calls.push(format!("message {} {}", String::from_utf8_lossy(text), last as u8));
        }
        fn set_label_lines(&mut self, form: u32, control: u32, text: &[u8]) {
            self.calls.push(format!("lines {form} {control} {}", String::from_utf8_lossy(text)));
        }
        fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
            format!("T{which}.{arg}").into_bytes()
        }
        fn set_campaign(&mut self, which: u32, text: &[u8]) {
            self.calls.push(format!("campaign {which} {}", String::from_utf8_lossy(text)));
        }
        fn campaign_info(&mut self, name: &[u8]) -> Option<(Vec<u8>, Vec<u8>)> {
            let n = String::from_utf8_lossy(name).into_owned();
            (!n.ends_with(".1")).then(|| (format!("D{n}").into_bytes(), format!("F{n}").into_bytes()))
        }
        fn set_player_name(&mut self, name: &[u8]) {
            self.calls.push(format!("player {}", String::from_utf8_lossy(name)));
        }
        fn call_with(&mut self, op: u32, arg: u32) {
            self.calls.push(if arg == 0 { format!("call {op}") } else { format!("call {op} {arg}") });
        }
        fn lobby_fill(&mut self, slots: &[EeScreensSinglePlayerSlot; 16], human: &[u8], computer: &[u8]) {
            assert_eq!(slots, &single_player::random_map_slots());
            self.calls.push(format!("fill {} {}", String::from_utf8_lossy(human), String::from_utf8_lossy(computer)));
        }
        fn start_local_server(&mut self, human: &[u8]) {
            self.calls.push(format!("server {}", String::from_utf8_lossy(human)));
        }
    }

    fn run(event: u32, typed: &str) -> Vec<String> {
        let mut h = Rec { box_text: typed.as_bytes().to_vec(), ..Default::default() };
        assert!(dispatch(&mut h, &Mutex::default(), event));
        h.calls
    }

    #[test]
    fn the_control_table() {
        // the original factories' cases, and nothing else in their ranges
        let mine: Vec<c_ulong> = (0..300).filter(|&i| event_for_control(i) != 0).collect();
        assert_eq!(
            mine,
            [28, 29, 30, 31, 32, 33, 35, 36, 37, 38, 39, 40, 85, 86, 87, 88, 90, 92, 93, 186, 187, 188, 189, 240]
        );
        assert_eq!(event_for_control(90), event_for_control(38));
        for i in 0..300 {
            let e = event_for_control(i);
            assert!(e == 0 || is_menu_event(e), "{i}");
        }
        assert_eq!(event_for_control(29), event_for_control(187));
        assert!(!is_menu_event(0) && !is_menu_event(0x0600) && !is_menu_event(EE_SCREENS_CPP_LOBBY_SINGLE));
    }

    #[test]
    fn events_outside_the_menus_do_nothing() {
        let mut h = Rec::default();
        assert!(!dispatch(&mut h, &Mutex::default(), 0x0500));
        assert!(!dispatch(&mut h, &Mutex::default(), EE_SCREENS_CPP_SETTINGS_OPEN));
        assert!(h.calls.is_empty());
        assert!(run(EE_SCREENS_MAIN_TEST, "").is_empty());
        assert!(run(EE_SCREENS_SP_LEARNING, "").is_empty());
    }

    #[test]
    fn the_main_menu() {
        assert_eq!(
            run(EE_SCREENS_MAIN_OPEN, ""),
            [
                "show 8 1",
                "label 5 T1.0",
                "hotkey 0x20 0xe0 0x7fff 0x10004",
                "hotkey 0x20 0xe0 0xe1 0x10005",
                "call 6",
                "call 7"
            ]
        );
        assert_eq!(run(EE_SCREENS_MAIN_SINGLE_PLAYER, ""), ["call 3", "call 4", "open 6 0x601", "push 6"]);
        assert_eq!(run(EE_SCREENS_MAIN_MULTI_PLAYER, ""), ["call 3", "call 5", "call 2"]);
        assert_eq!(run(EE_SCREENS_MAIN_GAME_SETTINGS, ""), ["open 5 0x10001", "push 5"]);
    }

    #[test]
    fn the_single_player_screen() {
        assert_eq!(run(EE_SCREENS_SP_MAIN_MENU, ""), ["pop"]);
        assert_eq!(run(EE_SCREENS_SP_MAIN_MENU, "Max"), ["player Max", "pop"]);
        assert_eq!(
            run(EE_SCREENS_SP_RANDOM_MAP, ""),
            ["call 12", "fill S20552 S30241", "open 26 0x10003", "server S20552"]
        );
        assert_eq!(run(EE_SCREENS_SP_RANDOM_MAP, " x"), ["call 12", "player  x", "message S10004 1", "call 13"]);
        // a name's characters are checked by their first bytes: a lone lead byte is a character of its own
        assert_eq!(run(EE_SCREENS_SP_RANDOM_MAP, "Zo\u{eb}")[2], "fill Zo\u{eb} S30241");
    }

    #[test]
    fn the_play_campaign_screen() {
        let state = Mutex::default();
        let mut h = Rec { entity: 4, ..Default::default() };
        // the descriptions are loaded on open (the Pacific campaign's file does not load), then shown and used
        assert!(dispatch(&mut h, &state, EE_SCREENS_PC_OPEN));
        assert!(dispatch(&mut h, &state, EE_SCREENS_PC_MOUSE_OVER));
        assert!(dispatch(&mut h, &state, EE_SCREENS_PC_CAMPAIGN_1));
        assert_eq!(
            h.calls,
            [
                "trigger 8 3 0x3f2 0x805",
                "trigger 8 4 0x3f2 0x805",
                "trigger 8 5 0x3f2 0x805",
                "lines 8 2 S7124",
                "campaign 11 T8.0",
                "campaign 12 DT8.0",
                "call 15",
                "campaign 13 FT8.0",
                "open 33 0x10006",
                "close 33 0x10007",
                "message S31842 0",
                "openform 33"
            ]
        );
        h.calls.clear();
        h.missing = true;
        h.entity = 6;
        assert!(dispatch(&mut h, &state, EE_SCREENS_PC_MOUSE_OVER));
        assert!(dispatch(&mut h, &state, EE_SCREENS_PC_LEARNING));
        assert!(dispatch(&mut h, &state, EE_SCREENS_PC_CUSTOM));
        assert!(dispatch(&mut h, &state, EE_SCREENS_PC_BACK));
        assert_eq!(
            h.calls,
            [
                "lines 8 2 ",
                "campaign 11 T8.3",
                "campaign 12 DT8.3",
                "call 15 1",
                "campaign 13 FT8.3",
                "open 33 0x10006",
                "close 33 0x10007",
                "switch 33",
                "open 34 0x10008",
                "close 34 0x10009",
                "switch 34",
                "pop",
                "call 14"
            ]
        );
        assert_eq!(state.lock().unwrap().first_scenarios[1], b"");
    }

    #[test]
    fn first_bytes_are_the_characters_leads() {
        assert_eq!(first_bytes("a\u{eb}b".as_bytes()), [b'a', 0xc3, b'b']);
        assert_eq!(first_bytes(&[0xc3]), [0xc3]);
        assert_eq!(first_bytes(&[0x80, b'a']), [0x80, b'a']);
    }
}
