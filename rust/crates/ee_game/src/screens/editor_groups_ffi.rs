//! `extern "C"` for the scenario editor's tab groups as Rust objects (`editor_groups.rs`): a group holds its tab's
//! FFI object (`editor_map_ffi.rs` .. `editor_instructions_ffi.rs`), made, called by slot and freed here. The C++
//! `EEUIFSEMainGroup` and its seven subclasses are gone: the editor form's slots hold the groups, and a C++ caller of
//! a tab's own member asks for the tab (`ee_screens_segr_tab`) and calls its FFI.

use core::cell::Cell;
use core::ffi::c_void;

use crate::screens::editor_elevation::*;
use crate::screens::editor_elevation_ffi::*;
use crate::screens::editor_form::*;
use crate::screens::editor_groups::*;
use crate::screens::editor_instructions::*;
use crate::screens::editor_instructions_ffi::*;
use crate::screens::editor_map::*;
use crate::screens::editor_map_ffi::*;
use crate::screens::editor_players_ffi::*;
use crate::screens::editor_terrain::*;
use crate::screens::editor_terrain_ffi::*;
use crate::screens::editor_tscreen_ffi::*;
use crate::screens::editor_unit_ffi::*;

/// A tab group (opaque to C++): its tab (`TAB_*`), the tab's FFI object, and the unit tab's C++ parts (its shell:
/// the build tools, the rhombus, the panel's rectangle), which the C++ frees.
pub struct EeScreensSeGroup {
    tab: u32,
    obj: Cell<usize>,
    parts: usize,
}

/// `CreateTabGroup`: the tab `tab`'s group, its controls made on `form` from the control index `*index` (advanced);
/// `scroll` and `brushes` the form's edge scroll and brush group (the elevation and terrain tabs'), `players` its
/// player combo box (the unit and player tabs'), `parts` the unit tab's C++ shell. The screen font is the C++'s
/// (set before). The map and terrain tabs' shell is the group itself (their events and controls call back through
/// it: `ee_screens_segr_tab`). Freed by `ee_screens_segr_free`.
///
/// # Safety
/// `index` points at the form's next control index; the pointers are the form's.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_segr_new(
    tab: u32,
    form: *mut c_void,
    scroll: *mut c_void,
    brushes: *mut c_void,
    players: *mut c_void,
    parts: *mut c_void,
    index: *mut u32,
) -> *mut EeScreensSeGroup {
    let g = Box::into_raw(Box::new(EeScreensSeGroup { tab, obj: Cell::new(0), parts: parts as usize }));
    let shell = g as *mut c_void;
    let obj = unsafe {
        match tab {
            TAB_MAP => ee_screens_semp_new(shell, form, index) as usize,
            TAB_ELEVATION => ee_screens_seel_new(form, scroll, brushes, index) as usize,
            TAB_TERRAIN => ee_screens_setn_new(shell, form, scroll, brushes, index) as usize,
            TAB_UNITS => ee_screens_seun_new(parts, form, players, index) as usize,
            TAB_TRIGGERS => ee_screens_sets_new(form, index) as usize,
            TAB_PLAYERS => ee_screens_sepl_new(form, players, index) as usize,
            TAB_INSTRUCTIONS => ee_screens_sein_new(form, index) as usize,
            _ => 0,
        }
    };
    unsafe { (*g).obj.set(obj) };
    g
}

/// The destructor: the tab freed (its controls deleted), the group freed -> the unit tab's C++ parts, for the C++ to
/// delete (NULL for the other tabs).
///
/// # Safety
/// `g` comes from `ee_screens_segr_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_segr_free(g: *mut EeScreensSeGroup) -> *mut c_void {
    if g.is_null() {
        return core::ptr::null_mut();
    }
    let g = unsafe { Box::from_raw(g) };
    let obj = g.obj.replace(0) as *mut c_void;
    unsafe {
        match g.tab {
            TAB_MAP => ee_screens_semp_free(obj.cast()),
            TAB_ELEVATION => ee_screens_seel_free(obj.cast()),
            TAB_TERRAIN => ee_screens_setn_free(obj.cast()),
            TAB_UNITS => ee_screens_seun_free(obj.cast()),
            TAB_TRIGGERS => ee_screens_sets_free(obj.cast()),
            TAB_PLAYERS => ee_screens_sepl_free(obj.cast()),
            TAB_INSTRUCTIONS => ee_screens_sein_free(obj.cast()),
            _ => {}
        }
    }
    g.parts as *mut c_void
}

/// The group's tab's FFI object (an `EeScreensSeMap*` .. `EeScreensSeInstructions*` by its tab; NULL for NULL, and
/// while the tab is being made).
///
/// # Safety
/// `g` comes from `ee_screens_segr_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_segr_tab(g: *const EeScreensSeGroup) -> *mut c_void {
    unsafe { g.as_ref() }.map_or(0, |g| g.obj.get()) as *mut c_void
}

/// The unit tab's C++ parts (its shell, `EESEUnit`; NULL for the other tabs).
///
/// # Safety
/// `g` comes from `ee_screens_segr_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_segr_parts(g: *const EeScreensSeGroup) -> *mut c_void {
    unsafe { g.as_ref() }.map_or(0, |g| g.parts) as *mut c_void
}

/// The group's slot `slot` (`EE_SCREENS_SEFM_GROUP_*`: `Draw`'s `p` the dirty `GERectangle`, `MouseMove` at `x`,
/// `y`): the tab's member when its class overrode the slot, else nothing (the base's empty slot).
///
/// # Safety
/// `g` comes from `ee_screens_segr_new` (or is NULL); `p` is what the slot takes.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_segr_call(g: *const EeScreensSeGroup, slot: i32, x: f32, y: f32, p: *mut c_void) {
    let Some(g) = (unsafe { g.as_ref() }) else { return };
    let obj = g.obj.get() as *mut c_void;
    if obj.is_null() || !overrides(g.tab, slot) {
        return;
    }
    let null = core::ptr::null_mut();
    unsafe {
        match g.tab {
            TAB_MAP => ee_screens_semp_call(obj.cast(), EE_SCREENS_SEMP_ENTER, 0),
            TAB_ELEVATION => {
                let what = if slot == EE_SCREENS_SEFM_GROUP_UPDATE {
                    EE_SCREENS_SEEL_UPDATE
                } else {
                    EE_SCREENS_SEEL_MOUSE_MOVE
                };
                ee_screens_seel_call(obj.cast(), what, x, y);
            }
            TAB_TERRAIN => {
                let what = match slot {
                    EE_SCREENS_SEFM_GROUP_UPDATE => EE_SCREENS_SETN_UPDATE,
                    EE_SCREENS_SEFM_GROUP_LEAVE => EE_SCREENS_SETN_LEAVE,
                    EE_SCREENS_SEFM_GROUP_ENTER => EE_SCREENS_SETN_ENTER,
                    _ => EE_SCREENS_SETN_MOUSE_MOVE,
                };
                ee_screens_setn_call(obj.cast(), what, null, x, y);
            }
            TAB_UNITS => {
                let what = match slot {
                    EE_SCREENS_SEFM_GROUP_UPDATE => EE_SCREENS_SEUN_UPDATE,
                    EE_SCREENS_SEFM_GROUP_LEAVE => EE_SCREENS_SEUN_LEAVE,
                    EE_SCREENS_SEFM_GROUP_DRAW => EE_SCREENS_SEUN_DRAW,
                    EE_SCREENS_SEFM_GROUP_ENTER => EE_SCREENS_SEUN_ENTER,
                    EE_SCREENS_SEFM_GROUP_MOUSE_MOVE => EE_SCREENS_SEUN_MOUSE_MOVE,
                    _ => EE_SCREENS_SEUN_UPDATE_KEYS,
                };
                ee_screens_seun_call(obj.cast(), what, 0, x, y, p);
            }
            TAB_TRIGGERS => {
                let what = match slot {
                    EE_SCREENS_SEFM_GROUP_UPDATE => EE_SCREENS_SETS_UPDATE,
                    EE_SCREENS_SEFM_GROUP_LEAVE => EE_SCREENS_SETS_LEAVE,
                    EE_SCREENS_SEFM_GROUP_DRAW => EE_SCREENS_SETS_DRAW,
                    EE_SCREENS_SEFM_GROUP_DISABLE => EE_SCREENS_SETS_DISABLE,
                    EE_SCREENS_SEFM_GROUP_LOAD => EE_SCREENS_SETS_SHOW_SELECTION,
                    EE_SCREENS_SEFM_GROUP_ENTER => EE_SCREENS_SETS_ENTER,
                    EE_SCREENS_SEFM_GROUP_STORE => EE_SCREENS_SETS_STORE_ENTRY,
                    EE_SCREENS_SEFM_GROUP_ENABLE => EE_SCREENS_SETS_ENABLE,
                    EE_SCREENS_SEFM_GROUP_MOUSE_MOVE => EE_SCREENS_SETS_MOUSE_MOVE,
                    _ => EE_SCREENS_SETS_UPDATE_KEYS,
                };
                ee_screens_sets_call(obj.cast(), what, 0, x, y, p);
            }
            TAB_PLAYERS => {
                let what = match slot {
                    EE_SCREENS_SEFM_GROUP_LEAVE => EE_SCREENS_SEPL_LEAVE,
                    EE_SCREENS_SEFM_GROUP_LOAD => EE_SCREENS_SEPL_LOAD,
                    EE_SCREENS_SEFM_GROUP_ENTER => EE_SCREENS_SEPL_ENTER,
                    _ => EE_SCREENS_SEPL_STORE,
                };
                ee_screens_sepl_call(obj.cast(), what, 0, 0);
            }
            TAB_INSTRUCTIONS => {
                let what = match slot {
                    EE_SCREENS_SEFM_GROUP_LOAD => EE_SCREENS_SEIN_LOAD,
                    EE_SCREENS_SEFM_GROUP_ENTER => EE_SCREENS_SEIN_ENTER,
                    // (Leave is Store)
                    _ => EE_SCREENS_SEIN_STORE,
                };
                ee_screens_sein_call(obj.cast(), what);
            }
            _ => {}
        }
    }
}
