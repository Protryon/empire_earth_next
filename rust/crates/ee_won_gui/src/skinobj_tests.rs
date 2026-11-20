//! The skin's maps and its lifetime (the statements themselves: tests/won/won_wonskin_test.cpp).

use super::*;
use crate::shell::tests::{host, take};

fn units(s: &str) -> Vec<u32> {
    s.chars().map(|c| c as u32).collect()
}

#[test]
fn the_windows_look_makes_every_template_and_frees_them_in_reverse() {
    let s = ee_won_skin_new(Some(host));
    let made = take();
    assert!(made.iter().any(|l| l.starts_with("GetNamedFont")));
    // SAFETY: made above
    unsafe {
        for i in 0..SLOTS {
            assert_eq!((*s).slots[i].get(), 7, "slot {i}");
        }
        ee_won_skin_free(s);
    }
    let freed = take();
    assert_eq!(freed.len(), SLOTS);
    assert!(freed.iter().all(|l| l == "Ref 7 -1 0 0"));
}

#[test]
fn the_type_maps_ignore_case_and_keep_their_order() {
    let s = ee_won_skin_new(Some(host));
    take();
    // SAFETY: made above
    let k = unsafe { &*s };
    k.set_type(false, units("Beta"), 0x31);
    k.set_type(false, units("alpha"), 0x32);
    k.set_type(false, units("ALPHA"), 0x33);
    assert_eq!(take(), ["Ref 49 1 0 0", "Ref 50 1 0 0", "Ref 50 -1 0 0", "Ref 51 1 0 0"]);
    let keys: Vec<isize> = k.button_types.borrow().iter().map(|(_, c)| c.get()).collect();
    assert_eq!(keys, [0x33, 0x31]);
    assert!(k.copy_type(false, &units("BETA"), 0x40));
    assert_eq!(take(), ["WonBtnCopy 64 49 0 0"]);
    assert!(!k.copy_type(true, &units("beta"), 0x40));
    k.click_sound(0x70);
    assert_eq!(take().len(), 7 + 2);
    // SAFETY: made above
    unsafe { ee_won_skin_free(s) };
    take();
}
