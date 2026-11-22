//! The toolkit's C++ hosts as Rust tables (UI lane round 14): each Rust object calls out through a host
//! (`EeUiHostFn`: the object, the op, four numbers and the float buffer); the host is a chain of tables, each of
//! which names, per op, the one-statement C++ shim of the op's case (`EE_UI_CASE` / `EE_UI_VOID` in
//! `lle/UIShims.h`, `ee_uicpp_*` in `src/Low-Level Engine/User Interface`). The C++ op switches (`UIControl::HostOp`,
//! `UIObjHost::Base`, ...) are gone; a multi-call case is a Rust function over the shims.
//!
//! A table answers `None` for an op that is not its; the host answers 0 when no table of its chain has the op (what
//! the C++ switches' `default` did). The chain's order is the C++ hosts' (`UIObjHost::Base`, then `Media`, then
//! `Part`, then the class's own cases).

#![cfg_attr(any(test, feature = "no_cpp"), allow(dead_code, unused_macros, unused_imports))]

use core::ffi::c_void;

use crate::obj::EeUiOp;

/// A table: the op's answer, `None` when the op is not the table's.
pub(crate) type Table = unsafe fn(*mut c_void, EeUiOp, isize, isize, isize, isize, *mut f32) -> Option<isize>;

/// The answer of the first table of `tables` that has `op` (0 when none has it).
///
/// # Safety
/// The host contract: `o` the live object of the tables' class, `f` the op's live floats.
#[allow(clippy::too_many_arguments)]
pub(crate) unsafe fn chain(
    tables: &[Table],
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    for t in tables {
        // SAFETY: the contract
        if let Some(r) = unsafe { t(o, op, a, b, c, d, f) } {
            return r;
        }
    }
    0
}

/// Tables of shims: `name { Op => r shim, Op => v shim, Op => x rust_fn, }`; `r` answers the shim's number, `v`
/// calls a void shim and answers 0, `x` calls a Rust function of the table's arguments (a multi-call case).
macro_rules! tables {
    ($( $(#[$m:meta])* $name:ident { $( $op:ident => $kind:ident $sym:path, )* } )*) => {$(
        $(#[$m])*
        #[cfg(not(any(test, feature = "no_cpp")))]
        #[allow(clippy::too_many_arguments, unused_variables)]
        pub(crate) unsafe fn $name(
            o: *mut c_void,
            op: EeUiOp,
            a: isize,
            b: isize,
            c: isize,
            d: isize,
            f: *mut f32,
        ) -> Option<isize> {
            Some(match op {
                // SAFETY: the host contract: the C++ case's own arguments
                $( EeUiOp::$op => tables!(@call $kind $sym, o, a, b, c, d, f), )*
                _ => return None,
            })
        }
    )*};
    (@call r $sym:path, $($x:ident),*) => { unsafe { $sym($($x),*) } };
    (@call v $sym:path, $($x:ident),*) => {{ unsafe { $sym($($x),*) }; 0 }};
    (@call x $sym:path, $($x:ident),*) => { unsafe { $sym($($x),*) } };
}

/// The shims' declarations: `r name;` answers a number, `v name;` nothing.
macro_rules! shims {
    ($( $kind:ident $sym:ident; )*) => {
        #[cfg(not(any(test, feature = "no_cpp")))]
        #[allow(dead_code)]
        mod sys {
            use core::ffi::c_void;
            extern "C" {
                $( shims!(@decl $kind $sym); )*
            }
        }
    };
    (@decl r $sym:ident) => { pub fn $sym(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize; };
    (@decl v $sym:ident) => { pub fn $sym(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32); };
}

#[cfg(not(any(test, feature = "no_cpp")))]
use sys::*;

include!("lle_hosts_shims.rs");
include!("lle_hosts_tables.rs");

/// `UIObjHost::Base`'s Enter and Space key ops (a: bit 0 Enter, else Space; a < 2 just pressed, else just released).
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn base_key(_o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    use crate::lle_cpp::Cpp;
    use crate::lle_control::{key, EeUiKey};
    let k = key(if a & 1 != 0 { EeUiKey::Enter } else { EeUiKey::Space });
    let mut cpp = crate::lle_cpp::Ffi;
    (if a < 2 { cpp.entity_just_pressed(k) } else { cpp.entity_just_unpressed(k) }) as isize
}

/// `UIObjHost::Media`'s text extent: the text's width and height in `f[0]`, `f[1]`.
///
/// # Safety
/// The host contract: `f` four live floats.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn media_text_extent(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    extern "C" {
        fn ee_uicpp_obj_text_extent(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32);
    }
    // the extent rectangle (bottom, left, right, top)
    let mut r = [0.0f32; 4];
    // SAFETY: the contract; r is the shim's four floats
    unsafe {
        ee_uicpp_obj_text_extent(o, a, b, c, d, r.as_mut_ptr());
        *f = r[2] - r[1];
        *f.add(1) = r[0] - r[3];
    }
    0
}

/// `UIScalableButton`'s slice stretch: the source rectangle's floats come packed two per number in `c` and `d`
/// (variantobj.rs); the shim reads the destination and the source from eight floats.
///
/// # Safety
/// The host contract: `f` four live floats (the destination).
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn button_bitmap_stretch(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    extern "C" {
        fn ee_uicpp_button_bitmap_stretch(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32);
    }
    let pair = |v: isize| [f32::from_bits((v as u64 >> 32) as u32), f32::from_bits(v as u64 as u32)];
    let (s0, s1) = (pair(c), pair(d));
    // SAFETY: the contract: f holds the destination's four floats
    let mut buf = unsafe { [*f, *f.add(1), *f.add(2), *f.add(3), s0[0], s0[1], s1[0], s1[1]] };
    // SAFETY: buf is the shim's eight floats
    unsafe { ee_uicpp_button_bitmap_stretch(o, a, b, c, d, buf.as_mut_ptr()) };
    0
}

/// UIButton's host (and its subclasses'): the base, media and part ops, then the button's.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_button_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, media, part, button], o, op, a, b, c, d, f) }
}

/// UIGraph's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_graph_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, media, graph], o, op, a, b, c, d, f) }
}

/// UISlider's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_slider_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, media, slider], o, op, a, b, c, d, f) }
}

/// UIStatusBar's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_status_bar_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, media, status], o, op, a, b, c, d, f) }
}

/// UITextLabel's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_label_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, media, part, label], o, op, a, b, c, d, f) }
}

/// UIScrollBar's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_scroll_bar_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, part, scroll_bar], o, op, a, b, c, d, f) }
}

/// The list boxes' ops that do not depend on T (`UIListBoxRust<T>::Op`'s default).
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_list_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, part, list], o, op, a, b, c, d, f) }
}

/// UIBitmap's host (its object the control: UIBitmap::Op converts the record's address).
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bitmap_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[base, media], o, op, a, b, c, d, f) }
}

/// UIEntityGroup's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_group_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[group], o, op, a, b, c, d, f) }
}

/// UIFullBackground's host (the backgrounds' C++ cases without ee_ui_background_host's moved ones).
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_background_full_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[background], o, op, a, b, c, d, f) }
}

/// The hosts still C++ (`UIComboBox::Rust::Op`, `UIControlsTableHost::Op`): each a table of every op.
macro_rules! cpp_hosts {
    ($( $(#[$m:meta])* $name:ident = $sym:ident; )*) => {$(
        $(#[$m])*
        #[cfg(not(any(test, feature = "no_cpp")))]
        pub(crate) unsafe fn $name(
            o: *mut c_void,
            op: EeUiOp,
            a: isize,
            b: isize,
            c: isize,
            d: isize,
            f: *mut f32,
        ) -> Option<isize> {
            extern "C" {
                fn $sym(o: *mut c_void, op: EeUiOp, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize;
            }
            // SAFETY: the host contract
            Some(unsafe { $sym(o, op, a, b, c, d, f) })
        }
    )*};
}

cpp_hosts! {
    /// The combo box's.
    combo = ee_uicpp_combo_host;
    /// The controls table's.
    table = ee_uicpp_table_host;
}
