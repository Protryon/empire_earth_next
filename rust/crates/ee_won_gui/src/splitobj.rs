//! WONGUI's Splitter as a whole Rust object (R6 WON round 22): the game room's chat and player list side by side
//! (the LAN and Internet staging screens) and the Internet lobby's main tab. The original's Splitter.cpp with its
//! state: the quadrants (panes), the splitter bars with their drag handles, the maximise buttons, the split and its
//! bars' edges, the percentages, the minimum panes, the drag and the maximised quadrant.
//!
//! The C++ `Splitter` stays the type the configs build (a `Container` in the tree, its vtable); its virtuals and
//! methods are one-line forwarders to `ee_won_split_call`, and every call out is an op of the component op table
//! (`guihost`): the base container's virtuals, its children (borrowed `Component*`s; the drag handles and the
//! maximise buttons are held with a reference, as the original's `SmartPtr`s) and the events. `guictl` keeps the
//! split's decisions, as before.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guictl::{self, EeWonRect4, EeWonSplit};
use crate::guihost::{EeWonGuiCursor as Cursor, EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};

/// `Splitter_LeftRight` and `Splitter_UpDown` (the bits a bar's type carries).
const LEFT_RIGHT: i32 = 5;
const UP_DOWN: i32 = 10;
/// `ComponentFlag_ParentDrag`.
const PARENT_DRAG: isize = 0x20;

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonSplitCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    /// `HandleComponentEvent(a)`.
    HandleEvent,
    /// `CheckMaxButton(a)` (0 or 1).
    CheckMaxButton,
    /// `RegisterMaxButton(a, b)`.
    RegisterMaxButton,
    /// `AddQuadrant(a, b)`.
    AddQuadrant,
    /// `AddSplitter(a, b)`.
    AddSplitter,
    /// `AddComplexSplitter(a, b, c)` (`c` the drag handle or NULL).
    AddComplexSplitter,
    /// `AddInvisibleSplitter(a, b)`.
    AddInvisibleSplitter,
    /// `MaxQuadrant(a)`.
    MaxQuadrant,
    UnMaxQuadrant,
    /// `SetMinLeftRight(a, b)`.
    SetMinLeftRight,
    /// `SetMinTopBottom(a, b)`.
    SetMinTopBottom,
    /// `SetPercentages(a, b)`.
    SetPercentages,
    /// `SetSplitterPos(a, b, c != 0)`.
    SetSplitterPos,
}

#[derive(Clone, Copy, Debug)]
struct Bar {
    comp: isize,
    kind: i32,
    drag: isize,
}

#[derive(Debug)]
struct State {
    quadrants: Vec<(isize, i32)>,
    bars: Vec<Bar>,
    buttons: Vec<isize>,
    lr_drag: bool,
    ud_drag: bool,
    drag_x: i32,
    drag_y: i32,
    /// left, right, top, bottom
    mins: [i32; 4],
    ud_max: bool,
    lr_max: bool,
    left_pct: f64,
    top_pct: f64,
    valid: bool,
    split: EeWonSplit,
    bar_w: i32,
    bar_h: i32,
    have_max: bool,
    cur_max: i32,
}

/// The Rust Splitter.
pub struct EeWonSplitter {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonSplitter {
    /// The constructor's state (the C++ constructor sets the clip flag).
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonSplitter {
            comp,
            host,
            st: RefCell::new(State {
                quadrants: Vec::new(),
                bars: Vec::new(),
                buttons: Vec::new(),
                lr_drag: false,
                ud_drag: false,
                drag_x: 0,
                drag_y: 0,
                mins: [0; 4],
                ud_max: false,
                lr_max: false,
                left_pct: 50.0,
                top_pct: 50.0,
                valid: false,
                split: EeWonSplit::default(),
                bar_w: 0,
                bar_h: 0,
                have_max: false,
                cur_max: 0,
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
    fn pos_size(&self, c: isize, r: EeWonRect4) {
        self.op(Op::ChildSetPosSize, c, &r as *const EeWonRect4 as isize, 0);
    }

    pub fn register_max_button(&self, button: isize, pos: i32) {
        self.op(Op::ButtonSetControlId, button, pos.wrapping_add(100005) as isize, 0);
        self.op(Op::ChildHold, button, 0, 0);
        self.st.borrow_mut().buttons.push(button);
    }

    pub fn add_quadrant(&self, c: isize, pos: i32) {
        self.st.borrow_mut().quadrants.push((c, pos));
        self.op(Op::AddChild, c, 0, 0);
    }

    fn add_splitter_prv(&self, c: isize, kind: i32, drag: isize) {
        let at = {
            let mut s = self.st.borrow_mut();
            s.bars.push(Bar { comp: c, kind, drag: 0 });
            s.bars.len() - 1
        };
        self.op(Op::ChildSetFlags, drag, PARENT_DRAG, 1);
        if self.op(Op::ChildHasDefaultCursor, drag, 0, 0) == 0 {
            let cursor = match guictl::split_cursor(kind) {
                1 => Some(Cursor::NorthSouthEastWest),
                2 => Some(Cursor::EastWest),
                3 => Some(Cursor::NorthSouth),
                _ => None,
            };
            if let Some(k) = cursor {
                self.op(Op::ChildSetDefaultCursor, drag, k as isize, 0);
            }
        }
        self.op(Op::ChildHold, drag, 0, 0);
        if let Some(b) = self.st.borrow_mut().bars.get_mut(at) {
            b.drag = drag;
        }
        self.op(Op::AddChild, c, 0, 0);
        if self.st.borrow().bar_w == 0 && kind & LEFT_RIGHT != 0 {
            let w = self.op(Op::ChildWidth, c, 0, 0) as i32;
            self.st.borrow_mut().bar_w = w;
        }
        if self.st.borrow().bar_h == 0 && kind & UP_DOWN != 0 {
            let h = self.op(Op::ChildHeight, c, 0, 0) as i32;
            self.st.borrow_mut().bar_h = h;
        }
    }

    pub fn add_splitter(&self, c: isize, kind: i32) {
        self.add_splitter_prv(c, kind, c);
    }

    pub fn add_complex_splitter(&self, container: isize, kind: i32, drag: isize) {
        let drag = if drag == 0 { self.op(Op::NewDragComponent, container, 0, 0) } else { drag };
        self.add_splitter_prv(container, kind, drag);
    }

    pub fn add_invisible_splitter(&self, kind: i32, size: i32) {
        let c = self.op(Op::NewSizedComponent, size as isize, 0, 0);
        self.add_splitter(c, kind);
        self.op(Op::ChildRelease, c, 0, 0);
    }

    pub fn max_quadrant(&self, pos: i32) {
        let (w, h) = (self.width(), self.height());
        let (x, y, buttons, bars, quadrants) = {
            let mut s = self.st.borrow_mut();
            s.have_max = true;
            s.cur_max = pos;
            let (x, y, lr, ud) = guictl::split_max(pos, w, h, s.bar_w, s.bar_h, s.split.x, s.split.y);
            s.lr_max = lr;
            s.ud_max = ud;
            (x, y, s.buttons.clone(), s.bars.clone(), s.quadrants.clone())
        };
        for b in buttons {
            let id = self.op(Op::ButtonControlId, b, 0, 0) as i32;
            let q = guictl::split_button_quadrant(id);
            self.op(Op::ButtonSetCheck, b, (q == pos) as isize, 0);
        }
        for bar in bars {
            self.op(Op::ChildSetEnabled, bar.drag, (bar.kind & pos == 0) as isize, 0);
        }
        for (c, p) in quadrants {
            self.op(Op::ChildSetVisible, c, (p & pos != 0) as isize, 0);
        }
        self.set_splitter_pos(x, y, false);
    }

    pub fn un_max_quadrant(&self) {
        let (bars, quadrants) = {
            let mut s = self.st.borrow_mut();
            s.have_max = false;
            s.ud_max = false;
            s.lr_max = false;
            (s.bars.clone(), s.quadrants.clone())
        };
        for bar in bars {
            self.op(Op::ChildSetEnabled, bar.drag, 1, 0);
        }
        for (c, _) in quadrants {
            self.op(Op::ChildSetVisible, c, 1, 0);
        }
        let (lp, tp) = {
            let s = self.st.borrow();
            (s.left_pct, s.top_pct)
        };
        let x = guictl::split_at(self.width(), lp, 0);
        let y = guictl::split_at(self.height(), tp, 0);
        self.set_splitter_pos(x, y, false);
    }

    fn calc_quadrants(&self) {
        let quadrants = self.st.borrow().quadrants.clone();
        for (c, pos) in quadrants {
            let split = self.st.borrow().split;
            let r = guictl::split_pane(pos, self.width(), self.height(), &split);
            self.pos_size(c, r);
        }
    }

    fn calc_splitter_positions(&self) {
        let bars = self.st.borrow().bars.clone();
        for bar in bars {
            if self.op(Op::ChildIsVisible, bar.comp, 0, 0) != 0 {
                let split = self.st.borrow().split;
                let r = guictl::split_bar(bar.kind, self.width(), self.height(), &split);
                self.pos_size(bar.comp, r);
            }
        }
    }

    pub fn set_splitter_pos(&self, x: i32, y: i32, remember: bool) {
        let (w, h) = (self.width(), self.height());
        {
            let mut s = self.st.borrow_mut();
            let Some(n) = guictl::split_set(
                x, y, s.lr_max, s.ud_max, s.mins, w, h, s.valid, s.split.x, s.split.y, s.bar_w, s.bar_h,
            ) else {
                return;
            };
            s.valid = true;
            s.split = n;
            if remember {
                if let Some(p) = guictl::split_percent(n.x, w) {
                    s.left_pct = p;
                }
                if let Some(p) = guictl::split_percent(n.y, h) {
                    s.top_pct = p;
                }
            }
        }
        self.calc_splitter_positions();
        self.calc_quadrants();
        self.op(Op::Invalidate, 0, 0, 0);
    }

    pub fn size_changed(&self) {
        self.op(Op::BaseSizeChanged, 0, 0, 0);
        let (w, h) = (self.width(), self.height());
        let (x, y) = {
            let mut s = self.st.borrow_mut();
            s.valid = false;
            guictl::split_layout(s.have_max, s.cur_max, s.left_pct, s.top_pct, s.bar_w, s.bar_h, w, h)
        };
        self.set_splitter_pos(x, y, false);
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.op(Op::BaseMouseDown, x as isize, y as isize, button as isize);
        let mc = self.op(Op::MouseChild, 0, 0, 0);
        if button != 0 || mc == 0 {
            return;
        }
        let dx = x.wrapping_sub(self.op(Op::ChildLeft, mc, 0, 0) as i32);
        let dy = y.wrapping_sub(self.op(Op::ChildTop, mc, 0, 0) as i32);
        {
            let mut s = self.st.borrow_mut();
            s.lr_drag = false;
            s.ud_drag = false;
            s.drag_x = dx;
            s.drag_y = dy;
        }
        let grand = self.op(Op::ChildMouseChild, mc, 0, 0);
        let handle = if grand == 0 { mc } else { grand };
        if self.op(Op::ChildFlagSet, handle, PARENT_DRAG, 0) == 0 {
            return;
        }
        let mut s = self.st.borrow_mut();
        if let Some(kind) = s.bars.iter().find(|b| b.comp == mc).map(|b| b.kind) {
            if kind & UP_DOWN != 0 {
                s.ud_drag = true;
            }
            if kind & LEFT_RIGHT != 0 {
                s.lr_drag = true;
            }
        }
    }

    pub fn mouse_up(&self, x: i32, y: i32, button: i32) {
        self.op(Op::BaseMouseUp, x as isize, y as isize, button as isize);
        if button != 0 || self.op(Op::MouseChild, 0, 0, 0) == 0 {
            return;
        }
        let mut s = self.st.borrow_mut();
        s.lr_drag = false;
        s.ud_drag = false;
    }

    pub fn mouse_drag(&self, x: i32, y: i32) {
        self.op(Op::BaseMouseDrag, x as isize, y as isize, 0);
        let mc = self.op(Op::MouseChild, 0, 0, 0);
        if mc == 0 {
            return;
        }
        let cx = self.op(Op::ChildLeft, mc, 0, 0) as i32;
        let cy = self.op(Op::ChildTop, mc, 0, 0) as i32;
        let (nx, ny) = {
            let s = self.st.borrow();
            guictl::split_drag(
                x,
                y,
                cx,
                cy,
                s.drag_x,
                s.drag_y,
                s.lr_drag && !s.lr_max,
                s.ud_drag && !s.ud_max,
                s.split.x,
                s.split.y,
            )
        };
        self.set_splitter_pos(nx, ny, true);
    }

    pub fn check_max_button(&self, event: isize) -> bool {
        if self.op(Op::EventIsButtonPress, event, 0, 0) == 0 {
            return false;
        }
        if !guictl::split_is_max_button(self.op(Op::EventControlId, event, 0, 0) as i32) {
            return false;
        }
        let button = self.op(Op::EventButton, event, 0, 0);
        if button == 0 {
            return false;
        }
        if self.op(Op::ButtonIsChecked, button, 0, 0) == 0 {
            self.un_max_quadrant();
            return true;
        }
        let q = guictl::split_button_quadrant(self.op(Op::EventControlId, event, 0, 0) as i32);
        self.max_quadrant(q);
        true
    }

    pub fn handle_event(&self, event: isize) {
        if !self.check_max_button(event) {
            self.op(Op::BaseHandleEvent, event, 0, 0);
        }
    }

    /// One inward call.
    pub fn call(&self, call: EeWonSplitCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonSplitCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseUp => self.mouse_up(ai, bi, ci),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::HandleEvent => self.handle_event(a),
            C::CheckMaxButton => return self.check_max_button(a) as isize,
            C::RegisterMaxButton => self.register_max_button(a, bi),
            C::AddQuadrant => self.add_quadrant(a, bi),
            C::AddSplitter => self.add_splitter(a, bi),
            C::AddComplexSplitter => self.add_complex_splitter(a, bi, c),
            C::AddInvisibleSplitter => self.add_invisible_splitter(ai, bi),
            C::MaxQuadrant => self.max_quadrant(ai),
            C::UnMaxQuadrant => self.un_max_quadrant(),
            C::SetMinLeftRight => {
                let mut s = self.st.borrow_mut();
                s.mins[0] = ai;
                s.mins[1] = bi;
            }
            C::SetMinTopBottom => {
                let mut s = self.st.borrow_mut();
                s.mins[2] = ai;
                s.mins[3] = bi;
            }
            C::SetPercentages => {
                let mut s = self.st.borrow_mut();
                s.left_pct = f64::from(ai);
                s.top_pct = f64::from(bi);
            }
            C::SetSplitterPos => self.set_splitter_pos(ai, bi, c != 0),
        }
        0
    }

    /// The held references go back (the C++ destructor's lists: the buttons, then the bars' drag handles).
    fn release_all(&self) {
        let (buttons, bars) = {
            let mut s = self.st.borrow_mut();
            (std::mem::take(&mut s.buttons), std::mem::take(&mut s.bars))
        };
        for b in buttons {
            self.op(Op::ChildRelease, b, 0, 0);
        }
        for bar in bars {
            if bar.drag != 0 {
                self.op(Op::ChildRelease, bar.drag, 0, 0);
            }
        }
    }
}

impl Drop for EeWonSplitter {
    fn drop(&mut self) {
        self.release_all();
    }
}

// ---- the C ABI

/// A new Rust Splitter for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_split_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonSplitter {
    Box::into_raw(Box::new(EeWonSplitter::new(component, host)))
}

/// # Safety
/// `split` came from `ee_won_split_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_split_free(split: *mut EeWonSplitter) {
    if !split.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(split) });
    }
}

/// One of Splitter's methods (`EeWonSplitCall`).
///
/// # Safety
/// `split` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_split_call(
    split: *const EeWonSplitter,
    call: EeWonSplitCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*split).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "splitobj_tests.rs"]
mod tests;
