//! `EEUnitClass`'s kind dispatchers that were a switch of calls (the units' lane, round 9; were
//! EEUnitClassKinds.cpp): `AddToRegion`, `AddToTile`, `BadlyHurt`, `CallParatroopPlane`, `CargoAdded`,
//! `CargoRemoved`, `CargoUnloaded`, `CheckGroundImpact`, `CheckWaterImpact`, `CreateUnit`, `CreateUnitAt`,
//! `FliesOver`, `GateReplaced`, `GetSoundParameters`, `GetUnloadFailedTextID`, `GroundLocation`, `HasSunk`,
//! `IsLandBuilding`, `IsPartLeftOut`, `IsPickable`, `IsSelectable`, `LaunchProjectile`, `LoadMedia`,
//! `LocationToTile`, `RemoveFromRegion`, `RemoveFromTile`, `SimpleUnitCreated`, `UnitCreated`, `UnloadAt`,
//! `UnloadMedia`. The class's row of the dispatch table (cls_dispatch.rs, `EEClassBody`) chooses the body, as the
//! vtables did; a C++ body is one `EE_UNITS_UCK_D_*` case of `ee_units_uck_host`, a Rust one (the footprints, the
//! base tile, unit creation and sounds, the priests' media and creation) is called here.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::cls_dispatch::*;
use crate::units::uc_body::UcHost;
use crate::units::uc_create::{base_unit_created, S};
use crate::units::uc_produce::EE_UNITS_UC_THIS_UNIT_TYPE;
use crate::units::uc_rest::{
    self, SoundOuts, EE_UNITS_UC_R_BASE_LOAD_MEDIA, EE_UNITS_UC_R_BASE_UNLOAD_MEDIA, EE_UNITS_UC_R_CRUISE_ALTITUDE,
};
use crate::units::uck_kinds::EE_UNITS_UCK_BODY;

// ---- the statements (3500..3599)
/// `class->BaseAddToRegion(unit a)`
pub const EE_UNITS_UCK_D_BASE_ADD_TO_REGION: i32 = 3500;
/// `class->SpaceTransportAddToTile(unit a)`
pub const EE_UNITS_UCK_D_SPACE_TRANSPORT_ADD_TO_TILE: i32 = 3501;
/// `class->AmbientAddToTile(unit a)`
pub const EE_UNITS_UCK_D_AMBIENT_ADD_TO_TILE: i32 = 3502;
/// `class->ResourceAddToTile(unit a)`
pub const EE_UNITS_UCK_D_RESOURCE_ADD_TO_TILE: i32 = 3503;
/// `class->SubmergedNavalAddToTile(unit a)`
pub const EE_UNITS_UCK_D_SUBMERGED_ADD_TO_TILE: i32 = 3504;
/// `class->LandTransportBadlyHurt(unit a)`
pub const EE_UNITS_UCK_D_LAND_TRANSPORT_BADLY_HURT: i32 = 3505;
/// `class->LandCallParatroopPlane(unit)`
pub const EE_UNITS_UCK_D_LAND_CALL_PARATROOP_PLANE: i32 = 3506;
/// `unit->mPlayer->mPopulationCount -= ` cargo `a`'s `mUnitClass->GetPopulationCost()`
pub const EE_UNITS_UCK_D_POPULATION_SUB: i32 = 3507;
/// `unit->mPlayer->mPopulationCount += ` cargo `a`'s `mUnitClass->GetPopulationCost()`
pub const EE_UNITS_UCK_D_POPULATION_ADD: i32 = 3508;
/// `class->TorpedoCheckGroundImpact(unit)`
pub const EE_UNITS_UCK_D_TORPEDO_GROUND_IMPACT: i32 = 3509;
/// `class->MissileCheckGroundImpact(unit)`
pub const EE_UNITS_UCK_D_MISSILE_GROUND_IMPACT: i32 = 3510;
/// `class->ProjectileCheckGroundImpact(unit)`
pub const EE_UNITS_UCK_D_PROJECTILE_GROUND_IMPACT: i32 = 3511;
/// `class->MissileCheckWaterImpact(unit)`
pub const EE_UNITS_UCK_D_MISSILE_WATER_IMPACT: i32 = 3512;
/// `class->ProjectileCheckWaterImpact(unit)`
pub const EE_UNITS_UCK_D_PROJECTILE_WATER_IMPACT: i32 = 3513;
/// `class->MiningBuildingCreateUnit(b's x, b's y, c >> 1, c & 1, creator a)` (the next four likewise)
pub const EE_UNITS_UCK_D_MINING_CREATE_UNIT: i32 = 3514;
pub const EE_UNITS_UCK_D_NAVAL_CITIZEN_CREATE_UNIT: i32 = 3515;
pub const EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT: i32 = 3516;
pub const EE_UNITS_UCK_D_BUILDING_CREATE_UNIT: i32 = 3517;
pub const EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT: i32 = 3518;
pub const EE_UNITS_UCK_D_BASE_CREATE_UNIT: i32 = 3519;
/// `class->MiningBuildingCreateUnitAt(the transformation at b, c >> 1, c & 1, creator a)` (the next five likewise)
pub const EE_UNITS_UCK_D_MINING_CREATE_UNIT_AT: i32 = 3520;
pub const EE_UNITS_UCK_D_NAVAL_PRIEST_CREATE_UNIT_AT: i32 = 3521;
pub const EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT_AT: i32 = 3522;
pub const EE_UNITS_UCK_D_BUILDING_CREATE_UNIT_AT: i32 = 3523;
pub const EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT_AT: i32 = 3524;
pub const EE_UNITS_UCK_D_BASE_CREATE_UNIT_AT: i32 = 3525;
/// `class->mDomains`
pub const EE_UNITS_UCK_D_DOMAINS: i32 = 3526;
/// `class->GateGateReplaced(gate a, unit b)`
pub const EE_UNITS_UCK_D_GATE_GATE_REPLACED: i32 = 3527;
/// `class->AirGetSoundParameters(unit a, sound b, the three out pointers at c)`
pub const EE_UNITS_UCK_D_AIR_SOUND: i32 = 3528;
/// `class->BuildingGroundLocation(f[0..3])`
pub const EE_UNITS_UCK_D_BUILDING_GROUND_LOCATION: i32 = 3530;
/// `class->SubmergedNavalGroundLocation(f[0..3])`
pub const EE_UNITS_UCK_D_SUBMERGED_GROUND_LOCATION: i32 = 3531;
/// `class->NavalBuildingHasSunk(unit)`
pub const EE_UNITS_UCK_D_NAVAL_BUILDING_SUNK: i32 = 3532;
/// `class->IsSidePart(unit)`
pub const EE_UNITS_UCK_D_IS_SIDE_PART: i32 = 3533;
/// `EEGetFirstPart(unit)->GetCommandType()`
pub const EE_UNITS_UCK_D_FIRST_PART_COMMAND: i32 = 3534;
/// `class->mIsFlare`
pub const EE_UNITS_UCK_D_IS_FLARE: i32 = 3535;
/// unit `a`'s `mUnitClass->mUnitType`
pub const EE_UNITS_UCK_D_UNIT_TYPE_OF: i32 = 3536;
/// `class->BombLaunchProjectile(unit, the point at a)` (the next two likewise)
pub const EE_UNITS_UCK_D_BOMB_LAUNCH: i32 = 3537;
pub const EE_UNITS_UCK_D_MISSILE_LAUNCH: i32 = 3538;
pub const EE_UNITS_UCK_D_PROJECTILE_LAUNCH: i32 = 3539;
/// `class->NavalCitizenLocationToTile(the point at a, the long at b, the long at c)` (the next three likewise)
pub const EE_UNITS_UCK_D_NAVAL_CITIZEN_TO_TILE: i32 = 3540;
pub const EE_UNITS_UCK_D_SPACE_BUILDING_TO_TILE: i32 = 3541;
pub const EE_UNITS_UCK_D_AMBIENT_TO_TILE: i32 = 3542;
pub const EE_UNITS_UCK_D_SUBMERGED_TO_TILE: i32 = 3543;
/// `class->BaseRemoveFromRegion(unit a)`
pub const EE_UNITS_UCK_D_BASE_REMOVE_FROM_REGION: i32 = 3544;
/// `class->SpaceTransportRemoveFromTile(unit a)`
pub const EE_UNITS_UCK_D_SPACE_TRANSPORT_REMOVE: i32 = 3545;
/// `class->AmbientRemoveFromTile(unit a)`
pub const EE_UNITS_UCK_D_AMBIENT_REMOVE: i32 = 3546;
/// `class->ResourceSimpleUnitCreated(unit a)`
pub const EE_UNITS_UCK_D_RESOURCE_SIMPLE_CREATED: i32 = 3547;
/// `class->BaseSimpleUnitCreated(unit a)`
pub const EE_UNITS_UCK_D_BASE_SIMPLE_CREATED: i32 = 3548;
/// `UnitCreated`'s bodies 1..14 (`3548 + body`): `class->AirAnimalUnitCreated(unit)`, `FortressUnitCreated`,
/// `GateUnitCreated`, (4: Rust's), `HelicopterTransportUnitCreated`, `SpaceBuildingUnitCreated`,
/// `SpaceTransportUnitCreated`, `BuildingUnitCreated`, `LandUnitCreated`, `LandableUnitCreated`,
/// `MissileUnitCreated`, `NavalUnitCreated`, `ProjectileUnitCreated`, `TransportUnitCreated`
pub const EE_UNITS_UCK_D_AIR_ANIMAL_CREATED: i32 = 3549;
pub const EE_UNITS_UCK_D_FORTRESS_CREATED: i32 = 3550;
pub const EE_UNITS_UCK_D_GATE_CREATED: i32 = 3551;
pub const EE_UNITS_UCK_D_HELICOPTER_TRANSPORT_CREATED: i32 = 3553;
pub const EE_UNITS_UCK_D_SPACE_BUILDING_CREATED: i32 = 3554;
pub const EE_UNITS_UCK_D_SPACE_TRANSPORT_CREATED: i32 = 3555;
pub const EE_UNITS_UCK_D_BUILDING_CREATED: i32 = 3556;
pub const EE_UNITS_UCK_D_LAND_CREATED: i32 = 3557;
pub const EE_UNITS_UCK_D_LANDABLE_CREATED: i32 = 3558;
pub const EE_UNITS_UCK_D_MISSILE_CREATED: i32 = 3559;
pub const EE_UNITS_UCK_D_NAVAL_CREATED: i32 = 3560;
pub const EE_UNITS_UCK_D_PROJECTILE_CREATED: i32 = 3561;
pub const EE_UNITS_UCK_D_TRANSPORT_CREATED: i32 = 3562;
/// `class->FortressUnloadAt(unit, b, c)`
pub const EE_UNITS_UCK_D_FORTRESS_UNLOAD_AT: i32 = 3563;
/// `class->TransportUnloadAt(unit, b, c)`
pub const EE_UNITS_UCK_D_TRANSPORT_UNLOAD_AT: i32 = 3564;

const MOVEMENT_AIR: i64 = 1;
const UNIT_TYPE_AMB_CATTAILS_1: i64 = 0x234;
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_VOLCANO: i64 = 0x16e;
const ACTION_CLOSED: i64 = 15001;

/// The unit utilities' footprint calls the dispatchers make (as the C++ made them).
pub trait UckRust {
    /// `ee_units_uu_footprint(which, unit)`: 0 link, 1 unlink, 2 the building's region, 3 off it
    fn footprint(&mut self, which: i32, unit: u64);
}

fn body<H: UcHost + ?Sized>(s: &mut S<'_, H>, row: usize) -> i64 {
    s.b(EE_UNITS_UCK_BODY, row as i64)
}

/// `UnitCreated`'s helicopter priest body
const UNIT_CREATED_HELICOPTER_PRIEST: i64 = 4;

/// The dispatcher `row` (`EE_UNITS_BODY_*`) of `class` on `unit`: `a` its object, `b` and `c` its values, `f` its
/// point; the answer (a bool, a unit, a text id), or 0.
///
/// # Safety
/// `f` is three writable floats for `GroundLocation` (or NULL).
#[allow(clippy::too_many_arguments)]
pub unsafe fn dispatch<H: UcHost + UckRust + ?Sized>(
    h: &mut H,
    row: usize,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    f: *mut f32,
) -> i64 {
    let mut s = S { h: &mut *h, class, unit };
    let chosen = body(&mut s, row);
    match row {
        EE_UNITS_BODY_ADD_TO_REGION => match chosen {
            1 => h.footprint(2, a),
            2 => {}
            _ => {
                s.a(EE_UNITS_UCK_D_BASE_ADD_TO_REGION, a);
            }
        },
        EE_UNITS_BODY_ADD_TO_TILE => match chosen {
            1 => h.footprint(0, a),
            2 => return s.a(EE_UNITS_UCK_D_SPACE_TRANSPORT_ADD_TO_TILE, a),
            3 => return s.a(EE_UNITS_UCK_D_AMBIENT_ADD_TO_TILE, a),
            4 => return s.a(EE_UNITS_UCK_D_RESOURCE_ADD_TO_TILE, a),
            5 => return s.a(EE_UNITS_UCK_D_SUBMERGED_ADD_TO_TILE, a),
            _ => uc_rest::base_tile(h, class, a, true),
        },
        EE_UNITS_BODY_BADLY_HURT => {
            if chosen == 1 {
                s.a(EE_UNITS_UCK_D_LAND_TRANSPORT_BADLY_HURT, a);
            }
        }
        EE_UNITS_BODY_CALL_PARATROOP_PLANE => {
            if chosen == 1 {
                s.s(EE_UNITS_UCK_D_LAND_CALL_PARATROOP_PLANE);
            }
        }
        EE_UNITS_BODY_CARGO_ADDED => {
            if chosen == 1 && b == 0 {
                s.a(EE_UNITS_UCK_D_POPULATION_SUB, a);
            }
        }
        EE_UNITS_BODY_CARGO_REMOVED | EE_UNITS_BODY_CARGO_UNLOADED => {
            if chosen == 1 {
                s.a(EE_UNITS_UCK_D_POPULATION_ADD, a);
            }
        }
        EE_UNITS_BODY_CHECK_GROUND_IMPACT => {
            return match chosen {
                1 => s.s(EE_UNITS_UCK_D_TORPEDO_GROUND_IMPACT),
                2 => s.s(EE_UNITS_UCK_D_MISSILE_GROUND_IMPACT),
                3 => s.s(EE_UNITS_UCK_D_PROJECTILE_GROUND_IMPACT),
                _ => 0,
            }
        }
        EE_UNITS_BODY_CHECK_WATER_IMPACT => {
            return match chosen {
                2 => s.s(EE_UNITS_UCK_D_MISSILE_WATER_IMPACT),
                3 => s.s(EE_UNITS_UCK_D_PROJECTILE_WATER_IMPACT),
                _ => 0,
            }
        }
        EE_UNITS_BODY_CREATE_UNIT => {
            let op = match chosen {
                1 => EE_UNITS_UCK_D_MINING_CREATE_UNIT,
                2 => EE_UNITS_UCK_D_NAVAL_CITIZEN_CREATE_UNIT,
                3 => EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT,
                4 => EE_UNITS_UCK_D_BUILDING_CREATE_UNIT,
                5 => EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT,
                _ => EE_UNITS_UCK_D_BASE_CREATE_UNIT,
            };
            return s.abc(op, a, b, c);
        }
        EE_UNITS_BODY_CREATE_UNIT_AT => {
            let op = match chosen {
                1 => EE_UNITS_UCK_D_MINING_CREATE_UNIT_AT,
                2 => EE_UNITS_UCK_D_NAVAL_PRIEST_CREATE_UNIT_AT,
                3 => EE_UNITS_UCK_D_SPACE_BUILDING_CREATE_UNIT_AT,
                4 => EE_UNITS_UCK_D_BUILDING_CREATE_UNIT_AT,
                5 => EE_UNITS_UCK_D_SUBMERGED_CREATE_UNIT_AT,
                _ => EE_UNITS_UCK_D_BASE_CREATE_UNIT_AT,
            };
            return s.abc(op, a, b, c);
        }
        EE_UNITS_BODY_FLIES_OVER => {
            return match chosen {
                1 => 1,
                2 => i64::from(s.s(EE_UNITS_UCK_D_DOMAINS) & MOVEMENT_AIR != 0),
                _ => 0,
            }
        }
        EE_UNITS_BODY_GATE_REPLACED => {
            if chosen == 1 {
                s.ab(EE_UNITS_UCK_D_GATE_GATE_REPLACED, a, b);
            }
        }
        EE_UNITS_BODY_GET_UNLOAD_FAILED_TEXT_ID => {
            return match chosen {
                1 => 0x7c39,
                2 => 0x7c38,
                _ => 0,
            }
        }
        EE_UNITS_BODY_GROUND_LOCATION => {
            // SAFETY: the caller's point
            let p = unsafe { &mut *f.cast::<[f32; 3]>() };
            match chosen {
                1 => p[2] = 0.0,
                2 => {
                    if p[2] < 0.0 {
                        p[2] = 0.0;
                    }
                }
                3 => p[2] = s.float(EE_UNITS_UC_R_CRUISE_ALTITUDE, 0, 0),
                4 => {
                    if s.s(EE_UNITS_UC_THIS_UNIT_TYPE) == UNIT_TYPE_AMB_CATTAILS_1 {
                        p[2] = 0.0;
                    }
                }
                5 => {
                    s.point(EE_UNITS_UCK_D_BUILDING_GROUND_LOCATION, 0, 0, 0, p);
                }
                6 => {
                    s.point(EE_UNITS_UCK_D_SUBMERGED_GROUND_LOCATION, 0, 0, 0, p);
                }
                _ => {}
            }
        }
        EE_UNITS_BODY_HAS_SUNK => {
            return match chosen {
                1 => s.s(EE_UNITS_UCK_D_NAVAL_BUILDING_SUNK),
                2 => 1,
                _ => 0,
            }
        }
        EE_UNITS_BODY_IS_LAND_BUILDING => return i64::from(chosen == 1 || chosen == 3),
        EE_UNITS_BODY_IS_PART_LEFT_OUT => {
            return i64::from(
                chosen == 1
                    && s.s(EE_UNITS_UCK_D_IS_SIDE_PART) == 0
                    && s.s(EE_UNITS_UCK_D_FIRST_PART_COMMAND) != ACTION_CLOSED,
            )
        }
        EE_UNITS_BODY_IS_PICKABLE => {
            return match chosen {
                1 => 0,
                2 => i64::from(not_flare_or_volcano(&mut s)),
                _ => 1,
            }
        }
        EE_UNITS_BODY_IS_SELECTABLE => {
            return match chosen {
                1 => i64::from(s.a(EE_UNITS_UCK_D_UNIT_TYPE_OF, a) == UNIT_TYPE_PATH_POINT),
                2 => i64::from(not_flare_or_volcano(&mut s)),
                _ => 1,
            }
        }
        EE_UNITS_BODY_LAUNCH_PROJECTILE => {
            return match chosen {
                1 => s.a(EE_UNITS_UCK_D_BOMB_LAUNCH, a),
                2 => s.a(EE_UNITS_UCK_D_MISSILE_LAUNCH, a),
                3 => s.a(EE_UNITS_UCK_D_PROJECTILE_LAUNCH, a),
                _ => 0,
            }
        }
        EE_UNITS_BODY_LOAD_MEDIA => match chosen {
            1 | 2 => uc_rest::priest_media(h, class, a, true),
            _ => {
                s.a(EE_UNITS_UC_R_BASE_LOAD_MEDIA, a);
            }
        },
        EE_UNITS_BODY_UNLOAD_MEDIA => match chosen {
            1 | 2 => uc_rest::priest_media(h, class, a, false),
            _ => {
                s.a(EE_UNITS_UC_R_BASE_UNLOAD_MEDIA, a);
            }
        },
        EE_UNITS_BODY_LOCATION_TO_TILE => {
            let op = match chosen {
                1 => EE_UNITS_UCK_D_NAVAL_CITIZEN_TO_TILE,
                2 => EE_UNITS_UCK_D_SPACE_BUILDING_TO_TILE,
                3 => EE_UNITS_UCK_D_AMBIENT_TO_TILE,
                4 => EE_UNITS_UCK_D_SUBMERGED_TO_TILE,
                _ => return 0,
            };
            s.abc(op, a, b, c);
        }
        EE_UNITS_BODY_REMOVE_FROM_REGION => match chosen {
            1 => h.footprint(3, a),
            2 => {}
            _ => {
                s.a(EE_UNITS_UCK_D_BASE_REMOVE_FROM_REGION, a);
            }
        },
        EE_UNITS_BODY_REMOVE_FROM_TILE => match chosen {
            1 => h.footprint(1, a),
            2 => return s.a(EE_UNITS_UCK_D_SPACE_TRANSPORT_REMOVE, a),
            3 => return s.a(EE_UNITS_UCK_D_AMBIENT_REMOVE, a),
            _ => uc_rest::base_tile(h, class, a, false),
        },
        EE_UNITS_BODY_SIMPLE_UNIT_CREATED => {
            s.a(
                if chosen == 1 { EE_UNITS_UCK_D_RESOURCE_SIMPLE_CREATED } else { EE_UNITS_UCK_D_BASE_SIMPLE_CREATED },
                a,
            );
        }
        EE_UNITS_BODY_UNIT_CREATED => match chosen {
            UNIT_CREATED_HELICOPTER_PRIEST => uc_rest::helicopter_priest_created(h, class, unit),
            1..=14 => {
                s.s(EE_UNITS_UCK_D_BASE_SIMPLE_CREATED + chosen as i32);
            }
            _ => base_unit_created(h, class, unit),
        },
        EE_UNITS_BODY_UNLOAD_AT => {
            let op = match chosen {
                1 => EE_UNITS_UCK_D_FORTRESS_UNLOAD_AT,
                2 => EE_UNITS_UCK_D_TRANSPORT_UNLOAD_AT,
                _ => return 0,
            };
            s.abc(op, 0, b, c);
        }
        _ => {}
    }
    0
}

fn not_flare_or_volcano<H: UcHost + ?Sized>(s: &mut S<'_, H>) -> bool {
    s.s(EE_UNITS_UCK_D_IS_FLARE) == 0 && s.s(EE_UNITS_UC_THIS_UNIT_TYPE) != UNIT_TYPE_VOLCANO
}

/// `GetSoundParameters(unit, sound, outs)`: the class's body (the air animal's and the landable's are the C++
/// `AirGetSoundParameters`; the priests' and the spaceships' and the base body are uc_rest.rs's).
///
/// # Safety
/// `outs` are live floats
pub unsafe fn sound<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, sound: u64, outs: SoundOuts) {
    let chosen = body(&mut S { h: &mut *h, class, unit: 0 }, EE_UNITS_BODY_GET_SOUND_PARAMETERS);
    let what = match chosen {
        1 | 6 => {
            let mut pointers = [outs.volume, outs.pan, outs.pitch];
            S { h, class, unit: 0 }.abc(EE_UNITS_UCK_D_AIR_SOUND, unit, sound as i64, pointers.as_mut_ptr() as i64);
            return;
        }
        2 => uc_rest::EE_UNITS_UC_SOUND_HELICOPTER_PRIEST,
        3 | 5 => uc_rest::EE_UNITS_UC_SOUND_SPACESHIP,
        4 => uc_rest::EE_UNITS_UC_SOUND_LAND_PRIEST,
        _ => uc_rest::EE_UNITS_UC_SOUND_BASE,
    };
    // SAFETY: the caller's contract
    unsafe { uc_rest::sound(h, what, class, unit, sound, outs) };
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    /// The kinds' host (`ee_units_uck_host`, through uck_kinds.rs's; the class host's statements pass through it)
    struct GameD;

    impl UcHost for GameD {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            crate::units::uck_kinds::GameK.uc(op, class, unit, a, b, c, f)
        }
    }

    impl UckRust for GameD {
        fn footprint(&mut self, which: i32, unit: u64) {
            crate::units::uu_ffi::ee_units_uu_footprint(which, unit);
        }
    }

    /// The kind dispatcher `row` (`EE_UNITS_BODY_*`) of `class` on `unit` (see [`dispatch`]).
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit` (or NULL), `a`, `b`, `c` the dispatcher's
    /// objects and values, `f` its point (or NULL), on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uck_dispatch(
        row: usize,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        f: *mut f32,
    ) -> i64 {
        // SAFETY: the caller's contract
        unsafe { dispatch(&mut GameD, row, class as usize as u64, unit as usize as u64, a, b, c, f) }
    }

    /// `GetSoundParameters(unit, sound, outs)` of `class`.
    ///
    /// # Safety
    /// `class`, `unit` live; `sound` a live `EESoundObject` or NULL; the outs live floats.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uck_sound(
        class: *mut c_void,
        unit: *mut c_void,
        sound: *mut c_void,
        volume: *mut f32,
        pan: *mut f32,
        pitch: *mut f32,
    ) {
        let outs = SoundOuts { volume, pan, pitch };
        // SAFETY: the caller's contract
        unsafe { super::sound(&mut GameD, class as usize as u64, unit as usize as u64, sound as usize as u64, outs) }
    }
}

#[cfg(test)]
#[path = "uck_dispatch_tests.rs"]
mod tests;
