//! The MS kinds' statements against the deleted classes' (MSControlConstructors.cpp, MSRemainingPainters.cpp,
//! MSPainters.cpp, MSBorderPainters.cpp, MenuSeparator.cpp, CrossPromotionButton.cpp), written out as the calls
//! each made. tests/won/won_shell_test.cpp runs the original statements themselves against the kinds.

use std::ffi::c_void;

use crate::btnobj::EeWonBtnCall as Bc;
use crate::compobj::EeWonCompCall as Cc;
use crate::msbtnobj::{EeWonMsBtnCall as M, EeWonMsBtnField as Mf};
use crate::shell::tests::{host, take};
use crate::shell::*;

fn comp(k: Cc, a: isize, b: isize) -> String {
    format!("Comp {} {a} {b} 0", k as isize)
}
fn btn(k: Bc, a: isize, b: isize) -> String {
    format!("Btn {} {a} {b} 0", k as isize)
}
fn msb(k: M, a: isize, b: isize, c: isize) -> String {
    format!("MsBtn {} {a} {b} {c}", k as isize)
}
fn set(f: Mf, v: isize) -> String {
    msb(M::Set, f as isize, v, 0)
}

/// A shell of `kind` made with `args` over the scripted host, on a 40 x 20 component.
fn made(kind: EeWonShellKind, args: [isize; 3]) -> (*mut EeWonShell, crate::compmem::FilledWorld) {
    let w = crate::compmem::filled_world(|_| crate::compmem::block(0, 0, 40, 20));
    let s = ee_won_shell_new(kind, 0x5e20 as *mut c_void, std::ptr::null_mut(), Some(host));
    take();
    // SAFETY: the shell just made
    unsafe { ee_won_shell_init(s, args[0], args[1], args[2]) };
    (s, w)
}

#[test]
fn the_buttons_constructors() {
    let (s, _w) = made(EeWonShellKind::MsButton, [0, 0, 0]);
    let init = vec![
        set(Mf::LeftTextPad, 8),
        set(Mf::RightTextPad, 8),
        set(Mf::TopTextPad, 4),
        set(Mf::BottomTextPad, 4),
        comp(Cc::SetFlags, 8, 1),
        btn(Bc::SetButtonFlags, 0x400, 1),
    ];
    assert_eq!(take(), init);
    // SAFETY: made above
    unsafe {
        ee_won_shell_init(s, 0x1234, 0, 0);
        let mut text = init.clone();
        text.push("WrapSetText 4660 0 0 0".into());
        text.push(comp(Cc::SetDesiredSize, 0, 0));
        assert_eq!(take(), text);
        ee_won_shell_free(s);
    }
    // the header button over MSButton(): its focus off, the pad, the left alignment; no text
    let (s, _w) = made(EeWonShellKind::MsHeaderButton, [0, 0, 0]);
    let mut header = init.clone();
    header.extend([comp(Cc::SetFlags, 8, 0), msb(M::IncrementHorzTextPad, 2, 0, 0), msb(M::SetHorzAlign, 0, 0, 0)]);
    assert_eq!(take(), header);
    // SAFETY: made above
    unsafe { ee_won_shell_free(s) };
}

#[test]
fn the_labels_and_tabs_constructors() {
    let (s, _w) = made(EeWonShellKind::MsLabel, [0, 0, 0]);
    let pads = [
        msb(M::SetStandardTextColor, 14, 0, 0),
        set(Mf::LeftTextPad, 0),
        set(Mf::RightTextPad, 0),
        set(Mf::TopTextPad, 0),
        set(Mf::BottomTextPad, 0),
    ];
    let mut d = pads.to_vec();
    d.extend([set(Mf::HAlign, 0), btn(Bc::SetButtonFlags, 0x200, 1)]);
    assert_eq!(take(), d);
    // SAFETY: made above
    unsafe {
        ee_won_shell_init(s, 0x55, 0, 0);
        let mut t = pads.to_vec();
        t.extend([btn(Bc::SetButtonFlags, 0x200, 1), btn(Bc::SetText, 0x55, 1), comp(Cc::SetDesiredSize, 0, 0)]);
        assert_eq!(take(), t);
        ee_won_shell_free(s);
    }
    // MSTabButton("", fill, 3): no text; the field alone, the press flags, centred down
    let (s, _w) = made(EeWonShellKind::MsTabButton, [0, 1, 3]);
    assert_eq!(take(), [set(Mf::Transparent, 0), btn(Bc::SetButtonFlags, 0x48, 1), set(Mf::VAlign, 1)]);
    // its desired size: the base's plus 16 by 5 and the lower amount (the op's)
    // SAFETY: made above; two live ints
    unsafe {
        ee_won_shell_op(s, EeWonShellOp::SetLowerAmount, 4, 0);
        let (mut w, mut h) = (10i32, 11i32);
        let (pw, ph) = (&mut w as *mut i32 as isize, &mut h as *mut i32 as isize);
        ee_won_shell_virtual(s, EeWonShellVirt::DesiredSize, pw, ph, 0);
        assert_eq!((w, h), (26, 20));
        assert_eq!(take(), [format!("Base 1 {pw} {ph} 0")]);
        ee_won_shell_free(s);
    }
}

#[test]
fn the_arrows_frames_and_plain_ones() {
    let (s, _w) = made(EeWonShellKind::MaxSplitterButton, [1, 0, 0]);
    assert_eq!(take(), [btn(Bc::SetButtonFlags, 2, 1), comp(Cc::SetSize, 15, 15), btn(Bc::SetButtonFlags, 2, 0)]);
    // SAFETY: made above
    unsafe { ee_won_shell_free(s) };
    let (s, _w) = made(EeWonShellKind::Ms3DFrame, [1, 1, 0]);
    assert_eq!(take(), [comp(Cc::SetFlags, 0x800, 1)]);
    // SAFETY: made above
    unsafe { ee_won_shell_free(s) };
    let (s, _w) = made(EeWonShellKind::MsCloseButton, [0, 0, 0]);
    assert_eq!(take(), [comp(Cc::SetSize, 16, 14)]);
    // SAFETY: made above
    unsafe { ee_won_shell_free(s) };
    let (s, _w) = made(EeWonShellKind::MsSeperator, [6, 0, 0]);
    assert_eq!(take(), [comp(Cc::SetSize, 40, 6)]);
    // SAFETY: made above; the paint: the base's, then two etched lines across the middle
    unsafe {
        ee_won_shell_virtual(s, EeWonShellVirt::Paint, 3, 0, 0);
        assert_eq!(
            take(),
            ["Base 2 3 0 0", "Draw 2 3 0 0 0", "Draw 6 0 9 39 9", "Draw 2 2 0 0 0", "Draw 6 0 10 39 10"]
        );
        let (mut w, mut h) = (0i32, 0i32);
        ee_won_shell_virtual(
            s,
            EeWonShellVirt::DesiredSize,
            &mut w as *mut i32 as isize,
            &mut h as *mut i32 as isize,
            0,
        );
        assert_eq!((w, h), (100, 6));
        ee_won_shell_free(s);
    }
    for (k, init) in
        [(EeWonShellKind::MsEtchedFrame, 1), (EeWonShellKind::MsDlgBack, 0), (EeWonShellKind::MsInputBox, 0)]
    {
        let (s, _w) = made(k, [0, 0, 0]);
        assert_eq!(take().len(), init, "{k:?}");
        // SAFETY: made above
        unsafe {
            ee_won_shell_virtual(s, EeWonShellVirt::Paint, 3, 0, 0);
            let t = take();
            assert_eq!(t[0], "Base 2 3 0 0", "{k:?}");
            assert!(t.len() > 1 && t[1..].iter().all(|l| l.starts_with("Draw ")), "{k:?} {t:?}");
            ee_won_shell_free(s);
        }
    }
}

#[test]
fn the_boxes_are_the_ms_button_bases() {
    let (s, _w) = made(EeWonShellKind::MsRadioButton, [0, 0, 0]);
    assert_eq!(take(), [msb(M::BoxInit, 1, 13, 0)]);
    // SAFETY: made above
    unsafe {
        ee_won_shell_virtual(s, EeWonShellVirt::Paint, 3, 0, 0);
        assert_eq!(take(), [msb(M::BoxPaint, 1, 3, 13)]);
        ee_won_shell_free(s);
    }
    // the push button's paint reads the press, transparency and default states (each read answers 7 here: not
    // pressed, transparent, default) then draws, its text through the base's DrawButtonText
    let (s, _w) = made(EeWonShellKind::MsButton, [0, 0, 0]);
    take();
    // SAFETY: made above
    unsafe {
        ee_won_shell_virtual(s, EeWonShellVirt::Paint, 3, 0, 0);
        let t = take();
        assert_eq!(
            t[..4],
            [
                "Base 2 3 0 0".to_string(),
                btn(Bc::GetField, 1, 0),
                msb(M::Get, Mf::Transparent as isize, 0, 0),
                btn(Bc::IsDefaultButton, 0, 0)
            ]
        );
        assert_eq!(t.last(), Some(&msb(M::DrawButtonText, 3, 0, 0)));
        ee_won_shell_free(s);
    }
}
