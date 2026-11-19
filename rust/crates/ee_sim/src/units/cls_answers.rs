//! The unit class's answers that read only its records (the unit classes cluster, round 1; EEUnitClass.cpp until
//! this round): `CanEnter`, `GetNumGoalQueues`, `SharesRallyPoint`, `SharesResearch`, `CanMoveOnTerrain`,
//! `GetPassableFlags`. The C++ members are one-line calls of the entry points below, given the records' addresses.

use crate::units::unit_class_fields::EeUnitsUnitClassFields;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeFields;

/// The C++ enums the answers test.
/// cbindgen:ignore
mod k {
    pub const CAT_BUILDING: u64 = 2;
    pub const CAT_AIRPLANE: u64 = 3;
    pub const CAT_LAND_TRANSPORT: u64 = 0x10;
    pub const CAT_HELICOPTER_TRANSPORT: u64 = 0x11;
    pub const CAT_NAVAL_TRANSPORT: u64 = 0x12;
    pub const CAT_NAVAL_BUILDING: u64 = 0x15;
    pub const CAT_FORTRESS: u64 = 0x1d;
    pub const CAT_SPACE_BUILDING: u64 = 0x20;
    pub const CAT_SPACE_TRANSPORT: u64 = 0x21;
    pub const CAT_PLANET_SPACESHIP: u64 = 0x22;
    pub const AI_AIRCRAFT_CARRIER: u64 = 0xf;
    pub const T_CITIZEN: u64 = 0x20;
    pub const T_BARRACKS: u64 = 0x22;
    pub const T_TOWN_CENTER: u64 = 0x23;
    pub const T_DOCK: u64 = 0x24;
    pub const T_CAPITOL: u64 = 0x29;
    pub const T_ARCHERY_RANGE: u64 = 0x36;
    pub const T_SIEGE_FACTORY: u64 = 0x3f;
    pub const T_STABLE: u64 = 0x40;
    pub const T_NAVY_YARD: u64 = 0x41;
    pub const T_AIRPORT: u64 = 0x42;
    pub const T_TANK_FACTORY: u64 = 0x44;
    pub const T_CYBER_FACTORY: u64 = 0x45;
    pub const T_FEMALE_CITIZEN: u64 = 0x1e0;
    pub const T_CYBER_LABORATORY: u64 = 0x24a;
    pub const T_SP15_SPACE_CARRIER: u64 = 0x2f3;
    pub const T_SP15_SPACE_FIGHTER: u64 = 0x2f6;
}
use k::*;

/// `EEUnitClass::CanEnter(other)`: whether units of `c` may board a container of class `other`.
pub fn can_enter(c: &EeUnitsUnitClassFields, other: &EeUnitsUnitClassFields) -> bool {
    let (cat, ty, ai) = (other.mCategory as u64, other.mUnitType as u64, other.mAIKind as u64);
    (c.mCanEnterAirTransports && cat == CAT_HELICOPTER_TRANSPORT)
        || (c.mCanEnterLandTransports && cat == CAT_LAND_TRANSPORT)
        || (c.mCanEnterSeaTransports && cat == CAT_NAVAL_TRANSPORT)
        || (c.mCanEnterSpaceTransports && cat == CAT_SPACE_TRANSPORT)
        || (c.mCanEnterFortresses && cat == CAT_FORTRESS)
        || (c.mCanEnterCarriers && ai == AI_AIRCRAFT_CARRIER)
        || (c.mCanEnterAirports && ty == T_AIRPORT)
        || (c.mCanEnterNavyYards && ty == T_NAVY_YARD)
        || (c.mUnitType as u64 == T_SP15_SPACE_FIGHTER && ty == T_SP15_SPACE_CARRIER)
}

/// `EEUnitClass::GetNumGoalQueues()`: airport 5, navy yard 3, carriers, buildings, airplanes and spaceships 2,
/// else 1.
pub fn num_goal_queues(c: &EeUnitsUnitClassFields) -> i32 {
    let (cat, ty) = (c.mCategory as u64, c.mUnitType as u64);
    if ty == T_AIRPORT {
        return 5;
    }
    if ty == T_NAVY_YARD {
        return 3;
    }
    if ((cat == CAT_NAVAL_BUILDING || cat == CAT_SPACE_BUILDING) && c.mAirTrafficType != 0)
        || cat == CAT_BUILDING
        || cat == CAT_AIRPLANE
        || cat == CAT_PLANET_SPACESHIP
    {
        return 2;
    }
    1
}

const MILITARY: [u64; 7] =
    [T_BARRACKS, T_ARCHERY_RANGE, T_SIEGE_FACTORY, T_STABLE, T_TANK_FACTORY, T_CYBER_FACTORY, T_CYBER_LABORATORY];

/// `EEUnitClass::SharesRallyPoint(other_type)`: the military buildings among themselves, the town centre and the
/// capitol, the dock and the navy yard.
pub fn shares_rally_point(c: &EeUnitsUnitClassFields, other: u64) -> bool {
    let ty = c.mUnitType as u64;
    if MILITARY.contains(&ty) {
        MILITARY.contains(&other)
    } else if ty == T_TOWN_CENTER || ty == T_CAPITOL {
        other == T_TOWN_CENTER || other == T_CAPITOL
    } else if ty == T_DOCK || ty == T_NAVY_YARD {
        other == T_DOCK || other == T_NAVY_YARD
    } else {
        false
    }
}

/// `EEUnitClass::SharesResearch(other_type)`: the town centre and the capitol, the two citizens.
pub fn shares_research(c: &EeUnitsUnitClassFields, other: i64) -> bool {
    let ty = c.mUnitType as u64;
    let other = other as u64;
    if ty == T_TOWN_CENTER || ty == T_CAPITOL {
        other == T_TOWN_CENTER || other == T_CAPITOL
    } else if ty == T_CITIZEN || ty == T_FEMALE_CITIZEN {
        other == T_CITIZEN || other == T_FEMALE_CITIZEN
    } else {
        false
    }
}

/// `EEUnitClass::CanMoveOnTerrain(unit, terrain)`: by the unit's stats' terrain mask, else the class's terrain
/// table (`stats` None: no unit).
pub fn can_move_on_terrain(
    c: &EeUnitsUnitClassFields,
    stats: Option<&EeUnitsUnitClassUpgradeFields>,
    terrain: i32,
) -> bool {
    match stats {
        Some(s) => (s.mTerrainMask as u64 & u64::from(1u32 << terrain)) != 0,
        None => c.mTerrainTypes[terrain as usize],
    }
}

/// `EEUnitClass::GetPassableFlags(unit)`: land 4, cliff 1, water 2, space 8.
pub fn passable_flags(c: &EeUnitsUnitClassFields, stats: Option<&EeUnitsUnitClassUpgradeFields>) -> i32 {
    let mut r = 0;
    if can_move_on_terrain(c, stats, 0) {
        r = 4;
    }
    if can_move_on_terrain(c, stats, 9) {
        r |= 1;
    }
    if can_move_on_terrain(c, stats, 1) {
        r |= 2;
    }
    if can_move_on_terrain(c, stats, 0xc) {
        r |= 8;
    }
    r
}

// ---- the entry points (records by address; `stats` 0: no unit)

/// # Safety
/// Both are live classes' records.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_can_enter(
    c: *const EeUnitsUnitClassFields,
    other: *const EeUnitsUnitClassFields,
) -> u8 {
    // SAFETY: the caller's contract
    u8::from(unsafe { can_enter(&*c, &*other) })
}

/// # Safety
/// A live class's record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_num_goal_queues(c: *const EeUnitsUnitClassFields) -> i32 {
    // SAFETY: the caller's contract
    unsafe { num_goal_queues(&*c) }
}

/// # Safety
/// A live class's record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_shares_rally_point(c: *const EeUnitsUnitClassFields, other: u64) -> u8 {
    // SAFETY: the caller's contract
    u8::from(unsafe { shares_rally_point(&*c, other) })
}

/// # Safety
/// A live class's record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_shares_research(c: *const EeUnitsUnitClassFields, other: i64) -> u8 {
    // SAFETY: the caller's contract
    u8::from(unsafe { shares_research(&*c, other) })
}

/// # Safety
/// A live class's record; `stats` a live stats record or null.
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_passable_flags(
    c: *const EeUnitsUnitClassFields,
    stats: *const EeUnitsUnitClassUpgradeFields,
) -> i32 {
    // SAFETY: the caller's contract
    unsafe { passable_flags(&*c, stats.as_ref()) }
}

/// # Safety
/// As [`ee_units_cls_passable_flags`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_cls_can_move_on_terrain(
    c: *const EeUnitsUnitClassFields,
    stats: *const EeUnitsUnitClassUpgradeFields,
    terrain: i32,
) -> u8 {
    // SAFETY: the caller's contract
    u8::from(unsafe { can_move_on_terrain(&*c, stats.as_ref(), terrain) })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn class(category: u64, unit_type: u64) -> EeUnitsUnitClassFields {
        EeUnitsUnitClassFields { mCategory: category as _, mUnitType: unit_type as _, ..Default::default() }
    }

    #[test]
    fn enter() {
        let mut c = class(1, 5);
        let heli = class(CAT_HELICOPTER_TRANSPORT, 9);
        assert!(!can_enter(&c, &heli));
        c.mCanEnterAirTransports = true;
        assert!(can_enter(&c, &heli));
        let fighter = class(1, T_SP15_SPACE_FIGHTER);
        assert!(can_enter(&fighter, &class(1, T_SP15_SPACE_CARRIER)));
        let mut carrier = class(1, 7);
        carrier.mAIKind = AI_AIRCRAFT_CARRIER as _;
        c.mCanEnterCarriers = true;
        assert!(can_enter(&c, &carrier));
    }

    #[test]
    fn queues() {
        assert_eq!(num_goal_queues(&class(CAT_BUILDING, T_AIRPORT)), 5);
        assert_eq!(num_goal_queues(&class(CAT_BUILDING, T_NAVY_YARD)), 3);
        assert_eq!(num_goal_queues(&class(CAT_BUILDING, T_DOCK)), 2);
        let mut naval = class(CAT_NAVAL_BUILDING, 1);
        assert_eq!(num_goal_queues(&naval), 1);
        naval.mAirTrafficType = 2;
        assert_eq!(num_goal_queues(&naval), 2);
        assert_eq!(num_goal_queues(&class(CAT_PLANET_SPACESHIP, 1)), 2);
        assert_eq!(num_goal_queues(&class(1, 1)), 1);
    }

    #[test]
    fn sharing() {
        assert!(shares_rally_point(&class(2, T_STABLE), T_CYBER_LABORATORY));
        assert!(!shares_rally_point(&class(2, T_STABLE), T_DOCK));
        assert!(shares_rally_point(&class(2, T_NAVY_YARD), T_DOCK));
        assert!(shares_rally_point(&class(2, T_CAPITOL), T_TOWN_CENTER));
        assert!(!shares_rally_point(&class(2, T_AIRPORT), T_AIRPORT));
        assert!(shares_research(&class(1, T_FEMALE_CITIZEN), T_CITIZEN as i64));
        assert!(!shares_research(&class(1, T_FEMALE_CITIZEN), T_TOWN_CENTER as i64));
        assert!(shares_research(&class(2, T_TOWN_CENTER), T_CAPITOL as i64));
    }

    #[test]
    fn terrain() {
        let mut c = class(1, 1);
        c.mTerrainTypes[0] = true;
        c.mTerrainTypes[12] = true;
        assert_eq!(passable_flags(&c, None), 12);
        let s = EeUnitsUnitClassUpgradeFields { mTerrainMask: (1 << 9) | (1 << 1), ..Default::default() };
        assert_eq!(passable_flags(&c, Some(&s)), 3);
        assert!(can_move_on_terrain(&c, Some(&s), 9));
        assert!(!can_move_on_terrain(&c, Some(&s), 0));
    }
}
