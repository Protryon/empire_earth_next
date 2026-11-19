//! The widget ops against the C++ forwards they replace: for each op, one widget object made through the C ABI (so
//! registered) answers the op, and a twin made the same way receives the forward the C++ inline method made
//! (`ee_won_*_call(this->mRust, ...)` with the C++ host case's arguments); the calls each object makes out (its host
//! trace) and the results must agree.

use std::cell::RefCell;
use std::ffi::c_void;

use super::*;
use ee_won_gui::btnobj::{ee_won_btn_call, ee_won_btn_free, ee_won_btn_new};
use ee_won_gui::guihost::EeWonGuiOp;
use ee_won_gui::inputobj::{ee_won_input_call, ee_won_input_free, ee_won_input_new, EeWonInputCall};
use ee_won_gui::listobj::{ee_won_list_call, ee_won_list_free, ee_won_list_new};

/// One call out: the op and its four arguments.
type Out = (i32, isize, isize, isize, isize);

thread_local! {
    static TRACE: RefCell<Vec<Out>> = const { RefCell::new(Vec::new()) };
}

unsafe extern "C" fn rec(_c: *mut c_void, op: EeWonGuiOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    // a text handed out is a pointer: its length (c) and its first unit stand for it
    let b =
        if op as i32 == EeWonGuiOp::TextStore as i32 && c > 0 { (unsafe { *(b as *const u32) }) as isize } else { b };
    TRACE.with(|t| t.borrow_mut().push((op as i32, a, b, c, d)));
    0
}

fn take() -> Vec<Out> {
    TRACE.with(|t| std::mem::take(&mut *t.borrow_mut()))
}

/// A zeroed component (its members block in place, as compmem.rs reads a live C++ component).
fn block() -> Box<[u64; 64]> {
    Box::new([0u64; 64])
}

#[cfg(windows)]
type WChar = u16;
#[cfg(not(windows))]
type WChar = u32;

fn wide(s: &[u32]) -> Vec<WChar> {
    s.iter().map(|&u| u as WChar).collect()
}

#[test]
fn input_ops_are_the_forwards() {
    let (c1, c2) = (block(), block());
    let (w1, w2) = (c1.as_ptr() as *mut c_void, c2.as_ptr() as *mut c_void);
    let i1 = ee_won_input_new(w1, Some(rec));
    let i2 = ee_won_input_new(w2, Some(rec));
    let five = wide(&number_units(-512));
    let seven = wide(&number_units(7));
    // (the op, b, c, the forward on the twin)
    type Fwd = Box<dyn Fn(*const EeWonInput) -> isize>;
    let cases: Vec<(DOp, isize, isize, Fwd)> = vec![
        (
            DOp::InputSetNumber,
            -512,
            1,
            Box::new(move |p| unsafe {
                ee_won_input_call(p, EeWonInputCall::SetText, five.as_ptr() as isize, five.len() as isize, 1)
            }),
        ),
        (DOp::InputTextLength, 0, 0, Box::new(|p| unsafe { (*p).real_text().len() as isize })),
        (DOp::InputToInt, 0, 0, Box::new(|_| -512)),
        (
            DOp::InputSelectRange,
            1,
            3,
            Box::new(|p| unsafe { ee_won_input_call(p, EeWonInputCall::SelectRange, 1, 3, 0) }),
        ),
        (DOp::InputClear, 1, 0, Box::new(|p| unsafe { ee_won_input_call(p, EeWonInputCall::SetText, 0, 0, 1) })),
        (DOp::InputSetEmpty, 0, 0, Box::new(|p| unsafe { ee_won_input_call(p, EeWonInputCall::SetText, 0, 0, 0) })),
        (DOp::InputShownLength, 0, 0, Box::new(|p| unsafe { (*p).shown_len() as isize })),
        (
            DOp::InputSetNumber,
            7,
            0,
            Box::new(move |p| unsafe {
                ee_won_input_call(p, EeWonInputCall::SetText, seven.as_ptr() as isize, seven.len() as isize, 0)
            }),
        ),
        (DOp::InputToInt, 0, 0, Box::new(|_| 7)),
    ];
    for (op, b, c, fwd) in cases {
        let r1 = dlg_op(op, w1 as isize, b, c).expect("registered");
        let t1 = take();
        let r2 = fwd(i2);
        let t2 = take();
        assert_eq!(r1, r2, "{op:?}");
        assert_eq!(t1, t2, "{op:?}");
    }
    unsafe {
        ee_won_input_free(i1);
        ee_won_input_free(i2);
    }
    // freed: no answer (the op would go to the C++ host)
    assert!(dlg_op(DOp::InputClear, w1 as isize, 0, 0).is_none());
}

#[test]
fn button_and_list_ops_are_the_forwards() {
    let (c1, c2) = (block(), block());
    let (w1, w2) = (c1.as_ptr() as *mut c_void, c2.as_ptr() as *mut c_void);
    let b1 = ee_won_btn_new(w1, Some(rec));
    let b2 = ee_won_btn_new(w2, Some(rec));
    for (b, c) in [(1, 0), (0, 1), (1, 1)] {
        let r1 = ctl_op(0, COp::ButtonSetCheck, w1 as isize, b, c).unwrap();
        let t1 = take();
        unsafe { ee_won_btn_call(b2, EeWonBtnCall::SetCheck, b, c, 0) };
        assert_eq!((r1, t1), (0, take()));
        let checked =
            unsafe { ee_won_btn_call(b2, EeWonBtnCall::GetField, EeWonBtnField::Flags as isize, 0, 0) } & 4 != 0;
        assert_eq!(dlg_op(DOp::ButtonIsChecked, w1 as isize, 0, 0), Some(checked as isize));
        assert_eq!(ctl_op(0, COp::ButtonIsChecked, w1 as isize, 0, 0), Some(checked as isize));
    }
    unsafe {
        ee_won_btn_free(b1);
        ee_won_btn_free(b2);
    }
    let l1 = ee_won_list_new(w1, Some(rec));
    let l2 = ee_won_list_new(w2, Some(rec));
    let calls: Vec<(COp, isize, isize, L, isize, isize)> = vec![
        (COp::ListBeginUpdate, 0, 0, L::BeginUpdate, 0, 0),
        (COp::ListEndUpdate, 0, 0, L::EndUpdate, 0, 0),
        (COp::ListNumItems, 0, 0, L::NumItems, 0, 0),
        (COp::ListGetSel, 0, 0, L::SelItem, 0, 0),
        (COp::ListSetSel, 0, 1, L::SetSelItemAt, 0, 1),
        (COp::ListItemAtY, 7, 0, L::GetItemPosAtYPos, 7, 0),
        (COp::ListClear, 1, 0, L::Clear, 1, 0),
        (COp::ListPaintWidth, 0, 0, L::PaintWidth, 0, 0),
    ];
    for (op, b, c, call, x, y) in calls {
        let r1 = ctl_op(0, op, w1 as isize, b, c).unwrap();
        let t1 = take();
        let r2 = unsafe { ee_won_list_call(l2, call, x, y, 0) };
        assert_eq!(t1, take(), "{op:?}");
        if !matches!(op, COp::ListSetSel | COp::ListClear | COp::ListBeginUpdate | COp::ListEndUpdate) {
            assert_eq!(r1, r2 as i32 as isize, "{op:?}");
        }
    }
    unsafe {
        ee_won_list_free(l1);
        ee_won_list_free(l2);
    }
}

#[test]
fn members_and_texts() {
    let c1 = block();
    let w = c1.as_ptr() as isize;
    assert_eq!(ctl_op(0, COp::SetControlId, w, 1234, 0), Some(0));
    assert_eq!(ctl_op(0, COp::ControlId, w, 0, 0), Some(1234));
    assert_eq!(dlg_op(DOp::ButtonSetControlId, w, 77, 0), Some(0));
    assert_eq!(m::control_id(w), 77);
    m::set_bits(w, 0x80);
    assert_eq!(dlg_op(DOp::ButtonIsDisabled, w, 0, 0), Some(1));
    assert_eq!(ctl_op(0, COp::IsDisabled, w, 0, 0), Some(1));
    m::set_timer(w, 3);
    assert_eq!(ctl_op(w, COp::ClearTimerFlag, 0, 0, 0), Some(0));
    assert_eq!(m::timer(w), 1);
    assert_eq!(number_units(-40), vec![u32::from(b'-'), u32::from(b'4'), u32::from(b'0')]);
    // a widget the registry does not know: the C++ host answers
    assert!(ctl_op(0, COp::ListNumItems, w, 0, 0).is_none());
    assert!(dlg_op(DOp::ComboGetSel, w, 0, 0).is_none());
}
