//! The debug sound panel (form 22) whole: the original `EEDSUIEvents.cpp` (its ten volume sliders' events, each
//! setting one sound of the first selected unit from its slider and showing the value, and the panel's close button)
//! over a host ([`DebugSoundHost`]) whose calls are each one game or toolkit call (`UI Events/EEScreens.cpp`).

use core::ffi::{c_long, c_ulong};

use crate::menus::EE_SCREENS_OP_POP;
use crate::settings_form::EE_SCREENS_OP_ENTITY_RANGE;

// ---- the events (EEScreenEvent::mEvent)

/// A slider's event: the first selected unit's sound of kind `event - EE_SCREENS_DSU_ADJUST + 1` (1, 2 its class's
/// acknowledge sounds, 3 creation, 4 death, 5, 6 selection; 7..10 its model's first and last sounds of its two arrays).
pub const EE_SCREENS_DSU_ADJUST: u32 = 0x1601;
/// The last kind's event (`EE_SCREENS_DSU_ADJUST + 9`).
pub const EE_SCREENS_DSU_ADJUST_LAST: u32 = 0x160a;
/// The close button (`EEDSUIHideDebugSoundPanel`): the top form popped.
pub const EE_SCREENS_DSU_HIDE: u32 = 0x160b;

// ---- the host's ops (ee_screenscpp_call)

/// (kind) -> 0 without a first selected unit or its complex unit, 1 when it has no sound of the kind, 2 when it has
/// one (kept for `EE_SCREENS_GAME_DSU_VOLUME`).
pub const EE_SCREENS_GAME_DSU_SOUND: u32 = 0x2ba;
/// -> 1 when the event's entity is a slider.
pub const EE_SCREENS_OP_DSU_SLIDER: u32 = 0x2bb;
/// (volume bits) the kept sound's `SetVolume`.
pub const EE_SCREENS_GAME_DSU_VOLUME: u32 = 0x2bc;
/// Put (control id): the debug sound form's control of that id, when it is a label, `SetText`.
pub const EE_SCREENS_TEXT_DSU_LABEL: u32 = 0x82;

/// The panel's value labels by sound kind (the original's `.rdata` table; kinds 1..10 read entries 1..10).
const DSU_VALUE_LABELS: [u32; 11] = [0x25c, 0x25e, 0x245, 0x248, 0x24b, 0x24e, 0x251, 0x254, 0x257, 0x25a, 0x25d];
/// `ProcessEvent`'s failure result.
const DSU_FAILED: c_long = 3;

/// The calls of the debug sound panel.
pub trait DebugSoundHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with one argument and an answer.
    fn call(&mut self, op: u32, a: u32) -> u32;
    /// A text into the game.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]);
}

/// The debug sound panel's event code for a control event id (107..116 the sliders, 117 the close button), 0 for
/// another form's.
pub fn event_for_control(id: c_ulong) -> u32 {
    const SLIDERS: [c_ulong; 10] = [107, 108, 109, 110, 111, 112, 113, 114, 115, 116];
    match SLIDERS.iter().position(|&slider| slider == id) {
        Some(k) => EE_SCREENS_DSU_ADJUST + k as u32,
        None if id == 117 => EE_SCREENS_DSU_HIDE,
        None => 0,
    }
}

/// `EEDSUIAdjustSound`: the first selected unit's sound of `kind` at the slider's volume (the slider runs from loud,
/// 0, to quiet, 100), the value shown in the kind's label; false when there is no unit, no slider or no sound.
fn adjust<H: DebugSoundHost + ?Sized>(h: &mut H, kind: u32) -> bool {
    let found = h.call(EE_SCREENS_GAME_DSU_SOUND, kind);
    if found == 0 || h.call(EE_SCREENS_OP_DSU_SLIDER, 0) == 0 {
        return false;
    }
    let value = 100i32.wrapping_sub(h.call(EE_SCREENS_OP_ENTITY_RANGE, 0) as i32);
    if found != 2 {
        return false;
    }
    h.call(EE_SCREENS_GAME_DSU_VOLUME, (value as f32 * 0.01f32).to_bits());
    h.put_text(EE_SCREENS_TEXT_DSU_LABEL, DSU_VALUE_LABELS[kind as usize], value.to_string().as_bytes());
    true
}

/// One of the debug sound panel's events; `None` for another form's, else its `ProcessEvent` result (0, or 3 when a
/// slider's event changed nothing).
pub fn dispatch<H: DebugSoundHost + ?Sized>(h: &mut H, event: u32) -> Option<c_long> {
    match event {
        EE_SCREENS_DSU_ADJUST..=EE_SCREENS_DSU_ADJUST_LAST => {
            Some(if adjust(h, event - EE_SCREENS_DSU_ADJUST + 1) { 0 } else { DSU_FAILED })
        }
        EE_SCREENS_DSU_HIDE => {
            h.call(EE_SCREENS_OP_POP, 0);
            Some(0)
        }
        _ => None,
    }
}

#[cfg(test)]
mod golden;

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn events() {
        assert_eq!(event_for_control(107), EE_SCREENS_DSU_ADJUST);
        assert_eq!(event_for_control(116), EE_SCREENS_DSU_ADJUST_LAST);
        assert_eq!(event_for_control(117), EE_SCREENS_DSU_HIDE);
        assert_eq!(event_for_control(106), 0);
        assert_eq!(event_for_control(118), 0);
    }
}
