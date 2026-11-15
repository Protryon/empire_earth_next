//! The skins' layouts in Rust (R6 WON round 37): where the Windows skin's and the WON skin's composite controls put
//! their parts when their size changes, and the sizes they ask for. MSComboBox's and WONComboBox's `Virtual08` (the
//! drop button beside or inside the frame, the edit box in the rest), MSScrollContainer's and WONScrollContainer's
//! (the visible scrollbars along the right and bottom inside the frame, the scroll area in the rest below the
//! header), MSListCtrl's and WONListCtrl's (the header strip over the list), WONListCtrl's column headers (the first
//! one fixes the header's height), WONComboBox's, MSEtchedFrameGroup's and WONSimpleButton's `GetDesiredSize`.
//!
//! The C++ classes keep their members (the parts the constructors and skins make, the frames, the flags); each
//! method is a forwarder to `ee_won_skin_layout`, which reads and moves the parts through the host's `SkinLay` op
//! (`EeWonSkinLay`): a part or field of the receiver, a component's size, place and visibility, its `SetPosSize`,
//! and the few other calls the originals made. Each layout makes the original's calls in its order; ints wrap as
//! the original's did (-fwrapv).

use std::ffi::c_void;

use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp as Op};

/// Which method (`ee_won_skin_layout`'s `kind`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSkinLayKind {
    /// `MSComboBox::Virtual08` (after `ComboBox::Virtual08`).
    MsCombo = 1,
    /// `MSScrollContainer::Virtual08` (after `Container::Virtual08`).
    MsScroll,
    /// `MSListCtrl::Virtual08`: the scroll container's, then the header.
    MsList,
    /// `WONComboBox::GetDesiredSize` (the size out).
    WonComboSize,
    /// `WONComboBox::Virtual08` (after `ComboBox::Virtual08`).
    WonCombo,
    /// `WONScrollContainer::Virtual08` (after `Container::Virtual08`).
    WonScroll,
    /// `WONListCtrl::Virtual08`: the scroll container's, then the header.
    WonList,
    /// `WONListCtrl::SetStandarColumnHeader(a, *(const GUIString*)b, c)`.
    WonListTextHeader,
    /// `WONListCtrl::SetColumnHeader(a, (Component*)b, c)`.
    WonListHeader,
    /// `MSEtchedFrameGroup::GetDesiredSize` (the size out).
    EtchedSize,
    /// `WONSimpleButton::GetDesiredSize` (the size out).
    SimpleButtonSize,
}

/// A part of the receiver (`EeWonSkinLay::Part`): a `Component*`, 0 for none.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSkinPart {
    /// A combo box's `mButton`.
    Button = 1,
    /// A combo box's `mEdit`.
    Edit,
    /// A combo box's `mListArea`.
    ListArea,
    /// A scroll container's `GetScrollArea()`.
    ScrollArea,
    /// `GetScrollbar(true)`.
    VertBar,
    /// `GetScrollbar(false)`.
    HorzBar,
    /// A list control's `mHeader`.
    Header,
    /// An etched group's `mLabel`.
    Label,
    /// An etched group's `mComponent`.
    Contained,
}

/// A field of the receiver (`EeWonSkinLay::Field`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSkinField {
    DrawFrame = 1,
    Draw3DFrame,
    ButtonInFrame,
    HeaderHeight,
    /// `mFrame.mLeft` (the WON skin's frame).
    FrameLeft,
    FrameTop,
    FrameRight,
    FrameBottom,
}

/// The host's calls (`EeWonGuiOp::SkinLay`; `a` this, `b`, `c`, `d` its arguments; `k` the layout's kind).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSkinLay {
    /// The receiver's part `c` (`EeWonSkinPart`) as the kind `b`'s class holds it.
    Part = 1,
    /// The receiver's field `c` (`EeWonSkinField`) as the kind `b`'s class holds it.
    Field,
    /// `mHeaderHeight = b`.
    SetHeaderHeight,
    /// `((Component*)b)->Width()`.
    Width,
    Height,
    Left,
    Top,
    Right,
    /// `IsVisible()` (0 or 1).
    Visible,
    /// `((Component*)b)->SetPosSize(r[0], r[1], r[2], r[3])` with `r` the four ints at `c`.
    SetPosSize,
    /// `((Component*)b)->GetDesiredSize(wh[0], wh[1])` with `wh` the two ints at `c`.
    DesiredSize,
    /// `((InputBox*)b)->SetLeftPad(c)`.
    SetLeftPad,
    /// `((ListArea*)b)->GetNumItems()`.
    ListCount,
    /// `((ListArea*)b)->GetTotalWidth()`.
    ListTotalWidth,
    /// `mHeader->SetColumn(b, *(const GUIString*)c, (int*)d)`.
    SetColumn,
    /// `mHeader->SetColumnItem(b, (Component*)c)`.
    SetColumnItem,
    /// `((MultiListArea*)GetScrollArea())->SetColumnWidth(b, c, false)`.
    SetColumnWidth,
    /// `mImages[b]`'s width and height into the two ints at `c` (0: no image, else 1).
    Image,
}

struct Host {
    me: *mut c_void,
    host: EeWonGuiHostFn,
    kind: EeWonSkinLayKind,
}

impl Host {
    fn op(&self, w: EeWonSkinLay, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this component
            Some(h) => unsafe { h(self.me, Op::SkinLay, w as isize, b, c, d) },
            None => 0,
        }
    }
    fn part(&self, p: EeWonSkinPart) -> isize {
        self.op(EeWonSkinLay::Part, self.kind as isize, p as isize, 0)
    }
    fn field(&self, f: EeWonSkinField) -> i32 {
        self.op(EeWonSkinLay::Field, self.kind as isize, f as isize, 0) as i32
    }
    fn me(&self) -> isize {
        self.me as isize
    }
    fn get(&self, w: EeWonSkinLay, c: isize) -> i32 {
        self.op(w, c, 0, 0) as i32
    }
    fn width(&self, c: isize) -> i32 {
        self.get(EeWonSkinLay::Width, c)
    }
    fn height(&self, c: isize) -> i32 {
        self.get(EeWonSkinLay::Height, c)
    }
    fn visible(&self, c: isize) -> bool {
        self.get(EeWonSkinLay::Visible, c) != 0
    }
    fn place(&self, c: isize, x: i32, y: i32, w: i32, h: i32) {
        let r = [x, y, w, h];
        self.op(EeWonSkinLay::SetPosSize, c, r.as_ptr() as isize, 0);
    }
}

/// MSComboBox's and WONComboBox's drop button and edit box (`Virtual08`).
fn combo(h: &Host, won: bool) {
    let (mut top, mut bottom, mut right, mut left) = (0i32, 0i32, 0i32, 0i32);
    if h.field(EeWonSkinField::DrawFrame) != 0 {
        if won {
            left = h.field(EeWonSkinField::FrameLeft);
            top = h.field(EeWonSkinField::FrameTop);
            bottom = h.field(EeWonSkinField::FrameBottom);
            right = h.field(EeWonSkinField::FrameRight);
        } else {
            top = 2;
            bottom = 2;
            right = 2;
        }
    }
    let me = h.me();
    let (button, edit) = (h.part(EeWonSkinPart::Button), h.part(EeWonSkinPart::Edit));
    let input_h = h.height(me);
    let button_h = input_h.wrapping_sub(bottom).wrapping_sub(top);
    let mut button_w = h.width(button);
    if !h.visible(button) {
        button_w = 0;
    }
    if !won {
        let input_w = h.width(me).wrapping_sub(button_w).wrapping_sub(left).wrapping_sub(right);
        if h.visible(button) {
            h.place(button, input_w.wrapping_add(left), top, button_w, button_h);
        }
        h.place(edit, left, 0, input_w, input_h);
        return;
    }
    let input_w = h.width(me).wrapping_sub(button_w).wrapping_sub(right);
    h.op(EeWonSkinLay::SetLeftPad, edit, left.wrapping_add(2) as isize, 0);
    h.place(edit, 0, 0, input_w, input_h);
    if !h.visible(button) {
        return;
    }
    if h.field(EeWonSkinField::ButtonInFrame) != 0 {
        h.place(button, h.get(EeWonSkinLay::Right, edit), top, button_w, button_h);
    } else {
        h.place(button, h.width(me).wrapping_sub(button_w), 0, button_w, h.height(me));
    }
}

/// MSScrollContainer's and WONScrollContainer's scrollbars and scroll area (`Virtual08`).
fn scroll(h: &Host, won: bool) {
    let area = h.part(EeWonSkinPart::ScrollArea);
    if area == 0 {
        return;
    }
    let (vbar, hbar) = (h.part(EeWonSkinPart::VertBar), h.part(EeWonSkinPart::HorzBar));
    let horz_on = h.visible(hbar);
    let vert_on = h.visible(vbar);
    let (left, right, top, bottom) = if won {
        (
            h.field(EeWonSkinField::FrameLeft),
            h.field(EeWonSkinField::FrameRight),
            h.field(EeWonSkinField::FrameTop),
            h.field(EeWonSkinField::FrameBottom),
        )
    } else {
        let off =
            if h.field(EeWonSkinField::DrawFrame) != 0 && h.field(EeWonSkinField::Draw3DFrame) != 0 { 2 } else { 0 };
        (off, off, off, off)
    };
    let me = h.me();
    let bar_w = h.width(vbar);
    let bar_h = h.height(hbar);
    let mut scroll_w = h.width(me).wrapping_sub(left).wrapping_sub(right);
    if vert_on {
        scroll_w = scroll_w.wrapping_sub(bar_w);
    }
    let mut scroll_h = h.height(me).wrapping_sub(top).wrapping_sub(bottom);
    if horz_on {
        scroll_h = scroll_h.wrapping_sub(bar_h);
    }
    if vert_on {
        h.place(vbar, h.width(me).wrapping_sub(bar_w).wrapping_sub(right), top, bar_w, scroll_h);
    }
    if horz_on {
        h.place(hbar, left, h.height(me).wrapping_sub(bar_h).wrapping_sub(bottom), scroll_w, bar_h);
    }
    let header_h = h.field(EeWonSkinField::HeaderHeight);
    h.place(area, left, header_h.wrapping_add(top), scroll_w, scroll_h.wrapping_sub(header_h));
}

/// MSListCtrl's and WONListCtrl's header over the list (after the scroll container's layout).
fn list_header(h: &Host) {
    let area = h.part(EeWonSkinPart::ScrollArea);
    let header_h = h.field(EeWonSkinField::HeaderHeight);
    h.place(
        h.part(EeWonSkinPart::Header),
        h.get(EeWonSkinLay::Left, area),
        h.get(EeWonSkinLay::Top, area).wrapping_sub(header_h),
        h.width(area),
        header_h,
    );
}

/// WONListCtrl's column headers: the first one sets the header height (its own, else 15).
fn column_header(h: &Host, col: i32, text_or_comp: isize, width: i32, text: bool) {
    if text {
        let mut desired = 0i32;
        h.op(EeWonSkinLay::SetColumn, col as isize, text_or_comp, &mut desired as *mut i32 as isize);
        if h.field(EeWonSkinField::HeaderHeight) == 0 {
            h.op(EeWonSkinLay::SetHeaderHeight, if desired > 0 { desired } else { 15 } as isize, 0, 0);
        }
    } else {
        if h.field(EeWonSkinField::HeaderHeight) == 0 {
            let hh = h.height(text_or_comp);
            h.op(EeWonSkinLay::SetHeaderHeight, if hh > 0 { hh } else { 15 } as isize, 0, 0);
        }
        h.op(EeWonSkinLay::SetColumnItem, col as isize, text_or_comp, 0);
    }
    h.op(EeWonSkinLay::SetColumnWidth, col as isize, width as isize, 0);
}

/// The desired sizes.
fn desired(h: &Host, w: &mut i32, ht: &mut i32) {
    match h.kind {
        EeWonSkinLayKind::WonComboSize => {
            let edit = h.part(EeWonSkinPart::Edit);
            if edit != 0 {
                let mut wh = [*w, *ht];
                h.op(EeWonSkinLay::DesiredSize, edit, wh.as_mut_ptr() as isize, 0);
                *w = wh[0];
                *ht = wh[1].wrapping_add(2);
            } else {
                *ht = 22;
            }
            let list = h.part(EeWonSkinPart::ListArea);
            if list != 0 && h.get(EeWonSkinLay::ListCount, list) > 0 {
                *w = 10i32.wrapping_add(h.get(EeWonSkinLay::ListTotalWidth, list));
                let button = h.part(EeWonSkinPart::Button);
                if button != 0 {
                    *w = w.wrapping_add(h.width(button));
                }
            } else {
                *w = 100;
            }
        }
        EeWonSkinLayKind::EtchedSize => {
            let c = h.part(EeWonSkinPart::Contained);
            if c != 0 {
                *w = h.width(c).wrapping_add(20);
                *ht = h.height(c).wrapping_add(h.height(h.part(EeWonSkinPart::Label))).wrapping_add(20);
            } else {
                *w = 20;
                *ht = 20;
            }
        }
        EeWonSkinLayKind::SimpleButtonSize => {
            *w = 0;
            *ht = 0;
            for i in 0..4 {
                let mut wh = [0i32; 2];
                if h.op(EeWonSkinLay::Image, i, wh.as_mut_ptr() as isize, 0) == 0 {
                    continue;
                }
                if wh[0] > *w {
                    *w = wh[0];
                }
                if wh[1] > *ht {
                    *ht = wh[1];
                }
            }
        }
        _ => {}
    }
}

/// One layout (`kind`) of the component `me` through `host`; `a`, `b`, `c` the column header's arguments; `w`, `h`
/// the desired size's out (and in: a WONComboBox's edit box writes both before the width is set).
#[allow(clippy::too_many_arguments)]
pub fn layout(
    kind: EeWonSkinLayKind,
    me: *mut c_void,
    host: EeWonGuiHostFn,
    a: i32,
    b: isize,
    c: i32,
    w: &mut i32,
    ht: &mut i32,
) {
    let h = Host { me, host, kind };
    use EeWonSkinLayKind as K;
    match kind {
        K::MsCombo => combo(&h, false),
        K::WonCombo => combo(&h, true),
        K::MsScroll => scroll(&h, false),
        K::WonScroll => scroll(&h, true),
        K::MsList => {
            scroll(&h, false);
            list_header(&h);
        }
        K::WonList => {
            scroll(&h, true);
            list_header(&h);
        }
        K::WonListTextHeader => column_header(&h, a, b, c, true),
        K::WonListHeader => column_header(&h, a, b, c, false),
        K::WonComboSize | K::EtchedSize | K::SimpleButtonSize => desired(&h, w, ht),
    }
}

/// `layout` for the C++ (`w`, `h` null for a layout that has no size out).
///
/// # Safety
/// `me` is the component `host` serves; `w` and `h` are null or point at ints.
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_won_skin_layout(
    kind: EeWonSkinLayKind,
    me: *mut c_void,
    host: EeWonGuiHostFn,
    a: i32,
    b: isize,
    c: i32,
    w: *mut i32,
    h: *mut i32,
) {
    let (mut ww, mut hh) = (0i32, 0i32);
    // SAFETY: the contract
    unsafe {
        if !w.is_null() {
            ww = *w;
        }
        if !h.is_null() {
            hh = *h;
        }
    }
    layout(kind, me, host, a, b, c, &mut ww, &mut hh);
    // SAFETY: the contract
    unsafe {
        if !w.is_null() {
            *w = ww;
        }
        if !h.is_null() {
            *h = hh;
        }
    }
}

#[cfg(test)]
#[path = "skinlay_tests.rs"]
mod tests;
