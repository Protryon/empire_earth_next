//! The WON skin's kinds' statements against the deleted classes' (written out as the calls each made);
//! tests/won/won_wonskin_test.cpp runs the original statements themselves against the kinds.

use std::ffi::c_void;

use super::*;
use crate::guihost::EeWonWbImage as I;
use crate::shell::tests::{host, take};
use crate::shell::*;

fn made(kind: EeWonShellKind, a: isize, b: isize) -> (*mut EeWonShell, crate::compmem::FilledWorld) {
    let w = crate::compmem::filled_world(|_| crate::compmem::block(0, 0, 40, 20));
    let s = ee_won_shell_new(kind, 0x5e40 as *mut c_void, std::ptr::null_mut(), Some(host));
    take();
    // SAFETY: the shell just made
    unsafe { ee_won_shell_init(s, a, b, 0) };
    (s, w)
}

#[test]
fn the_masks_names_and_bases() {
    for (k, name, over, b) in [
        (EeWonShellKind::WonSimpleButton, "WONSimpleButton", 0b110, EeWonShellBase::Button),
        (EeWonShellKind::WonScrollThumb, "WONScrollThumb", 0b110, EeWonShellBase::Button),
        (EeWonShellKind::WonHeaderButton, "WONHeaderButton", 0b100, EeWonShellBase::WonButton),
        (EeWonShellKind::WonInputBox, "WONInputBox", 0b100, EeWonShellBase::Input),
        (EeWonShellKind::WonScrollbar, "WONScrollbar", 0, EeWonShellBase::Scrollbar),
        (EeWonShellKind::WonComboBox, "WONComboBox", 0b111, EeWonShellBase::Combo),
        (EeWonShellKind::WonScrollContainer, "WONScrollContainer", 0b101, EeWonShellBase::Scroller),
        (EeWonShellKind::WonListCtrl, "WONListCtrl", 0b101, EeWonShellBase::Scroller),
        (EeWonShellKind::WonHeaderControl, "WONHeaderControl", 0, EeWonShellBase::Header),
        (EeWonShellKind::WonTabCtrl, "WONTabCtrl", 0, EeWonShellBase::Tab),
        (EeWonShellKind::WonFrameComponent, "WONFrameComponent", 0b100, EeWonShellBase::Component),
    ] {
        let s = ee_won_shell_new(k, 0x5e41 as *mut c_void, std::ptr::null_mut(), Some(host));
        // SAFETY: the shell just made
        unsafe {
            assert_eq!(std::ffi::CStr::from_ptr(ee_won_shell_name(s)).to_str().unwrap(), name);
            assert_eq!(ee_won_shell_overrides(s), over, "{name}");
            ee_won_shell_free(s);
        }
        assert_eq!(ee_won_shell_base(k), b, "{name}");
    }
    take();
}

#[test]
fn a_frame_set_is_a_duplicate_of_the_image_and_released_with_the_widget() {
    let (s, _w) = made(EeWonShellKind::WonInputBox, 0, 0);
    let f = EeWonFrameIn { left: 1, top: 2, right: 3, bottom: 4, image: 0x50 };
    // SAFETY: made above
    unsafe {
        ee_won_shell_op(s, EeWonShellOp::SetFrame, &f as *const EeWonFrameIn as isize, 0);
        // the host's duplicate is 7
        assert_eq!(take(), [format!("Img {} 80 0 0", I::Duplicate as isize)]);
        let mut out = EeWonFrameIn::default();
        ee_won_shell_op(s, EeWonShellOp::GetFrame, &mut out as *mut EeWonFrameIn as isize, 0);
        assert_eq!(out, EeWonFrameIn { image: 7, ..f });
        // a frame with no image releases the duplicate
        let none = EeWonFrameIn::default();
        ee_won_shell_op(s, EeWonShellOp::SetFrame, &none as *const EeWonFrameIn as isize, 0);
        assert_eq!(take(), [format!("Img {} 7 0 0", I::Release as isize)]);
        ee_won_shell_op(s, EeWonShellOp::SetFrame, &f as *const EeWonFrameIn as isize, 0);
        take();
        ee_won_shell_free(s);
        assert_eq!(take(), [format!("Img {} 7 0 0", I::Release as isize)]);
    }
}

#[test]
fn an_opaque_tab_frame_adds_the_stretched_image_and_a_dialog_back_inside_it() {
    let (s, _w) = made(EeWonShellKind::WonTabCtrl, 0, 0);
    let f = EeWonFrameIn { left: 2, top: 3, right: 4, bottom: 5, image: 0x51 };
    // SAFETY: made above
    unsafe {
        ee_won_shell_op(s, EeWonShellOp::SetFrame, &f as *const EeWonFrameIn as isize, 0);
    }
    let t = take();
    let ops: Vec<&str> = t.iter().map(|l| l.split(' ').next().unwrap()).collect();
    assert_eq!(
        ops,
        ["Img", "NewStretchImage", "Ref", "TabLayoutPoint", "Layout", "Call", "New", "Ref", "Layout", "Call", "Ref"]
    );
    assert_eq!(t[1], "NewStretchImage 7 0 0 0");
    assert_eq!(t[6], format!("New {} 0 0 0", EeWonShellKind::MsDlgBack as isize));
    assert_eq!(t[10], "Ref 7 -1 0 0");
    // SAFETY: made above
    unsafe {
        // transparent: the back goes (the flag is never stored)
        ee_won_shell_op(s, EeWonShellOp::SetTransparent, 1, 0);
        assert_eq!(take().len(), 1);
        ee_won_shell_free(s);
    }
    take();
}

#[test]
fn a_simple_buttons_layout_reads_its_state_images() {
    let (s, _w) = made(EeWonShellKind::WonSimpleButton, 0, 0);
    // the constructor: ButtonFlag_InvalidateOnMouseOver
    assert_eq!(take(), [format!("Btn {} 1 1 0", crate::btnobj::EeWonBtnCall::SetButtonFlags as isize)]);
    // SAFETY: made above
    unsafe {
        ee_won_shell_op(s, EeWonShellOp::SetImage, 2, 0x60);
        assert_eq!(take(), [format!("Img {} 96 0 0", I::Hold as isize)]);
        let me = &*s;
        let mut wh = [0i32; 2];
        let r = me.imp_skin_op(crate::skinlay::EeWonSkinLay::Image as isize, 2, wh.as_mut_ptr() as isize, 0);
        assert_eq!((r, wh), (Some(1), [7, 7]));
        let r = me.imp_skin_op(crate::skinlay::EeWonSkinLay::Image as isize, 1, wh.as_mut_ptr() as isize, 0);
        assert_eq!(r, Some(0));
        take();
        ee_won_shell_free(s);
        assert_eq!(take(), [format!("Img {} 96 0 0", I::Release as isize)]);
    }
}
