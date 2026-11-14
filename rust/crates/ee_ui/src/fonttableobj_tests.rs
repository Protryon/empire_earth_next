//! The font table's tests: its golden (the round's base UIFontTable over random call sequences, `build/ui18`), the
//! first sequences replayed, and changed answers that must fail.

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_fonttableobj.txt");

/// The replays share the lists: one at a time.
static SERIAL: Mutex<()> = Mutex::new(());

unsafe fn dispatch(_target: *const core::ffi::c_void, call: u32, a: isize, b: isize, _f: *mut f32) -> isize {
    let h: EeUiHostFn = Some(host_current);
    // the driver's calls: 1 Create, 2 CreateClosestFont, 3 DeleteAll, 4 GetFont, 5 FindOrCreateFont (b: the style,
    // quality and size in 16, 16 and 32 bits)
    // SAFETY: the host is the running replay's
    unsafe {
        match call {
            1 => ee_ui_font_tables(EeUiFontsCall::Create, a, 0x7100, 0, 0, h),
            2 => ee_ui_font_tables(EeUiFontsCall::CreateClosestFont, a, b, 0, 0, h),
            3 => ee_ui_font_tables(EeUiFontsCall::DeleteAll, 0, 0, 0, 0, h),
            4 => ee_ui_font_tables(EeUiFontsCall::GetFont, a, 0, 0, 0, h),
            5 => ee_ui_font_tables(EeUiFontsCall::FindOrCreateFont, a, b & 0xffff, (b >> 16) & 0xffff, b >> 32, h),
            _ => panic!("call {call}"),
        }
    }
}

/// Replays a golden -> the lines checked.
fn run(golden: &str) -> usize {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    // a failed replay before this one may have left entries
    {
        let mut l = lists();
        l.tables.clear();
        l.fonts.clear();
    }
    let replay = Replay::new(golden, dispatch);
    replay.run(|_, _| false);
    assert!(lists().fonts.is_empty() && lists().tables.is_empty(), "DeleteAll left entries");
    replay.checked.get()
}

#[test]
fn replays_the_font_tables_golden() {
    let checked = run(GOLDEN);
    assert!(checked > 1_000, "{checked} lines checked");
}

#[test]
#[should_panic]
fn a_font_that_never_matches_fails() {
    // every CompareFont answered false: the shared fonts are made again
    let changed: Vec<String> = GOLDEN.lines().map(|l| l.to_string()).collect();
    let mut out = Vec::new();
    let mut after_compare = false;
    for l in changed {
        if l.starts_with("op 502 ") {
            after_compare = true;
        } else if after_compare && l.starts_with("= 1 ") {
            out.push(l.replacen("= 1 ", "= 0 ", 1));
            after_compare = false;
            continue;
        } else if l.starts_with("= ") {
            after_compare = false;
        }
        out.push(l);
    }
    run(&out.join("\n"));
}

#[test]
fn an_empty_record_has_no_font() {
    let _serial = SERIAL.lock().unwrap_or_else(|e| e.into_inner());
    lists().tables = vec![None];
    // SAFETY: no op is made for an empty record
    assert_eq!(unsafe { ee_ui_font_tables(EeUiFontsCall::GetFont, 0, 0, 0, 0, None) }, 0);
    // SAFETY: as above; past the table
    assert_eq!(unsafe { ee_ui_font_tables(EeUiFontsCall::CreateClosestFont, 3, 12, 0, 0, None) }, 0);
    lists().tables.clear();
}
