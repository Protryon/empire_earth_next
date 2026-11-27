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

/// The pointer member at `object + offset`.
fn ptr_at(object: usize, offset: u64) -> usize {
    // SAFETY: a live object's pointer member (the C++ statement dereferenced it)
    unsafe { *((object + offset as usize) as *const usize) }
}

/// Unit `u`'s player's upgrade of its class (`u->GetPlayer()->mUnitClassUpgrades[u->GetUnitClass()->mUnitType]`).
fn unit_upgrade(u: usize) -> Option<u64> {
    let l = crate::world::layout::get()?;
    let class = ptr_at(u, l.unit_class) + l.unit_class_fields as usize;
    // SAFETY: the unit's live class's record
    let ty = unsafe { (*(class as *const crate::world::unit_class_fields::EeUnitsUnitClassFields)).mUnitType };
    Some(at(ptr_at(u, l.unit_player) as u64, ty as usize))
}

/// The unit host's statement `op` on unit `unit` (`a` the other unit) (EEUnit.cpp `ee_units_eum_host`): the
/// upgrade's maximum hit points, the unit's stats set to its upgrade.
pub fn eum(op: i32, unit: u64, a: u64) -> Option<i64> {
    use crate::units::eu_more::{EE_UNITS_EUM_SET_STATS, EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS};
    match op {
        EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS => {
            let upgrade = unit_upgrade(a as usize)?;
            let at = upgrade as usize + crate::world::layout::get()?.stats_fields as usize;
            // SAFETY: the player's live upgrade's record (the C++ statement dereferenced it)
            Some(unsafe { (*(at as *const crate::world::stats_fields::EeUnitsUnitClassUpgradeFields)).mMaxHitPoints }
                as i64)
        }
        EE_UNITS_EUM_SET_STATS => {
            let upgrade = unit_upgrade(unit as usize)?;
            let at = unit as usize + crate::world::layout::get()?.unit_stats as usize;
            // SAFETY: the live unit's mStats
            unsafe { *(at as *mut usize) = upgrade as usize };
            Some(0)
        }
        _ => None,
    }
}

/// The shots' host statement `op` on complex unit `unit` (EEUnitClassShots.cpp `ee_units_ucs_host`): f0 = its
/// player's upgrade of its launcher class's `mLaunchSpeed`.
pub fn ucs(op: i32, unit: u64, f: &mut [f32; 4]) -> Option<i64> {
    use crate::units::ucs_shots::{EE_UNITS_UCS_LAUNCH_SPEED, EE_UNITS_UCS_UPGRADE_LAUNCH_SPEED};
    if op != EE_UNITS_UCS_LAUNCH_SPEED && op != EE_UNITS_UCS_UPGRADE_LAUNCH_SPEED {
        return None;
    }
    let l = crate::world::layout::get()?;
    let u = unit as usize;
    let class = ptr_at(u, l.complex_launcher_class) + l.unit_class_fields as usize;
    // SAFETY: the unit's live launcher class's record (the C++ statement dereferenced it)
    let ty = unsafe { (*(class as *const crate::world::unit_class_fields::EeUnitsUnitClassFields)).mUnitType };
    let upgrade = at(ptr_at(u, l.unit_player) as u64, ty as usize) as usize + l.stats_fields as usize;
    // SAFETY: the player's live upgrade's record (as above)
    f[0] = unsafe { (*(upgrade as *const crate::world::stats_fields::EeUnitsUnitClassUpgradeFields)).mLaunchSpeed };
    Some(0)
}
