//! UIEvent's golden (`golden_eventobj.txt`): the first sequences the round-21 driver (`build/ui21`) printed over
//! UIEvent.cpp/.h at the round's base (a world of the virtuals' results, events files with failed counts and reads,
//! the owners by id), replayed op for op against the Rust record and loader. Calls 1, 2 are `EeUiEventCall`'s; the
//! driver's own: 3 `CreateControlEvents`, 4 `CreateFormEvents(manager a)`, 9 the count.

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_eventobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, _b: isize, _f: *mut f32) -> isize {
    let rec = target as *mut EeUiEventFields;
    let file = 0x1f11e as *mut c_void;
    // SAFETY: the test's live record; the host answers from the replay (the file is never read)
    unsafe {
        (match call {
            1 => ee_ui_event_call(rec, Some(host_current), EeUiEventCall::Initialize),
            2 => ee_ui_event_call(rec, Some(host_current), EeUiEventCall::Uninitialize),
            3 => ee_ui_event_create(file, Some(host_current), 0, core::ptr::null_mut()),
            4 => ee_ui_event_create(file, Some(host_current), 1, a as *mut c_void),
            _ => return (*rec).mInitializeCount as isize,
        }) as isize
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let rec = Box::into_raw(Box::new(EeUiEventFields { mInitializeCount: 0 }));
    r.target.set(rec as *const c_void);
    r.run(|_, _| false);
    // SAFETY: made above
    drop(unsafe { Box::from_raw(rec) });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 500);
}

#[test]
#[should_panic(expected = "golden line")]
fn an_owner_found_for_every_record_fails() {
    // every owner lookup answered with an owner: the records the original skipped are added
    let mut out = Vec::new();
    let mut after_owner = false;
    for l in GOLDEN.lines() {
        if after_owner && l.starts_with("= 0 ") {
            out.push(l.replacen("= 0 ", "= 1536 ", 1));
        } else {
            out.push(l.to_string());
        }
        after_owner = l.starts_with("op 632 ");
    }
    replay(&out.join("\n"));
}
