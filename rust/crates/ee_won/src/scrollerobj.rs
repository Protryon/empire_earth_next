//! WONGUI's Scroller and ScrollbarScroller as one whole Rust object (R6 WON round 25): the container around a scroll
//! area (every lobby list and text area sits in one: MSScrollContainer and WONScrollContainer derive from
//! ScrollbarScroller). The original's Scroller.cpp and ScrollbarScroller.cpp with their state: the scroll area and
//! the two scroll bars (held by reference, as the original's `SmartPtr`s), the bars' conditions and the flag that
//! marks their application.
//!
//! The C++ `Scroller` and `ScrollbarScroller` stay the types the configs, the combo box and the scroll areas hold
//! (their vtables: the subclasses' `Virtual08` and `Paint`); their virtuals and methods are one-line forwarders to
//! `ee_won_scroller_call` on the one Rust object the `Scroller` constructor makes (a plain Scroller never uses the
//! bars). A bar's own methods are called on its Rust object (`barobj`, through the op `BarOf`), its virtuals
//! (`SetEnabled`) through the C++. `guiscroll` keeps the conditions' and the bars' numbers, as before.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::barobj::EeWonScrollbar;
use crate::guihost::{EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guiscroll::{self, EeWonScrollArea};

/// `ComponentEvent_Scroll` (WindowEventType).
const EE_WON_SCROLLER_SCROLL: isize = 16;

/// The inward calls (the C++ shells' virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonScrollerCall {
    /// `MouseDown(a, b, c)`.
    MouseDown = 1,
    /// `MouseWheel(a)` (0 or 1).
    MouseWheel,
    GetScrollArea,
    /// `SetScrollArea(a)` (the original's `SmartPtr` assignment).
    SetScrollArea,
    /// `SetOffsets(a, b)` (horizontal, vertical).
    SetOffsets,
    /// `SetVertOffset(a)`.
    SetVertOffset,
    /// `SetHorzOffset(a)`.
    SetHorzOffset,
    // ---- ScrollbarScroller
    AddedToParent,
    /// `HandleComponentEvent(a)`.
    HandleEvent,
    /// `NotifyScrollChange(a != 0)`.
    NotifyScrollChange,
    /// `SetScrollbar(a)`.
    SetScrollbar,
    AddControls,
    /// `GetScrollbar(a != 0)`.
    GetScrollbar,
    /// `SetBar(a != 0, b)` (the field only).
    SetBar,
    /// `EnableScrollbar(a, b)` (0 or 1).
    EnableScrollbar,
    /// `SetScrollbarConditions(a, b)` (horizontal, vertical).
    SetScrollbarConditions,
}

#[derive(Clone, Copy)]
enum Slot {
    Area,
    Horz,
    Vert,
}

#[derive(Debug)]
struct State {
    area: isize,
    horz: isize,
    vert: isize,
    vert_cond: i32,
    horz_cond: i32,
    setting: bool,
}

/// The Rust Scroller (and ScrollbarScroller).
pub struct EeWonScroller {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonScroller {
    /// The constructors' state: no scroll area, no bars, both conditions Enable.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonScroller {
            comp,
            host,
            st: RefCell::new(State { area: 0, horz: 0, vert: 0, vert_cond: 1, horz_cond: 1, setting: false }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, 0) },
            None => 0,
        }
    }
    fn area(&self) -> isize {
        self.st.borrow().area
    }
    fn area_get(&self, f: F) -> i32 {
        self.op(Op::AreaGet, self.area(), f as isize, 0) as i32
    }
    fn area_numbers(&self) -> EeWonScrollArea {
        EeWonScrollArea {
            vert_offset: self.area_get(F::VertOffset),
            horz_offset: self.area_get(F::HorzOffset),
            total_width: self.area_get(F::TotalWidth),
            total_height: self.area_get(F::TotalHeight),
            extra_height: self.area_get(F::ExtraHeight),
            view_x: self.area_get(F::ScrollX),
            view_y: self.area_get(F::ScrollY),
            view_width: self.area_get(F::ScrollWidth),
            view_height: self.area_get(F::ScrollHeight),
        }
    }
    /// A bar's Rust object (NULL for no bar).
    fn bar(&self, c: isize) -> Option<&EeWonScrollbar> {
        if c == 0 {
            return None;
        }
        let p = self.op(Op::BarOf, c, 0, 0) as *const EeWonScrollbar;
        // SAFETY: the C++ bar's live Rust object (a shared reference: its calls out re-enter through the C ABI)
        unsafe { p.as_ref() }
    }
    /// The `SmartPtr` assignment: the old object given back, then the new one held.
    fn assign(&self, slot: Slot, p: isize) {
        let get = |s: &mut State| -> isize {
            match slot {
                Slot::Area => s.area,
                Slot::Horz => s.horz,
                Slot::Vert => s.vert,
            }
        };
        let old = get(&mut self.st.borrow_mut());
        if old == p {
            return;
        }
        if old != 0 {
            self.op(Op::ChildRelease, old, 0, 0);
        }
        {
            let mut s = self.st.borrow_mut();
            match slot {
                Slot::Area => s.area = p,
                Slot::Horz => s.horz = p,
                Slot::Vert => s.vert = p,
            }
        }
        if p != 0 {
            self.op(Op::ChildHold, p, 0, 0);
        }
    }

    pub fn set_scroll_area(&self, a: isize) {
        self.assign(Slot::Area, a);
    }

    pub fn set_offsets(&self, horizontal: i32, vertical: i32) {
        let area = self.area();
        if self.area_get(F::HorzOffset) == horizontal && self.area_get(F::VertOffset) == vertical {
            return;
        }
        self.op(Op::AreaSet, area, F::HorzOffset as isize, horizontal as isize);
        self.op(Op::AreaSet, area, F::VertOffset as isize, vertical as isize);
        self.op(Op::AreaAdjustPosition, area, 1, 0);
    }
    pub fn set_vert_offset(&self, vertical: i32) {
        self.set_offsets(self.area_get(F::HorzOffset), vertical);
    }
    pub fn set_horz_offset(&self, horizontal: i32) {
        self.set_offsets(horizontal, self.area_get(F::VertOffset));
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.op(Op::BaseMouseDown, x as isize, y as isize, button as isize);
        if button == 0 {
            if self.op(Op::HasFocusChild, 0, 0, 0) == 0 {
                self.op(Op::AreaRequestFocus, self.area(), 0, 0);
            } else {
                self.op(Op::RequestFocus, 0, 0, 0);
            }
        }
    }

    pub fn mouse_wheel(&self, amount: i32) -> bool {
        if self.op(Op::BaseMouseWheel, amount as isize, 0, 0) != 0 {
            return true;
        }
        let area = self.area();
        if area != 0 {
            return self.op(Op::AreaMouseWheel, area, amount as isize, 0) != 0;
        }
        false
    }

    pub fn set_scrollbar(&self, s: isize) {
        let vertical = self.bar(s).is_some_and(|b| b.is_vertical());
        if vertical {
            self.assign(Slot::Vert, s);
        } else {
            self.assign(Slot::Horz, s);
        }
        self.op(Op::ListenTo, s, 0, 0);
    }

    pub fn added_to_parent(&self) {
        let vert = self.st.borrow().vert;
        if self.op(Op::ChildIsMine, vert, 0, 0) == 0 {
            self.add_controls();
        }
        self.op(Op::BaseAddedToParent, 0, 0, 0);
    }

    pub fn add_controls(&self) {
        let area = self.area();
        self.op(Op::AreaSetScroller, area, 0, 0);
        self.op(Op::AddChild, area, 0, 0);
        let vert = self.st.borrow().vert;
        self.op(Op::AddChild, vert, 0, 0);
        let horz = self.st.borrow().horz;
        self.op(Op::AddChild, horz, 0, 0);
        if self.area_get(F::TotalWidth) > 0 || self.area_get(F::TotalHeight) > 0 {
            self.op(Op::VirtNotifyScrollChange, 0, 0, 0);
        }
    }

    pub fn enable_scrollbar(&self, s: isize, condition: i32) -> bool {
        if s == 0 || self.area() == 0 {
            return false;
        }
        let visible = self.op(Op::ChildIsVisible, s, 0, 0) != 0;
        let a = self.area_numbers();
        let vertical = self.bar(s).is_some_and(|b| b.is_vertical());
        let setting = self.st.borrow().setting;
        match guiscroll::scrollbar_condition(condition, setting, visible, vertical, &a) {
            None => return false,
            Some(Some(v)) => {
                self.op(Op::ChildSetVisible, s, v as isize, 0);
            }
            Some(None) => {}
        }
        visible != (self.op(Op::ChildIsVisible, s, 0, 0) != 0)
    }

    pub fn set_scrollbar_conditions(&self, horizontal: i32, vertical: i32) {
        let (horz, vert) = {
            let mut s = self.st.borrow_mut();
            s.horz_cond = horizontal;
            s.vert_cond = vertical;
            (s.horz, s.vert)
        };
        let mut changed = self.enable_scrollbar(horz, horizontal);
        changed = self.enable_scrollbar(vert, vertical) || changed;
        self.st.borrow_mut().setting = true;
        if changed {
            self.op(Op::Invalidate, 0, 0, 0);
            self.op(Op::VirtSizeChanged, 0, 0, 0);
        }
        self.st.borrow_mut().setting = false;
    }

    pub fn handle_event(&self, event: isize) {
        if self.op(Op::EventType, event, 0, 0) == EE_WON_SCROLLER_SCROLL {
            let s = self.op(Op::EventComponent, event, 0, 0);
            if let Some(b) = self.bar(s) {
                let position = b.position();
                let (horz, vert) = {
                    let st = self.st.borrow();
                    (st.horz, st.vert)
                };
                if b.is_vertical() {
                    if s == vert {
                        self.set_vert_offset(position);
                        return;
                    }
                } else if s == horz {
                    self.set_horz_offset(position);
                    return;
                }
            }
        }
        self.op(Op::ComponentHandleEvent, event, 0, 0);
    }

    pub fn notify_scroll_change(&self, because_of_scroller: bool) {
        let a = self.area_numbers();
        let vert = self.st.borrow().vert;
        if let Some(b) = self.bar(vert) {
            let (total, page, enabled) = guiscroll::scrollbar_sync(&a, true);
            b.set_range(total, page);
            b.set_line_scroll_sizes(self.area_get(F::UpScrollAmount), self.area_get(F::DownScrollAmount));
            if !because_of_scroller {
                b.set_position(self.area_get(F::VertOffset));
            }
            b.layout_thumb(!because_of_scroller);
            self.op(Op::ChildSetEnabled, vert, enabled as isize, 0);
            self.op(Op::ChildInvalidate, vert, 0, 0);
        }
        let horz = self.st.borrow().horz;
        if let Some(b) = self.bar(horz) {
            let (total, page, enabled) = guiscroll::scrollbar_sync(&a, false);
            b.set_range(total, page);
            if !because_of_scroller {
                b.set_position(self.area_get(F::HorzOffset));
            }
            b.layout_thumb(!because_of_scroller);
            self.op(Op::ChildSetEnabled, horz, enabled as isize, 0);
            self.op(Op::ChildInvalidate, horz, 0, 0);
        }
        let (h, v) = {
            let s = self.st.borrow();
            (s.horz_cond, s.vert_cond)
        };
        self.set_scrollbar_conditions(h, v);
    }

    pub fn call(&self, call: EeWonScrollerCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonScrollerCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseWheel => return self.mouse_wheel(ai) as isize,
            C::GetScrollArea => return self.area(),
            C::SetScrollArea => self.set_scroll_area(a),
            C::SetOffsets => self.set_offsets(ai, bi),
            C::SetVertOffset => self.set_vert_offset(ai),
            C::SetHorzOffset => self.set_horz_offset(ai),
            C::AddedToParent => self.added_to_parent(),
            C::HandleEvent => self.handle_event(a),
            C::NotifyScrollChange => self.notify_scroll_change(a != 0),
            C::SetScrollbar => self.set_scrollbar(a),
            C::AddControls => self.add_controls(),
            C::GetScrollbar => {
                let s = self.st.borrow();
                return if a != 0 { s.vert } else { s.horz };
            }
            C::SetBar => {
                if a != 0 {
                    self.assign(Slot::Vert, b);
                } else {
                    self.assign(Slot::Horz, b);
                }
            }
            C::EnableScrollbar => return self.enable_scrollbar(a, bi) as isize,
            C::SetScrollbarConditions => self.set_scrollbar_conditions(ai, bi),
        }
        0
    }
}

impl Drop for EeWonScroller {
    /// The original's members go in reverse order: ScrollbarScroller's vertical then horizontal bar, then
    /// Scroller's scroll area.
    fn drop(&mut self) {
        let (vert, horz, area) = {
            let mut s = self.st.borrow_mut();
            (std::mem::take(&mut s.vert), std::mem::take(&mut s.horz), std::mem::take(&mut s.area))
        };
        for p in [vert, horz, area] {
            if p != 0 {
                self.op(Op::ChildRelease, p, 0, 0);
            }
        }
    }
}

/// A new Rust Scroller for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_scroller_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonScroller {
    Box::into_raw(Box::new(EeWonScroller::new(component, host)))
}

/// # Safety
/// `scroller` came from `ee_won_scroller_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_scroller_free(scroller: *mut EeWonScroller) {
    if !scroller.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(scroller) });
    }
}

/// One of Scroller's or ScrollbarScroller's methods (`EeWonScrollerCall`).
///
/// # Safety
/// `scroller` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_scroller_call(
    scroller: *const EeWonScroller,
    call: EeWonScrollerCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*scroller).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "scrollerobj_tests.rs"]
mod tests;
