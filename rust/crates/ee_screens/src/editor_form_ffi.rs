//! `extern "C"` for the scenario editor's main form whole in Rust (`editor_form.rs`): the object
//! `EEUIFScenarioEditor` holds, made, built, called and freed by it, its slots read and set by its accessors, its
//! selection read by `GetSelectedUnit`, and the trigger-only save run by its Ctrl+S event. The shared control ops
//! (below 100) are `ee_screenscpp_sepl` (`Scenario Editor/EESERBUnitTypes.cpp`), the form's own `ee_screenscpp_sefm`
//! (`Scenario Editor/EEUIFScenarioEditor.cpp`).

use core::ffi::c_void;

use crate::editor_form::*;
use crate::editor_players::SeplHost;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_sepl(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
        pub fn ee_screenscpp_sefm(op: u32, p: *const *mut c_void, i: *mut i32, f: *mut f32) -> i32;
    }
}

struct Host;

impl SeplHost for Host {
    fn op(&mut self, op: u32, p: [usize; 3], i: &mut [i32; 4], f: &mut [f32; 4]) -> i32 {
        let p = p.map(|x| x as *mut c_void);
        unsafe {
            if op >= 800 {
                sys::ee_screenscpp_sefm(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            } else {
                sys::ee_screenscpp_sepl(op, p.as_ptr(), i.as_mut_ptr(), f.as_mut_ptr())
            }
        }
    }
}

/// The editor form (opaque to C++).
pub struct EeScreensSeForm(EditorForm);

/// The form's object over the C++ `EEUIFScenarioEditor` `shell`, empty until `EE_SCREENS_SEFM_BUILD` (the shell
/// stores it first: the build's controls call back into it). Freed by `ee_screens_sefm_free`.
#[no_mangle]
pub extern "C" fn ee_screens_sefm_new(shell: *mut c_void) -> *mut EeScreensSeForm {
    Box::into_raw(Box::new(EeScreensSeForm(EditorForm::new(shell as usize))))
}

/// The destructor: the events released and the controls deleted (the original's order), the object freed.
///
/// # Safety
/// `form` comes from `ee_screens_sefm_new` (or is NULL) and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefm_free(form: *mut EeScreensSeForm) {
    if form.is_null() {
        return;
    }
    unsafe { (*form).0.destroy(&mut Host) };
    drop(unsafe { Box::from_raw(form) });
}

/// The form's slot `slot` (`EE_SCREENS_SEFM_*`: a control, an event, a flag, the current tab ...); 0 for none.
///
/// # Safety
/// `form` comes from `ee_screens_sefm_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefm_get(form: *const EeScreensSeForm, slot: u32) -> usize {
    if form.is_null() {
        return 0;
    }
    unsafe { (*form).0.get(slot) }
}

/// The form's slot `slot` set to `value`.
///
/// # Safety
/// `form` comes from `ee_screens_sefm_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefm_set(form: *const EeScreensSeForm, slot: u32, value: usize) {
    if !form.is_null() {
        unsafe { (*form).0.set(slot, value) };
    }
}

/// The selection's size (`index` = -1) or its unit `index`'s id (`EEUnitHandle`'s 0xffffffff past the end).
///
/// # Safety
/// `form` comes from `ee_screens_sefm_new` (or is NULL).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefm_selected(form: *const EeScreensSeForm, index: isize) -> u32 {
    if form.is_null() {
        return if index < 0 { 0 } else { u32::MAX };
    }
    let f = unsafe { &(*form).0 };
    if index < 0 {
        f.selected_count() as u32
    } else {
        f.selected_id(index as usize)
    }
}

/// One of the form's members (`EE_SCREENS_SEFM_BUILD` ...): `a` its number (a flag, the tab's control group | its
/// tab << 8), `p` its object (a unit, a load event, the load text's `UWideString`), `r` its rectangle (four floats or
/// NULL). Returns the member's result (`DrawBackground`'s, the trigger-only save's) or whether the form's own
/// `UpdateKeyboard` / `UpdateMouse` runs.
///
/// # Safety
/// `form` comes from `ee_screens_sefm_new` (or is NULL); `r` is NULL or points at four floats.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_sefm_call(
    form: *const EeScreensSeForm,
    what: u32,
    a: i32,
    p: *mut c_void,
    r: *const f32,
) -> i32 {
    if form.is_null() {
        return 0;
    }
    let f = unsafe { &(*form).0 };
    let rect: [f32; 4] = if r.is_null() { [0.0; 4] } else { unsafe { core::ptr::read(r as *const [f32; 4]) } };
    let h = &mut Host;
    let p = p as usize;
    match what {
        EE_SCREENS_SEFM_BUILD => f.build(h),
        EE_SCREENS_SEFM_SELECT_TAB => f.select_tab(h, (a & 0xff) as u32, (a >> 8) as usize),
        EE_SCREENS_SEFM_LEAVE_TAB => f.leave_tab(h),
        EE_SCREENS_SEFM_ENTER_FULL_VIEW => f.enter_full_view(h),
        EE_SCREENS_SEFM_LEAVE_FULL_VIEW => f.leave_full_view(h),
        EE_SCREENS_SEFM_EXIT_EDITOR => f.exit_editor(h, a != 0),
        EE_SCREENS_SEFM_STORE_GROUP => f.store_group(h),
        EE_SCREENS_SEFM_RESUME_GROUP => f.resume_group(h),
        EE_SCREENS_SEFM_POST_LOAD_EVENT => f.post_suspended(h, EE_SCREENS_SEFM_POST_40B, p),
        EE_SCREENS_SEFM_POST_SHOW_LOADED => f.post_suspended(h, EE_SCREENS_SEFM_POST_40C, p),
        EE_SCREENS_SEFM_OPEN => f.open(h),
        EE_SCREENS_SEFM_COPY_DATA => f.copy_data(h),
        EE_SCREENS_SEFM_DRAW_BACKGROUND => return f.draw_background(h, rect),
        EE_SCREENS_SEFM_UPDATE_KEYBOARD => return i32::from(f.update_keyboard(h)),
        EE_SCREENS_SEFM_UPDATE_MOUSE => return i32::from(f.update_mouse(h)),
        EE_SCREENS_SEFM_SELECT_UNIT => f.select_unit(h, p),
        EE_SCREENS_SEFM_DESELECT_UNIT => f.deselect_unit(h, p),
        EE_SCREENS_SEFM_DESELECT_FIRST => f.deselect_first(h),
        EE_SCREENS_SEFM_CLEAR_SELECTION => f.clear_selection(h),
        EE_SCREENS_SEFM_SET_GAME_VIEW => f.set_game_view(h, rect, a != 0),
        EE_SCREENS_SEFM_SET_LOAD_TEXT => f.set_load_text(h, p, a == 0),
        EE_SCREENS_SEFM_SAVE_TRIGGERS => return save_triggers(),
        _ => {}
    }
    0
}

/// The trigger-only save (Ctrl+S): the scenario saved to ScenarioTriggers.scn, the report written; the open's
/// result.
pub(crate) fn save_triggers() -> i32 {
    EditorForm::save_triggers(&mut Host)
}
