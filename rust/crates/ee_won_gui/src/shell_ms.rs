//! The Windows skin's concrete widgets as shell kinds (R6 WONLobby round 20): MSButton, MSHeaderButton,
//! MSCheckbox, MSRadioButton, MSLabel, MSTabButton, MSArrowButton, MaxSplitterButton, MSCloseButton, MS3DFrame,
//! MSSeperator, MSEtchedFrame, MSScrollThumb, MSPopup, MSDlgBack and MSInputBox. Each was a C++ class of a
//! constructor, a paint and at most a desired size and a field or two over its base (MSControlConstructors.cpp,
//! MSRemainingPainters.cpp, MSPainters.cpp, MSBorderPainters.cpp, MenuSeparator.cpp, CrossPromotionButton.cpp);
//! the statements are those, in their order, their calls the shell's ([`Sh`]).
//!
//! The paints are the mspaint.rs plans the C++ ran (`WONMsPaint`); the check box's and radio button's replaceable
//! painter functions were never replaced, so the MS button base's box paint draws the default box (its host).

use std::cell::Cell;
use std::ffi::CStr;

use crate::guidraw::{EeWonDrawOp, EE_WON_DRAW_FILL_RECT, EE_WON_DRAW_LINE, EE_WON_DRAW_SET_SYS_COLOR};
use crate::msbtnobj::{EeWonMsBox, EeWonMsBtnCall as M, EeWonMsBtnField as Mf};
use crate::mspaint::{
    EeWonMsPaintKind as P, EE_WON_MSPAINT_DEFAULT, EE_WON_MSPAINT_DISABLED, EE_WON_MSPAINT_FRAME2,
    EE_WON_MSPAINT_PRESSED, EE_WON_MSPAINT_TRANSPARENT,
};
use crate::shell::{
    bit, gui_string_len, ms_paint_of, EeWonShellBase as B, EeWonShellHostOp as H, EeWonShellKind as K,
    EeWonShellOp as O, EeWonShellVirt as V, Sh, ShellKind,
};

/// `ComponentFlag_WantFocus`, `ComponentFlag_NoInput`.
const FLAG_WANT_FOCUS: i32 = 0x8;
const FLAG_NO_INPUT: i32 = 0x800;
/// `StandardColor_3DHilight`, `_3DShadow`, `_ButtonText`, `_Text`.
const SYS_3D_HILIGHT: i32 = 2;
const SYS_3D_SHADOW: i32 = 3;
const SYS_BUTTON_TEXT: i32 = 5;
const SYS_TEXT: i32 = 14;
/// `MSCheckbox::mCheckboxWidth` / `mCheckboxHeight`, `MSRadioButton::mRadioWidth` / `mRadioHeight`.
const BOX_SIZE: i32 = 13;

/// The C++ shell class of `kind`'s widget.
pub fn base(kind: K) -> B {
    match kind {
        K::MsButton | K::MsHeaderButton | K::MsCheckbox | K::MsRadioButton | K::MsLabel | K::MsTabButton => B::MsButton,
        K::MsArrowButton | K::MaxSplitterButton | K::MsCloseButton => B::Button,
        K::MsInputBox => B::Input,
        K::MsComboBox => B::Combo,
        K::MsScrollbar => B::Scrollbar,
        K::MsScrollContainer | K::MsListCtrl => B::Scroller,
        K::MsHeaderControl => B::Header,
        K::MsTabCtrl => B::Tab,
        K::MsEtchedFrameGroup => B::Container,
        K::MsMessageBox => B::Dialog,
        K::Ms3DFrame | K::MsSeperator | K::MsEtchedFrame | K::MsScrollThumb | K::MsPopup | K::MsDlgBack => B::Component,
        // (the WON skin's kinds: shell_won.rs)
        _ => B::Component,
    }
}

/// A new kind's state.
pub fn make(kind: K) -> Box<dyn ShellKind> {
    match kind {
        K::MsButton | K::MsHeaderButton => {
            Box::new(MsButton { header: kind == K::MsHeaderButton, frame2: Cell::new(false) })
        }
        K::MsCheckbox => Box::new(MsBox(EeWonMsBox::Checkbox)),
        K::MsRadioButton => Box::new(MsBox(EeWonMsBox::Radio)),
        K::MsLabel => Box::new(MsLabel),
        K::MsTabButton => Box::new(MsTabButton { lower: Cell::new(3) }),
        K::MsArrowButton | K::MaxSplitterButton => {
            Box::new(MsArrow { max: kind == K::MaxSplitterButton, direction: Cell::new(2) })
        }
        K::MsCloseButton => Box::new(MsClose),
        K::Ms3DFrame => Box::new(Ms3DFrame { frame_type: Cell::new(3), up: Cell::new(false) }),
        K::MsComboBox => Box::new(crate::shell_mscont::MsComboBox::default()),
        K::MsScrollbar => Box::new(crate::shell_mscont::MsScrollbar::default()),
        K::MsScrollContainer => Box::new(crate::shell_mscont::MsScroll::new(false)),
        K::MsListCtrl => Box::new(crate::shell_mscont::MsScroll::new(true)),
        K::MsHeaderControl => Box::new(crate::shell_mscont::MsHeaderControl),
        K::MsTabCtrl => Box::new(crate::shell_mscont::MsTabCtrl::default()),
        K::MsEtchedFrameGroup => Box::new(crate::shell_mscont::MsEtchedGroup::default()),
        K::MsMessageBox => Box::new(crate::shell_msgbox::MsMessageBox::default()),
        _ => Box::new(MsPlain(kind)),
    }
}

/// The paint every one starts with: its base class's (`Component::Paint`, through the C++ base's).
fn base_paint(sh: &Sh, g: isize) {
    sh.base(V::Paint, g, 0, 0);
}

/// `GetDesiredSize`'s two out-ints.
pub(crate) fn set_wh(a: isize, b: isize, w: i32, h: i32) {
    // SAFETY: the C++ passes two live ints
    unsafe {
        *(a as *mut i32) = w;
        *(b as *mut i32) = h;
    }
}

/// `mText->SetText(text); SetDesiredSize(0,0)`.
fn set_text_and_size(sh: &Sh, text: isize) {
    sh.host(H::WrapSetText, text, 0, 0, 0);
    sh.set_desired_size(0, 0);
}

/// `MSButton` and `MSHeaderButton` (an MSButton).
struct MsButton {
    header: bool,
    /// `mUseFrame2`.
    frame2: Cell<bool>,
}

impl MsButton {
    /// `MSButton::Paint`.
    fn button_paint(&self, sh: &Sh, g: isize) {
        base_paint(sh, g);
        let mut f = 0;
        if sh.is_pressed() {
            f |= EE_WON_MSPAINT_PRESSED;
        }
        if sh.msb(M::Get, Mf::Transparent as isize, 0, 0) != 0 {
            f |= EE_WON_MSPAINT_TRANSPARENT;
        }
        if sh.btn(crate::btnobj::EeWonBtnCall::IsDefaultButton, 0, 0, 0) != 0 {
            f |= EE_WON_MSPAINT_DEFAULT;
        }
        if self.frame2.get() {
            f |= EE_WON_MSPAINT_FRAME2;
        }
        sh.ms_paint(g, &ms_paint_of(P::Button, sh.width(), sh.height(), f, 0, 0, 0), true);
    }
}

impl ShellKind for MsButton {
    fn name(&self) -> &'static CStr {
        if self.header {
            c"MSHeaderButton"
        } else {
            c"MSButton"
        }
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        // MSButton::Init
        sh.msb_set(Mf::LeftTextPad, 8);
        sh.msb_set(Mf::RightTextPad, 8);
        sh.msb_set(Mf::TopTextPad, 4);
        sh.msb_set(Mf::BottomTextPad, 4);
        sh.set_flags(FLAG_WANT_FOCUS, true);
        sh.set_button_flags(0x400, true);
        self.frame2.set(false);
        if !self.header {
            // MSButton(text); MSButton() has no text
            if args[0] != 0 {
                set_text_and_size(sh, args[0]);
            }
            return;
        }
        // MSHeaderButton(text) over MSButton()
        sh.set_flags(FLAG_WANT_FOCUS, false);
        self.frame2.set(true);
        sh.msb(M::IncrementHorzTextPad, 2, 0, 0);
        sh.msb(M::SetHorzAlign, 0, 0, 0);
        // SAFETY: the constructor's text is live while it runs
        if unsafe { gui_string_len(args[0]) } != 0 {
            set_text_and_size(sh, args[0]);
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v != V::Paint {
            return 0;
        }
        self.button_paint(sh, a);
        if self.header {
            // the sort arrow after the text, pressed one pixel down
            let f = if sh.is_pressed() { EE_WON_MSPAINT_PRESSED } else { 0 };
            let sort = sh.host(H::SortType, 0, 0, 0, 0) as i32;
            let text_width = sh.host(H::WrapWidth, 0, 0, 0, 0) as i32;
            let pad = sh.msb(M::Get, Mf::LeftTextPad as isize, 0, 0) as i32;
            sh.ms_paint(a, &ms_paint_of(P::HeaderSort, sh.width(), sh.height(), f, sort, text_width, pad), false);
        }
        0
    }
    fn op(&self, _sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        if op == O::SetUseFrame2 {
            self.frame2.set(a != 0);
        }
        0
    }
}

/// `MSCheckbox` and `MSRadioButton`: the MS button base's box (its `Init` and `Paint`).
struct MsBox(EeWonMsBox);

impl ShellKind for MsBox {
    fn name(&self) -> &'static CStr {
        match self.0 {
            EeWonMsBox::Checkbox => c"MSCheckbox",
            EeWonMsBox::Radio => c"MSRadioButton",
        }
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        sh.msb(M::BoxInit, self.0 as isize, BOX_SIZE as isize, 0);
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v == V::Paint {
            sh.msb(M::BoxPaint, self.0 as isize, a, BOX_SIZE as isize);
        }
        0
    }
}

/// `MSLabel`: an unpadded label that never activates.
struct MsLabel;

impl ShellKind for MsLabel {
    fn name(&self) -> &'static CStr {
        c"MSLabel"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        sh.msb(M::SetStandardTextColor, SYS_TEXT as isize, 0, 0);
        sh.msb_set(Mf::LeftTextPad, 0);
        sh.msb_set(Mf::RightTextPad, 0);
        sh.msb_set(Mf::TopTextPad, 0);
        sh.msb_set(Mf::BottomTextPad, 0);
        if args[0] == 0 {
            // MSLabel(): left aligned
            sh.msb_set(Mf::HAlign, 0);
            sh.set_button_flags(0x200, true);
        } else {
            // MSLabel(text): the centred alignment kept
            sh.set_button_flags(0x200, true);
            sh.btn(crate::btnobj::EeWonBtnCall::SetText, args[0], 1, 0);
            sh.set_desired_size(0, 0);
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v == V::Paint {
            base_paint(sh, a);
            let t = sh.msb(M::Get, Mf::Transparent as isize, 0, 0) != 0;
            let f = if t { EE_WON_MSPAINT_TRANSPARENT } else { 0 };
            sh.ms_paint(a, &ms_paint_of(P::Label, sh.width(), sh.height(), f, 0, 0, 0), true);
        }
        0
    }
}

/// `MSTabButton`: a radio button that activates on press, painted as a tab.
struct MsTabButton {
    /// `mLowerAmount`.
    lower: Cell<i32>,
}

impl ShellKind for MsTabButton {
    fn name(&self) -> &'static CStr {
        c"MSTabButton"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint) | bit(V::DesiredSize)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        self.lower.set(args[2] as i32);
        // SAFETY: the constructor's text is live while it runs (0: the default "")
        if unsafe { gui_string_len(args[0]) } != 0 {
            set_text_and_size(sh, args[0]);
        }
        sh.msb_set(Mf::Transparent, (args[1] == 0) as i32);
        sh.set_button_flags(0x48, true);
        sh.msb_set(Mf::VAlign, 1);
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, b: isize, _c: isize) -> isize {
        match v {
            V::DesiredSize => {
                sh.base(V::DesiredSize, a, b, 0);
                // SAFETY: the C++ passes two live ints
                let (w, h) = unsafe { (*(a as *const i32), *(b as *const i32)) };
                set_wh(a, b, w.wrapping_add(16), h.wrapping_add(5).wrapping_add(self.lower.get()));
            }
            V::Paint => {
                base_paint(sh, a);
                sh.ms_paint(a, &ms_paint_of(P::TabButton, sh.width(), sh.height(), 0, 0, 0, 0), true);
            }
            _ => {}
        }
        0
    }
    fn op(&self, _sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        if op == O::SetLowerAmount {
            self.lower.set(a as i32);
        }
        0
    }
}

/// `MSArrowButton` and `MaxSplitterButton` (an arrow button that maximises a splitter pane).
struct MsArrow {
    max: bool,
    /// `mDirection`.
    direction: Cell<i32>,
}

impl ShellKind for MsArrow {
    fn name(&self) -> &'static CStr {
        if self.max {
            c"MaxSplitterButton"
        } else {
            c"MSArrowButton"
        }
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        self.direction.set(args[0] as i32);
        sh.set_button_flags(2, true);
        if self.max {
            sh.set_size(15, 15);
            sh.set_button_flags(2, false);
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v != V::Paint {
            return 0;
        }
        base_paint(sh, a);
        let mut f = 0;
        if sh.is_pressed() {
            f |= EE_WON_MSPAINT_PRESSED;
        }
        if sh.is_disabled() {
            f |= EE_WON_MSPAINT_DISABLED;
        }
        sh.ms_paint(a, &ms_paint_of(P::ArrowButton, sh.width(), sh.height(), f, self.direction.get(), 0, 0), false);
        if self.max && sh.is_checked() {
            // a small bar in the button text colour, one pixel down and right while pressed
            let (mut x, mut y) = ((sh.width() / 2).wrapping_sub(3), (sh.height() / 2).wrapping_sub(2));
            if sh.is_pressed() {
                x = x.wrapping_add(1);
                y = y.wrapping_add(1);
            }
            sh.draw(a, &EeWonDrawOp { kind: EE_WON_DRAW_SET_SYS_COLOR, a: SYS_BUTTON_TEXT, ..Default::default() });
            sh.draw(a, &EeWonDrawOp { kind: EE_WON_DRAW_FILL_RECT, a: x, b: y, c: 7, d: 4, ..Default::default() });
        }
        0
    }
    fn op(&self, _sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        if op == O::SetDirection {
            self.direction.set(a as i32);
        }
        0
    }
}

/// `MSCloseButton`: 16 x 14 with an X.
struct MsClose;

impl ShellKind for MsClose {
    fn name(&self) -> &'static CStr {
        c"MSCloseButton"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint) | bit(V::DesiredSize)
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        sh.set_size(16, 14);
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, b: isize, _c: isize) -> isize {
        match v {
            V::DesiredSize => set_wh(a, b, 16, 14),
            V::Paint => {
                base_paint(sh, a);
                let f = if sh.is_pressed() { EE_WON_MSPAINT_PRESSED } else { 0 };
                sh.ms_paint(a, &ms_paint_of(P::CloseButton, sh.width(), sh.height(), f, 0, 0, 0), false);
            }
            _ => {}
        }
        0
    }
}

/// `MS3DFrame`: a raised or sunken frame of a type, ignoring input.
struct Ms3DFrame {
    /// `mType`, `mIsUp`.
    frame_type: Cell<i32>,
    up: Cell<bool>,
}

impl ShellKind for Ms3DFrame {
    fn name(&self) -> &'static CStr {
        c"MS3DFrame"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        self.frame_type.set(args[0] as i32);
        self.up.set(args[1] != 0);
        sh.set_flags(FLAG_NO_INPUT, true);
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v == V::Paint {
            base_paint(sh, a);
            let f = if self.up.get() { 0 } else { EE_WON_MSPAINT_PRESSED };
            let p = ms_paint_of(P::Frame3DComponent, sh.width(), sh.height(), f, self.frame_type.get(), 0, 0);
            sh.ms_paint(a, &p, false);
        }
        0
    }
    fn op(&self, _sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        match op {
            O::SetFrameType => self.frame_type.set(a as i32),
            O::SetIsUp => self.up.set(a != 0),
            _ => {}
        }
        0
    }
}

/// The paint-only ones: `MSSeperator`, `MSEtchedFrame`, `MSScrollThumb`, `MSPopup`, `MSDlgBack`, `MSInputBox`.
struct MsPlain(K);

impl ShellKind for MsPlain {
    fn name(&self) -> &'static CStr {
        match self.0 {
            K::MsSeperator => c"MSSeperator",
            K::MsEtchedFrame => c"MSEtchedFrame",
            K::MsScrollThumb => c"MSScrollThumb",
            K::MsPopup => c"MSPopup",
            K::MsDlgBack => c"MSDlgBack",
            _ => c"MSInputBox",
        }
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint) | if matches!(self.0, K::MsInputBox | K::MsSeperator) { bit(V::DesiredSize) } else { 0 }
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        match self.0 {
            K::MsSeperator => sh.set_size(sh.width(), args[0] as i32),
            K::MsEtchedFrame => sh.set_flags(FLAG_NO_INPUT, true),
            _ => {}
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, b: isize, _c: isize) -> isize {
        if v == V::DesiredSize {
            // MSInputBox 100 x 22, MSSeperator 100 x 6 (the SDK's inline)
            set_wh(a, b, 100, if self.0 == K::MsSeperator { 6 } else { 22 });
            return 0;
        }
        if v != V::Paint {
            return 0;
        }
        base_paint(sh, a);
        let (w, h) = (sh.width(), sh.height());
        let kind = match self.0 {
            K::MsSeperator => {
                // an etched line across the middle: shadow over highlight
                let mut y = (h / 2).wrapping_sub(1);
                if y < 0 {
                    y = 0;
                }
                let line = |y: i32| EeWonDrawOp {
                    kind: EE_WON_DRAW_LINE,
                    a: 0,
                    b: y,
                    c: w.wrapping_sub(1),
                    d: y,
                    ..Default::default()
                };
                sh.draw(a, &EeWonDrawOp { kind: EE_WON_DRAW_SET_SYS_COLOR, a: SYS_3D_SHADOW, ..Default::default() });
                sh.draw(a, &line(y));
                y = y.wrapping_add(1);
                sh.draw(a, &EeWonDrawOp { kind: EE_WON_DRAW_SET_SYS_COLOR, a: SYS_3D_HILIGHT, ..Default::default() });
                sh.draw(a, &line(y));
                return 0;
            }
            K::MsEtchedFrame => P::EtchedFrame,
            K::MsScrollThumb => P::ScrollThumb,
            K::MsPopup => P::Popup,
            K::MsDlgBack => P::DlgBack,
            // MSInputBox: MSGraphicsUtil::DrawFrame3(g, 0, 0, w, h, true)
            _ => P::Frame3,
        };
        sh.ms_paint(a, &ms_paint_of(kind, w, h, 0, 0, 0, 0), false);
        0
    }
}

#[cfg(test)]
#[path = "shell_ms_tests.rs"]
mod tests;
