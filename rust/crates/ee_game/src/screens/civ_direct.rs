//! The civilization builder's host calls Rust answers itself (player/tech lane, round 3): the calls of
//! [`Cpp`](crate::screens::game_cpp::Cpp) marked `=> path` in game_cpp.rs. Each was a one-statement C++ shim of
//! `Civilization Builder/*.cpp` over an object whose state is Rust's: the toolkit's forms, edit boxes, labels and
//! list boxes (the same inward calls their C++ forwarders make: `ee_ui::direct`). The generic logic over `Cpp`
//! (civ_host.rs and the rest) and its tests on the recording fake are unchanged; only the FFI implementation of
//! these calls is.
//!
//! As the C++ did, a NULL object answers 0 (the toolkit's direct calls answer 0 for an unknown object, where the C++
//! would have crashed).

use core::ffi::{c_ulong, c_void};

use ee_ui::direct::{self, Cpp as UiCpp, Kind};
use ee_ui::editobj::EeUiEditCall;
use ee_ui::formobj::EeUiFormCall;
use ee_ui::listobj::{EeUiList, EeUiListCall};
use ee_ui::managerobj::EeUiManagerCall;
use ee_ui::obj::EeUiOp;

use ee_sim::world::layout::layout;
use ee_sim::world::player_fields::EeUnitsPlayerSaved;
use ee_sim::world::{PlayerView, UnitView, WorldRead, WorldView};
use ee_sim::world_field;

use crate::screens::editor_direct::{edit_line, edit_set_text};
use crate::screens::game_cpp::{EeScreensShimOut, EeScreensShimText, EeScreensShimULongOut};
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
    unsafe { crate::screens::game_cpp::ee_screens_shim_out(o.0 as *mut c_void, bytes.as_ptr(), bytes.len()) };
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
    label_set_long(ui(label), number as core::ffi::c_long);
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
