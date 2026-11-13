//! The Input System's device buffers (`include/lle/ISDeviceInput.h`): what the keyboard and the mouse read from
//! DirectInput 7 before the port, with its semantics, filled by the window thread's events (`ISHandleEvent`) and read
//! by the game threads (`ISKeyboard`, `ISMouse`). The whole class lives here (LLE lane round 1); C++ keeps the record
//! structs, a one-line `ISHandleEvent` and calls these functions through an opaque device pointer.
//!
//! A device holds, under one lock:
//! - a buffer of up to 256 records (DirectInput's `DIPROP_BUFFERSIZE`) taken while it is acquired, read oldest first;
//!   records that did not fit are reported once (`DI_BUFFEROVERFLOW`);
//! - the keyboard's 256 key bytes (0x80 held, by `DIK_*` scan code);
//! - the mouse's motion since the last state read (X, Y, the wheel Z in `WHEEL_DELTA` units) and its four buttons.
//!
//! `golden_input.txt` is the trace of the original C++ class (`tests/input/is_device_golden.cpp`); `golden.rs`
//! replays it.

pub mod ffi;
#[cfg(test)]
mod golden;

use std::collections::VecDeque;
use std::os::raw::{c_long, c_ulong};
use std::sync::Mutex;

/// `GetData`'s results (`kISInput*`: `DI_OK`, `DI_BUFFEROVERFLOW`, `DIERR_NOTACQUIRED`).
pub(crate) const INPUT_OK: i32 = 0;
pub(crate) const INPUT_OVERFLOW: i32 = 1;
pub(crate) const INPUT_NOT_ACQUIRED: i32 = -1;

/// DirectInput's mouse data offsets (the `DIMOUSESTATE` field offsets, `kISMouseOffset*`).
pub(crate) const MOUSE_OFFSET_X: c_ulong = 0;
pub(crate) const MOUSE_OFFSET_Y: c_ulong = 4;
pub(crate) const MOUSE_OFFSET_Z: c_ulong = 8;
pub(crate) const MOUSE_OFFSET_BUTTON0: c_ulong = 12;

/// The record buffer's size (`DIPROP_BUFFERSIZE`).
const BUFFER_SIZE: usize = 256;

/// A buffered record (`ISInputRecord`: `DIDEVICEOBJECTDATA`'s two fields the engine reads).
/// The fields keep DirectInput's names: C++ uses this struct as `ISInputRecord`.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeInputRecord {
    /// The `DIK_*` code (keyboard) or the `DIMOFS_*` offset (mouse).
    pub dwOfs: c_ulong,
    /// 0x80 / 0 for a press / release, the motion for an axis.
    pub dwData: c_ulong,
}

/// The mouse state (`ISMouseState`, DirectInput 7's `DIMOUSESTATE`: the axes are `long`; `ISMouseAxis` points at
/// them). The fields keep DirectInput's names: C++ uses this struct as `ISMouseState`.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeInputMouseState {
    pub lX: c_long,
    pub lY: c_long,
    pub lZ: c_long,
    pub rgbButtons: [u8; 4],
}

struct State {
    acquired: bool,
    /// Records were dropped since the last `get_data` (the buffer was full).
    lost: bool,
    records: VecDeque<EeInputRecord>,
    keys: [u8; 256],
    motion: [c_long; 3],
    buttons: [u8; 4],
}

impl State {
    fn push(&mut self, offset: c_ulong, data: c_ulong) {
        if !self.acquired {
            return;
        }
        if self.records.len() >= BUFFER_SIZE {
            self.lost = true;
            return;
        }
        self.records.push_back(EeInputRecord { dwOfs: offset, dwData: data });
    }
}

/// One device (`ISDeviceInput`): the system keyboard's or the system mouse's.
pub struct Device {
    state: Mutex<State>,
}

impl Default for Device {
    fn default() -> Self {
        Self::new()
    }
}

impl Device {
    pub fn new() -> Self {
        Device {
            state: Mutex::new(State {
                acquired: false,
                lost: false,
                records: VecDeque::new(),
                keys: [0; 256],
                motion: [0; 3],
                buttons: [0; 4],
            }),
        }
    }

    fn lock(&self) -> std::sync::MutexGuard<'_, State> {
        self.state.lock().unwrap_or_else(|e| e.into_inner())
    }

    /// A newly acquired device starts with an empty buffer and no motion.
    pub fn acquire(&self) {
        let mut s = self.lock();
        if !s.acquired {
            s.records.clear();
            s.lost = false;
            s.motion = [0; 3];
        }
        s.acquired = true;
    }

    pub fn unacquire(&self) {
        self.lock().acquired = false;
    }

    pub fn is_acquired(&self) -> bool {
        self.lock().acquired
    }

    /// Takes up to `count` records, oldest first, into `out` (`None` discards them) -> (result, how many).
    pub fn get_data(&self, mut out: Option<&mut [EeInputRecord]>, count: c_ulong) -> (i32, c_ulong) {
        let mut s = self.lock();
        if !s.acquired {
            return (INPUT_NOT_ACQUIRED, 0);
        }
        let mut n: c_ulong = 0;
        while n < count {
            let Some(record) = s.records.pop_front() else { break };
            if let Some(out) = out.as_deref_mut() {
                out[n as usize] = record;
            }
            n += 1;
        }
        let result = if s.lost { INPUT_OVERFLOW } else { INPUT_OK };
        s.lost = false;
        (result, n)
    }

    /// The keyboard's 256 key bytes into `out` (untouched when not acquired).
    pub fn get_key_state(&self, out: &mut [u8; 256]) -> i32 {
        let s = self.lock();
        if !s.acquired {
            return INPUT_NOT_ACQUIRED;
        }
        *out = s.keys;
        INPUT_OK
    }

    /// The motion since the last call and the buttons into `out` (untouched when not acquired); the motion restarts.
    pub fn get_mouse_state(&self, out: &mut EeInputMouseState) -> i32 {
        let mut s = self.lock();
        if !s.acquired {
            return INPUT_NOT_ACQUIRED;
        }
        out.lX = s.motion[0];
        out.lY = s.motion[1];
        out.lZ = s.motion[2];
        out.rgbButtons = s.buttons;
        s.motion = [0; 3];
        INPUT_OK
    }

    /// A key by its scan code (1..=0xff); only a change is recorded (no auto-repeat). The key bytes change even
    /// when the device is not acquired.
    pub fn key(&self, scan_code: c_ulong, down: bool) {
        if scan_code == 0 || scan_code > 0xff {
            return;
        }
        let mut s = self.lock();
        let state = if down { 0x80 } else { 0 };
        if s.keys[scan_code as usize] != state {
            s.keys[scan_code as usize] = state;
            s.push(scan_code, state as c_ulong);
        }
    }

    pub fn mouse_move(&self, dx: c_long, dy: c_long) {
        let mut s = self.lock();
        if s.acquired {
            s.motion[0] = s.motion[0].wrapping_add(dx);
            s.motion[1] = s.motion[1].wrapping_add(dy);
            if dx != 0 {
                s.push(MOUSE_OFFSET_X, dx as c_ulong);
            }
            if dy != 0 {
                s.push(MOUSE_OFFSET_Y, dy as c_ulong);
            }
        }
    }

    pub fn mouse_wheel(&self, delta: c_long) {
        let mut s = self.lock();
        if s.acquired && delta != 0 {
            s.motion[2] = s.motion[2].wrapping_add(delta);
            s.push(MOUSE_OFFSET_Z, delta as c_ulong);
        }
    }

    /// A button in DirectInput's order (0 left, 1 right, 2 middle, 3 side); others are ignored. The button bytes
    /// change even when the device is not acquired.
    pub fn mouse_button(&self, button: c_ulong, down: bool) {
        if button > 3 {
            return;
        }
        let mut s = self.lock();
        let state = if down { 0x80 } else { 0 };
        if s.buttons[button as usize] != state {
            s.buttons[button as usize] = state;
            s.push(MOUSE_OFFSET_BUTTON0 + button, state as c_ulong);
        }
    }
}

/// `PlatInputEventType` values `handle_event` reads (`include/platform/PlatWindow.h`).
pub(crate) mod event {
    pub(crate) const FOCUS_LOST: u32 = 5;
    pub(crate) const KEY_DOWN: u32 = 9;
    pub(crate) const KEY_UP: u32 = 10;
    pub(crate) const MOUSE_MOVE: u32 = 13;
    pub(crate) const MOUSE_DOWN: u32 = 14;
    pub(crate) const MOUSE_UP: u32 = 15;
    pub(crate) const MOUSE_WHEEL: u32 = 16;
}

/// `(long)lrintf(value)` on x86-64: round half to even; out of range, the conversion's indefinite value (the most
/// negative `long`: 32-bit on Windows, 64-bit on LP64 systems). (arm64 saturates instead: a wheel of more than
/// 17 million notches, which no device reports.)
pub(crate) fn lrintf_long(value: f32) -> c_long {
    let rounded = value.round_ties_even();
    let limit = (c_long::MAX as f32).abs(); // 2^31 or 2^63, exact as a float
    if rounded >= -limit && rounded < limit {
        rounded as c_long
    } else {
        c_long::MIN
    }
}

/// One window event as `ISHandleEvent` gets it: keys by their `DIK_*` scan code, the mouse's raw motion, buttons
/// (`PlatWindow.h`'s 1 left, 2 middle, 3 right, 4 side -> DirectInput's 0 left, 1 right, 2 middle, 3 side), the wheel
/// (`WHEEL_DELTA` 120 a notch); a focus loss lets go of what is held. Only the fields of the event's type are read.
#[allow(clippy::too_many_arguments)]
pub fn handle_event(
    keyboard: Option<&Device>,
    mouse: Option<&Device>,
    kind: u32,
    scan_code: u32,
    dx: i32,
    dy: i32,
    button: u32,
    wheel: f32,
) {
    match kind {
        event::KEY_DOWN | event::KEY_UP => {
            if let Some(k) = keyboard {
                k.key(scan_code as c_ulong, kind == event::KEY_DOWN);
            }
        }
        event::MOUSE_MOVE => {
            if let Some(m) = mouse {
                m.mouse_move(dx as c_long, dy as c_long);
            }
        }
        event::MOUSE_DOWN | event::MOUSE_UP => {
            const BUTTONS: [c_ulong; 6] = [4, 0, 2, 1, 3, 4];
            let b = if button < 6 { BUTTONS[button as usize] } else { 4 };
            if let Some(m) = mouse {
                m.mouse_button(b, kind == event::MOUSE_DOWN);
            }
        }
        event::MOUSE_WHEEL => {
            if let Some(m) = mouse {
                m.mouse_wheel(lrintf_long(wheel * 120.0));
            }
        }
        event::FOCUS_LOST => {
            if let Some(k) = keyboard {
                for i in 1..256 {
                    k.key(i, false);
                }
            }
            if let Some(m) = mouse {
                for i in 0..4 {
                    m.mouse_button(i, false);
                }
            }
        }
        _ => {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn buffer_overflows_once() {
        let d = Device::new();
        d.acquire();
        for _ in 0..300 {
            d.mouse_move(1, 0);
        }
        let mut out = [EeInputRecord::default(); 300];
        assert_eq!(d.get_data(Some(&mut out), 300), (INPUT_OVERFLOW, 256));
        assert_eq!(d.get_data(Some(&mut out), 300), (INPUT_OK, 0));
        let mut m = EeInputMouseState::default();
        assert_eq!(d.get_mouse_state(&mut m), INPUT_OK);
        assert_eq!(m.lX, 300);
        assert_eq!(d.get_mouse_state(&mut m), INPUT_OK);
        assert_eq!(m.lX, 0);
    }

    #[test]
    fn keys_change_unacquired_but_record_nothing() {
        let d = Device::new();
        d.key(0x1d, true);
        d.key(0x100, true);
        d.key(0, true);
        d.acquire();
        let mut keys = [0u8; 256];
        assert_eq!(d.get_key_state(&mut keys), INPUT_OK);
        assert_eq!(keys[0x1d], 0x80);
        assert_eq!(keys.iter().filter(|&&k| k != 0).count(), 1);
        assert_eq!(d.get_data(None, 10), (INPUT_OK, 0));
        d.key(0x1d, true); // no change: no record
        d.key(0x1d, false);
        let mut out = [EeInputRecord::default(); 4];
        assert_eq!(d.get_data(Some(&mut out), 4), (INPUT_OK, 1));
        assert_eq!(out[0], EeInputRecord { dwOfs: 0x1d, dwData: 0 });
    }

    #[test]
    fn not_acquired_reads() {
        let d = Device::new();
        let mut keys = [7u8; 256];
        assert_eq!(d.get_key_state(&mut keys), INPUT_NOT_ACQUIRED);
        assert_eq!(keys[0], 7);
        assert_eq!(d.get_data(None, 5), (INPUT_NOT_ACQUIRED, 0));
    }

    #[test]
    fn events_map_buttons_and_wheel() {
        let k = Device::new();
        let m = Device::new();
        m.acquire();
        handle_event(Some(&k), Some(&m), event::MOUSE_DOWN, 0, 0, 0, 3, 0.0); // right -> 1
        handle_event(Some(&k), Some(&m), event::MOUSE_WHEEL, 0, 0, 0, 0, 2.5);
        handle_event(Some(&k), Some(&m), event::MOUSE_WHEEL, 0, 0, 0, 0, 0.0125); // 1.5 -> 2
        let mut out = [EeInputRecord::default(); 4];
        assert_eq!(m.get_data(Some(&mut out), 4), (INPUT_OK, 3));
        assert_eq!(out[0], EeInputRecord { dwOfs: 13, dwData: 0x80 });
        assert_eq!(out[1], EeInputRecord { dwOfs: 8, dwData: 300 });
        assert_eq!(out[2], EeInputRecord { dwOfs: 8, dwData: 2 });
        handle_event(Some(&k), Some(&m), event::FOCUS_LOST, 0, 0, 0, 0, 0.0);
        assert_eq!(m.get_data(Some(&mut out), 4), (INPUT_OK, 1));
        assert_eq!(out[0], EeInputRecord { dwOfs: 13, dwData: 0 });
    }

    #[test]
    fn lrintf_out_of_range_is_indefinite() {
        assert_eq!(lrintf_long(2.5), 2);
        assert_eq!(lrintf_long(-0.5), 0);
        assert_eq!(lrintf_long(f32::INFINITY), c_long::MIN);
        assert_eq!(lrintf_long(f32::NAN), c_long::MIN);
    }
}
