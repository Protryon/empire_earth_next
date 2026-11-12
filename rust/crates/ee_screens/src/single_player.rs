//! The single player screen (form 6, `UI Events/EEUIESinglePlayerEvents.cpp`) and the player name check it shares
//! with the multiplayer screens (`EEIsValidName`, `Actions/EEAJoinOrHostAGame.cpp`): the name's rule, the name a
//! random map starts with, and the random map's first slot table (you against one computer, the other slots
//! closed).
//!
//! The C++ keeps the edit box, the strings, `gGameOptions` and the server it starts; it hands over the name's
//! characters and fills the slots from the plan.

use core::ffi::c_ulong;

/// `PlatIsMbcSpace` of a name's character as `EEIsValidName` passes it: the first byte as a `char`, sign-extended
/// (a multi-byte character's lead byte is never a space).
fn is_space(first: u8) -> bool {
    let cp = first as i8 as i32 as u32;
    cp == 0x20 || (0x09..=0x0D).contains(&cp)
}

/// `EEIsValidName` over the name's characters (each one's first byte: `UWideString::operator[]`): not empty, no
/// control character, DEL, quote, double quote, backslash or `@`, no space first or last, no two spaces in a row.
pub fn valid_name(chars: &[u8]) -> bool {
    if chars.is_empty() {
        return false;
    }
    if chars.iter().any(|&c| c < 0x20 || c == 0x7f || c == b'\'' || c == b'"' || c == b'\\' || c == b'@') {
        return false;
    }
    if is_space(chars[chars.len() - 1]) || is_space(chars[0]) {
        return false;
    }
    !chars.windows(2).any(|w| is_space(w[0]) && is_space(w[1]))
}

/// A slot of the random map's table: the human's, the computer's, or a closed one.
pub const EE_SCREENS_SP_HUMAN: u32 = 0;
pub const EE_SCREENS_SP_COMPUTER: u32 = 1;
pub const EE_SCREENS_SP_CLOSED: u32 = 2;

/// A slot's fields the random map sets (a closed slot: a new slot's, with `slot` 4, as the original's).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScreensSinglePlayerSlot {
    /// `EE_SCREENS_SP_*`: whose slot (the human's and the computer's take their names; a closed one keeps a new
    /// slot's type and name).
    pub source: u32,
    pub kind: c_ulong,
    pub color: c_ulong,
    pub slot: c_ulong,
    pub local: bool,
    pub ready: bool,
}

/// The random map's slot table: slot 0 the local human (type 1, colour 0, ready), slot 1 a computer (type 0,
/// colour 1, ready), slots 2..15 closed.
pub fn random_map_slots() -> [EeScreensSinglePlayerSlot; 16] {
    let mut slots = [EeScreensSinglePlayerSlot { source: EE_SCREENS_SP_CLOSED, slot: 4, ..Default::default() }; 16];
    slots[0] =
        EeScreensSinglePlayerSlot { source: EE_SCREENS_SP_HUMAN, kind: 1, color: 0, slot: 0, local: true, ready: true };
    slots[1] = EeScreensSinglePlayerSlot {
        source: EE_SCREENS_SP_COMPUTER,
        kind: 0,
        color: 1,
        slot: 1,
        local: false,
        ready: true,
    };
    slots
}

#[cfg(test)]
mod golden;
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn names() {
        assert!(valid_name(b"Max"));
        assert!(valid_name(b"Max Bruce"));
        assert!(!valid_name(b""));
        assert!(!valid_name(b" Max"));
        assert!(!valid_name(b"Max "));
        assert!(!valid_name(b"Max  Bruce"));
        for bad in [b'@', b'\'', b'"', b'\\', 0x7f, 0x1f, b'\t'] {
            assert!(!valid_name(&[b'a', bad, b'b']), "{bad:#x}");
        }
        // a multi-byte character's lead byte is no space, nor anything refused
        assert!(valid_name(&[0xC3, b'a']));
        assert!(valid_name(&[0xA0]));
    }

    #[test]
    fn the_random_map_table() {
        let s = random_map_slots();
        assert_eq!((s[0].source, s[0].kind, s[0].local), (EE_SCREENS_SP_HUMAN, 1, true));
        assert_eq!((s[1].source, s[1].kind, s[1].color), (EE_SCREENS_SP_COMPUTER, 0, 1));
        assert!(s[2..].iter().all(|x| x.source == EE_SCREENS_SP_CLOSED && x.slot == 4));
    }
}
