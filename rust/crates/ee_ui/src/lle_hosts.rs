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
    use crate::lle_control::{key, EeUiKey};
    use crate::lle_cpp::Cpp;
    let k = key(if a & 1 != 0 { EeUiKey::Enter } else { EeUiKey::Space });
    let mut cpp = crate::lle_cpp::Ffi;
    (if a < 2 { cpp.entity_just_pressed(k) } else { cpp.entity_just_unpressed(k) }) as isize
}

/// `CreateBackgrounds`' background for the entry `a` on the form `b` (texture description `c`): the entry's type's
/// class (`UIBackgroundType`), a tiled one with its tiling and, stretched, its across and down percents; another
/// type the manager's user-defined background.
///
/// # Safety
/// The host contract: `a` a live `EeUiBgTableFields` (the `UIBackgroundTable`'s base).
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn bgt_make(o: *mut c_void, a: isize, b: isize, c: isize, _d: isize, _f: *mut f32) -> isize {
    use crate::ids::UITiling as T;
    // SAFETY: the contract
    let e = unsafe { *(a as *const crate::bgtableobj::EeUiBgTableFields) };
    let mut f = [0.0f32; 4];
    let tiled = |tiling: T, f: &mut [f32; 4]| {
        // SAFETY: the contract
        unsafe { ee_uicpp_bgt_make_tiled(o, a, b, c, tiling as isize, f.as_mut_ptr()) }
    };
    // SAFETY: the contract
    unsafe {
        match e.mBackgroundType {
            0 => ee_uicpp_bgt_make_bitmap(o, a, b, c, 0, f.as_mut_ptr()),
            1 => tiled(T::kTilingBoth, &mut f),
            2 => ee_uicpp_bgt_make_full(o, a, b, c, 0, f.as_mut_ptr()),
            3 => tiled(T::kTilingLeftRight, &mut f),
            4 => tiled(T::kTilingTopBottom, &mut f),
            5 => {
                f[0] = e.mTileWidthPercent;
                f[1] = e.mTileHeightPercent;
                tiled(T::kTilingStretched, &mut f)
            }
            _ => ee_uicpp_bgt_make_user(o, a, b, c, 0, f.as_mut_ptr()),
        }
    }
}

/// The combo box's `ComboQuery` (`a`: the query in the top 8 bits, its argument below) and `ComboAct` (`a` the
/// action, `b`, `c`, `d`, `f` its arguments): one shim each; 0 for one that has none.
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn combo_query(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    // SAFETY: the contract (a query's argument goes in a)
    unsafe { combo_shim(combo_query_shim(a >> 24), o, a & 0xff_ffff, b, c, d, f) }
}

/// See [`combo_query`].
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn combo_act(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    use crate::comboobj::EeUiComboAct as A;
    let shim = match a {
        x if x == A::ListChangeSelection as isize => V(ee_uicpp_combo_a_list_change_selection),
        x if x == A::ListInsertLine as isize => V(ee_uicpp_combo_a_list_insert_line),
        x if x == A::ListDeleteLine as isize => V(ee_uicpp_combo_a_list_delete_line),
        x if x == A::ListDeleteAll as isize => V(ee_uicpp_combo_a_list_delete_all),
        x if x == A::ListChangeWidth as isize => V(ee_uicpp_combo_a_list_change_width),
        x if x == A::PartMouseDown as isize => R(ee_uicpp_combo_a_part_mouse_down),
        x if x == A::PartMouseUp as isize => R(ee_uicpp_combo_a_part_mouse_up),
        x if x == A::PartMouseMove as isize => R(ee_uicpp_combo_a_part_mouse_move),
        x if x == A::ListWheel as isize => R(ee_uicpp_combo_a_list_wheel),
        x if x == A::PartUpdateKeyboard as isize => R(ee_uicpp_combo_a_part_update_keyboard),
        x if x == A::PartTranslatedKey as isize => R(ee_uicpp_combo_a_part_translated_key),
        x if x == A::PartSetRectangle as isize => V(ee_uicpp_combo_a_part_set_rectangle),
        x if x == A::PartSetLocation as isize => V(ee_uicpp_combo_a_part_set_location),
        x if x == A::PartCopyData as isize => R(ee_uicpp_combo_a_part_copy_data),
        x if x == A::BaseCopyData as isize => R(ee_uicpp_combo_a_base_copy_data),
        x if x == A::SetPopup as isize => V(ee_uicpp_combo_a_set_popup),
        x if x == A::SetDirty as isize => V(ee_uicpp_combo_a_set_dirty),
        x if x == A::SetRectangle as isize => V(ee_uicpp_combo_a_set_rectangle),
        x if x == A::BaseSetRectangle as isize => V(ee_uicpp_combo_a_base_set_rectangle),
        x if x == A::BaseSetLocation as isize => V(ee_uicpp_combo_a_base_set_location),
        x if x == A::SetAlwaysDirty as isize => V(ee_uicpp_combo_a_set_always_dirty),
        x if x == A::Fire as isize => V(ee_uicpp_combo_a_fire),
        x if x == A::CaptureMouse as isize => V(ee_uicpp_combo_a_capture_mouse),
        x if x == A::CaptureKeyboard as isize => V(ee_uicpp_combo_a_capture_keyboard),
        x if x == A::ReleaseMouse as isize => V(ee_uicpp_combo_a_release_mouse),
        x if x == A::ReleaseKeyboard as isize => V(ee_uicpp_combo_a_release_keyboard),
        x if x == A::OpenList as isize => V(ee_uicpp_combo_a_open_list),
        x if x == A::CloseList as isize => V(ee_uicpp_combo_a_close_list),
        x if x == A::AddString as isize => R(ee_uicpp_combo_a_add_string),
        x if x == A::ChangeSelection as isize => V(ee_uicpp_combo_a_change_selection),
        _ => None,
    };
    // SAFETY: the contract
    unsafe { combo_shim(shim, o, a, b, c, d, f) }
}

/// The shim of a combo box query.
#[cfg(not(any(test, feature = "no_cpp")))]
fn combo_query_shim(q: isize) -> Option<ComboShim> {
    use crate::comboobj::EeUiComboQuery as Q;
    match q {
        x if x == Q::RectOf as isize => V(ee_uicpp_combo_q_rect_of),
        x if x == Q::TextBoxBorder as isize => R(ee_uicpp_combo_q_text_box_border),
        x if x == Q::LineCount as isize => R(ee_uicpp_combo_q_line_count),
        x if x == Q::Line as isize => R(ee_uicpp_combo_q_line),
        x if x == Q::LineHeight as isize => R(ee_uicpp_combo_q_line_height),
        x if x == Q::ListBorder as isize => R(ee_uicpp_combo_q_list_border),
        x if x == Q::ListScrollBarCaptured as isize => R(ee_uicpp_combo_q_list_scroll_bar_captured),
        x if x == Q::WindowHeight as isize => R(ee_uicpp_combo_q_window_height),
        x if x == Q::EditText as isize => R(ee_uicpp_combo_q_edit_text),
        x if x == Q::TabbingOn as isize => R(ee_uicpp_combo_q_tabbing_on),
        x if x == Q::TabStop as isize => R(ee_uicpp_combo_q_tab_stop),
        x if x == Q::HasMouseCapture as isize => R(ee_uicpp_combo_q_has_mouse_capture),
        _ => None,
    }
}

/// A combo box shim: a number's or a void one.
#[cfg(not(any(test, feature = "no_cpp")))]
#[derive(Clone, Copy)]
enum ComboShim {
    R(unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32) -> isize),
    V(unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32)),
}

#[cfg(not(any(test, feature = "no_cpp")))]
#[allow(non_snake_case)]
fn R(f: unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32) -> isize) -> Option<ComboShim> {
    Some(ComboShim::R(f))
}

#[cfg(not(any(test, feature = "no_cpp")))]
#[allow(non_snake_case)]
fn V(f: unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32)) -> Option<ComboShim> {
    Some(ComboShim::V(f))
}

/// Calls the shim (0 for none or a void one).
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn combo_shim(
    s: Option<ComboShim>,
    o: *mut c_void,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    // SAFETY: the contract
    unsafe {
        match s {
            Some(ComboShim::R(r)) => r(o, a, b, c, d, f),
            Some(ComboShim::V(v)) => {
                v(o, a, b, c, d, f);
                0
            }
            None => 0,
        }
    }
}

/// The manager's form calls (`b`: managerobj.rs F_OPEN .. F_DELETE) -> the call's result (0 for the void ones).
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn mgr_form_call(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    let r: unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32) -> isize = match b {
        6 => ee_uicpp_mgr_form_copy,
        7 => ee_uicpp_mgr_form_draw_polygons,
        8 => ee_uicpp_mgr_form_draw_text,
        9 => ee_uicpp_mgr_form_update_keyboard,
        10 => ee_uicpp_mgr_form_update_mouse,
        _ => {
            let v: unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32) = match b {
                0 => ee_uicpp_mgr_form_open,
                1 => ee_uicpp_mgr_form_close,
                2 => ee_uicpp_mgr_form_activate,
                3 => ee_uicpp_mgr_form_deactivate,
                4 => ee_uicpp_mgr_form_clear_clips,
                5 => ee_uicpp_mgr_form_all_dirty,
                _ => ee_uicpp_mgr_form_delete,
            };
            // SAFETY: the contract
            unsafe { v(o, a, b, c, d, f) };
            return 0;
        }
    };
    // SAFETY: the contract
    unsafe { r(o, a, b, c, d, f) }
}

/// The manager's form calls with a rectangle or a point in `f` (`b`: managerobj.rs R_ADD_DIRTY .. R_MOUSE_LOCATION).
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn mgr_form_rect_call(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    let v: unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32) = match b {
        0 => ee_uicpp_mgr_form_add_dirty,
        1 => ee_uicpp_mgr_form_redraw,
        2 => ee_uicpp_mgr_form_add_clip,
        3 => ee_uicpp_mgr_form_relayout,
        _ => ee_uicpp_mgr_form_mouse_location,
    };
    // SAFETY: the contract
    unsafe { v(o, a, b, c, d, f) };
    0
}

/// The manager's IME calls on form `a` (`b` a manager_media.rs `ImeCall`, `c` its argument) -> the form's result.
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn mgr_form_ime(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    // SAFETY: the contract
    unsafe {
        match b {
            0 => {
                ee_uicpp_mgr_form_ime_result(o, a, b, c, d, f);
                0
            }
            1 => ee_uicpp_mgr_form_ime_change(o, a, b, c, d, f),
            2 => ee_uicpp_mgr_form_ime_close(o, a, b, c, d, f),
            _ => ee_uicpp_mgr_form_ime_open(o, a, b, c, d, f),
        }
    }
}

/// The form's temporary hot key (AddHotKey's): made, on the form, its key `a` -> the hot key.
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn form_hot_key_temp(o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, f: *mut f32) -> isize {
    // SAFETY: the contract
    unsafe {
        let k = ee_uicpp_form_hot_key_new(o, 0, 0, 0, 0, f);
        ee_uicpp_form_hot_key_set_form(o, k, 0, 0, 0, f);
        ee_uicpp_form_hot_key_set_key(o, k, a, 0, 0, f);
        k
    }
}

/// The keyboard's input entity for key `a` (NULL when it has none).
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn form_key_entity(o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, f: *mut f32) -> isize {
    let mut e: *mut c_void = core::ptr::null_mut();
    // SAFETY: the contract; the slot is live for the call
    unsafe { ee_uicpp_form_key_entity_get(o, a, &mut e as *mut *mut c_void as isize, 0, 0, f) };
    e as isize
}

/// The mouse's input entity for button `a` (NULL when it has none).
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn form_mouse_entity(o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, f: *mut f32) -> isize {
    let mut e: *mut c_void = core::ptr::null_mut();
    // SAFETY: the contract; the slot is live for the call
    unsafe { ee_uicpp_form_mouse_entity_get(o, a, &mut e as *mut *mut c_void as isize, 0, 0, f) };
    e as isize
}

/// The edit and list boxes' key questions (`EditKey`, edit_keys.rs' numbering): UIControl's key predicates, Control's
/// and Backspace's values, Enter's and the keypad Enter's presses; false for another number.
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn ui_key(_o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    use crate::lle_control::{ee_ui_key_pressed, key, EeUiKey as K, EeUiKeyPress as P};
    use crate::lle_cpp::Cpp;
    let mut c = crate::lle_cpp::Ffi;
    let p = |w: P| ee_ui_key_pressed(w);
    (match a {
        0 => p(P::Shift),
        1 => c.entity_value(key(K::Control)) != 0,
        2 => c.entity_value(key(K::BackSpace)) != 0,
        3 => p(P::Delete),
        4 => p(P::Right),
        5 => p(P::Left),
        6 => p(P::Home),
        7 => p(P::End),
        8 => p(P::Up),
        9 => p(P::Down),
        10 => p(P::Enter),
        11 => p(P::Cut),
        12 => p(P::Copy),
        13 => p(P::Paste),
        17 => c.entity_just_pressed(key(K::Enter)),
        18 => c.entity_just_pressed(key(K::NumEnter)),
        _ => false,
    }) as isize
}

/// The edit box's paste text (`PlatClipboardGetText`'s copy, kept until the next ask, which frees it).
#[cfg(not(any(test, feature = "no_cpp")))]
static CLIPBOARD: core::sync::atomic::AtomicPtr<core::ffi::c_char> =
    core::sync::atomic::AtomicPtr::new(core::ptr::null_mut());

/// The clipboard's text (NULL without one), the previous ask's freed.
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn clipboard_get(_o: *mut c_void, _a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
    extern "C" {
        fn PlatClipboardGetText() -> *mut core::ffi::c_char;
        fn free(p: *mut c_void);
    }
    // SAFETY: the platform's malloc'ed copy; the previous one is not used again
    unsafe {
        let text = PlatClipboardGetText();
        free(CLIPBOARD.swap(text, core::sync::atomic::Ordering::Relaxed) as *mut c_void);
        text as isize
    }
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

// ---- the controls table's constructors, setters and texts (UI round 16: UIControlsTableHost's switches)

/// A shim answering a number.
#[cfg(not(any(test, feature = "no_cpp")))]
type Shim = unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32) -> isize;
/// A shim answering nothing.
#[cfg(not(any(test, feature = "no_cpp")))]
type VoidShim = unsafe extern "C" fn(*mut c_void, isize, isize, isize, isize, *mut f32);

/// `TblMake`'s constructors by [`EeUiTableKind`](crate::tableobj::EeUiTableKind) (UIControlsTable.cpp).
#[cfg(not(any(test, feature = "no_cpp")))]
const MAKE: [Shim; 18] = [
    ee_uicpp_tbl_make_bitmap,
    ee_uicpp_tbl_make_button,
    ee_uicpp_tbl_make_blinking_button,
    ee_uicpp_tbl_make_scalable_button,
    ee_uicpp_tbl_make_button,
    ee_uicpp_tbl_make_check_box,
    ee_uicpp_tbl_make_combo_box,
    ee_uicpp_tbl_make_file_combo_box,
    ee_uicpp_tbl_make_edit_box,
    ee_uicpp_tbl_make_file_list_box,
    ee_uicpp_tbl_make_line_graph,
    ee_uicpp_tbl_make_list_box,
    ee_uicpp_tbl_make_radio_button,
    ee_uicpp_tbl_make_scroll_bar,
    ee_uicpp_tbl_make_scrolling_label,
    ee_uicpp_tbl_make_slider,
    ee_uicpp_tbl_make_status_bar,
    ee_uicpp_tbl_make_text_label,
];

/// `TblSet`'s setters by [`Set`] (`None`: Rust's, [`table_set`] and [`table_call`]).
#[cfg(not(any(test, feature = "no_cpp")))]
const SET: [Option<VoidShim>; 45] = [
    Some(ee_uicpp_tbl_set_button_text),
    Some(ee_uicpp_tbl_set_button_focused_color),
    Some(ee_uicpp_tbl_set_button_down_sound),
    Some(ee_uicpp_tbl_set_button_up_sound),
    None,
    Some(ee_uicpp_tbl_set_check_set_checked),
    Some(ee_uicpp_tbl_set_combo_focused_rect),
    Some(ee_uicpp_tbl_set_combo_selected_bg),
    Some(ee_uicpp_tbl_set_combo_selection),
    Some(ee_uicpp_tbl_set_combo_text_box_opacity),
    Some(ee_uicpp_tbl_set_combo_text_box_color),
    Some(ee_uicpp_tbl_set_combo_list_always_dirty),
    Some(ee_uicpp_tbl_set_edit_number),
    Some(ee_uicpp_tbl_set_edit_text),
    Some(ee_uicpp_tbl_set_edit_selected_bg),
    Some(ee_uicpp_tbl_set_list_focused_rect),
    Some(ee_uicpp_tbl_set_list_selected_bg),
    Some(ee_uicpp_tbl_set_list_scroll_opacity),
    Some(ee_uicpp_tbl_set_list_selection),
    None,
    Some(ee_uicpp_tbl_set_graph_font),
    Some(ee_uicpp_tbl_set_graph_font_color),
    Some(ee_uicpp_tbl_set_radio_up_unchecked),
    None,
    Some(ee_uicpp_tbl_set_scroll_range),
    Some(ee_uicpp_tbl_set_scroll_value),
    Some(ee_uicpp_tbl_set_slider_range),
    Some(ee_uicpp_tbl_set_slider_value),
    Some(ee_uicpp_tbl_set_slider_page),
    Some(ee_uicpp_tbl_set_status_range),
    Some(ee_uicpp_tbl_set_status_value),
    Some(ee_uicpp_tbl_set_status_color),
    Some(ee_uicpp_tbl_set_status_opacity),
    Some(ee_uicpp_tbl_set_status_text),
    Some(ee_uicpp_tbl_set_status_focused_color),
    Some(ee_uicpp_tbl_set_label_number),
    Some(ee_uicpp_tbl_set_label_align),
    Some(ee_uicpp_tbl_set_label_scroll_opacity),
    Some(ee_uicpp_tbl_set_label_fit),
    Some(ee_uicpp_tbl_set_bg_color),
    Some(ee_uicpp_tbl_set_bg_opacity),
    Some(ee_uicpp_tbl_set_always_dirty),
    Some(ee_uicpp_tbl_set_set_rectangle),
    Some(ee_uicpp_tbl_set_change_font),
    Some(ee_uicpp_tbl_set_set_trigger),
];

/// `SetupText`'s texts by class (tableobj.rs `TEXT_*`).
#[cfg(not(any(test, feature = "no_cpp")))]
const TEXT: [Shim; 8] = [
    ee_uicpp_tbl_text_button,
    ee_uicpp_tbl_text_check,
    ee_uicpp_tbl_text_combo,
    ee_uicpp_tbl_text_edit,
    ee_uicpp_tbl_text_list,
    ee_uicpp_tbl_text_radio,
    ee_uicpp_tbl_text_label,
    ee_uicpp_tbl_text_status,
];

/// `TblMake`: the constructor of the record at `a`'s kind -> the control.
///
/// # Safety
/// The host contract: `a` a live [`crate::tableobj::EeUiTableMake`].
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn table_make(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    // SAFETY: the contract
    let kind = unsafe { (*(a as *const crate::tableobj::EeUiTableMake)).kind } as usize;
    use crate::buttonobj::EeUiButtonCall as B;
    use crate::tableobj::EeUiTableKind as K;
    // SAFETY: the contract; the kind's constructor
    let control = unsafe { MAKE[kind](o, a, b, c, d, f) };
    // the folded button subclasses' constructor parts (UI round 16): UIScalableButton's three slice rectangles and
    // its text rectangle (the control's), UIDualButton's right button
    // SAFETY: the contract
    let m = unsafe { &*(a as *const crate::tableobj::EeUiTableMake) };
    let button = |call: B, i: isize, r: &crate::rect::UiRect| {
        let mut buf = [r.bottom, r.left, r.right, r.top];
        let o = crate::direct::find(crate::direct::Kind::Button, control as *const c_void);
        if !o.is_null() {
            // SAFETY: a live button; buf four floats
            unsafe { crate::buttonobj::ee_ui_button_call(o as *const _, call, i, 0, buf.as_mut_ptr()) };
        }
    };
    if m.kind == K::ScalableButton {
        for (i, r) in [m.normal_rect, m.focused_rect, m.pressed_rect].iter().enumerate() {
            button(B::ScalableInit, i as isize, r);
        }
        button(B::SetTextRectangle, 0, &m.rect);
    } else if m.kind == K::DualButton {
        button(B::DualInit, 0, &m.rect);
    }
    control
}

/// `TblSet`: control `a`'s setter `b` with `c`, `d` and `f`; `SetupText` sets up the text of class `c` (tableobj.rs
/// `TEXT_*`) through [`ee_ui_table_setup_text`].
///
/// # Safety
/// The host contract.
#[cfg(not(any(test, feature = "no_cpp")))]
unsafe fn table_set(o: *mut c_void, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize {
    use crate::tableobj::EeUiTableSet as Set;
    if b == Set::SetupText as isize {
        // SAFETY: the contract; the class's text
        let text = unsafe { TEXT[c as usize](o, a, b, c, d, f) };
        // SAFETY: the table and the control's text
        unsafe { ee_ui_table_setup_text(o, text as *mut c_void) };
    } else if let Some(Some(set)) = SET.get(b as usize) {
        // SAFETY: the contract; the selector's setter
        unsafe { set(o, a, b, c, d, f) };
    }
    0
}

/// `UIControlsTable::SetupGEText(text)`: the text transparent, in the description's text colour.
///
/// # Safety
/// `table` is a live C++ `UIControlsTable`, `text` a live `GEText`.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_table_setup_text(table: *mut c_void, text: *mut c_void) {
    let mut f = [0.0f32; 4];
    // SAFETY: the contract
    unsafe {
        ee_uicpp_tbl_text_transparent(table, text as isize, 0, 0, 0, f.as_mut_ptr());
        ee_uicpp_tbl_text_fg(table, text as isize, 0, 0, 0, f.as_mut_ptr());
    }
}

// ---- the ops whose C++ shim was one call on a Rust object of another crate (LLE/UI round 2): Rust's own calls

/// The toolkit's polls (`TMPoll`, whose only member is ee_base's [`EeTimePoll`]): the objects' timers (the buttons',
/// edit boxes', forms', labels' and lists' `PollCreate` .. `PollSetDelay`; `a` the poll, `b` a new delay) are Rust
/// records made and freed here, never seen by the C++.
#[cfg(not(any(test, feature = "no_cpp")))]
mod polls {
    use core::ffi::c_void;
    use ee_base::time::{self as t, EeTimePoll};

    fn p(a: isize) -> *mut EeTimePoll {
        a as *mut EeTimePoll
    }

    /// `new TMPoll(a, 0, false)`: a stopped millisecond poll of delay `a`.
    pub(super) unsafe fn create(_o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
        let poll = Box::into_raw(Box::new(EeTimePoll::default()));
        // SAFETY: a fresh record
        unsafe { t::ee_time_poll_init(poll, a as u32, 0, false) };
        poll as isize
    }
    /// # Safety
    /// `a` a poll [`create`] made (or NULL).
    pub(super) unsafe fn delete(_o: *mut c_void, a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
        if a != 0 {
            // SAFETY: the contract
            drop(unsafe { Box::from_raw(p(a)) });
        }
        0
    }
    macro_rules! poll_ops {
        ($( $name:ident => |$q:ident, $b:ident| $e:expr; )*) => {$(
            /// # Safety
            /// `a` a live poll [`create`] made.
            pub(super) unsafe fn $name(_o: *mut c_void, a: isize, $b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
                let $q = p(a);
                // SAFETY: the contract
                unsafe { $e }
            }
        )*};
    }
    poll_ops! {
        running => |q, _b| t::ee_time_poll_is_running(q) as isize;
        poll => |q, _b| t::ee_time_poll_poll(q) as isize;
        start => |q, _b| { t::ee_time_poll_start(q); 0 };
        stop => |q, _b| { t::ee_time_poll_stop(q); 0 };
        pause => |q, _b| { t::ee_time_poll_pause(q); 0 };
        resume => |q, _b| { t::ee_time_poll_resume(q); 0 };
        time_left => |q, _b| t::ee_time_poll_time_left(q) as isize;
        set_delay => |q, b| { t::ee_time_poll_set_time_delay(q, b as u32); 0 };
    }

    /// The hot keys' pressed poll (the original's `UIHotKey::sPressedPoll`: 50 ms, running from the first update),
    /// polled once per manager update (`MgrHotKeyPoll`).
    pub(super) unsafe fn hot_key(_o: *mut c_void, _a: isize, _b: isize, _c: isize, _d: isize, _f: *mut f32) -> isize {
        static POLL: std::sync::Mutex<Option<EeTimePoll>> = std::sync::Mutex::new(None);
        let mut g = POLL.lock().unwrap_or_else(|e| e.into_inner());
        let q = g.get_or_insert_with(|| {
            let mut q = EeTimePoll::default();
            // SAFETY: a live record
            unsafe { t::ee_time_poll_init(&mut q, 50, 0, true) };
            q
        });
        // SAFETY: a live record
        unsafe { t::ee_time_poll_poll(q) };
        0
    }
}
