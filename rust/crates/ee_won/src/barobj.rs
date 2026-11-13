//! WONGUI's Scrollbar as a whole Rust object (R6 WON round 25): every lobby list's and text area's scroll bar
//! (MSScrollbar and WONScrollbar derive from it). The original's Scrollbar.cpp with its state: the up and down
//! arrows, the track (`mScrollRect`) and the thumb (held by reference, as the original's `SmartPtr`s), the
//! orientation, the thumb options, the position, page and total, the line and page steps and the drag.
//!
//! The C++ `Scrollbar` stays the type the skins, the configs and the scrollers hold (a `Container` in the tree,
//! its vtable); its virtuals and methods are one-line forwarders to `ee_won_bar_call`, and every call out is an op
//! of the component op table (`guihost`). `guiscroll` keeps the thumb's and the steps' arithmetic, as before.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guihost::{EeWonGuiEvent, EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guiscroll::{self, EeWonScrollPress, EeWonThumbIn};

/// `ComponentEvent_ButtonPressed` (WindowEventType).
const EE_WON_BAR_BUTTON_PRESSED: isize = 15;

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonBarCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    AddedToParent,
    /// `HandleComponentEvent(a)`.
    HandleEvent,
    /// `SetEnabled(a != 0)`.
    SetEnabled,
    /// `CopyAttributes(a)` (`a` the other bar's Rust object).
    CopyAttributes,
    AddControls,
    /// `LayoutThumb(a != 0)`.
    LayoutThumb,
    /// the part `a` (0 the up arrow, 1 the down arrow, 2 the track, 3 the thumb).
    GetPart,
    /// the part `a` = `b` (the original's `SmartPtr` assignment).
    SetPart,
    /// `IsVertical()` (0 or 1).
    IsVertical,
    /// `SetVertical(a != 0)`.
    SetVertical,
    /// `SetResizeThumb(a != 0)`.
    SetResizeThumb,
    /// `SetMinThumbSize(a)` (an unsigned char).
    SetMinThumbSize,
    GetPosition,
    /// `SetPosition(a)` (the field only).
    SetPosition,
    /// `SetRange(a, b)` (total, page; the page step is the page).
    SetRange,
    /// `SetLineScrollSizes(a, b)` (up, down).
    SetLineScrollSizes,
}

#[derive(Debug)]
struct State {
    /// up arrow, down arrow, track, thumb
    parts: [isize; 4],
    resize_thumb: bool,
    vertical: bool,
    min_thumb: u8,
    position: i32,
    page: i32,
    total: i32,
    line_up: i32,
    line_down: i32,
    page_step: i32,
    drag_x: i32,
    drag_y: i32,
}

const UP: usize = 0;
const DOWN: usize = 1;
const RECT: usize = 2;
const THUMB: usize = 3;

/// The Rust Scrollbar.
pub struct EeWonScrollbar {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonScrollbar {
    /// The constructor's state: no parts, position, page and total 0, line steps 10, page step 50, a resizing
    /// thumb of at least 10. The drag offsets the original left unset are 0.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, vertical: bool) -> Self {
        EeWonScrollbar {
            comp,
            host,
            st: RefCell::new(State {
                parts: [0; 4],
                resize_thumb: true,
                vertical,
                min_thumb: 10,
                position: 0,
                page: 0,
                total: 0,
                line_up: 10,
                line_down: 10,
                page_step: 50,
                drag_x: 0,
                drag_y: 0,
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
    fn part(&self, i: usize) -> isize {
        self.st.borrow().parts[i]
    }
    fn vert(&self) -> bool {
        self.st.borrow().vertical
    }
    fn left(&self, c: isize) -> i32 {
        self.op(Op::ChildLeft, c, 0, 0) as i32
    }
    fn top(&self, c: isize) -> i32 {
        self.op(Op::ChildTop, c, 0, 0) as i32
    }
    fn width(&self, c: isize) -> i32 {
        self.op(Op::ChildWidth, c, 0, 0) as i32
    }
    fn height(&self, c: isize) -> i32 {
        self.op(Op::ChildHeight, c, 0, 0) as i32
    }
    // the S_ helpers: along and across the bar (x and y swapped when horizontal)
    fn s_left(&self, c: isize) -> i32 {
        if self.vert() {
            self.left(c)
        } else {
            self.top(c)
        }
    }
    fn s_top(&self, c: isize) -> i32 {
        if self.vert() {
            self.top(c)
        } else {
            self.left(c)
        }
    }
    fn s_bottom(&self, c: isize) -> i32 {
        if self.vert() {
            self.top(c).wrapping_add(self.height(c))
        } else {
            self.left(c).wrapping_add(self.width(c))
        }
    }
    fn s_height(&self, c: isize) -> i32 {
        if self.vert() {
            self.height(c)
        } else {
            self.width(c)
        }
    }
    fn s_width(&self, c: isize) -> i32 {
        if self.vert() {
            self.width(c)
        } else {
            self.height(c)
        }
    }
    /// `S_Width(this)` and `S_Height(this)`.
    fn own_width(&self) -> i32 {
        let f = if self.vert() { F::Width } else { F::Height };
        self.op(Op::Get, f as isize, 0, 0) as i32
    }
    fn own_height(&self) -> i32 {
        let f = if self.vert() { F::Height } else { F::Width };
        self.op(Op::Get, f as isize, 0, 0) as i32
    }
    fn s_set_pos(&self, c: isize, x: i32, y: i32) {
        let (x, y) = if self.vert() { (x, y) } else { (y, x) };
        self.op(Op::ChildSetPos, c, x as isize, y as isize);
    }
    fn s_set_size(&self, c: isize, w: i32, h: i32) {
        let (w, h) = if self.vert() { (w, h) } else { (h, w) };
        self.op(Op::ChildSetSize, c, w as isize, h as isize);
    }
    fn s_set_pos_size(&self, c: isize, x: i32, y: i32, w: i32, h: i32) {
        let r = if self.vert() { EeWonRect4 { x, y, w, h } } else { EeWonRect4 { x: y, y: x, w: h, h: w } };
        self.op(Op::ChildSetPosSize, c, &r as *const EeWonRect4 as isize, 0);
    }
    fn disabled(&self) -> bool {
        self.op(Op::IsDisabled, 0, 0, 0) != 0
    }

    /// The `SmartPtr` assignment: the old part given back, then the new one held.
    pub fn set_part(&self, i: usize, p: isize) {
        let old = self.part(i);
        if old == p {
            return;
        }
        if old != 0 {
            self.op(Op::ChildRelease, old, 0, 0);
        }
        self.st.borrow_mut().parts[i] = p;
        if p != 0 {
            self.op(Op::ChildHold, p, 0, 0);
        }
    }

    pub fn copy_attributes(&self, from: &EeWonScrollbar) {
        let (up, down, resize, min) = {
            let f = from.st.borrow();
            (f.line_up, f.line_down, f.resize_thumb, f.min_thumb)
        };
        let mut s = self.st.borrow_mut();
        s.line_up = up;
        s.line_down = down;
        s.resize_thumb = resize;
        s.min_thumb = min;
    }

    pub fn added_to_parent(&self) {
        if self.op(Op::ChildIsMine, self.part(UP), 0, 0) == 0 {
            self.add_controls();
        }
        self.op(Op::BaseAddedToParent, 0, 0, 0);
    }

    pub fn add_controls(&self) {
        if self.part(RECT) == 0 {
            let b = self.op(Op::NewButton, 0, 0, 0);
            self.set_part(RECT, b);
        }
        for i in [UP, DOWN, RECT] {
            self.op(Op::ButtonSetFlags, self.part(i), 2, 1);
        }
        for i in [RECT, THUMB, UP, DOWN] {
            self.op(Op::AddChild, self.part(i), 0, 0);
        }
    }

    pub fn layout_thumb(&self, change_pos: bool) {
        let (rect, thumb) = (self.part(RECT), self.part(THUMB));
        let t = {
            let s = self.st.borrow();
            EeWonThumbIn {
                page: s.page,
                total: s.total,
                position: s.position,
                resize_thumb: s.resize_thumb,
                min_thumb: i32::from(s.min_thumb),
                change_pos,
                ..EeWonThumbIn::default()
            }
        };
        let t = EeWonThumbIn {
            track_top: self.s_top(rect),
            track_len: self.s_height(rect),
            thumb_top: self.s_top(thumb),
            thumb_len: self.s_height(thumb),
            ..t
        };
        let out = guiscroll::thumb_layout(t);
        if out.hidden {
            self.op(Op::ChildSetVisible, thumb, 0, 0);
            return;
        }
        self.op(Op::ChildSetVisible, thumb, (!self.disabled()) as isize, 0);
        let mut w = self.s_width(thumb);
        if w == 0 {
            w = self.own_width();
        }
        if out.set_pos {
            self.s_set_pos_size(thumb, self.s_left(thumb), out.pos, w, out.len);
        } else {
            self.s_set_size(thumb, w, out.len);
        }
    }

    pub fn size_changed(&self) {
        self.op(Op::BaseSizeChanged, 0, 0, 0);
        let (up, down, rect) = (self.part(UP), self.part(DOWN), self.part(RECT));
        if self.s_height(up) == 0 {
            self.s_set_size(up, self.own_width(), self.own_width());
        }
        if self.s_height(down) == 0 {
            self.s_set_size(down, self.own_width(), self.own_width());
        }
        self.s_set_pos(down, 0, self.own_height().wrapping_sub(self.s_height(down)));
        let len = self.own_height().wrapping_sub(self.s_height(up)).wrapping_sub(self.s_height(down));
        self.s_set_pos_size(rect, 0, self.s_bottom(up), self.own_width(), len);
        self.layout_thumb(false);
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.op(Op::BaseMouseDown, x as isize, y as isize, button as isize);
        let thumb = self.part(THUMB);
        if button == 0 && self.op(Op::MouseChild, 0, 0, 0) == thumb {
            let dx = x.wrapping_sub(self.left(thumb));
            let dy = y.wrapping_sub(self.top(thumb));
            let mut s = self.st.borrow_mut();
            s.drag_x = dx;
            s.drag_y = dy;
        }
    }

    pub fn mouse_drag(&self, x: i32, y: i32) {
        self.op(Op::BaseMouseDrag, x as isize, y as isize, 0);
        let (rect, thumb) = (self.part(RECT), self.part(THUMB));
        if self.op(Op::MouseChild, 0, 0, 0) != thumb {
            return;
        }
        let (drag_x, drag_y, total, page, old) = {
            let s = self.st.borrow();
            (s.drag_x, s.drag_y, s.total, s.page, s.position)
        };
        let dx = x.wrapping_sub(self.left(thumb)).wrapping_sub(drag_x);
        let dy = y.wrapping_sub(self.top(thumb)).wrapping_sub(drag_y);
        let new_y = if self.vert() { self.top(thumb).wrapping_add(dy) } else { self.left(thumb).wrapping_add(dx) };
        let (top, position) =
            guiscroll::thumb_drag(new_y, self.s_top(rect), self.s_height(rect), self.s_height(thumb), total, page);
        self.st.borrow_mut().position = position;
        if top != self.s_top(thumb) {
            self.s_set_pos(thumb, self.s_left(thumb), top);
            self.op(Op::ChildInvalidate, rect, 0, 0);
        }
        if position != old {
            self.op(Op::FireEvent, EeWonGuiEvent::Scroll as isize, 0, 0);
        }
    }

    pub fn handle_event(&self, event: isize) {
        if self.op(Op::EventType, event, 0, 0) != EE_WON_BAR_BUTTON_PRESSED {
            self.op(Op::BaseHandleEvent, event, 0, 0);
            return;
        }
        let comp = self.op(Op::EventComponent, event, 0, 0);
        let mut mouse = 0i32;
        let press = if comp == self.part(UP) {
            EeWonScrollPress::LineUp
        } else if comp == self.part(DOWN) {
            EeWonScrollPress::LineDown
        } else if comp == self.part(RECT) {
            let (mut mx, mut my) = (0i32, 0i32);
            self.op(Op::GetMousePos, &mut mx as *mut i32 as isize, &mut my as *mut i32 as isize, 0);
            mouse = if self.vert() { my } else { mx };
            EeWonScrollPress::Track
        } else {
            self.op(Op::BaseHandleEvent, event, 0, 0);
            return;
        };
        let thumb = self.part(THUMB);
        let (thumb_top, thumb_len) = (self.s_top(thumb), self.s_height(thumb));
        let changed = {
            let mut s = self.st.borrow_mut();
            let old = s.position;
            s.position = guiscroll::scroll_step(
                press,
                mouse,
                thumb_top,
                thumb_len,
                s.position,
                s.line_up,
                s.line_down,
                s.page_step,
                s.total,
                s.page,
            );
            s.position != old
        };
        if !changed {
            return;
        }
        self.layout_thumb(true);
        self.op(Op::ChildInvalidate, self.part(RECT), 0, 0);
        self.op(Op::FireEvent, EeWonGuiEvent::Scroll as isize, 0, 0);
    }

    pub fn set_enabled(&self, on: bool) {
        if on == !self.disabled() {
            return;
        }
        self.op(Op::BaseSetEnabled, on as isize, 0, 0);
        self.op(Op::ChildSetEnabled, self.part(UP), on as isize, 0);
        self.op(Op::ChildSetEnabled, self.part(DOWN), on as isize, 0);
        let overflow = {
            let s = self.st.borrow();
            s.total > s.page
        };
        if overflow {
            self.op(Op::ChildSetVisible, self.part(THUMB), on as isize, 0);
        }
    }

    pub fn is_vertical(&self) -> bool {
        self.vert()
    }
    pub fn position(&self) -> i32 {
        self.st.borrow().position
    }
    pub fn set_position(&self, p: i32) {
        self.st.borrow_mut().position = p;
    }
    pub fn set_range(&self, total: i32, page: i32) {
        let mut s = self.st.borrow_mut();
        s.total = total;
        s.page = page;
        s.page_step = page;
    }
    pub fn set_line_scroll_sizes(&self, up: i32, down: i32) {
        let mut s = self.st.borrow_mut();
        s.line_up = up;
        s.line_down = down;
    }

    pub fn call(&self, call: EeWonBarCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonBarCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::AddedToParent => self.added_to_parent(),
            C::HandleEvent => self.handle_event(a),
            C::SetEnabled => self.set_enabled(a != 0),
            C::CopyAttributes => {
                if a != 0 {
                    // SAFETY: the C++ passes the other bar's live Rust object (a shared reference)
                    self.copy_attributes(unsafe { &*(a as *const EeWonScrollbar) });
                }
            }
            C::AddControls => self.add_controls(),
            C::LayoutThumb => self.layout_thumb(a != 0),
            C::GetPart => return self.st.borrow().parts.get(a as usize).copied().unwrap_or(0),
            C::SetPart => {
                if (0..4).contains(&a) {
                    self.set_part(a as usize, b);
                }
            }
            C::IsVertical => return self.vert() as isize,
            C::SetVertical => self.st.borrow_mut().vertical = a != 0,
            C::SetResizeThumb => self.st.borrow_mut().resize_thumb = a != 0,
            C::SetMinThumbSize => self.st.borrow_mut().min_thumb = a as u8,
            C::GetPosition => return self.position() as isize,
            C::SetPosition => self.set_position(ai),
            C::SetRange => self.set_range(ai, bi),
            C::SetLineScrollSizes => self.set_line_scroll_sizes(ai, bi),
        }
        0
    }
}

impl Drop for EeWonScrollbar {
    /// The original's members go in reverse order: the thumb, the track, the down arrow, the up arrow.
    fn drop(&mut self) {
        let parts = std::mem::take(&mut self.st.borrow_mut().parts);
        for i in [THUMB, RECT, DOWN, UP] {
            if parts[i] != 0 {
                self.op(Op::ChildRelease, parts[i], 0, 0);
            }
        }
    }
}

/// A new Rust Scrollbar for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_bar_new(component: *mut c_void, host: EeWonGuiHostFn, vertical: bool) -> *mut EeWonScrollbar {
    Box::into_raw(Box::new(EeWonScrollbar::new(component, host, vertical)))
}

/// # Safety
/// `bar` came from `ee_won_bar_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_bar_free(bar: *mut EeWonScrollbar) {
    if !bar.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(bar) });
    }
}

/// One of Scrollbar's methods (`EeWonBarCall`).
///
/// # Safety
/// `bar` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_bar_call(
    bar: *const EeWonScrollbar,
    call: EeWonBarCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*bar).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "barobj_tests.rs"]
mod tests;
