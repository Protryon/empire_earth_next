//! The MS composite kinds' statements against the deleted classes' (written out as the calls each made);
//! tests/won/won_shell_test.cpp runs the original statements themselves against the kinds.

use std::ffi::c_void;

use crate::registry::EeWonGuiKind as G;
use crate::shell::tests::{host, take};
use crate::shell::*;

fn made(kind: EeWonShellKind, a: isize) -> (*mut EeWonShell, crate::compmem::FilledWorld) {
    let w = crate::compmem::filled_world(|_| crate::compmem::block(0, 0, 40, 20));
    let s = ee_won_shell_new(kind, 0x5e30 as *mut c_void, std::ptr::null_mut(), Some(host));
    take();
    // SAFETY: the shell just made
    unsafe { ee_won_shell_init(s, a, 0, 0) };
    (s, w)
}
fn call(p: isize, kind: G, k: isize, a: isize, b: isize) -> String {
    format!("Call {p} {} {a} {b}", ((kind as isize) << 16) | k)
}

#[test]
fn the_masks_and_names() {
    for (k, name, over) in [
        (EeWonShellKind::MsComboBox, "MSComboBox", 0b111),
        (EeWonShellKind::MsScrollbar, "MSScrollbar", 0b100),
        (EeWonShellKind::MsScrollContainer, "MSScrollContainer", 0b101),
        (EeWonShellKind::MsListCtrl, "MSListCtrl", 0b101),
        (EeWonShellKind::MsHeaderControl, "MSHeaderControl", 0),
        (EeWonShellKind::MsTabCtrl, "MSTabCtrl", 0),
        (EeWonShellKind::MsEtchedFrameGroup, "MSEtchedFrameGroup", 0b110 | bit(EeWonShellVirt::AddedToParent)),
    ] {
        let s = ee_won_shell_new(k, 0x5e31 as *mut c_void, std::ptr::null_mut(), Some(host));
        // SAFETY: the shell just made
        unsafe {
            assert_eq!(std::ffi::CStr::from_ptr(ee_won_shell_name(s)).to_str().unwrap(), name);
            assert_eq!(ee_won_shell_overrides(s), over, "{name}");
            ee_won_shell_free(s);
        }
    }
    take();
}

#[test]
fn a_scroll_containers_frame_change_lays_it_out_again() {
    let (s, _w) = made(EeWonShellKind::MsScrollContainer, 0x77);
    // its two new bars (the host answers 7 for each new widget), then the area
    let sc = |k: crate::scrollerobj::EeWonScrollerCall, a, b| call(0x5e30, G::Scroller, k as isize, a, b);
    use crate::scrollerobj::EeWonScrollerCall as Sc;
    let new = |k: EeWonShellKind, a: isize| format!("New {} {a} 0 0", k as isize);
    assert_eq!(
        take(),
        [
            new(EeWonShellKind::MsScrollbar, 1),
            sc(Sc::SetBar, 1, 7),
            new(EeWonShellKind::MsScrollbar, 0),
            sc(Sc::SetBar, 0, 7),
            sc(Sc::SetScrollArea, 0x77, 0),
        ]
    );
    // SAFETY: made above
    unsafe {
        // on by the constructor: no change; off: a relayout through the vtable
        ee_won_shell_op(s, EeWonShellOp::SetDraw3DFrame, 1, 0);
        assert!(take().is_empty());
        ee_won_shell_op(s, EeWonShellOp::SetDraw3DFrame, 0, 0);
        assert_eq!(take(), [format!("SelfVirt {} 0 0 0", EeWonShellVirt::SizeChanged as isize)]);
        ee_won_shell_free(s);
    }
}

#[test]
fn a_header_column_is_a_new_header_button_held_over_the_call() {
    let (s, _w) = made(EeWonShellKind::MsHeaderControl, 0);
    take();
    // SAFETY: made above
    unsafe {
        ee_won_shell_op(s, EeWonShellOp::SetColumn, 2, 0x99);
        assert_eq!(
            take(),
            [
                format!("New {} {} 0 0", EeWonShellKind::MsHeaderButton as isize, 0x99),
                "Ref 7 1 0 0".to_string(),
                call(0x5e30, G::MHead, crate::multiobj::EeWonMHeadCall::SetColumnItem as isize, 2, 7),
                "Ref 7 -1 0 0".to_string(),
            ]
        );
        ee_won_shell_free(s);
    }
}

#[test]
fn the_etched_group_holds_its_label_and_component() {
    let (s, _w) = made(EeWonShellKind::MsEtchedFrameGroup, 0);
    let label_align = call(7, G::Label, crate::labelobj::EeWonLabelCall::SetHorzAlign as isize, 1, 0);
    assert_eq!(
        take(),
        [format!("NewBase {} 0 0 0", EeWonShellBaseNew::Label as isize), "Ref 7 1 0 0".into(), label_align]
    );
    // SAFETY: made above
    unsafe {
        ee_won_shell_op(s, EeWonShellOp::AddContainedComponent, 0x42, 0);
        assert_eq!(take(), ["Ref 66 1 0 0"]);
        assert_eq!(ee_won_shell_op(s, EeWonShellOp::GetLabel, 0, 0), 7);
        // the members in reverse order
        ee_won_shell_free(s);
        assert_eq!(take(), ["Ref 66 -1 0 0", "Ref 7 -1 0 0"]);
    }
}
