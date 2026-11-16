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
