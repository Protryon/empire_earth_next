//! `extern "C"` for the single player screen and the name check (`single_player.rs`): plain values in and out.
#![allow(clippy::missing_safety_doc)]

use crate::single_player::{self, EeScreensSinglePlayerSlot};

/// `EEIsValidName` over the name's `count` characters (each one's first byte).
#[no_mangle]
pub unsafe extern "C" fn ee_screens_valid_name(chars: *const u8, count: usize) -> bool {
    let chars = if chars.is_null() || count == 0 { &[][..] } else { core::slice::from_raw_parts(chars, count) };
    single_player::valid_name(chars)
}

/// The random map's slot table: 16 slots into `out`.
#[no_mangle]
pub unsafe extern "C" fn ee_screens_single_player_slots(out: *mut EeScreensSinglePlayerSlot) {
    out.cast::<[EeScreensSinglePlayerSlot; 16]>().write(single_player::random_map_slots());
}
