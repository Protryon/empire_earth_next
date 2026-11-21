//! The trigger tab family's composed ops as statement streams.

use std::collections::HashMap;

use super::*;

/// A statement as the tests see it: its op, p0, p1 and integers (a text passed in: its bytes as the last item).
#[derive(Clone, Debug, PartialEq)]
struct Seen {
    op: u32,
    p0: usize,
    p1: usize,
    i: [i32; 4],
    text: Vec<u8>,
}

/// The statements whose p2 is an out word (a found or made object): answered with `0x1000 + op`, unless the table
/// says 0 (none found).
const FINDS: &[u32] = &[
    EE_SCREENS_TRGX_R_SLOT,
    EE_SCREENS_TRGX_R_TRIGGER_EVENT,
    EE_SCREENS_TRGX_R_SETS_CONTROL,
    EE_SCREENS_SEETX_CHECK_GETEXT,
    EE_SCREENS_SEETX_RADIO_GETEXT,
    EE_SCREENS_SEETX_LIST_GETEXT,
    EE_SCREENS_SEETX_EV_SCREEN,
    EE_SCREENS_SEETX_EV_SEE,
    EE_SCREENS_SEETX_EV_SLIDER,
    EE_SCREENS_SEETX_EV_USE_OBJECT_AREA,
    EE_SCREENS_SEETX_OBJECT,
    EE_SCREENS_SEETX_TRIGGER,
    EE_SCREENS_SEETX_CALAMITY,
    EE_SCREENS_SEETX_GRAPHIC,
    EE_SCREENS_SEETX_UNIT_TYPE_CLASS,
    EE_SCREENS_SEETX_TECH_NODE,
    EE_SCREENS_SEETX_TRIGGER_BOX,
    EE_SCREENS_SEETX_EFFECT_ENTRY,
    EE_SCREENS_SEETX_EFFECT_FIELDS,
    EE_SCREENS_SEETX_CONDITION_ENTRY,
    EE_SCREENS_SEETX_CONDITION_FIELDS,
    EE_SCREENS_SEOBX_NEW_CLICK,
    EE_SCREENS_SEOBX_NEW_LASSO,
    EE_SCREENS_SEOBX_NEW_SELECT,
    EE_SCREENS_SEOBX_WORLD_UNIT,
    EE_SCREENS_SETRX_CONDITION,
    EE_SCREENS_SETRX_EFFECT,
    EE_SCREENS_SETRX_TRIGGER_TAB,
    EE_SCREENS_SETSX_NEW_TAB,
    EE_SCREENS_SETSX_RADIO_GETEXT,
    EE_SCREENS_SETSX_SCREEN_EVENT,
    EE_SCREENS_SETSX_EDITOR,
    EE_SCREENS_SETSX_CONTROL_BY_ID,
    EE_SCREENS_SEARX_AREA,
    EE_SCREENS_SEARX_TILE_AT,
    EE_SCREENS_SEINX_NEW_PATH,
    EE_SCREENS_SEINX_NEW_FILE_COMBO,
    EE_SCREENS_SEINX_COMBO_GETEXT,
    EE_SCREENS_SEINX_NEW_IMPORT,
];

/// The statements that read a text into p2's buffer: answered "t<op>".
const TEXTS: &[u32] = &[
    EE_SCREENS_TRGX_R_EDIT_LINE,
    EE_SCREENS_TRGX_R_EDIT_ALL_TEXT,
    EE_SCREENS_SEETX_NAMES_AT,
    EE_SCREENS_SEETX_KEPT_TEXT,
    EE_SCREENS_SEETX_EFFECT_TEXT + 3,
    EE_SCREENS_SEETX_OBJECT_NAME,
    EE_SCREENS_SEETX_TRIGGER_NAME,
    EE_SCREENS_SETRX_NAME,
    EE_SCREENS_SETRX_TEXT,
    EE_SCREENS_SEARX_NAME,
    EE_SCREENS_SEARX_TEXT,
];

/// The statements that take a text (p1's i0 bytes; p0's for those on no object).
const PUTS: &[u32] = &[
    EE_SCREENS_TRGX_R_LABEL_SET_TEXT,
    EE_SCREENS_TRGX_R_EDIT_SET_TEXT,
    EE_SCREENS_TRGX_R_COMBO_APPEND,
    EE_SCREENS_TRGX_R_LIST_APPEND,
    EE_SCREENS_SEETX_RADIO_TEXT,
    EE_SCREENS_SEETX_CHECK_TEXT,
    EE_SCREENS_SEETX_FORMAT_NUMBER,
    EE_SCREENS_SEOBX_SET_NAME,
    EE_SCREENS_SETRX_SET_NAME,
    EE_SCREENS_SETRX_SET_TEXT,
    EE_SCREENS_SETRX_FORMAT_REMOVE,
    EE_SCREENS_SETRX_COMBO_LABEL_TEXT,
    EE_SCREENS_SEARX_SET_NAME,
    EE_SCREENS_SEARX_SET_TEXT,
    EE_SCREENS_SEETX_SET_CONDITION_TEXT + 2,
];

struct Fake {
    answers: HashMap<u32, i32>,
    seen: Vec<Seen>,
}

impl Fake {
    fn new(answers: &[(u32, i32)]) -> Fake {
        Fake { answers: answers.iter().copied().collect(), seen: Vec::new() }
    }

    fn statement(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let text = if PUTS.contains(&op) {
            let base = if p[1] != 0 { p[1] } else { p[0] };
            // SAFETY: the statement's text (i0 bytes)
            unsafe { core::slice::from_raw_parts(base as *const u8, i[0].max(0) as usize) }.to_vec()
        } else if op == EE_SCREENS_SEETX_EVENT_ADD_STRING {
            // SAFETY: as above (p0's)
            unsafe { core::slice::from_raw_parts(p[0] as *const u8, i[0].max(0) as usize) }.to_vec()
        } else {
            Vec::new()
        };
        self.seen.push(Seen { op, p0: p[0], p1: p[1], i: *i, text });
        let answer = self.answers.get(&op).copied();
        if FINDS.contains(&op) {
            let v = if answer == Some(0) { 0 } else { 0x1000 + op as usize };
            // SAFETY: the out word
            unsafe { *(p[2] as *mut usize) = v };
            return 0;
        }
        if TEXTS.contains(&op) || op == EE_SCREENS_TRGX_R_STRING {
            if op == EE_SCREENS_TRGX_R_STRING && answer == Some(-1) {
                return -1;
            }
            let t = if op == EE_SCREENS_TRGX_R_STRING { format!("s{}", i[0]) } else { format!("t{op}") };
            let n = t.len().min(i[3].max(0) as usize);
            // SAFETY: the buffer of i3 bytes
            unsafe { core::ptr::copy_nonoverlapping(t.as_ptr(), p[2] as *mut u8, n) };
            return t.len() as i32;
        }
        if op == EE_SCREENS_SEARX_RECT + 2 {
            f[0] = 7.5;
        }
        if op == EE_SCREENS_SEARX_PAINT_AREA {
            *i = [1, 2, 3, 4];
        }
        answer.unwrap_or(0)
    }

    fn ops(&self) -> Vec<u32> {
        self.seen.iter().map(|s| s.op).collect()
    }

    fn of(&self, op: u32) -> Vec<Seen> {
        self.seen.iter().filter(|s| s.op == op).cloned().collect()
    }
}

/// Runs `body` over a fake whose statements answer by `answers`.
fn with(answers: &[(u32, i32)], body: impl FnOnce(&mut Trgx) -> Option<i32>) -> (Option<i32>, Fake) {
    let mut fake = Fake::new(answers);
    let r = {
        let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]| fake.statement(op, p, i, f);
        body(&mut x)
    };
    (r, fake)
}

fn handle(op: u32) -> usize {
    0x1000 + op as usize
}

// ---- the generic host

#[test]
fn an_op_the_cpp_answers_is_not_composed() {
    let (r, fake) = with(&[], |x| seet(x, false, EE_SCREENS_SEET_OP_SET_RECT, [1, 0, 0], &mut [0; 4], &mut [0.0; 4]));
    assert_eq!(r, None);
    assert!(fake.seen.is_empty());
    let (r, _) = with(&[], |x| seet(x, true, EE_SCREENS_SEET_OP_SET_OBJECTS, [1, 0, 0], &mut [0; 4], &mut [0.0; 4]));
    assert_eq!(r, None, "the condition host has no objects");
}

#[test]
fn align_sets_the_label_then_notes_it() {
    let (r, fake) =
        with(&[], |x| seet(x, false, EE_SCREENS_SEET_OP_ALIGN, [9, 0, 0], &mut [0, 3, 2, 0], &mut [0.0; 4]));
    assert_eq!(r, Some(0));
    assert_eq!(fake.ops(), [EE_SCREENS_TRGX_R_SLOT, EE_SCREENS_SEETX_ALIGN, EE_SCREENS_SEETX_ALIGN_NOTE]);
    assert_eq!(fake.seen[0].i, [0, EE_SCREENS_SEET_A_LABEL as i32, 3, 0]);
    assert_eq!(fake.seen[1].p0, handle(EE_SCREENS_TRGX_R_SLOT));
    assert_eq!(fake.seen[2].i[0], 2);
}

#[test]
fn yellow_colours_the_control_text_by_its_array() {
    for (a, get, array) in [
        (EE_SCREENS_SEET_A_CHECK, EE_SCREENS_SEETX_CHECK_GETEXT, EE_SCREENS_SEET_A_CHECK),
        (EE_SCREENS_SEET_A_RADIO, EE_SCREENS_SEETX_RADIO_GETEXT, EE_SCREENS_SEET_A_RADIO),
        (EE_SCREENS_SEET_A_LABEL, EE_SCREENS_SEETX_LIST_GETEXT, EE_SCREENS_SEET_A_LIST),
    ] {
        let (_, fake) =
            with(&[], |x| seet(x, true, EE_SCREENS_SEET_OP_YELLOW, [9, 0, 0], &mut [a as i32, 1, 0, 0], &mut [0.0; 4]));
        assert_eq!(fake.ops(), [EE_SCREENS_TRGX_R_SLOT, get, EE_SCREENS_SEETX_YELLOW, EE_SCREENS_SEETX_TRANSPARENT]);
        assert_eq!(fake.seen[0].i, [1, array as i32, 1, 0]);
        assert_eq!(fake.seen[2].p0, handle(get));
    }
}

#[test]
fn font_530_takes_the_edit_box_else_the_combo_and_its_list() {
    let (_, fake) = with(&[], |x| {
        seet(
            x,
            false,
            EE_SCREENS_SEET_OP_FONT_530,
            [9, 0, 0],
            &mut [EE_SCREENS_SEET_A_EDIT as i32, 0, 1, 0],
            &mut [0.0; 4],
        )
    });
    assert_eq!(fake.ops(), [EE_SCREENS_TRGX_R_SLOT, EE_SCREENS_SEETX_EDIT_FONT_530]);
    let (_, fake) = with(&[], |x| {
        seet(
            x,
            false,
            EE_SCREENS_SEET_OP_FONT_530,
            [9, 0, 0],
            &mut [EE_SCREENS_SEET_A_COMBO as i32, 0, 1, 0],
            &mut [0.0; 4],
        )
    });
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_TRGX_R_SLOT,
            EE_SCREENS_SEETX_COMBO_FONT_530,
            EE_SCREENS_TRGX_R_SLOT,
            EE_SCREENS_SEETX_COMBO_LIST_FONT_530
        ]
    );
    // an edit array whose slot is empty: the combo path on no combo (as the C++'s NULL)
    let (_, fake) = with(&[(EE_SCREENS_TRGX_R_SLOT, 0)], |x| {
        seet(
            x,
            false,
            EE_SCREENS_SEET_OP_FONT_530,
            [9, 0, 0],
            &mut [EE_SCREENS_SEET_A_TEXT as i32, 0, 0, 0],
            &mut [0.0; 4],
        )
    });
    assert_eq!(fake.ops(), [EE_SCREENS_TRGX_R_SLOT, EE_SCREENS_SEETX_COMBO_FONT_530]);
    assert_eq!(fake.seen[1].p0, 0);
}

#[test]
fn fill_names_appends_the_array_or_each_line() {
    let (_, fake) = with(&[], |x| {
        seet(
            x,
            false,
            EE_SCREENS_SEET_OP_FILL_NAMES,
            [9, 0, 0],
            &mut [EE_SCREENS_SEET_A_SCEN as i32, 0, 5, 1],
            &mut [0.0; 4],
        )
    });
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_TRGX_R_SLOT,
            EE_SCREENS_SEETX_NAMES_CLEAR,
            EE_SCREENS_SEETX_NAMES_GET,
            EE_SCREENS_SEETX_COMBO_APPEND_NAMES
        ]
    );
    assert_eq!(fake.seen[2].i[..2], [1, 5]);
    let (_, fake) = with(&[(EE_SCREENS_SEETX_NAMES_COUNT, 2)], |x| {
        seet(
            x,
            false,
            EE_SCREENS_SEET_OP_FILL_NAMES,
            [9, 0, 0],
            &mut [EE_SCREENS_SEET_A_SCEN as i32, 0, 5, 0],
            &mut [0.0; 4],
        )
    });
    let appended = fake.of(EE_SCREENS_TRGX_R_COMBO_APPEND);
    assert_eq!(appended.len(), 2);
    assert_eq!(appended[0].text, format!("t{EE_SCREENS_SEETX_NAMES_AT}").into_bytes());
    assert_eq!(appended[0].p0, handle(EE_SCREENS_TRGX_R_SLOT));
}

#[test]
fn fill_area_object_fills_then_selects_the_first_silently() {
    let (_, fake) = with(&[(EE_SCREENS_SEETX_NAMES_COUNT, 1)], |x| {
        fill_area_object(x, 11, 22, true);
        None
    });
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_SEETX_SHELL_OP,
            EE_SCREENS_TRGX_R_STRING,
            EE_SCREENS_SEETX_NAMES_CLEAR,
            EE_SCREENS_SEETX_NAMES_GET,
            EE_SCREENS_SEETX_NAMES_COUNT,
            EE_SCREENS_SEETX_NAMES_AT,
            EE_SCREENS_TRGX_R_LABEL_SET_TEXT,
            EE_SCREENS_TRGX_R_COMBO_CLEAR,
            EE_SCREENS_TRGX_R_COMBO_APPEND,
            EE_SCREENS_TRGX_R_TRIGGER_EVENT,
            EE_SCREENS_TRGX_R_EVENT_CALL,
            EE_SCREENS_TRGX_R_SET_TRIGGER,
            EE_SCREENS_TRGX_R_COMBO_SELECT,
            EE_SCREENS_TRGX_R_SET_TRIGGER,
            EE_SCREENS_TRGX_R_EVENT_CALL,
        ]
    );
    assert_eq!(fake.seen[0].i[..2], [EE_SCREENS_SKOP_SET_MODE as i32, 4]);
    assert_eq!(fake.seen[1].i[0], 30303, "the area title");
    assert_eq!(fake.seen[3].i[1], EE_SCENARIO_NAMES_AREAS as i32);
    assert_eq!(fake.seen[6].text, b"s30303");
    assert_eq!(fake.seen[10].i[0], 2, "referenced");
    assert_eq!(fake.seen[11].p1, 0, "taken off");
    assert_eq!(fake.seen[13].p1, handle(EE_SCREENS_TRGX_R_TRIGGER_EVENT), "put back");
    assert_eq!(fake.seen[14].i[0], 1, "released");
    // no event: nothing to hold or restore
    let (_, fake) = with(&[(EE_SCREENS_TRGX_R_TRIGGER_EVENT, 0)], |x| {
        fill_area_object(x, 11, 22, false);
        None
    });
    assert_eq!(fake.of(EE_SCREENS_TRGX_R_EVENT_CALL).len(), 0);
    assert_eq!(fake.of(EE_SCREENS_TRGX_R_SET_TRIGGER).len(), 1);
    assert_eq!(fake.seen[0].i[1], 3, "objects");
}

#[test]
fn the_entry_and_its_fields() {
    let mut out = [7usize; 2];
    let (_, fake) = with(&[], |x| {
        seet(x, true, EE_SCREENS_SEET_OP_ENTRY, [0, 0, out.as_mut_ptr() as usize], &mut [0, 0, 4, 0], &mut [0.0; 4])
    });
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_CONDITION_ENTRY, EE_SCREENS_SEETX_CONDITION_FIELDS]);
    assert_eq!(out, [handle(EE_SCREENS_SEETX_CONDITION_ENTRY), handle(EE_SCREENS_SEETX_CONDITION_FIELDS)]);
    let (_, fake) = with(&[(EE_SCREENS_SEETX_EFFECT_ENTRY, 0)], |x| {
        seet(x, false, EE_SCREENS_SEET_OP_ENTRY, [0, 0, out.as_mut_ptr() as usize], &mut [0, 0, 4, 0], &mut [0.0; 4])
    });
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_EFFECT_ENTRY]);
    assert_eq!(out, [0, 0]);
}

#[test]
fn the_entry_texts_by_field() {
    let mut buf = [0u8; 16];
    let (r, fake) = with(&[], |x| {
        seet(
            x,
            false,
            EE_SCREENS_SEET_OP_EFFECT_TEXT,
            [0, 5, buf.as_mut_ptr() as usize],
            &mut [0, 16, EE_SCREENS_SEET_S_DESCRIPTION, 0],
            &mut [0.0; 4],
        )
    });
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_EFFECT_TEXT + 3]);
    assert_eq!(r, Some(3));
    let text = b"Hello";
    let (_, fake) = with(&[], |x| {
        seet(
            x,
            true,
            EE_SCREENS_SEET_OP_SET_EFFECT_TEXT,
            [0, 5, text.as_ptr() as usize],
            &mut [0, 0, EE_SCREENS_SEET_S_NAME, 5],
            &mut [0.0; 4],
        )
    });
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_SET_CONDITION_TEXT + 2]);
    assert_eq!(fake.seen[0].text, b"Hello");
    assert_eq!(fake.seen[0].p0, 5);
}

#[test]
fn the_lists_entry_operations() {
    for (op, want, entry) in [
        (EE_SCREENS_SEOB_SM_SORT, EE_SCREENS_SEETX_SM_SORT, [2, 0, 0]),
        (EE_SCREENS_SEOB_SM_DOWN, EE_SCREENS_SEETX_SM_MOVE, [2, 6, 0]),
        (EE_SCREENS_SEOB_SM_UP, EE_SCREENS_SEETX_SM_MOVE, [2, 6, 1]),
        (EE_SCREENS_SEOB_SM_DELETE, EE_SCREENS_SEETX_SM_DELETE, [2, 6, 0]),
        (EE_SCREENS_SEOB_SM_DUPLICATE, EE_SCREENS_SEETX_SM_DUPLICATE, [2, 6, 0]),
        (EE_SCREENS_SEOB_SM_NEW, EE_SCREENS_SEETX_SM_NEW, [2, 0, 0]),
    ] {
        let (_, fake) =
            with(&[], |x| seet(x, false, EE_SCREENS_SEET_OP_LIST, [0; 3], &mut [0, 0, op, 6], &mut [0.0; 4]));
        assert_eq!(fake.ops(), [want]);
        assert_eq!(fake.seen[0].i[..3], entry);
    }
}

#[test]
fn techs_keep_the_shown_then_sort() {
    // the node ids 10, 11, 12, then the two epochs pushed; 11 not shown, 12 makes a unit (dropped with i2 != 0)
    let mut kept: Vec<i32> = vec![10, 11, 12];
    let mut seen = Vec::new();
    let mut x = |op: u32, _p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]| -> i32 {
        seen.push(op);
        match op {
            EE_SCREENS_SEETX_EPOCH_COUNT => 2,
            EE_SCREENS_SEETX_TECHS_PUSH => {
                kept.push(i[0]);
                0
            }
            EE_SCREENS_SEETX_TECHS_COUNT => kept.len() as i32,
            EE_SCREENS_SEETX_TECHS_AT => kept[i[0] as usize],
            EE_SCREENS_SEETX_TECH_SHOWN => (i[0] != 11) as i32,
            EE_SCREENS_SEETX_TECH_NO_UNIT => (i[0] != 12) as i32,
            EE_SCREENS_SEETX_TECHS_ERASE => {
                kept.remove(i[0] as usize);
                0
            }
            _ => 0,
        }
    };
    let r = seet(&mut x, true, EE_SCREENS_SEET_OP_TECHS, [0; 3], &mut [0, 0, 1, 0], &mut [0.0; 4]);
    drop(x);
    assert_eq!(r, Some(3));
    assert_eq!(
        seen[..6],
        [
            EE_SCREENS_SEETX_TECHS_CLEAR,
            EE_SCREENS_SEETX_TECHS_NODE_IDS,
            EE_SCREENS_SEETX_EPOCH_COUNT,
            EE_SCREENS_SEETX_TECHS_PUSH,
            EE_SCREENS_SEETX_EPOCH_COUNT,
            EE_SCREENS_SEETX_TECHS_PUSH,
        ]
    );
    assert_eq!(seen[seen.len() - 2..], [EE_SCREENS_SEETX_TECHS_SORT, EE_SCREENS_SEETX_TECHS_COUNT]);
    assert_eq!(kept, [10, 0, 1]);
}

#[test]
fn tech_names_take_the_unit_class_when_there_is_one() {
    let (_, fake) = with(
        &[
            (EE_SCREENS_SEETX_NODE_UNIT_CLASS, 4),
            (EE_SCREENS_SEETX_CLASS_EXISTS, 1),
            (EE_SCREENS_SEETX_CLASS_NAME_ID, 77),
        ],
        |x| {
            assert_eq!(tech_name(x, 3), b"s77");
            None
        },
    );
    assert_eq!(fake.of(EE_SCREENS_SEETX_NODE_NAME_ID).len(), 0);
    let (_, _) = with(&[(EE_SCREENS_SEETX_NODE_UNIT_CLASS, -1), (EE_SCREENS_SEETX_NODE_NAME_ID, 9)], |x| {
        assert_eq!(tech_name(x, 3), b"s9");
        None
    });
    let (_, _) = with(&[(EE_SCREENS_SEETX_TECH_NODE, 0)], |x| {
        assert!(tech_name(x, 3).is_empty());
        None
    });
}

#[test]
fn a_fonts_tech_line_names_its_node() {
    let mut buf = [0u8; 64];
    let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]| -> i32 {
        match op {
            EE_SCREENS_SEETX_TECH_NODE => {
                // SAFETY: the out word
                unsafe { *(p[2] as *mut usize) = 5 };
                0
            }
            EE_SCREENS_SEETX_NODE_UNIT_CLASS => -1,
            EE_SCREENS_SEETX_NODE_ID => 42,
            EE_SCREENS_TRGX_R_STRING => {
                // SAFETY: the buffer
                unsafe { core::ptr::copy_nonoverlapping(b"FONTS".as_ptr(), p[2] as *mut u8, 5.min(i[3] as usize)) };
                5
            }
            _ => 0,
        }
    };
    let r = seet(
        &mut x,
        false,
        EE_SCREENS_SEET_OP_TECH_LINE,
        [0, 0, buf.as_mut_ptr() as usize],
        &mut [0, 64, 3, 0],
        &mut [0.0; 4],
    );
    let want = b"BAD Node 42 - (name not available in release)";
    assert_eq!(r, Some(want.len() as i32));
    assert_eq!(&buf[..want.len()], want);
}

#[test]
fn unit_types_put_type_zero_first() {
    let (_, fake) = with(&[], |x| {
        assert!(unit_type_less(x, 0, 5));
        None
    });
    assert!(fake.seen.is_empty());
}

#[test]
fn the_unit_type_domain() {
    for (domains, want) in [
        (1, EE_SCREENS_SEET_D_AIR),
        (5, EE_SCREENS_SEET_D_AIR),
        (4, EE_SCREENS_SEET_D_SEA),
        (2, EE_SCREENS_SEET_D_LAND),
    ] {
        let mut i = [0, 0, 3, 0];
        let (r, _) = with(
            &[
                (EE_SCREENS_SEETX_CLASS_SELECTABLE, 1),
                (EE_SCREENS_SEETX_CLASS_TYPE, 17),
                (EE_SCREENS_SEETX_CLASS_DOMAINS, domains),
            ],
            |x| seet(x, false, EE_SCREENS_SEET_OP_UNIT_TYPE, [0; 3], &mut i, &mut [0.0; 4]),
        );
        assert_eq!(r, Some(want));
        assert_eq!(i[2], 17);
    }
    let (r, _) = with(&[], |x| seet(x, false, EE_SCREENS_SEET_OP_UNIT_TYPE, [0; 3], &mut [0, 0, 3, 0], &mut [0.0; 4]));
    assert_eq!(r, Some(-1), "not selectable");
}

#[test]
fn calamity_and_graphic_reads() {
    let mut i = [0, 0, 2, 0];
    let (r, _) = with(&[(EE_SCREENS_SEETX_CALAMITY_ID, 8), (EE_SCREENS_SEETX_CALAMITY_NAME, 9)], |x| {
        seet(x, false, EE_SCREENS_SEET_OP_CALAMITY, [0; 3], &mut i, &mut [0.0; 4])
    });
    assert_eq!((r, i[2], i[3]), (Some(1), 8, 9));
    let (r, fake) = with(&[], |x| seet(x, false, EE_SCREENS_SEET_OP_GRAPHIC, [0; 3], &mut [0, 0, 2, 0], &mut [0.0; 4]));
    assert_eq!(r, Some(0), "not listed");
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_GRAPHIC, EE_SCREENS_SEETX_GRAPHIC_LISTED]);
    let (r, _) = with(&[], |x| seet(x, false, EE_SCREENS_SEET_OP_BEHAVIOR, [0; 3], &mut [0, 0, 2, 99], &mut [0.0; 4]));
    assert_eq!(r, Some(0));
    let (_, fake) =
        with(&[], |x| seet(x, false, EE_SCREENS_SEET_OP_BEHAVIOR, [0; 3], &mut [0, 0, 2, 99], &mut [0.0; 4]));
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_BEHAVIOR + EE_SCREENS_SEET_B_FLOAT_SCALE as u32]);
}

#[test]
fn a_format_goes_through_the_kept_text() {
    let mut buf = [0u8; 32];
    let (r, fake) = with(&[], |x| {
        seet(
            x,
            false,
            EE_SCREENS_SEET_OP_FORMAT_NUMBER,
            [0, 0, buf.as_mut_ptr() as usize],
            &mut [0, 32, 20000, 7],
            &mut [0.0; 4],
        )
    });
    assert_eq!(fake.ops(), [EE_SCREENS_TRGX_R_STRING, EE_SCREENS_SEETX_FORMAT_NUMBER, EE_SCREENS_SEETX_KEPT_TEXT]);
    assert_eq!(fake.seen[1].text, b"s20000");
    assert_eq!(fake.seen[1].i[1], 7);
    assert_eq!(r, Some(3));
}

// ---- the object screen

#[test]
fn an_object_op_on_a_missing_object_does_nothing() {
    let (r, fake) = with(&[(EE_SCREENS_SEETX_OBJECT, 0)], |x| {
        seob(x, EE_SCREENS_SEOB_OP_OBJ_SET, [0; 3], &mut [4, 1, 2, 0], &mut [0.0; 4])
    });
    assert_eq!(r, Some(0));
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_OBJECT]);
    let (r, _) = with(&[(EE_SCREENS_SEETX_OBJECT, 0)], |x| {
        seob(x, EE_SCREENS_SEOB_OP_OBJ_EXISTS, [0; 3], &mut [4, 0, 0, 0], &mut [0.0; 4])
    });
    assert_eq!(r, Some(0));
    let (r, _) = with(&[], |x| seob(x, EE_SCREENS_SEOB_OP_OBJ_EXISTS, [0; 3], &mut [4, 0, 0, 0], &mut [0.0; 4]));
    assert_eq!(r, Some(1));
    let (r, fake) = with(&[], |x| seob(x, EE_SCREENS_SEOB_OP_SELECTED_COUNT, [0; 3], &mut [0; 4], &mut [0.0; 4]));
    assert_eq!(r, None);
    assert!(fake.seen.is_empty(), "no object for the other ops");
}

#[test]
fn the_object_fields_by_kind() {
    for (field, want) in [
        (EE_SCREENS_SEOB_F_DYNAMIC, EE_SCREENS_SEOBX_SET_FLAG),
        (EE_SCREENS_SEOB_F_PARENT, EE_SCREENS_SEOBX_SET_PARENT),
        (EE_SCREENS_SEOB_F_OWNER, EE_SCREENS_SEOBX_SET_FIELD),
    ] {
        let (_, fake) =
            with(&[], |x| seob(x, EE_SCREENS_SEOB_OP_OBJ_SET, [0; 3], &mut [4, field, -1, 0], &mut [0.0; 4]));
        assert_eq!(fake.ops(), [EE_SCREENS_SEETX_OBJECT, want]);
        assert_eq!(fake.seen[1].i[..2], [field, -1]);
        assert_eq!(fake.seen[1].p0, handle(EE_SCREENS_SEETX_OBJECT));
    }
    let (_, fake) = with(&[], |x| {
        seob(x, EE_SCREENS_SEOB_OP_OBJ_GET, [0; 3], &mut [4, EE_SCREENS_SEOB_F_PERCENT, 0, 0], &mut [0.0; 4])
    });
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_OBJECT, EE_SCREENS_SEOBX_FLAG]);
}

#[test]
fn unit_types_skip_projectiles_and_missiles() {
    // classes 1..4: 1 a projectile, 2 missing, 3 selectable, then the sort; two types kept
    let mut out = [0u32; 4];
    let mut x = |op: u32, _p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]| -> i32 {
        match op {
            EE_SCREENS_SEETX_CLASS_COUNT => 4,
            EE_SCREENS_SEETX_CLASS_EXISTS => (i[0] != 2) as i32,
            EE_SCREENS_SEETX_CLASS_CATEGORY => {
                if i[0] == 1 {
                    9
                } else {
                    0
                }
            }
            EE_SCREENS_SEETX_CLASS_SELECTABLE_AT => 1,
            EE_SCREENS_SEOBX_TYPES_COUNT => 2,
            EE_SCREENS_SEOBX_TYPES_AT => 30 + i[0],
            EE_SCREENS_SEOBX_TYPES_PUSH => {
                assert!(i[0] == 0 || i[0] == 3, "pushed {}", i[0]);
                0
            }
            _ => 0,
        }
    };
    let r = seob(
        &mut x,
        EE_SCREENS_SEOB_OP_UNIT_TYPES,
        [out.as_mut_ptr() as usize, 0, 0],
        &mut [1, 0, 0, 0],
        &mut [0.0; 4],
    );
    assert_eq!(r, Some(2));
    assert_eq!(out, [30, 0, 0, 0], "only i0 written");
}

#[test]
fn the_player_combo_answers_only_its_get() {
    let (r, fake) = with(&[(EE_SCREENS_SEETX_SHELL_OP, 5)], |x| {
        seob(x, EE_SCREENS_SEOB_OP_PLAYER_COMBO, [8, 0, 0], &mut [EE_SCREENS_SEOB_PC_GET, 9, 0, 0], &mut [0.0; 4])
    });
    assert_eq!(r, Some(5));
    assert_eq!(fake.seen[0].i[..2], [EE_SCREENS_SKOP_SELECTED_PLAYER as i32, 0]);
    let (r, fake) = with(&[(EE_SCREENS_SEETX_SHELL_OP, 5)], |x| {
        seob(x, EE_SCREENS_SEOB_OP_PLAYER_COMBO, [8, 0, 0], &mut [EE_SCREENS_SEOB_PC_FLAGS, 9, 0, 0], &mut [0.0; 4])
    });
    assert_eq!(r, Some(0));
    assert_eq!(fake.seen[0].i[..2], [EE_SCREENS_SKOP_SET_FLAGS as i32, 9]);
}

#[test]
fn the_view_press_tests_in_order() {
    let (r, fake) = with(&[], |x| seob(x, EE_SCREENS_SEOB_OP_VIEW_PRESS, [0; 3], &mut [0; 4], &mut [0.0; 4]));
    assert_eq!(r, Some(0));
    assert_eq!(fake.ops(), [EE_SCREENS_SEOBX_IN_VIEW]);
    let (r, fake) = with(&[(EE_SCREENS_SEOBX_IN_VIEW, 1), (EE_SCREENS_SEOBX_LBUTTON_JUST, 1)], |x| {
        seob(x, EE_SCREENS_SEOB_OP_VIEW_PRESS, [0; 3], &mut [0; 4], &mut [0.0; 4])
    });
    assert_eq!(r, Some(1));
    assert_eq!(fake.ops(), [EE_SCREENS_SEOBX_IN_VIEW, EE_SCREENS_SEOBX_LBUTTON_JUST]);
}

#[test]
fn the_select_units_controls() {
    let mut out = [0usize; 3];
    let (_, fake) = with(&[], |x| {
        seob(x, EE_SCREENS_SEOB_OP_SELECT_UNITS, [out.as_mut_ptr() as usize, 0, 66], &mut [12, 0, 0, 0], &mut [0.0; 4])
    });
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_SEOBX_NEW_CLICK,
            EE_SCREENS_SEOBX_NEW_LASSO,
            EE_SCREENS_SEOBX_NEW_SELECT,
            EE_SCREENS_SEOBX_ADD_CONTROL
        ]
    );
    assert_eq!(fake.seen[2].p0, 66);
    assert_eq!(fake.seen[2].p1, handle(EE_SCREENS_SEOBX_NEW_CLICK));
    assert_eq!(
        out,
        [handle(EE_SCREENS_SEOBX_NEW_CLICK), handle(EE_SCREENS_SEOBX_NEW_LASSO), handle(EE_SCREENS_SEOBX_NEW_SELECT)]
    );
}

#[test]
fn the_unit_list_keeps_the_last_found_name() {
    let mut ids = vec![5, 0];
    let mut appended = Vec::new();
    let mut x = |op: u32, p: [usize; 3], i: &mut [i32; 4], _f: &mut [f32; 4]| -> i32 {
        match op {
            EE_SCREENS_SEOBX_SELECTED_COUNT => 2,
            EE_SCREENS_SEOBX_SELECTED_NAME_ID => ids.remove(0),
            EE_SCREENS_TRGX_R_STRING => {
                if i[0] == 0 {
                    return -1;
                }
                // SAFETY: the buffer
                unsafe { core::ptr::copy_nonoverlapping(b"Pike".as_ptr(), p[2] as *mut u8, 4) };
                4
            }
            EE_SCREENS_TRGX_R_LIST_APPEND => {
                // SAFETY: the text
                appended.push(unsafe { core::slice::from_raw_parts(p[1] as *const u8, i[0] as usize) }.to_vec());
                0
            }
            _ => 0,
        }
    };
    let r = seob(&mut x, EE_SCREENS_SEOB_OP_UNIT_LIST_FILL, [1, 2, 0], &mut [0; 4], &mut [0.0; 4]);
    drop(x);
    assert_eq!(r, Some(2));
    assert_eq!(appended, [b"Pike".to_vec(), b"Pike".to_vec()]);
}

// ---- the trigger screen and the trigger tab

#[test]
fn trigger_values_and_wrappers() {
    let (r, fake) =
        with(&[(EE_SCREENS_SETRX_GET + 5, 3)], |x| setr(x, EE_SCREENS_SETR_OP_GET, [0; 3], &mut [1, 9, 0, 0]));
    assert_eq!(r, Some(3), "past the counts: the effects'");
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_TRIGGER, EE_SCREENS_SETRX_GET + 5]);
    let (_, fake) = with(&[], |x| setr(x, EE_SCREENS_SETR_OP_SET, [0; 3], &mut [1, 4, 1, 0]));
    assert_eq!(fake.ops(), [EE_SCREENS_SEETX_TRIGGER], "the counts are not set");
    let (r, fake) = with(&[(EE_SCREENS_SETRX_EFFECT_INDEX, 6)], |x| {
        setr(x, EE_SCREENS_SETR_OP_ENTRY, [0; 3], &mut [1, 1, 2, EE_SCREENS_SETR_W_VALUE])
    });
    assert_eq!(r, Some(6));
    assert_eq!(fake.seen[1].i[0], 2);
    let (r, _) = with(&[], |x| setr(x, EE_SCREENS_SETR_OP_ENTRY, [0; 3], &mut [1, 0, 2, 9]));
    assert_eq!(r, Some(1));
    let (r, _) =
        with(&[(EE_SCREENS_SETRX_CONDITION, 0)], |x| setr(x, EE_SCREENS_SETR_OP_ENTRY, [0; 3], &mut [1, 0, 2, 9]));
    assert_eq!(r, Some(0));
    let (r, _) =
        with(&[(EE_SCREENS_SEETX_TRIGGER, 0)], |x| setr(x, EE_SCREENS_SETR_OP_EXISTS, [0; 3], &mut [1, 0, 0, 0]));
    assert_eq!(r, Some(0));
    let (r, _) = with(&[], |x| setr(x, EE_SCREENS_SETR_OP_EXISTS, [0; 3], &mut [1, 0, 0, 0]));
    assert_eq!(r, Some(1));
    let (r, _) = with(&[], |x| setr(x, EE_SCREENS_SETR_OP_NEW_EVENT, [0; 3], &mut [0; 4]));
    assert_eq!(r, None);
}

#[test]
fn the_trigger_texts_go_both_ways() {
    let (_, fake) = with(&[], |x| setr(x, EE_SCREENS_SETR_OP_TEXTS_FROM_SCREEN, [0; 3], &mut [1, 0, 0, 0]));
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_SEETX_TRIGGER,
            EE_SCREENS_SEETX_TRIGGER_BOX,
            EE_SCREENS_TRGX_R_EDIT_LINE,
            EE_SCREENS_SETRX_SET_NAME,
            EE_SCREENS_SEETX_TRIGGER_BOX,
            EE_SCREENS_TRGX_R_EDIT_ALL_TEXT,
            EE_SCREENS_SETRX_SET_TEXT
        ]
    );
    assert_eq!(fake.seen[1].i[0], 0);
    assert_eq!(fake.seen[4].i[0], 1);
    assert_eq!(fake.seen[6].text, format!("t{EE_SCREENS_TRGX_R_EDIT_ALL_TEXT}").into_bytes());
}

#[test]
fn the_trigger_jump_checks_the_tab_silently() {
    // condition 0 not an object condition, 1 one: the object conditions' first is 1
    let (_, fake) = with(&[(EE_SCREENS_SETRX_CONDITION_COUNT, 3), (EE_SCREENS_SETRX_CONDITION_IS_OBJECT, 0)], |x| {
        trigger_jump(x, 0, 1);
        None
    });
    let ops = fake.ops();
    assert_eq!(ops[0], EE_SCREENS_SETRX_TRIGGER_TAB);
    assert_eq!(fake.of(EE_SCREENS_SETRX_CONDITION_IS_OBJECT).len(), 3);
    let Some((screen, entry)) = crate::screens::editor_trigger::trigger_jump(0, 1, 3, 3) else {
        assert_eq!(*ops.last().unwrap(), EE_SCREENS_SETRX_CONDITION_IS_OBJECT);
        return;
    };
    assert_eq!(
        ops[ops.len() - 8..],
        [
            EE_SCREENS_TRGX_R_SETS_CONTROL,
            EE_SCREENS_TRGX_R_TRIGGER_EVENT,
            EE_SCREENS_TRGX_R_EVENT_CALL,
            EE_SCREENS_TRGX_R_SET_TRIGGER,
            EE_SCREENS_SETRX_RADIO_CHECK,
            EE_SCREENS_TRGX_R_SET_TRIGGER,
            EE_SCREENS_TRGX_R_SETS_SUB_SELECT,
            EE_SCREENS_TRGX_R_SETS_CALL,
        ]
    );
    assert_eq!(fake.of(EE_SCREENS_TRGX_R_SETS_CALL)[0].i[..2], [8, 0x7d0 + screen as i32]);
    assert_eq!(fake.of(EE_SCREENS_TRGX_R_SETS_SUB_SELECT)[0].i[..2], [screen as i32, entry as u32 as i32]);
}

#[test]
fn the_tab_radio_is_made_in_the_cpp_order() {
    let mut out = 0usize;
    let (_, fake) = with(&[], |x| {
        sets(x, EE_SCREENS_SETS_OP_TAB, [&mut out as *mut usize as usize, 3, 0], &mut [7, 2, 0, 0], &mut [0.0; 4])
    });
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_TRGX_R_STRING,
            EE_SCREENS_SETSX_NEW_TAB,
            EE_SCREENS_SETSX_UP_CHECKED_NULL,
            EE_SCREENS_SETSX_DOWN_CHECKED_NULL,
            EE_SCREENS_SETSX_DOWN_UNCHECKED_NULL,
            EE_SCREENS_SETSX_UP_UNCHECKED_NULL,
            EE_SCREENS_SETSX_LABEL_RECT,
            EE_SCREENS_SETSX_BG_OPACITY,
            EE_SCREENS_SETSX_SCREEN_EVENT,
            EE_SCREENS_SETSX_SET_TRIGGER,
            EE_SCREENS_SETSX_EVENT_RELEASE,
            EE_SCREENS_SETSX_RADIO_GETEXT,
            EE_SCREENS_SEETX_YELLOW,
            EE_SCREENS_SEETX_TRANSPARENT,
            EE_SCREENS_SETSX_ADD_CONTROL
        ]
    );
    assert_eq!(fake.seen[0].i[0], 30299 + 2);
    assert_eq!(fake.seen[1].i[..3], [2, 7, 6]);
    assert_eq!(fake.seen[9].i[0], 0x3e9);
    assert_eq!(out, handle(EE_SCREENS_SETSX_NEW_TAB));
}

#[test]
fn the_views_leave_and_enter() {
    let (_, fake) = with(&[], |x| sets(x, EE_SCREENS_SETS_OP_VIEW, [1, 0, 0], &mut [0; 4], &mut [0.0; 4]));
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_SETSX_EDITOR,
            EE_SCREENS_SETSX_CONTROL_BY_ID,
            EE_SCREENS_SETSX_SET_DISPLAY,
            EE_SCREENS_SETSX_SET_GAME_VIEW,
            EE_SCREENS_SETSX_CLEAR_529066,
            EE_SCREENS_SETSX_CLEAR_FOLLOWING,
            EE_SCREENS_SETSX_CLEAR_529068,
            EE_SCREENS_SETSX_MINIMAP,
            EE_SCREENS_SETSX_REDRAW_ALL,
            EE_SCREENS_SETSX_ALL_DIRTY
        ]
    );
    assert_eq!(fake.seen[1].i[0], 0x12);
    assert_eq!(fake.seen[2].i[0], 0, "hidden");
    let (_, fake) = with(&[(EE_SCREENS_SETSX_MINIMAP, 1)], |x| {
        sets(x, EE_SCREENS_SETS_OP_VIEW, [1, 0, 0], &mut [1, 0, 0, 0], &mut [0.0; 4])
    });
    let ops = fake.ops();
    assert_eq!(
        ops[..9],
        [
            EE_SCREENS_SETSX_EDITOR,
            EE_SCREENS_SETSX_CONTROL_BY_ID,
            EE_SCREENS_SETSX_GAME_VIEW_RECT,
            EE_SCREENS_SETSX_SET_DISPLAY,
            EE_SCREENS_SETSX_HIDE_UI_VIEWS,
            EE_SCREENS_SETSX_SET_GAME_VIEW,
            EE_SCREENS_SETSX_CONTROL_BY_ID,
            EE_SCREENS_SETSX_SET_CHECKED,
            EE_SCREENS_SETSX_GROUP_ENABLE
        ]
    );
    assert!(ops.contains(&EE_SCREENS_SETSX_MINIMAP_REFRESH));
    assert_eq!(fake.seen[3].i[0], 1, "shown");
    assert_eq!(fake.seen[8].i[0], 14);
}

#[test]
fn the_panel_draws_only_when_shown_and_met() {
    let (_, fake) = with(&[(EE_SCREENS_SETSX_WAS_UI_HIDDEN, 1)], |x| {
        sets(x, EE_SCREENS_SETS_OP_DRAW, [0, 5, 0], &mut [0; 4], &mut [0.0; 4])
    });
    assert_eq!(fake.ops(), [EE_SCREENS_SETSX_WAS_UI_HIDDEN]);
    let (_, fake) = with(&[(EE_SCREENS_SETSX_CLIP_PANEL, 1)], |x| {
        sets(x, EE_SCREENS_SETS_OP_DRAW, [0, 5, 0], &mut [0; 4], &mut [0.0; 4])
    });
    assert_eq!(fake.ops(), [EE_SCREENS_SETSX_WAS_UI_HIDDEN, EE_SCREENS_SETSX_CLIP_PANEL, EE_SCREENS_SETSX_DRAW_PANEL]);
    assert_eq!(fake.seen[1].p0, 5);
}

#[test]
fn the_names_of_a_sub_screen() {
    for (screen, n) in [(0, 0), (3, 3), (4, 4), (9, 4)] {
        let (_, fake) =
            with(&[], |x| sets(x, EE_SCREENS_SETS_OP_NAMES, [1, 2, 0], &mut [screen, 0, 0, 0], &mut [0.0; 4]));
        assert_eq!(
            fake.ops(),
            [EE_SCREENS_SETSX_NAMES_CLEAR, EE_SCREENS_SETSX_SUB_NAMES + n, EE_SCREENS_SETSX_LIST_APPEND_NAMES]
        );
    }
}

// ---- the area screen

#[test]
fn the_area_loads_into_the_screen() {
    let mut i = [2, 0, 0, 0];
    let mut f = [0.0f32; 4];
    let (r, fake) =
        with(&[(EE_SCREENS_SEARX_ANCHOR_X, 5), (EE_SCREENS_SEARX_HAS, 3), (EE_SCREENS_SEARX_SELECT_MODE, 1)], |x| {
            sear(x, EE_SCREENS_SEAR_OP_LOAD, [9, 0, 0], &mut i, &mut f)
        });
    assert_eq!(r, Some(1));
    assert_eq!(f, [0.0, 0.0, 7.5, 0.0]);
    assert_eq!(i[1..], [5, 0, 3]);
    let ops = fake.ops();
    assert_eq!(
        ops[..4],
        [EE_SCREENS_SEARX_AREA, EE_SCREENS_SEETX_TRIGGER_CALL, EE_SCREENS_SEETX_TRIGGER_BOX, EE_SCREENS_SEARX_NAME]
    );
    let select = fake.of(EE_SCREENS_TRGX_R_COMBO_SELECT);
    assert_eq!((select[0].p0, select[0].i[0]), (9, 1));
    assert_eq!(*ops.last().unwrap(), EE_SCREENS_TRGX_R_COMBO_SELECT);
    let (r, fake) = with(&[(EE_SCREENS_SEARX_AREA, 0)], |x| {
        sear(x, EE_SCREENS_SEAR_OP_LOAD, [9, 0, 0], &mut [2, 0, 0, 0], &mut [0.0; 4])
    });
    assert_eq!((r, fake.ops()), (Some(0), vec![EE_SCREENS_SEARX_AREA]));
}

#[test]
fn the_pick_and_the_ground_reset_their_coordinates() {
    let mut i = [1, 0, 0, 0];
    let (r, fake) = with(&[(EE_SCREENS_SEARX_PICK, 1), (EE_SCREENS_SEARX_X, 4), (EE_SCREENS_SEARX_Y, 6)], |x| {
        sear(x, EE_SCREENS_SEAR_OP_PICK, [0; 3], &mut i, &mut [0.0; 4])
    });
    assert_eq!((r, i[1], i[2]), (Some(1), 4, 6));
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_SEARX_XY_RESET,
            EE_SCREENS_SEARX_POINT,
            EE_SCREENS_SEARX_PICK,
            EE_SCREENS_SEARX_X,
            EE_SCREENS_SEARX_Y
        ]
    );
    let (_, fake) = with(&[], |x| sear(x, EE_SCREENS_SEAR_OP_GROUND, [0; 3], &mut [0; 4], &mut [0.0; 4]));
    assert_eq!(fake.of(EE_SCREENS_SEARX_SOLVE)[0].p0, handle(EE_SCREENS_SEARX_TILE_AT));
    let (r, _) = with(&[(EE_SCREENS_SEARX_LBUTTON_DOWN, 1), (EE_SCREENS_SEARX_LBUTTON_UNPRESSED, 1)], |x| {
        sear(x, EE_SCREENS_SEAR_OP_LBUTTON, [0; 3], &mut [0; 4], &mut [0.0; 4])
    });
    assert_eq!(r, Some(5));
}

#[test]
fn the_paint_ends_with_its_area() {
    let mut f = [0.0f32; 4];
    let (_, fake) = with(&[], |x| sear(x, EE_SCREENS_SEAR_OP_PAINT_END, [3, 0, 0], &mut [0; 4], &mut f));
    assert_eq!(f, [1.0, 2.0, 3.0, 4.0]);
    assert_eq!(
        fake.ops(),
        [
            EE_SCREENS_SEARX_BOUNDS_WINDOW,
            EE_SCREENS_SEARX_EDGES_WINDOW,
            EE_SCREENS_SEARX_PAINT_AREA,
            EE_SCREENS_SEARX_PAINT_RELEASE
        ]
    );
}

// ---- the instructions tab

#[test]
fn the_instructions_load_selects_the_movie_twice() {
    let controls = [10usize, 11, 12, 13, 14, 15, 16, 17];
    let (_, fake) = with(&[(EE_SCREENS_SEINX_FILE_SELECT, 1), (EE_SCREENS_SEINX_TEXT_LENGTH, 0)], |x| {
        sein(x, EE_SCREENS_SEIN_OP_LOAD, [controls.as_ptr() as usize, 0, 0], &mut [0; 4], &mut [0.0; 4])
    });
    let selects: Vec<usize> = fake.of(EE_SCREENS_SEINX_FILE_SELECT).iter().map(|s| s.p0).collect();
    assert_eq!(selects, [10, 11, 11, 12]);
    assert_eq!(fake.of(EE_SCREENS_SEINX_LABEL_TEXT).len(), 0, "empty texts");
    let edits: Vec<(usize, i32)> = fake.of(EE_SCREENS_SEINX_EDIT_MEDIA).iter().map(|s| (s.p0, s.i[0])).collect();
    assert_eq!(edits, [(13, 3), (14, 4), (15, 5), (16, 6)]);
    assert_eq!(fake.of(EE_SCREENS_SEINX_EDIT_EMPTY)[0].p0, 17);
    let (_, fake) = with(&[(EE_SCREENS_SEINX_TEXT_LENGTH, 9), (EE_SCREENS_SEINX_FILE_OPEN, 1)], |x| {
        sein(x, EE_SCREENS_SEIN_OP_LOAD, [controls.as_ptr() as usize, 0, 0], &mut [0; 4], &mut [0.0; 4])
    });
    let ops = fake.ops();
    assert_eq!(
        ops[ops.len() - 7..],
        [
            EE_SCREENS_SEINX_FILE_VOLUME,
            EE_SCREENS_SEINX_FILE_DIRECTORY,
            EE_SCREENS_SEINX_FILE_NAME,
            EE_SCREENS_SEINX_FILE_OPEN,
            EE_SCREENS_SEINX_NAME_STEM,
            EE_SCREENS_SEINX_EDIT_STEM,
            EE_SCREENS_SEINX_FILE_CLOSE,
        ]
    );
}

#[test]
fn the_instructions_store_and_paths() {
    let controls = [10usize, 11, 12, 13, 14, 15, 16, 17];
    let (_, fake) = with(&[], |x| {
        sein(x, EE_SCREENS_SEIN_OP_STORE, [controls.as_ptr() as usize, 0, 0], &mut [0; 4], &mut [0.0; 4])
    });
    let ops = fake.ops();
    assert_eq!(ops[..3], [EE_SCREENS_SEINX_SET_MEDIA_LABEL; 3]);
    assert_eq!(ops[3..5], [EE_SCREENS_SEINX_EDIT_ALL_TEXT, EE_SCREENS_SEINX_SET_MEDIA]);
    assert_eq!(fake.seen[10].i[0], EE_SCREENS_SEINX_M_HISTORY);
    let mut paths = [0usize; 3];
    let (_, fake) = with(&[(EE_SCREENS_SEINX_IS_DIRECTORY, 0)], |x| {
        sein(x, EE_SCREENS_SEIN_OP_PATHS, [paths.as_mut_ptr() as usize, 0, 0], &mut [0; 4], &mut [0.0; 4])
    });
    assert_eq!(paths, [handle(EE_SCREENS_SEINX_NEW_PATH); 3]);
    assert_eq!(fake.of(EE_SCREENS_SEINX_CREATE_DIRECTORY).len(), 3);
    assert_eq!(fake.of(EE_SCREENS_SEINX_DIR_RESET).len(), 3);
}
