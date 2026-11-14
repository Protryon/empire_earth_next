//! The C ABI of the Input System (`include/lle/ISKeyboard.h`, `ISMouse.h`, `ISEntity.h`, `ISDeviceInput.h`): the two
//! system devices, the window thread's events, and the system keyboard and mouse with their entities, which the
//! game threads read. Every pointer may be NULL: a NULL keyboard, mouse or entity reads as nothing held.
//!
//! Locks: a keyboard or mouse call holds that object's lock and, inside it, its device's; the window thread's events
//! take a device's lock only. No call goes out to C++.

use super::devices::{modifier_state, Keyboard, Mouse, ERROR_BEYOND_END, MOUSE_ENTITY_COUNT};
use super::{handle_event, Device};
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_long, c_ulong};
use std::ptr;
use std::sync::atomic::{AtomicPtr, AtomicU64, Ordering};
use std::sync::{Mutex, MutexGuard};

static KEYBOARD_DEVICE: AtomicPtr<Device> = AtomicPtr::new(ptr::null_mut());
static MOUSE_DEVICE: AtomicPtr<Device> = AtomicPtr::new(ptr::null_mut());

/// The double-press delay in ms (`gDoublePressTimeDelta`, 250 by default; the keys add 200).
static DOUBLE_PRESS_DELAY: AtomicU64 = AtomicU64::new(250);

fn delay() -> c_ulong {
    DOUBLE_PRESS_DELAY.load(Ordering::Relaxed) as c_ulong
}

/// The system device of a slot.
///
/// # Safety
/// The device lives until `ee_input_system_shutdown`, which the game calls after the keyboard and mouse are gone.
unsafe fn device<'a>(slot: &AtomicPtr<Device>) -> Option<&'a Device> {
    slot.load(Ordering::Acquire).as_ref()
}

/// The system keyboard (`ISKeyboard`; C++ holds a pointer to this static as `gISKeyboard`).
pub struct EeInputKeyboard {
    state: Mutex<Option<Keyboard>>,
}

/// The system mouse (`ISMouse`, `gISMouse`).
pub struct EeInputMouse {
    state: Mutex<Option<Mouse>>,
}

/// An input entity (`ISEntity`): a keyboard index (a key by `DIK_*` scan code, 0xe0..0xe2 the either-side keys) or a
/// mouse index (0..2 the axes, 3..6 the buttons). The records never change; the state is the device's.
pub struct EeInputEntity {
    mouse: bool,
    index: u16,
}

static SYSTEM_KEYBOARD: EeInputKeyboard = EeInputKeyboard { state: Mutex::new(None) };
static SYSTEM_MOUSE: EeInputMouse = EeInputMouse { state: Mutex::new(None) };

const fn key_entities() -> [EeInputEntity; 256] {
    let mut out = [const { EeInputEntity { mouse: false, index: 0 } }; 256];
    let mut i = 0;
    while i < 256 {
        out[i].index = i as u16;
        i += 1;
    }
    out
}
static KEY_ENTITIES: [EeInputEntity; 256] = key_entities();
static MOUSE_ENTITIES: [EeInputEntity; MOUSE_ENTITY_COUNT] = [
    EeInputEntity { mouse: true, index: 0 },
    EeInputEntity { mouse: true, index: 1 },
    EeInputEntity { mouse: true, index: 2 },
    EeInputEntity { mouse: true, index: 3 },
    EeInputEntity { mouse: true, index: 4 },
    EeInputEntity { mouse: true, index: 5 },
    EeInputEntity { mouse: true, index: 6 },
];

fn keyboard_lock() -> MutexGuard<'static, Option<Keyboard>> {
    SYSTEM_KEYBOARD.state.lock().unwrap_or_else(|e| e.into_inner())
}

fn mouse_lock() -> MutexGuard<'static, Option<Mouse>> {
    SYSTEM_MOUSE.state.lock().unwrap_or_else(|e| e.into_inner())
}

/// Creates the system keyboard's and mouse's devices (`ISInitialize`); a second call keeps the first ones.
#[no_mangle]
pub extern "C" fn ee_input_system_init() {
    for slot in [&KEYBOARD_DEVICE, &MOUSE_DEVICE] {
        let new = Box::into_raw(Box::new(Device::new()));
        if slot.compare_exchange(ptr::null_mut(), new, Ordering::AcqRel, Ordering::Acquire).is_err() {
            // SAFETY: `new` came from Box::into_raw above and was not published.
            drop(unsafe { Box::from_raw(new) });
        }
    }
}

/// Destroys both system devices (`ISShutdown`, after the keyboard and the mouse that read them). As in the original,
/// the window thread must not be inside `ee_input_handle_event` meanwhile.
#[no_mangle]
pub extern "C" fn ee_input_system_shutdown() {
    for slot in [&KEYBOARD_DEVICE, &MOUSE_DEVICE] {
        let old = slot.swap(ptr::null_mut(), Ordering::AcqRel);
        if !old.is_null() {
            // SAFETY: published by ee_input_system_init from Box::into_raw; taken out of the slot once.
            drop(unsafe { Box::from_raw(old) });
        }
    }
}

/// `ISHandleEvent`: a window event's type and the fields the Input System reads (the key's scan code, the mouse's
/// motion, button and wheel; only those of the event's type are used).
#[no_mangle]
pub extern "C" fn ee_input_handle_event(kind: u32, scan_code: u32, dx: i32, dy: i32, button: u32, wheel: f32) {
    // SAFETY: the slots hold devices from ee_input_system_init or NULL.
    let (keyboard, mouse) = unsafe { (device(&KEYBOARD_DEVICE), device(&MOUSE_DEVICE)) };
    handle_event(keyboard, mouse, kind, scan_code, dx, dy, button, wheel);
}

/// `ISSetDoublePressTimeDelta` (and `ISInitialize`'s delay): the longest time between two presses of a double press.
#[no_mangle]
pub extern "C" fn ee_input_set_double_press_delay(delay_ms: c_ulong) {
    DOUBLE_PRESS_DELAY.store(delay_ms as u64, Ordering::Relaxed);
}

/// `ISKeyboard::ComputeModifierState`: a scan code's modifier bit (1 Shift, 2 Control, 4 Alt).
#[no_mangle]
pub extern "C" fn ee_input_modifier_state(key: c_ulong) -> c_ulong {
    modifier_state(key)
}

// ---- the keyboard

/// How many key names `ee_input_keyboard_create` takes (the keys the keyboard creates, in their resource order).
#[no_mangle]
pub extern "C" fn ee_input_keyboard_key_count() -> c_ulong {
    super::devices::KEY_CODES.len() as c_ulong
}

/// Creates the system keyboard on the system device (`ISKeyboard`'s constructor; `time` is `TMGetTime`): `names`
/// holds `ee_input_keyboard_key_count()` UTF-8 names, or is NULL for the English table. A second call replaces it.
///
/// # Safety
/// `names` is NULL or holds that many pointers, each NULL or a NUL-terminated string.
#[no_mangle]
pub unsafe extern "C" fn ee_input_keyboard_create(
    names: *const *const c_char,
    time: c_ulong,
) -> *const EeInputKeyboard {
    let owned: Option<Vec<CString>> = (!names.is_null()).then(|| {
        (0..super::devices::KEY_CODES.len())
            .map(|i| {
                let p = *names.add(i);
                if p.is_null() {
                    CString::default()
                } else {
                    CStr::from_ptr(p).to_owned()
                }
            })
            .collect()
    });
    let k = Keyboard::new(owned.as_deref(), device(&KEYBOARD_DEVICE), time, delay());
    *keyboard_lock() = Some(k);
    &SYSTEM_KEYBOARD
}

/// Destroys the system keyboard (its entities read as released from now on).
#[no_mangle]
pub extern "C" fn ee_input_keyboard_destroy(_k: *const EeInputKeyboard) {
    *keyboard_lock() = None;
}

/// `GetEntity`: the entity at a scan code into `*out` (NULL for a code without a key) -> 0, or 0xa past 255.
///
/// # Safety
/// `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_input_keyboard_entity(
    k: *const EeInputKeyboard,
    index: c_ulong,
    out: *mut *const EeInputEntity,
) -> c_long {
    let g = keyboard_lock();
    let (result, present) = match (k.is_null(), g.as_ref()) {
        (false, Some(kb)) => kb.entity(index),
        _ => (ERROR_BEYOND_END, false),
    };
    *out = if present { &KEY_ENTITIES[index as usize] } else { ptr::null() };
    result
}

/// `GetModifierState`: the modifiers held (1 Shift, 2 Control, 4 Alt).
#[no_mangle]
pub extern "C" fn ee_input_keyboard_modifiers(k: *const EeInputKeyboard) -> c_ulong {
    if k.is_null() {
        return 0;
    }
    keyboard_lock().as_ref().map_or(0, |kb| kb.modifiers())
}

/// `GetShouldUpdate`: events were read, or a key is or was held.
#[no_mangle]
pub extern "C" fn ee_input_keyboard_should_update(k: *const EeInputKeyboard) -> bool {
    !k.is_null() && keyboard_lock().as_ref().is_some_and(|kb| kb.should_update())
}

/// `Refresh` (`time` is `TMGetTime`).
#[no_mangle]
pub extern "C" fn ee_input_keyboard_refresh(k: *const EeInputKeyboard, time: c_ulong) {
    if let (false, Some(kb)) = (k.is_null(), keyboard_lock().as_mut()) {
        // SAFETY: the device outlives the keyboard (see `device`).
        kb.refresh(unsafe { device(&KEYBOARD_DEVICE) }, time, delay());
    }
}

/// `Resume`: the device acquired and the whole state read again.
#[no_mangle]
pub extern "C" fn ee_input_keyboard_resume(k: *const EeInputKeyboard, time: c_ulong) {
    if let (false, Some(kb)) = (k.is_null(), keyboard_lock().as_mut()) {
        // SAFETY: as above.
        kb.resume(unsafe { device(&KEYBOARD_DEVICE) }, time, delay());
    }
}

/// `Suspend`: the device's events no longer buffered.
#[no_mangle]
pub extern "C" fn ee_input_keyboard_suspend(k: *const EeInputKeyboard) {
    if let (false, Some(kb)) = (k.is_null(), keyboard_lock().as_mut()) {
        // SAFETY: as above.
        kb.suspend(unsafe { device(&KEYBOARD_DEVICE) });
    }
}

// ---- the mouse

/// Creates the system mouse on the system device (`ISMouse`'s constructor, without the window's relative mode, which
/// C++ sets). A second call replaces it.
#[no_mangle]
pub extern "C" fn ee_input_mouse_create() -> *const EeInputMouse {
    // SAFETY: as above.
    let m = Mouse::new(unsafe { device(&MOUSE_DEVICE) });
    *mouse_lock() = Some(m);
    &SYSTEM_MOUSE
}

/// Destroys the system mouse.
#[no_mangle]
pub extern "C" fn ee_input_mouse_destroy(_m: *const EeInputMouse) {
    *mouse_lock() = None;
}

/// `GetEntity`: the axis or button at an index into `*out` -> 0, or 0xa (and NULL) past 6.
///
/// # Safety
/// `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_input_mouse_entity(
    m: *const EeInputMouse,
    index: c_ulong,
    out: *mut *const EeInputEntity,
) -> c_long {
    let live = !m.is_null() && mouse_lock().is_some();
    let (result, present) = if live { Mouse::entity(index) } else { (ERROR_BEYOND_END, false) };
    *out = if present { &MOUSE_ENTITIES[index as usize] } else { ptr::null() };
    result
}

/// `GetShouldUpdate`: events were read, a button changed or the mouse moved.
#[no_mangle]
pub extern "C" fn ee_input_mouse_should_update(m: *const EeInputMouse) -> bool {
    !m.is_null() && mouse_lock().as_ref().is_some_and(|ms| ms.should_update())
}

/// `Refresh` (`time` is `TMGetTime`).
#[no_mangle]
pub extern "C" fn ee_input_mouse_refresh(m: *const EeInputMouse, time: c_ulong) {
    if let (false, Some(ms)) = (m.is_null(), mouse_lock().as_mut()) {
        // SAFETY: as above.
        ms.refresh(unsafe { device(&MOUSE_DEVICE) }, time, delay());
    }
}

/// `RefreshXY`: the motion since the last read.
#[no_mangle]
pub extern "C" fn ee_input_mouse_refresh_xy(m: *const EeInputMouse) {
    if let (false, Some(ms)) = (m.is_null(), mouse_lock().as_mut()) {
        // SAFETY: as above.
        ms.refresh_xy(unsafe { device(&MOUSE_DEVICE) });
    }
}

/// `Resume`: the device acquired and the state read again (C++ puts the window's pointer in relative mode first).
#[no_mangle]
pub extern "C" fn ee_input_mouse_resume(m: *const EeInputMouse) {
    if let (false, Some(ms)) = (m.is_null(), mouse_lock().as_mut()) {
        // SAFETY: as above.
        ms.resume(unsafe { device(&MOUSE_DEVICE) });
    }
}

/// `Suspend` (C++ takes the window's pointer out of relative mode after it).
#[no_mangle]
pub extern "C" fn ee_input_mouse_suspend(m: *const EeInputMouse) {
    if let (false, Some(ms)) = (m.is_null(), mouse_lock().as_mut()) {
        // SAFETY: as above.
        ms.suspend(unsafe { device(&MOUSE_DEVICE) });
    }
}

/// `SetUseBufferedMovement`: false keeps the motion at zero.
#[no_mangle]
pub extern "C" fn ee_input_mouse_set_use_buffered_movement(m: *const EeInputMouse, used: bool) {
    if let (false, Some(ms)) = (m.is_null(), mouse_lock().as_mut()) {
        ms.set_use_buffered_movement(used);
    }
}

// ---- the entities

/// An entity's reading on the live keyboard or mouse (the default for NULL or a destroyed device).
fn read<T: Default>(
    e: *const EeInputEntity,
    key: impl Fn(&Keyboard, usize) -> T,
    button: impl Fn(&Mouse, usize) -> T,
) -> T {
    // SAFETY: entity pointers come from KEY_ENTITIES / MOUSE_ENTITIES (statics) or are NULL.
    let Some(e) = (unsafe { e.as_ref() }) else { return T::default() };
    let i = e.index as usize;
    if e.mouse {
        mouse_lock().as_ref().map(|m| button(m, i)).unwrap_or_default()
    } else {
        keyboard_lock().as_ref().map(|k| key(k, i)).unwrap_or_default()
    }
}

/// `GetValue`: a key or button 1 held, 0 not; an axis its motion since the last refresh.
#[no_mangle]
pub extern "C" fn ee_input_entity_value(e: *const EeInputEntity) -> c_long {
    read(e, |k, i| k.value(i), |m, i| m.value(i))
}

/// `GetNormalizedValue`: a key or button 0x7fff held; an axis its motion over its granularity (the wheel in notches).
#[no_mangle]
pub extern "C" fn ee_input_entity_normalized_value(e: *const EeInputEntity) -> c_long {
    read(e, |k, i| k.normalized_value(i), |m, i| m.normalized_value(i))
}

/// `JustPressed`: pressed since the last refresh.
#[no_mangle]
pub extern "C" fn ee_input_entity_just_pressed(e: *const EeInputEntity) -> bool {
    read(e, |k, i| k.just_pressed(i), |m, i| m.pressed_amount(i) > 0)
}

/// `JustUnpressed`: released since the last refresh.
#[no_mangle]
pub extern "C" fn ee_input_entity_just_unpressed(e: *const EeInputEntity) -> bool {
    read(e, |k, i| k.just_unpressed(i), |m, i| m.unpressed_amount(i) > 0)
}

/// `JustDoublePressed`: the press of the last refresh was a double press.
#[no_mangle]
pub extern "C" fn ee_input_entity_just_double_pressed(e: *const EeInputEntity) -> bool {
    read(e, |k, i| k.just_double_pressed(i), |m, i| m.just_double_pressed(i))
}

/// `GetIndex`: a key's scan code (a mouse entity's index).
///
/// # Safety
/// `e` is NULL or an entity from `ee_input_keyboard_entity` / `ee_input_mouse_entity`.
#[no_mangle]
pub unsafe extern "C" fn ee_input_entity_index(e: *const EeInputEntity) -> c_ulong {
    e.as_ref().map_or(0, |e| e.index as c_ulong)
}

/// `GetName`: the entity's UTF-8 name ("A key", "SHIFT key (left) and SHIFT key (right)", "Left button"), valid while
/// its keyboard lives; "" for NULL.
///
/// # Safety
/// As `ee_input_entity_index`.
#[no_mangle]
pub unsafe extern "C" fn ee_input_entity_name(e: *const EeInputEntity) -> *const c_char {
    let Some(e) = e.as_ref() else { return c"".as_ptr() };
    if e.mouse {
        return Mouse::name(e.index as usize).unwrap_or(c"").as_ptr();
    }
    // the name's bytes live in the keyboard's heap, not in the guard: valid until the keyboard is destroyed
    keyboard_lock().as_ref().and_then(|k| k.name(e.index as usize)).map_or(c"".as_ptr(), |n| n.as_ptr())
}

#[cfg(test)]
mod tests {
    use super::super::devices::{KeyKind, ERROR_OK};
    use super::*;

    #[test]
    fn system_keyboard_and_mouse_through_the_c_abi() {
        ee_input_system_init();
        let k = unsafe { ee_input_keyboard_create(ptr::null(), 0) };
        let m = ee_input_mouse_create();
        assert_eq!(keyboard_lock().as_ref().map(|kb| kb.kind(0xe1)), Some(KeyKind::Extended(0x2a, 0x36)));
        let mut shift: *const EeInputEntity = ptr::null();
        let mut none: *const EeInputEntity = &KEY_ENTITIES[0];
        unsafe {
            assert_eq!(ee_input_keyboard_entity(k, 0xe1, &mut shift), ERROR_OK);
            assert_eq!(ee_input_keyboard_entity(k, 0, &mut none), ERROR_OK);
            assert!(none.is_null());
            assert_eq!(ee_input_keyboard_entity(k, 300, &mut none), ERROR_BEYOND_END);
        }
        let name = unsafe { CStr::from_ptr(ee_input_entity_name(shift)) };
        assert_eq!(name.to_str().unwrap(), "SHIFT key (left) and SHIFT key (right)");
        ee_input_keyboard_resume(k, 0);
        ee_input_handle_event(super::super::event::KEY_DOWN, 0x36, 0, 0, 0, 0.0);
        ee_input_keyboard_refresh(k, 10);
        assert!(ee_input_entity_just_pressed(shift) && ee_input_entity_value(shift) == 1);
        assert_eq!(ee_input_keyboard_modifiers(k), 1);
        assert!(ee_input_keyboard_should_update(k));
        // a NULL entity reads as released
        assert!(!ee_input_entity_just_pressed(ptr::null()) && ee_input_entity_value(ptr::null()) == 0);

        let mut left: *const EeInputEntity = ptr::null();
        assert_eq!(unsafe { ee_input_mouse_entity(m, 3, &mut left) }, ERROR_OK);
        ee_input_mouse_resume(m);
        ee_input_handle_event(super::super::event::MOUSE_DOWN, 0, 0, 0, 1, 0.0);
        ee_input_mouse_refresh(m, 10);
        assert!(ee_input_entity_just_pressed(left) && unsafe { ee_input_entity_index(left) } == 3);
        let name = unsafe { CStr::from_ptr(ee_input_entity_name(left)) };
        assert_eq!(name.to_str().unwrap(), "Left button");

        ee_input_mouse_suspend(m);
        ee_input_mouse_destroy(m);
        ee_input_keyboard_suspend(k);
        ee_input_keyboard_destroy(k);
        ee_input_system_shutdown();
        assert_eq!(ee_input_entity_value(shift), 0);
    }
}
