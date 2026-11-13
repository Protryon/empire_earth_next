//! The scenario editor's events (the editor's form, `EEUIFScenarioEditor`, and its trigger tab): each event is an
//! `EEScreenEvent` code and its handler here, over a host ([`SeHost`]) whose calls are each one call of the editor's
//! C++ objects (the trigger tab `EESETriggerScreen` and its sub-screens, the editor's selection, the camera action),
//! `ee_screenscpp_se` in `Scenario Editor/EESEEvents.cpp`. The C++ keeps the screens, the controls and the trigger
//! records; the events that carry per-instance state (the slider's format, the check box's controls, the scenario
//! combo box's refill) stay C++ classes there.

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
const SE_HIDDEN: u32 = 0;
const SE_SHOWN: u32 = 1;

/// The editor's C++ objects, one call each.
pub trait SeHost {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32;
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
        _ => return None,
    }
    Some(0)
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
