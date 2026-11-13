//! The scroll bar's golden (`golden_scrollbarobj.txt`): the first sequences the round-10 driver (`build/ui10`) printed
//! over the unmodified UIScrollBar.cpp (its parts leaves of a world: rectangles, the slider's style and value, their
//! handlers stepping the value), replayed op for op against the Rust object.

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_scrollbarobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiScrollBarCall's (the golden's); the target is the live bar
    unsafe {
        ee_ui_scroll_bar_call(
            target as *const EeUiScrollBar,
            core::mem::transmute::<u32, EeUiScrollBarCall>(call),
            a,
            b,
            f,
        )
    }
}

fn hexf(w: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(w, 16).unwrap())
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiScrollBar = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let f = [hexf(w[3]), hexf(w[4]), hexf(w[5]), hexf(w[6])];
            // SAFETY: four floats
            obj = unsafe { ee_ui_scroll_bar_new(o, h, f.as_ptr(), w[1].parse().unwrap(), w[2].parse().unwrap()) };
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_scroll_bar_free(obj) };
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
fn a_moved_part_fails() {
    // the first CreateButton expected with another bitmap: the constructor's line no longer matches
    let g = GOLDEN.replacen("\nop 42 ", "\nop 42 9", 1);
    replay(&g);
}
