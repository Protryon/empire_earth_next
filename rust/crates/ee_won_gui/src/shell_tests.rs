//! The shell's FFI and its view's routing (the kinds' own statements: shell_ms_tests.rs).

use super::*;
use crate::guidraw::EE_WON_DRAW_SET_SYS_COLOR;
use std::cell::RefCell;

thread_local! {
    pub(crate) static TRACE: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

/// A scripted host: every op traced; a read answers 7.
pub(crate) unsafe extern "C" fn host(
    _me: *mut c_void,
    op: EeWonShellHostOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    let line = if op == EeWonShellHostOp::Draw {
        // SAFETY: the op's draw operation
        let o = unsafe { &*(b as *const EeWonDrawOp) };
        format!("Draw {} {} {} {} {}", o.kind, o.a, o.b, o.c, o.d)
    } else {
        format!("{op:?} {a} {b} {c} {d}")
    };
    TRACE.with(|t| t.borrow_mut().push(line));
    7
}

pub(crate) fn take() -> Vec<String> {
    TRACE.with(|t| std::mem::take(&mut *t.borrow_mut()))
}

#[test]
fn the_kinds_bases_and_names() {
    let s = ee_won_shell_new(EeWonShellKind::MsCheckbox, 0x5e11 as *mut c_void, std::ptr::null_mut(), Some(host));
    // SAFETY: the shell just made
    unsafe {
        assert_eq!(CStr::from_ptr(ee_won_shell_name(s)).to_str().unwrap(), "MSCheckbox");
        assert_eq!(ee_won_shell_kind(s), EeWonShellKind::MsCheckbox);
        assert_eq!(ee_won_shell_overrides(s), bit(EeWonShellVirt::Paint));
        // an op of another kind does nothing
        assert_eq!(ee_won_shell_op(s, EeWonShellOp::SetDirection, 3, 0), 0);
        ee_won_shell_free(s);
        ee_won_shell_free(std::ptr::null_mut());
    }
    assert_eq!(ee_won_shell_base(EeWonShellKind::MsInputBox), EeWonShellBase::Input);
    assert_eq!(ee_won_shell_base(EeWonShellKind::MaxSplitterButton), EeWonShellBase::Button);
    assert_eq!(ee_won_shell_base(EeWonShellKind::MsLabel), EeWonShellBase::MsButton);
    assert_eq!(ee_won_shell_base(EeWonShellKind::MsDlgBack), EeWonShellBase::Component);
    take();
}

#[test]
fn without_the_game_every_call_is_a_host_op() {
    let sh = Sh { comp: 0x5e12, me: std::ptr::null_mut(), host: Some(host) };
    take();
    sh.set_flags(8, true);
    sh.set_button_flags(0x400, false);
    sh.msb_set(EeWonMsBtnField::LeftTextPad, 8);
    assert_eq!(sh.base(EeWonShellVirt::Paint, 5, 0, 0), 7);
    assert_eq!(
        take(),
        [
            format!("Comp {} 8 1 0", EeWonCompCall::SetFlags as isize),
            format!("Btn {} 1024 0 0", EeWonBtnCall::SetButtonFlags as isize),
            format!("MsBtn {} {} 8 0", EeWonMsBtnCall::Set as isize, EeWonMsBtnField::LeftTextPad as isize),
            "Base 2 5 0 0".to_string(),
        ]
    );
}

#[test]
fn an_ms_paint_runs_its_plan_and_its_text_call() {
    let sh = Sh { comp: 0x5e13, me: std::ptr::null_mut(), host: Some(host) };
    take();
    // the label's plan: the face unless transparent, then the text
    sh.ms_paint(9, &ms_paint_of(EeWonMsPaintKind::Label, 30, 20, 0, 0, 0, 0), true);
    let t = take();
    assert!(t[0].starts_with(&format!("Draw {EE_WON_DRAW_SET_SYS_COLOR} ")), "{t:?}");
    let text = format!("MsBtn {} 9 0 0", EeWonMsBtnCall::DrawButtonText as isize);
    assert_eq!(t.last(), Some(&text));
    // without the text, the call is dropped
    sh.ms_paint(9, &ms_paint_of(EeWonMsPaintKind::Label, 30, 20, 0, 0, 0, 0), false);
    assert_eq!(take().len(), t.len() - 1);
    assert_eq!(bit(EeWonShellVirt::SetVisible), 1 << 19);
    // SAFETY: NULL is no text
    assert_eq!(unsafe { gui_string_len(0) }, 0);
}
