//! The condition and effect screens' shared controls (`EESETSConditionControls`, `EESETSEffectControls`) made and
//! deleted from tables: once their constructors and destructors (`EESETSCComboBox.cpp` 0x57daa8 / 0x57e3d7,
//! `EESETSEComboBox.cpp` 0x581792 / 0x5824fc).
//!
//! The controls stay C++ members of the two classes (the types address them by array and index through the generic
//! host, `editor_effects.rs`). Each is one record of the shared control maker (`EeScreensSeplCtl`, the host
//! `SeplMake`), stored into its member (`EE_SCREENS_SEET_OP_SLOT`) and hidden after it joins its control group, as
//! the original constructors' loops did; the destructors' orders are the tables below.

use crate::editor_effects::{
    EE_SCREENS_SEET_A_CHECK, EE_SCREENS_SEET_A_COMBO, EE_SCREENS_SEET_A_ECOMBO, EE_SCREENS_SEET_A_EDIT,
    EE_SCREENS_SEET_A_EDIT2, EE_SCREENS_SEET_A_LABEL, EE_SCREENS_SEET_A_LIST, EE_SCREENS_SEET_A_PLAYER,
    EE_SCREENS_SEET_A_RADIO, EE_SCREENS_SEET_A_SCEN, EE_SCREENS_SEET_A_SLIDER, EE_SCREENS_SEET_A_TEXT,
};
use crate::editor_object::{EE_SCREENS_SEPL_COMBO_PLAIN, EE_SCREENS_SEPL_COMBO_PLAYER, EE_SCREENS_SEPL_COMBO_SCENARIO};
use crate::editor_players::{
    display, EeScreensSeplCtl, Maker, SeplHost, EE_SCREENS_SEPL_CHECK, EE_SCREENS_SEPL_EDIT, EE_SCREENS_SEPL_FG,
    EE_SCREENS_SEPL_FONT_530, EE_SCREENS_SEPL_LABEL, EE_SCREENS_SEPL_LIST_COMBO, EE_SCREENS_SEPL_OP_DELETE,
    EE_SCREENS_SEPL_RADIO, EE_SCREENS_SEPL_SLIDER, EE_SCREENS_SEPL_UNIT_LIST,
};

/// `EESETSEffectControls::mEditBox48` (the effect controls' fourth edit box; index 0).
pub const EE_SCREENS_SEET_A_EDIT48: u32 = 12;

/// The control slot i0, i1 (array, index) of the controls p0: its handle into p2's first word, or (i2 = 1) set to
/// the handle p2.
pub const EE_SCREENS_SEET_OP_SLOT: u32 = 94;

/// The maker's combo box kinds of the two screens (`EeScreensSeplCtl::extra[0]` of an `EE_SCREENS_SEPL_LIST_COMBO`):
/// `EESETSCComboBox` (its selection renames the condition), `EESETSEComboBox` (the effect's).
pub const EE_SCREENS_SEPL_COMBO_CONDITION: i32 = 5;
pub const EE_SCREENS_SEPL_COMBO_EFFECT: i32 = 6;
/// The maker's list kind (`EeScreensSeplCtl::extra[0]` of an `EE_SCREENS_SEPL_UNIT_LIST`): a `UILBString` with the
/// constructor's flag set and a border of 2 (the effect controls' two lists).
pub const EE_SCREENS_SEPL_LIST_EFFECT: i32 = 4;
/// `EeScreensSeplCtl::flags`: a slider without a range (only its page size `extra[2]`, when not 0).
pub const EE_SCREENS_SEPL_NO_RANGE: u32 = 32;

/// `kSETriggerScreenTrigger`, `kSETriggerScreenCondition`, `kSETriggerScreenEffect` (a scenario combo box's
/// screen and mode).
const SCREEN_TRIGGER: i32 = 0;
const SCREEN_CONDITION: i32 = 1;
const SCREEN_EFFECT: i32 = 2;

/// The controls' groups on the trigger form: the condition screen's, the effect screen's.
const GROUP_CONDITION: u32 = 12;
const GROUP_EFFECT: u32 = 13;

/// The text boxes' and labels' opacity.
const BG: f32 = 0.4;

/// One control of a table: its member (array and index), its record, and whether the control index skips one
/// more after it (the effect controls' second, fourth and sixth radio buttons).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct ControlSlot {
    pub a: u32,
    pub k: u32,
    pub c: EeScreensSeplCtl,
    pub skip: bool,
}

fn slot(a: u32, k: u32, c: EeScreensSeplCtl) -> ControlSlot {
    ControlSlot { a, k, c, skip: false }
}

fn label(id: u32, group: u32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, id, 0.0, 0.0, 150.0, 20.0)
        .bg(BG)
        .flags(EE_SCREENS_SEPL_FG)
        .group(group)
}

/// A combo box of the kind `kind`, `w` wide (no lines, a border of 2, its list always dirty).
fn combo(kind: i32, id: u32, w: f32, group: u32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_LIST_COMBO, id, 0.0, 0.0, w, 20.0)
        .bg(1.0)
        .bg2(BG)
        .flags(EE_SCREENS_SEPL_FG)
        .extra([kind, 0, 2, 1])
        .group(group)
}

/// A scenario combo box of the screen `screen` listing the mode `mode`.
fn scenario(id: u32, screen: i32, mode: u32, group: u32) -> EeScreensSeplCtl {
    let mut c = combo(EE_SCREENS_SEPL_COMBO_SCENARIO, id, 290.0, group);
    c.number = screen;
    c.string = mode;
    c
}

/// An edit box of `chars` characters (`number`: a number box), `w` x `h`, in the screen's font or `gU531930`.
fn edit(id: u32, chars: i32, number: bool, multi: bool, w: f32, h: f32, font530: bool, group: u32) -> EeScreensSeplCtl {
    let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_EDIT, id, 0.0, 0.0, w, h)
        .bg(BG)
        .flags(EE_SCREENS_SEPL_FG)
        .extra([chars, number as i32, multi as i32, 0])
        .group(group);
    if font530 {
        c.font(EE_SCREENS_SEPL_FONT_530)
    } else {
        c
    }
}

fn check(id: u32, group: u32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_CHECK, id, 0.0, 0.0, 250.0, 20.0).bg(BG).group(group)
}

fn radio(id: u32, w: f32, group: u32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_RADIO, id, 0.0, 0.0, w, 20.0).bg(BG).group(group)
}

/// A slider without a range, its page size `page` (0: not set).
fn slider(id: u32, page: i32, group: u32) -> EeScreensSeplCtl {
    EeScreensSeplCtl::new(EE_SCREENS_SEPL_SLIDER, id, 0.0, 0.0, 100.0, 20.0)
        .bg(BG)
        .flags(EE_SCREENS_SEPL_NO_RANGE)
        .extra([0, 0, page, i32::MIN])
        .group(group)
}

/// `EESETSConditionControls`' constructor as a table, in its order: 5 labels, 3 condition combo boxes, 4 scenario
/// combo boxes (of the trigger list), 2 number boxes, the chat text box, 3 check boxes, 2 radio buttons, 2 player
/// pickers, the slider (group 12).
pub fn condition_controls() -> Vec<ControlSlot> {
    let g = GROUP_CONDITION;
    let mut t = Vec::new();
    t.extend((0..5).map(|k| slot(EE_SCREENS_SEET_A_LABEL, k, label(0x906 + k, g))));
    t.extend(
        (0..3).map(|k| slot(EE_SCREENS_SEET_A_COMBO, k, combo(EE_SCREENS_SEPL_COMBO_CONDITION, 0x933 + k, 290.0, g))),
    );
    t.extend(
        (0..4)
            .map(|k| slot(EE_SCREENS_SEET_A_SCEN, k, scenario(0x936 + k, SCREEN_CONDITION, SCREEN_TRIGGER as u32, g))),
    );
    t.extend((0..2).map(|k| slot(EE_SCREENS_SEET_A_EDIT, k, edit(0x910 + k, 6, true, false, 60.0, 20.0, false, g))));
    t.push(slot(EE_SCREENS_SEET_A_TEXT, 0, edit(0x915, 0xff, false, false, 120.0, 20.0, true, g)));
    t.extend((0..3).map(|k| slot(EE_SCREENS_SEET_A_CHECK, k, check(0x924 + k, g))));
    t.extend((0..2).map(|k| slot(EE_SCREENS_SEET_A_RADIO, k, radio(0x91a + k, 150.0, g))));
    t.extend(
        (0..2).map(|k| slot(EE_SCREENS_SEET_A_PLAYER, k, combo(EE_SCREENS_SEPL_COMBO_PLAYER, 0x92e + k, 150.0, g))),
    );
    t.push(slot(EE_SCREENS_SEET_A_SLIDER, 0, slider(0x942, 0, g)));
    t
}

/// `EESETSEffectControls`' constructor as a table, in its order: 13 labels, 2 check boxes, 4 combo boxes, 2 effect
/// combo boxes, 4 scenario combo boxes (modes 0, 3, 3, 4), 2 number boxes, 2 multi-line boxes, the text box, the
/// fourth box, 6 radio buttons (an index skipped after each pair), 4 sliders (page size 1), 3 player pickers, 2
/// lists (group 13).
pub fn effect_controls() -> Vec<ControlSlot> {
    let g = GROUP_EFFECT;
    let mut t = Vec::new();
    t.extend((0..13).map(|k| slot(EE_SCREENS_SEET_A_LABEL, k, label(0x96a + k, g))));
    t.extend((0..2).map(|k| slot(EE_SCREENS_SEET_A_CHECK, k, check(0x9b5 + k, g))));
    t.extend((0..4).map(|k| slot(EE_SCREENS_SEET_A_COMBO, k, combo(EE_SCREENS_SEPL_COMBO_PLAIN, 0x997 + k, 290.0, g))));
    t.extend(
        (0..2).map(|k| slot(EE_SCREENS_SEET_A_ECOMBO, k, combo(EE_SCREENS_SEPL_COMBO_EFFECT, 0x99b + k, 290.0, g))),
    );
    let modes = [0u32, 3, 3, 4];
    t.extend((0..4).map(|k| slot(EE_SCREENS_SEET_A_SCEN, k, scenario(0x99d + k, SCREEN_EFFECT, modes[k as usize], g))));
    t.extend((0..2).map(|k| slot(EE_SCREENS_SEET_A_EDIT, k, edit(0x979 + k, 6, true, false, 60.0, 20.0, false, g))));
    t.extend((0..2).map(|k| {
        let w = if k == 0 { 250.0 } else { 225.0 };
        slot(EE_SCREENS_SEET_A_EDIT2, k, edit(0x97e + k, 5, false, true, w, 80.0, true, g))
    }));
    t.push(slot(EE_SCREENS_SEET_A_TEXT, 0, edit(0x983, 0xff, false, false, 250.0, 20.0, true, g)));
    t.push(slot(EE_SCREENS_SEET_A_EDIT48, 0, edit(0x9ab, 0x19, false, false, 100.0, 20.0, true, g)));
    t.extend(
        (0..6)
            .map(|k| ControlSlot { skip: k & 1 != 0, ..slot(EE_SCREENS_SEET_A_RADIO, k, radio(0x988 + k, 125.0, g)) }),
    );
    t.extend((0..4).map(|k| slot(EE_SCREENS_SEET_A_SLIDER, k, slider(0x9a6 + k, 1, g))));
    t.extend(
        (0..3).map(|k| slot(EE_SCREENS_SEET_A_PLAYER, k, combo(EE_SCREENS_SEPL_COMBO_PLAYER, 0x992 + k, 175.0, g))),
    );
    t.extend((0..2).map(|k| {
        let c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_UNIT_LIST, 0x9bf + k, 0.0, 0.0, 150.0, 150.0)
            .bg(BG)
            .flags(EE_SCREENS_SEPL_FG)
            .extra([EE_SCREENS_SEPL_LIST_EFFECT, 0, 0, 0])
            .group(g);
        slot(EE_SCREENS_SEET_A_LIST, k, c)
    }));
    t
}

/// `EESETSConditionControls`' destructor's order: (array, count).
pub const CONDITION_DELETE_ORDER: [(u32, u32); 9] = [
    (EE_SCREENS_SEET_A_LABEL, 5),
    (EE_SCREENS_SEET_A_COMBO, 3),
    (EE_SCREENS_SEET_A_SCEN, 4),
    (EE_SCREENS_SEET_A_EDIT, 2),
    (EE_SCREENS_SEET_A_TEXT, 1),
    (EE_SCREENS_SEET_A_CHECK, 3),
    (EE_SCREENS_SEET_A_RADIO, 2),
    (EE_SCREENS_SEET_A_PLAYER, 2),
    (EE_SCREENS_SEET_A_SLIDER, 1),
];

/// `EESETSEffectControls`' destructor's order: (array, count).
pub const EFFECT_DELETE_ORDER: [(u32, u32); 13] = [
    (EE_SCREENS_SEET_A_LABEL, 13),
    (EE_SCREENS_SEET_A_COMBO, 4),
    (EE_SCREENS_SEET_A_ECOMBO, 2),
    (EE_SCREENS_SEET_A_SCEN, 4),
    (EE_SCREENS_SEET_A_TEXT, 1),
    (EE_SCREENS_SEET_A_EDIT2, 2),
    (EE_SCREENS_SEET_A_EDIT, 2),
    (EE_SCREENS_SEET_A_EDIT48, 1),
    (EE_SCREENS_SEET_A_RADIO, 6),
    (EE_SCREENS_SEET_A_PLAYER, 3),
    (EE_SCREENS_SEET_A_SLIDER, 4),
    (EE_SCREENS_SEET_A_CHECK, 2),
    (EE_SCREENS_SEET_A_LIST, 2),
];

fn slot_get<H: SeplHost + ?Sized>(h: &mut H, controls: usize, a: u32, k: u32) -> usize {
    let mut out = 0usize;
    let (mut i, mut f) = ([a as i32, k as i32, 0, 0], [0.0f32; 4]);
    h.op(EE_SCREENS_SEET_OP_SLOT, [controls, 0, &mut out as *mut usize as usize], &mut i, &mut f);
    out
}

fn slot_set<H: SeplHost + ?Sized>(h: &mut H, controls: usize, a: u32, k: u32, control: usize) {
    let (mut i, mut f) = ([a as i32, k as i32, 1, 0], [0.0f32; 4]);
    h.op(EE_SCREENS_SEET_OP_SLOT, [controls, 0, control], &mut i, &mut f);
}

/// The table's controls made on `form` from the control index `*index` (advanced) through the maker's host `s`,
/// each stored into its member of `controls` through the types' host `h`, then hidden.
pub(crate) fn make_controls<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(
    s: &mut S,
    h: &mut H,
    controls: usize,
    form: usize,
    index: &mut u32,
    table: &[ControlSlot],
) {
    for t in table {
        let control = {
            let mut m = Maker { h: &mut *s, form, index: *index };
            let control = m.make(t.c);
            *index = m.index;
            control
        };
        slot_set(h, controls, t.a, t.k, control);
        display(s, control, false);
        if t.skip {
            *index += 1;
        }
    }
}

/// The controls deleted in the destructor's order `order`.
pub(crate) fn delete_controls<S: SeplHost + ?Sized, H: SeplHost + ?Sized>(
    s: &mut S,
    h: &mut H,
    controls: usize,
    order: &[(u32, u32)],
) {
    for &(a, n) in order {
        for k in 0..n {
            let control = slot_get(h, controls, a, k);
            let (mut i, mut f) = ([0i32; 4], [0.0f32; 4]);
            s.op(EE_SCREENS_SEPL_OP_DELETE, [control, 0, 0], &mut i, &mut f);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn condition_table_has_the_original_ids_and_order() {
        let t = condition_controls();
        assert_eq!(t.len(), 5 + 3 + 4 + 2 + 1 + 3 + 2 + 2 + 1);
        let ids: Vec<u32> = t.iter().map(|s| s.c.id).collect();
        assert_eq!(&ids[..5], &[0x906, 0x907, 0x908, 0x909, 0x90a]);
        assert_eq!(&ids[5..12], &[0x933, 0x934, 0x935, 0x936, 0x937, 0x938, 0x939]);
        assert_eq!(&ids[12..15], &[0x910, 0x911, 0x915]);
        assert_eq!(*ids.last().unwrap(), 0x942);
        assert!(t.iter().all(|s| s.c.groups == 1 << 12 && !s.skip));
        // the chat box: gU531930, 255 characters; the scenario combo boxes list the triggers of the condition screen
        assert_eq!(t[14].c.font, EE_SCREENS_SEPL_FONT_530);
        assert_eq!(t[14].c.extra[0], 0xff);
        assert_eq!((t[8].c.number, t[8].c.string), (SCREEN_CONDITION, SCREEN_TRIGGER as u32));
        // the delete order covers every control once
        let n: u32 = CONDITION_DELETE_ORDER.iter().map(|o| o.1).sum();
        assert_eq!(n as usize, t.len());
    }

    #[test]
    fn effect_table_skips_an_index_after_each_radio_pair() {
        let t = effect_controls();
        assert_eq!(t.len(), 13 + 2 + 4 + 2 + 4 + 2 + 2 + 1 + 1 + 6 + 4 + 3 + 2);
        let skips: Vec<u32> = t.iter().filter(|s| s.skip).map(|s| s.c.id).collect();
        assert_eq!(skips, vec![0x989, 0x98b, 0x98d]);
        let scen: Vec<u32> = t.iter().filter(|s| s.a == EE_SCREENS_SEET_A_SCEN).map(|s| s.c.string).collect();
        assert_eq!(scen, vec![0, 3, 3, 4]);
        let sliders: Vec<i32> = t.iter().filter(|s| s.a == EE_SCREENS_SEET_A_SLIDER).map(|s| s.c.extra[2]).collect();
        assert_eq!(sliders, vec![1; 4]);
        let wide: Vec<f32> = t.iter().filter(|s| s.a == EE_SCREENS_SEET_A_EDIT2).map(|s| s.c.w).collect();
        assert_eq!(wide, vec![250.0, 225.0]);
        let n: u32 = EFFECT_DELETE_ORDER.iter().map(|o| o.1).sum();
        assert_eq!(n as usize, t.len());
    }

    /// A host recording the ops: the maker answers handles 1, 2, ...
    #[derive(Default)]
    struct Rec {
        ops: Vec<(u32, [usize; 3], [i32; 4])>,
        next: usize,
    }

    impl SeplHost for Rec {
        fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]) -> i32 {
            if op == crate::editor_players::EE_SCREENS_SEPL_OP_MAKE {
                self.next += 1;
                // SAFETY: the maker passes its out handle in p0
                unsafe { *(p[0] as *mut usize) = self.next };
                let c = unsafe { &*(p[1] as *const EeScreensSeplCtl) };
                self.ops.push((op, [0, 0, 0], [c.index as i32, c.id as i32, 0, 0]));
                return 0;
            }
            self.ops.push((op, p, *i));
            0
        }
    }

    #[test]
    fn make_stores_hides_and_advances_the_index() {
        let (mut s, mut h) = (Rec::default(), Rec::default());
        let mut index = 40;
        make_controls(&mut s, &mut h, 77, 5, &mut index, &effect_controls());
        assert_eq!(index, 40 + 46 + 3);
        // each control stored into its slot, then hidden
        assert_eq!(h.ops[0], (EE_SCREENS_SEET_OP_SLOT, [77, 0, 1], [EE_SCREENS_SEET_A_LABEL as i32, 0, 1, 0]));
        assert_eq!(s.ops[1].0, crate::editor_players::EE_SCREENS_SEPL_OP_SET_DISPLAY);
        // the radios' indexes after the 31 controls before them: 31, 32, (skip) 34, 35, (skip) 37, 38
        let radios: Vec<i32> = s
            .ops
            .iter()
            .filter(|o| o.1 == [0, 0, 0] && (0x988..0x98e).contains(&o.2[1]))
            .map(|o| o.2[0] - 40)
            .collect();
        assert_eq!(radios, vec![31, 32, 34, 35, 37, 38]);
    }
}
