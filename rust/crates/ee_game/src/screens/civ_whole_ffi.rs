//! `extern "C"` for the civilization builder whole (`civ_whole.rs`, `civ_edit.rs`, `civ_list.rs`): its entry
//! points (the game's open paths, the timer, the blinking button, the campaign's save, the edited civilization's
//! end, the questions' answers), the list box's overrides (`EECBUIListBox`'s forwarders), and the one host,
//! `ee_screenscpp_cb` (`Civilization Builder/EECivilizationBuilder.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_void};

use crate::screens::civ_list;
use crate::screens::civ_whole::{self, CbHost, EeScreensCbOut};

/// cbindgen:ignore
mod sys {
    extern "C" {
        pub fn ee_screenscpp_cb(
            op: u32,
            p: usize,
            a: i64,
            b: i64,
            c: i64,
            text: *const u8,
            length: usize,
            out: *mut core::ffi::c_void,
        ) -> i64;
    }
}

/// The C++ host.
pub(crate) struct Host;

impl CbHost for Host {
    fn op(&mut self, op: u32, p: usize, a: i64, b: i64, c: i64, text: &[u8], out: &mut EeScreensCbOut) -> i64 {
        let out = (out as *mut EeScreensCbOut).cast::<c_void>();
        unsafe { sys::ee_screenscpp_cb(op, p, a, b, c, text.as_ptr(), text.len(), out) }
    }
}

unsafe fn bytes<'a>(text: *const u8, length: usize) -> &'a [u8] {
    if text.is_null() || length == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(text, length) }
    }
}

/// An op's text appended (the host's answer).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cb_out_text(out: *mut EeScreensCbOut, text: *const u8, length: usize) {
    if let Some(out) = unsafe { out.as_mut() } {
        out.text.extend_from_slice(unsafe { bytes(text, length) });
    }
}

/// An op's value appended (the host's answer).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cb_out_value(out: *mut EeScreensCbOut, value: i64) {
    if let Some(out) = unsafe { out.as_mut() } {
        out.values.push(value);
    }
}

// ---- the builder's entry points

/// `CivBuilderEditCivilization`.
#[no_mangle]
pub extern "C" fn ee_screens_cb_edit_civilization() -> c_long {
    civ_whole::edit_civilization(&mut Host)
}

/// `CivBuilderCreateCivilization(civ, reload)` (`civ` an `EECivilization*`).
#[no_mangle]
pub extern "C" fn ee_screens_cb_create_civilization(civ: *mut c_void, reload: bool) -> c_long {
    civ_whole::create_civilization(&mut Host, civ as usize, reload)
}

/// `EEProcessTimerExpired`.
#[no_mangle]
pub extern "C" fn ee_screens_cb_timer_expired() -> c_long {
    civ_whole::process_timer_expired(&mut Host)
}

/// `EESetCivBuilderButtonBlinking`.
#[no_mangle]
pub extern "C" fn ee_screens_cb_set_blinking(blinking: bool) -> c_long {
    civ_whole::set_blinking(&mut Host, blinking)
}

/// `CivBuilderSaveCivilization(name, overwrite)` (UTF-8 bytes).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cb_save_civilization(name: *const u8, length: usize, overwrite: bool) -> c_long {
    civ_whole::save_civilization(&mut Host, unsafe { bytes(name, length) }, overwrite)
}

/// `CivBuilderEditCurrentCivilization`: the current player's civilization edited.
#[no_mangle]
pub extern "C" fn ee_screens_cb_edit_current() {
    civ_whole::edit_current_civilization(&mut Host)
}

/// The edited civilization deleted (`delete gEditableCivilization`).
#[no_mangle]
pub extern "C" fn ee_screens_cb_free_civilization() {
    civ_whole::free_editable()
}

/// A question's Yes (`EE_SCREENS_CBW_ANSWER_*`) with its form.
#[no_mangle]
pub extern "C" fn ee_screens_cb_answer(which: i64, form: *mut c_void) {
    civ_whole::answer(&mut Host, which, form as usize)
}

/// TEST ONLY (the builder's gate, `EESimCivBuilder.cpp`): the edited civilization: 0 its kind (0 none, 1 an edit,
/// 2 a creator, 3 an unlimited creator), 1 its points left, 2 its maximum, 3 `mPointsLeft`, 4 its player, 5 whether
/// it has choices, 6 whether it can change, 7 its picked upgrades' count, 8 picked upgrade `index`, 9 its upgrade ids'
/// count, 10 id `index`.
#[no_mangle]
pub extern "C" fn ee_screens_cb_civilization_query(what: u32, index: u32) -> i64 {
    let Some(e) = civ_whole::editable() else { return 0 };
    let h = &mut Host;
    match what {
        0 => match e.creator {
            None => 1,
            Some(false) => 2,
            Some(true) => 3,
        },
        1 => e.points_left() as i64,
        2 => e.max_points(h),
        3 => e.points.get() as i64,
        4 => e.player as i64,
        5 => e.has_choices(h) as i64,
        6 => e.can_change(h) as i64,
        7 => e.upgrades.borrow().len() as i64,
        8 => e.upgrades.borrow().get(index as usize).copied().unwrap_or(0) as i64,
        9 => e.copy_upgrade_ids(h).len() as i64,
        10 => e.copy_upgrade_ids(h).get(index as usize).copied().unwrap_or(0) as i64,
        _ => 0,
    }
}

/// TEST ONLY: the edited civilization's name into `out` (`capacity` bytes) -> its length.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cb_civilization_name(out: *mut u8, capacity: usize) -> usize {
    let name = civ_whole::editable().map(|e| e.name()).unwrap_or_default();
    if !out.is_null() {
        let n = name.len().min(capacity);
        unsafe { core::ptr::copy_nonoverlapping(name.as_ptr(), out, n) };
    }
    name.len()
}

/// TEST ONLY: the last saved civilization file's name (`gLastCivilizationFile`) into `out` -> its length.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cb_last_file(out: *mut u8, capacity: usize) -> usize {
    let name = civ_whole::state().last_file;
    if !out.is_null() {
        let n = name.len().min(capacity);
        unsafe { core::ptr::copy_nonoverlapping(name.as_ptr(), out, n) };
    }
    name.len()
}

// ---- the list box's overrides (`list` the `EECBUIListBox*`)

/// `AddLine`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cbl_add_line(list: *mut c_void, text: *const u8, length: usize) {
    civ_list::add_line(&mut Host, list as usize, unsafe { bytes(text, length) })
}

/// `ChangeSelectionIndex`.
#[no_mangle]
pub extern "C" fn ee_screens_cbl_change_selection(list: *mut c_void, index: u32, fire: bool, scroll: bool) {
    civ_list::change_selection(&mut Host, list as usize, index, fire, scroll)
}

/// `DeleteLine`.
#[no_mangle]
pub extern "C" fn ee_screens_cbl_delete_line(list: *mut c_void, index: u32) {
    civ_list::delete_line(&mut Host, list as usize, index)
}

/// `DrawTextA` -> its result.
#[no_mangle]
pub extern "C" fn ee_screens_cbl_draw_text(list: *mut c_void) -> c_long {
    civ_list::draw_text(&mut Host, list as usize) as c_long
}

/// `TabbedTo`.
#[no_mangle]
pub extern "C" fn ee_screens_cbl_tabbed_to(list: *mut c_void) {
    civ_list::tabbed_to(&mut Host, list as usize)
}

/// `GetSelectedUpgrade` -> 0 and the upgrade, bonus and cost (each written as the original wrote them), or the
/// error.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cbl_selected_upgrade(
    list: *mut c_void,
    upgrade: *mut u32,
    bonus: *mut u32,
    cost: *mut u32,
) -> c_long {
    let init = unsafe { (*upgrade, *bonus, *cost) };
    match civ_list::selected_upgrade(&mut Host, list as usize, init) {
        Ok((u, b, c)) => {
            unsafe {
                *upgrade = u;
                *bonus = b;
                *cost = c;
            }
            0
        }
        Err(e) => e,
    }
}

unsafe fn upgrades<'a>(upgrades: *const u32, count: usize) -> &'a [u32] {
    if upgrades.is_null() || count == 0 {
        &[]
    } else {
        unsafe { core::slice::from_raw_parts(upgrades, count) }
    }
}

/// `SetUpgradesExcept`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cbl_set_upgrades_except(list: *mut c_void, excluded: *const u32, count: usize) {
    civ_list::set_upgrades_except(&mut Host, list as usize, unsafe { upgrades(excluded, count) })
}

/// `SetCivilizationUpgrades`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_cbl_set_civilization_upgrades(list: *mut c_void, ids: *const u32, count: usize) {
    civ_list::set_civilization_upgrades(&mut Host, list as usize, unsafe { upgrades(ids, count) })
}

/// `ReformatUpgrades`.
#[no_mangle]
pub extern "C" fn ee_screens_cbl_reformat(list: *mut c_void, buy: bool) {
    civ_list::reformat(&mut Host, list as usize, buy)
}
