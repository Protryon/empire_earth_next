//! The C ABI of the Input System's devices (`include/lle/ISDeviceInput.h`): the two system devices, the window
//! thread's events and the game threads' reads. A device pointer is valid from `ee_input_system_init` to
//! `ee_input_system_shutdown`; the pointer may be NULL in every read (the reads then report "not acquired").

use super::{handle_event, Device, EeInputMouseState, EeInputRecord, INPUT_NOT_ACQUIRED};
use std::os::raw::{c_ulong, c_void};
use std::ptr;
use std::sync::atomic::{AtomicPtr, Ordering};

/// An opaque device (`ISDeviceInput` before).
pub type EeInputDevice = c_void;

static KEYBOARD: AtomicPtr<Device> = AtomicPtr::new(ptr::null_mut());
static MOUSE: AtomicPtr<Device> = AtomicPtr::new(ptr::null_mut());

unsafe fn device<'a>(d: *const EeInputDevice) -> Option<&'a Device> {
    (d as *const Device).as_ref()
}

/// Creates the system keyboard's and mouse's devices (`ISInitialize`); a second call keeps the first ones.
#[no_mangle]
pub extern "C" fn ee_input_system_init() {
    for slot in [&KEYBOARD, &MOUSE] {
        let new = Box::into_raw(Box::new(Device::new()));
        if slot.compare_exchange(ptr::null_mut(), new, Ordering::AcqRel, Ordering::Acquire).is_err() {
            // SAFETY: `new` came from Box::into_raw above and was not published.
            drop(unsafe { Box::from_raw(new) });
        }
    }
}

/// The system device: 0 the keyboard's, 1 the mouse's (NULL before `ee_input_system_init` or after the shutdown).
#[no_mangle]
pub extern "C" fn ee_input_system_device(which: i32) -> *mut EeInputDevice {
    let slot = if which == 0 { &KEYBOARD } else { &MOUSE };
    slot.load(Ordering::Acquire) as *mut EeInputDevice
}

/// Destroys both system devices (`ISShutdown`, after the keyboard and the mouse that read them are gone). As in the
/// original, the window thread must not be inside `ee_input_handle_event` meanwhile.
#[no_mangle]
pub extern "C" fn ee_input_system_shutdown() {
    for slot in [&KEYBOARD, &MOUSE] {
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
    let keyboard = unsafe { device(KEYBOARD.load(Ordering::Acquire) as *const EeInputDevice) };
    let mouse = unsafe { device(MOUSE.load(Ordering::Acquire) as *const EeInputDevice) };
    handle_event(keyboard, mouse, kind, scan_code, dx, dy, button, wheel);
}

/// `Acquire`: the device buffers records from now on (a newly acquired device starts empty, with no motion).
///
/// # Safety
/// `d` is a system device or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_input_acquire(d: *const EeInputDevice) {
    if let Some(d) = device(d) {
        d.acquire();
    }
}

/// `Unacquire`.
///
/// # Safety
/// `d` is a system device or NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_input_unacquire(d: *const EeInputDevice) {
    if let Some(d) = device(d) {
        d.unacquire();
    }
}

/// `GetData`: up to `*io_count` records, oldest first, into `out` (NULL discards them); `*io_count` becomes how many
/// -> 0 OK, 1 overflow (records were lost since the last call), -1 not acquired.
///
/// # Safety
/// `d` is a system device or NULL; `io_count` is valid; `out` is NULL or holds `*io_count` records.
#[no_mangle]
pub unsafe extern "C" fn ee_input_get_data(
    d: *const EeInputDevice,
    out: *mut EeInputRecord,
    io_count: *mut c_ulong,
) -> i32 {
    let Some(d) = device(d) else {
        *io_count = 0;
        return INPUT_NOT_ACQUIRED;
    };
    let count = *io_count;
    let slice = if out.is_null() {
        None
    } else {
        // The caller's buffer holds `count` records; a huge count with a real buffer (the keyboard's discard passes
        // NULL with ~0) never reaches here.
        Some(std::slice::from_raw_parts_mut(out, count.min(1 << 20) as usize))
    };
    let (result, n) = d.get_data(slice, count);
    *io_count = n;
    result
}

/// `GetKeyState`: the keyboard's 256 key bytes into `out` (untouched when not acquired) -> 0 or -1.
///
/// # Safety
/// `d` is a system device or NULL; `out` holds 256 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_input_get_key_state(d: *const EeInputDevice, out: *mut u8) -> i32 {
    match device(d) {
        Some(d) => d.get_key_state(&mut *(out as *mut [u8; 256])),
        None => INPUT_NOT_ACQUIRED,
    }
}

/// `GetMouseState`: the motion since the last call and the buttons (untouched when not acquired) -> 0 or -1.
///
/// # Safety
/// `d` is a system device or NULL; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_input_get_mouse_state(d: *const EeInputDevice, out: *mut EeInputMouseState) -> i32 {
    match device(d) {
        Some(d) => d.get_mouse_state(&mut *out),
        None => INPUT_NOT_ACQUIRED,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn system_devices_live_between_init_and_shutdown() {
        ee_input_system_init();
        let k = ee_input_system_device(0);
        let m = ee_input_system_device(1);
        assert!(!k.is_null() && !m.is_null() && k != m);
        unsafe {
            ee_input_acquire(k);
            ee_input_handle_event(super::super::event::KEY_DOWN, 0x1e, 0, 0, 0, 0.0);
            let mut records = [EeInputRecord::default(); 8];
            let mut n: c_ulong = 8;
            assert_eq!(ee_input_get_data(k, records.as_mut_ptr(), &mut n), 0);
            assert_eq!(n, 1);
            assert_eq!(records[0], EeInputRecord { dwOfs: 0x1e, dwData: 0x80 });
            let mut n: c_ulong = c_ulong::MAX;
            assert_eq!(ee_input_get_data(k, ptr::null_mut(), &mut n), 0);
            assert_eq!(n, 0);
        }
        ee_input_system_shutdown();
        assert!(ee_input_system_device(0).is_null());
        let mut n: c_ulong = 3;
        assert_eq!(unsafe { ee_input_get_data(ptr::null(), ptr::null_mut(), &mut n) }, -1);
        assert_eq!(n, 0);
    }
}
