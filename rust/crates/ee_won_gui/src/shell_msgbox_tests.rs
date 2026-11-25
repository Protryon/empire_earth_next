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

/// MSMessageBox::Msg as GuiShell.cpp's MSMessageBoxMsg had it at 2d31b7328: `SmartPtr<Component> aBox =
/// WONShellNew(MsMessageBox, texts); SmartPtr<Window> aWindow = CreateNativeWindow(); aWindow->SetParent(inParent);
/// aWindow->SetTitle(inTitle); aWindow->SetStyle(0x59); aWindow->Create(WONRectangle(0, 0, aBox->Width(),
/// aBox->Height())); aWindow->AddComponent(aBox.get()); ((Component*)WONShell::Op(aBox.get(), GetButton1))->
/// RequestFocus(); int aResult = aWindow->RunDialog(aBox.get()); aWindow->Close(); return aResult;` (the two
/// SmartPtrs released last, the window's first)
#[test]
fn the_message_box_runs_in_its_own_window() {
    let _w = crate::compmem::filled_world(|_| crate::compmem::block(0, 0, 40, 20));
    take();
    let t = EeWonMsgBoxTexts { title: 0x71, message: 0x72, buttons: [0x73, 0x74, 0x75], ok: 0x76 };
    let tp = &t as *const EeWonMsgBoxTexts;
    // SAFETY: the texts live through the call; the recording host
    let r = unsafe { crate::shell_msgbox::ee_won_msgbox_run(Some(host), 0x99, tp) };
    assert_eq!(r, 7);
    let expect = [
        format!("MsgBoxNew {} 0 0 0", tp as isize),
        "Ref 7 1 0 0".into(),
        "MsgWinNew 0 0 0 0".into(),
        "RefObj 7 1 0 0".into(),
        "MsgWinParent 7 153 0 0".into(),
        "MsgWinTitle 7 113 0 0".into(),
        "MsgWinStyle 7 89 0 0".into(),
        "MsgWinCreate 7 40 20 0".into(),
        "MsgWinAdd 7 7 0 0".into(),
        format!("MsgShellOp 7 {} 0 0", EeWonShellOp::GetButton1 as isize),
        "MsgFocus 7 0 0 0".into(),
        "MsgRunDialog 7 7 0 0".into(),
        "MsgWinClose 7 0 0 0".into(),
        "RefObj 7 0 0 0".into(),
        "Ref 7 0 0 0".to_string(),
    ];
    assert_eq!(take(), expect);
}
