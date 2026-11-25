//! `extern "C"` for the single player screen and the name check (`single_player.rs`): plain values in and out.
#![allow(clippy::missing_safety_doc)]

use crate::screens::single_player;

/// `EEIsValidName`: the UWideString's text and `GetLength()` (its characters); the rule reads each character's first
/// byte (`operator[]`, ee_base ustring wide.rs `ee_uwstring_at`; the loop was EEARust.cpp's).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_valid_name(text: *const core::ffi::c_char, length: core::ffi::c_ulong) -> bool {
    let name =
        ee_base::ustring::EeUWideString { string: text as *mut core::ffi::c_char, byte_length: 0, length, capacity: 0 };
    let chars: Vec<u8> = (0..length as i32).map(|i| ee_base::ustring::ee_uwstring_at(&name, i) as u8).collect();
    single_player::valid_name(&chars)
}

/// EEAJoinOrHostAGameRestartClient: the join's message box callback (a `void (*)(UIForm*)`; the form is unused; was
/// EEARust.cpp's): the communications object replaced with a freshly started client.
#[no_mangle]
pub extern "C" fn ee_screens_join_restart_client(_form: *mut core::ffi::c_void) {
    use crate::gamecpp::{h, O};
    h(O::CommDeleteDetached, 0, &[]);
    h(O::EntryNewClient, 0, &[]);
    h(O::EntryClientStartThreads, 0, &[]);
}
