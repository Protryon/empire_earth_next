//! The background table's golden (`golden_bgtableobj.txt`): the first sequences the round-20 driver (`build/ui20`)
//! printed over UIBackgroundTable.cpp and `UIManager::CreateBackgrounds` at the round's base (a world file of
//! background records with failed reads, the manager's forms by id and texture depth; entries as world handles),
//! replayed op for op against the Rust list. The calls: 1 `Create`, 2 `CreateBackgrounds`, 3 `DeleteAll`, 4 the entry
//! at a, 5 the array's size.

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_bgtableobj.txt");

unsafe fn dispatch(_target: *const c_void, call: u32, a: isize, _b: isize, _f: *mut f32) -> isize {
    let h: EeUiHostFn = Some(host_current);
    match call {
        // SAFETY: the driver's file is a handle the replay's host answers for
        1 => unsafe { ee_ui_bgtable_create(0x77 as *mut c_void, h) as isize },
        2 => {
            ee_ui_bgtable_create_backgrounds(0x88 as *mut c_void, h);
            0
        }
        3 => {
            ee_ui_bgtable_delete_all(h);
            0
        }
        4 => ee_ui_bgtable_at(a as c_ulong) as isize,
        _ => ee_ui_bgtable_count() as isize,
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    r.run(|_, _| false);
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 1_000);
    assert_eq!(ee_ui_bgtable_count(), 0, "every sequence ends with DeleteAll");
}

#[test]
fn an_empty_entry_is_the_constructors() {
    let e = empty();
    assert_eq!((e.mID, e.mBackgroundID, e.mFormID, e.mDrawPriority), (-1, c_ulong::MAX, c_ulong::MAX, 1));
    assert_eq!((e.mOpacity, e.mTileWidthPercent, e.mTileHeightPercent), (1.0, 1.0, 1.0));
    assert!(!e.mVisible && e.mBackgroundType == BITMAP);
}
