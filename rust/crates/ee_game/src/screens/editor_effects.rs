//! The scenario editor's effect types whole in Rust (`EESETSEffectType` and every `EESETSET*` class, once
//! `EESETSETAI.cpp`, `EESETSETGame.cpp`, `EESETSETLogObject.cpp`, `EESETSETMaintainObject.cpp`, `EESETSETMedia.cpp`,
//! `EESETSETMSDCameraScrollChange.cpp`, `EESETSETObject.cpp`, `EESETSETPlayer.cpp`, `EESETSETSAAAttributeChange.cpp`,
//! `EESETSETSetObjectStance.cpp`): what each effect type and action shows on the effect screen's shared controls
//! (`EESETSEffectControls`), stores into the effect, loads from it and names it.
//!
//! The controls stay C++ objects of the effect screen. Every one is addressed by its array and index
//! (`EE_SCREENS_SEET_A_*`: `mLabels[3]` is `(EE_SCREENS_SEET_A_LABEL, 3)`) through one generic host,
//! `ee_screenscpp_seet` (`Scenario Editor/EESETSEffect.cpp`): no effect type has a host of its own. The effect's
//! numbers are its `EeScenarioEffectFields`, read and written in place; its texts and objects are host ops.
//!
//! A host call can come back into the types (a combo box's event changes the action, the effect combo boxes rename
//! the effect): the types are used through `&self`, their state in `Cell`s and `RefCell`s never borrowed across a
//! host call.

use core::cell::{Cell, RefCell};

use ee_sim::scenario::fields::EeScenarioEffectFields;

use crate::screens::editor_players::SeplHost;

// ---- the control arrays (`EESETSEffectControls`' members)

pub const EE_SCREENS_SEET_A_LABEL: u32 = 0;
pub const EE_SCREENS_SEET_A_CHECK: u32 = 1;
pub const EE_SCREENS_SEET_A_COMBO: u32 = 2;
/// `mEffectComboBoxes` (`EESETSEComboBox`).
pub const EE_SCREENS_SEET_A_ECOMBO: u32 = 3;
/// `mScenarioComboBoxes` (`EESETSEScenarioComboBox`).
pub const EE_SCREENS_SEET_A_SCEN: u32 = 4;
pub const EE_SCREENS_SEET_A_EDIT: u32 = 5;
pub const EE_SCREENS_SEET_A_EDIT2: u32 = 6;
/// `mTextEditBox` (index 0).
pub const EE_SCREENS_SEET_A_TEXT: u32 = 7;
pub const EE_SCREENS_SEET_A_RADIO: u32 = 8;
pub const EE_SCREENS_SEET_A_SLIDER: u32 = 9;
/// `mPlayers` (`EESETSUICBPlayer`).
pub const EE_SCREENS_SEET_A_PLAYER: u32 = 10;
/// `mListBoxes` (`UILBString`).
pub const EE_SCREENS_SEET_A_LIST: u32 = 11;

// ---- the host's ops (`ee_screenscpp_seet`: p0 the controls, p1 the effect or a handle, p2 a byte buffer or an
// out handle; i0 the array, i1 the index, i2 and i3 the arguments; f the floats)

/// The control's display mode set to i2.
pub const EE_SCREENS_SEET_OP_DISPLAY: u32 = 1;
/// The control's rectangle -> f0..f3 (left, top, right, bottom) / set to f0..f3.
pub const EE_SCREENS_SEET_OP_RECT: u32 = 2;
pub const EE_SCREENS_SEET_OP_SET_RECT: u32 = 3;
/// The control moved to (f0, f1).
pub const EE_SCREENS_SEET_OP_LOCATION: u32 = 4;
/// The label's alignment set to i2.
pub const EE_SCREENS_SEET_OP_ALIGN: u32 = 5;
/// The label's or edit box's text set to the string i2 / to the i2 bytes at p2.
pub const EE_SCREENS_SEET_OP_TEXT_STRING: u32 = 6;
pub const EE_SCREENS_SEET_OP_TEXT: u32 = 7;
/// The label's text set to the number i2 (`SetText(long)`), the edit box's (`SetText(unsigned long)`).
pub const EE_SCREENS_SEET_OP_NUMBER: u32 = 8;
/// The label's, edit box's or effect combo box's first line / the edit box's whole text / the list box's line i2,
/// into p2 (capacity i3) -> its length.
pub const EE_SCREENS_SEET_OP_GET_TEXT: u32 = 9;
pub const EE_SCREENS_SEET_OP_GET_ALL_TEXT: u32 = 10;
pub const EE_SCREENS_SEET_OP_LIST_DATA: u32 = 11;
/// -> the label's or edit box's first line as an integer (`GetTextAsInt(0)`).
pub const EE_SCREENS_SEET_OP_TEXT_INT: u32 = 12;
/// The control's width changed to i2 (`ChangeWidth`) / -> the edit box's width.
pub const EE_SCREENS_SEET_OP_WIDTH: u32 = 13;
pub const EE_SCREENS_SEET_OP_GET_WIDTH: u32 = 14;
/// The combo box or list box emptied / given the string i2 / given the i2 bytes at p2.
pub const EE_SCREENS_SEET_OP_CLEAR: u32 = 15;
pub const EE_SCREENS_SEET_OP_APPEND_STRING: u32 = 16;
pub const EE_SCREENS_SEET_OP_APPEND: u32 = 17;
/// -> the combo box's selection (-1 when `GetCurrentSelection` fails) / set to i2 / -> its list box's line count.
pub const EE_SCREENS_SEET_OP_SELECTION: u32 = 18;
pub const EE_SCREENS_SEET_OP_SET_SELECTION: u32 = 19;
pub const EE_SCREENS_SEET_OP_LINE_COUNT: u32 = 21;
/// The control's trigger i2's event held off: referenced and removed, its handle (NULL: none) into p2's first word
/// / the held event p1 set back and released.
pub const EE_SCREENS_SEET_OP_EVENT_HOLD: u32 = 20;
pub const EE_SCREENS_SEET_OP_EVENT_RESTORE: u32 = 79;
/// The control's trigger i2's event removed / set to the event p1.
pub const EE_SCREENS_SEET_OP_EVENT_NULL: u32 = 22;
pub const EE_SCREENS_SEET_OP_EVENT_SET: u32 = 23;
/// -> whether the check box or radio button is checked / the check box set to i2 / the radio button checked.
pub const EE_SCREENS_SEET_OP_CHECKED: u32 = 24;
pub const EE_SCREENS_SEET_OP_SET_CHECKED: u32 = 25;
pub const EE_SCREENS_SEET_OP_CHECK: u32 = 26;
/// The check box's or radio button's text set to the string i2 in the font i3 (`EE_SCREENS_SEPL_FONT_*`).
pub const EE_SCREENS_SEET_OP_TEXT_FONT: u32 = 27;
/// The check box's, radio button's (its text label's) or list box's text yellow and transparent.
pub const EE_SCREENS_SEET_OP_YELLOW: u32 = 28;
/// -> the slider's value / set to i2 / its range set to i2..i3 / its page size to i2 / its range-changed event
/// fired.
pub const EE_SCREENS_SEET_OP_SLIDER: u32 = 29;
pub const EE_SCREENS_SEET_OP_SET_SLIDER: u32 = 30;
pub const EE_SCREENS_SEET_OP_RANGE: u32 = 31;
pub const EE_SCREENS_SEET_OP_PAGE_SIZE: u32 = 32;
pub const EE_SCREENS_SEET_OP_FIRE_RANGE: u32 = 33;
/// The control's help string set to i2 (`SetStringTableID`).
pub const EE_SCREENS_SEET_OP_STRING_TABLE: u32 = 34;
/// The combo box's or edit box's text font set to `gU531930` (i2 = 1: a combo box's list box's too).
pub const EE_SCREENS_SEET_OP_FONT_530: u32 = 35;
/// -> the player picker's player / set to i2 / its flags set to i2 (`SetFlags`: refilled).
pub const EE_SCREENS_SEET_OP_PLAYER: u32 = 36;
pub const EE_SCREENS_SEET_OP_SET_PLAYER: u32 = 37;
pub const EE_SCREENS_SEET_OP_PLAYER_FLAGS: u32 = 39;
/// The scenario combo box's kind set to i2 (`mMode`, `kSETriggerScreen*`).
pub const EE_SCREENS_SEET_OP_SCEN_MODE: u32 = 40;
/// The combo box given the scenario's names of the kind i2 (`EE_SCENARIO_NAMES_*`) line by line (i3 = 0) or as one
/// array (`AppendStringArray`).
pub const EE_SCREENS_SEET_OP_FILL_NAMES: u32 = 41;
/// The label i2 and the scenario combo box (the index) refilled with the areas (i3) or the objects:
/// `FillAreaObjectComboBox` / `FillScenarioComboBox`; the label i2 and the combo box (the index):
/// `FillPlaceComboBox`.
pub const EE_SCREENS_SEET_OP_FILL_AREA_OBJECT: u32 = 42;
pub const EE_SCREENS_SEET_OP_FILL_SCENARIO: u32 = 43;
pub const EE_SCREENS_SEET_OP_FILL_PLACE: u32 = 44;
/// The list box's and its scroll bar's background opacity set to f0.
pub const EE_SCREENS_SEET_OP_LIST_OPACITY: u32 = 45;
/// The i2 bytes at p2 inserted as the list box's line i3 / its line i2 deleted.
pub const EE_SCREENS_SEET_OP_LIST_INSERT: u32 = 46;
pub const EE_SCREENS_SEET_OP_LIST_DELETE: u32 = 47;
/// The form's control of id i2 given the display mode i3.
pub const EE_SCREENS_SEET_OP_FORM_DISPLAY: u32 = 48;
/// The slider's range event (a `UIESliderChange`) given the modifier f0.
pub const EE_SCREENS_SEET_OP_SLIDER_MODIFIER: u32 = 49;
/// An event made (its handle into p2's first word): i0 the kind (`EE_SCREENS_SEET_EV_*`), i1..i3 its arguments.
pub const EE_SCREENS_SEET_OP_EVENT_NEW: u32 = 50;
/// The slider event p1 given the value f0's text: the string i2 (i3 = 1) or the i2 bytes at p2.
pub const EE_SCREENS_SEET_OP_EVENT_STRING: u32 = 51;
/// The event p1 released.
pub const EE_SCREENS_SEET_OP_EVENT_RELEASE: u32 = 52;
/// The string i2 / the string i2 as a `sprintf` format of the number i3 / the NUL-terminated format p1 of the
/// float f0 (passed as a double) into p2 (capacity i1) -> the length.
pub const EE_SCREENS_SEET_OP_STRING: u32 = 53;
pub const EE_SCREENS_SEET_OP_FORMAT_NUMBER: u32 = 54;
pub const EE_SCREENS_SEET_OP_FORMAT_FLOAT: u32 = 55;
/// The trigger tab's name box set to the i2 bytes at p2 / its name (`GetName`) into p2 (capacity i1).
pub const EE_SCREENS_SEET_OP_SET_NAME_BOX: u32 = 56;
pub const EE_SCREENS_SEET_OP_NAME_BOX: u32 = 57;
/// The effect p1's text i2 (`EE_SCREENS_SEET_S_*`) into p2 (capacity i1) / set to the i3 bytes at p2.
pub const EE_SCREENS_SEET_OP_EFFECT_TEXT: u32 = 58;
pub const EE_SCREENS_SEET_OP_SET_EFFECT_TEXT: u32 = 59;
/// The effect p1's objects: -> their count / the object i2 / set to the i2 words at p2.
pub const EE_SCREENS_SEET_OP_OBJECT_COUNT: u32 = 60;
pub const EE_SCREENS_SEET_OP_OBJECT: u32 = 61;
pub const EE_SCREENS_SEET_OP_SET_OBJECTS: u32 = 62;
/// The scenario's names of the kind i2 kept by the host -> their count / the kept name i2 into p2 (capacity i1).
pub const EE_SCREENS_SEET_OP_NAMES: u32 = 63;
pub const EE_SCREENS_SEET_OP_NAME: u32 = 64;
/// The scenario object / trigger i2's name into p2 (capacity i1) -> its length, -1 when there is none.
pub const EE_SCREENS_SEET_OP_OBJECT_NAME: u32 = 65;
pub const EE_SCREENS_SEET_OP_TRIGGER_NAME: u32 = 66;
/// -> the calamity class count / whether the class i2 exists (its id into i2, its name into i3).
pub const EE_SCREENS_SEET_OP_CALAMITY_COUNT: u32 = 67;
pub const EE_SCREENS_SEET_OP_CALAMITY: u32 = 68;
/// -> the graphic effect count / whether the effect i2 exists and is listed for scenarios (id into i2, name i3).
pub const EE_SCREENS_SEET_OP_GRAPHIC_COUNT: u32 = 69;
pub const EE_SCREENS_SEET_OP_GRAPHIC: u32 = 70;
/// -> the computer player behaviours' count / the behaviour i2's value i3 (`EE_SCREENS_SEET_B_*`).
pub const EE_SCREENS_SEET_OP_BEHAVIOR_COUNT: u32 = 71;
pub const EE_SCREENS_SEET_OP_BEHAVIOR: u32 = 72;
/// -> the unit types' count / the type i2's: -1 not scenario selectable, else its domain (`EE_SCREENS_SEET_D_*`),
/// its unit type into i2 and its name string into i3.
pub const EE_SCREENS_SEET_OP_UNIT_TYPE_COUNT: u32 = 73;
pub const EE_SCREENS_SEET_OP_UNIT_TYPE: u32 = 74;
/// -> the unit type i2's name string.
pub const EE_SCREENS_SEET_OP_UNIT_NAME: u32 = 75;
/// The editor's technologies kept by the host (the tree's nodes shown in the editor and the epochs, sorted by
/// name; i2 = 1: only those making no unit, the condition types') -> their count / the kept technology i2 / the technology i2's line into p2 (capacity i1).
pub const EE_SCREENS_SEET_OP_TECHS: u32 = 76;
pub const EE_SCREENS_SEET_OP_TECH: u32 = 77;
pub const EE_SCREENS_SEET_OP_TECH_LINE: u32 = 78;
/// The control's trigger event p1 (an `EESEERust` check display event) given the control to show or hide
/// (`EESEEPushControl`) / processed on the control (`ProcessEvent` with the editor form).
pub const EE_SCREENS_SEET_OP_EVENT_PUSH: u32 = 80;
pub const EE_SCREENS_SEET_OP_EVENT_PROCESS: u32 = 81;
/// The technology i2's name (the unit class's for a unit-making node, "" for none) into p2 (capacity i1).
pub const EE_SCREENS_SEET_OP_TECH_NAME: u32 = 82;
/// The screen's controls (`EESETSConditionControls` / `EESETSEffectControls`) made on the form p1 from the control
/// index i2 (advanced; the screen font) -> their handle into p2's first word / the controls p0 deleted.
pub const EE_SCREENS_SEET_OP_CONTROLS_NEW: u32 = 83;
pub const EE_SCREENS_SEET_OP_CONTROLS_FREE: u32 = 84;
/// The scenario's condition / effect i2 (`GetCondition` / `GetEffect`) -> its pointer and its numbers' into p2's two
/// words (0: none).
pub const EE_SCREENS_SEET_OP_ENTRY: u32 = 85;
/// The conditions' / effects' list operation i2 (`EE_SCREENS_SEOB_SM_*`) on the entry i3.
pub const EE_SCREENS_SEET_OP_LIST: u32 = 86;
/// The user conditions' / effects' names into the `std::vector<UWideString>` p1.
pub const EE_SCREENS_SEET_OP_GET_NAMES: u32 = 87;
/// The trigger tab's i2 (`EE_SCREENS_SEET_TS_*`).
pub const EE_SCREENS_SEET_OP_TSCREEN: u32 = 88;
/// The trigger tab's description box's whole text into p2 (capacity i1) / set to the i2 bytes at p2.
pub const EE_SCREENS_SEET_OP_TEXT_BOX: u32 = 89;
pub const EE_SCREENS_SEET_OP_SET_TEXT_BOX: u32 = 90;
/// -> the player tab's player count / the player picker's (the index) player count set to i2.
pub const EE_SCREENS_SEET_OP_PLAYER_COUNT: u32 = 91;
pub const EE_SCREENS_SEET_OP_SET_PLAYER_COUNT: u32 = 92;
/// -> the combo box p1's selection, -1 when it has none.
pub const EE_SCREENS_SEET_OP_HANDLE_SELECTION: u32 = 93;

/// The trigger tab's description box (and alphabetical sort) hidden, its alphabetical sort shown, its description
/// box shown (`EE_SCREENS_SEET_OP_TSCREEN`).
pub const EE_SCREENS_SEET_TS_HIDE_TEXT: i32 = 0;
pub const EE_SCREENS_SEET_TS_SHOW_ALPHA_SORT: i32 = 1;
pub const EE_SCREENS_SEET_TS_SHOW_TEXT: i32 = 2;

/// The events (`EE_SCREENS_SEET_OP_EVENT_NEW`): a screen event (`EECreateScreenEvent(i1)`), an `EESEERust(i1)`, a
/// `UIESliderChange(label i1, modifier f0, offset i2)`, an `EESEENewSliderChange(label i1, format i2: 0 "%2.0f",
/// 1 "%#2.1f", f0, f1)`, an `EESEENewUseObjectArea(label i1, scenario combo box i2, radio i3)`, an
/// `EESEENewObjects(i1, p1 the effect types, the controls)`.
pub const EE_SCREENS_SEET_EV_SCREEN: i32 = 1;
pub const EE_SCREENS_SEET_EV_SEE: i32 = 2;
pub const EE_SCREENS_SEET_EV_SLIDER: i32 = 3;
pub const EE_SCREENS_SEET_EV_SLIDER_FORMAT: i32 = 4;
pub const EE_SCREENS_SEET_EV_USE_OBJECT_AREA: i32 = 5;
pub const EE_SCREENS_SEET_EV_OBJECTS: i32 = 6;

/// The effect's texts: its name, new name, message, file name, description.
pub const EE_SCREENS_SEET_S_NAME: i32 = 0;
pub const EE_SCREENS_SEET_S_NEW_NAME: i32 = 1;
pub const EE_SCREENS_SEET_S_MESSAGE: i32 = 2;
pub const EE_SCREENS_SEET_S_FILE_NAME: i32 = 3;
pub const EE_SCREENS_SEET_S_DESCRIPTION: i32 = 4;
/// A condition's chat text; the entry's text (the trigger tab's description box).
pub const EE_SCREENS_SEET_S_CHAT: i32 = 5;
pub const EE_SCREENS_SEET_S_TEXT: i32 = 6;

/// A behaviour's values: settable, the first, second and float sliders' minimum and maximum, the float scale.
pub const EE_SCREENS_SEET_B_SETTABLE: i32 = 0;
pub const EE_SCREENS_SEET_B_INT1_MIN: i32 = 1;
pub const EE_SCREENS_SEET_B_INT1_MAX: i32 = 2;
pub const EE_SCREENS_SEET_B_INT2_MIN: i32 = 3;
pub const EE_SCREENS_SEET_B_INT2_MAX: i32 = 4;
pub const EE_SCREENS_SEET_B_FLOAT_MIN: i32 = 5;
pub const EE_SCREENS_SEET_B_FLOAT_MAX: i32 = 6;
pub const EE_SCREENS_SEET_B_FLOAT_SCALE: i32 = 7;

/// A unit type's domain: air, sea, land.
pub const EE_SCREENS_SEET_D_AIR: i32 = 0;
pub const EE_SCREENS_SEET_D_SEA: i32 = 1;
pub const EE_SCREENS_SEET_D_LAND: i32 = 2;

/// The type slots (`EESETSEffectType`'s virtuals): ChangeAction, Store, VirtualC (hide), Load, Show, UpdateName.
pub const EE_SCREENS_SEET_CHANGE_ACTION: u32 = 1;
pub const EE_SCREENS_SEET_STORE: u32 = 2;
pub const EE_SCREENS_SEET_HIDE: u32 = 3;
pub const EE_SCREENS_SEET_LOAD: u32 = 4;
pub const EE_SCREENS_SEET_SHOW: u32 = 5;
pub const EE_SCREENS_SEET_UPDATE_NAME: u32 = 6;

// ---- shared values

pub(crate) const LABEL: u32 = EE_SCREENS_SEET_A_LABEL;
pub(crate) const CHECK: u32 = EE_SCREENS_SEET_A_CHECK;
pub(crate) const COMBO: u32 = EE_SCREENS_SEET_A_COMBO;
pub(crate) const ECOMBO: u32 = EE_SCREENS_SEET_A_ECOMBO;
pub(crate) const SCEN: u32 = EE_SCREENS_SEET_A_SCEN;
pub(crate) const EDIT: u32 = EE_SCREENS_SEET_A_EDIT;
pub(crate) const EDIT2: u32 = EE_SCREENS_SEET_A_EDIT2;
pub(crate) const TEXT: u32 = EE_SCREENS_SEET_A_TEXT;
pub(crate) const RADIO: u32 = EE_SCREENS_SEET_A_RADIO;
pub(crate) const SLIDER: u32 = EE_SCREENS_SEET_A_SLIDER;
pub(crate) const PLAYER: u32 = EE_SCREENS_SEET_A_PLAYER;
pub(crate) const LIST: u32 = EE_SCREENS_SEET_A_LIST;

/// `kControlTriggerRangeChanged`, `Checked`, `SelectionChanged`, `ButtonUp`.
pub(crate) const T_RANGE: i32 = 0x3e8;
pub(crate) const T_CHECKED: i32 = 0x3e9;
pub(crate) const T_SELECTION: i32 = 0x3eb;
pub(crate) const T_BUTTON_UP: i32 = 0x3f0;

/// `kTextAlignLeft`, `Center`, `Right`.
pub(crate) const AL_LEFT: i32 = 0;
pub(crate) const AL_CENTER: i32 = 1;
pub(crate) const AL_RIGHT: i32 = 2;

/// The fonts (`EE_SCREENS_SEPL_FONT_534`, `_538`).
pub(crate) const FONT_534: i32 = 2;
pub(crate) const FONT_538: i32 = 3;

/// The scenario combo boxes' kinds (`kSETriggerScreenTrigger`, `Object`, `Area`).
pub(crate) const MODE_TRIGGER: i32 = 0;
pub(crate) const MODE_OBJECT: i32 = 3;
pub(crate) const MODE_AREA: i32 = 4;

/// The scenario's names (`EE_SCENARIO_NAMES_TRIGGERS`, `OBJECTS`, `AREAS`).
pub(crate) const NAMES_TRIGGERS: i32 = ee_sim::scenario::lists::EE_SCENARIO_NAMES_TRIGGERS as i32;
pub(crate) const NAMES_OBJECTS: i32 = ee_sim::scenario::lists::EE_SCENARIO_NAMES_OBJECTS as i32;
pub(crate) const NAMES_AREAS: i32 = ee_sim::scenario::lists::EE_SCENARIO_NAMES_AREAS as i32;

/// The form's control the camera and dialogue types hide while shown (id 8).
pub(crate) const CONTROL_8: i32 = 8;

/// The screen events (`EE_SCREENS_SE_*`) and the editor events (`kSEE*`) the types make.
pub(crate) use crate::screens::editor_events::{
    EE_SCREENS_SEE_CAMG_MODIFY_OBJECT, EE_SCREENS_SEE_FACE_AREA_OBJECT, EE_SCREENS_SEE_MSD_CAMERA_SCROLL,
    EE_SCREENS_SEE_SAA_ATTRIBUTE, EE_SCREENS_SEE_USE_AREA_OBJECT,
};
pub(crate) use crate::screens::scenario_editor::{
    EE_SCREENS_SE_CHANGE_EFFECT_ACTION, EE_SCREENS_SE_CLEAR_ADD_LINE, EE_SCREENS_SE_PLACE,
};

pub(crate) use crate::screens::editor_effects_ids::*;

// ---- the generic host

/// One call's context: the host, the screen's controls, the entry (0 for a control-only slot) and its numbers
/// (`F`: an effect's `EeScenarioEffectFields`, a condition's `EeScenarioConditionFields`), the types' own handle
/// (the events of the military group, dialogue and AI attribute types).
pub struct Fx<'a, H: SeplHost + ?Sized, F = EeScenarioEffectFields> {
    pub h: &'a mut H,
    pub controls: usize,
    pub effect: usize,
    pub fields: *mut F,
    pub me: usize,
}

impl<H: SeplHost + ?Sized, F> Fx<'_, H, F> {
    /// The entry's numbers (never held across a host call).
    #[allow(clippy::mut_from_ref)]
    pub(crate) fn e(&self) -> &mut F {
        // SAFETY: the screen passes its selected entry's numbers for the slots that take an effect; the
        // reference lives for one field access
        unsafe { &mut *self.fields }
    }

    fn call(&mut self, op: u32, p: [usize; 2], i: [i32; 4], f: [f32; 4]) -> (i32, [i32; 4], [f32; 4]) {
        let (mut i, mut f) = (i, f);
        let r = self.h.op(op, [self.controls, p[0], p[1]], &mut i, &mut f);
        (r, i, f)
    }

    pub(crate) fn op(&mut self, op: u32, a: u32, k: u32, x: i32, y: i32) -> i32 {
        self.call(op, [self.effect, 0], [a as i32, k as i32, x, y], [0.0; 4]).0
    }

    fn opf(&mut self, op: u32, a: u32, k: u32, f: [f32; 4]) {
        self.call(op, [self.effect, 0], [a as i32, k as i32, 0, 0], f);
    }

    /// A text the host writes into a buffer (capacity in `i[cap]`), grown and asked again when short.
    fn read(&mut self, op: u32, p1: usize, mut i: [i32; 4], cap: usize) -> Option<Vec<u8>> {
        let mut buf = vec![0u8; 256];
        loop {
            i[cap] = buf.len() as i32;
            let (n, _, _) = self.call(op, [p1, buf.as_mut_ptr() as usize], i, [0.0; 4]);
            if n < 0 {
                return None;
            }
            let n = n as usize;
            if n <= buf.len() {
                buf.truncate(n);
                return Some(buf);
            }
            buf = vec![0u8; n];
        }
    }

    // -- any control

    pub(crate) fn display(&mut self, a: u32, k: u32, shown: bool) {
        self.op(EE_SCREENS_SEET_OP_DISPLAY, a, k, shown as i32, 0);
    }
    pub(crate) fn show(&mut self, a: u32, k: u32) {
        self.display(a, k, true);
    }
    pub(crate) fn hide(&mut self, a: u32, k: u32) {
        self.display(a, k, false);
    }
    pub(crate) fn rect(&mut self, a: u32, k: u32) -> [f32; 4] {
        self.call(EE_SCREENS_SEET_OP_RECT, [self.effect, 0], [a as i32, k as i32, 0, 0], [0.0; 4]).2
    }
    pub(crate) fn left(&mut self, a: u32, k: u32) -> f32 {
        self.rect(a, k)[0]
    }
    pub(crate) fn top(&mut self, a: u32, k: u32) -> f32 {
        self.rect(a, k)[1]
    }
    pub(crate) fn right(&mut self, a: u32, k: u32) -> f32 {
        self.rect(a, k)[2]
    }
    pub(crate) fn bottom(&mut self, a: u32, k: u32) -> f32 {
        self.rect(a, k)[3]
    }
    /// `SetRectangle(GERectangle(location, GE2DPoint(x + w, y + h)))`.
    pub(crate) fn set_rect(&mut self, a: u32, k: u32, x: f32, y: f32, w: f32, h: f32) {
        self.opf(EE_SCREENS_SEET_OP_SET_RECT, a, k, [x, y, x + w, y + h]);
    }
    pub(crate) fn locate(&mut self, a: u32, k: u32, x: f32, y: f32) {
        self.opf(EE_SCREENS_SEET_OP_LOCATION, a, k, [x, y, 0.0, 0.0]);
    }
    pub(crate) fn align(&mut self, a: u32, k: u32, align: i32) {
        self.op(EE_SCREENS_SEET_OP_ALIGN, a, k, align, 0);
    }
    pub(crate) fn text_id(&mut self, a: u32, k: u32, id: u32) {
        self.op(EE_SCREENS_SEET_OP_TEXT_STRING, a, k, id as i32, 0);
    }
    pub(crate) fn text(&mut self, a: u32, k: u32, text: &[u8]) {
        self.call(
            EE_SCREENS_SEET_OP_TEXT,
            [self.effect, text.as_ptr() as usize],
            [a as i32, k as i32, text.len() as i32, 0],
            [0.0; 4],
        );
    }
    pub(crate) fn number(&mut self, a: u32, k: u32, n: i32) {
        self.op(EE_SCREENS_SEET_OP_NUMBER, a, k, n, 0);
    }
    pub(crate) fn get_text(&mut self, a: u32, k: u32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_GET_TEXT, self.effect, [a as i32, k as i32, 0, 0], 3).unwrap_or_default()
    }
    pub(crate) fn get_all_text(&mut self, a: u32, k: u32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_GET_ALL_TEXT, self.effect, [a as i32, k as i32, 0, 0], 3).unwrap_or_default()
    }
    pub(crate) fn list_data(&mut self, k: u32, line: u32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_LIST_DATA, self.effect, [LIST as i32, k as i32, line as i32, 0], 3)
            .unwrap_or_default()
    }
    pub(crate) fn text_int(&mut self, a: u32, k: u32) -> i32 {
        self.op(EE_SCREENS_SEET_OP_TEXT_INT, a, k, 0, 0)
    }
    pub(crate) fn width(&mut self, a: u32, k: u32, w: i32) {
        self.op(EE_SCREENS_SEET_OP_WIDTH, a, k, w, 0);
    }
    pub(crate) fn get_width(&mut self, a: u32, k: u32) -> i32 {
        self.op(EE_SCREENS_SEET_OP_GET_WIDTH, a, k, 0, 0)
    }
    pub(crate) fn clear(&mut self, a: u32, k: u32) {
        self.op(EE_SCREENS_SEET_OP_CLEAR, a, k, 0, 0);
    }
    pub(crate) fn append_id(&mut self, a: u32, k: u32, id: u32) {
        self.op(EE_SCREENS_SEET_OP_APPEND_STRING, a, k, id as i32, 0);
    }
    /// The strings `first..=last` appended one by one.
    pub(crate) fn append_ids(&mut self, a: u32, k: u32, first: u32, last: u32) {
        for id in first..=last {
            self.append_id(a, k, id);
        }
    }
    pub(crate) fn append(&mut self, a: u32, k: u32, text: &[u8]) {
        self.call(
            EE_SCREENS_SEET_OP_APPEND,
            [self.effect, text.as_ptr() as usize],
            [a as i32, k as i32, text.len() as i32, 0],
            [0.0; 4],
        );
    }
    /// `GetCurrentSelection`: `None` when it fails.
    pub(crate) fn selection(&mut self, a: u32, k: u32) -> Option<u32> {
        let s = self.op(EE_SCREENS_SEET_OP_SELECTION, a, k, 0, 0);
        (s >= 0).then_some(s as u32)
    }
    pub(crate) fn select(&mut self, a: u32, k: u32, line: u32) {
        self.op(EE_SCREENS_SEET_OP_SET_SELECTION, a, k, line as i32, 0);
    }
    /// The selection set with the combo box's selection-changed event held off (referenced, removed, restored).
    pub(crate) fn select_silent(&mut self, a: u32, k: u32, line: u32) {
        let held = self.event_hold(a, k, T_SELECTION);
        self.select(a, k, line);
        self.event_restore(a, k, T_SELECTION, held);
    }
    /// The control's trigger's event held off: referenced and removed (0: there was none).
    pub(crate) fn event_hold(&mut self, a: u32, k: u32, trigger: i32) -> usize {
        let mut out = 0usize;
        self.call(
            EE_SCREENS_SEET_OP_EVENT_HOLD,
            [0, &mut out as *mut usize as usize],
            [a as i32, k as i32, trigger, 0],
            [0.0; 4],
        );
        out
    }
    /// A held event set back and released (none: nothing).
    pub(crate) fn event_restore(&mut self, a: u32, k: u32, trigger: i32, held: usize) {
        if held != 0 {
            self.call(EE_SCREENS_SEET_OP_EVENT_RESTORE, [held, 0], [a as i32, k as i32, trigger, 0], [0.0; 4]);
        }
    }
    pub(crate) fn line_count(&mut self, a: u32, k: u32) -> u32 {
        self.op(EE_SCREENS_SEET_OP_LINE_COUNT, a, k, 0, 0) as u32
    }
    pub(crate) fn event_null(&mut self, a: u32, k: u32, trigger: i32) {
        self.op(EE_SCREENS_SEET_OP_EVENT_NULL, a, k, trigger, 0);
    }
    pub(crate) fn event_set(&mut self, a: u32, k: u32, trigger: i32, event: usize) {
        self.call(EE_SCREENS_SEET_OP_EVENT_SET, [event, 0], [a as i32, k as i32, trigger, 0], [0.0; 4]);
    }
    pub(crate) fn checked(&mut self, a: u32, k: u32) -> bool {
        self.op(EE_SCREENS_SEET_OP_CHECKED, a, k, 0, 0) != 0
    }
    pub(crate) fn set_checked(&mut self, k: u32, on: bool) {
        self.op(EE_SCREENS_SEET_OP_SET_CHECKED, CHECK, k, on as i32, 0);
    }
    /// The check box set to `on` only when it differs (the types' `GetIsChecked() != value` guard).
    pub(crate) fn set_checked_if(&mut self, k: u32, on: bool) {
        if self.checked(CHECK, k) != on {
            self.set_checked(k, on);
        }
    }
    pub(crate) fn check(&mut self, k: u32) {
        self.op(EE_SCREENS_SEET_OP_CHECK, RADIO, k, 0, 0);
    }
    pub(crate) fn text_font(&mut self, a: u32, k: u32, id: u32, font: i32) {
        self.op(EE_SCREENS_SEET_OP_TEXT_FONT, a, k, id as i32, font);
    }
    pub(crate) fn yellow(&mut self, a: u32, k: u32) {
        self.op(EE_SCREENS_SEET_OP_YELLOW, a, k, 0, 0);
    }
    pub(crate) fn slider(&mut self, k: u32) -> i32 {
        self.op(EE_SCREENS_SEET_OP_SLIDER, SLIDER, k, 0, 0)
    }
    pub(crate) fn set_slider(&mut self, k: u32, v: i32) {
        self.op(EE_SCREENS_SEET_OP_SET_SLIDER, SLIDER, k, v, 0);
    }
    /// The slider moved to `v` only when it differs.
    pub(crate) fn set_slider_if(&mut self, k: u32, v: i32) {
        if self.slider(k) != v {
            self.set_slider(k, v);
        }
    }
    pub(crate) fn range(&mut self, k: u32, lo: i32, hi: i32) {
        self.op(EE_SCREENS_SEET_OP_RANGE, SLIDER, k, lo, hi);
    }
    pub(crate) fn page_size(&mut self, k: u32, n: i32) {
        self.op(EE_SCREENS_SEET_OP_PAGE_SIZE, SLIDER, k, n, 0);
    }
    pub(crate) fn fire_range(&mut self, k: u32) {
        self.op(EE_SCREENS_SEET_OP_FIRE_RANGE, SLIDER, k, 0, 0);
    }
    pub(crate) fn string_table(&mut self, a: u32, k: u32, id: u32) {
        self.op(EE_SCREENS_SEET_OP_STRING_TABLE, a, k, id as i32, 0);
    }
    pub(crate) fn font_530(&mut self, a: u32, k: u32) {
        self.op(EE_SCREENS_SEET_OP_FONT_530, a, k, 0, 0);
    }
    /// The combo box's text and list box fonts set to `gU531930`.
    pub(crate) fn font_530_both(&mut self, a: u32, k: u32) {
        self.op(EE_SCREENS_SEET_OP_FONT_530, a, k, 1, 0);
    }
    /// The check display event `event` given the control (a, k) to show or hide with its check box.
    pub(crate) fn event_push(&mut self, event: usize, a: u32, k: u32) {
        self.call(EE_SCREENS_SEET_OP_EVENT_PUSH, [event, 0], [a as i32, k as i32, 0, 0], [0.0; 4]);
    }
    /// The event processed on the control (a, k) (`ProcessEvent` with the editor form).
    pub(crate) fn event_process(&mut self, event: usize, a: u32, k: u32) {
        self.call(EE_SCREENS_SEET_OP_EVENT_PROCESS, [event, 0], [a as i32, k as i32, 0, 0], [0.0; 4]);
    }
    pub(crate) fn player(&mut self, k: u32) -> u32 {
        self.op(EE_SCREENS_SEET_OP_PLAYER, PLAYER, k, 0, 0) as u32
    }
    pub(crate) fn set_player(&mut self, k: u32, p: u32) {
        self.op(EE_SCREENS_SEET_OP_SET_PLAYER, PLAYER, k, p as i32, 0);
    }
    /// The player set with the picker's selection-changed event held off.
    pub(crate) fn set_player_silent(&mut self, k: u32, p: u32) {
        let held = self.event_hold(PLAYER, k, T_SELECTION);
        self.set_player(k, p);
        self.event_restore(PLAYER, k, T_SELECTION, held);
    }
    pub(crate) fn player_flags(&mut self, k: u32, flags: i32) {
        self.op(EE_SCREENS_SEET_OP_PLAYER_FLAGS, PLAYER, k, flags, 0);
    }
    pub(crate) fn scen_mode(&mut self, k: u32, mode: i32) {
        self.op(EE_SCREENS_SEET_OP_SCEN_MODE, SCEN, k, mode, 0);
    }
    pub(crate) fn fill_names(&mut self, a: u32, k: u32, kind: i32, as_array: bool) {
        self.op(EE_SCREENS_SEET_OP_FILL_NAMES, a, k, kind, as_array as i32);
    }
    /// `FillAreaObjectComboBox(mLabels[label], mScenarioComboBoxes[scen], areas)`.
    pub(crate) fn fill_area_object(&mut self, label: u32, scen: u32, areas: bool) {
        self.op(EE_SCREENS_SEET_OP_FILL_AREA_OBJECT, SCEN, scen, label as i32, areas as i32);
    }
    /// `FillScenarioComboBox(mScenarioComboBoxes[scen], mLabels[label], areas)`.
    pub(crate) fn fill_scenario(&mut self, label: u32, scen: u32, areas: bool) {
        self.op(EE_SCREENS_SEET_OP_FILL_SCENARIO, SCEN, scen, label as i32, areas as i32);
    }
    /// `FillPlaceComboBox(areas, mLabels[label], mComboBoxes[combo])`.
    pub(crate) fn fill_place(&mut self, label: u32, combo: u32, areas: bool) {
        self.op(EE_SCREENS_SEET_OP_FILL_PLACE, COMBO, combo, label as i32, areas as i32);
    }
    pub(crate) fn list_opacity(&mut self, k: u32, v: f32) {
        self.opf(EE_SCREENS_SEET_OP_LIST_OPACITY, LIST, k, [v, 0.0, 0.0, 0.0]);
    }
    pub(crate) fn list_insert(&mut self, k: u32, text: &[u8], at: u32) {
        self.call(
            EE_SCREENS_SEET_OP_LIST_INSERT,
            [self.effect, text.as_ptr() as usize],
            [LIST as i32, k as i32, text.len() as i32, at as i32],
            [0.0; 4],
        );
    }
    pub(crate) fn list_delete(&mut self, k: u32, at: u32) {
        self.op(EE_SCREENS_SEET_OP_LIST_DELETE, LIST, k, at as i32, 0);
    }
    pub(crate) fn form_display(&mut self, id: i32, shown: bool) {
        self.op(EE_SCREENS_SEET_OP_FORM_DISPLAY, 0, 0, id, shown as i32);
    }
    pub(crate) fn slider_modifier(&mut self, k: u32, m: f32) {
        self.opf(EE_SCREENS_SEET_OP_SLIDER_MODIFIER, SLIDER, k, [m, 0.0, 0.0, 0.0]);
    }

    // -- events

    /// An event of the kind `EE_SCREENS_SEET_EV_*` (one reference, released by `release`).
    pub(crate) fn event_new(&mut self, kind: i32, i: [i32; 3], f: [f32; 2], p1: usize) -> usize {
        let mut out = 0usize;
        self.call(
            EE_SCREENS_SEET_OP_EVENT_NEW,
            [p1, &mut out as *mut usize as usize],
            [kind, i[0], i[1], i[2]],
            [f[0], f[1], 0.0, 0.0],
        );
        out
    }
    pub(crate) fn screen_event(&mut self, code: u32) -> usize {
        self.event_new(EE_SCREENS_SEET_EV_SCREEN, [code as i32, 0, 0], [0.0; 2], 0)
    }
    pub(crate) fn see_event(&mut self, kind: u32) -> usize {
        self.event_new(EE_SCREENS_SEET_EV_SEE, [kind as i32, 0, 0], [0.0; 2], 0)
    }
    /// `UIESliderChange(mLabels[label], modifier, offset)`.
    pub(crate) fn slider_event(&mut self, label: u32, modifier: f32, offset: i32) -> usize {
        self.event_new(EE_SCREENS_SEET_EV_SLIDER, [label as i32, offset, 0], [modifier, 0.0], 0)
    }
    pub(crate) fn event_string_id(&mut self, event: usize, value: f32, id: u32) {
        self.call(EE_SCREENS_SEET_OP_EVENT_STRING, [event, 0], [0, 0, id as i32, 1], [value, 0.0, 0.0, 0.0]);
    }
    pub(crate) fn event_string(&mut self, event: usize, value: f32, text: &[u8]) {
        self.call(
            EE_SCREENS_SEET_OP_EVENT_STRING,
            [event, text.as_ptr() as usize],
            [0, 0, text.len() as i32, 0],
            [value, 0.0, 0.0, 0.0],
        );
    }
    pub(crate) fn release(&mut self, event: usize) {
        self.call(EE_SCREENS_SEET_OP_EVENT_RELEASE, [event, 0], [0; 4], [0.0; 4]);
    }

    // -- texts and the world

    pub(crate) fn string(&mut self, id: u32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_STRING, 0, [0, 0, id as i32, 0], 1).unwrap_or_default()
    }
    /// `sprintf` of the string `id` as a format of the number `n`.
    pub(crate) fn format_number(&mut self, id: u32, n: i32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_FORMAT_NUMBER, 0, [0, 0, id as i32, n], 1).unwrap_or_default()
    }
    /// `sprintf` of the NUL-terminated `format` of the float `v` (passed as a double).
    pub(crate) fn format_float(&mut self, format: &'static [u8], v: f32) -> Vec<u8> {
        let mut buf = vec![0u8; 64];
        loop {
            let (n, _, _) = self.call(
                EE_SCREENS_SEET_OP_FORMAT_FLOAT,
                [format.as_ptr() as usize, buf.as_mut_ptr() as usize],
                [0, buf.len() as i32, 0, 0],
                [v, 0.0, 0.0, 0.0],
            );
            let n = n.max(0) as usize;
            if n <= buf.len() {
                buf.truncate(n);
                return buf;
            }
            buf = vec![0u8; n];
        }
    }
    pub(crate) fn set_name_box(&mut self, text: &[u8]) {
        self.call(EE_SCREENS_SEET_OP_SET_NAME_BOX, [0, text.as_ptr() as usize], [0, 0, text.len() as i32, 0], [0.0; 4]);
    }
    pub(crate) fn name_box(&mut self) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_NAME_BOX, 0, [0; 4], 1).unwrap_or_default()
    }
    pub(crate) fn effect_text(&mut self, which: i32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_EFFECT_TEXT, self.effect, [0, 0, which, 0], 1).unwrap_or_default()
    }
    pub(crate) fn set_effect_text(&mut self, which: i32, text: &[u8]) {
        self.call(
            EE_SCREENS_SEET_OP_SET_EFFECT_TEXT,
            [self.effect, text.as_ptr() as usize],
            [0, 0, which, text.len() as i32],
            [0.0; 4],
        );
    }
    pub(crate) fn objects(&mut self) -> Vec<u32> {
        let n = self.op(EE_SCREENS_SEET_OP_OBJECT_COUNT, 0, 0, 0, 0).max(0) as u32;
        (0..n).map(|k| self.op(EE_SCREENS_SEET_OP_OBJECT, 0, 0, k as i32, 0) as u32).collect()
    }
    pub(crate) fn set_objects(&mut self, objects: &[u32]) {
        self.call(
            EE_SCREENS_SEET_OP_SET_OBJECTS,
            [self.effect, objects.as_ptr() as usize],
            [0, 0, objects.len() as i32, 0],
            [0.0; 4],
        );
    }
    pub(crate) fn names(&mut self, kind: i32) -> Vec<Vec<u8>> {
        let n = self.op(EE_SCREENS_SEET_OP_NAMES, 0, 0, kind, 0).max(0);
        (0..n).map(|k| self.read(EE_SCREENS_SEET_OP_NAME, 0, [0, 0, k, 0], 1).unwrap_or_default()).collect()
    }
    pub(crate) fn object_name(&mut self, k: u32) -> Option<Vec<u8>> {
        self.read(EE_SCREENS_SEET_OP_OBJECT_NAME, 0, [0, 0, k as i32, 0], 1)
    }
    pub(crate) fn trigger_name(&mut self, k: u32) -> Option<Vec<u8>> {
        self.read(EE_SCREENS_SEET_OP_TRIGGER_NAME, 0, [0, 0, k as i32, 0], 1)
    }
    /// The calamity classes 0..=count that exist: (id, name string).
    pub(crate) fn calamities(&mut self) -> Vec<(u32, u32)> {
        let n = self.op(EE_SCREENS_SEET_OP_CALAMITY_COUNT, 0, 0, 0, 0).max(0) as u32;
        let mut out = Vec::new();
        for k in 0..=n {
            let (r, i, _) = self.call(EE_SCREENS_SEET_OP_CALAMITY, [0, 0], [0, 0, k as i32, 0], [0.0; 4]);
            if r != 0 {
                out.push((i[2] as u32, i[3] as u32));
            }
        }
        out
    }
    /// The graphic effects listed for scenarios: (id, name string).
    pub(crate) fn graphic_effects(&mut self) -> Vec<(u32, u32)> {
        let n = self.op(EE_SCREENS_SEET_OP_GRAPHIC_COUNT, 0, 0, 0, 0).max(0) as u32;
        let mut out = Vec::new();
        for k in 0..n {
            let (r, i, _) = self.call(EE_SCREENS_SEET_OP_GRAPHIC, [0, 0], [0, 0, k as i32, 0], [0.0; 4]);
            if r != 0 {
                out.push((i[2] as u32, i[3] as u32));
            }
        }
        out
    }
    pub(crate) fn behavior(&mut self, k: u32, what: i32) -> i32 {
        self.op(EE_SCREENS_SEET_OP_BEHAVIOR, 0, 0, k as i32, what)
    }
    /// The unit type list's entry `k`: (its domain or -1 when it is not scenario selectable, its unit type).
    pub(crate) fn unit_type(&mut self, k: u32) -> (i32, i32) {
        let (r, i, _) = self.call(EE_SCREENS_SEET_OP_UNIT_TYPE, [0, 0], [0, 0, k as i32, 0], [0.0; 4]);
        (r, i[2])
    }
    /// The editor's technologies (the host's sorted list).
    pub(crate) fn techs(&mut self) -> Vec<i32> {
        self.techs_of(false)
    }
    /// The editor's technologies; `no_units`: only those making no unit.
    pub(crate) fn techs_of(&mut self, no_units: bool) -> Vec<i32> {
        let n = self.op(EE_SCREENS_SEET_OP_TECHS, 0, 0, no_units as i32, 0).max(0);
        (0..n).map(|k| self.op(EE_SCREENS_SEET_OP_TECH, 0, 0, k, 0)).collect()
    }
    /// The technology `t`'s line in the technology combo box.
    pub(crate) fn tech_line(&mut self, t: i32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_TECH_LINE, 0, [0, 0, t, 0], 1).unwrap_or_default()
    }
    /// The technology `t`'s name ("" for none).
    pub(crate) fn tech_name(&mut self, t: i32) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_TECH_NAME, 0, [0, 0, t, 0], 1).unwrap_or_default()
    }

    // -- the screens (`editor_subscreens.rs`)

    /// The screen's controls' object made (empty: `editor_controls.rs` makes the members); `*index` unchanged.
    pub(crate) fn controls_new(&mut self, form: usize, index: &mut u32) -> usize {
        let mut out = 0usize;
        let (_, i, _) = self.call(
            EE_SCREENS_SEET_OP_CONTROLS_NEW,
            [form, &mut out as *mut usize as usize],
            [0, 0, *index as i32, 0],
            [0.0; 4],
        );
        *index = i[2] as u32;
        out
    }
    pub(crate) fn controls_free(&mut self) {
        self.op(EE_SCREENS_SEET_OP_CONTROLS_FREE, 0, 0, 0, 0);
    }
    /// The scenario's entry `k`: its pointer and its numbers'.
    pub(crate) fn entry(&mut self, k: i32) -> Option<(usize, usize)> {
        let mut out = [0usize; 2];
        self.call(EE_SCREENS_SEET_OP_ENTRY, [0, out.as_mut_ptr() as usize], [0, 0, k, 0], [0.0; 4]);
        (out[0] != 0).then_some((out[0], out[1]))
    }
    /// The list operation `op` (`EE_SCREENS_SEOB_SM_*`) on the entry `k`.
    pub(crate) fn list(&mut self, op: i32, k: i32) {
        self.op(EE_SCREENS_SEET_OP_LIST, 0, 0, op, k);
    }
    pub(crate) fn get_names(&mut self, names: usize) {
        self.call(EE_SCREENS_SEET_OP_GET_NAMES, [names, 0], [0; 4], [0.0; 4]);
    }
    pub(crate) fn tscreen(&mut self, what: i32) {
        self.op(EE_SCREENS_SEET_OP_TSCREEN, 0, 0, what, 0);
    }
    pub(crate) fn text_box(&mut self) -> Vec<u8> {
        self.read(EE_SCREENS_SEET_OP_TEXT_BOX, 0, [0; 4], 1).unwrap_or_default()
    }
    pub(crate) fn set_text_box(&mut self, text: &[u8]) {
        self.call(EE_SCREENS_SEET_OP_SET_TEXT_BOX, [0, text.as_ptr() as usize], [0, 0, text.len() as i32, 0], [0.0; 4]);
    }
    pub(crate) fn player_count(&mut self) -> i32 {
        self.op(EE_SCREENS_SEET_OP_PLAYER_COUNT, 0, 0, 0, 0)
    }
    pub(crate) fn set_player_count(&mut self, k: u32, n: i32) {
        self.op(EE_SCREENS_SEET_OP_SET_PLAYER_COUNT, PLAYER, k, n, 0);
    }
    /// The combo box `combo`'s selection, `None` when it has none.
    pub(crate) fn handle_selection(&mut self, combo: usize) -> Option<u32> {
        let (s, _, _) = self.call(EE_SCREENS_SEET_OP_HANDLE_SELECTION, [combo, 0], [0; 4], [0.0; 4]);
        (s >= 0).then_some(s as u32)
    }
}

/// A yellow label of the effect screen: `mLabels[k]` at (x, y), w by h, the string `id`, shown.
pub(crate) fn label<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>, k: u32, id: u32, x: f32, y: f32, w: f32, h: f32) {
    c.set_rect(LABEL, k, x, y, w, h);
    c.text_id(LABEL, k, id);
    c.show(LABEL, k);
}

/// A radio button's or check box's text (the string `id` in `gU531934`), yellow and transparent.
pub(crate) fn radio_text<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>, a: u32, k: u32, id: u32) {
    c.text_font(a, k, id, FONT_534);
    c.yellow(a, k);
}

/// A radio button of the effect screen at (x, y): its text, yellow, located, shown.
pub(crate) fn radio<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>, k: u32, id: u32, x: f32, y: f32) {
    radio_text(c, RADIO, k, id);
    c.locate(RADIO, k, x, y);
    c.show(RADIO, k);
}

/// The first word of a string (up to its first space when that is past the start), as the types' default names.
pub(crate) fn first_word(text: &[u8]) -> &[u8] {
    match text.iter().position(|&b| b == b' ') {
        Some(p) if p > 0 => &text[..p],
        _ => text,
    }
}

/// Whether the effect still has a default `<...>` name.
pub(crate) fn default_named<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>) -> bool {
    c.effect_text(EE_SCREENS_SEET_S_NAME).first() == Some(&b'<')
}

/// The new name into the effect and the trigger tab's name box.
pub(crate) fn rename<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>, name: &[u8]) {
    c.set_effect_text(EE_SCREENS_SEET_S_NAME, name);
    c.set_name_box(name);
}

/// `"<" + first word of the string id + ">"` (the media, game and AI types' names).
pub(crate) fn rename_by_action<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>, id: u32) {
    if default_named(c) {
        let s = c.string(id);
        let mut name = b"<".to_vec();
        name.extend_from_slice(first_word(&s));
        name.push(b'>');
        rename(c, &name);
    }
}

/// A player's name as the triggers write it: the string of players 0..=6, else the player format of `p - 6`.
pub(crate) fn player_name<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>, p: u32) -> Vec<u8> {
    if p <= 6 {
        c.string(p + IDS_TRIGGER_PLAYER_FIRST)
    } else {
        c.format_number(IDS_TRIGGER_PLAYER, p.wrapping_sub(6) as i32)
    }
}

// ---- the effect types

/// A family's action: the current sub-type (`mCurrentType`) and the action combo box's event (`mAction`).
#[derive(Debug, Default)]
pub struct Family {
    pub current: Cell<u32>,
    pub event: usize,
}

impl Family {
    fn new<H: SeplHost + ?Sized>(c: &mut Fx<H>) -> Family {
        Family { current: Cell::new(0), event: c.screen_event(EE_SCREENS_SE_CHANGE_EFFECT_ACTION) }
    }
}

/// Every effect type (`EESETSEffect::mTypes`): no effect, object, player, trigger, media, game, AI.
#[derive(Debug, Default)]
pub struct EffectTypes {
    pub object: crate::screens::editor_effects_object::ObjectTypes,
    pub player: crate::screens::editor_effects_player::PlayerTypes,
    pub media: crate::screens::editor_effects_more::MediaTypes,
    pub game: crate::screens::editor_effects_more::GameTypes,
    pub ai: crate::screens::editor_effects_more::AiTypes,
}

impl EffectTypes {
    /// The constructors in `EESETSEffect`'s order (object, player, media, game, AI make their action event; the
    /// object family's stance type makes one more).
    pub fn new<H: SeplHost + ?Sized>(c: &mut Fx<H>) -> EffectTypes {
        let object = crate::screens::editor_effects_object::ObjectTypes::new(c);
        let player =
            crate::screens::editor_effects_player::PlayerTypes { family: Family::new(c), ..Default::default() };
        let media = crate::screens::editor_effects_more::MediaTypes { family: Family::new(c) };
        let game = crate::screens::editor_effects_more::GameTypes { family: Family::new(c), ..Default::default() };
        let ai = crate::screens::editor_effects_more::AiTypes { family: Family::new(c), ..Default::default() };
        EffectTypes { object, player, media, game, ai }
    }

    /// The destructors: each family's events released (the object family's: its stance type's first).
    pub fn destroy<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>) {
        c.release(self.object.stance_event);
        c.release(self.object.family.event);
        c.release(self.player.family.event);
        c.release(self.media.family.event);
        c.release(self.game.family.event);
        c.release(self.ai.family.event);
    }

    /// The type `ty`'s slot (`EE_SCREENS_SEET_CHANGE_ACTION` ...).
    pub fn call<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, ty: u32, slot: u32) {
        match ty {
            0 => no_effect(c, slot),
            1 => self.object.call(c, slot),
            2 => self.player.call(c, slot),
            3 => trigger(c, slot),
            4 => self.media.call(c, slot),
            5 => self.game.call(c, slot),
            6 => self.ai.call(c, slot),
            _ => {}
        }
    }
}

/// `kSETSEffectTitleLabel`, `kSETSEffectTypeComboBox`, `kSETriggerScreenNameEditBox`, `NameLabel`,
/// `DuplicateButton`, `DeleteButton`.
pub const EE_SCREENS_SEET_NO_EFFECT_CONTROLS: [i32; 6] = [0x960, 0x961, 0x7db, 0x7dc, 0x7d6, 0x7d7];

/// `EESETSETNoEffect`: its Show hides the effect title, type combo box and the entry's name, duplicate and delete
/// controls; its VirtualC shows them again.
fn no_effect<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    let shown = match slot {
        EE_SCREENS_SEET_HIDE => true,
        EE_SCREENS_SEET_SHOW => false,
        _ => return,
    };
    for id in EE_SCREENS_SEET_NO_EFFECT_CONTROLS {
        c.form_display(id, shown);
    }
}

/// `EESETSETTrigger`: a trigger switched to a state.
fn trigger<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_CHANGE_ACTION => {
            trigger(c, EE_SCREENS_SEET_STORE);
            trigger(c, EE_SCREENS_SEET_UPDATE_NAME);
            let name = c.name_box();
            c.set_effect_text(EE_SCREENS_SEET_S_NAME, &name);
        }
        EE_SCREENS_SEET_STORE => {
            if let Some(s) = c.selection(SCEN, 0) {
                c.e().mTrigger = s;
            }
            if let Some(s) = c.selection(ECOMBO, 1) {
                c.e().mTriggerState = s;
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 2);
            c.hide(LABEL, 0);
            c.hide(SCEN, 0);
            c.hide(LABEL, 1);
            c.hide(ECOMBO, 1);
        }
        EE_SCREENS_SEET_LOAD => {
            let (t, s) = (c.e().mTrigger, c.e().mTriggerState);
            c.select_silent(SCEN, 0, t);
            c.select_silent(ECOMBO, 1, s);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 2, IDS_TRIGGER_EFFECT, 4.0, 616.0, 206.0, 20.0);
            label(c, 0, IDS_TRIGGER_SCREEN_TAB_TRIGGER, 10.0, 641.0, 200.0, 20.0);
            c.locate(SCEN, 0, 10.0, 666.0);
            c.width(SCEN, 0, 200);
            c.clear(SCEN, 0);
            c.scen_mode(0, MODE_TRIGGER);
            c.fill_names(SCEN, 0, NAMES_TRIGGERS, false);
            c.show(SCEN, 0);
            let ev = c.screen_event(EE_SCREENS_SE_CHANGE_EFFECT_ACTION);
            c.event_set(SCEN, 0, T_SELECTION, ev);
            c.release(ev);
            let x = c.right(LABEL, 0) + 10.0;
            label(c, 1, IDS_STATE, x, 641.0, 200.0, 20.0);
            c.locate(ECOMBO, 1, x, 666.0);
            c.width(ECOMBO, 1, 200);
            c.clear(ECOMBO, 1);
            c.append_ids(ECOMBO, 1, IDS_TRIGGER_SWITCH_STATE_FIRST, IDS_NOT_FIRED);
            c.show(ECOMBO, 1);
        }
        EE_SCREENS_SEET_UPDATE_NAME if default_named(c) => {
            let t = c.e().mTrigger;
            let name = match c.trigger_name(t) {
                Some(n) => n,
                None => c.string(IDS_NONE),
            };
            // the original reads an unset selection when there is none: no state word then
            let state = match c.selection(ECOMBO, 1) {
                Some(s) => c.string(s.wrapping_add(IDS_TRIGGER_SWITCH_STATE_FIRST)),
                None => Vec::new(),
            };
            let mut text = b"<".to_vec();
            text.extend_from_slice(&name);
            text.push(b' ');
            text.extend_from_slice(&state);
            text.push(b'>');
            rename(c, &text);
        }
        _ => {}
    }
}

/// The family's title (`mLabels[0]` at (4, 616), `title_w` wide), its "Action" label and the action combo box
/// (10, 666) filled with the strings `first..=last`.
pub(crate) fn show_actions<H: SeplHost + ?Sized>(c: &mut Fx<H>, title: u32, title_w: f32, first: u32, last: u32) {
    label(c, 0, title, 4.0, 616.0, title_w, 20.0);
    label(c, 1, IDS_ACTION, 10.0, 641.0, 200.0, 20.0);
    c.locate(COMBO, 0, 10.0, 666.0);
    c.width(COMBO, 0, 200);
    c.clear(COMBO, 0);
    c.append_ids(COMBO, 0, first, last);
    c.show(COMBO, 0);
}

/// The action combo box set to the family's action with its event held off, then given the family's event back.
pub(crate) fn load_action<H: SeplHost + ?Sized>(c: &mut Fx<H>, family: &Family, action: u32) {
    family.current.set(action);
    c.event_null(COMBO, 0, T_SELECTION);
    c.select(COMBO, 0, action);
    c.event_set(COMBO, 0, T_SELECTION, family.event);
}

/// A copy of the `RefCell`'d list (never borrowed across a host call).
pub(crate) fn snapshot<T: Clone>(v: &RefCell<Vec<T>>) -> Vec<T> {
    v.borrow().clone()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn first_word_stops_at_a_space_past_the_start() {
        assert_eq!(first_word(b"Kill Object"), b"Kill");
        assert_eq!(first_word(b"Remove"), b"Remove");
        assert_eq!(first_word(b" lead"), b" lead");
        assert_eq!(first_word(b""), b"");
    }

    /// The private string ids equal `ee/EEStringIDs.h`'s.
    #[test]
    fn string_ids_match_the_header() {
        let header = std::fs::read_to_string(concat!(env!("CARGO_MANIFEST_DIR"), "/../../../include/ee/EEStringIDs.h"))
            .expect("include/ee/EEStringIDs.h");
        let ids = include_str!("editor_effects_ids.rs");
        let mut checked = 0;
        for line in ids.lines() {
            let Some(rest) = line.strip_prefix("pub(crate) const ") else { continue };
            let (name, value) = rest.split_once(": u32 = ").unwrap();
            let value = value.trim_end_matches(';');
            let want = header
                .lines()
                .find_map(|l| {
                    let mut w = l.split_whitespace();
                    (w.next() == Some("#define") && w.next() == Some(name)).then(|| w.next().unwrap().to_string())
                })
                .unwrap_or_else(|| panic!("{name} not in the header"));
            assert_eq!(want, value, "{name}");
            checked += 1;
        }
        assert!(checked > 100);
    }
}
