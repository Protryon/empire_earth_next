//! The game's windows' Rust parts in one place (R6 WON GUI round 37).
//!
//! A C++ `Window` is three Rust objects: its window (winobj.rs), its platform window (platwin.rs) and its root
//! container's root (rootobj.rs, over the root's container and component). Each used to reach the others through the
//! C++ shell's host ops (the window's platform calls through `WinPlat`, the platform window's window state through
//! `Open`, `Style`, ..., the managers' through `WmWin` and `EeWonPlatWmOp`), which ran one forwarder each. The
//! game's `Window` constructor binds its parts here (`ee_won_win_bind`); the objects then call each other directly
//! and the C++ cases went. The drivers' worlds bind nothing: their windows still answer every op.
//!
//! The parts live as long as the C++ window: the destructor binds the window again without its platform window
//! when it frees that, and the entry goes with the Rust window (`ee_won_win_free`).

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::sync::Mutex;

use crate::guihost::EeWonGuiOp as Op;
use crate::platwin::{ee_won_plat_win, EeWonPlatWin, EeWonPlatWinCall as P, EeWonPlatWinOp};
use crate::platwm::EeWonPlatWmOp;
use crate::rootobj::EeWonRoot;
use crate::winobj::{EeWonWinCall as C, EeWonWinField as F, EeWonWinPlat, EeWonWindow};
use crate::wmobj::EeWonWmWin;

/// A game window's Rust parts.
#[derive(Clone, Copy)]
pub struct Parts {
    pub win: *const EeWonWindow,
    /// NULL once the destructor freed it
    pub plat: *mut EeWonPlatWin,
    /// the root container (its `Component*`)
    pub root: isize,
    pub root_obj: *const EeWonRoot,
}

// (the addresses as integers: the map is shared, the objects are the GUI thread's)
static WINDOWS: Mutex<BTreeMap<isize, [isize; 4]>> = Mutex::new(BTreeMap::new());

/// The parts of the game window `window` (a `Window*`), None for a driver's.
pub fn parts(window: isize) -> Option<Parts> {
    let w = WINDOWS.lock().ok()?.get(&window).copied()?;
    Some(Parts { win: w[0] as _, plat: w[1] as _, root: w[2], root_obj: w[3] as _ })
}

/// The game window `window`'s parts (`plat` NULL: freed).
///
/// # Safety
/// The parts are the window's live objects until it binds them again or its Rust window is freed.
#[no_mangle]
pub unsafe extern "C" fn ee_won_win_bind(
    window: *mut c_void,
    win: *const EeWonWindow,
    plat: *mut EeWonPlatWin,
    root: *mut c_void,
    root_obj: *const EeWonRoot,
) {
    if let Ok(mut m) = WINDOWS.lock() {
        m.insert(window as isize, [win as isize, plat as isize, root as isize, root_obj as isize]);
    }
}

/// The window's entry gone (its Rust window freed).
pub(crate) fn unbind(window: isize) {
    if let Ok(mut m) = WINDOWS.lock() {
        m.remove(&window);
    }
}

fn plat(p: &Parts, c: P, a: isize, b: isize, cc: isize, d: isize) -> isize {
    if p.plat.is_null() {
        return 0; // Window::Rust's: no platform window
    }
    // SAFETY: the window's live platform window (the bind's contract); the call re-enters through its entry
    unsafe { ee_won_plat_win(p.plat, c, a, b, cc, d) }
}
fn win(p: &Parts, c: C, a: isize, b: isize) -> isize {
    // SAFETY: the window's live Rust window (the bind's contract)
    unsafe { (*p.win).call(c, a, b) }
}
fn field(p: &Parts, f: F) -> isize {
    win(p, C::GetField, f as isize, 0)
}
/// a void call's answer (the C++ case's `return 0`)
fn z(_: isize) -> isize {
    0
}
fn on(x: isize) -> isize {
    isize::from(x != 0)
}
fn comp(p: &Parts, k: crate::compobj::EeWonCompCall, a: isize, b: isize) -> isize {
    // SAFETY: the window's live root container; the call re-enters through Component's entry
    unsafe { crate::compobj::ee_won_comp_call(p.root as *mut c_void, crate::compobj::host(), k, a, b, 0) }
}

/// The window `window`'s own op `op` (winobj.rs's calls out) when the game binds it: its platform window's, another
/// window's.
pub(crate) fn win_op(window: isize, op: Op, a: isize, b: isize, c: isize, d: isize) -> Option<isize> {
    if !matches!(op, Op::WinPlat | Op::WinSetPosSize | Op::WinGetRect | Op::WinIsVisible | Op::WinCanClose) {
        return None;
    }
    let me = parts(window)?;
    Some(match op {
        Op::WinPlat => {
            use EeWonWinPlat as W;
            let (call, arg) = match a {
                x if x == W::SetTitle as isize => (P::SetTitle, 0),
                x if x == W::SetCursor as isize => (P::SetCursor, b),
                x if x == W::ShowNativeCursor as isize => (P::ShowNativeCursor, on(b)),
                x if x == W::Show as isize => (P::Show, on(b)),
                x if x == W::Minimize as isize => (P::Minimize, on(b)),
                x if x == W::SetFocus as isize => (P::SetFocus, 0),
                _ => return Some(0), // SetIcon: window icons have no place on the lobby surface
            };
            z(plat(&me, call, arg, 0, 0, 0))
        }
        Op::WinSetPosSize => z(plat(&me, P::SetPosSize, a, b, c, d)),
        // another window's (`a`)
        Op::WinGetRect => z(plat(&parts(a)?, P::GetRect, b, on(c), 0, 0)),
        Op::WinIsVisible => on(plat(&parts(a)?, P::IsVisible, 0, 0, 0, 0)),
        _ => on(win(&parts(a)?, C::CanClose, 0, 0)),
    })
}

/// The platform window's op on its window `window` (platwin.rs's calls out) when the game binds it.
pub(crate) fn plat_win_op(window: isize, op: EeWonPlatWinOp, a: isize, b: isize) -> Option<isize> {
    use crate::compobj::EeWonCompCall as K;
    use EeWonPlatWinOp as O;
    if !matches!(
        op,
        O::Fills
            | O::Frame
            | O::Popup
            | O::Open
            | O::Style
            | O::Center
            | O::Showing
            | O::Show
            | O::InvalidateRoot
            | O::MouseIn
            | O::RootContains
            | O::ParentPlat
            | O::WindowClose
            | O::WindowTimer
            | O::RootInvalid
            | O::RootInvalidate
    ) {
        return None;
    }
    let me = parts(window)?;
    Some(match op {
        O::Fills => isize::from(field(&me, F::Parent) == 0),
        O::Frame => isize::from(field(&me, F::Parent) != 0 && field(&me, F::Style) & 0x20 == 0),
        O::Popup => on(field(&me, F::IsPopup)),
        O::Open => isize::from(field(&me, F::State) == crate::winobj::EE_WON_WIN_OPEN as isize),
        O::Style => field(&me, F::Style),
        O::Center => z(win(&me, C::Center, on(a), 0)),
        O::Showing => z(win(&me, C::SetField, F::IsShowing as isize, on(a))),
        O::Show => z(plat(&me, P::Show, on(a), 0, 0, 0)),
        O::InvalidateRoot => z(win(&me, C::InvalidateRoot, 0, 0)),
        O::MouseIn => on(field(&me, F::InClient)),
        O::RootContains => on(comp(&me, K::Contains, a, b)),
        O::ParentPlat => parts(field(&me, F::Parent)).map_or(0, |p| p.plat as isize),
        O::WindowClose => z(win(&me, C::PlatformClose, 0, 0)),
        O::WindowTimer => z(win(&me, C::TimerEvent, a as u32 as isize, 0)),
        O::RootInvalid => isize::from(crate::compmem::get(me.root).invalid != 0),
        _ => z(comp(&me, K::Invalidate, 0, 0)),
    })
}

/// The manager's call on the window `window` (wmobj.rs's `WmWin`) when the game binds it.
pub(crate) fn wm_win_op(call: EeWonWmWin, window: isize, c: isize, d: isize) -> Option<isize> {
    use crate::compobj::EeWonCompCall as K;
    use crate::ctobj::{ee_won_ct_call, EeWonContainer, EeWonCtCall as T};
    use crate::rootobj::{ee_won_root_call, EeWonRootCall as R};
    use EeWonWmWin as W;
    if matches!(call, W::Hold | W::Release | W::HandleMouse | W::HandleKey | W::HandleInput | W::CloseEvent) {
        return None;
    }
    let me = parts(window)?;
    Some(match call {
        W::SetFocus => z(plat(&me, P::SetFocus, 0, 0, 0, 0)),
        W::SetCapture => z(plat(&me, P::SetCapture, on(c), 0, 0, 0)),
        W::GetRect => z(plat(&me, P::GetRect, c, on(d), 0, 0)),
        W::IsPopup => on(field(&me, F::IsPopup)),
        W::SetIsPopup => z(win(&me, C::SetField, F::IsPopup as isize, on(c))),
        W::Show => z(win(&me, C::Show, on(c), 0)),
        W::Close => z(win(&me, C::Close, 0, 0)),
        W::SetPosSize => {
            // SAFETY: `c` is the rectangle (the call's contract)
            let r = unsafe { *(c as *const crate::guictl::EeWonRect4) };
            z(plat(&me, P::SetPosSize, r.x as isize, r.y as isize, r.w as isize, r.h as isize))
        }
        W::Parent => field(&me, F::Parent),
        W::AddComponent => z(win(&me, C::AddComponent, c, 0)),
        W::SetModal => z(win(&me, C::SetModal, on(c), 0)),
        W::IsMouseDown => on(win(&me, C::IsMouseDown, c, 0)),
        W::MouseX => field(&me, F::MouseX),
        W::MouseY => field(&me, F::MouseY),
        W::Root => me.root,
        W::RootLostFocus => z(comp(&me, K::LostFocus, 0, 0)),
        W::RootGotFocus => z(comp(&me, K::GotFocus, 0, 0)),
        // SAFETY: the root's live container
        W::RootRemoveAll => z(unsafe {
            ee_won_ct_call(crate::compmem::get(me.root).ct as *const EeWonContainer, T::RemoveAllChildren, 0, 0, 0)
        }),
        // SAFETY: the root's live Rust root
        W::RootRemoveChild => z(unsafe { ee_won_root_call(me.root_obj, R::RemoveChild, c, 0, 0) }),
        W::State => field(&me, F::State),
        W::IsVisible => on(plat(&me, P::IsVisible, 0, 0, 0, 0)),
        W::MinimizeOnRestore => on(field(&me, F::MinimizeOnRestore)),
        W::Minimize => z(win(&me, C::Minimize, on(c), 0)),
        W::InClient => on(field(&me, F::InClient)),
        W::PlatformClose => z(plat(&me, P::Close, 0, 0, 0, 0)),
        W::TimerEvent => z(plat(&me, P::Timer, c as u32 as isize, 0, 0, 0)),
        W::IdleUpdate => z(win(&me, C::IdleUpdate, 0, 0)),
        _ => return None,
    })
}

/// The platform manager's op on the window `window` (platwm.rs's calls out) when the game binds it.
pub(crate) fn plat_wm_op(op: EeWonPlatWmOp, window: isize, b: isize, c: isize) -> Option<isize> {
    use EeWonPlatWmOp as O;
    if !matches!(
        op,
        O::Info
            | O::SetRegistered
            | O::FollowSurface
            | O::FollowPixelScale
            | O::QueueMouse
            | O::Parent
            | O::Close
            | O::SetFocus
            | O::SetPosSize
            | O::Paint
            | O::Title
    ) {
        return None;
    }
    let me = parts(window)?;
    Some(match op {
        O::Info => z(plat(&me, P::Info, b, 0, 0, 0)),
        O::SetRegistered => z(plat(&me, P::SetRegistered, on(b), 0, 0, 0)),
        O::FollowSurface => z(plat(&me, P::FollowSurface, 0, 0, 0, 0)),
        O::FollowPixelScale => z(plat(&me, P::FollowPixelScale, b, 0, 0, 0)),
        O::QueueMouse => z(plat(&me, P::QueueMouseAt, b, c, 0, 0)),
        O::Parent => field(&me, F::Parent),
        O::Close => z(win(&me, C::Close, 0, 0)),
        O::SetFocus => z(plat(&me, P::SetFocus, 0, 0, 0, 0)),
        O::SetPosSize => z(plat(&me, P::SetPosSize, b, c, -1, -1)),
        O::Paint => z(plat(&me, P::Paint, 0, 0, 0, 0)),
        // SAFETY: the window's live platform window; `b` the count's int (the op's contract)
        _ => unsafe { crate::platwin::ee_won_plat_win_title(me.plat, b as *mut i32) as isize },
    })
}
