//! UIEntityGroup's golden (`golden_groupobj.txt`): the first sequences the round-21 driver (`build/ui21`) printed over
//! UIEntityGroup.cpp/.h at the round's base (a world of entities: button or hot key, shown, on the current form,
//! tabbing, the tab stop), replayed op for op against the Rust object. Calls are `EeUiEntityGroupCall`'s; a group is
//! made per sequence (`new`, `free`).

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_groupobj.txt");

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, _b: isize, _f: *mut f32) -> isize {
    // SAFETY: the test's live object; the call numbers are the enum's (the golden's)
    unsafe {
        let g = *(target as *const *mut EeUiEntityGroup);
        ee_ui_entity_group_call(g, core::mem::transmute::<u32, EeUiEntityGroupCall>(call), a)
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let slot: *mut *mut EeUiEntityGroup = Box::into_raw(Box::new(core::ptr::null_mut()));
    r.target.set(slot as *const c_void);
    r.run(|_, w| match w[0] {
        "new" => {
            // SAFETY: the test's slot
            unsafe { *slot = ee_ui_entity_group_new(core::ptr::null_mut(), Some(host_current)) };
            true
        }
        "free" => {
            // SAFETY: made by the new line
            unsafe { ee_ui_entity_group_free(*slot) };
            true
        }
        _ => false,
    });
    // SAFETY: made above
    drop(unsafe { Box::from_raw(slot) });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 500);
}

#[test]
#[should_panic(expected = "golden line")]
fn every_entity_a_button_fails() {
    let g: Vec<String> = GOLDEN
        .lines()
        .map(|l| match l.strip_prefix("set 640 ") {
            Some(rest) => {
                let w: Vec<&str> = rest.split(' ').collect();
                format!("set 640 {} 1 {}", w[0], w[2..].join(" "))
            }
            None => l.to_string(),
        })
        .collect();
    replay(&g.join("\n"));
}

#[test]
fn a_new_group_is_disabled_and_empty() {
    let g = ee_ui_entity_group_new(core::ptr::null_mut(), None);
    // SAFETY: made above, freed once
    unsafe {
        assert_eq!(ee_ui_entity_group_call(g, EeUiEntityGroupCall::IsEnabled, 0), 0);
        assert_eq!(ee_ui_entity_group_call(g, EeUiEntityGroupCall::Count, 0), 0);
        assert_eq!(ee_ui_entity_group_call(g, EeUiEntityGroupCall::Remove, 7), 3);
        ee_ui_entity_group_call(g, EeUiEntityGroupCall::Enable, 0);
        assert_eq!(ee_ui_entity_group_call(g, EeUiEntityGroupCall::IsEnabled, 0), 1);
        ee_ui_entity_group_free(g);
    }
}
