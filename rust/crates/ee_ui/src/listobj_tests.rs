//! The list box's golden (`golden_listobj.txt`): the first sequences the round-15 driver (`build/ui15`) printed over
//! the template's statements at the census commit (lle/UIListBox.h over the list record FFI and draw_ffi's list
//! functions), replayed op for op against the Rust list box: the constructor's parts and first scroll bar check,
//! the lines' insertions and deletions, the copies and draws, the key repeat, the mouse, the scroll bar's creation
//! and removal, and the virtuals re-entering (`do` lines). UI round 15 (the lines Rust's): the per-`T` copy op (194)
//! left the golden, the copy being this object's, and the four `ProcessDown`..`ProcessEnd` ops (195 with `a` 0..3)
//! became one op each (900..903).

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_listobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiListCall's (the golden's)
    let c = unsafe { core::mem::transmute::<u32, EeUiListCall>(call) };
    // SAFETY: the target is the live list box; `f` is four live floats
    let r = unsafe { ee_ui_list_call(target as *const EeUiList, c, a, b, f) };
    if c == EeUiListCall::CopyTextRectangle {
        // the driver traced the rectangle at the answered address
        // SAFETY: the answer is the object's own rectangle; `f` is four live floats
        unsafe { write_buf(f, crate::obj::rect_buf(*(r as *const UiRect))) };
        return 0;
    }
    r
}

/// Replays a list box golden: `new LINE_HEIGHT AUTO_SELECT BORDER VARIANT` makes the box (and runs the C++
/// constructor's `CheckScrollBar`).
fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiList = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let line_height: c_ulong = w[1].parse().unwrap();
            let auto = w[2] == "1";
            let border: c_ulong = w[3].parse().unwrap();
            obj = Box::into_raw(Box::new(EeUiList::new(Host { obj: o, f: h }, line_height, auto, border)));
            r.target.set(obj as *const c_void);
            let mut f = [0.0f32; 4];
            // SAFETY: made above
            unsafe { ee_ui_list_call(obj, EeUiListCall::CheckScrollBar, 0, 0, f.as_mut_ptr()) };
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_list_free(obj) };
            obj = core::ptr::null_mut();
            true
        }
        _ => false,
    });
    r.checked.get()
}

#[test]
fn the_golden_replays() {
    let n = replay(GOLDEN);
    assert!(n > 3_000, "only {n} lines checked");
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_line_count_fails() {
    // the first box given one line fewer before its constructor's scroll bar check
    let changed = GOLDEN.replacen("set 191 0 9 ", "set 191 0 8 ", 1);
    assert_ne!(changed, GOLDEN);
    replay(&changed);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_border_fails() {
    // the first box made with another border thickness
    let changed = GOLDEN.replacen("new 16 1 2 0", "new 16 1 1 0", 1);
    assert_ne!(changed, GOLDEN);
    replay(&changed);
}

#[test]
fn the_sort_order_folds_case() {
    // SAFETY: NUL-terminated literals
    unsafe {
        assert!(ee_ui_text_less_no_case(c"apple".as_ptr(), c"Banana".as_ptr()));
        assert!(!ee_ui_text_less_no_case(c"banana".as_ptr(), c"Apple".as_ptr()));
    }
}

// ---- the lines (UI round 15): boxed numbers through a fake line type

unsafe extern "C" fn make() -> *mut c_void {
    Box::into_raw(Box::new(0u32)) as *mut c_void
}
unsafe extern "C" fn clone(l: *const c_void) -> *mut c_void {
    // SAFETY: a boxed number
    Box::into_raw(Box::new(unsafe { *(l as *const u32) })) as *mut c_void
}
unsafe extern "C" fn free(l: *mut c_void) {
    // SAFETY: made by make or clone
    drop(unsafe { Box::from_raw(l as *mut u32) });
}
unsafe extern "C" fn assign(to: *mut c_void, from: *const c_void) {
    // SAFETY: boxed numbers
    unsafe { *(to as *mut u32) = *(from as *const u32) };
}
unsafe extern "C" fn text(_: *const c_void) -> *const c_char {
    core::ptr::null()
}
unsafe extern "C" fn from_text(_: *const c_char) -> *mut c_void {
    core::ptr::null_mut()
}
unsafe extern "C" fn less(_: *mut c_void, a: *const c_void, b: *const c_void) -> bool {
    // SAFETY: boxed numbers
    unsafe { *(a as *const u32) < *(b as *const u32) }
}
unsafe extern "C" fn eq(a: *const c_void, b: *const c_void) -> bool {
    // SAFETY: boxed numbers
    unsafe { *(a as *const u32) == *(b as *const u32) }
}

/// A host answering every op 0 but the line count (the virtual `GetNumLines`), read from the object.
unsafe extern "C" fn host(o: *mut c_void, op: Op, _: isize, _: isize, _: isize, _: isize, _: *mut f32) -> isize {
    if op == Op::ListNumLines && !o.is_null() {
        // SAFETY: the test's list (set below)
        return unsafe { (*(o as *const EeUiList)).line_count(0) as isize };
    }
    0
}

fn numbers(o: &EeUiList) -> Vec<u32> {
    // SAFETY: boxed numbers
    (0..o.line_count(0)).map(|i| unsafe { *(o.line(0, i) as *const u32) }).collect()
}

#[test]
fn the_lines_are_owned_inserted_found_and_freed() {
    let ops = EeUiLineOps { make, clone, free, assign, text, from_text };
    let data = [5u32, 1, 9];
    let mut o = EeUiList::new(Host { obj: core::ptr::null_mut(), f: Some(host) }, 16, true, 2);
    // SAFETY: three numbers
    unsafe { o.set_lines(ops, data.as_ptr() as *const c_void, 3, 4) };
    let o = Box::new(o);
    assert_eq!(numbers(&o), [5, 1, 9]);
    let shown = o.line_count(1);
    let n = 7u32;
    // SAFETY: a number; the halving search over [1, 9] from index 1 puts 7 before 9
    unsafe { o.insert(&n as *const u32 as *const c_void, 1, core::ptr::null_mut(), Some(less)) };
    assert_eq!(numbers(&o), [5, 1, 7, 9]);
    // SAFETY: a number; no predicate: at the index
    unsafe { o.insert(&n as *const u32 as *const c_void, 0, core::ptr::null_mut(), None) };
    assert_eq!(numbers(&o), [7, 5, 1, 7, 9]);
    o.delete_line(1);
    assert_eq!(numbers(&o), [7, 1, 7, 9]);
    let nine = 9u32;
    // SAFETY: numbers
    assert_eq!(unsafe { o.find(&nine as *const u32 as *const c_void, eq) }, None, "the host's line count is 0");
    o.copy_lines(shown + 2, 1, 3);
    assert_eq!(o.line_count(1), shown + 2);
    // SAFETY: boxed numbers
    assert_eq!(unsafe { (*(o.line(1, 0) as *const u32), *(o.line(1, 1) as *const u32)) }, (1, 7));
    assert!(o.line(0, 99).is_null());
    o.delete_all();
    assert_eq!(o.line_count(0), 0);
    let p = Box::into_raw(o);
    // SAFETY: made above
    unsafe { ee_ui_list_free(p) };
}
