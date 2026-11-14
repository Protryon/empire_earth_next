//! The scenario editor's trigger tab trigger screen whole in Rust (`EESETSTrigger`, once `EESETSTrigger.cpp`): the
//! controls it makes (records for the shared maker, `EeScreensSeplCtl`), the two condition and two effect rows it
//! shows of the selected trigger (their not, delay, combo boxes, and/or operators and remove buttons), the rows'
//! steps, the store of the controls into the trigger and their load from it, the trigger list's operations. The
//! controls stay C++ objects; the screen drives them through the shared control ops (`ee_screenscpp_sepl`, below
//! 100) and its own (`ee_screenscpp_setr`, from 200: the scenario's triggers).
//!
//! A host call can come back into the screen (an operator combo's selection changes the operator): it is only used
//! through `&self`; the rows' first indexes are `Cell`s.

use core::cell::Cell;

use crate::editor_object::*;
use crate::editor_players::*;
use crate::scenario_editor::{
    EE_SCREENS_SE_TRIGGER_CHANGE_INDEX, EE_SCREENS_SE_TRIGGER_CONDITION_TIMER, EE_SCREENS_SE_TRIGGER_REMOVE,
};

// ---- the trigger screen's host ops (`ee_screenscpp_setr`)

/// The triggers' list: i0 the operation (`EE_SCREENS_SEOB_SM_*`) on the trigger i1.
pub const EE_SCREENS_SETR_OP_SM: u32 = 200;
/// The scenario's condition (i0 = 0) or effect (1) names appended to the combo boxes p0 and p1 / set into the
/// vector p0 (the triggers' names, `GetNames`).
pub const EE_SCREENS_SETR_OP_APPEND_NAMES: u32 = 201;
pub const EE_SCREENS_SETR_OP_GET_NAMES: u32 = 202;
/// The trigger screen's description and alphabetical sort hidden / its description shown.
pub const EE_SCREENS_SETR_OP_HIDE_TEXT: u32 = 203;
pub const EE_SCREENS_SETR_OP_SHOW_TEXT: u32 = 204;
/// -> whether the trigger i0 exists.
pub const EE_SCREENS_SETR_OP_EXISTS: u32 = 205;
/// The trigger i0's value i1 (`EE_SCREENS_SETR_V_*`) -> / set to i2.
pub const EE_SCREENS_SETR_OP_GET: u32 = 206;
pub const EE_SCREENS_SETR_OP_SET: u32 = 207;
/// The trigger i0 named and described from the trigger screen's boxes / the boxes set from it.
pub const EE_SCREENS_SETR_OP_TEXTS_FROM_SCREEN: u32 = 208;
pub const EE_SCREENS_SETR_OP_TEXTS_TO_SCREEN: u32 = 209;
/// The trigger i0's condition (i1 = 0) or effect (1) i2: -> its field i3 (`EE_SCREENS_SETR_W_*`; -1 when there is
/// no such entry).
pub const EE_SCREENS_SETR_OP_ENTRY: u32 = 210;
/// The trigger i0's condition i1 set: not i2, the condition i3 (`SetCondition`) / its effect i1: delay f-less i2,
/// the effect i3 (`SetEffect`).
pub const EE_SCREENS_SETR_OP_SET_CONDITION: u32 = 211;
pub const EE_SCREENS_SETR_OP_SET_EFFECT: u32 = 212;
/// The trigger i0's condition (i1 = 0) or effect (1) i2's operator set to i3 (`SetConditionOperator`,
/// `SetEffectOperator`) / deleted.
pub const EE_SCREENS_SETR_OP_SET_OPERATOR: u32 = 213;
pub const EE_SCREENS_SETR_OP_DELETE: u32 = 214;
/// The operator combo boxes' event made (its handle into p0) / released.
pub const EE_SCREENS_SETR_OP_NEW_EVENT: u32 = 215;
pub const EE_SCREENS_SETR_OP_RELEASE_EVENT: u32 = 216;
/// The combo box p0's selection-changed event set to p1 (none: NULL).
pub const EE_SCREENS_SETR_OP_SET_EVENT: u32 = 217;
/// The remove button p0's text: "Del" and the label p1's first line, in `gU531934`.
pub const EE_SCREENS_SETR_OP_REMOVE_TEXT: u32 = 218;
/// The operator combo box p0's text label set to "End".
pub const EE_SCREENS_SETR_OP_END_TEXT: u32 = 219;

/// The trigger's values: on, the condition duration, looping, in cinematics; its condition and effect counts.
pub const EE_SCREENS_SETR_V_ON: i32 = 0;
pub const EE_SCREENS_SETR_V_DURATION: i32 = 1;
pub const EE_SCREENS_SETR_V_LOOPING: i32 = 2;
pub const EE_SCREENS_SETR_V_CINEMATIC: i32 = 3;
pub const EE_SCREENS_SETR_V_CONDITIONS: i32 = 4;
pub const EE_SCREENS_SETR_V_EFFECTS: i32 = 5;

/// An entry's fields: a condition's not, condition and operator; an effect's delay, effect and operator.
pub const EE_SCREENS_SETR_W_FIRST: i32 = 0;
pub const EE_SCREENS_SETR_W_VALUE: i32 = 1;
pub const EE_SCREENS_SETR_W_OPERATOR: i32 = 2;

/// `kSMTriggerOperatorEnd`, `And`, `Or`.
pub const EE_SCREENS_SETR_END: i32 = 0;
pub const EE_SCREENS_SETR_AND: i32 = 1;
pub const EE_SCREENS_SETR_OR: i32 = 2;

// ---- the language's strings (`ee/EEStringIDs.h`; the host `static_assert`s them)

pub const EE_SCREENS_SETR_IDS_SE_REMOVE: u32 = 20539;
pub const EE_SCREENS_SETR_IDS_CONDITION: u32 = 30300;
pub const EE_SCREENS_SETR_IDS_EFFECT: u32 = 30301;
pub const EE_SCREENS_SETR_IDS_SECONDS: u32 = 30379;
/// If, Then, not, Delay, And, Or, Trigger Statement, Trigger Is On, Conditions True For, Looping: 30386..30395.
pub const EE_SCREENS_SETR_IDS_IF: u32 = 30386;
pub const EE_SCREENS_SETR_IDS_INCLUDE_IN_CINEMATIC: u32 = 31701;

const IDS_THEN: u32 = EE_SCREENS_SETR_IDS_IF + 1;
const IDS_NOT: u32 = EE_SCREENS_SETR_IDS_IF + 2;
const IDS_DELAY: u32 = EE_SCREENS_SETR_IDS_IF + 3;
const IDS_AND: u32 = EE_SCREENS_SETR_IDS_IF + 4;
const IDS_OR: u32 = EE_SCREENS_SETR_IDS_IF + 5;
const IDS_TRIGGER_STATEMENT: u32 = EE_SCREENS_SETR_IDS_IF + 6;
const IDS_TRIGGER_IS_ON: u32 = EE_SCREENS_SETR_IDS_IF + 7;
const IDS_CONDITIONS_TRUE_FOR: u32 = EE_SCREENS_SETR_IDS_IF + 8;
const IDS_LOOPING: u32 = EE_SCREENS_SETR_IDS_IF + 9;

/// The control ids the events tell apart: the operators, the remove buttons.
pub const EE_SCREENS_SETR_FIRST_CONDITION_OPERATOR: u32 = 0x9d0;
pub const EE_SCREENS_SETR_SECOND_CONDITION_OPERATOR: u32 = 0x9d1;
pub const EE_SCREENS_SETR_FIRST_EFFECT_OPERATOR: u32 = 0x9d6;
pub const EE_SCREENS_SETR_SECOND_EFFECT_OPERATOR: u32 = 0x9d7;
pub const EE_SCREENS_SETR_REMOVE_FIRST_CONDITION: u32 = 0x9d8;
pub const EE_SCREENS_SETR_REMOVE_SECOND_CONDITION: u32 = 0x9d9;
pub const EE_SCREENS_SETR_REMOVE_FIRST_EFFECT: u32 = 0x9da;
pub const EE_SCREENS_SETR_REMOVE_SECOND_EFFECT: u32 = 0x9db;

/// The trigger screen's control group.
const GROUP: u32 = 9;
/// The scenario's screens the trigger combo boxes open (`kSETriggerScreenCondition`, `kSETriggerScreenEffect`).
const SCREEN_CONDITION: i32 = 1;
const SCREEN_EFFECT: i32 = 2;

/// One condition or effect row: its not box (conditions) or delay box (effects), entry combo, operator combo,
/// remove button, its "not" (conditions) or "Delay" (effects) header and its "Condition n" or "Effect n" label.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct TriggerRow {
    pub value: usize,
    pub combo: usize,
    pub operator: usize,
    pub remove: usize,
    pub header: usize,
    pub label: usize,
}

/// The trigger screen (`EESETSTrigger`).
#[derive(Debug, Default)]
pub struct Trigger {
    pub if_label: usize,
    pub then_label: usize,
    pub statement_label: usize,
    pub seconds_label: usize,
    /// the first and second condition rows, the first and second effect rows
    pub conditions: [TriggerRow; 2],
    pub effects: [TriggerRow; 2],
    pub prev_conditions: usize,
    pub next_conditions: usize,
    pub prev_effects: usize,
    pub next_effects: usize,
    pub is_on: usize,
    pub timer: usize,
    pub duration: usize,
    pub looping: usize,
    pub cinematic: usize,
    /// the operator combo boxes' event
    pub operator_event: usize,
    /// the first condition and effect the rows show
    pub first_condition: Cell<i32>,
    pub first_effect: Cell<i32>,
}

/// A yellow label of the trigger screen, 20 pixels high.
fn label(id: u32, string: u32, x: f32, y: f32, w: f32, center: bool) -> EeScreensSeplCtl {
    let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, x, y, w, 20.0)
        .string(string)
        .flags(EE_SCREENS_SEPL_FG)
        .bg(YELLOW_BG)
        .group(GROUP);
    if center {
        c.align(EE_SCREENS_SEPL_ALIGN_CENTER)
    } else {
        c
    }
}

fn trigger_get<H: SeplHost + ?Sized>(h: &mut H, t: u32, what: i32) -> i32 {
    call(h, EE_SCREENS_SETR_OP_GET, [0; 3], [t as i32, what, 0, 0])
}

fn trigger_set<H: SeplHost + ?Sized>(h: &mut H, t: u32, what: i32, to: i32) {
    call(h, EE_SCREENS_SETR_OP_SET, [0; 3], [t as i32, what, to, 0]);
}

/// An entry's field (`EE_SCREENS_SETR_W_*`) of the trigger `t`'s condition (`effect` false) or effect `k`; `None`
/// when there is no such entry.
fn entry<H: SeplHost + ?Sized>(h: &mut H, t: u32, effect: bool, k: i32, what: i32) -> Option<i32> {
    let exists = call(h, EE_SCREENS_SETR_OP_ENTRY, [0; 3], [t as i32, effect as i32, k, -1]) != 0;
    exists.then(|| call(h, EE_SCREENS_SETR_OP_ENTRY, [0; 3], [t as i32, effect as i32, k, what]))
}

fn set_event<H: SeplHost + ?Sized>(h: &mut H, combo: usize, event: usize) {
    call(h, EE_SCREENS_SETR_OP_SET_EVENT, [combo, event, 0], [0; 4]);
}

impl Trigger {
    /// `EESETSTrigger`'s constructor on the form: the controls (group 9) from the control index `*index`
    /// (advanced), in the original's order; the operator event; the rows at the first condition and effect.
    pub fn new<H: SeplHost + ?Sized>(h: &mut H, form: usize, index: &mut u32) -> Trigger {
        let mut t = Trigger::default();
        let mut m = Maker { h, form, index: *index };
        t.if_label = m.make(label(0x9c4, EE_SCREENS_SETR_IDS_IF, 3.0, 664.0, 40.0, false));
        t.then_label = m.make(label(0x9c5, IDS_THEN, 3.0, 711.0, 40.0, false));
        t.statement_label = m.make(label(0x9c6, IDS_TRIGGER_STATEMENT, 0.0, 616.0, 150.0, false));
        t.effects[0].header = m.make(label(0x9c7, IDS_DELAY, 45.0, 689.0, 45.0, false));
        t.effects[1].header = m.make(label(0x9c8, IDS_DELAY, 353.0, 689.0, 45.0, false));
        t.conditions[0].header = m.make(label(0x9c9, IDS_NOT, 45.0, 641.0, 45.0, true));
        t.conditions[1].header = m.make(label(0x9ca, IDS_NOT, 353.0, 641.0, 45.0, true));
        t.seconds_label = m.make(label(0x9cb, EE_SCREENS_SETR_IDS_SECONDS, 411.0, 557.0, 100.0, false));
        // the four row labels: made with the last text ("seconds"), set by Load
        let s = EE_SCREENS_SETR_IDS_SECONDS;
        t.conditions[0].label = m.make(label(0x9e4, s, 103.0, 641.0, 200.0, true));
        t.conditions[1].label = m.make(label(0x9e5, s, 411.0, 641.0, 200.0, true));
        t.effects[0].label = m.make(label(0x9e6, s, 103.0, 689.0, 200.0, true));
        t.effects[1].label = m.make(label(0x9e7, s, 411.0, 689.0, 200.0, true));
        for (k, x) in [56.0, 364.0].into_iter().enumerate() {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, 0x9cc + k as u32, x, 664.0, 22.0, 20.0).group(GROUP);
            t.conditions[k].value = m.make(c);
        }
        let entry_combo = |id, x, y, screen| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LIST_COMBO, id, x, y, 200.0, 20.0)
                .font(EE_SCREENS_SEPL_FONT_530)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(1.0)
                .bg2(YELLOW_BG)
                .group(GROUP)
                .text(EE_SCREENS_SEPL_TEXT_NONE, 0, screen)
                .extra([EE_SCREENS_SEPL_COMBO_TRIGGER, EE_SCREENS_SEPL_COMBO_LINES_NONE, 2, 1])
        };
        let operator_combo = |id, x, y| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_LIST_COMBO, id, x, y, 55.0, 20.0)
                .font(EE_SCREENS_SEPL_FONT_530)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(1.0)
                .bg2(YELLOW_BG)
                .group(GROUP)
                .extra([EE_SCREENS_SEPL_COMBO_PLAIN, EE_SCREENS_SEPL_COMBO_LINES_NONE, 2, 1])
        };
        t.conditions[0].combo = m.make(entry_combo(0x9ce, 92.0, 664.0, SCREEN_CONDITION));
        t.conditions[1].combo = m.make(entry_combo(0x9cf, 400.0, 664.0, SCREEN_CONDITION));
        t.conditions[0].operator = make_operator(&mut m, operator_combo(0x9d0, 294.0, 664.0));
        t.conditions[1].operator = make_operator(&mut m, operator_combo(0x9d1, 602.0, 664.0));
        for (k, x) in [45.0, 353.0].into_iter().enumerate() {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, 0x9d2 + k as u32, x, 711.0, 45.0, 20.0)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([4, 1, 0, 0]);
            t.effects[k].value = m.make(c);
        }
        t.effects[0].combo = m.make(entry_combo(0x9d4, 92.0, 711.0, SCREEN_EFFECT));
        t.effects[1].combo = m.make(entry_combo(0x9d5, 400.0, 711.0, SCREEN_EFFECT));
        t.effects[0].operator = make_operator(&mut m, operator_combo(0x9d6, 294.0, 711.0));
        t.effects[1].operator = make_operator(&mut m, operator_combo(0x9d7, 602.0, 711.0));
        // the remove buttons, each right of the one before ("Remove", the last "--------------")
        let mut x = 180.0f32;
        for k in 0..4u32 {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_BUTTON, 0x9d8 + k, x, 616.0, 115.0, 20.0)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(GROUP)
                .extra([2, 0, 0, 0]);
            let c = if k < 3 {
                c.string(EE_SCREENS_SETR_IDS_SE_REMOVE)
            } else {
                c.text(EE_SCREENS_SEPL_TEXT_CHAR, 0, b'-' as i32 | 14 << 8)
            };
            let b = m.make(c);
            screen_event(m.h, b, EE_SCREENS_SE_TRIGGER_REMOVE, EE_SCREENS_SEPL_ON_BUTTON_UP);
            match k {
                0 => t.conditions[0].remove = b,
                1 => t.conditions[1].remove = b,
                2 => t.effects[0].remove = b,
                _ => t.effects[1].remove = b,
            }
            x = rect(m.h, b)[2] + 2.0;
        }
        // the rows' steps: arrows left of the rows and at the second condition operator's right end
        let right = rect(m.h, t.conditions[1].operator)[2] - 20.0;
        for (k, (x, y, r)) in
            [(3.0, 641.0, 0), (right, 641.0, 1), (3.0, 689.0, 0), (right, 689.0, 1)].into_iter().enumerate()
        {
            let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_ARROW, 0x9dc + k as u32, x, y, 20.0, 20.0)
                .group(GROUP)
                .extra([r, 0, 0, 0]);
            let b = m.make(c);
            screen_event(m.h, b, EE_SCREENS_SE_TRIGGER_CHANGE_INDEX, EE_SCREENS_SEPL_ON_BUTTON_UP);
            match k {
                0 => t.prev_conditions = b,
                1 => t.next_conditions = b,
                2 => t.prev_effects = b,
                _ => t.next_effects = b,
            }
        }
        let check = |id, string, x, y| {
            EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, id, x, y, 250.0, 20.0)
                .string(string)
                .flags(EE_SCREENS_SEPL_FG)
                .bg(YELLOW_BG)
                .group(GROUP)
        };
        t.is_on = m.make(check(0x9e0, IDS_TRIGGER_IS_ON, 329.0, 510.0));
        t.timer = m.make(check(0x9e1, IDS_CONDITIONS_TRUE_FOR, 329.0, 535.0));
        const BOTH: i32 = EE_SCREENS_SEPL_ON_CHECKED | EE_SCREENS_SEPL_ON_UNCHECKED;
        screen_event(m.h, t.timer, EE_SCREENS_SE_TRIGGER_CONDITION_TIMER, BOTH);
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, 0x9e2, 349.0, 557.0, 60.0, 20.0)
            .flags(EE_SCREENS_SEPL_FG)
            .bg(YELLOW_BG)
            .group(GROUP)
            .extra([5, 1, 0, 0]);
        t.duration = m.make(c);
        t.looping = m.make(check(0x9e3, IDS_LOOPING, 329.0, 586.0));
        let x = rect(m.h, t.looping)[2] + 5.0;
        t.cinematic = m.make(check(0x9e8, EE_SCREENS_SETR_IDS_INCLUDE_IN_CINEMATIC, x, 586.0));
        *index = m.index;
        let mut p = [0usize; 3];
        let (mut i, mut f) = ([0i32; 4], [0.0f32; 4]);
        p[0] = &mut t.operator_event as *mut usize as usize;
        m.h.op(EE_SCREENS_SETR_OP_NEW_EVENT, p, &mut i, &mut f);
        t
    }

    /// The destructor: every control deleted in the original's order, the operator event released.
    pub fn destroy<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let (c, e) = (&self.conditions, &self.effects);
        for x in [
            self.if_label,
            self.then_label,
            self.statement_label,
            e[0].header,
            e[1].header,
            c[0].header,
            c[1].header,
            self.seconds_label,
            c[0].value,
            c[1].value,
            c[0].combo,
            c[1].combo,
            c[0].operator,
            c[1].operator,
            e[0].value,
            e[1].value,
            e[0].combo,
            e[1].combo,
            e[0].operator,
            e[1].operator,
            c[0].remove,
            c[1].remove,
            e[0].remove,
            e[1].remove,
            self.prev_conditions,
            self.next_conditions,
            self.prev_effects,
            self.next_effects,
            c[0].label,
            c[1].label,
            e[0].label,
            e[1].label,
            self.duration,
            self.looping,
            self.is_on,
            self.timer,
            self.cinematic,
        ] {
            delete(h, x);
        }
        call(h, EE_SCREENS_SETR_OP_RELEASE_EVENT, [self.operator_event, 0, 0], [0; 4]);
    }

    /// The triggers' list operations (`Sort`, `MoveDown`, `MoveUp`, `Delete`, `Duplicate`, `New`) on the trigger
    /// `selection`; deleting, duplicating and adding show the rows from the start.
    pub fn list_op<H: SeplHost + ?Sized>(&self, h: &mut H, op: i32, selection: u32) {
        if op == EE_SCREENS_SEOB_SM_DELETE || op == EE_SCREENS_SEOB_SM_DUPLICATE || op == EE_SCREENS_SEOB_SM_NEW {
            self.first_condition.set(0);
            self.first_effect.set(0);
        }
        call(h, EE_SCREENS_SETR_OP_SM, [0; 3], [op, selection as i32, 0, 0]);
    }

    /// `UpdateSecondConditionRow` / `UpdateSecondEffectRow`: the second row hidden for a single entry.
    pub fn update_second_row<H: SeplHost + ?Sized>(&self, h: &mut H, effect: bool, single: bool) {
        let r = if effect { &self.effects } else { &self.conditions };
        for c in [r[1].header, r[1].value, r[1].combo, r[1].operator, r[1].remove, r[1].label] {
            display(h, c, !single);
        }
    }

    /// `Activate`: the entry combo boxes filled with the conditions' and effects' names; the description and
    /// the alphabetical sort hidden.
    pub fn activate<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for r in [&self.conditions, &self.effects] {
            for row in r {
                call(h, EE_SCREENS_SEPL_OP_COMBO_CLEAR, [row.combo, 0, 0], [0; 4]);
            }
        }
        for (k, r) in [&self.conditions, &self.effects].into_iter().enumerate() {
            call(h, EE_SCREENS_SETR_OP_APPEND_NAMES, [r[0].combo, r[1].combo, 0], [k as i32, 0, 0, 0]);
        }
        call(h, EE_SCREENS_SETR_OP_HIDE_TEXT, [0; 3], [0; 4]);
    }

    /// `Store`: the controls written into the trigger `selection`: its name and description, on, the timer (0
    /// unless checked), looping, in cinematics, and the shown rows' entries; the operator events detached.
    pub fn store<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32) {
        let t = selection;
        if call(h, EE_SCREENS_SETR_OP_EXISTS, [0; 3], [t as i32, 0, 0, 0]) == 0 {
            return;
        }
        call(h, EE_SCREENS_SETR_OP_TEXTS_FROM_SCREEN, [0; 3], [t as i32, 0, 0, 0]);
        let v = checked(h, self.is_on) as i32;
        trigger_set(h, t, EE_SCREENS_SETR_V_ON, v);
        let v = if checked(h, self.timer) {
            call(h, EE_SCREENS_SEPL_OP_EDIT_INT, [self.duration, 0, 0], [0; 4])
        } else {
            0
        };
        trigger_set(h, t, EE_SCREENS_SETR_V_DURATION, v);
        let v = checked(h, self.looping) as i32;
        trigger_set(h, t, EE_SCREENS_SETR_V_LOOPING, v);
        let v = checked(h, self.cinematic) as i32;
        trigger_set(h, t, EE_SCREENS_SETR_V_CINEMATIC, v);
        for effect in [false, true] {
            let (rows, first) = if effect {
                (&self.effects, self.first_effect.get())
            } else {
                (&self.conditions, self.first_condition.get())
            };
            let count =
                trigger_get(h, t, if effect { EE_SCREENS_SETR_V_EFFECTS } else { EE_SCREENS_SETR_V_CONDITIONS }) as u32;
            self.store_row(h, t, effect, &rows[0], first);
            if (first as u32) < count.wrapping_sub(1) {
                self.store_row(h, t, effect, &rows[1], first + 1);
                set_event(h, rows[1].operator, 0);
            }
        }
        set_event(h, self.conditions[0].operator, 0);
        set_event(h, self.effects[0].operator, 0);
    }

    /// One row's entry stored when its combo box has a selection and the trigger has the entry.
    fn store_row<H: SeplHost + ?Sized>(&self, h: &mut H, t: u32, effect: bool, row: &TriggerRow, k: i32) {
        let s = selection(h, row.combo);
        if s == NONE || entry(h, t, effect, k, EE_SCREENS_SETR_W_VALUE).is_none() {
            return;
        }
        let (op, v) = if effect {
            (EE_SCREENS_SETR_OP_SET_EFFECT, text_as_int(h, row.value))
        } else {
            (EE_SCREENS_SETR_OP_SET_CONDITION, checked(h, row.value) as i32)
        };
        call(h, op, [0; 3], [t as i32, k, v, s as i32]);
    }

    /// `ChangeOperator`: the trigger stored, the changed row's operator set (the combo box's line + 1; a second
    /// row's change shows the rows one on), the trigger loaded.
    pub fn change_operator<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32, id: u32) {
        self.store(h, selection);
        let (effect, row, first) = match id {
            EE_SCREENS_SETR_FIRST_CONDITION_OPERATOR => (false, 0, &self.first_condition),
            EE_SCREENS_SETR_SECOND_CONDITION_OPERATOR => (false, 1, &self.first_condition),
            EE_SCREENS_SETR_FIRST_EFFECT_OPERATOR => (true, 0, &self.first_effect),
            EE_SCREENS_SETR_SECOND_EFFECT_OPERATOR => (true, 1, &self.first_effect),
            _ => {
                self.load(h, selection);
                return;
            }
        };
        let rows = if effect { &self.effects } else { &self.conditions };
        let s = selection_of_combo(h, rows[row].operator);
        let k = first.get() + row as i32;
        call(
            h,
            EE_SCREENS_SETR_OP_SET_OPERATOR,
            [0; 3],
            [selection as i32, effect as i32, k, s.wrapping_add(1) as i32],
        );
        if row == 1 {
            first.set(first.get() + 1);
        }
        self.load(h, selection);
    }

    /// The rows' steps (`EESEETriggerChangeIndex`: 0 conditions back, 1 on, 2 effects back, 3 on): stored, stepped,
    /// loaded.
    pub fn step<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32, step: u32) {
        self.store(h, selection);
        let first = if step < 2 { &self.first_condition } else { &self.first_effect };
        first.set(if step.is_multiple_of(2) { first.get() - 1 } else { first.get() + 1 });
        self.load(h, selection);
    }

    /// `Hide`: the flags and the timer.
    pub fn hide<H: SeplHost + ?Sized>(&self, h: &mut H) {
        for c in [self.timer, self.looping, self.duration, self.is_on, self.seconds_label, self.cinematic] {
            display(h, c, false);
        }
    }

    /// `Show`: the flags; the duration and its label while the timer is checked.
    pub fn show<H: SeplHost + ?Sized>(&self, h: &mut H) {
        let timer = checked(h, self.timer);
        display(h, self.duration, timer);
        display(h, self.seconds_label, timer);
        for c in [self.timer, self.looping, self.is_on, self.cinematic] {
            display(h, c, true);
        }
    }

    /// `Load`: the trigger `selection` into the controls: the operator events detached, its name, description,
    /// flags and timer, the rows (back at the start when past the end), the second rows shown for several
    /// entries, the steps' arrows, the labels "Condition n" / "Effect n" and the remove buttons' texts.
    pub fn load<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32) {
        for c in [
            self.conditions[0].operator,
            self.effects[0].operator,
            self.conditions[1].operator,
            self.effects[1].operator,
        ] {
            set_event(h, c, 0);
        }
        let t = selection;
        if call(h, EE_SCREENS_SETR_OP_EXISTS, [0; 3], [t as i32, 0, 0, 0]) == 0 {
            return;
        }
        call(h, EE_SCREENS_SETR_OP_SHOW_TEXT, [0; 3], [0; 4]);
        call(h, EE_SCREENS_SETR_OP_TEXTS_TO_SCREEN, [0; 3], [t as i32, 0, 0, 0]);
        let v = trigger_get(h, t, EE_SCREENS_SETR_V_ON) != 0;
        set_checked(h, self.is_on, v);
        let duration = trigger_get(h, t, EE_SCREENS_SETR_V_DURATION);
        edit_number(h, self.duration, duration);
        set_checked(h, self.timer, duration as u32 > 0);
        let v = trigger_get(h, t, EE_SCREENS_SETR_V_LOOPING) != 0;
        set_checked(h, self.looping, v);
        let v = trigger_get(h, t, EE_SCREENS_SETR_V_CINEMATIC) != 0;
        set_checked(h, self.cinematic, v);
        let conditions = trigger_get(h, t, EE_SCREENS_SETR_V_CONDITIONS) as u32;
        let effects = trigger_get(h, t, EE_SCREENS_SETR_V_EFFECTS) as u32;
        if self.first_condition.get() as u32 >= conditions.wrapping_sub(1) {
            self.first_condition.set(0);
        }
        if self.first_effect.get() as u32 >= effects.wrapping_sub(1) {
            self.first_effect.set(0);
        }
        let (fc, fe) = (self.first_condition.get(), self.first_effect.get());
        let single = conditions.wrapping_sub(1) == 0;
        self.update_second_row(h, false, single);
        if !single {
            self.load_row(h, t, false, &self.conditions[1], fc + 1);
        }
        let single = effects.wrapping_sub(1) == 0;
        self.update_second_row(h, true, single);
        if !single {
            self.load_row(h, t, true, &self.effects[1], fe + 1);
        }
        display(h, self.prev_conditions, fc != 0);
        display(h, self.prev_effects, fe != 0);
        display(h, self.next_conditions, fc < conditions.wrapping_sub(2) as i32);
        display(h, self.next_effects, fe < effects.wrapping_sub(2) as i32);
        self.load_row(h, t, false, &self.conditions[0], fc);
        self.load_row(h, t, true, &self.effects[0], fe);
        for (rows, string, first) in
            [(&self.conditions, EE_SCREENS_SETR_IDS_CONDITION, fc), (&self.effects, EE_SCREENS_SETR_IDS_EFFECT, fe)]
        {
            for (k, row) in rows.iter().enumerate() {
                let p = [row.label, 0, 0];
                call(
                    h,
                    EE_SCREENS_SEPL_OP_LABEL_TEXT,
                    p,
                    [EE_SCREENS_SEPL_TEXT_STRING_NUMBER as i32, string as i32, first + k as i32, 0],
                );
            }
        }
        for (rows, first, count) in [(&self.conditions, fc, conditions), (&self.effects, fe, effects)] {
            if first == 0 && count == 1 {
                display(h, rows[0].remove, false);
            } else {
                call(h, EE_SCREENS_SETR_OP_REMOVE_TEXT, [rows[0].remove, rows[0].label, 0], [0; 4]);
                display(h, rows[0].remove, true);
            }
        }
        for rows in [&self.conditions, &self.effects] {
            call(h, EE_SCREENS_SETR_OP_REMOVE_TEXT, [rows[1].remove, rows[1].label, 0], [0; 4]);
        }
    }

    /// One row from the trigger's condition or effect `k`: its not or delay, its entry, its operator.
    fn load_row<H: SeplHost + ?Sized>(&self, h: &mut H, t: u32, effect: bool, row: &TriggerRow, k: i32) {
        // (an entry past the trigger's: the original read through a null wrapper; the row is left as it is)
        let Some(first) = entry(h, t, effect, k, EE_SCREENS_SETR_W_FIRST) else {
            return;
        };
        if effect {
            edit_number(h, row.value, first);
        } else {
            set_checked(h, row.value, first != 0);
        }
        let v = entry(h, t, effect, k, EE_SCREENS_SETR_W_VALUE).unwrap_or(0);
        set_selection(h, row.combo, v as u32);
        let op = entry(h, t, effect, k, EE_SCREENS_SETR_W_OPERATOR).unwrap_or(0);
        self.show_operator(h, row.operator, op);
    }

    /// `ShowOperator`: "End" (no line), "And" or "Or" in the operator combo box; its event attached.
    pub fn show_operator<H: SeplHost + ?Sized>(&self, h: &mut H, combo: usize, op: i32) {
        match op {
            EE_SCREENS_SETR_END => {
                call(h, EE_SCREENS_SETR_OP_END_TEXT, [combo, 0, 0], [0; 4]);
                set_selection(h, combo, u32::MAX);
            }
            EE_SCREENS_SETR_AND => set_selection(h, combo, 0),
            EE_SCREENS_SETR_OR => set_selection(h, combo, 1),
            _ => {}
        }
        set_event(h, combo, self.operator_event);
    }

    /// `RemoveEntry`: the trigger stored, the removed button's condition or effect deleted, the rows one back
    /// when the pair ran past the end, the trigger loaded.
    pub fn remove_entry<H: SeplHost + ?Sized>(&self, h: &mut H, selection: u32, id: u32) {
        self.store(h, selection);
        let (effect, row) = match id {
            EE_SCREENS_SETR_REMOVE_FIRST_CONDITION => (false, 0),
            EE_SCREENS_SETR_REMOVE_SECOND_CONDITION => (false, 1),
            EE_SCREENS_SETR_REMOVE_FIRST_EFFECT => (true, 0),
            EE_SCREENS_SETR_REMOVE_SECOND_EFFECT => (true, 1),
            _ => {
                self.load(h, selection);
                return;
            }
        };
        let first = if effect { &self.first_effect } else { &self.first_condition };
        call(h, EE_SCREENS_SETR_OP_DELETE, [0; 3], [selection as i32, effect as i32, first.get() + row, 0]);
        let count =
            trigger_get(h, selection, if effect { EE_SCREENS_SETR_V_EFFECTS } else { EE_SCREENS_SETR_V_CONDITIONS });
        let index = first.get() as u32;
        if index.wrapping_add(1) == count as u32 && index > 0 {
            first.set(index as i32 - 1);
        }
        self.load(h, selection);
    }

    /// `GetNames`: the triggers' names into the C++ vector `out`.
    pub fn get_names<H: SeplHost + ?Sized>(&self, h: &mut H, out: usize) {
        call(h, EE_SCREENS_SETR_OP_GET_NAMES, [out, 0, 0], [0; 4]);
    }
}

fn selection_of_combo<H: SeplHost + ?Sized>(h: &mut H, combo: usize) -> u32 {
    selection(h, combo)
}

/// An operator combo box: made empty, then "And" and "Or" appended.
fn make_operator<H: SeplHost + ?Sized>(m: &mut Maker<'_, H>, c: EeScreensSeplCtl) -> usize {
    let b = m.make(c);
    for s in [IDS_AND, IDS_OR] {
        call(m.h, EE_SCREENS_SEPL_OP_COMBO_APPEND, [b, 0, 0], [EE_SCREENS_SEPL_TEXT_STRING as i32, s as i32, 0, 0]);
    }
    b
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashMap;

    /// A recording host: controls are handles from 1 with their records; the trigger 0 has the conditions and
    /// effects in `entries` (not or delay, value, operator); display modes, checks, selections, events and edit
    /// numbers are kept; every other op is logged.
    #[derive(Default)]
    struct Rec {
        made: Vec<EeScreensSeplCtl>,
        mode: HashMap<usize, i32>,
        on: HashMap<usize, bool>,
        sel: HashMap<usize, u32>,
        number: HashMap<usize, i32>,
        event: HashMap<usize, usize>,
        entries: [Vec<[i32; 3]>; 2],
        values: HashMap<i32, i32>,
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
                EE_SCREENS_SETR_OP_NEW_EVENT => {
                    unsafe { *(p[0] as *mut usize) = 5000 };
                    0
                }
                EE_SCREENS_SEPL_OP_RECT => {
                    let c = self.made[p[0] - 1];
                    *f = [c.x, c.y, c.x + c.w - 1.0, c.y + c.h - 1.0];
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
                EE_SCREENS_SEPL_OP_CHECKED => *self.on.get(&p[0]).unwrap_or(&false) as i32,
                EE_SCREENS_SEPL_OP_SET_SELECTION => {
                    self.sel.insert(p[0], i[0] as u32);
                    0
                }
                EE_SCREENS_SEPL_OP_COMBO_SELECTION => *self.sel.get(&p[0]).unwrap_or(&u32::MAX) as i32,
                EE_SCREENS_SEPL_OP_EDIT_NUMBER => {
                    self.number.insert(p[0], i[0]);
                    0
                }
                EE_SCREENS_SEPL_OP_EDIT_INT => *self.number.get(&p[0]).unwrap_or(&0),
                EE_SCREENS_SETR_OP_SET_EVENT => {
                    self.event.insert(p[0], p[1]);
                    0
                }
                EE_SCREENS_SETR_OP_EXISTS => (i[0] == 0) as i32,
                EE_SCREENS_SETR_OP_GET => match i[1] {
                    EE_SCREENS_SETR_V_CONDITIONS => self.entries[0].len() as i32,
                    EE_SCREENS_SETR_V_EFFECTS => self.entries[1].len() as i32,
                    v => *self.values.get(&v).unwrap_or(&0),
                },
                EE_SCREENS_SETR_OP_SET => {
                    self.values.insert(i[1], i[2]);
                    0
                }
                EE_SCREENS_SETR_OP_ENTRY => match self.entries[i[1] as usize].get(i[2] as usize) {
                    None => 0,
                    Some(_) if i[3] < 0 => 1,
                    Some(e) => e[i[3] as usize],
                },
                EE_SCREENS_SETR_OP_SET_CONDITION | EE_SCREENS_SETR_OP_SET_EFFECT => {
                    let k = (op == EE_SCREENS_SETR_OP_SET_EFFECT) as usize;
                    self.entries[k][i[1] as usize][0] = i[2];
                    self.entries[k][i[1] as usize][1] = i[3];
                    0
                }
                EE_SCREENS_SETR_OP_SET_OPERATOR => {
                    let list = &mut self.entries[i[1] as usize];
                    list[i[2] as usize][2] = i[3];
                    if i[2] as usize + 1 == list.len() && i[3] != 0 {
                        list.push([0, 0, 0]);
                    }
                    0
                }
                EE_SCREENS_SETR_OP_DELETE => {
                    self.entries[i[1] as usize].remove(i[2] as usize);
                    0
                }
                _ => {
                    self.log.push((op, p, *i));
                    0
                }
            }
        }
    }

    fn made() -> (Rec, Trigger, u32) {
        let mut h = Rec::default();
        let mut index = 114;
        let t = Trigger::new(&mut h, 99, &mut index);
        (h, t, index)
    }

    #[test]
    fn the_screen_makes_its_controls_in_the_originals_order() {
        let (h, t, index) = made();
        assert_eq!(h.made.len(), 37);
        assert_eq!(index, 114 + 37);
        let ids: Vec<u32> = h.made.iter().map(|c| c.id).collect();
        let mut want: Vec<u32> = (0x9c4..=0x9cb).collect();
        want.extend(0x9e4..=0x9e7);
        want.extend(0x9cc..=0x9e3);
        want.push(0x9e8);
        assert_eq!(ids, want);
        for (k, c) in h.made.iter().enumerate() {
            assert_eq!(c.index, 114 + k as u32);
            assert_eq!(c.groups, 1 << 9);
        }
        // the remove buttons each 2 right of the one before; the last "--------------"
        let x: Vec<f32> = (0x9d8..=0x9db).map(|id| h.made.iter().find(|c| c.id == id).unwrap().x).collect();
        assert_eq!(x, vec![180.0, 296.0, 412.0, 528.0]);
        let last = h.made.iter().find(|c| c.id == 0x9db).unwrap();
        assert_eq!((last.text, last.number), (EE_SCREENS_SEPL_TEXT_CHAR, b'-' as i32 | 14 << 8));
        // the next arrows at the second condition operator's right end less 20
        let next = h.made.iter().find(|c| c.id == 0x9dd).unwrap();
        assert_eq!(next.x, 602.0 + 55.0 - 1.0 - 20.0);
        // each operator combo given "And" and "Or"
        let appends = h.log.iter().filter(|l| l.0 == EE_SCREENS_SEPL_OP_COMBO_APPEND).count();
        assert_eq!(appends, 8);
        assert_eq!(t.operator_event, 5000);
        assert_eq!(t.conditions[0].header, 6);
        assert_eq!(t.effects[0].header, 4);
    }

    #[test]
    fn load_shows_the_rows_and_store_writes_them_back() {
        let (mut h, t, _) = made();
        h.entries = [vec![[1, 4, 1], [0, 2, 0]], vec![[30, 7, 0]]];
        h.values.insert(EE_SCREENS_SETR_V_DURATION, 12);
        t.load(&mut h, 0);
        assert!(h.on[&t.conditions[0].value]);
        assert_eq!(h.sel[&t.conditions[0].combo], 4);
        assert_eq!(h.sel[&t.conditions[0].operator], 0);
        assert_eq!(h.sel[&t.conditions[1].operator], u32::MAX);
        assert_eq!(h.number[&t.effects[0].value], 30);
        assert!(h.on[&t.timer]);
        // a single effect: its second row hidden; the arrows hidden at the start
        assert_eq!(h.mode[&t.effects[1].combo], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(h.mode[&t.conditions[1].combo], EE_SCREENS_SEPL_SHOWN);
        assert_eq!(h.mode[&t.prev_conditions], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(h.mode[&t.next_conditions], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(h.mode[&t.effects[0].remove], EE_SCREENS_SEPL_HIDDEN);
        assert_eq!(h.event[&t.conditions[0].operator], 5000);
        // the person's edits stored; the operator events detached
        h.sel.insert(t.conditions[1].combo, 9);
        h.on.insert(t.conditions[1].value, true);
        h.number.insert(t.effects[0].value, 45);
        h.on.insert(t.timer, false);
        t.store(&mut h, 0);
        assert_eq!(h.entries[0][1], [1, 9, 0]);
        assert_eq!(h.entries[1][0], [45, 7, 0]);
        assert_eq!(h.values[&EE_SCREENS_SETR_V_DURATION], 0);
        assert_eq!(h.event[&t.conditions[0].operator], 0);
        assert_eq!(h.event[&t.conditions[1].operator], 0);
    }

    #[test]
    fn a_second_rows_operator_steps_on_and_a_removal_steps_back() {
        let (mut h, t, _) = made();
        h.entries = [vec![[0, 1, 1], [0, 2, 0]], vec![[0, 3, 0]]];
        t.load(&mut h, 0);
        h.sel.insert(t.conditions[1].operator, 1);
        t.change_operator(&mut h, 0, EE_SCREENS_SETR_SECOND_CONDITION_OPERATOR);
        assert_eq!(h.entries[0].len(), 3);
        assert_eq!(h.entries[0][1][2], 2);
        assert_eq!(t.first_condition.get(), 1);
        assert_eq!(h.mode[&t.prev_conditions], EE_SCREENS_SEPL_SHOWN);
        t.remove_entry(&mut h, 0, EE_SCREENS_SETR_REMOVE_SECOND_CONDITION);
        assert_eq!(h.entries[0].len(), 2);
        assert_eq!(t.first_condition.get(), 0);
        t.step(&mut h, 0, 3);
        assert_eq!(t.first_effect.get(), 0); // a single effect: back at the start on the load
        t.list_op(&mut h, EE_SCREENS_SEOB_SM_NEW, 0);
        assert_eq!(h.log.last().unwrap().2[0], EE_SCREENS_SEOB_SM_NEW);
    }

    #[test]
    fn the_destructor_deletes_every_control_once() {
        let (mut h, t, _) = made();
        h.log.clear();
        t.destroy(&mut h);
        let mut deleted: Vec<usize> =
            h.log.iter().filter(|l| l.0 == EE_SCREENS_SEPL_OP_DELETE).map(|l| l.1[0]).collect();
        assert_eq!(deleted.len(), 37);
        deleted.sort();
        deleted.dedup();
        assert_eq!(deleted.len(), 37);
        assert_eq!(h.log.last().unwrap(), &(EE_SCREENS_SETR_OP_RELEASE_EVENT, [5000, 0, 0], [0; 4]));
    }
}
