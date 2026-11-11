//! The hot keys (`User Interface/UIHotKey.cpp`: `CalculateKeyCount`, `GetHotKeyName`, `FireEvents`) and the form's
//! use of them (`UIForm.cpp`: the `UIHotKeySorter` order, `UpdateKeyboard`'s hot-key pass and Tab handling).
//!
//! The hot keys, their keys and events and the form's per-group lists stay C++ objects (`std::sort` and the lists'
//! edits too); the decisions are here, the keys' and events' virtuals are host calls. Key names are game text, read
//! as C strings of UTF-8 bytes (decision D6).

use core::ffi::{c_long, c_ulong, c_void};

/// A C++ `UIHotKey*`, opaque.
pub type HotKey = *mut c_void;

/// `CalculateKeyCount`: the key (when set) and each modifier that is set.
pub fn key_count(has_key: bool, modifiers: [bool; 3]) -> c_ulong {
    let mut n: c_ulong = has_key as c_ulong;
    for m in modifiers {
        if m {
            n += 1;
        }
    }
    n
}

/// `UIHotKeySorter`: whether `left` goes before `right` (always-fire keys last, then fewer keys first).
pub fn before(left_always: bool, left_keys: c_ulong, right_always: bool, right_keys: c_ulong) -> bool {
    if left_always && !right_always {
        return false;
    }
    if right_always && !left_always {
        return true;
    }
    left_keys < right_keys
}

/// `UString::Find(" key", 0)` then `GetSubstring(0, ..)`: the name up to its first " key" (all of it without one).
pub fn without_key_suffix(name: &[u8]) -> &[u8] {
    const SUFFIX: &[u8] = b" key";
    match name.windows(SUFFIX.len()).position(|w| w == SUFFIX) {
        Some(at) => &name[..at],
        None => name,
    }
}

/// `GetHotKeyName`: "Ctrl + Shift + A" from the modifiers' and the key's names (each without " key").
pub fn hot_key_name(modifiers: [Option<&[u8]>; 3], key: &[u8]) -> Vec<u8> {
    let mut out = Vec::new();
    for name in modifiers.into_iter().flatten() {
        out.extend_from_slice(without_key_suffix(name));
        out.extend_from_slice(b" + ");
    }
    out.extend_from_slice(without_key_suffix(key));
    out
}

/// A hot key's four events, in the order `FireEvents` tries them.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HotKeyEvent {
    /// `mDoublePressedEvent` when the key `JustDoublePressed()`.
    DoublePressed = 0,
    /// `mJustPressedEvent` when the key `JustPressed()`.
    JustPressed = 1,
    /// `mPressedEvent` when the key's `GetValue() > 0`.
    Pressed = 2,
    /// `mJustUnpressedEvent` when the key `JustUnpressed()`.
    JustUnpressed = 3,
}

const EVENTS: [HotKeyEvent; 4] =
    [HotKeyEvent::DoublePressed, HotKeyEvent::JustPressed, HotKeyEvent::Pressed, HotKeyEvent::JustUnpressed];

/// One hot key as `FireEvents` asks about it.
pub trait FireHost {
    /// `mEnabled`.
    fn enabled(&self) -> bool;
    /// Modifier `i`'s `GetValue()`, `None` when the slot is empty.
    fn modifier_value(&mut self, i: usize) -> Option<c_long>;
    /// `gISKeyboard->GetModifierState()`.
    fn keyboard_state(&mut self) -> c_ulong;
    /// `mModifierState`.
    fn own_state(&self) -> c_ulong;
    /// `ISKeyboard::ComputeModifierState(static_cast<ISKey*>(mKey)->GetIndex())`.
    fn key_state(&mut self) -> c_ulong;
    /// Whether the event is set.
    fn has_event(&self, e: HotKeyEvent) -> bool;
    /// The key's test for the event (`JustDoublePressed`, `JustPressed`, `GetValue() > 0`, `JustUnpressed`).
    fn key_test(&mut self, e: HotKeyEvent) -> bool;
    /// The event's `ProcessEvent(mForm, this)`.
    fn process(&mut self, e: HotKeyEvent);
}

/// `FireEvents`: when enabled and every set modifier is down, and the keyboard's modifier state is the hot key's (or
/// the key's own, for a modifier used as the key), the first event whose test passes fires -> whether one did.
pub fn fire_events(h: &mut impl FireHost) -> bool {
    if !h.enabled() {
        return false;
    }
    for i in 0..3 {
        if let Some(v) = h.modifier_value(i) {
            if v == 0 {
                return false;
            }
        }
    }
    let state = h.keyboard_state();
    if !(state == h.own_state() || state == h.key_state()) {
        return false;
    }
    for e in EVENTS {
        if h.has_event(e) && h.key_test(e) {
            h.process(e);
            return true;
        }
    }
    false
}

/// The form as `UpdateKeyboard` asks about it.
pub trait KeyboardHost {
    /// `mKeyboardCaptured != NULL` and, when it is, its `GetIMEEnabled()`.
    fn captured(&self) -> bool;
    fn captured_ime_enabled(&mut self) -> bool;
    /// `mHotKeys.size()`, `mHotKeys[g].size()`.
    fn groups(&self) -> usize;
    fn hot_keys(&self, g: usize) -> usize;
    /// `mHotKeys[g][j]`; the hot key's `GetAlwaysFire()`, its virtual `FireEvents()`.
    fn hot_key(&self, g: usize, j: usize) -> HotKey;
    fn always_fire(&self, k: HotKey) -> bool;
    fn fire(&mut self, k: HotKey) -> bool;
    /// `mTabbingOn`.
    fn tabbing(&self) -> bool;
    /// `UIForm::sTabKey->JustPressed()`, `UIForm::sShiftKey->GetValue()`.
    fn tab_just_pressed(&mut self) -> bool;
    fn shift_value(&mut self) -> c_long;
    /// `ShiftTab()`, `Tab()`.
    fn shift_tab(&mut self);
    fn tab(&mut self);
    /// `mKeyboardCaptured->UpdateKeyboard()`, `mControls[mTabStop]->UpdateKeyboard()`.
    fn captured_update(&mut self) -> c_long;
    fn tab_stop_update(&mut self) -> c_long;
}

/// `UIForm::UpdateKeyboard`: unless an IME control holds the keyboard, the hot keys fire, the highest group first
/// (a group is skipped once a group above it fired; after one key that is not always-fire fires, only always-fire
/// keys are tried), then Tab and Shift+Tab move the tab stop; unhandled, the keyboard goes to the capturing control
/// or, with tabbing, to the tab stop -> its result.
pub fn update_keyboard(h: &mut impl KeyboardHost) -> c_long {
    let mut result = 0;
    let mut fired = false;
    let mut handled = false;
    if !h.captured() || !h.captured_ime_enabled() {
        let n = h.groups();
        let mut group_fired = vec![false; n];
        let mut skip = false;
        for i in (0..n).rev() {
            group_fired[i] = false;
            // the C++ compares with the list's size as it is now
            skip = i != h.groups().wrapping_sub(1) && (skip || group_fired.get(i + 1).copied().unwrap_or(false));
            let mut j = h.hot_keys(i);
            while j > 0 && !skip {
                j -= 1;
                let k = h.hot_key(i, j);
                if (fired && !h.always_fire(k)) || !h.fire(k) {
                    continue;
                }
                group_fired[i] = true;
                if !h.always_fire(k) {
                    fired = true;
                }
            }
        }
        if fired {
            handled = true;
        } else if h.tabbing() {
            if h.tab_just_pressed() && h.shift_value() != 0 {
                h.shift_tab();
                handled = true;
            } else if h.tab_just_pressed() {
                h.tab();
                handled = true;
            }
        }
    }
    if !handled {
        if h.captured() {
            result = h.captured_update();
        } else if h.tabbing() {
            result = h.tab_stop_update();
        }
    }
    result
}

/// A `UIEvent*`, opaque.
pub type Event = *mut c_void;

/// The calls a hot key's event setters make on an event.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EventCall {
    /// `Uninitialize`.
    Uninitialize = 0,
    /// `Dereference` (the event deletes itself at 0).
    Dereference = 1,
    /// `Reference`.
    Reference = 2,
    /// `Initialize`.
    Initialize = 3,
}

/// One of a hot key's four event members and the calls on its events.
pub trait EventSlotHost {
    fn get(&self) -> Event;
    fn set(&mut self, e: Event);
    fn call(&mut self, e: Event, c: EventCall);
}

/// `SetDoublePressedEvent` .. `SetPressedEvent`: the old event (when set) uninitialised and released, the new one
/// (when not NULL) stored, referenced and initialised. Setting the event a slot already holds releases it first
/// (an event held only there is deleted before it is referenced again, as in the C++).
pub fn set_event(h: &mut impl EventSlotHost, event: Event) {
    let old = h.get();
    if !old.is_null() {
        h.call(old, EventCall::Uninitialize);
        h.call(old, EventCall::Dereference);
    }
    h.set(event);
    let new = h.get();
    if !new.is_null() {
        h.call(new, EventCall::Reference);
        h.call(new, EventCall::Initialize);
    }
}

/// `kKeyNone`: a hot key record's empty modifier.
pub const KEY_NONE: u32 = 0x7fff;
/// `kErrorOK`.
const ERROR_OK: c_long = 0;

/// One hot key record of the UI database (`UIDbDataHotKeys`, 124 bytes on every target).
#[repr(C)]
#[derive(Clone, Copy)]
pub struct DbHotKey {
    pub unused: u32,
    /// Negative: no hot key in this slot.
    pub id: i32,
    pub label: [u8; 100],
    /// `DIK_*` codes, [`KEY_NONE`]: none.
    pub modifiers: [u32; 3],
    pub key: u32,
}

const _: () = assert!(core::mem::size_of::<DbHotKey>() == 124);

impl DbHotKey {
    /// A record of zero bytes (the C++'s was uninitialised until the first read).
    pub fn zeroed() -> DbHotKey {
        // SAFETY: integers and a byte array: all bits zero is a value of each
        unsafe { core::mem::zeroed() }
    }
}

/// Which key member `GetEntity` fills: a modifier slot (0 to 2) or the key.
pub const KEY_SLOT: usize = 3;

/// A hot key's keys (`SetModifier`, `CreateHotKeys`).
pub trait KeysHost {
    /// `gISKeyboard->GetEntity(code, member)`, `member` a modifier slot or [`KEY_SLOT`]; its result.
    fn get_entity(&mut self, key: HotKey, code: c_ulong, member: usize) -> c_long;
    /// `ISKeyboard::ComputeModifierState(code)`.
    fn modifier_state(&mut self, code: c_ulong) -> c_ulong;
    /// `mModifierState`.
    fn state(&self, key: HotKey) -> c_ulong;
    fn set_state(&mut self, key: HotKey, state: c_ulong);
    /// `mModifiers[slot] = NULL`.
    fn clear_modifier(&mut self, key: HotKey, slot: usize);
    /// `CalculateKeyCount`.
    fn count_keys(&mut self, key: HotKey);
}

/// `SetModifier(code, slot)`: the code's modifier state added, the slot's entity set, the keys counted again.
pub fn set_modifier(h: &mut impl KeysHost, key: HotKey, code: c_ulong, slot: usize) {
    let s = h.state(key) | h.modifier_state(code);
    h.set_state(key, s);
    h.get_entity(key, code, slot);
    h.count_keys(key);
}

/// `CreateHotKeys`' file and table.
pub trait CreateHost: KeysHost {
    fn open(&mut self);
    fn read_count(&mut self, count: &mut c_ulong) -> c_long;
    fn read_record(&mut self, record: &mut DbHotKey) -> c_long;
    /// `new UIHotKey`.
    fn create(&mut self) -> HotKey;
    /// `sHotKey.push_back(key)` (NULL for an unused record).
    fn push(&mut self, key: HotKey);
    fn close(&mut self);
}

/// `CreateHotKeys`: every record of the file into the shared table, a hot key for each record with an id (its
/// modifiers' entities and states, then its key's), NULL for the others. A record whose read fails keeps the
/// previous record's bytes (zeros before the first). The result is the count's read when it fails, else the last
/// record's read or key lookup, as the C++ left it.
pub fn create(h: &mut impl CreateHost) -> c_long {
    h.open();
    let mut count: c_ulong = 0;
    let mut result = h.read_count(&mut count);
    if result == ERROR_OK {
        let mut record = DbHotKey::zeroed();
        let mut i: c_ulong = 0;
        while i < count {
            result = h.read_record(&mut record);
            let key = if record.id >= 0 {
                let k = h.create();
                for m in 0..3 {
                    let code = record.modifiers[m];
                    if code != KEY_NONE {
                        h.get_entity(k, code as c_ulong, m);
                        let s = h.state(k) | h.modifier_state(code as c_ulong);
                        h.set_state(k, s);
                    } else {
                        h.clear_modifier(k, m);
                    }
                }
                result = h.get_entity(k, record.key as c_ulong, KEY_SLOT);
                h.count_keys(k);
                k
            } else {
                core::ptr::null_mut()
            };
            h.push(key);
            i += 1;
        }
    }
    h.close();
    result
}

#[cfg(test)]
mod setters_tests;
#[cfg(test)]
mod tests;
