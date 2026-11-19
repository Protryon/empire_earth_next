use super::*;
use core::cell::RefCell;

/// One recorded call: the op, its words and floats.
type Call = (u32, i64, i64, [f32; 4]);

/// A host that records every call and answers from a table (op -> answer; 0 otherwise).
struct Rec {
    calls: RefCell<Vec<Call>>,
    answers: Vec<(u32, i64)>,
}

impl Rec {
    fn new(answers: &[(u32, i64)]) -> Self {
        Rec { calls: RefCell::new(Vec::new()), answers: answers.to_vec() }
    }
    fn ops(&self) -> Vec<(u32, i64, i64)> {
        self.calls.borrow().iter().map(|&(o, a, b, _)| (o, a, b)).collect()
    }
}

impl ShellHost for Rec {
    fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        self.calls.borrow_mut().push((op, a, b, *f));
        self.answers.iter().find(|&&(o, _)| o == op).map_or(0, |&(_, r)| r)
    }
}

fn ctx(h: &Rec) -> Ctx<'_> {
    Ctx { ctl: core::ptr::null_mut(), host: h }
}

const BASE: u32 = EE_SCREENS_SH_BASE;

#[test]
fn every_kind_is_made_with_its_class_name() {
    let names = [
        (EE_SCREENS_SK_COLOR, "EEUIBColor"),
        (EE_SCREENS_SK_SET_PLAYERS, "EESEPSSetPlayers"),
        (EE_SCREENS_SK_BRUSH, "EEUIFSERBBrush"),
        (EE_SCREENS_SK_CONDITION_COMBO, "EESETSCComboBox"),
        (EE_SCREENS_SK_EFFECT_COMBO, "EESETSEComboBox"),
        (EE_SCREENS_SK_SCENARIO_COMBO, "EESETSEScenarioComboBox"),
        (EE_SCREENS_SK_TRIGGER_COMBO, "EESETSTriggerUIComboBox"),
        (EE_SCREENS_SK_OBJECT_COMBO, "EESETSAComboBox"),
        (EE_SCREENS_SK_PLAYER_COMBO, "EESETSUICBPlayer"),
        (EE_SCREENS_SK_ERROR_LOG, "EEUIFMGErrorLog"),
        (EE_SCREENS_SK_ELEVATION_SLIDER, "EEUIFSEESElevation"),
        (EE_SCREENS_SK_OBJECT_EDIT, "EESETSAEditBox"),
        (EE_SCREENS_SK_UNIT_LIST, "EESETSOUnitList"),
    ];
    for (k, n) in names {
        assert_eq!(make_kind(k, &[]).unwrap().name().to_str().unwrap(), n);
    }
    assert!(make_kind(0, &[]).is_none());
}

#[test]
fn the_colour_button_steps_and_wraps() {
    let h = Rec::new(&[]);
    let k = make_kind(EE_SCREENS_SK_COLOR, &[15]).unwrap();
    k.init(&ctx(&h));
    assert_eq!(h.ops(), vec![(EE_SCREENS_SH_BG_PLAYER_COLOR, 15, 0), (EE_SCREENS_SH_BG_OPACITY, 0, 0)]);
    // left: 16 wraps to 0, then the base's release
    h.calls.borrow_mut().clear();
    k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_L_UP, 0, 0, &mut [0.0; 4]);
    assert_eq!(h.ops(), vec![(EE_SCREENS_SH_BG_PLAYER_COLOR, 0, 0), (BASE + EE_SCREENS_SV_MOUSE_L_UP, 0, 0)]);
    let mut f = [0.0; 4];
    assert_eq!(k.op(&ctx(&h), EE_SCREENS_SKOP_COLOR, 0, 0, &mut f), 0);
    // right: one below 0 wraps to 15; the release returns 0
    h.calls.borrow_mut().clear();
    assert_eq!(k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_R_UP, 0, 0, &mut f), 0);
    assert_eq!(k.op(&ctx(&h), EE_SCREENS_SKOP_COLOR, 0, 0, &mut f), 15);
    // a right press is the base's left press
    h.calls.borrow_mut().clear();
    k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_R_DOWN, 0, 0, &mut f);
    assert_eq!(h.ops(), vec![(BASE + EE_SCREENS_SV_MOUSE_L_DOWN, 0, 0)]);
    k.op(&ctx(&h), EE_SCREENS_SKOP_SET_COLOR, 7, 0, &mut f);
    assert_eq!(k.op(&ctx(&h), EE_SCREENS_SKOP_COLOR, 0, 0, &mut f), 7);
}

#[test]
fn the_player_list_maps_lines_and_players() {
    let k = PlayerCombo { flags: Cell::new(0), count: Cell::new(0) };
    // the world, any human, then 4 players
    k.flags.set(0x1 | 0x2 | 0x80);
    k.count.set(4);
    assert_eq!(k.player_of(0), 0); // the world
    assert_eq!(k.player_of(1), 1); // any human
    assert_eq!(k.player_of(2), 7); // player 1 (players start at 7)
    assert_eq!(k.player_of(5), 10);
    assert_eq!(k.player_of(u32::MAX), 0);
    assert_eq!(k.line_of(0), 0);
    assert_eq!(k.line_of(1), 1);
    assert_eq!(k.line_of(7), 2);
    assert_eq!(k.line_of(10), 5);
    // the fill: the flagged strings, then the numbered players
    let h = Rec::new(&[]);
    k.fill(&ctx(&h));
    assert_eq!(
        h.ops(),
        vec![
            (EE_SCREENS_SH_DELETE_ALL, 0, 0),
            (EE_SCREENS_SH_APPEND_STRING, 29_050, 0),
            (EE_SCREENS_SH_APPEND_STRING, 29_051, 0),
            (EE_SCREENS_SH_APPEND_NUMBERED, IDS_TRIGGER_PLAYER, 4),
        ]
    );
}

#[test]
fn the_scenario_combo_refreshes_its_screen_and_jumps() {
    let h = Rec::new(&[(EE_SCREENS_SH_SELECTION_INDEX, 3)]);
    let k = make_kind(EE_SCREENS_SK_SCENARIO_COMBO, &[2, 3]).unwrap();
    let mut f = [0.0; 4];
    k.virt(&ctx(&h), EE_SCREENS_SV_CHANGE_SELECTION, 0, 0, &mut f);
    k.op(&ctx(&h), EE_SCREENS_SKOP_SET_MODE, 4, 0, &mut f);
    k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_R_UP, 0, 0, &mut f);
    assert_eq!(
        h.ops(),
        vec![
            (BASE + EE_SCREENS_SV_CHANGE_SELECTION, 0, 0),
            (EE_SCREENS_SH_TRIGGER_REFRESH, 2, 0),
            (EE_SCREENS_SH_SELECTION_INDEX, 0, 0),
            (EE_SCREENS_SH_TRIGGER_JUMP, 4, 3),
        ]
    );
    // a screen out of 0..4 refreshes nothing
    let h = Rec::new(&[]);
    make_kind(EE_SCREENS_SK_SCENARIO_COMBO, &[-1, 3]).unwrap().virt(
        &ctx(&h),
        EE_SCREENS_SV_CHANGE_SELECTION,
        0,
        0,
        &mut f,
    );
    assert_eq!(h.ops(), vec![(BASE + EE_SCREENS_SV_CHANGE_SELECTION, 0, 0)]);
}

#[test]
fn the_unit_list_always_fires_and_the_elevation_slider_shows_its_level() {
    let h = Rec::new(&[]);
    let mut f = [0.0; 4];
    make_kind(EE_SCREENS_SK_UNIT_LIST, &[]).unwrap().virt(&ctx(&h), EE_SCREENS_SV_CHANGE_SELECTION_INDEX, 5, 3, &mut f);
    assert_eq!(h.ops(), vec![(BASE + EE_SCREENS_SV_CHANGE_SELECTION_INDEX, 5, 2), (EE_SCREENS_SH_FIRE, 0x3eb, 0)]);
    let h = Rec::new(&[(EE_SCREENS_SH_SLIDER_VALUE, 10), (EE_SCREENS_SH_SLIDER_MAX, 300)]);
    make_kind(EE_SCREENS_SK_ELEVATION_SLIDER, &[]).unwrap().virt(&ctx(&h), EE_SCREENS_SV_CHANGE_RANGE, 20, 0, &mut f);
    // 300 - 20 - 24 = 256: 0 as a char
    assert_eq!(h.ops()[2], (EE_SCREENS_SH_LABEL_NUMBER, 0x9f, 0));
    assert_eq!(h.ops()[3], (BASE + EE_SCREENS_SV_CHANGE_RANGE, 20, 0));
    // the same value: no label
    let h = Rec::new(&[(EE_SCREENS_SH_SLIDER_VALUE, 20)]);
    make_kind(EE_SCREENS_SK_ELEVATION_SLIDER, &[]).unwrap().virt(&ctx(&h), EE_SCREENS_SV_CHANGE_RANGE, 20, 0, &mut f);
    assert_eq!(h.ops().len(), 2);
}

#[test]
fn the_error_log_releases_the_mouse_unless_the_scroll_bar_holds_it() {
    let mut f = [0.0; 4];
    let h = Rec::new(&[(BASE + EE_SCREENS_SV_MOUSE_L_DOWN, 7)]);
    let k = make_kind(EE_SCREENS_SK_ERROR_LOG, &[]).unwrap();
    assert_eq!(k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_L_DOWN, 0, 0, &mut f), 7);
    assert_eq!(h.ops().last(), Some(&(EE_SCREENS_SH_RELEASE_MOUSE, 0, 0)));
    let h = Rec::new(&[(EE_SCREENS_SH_SCROLL_CAPTURED, 1)]);
    k.virt(&ctx(&h), EE_SCREENS_SV_MOUSE_L_DOWN, 0, 0, &mut f);
    assert_eq!(h.ops().last(), Some(&(EE_SCREENS_SH_SCROLL_CAPTURED, 0, 0)));
}

#[test]
fn a_form_kind_reaches_the_form_base_and_its_own_virtuals() {
    let h = Rec::new(&[(EE_SCREENS_SH_FORM_BASE + EE_SCREENS_SV_UPDATE_MOUSE, 7)]);
    let c = ctx(&h);
    let mut f = [1.0, 2.0, 3.0, 4.0];
    assert_eq!(c.form_base(EE_SCREENS_SV_UPDATE_MOUSE, 0, 0, &mut f), 7);
    c.form_self(EE_SCREENS_SV_OPEN);
    assert_eq!(
        h.ops(),
        vec![(EE_SCREENS_SH_FORM_BASE + EE_SCREENS_SV_UPDATE_MOUSE, 0, 0), (EE_SCREENS_SH_FORM_SELF, 32, 0)]
    );
    // the form ops sit above every control base call
    const { assert!(EE_SCREENS_SH_FORM_BASE > EE_SCREENS_SH_BASE + EE_SCREENS_SV_SET_RECTANGLE) };
    const { assert!(EE_SCREENS_SH_FORM_SELF < EE_SCREENS_SH_BASE) };
}
