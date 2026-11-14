//! The list box's golden (`golden_listobj.txt`): the first sequences the round-15 driver (`build/ui15`) printed over
//! the template's statements at the census commit (lle/UIListBox.h over the list record FFI and draw_ffi's list
//! functions), replayed op for op against the Rust list box: the constructor's parts and first scroll bar check,
//! the lines' insertions and deletions, the copies and draws, the key repeat, the mouse, the scroll bar's creation
//! and removal, and the virtuals re-entering (`do` lines).

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
