//! The shared maker's composed ops as statement streams.

use std::collections::HashMap;

use super::*;

/// The statements run (op, p0, i0..i3) over a table of answers by op (0 otherwise); a making statement answers the
/// handle 0x1_0000_0002.
/// A statement as the tests see it: its op, p0 and integers.
type Seen = (u32, usize, [i32; 4]);

fn run(op: u32, p: [usize; 3], i: [i32; 4], answers: &[(u32, i32)]) -> (Option<i32>, [i32; 4], Vec<Seen>) {
    let table: HashMap<u32, i32> = answers.iter().copied().collect();
    let mut seen = Vec::new();
    let mut x = |op: u32, p: [usize; 3], a: &mut [i32; 4], f: &mut [f32; 4]| {
        seen.push((op, p[0], *a));
        if matches!(
            op,
            EE_SCREENS_SEPX_EVENT_START_EPOCH
                | EE_SCREENS_SEPX_EVENT_DISPLAY_EPOCH
                | EE_SCREENS_SEPX_EVENT_SCREEN
                | EE_SCREENS_SEPX_COUNT_ACTION
        ) {
            a[0] = 2;
            a[1] = 1;
        }
        if op == EE_SCREENS_SEPX_CITIZENS {
            f[0] = 0.25;
        }
        *table.get(&op).unwrap_or(&0)
    };
    let mut i = i;
    let mut f = [0.0f32; 4];
    let r = compose(&mut x, op, p, &mut i, &mut f);
    (r, i, seen)
}

fn ops(seen: &[Seen]) -> Vec<u32> {
    seen.iter().map(|s| s.0).collect()
}

#[test]
fn an_op_the_cpp_answers_is_not_composed() {
    assert_eq!(run(EE_SCREENS_SEPL_OP_DELETE, [0; 3], [0; 4], &[]).0, None);
}

#[test]
fn a_world_value_is_read_and_set_by_its_number() {
    let (r, _, seen) = run(EE_SCREENS_SEPL_OP_WORLD_GET, [0; 3], [EE_SCREENS_SEPL_WORLD_TEAMS, 0, 0, 0], &[(4, 1)]);
    assert_eq!((r, ops(&seen)), (Some(1), vec![EE_SCREENS_SEPX_WORLD_GET + 4]));
    let (_, _, seen) = run(EE_SCREENS_SEPL_OP_WORLD_SET, [0; 3], [EE_SCREENS_SEPL_WORLD_TEAMS, 7, 0, 0], &[]);
    assert_eq!(seen, vec![(EE_SCREENS_SEPX_WORLD_SET + 4, 0, [7, 0, 0, 0])]);
}

#[test]
fn a_player_value_is_set_then_read_back() {
    let i = [2, EE_SCREENS_SEPL_PV_AI, 3, 1];
    let (_, _, seen) = run(EE_SCREENS_SEPL_OP_PLAYER_SET, [0; 3], i, &[]);
    assert_eq!(ops(&seen), vec![EE_SCREENS_SEPX_AI_SET + 3, EE_SCREENS_SEPX_AI_GET + 3]);
    let (_, _, seen) = run(EE_SCREENS_SEPL_OP_PLAYER_GET, [0; 3], i, &[]);
    assert_eq!(ops(&seen), vec![EE_SCREENS_SEPX_AI_GET + 3]);
    let (r, _, seen) = run(EE_SCREENS_SEPL_OP_PLAYER_GET, [0; 3], [2, EE_SCREENS_SEPL_PV_END_EPOCH, 0, 0], &[(43, 9)]);
    assert_eq!((r, seen.len()), (Some(9), 1));
}

#[test]
fn the_display_epoch_event_goes_on_both_triggers_and_is_released() {
    let (_, _, seen) =
        run(EE_SCREENS_SEPL_OP_EVENT, [5, 0, 0], [0x3e9, EE_SCREENS_SEPL_EVENT_DISPLAY_EPOCH, 0, 0], &[]);
    assert_eq!(
        ops(&seen),
        vec![
            EE_SCREENS_SEPX_EVENT_DISPLAY_EPOCH,
            EE_SCREENS_SEPX_SET_TRIGGER,
            EE_SCREENS_SEPX_SET_TRIGGER,
            EE_SCREENS_SEPX_EVENT_RELEASE
        ]
    );
    assert_eq!(seen[1].2[0], 0x3ea);
    assert_eq!(seen[2].2[0], 0x3e9);
}

#[test]
fn a_screen_event_goes_on_each_trigger_its_flags_name() {
    let flags = EE_SCREENS_SEPL_ON_SELECTION | EE_SCREENS_SEPL_ON_ENTER;
    let (_, _, seen) = run(EE_SCREENS_SEPL_OP_SCREEN_EVENT, [5, 0, 0], [3, flags, 0, 0], &[]);
    let triggers: Vec<i32> = seen.iter().filter(|s| s.0 == EE_SCREENS_SEPX_SET_TRIGGER).map(|s| s.2[0]).collect();
    assert_eq!(triggers, vec![0x3eb, 0x3f1]);
}

#[test]
fn the_tech_types_sort_by_class() {
    // every node: class 4, a land building
    let (_, _, seen) = run(
        EE_SCREENS_SEPL_OP_TECH_TYPES,
        [0; 3],
        [0; 4],
        &[
            (EE_SCREENS_SEPX_TT_COUNT, 2),
            (EE_SCREENS_SEPX_TT_CLASS, 4),
            (EE_SCREENS_SEPX_TT_CLASS_EXISTS, 1),
            (EE_SCREENS_SEPX_TT_LAND_BUILDING, 1),
        ],
    );
    let pushes: Vec<[i32; 4]> = seen.iter().filter(|s| s.0 == EE_SCREENS_SEPX_TT_PUSH).map(|s| s.2).collect();
    assert_eq!(pushes, vec![[0, TT_BUILDINGS, 0, 0], [1, TT_BUILDINGS, 0, 0]]);
    let (_, _, seen) = run(
        EE_SCREENS_SEPL_OP_TECH_TYPES,
        [0; 3],
        [0; 4],
        &[(EE_SCREENS_SEPX_TT_COUNT, 1), (EE_SCREENS_SEPX_TT_CLASS, -1)],
    );
    assert!(!ops(&seen).contains(&EE_SCREENS_SEPX_TT_CLASS_EXISTS));
    assert_eq!(seen.last().unwrap().2, [0, TT_TECHNOLOGIES, 0, 0]);
}

#[test]
fn a_fill_puts_the_obsoleted_techs_on_the_other_list() {
    let answers = [
        (EE_SCREENS_SEPL_OP_COMBO_SELECTION_RAW, 1),
        (EE_SCREENS_SEPX_FT_COUNT, 1),
        (EE_SCREENS_SEPX_FT_SHOWN, 1),
        (EE_SCREENS_SEPX_FT_OBSOLETED, 1),
    ];
    let (_, _, seen) = run(EE_SCREENS_SEPL_OP_FILL_TECHS, [10, 11, 12], [2, 0, 0, 0], &answers);
    let add = seen.iter().find(|s| s.0 == EE_SCREENS_SEPX_TECH_ADD).unwrap();
    assert_eq!((add.1, add.2), (11, [2, 0, 0, 0]));
    let shown = seen.iter().find(|s| s.0 == EE_SCREENS_SEPX_FT_SHOWN).unwrap();
    assert_eq!(shown.2[1], 2); // the selection's player
}

#[test]
fn the_unit_limits_come_back_in_the_integers() {
    let (_, i, _) = run(
        EE_SCREENS_SEPL_OP_UNIT_LIMIT_RULES,
        [0; 3],
        [0; 4],
        &[
            (EE_SCREENS_SEPX_UNIT_LIMIT_MIN, 1),
            (EE_SCREENS_SEPX_UNIT_LIMIT_MAX, 9),
            (EE_SCREENS_SEPX_UNIT_LIMIT_DEFAULT, 5),
        ],
    );
    assert_eq!(i[..3], [1, 9, 5]);
}

#[test]
fn a_made_label_is_set_up_hidden_and_grouped_in_order() {
    let mut c = EeScreensSeplCtl::new(EE_SCREENS_SEPL_LABEL, 7, 10.0, 20.0, 100.0, 15.0);
    c.align = 1;
    c.bg = 0.5;
    c.flags = EE_SCREENS_SEPL_FG | crate::screens::editor_tscreen::EE_SCREENS_SEPL_HIDE;
    c.groups = 0b101;
    let mut out = 0usize;
    let mut seen = Vec::new();
    let mut x = |op: u32, p: [usize; 3], a: &mut [i32; 4], f: &mut [f32; 4]| {
        seen.push((op, p[2], *a, *f));
        if op == EE_SCREENS_SEPX_MAKE_LABEL {
            a[0] = 0x40;
        }
        0
    };
    let p = [&mut out as *mut usize as usize, &c as *const EeScreensSeplCtl as usize, 0x99];
    assert_eq!(compose(&mut x, EE_SCREENS_SEPL_OP_MAKE, p, &mut [0; 4], &mut [0.0; 4]), Some(0));
    assert_eq!(out, 0x40);
    let ops: Vec<u32> = seen.iter().map(|s| s.0).collect();
    assert_eq!(
        ops,
        vec![
            EE_SCREENS_SEPX_MAKE_LABEL,
            EE_SCREENS_SEPX_ALIGN,
            EE_SCREENS_SEPX_BG,
            EE_SCREENS_SEPX_COLOR_LABEL,
            EE_SCREENS_SEPX_HIDE,
            EE_SCREENS_SEPX_ADD_CONTROL,
            EE_SCREENS_SEPX_ADD_CONTROL
        ]
    );
    // the rectangle: left, top, left + width - 1, top + height - 1
    assert_eq!(seen[0].3, [10.0, 20.0, 109.0, 34.0]);
    assert_eq!((seen[5].2[0], seen[6].2[0]), (0, 2));
    assert_eq!(seen[1].1, 0x40);
}
