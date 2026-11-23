//! `UIForm.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 10): the form's host's cases of more
//! than one call ([`form_call`], plugged in as `ee_ui_form_host` before `UIForm::HostOp`), the database's forms
//! (`UIForm::Create`) and the IME and typed keys passed to the control holding the keyboard. The form's state and
//! methods were already Rust (`formobj.rs`).

// the logic is the FFI hosts' (absent with no_cpp: a crate's tests that call the toolkit)
#![cfg_attr(feature = "no_cpp", allow(dead_code))]

use core::ffi::{c_long, c_ulong};

use crate::lle_cpp::{
    lle_host, Cpp, EeUiShimBytes as Bytes, EeUiShimU8Out as U8Out, EeUiShimULongOut as ULongOut, HostArgs,
};
use crate::lle_manager::new_lock;
use crate::obj::EeUiOp as Op;

/// `kErrorOK`.
const ERROR_OK: c_long = 0;
/// `kErrorFailed`.
const ERROR_FAILED: c_long = 9;
/// `sizeof(UIDbDataForms)`: the forms file's record.
pub const EE_UI_FORM_RECORD_SIZE: usize = 268;
/// `UIDbDataForms::mFormID`'s offset.
const FORM_ID_OFFSET: usize = 0xcc;

/// The form's host's moved cases (`UIForm::HostOp`); `None` for the C++ host's.
pub(crate) fn form_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    let form = h.obj;
    Some(match h.op {
        Op::FormHelpGet => {
            c.form_help_clear();
            c.control_help_string(h.pa()) as isize
        }
        Op::FormHotKeySetEvent => {
            let (key, event) = (h.pa(), h.c as usize);
            match h.b {
                0 => c.hot_key_set_just_pressed(key, event),
                1 => c.hot_key_set_pressed(key, event),
                2 => c.hot_key_set_just_unpressed(key, event),
                _ => c.hot_key_set_double_pressed(key, event),
            }
            0
        }
        Op::FormKeysFetch => {
            c.form_fetch_tab_key();
            c.form_fetch_shift_key();
            0
        }
        Op::FormEventCall => {
            match h.b {
                0 => c.event_uninitialize(h.pa()),
                1 => c.event_dereference(h.pa()),
                2 => c.event_reference(h.pa()),
                _ => c.event_initialize(h.pa()),
            }
            0
        }
        Op::FormMouseAsk => {
            let entity = h.pa();
            (match h.b {
                0 => c.entity_value(entity) != 0,
                1 => c.entity_just_pressed(entity),
                2 => c.entity_just_unpressed(entity),
                _ => c.entity_just_double_pressed(entity),
            }) as isize
        }
        Op::FormLockNew => new_lock(c, 1743) as isize,
        Op::FormBitmapTexSize => {
            let screen = c.form_screen(form);
            let texture = c.bitmap_texture(h.pa(), screen);
            h.set_f(0, c.texture_width(texture) as f32);
            let screen = c.form_screen(form);
            let texture = c.bitmap_texture(h.pa(), screen);
            h.set_f(1, c.texture_height(texture) as f32);
            0
        }
        Op::FormBitmapSetTexCoords => {
            let [u0, v0, u1, v1] = h.f4();
            c.bitmap_set_uvs(h.pa(), u0, v0, u1, v0, u0, v1, u1, v1);
            0
        }
        _ => return None,
    })
}

lle_host!(
    /// The form's host: [`form_call`], then `UIForm::HostOp`.
    ee_ui_form_host => form_call, [crate::lle_hosts::form]
);

/// `UIForm::Create(file, manager)`: every forms record read from the file; each with a form id above 0 made by the
/// manager (`CreateForm`), added and its group 0 enabled. The last read's result (the count's when it failed).
pub(crate) fn create_forms<C: Cpp + ?Sized>(c: &mut C, file: usize, m: usize) -> c_long {
    let mut count: c_ulong = 0;
    c.file_open(file);
    let mut result = c.file_read_count(file, ULongOut::of(&mut count));
    if result == ERROR_OK {
        // the record stays the last one read when a read fails (the original's)
        let mut record = [0u8; EE_UI_FORM_RECORD_SIZE];
        for _ in 0..count {
            result = c.file_read(file, U8Out(record.as_mut_ptr()), EE_UI_FORM_RECORD_SIZE as c_ulong);
            let id = i32::from_le_bytes([
                record[FORM_ID_OFFSET],
                record[FORM_ID_OFFSET + 1],
                record[FORM_ID_OFFSET + 2],
                record[FORM_ID_OFFSET + 3],
            ]);
            if id <= 0 {
                continue;
            }
            let form = c.mgr_create_form(m, Bytes::of(&record));
            c.mgr_add_form(m, form);
            c.form_enable_group0(form);
        }
    }
    c.file_close(file);
    result
}

/// The IME and typed-key passes to the control holding the keyboard (`UIForm::AddIMEResultString`,
/// `ChangeIMECompositionString`, `CloseIMECompositionString`, `OpenIMECompositionString`, `UpdateTranslatedKey`):
/// `which` 0 the result string `a`, 1 the composition (`a` the string, `b` the attributes, `cursor`), 2 the close,
/// 3 the open, 4 the key `a` repeated `cursor` times. With no control: OK, but the open's failure.
pub(crate) fn pass_to_keyboard<C: Cpp + ?Sized>(
    c: &mut C,
    form: usize,
    which: u32,
    a: usize,
    b: usize,
    cursor: u32,
) -> c_long {
    let control = c.form_keyboard_captured(form);
    if control == 0 {
        return if which == 3 { ERROR_FAILED } else { ERROR_OK };
    }
    match which {
        0 => {
            c.control_ime_result(control, a);
            ERROR_OK
        }
        1 => c.control_ime_change(control, a, b, cursor),
        2 => c.control_ime_close(control),
        3 => c.control_ime_open(control),
        _ => c.control_translated_key(control, a, cursor as i32),
    }
}

/// `UIForm::Create` (the C++ member's body).
///
/// # Safety
/// `file` (an `FSFileSpec*`) and `manager` are the C++ caller's live objects.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_create(file: *mut core::ffi::c_void, manager: *mut core::ffi::c_void) -> c_long {
    create_forms(&mut crate::lle_cpp::Ffi, file as usize, manager as usize)
}

/// The form's IME and typed-key members' bodies ([`pass_to_keyboard`]).
///
/// # Safety
/// `form` is a live `UIForm*`, `a` and `b` the live strings (or the key's characters) the call names.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_pass_to_keyboard(
    form: *mut core::ffi::c_void,
    which: u32,
    a: *const core::ffi::c_void,
    b: *const core::ffi::c_void,
    cursor: u32,
) -> c_long {
    pass_to_keyboard(&mut crate::lle_cpp::Ffi, form as usize, which, a as usize, b as usize, cursor)
}

/// The forms' packs (`UIForm::AddHotKey`, `RemoveHotKey`): a key and three modifiers, 16 bits each from the low
/// end (a code past 0xffff is no key, as `kKeyNone` 0x7fff is).
#[no_mangle]
pub extern "C" fn ee_ui_form_pack_keys(
    key: c_ulong,
    modifier1: c_ulong,
    modifier2: c_ulong,
    modifier3: c_ulong,
) -> isize {
    [key, modifier1, modifier2, modifier3]
        .iter()
        .enumerate()
        .fold(0u64, |p, (i, &k)| p | ((k as u64).min(0xffff)) << (16 * i)) as isize
}

/// `UIForm::SetLayout`'s pack: the four offset types' low 16 bits each (left, right, top, bottom).
#[no_mangle]
pub extern "C" fn ee_ui_form_pack_types(left: c_ulong, right: c_ulong, top: c_ulong, bottom: c_ulong) -> isize {
    [left, right, top, bottom].iter().enumerate().fold(0u64, |p, (i, &t)| p | ((t as u64) & 0xffff) << (16 * i))
        as isize
}

/// Two floats' bits in one number (`x` low, `y` high: `UIForm::DrawBackground`'s location).
#[no_mangle]
pub extern "C" fn ee_ui_float_pair_bits(x: f32, y: f32) -> isize {
    (x.to_bits() as u64 | (y.to_bits() as u64) << 32) as isize
}

#[cfg(test)]
#[path = "lle_form_tests.rs"]
mod tests;
