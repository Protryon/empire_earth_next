//! The Windows skin's composite widgets as shell kinds (R6 WONLobby round 20): MSComboBox, MSScrollbar,
//! MSScrollContainer, MSListCtrl, MSHeaderControl, MSTabCtrl and MSEtchedFrameGroup. Each was a C++ class of a
//! constructor that made and placed its parts, a paint, a layout or a desired size, and a flag or two
//! (MSComboComposition.cpp, MSScrollComposition.cpp, MSControlPainters.cpp, MSTabComposition.cpp,
//! MSEtchedFrameGroup.cpp, MSControlConstructors.cpp); the statements are those, in their order.
//!
//! The parts the constructors make are new C++ widgets (`New` for a shell kind, `NewBase` for a base class) held as
//! the base class's members (`ComboPart`) or by the kind (a reference each, `Ref`); the layouts are skinlay.rs's,
//! the kinds answering the receiver's fields and own parts (`skin`) and the C++ host the rest.

use std::cell::Cell;
use std::ffi::CStr;

use crate::barobj::EeWonBarCall as Bar;
use crate::compobj::EeWonCompCall as Cc;
use crate::ctobj::EeWonCtCall as Ct;
use crate::labelobj::EeWonLabelCall as Lb;
use crate::listobj::EeWonListCall as Ls;
use crate::mspaint::{
    EeWonMsPaint, EeWonMsPaintKind as P, EE_WON_MSPAINT_DRAW_3D, EE_WON_MSPAINT_DRAW_FRAME, EE_WON_MSPAINT_HORZ,
    EE_WON_MSPAINT_INVALID, EE_WON_MSPAINT_TRANSPARENT, EE_WON_MSPAINT_VERT,
};
use crate::multiobj::EeWonMHeadCall as Mh;
use crate::scrollerobj::EeWonScrollerCall as Sc;
use crate::shell::{
    bit, ms_paint_of, EeWonShellBaseNew as N, EeWonShellComboPart as Cp, EeWonShellHostOp as H, EeWonShellKind as K,
    EeWonShellOp as O, EeWonShellVirt as V, Sh, ShellKind,
};
use crate::skinlay::{EeWonSkinField as Sf, EeWonSkinLay as Sl, EeWonSkinLayKind as Lk, EeWonSkinPart as Sp};
use crate::tabobj::EeWonTabCall as Tb;

/// `Direction_Left`, `_Right`, `_Up`, `_Down`.
const DIR_LEFT: isize = 0;
const DIR_RIGHT: isize = 1;
const DIR_UP: isize = 2;
const DIR_DOWN: isize = 3;
/// `HorzAlign_Center`; `ScrollbarCondition_Disable`, `_Conditional`.
const HALIGN_CENTER: isize = 1;
const COND_DISABLE: isize = 0;
const COND_CONDITIONAL: isize = 2;

/// `WONMsPaintOf` with all five arguments.
fn paint_of(kind: P, w: i32, h: i32, flags: u32, a: [i32; 5]) -> EeWonMsPaint {
    EeWonMsPaint { d: a[3], e: a[4], ..ms_paint_of(kind, w, h, flags, a[0], a[1], a[2]) }
}

/// A new widget of a shell kind.
fn new_kind(sh: &Sh, k: K, a: isize, b: isize, c: isize) -> isize {
    sh.host(H::New, k as isize, a, b, c)
}

/// `MSComboBox`: a framed combo box with a down arrow, an edit box and a scrolling list in the MSButton font.
#[derive(Default)]
pub struct MsComboBox {
    /// `mDrawFrame`.
    draw_frame: Cell<bool>,
}

impl ShellKind for MsComboBox {
    fn name(&self) -> &'static CStr {
        c"MSComboBox"
    }
    fn overrides(&self) -> u64 {
        bit(V::SizeChanged) | bit(V::DesiredSize) | bit(V::Paint)
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        self.draw_frame.set(true);
        // mButton=MSComboBox::ArrowButtonFactory(Direction_Down) (DefaultMSArrowButtonFactory)
        let button = new_kind(sh, K::MsArrowButton, DIR_DOWN, 0, 0);
        sh.host(H::ComboPart, Cp::Button as isize, button, 0, 0);
        if crate::compmem::width(button) == 0 {
            sh.comp_on(button, Cc::SetSize, 16, crate::compmem::height(button) as isize);
        }
        let edit = sh.host(H::NewBase, N::InputBox as isize, 0, 0, 0);
        sh.host(H::ComboPart, Cp::Edit as isize, edit, 0, 0);
        let list = sh.host(H::NewBase, N::ListArea as isize, 0, 0, 0);
        sh.host(H::ComboPart, Cp::ListArea as isize, list, 0, 0);
        sh.host(H::NamedFont, list, N::ListArea as isize, 0, 0);
        sh.list_on(list, Ls::SetListFlags, 2, 1);
        sh.host(H::NamedFont, edit, N::InputBox as isize, 0, 0);
        // the local SmartPtr<MSScrollContainer> listBox holds it to the constructor's end
        let list_box = new_kind(sh, K::MsScrollContainer, list, 0, 0);
        sh.host(H::Ref, list_box, 1, 0, 0);
        sh.scroller_on(list_box, Sc::SetScrollbarConditions, COND_DISABLE, COND_CONDITIONAL);
        sh.shell_on(list_box, O::SetDraw3DFrame, 0, 0);
        sh.host(H::ComboPart, Cp::ListBox as isize, list_box, 0, 0);
        sh.btn_on(button, crate::btnobj::EeWonBtnCall::SetButtonFlags, 2, 0);
        sh.host(H::Ref, list_box, -1, 0, 0);
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, b: isize, _c: isize) -> isize {
        match v {
            V::SizeChanged => {
                sh.base(V::SizeChanged, 0, 0, 0);
                sh.skin_layout(Lk::MsCombo, 0, 0, 0, None);
            }
            V::DesiredSize => crate::shell_ms::set_wh(a, b, 100, 22),
            V::Paint => {
                sh.base(V::Paint, a, 0, 0);
                let f = if self.draw_frame.get() { EE_WON_MSPAINT_DRAW_FRAME } else { 0 };
                sh.ms_paint(a, &ms_paint_of(P::ComboBox, sh.width(), sh.height(), f, 0, 0, 0), false);
            }
            _ => {}
        }
        0
    }
    fn op(&self, _sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        if op == O::SetDrawFrame {
            self.draw_frame.set(a != 0);
        }
        0
    }
    fn skin(&self, _sh: &Sh, w: Sl, which: isize) -> Option<isize> {
        (w == Sl::Field && which == Sf::DrawFrame as isize).then(|| self.draw_frame.get() as isize)
    }
}

/// `MSScrollbar`: arrow buttons, a thumb and a track from the factories, opaque unless set transparent.
#[derive(Default)]
pub struct MsScrollbar {
    /// `mIsTransparent`.
    transparent: Cell<bool>,
}

impl ShellKind for MsScrollbar {
    fn name(&self) -> &'static CStr {
        c"MSScrollbar"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        let me = sh.comp();
        self.transparent.set(false);
        let (up, down) = if args[0] != 0 { (DIR_UP, DIR_DOWN) } else { (DIR_LEFT, DIR_RIGHT) };
        // SetUpArrow(ArrowButtonFactory(..)), SetDownArrow(..): each factory's SmartPtr held over the call
        for (part, dir) in [(0, up), (1, down)] {
            let b = new_kind(sh, K::MsArrowButton, dir, 0, 0);
            sh.host(H::Ref, b, 1, 0, 0);
            sh.bar_on(me, Bar::SetPart, part, b);
            sh.host(H::Ref, b, -1, 0, 0);
        }
        for part in [0, 1] {
            let arrow = sh.bar_on(me, Bar::GetPart, part, 0);
            let narrow = crate::compmem::width(arrow) == 0 || {
                let arrow = sh.bar_on(me, Bar::GetPart, part, 0);
                crate::compmem::height(arrow) == 0
            };
            if narrow {
                let arrow = sh.bar_on(me, Bar::GetPart, part, 0);
                sh.comp_on(arrow, Cc::SetSizeAtLeast, 15, 15);
            }
        }
        // SetThumb(ThumbFactory(ThumbType(IsVertical()))) (DefaultThumbFactory)
        sh.bar_on(me, Bar::IsVertical, 0, 0);
        let thumb = new_kind(sh, K::MsScrollThumb, 0, 0, 0);
        sh.host(H::Ref, thumb, 1, 0, 0);
        sh.bar_on(me, Bar::SetPart, 3, thumb);
        sh.host(H::Ref, thumb, -1, 0, 0);
        let rect = sh.host(H::NewBase, N::Button as isize, 0, 0, 0);
        sh.bar_on(me, Bar::SetPart, 2, rect);
        let up = sh.bar_on(me, Bar::GetPart, 0, 0);
        let w = crate::compmem::width(up);
        let up = sh.bar_on(me, Bar::GetPart, 0, 0);
        sh.set_size(w, crate::compmem::height(up));
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v == V::Paint {
            // the track's fill when it needs repainting (unless transparent), then the parts
            let me = sh.comp();
            let rect = |_: ()| sh.bar_on(me, Bar::GetPart, 2, 0);
            let mut f = if crate::compmem::invalid(rect(())) != 0 { EE_WON_MSPAINT_INVALID } else { 0 };
            if self.transparent.get() {
                f |= EE_WON_MSPAINT_TRANSPARENT;
            }
            let r = [rect(()), rect(()), rect(()), rect(())];
            let at = [
                crate::compmem::left(r[0]),
                crate::compmem::top(r[1]),
                crate::compmem::width(r[2]),
                crate::compmem::height(r[3]),
                0,
            ];
            sh.ms_paint(a, &paint_of(P::Scrollbar, 0, 0, f, at), false);
            sh.base(V::Paint, a, 0, 0);
        }
        0
    }
    fn op(&self, _sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        if op == O::SetTransparent {
            self.transparent.set(a != 0);
        }
        0
    }
}

/// `MSScrollContainer` and `MSListCtrl` (a scroll container over a multi-column list with a header strip).
pub struct MsScroll {
    list: bool,
    /// `mHeaderHeight`, `mDraw3DFrame`, `mDrawFrame`, `mTransparent`; the list's `mHeader` (held).
    header_height: Cell<i32>,
    draw_3d: Cell<bool>,
    draw_frame: Cell<bool>,
    transparent: Cell<bool>,
    header: Cell<isize>,
}

impl MsScroll {
    pub fn new(list: bool) -> Self {
        MsScroll {
            list,
            header_height: Cell::new(0),
            draw_3d: Cell::new(false),
            draw_frame: Cell::new(false),
            transparent: Cell::new(false),
            header: Cell::new(0),
        }
    }
    /// `MSScrollContainer::SetDraw3DFrame` / `SetDrawFrame`: a change lays the container out again.
    fn set_and_relayout(&self, sh: &Sh, f: &Cell<bool>, on: bool) {
        if f.get() == on {
            return;
        }
        f.set(on);
        sh.host(H::SelfVirt, V::SizeChanged as isize, 0, 0, 0);
    }
}

impl ShellKind for MsScroll {
    fn name(&self) -> &'static CStr {
        if self.list {
            c"MSListCtrl"
        } else {
            c"MSScrollContainer"
        }
    }
    fn overrides(&self) -> u64 {
        bit(V::SizeChanged) | bit(V::Paint)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        let me = sh.comp();
        // MSListCtrl(area): the scroll container over the area, or a new multi-column list
        let area =
            if self.list && args[0] == 0 { sh.host(H::NewBase, N::MultiListArea as isize, 0, 0, 0) } else { args[0] };
        // MSScrollContainer(area)
        let bar = new_kind(sh, K::MsScrollbar, 1, 0, 0);
        sh.scroller_on(me, Sc::SetBar, 1, bar);
        let bar = new_kind(sh, K::MsScrollbar, 0, 0, 0);
        sh.scroller_on(me, Sc::SetBar, 0, bar);
        sh.scroller_on(me, Sc::SetScrollArea, area, 0);
        self.header_height.set(0);
        self.draw_3d.set(true);
        self.draw_frame.set(true);
        self.transparent.set(false);
        if self.list {
            let list = sh.scroller_on(me, Sc::GetScrollArea, 0, 0);
            let header = new_kind(sh, K::MsHeaderControl, list, 0, 0);
            sh.host(H::Ref, header, 1, 0, 0);
            self.header.set(header);
            sh.ct_on(me, Ct::AddChild, header, 0);
            self.header_height.set(15);
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        let me = sh.comp();
        match v {
            V::SizeChanged => {
                sh.base(V::SizeChanged, 0, 0, 0);
                sh.skin_layout(if self.list { Lk::MsList } else { Lk::MsScroll }, 0, 0, 0, None);
            }
            V::Paint => {
                sh.base(V::Paint, a, 0, 0);
                let v = sh.scroller_on(me, Sc::GetScrollbar, 1, 0);
                let h = sh.scroller_on(me, Sc::GetScrollbar, 0, 0);
                let mut f = 0;
                for (on, bit) in [
                    (crate::compmem::is_visible(v), EE_WON_MSPAINT_VERT),
                    (crate::compmem::is_visible(h), EE_WON_MSPAINT_HORZ),
                    (self.transparent.get(), EE_WON_MSPAINT_TRANSPARENT),
                    (self.draw_frame.get(), EE_WON_MSPAINT_DRAW_FRAME),
                    (self.draw_3d.get(), EE_WON_MSPAINT_DRAW_3D),
                ] {
                    if on {
                        f |= bit;
                    }
                }
                let b = crate::compmem::bounds(v);
                let at = [b.x, b.y, b.width, b.height, crate::compmem::height(h)];
                sh.ms_paint(a, &paint_of(P::ScrollContainer, sh.width(), sh.height(), f, at), false);
            }
            _ => {}
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        match op {
            O::SetDraw3DFrame => self.set_and_relayout(sh, &self.draw_3d, a != 0),
            O::SetDrawFrame => self.set_and_relayout(sh, &self.draw_frame, a != 0),
            O::SetTransparent => self.transparent.set(a != 0),
            O::SetHeaderHeight if self.list => self.header_height.set(a as i32),
            O::GetHeader if self.list => return self.header.get(),
            _ => {}
        }
        0
    }
    fn skin(&self, _sh: &Sh, w: Sl, which: isize) -> Option<isize> {
        match w {
            Sl::Part if which == Sp::Header as isize => Some(self.header.get()),
            Sl::Field if which == Sf::DrawFrame as isize => Some(self.draw_frame.get() as isize),
            Sl::Field if which == Sf::Draw3DFrame as isize => Some(self.draw_3d.get() as isize),
            Sl::Field if which == Sf::HeaderHeight as isize => Some(self.header_height.get() as isize),
            _ => None,
        }
    }
    fn free(&self, sh: &Sh) {
        let h = self.header.replace(0);
        if h != 0 {
            sh.host(H::Ref, h, -1, 0, 0);
        }
    }
}

/// `MSHeaderControl`: a list header whose columns are MS header buttons.
pub struct MsHeaderControl;

impl ShellKind for MsHeaderControl {
    fn name(&self) -> &'static CStr {
        c"MSHeaderControl"
    }
    fn overrides(&self) -> u64 {
        0
    }
    fn virt(&self, _sh: &Sh, _v: V, _a: isize, _b: isize, _c: isize) -> isize {
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, b: isize) -> isize {
        if op == O::SetColumn {
            // SmartPtr<MSButton> button=new MSHeaderButton(title); SetColumnItem(column,button)
            let button = new_kind(sh, K::MsHeaderButton, b, 0, 0);
            sh.host(H::Ref, button, 1, 0, 0);
            sh.mhead_on(sh.comp(), Mh::SetColumnItem, a, button);
            sh.host(H::Ref, button, -1, 0, 0);
        }
        0
    }
}

/// `MSTabCtrl`: a tab control over a dialog back and a raised frame, with two 15 x 15 arrow buttons.
#[derive(Default)]
pub struct MsTabCtrl {
    /// `mIsTransparent`.
    transparent: Cell<bool>,
}

impl ShellKind for MsTabCtrl {
    fn name(&self) -> &'static CStr {
        c"MSTabCtrl"
    }
    fn overrides(&self) -> u64 {
        0
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        let me = sh.comp();
        self.transparent.set(false);
        let frame = new_kind(sh, K::Ms3DFrame, 1, 1, 0);
        sh.host(H::Ref, frame, 1, 0, 0);
        let back = new_kind(sh, K::MsDlgBack, 0, 0, 0);
        sh.host(H::Ref, back, 1, 0, 0);
        let page = sh.host(H::TabLayoutPoint, 0, 0, 0, 0);
        sh.layout(back, 0x5003, page, [0, 0, 0, 0, 0]);
        let page = sh.host(H::TabLayoutPoint, 0, 0, 0, 0);
        sh.layout(frame, 0x5003, page, [0, 0, 0, 0, 0]);
        sh.ct_on(me, Ct::AddChild, back, 0);
        sh.ct_on(me, Ct::AddChild, frame, 0);
        let left = new_kind(sh, K::MsArrowButton, DIR_LEFT, 0, 0);
        sh.host(H::Ref, left, 1, 0, 0);
        let right = new_kind(sh, K::MsArrowButton, DIR_RIGHT, 0, 0);
        sh.host(H::Ref, right, 1, 0, 0);
        sh.comp_on(left, Cc::SetSize, 15, 15);
        sh.comp_on(right, Cc::SetSize, 15, 15);
        sh.tab_on(me, Tb::SetTabArrows, left, right);
        // the locals' references go in reverse order
        for p in [right, left, back, frame] {
            sh.host(H::Ref, p, -1, 0, 0);
        }
    }
    fn virt(&self, _sh: &Sh, _v: V, _a: isize, _b: isize, _c: isize) -> isize {
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, b: isize) -> isize {
        match op {
            O::SetTransparent => self.transparent.set(a != 0),
            // AddStandardTab(text, container): AddTab(new MSTabButton(text), container)
            O::AddStandardTab => {
                let button = new_kind(sh, K::MsTabButton, a, 1, 3);
                sh.tab_on(sh.comp(), Tb::AddTab, button, b);
            }
            _ => {}
        }
        0
    }
}

/// `MSEtchedFrameGroup`: an etched frame around a contained component with a centred title label.
#[derive(Default)]
pub struct MsEtchedGroup {
    /// `mLabel`, `mComponent` (held).
    label: Cell<isize>,
    contained: Cell<isize>,
}

impl ShellKind for MsEtchedGroup {
    fn name(&self) -> &'static CStr {
        c"MSEtchedFrameGroup"
    }
    fn overrides(&self) -> u64 {
        bit(V::DesiredSize) | bit(V::AddedToParent) | bit(V::Paint)
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        let label = sh.host(H::NewBase, N::Label as isize, 0, 0, 0);
        sh.host(H::Ref, label, 1, 0, 0);
        self.label.set(label);
        sh.label_on(label, Lb::SetHorzAlign, HALIGN_CENTER, 0);
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, b: isize, _c: isize) -> isize {
        let me = sh.comp();
        match v {
            V::DesiredSize => sh.skin_layout(Lk::EtchedSize, 0, 0, 0, Some((a, b))),
            V::AddedToParent => {
                let label = self.label.get();
                if crate::compmem::parent(label) != me {
                    sh.ct_on(me, Ct::AddChild, label, 0);
                    sh.layout(label, 0x1000, me, [5, 0, 0, 0, 0]);
                    let c = self.contained.get();
                    if c != 0 {
                        sh.ct_on(me, Ct::AddChild, c, 0);
                        sh.layout(c, 0x1001, me, [10, 0, -20, 0, 0]);
                        sh.layout(c, 0x200, label, [0, 10, 0, 0, 0]);
                        sh.layout(c, 0x80000, me, [0, 0, 0, -10, 0]);
                    }
                }
                sh.base(V::AddedToParent, 0, 0, 0);
            }
            V::Paint => {
                sh.base(V::Paint, a, 0, 0);
                let l = self.label.get();
                let at = [crate::compmem::height(l), crate::compmem::left(l), crate::compmem::right(l), 0, 0];
                sh.ms_paint(a, &paint_of(P::EtchedGroup, sh.width(), sh.height(), 0, at), false);
            }
            _ => {}
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        match op {
            // SetLabelText(text): mLabel->SetText(text); mLabel->SetDesiredSize(10,0)
            O::SetLabelText => {
                let label = self.label.get();
                sh.host(H::LabelSetText, label, a, 0, 0);
                sh.comp_on(label, Cc::SetDesiredSize, 10, 0);
            }
            O::AddContainedComponent => {
                if a != 0 {
                    sh.host(H::Ref, a, 1, 0, 0);
                }
                let old = self.contained.replace(a);
                if old != 0 {
                    sh.host(H::Ref, old, -1, 0, 0);
                }
            }
            O::GetLabel => return self.label.get(),
            _ => {}
        }
        0
    }
    fn skin(&self, _sh: &Sh, w: Sl, which: isize) -> Option<isize> {
        match w {
            Sl::Part if which == Sp::Label as isize => Some(self.label.get()),
            Sl::Part if which == Sp::Contained as isize => Some(self.contained.get()),
            Sl::Field => Some(0),
            _ => None,
        }
    }
    fn free(&self, sh: &Sh) {
        // the members in reverse order: mComponent, then mLabel
        for f in [&self.contained, &self.label] {
            let p = f.replace(0);
            if p != 0 {
                sh.host(H::Ref, p, -1, 0, 0);
            }
        }
    }
}

#[cfg(test)]
#[path = "shell_mscont_tests.rs"]
mod tests;
