//! WONGUI's Window as a whole Rust object (R6 WON round 32): every lobby window's state and its input, cursor,
//! resize, close and centring logic (the original's Window.cpp).
//!
//! The C++ `Window` stays the type the manager, the platform subclass (`PlatGUIWindow`) and the components use: its
//! methods are one-line forwarders to `ee_won_win_call` and its accessors read `ee_won_win_field`. What stays C++ are
//! the members that are C++ objects: the root container (`mRoot`), the manager, the offscreen image, the display
//! context, the icon, the title, the font map and default font, the close event and the allow-close function. Every
//! call out is an op on the window's C++ object (`EeWonGuiOp::Win*`): the root's methods (`WinRoot` with an
//! `EeWonWinRoot`), the platform virtuals (`WinPlat` with an `EeWonWinPlat`, `WinSetPosSize`, `WinGetRect`), the
//! manager's (`WinScreenRect`, `WinAddToManager`), the tick, the new offscreen image, the close event, the cursors
//! and the other windows (`WinCanClose`, `WinChildSet`, `WinIsVisible`).
//!
//! The child window set is a `BTreeSet` of addresses: the original's `std::set<Window*>` ordered by pointer.

use std::cell::RefCell;
use std::collections::BTreeSet;
use std::ffi::c_void;

use crate::guictl::EeWonRect4;
use crate::guievent;
use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp as Op};

/// `Window::WindowState`.
pub const EE_WON_WIN_OPEN: i32 = 0;
pub const EE_WON_WIN_CLOSING: i32 = 1;
pub const EE_WON_WIN_CLOSED: i32 = 2;

const EV_SIZE_CHANGE: i32 = 1;
const EV_MOUSE_MOVE: i32 = 2;
const EV_MOUSE_DOWN: i32 = 3;
const EV_MOUSE_DOUBLE_CLICK: i32 = 4;
const EV_MOUSE_DOWN_CHECK_DOUBLE: i32 = 5;
const EV_MOUSE_UP: i32 = 6;
const EV_MOUSE_WHEEL: i32 = 7;
const EV_KEY_DOWN: i32 = 8;
const EV_KEY_UP: i32 = 9;
const EV_KEY_CHAR: i32 = 10;
const EV_GOT_FOCUS: i32 = 11;
const EV_LOST_FOCUS: i32 = 12;
const EV_MOUSE_EXIT: i32 = 13;

/// An input event's fields, as the C++ forwarder reads them (`KeyEvent`, `MouseEvent`, `SizeEvent`, `InputEvent`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonWinEvent {
    /// the `WindowEventType`
    pub kind: i32,
    pub x: i32,
    pub y: i32,
    pub key_mod: i32,
    /// a mouse event's button or wheel amount
    pub button: i32,
    /// a key event's key
    pub key: i32,
    /// a size event's
    pub width: i32,
    pub height: i32,
}

/// The root container's methods the window calls (`WinRoot`'s `a`; `b`, `c`, `d` its arguments).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWinRoot {
    /// `SetSize(b, c)`.
    SetSize = 1,
    Invalidate,
    MouseExit,
    KeyDown,
    KeyUp,
    KeyChar,
    /// `MouseDown(b, c, d)`.
    MouseDown,
    MouseEnter,
    MouseDrag,
    MouseMove,
    MouseUp,
    MouseWheel,
    GotFocus,
    LostFocus,
    MouseHover,
    TimerEvent,
    /// `CanClose()` (0 or 1).
    CanClose,
    /// `AddChild((Component*)b)`.
    AddChild,
    /// `RemoveChild((Component*)b)`.
    RemoveChild,
    EnterModal,
    LeaveModal,
    /// `UnionInvalidRect(GetBounds())` (the root's own bounds).
    UnionBounds,
    /// `GetCurrentCursor()` (a `Cursor*`; a pure read).
    CurrentCursor,
    /// the bounds' reads (pure)
    Left,
    Top,
    Width,
    Height,
    /// `mTimerFlags` (a pure read).
    TimerFlags,
}

/// The window's own platform virtuals (`WinPlat`'s `a`; `b` the argument).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWinPlat {
    SetTitle = 1,
    SetIcon,
    /// `PlatformSetCursor((Cursor*)b)`.
    SetCursor,
    ShowNativeCursor,
    Show,
    Minimize,
    SetFocus,
}

/// The inward calls (`ee_won_win_call`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWinCall {
    CanClose = 1,
    Close,
    /// `CheckResize(a)` (a tick).
    CheckResize,
    /// `SetTitle`'s platform part (the C++ stored the title).
    TitleChanged,
    /// `SetIcon`'s platform part (the C++ stored the icon).
    IconChanged,
    /// `Center(a != 0)`.
    Center,
    SetClientSize,
    Show,
    HandleMouseExit,
    /// `HandleKeyEvent`, `HandleMouseDown`, `HandleMouseEvent`, `HandleInputEvent` with `a` an `EeWonWinEvent*`.
    HandleKeyEvent,
    HandleMouseDown,
    HandleMouseEvent,
    HandleInputEvent,
    /// `IdleUpdate()` (0 or 1).
    IdleUpdate,
    /// `TimerEvent(a)`.
    TimerEvent,
    /// `Create`'s statements after the icon (`a`, `b` the rectangle's width and height).
    Create,
    /// the base `PlatformClose`.
    PlatformClose,
    ShowNativeCursor,
    CheckSetCursor,
    InvalidateRoot,
    Minimize,
    SuspendRootResize,
    RemoveComponent,
    IsMouseDown,
    AddComponent,
    SetModal,
    CheckDoubleClick,
    /// `SetParent((Window*)a)`.
    SetParent,
    /// the child window set: `a` inserted (`b != 0`) or erased (the parent's half of `Create` / `PlatformClose`).
    ChildSet,
    /// the field `a` (`EeWonWinField`).
    GetField,
    /// the field `a` set to `b`.
    SetField,
}

/// The fields (the accessors, the goldens and the unit tests).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWinField {
    State = 1,
    MouseX,
    MouseY,
    KeyMod,
    MouseDown0,
    MouseDown1,
    MouseDown2,
    InClient,
    CurrentCursor,
    NativeCursorShowing,
    DownTick0,
    DownTick1,
    DownTick2,
    Double0,
    Double1,
    Double2,
    DetectDouble,
    DoubleX,
    DoubleY,
    LastMoveTick,
    MinWidth,
    MinHeight,
    MaxWidth,
    MaxHeight,
    NeedResize,
    NewWidth,
    NewHeight,
    ResizeDelay,
    LastResizeTick,
    LastCheckCursorTick,
    SuspendRootResize,
    Style,
    IsPopup,
    OneComponent,
    MinimizeOnRestore,
    Parent,
    IsShowing,
    /// the number of child windows
    ChildCount,
}

/// The last field.
pub const EE_WON_WIN_FIELD_COUNT: i32 = 38;

#[derive(Debug, Clone)]
struct State {
    window_state: i32,
    mouse_x: i32,
    mouse_y: i32,
    key_mod: i32,
    mouse_down: [bool; 3],
    in_client: bool,
    current_cursor: isize,
    native_cursor_showing: bool,
    down_tick: [u32; 3],
    double: [bool; 3],
    detect_double: bool,
    double_x: i32,
    double_y: i32,
    last_move_tick: u32,
    min_width: i32,
    min_height: i32,
    max_width: i32,
    max_height: i32,
    need_resize: bool,
    new_width: i32,
    new_height: i32,
    resize_delay: u32,
    last_resize_tick: u32,
    last_check_cursor_tick: u32,
    suspend_root_resize: i32,
    style: i32,
    is_popup: bool,
    one_component: bool,
    minimize_on_restore: bool,
    parent: isize,
    is_showing: bool,
    children: BTreeSet<isize>,
}

/// The Rust Window.
pub struct EeWonWindow {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
}

impl EeWonWindow {
    /// The constructor's state: closed, hidden, no parent, the mouse outside, the system cursor shown, sizes
    /// 0..64000.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonWindow {
            comp,
            host,
            st: RefCell::new(State {
                window_state: EE_WON_WIN_CLOSED,
                mouse_x: 0,
                mouse_y: 0,
                key_mod: 0,
                mouse_down: [false; 3],
                in_client: false,
                current_cursor: 0,
                native_cursor_showing: true,
                // the original left the ticks, the double-click flags and place unset; they start at 0
                down_tick: [0; 3],
                double: [false; 3],
                detect_double: false,
                double_x: 0,
                double_y: 0,
                last_move_tick: 0,
                min_width: 0,
                min_height: 0,
                max_width: 64000,
                max_height: 64000,
                need_resize: false,
                new_width: 0,
                new_height: 0,
                resize_delay: 0,
                last_resize_tick: 0,
                last_check_cursor_tick: 0,
                suspend_root_resize: 0,
                style: 0,
                is_popup: false,
                one_component: false,
                minimize_on_restore: false,
                parent: 0,
                is_showing: false,
                children: BTreeSet::new(),
            }),
        }
    }

    fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's window; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    fn root(&self, call: EeWonWinRoot, b: isize, c: isize, d: isize) -> isize {
        self.op(Op::WinRoot, call as isize, b, c, d)
    }
    fn plat(&self, call: EeWonWinPlat, b: isize) {
        self.op(Op::WinPlat, call as isize, b, 0, 0);
    }
    fn tick(&self) -> u32 {
        self.op(Op::WinTick, 0, 0, 0, 0) as u32
    }
    fn rect_of(&self, window: isize, client: bool) -> EeWonRect4 {
        let mut r = EeWonRect4::default();
        self.op(Op::WinGetRect, window, &mut r as *mut EeWonRect4 as isize, isize::from(client), 0);
        r
    }

    /// `CanClose`: every child window asked (all of them, even after a refusal), then the root, then the
    /// allow-close function.
    pub fn can_close(&self) -> bool {
        let children: Vec<isize> = self.st.borrow().children.iter().copied().collect();
        let mut children_can_close = true;
        for c in children {
            if self.op(Op::WinCanClose, c, 0, 0, 0) == 0 {
                children_can_close = false;
            }
        }
        if !children_can_close {
            return false;
        }
        if self.root(EeWonWinRoot::CanClose, 0, 0, 0) == 0 {
            return false;
        }
        self.op(Op::WinAllowClose, 0, 0, 0, 0) != 0
    }

    /// `Close`: an open window that may close becomes closing and queues the close event.
    pub fn close(&self) -> bool {
        let state = self.st.borrow().window_state;
        if guievent::close_step(state) == 0 {
            return true;
        }
        if !self.can_close() {
            return false;
        }
        self.st.borrow_mut().window_state = EE_WON_WIN_CLOSING;
        self.op(Op::WinQueueClose, 0, 0, 0, 0);
        true
    }

    /// `CheckResize`: a due resize makes the new offscreen image and sizes the root.
    pub fn check_resize(&self, tick: u32) {
        let (w, h) = {
            let s = self.st.borrow();
            if !guievent::resize_due(s.need_resize, tick, s.last_resize_tick, s.resize_delay, s.suspend_root_resize) {
                return;
            }
            (s.new_width, s.new_height)
        };
        self.op(Op::WinNewOffscreen, w as isize, h as isize, 0, 0);
        self.root(EeWonWinRoot::SetSize, w as isize, h as isize, 0);
        self.root(EeWonWinRoot::Invalidate, 0, 0, 0);
        self.st.borrow_mut().need_resize = false;
        let t = self.tick();
        self.st.borrow_mut().last_resize_tick = t;
    }

    fn if_open_plat(&self, call: EeWonWinPlat) {
        if self.st.borrow().window_state == EE_WON_WIN_OPEN {
            self.plat(call, 0);
        }
    }

    /// `Center`: on the screen, or on the visible parent when asked.
    pub fn center(&self, in_parent: bool) {
        let parent = self.st.borrow().parent;
        let visible = in_parent && parent != 0 && self.op(Op::WinIsVisible, parent, 0, 0, 0) != 0;
        let screen = if !guievent::center_on_parent(in_parent, parent != 0, visible) {
            let mut r = EeWonRect4::default();
            self.op(Op::WinScreenRect, &mut r as *mut EeWonRect4 as isize, 0, 0, 0);
            r
        } else {
            self.rect_of(parent, false)
        };
        let win = self.rect_of(self.comp as isize, false);
        let (left, top) = guievent::center([screen.x, screen.y, screen.w, screen.h], win.w, win.h);
        self.op(Op::WinSetPosSize, left as isize, top as isize, win.w as isize, win.h as isize);
    }

    pub fn set_client_size(&self, width: i32, height: i32) {
        let r = self.rect_of(self.comp as isize, false);
        self.op(Op::WinSetPosSize, r.x as isize, r.y as isize, width as isize, height as isize);
    }

    /// `Show`: hiding ends a mouse-over or a drag first.
    pub fn show(&self, show: bool) {
        let exit = {
            let mut s = self.st.borrow_mut();
            s.is_showing = show;
            !show && (s.in_client || s.mouse_down[0])
        };
        if exit {
            self.handle_mouse_exit();
        }
        self.plat(EeWonWinPlat::Show, isize::from(show));
    }

    pub fn handle_mouse_exit(&self) {
        self.show_native_cursor(true);
        self.st.borrow_mut().in_client = false;
        let left = self.root(EeWonWinRoot::Left, 0, 0, 0) as i32;
        let top = self.root(EeWonWinRoot::Top, 0, 0, 0) as i32;
        let width = self.root(EeWonWinRoot::Width, 0, 0, 0) as i32;
        let height = self.root(EeWonWinRoot::Height, 0, 0, 0) as i32;
        let (x, y) = guievent::exit_point(left, top, width, height);
        {
            let mut s = self.st.borrow_mut();
            s.mouse_x = x;
            s.mouse_y = y;
            s.mouse_down = [false; 3];
        }
        self.root(EeWonWinRoot::MouseExit, 0, 0, 0);
        self.st.borrow_mut().current_cursor = 0;
    }

    pub fn handle_key_event(&self, e: &EeWonWinEvent) {
        self.st.borrow_mut().key_mod = e.key_mod;
        let call = match e.kind {
            EV_KEY_DOWN => EeWonWinRoot::KeyDown,
            EV_KEY_UP => EeWonWinRoot::KeyUp,
            EV_KEY_CHAR => EeWonWinRoot::KeyChar,
            _ => return,
        };
        self.root(call, e.key as isize, 0, 0);
    }

    /// `HandleMouseDown`: the press recorded, the double click detected (the tick read only when asked for).
    pub fn handle_mouse_down(&self, e: &EeWonWinEvent) {
        let b = e.button as usize;
        let (detecting, prev) = {
            let mut s = self.st.borrow_mut();
            if let Some(m) = s.mouse_down.get_mut(b) {
                *m = true;
            }
            (s.detect_double, s.down_tick.get(b).copied().unwrap_or(0))
        };
        let now = if e.kind == EV_MOUSE_DOWN_CHECK_DOUBLE { self.tick() } else { 0 };
        let d = guievent::double_click(e.kind, detecting, prev, now);
        let (x, y) = {
            let mut s = self.st.borrow_mut();
            if d.start {
                s.down_tick = [0; 3];
                s.detect_double = true;
                s.double_x = e.x;
                s.double_y = e.y;
            }
            if let (Some(t), Some(slot)) = (d.tick, s.down_tick.get_mut(b)) {
                *slot = t;
            }
            if let Some(slot) = s.double.get_mut(b) {
                *slot = d.double;
            }
            (s.mouse_x, s.mouse_y)
        };
        self.root(EeWonWinRoot::MouseDown, x as isize, y as isize, b as isize);
    }

    pub fn handle_mouse_event(&self, e: &EeWonWinEvent) {
        let (x, y, enters) = {
            let mut s = self.st.borrow_mut();
            s.mouse_x = e.x;
            s.mouse_y = e.y;
            s.key_mod = e.key_mod;
            (e.x, e.y, guievent::mouse_enters(s.in_client, s.mouse_down[0]))
        };
        if enters {
            self.st.borrow_mut().in_client = true;
            self.root(EeWonWinRoot::MouseEnter, x as isize, y as isize, 0);
            self.check_set_cursor(true);
        }
        match e.kind {
            EV_MOUSE_MOVE => {
                let t = self.tick();
                let drag = {
                    let mut s = self.st.borrow_mut();
                    s.last_move_tick = t;
                    if guievent::move_ends_double(s.detect_double, s.mouse_x, s.mouse_y, s.double_x, s.double_y) {
                        s.detect_double = false;
                    }
                    s.mouse_down[0]
                };
                let (x, y) = self.mouse();
                let call = if drag { EeWonWinRoot::MouseDrag } else { EeWonWinRoot::MouseMove };
                self.root(call, x as isize, y as isize, 0);
            }
            EV_MOUSE_DOWN | EV_MOUSE_DOUBLE_CLICK | EV_MOUSE_DOWN_CHECK_DOUBLE => self.handle_mouse_down(e),
            EV_MOUSE_UP => {
                let b = e.button;
                let exits = {
                    let mut s = self.st.borrow_mut();
                    s.last_move_tick = 0;
                    if let Some(m) = s.mouse_down.get_mut(b as usize) {
                        *m = false;
                    }
                    guievent::release_exits(b, s.in_client)
                };
                if exits {
                    self.handle_mouse_exit();
                    return;
                }
                let (x, y) = self.mouse();
                self.root(EeWonWinRoot::MouseUp, x as isize, y as isize, b as isize);
            }
            EV_MOUSE_WHEEL => {
                self.root(EeWonWinRoot::MouseWheel, e.button as isize, 0, 0);
            }
            _ => {}
        }
    }

    fn mouse(&self) -> (i32, i32) {
        let s = self.st.borrow();
        (s.mouse_x, s.mouse_y)
    }

    pub fn handle_input_event(&self, e: &EeWonWinEvent) {
        match e.kind {
            EV_SIZE_CHANGE => {
                let rw = self.root(EeWonWinRoot::Width, 0, 0, 0) as i32;
                let rh = self.root(EeWonWinRoot::Height, 0, 0, 0) as i32;
                if guievent::size_resizes(e.width, e.height, rw, rh) {
                    {
                        let mut s = self.st.borrow_mut();
                        s.need_resize = true;
                        s.new_width = e.width;
                        s.new_height = e.height;
                    }
                    let t = self.tick();
                    self.check_resize(t);
                }
            }
            EV_GOT_FOCUS => {
                self.root(EeWonWinRoot::GotFocus, 0, 0, 0);
            }
            EV_LOST_FOCUS => {
                self.root(EeWonWinRoot::LostFocus, 0, 0, 0);
            }
            EV_MOUSE_EXIT => {
                let exit = {
                    let mut s = self.st.borrow_mut();
                    s.in_client = false;
                    !s.mouse_down[0]
                };
                if exit {
                    self.handle_mouse_exit();
                }
            }
            _ => {}
        }
    }

    /// `IdleUpdate`: the cursor refreshed (forced once a second while the mouse is inside); whether open.
    pub fn idle_update(&self) -> bool {
        let (open, in_client) = {
            let s = self.st.borrow();
            (s.window_state == EE_WON_WIN_OPEN, s.in_client)
        };
        if !open {
            return false;
        }
        let mut force = false;
        if in_client {
            let t = self.tick();
            let last = self.st.borrow().last_check_cursor_tick;
            if guievent::cursor_forced(true, t, last) {
                force = true;
                self.st.borrow_mut().last_check_cursor_tick = t;
            }
        }
        self.check_set_cursor(force);
        true
    }

    /// `TimerEvent`: the hover after 100 ms without motion, a due resize, the root's timers.
    pub fn timer_event(&self, elapsed: u32) {
        let (hover, last_move) = {
            let s = self.st.borrow();
            (
                s.in_client && s.last_move_tick != 0 && !s.mouse_down[0] && !s.mouse_down[1] && !s.mouse_down[2],
                s.last_move_tick,
            )
        };
        if hover {
            let t = self.tick();
            if let Some(e) = guievent::hover_elapsed(true, last_move, [false; 3], t) {
                self.root(EeWonWinRoot::MouseHover, e as isize, 0, 0);
            }
        }
        let t = self.tick();
        self.check_resize(t);
        if self.root(EeWonWinRoot::TimerFlags, 0, 0, 0) != 0 {
            self.root(EeWonWinRoot::TimerEvent, elapsed as isize, 0, 0);
        }
    }

    /// `Create` after the default icon: open, registered with the parent, the root sized, added to the manager.
    pub fn create(&self, width: i32, height: i32) {
        let parent = {
            let mut s = self.st.borrow_mut();
            s.window_state = EE_WON_WIN_OPEN;
            s.parent
        };
        if parent != 0 {
            self.op(Op::WinChildSet, parent, 1, 0, 0);
        }
        self.root(EeWonWinRoot::SetSize, width as isize, height as isize, 0);
        self.op(Op::WinAddToManager, 0, 0, 0, 0);
    }

    /// The base `PlatformClose`: the system cursor shown, closed, hidden, out of the parent's children.
    pub fn platform_close(&self) {
        self.show_native_cursor(true);
        let parent = {
            let mut s = self.st.borrow_mut();
            s.window_state = EE_WON_WIN_CLOSED;
            s.is_showing = false;
            s.parent
        };
        if parent != 0 {
            self.op(Op::WinChildSet, parent, 0, 0, 0);
        }
    }

    pub fn show_native_cursor(&self, show: bool) {
        self.st.borrow_mut().native_cursor_showing = show;
        self.plat(EeWonWinPlat::ShowNativeCursor, isize::from(show));
    }

    /// `CheckSetCursor`: the root's cursor (the arrow when none) applied when it changed or when forced; a cursor
    /// with no native form hides the system cursor.
    pub fn check_set_cursor(&self, force: bool) {
        let cur = self.root(EeWonWinRoot::CurrentCursor, 0, 0, 0);
        let same = cur == self.st.borrow().current_cursor;
        if !guievent::cursor_refresh(force, same) {
            return;
        }
        let cur = self.root(EeWonWinRoot::CurrentCursor, 0, 0, 0);
        self.st.borrow_mut().current_cursor = cur;
        let mut cursor = self.root(EeWonWinRoot::CurrentCursor, 0, 0, 0);
        if cursor == 0 {
            cursor = self.op(Op::WinStdArrow, 0, 0, 0, 0);
        }
        let native = self.op(Op::WinCursorNative, cursor, 0, 0, 0);
        if native != 0 {
            self.show_native_cursor(true);
            self.plat(EeWonWinPlat::SetCursor, native);
        } else {
            self.show_native_cursor(false);
        }
    }

    pub fn invalidate_root(&self) {
        self.root(EeWonWinRoot::Invalidate, 0, 0, 0);
        self.root(EeWonWinRoot::UnionBounds, 0, 0, 0);
    }

    pub fn suspend_root_resize(&self, suspend: bool) {
        let check = {
            let mut s = self.st.borrow_mut();
            if suspend {
                s.suspend_root_resize = s.suspend_root_resize.wrapping_add(1);
                false
            } else {
                s.suspend_root_resize = s.suspend_root_resize.wrapping_sub(1);
                if s.suspend_root_resize <= 0 {
                    s.suspend_root_resize = 0;
                    true
                } else {
                    false
                }
            }
        };
        if check {
            let t = self.tick();
            self.check_resize(t);
        }
    }

    fn child_set(&self, child: isize, insert: bool) {
        let mut s = self.st.borrow_mut();
        if insert {
            s.children.insert(child);
        } else {
            s.children.remove(&child);
        }
    }

    pub fn field(&self, f: i32) -> isize {
        use EeWonWinField as F;
        let s = self.st.borrow();
        let b = isize::from;
        if !(F::State as i32..=F::ChildCount as i32).contains(&f) {
            return 0;
        }
        // SAFETY: in the enum's range (checked above); the enum is repr(u32) with consecutive values
        let field: F = unsafe { std::mem::transmute::<u32, F>(f as u32) };
        match field {
            F::State => s.window_state as isize,
            F::MouseX => s.mouse_x as isize,
            F::MouseY => s.mouse_y as isize,
            F::KeyMod => s.key_mod as isize,
            F::MouseDown0 => b(s.mouse_down[0]),
            F::MouseDown1 => b(s.mouse_down[1]),
            F::MouseDown2 => b(s.mouse_down[2]),
            F::InClient => b(s.in_client),
            F::CurrentCursor => s.current_cursor,
            F::NativeCursorShowing => b(s.native_cursor_showing),
            F::DownTick0 => s.down_tick[0] as isize,
            F::DownTick1 => s.down_tick[1] as isize,
            F::DownTick2 => s.down_tick[2] as isize,
            F::Double0 => b(s.double[0]),
            F::Double1 => b(s.double[1]),
            F::Double2 => b(s.double[2]),
            F::DetectDouble => b(s.detect_double),
            F::DoubleX => s.double_x as isize,
            F::DoubleY => s.double_y as isize,
            F::LastMoveTick => s.last_move_tick as isize,
            F::MinWidth => s.min_width as isize,
            F::MinHeight => s.min_height as isize,
            F::MaxWidth => s.max_width as isize,
            F::MaxHeight => s.max_height as isize,
            F::NeedResize => b(s.need_resize),
            F::NewWidth => s.new_width as isize,
            F::NewHeight => s.new_height as isize,
            F::ResizeDelay => s.resize_delay as isize,
            F::LastResizeTick => s.last_resize_tick as isize,
            F::LastCheckCursorTick => s.last_check_cursor_tick as isize,
            F::SuspendRootResize => s.suspend_root_resize as isize,
            F::Style => s.style as isize,
            F::IsPopup => b(s.is_popup),
            F::OneComponent => b(s.one_component),
            F::MinimizeOnRestore => b(s.minimize_on_restore),
            F::Parent => s.parent,
            F::IsShowing => b(s.is_showing),
            F::ChildCount => s.children.len() as isize,
        }
    }

    /// The setters the C++ accessors use (`SetIsShowing`, `SetIsPopup`, `SetOneComponentWindow`, `SetStyle`, the
    /// minimum and maximum sizes, the resize delay, the restore's minimize); other fields are not set from outside.
    pub fn set_field(&self, f: i32, v: isize) {
        use EeWonWinField as F;
        let mut s = self.st.borrow_mut();
        let on = v != 0;
        match f {
            x if x == F::IsShowing as i32 => s.is_showing = on,
            x if x == F::IsPopup as i32 => s.is_popup = on,
            x if x == F::OneComponent as i32 => s.one_component = on,
            x if x == F::Style as i32 => s.style = v as i32,
            x if x == F::MinWidth as i32 => s.min_width = v as i32,
            x if x == F::MinHeight as i32 => s.min_height = v as i32,
            x if x == F::MaxWidth as i32 => s.max_width = v as i32,
            x if x == F::MaxHeight as i32 => s.max_height = v as i32,
            x if x == F::ResizeDelay as i32 => s.resize_delay = v as u32,
            x if x == F::MinimizeOnRestore as i32 => s.minimize_on_restore = on,
            _ => {}
        }
    }

    pub fn call(&self, call: EeWonWinCall, a: isize, b: isize) -> isize {
        type C = EeWonWinCall;
        let ev = || {
            // SAFETY: the C++ passes an EeWonWinEvent for the four handlers
            unsafe { *(a as *const EeWonWinEvent) }
        };
        match call {
            C::CanClose => return isize::from(self.can_close()),
            C::Close => return isize::from(self.close()),
            C::CheckResize => self.check_resize(a as u32),
            C::TitleChanged => self.if_open_plat(EeWonWinPlat::SetTitle),
            C::IconChanged => self.if_open_plat(EeWonWinPlat::SetIcon),
            C::Center => self.center(a != 0),
            C::SetClientSize => self.set_client_size(a as i32, b as i32),
            C::Show => self.show(a != 0),
            C::HandleMouseExit => self.handle_mouse_exit(),
            C::HandleKeyEvent => self.handle_key_event(&ev()),
            C::HandleMouseDown => self.handle_mouse_down(&ev()),
            C::HandleMouseEvent => self.handle_mouse_event(&ev()),
            C::HandleInputEvent => self.handle_input_event(&ev()),
            C::IdleUpdate => return isize::from(self.idle_update()),
            C::TimerEvent => self.timer_event(a as u32),
            C::Create => self.create(a as i32, b as i32),
            C::PlatformClose => self.platform_close(),
            C::ShowNativeCursor => self.show_native_cursor(a != 0),
            C::CheckSetCursor => self.check_set_cursor(a != 0),
            C::InvalidateRoot => self.invalidate_root(),
            C::Minimize => self.plat(EeWonWinPlat::Minimize, isize::from(a != 0)),
            C::SuspendRootResize => self.suspend_root_resize(a != 0),
            C::RemoveComponent => {
                self.root(EeWonWinRoot::RemoveChild, a, 0, 0);
            }
            C::IsMouseDown => {
                return isize::from(self.st.borrow().mouse_down.get(a as usize).copied().unwrap_or(false))
            }
            C::AddComponent => {
                self.root(EeWonWinRoot::AddChild, a, 0, 0);
            }
            C::SetModal => {
                let call = if a != 0 { EeWonWinRoot::EnterModal } else { EeWonWinRoot::LeaveModal };
                self.root(call, 0, 0, 0);
            }
            C::CheckDoubleClick => {
                return isize::from(self.st.borrow().double.get(a as usize).copied().unwrap_or(false))
            }
            C::SetParent => self.st.borrow_mut().parent = a,
            C::ChildSet => self.child_set(a, b != 0),
            C::GetField => return self.field(a as i32),
            C::SetField => self.set_field(a as i32, b),
        }
        0
    }
}

/// A new Rust Window for the C++ `window`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_win_new(window: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonWindow {
    Box::into_raw(Box::new(EeWonWindow::new(window, host)))
}

/// The window's object freed (the child set with it).
///
/// # Safety
/// `win` came from `ee_won_win_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_win_free(win: *mut EeWonWindow) {
    if !win.is_null() {
        // SAFETY: the contract
        unsafe { drop(Box::from_raw(win)) };
    }
}

/// One of Window's methods (`EeWonWinCall`).
///
/// # Safety
/// `win` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_win_call(win: *const EeWonWindow, call: EeWonWinCall, a: isize, b: isize) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*win).call(call, a, b) }
}

#[cfg(test)]
#[path = "winobj_tests.rs"]
mod tests;
