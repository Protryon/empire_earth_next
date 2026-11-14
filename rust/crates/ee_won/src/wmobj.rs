//! WONGUI's WindowManager as a Rust object (R6 WON round 33): the popup stack, the modal dialog and the nested
//! message loop, focus and the mouse-down window, and the font and image caches (`wmcache.rs`) (the original's
//! WindowManager.cpp).
//!
//! The C++ `WindowManager` stays the type the windows, the components, the platform subclass (`PlatWindowManager`)
//! and the lobby use: the moved methods are one-line forwarders to `ee_won_wm_call` and the accessors read
//! `ee_won_wm_field`. Every call out is an op on the manager's C++ object (`EeWonGuiOp::Wm*`): a component's methods
//! (`WmComp` with an `EeWonWmComp`), a window's (`WmWin` with an `EeWonWmWin`), the screen rectangle, the new popup
//! window, the platform's next event, the manager's own reference and the window set.
//!
//! The references the original's `SmartPtr`s held (the saved popup window, the popup's input and owner components,
//! and the locals of DoDialog, DoPopup and EndPopup) are held and given back through `Hold` / `Release` ops in the
//! original's order: an assignment releases the old object and then holds the new one, nothing when they are the
//! same; locals are given back in the reverse order of their making.

use std::cell::RefCell;
use std::collections::VecDeque;
use std::ffi::c_void;
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::Mutex;

use crate::guictl::EeWonRect4;
use crate::guievent;
use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guiwin;
use crate::wmcache::Caches;
use crate::wmloop::Loop;

/// `PopupFlag_*` (`WONLobbyGUI.h`).
const POPUP_WANT_INPUT: i32 = 0x0080;
const POPUP_ADJUST_POSITION: i32 = 0x0200;
const POPUP_EAT_MOUSE_DOWN_ON_END: i32 = 0x0800;
const POPUP_FIRE_EVENT_ON_END: i32 = 0x2000;
const POPUP_DONT_WANT_MOUSE: i32 = 0x4000;
/// `ComponentFlag_*` (`WONComponentFlags.h`) and the manager's own bits (0x8000 modal, 0x10000 a popup).
const FLAG_GRAB_BG: isize = 0x100;
const FLAG_NO_INPUT: isize = 0x800;
const FLAG_ALWAYS_ON_TOP: isize = 0x1000;
const FLAG_MODAL: isize = 0x8000;
const FLAG_POPUP: isize = 0x10000;
/// `Window::WindowState_Open`.
const WINDOW_OPEN: isize = 0;

/// A component's methods the manager calls (`WmComp`'s `a`; `b` the component, `c`, `d` the arguments).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWmComp {
    /// `SetComponentFlags(c, d != 0)`.
    SetFlags = 1,
    /// `mParent` (a pure read).
    Parent,
    /// `((Container*)b)->EnterModal()`.
    EnterModal,
    /// `((Container*)b)->LeaveModal()`.
    LeaveModal,
    RequestFocus,
    /// `SetPos(c, d)`.
    SetPos,
    /// the bounds' reads (pure)
    Left,
    Top,
    Width,
    Height,
    /// `GetRoot()` (a `Window*`; a pure read).
    Root,
    /// `GetOwnColorScheme()` (a pure read).
    OwnScheme,
    /// `GetColorScheme()` (a pure read).
    Scheme,
    /// `SetColorScheme((ColorScheme*)c)`.
    SetScheme,
    /// `Fn2F5380(ComponentEvent_EndPopup, 0)`.
    FireEndPopup,
    /// `mComponentBits` (a pure read).
    Bits,
    /// `GetRootContainer()` (a pure read).
    RootContainer,
    /// `((Container*)b)->FindClickChildAt(c, d)` (virtual).
    ClickChildAt,
    /// `dynamic_cast<Container*>(b)` as a `Component*` (0 when not a container; a pure read).
    AsContainer,
    /// `AddRef()` (a `SmartPtr<Component>`).
    Hold,
    /// `Release()`.
    Release,
    /// `MouseWheel(c)` (virtual; DeliverEvent's wheel to the topmost popup).
    MouseWheel,
    /// `HandleComponentEvent((ComponentEvent*)c)` (virtual; a broadcast).
    HandleComponentEvent,
}

/// A window's methods the manager calls (`WmWin`'s `a`; `b` the window, `c`, `d` the arguments).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWmWin {
    /// `PlatformSetFocus()`.
    SetFocus = 1,
    /// `PlatformSetMouseCapture(c != 0)`.
    SetCapture,
    /// `GetRect(*(WONRectangle*)c, d != 0)` (virtual).
    GetRect,
    /// `IsPopup()` (a pure read).
    IsPopup,
    /// `SetIsPopup(c != 0)`.
    SetIsPopup,
    /// `Show(c != 0)` (virtual).
    Show,
    /// `Close()`.
    Close,
    /// `SetPosSize` of `*(WONRectangle*)c` (virtual).
    SetPosSize,
    /// `GetParent()` (a pure read).
    Parent,
    /// `AddComponent((Component*)c)`.
    AddComponent,
    /// `SetModal(c != 0)`.
    SetModal,
    /// `IsMouseDown(c)` (a pure read).
    IsMouseDown,
    /// `GetMouseX()` / `GetMouseY()` (pure reads).
    MouseX,
    MouseY,
    /// `mRoot.get()` as a `Component*` (a pure read).
    Root,
    /// `mRoot->LostFocus()`.
    RootLostFocus,
    /// `mRoot->GotFocus()`.
    RootGotFocus,
    /// `mRoot->RemoveAllChildren()`.
    RootRemoveAll,
    /// `mRoot->RemoveChild((Component*)c)` (virtual).
    RootRemoveChild,
    /// `GetWindowState()` (a pure read).
    State,
    /// `PlatformIsVisible()` (virtual; 0 or 1).
    IsVisible,
    /// `AddRef()` (a `SmartPtr<Window>`).
    Hold,
    /// `Release()`.
    Release,
    /// `GetMinimizeOnResolutionRestore()` (a pure read).
    MinimizeOnRestore,
    /// `Minimize(c != 0)`.
    Minimize,
    /// `IsMouseInClientArea()` (a pure read).
    InClient,
    /// `HandleMouseEvent((MouseEvent*)c)`.
    HandleMouse,
    /// `HandleKeyEvent((KeyEvent*)c)`.
    HandleKey,
    /// `HandleInputEvent((InputEvent*)c)`.
    HandleInput,
    /// `PlatformClose()` (virtual).
    PlatformClose,
    /// `mCloseEvent.get()` (a raw `WindowEventBase*`; a pure read).
    CloseEvent,
    /// `TimerEvent(c)` (virtual; the elapsed ticks).
    TimerEvent,
    /// `IdleUpdate()` (virtual).
    IdleUpdate,
}

/// A window event's fields and methods (`WmEvent`'s `a`; `b` the `WindowEventBase*`, `c`, `d` the arguments).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWmEvent {
    /// `mEventType` (a pure read).
    Type = 1,
    /// `mWindow` (a pure read).
    Window,
    /// `mWindow = (Window*)c`.
    SetWindow,
    /// a `MouseEvent`'s `mWindow`, `mx`, `my` and `mButtonOrAmount` into `*(EeWonWmMouse*)c` (a pure read).
    Mouse,
    /// a `MouseEvent`'s `mWindow`, `mx` and `my` from `*(EeWonWmMouse*)c`.
    SetMouse,
    /// a `MouseEvent`'s `mButtonOrAmount` (a pure read).
    Button,
    /// a `KeyEvent`'s `mKey` (a pure read).
    Key,
    /// a `SizeEvent`'s `mx`, `my`, `mWidth`, `mHeight` into `*(EeWonRect4*)c` (a pure read).
    Size,
    /// a `SizeEvent`'s fields from `*(EeWonRect4*)c`.
    SetSize,
    /// `Deliver()` (virtual).
    Deliver,
    /// `AddRef()` (a `SmartPtr<WindowEventBase>`).
    Hold,
    /// `Release()`.
    Release,
}

/// The inward calls (`ee_won_wm_call`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWmCall {
    NotifyLostFocus = 1,
    /// `NotifyGotFocus((Window*)a)`.
    NotifyGotFocus,
    /// `GetPopupWindow()` (a `Window*`).
    PopupWindow,
    CalcModality,
    /// `NotModalWindow((Window*)a)` (0 or 1).
    NotModalWindow,
    /// `DoDialog((Window*)a, (Component*)b)` (the result).
    DoDialog,
    /// `EndDialog(a, (Window*)b, (Component*)c)`.
    EndDialog,
    ExitMessageLoop,
    /// `MessageLoop()` (the result).
    MessageLoop,
    /// `AdjustPopupPosition((Window*)a, (Component*)b, c != 0, *(EeWonRect4*)d)`.
    AdjustPopupPosition,
    /// `DoPopup` of `*(EeWonWmPopup*)a`.
    DoPopup,
    /// `EndPopup((Component*)a)`.
    EndPopup,
    /// `CheckEndPopup((Component*)a)`.
    CheckEndPopup,
    /// `FindMouseWindow` over `*(EeWonWmMouse*)a` (changed in place).
    FindMouseWindow,
    /// `CheckPopupClick` over `*(EeWonWmMouse*)a` (changed in place; 0 or 1).
    CheckPopupClick,
    /// `CheckModalValid()` (0 or 1).
    CheckModalValid,
    /// the field `a` (`EeWonWmField`).
    GetField,
    /// the field `a` set to `b`.
    SetField,
    /// `SetNamedFont` of the name `a` (`b` bytes) to the font `c`.
    SetNamedFont,
    /// `GetNamedFont` of the name `a` (`b` bytes; a raw `Font*`).
    GetNamedFont,
    /// `GetDefaultFont()` (a raw `Font*`).
    GetDefaultFont,
    /// `SetDefaultFont((Font*)a)`.
    SetDefaultFont,
    /// `SetCacheImages(a != 0)`.
    SetCacheImages,
    /// `DecodeImage((DisplayContext*)a, path b of c bytes)` (a `NativeImage*` with the caller's reference).
    DecodeImage,
    /// `DecodeImageRaw(path a of b bytes)` (a `RawImage*` with the caller's reference).
    DecodeImageRaw,
    /// `DecodeDelayImage(path a of b bytes)` (an `Image*` with the caller's reference).
    DecodeDelayImage,
    // ---- R6 WON round 34 (`wmloop`)
    /// `SetScreenResolution(a, b)`.
    SetScreenResolution,
    RestoreScreenResolution,
    CloseAllWindows,
    /// `AddWindow((Window*)a)` (always 1).
    AddWindow,
    /// `RemoveWindow((Window*)a)` (0 or 1).
    RemoveWindow,
    /// `PostEvent((WindowEventBase*)a, b != 0)` (any thread).
    PostEvent,
    /// `AddTimerEvent((WindowEventBase*)a)`.
    AddTimerEvent,
    /// `RemoveTimerEvent((WindowEventBase*)a)`.
    RemoveTimerEvent,
    RemoveAllTimerEvents,
    /// `SubscribeToBroadcast((Component*)a, b != 0)`.
    SubscribeToBroadcast,
    ClearSubscriptions,
    /// `BroadcastEvent((ComponentEvent*)a)`.
    BroadcastEvent,
    /// `PeekNextEvent()` (a `WindowEventBase*`, or 0; any thread).
    PeekNextEvent,
    ProcessEvents,
    /// `DeliverEvent((WindowEventBase*)a)`.
    DeliverEvent,
}

/// The fields (the accessors, the goldens and the unit tests).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonWmField {
    /// `mEndPopup`
    EndPopup = 1,
    InDoPopup,
    InEndPopup,
    PopupIsUp,
    /// `mBA5`: popups in their own window by default
    OwnWindow,
    PopupFlags,
    /// `mDoMouseCapturePopupSupport`
    CaptureSupport,
    /// `mCurMsgLoopDepth`
    CurDepth,
    /// `mTargetMsgLoopDepth`
    TargetDepth,
    DialogResult,
    ModalWindow,
    ModalComponent,
    /// `mPopupWindowSave` (held; set with the `SmartPtr`'s assignment)
    PopupWindowSave,
    PopupOwnerWindow,
    /// `mPopupInputComponent` (held)
    PopupInput,
    /// `mPopupOwnerComponent` (held)
    PopupOwner,
    FocusWindow,
    MouseDownWindow,
    LastLoseFocusTick,
    /// the popup list's length, last and first entries (0 when empty)
    PopupCount,
    PopupBack,
    PopupFront,
    // ---- R6 WON round 34 (`wmloop`)
    /// `mDefaultWindow` (the window set's first, or 0)
    DefaultWindow,
    /// `mWindowCount`
    WindowCount,
    /// `mIsFullScreen` (settable)
    FullScreen,
    /// `mFullScreenWidth`, `mFullScreenHeight` (settable)
    FullScreenWidth,
    FullScreenHeight,
    /// `mOriginalScreenWidth`, `mOriginalScreenHeight`
    OriginalWidth,
    OriginalHeight,
    /// `mLastTimerTick` (settable: the constructor's tick)
    LastTimerTick,
    LastInputTick,
    CallingTimers,
    /// the sizes of the window set, the event list, the timer events, the deferred timer removals and the
    /// broadcast set (their entries: `ee_won_wm_set_at`)
    WindowSetCount,
    EventCount,
    TimerCount,
    RemoveTimerCount,
    BroadcastCount,
}

/// The last field (`EeWonWmField::BroadcastCount`; a number, as a cbindgen constant from an enum does not compile).
pub const EE_WON_WM_FIELD_COUNT: i32 = 37;
/// The last of round 33's fields (`EeWonWmField::PopupFront`; the goldens' first state line).
pub const EE_WON_WM_FIELD_POPUP_LAST: i32 = 22;

/// A mouse event's fields FindMouseWindow and CheckPopupClick read and change (`MouseEvent`'s `mWindow`, `mx`,
/// `my`, `mButtonOrAmount`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonWmMouse {
    pub window: isize,
    pub x: i32,
    pub y: i32,
    pub button: i32,
}

/// `PopupParams`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonWmPopup {
    pub parent: isize,
    pub popup: isize,
    pub input: isize,
    pub owner: isize,
    pub flags: i32,
    pub x: i32,
    pub y: i32,
    pub avoid: EeWonRect4,
}

#[derive(Debug, Clone, Default)]
struct State {
    end_popup: bool,
    in_do_popup: bool,
    in_end_popup: bool,
    popup_is_up: bool,
    own_window: bool,
    popup_flags: i32,
    capture_support: bool,
    target_depth: i32,
    dialog_result: i32,
    modal_window: isize,
    modal_component: isize,
    popup_window_save: isize,
    popup_owner_window: isize,
    popup_input: isize,
    popup_owner: isize,
    popups: Vec<isize>,
    focus_window: isize,
    mouse_down_window: isize,
    last_lose_focus_tick: u32,
}

/// Which held member an assignment changes.
#[derive(Clone, Copy)]
enum Held {
    WindowSave,
    Input,
    Owner,
}

/// The Rust WindowManager.
///
/// Every method runs on the GUI thread but PostEvent and PeekNextEvent, which other threads call (the original
/// locked `mDataCrit` there): they touch only `events` (a `Mutex`, never held across a call out) and
/// `cur_depth` (an atomic: the message loop's depth PostEvent reads), and make the event's `Hold` and the
/// `Signal` ops, which the C++ answers on any thread.
pub struct EeWonWm {
    comp: *mut c_void,
    host: EeWonGuiHostFn,
    st: RefCell<State>,
    pub(crate) caches: RefCell<Caches>,
    /// the window set, the timers, the broadcasts and the screen state (`wmloop.rs`)
    pub(crate) lp: RefCell<Loop>,
    /// `mWindowEventList`: the queued events, each held
    pub(crate) events: Mutex<VecDeque<isize>>,
    /// `mCurMsgLoopDepth`
    pub(crate) cur_depth: AtomicI32,
}

impl EeWonWm {
    /// The constructor's state: no popup, no dialog, no focus, popups in their own window.
    pub fn new(comp: *mut c_void, host: EeWonGuiHostFn) -> Self {
        EeWonWm {
            comp,
            host,
            st: RefCell::new(State { own_window: true, ..State::default() }),
            caches: RefCell::new(Caches::default()),
            lp: RefCell::new(Loop::default()),
            events: Mutex::new(VecDeque::new()),
            cur_depth: AtomicI32::new(0),
        }
    }

    pub(crate) fn op(&self, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
        match self.host {
            // SAFETY: the host was given with this object's manager; no borrow of the state is live here
            Some(h) => unsafe { h(self.comp, op, a, b, c, d) },
            None => 0,
        }
    }
    pub(crate) fn cm(&self, call: EeWonWmComp, comp: isize, c: isize, d: isize) -> isize {
        self.op(Op::WmComp, call as isize, comp, c, d)
    }
    pub(crate) fn wn(&self, call: EeWonWmWin, win: isize, c: isize, d: isize) -> isize {
        self.op(Op::WmWin, call as isize, win, c, d)
    }
    fn win_rect(&self, win: isize, client: bool) -> EeWonRect4 {
        let mut r = EeWonRect4::default();
        self.wn(EeWonWmWin::GetRect, win, &mut r as *mut EeWonRect4 as isize, isize::from(client));
        r
    }
    /// A `SmartPtr<Component>` local made from a raw pointer.
    pub(crate) fn hold_comp(&self, c: isize) -> isize {
        if c != 0 {
            self.cm(EeWonWmComp::Hold, c, 0, 0);
        }
        c
    }
    pub(crate) fn release_comp(&self, c: isize) {
        if c != 0 {
            self.cm(EeWonWmComp::Release, c, 0, 0);
        }
    }
    pub(crate) fn hold_win(&self, w: isize) -> isize {
        if w != 0 {
            self.wn(EeWonWmWin::Hold, w, 0, 0);
        }
        w
    }
    pub(crate) fn release_win(&self, w: isize) {
        if w != 0 {
            self.wn(EeWonWmWin::Release, w, 0, 0);
        }
    }

    /// A held member's `SmartPtr` assignment: the old object released, then the new one held (nothing when they
    /// are the same).
    fn assign(&self, which: Held, new: isize) {
        let old = {
            let s = self.st.borrow();
            match which {
                Held::WindowSave => s.popup_window_save,
                Held::Input => s.popup_input,
                Held::Owner => s.popup_owner,
            }
        };
        if old == new {
            return;
        }
        let is_win = matches!(which, Held::WindowSave);
        if old != 0 {
            if is_win {
                self.release_win(old);
            } else {
                self.release_comp(old);
            }
        }
        {
            let mut s = self.st.borrow_mut();
            match which {
                Held::WindowSave => s.popup_window_save = new,
                Held::Input => s.popup_input = new,
                Held::Owner => s.popup_owner = new,
            }
        }
        if new != 0 {
            if is_win {
                self.hold_win(new);
            } else {
                self.hold_comp(new);
            }
        }
    }

    /// `NotifyLostFocus`: no focus window; the tick focus was lost at (0 stored as 1).
    pub fn notify_lost_focus(&self) {
        self.st.borrow_mut().focus_window = 0;
        let t = self.op(Op::WmTick, 0, 0, 0, 0) as u32;
        self.st.borrow_mut().last_lose_focus_tick = if t == 0 { 1 } else { t };
    }

    pub fn notify_got_focus(&self, win: isize) {
        let mut s = self.st.borrow_mut();
        s.focus_window = win;
        s.last_lose_focus_tick = 0;
    }

    /// `GetPopupWindow`: the first popup's window while popups are up. The original read the front of an empty
    /// list when a popup's end event asked mid-EndPopup; that answers no window.
    pub fn popup_window(&self) -> isize {
        let front = {
            let s = self.st.borrow();
            if !s.popup_is_up {
                return 0;
            }
            s.popups.first().copied()
        };
        match front {
            Some(p) => self.cm(EeWonWmComp::Root, p, 0, 0),
            None => 0,
        }
    }

    /// The static `SetModal(Component*, bool)`: the component's modal flag, then every ancestor's flag and its
    /// Enter/LeaveModal.
    fn set_modal(&self, comp: isize, modal: bool) {
        self.cm(EeWonWmComp::SetFlags, comp, FLAG_MODAL, isize::from(modal));
        let mut parent = self.cm(EeWonWmComp::Parent, comp, 0, 0);
        while parent != 0 {
            self.cm(EeWonWmComp::SetFlags, parent, FLAG_MODAL, isize::from(modal));
            let call = if modal { EeWonWmComp::EnterModal } else { EeWonWmComp::LeaveModal };
            self.cm(call, parent, 0, 0);
            parent = self.cm(EeWonWmComp::Parent, parent, 0, 0);
        }
    }

    /// `CalcModality`: the popups' and the dialog's flags and every window's modal state, from the popup list and
    /// the modal dialog.
    pub fn calc_modality(&self) {
        let input = self.st.borrow().popup_input;
        if input != 0 {
            self.set_modal(input, false);
        }
        let dialog = self.st.borrow().modal_component;
        if dialog != 0 {
            self.set_modal(dialog, false);
        }
        let any = {
            let mut s = self.st.borrow_mut();
            s.popup_is_up = !s.popups.is_empty();
            s.popup_is_up
        };
        if any {
            // the list read again at each step, as the original's iterator walked the live list
            let mut i = 0;
            loop {
                let p = match self.st.borrow().popups.get(i) {
                    Some(&p) => p,
                    None => break,
                };
                self.cm(EeWonWmComp::SetFlags, p, FLAG_MODAL | FLAG_POPUP, 1);
                i += 1;
            }
        }
        let (up, flags, dialog) = {
            let s = self.st.borrow();
            (s.popup_is_up, s.popup_flags, s.modal_component)
        };
        let modality = guiwin::modality(up, flags, dialog != 0);
        let modal = modality & 1 != 0;
        if modality & 2 != 0 {
            self.set_modal(dialog, true);
        }
        let (up, owner) = {
            let s = self.st.borrow();
            (s.popup_is_up, s.popup_owner)
        };
        if !up && owner != 0 {
            self.assign(Held::Owner, 0);
        }
        let input = self.st.borrow().popup_input;
        if input != 0 {
            if self.st.borrow().popup_is_up {
                self.set_modal(input, true);
            } else {
                self.assign(Held::Input, 0);
            }
        }
        // the window set walked as the original's iterator: the window after the last one visited
        let mut at = None;
        while let Some(w) = self.next_window(at) {
            at = Some(w);
            let (modal_window, up, dialog) = {
                let s = self.st.borrow();
                (s.modal_window, s.popup_is_up, s.modal_component)
            };
            let root = self.wn(EeWonWmWin::Root, w, 0, 0);
            let on = guiwin::window_modal(w == modal_window, up, dialog == root, modal);
            self.wn(EeWonWmWin::SetModal, w, isize::from(on), 0);
        }
    }

    pub fn not_modal_window(&self, win: isize) -> bool {
        let s = self.st.borrow();
        s.modal_window != 0 && win != s.modal_window
    }

    /// `DoDialog`: the component (the window's root by default) made the modal dialog until its message loop
    /// ends; the previous dialog restored after; the result.
    pub fn do_dialog(&self, window: isize, component: isize) -> i32 {
        let (old_dialog, old_window) = {
            let s = self.st.borrow();
            (s.modal_component, s.modal_window)
        };
        self.hold_comp(old_dialog);
        self.hold_win(old_window);
        self.st.borrow_mut().dialog_result = 0;
        if old_dialog != 0 {
            self.cm(EeWonWmComp::SetFlags, old_dialog, FLAG_MODAL, 0);
        }
        let component = if component == 0 { self.wn(EeWonWmWin::Root, window, 0, 0) } else { component };
        let dialog = self.st.borrow().modal_component;
        if dialog != 0 {
            self.set_modal(dialog, false);
        }
        {
            let mut s = self.st.borrow_mut();
            s.modal_window = window;
            s.modal_component = component;
        }
        self.calc_modality();
        let modal_window = self.st.borrow().modal_window;
        self.wn(EeWonWmWin::SetFocus, modal_window, 0, 0);
        self.cm(EeWonWmComp::RequestFocus, component, 0, 0);
        self.message_loop();
        let dialog = self.st.borrow().modal_component;
        self.set_modal(dialog, false);
        {
            let mut s = self.st.borrow_mut();
            s.modal_window = old_window;
            s.modal_component = old_dialog;
        }
        self.calc_modality();
        let result = self.st.borrow().dialog_result;
        self.release_win(old_window);
        self.release_comp(old_dialog);
        result
    }

    /// `EndDialog`: the current dialog's result, its loop ended, unless another window or dialog is named.
    pub fn end_dialog(&self, result: i32, window: isize, dialog: isize) {
        let mut s = self.st.borrow_mut();
        if !guiwin::ends_dialog(window != 0, window == s.modal_window, dialog != 0, dialog == s.modal_component) {
            return;
        }
        s.dialog_result = result;
        s.target_depth = s.target_depth.wrapping_sub(1);
    }

    pub fn exit_message_loop(&self) {
        let mut s = self.st.borrow_mut();
        s.target_depth = s.target_depth.wrapping_sub(1);
    }

    /// `MessageLoop`: the platform's events until the loop is exited; the dialog result, or -1 without windows.
    /// The result is read before the manager's reference is given back (the last one deletes the manager).
    pub fn message_loop(&self) -> i32 {
        if self.lp.borrow().windows.is_empty() {
            return -1;
        }
        self.op(Op::WmSelfHold, 0, 0, 0, 0);
        {
            let depth = self.cur_depth.load(Ordering::SeqCst).wrapping_add(1);
            self.cur_depth.store(depth, Ordering::SeqCst);
            self.st.borrow_mut().target_depth = depth;
        }
        while self.cur_depth.load(Ordering::SeqCst) <= self.st.borrow().target_depth {
            self.op(Op::WmNextEvent, 0, 0, 0, 0);
        }
        self.cur_depth.store(self.cur_depth.load(Ordering::SeqCst).wrapping_sub(1), Ordering::SeqCst);
        let result = self.st.borrow().dialog_result;
        self.op(Op::WmSelfRelease, 0, 0, 0, 0);
        result
    }

    /// `AdjustPopupPosition`: the popup moved back inside the screen (its own window) or the parent's root,
    /// flipped across the avoid rectangle.
    pub fn adjust_popup_position(&self, parent: isize, popup: isize, in_own_window: bool, avoid: EeWonRect4) {
        let bounds = if in_own_window {
            let mut b = EeWonRect4::default();
            self.op(Op::WmScreenRect, &mut b as *mut EeWonRect4 as isize, 0, 0, 0);
            let w = self.win_rect(parent, true);
            b.x = b.x.wrapping_sub(w.x);
            b.y = b.y.wrapping_sub(w.y);
            b
        } else {
            let root = self.wn(EeWonWmWin::Root, parent, 0, 0);
            EeWonRect4 {
                x: self.cm(EeWonWmComp::Left, root, 0, 0) as i32,
                y: self.cm(EeWonWmComp::Top, root, 0, 0) as i32,
                w: self.cm(EeWonWmComp::Width, root, 0, 0) as i32,
                h: self.cm(EeWonWmComp::Height, root, 0, 0) as i32,
            }
        };
        let place = EeWonRect4 {
            x: self.cm(EeWonWmComp::Left, popup, 0, 0) as i32,
            y: self.cm(EeWonWmComp::Top, popup, 0, 0) as i32,
            w: self.cm(EeWonWmComp::Width, popup, 0, 0) as i32,
            h: self.cm(EeWonWmComp::Height, popup, 0, 0) as i32,
        };
        for (x, y) in guiwin::popup_adjust(place, bounds, avoid) {
            self.cm(EeWonWmComp::SetPos, popup, x as isize, y as isize);
        }
    }

    /// `DoPopup`: the popup opened at the given or the mouse's place, in a popup window of its own or in the
    /// parent window; ignored while another popup opens or closes, or when the parent lacks the focus.
    pub fn do_popup(&self, p: &EeWonWmPopup) {
        let parent = self.hold_win(p.parent);
        let popup = self.hold_comp(p.popup);
        self.do_popup_body(p, parent, popup);
        self.release_comp(popup);
        self.release_win(parent);
    }

    fn do_popup_body(&self, p: &EeWonWmPopup, parent: isize, popup: isize) {
        let additional = {
            let s = self.st.borrow();
            if s.in_do_popup || s.in_end_popup {
                return;
            }
            let additional = guiwin::popup_additional(p.flags, !s.popups.is_empty());
            let focus_check = if additional { s.popup_owner_window } else { parent };
            if s.focus_window != focus_check {
                return;
            }
            additional
        };
        let up = {
            let mut s = self.st.borrow_mut();
            s.in_do_popup = true;
            s.popup_is_up
        };
        if up && !additional {
            self.end_popup(0);
        }
        let mouse_x = self.wn(EeWonWmWin::MouseX, parent, 0, 0) as i32;
        let mouse_y = self.wn(EeWonWmWin::MouseY, parent, 0, 0) as i32;
        let (x, y) = guiwin::popup_place(p.x, p.y, mouse_x, mouse_y);
        self.cm(EeWonWmComp::SetPos, popup, x as isize, y as isize);
        self.st.borrow_mut().end_popup = false;
        if !additional {
            {
                let mut s = self.st.borrow_mut();
                s.popup_flags = p.flags;
                s.popup_owner_window = parent;
            }
            self.assign(Held::Input, p.input);
        }
        let (in_own_window, flags) = {
            let mut s = self.st.borrow_mut();
            s.popups.push(popup);
            (guiwin::popup_own_window(s.popup_flags, s.own_window), s.popup_flags)
        };
        if flags & POPUP_ADJUST_POSITION != 0 {
            self.adjust_popup_position(parent, popup, in_own_window, p.avoid);
        }
        let mut popup_window = 0;
        if in_own_window {
            let mut r = self.win_rect(parent, true);
            r.x = r.x.wrapping_add(self.cm(EeWonWmComp::Left, popup, 0, 0) as i32);
            r.y = r.y.wrapping_add(self.cm(EeWonWmComp::Top, popup, 0, 0) as i32);
            r.w = self.cm(EeWonWmComp::Width, popup, 0, 0) as i32;
            r.h = self.cm(EeWonWmComp::Height, popup, 0, 0) as i32;
            let save = self.st.borrow().popup_window_save;
            if save != 0 {
                if self.wn(EeWonWmWin::Parent, save, 0, 0) != parent {
                    self.wn(EeWonWmWin::Close, save, 0, 0);
                } else {
                    self.wn(EeWonWmWin::SetPosSize, save, &mut r as *mut EeWonRect4 as isize, 0);
                    self.wn(EeWonWmWin::SetIsPopup, save, 1, 0);
                    self.wn(EeWonWmWin::Show, save, 1, 0);
                    popup_window = self.hold_win(save);
                }
                self.assign(Held::WindowSave, 0);
            }
            if popup_window == 0 {
                // the new window comes held once: the local's reference
                popup_window = self.op(Op::WmNewPopupWindow, parent, &mut r as *mut EeWonRect4 as isize, 0, 0);
            }
            self.wn(EeWonWmWin::SetIsPopup, popup_window, 1, 0);
            if self.st.borrow().popup_flags & POPUP_WANT_INPUT != 0 {
                let owner = self.st.borrow().popup_owner_window;
                self.wn(EeWonWmWin::SetFocus, owner, 0, 0);
                let owner = self.st.borrow().popup_owner_window;
                self.wn(EeWonWmWin::RootLostFocus, owner, 0, 0);
            }
            self.cm(EeWonWmComp::SetPos, popup, 0, 0);
        } else {
            popup_window = self.hold_win(parent);
            self.cm(EeWonWmComp::SetFlags, popup, FLAG_GRAB_BG, 1);
            if self.st.borrow().popup_flags & POPUP_DONT_WANT_MOUSE != 0 {
                self.cm(EeWonWmComp::SetFlags, popup, FLAG_NO_INPUT, 1);
            }
        }
        self.assign(Held::Owner, p.owner);
        if p.owner != 0 && self.cm(EeWonWmComp::OwnScheme, popup, 0, 0) == 0 {
            let scheme = self.cm(EeWonWmComp::Scheme, p.owner, 0, 0);
            self.cm(EeWonWmComp::SetScheme, popup, scheme, 0);
        }
        self.cm(EeWonWmComp::SetFlags, popup, FLAG_ALWAYS_ON_TOP, 1);
        self.wn(EeWonWmWin::AddComponent, popup_window, popup, 0);
        if self.st.borrow().popup_flags & POPUP_WANT_INPUT != 0 {
            self.cm(EeWonWmComp::RequestFocus, popup, 0, 0);
        }
        let (capture, owner) = {
            let s = self.st.borrow();
            (s.capture_support, s.popup_owner_window)
        };
        if capture {
            self.wn(EeWonWmWin::SetCapture, owner, 1, 0);
        }
        self.st.borrow_mut().in_do_popup = false;
        self.calc_modality();
        self.release_win(popup_window);
    }

    /// `EndPopup`: the given popup and every popup opened after it closed (NULL: all), the end event fired, the
    /// last popup window kept for reuse.
    pub fn end_popup(&self, popup: isize) {
        if self.st.borrow().popups.is_empty() {
            return;
        }
        if popup != 0 && self.cm(EeWonWmComp::Bits, popup, 0, 0) & FLAG_POPUP == 0 {
            return;
        }
        let was = {
            let mut s = self.st.borrow_mut();
            std::mem::replace(&mut s.in_end_popup, true)
        };
        loop {
            let back = match self.st.borrow().popups.last() {
                Some(&b) if b != popup => b,
                _ => break,
            };
            self.end_popup(back);
        }
        let (empty, up) = {
            let mut s = self.st.borrow_mut();
            if s.popups.is_empty() {
                (true, false)
            } else {
                s.end_popup = false;
                (false, s.popup_is_up)
            }
        };
        if empty {
            self.st.borrow_mut().in_end_popup = was;
            return;
        }
        if up {
            let comp = self.hold_comp(*self.st.borrow().popups.last().expect("not empty"));
            let window = self.hold_win(self.cm(EeWonWmComp::Root, comp, 0, 0));
            let owner_window = self.hold_win(self.st.borrow().popup_owner_window);
            let flags = {
                let mut s = self.st.borrow_mut();
                s.popups.pop();
                s.popup_flags
            };
            if flags & POPUP_FIRE_EVENT_ON_END != 0 {
                self.cm(EeWonWmComp::FireEndPopup, comp, 0, 0);
            }
            if self.st.borrow().popups.is_empty() {
                let (capture, owner) = {
                    let s = self.st.borrow();
                    (s.capture_support, s.popup_owner_window)
                };
                if capture {
                    self.wn(EeWonWmWin::SetCapture, owner, 0, 0);
                }
                let mut s = self.st.borrow_mut();
                s.popup_owner_window = 0;
                s.mouse_down_window = 0;
            }
            if window != owner_window {
                self.wn(EeWonWmWin::SetIsPopup, window, 0, 0);
                if self.st.borrow().popups.is_empty() {
                    self.assign(Held::WindowSave, window);
                }
                self.wn(EeWonWmWin::RootRemoveAll, window, 0, 0);
                if self.st.borrow().popup_window_save != 0 {
                    self.wn(EeWonWmWin::RootRemoveAll, window, 0, 0);
                    if !self.st.borrow().in_do_popup {
                        self.wn(EeWonWmWin::Show, window, 0, 0);
                    }
                } else {
                    self.wn(EeWonWmWin::Close, window, 0, 0);
                }
                let (empty, flags, focus) = {
                    let s = self.st.borrow();
                    (s.popups.is_empty(), s.popup_flags, s.focus_window)
                };
                if empty && flags & POPUP_WANT_INPUT != 0 && focus == owner_window {
                    self.wn(EeWonWmWin::RootGotFocus, owner_window, 0, 0);
                }
            } else {
                self.cm(EeWonWmComp::SetFlags, comp, FLAG_MODAL | FLAG_POPUP, 0);
                self.wn(EeWonWmWin::RootRemoveChild, window, comp, 0);
            }
            self.calc_modality();
            self.release_win(owner_window);
            self.release_win(window);
            self.release_comp(comp);
        }
        self.st.borrow_mut().in_end_popup = was;
    }

    /// `CheckEndPopup`: the popups closed when their owner component lies inside the given one.
    pub fn check_end_popup(&self, ancestor: isize) {
        let mut c = {
            let s = self.st.borrow();
            if !s.popup_is_up || s.popup_owner == 0 {
                return;
            }
            s.popup_owner
        };
        while c != 0 {
            if c == ancestor {
                self.end_popup(0);
                return;
            }
            c = self.cm(EeWonWmComp::Parent, c, 0, 0);
        }
    }

    /// `FindMouseWindow`: a mouse event while popups are up given to the popup's owner, the window the mouse went
    /// down in, or the topmost popup window under the mouse.
    pub fn find_mouse_window(&self, ev: &mut EeWonWmMouse) {
        let (flags, owner) = {
            let s = self.st.borrow();
            if !s.popup_is_up {
                return;
            }
            (s.popup_flags, s.popup_owner_window)
        };
        if flags & POPUP_DONT_WANT_MOUSE != 0 && self.wn(EeWonWmWin::IsPopup, ev.window, 0, 0) != 0 {
            let popup_rect = self.win_rect(ev.window, true);
            let mut owner_rect = self.win_rect(owner, true);
            owner_rect.x = owner_rect.x.wrapping_sub(popup_rect.x);
            owner_rect.y = owner_rect.y.wrapping_sub(popup_rect.y);
            if guiwin::rect_contains(owner_rect, ev.x, ev.y) {
                ev.window = self.st.borrow().popup_owner_window;
                ev.x = ev.x.wrapping_sub(owner_rect.x);
                ev.y = ev.y.wrapping_sub(owner_rect.y);
                return;
            }
        }
        if !self.st.borrow().capture_support {
            return;
        }
        let owner_rect = self.win_rect(ev.window, true);
        let down = self.st.borrow().mouse_down_window;
        if down != 0 && self.wn(EeWonWmWin::IsMouseDown, down, 0, 0) != 0 {
            let mut r = self.win_rect(down, true);
            r.x = r.x.wrapping_sub(owner_rect.x);
            r.y = r.y.wrapping_sub(owner_rect.y);
            ev.window = down;
            ev.x = ev.x.wrapping_sub(r.x);
            ev.y = ev.y.wrapping_sub(r.y);
            return;
        }
        let mut windows = Vec::new();
        let mut rects = Vec::new();
        let mut i = 0;
        loop {
            let p = match self.st.borrow().popups.get(i) {
                Some(&p) => p,
                None => break,
            };
            let w = self.cm(EeWonWmComp::Root, p, 0, 0);
            let mut r = self.win_rect(w, true);
            r.x = r.x.wrapping_sub(owner_rect.x);
            r.y = r.y.wrapping_sub(owner_rect.y);
            windows.push(w);
            rects.push(r);
            i += 1;
        }
        if let Some(hit) = guiwin::popup_window_at(&rects, ev.x, ev.y) {
            ev.window = windows[hit];
            ev.x = ev.x.wrapping_sub(rects[hit].x);
            ev.y = ev.y.wrapping_sub(rects[hit].y);
        }
    }

    /// `CheckPopupClick`: a press while popups are up; a press outside closes them; whether it is eaten.
    pub fn check_popup_click(&self, ev: &mut EeWonWmMouse) -> bool {
        self.find_mouse_window(ev);
        if ev.button == 0 {
            self.st.borrow_mut().mouse_down_window = ev.window;
        }
        let flags = self.st.borrow().popup_flags;
        if guiwin::popup_click_ends(flags, ev.button) {
            let (mut x, mut y) = (ev.x, ev.y);
            let mut container = self.wn(EeWonWmWin::Root, ev.window, 0, 0);
            let mut end = true;
            loop {
                x = x.wrapping_sub(self.cm(EeWonWmComp::Left, container, 0, 0) as i32);
                y = y.wrapping_sub(self.cm(EeWonWmComp::Top, container, 0, 0) as i32);
                let child = self.cm(EeWonWmComp::ClickChildAt, container, x as isize, y as isize);
                let child_container = if child != 0 { self.cm(EeWonWmComp::AsContainer, child, 0, 0) } else { 0 };
                let bits = if child != 0 { self.cm(EeWonWmComp::Bits, child, 0, 0) as u32 } else { 0 };
                let input = self.st.borrow().popup_input;
                let step =
                    guievent::popup_click_step(child != 0, child == input, bits, input != 0, child_container != 0);
                if step == 1 {
                    end = false;
                    break;
                } else if step == 0 {
                    break;
                }
                container = child_container;
            }
            if end {
                let eat = self.st.borrow().popup_flags & POPUP_EAT_MOUSE_DOWN_ON_END != 0;
                self.end_popup(0);
                if eat {
                    return true;
                }
            }
        }
        if self.wn(EeWonWmWin::IsPopup, ev.window, 0, 0) != 0 {
            let owner = self.st.borrow().popup_owner_window;
            self.wn(EeWonWmWin::SetFocus, owner, 0, 0);
        }
        false
    }

    /// `CheckModalValid`: the modal window open and visible, and the dialog shown in it.
    pub fn check_modal_valid(&self) -> bool {
        let window = self.st.borrow().modal_window;
        if window == 0 {
            return true;
        }
        if self.wn(EeWonWmWin::State, window, 0, 0) != WINDOW_OPEN {
            return false;
        }
        if self.wn(EeWonWmWin::IsVisible, window, 0, 0) == 0 {
            return false;
        }
        let dialog = self.st.borrow().modal_component;
        if dialog != 0 {
            if self.cm(EeWonWmComp::Bits, dialog, 0, 0) & 1 != 0 {
                return false;
            }
            let window = self.st.borrow().modal_window;
            if self.cm(EeWonWmComp::RootContainer, dialog, 0, 0) != self.wn(EeWonWmWin::Root, window, 0, 0) {
                return false;
            }
        }
        true
    }

    pub fn field(&self, f: i32) -> isize {
        use EeWonWmField as F;
        if f > F::PopupFront as i32 {
            return self.loop_field(f);
        }
        let s = self.st.borrow();
        let b = isize::from;
        if !(F::EndPopup as i32..=F::PopupFront as i32).contains(&f) {
            return 0;
        }
        // SAFETY: in the enum's range (checked above); the enum is repr(u32) with consecutive values
        let field: F = unsafe { std::mem::transmute::<u32, F>(f as u32) };
        match field {
            F::EndPopup => b(s.end_popup),
            F::InDoPopup => b(s.in_do_popup),
            F::InEndPopup => b(s.in_end_popup),
            F::PopupIsUp => b(s.popup_is_up),
            F::OwnWindow => b(s.own_window),
            F::PopupFlags => s.popup_flags as isize,
            F::CaptureSupport => b(s.capture_support),
            F::CurDepth => self.cur_depth.load(Ordering::SeqCst) as isize,
            F::TargetDepth => s.target_depth as isize,
            F::DialogResult => s.dialog_result as isize,
            F::ModalWindow => s.modal_window,
            F::ModalComponent => s.modal_component,
            F::PopupWindowSave => s.popup_window_save,
            F::PopupOwnerWindow => s.popup_owner_window,
            F::PopupInput => s.popup_input,
            F::PopupOwner => s.popup_owner,
            F::FocusWindow => s.focus_window,
            F::MouseDownWindow => s.mouse_down_window,
            F::LastLoseFocusTick => s.last_lose_focus_tick as isize,
            F::PopupCount => s.popups.len() as isize,
            F::PopupBack => s.popups.last().copied().unwrap_or(0),
            F::PopupFront => s.popups.first().copied().unwrap_or(0),
            _ => 0,
        }
    }

    /// The setters the C++ still uses (the event loop's and RemoveWindow's statements, the popup default, the
    /// capture support): `PopupWindowSave` is the `SmartPtr`'s assignment; other fields are not set from outside.
    pub fn set_field(&self, f: i32, v: isize) {
        use EeWonWmField as F;
        if f == F::PopupWindowSave as i32 {
            self.assign(Held::WindowSave, v);
            return;
        }
        if f > F::PopupFront as i32 {
            self.set_loop_field(f, v);
            return;
        }
        let mut s = self.st.borrow_mut();
        let on = v != 0;
        match f {
            x if x == F::EndPopup as i32 => s.end_popup = on,
            x if x == F::OwnWindow as i32 => s.own_window = on,
            x if x == F::CaptureSupport as i32 => s.capture_support = on,
            x if x == F::TargetDepth as i32 => s.target_depth = v as i32,
            x if x == F::MouseDownWindow as i32 => s.mouse_down_window = v,
            x if x == F::LastLoseFocusTick as i32 => s.last_lose_focus_tick = v as u32,
            _ => {}
        }
    }

    /// The held members given back as the original's destructor did (the members in reverse order: the owner
    /// component, the input component, the saved popup window).
    pub fn release_members(&self) {
        self.assign(Held::Owner, 0);
        self.assign(Held::Input, 0);
        self.assign(Held::WindowSave, 0);
    }

    pub fn call(&self, call: EeWonWmCall, a: isize, b: isize, c: isize, d: isize) -> isize {
        type C = EeWonWmCall;
        let bytes = |p: isize, n: isize| -> Vec<u8> {
            if p == 0 || n <= 0 {
                Vec::new()
            } else {
                // SAFETY: the C++ passes a string's bytes and length
                unsafe { std::slice::from_raw_parts(p as *const u8, n as usize) }.to_vec()
            }
        };
        match call {
            C::SetNamedFont => self.set_named_font(&bytes(a, b), c),
            C::GetNamedFont => return self.named_font(&bytes(a, b)),
            C::GetDefaultFont => return self.default_font(),
            C::SetDefaultFont => self.set_default_font(a),
            C::SetCacheImages => self.set_cache_images(a != 0),
            C::DecodeImage => return self.decode_image(a, &bytes(b, c)),
            C::DecodeImageRaw => return self.decode_image_raw(&bytes(a, b)),
            C::DecodeDelayImage => return self.decode_delay_image(&bytes(a, b)),
            C::NotifyLostFocus => self.notify_lost_focus(),
            C::NotifyGotFocus => self.notify_got_focus(a),
            C::PopupWindow => return self.popup_window(),
            C::CalcModality => self.calc_modality(),
            C::NotModalWindow => return isize::from(self.not_modal_window(a)),
            C::DoDialog => return self.do_dialog(a, b) as isize,
            C::EndDialog => self.end_dialog(a as i32, b, c),
            C::ExitMessageLoop => self.exit_message_loop(),
            C::MessageLoop => return self.message_loop() as isize,
            C::AdjustPopupPosition => {
                // SAFETY: the C++ passes a rectangle
                let avoid = unsafe { *(d as *const EeWonRect4) };
                self.adjust_popup_position(a, b, c != 0, avoid);
            }
            C::DoPopup => {
                // SAFETY: the C++ passes the popup's parameters
                let p = unsafe { *(a as *const EeWonWmPopup) };
                self.do_popup(&p);
            }
            C::EndPopup => self.end_popup(a),
            C::CheckEndPopup => self.check_end_popup(a),
            C::FindMouseWindow => {
                // SAFETY: the C++ passes its event's fields, written back after
                let ev = unsafe { &mut *(a as *mut EeWonWmMouse) };
                let mut e = *ev;
                self.find_mouse_window(&mut e);
                *ev = e;
            }
            C::CheckPopupClick => {
                // SAFETY: as above
                let ev = unsafe { &mut *(a as *mut EeWonWmMouse) };
                let mut e = *ev;
                let r = self.check_popup_click(&mut e);
                *ev = e;
                return isize::from(r);
            }
            C::CheckModalValid => return isize::from(self.check_modal_valid()),
            C::GetField => return self.field(a as i32),
            C::SetField => self.set_field(a as i32, b),
            C::SetScreenResolution => self.set_screen_resolution(a as i32, b as i32),
            C::RestoreScreenResolution => self.restore_screen_resolution(),
            C::CloseAllWindows => self.close_all_windows(),
            C::AddWindow => return isize::from(self.add_window(a)),
            C::RemoveWindow => return isize::from(self.remove_window(a)),
            C::PostEvent => self.post_event(a, b != 0),
            C::AddTimerEvent => self.add_timer_event(a),
            C::RemoveTimerEvent => self.remove_timer_event(a),
            C::RemoveAllTimerEvents => self.remove_all_timer_events(),
            C::SubscribeToBroadcast => self.subscribe_to_broadcast(a, b != 0),
            C::ClearSubscriptions => self.clear_subscriptions(),
            C::BroadcastEvent => self.broadcast_event(a),
            C::PeekNextEvent => return self.peek_next_event(),
            C::ProcessEvents => self.process_events(),
            C::DeliverEvent => self.deliver_event(a),
        }
        0
    }
}

/// A new Rust WindowManager for the C++ `manager`, its calls out through `host`.
#[no_mangle]
pub extern "C" fn ee_won_wm_new(manager: *mut c_void, host: EeWonGuiHostFn) -> *mut EeWonWm {
    Box::into_raw(Box::new(EeWonWm::new(manager, host)))
}

/// The manager's object freed, its held members given back first (the broadcast set, the deferred timer
/// removals, the timer events, the event list and the window set; then the popup members; then the caches: the
/// original's members in reverse order).
///
/// # Safety
/// `wm` came from `ee_won_wm_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wm_free(wm: *mut EeWonWm) {
    if !wm.is_null() {
        // SAFETY: the contract
        unsafe {
            (*wm).release_loop();
            (*wm).release_members();
            (*wm).release_caches();
            drop(Box::from_raw(wm));
        }
    }
}

/// One of WindowManager's methods (`EeWonWmCall`).
///
/// # Safety
/// `wm` is live; the call's arguments as documented.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wm_call(
    wm: *const EeWonWm,
    call: EeWonWmCall,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
) -> isize {
    // SAFETY: live (the contract); a shared reference: the calls out re-enter through this function
    unsafe { (*wm).call(call, a, b, c, d) }
}

/// The popup list's `i`-th entry (0 past the end; the goldens' trace of the list).
///
/// # Safety
/// `wm` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wm_popup_at(wm: *const EeWonWm, i: usize) -> isize {
    // SAFETY: live (the contract)
    unsafe { (*wm).st.borrow().popups.get(i).copied().unwrap_or(0) }
}

/// The caches as one line (the goldens' trace) into `buf` (at most `cap` bytes); the length written.
///
/// # Safety
/// `wm` is live; `buf` has room for `cap` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wm_cache_line(wm: *const EeWonWm, buf: *mut u8, cap: usize) -> usize {
    // SAFETY: live (the contract)
    let s = unsafe { (*wm).cache_line() };
    let n = s.len().min(cap);
    // SAFETY: room for `cap` bytes (the contract)
    unsafe { std::ptr::copy_nonoverlapping(s.as_ptr(), buf, n) };
    n
}

#[cfg(test)]
#[path = "wmobj_tests.rs"]
mod tests;
