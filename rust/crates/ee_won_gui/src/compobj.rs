//! WONGUI's Component as Rust (R6 WON round 38): the base class of every lobby control, its methods whole. The
//! original's Component.cpp, ComponentTraversal.cpp, ComponentDefaultHooks.cpp, ComponentApiCompletion.cpp and
//! ComponentResourceAdapters.cpp: the position and size (with the size rules of `guilayout`), the dirty rectangles
//! and invalid state, visibility and enabling, the focus (`guifocus`'s rule), the timers, the events (delivered
//! now or queued, broadcast or bubbled up to a listener, the parent or the owner window's root), the tool tip's
//! hover, the background grabber's hooks, the root container and window, the extra data's cursor, listener and
//! colour scheme.
//!
//! The C++ `Component` stays the type every control derives from: it keeps its members (151 files read its
//! bounds, flags and parent inline) and its vtable. Each method is a one-line forwarder to `ee_won_comp_call`; the
//! Rust has no state of its own and reads and writes the members in place (compmem's block, R6 WONGUI B round 2;
//! the extra data's members through the host), makes the virtual calls (dispatched by the C++
//! on the object's class) and calls the parent container, the window, the event, the listener and the background
//! grabber through the host's `Comp` op (`EeWonComp`). A nonvirtual Component method the original called is the
//! Rust function here (on the same or another component), as the original's call was to that one body. Ints wrap
//! as the original's did (-fwrapv); the timer flags and invalid state are the original's `char`s.

use std::cell::Cell;
use std::ffi::c_void;
use std::sync::atomic::{AtomicUsize, Ordering};

use crate::compmem as mem;
use crate::guifocus::want_focus;
use crate::guihost::{EeWonGuiHostFn, EeWonGuiOp as Op};
use crate::guilayout::{intersects, set_size, size_at_least, EeWonRect};

/// `ComponentFlag_*` (WONComponentFlags.h; the C++ asserts them).
pub const EE_WON_COMP_FLAG_INVISIBLE: i32 = 0x1;
pub const EE_WON_COMP_FLAG_HAS_FOCUS: i32 = 0x10;
pub const EE_WON_COMP_FLAG_DISABLED: i32 = 0x80;
pub const EE_WON_COMP_FLAG_GRAB_BG: i32 = 0x100;
pub const EE_WON_COMP_FLAG_TRANSLUCENT: i32 = 0x200;
pub const EE_WON_COMP_FLAG_NO_INPUT: i32 = 0x800;
pub const EE_WON_COMP_FLAG_WANT_TAB_FOCUS: i32 = 0x4000;
/// The root-visible bit (no keyword; `IsRootVisible`).
pub const EE_WON_COMP_FLAG_ROOT_VISIBLE: i32 = 0x40000;
/// `gComponentToolTipStartTime` and `gComponentToolTipEndTime` (ms; the original's globals, never written).
pub const EE_WON_COMP_TIP_START: u32 = 500;
pub const EE_WON_COMP_TIP_END: u32 = 5000;

/// The inward calls: Component's methods (`a`, `b`, `c` the arguments; pointers as `intptr_t`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCompCall {
    /// `Virtual08()` (the size changed).
    SizeChanged = 1,
    /// `GetDesiredSize(*(int*)a, *(int*)b)`: 20 x 20.
    DesiredSize,
    /// `NeedsPaint()` (0 or 1).
    NeedsPaint,
    /// `PrePaint(*(Graphics*)a)`.
    PrePaint,
    /// `Paint(*(Graphics*)a)`.
    Paint,
    /// `HitTest(a, b)` (0 or 1).
    HitTest,
    /// `MouseEnter(a, b)`.
    MouseEnter,
    /// `MouseExit()`.
    MouseExit,
    /// `MouseHover((unsigned long)a)`.
    MouseHover,
    /// `GotFocus()`.
    GotFocus,
    /// `LostFocus()`.
    LostFocus,
    /// `InvalidateRect(*(WONRectangle*)a)`.
    InvalidateRect,
    /// `Invalidate()`.
    Invalidate,
    /// `MarkInvalid()`.
    MarkInvalid,
    /// `AddedToParent()`.
    AddedToParent,
    /// `ClearGrabBG()`.
    ClearGrabBG,
    /// `SetCursor((Cursor*)a)`.
    SetCursor,
    /// `HandleComponentEvent((ComponentEvent*)a)`.
    HandleEvent,
    /// `HandleBroadcastEvent((ComponentEvent*)a)`.
    HandleBroadcast,
    /// `NotifyRootVisibility(a != 0)`.
    NotifyRootVisibility,
    /// `TabFocus(a != 0, b != 0)` (0 or 1).
    TabFocus,
    /// `EnableControlsById(*(const ControlIdSet*)a, b != 0)`.
    EnableControlsById,
    /// `SetEnabled(a != 0)`.
    SetEnabled,
    /// `SetVisible(a != 0)`.
    SetVisible,
    /// `SetColorScheme((ColorScheme*)a)`.
    SetColorScheme,
    /// `GetOwnColorScheme()`.
    OwnColorScheme,
    /// `GetColorScheme()`.
    ColorScheme,
    /// `SetPos(a, b)`.
    SetPos,
    /// `SetSize(a, b)`.
    SetSize,
    /// `SetSizeAtLeast(a, b)`.
    SetSizeAtLeast,
    /// `SetPosSize(a, b, p[0], p[1])` with `p` the two ints at `c`.
    SetPosSize,
    /// `FireBroadcastEvent((ComponentEvent*)a, b != 0)`.
    FireBroadcast,
    /// `BroadcastEventDown((ComponentEvent*)a)`.
    BroadcastDown,
    /// `DeliverEvent((ComponentEvent*)a, b != 0)`.
    Deliver,
    /// `AddListener((ComponentListener*)a)`.
    AddListener,
    /// `SetDesiredSize(a, b)`.
    SetDesiredSize,
    /// `FireEvent(a, b != 0)`.
    FireEvent,
    /// `IsRootVisible()` (0 or 1).
    IsRootVisible,
    /// `LocalToRoot(*(int*)a, *(int*)b)`.
    LocalToRoot,
    /// `BubbleEvent((ComponentEvent*)a)`.
    Bubble,
    /// `GetNextSibling(a != 0, b != 0)`.
    NextSibling,
    /// `WantFocus()` (0 or 1).
    WantFocus,
    /// `RootToChild(*(int*)a, *(int*)b)`.
    RootToChild,
    /// `GetMousePos(*(int*)a, *(int*)b)`.
    MousePos,
    /// `GetKeyMod()`.
    KeyMod,
    /// `CheckDoubleClick(a)` (0 or 1).
    DoubleClick,
    /// `Contains(a, b)` (0 or 1).
    Contains,
    /// `Intersects((Component*)a)` (0 or 1).
    Intersects,
    /// `GetDefaultCursor()`.
    DefaultCursor,
    /// `GetGrabBG()`.
    GrabBG,
    /// `CheckGrabBG()` (0 or 1).
    CheckGrabBG,
    /// `EnsureOnTopOf((Component*)a)`.
    EnsureOnTopOf,
    /// `SetDefaultCursor((Cursor*)a)`.
    SetDefaultCursor,
    /// `SetDesiredHeight(a)`.
    SetDesiredHeight,
    /// `SetDesiredWidth(a)`.
    SetDesiredWidth,
    /// `SetTranslucentColor(a, b)`.
    SetTranslucentColor,
    /// `SetComponentFlags(a, b != 0)`.
    SetFlags,
    /// `SubscribeToBroadcast(a != 0)`.
    Subscribe,
    /// `RequestTimer(a != 0)`.
    RequestTimer,
    /// `RequestFocus()`.
    RequestFocus,
    /// `GetRoot()`: the root container's window, else `Window::GetDefaultWindow()`.
    Root,
    /// `GetRootContainer()`.
    RootContainer,
    /// `GetRootWindow()`: the root container's window, else NULL.
    RootWindow,
    /// `GetFontMod((Font*)a, b, c)`.
    FontMod,
}

/// The host's calls (`EeWonGuiOp::Comp`; `b` the receiver, `c`, `d` the arguments; a component as `Component*`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonComp {
    // ---- the component `b`'s extra data (its other members are compmem's block)
    /// `AllocateExtraStuff()`.
    AllocExtra = 1,
    /// a member of `mExtraStuff` (`c` an `EeWonCompExtra`; a pointer, or the tip flag 0 or 1).
    Extra,
    /// a member of `mExtraStuff` (`c` an `EeWonCompExtra`) = `d`; `GrabBG`: `new BackgroundGrabber`.
    SetExtra,
    // ---- the component `b`'s virtual methods
    /// `PositionChanged()`.
    VPositionChanged,
    /// `Virtual08()`.
    VSizeChanged,
    /// `GetDesiredSize(p[0], p[1])` with `p` the two ints at `c`.
    VDesiredSize,
    /// `HandleComponentEvent((ComponentEvent*)c)`.
    VHandleEvent,
    /// `HandleBroadcastEvent((ComponentEvent*)c)`.
    VHandleBroadcast,
    /// `InvalidateRect(*(WONRectangle*)c)`.
    VInvalidateRect,
    /// `Invalidate()`.
    VInvalidate,
    /// `MarkInvalid()`.
    VMarkInvalid,
    /// `GotFocus()`.
    VGotFocus,
    /// `LostFocus()`.
    VLostFocus,
    /// `NotifyRootVisibility(c != 0)`.
    VNotifyRootVisibility,
    /// `SetEnabled(c != 0)`.
    VSetEnabled,
    /// `EnabledChanged(c != 0)`.
    VEnabledChanged,
    /// `SetCursor((Cursor*)c)`.
    VSetCursor,
    // ---- the parent `b` (a `Container`)
    /// `NextChild((Component*)c, d & 1, d & 2)`.
    PNextChild,
    /// `SetFocusChild((Component*)c)`.
    PSetFocusChild,
    /// `AddTimerChild((Component*)c)`.
    PAddTimerChild,
    /// `ChildStateChanged((Component*)c)`.
    PChildStateChanged,
    /// `ChildInvalidated((Component*)c)` (the result unused, as the original's).
    PChildInvalidated,
    /// `IsModal()` (0 or 1).
    PIsModal,
    /// `EnsureOnTopOf((Component*)c, (Component*)d)`.
    PEnsureOnTopOf,
    // ---- the root container and the windows
    /// `dynamic_cast<RootContainer*>((Component*)b)` (0 when it is not one).
    AsRoot,
    /// `((RootContainer*)b)->mWindow`.
    RootWin,
    /// `((RootContainer*)b)->AddBG((BackgroundGrabber*)c)`.
    RootAddBG,
    /// `Window::GetDefaultWindow()`.
    DefaultWindow,
    /// `((Window*)b)->GetWindowState()`.
    WState,
    /// `((Window*)b)->mManager->BroadcastEvent((ComponentEvent*)c)`.
    WBroadcast,
    /// `((Window*)b)->QueueEvent((ComponentEvent*)c)`.
    WQueue,
    /// `((Window*)b)->GetParent()`.
    WParent,
    /// `((Window*)b)->mRoot.get()` (as a `Component*`).
    WRoot,
    /// `((Window*)b)->GetMouseX()` and `GetMouseY()` into the two ints at `c`.
    WMouse,
    /// `((Window*)b)->OpenPopup(PopupParams(p[0], PopupFlag_StandardTip, p[1], p[2], p[3]))` with `p` the four
    /// `intptr_t`s at `c` (the tip, its owner, x, y).
    WOpenTip,
    /// `((Window*)b)->ClosePopup((Component*)c)`.
    WClosePopup,
    /// `((Window*)b)->GetKeyMod()`.
    WKeyMod,
    /// `((Window*)b)->CheckDoubleClick((MouseButton)c)` (0 or 1).
    WDoubleClick,
    /// `((Window*)b)->GetWindowManager()->SubscribeToBroadcast((Component*)c, d != 0)`.
    WSubscribe,
    /// `((Window*)b)->GetWindowManager()->mColorScheme.get()`.
    WScheme,
    /// the font `c` with style `p[0]` and, when `p[1]` is not 0, size `p[1]` (`p` the two ints at `d`), from
    /// `((Window*)b)->GetFont(descriptor)` (its pointer, held by the window's cache as the original's was).
    WFontMod,
    // ---- an event `b`, a listener, a background grabber, an id set
    /// `AddRef()` (a `SmartPtr` the method holds).
    EvHold,
    /// `Release()` (its end; the event is deleted at its last reference).
    EvRelease,
    /// `mBroadcast` (0 or 1).
    EvBroadcast,
    /// `mBroadcast = c != 0`.
    EvSetBroadcast,
    /// `mComponent`.
    EvComp,
    /// `SetComponent((Component*)c)`.
    EvSetComp,
    /// `new ComponentEvent((Component*)b, c)` (no reference yet).
    EvNew,
    /// `((ComponentListener*)b)->HandleComponentEvent((ComponentEvent*)c)`.
    LHandle,
    /// `((BackgroundGrabber*)b)->SizeChanged((Component*)c)`.
    GSizeChanged,
    /// `((BackgroundGrabber*)b)->PrePaint((Component*)c, *(Graphics*)d)`.
    GPrePaint,
    /// `((BackgroundGrabber*)b)->Paint((Component*)c, *(Graphics*)d)`.
    GPaint,
    /// `((BackgroundGrabber*)b)->Clear()`.
    GClear,
    /// `((BackgroundGrabber*)b)->Invalidate((Component*)c)`.
    GInvalidate,
    /// `((BackgroundGrabber*)b)->NeedDrawBG()` (0 or 1).
    GNeedDrawBG,
    /// `((BackgroundGrabber*)b)->SetTranslucentColor(c, d)`.
    GTranslucent,
    /// `((const ControlIdSet*)b)->find(c) != end()` (0 or 1).
    IdIn,
    // ---- more of the component `b`'s virtual methods (the Rust objects' calls on another component, round 3)
    /// `SetVisible(c != 0)`.
    VSetVisible,
    /// `GetMouseChild()`.
    VMouseChild,
}

/// A member of a component's extra data (`EeWonComp::Extra` / `SetExtra`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCompExtra {
    /// `mListener.get()`.
    Listener = 1,
    /// `mDefaultCursor.get()`.
    Cursor,
    /// `mColorScheme.get()`.
    Scheme,
    /// `mGrabBG` (set: a new one).
    GrabBG,
    /// `mToolTip.get()`.
    ToolTip,
    /// `mDontDoTip` (0 or 1).
    DontDoTip,
}

use EeWonComp as K;

/// The game's component host (`ee_won_guicpp_comp_call`, registered by ComponentLifetime.cpp at start).
static GAME_HOST: AtomicUsize = AtomicUsize::new(0);

thread_local! {
    /// A world's component host (drivers and tests), ahead of the game's on its thread.
    static WORLD_HOST: Cell<EeWonGuiHostFn> = const { Cell::new(None) };
}

/// Registers the game's component host: the one C++ host every Rust object's call on a component goes through
/// (its virtuals, its parent, window, events and extra data).
#[no_mangle]
pub extern "C" fn ee_won_comp_host_register(host: EeWonGuiHostFn) {
    GAME_HOST.store(host.map_or(0, |f| f as usize), Ordering::Release);
}

/// A world's component host on this thread (drivers and tests; NULL removes it).
#[no_mangle]
pub extern "C" fn ee_won_comp_host_world(host: EeWonGuiHostFn) {
    WORLD_HOST.with(|h| h.set(host));
}

/// The component host: this thread's world's, else the game's.
pub fn host() -> EeWonGuiHostFn {
    if let Some(f) = WORLD_HOST.with(|h| h.get()) {
        return Some(f);
    }
    let f = GAME_HOST.load(Ordering::Acquire);
    if f == 0 {
        return None;
    }
    // SAFETY: a value `ee_won_comp_host_register` stored from a host function pointer
    Some(unsafe { std::mem::transmute::<usize, unsafe extern "C" fn(*mut c_void, Op, isize, isize, isize, isize) -> isize>(f) })
}

/// A test's component host on its thread, until the guard is dropped.
#[cfg(test)]
pub(crate) struct HostWorld;

#[cfg(test)]
impl Drop for HostWorld {
    fn drop(&mut self) {
        ee_won_comp_host_world(None);
    }
}

#[cfg(test)]
pub(crate) fn host_world(host: EeWonGuiHostFn) -> HostWorld {
    ee_won_comp_host_world(host);
    HostWorld
}

/// Component's methods over the component host: what a Rust object calls on a component (its own or another's).
pub fn comp() -> Comp {
    Comp::new(host())
}

/// The host and the calls out; every function below takes the component as `c`.
#[derive(Clone, Copy)]
pub struct Comp {
    host: EeWonGuiHostFn,
}

impl Comp {
    pub fn new(host: EeWonGuiHostFn) -> Self {
        Comp { host }
    }

    fn h(&self, k: EeWonComp, b: isize, c: isize, d: isize) -> isize {
        let f = self.host.expect("compobj: no host");
        // SAFETY: the host's contract: `b` is the op's receiver, `c` and `d` its arguments
        unsafe { f(b as *mut c_void, Op::Comp, k as isize, b, c, d) }
    }

    // ---- the members (the C++ object's block, compmem)
    fn bits(&self, c: isize) -> i32 {
        mem::bits(c)
    }
    fn flag_set(&self, c: isize, f: i32) -> bool {
        self.bits(c) & f != 0
    }
    fn is_visible(&self, c: isize) -> bool {
        !self.flag_set(c, EE_WON_COMP_FLAG_INVISIBLE)
    }
    fn has_focus(&self, c: isize) -> bool {
        self.flag_set(c, EE_WON_COMP_FLAG_HAS_FOCUS)
    }
    fn is_disabled(&self, c: isize) -> bool {
        self.bits(c) & EE_WON_COMP_FLAG_DISABLED != 0
    }
    fn invalid(&self, c: isize) -> i8 {
        mem::invalid(c)
    }
    fn set_invalid(&self, c: isize, v: i8) {
        mem::set_invalid(c, v);
    }
    fn bounds(&self, c: isize) -> EeWonRect {
        mem::bounds(c)
    }
    fn parent(&self, c: isize) -> isize {
        mem::parent(c)
    }
    fn has_extra(&self, c: isize) -> bool {
        mem::has_extra(c)
    }
    fn extra(&self, c: isize, e: EeWonCompExtra) -> isize {
        self.h(K::Extra, c, e as isize, 0)
    }
    fn set_extra(&self, c: isize, e: EeWonCompExtra, v: isize) {
        self.h(K::SetExtra, c, e as isize, v);
    }
    fn alloc_extra(&self, c: isize) {
        self.h(K::AllocExtra, c, 0, 0);
    }

    // ---- the methods
    // ---- the virtual methods, dispatched by the C++ on the component's class
    /// `Invalidate()`.
    pub fn v_invalidate(&self, c: isize) {
        self.h(K::VInvalidate, c, 0, 0);
    }
    /// `SetVisible(on)`.
    pub fn v_set_visible(&self, c: isize, on: bool) {
        self.h(K::VSetVisible, c, on as isize, 0);
    }
    /// `SetEnabled(on)`.
    pub fn v_set_enabled(&self, c: isize, on: bool) {
        self.h(K::VSetEnabled, c, on as isize, 0);
    }
    /// `GetDesiredSize(w, h)`.
    pub fn v_desired_size(&self, c: isize) -> (i32, i32) {
        self.desired(c)
    }
    /// `GetMouseChild()`.
    pub fn v_mouse_child(&self, c: isize) -> isize {
        self.h(K::VMouseChild, c, 0, 0)
    }

    /// `GetDefaultCursor()`.
    pub fn default_cursor(&self, c: isize) -> isize {
        self.extra_or_null(c, EeWonCompExtra::Cursor)
    }

    /// `SetPosSize`.
    pub fn set_pos_size(&self, c: isize, x: i32, y: i32, width: i32, height: i32) {
        self.set_pos(c, x, y);
        self.set_size(c, width, height);
    }

    /// `SetComponentFlags`.
    pub fn set_flags(&self, c: isize, flags: i32, on: bool) {
        let b = self.bits(c);
        mem::set_bits(c, if on { b | flags } else { b & !flags });
    }

    /// `SetPos`.
    pub fn set_pos(&self, c: isize, x: i32, y: i32) {
        mem::set_xy(c, x, y);
        self.h(K::VPositionChanged, c, 0, 0);
    }

    /// `SetSize`.
    pub fn set_size(&self, c: isize, width: i32, height: i32) {
        let r = self.bounds(c);
        if let Some((w, h)) = set_size(r.width, r.height, width, height) {
            mem::set_wh(c, w, h);
            self.h(K::VSizeChanged, c, 0, 0);
        }
    }

    /// `SetSizeAtLeast`.
    pub fn set_size_at_least(&self, c: isize, width: i32, height: i32) {
        let r = self.bounds(c);
        if let Some((w, h)) = size_at_least(r.width, r.height, width, height) {
            self.set_size(c, w, h);
        }
    }

    fn desired(&self, c: isize) -> (i32, i32) {
        let mut p = [0i32; 2];
        self.h(K::VDesiredSize, c, p.as_mut_ptr() as isize, 0);
        (p[0], p[1])
    }

    /// `SetDesiredSize`.
    pub fn set_desired_size(&self, c: isize, extra_width: i32, extra_height: i32) {
        let (w, h) = self.desired(c);
        self.set_size(c, w.wrapping_add(extra_width), h.wrapping_add(extra_height));
    }

    /// `SetDesiredWidth`.
    pub fn set_desired_width(&self, c: isize, padding: i32) {
        let (w, _) = self.desired(c);
        let height = self.bounds(c).height;
        self.set_size(c, w.wrapping_add(padding), height);
    }

    /// `SetDesiredHeight`.
    pub fn set_desired_height(&self, c: isize, padding: i32) {
        let (_, h) = self.desired(c);
        let width = self.bounds(c).width;
        self.set_size(c, width, h.wrapping_add(padding));
    }

    /// `LocalToRoot`.
    pub fn local_to_root(&self, c: isize, x: &mut i32, y: &mut i32) {
        let r = self.bounds(c);
        *x = x.wrapping_add(r.x);
        *y = y.wrapping_add(r.y);
        let mut p = self.parent(c);
        while p != 0 {
            let r = self.bounds(p);
            *x = x.wrapping_add(r.x);
            *y = y.wrapping_add(r.y);
            p = self.parent(p);
        }
    }

    /// `RootToChild`.
    pub fn root_to_child(&self, c: isize, x: &mut i32, y: &mut i32) {
        let r = self.bounds(c);
        *x = x.wrapping_sub(r.x);
        *y = y.wrapping_sub(r.y);
        let p = self.parent(c);
        if p != 0 {
            self.root_to_child(p, x, y);
        }
    }

    /// `InvalidateRect` (the rectangle at `rect` moved into the parent's coordinates).
    pub fn invalidate_rect(&self, c: isize, rect: &mut EeWonRect) {
        let p = self.parent(c);
        if p == 0 {
            return;
        }
        let r = self.bounds(c);
        rect.x = rect.x.wrapping_add(r.x);
        rect.y = rect.y.wrapping_add(r.y);
        self.h(K::VInvalidateRect, p, rect as *mut EeWonRect as isize, 0);
    }

    /// `EnableControlsById`.
    pub fn enable_controls_by_id(&self, c: isize, ids: isize, enabled: bool) {
        let id = mem::control_id(c);
        if id != 0 && self.h(K::IdIn, ids, id as isize, 0) != 0 {
            self.h(K::VSetEnabled, c, enabled as isize, 0);
        }
    }

    /// `Contains`.
    pub fn contains(&self, c: isize, x: i32, y: i32) -> bool {
        let r = self.bounds(c);
        x >= r.x && x < r.x.wrapping_add(r.width) && y >= r.y && y < r.y.wrapping_add(r.height)
    }

    /// `Intersects`.
    pub fn intersects(&self, c: isize, other: isize) -> bool {
        intersects(self.bounds(c), self.bounds(other))
    }

    /// `SetVisible`.
    pub fn set_visible(&self, c: isize, visible: bool) {
        if visible == self.is_visible(c) {
            return;
        }
        self.set_flags(c, EE_WON_COMP_FLAG_INVISIBLE, !visible);
        self.set_invalid(c, 0);
        let p = self.parent(c);
        if p != 0 {
            self.h(K::PChildStateChanged, p, c, 0);
        }
        self.h(K::VInvalidate, c, 0, 0);
        self.h(K::VNotifyRootVisibility, c, visible as isize, 0);
    }

    /// `SetEnabled`.
    pub fn set_enabled(&self, c: isize, enabled: bool) {
        if enabled == !self.is_disabled(c) {
            return;
        }
        self.set_flags(c, EE_WON_COMP_FLAG_DISABLED, !enabled);
        self.h(K::VEnabledChanged, c, enabled as isize, 0);
        let p = self.parent(c);
        if p != 0 {
            self.h(K::PChildStateChanged, p, c, 0);
        }
        self.h(K::VInvalidate, c, 0, 0);
    }

    /// `BroadcastEventDown` (the event held for the call).
    pub fn broadcast_down(&self, c: isize, ev: isize) {
        self.h(K::EvHold, ev, 0, 0);
        self.h(K::EvSetBroadcast, ev, 1, 0);
        self.h(K::VHandleBroadcast, c, ev, 0);
        self.h(K::EvRelease, ev, 0, 0);
    }

    /// `DeliverEvent`: dropped (its reference taken and given back: an event nothing holds is deleted) without an
    /// open root window; else sent now (broadcast through the window manager or bubbled up) or queued.
    pub fn deliver(&self, c: isize, ev: isize, send_now: bool) {
        let w = self.root_window(c);
        if w == 0 || self.h(K::WState, w, 0, 0) as i32 != 0 {
            self.h(K::EvHold, ev, 0, 0);
            self.h(K::EvRelease, ev, 0, 0);
            return;
        }
        if self.h(K::EvComp, ev, 0, 0) == 0 {
            self.h(K::EvSetComp, ev, c, 0);
        }
        if send_now {
            self.h(K::EvHold, ev, 0, 0);
            if self.h(K::EvBroadcast, ev, 0, 0) != 0 {
                self.h(K::WBroadcast, w, ev, 0);
            } else {
                self.bubble(c, ev);
            }
            self.h(K::EvRelease, ev, 0, 0);
        } else {
            self.h(K::WQueue, w, ev, 0);
        }
    }

    /// `FireBroadcastEvent`.
    pub fn fire_broadcast(&self, c: isize, ev: isize, send_now: bool) {
        self.h(K::EvSetBroadcast, ev, 1, 0);
        self.deliver(c, ev, send_now);
    }

    /// `FireEvent`: a new event of the type from this component, delivered.
    pub fn fire_event(&self, c: isize, kind: i32, send_now: bool) {
        let ev = self.h(K::EvNew, c, kind as isize, 0);
        self.deliver(c, ev, send_now);
    }

    /// `BubbleEvent`: a non-broadcast event to the listener, else the parent, else the owner window's root.
    pub fn bubble(&self, c: isize, ev: isize) {
        if self.h(K::EvBroadcast, ev, 0, 0) != 0 {
            return;
        }
        if self.has_extra(c) && self.extra(c, EeWonCompExtra::Listener) != 0 {
            let l = self.extra(c, EeWonCompExtra::Listener);
            self.h(K::LHandle, l, ev, 0);
            return;
        }
        let p = self.parent(c);
        if p != 0 {
            self.h(K::VHandleEvent, p, ev, 0);
            return;
        }
        let w = self.root(c);
        if w != 0 {
            let w = self.h(K::WParent, w, 0, 0);
            if w != 0 {
                let r = self.h(K::WRoot, w, 0, 0);
                if r != 0 {
                    self.bubble(r, ev);
                }
            }
        }
    }

    /// `GetNextSibling`.
    pub fn next_sibling(&self, c: isize, forward: bool, wrap: bool) -> isize {
        let p = self.parent(c);
        if p == 0 {
            return 0;
        }
        self.h(K::PNextChild, p, c, (forward as isize) | ((wrap as isize) << 1))
    }

    /// `AddedToParent`.
    pub fn added_to_parent(&self, c: isize) {
        if self.has_focus(c) {
            if !self.want_focus(c) {
                self.h(K::VLostFocus, c, 0, 0);
            } else {
                self.h(K::PSetFocusChild, self.parent(c), c, 0);
            }
        }
        if mem::timer(c) != 0 {
            self.h(K::PAddTimerChild, self.parent(c), c, 0);
        }
        if self.is_root_visible(self.parent(c)) && self.is_visible(c) {
            self.h(K::VNotifyRootVisibility, c, 1, 0);
        }
        self.set_invalid(c, 0);
        self.h(K::VInvalidate, c, 0, 0);
    }

    /// `WantFocus` (`guifocus`'s rule).
    pub fn want_focus(&self, c: isize) -> bool {
        let p = self.parent(c);
        let modal = p != 0 && self.h(K::PIsModal, p, 0, 0) != 0;
        want_focus(self.bits(c) as u32, self.is_visible(c), self.is_disabled(c), modal)
    }

    /// `MouseEnter`: the default cursor.
    pub fn mouse_enter(&self, c: isize) {
        if self.has_extra(c) {
            let cur = self.extra(c, EeWonCompExtra::Cursor);
            self.h(K::VSetCursor, c, cur, 0);
        }
    }

    /// `MouseExit`: the tip closed and re-armed.
    pub fn mouse_exit(&self, c: isize) {
        if self.has_extra(c) && self.extra(c, EeWonCompExtra::ToolTip) != 0 {
            self.set_extra(c, EeWonCompExtra::DontDoTip, 0);
            let w = self.root(c);
            self.h(K::WClosePopup, w, self.extra(c, EeWonCompExtra::ToolTip), 0);
        }
    }

    /// `MouseHover`: the tip opened beside the mouse after the delay, closed after the display time.
    pub fn mouse_hover(&self, c: isize, elapsed: u32) {
        if !self.has_extra(c) || self.extra(c, EeWonCompExtra::ToolTip) == 0 {
            return;
        }
        if elapsed >= EE_WON_COMP_TIP_START && self.extra(c, EeWonCompExtra::DontDoTip) == 0 {
            let w = self.root(c);
            let mut m = [0i32; 2];
            self.h(K::WMouse, w, m.as_mut_ptr() as isize, 0);
            let p: [isize; 4] = [
                self.extra(c, EeWonCompExtra::ToolTip),
                c,
                m[0].wrapping_add(15) as isize,
                m[1].wrapping_add(15) as isize,
            ];
            self.h(K::WOpenTip, w, p.as_ptr() as isize, 0);
            self.set_extra(c, EeWonCompExtra::DontDoTip, 1);
        } else if elapsed >= EE_WON_COMP_TIP_START.wrapping_add(EE_WON_COMP_TIP_END) {
            let w = self.root(c);
            self.h(K::WClosePopup, w, self.extra(c, EeWonCompExtra::ToolTip), 0);
        }
    }

    /// `TabFocus`.
    pub fn tab_focus(&self, c: isize, init: bool) -> bool {
        if init && self.flag_set(c, EE_WON_COMP_FLAG_WANT_TAB_FOCUS) && self.want_focus(c) {
            self.request_focus(c);
            return true;
        }
        false
    }

    /// `RequestTimer`: bit 2 of the timer flags; the parent told when the need changes.
    pub fn request_timer(&self, c: isize, on: bool) {
        let old = mem::timer(c);
        let new = if on { old | 2 } else { old & !2 };
        mem::set_timer(c, new);
        if (new != 0) != (old != 0) {
            let p = self.parent(c);
            if p != 0 {
                self.h(K::PAddTimerChild, p, c, 0);
            }
        }
    }

    /// `RequestFocus`.
    pub fn request_focus(&self, c: isize) {
        if self.has_focus(c) || !self.want_focus(c) {
            return;
        }
        let p = self.parent(c);
        if p == 0 {
            self.h(K::VGotFocus, c, 0, 0);
        } else {
            self.h(K::PSetFocusChild, p, c, 0);
        }
    }

    /// `GetRootContainer`: the topmost ancestor if it is a root container.
    pub fn root_container(&self, c: isize) -> isize {
        let mut r = c;
        loop {
            let p = self.parent(r);
            if p == 0 {
                break;
            }
            r = p;
        }
        self.h(K::AsRoot, r, 0, 0)
    }

    /// `GetRoot`.
    pub fn root(&self, c: isize) -> isize {
        let rc = self.root_container(c);
        if rc != 0 {
            self.h(K::RootWin, rc, 0, 0)
        } else {
            self.h(K::DefaultWindow, 0, 0, 0)
        }
    }

    /// `GetRootWindow`.
    pub fn root_window(&self, c: isize) -> isize {
        let rc = self.root_container(c);
        if rc != 0 {
            self.h(K::RootWin, rc, 0, 0)
        } else {
            0
        }
    }

    /// `IsRootVisible`.
    pub fn is_root_visible(&self, c: isize) -> bool {
        self.flag_set(c, EE_WON_COMP_FLAG_ROOT_VISIBLE)
    }

    /// `CheckGrabBG`: whether the component grabs its background (its grabber made on first use).
    pub fn check_grab_bg(&self, c: isize) -> bool {
        if self.flag_set(c, EE_WON_COMP_FLAG_GRAB_BG | EE_WON_COMP_FLAG_TRANSLUCENT) {
            self.alloc_extra(c);
            if self.extra(c, EeWonCompExtra::GrabBG) == 0 {
                self.set_extra(c, EeWonCompExtra::GrabBG, 0);
            }
            return true;
        }
        false
    }

    fn grab(&self, c: isize) -> isize {
        self.extra(c, EeWonCompExtra::GrabBG)
    }

    /// `NeedsPaint`.
    pub fn needs_paint(&self, c: isize) -> bool {
        if self.invalid(c) == 0 {
            return false;
        }
        if self.is_visible(c) {
            return true;
        }
        self.check_grab_bg(c) && self.h(K::GNeedDrawBG, self.grab(c), 0, 0) != 0
    }

    /// `Invalidate`.
    pub fn invalidate(&self, c: isize) {
        if self.invalid(c) == 2 {
            return;
        }
        let r = self.bounds(c);
        let mut rect = EeWonRect::new(0, 0, r.width, r.height);
        self.h(K::VInvalidateRect, c, &mut rect as *mut EeWonRect as isize, 0);
        let p = self.parent(c);
        if p != 0 {
            self.h(K::PChildInvalidated, p, c, 0);
        }
        self.h(K::VMarkInvalid, c, 0, 0);
    }

    /// `MarkInvalid`.
    pub fn mark_invalid(&self, c: isize) {
        self.set_invalid(c, 2);
        if !self.check_grab_bg(c) {
            return;
        }
        self.h(K::GInvalidate, self.grab(c), c, 0);
        let rc = self.root_container(c);
        if rc != 0 {
            self.h(K::RootAddBG, rc, self.grab(c), 0);
        }
    }

    /// `Virtual08`.
    pub fn size_changed(&self, c: isize) {
        if self.check_grab_bg(c) {
            self.h(K::GSizeChanged, self.grab(c), c, 0);
        }
    }

    /// `PrePaint`.
    pub fn pre_paint(&self, c: isize, g: isize) {
        if self.check_grab_bg(c) {
            self.h(K::GPrePaint, self.grab(c), c, g);
        }
    }

    /// `Paint`.
    pub fn paint(&self, c: isize, g: isize) {
        self.set_invalid(c, 0);
        if self.check_grab_bg(c) {
            self.h(K::GPaint, self.grab(c), c, g);
        }
    }

    /// `ClearGrabBG`.
    pub fn clear_grab_bg(&self, c: isize) {
        if self.check_grab_bg(c) {
            self.h(K::GClear, self.grab(c), 0, 0);
        }
    }

    /// `SetTranslucentColor`.
    pub fn set_translucent_color(&self, c: isize, color: i32, level: i32) {
        self.set_flags(c, EE_WON_COMP_FLAG_TRANSLUCENT, true);
        if !self.check_grab_bg(c) {
            return;
        }
        self.h(K::GTranslucent, self.grab(c), color as isize, level as isize);
    }

    /// `GetColorScheme`: its own, else the parent's, else the window manager's.
    pub fn color_scheme(&self, c: isize) -> isize {
        if self.has_extra(c) && self.extra(c, EeWonCompExtra::Scheme) != 0 {
            return self.extra(c, EeWonCompExtra::Scheme);
        }
        let p = self.parent(c);
        if p != 0 {
            return self.color_scheme(p);
        }
        let w = self.root(c);
        self.h(K::WScheme, w, 0, 0)
    }

    fn extra_or_null(&self, c: isize, e: EeWonCompExtra) -> isize {
        if self.has_extra(c) {
            self.extra(c, e)
        } else {
            0
        }
    }

    fn set_own(&self, c: isize, e: EeWonCompExtra, v: isize) {
        self.alloc_extra(c);
        self.set_extra(c, e, v);
    }

    /// `GetMousePos`.
    pub fn mouse_pos(&self, c: isize, x: &mut i32, y: &mut i32) {
        let w = self.root(c);
        let mut m = [0i32; 2];
        self.h(K::WMouse, w, m.as_mut_ptr() as isize, 0);
        *x = m[0];
        *y = m[1];
        self.root_to_child(c, x, y);
    }

    /// `GetFontMod`: the font in another style and, if `size > 0`, size.
    pub fn font_mod(&self, c: isize, font: isize, style: i32, size: i32) -> isize {
        let w = self.root(c);
        let p = [style, if size > 0 { size } else { 0 }];
        self.h(K::WFontMod, w, font, p.as_ptr() as isize)
    }

    /// One inward call.
    pub fn call(&self, c: isize, k: EeWonCompCall, a: isize, b: isize, cc: isize) -> isize {
        use EeWonCompCall as C;
        let (ai, bi) = (a as i32, b as i32);
        let ints = |p: isize| p as *mut i32;
        match k {
            C::SizeChanged => self.size_changed(c),
            C::DesiredSize => {
                // SAFETY: the caller's ints
                unsafe {
                    *ints(a) = 20;
                    *ints(b) = 20;
                }
            }
            C::NeedsPaint => return self.needs_paint(c) as isize,
            C::PrePaint => self.pre_paint(c, a),
            C::Paint => self.paint(c, a),
            C::HitTest => return (self.is_visible(c) && !self.flag_set(c, EE_WON_COMP_FLAG_NO_INPUT)) as isize,
            C::MouseEnter => self.mouse_enter(c),
            C::MouseExit => self.mouse_exit(c),
            C::MouseHover => self.mouse_hover(c, a as u32),
            C::GotFocus => self.set_flags(c, EE_WON_COMP_FLAG_HAS_FOCUS, true),
            C::LostFocus => self.set_flags(c, EE_WON_COMP_FLAG_HAS_FOCUS, false),
            // SAFETY: the caller's rectangle
            C::InvalidateRect => self.invalidate_rect(c, unsafe { &mut *(a as *mut EeWonRect) }),
            C::Invalidate => self.invalidate(c),
            C::MarkInvalid => self.mark_invalid(c),
            C::AddedToParent => self.added_to_parent(c),
            C::ClearGrabBG => self.clear_grab_bg(c),
            C::SetCursor => {
                let p = self.parent(c);
                if p != 0 {
                    self.h(K::VSetCursor, p, a, 0);
                }
            }
            C::HandleEvent => self.bubble(c, a),
            C::HandleBroadcast => {
                self.h(K::VHandleEvent, c, a, 0);
            }
            C::NotifyRootVisibility => self.set_flags(c, EE_WON_COMP_FLAG_ROOT_VISIBLE, a != 0),
            C::TabFocus => return self.tab_focus(c, b != 0) as isize,
            C::EnableControlsById => self.enable_controls_by_id(c, a, b != 0),
            C::SetEnabled => self.set_enabled(c, a != 0),
            C::SetVisible => self.set_visible(c, a != 0),
            C::SetColorScheme => self.set_own(c, EeWonCompExtra::Scheme, a),
            C::OwnColorScheme => return self.extra_or_null(c, EeWonCompExtra::Scheme),
            C::ColorScheme => return self.color_scheme(c),
            C::SetPos => self.set_pos(c, ai, bi),
            C::SetSize => self.set_size(c, ai, bi),
            C::SetSizeAtLeast => self.set_size_at_least(c, ai, bi),
            C::SetPosSize => {
                // SAFETY: the caller's two ints
                let (w, h) = unsafe { (*ints(cc), *ints(cc).add(1)) };
                self.set_pos_size(c, ai, bi, w, h);
            }
            C::FireBroadcast => self.fire_broadcast(c, a, b != 0),
            C::BroadcastDown => self.broadcast_down(c, a),
            C::Deliver => self.deliver(c, a, b != 0),
            C::AddListener => self.set_own(c, EeWonCompExtra::Listener, a),
            C::SetDesiredSize => self.set_desired_size(c, ai, bi),
            C::FireEvent => self.fire_event(c, ai, b != 0),
            C::IsRootVisible => return self.is_root_visible(c) as isize,
            C::LocalToRoot | C::RootToChild | C::MousePos => {
                // SAFETY: the caller's ints
                let (x, y) = unsafe { (&mut *ints(a), &mut *ints(b)) };
                match k {
                    C::LocalToRoot => self.local_to_root(c, x, y),
                    C::RootToChild => self.root_to_child(c, x, y),
                    _ => self.mouse_pos(c, x, y),
                }
            }
            C::Bubble => self.bubble(c, a),
            C::NextSibling => return self.next_sibling(c, a != 0, b != 0),
            C::WantFocus => return self.want_focus(c) as isize,
            C::KeyMod => return self.h(K::WKeyMod, self.root(c), 0, 0),
            C::DoubleClick => return self.h(K::WDoubleClick, self.root(c), a, 0),
            C::Contains => return self.contains(c, ai, bi) as isize,
            C::Intersects => return self.intersects(c, a) as isize,
            C::DefaultCursor => return self.extra_or_null(c, EeWonCompExtra::Cursor),
            C::GrabBG => return self.extra_or_null(c, EeWonCompExtra::GrabBG),
            C::CheckGrabBG => return self.check_grab_bg(c) as isize,
            C::EnsureOnTopOf => {
                let p = self.parent(c);
                if p != 0 {
                    self.h(K::PEnsureOnTopOf, p, a, c);
                }
            }
            C::SetDefaultCursor => self.set_own(c, EeWonCompExtra::Cursor, a),
            C::SetDesiredHeight => self.set_desired_height(c, ai),
            C::SetDesiredWidth => self.set_desired_width(c, ai),
            C::SetTranslucentColor => self.set_translucent_color(c, ai, bi),
            C::SetFlags => self.set_flags(c, ai, b != 0),
            C::Subscribe => {
                self.h(K::WSubscribe, self.root(c), c, a);
            }
            C::RequestTimer => self.request_timer(c, a != 0),
            C::RequestFocus => self.request_focus(c),
            C::Root => return self.root(c),
            C::RootContainer => return self.root_container(c),
            C::RootWindow => return self.root_window(c),
            C::FontMod => return self.font_mod(c, a, bi, cc as i32),
        }
        0
    }
}

/// One of Component's methods on the C++ component `me` (`EeWonCompCall`), its calls out through `host`.
///
/// # Safety
/// `me` is a live C++ `Component*` and `host` answers `EeWonGuiOp::Comp` for it; pointer arguments are as the call
/// documents them.
#[no_mangle]
pub unsafe extern "C" fn ee_won_comp_call(
    me: *mut c_void,
    host: EeWonGuiHostFn,
    call: EeWonCompCall,
    a: isize,
    b: isize,
    c: isize,
) -> isize {
    Comp::new(host).call(me as isize, call, a, b, c)
}

#[cfg(test)]
#[path = "compobj_tests.rs"]
mod tests;
