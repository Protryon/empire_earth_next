//! The screens' question answers (the `EEUIFErrorCallback`s `EEUIManager::DisplayQuestion` calls with the
//! question's object; once EEScreens.cpp's static functions and their tables): each hands its event to
//! `ee_screens_event`, the save's after sending or posting the pending save, the boot question's after storing its
//! slot copy. `ee_screens_question_answer` gives the C++ question shims the answer of a table's index.
#![allow(clippy::missing_safety_doc)]

use core::ffi::c_void;

use crate::screens::campaign_editor::*;
use crate::screens::lobby_form::{EE_SCREENS_LE_BOOT_NO, EE_SCREENS_LE_BOOT_YES};
use crate::screens::main_game_options::*;
use crate::screens::menus_ffi::ee_screens_event;
use crate::screens::save_load::EE_SCREENS_SL_SAVED;

/// cbindgen:ignore
mod sys {
    use core::ffi::c_void;

    extern "C" {
        pub fn ee_screenscpp_save_send_wait(message: *mut c_void);
        pub fn ee_screenscpp_save_post(action: *mut c_void);
        pub fn ee_screenscpp_save_release(action: *mut c_void);
        pub fn ee_screenscpp_lobby_boot_set(copy: *mut c_void);
    }
}

type Answer = unsafe extern "C" fn(*mut c_void);

/// The tables (`EE_SCREENS_QA_*`): the save question's yes (send, post), the in-game options' yeses, the campaign
/// editor's yeses and noes (by its question index), the boot question's yes and no.
pub const EE_SCREENS_QA_SAVE_YES: u32 = 0;
pub const EE_SCREENS_QA_GO_YES: u32 = 1;
pub const EE_SCREENS_QA_CE_YES: u32 = 2;
pub const EE_SCREENS_QA_CE_NO: u32 = 3;
pub const EE_SCREENS_QA_BOOT: u32 = 4;

macro_rules! answers {
    ($($name:ident => $event:expr, $form:ident;)*) => {
        $(
            unsafe extern "C" fn $name(form: *mut c_void) {
                // SAFETY: the question's object, handed on as the event's form
                unsafe { ee_screens_event($event, $form(form), core::ptr::null_mut()) };
            }
        )*
    };
}

fn with(form: *mut c_void) -> *mut c_void {
    form
}

fn none(_: *mut c_void) -> *mut c_void {
    core::ptr::null_mut()
}

answers! {
    go_resign => EE_SCREENS_GO_RESIGN_YES, none;
    go_restart => EE_SCREENS_GO_RESTART_YES, none;
    go_to_editor => EE_SCREENS_GO_RETURN_TO_EDITOR_YES, none;
    go_exit => EE_SCREENS_GO_EXIT_YES, none;
    ce_save_before_open => EE_SCREENS_CE_SAVE_BEFORE_OPEN, with;
    ce_discard_before_open => EE_SCREENS_CE_DISCARD_BEFORE_OPEN, with;
    ce_save_before_new => EE_SCREENS_CE_SAVE_BEFORE_NEW, with;
    ce_discard_before_new => EE_SCREENS_CE_DISCARD_BEFORE_NEW, with;
    ce_save_before_main => EE_SCREENS_CE_SAVE_BEFORE_MAIN, with;
    ce_show_main => EE_SCREENS_CE_SHOW_MAIN, with;
    ce_save_before_editor => EE_SCREENS_CE_SAVE_BEFORE_EDITOR, with;
    ce_show_editor => EE_SCREENS_CE_SHOW_EDITOR, with;
    ce_delete_yes => EE_SCREENS_CE_DELETE_YES, with;
}

/// A saved game's save message to every player, waiting up to 15 s; the save handed on.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_save_send(message: *mut c_void) {
    // SAFETY: the pending save message
    unsafe {
        sys::ee_screenscpp_save_send_wait(message);
        ee_screens_event(EE_SCREENS_SL_SAVED, core::ptr::null_mut(), core::ptr::null_mut());
    }
}

/// A scenario's save action posted, our reference dropped; the save handed on.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_save_post(action: *mut c_void) {
    // SAFETY: the pending save action
    unsafe {
        sys::ee_screenscpp_save_post(action);
        sys::ee_screenscpp_save_release(action);
        ee_screens_event(EE_SCREENS_SL_SAVED, core::ptr::null_mut(), core::ptr::null_mut());
    }
}

unsafe extern "C" fn boot_yes(copy: *mut c_void) {
    // SAFETY: the slot copy the question was asked on
    unsafe {
        sys::ee_screenscpp_lobby_boot_set(copy);
        ee_screens_event(EE_SCREENS_LE_BOOT_YES, core::ptr::null_mut(), core::ptr::null_mut());
    }
}

unsafe extern "C" fn boot_no(copy: *mut c_void) {
    // SAFETY: the slot copy the question was asked on
    unsafe {
        sys::ee_screenscpp_lobby_boot_set(copy);
        ee_screens_event(EE_SCREENS_LE_BOOT_NO, core::ptr::null_mut(), core::ptr::null_mut());
    }
}

/// The answer `index` of the table `table` (`EE_SCREENS_QA_*`; the boot table's 0 yes, 1 no), NULL for none (the
/// campaign editor's delete has no "no").
#[no_mangle]
pub extern "C" fn ee_screens_question_answer(table: u32, index: u32) -> *const c_void {
    let t: &[Option<Answer>] = match table {
        EE_SCREENS_QA_SAVE_YES => &[Some(ee_screens_save_send), Some(ee_screens_save_post)],
        EE_SCREENS_QA_GO_YES => &[Some(go_resign), Some(go_restart), Some(go_to_editor), Some(go_exit)],
        EE_SCREENS_QA_CE_YES => &[
            Some(ce_save_before_open),
            Some(ce_save_before_new),
            Some(ce_save_before_main),
            Some(ce_save_before_editor),
            Some(ce_delete_yes),
        ],
        EE_SCREENS_QA_CE_NO => {
            &[Some(ce_discard_before_open), Some(ce_discard_before_new), Some(ce_show_main), Some(ce_show_editor), None]
        }
        EE_SCREENS_QA_BOOT => &[Some(boot_yes), Some(boot_no)],
        _ => &[],
    };
    t.get(index as usize).copied().flatten().map_or(core::ptr::null(), |f| f as *const c_void)
}
