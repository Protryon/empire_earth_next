//! The object effect types (`EESETSETObject` and its thirteen actions: create, freeze, unfreeze, kill, remove,
//! `EESETSETObjectToObject`, `EESETSETObjectToArea`, `EESETSETSetObjectStance`, `EESETSETSetObjectAttribute`,
//! `EESETSETSetObjectOwner`, `EESETSETSetObjectName`, `EESETSETLogObject`, `EESETSETSetObjectClassAttribute`) over
//! the effect screen's controls (`editor_effects.rs`).

use core::cell::RefCell;

use crate::screens::editor_effects::*;
use crate::screens::editor_players::SeplHost;

/// `kSMObjectAction*`.
const TASK_OBJECT: u32 = 5;
const TASK_AREA: u32 = 6;
const STANCE: u32 = 7;
const UNIT_ATTRIBUTE: u32 = 8;
const OWNER: u32 = 9;
const CLASS_NAME: u32 = 10;
const FACE_OBJECT: u32 = 11;
const CLASS_ATTRIBUTE: u32 = 12;

/// The task actions' first string and the value its line stores (`theIndex - 0x72bf`).
const TASK_ACTION_BASE: u32 = 0x72bf;

/// The object family: its action, the two task types' action lists (`mActions`), the stance type's own event.
#[derive(Debug, Default)]
pub struct ObjectTypes {
    pub family: Family,
    pub to_object: RefCell<Vec<i32>>,
    pub to_area: RefCell<Vec<i32>>,
    pub stance_event: usize,
}

impl ObjectTypes {
    /// The constructor: the stance type's event (made with the types), then the family's.
    pub fn new<H: SeplHost + ?Sized>(c: &mut Fx<H>) -> ObjectTypes {
        let stance_event = c.screen_event(EE_SCREENS_SE_CHANGE_EFFECT_ACTION);
        let event = c.screen_event(EE_SCREENS_SE_CHANGE_EFFECT_ACTION);
        ObjectTypes { family: Family { event, ..Default::default() }, stance_event, ..Default::default() }
    }

    pub fn call<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, slot: u32) {
        let cur = || self.family.current.get();
        match slot {
            EE_SCREENS_SEET_CHANGE_ACTION => {
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
                if let Some(s) = c.selection(COMBO, 0) {
                    self.family.current.set(s);
                }
                self.sub(c, cur(), EE_SCREENS_SEET_SHOW);
                self.sub(c, cur(), EE_SCREENS_SEET_LOAD);
                self.update_name(c);
            }
            EE_SCREENS_SEET_STORE => {
                if let Some(s) = c.selection(COMBO, 0) {
                    c.e().mObjectAction = s;
                    self.family.current.set(s);
                }
                if let Some(s) = c.selection(SCEN, 1) {
                    c.e().mObject = s;
                }
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
                self.sub(c, cur(), EE_SCREENS_SEET_STORE);
                c.event_null(COMBO, 0, T_SELECTION);
            }
            EE_SCREENS_SEET_HIDE => {
                c.hide(LABEL, 0);
                c.hide(LABEL, 1);
                c.hide(COMBO, 0);
                c.hide(LABEL, 2);
                c.hide(SCEN, 1);
                self.sub(c, cur(), EE_SCREENS_SEET_HIDE);
            }
            EE_SCREENS_SEET_LOAD => {
                let action = c.e().mObjectAction;
                self.family.current.set(action);
                c.event_null(COMBO, 0, T_SELECTION);
                c.select(COMBO, 0, action);
                self.sub(c, cur(), EE_SCREENS_SEET_SHOW);
                let o = c.e().mObject;
                c.select(SCEN, 1, o);
                c.event_set(COMBO, 0, T_SELECTION, self.family.event);
                self.sub(c, cur(), EE_SCREENS_SEET_LOAD);
            }
            EE_SCREENS_SEET_SHOW => {
                show_actions(c, IDS_OBJECT_EFFECT, 300.0, IDS_TRIGGER_OBJECT_EFFECT_FIRST, IDS_CLASS_ATTRIBUTE);
                label(c, 2, IDS_TRIGGER_SCREEN_TAB_OBJECT, 220.0, 641.0, 200.0, 20.0);
                c.locate(SCEN, 1, 220.0, 666.0);
                c.width(SCEN, 1, 200);
                c.clear(SCEN, 1);
                c.scen_mode(1, MODE_OBJECT);
                c.fill_names(SCEN, 1, NAMES_OBJECTS, false);
                c.show(SCEN, 1);
            }
            EE_SCREENS_SEET_UPDATE_NAME => self.update_name(c),
            _ => {}
        }
    }

    /// `UpdateName`: `<first word of the action> <object>` and the stance or attribute picked.
    fn update_name<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>) {
        if !default_named(c) {
            return;
        }
        let cur = self.family.current.get();
        let text = c.string(cur.wrapping_add(IDS_TRIGGER_OBJECT_EFFECT_FIRST));
        let mut name = b"<".to_vec();
        name.extend_from_slice(first_word(&text));
        // the original reads an unset selection when there is none: no object then
        let object = match c.selection(SCEN, 1).and_then(|s| c.object_name(s)) {
            Some(n) => n,
            None => c.string(IDS_NONE),
        };
        let detail = |c: &mut Fx<H>, combo: u32, first: u32| match c.selection(ECOMBO, combo) {
            Some(s) => c.string(s.wrapping_add(first)),
            None => Vec::new(),
        };
        let detail = match cur {
            0..=6 | OWNER | CLASS_NAME | FACE_OBJECT => None,
            STANCE => Some(detail(c, 1, IDS_TRIGGER_UNIT_BEHAVIOR_FIRST)),
            UNIT_ATTRIBUTE => Some(detail(c, 0, IDS_TRIGGER_ATTRIBUTE_FIRST)),
            CLASS_ATTRIBUTE => Some(detail(c, 0, IDS_TRIGGER_ARMOR_ATTRIBUTE_FIRST)),
            _ => {
                rename(c, &name);
                return;
            }
        };
        name.push(b' ');
        name.extend_from_slice(&object);
        if let Some(d) = detail {
            name.push(b' ');
            name.extend_from_slice(&d);
        }
        name.push(b'>');
        rename(c, &name);
    }

    /// One action's slot (the empty actions 0..=4 have none).
    fn sub<H: SeplHost + ?Sized>(&self, c: &mut Fx<H>, k: u32, slot: u32) {
        match k {
            TASK_OBJECT => task(c, &self.to_object, false, slot),
            TASK_AREA => task(c, &self.to_area, true, slot),
            STANCE => stance(c, slot),
            UNIT_ATTRIBUTE => attribute(c, false, slot),
            OWNER => owner(c, slot),
            CLASS_NAME => class_name(c, slot),
            FACE_OBJECT => face_object(c, slot),
            CLASS_ATTRIBUTE => attribute(c, true, slot),
            _ => {}
        }
    }
}

/// "to" centred left of a column (`mLabels[k]` at (425, 641), 25 wide).
fn to_label<H: SeplHost + ?Sized>(c: &mut Fx<H>, k: u32) {
    c.set_rect(LABEL, k, 425.0, 641.0, 25.0, 20.0);
    c.text_id(LABEL, k, IDS_TO);
    c.align(LABEL, k, AL_CENTER);
    c.show(LABEL, k);
}

/// `EESETSETObjectToObject` (the target object `mScenarioComboBoxes[2]`, the actions `mEffectComboBoxes[1]`) and
/// `EESETSETObjectToArea` (the area `mScenarioComboBoxes[3]`, the actions `mEffectComboBoxes[0]`).
fn task<H: SeplHost + ?Sized>(c: &mut Fx<H>, actions: &RefCell<Vec<i32>>, area: bool, slot: u32) {
    let (scen, combo) = if area { (3, 0) } else { (2, 1) };
    match slot {
        EE_SCREENS_SEET_STORE => {
            if let Some(s) = c.selection(SCEN, scen) {
                if area {
                    c.e().mTaskArea = s;
                } else {
                    c.e().mTaskObject = s;
                }
            }
            // the original indexes its list with an unset selection when there is none: nothing stored then
            if let Some(s) = c.selection(ECOMBO, combo) {
                if let Some(&a) = actions.borrow().get(s as usize) {
                    c.e().mTaskAction = a as u32;
                }
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 5);
            c.align(LABEL, 5, AL_LEFT);
            c.hide(LABEL, 3);
            c.hide(LABEL, 4);
            c.hide(SCEN, scen);
            c.hide(ECOMBO, combo);
        }
        EE_SCREENS_SEET_LOAD => {
            let target = if area { c.e().mTaskArea } else { c.e().mTaskObject };
            let value = c.e().mTaskAction as i32;
            c.select(SCEN, scen, target);
            let found = actions.borrow().iter().position(|&a| a == value);
            match found {
                Some(p) => c.select(ECOMBO, combo, p as u32),
                None => {
                    if c.line_count(ECOMBO, combo) > 0 {
                        c.select(ECOMBO, combo, 0);
                    }
                }
            }
        }
        EE_SCREENS_SEET_SHOW => {
            to_label(c, 5);
            let x = c.right(LABEL, 5) + 5.0;
            let title = if area { IDS_AREA } else { IDS_TRIGGER_SCREEN_TAB_OBJECT };
            label(c, 3, title, x, 616.0, 200.0, 20.0);
            let y = c.bottom(LABEL, 3) + 5.0;
            c.locate(SCEN, scen, x, y);
            c.width(SCEN, scen, 200);
            c.clear(SCEN, scen);
            c.scen_mode(scen, if area { MODE_AREA } else { MODE_OBJECT });
            c.fill_names(SCEN, scen, if area { NAMES_AREAS } else { NAMES_OBJECTS }, false);
            c.show(SCEN, scen);
            let y = c.bottom(SCEN, scen) + 10.0;
            label(c, 4, IDS_ACTION, x, y, 200.0, 20.0);
            let y = c.bottom(LABEL, 4) + 5.0;
            c.locate(ECOMBO, combo, x, y);
            c.width(ECOMBO, combo, 200);
            c.clear(ECOMBO, combo);
            actions.borrow_mut().clear();
            c.clear(ECOMBO, combo);
            for id in IDS_NORMAL..=IDS_TRIGGER_GARRISON {
                c.append_id(ECOMBO, combo, id);
                actions.borrow_mut().push(id.wrapping_sub(TASK_ACTION_BASE) as i32);
            }
            for (id, name) in c.calamities() {
                actions.borrow_mut().push(id.wrapping_add(6) as i32);
                c.append_id(ECOMBO, combo, name);
            }
            c.show(ECOMBO, combo);
        }
        _ => {}
    }
}

/// `EESETSETSetObjectStance`: the stance (`mEffectComboBoxes[1]`).
fn stance<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            if let Some(s) = c.selection(ECOMBO, 1) {
                c.e().mStance = s;
            }
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(LABEL, 4);
            c.hide(ECOMBO, 1);
        }
        EE_SCREENS_SEET_LOAD => {
            let s = c.e().mStance;
            c.select_silent(ECOMBO, 1, s);
        }
        EE_SCREENS_SEET_SHOW => {
            to_label(c, 3);
            let x = c.right(LABEL, 3) + 5.0;
            label(c, 4, IDS_TRIGGER_EDITOR_STANCE, x, 641.0, 200.0, 20.0);
            let y = c.bottom(LABEL, 4) + 5.0;
            c.locate(ECOMBO, 1, x, y);
            c.width(ECOMBO, 1, 200);
            c.clear(ECOMBO, 1);
            c.append_ids(ECOMBO, 1, IDS_TRIGGER_UNIT_BEHAVIOR_FIRST, IDS_AGGRESSIVE);
            c.show(ECOMBO, 1);
        }
        _ => {}
    }
}

/// `EESETSETSetObjectAttribute` and `EESETSETSetObjectClassAttribute` (`class`: the armour attributes from 20,
/// increase or decrease only): the attribute (`mEffectComboBoxes[0]`), the mode (`[1]`), percent (`mCheckBoxes[0]`),
/// the amount (`mEditBoxes[0]`).
fn attribute<H: SeplHost + ?Sized>(c: &mut Fx<H>, class: bool, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let percent = c.checked(CHECK, 0);
            c.e().mUsesPercent = percent;
            // the original stores an unset selection when there is none: kept then
            if let Some(s) = c.selection(ECOMBO, 1) {
                c.e().mAttributeMode = s;
            }
            if let Some(s) = c.selection(ECOMBO, 0) {
                c.e().mAttribute = if class { s.wrapping_add(0x14) } else { s };
            }
            let amount = c.text_int(EDIT, 0);
            c.e().mAttributeAmount = amount as u32;
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(ECOMBO, 1);
            c.hide(ECOMBO, 0);
            c.hide(LABEL, 3);
            c.hide(CHECK, 0);
            c.hide(EDIT, 0);
        }
        EE_SCREENS_SEET_LOAD => {
            let (a, mode, amount, percent) =
                (c.e().mAttribute, c.e().mAttributeMode, c.e().mAttributeAmount, c.e().mUsesPercent);
            if class {
                let held = c.event_hold(ECOMBO, 0, T_SELECTION);
                c.select(ECOMBO, 0, a.wrapping_sub(0x14));
                if c.selection(ECOMBO, 0).is_none() {
                    c.select(ECOMBO, 0, 0);
                }
                c.event_restore(ECOMBO, 0, T_SELECTION, held);
                c.select(ECOMBO, 1, if mode < 2 { mode } else { 0 });
                c.number(EDIT, 0, amount as i32);
                c.set_checked(0, percent);
            } else {
                c.select_silent(ECOMBO, 0, a);
                c.select_silent(ECOMBO, 1, mode);
                c.number(EDIT, 0, amount as i32);
                c.set_checked_if(0, percent);
            }
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 3, IDS_TRIGGER_EDITOR_ATTRIBUTE_7695, 430.0, 616.0, 200.0, 20.0);
            c.locate(ECOMBO, 0, 430.0, 641.0);
            c.width(ECOMBO, 0, 200);
            c.clear(ECOMBO, 0);
            if class {
                c.append_ids(ECOMBO, 0, IDS_TRIGGER_ARMOR_ATTRIBUTE_FIRST, IDS_CAMERA_FOCUS_PLUS);
            } else {
                c.append_ids(ECOMBO, 0, IDS_TRIGGER_ATTRIBUTE_FIRST, 0x71c0 - 1);
            }
            c.show(ECOMBO, 0);
            let y = c.bottom(ECOMBO, 0) + 10.0;
            c.locate(ECOMBO, 1, 430.0, y);
            c.width(ECOMBO, 1, 100);
            c.clear(ECOMBO, 1);
            c.append_ids(ECOMBO, 1, IDS_INCREASE, if class { IDS_DECREASE } else { IDS_EXACTLY });
            c.show(ECOMBO, 1);
            let y = c.bottom(ECOMBO, 1) + 5.0;
            radio_text(c, CHECK, 0, IDS_USE_PERCENT);
            c.locate(CHECK, 0, 430.0, y);
            c.width(CHECK, 0, 100);
            c.show(CHECK, 0);
            let (x, y) = (c.right(ECOMBO, 1) + 5.0, c.top(ECOMBO, 1));
            c.number(EDIT, 0, 0);
            c.locate(EDIT, 0, x, y);
            c.show(EDIT, 0);
        }
        _ => {}
    }
}

/// `EESETSETSetObjectOwner`: the new owner (`mPlayers[0]`).
fn owner<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let p = c.player(0);
            c.e().mNewOwner = p;
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.align(LABEL, 3, AL_LEFT);
            c.hide(LABEL, 4);
            c.hide(PLAYER, 0);
        }
        EE_SCREENS_SEET_LOAD => {
            let p = c.e().mNewOwner;
            c.set_player_silent(0, p);
        }
        EE_SCREENS_SEET_SHOW => {
            to_label(c, 3);
            let x = c.right(LABEL, 3) + 5.0;
            label(c, 4, IDS_TRIGGER_EDITOR_OWNER, x, 641.0, 175.0, 20.0);
            let y = c.bottom(LABEL, 4) + 5.0;
            c.locate(PLAYER, 0, x, y);
            c.player_flags(0, 0xe7);
            c.show(PLAYER, 0);
        }
        _ => {}
    }
}

/// `EESETSETSetObjectName`: the class's new name (`mTextEditBox`).
fn class_name<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let name = c.get_text(TEXT, 0);
            c.set_effect_text(EE_SCREENS_SEET_S_NEW_NAME, &name);
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(TEXT, 0);
            c.hide(LABEL, 3);
        }
        EE_SCREENS_SEET_LOAD => {
            let name = c.effect_text(EE_SCREENS_SEET_S_NEW_NAME);
            c.text(TEXT, 0, &name);
        }
        EE_SCREENS_SEET_SHOW => {
            label(c, 3, IDS_TRIGGER_EDITOR_NAME, 430.0, 641.0, 150.0, 20.0);
            c.text(TEXT, 0, b"");
            c.locate(TEXT, 0, 430.0, 666.0);
            c.font_530(TEXT, 0);
            c.width(TEXT, 0, 0x96);
            c.show(TEXT, 0);
        }
        _ => {}
    }
}

/// `EESETSETLogObject` (face an object): the degrees (`mEditBoxes[0]`) and snap (`mCheckBoxes[0]`) beside the
/// object combo box.
fn face_object<H: SeplHost + ?Sized>(c: &mut Fx<H>, slot: u32) {
    match slot {
        EE_SCREENS_SEET_STORE => {
            let degrees = c.text_int(EDIT, 0);
            c.e().mAttributeAmount = degrees as u32;
            let snap = c.checked(CHECK, 0);
            c.e().mUsesPercent = snap;
        }
        EE_SCREENS_SEET_HIDE => {
            c.hide(LABEL, 3);
            c.hide(LABEL, 4);
            c.hide(CHECK, 0);
            c.hide(EDIT, 0);
        }
        EE_SCREENS_SEET_LOAD => {
            let (degrees, snap) = (c.e().mAttributeAmount, c.e().mUsesPercent);
            c.number(EDIT, 0, degrees as i32);
            c.set_checked_if(0, snap);
        }
        EE_SCREENS_SEET_SHOW => {
            let x = c.right(SCEN, 1) + 5.0;
            label(c, 3, IDS_DEGREES, x, 641.0, 100.0, 20.0);
            let (x, y) = (c.right(SCEN, 1) + 5.0, c.top(SCEN, 1));
            c.number(EDIT, 0, 0);
            c.locate(EDIT, 0, x, y);
            c.show(EDIT, 0);
            let x = c.right(EDIT, 0) + 50.0;
            label(c, 4, IDS_SNAP, x, 641.0, 100.0, 20.0);
            let (x, y) = (c.right(EDIT, 0) + 50.0, c.top(SCEN, 1));
            c.locate(CHECK, 0, x, y);
            c.width(CHECK, 0, 0x14);
            c.show(CHECK, 0);
        }
        _ => {}
    }
}
