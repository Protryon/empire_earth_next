//! The edit box's golden (`golden_editobj.txt`): the first sequences the round-14 driver (`build/ui14`) printed over
//! the unit's statements at the branch's start (UIEditBox.cpp over the edit, edit_keys and edit_draw record FFI),
//! replayed op for op against the Rust edit box: the constructor's objects, the typing and the key dispatch with its
//! polls, the clipboard, the IME composition and its underlines, the copies and draws, the mouse, the focus, the
//! virtuals re-entering (`do` lines).

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_editobj.txt");

/// The driver's texts (handles 0x9000 + i), NUL-terminated.
const TEXTS: [&str; 16] = [
    "\0",
    "OK\0",
    "Hello world\0",
    "The quick brown fox jumps over the lazy dog\0",
    "Line one\nLine two\nLine three\0",
    "Supercalifragilisticexpialidocious and more\0",
    "caf\u{e9} na\u{ef}ve \u{e9}t\u{e9} r\u{e9}sum\u{e9}\0",
    "12345\0",
    "Many short words in a row so that the box wraps them into several lines of text here\0",
    "\n\nblank lines\n\n\0",
    "x\0",
    "one two  three   four\0",
    "-42\0",
    "7\0",
    "\u{65e5}\u{672c}\u{8a9e}\0",
    " \0",
];

/// The driver's IME attributes (handles 0xa000 + i), NUL-terminated.
const ATTRS: [&[u8]; 6] = [
    b"\x01\x01\x02\0",
    b"\x02\x02\x02\x02\x02\0",
    b"\x01\0",
    b"\x03\x01\x02\x01\x03\x03\0",
    b"\0",
    b"\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\0",
];

thread_local! {
    /// The replay running (its last bytes: the manager's typed key).
    static REPLAY: Cell<usize> = const { Cell::new(0) };
}

/// The first UTF-8 character of `bytes`, terminated (the manager's `UpdateTranslatedKey` copies one).
fn first_char(bytes: &[u8]) -> Vec<u8> {
    let n = match bytes.first() {
        None | Some(0) => 0,
        Some(&c) if c < 0x80 => 1,
        Some(&c) if c < 0xe0 => 2,
        Some(&c) if c < 0xf0 => 3,
        _ => 4,
    };
    let mut v = bytes[..n.min(bytes.len())].to_vec();
    v.push(0);
    v
}

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiEditCall's (the golden's)
    let c = unsafe { core::mem::transmute::<u32, EeUiEditCall>(call) };
    // SAFETY: `f` is four live floats
    let buf = unsafe { read_buf(f) };
    let (mut cc, mut dd) = (0isize, 0isize);
    if matches!(c, EeUiEditCall::ImeChange | EeUiEditCall::DeleteRange | EeUiEditCall::Select) {
        cc = buf[2].to_bits() as isize;
        dd = buf[3].to_bits() as isize;
    }
    let mut a = a;
    let mut index: c_long = 0;
    let key: Vec<u8>;
    match c {
        EeUiEditCall::AddImeResult
        | EeUiEditCall::ImeChange
        | EeUiEditCall::SetText
        | EeUiEditCall::UpdateTranslatedKey => {
            if a == 0x8001 {
                // SAFETY: the replay running is live (`replay` sets it)
                let r = unsafe { &*(REPLAY.with(|r| r.get()) as *const Replay) };
                key = first_char(&r.last_bytes.borrow());
                a = key.as_ptr() as isize;
            } else {
                a = TEXTS[(a - 0x9000) as usize].as_ptr() as isize;
            }
        }
        EeUiEditCall::CaretFromMouse => a = &mut index as *mut c_long as isize,
        _ => {}
    }
    if c == EeUiEditCall::ImeChange {
        cc = ATTRS[(cc - 0xa000) as usize].as_ptr() as isize;
    }
    // SAFETY: the target is the live edit box; the texts are NUL-terminated
    let r = unsafe { ee_ui_edit_call(target as *const EeUiEdit, c, a, b, cc, dd, f) };
    match c {
        EeUiEditCall::CaretFromMouse => {
            // SAFETY: `f` is four live floats
            unsafe { *f.add(2) = f32::from_bits(index as u32) };
            r
        }
        // SAFETY: the object answered NUL-terminated bytes
        EeUiEditCall::AllText | EeUiEditCall::SelectedText | EeUiEditCall::Line if r != 0 => unsafe {
            crate::obj::replay::fnv(r)
        },
        _ => r,
    }
}

/// The world's clipboard is a text handle: its bytes for the object.
fn clipboard(_: &Replay, op: Op, _: isize, r: isize) -> isize {
    if op == Op::ClipboardGet && r != 0 {
        TEXTS[(r - 0x9000) as usize].as_ptr() as isize
    } else {
        r
    }
}

/// Replays an edit box golden: `new FONT MAX FLAGS BORDER VARIANT` makes the box.
fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch).with_pure_hook(clipboard);
    REPLAY.with(|p| p.set(&r as *const Replay as usize));
    let mut obj: *mut EeUiEdit = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let font: isize = w[1].parse().unwrap();
            let max: c_ulong = w[2].parse().unwrap();
            let flags: u32 = w[3].parse().unwrap();
            let border: c_ulong = w[4].parse().unwrap();
            obj = Box::into_raw(Box::new(EeUiEdit::new(Host { obj: o, f: h }, font, max, flags, border)));
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_edit_free(obj) };
            obj = core::ptr::null_mut();
            true
        }
        _ => false,
    });
    REPLAY.with(|p| p.set(0));
    r.checked.get()
}

#[test]
fn the_golden_replays() {
    let n = replay(GOLDEN);
    assert!(n > 5_000, "only {n} lines checked");
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_font_height_fails() {
    // the first text's font answered a pixel shorter
    let changed = GOLDEN.replacen("set 154 131072 16 ", "set 154 131072 15 ", 1);
    assert_ne!(changed, GOLDEN);
    replay(&changed);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_text_fails() {
    // the first SetText given another text ("Many short words ..." for "12345")
    let changed = GOLDEN.replacen("call 40 36871 ", "call 40 36872 ", 1);
    assert_ne!(changed, GOLDEN);
    replay(&changed);
}
