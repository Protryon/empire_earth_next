//! The loading screen (form 23, the splash screen a game loads behind) whole: the original `EEUIESSEOpen.cpp` (its
//! open event: the registered-to and copyright lines, and with several humans a name and progress bar per human or
//! observer, the local player's bar index noted) over a host ([`SplashHost`]) whose calls are each one game or
//! toolkit call (`UI Events/EEScreens.cpp`).

use core::ffi::c_long;

use crate::menus::{EE_SCREENS_OP_SHOW, EE_SCREENS_TEXT_LABEL};
use crate::post_game::{EE_SCREENS_OP_PGS_BAR_COLOUR, EE_SCREENS_OP_PGS_BAR_OPACITY, EE_SCREENS_OP_PGS_RECT};
use crate::settings_form::EE_SCREENS_OP_BACKGROUND;

/// The loading screen's open event (`kFormEventSSFormOpen`).
pub const EE_SCREENS_SS_OPEN: u32 = 0x1701;

// ---- the host's ops (ee_screenscpp_call on the loading screen)

/// (1 lock, 0 unlock) the lobby options' read lock.
pub const EE_SCREENS_GAME_SS_LOCK: u32 = 0x300;
/// -> `gCommunications->GetNumHumanPlayers()`.
pub const EE_SCREENS_GAME_SS_HUMANS: u32 = 0x301;
/// (slot) -> the lobby slot's type in bits 0..7, 1 << 8 when it is the local player's.
pub const EE_SCREENS_GAME_SS_PLAYER: u32 = 0x302;
/// (index) `gUserInterface->mLocalPlayer = index`.
pub const EE_SCREENS_GAME_SS_LOCAL: u32 = 0x303;
/// (bar, slot) `SetStatusBarColor(EEGetPlayerColor(the slot's colour))`.
pub const EE_SCREENS_OP_SS_BAR_PLAYER_COLOUR: u32 = 0x304;
/// (bar, value) `SetCurrentRangeValue`.
pub const EE_SCREENS_OP_SS_BAR_VALUE: u32 = 0x305;
/// (bottom bits) background 0x84's rectangle with that bottom, shown.
pub const EE_SCREENS_OP_SS_BACKGROUND: u32 = 0x306;
/// Texts in: the decrypted registered-to line, the decrypted copyright line, the registered name (`gU530BC8`), a lobby
/// slot's name.
pub const EE_SCREENS_TEXT_SS_TITLE: u32 = 0x92;
pub const EE_SCREENS_TEXT_SS_COPYRIGHT: u32 = 0x93;
pub const EE_SCREENS_TEXT_SS_SUFFIX: u32 = 0x94;
pub const EE_SCREENS_TEXT_SS_NAME: u32 = 0x95;

const SS_REGISTERED_TO: u32 = 0;
const SS_COPYRIGHT: u32 = 1;
const SS_BACKGROUND: u32 = 0x84;
const SS_SHOWN: u32 = 1;
const SS_TYPE_HUMAN: u32 = 1;
const SS_TYPE_OBSERVER: u32 = 2;

/// The calls of the loading screen.
pub trait SplashHost {
    /// `EE_SCREENS_OP_*` / `EE_SCREENS_GAME_*` with up to four arguments and an answer.
    fn call(&mut self, op: u32, a: u32, b: u32, c: u32, d: u32) -> u32;
    /// A text of the game.
    fn text(&mut self, which: u32, arg: u32) -> Vec<u8>;
    /// A text into the game.
    fn put_text(&mut self, which: u32, arg: u32, text: &[u8]);
}

/// `EEUIESSEOpen`: under the lobby options' read lock, the two lines, then (with several humans) a row per human or
/// observer slot (its name, a full-opacity bar in its colour, white for an observer, at 0) and the rows' backdrop
/// down to the last row, or no backdrop.
fn open<H: SplashHost + ?Sized>(h: &mut H) {
    h.call(EE_SCREENS_GAME_SS_LOCK, 1, 0, 0, 0);
    let mut title = h.text(EE_SCREENS_TEXT_SS_TITLE, 0);
    let copyright = h.text(EE_SCREENS_TEXT_SS_COPYRIGHT, 0);
    title.extend_from_slice(&h.text(EE_SCREENS_TEXT_SS_SUFFIX, 0));
    h.put_text(EE_SCREENS_TEXT_LABEL, SS_REGISTERED_TO, &title);
    h.put_text(EE_SCREENS_TEXT_LABEL, SS_COPYRIGHT, &copyright);
    if h.call(EE_SCREENS_GAME_SS_HUMANS, 0, 0, 0, 0) > 1 {
        let mut count: u32 = 0;
        for slot in 0..16 {
            let player = h.call(EE_SCREENS_GAME_SS_PLAYER, slot, 0, 0, 0);
            if player & 1 << 8 != 0 {
                h.call(EE_SCREENS_GAME_SS_LOCAL, count, 0, 0, 0);
            }
            let kind = player & 0xff;
            if kind != SS_TYPE_HUMAN && kind != SS_TYPE_OBSERVER {
                continue;
            }
            let (label, bar) = (count + 4, count + 0x14);
            h.call(EE_SCREENS_OP_SHOW, label, SS_SHOWN, 0, 0);
            let name = h.text(EE_SCREENS_TEXT_SS_NAME, slot);
            h.put_text(EE_SCREENS_TEXT_LABEL, label, &name);
            h.call(EE_SCREENS_OP_SHOW, bar, SS_SHOWN, 0, 0);
            h.call(EE_SCREENS_OP_PGS_BAR_OPACITY, bar, 1.0f32.to_bits(), 0, 0);
            if kind == SS_TYPE_OBSERVER {
                let white = 1.0f32.to_bits();
                h.call(EE_SCREENS_OP_PGS_BAR_COLOUR, bar, white, white, white);
            } else {
                h.call(EE_SCREENS_OP_SS_BAR_PLAYER_COLOUR, bar, slot, 0, 0);
            }
            h.call(EE_SCREENS_OP_SS_BAR_VALUE, bar, 0, 0, 0);
            count += 1;
        }
        let bottom = f32::from_bits(h.call(EE_SCREENS_OP_PGS_RECT, count + 3, 3, 0, 0)) + 3.0f32;
        h.call(EE_SCREENS_OP_SS_BACKGROUND, bottom.to_bits(), 0, 0, 0);
    } else {
        h.call(EE_SCREENS_OP_BACKGROUND, SS_BACKGROUND, 0, 0, 0);
    }
    h.call(EE_SCREENS_GAME_SS_LOCK, 0, 0, 0, 0);
}

/// The loading screen's event; `None` for another form's.
pub fn dispatch<H: SplashHost + ?Sized>(h: &mut H, event: u32) -> Option<c_long> {
    if event != EE_SCREENS_SS_OPEN {
        return None;
    }
    open(h);
    Some(0)
}

#[cfg(test)]
mod golden;
