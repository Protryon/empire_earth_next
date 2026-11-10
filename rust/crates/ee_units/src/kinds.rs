//! Unit kinds as data (RUST_PLAN R5b.4): which C++ classes a unit goal and a unit class are, as bits, so that the
//! `dynamic_cast` tests become table reads (`include/ee/EEUnitKind.h`).
//!
//! - A goal's class follows from `EEUnitGoal::mKind`: every goal class's constructors pass their kind
//!   (`EEUnitGoal::CreateGoal` lists them), `EEUGHealUnit`'s set 15 over its base `EEUGLocation`'s 1, the copies
//!   (`Clone`) copy it, and nothing else writes it. `EEUGLoadContainer` is abstract (no `Clone`): its kinds are
//!   its two subclasses'.
//! - A unit class's C++ class follows from its category: `EEUnitClass::CreateUnitClasses` builds the class from the
//!   record's category alone, and `EEUnitClass::mCategory` is written only by the constructor.
//!
//! Each entry holds the class itself and its bases among the kinds, as `dynamic_cast` answers for a base.

/// The goal classes (`EEUG*`).
pub const EE_GOAL_KIND_LOCATION: u32 = 0x1;
pub const EE_GOAL_KIND_ATTACK_UNIT: u32 = 0x2;
pub const EE_GOAL_KIND_ATTACK_GROUND: u32 = 0x4;
pub const EE_GOAL_KIND_BUILD_REPAIR: u32 = 0x8;
pub const EE_GOAL_KIND_GATHER: u32 = 0x10;
pub const EE_GOAL_KIND_LOAD_CONTAINER: u32 = 0x20;
pub const EE_GOAL_KIND_LOAD_TRANSPORT: u32 = 0x40;
pub const EE_GOAL_KIND_UNLOAD_TRANSPORT: u32 = 0x80;
pub const EE_GOAL_KIND_PATROL: u32 = 0x100;
pub const EE_GOAL_KIND_EXPLORE: u32 = 0x200;
pub const EE_GOAL_KIND_CAST_CALAMITY: u32 = 0x400;
pub const EE_GOAL_KIND_LOAD_GARRISON: u32 = 0x800;
pub const EE_GOAL_KIND_HEAL_UNIT: u32 = 0x1000;

/// The unit classes (`EEUC*`).
pub const EE_CLASS_KIND_AMBIENT: u64 = 0x1;
pub const EE_CLASS_KIND_RESOURCE: u64 = 0x2;
pub const EE_CLASS_KIND_BUILDING: u64 = 0x4;
pub const EE_CLASS_KIND_LANDABLE: u64 = 0x8;
pub const EE_CLASS_KIND_AIRPLANE: u64 = 0x10;
pub const EE_CLASS_KIND_NAVAL: u64 = 0x20;
pub const EE_CLASS_KIND_LAND: u64 = 0x40;
pub const EE_CLASS_KIND_BASIC_CITIZEN: u64 = 0x80;
pub const EE_CLASS_KIND_LAND_CITIZEN: u64 = 0x100;
pub const EE_CLASS_KIND_NAVAL_CITIZEN: u64 = 0x200;
pub const EE_CLASS_KIND_HELICOPTER: u64 = 0x400;
pub const EE_CLASS_KIND_PROJECTILE: u64 = 0x800;
pub const EE_CLASS_KIND_BOMB: u64 = 0x1000;
pub const EE_CLASS_KIND_MISSILE: u64 = 0x2000;
pub const EE_CLASS_KIND_TORPEDO: u64 = 0x4000;
pub const EE_CLASS_KIND_BASIC_PRIEST: u64 = 0x8000;
pub const EE_CLASS_KIND_LAND_PRIEST: u64 = 0x10000;
pub const EE_CLASS_KIND_NAVAL_PRIEST: u64 = 0x20000;
pub const EE_CLASS_KIND_HELICOPTER_PRIEST: u64 = 0x40000;
pub const EE_CLASS_KIND_NAVAL_BUILDING: u64 = 0x80000;
pub const EE_CLASS_KIND_MINING_BUILDING: u64 = 0x100000;
pub const EE_CLASS_KIND_GATE: u64 = 0x200000;
pub const EE_CLASS_KIND_BUILDING_PRIEST: u64 = 0x400000;
pub const EE_CLASS_KIND_SPACE_BUILDING: u64 = 0x800000;
pub const EE_CLASS_KIND_SUBMERGED_NAVAL: u64 = 0x1000000;
pub const EE_CLASS_KIND_SEA_ANIMAL: u64 = 0x2000000;
pub const EE_CLASS_KIND_AIR_ANIMAL: u64 = 0x4000000;
pub const EE_CLASS_KIND_TRANSPORT: u64 = 0x8000000;
pub const EE_CLASS_KIND_LAND_TRANSPORT: u64 = 0x10000000;
pub const EE_CLASS_KIND_HELICOPTER_TRANSPORT: u64 = 0x20000000;
pub const EE_CLASS_KIND_NAVAL_TRANSPORT: u64 = 0x40000000;
pub const EE_CLASS_KIND_SPACE_TRANSPORT: u64 = 0x80000000;
pub const EE_CLASS_KIND_FORTRESS: u64 = 0x100000000;
pub const EE_CLASS_KIND_LAND_MILITARY_HERO: u64 = 0x200000000;
pub const EE_CLASS_KIND_SPACESHIP: u64 = 0x400000000;
pub const EE_CLASS_KIND_PLANET_SPACESHIP: u64 = 0x800000000;

/// The goal classes of `EEUnitGoal::mKind` (`EEUnitGoalKind`), by kind; 0 for a kind no goal has.
pub const fn goal_kinds_of(kind: u8) -> u32 {
    match kind {
        1 => EE_GOAL_KIND_LOCATION,
        2 => EE_GOAL_KIND_ATTACK_UNIT,
        3 => EE_GOAL_KIND_ATTACK_GROUND,
        4 => EE_GOAL_KIND_BUILD_REPAIR,
        6 => EE_GOAL_KIND_GATHER,
        7 => EE_GOAL_KIND_LOAD_TRANSPORT | EE_GOAL_KIND_LOAD_CONTAINER,
        8 => EE_GOAL_KIND_UNLOAD_TRANSPORT,
        9 => EE_GOAL_KIND_PATROL,
        10 => EE_GOAL_KIND_EXPLORE,
        11 => EE_GOAL_KIND_CAST_CALAMITY,
        12 => EE_GOAL_KIND_LOAD_GARRISON | EE_GOAL_KIND_LOAD_CONTAINER,
        15 => EE_GOAL_KIND_HEAL_UNIT | EE_GOAL_KIND_LOCATION,
        _ => 0,
    }
}

/// The unit classes the class of a category is (`EEUnitClassCategory`, `CreateUnitClasses`' switch); 0 for a
/// category it builds no class for.
pub const fn class_kinds_of(category: u32) -> u64 {
    match category {
        0 => EE_CLASS_KIND_AMBIENT,
        1 => EE_CLASS_KIND_RESOURCE,
        2 | 0x13 => EE_CLASS_KIND_BUILDING,
        3 => EE_CLASS_KIND_AIRPLANE | EE_CLASS_KIND_LANDABLE,
        4 => EE_CLASS_KIND_NAVAL,
        5 | 8 | 0xf => EE_CLASS_KIND_LAND,
        6 => EE_CLASS_KIND_LAND_CITIZEN | EE_CLASS_KIND_BASIC_CITIZEN,
        7 => EE_CLASS_KIND_HELICOPTER,
        9 => EE_CLASS_KIND_PROJECTILE,
        0xa => EE_CLASS_KIND_SUBMERGED_NAVAL,
        0xb => EE_CLASS_KIND_BOMB | EE_CLASS_KIND_PROJECTILE,
        0xc => EE_CLASS_KIND_MISSILE,
        0xd => EE_CLASS_KIND_LAND_PRIEST | EE_CLASS_KIND_BASIC_PRIEST,
        0xe => EE_CLASS_KIND_NAVAL_PRIEST | EE_CLASS_KIND_BASIC_PRIEST,
        0x10 => EE_CLASS_KIND_LAND_TRANSPORT | EE_CLASS_KIND_TRANSPORT,
        0x11 => EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_TRANSPORT,
        0x12 => EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_TRANSPORT,
        0x14 => EE_CLASS_KIND_TORPEDO | EE_CLASS_KIND_MISSILE,
        0x15 => EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_BUILDING,
        0x16 => EE_CLASS_KIND_NAVAL_CITIZEN | EE_CLASS_KIND_BASIC_CITIZEN,
        0x17 => EE_CLASS_KIND_LAND_MILITARY_HERO | EE_CLASS_KIND_LAND,
        0x18 => EE_CLASS_KIND_MINING_BUILDING | EE_CLASS_KIND_BUILDING,
        0x19 => EE_CLASS_KIND_GATE | EE_CLASS_KIND_BUILDING,
        0x1a => EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_BASIC_PRIEST,
        0x1b => EE_CLASS_KIND_SEA_ANIMAL | EE_CLASS_KIND_SUBMERGED_NAVAL,
        0x1c => EE_CLASS_KIND_AIR_ANIMAL | EE_CLASS_KIND_AIRPLANE | EE_CLASS_KIND_LANDABLE,
        0x1d => EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_LAND_TRANSPORT | EE_CLASS_KIND_TRANSPORT,
        0x1e => EE_CLASS_KIND_BUILDING_PRIEST | EE_CLASS_KIND_BUILDING,
        0x1f => EE_CLASS_KIND_SPACESHIP,
        0x20 => EE_CLASS_KIND_SPACE_BUILDING | EE_CLASS_KIND_BUILDING,
        0x21 => EE_CLASS_KIND_SPACE_TRANSPORT | EE_CLASS_KIND_TRANSPORT,
        0x22 => EE_CLASS_KIND_PLANET_SPACESHIP | EE_CLASS_KIND_LANDABLE,
        _ => 0,
    }
}

const fn goal_table() -> [u32; 256] {
    let mut t = [0u32; 256];
    let mut i = 0;
    while i < 256 {
        t[i] = goal_kinds_of(i as u8);
        i += 1;
    }
    t
}

const fn class_table() -> [u64; 256] {
    let mut t = [0u64; 256];
    let mut i = 0;
    while i < 256 {
        t[i] = class_kinds_of(i as u32);
        i += 1;
    }
    t
}

/// The goal kinds by `EEUnitGoal::mKind`, read by `EEGoalCast<T>` (ee/EEUnitKind.h).
#[no_mangle]
pub static EE_UNIT_GOAL_KINDS: [u32; 256] = goal_table();

/// The class kinds by `EEUnitClass::mCategory` (below 256), read by `EEUnitClassCast<T>` (ee/EEUnitKind.h).
#[no_mangle]
pub static EE_UNIT_CLASS_KINDS: [u64; 256] = class_table();

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn every_goal_kind_is_its_class_and_its_bases() {
        assert_eq!(EE_UNIT_GOAL_KINDS[1], EE_GOAL_KIND_LOCATION);
        assert_eq!(EE_UNIT_GOAL_KINDS[15] & EE_GOAL_KIND_LOCATION, EE_GOAL_KIND_LOCATION); // a heal goal is a location goal
        assert_eq!(EE_UNIT_GOAL_KINDS[7], EE_GOAL_KIND_LOAD_TRANSPORT | EE_GOAL_KIND_LOAD_CONTAINER);
        assert_eq!(EE_UNIT_GOAL_KINDS[12], EE_GOAL_KIND_LOAD_GARRISON | EE_GOAL_KIND_LOAD_CONTAINER);
        // the kinds no goal has (5, 13, 14 and the rest) are no class
        for k in [0usize, 5, 13, 14, 16, 255] {
            assert_eq!(EE_UNIT_GOAL_KINDS[k], 0, "kind {k}");
        }
        // twelve classes each named by exactly one kind; the load container by two
        let mut owners = [0; 13];
        for kinds in EE_UNIT_GOAL_KINDS {
            for (b, o) in owners.iter_mut().enumerate() {
                *o += (kinds >> b) & 1;
            }
        }
        assert_eq!(owners, [2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1]);
    }

    #[test]
    fn every_category_is_its_class_and_its_bases() {
        assert_eq!(EE_UNIT_CLASS_KINDS[0x19], EE_CLASS_KIND_GATE | EE_CLASS_KIND_BUILDING);
        assert_eq!(
            EE_UNIT_CLASS_KINDS[0x1d],
            EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_LAND_TRANSPORT | EE_CLASS_KIND_TRANSPORT
        );
        assert_eq!(
            EE_UNIT_CLASS_KINDS[0x1c],
            EE_CLASS_KIND_AIR_ANIMAL | EE_CLASS_KIND_AIRPLANE | EE_CLASS_KIND_LANDABLE
        );
        // the three categories no record has still build their classes (CreateUnitClasses' cases)
        assert_eq!(EE_UNIT_CLASS_KINDS[8], EE_CLASS_KIND_LAND);
        assert_eq!(EE_UNIT_CLASS_KINDS[0xf], EE_CLASS_KIND_LAND);
        assert_eq!(EE_UNIT_CLASS_KINDS[0x13], EE_CLASS_KIND_BUILDING);
        assert_eq!(EE_UNIT_CLASS_KINDS[0x23], 0);
        assert_eq!(EE_UNIT_CLASS_KINDS[255], 0);
        // the 35 categories, each a distinct class except the land and building twins
        let distinct: std::collections::BTreeSet<u64> = (0..0x23).map(|c| EE_UNIT_CLASS_KINDS[c]).collect();
        assert_eq!(distinct.len(), 35 - 3);
        assert!((0..0x23).all(|c| EE_UNIT_CLASS_KINDS[c] != 0));
        // the transports: helicopter, land, naval, space, and the fortress through the land transport
        let transports = (0..0x23).filter(|&c| EE_UNIT_CLASS_KINDS[c] & EE_CLASS_KIND_TRANSPORT != 0).count();
        assert_eq!(transports, 5);
        let buildings = (0..0x23).filter(|&c| EE_UNIT_CLASS_KINDS[c] & EE_CLASS_KIND_BUILDING != 0).count();
        assert_eq!(buildings, 7); // 2, 0x13, naval, mining, gate, priest, space
    }
}
