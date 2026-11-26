//! A player's unit class upgrades (`EEPlayer::mUnitClassUpgrades`, a `std::vector<EEUnitClassUpgrade*>`) read in place
//! through the world layout (player/tech lane, round 4): its size and elements, and the upgrade of a unit class, for
//! the player hosts' reads (EEPlayer.cpp's body, host and save statements), answered here before the C++ is asked.
//! The vector's growth, clearing and its upgrades' `new`/`delete` stay C++ (the upgrade is a C++ class).

use crate::units::player::EeUnitsPlayerIo;
use crate::units::player_save::EeUnitsPlayerSaveIo;

/// The vector's begin and end (empty without the layout or a player).
fn bounds(player: u64) -> (usize, usize) {
    let Some(l) = crate::world::layout::get() else { return (0, 0) };
    if player == 0 || l.player_unit_class_upgrades == 0 {
        return (0, 0);
    }
    let at = player as usize + l.player_unit_class_upgrades as usize;
    // SAFETY: a live player's vector: its begin and end pointers are its first two words (MSVC, libstdc++, libc++)
    unsafe { (*(at as *const usize), *((at + std::mem::size_of::<usize>()) as *const usize)) }
}

/// `mUnitClassUpgrades.size()`.
pub fn slots(player: u64) -> usize {
    let (begin, end) = bounds(player);
    (end - begin) / std::mem::size_of::<usize>()
}

/// `mUnitClassUpgrades[i]` (unchecked, as the C++'s `operator[]`).
pub fn at(player: u64, i: usize) -> u64 {
    let (begin, _) = bounds(player);
    // SAFETY: the vector's element i (the caller's index is in range, as the C++ statement's was)
    unsafe { *((begin + i * std::mem::size_of::<usize>()) as *const usize) as u64 }
}

/// The player's upgrade of unit class `class` (`GetUpgrade`; 0 past the vector or for none): EETechTree.cpp's
/// `TkUpgrade`.
pub fn upgrade(player: u64, class: i64) -> u64 {
    usize::try_from(class).ok().filter(|&c| c < slots(player)).map_or(0, |c| at(player, c))
}

/// `EEUnitClassCount()`: `gUnitClassTable`'s size.
fn class_count() -> usize {
    let Some(l) = crate::world::layout::get() else { return 0 };
    let t = l.unit_class_table as usize;
    if t == 0 {
        return 0;
    }
    // SAFETY: the registered vector's begin and end
    let (b, e) = unsafe { (*(t as *const usize), *((t + std::mem::size_of::<usize>()) as *const usize)) };
    (e - b) / std::mem::size_of::<usize>()
}

/// The body host's statement `op` of player `p` (EEPlayer.cpp `ee_units_player_body_host`).
pub fn body(op: i32, p: u64, b: i64) -> Option<i64> {
    crate::world::layout::get()?;
    use crate::units::player_rest::{EE_UNITS_PLAYER_BODY_UPGRADE_AT, EE_UNITS_PLAYER_BODY_UPGRADE_SLOTS};
    Some(match op {
        EE_UNITS_PLAYER_BODY_UPGRADE_SLOTS => slots(p) as i64,
        EE_UNITS_PLAYER_BODY_UPGRADE_AT => at(p, b as usize) as i64,
        _ => return None,
    })
}

/// The player host's statement `op` (EEPlayer.cpp `EEPlayerHost`).
pub fn player(op: i32, p: u64, io: &EeUnitsPlayerIo) -> Option<i64> {
    crate::world::layout::get()?;
    use crate::units::player_split::{EE_UNITS_PLAYER_X_UPGRADE_AT, EE_UNITS_PLAYER_X_UPGRADE_SLOTS};
    Some(match op {
        EE_UNITS_PLAYER_X_UPGRADE_SLOTS => slots(p) as i64,
        EE_UNITS_PLAYER_X_UPGRADE_AT => at(p, io.i[0] as usize) as i64,
        _ => return None,
    })
}

/// The save host's statement `op` (EEPlayer.cpp `EEPlayerSaveOperation`; `index` its index).
pub fn save(op: i32, p: u64, index: u32, io: &mut EeUnitsPlayerSaveIo) -> Option<i32> {
    crate::world::layout::get()?;
    use crate::units::player_save::{EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT, EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS};
    use crate::units::player_save_split::EE_UNITS_PLAYER_SAVE_X_UPGRADE_PRESENT;
    if p == 0 {
        return None;
    }
    Some(match op {
        EE_UNITS_PLAYER_SAVE_UPGRADE_SLOTS => {
            io.i = slots(p) as i64;
            0
        }
        EE_UNITS_PLAYER_SAVE_UPGRADE_PRESENT => i32::from(at(p, index as usize) != 0),
        EE_UNITS_PLAYER_SAVE_X_UPGRADE_PRESENT => {
            let i = io.i as u32 as usize;
            i32::from(i < class_count() && at(p, i) != 0)
        }
        _ => return None,
    })
}
