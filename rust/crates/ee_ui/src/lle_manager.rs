//! `UIManager.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 10): the manager's host's cases of
//! more than one call ([`manager_call`], plugged in as `ee_ui_manager_host` before `UIManager::HostOp`), the UI
//! texture's load, the events' host ([`event_call`], `ee_ui_event_host`), the database's load and removal, the
//! shared mouse buttons. The manager's state and methods were already Rust (`managerobj.rs`).

// the logic is the FFI hosts' (absent with no_cpp: a crate's tests that call the toolkit)
#![cfg_attr(feature = "no_cpp", allow(dead_code))]

use core::ffi::c_long;

use crate::lle_cpp::{
    lle_host, Cpp, EeUiShimF32Out as F32Out, EeUiShimPtrOut as PtrOut, EeUiShimText as Text,
    EeUiShimULongOut as ULongOut, HostArgs,
};
use crate::obj::EeUiOp as Op;

/// `kStageOpModulate`.
const STAGE_OP_MODULATE: u32 = 2;
/// `kMipFilterPoint`.
const MIP_FILTER_POINT: u32 = 1;
/// `kAddressingClamp`.
const ADDRESSING_CLAMP: u32 = 0;

/// The UI textures' stages (`UIManager::LoadTexture`, `UIControlsTable::CreateBitmap`): modulated alpha and colour,
/// linear min and mag filters, point MIP filter, bias -0.75, clamped.
pub(crate) fn ui_texture_stages<C: Cpp + ?Sized>(c: &mut C, texture: usize) {
    c.texture_stage_alpha(texture, STAGE_OP_MODULATE);
    c.texture_stage_color(texture, STAGE_OP_MODULATE);
    c.texture_stage_min(texture, 1);
    c.texture_stage_mag(texture, 1);
    c.texture_stage_mip(texture, MIP_FILTER_POINT);
    c.texture_stage_bias(texture, -0.75);
    c.texture_stage_address(texture, ADDRESSING_CLAMP);
}

/// `UIManager::LoadTexture(name)`: the texture loaded with the UI bit depth and alpha, referenced, its stages set.
pub(crate) fn load_ui_texture<C: Cpp + ?Sized>(c: &mut C, m: usize, name: &[u8]) -> usize {
    let manager = c.mgr_texture_manager(m);
    let depth = c.mgr_ui_depth(m);
    let texture = c.texture_load_ui(manager, Text::of(name), depth, true);
    c.texture_reference(texture);
    ui_texture_stages(c, texture);
    texture
}

/// Two 32-bit numbers in one answer (low, high).
fn pack(low: u64, high: u64) -> isize {
    (low | high << 32) as isize
}

/// The manager's host's moved cases (`UIManager::HostOp`); `None` for the C++ host's.
pub(crate) fn manager_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    let m = h.obj;
    Some(match h.op {
        Op::MgrTranslatedKey => {
            c.key_copy(h.pb());
            c.form_translated_key(h.pa(), h.c as i32) as isize
        }
        Op::FormEventCall => {
            if h.b == 1 {
                c.event_dereference(h.pa());
            } else {
                c.event_reference(h.pa());
            }
            0
        }
        Op::MgrCallback => {
            if h.b == 0 {
                c.callback_manager(h.pa(), m, h.c as u32) as isize
            } else {
                c.callback_form(h.pa(), h.d as usize, h.c as u32) as isize
            }
        }
        Op::MgrWindow => {
            let window = c.mgr_window(m);
            if window == 0 {
                return Some(0);
            }
            h.set_f(0, c.window_origin_x(window));
            h.set_f(1, c.window_origin_y(window));
            1
        }
        Op::MgrWindowSize => {
            let window = c.mgr_window(m);
            let width = c.window_width(window);
            pack(u64::from(width), u64::from(c.window_height(window)))
        }
        Op::MgrBlending => {
            let (mut source, mut destination) = (0, 0);
            c.mgr_blending(m, ULongOut::of(&mut source), ULongOut::of(&mut destination));
            pack(u64::from(source), u64::from(destination))
        }
        Op::MgrTextureLoad => {
            c.mgr_rasterizer_screen(m);
            load_ui_texture(c, m, b"textures\\UIControls") as isize
        }
        Op::MgrTextureSize => {
            h.set_f(0, c.texture_width(h.pa()) as f32);
            h.set_f(1, c.texture_height(h.pa()) as f32);
            0
        }
        Op::MgrBitmapNew => {
            let bitmap = c.bitmap_new();
            let screen = c.mgr_screen(m);
            c.bitmap_set_texture(bitmap, h.pa(), screen);
            bitmap as isize
        }
        Op::MgrBitmapCoords => {
            // CreateBitmap (0x1007a438): top-left, top-right, bottom-left, bottom-right
            let [u0, v0, u1, v1] = h.f4();
            c.bitmap_set_opacity(h.pa(), 1.0);
            c.bitmap_set_uvs(h.pa(), u0, v0, u1, v0, u0, v1, u1, v1);
            0
        }
        Op::MgrScaledBitmapNew => {
            let mut uv = [0.0f32; 8];
            c.bitmap_uvs(h.pa(), F32Out(uv.as_mut_ptr()));
            let bitmap = c.bitmap_new();
            let screen = c.mgr_screen(m);
            let texture = c.bitmap_texture(h.pa(), screen);
            c.bitmap_set_texture(bitmap, texture, screen);
            c.bitmap_set_uvs(bitmap, uv[0], uv[1], uv[2], uv[3], uv[4], uv[5], uv[6], uv[7]);
            bitmap as isize
        }
        Op::MgrLockNew => new_lock(c, h.a as u32) as isize,
        Op::MgrInstanceRef => {
            if h.b == 0 {
                c.instance_reference(h.pa());
            } else {
                c.instance_dereference(h.pa());
            }
            0
        }
        _ => return None,
    })
}

/// `TSScheduler::CreateCriticalSection`, verified at `line`: the lock (the forms' and the manager's).
pub(crate) fn new_lock<C: Cpp + ?Sized>(c: &mut C, line: u32) -> usize {
    let mut lock = 0usize;
    if !c.lock_create(PtrOut::of(&mut lock)) {
        c.es_verify(line, Text::of(b"TSScheduler::CreateCriticalSection(theLock) == keseOK"));
    }
    lock
}

lle_host!(
    /// The UI manager's host: [`manager_call`], then `UIManager::HostOp`.
    ee_ui_manager_host => manager_call, [crate::lle_hosts::manager]
);

/// The events' host's moved cases (`UIEvent::HostOp`: the events file's count, the owner, the event added).
pub(crate) fn event_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    Some(match h.op {
        Op::BgtFileCount => {
            let mut count = 0;
            let result = c.file_read_count(h.obj, ULongOut::of(&mut count));
            h.set_f_bits(0, u64::from(count) as u32);
            result as isize
        }
        Op::EvOwner => {
            if h.a == 0 {
                c.table_get(h.b as u32) as isize
            } else {
                c.mgr_form_by_id(h.c as usize, h.b as u32) as isize
            }
        }
        Op::EvAdd => {
            if h.a == 0 {
                c.table_add_event(h.pb(), h.c as u32, h.d as u32);
            } else {
                c.form_add_event(h.pb(), h.c as u32, h.d as u32);
            }
            0
        }
        _ => return None,
    })
}

lle_host!(
    /// The events' host: [`event_call`], then `UIEvent::HostOp`.
    ee_ui_event_host => event_call, [crate::lle_hosts::event]
);

/// The database's tables in load order: (their file, the find's two assert lines, the create's).
const DB_TABLES: [(u32, u32, u32); 7] = [
    (1081, 1082, 1084),
    (1089, 1090, 1092),
    (1097, 1098, 1100),
    (1106, 1107, 0),
    (1113, 1114, 0),
    (1120, 1121, 0),
    (1127, 1128, 0),
];

/// The UI database's table files, in load order (once lle/UIManager.h's `gDbUI*FileName` globals).
const DB_NAMES: [&[u8]; 7] = [
    b"DB\\dbuifonts.dat",
    b"DB\\dbuiforms.dat",
    b"DB\\dbuiback.dat",
    b"DB\\dbuicontrols.dat",
    b"DB\\dbuicontrolevents.dat",
    b"DB\\dbuiformevents.dat",
    b"DB\\dbuihotkey.dat",
];

/// `UIManager::LoadDatabase(path, directory)`: the fonts, forms, backgrounds (and their creation), controls, control
/// and form events and hot keys, each from its table file found under the directory; every find and the first
/// three creates verified.
pub(crate) fn load_database<C: Cpp + ?Sized>(c: &mut C, m: usize, path: usize, directory: usize) {
    for (table, &(find_line, file_line, create_line)) in DB_TABLES.iter().enumerate() {
        let mut file = 0usize;
        let found = c.db_find(path, directory, Text::of(DB_NAMES[table]), PtrOut::of(&mut file));
        if found != 0 {
            c.es_verify(find_line, Text::of(b"theResult == keseOK"));
        }
        if file == 0 {
            c.es_verify(file_line, Text::of(b"theDbFile != NULL"));
        }
        let created: c_long = match table {
            0 => c.db_fonts(file, m),
            1 => c.db_forms(file, m),
            2 => c.db_backgrounds(file),
            3 => {
                c.db_controls(file, m);
                0
            }
            4 => {
                c.db_control_events(file);
                0
            }
            5 => {
                c.db_form_events(file, m);
                0
            }
            _ => {
                c.db_hot_keys(file);
                0
            }
        };
        if create_line != 0 && created != 0 {
            c.es_verify(create_line, Text::of(b"theResult == keseOK"));
        }
        if table == 2 {
            c.db_create_backgrounds(m);
        }
        c.file_delete(file);
    }
}

/// `UIManager::RemoveDatabase()`: the controls', backgrounds' and fonts' tables deleted.
pub(crate) fn remove_database<C: Cpp + ?Sized>(c: &mut C) {
    c.db_remove_controls();
    c.db_remove_backgrounds();
    c.db_remove_fonts();
}

/// The constructor's shared mouse buttons (left, right, the wheel), looked up on first use.
pub(crate) fn fetch_mouse_buttons<C: Cpp + ?Sized>(c: &mut C) {
    if c.mouse_buttons_ready() {
        return;
    }
    use ee_base::lle_enums::ISMouseEntity as M;
    for (which, button) in [M::kMouseLeftButton, M::kMouseRightButton, M::kMouseZAxis].into_iter().enumerate() {
        c.mouse_button_fetch(which as u32, button as u32);
    }
}

/// `UIManager::SetMouseLeftHanded(left)`: the buttons swapped when the setting changes.
pub(crate) fn set_mouse_left_handed<C: Cpp + ?Sized>(c: &mut C, left: bool) {
    if left == c.mouse_left_handed() {
        return;
    }
    c.mouse_swap_buttons();
    c.mouse_set_left_handed(left);
}

/// `UIManager::LoadDatabase` (the C++ member's body).
///
/// # Safety
/// `manager`, `path` and `directory` (a `const UString*`) are the C++ caller's live objects.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_load_database(
    manager: *mut core::ffi::c_void,
    path: *mut core::ffi::c_void,
    directory: *const core::ffi::c_void,
) {
    load_database(&mut crate::lle_cpp::Ffi, manager as usize, path as usize, directory as usize);
}

/// `UIManager::RemoveDatabase` (the C++ member's body).
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub extern "C" fn ee_ui_manager_remove_database() {
    remove_database(&mut crate::lle_cpp::Ffi);
}

/// The constructor's body after the Rust manager is made: `Init` (its ops read the manager through the C++
/// forwarders, so `mRust` must be set), then the shared mouse buttons looked up.
///
/// # Safety
/// `manager` is the live manager `ee_ui_manager_new` made.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_start(manager: *const crate::managerobj::EeUiManager) {
    use crate::managerobj::{ee_ui_manager_call, EeUiManagerCall};
    // SAFETY: the contract
    unsafe { ee_ui_manager_call(manager, EeUiManagerCall::Init, 0, 0, 0, core::ptr::null_mut()) };
    fetch_mouse_buttons(&mut crate::lle_cpp::Ffi);
}

/// The destructor: the Rust manager released and freed, then the database's tables deleted.
///
/// # Safety
/// `manager` came from `ee_ui_manager_new` (or is NULL) and is not used again.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_manager_destroy(manager: *mut crate::managerobj::EeUiManager) {
    // SAFETY: the contract
    unsafe { crate::managerobj::ee_ui_manager_free(manager) };
    remove_database(&mut crate::lle_cpp::Ffi);
}

/// `UIManager::sMouseLeftHanded` (the buttons' swap state).
static MOUSE_LEFT_HANDED: core::sync::atomic::AtomicBool = core::sync::atomic::AtomicBool::new(false);

/// `UIManager::sMouseLeftHanded`.
#[no_mangle]
pub extern "C" fn ee_ui_mouse_left_handed() -> bool {
    MOUSE_LEFT_HANDED.load(core::sync::atomic::Ordering::Relaxed)
}

/// `UIManager::sMouseLeftHanded = left`.
#[no_mangle]
pub extern "C" fn ee_ui_mouse_set_left_handed(left: bool) {
    MOUSE_LEFT_HANDED.store(left, core::sync::atomic::Ordering::Relaxed)
}

/// `UIManager::SetMouseLeftHanded` (the C++ member's body).
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub extern "C" fn ee_ui_manager_set_mouse_left_handed(left: bool) {
    set_mouse_left_handed(&mut crate::lle_cpp::Ffi, left);
}

#[cfg(test)]
#[path = "lle_manager_tests.rs"]
mod tests;
