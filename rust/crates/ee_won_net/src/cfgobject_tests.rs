//! The group's own bookkeeping over a scripted host (the parse runs against the original statements in
//! `won_cfgobject_test`): the maps' case-insensitive order and first-registration rule, the child iteration's end
//! (a node added after the end is not reached), the references taken and given back, the queries and the copy.

use super::*;
use std::cell::RefCell;

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn take() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

/// Objects and values are small integers; a value's text is "v<n>", its default flag n even.
unsafe extern "C" fn host(
    op: EeWonCfgoOp,
    o: *mut c_void,
    a: isize,
    b: isize,
    _c: isize,
    _d: isize,
    _x: *mut c_void,
) -> isize {
    let n = o as isize;
    match op {
        EeWonCfgoOp::ValWrite => {
            let t = format!("v{n}");
            unsafe {
                (std::mem::transmute::<isize, crate::ffi::EeWonByteSink>(a))(b as *mut c_void, t.as_ptr(), t.len())
            };
            0
        }
        EeWonCfgoOp::ValIsDefault => (n % 2 == 0) as isize,
        _ => {
            log(format!("{op:?} {n} {a}"));
            0
        }
    }
}

fn group() -> *mut EeWonCfgObject {
    ee_won_cfgo_new(1000 as *mut c_void, host)
}

#[test]
fn keys_keep_their_first_registration_in_case_insensitive_order() {
    let g = group();
    unsafe {
        ee_won_cfgo_register_key_value(g, b"b".as_ptr(), 1, 2 as *mut c_void);
        ee_won_cfgo_register_key_value(g, b"A".as_ptr(), 1, 3 as *mut c_void);
        ee_won_cfgo_register_key_value(g, b"B".as_ptr(), 1, 4 as *mut c_void);
    }
    assert_eq!(take(), ["ValAddRef 2 0", "ValAddRef 3 0", "ValAddRef 4 0", "ValRelease 4 0"]);
    let mut out = Vec::new();
    unsafe { ee_won_cfgo_get_all_key_values(g, sink_vec, &mut out as *mut Vec<u8> as *mut c_void) };
    assert_eq!(out, b"A: v3\nb: v2  [DEF]\n");
    let mut one = Vec::new();
    assert!(unsafe {
        ee_won_cfgo_get_key_value(g, b"a".as_ptr(), 1, sink_vec, &mut one as *mut Vec<u8> as *mut c_void)
    });
    assert_eq!(one, b"v3");
    assert!(!unsafe {
        ee_won_cfgo_get_key_value(g, b"c".as_ptr(), 1, sink_vec, &mut one as *mut Vec<u8> as *mut c_void)
    });
    unsafe { ee_won_cfgo_free(g) };
    assert_eq!(take(), ["ValRelease 3 0", "ValRelease 2 0"]);
}

#[test]
fn the_iteration_end_stays_the_end() {
    let g = group();
    unsafe {
        ee_won_cfgo_rewind(g);
        ee_won_cfgo_add_child(g, 7 as *mut c_void);
        assert!(ee_won_cfgo_next_child(g).is_null(), "rewound on an empty list: at the end");
        ee_won_cfgo_rewind(g);
        assert_eq!(ee_won_cfgo_next_child(g) as isize, 7);
        ee_won_cfgo_add_child(g, 8 as *mut c_void);
        assert!(ee_won_cfgo_next_child(g).is_null(), "the end does not move to a new node");
        ee_won_cfgo_rewind(g);
        assert_eq!(ee_won_cfgo_next_child(g) as isize, 7);
        ee_won_cfgo_add_child(g, 9 as *mut c_void);
        assert_eq!(ee_won_cfgo_next_child(g) as isize, 8);
        assert_eq!(ee_won_cfgo_next_child(g) as isize, 9);
        assert_eq!(ee_won_cfgo_child_count(g), 3);
    }
    assert_eq!(
        take(),
        [
            "ObjSetParent 7 1000",
            "ObjAddRef 7 0",
            "ObjSetParent 8 1000",
            "ObjAddRef 8 0",
            "ObjSetParent 9 1000",
            "ObjAddRef 9 0"
        ]
    );
    unsafe { ee_won_cfgo_clear(g) };
    assert_eq!(take(), ["ObjRelease 7 0", "ObjRelease 8 0", "ObjRelease 9 0"]);
    unsafe { ee_won_cfgo_free(g) };
}

#[test]
fn reset_clears_names_values_and_group_pointers() {
    let g = group();
    unsafe {
        ee_won_cfgo_register_key_value(g, b"k".as_ptr(), 1, 5 as *mut c_void);
        ee_won_cfgo_register_key_group(g, b"G".as_ptr(), 1, 11, 22, std::ptr::null_mut());
        ee_won_cfgo_register_key_group(g, b"F".as_ptr(), 1, 0, 0, 33 as *mut c_void);
        ee_won_cfgo_register_key_group(g, b"f".as_ptr(), 1, 0, 0, 44 as *mut c_void);
        take();
        ee_won_cfgo_reset(g, true);
    }
    assert_eq!(take(), ["ObjSetName 1000 0", "ObjSetName 1000 1", "ValInit 5 0", "ValSetDefault 5 1", "GroupSet 0 22"]);
    unsafe { ee_won_cfgo_free(g) };
    assert_eq!(take(), ["ObjRelease 33 0", "ValRelease 5 0"]);
}

#[test]
fn copy_walks_both_maps_in_order() {
    let (a, b) = (group(), group());
    unsafe {
        for (k, v) in [(&b"x"[..], 1), (b"Y", 2), (b"z", 3)] {
            ee_won_cfgo_register_key_value(a, k.as_ptr(), k.len(), v as *mut c_void);
        }
        for (k, v) in [(&b"w"[..], 4), (b"y", 5), (b"Z", 6)] {
            ee_won_cfgo_register_key_value(b, k.as_ptr(), k.len(), v as *mut c_void);
        }
        take();
        ee_won_cfgo_copy_key_vals_from(a, b);
    }
    assert_eq!(take(), ["ValCopy 2 5", "ValCopy 3 6"]);
    unsafe {
        ee_won_cfgo_free(a);
        ee_won_cfgo_free(b);
    }
}

#[test]
fn includes_take_the_directory_and_the_flag() {
    assert_eq!(include_path(b"d/x.cfg", true, b"base/"), b"base/x.cfg");
    assert_eq!(up_to_nul(b"ab\0c"), b"ab");
    assert_eq!(read_string(b"\"q w\" rest", false), b"q w");
}
