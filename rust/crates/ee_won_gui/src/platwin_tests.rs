//! platwin: unit tests over a scripted shell (the window's Window base, root and manager as a logged world).
use super::*;

/// The world one window sees: its Window base, its root, its manager.
#[derive(Default)]
struct World {
    log: Vec<String>,
    has_wm: bool,
    fills: bool,
    popup: bool,
    open: bool,
    style: isize,
    surface: (isize, isize),
    mouse_in: bool,
    mouse: Option<(i32, i32)>,
    hit: isize,
    root: (i32, i32),
    focus: isize,
    capture: isize,
    parent_visible: isize,
    popup_hit: bool,
    invalid: bool,
    plat_window: isize,
}

thread_local! { static ME_PTR: std::cell::RefCell<isize> = const { std::cell::RefCell::new(0) }; }

fn me_ptr() -> isize {
    ME_PTR.with(|p| *p.borrow())
}

extern "C" fn host(me: *mut c_void, op: EeWonPlatWinOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    // SAFETY: the test's world
    let w = unsafe { &mut *(me as *mut World) };
    type O = EeWonPlatWinOp;
    let s = match op {
        O::HasWm => return isize::from(w.has_wm),
        O::Wm => {
            type M = EeWonPlatWmCall;
            // SAFETY: a call the window sent
            let call: M = unsafe { std::mem::transmute(a as i32) };
            match call {
                M::SurfaceWidth => return w.surface.0,
                M::SurfaceHeight => return w.surface.1,
                M::Capture => return w.capture,
                M::Focus => return w.focus,
                M::WindowFromPoint => return w.hit,
                M::PlatWindow => return w.plat_window,
                M::SetFocus => w.focus = b,
                M::SetCapture => w.capture = b,
                _ => {}
            }
            let arg = if b != 0 && b == me_ptr() { "me".to_string() } else { format!("{b:x}") };
            format!("wm {call:?} {arg}")
        }
        O::Mouse => {
            if let Some((x, y)) = w.mouse {
                // SAFETY: two ints
                unsafe { *(a as *mut [i32; 2]) = [x, y] };
                return 1;
            }
            return 0;
        }
        O::Fills => return isize::from(w.fills),
        O::Frame => return isize::from(!w.fills && w.style & 0x20 == 0),
        O::Popup => return isize::from(w.popup),
        O::Open => return isize::from(w.open),
        O::Style => return w.style,
        O::WindowCreate => {
            w.root = (c as i32, d as i32);
            format!("Window::Create {a},{b},{c},{d}")
        }
        O::Center => format!("Center {a}"),
        O::Showing => format!("SetIsShowing {a}"),
        O::Show => format!("PlatformShow {a}"),
        O::QueueSize => format!("size {a},{b},{c},{d}"),
        O::QueueExit => "exit".into(),
        O::QueueMouse => {
            // SAFETY: five ints
            let p = unsafe { *(a as *const [i32; 5]) };
            format!("mouse {p:?}")
        }
        O::InvalidateRoot => "InvalidateRoot".into(),
        O::MouseIn => return isize::from(w.mouse_in),
        O::RootContains => return isize::from(a >= 0 && b >= 0 && a < w.root.0 as isize && b < w.root.1 as isize),
        O::Paint => {
            if !w.invalid {
                return 0;
            }
            w.log.push("paint".into());
            return 1;
        }
        O::ParentVisible => return w.parent_visible,
        O::WindowClose => "Window::PlatformClose".into(),
        O::WindowTimer => format!("Window::TimerEvent {a}"),
        O::ApplyCursor => format!("cursor {a:x}"),
        O::PopupHit => return isize::from(w.popup_hit),
        O::CursorShow => format!("PlatCursorShow {a}"),
        O::PlatMinimize => format!("PlatWindowMinimize {a:x}"),
    };
    w.log.push(s);
    0
}

fn win(world: &mut World) -> *mut EeWonPlatWin {
    let p = world as *mut World as *mut c_void;
    ME_PTR.with(|m| *m.borrow_mut() = p as isize);
    ee_won_plat_win_new(p, host)
}

fn run(w: *mut EeWonPlatWin, c: EeWonPlatWinCall, a: isize, b: isize, cc: isize, d: isize) -> isize {
    // SAFETY: the test's window
    unsafe { ee_won_plat_win(w, c, a, b, cc, d) }
}

fn create(w: *mut EeWonPlatWin, r: [i32; 4]) {
    run(w, EeWonPlatWinCall::Create, r.as_ptr() as isize, 0, 0, 0);
}

fn rect(w: *mut EeWonPlatWin, client: bool) -> [i32; 4] {
    let mut r = [0i32; 4];
    run(w, EeWonPlatWinCall::GetRect, r.as_mut_ptr() as isize, isize::from(client), 0, 0);
    r
}

fn free(w: *mut EeWonPlatWin) {
    run(w, EeWonPlatWinCall::Destroy, 0, 0, 0, 0);
    // SAFETY: freed once
    unsafe { ee_won_plat_win_free(w) };
}

#[test]
fn a_framed_window_from_its_client_rect_is_centred_and_shown() {
    let mut world = World { has_wm: true, open: true, style: 0x41, ..World::default() };
    let w = win(&mut world);
    create(w, [100, 50, 200, 120]);
    // the frame (3) and caption (18) around the client rectangle
    assert_eq!(rect(w, false), [100, 50, 206, 144]);
    assert_eq!(rect(w, true), [103, 71, 200, 120]);
    assert_eq!(
        world.log,
        ["Window::Create 103,71,200,120", "wm Add me", "Center 1", "SetIsShowing 1", "PlatformShow 1"]
    );
    // a hidden start (0x02) and the default position (0x80); without 0x40 the rectangle is the window's
    world.log.clear();
    world.style = 0x82;
    create(w, [100, 50, 200, 120]);
    assert_eq!(world.log, ["Window::Create 103,71,194,96", "wm Add me", "Center 0", "SetIsShowing 0"]);
    free(w);
}

#[test]
fn a_window_that_fills_the_surface_follows_it() {
    let mut world = World { has_wm: true, open: true, fills: true, surface: (1024, 768), ..World::default() };
    let w = win(&mut world);
    create(w, [5, 5, 10, 10]);
    assert_eq!(rect(w, false), [0, 0, 1024, 768]);
    world.log.clear();
    run(w, EeWonPlatWinCall::FollowSurface, 0, 0, 0, 0);
    assert!(world.log.is_empty(), "same size: nothing");
    world.surface = (800, 600);
    run(w, EeWonPlatWinCall::FollowSurface, 0, 0, 0, 0);
    assert_eq!(world.log, ["size -1,-1,800,600", "wm SetDirty 0"]);
    // it never moves
    world.log.clear();
    run(w, EeWonPlatWinCall::SetPosSize, 1, 2, 3, 4);
    assert!(world.log.is_empty());
    assert_eq!(rect(w, true), [0, 0, 800, 600]);
    free(w);
}

#[test]
fn moves_and_resizes_queue_their_events() {
    let mut world = World { has_wm: true, open: true, style: 0x22, ..World::default() };
    let w = win(&mut world);
    create(w, [0, 0, 50, 40]);
    world.log.clear();
    run(w, EeWonPlatWinCall::SetPosSize, 10, 20, -1, -1);
    assert_eq!(world.log, ["size 10,20,-1,-1", "wm SetDirty 0"]);
    world.log.clear();
    run(w, EeWonPlatWinCall::SetPosSize, 10, 20, 60, -1);
    assert_eq!(world.log, ["size -1,-1,60,40", "wm SetDirty 0"]);
    // closed: nothing
    world.open = false;
    world.log.clear();
    run(w, EeWonPlatWinCall::SetPosSize, 0, 0, 1, 1);
    assert!(world.log.is_empty());
    free(w);
}

#[test]
fn hiding_the_focus_window_moves_the_focus_to_its_visible_parent() {
    let mut world = World { has_wm: true, open: true, style: 0x22, ..World::default() };
    let w = win(&mut world);
    create(w, [0, 0, 50, 40]);
    world.log.clear();
    run(w, EeWonPlatWinCall::Show, 1, 0, 0, 0);
    assert_eq!(world.log, ["wm SetDirty 0", "InvalidateRoot", "wm BringToFront me", "wm SetFocus me"]);
    assert_eq!(run(w, EeWonPlatWinCall::IsVisible, 0, 0, 0, 0), 1);
    world.log.clear();
    world.parent_visible = 0x55;
    run(w, EeWonPlatWinCall::Show, 0, 0, 0, 0);
    assert_eq!(world.log, ["wm SetDirty 0", "wm SetFocus 55"]);
    // showing a popup does not take the focus; showing twice does nothing
    world.popup = true;
    world.log.clear();
    run(w, EeWonPlatWinCall::Show, 1, 0, 0, 0);
    run(w, EeWonPlatWinCall::Show, 1, 0, 0, 0);
    assert_eq!(world.log, ["wm SetDirty 0", "InvalidateRoot"]);
    free(w);
}

#[test]
fn the_timer_queues_a_mouse_exit_once_the_pointer_left() {
    let mut world = World { has_wm: true, open: true, style: 0x22, mouse_in: true, ..World::default() };
    let w = win(&mut world);
    create(w, [10, 10, 50, 40]);
    let me = me_ptr();
    // over this window, inside the root: nothing
    world.mouse = Some((20, 20));
    world.hit = me;
    world.log.clear();
    run(w, EeWonPlatWinCall::Timer, 7, 0, 0, 0);
    assert_eq!(world.log, ["Window::TimerEvent 7"]);
    // over another window that is the popup not wanting the mouse: nothing
    world.hit = 0x99;
    world.popup_hit = true;
    world.log.clear();
    run(w, EeWonPlatWinCall::Timer, 7, 0, 0, 0);
    assert_eq!(world.log, ["Window::TimerEvent 7"]);
    world.popup_hit = false;
    world.log.clear();
    run(w, EeWonPlatWinCall::Timer, 7, 0, 0, 0);
    assert_eq!(world.log, ["Window::TimerEvent 7", "exit"]);
    // the pointer off the surface
    world.mouse = None;
    world.hit = me;
    world.log.clear();
    run(w, EeWonPlatWinCall::Timer, 1, 0, 0, 0);
    assert_eq!(world.log, ["Window::TimerEvent 1", "exit"]);
    free(w);
}

#[test]
fn cursors_minimize_close_and_mouse_events() {
    let mut world = World { has_wm: true, open: true, fills: true, surface: (640, 480), ..World::default() };
    let w = win(&mut world);
    create(w, [0, 0, 1, 1]);
    run(w, EeWonPlatWinCall::SetRegistered, 1, 0, 0, 0);
    world.log.clear();
    // outside and without the capture: no cursor; with the capture: applied
    run(w, EeWonPlatWinCall::SetCursor, 0x42, 0, 0, 0);
    world.capture = me_ptr();
    run(w, EeWonPlatWinCall::SetCursor, 0x42, 0, 0, 0);
    run(w, EeWonPlatWinCall::ShowNativeCursor, 0, 0, 0, 0);
    run(w, EeWonPlatWinCall::ShowNativeCursor, 1, 0, 0, 0);
    assert_eq!(world.log, ["cursor 42", "PlatCursorShow 1"]);
    world.log.clear();
    run(w, EeWonPlatWinCall::Minimize, 1, 0, 0, 0);
    world.plat_window = 0x1234;
    run(w, EeWonPlatWinCall::Minimize, 1, 0, 0, 0);
    run(w, EeWonPlatWinCall::Minimize, 0, 0, 0, 0);
    assert_eq!(world.log, ["PlatWindowMinimize 1234"]);
    world.log.clear();
    let p = [3, 100, 50, 4, 1];
    run(w, EeWonPlatWinCall::QueueMouse, p.as_ptr() as isize, 0, 0, 0);
    world.invalid = true;
    run(w, EeWonPlatWinCall::Paint, 0, 0, 0, 0);
    run(w, EeWonPlatWinCall::Close, 0, 0, 0, 0);
    assert_eq!(
        world.log,
        ["mouse [3, 100, 50, 4, 1]", "paint", "wm SetDirty 0", "Window::PlatformClose", "wm Remove me"]
    );
    assert_eq!(run(w, EeWonPlatWinCall::IsVisible, 0, 0, 0, 0), 0);
    // the manager's fields
    let mut i = EeWonPlatWinInfo {
        x: 9,
        y: 9,
        width: 9,
        height: 9,
        visible: true,
        top_most: false,
        registered: false,
        popup: true,
        fills: false,
        frame: true,
        open: false,
        minimize_on_alt_tab: false,
        forward_keys: false,
    };
    run(w, EeWonPlatWinCall::SetTopMost, 1, 0, 0, 0);
    run(w, EeWonPlatWinCall::SetForwardKeys, 1, 0, 0, 0);
    run(w, EeWonPlatWinCall::Info, &mut i as *mut _ as isize, 0, 0, 0);
    assert_eq!(
        (i.x, i.width, i.visible, i.top_most, i.registered, i.popup, i.fills, i.frame, i.open),
        (0, 640, false, true, true, false, true, false, true)
    );
    assert!(i.forward_keys && !i.minimize_on_alt_tab);
    // registered: the destructor takes it off the manager
    world.log.clear();
    free(w);
    assert_eq!(world.log, ["wm Remove me"]);
}

#[test]
fn focus_moves_and_exits_at_the_edges() {
    let mut world = World { has_wm: true, open: true, style: 0x22, mouse_in: true, ..World::default() };
    let w = win(&mut world);
    create(w, [0, 0, 50, 40]);
    // a popup takes the focus without being raised
    world.popup = true;
    world.log.clear();
    run(w, EeWonPlatWinCall::SetFocus, 0, 0, 0, 0);
    assert_eq!(world.log, ["wm SetFocus me"]);
    // a move down only queues the move
    world.popup = false;
    world.log.clear();
    run(w, EeWonPlatWinCall::SetPosSize, 0, 5, -1, -1);
    assert_eq!(world.log, ["size 0,5,-1,-1", "wm SetDirty 0"]);
    // over this window but outside its root: the exit, whatever the popup is
    world.mouse = Some((500, 500));
    world.hit = me_ptr();
    world.popup_hit = true;
    world.log.clear();
    run(w, EeWonPlatWinCall::Timer, 2, 0, 0, 0);
    assert_eq!(world.log, ["Window::TimerEvent 2", "exit"]);
    free(w);
}
