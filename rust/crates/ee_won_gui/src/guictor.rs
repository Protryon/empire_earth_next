//! The GUI components' constructors (R6 WONLobby round 30): GuiHost.cpp's TextArea, InputBox, Label, Splitter,
//! ScrollArea, ListArea, MultiListArea, MultiListHeader, TreeArea and PopupMenu constructors, their statements in
//! order (after the Rust object is made and stored in its member, so a statement that reaches the object finds
//! it), each one op of the component's host (`EeWonGuiOp` from `CtorTextColor` on; the tree's `TCall(Init)` its Rust
//! object's own call). The one decision (the text area's link font underlined when it is the window manager's
//! default) is here.

use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp as Op};
use std::ffi::c_void;

/// The component class being constructed.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiCtor {
    TextArea = 1,
    InputBox,
    Label,
    Splitter,
    ScrollArea,
    ListArea,
    /// the column count the argument
    MultiListArea,
    /// the list area the argument
    MultiListHeader,
    TreeArea,
    PopupMenu,
}

/// ComponentFlag_WantFocus, ComponentFlag_Clip (ee/wonapi/WONComponentFlags.h)
const WANT_FOCUS: isize = 0x0008;
const CLIP: isize = 0x2000;
/// EeWonGuiCursor::IBeam
const IBEAM: isize = crate::guihost::EeWonGuiCursor::IBeam as isize;

/// The ops of a constructor, in the original's order (`(op, a)`); the text area's link font is decided between.
fn steps(kind: EeWonGuiCtor) -> &'static [(Op, isize)] {
    match kind {
        EeWonGuiCtor::TextArea => &[(Op::CtorTextColor, 0), (Op::CtorFontNamed, 0), (Op::CtorLinkColor, 0), (Op::CtorLinkFontNamed, 0)],
        EeWonGuiCtor::InputBox => &[
            (Op::CtorFontNamed, 0),
            (Op::CtorSetFlags, 0x4008),
            (Op::CtorTextColor, 0),
            (Op::CtorDisabledTextColor, 0),
            (Op::CtorDisabledBackground, 0),
            (Op::CtorUseDisabledColors, 0),
            (Op::SetDefaultCursor, IBEAM),
            (Op::CtorNoFilter, 0),
        ],
        EeWonGuiCtor::Label => &[
            (Op::CtorWrapNew, 0),
            (Op::CtorFontNamed, 0),
            (Op::WrapSetWrap, 0),
            (Op::CtorSetFlags, CLIP),
            (Op::CtorBackgroundNone, 0),
        ],
        EeWonGuiCtor::Splitter => &[(Op::CtorSetFlags, CLIP)],
        EeWonGuiCtor::ScrollArea => &[(Op::CtorViewportNew, 0), (Op::CtorAddViewport, 0)],
        EeWonGuiCtor::ListArea => &[
            (Op::CtorTextColor, 0),
            (Op::CtorFontNamed, 0),
            (Op::CtorSetFlags, WANT_FOCUS),
            (Op::CtorPads, 2),
            (Op::CtorNoSortFunc, 0),
        ],
        EeWonGuiCtor::MultiListArea => &[(Op::CtorMultiColumns, 0)],
        EeWonGuiCtor::MultiListHeader => &[(Op::CtorSetFlags, CLIP), (Op::CtorHeaderArea, 0)],
        EeWonGuiCtor::TreeArea => &[],
        EeWonGuiCtor::PopupMenu => &[
            (Op::CtorMenuFont, 0),
            (Op::CtorMenuListFlags, 0),
            (Op::CtorMenuBackground, 0),
            (Op::CtorMenuTextColor, 0),
            (Op::CtorMenuBorderPadding, 0),
        ],
    }
}

/// Runs a constructor's statements over the host.
fn construct(made: *mut c_void, component: *mut c_void, host: EeWonGuiHostFn, kind: EeWonGuiCtor, arg: isize) {
    if kind == EeWonGuiCtor::TreeArea && !made.is_null() {
        // `TCall(EeWonTreeCall::Init)`: the tree's own Rust object
        // SAFETY: `made` is the tree's new Rust object (the contract)
        unsafe { crate::treeobj::ee_won_tree_call(made as *const crate::treeobj::EeWonTree, crate::treeobj::EeWonTreeCall::Init, 0, 0, 0) };
    }
    let Some(h) = host else { return };
    // SAFETY: the component's own host, called as its Rust object calls it
    let op = |op: Op, a: isize| unsafe { h(component, op, a, 0, 0, 0) };
    for &(o, a) in steps(kind) {
        // the constructor's argument where the statement takes it (the columns, the list area)
        let a = if matches!(o, Op::CtorMultiColumns | Op::CtorHeaderArea) { arg } else { a };
        op(o, a);
    }
    if kind == EeWonGuiCtor::TextArea {
        if op(Op::CtorLinkFontIsDefault, 0) != 0 {
            op(Op::CtorLinkFontUnderline, 0);
        }
        op(Op::CtorSetFlags, WANT_FOCUS);
        op(Op::CtorPads, 5);
        op(Op::CtorReset, 0);
    }
}

/// A component's constructor: `made` (its new Rust object) stored at `slot` (the C++ member), then the class's
/// statements through `host`.
///
/// # Safety
/// `slot` the component's member for its Rust object; `component` the C++ component `host` answers for.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_construct(
    slot: *mut *mut c_void,
    made: *mut c_void,
    component: *mut c_void,
    host: EeWonGuiHostFn,
    kind: EeWonGuiCtor,
    arg: isize,
) {
    if !slot.is_null() {
        // SAFETY: the contract
        unsafe { *slot = made };
    }
    construct(made, component, host, kind, arg);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    thread_local! {
        static LOG: RefCell<Vec<(Op, isize)>> = const { RefCell::new(Vec::new()) };
        static DEFAULT_FONT: RefCell<bool> = const { RefCell::new(false) };
    }

    unsafe extern "C" fn host(_: *mut c_void, op: Op, a: isize, _: isize, _: isize, _: isize) -> isize {
        LOG.with(|l| l.borrow_mut().push((op, a)));
        (op == Op::CtorLinkFontIsDefault && DEFAULT_FONT.with(|d| *d.borrow())) as isize
    }

    fn run(kind: EeWonGuiCtor, arg: isize) -> Vec<(Op, isize)> {
        let mut slot: *mut c_void = std::ptr::null_mut();
        let made = 0x40 as *mut c_void;
        // SAFETY: a local slot; the test host
        unsafe { ee_won_gui_construct(&mut slot, made, 0x10 as *mut c_void, Some(host), kind, arg) };
        assert_eq!(slot, made);
        LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
    }

    #[test]
    fn the_text_area_underlines_a_default_link_font_only() {
        DEFAULT_FONT.with(|d| *d.borrow_mut() = false);
        let plain = run(EeWonGuiCtor::TextArea, 0);
        assert_eq!(plain.len(), 8);
        assert!(!plain.iter().any(|s| s.0 == Op::CtorLinkFontUnderline));
        assert_eq!(plain.last(), Some(&(Op::CtorReset, 0)));
        DEFAULT_FONT.with(|d| *d.borrow_mut() = true);
        let underlined = run(EeWonGuiCtor::TextArea, 0);
        assert_eq!(underlined[5], (Op::CtorLinkFontUnderline, 0));
        assert_eq!(underlined[6], (Op::CtorSetFlags, WANT_FOCUS));
    }

    #[test]
    fn the_arguments_reach_their_statements() {
        assert_eq!(run(EeWonGuiCtor::MultiListArea, 7), [(Op::CtorMultiColumns, 7)]);
        assert_eq!(run(EeWonGuiCtor::MultiListHeader, 0x99), [(Op::CtorSetFlags, CLIP), (Op::CtorHeaderArea, 0x99)]);
        let input = run(EeWonGuiCtor::InputBox, 0);
        assert_eq!(input[1], (Op::CtorSetFlags, 0x4008));
        assert_eq!(input[6], (Op::SetDefaultCursor, IBEAM));
        assert_eq!(run(EeWonGuiCtor::ListArea, 0)[3], (Op::CtorPads, 2));
    }
}
