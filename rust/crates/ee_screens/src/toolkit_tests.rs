use super::*;
use crate::save_load::EE_SCREENS_OP_POST_EVENT;
use core::ptr::null_mut;
use ee_ui::formobj::{ee_ui_form_free, ee_ui_form_new};

thread_local! {
    /// The records' colours `GAME_PGS_COMBINE` reads (the statistics' in the game).
    pub(super) static RECORD_COLOURS: core::cell::RefCell<Vec<[f32; 3]>> = const { core::cell::RefCell::new(Vec::new()) };
}

thread_local! {
    /// The events the tests' helper made and the reference changes it saw: (op, event, code).
    pub(super) static EVENTS: core::cell::RefCell<Vec<(u32, usize, u32)>> = const { core::cell::RefCell::new(Vec::new()) };
}

/// The host's event helper in the tests: events numbered from 0x9000 by code, every op recorded.
pub(super) fn event_op(o: u32, event: *mut c_void, code: u32) -> *mut c_void {
    let made = if o == 0 && code != 0 { (0x9000 + code as usize) as *mut c_void } else { core::ptr::null_mut() };
    EVENTS.with(|e| e.borrow_mut().push((o, if o == 0 { made as usize } else { event as usize }, code)));
    made
}

fn op(form: *mut c_void, entity: *mut c_void, o: u32, a: u32, b: u32, c: u32) -> Option<u32> {
    super::op(form, entity, o, a, b, c, 0)
}

fn p(v: usize) -> *mut c_void {
    v as *mut c_void
}

#[test]
fn the_game_ops_go_to_the_host() {
    for o in [EE_SCREENS_OP_POST_EVENT, EE_SCREENS_OP_SLIDER_LABEL, 0, 0xffff_ffff] {
        assert_eq!(op(p(0x6100), p(0x6200), o, 1, 2, 3), None, "{o:#x}");
    }
    assert_eq!(text(p(0x6100), EE_SCREENS_TEXT_STRING, 5), None);
    assert_eq!(put_text(p(0x6100), EE_SCREENS_TEXT_QUESTION, 0, b"x"), None);
}

#[test]
fn unknown_controls_answer_as_missing_ones() {
    let f = p(0x6300);
    assert_eq!(op(f, null_mut(), EE_SCREENS_OP_ENTITY_ID, 0, 0, 0), Some(0xffff_ffff));
    assert_eq!(op(f, p(0x6400), EE_SCREENS_OP_ENTITY_ID, 0, 0, 0), Some(0));
    for o in [
        EE_SCREENS_OP_CHECKBOX,
        EE_SCREENS_OP_RADIO,
        EE_SCREENS_OP_RANGE,
        EE_SCREENS_OP_CONTROL_ID,
        EE_SCREENS_OP_LE_HAS_TRIGGER,
        EE_SCREENS_OP_LABEL_LINE_COUNT,
        EE_SCREENS_OP_LE_FORM_VISIBLE,
        EE_SCREENS_OP_LE_KEYBOARD_CAPTURED,
        EE_SCREENS_OP_DSU_SLIDER,
        EE_SCREENS_OP_SHOW,
        EE_SCREENS_OP_PUSH,
        EE_SCREENS_OP_LE_TYPE_SELECT,
    ] {
        assert_eq!(op(f, p(0x6400), o, 1, 1, 1), Some(0), "{o:#x}");
    }
    // no current form and no such form: equal, as the C++'s two NULLs
    assert_eq!(op(f, null_mut(), EE_SCREENS_OP_CURRENT_FORM_IS, 1, 0, 0), Some(1));
    assert_eq!(text(f, EE_SCREENS_TEXT_MUSIC_FILE, 0), Some(Vec::new()));
    assert_eq!(text(f, EE_SCREENS_TEXT_EDIT, 2), Some(Vec::new()));
    assert_eq!(put_text(f, EE_SCREENS_TEXT_LABEL, 2, b"x"), Some(0));
}

unsafe extern "C" fn quiet(
    _: *mut c_void,
    _: ee_ui::obj::EeUiOp,
    _: isize,
    _: isize,
    _: isize,
    _: isize,
    _: *mut f32,
) -> isize {
    0
}

#[test]
fn a_registered_form_answers_its_music_and_keyboard() {
    let cpp = p(0x6500);
    let form = ee_ui_form_new(cpp, Some(quiet), 2, 1, null_mut(), 12, false);
    assert_eq!(put_text(cpp, EE_SCREENS_TEXT_MUSIC_FILE, 0, b"menu.mp3\0junk"), Some(0));
    assert_eq!(text(cpp, EE_SCREENS_TEXT_MUSIC_FILE, 0), Some(b"menu.mp3".to_vec()));
    assert_eq!(op(cpp, null_mut(), EE_SCREENS_OP_CH_KEYBOARD_CAPTURED, 0, 0, 0), Some(0));
    assert_eq!(op(cpp, null_mut(), EE_SCREENS_OP_CONTROL_ID, 1, 0, 0), Some(0));
    // SAFETY: made above, not used again
    unsafe { ee_ui_form_free(form) };
    assert_eq!(text(cpp, EE_SCREENS_TEXT_MUSIC_FILE, 0), Some(Vec::new()));
}

fn events() -> Vec<(u32, usize, u32)> {
    EVENTS.with(|e| core::mem::take(&mut *e.borrow_mut()))
}

#[test]
fn the_event_group_makes_and_releases_its_events() {
    let f = p(0x6600);
    events();
    // an open event: made, handed to the form (none here), released
    assert_eq!(op(f, null_mut(), EE_SCREENS_OP_OPEN_EVENT, 3, 5, 0), Some(0));
    assert_eq!(events(), vec![(0, 0x9005, 5), (2, 0x9005, 0)]);
    // no event for code 0, nothing to release
    assert_eq!(op(f, null_mut(), EE_SCREENS_OP_TRIGGER_HERE, 1, 2, 0), Some(0));
    assert_eq!(events(), vec![(0, 0, 0)]);
    // a hot key keeps its event's reference
    assert_eq!(super::op(f, null_mut(), EE_SCREENS_OP_HOT_KEY, 1, 2, 3, 7), Some(0));
    assert_eq!(events(), vec![(0, 0x9007, 7)]);
    // the grab on a form with no mouse move event: none saved, the grab's made and released, nothing at its end
    assert_eq!(op(f, null_mut(), EE_SCREENS_OP_CAM_GRAB_BEGIN, 1, 0, 0), Some(0));
    assert_eq!(
        events(),
        vec![
            (0, 0x9000 + EE_SCREENS_CAM_GRAB_EDITOR as usize, EE_SCREENS_CAM_GRAB_EDITOR),
            (2, 0x9000 + EE_SCREENS_CAM_GRAB_EDITOR as usize, 0)
        ]
    );
    assert_eq!(op(f, null_mut(), EE_SCREENS_OP_CAM_GRAB_END, 1, 0, 0), Some(0));
    release_grab(1);
    assert_eq!(events(), vec![]);
}

thread_local! {
    static LIST_OPS: core::cell::RefCell<Vec<(u32, isize, isize, isize)>> = const { core::cell::RefCell::new(Vec::new()) };
}

/// A list box's per-type host in the tests: two lines, a selection of 1, every op recorded.
unsafe extern "C" fn list_host(
    _: *mut c_void,
    o: ee_ui::obj::EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    _: isize,
    f: *mut f32,
) -> isize {
    use ee_ui::obj::EeUiOp as O;
    LIST_OPS.with(|l| l.borrow_mut().push((o as u32, a, b, c)));
    match o {
        O::ListLineCount => 2,
        O::ListLineText if a == 0 => c"first".as_ptr() as isize,
        O::ListLineText if a == 1 => c"second".as_ptr() as isize,
        O::ListSelection => {
            // SAFETY: the op's four floats
            unsafe { *f = f32::from_bits(1) };
            0
        }
        _ => 0,
    }
}

#[test]
fn the_list_ops_reach_the_list_s_lines() {
    use ee_ui::obj::EeUiOp as O;
    let cpp = p(0x6700);
    // SAFETY: the host answers every op
    let list = unsafe { ee_ui::listobj::ee_ui_list_new(cpp, Some(list_host), 12, false, 1) };
    LIST_OPS.with(|l| l.borrow_mut().clear());
    assert_eq!(direct::list_line(cpp, 1), Some(b"second".to_vec()));
    assert_eq!(direct::list_line(cpp, 2), None);
    assert_eq!(list_selection(cpp), Some((0, 1)));
    list_select(cpp, 4, true, false);
    list_op(cpp, O::ListAppendText, 0x77, 0, 0);
    let seen: Vec<(u32, isize, isize, isize)> = LIST_OPS.with(|l| l.borrow().clone());
    assert!(seen.contains(&(O::ListChangeSelection as u32, 4, 1, 0)), "{seen:?}");
    assert!(seen.contains(&(O::ListAppendText as u32, 0x77, 0, 0)), "{seen:?}");
    // not a list: nothing
    assert_eq!(direct::list_line(p(0x6800), 0), None);
    assert_eq!(list_selection(p(0x6800)), None);
    // SAFETY: made above
    unsafe { ee_ui::listobj::ee_ui_list_free(list) };
    assert_eq!(list_selection(cpp), None);
}
