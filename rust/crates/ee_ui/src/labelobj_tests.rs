//! The text label's golden (`golden_labelobj.txt`): the first sequences the round-13 driver (`build/ui13`) printed
//! over the unit's statements at the branch's start (UITextLabel.cpp over the label_fit, wrap and draw FFI), replayed
//! op for op against the Rust label: the wrapping's measures and new strings, the fit, the scroll bar's creation,
//! range and removal, the display copies, the draws, the mouse, the virtuals re-entering (`do` lines).

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_labelobj.txt");

/// The driver's texts (handles 0x9000 + i), NUL-terminated.
pub(crate) const TEXTS: [&str; 16] = [
    "\0",
    "OK\0",
    "Hello world\0",
    "The quick brown fox jumps over the lazy dog\0",
    "Line one\nLine two\nLine three\0",
    "Supercalifragilisticexpialidocious and more\0",
    "A\tB\tC D E F G H I J K L M N O P Q R S T U V W X Y Z\0",
    "caf\u{e9} na\u{ef}ve \u{e9}t\u{e9} r\u{e9}sum\u{e9}\0",
    "12345\0",
    "Many short words in a row so that the label wraps them into several lines of text here\0",
    "\n\nblank lines\n\n\0",
    "x\0",
    "Victory! The enemy has been defeated and your empire stands alone across the known world.\0",
    "one two  three   four\0",
    "trailing space \0",
    "-42\0",
];

pub(crate) unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiLabelCall's (the golden's)
    let c = unsafe { core::mem::transmute::<u32, EeUiLabelCall>(call) };
    let a = match c {
        EeUiLabelCall::Init | EeUiLabelCall::SetText => TEXTS[(a - 0x9000) as usize].as_ptr() as isize,
        _ => a,
    };
    // SAFETY: the target is the live label
    let r = unsafe { ee_ui_label_call(target as *const EeUiLabel, c, a, b, f) };
    match c {
        // the driver traced the rectangle an address answers
        EeUiLabelCall::TextRectangle | EeUiLabelCall::CopiedTextRectangle | EeUiLabelCall::ScrollBarRectangle => {
            // SAFETY: the address of a live rectangle; `f` four live floats
            unsafe {
                let r = *(r as *const UiRect);
                crate::obj::write_buf(f, crate::obj::rect_buf(r));
            }
            0
        }
        _ => r,
    }
}

/// Replays a label golden: `new TYPE FONT BORDER VARIANT` makes the label with the world's rectangle.
pub(crate) fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let mut obj: *mut EeUiLabel = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let t: c_ulong = w[1].parse().unwrap();
            let font: isize = w[2].parse().unwrap();
            let border: c_ulong = w[3].parse().unwrap();
            // the constructor's rectangle is the world's (`set 6 0 ...` before the line)
            let host = Host { obj: o, f: h };
            let ctor = host.rect();
            obj = Box::into_raw(Box::new(EeUiLabel::new(host, ctor, font, t, border)));
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_label_free(obj) };
            obj = core::ptr::null_mut();
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
fn a_changed_measure_fails() {
    // the first word measured answers another width: the wrap no longer matches
    let mut from = 0;
    let at = loop {
        let word = from + GOLDEN[from..].find("\nop 153 ").expect("a word measured");
        let next = word + 1 + GOLDEN[word + 1..].find('\n').unwrap();
        if GOLDEN[next..].starts_with("\nset 80 ") {
            break next;
        }
        from = next;
    };
    let line_end = at + 1 + GOLDEN[at + 1..].find('\n').unwrap();
    let w: Vec<&str> = GOLDEN[at + 1..line_end].split(' ').collect();
    let g = format!("{}\nset 80 {} 0 43c80000 {}{}", &GOLDEN[..at], w[2], w[5..].join(" "), &GOLDEN[line_end..]);
    replay(&g);
}

#[test]
fn rows_at_least_one() {
    assert_eq!(rows(30.0, 0, 12), 2);
    assert_eq!(rows(5.0, 1, 12), 1);
    assert_eq!(rows(100.0, 3, 16), 5);
}
