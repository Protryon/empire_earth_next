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
use crate::platwm::EeWonPlatWm;
use crate::platwm::EeWonPlatWmOp;
use crate::rootobj::EeWonRoot;
use crate::winobj::{EeWonWinCall, EeWonWinCall as C, EeWonWinField as F, EeWonWinPlat, EeWonWindow};
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

// (the addresses as integers: the map is shared, the objects are the GUI thread's); the fifth is the window's
// offscreen image (one reference to its handle: the C++ member `mOffscreenImage` until R6 WON GUI round 40)
static WINDOWS: Mutex<BTreeMap<isize, [isize; 5]>> = Mutex::new(BTreeMap::new());

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
        let offscreen = m.get(&(window as isize)).map_or(0, |w| w[4]);
        m.insert(window as isize, [win as isize, plat as isize, root as isize, root_obj as isize, offscreen]);
    }
}

/// The game window `window`'s offscreen image (its handle, borrowed; 0 for none).
pub(crate) fn offscreen(window: isize) -> isize {
    WINDOWS.lock().ok().and_then(|m| m.get(&window).map(|w| w[4])).unwrap_or(0)
}
/// The offscreen image replaced by `image` (its reference given to the window), the old one's given back.
fn set_offscreen(window: isize, image: isize) {
    let old = WINDOWS.lock().ok().and_then(|mut m| m.get_mut(&window).map(|w| std::mem::replace(&mut w[4], image)));
    if let Some(o) = old.filter(|&o| o != 0) {
        rf(o, false);
    }
}
/// The image's Graphics (`GetGraphics()`; 0 without an image).
fn graphics_of(image: isize) -> isize {
    // SAFETY: a game image handle or 0
    unsafe { crate::imgobj::image_of(image) }.map_or(0, |i| i.graphics())
}
/// `CreateImage(w, h)`: a new offscreen image of that size (a handle with one reference; 0 without the image host).
pub(crate) fn new_image(w: isize, h: isize) -> isize {
    let f = IMAGE_HOST.load(std::sync::atomic::Ordering::Acquire);
    if f == 0 {
        return 0;
    }
    // SAFETY: a value ee_won_wm_bind stored from the game's image host
    let f = unsafe { std::mem::transmute::<usize, crate::imgobj::EeWonImageHostFn>(f) };
    f(crate::imgobj::EeWonImageHost::CreateImage, 0, w, h, 0)
}
/// The test fixture's input event `ev` for the game window `window` and the handler (its `new MouseEvent` and
/// `new KeyEvent`); nothing for a window no game binds.
///
/// # Safety
/// `ev` points at a record.
#[no_mangle]
pub unsafe extern "C" fn ee_won_win_queue_input(
    window: *mut c_void,
    handler: EeWonWinCall,
    ev: *const crate::winobj::EeWonWinEvent,
) {
    if parts(window as isize).is_some() {
        // SAFETY: the contract
        queue_input(window as isize, handler, unsafe { *ev });
    }
}
/// `QueueEvent(new <input event>(..))` on the game window `window` (R6 WON GUI round 40: the event is Rust's,
/// wmobj.rs; Window::QueueEvent's window set, posted to the manager).
fn queue_input(window: isize, handler: C, ev: crate::winobj::EeWonWinEvent) -> Option<isize> {
    queue(window, crate::wmobj::new_input_event(window, handler, ev))
}
/// The Rust-made event `e` queued on the game window `window`.
fn queue(window: isize, e: isize) -> Option<isize> {
    // SAFETY: the new event (no one else holds it)
    unsafe { (*(e as *mut crate::wmobj::EeWonRustEvent)).rec.window = window };
    if wm(EeWonWmCall::PostEvent, e, 1).is_none() {
        // SAFETY: the event no one holds
        unsafe { crate::wmobj::free_rust_event(e as *mut c_void) };
        return None;
    }
    Some(0)
}
/// An input event's record: its type, place, modifiers, button, key and size (InputEvent's constructor).
#[allow(clippy::too_many_arguments)]
fn input(
    kind: i32,
    x: i32,
    y: i32,
    key_mod: i32,
    button: i32,
    key: i32,
    width: i32,
    height: i32,
) -> crate::winobj::EeWonWinEvent {
    crate::winobj::EeWonWinEvent { kind, x, y, key_mod, button, key, width, height }
}
/// DoPopupEvent's `Deliver`: `GetWindow()->OpenPopup(mParams)` (the parameters' parent the event's window, then the
/// manager's DoPopup).
pub(crate) fn deliver_popup(e: isize) -> Option<isize> {
    // SAFETY: a live Rust-made popup event (its handler's contract)
    let mut p = unsafe { (*(e as *const crate::wmobj::EeWonRustEvent)).popup };
    // SAFETY: as above
    p.parent = unsafe { (*(e as *const crate::wmobj::EeWonRustEvent)).rec.window };
    wm(EeWonWmCall::DoPopup, &p as *const crate::wmobj::EeWonWmPopup as isize, 0).map(z)
}
/// `c->GetRoot()->OpenPopup(p)` in the game (R6 WON GUI round 41): the parameters' parent the component's window,
/// then the manager's DoPopup.
pub(crate) fn open_popup(c: isize, mut p: crate::wmobj::EeWonWmPopup) -> Option<isize> {
    // SAFETY: the game's live component; the call re-enters through Component's entry
    p.parent = unsafe {
        crate::compobj::ee_won_comp_call(
            c as *mut c_void,
            crate::compobj::host(),
            crate::compobj::EeWonCompCall::Root,
            0,
            0,
            0,
        )
    };
    wm(EeWonWmCall::DoPopup, &p as *const crate::wmobj::EeWonWmPopup as isize, 0).map(z)
}
/// `GetRoot()->ClosePopup(c)` in the game: the manager's EndPopup (every window's manager is the default one).
pub(crate) fn end_popup(c: isize) -> Option<isize> {
    wm(EeWonWmCall::EndPopup, c, 0).map(z)
}
// QueueSubMenu's popup parameters (the list host's PopupParams local until R6 WON GUI round 40)
static SUBMENU: Mutex<crate::wmobj::EeWonWmPopup> = Mutex::new(crate::wmobj::EeWonWmPopup {
    parent: 0,
    popup: 0,
    input: 0,
    owner: 0,
    flags: 0,
    x: 0,
    y: 0,
    avoid: crate::guictl::EeWonRect4 { x: 0, y: 0, w: 0, h: 0 },
});

/// `InputEvent_SizeChange`, `InputEvent_MouseUp`, `InputEvent_MouseExit`, `InputEvent_WindowClose` (WONEventTypes.h).
const SIZE_CHANGE: i32 = 1;
const MOUSE_UP: i32 = 6;
const MOUSE_EXIT: i32 = 13;
const WINDOW_CLOSE: i32 = 14;

/// WinNewOffscreen: a new offscreen image of `w` x `h` with the old one drawn into it at 0, 0.
fn copy_offscreen(window: isize, w: isize, h: isize) -> isize {
    let image = new_image(w, h);
    let g = graphics_of(image) as *mut crate::gfxrun::EeWonGraphics;
    if !g.is_null() {
        let old = offscreen(window) as *const c_void;
        // SAFETY: the new image's live Graphics; the old image a live handle or NULL
        unsafe {
            crate::gfxrun::ee_won_graphics_call(g, crate::gfxobj::EeWonGfxCall::DrawImage, 0, 0, 0, 0, 0, 0, 0, old)
        };
    }
    set_offscreen(window, image);
    0
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
// the game's image host (WONImageHost), which makes the windows' offscreen images
static IMAGE_HOST: std::sync::atomic::AtomicUsize = std::sync::atomic::AtomicUsize::new(0);

/// The game's window manager's Rust parts (`plat` NULL: freed, as the destructor does; `wm` NULL: gone) and the
/// game's image host.
///
/// # Safety
/// The parts are the manager's live objects until it binds them again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wm_bind(
    manager: *mut c_void,
    wm: *const EeWonWm,
    plat: *const EeWonPlatWm,
    images: crate::imgobj::EeWonImageHostFn,
) {
    if let Ok(mut m) = MANAGER.lock() {
        *m = [manager as isize, wm as isize, plat as isize];
    }
    IMAGE_HOST.store(images as usize, std::sync::atomic::Ordering::Release);
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
    let old = WINDOWS.lock().ok().and_then(|mut m| m.remove(&window));
    if let Some(o) = old.map(|w| w[4]).filter(|&o| o != 0) {
        rf(o, false);
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
        Op::WinNewOffscreen => return parts(window).map(|_| copy_offscreen(window, a, b)),
        Op::WinQueueClose => {
            parts(window)?;
            return queue_input(window, C::HandleInputEvent, input(WINDOW_CLOSE, 0, 0, 0, 0, 0, 0, 0));
        }
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
pub(crate) fn plat_win_op(window: isize, op: EeWonPlatWinOp, a: isize, b: isize, c: isize, d: isize) -> Option<isize> {
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
            // the input events are Rust's (R6 WON GUI round 40)
            O::QueueSize => {
                let (x, y, w, h) = (a as i32, b as i32, c as i32, d as i32);
                return queue_input(window, C::HandleInputEvent, input(SIZE_CHANGE, x, y, 0, 0, 0, w, h));
            }
            O::QueueExit => return queue_input(window, C::HandleInputEvent, input(MOUSE_EXIT, 0, 0, 0, 0, 0, 0, 0)),
            O::QueueMouse => {
                // SAFETY: `a` the five ints (the op's contract)
                let p = unsafe { *(a as *const [i32; 5]) };
                return queue_input(window, C::HandleMouseEvent, input(p[0], p[1], p[2], p[3], p[4], 0, 0, 0));
            }
            // the offscreen image is Rust's (R6 WON GUI round 40)
            O::OffscreenOfRoot | O::Offscreen | O::CopyOffscreen | O::Paint | O::RootPaint => {
                let me = parts(window)?;
                let image = offscreen(window);
                return Some(match op {
                    O::OffscreenOfRoot => {
                        let r = crate::compmem::bounds(me.root);
                        set_offscreen(window, new_image(r.width as isize, r.height as isize));
                        0
                    }
                    // SAFETY: the window's live image handle or 0
                    O::Offscreen => unsafe { crate::imgobj::image_of(image) }.map_or(0, |i| i as *const _ as isize),
                    O::CopyOffscreen => copy_offscreen(window, a, b),
                    O::Paint => z(comp(&me, K::PrePaint, graphics_of(image), 0)),
                    _ => z(comp(&me, K::Paint, graphics_of(image), 0)),
                });
            }
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
pub(crate) fn plat_wm_op(op: EeWonPlatWmOp, window: isize, b: isize, c: isize, d: isize) -> Option<isize> {
    use EeWonPlatWmOp as O;
    match op {
        O::NotModal => return parts(window).and_then(|_| wm(EeWonWmCall::NotModalWindow, window, 0)),
        O::Hold => return rf(window, true),
        O::Release => return rf(window, false),
        // the input and key events (R6 WON GUI round 40: Rust's)
        O::QueueInput | O::QueueKey => {
            parts(window)?;
            let (t, k, m) = (b as i32, c as i32, d as i32);
            return if op == O::QueueKey {
                queue_input(window, C::HandleKeyEvent, input(t, 0, 0, m, 0, k, 0, 0))
            } else {
                queue_input(window, C::HandleInputEvent, input(t, 0, 0, 0, 0, 0, 0, 0))
            };
        }
        O::Offscreen => {
            // SAFETY: the window's live image handle or 0
            return parts(window).map(|_| unsafe { crate::imgobj::image_of(offscreen(window)) }.map_or(0, surface_of));
        }
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
        // an event's members: its record in place (R6 WON GUI round 39)
        K::EvBroadcast | K::EvSetBroadcast | K::EvComp | K::EvSetComp => {
            let r = crate::compobj::event_rec(b)?;
            // SAFETY: the game's live event's record (the ops' contract); no reference outlives the statement
            unsafe {
                Some(match k {
                    K::EvBroadcast => isize::from((*r).broadcast != 0),
                    K::EvSetBroadcast => {
                        (*r).broadcast = u8::from(c != 0);
                        0
                    }
                    K::EvComp => (*r).comp,
                    _ => {
                        crate::compobj::event_set_comp(r, c);
                        0
                    }
                })
            }
        }
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
        // `w->QueueEvent(e)`: the event's window set in its record, posted (R6 WON GUI round 40)
        K::WQueue => {
            parts(b)?;
            // SAFETY: the game's live event `c`: its record at WindowEventBase's offset
            unsafe {
                (*((c as usize + crate::wmobj::EE_WON_WM_EVENT_REC_AT) as *mut crate::wmobj::EeWonWmEventRec)).window =
                    b
            };
            wm(EeWonWmCall::PostEvent, c, 1).map(z)
        }
        // `w->mManager->BroadcastEvent(e)`: the one manager's
        K::WBroadcast => {
            parts(b)?;
            wm(EeWonWmCall::BroadcastEvent, c, 0).map(z)
        }
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
/// component; the Graphics' plain draws and scheme are gfxrun.rs's (round 38). None in a driver's world and for the
/// others (virtuals, references).
pub(crate) fn common_op(comp: isize, op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    use crate::compobj::EeWonCompCall as K;
    use crate::guihost::EeWonCtVirt as V;
    use crate::guihost::EeWonGuiField as Fd;
    match op {
        Op::Draw | Op::CtScheme | Op::CtSetScheme => {
            return crate::compobj::is_game().then(|| crate::gfxrun::game_op(op, a, b)).flatten()
        }
        Op::ChildHold | Op::CtCursorHold | Op::RootImageHold => return rf(a, true),
        Op::ChildRelease | Op::CtCursorRelease | Op::RootImageRelease => return rf(a, false),
        Op::ItemHold | Op::LayHold => return rf(a, true),
        Op::ItemRelease | Op::LayRelease => return rf(a, false),
        Op::EventType
        | Op::EventComponent
        | Op::EventControlId
        | Op::EventCompControlId
        | Op::EventComponentTop
        | Op::EventIsButtonPress
        | Op::EventButton
        | Op::EventChildUnder
        | Op::ComboEventButton
        | Op::DlgEventButtonFlags => return event_op(comp, op, a),
        // `FireEvent(type, b != 0)` on the asking component, the type Rust's map (R6 WON GUI round 39: the hosts'
        // cases went)
        Op::FireEvent if comp != 0 && crate::compobj::is_game() => {
            use crate::guihost::EeWonGuiEvent as E;
            let e = [
                E::InputReturn,
                E::InputTextChanged,
                E::ListSelChanged,
                E::Scroll,
                E::TabCtrlTabSelected,
                E::ButtonPressed,
            ]
            .into_iter()
            .find(|e| *e as isize == a)?;
            let kind = crate::guihost::ee_won_gui_event_type(e) as isize;
            // SAFETY: the game's live component (the op's contract); the call re-enters through Component's entry
            unsafe {
                crate::compobj::ee_won_comp_call(
                    comp as *mut c_void,
                    crate::compobj::host(),
                    crate::compobj::EeWonCompCall::FireEvent,
                    kind,
                    on(b),
                    0,
                )
            };
            return Some(0);
        }
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
            | Op::Invalidate
            | Op::ChildInvalidate
            | Op::CtChild
            | Op::CtBase
            | Op::BtnOf
            | Op::SkinBtn
            | Op::ItemTop
            | Op::ItemSetTop
            | Op::ItemHeight
            | Op::ItemSetHeight
            | Op::ItemWidth
            | Op::ItemSetWidth
            | Op::ItemSelected
            | Op::ItemSetSelected
            | Op::ItemSelectable
            | Op::ItemSetSelectable
            | Op::ChildRight
            | Op::ChildSetPos
            | Op::ChildSetSize
            | Op::ChildSetPosSize
            | Op::ChildSetVisible
            | Op::ChildSetControlId
            | Op::ParentOf
            | Op::LaySetSizeAtLeast
    ) || !crate::compobj::is_game()
    {
        return None;
    }
    // SAFETY: the game's live components (the ops' contracts); each call re-enters through Component's entry
    let kc = |x: isize, k: K, p: isize, q: isize| unsafe {
        crate::compobj::ee_won_comp_call(x as *mut c_void, crate::compobj::host(), k, p, q, 0)
    };
    if let Some(r) = item_op(op, a, b) {
        return Some(r);
    }
    if let Some(r) = child_op(op, a, b, c) {
        return Some(r);
    }
    Some(match op {
        Op::Get if a == Fd::Width as isize => crate::compmem::get(comp).bounds.width as isize,
        Op::Get if a == Fd::Height as isize => crate::compmem::get(comp).bounds.height as isize,
        Op::Get => return None,
        Op::CtBase => crate::compobj::base_call(comp, a, b, c),
        // (no class overrides Invalidate: Component's own)
        Op::Invalidate => z(kc(comp, K::Invalidate, 0, 0)),
        Op::ChildInvalidate => z(kc(a, K::Invalidate, 0, 0)),
        // a child's Invalidate, RequestTimer and RequestFocus (Component's own); its other virtuals are the host's
        Op::CtChild if b == V::Invalidate as isize => z(kc(a, K::Invalidate, 0, 0)),
        Op::CtChild if b == V::RequestTimer as isize => z(kc(a, K::RequestTimer, on(c), 0)),
        Op::CtChild if b == V::RequestFocus as isize => z(kc(a, K::RequestFocus, 0, 0)),
        Op::CtChild => return None,
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
        // `dynamic_cast<Button*>(a)`'s Rust button: every game button registers its own (btnobj.rs)
        Op::BtnOf => crate::registry::find(a, crate::registry::EeWonGuiKind::Button) as isize,
        Op::SkinBtn => skin_btn(comp, a, b, c, &kc),
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

/// `ComponentEvent::Deliver` (R6 WON GUI round 39): a broadcast through the manager, else bubbled up from the
/// component. None outside the game.
pub(crate) fn deliver_component_event(e: isize) -> Option<isize> {
    let r = crate::compobj::event_rec(e)?;
    // SAFETY: the game's live event's record, copied
    let rec = unsafe { *r };
    if rec.broadcast != 0 {
        wm(EeWonWmCall::BroadcastEvent, e, 0)?;
    } else {
        crate::compobj::comp().bubble(rec.comp, e);
    }
    Some(0)
}

/// An event's members (`a` a game `ComponentEvent*`) read in place (compobj.rs's record; R6 WON GUI round 39):
/// its type and component, the component's control id and top, a button press, `dynamic_cast<Button*>` of the
/// component (every game button registers its Rust button) and its button flags, the child of the asking container
/// `comp` the component is under, a click's mouse button. None in a driver's world.
fn event_op(comp: isize, op: Op, a: isize) -> Option<isize> {
    let r = crate::compobj::event_rec(a)?;
    // SAFETY: the game's live event's record (the ops' contract), copied
    let e = unsafe { *r };
    let c = e.comp;
    Some(match op {
        Op::EventType => e.kind as isize,
        Op::EventComponent => c,
        Op::EventControlId | Op::EventCompControlId => crate::compmem::get(c).control_id as isize,
        Op::EventComponentTop => crate::compmem::bounds(c).y as isize,
        Op::EventIsButtonPress => {
            isize::from(e.kind == crate::guihost::ee_won_gui_event_type(crate::guihost::EeWonGuiEvent::ButtonPressed))
        }
        Op::EventButton => {
            if crate::registry::find(c, crate::registry::EeWonGuiKind::Button).is_null() {
                0
            } else {
                c
            }
        }
        Op::EventChildUnder => {
            let (mut child, mut parent) = (c, crate::compmem::parent(c));
            while parent != comp && parent != 0 {
                child = parent;
                parent = crate::compmem::parent(child);
            }
            if parent == 0 {
                0
            } else {
                child
            }
        }
        Op::DlgEventButtonFlags => {
            // SAFETY: a game button's Rust object lives as long as its C++ object, the event's component
            let b = unsafe {
                crate::registry::get::<crate::btnobj::EeWonButton>(c, crate::registry::EeWonGuiKind::Button)
            }?;
            b.call(crate::btnobj::EeWonBtnCall::GetField, crate::btnobj::EeWonBtnField::Flags as isize, 0, 0) as u32
                as isize
        }
        _ => e.button as isize,
    })
}

/// The list area's and text area's hosts' statements that are ScrollArea's and Component's non-virtual methods
/// (guisplit.rs's game hosts for the lists, menus, multi-column lists, trees and text areas; R6 WON GUI round 38):
/// the scroll area's Rust pane and Component's calls, directly, and the references. None for the others.
pub(crate) fn list_host_op(list: isize, op: Op, a: isize, b: isize, c: isize, d: isize) -> Option<isize> {
    if !crate::compobj::is_game() {
        return None;
    }
    use crate::compobj::EeWonCompCall as K;
    use crate::guihost::EeWonGuiField as Fd;
    use crate::paneobj::{EeWonPaneCall as P, EeWonScrollPane};
    // SAFETY: the game's live components (the ops' contracts); each call re-enters through Component's entry
    let kc = |x: isize, k: K, p: isize, q: isize| unsafe {
        crate::compobj::ee_won_comp_call(x as *mut c_void, crate::compobj::host(), k, p, q, 0)
    };
    let pane = |call: P, x: isize, y: isize| {
        // SAFETY: the list's Rust pane lives as long as the list
        let p = unsafe { crate::registry::get::<EeWonScrollPane>(list, crate::registry::EeWonGuiKind::Pane) }?;
        Some(p.call(call, x, y, 0))
    };
    let at = |p: isize| p.wrapping_add(4);
    match op {
        // (list_op's own calls out: a new column viewport's and tree row's references; the text area's segments')
        Op::ChildHold | Op::ItemHold | Op::SegHold => rf(a, true),
        Op::ChildRelease | Op::ItemRelease | Op::SegRelease => rf(a, false),
        Op::Get if a != Fd::Width as isize && a != Fd::Height as isize => {
            pane(P::GetField, a, 0).map(|r| r as i32 as isize)
        }
        Op::Set => pane(P::SetField, a, b as i32 as isize).map(z),
        Op::ScrollTranslate => pane(P::ScrollTranslate, a, b).map(z),
        Op::ScrollUntranslate => pane(P::ScrollUntranslate, a, b).map(z),
        Op::SetVertOffset => pane(P::SetVertOffset, a as i32 as isize, 0).map(z),
        Op::OnBottom => pane(P::OnBottom, 0, 0).map(on),
        Op::ListRoot => Some(kc(list, K::Root, 0, 0)),
        Op::ListParent => Some(crate::compmem::parent(list)),
        Op::ParentRootToChild => Some(z(kc(a, K::RootToChild, b, at(b)))),
        Op::ListContains => Some(on(kc(list, K::Contains, a, b))),
        Op::ListLocalToRoot => Some(z(kc(list, K::LocalToRoot, a, at(a)))),
        Op::LocalToRoot => Some(z(kc(list, K::LocalToRoot, a, b))),
        // a submenu's popup: its parameters (the menu `a`, the flags `b`, this list the owner, at (c, d)), the rectangle
        // it avoids (at `a`), and its event queued on the list's window (R6 WON GUI round 40: Rust's)
        Op::ParamsMake => {
            let (popup, flags, x, y) = (a, b as i32, c as i32, d as i32);
            let avoid = crate::guictl::EeWonRect4::default();
            *SUBMENU.lock().ok()? =
                crate::wmobj::EeWonWmPopup { popup, owner: list, flags, x, y, avoid, ..Default::default() };
            Some(0)
        }
        Op::ParamsAvoid => {
            // SAFETY: `a` the rectangle (the op's contract)
            SUBMENU.lock().ok()?.avoid = unsafe { *(a as *const crate::guictl::EeWonRect4) };
            Some(0)
        }
        Op::QueueDoPopup => {
            let w = kc(list, K::Root, 0, 0);
            parts(w)?;
            queue(w, crate::wmobj::new_popup_event(*SUBMENU.lock().ok()?))
        }
        // a mouse up of the button `d` at (b, c) on the window `a` with its modifiers (R6 WON GUI round 40: Rust's)
        Op::WinQueueMouseUp => {
            let m = field(&parts(a)?, F::KeyMod) as i32;
            queue_input(a, C::HandleMouseEvent, input(MOUSE_UP, b as i32, c as i32, m, d as i32, 0, 0, 0))
        }
        _ => None,
    }
}

/// A WONButton's references and images' reads (`WbText`, `WbImage`, `WbFont`; wonbtnobj.rs) in the game (R6 WON GUI
/// round 38): the game's reference function, the image handles' Rust images, the manager's default font. None in a
/// driver's world and for the others.
pub(crate) fn wonbtn_op(op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    use crate::guihost::{EeWonWbFont as Wf, EeWonWbImage as Wi, EeWonWbText as Wt};
    if !crate::compobj::is_game() {
        return None;
    }
    match op {
        Op::WbText if a == Wt::Hold as isize => rf(b, true),
        Op::WbText if a == Wt::Release as isize => rf(b, false),
        // the wrapped text's methods (wrapobj.rs, R6 WON GUI round 40)
        Op::WbText if a != Wt::New as isize => {
            // SAFETY: `a` an EeWonWbText (the op's contract)
            let which = unsafe { std::mem::transmute::<u32, Wt>(a as u32) };
            crate::wrapobj::game_wrap_op(b, which, c)
        }
        Op::WbFont if a == Wf::Hold as isize => rf(b, true),
        Op::WbFont if a == Wf::Release as isize => rf(b, false),
        Op::WbFont if a == Wf::Default as isize => wm(EeWonWmCall::GetDefaultFont, 0, 0),
        Op::WbImage if a == Wi::Hold as isize => rf(b, true),
        Op::WbImage if a == Wi::Release as isize => rf(b, false),
        Op::WbImage => {
            // SAFETY: the button's live image handle (the op's contract)
            let i = unsafe { crate::imgobj::image_of(b) }?;
            match a {
                x if x == Wi::Width as isize => Some(i.width() as u16 as isize),
                x if x == Wi::Height as isize => Some(i.height() as u16 as isize),
                // a duplicate's handle with its one reference (the C++ case's SmartPtr kept one for the Rust)
                // SAFETY: a live image
                x if x == Wi::Duplicate as isize => Some(unsafe {
                    crate::imgobj::ee_won_image_call(i, crate::imgobj::EeWonImageCall::Duplicate, 0, 0, 0, 0)
                }),
                _ => None,
            }
        }
        _ => None,
    }
}

/// A container's or layout's statements on a child component `a` (R6 WON GUI round 38): its members in place and
/// Component's non-virtual calls (no class overrides SetVisible).
fn child_op(op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    use crate::compobj::EeWonCompCall as K;
    // SAFETY: the game's live child (the ops' contracts); the call re-enters through Component's entry
    let kc = |k: K, p: isize, q: isize, r: isize| unsafe {
        crate::compobj::ee_won_comp_call(a as *mut c_void, crate::compobj::host(), k, p, q, r)
    };
    Some(match op {
        Op::ChildRight => {
            let r = crate::compmem::bounds(a);
            r.x.wrapping_add(r.width) as isize
        }
        Op::ChildSetPos => z(kc(K::SetPos, b as i32 as isize, c as i32 as isize, 0)),
        Op::ChildSetSize => z(kc(K::SetSize, b as i32 as isize, c as i32 as isize, 0)),
        Op::ChildSetPosSize => {
            // SAFETY: `b` is the EeWonRect4 (the op's contract)
            let r = unsafe { *(b as *const crate::guictl::EeWonRect4) };
            let wh = [r.w, r.h];
            z(kc(K::SetPosSize, r.x as isize, r.y as isize, wh.as_ptr() as isize))
        }
        Op::ChildSetVisible => z(kc(K::SetVisible, on(b), 0, 0)),
        Op::ChildSetControlId => {
            crate::compmem::edit(a, |m| m.control_id = b as i32);
            0
        }
        Op::ParentOf => crate::compmem::parent(a),
        Op::LaySetSizeAtLeast => z(kc(K::SetSizeAtLeast, b as i32 as isize, c as i32 as isize, 0)),
        _ => return None,
    })
}

/// A list item's members (`a` a `ListItem*`) read and set in place (leafpaint.rs's record; R6 WON GUI round 38).
fn item_op(op: Op, a: isize, b: isize) -> Option<isize> {
    let i = crate::leafpaint::item_rec(a);
    // SAFETY: the game's live list item (the ops' contract); no reference outlives the statement
    unsafe {
        Some(match op {
            Op::ItemTop => (*i).top as isize,
            Op::ItemSetTop => {
                (*i).top = b as i32;
                0
            }
            Op::ItemHeight => (*i).height as isize,
            Op::ItemSetHeight => {
                (*i).height = b as i32;
                0
            }
            Op::ItemWidth => (*i).width as isize,
            Op::ItemSetWidth => {
                (*i).width = b as i32;
                0
            }
            Op::ItemSelected => isize::from((*i).selected != 0),
            Op::ItemSetSelected => {
                (*i).selected = u8::from(b != 0);
                0
            }
            Op::ItemSelectable => isize::from((*i).selectable != 0),
            Op::ItemSetSelectable => {
                (*i).selectable = u8::from(b != 0);
                0
            }
            _ => return None,
        })
    }
}

/// A skinned button's own state and its component's (`EeWonSkinBtn`, msbtnobj.rs's and wonbtnobj.rs's `SkinBtn`;
/// R6 WON GUI round 38): Component's members in place, the button's Rust object, Component's calls.
fn skin_btn(
    comp: isize,
    which: isize,
    b: isize,
    c: isize,
    kc: &dyn Fn(isize, crate::compobj::EeWonCompCall, isize, isize) -> isize,
) -> isize {
    use crate::btnobj::{EeWonBtnCall as B, EeWonBtnField};
    use crate::compobj::{EeWonCompCall as K, EE_WON_COMP_FLAG_GRAB_BG, EE_WON_COMP_FLAG_WANT_FOCUS};
    use crate::guihost::EeWonSkinBtn as Sb;
    let bits = |x: isize| crate::compmem::get(x).bits;
    // SAFETY: a game button's Rust object lives as long as its C++ object, the asking one
    let btn = |call: B, x: isize, y: isize| match unsafe {
        crate::registry::get::<crate::btnobj::EeWonButton>(comp, crate::registry::EeWonGuiKind::Button)
    } {
        Some(o) => o.call(call, x, y, 0),
        None => 0,
    };
    let flags = || btn(B::GetField, EeWonBtnField::Flags as isize, 0) as u32;
    match which {
        x if x == Sb::IsDisabled as isize => on((bits(comp) & crate::compobj::EE_WON_COMP_FLAG_DISABLED) as isize),
        x if x == Sb::HasFocus as isize => on((bits(comp) & crate::compobj::EE_WON_COMP_FLAG_HAS_FOCUS) as isize),
        x if x == Sb::Parent as isize => crate::compmem::parent(comp),
        x if x == Sb::UsedDialogKeys as isize => {
            let ct = crate::compmem::get(b).ct as *const crate::ctobj::EeWonContainer;
            // SAFETY: the container's live Rust container (`b` a game container)
            on(unsafe {
                crate::ctobj::ee_won_ct_call(
                    ct,
                    crate::ctobj::EeWonCtCall::GetField,
                    crate::ctobj::EeWonCtField::HasUsedDialogKeys as isize,
                    0,
                    0,
                )
            })
        }
        x if x == Sb::IsPressed as isize => on((flags() & 0x20) as isize),
        x if x == Sb::IsChecked as isize => on((flags() & 0x04) as isize),
        x if x == Sb::ButtonFlags as isize => flags() as isize,
        x if x == Sb::IsDefaultButton as isize => on(btn(B::IsDefaultButton, 0, 0)),
        x if x == Sb::FlagSet as isize => on((bits(b) & c as i32) as isize),
        x if x == Sb::SetFlag as isize => z(kc(comp, K::SetFlags, b, 1)),
        x if x == Sb::SetGrabBG as isize => z(kc(comp, K::SetFlags, EE_WON_COMP_FLAG_GRAB_BG as isize, 1)),
        x if x == Sb::SetWantFocus as isize => z(kc(comp, K::SetFlags, EE_WON_COMP_FLAG_WANT_FOCUS as isize, on(b))),
        x if x == Sb::SetButtonFlags as isize => z(btn(B::SetButtonFlags, b, on(c))),
        _ => 0,
    }
}

/// The parts of the game window whose root container is `root`.
fn window_of_root(root: isize) -> Option<Parts> {
    let w = WINDOWS.lock().ok()?.values().find(|w| w[2] == root).copied()?;
    Some(Parts { win: w[0] as _, plat: w[1] as _, root: w[2], root_obj: w[3] as _ })
}

/// The game's root container `root`'s own op `op` (rootobj.rs's calls out; R6 WON GUI round 38): its window's
/// state, rectangle and place (nothing while no window binds it), Component's calls on a child. None in a driver's
/// world and for the others (the cursor's component, the grabbers, the images).
pub(crate) fn root_op(root: isize, op: Op, a: isize, b: isize, c: isize) -> Option<isize> {
    use crate::compobj::EeWonCompCall as K;
    if !matches!(
        op,
        Op::RootOneComponentWindow
            | Op::RootWindowMouse
            | Op::RootWindowRect
            | Op::RootWindowSetPos
            | Op::RootMouseChildOf
            | Op::RootScheme
            | Op::RootEnsureOnTop
            | Op::ChildSetPos
            | Op::ChildSetVisible
            | Op::RootDrawImage
            | Op::RootGrabberRead
            | Op::RootGrabberClear
            | Op::RootGrabberDrop
            | Op::RootImageSize
            | Op::RootCursorComponent
    ) || !crate::compobj::is_game()
    {
        return None;
    }
    // SAFETY: the game's live components (the ops' contracts); each call re-enters through Component's entry
    let kc = |x: isize, k: K, p: isize, q: isize| unsafe {
        crate::compobj::ee_won_comp_call(x as *mut c_void, crate::compobj::host(), k, p, q, 0)
    };
    match op {
        // (no class but Container overrides GetMouseChild)
        Op::RootMouseChildOf => return Some(kc(a, K::MouseChild, 0, 0)),
        Op::RootScheme => return Some(kc(root, K::ColorScheme, 0, 0)),
        Op::RootEnsureOnTop => return Some(z(kc(a, K::EnsureOnTopOf, b, 0))),
        Op::ChildSetPos => return Some(z(kc(a, K::SetPos, b, c))),
        // (no class overrides SetVisible)
        Op::ChildSetVisible => return Some(z(kc(a, K::SetVisible, on(b), 0))),
        Op::RootDrawImage => return Some(crate::gfxrun::draw_image_part(a, b, c)),
        // a queued grabber's state, clear and drop, a background image's size (R6 WON GUI round 39)
        Op::RootGrabberRead | Op::RootGrabberClear | Op::RootGrabberDrop => {
            return Some(crate::grabobj::root_grabber_op(op, a, b))
        }
        // the cursor's component and hot spot (R6 WON GUI round 40); no cursor: no component (the original's
        // MouseMove read through a NULL current cursor)
        Op::RootCursorComponent => {
            // SAFETY: `a` 0 or the game's live cursor handle (its Rust cursor at EE_WON_CURSOR_RUST_AT); `b`, `c` ints
            unsafe {
                if a == 0 {
                    (*(b as *mut i32), *(c as *mut i32)) = (0, 0);
                    return Some(0);
                }
                let r = *((a as usize + crate::cursorobj::EE_WON_CURSOR_RUST_AT)
                    as *const *const crate::cursorobj::EeWonCursor);
                return Some(crate::cursorobj::ee_won_cursor_component(r, b as *mut i32, c as *mut i32));
            }
        }
        Op::RootImageSize => {
            // SAFETY: the game's live image handle; `b` and `c` the two ints (the op's contract)
            unsafe {
                let i = crate::imgobj::image_of(a)?;
                *(b as *mut i32) = i.width() as u16 as i32;
                *(c as *mut i32) = i.height() as u16 as i32;
            }
            return Some(0);
        }
        _ => {}
    }
    let Some(me) = window_of_root(root) else {
        return Some(0);
    };
    Some(match op {
        Op::RootOneComponentWindow => on(field(&me, F::OneComponent)),
        Op::RootWindowMouse => {
            // SAFETY: `a` and `b` are the two ints (the op's contract)
            unsafe {
                *(a as *mut i32) = field(&me, F::MouseX) as i32;
                *(b as *mut i32) = field(&me, F::MouseY) as i32;
            }
            0
        }
        Op::RootWindowRect => z(plat(&me, P::GetRect, a, 0, 0, 0)),
        _ => z(plat(&me, P::SetPosSize, a, b, -1, -1)),
    })
}

/// The Rust root of the game's root container `root`.
fn root_obj_of(root: isize) -> Option<*const EeWonRoot> {
    let m = WINDOWS.lock().ok()?;
    m.values().find(|w| w[2] == root).map(|w| w[3] as *const EeWonRoot)
}

/// MSMessageBox::Msg's statements on the game's window `window` (shell_msgbox.rs's host ops; R6 WON GUI round 38:
/// GuiShell.cpp's cases went): its parent, title, style, creation, component, modal run and close, and the focus on
/// the component `window`. None for a window no game binds (the drivers'), and for the box's making and kind op.
/// (`MSMessageBoxMsg` is reached only by a config's SHOWPARSETIME: WONCfgLoad.inl's ParseTimeBox.)
pub(crate) fn msgbox_op(op: crate::shell::EeWonShellHostOp, window: isize, b: isize, c: isize) -> Option<isize> {
    use crate::shell::EeWonShellHostOp as H;
    if op == H::MsgFocus {
        if !crate::compobj::is_game() {
            return None;
        }
        // SAFETY: the game's live component
        let k = crate::compobj::EeWonCompCall::RequestFocus;
        return Some(z(unsafe {
            crate::compobj::ee_won_comp_call(window as *mut c_void, crate::compobj::host(), k, 0, 0, 0)
        }));
    }
    if !matches!(
        op,
        H::MsgWinParent
            | H::MsgWinTitle
            | H::MsgWinStyle
            | H::MsgWinCreate
            | H::MsgWinAdd
            | H::MsgRunDialog
            | H::MsgWinClose
    ) {
        return None;
    }
    let me = parts(window)?;
    Some(match op {
        H::MsgWinParent => z(win_call(&me, C::SetParent, b)),
        H::MsgWinTitle => {
            // SAFETY: `b` a live GUIString (its data's block, guistrobj.rs); the window's live platform window
            unsafe {
                let data = *(b as *const *const u8);
                let buf =
                    &*(data.add(crate::guistrobj::EE_WON_GSTR_DATA_AT) as *const crate::guistrobj::EeWonGuiStrBuf);
                crate::platwin::ee_won_plat_win_set_title(me.plat, buf.text, buf.length as usize);
            }
            z(win_call(&me, C::TitleChanged, 0))
        }
        H::MsgWinStyle => z(win(&me, C::SetField, F::Style as isize, b)),
        H::MsgWinCreate => {
            let r = [0i32, 0, b as i32, c as i32];
            z(plat(&me, P::Create, r.as_ptr() as isize, 0, 0, 0))
        }
        H::MsgWinAdd => z(win_call(&me, C::AddComponent, b)),
        H::MsgRunDialog => wm(EeWonWmCall::DoDialog, window, b)? as i32 as isize,
        _ => z(win_call(&me, C::Close, 0)),
    })
}
fn surface_of(i: &crate::imgobj::EeWonImage) -> isize {
    // SAFETY: a live image
    unsafe { crate::imgobj::ee_won_image_get(i, crate::imgobj::EeWonImageField::Surface) }
}
fn win_call(p: &Parts, c: C, a: isize) -> isize {
    win(p, c, a, 0)
}

/// Where the game's text segments keep their members (offsets in a `TextSegment`, a `TextLink`'s past it, and a
/// `Font`'s platform font; WONLobbyTextAreaConfig.h, registered by GuiHost.cpp at static initialisation).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonSegAt {
    pub text: usize,
    pub color: usize,
    pub font: usize,
    pub line_break: usize,
    pub pressed: usize,
    pub down_color: usize,
    pub normal_color: usize,
    pub font_rust: usize,
}
static SEG_AT: Mutex<Option<EeWonSegAt>> = Mutex::new(None);
/// The text a copy gathers (the C++ `TextArea::mSelText` until R6 WON GUI round 40): a copy runs to its end in one
/// call (`CopySelection`), so one buffer serves every area.
static SEL_TEXT: Mutex<Vec<u32>> = Mutex::new(Vec::new());

/// Registers the text segments' member offsets (GuiHost.cpp).
///
/// # Safety
/// `at` points at the offsets.
#[no_mangle]
pub unsafe extern "C" fn ee_won_text_seg_register(at: *const EeWonSegAt) {
    if let Ok(mut s) = SEG_AT.lock() {
        // SAFETY: the contract
        *s = Some(unsafe { *at });
    }
}

/// A game segment's members at their registered offsets.
#[derive(Clone, Copy)]
struct Seg {
    p: usize,
    at: EeWonSegAt,
}

impl Seg {
    fn m(&self, off: usize) -> *mut u8 {
        (self.p + off) as *mut u8
    }
    fn int(&self, off: usize) -> i32 {
        // SAFETY: an int member of the live segment (`Seg`'s contract)
        unsafe { *(self.m(off) as *const i32) }
    }
    fn set_int(&self, off: usize, v: i32) {
        // SAFETY: as `int`
        unsafe { *(self.m(off) as *mut i32) = v };
    }
    fn flag(&self, off: usize) -> bool {
        // SAFETY: a bool member of the live segment
        unsafe { *self.m(off) != 0 }
    }
    fn set_flag(&self, off: usize, v: bool) {
        // SAFETY: as `flag`
        unsafe { *self.m(off) = u8::from(v) };
    }
    /// The text slot (a `GUIString`: one pointer to its text).
    fn text_slot(&self) -> *mut *const crate::guistrobj::EeWonGstr {
        self.m(self.at.text).cast()
    }
    /// The text's block (None for no text).
    fn buf(&self) -> Option<&crate::guistrobj::EeWonGuiStrBuf> {
        // SAFETY: the segment's text slot
        let t = unsafe { *self.text_slot() } as *const u8;
        // SAFETY: a live text's block at its offset (guistrobj.rs)
        (!t.is_null()).then(|| unsafe {
            &*(t.add(crate::guistrobj::EE_WON_GSTR_DATA_AT) as *const crate::guistrobj::EeWonGuiStrBuf)
        })
    }
    fn len(&self) -> usize {
        self.buf().map_or(0, |b| usize::from(b.length))
    }
    fn unit(&self, i: usize) -> u32 {
        self.buf().map_or(0, |b| b.unit(i, crate::guistrobj::EE_WON_GSTR_WCHAR))
    }
    /// The held font's platform font.
    fn font(&self) -> *const crate::platfont::EeWonPlatFont {
        // SAFETY: the segment's font slot holds a live Font, its platform font at the registered offset
        unsafe {
            let f = *(self.m(self.at.font) as *const *const u8);
            *(f.add(self.at.font_rust) as *const *const crate::platfont::EeWonPlatFont)
        }
    }
    /// `mFont->GetHeight()` (0) or `GetAscent()` (1).
    fn metric(&self, which: i32) -> isize {
        // SAFETY: the segment's live platform font
        let m = unsafe { crate::platfont::ee_won_plat_font_metric(self.font(), which) };
        m as isize
    }
    /// `mFont->GetCharacterWidth(ch)`.
    fn char_width(&self, ch: i32) -> isize {
        // SAFETY: as `metric`
        let w = unsafe { crate::platfont::ee_won_plat_font_char_width(self.font(), ch) };
        w as isize
    }
    /// `mFont->GetStringWidth(mText, from, n)`.
    fn width(&self, from: usize, n: usize) -> isize {
        if n == 0 {
            return 0;
        }
        let t = self.buf().map_or(std::ptr::null_mut(), |b| b.text) as *const u8;
        let ws = crate::guistrobj::EE_WON_GSTR_WCHAR;
        // SAFETY: the segment's font and its text from `from` (the original's `mText + offset`, `n` units)
        let w =
            unsafe { crate::platfont::ee_won_plat_font_width(self.font(), t.wrapping_add(from * ws).cast(), n as u16) };
        w as isize
    }
}

/// The text area's host's statements on its segments, its link and its copy in the game (R6 WON GUI round 40): a
/// segment's text, colour, font and line break and a link's pressed state read and set in place, the font's
/// measures its platform font's, the copy's text Rust's; the area's link parameter (never read, and no link sets
/// one) is gone. `out` is the area's other calls. None in a driver's world and for the others.
pub(crate) fn text_host_op(op: Op, a: isize, b: isize, c: isize, out: &dyn Fn(Op, isize) -> isize) -> Option<isize> {
    if !crate::compobj::is_game() {
        return None;
    }
    let at = (*SEG_AT.lock().ok()?)?;
    // (the op's contract: `a` a live TextSegment, a TextLink for the link's statements)
    let s = Seg { p: a as usize, at };
    Some(match op {
        Op::TextSetLinkParam => 0,
        Op::SegLineBreak => isize::from(s.flag(at.line_break)),
        Op::SegSetLineBreak => {
            s.set_flag(at.line_break, b != 0);
            0
        }
        Op::SegLength => s.len() as isize,
        Op::SegColor => s.int(at.color) as isize,
        Op::SegSetColor => {
            s.set_int(at.color, b as i32);
            0
        }
        Op::SegTextTake => {
            let v: Vec<u32> = (0..s.len()).map(|i| s.unit(i)).collect();
            if b != 0 && !v.is_empty() {
                // SAFETY: a take sink (the op's contract)
                unsafe { (*(b as *mut Vec<u32>)).extend(v) };
            }
            0
        }
        Op::SegSetText => {
            // SAFETY: `b` a live GUIString; the slot holds its text's reference (SmartPtr's assignment)
            unsafe {
                let new = *(b as *const *const crate::guistrobj::EeWonGstr);
                if !new.is_null() {
                    crate::guistrobj::gstr_ref(new);
                }
                let old = std::mem::replace(&mut *s.text_slot(), new);
                if !old.is_null() {
                    crate::guistrobj::gstr_release(old);
                }
            }
            0
        }
        Op::SegSetFont => {
            if b != 0 {
                rf(b, true)?;
            }
            // SAFETY: the segment's font slot (a SmartPtr: one pointer)
            let old = unsafe { std::mem::replace(&mut *(s.m(at.font) as *mut isize), b) };
            if old != 0 {
                rf(old, false)?;
            }
            0
        }
        Op::SegFontStringWidth => s.width(0, s.len()),
        Op::SegStringWidth => s.width(b as u16 as usize, c as u16 as usize),
        Op::SegFontHeight => s.metric(0),
        Op::SegFontAscent => s.metric(1),
        Op::SegCharWidth => s.char_width(s.unit(b as usize) as i32),
        // a link's Pressed and Released (TextLink's statements; only a link is the area's current link)
        Op::SegPressed => {
            s.set_flag(at.pressed, true);
            s.set_int(at.normal_color, s.int(at.color));
            s.set_int(at.color, s.int(at.down_color));
            0
        }
        Op::SegReleased => {
            if !s.flag(at.pressed) {
                return Some(0);
            }
            s.set_int(at.color, s.int(at.normal_color));
            s.set_flag(at.pressed, false);
            1
        }
        // the copy's text: a segment's part, a line break; then the clipboard's, and emptied
        Op::SelTextAppend => {
            let from = b as u16 as usize;
            let part: Vec<u32> = (from..from + c as u16 as usize).map(|i| s.unit(i)).collect();
            SEL_TEXT.lock().ok()?.extend(part);
            0
        }
        Op::SelTextBreak => {
            SEL_TEXT.lock().ok()?.extend([13, 10]);
            0
        }
        Op::SelTextCopy => {
            let units = std::mem::take(&mut *SEL_TEXT.lock().ok()?);
            let mut t = crate::guistrobj::GText::new(&units);
            out(Op::ClipboardSetText, t.arg());
            0
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "gamewin_tests.rs"]
mod tests;
