//! The Windows skin's button base, MSButtonBase, as a whole Rust object (R6 WON round 35): every push button, tab,
//! list header, check box, radio button and MS label of the lobby's Windows look. The original's MSButtonBase.cpp
//! with its state: the standard and own text colours, the two alignments, the focus style, the four text pads, the
//! transparent and fit-to-text flags.
//!
//! The C++ `MSButtonBase` stays the type the lobby and its subclasses hold (a `Button` in the tree, its vtable); its
//! virtuals and methods are one-line forwarders to `ee_won_msbtn_call`, and every call out is an op of the
//! component op table (`guihost`). The C++ keeps the wrapped text `mText` (the fixture, the lobby, `GetText` and the
//! header button's paint read it): `SetTextHook` sets it in C++ before the Rust steps, and the text's own calls are
//! the `Wrap*` ops and the `Draw` op's text slot. The button's and component's methods are `SkinBtn` ops
//! (`EeWonSkinBtn`).

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guidraw::{
    EeWonDrawOp, EE_WON_DRAW_DOTTED_RECT, EE_WON_DRAW_SET_COLOR, EE_WON_DRAW_SET_SYS_COLOR, EE_WON_DRAW_TEXT,
};
use crate::guihost::{EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op, EeWonSkinBtn as Sb};

/// `HorzAlign_Center`, `HorzAlign_Right`; `VertAlign_Center`, `VertAlign_Bottom`.
const ALIGN_CENTER: u8 = 1;
const ALIGN_FAR: u8 = 2;
/// `FocusStyle_AroundWindow`, `FocusStyle_AroundText`.
const FOCUS_AROUND_WINDOW: u8 = 1;
const FOCUS_AROUND_TEXT: u8 = 2;
/// `StandardColor_3DHilight`, `_ButtonText`, `_GrayText`.
const SYS_3D_HILIGHT: i32 = 2;
const SYS_BUTTON_TEXT: i32 = 5;
const SYS_GRAY_TEXT: i32 = 6;

/// A field of the button base (`Get` / `Set` calls). A set stores what the original's member held: the
/// alignments, focus style and pads as `unsigned char`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonMsBtnField {
    /// `mStandardTextColor`.
    StandardTextColor = 1,
    /// `mTextColor` (-1: the standard colour).
    TextColor,
    /// `mHAlign` (the field alone; `SetHorzAlign` also centres the text).
    HAlign,
    /// `mVAlign`.
    VAlign,
    /// `mFocusStyle`.
    FocusStyle,
    /// `mTopTextPad`.
    TopTextPad,
    /// `mBottomTextPad`.
    BottomTextPad,
    /// `mLeftTextPad`.
    LeftTextPad,
    /// `mRightTextPad`.
    RightTextPad,
    /// `mIsTransparent` (the flag alone; `SetTransparent` also grabs the background).
    Transparent,
    /// `mAdjustHeightToFitText` (the flag alone).
    AdjustHeight,
}

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonMsBtnCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `GetDesiredSize(*a, *b)` (`a`, `b` point at i32s).
    DesiredSize,
    /// `SetTextHook`'s step after the C++ set the text: `AdjustHeightToFitText`.
    TextSet,
    /// `SetTransparent(a != 0)`.
    SetTransparent,
    /// `SetAdjustHeightToFitText(a != 0)`.
    SetAdjustHeightToFitText,
    /// `AdjustHeightToFitText()`.
    AdjustHeightToFitText,
    /// `DrawButtonText(*a, b, c)` (`a` the Graphics).
    DrawButtonText,
    /// the field `a` (`EeWonMsBtnField`).
    Get,
    /// the field `a` = `b`.
    Set,
    /// `SetHorzAlign(a)`: the field, then the text centred when it is `HorzAlign_Center`.
    SetHorzAlign,
    /// `SetStandardTextColor(a)`: the standard colour, the own colour -1.
    SetStandardTextColor,
    /// `IncrementHorzTextPad(a, b)` (each pad plus the low byte, as `unsigned char`).
    IncrementHorzTextPad,
    /// `IncrementVertTextPad(a, b)`.
    IncrementVertTextPad,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
struct State {
    standard_text_color: i32,
    text_color: i32,
    h_align: u8,
    v_align: u8,
    focus_style: u8,
    top_pad: u8,
    bottom_pad: u8,
    left_pad: u8,
    right_pad: u8,
    transparent: bool,
    adjust_height: bool,
}

/// The Rust MSButtonBase.
pub struct EeWonMsButton {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

fn draw_op(kind: i32, args: &[i32]) -> EeWonDrawOp {
    let mut o = EeWonDrawOp { kind, ..Default::default() };
    let slots = [&mut o.a, &mut o.b, &mut o.c, &mut o.d, &mut o.e, &mut o.f, &mut o.g, &mut o.h];
    for (s, v) in slots.into_iter().zip(args) {
        *s = *v;
    }
    o
}

impl EeWonMsButton {
    /// The constructor's state (the C++ constructor makes the wrapped text first, unwrapped, in the MSButton font):
    /// the button text colour, centred both ways, the focus around the window, 2-pixel pads.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonMsButton {
            comp,
            host,
            st: RefCell::new(State {
                standard_text_color: SYS_BUTTON_TEXT,
                text_color: -1,
                h_align: ALIGN_CENTER,
                v_align: ALIGN_CENTER,
                focus_style: FOCUS_AROUND_WINDOW,
                top_pad: 2,
                bottom_pad: 2,
                left_pad: 2,
                right_pad: 2,
                transparent: false,
                adjust_height: false,
            }),
        }
    }

    fn st(&self) -> State {
        *self.st.borrow()
    }
    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, 0) },
            None => 0,
        }
    }
    fn sb(&self, which: Sb, b: isize, c: isize) -> isize {
        self.op(Op::SkinBtn, which as isize, b, c)
    }
    fn width(&self) -> i32 {
        self.op(Op::Get, F::Width as isize, 0, 0) as i32
    }
    fn height(&self) -> i32 {
        self.op(Op::Get, F::Height as isize, 0, 0) as i32
    }
    fn text_width(&self) -> i32 {
        self.op(Op::WrapWidth, 0, 0, 0) as i32
    }
    fn text_height(&self) -> i32 {
        self.op(Op::WrapHeight, 0, 0, 0) as i32
    }
    fn draw(&self, g: isize, kind: i32, args: &[i32]) {
        let o = draw_op(kind, args);
        self.op(Op::Draw, g, &o as *const EeWonDrawOp as isize, 0);
    }

    /// `GetDesiredSize`: the text's size plus the pads.
    pub fn desired_size(&self) -> (i32, i32) {
        let s = self.st();
        let w = self.text_width().wrapping_add(s.left_pad as i32).wrapping_add(s.right_pad as i32);
        let h = self.text_height().wrapping_add(s.top_pad as i32).wrapping_add(s.bottom_pad as i32);
        (w, h)
    }

    /// `SetTransparent`: the flag; a transparent button grabs the background behind it.
    pub fn set_transparent(&self, transparent: bool) {
        self.st.borrow_mut().transparent = transparent;
        if transparent {
            self.sb(Sb::SetGrabBG, 0, 0);
        }
    }

    /// `AdjustHeightToFitText`: the height set to the text's plus the vertical pads, when fitting is on.
    pub fn adjust_height_to_fit_text(&self) {
        let s = self.st();
        if !s.adjust_height {
            return;
        }
        let (t, b) = (s.top_pad as i32, s.bottom_pad as i32);
        if self.height().wrapping_sub(t).wrapping_sub(b) != self.text_height() {
            let w = self.width();
            let h = self.text_height().wrapping_add(t).wrapping_add(b);
            self.op(Op::SetSize, w as isize, h as isize, 0);
        }
    }

    /// `SetAdjustHeightToFitText`: refits at once when the flag changes.
    pub fn set_adjust_height_to_fit_text(&self, adjust: bool) {
        if adjust == self.st().adjust_height {
            return;
        }
        self.st.borrow_mut().adjust_height = adjust;
        self.adjust_height_to_fit_text();
    }

    /// `Virtual08`: the base's, the text wrapped to the width inside the pads, then the height refitted.
    pub fn size_changed(&self) {
        self.op(Op::BaseSizeChanged, 0, 0, 0);
        let s = self.st();
        let w = self.width().wrapping_sub(s.left_pad as i32).wrapping_sub(s.right_pad as i32);
        self.op(Op::WrapSetMaxWidth, w as isize, 0, 0);
        self.adjust_height_to_fit_text();
    }

    /// The text colour: the standard one when the own colour is negative.
    fn text_color(&self, g: isize, s: &State) {
        if s.text_color < 0 {
            self.draw(g, EE_WON_DRAW_SET_SYS_COLOR, &[s.standard_text_color]);
        } else {
            self.draw(g, EE_WON_DRAW_SET_COLOR, &[s.text_color]);
        }
    }

    /// `DrawButtonText`: the text by the alignments inside the pads (embossed grey when disabled), and the focus
    /// rectangle around the window or the text once the parent has used the dialog keys.
    pub fn draw_button_text(&self, g: isize, xoff: i32, yoff: i32) {
        let text_width = self.text_width();
        let text_height = self.text_height();
        let s = self.st();
        let (l, t, r, b) = (s.left_pad as i32, s.top_pad as i32, s.right_pad as i32, s.bottom_pad as i32);
        let width = self.width().wrapping_sub(l).wrapping_sub(r);
        let height = self.height().wrapping_sub(t).wrapping_sub(b);
        let mut x = match s.h_align {
            ALIGN_CENTER => width.wrapping_sub(text_width) / 2,
            ALIGN_FAR => width.wrapping_sub(text_width),
            _ => 0,
        };
        let mut y = match s.v_align {
            ALIGN_CENTER => height.wrapping_sub(text_height) / 2,
            ALIGN_FAR => height.wrapping_sub(text_height),
            _ => 0,
        };
        x = x.wrapping_add(xoff.wrapping_add(l));
        y = y.wrapping_add(yoff.wrapping_add(t));
        if self.sb(Sb::IsDisabled, 0, 0) != 0 {
            self.draw(g, EE_WON_DRAW_SET_SYS_COLOR, &[SYS_3D_HILIGHT]);
            self.draw(g, EE_WON_DRAW_TEXT, &[0, x.wrapping_add(1), y.wrapping_add(1)]);
            self.draw(g, EE_WON_DRAW_SET_SYS_COLOR, &[SYS_GRAY_TEXT]);
        } else {
            self.text_color(g, &s);
        }
        self.draw(g, EE_WON_DRAW_TEXT, &[0, x, y]);
        if self.sb(Sb::HasFocus, 0, 0) == 0 {
            return;
        }
        let parent = self.sb(Sb::Parent, 0, 0);
        if parent == 0 || self.sb(Sb::UsedDialogKeys, parent, 0) == 0 {
            return;
        }
        // the state again: the parent's calls cannot reach this button's setters, but the original read its members
        // here
        let s = self.st();
        self.text_color(g, &s);
        match s.focus_style {
            FOCUS_AROUND_WINDOW => {
                let (w, h) = (self.width(), self.height());
                self.draw(g, EE_WON_DRAW_DOTTED_RECT, &[4, 3, w.wrapping_sub(8), h.wrapping_sub(7)]);
            }
            FOCUS_AROUND_TEXT => self.draw(
                g,
                EE_WON_DRAW_DOTTED_RECT,
                &[x.wrapping_sub(1), y.wrapping_sub(1), text_width.wrapping_add(2), text_height.wrapping_add(2)],
            ),
            _ => {}
        }
    }

    fn get(&self, f: EeWonMsBtnField) -> i32 {
        let s = self.st();
        type B = EeWonMsBtnField;
        match f {
            B::StandardTextColor => s.standard_text_color,
            B::TextColor => s.text_color,
            B::HAlign => s.h_align as i32,
            B::VAlign => s.v_align as i32,
            B::FocusStyle => s.focus_style as i32,
            B::TopTextPad => s.top_pad as i32,
            B::BottomTextPad => s.bottom_pad as i32,
            B::LeftTextPad => s.left_pad as i32,
            B::RightTextPad => s.right_pad as i32,
            B::Transparent => s.transparent as i32,
            B::AdjustHeight => s.adjust_height as i32,
        }
    }

    fn set(&self, f: EeWonMsBtnField, v: i32) {
        let mut s = self.st.borrow_mut();
        type B = EeWonMsBtnField;
        match f {
            B::StandardTextColor => s.standard_text_color = v,
            B::TextColor => s.text_color = v,
            B::HAlign => s.h_align = v as u8,
            B::VAlign => s.v_align = v as u8,
            B::FocusStyle => s.focus_style = v as u8,
            B::TopTextPad => s.top_pad = v as u8,
            B::BottomTextPad => s.bottom_pad = v as u8,
            B::LeftTextPad => s.left_pad = v as u8,
            B::RightTextPad => s.right_pad = v as u8,
            B::Transparent => s.transparent = v != 0,
            B::AdjustHeight => s.adjust_height = v != 0,
        }
    }

    /// `SetHorzAlign`: the field, then the text centred or not.
    pub fn set_horz_align(&self, align: i32) {
        self.st.borrow_mut().h_align = align as u8;
        let centered = self.st().h_align == ALIGN_CENTER;
        self.op(Op::WrapSetCentered, centered as isize, 0, 0);
    }

    /// One inward call.
    pub fn call(&self, call: EeWonMsBtnCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonMsBtnCall;
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
            C::TextSet | C::AdjustHeightToFitText => self.adjust_height_to_fit_text(),
            C::SetTransparent => self.set_transparent(a != 0),
            C::SetAdjustHeightToFitText => self.set_adjust_height_to_fit_text(a != 0),
            C::DrawButtonText => self.draw_button_text(a, bi, c as i32),
            C::Get => match field_of(ai) {
                Some(f) => return self.get(f) as isize,
                None => return 0,
            },
            C::Set => {
                if let Some(f) = field_of(ai) {
                    self.set(f, bi);
                }
            }
            C::SetHorzAlign => self.set_horz_align(ai),
            C::SetStandardTextColor => {
                let mut s = self.st.borrow_mut();
                s.standard_text_color = ai;
                s.text_color = -1;
            }
            C::IncrementHorzTextPad => {
                let mut s = self.st.borrow_mut();
                s.left_pad = s.left_pad.wrapping_add(ai as u8);
                s.right_pad = s.right_pad.wrapping_add(bi as u8);
            }
            C::IncrementVertTextPad => {
                let mut s = self.st.borrow_mut();
                s.top_pad = s.top_pad.wrapping_add(ai as u8);
                s.bottom_pad = s.bottom_pad.wrapping_add(bi as u8);
            }
        }
        0
    }
}

fn field_of(v: i32) -> Option<EeWonMsBtnField> {
    type B = EeWonMsBtnField;
    Some(match v {
        1 => B::StandardTextColor,
        2 => B::TextColor,
        3 => B::HAlign,
        4 => B::VAlign,
        5 => B::FocusStyle,
        6 => B::TopTextPad,
        7 => B::BottomTextPad,
        8 => B::LeftTextPad,
        9 => B::RightTextPad,
        10 => B::Transparent,
        11 => B::AdjustHeight,
        _ => return None,
    })
}

// ---- the C ABI

/// A new Rust MSButtonBase for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_msbtn_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonMsButton {
    Box::into_raw(Box::new(EeWonMsButton::new(component, host)))
}

/// # Safety
/// `btn` came from `ee_won_msbtn_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_msbtn_free(btn: *mut EeWonMsButton) {
    if !btn.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(btn) });
    }
}

/// One of MSButtonBase's methods (`EeWonMsBtnCall`).
///
/// # Safety
/// `btn` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_msbtn_call(
    btn: *const EeWonMsButton,
    call: EeWonMsBtnCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*btn).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "msbtnobj_tests.rs"]
mod tests;
