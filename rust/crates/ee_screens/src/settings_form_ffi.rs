//! The settings form's host over the C++ calls (`settings_form.rs`; `UI Events/EEScreens.cpp`), and its control-event
//! table.
#![allow(clippy::missing_safety_doc)]
// `c_ulong` is `u32` on Windows and `u64` on LP64: the casts between it and the host's `u32` stay
#![allow(clippy::unnecessary_cast)]

use core::ffi::{c_long, c_ulong};
use std::sync::Mutex;

use crate::menus::*;
use crate::menus_ffi::{sys, Form};
use crate::settings::EeScreensVideoMode;
use crate::settings_form::*;

/// The form's state (the UI thread's; locked only around its own reads and writes).
static SETTINGS: Mutex<SettingsForm> = Mutex::new(SettingsForm::new());

/// One of the settings form's events (`EEScreenEvent::ProcessEvent`): its result; none for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    if !is_settings_event(event) {
        return None;
    }
    crate::settings_form::dispatch(host, &SETTINGS, event)
}

impl Form {
    fn selection(&mut self, control: u32, list: bool) -> (c_long, c_ulong) {
        let mut out = 0u32;
        let result = unsafe { sys::ee_screenscpp_selection(self.0, control, list, &mut out) };
        (result as c_long, out as c_ulong)
    }
}

impl SettingsHost for Form {
    fn group(&mut self, form: Option<u32>, group: u32, enable: bool) {
        self.op(EE_SCREENS_OP_GROUP, group, enable as u32, form.map_or(0, |f| f + 1), 0);
    }
    fn background(&mut self, form: Option<u32>, id: u32, visible: bool) {
        self.op(EE_SCREENS_OP_BACKGROUND, id, visible as u32, form.map_or(0, |f| f + 1), 0);
    }
    fn checkbox(&mut self, control: u32) -> bool {
        self.op(EE_SCREENS_OP_CHECKBOX, control, 0, 0, 0) != 0
    }
    fn set_checkbox(&mut self, control: u32, checked: bool) {
        self.op(EE_SCREENS_OP_SET_CHECKBOX, control, checked as u32, 0, 0);
    }
    fn radio(&mut self, control: u32) -> bool {
        self.op(EE_SCREENS_OP_RADIO, control, 0, 0, 0) != 0
    }
    fn check_radio(&mut self, control: u32) {
        self.op(EE_SCREENS_OP_CHECK_RADIO, control, 0, 0, 0);
    }
    fn range(&mut self, control: u32) -> c_long {
        self.op(EE_SCREENS_OP_RANGE, control, 0, 0, 0) as c_long
    }
    fn set_range(&mut self, control: u32, value: c_long) {
        self.op(EE_SCREENS_OP_SET_RANGE, control, value as u32, 0, 0);
    }
    fn entity_range(&mut self) -> c_long {
        self.op(EE_SCREENS_OP_ENTITY_RANGE, 0, 0, 0, 0) as c_long
    }
    fn combo_selection(&mut self, control: u32) -> (c_long, c_ulong) {
        self.selection(control, false)
    }
    fn list_selection(&mut self, control: u32) -> (c_long, c_ulong) {
        self.selection(control, true)
    }
    fn combo_select(&mut self, control: u32, line: c_ulong) {
        self.op(EE_SCREENS_OP_COMBO_SELECT, control, line as u32, 0, 0);
    }
    fn list_select(&mut self, control: u32, line: c_ulong) {
        self.op(EE_SCREENS_OP_LIST_SELECT, control, line as u32, 0, 0);
    }
    fn combo_clear(&mut self, control: u32) {
        self.op(EE_SCREENS_OP_COMBO_CLEAR, control, 0, 0, 0);
    }
    fn list_clear(&mut self, control: u32) {
        self.op(EE_SCREENS_OP_LIST_CLEAR, control, 0, 0, 0);
    }
    fn combo_append(&mut self, control: u32, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_COMBO_APPEND, control, text);
    }
    fn combo_insert(&mut self, control: u32, line: u32, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_COMBO_INSERT, control << 16 | line, text);
    }
    fn list_append(&mut self, control: u32, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_LIST_APPEND, control, text);
    }
    fn combo_data(&mut self, control: u32, line: c_ulong) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_COMBO_DATA, control << 16 | (line as u32 & 0xffff))
    }
    fn list_data(&mut self, control: u32, line: c_ulong) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_LIST_DATA, control << 16 | (line as u32 & 0xffff))
    }
    fn release_mouse(&mut self, control: u32) {
        self.op(EE_SCREENS_OP_RELEASE_MOUSE, control, 0, 0, 0);
    }
    fn show(&mut self, control: u32, mode: u32) {
        self.op(EE_SCREENS_OP_SHOW, control, mode, 0, 0);
    }
    fn label_number(&mut self, control: u32, value: c_long) {
        self.op(EE_SCREENS_OP_LABEL_NUMBER, control, value as u32, 0, 0);
    }
    fn entity_id(&mut self) -> Option<c_ulong> {
        if self.1.is_null() {
            return None;
        }
        Some(self.op(EE_SCREENS_OP_ENTITY_ID, 0, 0, 0, 0) as c_ulong)
    }
    fn control_id(&mut self, control: u32) -> c_ulong {
        self.op(EE_SCREENS_OP_CONTROL_ID, control, 0, 0, 0) as c_ulong
    }
    fn trigger(&mut self, control: u32, trigger: u32, event: u32) {
        self.op(EE_SCREENS_OP_TRIGGER_HERE, control, trigger, event, 0);
    }
    fn slider_label(&mut self, slider: u32, label: u32, off: Option<u32>) {
        self.op(EE_SCREENS_OP_SLIDER_LABEL, slider, label, off.unwrap_or(0), 0);
    }
    fn music_file(&mut self) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_MUSIC_FILE, 0)
    }
    fn set_music_file(&mut self, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_MUSIC_FILE, 0, text);
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
    fn question(&mut self, text: &[u8]) {
        self.put(EE_SCREENS_TEXT_QUESTION, 0, text);
    }
    fn pop(&mut self) {
        self.op(EE_SCREENS_OP_POP, 0, 0, 0, 0);
    }
    fn call(&mut self, op: u32) {
        self.op(op, 0, 0, 0, 0);
    }
    fn call_with(&mut self, op: u32, a: u32, b: u32) -> u32 {
        self.op(op, a, b, 0, 0)
    }
    fn settings(&mut self, defaults: bool) -> EeScreensSettingsRecord {
        let mut out = EeScreensSettingsRecord::default();
        unsafe { sys::ee_screenscpp_settings(defaults, &mut out) };
        out
    }
    fn put_settings(&mut self, settings: &EeScreensSettingsRecord) {
        unsafe { sys::ee_screenscpp_put_settings(settings) }
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
    fn modes(&mut self, rasterizer: c_ulong, screen: c_ulong) -> Vec<EeScreensVideoMode> {
        let mut out = vec![EeScreensVideoMode::default(); 64];
        let n = unsafe { sys::ee_screenscpp_modes(rasterizer as u32, screen as u32, out.as_mut_ptr(), out.len()) };
        if n > out.len() {
            out.resize(n, EeScreensVideoMode::default());
            unsafe { sys::ee_screenscpp_modes(rasterizer as u32, screen as u32, out.as_mut_ptr(), out.len()) };
        }
        out.truncate(n);
        out
    }
    fn current_mode(&mut self, screen: c_ulong) -> EeScreensVideoMode {
        let mut out = EeScreensVideoMode::default();
        unsafe { sys::ee_screenscpp_current_mode(screen as u32, &mut out) };
        out
    }
    fn game_size(&mut self, percent: c_ulong, windowed: bool) -> (c_ulong, c_ulong) {
        let (mut w, mut h) = (0u32, 0u32);
        unsafe { sys::ee_screenscpp_game_size(percent as u32, windowed, &mut w, &mut h) };
        (w as c_ulong, h as c_ulong)
    }
}

/// The settings form's event code for a control event id, 0 for another form's.
#[no_mangle]
pub extern "C" fn ee_screens_settings_event_for(id: c_ulong) -> u32 {
    crate::settings_form::event_for_control(id)
}
