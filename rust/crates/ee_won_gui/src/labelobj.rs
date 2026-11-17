//! WONGUI's Label as a whole Rust object (R6 WON round 22): every lobby screen's captions, the status and detail
//! lines, the dialogs' messages. The original's Label.cpp with its state: the text colour, the two alignments, the
//! padding and the fit-to-text flag.
//!
//! The C++ `Label` stays the type the lobby holds (a `Component` in the tree, its vtable); its virtuals and methods
//! are one-line forwarders to `ee_won_label_call`, and every call out is an op of the component op table
//! (`guihost`). The C++ keeps the wrapped text (`WrappedText`: the text, its font and its wrap, which the test
//! fixture and the lobby read) and the background; `SetText` and `SetFont` set the wrapped text in C++ before the
//! Rust steps.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::{self, EeWonDrawOp};
use crate::guihost::{EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};

/// `HorzAlign_Center`.
const HORZ_ALIGN_CENTER: i32 = 1;

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLabelCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `GetDesiredSize(*a, *b)` (`a`, `b` point at i32s).
    DesiredSize,
    /// `Paint(*a)` (`a` the Graphics).
    Paint,
    /// `SetText`'s steps after the C++ set the wrapped text: refit, and repaint when `a != 0`.
    TextSet,
    /// `SetHorzAlign(a)`.
    SetHorzAlign,
    /// `SetVertAlign(a)`.
    SetVertAlign,
    /// `SetWrap(a != 0)`.
    SetWrap,
    /// `SetAdjustHeightToFitText(a != 0)`.
    SetAdjustHeightToFitText,
    /// `SetVertPad(a, b)`.
    SetVertPad,
    /// `SetTextColor(a)`.
    SetTextColor,
    /// `mColor`.
    TextColor,
}

#[derive(Debug)]
struct State {
    color: i32,
    h_align: i32,
    v_align: i32,
    left_pad: i32,
    right_pad: i32,
    top_pad: i32,
    bottom_pad: i32,
    adjust_height: bool,
}

/// The Rust Label.
pub struct EeWonLabel {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonLabel {
    /// The constructor's state (the C++ constructor makes the wrapped text, sets its font and no wrap, the clip
    /// flag and the background).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonLabel {
            comp,
            host,
            st: RefCell::new(State {
                color: 0x80000005_u32 as i32,
                h_align: 0,
                v_align: 0,
                left_pad: 0,
                right_pad: 0,
                top_pad: 0,
                bottom_pad: 0,
                adjust_height: false,
            }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, 0) },
            None => 0,
        }
    }
    fn width(&self) -> i32 {
        self.op(Op::Get, F::Width as isize, 0, 0) as i32
    }
    fn height(&self) -> i32 {
        self.op(Op::Get, F::Height as isize, 0, 0) as i32
    }
    fn text_height(&self) -> i32 {
        self.op(Op::WrapHeight, 0, 0, 0) as i32
    }

    pub fn set_max_width(&self) {
        let (l, r) = {
            let s = self.st.borrow();
            (s.left_pad, s.right_pad)
        };
        let mut w = self.width().wrapping_sub(l).wrapping_sub(r);
        if w < 0 {
            w = 5;
        }
        self.op(Op::WrapSetMaxWidth, w as isize, 0, 0);
        self.adjust_height_to_fit_text();
    }

    pub fn size_changed(&self) {
        self.op(Op::BaseSizeChanged, 0, 0, 0);
        self.set_max_width();
    }

    pub fn set_horz_align(&self, align: i32) {
        self.st.borrow_mut().h_align = align;
        self.op(Op::WrapSetCentered, (align == HORZ_ALIGN_CENTER) as isize, 0, 0);
    }

    pub fn text_set(&self, invalidate: bool) {
        self.adjust_height_to_fit_text();
        if invalidate {
            self.op(Op::Invalidate, 0, 0, 0);
        }
    }

    pub fn adjust_height_to_fit_text(&self) {
        let (adjust, t, b) = {
            let s = self.st.borrow();
            (s.adjust_height, s.top_pad, s.bottom_pad)
        };
        if !adjust {
            return;
        }
        if self.height().wrapping_sub(t).wrapping_sub(b) != self.text_height() {
            let w = self.width();
            let h = self.text_height().wrapping_add(t).wrapping_add(b);
            self.op(Op::SetSize, w as isize, h as isize, 0);
        }
    }

    pub fn set_adjust_height_to_fit_text(&self, adjust: bool) {
        if adjust == self.st.borrow().adjust_height {
            return;
        }
        self.st.borrow_mut().adjust_height = adjust;
        self.adjust_height_to_fit_text();
    }

    pub fn set_wrap(&self, wrap: bool) {
        self.op(Op::WrapSetWrap, wrap as isize, 0, 0);
        self.adjust_height_to_fit_text();
    }

    pub fn desired_size(&self) -> (i32, i32) {
        let (l, r, t, b) = {
            let s = self.st.borrow();
            (s.left_pad, s.right_pad, s.top_pad, s.bottom_pad)
        };
        let w = (self.op(Op::WrapWidth, 0, 0, 0) as i32).wrapping_add(l).wrapping_add(r);
        let h = self.text_height().wrapping_add(t).wrapping_add(b);
        (w, h)
    }

    pub fn paint(&self, g: isize) {
        self.op(Op::BasePaint, g, 0, 0);
        let (w, h) = (self.width(), self.height());
        self.op(Op::PaintBackground, g, w as isize, h as isize);
        let tw = self.op(Op::WrapWidth, 0, 0, 0) as i32;
        let th = self.text_height();
        let ops: Vec<EeWonDrawOp> = {
            let s = self.st.borrow();
            guidraw::label(
                w,
                h,
                [s.left_pad, s.top_pad, s.right_pad, s.bottom_pad],
                s.h_align,
                s.v_align,
                tw,
                th,
                s.color,
            )
        };
        for o in &ops {
            self.op(Op::Draw, g, o as *const EeWonDrawOp as isize, 0);
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonLabelCall, a: isize, b: isize, _c: isize) -> isize {
        type C = EeWonLabelCall;
        let (ai, bi) = (a as i32, b as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::DesiredSize => {
                let (w, h) = self.desired_size();
                // SAFETY: the C++ passes two live ints
                unsafe {
                    *(a as *mut i32) = w;
                    *(b as *mut i32) = h;
                }
            }
            C::Paint => self.paint(a),
            C::TextSet => self.text_set(a != 0),
            C::SetHorzAlign => self.set_horz_align(ai),
            C::SetVertAlign => self.st.borrow_mut().v_align = ai,
            C::SetWrap => self.set_wrap(a != 0),
            C::SetAdjustHeightToFitText => self.set_adjust_height_to_fit_text(a != 0),
            C::SetVertPad => {
                let mut s = self.st.borrow_mut();
                s.top_pad = ai;
                s.bottom_pad = bi;
            }
            C::SetTextColor => self.st.borrow_mut().color = ai,
            C::TextColor => return self.st.borrow().color as isize,
        }
        0
    }
}

// ---- the C ABI

/// A new Rust Label for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_label_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonLabel {
    Box::into_raw(Box::new(EeWonLabel::new(component, host)))
}

/// # Safety
/// `label` came from `ee_won_label_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_label_free(label: *mut EeWonLabel) {
    if !label.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(label) });
    }
}

/// One of Label's methods (`EeWonLabelCall`).
///
/// # Safety
/// `label` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_label_call(
    label: *const EeWonLabel,
    call: EeWonLabelCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*label).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "labelobj_tests.rs"]
mod tests;
