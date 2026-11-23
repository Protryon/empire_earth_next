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

/// The file lists' `FlDirFirst`: directory `c` of the path `b` into the search `a`, then its first file.
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn file_dir_first(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    extern "C" {
        fn ee_uicpp_fl_dir_get(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32);
        fn ee_uicpp_fl_first(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize;
    }
    // SAFETY: the contract
    unsafe {
        ee_uicpp_fl_dir_get(o, a, b, c, d, f);
        ee_uicpp_fl_first(o, a, b, c, d, f)
    }
}

/// The file lists' `FlDeleteFile`: the file named by the bytes at `a` found on the path `b` and deleted; `f[0]`'s
/// bits whether it was found; the find's (or the delete's) result.
///
/// # Safety
/// The host contract: `f` four live floats.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn file_delete(_o: *mut c_void, a: isize, b: isize, _c: isize, _d: isize, f: *mut f32) -> isize {
    extern "C" {
        fn ee_uicpp_fl_find_file(name: isize, path: isize, file: *mut *mut c_void) -> isize;
        fn ee_uicpp_fl_file_delete(file: *mut c_void) -> isize;
        fn ee_uicpp_fl_file_free(file: *mut c_void);
    }
    let mut file = core::ptr::null_mut();
    // SAFETY: the contract; file is the find's out
    let mut result = unsafe { ee_uicpp_fl_find_file(a, b, &mut file) };
    let found = result == 0;
    if found {
        // SAFETY: the file the find made
        unsafe {
            result = ee_uicpp_fl_file_delete(file);
            ee_uicpp_fl_file_free(file);
        }
    }
    // SAFETY: the contract
    unsafe { *f = f32::from_bits(found as u32) };
    result
}

/// UIFileListBox's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_file_list_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[file_shared, file_list], o, op, a, b, c, d, f) }
}

/// UIFileComboBox's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_file_combo_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[file_shared, file_combo], o, op, a, b, c, d, f) }
}

/// The hot keys' `HkEntity`: the keyboard's entity of key `a` (`a` itself when there is none, the original's
/// unset local).
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn hotkey_entity(_o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    let mut e = a as *mut c_void;
    // SAFETY: e is the shim's in-out entity
    unsafe { hotkey_get_entity(a, &mut e) };
    e as isize
}

/// The hot keys' `HkEntityResult`: the keyboard's lookup of key `a`'s entity, its result.
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn hotkey_entity_result(_o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    let mut e = a as *mut c_void;
    // SAFETY: e is the shim's in-out entity
    unsafe { hotkey_get_entity(a, &mut e) }
}

/// The hot keys' `HkEventCall`: event `a`'s 0 `Uninitialize`, 1 `Dereference`, 2 `Reference`, else `Initialize`.
///
/// # Safety
/// The host contract: `a` a live event.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn hotkey_event_call(_o: *mut c_void, a: isize, b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    use crate::lle_cpp::Cpp;
    let mut cpp = crate::lle_cpp::Ffi;
    let e = a as usize;
    match b {
        0 => cpp.event_uninitialize(e),
        1 => cpp.event_dereference(e),
        2 => cpp.event_reference(e),
        _ => cpp.event_initialize(e),
    }
    0
}

/// The keyboard's entity of a key into `entity` (UIHotKey.cpp), the lookup's result.
///
/// # Safety
/// `entity` a live pointer.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn hotkey_get_entity(key: isize, entity: *mut *mut c_void) -> isize {
    extern "C" {
        fn ee_uicpp_hk_get_entity(key: isize, entity: *mut *mut c_void) -> isize;
    }
    // SAFETY: the contract
    unsafe { ee_uicpp_hk_get_entity(key, entity) }
}

/// UIHotKey's host (its object the record).
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[hotkey], o, op, a, b, c, d, f) }
}


/// The font table's `TblFileReadCount`: the file `a`'s next count, packed with the read's result (count << 32).
///
/// # Safety
/// The host contract: `a` a live open file.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn font_file_read_count(_o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    use crate::lle_cpp::{Cpp, EeUiShimULongOut};
    let mut count: core::ffi::c_ulong = 0;
    let result = crate::lle_cpp::Ffi.file_read_count(a as usize, EeUiShimULongOut::of(&mut count));
    ((count as u64) << 32 | result as u32 as u64) as isize
}

/// The font table's `TblFileRead`: `c` bytes of the file `a` into `b`.
///
/// # Safety
/// The host contract: `b` `c` writable bytes.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn font_file_read(_o: *mut c_void, a: isize, b: isize, c: isize, _d: isize, _f: *mut f32) -> isize {
    use crate::lle_cpp::{Cpp, EeUiShimU8Out};
    crate::lle_cpp::Ffi.file_read(a as usize, EeUiShimU8Out(b as *mut u8), c as core::ffi::c_ulong) as isize
}

/// The font table's `FontNew`: a new font (name `a`, size `d`, style `b`, quality `c`), referenced.
///
/// # Safety
/// The host contract: `a` a live UString.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn font_new(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    extern "C" {
        fn ee_uicpp_font_create(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize;
    }
    // SAFETY: the contract; the reference is the new font's
    unsafe {
        let font = ee_uicpp_font_create(o, a, b, c, d, f);
        ee_uicpp_font_reference(o, font, 0, 0, 0, f);
        font
    }
}

/// UIFontTable's host.
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_font_table_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[fonts], o, op, a, b, c, d, f) }
}

/// The background table's `BgtFileCount`: the file's (the object's) next count in `f[0]`'s bits, the read's result.
///
/// # Safety
/// The host contract: `o` a live open file, `f` four live floats.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn bg_table_file_count(o: *mut c_void, _a: isize, _b: isize, _c: isize, _d: isize, f: *mut f32) -> isize {
    use crate::lle_cpp::{Cpp, EeUiShimULongOut};
    let mut count: core::ffi::c_ulong = 0;
    let result = crate::lle_cpp::Ffi.file_read_count(o as usize, EeUiShimULongOut::of(&mut count));
    // SAFETY: the contract
    unsafe { *f = f32::from_bits(count as u32) };
    result as isize
}

/// UIBackgroundTable's host (its object the file or the manager).
///
/// # Safety
/// The host contract: `o` the class's live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_bg_table_host(
    o: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe { chain(&[bg_table], o, op, a, b, c, d, f) }
}
