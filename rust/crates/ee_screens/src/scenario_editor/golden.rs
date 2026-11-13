//! The trigger tab's events against the real C++: a scratch driver (`build/sc14/se_driver.cpp`) ran the unmodified
//! `EESEEvents.cpp`'s event classes on a fake editor form (its trigger tab and five screens with probe vtables, the
//! form's controls, the editor's selected units, the scenario manager), each case drawing the event, its control (id,
//! check box, string list or combo box selection), the map flag's answer and the area's centre, and printing every
//! call out of the unit and the state after; the driver relinked over the new unit (`EEScreenEvent`s over this module
//! and `ee_screenscpp_se`) printed the same on seeds 1-4 (3,000 cases each). Its host log is
//! `golden_scenario_editor.txt` (seed 1): per case `case N code id checked units manager-is-form map centre`, then
//! the host calls `op a b answer`. The replay dispatches each case's code over a host that checks every call against
//! the log and gives its answer.

use super::*;

const GOLDEN: &str = include_str!("../golden_scenario_editor.txt");

struct Replay<'a> {
    calls: std::slice::Iter<'a, [u32; 4]>,
    case: &'a str,
}

impl SeHost for Replay<'_> {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        let want = self.calls.next().unwrap_or_else(|| panic!("{}: an extra call {op} {a} {b}", self.case));
        assert_eq!([op, a, b], want[..3], "{}", self.case);
        want[3]
    }
}

/// The log as (header, calls) blocks.
fn blocks() -> Vec<(&'static str, Vec<[u32; 4]>)> {
    let mut out: Vec<(&'static str, Vec<[u32; 4]>)> = Vec::new();
    for line in GOLDEN.lines() {
        if let Some(call) = line.strip_prefix(' ') {
            let v: Vec<u32> = call.split(' ').map(|x| x.parse().unwrap()).collect();
            out.last_mut().unwrap().1.push([v[0], v[1], v[2], v[3]]);
        } else {
            out.push((line, Vec::new()));
        }
    }
    out
}

#[test]
fn replays_the_golden() {
    let blocks = blocks();
    let mut seen = [0u32; 23];
    for (header, expected) in &blocks {
        let code: u32 = header.split(' ').nth(2).unwrap().parse().unwrap();
        let mut h = Replay { calls: expected.iter(), case: header };
        assert_eq!(dispatch(&mut h, code), Some(0), "{header}");
        assert!(h.calls.next().is_none(), "{header}: calls left");
        seen[(code - EE_SCREENS_SE_SHOW_TAB) as usize] += 1;
    }
    assert_eq!(blocks.len(), 3000);
    assert!(seen.iter().all(|&n| n > 80), "{seen:?}");
}

/// Every branch of the events with decisions reached by the golden.
#[test]
fn the_golden_reaches_every_branch() {
    let blocks = blocks();
    let has = |code: u32, op: u32, a: Option<u32>, b: Option<u32>| {
        blocks.iter().any(|(header, calls)| {
            header.split(' ').nth(2).unwrap().parse::<u32>().unwrap() == code
                && calls.iter().any(|c| c[0] == op && a.is_none_or(|a| c[1] == a) && b.is_none_or(|b| c[2] == b))
        })
    };
    assert!(has(EE_SCREENS_SE_OBJECT_DETAIL, EE_SCREENS_SE_OP_OBJECT_CLEAR_IDS, None, None));
    assert!(has(EE_SCREENS_SE_OBJECT_DETAIL, EE_SCREENS_SE_OP_OBJECT_SET_MAP, Some(1), None));
    assert!(has(EE_SCREENS_SE_OBJECT_UNIT_SELECT, EE_SCREENS_SE_OP_CENTER_ON_UNIT, None, None));
    assert!(has(EE_SCREENS_SE_AREA_CHECK, EE_SCREENS_SE_OP_AREA_MODE, Some(1), None));
    assert!(has(EE_SCREENS_SE_AREA_CHECK, EE_SCREENS_SE_OP_AREA_MODE, Some(0), None));
    assert!(has(EE_SCREENS_SE_CENTER_CAMERA_ON_AREA, EE_SCREENS_SE_OP_CENTER_ON_AREA, None, None));
    assert!(has(EE_SCREENS_SE_CONDITION_PLAYER_RESOURCE, EE_SCREENS_SE_OP_SHOW, Some(SE_CONDITION_AMOUNT), Some(1)));
    assert!(has(EE_SCREENS_SE_CONDITION_PLAYER_RESOURCE, EE_SCREENS_SE_OP_SHOW, Some(SE_CONDITION_AMOUNT), Some(0)));
    assert!(has(EE_SCREENS_SE_TRIGGER_CONDITION_TIMER, EE_SCREENS_SE_OP_SHOW, None, Some(1)));
    assert!(has(EE_SCREENS_SE_TRIGGER_CONDITION_TIMER, EE_SCREENS_SE_OP_SHOW, None, Some(0)));
    for step in 0..4 {
        assert!(has(EE_SCREENS_SE_TRIGGER_CHANGE_INDEX, EE_SCREENS_SE_OP_TRIGGER_STEP, Some(step), None));
    }
}

/// A host that answers from a table and records the calls.
#[derive(Default)]
struct Record {
    calls: Vec<[u32; 3]>,
    id: u32,
    selection: u32,
    units: u32,
}

impl SeHost for Record {
    fn call(&mut self, op: u32, a: u32, b: u32) -> u32 {
        self.calls.push([op, a, b]);
        match op {
            EE_SCREENS_SE_OP_ENTITY_ID => self.id,
            EE_SCREENS_SE_OP_LIST_SELECTION | EE_SCREENS_SE_OP_COMBO_SELECTION => self.selection,
            EE_SCREENS_SE_OP_SELECTED_UNIT_COUNT => self.units,
            _ => 0,
        }
    }
}

#[test]
fn a_unit_list_pick_past_the_selection_centres_nothing() {
    let mut h = Record { selection: 2, units: 2, ..Default::default() };
    dispatch(&mut h, EE_SCREENS_SE_OBJECT_UNIT_SELECT);
    assert!(!h.calls.iter().any(|c| c[0] == EE_SCREENS_SE_OP_CENTER_ON_UNIT));
    let mut h = Record { selection: 1, units: 2, ..Default::default() };
    dispatch(&mut h, EE_SCREENS_SE_OBJECT_UNIT_SELECT);
    assert_eq!(h.calls.last(), Some(&[EE_SCREENS_SE_OP_CENTER_ON_UNIT, 1, 0]));
}

#[test]
fn a_combo_box_without_a_selection_leaves_rectangle_mode() {
    let mut h = Record { selection: u32::MAX, ..Default::default() };
    dispatch(&mut h, EE_SCREENS_SE_AREA_CHECK);
    assert_eq!(h.calls.last(), Some(&[EE_SCREENS_SE_OP_AREA_MODE, 0, 0]));
}

#[test]
fn another_forms_code_is_not_the_editors() {
    let mut h = Record::default();
    assert_eq!(dispatch(&mut h, 0x2418), None);
    assert_eq!(dispatch(&mut h, 0x2400), None);
    assert!(h.calls.is_empty());
}
