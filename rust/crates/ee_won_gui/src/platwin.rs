//! PlatGUIWindow (WONLobbyPlatform.h) as a whole Rust object (R6 WON round 41): the WONAPI window on the lobby
//! surface. Its rectangle (frame included) and flags, its client rectangle (platwm.rs `client_rect`), and its
//! methods: Create (the rectangle in the surface, the root at its client size, the centring and the first show the
//! style asks for), following the surface's size, moves and resizes with their size events, show and hide with
//! the focus they move, minimize, top-most, close, focus, capture, the cursor while the pointer is inside, the
//! mouse-exit check of the timer, the repaint's dirty mark and the mouse events in client coordinates.
//!
//! The style keeps MSWindow::Create's bits: 0x01 centre, 0x02 start hidden, 0x20 no frame, 0x40 the rectangle is
//! the client area, 0x80 default position. A window without a parent fills the surface.
//!
//! The C++ class keeps the type (a Window: the WONAPI code and the manager hold it) and a host of 26 ops
//! (`EeWonPlatWinOp`: the Window base's calls, its root and events, the manager's calls, the cursor's dynamic type,
//! the platform's pointer and minimize).
//! The manager is called through the host (its C++ shell's `Call`), so no Rust reference to this window lives
//! across a call that can come back to it: each method reads and writes the fields through the raw pointer.

use std::ffi::c_void;

use crate::platwm::{EeWonPlatWinInfo, EeWonPlatWmCall};

/// What the window asks of its C++ shell (`me`).
#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonPlatWinOp {
    /// The window has a PlatWindowManager (`GetPlatManager() != NULL`).
    HasWm = 0,
    /// The manager's `Call(a, b, c)`.
    Wm = 1,
    /// `GetMousePosition(x, y)` into `a` (two ints) -> whether the pointer is over the surface.
    Mouse = 2,
    /// `FillsSurface()`.
    Fills = 3,
    /// `HasFrame()`.
    Frame = 4,
    /// `IsPopup()`.
    Popup = 5,
    /// `GetWindowState() == WindowState_Open`.
    Open = 6,
    /// `GetStyle()`.
    Style = 7,
    /// `Window::Create(WONRectangle(a..d))`, the display context and the offscreen image of the root's size.
    WindowCreate = 8,
    /// `Center(a)`.
    Center = 9,
    /// `SetIsShowing(a)`.
    Showing = 10,
    /// `PlatformShow(a)` (the virtual call).
    Show = 11,
    /// `QueueEvent(new SizeEvent(a, b, c, d))`.
    QueueSize = 12,
    /// `QueueEvent(new InputEvent(InputEvent_MouseExit))`.
    QueueExit = 13,
    /// `QueueEvent(new MouseEvent(p[0] .. p[4]))`, `a` five ints.
    QueueMouse = 14,
    /// `InvalidateRoot()`.
    InvalidateRoot = 15,
    /// `IsMouseInClientArea()`.
    MouseIn = 16,
    /// `mRoot->Contains(a, b)`.
    RootContains = 17,
    /// The root repainted into the offscreen image when it is invalid and there is one -> whether it was.
    Paint = 18,
    /// The parent when it is a PlatGUIWindow and visible, else 0.
    ParentVisible = 19,
    /// `Window::PlatformClose()`.
    WindowClose = 20,
    /// `Window::TimerEvent(a)`.
    WindowTimer = 21,
    /// The cursor `a` when it is a PlatCursor: the system pointer shown and the cursor applied.
    ApplyCursor = 22,
    /// Whether the manager's popup window does not want the mouse and is the window `a`.
    PopupHit = 23,
    /// `PlatCursorShow(a)`.
    CursorShow = 24,
    /// `PlatWindowMinimize((PlatWindow*)a)`.
    PlatMinimize = 25,
}

pub type EeWonPlatWinHost =
    extern "C" fn(me: *mut c_void, op: EeWonPlatWinOp, a: isize, b: isize, c: isize, d: isize) -> isize;

/// The window's calls (`ee_won_plat_win`).
#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonPlatWinCall {
    /// The destructor's: off the manager when registered.
    Destroy = 0,
    /// `Create(rect a)` (four ints).
    Create = 1,
    FollowSurface = 2,
    /// `SetPosSize(a, b, c, d)`.
    SetPosSize = 3,
    /// `GetRect(out a, b)` (four ints; b: the client area).
    GetRect = 4,
    SetTitle = 5,
    /// `PlatformSetCursor(a)`.
    SetCursor = 6,
    /// `PlatformShowNativeCursor(a)`.
    ShowNativeCursor = 7,
    /// `PlatformShow(a)`.
    Show = 8,
    /// `PlatformMinimize(a)`.
    Minimize = 9,
    /// `PlatformSetTopMost(a)`.
    SetTopMost = 10,
    Close = 11,
    SetFocus = 12,
    /// `PlatformSetMouseCapture(a)`.
    SetCapture = 13,
    IsVisible = 14,
    /// `TimerEvent(a)`.
    Timer = 15,
    /// `OnPaint()`.
    Paint = 16,
    /// `QueueMouse(a: five ints)`.
    QueueMouse = 17,
    /// The fields the manager reads into `a` (EeWonPlatWinInfo).
    Info = 18,
    /// `mRegistered = a`.
    SetRegistered = 19,
    /// `SetMinimizeOnAltTab(a)`.
    SetMinimizeOnAltTab = 20,
    /// `SetForwardKeyMsgToParent(a)`.
    SetForwardKeys = 21,
}

/// The window's own state.
pub struct EeWonPlatWin {
    me: *mut c_void,
    host: EeWonPlatWinHost,
    x: i32,
    y: i32,
    width: i32,
    height: i32,
    visible: bool,
    top_most: bool,
    registered: bool,
    minimize_on_alt_tab: bool,
    forward_keys: bool,
}

/// the classic dialog frame: a 3-pixel border and an 18-pixel caption
const FRAME_BORDER: i32 = 3;
const FRAME_CAPTION: i32 = 18;

type W = *mut EeWonPlatWin;
type Op = EeWonPlatWinOp;

// SAFETY (every accessor): `w` is a live window; each borrow ends before the next call out.
fn st(w: W) -> &'static mut EeWonPlatWin {
    unsafe { &mut *w }
}
fn host(w: W, op: Op, a: isize, b: isize, c: isize, d: isize) -> isize {
    let (me, h) = (st(w).me, st(w).host);
    h(me, op, a, b, c, d)
}
fn ask(w: W, op: Op) -> bool {
    host(w, op, 0, 0, 0, 0) != 0
}
fn wm(w: W, call: EeWonPlatWmCall, a: isize, b: isize) -> isize {
    host(w, Op::Wm, call as isize, a, b, 0)
}
fn dirty(w: W) {
    if ask(w, Op::HasWm) {
        wm(w, EeWonPlatWmCall::SetDirty, 0, 0);
    }
}

/// `GetInfo`: the fields the manager reads.
fn info(w: W) -> EeWonPlatWinInfo {
    let s = st(w);
    let (x, y, width, height, visible, top_most, registered) =
        (s.x, s.y, s.width, s.height, s.visible, s.top_most, s.registered);
    let (minimize_on_alt_tab, forward_keys) = (s.minimize_on_alt_tab, s.forward_keys);
    EeWonPlatWinInfo {
        x,
        y,
        width,
        height,
        visible,
        top_most,
        registered,
        popup: ask(w, Op::Popup),
        fills: ask(w, Op::Fills),
        frame: ask(w, Op::Frame),
        open: ask(w, Op::Open),
        minimize_on_alt_tab,
        forward_keys,
    }
}

/// `GetClientRect`.
fn client(w: W) -> [i32; 4] {
    crate::platwm::client_rect(&info(w))
}

fn create(w: W, r: [i32; 4]) {
    let mgr = ask(w, Op::HasWm);
    if ask(w, Op::Fills) && mgr {
        let (sw, sh) = (wm(w, EeWonPlatWmCall::SurfaceWidth, 0, 0), wm(w, EeWonPlatWmCall::SurfaceHeight, 0, 0));
        let s = st(w);
        (s.x, s.y, s.width, s.height) = (0, 0, sw as i32, sh as i32);
    } else {
        {
            let s = st(w);
            (s.x, s.y, s.width, s.height) = (r[0], r[1], r[2], r[3]);
        }
        if ask(w, Op::Frame) && host(w, Op::Style, 0, 0, 0, 0) & 0x40 != 0 {
            let s = st(w);
            s.width += 2 * FRAME_BORDER;
            s.height += 2 * FRAME_BORDER + FRAME_CAPTION;
        }
    }
    let c = client(w);
    host(w, Op::WindowCreate, c[0] as isize, c[1] as isize, c[2] as isize, c[3] as isize);
    if mgr {
        wm(w, EeWonPlatWmCall::Add, st(w).me as isize, 0);
    }
    if !ask(w, Op::Fills) {
        let style = host(w, Op::Style, 0, 0, 0, 0);
        if style & 0x01 != 0 {
            host(w, Op::Center, 1, 0, 0, 0);
        } else if style & 0x80 != 0 {
            host(w, Op::Center, 0, 0, 0, 0);
        }
    }
    if host(w, Op::Style, 0, 0, 0, 0) & 0x02 == 0 {
        host(w, Op::Showing, 1, 0, 0, 0);
        host(w, Op::Show, 1, 0, 0, 0);
    } else {
        host(w, Op::Showing, 0, 0, 0, 0);
    }
}

fn follow_surface(w: W) {
    if !ask(w, Op::Fills) || !ask(w, Op::HasWm) || !ask(w, Op::Open) {
        return;
    }
    let (sw, sh) =
        (wm(w, EeWonPlatWmCall::SurfaceWidth, 0, 0) as i32, wm(w, EeWonPlatWmCall::SurfaceHeight, 0, 0) as i32);
    let s = st(w);
    if sw == s.width && sh == s.height {
        return;
    }
    (s.x, s.y, s.width, s.height) = (0, 0, sw, sh);
    host(w, Op::QueueSize, -1, -1, sw as isize, sh as isize);
    wm(w, EeWonPlatWmCall::SetDirty, 0, 0);
}

fn set_pos_size(w: W, x: i32, y: i32, mut width: i32, mut height: i32) {
    if !ask(w, Op::Open) || ask(w, Op::Fills) {
        return;
    }
    if width == -1 {
        width = st(w).width;
    }
    if height == -1 {
        height = st(w).height;
    }
    let old = client(w);
    {
        let s = st(w);
        (s.x, s.y, s.width, s.height) = (x, y, width, height);
    }
    let new = client(w);
    if new[2] != old[2] || new[3] != old[3] {
        host(w, Op::QueueSize, -1, -1, new[2] as isize, new[3] as isize);
    }
    if new[0] != old[0] || new[1] != old[1] {
        host(w, Op::QueueSize, new[0] as isize, new[1] as isize, -1, -1);
    }
    dirty(w);
}

fn set_cursor(w: W, cursor: isize) {
    let mgr = ask(w, Op::HasWm);
    if !ask(w, Op::MouseIn) && (!mgr || wm(w, EeWonPlatWmCall::Capture, 0, 0) != st(w).me as isize) {
        return;
    }
    host(w, Op::ApplyCursor, cursor, 0, 0, 0);
}

fn show(w: W, on: bool) {
    let mgr = ask(w, Op::HasWm);
    if st(w).visible == on {
        return;
    }
    st(w).visible = on;
    if !mgr {
        return;
    }
    wm(w, EeWonPlatWmCall::SetDirty, 0, 0);
    let me = st(w).me as isize;
    if on {
        host(w, Op::InvalidateRoot, 0, 0, 0, 0);
        if !ask(w, Op::Popup) {
            wm(w, EeWonPlatWmCall::BringToFront, me, 0);
            wm(w, EeWonPlatWmCall::SetFocus, me, 0);
        }
    } else if wm(w, EeWonPlatWmCall::Focus, 0, 0) == me {
        let next = host(w, Op::ParentVisible, 0, 0, 0, 0);
        wm(w, EeWonPlatWmCall::SetFocus, next, 0);
    }
}

fn timer(w: W, elapsed: isize) {
    host(w, Op::WindowTimer, elapsed, 0, 0, 0);
    if !ask(w, Op::MouseIn) || !ask(w, Op::HasWm) {
        return;
    }
    let mut p = [-100000i32, -100000];
    let mut hit = 0;
    if host(w, Op::Mouse, p.as_mut_ptr() as isize, 0, 0, 0) != 0 {
        hit = wm(w, EeWonPlatWmCall::WindowFromPoint, p[0] as isize, p[1] as isize);
    }
    let c = client(w);
    let me = st(w).me as isize;
    if hit == me && host(w, Op::RootContains, (p[0] - c[0]) as isize, (p[1] - c[1]) as isize, 0, 0) != 0 {
        return;
    }
    if hit != me && host(w, Op::PopupHit, hit, 0, 0, 0) != 0 {
        return;
    }
    host(w, Op::QueueExit, 0, 0, 0, 0);
}

/// One call.
fn call(w: W, c: EeWonPlatWinCall, a: isize, b: isize, cc: isize, d: isize) -> isize {
    type C = EeWonPlatWinCall;
    let ints = |p: isize, n: usize| -> &'static mut [i32] {
        // SAFETY: the caller's n ints (the call's contract)
        unsafe { std::slice::from_raw_parts_mut(p as *mut i32, n) }
    };
    match c {
        C::Destroy => {
            if st(w).registered && ask(w, Op::HasWm) {
                wm(w, EeWonPlatWmCall::Remove, st(w).me as isize, 0);
            }
        }
        C::Create => {
            let r = ints(a, 4);
            create(w, [r[0], r[1], r[2], r[3]]);
        }
        C::FollowSurface => follow_surface(w),
        C::SetPosSize => set_pos_size(w, a as i32, b as i32, cc as i32, d as i32),
        C::GetRect => {
            let r = if b != 0 {
                client(w)
            } else {
                let s = st(w);
                [s.x, s.y, s.width, s.height]
            };
            ints(a, 4).copy_from_slice(&r);
        }
        C::SetTitle => dirty(w),
        C::SetCursor => set_cursor(w, a),
        C::ShowNativeCursor => {
            if a != 0 {
                host(w, Op::CursorShow, 1, 0, 0, 0);
            } else if ask(w, Op::MouseIn) {
                host(w, Op::CursorShow, 0, 0, 0, 0);
            }
        }
        C::Show => show(w, a != 0),
        C::Minimize => {
            if a != 0 && ask(w, Op::Fills) && ask(w, Op::HasWm) && wm(w, EeWonPlatWmCall::PlatWindow, 0, 0) != 0 {
                let p = wm(w, EeWonPlatWmCall::PlatWindow, 0, 0);
                host(w, Op::PlatMinimize, p, 0, 0, 0);
            }
        }
        C::SetTopMost => {
            st(w).top_most = a != 0;
            dirty(w);
        }
        C::Close => {
            host(w, Op::WindowClose, 0, 0, 0, 0);
            st(w).visible = false;
            if st(w).registered && ask(w, Op::HasWm) {
                wm(w, EeWonPlatWmCall::Remove, st(w).me as isize, 0);
            }
        }
        C::SetFocus => {
            if !ask(w, Op::HasWm) || !ask(w, Op::Open) {
                return 0;
            }
            let me = st(w).me as isize;
            if !ask(w, Op::Popup) {
                wm(w, EeWonPlatWmCall::BringToFront, me, 0);
            }
            wm(w, EeWonPlatWmCall::SetFocus, me, 0);
        }
        C::SetCapture => {
            if ask(w, Op::HasWm) {
                let me = st(w).me as isize;
                wm(w, EeWonPlatWmCall::SetCapture, if a != 0 { me } else { 0 }, 0);
            }
        }
        C::IsVisible => return isize::from(st(w).visible),
        C::Timer => timer(w, a),
        C::Paint => {
            if ask(w, Op::Paint) {
                dirty(w);
            }
        }
        C::QueueMouse => {
            let p = ints(a, 5);
            let (t, sx, sy, m, n) = (p[0], p[1], p[2], p[3], p[4]);
            let c = client(w);
            let q = [t, sx - c[0], sy - c[1], m, n];
            host(w, Op::QueueMouse, q.as_ptr() as isize, 0, 0, 0);
        }
        C::Info => {
            let i = info(w);
            // SAFETY: the caller's EeWonPlatWinInfo
            unsafe { *(a as *mut EeWonPlatWinInfo) = i };
        }
        C::SetRegistered => st(w).registered = a != 0,
        C::SetMinimizeOnAltTab => st(w).minimize_on_alt_tab = a != 0,
        C::SetForwardKeys => st(w).forward_keys = a != 0,
    }
    0
}

/// A new window for the shell `me` and its host.
#[no_mangle]
pub extern "C" fn ee_won_plat_win_new(me: *mut c_void, host: EeWonPlatWinHost) -> *mut EeWonPlatWin {
    Box::into_raw(Box::new(EeWonPlatWin {
        me,
        host,
        x: 0,
        y: 0,
        width: 0,
        height: 0,
        visible: false,
        top_most: false,
        registered: false,
        minimize_on_alt_tab: false,
        forward_keys: false,
    }))
}

/// # Safety
/// `w` came from `ee_won_plat_win_new`, freed once (after its Destroy call).
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_win_free(w: *mut EeWonPlatWin) {
    if !w.is_null() {
        // SAFETY: the contract
        drop(unsafe { Box::from_raw(w) });
    }
}

/// One of the window's calls (`EeWonPlatWinCall`, its arguments as each says).
///
/// # Safety
/// `w` is a live window; the pointers a call reads or writes are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_plat_win(
    w: *mut EeWonPlatWin,
    c: EeWonPlatWinCall,
    a: isize,
    b: isize,
    cc: isize,
    d: isize,
) -> isize {
    call(w, c, a, b, cc, d)
}

#[cfg(test)]
#[path = "platwin_tests.rs"]
mod tests;
