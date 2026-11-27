//! `Input System.cpp`'s entry points in Rust (LLE lane round 7): `ISInitialize`'s devices and objects, `ISShutdown`
//! (registered with the engine's shutdown list, ee_util), and the members that read the clock (`TMGetTime`,
//! ee_time) or the window's relative pointer mode (`PlatMouseSetRelative`: DirectInput's exclusive mouse). C++ keeps
//! the key names' resource strings and the exported globals, whose addresses `ee_input_initialize` takes so that the
//! shutdown clears them.
//!
//! The order of every call is the original's: the delay, the shutdown registration, the devices, the keyboard
//! (resumed at the clock's time), the window's pointer into relative mode and out again around the mouse's creation
//! (the original constructor resumed and suspended it); at shutdown the mouse suspended (relative mode off after it)
//! and destroyed, then the keyboard, then the devices.

use super::ffi::*;
use std::ffi::{c_char, c_int, c_long, c_ulong, c_void};
use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};

/// cbindgen:ignore
#[cfg(not(test))]
mod sys {
    use std::ffi::{c_int, c_void};
    extern "C" {
        pub(super) fn PlatMouseSetRelative(window: *mut c_void, relative: c_int) -> c_int;
    }
}
#[cfg(not(test))]
use sys::PlatMouseSetRelative;

/// The test build's window layer: records the calls.
#[cfg(test)]
pub(crate) static RELATIVE_CALLS: std::sync::Mutex<Vec<(usize, c_int)>> = std::sync::Mutex::new(Vec::new());
#[cfg(test)]
#[allow(non_snake_case)]
unsafe fn PlatMouseSetRelative(window: *mut c_void, relative: c_int) -> c_int {
    RELATIVE_CALLS.lock().unwrap().push((window as usize, relative));
    0
}

/// The game window (`PlatWindow*`), whose pointer is in relative mode while the mouse is resumed.
static MOUSE_WINDOW: AtomicPtr<c_void> = AtomicPtr::new(ptr::null_mut());
/// The C++ globals `gISKeyboard` and `gISMouse`.
static KEYBOARD_GLOBAL: AtomicPtr<*const EeInputKeyboard> = AtomicPtr::new(ptr::null_mut());
static MOUSE_GLOBAL: AtomicPtr<*const EeInputMouse> = AtomicPtr::new(ptr::null_mut());

/// `gISKeyboard`'s value (NULL outside ISInitialize..ISShutdown), for the toolkit's Rust.
pub fn system_keyboard() -> *const EeInputKeyboard {
    let g = KEYBOARD_GLOBAL.load(Ordering::Acquire);
    // SAFETY: the global's address lives for the process (ee_input_initialize's contract)
    if g.is_null() { ptr::null() } else { unsafe { *g } }
}

/// `gISMouse`'s value (NULL outside ISInitialize..ISShutdown), for the toolkit's Rust.
pub fn system_mouse() -> *const EeInputMouse {
    let g = MOUSE_GLOBAL.load(Ordering::Acquire);
    // SAFETY: as system_keyboard's
    if g.is_null() { ptr::null() } else { unsafe { *g } }
}

fn set_relative(relative: c_int) {
    let window = MOUSE_WINDOW.load(Ordering::Acquire);
    if !window.is_null() {
        // SAFETY: the window outlives the Input System (ISShutdown runs before the window goes)
        unsafe { PlatMouseSetRelative(window, relative) };
    }
}

fn now() -> c_ulong {
    crate::time::ee_time_get() as c_ulong
}

/// `ISInitialize`: the system keyboard and mouse on the window's events. `names` is NULL (the English names) or one
/// string per key code (`ee_input_keyboard_key_count`); `keyboard` and `mouse` are the C++ globals, set here and
/// cleared by the shutdown. Always 0 (`kErrorOK`).
///
/// # Safety
/// `names` as `ee_input_keyboard_create`'s; `keyboard` and `mouse` live for the process; `window` is NULL or the game
/// window.
#[no_mangle]
pub unsafe extern "C" fn ee_input_initialize(
    window: *mut c_void,
    names: *const *const c_char,
    double_press_delay: c_ulong,
    keyboard: *mut *const EeInputKeyboard,
    mouse: *mut *const EeInputMouse,
) -> c_long {
    ee_input_set_double_press_delay(double_press_delay);
    crate::util::ee_util_register_shutdown(ee_input_shutdown);
    ee_input_system_init();
    KEYBOARD_GLOBAL.store(keyboard, Ordering::Release);
    MOUSE_GLOBAL.store(mouse, Ordering::Release);
    *keyboard = ee_input_keyboard_create(names, now());
    MOUSE_WINDOW.store(window, Ordering::Release);
    set_relative(1);
    *mouse = ee_input_mouse_create();
    set_relative(0);
    0
}

/// The keys' names from the resource strings `first_id`, `first_id + 1`, ... (`ISInitialize`'s loop over
/// `UResourceFile::GetString`): a string the table lacks records native error 1814 and keeps the name before it
/// (the loop's one string, empty at first).
pub fn key_names(first_id: c_ulong) -> Vec<Vec<u8>> {
    let mut name = vec![0u8];
    (0..super::devices::KEY_CODES.len() as c_ulong)
        .map(|i| {
            match crate::util::strings::resource_string_for_get((i + first_id) as u16) {
                Some(text) => name = text,
                None => {
                    crate::error::ee_error_record_native(1814);
                }
            }
            name.clone()
        })
        .collect()
}

/// `ISInitialize` whole: the keys' names from the resource strings (`has_names`: the game passed a resource file;
/// else the English names), then [`ee_input_initialize`].
///
/// # Safety
/// As [`ee_input_initialize`].
#[no_mangle]
pub unsafe extern "C" fn ee_input_initialize_strings(
    window: *mut c_void,
    has_names: bool,
    first_id: c_ulong,
    double_press_delay: c_ulong,
    keyboard: *mut *const EeInputKeyboard,
    mouse: *mut *const EeInputMouse,
) -> c_long {
    let names = if has_names { key_names(first_id) } else { Vec::new() };
    let pointers: Vec<*const c_char> = names.iter().map(|n| n.as_ptr().cast()).collect();
    let list = if has_names { pointers.as_ptr() } else { ptr::null() };
    ee_input_initialize(window, list, double_press_delay, keyboard, mouse)
}

/// `ISShutdown` (a shutdown-list function): the mouse and the keyboard suspended and destroyed, their globals
/// cleared, then the devices.
///
/// # Safety
/// The globals `ee_input_initialize` was given are still live (they are the C++ statics).
#[no_mangle]
pub unsafe extern "C" fn ee_input_shutdown(_forced: bool) {
    let mouse = MOUSE_GLOBAL.load(Ordering::Acquire);
    if !mouse.is_null() && !(*mouse).is_null() {
        ee_input_mouse_suspend_window(*mouse);
        ee_input_mouse_destroy(*mouse);
        *mouse = ptr::null();
    }
    let keyboard = KEYBOARD_GLOBAL.load(Ordering::Acquire);
    if !keyboard.is_null() && !(*keyboard).is_null() {
        ee_input_keyboard_suspend(*keyboard);
        ee_input_keyboard_destroy(*keyboard);
        *keyboard = ptr::null();
    }
    MOUSE_WINDOW.store(ptr::null_mut(), Ordering::Release);
    ee_input_system_shutdown();
}

/// `ISKeyboard::Refresh` at the clock's time.
#[no_mangle]
pub extern "C" fn ee_input_keyboard_refresh_now(k: *const EeInputKeyboard) {
    ee_input_keyboard_refresh(k, now());
}

/// `ISKeyboard::Resume` at the clock's time.
#[no_mangle]
pub extern "C" fn ee_input_keyboard_resume_now(k: *const EeInputKeyboard) {
    ee_input_keyboard_resume(k, now());
}

/// `ISMouse::Refresh` at the clock's time.
#[no_mangle]
pub extern "C" fn ee_input_mouse_refresh_now(m: *const EeInputMouse) {
    ee_input_mouse_refresh(m, now());
}

/// `ISMouse::Resume`: the window's pointer into relative mode, then the device acquired.
#[no_mangle]
pub extern "C" fn ee_input_mouse_resume_window(m: *const EeInputMouse) {
    set_relative(1);
    ee_input_mouse_resume(m);
}

/// `ISMouse::Suspend`: the device released, then the window's pointer out of relative mode.
#[no_mangle]
pub extern "C" fn ee_input_mouse_suspend_window(m: *const EeInputMouse) {
    ee_input_mouse_suspend(m);
    set_relative(0);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn initialize_and_shutdown() {
        let _guard = test_system();
        static mut KEYBOARD: *const EeInputKeyboard = ptr::null();
        static mut MOUSE: *const EeInputMouse = ptr::null();
        RELATIVE_CALLS.lock().unwrap().clear();
        unsafe {
            let k = &raw mut KEYBOARD;
            let m = &raw mut MOUSE;
            assert_eq!(ee_input_initialize(0x40 as *mut c_void, ptr::null(), 300, k, m), 0);
            assert!(!(*k).is_null() && !(*m).is_null());
            ee_input_mouse_resume_window(*m);
            ee_input_keyboard_refresh_now(*k);
            ee_input_mouse_refresh_now(*m);
            ee_input_shutdown(false);
            assert!((*k).is_null() && (*m).is_null());
            ee_input_shutdown(false); // nothing left: only the devices again
        }
        assert_eq!(*RELATIVE_CALLS.lock().unwrap(), vec![(0x40, 1), (0x40, 0), (0x40, 1), (0x40, 0)]);
        ee_input_set_double_press_delay(250);
    }

    #[test]
    fn the_key_names_come_from_the_strings_and_a_missing_one_repeats() {
        let names = key_names(100);
        assert_eq!(names.len(), crate::input::devices::KEY_CODES.len());
        for (i, n) in names.iter().enumerate() {
            assert_eq!(n.last(), Some(&0), "NUL-terminated");
            match crate::util::strings::resource_string_for_get(100 + i as u16) {
                Some(text) => assert_eq!(*n, text),
                None if i > 0 => assert_eq!(*n, names[i - 1]),
                None => assert_eq!(*n, [0]),
            }
        }
        // past the table: every name is the empty first one
        assert!(key_names(60000).iter().all(|n| *n == [0]));
    }
}
