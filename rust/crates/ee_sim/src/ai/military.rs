//! The combat manager's military class tests (EECPCombatManager.cpp before; lane A, round 2): what the engine and
//! the next engine's view ask of a unit class. Pure over the class's category, type and speed; the C ABI reads the
//! class through `ee_sim::world`'s view.

use std::os::raw::c_ulong;

use crate::world::{ClassView, WorldRead};

/// The categories of a military class (`EEUnitCategory`): air, naval and land fighters, priests, transports, the
/// naval building, the hero, the animals that fight, the space units
const MILITARY: [u64; 21] = [
    0x3, 0x4, 0x5, 0x7, 0x8, 0xa, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x15, 0x17, 0x1a, 0x1b, 0x1c, 0x1f, 0x20, 0x21, 0x22,
];
const CAT_LAND_CITIZEN: u64 = 6;
const CAT_NAVAL_CITIZEN: u64 = 0x16;
/// `kUnitTypeS10NavalMine`
const TYPE_NAVAL_MINE: u64 = 0x182;

/// `IsMilitaryType`: the class's category is a military one
pub fn military_category(category: u64) -> bool {
    MILITARY.contains(&category)
}

/// `IsMilitaryClass`: a mobile military class (not a citizen, a building, a projectile or a naval mine)
pub fn military_class(category: u64, unit_type: u64, max_velocity: f32, projectile: bool) -> bool {
    max_velocity > 0.0
        && category != CAT_LAND_CITIZEN
        && category != CAT_NAVAL_CITIZEN
        && !projectile
        && military_category(category)
        && unit_type != TYPE_NAVAL_MINE
}

/// `EECPCombatManager::IsMilitaryClass(class)` (an `EEUnitClass*`; false for NULL).
///
/// # Safety
/// `class` is 0 or a live unit class.
#[no_mangle]
pub unsafe extern "C" fn ee_ai_military_class(class: u64) -> bool {
    let t = WorldRead::begin();
    let Some(c) = ClassView::of(&t, class) else { return false };
    let v: f32 = crate::world_field!(c.fields(), mMaxVelocity);
    let projectile: bool = crate::world_field!(c.fields(), mIsProjectile);
    military_class(c.category(), c.unit_type(), v, projectile)
}

/// `EECPCombatManager::IsMilitaryType(type)`: `gUnitClassTable[type]`'s category is military (false: no class).
#[no_mangle]
pub extern "C" fn ee_ai_military_type(unit_type: c_ulong) -> bool {
    // SAFETY: the caller holds the world (the simulation thread or a reader of the classes)
    let t = unsafe { WorldRead::begin() };
    ClassView::by_index(&t, unit_type as usize).is_some_and(|c| military_category(c.category()))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn categories() {
        // the original's switch, case by case
        let yes =
            [3, 4, 5, 7, 8, 0xa, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12, 0x15, 0x17, 0x1a, 0x1b, 0x1c, 0x1f, 0x20, 0x21, 0x22];
        for k in 0..64u64 {
            assert_eq!(military_category(k), yes.contains(&k), "category {k:#x}");
        }
    }

    #[test]
    fn classes() {
        assert!(military_class(5, 0x10, 2.0, false));
        assert!(!military_class(5, 0x10, 0.0, false)); // does not move
        assert!(!military_class(5, 0x10, -1.0, false));
        assert!(!military_class(6, 0x20, 2.0, false)); // a citizen
        assert!(!military_class(0x16, 0x21, 2.0, false)); // a fishing boat
        assert!(!military_class(5, 0x10, 2.0, true)); // a projectile class
        assert!(!military_class(0xa, TYPE_NAVAL_MINE, 2.0, false)); // the naval mine
        assert!(military_class(0xa, 0x181, 2.0, false));
        assert!(!military_class(2, 0x23, 2.0, false)); // a building
        assert!(!military_class(0x19, 0x30, 2.0, false)); // a gate
    }
}
