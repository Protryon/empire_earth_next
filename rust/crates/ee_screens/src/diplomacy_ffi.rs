//! The diplomacy form's host over the C++ calls (`diplomacy.rs`; `UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::diplomacy::{DiplomacyHost, EeScreensDiplomacyPlayer};
use crate::menus::EE_SCREENS_TEXT_STRING;
use crate::menus_ffi::Form;

/// cbindgen:ignore
mod sys {
    use crate::diplomacy::EeScreensDiplomacyPlayer;

    extern "C" {
        /// Player `index` of the world into `out`; false (nothing written) for an empty entry.
        pub fn ee_screenscpp_diplomacy_player(index: u32, out: *mut EeScreensDiplomacyPlayer) -> bool;
        /// `EEMPayTribute` to player `to` with food, wood, gold, stone, iron, to the local input.
        pub fn ee_screenscpp_diplomacy_tribute(to: u32, amounts: *const u32);
    }
}

/// One of the diplomacy form's events and its result; `None` for another form's.
pub(crate) fn dispatch(host: &mut Form, event: u32) -> Option<c_long> {
    crate::diplomacy::dispatch(host, event)
}

impl DiplomacyHost for Form {
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32 {
        self.op(op, a, b, c, d)
    }
    fn player(&mut self, index: u32) -> Option<EeScreensDiplomacyPlayer> {
        let mut out = EeScreensDiplomacyPlayer::default();
        unsafe { sys::ee_screenscpp_diplomacy_player(index, &mut out) }.then_some(out)
    }
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8> {
        self.get(which, arg)
    }
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]) {
        self.put(which, arg, text);
    }
    fn string(&mut self, id: u32) -> Vec<u8> {
        self.get(EE_SCREENS_TEXT_STRING, id)
    }
    fn pay_tribute(&mut self, to: u32, amounts: [u32; 5]) {
        unsafe { sys::ee_screenscpp_diplomacy_tribute(to, amounts.as_ptr()) }
    }
}
