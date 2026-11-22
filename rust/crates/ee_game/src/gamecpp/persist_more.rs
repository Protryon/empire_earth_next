//! The saved-calamities chunk's own bodies in Rust (game core lane, round 10; was EEPCalamityHelper.cpp's
//! constructor, destructor, Reset and operator=): the chunk's calamity list and contagion manager made, deleted and
//! copied. Each C++ call or member is a case of EEPCalamityHelper.cpp's `EECore10OpB8` or of round 9's
//! `EEPCalamityOp`; the methods forward here (`ee_gamecpp_pch_*`).

#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::c_void;

use super::{h, hp, pa, O, P};

/// The chunk's constructor (`fresh` only: a new contagion manager), destructor (`clear` only: the calamity list and
/// the contagion manager deleted) and Reset (both).
#[no_mangle]
pub extern "C" fn ee_gamecpp_pch_reset(helper: *mut c_void, clear: bool, fresh: bool) {
    let c = helper as P;
    if clear {
        h(O::PchClearCalamities, c, &[]);
        h(O::PchClearContagion, c, &[]);
    }
    if fresh {
        h(O::PchNewContagion, c, &[]);
    }
}

/// EEPCalamityHelper::operator=: another chunk's calamity list and contagion manager copied into fresh ones of its
/// own (each only when the two differ).
#[no_mangle]
pub extern "C" fn ee_gamecpp_pch_assign(helper: *mut c_void, other: *const c_void) -> *mut c_void {
    let (c, o) = (helper as P, other as P);
    if hp(O::PchCalamities, c, &[]) != hp(O::PchCalamities, o, &[]) {
        h(O::PchClearCalamities, c, &[]);
        h(O::PchNewCalamities, c, &[]);
        if hp(O::PchCalamities, o, &[]) != 0 {
            h(O::PchCopyCalamities, c, &[pa(o)]);
        }
    }
    if hp(O::PchContagion, c, &[]) != hp(O::PchContagion, o, &[]) {
        h(O::PchClearContagion, c, &[]);
        h(O::PchNewContagion, c, &[]);
        if hp(O::PchContagion, o, &[]) != 0 {
            h(O::PchCopyContagion, c, &[pa(o)]);
        }
    }
    helper
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_reset_with_nothing_to_do_calls_no_cpp() {
        ee_gamecpp_pch_reset(std::ptr::null_mut(), false, false);
    }
}
