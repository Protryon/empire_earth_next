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
use crate::platwm::EeWonPlatWm;
use crate::wmobj::{EeWonWm, EeWonWmCall, EeWonWmField, EeWonWmWin};

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

// the game's reference count calls (RefCount::AddRef / Release on any of its objects, ComponentLifetime.cpp)
static REF: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);

/// The game's reference function: `f(object, 1)` takes a reference to a `RefCount` object, `f(object, 0)` gives one
/// back (the object deleted with its last).
#[no_mangle]
pub extern "C" fn ee_won_gui_ref_register(f: Option<unsafe extern "C" fn(object: *mut c_void, hold: i32)>) {
    REF.store(f.map_or(0, |f| f as usize), std::sync::atomic::Ordering::Release);
}

/// A reference taken (`hold`) or given back in the game (R6 WON GUI round 37: the hosts' Hold and Release cases,
/// one statement each, went); None in a driver's world, whose host answers.
pub(crate) fn rf(object: isize, hold: bool) -> Option<isize> {
    let f = REF.load(std::sync::atomic::Ordering::Acquire);
    if f == 0 {
        return None;
    }
    // SAFETY: a value ee_won_gui_ref_register stored from the game's function
    let f = unsafe { std::mem::transmute::<usize, unsafe extern "C" fn(*mut c_void, i32)>(f) };
    // SAFETY: a live RefCount object (the op's contract)
    unsafe { f(object as *mut c_void, i32::from(hold)) };
    Some(0)
}

/// The window manager's host's reference ops (its own, its fonts', images' and events'), for guisplit.rs's game host.
pub(crate) fn wm_ref_op(manager: isize, op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    match op {
        Op::WmFontHold | Op::WmRawHold | Op::WmImageHold => rf(a, true),
        Op::WmFontRelease | Op::WmRawRelease | Op::WmImageRelease => rf(a, false),
        Op::WmSelfHold => rf(manager, true),
        Op::WmSelfRelease => rf(manager, false),
        Op::WmRef => rf(b, c != 0),
        Op::WmEvent if a == crate::wmobj::EeWonWmEvent::Hold as isize => rf(b, true),
        Op::WmEvent if a == crate::wmobj::EeWonWmEvent::Release as isize => rf(b, false),
        _ => None,
    }
}

// the game's window manager (the default one): its C++ object, its Rust manager and its platform manager
static MANAGER: Mutex<[isize; 3]> = Mutex::new([0; 3]);

/// The game's window manager's Rust parts (`plat` NULL: freed, as the destructor does; `wm` NULL: gone).
///
/// # Safety
/// The parts are the manager's live objects until it binds them again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wm_bind(manager: *mut c_void, wm: *const EeWonWm, plat: *const EeWonPlatWm) {
    if let Ok(mut m) = MANAGER.lock() {
        *m = [manager as isize, wm as isize, plat as isize];
    }
}

/// The manager's entry gone (its Rust manager freed).
pub(crate) fn unbind_manager(wm: isize) {
    if let Ok(mut m) = MANAGER.lock() {
        if m[1] == wm {
            *m = [0; 3];
        }
    }
}
fn manager() -> Option<(*const EeWonWm, *const EeWonPlatWm)> {
    let m = *MANAGER.lock().ok()?;
    (m[1] != 0).then_some((m[1] as *const EeWonWm, m[2] as *const EeWonPlatWm))
}
/// The platform manager a bound window `window` calls (`GetPlatManager`; NULL once freed), None for a driver's window.
pub(crate) fn plat_manager_of(window: isize) -> Option<*const EeWonPlatWm> {
    parts(window)?;
    Some(manager().map_or(std::ptr::null(), |m| m.1))
}
fn wm(c: EeWonWmCall, a: isize, b: isize) -> Option<isize> {
    let (w, _) = manager()?;
    // SAFETY: the manager's live Rust manager (the bind's contract)
    Some(unsafe { crate::wmobj::ee_won_wm_call(w, c, a, b, 0, 0) })
}
fn wm_field(f: EeWonWmField) -> Option<isize> {
    wm(EeWonWmCall::GetField, f as isize, 0)
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
    match op {
        Op::WinAddToManager => return wm(EeWonWmCall::AddWindow, window, 0).map(z),
        Op::WinScreenRect => {
            let (_, p) = manager()?;
            // SAFETY: the manager's live platform manager; `a` the rectangle (the op's contract)
            unsafe { crate::platloop::ee_won_plat_wm_screen_rect(p, a as *mut i32) };
            return Some(0);
        }
        // (no window has an icon, and the manager has no default one)
        Op::WinHasIcon | Op::WinIconDefault => return parts(window).map(|_| 0),
        Op::WinFontHold => return rf(a, true),
        Op::WinChildSet => return parts(a).map(|p| z(win(&p, C::ChildSet, window, b))),
        _ => {}
    }
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
    if parts(window).is_some() {
        // the window's manager (the default one, while its platform part lives: GetPlatManager)
        let plat_wm = manager().map_or(std::ptr::null(), |m| m.1);
        match op {
            O::HasWm => return Some(isize::from(!plat_wm.is_null())),
            // SAFETY: the manager's live platform manager; `a` the mouse's two ints (the op's contract)
            O::Mouse => {
                return Some(isize::from(unsafe {
                    crate::platwm::ee_won_plat_wm_mouse(plat_wm, a as *mut i32, (a as *mut i32).add(1))
                }))
            }
            O::PopupWindow => return wm(EeWonWmCall::PopupWindow, 0, 0),
            O::PopupFlags => return wm_field(EeWonWmField::PopupFlags),
            _ => {}
        }
    }
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
    match call {
        W::Hold => return rf(window, true),
        W::Release => return rf(window, false),
        W::HandleMouse | W::HandleKey | W::HandleInput | W::CloseEvent => return None,
        _ => {}
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
    match op {
        O::NotModal => return parts(window).and_then(|_| wm(EeWonWmCall::NotModalWindow, window, 0)),
        O::Hold => return rf(window, true),
        O::Release => return rf(window, false),
        _ => {}
    }
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

/// Component's calls out (compobj.rs `EeWonComp`) the game answers from its Rust parts: the parent container's
/// non-virtual calls, the window's state and the manager's; None for the others (the C++ host's).
pub(crate) fn comp_op(k: crate::compobj::EeWonComp, b: isize, c: isize, d: isize) -> Option<isize> {
    use crate::compobj::EeWonComp as K;
    use crate::ctobj::{ee_won_ct_call, EeWonContainer, EeWonCtCall as T, EeWonCtField};
    match k {
        K::RefHold | K::EvHold => rf(b, true),
        K::RefDrop | K::EvRelease => rf(b, false),
        K::PNextChild | K::PIsModal | K::PEnsureOnTopOf => {
            if !crate::compobj::is_game() {
                return None;
            }
            let ct = crate::compmem::get(b).ct as *const EeWonContainer;
            if ct.is_null() {
                return None;
            }
            // SAFETY: the parent's live container (`b` a game container)
            let call = |t: T, x: isize, y: isize, w: isize| unsafe { ee_won_ct_call(ct, t, x, y, w) };
            Some(match k {
                K::PNextChild => call(T::NextChild, c, d & 1, (d >> 1) & 1),
                K::PIsModal => isize::from(call(T::GetField, EeWonCtField::ModalCount as isize, 0, 0) > 0),
                _ => z(call(T::EnsureOnTopOf, c, d, 0)),
            })
        }
        K::DefaultWindow => wm_field(EeWonWmField::DefaultWindow),
        K::WSubscribe => {
            parts(b)?;
            wm(EeWonWmCall::SubscribeToBroadcast, c, on(d)).map(z)
        }
        K::WState | K::WParent | K::WRoot | K::WMouse | K::WKeyMod | K::WDoubleClick => {
            let me = parts(b)?;
            Some(match k {
                K::WState => field(&me, F::State),
                K::WParent => field(&me, F::Parent),
                K::WRoot => me.root,
                K::WMouse => {
                    let (x, y) = (field(&me, F::MouseX) as i32, field(&me, F::MouseY) as i32);
                    // SAFETY: `c` is two ints (the op's contract)
                    unsafe { *(c as *mut [i32; 2]) = [x, y] };
                    0
                }
                K::WKeyMod => field(&me, F::KeyMod),
                _ => on(win(&me, C::CheckDoubleClick, c, 0)),
            })
        }
        _ => None,
    }
}

/// The component services every GUI object asks its host for (`ComponentOp`'s and Container's host's), answered
/// in the game by Component's and the container's own calls (R6 WON GUI round 37); `comp` is the asking object's
/// component. None in a driver's world and for the others (virtuals, references, the Graphics).
pub(crate) fn common_op(comp: isize, op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    use crate::compobj::EeWonCompCall as K;
    use crate::guihost::EeWonGuiField as Fd;
    match op {
        Op::ChildHold | Op::CtCursorHold | Op::RootImageHold => return rf(a, true),
        Op::ChildRelease | Op::CtCursorRelease | Op::RootImageRelease => return rf(a, false),
        _ => {}
    }
    if !matches!(
        op,
        Op::Get
            | Op::RequestTimer
            | Op::RequestFocus
            | Op::GetMousePos
            | Op::KeyMod
            | Op::SetSize
            | Op::SetDefaultCursor
            | Op::CtorSetFlags
            | Op::ChildSetFlags
            | Op::CtGrabBG
            | Op::CtOwnScheme
            | Op::CtRoot
            | Op::CtRootFocused
            | Op::CtMouseInClient
            | Op::CtDefaultCursor
            | Op::CtWantFocus
            | Op::CtNextSibling
            | Op::CtContainerOf
            | Op::CtAddBG
    ) || !crate::compobj::is_game()
    {
        return None;
    }
    // SAFETY: the game's live components (the ops' contracts); each call re-enters through Component's entry
    let kc = |x: isize, k: K, p: isize, q: isize| unsafe {
        crate::compobj::ee_won_comp_call(x as *mut c_void, crate::compobj::host(), k, p, q, 0)
    };
    Some(match op {
        Op::Get if a == Fd::Width as isize => crate::compmem::get(comp).bounds.width as isize,
        Op::Get if a == Fd::Height as isize => crate::compmem::get(comp).bounds.height as isize,
        Op::Get => return None,
        Op::RequestTimer => z(kc(comp, K::RequestTimer, on(a), 0)),
        Op::RequestFocus => z(kc(comp, K::RequestFocus, 0, 0)),
        Op::GetMousePos => z(kc(comp, K::MousePos, a, b)),
        Op::KeyMod => kc(comp, K::KeyMod, 0, 0),
        Op::SetSize => z(kc(comp, K::SetSize, a, b)),
        Op::SetDefaultCursor => z(kc(comp, K::SetDefaultCursor, crate::guihost::ee_won_gui_cursor(a), 0)),
        Op::CtorSetFlags => z(kc(comp, K::SetFlags, a, 1)),
        Op::ChildSetFlags => z(kc(a, K::SetFlags, b, on(c))),
        Op::CtGrabBG => kc(a, K::GrabBG, 0, 0),
        Op::CtOwnScheme => kc(a, K::OwnColorScheme, 0, 0),
        Op::CtRoot => kc(comp, K::RootContainer, 0, 0),
        Op::CtRootFocused => {
            let w = kc(comp, K::Root, 0, 0);
            isize::from(wm_field(EeWonWmField::FocusWindow)? == w)
        }
        Op::CtMouseInClient => on(field(&parts(kc(comp, K::Root, 0, 0))?, F::InClient)),
        Op::CtDefaultCursor => kc(comp, K::DefaultCursor, 0, 0),
        Op::CtWantFocus => on(kc(comp, K::WantFocus, 0, 0)),
        Op::CtNextSibling => kc(a, K::NextSibling, on(b), on(c)),
        Op::CtContainerOf => {
            if a == 0 {
                0
            } else {
                crate::compmem::get(a).ct
            }
        }
        _ => {
            // CtAddBG: the background queued on the root container (none for a container not yet under a root)
            let r = kc(comp, K::RootContainer, 0, 0);
            if r != 0 {
                let obj = root_obj_of(r)?;
                // SAFETY: the root's live Rust root
                unsafe { crate::rootobj::ee_won_root_call(obj, crate::rootobj::EeWonRootCall::AddBG, a, 0, 0) };
            }
            0
        }
    })
}

/// The Rust root of the game's root container `root`.
fn root_obj_of(root: isize) -> Option<*const EeWonRoot> {
    let m = WINDOWS.lock().ok()?;
    m.values().find(|w| w[2] == root).map(|w| w[3] as *const EeWonRoot)
}
