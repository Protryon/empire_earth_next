//! platwm: the manager over a test host (windows as plain records; every call logged).
use super::*;
use std::sync::Mutex as StdMutex;

struct World {
    wins: Vec<EeWonPlatWinInfo>,
    log: Vec<String>,
    ui: bool,
}

static WORLD: StdMutex<Option<World>> = StdMutex::new(None);

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    f(WORLD.lock().unwrap_or_else(|e| e.into_inner()).as_mut().unwrap())
}

fn id(w: isize) -> usize {
    (w as usize) - 1
}

unsafe extern "C" fn host(_me: *mut c_void, op: EeWonPlatWmOp, a: isize, b: isize, c: isize, d: isize) -> isize {
    use EeWonPlatWmOp as O;
    match op {
        O::IsUiThread => with(|w| isize::from(w.ui)),
        O::Info => {
            let i = with(|w| w.wins[id(a)]);
            // SAFETY: the manager's out struct
            unsafe { *(b as *mut EeWonPlatWinInfo) = i };
            0
        }
        O::SetRegistered => with(|w| {
            w.wins[id(a)].registered = b != 0;
            0
        }),
        O::QueueMouse => {
            // SAFETY: four ints
            let p = unsafe { std::slice::from_raw_parts(c as *const i32, 4) };
            with(|w| w.log.push(format!("mouse w{} {b} {p:?}", id(a))));
            0
        }
        O::ClientSize => {
            // SAFETY: two ints
            unsafe {
                *(b as *mut i32) = 1000;
                *(b as *mut i32).add(1) = 750;
            }
            0
        }
        O::Title | O::Offscreen | O::Parent | O::NotModal | O::Modifiers => 0,
        _ => {
            with(|w| w.log.push(format!("{op:?} w{} {b} {c} {d}", if a > 0 && a < 100 { id(a) as isize } else { -1 })));
            0
        }
    }
}

fn win(x: i32, y: i32, w: i32, h: i32, fills: bool) -> EeWonPlatWinInfo {
    EeWonPlatWinInfo {
        x,
        y,
        width: w,
        height: h,
        visible: true,
        open: true,
        fills,
        frame: !fills,
        ..Default::default()
    }
}

fn setup(wins: Vec<EeWonPlatWinInfo>) -> EeWonPlatWm {
    *WORLD.lock().unwrap_or_else(|e| e.into_inner()) = Some(World { wins, log: Vec::new(), ui: true });
    EeWonPlatWm::new(std::ptr::null_mut(), Some(host))
}

fn log() -> Vec<String> {
    with(|w| std::mem::take(&mut w.log))
}

fn p(i: usize) -> *mut c_void {
    (i + 1) as *mut c_void
}

/// One test at a time: the host's world is a static.
static SERIAL: StdMutex<()> = StdMutex::new(());

#[test]
fn client_rect_of_a_framed_window() {
    let i = win(10, 20, 100, 80, false);
    assert_eq!(client_rect(&i), [13, 41, 94, 56]);
    let small = win(0, 0, 4, 10, false);
    assert_eq!(client_rect(&small), [3, 21, 0, 0]);
    assert_eq!(client_rect(&win(1, 2, 3, 4, true)), [1, 2, 3, 4]);
}

#[test]
fn drawing_order_layers() {
    let mut a = win(0, 0, 1, 1, true);
    let mut b = win(0, 0, 1, 1, false);
    b.popup = true;
    let c = win(0, 0, 1, 1, false);
    a.top_most = false;
    let mut d = win(0, 0, 1, 1, false);
    d.top_most = true;
    let order = drawing_order(&[(p(0), b), (p(1), d), (p(2), c), (p(3), a)]);
    assert_eq!(order.iter().map(|o| o.0 as usize).collect::<Vec<_>>(), vec![4, 3, 2, 1]);
}

#[test]
fn focus_moves_and_removal_picks_the_topmost() {
    let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    let m = setup(vec![win(0, 0, 800, 600, true), win(10, 10, 100, 100, false)]);
    m.add(p(0));
    m.add(p(1));
    log();
    m.set_focus(p(1));
    assert_eq!(log(), vec!["QueueInput w1 0 0 0".to_string()]);
    m.remove(p(1));
    // the parent (none) -> the topmost visible window that is not a popup
    assert_eq!(m.focus(), p(0));
    assert!(!with(|w| w.wins[1].registered));
}

#[test]
fn mouse_goes_to_the_window_under_the_pointer_in_client_coordinates() {
    let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    let m = setup(vec![win(0, 0, 800, 600, true), win(100, 100, 200, 150, false)]);
    m.add(p(0));
    m.add(p(1));
    log();
    let mut e = EeWonPlatEvent { kind: EV_MOUSE_MOVE, time: 0, window: 7 as *mut c_void, data: [0; 18] };
    // a 1000 x 750 client showing the 800 x 600 surface: 1.25 times
    e.data[0] = 250;
    e.data[1] = 250;
    assert!(!m.handle(&e));
    assert_eq!(m.mouse(), (200, 200, true));
    assert_eq!(log(), vec!["mouse w1 0 [200, 200, 0, 0]".to_string()]);
}

#[test]
fn keys_make_control_characters_and_alt_is_left_alone() {
    let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    let m = setup(vec![win(0, 0, 800, 600, true)]);
    m.add(p(0));
    m.set_focus(p(0));
    log();
    let mut e = EeWonPlatEvent { kind: EV_KEY_DOWN, time: 0, window: std::ptr::null_mut(), data: [0; 18] };
    e.data[1] = b'C' as u32;
    e.data[2] = MOD_CONTROL;
    m.handle(&e);
    assert_eq!(log(), vec!["QueueKey w0 0 67 4".to_string(), "QueueKey w0 2 3 4".to_string()]);
    e.data[2] = MOD_ALT;
    m.handle(&e);
    assert!(log().is_empty());
}

#[test]
fn composite_frames_and_serial() {
    let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    let m = setup(vec![win(2, 2, 10, 30, false)]);
    m.add(p(0));
    m.apply_surface_size(20, 40);
    m.composite();
    let mut o = (std::ptr::null(), 0, 0, 0, 0, 0, 0u32);
    assert!(m.lock_frame(&mut o));
    // SAFETY: the frame while locked
    let f = unsafe { std::slice::from_raw_parts(o.0, 20 * 40) };
    assert_eq!((o.1, o.2, o.3, o.6), (20, 40, 80, 1));
    assert_eq!(f[0], 0xFF00_0000);
    assert_eq!(f[2 * 20 + 2], 0xFF00_0000 | FACE);
    assert_eq!(f[3 * 20 + 3], 0xFF00_0000 | HILIGHT);
    assert_eq!(f[5 * 20 + 6], 0xFF00_0000 | CAPTION_INACTIVE);
    m.unlock_frame();
}

#[test]
fn pending_size_and_scale_from_another_thread() {
    let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    let m = setup(vec![]);
    with(|w| w.ui = false);
    m.set_surface_size(640, 480);
    m.set_pixel_scale(9.0);
    m.set_pixel_scale(4.0); // already pending: no second wake
    assert_eq!(log(), vec!["Wake w-1 0 0 0".to_string()]);
    assert_eq!(m.surface_size(), (800, 600));
    m.begin_pass();
    assert_eq!(m.surface_size(), (640, 480));
    assert_eq!(m.pixel_scale(), 4.0);
}

#[test]
fn key_mods_and_classic_colours() {
    assert_eq!(key_mod(MOD_SHIFT | MOD_CONTROL), 5);
    assert_eq!(key_mod(MOD_ALT), 2);
    assert_eq!(CLASSIC[1], FACE);
    assert_eq!(CLASSIC.len(), 15);
}

#[test]
fn control_characters_and_double_clicks() {
    let _g = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    let m = setup(vec![win(0, 0, 800, 600, true)]);
    m.add(p(0));
    m.set_focus(p(0));
    log();
    let mut e = EeWonPlatEvent { kind: EV_KEY_DOWN, time: 0, window: std::ptr::null_mut(), data: [0; 18] };
    for (key, ch) in [(VK_BACK, 8), (VK_TAB, 9), (VK_RETURN, 13), (VK_ESCAPE, 27)] {
        e.data[1] = key;
        m.handle(&e);
        assert_eq!(log(), vec![format!("QueueKey w0 0 {key} 0"), format!("QueueKey w0 2 {ch} 0")]);
    }
    // the press: 1 click a press, 2 a double click, 3 a press again, 4 a double click
    let mut e = EeWonPlatEvent { kind: EV_MOUSE_DOWN, time: 0, window: 7 as *mut c_void, data: [0; 18] };
    e.data[4] = 1;
    for (clicks, kind) in [(1, 1), (2, 2), (3, 1), (4, 2), (0, 1)] {
        e.data[5] = clicks;
        m.handle(&e);
        assert_eq!(log(), vec![format!("mouse w0 {kind} [0, 0, 0, 0]")]);
    }
}
