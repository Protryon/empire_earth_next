//! The message box's constructor against the deleted classes' (MSControlConstructors.cpp's MSDialog, MSMessageBox.cpp),
//! written out as the calls each made; tests/won/won_shell_test.cpp runs the original statements themselves.

use std::ffi::c_void;

use crate::btnobj::EeWonBtnCall as Bc;
use crate::compobj::EeWonCompCall as Cc;
use crate::ctobj::EeWonCtCall as Ct;
use crate::dialogobj::EeWonDlgCall as Dc;
use crate::msbtnobj::EeWonMsBtnCall as M;
use crate::registry::EeWonGuiKind as G;
use crate::shell::tests::{host, take};
use crate::shell::*;
use crate::shell_msgbox::EeWonMsgBoxTexts;

const ME: isize = 0x5e40;

fn call(p: isize, kind: G, k: isize, a: isize, b: isize) -> String {
    format!("Call {p} {} {a} {b}", ((kind as isize) << 16) | k)
}
/// A layout's line without its rectangle's address.
fn norm(t: Vec<String>) -> Vec<String> {
    t.into_iter()
        .map(|l| if l.starts_with("Layout ") { l.rsplit_once(' ').unwrap().0.to_string() } else { l })
        .collect()
}

#[test]
fn the_box_with_empty_texts() {
    let _w = crate::compmem::filled_world(|_| crate::compmem::block(0, 0, 40, 20));
    let s = ee_won_shell_new(EeWonShellKind::MsMessageBox, ME as *mut c_void, std::ptr::null_mut(), Some(host));
    // SAFETY: the shell just made
    unsafe {
        assert_eq!(std::ffi::CStr::from_ptr(ee_won_shell_name(s)).to_str().unwrap(), "MSMessageBox");
        assert_eq!(ee_won_shell_overrides(s), 0);
    }
    assert_eq!(ee_won_shell_base(EeWonShellKind::MsMessageBox), EeWonShellBase::Dialog);
    take();
    // every text an empty one (no GUIString: length 0), "Ok" too: the host answers 7 for each new widget
    let t = EeWonMsgBoxTexts { title: 0, message: 0, buttons: [0; 3], ok: 0 };
    // SAFETY: as above
    unsafe { ee_won_shell_init(s, &t as *const EeWonMsgBoxTexts as isize, 0, 0) };
    let new = |k: EeWonShellKind| format!("New {} 0 0 0", k as isize);
    let r = |d: isize| format!("Ref 7 {d} 0 0");
    let ct = |a| call(ME, G::Container, Ct::AddChild as isize, a, 0);
    let expect = vec![
        new(EeWonShellKind::MsDlgBack),
        r(1),
        format!("Layout 7 3 {ME}"),
        ct(7),
        r(-1),
        new(EeWonShellKind::MsLabel),
        r(1),
        call(7, G::MsButton, M::SetHorzAlign as isize, 1, 0),
        call(7, G::MsButton, M::SetAdjustHeightToFitText as isize, 1, 0),
        "SetWrap 7 1 0 0".into(),
        new(EeWonShellKind::MsButton),
        r(1),
        new(EeWonShellKind::MsButton),
        r(1),
        new(EeWonShellKind::MsButton),
        r(1),
        "SameSize 7 7 7 0".into(),
        "WrapWidthOf 7 0 0 0".into(),
        // the first button 40 wide: widened to 80; the text 7 wide: the box 140 + 20
        format!("Call 7 {} 80 20", Cc::SetSize as isize),
        call(ME, G::Dialog, Dc::SetDefaultButton as isize, 7, 0),
        call(7, G::Button, Bc::SetButtonFlags as isize, 0x100, 1),
        call(7, G::Button, Bc::SetButtonFlags as isize, 0x100, 1),
        call(7, G::Button, Bc::SetButtonFlags as isize, 0x100, 1),
        call(ME, G::Dialog, Dc::SetEndOnEscape as isize, 1, 0),
        format!("Comp {} 160 20 0", Cc::SetSize as isize),
        format!("Call 7 {} 10 10", Cc::SetPos as isize),
        format!("Layout 7 1 {ME}"),
        "Layout 7 512 7".into(),
        "CenterLayout 7 0 0 0".into(),
        format!("Comp {} 40 30 0", Cc::SetSize as isize),
        ct(7),
        ct(7),
    ];
    assert_eq!(norm(take()), expect);
    // SAFETY: as above
    unsafe {
        assert_eq!(ee_won_shell_op(s, EeWonShellOp::GetButton1, 0, 0), 7);
        ee_won_shell_free(s);
    }
    assert_eq!(take(), [r(-1), r(-1), r(-1), r(-1)]);
}

#[test]
fn without_texts_it_is_the_dialog_alone() {
    let _w = crate::compmem::filled_world(|_| crate::compmem::block(0, 0, 40, 20));
    let s = ee_won_shell_new(EeWonShellKind::MsMessageBox, ME as *mut c_void, std::ptr::null_mut(), Some(host));
    take();
    // SAFETY: the shell just made
    unsafe { ee_won_shell_init(s, 0, 0, 0) };
    assert_eq!(norm(take()).len(), 5);
    // SAFETY: as above
    unsafe { ee_won_shell_free(s) };
    assert!(take().is_empty());
}
