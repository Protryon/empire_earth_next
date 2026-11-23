//! The editor's smaller hosts composed in Rust (the scenario editor lane's round 4): the map tab's
//! (`ee_screenscpp_semp`, EEUIFSEMap.cpp), the elevation tab's (`ee_screenscpp_seel`, EEUIFSEElevation.cpp), the
//! unit tab's upgrade panel (`EEUIFSEUpgradePanel`'s constructor and destructor, EEUIFSEUpgradePanel.cpp), the
//! editor's events' (`ee_screenscpp_se`, EESEEvents.cpp) and the form's events' (`ee_screenscpp_see`,
//! EEUIFSETRBPaintType.cpp). Each op that was a C++ statement sequence, or a call on a Rust object through a C++
//! accessor, is composed here in the C++'s order: of the form host's generic statements ([`editor_sefmx`]:
//! controls, texts, events, files) and each file's own statements (`EE_SCREENS_SEMPX_*` ... `EE_SCREENS_SEEX_*`),
//! the tabs' Rust objects called directly. `None`: the C++ host's own op.
//!
//! [`editor_sefmx`]: crate::screens::editor_sefmx

// (the casts of `long`s are no-ops on Windows and conversions on LP64)
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::screens::editor_calls as calls;
use crate::screens::editor_sefmx::*;

// ---- the map tab's statements (`ee_screenscpp_sempx`; p0 the object, p1 the form, p2 the tab)
/// `EETerrainTable::Get(i0) != NULL`, its `mIsInEditor`, `mID`, `mNameID`.
pub const EE_SCREENS_SEMPX_TERRAIN_EXISTS: u32 = 0;
pub const EE_SCREENS_SEMPX_TERRAIN_IN_EDITOR: u32 = 1;
pub const EE_SCREENS_SEMPX_TERRAIN_ID: u32 = 2;
pub const EE_SCREENS_SEMPX_TERRAIN_NAME: u32 = 3;
/// `EERMGetNumberOfGenerators()`, `SeplLinePush(EERMGetGeneratorDisplayName(i0))`; the rules' map sizes' count, size
/// i0's name into `gSefxText`; `SeplLinePush(gSefxText)`.
pub const EE_SCREENS_SEMPX_GENERATORS: u32 = 4;
pub const EE_SCREENS_SEMPX_PUSH_GENERATOR: u32 = 5;
pub const EE_SCREENS_SEMPX_MAP_SIZES: u32 = 6;
pub const EE_SCREENS_SEMPX_MAP_SIZE_NAME: u32 = 7;
pub const EE_SCREENS_SEMPX_PUSH_TEXT: u32 = 8;
/// `(unsigned long)` the form's manager's radio bitmap's height; the screen font's height.
pub const EE_SCREENS_SEMPX_RADIO_HEIGHT: u32 = 9;
pub const EE_SCREENS_SEMPX_FONT_HEIGHT: u32 = 10;
/// The kind radio (`new EEUIShellRadio(EE_SCREENS_SK_MAP_RADIO, {}, i0, gSefxText, the screen font, 100 x
/// (unsigned long)f2 at (f0, f1), form, i2, 0, false, i3 != 0, false)`); the radio p0's text label (a handle).
pub const EE_SCREENS_SEMPX_NEW_RADIO: u32 = 11;
pub const EE_SCREENS_SEMPX_RADIO_LABEL: u32 = 12;
/// The create button (`new EEUIShellButton(EE_SCREENS_SK_MAP_BUTTON, {p2}, 0x65, no bitmaps, 260 x the font's height
/// + 10 at (f0, f1), form, i2, 2)`); the button p0's `SetText(gSefxText, the screen font)`.
pub const EE_SCREENS_SEMPX_NEW_BUTTON: u32 = 13;
pub const EE_SCREENS_SEMPX_BUTTON_TEXT: u32 = 14;
/// The file spec p0's directory the elevation maps'; `new FSPath`, p0's `AddDirectory(*p2, true)`, its
/// `Dereference()`; the file combo box (`EEUIShellNewFileCombo(p0, "*.dat", kSEMapElevationFile, the screen font,
/// (f0, f1), 200, form, i2)`); its `SetStringTableID(IDS_ELEVATION_FILE_TO_USE_WITH_BLANK)`.
pub const EE_SCREENS_SEMPX_ELEVATION_DIRECTORY: u32 = 15;
pub const EE_SCREENS_SEMPX_NEW_PATH: u32 = 16;
pub const EE_SCREENS_SEMPX_PATH_ADD: u32 = 17;
pub const EE_SCREENS_SEMPX_PATH_RELEASE: u32 = 18;
pub const EE_SCREENS_SEMPX_NEW_FILE_COMBO: u32 = 19;
pub const EE_SCREENS_SEMPX_FILE_COMBO_HELP: u32 = 20;
/// `(new EESEARust(kSEABlankMap, NULL, i0, i1, i2))->Dereference()`, `(kSEARandomMap, NULL, i0, i1)`.
pub const EE_SCREENS_SEMPX_BLANK_MAP: u32 = 21;
pub const EE_SCREENS_SEMPX_RANDOM_MAP: u32 = 22;
/// `new URandomIntegers` (a handle), p0's `SetSeed(TMGetTime())`, `Generate(0, 1000000)`, `delete`;
/// `gUserInterface->mRandomSeed = i0`, its read.
pub const EE_SCREENS_SEMPX_RANDOM_SEED: u32 = 23;
pub const EE_SCREENS_SEMPX_RANDOM_DRAW: u32 = 24;
pub const EE_SCREENS_SEMPX_SEED_SET: u32 = 25;
pub const EE_SCREENS_SEMPX_SEED: u32 = 26;
pub const EE_SCREENS_SEMPX_RANDOM_NEW: u32 = 29;
pub const EE_SCREENS_SEMPX_RANDOM_DELETE: u32 = 30;
/// The control p0's selection changed event's `ProcessEvent(NULL, p0)`; `EEUIFileComboEntries(p0)`.
pub const EE_SCREENS_SEMPX_PROCESS_SELECTION: u32 = 27;
pub const EE_SCREENS_SEMPX_FILE_ENTRIES: u32 = 28;

// ---- the elevation tab's statements (`ee_screenscpp_seelx`; p0 the object, p1 the form)
/// The paint area (`new EEUIShellControl(EE_SCREENS_SK_PAINT_ELEVATION, {the two words at p2}, kSEElevationPaint,
/// 1024 x 606 at the top left, form, i0, 0)`).
pub const EE_SCREENS_SEELX_NEW_PAINT: u32 = 0;
/// The elevation slider (`new EEUIShellSlider(EE_SCREENS_SK_ELEVATION_SLIDER, {}, kSEElevationElevationSlider, 20 x
/// (unsigned long)f2 at (f0, f1), 1, form, i0, 2)`), the strength slider (`new UISlider(kSEElevationStrengthSlider,
/// 20 x f2 at (f0, f1), 1, form, i0, 2)`); the slider p0's `SetRange(0, i0)`, `GetMaxRange()`,
/// `SetCurrentRangeValue(i0)`, `GetCurrentRangeValue()`, `SetPageSize(1)`.
pub const EE_SCREENS_SEELX_NEW_ELEVATION_SLIDER: u32 = 1;
pub const EE_SCREENS_SEELX_NEW_STRENGTH_SLIDER: u32 = 2;
pub const EE_SCREENS_SEELX_SLIDER_RANGE: u32 = 3;
pub const EE_SCREENS_SEELX_SLIDER_MAX: u32 = 4;
pub const EE_SCREENS_SEELX_SLIDER_SET: u32 = 5;
pub const EE_SCREENS_SEELX_SLIDER_VALUE: u32 = 6;
pub const EE_SCREENS_SEELX_SLIDER_PAGE: u32 = 7;
/// `new UIESliderChange(p2, -4.0f, 0x28)` (an event).
pub const EE_SCREENS_SEELX_NEW_SLIDER_CHANGE: u32 = 8;
/// `gMouse->mPosition`'s x, y into f0; the form p0 is its manager's current form.
pub const EE_SCREENS_SEELX_MOUSE_X: u32 = 9;
pub const EE_SCREENS_SEELX_MOUSE_Y: u32 = 10;
pub const EE_SCREENS_SEELX_IS_CURRENT: u32 = 11;
/// The view rectangle holds (f0, f1); the editor p0's brush group's `mIsHighlightShown = i0 != 0`, its
/// `SetHighlight(f0, f1, i0 != 0)`; `EEPointUnitOrTile(GE2DPoint(f0, f1), sSeelxTile, sSeelxPoint, false)`, the
/// point's x, y into f0; `EEPointUnitOrTileXY(GE2DPoint(f0, f1), sSeelxTile, sSeelxX, sSeelxY, false)`, the tile's x,
/// y.
pub const EE_SCREENS_SEELX_VIEW_HOLDS: u32 = 12;
pub const EE_SCREENS_SEELX_HIGHLIGHT_SHOWN: u32 = 13;
pub const EE_SCREENS_SEELX_SET_HIGHLIGHT: u32 = 14;
pub const EE_SCREENS_SEELX_POINT: u32 = 15;
pub const EE_SCREENS_SEELX_POINT_X: u32 = 16;
pub const EE_SCREENS_SEELX_POINT_Y: u32 = 17;
pub const EE_SCREENS_SEELX_TILE: u32 = 18;
pub const EE_SCREENS_SEELX_TILE_X: u32 = 19;
pub const EE_SCREENS_SEELX_TILE_Y: u32 = 20;
/// The screen font's height.
pub const EE_SCREENS_SEELX_FONT_HEIGHT: u32 = 21;

// ---- the upgrade panel's statements (`ee_screenscpp_seupx`; p0 the box or a control, p1 the form or a control, p2
// the layout or a control; i0 the id, i1 the index, i2 a row; f a rectangle; made objects answer their handles)
/// `new EEUIFMGStatusBox(form)`; `new EEUICPortrait(form, GERectangle(f0, f1, f2, f3), i0, i1, false)`; the
/// portrait p0's right and top into f0.
pub const EE_SCREENS_SEUPX_NEW_BOX: u32 = 0;
pub const EE_SCREENS_SEUPX_NEW_PORTRAIT: u32 = 1;
pub const EE_SCREENS_SEUPX_PORTRAIT_RIGHT: u32 = 2;
pub const EE_SCREENS_SEUPX_PORTRAIT_TOP: u32 = 3;
/// `new UITextLabel("", gFontTahoma12, i0, GERectangle(GE2DPoint(f0, f1), f2, f3), form, 0, i1, 0)`; `new
/// UILayoutUtility`, p2's `SetLeft(0, f0)`, `SetRight(0, f0)`, `SetTop(3, f0)`, `SetBottom(3, f0)`, its
/// `CalculateRectangle(the form's rectangle, f as a `GERectangle`, false)`, `delete`.
pub const EE_SCREENS_SEUPX_NEW_NAME_LABEL: u32 = 4;
pub const EE_SCREENS_SEUPX_NEW_LAYOUT: u32 = 5;
pub const EE_SCREENS_SEUPX_LAYOUT_LEFT: u32 = 6;
pub const EE_SCREENS_SEUPX_LAYOUT_RIGHT: u32 = 7;
pub const EE_SCREENS_SEUPX_LAYOUT_TOP: u32 = 8;
pub const EE_SCREENS_SEUPX_LAYOUT_BOTTOM: u32 = 9;
pub const EE_SCREENS_SEUPX_LAYOUT_CALCULATE: u32 = 10;
pub const EE_SCREENS_SEUPX_LAYOUT_DELETE: u32 = 11;
/// A status bar (the left and the middle one): `new EEUIFMGStatusBar(i0, i1, p2, 0x400, 0x300, GERectangle(f0, f1,
/// f2, f3), form, true, true, false)`.
pub const EE_SCREENS_SEUPX_NEW_BAR: u32 = 12;
/// `new EEUIFMGAttribute(gFontTahoma10, form, GERectangle(f0, f1, f2, f3), i1, i0, black, true, 0)`; `new
/// UITextLabel("", gFontTahoma12, i0, GERectangle(GE2DPoint(f0, f1), f2, f3), form, 0, i1, 0)`; the box p0's
/// `SetControls(i2, p1, p2)`.
pub const EE_SCREENS_SEUPX_NEW_ATTRIBUTE: u32 = 14;
pub const EE_SCREENS_SEUPX_NEW_ROW_LABEL: u32 = 15;
pub const EE_SCREENS_SEUPX_BOX_ROW: u32 = 16;
/// `new UITextLabel(gSefxText, gFontTahoma12, i0, GERectangle(156, 634, 240, 650), form, 0, i1, 0)`.
pub const EE_SCREENS_SEUPX_NEW_ARMOR_LABEL: u32 = 17;
/// The box p0's portrait (`mC8C`), name label (`mC90`), left bar (`mC84`), middle bar (`mC88`), armor title (`mC98`)
/// set to p1.
pub const EE_SCREENS_SEUPX_BOX_PORTRAIT: u32 = 18;
pub const EE_SCREENS_SEUPX_BOX_NAME: u32 = 19;
pub const EE_SCREENS_SEUPX_BOX_LEFT_BAR: u32 = 20;
pub const EE_SCREENS_SEUPX_BOX_MIDDLE_BAR: u32 = 21;
pub const EE_SCREENS_SEUPX_BOX_ARMOR: u32 = 22;
/// `delete` p0: the status box, the portrait, a text label, a status bar, an attribute icon.
pub const EE_SCREENS_SEUPX_DELETE_BOX: u32 = 23;
pub const EE_SCREENS_SEUPX_DELETE_PORTRAIT: u32 = 24;
pub const EE_SCREENS_SEUPX_DELETE_LABEL: u32 = 25;
pub const EE_SCREENS_SEUPX_DELETE_BAR: u32 = 26;
pub const EE_SCREENS_SEUPX_DELETE_ATTRIBUTE: u32 = 29;
/// The box p0's `Fill(p1)` (`EEUIFSEUpgradePanel::ShowUnit`: the status box shows the unit).
pub const EE_SCREENS_SEUPX_FILL: u32 = 5000;

// ---- the editor events' statements (`ee_screenscpp_sex`; p0 the object, p1 the form)
/// `gWorld.mScenarioManager->mShowDebugIDs`, set to i0 != 0.
pub const EE_SCREENS_SEX_DEBUG_IDS: u32 = 0;
pub const EE_SCREENS_SEX_SET_DEBUG_IDS: u32 = 1;
/// `new EEACenterCamera` (an action), p0's `SetUnit(the form's selected unit i0)`, `SetLocation(i0, i1)`.
pub const EE_SCREENS_SEX_NEW_CENTER: u32 = 2;
pub const EE_SCREENS_SEX_CENTER_UNIT: u32 = 3;
pub const EE_SCREENS_SEX_CENTER_LOCATION: u32 = 4;
/// `FillPlaceComboBox(i0 != 0, the form's label 0x96d, its combo box 0x999)`; the combo box 0x999's
/// `SetCurrentSelection(0)`.
pub const EE_SCREENS_SEX_PLACE_FILL: u32 = 5;
pub const EE_SCREENS_SEX_PLACE_FIRST: u32 = 6;
/// `new EESEARust(kSEASave)`, `(kSEATest)`, `(kSEAExit)`.
pub const EE_SCREENS_SEX_NEW_SAVE_ACTION: u32 = 7;
pub const EE_SCREENS_SEX_NEW_TEST_ACTION: u32 = 8;
/// `gUIManager->DisplayYesNoCancel(gSefxText, 1, NULL, the save and exit answers)`.
pub const EE_SCREENS_SEX_ASK_EXIT: u32 = 9;
/// The tri-state check box p0's `GetId()`, `GetCheckState()`.
pub const EE_SCREENS_SEX_CHECK_ID: u32 = 10;
pub const EE_SCREENS_SEX_CHECK_STATE: u32 = 11;

// ---- the form events' statements (`ee_screenscpp_seex`; p0 the object, p1 a second)
/// p0's `Reference()` as an action; `new EESEERust(kSEEChangeViewport)` (an event), its `mEvent`;
/// `gUserInterface->PostEvent(p0, NULL)`.
pub const EE_SCREENS_SEEX_REFERENCE: u32 = 0;
pub const EE_SCREENS_SEEX_NEW_VIEWPORT: u32 = 1;
pub const EE_SCREENS_SEEX_EVENT_RUST: u32 = 2;
pub const EE_SCREENS_SEEX_POST_EVENT: u32 = 3;
/// `new EESEARust(kSEAQuickSave, p0)` (an action).
pub const EE_SCREENS_SEEX_NEW_QUICK_SAVE: u32 = 4;
/// `sprintf(gSefxText, p1, (double)f0)`; the label p0's `SetText(gSefxText)`.
pub const EE_SCREENS_SEEX_PRINT: u32 = 5;
pub const EE_SCREENS_SEEX_LABEL_TEXT: u32 = 6;
/// `FillScenarioComboBox(p0, p1, i0 != 0)`; `FillAreaObjectComboBox(the form p0's label i0, its combo box i1, i2 !=
/// 0)`.
pub const EE_SCREENS_SEEX_FILL_SCENARIO: u32 = 7;
pub const EE_SCREENS_SEEX_FILL_AREA_OBJECT: u32 = 8;
/// `sSeexNames.clear()`, epoch i0's title into `gSefxText`, `sSeexNames.push_back(gSefxText)`; the combo box p0's
/// `DeleteAllEntries()`, `AppendStringArray(sSeexNames)`.
pub const EE_SCREENS_SEEX_NAMES_CLEAR: u32 = 9;
pub const EE_SCREENS_SEEX_EPOCH_TITLE: u32 = 10;
pub const EE_SCREENS_SEEX_NAMES_PUSH: u32 = 11;
pub const EE_SCREENS_SEEX_COMBO_CLEAR: u32 = 12;
pub const EE_SCREENS_SEEX_COMBO_APPEND: u32 = 13;
/// `EEEpochCount()`.
pub const EE_SCREENS_SEEX_EPOCH_COUNT: u32 = 14;

/// `TABSX_IDS_CREATE_MAP`, `TABSX_IDS_SAVE_SCENARIO_BEFORE_EXITING`; the map tab's yellow; the sliders' grey;
/// `kControlTriggerSelectionChanged`, `kControlTriggerRangeChanged`; the trigger tab's sub-screens
/// (`kSETriggerScreen*`); the editor's tab groups (`kEditorTab*`).
const TABSX_IDS_CREATE_MAP: i32 = 20012;
const TABSX_IDS_SAVE_SCENARIO_BEFORE_EXITING: i32 = 10014;
const TABSX_YELLOW: [f32; 4] = [254.0 / 255.0, 247.0 / 255.0, 32.0 / 255.0, 0.0];
const TABSX_WHITE: [f32; 4] = [1.0, 1.0, 1.0, 0.0];
const TABSX_BLACK: [f32; 4] = [0.0, 0.0, 0.0, 0.0];
const TABSX_GREY: [f32; 4] = [0.2, 0.2, 0.2, 0.0];
const TABSX_BLUE: [f32; 4] = [0.0, 0.0, 1.0, 0.0];
const TABSX_TRIGGER_SELECTION: i32 = 0x3eb;
const TABSX_TRIGGER_RANGE: i32 = 0x3e8;
const TABSX_SCREEN_TRIGGER: u32 = 0;
const TABSX_SCREEN_CONDITION: u32 = 1;
const TABSX_SCREEN_EFFECT: u32 = 2;
const TABSX_SCREEN_OBJECT: u32 = 3;
const TABSX_SCREEN_AREA: u32 = 4;
const TABSX_TAB_UNITS: u32 = 3;
const TABSX_TAB_TRIGGERS: u32 = 4;
const TABSX_TAB_PLAYERS: u32 = 5;

/// A statement over copies of its numbers.
fn st(x: &mut Sefmx, op: u32, p: [usize; 3], a: [i32; 4], f: [f32; 4]) -> i32 {
    let (mut a, mut f) = (a, f);
    x(op, p, &mut a, &mut f)
}

/// A statement on p0 with p1 and no numbers.
fn on(x: &mut Sefmx, op: u32, c: usize, q: usize) -> i32 {
    st(x, op, [c, q, 0], [0; 4], [0.0; 4])
}

/// A statement's float answer (f0).
fn fl(x: &mut Sefmx, op: u32, p: [usize; 3], a: [i32; 4]) -> f32 {
    let mut f = [0.0; 4];
    x(op, p, &mut { a }, &mut f);
    f[0]
}

/// A making statement: the object's handle, from i0 (low 32 bits) and i1 (high).
fn mk(x: &mut Sefmx, op: u32, p: [usize; 3], a: [i32; 4], f: [f32; 4]) -> usize {
    let (mut a, mut f) = (a, f);
    x(op, p, &mut a, &mut f);
    (a[0] as u32 as usize) | ((a[1] as u32 as usize) << 32)
}

/// The out word p0.
///
/// # Safety
/// `out` is the op's out word.
unsafe fn put(out: usize, v: usize) {
    unsafe { *(out as *mut usize) = v };
}

/// The text of `t`: its colour then transparent (`SefmColor`'s order) or transparent first (`SempColor`'s).
fn color(x: &mut Sefmx, t: usize, c: [f32; 4], transparent_first: bool) {
    if transparent_first {
        on(x, EE_SCREENS_SEFMX_TEXT_TRANSPARENT, t, 0);
    }
    st(x, EE_SCREENS_SEFMX_TEXT_FG, [t, 0, 0], [0; 4], c);
    if !transparent_first {
        on(x, EE_SCREENS_SEFMX_TEXT_TRANSPARENT, t, 0);
    }
}

// ------------------------------------------------------------------------------------------------ the map tab

/// The map tab's op composed (`p0` the out word or the control, `p1` the form, `p2` the tab); `None` for the
/// C++'s.
///
/// # Safety
/// `p`, `i`, `f` are the op's arguments.
pub unsafe fn semp(
    x: &mut Sefmx,
    y: &mut Sefmx,
    op: u32,
    p: [usize; 3],
    i: &mut [i32; 4],
    f: &[f32; 4],
) -> Option<i32> {
    use crate::screens::editor_map::*;
    let (out, form, tab) = (p[0], p[1], p[2]);
    let ii = *i;
    let none = [0.0f32; 4];
    let at = |k: i32| [k, 0, 0, 0];
    Some(match op {
        EE_SCREENS_SEMP_OP_TERRAIN => {
            if st(y, EE_SCREENS_SEMPX_TERRAIN_EXISTS, [0; 3], at(ii[0]), none) == 0
                || st(y, EE_SCREENS_SEMPX_TERRAIN_IN_EDITOR, [0; 3], at(ii[0]), none) == 0
            {
                return Some(0);
            }
            i[1] = st(y, EE_SCREENS_SEMPX_TERRAIN_ID, [0; 3], at(ii[0]), none);
            i[2] = st(y, EE_SCREENS_SEMPX_TERRAIN_NAME, [0; 3], at(ii[0]), none);
            1
        }
        EE_SCREENS_SEMP_OP_LINES_GENERATORS => {
            let mut k = 0u32;
            while k < on(y, EE_SCREENS_SEMPX_GENERATORS, 0, 0) as u32 {
                st(y, EE_SCREENS_SEMPX_PUSH_GENERATOR, [0; 3], at(k as i32), none);
                k += 1;
            }
            0
        }
        EE_SCREENS_SEMP_OP_LINES_MAP_SIZES => {
            let mut k = 0u32;
            while k < on(y, EE_SCREENS_SEMPX_MAP_SIZES, 0, 0) as u32 {
                st(y, EE_SCREENS_SEMPX_MAP_SIZE_NAME, [0; 3], at(k as i32), none);
                on(y, EE_SCREENS_SEMPX_PUSH_TEXT, 0, 0);
                k += 1;
            }
            0
        }
        EE_SCREENS_SEMP_OP_RADIO_HEIGHT => {
            let h = on(y, EE_SCREENS_SEMPX_RADIO_HEIGHT, 0, form) as u32;
            let font = on(y, EE_SCREENS_SEMPX_FONT_HEIGHT, 0, 0) as u32;
            h.max(font) as i32
        }
        EE_SCREENS_SEMP_OP_MAKE_RADIO => unsafe {
            st(x, EE_SCREENS_SEFMX_STRING, [0; 3], at(ii[1]), none);
            let t = mk(y, EE_SCREENS_SEMPX_NEW_RADIO, [0, form, 0], ii, *f);
            let label = mk(y, EE_SCREENS_SEMPX_RADIO_LABEL, [t, form, 0], [0; 4], none);
            st(x, EE_SCREENS_SEFMX_BG_OPACITY, [label, form, 0], [0; 4], [0.4, 0.0, 0.0, 0.0]);
            let g = mk(x, EE_SCREENS_SEFMX_LABEL_GETEXT, [label, form, 0], [0; 4], none);
            color(x, g, TABSX_YELLOW, true);
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], at(1), none);
            put(out, t);
            0
        },
        EE_SCREENS_SEMP_OP_MAKE_BUTTON => unsafe {
            let t = mk(y, EE_SCREENS_SEMPX_NEW_BUTTON, [0, form, tab], ii, *f);
            st(x, EE_SCREENS_SEFMX_BG_OPACITY, [t, form, 0], [0; 4], [1.0, 0.0, 0.0, 0.0]);
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], at(1), none);
            put(out, t);
            0
        },
        EE_SCREENS_SEMP_OP_BUTTON_TEXT => {
            st(x, EE_SCREENS_SEFMX_STRING, [0; 3], at(TABSX_IDS_CREATE_MAP), none);
            on(y, EE_SCREENS_SEMPX_BUTTON_TEXT, out, form);
            let g = mk(x, EE_SCREENS_SEFMX_BUTTON_GETEXT, [out, form, 0], [0; 4], none);
            color(x, g, TABSX_WHITE, true);
            0
        }
        EE_SCREENS_SEMP_OP_MAKE_FILE_COMBO => unsafe {
            let spec = mk(x, EE_SCREENS_SEFMX_FILE_NEW, [0; 3], [0; 4], none);
            on(x, EE_SCREENS_SEFMX_FILE_VOLUME, spec, 0);
            on(y, EE_SCREENS_SEMPX_ELEVATION_DIRECTORY, spec, 0);
            let path = mk(y, EE_SCREENS_SEMPX_NEW_PATH, [0; 3], [0; 4], none);
            st(y, EE_SCREENS_SEMPX_PATH_ADD, [path, 0, spec], [0; 4], none);
            let t = mk(y, EE_SCREENS_SEMPX_NEW_FILE_COMBO, [path, form, 0], ii, *f);
            let g = mk(x, EE_SCREENS_SEFMX_COMBO_GETEXT, [t, form, 0], [0; 4], none);
            color(x, g, TABSX_YELLOW, false);
            st(x, EE_SCREENS_SEFMX_COMBO_SELECTED_BG, [t, form, 0], [0; 4], TABSX_BLUE);
            st(x, EE_SCREENS_SEFMX_COMBO_TEXTBOX_OPACITY, [t, form, 0], [0; 4], [0.4, 0.0, 0.0, 0.0]);
            let kind = crate::screens::editor_events::EE_SCREENS_SEE_MAP_TYPE as i32;
            let e = mk(x, EE_SCREENS_SEFMX_SEE_EVENT, [0, 0, tab], at(kind), none);
            st(x, EE_SCREENS_SEFMX_SET_TRIGGER, [t, form, e], at(TABSX_TRIGGER_SELECTION), none);
            on(x, EE_SCREENS_SEFMX_EVENT_RELEASE, e, form);
            on(y, EE_SCREENS_SEMPX_FILE_COMBO_HELP, t, form);
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], at(1), none);
            on(y, EE_SCREENS_SEMPX_PATH_RELEASE, path, 0);
            put(out, t);
            on(x, EE_SCREENS_SEFMX_FILE_DELETE, spec, 0);
            0
        },
        EE_SCREENS_SEMP_OP_BLANK_MAP => st(y, EE_SCREENS_SEMPX_BLANK_MAP, [0; 3], ii, none),
        EE_SCREENS_SEMP_OP_RANDOM_MAP => st(y, EE_SCREENS_SEMPX_RANDOM_MAP, [0; 3], ii, none),
        EE_SCREENS_SEMP_OP_SEED_DRAW => {
            let random = mk(y, EE_SCREENS_SEMPX_RANDOM_NEW, [0; 3], [0; 4], none);
            on(y, EE_SCREENS_SEMPX_RANDOM_SEED, random, 0);
            let seed = on(y, EE_SCREENS_SEMPX_RANDOM_DRAW, random, 0);
            st(y, EE_SCREENS_SEMPX_SEED_SET, [0; 3], at(seed), none);
            let r = on(y, EE_SCREENS_SEMPX_SEED, 0, 0);
            on(y, EE_SCREENS_SEMPX_RANDOM_DELETE, random, 0);
            r
        }
        EE_SCREENS_SEMP_OP_SEED_SET => st(y, EE_SCREENS_SEMPX_SEED_SET, [0; 3], ii, none),
        EE_SCREENS_SEMP_OP_PROCESS_SELECTION => on(y, EE_SCREENS_SEMPX_PROCESS_SELECTION, out, 0),
        EE_SCREENS_SEMP_OP_FILE_ENTRIES => on(y, EE_SCREENS_SEMPX_FILE_ENTRIES, out, 0),
        _ => return None,
    })
}

// ------------------------------------------------------------------------------------------ the elevation tab

/// The elevation tab's op composed (`p0` the out word, the control or the editor, `p1` the form, `p2` the op's
/// object); every op of the host.
///
/// # Safety
/// `p`, `i`, `f` are the op's arguments.
pub unsafe fn seel(x: &mut Sefmx, y: &mut Sefmx, op: u32, p: [usize; 3], i: &[i32; 4], f: &mut [f32; 4]) -> i32 {
    use crate::screens::editor_elevation::*;
    let (out, form, p2) = (p[0], p[1], p[2]);
    let ii = *i;
    let ff = *f;
    let none = [0.0f32; 4];
    let at = |k: i32| [k, 0, 0, 0];
    let slider = |x: &mut Sefmx, y: &mut Sefmx, t: usize, range: i32, below: i32| {
        st(y, EE_SCREENS_SEELX_SLIDER_RANGE, [t, form, 0], at(range), none);
        let max = on(y, EE_SCREENS_SEELX_SLIDER_MAX, t, form);
        st(
            y,
            EE_SCREENS_SEELX_SLIDER_SET,
            [t, form, 0],
            at((max as c_long).wrapping_sub(below as c_long) as i32),
            none,
        );
        on(y, EE_SCREENS_SEELX_SLIDER_PAGE, t, form);
        st(x, EE_SCREENS_SEFMX_BG_COLOR, [t, form, 0], [0; 4], TABSX_GREY);
        st(x, EE_SCREENS_SEFMX_BG_OPACITY, [t, form, 0], [0; 4], [0.4, 0.0, 0.0, 0.0]);
    };
    match op {
        EE_SCREENS_SEEL_OP_PAINT => unsafe {
            let t = mk(y, EE_SCREENS_SEELX_NEW_PAINT, [0, form, p2], at(ii[0]), none);
            st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], at(2), none);
            put(out, t);
            0
        },
        EE_SCREENS_SEEL_OP_ELEVATION_SLIDER => unsafe {
            let t = mk(y, EE_SCREENS_SEELX_NEW_ELEVATION_SLIDER, [0, form, 0], at(ii[0]), ff);
            slider(x, y, t, 0x40, 0x18);
            put(out, t);
            let max = on(y, EE_SCREENS_SEELX_SLIDER_MAX, t, form) as c_long;
            let value = on(y, EE_SCREENS_SEELX_SLIDER_VALUE, t, form) as c_long;
            max.wrapping_sub(value).wrapping_sub(0x18) as i32
        },
        EE_SCREENS_SEEL_OP_STRENGTH_SLIDER => unsafe {
            let t = mk(y, EE_SCREENS_SEELX_NEW_STRENGTH_SLIDER, [0, form, 0], at(ii[0]), ff);
            slider(x, y, t, 0x10, 6);
            let e = mk(y, EE_SCREENS_SEELX_NEW_SLIDER_CHANGE, [0, 0, p2], [0; 4], none);
            st(x, EE_SCREENS_SEFMX_SET_TRIGGER, [t, form, e], at(TABSX_TRIGGER_RANGE), none);
            on(x, EE_SCREENS_SEFMX_EVENT_RELEASE, e, form);
            put(out, t);
            0
        },
        EE_SCREENS_SEEL_OP_FONT_HEIGHT => on(y, EE_SCREENS_SEELX_FONT_HEIGHT, 0, 0),
        EE_SCREENS_SEEL_OP_ADD => {
            if ii[0] != 0 {
                on(x, EE_SCREENS_SEFMX_HIDE, out, form);
            } else {
                st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [out, form, 0], at(2), none);
            }
            0
        }
        EE_SCREENS_SEEL_OP_MOUSE => {
            f[0] = fl(y, EE_SCREENS_SEELX_MOUSE_X, [0; 3], [0; 4]);
            f[1] = fl(y, EE_SCREENS_SEELX_MOUSE_Y, [0; 3], [0; 4]);
            on(y, EE_SCREENS_SEELX_IS_CURRENT, out, 0)
        }
        EE_SCREENS_SEEL_OP_HIGHLIGHT => {
            let editor = out;
            let at_point = [ff[0], ff[1], 0.0, 0.0];
            if st(y, EE_SCREENS_SEELX_VIEW_HOLDS, [0; 3], [0; 4], at_point) == 0 {
                st(y, EE_SCREENS_SEELX_HIGHLIGHT_SHOWN, [editor, 0, 0], at(0), none);
            } else if ii[0] == crate::screens::editor_terrain::EE_SCREENS_SEEL_HIGHLIGHT_POINT {
                st(y, EE_SCREENS_SEELX_POINT, [0; 3], [0; 4], at_point);
                st(y, EE_SCREENS_SEELX_HIGHLIGHT_SHOWN, [editor, 0, 0], at(1), none);
                let px = fl(y, EE_SCREENS_SEELX_POINT_X, [0; 3], [0; 4]);
                let py = fl(y, EE_SCREENS_SEELX_POINT_Y, [0; 3], [0; 4]);
                st(y, EE_SCREENS_SEELX_SET_HIGHLIGHT, [editor, 0, 0], at((ii[1] != 0) as i32), [px, py, 0.0, 0.0]);
            } else {
                st(y, EE_SCREENS_SEELX_TILE, [0; 3], [0; 4], at_point);
                st(y, EE_SCREENS_SEELX_HIGHLIGHT_SHOWN, [editor, 0, 0], at(1), none);
                let tx = on(y, EE_SCREENS_SEELX_TILE_X, 0, 0) as c_long as f32;
                let ty = on(y, EE_SCREENS_SEELX_TILE_Y, 0, 0) as c_long as f32;
                st(y, EE_SCREENS_SEELX_SET_HIGHLIGHT, [editor, 0, 0], at(0), [tx, ty, 0.0, 0.0]);
            }
            0
        }
        _ => 0,
    }
}

// --------------------------------------------------------------------------------------------- the upgrade panel

/// The unit tab's upgrade panel (once the C++ class `EEUIFSEUpgradePanel`, EESEUnit +0xb4, new 0x60; ctor 0x1a4e99,
/// dtor 0x1a549d, ShowUnit 0x1a551a): a status box of the main game's kind over the editor form with its portrait,
/// unit name, two status bars, nine attribute icons and their labels and the armor title (the handles; the box's
/// own +0x8c, +0x90, +0x84, +0x88, +0x98 and rows point at them).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SeupPanel {
    pub status_box: usize,
    pub portrait: usize,
    pub name: usize,
    pub left_bar: usize,
    pub middle_bar: usize,
    pub armor: usize,
    pub attributes: [usize; 9],
    pub labels: [usize; 9],
}

/// The panel's constructor: the status box, the portrait, the unit name, the two status bars, the nine attribute
/// rows and the armor title, each hidden and wired into the box, the id and index advanced past each.
///
/// # Safety
/// `id` and `index` are the caller's counters.
pub unsafe fn upgrade_panel(
    x: &mut Sefmx,
    y: &mut Sefmx,
    panel: &mut SeupPanel,
    id: *mut c_ulong,
    index: *mut c_ulong,
    form: usize,
) {
    let none = [0.0f32; 4];
    let ids = || unsafe { [*id as i32, *index as i32, 0, 0] };
    let next = || unsafe {
        *id = (*id).wrapping_add(1);
        *index = (*index).wrapping_add(1);
    };
    let pf = [0, form, 0];
    // a control added to the panel's group 6 and hidden
    let show = |x: &mut Sefmx, t: usize| {
        st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [6, 0, 0, 0], none);
        on(x, EE_SCREENS_SEFMX_HIDE, t, form);
    };
    let black = |x: &mut Sefmx, t: usize| {
        let g = mk(x, EE_SCREENS_SEFMX_LABEL_GETEXT, [t, form, 0], [0; 4], none);
        color(x, g, TABSX_BLACK, false);
    };
    panel.status_box = mk(y, EE_SCREENS_SEUPX_NEW_BOX, pf, [0; 4], none);
    let b = panel.status_box;
    // (left, top, right, bottom)
    let mut r = [6.0f32, 619.0, 36.0, 649.0];
    let t = mk(y, EE_SCREENS_SEUPX_NEW_PORTRAIT, pf, ids(), r);
    panel.portrait = t;
    st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [6, 0, 0, 0], none);
    on(y, EE_SCREENS_SEUPX_BOX_PORTRAIT, b, t);
    on(x, EE_SCREENS_SEFMX_HIDE, t, form);
    next();
    let mut pos = [
        fl(y, EE_SCREENS_SEUPX_PORTRAIT_RIGHT, [t, form, 0], [0; 4]) + 4.0,
        fl(y, EE_SCREENS_SEUPX_PORTRAIT_TOP, [t, form, 0], [0; 4]),
    ];
    let t = mk(y, EE_SCREENS_SEUPX_NEW_NAME_LABEL, pf, ids(), [pos[0], pos[1], 200.0, 15.0]);
    panel.name = t;
    black(x, t);
    st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [6, 0, 0, 0], none);
    on(y, EE_SCREENS_SEUPX_BOX_NAME, b, t);
    on(x, EE_SCREENS_SEFMX_HIDE, t, form);
    next();
    let layout = mk(y, EE_SCREENS_SEUPX_NEW_LAYOUT, [0; 3], [0; 4], none);
    for (top, bottom, left_bar) in [(111.0f32, 76.0f32, true), (74.0, 39.0, false)] {
        let lp = [0, form, layout];
        st(y, EE_SCREENS_SEUPX_LAYOUT_LEFT, lp, [0; 4], [6.0, 0.0, 0.0, 0.0]);
        st(y, EE_SCREENS_SEUPX_LAYOUT_RIGHT, lp, [0; 4], [57.0, 0.0, 0.0, 0.0]);
        st(y, EE_SCREENS_SEUPX_LAYOUT_TOP, lp, [0; 4], [top, 0.0, 0.0, 0.0]);
        st(y, EE_SCREENS_SEUPX_LAYOUT_BOTTOM, lp, [0; 4], [bottom, 0.0, 0.0, 0.0]);
        // (the rectangle in `GERectangle`'s member order: bottom, left, right, top)
        let mut g = [0.0f32; 4];
        y(EE_SCREENS_SEUPX_LAYOUT_CALCULATE, lp, &mut [0; 4], &mut g);
        r = [g[1], g[3], g[2], g[0]];
        let t = mk(y, EE_SCREENS_SEUPX_NEW_BAR, [0, form, layout], ids(), r);
        st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [6, 0, 0, 0], none);
        if left_bar {
            panel.left_bar = t;
            on(y, EE_SCREENS_SEUPX_BOX_LEFT_BAR, b, t);
        } else {
            panel.middle_bar = t;
            on(y, EE_SCREENS_SEUPX_BOX_MIDDLE_BAR, b, t);
        }
        on(x, EE_SCREENS_SEFMX_HIDE, t, form);
        next();
    }
    for k in 0..9i32 {
        let first = k < 5;
        let left = if first { 62.0f32 } else { 156.0 };
        pos[1] = if first { k as f32 * 20.0 + 634.0 } else { (k - 5) as f32 * 20.0 + 654.0 };
        let width: c_long = if first { 57 } else { 48 };
        r[1] = pos[1];
        r[0] = left;
        r[2] = r[0] + width as f32 + 19.0;
        r[3] = r[1] + 17.0;
        let row = |a: [i32; 4]| [a[0], a[1], k, 0];
        let icon = mk(y, EE_SCREENS_SEUPX_NEW_ATTRIBUTE, pf, row(ids()), r);
        panel.attributes[k as usize] = icon;
        show(x, icon);
        next();
        pos[0] = r[0] + 19.0;
        let t = mk(y, EE_SCREENS_SEUPX_NEW_ROW_LABEL, pf, row(ids()), [pos[0], pos[1], width as f32, 17.0]);
        panel.labels[k as usize] = t;
        black(x, t);
        st(x, EE_SCREENS_SEFMX_LABEL_ALIGN, [t, form, 0], [2, 0, 0, 0], none);
        show(x, t);
        next();
        st(y, EE_SCREENS_SEUPX_BOX_ROW, [b, icon, t], [0, 0, k, 0], none);
    }
    st(x, EE_SCREENS_SEFMX_STRING, [0; 3], [TABSX_IDS_GAME_ARMOR, 0, 0, 0], none);
    let t = mk(y, EE_SCREENS_SEUPX_NEW_ARMOR_LABEL, pf, ids(), none);
    panel.armor = t;
    st(x, EE_SCREENS_SEFMX_LABEL_ALIGN, [t, form, 0], [1, 0, 0, 0], none);
    black(x, t);
    st(x, EE_SCREENS_SEFMX_ADD_CONTROL, [t, form, 0], [6, 0, 0, 0], none);
    on(y, EE_SCREENS_SEUPX_BOX_ARMOR, b, t);
    on(x, EE_SCREENS_SEFMX_HIDE, t, form);
    next();
    on(y, EE_SCREENS_SEUPX_LAYOUT_DELETE, 0, layout);
}

/// `TABSX_IDS_GAME_ARMOR`.
const TABSX_IDS_GAME_ARMOR: i32 = 30423;

/// The panel's destructor: the box and every control deleted (the original's order).
pub fn upgrade_panel_free(y: &mut Sefmx, panel: &SeupPanel) {
    for (op, c) in [
        (EE_SCREENS_SEUPX_DELETE_BOX, panel.status_box),
        (EE_SCREENS_SEUPX_DELETE_PORTRAIT, panel.portrait),
        (EE_SCREENS_SEUPX_DELETE_LABEL, panel.armor),
        (EE_SCREENS_SEUPX_DELETE_BAR, panel.left_bar),
        (EE_SCREENS_SEUPX_DELETE_BAR, panel.middle_bar),
        (EE_SCREENS_SEUPX_DELETE_LABEL, panel.name),
    ] {
        on(y, op, c, 0);
    }
    for k in 0..9 {
        on(y, EE_SCREENS_SEUPX_DELETE_ATTRIBUTE, panel.attributes[k], 0);
        on(y, EE_SCREENS_SEUPX_DELETE_LABEL, panel.labels[k], 0);
    }
}

/// `ShowUnit(unit)`: the status box filled with the unit's details.
pub fn upgrade_panel_show(y: &mut Sefmx, panel: &SeupPanel, unit: usize) {
    on(y, EE_SCREENS_SEUPX_FILL, panel.status_box, unit);
}

// ------------------------------------------------------------------------------------------- the editor's events

thread_local! {
    /// `EE_SCREENS_SE_OP_AREA_CENTER`'s centre, for `EE_SCREENS_SE_OP_CENTER_ON_AREA` (the C++'s `sAreaCenter`).
    static AREA_CENTER: core::cell::Cell<[f32; 2]> = const { core::cell::Cell::new([0.0; 2]) };
}

/// The editor form's Rust object (`mRust`) of the C++ form `form`.
fn form_rust(x: &mut Sefmx, form: usize) -> usize {
    mk(x, EE_SCREENS_SEFMX_FORM_RUST, [0, form, 0], [0; 4], [0.0; 4])
}

/// The tab group `tab`'s Rust tab of the form (`GetTab`).
fn tab_of(x: &mut Sefmx, form: usize, tab: u32) -> usize {
    use crate::screens::editor_form::EE_SCREENS_SEFM_GROUP;
    let rust = form_rust(x, form);
    calls::segr_tab(calls::sefm_get(rust, EE_SCREENS_SEFM_GROUP + tab))
}

/// The editor's event op composed (`form` the editor, `entity` the event's control); `None` for the C++'s.
///
/// # Safety
/// `form` is the live editor form, `entity` the event's control.
pub unsafe fn se(x: &mut Sefmx, y: &mut Sefmx, form: usize, entity: usize, op: u32, a: u32, b: u32) -> Option<u32> {
    use crate::screens::editor_calls::*;
    use crate::screens::editor_form::*;
    use crate::screens::scenario_editor::*;
    let none = [0.0f32; 4];
    let triggers = |x: &mut Sefmx| tab_of(x, form, TABSX_TAB_TRIGGERS);
    let sub = |x: &mut Sefmx, screen: u32, what: u32, a: u32, b: u32| sets_sub_op(triggers(x), screen, what, a, b);
    Some({
        match op {
            EE_SCREENS_SE_OP_TAB => {
                let what = match a {
                    EE_SCREENS_SE_TAB_SHOW => EE_SCREENS_SETS_SHOW_TAB,
                    EE_SCREENS_SE_TAB_SELECT_ENTRY => EE_SCREENS_SETS_SELECT_ENTRY,
                    EE_SCREENS_SE_TAB_NEW_ENTRY => EE_SCREENS_SETS_NEW_ENTRY,
                    EE_SCREENS_SE_TAB_DUPLICATE_ENTRY => EE_SCREENS_SETS_DUPLICATE_ENTRY,
                    EE_SCREENS_SE_TAB_DELETE_ENTRY => EE_SCREENS_SETS_DELETE_ENTRY,
                    EE_SCREENS_SE_TAB_INC_PRIORITY => EE_SCREENS_SETS_INC_PRIORITY,
                    EE_SCREENS_SE_TAB_DEC_PRIORITY => EE_SCREENS_SETS_DEC_PRIORITY,
                    EE_SCREENS_SE_TAB_ALPHA_SORT => EE_SCREENS_SETS_ALPHA_SORT,
                    _ => EE_SCREENS_SETS_SELECT_NAME,
                };
                let arg = if a == EE_SCREENS_SE_TAB_SHOW { b } else { 0 };
                sets_call(triggers(x), what, arg);
                0
            }
            EE_SCREENS_SE_OP_TOGGLE_DEBUG_IDS => {
                let shown = on(y, EE_SCREENS_SEX_DEBUG_IDS, 0, 0) != 0;
                st(y, EE_SCREENS_SEX_SET_DEBUG_IDS, [0; 3], [(!shown) as i32, 0, 0, 0], none);
                0
            }
            EE_SCREENS_SE_OP_OBJECT_SET_MAP => {
                use crate::screens::editor_object::EE_SCREENS_SEOB_SET_SELECT_ON_MAP;
                sub(x, TABSX_SCREEN_OBJECT, EE_SCREENS_SEOB_SET_SELECT_ON_MAP, (a != 0) as u32, 0);
                0
            }
            EE_SCREENS_SE_OP_OBJECT_REFRESH => {
                sub(x, TABSX_SCREEN_OBJECT, crate::screens::editor_object::EE_SCREENS_SEOB_REFRESH, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_OBJECT_IS_MAP => {
                (sub(x, TABSX_SCREEN_OBJECT, crate::screens::editor_object::EE_SCREENS_SEOB_IS_SELECT_ON_MAP, 0, 0)
                    != 0) as u32
            }
            EE_SCREENS_SE_OP_MANAGER_CLEAR_SELECTION => {
                let editor = mk(x, EE_SCREENS_SEFMX_MANAGER_EDITOR, [0; 3], [0; 4], none);
                sefm_call(form_rust(x, editor), EE_SCREENS_SEFM_CLEAR_SELECTION, 0, 0, core::ptr::null());
                0
            }
            EE_SCREENS_SE_OP_OBJECT_CLEAR_UNIT_LIST => {
                sub(x, TABSX_SCREEN_OBJECT, crate::screens::editor_object::EE_SCREENS_SEOB_CLEAR_UNIT_LIST, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_FORM_CLEAR_SELECTION => {
                sefm_call(form_rust(x, form), EE_SCREENS_SEFM_CLEAR_SELECTION, 0, 0, core::ptr::null());
                0
            }
            EE_SCREENS_SE_OP_OBJECT_CLEAR_IDS => {
                sub(x, TABSX_SCREEN_OBJECT, crate::screens::editor_object::EE_SCREENS_SEOB_CLEAR_UNIT_IDS, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_OBJECT_UPDATE_SELECTION => {
                sub(x, TABSX_SCREEN_OBJECT, crate::screens::editor_object::EE_SCREENS_SEOB_UPDATE_SELECTION, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_OBJECT_CHECK_CHANGED => {
                let id = on(y, EE_SCREENS_SEX_CHECK_ID, entity, 0) as u32;
                let state = on(y, EE_SCREENS_SEX_CHECK_STATE, entity, 0) as u32;
                sub(x, TABSX_SCREEN_OBJECT, crate::screens::editor_object::EE_SCREENS_SEOB_CHECK_CHANGED, id, state);
                0
            }
            EE_SCREENS_SE_OP_SELECTED_UNIT_COUNT => sefm_selected(form_rust(x, form), -1),
            EE_SCREENS_SE_OP_CENTER_ON_UNIT => {
                let action = mk(y, EE_SCREENS_SEX_NEW_CENTER, [0; 3], [0; 4], none);
                st(y, EE_SCREENS_SEX_CENTER_UNIT, [action, form, 0], [a as i32, 0, 0, 0], none);
                on(x, EE_SCREENS_SEFMX_DEREF_ACTION, action, 0);
                0
            }
            EE_SCREENS_SE_OP_AREA_MODE => {
                sub(x, TABSX_SCREEN_AREA, crate::screens::editor_area::EE_SCREENS_SEAR_SET_MODE, (a != 0) as u32, 0);
                0
            }
            EE_SCREENS_SE_OP_AREA_CENTER => {
                let screen = sets_sub(triggers(x), TABSX_SCREEN_AREA);
                let mut c = [0.0f32; 2];
                if !sear_center(screen, &mut c) {
                    return Some(0);
                }
                AREA_CENTER.with(|k| k.set(c));
                1
            }
            EE_SCREENS_SE_OP_CENTER_ON_AREA => {
                let c = AREA_CENTER.with(|k| k.get());
                let action = mk(y, EE_SCREENS_SEX_NEW_CENTER, [0; 3], [0; 4], none);
                let at = [c[0] as c_long as i32, c[1] as c_long as i32, 0, 0];
                st(y, EE_SCREENS_SEX_CENTER_LOCATION, [action, 0, 0], at, none);
                on(x, EE_SCREENS_SEFMX_DEREF_ACTION, action, 0);
                0
            }
            EE_SCREENS_SE_OP_CONDITION_TYPE => {
                sub(x, TABSX_SCREEN_CONDITION, crate::screens::editor_subscreens::EE_SCREENS_SESC_CHANGE_TYPE, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_EFFECT_TYPE => {
                sub(x, TABSX_SCREEN_EFFECT, crate::screens::editor_subscreens::EE_SCREENS_SESC_CHANGE_TYPE, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_EFFECT_ACTION => {
                sub(x, TABSX_SCREEN_EFFECT, crate::screens::editor_subscreens::EE_SCREENS_SESC_CHANGE_ACTION, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_TRIGGER_OPERATOR => {
                sub(x, TABSX_SCREEN_TRIGGER, crate::screens::editor_trigger::EE_SCREENS_SETR_CHANGE_OPERATOR, a, 0);
                0
            }
            EE_SCREENS_SE_OP_TRIGGER_STEP => {
                sub(x, TABSX_SCREEN_TRIGGER, crate::screens::editor_trigger::EE_SCREENS_SETR_STEP, a.min(3), 0);
                0
            }
            EE_SCREENS_SE_OP_TRIGGER_REMOVE => {
                sub(x, TABSX_SCREEN_TRIGGER, crate::screens::editor_trigger::EE_SCREENS_SETR_REMOVE_ENTRY, a, 0);
                0
            }
            EE_SCREENS_SE_OP_UNIT_RESELECT => {
                seun_call(tab_of(x, form, TABSX_TAB_UNITS), EE_SCREENS_SEUN_RESELECT, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_PLAYER_FILL_TECH | EE_SCREENS_SE_OP_PLAYER_REBUILD_TECH => {
                let what = if op == EE_SCREENS_SE_OP_PLAYER_FILL_TECH {
                    EE_SCREENS_SEPL_FILL_TECH_LISTS
                } else {
                    EE_SCREENS_SEPL_REBUILD_TECH_LISTS
                };
                sepl_call(tab_of(x, form, TABSX_TAB_PLAYERS), what);
                0
            }
            EE_SCREENS_SE_OP_HOME_ROTATION => {
                on(x, EE_SCREENS_SEFMX_TURN_RESET, 0, 0);
                on(x, EE_SCREENS_SEFMX_TURN_VIEW, 0, 0);
                st(
                    x,
                    EE_SCREENS_SEFMX_TURN_YPR,
                    [0; 3],
                    [0; 4],
                    [core::f32::consts::FRAC_PI_4, -0.610_865_24, 0.0, 0.0],
                );
                on(x, EE_SCREENS_SEFMX_CAMERA_OFFSET_TURN, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_LOAD_TRIGGERS => {
                load_triggers(x);
                0
            }
            EE_SCREENS_SE_OP_PLACE_FILL => {
                st(y, EE_SCREENS_SEX_PLACE_FILL, [0, form, 0], [(a != 0) as i32, 0, 0, 0], none);
                on(y, EE_SCREENS_SEX_PLACE_FIRST, 0, form);
                0
            }
            EE_SCREENS_SE_OP_POST_LOAD_EVENT => {
                let e = mk(x, EE_SCREENS_SEFMX_SCREEN_EVENT, [0; 3], [a as i32, 0, 0, 0], none);
                sefm_call(form_rust(x, form), EE_SCREENS_SEFM_POST_LOAD_EVENT, 0, e, core::ptr::null());
                on(x, EE_SCREENS_SEFMX_EVENT_RELEASE, e, 0);
                0
            }
            EE_SCREENS_SE_OP_POST_ACTION => {
                let action = match a {
                    EE_SCREENS_SE_ACTION_SAVE => mk(y, EE_SCREENS_SEX_NEW_SAVE_ACTION, [0; 3], [0; 4], none),
                    EE_SCREENS_SE_ACTION_TEST => mk(y, EE_SCREENS_SEX_NEW_TEST_ACTION, [0; 3], [0; 4], none),
                    _ => mk(x, EE_SCREENS_SEFMX_NEW_EXIT_ACTION, [0; 3], [0; 4], none),
                };
                on(x, EE_SCREENS_SEFMX_POST_ACTION, action, 0);
                on(x, EE_SCREENS_SEFMX_DEREF_ACTION, action, 0);
                0
            }
            EE_SCREENS_SE_OP_ASK_EXIT => {
                st(x, EE_SCREENS_SEFMX_STRING, [0; 3], [TABSX_IDS_SAVE_SCENARIO_BEFORE_EXITING, 0, 0, 0], none);
                on(y, EE_SCREENS_SEX_ASK_EXIT, 0, 0);
                0
            }
            EE_SCREENS_SE_OP_EXIT_AFTER_SAVE => {
                let editor = mk(x, EE_SCREENS_SEFMX_MANAGER_EDITOR, [0; 3], [0; 4], none);
                sefm_set(form_rust(x, editor), EE_SCREENS_SEFM_EXIT_AFTER_SAVE, 1);
                0
            }
            _ => return None,
        }
    })
}

// ------------------------------------------------------------------------------------------ the form's events

/// The form events' op composed (`p0` and `p1` the op's pointers); `None` for the C++'s.
///
/// # Safety
/// `p`, `i`, `f` are the op's arguments.
pub unsafe fn see(x: &mut Sefmx, y: &mut Sefmx, op: u32, p: [usize; 3], i: &[i32; 4], f: &[f32; 4]) -> Option<i32> {
    use crate::screens::editor_calls::*;
    use crate::screens::editor_events::*;
    use crate::screens::editor_form::*;
    let (c, q) = (p[0], p[1]);
    let ii = *i;
    let none = [0.0f32; 4];
    Some({
        match op {
            EE_SCREENS_SEE_OP_REFERENCE => on(y, EE_SCREENS_SEEX_REFERENCE, c, 0),
            EE_SCREENS_SEE_OP_DEREFERENCE => on(x, EE_SCREENS_SEFMX_DEREF_ACTION, c, 0),
            EE_SCREENS_SEE_OP_POST_VIEWPORT => {
                let e = mk(y, EE_SCREENS_SEEX_NEW_VIEWPORT, [0; 3], [0; 4], none);
                let rust = mk(y, EE_SCREENS_SEEX_EVENT_RUST, [e, 0, 0], [0; 4], none);
                see_viewport(rust, f, ii[0] != 0);
                let r = on(y, EE_SCREENS_SEEX_POST_EVENT, e, 0);
                on(x, EE_SCREENS_SEFMX_EVENT_RELEASE, e, 0);
                r
            }
            EE_SCREENS_SEE_OP_SET_GAME_VIEW => {
                let editor = mk(x, EE_SCREENS_SEFMX_MANAGER_EDITOR, [0; 3], [0; 4], none);
                sefm_call(form_rust(x, editor), EE_SCREENS_SEFM_SET_GAME_VIEW, (ii[0] != 0) as i32, 0, f.as_ptr());
                0
            }
            EE_SCREENS_SEE_OP_REDRAW_ALL => on(x, EE_SCREENS_SEFMX_REDRAW_ALL, 0, 0),
            EE_SCREENS_SEE_OP_SHOW_LOADED => {
                let editor = mk(x, EE_SCREENS_SEFMX_MANAGER_EDITOR, [0; 3], [0; 4], none);
                sefm_call(form_rust(x, editor), EE_SCREENS_SEFM_POST_SHOW_LOADED, 0, c, core::ptr::null());
                0
            }
            EE_SCREENS_SEE_OP_QUICK_SAVE => {
                let action = mk(y, EE_SCREENS_SEEX_NEW_QUICK_SAVE, [c, 0, 0], [0; 4], none);
                on(x, EE_SCREENS_SEFMX_POST_ACTION, action, 0);
                on(x, EE_SCREENS_SEFMX_DEREF_ACTION, action, 0);
                0
            }
            EE_SCREENS_SEE_OP_SLIDER_TEXT => {
                st(y, EE_SCREENS_SEEX_PRINT, [0, q, 0], [0; 4], *f);
                on(y, EE_SCREENS_SEEX_LABEL_TEXT, c, 0);
                0
            }
            EE_SCREENS_SEE_OP_FILL_SCENARIO => st(y, EE_SCREENS_SEEX_FILL_SCENARIO, [c, q, 0], ii, none),
            EE_SCREENS_SEE_OP_CAMG_ADD | EE_SCREENS_SEE_OP_CAMG_REMOVE => {
                let add = (op == EE_SCREENS_SEE_OP_CAMG_ADD) as u32;
                seet_event(c, q, 0, add, ii[0] as u32);
                0
            }
            EE_SCREENS_SEE_OP_MSD_SHOW => {
                seet_event(c, q, 1, 0, 0);
                0
            }
            EE_SCREENS_SEE_OP_SAA_SHOW => {
                seet_event(c, q, 2, 0, ii[0] as u32);
                0
            }
            EE_SCREENS_SEE_OP_FILL_AREA_OBJECT => st(y, EE_SCREENS_SEEX_FILL_AREA_OBJECT, [c, 0, 0], ii, none),
            EE_SCREENS_SEE_OP_MAP_ELEVATION => {
                use crate::screens::editor_map::EE_SCREENS_SEMP_SHOW_ELEVATION;
                semp_call(segr_tab(c), EE_SCREENS_SEMP_SHOW_ELEVATION, (ii[0] != 0) as i32);
                0
            }
            EE_SCREENS_SEE_OP_REFRESH_UNIT_LIST => {
                seun_call(tab_of(x, c, TABSX_TAB_UNITS), EE_SCREENS_SEUN_REFRESH, 0, 0);
                0
            }
            EE_SCREENS_SEE_OP_FILL_EPOCHS => {
                on(y, EE_SCREENS_SEEX_NAMES_CLEAR, 0, 0);
                let count = on(y, EE_SCREENS_SEEX_EPOCH_COUNT, 0, 0);
                let mut k = ii[0] as u32;
                while k < count as u32 {
                    st(y, EE_SCREENS_SEEX_EPOCH_TITLE, [0; 3], [k as i32, 0, 0, 0], none);
                    on(y, EE_SCREENS_SEEX_NAMES_PUSH, 0, 0);
                    k += 1;
                }
                on(y, EE_SCREENS_SEEX_COMBO_CLEAR, c, 0);
                on(y, EE_SCREENS_SEEX_COMBO_APPEND, c, 0);
                0
            }
            _ => return None,
        }
    })
}

// ---- the C ABI

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sempx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seelx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seupx(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sex(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_seex(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

macro_rules! statements {
    ($name:ident, $sys:ident) => {
        /// The file's C++ statement host.
        pub fn $name(op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            let p = p.map(|v| v as *mut c_void);
            // SAFETY: the statement's contract (the composer passes each statement's objects)
            unsafe { sys::$sys(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr()) }
        }
    };
}
statements!(sempx, ee_screenscpp_sempx);
statements!(seelx, ee_screenscpp_seelx);
statements!(seupx, ee_screenscpp_seupx);
statements!(sex, ee_screenscpp_sex);
statements!(seex, ee_screenscpp_seex);

/// The elevation tab's host (also the terrain tab's ops from 900): every op composed here.
///
/// # Safety
/// `p` points at the op's three handles, `i` and `f` at four values each.
#[cfg(not(test))]
pub(crate) unsafe fn seel_host(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32 {
    unsafe {
        let q = [*p as usize, *p.add(1) as usize, *p.add(2) as usize];
        seel(&mut cpp, &mut seelx, op, q, &*(i as *const [i32; 4]), &mut *(f as *mut [f32; 4]))
    }
}

/// The unit tab's upgrade panel made (once `new EEUIFSEUpgradePanel(id, index, form)`): its handle, freed by
/// `ee_screens_seup_free`.
///
/// # Safety
/// `id` and `index` are the caller's counters, `form` the live editor form.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seup_new(id: *mut c_ulong, index: *mut c_ulong, form: *mut c_void) -> *mut c_void {
    let mut panel = SeupPanel::default();
    unsafe { upgrade_panel(&mut cpp, &mut seupx, &mut panel, id, index, form as usize) };
    Box::into_raw(Box::new(panel)).cast()
}

/// The panel's `ShowUnit(unit)`.
///
/// # Safety
/// `panel` comes from `ee_screens_seup_new`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seup_show(panel: *mut c_void, unit: *mut c_void) {
    // a copy: no Rust reference into the box across the C++ call
    let p = unsafe { (*(panel as *const SeupPanel)).clone() };
    upgrade_panel_show(&mut seupx, &p, unit as usize);
}

/// The panel destroyed (once `delete` of the `EEUIFSEUpgradePanel`): its box and controls deleted.
///
/// # Safety
/// `panel` comes from `ee_screens_seup_new` and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_seup_free(panel: *mut c_void) {
    let p = unsafe { Box::from_raw(panel as *mut SeupPanel) };
    upgrade_panel_free(&mut seupx, &p);
}

#[cfg(test)]
#[path = "editor_tabsx_tests.rs"]
mod tests;
