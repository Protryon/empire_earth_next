//! `extern "C"` for the civilization builder whole (`civ_whole.rs`, `civ_edit.rs`, `civ_list.rs`): its entry
//! points (the game's open paths, the timer, the blinking button, the campaign's save, the edited civilization's
//! end, the questions' answers), the list box's overrides (`EECBUIListBox`'s forwarders), and the one host
//! (`civ_host.rs`, over the C++ shims of `Civilization Builder/EECivilizationBuilder.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};

use crate::screens::civ_list;
use crate::screens::civ_whole::{self, CbHost, EeScreensCbOut};

/// The host: its ops in Rust (`civ_host.rs`) over the one-statement C++ shims.
pub(crate) struct Host;

impl CbHost for Host {
    fn op(&mut self, op: u32, p: usize, a: i64, b: i64, c: i64, text: &[u8], out: &mut EeScreensCbOut) -> i64 {
        crate::screens::civ_host::op(&mut crate::screens::game_cpp::Ffi, op, p, a, b, c, text, out)
    }
}

/// The civilization simulation's host (`EeTechCivHost`'s call: `call` an `EeTechCivCall*`, `reply` an
/// `EeTechCivReply*`; `civ_manager_calls.rs`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civ_host(call: *const c_void, reply: *mut c_void) {
    use ee_sim::tech::civ_apply::{EeTechCivCall, EeTechCivReply};
    // SAFETY: the simulation's call and reply, live for the call
    let (call, reply) = unsafe { (&*(call as *const EeTechCivCall), &mut *(reply as *mut EeTechCivReply)) };
    crate::screens::civ_manager_calls::host(&mut crate::screens::game_cpp::Ffi, call, reply)
}

/// The civilization file host (`EeTechCivFileHost`'s call: `ctx` the `EECivFileContext*`, `call` an
/// `EeTechCivFileCall*`, `reply` an `EeTechCivFileReply*`; `civ_file_calls.rs`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_civ_file_host(ctx: *mut c_void, call: *const c_void, reply: *mut c_void) {
    use ee_sim::tech::civ_file::{EeTechCivFileCall, EeTechCivFileReply};
    // SAFETY: the simulation's call and reply, live for the call
    let (call, reply) = unsafe { (&*(call as *const EeTechCivFileCall), &mut *(reply as *mut EeTechCivFileReply)) };
    crate::screens::civ_file_calls::host(&mut crate::screens::game_cpp::Ffi, ctx as usize, call, reply)
}

/// The area effects' host (`EEAEHost::Call`: `effect` the `EEAreaEffect*`, `target` the `EEComplexUnit*`, either
/// NULL; `call` an `EeAreaEffectCall*`, `reply` an `EeAreaEffectReply*`; `area_effect_host.rs`).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_ae_host(
    effect: *mut c_void,
    target: *mut c_void,
    call: *const c_void,
    reply: *mut c_void,
) {
    use ee_sim::tech::area_effect::{EeAreaEffectCall, EeAreaEffectReply};
    // SAFETY: the simulation's call and reply, live for the call
    let (call, reply) = unsafe { (&*(call as *const EeAreaEffectCall), &mut *(reply as *mut EeAreaEffectReply)) };
    let ffi = &mut crate::screens::game_cpp::Ffi;
    crate::screens::area_effect_host::host(ffi, effect as usize, target as usize, call, reply)
}

/// `EEAEManager::CollectEffects(radius, location, player, list, unit)` (`list` the `EEUnitAreaEffectList*`, `unit`
/// the `EEComplexUnit*`; `area_effect_manager.rs`).
#[no_mangle]
pub extern "C" fn ee_screens_aem_collect(
    radius: f32,
    x: f32,
    y: f32,
    player: c_ulong,
    list: *mut c_void,
    unit: *mut c_void,
) {
    let ffi = &mut crate::screens::game_cpp::Ffi;
    crate::screens::area_effect_manager::collect_effects(ffi, radius, x, y, player as u64, list as usize, unit as usize)
}

/// `EEAEManager::FindEffect(kind, x, y, player, mode, exclude)` on the manager -> the `EEAreaEffect*` or NULL.
#[no_mangle]
pub extern "C" fn ee_screens_aem_find(
    manager: *mut c_void,
    kind: c_long,
    x: f32,
    y: f32,
    player: c_ulong,
    mode: bool,
    exclude: *mut c_void,
) -> *mut c_void {
    let ffi = &mut crate::screens::game_cpp::Ffi;
    let m = manager as usize;
    crate::screens::area_effect_manager::find_effect(ffi, m, kind as i64, x, y, player as u64, mode, exclude as usize)
        as *mut c_void
}

/// The area effects' I/O op (`EEAEManager.cpp`'s `EEAEIoCall` on its `FSFile`).
pub type EeScreensAesIo = Option<unsafe extern "C" fn(ctx: *mut c_void, op: u32, value: *mut c_void) -> c_long>;

/// `EEAEManager::Load(file)` on the manager -> its result (`area_effect_running.rs`).
#[no_mangle]
pub extern "C" fn ee_screens_aes_load_templates(manager: *mut c_void, file: *mut c_void) -> c_long {
    let ffi = &mut crate::screens::game_cpp::Ffi;
    crate::screens::area_effect_running::load_templates(ffi, manager as usize, file as usize) as c_long
}

/// `EEAESaveRunningState`: written through `call` on the file `ctx`.
#[no_mangle]
pub extern "C" fn ee_screens_aes_save(ctx: *mut c_void, call: EeScreensAesIo) {
    let io = ee_sim::tech::calamity_record::EeCalamityIoFfi { ctx, call };
    crate::screens::area_effect_running::save(&mut crate::screens::game_cpp::Ffi, io)
}

/// `EEAELoadRunningState`: read through `call` on the file `ctx` -> the read's result.
#[no_mangle]
pub extern "C" fn ee_screens_aes_load(ctx: *mut c_void, call: EeScreensAesIo) -> c_long {
    let io = ee_sim::tech::calamity_record::EeCalamityIoFfi { ctx, call };
    crate::screens::area_effect_running::load(&mut crate::screens::game_cpp::Ffi, io)
}

/// The running state's fix-up after a load (`aes_after_load`'s registered call; `state` the record).
///
/// # Safety
/// `state` is a live `EeAreaEffectRunning`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_aes_restore(state: *const c_void) {
    let state = state as *const ee_sim::tech::area_effect_save::EeAreaEffectRunning;
    // SAFETY: the caller's record
    unsafe { crate::screens::area_effect_running::restore(&mut crate::screens::game_cpp::Ffi, state) }
}

/// `EEAreaEffectTemplate::Load(on_demand)` and `Unload` on the template (`area_effect_lists.rs`).
#[no_mangle]
pub extern "C" fn ee_screens_ael_template_load(template: *mut c_void, on_demand: bool) {
    crate::screens::area_effect_lists::template_load(&mut crate::screens::game_cpp::Ffi, template as usize, on_demand)
}

#[no_mangle]
pub extern "C" fn ee_screens_ael_template_unload(template: *mut c_void) {
    crate::screens::area_effect_lists::template_unload(&mut crate::screens::game_cpp::Ffi, template as usize)
}

/// `EEUnitAreaEffectList::AddAreaEffect(effect)` on the list.
#[no_mangle]
pub extern "C" fn ee_screens_ael_add(list: *mut c_void, effect: *mut c_void) {
    crate::screens::area_effect_lists::add_area_effect(
        &mut crate::screens::game_cpp::Ffi,
        list as usize,
        effect as usize,
    )
}

/// `EEUnitAreaEffectList::ApplyModifiers(kind, value, out, unit)` on the list (`value` and `out` the caller's longs).
#[no_mangle]
pub extern "C" fn ee_screens_ael_apply_modifiers(
    list: *mut c_void,
    kind: c_long,
    value: *const c_long,
    out: *mut c_long,
    unit: *mut c_void,
) {
    let ffi = &mut crate::screens::game_cpp::Ffi;
    let (l, v, o) = (list as usize, value as usize, out as usize);
    crate::screens::area_effect_lists::apply_modifiers(ffi, l, kind as i64, v, o, unit as usize)
}

/// `EEUnitAreaEffectList::Remove(kind)` on the list -> whether one was removed.
#[no_mangle]
pub extern "C" fn ee_screens_ael_remove(list: *mut c_void, kind: c_long) -> bool {
    crate::screens::area_effect_lists::remove(&mut crate::screens::game_cpp::Ffi, list as usize, kind as i64)
}

/// `EEUnitAreaEffectList::MarkDirty`, `MoveAndUpdate(x, y, value)` and `Tick(unit, elapsed)` on the list.
#[no_mangle]
pub extern "C" fn ee_screens_ael_mark_dirty(list: *mut c_void) {
    crate::screens::area_effect_lists::mark_dirty(&mut crate::screens::game_cpp::Ffi, list as usize)
}

#[no_mangle]
pub extern "C" fn ee_screens_ael_move_and_update(list: *mut c_void, x: f32, y: f32, value: f32) {
    crate::screens::area_effect_lists::move_and_update(&mut crate::screens::game_cpp::Ffi, list as usize, x, y, value)
}

#[no_mangle]
pub extern "C" fn ee_screens_ael_tick(list: *mut c_void, unit: *mut c_void, elapsed: f32) {
    crate::screens::area_effect_lists::tick(&mut crate::screens::game_cpp::Ffi, list as usize, unit as usize, elapsed)
}

/// `EECivilizationManager::FillCivilizationList(list)` (`list` the `EECBUIListBox*`).
#[no_mangle]
pub extern "C" fn ee_screens_civ_fill_list(list: *mut c_void) {
    crate::screens::civ_manager_calls::fill_civilization_list(
        &mut crate::screens::game_cpp::Ffi,
        &mut Host,
        list as usize,
    )
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
