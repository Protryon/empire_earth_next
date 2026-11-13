//! WONGUI's popup menu as a whole Rust object (R6 WON round 24): PopupMenu (the lobby's right-click menus on users
//! and games, the combo boxes' drop-downs are plain lists). The original's MenuBehavior.cpp (PopupMenu's part) and
//! MenuItemLifetime.cpp's Reset, with their state: the open submenu and its item (held), the swallowed first release,
//! the submenu timer, the last mouse place and the keyboard mode.
//!
//! The C++ `PopupMenu` stays the type the lobby and the window manager hold (its constructor's font, flags,
//! background, colour and padding, and the item adders, which make C++ items), with one-line forwarders to
//! `ee_won_menu_call`. The ListArea base is the Rust list (`listobj`), called directly. The submenus are other C++
//! menus, reached through ops (their selection, Reset, keys), as the popups' and the window's calls.
//!
//! Every method is the original's statements in order with the calls out at the same points (`guimenu` and
//! `guiwin` keep the decisions). The state is behind a `RefCell` never borrowed across a call out.

use std::cell::RefCell;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guihost::{EeWonGuiField as F, EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guimenu;
use crate::guiwin;
use crate::listobj::{EeWonList, EeWonListCall as L};

#[derive(Debug, Default)]
struct MenuState {
    /// the open submenu (a C++ PopupMenu, held), or 0
    sub: isize,
    /// its item in this menu (a C++ ListItem, held), or 0
    sub_item: isize,
    mouse_up_count: i32,
    timer_acc: i32,
    last_x: i32,
    last_y: i32,
    keyboard: bool,
}

/// The inward calls (`PopupMenu`'s virtuals and methods).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonMenuCall {
    /// `MouseDown(a, b, c)`.
    MouseDown = 1,
    /// `MouseUp(a, b, c)`.
    MouseUp,
    /// `MouseMove(a, b)`.
    MouseMove,
    /// `TimerEvent(a)` (0 or 1).
    TimerEvent,
    /// `HandleComponentEvent(a)`.
    HandleEvent,
    ListSelChangedHook,
    EndSubMenu,
    CheckSubMenu,
    /// `TryKeyDown(a)` (0 or 1).
    TryKeyDown,
    Reset,
    /// `mCurSubMenu`.
    SubMenu,
    /// `mKeyboardMode` (0 or 1).
    KeyboardMode,
    /// `mTimerAcc`.
    TimerAcc,
    /// `mMouseUpCount`.
    MouseUpCount,
}

/// The Rust PopupMenu.
pub struct EeWonMenu {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    list: *const EeWonList,
    st: RefCell<MenuState>,
}

impl EeWonMenu {
    /// The constructor's state (no submenu, no release seen, the timer at 0, mouse mode). The original left the
    /// last mouse place unset (only keyboard mode reads it, after a move set it): 0 here.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn, list: *const EeWonList) -> Self {
        EeWonMenu { comp, host, list, st: RefCell::new(MenuState::default()) }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's component; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn op1(&self, op: Op, a: isize) -> isize {
        self.op(op, a, 0, 0, 0)
    }
    fn base(&self, c: L, a: isize, b: isize, cc: isize) -> isize {
        if self.list.is_null() {
            return 0;
        }
        // SAFETY: the list's Rust object lives as long as the C++ menu (the base's destructor frees it after this
        // one); a shared reference: the calls out re-enter through it
        unsafe { (*self.list).call(c, a, b, cc) }
    }
    fn keyboard(&self) -> bool {
        self.st.borrow().keyboard
    }
    fn sub(&self) -> isize {
        self.st.borrow().sub
    }

    /// `mCurSubMenu = s` (a `SmartPtr` assignment).
    fn set_sub(&self, s: isize) {
        let old = self.sub();
        if old != s {
            if old != 0 {
                self.op1(Op::ChildRelease, old);
            }
            self.st.borrow_mut().sub = s;
            if s != 0 {
                self.op1(Op::ChildHold, s);
            }
        }
    }
    /// `mCurSubMenuItem = i`.
    fn set_sub_item(&self, i: isize) {
        let old = self.st.borrow().sub_item;
        if old != i {
            if old != 0 {
                self.op1(Op::ItemRelease, old);
            }
            self.st.borrow_mut().sub_item = i;
            if i != 0 {
                self.op1(Op::ItemHold, i);
            }
        }
    }

    pub fn end_sub_menu(&self) {
        let s = self.sub();
        if s == 0 {
            return;
        }
        self.op1(Op::ClosePopup, s);
        if !self.keyboard() {
            self.base(L::SetListFlags, 4, 1, 0);
        }
        self.set_sub(0);
        self.set_sub_item(0);
    }

    pub fn handle_event(&self, event: isize) {
        let from = self.op1(Op::EventComponent, event);
        let kind = self.op1(Op::EventType, event) as i32;
        if guimenu::popup_event(from == self.sub(), kind) != 0 {
            if self.op1(Op::MenuSelItem, self.sub()) != 0 {
                let item = self.st.borrow().sub_item;
                let pos = self.base(L::GetItemPos, item, 0, 0);
                self.base(L::SetSelItemAt, pos, 1, 0);
            }
        } else {
            self.op1(Op::BaseHandleEvent, event);
        }
    }

    pub fn timer_event(&self, delta: i32) -> bool {
        let (acc, check, keep) = guiwin::menu_timer(self.keyboard(), self.st.borrow().timer_acc, delta);
        self.st.borrow_mut().timer_acc = acc;
        if check {
            self.check_sub_menu();
        }
        keep
    }

    pub fn list_sel_changed_hook(&self) {
        self.st.borrow_mut().timer_acc = 0;
        if self.keyboard() {
            return;
        }
        let (sub, sub_item) = {
            let s = self.st.borrow();
            (s.sub, s.sub_item)
        };
        let sel = self.base(L::SelItem, 0, 0, 0);
        if guimenu::sel_clears_sub(self.keyboard(), sub != 0, sel == sub_item) {
            self.op(Op::MenuSetSelAt, sub, -1, 0, 0);
        }
        self.op1(Op::RequestTimer, 1);
    }

    pub fn mouse_move(&self, x: i32, y: i32) {
        let (kb, lx, ly) = {
            let s = self.st.borrow();
            (s.keyboard, s.last_x, s.last_y)
        };
        let Some(moved) = guiwin::menu_mouse_move(kb, x, y, lx, ly) else { return };
        if moved {
            self.st.borrow_mut().keyboard = false;
            let none = self.sub() == 0;
            self.base(L::SetListFlags, 4, none as isize, 0);
        }
        {
            let mut s = self.st.borrow_mut();
            s.last_x = x;
            s.last_y = y;
        }
        self.base(L::MouseMove, x as isize, y as isize, 0);
    }

    pub fn mouse_up(&self, x: i32, y: i32, button: i32) {
        let (count, go) = guiwin::menu_mouse_up(button, self.st.borrow().mouse_up_count);
        self.st.borrow_mut().mouse_up_count = count;
        if !go {
            return;
        }
        self.base(L::MouseUp, x as isize, y as isize, button as isize);
    }

    pub fn check_sub_menu(&self) {
        self.st.borrow_mut().timer_acc = 0;
        let sel = self.base(L::SelItem, 0, 0, 0);
        let item = self.op1(Op::MenuItemOf, sel);
        let submenu = if item != 0 { self.op1(Op::MenuItemSub, item) } else { 0 };
        let cur = self.sub();
        let disabled = item != 0 && self.op1(Op::MenuItemDisabled, item) != 0;
        let step = guimenu::check_sub(item != 0, cur != 0, item != 0 && cur == submenu, submenu != 0, disabled);
        if step == 0 {
            return;
        }
        self.end_sub_menu();
        if step != 2 {
            return;
        }
        self.base(L::SetListFlags, 4, 0, 0);
        self.set_sub(submenu);
        self.set_sub_item(item);
        let width = self.op(Op::Get, F::ScrollWidth as isize, 0, 0, 0) as i32;
        let (mut x, mut y) = guiwin::submenu_place(width, self.op1(Op::ItemTop, item) as i32);
        self.op(Op::ScrollUntranslate, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0, 0);
        self.op(Op::LocalToRoot, &mut x as *mut i32 as isize, &mut y as *mut i32 as isize, 0, 0);
        self.op1(Op::MenuReset, submenu);
        let width = self.op(Op::Get, F::ScrollWidth as isize, 0, 0, 0) as i32;
        let avoid: EeWonRect4 = guiwin::submenu_avoid(x, width);
        let at = [x, y];
        self.op(
            Op::QueueSubMenu,
            submenu,
            at.as_ptr() as isize,
            &avoid as *const EeWonRect4 as isize,
            guimenu::ee_won_gui_menu_submenu_flags() as isize,
        );
    }

    pub fn mouse_down(&self, x: i32, y: i32, button: i32) {
        if button == 0 {
            self.st.borrow_mut().mouse_up_count = 0;
            self.op(Op::QueueMouseUp, x as isize, y as isize, button as isize, 0);
            return;
        }
        self.base(L::MouseDown, x as isize, y as isize, button as isize);
    }

    pub fn try_key_down(&self, key: i32) -> bool {
        self.st.borrow_mut().keyboard = true;
        let sub = self.sub();
        let has = sub != 0;
        if has && self.op(Op::MenuTryKeyDown, sub, key as isize, 0, 0) != 0 {
            return true;
        }
        let sub_has_sel = has && self.op1(Op::MenuSelItem, sub) != 0;
        match guimenu::popup_key(has, key, sub_has_sel) {
            1 => {
                self.end_sub_menu();
                true
            }
            2 => {
                self.op(Op::MenuSetSelAt, self.sub(), 0, 0, 0);
                true
            }
            3 => {
                self.check_sub_menu();
                let s = self.sub();
                if s != 0 {
                    self.op(Op::MenuSetSelAt, s, 0, 0, 0);
                    true
                } else {
                    false
                }
            }
            4 => {
                self.base(L::KeyDown, key as isize, 0, 0);
                true
            }
            _ => false,
        }
    }

    pub fn reset(&self) {
        self.set_sub(0);
        self.base(L::SetSelItemAt, -1, 1, 0);
    }

    /// One inward call.
    pub fn call(&self, call: EeWonMenuCall, a: isize, b: isize, c: isize) -> isize {
        type C = EeWonMenuCall;
        let (ai, bi, ci) = (a as i32, b as i32, c as i32);
        match call {
            C::MouseDown => self.mouse_down(ai, bi, ci),
            C::MouseUp => self.mouse_up(ai, bi, ci),
            C::MouseMove => self.mouse_move(ai, bi),
            C::TimerEvent => return self.timer_event(ai) as isize,
            C::HandleEvent => self.handle_event(a),
            C::ListSelChangedHook => self.list_sel_changed_hook(),
            C::EndSubMenu => self.end_sub_menu(),
            C::CheckSubMenu => self.check_sub_menu(),
            C::TryKeyDown => return self.try_key_down(ai) as isize,
            C::Reset => self.reset(),
            C::SubMenu => return self.sub(),
            C::KeyboardMode => return self.keyboard() as isize,
            C::TimerAcc => return self.st.borrow().timer_acc as isize,
            C::MouseUpCount => return self.st.borrow().mouse_up_count as isize,
        }
        0
    }
}

impl Drop for EeWonMenu {
    fn drop(&mut self) {
        // the original's members go in reverse order: the submenu's item, then the submenu
        let (sub, item) = {
            let mut s = self.st.borrow_mut();
            (std::mem::take(&mut s.sub), std::mem::take(&mut s.sub_item))
        };
        if item != 0 {
            self.op1(Op::ItemRelease, item);
        }
        if sub != 0 {
            self.op1(Op::ChildRelease, sub);
        }
    }
}

// ---- the C ABI

/// A new Rust PopupMenu for the C++ `component`, its calls out through `host`, its base the Rust list `list`.
#[no_mangle]
pub extern "C" fn ee_won_menu_new(
    component: *mut c_void,
    host: EeWonGuiHostFn,
    list: *const EeWonList,
) -> *mut EeWonMenu {
    Box::into_raw(Box::new(EeWonMenu::new(component, host, list)))
}

/// # Safety
/// `menu` came from `ee_won_menu_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_menu_free(menu: *mut EeWonMenu) {
    if !menu.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(menu) });
    }
}

/// One of PopupMenu's methods (`EeWonMenuCall`).
///
/// # Safety
/// `menu` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_menu_call(
    menu: *const EeWonMenu,
    call: EeWonMenuCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*menu).call(call, a, b, c) }
}

#[cfg(test)]
#[path = "menuobj_tests.rs"]
mod tests;
