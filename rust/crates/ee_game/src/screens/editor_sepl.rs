//! The shared maker's host ops that were C++ statement sequences (`ee_screenscpp_sepl`, Scenario Editor/
//! EESERBUnitTypes.cpp; the scenario editor lane's round 3): the world's and a player's values read and set, the
//! controls' events, the player count's action, the tech type lists and their fill, the player colour, the load
//! thread's posts, the mini map's redraw; composed here of single C++ statements (`EE_SCREENS_SEPX_*`,
//! `ee_screenscpp_sepx`) in the C++'s order. [`compose`] answers an `EE_SCREENS_SEPL_OP_*` op or `None` for the
//! C++'s.

use crate::screens::editor_object::{
    EE_SCREENS_SEPL_ON_BUTTON_UP, EE_SCREENS_SEPL_ON_CHECKED, EE_SCREENS_SEPL_ON_ENTER, EE_SCREENS_SEPL_ON_SELECTION,
    EE_SCREENS_SEPL_ON_UNCHECKED,
};
use crate::screens::editor_players::*;

// ---- the statements (p0 the op's control, i and f as the op's)
/// The world's value `w` (`EE_SCREENS_SEPL_WORLD_*`): `EE_SCREENS_SEPX_WORLD_GET + w` reads it, `_WORLD_SET + w`
/// sets it to i0 (`SetPopulationLimit`, `mNumberOfWondersToWin`, the flags, the game options').
pub const EE_SCREENS_SEPX_WORLD_GET: u32 = 0;
pub const EE_SCREENS_SEPX_WORLD_SET: u32 = 20;
/// Player i0's values: resource cap i2 (read, set to i3), the current, end and start epochs, the shared line of
/// sight (read, `SetSharedLOS(i3 != 0, map)`), the game options' colour and CPU flag (of `mPlayers[i0 - 1]`), the
/// computer player's flag k (`EE_SCREENS_SEPX_AI_GET + k`, `_AI_SET + k` to i3 != 0; `SetActive` for k 0), the
/// citizen percentage (f0; `SetCitizenPercentage((float)i3 * kHeightEpsilon)`).
pub const EE_SCREENS_SEPX_CAP: u32 = 40;
pub const EE_SCREENS_SEPX_SET_CAP: u32 = 41;
pub const EE_SCREENS_SEPX_CURRENT_EPOCH: u32 = 42;
pub const EE_SCREENS_SEPX_END_EPOCH: u32 = 43;
pub const EE_SCREENS_SEPX_START_EPOCH: u32 = 44;
pub const EE_SCREENS_SEPX_LOS: u32 = 45;
pub const EE_SCREENS_SEPX_SET_LOS: u32 = 46;
pub const EE_SCREENS_SEPX_OPTIONS_COLOR: u32 = 47;
pub const EE_SCREENS_SEPX_SET_OPTIONS_COLOR: u32 = 48;
pub const EE_SCREENS_SEPX_OPTIONS_CPU: u32 = 49;
pub const EE_SCREENS_SEPX_SET_OPTIONS_CPU: u32 = 50;
pub const EE_SCREENS_SEPX_CITIZENS: u32 = 51;
pub const EE_SCREENS_SEPX_SET_CITIZENS: u32 = 52;
pub const EE_SCREENS_SEPX_AI_GET: u32 = 60;
pub const EE_SCREENS_SEPX_AI_SET: u32 = 70;
/// The events (their handle answered): `new EESEERust(kSEEStartEpoch, p1)`, `new UIESliderChange(p1, 1, 0)`,
/// `EESEENewUnitLimit(p1, 1, 0)`, `EECreateScreenEvent(EE_SCREENS_SE_DISPLAY_EPOCH)`, `EECreateScreenEvent(i0)`;
/// p0's `SetTriggerEvent(i0, p1)`; p1's `Dereference()`.
pub const EE_SCREENS_SEPX_EVENT_START_EPOCH: u32 = 80;
pub const EE_SCREENS_SEPX_EVENT_SLIDER: u32 = 81;
pub const EE_SCREENS_SEPX_EVENT_UNIT_LIMIT: u32 = 82;
pub const EE_SCREENS_SEPX_EVENT_DISPLAY_EPOCH: u32 = 83;
pub const EE_SCREENS_SEPX_EVENT_SCREEN: u32 = 84;
pub const EE_SCREENS_SEPX_SET_TRIGGER: u32 = 85;
pub const EE_SCREENS_SEPX_EVENT_RELEASE: u32 = 86;
/// `new EESEAChangeWorldPlayerCount(i0)`, `gUserInterface->PostAction(p0)`, its `Dereference()`.
pub const EE_SCREENS_SEPX_COUNT_ACTION: u32 = 87;
pub const EE_SCREENS_SEPX_POST_ACTION: u32 = 88;
pub const EE_SCREENS_SEPX_ACTION_RELEASE: u32 = 89;
/// p0's `EEUIShell::Op(EE_SCREENS_SKOP_TECH_REBUILD)`, `_TECH_CLEAR`, `_TECH_ADD` of list i0's tech i1.
pub const EE_SCREENS_SEPX_TECH_REBUILD: u32 = 90;
pub const EE_SCREENS_SEPX_TECH_CLEAR: u32 = 91;
pub const EE_SCREENS_SEPX_TECH_ADD: u32 = 92;
/// The rules' unit limit minimum, maximum, default.
pub const EE_SCREENS_SEPX_UNIT_LIMIT_MIN: u32 = 93;
pub const EE_SCREENS_SEPX_UNIT_LIMIT_MAX: u32 = 94;
pub const EE_SCREENS_SEPX_UNIT_LIMIT_DEFAULT: u32 = 95;
/// Player i0's colour differs from p0's (`EEGetPlayerColor` of its colour index); `SetColor` of it.
pub const EE_SCREENS_SEPX_COLOR_DIFFERS: u32 = 96;
pub const EE_SCREENS_SEPX_SET_PLAYER_COLOR: u32 = 97;
/// `gMiniMap != NULL`, `gMiniMap->Redraw()`.
pub const EE_SCREENS_SEPX_MINIMAP: u32 = 98;
pub const EE_SCREENS_SEPX_MINIMAP_REDRAW: u32 = 99;
/// The editor's load thread's `Post406(player i0, its start epoch)`, `Post403(i0, i1 != 0)`, `Post407(i0, its start
/// epoch)`.
pub const EE_SCREENS_SEPX_POST_406: u32 = 100;
pub const EE_SCREENS_SEPX_POST_403: u32 = 101;
pub const EE_SCREENS_SEPX_POST_407: u32 = 102;
/// The tech type lists: `gTechTree.GetNodeIDs` into the host's ids, their count, the four lists cleared (units,
/// buildings, heroes, technologies), node id i0's unit class index, `gUnitClassTable[i0] != NULL`, its
/// `IsLandBuilding()`, its `mAIKind`, id i0 pushed on list i1.
pub const EE_SCREENS_SEPX_TT_IDS: u32 = 103;
pub const EE_SCREENS_SEPX_TT_COUNT: u32 = 104;
pub const EE_SCREENS_SEPX_TT_CLEAR: u32 = 105;
/// The host's ids emptied (a fresh list each time, as the C++'s local).
pub const EE_SCREENS_SEPX_TT_IDS_CLEAR: u32 = 106;
pub const EE_SCREENS_SEPX_TT_CLASS: u32 = 109;
pub const EE_SCREENS_SEPX_TT_CLASS_EXISTS: u32 = 110;
pub const EE_SCREENS_SEPX_TT_LAND_BUILDING: u32 = 111;
pub const EE_SCREENS_SEPX_TT_AI_KIND: u32 = 112;
pub const EE_SCREENS_SEPX_TT_PUSH: u32 = 113;
/// List i0's tech count; player i1's node of its tech i2: `mData->mShowInEditor`, `mScenEditorObsoleted`.
pub const EE_SCREENS_SEPX_FT_COUNT: u32 = 114;
pub const EE_SCREENS_SEPX_FT_SHOWN: u32 = 115;
pub const EE_SCREENS_SEPX_FT_OBSOLETED: u32 = 116;

/// The tech type lists (`sSeplV531FB0`, `sSeplBuildingTechs`, `sSeplV531F88`, `sSeplTechnologyTechs`).
const TT_UNITS: i32 = 0;
const TT_BUILDINGS: i32 = 1;
const TT_HEROES: i32 = 2;
const TT_TECHNOLOGIES: i32 = 3;
/// `kAIKindHero`
const AI_KIND_HERO: i32 = 0x1b;
/// The triggers (`kControlTrigger*`): checked, unchecked, selection changed, button up, enter pressed.
const TRIGGER_CHECKED: i32 = 0x3e9;
const TRIGGER_UNCHECKED: i32 = 0x3ea;
const TRIGGER_SELECTION: i32 = 0x3eb;
const TRIGGER_BUTTON_UP: i32 = 0x3f0;
const TRIGGER_ENTER: i32 = 0x3f1;

/// One statement: `x(op, p, i, f)` with p the three handles; the answer, i and f as the C++ leaves them.
pub type Sepx<'a> = dyn FnMut(u32, [usize; 3], &mut [i32; 4], &mut [f32; 4]) -> i32 + 'a;

/// A statement over a copy of its integers.
fn st(x: &mut Sepx, op: u32, p: [usize; 3], a: [i32; 4]) -> i32 {
    let mut a = a;
    x(op, p, &mut a, &mut [0.0; 4])
}

/// A making statement: the object's handle, which the C++ answers in i0 (low 32 bits) and i1 (high).
fn mk(x: &mut Sepx, op: u32, p: [usize; 3], a: [i32; 4]) -> usize {
    let mut a = a;
    x(op, p, &mut a, &mut [0.0; 4]);
    (a[0] as u32 as usize) | ((a[1] as u32 as usize) << 32)
}

/// The op composed, or `None` for the C++ host's.
pub fn compose(x: &mut Sepx, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    let ii = *i;
    Some(match op {
        // SAFETY: the op's contract (p1 the record, p0 the out pointer)
        EE_SCREENS_SEPL_OP_MAKE => unsafe { make(x, p) },
        EE_SCREENS_SEPL_OP_WORLD_GET => st(x, EE_SCREENS_SEPX_WORLD_GET + i[0] as u32, p, ii),
        EE_SCREENS_SEPL_OP_WORLD_SET => {
            st(x, EE_SCREENS_SEPX_WORLD_SET + i[0] as u32, p, [i[1], 0, 0, 0]);
            0
        }
        EE_SCREENS_SEPL_OP_PLAYER_GET | EE_SCREENS_SEPL_OP_PLAYER_SET => {
            let set = op == EE_SCREENS_SEPL_OP_PLAYER_SET;
            let pair = |x: &mut Sepx, get: u32, put: u32| {
                if set {
                    st(x, put, p, ii);
                }
                st(x, get, p, ii)
            };
            match i[1] {
                EE_SCREENS_SEPL_PV_CAP => pair(x, EE_SCREENS_SEPX_CAP, EE_SCREENS_SEPX_SET_CAP),
                EE_SCREENS_SEPL_PV_CURRENT_EPOCH => st(x, EE_SCREENS_SEPX_CURRENT_EPOCH, p, ii),
                EE_SCREENS_SEPL_PV_END_EPOCH => st(x, EE_SCREENS_SEPX_END_EPOCH, p, ii),
                EE_SCREENS_SEPL_PV_START_EPOCH => st(x, EE_SCREENS_SEPX_START_EPOCH, p, ii),
                EE_SCREENS_SEPL_PV_SHARED_LOS => pair(x, EE_SCREENS_SEPX_LOS, EE_SCREENS_SEPX_SET_LOS),
                EE_SCREENS_SEPL_PV_OPTIONS_COLOR => {
                    pair(x, EE_SCREENS_SEPX_OPTIONS_COLOR, EE_SCREENS_SEPX_SET_OPTIONS_COLOR)
                }
                EE_SCREENS_SEPL_PV_OPTIONS_CPU => pair(x, EE_SCREENS_SEPX_OPTIONS_CPU, EE_SCREENS_SEPX_SET_OPTIONS_CPU),
                EE_SCREENS_SEPL_PV_AI => {
                    let k = i[2] as u32;
                    pair(x, EE_SCREENS_SEPX_AI_GET + k, EE_SCREENS_SEPX_AI_SET + k)
                }
                EE_SCREENS_SEPL_PV_CITIZENS => {
                    if set {
                        st(x, EE_SCREENS_SEPX_SET_CITIZENS, p, ii);
                    }
                    x(EE_SCREENS_SEPX_CITIZENS, p, i, f);
                    0
                }
                _ => 0,
            }
        }
        EE_SCREENS_SEPL_OP_EVENT => {
            let make = match i[1] {
                EE_SCREENS_SEPL_EVENT_START_EPOCH => EE_SCREENS_SEPX_EVENT_START_EPOCH,
                EE_SCREENS_SEPL_EVENT_SLIDER => EE_SCREENS_SEPX_EVENT_SLIDER,
                EE_SCREENS_SEPL_EVENT_UNIT_LIMIT => EE_SCREENS_SEPX_EVENT_UNIT_LIMIT,
                EE_SCREENS_SEPL_EVENT_DISPLAY_EPOCH => EE_SCREENS_SEPX_EVENT_DISPLAY_EPOCH,
                _ => return None,
            };
            let event = mk(x, make, p, ii);
            if i[1] == EE_SCREENS_SEPL_EVENT_DISPLAY_EPOCH {
                // one event on both the checked and the unchecked trigger
                st(x, EE_SCREENS_SEPX_SET_TRIGGER, [p[0], event, 0], [TRIGGER_UNCHECKED, 0, 0, 0]);
            }
            st(x, EE_SCREENS_SEPX_SET_TRIGGER, [p[0], event, 0], [i[0], 0, 0, 0]);
            st(x, EE_SCREENS_SEPX_EVENT_RELEASE, [0, event, 0], [0; 4]);
            0
        }
        EE_SCREENS_SEPL_OP_SCREEN_EVENT => {
            let event = mk(x, EE_SCREENS_SEPX_EVENT_SCREEN, p, ii);
            for (flag, trigger) in [
                (EE_SCREENS_SEPL_ON_CHECKED, TRIGGER_CHECKED),
                (EE_SCREENS_SEPL_ON_UNCHECKED, TRIGGER_UNCHECKED),
                (EE_SCREENS_SEPL_ON_SELECTION, TRIGGER_SELECTION),
                (EE_SCREENS_SEPL_ON_BUTTON_UP, TRIGGER_BUTTON_UP),
                (EE_SCREENS_SEPL_ON_ENTER, TRIGGER_ENTER),
            ] {
                if i[1] & flag != 0 {
                    st(x, EE_SCREENS_SEPX_SET_TRIGGER, [p[0], event, 0], [trigger, 0, 0, 0]);
                }
            }
            st(x, EE_SCREENS_SEPX_EVENT_RELEASE, [0, event, 0], [0; 4]);
            0
        }
        EE_SCREENS_SEPL_OP_POST_PLAYER_COUNT => {
            let action = mk(x, EE_SCREENS_SEPX_COUNT_ACTION, p, ii);
            st(x, EE_SCREENS_SEPX_POST_ACTION, [action, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEPX_ACTION_RELEASE, [action, 0, 0], [0; 4]);
            0
        }
        EE_SCREENS_SEPL_OP_REBUILD_TECHS => {
            st(x, EE_SCREENS_SEPX_TECH_REBUILD, [p[0], 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEPX_TECH_REBUILD, [p[1], 0, 0], [0; 4]);
            0
        }
        EE_SCREENS_SEPL_OP_UNIT_LIMIT_RULES => {
            i[0] = st(x, EE_SCREENS_SEPX_UNIT_LIMIT_MIN, p, [0; 4]);
            i[1] = st(x, EE_SCREENS_SEPX_UNIT_LIMIT_MAX, p, [0; 4]);
            i[2] = st(x, EE_SCREENS_SEPX_UNIT_LIMIT_DEFAULT, p, [0; 4]);
            0
        }
        EE_SCREENS_SEPL_OP_APPLY_COLOR => {
            let differs = st(x, EE_SCREENS_SEPX_COLOR_DIFFERS, p, ii);
            st(x, EE_SCREENS_SEPX_SET_PLAYER_COLOR, p, ii);
            differs
        }
        EE_SCREENS_SEPL_OP_MINIMAP_REDRAW => {
            if st(x, EE_SCREENS_SEPX_MINIMAP, p, [0; 4]) != 0 {
                st(x, EE_SCREENS_SEPX_MINIMAP_REDRAW, p, [0; 4]);
            }
            0
        }
        EE_SCREENS_SEPL_OP_LOAD_THREAD => {
            let post = match i[0] {
                406 => EE_SCREENS_SEPX_POST_406,
                403 => EE_SCREENS_SEPX_POST_403,
                _ => EE_SCREENS_SEPX_POST_407,
            };
            st(x, post, p, [i[1], i[2], 0, 0]);
            0
        }
        EE_SCREENS_SEPL_OP_TECH_TYPES => {
            st(x, EE_SCREENS_SEPX_TT_IDS_CLEAR, p, [0; 4]);
            st(x, EE_SCREENS_SEPX_TT_IDS, p, [0; 4]);
            let count = st(x, EE_SCREENS_SEPX_TT_COUNT, p, [0; 4]) as u32;
            for list in [TT_UNITS, TT_BUILDINGS, TT_TECHNOLOGIES, TT_HEROES] {
                st(x, EE_SCREENS_SEPX_TT_CLEAR, p, [list, 0, 0, 0]);
            }
            for n in 0..count as i32 {
                let class = st(x, EE_SCREENS_SEPX_TT_CLASS, p, [n, 0, 0, 0]);
                let exists = class >= 0 && st(x, EE_SCREENS_SEPX_TT_CLASS_EXISTS, p, [class, 0, 0, 0]) != 0;
                let list = if !exists {
                    TT_TECHNOLOGIES
                } else if st(x, EE_SCREENS_SEPX_TT_LAND_BUILDING, p, [class, 0, 0, 0]) != 0 {
                    TT_BUILDINGS
                } else if st(x, EE_SCREENS_SEPX_TT_AI_KIND, p, [class, 0, 0, 0]) == AI_KIND_HERO {
                    TT_HEROES
                } else {
                    TT_UNITS
                };
                st(x, EE_SCREENS_SEPX_TT_PUSH, p, [n, list, 0, 0]);
            }
            0
        }
        EE_SCREENS_SEPL_OP_FILL_TECHS => {
            // p0 the list, p1 the other, p2 the player combo (its selection + 1 the player), i0 the tech list
            let selection = st(x, EE_SCREENS_SEPL_OP_COMBO_SELECTION_RAW, [p[2], 0, 0], [0; 4]);
            let player = selection.wrapping_add(1);
            st(x, EE_SCREENS_SEPX_TECH_CLEAR, [p[1], 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEPX_TECH_CLEAR, [p[0], 0, 0], [0; 4]);
            let count = st(x, EE_SCREENS_SEPX_FT_COUNT, p, [i[0], 0, 0, 0]) as u32;
            for k in 0..count as i32 {
                if st(x, EE_SCREENS_SEPX_FT_SHOWN, p, [i[0], player, k, 0]) != 0 {
                    let obsoleted = st(x, EE_SCREENS_SEPX_FT_OBSOLETED, p, [i[0], player, k, 0]) != 0;
                    st(x, EE_SCREENS_SEPX_TECH_ADD, [if obsoleted { p[1] } else { p[0] }, 0, 0], [i[0], k, 0, 0]);
                }
            }
            st(x, EE_SCREENS_SEPX_TECH_REBUILD, [p[0], 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEPX_TECH_REBUILD, [p[1], 0, 0], [0; 4]);
            0
        }
        _ => return crate::screens::editor_seplx::compose(x, op, p, i, f),
    })
}

/// The combo box p0's `GetCurrentSelection` as the C++ wrote it into its `unsigned long` (`ee_screenscpp_sepx`).
pub const EE_SCREENS_SEPL_OP_COMBO_SELECTION_RAW: u32 = 117;

// ---- the shared maker's control (`SeplMake`): its statements on the record p0, the form p1 and the made control p2
// (`ee_screenscpp_sepx`); the rectangle in f (left, top, right, bottom)
/// `SeplFont(i0)->GetHeight()`.
pub const EE_SCREENS_SEPX_FONT_HEIGHT: u32 = 120;
/// The controls by kind (their handle answered), each with the record's font, text, numbers and the rectangle f:
/// a label, an edit box, a combo box (with the epochs' titles), the max player combo, a check box, a slider, the unit
/// types radio, the tech list, the select, tab, set players, colour and diplomacy buttons, a radio, a tri-state, the
/// object edit, the trigger screens' combo boxes (a shell of kind i0, the trigger's and the scenario's with their
/// arguments, the plain one), a text button, an arrow, the unit lists (plain, effect, object, the unit tab's).
pub const EE_SCREENS_SEPX_MAKE_LABEL: u32 = 121;
pub const EE_SCREENS_SEPX_MAKE_EDIT: u32 = 122;
pub const EE_SCREENS_SEPX_MAKE_COMBO: u32 = 123;
pub const EE_SCREENS_SEPX_MAKE_COMBO_EPOCHS: u32 = 124;
pub const EE_SCREENS_SEPX_MAKE_MAX_PLAYER: u32 = 125;
pub const EE_SCREENS_SEPX_MAKE_CHECK: u32 = 126;
pub const EE_SCREENS_SEPX_MAKE_SLIDER: u32 = 127;
pub const EE_SCREENS_SEPX_MAKE_UNIT_TYPE: u32 = 128;
pub const EE_SCREENS_SEPX_MAKE_TECH_LIST: u32 = 129;
pub const EE_SCREENS_SEPX_MAKE_SELECT: u32 = 130;
pub const EE_SCREENS_SEPX_MAKE_TAB: u32 = 131;
pub const EE_SCREENS_SEPX_MAKE_SET_PLAYERS: u32 = 132;
pub const EE_SCREENS_SEPX_MAKE_COLOR: u32 = 133;
pub const EE_SCREENS_SEPX_MAKE_DIPLOMACY: u32 = 134;
pub const EE_SCREENS_SEPX_MAKE_RADIO: u32 = 135;
pub const EE_SCREENS_SEPX_MAKE_TRISTATE: u32 = 136;
pub const EE_SCREENS_SEPX_MAKE_OBJ_EDIT: u32 = 137;
pub const EE_SCREENS_SEPX_MAKE_SHELL_COMBO: u32 = 138;
pub const EE_SCREENS_SEPX_MAKE_TRIGGER_COMBO: u32 = 139;
pub const EE_SCREENS_SEPX_MAKE_SCENARIO_COMBO: u32 = 140;
pub const EE_SCREENS_SEPX_MAKE_PLAIN_COMBO: u32 = 141;
pub const EE_SCREENS_SEPX_MAKE_BUTTON: u32 = 142;
pub const EE_SCREENS_SEPX_MAKE_ARROW: u32 = 143;
pub const EE_SCREENS_SEPX_MAKE_LIST_PLAIN: u32 = 144;
pub const EE_SCREENS_SEPX_MAKE_LIST_EFFECT: u32 = 145;
pub const EE_SCREENS_SEPX_MAKE_LIST_OBJECT: u32 = 146;
/// (the unit tab's lists: an `EEUIShellList` of shell kind i0)
pub const EE_SCREENS_SEPX_MAKE_LIST_UNIT: u32 = 147;
/// The made control p2: `SetAlignment(i0)`, `SetBgOpacity(f0)`, its text label's `SetBgOpacity(f0)` (a radio),
/// `SeplColor(its GEText, i0)` by class (a label, an edit box, a combo box, a list, a button, a check box's, a
/// radio's, a tri-state's label), the edit box's `SetText(the record's text)`, a combo box's `SetTextBoxBgOpacity(f0)`,
/// `SetListBoxSelectedBgColor(blue)`, `SetCurrentSelection(i0)`, `SetListBoxAlwaysDirty(true)`, a slider's
/// `SetPageSize(i0)`, `SetRange(i0, i1)`, `SetCurrentRangeValue(i0)`, a list's `SetScrollBarBgOpacity(f0)`, its selected
/// background blue, a button's `SetText(the record's text, its font)`, `SetDisplayMode(hidden)`, the form's
/// `AddControl(it, i0)`.
pub const EE_SCREENS_SEPX_ALIGN: u32 = 150;
pub const EE_SCREENS_SEPX_BG: u32 = 151;
pub const EE_SCREENS_SEPX_RADIO_LABEL_BG: u32 = 152;
pub const EE_SCREENS_SEPX_COLOR_LABEL: u32 = 153;
pub const EE_SCREENS_SEPX_COLOR_EDIT: u32 = 154;
pub const EE_SCREENS_SEPX_COLOR_COMBO: u32 = 155;
pub const EE_SCREENS_SEPX_COLOR_LIST: u32 = 156;
pub const EE_SCREENS_SEPX_COLOR_BUTTON: u32 = 157;
pub const EE_SCREENS_SEPX_COLOR_CHECK: u32 = 158;
pub const EE_SCREENS_SEPX_COLOR_RADIO: u32 = 159;
pub const EE_SCREENS_SEPX_COLOR_TRISTATE: u32 = 160;
pub const EE_SCREENS_SEPX_EDIT_TEXT: u32 = 161;
pub const EE_SCREENS_SEPX_COMBO_TEXT_BG: u32 = 162;
pub const EE_SCREENS_SEPX_COMBO_SELECT_BLUE: u32 = 163;
pub const EE_SCREENS_SEPX_COMBO_SELECT: u32 = 164;
pub const EE_SCREENS_SEPX_COMBO_ALWAYS_DIRTY: u32 = 165;
pub const EE_SCREENS_SEPX_SLIDER_PAGE: u32 = 166;
pub const EE_SCREENS_SEPX_SLIDER_RANGE: u32 = 167;
pub const EE_SCREENS_SEPX_SLIDER_VALUE: u32 = 168;
pub const EE_SCREENS_SEPX_LIST_SCROLL_BG: u32 = 169;
pub const EE_SCREENS_SEPX_LIST_SELECT_BLUE: u32 = 170;
pub const EE_SCREENS_SEPX_BUTTON_TEXT: u32 = 171;
pub const EE_SCREENS_SEPX_HIDE: u32 = 172;
pub const EE_SCREENS_SEPX_ADD_CONTROL: u32 = 173;

/// `SeplMake`: the record p1's control made on the form p2 and written to the out pointer p0 (0 for an unknown
/// kind): its rectangle (the record's left, top, right, bottom, or its location and size with the font's height
/// added), the control by kind, its settings in the C++'s order, hidden before it joins its groups.
///
/// # Safety
/// p1 points to a live record, p0 to the out pointer.
unsafe fn make(x: &mut Sepx, p: [usize; 3]) -> i32 {
    use crate::screens::editor_area::{EE_SCREENS_SEPL_FG_RED, EE_SCREENS_SEPL_NO_SELECT_COLOR};
    use crate::screens::editor_controls::{
        EE_SCREENS_SEPL_COMBO_CONDITION, EE_SCREENS_SEPL_COMBO_EFFECT, EE_SCREENS_SEPL_LIST_EFFECT,
        EE_SCREENS_SEPL_NO_RANGE,
    };
    use crate::screens::editor_elevation::EE_SCREENS_SEPL_LABEL_BG;
    use crate::screens::editor_map::EE_SCREENS_SEPL_FG_WHITE;
    use crate::screens::editor_object::{
        EE_SCREENS_SEPL_COMBO_OBJECT, EE_SCREENS_SEPL_COMBO_PLAYER, EE_SCREENS_SEPL_COMBO_SCENARIO,
        EE_SCREENS_SEPL_COMBO_TRIGGER,
    };
    use crate::screens::editor_tscreen::{EE_SCREENS_SEPL_HIDE, EE_SCREENS_SEPL_LIST_PLAIN};
    use crate::screens::editor_unit::{EE_SCREENS_SEPL_LIST_OBJECT, EE_SCREENS_SEPL_LIST_UNIT_TYPES};
    use crate::screens::shells::*;
    // SAFETY: the contract
    let c = unsafe { *(p[1] as *const EeScreensSeplCtl) };
    let (ctl, form) = (p[1], p[2]);
    let mut height = c.h;
    if c.flags & EE_SCREENS_SEPL_FONT_HEIGHT != 0 {
        let font = if c.font == EE_SCREENS_SEPL_FONT_530 { EE_SCREENS_SEPL_FONT_SCREEN } else { c.font };
        height = st(x, EE_SCREENS_SEPX_FONT_HEIGHT, [ctl, form, 0], [font as i32, 0, 0, 0]) as f32 + c.h;
    }
    // GERectangle(location, w, h): right = left + (w - 1), bottom = top + (h - 1)
    let sized = |w: f32, h: f32| [c.x, c.y, c.x + (w - 1.0), c.y + (h - 1.0)];
    let rect = if c.flags & EE_SCREENS_SEPL_LTRB != 0 { [c.x, c.y, c.w, c.h] } else { sized(c.w, height) };
    let whole = |v: f32| (v as u32) as f32;
    let fg = c.flags & EE_SCREENS_SEPL_FG != 0;
    let colored = c.flags & (EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FG_WHITE | EE_SCREENS_SEPL_FG_RED) != 0;
    // a making statement over the rectangle `r`, a setting on the made control `h`
    let made = |x: &mut Sepx, op: u32, r: [f32; 4], a: [i32; 4]| -> usize {
        let mut a = a;
        let mut r = r;
        x(op, [ctl, form, 0], &mut a, &mut r);
        (a[0] as u32 as usize) | ((a[1] as u32 as usize) << 32)
    };
    let set = |x: &mut Sepx, h: usize, op: u32, a: [i32; 4], v: f32| {
        let mut a = a;
        x(op, [ctl, form, h], &mut a, &mut [v, 0.0, 0.0, 0.0]);
    };
    let flags = c.flags as i32;
    let yellow = EE_SCREENS_SEPL_FG as i32;
    let h = match c.kind {
        EE_SCREENS_SEPL_LABEL => {
            let h = made(x, EE_SCREENS_SEPX_MAKE_LABEL, rect, [0; 4]);
            if c.align >= 0 {
                set(x, h, EE_SCREENS_SEPX_ALIGN, [c.align, 0, 0, 0], 0.0);
            }
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            if colored {
                set(x, h, EE_SCREENS_SEPX_COLOR_LABEL, [flags, 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_EDIT => {
            let h = made(x, EE_SCREENS_SEPX_MAKE_EDIT, rect, [0; 4]);
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            if c.text != EE_SCREENS_SEPL_TEXT_NONE {
                set(x, h, EE_SCREENS_SEPX_EDIT_TEXT, [0; 4], 0.0);
            }
            if colored {
                set(x, h, EE_SCREENS_SEPX_COLOR_EDIT, [flags, 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_COMBO => {
            let op = if c.extra[0] == EE_SCREENS_SEPL_LINES_EPOCHS {
                EE_SCREENS_SEPX_MAKE_COMBO_EPOCHS
            } else {
                EE_SCREENS_SEPX_MAKE_COMBO
            };
            let h = made(x, op, rect, [0; 4]);
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            if c.bg2 >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_COMBO_TEXT_BG, [0; 4], c.bg2);
            }
            if fg {
                set(x, h, EE_SCREENS_SEPX_COLOR_COMBO, [yellow, 0, 0, 0], 0.0);
            }
            set(x, h, EE_SCREENS_SEPX_COMBO_SELECT_BLUE, [0; 4], 0.0);
            if c.extra[1] >= 0 {
                set(x, h, EE_SCREENS_SEPX_COMBO_SELECT, [c.extra[1], 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_MAX_PLAYER => {
            let h = made(x, EE_SCREENS_SEPX_MAKE_MAX_PLAYER, sized(whole(c.w), 20.0), [0; 4]);
            if fg {
                set(x, h, EE_SCREENS_SEPX_COLOR_COMBO, [yellow, 0, 0, 0], 0.0);
            }
            set(x, h, EE_SCREENS_SEPX_COMBO_SELECT_BLUE, [0; 4], 0.0);
            if c.bg2 >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_COMBO_TEXT_BG, [0; 4], c.bg2);
            }
            h
        }
        EE_SCREENS_SEPL_CHECK | EE_SCREENS_SEPL_TRISTATE => {
            let tri = c.kind == EE_SCREENS_SEPL_TRISTATE;
            let h = made(x, if tri { EE_SCREENS_SEPX_MAKE_TRISTATE } else { EE_SCREENS_SEPX_MAKE_CHECK }, rect, [0; 4]);
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            if fg {
                let op = if tri { EE_SCREENS_SEPX_COLOR_TRISTATE } else { EE_SCREENS_SEPX_COLOR_CHECK };
                set(x, h, op, [yellow, 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_SLIDER => {
            let h = made(x, EE_SCREENS_SEPX_MAKE_SLIDER, rect, [0; 4]);
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            let page = |x: &mut Sepx| set(x, h, EE_SCREENS_SEPX_SLIDER_PAGE, [c.extra[2], 0, 0, 0], 0.0);
            let range = |x: &mut Sepx| set(x, h, EE_SCREENS_SEPX_SLIDER_RANGE, [c.extra[0], c.extra[1], 0, 0], 0.0);
            if c.flags & EE_SCREENS_SEPL_NO_RANGE != 0 {
                if c.extra[2] != 0 {
                    page(x);
                }
            } else if c.flags & EE_SCREENS_SEPL_RANGE_FIRST != 0 {
                range(x);
                page(x);
            } else {
                page(x);
                range(x);
            }
            if c.extra[3] != i32::MIN && c.flags & EE_SCREENS_SEPL_NO_RANGE == 0 {
                set(x, h, EE_SCREENS_SEPX_SLIDER_VALUE, [c.extra[3], 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_UNIT_TYPE | EE_SCREENS_SEPL_RADIO => {
            let unit_type = c.kind == EE_SCREENS_SEPL_UNIT_TYPE;
            let h = if unit_type {
                made(x, EE_SCREENS_SEPX_MAKE_UNIT_TYPE, sized(whole(c.w), whole(c.h)), [0; 4])
            } else {
                made(x, EE_SCREENS_SEPX_MAKE_RADIO, rect, [0; 4])
            };
            if c.bg >= 0.0 && (unit_type || c.flags & EE_SCREENS_SEPL_LABEL_BG != 0) {
                set(x, h, EE_SCREENS_SEPX_RADIO_LABEL_BG, [0; 4], c.bg);
            } else if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            if fg {
                set(x, h, EE_SCREENS_SEPX_COLOR_RADIO, [yellow, 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_TECH_LIST => {
            let h = made(x, EE_SCREENS_SEPX_MAKE_TECH_LIST, sized(whole(c.w), whole(c.h)), [0; 4]);
            if fg {
                set(x, h, EE_SCREENS_SEPX_COLOR_LIST, [yellow, 0, 0, 0], 0.0);
            }
            set(x, h, EE_SCREENS_SEPX_LIST_SCROLL_BG, [0; 4], c.bg2);
            set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            h
        }
        EE_SCREENS_SEPL_SELECT | EE_SCREENS_SEPL_TAB | EE_SCREENS_SEPL_SET_PLAYERS => {
            let (op, w) = match c.kind {
                EE_SCREENS_SEPL_SELECT => (EE_SCREENS_SEPX_MAKE_SELECT, 40.0),
                EE_SCREENS_SEPL_TAB => (EE_SCREENS_SEPX_MAKE_TAB, 150.0),
                _ => (EE_SCREENS_SEPX_MAKE_SET_PLAYERS, 100.0),
            };
            let h = made(x, op, sized(w, 25.0), [0; 4]);
            set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            h
        }
        EE_SCREENS_SEPL_COLOR => made(x, EE_SCREENS_SEPX_MAKE_COLOR, sized(25.0, 25.0), [0; 4]),
        EE_SCREENS_SEPL_DIPLOMACY => made(x, EE_SCREENS_SEPX_MAKE_DIPLOMACY, sized(23.0, 23.0), [0; 4]),
        EE_SCREENS_SEPL_OBJ_EDIT => {
            let h = made(x, EE_SCREENS_SEPX_MAKE_OBJ_EDIT, sized(c.w, whole(height)), [0; 4]);
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            if fg {
                set(x, h, EE_SCREENS_SEPX_COLOR_EDIT, [yellow, 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_LIST_COMBO => {
            let shell = match c.extra[0] {
                EE_SCREENS_SEPL_COMBO_OBJECT => {
                    Some((EE_SCREENS_SEPX_MAKE_SHELL_COMBO, EE_SCREENS_SK_OBJECT_COMBO, false))
                }
                EE_SCREENS_SEPL_COMBO_PLAYER => {
                    Some((EE_SCREENS_SEPX_MAKE_SHELL_COMBO, EE_SCREENS_SK_PLAYER_COMBO, false))
                }
                EE_SCREENS_SEPL_COMBO_TRIGGER => {
                    Some((EE_SCREENS_SEPX_MAKE_TRIGGER_COMBO, EE_SCREENS_SK_TRIGGER_COMBO, true))
                }
                EE_SCREENS_SEPL_COMBO_SCENARIO => {
                    Some((EE_SCREENS_SEPX_MAKE_SCENARIO_COMBO, EE_SCREENS_SK_SCENARIO_COMBO, true))
                }
                EE_SCREENS_SEPL_COMBO_CONDITION => {
                    Some((EE_SCREENS_SEPX_MAKE_SHELL_COMBO, EE_SCREENS_SK_CONDITION_COMBO, false))
                }
                EE_SCREENS_SEPL_COMBO_EFFECT => {
                    Some((EE_SCREENS_SEPX_MAKE_SHELL_COMBO, EE_SCREENS_SK_EFFECT_COMBO, false))
                }
                _ => None,
            };
            let h = match shell {
                // the trigger's and the scenario's combo boxes are as wide as the record, the others' a whole number
                Some((op, kind, exact)) => {
                    made(x, op, sized(if exact { c.w } else { whole(c.w) }, 20.0), [kind as i32, 0, 0, 0])
                }
                None => made(x, EE_SCREENS_SEPX_MAKE_PLAIN_COMBO, rect, [0; 4]),
            };
            if c.bg2 >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_COMBO_TEXT_BG, [0; 4], c.bg2);
            }
            if c.extra[3] != 0 {
                set(x, h, EE_SCREENS_SEPX_COMBO_ALWAYS_DIRTY, [0; 4], 0.0);
            }
            if colored {
                set(x, h, EE_SCREENS_SEPX_COLOR_COMBO, [flags, 0, 0, 0], 0.0);
            }
            if c.flags & EE_SCREENS_SEPL_NO_SELECT_COLOR == 0 {
                set(x, h, EE_SCREENS_SEPX_COMBO_SELECT_BLUE, [0; 4], 0.0);
            }
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            h
        }
        EE_SCREENS_SEPL_BUTTON => {
            let h = made(x, EE_SCREENS_SEPX_MAKE_BUTTON, rect, [0; 4]);
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            set(x, h, EE_SCREENS_SEPX_BUTTON_TEXT, [0; 4], 0.0);
            if fg {
                set(x, h, EE_SCREENS_SEPX_COLOR_BUTTON, [yellow, 0, 0, 0], 0.0);
            }
            h
        }
        EE_SCREENS_SEPL_ARROW => made(x, EE_SCREENS_SEPX_MAKE_ARROW, rect, [0; 4]),
        EE_SCREENS_SEPL_UNIT_LIST => {
            let h = match c.extra[0] {
                EE_SCREENS_SEPL_LIST_PLAIN => made(x, EE_SCREENS_SEPX_MAKE_LIST_PLAIN, rect, [0; 4]),
                EE_SCREENS_SEPL_LIST_EFFECT => made(x, EE_SCREENS_SEPX_MAKE_LIST_EFFECT, rect, [0; 4]),
                EE_SCREENS_SEPL_LIST_OBJECT => {
                    made(x, EE_SCREENS_SEPX_MAKE_LIST_OBJECT, sized(c.w, whole(c.h)), [0; 4])
                }
                _ => {
                    // the unit tab's lists (once EESEUnitNewList): the class list or the cargo list by kind
                    use crate::screens::shells_unit::{EE_SCREENS_SK_CARGO_LIST, EE_SCREENS_SK_UNIT_CLASS_LIST};
                    let kind = if c.extra[0] == EE_SCREENS_SEPL_LIST_UNIT_TYPES {
                        EE_SCREENS_SK_UNIT_CLASS_LIST
                    } else {
                        EE_SCREENS_SK_CARGO_LIST
                    };
                    made(x, EE_SCREENS_SEPX_MAKE_LIST_UNIT, sized(whole(c.w), whole(c.h)), [kind as i32, 0, 0, 0])
                }
            };
            if c.bg >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_BG, [0; 4], c.bg);
            }
            if c.bg2 >= 0.0 {
                set(x, h, EE_SCREENS_SEPX_LIST_SCROLL_BG, [0; 4], c.bg2);
            }
            if fg {
                set(x, h, EE_SCREENS_SEPX_COLOR_LIST, [yellow, 0, 0, 0], 0.0);
            }
            set(x, h, EE_SCREENS_SEPX_LIST_SELECT_BLUE, [0; 4], 0.0);
            h
        }
        _ => 0,
    };
    if h != 0 && c.flags & EE_SCREENS_SEPL_HIDE != 0 {
        // hidden before it joins its groups
        set(x, h, EE_SCREENS_SEPX_HIDE, [0; 4], 0.0);
    }
    for g in 0..32 {
        if h != 0 && (c.groups >> g) & 1 != 0 {
            set(x, h, EE_SCREENS_SEPX_ADD_CONTROL, [g, 0, 0, 0], 0.0);
        }
    }
    // SAFETY: the contract
    unsafe { *(p[0] as *mut usize) = h };
    0
}

#[cfg(test)]
#[path = "editor_sepl_tests.rs"]
mod tests;
