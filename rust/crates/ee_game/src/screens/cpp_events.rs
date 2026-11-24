//! The game UI's last C++ event classes as screen-event codes (UI lane round 16): `EEUIECancel`, `EEUIELoadUnit`,
//! `EEUIEMGHotKeyStatusButton`, `EEUIEProcessTimerExpired` and `EEUIEndCinematicModeEvent` are `EEScreenEvent`s
//! carrying two arguments (ee/EEScreenEvent.h); their ProcessEvent bodies are here over one-statement shims.

use core::ffi::c_long;

use crate::screens::game_cpp::Cpp;
use crate::screens::main_group_calls;

/// `EEUIECancel` (the save and load forms' Cancel): a the form to open.
pub const EE_SCREENS_EV_CANCEL: u32 = 0x7e00_0001;
/// `EEUIELoadUnit` (the main game form's debug unit reload): the unit hack of the form it runs on loads.
pub const EE_SCREENS_EV_LOAD_UNIT: u32 = 0x7e00_0002;
/// `EEUIEMGHotKeyStatusButton`: a the command button (`EEUIFMGStatusButton*`), b 1 pressed, 0 released.
pub const EE_SCREENS_EV_HOT_KEY: u32 = 0x7e00_0003;
/// `EEUIEProcessTimerExpired` (posted by `EEMCivTimerExpired::Execute`): `EEProcessTimerExpired`.
pub const EE_SCREENS_EV_TIMER_EXPIRED: u32 = 0x7e00_0004;
/// `EEUIEndCinematicModeEvent`: the cinematic ended at its saved end time.
pub const EE_SCREENS_EV_END_CINEMATIC: u32 = 0x7e00_0005;

/// The event `event` (its arguments a and b) on `form`: its result, `None` for a code that is not one of these.
pub(crate) fn event_args<C: Cpp>(c: &mut C, event: u32, a: usize, b: usize, form: usize) -> Option<c_long> {
    Some(match event {
        EE_SCREENS_EV_CANCEL => c.ev_open_form(a),
        EE_SCREENS_EV_LOAD_UNIT => {
            c.ev_load_unit(form);
            0
        }
        EE_SCREENS_EV_HOT_KEY => {
            main_group_calls::hot_key_event(c, a, b != 0);
            0
        }
        EE_SCREENS_EV_TIMER_EXPIRED => c.ev_timer_expired(),
        EE_SCREENS_EV_END_CINEMATIC => {
            c.ev_end_cinematic();
            0
        }
        _ => return None,
    })
}

#[cfg(not(test))]
mod ffi {
    use core::ffi::{c_long, c_void};

    use crate::screens::game_cpp::Ffi;

    /// `EEScreenEvent::ProcessEvent`: the event `event` with its arguments on `form` from `entity`; a screen event's
    /// code without arguments goes on to `ee_screens_event`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_screens_event_args(
        event: u32,
        a: usize,
        b: usize,
        form: *mut c_void,
        entity: *mut c_void,
    ) -> c_long {
        match super::event_args(&mut Ffi, event, a, b, form as usize) {
            Some(result) => result,
            // SAFETY: the caller's form and entity
            None => unsafe { crate::screens::menus_ffi::ee_screens_event(event, form, entity) },
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    #[test]
    fn the_events() {
        let mut f = Fake::default();
        f.answer("ev_open_form", &[0]).answer("ev_timer_expired", &[3]);
        assert_eq!(event_args(&mut f, EE_SCREENS_EV_CANCEL, 40, 0, 9), Some(0));
        assert_eq!(event_args(&mut f, EE_SCREENS_EV_LOAD_UNIT, 0, 0, 9), Some(0));
        assert_eq!(event_args(&mut f, EE_SCREENS_EV_TIMER_EXPIRED, 0, 0, 9), Some(3));
        assert_eq!(event_args(&mut f, EE_SCREENS_EV_END_CINEMATIC, 0, 0, 9), Some(0));
        assert_eq!(event_args(&mut f, 0x1234, 0, 0, 9), None);
        assert_eq!(f.calls(), "ev_open_form(40)\nev_load_unit(9)\nev_timer_expired()\nev_end_cinematic()");
        // a hot key: pressed (the saved groups' state reset), released; no button: nothing
        let mut f = Fake::default();
        event_args(&mut f, EE_SCREENS_EV_HOT_KEY, 7, 1, 9);
        assert_eq!(f.calls(), "mg_button_hot_key_down(7)\nmg_saved_groups_reset()");
        let mut f = Fake::default();
        event_args(&mut f, EE_SCREENS_EV_HOT_KEY, 0, 1, 9);
        assert_eq!(f.calls(), "");
    }
}
