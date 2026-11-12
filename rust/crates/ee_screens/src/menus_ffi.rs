//! `extern "C"` for the screens that are Rust's whole (`menus.rs`, `settings_form.rs`): `EEScreenEvent::ProcessEvent`'s
//! dispatch, the control-event table, the menus' state, and the host over the C++ calls (`ee_screenscpp_*`,
//! `UI Events/EEScreens.cpp`).
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_long, c_ulong, c_void};
use std::sync::Mutex;

use crate::menus::*;
use crate::single_player::EeScreensSinglePlayerSlot;

/// cbindgen:ignore
pub(crate) mod sys {
    use core::ffi::c_void;

    use crate::single_player::EeScreensSinglePlayerSlot;

    extern "C" {
        pub fn ee_screenscpp_call(
            form: *mut c_void,
            entity: *mut c_void,
            op: u32,
            a: u32,
            b: u32,
            c: u32,
            d: u32,
        ) -> u32;
        pub fn ee_screenscpp_text(form: *mut c_void, which: u32, arg: u32, out: *mut u8, capacity: usize) -> usize;
        pub fn ee_screenscpp_put_text(form: *mut c_void, which: u32, arg: u32, text: *const u8, length: usize) -> u32;
        pub fn ee_screenscpp_lobby_fill(
            slots: *const EeScreensSinglePlayerSlot,
            human: *const u8,
            human_length: usize,
            computer: *const u8,
            computer_length: usize,
        );
        // the settings form's (settings_form_ffi.rs)
        pub fn ee_screenscpp_selection(form: *mut c_void, control: u32, list: bool, out: *mut u32) -> i32;
        pub fn ee_screenscpp_settings(defaults: bool, out: *mut crate::settings_form::EeScreensSettingsRecord);
        pub fn ee_screenscpp_put_settings(record: *const crate::settings_form::EeScreensSettingsRecord);
        pub fn ee_screenscpp_modes(
            rasterizer: u32,
            screen: u32,
            out: *mut crate::settings::EeScreensVideoMode,
            capacity: usize,
        ) -> usize;
        pub fn ee_screenscpp_current_mode(screen: u32, out: *mut crate::settings::EeScreensVideoMode);
        pub fn ee_screenscpp_game_size(percent: u32, windowed: bool, width: *mut u32, height: *mut u32);
    }
}

/// The menus' state (the UI thread's; locked only around its own reads and writes).
static MENUS: Mutex<Menus> = Mutex::new(Menus {
    descriptions: [Vec::new(), Vec::new(), Vec::new(), Vec::new()],
    first_scenarios: [Vec::new(), Vec::new(), Vec::new(), Vec::new()],
});

/// The event's form and entity (`UIForm*`, `UIEntity*`).
pub(crate) struct Form(pub(crate) *mut c_void, pub(crate) *mut c_void);

impl Form {
    pub(crate) fn op(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        unsafe { sys::ee_screenscpp_call(self.0, self.1, op, a, b, c, d) }
    }
    /// A text, `None` when the host says it is missing (`usize::MAX`).
    pub(crate) fn get_checked(&mut self, which: u32, arg: u32) -> Option<Vec<u8>> {
        let mut out = vec![0u8; 256];
        let n = unsafe { sys::ee_screenscpp_text(self.0, which, arg, out.as_mut_ptr(), out.len()) };
        if n == usize::MAX {
            return None;
        }
        if n > out.len() {
            out.resize(n, 0);
            let m = unsafe { sys::ee_screenscpp_text(self.0, which, arg, out.as_mut_ptr(), out.len()) };
            out.truncate(m.min(n));
        } else {
            out.truncate(n);
        }
        Some(out)
    }
    pub(crate) fn get(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get_checked(which, arg).unwrap_or_default()
    }
    pub(crate) fn put(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        unsafe { sys::ee_screenscpp_put_text(self.0, which, arg, text.as_ptr(), text.len()) }
    }
}

impl crate::custom_campaign::CustomCampaignHost for Form {
    fn set_trigger(&mut self, form: u32, control: u32, trigger: u32, event: u32) {
        self.op(EE_SCREENS_OP_TRIGGER, form, control, trigger, event);
    }
    fn set_open_event(&mut self, form: u32, event: u32) {
        self.op(EE_SCREENS_OP_OPEN_EVENT, form, event, 0, 0);
    }
    fn set_close_event(&mut self, form: u32, event: u32) {
        self.op(EE_SCREENS_OP_CLOSE_EVENT, form, event, 0, 0);
    }
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32) -> u32 {
        self.op(op, a, b, c, 0)
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) -> u32 {
        self.put(which, arg, text)
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
    fn string_checked(&mut self, id: u32) -> Option<Vec<u8>> {
        self.get_checked(EE_SCREENS_TEXT_STRING_CHECKED, id)
    }
}

impl MenuHost for Form {
    fn push(&mut self, form: u32) {
        self.op(EE_SCREENS_OP_PUSH, form, 0, 0, 0);
    }
    fn pop(&mut self) {
        self.op(EE_SCREENS_OP_POP, 0, 0, 0, 0);
    }
    fn switch_to(&mut self, form: u32) {
        self.op(EE_SCREENS_OP_SWITCH, form, 0, 0, 0);
    }
    fn set_open_event(&mut self, form: u32, event: u32) {
        self.op(EE_SCREENS_OP_OPEN_EVENT, form, event, 0, 0);
    }
    fn show(&mut self, control: u32, mode: u32) {
        self.op(EE_SCREENS_OP_SHOW, control, mode, 0, 0);
    }
    fn set_label(&mut self, control: u32, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_LABEL, control, text);
    }
    fn edit_text(&mut self, control: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_EDIT, control)
    }
    fn set_edit_text(&mut self, control: u32, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_EDIT, control, text);
    }
    fn add_hot_key(&mut self, key: u32, m1: u32, m2: u32, event: u32) {
        self.op(EE_SCREENS_OP_HOT_KEY, key, m1, m2, event);
    }
    fn set_close_event(&mut self, form: u32, event: u32) {
        self.op(EE_SCREENS_OP_CLOSE_EVENT, form, event, 0, 0);
    }
    fn set_trigger(&mut self, form: u32, control: u32, trigger: u32, event: u32) {
        self.op(EE_SCREENS_OP_TRIGGER, form, control, trigger, event);
    }
    fn open_form(&mut self, form: u32) {
        self.op(EE_SCREENS_OP_OPEN_FORM, form, 0, 0, 0);
    }
    fn entity_index(&mut self) -> u32 {
        self.op(EE_SCREENS_OP_ENTITY_INDEX, 0, 0, 0, 0)
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
    fn string_checked(&mut self, id: u32) -> Option<Vec<u8>> {
        self.get_checked(EE_SCREENS_TEXT_STRING_CHECKED, id)
    }
    fn message(&mut self, text: &[u8], last: bool) {
        self.put(EE_SCREENS_TEXT_MESSAGE, last as u32, text);
    }
    fn set_label_lines(&mut self, form: u32, control: u32, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_LABEL_LINES, form << 16 | control, text);
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn set_campaign(&mut self, which: u32, text: &[u8]) {
        self.put(which, 0, text);
    }
    fn campaign_info(&mut self, name: &[u8]) -> Option<(Vec<u8>, Vec<u8>)> {
        if self.put(EE_SCREENS_TEXT_CAMPAIGN_LOAD, 0, name) == 0 {
            return None;
        }
        Some((self.get(EE_SCREENS_TEXT_CAMPAIGN_LOADED, 0), self.get(EE_SCREENS_TEXT_CAMPAIGN_CIV, 0)))
    }
    fn set_player_name(&mut self, name: &[u8]) {
        self.put(EE_SCREENS_TEXT_PLAYER, 0, name);
    }
    fn call_with(&mut self, op: u32, arg: u32) {
        self.op(op, arg, 0, 0, 0);
    }
    fn lobby_fill(&mut self, slots: &[EeScreensSinglePlayerSlot; 16], human: &[u8], computer: &[u8]) {
        unsafe {
            sys::ee_screenscpp_lobby_fill(
                slots.as_ptr(),
                human.as_ptr(),
                human.len(),
                computer.as_ptr(),
                computer.len(),
            )
        }
    }
    fn start_local_server(&mut self, human: &[u8]) {
        self.put(EE_SCREENS_TEXT_SERVER, 0, human);
    }
}

/// `EEScreenEvent::ProcessEvent`: the screen event `event` on `form` (`UIForm*`) from `entity`; `kErrorOK` (0).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_event(event: u32, form: *mut c_void, entity: *mut c_void) -> c_long {
    let mut host = Form(form, entity);
    if let Some(result) = crate::settings_form_ffi::dispatch(&mut host, event) {
        return result;
    }
    if crate::custom_campaign::dispatch(&mut host, event) {
        return 0;
    }
    if crate::save_load_ffi::dispatch(&mut host, event) {
        return 0;
    }
    if crate::scenario_screens_ffi::dispatch(&mut host, event) {
        return 0;
    }
    if crate::lobby_form_ffi::dispatch(&mut host, event) {
        return 0;
    }
    if let Some(result) = crate::main_game_options_ffi::dispatch(&mut host, event) {
        return result;
    }
    if let Some(result) = crate::main_game_ffi::dispatch(&mut host, event) {
        return result;
    }
    if let Some(result) = crate::chat_ffi::dispatch(&mut host, event) {
        return result;
    }
    if let Some(result) = crate::hot_keys_ffi::dispatch(&mut host, event) {
        return result;
    }
    if let Some(result) = crate::diplomacy_ffi::dispatch(&mut host, event) {
        return result;
    }
    if let Some(result) = crate::camera_ffi::dispatch(&mut host, event) {
        return result;
    }
    if let Some(result) = crate::civ_builder_ffi::dispatch(&mut host, event) {
        return result;
    }
    if let Some(result) = crate::debug_sound_ffi::dispatch(&mut host, event) {
        return result;
    }
    dispatch(&mut host, &MENUS, event);
    0
}

/// The menus' event code for a control event id, 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_menus_event_for(id: c_ulong) -> u32 {
    event_for_control(id)
}
