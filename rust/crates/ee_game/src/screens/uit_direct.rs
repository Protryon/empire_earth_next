//! The UI thread's host calls Rust answers itself (LLE/UI lane, round 3): the calls of
//! [`Cpp`](crate::screens::game_cpp::Cpp) marked `= uit_direct::name` in game_cpp.rs. Each was a one-statement C++
//! shim of `EEUserInterface.cpp` whose statement was a chain of the toolkit's non-virtual forwarders
//! (`GetForm()->GetManager()->GetScreen()`, `gUIManager->Push(gUIManager->GetFormByID(id))`): every link one inward
//! call on a Rust object, which [`ee_ui::direct`] makes from the C++ pointer. The logic over `Cpp` and its tests on
//! the recording fake are unchanged; only the FFI implementation of these calls is.
//!
//! As the toolkit's direct calls do, an unknown or NULL object answers 0 (the C++ would have crashed).

// (`c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the calls' numbers stay)
#![allow(clippy::unnecessary_cast)]
#![cfg_attr(test, allow(dead_code))]

use core::ffi::c_ulong;

use ee_ui::bgobj::EeUiBackgroundFields;
use ee_ui::direct::{self, Cpp, Kind};
use ee_ui::formobj::EeUiFormCall as F;
use ee_ui::managerobj::EeUiManagerCall as M;

fn ui(p: usize) -> Cpp {
    p as Cpp
}

/// The game's manager (`gUIManager`).
fn manager() -> Cpp {
    direct::manager_of(core::ptr::null_mut())
}

fn mgr(m: Cpp, call: M, a: isize) -> isize {
    direct::manager_call(m, call, a, 0, 0)
}

/// `gUIManager->GetFormByID(id)`.
fn form_by_id(id: u32) -> Cpp {
    direct::form_by_id(manager(), id as c_ulong)
}

/// The background's form (`GetForm()`), NULL for an unknown background.
fn bg_form(bg: usize) -> Cpp {
    let rec = direct::find(Kind::Background, ui(bg)) as *const EeUiBackgroundFields;
    if rec.is_null() {
        return core::ptr::null_mut();
    }
    // SAFETY: the registry's record of a live background
    unsafe { (*rec).mForm }
}

/// The control's form (`GetForm()`), NULL for an unknown control.
fn ctl_form(c: usize) -> Cpp {
    direct::with_fields(ui(c), |r| r.mForm).unwrap_or(core::ptr::null_mut())
}

/// The form's mouse location (`GetMouseLocation()`: the address of the Rust form's point), (0, 0) for none.
fn form_mouse(form: Cpp) -> (f32, f32) {
    let p = direct::form_call(form, F::MouseLocation, 0, 0) as *const f32;
    if p.is_null() {
        return (0.0, 0.0);
    }
    // SAFETY: the Rust form's GE2DPoint (two floats), live with the form
    unsafe { (*p, *p.add(1)) }
}

// ---- the generic background's (ee_screens backgrounds_calls.rs)

/// `GetForm()->GetManager()->GetRasterizer()`.
pub(crate) fn gb_rasterizer(bg: usize) -> usize {
    mgr(direct::manager_of(bg_form(bg)), M::Rasterizer, 0) as usize
}
/// `GetForm()->GetManager()->GetScreen()`.
pub(crate) fn gb_screen(bg: usize) -> u32 {
    mgr(direct::manager_of(bg_form(bg)), M::Screen, 0) as c_ulong as u32
}
/// `(float)GetForm()->GetDesignedWidth()`.
pub(crate) fn gb_designed_width(bg: usize) -> f32 {
    direct::form_call(bg_form(bg), F::DesignedWidth, 0, 0) as c_ulong as f32
}
/// `(float)GetForm()->GetDesignedHeight()`.
pub(crate) fn gb_designed_height(bg: usize) -> f32 {
    direct::form_call(bg_form(bg), F::DesignedHeight, 0, 0) as c_ulong as f32
}

// ---- a control's form

/// `GetForm()->UpdateHelpString()` (the improvement button's, the portrait's).
fn update_help(c: usize) {
    direct::form_call(ctl_form(c), F::UpdateHelpString, 0, 0);
}
pub(crate) fn upb_update_help(button: usize) {
    update_help(button)
}
pub(crate) fn pt_update_help(p: usize) {
    update_help(p)
}
/// The mini map's `GetForm()->GetMouseLocation().GetX()`.
pub(crate) fn mm_form_mouse_x(map: usize) -> f32 {
    form_mouse(ctl_form(map)).0
}
pub(crate) fn mm_form_mouse_y(map: usize) -> f32 {
    form_mouse(ctl_form(map)).1
}
/// `GetForm()->GetControlById(id)`.
pub(crate) fn sh_form_control(control: usize, id: i64) -> usize {
    direct::form_call(ctl_form(control), F::ControlById, id as c_ulong as isize, 0) as usize
}

// ---- the forms and managers

pub(crate) fn um_current_form(manager: usize) -> usize {
    mgr(ui(manager), M::CurrentForm, 0) as usize
}
pub(crate) fn um_form_by_id(manager: usize, id: u32) -> usize {
    direct::form_by_id(ui(manager), id as c_ulong) as usize
}
pub(crate) fn form_keyboard_captured(form: usize) -> bool {
    direct::form_call(ui(form), F::KeyboardCaptured, 0, 0) != 0
}
pub(crate) fn form_manager(form: usize) -> usize {
    direct::form_call(ui(form), F::Manager, 0, 0) as usize
}

// ---- gUIManager's

pub(crate) fn mm_redraw_all() {
    mgr(manager(), M::RedrawAll, 0);
}
pub(crate) fn manager_lock_for_copy() {
    mgr(manager(), M::LockForCopy, 0);
}
pub(crate) fn manager_unlock_for_copy() {
    mgr(manager(), M::LockForCopy, 1);
}
/// `IsFormVisible(GetFormByID(id))`.
pub(crate) fn manager_form_visible(form: u32) -> bool {
    mgr(manager(), M::IsFormVisible, form_by_id(form) as isize) != 0
}
/// `GetFormByID(id)->SetFormOpenEvent(event)` (the Rust form's event 0).
pub(crate) fn manager_form_set_open_event(form: u32, event: usize) {
    direct::form_call(form_by_id(form), F::SetEvent, 0, event as isize);
}
/// `GetFormByID(id)->SetFormCloseEvent(event)` (the Rust form's event 1).
pub(crate) fn manager_form_set_close_event(form: u32, event: usize) {
    direct::form_call(form_by_id(form), F::SetEvent, 1, event as isize);
}
pub(crate) fn manager_pop_to(form: u32) {
    mgr(manager(), M::PopTo, form_by_id(form) as isize);
}
pub(crate) fn manager_push(form: u32) {
    mgr(manager(), M::Push, form_by_id(form) as isize);
}
pub(crate) fn manager_form(form: u32) -> usize {
    form_by_id(form) as usize
}

// ---- the game classes' Rust records (hud_fields.rs and the others: each class's second base, at the offset C++
// gives it: `ee_screenscpp_rec_offset`)

/// The records' kinds: `ee_screenscpp_rec_offset`'s switch.
#[derive(Clone, Copy)]
#[repr(u32)]
enum Rec {
    Background = 0,
    Portrait = 1,
    MiniMap = 2,
    Upgrade = 3,
    Attribute = 4,
    StatusBar = 5,
    StatusButton = 6,
    StatusButtonObjects = 7,
    UiManager = 8,
}

extern "C" {
    fn ee_screenscpp_rec_offset(kind: u32) -> isize;
}

/// The record `T` of kind `kind` in the C++ object `o` (a pointer to the class `ee_screenscpp_rec_offset` names).
fn rec<T>(o: usize, kind: Rec) -> *mut T {
    // SAFETY: a plain C++ function of a constant answer
    (o as isize + unsafe { ee_screenscpp_rec_offset(kind as u32) }) as *mut T
}

/// `o`'s record of kind `$kind` read or written: `$e` over `r`.
macro_rules! with_rec {
    ($o:expr, $kind:ident, $t:ty, |$r:ident| $e:expr) => {{
        // SAFETY: the shim's contract: `o` a live object of the kind's class, whose record is at the offset
        let $r: &mut $t = unsafe { &mut *rec::<$t>($o, Rec::$kind) };
        $e
    }};
}

use crate::screens::hud_fields::{
    EeScreensAttributeFields as AttF, EeScreensBackgroundFields as BgF, EeScreensMiniMapFields as MmF,
    EeScreensPortraitFields as PtF, EeScreensStatusBarFields as SbaF, EeScreensStatusButtonObjects as SboF,
    EeScreensUpgradeFields as UpF,
};
use crate::screens::main_form_fields::EeScreensStatusButtonFields as SbbF;
use crate::screens::ui_objects::EeScreensUiManagerFields as UmF;

pub(crate) fn gb_set_opacity(bg: usize, opacity: f32) {
    with_rec!(bg, Background, BgF, |r| r.mOpacity = opacity)
}
pub(crate) fn gb_set_flags(bg: usize, flags: u32) {
    with_rec!(bg, Background, BgF, |r| r.mFrameFlags = flags as c_ulong)
}
pub(crate) fn gb_set_scale_x(bg: usize, scale: f32) {
    with_rec!(bg, Background, BgF, |r| r.mScaleX = scale)
}
pub(crate) fn gb_set_scale_y(bg: usize, scale: f32) {
    with_rec!(bg, Background, BgF, |r| r.mScaleY = scale)
}
pub(crate) fn gb_opacity(bg: usize) -> f32 {
    with_rec!(bg, Background, BgF, |r| r.mOpacity)
}

pub(crate) fn pt_level(p: usize) -> u64 {
    with_rec!(p, Portrait, PtF, |r| r.mLevel as u64)
}
pub(crate) fn pt_first_bar(p: usize) -> bool {
    with_rec!(p, Portrait, PtF, |r| r.mShowFirstBar)
}
pub(crate) fn pt_second_bar(p: usize) -> bool {
    with_rec!(p, Portrait, PtF, |r| r.mShowSecondBar)
}
pub(crate) fn pt_set_level(p: usize, v: u64) {
    with_rec!(p, Portrait, PtF, |r| r.mLevel = v as c_ulong)
}
pub(crate) fn pt_set_first_bar(p: usize, v: bool) {
    with_rec!(p, Portrait, PtF, |r| r.mShowFirstBar = v)
}
pub(crate) fn pt_set_second_bar(p: usize, v: bool) {
    with_rec!(p, Portrait, PtF, |r| r.mShowSecondBar = v)
}
pub(crate) fn pt_set_show_level(p: usize, v: bool) {
    with_rec!(p, Portrait, PtF, |r| r.mShowLevel = v)
}

/// The attribute's queued count (its improvement button's).
pub(crate) fn att_queued(attr: usize) -> i64 {
    with_rec!(attr, Upgrade, UpF, |r| r.mNumQueued as i64)
}
pub(crate) fn att_set_show(attr: usize, show: bool) {
    with_rec!(attr, Attribute, AttF, |r| r.mShowLevel = show)
}
pub(crate) fn att_show(attr: usize) -> bool {
    with_rec!(attr, Attribute, AttF, |r| r.mShowLevel)
}

pub(crate) fn sba_set_pressed(bar: usize, pressed: bool) {
    with_rec!(bar, StatusBar, SbaF, |r| r.mPressed = pressed)
}
pub(crate) fn sba_set_show_count(bar: usize, show: bool) {
    with_rec!(bar, StatusBar, SbaF, |r| r.mShowCount = show)
}
/// The status bar's queued count (its improvement button's).
pub(crate) fn sba_queued(bar: usize) -> i64 {
    with_rec!(bar, Upgrade, UpF, |r| r.mNumQueued as i64)
}
pub(crate) fn sba_copy_enabled(bar: usize) {
    with_rec!(bar, StatusBar, SbaF, |r| r.mCopiedEnabled = r.mEnabled)
}
pub(crate) fn sba_copied_enabled(bar: usize) -> bool {
    with_rec!(bar, StatusBar, SbaF, |r| r.mCopiedEnabled)
}
pub(crate) fn sba_show_count(bar: usize) -> bool {
    with_rec!(bar, StatusBar, SbaF, |r| r.mShowCount)
}

pub(crate) fn sbt_object_clear(button: usize) {
    with_rec!(button, StatusButtonObjects, SboF, |r| r.mButtonObject = core::ptr::null_mut())
}
pub(crate) fn sbt_set_button(button: usize, index: u32) {
    with_rec!(button, StatusButton, SbbF, |r| r.mButton = index as c_ulong)
}
pub(crate) fn sbt_set_unit_id(button: usize, id: u32) {
    with_rec!(button, StatusButton, SbbF, |r| r.mUnitID = id as c_ulong)
}
pub(crate) fn sbt_draw_icon(button: usize) -> bool {
    with_rec!(button, StatusButton, SbbF, |r| r.mDrawIcon)
}
pub(crate) fn sbt_available(button: usize) -> bool {
    with_rec!(button, StatusButton, SbbF, |r| r.mAvailable)
}
pub(crate) fn sbt_show_count(button: usize) -> bool {
    with_rec!(button, StatusButton, SbbF, |r| r.mShowCount)
}
pub(crate) fn sbm_is_command(button: usize) -> bool {
    with_rec!(button, StatusButton, SbbF, |r| r.mIsCommand)
}

pub(crate) fn mm_set_show_flare(map: usize, show: bool) {
    with_rec!(map, MiniMap, MmF, |r| r.mShowFlare = show)
}
pub(crate) fn mm_set_flare_blink(map: usize, blink: bool) {
    with_rec!(map, MiniMap, MmF, |r| r.mFlareBlink = blink)
}
pub(crate) fn mm_show_flare(map: usize) -> bool {
    with_rec!(map, MiniMap, MmF, |r| r.mShowFlare)
}
pub(crate) fn mm_flare_blink(map: usize) -> bool {
    with_rec!(map, MiniMap, MmF, |r| r.mFlareBlink)
}

pub(crate) fn um_clear_u138(manager: usize) {
    with_rec!(manager, UiManager, UmF, |r| r.mU138 = 0)
}
pub(crate) fn um_set_in_editor(manager: usize) {
    with_rec!(manager, UiManager, UmF, |r| r.mInScenarioEditor = true)
}
pub(crate) fn um_in_editor(manager: usize) -> bool {
    with_rec!(manager, UiManager, UmF, |r| r.mInScenarioEditor)
}
pub(crate) fn um_clear_editor(manager: usize) {
    with_rec!(manager, UiManager, UmF, |r| r.mScenarioEditorForm = core::ptr::null_mut())
}
