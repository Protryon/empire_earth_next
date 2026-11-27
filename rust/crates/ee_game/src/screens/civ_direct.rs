//! The civilization builder's host calls Rust answers itself (player/tech lane, round 3): the calls of
//! [`Cpp`](crate::screens::game_cpp::Cpp) marked `=> path` in game_cpp.rs. Each was a one-statement C++ shim of
//! `Civilization Builder/*.cpp` over an object whose state is Rust's: the toolkit's forms, edit boxes, labels and
//! list boxes (the same inward calls their C++ forwarders make: `ee_ui::direct`). The generic logic over `Cpp`
//! (civ_host.rs and the rest) and its tests on the recording fake are unchanged; only the FFI implementation of
//! these calls is.
//!
//! As the C++ did, a NULL object answers 0 (the toolkit's direct calls answer 0 for an unknown object, where the C++
//! would have crashed).

// (`c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the calls' `u32` stay)
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_char, c_long, c_ulong, c_void};

use ee_ui::direct::{self, Cpp as UiCpp, Kind};
use ee_ui::editobj::EeUiEditCall;
use ee_ui::formobj::EeUiFormCall;
use ee_ui::listobj::{EeUiList, EeUiListCall};
use ee_ui::managerobj::EeUiManagerCall;
use ee_ui::obj::EeUiOp;

use ee_audio::sound::managerobj::{
    ee_sound_manager_load, ee_sound_manager_play, ee_sound_manager_unload, EeSoundManager,
};
use ee_net::net::ffi_comms::EeNetCommsField;
use ee_sim::app::unit_sets::{ee_unit_sets_contains, ee_unit_sets_for_each, ee_unit_sets_get};
use ee_sim::tech::civ::ee_tech_civ_power_record;
use ee_sim::tech::nodes::EeTechNodeFields;
use ee_sim::tech_consts::EECivPowerID;
use ee_sim::units::player_lists;
use ee_sim::world::layout::layout;
use ee_sim::world::player_fields::EeUnitsPlayerSaved;
use ee_sim::world::{ClassView, NodeView, PlayerView, StatsView, UnitView, WorldRead, WorldView};
use ee_sim::world_field;

use crate::app::game_options::EeAppOptionsFields;
use crate::civilization::EeGameCivFields;
use crate::screens::civ_list_state;
use crate::screens::editor_direct::{edit_line, edit_set_text};
use crate::screens::game_cpp::{Cpp as _, EeScreensShimOut, EeScreensShimText, EeScreensShimULongOut, Ffi};
use crate::screens::hud_direct::{label_set_long, label_set_text, label_text};

fn ui(p: usize) -> UiCpp {
    p as UiCpp
}

/// The game's manager (`gUIManager`).
fn manager() -> UiCpp {
    direct::manager_of(core::ptr::null_mut())
}

/// A text handed in (the bytes of the call).
fn text(t: EeScreensShimText) -> &'static [u8] {
    if t.len == 0 || t.ptr.is_null() {
        return &[];
    }
    // SAFETY: a shim's text lives for the call it is handed to (and this one's use ends inside it)
    unsafe { core::slice::from_raw_parts(t.ptr, t.len) }
}

/// The out text's bytes appended (`EEShimOut`).
fn out(o: EeScreensShimOut, bytes: &[u8]) {
    // SAFETY: an out text is a live Vec<u8> for the call
    unsafe { crate::screens::game_cpp::ee_screens_shim_out(o.0, bytes.as_ptr(), bytes.len()) };
}

/// `dynamic_cast<K*>(control)`: the control itself when it is a toolkit object of `kind` (the toolkit's classes
/// have one base chain, so the cast moves no pointer), else 0.
fn cast(kind: Kind, control: usize) -> usize {
    if direct::find(kind, control as *const c_void).is_null() {
        0
    } else {
        control
    }
}

// ---- EECivilizationBuilder.cpp's builder host (civ_host.rs)

/// `gUIManager->GetFormByID(id)`.
pub(crate) fn cbh_form(id: u32) -> usize {
    direct::form_by_id(manager(), id as c_ulong) as usize
}

/// `form->GetControlById(id)`.
pub(crate) fn cbh_control_by_id(form: usize, id: u32) -> usize {
    direct::form_call(ui(form), EeUiFormCall::ControlById, id as c_ulong as isize, 0) as usize
}

/// `form->GetControl(index)`.
pub(crate) fn cbh_control(form: usize, index: u32) -> usize {
    direct::control(ui(form), index as c_ulong) as usize
}

/// `dynamic_cast<UIEditBox*>(control)`.
pub(crate) fn cbh_as_edit(control: usize) -> usize {
    cast(Kind::Edit, control)
}

/// `dynamic_cast<UITextLabel*>(control)`.
pub(crate) fn cbh_as_label(control: usize) -> usize {
    cast(Kind::Label, control)
}

/// `edit->GetText(0)`.
pub(crate) fn cbh_edit_text(edit: usize, o: EeScreensShimOut) {
    out(o, &edit_line(ui(edit)));
}

/// `edit->SetText(text)`.
pub(crate) fn cbh_edit_set_text(edit: usize, t: EeScreensShimText) {
    edit_set_text(ui(edit), text(t));
}

/// `edit->SelectText()`.
pub(crate) fn cbh_edit_select(edit: usize) {
    direct::edit_call(ui(edit), EeUiEditCall::SelectAll, 0, 0);
}

/// `edit->GetIndex()`.
pub(crate) fn cbh_edit_index(edit: usize) -> u32 {
    direct::with_fields(ui(edit), |r| r.mIndex as u32).unwrap_or(0)
}

/// `form->SetTabStop(index)`.
pub(crate) fn cbh_form_set_tab_stop(form: usize, index: u32) {
    direct::form_call(ui(form), EeUiFormCall::SetTabStop, index as c_ulong as isize, 0);
}

/// `label->GetText(0)`.
pub(crate) fn cbh_label_text(label: usize, o: EeScreensShimOut) {
    out(o, &label_text(ui(label)));
}

/// `label->SetText(text)`.
pub(crate) fn cbh_label_set_text(label: usize, t: EeScreensShimText) {
    label_set_text(ui(label), text(t));
}

/// `label->SetText((long)number)`.
pub(crate) fn cbh_label_set_number(label: usize, number: i32) {
    label_set_long(ui(label), number as c_long);
}

/// `gUIManager->Push(form)`.
pub(crate) fn cbh_push(form: usize) -> i32 {
    direct::manager_call(manager(), EeUiManagerCall::Push, form as isize, 0, 0) as i32
}

/// `gUIManager->GetCurrentForm()`.
pub(crate) fn cbh_current_form() -> usize {
    direct::manager_call(manager(), EeUiManagerCall::CurrentForm, 0, 0, 0) as usize
}

/// `form->SetFormOpenEvent(event)`.
pub(crate) fn cbh_form_set_open_event(form: usize, event: usize) {
    direct::form_call(ui(form), EeUiFormCall::SetEvent, 0, event as isize);
}

/// `form->SetFormCloseEvent(event)`.
pub(crate) fn cbh_form_set_close_event(form: usize, event: usize) {
    direct::form_call(ui(form), EeUiFormCall::SetEvent, 1, event as isize);
}

/// `form->GetFormID()`.
pub(crate) fn cbh_form_id(form: usize) -> u32 {
    direct::form_call(ui(form), EeUiFormCall::FormId, 0, 0) as u32
}

/// `kFormMainGame`.
pub(crate) fn cbh_main_game_form_id() -> u32 {
    crate::screens::ui_ids::EEFormID::kFormMainGame as u32
}

// ---- the builder's list box (EECBUIListBox: a UILBString, its lines and selection the Rust list's; civ_host.rs's
// list ops)

/// The list's Rust object (NULL for another control).
fn list(control: usize) -> *const EeUiList {
    direct::find(Kind::List, control as *const c_void) as *const EeUiList
}

/// An op of the list's direct path (0 for another control).
fn list_op(control: usize, op: EeUiOp, a: isize) -> isize {
    direct::list_op(ui(control), op, a, 0, 0).map_or(0, |r| r.0)
}

/// One of `UIListBox`'s methods (0 for another control).
fn list_call(control: usize, call: EeUiListCall, a: isize, b: isize) -> isize {
    direct::list_call(ui(control), call, a, b).unwrap_or(0)
}

/// `list->GetNumLines()` (the lines: no class overrides it).
pub(crate) fn cbl_count(list: usize) -> u32 {
    list_op(list, EeUiOp::ListLineCount, 0) as u32
}

/// `list->mData[line]`.
pub(crate) fn cbl_line(list: usize, line: u32, o: EeScreensShimOut) {
    out(o, &direct::list_line(ui(list), line as isize).unwrap_or_default());
}

/// `list->UIListBox<UWideString>::AppendLine(text)` (no class overrides it).
pub(crate) fn cbl_append(list: usize, t: EeScreensShimText) {
    let line = direct::c_text(text(t));
    list_op(list, EeUiOp::ListAppendText, line.as_ptr() as isize);
}

/// `list->UIListBox<UWideString>::DeleteLine(line)` (the base's, not the builder's override).
pub(crate) fn cbl_delete(control: usize, line: u32) {
    let l = list(control);
    if !l.is_null() {
        // SAFETY: a live list box
        unsafe { ee_ui::listobj::ee_ui_list_delete_line(l, line as c_ulong) };
    }
}

/// `list->DeleteAllEntries()` (no class overrides it).
pub(crate) fn cbl_delete_all(list: usize) {
    list_op(list, EeUiOp::ListDeleteAll, 0);
}

/// `list->UIListBox<UWideString>::ChangeSelectionIndex(line, fire, scroll)` (the base's).
pub(crate) fn cbl_select(list: usize, line: u32, fire: bool, scroll: bool) {
    list_call(
        list,
        EeUiListCall::ChangeSelectionIndex,
        line as c_ulong as isize,
        fire as isize | (scroll as isize) << 1,
    );
}

/// `list->GetCurrentSelection(*index)`.
pub(crate) fn cbl_current(control: usize, index: EeScreensShimULongOut) -> i32 {
    let l = list(control);
    if l.is_null() {
        return 0;
    }
    // SAFETY: a live list box; the out number of the call
    unsafe { ee_ui::listobj::ee_ui_list_current_selection(l, index.0) as i32 }
}

/// `list->GetAutoSelect()`.
pub(crate) fn cbl_auto(list: usize) -> bool {
    list_call(list, EeUiListCall::AutoSelect, 0, 0) != 0
}

/// `list->GetHasSelection()`.
pub(crate) fn cbl_has(list: usize) -> bool {
    list_call(list, EeUiListCall::HasSelection, 0, 0) != 0
}

/// `list->GetSelectionIndex()`.
pub(crate) fn cbl_index(list: usize) -> u32 {
    list_call(list, EeUiListCall::SelectionIndex, 0, 0) as u32
}

/// `list->SetDirty(true)` (the virtual).
pub(crate) fn cbl_dirty(list: usize) {
    direct::control_virtual(ui(list), EeUiOp::SetDirty, 1, 0, &mut [0.0; 4]);
}

/// `list->GetDrawCount()`.
pub(crate) fn cbl_draw_count(list: usize) -> u32 {
    list_call(list, EeUiListCall::DrawCount, 0, 0) as u32
}

/// `list->GetLineHeight()`.
pub(crate) fn cbl_line_height(list: usize) -> u32 {
    list_call(list, EeUiListCall::LineHeight, 0, 0) as u32
}

/// `list->DrawScrollBarText()`.
pub(crate) fn cbl_draw_end(list: usize) -> i32 {
    list_call(list, EeUiListCall::DrawScrollBarText, 0, 0) as i32
}

// ---- the list's constructor and copy (EECBUIListBox.cpp's: hud_bodies.rs)

/// A list call with three floats (0 for another control).
fn list_call_f(control: usize, call: EeUiListCall, f0: f32, f1: f32, f2: f32) {
    let l = list(control);
    if !l.is_null() {
        let mut f = [f0, f1, f2, 0.0];
        // SAFETY: a live list box
        unsafe { ee_ui::listobj::ee_ui_list_call(l, call, 0, 0, f.as_mut_ptr()) };
    }
}

/// `list->SetScrollBarBgColor(GEColor(red, green, blue))`.
pub(crate) fn cbb_scroll_bg_colour(list: usize, red: f32, green: f32, blue: f32) {
    list_call_f(list, EeUiListCall::SetScrollBarBgColor, red, green, blue);
}

/// `list->SetScrollBarBgOpacity(opacity)`.
pub(crate) fn cbb_scroll_bg_opacity(list: usize, opacity: f32) {
    list_call_f(list, EeUiListCall::SetScrollBarBgOpacity, opacity, 0.0, 0.0);
}

/// `list->GetSelectionIndex()`.
pub(crate) fn cbb_selection(list: usize) -> u64 {
    list_call(list, EeUiListCall::SelectionIndex, 0, 0) as c_ulong as u64
}

/// `list->SetCopiedHasSelection(false)`.
pub(crate) fn cbb_copied_no_selection(list: usize) {
    list_call(list, EeUiListCall::SetCopiedHasSelection, 0, 0);
}

// ---- the world's players and units through ee_sim's views (the builder's, the civilization capture's, the area
// effects' hosts: the simulation or UI thread inside a call from C++, which holds the world)

fn token() -> WorldRead {
    // SAFETY: these calls come from C++ hosts that hold the world (the module's threads)
    unsafe { WorldRead::begin() }
}

/// `gWorld.mPlayers[index]` (0: an empty slot or no world).
fn player_handle(index: i64) -> usize {
    let t = token();
    usize::try_from(index)
        .ok()
        .and_then(|i| WorldView::global(&t).and_then(|w| w.player(i)))
        .map_or(0, |p| p.handle() as usize)
}

/// `gWorld.GetCurrentPlayer()` (0: none).
fn current_player_handle() -> usize {
    let t = token();
    WorldView::global(&t).and_then(|w| w.current_player()).map_or(0, |p| p.handle() as usize)
}

/// The unit `handle`'s view handed to `f` (`default` for 0).
fn with_unit<R>(handle: usize, default: R, f: impl FnOnce(&UnitView<'_>) -> R) -> R {
    let t = token();
    // SAFETY: the hosts hand live units
    match unsafe { UnitView::of(&t, handle as u64) } {
        Some(u) => f(&u),
        None => default,
    }
}

/// The current player's saved numbers (NULL without one).
fn current_player_saved() -> *mut EeUnitsPlayerSaved {
    let p = current_player_handle();
    if p == 0 {
        core::ptr::null_mut()
    } else {
        (p + layout().player_saved as usize) as *mut EeUnitsPlayerSaved
    }
}

/// A C++ `bool`'s byte (never a Rust `bool` of an unchecked byte).
///
/// # Safety
/// `p` is a readable `bool` member.
unsafe fn bool_byte(p: *const bool) -> bool {
    // SAFETY: the contract
    unsafe { *(p as *const u8) != 0 }
}

/// `gWorld.GetCurrentPlayer()`.
pub(crate) fn cbh_current_player() -> usize {
    current_player_handle()
}

/// `(unsigned char)gWorld.mCurrentPlayer`.
pub(crate) fn cbh_player_id() -> u8 {
    let t = token();
    WorldView::global(&t).map_or(0, |w| w.current_player_index() as u8)
}

/// `gWorld.mNumPlayers`.
pub(crate) fn cbh_num_players() -> u32 {
    let t = token();
    WorldView::global(&t).map_or(0, |w| w.num_players() as u32)
}

/// `gWorld.GetCurrentPlayer()->mCivilizationChosen`.
pub(crate) fn cbh_civ_chosen() -> bool {
    let f = current_player_saved();
    // SAFETY: the live player's member
    !f.is_null() && unsafe { bool_byte(core::ptr::addr_of!((*f).mCivilizationChosen)) }
}

/// `gWorld.GetCurrentPlayer()->mCivilizationLocked`.
pub(crate) fn cbh_civ_locked() -> bool {
    let f = current_player_saved();
    // SAFETY: the live player's member
    !f.is_null() && unsafe { bool_byte(core::ptr::addr_of!((*f).mCivilizationLocked)) }
}

/// `gWorld.GetCurrentPlayer()->mCivilizationChosen = value`.
pub(crate) fn cbh_set_civ_chosen(value: bool) {
    let f = current_player_saved();
    if !f.is_null() {
        // SAFETY: the live player's member
        unsafe { (*f).mCivilizationChosen = value };
    }
}

/// `gWorld.GetCurrentPlayer()->mCivilizationLocked = value`.
pub(crate) fn cbh_set_civ_locked(value: bool) {
    let f = current_player_saved();
    if !f.is_null() {
        // SAFETY: the live player's member
        unsafe { (*f).mCivilizationLocked = value };
    }
}

/// `gWorld.mPlayers[index]` (the civilization capture's).
pub(crate) fn civc_player(index: i64) -> usize {
    player_handle(index)
}

/// `static_cast<EeUnitsPlayerSaved*>(player)`.
pub(crate) fn civc_player_saved(player: usize) -> usize {
    if player == 0 {
        0
    } else {
        player + layout().player_saved as usize
    }
}

/// `static_cast<EeUnitsPlayerFields*>(player)`.
pub(crate) fn civc_player_fields(player: usize) -> usize {
    if player == 0 {
        0
    } else {
        player + layout().player_fields as usize
    }
}

/// `gWorld.mNumPlayers` (the area effects' capture).
pub(crate) fn aec_num_players() -> u64 {
    cbh_num_players() as u64
}

/// `gWorld.mPlayers[index]`.
pub(crate) fn aec_player(index: u64) -> usize {
    player_handle(index as i64)
}

/// `player->mFirstUnit`.
pub(crate) fn aec_first_unit(player: usize) -> usize {
    let t = token();
    // SAFETY: a live player
    unsafe { PlayerView::of(&t, player as u64) }.and_then(|p| p.first_unit()).map_or(0, |u| u.handle() as usize)
}

/// `unit->mNext`.
pub(crate) fn aec_next_unit(unit: usize) -> usize {
    with_unit(unit, 0, |u| u.next().map_or(0, |n| n.handle() as usize))
}

/// `((EEComplexUnit*)unit)->mIsCloakable`.
pub(crate) fn aec_cloakable(unit: usize) -> bool {
    with_unit(unit, false, |u| u.cloakable())
}

/// `((EEComplexUnit*)unit)->mIsCloaked`.
pub(crate) fn aec_cloaked(unit: usize) -> bool {
    with_unit(unit, false, |u| u.cloaked())
}

/// `gWorld.mPlayers[player]->mFirstUnit` (the civilization manager's host).
pub(crate) fn civh_first_unit(player: i32) -> usize {
    aec_first_unit(player_handle(player as i64))
}

/// `unit->mNext`.
pub(crate) fn civh_next_unit(unit: usize) -> usize {
    aec_next_unit(unit)
}

/// `unit->mUnitClass->mUnitType`.
pub(crate) fn civh_unit_type(unit: usize) -> u64 {
    with_unit(unit, 0, |u| u.class().map_or(0, |c| c.unit_type()))
}

// the area effects' host (area_effect_host.rs): the unit `t` an EEComplexUnit

/// `gWorld.mPlayers[index]`.
pub(crate) fn aeh_player(index: i32) -> usize {
    player_handle(index as i64)
}

/// `gWorld.mPlayers[gWorld.mCurrentPlayer]`.
pub(crate) fn aeh_current_player() -> usize {
    current_player_handle()
}

/// `gWorld.mInCinematic`.
pub(crate) fn aeh_in_cinematic() -> bool {
    let t = token();
    WorldView::global(&t).is_some_and(|w| w.in_cinematic())
}

/// `player->mEnemies[other->mPlayerID] != 0`.
pub(crate) fn aeh_enemy(player: usize, other: usize) -> bool {
    let t = token();
    // SAFETY: live players
    let (p, o) = unsafe { (PlayerView::of(&t, player as u64), PlayerView::of(&t, other as u64)) };
    match (p, o) {
        (Some(p), Some(o)) => p.enemy(o.id() as usize) != 0,
        _ => false,
    }
}

/// `t->mIsBuilt`.
pub(crate) fn aeh_built(t: usize) -> bool {
    with_unit(t, false, |u| u.is_built())
}

/// `t->mPhysicalModel.GetLocation().GetPosition().GetX()`.
pub(crate) fn aeh_position_x(t: usize) -> f32 {
    with_unit(t, 0.0, |u| u.location()[0])
}

/// `...GetY()`.
pub(crate) fn aeh_position_y(t: usize) -> f32 {
    with_unit(t, 0.0, |u| u.location()[1])
}

/// `(EEComplexUnit*)t->mComplexUnit`.
pub(crate) fn aeh_carrier(t: usize) -> usize {
    with_unit(t, 0, |u| u.complex_unit_handle() as usize)
}

/// `t->mUnitClass->GetUnitType()`.
pub(crate) fn aeh_unit_type(t: usize) -> u64 {
    civh_unit_type(t)
}

/// `t->mPlanetID`.
pub(crate) fn aeh_planet(carrier: usize) -> i32 {
    with_unit(carrier, 0, |u| world_field!(u.complex(), mPlanetID) as i32)
}

/// `t->mHitPoints`.
pub(crate) fn aeh_hit_points(t: usize) -> i32 {
    with_unit(t, 0, |u| u.hit_points() as i32)
}

/// `t->mStats->mMaxHitPoints`.
pub(crate) fn aeh_max_hit_points(t: usize) -> i32 {
    with_unit(t, 0, |u| u.stats().map_or(0, |s| s.max_hit_points() as i32))
}

/// `t->mCamouflageTimer`.
pub(crate) fn aeh_camouflage_timer(t: usize) -> u64 {
    with_unit(t, 0, |u| world_field!(u.complex(), mCamouflageTimer) as u64)
}

/// `t->mU32C`.
pub(crate) fn aeh_u32c(t: usize) -> i64 {
    with_unit(t, 0, |u| world_field!(u.complex(), mU32C) as i64)
}

/// `owner->mStats->mAreaEffectScale`.
pub(crate) fn aeh_owner_scale(owner: usize) -> f32 {
    with_unit(owner, 0.0, |u| u.stats().map_or(0.0, |s| world_field!(s.fields(), mAreaEffectScale)))
}

/// `owner->mStats->mLeadership`.
pub(crate) fn aeh_leadership(owner: usize) -> f32 {
    with_unit(owner, 0.0, |u| u.stats().map_or(0.0, |s| world_field!(s.fields(), mLeadership)))
}

/// `owner->mUnitClass->mGarrisonBonus`.
pub(crate) fn aeh_garrison_bonus(owner: usize) -> f32 {
    with_unit(owner, 0.0, |u| u.class().map_or(0.0, |c| world_field!(c.fields(), mGarrisonBonus)))
}

/// `((EEUnit*)unit)->mComplexUnit` (the running state's).
pub(crate) fn aes_unit_complex(unit: usize) -> usize {
    with_unit(unit, 0, |u| u.complex_unit_handle() as usize)
}

/// `unit->GetUnitID()`.
pub(crate) fn aes_unit_id(unit: usize) -> u64 {
    with_unit(unit, 0, |u| u.id() as u64)
}

/// `unit->GetPlayer()->mPlayerID`.
pub(crate) fn ae_unit_player_id(unit: usize) -> i32 {
    with_unit(unit, 0, |u| u.player().map_or(0, |p| p.id() as i32))
}

// ---- the game options, the unit sets, the powers, the classes and the tech nodes (Rust's records and tables)

/// `gWorld.mGameOptions` (NULL without a world).
fn options() -> *mut EeAppOptionsFields {
    let t = token();
    // the options' plain members are their base, at the object's start (gamecpp options.rs)
    WorldView::global(&t).map_or(core::ptr::null_mut(), |w| w.game_options_handle() as *mut EeAppOptionsFields)
}

/// `gWorld.mGameOptions.CanBuyCivilization()`.
pub(crate) fn cbh_can_buy() -> bool {
    let o = options();
    !o.is_null() && crate::gamecpp::options::ee_gamecpp_options_can_buy_civilization(o as *mut c_void)
}

/// `gAllowCustomCivs` (`gWorld.mGameOptions.mAllowCustomCivs`).
pub(crate) fn cbh_custom_civs() -> bool {
    let o = options();
    // SAFETY: the world's options
    !o.is_null() && unsafe { bool_byte(core::ptr::addr_of!((*o).mAllowCustomCivs)) }
}

/// `gWorld.GetCurrentPlayer()->GetGameResult()`.
pub(crate) fn cbh_game_result() -> i32 {
    let s = current_player_saved();
    if s.is_null() {
        return 0;
    }
    // SAFETY: the live player's member
    unsafe { (*s).mGameResult as i32 }
}

/// `gWorld.mGameOptions.CanBuyCivilization()` (the civilization files' host).
pub(crate) fn civf_can_buy() -> bool {
    cbh_can_buy()
}

/// `gAllowCustomCivs`.
pub(crate) fn civf_custom_civs() -> bool {
    cbh_custom_civs()
}

/// `gIsScenarioGame` (`gWorld.mGameOptions.mIsScenarioGame`).
pub(crate) fn civf_scenario_game() -> bool {
    let o = options();
    // SAFETY: the world's options
    !o.is_null() && unsafe { bool_byte(core::ptr::addr_of!((*o).mIsScenarioGame)) }
}

/// `gUnitSetManager.GetSet(set) != NULL`.
pub(crate) fn civh_set_exists(set: i64) -> bool {
    !ee_unit_sets_get(set as c_ulong as usize).is_null()
}

/// `gUnitSetManager.GetSet(set)->Contains(kind)` (the callers ask civh_set_exists first).
pub(crate) fn civh_set_contains(set: i64, kind: i64) -> bool {
    ee_unit_sets_contains(set as c_ulong as usize, kind as c_ulong as u32)
}

/// `EEUnitClassCount()`.
pub(crate) fn civh_class_count() -> i64 {
    let t = token();
    ClassView::count(&t) as i64
}

/// `gUnitClassTable[kind] != NULL`.
pub(crate) fn civh_class_listed(kind: i64) -> bool {
    let t = token();
    usize::try_from(kind).ok().and_then(|k| ClassView::by_index(&t, k)).is_some()
}

/// `upgrade->mUnitClass`.
pub(crate) fn civh_upgrade_class(up: usize) -> usize {
    let t = token();
    // SAFETY: a live class upgrade
    unsafe { StatsView::of(&t, up as u64) }.map_or(0, |s| s.class_handle() as usize)
}

/// `static_cast<EeUnitsUnitClassUpgradeFields*>(upgrade)`.
pub(crate) fn civh_upgrade_fields(up: usize) -> usize {
    if up == 0 {
        0
    } else {
        up + layout().stats_fields as usize
    }
}

/// `static_cast<EeUnitsUnitClassFields*>(unit_class)`.
pub(crate) fn civh_class_fields(k: usize) -> usize {
    if k == 0 {
        0
    } else {
        k + layout().unit_class_fields as usize
    }
}

/// The tech node `node`'s record.
fn node_fields(node: usize) -> *mut EeTechNodeFields {
    (node + layout().node_fields as usize) as *mut EeTechNodeFields
}

/// `node->mIsResearched`.
pub(crate) fn civh_node_researched(node: usize) -> bool {
    // SAFETY: a live node's member
    node != 0 && unsafe { bool_byte(core::ptr::addr_of!((*node_fields(node)).mIsResearched)) }
}

/// `node->mIsAllowed = value`.
pub(crate) fn civh_node_set_allowed(node: usize, value: bool) {
    if node != 0 {
        // SAFETY: a live node's member
        unsafe { (*node_fields(node)).mIsAllowed = value };
    }
}

/// `gUnitSetManager.Contains(t->GetUnitClass()->GetUnitType(), set)`.
pub(crate) fn aeh_set_contains(t: usize, set: i32) -> bool {
    ee_unit_sets_contains(set as c_ulong as usize, civh_unit_type(t) as u32)
}

/// `gCivPowersManager.HasPower(t->GetPlayer(), kCivPowerCamouflage)`.
pub(crate) fn aeh_has_camouflage(t: usize) -> bool {
    let player = with_unit(t, 0, |u| u.player().map_or(0, |p| p.handle() as usize));
    crate::screens::civ_manager_ffi::ee_screens_civm_has_power(player, EECivPowerID::kCivPowerCamouflage as c_long)
}

/// The camouflage power's unit set (`GetPowerRecord(kCivPowerCamouflage)->mUnitSet`).
fn camouflage_set() -> usize {
    let p = ee_tech_civ_power_record(EECivPowerID::kCivPowerCamouflage as i32);
    if p.is_null() {
        return usize::MAX;
    }
    // SAFETY: a power record of the table (the C++ read it without a check)
    unsafe { (*p).mUnitSet as c_ulong as usize }
}

/// `gUnitSetManager.GetSet(camouflage's set) != NULL`.
pub(crate) fn aeh_camouflage_set_exists() -> bool {
    !ee_unit_sets_get(camouflage_set()).is_null()
}

/// `GetSet(camouflage's set)->Contains(t->mUnitClass->mUnitType)`.
pub(crate) fn aeh_camouflage_set_contains(t: usize) -> bool {
    ee_unit_sets_contains(camouflage_set(), civh_unit_type(t) as u32)
}

/// `ee_units_lists_len(player->mLists, EE_UNITS_LISTS_MEDIA)` (the capture's loaded media count).
pub(crate) fn civc_media_count(player: usize) -> u64 {
    if player == 0 {
        0
    } else {
        player_lists::player_op(player as u64, player_lists::EE_UNITS_LISTS_MEDIA, player_lists::ListOp::Len)
    }
}

// ---- the communications, the sound manager, a unit set's types and a node's kind

/// `gCommunications->GetIsMultiplayerSession()` (false without communications).
pub(crate) fn cbh_multiplayer() -> bool {
    let n = crate::gamecpp::net::global();
    !n.comms.is_null() && n.get(EeNetCommsField::IsMultiplayerSession) != 0
}

/// `gCommunications->GetIsMultiplayerSession()` (the civilization files' host).
pub(crate) fn civf_multiplayer() -> bool {
    cbh_multiplayer()
}

/// `gSoundManager` (the UI manager's).
fn sound_manager() -> *const EeSoundManager {
    direct::manager_call(manager(), EeUiManagerCall::SoundManager, 0, 0, 0) as *const EeSoundManager
}

/// `gSoundManager->Load(name, 0, 1)`.
pub(crate) fn cbh_sound_load(name: EeScreensShimText) -> u32 {
    let m = sound_manager();
    if m.is_null() {
        return 0;
    }
    let name = direct::c_text(text(name));
    // SAFETY: the live sound manager; a NUL-terminated name
    unsafe { ee_sound_manager_load(m, name.as_ptr() as *const c_char, 0, 1) }
}

/// `gSoundManager->Unload(sound)`.
pub(crate) fn cbh_sound_unload(sound: u32) {
    let m = sound_manager();
    if !m.is_null() {
        // SAFETY: the live sound manager
        unsafe { ee_sound_manager_unload(m, sound) };
    }
}

/// `gSoundManager->Play(sound, 1.0f, 0.0f, 1.0f, false, NULL)`.
pub(crate) fn cbh_sound_play(sound: u32) {
    let m = sound_manager();
    if !m.is_null() {
        // SAFETY: the live sound manager
        unsafe { ee_sound_manager_play(m, sound, 1.0, 0.0, 1.0, false) };
    }
}

thread_local! {
    /// The unit types of the set the civilization capture walked last (`EECivcCollect`'s vector).
    static SET_TYPES: core::cell::RefCell<Vec<i64>> = const { core::cell::RefCell::new(Vec::new()) };
}

/// The set walk's visit: the type kept (`EECivcCollect::Execute`, which goes on).
unsafe extern "C" fn collect_type(_functor: *mut c_void, unit_type: u32) -> bool {
    SET_TYPES.with(|t| t.borrow_mut().push(unit_type as c_ulong as c_long as i64));
    true
}

/// `kScratchCivcTypes.clear()`.
pub(crate) fn civc_types_clear() {
    SET_TYPES.with(|t| t.borrow_mut().clear());
}

/// `gUnitSetManager.ForEach(set, EECivcCollect)`: the set's unit types, in its order.
pub(crate) fn civc_collect(set: i64) {
    // SAFETY: the visit takes no functor
    unsafe { ee_unit_sets_for_each(set as c_long as c_ulong as usize, core::ptr::null_mut(), collect_type) };
}

/// The types collected.
pub(crate) fn civc_types_count() -> usize {
    SET_TYPES.with(|t| t.borrow().len())
}

/// The type `index` collected.
pub(crate) fn civc_type_at(index: usize) -> i64 {
    SET_TYPES.with(|t| t.borrow()[index])
}

/// `node->mData->mKind`.
pub(crate) fn aeh_node_kind(node: usize) -> i64 {
    let t = token();
    // SAFETY: a live tech node
    unsafe { NodeView::of(&t, node as u64) }.and_then(|n| n.data()).map_or(0, |d| world_field!(d, mKind) as i64)
}

// ---- the builder list box's own members (civ_list_state.rs)

/// `list->mSelectUpgradesOnly`.
pub(crate) fn cbl_upgrades_only(list: usize) -> bool {
    civ_list_state::get(list).upgrades_only
}

/// `list->mShowPoints`.
pub(crate) fn cbl_show_points(list: usize) -> bool {
    civ_list_state::get(list).show_points
}

/// `list->mSortLines`.
pub(crate) fn cbl_sort(list: usize) -> bool {
    civ_list_state::get(list).sort
}

/// `list->mCopiedShowPoints`.
pub(crate) fn cbl_copied_points(list: usize) -> bool {
    civ_list_state::get(list).copied_points
}

/// `list->mSelectUpgradesOnly = value`.
pub(crate) fn cbl_set_upgrades_only(list: usize, value: bool) {
    civ_list_state::update(list, |s| s.upgrades_only = value);
}

/// `list->mShowPoints = value`.
pub(crate) fn cbl_set_show_points(list: usize, value: bool) {
    civ_list_state::update(list, |s| s.show_points = value);
}

/// `list->mSortLines = value`.
pub(crate) fn cbl_set_sort(list: usize, value: bool) {
    civ_list_state::update(list, |s| s.sort = value);
}

/// `list->mCopiedShowPoints = value`.
pub(crate) fn cbl_set_copied_points(list: usize, value: bool) {
    civ_list_state::update(list, |s| s.copied_points = value);
}

/// `list->GetTabable()` (the builder list's `mIsTabable`).
pub(crate) fn cbl_tabable(list: usize) -> bool {
    civ_list_state::get(list).tabable
}

/// `text->SetFgColor(list->mC148)`.
pub(crate) fn cbl_colour_header(text: usize, list: usize) {
    let [r, g, b] = civ_list_state::get(list).copied_header;
    Ffi.cbl_colour(text, r, g, b);
}

/// `list->mC13C = GEColor(red, green, blue)` (the constructor's body).
pub(crate) fn cbb_set_header_colour(list: usize, red: f32, green: f32, blue: f32) {
    civ_list_state::update(list, |s| s.header = [red, green, blue]);
}

/// `list->mSelectUpgradesOnly = value`.
pub(crate) fn cbb_set_select_upgrades_only(list: usize, value: bool) {
    cbl_set_upgrades_only(list, value);
}

/// `list->mShowPoints = value`.
pub(crate) fn cbb_set_show_points(list: usize, value: bool) {
    cbl_set_show_points(list, value);
}

/// `list->mIsTabable = value`.
pub(crate) fn cbb_set_tabable(list: usize, value: bool) {
    civ_list_state::update(list, |s| s.tabable = value);
}

/// `list->mC148 = list->mC13C` (CopyData).
pub(crate) fn cbb_copy_header(list: usize) {
    civ_list_state::update(list, |s| s.copied_header = s.header);
}

/// `list->mCopiedShowPoints = list->mShowPoints`.
pub(crate) fn cbb_copy_show_points(list: usize) {
    civ_list_state::update(list, |s| s.copied_points = s.show_points);
}

/// `list->SetSortLines(value)`.
pub(crate) fn cbb_set_sort_lines(list: usize, value: bool) {
    cbl_set_sort(list, value);
}

/// The choice form's `list->mShowPoints = value` (EEScreens.cpp's).
pub(crate) fn cb_list_set_show_points(list: usize, value: bool) {
    cbl_set_show_points(list, value);
}

/// `list->mSelectUpgradesOnly = value`.
pub(crate) fn cb_list_set_select_upgrades_only(list: usize, value: bool) {
    cbl_set_upgrades_only(list, value);
}

/// `list->SetSortLines(value)`.
pub(crate) fn cb_list_set_sort(list: usize, value: bool) {
    cbl_set_sort(list, value);
}

/// `list->mC13C = GEColor(red, green, blue)`.
pub(crate) fn cb_list_set_colour(list: usize, red: f32, green: f32, blue: f32) {
    cbb_set_header_colour(list, red, green, blue);
}

// ---- a civilization's plain members (its Rust base: ee_game civilization.rs)

fn civ(c: usize) -> EeGameCivFields {
    if c == 0 {
        return EeGameCivFields::default();
    }
    *crate::civilization::fields(c as crate::gamecpp::P)
}

/// `civ->GetAvailablePoints()`.
pub(crate) fn cbh_civ_available(c: usize) -> i32 {
    civ(c).mAvailablePoints as c_long as i32
}

/// `civ->GetUnchanged()`.
pub(crate) fn cbh_civ_unchanged(c: usize) -> i32 {
    civ(c).mUnchanged as i32
}

/// `civ->GetMaxPoints()`.
pub(crate) fn cbh_civ_max(c: usize) -> i32 {
    civ(c).mMaxPoints as c_long as i32
}

/// `civ->mAvailablePoints` (the civilization capture's).
pub(crate) fn civc_civ_available(c: usize) -> u32 {
    civ(c).mAvailablePoints as u32
}

/// `civ->mMaxPoints`.
pub(crate) fn civc_civ_max(c: usize) -> u32 {
    civ(c).mMaxPoints as u32
}

/// `civ->mNoPointsAdded`.
pub(crate) fn civc_civ_no_points(c: usize) -> bool {
    civ(c).mNoPointsAdded
}

/// `civ->mUnchanged`.
pub(crate) fn civc_civ_unchanged(c: usize) -> bool {
    civ(c).mUnchanged
}

// ---- player/tech lane, round 4: the class upgrades, the tech trees, the player's powers and the civilization's
// upgrade lists (the C++'s one-statement shims of EECivilizationBuilder.cpp and EEAEManager.cpp)

/// `player->mUnitClassUpgrades[kind]` (unchecked, as the C++'s `operator[]`).
fn class_upgrade(player: usize, kind: i64) -> usize {
    ee_sim::units::player_upgrades::at(player as u64, kind as c_long as usize) as usize
}

/// `gWorld.mPlayers[player]->mUnitClassUpgrades[kind]`.
pub(crate) fn civh_upgrade(player: i32, kind: i64) -> usize {
    class_upgrade(player_handle(player as i64), kind)
}

/// `player->mUnitClassUpgrades[kind]` (the civilization capture's).
pub(crate) fn civc_class_upgrade(player: usize, kind: i64) -> usize {
    class_upgrade(player, kind)
}

/// `gWorld.mPlayers[player]->GetTechTree()`.
fn player_tree(player: i32) -> usize {
    // SAFETY: a live player of the world's table
    unsafe { ee_sim::tech::tree_objects::player_tree(player_handle(player as i64)) }
}

/// `gWorld.mPlayers[player]->GetTechTree()->Research(kind, NULL)`.
pub(crate) fn civh_research(player: i32, kind: i64) {
    use ee_sim::tech::nodes::EeTechTreeOp;
    // SAFETY: the player's live tree
    unsafe { ee_sim::tech::tree_objects::walk_tree(player_tree(player), EeTechTreeOp::Research, kind as c_long as i32, 0, 0) };
}

/// `tree->Get(id)`.
fn tree_get(tree: usize, id: i64) -> usize {
    // SAFETY: a live tree
    unsafe { ee_sim::tech::tree_objects::tree_get(tree as *mut _, id as c_long) as usize }
}

/// `gWorld.mPlayers[player]->GetTechTree()->Get(kind)`.
pub(crate) fn civh_node(player: i32, kind: i64) -> usize {
    tree_get(player_tree(player), kind)
}

/// `node->ResearchNode(NULL)`.
pub(crate) fn civh_node_research(node: usize) {
    use ee_sim::tech::nodes::EeTechNodeOp;
    // SAFETY: a live node of a live tree
    unsafe { ee_sim::tech::tree_objects::walk_node(node, EeTechNodeOp::Research, 0, false, 0) };
}

/// `node->SetObsolete(true, false)`.
pub(crate) fn civh_node_set_obsolete(node: usize) {
    use ee_sim::tech::nodes::EeTechNodeOp;
    // SAFETY: as above
    unsafe { ee_sim::tech::tree_objects::walk_node(node, EeTechNodeOp::SetObsolete, 1, false, 0) };
}

/// `gWorld.mPlayers[player]->GetTechTree()->Get(t->GetUnitClass()->mTechnology)`.
pub(crate) fn aeh_tech_node(player: i32, t: usize) -> usize {
    let technology = with_unit(t, 0, |u| u.class().map_or(0, |c| c.technology()));
    civh_node(player, technology)
}

/// `gWorld.mPlayers[player]->...` through the player body (ee_sim player_body.rs `EE_UNITS_PLAYER_DO_*`).
fn player_do(player: i32, what: u32, a: u64, b: i64, f: f32) {
    // SAFETY: a live player of the world's table
    unsafe { ee_sim::units::player_body::game::ee_units_player_body(player_handle(player as i64) as *mut c_void, what, a, b, 0, f) };
}

/// `gWorld.mPlayers[player]->PulseSight(false, radius, kind)`.
pub(crate) fn civh_pulse_sight(player: i32, radius: f32, kind: i64) {
    player_do(player, ee_sim::units::player_body::EE_UNITS_PLAYER_DO_PULSE_SIGHT, kind as c_long as i64 as u64, 0, radius);
}

/// `gWorld.mPlayers[player]->EnableCamouflagePower()`.
pub(crate) fn civh_camouflage(player: i32) {
    player_do(player, ee_sim::units::player_body::EE_UNITS_PLAYER_DO_CAMOUFLAGE, 0, 1, 0.0);
}

/// `gWorld.PauseGame()`.
pub(crate) fn cbh_pause() -> i32 {
    crate::gamecpp::world::ee_gamecpp_world_pause(true) as i32
}

/// `gWorld.UnpauseGame()`.
pub(crate) fn cbh_unpause() -> i32 {
    crate::gamecpp::world::ee_gamecpp_world_pause(false) as i32
}

/// `gFreeCamera`.
pub(crate) fn cbh_free_camera() -> bool {
    crate::gamecpp::globals::get!(gFreeCamera)
}

/// `&player->mCivilization`.
pub(crate) fn cbh_player_civ(player: usize) -> usize {
    if player == 0 {
        return 0;
    }
    player + layout().player_civilization as usize
}

/// `&player->mCivilization` (the civilization capture's).
pub(crate) fn civc_civ(player: usize) -> usize {
    cbh_player_civ(player)
}

/// The civilization's `std::vector<EECivUpgrade>` `which` (0 `mPurchasedUpgrades`, 1 `mDeferredUpgrades`, after
/// its base `EeGameCivFields`): its begin and end.
fn civ_list(c: usize, which: usize) -> (usize, usize) {
    let word = core::mem::size_of::<usize>();
    let at = c + core::mem::size_of::<EeGameCivFields>().next_multiple_of(word) + which * 3 * word;
    // SAFETY: a live civilization's vector: its begin and end pointers are its first two words (MSVC, libstdc++,
    // libc++)
    unsafe { (*(at as *const usize), *((at + word) as *const usize)) }
}

fn civ_list_len(c: usize, which: usize) -> usize {
    let (begin, end) = civ_list(c, which);
    (end - begin) / core::mem::size_of::<u32>()
}

fn civ_list_at(c: usize, which: usize, i: usize) -> u32 {
    // SAFETY: the vector's element i (the caller's index is in range, as the C++'s was)
    unsafe { *((civ_list(c, which).0 + i * core::mem::size_of::<u32>()) as *const u32) }
}

/// `civ->GetPurchasedUpgrades().size()`.
pub(crate) fn cbh_civ_purchased_count(c: usize) -> u32 {
    civ_list_len(c, 0) as u32
}

/// `civ->GetPurchasedUpgrades()[i]`.
pub(crate) fn cbh_civ_purchased(c: usize, i: u32) -> u32 {
    civ_list_at(c, 0, i as usize)
}

/// `civ->mPurchasedUpgrades.size()`.
pub(crate) fn civc_civ_purchased_count(c: usize) -> usize {
    civ_list_len(c, 0)
}

/// `civ->mPurchasedUpgrades[i]`.
pub(crate) fn civc_civ_purchased_at(c: usize, i: usize) -> u32 {
    civ_list_at(c, 0, i)
}

/// `civ->mDeferredUpgrades.size()`.
pub(crate) fn civc_civ_deferred_count(c: usize) -> usize {
    civ_list_len(c, 1)
}

/// `civ->mDeferredUpgrades[i]`.
pub(crate) fn civc_civ_deferred_at(c: usize, i: usize) -> u32 {
    civ_list_at(c, 1, i)
}

/// `player->mCivilization.GetPurchasedUpgrades().size()`.
pub(crate) fn civm_purchased_count(player: usize) -> usize {
    civ_list_len(cbh_player_civ(player), 0)
}

/// `player->mCivilization.GetPurchasedUpgrades().data()`.
pub(crate) fn civm_purchased_data(player: usize) -> usize {
    civ_list(cbh_player_civ(player), 0).0
}
