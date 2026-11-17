//! The scenario editor's player tab whole in Rust (`EESEPlayerScreen` with its player rows `EESEPlayerRow` and its
//! game options page `EESEGamePage`, once `EESERBUnitTypes.cpp`): the controls it makes (a table per page: kind,
//! id, text, font, place, size, alignment, opacity, control group), its pages, the rows' show, hide, copy, load,
//! save and apply, the game page's diplomacy grid, its load and save, and the store of the edits. The controls stay
//! C++ objects (the toolkit's and the editor's control classes); the object holds them as handles and drives them
//! through one host, `ee_screenscpp_sepl` (`Scenario Editor/EESEPlayerScreen.cpp`).
//!
//! A host call can come back into the object (checking a page tab runs `SelectPage`): the object is only ever used
//! through `&self`, its few changing numbers are `Cell`s, the handles are set once when it is made.

use core::cell::Cell;

// ---- the control kinds the host makes (`EeScreensSeplCtl::kind`)

/// `UITextLabel`.
pub const EE_SCREENS_SEPL_LABEL: u32 = 1;
/// `UIEditBox` (extra: max characters, numeric).
pub const EE_SCREENS_SEPL_EDIT: u32 = 2;
/// `UIComboBox` (extra: the lines (`EE_SCREENS_SEPL_LINES_*`), the first selection or -1).
pub const EE_SCREENS_SEPL_COMBO: u32 = 3;
/// `EESECBMaxPlayer` (the player counts 2..16 as lines).
pub const EE_SCREENS_SEPL_MAX_PLAYER: u32 = 4;
/// `UICheckBox`.
pub const EE_SCREENS_SEPL_CHECK: u32 = 5;
/// `UISlider` (extra: the range's low and high, the page size, the value).
pub const EE_SCREENS_SEPL_SLIDER: u32 = 6;
/// `EESERBUnitTypes` (extra: checked).
pub const EE_SCREENS_SEPL_UNIT_TYPE: u32 = 7;
/// `EESEUITechListBox`.
pub const EE_SCREENS_SEPL_TECH_LIST: u32 = 8;
/// `EESEBSelect`.
pub const EE_SCREENS_SEPL_SELECT: u32 = 9;
/// `EESETab` (extra: checked).
pub const EE_SCREENS_SEPL_TAB: u32 = 10;
/// `EESEPSSetPlayers`.
pub const EE_SCREENS_SEPL_SET_PLAYERS: u32 = 11;
/// `EEUIBColor` (extra: the player).
pub const EE_SCREENS_SEPL_COLOR: u32 = 12;
/// `EESEUIBDiplomacy`.
pub const EE_SCREENS_SEPL_DIPLOMACY: u32 = 13;
/// `UIRadioButton` (extra: checked).
pub const EE_SCREENS_SEPL_RADIO: u32 = 14;
/// `UITriStateCheckBox`.
pub const EE_SCREENS_SEPL_TRISTATE: u32 = 15;
/// `EESETSAEditBox` (the object screen's counts; extra: max characters, numeric).
pub const EE_SCREENS_SEPL_OBJ_EDIT: u32 = 16;
/// A combo box of the trigger screens (extra: the class, the lines, the border, always dirty): its text box's
/// opacity, always dirty, the text colour, the selected line's colour, its opacity, in that order.
pub const EE_SCREENS_SEPL_LIST_COMBO: u32 = 17;
/// `EESETSOUnitList` (the object screen's picked units).
pub const EE_SCREENS_SEPL_UNIT_LIST: u32 = 18;
/// `UIButton` with a text (extra: the border): its opacity, text in the font, text colour.
pub const EE_SCREENS_SEPL_BUTTON: u32 = 19;
/// `UIButton` with the manager's scroll bar arrows (extra: 0 left, 1 right).
pub const EE_SCREENS_SEPL_ARROW: u32 = 20;

/// A combo box's first lines: none, the epochs' titles.
pub const EE_SCREENS_SEPL_LINES_NONE: i32 = 0;
pub const EE_SCREENS_SEPL_LINES_EPOCHS: i32 = 1;

/// The texts (`EeScreensSeplCtl::text`): none (""), a resource string, the number printed ("%ld"), the resource
/// string then the number ("%s %ld"), the number then the resource string ("%ld %s").
pub const EE_SCREENS_SEPL_TEXT_NONE: u32 = 0;
pub const EE_SCREENS_SEPL_TEXT_STRING: u32 = 1;
pub const EE_SCREENS_SEPL_TEXT_NUMBER: u32 = 2;
pub const EE_SCREENS_SEPL_TEXT_STRING_NUMBER: u32 = 3;
pub const EE_SCREENS_SEPL_TEXT_NUMBER_STRING: u32 = 4;
/// The character `number & 0xff`, `number >> 8` times (at least once).
pub const EE_SCREENS_SEPL_TEXT_CHAR: u32 = 5;

/// The fonts: the screen's, `gU531930`, `gU531934`, `gU531938`, `gFontArial18`.
pub const EE_SCREENS_SEPL_FONT_SCREEN: u32 = 0;
pub const EE_SCREENS_SEPL_FONT_530: u32 = 1;
pub const EE_SCREENS_SEPL_FONT_534: u32 = 2;
pub const EE_SCREENS_SEPL_FONT_538: u32 = 3;
pub const EE_SCREENS_SEPL_FONT_ARIAL18: u32 = 4;

/// `EeScreensSeplCtl::flags`: the text yellow and transparent; the rectangle given as left, top, right, bottom;
/// the height the font's (`h` added).
pub const EE_SCREENS_SEPL_FG: u32 = 1;
pub const EE_SCREENS_SEPL_LTRB: u32 = 2;
pub const EE_SCREENS_SEPL_FONT_HEIGHT: u32 = 4;
/// A slider's range set before its page size (else the page size first); its value set unless `i32::MIN`.
pub const EE_SCREENS_SEPL_RANGE_FIRST: u32 = 8;

/// No alignment set; no opacity set.
pub const EE_SCREENS_SEPL_NONE: i32 = -1;

/// One control as the host makes it: `new` of the kind's class at the index, the alignment, the opacities, the
/// text colour, then `AddControl` to each control group of `groups` (a bit per group).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeScreensSeplCtl {
    pub kind: u32,
    pub id: u32,
    pub index: u32,
    pub font: u32,
    pub text: u32,
    pub string: u32,
    pub number: i32,
    pub flags: u32,
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32,
    pub align: i32,
    /// the background opacity (`SetBgOpacity`), the text box's or scroll bar's (`SetTextBoxBgOpacity`,
    /// `SetScrollBarBgOpacity`); negative: not set
    pub bg: f32,
    pub bg2: f32,
    pub groups: u32,
    pub extra: [i32; 4],
    /// a combo box's list's height and its text's characters (400 and 0 unless set)
    pub list_height: i32,
    pub max_chars: i32,
}

impl EeScreensSeplCtl {
    pub(crate) fn new(kind: u32, id: u32, x: f32, y: f32, w: f32, h: f32) -> Self {
        EeScreensSeplCtl {
            kind,
            id,
            index: 0,
            font: EE_SCREENS_SEPL_FONT_SCREEN,
            text: EE_SCREENS_SEPL_TEXT_NONE,
            string: 0,
            number: 0,
            flags: 0,
            x,
            y,
            w,
            h,
            align: EE_SCREENS_SEPL_NONE,
            bg: -1.0,
            bg2: -1.0,
            groups: 0,
            extra: [0; 4],
            list_height: 400,
            max_chars: 0,
        }
    }
    /// A combo box's list `height` high, its text up to `chars` characters.
    pub(crate) fn list(mut self, height: i32, chars: i32) -> Self {
        self.list_height = height;
        self.max_chars = chars;
        self
    }
    pub(crate) fn font(mut self, f: u32) -> Self {
        self.font = f;
        self
    }
    pub(crate) fn string(mut self, id: u32) -> Self {
        self.text = EE_SCREENS_SEPL_TEXT_STRING;
        self.string = id;
        self
    }
    pub(crate) fn text(mut self, kind: u32, id: u32, n: i32) -> Self {
        self.text = kind;
        self.string = id;
        self.number = n;
        self
    }
    pub(crate) fn flags(mut self, f: u32) -> Self {
        self.flags |= f;
        self
    }
    pub(crate) fn align(mut self, a: i32) -> Self {
        self.align = a;
        self
    }
    pub(crate) fn bg(mut self, bg: f32) -> Self {
        self.bg = bg;
        self
    }
    pub(crate) fn bg2(mut self, bg: f32) -> Self {
        self.bg2 = bg;
        self
    }
    pub(crate) fn group(mut self, g: u32) -> Self {
        self.groups |= 1 << g;
        self
    }
    pub(crate) fn extra(mut self, e: [i32; 4]) -> Self {
        self.extra = e;
        self
    }
}

// ---- the host's ops (`ee_screenscpp_sepl`: `p` three handles, `i` four integers, `f` four floats)

/// p0 made from the control record p1 (an `EeScreensSeplCtl`) on the form p2 -> the handle in p0.
pub const EE_SCREENS_SEPL_OP_MAKE: u32 = 1;
/// The control p0 deleted (none: nothing).
pub const EE_SCREENS_SEPL_OP_DELETE: u32 = 2;
/// The control p0's rectangle -> f0..f3 (left, top, right, bottom).
pub const EE_SCREENS_SEPL_OP_RECT: u32 = 3;
/// The control p0's display mode set to i0 / -> its display mode.
pub const EE_SCREENS_SEPL_OP_SET_DISPLAY: u32 = 4;
pub const EE_SCREENS_SEPL_OP_DISPLAY: u32 = 5;
/// The check box p0 checked (i0) / -> whether it is.
pub const EE_SCREENS_SEPL_OP_SET_CHECKED: u32 = 6;
pub const EE_SCREENS_SEPL_OP_CHECKED: u32 = 7;
/// The combo box p0 emptied / given a line (text i0, string i1, number i2 as `EE_SCREENS_SEPL_TEXT_*`).
pub const EE_SCREENS_SEPL_OP_COMBO_CLEAR: u32 = 8;
pub const EE_SCREENS_SEPL_OP_COMBO_APPEND: u32 = 9;
/// -> the combo box p0's selection (`GetCurrentSelection`; -1 left when it has none) / its list box's.
pub const EE_SCREENS_SEPL_OP_COMBO_SELECTION: u32 = 10;
pub const EE_SCREENS_SEPL_OP_LIST_SELECTION: u32 = 11;
/// The combo box p0's selection set to i0.
pub const EE_SCREENS_SEPL_OP_SET_SELECTION: u32 = 12;
/// The edit box p0's text set to the number i0 ("%lu") / to the edit box p1's first line / -> its first line as
/// an integer (`GetTextAsInt`).
pub const EE_SCREENS_SEPL_OP_EDIT_NUMBER: u32 = 13;
pub const EE_SCREENS_SEPL_OP_EDIT_COPY: u32 = 14;
pub const EE_SCREENS_SEPL_OP_EDIT_INT: u32 = 15;
/// -> the slider p0's value / its value set to i0.
pub const EE_SCREENS_SEPL_OP_SLIDER: u32 = 16;
pub const EE_SCREENS_SEPL_OP_SET_SLIDER: u32 = 17;
/// The label p0's text set to the number i0 (`SetText(long)`; i1: `SetText(unsigned long)`).
pub const EE_SCREENS_SEPL_OP_LABEL_NUMBER: u32 = 18;
/// The colour button p0's colour -> / set to i0.
pub const EE_SCREENS_SEPL_OP_COLOR: u32 = 19;
pub const EE_SCREENS_SEPL_OP_SET_COLOR: u32 = 20;
/// The control group i0 of the form of the control p0 enabled (i1) or disabled.
pub const EE_SCREENS_SEPL_OP_GROUP: u32 = 21;
/// The control p0 moved to (f0, f1).
pub const EE_SCREENS_SEPL_OP_SET_LOCATION: u32 = 22;
/// The radio button p0 checked (`Check`: its own events run).
pub const EE_SCREENS_SEPL_OP_RADIO_CHECK: u32 = 23;
/// -> whether the unit type button p0 is checked.
pub const EE_SCREENS_SEPL_OP_RADIO_CHECKED: u32 = 24;
/// The trigger event i0 of the control p0 set: i1 the event (`EE_SCREENS_SEPL_EVENT_*`) over the control p1.
pub const EE_SCREENS_SEPL_OP_EVENT: u32 = 25;
/// The diplomacy buttons' bitmaps cut from the editor p0's texture / released.
pub const EE_SCREENS_SEPL_OP_LOAD_BITMAPS: u32 = 26;
pub const EE_SCREENS_SEPL_OP_RELEASE_BITMAPS: u32 = 27;
/// The diplomacy button p0's state -> / set to i0 with its bitmaps (`UpdateBitmaps`).
pub const EE_SCREENS_SEPL_OP_DIPLOMACY: u32 = 28;
pub const EE_SCREENS_SEPL_OP_SET_DIPLOMACY: u32 = 29;
/// The game options' rules: -> f-less i0 minimum, i1 maximum, i2 default of the unit limit.
pub const EE_SCREENS_SEPL_OP_UNIT_LIMIT_RULES: u32 = 30;
/// -> `gWorld.GetNumPlayers()` / whether player i0 exists / `gMaxPlayers`.
pub const EE_SCREENS_SEPL_OP_NUM_PLAYERS: u32 = 31;
pub const EE_SCREENS_SEPL_OP_PLAYER_EXISTS: u32 = 32;
pub const EE_SCREENS_SEPL_OP_MAX_PLAYERS: u32 = 33;
/// An `EESEAChangeWorldPlayerCount(i0)` posted.
pub const EE_SCREENS_SEPL_OP_POST_PLAYER_COUNT: u32 = 34;
/// -> whether player i0 has a computer player.
pub const EE_SCREENS_SEPL_OP_IS_COMPUTER: u32 = 35;
/// The tech type lists rebuilt from the tech tree (`BuildTechTypeLists`' sort into `EESEPSSetPlayers`' lists).
pub const EE_SCREENS_SEPL_OP_TECH_TYPES: u32 = 36;
/// The tech lists p0 (enabled) and p1 (disabled) refilled for the player combo p2's player from the list i0
/// (`kSETechList*`) / both rebuilt.
pub const EE_SCREENS_SEPL_OP_FILL_TECHS: u32 = 37;
pub const EE_SCREENS_SEPL_OP_REBUILD_TECHS: u32 = 38;
/// -> player i0's: resource cap i1 / tech tree's current epoch / end epoch / start epoch / shared line of sight /
/// the game options' colour / CPU-only flag; the computer player's flag i1 (`EE_SCREENS_SEPL_AI_*`); f0 its
/// citizen percentage.
pub const EE_SCREENS_SEPL_OP_PLAYER_GET: u32 = 39;
/// Player i0's value i1 (as above) set to i2 (the citizen percentage f0).
pub const EE_SCREENS_SEPL_OP_PLAYER_SET: u32 = 40;
/// The combo box p0 filled with the civilizations (none, the built-ins, the custom files), the first picked.
pub const EE_SCREENS_SEPL_OP_CIV_FILL: u32 = 41;
/// The combo box p0's civilization picked for player i0 (unknown: none, titled "unknown").
pub const EE_SCREENS_SEPL_OP_CIV_SELECT: u32 = 42;
/// Player i0's civilization from the combo box p0 -> whether it changed.
pub const EE_SCREENS_SEPL_OP_CIV_APPLY: u32 = 43;
/// The combo box p0 emptied and given the epochs' titles from i0 on.
pub const EE_SCREENS_SEPL_OP_FILL_EPOCHS: u32 = 44;
/// The edit box p0's text set to player i0's name / player i0's name set from it / the label p0's text set to it.
pub const EE_SCREENS_SEPL_OP_NAME_TO_EDIT: u32 = 45;
pub const EE_SCREENS_SEPL_OP_NAME_FROM_EDIT: u32 = 46;
pub const EE_SCREENS_SEPL_OP_NAME_TO_LABEL: u32 = 47;
/// Player i0's tech tree's epoch range reset to i1..i2 (`ResetEpochRange`).
pub const EE_SCREENS_SEPL_OP_EPOCH_RANGE: u32 = 48;
/// Player i0's colour set from the colour button p0 -> whether it differed.
pub const EE_SCREENS_SEPL_OP_APPLY_COLOR: u32 = 49;
/// -> player i0's `mEnemies[i1]`.
pub const EE_SCREENS_SEPL_OP_ENEMY: u32 = 50;
/// The stance of players i0 and i1 set to i2 (`SetDiplomaticStance`).
pub const EE_SCREENS_SEPL_OP_SET_STANCE: u32 = 51;
/// -> / set to i1 the world's value i0 (`EE_SCREENS_SEPL_WORLD_*`).
pub const EE_SCREENS_SEPL_OP_WORLD_GET: u32 = 52;
pub const EE_SCREENS_SEPL_OP_WORLD_SET: u32 = 53;
/// The load thread's message i0 (406, 403, 407) posted for player i1 with i2 (406, 407: the player's tech tree's
/// start epoch, read by the host).
pub const EE_SCREENS_SEPL_OP_LOAD_THREAD: u32 = 54;
/// The mini-map redrawn (if any) / recoloured.
pub const EE_SCREENS_SEPL_OP_MINIMAP_REDRAW: u32 = 55;
pub const EE_SCREENS_SEPL_OP_MINIMAP_RECOLOR: u32 = 56;
/// The tri-state check box p0's state -> / set to i0.
pub const EE_SCREENS_SEPL_OP_TRI_STATE: u32 = 57;
pub const EE_SCREENS_SEPL_OP_SET_TRI_STATE: u32 = 58;
/// -> whether the radio button p0 is checked.
pub const EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED: u32 = 59;
/// The list box p0 emptied.
pub const EE_SCREENS_SEPL_OP_LIST_CLEAR: u32 = 60;
/// -> the combo box p0's line count.
pub const EE_SCREENS_SEPL_OP_LINE_COUNT: u32 = 61;
/// -> the edit box p0's first line as `atoi` reads it.
pub const EE_SCREENS_SEPL_OP_EDIT_ATOI: u32 = 62;
/// The control p0 given one screen event (`EECreateScreenEvent(i0)`) on the triggers of the bits i1 (checked,
/// unchecked, selection changed).
pub const EE_SCREENS_SEPL_OP_SCREEN_EVENT: u32 = 63;
/// The label p0's text set to a text (i0 as `EE_SCREENS_SEPL_TEXT_*`, i1 the string, i2 the number).
pub const EE_SCREENS_SEPL_OP_LABEL_TEXT: u32 = 64;

/// The trigger events: the start epoch's (`EESEERust` start epoch over the end epoch combo box p1), a slider's
/// label (`UIESliderChange(p1, 1, 0)`), the unit limit's (`EESEENewUnitLimit(p1, 1, 0)`), the display epochs box's
/// screen event.
pub const EE_SCREENS_SEPL_EVENT_START_EPOCH: i32 = 1;
pub const EE_SCREENS_SEPL_EVENT_SLIDER: i32 = 2;
pub const EE_SCREENS_SEPL_EVENT_UNIT_LIMIT: i32 = 3;
pub const EE_SCREENS_SEPL_EVENT_DISPLAY_EPOCH: i32 = 4;

/// Player values (`EE_SCREENS_SEPL_OP_PLAYER_GET`/`SET`).
pub const EE_SCREENS_SEPL_PV_CAP: i32 = 1;
pub const EE_SCREENS_SEPL_PV_CURRENT_EPOCH: i32 = 2;
pub const EE_SCREENS_SEPL_PV_END_EPOCH: i32 = 3;
pub const EE_SCREENS_SEPL_PV_START_EPOCH: i32 = 4;
pub const EE_SCREENS_SEPL_PV_SHARED_LOS: i32 = 5;
pub const EE_SCREENS_SEPL_PV_OPTIONS_COLOR: i32 = 6;
pub const EE_SCREENS_SEPL_PV_OPTIONS_CPU: i32 = 7;
pub const EE_SCREENS_SEPL_PV_AI: i32 = 8;
pub const EE_SCREENS_SEPL_PV_CITIZENS: i32 = 9;
pub const EE_SCREENS_SEPL_PV_ID: i32 = 10;

/// The computer player's flags (`EE_SCREENS_SEPL_PV_AI`'s third integer): active, cheating, attack, defend,
/// build, explore, ignore others, gather.
pub const EE_SCREENS_SEPL_AI_COUNT: usize = 8;

/// The world's values: the population limit, the wonders to win, victory, teams locked, cheats, unit
/// improvements, carbon; the game options' unit limit, wonders, victory, lock teams, cheat codes.
pub const EE_SCREENS_SEPL_WORLD_POPULATION: i32 = 1;
pub const EE_SCREENS_SEPL_WORLD_WONDERS: i32 = 2;
pub const EE_SCREENS_SEPL_WORLD_VICTORY: i32 = 3;
pub const EE_SCREENS_SEPL_WORLD_TEAMS: i32 = 4;
pub const EE_SCREENS_SEPL_WORLD_CHEATS: i32 = 5;
pub const EE_SCREENS_SEPL_WORLD_IMPROVEMENTS: i32 = 6;
pub const EE_SCREENS_SEPL_WORLD_CARBON: i32 = 7;
pub const EE_SCREENS_SEPL_WORLD_OPT_UNIT_LIMIT: i32 = 8;
pub const EE_SCREENS_SEPL_WORLD_OPT_WONDERS: i32 = 9;
pub const EE_SCREENS_SEPL_WORLD_OPT_VICTORY: i32 = 10;
pub const EE_SCREENS_SEPL_WORLD_OPT_TEAMS: i32 = 11;
pub const EE_SCREENS_SEPL_WORLD_OPT_CHEATS: i32 = 12;

/// `kDisplayModeShown` / `kDisplayModeHidden`.
pub const EE_SCREENS_SEPL_SHOWN: i32 = 1;
pub const EE_SCREENS_SEPL_HIDDEN: i32 = 0;
/// `kTextAlignLeft` / `kTextAlignCenter`.
pub const EE_SCREENS_SEPL_ALIGN_LEFT: i32 = 0;
pub const EE_SCREENS_SEPL_ALIGN_CENTER: i32 = 1;

/// The pages (`kPlayerScreen*`).
pub const EE_SCREENS_SEPL_PAGE1: u32 = 0;
pub const EE_SCREENS_SEPL_PAGE2: u32 = 1;
pub const EE_SCREENS_SEPL_AI_SETTINGS: u32 = 2;
pub const EE_SCREENS_SEPL_TECH_TREE: u32 = 3;
pub const EE_SCREENS_SEPL_OPTIONS: u32 = 4;

/// The form, its controls and the world.
pub trait SeplHost {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32;
}

// ---- the language's strings (`ee/EEStringIDs.h`; the host `static_assert`s them)

pub const EE_SCREENS_SEPL_IDS_MAX_PLAYERS: u32 = 20512;
pub const EE_SCREENS_SEPL_IDS_START_EPOCH: u32 = 20515;
pub const EE_SCREENS_SEPL_IDS_ALL_PLAYERS: u32 = 20517;
pub const EE_SCREENS_SEPL_IDS_SE_PLAYERS: u32 = 20518;
pub const EE_SCREENS_SEPL_IDS_SE_PLAYER: u32 = 20523;
pub const EE_SCREENS_SEPL_IDS_SE_CIVILIZATION: u32 = 20524;
pub const EE_SCREENS_SEPL_IDS_ENABLE_TECHNOLOGIES: u32 = 20528;
pub const EE_SCREENS_SEPL_IDS_DISABLE_TECHNOLOGIES: u32 = 20529;
pub const EE_SCREENS_SEPL_IDS_UNIT_TYPES: u32 = 20530;
pub const EE_SCREENS_SEPL_IDS_SE_UNIT_GROUP_FIRST: u32 = 20531;
pub const EE_SCREENS_SEPL_IDS_DISPLAY_EPOCHS: u32 = 20547;
pub const EE_SCREENS_SEPL_IDS_END_EPOCH: u32 = 20548;
pub const EE_SCREENS_SEPL_IDS_SE_HUMAN: u32 = 20552;
/// Active, Cheat, Attack, Defend, Build/Repair, Explore, Ignore, Gather, Citizen Percentage: 20556..20564.
pub const EE_SCREENS_SEPL_IDS_SE_ACTIVE: u32 = 20556;
pub const EE_SCREENS_SEPL_IDS_RESOURCE_NAME_FIRST: u32 = 30116;
pub const EE_SCREENS_SEPL_IDS_GAME_OPTIONS: u32 = 30237;
pub const EE_SCREENS_SEPL_IDS_LOBBY_RESOURCES: u32 = 30252;
/// Game Unit Limit, Per Player, Wonders For Victory, Victory Allowed, Teams Locked, Cheat Codes, Unit Improvements,
/// CPU, Only, Shared, LOS: 30448..30458.
pub const EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT: u32 = 30448;
pub const EE_SCREENS_SEPL_IDS_MUTUAL_DIPLOMACY_SETTINGS: u32 = 30480;
pub const EE_SCREENS_SEPL_IDS_USE_CARBON_RESOURCE: u32 = 30493;

const IDS_PER_PLAYER: u32 = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 1;
const IDS_WONDERS_FOR_VICTORY: u32 = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 2;
const IDS_GAME_VICTORY_ALLOWED: u32 = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 3;
const IDS_TEAMS_LOCKED: u32 = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 4;
const IDS_GAME_CHEAT_CODES: u32 = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 5;
const IDS_UNIT_IMPROVEMENTS: u32 = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 6;
const IDS_CPU: u32 = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT + 7;

/// The yellow texts' background opacity.
pub(crate) const YELLOW_BG: f32 = 0.4;

/// `kControlTriggerRangeChanged`, `kControlTriggerChecked`, `kControlTriggerUnchecked`,
/// `kControlTriggerSelectionChanged`.
pub const EE_SCREENS_SEPL_TRIGGER_RANGE: i32 = 0x3e8;
pub const EE_SCREENS_SEPL_TRIGGER_CHECKED: i32 = 0x3e9;
pub const EE_SCREENS_SEPL_TRIGGER_UNCHECKED: i32 = 0x3ea;
pub const EE_SCREENS_SEPL_TRIGGER_SELECTION: i32 = 0x3eb;

// ---- the object

/// A player row (`EESEPlayerRow`): row 0 edits all players (no title, name, colour, CPU-only or shared LOS).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Row {
    pub title: usize,
    pub values: [usize; 5],
    pub start_epoch: usize,
    pub end_epoch: usize,
    pub color: usize,
    pub name: usize,
    pub control: usize,
    pub cpu_only: usize,
    pub shared_los: usize,
    /// active, cheat, attack, defend, build/repair, explore, ignore, gather (the computer player's flags' order)
    pub ai: [usize; EE_SCREENS_SEPL_AI_COUNT],
    pub slider: usize,
    pub slider_label: usize,
}

/// The game options page (`EESEGamePage`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct GamePage {
    pub title: usize,
    pub player_title: usize,
    pub player_names: [usize; 15],
    pub column_labels: [usize; 15],
    pub row_labels: [usize; 15],
    pub unit_limit_label: usize,
    pub unit_limit_count: usize,
    pub per_player_label: usize,
    pub per_player_count: usize,
    pub wonders_label: usize,
    pub wonders_count: usize,
    pub unit_limit: usize,
    pub wonders: usize,
    /// victory, teams locked, cheat codes, unit improvements, carbon
    pub checks: [usize; 5],
    pub diplomacy: Vec<usize>,
}

/// The player tab (`EESEPlayerScreen`).
#[derive(Debug, Default)]
pub struct Players {
    pub tabs: [usize; 5],
    pub title: usize,
    pub column_labels: [usize; 5],
    pub start_epoch_label: usize,
    pub end_epoch_label: usize,
    pub civilization_label: usize,
    pub player_label: usize,
    /// CPU, Only, Shared, LOS
    pub los_labels: [usize; 4],
    pub all_players_label: usize,
    pub max_players_label: usize,
    /// active .. gather, citizen percentage (the AI settings page's headers)
    pub ai_labels: [usize; 9],
    pub max_player: usize,
    pub rows: [Row; 17],
    pub set_players: usize,
    pub tech_player_label: usize,
    pub unit_types_label: usize,
    pub enable_label: usize,
    pub disable_label: usize,
    pub player_selection: usize,
    pub type_buttons: [usize; 4],
    pub tech_list: usize,
    pub other_tech_list: usize,
    /// add, remove, add all, remove all
    pub selects: [usize; 4],
    pub display_epoch: usize,
    pub game: GamePage,
    pub page: Cell<u32>,
    /// per player: changed, and a full tech tree reload
    pub changes: [Cell<(bool, bool)>; 17],
    pub dirty: Cell<bool>,
}

pub(crate) fn call<H: SeplHost + ?Sized>(h: &mut H, op: u32, p: [usize; 3], ints: [i32; 4]) -> i32 {
    let (mut i, mut f) = (ints, [0.0f32; 4]);
    h.op(op, p, &mut i, &mut f)
}

pub(crate) fn display<H: SeplHost + ?Sized>(h: &mut H, control: usize, shown: bool) {
    let mode = if shown { EE_SCREENS_SEPL_SHOWN } else { EE_SCREENS_SEPL_HIDDEN };
    call(h, EE_SCREENS_SEPL_OP_SET_DISPLAY, [control, 0, 0], [mode, 0, 0, 0]);
}

pub(crate) fn is_shown<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> bool {
    call(h, EE_SCREENS_SEPL_OP_DISPLAY, [control, 0, 0], [0; 4]) == EE_SCREENS_SEPL_SHOWN
}

pub(crate) fn checked<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> bool {
    call(h, EE_SCREENS_SEPL_OP_CHECKED, [control, 0, 0], [0; 4]) != 0
}

pub(crate) fn set_checked<H: SeplHost + ?Sized>(h: &mut H, control: usize, on: bool) {
    call(h, EE_SCREENS_SEPL_OP_SET_CHECKED, [control, 0, 0], [on as i32, 0, 0, 0]);
}

/// A combo box's selection (`GetCurrentSelection`), u32::MAX for none.
pub(crate) fn selection<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> u32 {
    call(h, EE_SCREENS_SEPL_OP_COMBO_SELECTION, [control, 0, 0], [0; 4]) as u32
}

/// Its list box's (`GetListBox()->GetCurrentSelection`).
pub(crate) fn list_selection<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> u32 {
    call(h, EE_SCREENS_SEPL_OP_LIST_SELECTION, [control, 0, 0], [0; 4]) as u32
}

pub(crate) fn set_selection<H: SeplHost + ?Sized>(h: &mut H, control: usize, line: u32) {
    call(h, EE_SCREENS_SEPL_OP_SET_SELECTION, [control, 0, 0], [line as i32, 0, 0, 0]);
}

pub(crate) fn slider<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> i32 {
    call(h, EE_SCREENS_SEPL_OP_SLIDER, [control, 0, 0], [0; 4])
}

pub(crate) fn set_slider<H: SeplHost + ?Sized>(h: &mut H, control: usize, value: i32) {
    call(h, EE_SCREENS_SEPL_OP_SET_SLIDER, [control, 0, 0], [value, 0, 0, 0]);
}

pub(crate) fn label_number<H: SeplHost + ?Sized>(h: &mut H, control: usize, value: i32, unsigned: bool) {
    call(h, EE_SCREENS_SEPL_OP_LABEL_NUMBER, [control, 0, 0], [value, unsigned as i32, 0, 0]);
}

pub(crate) fn group<H: SeplHost + ?Sized>(h: &mut H, control: usize, group: i32, enable: bool) {
    call(h, EE_SCREENS_SEPL_OP_GROUP, [control, 0, 0], [group, enable as i32, 0, 0]);
}

pub(crate) fn rect<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> [f32; 4] {
    let (mut i, mut f) = ([0i32; 4], [0.0f32; 4]);
    h.op(EE_SCREENS_SEPL_OP_RECT, [control, 0, 0], &mut i, &mut f);
    f
}

pub(crate) fn event<H: SeplHost + ?Sized>(h: &mut H, control: usize, trigger: i32, kind: i32, over: usize) {
    call(h, EE_SCREENS_SEPL_OP_EVENT, [control, over, 0], [trigger, kind, 0, 0]);
}

pub(crate) fn player_get<H: SeplHost + ?Sized>(h: &mut H, player: u32, value: i32, arg: i32) -> i32 {
    call(h, EE_SCREENS_SEPL_OP_PLAYER_GET, [0; 3], [player as i32, value, arg, 0])
}

pub(crate) fn player_set<H: SeplHost + ?Sized>(h: &mut H, player: u32, value: i32, arg: i32, to: i32) {
    call(h, EE_SCREENS_SEPL_OP_PLAYER_SET, [0; 3], [player as i32, value, arg, to]);
}

pub(crate) fn num_players<H: SeplHost + ?Sized>(h: &mut H) -> u32 {
    call(h, EE_SCREENS_SEPL_OP_NUM_PLAYERS, [0; 3], [0; 4]) as u32
}

pub(crate) fn player_exists<H: SeplHost + ?Sized>(h: &mut H, player: u32) -> bool {
    call(h, EE_SCREENS_SEPL_OP_PLAYER_EXISTS, [0; 3], [player as i32, 0, 0, 0]) != 0
}

pub(crate) fn delete<H: SeplHost + ?Sized>(h: &mut H, control: usize) {
    if control != 0 {
        call(h, EE_SCREENS_SEPL_OP_DELETE, [control, 0, 0], [0; 4]);
    }
}

/// The controls' maker: the host, the form, the next control index.
pub(crate) struct Maker<'a, H: SeplHost + ?Sized> {
    pub(crate) h: &'a mut H,
    pub(crate) form: usize,
    pub(crate) index: u32,
}

impl<H: SeplHost + ?Sized> Maker<'_, H> {
    /// One control made at the next index; the host answers its handle in p0.
    pub(crate) fn make(&mut self, mut c: EeScreensSeplCtl) -> usize {
        c.index = self.index;
        self.index += 1;
        let (mut i, mut f) = ([0i32; 4], [0.0f32; 4]);
        let mut out = 0usize;
        let p = [&mut out as *mut usize as usize, &c as *const EeScreensSeplCtl as usize, self.form];
        self.h.op(EE_SCREENS_SEPL_OP_MAKE, p, &mut i, &mut f);
        out
    }
}

/// A yellow label of the player pages: the screen's font at its height.
fn page_label(id: u32, string: u32, x: f32, y: f32, w: f32, g: u32, align: i32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 0.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
        .align(align)
        .bg(YELLOW_BG)
        .group(g)
}

/// The first diplomacy button of a player (the upper triangle packed row by row; `PairIndex`).
pub fn pair_index(player: u32) -> u32 {
    let remaining = 16u32.wrapping_sub(player);
    120u32.wrapping_sub((remaining.wrapping_add(1).wrapping_mul(remaining) as f32 * 0.5f32) as u32)
}

impl Players {
    /// `EESEPlayerScreen`'s constructor on the form: the player pages' controls, the tech tree page's, the game
    /// options page's (ids from 1200); `index` is the form's next control index (advanced).
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, form: usize, player_selection: usize, index: &mut u32) -> Players {
        let mut m = Maker { h, form, index: *index };
        let mut p = Players { player_selection, ..Default::default() };
        p.game.diplomacy = vec![0; 120];
        p.make_player_controls(&mut m);
        p.make_tech_controls(&mut m);
        p.make_game_page(&mut m, 1200);
        *index = m.index;
        p
    }

    fn make_player_controls<H: SeplHost + ?Sized>(&mut self, m: &mut Maker<'_, H>) {
        const C: i32 = EE_SCREENS_SEPL_ALIGN_CENTER;
        self.page.set(EE_SCREENS_SEPL_PAGE1);
        self.title = m.make(page_label(0x12e, EE_SCREENS_SEPL_IDS_LOBBY_RESOURCES, 145.0, 25.0, 290.0, 4, C));
        self.max_players_label = m.make(page_label(0x15b, EE_SCREENS_SEPL_IDS_MAX_PLAYERS, 5.0, 25.0, 125.0, 4, C));
        for i in 0..5u32 {
            let x = i as f32 * 55.0 + 145.0;
            let c = page_label(0x12f + i, EE_SCREENS_SEPL_IDS_RESOURCE_NAME_FIRST + i, x, 50.0, 55.0, 4, C);
            self.column_labels[i as usize] = m.make(c);
        }
        self.start_epoch_label = m.make(page_label(0x13b, EE_SCREENS_SEPL_IDS_START_EPOCH, 440.0, 50.0, 175.0, 4, C));
        self.end_epoch_label = m.make(page_label(0x16b, EE_SCREENS_SEPL_IDS_END_EPOCH, 620.0, 50.0, 175.0, 4, C));
        // CPU / Only, Shared / LOS
        for (k, (x, y)) in [(505.0, 25.0), (505.0, 50.0), (560.0, 25.0), (560.0, 50.0)].into_iter().enumerate() {
            self.los_labels[k] = m.make(page_label(0x16c + k as u32, IDS_CPU + k as u32, x, y, 48.0, 4, C));
        }
        let c = page_label(0x13e, EE_SCREENS_SEPL_IDS_SE_CIVILIZATION, 325.0, 50.0, 175.0, 4, C);
        self.civilization_label = m.make(c);
        self.player_label = m.make(page_label(0x13f, EE_SCREENS_SEPL_IDS_SE_PLAYER, 145.0, 50.0, 175.0, 4, C));
        // the AI settings' headers on alternating lines; Build/Repair in gU531930 at the screen font's height
        for k in 0..9u32 {
            let (x, y, w) = if k == 8 {
                (570.0, 50.0, 175.0)
            } else {
                (158.0 + 50.0 * k as f32, if k % 2 == 0 { 50.0 } else { 25.0 }, 75.0)
            };
            let mut c = page_label(0x171 + k, EE_SCREENS_SEPL_IDS_SE_ACTIVE + k, x, y, w, 4, C);
            if k == 4 {
                c = c.font(EE_SCREENS_SEPL_FONT_530);
            }
            self.ai_labels[k as usize] = m.make(c);
        }
        self.all_players_label = m.make(page_label(0x143, EE_SCREENS_SEPL_IDS_ALL_PLAYERS, 5.0, 74.0, 100.0, 4, C));
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_MAX_PLAYER, 0x15c, 5.0, 47.0, 125.0, 20.0)
            .text(EE_SCREENS_SEPL_TEXT_NUMBER_STRING, EE_SCREENS_SEPL_IDS_SE_PLAYERS, 0)
            .flags(EE_SCREENS_SEPL_FG)
            .bg2(YELLOW_BG)
            .group(4);
        self.max_player = m.make(c);
        let below = rect(m.h, self.all_players_label)[3] + 2.0;
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_SET_PLAYERS, 0, 5.0, below, 0.0, 0.0).bg(1.0).group(4);
        self.set_players = m.make(c);
        let mut id = 400u32;
        self.rows[0] = make_row(m, &mut id, 74.0, 0, true);
        for i in 1..=16u32 {
            self.rows[i as usize] = make_row(m, &mut id, (i - 1) as f32 * 26.0 + 130.0, i, false);
        }
        // the page tabs, each right of the one before
        let mut x = 0.0f32;
        for (k, tab) in [0x12c, 0x15d, 0x170, 0x12d, 0x15a].into_iter().enumerate() {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_TAB, tab, x, 0.0, 0.0, 0.0)
                .bg(1.0)
                .group(4)
                .group(5)
                .group(8)
                .extra([(k == 0) as i32, 0, 0, 0]);
            self.tabs[k] = m.make(c);
            x = rect(m.h, self.tabs[k])[2] + 1.0;
        }
    }

    fn make_tech_controls<H: SeplHost + ?Sized>(&mut self, m: &mut Maker<'_, H>) {
        const L: i32 = EE_SCREENS_SEPL_ALIGN_LEFT;
        const C: i32 = EE_SCREENS_SEPL_ALIGN_CENTER;
        self.tech_player_label = m.make(page_label(0x145, EE_SCREENS_SEPL_IDS_SE_PLAYER, 5.0, 50.0, 140.0, 5, L));
        self.unit_types_label = m.make(page_label(0x146, EE_SCREENS_SEPL_IDS_UNIT_TYPES, 5.0, 200.0, 140.0, 5, L));
        let c = page_label(0x147, EE_SCREENS_SEPL_IDS_ENABLE_TECHNOLOGIES, 245.0, 50.0, 250.0, 5, C);
        self.enable_label = m.make(c);
        let c = page_label(0x148, EE_SCREENS_SEPL_IDS_DISABLE_TECHNOLOGIES, 545.0, 50.0, 250.0, 5, C);
        self.disable_label = m.make(c);
        let mut y = 220.0f32;
        for i in 0..4u32 {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_UNIT_TYPE, 0x14a + i, 5.0, y, 140.0, 24.0)
                .string(EE_SCREENS_SEPL_IDS_SE_UNIT_GROUP_FIRST + i)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(5)
                .extra([(i == 0) as i32, 0, 0, 0]);
            self.type_buttons[i as usize] = m.make(c);
            y = rect(m.h, self.type_buttons[i as usize])[3] + 2.0;
        }
        let list = |id: u32, x: f32| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_TECH_LIST, id, x, 74.0, 250.0, 450.0)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .bg2(YELLOW_BG)
                .group(5)
        };
        self.tech_list = m.make(list(0x154, 245.0));
        for (k, y) in [240.0, 271.0, 302.0, 333.0].into_iter().enumerate() {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_SELECT, 0x156 + k as u32, 500.0, y, 0.0, 0.0);
            self.selects[k] = m.make(c.bg(1.0).group(5));
        }
        self.other_tech_list = m.make(list(0x155, 545.0));
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, 0x15e, 5.0, 370.0, 150.0, 1.0)
            .font(EE_SCREENS_SEPL_FONT_538)
            .string(EE_SCREENS_SEPL_IDS_DISPLAY_EPOCHS)
            .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
            .bg(YELLOW_BG)
            .group(5);
        self.display_epoch = m.make(c);
        let (t, e) = (EE_SCREENS_SEPL_TRIGGER_CHECKED, EE_SCREENS_SEPL_EVENT_DISPLAY_EPOCH);
        event(m.h, self.display_epoch, t, e, 0);
    }

    fn make_game_page<H: SeplHost + ?Sized>(&mut self, m: &mut Maker<'_, H>, mut id: u32) {
        const C: i32 = EE_SCREENS_SEPL_ALIGN_CENTER;
        const L: i32 = EE_SCREENS_SEPL_ALIGN_LEFT;
        const S: u32 = EE_SCREENS_SEPL_TEXT_STRING;
        const N: u32 = EE_SCREENS_SEPL_TEXT_NUMBER;
        let g = &mut self.game;
        // the grid's labels: columns 2..16, rows 1..15 each with its player's name
        let grid = |id: u32, text: u32, number: i32, r: [f32; 4], align: i32| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, r[0], r[1], r[2], r[3])
                .font(EE_SCREENS_SEPL_FONT_538)
                .text(text, 0, number)
                .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_LTRB)
                .align(align)
                .bg(YELLOW_BG)
                .group(8)
        };
        for c in 0..15u32 {
            let x = (c + 1) as f32 * 25.0 + 274.0;
            g.column_labels[c as usize] = m.make(grid(id, N, c as i32 + 2, [x, 100.0, x + 22.0, 122.0], C));
            id += 1;
        }
        for i in 0..15u32 {
            let y = i as f32 * 25.0 + 125.0;
            g.row_labels[i as usize] = m.make(grid(id, N, i as i32 + 1, [249.0, y, 271.0, y + 22.0], C));
            id += 1;
            let none = EE_SCREENS_SEPL_TEXT_NONE;
            g.player_names[i as usize] = m.make(grid(id, none, 0, [7.0, y, 131.0, y + 22.0], L));
            id += 1;
        }
        call(m.h, EE_SCREENS_SEPL_OP_LOAD_BITMAPS, [m.form, 0, 0], [0; 4]);
        for player in 1..=16u32 {
            let mut index = pair_index(player);
            let end = index.wrapping_sub(player).wrapping_add(16);
            let mut x = player as f32 * 25.0 + 274.0;
            let y = (player - 1) as f32 * 25.0 + 125.0;
            while index < end {
                let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_DIPLOMACY, id, x, y, 23.0, 23.0).group(8);
                g.diplomacy[index as usize] = m.make(c);
                x += 25.0;
                id += 1;
                index += 1;
            }
        }
        let title = |id: u32, string: u32, x: f32, w: f32| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, 50.0, w, 25.0)
                .font(EE_SCREENS_SEPL_FONT_ARIAL18)
                .string(string)
                .flags(EE_SCREENS_SEPL_FG)
                .align(C)
                .bg(YELLOW_BG)
                .group(8)
        };
        g.title = m.make(title(0x17a, EE_SCREENS_SEPL_IDS_MUTUAL_DIPLOMACY_SETTINGS, 7.0, 555.0));
        g.player_title = m.make(title(0x17b, EE_SCREENS_SEPL_IDS_GAME_OPTIONS, 600.0, 175.0));
        // the options column in gU531934: labels at its height, sliders 125 x 20 with their counts
        let label = |id: u32, text: u32, string: u32, x: f32, y: f32, w: f32| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 0.0)
                .font(EE_SCREENS_SEPL_FONT_534)
                .text(text, string, 0)
                .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
                .bg(YELLOW_BG)
                .group(8)
        };
        let slider = |id: u32, x: f32, y: f32, range: [i32; 3]| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_SLIDER, id, x, y, 125.0, 20.0)
                .bg(YELLOW_BG)
                .group(8)
                .extra([range[0], range[1], 1, range[2]])
        };
        let unit_limit = EE_SCREENS_SEPL_IDS_GAME_GAME_UNIT_LIMIT;
        g.unit_limit_label = m.make(label(0x15f, S, unit_limit, 600.0, 100.0, 175.0));
        let mut rules = [0i32; 4];
        m.h.op(EE_SCREENS_SEPL_OP_UNIT_LIMIT_RULES, [0; 3], &mut rules, &mut [0.0; 4]);
        let r = rect(m.h, g.unit_limit_label);
        g.unit_limit = m.make(slider(0x160, 600.0, r[3] + 1.0, [rules[0], rules[1], rules[2]]));
        let r = rect(m.h, g.unit_limit);
        g.unit_limit_count = m.make(label(0x16a, N, 0, r[2] + 1.0, r[1], 50.0));
        label_number(m.h, g.unit_limit_count, rules[2], false);
        let (t, e) = (EE_SCREENS_SEPL_TRIGGER_RANGE, EE_SCREENS_SEPL_EVENT_UNIT_LIMIT);
        event(m.h, g.unit_limit, t, e, g.unit_limit_count);
        let r = rect(m.h, g.unit_limit);
        g.per_player_label = m.make(label(0x161, S, IDS_PER_PLAYER, r[0], r[3] + 1.0, 125.0));
        let pr = rect(m.h, g.per_player_label);
        g.per_player_count = m.make(label(0x162, N, 0, pr[2] + 1.0, pr[1], 50.0));
        label_number(m.h, g.per_player_count, rules[2] / 2, false);
        g.wonders_label = m.make(label(0x163, S, IDS_WONDERS_FOR_VICTORY, pr[0], pr[3] + 5.0, 175.0));
        let r = rect(m.h, g.wonders_label);
        g.wonders = m.make(slider(0x164, pr[0], r[3] + 1.0, [0, 6, 3]));
        let r = rect(m.h, g.wonders);
        g.wonders_count = m.make(label(0x165, N, 0, r[2] + 1.0, r[1], 50.0));
        label_number(m.h, g.wonders_count, 3, false);
        event(m.h, g.wonders, EE_SCREENS_SEPL_TRIGGER_RANGE, EE_SCREENS_SEPL_EVENT_SLIDER, g.wonders_count);
        // the five option boxes, each 5 below the one before
        let boxes = [
            (0x166, IDS_GAME_VICTORY_ALLOWED),
            (0x167, IDS_TEAMS_LOCKED),
            (0x168, IDS_GAME_CHEAT_CODES),
            (0x169, IDS_UNIT_IMPROVEMENTS),
            (0x17c, EE_SCREENS_SEPL_IDS_USE_CARBON_RESOURCE),
        ];
        let mut r = rect(m.h, g.wonders);
        for (k, (id, string)) in boxes.into_iter().enumerate() {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, id, r[0], r[3] + 5.0, 175.0, 20.0)
                .font(EE_SCREENS_SEPL_FONT_534)
                .string(string)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(8);
            g.checks[k] = m.make(c);
            r = rect(m.h, g.checks[k]);
        }
    }
}

/// `EESEPlayerRow`'s constructor: row `player` at `y`, its controls' ids from `id` (advanced).
fn make_row<H: SeplHost + ?Sized>(m: &mut Maker<'_, H>, id: &mut u32, y: f32, player: u32, global: bool) -> Row {
    let mut row = Row::default();
    let next = |m: &mut Maker<'_, H>, id: &mut u32, mut c: EeScreensSeplCtl| {
        c.id = *id;
        *id += 1;
        m.make(c.group(4))
    };
    if !global {
        // the title keeps the id 0x13d; the row's ids still advance
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, 0x13d, 5.0, y, 100.0, 24.0);
        let c = if player != 1 {
            c.text(EE_SCREENS_SEPL_TEXT_STRING_NUMBER, EE_SCREENS_SEPL_IDS_SE_PLAYER, player as i32)
        } else {
            c.string(EE_SCREENS_SEPL_IDS_SE_HUMAN)
        };
        row.title = m.make(c.flags(EE_SCREENS_SEPL_FG).align(EE_SCREENS_SEPL_ALIGN_LEFT).bg(YELLOW_BG).group(4));
        *id += 1;
    }
    for i in 0..5usize {
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, 0, i as f32 * 55.0 + 145.0, y, 55.0, 24.0)
            .text(EE_SCREENS_SEPL_TEXT_NUMBER, 0, 1000)
            .flags(EE_SCREENS_SEPL_FG)
            .bg(YELLOW_BG)
            .extra([6, 1, 0, 0]);
        row.values[i] = next(m, id, c);
    }
    let combo = |x: f32, lines: i32, first: i32| {
        EeScreensSeplCtl::new(EE_SCREENS_SEPL_COMBO, 0, x, y, 175.0, 20.0)
            .flags(EE_SCREENS_SEPL_FG)
            .bg(YELLOW_BG)
            .bg2(YELLOW_BG)
            .extra([lines, first, 0, 0])
    };
    row.end_epoch = next(m, id, combo(620.0, EE_SCREENS_SEPL_LINES_EPOCHS, if global { 14 } else { 0 }));
    row.start_epoch = next(m, id, combo(440.0, EE_SCREENS_SEPL_LINES_EPOCHS, 0));
    let (t, e) = (EE_SCREENS_SEPL_TRIGGER_SELECTION, EE_SCREENS_SEPL_EVENT_START_EPOCH);
    event(m.h, row.start_epoch, t, e, row.end_epoch);
    row.control = next(m, id, combo(325.0, EE_SCREENS_SEPL_LINES_NONE, -1));
    let check = |x: f32| EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, 0, x, y, 20.0, 20.0);
    if !global {
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, 0, 145.0, y, 175.0, 20.0)
            .flags(EE_SCREENS_SEPL_FG)
            .bg(0.5)
            .extra([50, 0, 0, 0]);
        row.name = next(m, id, c);
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_COLOR, 0, 110.0, y, 0.0, 0.0).extra([player as i32, 0, 0, 0]);
        row.color = next(m, id, c);
        row.cpu_only = next(m, id, check(519.0));
        row.shared_los = next(m, id, check(574.0));
    }
    for k in 0..EE_SCREENS_SEPL_AI_COUNT {
        row.ai[k] = next(m, id, check(182.0 + 50.0 * k as f32));
    }
    let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_SLIDER, 0, 570.0, y, 116.0, 20.0)
        .flags(EE_SCREENS_SEPL_RANGE_FIRST)
        .extra([0, 100, 1, i32::MIN]);
    row.slider = next(m, id, c);
    let x = rect(m.h, row.slider)[2] + 5.0;
    let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, 0, x, y, 50.0, 20.0).flags(EE_SCREENS_SEPL_FG).bg(0.5);
    row.slider_label = next(m, id, c);
    event(m.h, row.slider, EE_SCREENS_SEPL_TRIGGER_RANGE, EE_SCREENS_SEPL_EVENT_SLIDER, row.slider_label);
    label_number(m.h, row.slider_label, 0, false);
    row
}

/// A combo box line or a label text: `EE_SCREENS_SEPL_TEXT_*`, the resource string, the number.
fn append<H: SeplHost + ?Sized>(h: &mut H, combo: usize, text: u32, string: u32, number: i32) {
    call(h, EE_SCREENS_SEPL_OP_COMBO_APPEND, [combo, 0, 0], [text as i32, string as i32, number, 0]);
}

fn edit_copy<H: SeplHost + ?Sized>(h: &mut H, to: usize, from: usize) {
    call(h, EE_SCREENS_SEPL_OP_EDIT_COPY, [to, from, 0], [0; 4]);
}

fn color<H: SeplHost + ?Sized>(h: &mut H, button: usize) -> i32 {
    call(h, EE_SCREENS_SEPL_OP_COLOR, [button, 0, 0], [0; 4])
}

fn is_computer<H: SeplHost + ?Sized>(h: &mut H, player: u32) -> bool {
    call(h, EE_SCREENS_SEPL_OP_IS_COMPUTER, [0; 3], [player as i32, 0, 0, 0]) != 0
}

fn world_get<H: SeplHost + ?Sized>(h: &mut H, what: i32) -> i32 {
    call(h, EE_SCREENS_SEPL_OP_WORLD_GET, [0; 3], [what, 0, 0, 0])
}

fn world_set<H: SeplHost + ?Sized>(h: &mut H, what: i32, to: i32) {
    call(h, EE_SCREENS_SEPL_OP_WORLD_SET, [0; 3], [what, to, 0, 0]);
}

/// u32::MAX: a combo box with no selection (`UIsNone32`).
pub(crate) const NONE: u32 = u32::MAX;

impl Row {
    /// `Hide`: every control hidden.
    pub fn hide<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let optional = [self.title, self.start_epoch, self.end_epoch, self.color, self.control, self.name];
        for c in optional.into_iter().chain([self.cpu_only, self.shared_los]).chain(self.values) {
            if c != 0 {
                display(h, c, false);
            }
        }
        for c in self.ai.into_iter().chain([self.slider, self.slider_label]) {
            display(h, c, false);
        }
    }

    /// `Show`: the controls of a page (epochs, colour, resource caps; civilization, name, shared LOS and computer
    /// players' CPU-only; computer players' AI settings).
    pub fn show<H: SeplHost + ?Sized>(&self, h: &mut H, page: u32, player: u32) {
        let exists = player > 0 && player_exists(h, player);
        self.hide(h);
        let maybe = |h: &mut H, c: usize| {
            if c != 0 {
                display(h, c, true);
            }
        };
        match page {
            EE_SCREENS_SEPL_PAGE1 => {
                maybe(h, self.title);
                display(h, self.start_epoch, true);
                display(h, self.end_epoch, true);
                maybe(h, self.color);
                for c in self.values {
                    display(h, c, true);
                }
            }
            EE_SCREENS_SEPL_PAGE2 => {
                maybe(h, self.title);
                maybe(h, self.color);
                display(h, self.control, true);
                maybe(h, self.name);
                if !exists {
                    return;
                }
                display(h, self.shared_los, true);
                if !is_computer(h, player) {
                    return;
                }
                display(h, self.cpu_only, true);
            }
            EE_SCREENS_SEPL_AI_SETTINGS => {
                maybe(h, self.title);
                maybe(h, self.color);
                if exists && !is_computer(h, player) {
                    return;
                }
                for c in self.ai.into_iter().chain([self.slider, self.slider_label]) {
                    display(h, c, true);
                }
            }
            _ => {}
        }
    }

    /// `CopyFrom`: another row's settings into this row's shown controls.
    pub fn copy_from<H: SeplHost + ?Sized>(&self, h: &mut H, from: &Row) {
        if from.color != 0 && self.color != 0 && is_shown(h, self.color) {
            let c = color(h, from.color);
            call(h, EE_SCREENS_SEPL_OP_SET_COLOR, [self.color, 0, 0], [c, 0, 0, 0]);
        }
        let to_epoch = selection(h, self.start_epoch);
        let from_epoch = selection(h, from.start_epoch);
        if from_epoch != NONE && is_shown(h, self.start_epoch) {
            set_selection(h, self.start_epoch, from_epoch);
        }
        let from_end = selection(h, from.end_epoch);
        if from_end != NONE && is_shown(h, self.end_epoch) {
            set_selection(h, self.end_epoch, from_end.wrapping_sub(to_epoch).wrapping_add(from_epoch));
        }
        let from_control = selection(h, from.control);
        if from_control != NONE && is_shown(h, self.control) {
            set_selection(h, self.control, from_control);
        }
        if from.name != 0 && self.name != 0 {
            edit_copy(h, self.name, from.name);
        }
        for (to, src) in [(self.cpu_only, from.cpu_only), (self.shared_los, from.shared_los)] {
            if src != 0 && to != 0 && is_shown(h, to) {
                let on = checked(h, src);
                set_checked(h, to, on);
            }
        }
        for i in 0..5 {
            if is_shown(h, self.values[i]) {
                edit_copy(h, self.values[i], from.values[i]);
            }
        }
        for k in 0..EE_SCREENS_SEPL_AI_COUNT {
            if is_shown(h, self.ai[k]) {
                let on = checked(h, from.ai[k]);
                set_checked(h, self.ai[k], on);
            }
        }
        if is_shown(h, self.slider) {
            let v = slider(h, from.slider);
            set_slider(h, self.slider, v);
        }
    }

    /// `Load`: the civilizations; for a player its caps, colour, flags, epochs, name, civilization and AI.
    pub fn load<H: SeplHost + ?Sized>(&self, h: &mut H, player: u32) {
        call(h, EE_SCREENS_SEPL_OP_CIV_FILL, [self.control, 0, 0], [0; 4]);
        if player == 0 {
            return;
        }
        for i in 0..5 {
            let cap = player_get(h, player, EE_SCREENS_SEPL_PV_CAP, i as i32 + 2);
            call(h, EE_SCREENS_SEPL_OP_EDIT_NUMBER, [self.values[i], 0, 0], [cap, 0, 0, 0]);
        }
        let c = player_get(h, player, EE_SCREENS_SEPL_PV_OPTIONS_COLOR, 0);
        call(h, EE_SCREENS_SEPL_OP_SET_COLOR, [self.color, 0, 0], [c, 0, 0, 0]);
        let cpu = player_get(h, player, EE_SCREENS_SEPL_PV_OPTIONS_CPU, 0) != 0;
        set_checked(h, self.cpu_only, cpu);
        let los = player_get(h, player, EE_SCREENS_SEPL_PV_SHARED_LOS, 0) != 0;
        set_checked(h, self.shared_los, los);
        let start = player_get(h, player, EE_SCREENS_SEPL_PV_CURRENT_EPOCH, 0);
        set_selection(h, self.start_epoch, start as u32);
        call(h, EE_SCREENS_SEPL_OP_FILL_EPOCHS, [self.end_epoch, 0, 0], [start, 0, 0, 0]);
        let end = player_get(h, player, EE_SCREENS_SEPL_PV_END_EPOCH, 0);
        set_selection(h, self.end_epoch, if start > end { 0 } else { end.wrapping_sub(start) as u32 });
        call(h, EE_SCREENS_SEPL_OP_NAME_TO_EDIT, [self.name, 0, 0], [player as i32, 0, 0, 0]);
        call(h, EE_SCREENS_SEPL_OP_CIV_SELECT, [self.control, 0, 0], [player as i32, 0, 0, 0]);
        if !is_computer(h, player) {
            return;
        }
        for k in 0..EE_SCREENS_SEPL_AI_COUNT {
            let on = player_get(h, player, EE_SCREENS_SEPL_PV_AI, k as i32) != 0;
            set_checked(h, self.ai[k], on);
        }
        let (mut i, mut f) = ([player as i32, EE_SCREENS_SEPL_PV_CITIZENS, 0, 0], [0.0f32; 4]);
        h.op(EE_SCREENS_SEPL_OP_PLAYER_GET, [0; 3], &mut i, &mut f);
        // (long)(percentage * 100 + 0.0001): two f32 operations, truncated
        set_slider(h, self.slider, (f[0] * 100.0f32 + 0.0001f32) as i32);
        let v = slider(h, self.slider);
        label_number(h, self.slider_label, v, false);
    }
}

impl GamePage {
    /// `SetPlayerCount`: the grid's labels and buttons for `count` players, the per-player unit limit.
    pub fn set_player_count<H: SeplHost + ?Sized>(&self, h: &mut H, count: u32) {
        for i in 0..15u32 {
            display(h, self.column_labels[i as usize], i + 1 < count);
            let row = i < count.wrapping_sub(1);
            display(h, self.row_labels[i as usize], row);
            display(h, self.player_names[i as usize], row);
        }
        for j in 2..=16u32 {
            let i = pair_index(j - 1);
            for k in j..=16u32 {
                let index = i.wrapping_sub(j).wrapping_add(k) as usize;
                display(h, self.diplomacy[index], k <= count);
            }
        }
        // (unsigned long)GetCurrentRangeValue() / count (the original divided by zero for none)
        let per = (slider(h, self.unit_limit) as u32).checked_div(count).unwrap_or(0);
        label_number(h, self.per_player_count, per as i32, true);
    }

    /// `Load`: the players' names and pairs' stances (enemies if either side is), the options.
    pub fn load<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let n = num_players(h);
        for i in 1..n {
            let start = pair_index(i);
            if i < n - 1 && i <= 15 {
                call(
                    h,
                    EE_SCREENS_SEPL_OP_NAME_TO_LABEL,
                    [self.player_names[i as usize - 1], 0, 0],
                    [i as i32, 0, 0, 0],
                );
            }
            for j in i + 1..n {
                let index = start.wrapping_add(j).wrapping_sub(i).wrapping_sub(1) as usize;
                let a = call(h, EE_SCREENS_SEPL_OP_ENEMY, [0; 3], [i as i32, j as i32, 0, 0]);
                let b = call(h, EE_SCREENS_SEPL_OP_ENEMY, [0; 3], [j as i32, i as i32, 0, 0]);
                if let Some(&button) = self.diplomacy.get(index) {
                    call(h, EE_SCREENS_SEPL_OP_SET_DIPLOMACY, [button, 0, 0], [a.max(b), 0, 0, 0]);
                }
            }
        }
        let v = world_get(h, EE_SCREENS_SEPL_WORLD_POPULATION);
        set_slider(h, self.unit_limit, v);
        let v = world_get(h, EE_SCREENS_SEPL_WORLD_WONDERS);
        set_slider(h, self.wonders, v);
        for (k, what) in [
            EE_SCREENS_SEPL_WORLD_VICTORY,
            EE_SCREENS_SEPL_WORLD_TEAMS,
            EE_SCREENS_SEPL_WORLD_CHEATS,
            EE_SCREENS_SEPL_WORLD_IMPROVEMENTS,
            EE_SCREENS_SEPL_WORLD_CARBON,
        ]
        .into_iter()
        .enumerate()
        {
            let on = world_get(h, what) != 0;
            set_checked(h, self.checks[k], on);
        }
    }

    /// `Save`: each existing pair's stance both ways; with a player 0, the options to the world (most also to its
    /// game options).
    pub fn save<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for i in 1..=16u32 {
            if !player_exists(h, i) {
                continue;
            }
            let start = pair_index(i);
            for j in i + 1..=16u32 {
                if !player_exists(h, j) {
                    continue;
                }
                let state =
                    call(h, EE_SCREENS_SEPL_OP_DIPLOMACY, [self.diplomacy[(start + j - i - 1) as usize], 0, 0], [0; 4]);
                call(h, EE_SCREENS_SEPL_OP_SET_STANCE, [0; 3], [i as i32, j as i32, state, 0]);
                call(h, EE_SCREENS_SEPL_OP_SET_STANCE, [0; 3], [j as i32, i as i32, state, 0]);
            }
        }
        if !player_exists(h, 0) {
            return;
        }
        let v = slider(h, self.unit_limit);
        world_set(h, EE_SCREENS_SEPL_WORLD_POPULATION, v);
        world_set(h, EE_SCREENS_SEPL_WORLD_OPT_UNIT_LIMIT, v);
        let v = slider(h, self.wonders);
        world_set(h, EE_SCREENS_SEPL_WORLD_WONDERS, v);
        world_set(h, EE_SCREENS_SEPL_WORLD_OPT_WONDERS, v);
        let pairs = [
            (EE_SCREENS_SEPL_WORLD_VICTORY, EE_SCREENS_SEPL_WORLD_OPT_VICTORY),
            (EE_SCREENS_SEPL_WORLD_TEAMS, EE_SCREENS_SEPL_WORLD_OPT_TEAMS),
            (EE_SCREENS_SEPL_WORLD_CHEATS, EE_SCREENS_SEPL_WORLD_OPT_CHEATS),
            (EE_SCREENS_SEPL_WORLD_IMPROVEMENTS, 0),
            (EE_SCREENS_SEPL_WORLD_CARBON, 0),
        ];
        for (k, (world, option)) in pairs.into_iter().enumerate() {
            let on = checked(h, self.checks[k]) as i32;
            world_set(h, world, on);
            if option != 0 {
                world_set(h, option, on);
            }
        }
    }

    /// The destructor's deletes, in its order.
    fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for i in 0..15 {
            delete(h, self.column_labels[i]);
            delete(h, self.row_labels[i]);
            delete(h, self.player_names[i]);
        }
        for &b in &self.diplomacy {
            delete(h, b);
        }
        call(h, EE_SCREENS_SEPL_OP_RELEASE_BITMAPS, [0; 3], [0; 4]);
        let rest = [
            self.unit_limit_label,
            self.unit_limit_count,
            self.per_player_label,
            self.per_player_count,
            self.wonders_label,
            self.wonders_count,
            self.unit_limit,
            self.wonders,
            self.checks[0],
            self.checks[1],
            self.checks[2],
            self.checks[3],
            self.player_title,
            self.title,
            self.checks[4],
        ];
        for c in rest {
            delete(h, c);
        }
    }
}

impl Players {
    /// `SetPlayerCount`: rows 1..count shown on the page and the rest hidden, the player combo refilled (Human,
    /// Player 2, ...), the diplomacy grid, and the world's player count posted as count + 1.
    pub fn set_player_count<H: SeplHost + ?Sized>(&self, h: &mut H, count: u32) {
        call(h, EE_SCREENS_SEPL_OP_COMBO_CLEAR, [self.player_selection, 0, 0], [0; 4]);
        for i in 1..=count.min(16) {
            self.rows[i as usize].show(h, self.page.get(), i);
            if i > 1 {
                append(
                    h,
                    self.player_selection,
                    EE_SCREENS_SEPL_TEXT_STRING_NUMBER,
                    EE_SCREENS_SEPL_IDS_SE_PLAYER,
                    i as i32,
                );
            } else {
                append(h, self.player_selection, EE_SCREENS_SEPL_TEXT_STRING, EE_SCREENS_SEPL_IDS_SE_HUMAN, 0);
            }
        }
        set_selection(h, self.player_selection, 0);
        for i in count.wrapping_add(1)..=16 {
            self.rows[i as usize].hide(h);
        }
        self.game.set_player_count(h, count);
        call(h, EE_SCREENS_SEPL_OP_POST_PLAYER_COUNT, [0; 3], [count.wrapping_add(1) as i32, 0, 0, 0]);
    }

    /// The rows 0..=count shown on a page, the rest hidden.
    fn show_rows<H: SeplHost + ?Sized>(&self, h: &mut H, page: u32, count: u32) {
        let mut i = 0u32;
        loop {
            self.rows[i as usize].show(h, page, i);
            i += 1;
            if i > count || i > 16 {
                break;
            }
        }
        for i in count.wrapping_add(1)..=16 {
            self.rows[i as usize].hide(h);
        }
    }

    /// The headers of page 1, 2 or the AI settings shown, the others hidden.
    fn show_headers<H: SeplHost + ?Sized>(&self, h: &mut H, page: u32) {
        let first = page == EE_SCREENS_SEPL_PAGE1;
        for c in [self.title, self.start_epoch_label, self.end_epoch_label].into_iter().chain(self.column_labels) {
            display(h, c, first);
        }
        for c in [self.civilization_label, self.player_label].into_iter().chain(self.los_labels) {
            display(h, c, page == EE_SCREENS_SEPL_PAGE2);
        }
        for c in self.ai_labels {
            display(h, c, page == EE_SCREENS_SEPL_AI_SETTINGS);
        }
    }

    /// `Refresh`: page 1's headers, the player counts (2 up to the world's players - 1), `gMaxPlayers - 1`
    /// players, the tech type lists.
    pub fn refresh<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.show_headers(h, EE_SCREENS_SEPL_PAGE1);
        call(h, EE_SCREENS_SEPL_OP_COMBO_CLEAR, [self.max_player, 0, 0], [0; 4]);
        let n = num_players(h);
        for count in 2..n {
            append(
                h,
                self.max_player,
                EE_SCREENS_SEPL_TEXT_NUMBER_STRING,
                EE_SCREENS_SEPL_IDS_SE_PLAYERS,
                count as i32,
            );
        }
        let count = list_selection(h, self.max_player).wrapping_add(2);
        self.show_rows(h, EE_SCREENS_SEPL_PAGE1, count);
        let max = call(h, EE_SCREENS_SEPL_OP_MAX_PLAYERS, [0; 3], [0; 4]) as u32;
        set_selection(h, self.max_player, max.wrapping_sub(3));
        self.set_player_count(h, max.wrapping_sub(1));
        self.build_tech_type_lists(h);
    }

    /// `Load`: refreshed, every player's row and the game options page loaded from the world.
    pub fn load<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.refresh(h);
        let n = num_players(h);
        for i in 0..n.min(17) {
            self.rows[i as usize].load(h, i);
        }
        self.game.load(h);
    }

    /// `Enter`: page 1 checked (its tab's own event switches the page), the player pages' controls enabled, the
    /// player combo moved to (5, 74).
    pub fn enter<H: SeplHost + ?Sized>(&self, h: &mut H) {
        self.page.set(EE_SCREENS_SEPL_PAGE1);
        call(h, EE_SCREENS_SEPL_OP_RADIO_CHECK, [self.tabs[0], 0, 0], [0; 4]);
        group(h, self.tabs[0], 5, false);
        group(h, self.tabs[0], 4, true);
        group(h, self.tabs[0], 8, false);
        let (mut i, mut f) = ([0i32; 4], [5.0f32, 74.0, 0.0, 0.0]);
        h.op(EE_SCREENS_SEPL_OP_SET_LOCATION, [self.player_selection, 0, 0], &mut i, &mut f);
    }

    /// `Leave`: the tech tree and options pages' controls disabled, the player combo hidden, the edits stored.
    pub fn leave<H: SeplHost + ?Sized>(&self, h: &mut H) {
        group(h, self.selects[3], 5, false);
        group(h, self.selects[3], 8, false);
        display(h, self.player_selection, false);
        self.store(h);
    }

    /// `GetPlayerCount`: the max-players combo's selection + 2.
    pub fn player_count<H: SeplHost + ?Sized>(&self, h: &mut H) -> u32 {
        list_selection(h, self.max_player).wrapping_add(2)
    }

    /// `MarkPlayerChanged`: changed, and needing a full tech tree reload when `refresh`.
    pub fn mark_changed(&self, player: u32, refresh: bool) {
        if let Some(c) = self.changes.get(player as usize) {
            let (_, r) = c.get();
            c.set((true, r || refresh));
        }
    }

    /// `Store`: the rows' epochs saved, all settings applied, each changed player's epoch media and tech tree
    /// reloaded by the load thread, the mini-map redrawn after a colour change.
    pub fn store<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let count = list_selection(h, self.max_player).wrapping_add(3);
        for i in 1..count.min(17) {
            self.save_row(h, i);
        }
        self.apply_player_settings(h);
        let post = |h: &mut H, message: i32, player: usize, value: i32| {
            call(h, EE_SCREENS_SEPL_OP_LOAD_THREAD, [0; 3], [message, player as i32, value, 0]);
        };
        for i in 0..17 {
            if self.changes[i].get().0 {
                post(h, 406, i, 0);
            }
        }
        for i in 0..17 {
            let (changed, refresh) = self.changes[i].get();
            if changed {
                post(h, 403, i, refresh as i32);
            }
        }
        for i in 0..17 {
            if self.changes[i].get().0 {
                post(h, 407, i, 0);
                self.changes[i].set((false, false));
            }
        }
        if self.dirty.get() {
            call(h, EE_SCREENS_SEPL_OP_MINIMAP_REDRAW, [0; 3], [0; 4]);
            self.dirty.set(false);
        }
    }

    /// `EESEPlayerRow::Save`: row `player`'s epochs to its tech tree, the player marked changed if the start moved.
    fn save_row<H: SeplHost + ?Sized>(&self, h: &mut H, player: u32) {
        let row = &self.rows[player as usize];
        let start = selection(h, row.start_epoch);
        let end = selection(h, row.end_epoch).wrapping_add(start);
        if start != player_get(h, player, EE_SCREENS_SEPL_PV_CURRENT_EPOCH, 0) as u32 {
            self.mark_changed(player, true);
        }
        call(h, EE_SCREENS_SEPL_OP_EPOCH_RANGE, [0; 3], [player as i32, start as i32, end as i32, 0]);
    }

    /// `EESEPlayerRow::ApplySettings`: row `player` to its player.
    fn apply_row<H: SeplHost + ?Sized>(&self, h: &mut H, player: u32) {
        let row = &self.rows[player as usize];
        for i in 0..5 {
            let v = call(h, EE_SCREENS_SEPL_OP_EDIT_INT, [row.values[i], 0, 0], [0; 4]);
            player_set(h, player, EE_SCREENS_SEPL_PV_CAP, i as i32 + 2, v);
        }
        if call(h, EE_SCREENS_SEPL_OP_APPLY_COLOR, [row.color, 0, 0], [player as i32, 0, 0, 0]) != 0 {
            self.dirty.set(true);
        }
        let c = color(h, row.color);
        player_set(h, player, EE_SCREENS_SEPL_PV_OPTIONS_COLOR, 0, c);
        let cpu = checked(h, row.cpu_only) as i32;
        player_set(h, player, EE_SCREENS_SEPL_PV_OPTIONS_CPU, 0, cpu);
        call(h, EE_SCREENS_SEPL_OP_NAME_FROM_EDIT, [row.name, 0, 0], [player as i32, 0, 0, 0]);
        let los = checked(h, row.shared_los) as i32;
        player_set(h, player, EE_SCREENS_SEPL_PV_SHARED_LOS, 0, los);
        let changed = call(h, EE_SCREENS_SEPL_OP_CIV_APPLY, [row.control, 0, 0], [player as i32, 0, 0, 0]) != 0;
        if is_computer(h, player) {
            for k in 0..EE_SCREENS_SEPL_AI_COUNT {
                let on = checked(h, row.ai[k]) as i32;
                player_set(h, player, EE_SCREENS_SEPL_PV_AI, k as i32, on);
            }
            let v = slider(h, row.slider);
            player_set(h, player, EE_SCREENS_SEPL_PV_CITIZENS, 0, v);
        }
        if changed {
            let id = player_get(h, player, EE_SCREENS_SEPL_PV_ID, 0);
            self.mark_changed(id as u32, false);
        }
    }

    /// `CopyFirstPlayerToAll`: the all-players row into every player's row.
    pub fn copy_first_to_all<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let n = num_players(h);
        for i in 1..n.min(17) {
            self.rows[i as usize].copy_from(h, &self.rows[0]);
        }
    }

    /// `ApplyPlayerSettings`: every existing player's row and the game options page to the world, the mini-map
    /// recoloured.
    pub fn apply_player_settings<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let n = num_players(h);
        for i in 1..n.min(17) {
            if player_exists(h, i) {
                self.apply_row(h, i);
            }
        }
        self.game.save(h);
        call(h, EE_SCREENS_SEPL_OP_MINIMAP_RECOLOR, [0; 3], [0; 4]);
    }

    /// `BuildTechTypeLists`: the tech tree sorted into the four lists, the tech lists refilled.
    pub fn build_tech_type_lists<H: SeplHost + ?Sized>(&self, h: &mut H) {
        call(h, EE_SCREENS_SEPL_OP_TECH_TYPES, [0; 3], [0; 4]);
        self.fill_tech_lists(h);
    }

    /// `FillTechLists`: both tech lists for the chosen player and the checked unit type (units, buildings, heroes,
    /// else technologies).
    pub fn fill_tech_lists<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let mut which = 3;
        for k in 0..3 {
            if call(h, EE_SCREENS_SEPL_OP_RADIO_CHECKED, [self.type_buttons[k], 0, 0], [0; 4]) != 0 {
                which = k as i32;
                break;
            }
        }
        let p = [self.tech_list, self.other_tech_list, self.player_selection];
        call(h, EE_SCREENS_SEPL_OP_FILL_TECHS, p, [which, 0, 0, 0]);
    }

    /// `RebuildTechLists`.
    pub fn rebuild_tech_lists<H: SeplHost + ?Sized>(&self, h: &mut H) {
        call(h, EE_SCREENS_SEPL_OP_REBUILD_TECHS, [self.tech_list, self.other_tech_list, 0], [0; 4]);
    }

    /// `SelectPage`: the page's control groups enabled, its headers and rows shown; nothing if already there.
    pub fn select_page<H: SeplHost + ?Sized>(&self, h: &mut H, page: u32) {
        if self.page.get() == page {
            return;
        }
        self.page.set(page);
        let form = self.tabs[0];
        match page {
            EE_SCREENS_SEPL_PAGE1 | EE_SCREENS_SEPL_PAGE2 | EE_SCREENS_SEPL_AI_SETTINGS => {
                group(h, form, 4, true);
                group(h, form, 5, false);
                group(h, form, 8, false);
                display(h, self.player_selection, false);
                self.show_headers(h, page);
                let count = list_selection(h, self.max_player).wrapping_add(2);
                self.show_rows(h, page, count);
            }
            EE_SCREENS_SEPL_TECH_TREE => {
                group(h, form, 4, false);
                group(h, form, 5, true);
                group(h, form, 8, false);
                display(h, self.player_selection, true);
            }
            EE_SCREENS_SEPL_OPTIONS => {
                group(h, form, 4, false);
                group(h, form, 5, false);
                group(h, form, 8, true);
                display(h, self.player_selection, false);
            }
            _ => {}
        }
    }

    /// The destructor: every control deleted in the original's order, the game page's among them.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        delete(h, self.title);
        delete(h, self.max_players_label);
        self.game.destroy(h);
        let mut order: Vec<usize> = self.column_labels.to_vec();
        order.extend(self.los_labels);
        order.extend([self.start_epoch_label, self.end_epoch_label, self.player_label, self.max_player]);
        order.push(self.civilization_label);
        order.extend(self.tabs);
        order.push(self.set_players);
        order.extend([self.tech_player_label, self.unit_types_label, self.enable_label, self.disable_label]);
        order.extend(self.type_buttons);
        order.extend([self.tech_list, self.other_tech_list]);
        order.extend(self.selects);
        order.extend([self.all_players_label, self.display_epoch]);
        order.extend(self.ai_labels);
        for c in order {
            delete(h, c);
        }
        for row in &self.rows {
            let mut order = vec![row.title, row.start_epoch, row.end_epoch, row.name, row.color, row.control];
            order.extend([row.cpu_only, row.shared_los]);
            order.extend(row.values);
            order.extend(row.ai);
            order.extend([row.slider, row.slider_label]);
            for c in order {
                delete(h, c);
            }
        }
    }

    /// A player row's control by `EE_SCREENS_SEPL_ROW_*` (the editor gate's epoch step); 0 for none.
    pub fn row_control(&self, row: u32, which: u32) -> usize {
        match (self.rows.get(row as usize), which) {
            (Some(r), EE_SCREENS_SEPL_ROW_START_EPOCH) => r.start_epoch,
            (Some(r), EE_SCREENS_SEPL_ROW_END_EPOCH) => r.end_epoch,
            _ => 0,
        }
    }
}

/// A row's start and end epoch combo boxes (`ee_screens_sepl_row_control`).
pub const EE_SCREENS_SEPL_ROW_START_EPOCH: u32 = 1;
pub const EE_SCREENS_SEPL_ROW_END_EPOCH: u32 = 2;

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// A recording host: controls are handles from 1 with their records; a rectangle is its record's (right and
    /// bottom one less than left + width and top + height, as `GERectangle(point, w, h)`); display modes, checks,
    /// selections and sliders are kept; every other op is logged.
    #[derive(Default)]
    struct Rec {
        made: Vec<EeScreensSeplCtl>,
        mode: HashMap<usize, i32>,
        on: HashMap<usize, bool>,
        sel: HashMap<usize, u32>,
        value: HashMap<usize, i32>,
        log: Vec<(u32, [usize; 3], [i32; 4])>,
        players: u32,
        computer: bool,
        current_epoch: i32,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            match op {
                EE_SCREENS_SEPL_OP_MAKE => {
                    let c = unsafe { *(p[1] as *const EeScreensSeplCtl) };
                    self.made.push(c);
                    unsafe { *(p[0] as *mut usize) = self.made.len() };
                    0
                }
                EE_SCREENS_SEPL_OP_RECT => {
                    let c = self.made[p[0] - 1];
                    let h = if c.flags & EE_SCREENS_SEPL_FONT_HEIGHT != 0 { 16.0 + c.h } else { c.h.max(24.0) };
                    *f = [c.x, c.y, c.x + c.w.max(100.0) - 1.0, c.y + h - 1.0];
                    0
                }
                EE_SCREENS_SEPL_OP_SET_DISPLAY => {
                    self.mode.insert(p[0], i[0]);
                    0
                }
                EE_SCREENS_SEPL_OP_DISPLAY => *self.mode.get(&p[0]).unwrap_or(&1),
                EE_SCREENS_SEPL_OP_SET_CHECKED => {
                    self.on.insert(p[0], i[0] != 0);
                    0
                }
                EE_SCREENS_SEPL_OP_CHECKED | EE_SCREENS_SEPL_OP_RADIO_CHECKED => {
                    *self.on.get(&p[0]).unwrap_or(&false) as i32
                }
                EE_SCREENS_SEPL_OP_SET_SELECTION => {
                    self.sel.insert(p[0], i[0] as u32);
                    0
                }
                EE_SCREENS_SEPL_OP_COMBO_SELECTION | EE_SCREENS_SEPL_OP_LIST_SELECTION => {
                    *self.sel.get(&p[0]).unwrap_or(&u32::MAX) as i32
                }
                EE_SCREENS_SEPL_OP_SET_SLIDER => {
                    self.value.insert(p[0], i[0]);
                    0
                }
                EE_SCREENS_SEPL_OP_SLIDER => *self.value.get(&p[0]).unwrap_or(&0),
                EE_SCREENS_SEPL_OP_NUM_PLAYERS => self.players as i32,
                EE_SCREENS_SEPL_OP_PLAYER_EXISTS => ((i[0] as u32) < self.players) as i32,
                EE_SCREENS_SEPL_OP_IS_COMPUTER => self.computer as i32,
                EE_SCREENS_SEPL_OP_PLAYER_GET if i[1] == EE_SCREENS_SEPL_PV_CURRENT_EPOCH => self.current_epoch,
                EE_SCREENS_SEPL_OP_MAX_PLAYERS => 4,
                _ => {
                    self.log.push((op, p, *i));
                    0
                }
            }
        }
    }

    fn made() -> (Rec, Players, u32) {
        let mut h = Rec { players: 5, ..Default::default() };
        let mut index = 270;
        let p = Players::new(&mut h, 99, 1000, &mut index);
        (h, p, index)
    }

    #[test]
    fn pair_index_packs_the_upper_triangle() {
        assert_eq!(pair_index(1), 0);
        assert_eq!(pair_index(2), 15);
        assert_eq!(pair_index(3), 29);
        assert_eq!(pair_index(15), 119);
        assert_eq!(pair_index(16), 120);
    }

    #[test]
    fn the_tab_makes_its_controls_in_the_originals_order_and_ids() {
        let (h, p, index) = made();
        // 25 headers, the combo, Set All Players, row 0 (18), rows 1..16 (23 each), 5 tabs; 4 tech labels, 4 type
        // buttons, 2 lists, 4 arrows, the check box; 15 + 30 grid labels, 120 buttons, 2 titles, 6 labels, 2
        // sliders, 5 boxes
        let count = 25 + 1 + 1 + 18 + 16 * 23 + 5 + 4 + 4 + 2 + 4 + 1 + 45 + 120 + 2 + 6 + 2 + 5;
        assert_eq!(h.made.len(), count);
        assert_eq!(index, 270 + count as u32);
        for (k, c) in h.made.iter().enumerate() {
            assert_eq!(c.index, 270 + k as u32);
        }
        // the rows' ids from 400: row 0 takes 18, the others 23 (the title keeps 0x13d but takes an id)
        assert_eq!(h.made[p.rows[0].values[0] - 1].id, 400);
        assert_eq!(h.made[p.rows[1].values[0] - 1].id, 419);
        assert_eq!(h.made[p.rows[1].title - 1].id, 0x13d);
        assert_eq!(h.made[p.rows[1].name - 1].id, 427);
        assert_eq!(h.made[p.rows[2].values[0] - 1].id, 442);
        assert_eq!(h.made[p.rows[16].slider_label - 1].id, 418 + 16 * 23 - 1);
        // the game page's ids from 1200: 15 columns, 15 rows with names, then the grid
        assert_eq!(h.made[p.game.column_labels[0] - 1].id, 1200);
        assert_eq!(h.made[p.game.player_names[14] - 1].id, 1244);
        assert_eq!(h.made[p.game.diplomacy[0] - 1].id, 1245);
        assert_eq!(h.made[p.game.diplomacy[15] - 1].id, 1260);
        assert_eq!(h.made[p.game.diplomacy[119] - 1].id, 1364);
        let b = &h.made[p.game.diplomacy[15] - 1];
        assert_eq!((b.x, b.y), (2.0 * 25.0 + 274.0, 25.0 + 125.0));
        // Build/Repair's header is the one in gU531930
        assert_eq!(h.made[p.ai_labels[4] - 1].font, EE_SCREENS_SEPL_FONT_530);
        // the tabs in the three page groups, each right of the one before, the first checked
        let tab = &h.made[p.tabs[1] - 1];
        assert_eq!(tab.groups, (1 << 4) | (1 << 5) | (1 << 8));
        assert_eq!(tab.x, 100.0);
        assert_eq!(h.made[p.tabs[0] - 1].extra[0], 1);
        // the start epochs' events over their end epochs
        let starts = h.log.iter().filter(|e| e.0 == EE_SCREENS_SEPL_OP_EVENT && e.2[1] == 1).count();
        assert_eq!(starts, 17);
    }

    #[test]
    fn select_page_switches_groups_once() {
        let (mut h, p, _) = made();
        h.log.clear();
        p.select_page(&mut h, EE_SCREENS_SEPL_PAGE1);
        assert!(h.log.is_empty(), "already on page 1");
        p.select_page(&mut h, EE_SCREENS_SEPL_TECH_TREE);
        let groups: Vec<[i32; 4]> = h.log.iter().filter(|e| e.0 == EE_SCREENS_SEPL_OP_GROUP).map(|e| e.2).collect();
        assert_eq!(groups, vec![[4, 0, 0, 0], [5, 1, 0, 0], [8, 0, 0, 0]]);
        assert_eq!(h.mode[&p.player_selection], EE_SCREENS_SEPL_SHOWN);
        p.select_page(&mut h, EE_SCREENS_SEPL_PAGE2);
        assert_eq!(h.mode[&p.civilization_label], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode[&p.title], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(h.mode[&p.player_selection], EE_SCREENS_SEPL_HIDDEN);
    }

    #[test]
    fn rows_show_their_page_controls_by_player_kind() {
        let (mut h, p, _) = made();
        let r = p.rows[2];
        r.show(&mut h, EE_SCREENS_SEPL_PAGE2, 2);
        assert_eq!(h.mode[&r.shared_los], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode[&r.cpu_only], EE_SCREENS_SEPL_HIDDEN, "a human player has no CPU-only box");
        r.show(&mut h, EE_SCREENS_SEPL_AI_SETTINGS, 2);
        assert_eq!(h.mode[&r.ai[0]], EE_SCREENS_SEPL_HIDDEN);
        h.computer = true;
        r.show(&mut h, EE_SCREENS_SEPL_AI_SETTINGS, 2);
        assert_eq!(h.mode[&r.ai[7]], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode[&r.slider_label], EE_SCREENS_SEPL_SHOWN);
        // a row past the players: the AI settings shown (no player to ask)
        h.computer = false;
        p.rows[9].show(&mut h, EE_SCREENS_SEPL_AI_SETTINGS, 9);
        assert_eq!(h.mode[&p.rows[9].ai[0]], EE_SCREENS_SEPL_SHOWN);
    }

    #[test]
    fn copy_from_fills_only_shown_controls_and_shifts_the_end_epoch() {
        let (mut h, p, _) = made();
        let (src, dst) = (p.rows[0], p.rows[3]);
        h.sel.insert(src.start_epoch, 4);
        h.sel.insert(src.end_epoch, 6);
        h.sel.insert(dst.start_epoch, 1);
        h.on.insert(src.ai[1], true);
        h.value.insert(src.slider, 40);
        h.mode.insert(dst.ai[1], EE_SCREENS_SEPL_HIDDEN);
        h.log.clear();
        dst.copy_from(&mut h, &src);
        assert_eq!(h.sel[&dst.start_epoch], 4);
        assert_eq!(h.sel[&dst.end_epoch], 6 - 1 + 4);
        assert!(!h.on.get(&dst.ai[1]).copied().unwrap_or(false), "hidden: not copied");
        assert_eq!(h.value[&dst.slider], 40);
        let copies = h.log.iter().filter(|e| e.0 == EE_SCREENS_SEPL_OP_EDIT_COPY).count();
        assert_eq!(copies, 5, "the caps (row 0 has no name)");
    }

    #[test]
    fn store_posts_the_changed_players_then_clears_them() {
        let (mut h, p, _) = made();
        p.mark_changed(2, false);
        p.mark_changed(5, true);
        p.mark_changed(5, false);
        // no row's start epoch moved: none selected, the players' current epoch "none" too
        h.current_epoch = -1;
        h.log.clear();
        p.store(&mut h);
        let posts: Vec<[i32; 4]> =
            h.log.iter().filter(|e| e.0 == EE_SCREENS_SEPL_OP_LOAD_THREAD).map(|e| e.2).collect();
        assert_eq!(
            posts,
            vec![[406, 2, 0, 0], [406, 5, 0, 0], [403, 2, 0, 0], [403, 5, 1, 0], [407, 2, 0, 0], [407, 5, 0, 0]]
        );
        assert!(p.changes.iter().all(|c| c.get() == (false, false)));
    }

    #[test]
    fn a_moved_start_epoch_marks_the_player_for_a_reload() {
        let (mut h, p, _) = made();
        h.sel.insert(p.rows[1].start_epoch, 2);
        h.sel.insert(p.rows[1].end_epoch, 3);
        h.sel.insert(p.rows[2].start_epoch, 0);
        h.sel.insert(p.rows[2].end_epoch, 0);
        h.current_epoch = 0;
        h.log.clear();
        p.save_row(&mut h, 1);
        p.save_row(&mut h, 2);
        assert_eq!(p.changes[1].get(), (true, true));
        assert_eq!(p.changes[2].get(), (false, false));
        let range = h.log.iter().find(|e| e.0 == EE_SCREENS_SEPL_OP_EPOCH_RANGE).unwrap().2;
        assert_eq!(range, [1, 2, 5, 0]);
    }

    #[test]
    fn the_grid_shows_the_pairs_of_the_players_and_the_limit_per_player() {
        let (mut h, p, _) = made();
        h.value.insert(p.game.unit_limit, 1000);
        h.log.clear();
        p.game.set_player_count(&mut h, 3);
        assert_eq!(h.mode[&p.game.column_labels[1]], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode[&p.game.column_labels[2]], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(h.mode[&p.game.row_labels[1]], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode[&p.game.row_labels[2]], EE_SCREENS_SEPL_HIDDEN);
        // (1, 2), (1, 3), (2, 3) shown; (1, 4), (2, 4), (3, 4) hidden
        for (k, shown) in [(0, true), (1, true), (2, false), (15, true), (16, false), (29, false)] {
            assert_eq!(h.mode[&p.game.diplomacy[k]] == EE_SCREENS_SEPL_SHOWN, shown, "button {k}");
        }
        let per = h.log.iter().find(|e| e.0 == EE_SCREENS_SEPL_OP_LABEL_NUMBER).unwrap().2;
        assert_eq!(per, [333, 1, 0, 0]);
    }

    #[test]
    fn destroy_deletes_every_control_once() {
        let (mut h, p, _) = made();
        h.log.clear();
        p.destroy(&mut h);
        let mut deleted: Vec<usize> =
            h.log.iter().filter(|e| e.0 == EE_SCREENS_SEPL_OP_DELETE).map(|e| e.1[0]).collect();
        assert_eq!(deleted[0], p.title);
        let n = deleted.len();
        deleted.sort_unstable();
        deleted.dedup();
        assert_eq!(deleted.len(), n);
        assert_eq!(n, h.made.len());
    }
}
