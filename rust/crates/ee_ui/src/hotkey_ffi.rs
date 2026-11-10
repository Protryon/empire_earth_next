//! `extern "C"` for the hot keys (`User Interface/UIHotKey.cpp`) and the form's keyboard pass (`UIForm.cpp`): the
//! hot key, its keys and events and the form are C++ objects, asked through `ee_uicpp_hotkey_*` and
//! `ee_uicpp_form_keyboard_*` on each step, never cached.
#![allow(clippy::missing_safety_doc)]

use core::ffi::{c_char, c_int, c_long, c_ulong, c_void};

use crate::hotkey::{self, FireHost, HotKey, HotKeyEvent, KeyboardHost};

/// cbindgen:ignore
mod sys {
    use core::ffi::{c_int, c_long, c_ulong, c_void};

    extern "C" {
        // value: 0 mEnabled, 1 mModifierState, 2 gISKeyboard->GetModifierState(),
        // 3 ComputeModifierState(the key's GetIndex()); 4 + i: whether modifier i is set
        pub fn ee_uicpp_hotkey_value(key: *mut c_void, which: c_int) -> c_ulong;
        pub fn ee_uicpp_hotkey_modifier_value(key: *mut c_void, i: c_int) -> c_long;
        pub fn ee_uicpp_hotkey_has_event(key: *mut c_void, event: c_int) -> bool;
        pub fn ee_uicpp_hotkey_key_test(key: *mut c_void, event: c_int) -> bool;
        pub fn ee_uicpp_hotkey_process(key: *mut c_void, event: c_int);
        // value: 0 captured, 1 its GetIMEEnabled, 2 mHotKeys.size, 3 + g: mHotKeys[g].size, as an argument;
        // 4 mTabbingOn
        pub fn ee_uicpp_form_keyboard_value(form: *mut c_void, which: c_int, group: c_ulong) -> c_ulong;
        pub fn ee_uicpp_form_keyboard_hot_key(form: *mut c_void, group: c_ulong, index: c_ulong) -> *mut c_void;
        pub fn ee_uicpp_form_keyboard_always_fire(key: *mut c_void) -> bool;
        pub fn ee_uicpp_form_keyboard_fire(key: *mut c_void) -> bool;
        // call: 0 sTabKey->JustPressed, 1 sShiftKey->GetValue, 2 ShiftTab, 3 Tab, 4 the captured control's
        // UpdateKeyboard, 5 the tab stop's UpdateKeyboard
        pub fn ee_uicpp_form_keyboard_call(form: *mut c_void, call: c_int) -> c_long;
    }
}

struct Key(*mut c_void);

impl FireHost for Key {
    fn enabled(&self) -> bool {
        unsafe { sys::ee_uicpp_hotkey_value(self.0, 0) != 0 }
    }
    fn modifier_value(&mut self, i: usize) -> Option<c_long> {
        let set = unsafe { sys::ee_uicpp_hotkey_value(self.0, 4 + i as c_int) != 0 };
        set.then(|| unsafe { sys::ee_uicpp_hotkey_modifier_value(self.0, i as c_int) })
    }
    fn keyboard_state(&mut self) -> c_ulong {
        unsafe { sys::ee_uicpp_hotkey_value(self.0, 2) }
    }
    fn own_state(&self) -> c_ulong {
        unsafe { sys::ee_uicpp_hotkey_value(self.0, 1) }
    }
    fn key_state(&mut self) -> c_ulong {
        unsafe { sys::ee_uicpp_hotkey_value(self.0, 3) }
    }
    fn has_event(&self, e: HotKeyEvent) -> bool {
        unsafe { sys::ee_uicpp_hotkey_has_event(self.0, e as c_int) }
    }
    fn key_test(&mut self, e: HotKeyEvent) -> bool {
        unsafe { sys::ee_uicpp_hotkey_key_test(self.0, e as c_int) }
    }
    fn process(&mut self, e: HotKeyEvent) {
        unsafe { sys::ee_uicpp_hotkey_process(self.0, e as c_int) }
    }
}

struct Form(*mut c_void);

impl Form {
    fn value(&self, which: c_int, group: usize) -> c_ulong {
        unsafe { sys::ee_uicpp_form_keyboard_value(self.0, which, group as c_ulong) }
    }
    fn call(&mut self, call: c_int) -> c_long {
        unsafe { sys::ee_uicpp_form_keyboard_call(self.0, call) }
    }
}

impl KeyboardHost for Form {
    fn captured(&self) -> bool {
        self.value(0, 0) != 0
    }
    fn captured_ime_enabled(&mut self) -> bool {
        self.value(1, 0) != 0
    }
    fn groups(&self) -> usize {
        self.value(2, 0) as usize
    }
    fn hot_keys(&self, g: usize) -> usize {
        self.value(3, g) as usize
    }
    fn hot_key(&self, g: usize, j: usize) -> HotKey {
        unsafe { sys::ee_uicpp_form_keyboard_hot_key(self.0, g as c_ulong, j as c_ulong) }
    }
    fn always_fire(&self, k: HotKey) -> bool {
        unsafe { sys::ee_uicpp_form_keyboard_always_fire(k) }
    }
    fn fire(&mut self, k: HotKey) -> bool {
        unsafe { sys::ee_uicpp_form_keyboard_fire(k) }
    }
    fn tabbing(&self) -> bool {
        self.value(4, 0) != 0
    }
    fn tab_just_pressed(&mut self) -> bool {
        self.call(0) != 0
    }
    fn shift_value(&mut self) -> c_long {
        self.call(1)
    }
    fn shift_tab(&mut self) {
        self.call(2);
    }
    fn tab(&mut self) {
        self.call(3);
    }
    fn captured_update(&mut self) -> c_long {
        self.call(4)
    }
    fn tab_stop_update(&mut self) -> c_long {
        self.call(5)
    }
}

/// `UIHotKey::CalculateKeyCount` from which of the key and the three modifiers are set.
#[no_mangle]
pub extern "C" fn ee_ui_hotkey_key_count(has_key: bool, m0: bool, m1: bool, m2: bool) -> c_ulong {
    hotkey::key_count(has_key, [m0, m1, m2])
}

/// `UIHotKeySorter::operator()`.
#[no_mangle]
pub extern "C" fn ee_ui_hotkey_before(
    left_always: bool,
    left_keys: c_ulong,
    right_always: bool,
    right_keys: c_ulong,
) -> bool {
    hotkey::before(left_always, left_keys, right_always, right_keys)
}

unsafe fn c_bytes<'a>(s: *const c_char) -> Option<&'a [u8]> {
    if s.is_null() {
        None
    } else {
        Some(core::ffi::CStr::from_ptr(s).to_bytes())
    }
}

/// `UIHotKey::GetHotKeyName`: the modifiers' names (NULL: no modifier) and the key's, as C strings -> the name in
/// `out` (`capacity` bytes, NUL-terminated); returns its length. The C++ sizes `out` from the names' lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_name(
    m0: *const c_char,
    m1: *const c_char,
    m2: *const c_char,
    key: *const c_char,
    out: *mut u8,
    capacity: usize,
) -> usize {
    let name = hotkey::hot_key_name([c_bytes(m0), c_bytes(m1), c_bytes(m2)], c_bytes(key).unwrap_or(b""));
    let n = name.len().min(capacity.saturating_sub(1));
    core::ptr::copy_nonoverlapping(name.as_ptr(), out, n);
    out.add(n).write(0);
    n
}

/// `UIHotKey::FireEvents` -> whether an event fired.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_fire_events(key: *mut c_void) -> bool {
    hotkey::fire_events(&mut Key(key))
}

/// `UIForm::UpdateKeyboard` -> its result.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_form_update_keyboard(form: *mut c_void) -> c_long {
    hotkey::update_keyboard(&mut Form(form))
}
