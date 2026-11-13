//! WONGUI's Dialog as a whole Rust object (R6 WON round 26): the base of the lobby's dialogs (the message boxes, the
//! connect, password and game-options dialogs). The original's DialogBehavior.cpp and the constructor's state: the
//! drag (its place and whether it runs), whether it may run and stays inside the root, the Escape result and the
//! default button (held by reference, as the original's `SmartPtr`).
//!
//! The C++ `Dialog` stays the type the lobby derives its dialogs from (its vtable their overrides); its virtuals and
//! methods are one-line forwarders to `ee_won_dlg_call`. Its Rust object holds the container's Rust object (made by
//! the base's constructor, freed after this one) and calls it for every `Container::X` the original made; every
//! virtual the original called on itself (Invalidate, InvalidateRect) and every call on the parent, the window, the
//! default button and the event goes through the C++ object.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::ctobj::EeWonContainer;
use crate::guictl::EeWonRect4;
use crate::guihost::{EeWonCtVirt, EeWonGuiHostFn, EeWonGuiOp as Op};

const FLAG_PARENT_DRAG: isize = 0x20;
const FLAG_DISABLED: isize = 0x80;
const EVENT_BUTTON_PRESSED: isize = 15;
const EVENT_INPUT_RETURN: isize = 20;
const KEY_ENTER: i32 = 13;
const KEY_ESCAPE: i32 = 27;

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonDlgCall {
    /// `SetEndOnEscape(a)`.
    SetEndOnEscape = 1,
    SetDontEndOnEscape,
    /// `SetDefaultButton(a)` (a `Button*` as a `Component*`).
    SetDefaultButton,
    GotFocus,
    LostFocus,
    /// `Virtual08` (SizeChanged).
    SizeChanged,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    MouseExit,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    /// `KeyDown(a)` (0 or 1).
    KeyDown,
    /// `CanClose()` (0 or 1).
    CanClose,
    /// `ActivateDefaultButton()` (0 or 1).
    ActivateDefaultButton,
    /// `HandleComponentEvent(a)` (a `ComponentEvent*`).
    HandleComponentEvent,
    /// the field `a` (`EeWonDlgField`).
    GetField,
    /// the field `a` = `b`.
    SetField,
}

/// The dialog's fields its configs and subclasses read and set.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonDlgField {
    AllowDrag = 1,
    KeepInRoot,
    EndOnEscape,
    EscapeId,
    Dragging,
    DragX,
    DragY,
    DefaultButton,
}

#[derive(Debug, Default)]
struct State {
    drag_x: i32,
    drag_y: i32,
    dragging: bool,
    allow_drag: bool,
    keep_in_root: bool,
    end_on_escape: bool,
    escape_id: i32,
    default_button: isize,
}

/// The Rust Dialog.
pub struct EeWonDialog {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
    st: RefCell<State>,
}

impl EeWonDialog {
    /// The constructor's state: no drag (the original left the flag unset: false here), the drag allowed and kept in
    /// the root, Escape not ending the dialog (its result -1), no default button.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, ct: *const EeWonContainer) -> Self {
        EeWonDialog {
            comp,
            host,
            ct,
            st: RefCell::new(State { allow_drag: true, keep_in_root: true, escape_id: -1, ..State::default() }),
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
    fn flag(&self, c: isize, f: isize) -> bool {
        self.op(Op::CtBits, c, 0, 0, 0) & f != 0
    }

    pub fn set_default_button(&self, button: isize) {
        // the SmartPtr assignment: the old one released, then the new one held
        let old = self.st.borrow().default_button;
        if old == button {
            return;
        }
        if old != 0 {
            self.op(Op::ChildRelease, old, 0, 0, 0);
        }
        self.st.borrow_mut().default_button = button;
        if button != 0 {
            self.op(Op::ChildHold, button, 0, 0, 0);
        }
    }

    pub fn got_focus(&self) {
        let b = self.st.borrow().default_button;
        self.op(Op::DlgDefaultButton, 1, b, 0, 0);
        self.ct().got_focus();
    }

    pub fn lost_focus(&self) {
        let b = self.st.borrow().default_button;
        self.op(Op::DlgDefaultButton, 0, b, 0, 0);
        self.ct().lost_focus();
    }

    pub fn size_changed(&self) {
        self.st.borrow_mut().dragging = false;
        self.ct().size_changed();
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        self.ct().mouse_down(x, y, button);
        if button != 0 {
            return;
        }
        self.op(Op::RequestFocus, 0, 0, 0, 0);
        let mouse = self.ct().field(crate::ctobj::EeWonCtField::MouseChild as i32);
        if !self.st.borrow().allow_drag || mouse == 0 || !self.flag(mouse, FLAG_PARENT_DRAG) {
            return;
        }
        if self.op(Op::CtNextSibling, self.me(), 1, 0, 0) != 0 {
            self.op(Op::DlgBringToTop, 0, 0, 0, 0);
        }
        let mut s = self.st.borrow_mut();
        s.dragging = true;
        s.drag_x = x;
        s.drag_y = y;
    }

    pub fn mouse_up(&self, x: i32, y: i32, button: i32) {
        self.ct().mouse_up(x, y, button);
        if button == 0 {
            self.st.borrow_mut().dragging = false;
        }
    }

    pub fn mouse_exit(&self) {
        self.ct().mouse_exit();
        self.st.borrow_mut().dragging = false;
    }

    pub fn mouse_drag(&self, x: i32, y: i32) {
        let (dragging, drag_x, drag_y, keep) = {
            let s = self.st.borrow();
            (s.dragging, s.drag_x, s.drag_y, s.keep_in_root)
        };
        if !dragging {
            self.ct().mouse_drag(x, y);
            return;
        }
        // the original clamps against GetRootContainer(), where the newer SDK uses GetParent()
        let root = self.op(Op::CtRoot, 0, 0, 0, 0);
        let me = self.me();
        let (left, top) = (self.op(Op::ChildLeft, me, 0, 0, 0) as i32, self.op(Op::ChildTop, me, 0, 0, 0) as i32);
        let (w, h) = (self.op(Op::ChildWidth, me, 0, 0, 0) as i32, self.op(Op::ChildHeight, me, 0, 0, 0) as i32);
        let mut nx = left.wrapping_add(x.wrapping_sub(drag_x));
        let mut ny = top.wrapping_add(y.wrapping_sub(drag_y));
        if keep {
            let (rw, rh) =
                (self.op(Op::ChildWidth, root, 0, 0, 0) as i32, self.op(Op::ChildHeight, root, 0, 0, 0) as i32);
            if nx.wrapping_add(w) > rw {
                nx = rw.wrapping_sub(w);
            }
            if nx < 0 {
                nx = 0;
            }
            if ny.wrapping_add(h) > rh {
                ny = rh.wrapping_sub(h);
            }
            if ny < 0 {
                ny = 0;
            }
        }
        if nx == left && ny == top {
            return;
        }
        // one rectangle for both calls: Component::InvalidateRect moves it by the dialog's place, and the original
        // gave the moved one to the second call
        let mut r = EeWonRect4 { x: 0, y: 0, w, h };
        let rp = &mut r as *mut EeWonRect4 as isize;
        self.op(Op::CtParentCall, EeWonCtVirt::ChildInvalidated as isize, 0, 0, 0);
        self.op(Op::DlgInvalidateRect, rp, 0, 0, 0);
        self.op(Op::ChildSetPos, me, nx as isize, ny as isize, 0);
        self.op(Op::DlgInvalidateRect, rp, 0, 0, 0);
        self.op(Op::Invalidate, 0, 0, 0, 0);
    }

    pub fn key_down(&self, key: i32) -> bool {
        if self.ct().key_down(key) {
            return true;
        }
        let (end, id) = {
            let s = self.st.borrow();
            (s.end_on_escape, s.escape_id)
        };
        if key == KEY_ESCAPE && end {
            self.op(Op::DlgEndDialog, id as isize, self.me(), 0, 0);
            return true;
        } else if key == KEY_ENTER {
            return self.activate_default_button();
        }
        false
    }

    pub fn can_close(&self) -> bool {
        if !self.ct().can_close() {
            return false;
        }
        if self.op(Op::DlgIsModal, 0, 0, 0, 0) != 0 {
            let (end, id) = {
                let s = self.st.borrow();
                (s.end_on_escape, s.escape_id)
            };
            if end {
                self.op(Op::DlgEndDialog, id as isize, 0, 0, 0);
            } else {
                return false;
            }
        }
        true
    }

    pub fn activate_default_button(&self) -> bool {
        let b = self.st.borrow().default_button;
        if b != 0 && !self.flag(b, FLAG_DISABLED) && self.op(Op::DlgIsDefault, b, 0, 0, 0) != 0 {
            self.op(Op::DlgClick, b, 0, 0, 0);
            return true;
        }
        false
    }

    pub fn handle_component_event(&self, event: isize) {
        let kind = self.op(Op::EventType, event, 0, 0, 0);
        if kind == EVENT_INPUT_RETURN {
            self.activate_default_button();
            return;
        }
        if kind == EVENT_BUTTON_PRESSED && self.op(Op::DlgEventButtonFlags, event, 0, 0, 0) & 0x100 != 0 {
            let id = self.op(Op::EventCompControlId, event, 0, 0, 0);
            self.op(Op::DlgEndDialog, id, 0, 0, 0);
            return;
        }
        self.op(Op::ComponentHandleEvent, event, 0, 0, 0);
    }

    pub fn field(&self, f: i32) -> isize {
        type F = EeWonDlgField;
        let s = self.st.borrow();
        match f {
            x if x == F::AllowDrag as i32 => s.allow_drag as isize,
            x if x == F::KeepInRoot as i32 => s.keep_in_root as isize,
            x if x == F::EndOnEscape as i32 => s.end_on_escape as isize,
            x if x == F::EscapeId as i32 => s.escape_id as isize,
            x if x == F::Dragging as i32 => s.dragging as isize,
            x if x == F::DragX as i32 => s.drag_x as isize,
            x if x == F::DragY as i32 => s.drag_y as isize,
            x if x == F::DefaultButton as i32 => s.default_button,
            _ => 0,
        }
    }

    pub fn set_field(&self, f: i32, v: isize) {
        type F = EeWonDlgField;
        let mut s = self.st.borrow_mut();
        match f {
            x if x == F::AllowDrag as i32 => s.allow_drag = v != 0,
            x if x == F::KeepInRoot as i32 => s.keep_in_root = v != 0,
            x if x == F::Dragging as i32 => s.dragging = v != 0,
            _ => {}
        }
    }

    /// The destructor: the default button released.
    pub fn destroy(&self) {
        self.set_default_button(0);
    }

    pub fn call(&self, call: EeWonDlgCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonDlgCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::SetEndOnEscape => {
                let mut s = self.st.borrow_mut();
                s.end_on_escape = true;
                s.escape_id = ai;
            }
            C::SetDontEndOnEscape => self.st.borrow_mut().end_on_escape = false,
            C::SetDefaultButton => self.set_default_button(a),
            C::GotFocus => self.got_focus(),
            C::LostFocus => self.lost_focus(),
            C::SizeChanged => self.size_changed(),
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseUp => self.mouse_up(ai, bi, ci),
            C::MouseExit => self.mouse_exit(),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::KeyDown => return self.key_down(ai) as isize,
            C::CanClose => return self.can_close() as isize,
            C::ActivateDefaultButton => return self.activate_default_button() as isize,
            C::HandleComponentEvent => self.handle_component_event(a),
            C::GetField => return self.field(ai),
            C::SetField => self.set_field(ai, b),
        }
        0
    }
}

/// A new Rust Dialog for the C++ `component` over its container's Rust object `ct`, its calls out through `host`.
///
/// # Safety
/// `ct` is the component's live container object and outlives the dialog's.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dlg_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    ct: *const EeWonContainer,
) -> *mut EeWonDialog {
    Box::into_raw(Box::new(EeWonDialog::new(component, host, ct)))
}

/// The destructor's statement (the default button released), then the object freed.
///
/// # Safety
/// `dlg` came from `ee_won_dlg_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dlg_free(dlg: *mut EeWonDialog) {
    if !dlg.is_null() {
        // SAFETY: the contract
        unsafe {
            (*dlg).destroy();
            drop(Box::from_raw(dlg));
        }
    }
}

/// One of Dialog's methods (`EeWonDlgCall`).
///
/// # Safety
/// `dlg` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_dlg_call(
    dlg: *const EeWonDialog,
    call: EeWonDlgCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*dlg).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "dialogobj_tests.rs"]
mod tests;
