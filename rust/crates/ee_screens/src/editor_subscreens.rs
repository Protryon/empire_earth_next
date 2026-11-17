//! The scenario editor's trigger tab condition and effect screens whole in Rust (`EESETSCondition`, once
//! `EESETSCondition.cpp`, and `EESETSEffect`, once the screen half of `EESETSEffect.cpp`): the type title and combo
//! box they make (records for the shared maker, `EeScreensSeplCtl`), their shared controls
//! (`EESETSConditionControls`, `EESETSEffectControls`, made and deleted by the types' host), the condition or effect
//! the trigger tab's list selects (stored, loaded, named), the type switch, the list's operations, and their types
//! (`editor_conditions.rs`, `editor_effects.rs`).
//!
//! Two hosts: the shared maker's (`ee_screenscpp_sepl`: the title and the type combo box, by handle) and the types'
//! (`ee_screenscpp_sect` / `ee_screenscpp_seet`: the controls by array, the entry, the scenario's list and the trigger
//! tab). A host call can come back into a screen (picking a line of the type combo box runs `ChangeConditionType`):
//! the screens are used through `&self`, their type a `Cell`.

use crate::editor_controls::{
    condition_controls, delete_controls, effect_controls, make_controls, CONDITION_DELETE_ORDER, EFFECT_DELETE_ORDER,
};
use core::cell::Cell;

use crate::editor_conditions::{ConditionTypes, Cx};
use crate::editor_effects::*;
use crate::editor_object::{
    screen_event, EE_SCREENS_SEOB_SM_DELETE, EE_SCREENS_SEOB_SM_DOWN, EE_SCREENS_SEOB_SM_DUPLICATE,
    EE_SCREENS_SEOB_SM_NEW, EE_SCREENS_SEOB_SM_SORT, EE_SCREENS_SEOB_SM_UP, EE_SCREENS_SEPL_COMBO_LINES_STRINGS,
    EE_SCREENS_SEPL_COMBO_PLAIN, EE_SCREENS_SEPL_ON_SELECTION,
};
use crate::editor_players::{
    delete, display, rect, set_selection, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_FG, EE_SCREENS_SEPL_LABEL,
    EE_SCREENS_SEPL_LIST_COMBO, YELLOW_BG,
};
use crate::scenario_editor::{EE_SCREENS_SE_CHANGE_CONDITION_TYPE, EE_SCREENS_SE_CHANGE_EFFECT_TYPE};

// ---- the screens' slots (`ee_screens_secd_call`'s and `ee_screens_sefx_call`'s `what`)

pub const EE_SCREENS_SESC_SORT: u32 = 1;
pub const EE_SCREENS_SESC_MOVE_DOWN: u32 = 2;
pub const EE_SCREENS_SESC_MOVE_UP: u32 = 3;
pub const EE_SCREENS_SESC_DELETE: u32 = 4;
pub const EE_SCREENS_SESC_DUPLICATE: u32 = 5;
pub const EE_SCREENS_SESC_NEW: u32 = 6;
pub const EE_SCREENS_SESC_ACTIVATE: u32 = 7;
pub const EE_SCREENS_SESC_STORE: u32 = 8;
pub const EE_SCREENS_SESC_HIDE: u32 = 9;
pub const EE_SCREENS_SESC_LOAD: u32 = 10;
pub const EE_SCREENS_SESC_SHOW: u32 = 11;
pub const EE_SCREENS_SESC_REFRESH: u32 = 12;
/// `ChangeConditionType` / `ChangeEffectType`; `ChangeEffectAction`.
pub const EE_SCREENS_SESC_CHANGE_TYPE: u32 = 13;
pub const EE_SCREENS_SESC_CHANGE_ACTION: u32 = 14;

/// The trigger tab's control groups of the condition and effect screens.
const GROUP_CONDITION: u32 = 12;
const GROUP_EFFECT: u32 = 13;

/// `kSMConditionAlwaysTrue`, `Object`, `PlayerAttribute`, `PlayerTechnology`, `TriggerAttribute`,
/// `PlayerDiplomacy`, `ChatMessageContains`.
const C_ALWAYS_TRUE: u32 = 0;
const C_OBJECT: u32 = 1;
const C_PLAYER_ATTRIBUTE: u32 = 2;
const C_PLAYER_TECHNOLOGY: u32 = 3;
const C_TRIGGER_ATTRIBUTE: u32 = 4;
const C_PLAYER_DIPLOMACY: u32 = 5;
const C_CHAT_MESSAGE: u32 = 7;

/// An entry: its pointer and its numbers' (the host's `EE_SCREENS_SEET_OP_ENTRY`).
type Entry = Option<(usize, usize)>;

/// The title label and the type combo box of a condition or effect screen (control group `g`, ids `ids`): the title
/// at (329, 504), the combo box 4 pixels below it with the six strings from `first`, its selection-changed event
/// `code`.
#[allow(clippy::too_many_arguments)]
fn make_title_and_combo<S: SeplHost + ?Sized>(
    s: &mut S,
    form: usize,
    index: &mut u32,
    ids: (u32, u32),
    title: u32,
    first: u32,
    code: u32,
    g: u32,
) -> (usize, usize) {
    let mut m = Maker { h: s, form, index: *index };
    let t = m.make(
        EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, ids.0, 329.0, 504.0, 200.0, 20.0)
            .string(title)
            .flags(EE_SCREENS_SEPL_FG)
            .bg(YELLOW_BG)
            .group(g),
    );
    let y = rect(m.h, t)[3] + 4.0;
    let c = m.make(
        EeScreensSeplCtl::new(EE_SCREENS_SEPL_LIST_COMBO, ids.1, 329.0, y, 200.0, 20.0)
            .flags(EE_SCREENS_SEPL_FG)
            .text(0, first, 6)
            .bg(1.0)
            .bg2(YELLOW_BG)
            .group(g)
            .extra([EE_SCREENS_SEPL_COMBO_PLAIN, EE_SCREENS_SEPL_COMBO_LINES_STRINGS, 2, 1]),
    );
    *index = m.index;
    screen_event(s, c, code, EE_SCREENS_SEPL_ON_SELECTION);
    (t, c)
}

/// One call's context of a screen over the types' host: the controls and the entry, the types' handle.
fn at<H: SeplHost + ?Sized, F>(h: &mut H, controls: usize, entry: Entry, me: usize) -> Fx<'_, H, F> {
    let (effect, fields) = entry.unwrap_or((0, 0));
    Fx { h, controls, effect, fields: fields as *mut F, me }
}

/// `Load`'s first lines: the trigger tab's text box shown, its name and text boxes set to the entry's.
fn show_texts<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>) {
    c.tscreen(EE_SCREENS_SEET_TS_SHOW_TEXT);
    let name = c.effect_text(EE_SCREENS_SEET_S_NAME);
    c.set_name_box(&name);
    let text = c.effect_text(EE_SCREENS_SEET_S_TEXT);
    c.set_text_box(&text);
}

/// `Store`'s first lines: the entry named and described from the trigger tab's boxes.
fn store_texts<H: SeplHost + ?Sized, F>(c: &mut Fx<H, F>) {
    let name = c.name_box();
    c.set_effect_text(EE_SCREENS_SEET_S_NAME, &name);
    let text = c.text_box();
    c.set_effect_text(EE_SCREENS_SEET_S_TEXT, &text);
}

/// The list operation of a slot (`EE_SCREENS_SESC_SORT` .. `_NEW`) on the entry `k`: the host's operation and
/// entry, `None` for another slot.
fn list_op(what: u32, k: i32) -> Option<(i32, i32)> {
    Some(match what {
        EE_SCREENS_SESC_SORT => (EE_SCREENS_SEOB_SM_SORT, 0),
        EE_SCREENS_SESC_MOVE_DOWN => (EE_SCREENS_SEOB_SM_DOWN, k),
        EE_SCREENS_SESC_MOVE_UP => (EE_SCREENS_SEOB_SM_UP, k),
        EE_SCREENS_SESC_DELETE => (EE_SCREENS_SEOB_SM_DELETE, k),
        EE_SCREENS_SESC_DUPLICATE => (EE_SCREENS_SEOB_SM_DUPLICATE, k),
        EE_SCREENS_SESC_NEW => (EE_SCREENS_SEOB_SM_NEW, 0),
        _ => return None,
    })
}

// ---- the condition screen

/// The condition screen (`EESETSCondition`): its controls, title, type combo box, types and current type.
#[derive(Debug, Default)]
pub struct ConditionScreen {
    pub controls: usize,
    pub title: usize,
    pub type_combo: usize,
    pub types: ConditionTypes,
    pub ty: Cell<u32>,
}

impl ConditionScreen {
    /// The constructor: the controls, the title, the type combo box (group 12) from the control index `*index`
    /// (advanced), the seven types; the object condition first.
    pub fn new<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(
        s: &mut S,
        h: &mut H,
        form: usize,
        index: &mut u32,
    ) -> ConditionScreen {
        let controls = at::<H, ()>(h, 0, None, 0).controls_new(form, index);
        make_controls(s, h, controls, form, index, &condition_controls());
        let (title, type_combo) = make_title_and_combo(
            s,
            form,
            index,
            (0x8fc, 0x8fd),
            IDS_TYPE_OF_CONDITION,
            IDS_TRIGGER_CONDITION_TYPE_FIRST,
            EE_SCREENS_SE_CHANGE_CONDITION_TYPE,
            GROUP_CONDITION,
        );
        let types = ConditionTypes::new(&mut at(h, controls, None, 0));
        ConditionScreen { controls, title, type_combo, types, ty: Cell::new(C_OBJECT) }
    }

    /// The destructor: the controls, the type combo box, the title, then the types.
    pub fn destroy<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(&self, s: &mut S, h: &mut H) {
        delete_controls(s, h, self.controls, &CONDITION_DELETE_ORDER);
        at::<H, ()>(h, self.controls, None, 0).controls_free();
        delete(s, self.type_combo);
        delete(s, self.title);
        self.types.destroy(&mut at(h, self.controls, None, 0));
    }

    fn cx<'a, H: SeplHost + ?Sized>(&self, h: &'a mut H, entry: Entry) -> Cx<'a, H> {
        at(h, self.controls, entry, 0)
    }

    fn entry<H: SeplHost + ?Sized>(&self, h: &mut H, sel: i32) -> Entry {
        self.cx(h, None).entry(sel.wrapping_add(1))
    }

    fn type_call<H: SeplHost + ?Sized>(&self, h: &mut H, slot: u32, entry: Entry) {
        self.types.call(&mut self.cx(h, entry), self.ty.get(), slot);
    }

    /// `GetNames`: the user conditions' names into the vector `names`.
    pub fn names<H: SeplHost + ?Sized>(&self, h: &mut H, names: usize) {
        self.cx(h, None).get_names(names);
    }

    /// The screen's slot `what` (`EE_SCREENS_SESC_*`) with the trigger tab's selection `sel` (`mSelection`).
    pub fn call<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(&self, s: &mut S, h: &mut H, what: u32, sel: i32) {
        if let Some((op, k)) = list_op(what, sel.wrapping_add(1)) {
            self.cx(h, None).list(op, k);
            return;
        }
        match what {
            EE_SCREENS_SESC_ACTIVATE => {
                {
                    let mut c = self.cx(h, None);
                    let n = c.player_count();
                    c.tscreen(EE_SCREENS_SEET_TS_HIDE_TEXT);
                    c.tscreen(EE_SCREENS_SEET_TS_SHOW_ALPHA_SORT);
                    for p in 0..2 {
                        c.set_player_count(p, n);
                    }
                }
                if self.ty.get() == C_ALWAYS_TRUE || self.ty.get() == C_OBJECT {
                    self.ty.set(C_PLAYER_ATTRIBUTE);
                }
                self.type_call(h, EE_SCREENS_SEET_SHOW, None);
                self.refresh(h, sel);
            }
            EE_SCREENS_SESC_STORE => {
                // the original reads a missing condition: nothing here
                let Some(e) = self.entry(h, sel) else { return };
                let mut c = self.cx(h, Some(e));
                if c.e().mType != C_ALWAYS_TRUE {
                    store_texts(&mut c);
                    self.types.call(&mut c, self.ty.get(), EE_SCREENS_SEET_STORE);
                    c.e().mType = self.ty.get();
                }
            }
            EE_SCREENS_SESC_HIDE => {
                display(s, self.title, false);
                display(s, self.type_combo, false);
            }
            EE_SCREENS_SESC_LOAD => self.load(s, h, sel),
            EE_SCREENS_SESC_SHOW => {
                display(s, self.title, true);
                display(s, self.type_combo, true);
            }
            EE_SCREENS_SESC_REFRESH => self.refresh(h, sel),
            EE_SCREENS_SESC_CHANGE_TYPE => {
                let e = self.entry(h, sel);
                let picked = self.cx(h, None).handle_selection(self.type_combo).unwrap_or(0);
                let ty = picked.wrapping_add(2);
                if ty == self.ty.get() {
                    return;
                }
                self.type_call(h, EE_SCREENS_SEET_HIDE, None);
                self.ty.set(ty);
                self.type_call(h, EE_SCREENS_SEET_SHOW, None);
                // the original loads a missing condition: not here
                if e.is_some() {
                    self.type_call(h, EE_SCREENS_SEET_LOAD, e);
                }
                self.refresh(h, sel);
            }
            _ => {}
        }
    }

    /// `Load`: the selected condition's name and text, its type shown and loaded (an object condition keeps its
    /// own screen).
    fn load<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(&self, s: &mut S, h: &mut H, sel: i32) {
        let Some(e) = self.entry(h, sel) else { return };
        let entry_ty = {
            let mut c = self.cx(h, Some(e));
            show_texts(&mut c);
            c.e().mType
        };
        if self.ty.get() != entry_ty {
            self.type_call(h, EE_SCREENS_SEET_HIDE, None);
            self.ty.set(entry_ty);
            if entry_ty > C_OBJECT {
                set_selection(s, self.type_combo, entry_ty.wrapping_sub(2));
            }
            if entry_ty != C_OBJECT {
                self.type_call(h, EE_SCREENS_SEET_SHOW, None);
            }
        }
        if entry_ty != C_OBJECT {
            self.type_call(h, EE_SCREENS_SEET_LOAD, Some(e));
            self.refresh(h, sel);
        }
    }

    /// `RefreshControls`: a condition still named `<...>` renamed from its type, player, chat text or trigger.
    fn refresh<H: SeplHost + ?Sized>(&self, h: &mut H, sel: i32) {
        let Some(e) = self.entry(h, sel) else { return };
        let mut c = self.cx(h, Some(e));
        if !default_named(&mut c) {
            return;
        }
        let ty = self.ty.get();
        let type_name = c.string(ty.wrapping_add(IDS_TRIGGER_CONDITION_TYPE_BASE));
        let mut name = first_word(&type_name).to_vec();
        if ty == C_PLAYER_TECHNOLOGY || ty == C_PLAYER_DIPLOMACY || ty == C_PLAYER_ATTRIBUTE {
            let p = c.player(0);
            let player = player_name(&mut c, p);
            name.push(b' ');
            name.extend_from_slice(&player);
        }
        match ty {
            C_CHAT_MESSAGE => {
                name.extend_from_slice(b"=\"");
                let text = c.get_text(TEXT, 0);
                name.extend_from_slice(&text);
                name.push(b'"');
            }
            C_TRIGGER_ATTRIBUTE => {
                let text = c.get_text(SCEN, 0);
                name.extend_from_slice(&text);
            }
            _ => {}
        }
        let mut full = b"<".to_vec();
        full.extend_from_slice(&name);
        full.push(b'>');
        rename(&mut c, &full);
    }
}

// ---- the effect screen

/// The effect screen (`EESETSEffect`): its controls, title, type combo box, types and current type.
#[derive(Debug, Default)]
pub struct EffectScreen {
    pub controls: usize,
    pub title: usize,
    pub type_combo: usize,
    pub types: EffectTypes,
    pub ty: Cell<u32>,
}

impl EffectScreen {
    /// The constructor: the controls, the title, the type combo box (group 13) from the control index `*index`
    /// (advanced), the seven types; the object effect first.
    pub fn new<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(
        s: &mut S,
        h: &mut H,
        form: usize,
        index: &mut u32,
    ) -> EffectScreen {
        let controls = at::<H, ()>(h, 0, None, 0).controls_new(form, index);
        make_controls(s, h, controls, form, index, &effect_controls());
        let (title, type_combo) = make_title_and_combo(
            s,
            form,
            index,
            (0x960, 0x961),
            IDS_TYPE_OF_EFFECT,
            IDS_TRIGGER_EFFECT_TYPE_FIRST,
            EE_SCREENS_SE_CHANGE_EFFECT_TYPE,
            GROUP_EFFECT,
        );
        let types = EffectTypes::new(&mut at(h, controls, None, 0));
        EffectScreen { controls, title, type_combo, types, ty: Cell::new(1) }
    }

    /// The destructor: the controls, the title, the type combo box, then the types (`me`: their handle).
    pub fn destroy<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(&self, s: &mut S, h: &mut H, me: usize) {
        delete_controls(s, h, self.controls, &EFFECT_DELETE_ORDER);
        at::<H, ()>(h, self.controls, None, me).controls_free();
        delete(s, self.title);
        delete(s, self.type_combo);
        self.types.destroy(&mut at(h, self.controls, None, me));
    }

    fn fx<'a, H: SeplHost + ?Sized>(&self, h: &'a mut H, entry: Entry, me: usize) -> Fx<'a, H> {
        at(h, self.controls, entry, me)
    }

    fn entry<H: SeplHost + ?Sized>(&self, h: &mut H, sel: i32) -> Entry {
        self.fx(h, None, 0).entry(sel.wrapping_add(1))
    }

    fn type_call<H: SeplHost + ?Sized>(&self, h: &mut H, me: usize, slot: u32, entry: Entry) {
        self.types.call(&mut self.fx(h, entry, me), self.ty.get(), slot);
    }

    /// `GetNames`: the user effects' names into the vector `names`.
    pub fn names<H: SeplHost + ?Sized>(&self, h: &mut H, names: usize) {
        self.fx(h, None, 0).get_names(names);
    }

    /// The screen's slot `what` (`EE_SCREENS_SESC_*`) with the trigger tab's selection `sel` (`mSelection`); `me`
    /// is the types' handle (their events carry it).
    pub fn call<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(
        &self,
        s: &mut S,
        h: &mut H,
        me: usize,
        what: u32,
        sel: i32,
    ) {
        if let Some((op, k)) = list_op(what, sel.wrapping_add(1)) {
            self.fx(h, None, me).list(op, k);
            if what == EE_SCREENS_SESC_DELETE {
                self.type_call(h, me, EE_SCREENS_SEET_HIDE, None);
            }
            return;
        }
        match what {
            EE_SCREENS_SESC_ACTIVATE => {
                {
                    let mut c = self.fx(h, None, me);
                    let n = c.player_count();
                    for p in 0..3 {
                        c.set_player_count(p, n);
                    }
                }
                self.type_call(h, me, EE_SCREENS_SEET_SHOW, None);
                let mut c = self.fx(h, None, me);
                c.tscreen(EE_SCREENS_SEET_TS_HIDE_TEXT);
                c.tscreen(EE_SCREENS_SEET_TS_SHOW_ALPHA_SORT);
            }
            EE_SCREENS_SESC_STORE => {
                // the original reads a missing effect: nothing here
                let Some(e) = self.entry(h, sel) else { return };
                let mut c = self.fx(h, Some(e), me);
                store_texts(&mut c);
                self.types.call(&mut c, self.ty.get(), EE_SCREENS_SEET_STORE);
                c.e().mType = self.ty.get();
            }
            EE_SCREENS_SESC_HIDE => {
                display(s, self.type_combo, false);
                display(s, self.title, false);
            }
            EE_SCREENS_SESC_LOAD => self.load(s, h, me, sel),
            EE_SCREENS_SESC_SHOW => {
                display(s, self.type_combo, true);
                display(s, self.title, true);
            }
            EE_SCREENS_SESC_REFRESH => {
                if let Some(e) = self.entry(h, sel) {
                    self.type_call(h, me, EE_SCREENS_SEET_UPDATE_NAME, Some(e));
                }
            }
            EE_SCREENS_SESC_CHANGE_TYPE => {
                let e = self.entry(h, sel);
                let picked = self.fx(h, None, me).handle_selection(self.type_combo).unwrap_or(0);
                let ty = picked.wrapping_add(1);
                if ty == self.ty.get() {
                    return;
                }
                self.type_call(h, me, EE_SCREENS_SEET_HIDE, None);
                self.ty.set(ty);
                self.type_call(h, me, EE_SCREENS_SEET_SHOW, None);
                // the original loads a missing effect: not here
                if e.is_some() {
                    self.type_call(h, me, EE_SCREENS_SEET_LOAD, e);
                }
            }
            EE_SCREENS_SESC_CHANGE_ACTION => {
                // the original passes a missing effect on: not here
                let e = self.entry(h, sel);
                if e.is_some() {
                    self.type_call(h, me, EE_SCREENS_SEET_CHANGE_ACTION, e);
                }
            }
            _ => {}
        }
    }

    /// `Load`: the selected effect's name and text, its type shown (the no-effect type twice) and loaded.
    fn load<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(&self, s: &mut S, h: &mut H, me: usize, sel: i32) {
        let Some(e) = self.entry(h, sel) else { return };
        let entry_ty = {
            let mut c = self.fx(h, Some(e), me);
            show_texts(&mut c);
            c.e().mType
        };
        self.type_call(h, me, EE_SCREENS_SEET_HIDE, None);
        self.ty.set(entry_ty);
        if entry_ty != 0 {
            set_selection(s, self.type_combo, entry_ty.wrapping_sub(1));
            // the original compares an unset selection when there is none: not shown then
            let picked = self.fx(h, None, me).handle_selection(self.type_combo);
            if picked.map(|p| p.wrapping_add(1)) == Some(self.ty.get()) {
                self.type_call(h, me, EE_SCREENS_SEET_SHOW, None);
            }
        } else {
            self.type_call(h, me, EE_SCREENS_SEET_SHOW, None);
        }
        if self.ty.get() == 0 {
            self.type_call(h, me, EE_SCREENS_SEET_SHOW, None);
        }
        self.type_call(h, me, EE_SCREENS_SEET_LOAD, Some(e));
    }
}

#[cfg(test)]
mod tests {
    use ee_sim::scenario::fields::EeScenarioEffectFields;

    use super::*;

    /// A host logging each op, answering an entry of type `ty` and a combo box selection.
    struct Log {
        ops: Vec<(u32, [usize; 3], [i32; 4])>,
        fields: EeScenarioEffectFields,
        selection: i32,
    }

    impl SeplHost for Log {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
            self.ops.push((op, p, *i));
            match op {
                EE_SCREENS_SEET_OP_ENTRY => {
                    let out = p[2] as *mut usize;
                    // SAFETY: the out words are two usizes
                    unsafe {
                        *out = 0x1000;
                        *out.add(1) = &mut self.fields as *mut EeScenarioEffectFields as usize;
                    }
                    0
                }
                EE_SCREENS_SEET_OP_HANDLE_SELECTION => self.selection,
                EE_SCREENS_SEET_OP_RECT => {
                    *f = [0.0; 4];
                    0
                }
                _ => 0,
            }
        }
    }

    fn log(ty: u32, selection: i32) -> Log {
        Log { ops: Vec::new(), fields: EeScenarioEffectFields { mType: ty, ..Default::default() }, selection }
    }

    #[test]
    fn list_slots_pass_the_entry_after_the_selection() {
        assert_eq!(list_op(EE_SCREENS_SESC_MOVE_UP, 5), Some((EE_SCREENS_SEOB_SM_UP, 5)));
        assert_eq!(list_op(EE_SCREENS_SESC_SORT, 5), Some((EE_SCREENS_SEOB_SM_SORT, 0)));
        assert_eq!(list_op(EE_SCREENS_SESC_LOAD, 5), None);
        let screen = EffectScreen { ty: Cell::new(3), ..Default::default() };
        let (mut s, mut h) = (log(0, 0), log(0, 0));
        screen.call(&mut s, &mut h, 0, EE_SCREENS_SESC_DELETE, 2);
        assert_eq!(h.ops[0].0, EE_SCREENS_SEET_OP_LIST);
        assert_eq!((h.ops[0].2[2], h.ops[0].2[3]), (EE_SCREENS_SEOB_SM_DELETE, 3));
        // then the trigger type's hide: five controls
        assert_eq!(h.ops.len(), 6);
    }

    #[test]
    fn effect_store_writes_the_type_and_the_texts() {
        let screen = EffectScreen { ty: Cell::new(0), ..Default::default() };
        let (mut s, mut h) = (log(0, 0), log(4, 0));
        screen.call(&mut s, &mut h, 0, EE_SCREENS_SESC_STORE, 0);
        assert_eq!(h.fields.mType, 0);
        let ops: Vec<u32> = h.ops.iter().map(|o| o.0).collect();
        assert_eq!(
            ops,
            [
                EE_SCREENS_SEET_OP_ENTRY,
                EE_SCREENS_SEET_OP_NAME_BOX,
                EE_SCREENS_SEET_OP_SET_EFFECT_TEXT,
                EE_SCREENS_SEET_OP_TEXT_BOX,
                EE_SCREENS_SEET_OP_SET_EFFECT_TEXT
            ]
        );
    }

    #[test]
    fn change_type_does_nothing_for_the_shown_type() {
        let screen = EffectScreen { ty: Cell::new(3), ..Default::default() };
        let (mut s, mut h) = (log(0, 0), log(0, 2));
        screen.call(&mut s, &mut h, 0, EE_SCREENS_SESC_CHANGE_TYPE, 0);
        assert_eq!(h.ops.len(), 2);
        assert_eq!(screen.ty.get(), 3);
        // no selection reads as the first type
        let mut h = log(0, -1);
        screen.call(&mut s, &mut h, 0, EE_SCREENS_SESC_CHANGE_TYPE, 0);
        assert_eq!(screen.ty.get(), 1);
    }

    #[test]
    fn effect_load_shows_the_no_effect_type_twice() {
        let screen = EffectScreen { ty: Cell::new(3), ..Default::default() };
        let (mut s, mut h) = (log(0, 0), log(0, 0));
        screen.call(&mut s, &mut h, 0, EE_SCREENS_SESC_LOAD, 0);
        assert_eq!(screen.ty.get(), 0);
        // the no-effect type's show hides six form controls, twice
        let hidden = h.ops.iter().filter(|o| o.0 == EE_SCREENS_SEET_OP_FORM_DISPLAY && o.2[3] == 0).count();
        assert_eq!(hidden, 12);
        assert!(s.ops.is_empty());
    }

    #[test]
    fn condition_activate_switches_object_to_player_attribute() {
        let screen = ConditionScreen { ty: Cell::new(C_OBJECT), ..Default::default() };
        let (mut s, mut h) = (log(0, 0), log(0, 0));
        screen.call(&mut s, &mut h, EE_SCREENS_SESC_ACTIVATE, 0);
        assert_eq!(screen.ty.get(), C_PLAYER_ATTRIBUTE);
        let counts = h.ops.iter().filter(|o| o.0 == EE_SCREENS_SEET_OP_SET_PLAYER_COUNT).count();
        assert_eq!(counts, 2);
    }
}
