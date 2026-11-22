//! The trigger tab's host ops that were C++ statement sequences (the scenario editor lane's round 4): the effect and
//! condition screens' generic host (`ee_screenscpp_seet` / `ee_screenscpp_sect`, Scenario Editor/EESETSEffect.cpp:
//! `SeetOp`'s sequences, the entry, its texts and objects, the scenario's lists, the place fills, the tech names and
//! their sort), the object screen's (`ee_screenscpp_seob`, EESETSObject.cpp), the trigger screen's
//! (`ee_screenscpp_setr`, EESETSTrigger.cpp, and `EESETriggerJump`), the trigger tab's (`ee_screenscpp_sets`,
//! EESETriggerScreen.cpp), the area screen's (`ee_screenscpp_sear`, EESETSArea.cpp) and the instructions tab's
//! (`ee_screenscpp_sein`, EESEInstructions.cpp); composed here of single C++ statements in the C++'s order.
//!
//! The statements are numbered once for the family (one C++ statement host per file: `ee_screenscpp_seetx` 0..199,
//! `_seobx` 200.., `_setrx` 300.., `_setsx` 400.., `_searx` 500.., `_seinx` 600..) and the toolkit's own calls
//! (`EE_SCREENS_TRGX_R_*`, 900..) are answered in Rust by the live adapter (`editor_trigx_ffi.rs`) through
//! `ee_ui::direct`, so the streams below are the whole sequence in the tests. A statement's arguments: p0 (and p1)
//! its objects, i and f its numbers; a statement that makes or finds an object writes it to the word p2 points at;
//! a statement that reads a text writes it into the buffer p2 (capacity i3) and answers its whole length; a text
//! passed in is p1's bytes, i0 of them (p0's for a statement with no object).

// (the casts of the C `unsigned long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_ulong};

use ee_sim::scenario::lists::*;

use crate::screens::editor_area::*;
use crate::screens::editor_effects::*;
use crate::screens::editor_instructions::*;
use crate::screens::editor_object::*;
use crate::screens::editor_trigger::*;
use crate::screens::editor_tscreen::{
    EE_SCREENS_SETS_OP_DRAW, EE_SCREENS_SETS_OP_IMAGE_BUTTON, EE_SCREENS_SETS_OP_NAMES, EE_SCREENS_SETS_OP_SHOW_IDS,
    EE_SCREENS_SETS_OP_TAB, EE_SCREENS_SETS_OP_VIEW,
};
use crate::screens::scenario_editor::{EE_SCREENS_SE_SHOW_DEBUG_IDS, EE_SCREENS_SE_SHOW_TAB};
use crate::screens::shells::{EE_SCREENS_SKOP_SELECTED_PLAYER, EE_SCREENS_SKOP_SET_FLAGS, EE_SCREENS_SKOP_SET_MODE};
use crate::screens::shells::{EE_SCREENS_SKOP_SET_PLAYER_COUNT, EE_SCREENS_SKOP_SET_SELECTED_PLAYER};

/// One statement: `x(op, p, i, f)`; the answer, i and f as the statement leaves them.
pub type Trgx<'a> = dyn FnMut(u32, [usize; 3], &mut [i32; 4], &mut [f32; 4]) -> i32 + 'a;

// ---------------------------------------------------------------- the statements: ee_screenscpp_seetx (0..199)
/// p0's `SetAlignment(i0)`; the test's note of it (`gSeetAlignments[p0] = i0`).
pub const EE_SCREENS_SEETX_ALIGN: u32 = 0;
pub const EE_SCREENS_SEETX_ALIGN_NOTE: u32 = 1;
/// The radio button's or check box's (p0) `SetText(the text, i1 the font: EE_SCREENS_SEPL_FONT_538 or 534)`.
pub const EE_SCREENS_SEETX_RADIO_TEXT: u32 = 2;
pub const EE_SCREENS_SEETX_CHECK_TEXT: u32 = 3;
/// The check box's, radio button's (their text label's) or list box's `GetGEText()`.
pub const EE_SCREENS_SEETX_CHECK_GETEXT: u32 = 4;
pub const EE_SCREENS_SEETX_RADIO_GETEXT: u32 = 5;
pub const EE_SCREENS_SEETX_LIST_GETEXT: u32 = 6;
/// The GEText p0's `SetFgColor(the editor's yellow)`, `SetIsTransparent(true)`.
pub const EE_SCREENS_SEETX_YELLOW: u32 = 7;
pub const EE_SCREENS_SEETX_TRANSPARENT: u32 = 8;
/// The edit box's, the combo box's, the combo box's list's GEText `SetFont(gU531930)`.
pub const EE_SCREENS_SEETX_EDIT_FONT_530: u32 = 9;
pub const EE_SCREENS_SEETX_COMBO_FONT_530: u32 = 10;
pub const EE_SCREENS_SEETX_COMBO_LIST_FONT_530: u32 = 11;
/// The kept names i0 (0: the NAMES op's, 1: the sequences'): cleared, `GetNames(i1)` into them, their count, name
/// i1 (a text), p0's `AppendStringArray` of them.
pub const EE_SCREENS_SEETX_NAMES_CLEAR: u32 = 12;
pub const EE_SCREENS_SEETX_NAMES_GET: u32 = 13;
pub const EE_SCREENS_SEETX_NAMES_COUNT: u32 = 14;
pub const EE_SCREENS_SEETX_NAMES_AT: u32 = 15;
pub const EE_SCREENS_SEETX_COMBO_APPEND_NAMES: u32 = 16;
/// The list box's `SetBgOpacity(f0)`, `SetScrollBarBgOpacity(f0)`.
pub const EE_SCREENS_SEETX_LIST_BG_OPACITY: u32 = 17;
pub const EE_SCREENS_SEETX_LIST_SCROLL_OPACITY: u32 = 18;
/// The events (made): `EECreateScreenEvent(i0)`, `new EESEERust(i0)`, `new UIESliderChange(label p0, f0, i0)`,
/// `EESEENewSliderChange(p0, i0 ? "%#2.1f" : "%2.0f", f0, f1)`, `EESEENewUseObjectArea(p0, combo p1, the radio
/// i0 | i1 << 32)`, `EESEENewObjects(i0, types p1, controls p0)`.
pub const EE_SCREENS_SEETX_EV_SCREEN: u32 = 19;
pub const EE_SCREENS_SEETX_EV_SEE: u32 = 20;
pub const EE_SCREENS_SEETX_EV_SLIDER: u32 = 21;
pub const EE_SCREENS_SEETX_EV_SLIDER_FORMAT: u32 = 22;
pub const EE_SCREENS_SEETX_EV_USE_OBJECT_AREA: u32 = 23;
pub const EE_SCREENS_SEETX_EV_OBJECTS: u32 = 24;
/// The slider event p1's `AddString(f0, the text p0)`.
pub const EE_SCREENS_SEETX_EVENT_ADD_STRING: u32 = 25;
/// `sprintf(the kept text, the format p0 (i0 bytes), (unsigned long)i1)`; `sprintf(the kept text, the C format p0,
/// (double)f0)`; the kept text.
pub const EE_SCREENS_SEETX_FORMAT_NUMBER: u32 = 26;
pub const EE_SCREENS_SEETX_FORMAT_FLOAT: u32 = 27;
pub const EE_SCREENS_SEETX_KEPT_TEXT: u32 = 28;
/// The scenario's object i0 (found), its `mName`; trigger i0, its `mName`.
pub const EE_SCREENS_SEETX_OBJECT: u32 = 29;
pub const EE_SCREENS_SEETX_OBJECT_NAME: u32 = 30;
pub const EE_SCREENS_SEETX_TRIGGER: u32 = 31;
pub const EE_SCREENS_SEETX_TRIGGER_NAME: u32 = 32;
/// Calamity class i0 (found), its `mID`, `mNameID`; graphic effect i0 (found), its `mIsScenarioListed == true`,
/// `mID`, `mNameStringId`.
pub const EE_SCREENS_SEETX_CALAMITY: u32 = 33;
pub const EE_SCREENS_SEETX_CALAMITY_ID: u32 = 34;
pub const EE_SCREENS_SEETX_CALAMITY_NAME: u32 = 35;
pub const EE_SCREENS_SEETX_GRAPHIC: u32 = 36;
pub const EE_SCREENS_SEETX_GRAPHIC_LISTED: u32 = 37;
pub const EE_SCREENS_SEETX_GRAPHIC_ID: u32 = 38;
pub const EE_SCREENS_SEETX_GRAPHIC_NAME: u32 = 39;
/// `gCPBehaviors[i0]`'s field `EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_*`.
pub const EE_SCREENS_SEETX_BEHAVIOR: u32 = 40;
/// `gUnitClassTable[GetUnitTypes()[i0]]` (found); a class p0's `mIsScenarioSelectable == true`, `mUnitType`,
/// `mDomains`.
pub const EE_SCREENS_SEETX_UNIT_TYPE_CLASS: u32 = 48;
pub const EE_SCREENS_SEETX_CLASS_SELECTABLE: u32 = 49;
pub const EE_SCREENS_SEETX_CLASS_TYPE: u32 = 50;
pub const EE_SCREENS_SEETX_CLASS_DOMAINS: u32 = 51;
/// The kept technologies (`sSeetTechs`): cleared, `gTechTree.GetNodeIDs` into them, `EEEpochCount()`, i0 pushed,
/// their count, technology i0, node i0's `mShowInEditor == true`, its `mMakesUnit == false`, entry i0 erased,
/// sorted by name (`SeetTechNameLess`).
pub const EE_SCREENS_SEETX_TECHS_CLEAR: u32 = 52;
pub const EE_SCREENS_SEETX_TECHS_NODE_IDS: u32 = 53;
pub const EE_SCREENS_SEETX_EPOCH_COUNT: u32 = 54;
pub const EE_SCREENS_SEETX_TECHS_PUSH: u32 = 55;
pub const EE_SCREENS_SEETX_TECHS_COUNT: u32 = 56;
pub const EE_SCREENS_SEETX_TECHS_AT: u32 = 57;
pub const EE_SCREENS_SEETX_TECH_SHOWN: u32 = 58;
pub const EE_SCREENS_SEETX_TECH_NO_UNIT: u32 = 59;
pub const EE_SCREENS_SEETX_TECHS_ERASE: u32 = 60;
pub const EE_SCREENS_SEETX_TECHS_SORT: u32 = 61;
/// `gTechTree.Get(i0)` (found); a node p0's `mData->mUnitClass`, `mNameID`, `mID`; `gUnitClassTable[i0] != NULL`,
/// its `mNameStringID`.
pub const EE_SCREENS_SEETX_TECH_NODE: u32 = 62;
pub const EE_SCREENS_SEETX_NODE_UNIT_CLASS: u32 = 63;
pub const EE_SCREENS_SEETX_NODE_NAME_ID: u32 = 64;
pub const EE_SCREENS_SEETX_NODE_ID: u32 = 65;
pub const EE_SCREENS_SEETX_CLASS_EXISTS: u32 = 66;
pub const EE_SCREENS_SEETX_CLASS_NAME_ID: u32 = 67;
/// The editor form's `TriggerCall(i0)`, its `GetTriggerBox(i0 != 0)` (found).
pub const EE_SCREENS_SEETX_TRIGGER_CALL: u32 = 68;
pub const EE_SCREENS_SEETX_TRIGGER_BOX: u32 = 69;
/// The scenario's effect i0 (found) and an effect p0's `EeScenarioEffectFields` (found); the condition's.
pub const EE_SCREENS_SEETX_EFFECT_ENTRY: u32 = 70;
pub const EE_SCREENS_SEETX_EFFECT_FIELDS: u32 = 71;
pub const EE_SCREENS_SEETX_CONDITION_ENTRY: u32 = 72;
pub const EE_SCREENS_SEETX_CONDITION_FIELDS: u32 = 73;
/// The effect p0's text (`EE_SCREENS_SEETX_EFFECT_TEXT + k`, k 0..5: mNewName, mMessage, mFileName, mDescription,
/// mText, mName) and its setter (`_SET_EFFECT_TEXT + k`); the condition's (k 0..2: mChatText, mText, mName).
pub const EE_SCREENS_SEETX_EFFECT_TEXT: u32 = 74;
pub const EE_SCREENS_SEETX_SET_EFFECT_TEXT: u32 = 80;
pub const EE_SCREENS_SEETX_CONDITION_TEXT: u32 = 86;
pub const EE_SCREENS_SEETX_SET_CONDITION_TEXT: u32 = 89;
/// The effect p0's `mObjects.assign(p1, p1 + i0)`.
pub const EE_SCREENS_SEETX_SET_OBJECTS: u32 = 92;
/// The scenario's list i0: `SortList`, `MoveEntry(i1, i2 != 0)`, `DeleteEntry(i1)`, `DuplicateEntry(i1)`,
/// `NewEntry`.
pub const EE_SCREENS_SEETX_SM_SORT: u32 = 93;
pub const EE_SCREENS_SEETX_SM_MOVE: u32 = 94;
pub const EE_SCREENS_SEETX_SM_DELETE: u32 = 95;
pub const EE_SCREENS_SEETX_SM_DUPLICATE: u32 = 96;
pub const EE_SCREENS_SEETX_SM_NEW: u32 = 97;
/// `EEUIShell::Op(p0, i0, (uint32_t)i1)`.
pub const EE_SCREENS_SEETX_SHELL_OP: u32 = 98;
/// `gUnitClassTable[i0]`'s `mCategory`, its `mIsScenarioSelectable == true`; `EEUnitClassCount()`.
pub const EE_SCREENS_SEETX_CLASS_CATEGORY: u32 = 99;
pub const EE_SCREENS_SEETX_CLASS_SELECTABLE_AT: u32 = 100;
pub const EE_SCREENS_SEETX_CLASS_COUNT: u32 = 101;

// ---------------------------------------------------------------- the statements: ee_screenscpp_seobx (200..)
/// `new EESETSAMouseClick`, `new EESETSALasso`, `new EESEUICTriggerSelectUnits(form p0, kSETSObjectSelectUnits,
/// i0, click p1, lasso: the word p2 points at on the way in)`, form p0's `AddControl(p1, i0)`.
pub const EE_SCREENS_SEOBX_NEW_CLICK: u32 = 200;
pub const EE_SCREENS_SEOBX_NEW_LASSO: u32 = 201;
pub const EE_SCREENS_SEOBX_NEW_SELECT: u32 = 202;
pub const EE_SCREENS_SEOBX_ADD_CONTROL: u32 = 203;
/// The lasso's and the click's `Dereference()`.
pub const EE_SCREENS_SEOBX_LASSO_RELEASE: u32 = 204;
pub const EE_SCREENS_SEOBX_CLICK_RELEASE: u32 = 205;
/// The kept unit types: cleared, i0 pushed, sorted (`EESETSOUnitTypeNameLess`), their count, type i0.
pub const EE_SCREENS_SEOBX_TYPES_CLEAR: u32 = 206;
pub const EE_SCREENS_SEOBX_TYPES_PUSH: u32 = 207;
pub const EE_SCREENS_SEOBX_TYPES_SORT: u32 = 208;
pub const EE_SCREENS_SEOBX_TYPES_COUNT: u32 = 209;
pub const EE_SCREENS_SEOBX_TYPES_AT: u32 = 210;
/// The object p0's flag i0 (`SeobFlag`), its number field i0 (`SeobField`), the flag set to i1 != 0, the parent
/// (`SetParent(i1)`), the number field set to `(unsigned long)(long)i1`.
pub const EE_SCREENS_SEOBX_FLAG: u32 = 211;
pub const EE_SCREENS_SEOBX_FIELD: u32 = 212;
pub const EE_SCREENS_SEOBX_SET_FLAG: u32 = 213;
pub const EE_SCREENS_SEOBX_SET_PARENT: u32 = 214;
pub const EE_SCREENS_SEOBX_SET_FIELD: u32 = 215;
/// The object p0's `SetName(the text)`, `SetUnits(p1's i0 ids)`.
pub const EE_SCREENS_SEOBX_SET_NAME: u32 = 216;
pub const EE_SCREENS_SEOBX_SET_UNITS: u32 = 217;
/// The object p0's `GetUnits` into the kept units, their count, unit i0; `gWorld.GetUnit(i0)` (found, an `EEUnit*`);
/// the editor of control p0's `SelectUnit(p1)`.
pub const EE_SCREENS_SEOBX_UNITS_GET: u32 = 218;
pub const EE_SCREENS_SEOBX_UNITS_COUNT: u32 = 219;
pub const EE_SCREENS_SEOBX_UNITS_AT: u32 = 220;
pub const EE_SCREENS_SEOBX_WORLD_UNIT: u32 = 221;
pub const EE_SCREENS_SEOBX_SELECT_UNIT: u32 = 222;
/// The editor p0's `GetSelectedUnitCount()`, its selected unit i0's class's `mNameStringID`.
pub const EE_SCREENS_SEOBX_SELECTED_COUNT: u32 = 223;
pub const EE_SCREENS_SEOBX_SELECTED_NAME_ID: u32 = 224;
/// `gViewport.GetViewRectangle().Contains(f0, f1)`; the left and right mouse buttons' `JustPressed()`.
pub const EE_SCREENS_SEOBX_IN_VIEW: u32 = 225;
pub const EE_SCREENS_SEOBX_LBUTTON_JUST: u32 = 226;
pub const EE_SCREENS_SEOBX_RBUTTON_JUST: u32 = 227;

// ---------------------------------------------------------------- the statements: ee_screenscpp_setrx (300..)
/// The trigger p0's `SetName(the text)`, `SetText(the text)`, `mName`, `mText`.
pub const EE_SCREENS_SETRX_SET_NAME: u32 = 300;
pub const EE_SCREENS_SETRX_SET_TEXT: u32 = 301;
pub const EE_SCREENS_SETRX_NAME: u32 = 302;
pub const EE_SCREENS_SETRX_TEXT: u32 = 303;
/// The trigger p0's condition i0 (found), its `mIsNot`, `mValue`, `mOperator`; its effect i0 (found), its
/// `mDelay`, `mEffectIndex`, `mOperator`.
pub const EE_SCREENS_SETRX_CONDITION: u32 = 304;
pub const EE_SCREENS_SETRX_CONDITION_NOT: u32 = 305;
pub const EE_SCREENS_SETRX_CONDITION_VALUE: u32 = 306;
pub const EE_SCREENS_SETRX_CONDITION_OPERATOR: u32 = 307;
pub const EE_SCREENS_SETRX_EFFECT: u32 = 308;
pub const EE_SCREENS_SETRX_EFFECT_DELAY: u32 = 309;
pub const EE_SCREENS_SETRX_EFFECT_INDEX: u32 = 310;
pub const EE_SCREENS_SETRX_EFFECT_OPERATOR: u32 = 311;
/// The trigger p0's `SetConditionOperator(i0, i1)`, `SetEffectOperator(i0, i1)`, `DeleteCondition(i0)`,
/// `DeleteEffect(i0)`.
pub const EE_SCREENS_SETRX_SET_CONDITION_OPERATOR: u32 = 312;
pub const EE_SCREENS_SETRX_SET_EFFECT_OPERATOR: u32 = 313;
pub const EE_SCREENS_SETRX_DELETE_CONDITION: u32 = 314;
pub const EE_SCREENS_SETRX_DELETE_EFFECT: u32 = 315;
/// `sprintf(the kept text, "%s %s", the text p0 (i0 bytes), the label p1's line 0)`; the button p0's `SetText(the
/// kept text, gU531934)`; the combo box p0's text label's `SetText(the text)` (the label's own, not the toolkit's
/// fit).
pub const EE_SCREENS_SETRX_FORMAT_REMOVE: u32 = 316;
pub const EE_SCREENS_SETRX_BUTTON_KEPT_TEXT: u32 = 317;
/// The scenario's condition i0 is an object condition (`GetType() == kSMConditionObject`), the conditions' count.
pub const EE_SCREENS_SETRX_CONDITION_IS_OBJECT: u32 = 318;
pub const EE_SCREENS_SETRX_CONDITION_COUNT: u32 = 319;
/// The editor form's trigger tab (found); the radio button p0's `Check()`.
pub const EE_SCREENS_SETRX_TRIGGER_TAB: u32 = 320;
pub const EE_SCREENS_SETRX_RADIO_CHECK: u32 = 321;
/// The combo box p0's text label's `SetText(the text)`.
pub const EE_SCREENS_SETRX_COMBO_LABEL_TEXT: u32 = 322;
/// The trigger p0's value `EE_SCREENS_SETRX_GET + EE_SCREENS_SETR_V_*` (mIsOn, mConditionDuration, mIsLooping,
/// mIsInCinematic, the condition and effect counts) and its setter `_SET + v` to i0 (`SetIsOn(i0 != 0)`,
/// `SetConditionDuration((unsigned long)(long)i0)`, `SetIsLooping`, `SetIsInCinematic`); `SetCondition(i0, i1 != 0,
/// i2)`, `SetEffect(i0, i1, i2)`.
pub const EE_SCREENS_SETRX_GET: u32 = 330;
pub const EE_SCREENS_SETRX_SET: u32 = 336;
pub const EE_SCREENS_SETRX_SET_CONDITION: u32 = 340;
pub const EE_SCREENS_SETRX_SET_EFFECT: u32 = 341;

// ---------------------------------------------------------------- the statements: ee_screenscpp_setsx (400..)
/// `new UIRadioButton(2000 + i0, the text p1, gU531930, (f0, f1, f2, f3), form p0, i1, 2, true, i0 == 0, false)`
/// (made); a radio p0's four bitmaps set to NULL (up checked, down checked, down unchecked, up unchecked), its text
/// label's `SetRectangle(f)`, its `SetBgOpacity(f0)`, its text label's GEText (found).
pub const EE_SCREENS_SETSX_NEW_TAB: u32 = 400;
pub const EE_SCREENS_SETSX_UP_CHECKED_NULL: u32 = 401;
pub const EE_SCREENS_SETSX_DOWN_CHECKED_NULL: u32 = 402;
pub const EE_SCREENS_SETSX_DOWN_UNCHECKED_NULL: u32 = 403;
pub const EE_SCREENS_SETSX_UP_UNCHECKED_NULL: u32 = 404;
pub const EE_SCREENS_SETSX_LABEL_RECT: u32 = 405;
pub const EE_SCREENS_SETSX_BG_OPACITY: u32 = 406;
pub const EE_SCREENS_SETSX_RADIO_GETEXT: u32 = 407;
/// Form p0's manager's bitmaps (found, `EE_SCREENS_SETSX_BITMAP + n`): the alpha sort's up and down, the scroll
/// bar's up normal and pressed, its down normal and pressed.
pub const EE_SCREENS_SETSX_BITMAP: u32 = 408;
/// `new UIButton(i0, up p1, NULL, down: the word p2 points at on the way in, (305, f0) and the up bitmap's size,
/// form p0, i1, 0)` (made).
pub const EE_SCREENS_SETSX_NEW_IMAGE_BUTTON: u32 = 416;
/// `new EEUIShellCheck(EE_SCREENS_SK_SHOW_IDS, {}, 0x7de, gEmptyString, gU531930, (305, f0) and the size of form
/// p0's manager's check-on bitmap, form p0, i0, 0, false)` (made).
pub const EE_SCREENS_SETSX_NEW_SHOW_IDS: u32 = 419;
/// `EECreateScreenEvent(i0)` (made), control p0's `SetTriggerEvent(i0, event p1)`, the event p0's
/// `Dereference()`; form p0's `AddControl(p1, 14)`.
pub const EE_SCREENS_SETSX_SCREEN_EVENT: u32 = 420;
pub const EE_SCREENS_SETSX_SET_TRIGGER: u32 = 421;
pub const EE_SCREENS_SETSX_EVENT_RELEASE: u32 = 422;
pub const EE_SCREENS_SETSX_ADD_CONTROL: u32 = 423;
/// The kept names cleared; the sub-screen object p1's names into them (`EE_SCREENS_SETSX_SUB_NAMES + n`: the
/// trigger, condition, effect, object, area screens' `GetNames`, Rust's); the list p0's `AppendStringArray` of them.
pub const EE_SCREENS_SETSX_NAMES_CLEAR: u32 = 424;
pub const EE_SCREENS_SETSX_LIST_APPEND_NAMES: u32 = 425;
pub const EE_SCREENS_SETSX_SUB_NAMES: u32 = 444;
/// Control p0's editor (found); the editor p0's control i0 (found); control p0's `SetDisplayMode(i0)`; the editor
/// p0's `SetGameView(f, false)`; `EESetGameViewRectangle(f)`; the hide-UI event's views (`ee_screens_see_hide_ui_views`
/// of control p0's checked event: unchecked f, checked the four floats at p1); the check box p0's
/// `SetChecked(true)`; the editor p0's entity group i0's `Enable()`.
pub const EE_SCREENS_SETSX_EDITOR: u32 = 426;
pub const EE_SCREENS_SETSX_CONTROL_BY_ID: u32 = 427;
pub const EE_SCREENS_SETSX_SET_DISPLAY: u32 = 428;
pub const EE_SCREENS_SETSX_SET_GAME_VIEW: u32 = 429;
pub const EE_SCREENS_SETSX_GAME_VIEW_RECT: u32 = 430;
pub const EE_SCREENS_SETSX_HIDE_UI_VIEWS: u32 = 431;
/// The hide-UI box p0's checked event's Rust event (`EESEERust::mEvent`; found): `HIDE_UI_VIEWS` (answered by the
/// adapter since round 6) sets its views through it.
pub const EE_SCREENS_SETSX_CHECKED_EVENT: u32 = 480;
pub const EE_SCREENS_SETSX_SET_CHECKED: u32 = 432;
pub const EE_SCREENS_SETSX_GROUP_ENABLE: u32 = 433;
/// `gB529066 = false`, `gCameraFollowing = false`, `gB529068 = false`; `gMiniMap != NULL`, its `RefreshView()`;
/// `gUIManager->RedrawAll()`, the current form's `SetAllControlsDirty()`.
pub const EE_SCREENS_SETSX_CLEAR_529066: u32 = 434;
pub const EE_SCREENS_SETSX_CLEAR_FOLLOWING: u32 = 435;
pub const EE_SCREENS_SETSX_CLEAR_529068: u32 = 436;
pub const EE_SCREENS_SETSX_MINIMAP: u32 = 437;
pub const EE_SCREENS_SETSX_MINIMAP_REFRESH: u32 = 438;
pub const EE_SCREENS_SETSX_REDRAW_ALL: u32 = 439;
pub const EE_SCREENS_SETSX_ALL_DIRTY: u32 = 440;
/// The editor form's `GetWasUIHidden()`; the rectangle p0 `And` the panel (0, 448, 1023, 606) into the kept clip
/// (whether they meet); the editor's `UIForm::DrawBackground(its panel background, the clip, 256 x 256, the
/// panel)`.
pub const EE_SCREENS_SETSX_WAS_UI_HIDDEN: u32 = 441;
pub const EE_SCREENS_SETSX_CLIP_PANEL: u32 = 442;
pub const EE_SCREENS_SETSX_DRAW_PANEL: u32 = 443;

// ---------------------------------------------------------------- the statements: ee_screenscpp_searx (500..)
/// The scenario's area i0 (found); an area p0's `GetName()`, `GetText()`, its rectangle's left, top, right, bottom
/// (`EE_SCREENS_SEARX_RECT + n`, into f0), anchor x, y, flags
/// (1 has a rectangle, 2 an anchor), select mode; `mName = the text`, `mText = the text`, the combo p1's
/// `GetCurrentSelection(p0's mSelectMode)`, `SetRectangle(f)`, `SetAnchor(i0, i1)`.
pub const EE_SCREENS_SEARX_AREA: u32 = 500;
pub const EE_SCREENS_SEARX_NAME: u32 = 501;
pub const EE_SCREENS_SEARX_TEXT: u32 = 502;
pub const EE_SCREENS_SEARX_ANCHOR_X: u32 = 504;
pub const EE_SCREENS_SEARX_ANCHOR_Y: u32 = 505;
pub const EE_SCREENS_SEARX_HAS: u32 = 506;
pub const EE_SCREENS_SEARX_SELECT_MODE: u32 = 507;
pub const EE_SCREENS_SEARX_SET_NAME: u32 = 508;
pub const EE_SCREENS_SEARX_SET_TEXT: u32 = 509;
pub const EE_SCREENS_SEARX_GET_SELECTION: u32 = 510;
pub const EE_SCREENS_SEARX_SET_RECT: u32 = 511;
pub const EE_SCREENS_SEARX_SET_ANCHOR: u32 = 512;
pub const EE_SCREENS_SEARX_RECT: u32 = 530;
/// The point (f0, f1, or the mouse's for i0 != 0) kept; the kept x and y zeroed (the C++'s `theX = 0, theY = 0`),
/// the kept location reset; `PickScreenTile(the point, x, y, true) != NULL`; `GetTileAt(the point, x, y)` (found);
/// `GetScreenLine(the point)` into the kept line; the line solved at the tile p0's height into the location; x and
/// y set to the location's, truncated; the kept x, y.
pub const EE_SCREENS_SEARX_POINT: u32 = 513;
pub const EE_SCREENS_SEARX_XY_RESET: u32 = 534;
pub const EE_SCREENS_SEARX_LOCATION_RESET: u32 = 535;
pub const EE_SCREENS_SEARX_PICK: u32 = 514;
pub const EE_SCREENS_SEARX_TILE_AT: u32 = 515;
pub const EE_SCREENS_SEARX_SCREEN_LINE: u32 = 516;
pub const EE_SCREENS_SEARX_SOLVE: u32 = 517;
pub const EE_SCREENS_SEARX_GROUND_X: u32 = 518;
pub const EE_SCREENS_SEARX_GROUND_Y: u32 = 519;
pub const EE_SCREENS_SEARX_X: u32 = 536;
pub const EE_SCREENS_SEARX_Y: u32 = 537;
/// The left mouse button's `GetValue() > 0`, `JustPressed()`, `JustUnpressed()`.
pub const EE_SCREENS_SEARX_LBUTTON_DOWN: u32 = 520;
pub const EE_SCREENS_SEARX_LBUTTON_PRESSED: u32 = 521;
pub const EE_SCREENS_SEARX_LBUTTON_UNPRESSED: u32 = 522;
/// `new EESEARust(kSEAPaintArea, NULL, i0, i1)` (made); `gMouse->SetBounds(the view)`, the edge scroll's edges
/// the view less one; the bounds and edges of the window; the action p0's area (into i); its `Dereference()`.
pub const EE_SCREENS_SEARX_NEW_PAINT: u32 = 523;
pub const EE_SCREENS_SEARX_BOUNDS_VIEW: u32 = 524;
pub const EE_SCREENS_SEARX_BOUNDS_WINDOW: u32 = 526;
pub const EE_SCREENS_SEARX_PAINT_AREA: u32 = 528;
pub const EE_SCREENS_SEARX_PAINT_RELEASE: u32 = 529;
/// The view rectangle's sides (`gViewport.GetViewRectangle()`'s left, top, right, bottom) and the game window's
/// width and height (as floats) into f0; the edge scroll's edges `= GERectangle(f0, f1, f2, f3)` (round 6: the
/// edges' arithmetic, the view or the window less one, is Rust's).
pub const EE_SCREENS_SEARX_VIEW_LEFT: u32 = 580;
pub const EE_SCREENS_SEARX_VIEW_TOP: u32 = 581;
pub const EE_SCREENS_SEARX_VIEW_RIGHT: u32 = 582;
pub const EE_SCREENS_SEARX_VIEW_BOTTOM: u32 = 583;
pub const EE_SCREENS_SEARX_WINDOW_WIDTH: u32 = 584;
pub const EE_SCREENS_SEARX_WINDOW_HEIGHT: u32 = 585;
pub const EE_SCREENS_SEARX_SET_EDGES: u32 = 586;

// ---------------------------------------------------------------- the statements: ee_screenscpp_seinx (600..)
/// Directory k (i0: 0 scenarios, 1 sounds, 2 movies; kept): `SetVolume(gStartupVolume)`, `SetDirectory(
/// gStartupDirectory + "data\\" + its name)`; `new FSPath` (made); a path p0's `AddDirectory(directory i0, true)`;
/// directory i0's `IsDirectory()`; its `ESVERIFY_AT(CreateDirectory() == keseOK, 536 + 3 * i0)`; a path p0's
/// `Dereference()`.
pub const EE_SCREENS_SEINX_DIR_VOLUME: u32 = 600;
pub const EE_SCREENS_SEINX_DIR_DIRECTORY: u32 = 601;
pub const EE_SCREENS_SEINX_NEW_PATH: u32 = 602;
pub const EE_SCREENS_SEINX_ADD_DIRECTORY: u32 = 603;
pub const EE_SCREENS_SEINX_IS_DIRECTORY: u32 = 604;
pub const EE_SCREENS_SEINX_CREATE_DIRECTORY: u32 = 605;
pub const EE_SCREENS_SEINX_PATH_RELEASE: u32 = 606;
/// `EEUIShellNewFileCombo(path p0, the pattern of kind i0, its id, gSeplScreenFont, (f0, f1), 150, form p1, i1)`
/// (made); a combo p0's GEText (found), `SetListBoxSelectedBgColor(blue)`, `SetTextBoxBgOpacity(0.4f)`; form p0's
/// `AddControl(p1, 7)`.
pub const EE_SCREENS_SEINX_NEW_FILE_COMBO: u32 = 607;
pub const EE_SCREENS_SEINX_COMBO_GETEXT: u32 = 608;
pub const EE_SCREENS_SEINX_YELLOW: u32 = 609;
pub const EE_SCREENS_SEINX_LIST_BLUE: u32 = 610;
pub const EE_SCREENS_SEINX_BOX_OPACITY: u32 = 611;
pub const EE_SCREENS_SEINX_ADD_CONTROL: u32 = 612;
/// `new EESEIEImport`, `new EESEIEExport` (made); control p0's `SetTriggerEvent(kControlTriggerButtonUp, p1)`; the
/// event p0's `Dereference()`.
pub const EE_SCREENS_SEINX_NEW_IMPORT: u32 = 613;
pub const EE_SCREENS_SEINX_NEW_EXPORT: u32 = 614;
pub const EE_SCREENS_SEINX_SET_BUTTON_UP: u32 = 615;
pub const EE_SCREENS_SEINX_EVENT_RELEASE: u32 = 616;
/// The kept text (`theText`): the media string i0 (`EE_SCREENS_SEINX_M_*`) or the export file name copied in;
/// `EEUIFileComboSelect(combo p0, it)`; its `GetLength()`; the combo p0's text label's `SetText(it)`; the edit p0's
/// `SetText(media string i0)`; the edit p0's `GetAllText` into it; media string i0 set to it; media string i0 set to
/// the combo p0's text label's line 0.
pub const EE_SCREENS_SEINX_TEXT_MEDIA: u32 = 617;
pub const EE_SCREENS_SEINX_TEXT_EXPORT: u32 = 618;
pub const EE_SCREENS_SEINX_FILE_SELECT: u32 = 619;
pub const EE_SCREENS_SEINX_TEXT_LENGTH: u32 = 620;
pub const EE_SCREENS_SEINX_LABEL_TEXT: u32 = 621;
pub const EE_SCREENS_SEINX_EDIT_MEDIA: u32 = 622;
pub const EE_SCREENS_SEINX_EDIT_ALL_TEXT: u32 = 623;
pub const EE_SCREENS_SEINX_SET_MEDIA: u32 = 624;
pub const EE_SCREENS_SEINX_SET_MEDIA_LABEL: u32 = 625;
/// The kept file: `SetVolume(gStartupVolume)`, `SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\")`,
/// `SetFile(the kept text)`, `FOpen("r") == 0`, `Close()`; the edit p0's `SetText(the kept name)`; its
/// `SetText(gEmptyString)`.
pub const EE_SCREENS_SEINX_FILE_VOLUME: u32 = 626;
pub const EE_SCREENS_SEINX_FILE_DIRECTORY: u32 = 627;
pub const EE_SCREENS_SEINX_FILE_NAME: u32 = 628;
pub const EE_SCREENS_SEINX_FILE_OPEN: u32 = 629;
pub const EE_SCREENS_SEINX_FILE_CLOSE: u32 = 630;
pub const EE_SCREENS_SEINX_EDIT_STEM: u32 = 631;
pub const EE_SCREENS_SEINX_EDIT_EMPTY: u32 = 632;
/// The kept name set to the kept text less its last four characters (`CopyString`); the kept directory i0 and the
/// kept file reset (the C++'s fresh locals).
pub const EE_SCREENS_SEINX_NAME_STEM: u32 = 633;
pub const EE_SCREENS_SEINX_DIR_RESET: u32 = 634;
pub const EE_SCREENS_SEINX_FILE_RESET: u32 = 635;
/// The media strings (`EE_SCREENS_SEINX_TEXT_MEDIA`'s i0).
pub const EE_SCREENS_SEINX_M_MAP: i32 = 0;
pub const EE_SCREENS_SEINX_M_MOVIE: i32 = 1;
pub const EE_SCREENS_SEINX_M_SOUND: i32 = 2;
pub const EE_SCREENS_SEINX_M_INSTRUCTIONS: i32 = 3;
pub const EE_SCREENS_SEINX_M_AFTERMATH: i32 = 4;
pub const EE_SCREENS_SEINX_M_HINTS: i32 = 5;
pub const EE_SCREENS_SEINX_M_HISTORY: i32 = 6;

// ---------------------------------------------------------------- the toolkit's calls, answered in Rust (900..)
/// The label's `SetText(the text)`; the edit box's `SetText(the text)`, its line 0, its whole text; the combo box's
/// text label's line 0, its `AppendLine(the text)`, `DeleteAllEntries()`, `SetCurrentSelection(i0)`; the list
/// box's `DeleteAllEntries()`, `AppendLine(the text)`.
pub const EE_SCREENS_TRGX_R_LABEL_SET_TEXT: u32 = 900;
pub const EE_SCREENS_TRGX_R_EDIT_SET_TEXT: u32 = 901;
pub const EE_SCREENS_TRGX_R_EDIT_LINE: u32 = 902;
pub const EE_SCREENS_TRGX_R_EDIT_ALL_TEXT: u32 = 903;
pub const EE_SCREENS_TRGX_R_COMBO_TEXT: u32 = 904;
pub const EE_SCREENS_TRGX_R_COMBO_APPEND: u32 = 905;
pub const EE_SCREENS_TRGX_R_COMBO_CLEAR: u32 = 906;
pub const EE_SCREENS_TRGX_R_COMBO_SELECT: u32 = 907;
pub const EE_SCREENS_TRGX_R_LIST_CLEAR: u32 = 908;
pub const EE_SCREENS_TRGX_R_LIST_APPEND: u32 = 909;
/// The control's `GetTriggerEvent(i0)` (found), its `SetTriggerEvent(i0, p1)`; the event p1's `Dereference()`
/// (i0 1) or `Reference()` (i0 2) through the control p0.
pub const EE_SCREENS_TRGX_R_TRIGGER_EVENT: u32 = 910;
pub const EE_SCREENS_TRGX_R_SET_TRIGGER: u32 = 911;
pub const EE_SCREENS_TRGX_R_EVENT_CALL: u32 = 912;
/// The language string i0 (`gResource->GetString`: -1 when there is none, the caller's text left alone).
pub const EE_SCREENS_TRGX_R_STRING: u32 = 913;
/// The controls record p0's member i1, i2 (i0 != 0: the condition screen's) (found; `editor_direct::slot`).
pub const EE_SCREENS_TRGX_R_SLOT: u32 = 914;
/// The trigger tab p0's control i0, i1 (`ee_screens_sets_control`, found), its `ee_screens_sets_sub_select(i0, i1)`,
/// its `ee_screens_sets_call(i0, i1)`.
pub const EE_SCREENS_TRGX_R_SETS_CONTROL: u32 = 915;
pub const EE_SCREENS_TRGX_R_SETS_SUB_SELECT: u32 = 916;
pub const EE_SCREENS_TRGX_R_SETS_CALL: u32 = 917;

// ---- numbers of the C++
/// `kSETriggerScreenObject`, `kSETriggerScreenArea`.
const SCREEN_OBJECT: i32 = 3;
const SCREEN_AREA: i32 = 4;
/// `kControlTriggerChecked`, `kControlTriggerSelectionChanged`.
const TRIGGER_CHECKED: i32 = 0x3e9;
const TRIGGER_SELECTION: i32 = 0x3eb;
/// `kControlTriggerButtonUp`.
const TRIGGER_BUTTON_UP: i32 = 0x3f0;
/// `IDS_TRIGGER_SCREEN_TAB_OBJECT`, `IDS_AREA`, `IDS_DEL`, `IDS_TRIGGER_EDITOR_END`, `IDS_TRIGGER_SCREEN_TAB_FIRST`.
const IDS_TRIGGER_SCREEN_TAB_OBJECT: i32 = 30302;
const IDS_AREA: i32 = 30303;
const IDS_DEL: i32 = 30398;
const IDS_TRIGGER_EDITOR_END: i32 = 30397;
const IDS_TRIGGER_SCREEN_TAB_FIRST: i32 = 30299;
/// `kMovementAir`, `kMovementSea`.
const MOVEMENT_AIR: i32 = 1;
const MOVEMENT_SEA: i32 = 4;
/// `kUnitCategoryProjectile`, `kUnitCategoryMissile`.
const CATEGORY_PROJECTILE: i32 = 9;
const CATEGORY_MISSILE: i32 = 0xc;
/// `kSETriggerTabTrigger` (the first sub-tab radio's id).
const TAB_TRIGGER: i32 = 0x7d0;
/// `kScenarioEditorHideUICheckBox`.
const HIDE_UI_CHECK_BOX: i32 = 0x12;
/// `kDisplayModeShown`, `kDisplayModeHidden`.
const DISPLAY_SHOWN: i32 = 1;
const DISPLAY_HIDDEN: i32 = 0;
/// The trigger tab's calls and controls (`EE_SCREENS_SETS_*` of editor_tscreen_ffi.rs): its sub-tab radios, the
/// sub-tab shown, the texts' and the alpha sort's shows and hides.
const SETS_TAB: i32 = 4;
const SETS_SHOW_TAB: i32 = 8;
const SETS_HIDE_TEXT: i32 = 11;
const SETS_SHOW_TEXT: i32 = 12;
const SETS_HIDE_ALPHA_SORT: i32 = 13;
const SETS_SHOW_ALPHA_SORT: i32 = 14;

// -------------------------------------------------------------------------------------------------- the helpers

/// A statement over a copy of its integers.
fn st(x: &mut Trgx, op: u32, p: [usize; 3], a: [i32; 4]) -> i32 {
    let mut a = a;
    x(op, p, &mut a, &mut [0.0; 4])
}

/// A statement's float answer (f0).
fn fl(x: &mut Trgx, op: u32) -> f32 {
    let mut f = [0.0f32; 4];
    x(op, [0; 3], &mut [0; 4], &mut f);
    f[0]
}

/// A statement over floats.
fn stf(x: &mut Trgx, op: u32, p: [usize; 3], a: [i32; 4], f: [f32; 4]) -> i32 {
    let mut a = a;
    let mut f = f;
    x(op, p, &mut a, &mut f)
}

/// A finding or making statement: the object it writes to the word p2 points at.
fn find(x: &mut Trgx, op: u32, p0: usize, p1: usize, a: [i32; 4]) -> usize {
    find_f(x, op, p0, p1, a, [0.0; 4])
}

fn find_f(x: &mut Trgx, op: u32, p0: usize, p1: usize, a: [i32; 4], f: [f32; 4]) -> usize {
    find_in(x, op, p0, p1, 0, a, f)
}

/// As [`find`], the word p2 points at holding `seed` on the way in.
fn find_in(x: &mut Trgx, op: u32, p0: usize, p1: usize, seed: usize, a: [i32; 4], f: [f32; 4]) -> usize {
    let mut out = seed;
    stf(x, op, [p0, p1, &mut out as *mut usize as usize], a, f);
    out
}

/// A reading statement's text (into a buffer, again larger when it did not fit).
fn text(x: &mut Trgx, op: u32, p0: usize, p1: usize, a: [i32; 3]) -> Vec<u8> {
    let mut buf = vec![0u8; 256];
    loop {
        let n = st(x, op, [p0, p1, buf.as_mut_ptr() as usize], [a[0], a[1], a[2], buf.len() as i32]);
        if n < 0 {
            return Vec::new();
        }
        if (n as usize) <= buf.len() {
            buf.truncate(n as usize);
            return buf;
        }
        buf = vec![0u8; n as usize];
    }
}

/// A statement taking a text: p0 the object, p1 the text, i0 its length, then the integers `a`.
fn put(x: &mut Trgx, op: u32, p0: usize, t: &[u8], a: [i32; 3]) -> i32 {
    st(x, op, [p0, t.as_ptr() as usize, 0], [t.len() as i32, a[0], a[1], a[2]])
}

/// The language string `id` (`None`: none, the C++ leaves its text alone).
fn string(x: &mut Trgx, id: i32) -> Option<Vec<u8>> {
    let mut buf = vec![0u8; 4500];
    let n = st(x, EE_SCREENS_TRGX_R_STRING, [0, 0, buf.as_mut_ptr() as usize], [id, 0, 0, buf.len() as i32]);
    if n < 0 {
        return None;
    }
    buf.truncate((n as usize).min(buf.len()));
    Some(buf)
}

/// The language string `id` into a fresh text (empty when there is none).
fn string_new(x: &mut Trgx, id: i32) -> Vec<u8> {
    string(x, id).unwrap_or_default()
}

/// `text` as the C++ `sprintf` text pointer took it (up to its first NUL).
fn c_str(t: &[u8]) -> &[u8] {
    &t[..t.iter().position(|&b| b == 0).unwrap_or(t.len())]
}

/// The kept names list `list` after `GetNames(kind)`, read back.
fn names(x: &mut Trgx, list: i32, kind: u32) -> Vec<Vec<u8>> {
    st(x, EE_SCREENS_SEETX_NAMES_CLEAR, [0; 3], [list, 0, 0, 0]);
    st(x, EE_SCREENS_SEETX_NAMES_GET, [0; 3], [list, kind as i32, 0, 0]);
    let n = st(x, EE_SCREENS_SEETX_NAMES_COUNT, [0; 3], [list, 0, 0, 0]);
    (0..n.max(0)).map(|k| text(x, EE_SCREENS_SEETX_NAMES_AT, 0, 0, [list, k, 0])).collect()
}

/// The sequences' own names list.
const NAMES_OWN: i32 = 1;
/// The NAMES op's list (the effect types read it back name by name).
const NAMES_OP: i32 = 0;

/// The editor form's name (false) or description (true) box.
fn trigger_box(x: &mut Trgx, text_box: bool) -> usize {
    find(x, EE_SCREENS_SEETX_TRIGGER_BOX, 0, 0, [text_box as i32, 0, 0, 0])
}

/// The combo box's `AppendLine` of each name.
fn combo_append_all(x: &mut Trgx, combo: usize, names: &[Vec<u8>]) {
    for n in names {
        put(x, EE_SCREENS_TRGX_R_COMBO_APPEND, combo, n, [0; 3]);
    }
}

/// The scenario's list `list`'s entry operation (`EE_SCREENS_SEOB_SM_*`; the screens' list buttons).
fn entry_list(x: &mut Trgx, list: u32, op: i32, entry: i32) {
    let l = list as i32;
    match op {
        EE_SCREENS_SEOB_SM_SORT => st(x, EE_SCREENS_SEETX_SM_SORT, [0; 3], [l, 0, 0, 0]),
        EE_SCREENS_SEOB_SM_DOWN => st(x, EE_SCREENS_SEETX_SM_MOVE, [0; 3], [l, entry, 0, 0]),
        EE_SCREENS_SEOB_SM_UP => st(x, EE_SCREENS_SEETX_SM_MOVE, [0; 3], [l, entry, 1, 0]),
        EE_SCREENS_SEOB_SM_DELETE => st(x, EE_SCREENS_SEETX_SM_DELETE, [0; 3], [l, entry, 0, 0]),
        EE_SCREENS_SEOB_SM_DUPLICATE => st(x, EE_SCREENS_SEETX_SM_DUPLICATE, [0; 3], [l, entry, 0, 0]),
        _ => st(x, EE_SCREENS_SEETX_SM_NEW, [0; 3], [l, 0, 0, 0]),
    };
}

// ------------------------------------------------------------------------------------------ the place combo boxes

/// `FillPlaceComboBox`: the place label 'Area' or 'Object' and the combo box filled with the scenario's area or
/// object names.
pub fn fill_place(x: &mut Trgx, areas: bool, label: usize, combo: usize) {
    let (title, kind) = if areas {
        (IDS_AREA, EE_SCENARIO_NAMES_AREAS)
    } else {
        (IDS_TRIGGER_SCREEN_TAB_OBJECT, EE_SCENARIO_NAMES_OBJECTS)
    };
    let title = string_new(x, title);
    let names = names(x, NAMES_OWN, kind);
    put(x, EE_SCREENS_TRGX_R_LABEL_SET_TEXT, label, &title, [0; 3]);
    st(x, EE_SCREENS_TRGX_R_COMBO_CLEAR, [combo, 0, 0], [0; 4]);
    combo_append_all(x, combo, &names);
}

/// `FillScenarioComboBox`: the combo box's mode (areas or objects), then [`fill_place`].
pub fn fill_scenario(x: &mut Trgx, combo: usize, label: usize, areas: bool) {
    let mode = if areas { SCREEN_AREA } else { SCREEN_OBJECT };
    st(x, EE_SCREENS_SEETX_SHELL_OP, [combo, 0, 0], [EE_SCREENS_SKOP_SET_MODE as i32, mode, 0, 0]);
    fill_place(x, areas, label, combo);
}

/// The control's trigger event held (referenced and taken off), as `FillAreaObjectComboBox` and `EESETriggerJump`
/// do around a silent change; [`restore`] puts it back.
fn hold(x: &mut Trgx, control: usize, trigger: i32) -> usize {
    let event = find(x, EE_SCREENS_TRGX_R_TRIGGER_EVENT, control, 0, [trigger, 0, 0, 0]);
    if event != 0 {
        st(x, EE_SCREENS_TRGX_R_EVENT_CALL, [control, event, 0], [2, 0, 0, 0]);
    }
    st(x, EE_SCREENS_TRGX_R_SET_TRIGGER, [control, 0, 0], [trigger, 0, 0, 0]);
    event
}

/// `FillAreaObjectComboBox`: [`fill_scenario`], then the first line selected without its event.
pub fn fill_area_object(x: &mut Trgx, label: usize, combo: usize, areas: bool) {
    fill_scenario(x, combo, label, areas);
    let event = hold(x, combo, TRIGGER_SELECTION);
    st(x, EE_SCREENS_TRGX_R_COMBO_SELECT, [combo, 0, 0], [0; 4]);
    if event != 0 {
        st(x, EE_SCREENS_TRGX_R_SET_TRIGGER, [combo, event, 0], [TRIGGER_SELECTION, 0, 0, 0]);
        st(x, EE_SCREENS_TRGX_R_EVENT_CALL, [combo, event, 0], [1, 0, 0, 0]);
    }
}

// -------------------------------------------------------------------------------------------- the tech names

/// `SeetTechName`: a technology's name in the editor (its unit class's name when it makes one).
pub fn tech_name(x: &mut Trgx, id: c_long) -> Vec<u8> {
    let node = find(x, EE_SCREENS_SEETX_TECH_NODE, 0, 0, [id as i32, 0, 0, 0]);
    if node == 0 {
        return Vec::new();
    }
    let class = st(x, EE_SCREENS_SEETX_NODE_UNIT_CLASS, [node, 0, 0], [0; 4]);
    let name = if class >= 0 && st(x, EE_SCREENS_SEETX_CLASS_EXISTS, [0; 3], [class, 0, 0, 0]) != 0 {
        st(x, EE_SCREENS_SEETX_CLASS_NAME_ID, [0; 3], [class, 0, 0, 0])
    } else {
        st(x, EE_SCREENS_SEETX_NODE_NAME_ID, [node, 0, 0], [0; 4])
    };
    string_new(x, name)
}

/// `SeetTechNameLess`: the names' multibyte order (`UWCompare < 0`).
pub fn tech_less(x: &mut Trgx, a: c_long, b: c_long) -> bool {
    let (a, b) = (tech_name(x, a), tech_name(x, b));
    mbs_less(&a, &b)
}

/// `UWCompare(a, b) < 0` (`PlatMbsCmp`).
pub fn mbs_less(a: &[u8], b: &[u8]) -> bool {
    let (a, b) = (ee_ui::direct::c_text(a), ee_ui::direct::c_text(b));
    // SAFETY: two NUL-terminated strings
    unsafe { ee_base::plat_core::string::PlatMbsCmp(a.as_ptr(), b.as_ptr()) < 0 }
}

/// `EESETSOUnitTypeNameLess`: type 0 first, then the classes by name (a missing class's name empty).
pub fn unit_type_less(x: &mut Trgx, a: c_ulong, b: c_ulong) -> bool {
    if a == 0 {
        return true;
    }
    let name = |x: &mut Trgx, t: c_ulong| {
        if st(x, EE_SCREENS_SEETX_CLASS_EXISTS, [0; 3], [t as i32, 0, 0, 0]) != 0 {
            let id = st(x, EE_SCREENS_SEETX_CLASS_NAME_ID, [0; 3], [t as i32, 0, 0, 0]);
            string_new(x, id)
        } else {
            Vec::new()
        }
    };
    let (a, b) = (name(x, a), name(x, b));
    mbs_less(&a, &b)
}

// ----------------------------------------------------------------- the effect and condition screens' generic host

/// The generic host's op composed (`condition`: the condition screen's host, `ee_screenscpp_sect`), or `None` for
/// the C++ host's. p0 the controls, p1 the entry or a handle, p2 a buffer or an out word; i0 the control array, i1
/// its index, i2 and i3 the arguments.
pub fn seet(x: &mut Trgx, condition: bool, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    let (a, k) = (i[0], i[1]);
    let at = |x: &mut Trgx, a: u32, k: i32| slot(x, p[0], condition, a, k);
    // a text op's answer into its buffer (p2, capacity i1)
    // SAFETY: p2 holds i1 bytes (the op's contract)
    let out = |t: &[u8]| unsafe { write(p[2], k, t) };
    Some(match op {
        EE_SCREENS_SEET_OP_ALIGN => {
            let label = at(x, EE_SCREENS_SEET_A_LABEL, k);
            st(x, EE_SCREENS_SEETX_ALIGN, [label, 0, 0], [i[2], 0, 0, 0]);
            st(x, EE_SCREENS_SEETX_ALIGN_NOTE, [label, 0, 0], [i[2], 0, 0, 0]);
            0
        }
        EE_SCREENS_SEET_OP_TEXT_FONT => {
            let t = string_new(x, i[2]);
            let (array, set) = if a == EE_SCREENS_SEET_A_RADIO as i32 {
                (EE_SCREENS_SEET_A_RADIO, EE_SCREENS_SEETX_RADIO_TEXT)
            } else {
                (EE_SCREENS_SEET_A_CHECK, EE_SCREENS_SEETX_CHECK_TEXT)
            };
            let c = at(x, array, k);
            put(x, set, c, &t, [i[3], 0, 0]);
            0
        }
        EE_SCREENS_SEET_OP_YELLOW => {
            let (array, get) = if a == EE_SCREENS_SEET_A_CHECK as i32 {
                (EE_SCREENS_SEET_A_CHECK, EE_SCREENS_SEETX_CHECK_GETEXT)
            } else if a == EE_SCREENS_SEET_A_RADIO as i32 {
                (EE_SCREENS_SEET_A_RADIO, EE_SCREENS_SEETX_RADIO_GETEXT)
            } else {
                (EE_SCREENS_SEET_A_LIST, EE_SCREENS_SEETX_LIST_GETEXT)
            };
            let c = at(x, array, k);
            let t = find(x, get, c, 0, [0; 4]);
            st(x, EE_SCREENS_SEETX_YELLOW, [t, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEETX_TRANSPARENT, [t, 0, 0], [0; 4]);
            0
        }
        EE_SCREENS_SEET_OP_FONT_530 => {
            let ua = a as u32;
            let edit = if matches!(ua, EE_SCREENS_SEET_A_EDIT | EE_SCREENS_SEET_A_EDIT2 | EE_SCREENS_SEET_A_TEXT) {
                at(x, ua, k)
            } else {
                0
            };
            if edit != 0 {
                st(x, EE_SCREENS_SEETX_EDIT_FONT_530, [edit, 0, 0], [0; 4]);
            } else {
                let combo = combo_at(x, p[0], condition, ua, k);
                st(x, EE_SCREENS_SEETX_COMBO_FONT_530, [combo, 0, 0], [0; 4]);
                if i[2] != 0 {
                    let combo = combo_at(x, p[0], condition, ua, k);
                    st(x, EE_SCREENS_SEETX_COMBO_LIST_FONT_530, [combo, 0, 0], [0; 4]);
                }
            }
            0
        }
        EE_SCREENS_SEET_OP_FILL_NAMES => {
            let combo = combo_at(x, p[0], condition, a as u32, k);
            if i[3] != 0 {
                st(x, EE_SCREENS_SEETX_NAMES_CLEAR, [0; 3], [NAMES_OWN, 0, 0, 0]);
                st(x, EE_SCREENS_SEETX_NAMES_GET, [0; 3], [NAMES_OWN, i[2], 0, 0]);
                st(x, EE_SCREENS_SEETX_COMBO_APPEND_NAMES, [combo, 0, 0], [NAMES_OWN, 0, 0, 0]);
            } else {
                let names = names(x, NAMES_OWN, i[2] as u32);
                combo_append_all(x, combo, &names);
            }
            0
        }
        EE_SCREENS_SEET_OP_FILL_AREA_OBJECT => {
            let label = at(x, EE_SCREENS_SEET_A_LABEL, i[2]);
            let combo = at(x, EE_SCREENS_SEET_A_SCEN, k);
            fill_area_object(x, label, combo, i[3] != 0);
            0
        }
        EE_SCREENS_SEET_OP_FILL_SCENARIO => {
            let combo = at(x, EE_SCREENS_SEET_A_SCEN, k);
            let label = at(x, EE_SCREENS_SEET_A_LABEL, i[2]);
            fill_scenario(x, combo, label, i[3] != 0);
            0
        }
        EE_SCREENS_SEET_OP_FILL_PLACE => {
            let label = at(x, EE_SCREENS_SEET_A_LABEL, i[2]);
            let combo = at(x, EE_SCREENS_SEET_A_COMBO, k);
            fill_place(x, i[3] != 0, label, combo);
            0
        }
        EE_SCREENS_SEET_OP_LIST_OPACITY => {
            let list = at(x, EE_SCREENS_SEET_A_LIST, k);
            stf(x, EE_SCREENS_SEETX_LIST_BG_OPACITY, [list, 0, 0], [0; 4], *f);
            let list = at(x, EE_SCREENS_SEET_A_LIST, k);
            stf(x, EE_SCREENS_SEETX_LIST_SCROLL_OPACITY, [list, 0, 0], [0; 4], *f);
            0
        }
        EE_SCREENS_SEET_OP_EVENT_NEW => {
            let event = match a {
                EE_SCREENS_SEET_EV_SCREEN => find(x, EE_SCREENS_SEETX_EV_SCREEN, 0, 0, [k, 0, 0, 0]),
                EE_SCREENS_SEET_EV_SEE => find(x, EE_SCREENS_SEETX_EV_SEE, 0, 0, [k, 0, 0, 0]),
                EE_SCREENS_SEET_EV_SLIDER => {
                    let label = at(x, EE_SCREENS_SEET_A_LABEL, k);
                    find_f(x, EE_SCREENS_SEETX_EV_SLIDER, label, 0, [i[2], 0, 0, 0], *f)
                }
                EE_SCREENS_SEET_EV_SLIDER_FORMAT => {
                    let label = at(x, EE_SCREENS_SEET_A_LABEL, k);
                    let format: &[u8] = if i[2] != 0 { b"%#2.1f\0" } else { b"%2.0f\0" };
                    find_f(x, EE_SCREENS_SEETX_EV_SLIDER_FORMAT, label, format.as_ptr() as usize, [i[2], 0, 0, 0], *f)
                }
                EE_SCREENS_SEET_EV_USE_OBJECT_AREA => {
                    let label = at(x, EE_SCREENS_SEET_A_LABEL, k);
                    let combo = at(x, EE_SCREENS_SEET_A_SCEN, i[2]);
                    let radio = at(x, EE_SCREENS_SEET_A_RADIO, i[3]);
                    find(
                        x,
                        EE_SCREENS_SEETX_EV_USE_OBJECT_AREA,
                        label,
                        combo,
                        [radio as u32 as i32, (radio as u64 >> 32) as i32, 0, 0],
                    )
                }
                EE_SCREENS_SEET_EV_OBJECTS => find(x, EE_SCREENS_SEETX_EV_OBJECTS, p[0], p[1], [k, 0, 0, 0]),
                _ => 0,
            };
            // SAFETY: p2 is the out word (the op's contract)
            unsafe { *(p[2] as *mut usize) = event };
            0
        }
        EE_SCREENS_SEET_OP_EVENT_STRING => {
            let t = if i[3] != 0 {
                string_new(x, i[2])
            } else {
                // SAFETY: p2 holds i2 bytes (the op's contract)
                unsafe { bytes(p[2], i[2]) }
            };
            stf(x, EE_SCREENS_SEETX_EVENT_ADD_STRING, [t.as_ptr() as usize, p[1], 0], [t.len() as i32, 0, 0, 0], *f);
            0
        }
        EE_SCREENS_SEET_OP_FORMAT_NUMBER => {
            let format = string_new(x, i[2]);
            put(x, EE_SCREENS_SEETX_FORMAT_NUMBER, 0, &format, [i[3], 0, 0]);
            let t = text(x, EE_SCREENS_SEETX_KEPT_TEXT, 0, 0, [0; 3]);
            out(&t)
        }
        EE_SCREENS_SEET_OP_FORMAT_FLOAT => {
            stf(x, EE_SCREENS_SEETX_FORMAT_FLOAT, [p[1], 0, 0], [0; 4], *f);
            let t = text(x, EE_SCREENS_SEETX_KEPT_TEXT, 0, 0, [0; 3]);
            out(&t)
        }
        EE_SCREENS_SEET_OP_NAMES => {
            st(x, EE_SCREENS_SEETX_NAMES_CLEAR, [0; 3], [NAMES_OP, 0, 0, 0]);
            st(x, EE_SCREENS_SEETX_NAMES_GET, [0; 3], [NAMES_OP, i[2], 0, 0]);
            st(x, EE_SCREENS_SEETX_NAMES_COUNT, [0; 3], [NAMES_OP, 0, 0, 0])
        }
        EE_SCREENS_SEET_OP_OBJECT_NAME | EE_SCREENS_SEET_OP_TRIGGER_NAME => {
            let (get, name) = if op == EE_SCREENS_SEET_OP_OBJECT_NAME {
                (EE_SCREENS_SEETX_OBJECT, EE_SCREENS_SEETX_OBJECT_NAME)
            } else {
                (EE_SCREENS_SEETX_TRIGGER, EE_SCREENS_SEETX_TRIGGER_NAME)
            };
            let o = find(x, get, 0, 0, [i[2], 0, 0, 0]);
            if o == 0 {
                -1
            } else {
                let t = text(x, name, o, 0, [0; 3]);
                out(&t)
            }
        }
        EE_SCREENS_SEET_OP_CALAMITY => {
            let c = find(x, EE_SCREENS_SEETX_CALAMITY, 0, 0, [i[2], 0, 0, 0]);
            if c == 0 {
                return Some(0);
            }
            i[2] = st(x, EE_SCREENS_SEETX_CALAMITY_ID, [c, 0, 0], [0; 4]);
            i[3] = st(x, EE_SCREENS_SEETX_CALAMITY_NAME, [c, 0, 0], [0; 4]);
            1
        }
        EE_SCREENS_SEET_OP_GRAPHIC => {
            let g = find(x, EE_SCREENS_SEETX_GRAPHIC, 0, 0, [i[2], 0, 0, 0]);
            if g == 0 || st(x, EE_SCREENS_SEETX_GRAPHIC_LISTED, [g, 0, 0], [0; 4]) == 0 {
                return Some(0);
            }
            i[2] = st(x, EE_SCREENS_SEETX_GRAPHIC_ID, [g, 0, 0], [0; 4]);
            i[3] = st(x, EE_SCREENS_SEETX_GRAPHIC_NAME, [g, 0, 0], [0; 4]);
            1
        }
        EE_SCREENS_SEET_OP_BEHAVIOR => {
            // (an unknown field: the scale, the C++'s default)
            let field = if (EE_SCREENS_SEET_B_SETTABLE..=EE_SCREENS_SEET_B_FLOAT_SCALE).contains(&i[3]) {
                i[3]
            } else {
                EE_SCREENS_SEET_B_FLOAT_SCALE
            };
            st(x, EE_SCREENS_SEETX_BEHAVIOR + field as u32, [0; 3], [i[2], 0, 0, 0])
        }
        EE_SCREENS_SEET_OP_UNIT_TYPE => {
            let class = find(x, EE_SCREENS_SEETX_UNIT_TYPE_CLASS, 0, 0, [i[2], 0, 0, 0]);
            if st(x, EE_SCREENS_SEETX_CLASS_SELECTABLE, [class, 0, 0], [0; 4]) == 0 {
                return Some(-1);
            }
            i[2] = st(x, EE_SCREENS_SEETX_CLASS_TYPE, [class, 0, 0], [0; 4]);
            let domains = st(x, EE_SCREENS_SEETX_CLASS_DOMAINS, [class, 0, 0], [0; 4]);
            if domains & MOVEMENT_AIR == MOVEMENT_AIR {
                EE_SCREENS_SEET_D_AIR
            } else if domains & MOVEMENT_SEA == MOVEMENT_SEA {
                EE_SCREENS_SEET_D_SEA
            } else {
                EE_SCREENS_SEET_D_LAND
            }
        }
        EE_SCREENS_SEET_OP_TECHS => {
            // the tree's nodes and the epochs, those shown in the editor (i2: and making no unit, the condition
            // types'), sorted by name (the originals' first Show)
            st(x, EE_SCREENS_SEETX_TECHS_CLEAR, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SEETX_TECHS_NODE_IDS, [0; 3], [0; 4]);
            let mut epoch = 0;
            while epoch < st(x, EE_SCREENS_SEETX_EPOCH_COUNT, [0; 3], [0; 4]) as c_ulong {
                st(x, EE_SCREENS_SEETX_TECHS_PUSH, [0; 3], [epoch as i32, 0, 0, 0]);
                epoch += 1;
            }
            let mut index = 0;
            while index < st(x, EE_SCREENS_SEETX_TECHS_COUNT, [0; 3], [0; 4]) {
                let id = st(x, EE_SCREENS_SEETX_TECHS_AT, [0; 3], [index, 0, 0, 0]);
                if st(x, EE_SCREENS_SEETX_TECH_SHOWN, [0; 3], [id, 0, 0, 0]) != 0
                    && (i[2] == 0 || st(x, EE_SCREENS_SEETX_TECH_NO_UNIT, [0; 3], [id, 0, 0, 0]) != 0)
                {
                    index += 1;
                } else {
                    st(x, EE_SCREENS_SEETX_TECHS_ERASE, [0; 3], [index, 0, 0, 0]);
                }
            }
            st(x, EE_SCREENS_SEETX_TECHS_SORT, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SEETX_TECHS_COUNT, [0; 3], [0; 4])
        }
        EE_SCREENS_SEET_OP_TECH_LINE => {
            let node = find(x, EE_SCREENS_SEETX_TECH_NODE, 0, 0, [i[2], 0, 0, 0]);
            let mut t = tech_name(x, i[2] as c_long);
            if node != 0 && c_str(&t) == b"FONTS" {
                let id = st(x, EE_SCREENS_SEETX_NODE_ID, [node, 0, 0], [0; 4]) as c_long;
                t = format!("BAD Node {id} - (name not available in release)").into_bytes();
            }
            out(&t)
        }
        EE_SCREENS_SEET_OP_TECH_NAME => {
            let t = tech_name(x, i[2] as c_long);
            out(&t)
        }
        EE_SCREENS_SEET_OP_TSCREEN => {
            let what = match i[2] {
                EE_SCREENS_SEET_TS_HIDE_TEXT => SETS_HIDE_TEXT,
                EE_SCREENS_SEET_TS_SHOW_ALPHA_SORT => SETS_SHOW_ALPHA_SORT,
                _ => SETS_SHOW_TEXT,
            };
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [what, 0, 0, 0]);
            0
        }
        EE_SCREENS_SEET_OP_TEXT_BOX => {
            let b = trigger_box(x, true);
            let t = text(x, EE_SCREENS_TRGX_R_EDIT_ALL_TEXT, b, 0, [0; 3]);
            out(&t)
        }
        // ---- the entry's ops (ee_screenscpp_seet's and ee_screenscpp_sect's own)
        EE_SCREENS_SEET_OP_ENTRY => {
            let (get, fields) = if condition {
                (EE_SCREENS_SEETX_CONDITION_ENTRY, EE_SCREENS_SEETX_CONDITION_FIELDS)
            } else {
                (EE_SCREENS_SEETX_EFFECT_ENTRY, EE_SCREENS_SEETX_EFFECT_FIELDS)
            };
            let e = find(x, get, 0, 0, [i[2], 0, 0, 0]);
            let r = if e != 0 { find(x, fields, e, 0, [0; 4]) } else { 0 };
            // SAFETY: p2 holds two out words (the op's contract)
            unsafe {
                let w = p[2] as *mut usize;
                *w = e;
                *w.add(1) = r;
            }
            0
        }
        EE_SCREENS_SEET_OP_LIST => {
            let list = if condition { EE_SCENARIO_LIST_CONDITIONS } else { EE_SCENARIO_LIST_EFFECTS };
            entry_list(x, list, i[2], i[3]);
            0
        }
        EE_SCREENS_SEET_OP_EFFECT_TEXT | EE_SCREENS_SEET_OP_SET_EFFECT_TEXT => {
            let field = if condition {
                match i[2] {
                    EE_SCREENS_SEET_S_CHAT => 0,
                    EE_SCREENS_SEET_S_TEXT => 1,
                    _ => 2,
                }
            } else {
                match i[2] {
                    EE_SCREENS_SEET_S_NEW_NAME => 0,
                    EE_SCREENS_SEET_S_MESSAGE => 1,
                    EE_SCREENS_SEET_S_FILE_NAME => 2,
                    EE_SCREENS_SEET_S_DESCRIPTION => 3,
                    EE_SCREENS_SEET_S_TEXT => 4,
                    _ => 5,
                }
            };
            let (get, set) = if condition {
                (EE_SCREENS_SEETX_CONDITION_TEXT, EE_SCREENS_SEETX_SET_CONDITION_TEXT)
            } else {
                (EE_SCREENS_SEETX_EFFECT_TEXT, EE_SCREENS_SEETX_SET_EFFECT_TEXT)
            };
            if op == EE_SCREENS_SEET_OP_EFFECT_TEXT {
                let t = text(x, get + field, p[1], 0, [0; 3]);
                out(&t)
            } else {
                // SAFETY: p2 holds i3 bytes (the op's contract)
                let t = unsafe { bytes(p[2], i[3]) };
                put(x, set + field, p[1], &t, [0; 3]);
                0
            }
        }
        EE_SCREENS_SEET_OP_SET_OBJECTS if !condition => {
            st(x, EE_SCREENS_SEETX_SET_OBJECTS, [p[1], p[2], 0], [i[2].max(0), 0, 0, 0]);
            0
        }
        _ => return None,
    })
}

/// The controls record's member `a`, `k` (`SeetC::At`).
fn slot(x: &mut Trgx, controls: usize, condition: bool, a: u32, k: i32) -> usize {
    find(x, EE_SCREENS_TRGX_R_SLOT, controls, 0, [condition as i32, a as i32, k, 0])
}

/// `SeetC::Combo`: the control of array `a` when it holds combo boxes, else none.
fn combo_at(x: &mut Trgx, controls: usize, condition: bool, a: u32, k: i32) -> usize {
    if matches!(
        a,
        EE_SCREENS_SEET_A_COMBO | EE_SCREENS_SEET_A_ECOMBO | EE_SCREENS_SEET_A_SCEN | EE_SCREENS_SEET_A_PLAYER
    ) {
        slot(x, controls, condition, a, k)
    } else {
        0
    }
}

/// `SeetBytes`' input: the `n` bytes at `p`.
///
/// # Safety
/// `p` holds `n` bytes (or `n` is not positive).
unsafe fn bytes(p: usize, n: i32) -> Vec<u8> {
    if n <= 0 || p == 0 {
        return Vec::new();
    }
    // SAFETY: the contract
    unsafe { core::slice::from_raw_parts(p as *const u8, n as usize) }.to_vec()
}

/// `SeetWrite` into the caller's buffer `out` (capacity `cap`): the whole length.
///
/// # Safety
/// `out` holds `cap` bytes (or `cap` is not positive).
unsafe fn write(out: usize, cap: i32, t: &[u8]) -> i32 {
    let n = t.len().min(cap.max(0) as usize);
    if n > 0 {
        // SAFETY: the contract
        unsafe { core::ptr::copy_nonoverlapping(t.as_ptr(), out as *mut u8, n) };
    }
    t.len() as i32
}

// --------------------------------------------------------------------------------------- the object screen's host

/// The object screen's host op composed (`ee_screenscpp_seob`), or `None` for the C++ host's.
pub fn seob(x: &mut Trgx, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    // the ops on a scenario object (the C++'s: nothing for a missing one but the existence test)
    let object_op = matches!(
        op,
        EE_SCREENS_SEOB_OP_OBJ_GET
            | EE_SCREENS_SEOB_OP_OBJ_SET
            | EE_SCREENS_SEOB_OP_OBJ_EXISTS
            | EE_SCREENS_SEOB_OP_OBJ_SET_NAME
    ) || (EE_SCREENS_SEOB_OP_OBJ_NAME_FROM_SCREEN..=EE_SCREENS_SEOB_OP_SELECT_OBJECT_UNITS)
        .contains(&op);
    let o = if object_op { find(x, EE_SCREENS_SEETX_OBJECT, 0, 0, [i[0], 0, 0, 0]) } else { 0 };
    if object_op && o == 0 && op != EE_SCREENS_SEOB_OP_OBJ_EXISTS {
        return Some(0);
    }
    Some(match op {
        EE_SCREENS_SEOB_OP_SELECT_UNITS => {
            let form = p[2];
            let click = find(x, EE_SCREENS_SEOBX_NEW_CLICK, 0, 0, [0; 4]);
            let lasso = find(x, EE_SCREENS_SEOBX_NEW_LASSO, 0, 0, [0; 4]);
            let select = find_in(x, EE_SCREENS_SEOBX_NEW_SELECT, form, click, lasso, [i[0], 0, 0, 0], [0.0; 4]);
            st(x, EE_SCREENS_SEOBX_ADD_CONTROL, [form, select, 0], [0; 4]);
            // SAFETY: p0 holds three out words (the op's contract)
            unsafe {
                let w = p[0] as *mut usize;
                *w = click;
                *w.add(1) = lasso;
                *w.add(2) = select;
            }
            0
        }
        EE_SCREENS_SEOB_OP_RELEASE => {
            st(x, EE_SCREENS_SEOBX_LASSO_RELEASE, [p[1], 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEOBX_CLICK_RELEASE, [p[0], 0, 0], [0; 4]);
            0
        }
        EE_SCREENS_SEOB_OP_UNIT_TYPES => {
            st(x, EE_SCREENS_SEOBX_TYPES_CLEAR, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SEOBX_TYPES_PUSH, [0; 3], [0; 4]);
            let mut t = 1;
            while t < st(x, EE_SCREENS_SEETX_CLASS_COUNT, [0; 3], [0; 4]) as c_ulong {
                let n = t as i32;
                if st(x, EE_SCREENS_SEETX_CLASS_EXISTS, [0; 3], [n, 0, 0, 0]) != 0
                    && st(x, EE_SCREENS_SEETX_CLASS_CATEGORY, [0; 3], [n, 0, 0, 0]) != CATEGORY_PROJECTILE
                    && st(x, EE_SCREENS_SEETX_CLASS_CATEGORY, [0; 3], [n, 0, 0, 0]) != CATEGORY_MISSILE
                    && st(x, EE_SCREENS_SEETX_CLASS_SELECTABLE_AT, [0; 3], [n, 0, 0, 0]) != 0
                {
                    st(x, EE_SCREENS_SEOBX_TYPES_PUSH, [0; 3], [n, 0, 0, 0]);
                }
                t += 1;
            }
            st(x, EE_SCREENS_SEOBX_TYPES_SORT, [0; 3], [0; 4]);
            let n = st(x, EE_SCREENS_SEOBX_TYPES_COUNT, [0; 3], [0; 4]);
            let w = p[0] as *mut u32;
            for k in 0..n.min(i[0]).max(0) {
                let v = st(x, EE_SCREENS_SEOBX_TYPES_AT, [0; 3], [k, 0, 0, 0]);
                // SAFETY: p0 holds i0 words (the op's contract)
                unsafe { *w.add(k as usize) = v as u32 };
            }
            n
        }
        EE_SCREENS_SEOB_OP_SM => {
            entry_list(x, EE_SCENARIO_LIST_OBJECTS, i[0], i[1]);
            0
        }
        EE_SCREENS_SEOB_OP_TRIGGER_TEXT => {
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_HIDE_TEXT, 0, 0, 0]);
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_SHOW_ALPHA_SORT, 0, 0, 0]);
            0
        }
        EE_SCREENS_SEOB_OP_PLAYER_COMBO => {
            let (skop, arg) = match i[0] {
                EE_SCREENS_SEOB_PC_COUNT => (EE_SCREENS_SKOP_SET_PLAYER_COUNT, i[1]),
                EE_SCREENS_SEOB_PC_FLAGS => (EE_SCREENS_SKOP_SET_FLAGS, i[1]),
                EE_SCREENS_SEOB_PC_GET => (EE_SCREENS_SKOP_SELECTED_PLAYER, 0),
                _ => (EE_SCREENS_SKOP_SET_SELECTED_PLAYER, i[1]),
            };
            let r = st(x, EE_SCREENS_SEETX_SHELL_OP, [p[0], 0, 0], [skop as i32, arg, 0, 0]);
            if i[0] == EE_SCREENS_SEOB_PC_GET {
                r
            } else {
                0
            }
        }
        EE_SCREENS_SEOB_OP_APPEND_NAMES => {
            let kind =
                if i[0] == EE_SCREENS_SEOB_NAMES_AREAS { EE_SCENARIO_NAMES_AREAS } else { EE_SCENARIO_NAMES_OBJECTS };
            let names = names(x, NAMES_OWN, kind);
            for n in &names {
                put(x, EE_SCREENS_TRGX_R_COMBO_APPEND, p[0], n, [0; 3]);
                if p[1] != 0 {
                    put(x, EE_SCREENS_TRGX_R_COMBO_APPEND, p[1], n, [0; 3]);
                }
            }
            0
        }
        EE_SCREENS_SEOB_OP_OBJ_GET => {
            if seob_flag(i[1]) {
                st(x, EE_SCREENS_SEOBX_FLAG, [o, 0, 0], [i[1], 0, 0, 0])
            } else {
                st(x, EE_SCREENS_SEOBX_FIELD, [o, 0, 0], [i[1], 0, 0, 0])
            }
        }
        EE_SCREENS_SEOB_OP_OBJ_SET => {
            let set = if seob_flag(i[1]) {
                EE_SCREENS_SEOBX_SET_FLAG
            } else if i[1] == EE_SCREENS_SEOB_F_PARENT {
                EE_SCREENS_SEOBX_SET_PARENT
            } else {
                EE_SCREENS_SEOBX_SET_FIELD
            };
            st(x, set, [o, 0, 0], [i[1], i[2], 0, 0]);
            0
        }
        EE_SCREENS_SEOB_OP_OBJ_NAME_FROM_SCREEN => {
            let b = trigger_box(x, false);
            let t = text(x, EE_SCREENS_TRGX_R_EDIT_LINE, b, 0, [0; 3]);
            put(x, EE_SCREENS_SEOBX_SET_NAME, o, c_str(&t), [0; 3]);
            0
        }
        EE_SCREENS_SEOB_OP_OBJ_SET_UNITS => {
            st(x, EE_SCREENS_SEOBX_SET_UNITS, [o, p[0], 0], [i[1], 0, 0, 0]);
            0
        }
        EE_SCREENS_SEOB_OP_OBJ_EXISTS => (o != 0) as i32,
        EE_SCREENS_SEOB_OP_NAME_TO_EDIT => {
            let b = trigger_box(x, false);
            let t = text(x, EE_SCREENS_SEETX_OBJECT_NAME, o, 0, [0; 3]);
            put(x, EE_SCREENS_TRGX_R_EDIT_SET_TEXT, b, &t, [0; 3]);
            0
        }
        EE_SCREENS_SEOB_OP_SELECT_OBJECT_UNITS => {
            st(x, EE_SCREENS_SEOBX_UNITS_GET, [o, 0, 0], [0; 4]);
            let mut k = 0;
            while k < st(x, EE_SCREENS_SEOBX_UNITS_COUNT, [0; 3], [0; 4]) {
                let id = st(x, EE_SCREENS_SEOBX_UNITS_AT, [0; 3], [k, 0, 0, 0]);
                let unit = find(x, EE_SCREENS_SEOBX_WORLD_UNIT, 0, 0, [id, 0, 0, 0]);
                if unit != 0 {
                    st(x, EE_SCREENS_SEOBX_SELECT_UNIT, [p[0], unit, 0], [0; 4]);
                }
                k += 1;
            }
            0
        }
        EE_SCREENS_SEOB_OP_UNIT_LIST_FILL => {
            let (list, editor) = (p[0], p[1]);
            let mut name = Vec::new();
            st(x, EE_SCREENS_TRGX_R_LIST_CLEAR, [list, 0, 0], [0; 4]);
            let count = st(x, EE_SCREENS_SEOBX_SELECTED_COUNT, [editor, 0, 0], [0; 4]);
            for k in 0..count as c_ulong {
                let id = st(x, EE_SCREENS_SEOBX_SELECTED_NAME_ID, [editor, 0, 0], [k as i32, 0, 0, 0]);
                if let Some(t) = string(x, id) {
                    name = t;
                }
                put(x, EE_SCREENS_TRGX_R_LIST_APPEND, list, &name, [0; 3]);
            }
            count
        }
        EE_SCREENS_SEOB_OP_TEXT => {
            let t = match i[0] {
                EE_SCREENS_SEOB_TEXT_OBJECT_NAME => {
                    let o = find(x, EE_SCREENS_SEETX_OBJECT, 0, 0, [i[1], 0, 0, 0]);
                    if o == 0 {
                        return Some(-1);
                    }
                    text(x, EE_SCREENS_SEETX_OBJECT_NAME, o, 0, [0; 3])
                }
                EE_SCREENS_SEOB_TEXT_UNIT_CLASS => {
                    let id = st(x, EE_SCREENS_SEOBX_SELECTED_NAME_ID, [p[1], 0, 0], [i[1], 0, 0, 0]);
                    string_new(x, id)
                }
                _ => Vec::new(),
            };
            // SAFETY: p0 holds i2 bytes (the op's contract)
            unsafe { write(p[0], i[2], c_str(&t)) }
        }
        EE_SCREENS_SEOB_OP_OBJ_SET_NAME => {
            // SAFETY: p0 holds i1 bytes (the op's contract)
            let t = unsafe { bytes(p[0], i[1]) };
            put(x, EE_SCREENS_SEOBX_SET_NAME, o, c_str(&t), [0; 3]);
            let b = trigger_box(x, false);
            let t = text(x, EE_SCREENS_SEETX_OBJECT_NAME, o, 0, [0; 3]);
            put(x, EE_SCREENS_TRGX_R_EDIT_SET_TEXT, b, &t, [0; 3]);
            0
        }
        EE_SCREENS_SEOB_OP_VIEW_PRESS => {
            (stf(x, EE_SCREENS_SEOBX_IN_VIEW, [0; 3], [0; 4], *f) != 0
                && (st(x, EE_SCREENS_SEOBX_LBUTTON_JUST, [0; 3], [0; 4]) != 0
                    || st(x, EE_SCREENS_SEOBX_RBUTTON_JUST, [0; 3], [0; 4]) != 0)) as i32
        }
        _ => return None,
    })
}

/// `SeobFlag`'s fields (the three flags).
fn seob_flag(field: i32) -> bool {
    matches!(field, EE_SCREENS_SEOB_F_SELECT_ON_MAP | EE_SCREENS_SEOB_F_DYNAMIC | EE_SCREENS_SEOB_F_PERCENT)
}

// -------------------------------------------------------------------------------------- the trigger screen's host

/// The trigger screen's host op composed (`ee_screenscpp_setr`), or `None` for the C++ host's.
pub fn setr(x: &mut Trgx, op: u32, p: [usize; 3], i: &mut [i32; 4]) -> Option<i32> {
    let trigger_op = (EE_SCREENS_SETR_OP_EXISTS..=EE_SCREENS_SETR_OP_DELETE).contains(&op);
    let t = if trigger_op { find(x, EE_SCREENS_SEETX_TRIGGER, 0, 0, [i[0], 0, 0, 0]) } else { 0 };
    if trigger_op && t == 0 {
        return Some(0);
    }
    Some(match op {
        EE_SCREENS_SETR_OP_SM => {
            entry_list(x, EE_SCENARIO_LIST_TRIGGERS, i[0], i[1]);
            0
        }
        EE_SCREENS_SETR_OP_EXISTS => 1,
        EE_SCREENS_SETR_OP_GET => {
            // (past the counts: the effects')
            let v = if (EE_SCREENS_SETR_V_ON..=EE_SCREENS_SETR_V_CONDITIONS).contains(&i[1]) {
                i[1]
            } else {
                EE_SCREENS_SETR_V_EFFECTS
            };
            st(x, EE_SCREENS_SETRX_GET + v as u32, [t, 0, 0], [0; 4])
        }
        EE_SCREENS_SETR_OP_SET => {
            if (EE_SCREENS_SETR_V_ON..=EE_SCREENS_SETR_V_CINEMATIC).contains(&i[1]) {
                st(x, EE_SCREENS_SETRX_SET + i[1] as u32, [t, 0, 0], [i[2], 0, 0, 0]);
            }
            0
        }
        EE_SCREENS_SETR_OP_SET_CONDITION | EE_SCREENS_SETR_OP_SET_EFFECT => {
            let set = if op == EE_SCREENS_SETR_OP_SET_CONDITION {
                EE_SCREENS_SETRX_SET_CONDITION
            } else {
                EE_SCREENS_SETRX_SET_EFFECT
            };
            st(x, set, [t, 0, 0], [i[1], i[2], i[3], 0]);
            0
        }
        EE_SCREENS_SETR_OP_APPEND_NAMES => {
            let kind = if i[0] == 0 { EE_SCENARIO_NAMES_CONDITIONS } else { EE_SCENARIO_NAMES_EFFECTS };
            let names = names(x, NAMES_OWN, kind);
            for n in &names {
                put(x, EE_SCREENS_TRGX_R_COMBO_APPEND, p[0], n, [0; 3]);
                put(x, EE_SCREENS_TRGX_R_COMBO_APPEND, p[1], n, [0; 3]);
            }
            0
        }
        EE_SCREENS_SETR_OP_SHOW_TEXT => {
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_SHOW_TEXT, 0, 0, 0]);
            0
        }
        EE_SCREENS_SETR_OP_HIDE_TEXT => {
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_HIDE_TEXT, 0, 0, 0]);
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_HIDE_ALPHA_SORT, 0, 0, 0]);
            0
        }
        EE_SCREENS_SETR_OP_TEXTS_FROM_SCREEN => {
            let b = trigger_box(x, false);
            let name = text(x, EE_SCREENS_TRGX_R_EDIT_LINE, b, 0, [0; 3]);
            put(x, EE_SCREENS_SETRX_SET_NAME, t, c_str(&name), [0; 3]);
            let b = trigger_box(x, true);
            let all = text(x, EE_SCREENS_TRGX_R_EDIT_ALL_TEXT, b, 0, [0; 3]);
            put(x, EE_SCREENS_SETRX_SET_TEXT, t, c_str(&all), [0; 3]);
            0
        }
        EE_SCREENS_SETR_OP_TEXTS_TO_SCREEN => {
            let b = trigger_box(x, false);
            let name = text(x, EE_SCREENS_SETRX_NAME, t, 0, [0; 3]);
            put(x, EE_SCREENS_TRGX_R_EDIT_SET_TEXT, b, &name, [0; 3]);
            let b = trigger_box(x, true);
            let all = text(x, EE_SCREENS_SETRX_TEXT, t, 0, [0; 3]);
            put(x, EE_SCREENS_TRGX_R_EDIT_SET_TEXT, b, &all, [0; 3]);
            0
        }
        EE_SCREENS_SETR_OP_ENTRY => {
            let (get, first, value, operator) = if i[1] == 0 {
                (
                    EE_SCREENS_SETRX_CONDITION,
                    EE_SCREENS_SETRX_CONDITION_NOT,
                    EE_SCREENS_SETRX_CONDITION_VALUE,
                    EE_SCREENS_SETRX_CONDITION_OPERATOR,
                )
            } else {
                (
                    EE_SCREENS_SETRX_EFFECT,
                    EE_SCREENS_SETRX_EFFECT_DELAY,
                    EE_SCREENS_SETRX_EFFECT_INDEX,
                    EE_SCREENS_SETRX_EFFECT_OPERATOR,
                )
            };
            let w = find(x, get, t, 0, [i[2], 0, 0, 0]);
            if w == 0 {
                return Some(0);
            }
            match i[3] {
                EE_SCREENS_SETR_W_FIRST => st(x, first, [w, 0, 0], [0; 4]),
                EE_SCREENS_SETR_W_VALUE => st(x, value, [w, 0, 0], [0; 4]),
                EE_SCREENS_SETR_W_OPERATOR => st(x, operator, [w, 0, 0], [0; 4]),
                _ => 1,
            }
        }
        EE_SCREENS_SETR_OP_SET_OPERATOR => {
            let set =
                if i[1] == 0 { EE_SCREENS_SETRX_SET_CONDITION_OPERATOR } else { EE_SCREENS_SETRX_SET_EFFECT_OPERATOR };
            st(x, set, [t, 0, 0], [i[2], i[3], 0, 0]);
            0
        }
        EE_SCREENS_SETR_OP_DELETE => {
            let del = if i[1] == 0 { EE_SCREENS_SETRX_DELETE_CONDITION } else { EE_SCREENS_SETRX_DELETE_EFFECT };
            st(x, del, [t, 0, 0], [i[2], 0, 0, 0]);
            0
        }
        EE_SCREENS_SETR_OP_REMOVE_TEXT => {
            let format = string_new(x, IDS_DEL);
            put(x, EE_SCREENS_SETRX_FORMAT_REMOVE, p[1], c_str(&format), [0; 3]);
            st(x, EE_SCREENS_SETRX_BUTTON_KEPT_TEXT, [p[0], 0, 0], [0; 4]);
            0
        }
        EE_SCREENS_SETR_OP_END_TEXT => {
            let end = string_new(x, IDS_TRIGGER_EDITOR_END);
            put(x, EE_SCREENS_SETRX_COMBO_LABEL_TEXT, p[0], &end, [0; 3]);
            0
        }
        _ => return None,
    })
}

/// `EESETriggerJump`: the trigger screen's tab of the entry right-clicked in a combo box opened (which tab and entry:
/// `ee_screens_trigger_jump`), the tab's radio checked without its event.
pub fn trigger_jump(x: &mut Trgx, kind: c_ulong, selection: c_ulong) {
    let tab = find(x, EE_SCREENS_SETRX_TRIGGER_TAB, 0, 0, [0; 4]);
    let count = st(x, EE_SCREENS_SETRX_CONDITION_COUNT, [0; 3], [0; 4]) as c_ulong;
    let mut object: c_ulong = 0;
    while object < count && st(x, EE_SCREENS_SETRX_CONDITION_IS_OBJECT, [0; 3], [object as i32, 0, 0, 0]) == 0 {
        object += 1;
    }
    // (`ee_screens_trigger_jump`'s decision)
    let Some((screen, entry)) =
        crate::screens::editor_trigger::trigger_jump(kind as u64, selection as u64, object as u64, count as u64)
    else {
        return;
    };
    let (screen, entry) = (screen as c_long, entry as u32 as i32);
    let button = find(x, EE_SCREENS_TRGX_R_SETS_CONTROL, tab, 0, [SETS_TAB, screen as i32, 0, 0]);
    let event = find(x, EE_SCREENS_TRGX_R_TRIGGER_EVENT, button, 0, [TRIGGER_CHECKED, 0, 0, 0]);
    if event != 0 {
        st(x, EE_SCREENS_TRGX_R_EVENT_CALL, [button, event, 0], [2, 0, 0, 0]);
    }
    st(x, EE_SCREENS_TRGX_R_SET_TRIGGER, [button, 0, 0], [TRIGGER_CHECKED, 0, 0, 0]);
    st(x, EE_SCREENS_SETRX_RADIO_CHECK, [button, 0, 0], [0; 4]);
    st(x, EE_SCREENS_TRGX_R_SET_TRIGGER, [button, event, 0], [TRIGGER_CHECKED, 0, 0, 0]);
    st(x, EE_SCREENS_TRGX_R_SETS_SUB_SELECT, [tab, 0, 0], [screen as i32, entry, 0, 0]);
    st(x, EE_SCREENS_TRGX_R_SETS_CALL, [tab, 0, 0], [SETS_SHOW_TAB, TAB_TRIGGER + screen as i32, 0, 0]);
}

// ---------------------------------------------------------------------------------------- the trigger tab's host

/// The trigger tab's host op composed (`ee_screenscpp_sets`), or `None` for the C++ host's.
pub fn sets(x: &mut Trgx, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    let form = p[1];
    let event = |x: &mut Trgx, control: usize, trigger: i32, code: i32| {
        let e = find(x, EE_SCREENS_SETSX_SCREEN_EVENT, 0, 0, [code, 0, 0, 0]);
        st(x, EE_SCREENS_SETSX_SET_TRIGGER, [control, e, 0], [trigger, 0, 0, 0]);
        st(x, EE_SCREENS_SETSX_EVENT_RELEASE, [e, 0, 0], [0; 4]);
    };
    let yellow = |x: &mut Trgx, t: usize| {
        st(x, EE_SCREENS_SEETX_YELLOW, [t, 0, 0], [0; 4]);
        st(x, EE_SCREENS_SEETX_TRANSPARENT, [t, 0, 0], [0; 4]);
    };
    let out = |made: usize| {
        // SAFETY: p0 is the out word (the op's contract)
        unsafe { *(p[0] as *mut usize) = made };
    };
    Some(match op {
        EE_SCREENS_SETS_OP_TAB => {
            let k = i[1] as c_ulong;
            let t = string_new(x, IDS_TRIGGER_SCREEN_TAB_FIRST + k as i32);
            let left = k as f32 * 60.0 + 4.0;
            let tab = {
                let mut a = [k as i32, i[0], t.len() as i32, 0];
                let mut r = [left, 458.0, left + 57.0, 477.0];
                let mut made = 0usize;
                x(
                    EE_SCREENS_SETSX_NEW_TAB,
                    [form, t.as_ptr() as usize, &mut made as *mut usize as usize],
                    &mut a,
                    &mut r,
                );
                made
            };
            st(x, EE_SCREENS_SETSX_UP_CHECKED_NULL, [tab, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SETSX_DOWN_CHECKED_NULL, [tab, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SETSX_DOWN_UNCHECKED_NULL, [tab, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SETSX_UP_UNCHECKED_NULL, [tab, 0, 0], [0; 4]);
            stf(x, EE_SCREENS_SETSX_LABEL_RECT, [tab, 0, 0], [0; 4], [left + 2.0, 460.0, left + 54.0, 474.0]);
            stf(x, EE_SCREENS_SETSX_BG_OPACITY, [tab, 0, 0], [0; 4], [0.4, 0.0, 0.0, 0.0]);
            event(x, tab, TRIGGER_CHECKED, EE_SCREENS_SE_SHOW_TAB as i32);
            let g = find(x, EE_SCREENS_SETSX_RADIO_GETEXT, tab, 0, [0; 4]);
            yellow(x, g);
            st(x, EE_SCREENS_SETSX_ADD_CONTROL, [form, tab, 0], [0; 4]);
            out(tab);
            0
        }
        EE_SCREENS_SETS_OP_IMAGE_BUTTON => {
            let (up, down) = match i[1] {
                0 => (0, 1),
                1 => (2, 3),
                _ => (4, 5),
            };
            let up = find(x, EE_SCREENS_SETSX_BITMAP + up, form, 0, [0; 4]);
            let down = find(x, EE_SCREENS_SETSX_BITMAP + down, form, 0, [0; 4]);
            let id = if i[1] == 0 { 0x7dd } else { 0x7d8 + i[1] - 1 };
            let b = find_in(x, EE_SCREENS_SETSX_NEW_IMAGE_BUTTON, form, up, down, [id, i[0], 0, 0], *f);
            event(x, b, TRIGGER_BUTTON_UP, i[2]);
            st(x, EE_SCREENS_SETSX_ADD_CONTROL, [form, b, 0], [0; 4]);
            out(b);
            0
        }
        EE_SCREENS_SETS_OP_SHOW_IDS => {
            let c = find_f(x, EE_SCREENS_SETSX_NEW_SHOW_IDS, form, 0, [i[0], 0, 0, 0], *f);
            event(x, c, TRIGGER_BUTTON_UP, EE_SCREENS_SE_SHOW_DEBUG_IDS as i32);
            st(x, EE_SCREENS_SETSX_ADD_CONTROL, [form, c, 0], [0; 4]);
            out(c);
            0
        }
        EE_SCREENS_SETS_OP_NAMES => {
            // (the trigger, condition, effect, object screens; any other the area screen's)
            let n = if (0..4).contains(&i[0]) { i[0] as u32 } else { 4 };
            st(x, EE_SCREENS_SETSX_NAMES_CLEAR, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SETSX_SUB_NAMES + n, [0, p[1], 0], [0; 4]);
            st(x, EE_SCREENS_SETSX_LIST_APPEND_NAMES, [p[0], 0, 0], [0; 4]);
            0
        }
        EE_SCREENS_SETS_OP_VIEW => {
            let editor = find(x, EE_SCREENS_SETSX_EDITOR, p[0], 0, [0; 4]);
            sets_view(x, editor, i[0] != 0);
            0
        }
        EE_SCREENS_SETS_OP_DRAW => {
            // (the panel: the tab's +0x18)
            if st(x, EE_SCREENS_SETSX_WAS_UI_HIDDEN, [0; 3], [0; 4]) == 0
                && st(x, EE_SCREENS_SETSX_CLIP_PANEL, [p[1], 0, 0], [0; 4]) != 0
            {
                st(x, EE_SCREENS_SETSX_DRAW_PANEL, [0; 3], [0; 4]);
            }
            0
        }
        _ => return None,
    })
}

/// `SetsView`: Leave's and Enter's views (the hide-UI box, the game view, the camera flags, the mini map, the
/// redraw).
fn sets_view(x: &mut Trgx, editor: usize, enter: bool) {
    let hide = find(x, EE_SCREENS_SETSX_CONTROL_BY_ID, editor, 0, [HIDE_UI_CHECK_BOX, 0, 0, 0]);
    if !enter {
        st(x, EE_SCREENS_SETSX_SET_DISPLAY, [hide, 0, 0], [DISPLAY_HIDDEN, 0, 0, 0]);
        stf(x, EE_SCREENS_SETSX_SET_GAME_VIEW, [editor, 0, 0], [0; 4], [0.0, 0.0, 1024.0, 606.0]);
    } else {
        let view = [0.0, 0.0, 1024.0, 447.0];
        stf(x, EE_SCREENS_SETSX_GAME_VIEW_RECT, [0; 3], [0; 4], view);
        st(x, EE_SCREENS_SETSX_SET_DISPLAY, [hide, 0, 0], [DISPLAY_SHOWN, 0, 0, 0]);
        // the hide-UI event's two views (the original's inline SetRectangles; the event is an EESEERust)
        stf(x, EE_SCREENS_SETSX_HIDE_UI_VIEWS, [hide, view.as_ptr() as usize, 0], [0; 4], [0.0, 0.0, 1024.0, 606.0]);
        stf(x, EE_SCREENS_SETSX_SET_GAME_VIEW, [editor, 0, 0], [0; 4], view);
        let hide = find(x, EE_SCREENS_SETSX_CONTROL_BY_ID, editor, 0, [HIDE_UI_CHECK_BOX, 0, 0, 0]);
        st(x, EE_SCREENS_SETSX_SET_CHECKED, [hide, 0, 0], [0; 4]);
        st(x, EE_SCREENS_SETSX_GROUP_ENABLE, [editor, 0, 0], [14, 0, 0, 0]);
    }
    st(x, EE_SCREENS_SETSX_CLEAR_529066, [0; 3], [0; 4]);
    st(x, EE_SCREENS_SETSX_CLEAR_FOLLOWING, [0; 3], [0; 4]);
    st(x, EE_SCREENS_SETSX_CLEAR_529068, [0; 3], [0; 4]);
    if st(x, EE_SCREENS_SETSX_MINIMAP, [0; 3], [0; 4]) != 0 {
        st(x, EE_SCREENS_SETSX_MINIMAP_REFRESH, [0; 3], [0; 4]);
    }
    st(x, EE_SCREENS_SETSX_REDRAW_ALL, [0; 3], [0; 4]);
    st(x, EE_SCREENS_SETSX_ALL_DIRTY, [0; 3], [0; 4]);
}

// ------------------------------------------------------------------------------------------ the area screen's host

/// The area screen's host op composed (`ee_screenscpp_sear`), or `None` for the C++ host's.
pub fn sear(x: &mut Trgx, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    Some(match op {
        EE_SCREENS_SEAR_OP_LIST => {
            entry_list(x, EE_SCENARIO_LIST_AREAS, i[0], i[1]);
            0
        }
        EE_SCREENS_SEAR_OP_ACTIVATE => {
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_HIDE_TEXT, 0, 0, 0]);
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_SHOW_ALPHA_SORT, 0, 0, 0]);
            0
        }
        EE_SCREENS_SEAR_OP_LOAD => {
            let area = find(x, EE_SCREENS_SEARX_AREA, 0, 0, [i[0], 0, 0, 0]);
            if area == 0 {
                return Some(0);
            }
            st(x, EE_SCREENS_SEETX_TRIGGER_CALL, [0; 3], [SETS_SHOW_TEXT, 0, 0, 0]);
            let b = trigger_box(x, false);
            let name = text(x, EE_SCREENS_SEARX_NAME, area, 0, [0; 3]);
            put(x, EE_SCREENS_TRGX_R_EDIT_SET_TEXT, b, &name, [0; 3]);
            let b = trigger_box(x, true);
            let t = text(x, EE_SCREENS_SEARX_TEXT, area, 0, [0; 3]);
            put(x, EE_SCREENS_TRGX_R_EDIT_SET_TEXT, b, &t, [0; 3]);
            for (n, v) in f.iter_mut().enumerate() {
                let mut g = [0.0f32; 4];
                x(EE_SCREENS_SEARX_RECT + n as u32, [area, 0, 0], &mut [0; 4], &mut g);
                *v = g[0];
            }
            i[1] = st(x, EE_SCREENS_SEARX_ANCHOR_X, [area, 0, 0], [0; 4]);
            i[2] = st(x, EE_SCREENS_SEARX_ANCHOR_Y, [area, 0, 0], [0; 4]);
            i[3] = st(x, EE_SCREENS_SEARX_HAS, [area, 0, 0], [0; 4]);
            let mode = st(x, EE_SCREENS_SEARX_SELECT_MODE, [area, 0, 0], [0; 4]);
            st(x, EE_SCREENS_TRGX_R_COMBO_SELECT, [p[0], 0, 0], [mode, 0, 0, 0]);
            1
        }
        EE_SCREENS_SEAR_OP_STORE => {
            // the original reads a missing area: nothing here (also for the two below)
            let area = find(x, EE_SCREENS_SEARX_AREA, 0, 0, [i[0], 0, 0, 0]);
            if area == 0 {
                return Some(0);
            }
            let b = trigger_box(x, false);
            let name = text(x, EE_SCREENS_TRGX_R_EDIT_LINE, b, 0, [0; 3]);
            put(x, EE_SCREENS_SEARX_SET_NAME, area, c_str(&name), [0; 3]);
            let b = trigger_box(x, true);
            let all = text(x, EE_SCREENS_TRGX_R_EDIT_ALL_TEXT, b, 0, [0; 3]);
            put(x, EE_SCREENS_SEARX_SET_TEXT, area, c_str(&all), [0; 3]);
            st(x, EE_SCREENS_SEARX_GET_SELECTION, [area, p[0], 0], [0; 4]);
            0
        }
        EE_SCREENS_SEAR_OP_SET_RECT | EE_SCREENS_SEAR_OP_SET_ANCHOR => {
            let area = find(x, EE_SCREENS_SEARX_AREA, 0, 0, [i[0], 0, 0, 0]);
            if area != 0 {
                if op == EE_SCREENS_SEAR_OP_SET_RECT {
                    stf(x, EE_SCREENS_SEARX_SET_RECT, [area, 0, 0], [0; 4], *f);
                } else {
                    st(x, EE_SCREENS_SEARX_SET_ANCHOR, [area, 0, 0], [i[1], i[2], 0, 0]);
                }
            }
            0
        }
        EE_SCREENS_SEAR_OP_PICK => {
            st(x, EE_SCREENS_SEARX_XY_RESET, [0; 3], [0; 4]);
            stf(x, EE_SCREENS_SEARX_POINT, [0; 3], [i[0], 0, 0, 0], *f);
            let picked = st(x, EE_SCREENS_SEARX_PICK, [0; 3], [0; 4]);
            i[1] = st(x, EE_SCREENS_SEARX_X, [0; 3], [0; 4]);
            i[2] = st(x, EE_SCREENS_SEARX_Y, [0; 3], [0; 4]);
            picked
        }
        EE_SCREENS_SEAR_OP_GROUND => {
            st(x, EE_SCREENS_SEARX_XY_RESET, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SEARX_LOCATION_RESET, [0; 3], [0; 4]);
            stf(x, EE_SCREENS_SEARX_POINT, [0; 3], [i[0], 0, 0, 0], *f);
            let tile = find(x, EE_SCREENS_SEARX_TILE_AT, 0, 0, [0; 4]);
            st(x, EE_SCREENS_SEARX_SCREEN_LINE, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SEARX_SOLVE, [tile, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEARX_GROUND_X, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SEARX_GROUND_Y, [0; 3], [0; 4]);
            i[1] = st(x, EE_SCREENS_SEARX_X, [0; 3], [0; 4]);
            i[2] = st(x, EE_SCREENS_SEARX_Y, [0; 3], [0; 4]);
            0
        }
        EE_SCREENS_SEAR_OP_LBUTTON => {
            let down = st(x, EE_SCREENS_SEARX_LBUTTON_DOWN, [0; 3], [0; 4]) != 0;
            let pressed = st(x, EE_SCREENS_SEARX_LBUTTON_PRESSED, [0; 3], [0; 4]) != 0;
            let unpressed = st(x, EE_SCREENS_SEARX_LBUTTON_UNPRESSED, [0; 3], [0; 4]) != 0;
            down as i32 | (pressed as i32) << 1 | (unpressed as i32) << 2
        }
        EE_SCREENS_SEAR_OP_PAINT_BEGIN => {
            let action = find(x, EE_SCREENS_SEARX_NEW_PAINT, 0, 0, [i[0], i[1], 0, 0]);
            // SAFETY: p0 is the out word (the op's contract)
            unsafe { *(p[0] as *mut usize) = action };
            st(x, EE_SCREENS_SEARX_BOUNDS_VIEW, [0; 3], [0; 4]);
            // the edge scroll's edges: the view less one
            let view = [
                fl(x, EE_SCREENS_SEARX_VIEW_LEFT),
                fl(x, EE_SCREENS_SEARX_VIEW_TOP),
                fl(x, EE_SCREENS_SEARX_VIEW_RIGHT) - 1.0,
                fl(x, EE_SCREENS_SEARX_VIEW_BOTTOM) - 1.0,
            ];
            stf(x, EE_SCREENS_SEARX_SET_EDGES, [0; 3], [0; 4], view);
            0
        }
        EE_SCREENS_SEAR_OP_PAINT_END => {
            st(x, EE_SCREENS_SEARX_BOUNDS_WINDOW, [0; 3], [0; 4]);
            // the edges of the window less one
            let right = fl(x, EE_SCREENS_SEARX_WINDOW_WIDTH) - 1.0;
            let bottom = fl(x, EE_SCREENS_SEARX_WINDOW_HEIGHT) - 1.0;
            stf(x, EE_SCREENS_SEARX_SET_EDGES, [0; 3], [0; 4], [0.0, 0.0, right, bottom]);
            let mut a = [0; 4];
            x(EE_SCREENS_SEARX_PAINT_AREA, [p[0], 0, 0], &mut a, &mut [0.0; 4]);
            *f = [a[0] as f32, a[1] as f32, a[2] as f32, a[3] as f32];
            st(x, EE_SCREENS_SEARX_PAINT_RELEASE, [p[0], 0, 0], [0; 4]);
            0
        }
        _ => return None,
    })
}

// -------------------------------------------------------------------------------------- the instructions tab's host

/// The instructions tab's host op composed (`ee_screenscpp_sein`), or `None` for the C++ host's.
pub fn sein(x: &mut Trgx, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    Some(match op {
        EE_SCREENS_SEIN_OP_PATHS => {
            let paths = p[0] as *mut usize;
            for k in 0..3 {
                st(x, EE_SCREENS_SEINX_DIR_RESET, [0; 3], [k, 0, 0, 0]);
            }
            for k in 0..3 {
                st(x, EE_SCREENS_SEINX_DIR_VOLUME, [0; 3], [k, 0, 0, 0]);
                st(x, EE_SCREENS_SEINX_DIR_DIRECTORY, [0; 3], [k, 0, 0, 0]);
                let path = find(x, EE_SCREENS_SEINX_NEW_PATH, 0, 0, [0; 4]);
                // SAFETY: p0 holds three out words (the op's contract)
                unsafe { *paths.add(k as usize) = path };
                st(x, EE_SCREENS_SEINX_ADD_DIRECTORY, [path, 0, 0], [k, 0, 0, 0]);
            }
            for k in 0..3 {
                if st(x, EE_SCREENS_SEINX_IS_DIRECTORY, [0; 3], [k, 0, 0, 0]) == 0 {
                    st(x, EE_SCREENS_SEINX_CREATE_DIRECTORY, [0; 3], [k, 0, 0, 0]);
                }
            }
            0
        }
        EE_SCREENS_SEIN_OP_PATHS_RELEASE => {
            for k in 0..3 {
                // SAFETY: p0 holds the three paths (the op's contract)
                let path = unsafe { *(p[0] as *const usize).add(k) };
                st(x, EE_SCREENS_SEINX_PATH_RELEASE, [path, 0, 0], [0; 4]);
            }
            0
        }
        EE_SCREENS_SEIN_OP_MAKE_FILES => {
            // the scenarios' *.jpg, the movies' *.mpg, the sounds' *.mp3 (the paths: scenarios, sounds, movies)
            let k = match i[0] {
                EE_SCREENS_SEIN_FILES_MAP => 0,
                EE_SCREENS_SEIN_FILES_MOVIE => 2,
                _ => 1,
            };
            // SAFETY: p1 holds the three paths (the op's contract)
            let path = unsafe { *(p[1] as *const usize).add(k) };
            let form = p[2];
            let c = find_f(x, EE_SCREENS_SEINX_NEW_FILE_COMBO, path, form, [i[0], i[1], 0, 0], *f);
            let g = find(x, EE_SCREENS_SEINX_COMBO_GETEXT, c, 0, [0; 4]);
            st(x, EE_SCREENS_SEINX_YELLOW, [g, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEETX_TRANSPARENT, [g, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEINX_LIST_BLUE, [c, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEINX_BOX_OPACITY, [c, 0, 0], [0; 4]);
            st(x, EE_SCREENS_SEINX_ADD_CONTROL, [form, c, 0], [0; 4]);
            // SAFETY: p0 is the out word (the op's contract)
            unsafe { *(p[0] as *mut usize) = c };
            0
        }
        EE_SCREENS_SEIN_OP_BUTTON_EVENT => {
            let make = if i[0] == 0 { EE_SCREENS_SEINX_NEW_IMPORT } else { EE_SCREENS_SEINX_NEW_EXPORT };
            let e = find(x, make, 0, 0, [0; 4]);
            st(x, EE_SCREENS_SEINX_SET_BUTTON_UP, [p[0], e, 0], [0; 4]);
            st(x, EE_SCREENS_SEINX_EVENT_RELEASE, [e, 0, 0], [0; 4]);
            0
        }
        EE_SCREENS_SEIN_OP_LOAD => {
            // SAFETY: p0 holds the tab's controls (the op's contract)
            let c = |k: usize| unsafe { *(p[0] as *const usize).add(k) };
            let combos = [
                (EE_SCREENS_SEIN_C_MAP, EE_SCREENS_SEINX_M_MAP),
                (EE_SCREENS_SEIN_C_MOVIE, EE_SCREENS_SEINX_M_MOVIE),
                (EE_SCREENS_SEIN_C_SOUND, EE_SCREENS_SEINX_M_SOUND),
            ];
            for (control, media) in combos {
                let combo = c(control);
                st(x, EE_SCREENS_SEINX_TEXT_MEDIA, [0; 3], [media, 0, 0, 0]);
                if media == EE_SCREENS_SEINX_M_MOVIE {
                    // (the original selects the movie twice)
                    st(x, EE_SCREENS_SEINX_FILE_SELECT, [combo, 0, 0], [0; 4]);
                }
                if st(x, EE_SCREENS_SEINX_FILE_SELECT, [combo, 0, 0], [0; 4]) != 0
                    && st(x, EE_SCREENS_SEINX_TEXT_LENGTH, [0; 3], [0; 4]) != 0
                {
                    st(x, EE_SCREENS_SEINX_LABEL_TEXT, [combo, 0, 0], [0; 4]);
                }
            }
            let edits = [
                (EE_SCREENS_SEIN_C_INSTRUCTIONS, EE_SCREENS_SEINX_M_INSTRUCTIONS),
                (EE_SCREENS_SEIN_C_AFTERMATH, EE_SCREENS_SEINX_M_AFTERMATH),
                (EE_SCREENS_SEIN_C_HINTS, EE_SCREENS_SEINX_M_HINTS),
                (EE_SCREENS_SEIN_C_HISTORY, EE_SCREENS_SEINX_M_HISTORY),
            ];
            for (control, media) in edits {
                st(x, EE_SCREENS_SEINX_EDIT_MEDIA, [c(control), 0, 0], [media, 0, 0, 0]);
            }
            let local = c(EE_SCREENS_SEIN_C_LOCALIZATION);
            st(x, EE_SCREENS_SEINX_FILE_RESET, [0; 3], [0; 4]);
            st(x, EE_SCREENS_SEINX_TEXT_EXPORT, [0; 3], [0; 4]);
            if st(x, EE_SCREENS_SEINX_TEXT_LENGTH, [0; 3], [0; 4]) > 0 {
                st(x, EE_SCREENS_SEINX_FILE_VOLUME, [0; 3], [0; 4]);
                st(x, EE_SCREENS_SEINX_FILE_DIRECTORY, [0; 3], [0; 4]);
                st(x, EE_SCREENS_SEINX_FILE_NAME, [0; 3], [0; 4]);
                if st(x, EE_SCREENS_SEINX_FILE_OPEN, [0; 3], [0; 4]) != 0 {
                    st(x, EE_SCREENS_SEINX_NAME_STEM, [0; 3], [0; 4]);
                    st(x, EE_SCREENS_SEINX_EDIT_STEM, [local, 0, 0], [0; 4]);
                    st(x, EE_SCREENS_SEINX_FILE_CLOSE, [0; 3], [0; 4]);
                } else {
                    st(x, EE_SCREENS_SEINX_EDIT_EMPTY, [local, 0, 0], [0; 4]);
                }
            } else {
                st(x, EE_SCREENS_SEINX_EDIT_EMPTY, [local, 0, 0], [0; 4]);
            }
            0
        }
        EE_SCREENS_SEIN_OP_STORE => {
            // SAFETY: p0 holds the tab's controls (the op's contract)
            let c = |k: usize| unsafe { *(p[0] as *const usize).add(k) };
            for (control, media) in [
                (EE_SCREENS_SEIN_C_MAP, EE_SCREENS_SEINX_M_MAP),
                (EE_SCREENS_SEIN_C_MOVIE, EE_SCREENS_SEINX_M_MOVIE),
                (EE_SCREENS_SEIN_C_SOUND, EE_SCREENS_SEINX_M_SOUND),
            ] {
                st(x, EE_SCREENS_SEINX_SET_MEDIA_LABEL, [c(control), 0, 0], [media, 0, 0, 0]);
            }
            for (control, media) in [
                (EE_SCREENS_SEIN_C_INSTRUCTIONS, EE_SCREENS_SEINX_M_INSTRUCTIONS),
                (EE_SCREENS_SEIN_C_AFTERMATH, EE_SCREENS_SEINX_M_AFTERMATH),
                (EE_SCREENS_SEIN_C_HINTS, EE_SCREENS_SEINX_M_HINTS),
                (EE_SCREENS_SEIN_C_HISTORY, EE_SCREENS_SEINX_M_HISTORY),
            ] {
                st(x, EE_SCREENS_SEINX_EDIT_ALL_TEXT, [c(control), 0, 0], [0; 4]);
                st(x, EE_SCREENS_SEINX_SET_MEDIA, [0; 3], [media, 0, 0, 0]);
            }
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "editor_trigx_tests.rs"]
mod tests;
