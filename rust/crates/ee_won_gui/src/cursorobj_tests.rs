//! cursorobj: the cursor kinds over a test host (handles and components as counted records; platform calls
//! logged). The driver tests/won/won_win35_test.cpp runs the original classes against the C++ shell over this.
use super::*;
use std::cell::RefCell;
use std::ffi::c_void;

/// A C++ handle: its reference count and its Rust cursor (freed when the count falls to zero, as RefCount does).
struct Handle {
    refs: i32,
    rust: *mut EeWonCursor,
}

#[derive(Default)]
struct World {
    handles: Vec<Option<Handle>>,
    comps: Vec<i32>,
    log: Vec<String>,
}

thread_local! {
    static W: RefCell<World> = RefCell::new(World::default());
}

fn log(s: String) {
    W.with(|w| w.borrow_mut().log.push(s));
}
fn take_log() -> Vec<String> {
    W.with(|w| std::mem::take(&mut w.borrow_mut().log))
}
/// A handle's number as a `Cursor*` (1-based: 0 is NULL).
fn wrap(rust: *mut EeWonCursor) -> isize {
    W.with(|w| {
        let mut w = w.borrow_mut();
        w.handles.push(Some(Handle { refs: 0, rust }));
        w.handles.len() as isize
    })
}
fn rust_of(h: isize) -> *mut EeWonCursor {
    W.with(|w| w.borrow().handles[h as usize - 1].as_ref().expect("a live handle").rust)
}
fn refs(h: isize) -> Option<i32> {
    W.with(|w| w.borrow().handles[h as usize - 1].as_ref().map(|x| x.refs))
}
fn hold(h: isize) {
    W.with(|w| w.borrow_mut().handles[h as usize - 1].as_mut().expect("a live handle").refs += 1);
}
fn release(h: isize) {
    let dead = W.with(|w| {
        let mut w = w.borrow_mut();
        let x = w.handles[h as usize - 1].as_mut().expect("a live handle");
        x.refs -= 1;
        if x.refs <= 0 {
            w.handles[h as usize - 1].take().map(|x| x.rust)
        } else {
            None
        }
    });
    if let Some(r) = dead {
        // SAFETY: the handle's own cursor, freed with it (the world is not borrowed: the free calls the host)
        unsafe { ee_won_cursor_free(r) };
    }
}
fn comp(i: usize) -> i32 {
    W.with(|w| w.borrow().comps[i])
}

unsafe extern "C" fn host(op: EeWonCursorOp, a: isize, b: isize, c: isize) -> isize {
    use EeWonCursorOp as O;
    match op {
        O::Hold => hold(a),
        O::Release => release(a),
        O::CompHold => W.with(|w| w.borrow_mut().comps[a as usize - 1] += 1),
        O::CompRelease => W.with(|w| w.borrow_mut().comps[a as usize - 1] -= 1),
        // SAFETY: a live handle's cursor
        O::Native => return unsafe { ee_won_cursor_native(rust_of(a), a) },
        // SAFETY: a live handle's cursor; the caller's outs
        O::Component => return unsafe { ee_won_cursor_component(rust_of(a), b as *mut i32, c as *mut i32) },
        O::Wrap => return wrap(a as *mut EeWonCursor),
        O::RustOf => return rust_of(a) as isize,
    }
    0
}

extern "C" fn set_system(c: u32) {
    log(format!("sys {c}"));
}
extern "C" fn create(px: *const u32, w: i32, h: i32, x: i32, y: i32) -> *mut c_void {
    // SAFETY: the cursor's own pixels
    log(format!("create {w}x{h} @{x},{y} {:08x}", if px.is_null() { 0 } else { unsafe { *px } }));
    7usize as *mut c_void
}
extern "C" fn set(c: *mut c_void) {
    log(format!("set {}", c as usize));
}
extern "C" fn destroy(c: *mut c_void) {
    log(format!("destroy {}", c as usize));
}
unsafe extern "C" fn show(s: i32) {
    log(format!("show {s}"));
}
const FNS: EeWonPlatCursorFns = EeWonPlatCursorFns { set_system, create, set, destroy };

fn component_of(h: isize) -> (isize, i32, i32) {
    let (mut x, mut y) = (-7, -9);
    // SAFETY: a live handle's cursor
    let c = unsafe { ee_won_cursor_component(rust_of(h), &mut x, &mut y) };
    (c, x, y)
}

#[test]
fn the_kinds_answer_as_the_four_classes_did() {
    W.with(|w| w.borrow_mut().comps = vec![0, 0]);
    // Cursor: no native cursor, no component, the outs untouched
    let plain = wrap(ee_won_cursor_new());
    hold(plain);
    // SAFETY: live cursors throughout
    unsafe {
        assert_eq!(ee_won_cursor_native(rust_of(plain), plain), 0);
        assert_eq!(component_of(plain), (0, -7, -9));
        // PlatCursor: itself
        let sys = wrap(ee_won_cursor_new_system(&FNS, 3));
        hold(sys);
        assert_eq!(ee_won_cursor_native(rust_of(sys), sys), sys);
        assert_eq!(component_of(sys), (0, -7, -9));
        // ComponentCursor: its component (held) and hot spot
        let cc = wrap(ee_won_cursor_new_component(Some(host), 2, 5, -6));
        hold(cc);
        assert_eq!(comp(1), 1);
        assert_eq!(component_of(cc), (2, 5, -6));
        assert_eq!(ee_won_cursor_native(rust_of(cc), cc), 0);
        let none = wrap(ee_won_cursor_new_component(Some(host), 0, 1, 2));
        hold(none);
        assert_eq!(component_of(none), (0, 1, 2));
        // CursorRef: its cursor's answers, the cursor held
        let r = wrap(ee_won_cursor_new_ref(Some(host), sys));
        hold(r);
        assert_eq!(refs(sys), Some(2));
        assert_eq!(ee_won_cursor_native(rust_of(r), r), sys);
        let rc = wrap(ee_won_cursor_new_ref(Some(host), cc));
        hold(rc);
        assert_eq!(component_of(rc), (2, 5, -6));
        // SetCursor: the old one released, the new one held; the same one: nothing
        (*rust_of(r)).set_inner(cc);
        assert_eq!((refs(sys), refs(cc)), (Some(1), Some(3)));
        (*rust_of(r)).set_inner(cc);
        assert_eq!(refs(cc), Some(3));
        assert_eq!(component_of(r), (2, 5, -6));
        (*rust_of(r)).set_inner(0);
        assert_eq!(refs(cc), Some(2));
        // (a reference to none: the original read through NULL; here it has neither)
        assert_eq!(ee_won_cursor_native(rust_of(r), r), 0);
        assert_eq!(component_of(r), (0, -7, -9));
        // the ends: a reference gives its cursor back, a component cursor its component
        release(rc);
        assert_eq!(refs(cc), Some(1));
        release(cc);
        assert_eq!((refs(cc), comp(1)), (None, 0));
        release(none);
        release(r);
        release(plain);
        // the window's cursor change: only a platform cursor is shown and applied
        take_log();
        ee_won_cursor_apply(std::ptr::null(), show);
        let p2 = wrap(ee_won_cursor_new());
        ee_won_cursor_apply(rust_of(p2), show);
        assert!(take_log().is_empty());
        ee_won_cursor_apply(rust_of(sys), show);
        assert_eq!(take_log(), ["show 1", "sys 3"]);
        let px = [0x1122_3344u32; 4];
        let pix = wrap(ee_won_cursor_new_pixels(&FNS, px.as_ptr(), 2, 2, 1, 0));
        hold(pix);
        ee_won_cursor_apply(rust_of(pix), show);
        ee_won_cursor_apply(rust_of(pix), show);
        assert_eq!(take_log(), ["show 1", "create 2x2 @1,0 11223344", "set 7", "show 1", "set 7"]);
        // an applied cursor's end restores the default shape and destroys the platform cursor
        release(pix);
        assert_eq!(take_log(), ["set 0", "destroy 7"]);
        release(sys);
        hold(p2);
        release(p2);
    }
    assert!(W.with(|w| w.borrow().handles.iter().all(|h| h.is_none())));
}

/// The one test that touches the table (a process-wide static).
#[test]
fn the_standard_table_holds_one_reference_per_kind_and_follows_a_new_cursor() {
    W.with(|w| w.borrow_mut().comps = vec![0]);
    assert_eq!(ee_won_cursor_standard(-1), 0);
    assert_eq!(ee_won_cursor_standard(7), 0);
    assert_eq!(ee_won_cursor_standard(6), 0);
    // SAFETY: live cursors and the test host
    unsafe {
        let a = wrap(ee_won_cursor_new_system(&FNS, 0));
        hold(a); // the test's own
        ee_won_cursor_set_standard(Some(host), 6, a);
        let table = ee_won_cursor_standard(6);
        assert_ne!(table, 0);
        assert_ne!(table, a);
        assert_eq!((refs(table), refs(a)), (Some(1), Some(2)));
        assert_eq!(ee_won_cursor_native(rust_of(table), table), a);
        // set again: the same table cursor, now the new cursor's
        let b = wrap(ee_won_cursor_new_component(Some(host), 1, 3, 4));
        ee_won_cursor_set_standard(Some(host), 6, b);
        assert_eq!(ee_won_cursor_standard(6), table);
        assert_eq!((refs(table), refs(a), refs(b)), (Some(1), Some(1), Some(1)));
        assert_eq!(component_of(table), (1, 3, 4));
        assert_eq!(ee_won_cursor_native(rust_of(table), table), 0);
        // out of range, or without a host: nothing
        ee_won_cursor_set_standard(Some(host), 7, a);
        ee_won_cursor_set_standard(None, 6, a);
        assert_eq!(refs(a), Some(1));
        // none: the table cursor stays, holding nothing
        ee_won_cursor_set_standard(Some(host), 6, 0);
        assert_eq!((ee_won_cursor_standard(6), refs(b), comp(0)), (table, None, 0));
        release(a);
        // the manager's six: the hand from its bits, the others the system's, each its own cursor
        take_log();
        ee_won_cursor_install_standard(Some(host), &FNS);
        let six: Vec<isize> = (0..6).map(|k| ee_won_cursor_standard(k)).collect();
        assert!(six.iter().all(|&h| h != 0 && refs(h) == Some(1)));
        for (kind, system) in [(0usize, 0u32), (2, 2), (3, 3), (4, 4), (5, 5)] {
            let n = ee_won_cursor_native(rust_of(six[kind]), six[kind]);
            assert_eq!(refs(n), Some(1));
            ee_won_cursor_apply(rust_of(n), show);
            assert_eq!(take_log(), ["show 1".to_string(), format!("sys {system}")]);
        }
        let hand = ee_won_cursor_native(rust_of(six[1]), six[1]);
        ee_won_cursor_apply(rust_of(hand), show);
        let l = take_log();
        assert_eq!(l.len(), 3);
        assert!(l[1].starts_with("create 32x32 @12,4 "), "{l:?}");
        // installed again: the table cursors stay, the old cursors end
        ee_won_cursor_install_standard(Some(host), &FNS);
        assert_eq!((0..6).map(|k| ee_won_cursor_standard(k)).collect::<Vec<_>>(), six);
        assert_eq!(refs(hand), None);
        assert_eq!(take_log(), ["set 0", "destroy 7"]);
    }
}
