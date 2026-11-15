use super::*;
use crate::formobj::{ee_ui_form_free, ee_ui_form_new};

fn p(v: usize) -> *mut c_void {
    v as *mut c_void
}

#[test]
fn put_find_forget() {
    put(Kind::Label, p(0x7100), p(0x7200));
    put(Kind::Control, p(0x7100), p(0x7300));
    assert_eq!(find(Kind::Label, p(0x7100)), p(0x7200));
    assert_eq!(find(Kind::Control, p(0x7100)), p(0x7300));
    assert!(find(Kind::Edit, p(0x7100)).is_null());
    assert!(find(Kind::Label, core::ptr::null()).is_null());
    // a later object at the same C++ address replaces the first; forgetting the first then leaves it alone
    put(Kind::Label, p(0x7100), p(0x7400));
    forget(Kind::Label, p(0x7200));
    assert_eq!(find(Kind::Label, p(0x7100)), p(0x7400));
    forget(Kind::Label, p(0x7400));
    assert!(find(Kind::Label, p(0x7100)).is_null());
    forget(Kind::Control, p(0x7300));
    assert!(find(Kind::Control, p(0x7100)).is_null());
    // unknown objects and NULLs are no-ops
    forget(Kind::Slider, p(0x7500));
    put(Kind::Slider, core::ptr::null(), p(0x7600));
    put(Kind::Slider, p(0x7600), core::ptr::null());
    assert!(find(Kind::Slider, p(0x7600)).is_null());
}

#[test]
fn unknown_objects_answer_nothing() {
    let x = p(0x7700);
    assert_eq!(manager_call(x, EeUiManagerCall::Pop, 0, 0, 0), 0);
    assert_eq!(form_call(x, EeUiFormCall::FormId, 0, 0), 0);
    assert!(control(x, 3).is_null());
    assert_eq!(control_call(x, EeUiControlCall::TriggerEvent, 0, 0), 0);
    assert_eq!(control_virtual(x, EeUiOp::SetDirty, 1, 0, &mut [0.0; 4]), 0);
    assert!(with_fields(x, |r| r.mId).is_none());
    assert!(button_call(x, EeUiButtonCall::CheckIsChecked, 0, 0).is_none());
    assert!(combo_call(x, EeUiComboCall::DeleteAllEntries, 0, 0).is_none());
    assert!(label_call(x, EeUiLabelCall::LineCount, 0, 0).is_none());
    assert!(edit_call(x, EeUiEditCall::Clear, 0, 0).is_none());
    assert!(slider_call(x, EeUiSliderCall::CurrentRangeValue, 0, 0).is_none());
    assert_eq!(group_call(x, EeUiEntityGroupCall::Enable, 0), 0);
    assert_eq!(background_call(x, EeUiBgCall::SetVisibility, 1, 0), 0);
}

unsafe extern "C" fn quiet(_: *mut c_void, _: EeUiOp, _: isize, _: isize, _: isize, _: isize, _: *mut f32) -> isize {
    0
}

#[test]
fn a_form_is_reached_by_its_cpp_pointer() {
    let cpp = p(0x7800);
    let form = ee_ui_form_new(cpp, Some(quiet), 4, 1, p(0x7900), 37, false);
    assert_eq!(find(Kind::Form, cpp), form as *mut c_void);
    assert_eq!(form_call(cpp, EeUiFormCall::FormId, 0, 0), 37);
    assert_eq!(manager_of(cpp), p(0x7900));
    assert!(control(cpp, 2).is_null());
    // SAFETY: made above, not used again
    unsafe { ee_ui_form_free(form) };
    assert!(find(Kind::Form, cpp).is_null());
    assert_eq!(form_call(cpp, EeUiFormCall::FormId, 0, 0), 0);
}

#[test]
fn texts() {
    assert_eq!(label_number_text(0), b"0\0");
    assert_eq!(label_number_text(-2147483648), b"-2147483648\0");
    assert_eq!(label_number_text(905), b"905\0");
    assert_eq!(c_text(b"ab\0cd"), b"ab\0");
    assert_eq!(c_text(b""), b"\0");
    assert_eq!(c_text("é".as_bytes()), "é\0".as_bytes());
    // SAFETY: a NUL-terminated literal
    assert_eq!(unsafe { c_bytes(c"xy".as_ptr() as *const c_void) }, b"xy");
    // SAFETY: NULL
    assert!(unsafe { c_bytes(core::ptr::null()) }.is_empty());
}

#[test]
fn a_background_is_found_by_its_cpp_pointer_not_its_record() {
    // the record is a base of the C++ background at another address; the constructor names both
    let (rec, cpp) = (0x7a00usize as *mut EeUiBackgroundFields, p(0x79f8));
    // SAFETY: only recorded, never dereferenced
    unsafe { ee_ui_direct_background(rec, cpp) };
    assert_eq!(find(Kind::Background, cpp), rec as *mut c_void);
    assert!(find(Kind::Background, rec as *const c_void).is_null());
    forget(Kind::Background, rec as *const c_void);
    assert!(find(Kind::Background, cpp).is_null());
}
