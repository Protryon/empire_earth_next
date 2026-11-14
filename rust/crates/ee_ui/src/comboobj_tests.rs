//! The combo box's golden (`golden_comboobj.txt`): the first sequences the round-15 driver (`build/ui15`) printed over
//! UIComboBox.cpp at the census commit (its forwarders over the combo record FFI, the ee_uicpp_combo_* host calls
//! answered by the driver's world), replayed op for op against the Rust combo box: opening and closing the list, the
//! mouse and keyboard routing, the selection, the line edits, the copies and draws, and the virtuals re-entering.

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_comboobj.txt");

/// The driver's texts (handles 0x9000 + i), NUL-terminated.
const TEXTS: [&str; 12] = [
    "\0",
    "Easy\0",
    "Normal\0",
    "Hard\0",
    "Very hard\0",
    "caf\u{e9}\0",
    "Random Map\0",
    "Player 1\0",
    "\u{65e5}\u{672c}\0",
    "Tournament\0",
    "easy\0",
    "NORMAL\0",
];

fn text(h: isize) -> isize {
    TEXTS[(h - 0x9000) as usize].as_ptr() as isize
}

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the call numbers are EeUiComboCall's (the golden's)
    let c = unsafe { core::mem::transmute::<u32, EeUiComboCall>(call) };
    let a = if matches!(c, EeUiComboCall::InsertLine | EeUiComboCall::UpdateTranslatedKey) { text(a) } else { a };
    // SAFETY: the target is the live combo box; `f` is four live floats; the texts are NUL-terminated
    unsafe { ee_ui_combo_call(target as *const EeUiCombo, c, a, b, f) }
}

/// The world's line and edit box texts are handles: their bytes for the object.
fn texts(_: &Replay, op: Op, a: isize, r: isize) -> isize {
    if op == Op::ComboQuery && matches!(a >> 24, 4 | 11) && r != 0 {
        text(r)
    } else {
        r
    }
}

/// Replays a combo box golden: `new LABEL EDIT MAX_HEIGHT SELECTION VARIANT` makes the box (drop 0x70001, list
/// 0x70002, the focus highlight's colour (0, 0.5, 1)).
fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch).with_pure_hook(texts);
    let mut obj: *mut EeUiCombo = core::ptr::null_mut();
    r.run(|r, w| match w[0] {
        "new" => {
            let (o, h) = r.host();
            let label: isize = w[1].parse().unwrap();
            let edit: isize = w[2].parse().unwrap();
            let max: c_ulong = w[3].parse().unwrap();
            let sel: c_ulong = w[4].parse().unwrap();
            let host = Host { obj: o, f: h };
            let kind = if label != 0 { 1 } else { 0 };
            obj = Box::into_raw(Box::new(EeUiCombo::new(
                host,
                label,
                0x70001,
                0x70002,
                edit,
                kind,
                max,
                sel,
                [0.0, 0.5, 1.0],
            )));
            r.target.set(obj as *const c_void);
            true
        }
        "free" => {
            // SAFETY: made by "new"
            unsafe { ee_ui_combo_free(obj) };
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
fn a_changed_line_height_fails() {
    // the list's lines ten pixels taller: the list opens taller
    let changed = GOLDEN.replace("set 197 83886080 1", "set 197 83886080 2");
    assert_ne!(changed, GOLDEN);
    replay(&changed);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_changed_inserted_text_fails() {
    // the first InsertLine given another text
    let i = GOLDEN.find("\ncall 12 ").expect("an InsertLine") + 9;
    let end = i + GOLDEN[i..].find(' ').unwrap();
    let h: isize = GOLDEN[i..end].parse().unwrap();
    let other = if h == 0x9001 { 0x9002 } else { 0x9001 };
    let changed = format!("{}{}{}", &GOLDEN[..i], other, &GOLDEN[end..]);
    replay(&changed);
}
