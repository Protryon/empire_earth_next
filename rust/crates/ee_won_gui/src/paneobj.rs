//! WONGUI's ScrollArea as a whole Rust object (R6 WON round 25): the scrolled container under every lobby list
//! and text area (ListArea, its subclasses and TextArea derive from it, their Rust objects call it back as their
//! base). The original's ScrollArea.cpp and the scroll area's part of ScrollAreaViewport.cpp with their state:
//! the offsets, the content, the last adjusted offsets and content, the last view size, the padding, the user's
//! scroll, the scroller (borrowed), the view, the extra height and the scroll amounts.
//!
//! The C++ `ScrollArea` stays the type the lists, the scrollers and the configs hold (its viewport and background
//! stay C++ members, its vtable the subclasses' overrides); its virtuals and methods are one-line forwarders to
//! `ee_won_pane_call`. Every virtual the original called on itself (AdjustPosition, Virtual08, ScrollSizeChanged,
//! ScrollPaint) is called through the C++ object, so the subclasses' overrides run. `guiscroll` keeps the
//! clamp, view and size arithmetic, as before.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guidraw::{EeWonDrawOp, EE_WON_DRAW_POP_CLIP, EE_WON_DRAW_PUSH_CLIP, EE_WON_DRAW_TRANSLATE};
use crate::guihost::{EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guilayout::{self, EeWonRect};
use crate::guiscroll::{self, EeWonScrollArea};

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonPaneCall {
    /// `AdjustPosition(a != 0)`.
    AdjustPosition = 1,
    /// `Virtual08` (SizeChanged).
    SizeChanged,
    /// `GetDesiredSize(*a, *b)` (`a`, `b` point at i32s).
    DesiredSize,
    /// `MouseWheel(a)` (0 or 1).
    MouseWheel,
    /// `Paint(*a)` (`a` the Graphics).
    Paint,
    SetDelegatePos,
    /// `OnBottom()` (0 or 1).
    OnBottom,
    /// `SetBorderPadding(a[0], a[1], a[2], a[3])` (`a` points at the left, top, right and bottom padding).
    SetBorderPadding,
    /// `ScrollTranslate(*a, *b)` (`a`, `b` point at i32s).
    ScrollTranslate,
    /// `ScrollUntranslate(*a, *b)`.
    ScrollUntranslate,
    /// `SetSizeEqualToTotal(a, b)`.
    SetSizeEqualToTotal,
    /// `IsRectVisible(*a)` (`a` a `WONRectangle`; 0 or 1).
    IsRectVisible,
    /// `SetScrollDimension(a, b)`.
    SetScrollDimension,
    /// `SetScrollbar(a)` (a `Scrollbar*`).
    SetScrollbar,
    /// `SetScroller(a)` (a `Scroller*`, borrowed).
    SetScroller,
    /// `SetVertOffset(a)`.
    SetVertOffset,
    /// `SetOffsets(a, b)` (horizontal, vertical).
    SetOffsets,
    /// the field `a` (`EeWonGuiField`).
    GetField,
    /// the field `a` = `b`.
    SetField,
}

#[derive(Debug, Default)]
struct State {
    vert: i32,
    horz: i32,
    total_h: i32,
    total_w: i32,
    /// the offsets and content the last AdjustPosition left
    adjusted: [i32; 4],
    old_w: i32,
    old_h: i32,
    /// left, top, right, bottom
    pads: [i32; 4],
    user_scrolled: bool,
    scroller: isize,
    view: EeWonRect,
    extra_h: i32,
    /// up, down, left, right
    amounts: [i32; 4],
}

/// The Rust ScrollArea.
pub struct EeWonScrollPane {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonScrollPane {
    /// The constructor's state: every number 0, the scroll amounts 10, no scroller (the C++ constructor makes the
    /// viewport and adds it).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonScrollPane { comp, host, st: RefCell::new(State { amounts: [10; 4], ..State::default() }) }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, 0) },
            None => 0,
        }
    }
    fn draw(&self, g: isize, kind: i32, args: [i32; 4]) {
        let o = EeWonDrawOp { kind, a: args[0], b: args[1], c: args[2], d: args[3], ..EeWonDrawOp::default() };
        self.op(Op::Draw, g, &o as *const EeWonDrawOp as isize, 0);
    }
    fn numbers(&self) -> EeWonScrollArea {
        let s = self.st.borrow();
        EeWonScrollArea {
            vert_offset: s.vert,
            horz_offset: s.horz,
            total_width: s.total_w,
            total_height: s.total_h,
            extra_height: s.extra_h,
            view_x: s.view.x,
            view_y: s.view.y,
            view_width: s.view.width,
            view_height: s.view.height,
        }
    }

    pub fn field(&self, f: i32) -> i32 {
        let s = self.st.borrow();
        match f {
            x if x == F::ScrollWidth as i32 => s.view.width,
            x if x == F::ScrollHeight as i32 => s.view.height,
            x if x == F::ScrollX as i32 => s.view.x,
            x if x == F::ScrollY as i32 => s.view.y,
            x if x == F::VertOffset as i32 => s.vert,
            x if x == F::HorzOffset as i32 => s.horz,
            x if x == F::TotalWidth as i32 => s.total_w,
            x if x == F::TotalHeight as i32 => s.total_h,
            x if x == F::ExtraHeight as i32 => s.extra_h,
            x if x == F::UserHasScrolled as i32 => i32::from(s.user_scrolled),
            x if x == F::UpScrollAmount as i32 => s.amounts[0],
            x if x == F::DownScrollAmount as i32 => s.amounts[1],
            x if x == F::LeftScrollAmount as i32 => s.amounts[2],
            x if x == F::RightScrollAmount as i32 => s.amounts[3],
            x if x == F::LeftPad as i32 => s.pads[0],
            x if x == F::TopPad as i32 => s.pads[1],
            x if x == F::RightPad as i32 => s.pads[2],
            x if x == F::BottomPad as i32 => s.pads[3],
            _ => 0,
        }
    }

    pub fn set_field(&self, f: i32, v: i32) {
        let mut s = self.st.borrow_mut();
        match f {
            x if x == F::ScrollWidth as i32 => s.view.width = v,
            x if x == F::ScrollHeight as i32 => s.view.height = v,
            x if x == F::ScrollX as i32 => s.view.x = v,
            x if x == F::ScrollY as i32 => s.view.y = v,
            x if x == F::VertOffset as i32 => s.vert = v,
            x if x == F::HorzOffset as i32 => s.horz = v,
            x if x == F::TotalWidth as i32 => s.total_w = v,
            x if x == F::TotalHeight as i32 => s.total_h = v,
            x if x == F::ExtraHeight as i32 => s.extra_h = v,
            x if x == F::UserHasScrolled as i32 => s.user_scrolled = v != 0,
            x if x == F::UpScrollAmount as i32 => s.amounts[0] = v,
            x if x == F::DownScrollAmount as i32 => s.amounts[1] = v,
            x if x == F::LeftScrollAmount as i32 => s.amounts[2] = v,
            x if x == F::RightScrollAmount as i32 => s.amounts[3] = v,
            x if x == F::LeftPad as i32 => s.pads[0] = v,
            x if x == F::TopPad as i32 => s.pads[1] = v,
            x if x == F::RightPad as i32 => s.pads[2] = v,
            x if x == F::BottomPad as i32 => s.pads[3] = v,
            _ => {}
        }
    }

    pub fn set_delegate_pos(&self) {
        let r = {
            let s = self.st.borrow();
            EeWonRect4 {
                x: s.view.x.wrapping_sub(s.horz),
                y: s.view.y.wrapping_sub(s.vert),
                w: s.total_w,
                h: s.total_h,
            }
        };
        let viewport = self.op(Op::GetViewport, 0, 0, 0);
        self.op(Op::ChildSetPosSize, viewport, &r as *const EeWonRect4 as isize, 0);
    }

    pub fn adjust_position(&self, scrollbar: bool) {
        if scrollbar {
            self.st.borrow_mut().user_scrolled = true;
        }
        let (v, h) = guiscroll::clamp_offsets(&self.numbers());
        let scroller = {
            let mut s = self.st.borrow_mut();
            s.vert = v;
            s.horz = h;
            s.scroller
        };
        if scroller != 0 {
            self.op(Op::ScrollerNotify, scroller, scrollbar as isize, 0);
        }
        {
            // read after the scroller's notification (it may lay the area out again)
            let mut s = self.st.borrow_mut();
            s.adjusted = [s.vert, s.horz, s.total_h, s.total_w];
            s.old_w = s.view.width;
            s.old_h = s.view.height;
        }
        self.set_delegate_pos();
        self.op(Op::Invalidate, 0, 0, 0);
    }

    pub fn set_vert_offset(&self, offset: i32) {
        self.st.borrow_mut().vert = offset;
        self.op(Op::VirtAdjustPosition, 0, 0, 0);
    }

    pub fn set_offsets(&self, horizontal: i32, vertical: i32) {
        {
            let mut s = self.st.borrow_mut();
            s.horz = horizontal;
            s.vert = vertical;
        }
        self.op(Op::VirtAdjustPosition, 0, 0, 0);
    }

    pub fn size_changed(&self) {
        self.op(Op::BaseSizeChanged, 0, 0, 0);
        let w = self.op(Op::Get, F::Width as isize, 0, 0) as i32;
        let h = self.op(Op::Get, F::Height as isize, 0, 0) as i32;
        let pads = self.st.borrow().pads;
        let n = guiscroll::view_layout(&self.numbers(), w, h, pads[0], pads[1], pads[2], pads[3]);
        {
            let mut s = self.st.borrow_mut();
            s.view = EeWonRect { x: n.view_x, y: n.view_y, width: n.view_width, height: n.view_height };
            s.vert = n.vert_offset;
        }
        self.op(Op::VirtScrollSizeChanged, 0, 0, 0);
        self.op(Op::VirtAdjustPosition, 0, 0, 0);
    }

    pub fn set_border_padding(&self, pads: [i32; 4]) {
        {
            let mut s = self.st.borrow_mut();
            if s.pads == pads {
                return;
            }
            s.pads = pads;
        }
        self.op(Op::VirtSizeChanged, 0, 0, 0);
    }

    pub fn scroll_translate(&self, x: i32, y: i32) -> (i32, i32) {
        let s = self.st.borrow();
        (x.wrapping_add(s.horz.wrapping_sub(s.view.x)), y.wrapping_add(s.vert.wrapping_sub(s.view.y)))
    }

    pub fn scroll_untranslate(&self, x: i32, y: i32) -> (i32, i32) {
        let s = self.st.borrow();
        (x.wrapping_add(s.view.x.wrapping_sub(s.horz)), y.wrapping_add(s.view.y.wrapping_sub(s.vert)))
    }

    pub fn mouse_wheel(&self, amount: i32) -> bool {
        if self.op(Op::BaseMouseWheel, amount as isize, 0, 0) != 0 {
            return true;
        }
        {
            let mut s = self.st.borrow_mut();
            s.user_scrolled = true;
            s.vert = s.vert.wrapping_sub(amount);
        }
        self.op(Op::VirtAdjustPosition, 0, 0, 0);
        true
    }

    pub fn set_size_equal_to_total(&self, min_width: i32, min_height: i32) {
        let pads = self.st.borrow().pads;
        let (w, h) = guiscroll::size_equal_to_total(&self.numbers(), min_width, min_height, pads);
        self.op(Op::SetSize, w as isize, h as isize, 0);
    }

    pub fn desired_size(&self) -> (i32, i32) {
        let s = self.st.borrow();
        guiscroll::desired_size(s.total_w, s.total_h)
    }

    pub fn is_rect_visible(&self, r: EeWonRect) -> bool {
        let s = self.st.borrow();
        guilayout::intersects(r, EeWonRect { x: s.horz, y: s.vert, width: s.view.width, height: s.view.height })
    }

    pub fn paint(&self, g: isize) {
        let (horz, vert, view) = {
            let s = self.st.borrow();
            (s.horz, s.vert, s.view)
        };
        self.op(Op::PaneBackground, g, horz as isize, vert as isize);
        self.draw(g, EE_WON_DRAW_PUSH_CLIP, [view.x, view.y, view.width, view.height]);
        self.op(Op::ContainerPaint, g, 0, 0);
        let (dx, dy) = self.scroll_translate(0, 0);
        self.draw(g, EE_WON_DRAW_TRANSLATE, [dx.wrapping_neg(), dy.wrapping_neg(), 0, 0]);
        self.op(Op::VirtScrollPaint, g, 0, 0);
        self.draw(g, EE_WON_DRAW_TRANSLATE, [dx, dy, 0, 0]);
        self.draw(g, EE_WON_DRAW_POP_CLIP, [0; 4]);
    }

    pub fn set_scroll_dimension(&self, width: i32, height: i32) {
        {
            let mut s = self.st.borrow_mut();
            s.total_w = width;
            s.total_h = height;
        }
        self.op(Op::VirtAdjustPosition, 0, 0, 0);
    }

    pub fn set_scroller(&self, scroller: isize) {
        self.st.borrow_mut().scroller = scroller;
        if scroller != 0 {
            self.op(Op::ScrollerSetArea, scroller, 0, 0);
        }
    }

    pub fn set_scrollbar(&self, bar: isize) {
        let scroller = self.st.borrow().scroller;
        let mut s = self.op(Op::ScrollerAsBars, scroller, 0, 0);
        if s == 0 {
            s = self.op(Op::NewScrollbarScroller, 0, 0, 0);
            self.set_scroller(s);
        }
        self.op(Op::ScrollerSetScrollbar, s, bar, 0);
    }

    pub fn call(&self, call: EeWonPaneCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonPaneCall;
        let (ai, bi) = (a as i32, b as i32);
        let _ = c;
        match call {
            C::AdjustPosition => self.adjust_position(a != 0),
            C::SizeChanged => self.size_changed(),
            C::DesiredSize => {
                let (w, h) = self.desired_size();
                // SAFETY: the C++ passes two writable i32s
                unsafe {
                    *(a as *mut i32) = w;
                    *(b as *mut i32) = h;
                }
            }
            C::MouseWheel => return self.mouse_wheel(ai) as isize,
            C::Paint => self.paint(a),
            C::SetDelegatePos => self.set_delegate_pos(),
            C::OnBottom => return guiscroll::on_bottom(&self.numbers()) as isize,
            C::SetBorderPadding => {
                // SAFETY: the C++ passes four readable i32s
                let p = unsafe { *(a as *const [i32; 4]) };
                self.set_border_padding(p);
            }
            C::ScrollTranslate | C::ScrollUntranslate => {
                // SAFETY: the C++ passes two writable i32s
                unsafe {
                    let (x, y) = (*(a as *const i32), *(b as *const i32));
                    let (x, y) = if call == C::ScrollTranslate {
                        self.scroll_translate(x, y)
                    } else {
                        self.scroll_untranslate(x, y)
                    };
                    *(a as *mut i32) = x;
                    *(b as *mut i32) = y;
                }
            }
            C::SetSizeEqualToTotal => self.set_size_equal_to_total(ai, bi),
            C::IsRectVisible => {
                // SAFETY: the C++ passes a WONRectangle (x, y, width, height)
                let r = unsafe { *(a as *const EeWonRect) };
                return self.is_rect_visible(r) as isize;
            }
            C::SetScrollDimension => self.set_scroll_dimension(ai, bi),
            C::SetScrollbar => self.set_scrollbar(a),
            C::SetScroller => self.set_scroller(a),
            C::SetVertOffset => self.set_vert_offset(ai),
            C::SetOffsets => self.set_offsets(ai, bi),
            C::GetField => return self.field(ai) as isize,
            C::SetField => self.set_field(ai, bi),
        }
        0
    }
}

/// A new Rust ScrollArea for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_pane_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonScrollPane {
    Box::into_raw(Box::new(EeWonScrollPane::new(component, host)))
}

/// # Safety
/// `pane` came from `ee_won_pane_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_pane_free(pane: *mut EeWonScrollPane) {
    if !pane.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(pane) });
    }
}

/// One of ScrollArea's methods (`EeWonPaneCall`).
///
/// # Safety
/// `pane` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_pane_call(
    pane: *const EeWonScrollPane,
    call: EeWonPaneCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*pane).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "paneobj_tests.rs"]
mod tests;
