use super::*;

use ee_base::ustring::{ee_uwstring_free, ee_uwstring_init, ee_uwstring_init_text};

/// A `UWideString` in place (no vtable: the helpers never read it).
fn wide(text: &str) -> CppWide {
    let mut w = CppWide { vtable: core::ptr::null(), s: unsafe { core::mem::zeroed() } };
    let t = direct::c_text(text.as_bytes());
    unsafe { ee_uwstring_init_text(&mut w.s, t.as_ptr() as *const c_char) };
    w
}

fn free(w: &mut CppWide) {
    unsafe { ee_uwstring_free(&mut w.s) };
}

/// The `n`th string of the language table: its id and text.
fn present(n: usize) -> (u16, &'static str) {
    (0..=u16::MAX).filter_map(|i| ee_base::util::strings::resource_string(i).map(|t| (i, t))).nth(n).unwrap()
}

#[test]
fn atoi_is_the_crt_s() {
    assert_eq!(atoi(b"42"), 42);
    assert_eq!(atoi(b"  -17x"), -17);
    assert_eq!(atoi(b"+8"), 8);
    assert_eq!(atoi(b"\t\n 3 4"), 3);
    assert_eq!(atoi(b""), 0);
    assert_eq!(atoi(b"abc"), 0);
    assert_eq!(atoi(b"2147483648"), i32::MIN);
    assert_eq!(atoi(b"-"), 0);
}

#[test]
fn strings_differ_as_platmbscmp() {
    assert!(!differs(b"abc", b"abc"));
    assert!(differs(b"abc", b"abd"));
    assert!(differs(b"", b"a"));
    // the comparison stops at a NUL, as the C strings did
    assert!(!differs(b"ab\0c", b"ab"));
}

#[test]
fn resource_strings_are_the_table_s() {
    let (id, text) = present(0);
    assert_eq!(resource_string(id as i64), Some(text.as_bytes()));
    // the id is cut to 16 bits, as GetString's (uint16_t) cast
    assert_eq!(resource_string(id as i64 + 0x10000), resource_string(id as i64));
    let missing = (0..=u16::MAX).find(|&i| ee_base::util::strings::resource_string(i).is_none()).unwrap();
    assert_eq!(resource_string(missing as i64), None);
}

#[test]
fn wide_strings_read_and_assign_in_place() {
    let mut w = wide("hello");
    assert_eq!(unsafe { wide_bytes(&w) }, b"hello");
    unsafe { wide_assign(&mut w, b"bye\0rest") };
    assert_eq!(unsafe { wide_bytes(&w) }, b"bye");
    assert_eq!(w.s.length, 3);
    free(&mut w);
    assert!(unsafe { wide_bytes(core::ptr::null()) }.is_empty());
}

fn head(text: &str, chat: &str) -> FormCallHead {
    let mut name = CppWide { vtable: core::ptr::null(), s: unsafe { core::mem::zeroed() } };
    unsafe { ee_uwstring_init(&mut name.s) };
    unsafe { wide_assign(&mut name, b"Somebody") };
    FormCallHead {
        form: core::ptr::null_mut(),
        picked: core::ptr::null_mut(),
        text: wide(text),
        color: core::ptr::null(),
        name,
        chat: wide(chat),
        flags: 0,
        objects: [core::ptr::null_mut(); 11],
        button: core::ptr::null_mut(),
        out: core::ptr::null_mut(),
    }
}

#[test]
fn the_held_text_goes_both_ways() {
    let mut h = head("old", "");
    let mut io = EeScreensMainFormIo::default();
    let t = b"new text";
    let r = unsafe { main_form(&mut h, EE_SCREENS_MF_OP_TEXT_SET, t.len() as i64, t.as_ptr() as i64, &mut io) };
    assert_eq!(r, Some(0));
    assert_eq!(unsafe { main_form(&mut h, EE_SCREENS_MF_OP_TEXT_GET, 0, 0, &mut io) }, Some(8));
    let mut out = [0u8; 3];
    assert_eq!(unsafe { main_form(&mut h, EE_SCREENS_MF_OP_TEXT_GET, 3, out.as_mut_ptr() as i64, &mut io) }, Some(8));
    assert_eq!(&out, b"new");
    free(&mut h.text);
    free(&mut h.name);
    free(&mut h.chat);
}

#[test]
fn the_chat_rest_skips_one_line_break_and_empties_the_name() {
    let mut h = head("", "abc\ndef");
    let mut io = EeScreensMainFormIo::default();
    assert_eq!(unsafe { main_form(&mut h, EE_SCREENS_MF_OP_CHAT_LENGTH, 0, 0, &mut io) }, Some(7));
    assert_eq!(unsafe { main_form(&mut h, EE_SCREENS_MF_OP_CHAT_REST, 3, 0, &mut io) }, Some(0));
    assert_eq!(unsafe { wide_bytes(&h.chat) }, b"def");
    assert!(unsafe { wide_bytes(&h.name) }.is_empty());
    // no break at the cut: the rest as it is
    unsafe { main_form(&mut h, EE_SCREENS_MF_OP_CHAT_REST, 1, 0, &mut io) };
    assert_eq!(unsafe { wide_bytes(&h.chat) }, b"ef");
    free(&mut h.text);
    free(&mut h.name);
    free(&mut h.chat);
}

#[test]
fn a_string_op_falls_through_for_a_missing_string() {
    let mut h = head("kept", "");
    let mut io = EeScreensMainFormIo::default();
    let missing = (0..=u16::MAX).find(|&i| ee_base::util::strings::resource_string(i).is_none()).unwrap();
    assert_eq!(unsafe { main_form(&mut h, EE_SCREENS_MF_OP_TEXT_STRING, missing as i64, 0, &mut io) }, None);
    assert_eq!(unsafe { wide_bytes(&h.text) }, b"kept");
    let (id, text) = present(1);
    assert_eq!(unsafe { main_form(&mut h, EE_SCREENS_MF_OP_TEXT_STRING, id as i64, 0, &mut io) }, Some(0));
    assert_eq!(unsafe { wide_bytes(&h.text) }, text.as_bytes());
    free(&mut h.text);
    free(&mut h.name);
    free(&mut h.chat);
}

#[test]
fn the_toolkit_ops_answer_zero_without_a_form() {
    let mut h = head("", "");
    let mut io = EeScreensMainFormIo::default();
    for op in [
        EE_SCREENS_MF_OP_LABEL_INT,
        EE_SCREENS_MF_OP_LABEL_DIFFERS,
        EE_SCREENS_MF_OP_INFO_HAS_TEXT,
        EE_SCREENS_MF_OP_INFO_HAS_PORTRAIT,
        EE_SCREENS_MF_OP_MOUSE_CAPTURED,
        EE_SCREENS_MF_OP_FORM_VISIBLE,
        EE_SCREENS_MF_OP_BASE_COPY_DATA,
        EE_SCREENS_MF_OP_ALL_DIRTY,
    ] {
        assert_eq!(unsafe { main_form(&mut h, op, 1, 1, &mut io) }, Some(0), "op {op}");
    }
    // an op the C++ answers
    assert_eq!(unsafe { main_form(&mut h, EE_SCREENS_MF_OP_POPULATION, 0, 0, &mut io) }, None);
    free(&mut h.text);
    free(&mut h.name);
    free(&mut h.chat);
}

#[test]
fn the_controls_base_ops_answer_without_a_control() {
    use crate::screens::hud_ctl::*;
    let mut f = [9.0f32; 6];
    assert_eq!(control(core::ptr::null_mut(), EE_SCREENS_HC_DISPLAY_MODE, 0, &mut f), Some(0));
    assert_eq!(control(core::ptr::null_mut(), EE_SCREENS_HC_SET_DIRTY, 1, &mut f), Some(0));
    // a missing control's rectangle goes on to the C++ (which never sees one)
    assert_eq!(control(core::ptr::null_mut(), EE_SCREENS_HC_RECT, 0, &mut f), None);
    assert_eq!(control(core::ptr::null_mut(), EE_SCREENS_HC_VIEW_RECT, 0, &mut f), None);
    assert_eq!(f, [9.0; 6]);
}

#[test]
fn the_cursor_reads_answer_zero_without_units() {
    use crate::screens::cursor::*;
    let read = CursorRead {
        form: core::ptr::null_mut(),
        player: core::ptr::null_mut(),
        group: core::ptr::null_mut(),
        first: core::ptr::null_mut(),
        target: core::ptr::null_mut(),
    };
    let mut f = [1.0f32; 3];
    for op in [
        EE_SCREENS_CURSOR_OP_FIRST,
        EE_SCREENS_CURSOR_OP_T_KIND,
        EE_SCREENS_CURSOR_OP_T_HP,
        EE_SCREENS_CURSOR_OP_T_ATTACK,
        EE_SCREENS_CURSOR_OP_SEL_COUNT,
        EE_SCREENS_CURSOR_OP_SEL_CARGO,
    ] {
        assert_eq!(unsafe { cursor(&read, op, 0, 0, &mut f) }, Some(0), "op {op}");
    }
    // the owner of no target is no player: "not ours", and no war with nobody
    assert_eq!(unsafe { cursor(&read, EE_SCREENS_CURSOR_OP_T_OWNER, 0, 0, &mut f) }, Some(0));
    assert_eq!(unsafe { cursor(&read, EE_SCREENS_CURSOR_OP_SEL_POSITION, 0, 0, &mut f) }, Some(0));
    assert_eq!(f, [0.0; 3]);
    // the method tests stay the C++'s
    assert_eq!(unsafe { cursor(&read, EE_SCREENS_CURSOR_OP_CAN_HEAL, 0, 0, &mut f) }, None);
}

#[test]
fn the_command_buttons_ops_need_a_registered_button() {
    use crate::screens::status_button::*;
    // no button: the C++'s (which never sees one)
    let r = status_button(
        core::ptr::null_mut(),
        core::ptr::null_mut(),
        core::ptr::null_mut(),
        EE_SCREENS_SB_OP_COUNT,
        0,
        0,
    );
    assert_eq!(r, None);
}

#[test]
fn the_main_groups_button_ops_pass_the_others_on() {
    use crate::screens::main_group::*;
    let mut asked = false;
    assert_eq!(
        main_group(
            || {
                asked = true;
                core::ptr::null_mut()
            },
            EE_SCREENS_MGR_OP_FORMS,
            0,
            0
        ),
        None
    );
    assert!(!asked, "the form is asked only for a button op");
    // a button op on a missing button: the copy is a no-op, the record ops the C++'s
    assert_eq!(main_group(core::ptr::null_mut, EE_SCREENS_MGR_OP_COPY_BITMAPS, 1, 2), Some(0));
    assert_eq!(main_group(core::ptr::null_mut, EE_SCREENS_MGR_OP_RESET_COUNT, 1, 0), None);
}

#[test]
fn the_button_record_is_found_by_its_offset() {
    use crate::screens::main_form_fields::*;
    let mut object = [0u64; 16];
    let base = object.as_mut_ptr() as *mut c_void;
    let fields = unsafe { (base as *mut u8).add(24) } as *mut EeScreensStatusButtonFields;
    ee_screens_status_button_fields(base, fields);
    assert_eq!(button_record(base), Some(fields));
    assert_eq!(button_record(core::ptr::null_mut()), None);
}

fn new_box() -> crate::screens::main_form_fields::EeScreensStatusBoxFields {
    let mut r = core::mem::MaybeUninit::uninit();
    unsafe { crate::screens::main_form_fields::ee_screens_status_box_init(r.as_mut_ptr(), 7 as *mut c_void) };
    unsafe { r.assume_init() }
}

#[test]
fn the_status_box_record_starts_empty_and_names_its_slots() {
    use crate::screens::status_box::*;
    let mut r = new_box();
    assert_eq!(r.mAttributes, [-1; 14]);
    assert_eq!(r.mForm, 7 as *mut c_void);
    r.mIcons[2] = 0x10 as *mut c_void;
    r.mLabels[8] = 0x20 as *mut c_void;
    r.mC8C = 0x30 as *mut c_void;
    r.mCAC = 0x40 as *mut c_void;
    assert_eq!(r.control(EE_SCREENS_BOX_CTL_ICON + 2), 0x10 as *mut c_void);
    assert_eq!(r.control(EE_SCREENS_BOX_CTL_LABEL + 8), 0x20 as *mut c_void);
    assert_eq!(r.control(EE_SCREENS_BOX_CTL_PORTRAIT), 0x30 as *mut c_void);
    assert_eq!(r.control(EE_SCREENS_BOX_CTL_FOURTH_BAR), 0x40 as *mut c_void);
    assert!(r.control(EE_SCREENS_BOX_CTL_FOURTH_BAR + 1).is_null());
    assert!(r.control(-1).is_null());
}

#[test]
fn the_status_box_fields_and_formats() {
    use crate::screens::status_box::*;
    let mut r = new_box();
    let mut call =
        BoxCallHead { record: &mut r, unit: core::ptr::null_mut(), complex: core::ptr::null_mut(), text: wide("x") };
    let op = |call: &mut BoxCallHead, op, a, b, c| unsafe { status_box(call, op, a, b, c) };
    assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_SET, EE_SCREENS_BOX_FIELD_COUNT, 5, 0), Some(0));
    assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_SET, EE_SCREENS_BOX_FIELD_ATTRIBUTES + 3, 9, 0), Some(0));
    assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_SET, EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN, 2, 0), Some(0));
    assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_GET, EE_SCREENS_BOX_FIELD_COUNT, 0, 0), Some(5));
    assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_GET, EE_SCREENS_BOX_FIELD_ATTRIBUTES + 3, 0, 0), Some(9));
    assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_GET, EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN, 0, 0), Some(1));
    for (fmt, b, c, want) in [
        (EE_SCREENS_BOX_FMT_PAIR, 3, 40, "3/40"),
        (EE_SCREENS_BOX_FMT_BONUS, 2, 0, "+ 2"),
        (EE_SCREENS_BOX_FMT_SUM, -1, 7, "-1+7"),
        (EE_SCREENS_BOX_FMT_ONE, 12, 0, "12"),
        (EE_SCREENS_BOX_FMT_EMPTY, 1, 1, ""),
    ] {
        assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_FORMAT, fmt, b, c), Some(0));
        assert_eq!(unsafe { wide_bytes(&call.text) }, want.as_bytes());
    }
    // the C++'s ops (the unit's, the bars', the bitmaps')
    assert_eq!(op(&mut call, EE_SCREENS_BOX_OP_NAME, 0, 0, 0), None);
    assert_eq!(r.mAttributeCount, 5);
    free(&mut call.text);
}

#[test]
fn the_main_game_events_ops_on_the_form_record() {
    use crate::screens::main_form_fields::*;
    use crate::screens::main_game::*;
    let mut object = [0u64; 64];
    let form = object.as_mut_ptr() as *mut c_void;
    // (the offset main_form_fields.rs' own test registers: the tests may run at once)
    let fields = unsafe { (form as *mut u8).add(0x40) } as *mut EeScreensMainFormFields;
    ee_screens_main_form_fields(form, fields);
    unsafe { (*fields).mUIState = 6 };
    assert_eq!(main_game(form, EE_SCREENS_GAME_MG_UI_STATE, 0), Some(6));
    assert_eq!(main_game(form, EE_SCREENS_GAME_MG_CURSOR_LOCK, 1), Some(0));
    assert_eq!(main_game(form, EE_SCREENS_GAME_MG_CURRENT_GROUP, 3), Some(0));
    unsafe {
        assert!((*fields).mIsCursorLocked);
        assert_eq!((*fields).mCurrentGroup, 3);
    }
    // no form: the C++'s (never asked so); (the dialog test reads the world's views: a live game only)
    assert_eq!(main_game(core::ptr::null_mut(), EE_SCREENS_GAME_MG_UI_STATE, 0), None);
    assert_eq!(main_game(form, EE_SCREENS_GAME_MG_CLEAR_GHOSTS, 0), None);
}
