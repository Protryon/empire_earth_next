//! The scenario editor's trigger tab object screen whole in Rust (`EESETSObject`, once `EESETSObject.cpp`): the
//! controls it makes (records for the shared maker of the player tab, `EeScreensSeplCtl`), its two modes (units
//! picked on the map, or an object specification), the filter check boxes, the class and action lists, the store
//! of the controls into the selected scenario object and their load from it, the generated `<...>` names, the
//! entry list's operations. The controls stay C++ objects; the object holds them as handles and drives them
//! through the shared control ops (`ee_screenscpp_sepl`, below 100) and its own (`ee_screenscpp_seob`, from 100:
//! the scenario object, the editor's selection, the unit classes and actions).
//!
//! A host call can come back into the object (the mode radios' events refresh it, a load's radio check updates its
//! selection): it is only used through `&self`, its few changing values are `Cell`s and `RefCell`s that are never
//! borrowed across a host call.

use core::cell::{Cell, RefCell};

use crate::editor_players::*;
use crate::scenario_editor::{
    EE_SCREENS_SE_OBJECT_CHECK, EE_SCREENS_SE_OBJECT_DETAIL, EE_SCREENS_SE_OBJECT_UNIT_SELECT,
};

// ---- the object screen's host ops (`ee_screenscpp_seob`)

/// The selection tool: p0 -> three handles (the click, the lasso, the `EESEUICTriggerSelectUnits` made at the
/// index i0 on the form p2, added to group 0).
pub const EE_SCREENS_SEOB_OP_SELECT_UNITS: u32 = 100;
/// The click p0 and the lasso p1 released (`Dereference`).
pub const EE_SCREENS_SEOB_OP_RELEASE: u32 = 101;
/// -> the unit set manager's size / whether its set i0 is listed in the editor.
pub const EE_SCREENS_SEOB_OP_UNIT_SET_COUNT: u32 = 102;
pub const EE_SCREENS_SEOB_OP_UNIT_SET_SHOWN: u32 = 103;
/// The class list: the scenario-selectable unit classes (no projectile or missile) after class 0, sorted by name
/// (the original's `std::sort` and comparator), into the buffer p0 of i0 entries -> their count.
pub const EE_SCREENS_SEOB_OP_UNIT_TYPES: u32 = 104;
/// The combo box p0 emptied, then "Any Class" and the names of the classes p1[1..i0].
pub const EE_SCREENS_SEOB_OP_CLASS_NAMES: u32 = 105;
/// The unit action i0's name appended to the combo box p0 (none for an unknown action).
pub const EE_SCREENS_SEOB_OP_ACTION_APPEND: u32 = 106;
/// The scenario objects' list: i0 the operation (`EE_SCREENS_SEOB_SM_*`) on the entry i1.
pub const EE_SCREENS_SEOB_OP_SM: u32 = 107;
/// The editor's unit selection cleared (the form of the control p0).
pub const EE_SCREENS_SEOB_OP_CLEAR_SELECTION: u32 = 108;
/// -> the player tab's player count (the form of the control p0).
pub const EE_SCREENS_SEOB_OP_PLAYER_COUNT: u32 = 109;
/// The trigger screen's description hidden and its alphabetical sort shown.
pub const EE_SCREENS_SEOB_OP_TRIGGER_TEXT: u32 = 110;
/// The player combo p0: i0 the member (`EE_SCREENS_SEOB_PC_*`), i1 its value -> the selected player.
pub const EE_SCREENS_SEOB_OP_PLAYER_COMBO: u32 = 111;
/// The scenario's names i0 (`EE_SCREENS_SEOB_NAMES_*`) appended to the combo boxes p0 and p1 (p1 may be none) /
/// set into the vector p0 (`GetNames`).
pub const EE_SCREENS_SEOB_OP_APPEND_NAMES: u32 = 112;
pub const EE_SCREENS_SEOB_OP_GET_NAMES: u32 = 113;
/// The scenario object i0 (1 from the first): its field i1 (`EE_SCREENS_SEOB_F_*`) -> / set to i2.
pub const EE_SCREENS_SEOB_OP_OBJ_GET: u32 = 114;
pub const EE_SCREENS_SEOB_OP_OBJ_SET: u32 = 115;
/// The scenario object i0 named from the trigger screen's name / given the units p0[..i1] / -> whether it exists.
pub const EE_SCREENS_SEOB_OP_OBJ_NAME_FROM_SCREEN: u32 = 116;
pub const EE_SCREENS_SEOB_OP_OBJ_SET_UNITS: u32 = 117;
pub const EE_SCREENS_SEOB_OP_OBJ_EXISTS: u32 = 118;
/// The trigger screen's name box set to the scenario object i0's name.
pub const EE_SCREENS_SEOB_OP_NAME_TO_EDIT: u32 = 119;
/// The scenario object i0's existing units selected in the editor (the form of the control p0).
pub const EE_SCREENS_SEOB_OP_SELECT_OBJECT_UNITS: u32 = 120;
/// -> the unit action named by the action combo p0's selected line.
pub const EE_SCREENS_SEOB_OP_ACTION_ID: u32 = 121;
/// -> the first line of the action combo p0 naming the action i0 (its line count when none does).
pub const EE_SCREENS_SEOB_OP_ACTION_INDEX: u32 = 122;
/// The unit list p0 refilled with the names of the editor's selected units (the form p1) -> their count.
pub const EE_SCREENS_SEOB_OP_UNIT_LIST_FILL: u32 = 123;
/// -> the editor's (the form p0) selected unit i0's id / the number of selected units.
pub const EE_SCREENS_SEOB_OP_SELECTED_UNIT_ID: u32 = 124;
pub const EE_SCREENS_SEOB_OP_SELECTED_COUNT: u32 = 125;
/// A text (`EE_SCREENS_SEOB_TEXT_*` i0 with i1, the control p1) into the buffer p0 of i2 bytes -> its length
/// (the whole length even when longer than the buffer; -1 for none).
pub const EE_SCREENS_SEOB_OP_TEXT: u32 = 126;
/// The scenario object i0 named the bytes p0[..i1] (`SetName`), the trigger screen's name box set to it.
pub const EE_SCREENS_SEOB_OP_OBJ_SET_NAME: u32 = 127;
/// -> whether the game view holds the point (f0, f1) and a mouse button was just pressed.
pub const EE_SCREENS_SEOB_OP_VIEW_PRESS: u32 = 128;
/// The selection tool p0's selection started at (f0, f1).
pub const EE_SCREENS_SEOB_OP_START_SELECTION: u32 = 129;

/// The scenario list operations: sort, move down, move up, delete, duplicate, new.
pub const EE_SCREENS_SEOB_SM_SORT: i32 = 0;
pub const EE_SCREENS_SEOB_SM_DOWN: i32 = 1;
pub const EE_SCREENS_SEOB_SM_UP: i32 = 2;
pub const EE_SCREENS_SEOB_SM_DELETE: i32 = 3;
pub const EE_SCREENS_SEOB_SM_DUPLICATE: i32 = 4;
pub const EE_SCREENS_SEOB_SM_NEW: i32 = 5;

/// The player combo's members: `SetPlayerCount`, `SetFlags`, `GetSelectedPlayer`, `SetSelectedPlayer`.
pub const EE_SCREENS_SEOB_PC_COUNT: i32 = 0;
pub const EE_SCREENS_SEOB_PC_FLAGS: i32 = 1;
pub const EE_SCREENS_SEOB_PC_GET: i32 = 2;
pub const EE_SCREENS_SEOB_PC_SET: i32 = 3;

/// The scenario's names: the areas, every object, the user's objects.
pub const EE_SCREENS_SEOB_NAMES_AREAS: i32 = 0;
pub const EE_SCREENS_SEOB_NAMES_OBJECTS: i32 = 1;
pub const EE_SCREENS_SEOB_NAMES_USER_OBJECTS: i32 = 2;

/// The texts: the scenario object i1's name (by its own index, not from the selection), the editor's selected
/// unit i1's class name, the resource string i1, the line i1 of the combo box p1.
pub const EE_SCREENS_SEOB_TEXT_OBJECT_NAME: i32 = 0;
pub const EE_SCREENS_SEOB_TEXT_UNIT_CLASS: i32 = 1;
pub const EE_SCREENS_SEOB_TEXT_STRING: i32 = 2;
pub const EE_SCREENS_SEOB_TEXT_LINE: i32 = 3;

/// The scenario object's fields (`EESMObject`): owner, family, class, min and max counts, area, near mode, near
/// object, parent (`SetParent`), attribute, its min and max, visible by, state, selected by; the six checks (in
/// area, near, has attribute, visible by, has state, selected by); select on map, dynamic, attribute percent.
pub const EE_SCREENS_SEOB_F_OWNER: i32 = 0;
pub const EE_SCREENS_SEOB_F_FAMILY: i32 = 1;
pub const EE_SCREENS_SEOB_F_CLASS: i32 = 2;
pub const EE_SCREENS_SEOB_F_MIN: i32 = 3;
pub const EE_SCREENS_SEOB_F_MAX: i32 = 4;
pub const EE_SCREENS_SEOB_F_AREA: i32 = 5;
pub const EE_SCREENS_SEOB_F_NEAR_MODE: i32 = 6;
pub const EE_SCREENS_SEOB_F_NEAR_OBJECT: i32 = 7;
pub const EE_SCREENS_SEOB_F_PARENT: i32 = 8;
pub const EE_SCREENS_SEOB_F_ATTRIBUTE: i32 = 9;
pub const EE_SCREENS_SEOB_F_ATTRIBUTE_MIN: i32 = 10;
pub const EE_SCREENS_SEOB_F_ATTRIBUTE_MAX: i32 = 11;
pub const EE_SCREENS_SEOB_F_VISIBLE_BY: i32 = 12;
pub const EE_SCREENS_SEOB_F_STATE: i32 = 13;
pub const EE_SCREENS_SEOB_F_SELECTED_BY: i32 = 14;
pub const EE_SCREENS_SEOB_F_IN_AREA_CHECK: i32 = 15;
pub const EE_SCREENS_SEOB_F_NEAR_CHECK: i32 = 16;
pub const EE_SCREENS_SEOB_F_HAS_ATTRIBUTE_CHECK: i32 = 17;
pub const EE_SCREENS_SEOB_F_VISIBLE_BY_CHECK: i32 = 18;
pub const EE_SCREENS_SEOB_F_HAS_STATE_CHECK: i32 = 19;
pub const EE_SCREENS_SEOB_F_SELECTED_BY_CHECK: i32 = 20;
pub const EE_SCREENS_SEOB_F_SELECT_ON_MAP: i32 = 21;
pub const EE_SCREENS_SEOB_F_DYNAMIC: i32 = 22;
pub const EE_SCREENS_SEOB_F_PERCENT: i32 = 23;

// ---- the language's strings (`ee/EEStringIDs.h`; the host `static_assert`s them)

pub const EE_SCREENS_SEOB_IDS_ANY_CLASS: u32 = 13000;
pub const EE_SCREENS_SEOB_IDS_NONE: u32 = 13001;
pub const EE_SCREENS_SEOB_IDS_SE_UNIT_GROUP_UNITS: u32 = 20531;
pub const EE_SCREENS_SEOB_IDS_FAMILY: u32 = 20538;
/// Select On Map, Object Specification, Owner, Class, -, Min, Max, Dynamic, In Area, Visible By, Has State,
/// Selected By, Has Attribute: 30308..30320.
pub const EE_SCREENS_SEOB_IDS_SELECT_ON_MAP: u32 = 30308;
pub const EE_SCREENS_SEOB_IDS_UNIT_SELECTED_LIST: u32 = 30322;
pub const EE_SCREENS_SEOB_IDS_TRIGGER_EDITOR_USE_OBJECT: u32 = 30375;
pub const EE_SCREENS_SEOB_IDS_TRIGGER_PROXIMITY_FIRST: u32 = 29001;
pub const EE_SCREENS_SEOB_IDS_TRIGGER_ATTRIBUTE_FIRST: u32 = 29100;

const IDS_OBJECT_SPECIFICATION: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 1;
const IDS_OWNER: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 2;
const IDS_CLASS: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 3;
const IDS_MIN: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 5;
const IDS_MAX: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 6;
const IDS_DYNAMIC: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 7;
const IDS_IN_AREA: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 8;
const IDS_VISIBLE_BY: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 9;
const IDS_HAS_STATE: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 10;
const IDS_SELECTED_BY: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 11;
const IDS_HAS_ATTRIBUTE: u32 = EE_SCREENS_SEOB_IDS_SELECT_ON_MAP + 12;

/// The unit groups listed before the unit sets in the family combo box.
pub const EE_SCREENS_SEOB_FAMILIES: u32 = 0x75;
/// The unit attributes the attribute combo box lists.
const ATTRIBUTES: i32 = 0x4b;

/// The unit actions the state combo box lists (those with a name), in id order.
pub const EE_SCREENS_SEOB_ACTIONS: [i32; 24] = [
    1000, 1002, 1003, 1006, 1008, 1009, 1012, 1013, 1017, 2001, 2004, 2011, 2017, 2021, 4002, 4004, 4012, 4026, 5000,
    6001, 6002, 10003, 15000, 15001,
];

/// The control ids (`kSETSObject*`; the ones the check boxes are told apart by).
pub const EE_SCREENS_SEOB_AREA_CHECK: u32 = 0x846;
pub const EE_SCREENS_SEOB_NEAR_CHECK: u32 = 0x848;
pub const EE_SCREENS_SEOB_ATTRIBUTE_CHECK: u32 = 0x84a;
pub const EE_SCREENS_SEOB_VISIBLE_BY_CHECK: u32 = 0x84e;
pub const EE_SCREENS_SEOB_HAS_STATE_CHECK: u32 = 0x850;
pub const EE_SCREENS_SEOB_SELECTED_BY_CHECK: u32 = 0x852;
pub const EE_SCREENS_SEOB_SELECT_UNITS: u32 = 0x855;

/// The object's control group.
const GROUP: u32 = 10;

/// The trigger screens' combo box classes (`EE_SCREENS_SEPL_LIST_COMBO`'s extra[0]).
pub const EE_SCREENS_SEPL_COMBO_PLAIN: i32 = 0;
pub const EE_SCREENS_SEPL_COMBO_OBJECT: i32 = 1;
pub const EE_SCREENS_SEPL_COMBO_PLAYER: i32 = 2;
pub const EE_SCREENS_SEPL_COMBO_SCENARIO: i32 = 3;
/// `EESETSTriggerUIComboBox` (its kind in `number`).
pub const EE_SCREENS_SEPL_COMBO_TRIGGER: i32 = 4;
/// Its lines (extra[1]): none, the unit families and the listed unit sets, the resource strings from `string`
/// (`number` of them).
pub const EE_SCREENS_SEPL_COMBO_LINES_NONE: i32 = 0;
pub const EE_SCREENS_SEPL_COMBO_LINES_FAMILIES: i32 = 1;
pub const EE_SCREENS_SEPL_COMBO_LINES_STRINGS: i32 = 2;

/// The trigger screens' screen event triggers (`EE_SCREENS_SEPL_OP_SCREEN_EVENT`'s i1 bits): checked, unchecked,
/// selection changed, button up.
pub const EE_SCREENS_SEPL_ON_CHECKED: i32 = 1;
pub const EE_SCREENS_SEPL_ON_UNCHECKED: i32 = 2;
pub const EE_SCREENS_SEPL_ON_SELECTION: i32 = 4;
pub const EE_SCREENS_SEPL_ON_BUTTON_UP: i32 = 8;

/// The object screen (`EESETSObject`).
#[derive(Debug, Default)]
pub struct Object {
    pub form: usize,
    pub mouse_click: usize,
    pub lasso: usize,
    pub select_units: usize,
    pub units_radio: usize,
    pub type_radio: usize,
    pub min_label: usize,
    pub min_edit: usize,
    pub max_label: usize,
    pub max_edit: usize,
    pub dynamic: usize,
    pub player_label: usize,
    pub player: usize,
    pub group_label: usize,
    pub group_combo: usize,
    pub type_label: usize,
    pub type_combo: usize,
    pub parent_label: usize,
    pub parent_combo: usize,
    pub unit_list_label: usize,
    pub unit_list: usize,
    pub area_check: usize,
    pub area_combo: usize,
    pub near_check: usize,
    pub near_mode: usize,
    pub near_object: usize,
    pub attribute_check: usize,
    pub attribute_combo: usize,
    pub attribute_min_label: usize,
    pub attribute_min: usize,
    pub attribute_max_label: usize,
    pub attribute_max: usize,
    pub percent_label: usize,
    pub percent: usize,
    pub visible_by_check: usize,
    pub player_bc: usize,
    pub has_state_check: usize,
    pub action_combo: usize,
    pub selected_by_check: usize,
    pub player_cc: usize,
    /// the unit sets listed after the unit groups (their indexes in the unit set manager)
    pub unit_sets: Vec<i32>,
    /// the units picked on the map (their ids)
    pub unit_ids: RefCell<Vec<u32>>,
    /// the class combo box's classes (0 first), built on the first `Activate`
    pub unit_types: RefCell<Vec<u32>>,
    pub select_on_map: Cell<bool>,
}

/// A yellow label of the object screen: the screen's font at its height, unless `h` is given.
fn label(id: u32, string: u32, x: f32, y: f32, w: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 0.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
        .bg(YELLOW_BG)
        .group(GROUP)
}

/// A 20-pixel-high label at its own height.
fn label20(id: u32, string: u32, x: f32, y: f32, w: f32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 20.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG)
        .bg(YELLOW_BG)
        .group(GROUP)
}

/// A combo box of the trigger screens (`kind` its class, `lines` its lines) at (x, y), 20 pixels high.
fn combo(kind: i32, id: u32, x: f32, y: f32, w: f32, lines: i32, border: i32, dirty: bool) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LIST_COMBO, id, x, y, w, 20.0)
        .font(EE_SCREENS_SEPL_FONT_530)
        .flags(EE_SCREENS_SEPL_FG)
        .bg(1.0)
        .bg2(YELLOW_BG)
        .group(GROUP)
        .extra([kind, lines, border, dirty as i32])
}

/// A tri-state check box (20 pixels high); `yellow`: its text yellow over the yellow texts' background.
fn tri_state(id: u32, string: u32, x: f32, y: f32, w: f32, yellow: bool) -> EeScreensSeplCtl {
    let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_TRISTATE, id, x, y, w, 20.0).group(GROUP);
    let c = if string != 0 { c.string(string) } else { c };
    if yellow {
        c.flags(EE_SCREENS_SEPL_FG).bg(YELLOW_BG)
    } else {
        c
    }
}

pub(crate) fn screen_event<H: SeplHost + ?Sized>(h: &mut H, control: usize, code: u32, on: i32) {
    call(h, EE_SCREENS_SEPL_OP_SCREEN_EVENT, [control, 0, 0], [code as i32, on, 0, 0]);
}

fn tri<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> i32 {
    call(h, EE_SCREENS_SEPL_OP_TRI_STATE, [control, 0, 0], [0; 4])
}

fn set_tri<H: SeplHost + ?Sized>(h: &mut H, control: usize, state: i32) {
    call(h, EE_SCREENS_SEPL_OP_SET_TRI_STATE, [control, 0, 0], [state, 0, 0, 0]);
}

fn radio_checked<H: SeplHost + ?Sized>(h: &mut H, control: usize) -> bool {
    call(h, EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED, [control, 0, 0], [0; 4]) != 0
}

fn line_count<H: SeplHost + ?Sized>(h: &mut H, combo: usize) -> u32 {
    call(h, EE_SCREENS_SEPL_OP_LINE_COUNT, [combo, 0, 0], [0; 4]) as u32
}

fn atoi<H: SeplHost + ?Sized>(h: &mut H, edit: usize) -> i32 {
    call(h, EE_SCREENS_SEPL_OP_EDIT_ATOI, [edit, 0, 0], [0; 4])
}

pub(crate) fn text_as_int<H: SeplHost + ?Sized>(h: &mut H, edit: usize) -> i32 {
    call(h, EE_SCREENS_SEPL_OP_EDIT_INT, [edit, 0, 0], [0; 4])
}

/// `UIEditBox::SetText(unsigned long)` ("%lu" of the value as the original's conversion).
pub(crate) fn edit_number<H: SeplHost + ?Sized>(h: &mut H, edit: usize, value: i32) {
    call(h, EE_SCREENS_SEPL_OP_EDIT_NUMBER, [edit, 0, 0], [value, 0, 0, 0]);
}

/// A text from the host (`EE_SCREENS_SEOB_OP_TEXT`); `None` for none.
fn text<H: SeplHost + ?Sized>(h: &mut H, kind: i32, arg: i32, control: usize) -> Option<Vec<u8>> {
    let mut buf = vec![0u8; 256];
    loop {
        let p = [buf.as_mut_ptr() as usize, control, 0];
        let n = call(h, EE_SCREENS_SEOB_OP_TEXT, p, [kind, arg, buf.len() as i32, 0]);
        if n < 0 {
            return None;
        }
        if (n as usize) <= buf.len() {
            buf.truncate(n as usize);
            return Some(buf);
        }
        buf = vec![0u8; n as usize];
    }
}

fn obj_get<H: SeplHost + ?Sized>(h: &mut H, entry: u32, field: i32) -> i32 {
    call(h, EE_SCREENS_SEOB_OP_OBJ_GET, [0; 3], [entry as i32, field, 0, 0])
}

fn obj_set<H: SeplHost + ?Sized>(h: &mut H, entry: u32, field: i32, value: i32) {
    call(h, EE_SCREENS_SEOB_OP_OBJ_SET, [0; 3], [entry as i32, field, value, 0]);
}

fn player_combo<H: SeplHost + ?Sized>(h: &mut H, combo: usize, member: i32, value: i32) -> i32 {
    call(h, EE_SCREENS_SEOB_OP_PLAYER_COMBO, [combo, 0, 0], [member, value, 0, 0])
}

impl Object {
    /// `EESETSObject`'s constructor on the form: the selection tool, then the controls (group 10) from the control
    /// index `*index` (advanced), in the original's order; the specification mode first (`SetDetailMode(true)`:
    /// units picked on the map).
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, form: usize, index: &mut u32) -> Object {
        let mut o = Object { form, ..Default::default() };
        let mut tools = [0usize; 3];
        call(h, EE_SCREENS_SEOB_OP_SELECT_UNITS, [tools.as_mut_ptr() as usize, 0, form], [*index as i32, 0, 0, 0]);
        [o.mouse_click, o.lasso, o.select_units] = tools;
        let mut m = Maker { h, form, index: *index + 1 };
        let radio = |id, string, y, on| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_RADIO, id, 329.0, y, 200.0, 20.0)
                .string(string)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([on as i32, 0, 0, 0])
        };
        o.units_radio = m.make(radio(0x834, EE_SCREENS_SEOB_IDS_SELECT_ON_MAP, 504.0, true));
        screen_event(m.h, o.units_radio, EE_SCREENS_SE_OBJECT_DETAIL, EE_SCREENS_SEPL_ON_CHECKED);
        let y = rect(m.h, o.units_radio)[3] + 1.0;
        o.type_radio = m.make(radio(0x835, IDS_OBJECT_SPECIFICATION, y, false));
        screen_event(m.h, o.type_radio, EE_SCREENS_SE_OBJECT_DETAIL, EE_SCREENS_SEPL_ON_CHECKED);
        // the counts: a label, its box two pixels below (the screen font's height + 6)
        let count_edit = |id, x, y| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_OBJ_EDIT, id, x, y, 50.0, 6.0)
                .flags(EE_SCREENS_SEPL_FG | EE_SCREENS_SEPL_FONT_HEIGHT)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([5, 1, 0, 0])
        };
        o.min_label = m.make(label(0x839, IDS_MIN, 329.0, 566.0, 75.0));
        let y = rect(m.h, o.min_label)[3] + 2.0;
        o.min_edit = m.make(count_edit(0x837, 341.0, y));
        o.max_label = m.make(label(0x83a, IDS_MAX, 429.0, 566.0, 75.0));
        let y = rect(m.h, o.max_label)[3] + 2.0;
        o.max_edit = m.make(count_edit(0x838, 441.0, y));
        let y = rect(m.h, o.type_radio)[3] + 2.0;
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, 0x844, 379.0, y, 100.0, 20.0)
            .string(IDS_DYNAMIC)
            .flags(EE_SCREENS_SEPL_FG)
            .bg(YELLOW_BG)
            .group(GROUP);
        o.dynamic = m.make(c);
        // the specification column: owner, family, class, parent
        const P: i32 = EE_SCREENS_SEPL_COMBO_PLAYER;
        const OB: i32 = EE_SCREENS_SEPL_COMBO_OBJECT;
        const NO: i32 = EE_SCREENS_SEPL_COMBO_LINES_NONE;
        o.player_label = m.make(label(0x83e, IDS_OWNER, 589.0, 458.0, 200.0));
        let y = rect(m.h, o.player_label)[3] + 1.0;
        o.player = m.make(combo(P, 0x841, 589.0, y, 200.0, NO, 0, true));
        let y = rect(m.h, o.player)[3] + 3.0;
        o.group_label = m.make(label(0x83e, EE_SCREENS_SEOB_IDS_FAMILY, 589.0, y, 200.0));
        let y = rect(m.h, o.group_label)[3] + 1.0;
        let count = call(m.h, EE_SCREENS_SEOB_OP_UNIT_SET_COUNT, [0; 3], [0; 4]);
        for j in 0..count {
            if call(m.h, EE_SCREENS_SEOB_OP_UNIT_SET_SHOWN, [0; 3], [j, 0, 0, 0]) != 0 {
                o.unit_sets.push(j);
            }
        }
        o.group_combo = m.make(combo(OB, 0x841, 589.0, y, 200.0, EE_SCREENS_SEPL_COMBO_LINES_FAMILIES, 2, true));
        let y = rect(m.h, o.group_combo)[3] + 3.0;
        o.type_label = m.make(label(0x83e, IDS_CLASS, 589.0, y, 200.0));
        let y = rect(m.h, o.type_label)[3] + 1.0;
        o.type_combo = m.make(combo(OB, 0x841, 589.0, y, 200.0, NO, 2, true));
        let y = rect(m.h, o.type_combo)[3] + 3.0;
        o.parent_label = m.make(label(0x83e, EE_SCREENS_SEOB_IDS_TRIGGER_EDITOR_USE_OBJECT, 589.0, y, 200.0));
        let y = rect(m.h, o.parent_label)[3] + 1.0;
        o.parent_combo = m.make(combo(OB, 0x859, 589.0, y, 200.0, NO, 2, true));
        // the units picked on the map, as tall as down to the max count's box
        o.unit_list_label = m.make(label(0x845, EE_SCREENS_SEOB_IDS_UNIT_SELECTED_LIST, 589.0, 458.0, 200.0));
        let y = rect(m.h, o.unit_list_label)[3] + 1.0;
        let bottom = rect(m.h, o.max_edit)[3];
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_UNIT_LIST, 0x836, 589.0, y, 200.0, bottom - y)
            .font(EE_SCREENS_SEPL_FONT_530)
            .flags(EE_SCREENS_SEPL_FG)
            .bg(YELLOW_BG)
            .bg2(YELLOW_BG)
            .group(GROUP);
        o.unit_list = m.make(c);
        screen_event(m.h, o.unit_list, EE_SCREENS_SE_OBJECT_UNIT_SELECT, EE_SCREENS_SEPL_ON_SELECTION);
        // the filters
        const BOTH: i32 = EE_SCREENS_SEPL_ON_CHECKED | EE_SCREENS_SEPL_ON_UNCHECKED;
        o.area_check = m.make(tri_state(EE_SCREENS_SEOB_AREA_CHECK, IDS_IN_AREA, 4.0, 616.0, 100.0, true));
        screen_event(m.h, o.area_check, EE_SCREENS_SE_OBJECT_CHECK, BOTH);
        let c = combo(EE_SCREENS_SEPL_COMBO_SCENARIO, 0x847, 114.0, 616.0, 270.0, NO, 2, true);
        o.area_combo = m.make(c.text(EE_SCREENS_SEPL_TEXT_NONE, SCREEN_AREA, SCREEN_OBJECT));
        o.near_check = m.make(tri_state(EE_SCREENS_SEOB_NEAR_CHECK, 0, 4.0, 652.0, 22.0, false));
        screen_event(m.h, o.near_check, EE_SCREENS_SE_OBJECT_CHECK, BOTH);
        let x = rect(m.h, o.near_check)[2] + 2.0;
        let c =
            combo(EE_SCREENS_SEPL_COMBO_PLAIN, 0x849, x, 652.0, 80.0, EE_SCREENS_SEPL_COMBO_LINES_STRINGS, 2, false);
        o.near_mode = m.make(c.text(EE_SCREENS_SEPL_TEXT_NONE, EE_SCREENS_SEOB_IDS_TRIGGER_PROXIMITY_FIRST, 3));
        o.near_object = m.make(combo(EE_SCREENS_SEPL_COMBO_PLAIN, 0x849, 114.0, 652.0, 270.0, NO, 2, true));
        o.attribute_check =
            m.make(tri_state(EE_SCREENS_SEOB_ATTRIBUTE_CHECK, IDS_HAS_ATTRIBUTE, 4.0, 688.0, 110.0, true));
        screen_event(m.h, o.attribute_check, EE_SCREENS_SE_OBJECT_CHECK, BOTH);
        let r = rect(m.h, o.attribute_check);
        let c = combo(
            EE_SCREENS_SEPL_COMBO_PLAIN,
            0x84b,
            4.0,
            r[3] + 1.0,
            225.0,
            EE_SCREENS_SEPL_COMBO_LINES_STRINGS,
            2,
            true,
        );
        o.attribute_combo =
            m.make(c.text(EE_SCREENS_SEPL_TEXT_NONE, EE_SCREENS_SEOB_IDS_TRIGGER_ATTRIBUTE_FIRST, ATTRIBUTES));
        let attribute_edit = |id, x, y| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, id, x, y, 45.0, 20.0)
                .font(EE_SCREENS_SEPL_FONT_530)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([5, 1, 0, 0])
        };
        let r = rect(m.h, o.attribute_check);
        o.attribute_min_label = m.make(label20(0x84b, IDS_MIN, r[2] + 130.0, r[1], 45.0));
        let y = rect(m.h, o.attribute_min_label)[3] + 1.0;
        o.attribute_min = m.make(attribute_edit(0x84a, r[2] + 130.0, y));
        let r = rect(m.h, o.attribute_check);
        o.attribute_max_label = m.make(label20(0x84d, IDS_MAX, r[2] + 180.0, r[1], 45.0));
        let y = rect(m.h, o.attribute_max_label)[3] + 1.0;
        o.attribute_max = m.make(attribute_edit(0x84c, r[2] + 180.0, y));
        let r = rect(m.h, o.attribute_check);
        let c = label20(0x856, 0, r[2] + 230.0, r[1], 20.0).text(EE_SCREENS_SEPL_TEXT_CHAR, 0, b'%' as i32);
        o.percent_label = m.make(c);
        let y = rect(m.h, o.percent_label)[3] + 1.0;
        let c =
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, 0x857, r[2] + 230.0, y, 20.0, 20.0).bg(YELLOW_BG).group(GROUP);
        o.percent = m.make(c);
        o.visible_by_check =
            m.make(tri_state(EE_SCREENS_SEOB_VISIBLE_BY_CHECK, IDS_VISIBLE_BY, 404.0, 616.0, 100.0, true));
        screen_event(m.h, o.visible_by_check, EE_SCREENS_SE_OBJECT_CHECK, BOTH);
        o.player_bc = m.make(combo(P, 0x84f, 508.0, 616.0, 150.0, NO, 2, true));
        o.has_state_check =
            m.make(tri_state(EE_SCREENS_SEOB_HAS_STATE_CHECK, IDS_HAS_STATE, 404.0, 652.0, 100.0, true));
        screen_event(m.h, o.has_state_check, EE_SCREENS_SE_OBJECT_CHECK, BOTH);
        o.action_combo = m.make(combo(EE_SCREENS_SEPL_COMBO_PLAIN, 0x851, 508.0, 652.0, 150.0, NO, 2, true));
        o.selected_by_check =
            m.make(tri_state(EE_SCREENS_SEOB_SELECTED_BY_CHECK, IDS_SELECTED_BY, 404.0, 688.0, 100.0, true));
        screen_event(m.h, o.selected_by_check, EE_SCREENS_SE_OBJECT_CHECK, BOTH);
        o.player_cc = m.make(combo(P, 0x853, 508.0, 688.0, 150.0, NO, 2, true));
        *index = m.index;
        // SetDetailMode(true)
        o.select_on_map.set(true);
        o.refresh(m.h);
        o
    }

    /// The destructor: every control deleted in the original's order, the click and the lasso released.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [
            self.near_object,
            self.parent_combo,
            self.type_radio,
            self.max_edit,
            self.max_label,
            self.min_edit,
            self.min_label,
            self.dynamic,
            self.player,
            self.player_label,
            self.group_combo,
            self.group_label,
            self.type_combo,
            self.type_label,
            self.parent_label,
            self.unit_list,
            self.unit_list_label,
            self.area_check,
            self.area_combo,
            self.near_check,
            self.attribute_check,
            self.attribute_combo,
            self.visible_by_check,
            self.player_bc,
            self.has_state_check,
            self.action_combo,
            self.selected_by_check,
            self.player_cc,
            self.attribute_min,
            self.attribute_min_label,
            self.attribute_max,
            self.attribute_max_label,
            self.near_mode,
            self.select_units,
            self.units_radio,
            self.percent_label,
            self.percent,
        ] {
            delete(h, c);
        }
        call(h, EE_SCREENS_SEOB_OP_RELEASE, [self.mouse_click, self.lasso, 0], [0; 4]);
    }

    /// The specification column's controls (owner, family, class, parent, counts, dynamic).
    fn specification(&self) -> [usize; 13] {
        [
            self.player,
            self.player_label,
            self.type_label,
            self.type_combo,
            self.parent_label,
            self.parent_combo,
            self.group_combo,
            self.group_label,
            self.min_label,
            self.max_label,
            self.min_edit,
            self.max_edit,
            self.dynamic,
        ]
    }

    /// `Refresh`: the unit list in the pick-on-map mode, else the specification column.
    pub fn refresh<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let map = self.select_on_map.get();
        for c in self.specification() {
            display(h, c, !map);
        }
        display(h, self.unit_list, map);
        display(h, self.unit_list_label, map);
    }

    /// `Show`: the mode radios, then the mode's controls.
    pub fn show<H: SeplHost + ?Sized>(&self, h: &mut H) {
        display(h, self.units_radio, true);
        display(h, self.type_radio, true);
        self.refresh(h);
    }

    /// `Hide`: the mode radios, the counts, the specification column and the unit list.
    pub fn hide<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [
            self.units_radio,
            self.type_radio,
            self.min_edit,
            self.max_edit,
            self.min_label,
            self.max_label,
            self.dynamic,
            self.player_label,
            self.player,
            self.group_label,
            self.group_combo,
            self.type_label,
            self.type_combo,
            self.parent_label,
            self.parent_combo,
            self.unit_list_label,
            self.unit_list,
        ] {
            display(h, c, false);
        }
    }

    /// `CheckChanged`: the controls of the filter check box `id` shown while it is on or "not" (`state` != 0).
    pub fn check_changed<H: SeplHost + ?Sized>(&self, h: &mut H, id: u32, state: i32) {
        let shown = state != 0;
        let controls: &[usize] = match id {
            EE_SCREENS_SEOB_AREA_CHECK => &[self.area_combo],
            EE_SCREENS_SEOB_NEAR_CHECK => &[self.near_object],
            EE_SCREENS_SEOB_ATTRIBUTE_CHECK => &[
                self.attribute_combo,
                self.attribute_min,
                self.attribute_min_label,
                self.attribute_max,
                self.attribute_max_label,
                self.percent_label,
                self.percent,
            ],
            EE_SCREENS_SEOB_VISIBLE_BY_CHECK => &[self.player_bc],
            EE_SCREENS_SEOB_HAS_STATE_CHECK => &[self.action_combo],
            EE_SCREENS_SEOB_SELECTED_BY_CHECK => &[self.player_cc],
            _ => &[],
        };
        for &c in controls {
            display(h, c, shown);
        }
    }

    /// `ClearUnitList`: the picked units' list emptied.
    pub fn clear_unit_list<H: SeplHost + ?Sized>(&self, h: &mut H) {
        if self.unit_list != 0 {
            call(h, EE_SCREENS_SEPL_OP_LIST_CLEAR, [self.unit_list, 0, 0], [0; 4]);
        }
    }

    /// `Deactivate`: the editor's selection cleared.
    pub fn deactivate<H: SeplHost + ?Sized>(&self, h: &mut H) {
        call(h, EE_SCREENS_SEOB_OP_CLEAR_SELECTION, [self.select_units, 0, 0], [0; 4]);
    }

    /// The scenario objects' list operations (`Sort`, `MoveDown`, `MoveUp`, `Delete` (the editor's selection
    /// cleared first), `Duplicate`, `New`) on the screen's entry `selection`.
    pub fn list_op<H: SeplHost + ?Sized>(&self, h: &mut H, op: i32, selection: u32) {
        if op == EE_SCREENS_SEOB_SM_DELETE {
            call(h, EE_SCREENS_SEOB_OP_CLEAR_SELECTION, [self.select_units, 0, 0], [0; 4]);
        }
        call(h, EE_SCREENS_SEOB_OP_SM, [0; 3], [op, selection.wrapping_add(1) as i32, 0, 0]);
    }

    /// `Activate`: the player combos' counts and flags, the areas, the class list (the first time), the actions.
    pub fn activate<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let players = call(h, EE_SCREENS_SEOB_OP_PLAYER_COUNT, [self.area_check, 0, 0], [0; 4]);
        call(h, EE_SCREENS_SEOB_OP_TRIGGER_TEXT, [0; 3], [0; 4]);
        for (combo, flags) in [(self.player_bc, 0xc6), (self.player_cc, 0x82), (self.player, 0xe7)] {
            player_combo(h, combo, EE_SCREENS_SEOB_PC_COUNT, players);
            player_combo(h, combo, EE_SCREENS_SEOB_PC_FLAGS, flags);
        }
        call(h, EE_SCREENS_SEPL_OP_COMBO_CLEAR, [self.area_combo, 0, 0], [0; 4]);
        call(h, EE_SCREENS_SEOB_OP_APPEND_NAMES, [self.area_combo, 0, 0], [EE_SCREENS_SEOB_NAMES_AREAS, 0, 0, 0]);
        if self.unit_types.borrow().is_empty() {
            let cap = 4096usize;
            let mut types = vec![0u32; cap];
            let n = call(h, EE_SCREENS_SEOB_OP_UNIT_TYPES, [types.as_mut_ptr() as usize, 0, 0], [cap as i32, 0, 0, 0]);
            types.truncate((n.max(0) as usize).min(cap));
            let p = [self.type_combo, types.as_ptr() as usize, 0];
            call(h, EE_SCREENS_SEOB_OP_CLASS_NAMES, p, [types.len() as i32, 0, 0, 0]);
            *self.unit_types.borrow_mut() = types;
        }
        call(h, EE_SCREENS_SEPL_OP_COMBO_CLEAR, [self.action_combo, 0, 0], [0; 4]);
        for a in EE_SCREENS_SEOB_ACTIONS {
            call(h, EE_SCREENS_SEOB_OP_ACTION_APPEND, [self.action_combo, 0, 0], [a, 0, 0, 0]);
        }
    }

    /// `Store`: the controls written into the scenario object of the entry `selection`: its name (the trigger
    /// screen's), the mode, the picked units, the counts (the larger kept no smaller than the other), dynamic, the
    /// owner, family (a unit set by its manager's index), class, parent, the filters and their values.
    pub fn store<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32) {
        let e = selection.wrapping_add(1);
        call(h, EE_SCREENS_SEOB_OP_OBJ_NAME_FROM_SCREEN, [0; 3], [e as i32, 0, 0, 0]);
        let v = radio_checked(h, self.units_radio) as i32;
        obj_set(h, e, EE_SCREENS_SEOB_F_SELECT_ON_MAP, v);
        let ids = self.unit_ids.borrow().clone();
        call(h, EE_SCREENS_SEOB_OP_OBJ_SET_UNITS, [ids.as_ptr() as usize, 0, 0], [e as i32, ids.len() as i32, 0, 0]);
        if atoi(h, self.max_edit) < atoi(h, self.min_edit) {
            let v = atoi(h, self.min_edit);
            edit_number(h, self.max_edit, v);
        } else if atoi(h, self.min_edit) > atoi(h, self.max_edit) {
            let v = atoi(h, self.max_edit);
            edit_number(h, self.min_edit, v);
        }
        let v = atoi(h, self.min_edit);
        obj_set(h, e, EE_SCREENS_SEOB_F_MIN, v);
        let v = atoi(h, self.max_edit);
        obj_set(h, e, EE_SCREENS_SEOB_F_MAX, v);
        let v = checked(h, self.dynamic) as i32;
        obj_set(h, e, EE_SCREENS_SEOB_F_DYNAMIC, v);
        let v = player_combo(h, self.player, EE_SCREENS_SEOB_PC_GET, 0);
        obj_set(h, e, EE_SCREENS_SEOB_F_OWNER, v);
        let mut family = selection_of(h, self.group_combo);
        if family >= EE_SCREENS_SEOB_FAMILIES {
            // (a family past the sets: the original read past its list; the first set's here)
            let k = (family - EE_SCREENS_SEOB_FAMILIES) as usize;
            let set = self.unit_sets.get(k).or(self.unit_sets.first()).copied().unwrap_or(0);
            family = (set as u32).wrapping_add(EE_SCREENS_SEOB_FAMILIES);
        }
        obj_set(h, e, EE_SCREENS_SEOB_F_FAMILY, family as i32);
        let mut t = selection_of(h, self.type_combo);
        if t == NONE {
            t = 0;
        }
        let class = self.unit_types.borrow().get(t as usize).copied().unwrap_or(0);
        obj_set(h, e, EE_SCREENS_SEOB_F_CLASS, class as i32);
        let v = selection_of(h, self.parent_combo);
        obj_set(h, e, EE_SCREENS_SEOB_F_PARENT, v as i32);
        for (c, f) in self.checks() {
            let v = tri(h, c);
            obj_set(h, e, f, v);
        }
        let area = selection_of(h, self.area_combo);
        if area != NONE {
            obj_set(h, e, EE_SCREENS_SEOB_F_AREA, area as i32);
        } else {
            obj_set(h, e, EE_SCREENS_SEOB_F_IN_AREA_CHECK, 0);
        }
        let v = selection_of(h, self.near_mode);
        obj_set(h, e, EE_SCREENS_SEOB_F_NEAR_MODE, v as i32);
        let near = selection_of(h, self.near_object);
        if near != NONE {
            obj_set(h, e, EE_SCREENS_SEOB_F_NEAR_OBJECT, near as i32);
        } else {
            obj_set(h, e, EE_SCREENS_SEOB_F_NEAR_CHECK, 0);
        }
        let v = selection_of(h, self.attribute_combo);
        obj_set(h, e, EE_SCREENS_SEOB_F_ATTRIBUTE, v as i32);
        if text_as_int(h, self.attribute_max) < text_as_int(h, self.attribute_min) {
            let v = text_as_int(h, self.attribute_min);
            edit_number(h, self.attribute_max, v);
        } else if text_as_int(h, self.attribute_min) > text_as_int(h, self.attribute_max) {
            let v = text_as_int(h, self.attribute_max);
            edit_number(h, self.attribute_min, v);
        }
        let v = text_as_int(h, self.attribute_min);
        obj_set(h, e, EE_SCREENS_SEOB_F_ATTRIBUTE_MIN, v);
        let v = text_as_int(h, self.attribute_max);
        obj_set(h, e, EE_SCREENS_SEOB_F_ATTRIBUTE_MAX, v);
        let v = checked(h, self.percent) as i32;
        obj_set(h, e, EE_SCREENS_SEOB_F_PERCENT, v);
        let v = player_combo(h, self.player_bc, EE_SCREENS_SEOB_PC_GET, 0);
        obj_set(h, e, EE_SCREENS_SEOB_F_VISIBLE_BY, v);
        let v = call(h, EE_SCREENS_SEOB_OP_ACTION_ID, [self.action_combo, 0, 0], [0; 4]);
        obj_set(h, e, EE_SCREENS_SEOB_F_STATE, v);
        let v = player_combo(h, self.player_cc, EE_SCREENS_SEOB_PC_GET, 0);
        obj_set(h, e, EE_SCREENS_SEOB_F_SELECTED_BY, v);
    }

    /// The six filter check boxes with their fields.
    fn checks(&self) -> [(usize, i32); 6] {
        [
            (self.area_check, EE_SCREENS_SEOB_F_IN_AREA_CHECK),
            (self.near_check, EE_SCREENS_SEOB_F_NEAR_CHECK),
            (self.attribute_check, EE_SCREENS_SEOB_F_HAS_ATTRIBUTE_CHECK),
            (self.visible_by_check, EE_SCREENS_SEOB_F_VISIBLE_BY_CHECK),
            (self.has_state_check, EE_SCREENS_SEOB_F_HAS_STATE_CHECK),
            (self.selected_by_check, EE_SCREENS_SEOB_F_SELECTED_BY_CHECK),
        ]
    }

    /// `Load`: the object lists, then the controls from the scenario object of the entry `selection` (a value past
    /// its list selects the first line), the picked units selected in the editor, the name regenerated.
    pub fn load<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32) {
        let e = selection.wrapping_add(1);
        call(h, EE_SCREENS_SEPL_OP_COMBO_CLEAR, [self.near_object, 0, 0], [0; 4]);
        call(h, EE_SCREENS_SEPL_OP_COMBO_CLEAR, [self.parent_combo, 0, 0], [0; 4]);
        let p = [self.near_object, self.parent_combo, 0];
        call(h, EE_SCREENS_SEOB_OP_APPEND_NAMES, p, [EE_SCREENS_SEOB_NAMES_OBJECTS, 0, 0, 0]);
        call(h, EE_SCREENS_SEOB_OP_NAME_TO_EDIT, [0; 3], [e as i32, 0, 0, 0]);
        let radio =
            if obj_get(h, e, EE_SCREENS_SEOB_F_SELECT_ON_MAP) != 0 { self.units_radio } else { self.type_radio };
        call(h, EE_SCREENS_SEPL_OP_RADIO_CHECK, [radio, 0, 0], [0; 4]);
        call(h, EE_SCREENS_SEOB_OP_CLEAR_SELECTION, [self.area_check, 0, 0], [0; 4]);
        self.unit_ids.borrow_mut().clear();
        call(h, EE_SCREENS_SEOB_OP_SELECT_OBJECT_UNITS, [self.area_check, 0, 0], [e as i32, 0, 0, 0]);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_MIN);
        edit_number(h, self.min_edit, v);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_MAX);
        edit_number(h, self.max_edit, v);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_DYNAMIC) != 0;
        set_checked(h, self.dynamic, v);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_OWNER);
        player_combo(h, self.player, EE_SCREENS_SEOB_PC_SET, v);
        let family = obj_get(h, e, EE_SCREENS_SEOB_F_FAMILY) as u32;
        let mut group = family;
        if group >= EE_SCREENS_SEOB_FAMILIES {
            let set = family.wrapping_sub(EE_SCREENS_SEOB_FAMILIES) as i32;
            group = match self.unit_sets.iter().position(|&s| s == set) {
                Some(k) => k as u32 + EE_SCREENS_SEOB_FAMILIES,
                None => 0,
            };
        }
        let lines = line_count(h, self.group_combo);
        set_selection(h, self.group_combo, if group < lines { group } else { 0 });
        set_selection(h, self.type_combo, 0);
        let class = obj_get(h, e, EE_SCREENS_SEOB_F_CLASS) as u32;
        let found = self.unit_types.borrow().iter().position(|&t| t == class);
        if let Some(t) = found {
            set_selection(h, self.type_combo, t as u32);
        }
        for (c, f) in self.checks() {
            let v = obj_get(h, e, f);
            set_tri(h, c, v);
        }
        for (combo, field) in [
            (self.area_combo, EE_SCREENS_SEOB_F_AREA),
            (usize::MAX, EE_SCREENS_SEOB_F_NEAR_MODE),
            (self.near_object, EE_SCREENS_SEOB_F_NEAR_OBJECT),
            (self.parent_combo, EE_SCREENS_SEOB_F_PARENT),
        ] {
            if combo == usize::MAX {
                let v = obj_get(h, e, field) as u32;
                set_selection(h, self.near_mode, v);
                continue;
            }
            let v = obj_get(h, e, field) as u32;
            let lines = line_count(h, combo);
            if v < lines {
                set_selection(h, combo, v);
            } else if lines > 0 {
                set_selection(h, combo, 0);
            }
        }
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_ATTRIBUTE) as u32;
        set_selection(h, self.attribute_combo, v);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_ATTRIBUTE_MIN);
        edit_number(h, self.attribute_min, v);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_ATTRIBUTE_MAX);
        edit_number(h, self.attribute_max, v);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_PERCENT) != 0;
        set_checked(h, self.percent, v);
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_VISIBLE_BY);
        player_combo(h, self.player_bc, EE_SCREENS_SEOB_PC_SET, v);
        let state = obj_get(h, e, EE_SCREENS_SEOB_F_STATE);
        let i = call(h, EE_SCREENS_SEOB_OP_ACTION_INDEX, [self.action_combo, 0, 0], [state, 0, 0, 0]) as u32;
        let lines = line_count(h, self.action_combo);
        set_selection(h, self.action_combo, if i < lines { i } else { 0 });
        let v = obj_get(h, e, EE_SCREENS_SEOB_F_SELECTED_BY);
        player_combo(h, self.player_cc, EE_SCREENS_SEOB_PC_SET, v);
        self.update_selection(h, selection);
    }

    /// `UpdateSelection`: in the pick-on-map mode the editor's selected units listed (their ids kept); then an
    /// object named automatically (`<...>`) is renamed: from its units (the first one's class and id % 1000, "+"
    /// for more), else from its parent ("#:" and its name), its class, its family or its owner's units, "*" when
    /// dynamic.
    pub fn update_selection<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32) {
        let map = radio_checked(h, self.units_radio);
        if map {
            let n = call(h, EE_SCREENS_SEOB_OP_UNIT_LIST_FILL, [self.unit_list, self.form, 0], [0; 4]).max(0) as u32;
            let mut ids = Vec::with_capacity(n as usize);
            for i in 0..n {
                ids.push(call(h, EE_SCREENS_SEOB_OP_SELECTED_UNIT_ID, [self.form, 0, 0], [i as i32, 0, 0, 0]) as u32);
            }
            *self.unit_ids.borrow_mut() = ids;
        }
        let e = selection.wrapping_add(1);
        if call(h, EE_SCREENS_SEOB_OP_OBJ_EXISTS, [0; 3], [e as i32, 0, 0, 0]) == 0 {
            return;
        }
        let name = text(h, EE_SCREENS_SEOB_TEXT_OBJECT_NAME, e as i32, 0).unwrap_or_default();
        if name.first() != Some(&b'<') {
            return;
        }
        let mut type_name: Vec<u8>;
        if radio_checked(h, self.units_radio) {
            let count = call(h, EE_SCREENS_SEOB_OP_SELECTED_COUNT, [self.form, 0, 0], [0; 4]) as u32;
            if count > 0 {
                type_name = text(h, EE_SCREENS_SEOB_TEXT_UNIT_CLASS, 0, self.form).unwrap_or_default();
                let id = call(h, EE_SCREENS_SEOB_OP_SELECTED_UNIT_ID, [self.form, 0, 0], [0; 4]);
                type_name.extend_from_slice(format!("{}", id as u32 % 1000).as_bytes());
            } else {
                type_name =
                    text(h, EE_SCREENS_SEOB_TEXT_STRING, EE_SCREENS_SEOB_IDS_NONE as i32, 0).unwrap_or_default();
            }
            if count > 1 {
                type_name.push(b'+');
            }
        } else {
            let parent = selection_of(h, self.parent_combo);
            if parent == 0 {
                let mut combo = self.type_combo;
                let mut s = selection_of(h, combo);
                if s == 0 {
                    combo = self.group_combo;
                    s = selection_of(h, combo);
                }
                if s != 0 {
                    type_name = text(h, EE_SCREENS_SEOB_TEXT_LINE, s as i32, combo).unwrap_or_default();
                } else {
                    let p = selection_of(h, self.player);
                    type_name = text(h, EE_SCREENS_SEOB_TEXT_LINE, p as i32, self.player).unwrap_or_default();
                    type_name.push(b' ');
                    let units = EE_SCREENS_SEOB_IDS_SE_UNIT_GROUP_UNITS as i32;
                    type_name.extend(text(h, EE_SCREENS_SEOB_TEXT_STRING, units, 0).unwrap_or_default());
                }
            } else {
                type_name = b"#:".to_vec();
                type_name.extend(text(h, EE_SCREENS_SEOB_TEXT_OBJECT_NAME, parent as i32, 0).unwrap_or_default());
            }
            if checked(h, self.dynamic) {
                type_name.push(b'*');
            }
        }
        let mut name = vec![b'<'];
        name.extend(type_name);
        name.push(b'>');
        call(h, EE_SCREENS_SEOB_OP_OBJ_SET_NAME, [name.as_ptr() as usize, 0, 0], [e as i32, name.len() as i32, 0, 0]);
    }

    /// `GetIsSelectOnMap`: the pick-on-map radio checked.
    pub fn is_select_on_map<H: SeplHost + ?Sized>(&self, h: &mut H) -> bool {
        radio_checked(h, self.units_radio)
    }

    /// `MouseMove`: a press in the game view starts a selection while units are picked on the map.
    pub fn mouse_move<H: SeplHost + ?Sized>(&self, h: &mut H, x: f32, y: f32) {
        let (mut i, mut f) = ([0i32; 4], [x, y, 0.0, 0.0]);
        if h.op(EE_SCREENS_SEOB_OP_VIEW_PRESS, [0; 3], &mut i, &mut f) != 0 && radio_checked(h, self.units_radio) {
            let (mut i, mut f) = ([0i32; 4], [x, y, 0.0, 0.0]);
            h.op(EE_SCREENS_SEOB_OP_START_SELECTION, [self.select_units, 0, 0], &mut i, &mut f);
        }
    }

    /// `GetNames`: the user's objects' names into the C++ vector `out`.
    pub fn get_names<H: SeplHost + ?Sized>(&self, h: &mut H, out: usize) {
        call(h, EE_SCREENS_SEOB_OP_GET_NAMES, [out, 0, 0], [EE_SCREENS_SEOB_NAMES_USER_OBJECTS, 0, 0, 0]);
    }
}

/// A combo box's selection (`GetCurrentSelection`: none reads as `NONE`).
fn selection_of<H: SeplHost + ?Sized>(h: &mut H, combo: usize) -> u32 {
    selection(h, combo)
}

/// The scenario combo box's screens (`kSETriggerScreenObject`, `kSETriggerScreenArea`).
const SCREEN_OBJECT: i32 = 3;
const SCREEN_AREA: u32 = 4;

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// A recording host: controls are handles from 1 with their records (the selection tool from 1000); a
    /// rectangle is its record's (a font height of 16); display modes, checks, tri-states, selections, edit
    /// numbers and the scenario objects' fields are kept; every other op is logged.
    #[derive(Default)]
    struct Rec {
        made: Vec<EeScreensSeplCtl>,
        mode: HashMap<usize, i32>,
        on: HashMap<usize, bool>,
        state: HashMap<usize, i32>,
        sel: HashMap<usize, u32>,
        number: HashMap<usize, i32>,
        lines: HashMap<usize, u32>,
        fields: HashMap<(i32, i32), i32>,
        names: HashMap<i32, Vec<u8>>,
        selected: Vec<(i32, Vec<u8>)>,
        log: Vec<(u32, [usize; 3], [i32; 4])>,
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
                EE_SCREENS_SEOB_OP_SELECT_UNITS => {
                    let out = p[0] as *mut usize;
                    unsafe { *out = 1000 };
                    unsafe { *out.add(1) = 1001 };
                    unsafe { *out.add(2) = 1002 };
                    0
                }
                EE_SCREENS_SEPL_OP_RECT => {
                    let c = self.made[p[0] - 1];
                    let h = if c.flags & EE_SCREENS_SEPL_FONT_HEIGHT != 0 { 16.0 + c.h } else { c.h };
                    *f = [c.x, c.y, c.x + c.w - 1.0, c.y + h - 1.0];
                    0
                }
                EE_SCREENS_SEPL_OP_SET_DISPLAY => {
                    self.mode.insert(p[0], i[0]);
                    0
                }
                EE_SCREENS_SEPL_OP_SET_CHECKED => {
                    self.on.insert(p[0], i[0] != 0);
                    0
                }
                EE_SCREENS_SEPL_OP_RADIO_CHECK => {
                    let (u, t) = (1, 2); // the two mode radios
                    self.on.insert(u, p[0] == u);
                    self.on.insert(t, p[0] == t);
                    0
                }
                EE_SCREENS_SEPL_OP_CHECKED | EE_SCREENS_SEPL_OP_RADIO_IS_CHECKED => {
                    *self.on.get(&p[0]).unwrap_or(&false) as i32
                }
                EE_SCREENS_SEPL_OP_TRI_STATE => *self.state.get(&p[0]).unwrap_or(&0),
                EE_SCREENS_SEPL_OP_SET_TRI_STATE => {
                    self.state.insert(p[0], i[0]);
                    0
                }
                EE_SCREENS_SEPL_OP_SET_SELECTION => {
                    self.sel.insert(p[0], i[0] as u32);
                    0
                }
                EE_SCREENS_SEPL_OP_COMBO_SELECTION => *self.sel.get(&p[0]).unwrap_or(&u32::MAX) as i32,
                EE_SCREENS_SEPL_OP_LINE_COUNT => *self.lines.get(&p[0]).unwrap_or(&0) as i32,
                EE_SCREENS_SEPL_OP_EDIT_NUMBER => {
                    self.number.insert(p[0], i[0]);
                    0
                }
                EE_SCREENS_SEPL_OP_EDIT_ATOI | EE_SCREENS_SEPL_OP_EDIT_INT => *self.number.get(&p[0]).unwrap_or(&0),
                EE_SCREENS_SEOB_OP_OBJ_GET => *self.fields.get(&(i[0], i[1])).unwrap_or(&0),
                EE_SCREENS_SEOB_OP_OBJ_SET => {
                    self.fields.insert((i[0], i[1]), i[2]);
                    0
                }
                EE_SCREENS_SEOB_OP_OBJ_EXISTS => self.names.contains_key(&i[0]) as i32,
                EE_SCREENS_SEOB_OP_UNIT_SET_COUNT => 4,
                EE_SCREENS_SEOB_OP_UNIT_SET_SHOWN => (i[0] % 2 == 1) as i32,
                EE_SCREENS_SEOB_OP_UNIT_TYPES => {
                    let out = p[0] as *mut u32;
                    for (k, t) in [0u32, 30, 10, 20].into_iter().enumerate() {
                        unsafe { *out.add(k) = t };
                    }
                    4
                }
                EE_SCREENS_SEOB_OP_SELECTED_COUNT | EE_SCREENS_SEOB_OP_UNIT_LIST_FILL => self.selected.len() as i32,
                EE_SCREENS_SEOB_OP_SELECTED_UNIT_ID => self.selected[i[0] as usize].0,
                EE_SCREENS_SEOB_OP_TEXT => {
                    let t: Vec<u8> = match i[0] {
                        EE_SCREENS_SEOB_TEXT_OBJECT_NAME => match self.names.get(&i[1]) {
                            Some(n) => n.clone(),
                            None => return -1,
                        },
                        EE_SCREENS_SEOB_TEXT_UNIT_CLASS => self.selected[i[1] as usize].1.clone(),
                        EE_SCREENS_SEOB_TEXT_STRING => format!("S{}", i[1]).into_bytes(),
                        _ => format!("L{}.{}", p[1], i[1]).into_bytes(),
                    };
                    let n = t.len().min(i[2] as usize);
                    unsafe { core::ptr::copy_nonoverlapping(t.as_ptr(), p[0] as *mut u8, n) };
                    t.len() as i32
                }
                EE_SCREENS_SEOB_OP_OBJ_SET_NAME => {
                    let s = unsafe { core::slice::from_raw_parts(p[0] as *const u8, i[1] as usize) };
                    self.names.insert(i[0], s.to_vec());
                    0
                }
                _ => {
                    self.log.push((op, p, *i));
                    0
                }
            }
        }
    }

    fn made() -> (Rec, Object, u32) {
        let mut h = Rec::default();
        let mut index = 227;
        let o = Object::new(&mut h, 99, &mut index);
        (h, o, index)
    }

    #[test]
    fn the_screen_makes_its_controls_in_the_originals_order() {
        let (h, o, index) = made();
        // the selection tool at 227, then 36 controls
        assert_eq!(h.made.len(), 36);
        assert_eq!(index, 227 + 37);
        for (k, c) in h.made.iter().enumerate() {
            assert_eq!(c.index, 228 + k as u32);
            assert_eq!(c.groups, 1 << 10);
        }
        let ids: Vec<u32> = h.made.iter().map(|c| c.id).collect();
        assert_eq!(&ids[..6], &[0x834, 0x835, 0x839, 0x837, 0x83a, 0x838]);
        assert_eq!(ids[35], 0x853);
        assert_eq!(h.log[0].0, EE_SCREENS_SEPL_OP_SCREEN_EVENT);
        // the two mode radios; the units picked on the map first
        assert_eq!(h.made[0].extra[0], 1);
        assert_eq!(h.made[1].extra[0], 0);
        // the type radio one below the units radio, the dynamic box two below it
        assert_eq!(h.made[1].y, 504.0 + 20.0);
        assert_eq!(h.made[6].y, 524.0 + 20.0 + 1.0);
        // the family combo: the unit sets 1 and 3 listed
        assert_eq!(o.unit_sets, vec![1, 3]);
        // the unit list down to the max count's box (583 + 22 - 1)
        let list = &h.made[(o.unit_list - 1) as usize];
        assert_eq!(list.kind, EE_SCREENS_SEPL_UNIT_LIST);
        assert_eq!(list.y, 458.0 + 16.0);
        assert_eq!(list.h, 583.0 + 22.0 - 1.0 - 474.0);
        // the percent label: "%"
        let pc = &h.made[(o.percent_label - 1) as usize];
        assert_eq!((pc.text, pc.number), (EE_SCREENS_SEPL_TEXT_CHAR, b'%' as i32));
        // the pick-on-map mode shown: the unit list, not the specification
        assert_eq!(h.mode[&o.unit_list], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode[&o.player], EE_SCREENS_SEPL_HIDDEN);
    }

    #[test]
    fn refresh_shows_one_mode_and_check_changed_its_filter() {
        let (mut h, o, _) = made();
        o.select_on_map.set(false);
        o.refresh(&mut h);
        for c in o.specification() {
            assert_eq!(h.mode[&c], EE_SCREENS_SEPL_SHOWN);
        }
        assert_eq!(h.mode[&o.unit_list_label], EE_SCREENS_SEPL_HIDDEN);
        o.check_changed(&mut h, EE_SCREENS_SEOB_ATTRIBUTE_CHECK, 2);
        assert_eq!(h.mode[&o.percent], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode.get(&o.player_bc), None);
        o.check_changed(&mut h, EE_SCREENS_SEOB_ATTRIBUTE_CHECK, 0);
        assert_eq!(h.mode[&o.attribute_combo], EE_SCREENS_SEPL_HIDDEN);
        o.check_changed(&mut h, EE_SCREENS_SEOB_SELECTED_BY_CHECK, 1);
        assert_eq!(h.mode[&o.player_cc], EE_SCREENS_SEPL_SHOWN);
        o.hide(&mut h);
        assert_eq!(h.mode[&o.units_radio], EE_SCREENS_SEPL_HIDDEN);
    }

    #[test]
    fn activate_builds_the_class_list_once() {
        let (mut h, o, _) = made();
        o.activate(&mut h);
        o.activate(&mut h);
        assert_eq!(*o.unit_types.borrow(), vec![0, 30, 10, 20]);
        let n = h.log.iter().filter(|l| l.0 == EE_SCREENS_SEOB_OP_CLASS_NAMES).count();
        assert_eq!(n, 1);
        let actions: Vec<i32> =
            h.log.iter().filter(|l| l.0 == EE_SCREENS_SEOB_OP_ACTION_APPEND).map(|l| l.2[0]).collect();
        assert_eq!(actions.len(), 48);
        assert_eq!(&actions[..3], &[1000, 1002, 1003]);
        let flags: Vec<(usize, i32)> = h
            .log
            .iter()
            .filter(|l| l.0 == EE_SCREENS_SEOB_OP_PLAYER_COMBO && l.2[0] == EE_SCREENS_SEOB_PC_FLAGS)
            .map(|l| (l.1[0], l.2[1]))
            .take(3)
            .collect();
        assert_eq!(flags, vec![(o.player_bc, 0xc6), (o.player_cc, 0x82), (o.player, 0xe7)]);
    }

    #[test]
    fn store_clamps_the_counts_and_maps_the_unit_sets() {
        let (mut h, o, _) = made();
        o.activate(&mut h);
        h.number.insert(o.min_edit, 5);
        h.number.insert(o.max_edit, 3);
        h.number.insert(o.attribute_min, 9);
        h.number.insert(o.attribute_max, 12);
        h.sel.insert(o.group_combo, EE_SCREENS_SEOB_FAMILIES + 1);
        h.sel.insert(o.type_combo, 2);
        h.sel.insert(o.parent_combo, 1);
        h.state.insert(o.area_check, 1);
        h.state.insert(o.near_check, 2);
        // no area selected: the area check cleared; a near object: kept
        h.sel.insert(o.near_object, 0);
        o.store(&mut h, 4);
        let f = |h: &Rec, k| h.fields[&(5, k)];
        assert_eq!(h.number[&o.max_edit], 5);
        assert_eq!((f(&h, EE_SCREENS_SEOB_F_MIN), f(&h, EE_SCREENS_SEOB_F_MAX)), (5, 5));
        assert_eq!((f(&h, EE_SCREENS_SEOB_F_ATTRIBUTE_MIN), f(&h, EE_SCREENS_SEOB_F_ATTRIBUTE_MAX)), (9, 12));
        assert_eq!(f(&h, EE_SCREENS_SEOB_F_FAMILY), 3 + EE_SCREENS_SEOB_FAMILIES as i32);
        assert_eq!(f(&h, EE_SCREENS_SEOB_F_CLASS), 10);
        assert_eq!(f(&h, EE_SCREENS_SEOB_F_PARENT), 1);
        assert_eq!(f(&h, EE_SCREENS_SEOB_F_IN_AREA_CHECK), 0);
        assert_eq!(f(&h, EE_SCREENS_SEOB_F_NEAR_CHECK), 2);
        assert_eq!(f(&h, EE_SCREENS_SEOB_F_NEAR_OBJECT), 0);
        assert_eq!(h.fields.get(&(5, EE_SCREENS_SEOB_F_AREA)), None);
        // the min count raised to the max when it is the larger one
        h.number.insert(o.min_edit, 1);
        h.number.insert(o.max_edit, 0);
        o.store(&mut h, 4);
        assert_eq!(h.number[&o.max_edit], 1);
    }

    #[test]
    fn load_selects_within_the_lists_and_maps_the_sets_back() {
        let (mut h, o, _) = made();
        o.activate(&mut h);
        h.lines.insert(o.group_combo, 200);
        h.lines.insert(o.area_combo, 2);
        h.lines.insert(o.parent_combo, 3);
        h.lines.insert(o.near_object, 0);
        h.lines.insert(o.action_combo, 24);
        h.fields.insert((2, EE_SCREENS_SEOB_F_FAMILY), 3 + EE_SCREENS_SEOB_FAMILIES as i32);
        h.fields.insert((2, EE_SCREENS_SEOB_F_CLASS), 20);
        h.fields.insert((2, EE_SCREENS_SEOB_F_AREA), 5);
        h.fields.insert((2, EE_SCREENS_SEOB_F_PARENT), 2);
        h.fields.insert((2, EE_SCREENS_SEOB_F_NEAR_OBJECT), 1);
        h.fields.insert((2, EE_SCREENS_SEOB_F_HAS_STATE_CHECK), 2);
        o.load(&mut h, 1);
        assert_eq!(h.sel[&o.group_combo], EE_SCREENS_SEOB_FAMILIES + 1);
        assert_eq!(h.sel[&o.type_combo], 3);
        assert_eq!(h.sel[&o.area_combo], 0);
        assert_eq!(h.sel[&o.parent_combo], 2);
        assert_eq!(h.sel.get(&o.near_object), None);
        assert_eq!(h.state[&o.has_state_check], 2);
        assert_eq!(h.on[&o.type_radio], true);
        // an unknown set: any family
        h.fields.insert((2, EE_SCREENS_SEOB_F_FAMILY), 2 + EE_SCREENS_SEOB_FAMILIES as i32);
        o.load(&mut h, 1);
        assert_eq!(h.sel[&o.group_combo], 0);
    }

    #[test]
    fn update_selection_names_an_automatic_object() {
        let (mut h, o, _) = made();
        h.names.insert(1, b"<x>".to_vec());
        h.names.insert(2, b"Mine".to_vec());
        h.on.insert(o.units_radio, true);
        h.selected = vec![(1234, b"Citizen".to_vec()), (7, b"Clubman".to_vec())];
        o.update_selection(&mut h, 0);
        assert_eq!(h.names[&1], b"<Citizen234+>");
        assert_eq!(*o.unit_ids.borrow(), vec![1234, 7]);
        h.selected.clear();
        o.update_selection(&mut h, 0);
        assert_eq!(h.names[&1], format!("<S{}>", EE_SCREENS_SEOB_IDS_NONE).into_bytes());
        // the specification: the parent, else the class, the family, the owner's units; dynamic
        h.on.insert(o.units_radio, false);
        h.on.insert(o.dynamic, true);
        h.sel.insert(o.parent_combo, 2);
        o.update_selection(&mut h, 0);
        assert_eq!(h.names[&1], b"<#:Mine*>");
        h.on.insert(o.dynamic, false);
        h.sel.insert(o.parent_combo, 0);
        h.sel.insert(o.type_combo, 0);
        h.sel.insert(o.group_combo, 4);
        o.update_selection(&mut h, 0);
        assert_eq!(h.names[&1], format!("<L{}.4>", o.group_combo).into_bytes());
        h.sel.insert(o.group_combo, 0);
        h.sel.insert(o.player, 1);
        o.update_selection(&mut h, 0);
        let want = format!("<L{}.1 S{}>", o.player, EE_SCREENS_SEOB_IDS_SE_UNIT_GROUP_UNITS);
        assert_eq!(h.names[&1], want.into_bytes());
        // a name of the person's own: kept
        o.update_selection(&mut h, 1);
        assert_eq!(h.names[&2], b"Mine");
    }

    #[test]
    fn delete_clears_the_selection_first_and_the_destructor_deletes_every_control() {
        let (mut h, o, _) = made();
        h.log.clear();
        o.list_op(&mut h, EE_SCREENS_SEOB_SM_DELETE, 2);
        assert_eq!(h.log[0].0, EE_SCREENS_SEOB_OP_CLEAR_SELECTION);
        assert_eq!((h.log[1].0, h.log[1].2[0], h.log[1].2[1]), (EE_SCREENS_SEOB_OP_SM, EE_SCREENS_SEOB_SM_DELETE, 3));
        h.log.clear();
        o.destroy(&mut h);
        let deleted: Vec<usize> = h.log.iter().filter(|l| l.0 == EE_SCREENS_SEPL_OP_DELETE).map(|l| l.1[0]).collect();
        assert_eq!(deleted.len(), 37);
        let mut sorted = deleted.clone();
        sorted.sort();
        sorted.dedup();
        assert_eq!(sorted.len(), 37);
        assert_eq!(h.log.last().unwrap().0, EE_SCREENS_SEOB_OP_RELEASE);
    }
}
