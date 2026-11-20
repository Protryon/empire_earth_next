//! The WON skin's widgets as shell kinds (R6 WONLobby round 23): WONSimpleButton, WONScrollThumb, WONHeaderButton,
//! WONInputBox, WONScrollbar, WONComboBox, WONScrollContainer, WONListCtrl, WONHeaderControl, WONTabCtrl and
//! WONFrameComponent. Each was a C++ class of a frame (`WONFrame`: four borders and a held image whose copy duplicates
//! it), a few images and flags, a paint, a layout and a `Duplicate` the skin (skinobj.rs) makes its widgets with
//! (WONFrame.cpp, WONFrameOwnerCompletion.cpp, WONSimpleButton.cpp, WONInputBox.cpp, SkinnedContainers.cpp,
//! SkinListControls.cpp, SkinTab.cpp, WONControlDuplicates.cpp, WONControlSetup.cpp, WONListCtrlSetup.cpp,
//! SimpleSkinControlConstructors.cpp, SkinControlConstructors.cpp, ControlSetupCompletion.cpp, WONSkinCloneProviders.cpp);
//! the statements are those, in their order.
//!
//! The frame is a Rust record ([`FrameRec`]) holding its image's reference: the image's methods (its reference, its
//! size, its duplicate) are the host's `Img` op, the frame's paint guidraw.rs's plan run with the image as its slot
//! (`DrawImages`). A frame crosses between kinds and from the configs as an [`EeWonFrameIn`] (the image borrowed).

use std::cell::Cell;
use std::ffi::CStr;

use crate::btnobj::{EeWonBtnCall as Bc, EeWonBtnField as Bf};
use crate::compobj::EeWonCompCall as Cc;
use crate::ctobj::EeWonCtCall as Ct;
use crate::guidraw::{self, EeWonFrame};
use crate::guihost::EeWonWbImage as Im;
use crate::listobj::EeWonListCall as Ls;
use crate::multiobj::EeWonMHeadCall as Mh;
use crate::scrollerobj::EeWonScrollerCall as Sc;
use crate::shell::{
    bit, EeWonShellBase as B, EeWonShellBaseNew as N, EeWonShellComboPart as Cp, EeWonShellHostOp as H,
    EeWonShellKind as K, EeWonShellOp as O, EeWonShellVirt as V, Sh, ShellKind,
};
use crate::skinlay::{EeWonSkinField as Sf, EeWonSkinLay as Sl, EeWonSkinLayKind as Lk, EeWonSkinPart as Sp};
use crate::skinobj::EeWonSkin;
use crate::tabobj::{EeWonTabCall as Tb, EeWonTabCtrl};
use crate::wonbtnobj::{EeWonWbCall as Wb, EeWonWbField as Wf};

/// A frame as it crosses (`WONFrame`'s members; the image borrowed).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonFrameIn {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    /// an `Image*`, or 0
    pub image: isize,
}

/// A held reference cell's `SmartPtr` assignment: the old one released, then the new one held (none when the same).
pub(crate) fn hold_image(sh: &Sh, cell: &Cell<isize>, img: isize) {
    let old = cell.get();
    if old == img {
        return;
    }
    if old != 0 {
        sh.img(Im::Release, old);
    }
    if img != 0 {
        sh.img(Im::Hold, img);
    }
    cell.set(img);
}

/// The same for a component.
pub(crate) fn hold_comp(sh: &Sh, cell: &Cell<isize>, c: isize) {
    let old = cell.get();
    if old == c {
        return;
    }
    if old != 0 {
        sh.comp_ref(old, -1);
    }
    if c != 0 {
        sh.comp_ref(c, 1);
    }
    cell.set(c);
}

/// `WONFrame`: four borders and a held image.
#[derive(Default)]
pub struct FrameRec {
    left: Cell<i32>,
    top: Cell<i32>,
    right: Cell<i32>,
    bottom: Cell<i32>,
    image: Cell<isize>,
}

impl FrameRec {
    pub fn get(&self) -> EeWonFrameIn {
        EeWonFrameIn {
            left: self.left.get(),
            top: self.top.get(),
            right: self.right.get(),
            bottom: self.bottom.get(),
            image: self.image.get(),
        }
    }
    pub fn image(&self) -> isize {
        self.image.get()
    }
    /// `WONFrame::Copy(from)`: a private duplicate of its image (none when it has none), then its borders.
    pub fn copy_from(&self, sh: &Sh, f: &EeWonFrameIn) {
        if f.image == 0 {
            hold_image(sh, &self.image, 0);
        } else {
            // mImage = from.mImage->Duplicate(): the temporary's reference becomes the frame's
            let d = sh.img(Im::Duplicate, f.image);
            let old = self.image.replace(d);
            if old != 0 && old != d {
                sh.img(Im::Release, old);
            }
        }
        self.left.set(f.left);
        self.top.set(f.top);
        self.right.set(f.right);
        self.bottom.set(f.bottom);
    }
    /// `SetImage(image)` (shared).
    pub fn set_image(&self, sh: &Sh, img: isize) {
        hold_image(sh, &self.image, img);
    }
    /// The border `side` (0 left, 1 top, 2 right, 3 bottom).
    fn border(&self, side: isize) -> &Cell<i32> {
        match side {
            0 => &self.left,
            1 => &self.top,
            2 => &self.right,
            _ => &self.bottom,
        }
    }
    /// `SkinFrameField`: a frame field of the skin layouts, else `None`.
    fn field(&self, which: isize) -> Option<isize> {
        let side = match which {
            x if x == Sf::FrameLeft as isize => 0,
            x if x == Sf::FrameTop as isize => 1,
            x if x == Sf::FrameRight as isize => 2,
            x if x == Sf::FrameBottom as isize => 3,
            _ => return None,
        };
        Some(self.border(side).get() as isize)
    }
    /// `WONDrawFrameOf(frame, this, lp, tp, rp, bp)`: the frame's inputs over the shell's widget.
    pub fn draw_of(&self, sh: &Sh, pads: [i32; 4]) -> EeWonFrame {
        let img = self.image.get();
        let has = img != 0;
        EeWonFrame {
            has_image: has,
            image_width: if has { sh.img(Im::Width, img) as i32 } else { 0 },
            image_height: if has { sh.img(Im::Height, img) as i32 } else { 0 },
            width: sh.width(),
            height: sh.height(),
            left: pads[0],
            top: pads[1],
            right: pads[2],
            bottom: pads[3],
        }
    }
    /// `WONFrame::Paint(g, this)`: the image stretched over the widget (guidraw.rs frame_paint).
    pub fn paint(&self, sh: &Sh, g: isize) {
        let f = self.draw_of(sh, [0; 4]);
        let slots = [self.image.get(), 0, 0, 0];
        for o in &guidraw::frame_paint(&f) {
            sh.draw_images(g, o, &slots);
        }
    }
    /// The frame's member destructor.
    pub fn release(&self, sh: &Sh) {
        hold_image(sh, &self.image, 0);
    }
}

/// The frame a C++ caller passes (`*(const EeWonFrameIn*)a`).
fn frame_in(a: isize) -> EeWonFrameIn {
    if a == 0 {
        return EeWonFrameIn::default();
    }
    // SAFETY: the C++ passes a live record
    unsafe { *(a as *const EeWonFrameIn) }
}

/// The kind's frame into `*(EeWonFrameIn*)a`.
fn frame_out(a: isize, f: EeWonFrameIn) {
    if a != 0 {
        // SAFETY: the caller passes a live record
        unsafe { *(a as *mut EeWonFrameIn) = f };
    }
}

/// Two `intptr_t`s the C++ passes at `b`.
fn pair(b: isize) -> [isize; 2] {
    if b == 0 {
        return [0, 0];
    }
    // SAFETY: the C++ passes two live intptr_ts
    unsafe { *(b as *const [isize; 2]) }
}

fn new_kind(sh: &Sh, k: K, a: isize, b: isize) -> isize {
    sh.host(H::New, k as isize, a, b, 0)
}

/// The skin a kind holds (`SmartPtr<WONSkin>`: the skin is the game's one, never freed, so no reference is taken).
///
/// # Safety
/// `p` is a live skin (or 0).
unsafe fn skin<'a>(p: isize) -> Option<&'a EeWonSkin> {
    // SAFETY: the contract
    unsafe { (p as *const EeWonSkin).as_ref() }
}

/// The WON skin's kinds: their C++ shell classes.
pub fn base(kind: K) -> Option<B> {
    Some(match kind {
        K::WonSimpleButton | K::WonScrollThumb => B::Button,
        K::WonHeaderButton => B::WonButton,
        K::WonInputBox => B::Input,
        K::WonScrollbar => B::Scrollbar,
        K::WonComboBox => B::Combo,
        K::WonScrollContainer | K::WonListCtrl => B::Scroller,
        K::WonHeaderControl => B::Header,
        K::WonTabCtrl => B::Tab,
        K::WonFrameComponent => B::Component,
        _ => return None,
    })
}

/// A WON kind's state.
pub fn make(kind: K) -> Option<Box<dyn ShellKind>> {
    Some(match kind {
        K::WonSimpleButton | K::WonScrollThumb => Box::new(WonSimpleButton::new(kind == K::WonScrollThumb)),
        K::WonHeaderButton => Box::new(WonHeaderButton),
        K::WonInputBox => Box::new(WonInputBox::default()),
        K::WonScrollbar => Box::new(WonScrollbar::default()),
        K::WonComboBox => Box::new(WonComboBox::default()),
        K::WonScrollContainer | K::WonListCtrl => Box::new(WonScroll::new(kind == K::WonListCtrl)),
        K::WonHeaderControl => Box::new(WonHeaderControl::default()),
        K::WonTabCtrl => Box::new(WonTabCtrl::default()),
        K::WonFrameComponent => Box::new(WonFrameComponent::default()),
        _ => return None,
    })
}

/// `WONSimpleButton` (and `WONScrollThumb`, a simple button that checks the mouse-down state and scales its image).
pub struct WonSimpleButton {
    thumb: bool,
    /// `mImages[4]` (held), `mScaleImage`, `mCheckMouseButtonDown`.
    images: [Cell<isize>; 4],
    scale: Cell<bool>,
    check_down: Cell<bool>,
}

impl WonSimpleButton {
    fn new(thumb: bool) -> Self {
        WonSimpleButton {
            thumb,
            images: Default::default(),
            scale: Cell::new(false),
            check_down: Cell::new(false),
        }
    }
}

impl ShellKind for WonSimpleButton {
    fn name(&self) -> &'static CStr {
        if self.thumb {
            c"WONScrollThumb"
        } else {
            c"WONSimpleButton"
        }
    }
    fn overrides(&self) -> u64 {
        bit(V::DesiredSize) | bit(V::Paint)
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        self.scale.set(false);
        self.check_down.set(false);
        sh.set_button_flags(1, true); // ButtonFlag_InvalidateOnMouseOver
        if self.thumb {
            self.check_down.set(true);
            self.scale.set(true);
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, b: isize, _c: isize) -> isize {
        match v {
            V::DesiredSize => sh.skin_layout(Lk::SimpleButtonSize, 0, 0, 0, Some((a, b))),
            V::Paint => {
                sh.base(V::Paint, a, 0, 0);
                let mut p = guidraw::EeWonSimpleButtonPaint {
                    disabled: sh.is_disabled(),
                    check_mouse_down: self.check_down.get(),
                    pressed: sh.is_pressed(),
                    mouse_over: sh.btn(Bc::IsMouseOver, 0, 0, 0) != 0,
                    scale_image: self.scale.get(),
                    mouse_down_state: sh.btn(Bc::GetField, Bf::MouseDownState as isize, 0, 0) as u8,
                    has_image: [false; 4],
                    image_width: [0; 4],
                    image_height: [0; 4],
                    width: sh.width(),
                    height: sh.height(),
                };
                let mut slots = [0isize; 4];
                for i in 0..4 {
                    let img = self.images[i].get();
                    slots[i] = img;
                    p.has_image[i] = img != 0;
                    p.image_width[i] = if img != 0 { sh.img(Im::Width, img) as i32 } else { 0 };
                    p.image_height[i] = if img != 0 { sh.img(Im::Height, img) as i32 } else { 0 };
                }
                for o in &guidraw::simple_button(&p) {
                    sh.draw_images(a, o, &slots);
                }
            }
            _ => {}
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, b: isize) -> isize {
        match op {
            O::SetImage if (0..4).contains(&a) => hold_image(sh, &self.images[a as usize], b),
            O::SetScaleImage => self.scale.set(a != 0),
            O::SetCheckMouseButtonDown => self.check_down.set(a != 0),
            // WONSimpleButton::Duplicate: the images, scaling, mouse-down check, the button's attributes and the size
            O::Duplicate => {
                let n = new_kind(sh, K::WonSimpleButton, 0, 0);
                for i in 0..4 {
                    sh.shell_on(n, O::SetImage, i as isize, self.images[i].get());
                }
                sh.shell_on(n, O::SetScaleImage, self.scale.get() as isize, 0);
                sh.shell_on(n, O::SetCheckMouseButtonDown, self.check_down.get() as isize, 0);
                sh.host(H::BtnCopy, n, sh.comp(), 0, 0);
                sh.comp_on(n, Cc::SetSize, sh.width() as isize, sh.height() as isize);
                return n;
            }
            _ => {}
        }
        0
    }
    fn skin_op(&self, sh: &Sh, w: isize, b: isize, c: isize, _d: isize) -> Option<isize> {
        if w != Sl::Image as isize {
            return None;
        }
        let img = if (0..4).contains(&b) { self.images[b as usize].get() } else { 0 };
        if img == 0 {
            return Some(0);
        }
        let (iw, ih) = (sh.img(Im::Width, img) as i32, sh.img(Im::Height, img) as i32);
        // SAFETY: the layout passes two live ints
        unsafe {
            *(c as *mut i32) = iw;
            *(c as *mut i32).add(1) = ih;
        }
        Some(1)
    }
    fn free(&self, sh: &Sh) {
        for i in (0..4).rev() {
            hold_image(sh, &self.images[i], 0);
        }
    }
}

/// `WONHeaderButton`: a WONButton with left-aligned text, 4-pixel pads and no focus, painting a sort arrow.
pub struct WonHeaderButton;

impl ShellKind for WonHeaderButton {
    fn name(&self) -> &'static CStr {
        c"WONHeaderButton"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        let me = sh.comp();
        sh.wb(me, Wb::SetHorzAlign, [0, 0, 0, 0]); // HorzAlign_Left
        for f in [Wf::LeftTextPad, Wf::TopTextPad, Wf::RightTextPad, Wf::BottomTextPad] {
            sh.wb_set(me, f, 4);
        }
        sh.set_flags(0x8, false); // ComponentFlag_WantFocus
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v != V::Paint {
            return 0;
        }
        let me = sh.comp();
        sh.base(V::Paint, a, 0, 0);
        let sort = sh.host(H::SortType, 0, 0, 0, 0) as i32;
        if sort == 2 {
            return 0; // ListSortType_None
        }
        let mut color = 0i32;
        let mut text = 0isize;
        sh.wb(me, Wb::GetButtonState, [a, &mut color as *mut i32 as isize, &mut text as *mut isize as isize, 0]);
        let text_w = if text != 0 { sh.host(H::TextWidth, text, 0, 0, 0) as i32 } else { 0 };
        let pad = sh.wb(me, Wb::Get, [Wf::LeftTextPad as isize, 0, 0, 0]) as i32;
        let ops = guidraw::header_arrow(sort, color, text != 0, text_w, pad, sh.width(), sh.height(), sh.is_pressed());
        for o in &ops {
            sh.draw(a, o);
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, _a: isize, _b: isize) -> isize {
        if op == O::Duplicate {
            // WONHeaderButton::Duplicate: a new header button with this one's attributes
            let n = new_kind(sh, K::WonHeaderButton, 0, 0);
            sh.wb_copy(n, sh.comp());
            return n;
        }
        0
    }
}

/// `WONInputBox`: an input box with a frame over it.
#[derive(Default)]
pub struct WonInputBox {
    frame: FrameRec,
}

impl ShellKind for WonInputBox {
    fn name(&self) -> &'static CStr {
        c"WONInputBox"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v == V::Paint {
            sh.base(V::Paint, a, 0, 0);
            self.frame.paint(sh, a);
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        match op {
            O::SetFrame => self.frame.copy_from(sh, &frame_in(a)),
            O::GetFrame => frame_out(a, self.frame.get()),
            // a new input box with a copy of this one's frame (nothing else)
            O::Duplicate => {
                let n = new_kind(sh, K::WonInputBox, 0, 0);
                let f = self.frame.get();
                sh.shell_on(n, O::SetFrame, &f as *const EeWonFrameIn as isize, 0);
                return n;
            }
            _ => {}
        }
        0
    }
    fn free(&self, sh: &Sh) {
        self.frame.release(sh);
    }
}

/// `WONScrollbar`: a scrollbar with a transparency flag nothing reads.
#[derive(Default)]
pub struct WonScrollbar {
    transparent: Cell<bool>,
}

impl ShellKind for WonScrollbar {
    fn name(&self) -> &'static CStr {
        c"WONScrollbar"
    }
    fn overrides(&self) -> u64 {
        0
    }
    fn init(&self, _sh: &Sh, _args: [isize; 3]) {
        self.transparent.set(false);
    }
    fn virt(&self, _sh: &Sh, _v: V, _a: isize, _b: isize, _c: isize) -> isize {
        0
    }
}

/// `WONComboBox`: a combo box with a frame, the button inside or beside it.
#[derive(Default)]
pub struct WonComboBox {
    draw_frame: Cell<bool>,
    button_in_frame: Cell<bool>,
    frame: FrameRec,
}

impl ShellKind for WonComboBox {
    fn name(&self) -> &'static CStr {
        c"WONComboBox"
    }
    fn overrides(&self) -> u64 {
        bit(V::SizeChanged) | bit(V::DesiredSize) | bit(V::Paint)
    }
    fn init(&self, sh: &Sh, _args: [isize; 3]) {
        self.draw_frame.set(true);
        self.button_in_frame.set(true);
        let edit = sh.host(H::NewBase, N::InputBox as isize, 0, 0, 0);
        sh.host(H::ComboPart, Cp::Edit as isize, edit, 0, 0);
        let list = sh.host(H::NewBase, N::ListArea as isize, 0, 0, 0);
        sh.host(H::ComboPart, Cp::ListArea as isize, list, 0, 0);
        sh.list_on(list, Ls::SetListFlags, 2, 1);
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, b: isize, _c: isize) -> isize {
        match v {
            V::SizeChanged => {
                sh.base(V::SizeChanged, 0, 0, 0);
                sh.skin_layout(Lk::WonCombo, 0, 0, 0, None);
            }
            V::DesiredSize => sh.skin_layout(Lk::WonComboSize, 0, 0, 0, Some((a, b))),
            V::Paint => {
                sh.base(V::Paint, a, 0, 0);
                // the frame, less the button when it sits visibly outside it (guidraw.rs combo_frame)
                let f = self.frame.draw_of(sh, [0; 4]);
                let (df, inf) = (self.draw_frame.get(), self.button_in_frame.get());
                let visible = if df && !inf {
                    crate::compmem::is_visible(sh.host(H::ComboGet, Cp::Button as isize, 0, 0, 0))
                } else {
                    true
                };
                let bw = if df && !inf && visible {
                    crate::compmem::width(sh.host(H::ComboGet, Cp::Button as isize, 0, 0, 0))
                } else {
                    0
                };
                let slots = [self.frame.image(), 0, 0, 0];
                for o in &guidraw::combo_frame(df, inf, visible, bw, &f) {
                    sh.draw_images(a, o, &slots);
                }
            }
            _ => {}
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, _b: isize) -> isize {
        match op {
            O::SetFrame => self.frame.copy_from(sh, &frame_in(a)),
            O::GetFrame => frame_out(a, self.frame.get()),
            O::SetDrawFrameField => self.draw_frame.set(a != 0),
            O::SetButtonInFrame => self.button_in_frame.set(a != 0),
            // a new combo box with the same frame and frame options
            O::Duplicate => {
                let n = new_kind(sh, K::WonComboBox, 0, 0);
                let f = self.frame.get();
                sh.shell_on(n, O::SetFrame, &f as *const EeWonFrameIn as isize, 0);
                sh.shell_on(n, O::SetDrawFrameField, self.draw_frame.get() as isize, 0);
                sh.shell_on(n, O::SetButtonInFrame, self.button_in_frame.get() as isize, 0);
                return n;
            }
            _ => {}
        }
        0
    }
    fn skin(&self, _sh: &Sh, w: Sl, which: isize) -> Option<isize> {
        if w != Sl::Field {
            return None;
        }
        Some(match which {
            x if x == Sf::DrawFrame as isize => self.draw_frame.get() as isize,
            x if x == Sf::ButtonInFrame as isize => self.button_in_frame.get() as isize,
            x => self.frame.field(x).unwrap_or(0),
        })
    }
    fn free(&self, sh: &Sh) {
        self.frame.release(sh);
    }
}

/// `WONScrollContainer` and `WONListCtrl` (a scroll container with a header strip over a multi-column list).
pub struct WonScroll {
    list: bool,
    /// `mHeaderHeight`, `mTransparent`, `mFrame`, `mCorner` (held); the list's `mHeader` (held).
    header_height: Cell<i32>,
    transparent: Cell<bool>,
    frame: FrameRec,
    corner: Cell<isize>,
    header: Cell<isize>,
}

impl WonScroll {
    fn new(list: bool) -> Self {
        WonScroll {
            list,
            header_height: Cell::new(0),
            transparent: Cell::new(false),
            frame: FrameRec::default(),
            corner: Cell::new(0),
            header: Cell::new(0),
        }
    }
    /// `WONScrollContainer::Duplicate`: a new scroll container with this one's attributes.
    fn duplicate_scroll(&self, sh: &Sh) -> isize {
        let n = new_kind(sh, K::WonScrollContainer, 0, 0);
        sh.shell_on(n, O::CopyScrollAttributes, sh.comp(), 0);
        n
    }
}

impl ShellKind for WonScroll {
    fn name(&self) -> &'static CStr {
        if self.list {
            c"WONListCtrl"
        } else {
            c"WONScrollContainer"
        }
    }
    fn overrides(&self) -> u64 {
        bit(V::SizeChanged) | bit(V::Paint)
    }
    fn init(&self, _sh: &Sh, _args: [isize; 3]) {
        self.header_height.set(0);
        self.transparent.set(false);
        if self.list {
            self.header_height.set(0);
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        let me = sh.comp();
        match v {
            V::SizeChanged => {
                sh.base(V::SizeChanged, 0, 0, 0);
                sh.skin_layout(if self.list { Lk::WonList } else { Lk::WonScroll }, 0, 0, 0, None);
            }
            V::Paint => {
                sh.base(V::Paint, a, 0, 0);
                // the corner between two visible scrollbars (opaque), then the frame (guidraw.rs scroll_container)
                let f = self.frame.draw_of(sh, [0; 4]);
                let vert = crate::compmem::is_visible(sh.scroller_on(me, Sc::GetScrollbar, 1, 0));
                let horz = vert && crate::compmem::is_visible(sh.scroller_on(me, Sc::GetScrollbar, 0, 0));
                let slots = [self.frame.image(), self.corner.get(), 0, 0];
                let left = crate::compmem::left(sh.scroller_on(me, Sc::GetScrollbar, 1, 0));
                let top = crate::compmem::top(sh.scroller_on(me, Sc::GetScrollbar, 1, 0));
                let height = crate::compmem::height(sh.scroller_on(me, Sc::GetScrollbar, 1, 0));
                let ops = guidraw::scroll_container(
                    vert,
                    horz,
                    self.transparent.get(),
                    self.corner.get() != 0,
                    left,
                    top.wrapping_add(height),
                    &f,
                );
                for o in &ops {
                    sh.draw_images(a, o, &slots);
                }
            }
            _ => {}
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, b: isize) -> isize {
        match op {
            O::SetFrame => self.frame.copy_from(sh, &frame_in(a)),
            O::GetFrame => frame_out(a, self.frame.get()),
            O::SetCorner => hold_image(sh, &self.corner, a),
            O::GetCorner => return self.corner.get(),
            O::SetTransparentField => self.transparent.set(a != 0),
            O::SetHeaderHeightField => self.header_height.set(a as i32),
            // CopyAttributes(from): from's frame (copied) and corner
            O::CopyScrollAttributes => {
                let mut f = EeWonFrameIn::default();
                sh.shell_on(a, O::GetFrame, &mut f as *mut EeWonFrameIn as isize, 0);
                self.frame.copy_from(sh, &f);
                let corner = sh.shell_on(a, O::GetCorner, 0, 0);
                hold_image(sh, &self.corner, corner);
            }
            O::DuplicateScroll => return self.duplicate_scroll(sh),
            O::Duplicate if !self.list => return self.duplicate_scroll(sh),
            // WONListCtrl::Duplicate: a new list control with the same header height (nothing else)
            O::Duplicate => {
                let n = new_kind(sh, K::WonListCtrl, 0, 0);
                sh.shell_on(n, O::SetHeaderHeightField, self.header_height.get() as isize, 0);
                return n;
            }
            O::SetHeader if self.list => {
                hold_comp(sh, &self.header, a);
                sh.ct_on(sh.comp(), Ct::AddChild, a, 0);
            }
            O::GetHeader if self.list => return self.header.get(),
            O::SetStandardColumnHeader if self.list => {
                let [title, width] = pair(b);
                sh.skin_layout(Lk::WonListTextHeader, a as i32, title, width as i32, None);
            }
            O::SetColumnHeader if self.list => {
                let [comp, width] = pair(b);
                sh.skin_layout(Lk::WonListHeader, a as i32, comp, width as i32, None);
            }
            _ => {}
        }
        0
    }
    fn skin(&self, _sh: &Sh, w: Sl, which: isize) -> Option<isize> {
        match w {
            Sl::Part if self.list && which == Sp::Header as isize => Some(self.header.get()),
            Sl::Field if which == Sf::HeaderHeight as isize => Some(self.header_height.get() as isize),
            Sl::Field => Some(self.frame.field(which).unwrap_or(0)),
            _ => None,
        }
    }
    fn skin_op(&self, sh: &Sh, w: isize, b: isize, c: isize, d: isize) -> Option<isize> {
        match w {
            x if x == Sl::SetHeaderHeight as isize => self.header_height.set(b as i32),
            x if x == Sl::SetColumn as isize && self.list => {
                let args = [c, d];
                sh.shell_on(self.header.get(), O::WonSetColumn, b, args.as_ptr() as isize);
            }
            x if x == Sl::SetColumnItem as isize && self.list => {
                sh.mhead_on(self.header.get(), Mh::SetColumnItem, b, c);
            }
            _ => return None,
        }
        Some(0)
    }
    fn free(&self, sh: &Sh) {
        // ~WONListCtrl: the header; ~WONScrollContainer: the corner, then the frame
        hold_comp(sh, &self.header, 0);
        hold_image(sh, &self.corner, 0);
        self.frame.release(sh);
    }
}

/// `WONHeaderControl`: a list header whose columns are the skin's header buttons.
#[derive(Default)]
pub struct WonHeaderControl {
    /// `mTextLeftPad`, `mSkin`.
    text_left_pad: Cell<i32>,
    skin: Cell<isize>,
}

impl ShellKind for WonHeaderControl {
    fn name(&self) -> &'static CStr {
        c"WONHeaderControl"
    }
    fn overrides(&self) -> u64 {
        0
    }
    fn init(&self, _sh: &Sh, args: [isize; 3]) {
        self.skin.set(args[1]);
        self.text_left_pad.set(0);
    }
    fn virt(&self, _sh: &Sh, _v: V, _a: isize, _b: isize, _c: isize) -> isize {
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, b: isize) -> isize {
        match op {
            // SetColumn(column, title, desiredHeight): a new skin header button with the title on the column
            O::WonSetColumn => {
                let [title, desired] = pair(b);
                // SAFETY: the skin is the game's (or the driver's), live
                let Some(s) = (unsafe { skin(self.skin.get()) }) else { return 0 };
                let button = s.get_header_button(sh);
                sh.comp_ref(button, 1);
                sh.btn_on(button, Bc::SetText, title, 1);
                if desired != 0 {
                    let mut wh = [0i32, 0];
                    sh.host(H::VDesiredSize, button, wh.as_mut_ptr() as isize, 0, 0);
                    // SAFETY: the caller's int
                    unsafe { *(desired as *mut i32) = wh[1] };
                }
                sh.mhead_on(sh.comp(), Mh::SetColumnItem, a, button);
                sh.comp_ref(button, -1);
            }
            O::SetSkin => self.skin.set(a),
            _ => {}
        }
        0
    }
}

/// `WONTabCtrl`: a tab control over a stretched frame image and (opaque) a dialog back, its tabs the skin's.
#[derive(Default)]
pub struct WonTabCtrl {
    /// `mSkin`, `mFrame`, `mBack` (held), `mIsTransparent`.
    skin: Cell<isize>,
    frame: FrameRec,
    back: Cell<isize>,
    transparent: Cell<bool>,
}

impl ShellKind for WonTabCtrl {
    fn name(&self) -> &'static CStr {
        c"WONTabCtrl"
    }
    fn overrides(&self) -> u64 {
        0
    }
    fn init(&self, _sh: &Sh, args: [isize; 3]) {
        self.skin.set(args[0]);
        self.transparent.set(false);
    }
    fn virt(&self, _sh: &Sh, _v: V, _a: isize, _b: isize, _c: isize) -> isize {
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, b: isize) -> isize {
        let me = sh.comp();
        match op {
            // SetFrame: a stretched frame image over the page area and, when opaque, a dialog back inside it
            O::SetFrame => {
                self.frame.copy_from(sh, &frame_in(a));
                let image = sh.host(H::NewStretchImage, self.frame.image(), 0, 0, 0);
                sh.comp_ref(image, 1);
                let page = sh.host(H::TabLayoutPoint, 0, 0, 0, 0);
                sh.layout(image, 0x5003, page, [0; 5]);
                sh.ct_on(me, Ct::AddChild, image, 0);
                if !self.transparent.get() {
                    let back = new_kind(sh, K::MsDlgBack, 0, 0);
                    hold_comp(sh, &self.back, back);
                    let f = self.frame.get();
                    let r = [
                        f.left,
                        f.top,
                        f.right.wrapping_neg().wrapping_sub(f.left),
                        f.top.wrapping_neg().wrapping_sub(f.bottom),
                        0,
                    ];
                    sh.layout(self.back.get(), 0x5003, image, r);
                    sh.ct_on(me, Ct::AddChild, self.back.get(), 0);
                }
                sh.comp_ref(image, -1);
            }
            O::GetFrame => frame_out(a, self.frame.get()),
            // SetTransparent: removes the dialog back; never stores the flag or adds the back again
            O::SetTransparent => {
                if self.back.get() != 0 && a != 0 {
                    sh.ct_on(me, Ct::RemoveChild, self.back.get(), 0);
                }
            }
            // AddStandardTab(text, container): a page with the skin's tab button showing the text
            O::AddStandardTab => {
                // SAFETY: the skin is the game's (or the driver's), live
                let Some(s) = (unsafe { skin(self.skin.get()) }) else { return 0 };
                let button = s.get_tab_button(sh);
                sh.comp_ref(button, 1);
                sh.btn_on(button, Bc::SetText, a, 1);
                sh.comp_on(button, Cc::SetDesiredWidth, 0, 0);
                sh.tab_on(me, Tb::AddTab, button, b);
                sh.comp_ref(button, -1);
            }
            // Duplicate: a new tab control on the same skin with the same attributes, transparency and frame
            O::Duplicate => {
                let n = new_kind(sh, K::WonTabCtrl, self.skin.get(), 0);
                sh.tab_on(n, Tb::CopyAttributes, sh.obj_of::<EeWonTabCtrl>(me, crate::registry::EeWonGuiKind::Tab), 0);
                sh.shell_on(n, O::SetTransparent, self.transparent.get() as isize, 0);
                let f = self.frame.get();
                sh.shell_on(n, O::SetFrame, &f as *const EeWonFrameIn as isize, 0);
                return n;
            }
            O::SetSkin => self.skin.set(a),
            _ => {}
        }
        0
    }
    fn free(&self, sh: &Sh) {
        hold_comp(sh, &self.back, 0);
        self.frame.release(sh);
    }
}

/// `WONFrameComponent`: a component painting a frame image over itself.
#[derive(Default)]
pub struct WonFrameComponent {
    frame: FrameRec,
}

impl ShellKind for WonFrameComponent {
    fn name(&self) -> &'static CStr {
        c"WONFrameComponent"
    }
    fn overrides(&self) -> u64 {
        bit(V::Paint)
    }
    fn init(&self, sh: &Sh, args: [isize; 3]) {
        // WONFrameComponent(const WONFrame&): a copy; the default constructor: no image and no borders
        if args[0] != 0 {
            self.frame.copy_from(sh, &frame_in(args[0]));
        }
    }
    fn virt(&self, sh: &Sh, v: V, a: isize, _b: isize, _c: isize) -> isize {
        if v == V::Paint {
            sh.base(V::Paint, a, 0, 0);
            self.frame.paint(sh, a);
        }
        0
    }
    fn op(&self, sh: &Sh, op: O, a: isize, b: isize) -> isize {
        match op {
            O::SetFrameImage => self.frame.set_image(sh, a),
            O::SetFrameBorder => self.frame.border(a).set(b as i32),
            O::GetFrameBorder => return self.frame.border(a).get() as isize,
            O::GetFrame => frame_out(a, self.frame.get()),
            _ => {}
        }
        0
    }
    fn free(&self, sh: &Sh) {
        self.frame.release(sh);
    }
}

#[cfg(test)]
#[path = "shell_won_tests.rs"]
mod tests;
