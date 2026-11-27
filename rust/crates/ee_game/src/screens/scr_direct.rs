//! The screens' toolkit calls Rust answers itself (LLE/UI lane, round 3): the calls of
//! [`Cpp`](crate::screens::game_cpp::Cpp) marked `= scr_direct::name` in game_cpp.rs. Each was a one-statement C++
//! shim of `UI Events/EEScreens.cpp` or `Civilization Builder/EECivilizationBuilder.cpp` that is only a chain of the
//! toolkit's non-virtual forwarders (UIForm, UIManager, UIControl, the label, button, combo box, list, file list,
//! background, hot key and entity group members, each one inward call on the Rust object) or a read of the game
//! UI manager's Rust record (`EEUIManager`'s `mMainGameForm`, `mInScenarioEditor`: ui_objects.rs). The Rust makes
//! the same inward calls through `ee_ui::direct`, as civ_direct.rs does; the logic over `Cpp` and its tests on the
//! recording fake are unchanged.
//!
//! An unknown or NULL object answers 0 (or nothing), where the C++ would have crashed.

// (`c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the calls' `u32` stay)
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_ulong, c_void};

use ee_render::ge::smallobj::{ee_ge_text_color, ee_ge_text_font, ee_ge_text_string, EeGeText};
use ee_ui::bgobj::{ee_ui_bg_call, EeUiBackgroundFields, EeUiBgCall};
use ee_ui::buttonobj::EeUiButtonCall;
use ee_ui::comboobj::{EeUiCombo, EeUiComboCall};
use ee_ui::controlobj::EeUiControlCall;
use ee_ui::direct::{self, Cpp as UiCpp, Kind};
use ee_ui::filelistobj::{ee_ui_file_list_call, EeUiFileList, EeUiFileListCall};
use ee_ui::formobj::EeUiFormCall;
use ee_ui::groupobj::EeUiEntityGroupCall;
use ee_ui::hotkeyobj::{ee_ui_hotkey_call, EeUiHotKeyCall, EeUiHotKeyFields};
use ee_ui::labelobj::EeUiLabelCall;
use ee_ui::listobj::{EeUiList, EeUiListCall};
use ee_ui::managerobj::EeUiManagerCall;

use crate::screens::game_cpp::{EeScreensShimOut, EeScreensShimText, EeScreensShimULongOut};
use crate::screens::hud_direct::{atoi, label_set_text, label_text, wide_bytes, CppWide};
use crate::screens::ui_ids::{EEChatControl, EEChatLogControl, EEFormID, EEMainGameControl};
use crate::screens::ui_objects;

fn ui(p: usize) -> UiCpp {
    p as UiCpp
}

/// The game's manager (`gUIManager`).
fn manager() -> UiCpp {
    direct::manager_of(core::ptr::null_mut())
}

/// `gUIManager->GetFormByID(id)`.
fn form_by_id(id: u32) -> UiCpp {
    direct::form_by_id(manager(), id as c_ulong)
}

/// `form->GetControl(index)`.
fn ctl(form: UiCpp, index: u32) -> UiCpp {
    direct::control(form, index as c_ulong)
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

/// `dynamic_cast<K*>(control) != NULL` for a toolkit class `kind` (one base chain: the cast moves no pointer).
fn is(kind: Kind, control: UiCpp) -> bool {
    !direct::find(kind, control as *const c_void).is_null()
}

/// `gUIManager->GetMainGameForm()` (the game manager's Rust record; NULL before the manager is made).
fn main_form() -> UiCpp {
    // SAFETY: the registered live manager's record
    ui_objects::manager().map_or(core::ptr::null_mut(), |m| unsafe { (*m).mMainGameForm })
}

/// A `GEText*` shell's Rust object (its only member, `mRust`), or NULL.
fn ge_text(text: isize) -> *mut EeGeText {
    if text == 0 {
        return core::ptr::null_mut();
    }
    // SAFETY: a live GEText shell: one pointer, the Rust object
    unsafe { *(text as *const *mut EeGeText) }
}

/// `text->SetFgColor(GEColor(red, green, blue))` on a `GEText*` shell.
fn text_set_fg(text: isize, red: f32, green: f32, blue: f32) {
    let t = ge_text(text);
    if t.is_null() {
        return;
    }
    // SAFETY: a live text; the colour is three floats (GEColor's layout)
    unsafe {
        let c = ee_ge_text_color(t, true);
        *c = red;
        *c.add(1) = green;
        *c.add(2) = blue;
    }
}

/// `list->GetCurrentSelection(*selection)` of a `UIListBox` (0 for another control).
fn list_selection(list: UiCpp, selection: EeScreensShimULongOut) -> i32 {
    let l = direct::find(Kind::List, list as *const c_void) as *const EeUiList;
    if l.is_null() {
        return 0;
    }
    // SAFETY: a live list box; the out number of the call
    unsafe { ee_ui::listobj::ee_ui_list_current_selection(l, selection.0) as i32 }
}

/// One of the file lists' methods on a `UIFileListBox` or `UIFileComboBox` (0 for another control).
fn file_list(control: UiCpp, call: EeUiFileListCall, a: isize) -> isize {
    let l = direct::find(Kind::FileList, control as *const c_void) as *const EeUiFileList;
    if l.is_null() {
        return 0;
    }
    // SAFETY: a live file list
    unsafe { ee_ui_file_list_call(l, call, a) }
}

/// One of `UIHotKey`'s methods.
fn hot_key(key: usize, call: EeUiHotKeyCall, a: isize, b: isize) {
    let r = direct::find(Kind::HotKey, key as *const c_void) as *mut EeUiHotKeyFields;
    if !r.is_null() {
        // SAFETY: a live hot key record
        unsafe { ee_ui_hotkey_call(r, call, a, b) };
    }
}

/// One of `UIBackground`'s methods with the float buffer `f`.
fn background_f(background: usize, call: EeUiBgCall, a: isize, f: &mut [f32; 4]) {
    let r = direct::find(Kind::Background, background as *const c_void) as *mut EeUiBackgroundFields;
    if !r.is_null() {
        // SAFETY: a live background record
        unsafe { ee_ui_bg_call(r, call, a, 0, f.as_mut_ptr()) };
    }
}

/// The background's rectangle (`GetRectangle()`), zero for an unknown one.
fn background_rect(background: usize) -> ee_ui::rect::UiRect {
    let r = direct::find(Kind::Background, background as *const c_void) as *const EeUiBackgroundFields;
    if r.is_null() {
        return ee_ui::rect::UiRect::default();
    }
    // SAFETY: a live background record
    unsafe { (*r).mRectangle }
}

// ---- forms and the manager

/// `form->GetControl(control)`.
pub(crate) fn sel_control(form: usize, control: u32) -> usize {
    ctl(ui(form), control) as usize
}

/// `form->GetControl(control)`.
pub(crate) fn form_control(form: usize, control: u32) -> usize {
    ctl(ui(form), control) as usize
}

/// `reinterpret_cast<EEUIFMainGame*>(form)->GetControl(control)`.
pub(crate) fn sci_main_control(form: usize, control: u32) -> usize {
    ctl(ui(form), control) as usize
}

/// `gUIManager->GetFormByID(form)->GetControl(control)`.
pub(crate) fn manager_form_control(form: u32, control: u32) -> usize {
    ctl(form_by_id(form), control) as usize
}

/// `gUIManager->GetFormByID(id)`.
pub(crate) fn uh_form(id: u32) -> usize {
    form_by_id(id) as usize
}

/// `gUIManager->GetFormByID(kFormScenarioIntro)`.
pub(crate) fn sci_form() -> usize {
    form_by_id(EEFormID::kFormScenarioIntro as u32) as usize
}

/// `gUIManager->GetFormByID(kFormCampaignScenarioSelect)` (the answer unused).
pub(crate) fn tp_csel_form() {
    form_by_id(EEFormID::kFormCampaignScenarioSelect as u32);
}

/// `form->GetBackgroundByID(id)`.
pub(crate) fn form_background(form: usize, id: u32) -> usize {
    direct::form_call(ui(form), EeUiFormCall::BackgroundById, id as c_ulong as isize, 0) as usize
}

/// `form->RemoveBackground(background)`.
pub(crate) fn form_remove_background(form: usize, background: usize) {
    direct::form_call(ui(form), EeUiFormCall::RemoveBackground, background as isize, 0);
}

/// `form->LockDrawBackground()`.
pub(crate) fn form_lock_background(form: usize) {
    direct::form_call(ui(form), EeUiFormCall::LockDraw, 1, 0);
}

/// `form->UnlockDrawBackground()`.
pub(crate) fn form_unlock_background(form: usize) {
    direct::form_call(ui(form), EeUiFormCall::LockDraw, 0, 0);
}

/// `gUIManager->GetFormByID(form)->GetMusicFileName()`.
pub(crate) fn form_music(form: u32, o: EeScreensShimOut) {
    let p = direct::form_call(form_by_id(form), EeUiFormCall::MusicFileName, 0, 0);
    // SAFETY: the form's NUL-terminated music file name (or NULL)
    out(o, &unsafe { direct::c_bytes(p as *const c_void) });
}

/// `gUIManager->GetFormByID(form)->SetMusicFileName(text)`.
pub(crate) fn form_set_music(form: u32, t: EeScreensShimText) {
    let name = direct::c_text(text(t));
    direct::form_call(form_by_id(form), EeUiFormCall::SetMusicFileName, name.as_ptr() as isize, 0);
}

/// `form->AddHotKey(*key, 0)`.
pub(crate) fn form_add_hot_key(form: usize, key: usize) {
    direct::form_call(ui(form), EeUiFormCall::AddHotKey, key as isize, 0);
}

/// `form->AddControl(control, group)`.
pub(crate) fn form_add_control(form: usize, control: usize, group: u32) {
    direct::form_call(ui(form), EeUiFormCall::AddControl, control as isize, group as c_ulong as isize);
}

/// `form->RemoveControl(control, flag)`.
pub(crate) fn form_remove_control(form: usize, control: usize, flag: u32) {
    direct::form_call(ui(form), EeUiFormCall::RemoveControl, control as isize, flag as c_ulong as isize);
}

/// `form->GetManager()->GetUITextureBitDepth()`.
pub(crate) fn form_ui_bit_depth(form: usize) -> u32 {
    direct::manager_call(direct::manager_of(ui(form)), EeUiManagerCall::TextureBitDepth, 0, 0, 0) as u32
}

/// `form->GetManager()->GetScreen()`.
pub(crate) fn form_screen(form: usize) -> u32 {
    direct::manager_call(direct::manager_of(ui(form)), EeUiManagerCall::Screen, 0, 0, 0) as u32
}

/// `form->GetRectangle().Contains(form->GetMouseLocation())` (edges included).
pub(crate) fn sc_ch_mouse_inside(form: usize) -> bool {
    let r = direct::form_call(ui(form), EeUiFormCall::Rectangle, 0, 0) as *const ee_ui::rect::UiRect;
    let m = direct::form_call(ui(form), EeUiFormCall::MouseLocation, 0, 0) as *const [f32; 2];
    if r.is_null() || m.is_null() {
        return false;
    }
    // SAFETY: the form's rectangle and mouse location (GERectangle's and GE2DPoint's layouts)
    let (r, [x, y]) = unsafe { (*r, *m) };
    x >= r.left && x <= r.right && y >= r.top && y <= r.bottom
}

/// `gUIManager->Pop()`.
pub(crate) fn manager_pop() {
    direct::manager_call(manager(), EeUiManagerCall::Pop, 0, 0, 0);
}

/// `gUIManager->SwapForm(gUIManager->GetFormByID(form))`.
pub(crate) fn manager_swap(form: u32) {
    let f = form_by_id(form);
    direct::manager_call(manager(), EeUiManagerCall::SwapForm, f as isize, 0, 0);
}

/// `gUIManager->SetMusicVolume(volume)`.
pub(crate) fn rc_ui_music_volume(volume: f32) {
    direct::manager_call_f(manager(), EeUiManagerCall::SetMusicVolume, 0, 0, 0, &mut [volume, 0.0, 0.0, 0.0]);
}

// ---- the game manager's record

/// `gUIManager->GetMainGameForm() != NULL`.
pub(crate) fn gv_main_form() -> bool {
    !main_form().is_null()
}

/// `gUIManager->GetMainGameForm()`.
pub(crate) fn sci_main_form() -> usize {
    main_form() as usize
}

/// `gUIManager->GetInScenarioEditor()`.
pub(crate) fn civf_in_scenario_editor() -> bool {
    // SAFETY: the registered live manager's record
    ui_objects::manager().is_some_and(|m| unsafe { (*m).mInScenarioEditor })
}

/// `gUIManager->GetMainGameForm()->GetEntityGroup(group)->Disable()`.
pub(crate) fn main_form_group_disable(group: u32) {
    let g = direct::form_call(main_form(), EeUiFormCall::EntityGroup, group as c_ulong as isize, 0);
    direct::group_call(g as UiCpp, EeUiEntityGroupCall::Disable, 0);
}

/// `gUIManager->GetMainGameForm()->GetBackgroundByID(id)->SetVisibility(visible)`.
pub(crate) fn main_form_background_visible(id: u32, visible: bool) {
    let b = direct::form_call(main_form(), EeUiFormCall::BackgroundById, id as c_ulong as isize, 0);
    direct::background_call(b as UiCpp, EeUiBgCall::SetVisibility, visible as isize, 0);
}

/// `gUIManager->GetMainGameForm()->GetControl(kMainGameLargeCivBuilderAlert)`.
pub(crate) fn cbh_main_alert_button() -> usize {
    ctl(main_form(), EEMainGameControl::kMainGameLargeCivBuilderAlert as u32) as usize
}

// ---- controls

/// `form->GetControl(control)->SetTriggerEvent(kControlTriggerRangeChanged, event)`.
pub(crate) fn control_set_range_trigger(form: usize, control: u32, event: usize) {
    let trigger = ee_ui::ids::UIControlTriggerID::kControlTriggerRangeChanged as isize;
    direct::control_call(ctl(ui(form), control), EeUiControlCall::SetTrigger, trigger, event as isize);
}

/// `form->GetControl(kChatTextEditBox)->SetTriggerEvent(kControlTriggerEnterPressed, event)`.
pub(crate) fn chat_set_enter_trigger(form: usize, event: usize) {
    let trigger = ee_ui::ids::UIControlTriggerID::kControlTriggerEnterPressed as isize;
    let c = ctl(ui(form), EEChatControl::kChatTextEditBox as u32);
    direct::control_call(c, EeUiControlCall::SetTrigger, trigger, event as isize);
}

/// `control->GetId()`.
pub(crate) fn control_id(control: usize) -> u32 {
    direct::with_fields(ui(control), |r| r.mId as u32).unwrap_or(0)
}

/// `control->GetIndex()`.
pub(crate) fn control_index(control: usize) -> u32 {
    direct::with_fields(ui(control), |r| r.mIndex as u32).unwrap_or(0)
}

/// `control->GetBorderThickness()`.
pub(crate) fn control_border(control: usize) -> u32 {
    direct::with_fields(ui(control), |r| r.mBorderThickness as u32).unwrap_or(0)
}

/// `form->GetControl(control)->GetStringTableID()`.
pub(crate) fn control_string_id(form: usize, control: u32) -> u32 {
    direct::with_fields(ctl(ui(form), control), |r| r.mStringTableID as u32).unwrap_or(0)
}

/// `form->GetControl(control)->SetStringTableID(id)`.
pub(crate) fn control_set_string_id(form: usize, control: u32, id: u32) {
    direct::with_fields(ctl(ui(form), control), |r| r.mStringTableID = id as c_ulong);
}

/// `dynamic_cast<UIEditBox*>(form->GetControl(control)) != NULL`.
pub(crate) fn sc_ce_is_edit(form: usize, control: u32) -> bool {
    is(Kind::Edit, ctl(ui(form), control))
}

/// `dynamic_cast<UITextLabel*>(form->GetControl(control)) != NULL`.
pub(crate) fn sc_ce_is_label(form: usize, control: u32) -> bool {
    is(Kind::Label, ctl(ui(form), control))
}

// ---- labels

/// `label->GetText(0)`.
pub(crate) fn label_text_out(label: usize, o: EeScreensShimOut) {
    out(o, &label_text(ui(label)));
}

/// `label->SetText(text)`.
pub(crate) fn label_set_text_in(label: usize, t: EeScreensShimText) {
    label_set_text(ui(label), text(t));
}

/// `label->AppendText(text)` (no class overrides it: `SetText(text, false)`).
fn label_append(label: UiCpp, bytes: &[u8]) {
    let t = direct::c_text(bytes);
    direct::label_call(label, EeUiLabelCall::SetText, t.as_ptr() as isize, 0);
}

/// `label->AppendText(text)`.
pub(crate) fn label_append_text(label: usize, t: EeScreensShimText) {
    label_append(ui(label), text(t));
}

/// `label->DeleteArray()`.
pub(crate) fn label_delete_array(label: usize) {
    direct::label_call(ui(label), EeUiLabelCall::DeleteArray, 0, 0);
}

/// `form->GetControl(control)` as a label: `GetGEText()->SetFgColor(GEColor(red, green, blue))`.
pub(crate) fn label_text_colour(form: usize, control: u32, red: f32, green: f32, blue: f32) {
    let t = direct::label_call(ctl(ui(form), control), EeUiLabelCall::Text, 0, 0).unwrap_or(0);
    text_set_fg(t, red, green, blue);
}

/// `form->GetControl(control)` as a combo box: `GetTextLabel()->GetGEText()->SetFgColor(GEColor(red, green, blue))`.
pub(crate) fn combo_text_colour(form: usize, control: u32, red: f32, green: f32, blue: f32) {
    let label = direct::combo_call(ctl(ui(form), control), EeUiComboCall::Part, 4, 0).unwrap_or(0);
    let t = direct::label_call(label as UiCpp, EeUiLabelCall::Text, 0, 0).unwrap_or(0);
    text_set_fg(t, red, green, blue);
}

/// `static_cast<UITextLabel*>(form->GetControl(control))->GetTextAsInt(0)` (`atoi` of line 0).
pub(crate) fn sc_ds_label_number(form: usize, control: u32) -> u32 {
    atoi(&label_text(ctl(ui(form), control))) as u32
}

/// `dynamic_cast<UITextLabel*>(gUIManager->GetFormByID(kFormDebugSound)->GetControlById(id))`.
pub(crate) fn debug_sound_label(id: u32) -> usize {
    let form = form_by_id(EEFormID::kFormDebugSound as u32);
    let c = direct::form_call(form, EeUiFormCall::ControlById, id as c_ulong as isize, 0) as UiCpp;
    if is(Kind::Label, c) {
        c as usize
    } else {
        0
    }
}

/// `gUIManager->GetFormByID(kFormChatLog)->GetControl(kChatLogChatLogTextBox)->AppendText(text)`.
pub(crate) fn tp_cl_append(t: EeScreensShimText) {
    let form = form_by_id(EEFormID::kFormChatLog as u32);
    label_append(ctl(form, EEChatLogControl::kChatLogChatLogTextBox as u32), text(t));
}

/// The predefined civilization form's control `index` (`EEScreensPdcsControl`).
fn pdcs_control(index: u32) -> UiCpp {
    ctl(form_by_id(EEFormID::kFormPredefinedCiv as u32), index)
}

/// `EEScreensPdcsControl(5)->SetText(text)` (a label).
pub(crate) fn tp_pdcs_label(t: EeScreensShimText) {
    label_set_text(pdcs_control(5), text(t));
}

// ---- buttons

/// `static_cast<UICheckBox*>(check)->GetTextLabel()`.
pub(crate) fn check_box_label(check: usize) -> usize {
    direct::button_call(ui(check), EeUiButtonCall::TextLabel, 0, 0).unwrap_or(0) as usize
}

/// `static_cast<UIButton*>(form->GetControl(control))->GetGEText()->GetFont()`.
pub(crate) fn button_font(form: usize, control: u32) -> usize {
    let t = ge_text(direct::button_call(ctl(ui(form), control), EeUiButtonCall::Text, 0, 0).unwrap_or(0));
    if t.is_null() {
        return 0;
    }
    // SAFETY: a live text
    unsafe { ee_ge_text_font(t) as usize }
}

/// `atoi(static_cast<UIButton*>(form->GetControl(control))->GetGEText()->GetText())`.
pub(crate) fn sc_ds_button_number(form: usize, control: u32) -> u32 {
    let t = ge_text(direct::button_call(ctl(ui(form), control), EeUiButtonCall::Text, 0, 0).unwrap_or(0));
    if t.is_null() {
        return 0;
    }
    // SAFETY: a live text and its string (a UWideString it holds)
    let bytes = unsafe { wide_bytes(ee_ge_text_string(t) as *const CppWide) };
    atoi(&bytes) as u32
}

/// `static_cast<UIBlinkingButton*>(button)->SetIsBlinking(value)`.
pub(crate) fn cbh_button_blink(button: usize, value: bool) {
    direct::button_call(ui(button), EeUiButtonCall::BlinkSetIsBlinking, value as isize, 0);
}

// ---- lists and combo boxes

/// `static_cast<UILBString*>(control)->GetCurrentSelection(*selection)`.
pub(crate) fn sel_list(control: usize, selection: EeScreensShimULongOut) -> i32 {
    list_selection(ui(control), selection)
}

/// `static_cast<UIComboBox*>(control)->GetCurrentSelection(*selection)`.
pub(crate) fn sel_combo(control: usize, selection: EeScreensShimULongOut) -> i32 {
    let c = direct::find(Kind::Combo, control as *const c_void) as *const EeUiCombo;
    if c.is_null() {
        return 0;
    }
    // SAFETY: a live combo box; the out number of the call
    unsafe { ee_ui::comboobj::ee_ui_combo_current_selection(c, selection.0) as i32 }
}

/// `EEScreensPdcsControl(3)->GetCurrentSelection(*selection)`.
pub(crate) fn pdcs_selection(selection: EeScreensShimULongOut) -> i32 {
    list_selection(pdcs_control(3), selection)
}

/// `EEScreensPdcsControl(3)->GetData(selection)`.
pub(crate) fn pdcs_line(selection: u32, o: EeScreensShimOut) {
    out(o, &direct::list_line(pdcs_control(3), selection as c_ulong as isize).unwrap_or_default());
}

/// `static_cast<EECBUIListBox*>(list)->SetAutoSelect(value)` (UIListBox's).
pub(crate) fn cb_list_set_auto_select(list: usize, value: bool) {
    direct::list_call(ui(list), EeUiListCall::SetAutoSelect, value as isize, 0);
}

/// `CBB(list)->UILBString::CopyData()` (the base's).
pub(crate) fn cbb_base_copy_data(list: usize) -> i64 {
    direct::list_call(ui(list), EeUiListCall::CopyData, 0, 0).unwrap_or(0) as core::ffi::c_long as i64
}

/// `static_cast<UIFileComboBox*>(form->GetControl(control))->InsertLine(text, index)` (UIComboBox's).
pub(crate) fn combo_insert_line(form: usize, control: u32, t: EeScreensShimText, index: u32) {
    let line = direct::c_text(text(t));
    direct::combo_call(ctl(ui(form), control), EeUiComboCall::InsertLine, line.as_ptr() as isize, index as c_ulong as isize);
}

/// `static_cast<UIFileComboBox*>(form->GetControl(control))->SetCurrentSelection(index)` (UIComboBox's).
pub(crate) fn combo_select(form: usize, control: u32, index: u32) {
    direct::combo_call(ctl(ui(form), control), EeUiComboCall::SetCurrentSelection, index as c_ulong as isize, 0);
}

// ---- file lists (UIFileListBox, UIFileComboBox: their own Rust file list)

/// `static_cast<UIFileComboBox*>(form->GetControl(control))->AddValidExtension(extension)`.
pub(crate) fn combo_add_extension(form: usize, control: u32, extension: EeScreensShimText) {
    let e = direct::c_text(text(extension));
    file_list(ctl(ui(form), control), EeUiFileListCall::AddExtension, e.as_ptr() as isize);
}

/// `static_cast<UIFileComboBox*>(form->GetControl(control))->SetSearchPaths(path)`.
pub(crate) fn combo_set_paths(form: usize, control: u32, path: usize) {
    file_list(ctl(ui(form), control), EeUiFileListCall::SetSearchPaths, path as isize);
}

/// `static_cast<UIFileComboBox*>(form->GetControl(control))->CreateEntries(1)`.
pub(crate) fn combo_create(form: usize, control: u32) {
    file_list(ctl(ui(form), control), EeUiFileListCall::CreateEntries, 1);
}

/// `static_cast<UIFileListBox*>(list)->AddValidExtension(extension)`.
pub(crate) fn file_list_add_extension(list: usize, extension: EeScreensShimText) {
    let e = direct::c_text(text(extension));
    file_list(ui(list), EeUiFileListCall::AddExtension, e.as_ptr() as isize);
}

/// `static_cast<UIFileListBox*>(list)->SetSearchPaths(path)`.
pub(crate) fn file_list_set_paths(list: usize, path: usize) {
    file_list(ui(list), EeUiFileListCall::SetSearchPaths, path as isize);
}

/// `static_cast<UIFileListBox*>(list)->CreateEntries(1)`.
pub(crate) fn file_list_create(list: usize) {
    file_list(ui(list), EeUiFileListCall::CreateEntries, 1);
}

/// `dynamic_cast<UIFileListBox*>(control)` (a file combo box is no list: its file list's C++ object is a combo).
pub(crate) fn cbh_as_file_list(control: usize) -> usize {
    if is(Kind::FileList, ui(control)) && is(Kind::List, ui(control)) {
        control
    } else {
        0
    }
}

/// `list->AddValidExtension(UString(".civ"))`.
pub(crate) fn cbh_fl_add_extension(list: usize) {
    file_list(ui(list), EeUiFileListCall::AddExtension, c".civ".as_ptr() as isize);
}

/// `list->CreateEntries(1)`.
pub(crate) fn cbh_fl_create_entries(list: usize) {
    file_list(ui(list), EeUiFileListCall::CreateEntries, 1);
}

/// `list->GetCurrentSelection(*index)`.
pub(crate) fn cbh_fl_selection(list: usize, index: EeScreensShimULongOut) -> i32 {
    list_selection(ui(list), index)
}

/// `list->GetData(line)`.
pub(crate) fn cbh_fl_line(list: usize, line: u32, o: EeScreensShimOut) {
    out(o, &direct::list_line(ui(list), line as c_ulong as isize).unwrap_or_default());
}

/// `list->DeleteSelectedFile()`.
pub(crate) fn cbh_fl_delete(list: usize) -> i32 {
    file_list(ui(list), EeUiFileListCall::DeleteSelected, 0) as i32
}

// ---- hot keys

/// `key->SetKey(code)`.
pub(crate) fn hot_key_set_key(key: usize, code: u32) {
    hot_key(key, EeUiHotKeyCall::SetKey, code as c_ulong as isize, 0);
}

/// `key->SetJustPressedEvent(event)`.
pub(crate) fn hot_key_set_just_pressed(key: usize, event: usize) {
    hot_key(key, EeUiHotKeyCall::SetEvent, 1, event as isize);
}

// ---- backgrounds

/// `background->GetRectangle().mLeft`.
pub(crate) fn background_left(background: usize) -> f32 {
    background_rect(background).left
}

/// `background->GetRectangle().mTop`.
pub(crate) fn background_top(background: usize) -> f32 {
    background_rect(background).top
}

/// `background->GetRectangle().mRight`.
pub(crate) fn background_right(background: usize) -> f32 {
    background_rect(background).right
}

/// `background->SetRectangle(GERectangle(left, top, right, bottom))` (the floats as UIF4 lays a rectangle out:
/// bottom, left, right, top).
pub(crate) fn background_set_rect(background: usize, left: f32, top: f32, right: f32, bottom: f32) {
    background_f(background, EeUiBgCall::SetRectangle, 0, &mut [bottom, left, right, top]);
}

/// `background->SetVisibility(visible)`.
pub(crate) fn background_set_visible(background: usize, visible: bool) {
    direct::background_call(ui(background), EeUiBgCall::SetVisibility, visible as isize, 0);
}

// ---- the predefined civilization form's controls

/// `EEScreensPdcsControl(index)`.
pub(crate) fn sc_pdcs_control(index: u32) -> usize {
    pdcs_control(index) as usize
}

/// `EEScreensPdcsControl(5)->SetDisplayMode(kDisplayModeHidden)` (UIControl's, not virtual).
pub(crate) fn sc_pdcs_hide_label() {
    let hidden = ee_ui::ids::UIDisplayMode::kDisplayModeHidden as isize;
    direct::control_call(pdcs_control(5), EeUiControlCall::SetDisplayMode, hidden, 0);
}
