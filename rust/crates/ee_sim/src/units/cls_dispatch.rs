//! The unit classes' dispatch (R5b.4, the thirty-fourth round): which of `EEUnitClass`'s bodies each virtual runs
//! for a class, by its kinds. EEUnitClassKinds.cpp's 37 dispatchers tested the kinds deepest first, as the vtables of
//! the 36 `EEUC*` classes resolved an override (round 32); the order of those tests is this table, and each dispatcher
//! is a `switch` over its row's answer for the class's category ([`EE_UNIT_CLASS_BODIES`], read in place by
//! `EE_CLASS_BODY`, ee/EEUnitKind.h): 0 the default body, n the n-th body of the row.

// the table's indices are the C++ switch's: usize in, u8 out
#![allow(clippy::cast_possible_truncation)]

use crate::units::kinds::*;

/// `EEUnitClass::ActionChanged`
pub const EE_UNITS_BODY_ACTION_CHANGED: usize = 0;
/// `EEUnitClass::AddToRegion`
pub const EE_UNITS_BODY_ADD_TO_REGION: usize = 1;
/// `EEUnitClass::AddToTile`
pub const EE_UNITS_BODY_ADD_TO_TILE: usize = 2;
/// `EEUnitClass::AdjustPath`
pub const EE_UNITS_BODY_ADJUST_PATH: usize = 3;
/// `EEUnitClass::BadlyHurt`
pub const EE_UNITS_BODY_BADLY_HURT: usize = 4;
/// `EEUnitClass::CallParatroopPlane`
pub const EE_UNITS_BODY_CALL_PARATROOP_PLANE: usize = 5;
/// `EEUnitClass::CanAttackMove`
pub const EE_UNITS_BODY_CAN_ATTACK_MOVE: usize = 6;
/// `EEUnitClass::CargoAdded`
pub const EE_UNITS_BODY_CARGO_ADDED: usize = 7;
/// `EEUnitClass::CargoRemoved`
pub const EE_UNITS_BODY_CARGO_REMOVED: usize = 8;
/// `EEUnitClass::CargoUnloaded`
pub const EE_UNITS_BODY_CARGO_UNLOADED: usize = 9;
/// `EEUnitClass::CheckGroundImpact`
pub const EE_UNITS_BODY_CHECK_GROUND_IMPACT: usize = 10;
/// `EEUnitClass::CheckWaterImpact`
pub const EE_UNITS_BODY_CHECK_WATER_IMPACT: usize = 11;
/// `EEUnitClass::CreateUnit`
pub const EE_UNITS_BODY_CREATE_UNIT: usize = 12;
/// `EEUnitClass::CreateUnitAt`
pub const EE_UNITS_BODY_CREATE_UNIT_AT: usize = 13;
/// `EEUnitClass::FliesOver`
pub const EE_UNITS_BODY_FLIES_OVER: usize = 14;
/// `EEUnitClass::GateReplaced`
pub const EE_UNITS_BODY_GATE_REPLACED: usize = 15;
/// `EEUnitClass::GetSoundParameters`
pub const EE_UNITS_BODY_GET_SOUND_PARAMETERS: usize = 16;
/// `EEUnitClass::GetUnloadFailedTextID`
pub const EE_UNITS_BODY_GET_UNLOAD_FAILED_TEXT_ID: usize = 17;
/// `EEUnitClass::GroundLocation`
pub const EE_UNITS_BODY_GROUND_LOCATION: usize = 18;
/// `EEUnitClass::HasSunk`
pub const EE_UNITS_BODY_HAS_SUNK: usize = 19;
/// `EEUnitClass::IsBuilding`
pub const EE_UNITS_BODY_IS_BUILDING: usize = 20;
/// `EEUnitClass::IsLandBuilding`
pub const EE_UNITS_BODY_IS_LAND_BUILDING: usize = 21;
/// `EEUnitClass::IsPartLeftOut`
pub const EE_UNITS_BODY_IS_PART_LEFT_OUT: usize = 22;
/// `EEUnitClass::IsPickable`
pub const EE_UNITS_BODY_IS_PICKABLE: usize = 23;
/// `EEUnitClass::IsSelectable`
pub const EE_UNITS_BODY_IS_SELECTABLE: usize = 24;
/// `EEUnitClass::LaunchProjectile`
pub const EE_UNITS_BODY_LAUNCH_PROJECTILE: usize = 25;
/// `EEUnitClass::LoadMedia`
pub const EE_UNITS_BODY_LOAD_MEDIA: usize = 26;
/// `EEUnitClass::LocationToTile`
pub const EE_UNITS_BODY_LOCATION_TO_TILE: usize = 27;
/// `EEUnitClass::RemoveFromRegion`
pub const EE_UNITS_BODY_REMOVE_FROM_REGION: usize = 28;
/// `EEUnitClass::RemoveFromTile`
pub const EE_UNITS_BODY_REMOVE_FROM_TILE: usize = 29;
/// `EEUnitClass::SimpleUnitCreated`
pub const EE_UNITS_BODY_SIMPLE_UNIT_CREATED: usize = 30;
/// `EEUnitClass::UnitCreated`
pub const EE_UNITS_BODY_UNIT_CREATED: usize = 31;
/// `EEUnitClass::UnloadAt`
pub const EE_UNITS_BODY_UNLOAD_AT: usize = 32;
/// `EEUnitClass::UnloadMedia`
pub const EE_UNITS_BODY_UNLOAD_MEDIA: usize = 33;
/// `EEUnitClass::Update`
pub const EE_UNITS_BODY_UPDATE: usize = 34;
/// `EEUnitClass::UpdateEffects`
pub const EE_UNITS_BODY_UPDATE_EFFECTS: usize = 35;
/// `EEUnitClass::Virtual70`
pub const EE_UNITS_BODY_VIRTUAL70: usize = 36;

/// Each virtual's bodies in the order the dispatcher tests them: the kinds that select body n (from 1).
const BODIES: [&[u64]; EE_UNITS_BODY_COUNT] = [
    &[
        EE_CLASS_KIND_AIR_ANIMAL,
        EE_CLASS_KIND_AIRPLANE,
        EE_CLASS_KIND_BUILDING_PRIEST,
        EE_CLASS_KIND_GATE,
        EE_CLASS_KIND_HELICOPTER_PRIEST,
        EE_CLASS_KIND_HELICOPTER_TRANSPORT,
        EE_CLASS_KIND_MINING_BUILDING,
        EE_CLASS_KIND_PLANET_SPACESHIP,
        EE_CLASS_KIND_BASIC_CITIZEN,
        EE_CLASS_KIND_BASIC_PRIEST,
        EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_HELICOPTER,
        EE_CLASS_KIND_LAND,
        EE_CLASS_KIND_LANDABLE,
        EE_CLASS_KIND_MISSILE,
        EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_PROJECTILE,
        EE_CLASS_KIND_SPACESHIP,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // ActionChanged
    &[EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING, EE_CLASS_KIND_MISSILE | EE_CLASS_KIND_PROJECTILE], // AddToRegion
    &[
        EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_HELICOPTER_PRIEST
            | EE_CLASS_KIND_HELICOPTER_TRANSPORT
            | EE_CLASS_KIND_NAVAL_TRANSPORT
            | EE_CLASS_KIND_SPACE_TRANSPORT
            | EE_CLASS_KIND_HELICOPTER
            | EE_CLASS_KIND_LANDABLE
            | EE_CLASS_KIND_NAVAL
            | EE_CLASS_KIND_SPACESHIP,
        EE_CLASS_KIND_AMBIENT,
        EE_CLASS_KIND_RESOURCE,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // AddToTile
    &[
        EE_CLASS_KIND_NAVAL_CITIZEN,
        EE_CLASS_KIND_NAVAL_PRIEST,
        EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_SEA_ANIMAL,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // AdjustPath
    &[EE_CLASS_KIND_LAND_TRANSPORT],                                                                      // BadlyHurt
    &[EE_CLASS_KIND_LAND], // CallParatroopPlane
    &[
        EE_CLASS_KIND_AIR_ANIMAL,
        EE_CLASS_KIND_HELICOPTER_TRANSPORT
            | EE_CLASS_KIND_BASIC_PRIEST
            | EE_CLASS_KIND_HELICOPTER
            | EE_CLASS_KIND_LAND
            | EE_CLASS_KIND_LANDABLE
            | EE_CLASS_KIND_NAVAL
            | EE_CLASS_KIND_SPACESHIP
            | EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // CanAttackMove
    &[EE_CLASS_KIND_FORTRESS], // CargoAdded
    &[EE_CLASS_KIND_FORTRESS], // CargoRemoved
    &[EE_CLASS_KIND_FORTRESS], // CargoUnloaded
    &[EE_CLASS_KIND_TORPEDO, EE_CLASS_KIND_MISSILE, EE_CLASS_KIND_PROJECTILE], // CheckGroundImpact
    &[EE_CLASS_KIND_TORPEDO, EE_CLASS_KIND_MISSILE, EE_CLASS_KIND_PROJECTILE], // CheckWaterImpact
    &[
        EE_CLASS_KIND_MINING_BUILDING,
        EE_CLASS_KIND_NAVAL_BUILDING
            | EE_CLASS_KIND_NAVAL_CITIZEN
            | EE_CLASS_KIND_NAVAL_PRIEST
            | EE_CLASS_KIND_NAVAL_TRANSPORT
            | EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_SPACE_BUILDING,
        EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // CreateUnit
    &[
        EE_CLASS_KIND_MINING_BUILDING,
        EE_CLASS_KIND_NAVAL_BUILDING
            | EE_CLASS_KIND_NAVAL_CITIZEN
            | EE_CLASS_KIND_NAVAL_PRIEST
            | EE_CLASS_KIND_NAVAL_TRANSPORT
            | EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_SPACE_BUILDING,
        EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // CreateUnitAt
    &[EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER, EE_CLASS_KIND_TRANSPORT], // FliesOver
    &[EE_CLASS_KIND_GATE], // GateReplaced
    &[
        EE_CLASS_KIND_AIR_ANIMAL,
        EE_CLASS_KIND_HELICOPTER_PRIEST,
        EE_CLASS_KIND_HELICOPTER_TRANSPORT | EE_CLASS_KIND_HELICOPTER | EE_CLASS_KIND_SPACESHIP,
        EE_CLASS_KIND_LAND_PRIEST,
        EE_CLASS_KIND_SPACE_TRANSPORT,
        EE_CLASS_KIND_LANDABLE,
    ], // GetSoundParameters
    &[EE_CLASS_KIND_FORTRESS, EE_CLASS_KIND_TRANSPORT], // GetUnloadFailedTextID
    &[
        EE_CLASS_KIND_NAVAL_BUILDING,
        EE_CLASS_KIND_NAVAL_CITIZEN | EE_CLASS_KIND_NAVAL_TRANSPORT | EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_SPACE_BUILDING,
        EE_CLASS_KIND_AMBIENT,
        EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // GroundLocation
    &[EE_CLASS_KIND_NAVAL_BUILDING, EE_CLASS_KIND_SPACE_BUILDING], // HasSunk
    &[EE_CLASS_KIND_BUILDING], // IsBuilding
    &[EE_CLASS_KIND_FORTRESS, EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING, EE_CLASS_KIND_BUILDING], // IsLandBuilding
    &[EE_CLASS_KIND_GATE],                                                  // IsPartLeftOut
    &[EE_CLASS_KIND_AMBIENT, EE_CLASS_KIND_BUILDING],                       // IsPickable
    &[EE_CLASS_KIND_AMBIENT, EE_CLASS_KIND_BUILDING],                       // IsSelectable
    &[EE_CLASS_KIND_BOMB, EE_CLASS_KIND_MISSILE, EE_CLASS_KIND_PROJECTILE], // LaunchProjectile
    &[EE_CLASS_KIND_BUILDING_PRIEST, EE_CLASS_KIND_BASIC_PRIEST],           // LoadMedia
    &[
        EE_CLASS_KIND_NAVAL_BUILDING
            | EE_CLASS_KIND_NAVAL_CITIZEN
            | EE_CLASS_KIND_NAVAL_TRANSPORT
            | EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_SPACE_BUILDING,
        EE_CLASS_KIND_AMBIENT,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
    ], // LocationToTile
    &[EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING, EE_CLASS_KIND_MISSILE | EE_CLASS_KIND_PROJECTILE], // RemoveFromRegion
    &[
        EE_CLASS_KIND_FORTRESS | EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_HELICOPTER_PRIEST
            | EE_CLASS_KIND_HELICOPTER_TRANSPORT
            | EE_CLASS_KIND_NAVAL_TRANSPORT
            | EE_CLASS_KIND_SPACE_TRANSPORT
            | EE_CLASS_KIND_HELICOPTER
            | EE_CLASS_KIND_LANDABLE
            | EE_CLASS_KIND_NAVAL
            | EE_CLASS_KIND_SPACESHIP
            | EE_CLASS_KIND_SUBMERGED_NAVAL,
        EE_CLASS_KIND_AMBIENT | EE_CLASS_KIND_RESOURCE,
    ], // RemoveFromTile
    &[EE_CLASS_KIND_RESOURCE], // SimpleUnitCreated
    &[
        EE_CLASS_KIND_AIR_ANIMAL,
        EE_CLASS_KIND_FORTRESS,
        EE_CLASS_KIND_GATE,
        EE_CLASS_KIND_HELICOPTER_PRIEST | EE_CLASS_KIND_HELICOPTER | EE_CLASS_KIND_SPACESHIP,
        EE_CLASS_KIND_HELICOPTER_TRANSPORT,
        EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING,
        EE_CLASS_KIND_SPACE_TRANSPORT,
        EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_LAND,
        EE_CLASS_KIND_LANDABLE,
        EE_CLASS_KIND_MISSILE,
        EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_PROJECTILE,
        EE_CLASS_KIND_TRANSPORT,
    ], // UnitCreated
    &[EE_CLASS_KIND_FORTRESS, EE_CLASS_KIND_TRANSPORT], // UnloadAt
    &[EE_CLASS_KIND_BUILDING_PRIEST, EE_CLASS_KIND_BASIC_PRIEST], // UnloadMedia
    &[
        EE_CLASS_KIND_AIR_ANIMAL,
        EE_CLASS_KIND_FORTRESS,
        EE_CLASS_KIND_BOMB,
        EE_CLASS_KIND_BUILDING_PRIEST,
        EE_CLASS_KIND_GATE,
        EE_CLASS_KIND_HELICOPTER_TRANSPORT,
        EE_CLASS_KIND_LAND_CITIZEN,
        EE_CLASS_KIND_LAND_TRANSPORT,
        EE_CLASS_KIND_MINING_BUILDING,
        EE_CLASS_KIND_NAVAL_BUILDING,
        EE_CLASS_KIND_NAVAL_CITIZEN,
        EE_CLASS_KIND_SPACE_BUILDING,
        EE_CLASS_KIND_TORPEDO,
        EE_CLASS_KIND_AMBIENT,
        EE_CLASS_KIND_BASIC_CITIZEN,
        EE_CLASS_KIND_BASIC_PRIEST,
        EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_HELICOPTER,
        EE_CLASS_KIND_LAND,
        EE_CLASS_KIND_LANDABLE,
        EE_CLASS_KIND_MISSILE,
        EE_CLASS_KIND_NAVAL,
        EE_CLASS_KIND_PROJECTILE,
        EE_CLASS_KIND_SPACESHIP,
        EE_CLASS_KIND_SUBMERGED_NAVAL,
        EE_CLASS_KIND_TRANSPORT,
    ], // Update
    &[
        EE_CLASS_KIND_AIR_ANIMAL
            | EE_CLASS_KIND_FORTRESS
            | EE_CLASS_KIND_HELICOPTER_PRIEST
            | EE_CLASS_KIND_HELICOPTER_TRANSPORT
            | EE_CLASS_KIND_SPACE_BUILDING,
        EE_CLASS_KIND_AIRPLANE,
        EE_CLASS_KIND_NAVAL_BUILDING
            | EE_CLASS_KIND_SEA_ANIMAL
            | EE_CLASS_KIND_TORPEDO
            | EE_CLASS_KIND_BASIC_CITIZEN
            | EE_CLASS_KIND_BASIC_PRIEST
            | EE_CLASS_KIND_NAVAL
            | EE_CLASS_KIND_TRANSPORT,
        EE_CLASS_KIND_BUILDING,
        EE_CLASS_KIND_LAND,
        EE_CLASS_KIND_MISSILE,
        EE_CLASS_KIND_PROJECTILE,
    ], // UpdateEffects
    &[EE_CLASS_KIND_BUILDING, EE_CLASS_KIND_RESOURCE], // Virtual70
];

/// The number of dispatched virtuals.
pub const EE_UNITS_BODY_COUNT: usize = 37;
const _: () = assert!(BODIES.len() == EE_UNITS_BODY_COUNT);

/// A row's body for a class of the given kinds: the first entry whose kinds it has (from 1), 0 for none.
pub const fn body_of(row: &[u64], kinds: u64) -> u8 {
    let mut i = 0;
    while i < row.len() {
        if kinds & row[i] != 0 {
            return (i + 1) as u8;
        }
        i += 1;
    }
    0
}

const fn body_table() -> [[u8; 256]; EE_UNITS_BODY_COUNT] {
    let mut t = [[0u8; 256]; EE_UNITS_BODY_COUNT];
    let mut v = 0;
    while v < EE_UNITS_BODY_COUNT {
        let mut c = 0;
        while c < 256 {
            t[v][c] = body_of(BODIES[v], EE_UNIT_CLASS_KINDS[c]);
            c += 1;
        }
        v += 1;
    }
    t
}

/// The body each virtual runs by `EEUnitClass::mCategory` (below 256), read by `EE_CLASS_BODY` (ee/EEUnitKind.h).
#[no_mangle]
pub static EE_UNIT_CLASS_BODIES: [[u8; 256]; EE_UNITS_BODY_COUNT] = body_table();

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_deepest_kind_wins_in_the_dispatchers_order() {
        // a gate is a building: its own update, its building's surrounding tiles and the building's facts
        let gate = 0x19;
        assert_eq!(EE_UNIT_CLASS_KINDS[gate], EE_CLASS_KIND_GATE | EE_CLASS_KIND_BUILDING);
        assert_eq!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_UPDATE][gate], 5);
        assert_eq!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_IS_BUILDING][gate], 1);
        assert_eq!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_GATE_REPLACED][gate], 1);
        assert_eq!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_IS_LAND_BUILDING][gate], 3);
        // a land unit: no building body, the land update, it can attack-move
        assert_eq!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_IS_BUILDING][8], 0);
        assert_eq!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_UPDATE][8], 19);
        assert_eq!(EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_CAN_ATTACK_MOVE][8], 2);
        // a category past the table runs every default
        assert!((0..EE_UNITS_BODY_COUNT).all(|v| EE_UNIT_CLASS_BODIES[v][255] == 0));
    }

    #[test]
    fn a_row_takes_its_first_match() {
        assert_eq!(body_of(&[EE_CLASS_KIND_GATE, EE_CLASS_KIND_BUILDING], EE_CLASS_KIND_BUILDING), 2);
        assert_eq!(
            body_of(&[EE_CLASS_KIND_GATE, EE_CLASS_KIND_BUILDING], EE_CLASS_KIND_GATE | EE_CLASS_KIND_BUILDING),
            1
        );
        assert_eq!(body_of(&[EE_CLASS_KIND_GATE], EE_CLASS_KIND_LAND), 0);
    }

    #[test]
    fn the_condition_module_agrees_on_attack_move() {
        for c in 0..256 {
            let k = EE_UNIT_CLASS_KINDS[c];
            assert_eq!(
                EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_CAN_ATTACK_MOVE][c] == 2,
                crate::units::cls_condition::can_attack_move(k)
            );
        }
    }
}
