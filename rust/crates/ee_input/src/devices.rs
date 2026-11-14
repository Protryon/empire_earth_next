//! The Input System's keyboard and mouse with their entities (`ISKeyboard`, `ISMouse`, `ISKey`, `ISExtendedKey`,
//! `ISMouseButton`, `ISMouseAxis` before; LLE lane round 2): what the game reads each frame from the device buffers
//! (`Device`). The keyboard keeps, per `DIK_*` scan code, a transition count whose bit 0 is "held" and the count of
//! the previous refresh, so a key's presses and releases since then are the counts' difference; the mouse keeps the
//! same for its four buttons, and its motion. A press within the double-press delay of the previous one (+200 ms
//! for the keys) is a double press.
//!
//! Entities: the keys the keyboard created (121, by scan code), the three either-side keys 0xe0..0xe2 (Control,
//! Shift, Alt: a left/right pair), and the mouse's X, Y, Z axes and four buttons (0..6). C++ holds them as
//! `ISEntity*`, pointers to the immutable records of `ffi.rs`; their state is the system keyboard's or mouse's.

use super::{Device, EeInputMouseState, EeInputRecord, INPUT_NOT_ACQUIRED, INPUT_OK, INPUT_OVERFLOW};
use std::ffi::{CStr, CString};
use std::os::raw::{c_long, c_ulong};

include!("keytable.rs");

/// The engine's error codes the devices return (`kErrorOK`, `kErrorBeyondEnd`).
pub(crate) const ERROR_OK: c_long = 0;
pub(crate) const ERROR_BEYOND_END: c_long = 0xa;

/// The either-side keys: (index, left key, right key) for Control, Shift and Alt.
pub(crate) const EXTENDED: [(usize, usize, usize); 3] = [(0xe0, 0x1d, 0x9d), (0xe1, 0x2a, 0x36), (0xe2, 0x38, 0xb8)];

/// The modifier bit of a scan code (1 Shift, 2 Control, 4 Alt; the either-side keys count too).
pub fn modifier_state(key: c_ulong) -> c_ulong {
    let mut state = 0;
    if key == 0x2a || key == 0x36 || key == 0xe1 {
        state = 1;
    }
    if key == 0x38 || key == 0xb8 || key == 0xe2 {
        state |= 4;
    }
    if key == 0x1d || key == 0x9d || key == 0xe0 {
        state |= 2;
    }
    state
}

/// Presses since the last refresh from a transition count and its previous value.
fn pressed_amount(current: u8, last: u8) -> c_ulong {
    let changes = current.wrapping_sub(last);
    let n = if changes & 1 == 0 {
        changes / 2
    } else if current & 1 != 0 {
        (changes - 1) / 2 + 1
    } else {
        (changes - 1) / 2
    };
    n as c_ulong
}

/// Releases since the last refresh.
fn unpressed_amount(current: u8, last: u8) -> c_ulong {
    let changes = current.wrapping_sub(last);
    let n = if changes & 1 == 0 {
        changes / 2
    } else if current & 1 == 0 {
        (changes - 1) / 2 + 1
    } else {
        (changes - 1) / 2
    };
    n as c_ulong
}

/// A press within `window` ms of the previous one is a double press (and starts no new pair).
fn check_double(pressed: bool, time: c_ulong, window: c_ulong, last_press: &mut c_ulong, double: &mut bool) {
    if pressed {
        if time.wrapping_sub(*last_press) < window {
            *last_press = 0;
            *double = true;
        } else {
            *double = false;
            *last_press = time;
        }
    } else {
        *double = false;
    }
}

fn get_data(dev: Option<&Device>, out: Option<&mut [EeInputRecord]>, count: c_ulong) -> (i32, c_ulong) {
    match dev {
        Some(d) => d.get_data(out, count),
        None => (INPUT_NOT_ACQUIRED, 0),
    }
}

/// What a keyboard index holds.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum KeyKind {
    None,
    Key,
    /// An either-side key over its left and right keys.
    Extended(usize, usize),
}

/// The system keyboard (`ISKeyboard`).
pub struct Keyboard {
    names: Vec<Option<CString>>,
    pub(crate) status: [u8; 256],
    pub(crate) last: [u8; 256],
    pub(crate) modifiers: c_ulong,
    pub(crate) keys_down: c_long,
    pub(crate) last_keys_down: c_long,
    /// Events were read on the last refresh (the original's name: set on any read, not only an overflow).
    pub(crate) overflowed: bool,
    suspended: bool,
    pub(crate) press_time: [c_ulong; 256],
    pub(crate) double: [bool; 256],
}

impl Keyboard {
    /// The constructor: the keys named by `names` (in `KEY_CODES`' order; `None`: the English table), resumed (the
    /// state read), then the either-side keys "<left> and <right>"; left suspended.
    pub fn new(names: Option<&[CString]>, dev: Option<&Device>, time: c_ulong, window: c_ulong) -> Keyboard {
        let mut k = Keyboard {
            names: vec![None; 256],
            status: [0; 256],
            last: [0; 256],
            modifiers: 0,
            keys_down: 0,
            last_keys_down: 0,
            overflowed: false,
            suspended: false,
            press_time: [0; 256],
            double: [false; 256],
        };
        for (i, &code) in KEY_CODES.iter().enumerate() {
            let name = match names {
                Some(n) => n.get(i).cloned().unwrap_or_default(),
                None => CString::new(KEY_NAMES[i]).unwrap_or_default(),
            };
            k.names[code as usize] = Some(name);
        }
        k.resume(dev, time, window);
        k.status = [0; 256];
        k.last = [0; 256];
        for (index, left, right) in EXTENDED {
            let mut name = k.names[left].as_ref().map(|n| n.as_bytes().to_vec()).unwrap_or_default();
            name.extend_from_slice(b" and ");
            name.extend_from_slice(k.names[right].as_ref().map(|n| n.as_bytes()).unwrap_or_default());
            k.names[index] = Some(CString::new(name).unwrap_or_default());
        }
        k.overflowed = false;
        k.suspend(dev);
        k
    }

    pub fn kind(&self, index: usize) -> KeyKind {
        match EXTENDED.iter().find(|e| e.0 == index) {
            Some(&(_, left, right)) => KeyKind::Extended(left, right),
            None if index < 256 && self.names[index].is_some() => KeyKind::Key,
            None => KeyKind::None,
        }
    }

    pub fn name(&self, index: usize) -> Option<&CStr> {
        self.names.get(index)?.as_deref()
    }

    /// `GetEntity`: (result, whether an entity is there).
    pub fn entity(&self, index: c_ulong) -> (c_long, bool) {
        if index <= 255 {
            (ERROR_OK, self.kind(index as usize) != KeyKind::None)
        } else {
            (ERROR_BEYOND_END, false)
        }
    }

    pub fn modifiers(&self) -> c_ulong {
        self.modifiers
    }

    pub fn should_update(&self) -> bool {
        self.overflowed || self.keys_down > 0 || self.last_keys_down > 0
    }

    pub fn is_suspended(&self) -> bool {
        self.suspended
    }

    fn check_double_press(&mut self, index: usize, time: c_ulong, window: c_ulong) {
        if self.kind(index) == KeyKind::Key {
            let pressed = self.pressed_amount(index) > 0;
            check_double(pressed, time, window.wrapping_add(200), &mut self.press_time[index], &mut self.double[index]);
        }
    }

    /// `Refresh`: the buffered key events since the last call.
    pub fn refresh(&mut self, dev: Option<&Device>, time: c_ulong, window: c_ulong) {
        self.last_keys_down = self.keys_down;
        self.last = self.status;
        if self.overflowed {
            for j in 0..256 {
                // (the original cast every entity to a key here and wrote into the either-side keys' members)
                if self.kind(j) == KeyKind::Key && self.double[j] {
                    self.double[j] = false;
                    self.press_time[j] = 0;
                }
            }
        }
        self.overflowed = false;
        let mut data = [EeInputRecord::default(); 256];
        let (result, count) = get_data(dev, Some(&mut data), 256);
        if result == INPUT_NOT_ACQUIRED {
            return;
        }
        if result == INPUT_OVERFLOW {
            self.overflowed = true;
            self.recompute(dev, time, window);
        } else if count > 0 {
            self.overflowed = true;
            for record in &data[..count as usize] {
                let ofs = record.dwOfs as usize;
                if ofs > 255 {
                    continue; // the keyboard's device records scan codes 1..=0xff only
                }
                let modifier = modifier_state(record.dwOfs);
                if record.dwData & 0x80 != 0 {
                    if self.status[ofs] & 1 == 0 {
                        self.keys_down = self.keys_down.wrapping_add(1);
                        self.status[ofs] = self.status[ofs].wrapping_add(1);
                    }
                    if ofs < 0xdd {
                        self.modifiers |= modifier;
                    }
                } else {
                    if self.status[ofs] & 1 != 0 {
                        self.keys_down = self.keys_down.wrapping_sub(1);
                        self.status[ofs] = self.status[ofs].wrapping_add(1);
                    }
                    if ofs < 0xdd {
                        self.modifiers ^= modifier;
                    }
                }
                self.check_double_press(ofs, time, window);
            }
        }
    }

    /// Rereads the whole state (the buffered events discarded, the modifiers reset).
    fn recompute(&mut self, dev: Option<&Device>, time: c_ulong, window: c_ulong) {
        let mut state = [0u8; 256];
        self.status = [0; 256];
        self.last = [0; 256];
        self.modifiers = 0;
        let Some(d) = dev else { return };
        if d.get_key_state(&mut state) != INPUT_OK {
            return;
        }
        d.get_data(None, c_ulong::MAX);
        self.keys_down = 0;
        for (i, &s) in state.iter().enumerate() {
            if s & 0x80 == 0x80 {
                self.status[i] = 1;
                self.keys_down = self.keys_down.wrapping_add(1);
                if i < 0xdd {
                    self.modifiers |= modifier_state(i as c_ulong);
                }
            } else {
                self.status[i] = 0;
            }
            self.last[i] = self.status[i];
            self.check_double_press(i, time, window);
        }
    }

    pub fn resume(&mut self, dev: Option<&Device>, time: c_ulong, window: c_ulong) {
        if let Some(d) = dev {
            d.acquire();
        }
        self.suspended = false;
        self.recompute(dev, time, window);
    }

    pub fn suspend(&mut self, dev: Option<&Device>) {
        if let Some(d) = dev {
            d.unacquire();
        }
        self.suspended = true;
    }

    // ---- the entities, by index (an absent one reads as released)

    pub fn value(&self, index: usize) -> c_long {
        match self.kind(index) {
            KeyKind::Key => (self.status[index] & 1) as c_long,
            KeyKind::Extended(l, r) => self.value(l) | self.value(r),
            KeyKind::None => 0,
        }
    }

    pub fn normalized_value(&self, index: usize) -> c_long {
        match self.kind(index) {
            KeyKind::Extended(l, r) => self.normalized_value(l) | self.normalized_value(r),
            _ => {
                if self.value(index) != 0 {
                    0x7fff
                } else {
                    0
                }
            }
        }
    }

    pub fn pressed_amount(&self, index: usize) -> c_ulong {
        match self.kind(index) {
            KeyKind::Key => pressed_amount(self.status[index], self.last[index]),
            KeyKind::Extended(..) => self.just_pressed(index) as c_ulong,
            KeyKind::None => 0,
        }
    }

    pub fn unpressed_amount(&self, index: usize) -> c_ulong {
        match self.kind(index) {
            KeyKind::Key => unpressed_amount(self.status[index], self.last[index]),
            KeyKind::Extended(..) => self.just_unpressed(index) as c_ulong,
            KeyKind::None => 0,
        }
    }

    pub fn just_pressed(&self, index: usize) -> bool {
        match self.kind(index) {
            KeyKind::Extended(l, r) => self.just_pressed(l) || self.just_pressed(r),
            _ => self.pressed_amount(index) > 0,
        }
    }

    pub fn just_unpressed(&self, index: usize) -> bool {
        match self.kind(index) {
            KeyKind::Extended(l, r) => self.just_unpressed(l) || self.just_unpressed(r),
            _ => self.unpressed_amount(index) > 0,
        }
    }

    /// A key's double press; never an either-side key's (the original's pair held copies of its two keys made at
    /// construction, whose double-press flags nothing updated).
    pub fn just_double_pressed(&self, index: usize) -> bool {
        self.kind(index) == KeyKind::Key && self.double[index]
    }
}

/// The mouse's entities: the X, Y, Z axes, then the left, right, middle and side buttons.
pub(crate) const MOUSE_ENTITY_COUNT: usize = 7;
const MOUSE_NAMES: [&CStr; MOUSE_ENTITY_COUNT] =
    [c"X Axis", c"Y Axis", c"Z Axis", c"Left button", c"Right button", c"Middle button", c"Other button"];
/// The axes' granularity (DirectInput's: 1 for X and Y, `WHEEL_DELTA` for the wheel).
const AXIS_GRANULARITY: [c_long; 3] = [1, 1, 120];

/// The system mouse (`ISMouse`).
pub struct Mouse {
    pub(crate) buttons: [u8; 4],
    pub(crate) last_buttons: [u8; 4],
    pub(crate) state: EeInputMouseState,
    pub(crate) last_state: EeInputMouseState,
    pub(crate) overflowed: bool,
    use_buffered: bool,
    suspended: bool,
    pub(crate) press_time: [c_ulong; 4],
    pub(crate) double: [bool; 4],
}

impl Mouse {
    /// The constructor: resumed (the state read), the buttons cleared, left suspended.
    pub fn new(dev: Option<&Device>) -> Mouse {
        let mut m = Mouse {
            buttons: [0; 4],
            last_buttons: [0; 4],
            state: EeInputMouseState::default(),
            last_state: EeInputMouseState::default(),
            overflowed: false,
            use_buffered: true,
            suspended: false,
            press_time: [0; 4],
            double: [false; 4],
        };
        m.resume(dev);
        m.buttons = [0; 4];
        m.last_buttons = [0; 4];
        m.overflowed = false;
        m.suspend(dev);
        m
    }

    pub fn name(index: usize) -> Option<&'static CStr> {
        MOUSE_NAMES.get(index).copied()
    }

    /// `GetEntity`: (result, whether an entity is there).
    pub fn entity(index: c_ulong) -> (c_long, bool) {
        if index < MOUSE_ENTITY_COUNT as c_ulong {
            (ERROR_OK, true)
        } else {
            (ERROR_BEYOND_END, false)
        }
    }

    pub fn should_update(&self) -> bool {
        self.overflowed
            || self.buttons != self.last_buttons
            || self.state.lX != 0
            || self.state.lY != 0
            || self.state.lZ != 0
    }

    pub fn set_use_buffered_movement(&mut self, used: bool) {
        self.use_buffered = used;
    }

    pub fn is_suspended(&self) -> bool {
        self.suspended
    }

    /// Clears the state, then reads the device's buttons and motion directly and flushes its buffer.
    fn recompute(&mut self, dev: Option<&Device>) {
        self.state = EeInputMouseState::default();
        self.last_state = EeInputMouseState::default();
        self.buttons = [0; 4];
        self.last_buttons = [0; 4];
        let Some(d) = dev else { return };
        if d.get_mouse_state(&mut self.state) != INPUT_OK {
            return;
        }
        d.get_data(None, c_ulong::MAX);
        for i in 0..4 {
            self.buttons[i] = (self.state.rgbButtons[i] & 0x80 == 0x80) as u8;
        }
        if !self.use_buffered {
            self.state.lX = 0;
            self.state.lY = 0;
            self.state.lZ = 0;
        }
        self.last_buttons = self.buttons;
        self.last_state = self.state;
    }

    /// `Refresh`: the motion since the last call, the wheel and the buttons' presses and releases from the buffer.
    pub fn refresh(&mut self, dev: Option<&Device>, time: c_ulong, window: c_ulong) {
        self.last_state = self.state;
        self.last_buttons = self.buttons;
        let mut data = [EeInputRecord::default(); 256];
        let (result, count) = get_data(dev, Some(&mut data), 256);
        self.state.lX = 0;
        self.state.lY = 0;
        self.state.lZ = 0;
        self.overflowed = false;
        if result != INPUT_NOT_ACQUIRED && count > 0 {
            self.overflowed = true;
            if result == INPUT_OVERFLOW {
                self.recompute(dev);
            } else {
                let mut s = EeInputMouseState::default();
                if dev.map(|d| d.get_mouse_state(&mut s)) == Some(INPUT_OK) {
                    self.state.lX = s.lX;
                    self.state.lY = s.lY;
                }
                for record in &data[..count as usize] {
                    let value = record.dwData;
                    match record.dwOfs {
                        12..=15 => {
                            let b = (record.dwOfs - 12) as usize;
                            self.state.rgbButtons[b] = value as u8;
                            let held = self.buttons[b] & 1 != 0;
                            if (value & 0x80 == 0x80 && !held) || (value == 0 && held) {
                                self.buttons[b] = self.buttons[b].wrapping_add(1);
                            }
                        }
                        8 => self.state.lZ = self.state.lZ.wrapping_add(value as c_long),
                        _ => {}
                    }
                }
                if !self.use_buffered {
                    self.state.lX = 0;
                    self.state.lY = 0;
                    self.state.lZ = 0;
                }
            }
        }
        for b in 0..4 {
            let pressed = self.pressed_amount(3 + b) > 0;
            check_double(pressed, time, window, &mut self.press_time[b], &mut self.double[b]);
        }
    }

    /// `RefreshXY`: the motion since the last read (zero when buffered movement is off).
    pub fn refresh_xy(&mut self, dev: Option<&Device>) {
        let mut s = EeInputMouseState::default();
        if dev.map(|d| d.get_mouse_state(&mut s)) == Some(INPUT_OK) {
            self.state.lX = s.lX;
            self.state.lY = s.lY;
        }
        if !self.use_buffered {
            self.state.lX = 0;
            self.state.lY = 0;
        }
    }

    pub fn resume(&mut self, dev: Option<&Device>) {
        if let Some(d) = dev {
            d.acquire();
        }
        self.suspended = false;
        self.recompute(dev);
    }

    pub fn suspend(&mut self, dev: Option<&Device>) {
        if let Some(d) = dev {
            d.unacquire();
        }
        self.suspended = true;
    }

    // ---- the entities, by index 0..6

    pub fn value(&self, index: usize) -> c_long {
        match index {
            0 => self.state.lX,
            1 => self.state.lY,
            2 => self.state.lZ,
            3..=6 => (self.buttons[index - 3] & 1) as c_long,
            _ => 0,
        }
    }

    pub fn normalized_value(&self, index: usize) -> c_long {
        match index {
            0..=2 => self.value(index) / AXIS_GRANULARITY[index],
            _ => {
                if self.value(index) != 0 {
                    0x7fff
                } else {
                    0
                }
            }
        }
    }

    pub fn pressed_amount(&self, index: usize) -> c_ulong {
        match index {
            3..=6 => pressed_amount(self.buttons[index - 3], self.last_buttons[index - 3]),
            _ => 0,
        }
    }

    pub fn unpressed_amount(&self, index: usize) -> c_ulong {
        match index {
            3..=6 => unpressed_amount(self.buttons[index - 3], self.last_buttons[index - 3]),
            _ => 0,
        }
    }

    pub fn just_double_pressed(&self, index: usize) -> bool {
        matches!(index, 3..=6) && self.double[index - 3]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn key_table_is_unique_and_named() {
        let mut seen = [false; 256];
        for &c in &KEY_CODES {
            assert!(!seen[c as usize], "{c:#x} twice");
            seen[c as usize] = true;
            assert!(!(0xe0..=0xe2).contains(&c));
        }
        let k = Keyboard::new(None, None, 0, 250);
        assert_eq!(k.name(0x1e).unwrap().to_str().unwrap(), "A key");
        assert_eq!(k.name(0xe1).unwrap().to_str().unwrap(), "SHIFT key (left) and SHIFT key (right)");
        assert!(k.name(0).is_none());
        assert_eq!(k.entity(0), (ERROR_OK, false));
        assert_eq!(k.entity(256), (ERROR_BEYOND_END, false));
        assert_eq!(Mouse::entity(6), (ERROR_OK, true));
        assert_eq!(Mouse::entity(7), (ERROR_BEYOND_END, false));
    }

    #[test]
    fn resource_names_and_either_side_keys() {
        let names: Vec<CString> = (0..KEY_CODES.len()).map(|i| CString::new(format!("k{i}")).unwrap()).collect();
        let k = Keyboard::new(Some(&names), None, 0, 250);
        assert_eq!(k.name(0x01).unwrap().to_str().unwrap(), "k0");
        assert_eq!(k.name(0xe0).unwrap().to_str().unwrap(), "k28 and k102");
    }

    #[test]
    fn press_release_and_double_press() {
        let d = Device::new();
        let mut k = Keyboard::new(None, Some(&d), 0, 250);
        k.resume(Some(&d), 0, 250);
        d.key(0x2a, true);
        k.refresh(Some(&d), 1000, 250);
        assert!(k.just_pressed(0x2a) && k.just_pressed(0xe1) && k.value(0xe1) == 1);
        assert_eq!(k.modifiers(), 1);
        assert_eq!(k.normalized_value(0xe1), 0x7fff);
        d.key(0x2a, false);
        d.key(0x2a, true);
        k.refresh(Some(&d), 1300, 250);
        assert_eq!((k.pressed_amount(0x2a), k.unpressed_amount(0x2a)), (1, 1));
        assert!(k.just_double_pressed(0x2a) && !k.just_double_pressed(0xe1));
        k.refresh(Some(&d), 1400, 250);
        assert!(!k.just_double_pressed(0x2a) && k.value(0x2a) == 1 && k.should_update());
    }

    #[test]
    fn mouse_wheel_and_buttons() {
        let d = Device::new();
        let mut m = Mouse::new(Some(&d));
        m.resume(Some(&d));
        d.mouse_wheel(-240);
        d.mouse_move(3, -4);
        d.mouse_button(1, true);
        m.refresh(Some(&d), 10, 250);
        assert_eq!((m.value(0), m.value(1), m.value(2), m.normalized_value(2)), (3, -4, -240, -2));
        assert!(m.pressed_amount(4) == 1 && m.value(4) == 1 && m.should_update());
        m.set_use_buffered_movement(false);
        d.mouse_move(5, 5);
        m.refresh_xy(Some(&d));
        assert_eq!((m.value(0), m.value(1)), (0, 0));
    }
}
