//! The status bar's golden (`golden_statusobj.txt`): the first sequences the round-10 driver (`build/ui10`) printed
//! over the unmodified UIStatusBar.cpp (a world of the base control, the two rhombi and the texts), replayed op for op
//! against the Rust object.

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_statusobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiStatusBarCall's (the golden's); the target is the live bar
    unsafe {
        ee_ui_status_bar_call(
            target as *const EeUiStatusBar,
            core::mem::transmute::<u32, EeUiStatusBarCall>(call),
            a,
            b,
            f,
        )
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiStatusBar = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            obj = ee_ui_status_bar_new(o, h, w[1].parse().unwrap());
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_status_bar_free(obj) };
            true
        }
        _ => false,
    });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 1_000);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_fill_fails() {
    // the bar's first SetRectangle expected with another bottom edge: the Rust's line no longer matches
    let at = GOLDEN.find("\nop 71 0 ").expect("a fill") + 1;
    let end = at + GOLDEN[at..].find('\n').unwrap();
    let mut w: Vec<&str> = GOLDEN[at..end].split(' ').collect();
    w[6] = "7f7f7f7f";
    let g = format!("{}{}{}", &GOLDEN[..at], w.join(" "), &GOLDEN[end..]);
    replay(&g);
}
