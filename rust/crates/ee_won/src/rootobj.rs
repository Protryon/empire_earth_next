//! WONGUI's RootContainer as a whole Rust object (R6 WON round 29): the top-level container of every lobby window.
//! The original's RootContainer.cpp: the window's invalid rectangle, the backgrounds the grabbers queue for drawing
//! under the children (keyed by their image order, each image held), the software cursor's component and the window
//! drag (its place, whether it runs, whether it may).
//!
//! The C++ `RootContainer` stays the type the window makes and the components reach through `GetRootContainer`; its
//! virtuals and `AddBG` are one-line forwarders to `ee_won_root_call`. Its window (`mWindow`) stays a C++ member: the
//! window's fields, rectangle and place are root ops. The Rust object holds the container's Rust object (made by the
//! base's constructor, freed after this one) and calls it for every `Container::X` the original made; its own
//! `RemoveChild`, called as a virtual by the original, goes through the C++ object (`CtVirt`). No class derives from
//! RootContainer, so its `InvalidateRect`, called on itself, is this object's.
//!
//! Dropped: the keyboard listener (`mKeyboardListener`: nothing in the game set it, so its three calls never ran), the
//! popup set and the modal component (never read or written after the constructor). The overrides that only called
//! the container's (FindChildAt, ChildInvalidated, MouseEnter) are gone: the container's own run.

use std::cell::RefCell;
use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::ctobj::{EeWonContainer, EeWonCtField};
use crate::guictl::EeWonRect4;
use crate::guidraw::{EeWonDrawOp, EE_WON_DRAW_POP_CLIP, EE_WON_DRAW_PUSH_CLIP};
use crate::guievent;
use crate::guihost::{EeWonCtVirt as V, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guilayout::{self, EeWonRect};

const FLAG_PARENT_DRAG: isize = 0x20;
/// `ComponentFlag_NoInput | ComponentFlag_AlwaysOnTop | ComponentFlag_NoParentCursor` (0x100 | 0x800 | 0x1000): the
/// cursor's component.
const FLAGS_CURSOR_COMPONENT: isize = 0x1900;

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRootCall {
    /// `Virtual08` (SizeChanged).
    SizeChanged = 1,
    /// `PrePaint(*a)` (`a` the Graphics).
    PrePaint,
    /// `Paint(*a)`.
    Paint,
    MouseExit,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    /// `MouseMove(a, b)`.
    MouseMove,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    /// `KeyChar(a)` (always 1).
    KeyChar,
    /// `KeyDown(a)` (always 1).
    KeyDown,
    /// `KeyUp(a)` (always 1).
    KeyUp,
    /// `InvalidateRect(*a)` (a `WONRectangle*`, not changed), also Window::InvalidateRoot's union.
    InvalidateRect,
    /// `SetCursor(a)` (a `Cursor*`).
    SetCursor,
    /// `RemoveChild(a)`.
    RemoveChild,
    /// `AddBG(a)` (a `BackgroundGrabber*`).
    AddBG,
    /// the field `a` (`EeWonRootField`).
    GetField,
}

/// The root's fields (for the goldens and the unit tests).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonRootField {
    InvalidX = 1,
    InvalidY,
    InvalidWidth,
    InvalidHeight,
    CursorComponent,
    Dragging,
    DragX,
    DragY,
    AllowDrag,
    /// the number of queued backgrounds
    BgCount,
}

/// A grabber's drawing state (`RootGrabberRead`): whether it needs drawing (`NeedDrawBG`), its image order (as the
/// map's `int` key), its image's place, its clip rectangle and its image.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonRootGrab {
    pub need: i32,
    pub order: i32,
    pub x: i32,
    pub y: i32,
    pub clip: EeWonRect4,
    pub image: isize,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
struct Bg {
    x: i32,
    y: i32,
    clip: EeWonRect,
    image: isize,
}

#[derive(Debug)]
struct State {
    invalid: EeWonRect,
    cursor_component: isize,
    drag_x: i32,
    drag_y: i32,
    dragging: bool,
    allow_drag: bool,
    bgs: BTreeMap<i32, Bg>,
}

/// The Rust RootContainer.
pub struct EeWonRoot {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
    st: RefCell<State>,
}

fn rect_of(r: EeWonRect4) -> EeWonRect {
    EeWonRect { x: r.x, y: r.y, width: r.w, height: r.h }
}

impl EeWonRoot {
    /// The constructor's state: no invalid rectangle, no cursor component, no drag (allowed), no background.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, ct: *const EeWonContainer) -> Self {
        EeWonRoot {
            comp,
            host,
            ct,
            st: RefCell::new(State {
                invalid: EeWonRect { x: 0, y: 0, width: 0, height: 0 },
                cursor_component: 0,
                drag_x: 0,
                drag_y: 0,
                dragging: false,
                allow_drag: true,
                bgs: BTreeMap::new(),
            }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn ct(&self) -> &EeWonContainer {
        // SAFETY: the base's Rust object outlives this one (made before it, freed after it)
        unsafe { &*self.ct }
    }
    fn me(&self) -> isize {
        self.comp as isize
    }
    fn cursor_component(&self) -> isize {
        self.st.borrow().cursor_component
    }
    /// `theCursor->GetComponent(mx, my)`: the component and its hot spot.
    fn cursor_of(&self, cursor: isize) -> (isize, i32, i32) {
        let (mut mx, mut my) = (0i32, 0i32);
        let c = self.op(Op::RootCursorComponent, cursor, &mut mx as *mut i32 as isize, &mut my as *mut i32 as isize, 0);
        (c, mx, my)
    }
    fn hold_image(&self, image: isize) {
        if image != 0 {
            self.op(Op::RootImageHold, image, 0, 0, 0);
        }
    }
    fn release_image(&self, image: isize) {
        if image != 0 {
            self.op(Op::RootImageRelease, image, 0, 0, 0);
        }
    }
    fn draw(&self, g: isize, kind: i32, args: [i32; 4]) {
        let o = EeWonDrawOp { kind, a: args[0], b: args[1], c: args[2], d: args[3], ..EeWonDrawOp::default() };
        self.op(Op::Draw, g, &o as *const EeWonDrawOp as isize, 0, 0);
    }

    pub fn add_bg(&self, grabber: isize) {
        if grabber == 0 {
            return;
        }
        let mut q = EeWonRootGrab::default();
        self.op(Op::RootGrabberRead, grabber, &mut q as *mut EeWonRootGrab as isize, 0, 0);
        if q.need == 0 {
            return;
        }
        // `mBGStructMap[order] = BGStruct(...)`: the temporary holds the image; the entry's SmartPtr assignment
        // releases its old image and holds the new one (nothing when they are the same); the temporary lets go
        let new = Bg { x: q.x, y: q.y, clip: rect_of(q.clip), image: q.image };
        self.hold_image(q.image);
        let old = self.st.borrow_mut().bgs.insert(q.order, new).map_or(0, |b| b.image);
        if old != q.image {
            self.release_image(old);
            self.hold_image(q.image);
        }
        self.release_image(q.image);
        self.op(Op::RootGrabberClear, grabber, 0, 0, 0);
    }

    pub fn size_changed(&self) {
        self.ct().size_changed();
        let me = self.me();
        let r = EeWonRect {
            x: self.op(Op::ChildLeft, me, 0, 0, 0) as i32,
            y: self.op(Op::ChildTop, me, 0, 0, 0) as i32,
            width: self.op(Op::ChildWidth, me, 0, 0, 0) as i32,
            height: self.op(Op::ChildHeight, me, 0, 0, 0) as i32,
        };
        self.st.borrow_mut().invalid = r;
    }

    pub fn pre_paint(&self, g: isize) {
        self.ct().pre_paint(g);
        // highest image order first
        let bgs: Vec<Bg> = self.st.borrow().bgs.values().rev().copied().collect();
        for b in bgs {
            let (mut w, mut h) = (0i32, 0i32);
            self.op(Op::RootImageSize, b.image, &mut w as *mut i32 as isize, &mut h as *mut i32 as isize, 0);
            let r = guilayout::intersect(EeWonRect { x: b.x, y: b.y, width: w, height: h }, b.clip);
            let q = [r.x, r.y, r.x.wrapping_sub(b.x), r.y.wrapping_sub(b.y), r.width, r.height];
            self.op(Op::RootDrawImage, g, b.image, q.as_ptr() as isize, 0);
        }
        // the map cleared: its images released (in key order; the original's tree went in its own order)
        let all = std::mem::take(&mut self.st.borrow_mut().bgs);
        for b in all.values() {
            self.release_image(b.image);
        }
    }

    pub fn paint(&self, g: isize) {
        let scheme = self.op(Op::RootScheme, 0, 0, 0, 0);
        self.op(Op::CtSetScheme, g, scheme, 0, 0);
        let r = self.st.borrow().invalid;
        self.draw(g, EE_WON_DRAW_PUSH_CLIP, [r.x, r.y, r.width, r.height]);
        self.ct().paint(g);
        self.draw(g, EE_WON_DRAW_POP_CLIP, [0; 4]);
        self.st.borrow_mut().invalid = EeWonRect { x: 0, y: 0, width: 0, height: 0 };
    }

    pub fn invalidate_rect(&self, r: EeWonRect) {
        let mut s = self.st.borrow_mut();
        s.invalid = guilayout::union(s.invalid, r);
    }

    /// The cursor's component moved to the mouse at (x, y) (MouseMove's and MouseDrag's tail).
    fn move_cursor_component(&self, x: i32, y: i32) {
        let cursor = self.ct().field(EeWonCtField::CurrentCursor as i32);
        let (_, mx, my) = self.cursor_of(cursor);
        let (x, y) = guievent::cursor_place(x, y, mx, my);
        self.op(Op::ChildInvalidate, self.cursor_component(), 0, 0, 0);
        self.op(Op::ChildSetPos, self.cursor_component(), x as isize, y as isize, 0);
        let c = self.cursor_component();
        let (w, h) = (self.op(Op::ChildWidth, c, 0, 0, 0) as i32, self.op(Op::ChildHeight, c, 0, 0, 0) as i32);
        self.invalidate_rect(EeWonRect { x, y, width: w, height: h });
    }

    pub fn mouse_move(&self, x: i32, y: i32) {
        self.ct().mouse_move(x, y);
        if self.cursor_component() == 0 {
            return;
        }
        self.move_cursor_component(x, y);
    }

    pub fn mouse_drag(&self, x: i32, y: i32) {
        let (dragging, dx, dy) = {
            let s = self.st.borrow();
            (s.dragging, s.drag_x, s.drag_y)
        };
        if !dragging {
            self.ct().mouse_drag(x, y);
            if self.cursor_component() != 0 {
                self.move_cursor_component(x, y);
            }
            return;
        }
        let me = self.me();
        let (left, top) = (self.op(Op::ChildLeft, me, 0, 0, 0) as i32, self.op(Op::ChildTop, me, 0, 0, 0) as i32);
        let Some((nx, ny)) = guievent::drag_place(left, top, x, y, dx, dy) else {
            return;
        };
        let mut r = EeWonRect4::default();
        self.op(Op::RootWindowRect, &mut r as *mut EeWonRect4 as isize, 0, 0, 0);
        let (wx, wy) = guievent::drag_window(r.x, r.y, nx, ny);
        self.op(Op::RootWindowSetPos, wx as isize, wy as isize, 0, 0);
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.ct().mouse_down(x, y, button);
        let allow = self.st.borrow().allow_drag;
        if button != 0 || !allow {
            return;
        }
        let mut mouse = self.ct().field(EeWonCtField::MouseChild as i32);
        if self.op(Op::RootOneComponentWindow, 0, 0, 0, 0) != 0 && mouse != 0 {
            mouse = self.op(Op::RootMouseChildOf, mouse, 0, 0, 0);
        }
        if mouse == 0 {
            return;
        }
        let drags = self.op(Op::CtBits, mouse, 0, 0, 0) & FLAG_PARENT_DRAG != 0;
        if !guievent::drag_starts(button, allow, drags) {
            return;
        }
        let mut s = self.st.borrow_mut();
        s.dragging = true;
        s.drag_x = x;
        s.drag_y = y;
    }

    pub fn mouse_exit(&self) {
        self.ct().mouse_exit();
        self.st.borrow_mut().dragging = false;
        let c = self.cursor_component();
        if c != 0 {
            self.op(Op::CtVirt, V::RemoveChild as isize, c, 0, 0);
        }
    }

    pub fn mouse_up(&self, x: i32, y: i32, button: i32) {
        self.ct().mouse_up(x, y, button);
        if guievent::drag_ends(button) {
            self.st.borrow_mut().dragging = false;
        }
    }

    pub fn remove_child(&self, c: isize) {
        {
            let mut s = self.st.borrow_mut();
            if c == s.cursor_component {
                s.cursor_component = 0;
            }
        }
        self.ct().remove_child(c);
    }

    pub fn set_cursor(&self, cursor: isize) {
        self.ct().set_cursor(cursor);
        let cursor = if cursor == 0 { self.op(Op::CtDefaultCursor, 0, 0, 0, 0) } else { cursor };
        let (comp, mx, my) = if cursor != 0 { self.cursor_of(cursor) } else { (0, 0, 0) };
        let old = self.cursor_component();
        if old == comp {
            return;
        }
        if old != 0 {
            self.op(Op::CtVirt, V::RemoveChild as isize, old, 0, 0);
        }
        self.st.borrow_mut().cursor_component = comp;
        if comp == 0 {
            return;
        }
        if self.op(Op::CtParentOf, comp, 0, 0, 0) != 0 {
            self.op(Op::CtDetach, comp, 0, 0, 0);
            let bg = self.op(Op::CtGrabBG, comp, 0, 0, 0);
            if bg != 0 {
                self.op(Op::RootGrabberDrop, bg, 0, 0, 0);
            }
        }
        self.op(Op::ChildSetFlags, comp, FLAGS_CURSOR_COMPONENT, 1, 0);
        self.op(Op::ChildSetVisible, comp, 1, 0, 0);
        let (mut wx, mut wy) = (0i32, 0i32);
        self.op(Op::RootWindowMouse, &mut wx as *mut i32 as isize, &mut wy as *mut i32 as isize, 0, 0);
        let (x, y) = guievent::cursor_place(wx, wy, mx, my);
        self.op(Op::ChildSetPos, comp, x as isize, y as isize, 0);
        self.ct().add_child(comp);
        let top = self.ct().field(EeWonCtField::BackChild as i32);
        if top != comp {
            self.op(Op::RootEnsureOnTop, comp, top, 0, 0);
        }
    }

    pub fn field(&self, f: i32) -> isize {
        type F = EeWonRootField;
        let s = self.st.borrow();
        match f {
            x if x == F::InvalidX as i32 => s.invalid.x as isize,
            x if x == F::InvalidY as i32 => s.invalid.y as isize,
            x if x == F::InvalidWidth as i32 => s.invalid.width as isize,
            x if x == F::InvalidHeight as i32 => s.invalid.height as isize,
            x if x == F::CursorComponent as i32 => s.cursor_component,
            x if x == F::Dragging as i32 => s.dragging as isize,
            x if x == F::DragX as i32 => s.drag_x as isize,
            x if x == F::DragY as i32 => s.drag_y as isize,
            x if x == F::AllowDrag as i32 => s.allow_drag as isize,
            x if x == F::BgCount as i32 => s.bgs.len() as isize,
            _ => 0,
        }
    }

    /// The destructor: the queued backgrounds' images released (in key order).
    pub fn destroy(&self) {
        let all = std::mem::take(&mut self.st.borrow_mut().bgs);
        for b in all.values() {
            self.release_image(b.image);
        }
    }

    pub fn call(&self, call: EeWonRootCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonRootCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SizeChanged => self.size_changed(),
            C::PrePaint => self.pre_paint(a),
            C::Paint => self.paint(a),
            C::MouseExit => self.mouse_exit(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseUp => self.mouse_up(ai, bi, ci),
            C::MouseMove => self.mouse_move(ai, bi),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::KeyChar => {
                self.ct().key_char(ai);
                return 1;
            }
            C::KeyDown => {
                self.ct().key_down(ai);
                return 1;
            }
            C::KeyUp => {
                self.ct().key_up(ai);
                return 1;
            }
            C::InvalidateRect => {
                // SAFETY: the C++ passes a WONRectangle (x, y, width, height)
                let r = unsafe { *(a as *const EeWonRect4) };
                self.invalidate_rect(rect_of(r));
            }
            C::SetCursor => self.set_cursor(a),
            C::RemoveChild => self.remove_child(a),
            C::AddBG => self.add_bg(a),
            C::GetField => return self.field(ai),
        }
        0
    }
}

/// A new Rust RootContainer for the C++ `component` over its container's Rust object `ct`, its calls out through
/// `host`.
///
/// # Safety
/// `ct` is the component's live container object and outlives the root's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_root_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
) -> *mut EeWonRoot {
    Box::into_raw(Box::new(EeWonRoot::new(component, host, ct)))
}

/// The destructor's statements (the queued backgrounds' images released), then the object freed.
///
/// # Safety
/// `root` came from `ee_won_root_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_root_free(root: *mut EeWonRoot) {
    if !root.is_null() {
        // SAFETY: the contract
        unsafe {
            (*root).destroy();
            drop(Box::from_raw(root));
        }
    }
}

/// One of RootContainer's methods (`EeWonRootCall`).
///
/// # Safety
/// `root` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_root_call(
    root: *const EeWonRoot,
    call: EeWonRootCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*root).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "rootobj_tests.rs"]
mod tests;
