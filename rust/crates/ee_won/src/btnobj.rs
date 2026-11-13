//! WONGUI's Button as a whole Rust object (R6 WON round 30): every push button, check box, radio button, arrow and
//! tab of the lobby. The original's Button.cpp: the button flags, whether Space holds it, the repeat delay and the
//! mouse's press state, with every method, the radio group's walk and the two default buttons (the original's static
//! `SmartPtr`s `mDefaultButton` and `mDialogDefaultButton`).
//!
//! The C++ `Button` stays the type its skins (MSButton, MSCheckbox, MSRadioButton, WONButton, ...) and the lobby use
//! (its vtable their overrides); its methods are one-line forwarders. The click sound stays a C++ member: the Rust
//! object asks whether there is one and plays it through ops. Every virtual the original called on itself
//! (`Invalidate`, `HandleActivate`, `SetTextHook`) goes through the C++ object, so the skins' overrides run. The radio
//! group's walk reads the other buttons' Rust objects (`BtnOf`, a pure read as the container's `CtContainerOf`) and
//! calls their non-virtual methods directly, as the original did; the parent's `SetHasUsedDialogKeys` goes to the
//! parent's Rust container.

use std::cell::Cell;
use std::ffi::c_void;
use std::sync::Mutex;

use crate::ctobj::{EeWonContainer, EeWonCtField};
use crate::guifocus::{self, EeWonButtonKey, EeWonButtonUp};
use crate::guihost::{EeWonCtVirt as V, EeWonGuiEvent, EeWonGuiHostFn, EeWonGuiOp as Op};

const FLAG_WANT_FOCUS: isize = 0x8;
const FLAG_DISABLED: isize = 0x80;

const BTN_HIGHLIGHT: u32 = 0x01;
const BTN_REPEAT: u32 = 0x02;
const BTN_CHECKED: u32 = 0x04;
const BTN_RADIO: u32 = 0x08;
const BTN_MOUSE_OVER: u32 = 0x10;
const BTN_PRESSED: u32 = 0x20;
const BTN_ACTIVATE_ON_PRESS: u32 = 0x40;
const BTN_NO_ACTIVATE: u32 = 0x200;
const BTN_DEFAULT_ON_FOCUS: u32 = 0x400;

/// The inward calls (the C++ shell's virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonBtnCall {
    /// `Fn2F3440()` (the activation).
    Click = 1,
    /// `SetText(*a, b != 0)` (`a` a `const GUIString*`).
    SetText,
    /// `SetCheck(a != 0, b != 0)`.
    SetCheck,
    /// `SetButtonFlags(a, b != 0)`.
    SetButtonFlags,
    /// `CopyAttributes`'s flags (`a` the other button's `EeWonButton*`; the click sound is C++'s statement).
    CopyAttributes,
    /// `IsMouseOver()` (0 or 1).
    IsMouseOver,
    /// `IsDefaultButton()` (0 or 1).
    IsDefaultButton,
    AddedToParent,
    /// `MouseEnter(a, b)`.
    MouseEnter,
    /// `MouseDown(a, b, c)`.
    MouseDown,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    MouseExit,
    ButtonDown,
    /// `ButtonUp(a != 0)`.
    ButtonUp,
    /// `MouseDrag(a, b)`.
    MouseDrag,
    GotFocus,
    LostFocus,
    /// `KeyDown(a)` (0 or 1).
    KeyDown,
    /// `KeyUp(a)` (0 or 1).
    KeyUp,
    /// `TimerEvent(a)` (1).
    TimerEvent,
    /// `MouseHover(a)`.
    MouseHover,
    /// `GetNextRadio(a != 0)`: the radio's `Component*`, or 0.
    GetNextRadio,
    /// `GetEndRadio(a != 0)`: the radio's `Component*`.
    GetEndRadio,
    /// the field `a` (`EeWonBtnField`).
    GetField,
    /// the field `a` = `b`.
    SetField,
}

/// The button's fields (the skins and the tests read them).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonBtnField {
    /// `mButtonFlags`.
    Flags = 1,
    /// `mKeyDown` (0 or 1).
    KeyDown,
    /// `mDelayAcc`.
    DelayAcc,
    /// `mMouseDownState` (0 up, 1 down outside, 2 down over).
    MouseDownState,
}

/// Button's static functions and the two default buttons (`ee_won_btn_static`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonBtnStatic {
    /// `SetDialogDefaultButton(a)`.
    SetDialogDefault = 1,
    /// `UnSetDialogDefaultButton(a)`.
    UnSetDialogDefault,
    /// `SetDefaultButton(a)`.
    SetDefault,
    /// `UnSetDefaultButton(a)`.
    UnSetDefault,
    /// `mDefaultButton.get()`.
    Default,
    /// `mDialogDefaultButton.get()`.
    DialogDefault,
}

/// The two default buttons, each held (`ChildHold`, as the original's static `SmartPtr`s): the current default and
/// the dialog's. Never locked across a call out.
static DEFAULTS: Mutex<[isize; 2]> = Mutex::new([0, 0]);
const CURRENT: usize = 0;
const DIALOG: usize = 1;

fn default_of(k: usize) -> isize {
    DEFAULTS.lock().unwrap_or_else(|e| e.into_inner())[k]
}

fn sop(host: EeWonGuiHostFn, op: Op, a: isize) -> isize {
    match host {
        // SAFETY: the host takes a NULL component for the ops on `a`
        Some(h) => unsafe { h(std::ptr::null_mut(), op, a, 0, 0, 0) },
        None => 0,
    }
}

/// `SmartPtr::operator=` on the default `k`: the old one released, the new one held (nothing when they are the same).
fn assign_default(host: EeWonGuiHostFn, k: usize, b: isize) {
    let old = default_of(k);
    if old == b {
        return;
    }
    if old != 0 {
        sop(host, Op::ChildRelease, old);
    }
    if b != 0 {
        sop(host, Op::ChildHold, b);
    }
    DEFAULTS.lock().unwrap_or_else(|e| e.into_inner())[k] = b;
}

/// `Button::SetDefaultButton`: the old and the new default repainted.
fn set_default(host: EeWonGuiHostFn, b: isize) {
    let cur = default_of(CURRENT);
    if b == cur {
        return;
    }
    if cur != 0 {
        sop(host, Op::ChildInvalidate, cur);
    }
    assign_default(host, CURRENT, b);
    if b != 0 {
        sop(host, Op::ChildInvalidate, b);
    }
}

/// `Button::UnSetDefaultButton`: the original ignores its argument and falls back to the dialog's default button.
fn unset_default(host: EeWonGuiHostFn) {
    set_default(host, default_of(DIALOG));
}

fn set_dialog_default(host: EeWonGuiHostFn, b: isize) {
    if b == default_of(DIALOG) {
        return;
    }
    assign_default(host, DIALOG, b);
    set_default(host, b);
}

fn unset_dialog_default(host: EeWonGuiHostFn, b: isize) {
    if b != default_of(DIALOG) {
        return;
    }
    assign_default(host, DIALOG, 0);
    unset_default(host);
}

/// The default buttons dropped without their releases (a test's failed replay).
#[cfg(test)]
pub(crate) fn forget_defaults() {
    *DEFAULTS.lock().unwrap_or_else(|e| e.into_inner()) = [0, 0];
}

/// One of Button's static functions (`EeWonBtnStatic`) over `host` (called with a NULL component).
pub fn static_call(host: EeWonGuiHostFn, call: EeWonBtnStatic, a: isize) -> isize {
    type S = EeWonBtnStatic;
    match call {
        S::SetDialogDefault => set_dialog_default(host, a),
        S::UnSetDialogDefault => unset_dialog_default(host, a),
        S::SetDefault => set_default(host, a),
        S::UnSetDefault => unset_default(host),
        S::Default => return default_of(CURRENT),
        S::DialogDefault => return default_of(DIALOG),
    }
    0
}

/// The Rust Button.
pub struct EeWonButton {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    flags: Cell<u32>,
    key_down: Cell<bool>,
    delay_acc: Cell<i32>,
    mouse_down_state: Cell<u8>,
}

impl EeWonButton {
    /// The constructor's state: no flags, not held, the mouse up.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonButton {
            comp,
            host,
            flags: Cell::new(0),
            key_down: Cell::new(false),
            delay_acc: Cell::new(0),
            mouse_down_state: Cell::new(0),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }
    fn me(&self) -> isize {
        self.comp as isize
    }
    fn base(&self, v: V, a: isize, b: isize) {
        self.op(Op::CtBase, v as isize, a, b, 0);
    }
    fn invalidate(&self) {
        self.op1(Op::Invalidate, 0);
    }
    fn set_component_flags(&self, comp: isize, f: isize, on: bool) {
        self.op(Op::ChildSetFlags, comp, f, on as isize, 0);
    }
    fn has(&self, f: u32) -> bool {
        self.flags.get() & f != 0
    }
    fn is_radio(&self) -> bool {
        self.has(BTN_RADIO)
    }
    fn is_checked(&self) -> bool {
        self.has(BTN_CHECKED)
    }
    fn is_pressed(&self) -> bool {
        self.has(BTN_PRESSED)
    }
    fn want_focus(&self) -> bool {
        self.op1(Op::CtWantFocus, 0) != 0
    }
    fn fire_pressed(&self) {
        self.op(Op::FireEvent, EeWonGuiEvent::ButtonPressed as isize, 0, 0, 0);
    }
    /// The Rust object of the button `comp` (`dynamic_cast<Button*>`), if it is one.
    fn btn_of(&self, comp: isize) -> Option<&EeWonButton> {
        let p = self.op1(Op::BtnOf, comp) as *const EeWonButton;
        // SAFETY: a button's Rust object lives as long as its C++ object, which the walk reached as a sibling
        (!p.is_null()).then(|| unsafe { &*p })
    }

    pub fn is_mouse_over(&self) -> bool {
        self.has(BTN_MOUSE_OVER)
    }

    pub fn is_default_button(&self) -> bool {
        default_of(CURRENT) == self.me()
    }

    pub fn set_text(&self, text: isize, invalidate: bool) {
        self.op1(Op::BtnSetTextHook, text);
        if invalidate {
            self.invalidate();
        }
    }

    pub fn set_button_flags(&self, flags: u32, set: bool) {
        let f = self.flags.get();
        self.flags.set(if set { f | flags } else { f & !flags });
        if self.is_radio() {
            self.set_component_flags(self.me(), FLAG_WANT_FOCUS, self.is_checked());
        }
    }

    /// The next (or previous) sibling if it is a radio button: its component and Rust object.
    pub fn next_radio(&self, forward: bool) -> Option<(isize, &EeWonButton)> {
        let sib = self.op(Op::CtNextSibling, self.me(), forward as isize, 0, 0);
        match self.btn_of(sib) {
            Some(b) if b.is_radio() => Some((sib, b)),
            _ => None,
        }
    }

    /// The last (or first) radio button of this button's group.
    pub fn end_radio(&self, forward: bool) -> (isize, &EeWonButton) {
        let mut last = (self.me(), self);
        while let Some(next) = last.1.next_radio(forward) {
            last = next;
        }
        last
    }

    pub fn added_to_parent(&self) {
        self.base(V::AddedToParent, 0, 0);
        self.set_button_flags(BTN_MOUSE_OVER | BTN_PRESSED, false);
        if !self.is_radio() {
            return;
        }
        self.set_check(self.is_checked(), false);
        if self.want_focus() {
            return;
        }
        let mut radio = Some(self.end_radio(false));
        while let Some((_, r)) = radio {
            if r.want_focus() {
                return;
            }
            radio = r.next_radio(true);
        }
        self.set_component_flags(self.me(), FLAG_WANT_FOCUS, true);
    }

    /// `Fn2F3440`: unless disabled, checks a radio or toggles the check, then fires the button event.
    pub fn click(&self) {
        if self.op1(Op::CtBits, self.me()) & FLAG_DISABLED != 0 || self.has(BTN_NO_ACTIVATE) {
            return;
        }
        if self.is_radio() {
            self.set_check(true, false);
        } else {
            self.set_check(!self.is_checked(), false);
        }
        if self.op1(Op::BtnHandleActivate, 0) != 0 {
            self.fire_pressed();
        }
    }

    pub fn set_check(&self, checked: bool, notify: bool) {
        if self.is_radio() {
            self.set_component_flags(self.me(), FLAG_WANT_FOCUS, checked);
            if checked {
                let mut radio = Some(self.end_radio(false));
                while let Some((c, r)) = radio {
                    if c != self.me() {
                        self.set_component_flags(c, FLAG_WANT_FOCUS, false);
                        if r.is_checked() {
                            r.set_button_flags(BTN_CHECKED, false);
                            self.op1(Op::ChildInvalidate, c);
                        }
                    }
                    radio = r.next_radio(true);
                }
                self.op1(Op::RequestFocus, 0);
            }
        }
        if notify {
            self.fire_pressed();
        }
        if self.is_checked() != checked {
            self.set_button_flags(BTN_CHECKED, checked);
            self.invalidate();
        }
    }

    pub fn mouse_enter(&self, x: i32, y: i32) {
        self.base(V::MouseEnter, x as isize, y as isize);
        self.set_button_flags(BTN_MOUSE_OVER, true);
        if self.has(BTN_HIGHLIGHT) {
            self.invalidate();
        }
    }

    pub fn mouse_down(&self, button: i32) {
        if button == 0 {
            self.mouse_down_state.set(2);
            self.button_down();
        }
    }

    pub fn mouse_up(&self, button: i32) {
        if button == 0 {
            self.mouse_down_state.set(0);
            self.button_up(true);
        }
    }

    pub fn mouse_exit(&self) {
        self.base(V::MouseExit, 0, 0);
        self.mouse_down_state.set(0);
        self.button_up(false);
        if self.is_mouse_over() && self.has(BTN_HIGHLIGHT) {
            self.invalidate();
        }
        self.set_button_flags(BTN_MOUSE_OVER, false);
    }

    pub fn button_down(&self) {
        if self.is_pressed() {
            return;
        }
        if self.op1(Op::BtnHasClickSound, 0) != 0 {
            self.op1(Op::BtnPlayClick, 0);
        }
        self.set_button_flags(BTN_PRESSED, true);
        self.invalidate();
        if self.has(BTN_REPEAT) {
            self.delay_acc.set(0);
            self.op1(Op::RequestTimer, 1);
            self.click();
        }
        if self.has(BTN_ACTIVATE_ON_PRESS) {
            self.click();
        }
        self.op1(Op::RequestFocus, 0);
    }

    pub fn button_up(&self, activate: bool) {
        let up = guifocus::button_up(
            self.flags.get(),
            i32::from(self.mouse_down_state.get()),
            self.key_down.get(),
            activate,
        );
        if up == EeWonButtonUp::Ignore {
            return;
        }
        self.set_button_flags(BTN_PRESSED, false);
        self.invalidate();
        if up == EeWonButtonUp::StopRepeat {
            self.op1(Op::RequestTimer, 0);
            return;
        }
        if up == EeWonButtonUp::Activate {
            self.click();
        }
    }

    pub fn mouse_drag(&self, x: i32, y: i32) {
        let w = self.op1(Op::ChildWidth, self.me()) as i32;
        let h = self.op1(Op::ChildHeight, self.me()) as i32;
        let place = guifocus::button_drag(i32::from(self.mouse_down_state.get()), x, y, w, h);
        if place < 0 {
            return;
        }
        if place == 0 {
            self.mouse_down_state.set(2);
            if !self.is_pressed() {
                self.button_down();
                self.delay_acc.set(500);
            }
        } else {
            self.mouse_down_state.set(1);
            self.button_up(false);
        }
    }

    pub fn got_focus(&self) {
        self.base(V::GotFocus, 0, 0);
        if self.has(BTN_DEFAULT_ON_FOCUS) {
            set_default(self.host, self.me());
        }
        self.invalidate();
    }

    pub fn lost_focus(&self) {
        self.base(V::LostFocus, 0, 0);
        if self.key_down.get() {
            self.key_down.set(false);
            self.button_up(false);
        }
        unset_default(self.host);
        self.invalidate();
    }

    /// `mParent->SetHasUsedDialogKeys(true)` on the parent's Rust container.
    fn parent_used_dialog_keys(&self) {
        let parent = self.op1(Op::CtParentOf, self.me());
        if parent == 0 {
            return;
        }
        let ct = self.op1(Op::CtContainerOf, parent) as *const EeWonContainer;
        if !ct.is_null() {
            // SAFETY: the parent's Rust container lives as long as the parent
            unsafe { &*ct }.set_field(EeWonCtField::HasUsedDialogKeys as i32, 1);
        }
    }

    pub fn key_down(&self, key: i32) -> bool {
        let action = guifocus::button_key(key, self.is_default_button(), self.key_down.get(), self.is_radio());
        match action {
            EeWonButtonKey::Activate => {
                self.click();
                true
            }
            EeWonButtonKey::Press => {
                self.key_down.set(true);
                self.parent_used_dialog_keys();
                self.button_down();
                true
            }
            EeWonButtonKey::RadioBack | EeWonButtonKey::RadioForward => {
                self.parent_used_dialog_keys();
                if let Some((c, next)) = self.next_radio(action == EeWonButtonKey::RadioForward) {
                    next.click();
                    self.op(Op::CtChild, c, V::RequestFocus as isize, 0, 0);
                }
                true
            }
            EeWonButtonKey::None => false,
        }
    }

    pub fn key_up(&self, key: i32) -> bool {
        if key == 32 {
            self.key_down.set(false);
            self.button_up(true);
            return true;
        }
        false
    }

    pub fn timer_event(&self, delta: i32) -> bool {
        let (d, fire) = guifocus::button_repeat(self.delay_acc.get(), delta);
        self.delay_acc.set(d);
        if fire {
            self.click();
        }
        true
    }

    pub fn field(&self, f: i32) -> isize {
        type F = EeWonBtnField;
        match f {
            x if x == F::Flags as i32 => self.flags.get() as isize,
            x if x == F::KeyDown as i32 => self.key_down.get() as isize,
            x if x == F::DelayAcc as i32 => self.delay_acc.get() as isize,
            x if x == F::MouseDownState as i32 => self.mouse_down_state.get() as isize,
            _ => 0,
        }
    }

    pub fn set_field(&self, f: i32, v: isize) {
        type F = EeWonBtnField;
        match f {
            x if x == F::Flags as i32 => self.flags.set(v as u32),
            x if x == F::KeyDown as i32 => self.key_down.set(v != 0),
            x if x == F::DelayAcc as i32 => self.delay_acc.set(v as i32),
            x if x == F::MouseDownState as i32 => self.mouse_down_state.set(v as u8),
            _ => {}
        }
    }

    pub fn call(&self, call: EeWonBtnCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonBtnCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::Click => self.click(),
            C::SetText => self.set_text(a, b != 0),
            C::SetCheck => self.set_check(a != 0, b != 0),
            C::SetButtonFlags => self.set_button_flags(a as u32, b != 0),
            C::CopyAttributes => {
                // SAFETY: the other button's live Rust object (the forwarder passes `from->mBtn`)
                if let Some(from) = unsafe { (a as *const EeWonButton).as_ref() } {
                    self.flags.set(from.flags.get());
                }
            }
            C::IsMouseOver => return self.is_mouse_over() as isize,
            C::IsDefaultButton => return self.is_default_button() as isize,
            C::AddedToParent => self.added_to_parent(),
            C::MouseEnter => self.mouse_enter(ai, bi),
            C::MouseDown => self.mouse_down(ci),
            C::MouseUp => self.mouse_up(ci),
            C::MouseExit => self.mouse_exit(),
            C::ButtonDown => self.button_down(),
            C::ButtonUp => self.button_up(a != 0),
            C::MouseDrag => self.mouse_drag(ai, bi),
            C::GotFocus => self.got_focus(),
            C::LostFocus => self.lost_focus(),
            C::KeyDown => return self.key_down(ai) as isize,
            C::KeyUp => return self.key_up(ai) as isize,
            C::TimerEvent => return self.timer_event(ai) as isize,
            C::MouseHover => self.base(V::MouseHover, a, 0),
            C::GetNextRadio => return self.next_radio(a != 0).map_or(0, |(c, _)| c),
            C::GetEndRadio => return self.end_radio(a != 0).0,
            C::GetField => return self.field(ai),
            C::SetField => self.set_field(ai, b),
        }
        0
    }
}

/// A new Rust Button for the C++ `component`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_btn_new(component: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonButton {
    Box::into_raw(Box::new(EeWonButton::new(component, host)))
}

/// Frees a Rust Button (the original's destructor had no statement).
///
/// # Safety
/// `btn` came from `ee_won_btn_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_btn_free(btn: *mut EeWonButton) {
    if !btn.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(btn) });
    }
}

/// One of Button's methods (`EeWonBtnCall`).
///
/// # Safety
/// `btn` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_btn_call(
    btn: *const EeWonButton,
    call: EeWonBtnCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*btn).call(call, a, b, c) }
}

/// One of Button's static functions (`EeWonBtnStatic`; `a` a `Button*` as a `Component*`), its calls out through
/// `host` with a NULL component.
#[no_mangle]
pub extern "C" fn ee_won_btn_static(host: EeWonGuiHostFn, call: EeWonBtnStatic, a: isize) -> isize {
    static_call(host, call, a)
}

#[cfg(test)]
#[path = "btnobj_tests.rs"]
mod tests;
