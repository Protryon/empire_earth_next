//! The scenario editor's events (the editor's form, `EEUIFScenarioEditor`, and its trigger tab): each event is an
//! `EEScreenEvent` code and its handler here, over a host ([`SeHost`]) whose calls are each one call of the editor's
//! C++ objects (the trigger tab `EESETriggerScreen` and its sub-screens, the editor's selection, the camera, the
//! tabs, the files, the posted actions), `ee_screenscpp_se` in `Scenario Editor/EESEEvents.cpp`. The C++ keeps the
//! screens, the controls and the trigger records; the events that carry per-instance state (the slider's format, the
//! check box's controls, the scenario combo box's refill, the quick save's state, the hide-UI rectangles) and the
//! triggers-only save's report stay C++ classes.

/// The trigger tab's events (`EESEEvents.cpp`'s classes). The tab's buttons (`EESETriggerScreen`): the clicked tab,
/// the list's pick, new, duplicate, delete, up, down, sort, the debug ids, a name or text box's Enter.
pub const EE_SCREENS_SE_SHOW_TAB: u32 = 0x2401;
pub const EE_SCREENS_SE_SELECT_ENTRY: u32 = 0x2402;
pub const EE_SCREENS_SE_NEW_ENTRY: u32 = 0x2403;
pub const EE_SCREENS_SE_DUPLICATE_ENTRY: u32 = 0x2404;
pub const EE_SCREENS_SE_DELETE_ENTRY: u32 = 0x2405;
pub const EE_SCREENS_SE_INC_PRIORITY: u32 = 0x2406;
pub const EE_SCREENS_SE_DEC_PRIORITY: u32 = 0x2407;
pub const EE_SCREENS_SE_ALPHA_SORT: u32 = 0x2408;
pub const EE_SCREENS_SE_SHOW_DEBUG_IDS: u32 = 0x2409;
pub const EE_SCREENS_SE_SELECT_NAME: u32 = 0x240a;
/// The object screen (`EESETSObject`): its mode radios, its check boxes, its unit list's pick.
pub const EE_SCREENS_SE_OBJECT_DETAIL: u32 = 0x240b;
pub const EE_SCREENS_SE_OBJECT_CHECK: u32 = 0x240c;
pub const EE_SCREENS_SE_OBJECT_UNIT_SELECT: u32 = 0x240d;
/// The area screen (`EESETSArea`): its mode combo box, its centre button.
pub const EE_SCREENS_SE_AREA_CHECK: u32 = 0x240e;
pub const EE_SCREENS_SE_CENTER_CAMERA_ON_AREA: u32 = 0x240f;
/// The condition and effect screens' type and action combo boxes.
pub const EE_SCREENS_SE_CHANGE_CONDITION_TYPE: u32 = 0x2410;
/// The player-attribute condition's radios (`EESETSCTPlayerResource`'s event; its `Load` also runs it).
pub const EE_SCREENS_SE_CONDITION_PLAYER_RESOURCE: u32 = 0x2417;
pub const EE_SCREENS_SE_CHANGE_EFFECT_TYPE: u32 = 0x2411;
pub const EE_SCREENS_SE_CHANGE_EFFECT_ACTION: u32 = 0x2412;
/// The trigger screen (`EESETSTrigger`): an and/or combo box, the condition timer's check box, the pairs' back and
/// forward buttons, a remove button.
pub const EE_SCREENS_SE_TRIGGER_OPERATOR: u32 = 0x2413;
pub const EE_SCREENS_SE_TRIGGER_CONDITION_TIMER: u32 = 0x2414;
pub const EE_SCREENS_SE_TRIGGER_CHANGE_INDEX: u32 = 0x2415;
pub const EE_SCREENS_SE_TRIGGER_REMOVE: u32 = 0x2416;

/// The editor form's own events and its tabs' (group 2): the player combo box's change, the camera's home rotation,
/// the civilization builder's button, the triggers-only load (Ctrl+L), the elevation tab's raise, lower and level
/// buttons, the player tab's epoch box, the change-text effect's and the place effects' radios, the main buttons'
/// load, save, test and exit, the exit question's answers, the instructions' "///" expansion.
pub const EE_SCREENS_SE_NEW_PLAYER: u32 = 0x2420;
pub const EE_SCREENS_SE_HOME_ROTATION: u32 = 0x2421;
pub const EE_SCREENS_SE_OPEN_CIV_BUILDER: u32 = 0x2422;
pub const EE_SCREENS_SE_LOAD_TRIGGERS_ONLY: u32 = 0x2423;
pub const EE_SCREENS_SE_ELEVATION_TOOL: u32 = 0x2424;
pub const EE_SCREENS_SE_DISPLAY_EPOCH: u32 = 0x2425;
pub const EE_SCREENS_SE_CLEAR_ADD_LINE: u32 = 0x2426;
pub const EE_SCREENS_SE_PLACE: u32 = 0x2427;
pub const EE_SCREENS_SE_LOAD_SCENARIO: u32 = 0x2428;
pub const EE_SCREENS_SE_SAVE_SCENARIO: u32 = 0x2429;
pub const EE_SCREENS_SE_TEST_SCENARIO: u32 = 0x242a;
pub const EE_SCREENS_SE_EXIT_SCENARIO: u32 = 0x242b;
pub const EE_SCREENS_SE_SAVE_BEFORE_EXIT: u32 = 0x242c;
pub const EE_SCREENS_SE_EXIT_WITHOUT_SAVING: u32 = 0x242d;
pub const EE_SCREENS_SE_FIND_TEXT: u32 = 0x242e;

// ---- the host's ops (ee_screenscpp_se on the event's form, the editor's, and entity)

/// -> the entity's control id.
pub const EE_SCREENS_SE_OP_ENTITY_ID: u32 = 1;
/// (method, arg) a call of the trigger tab (`EE_SCREENS_SE_TAB_*`).
pub const EE_SCREENS_SE_OP_TAB: u32 = 2;
/// The scenario manager's debug ids flag toggled.
pub const EE_SCREENS_SE_OP_TOGGLE_DEBUG_IDS: u32 = 3;
/// (flag) the object screen's select-on-map flag.
pub const EE_SCREENS_SE_OP_OBJECT_SET_MAP: u32 = 4;
/// The object screen's `Refresh`.
pub const EE_SCREENS_SE_OP_OBJECT_REFRESH: u32 = 5;
/// -> the object screen's `GetIsSelectOnMap`.
pub const EE_SCREENS_SE_OP_OBJECT_IS_MAP: u32 = 6;
/// The UI manager's scenario editor form's `ClearSelection`.
pub const EE_SCREENS_SE_OP_MANAGER_CLEAR_SELECTION: u32 = 7;
/// The object screen's `ClearUnitList`.
pub const EE_SCREENS_SE_OP_OBJECT_CLEAR_UNIT_LIST: u32 = 8;
/// The event's form's `ClearSelection`.
pub const EE_SCREENS_SE_OP_FORM_CLEAR_SELECTION: u32 = 9;
/// The object screen's unit ids cleared.
pub const EE_SCREENS_SE_OP_OBJECT_CLEAR_IDS: u32 = 10;
/// The object screen's `UpdateSelection`.
pub const EE_SCREENS_SE_OP_OBJECT_UPDATE_SELECTION: u32 = 11;
/// The object screen's `CheckChanged(entity)`.
pub const EE_SCREENS_SE_OP_OBJECT_CHECK_CHANGED: u32 = 12;
/// -> the entity's (a string list's) current selection, `u32::MAX` when it has none.
pub const EE_SCREENS_SE_OP_LIST_SELECTION: u32 = 13;
/// -> the editor's selected unit count.
pub const EE_SCREENS_SE_OP_SELECTED_UNIT_COUNT: u32 = 14;
/// (index) a centre-camera action on the editor's selected unit.
pub const EE_SCREENS_SE_OP_CENTER_ON_UNIT: u32 = 15;
/// -> the entity's (a combo box's) current selection, `u32::MAX` when it has none.
pub const EE_SCREENS_SE_OP_COMBO_SELECTION: u32 = 16;
/// (flag) the area screen's rectangle mode.
pub const EE_SCREENS_SE_OP_AREA_MODE: u32 = 17;
/// -> the area screen has a centre (kept for the next op).
pub const EE_SCREENS_SE_OP_AREA_CENTER: u32 = 18;
/// A centre-camera action on the kept centre (each coordinate truncated to an integer).
pub const EE_SCREENS_SE_OP_CENTER_ON_AREA: u32 = 19;
/// The condition screen's `ChangeConditionType`.
pub const EE_SCREENS_SE_OP_CONDITION_TYPE: u32 = 20;
/// The effect screen's `ChangeEffectType`.
pub const EE_SCREENS_SE_OP_EFFECT_TYPE: u32 = 21;
/// The effect screen's `ChangeEffectAction`.
pub const EE_SCREENS_SE_OP_EFFECT_ACTION: u32 = 22;
/// (control id) the trigger screen's `ChangeOperator`.
pub const EE_SCREENS_SE_OP_TRIGGER_OPERATOR: u32 = 23;
/// (step: 0 conditions back, 1 forward, 2 effects back, 3 forward) the trigger screen's pair index.
pub const EE_SCREENS_SE_OP_TRIGGER_STEP: u32 = 24;
/// (control id) the trigger screen's `RemoveEntry`.
pub const EE_SCREENS_SE_OP_TRIGGER_REMOVE: u32 = 25;
/// -> the entity (a check box) checked.
pub const EE_SCREENS_SE_OP_CHECKED: u32 = 26;
/// (control id, mode) the form's control of the id (`GetControlById`) shown or hidden.
pub const EE_SCREENS_SE_OP_SHOW: u32 = 27;

/// The unit tab's `ReselectUnitType`.
pub const EE_SCREENS_SE_OP_UNIT_RESELECT: u32 = 28;
/// The player tab's `FillTechLists`.
pub const EE_SCREENS_SE_OP_PLAYER_FILL_TECH: u32 = 29;
/// The camera's view turn the identity, its offset turn yaw 45 degrees, pitch -35.
pub const EE_SCREENS_SE_OP_HOME_ROTATION: u32 = 30;
/// -> `OpenCivilizationBuilder` on the editor's civilization (its result).
pub const EE_SCREENS_SE_OP_OPEN_CIV_BUILDER: u32 = 31;
/// The scenario manager loaded from "ScenarioTriggers.scn" (when it opens).
pub const EE_SCREENS_SE_OP_LOAD_TRIGGERS: u32 = 32;
/// (control id) -> the form's slider's range value.
pub const EE_SCREENS_SE_OP_RANGE: u32 = 33;
/// (control id, value) the form's text label's number.
pub const EE_SCREENS_SE_OP_LABEL_NUMBER: u32 = 34;
/// The player tab's `RebuildTechLists`.
pub const EE_SCREENS_SE_OP_PLAYER_REBUILD_TECH: u32 = 35;
/// (flag) the place label (0x96d) and combo box (0x999) filled with area (flag) or object names, its first picked.
pub const EE_SCREENS_SE_OP_PLACE_FILL: u32 = 36;
/// -> the UI manager in the scenario editor.
pub const EE_SCREENS_SE_OP_IN_EDITOR: u32 = 37;
/// (screen event code) the editor's `PostLoadEvent` of that screen event.
pub const EE_SCREENS_SE_OP_POST_LOAD_EVENT: u32 = 38;
/// (kind: `EE_SCREENS_SE_ACTION_*`) the editor's action posted.
pub const EE_SCREENS_SE_OP_POST_ACTION: u32 = 39;
/// The save question (Yes saves then exits, No exits, Cancel stays).
pub const EE_SCREENS_SE_OP_ASK_EXIT: u32 = 40;
/// The editor exits once the save is done.
pub const EE_SCREENS_SE_OP_EXIT_AFTER_SAVE: u32 = 41;
/// -> the entity's (an edit box's) line count.
pub const EE_SCREENS_SE_OP_EDIT_LINES: u32 = 42;

/// The host's texts (`ee_screenscpp_se_text`): the entity's (an edit box's) line, a string of the game.
pub const EE_SCREENS_SE_TEXT_EDIT_LINE: u32 = 0;
pub const EE_SCREENS_SE_TEXT_STRING: u32 = 1;

/// The editor's actions (`EE_SCREENS_SE_OP_POST_ACTION`).
pub const EE_SCREENS_SE_ACTION_SAVE: u32 = 0;
pub const EE_SCREENS_SE_ACTION_TEST: u32 = 1;
pub const EE_SCREENS_SE_ACTION_EXIT: u32 = 2;

/// The trigger tab's calls (`EE_SCREENS_SE_OP_TAB`'s method): `ShowTab(arg)`, then the argless ones.
pub const EE_SCREENS_SE_TAB_SHOW: u32 = 0;
pub const EE_SCREENS_SE_TAB_SELECT_ENTRY: u32 = 1;
pub const EE_SCREENS_SE_TAB_NEW_ENTRY: u32 = 2;
pub const EE_SCREENS_SE_TAB_DUPLICATE_ENTRY: u32 = 3;
pub const EE_SCREENS_SE_TAB_DELETE_ENTRY: u32 = 4;
pub const EE_SCREENS_SE_TAB_INC_PRIORITY: u32 = 5;
pub const EE_SCREENS_SE_TAB_DEC_PRIORITY: u32 = 6;
pub const EE_SCREENS_SE_TAB_ALPHA_SORT: u32 = 7;
pub const EE_SCREENS_SE_TAB_SELECT_NAME: u32 = 8;

// the editor's control ids (ee/EEScenarioEditorIDs.h)
const SE_OBJECT_UNITS_RADIO: u32 = 0x834;
const SE_CONDITION_AMOUNT: u32 = 0x910;
const SE_CONDITION_USE_AMOUNT: u32 = 0x91a;
const SE_CONDITION_OTHER_PLAYER: u32 = 0x92f;
const SE_CONDITION_OTHER_ATTRIBUTE: u32 = 0x935;
const SE_TRIGGER_SECONDS_LABEL: u32 = 0x9cb;
const SE_TRIGGER_PREV_CONDITIONS: u32 = 0x9dc;
const SE_TRIGGER_NEXT_CONDITIONS: u32 = 0x9dd;
const SE_TRIGGER_PREV_EFFECTS: u32 = 0x9de;
const SE_TRIGGER_NEXT_EFFECTS: u32 = 0x9df;
const SE_TRIGGER_DURATION_BOX: u32 = 0x9e2;
const SE_ELEVATION_RAISE: u32 = 0x96;
const SE_ELEVATION_LOWER: u32 = 0x97;
const SE_ELEVATION_TITLE: u32 = 0x98;
const SE_ELEVATION_STRENGTH_TITLE: u32 = 0x99;
const SE_ELEVATION_SLIDER: u32 = 0x9d;
const SE_ELEVATION_STRENGTH_LABEL: u32 = 0x9f;
const SE_ELEVATION_LEVEL: u32 = 0xa0;
const SE_ELEVATION_STRENGTH_SLIDER: u32 = 0xa2;
const SE_AREA_RADIO: u32 = 0x988;
const SE_TEXT_EDIT_BOX: u32 = 0x983;
const SE_PLACE_LABEL: u32 = 0x96d;
const SE_HIDDEN: u32 = 0;
const SE_SHOWN: u32 = 1;

/// The editor's C++ objects, one call each.
pub trait SeHost {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32;
    /// A text (`EE_SCREENS_SE_TEXT_*`).
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// The entity's (an edit box's) text.
    fn set_edit_text(&mut self, text: &[u8]);
}

fn tab(h: &mut dyn SeHost, method: u32, arg: u32) {
    h.call(EE_SCREENS_SE_OP_TAB, method, arg);
}

/// One of the editor's events; `None` for another form's. Every handler answers `kErrorOK`.
pub fn dispatch(h: &mut dyn SeHost, event: u32) -> Option<core::ffi::c_long> {
    match event {
        EE_SCREENS_SE_SHOW_TAB => {
            let id = h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0);
            tab(h, EE_SCREENS_SE_TAB_SHOW, id);
        }
        EE_SCREENS_SE_SELECT_ENTRY => tab(h, EE_SCREENS_SE_TAB_SELECT_ENTRY, 0),
        EE_SCREENS_SE_NEW_ENTRY => tab(h, EE_SCREENS_SE_TAB_NEW_ENTRY, 0),
        EE_SCREENS_SE_DUPLICATE_ENTRY => tab(h, EE_SCREENS_SE_TAB_DUPLICATE_ENTRY, 0),
        EE_SCREENS_SE_DELETE_ENTRY => tab(h, EE_SCREENS_SE_TAB_DELETE_ENTRY, 0),
        EE_SCREENS_SE_INC_PRIORITY => tab(h, EE_SCREENS_SE_TAB_INC_PRIORITY, 0),
        EE_SCREENS_SE_DEC_PRIORITY => tab(h, EE_SCREENS_SE_TAB_DEC_PRIORITY, 0),
        EE_SCREENS_SE_ALPHA_SORT => tab(h, EE_SCREENS_SE_TAB_ALPHA_SORT, 0),
        EE_SCREENS_SE_SHOW_DEBUG_IDS => {
            h.call(EE_SCREENS_SE_OP_TOGGLE_DEBUG_IDS, 0, 0);
            tab(h, EE_SCREENS_SE_TAB_SELECT_ENTRY, 0);
        }
        EE_SCREENS_SE_SELECT_NAME => tab(h, EE_SCREENS_SE_TAB_SELECT_NAME, 0),
        EE_SCREENS_SE_OBJECT_DETAIL => object_detail(h),
        EE_SCREENS_SE_OBJECT_CHECK => {
            h.call(EE_SCREENS_SE_OP_OBJECT_CHECK_CHANGED, 0, 0);
        }
        EE_SCREENS_SE_OBJECT_UNIT_SELECT => {
            let selection = h.call(EE_SCREENS_SE_OP_LIST_SELECTION, 0, 0);
            if selection != u32::MAX && selection < h.call(EE_SCREENS_SE_OP_SELECTED_UNIT_COUNT, 0, 0) {
                h.call(EE_SCREENS_SE_OP_CENTER_ON_UNIT, selection, 0);
            }
        }
        EE_SCREENS_SE_AREA_CHECK => {
            let selection = h.call(EE_SCREENS_SE_OP_COMBO_SELECTION, 0, 0);
            h.call(EE_SCREENS_SE_OP_AREA_MODE, (selection == 0) as u32, 0);
        }
        EE_SCREENS_SE_CENTER_CAMERA_ON_AREA => {
            if h.call(EE_SCREENS_SE_OP_AREA_CENTER, 0, 0) != 0 {
                h.call(EE_SCREENS_SE_OP_CENTER_ON_AREA, 0, 0);
            }
        }
        EE_SCREENS_SE_CHANGE_CONDITION_TYPE => {
            h.call(EE_SCREENS_SE_OP_CONDITION_TYPE, 0, 0);
        }
        EE_SCREENS_SE_CONDITION_PLAYER_RESOURCE => {
            // the amount box with 'use amount', else the other player's and attribute's combo boxes
            let amount = h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0) == SE_CONDITION_USE_AMOUNT;
            let (a, b) = if amount { (SE_SHOWN, SE_HIDDEN) } else { (SE_HIDDEN, SE_SHOWN) };
            h.call(EE_SCREENS_SE_OP_SHOW, SE_CONDITION_AMOUNT, a);
            h.call(EE_SCREENS_SE_OP_SHOW, SE_CONDITION_OTHER_PLAYER, b);
            h.call(EE_SCREENS_SE_OP_SHOW, SE_CONDITION_OTHER_ATTRIBUTE, b);
        }
        EE_SCREENS_SE_CHANGE_EFFECT_TYPE => {
            h.call(EE_SCREENS_SE_OP_EFFECT_TYPE, 0, 0);
        }
        EE_SCREENS_SE_CHANGE_EFFECT_ACTION => {
            h.call(EE_SCREENS_SE_OP_EFFECT_ACTION, 0, 0);
        }
        EE_SCREENS_SE_TRIGGER_OPERATOR => {
            let id = h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0);
            h.call(EE_SCREENS_SE_OP_TRIGGER_OPERATOR, id, 0);
        }
        EE_SCREENS_SE_TRIGGER_CONDITION_TIMER => {
            let mode = if h.call(EE_SCREENS_SE_OP_CHECKED, 0, 0) != 0 { SE_SHOWN } else { SE_HIDDEN };
            h.call(EE_SCREENS_SE_OP_SHOW, SE_TRIGGER_DURATION_BOX, mode);
            h.call(EE_SCREENS_SE_OP_SHOW, SE_TRIGGER_SECONDS_LABEL, mode);
        }
        EE_SCREENS_SE_TRIGGER_CHANGE_INDEX => {
            let step = match h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0) {
                SE_TRIGGER_PREV_CONDITIONS => Some(0),
                SE_TRIGGER_NEXT_CONDITIONS => Some(1),
                SE_TRIGGER_PREV_EFFECTS => Some(2),
                SE_TRIGGER_NEXT_EFFECTS => Some(3),
                _ => None,
            };
            if let Some(step) = step {
                h.call(EE_SCREENS_SE_OP_TRIGGER_STEP, step, 0);
            }
        }
        EE_SCREENS_SE_TRIGGER_REMOVE => {
            let id = h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0);
            h.call(EE_SCREENS_SE_OP_TRIGGER_REMOVE, id, 0);
        }
        EE_SCREENS_SE_NEW_PLAYER => {
            h.call(EE_SCREENS_SE_OP_UNIT_RESELECT, 0, 0);
            h.call(EE_SCREENS_SE_OP_PLAYER_FILL_TECH, 0, 0);
        }
        EE_SCREENS_SE_HOME_ROTATION => {
            h.call(EE_SCREENS_SE_OP_HOME_ROTATION, 0, 0);
        }
        EE_SCREENS_SE_OPEN_CIV_BUILDER => {
            return Some(h.call(EE_SCREENS_SE_OP_OPEN_CIV_BUILDER, 0, 0) as i32 as core::ffi::c_long);
        }
        EE_SCREENS_SE_LOAD_TRIGGERS_ONLY => {
            h.call(EE_SCREENS_SE_OP_LOAD_TRIGGERS, 0, 0);
        }
        EE_SCREENS_SE_ELEVATION_TOOL => elevation_tool(h),
        EE_SCREENS_SE_DISPLAY_EPOCH => {
            h.call(EE_SCREENS_SE_OP_PLAYER_REBUILD_TECH, 0, 0);
        }
        EE_SCREENS_SE_CLEAR_ADD_LINE => {
            // the text box and its label hidden for the clear radio, shown for the add-line one
            let mode = if h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0) == SE_AREA_RADIO { SE_HIDDEN } else { SE_SHOWN };
            h.call(EE_SCREENS_SE_OP_SHOW, SE_TEXT_EDIT_BOX, mode);
            h.call(EE_SCREENS_SE_OP_SHOW, SE_PLACE_LABEL, mode);
        }
        EE_SCREENS_SE_PLACE => {
            let areas = h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0) == SE_AREA_RADIO;
            h.call(EE_SCREENS_SE_OP_PLACE_FILL, areas as u32, 0);
        }
        EE_SCREENS_SE_LOAD_SCENARIO => {
            let screen = if h.call(EE_SCREENS_SE_OP_IN_EDITOR, 0, 0) != 0 {
                crate::save_load::EE_SCREENS_SL_LOAD_SCREEN_SCENARIO
            } else {
                crate::save_load::EE_SCREENS_SL_LOAD_SCREEN_GAME
            };
            h.call(EE_SCREENS_SE_OP_POST_LOAD_EVENT, screen, 0);
        }
        EE_SCREENS_SE_SAVE_SCENARIO => {
            h.call(EE_SCREENS_SE_OP_POST_ACTION, EE_SCREENS_SE_ACTION_SAVE, 0);
        }
        EE_SCREENS_SE_TEST_SCENARIO => {
            h.call(EE_SCREENS_SE_OP_POST_ACTION, EE_SCREENS_SE_ACTION_TEST, 0);
        }
        EE_SCREENS_SE_EXIT_SCENARIO => {
            h.call(EE_SCREENS_SE_OP_ASK_EXIT, 0, 0);
        }
        EE_SCREENS_SE_SAVE_BEFORE_EXIT => {
            h.call(EE_SCREENS_SE_OP_POST_ACTION, EE_SCREENS_SE_ACTION_SAVE, 0);
            h.call(EE_SCREENS_SE_OP_EXIT_AFTER_SAVE, 0, 0);
        }
        EE_SCREENS_SE_EXIT_WITHOUT_SAVING => {
            h.call(EE_SCREENS_SE_OP_POST_ACTION, EE_SCREENS_SE_ACTION_EXIT, 0);
        }
        EE_SCREENS_SE_FIND_TEXT => find_text(h),
        _ => return None,
    }
    Some(0)
}

/// The elevation tab's tool radios: raise shows the elevation slider, lower the strength slider (each with the
/// titles and the value label, the value 40 less the elevation, or 40 less four times the strength); level hides
/// them all.
fn elevation_tool(h: &mut dyn SeHost) {
    const TITLES: [u32; 3] = [SE_ELEVATION_TITLE, SE_ELEVATION_STRENGTH_TITLE, SE_ELEVATION_STRENGTH_LABEL];
    let value = match h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0) {
        SE_ELEVATION_RAISE => {
            h.call(EE_SCREENS_SE_OP_SHOW, SE_ELEVATION_STRENGTH_SLIDER, SE_HIDDEN);
            h.call(EE_SCREENS_SE_OP_SHOW, SE_ELEVATION_SLIDER, SE_SHOWN);
            for id in TITLES {
                h.call(EE_SCREENS_SE_OP_SHOW, id, SE_SHOWN);
            }
            40i32.wrapping_sub(h.call(EE_SCREENS_SE_OP_RANGE, SE_ELEVATION_SLIDER, 0) as i32)
        }
        SE_ELEVATION_LOWER => {
            h.call(EE_SCREENS_SE_OP_SHOW, SE_ELEVATION_STRENGTH_SLIDER, SE_SHOWN);
            h.call(EE_SCREENS_SE_OP_SHOW, SE_ELEVATION_SLIDER, SE_HIDDEN);
            for id in TITLES {
                h.call(EE_SCREENS_SE_OP_SHOW, id, SE_SHOWN);
            }
            let strength = h.call(EE_SCREENS_SE_OP_RANGE, SE_ELEVATION_STRENGTH_SLIDER, 0) as i32;
            ftol(40.0f32 - strength as f32 * 4.0f32)
        }
        SE_ELEVATION_LEVEL => {
            h.call(EE_SCREENS_SE_OP_SHOW, SE_ELEVATION_STRENGTH_SLIDER, SE_HIDDEN);
            h.call(EE_SCREENS_SE_OP_SHOW, SE_ELEVATION_SLIDER, SE_HIDDEN);
            for id in TITLES {
                h.call(EE_SCREENS_SE_OP_SHOW, id, SE_HIDDEN);
            }
            return;
        }
        _ => return,
    };
    h.call(EE_SCREENS_SE_OP_LABEL_NUMBER, SE_ELEVATION_STRENGTH_LABEL, value as u32);
}

/// The C cast `(long)` of a float as x86-64 compiles it: toward zero, `i32::MIN` for NaN or out of range.
fn ftol(value: f32) -> i32 {
    if (-2_147_483_648.0..2_147_483_648.0).contains(&value) {
        value as i32
    } else {
        i32::MIN
    }
}

/// `atoi` of a run of decimal digits (the CRT's: past `i32::MAX` it answers `i32::MAX`; an empty run 0).
fn atoi(digits: &[u8]) -> i32 {
    let mut value: i64 = 0;
    for &d in digits {
        value = (value * 10 + i64::from(d - b'0')).min(i64::from(i32::MAX) + 1);
    }
    value.min(i64::from(i32::MAX)) as i32
}

/// The instructions' edit boxes' Enter: a text starting "///" followed by string ids separated by commas (read
/// across the box's lines) becomes those strings of the game run together; the expansion stops at the first other
/// character (the ids read so far are kept). An empty result leaves the box as it is.
fn find_text(h: &mut dyn SeHost) {
    let lines = h.call(EE_SCREENS_SE_OP_EDIT_LINES, 0, 0);
    let first = h.text(EE_SCREENS_SE_TEXT_EDIT_LINE, 0);
    if !first.starts_with(b"///") {
        return;
    }
    let mut text: Vec<u8> = Vec::new();
    let mut number: Vec<u8> = Vec::new();
    let mut line = 0u32;
    let mut current = first;
    let mut at = 3usize;
    while line < lines {
        match current.get(at).copied().unwrap_or(0) {
            0 => {
                line += 1;
                if lines <= line {
                    break;
                }
                current = h.text(EE_SCREENS_SE_TEXT_EDIT_LINE, line);
                at = 0;
                continue;
            }
            d @ b'0'..=b'9' => number.push(d),
            b',' => {
                text.extend(h.text(EE_SCREENS_SE_TEXT_STRING, atoi(&number) as u32));
                number.clear();
            }
            _ => break,
        }
        at += 1;
    }
    if !number.is_empty() {
        text.extend(h.text(EE_SCREENS_SE_TEXT_STRING, atoi(&number) as u32));
    }
    if !text.is_empty() {
        h.set_edit_text(&text);
    }
}

/// The object screen's mode radios: picking units on the map or not; leaving map mode clears the editor's unit
/// selection (the manager's form's and the event's form's, the same form) and the object's unit list.
fn object_detail(h: &mut dyn SeHost) {
    let id = h.call(EE_SCREENS_SE_OP_ENTITY_ID, 0, 0);
    h.call(EE_SCREENS_SE_OP_OBJECT_SET_MAP, (id == SE_OBJECT_UNITS_RADIO) as u32, 0);
    h.call(EE_SCREENS_SE_OP_OBJECT_REFRESH, 0, 0);
    if h.call(EE_SCREENS_SE_OP_OBJECT_IS_MAP, 0, 0) == 0 {
        h.call(EE_SCREENS_SE_OP_MANAGER_CLEAR_SELECTION, 0, 0);
        h.call(EE_SCREENS_SE_OP_OBJECT_CLEAR_UNIT_LIST, 0, 0);
        h.call(EE_SCREENS_SE_OP_FORM_CLEAR_SELECTION, 0, 0);
        h.call(EE_SCREENS_SE_OP_OBJECT_CLEAR_IDS, 0, 0);
    }
    h.call(EE_SCREENS_SE_OP_OBJECT_UPDATE_SELECTION, 0, 0);
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod golden2;
