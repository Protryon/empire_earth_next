//! More of `EEComplexUnit`'s bodies over its C++ members (the units' lane, round 6; were EEComplexUnit.cpp):
//! `TakeOffMap`, `UpdateWaterState`, `LoadMedia`, `SetDisabled`, `UpdateCamouflage`, `SetGatheredResource`,
//! `PlayEffect`, `ReleaseFireEffect`, `Halt`, `FleeAfterCalamity`, `LinkPart`, `ClearGoalQueue`. As cu_body.rs:
//! each C++ statement one `EE_UNITS_CU_*` case of `ee_units_cu_host`, asked in the original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::cu_body::*;
use crate::units::cu_cargo::{
    CuCargoHost, EE_UNITS_CU_REFERENCE, EE_UNITS_CU_UNIT_ACTION_CHANGED, EE_UNITS_CU_UNIT_START_ACTION,
};
use std::os::raw::c_long;

// ---- the statements (from 300)
/// `GetPosition(mLastMapPosition)`
pub const EE_UNITS_CU_KEEP_MAP_POSITION: i32 = 300;
/// `mPlayer->DeselectUnit(complex unit a)`
pub const EE_UNITS_CU_DESELECT: i32 = 301;
/// player `b`'s `mViewedUnit == complex unit a`
pub const EE_UNITS_CU_PLAYER_VIEWS: i32 = 302;
/// player `b`'s `UpdateSelection(NULL, true)`
pub const EE_UNITS_CU_PLAYER_UPDATE_SELECTION: i32 = 303;
/// player `b`'s `mSelectedViewedUnit == complex unit a`
pub const EE_UNITS_CU_PLAYER_SELECTED_VIEWS: i32 = 304;
/// player `b`'s `SetSelectedViewedUnit(NULL)`
pub const EE_UNITS_CU_PLAYER_CLEAR_SELECTED_VIEWED: i32 = 305;
/// complex unit `a`'s `GetPosition(a->mLastMapPosition)`
pub const EE_UNITS_CU_PART_KEEP_MAP_POSITION: i32 = 306;
/// complex unit `a`'s `SetLocation((-10000, -10000, its location's Z), true)`
pub const EE_UNITS_CU_PART_SET_AWAY: i32 = 307;
/// `mGoalState != NULL`
pub const EE_UNITS_CU_HAS_GOAL_STATE: i32 = 308;
/// `mGoalState->mKind`
pub const EE_UNITS_CU_GOAL_KIND: i32 = 309;
/// `EECastCalamityGoalCast(GetGoal())->GetCalamityType()`
pub const EE_UNITS_CU_CAST_CALAMITY_TYPE: i32 = 310;
/// `SetVelocity(0.0f)`
pub const EE_UNITS_CU_STOP: i32 = 311;
/// `GetUnitClass()->mDomains`
pub const EE_UNITS_CU_CLASS_DOMAINS: i32 = 312;
/// `mTile` (0: none)
pub const EE_UNITS_CU_TILE: i32 = 313;
/// tile `a`'s `mTerrainType`
pub const EE_UNITS_CU_TILE_TERRAIN: i32 = 314;
/// tile `a`'s `mAIValue` as a signed char
pub const EE_UNITS_CU_TILE_LEVEL: i32 = 315;
/// `GetUnitClass()->mWaterKind`
pub const EE_UNITS_CU_WATER_KIND: i32 = 316;
/// `mWaterState = b`
pub const EE_UNITS_CU_SET_WATER_STATE: i32 = 317;
/// `EEUnit::LoadMedia(b != 0)`
pub const EE_UNITS_CU_UNIT_LOAD_MEDIA: i32 = 318;
/// `GetPlayer()->LoadUnitClassButtons(GetUnitClass()->GetUnitType())`
pub const EE_UNITS_CU_LOAD_BUTTONS: i32 = 319;
/// `mIsFoundation = b != 0`
pub const EE_UNITS_CU_SET_FOUNDATION: i32 = 320;
/// `mPhysicalModel.SetModel(EEGetPlayerClassGraphic(GetPlayer(), GetUnitClass())->mModel)`
pub const EE_UNITS_CU_PM_SET_FOUNDATION_MODEL: i32 = 321;
/// `mPhysicalModel.SetModel(model a's mModels[mAnimation])`
pub const EE_UNITS_CU_PM_SET_ANIMATION_MODEL: i32 = 322;
/// `mTurretAngles = NULL`
pub const EE_UNITS_CU_CLEAR_TURRET_ANGLES: i32 = 323;
/// `ResizeTurretAngles(model a's GetNumParts())`
pub const EE_UNITS_CU_RESIZE_TURRET_ANGLES: i32 = 324;
/// model `a`'s `GetOrbitalCount()`
pub const EE_UNITS_CU_MODEL_ORBITAL_COUNT: i32 = 325;
/// `mNumOrbitals = b`
pub const EE_UNITS_CU_SET_ORBITAL_COUNT: i32 = 326;
/// `mOrbitals = NULL`
pub const EE_UNITS_CU_CLEAR_ORBITALS: i32 = 327;
/// `mOrbitals = new EEOrbital*[mNumOrbitals]`
pub const EE_UNITS_CU_NEW_ORBITALS: i32 = 328;
/// `mOrbitals[b] = new EEOrbital(*model a's mOrbitals[b])`
pub const EE_UNITS_CU_NEW_ORBITAL: i32 = 329;
/// model `a`'s `GetWheelCount()`
pub const EE_UNITS_CU_MODEL_WHEEL_COUNT: i32 = 330;
/// `mNumWheels = b`
pub const EE_UNITS_CU_SET_WHEEL_COUNT: i32 = 331;
/// `mWheels = NULL`
pub const EE_UNITS_CU_CLEAR_WHEELS: i32 = 332;
/// `mWheels = new EEWheel*[mNumWheels]`
pub const EE_UNITS_CU_NEW_WHEELS: i32 = 333;
/// `mWheels[b] = new EEWheel(*model a's mWheels[b])`
pub const EE_UNITS_CU_NEW_WHEEL: i32 = 334;
/// `mWheels[b]->mAngle = 0.0f`
pub const EE_UNITS_CU_WHEEL_ANGLE_ZERO: i32 = 335;
/// `mIsDisabled`
pub const EE_UNITS_CU_DISABLED: i32 = 336;
/// `mIsDisabled = b != 0`
pub const EE_UNITS_CU_SET_DISABLED_FIELD: i32 = 337;
/// `mDisabledReason = b`
pub const EE_UNITS_CU_SET_DISABLED_REASON: i32 = 338;
/// `ee_units_atc_add_airport(mPlayer->mAirTraffic[b - 1], this)`
pub const EE_UNITS_CU_ATC_ADD_AIRPORT: i32 = 339;
/// `SetIsTinted(b != 0)`
pub const EE_UNITS_CU_SET_TINTED: i32 = 340;
/// `SetColor(kTimeWarpColor)`
pub const EE_UNITS_CU_TIME_WARP_COLOR: i32 = 341;
/// `IsCloaked()`
pub const EE_UNITS_CU_IS_CLOAKED: i32 = 342;
/// `gCivPowersManager.HasPower(GetPlayer(), kCivPowerCamouflage)`
pub const EE_UNITS_CU_HAS_CAMOUFLAGE: i32 = 343;
/// `gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)`
pub const EE_UNITS_CU_CAMOUFLAGE_SET: i32 = 344;
/// `mPlayer->mNumVitalBuildings`
pub const EE_UNITS_CU_VITAL_BUILDINGS: i32 = 345;
/// `mIsCloakable`
pub const EE_UNITS_CU_CLOAKABLE: i32 = 346;
/// `mIsCloaked`
pub const EE_UNITS_CU_CLOAKED_FIELD: i32 = 347;
/// `ee_units_uu_cloaked_enemy(this, gWorld.GetCurrentPlayer())`
pub const EE_UNITS_CU_CLOAKED_ENEMY: i32 = 348;
/// `gMiniMap->MoveUnit(this, -10000, -10000, GetTileX(), GetTileY())`
pub const EE_UNITS_CU_MINIMAP_REMOVE: i32 = 349;
/// `mIsCloakable = false; mIsCloaked = false` (the two stores in order)
pub const EE_UNITS_CU_CLEAR_CLOAKABLE: i32 = 350;
pub const EE_UNITS_CU_CLEAR_CLOAKED: i32 = 351;
/// `mCamouflageTimer = b`
pub const EE_UNITS_CU_SET_CAMOUFLAGE_TIMER: i32 = 352;
/// `mU32C = b`
pub const EE_UNITS_CU_SET_U32C: i32 = 353;
/// `gWorldTable.GetLongValue2(kWorldCamouflageTiming)`
pub const EE_UNITS_CU_CAMOUFLAGE_TIMING2: i32 = 354;
/// `gWorldTable.GetLongValue(kWorldCamouflageTiming)`
pub const EE_UNITS_CU_CAMOUFLAGE_TIMING: i32 = 355;
/// `mGatheredResource`
pub const EE_UNITS_CU_GATHERED: i32 = 356;
/// `gWorld.ReleaseResourceUser(mGatheredResource)`
pub const EE_UNITS_CU_RELEASE_RESOURCE_USER: i32 = 357;
/// `mGatheredResource->mIsValid`
pub const EE_UNITS_CU_GATHERED_VALID: i32 = 358;
/// `EEGetWaitingGatherer(mGatheredResource)`
pub const EE_UNITS_CU_WAITING_GATHERER: i32 = 359;
/// `mGatheredResource = unit a` (0: NULL)
pub const EE_UNITS_CU_SET_GATHERED_FIELD: i32 = 360;
/// `gWorld.AddResourceUser(mGatheredResource)`
pub const EE_UNITS_CU_ADD_RESOURCE_USER: i32 = 361;
/// `gDataPreLoading->PostUnloadTechnology(mPlayer->mPlayerID, mCurrentResearch)`
pub const EE_UNITS_CU_POST_UNLOAD_TECHNOLOGY: i32 = 362;
/// `GetPlayer()->GetTechTree()->Get(mCurrentResearch)->UnloadMedia(GetPlayer())`
pub const EE_UNITS_CU_UNLOAD_TECHNOLOGY_MEDIA: i32 = 363;
/// `mCurrentResearch = b`
pub const EE_UNITS_CU_SET_CURRENT_RESEARCH: i32 = 364;
/// `gDataPreLoading->PostLoadTechnology(mPlayer->mPlayerID, b)`
pub const EE_UNITS_CU_POST_LOAD_TECHNOLOGY: i32 = 365;
/// `GetUnitClass()->mIsWall`
pub const EE_UNITS_CU_IS_WALL: i32 = 366;
/// `mFireEffect.mValue`
pub const EE_UNITS_CU_FIRE_EFFECT: i32 = 367;
/// `gEffectManager->DestroyEffect(mFireEffect)`; `mFireEffect.mValue = 0` (in order)
pub const EE_UNITS_CU_DESTROY_FIRE_EFFECT: i32 = 368;
pub const EE_UNITS_CU_CLEAR_FIRE_EFFECT: i32 = 369;
/// `GetUnitClass()->mUnitType`
pub const EE_UNITS_CU_CLASS_TYPE_FIELD: i32 = 370;
/// `EERemoveCloakEffect(this)`
pub const EE_UNITS_CU_REMOVE_CLOAK_EFFECT: i32 = 371;
/// `ClearBuildTargets()`
pub const EE_UNITS_CU_CLEAR_BUILD_TARGETS: i32 = 372;
/// `ee_units_atc_clear_airport_queue(mPlayer->mAirTraffic[GetUnitClass()->mAirTrafficType - 1], this, b)`
pub const EE_UNITS_CU_ATC_CLEAR_AIRPORT_QUEUE: i32 = 373;
/// `mPlayer->mComputerPlayer != NULL`
pub const EE_UNITS_CU_HAS_COMPUTER_PLAYER: i32 = 374;
/// `EEGetRandomPointAhead(this, &point)` (the point into `f[0..2]`)
pub const EE_UNITS_CU_RANDOM_POINT_AHEAD: i32 = 375;
/// `SetGoal(EEUGRust::NewLocation(GE2DPoint(f[0], f[1]), true, true, false, 0.0f, false, false), 0, 0)`
pub const EE_UNITS_CU_FLEE_GOAL: i32 = 376;
/// `mPreviousPart->mNextPart`
pub const EE_UNITS_CU_PREVIOUS_NEXT: i32 = 377;
/// `mPreviousPart->mNextPart = NULL`
pub const EE_UNITS_CU_CLEAR_PREVIOUS_NEXT: i32 = 378;
/// `mPreviousPart = complex unit a`
pub const EE_UNITS_CU_SET_PREVIOUS_PART: i32 = 379;
/// `mPreviousPart->mNextPart = this`
pub const EE_UNITS_CU_SET_PREVIOUS_NEXT_THIS: i32 = 380;
/// `mGoalQueues[0].mGoals.size()`
pub const EE_UNITS_CU_FIRST_QUEUE_LEN: i32 = 381;
/// `mGoalQueues[0].GetGoalAt(b)->Abandon(this)`
pub const EE_UNITS_CU_ABANDON_GOAL: i32 = 382;
/// `mGoalQueues[b].Clear()`
pub const EE_UNITS_CU_GOAL_QUEUE_CLEAR: i32 = 383;
/// `GetUnitClass() != NULL`
pub const EE_UNITS_CU_HAS_CLASS: i32 = 384;
/// `RefreshGoals()`; `RefreshActiveGoal()`
pub const EE_UNITS_CU_REFRESH_GOALS: i32 = 385;
pub const EE_UNITS_CU_REFRESH_ACTIVE_GOAL: i32 = 386;
/// `GetUnitClass()->mCategory` is the airplane's or the planet spaceship's
pub const EE_UNITS_CU_CIRCLES: i32 = 387;

// ---- the bodies (from 40)
pub const EE_UNITS_CU_DO_TAKE_OFF_MAP: i32 = 40;
pub const EE_UNITS_CU_DO_UPDATE_WATER_STATE: i32 = 41;
pub const EE_UNITS_CU_DO_LOAD_MEDIA: i32 = 42;
pub const EE_UNITS_CU_DO_SET_DISABLED: i32 = 43;
pub const EE_UNITS_CU_DO_UPDATE_CAMOUFLAGE: i32 = 44;
pub const EE_UNITS_CU_DO_SET_GATHERED_RESOURCE: i32 = 45;
pub const EE_UNITS_CU_DO_PLAY_EFFECT: i32 = 46;
pub const EE_UNITS_CU_DO_RELEASE_FIRE_EFFECT: i32 = 47;
pub const EE_UNITS_CU_DO_HALT: i32 = 48;
pub const EE_UNITS_CU_DO_FLEE_AFTER_CALAMITY: i32 = 49;
pub const EE_UNITS_CU_DO_LINK_PART: i32 = 50;
pub const EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE: i32 = 51;

const GOAL_CAST_CALAMITY: i64 = 11;
const ACTION_IDLE: i64 = 5000;
const ACTION_CIRCLE: i64 = 1008;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
const ACTION_WAIT_FOR_OPEN_POSITION: i64 = 4026;
const ACTION_MOVE_TO_RESOURCE: i64 = 8008;
const TERRAIN_DEEP_WATER: i64 = 1;
const TERRAIN_SHALLOW_WATER: i64 = 10;
const TERRAIN_SPACE: i64 = 12;
/// `kMovementSeaOrSpace`
const MOVEMENT_SEA_OR_SPACE: i64 = 4 | 8;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_GATE: i64 = 0x19;
const UNIT_TYPE_MARKET_CP: i64 = 0x321;

fn s<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64) -> i64 {
    h.cu(op, unit, 0, 0, 0, &mut [0.0; 4])
}

fn sa<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64) -> i64 {
    h.cu(op, unit, a, 0, 0, &mut [0.0; 4])
}

fn sb<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, b: i64) -> i64 {
    h.cu(op, unit, 0, b, 0, &mut [0.0; 4])
}

fn sab<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64, b: i64) -> i64 {
    h.cu(op, unit, a, b, 0, &mut [0.0; 4])
}

/// `TakeOffMap()`: the unit and its parts deselected (by every player's view), moved away and deactivated; idle
/// unless casting the 0x77c calamity; stopped, its effects and tint released.
pub fn take_off_map<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    s(h, EE_UNITS_CU_KEEP_MAP_POSITION, unit);
    s(h, EE_UNITS_CU_DROP_PATH, unit);
    s(h, EE_UNITS_CU_CLEAR_COLLISION, unit);
    let mut part = unit;
    while part != 0 {
        sa(h, EE_UNITS_CU_DESELECT, unit, part);
        let mut i: i64 = 1;
        while i < s(h, EE_UNITS_CU_NUM_PLAYERS, unit) {
            let player = sb(h, EE_UNITS_CU_PLAYER_AT, unit, i);
            if sab(h, EE_UNITS_CU_PLAYER_VIEWS, unit, part, player) != 0 {
                sab(h, EE_UNITS_CU_PLAYER_UPDATE_SELECTION, unit, part, player);
            }
            if sab(h, EE_UNITS_CU_PLAYER_SELECTED_VIEWS, unit, part, player) != 0 {
                sab(h, EE_UNITS_CU_PLAYER_CLEAR_SELECTED_VIEWED, unit, part, player);
            }
            i += 1;
        }
        sa(h, EE_UNITS_CU_PART_KEEP_MAP_POSITION, unit, part);
        sa(h, EE_UNITS_CU_PART_SET_AWAY, unit, part);
        sa(h, crate::units::cu_cargo::EE_UNITS_CU_DEACTIVATE_UNIT, unit, part);
        part = sa(h, EE_UNITS_CU_PART_NEXT, unit, part) as u64;
    }
    if s(h, EE_UNITS_CU_HAS_GOAL_STATE, unit) != 0
        && (s(h, EE_UNITS_CU_GOAL_KIND, unit) != GOAL_CAST_CALAMITY
            || s(h, EE_UNITS_CU_CAST_CALAMITY_TYPE, unit) != 0x77c)
    {
        sb(h, EE_UNITS_CU_START_ACTION, unit, ACTION_IDLE);
        s(h, EE_UNITS_CU_ACTION_CHANGED, unit);
    }
    for op in [
        EE_UNITS_CU_STOP,
        EE_UNITS_CU_RELEASE_CONVERSION_EFFECT,
        EE_UNITS_CU_RELEASE_HEAL_EFFECT,
        EE_UNITS_CU_CLEAR_TINT,
        EE_UNITS_CU_RELEASE_FIRE_EFFECT,
    ] {
        s(h, op, unit);
    }
}

/// `UpdateWaterState(on_tile)`: a sea or space unit's water state from its tile's water level (`on_tile`) or its
/// class's water kind; 0 for the others.
pub fn update_water_state<H: CuHost + ?Sized>(h: &mut H, unit: u64, on_tile: bool) {
    let set = |h: &mut H, state: i64| {
        sb(h, EE_UNITS_CU_SET_WATER_STATE, unit, state);
    };
    if on_tile {
        if s(h, EE_UNITS_CU_CLASS_DOMAINS, unit) & MOVEMENT_SEA_OR_SPACE == 0 {
            return;
        }
        let tile = s(h, EE_UNITS_CU_TILE, unit) as u64;
        if tile == 0 {
            return;
        }
        let terrain = sa(h, EE_UNITS_CU_TILE_TERRAIN, unit, tile);
        if terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_SHALLOW_WATER && terrain != TERRAIN_SPACE {
            return;
        }
        let level = sa(h, EE_UNITS_CU_TILE_LEVEL, unit, tile);
        let mode = s(h, EE_UNITS_CU_WATER_KIND, unit);
        if level >= 0 && mode != 0 {
            if (level > 0 && mode == 2) || mode == 1 {
                return set(h, 2);
            }
            if mode == 2 {
                return set(h, 3);
            }
        }
        set(h, 0);
    } else if s(h, EE_UNITS_CU_CLASS_DOMAINS, unit) & MOVEMENT_SEA_OR_SPACE != 0 {
        if s(h, EE_UNITS_CU_WATER_KIND, unit) == 1 {
            set(h, 2);
        } else if s(h, EE_UNITS_CU_WATER_KIND, unit) == 2 {
            set(h, 3);
        } else {
            set(h, 0);
        }
    } else {
        set(h, 0);
    }
}

/// `LoadMedia(foundation)`: the unit's media, its class's buttons (not a land building's), the foundation's model
/// or the animation's with fresh turret angles, orbitals and wheels copied from the model.
pub fn load_media<H: CuHost + ?Sized>(h: &mut H, unit: u64, foundation: bool) {
    sb(h, EE_UNITS_CU_UNIT_LOAD_MEDIA, unit, i64::from(foundation));
    if s(h, EE_UNITS_CU_IS_LAND_BUILDING, unit) == 0 {
        s(h, EE_UNITS_CU_LOAD_BUTTONS, unit);
    }
    sb(h, EE_UNITS_CU_SET_FOUNDATION, unit, i64::from(foundation));
    if foundation {
        s(h, EE_UNITS_CU_PM_SET_FOUNDATION_MODEL, unit);
        return;
    }
    let model = s(h, EE_UNITS_CU_MODEL, unit) as u64;
    sa(h, EE_UNITS_CU_PM_SET_ANIMATION_MODEL, unit, model);
    s(h, EE_UNITS_CU_CLEAR_TURRET_ANGLES, unit);
    sa(h, EE_UNITS_CU_RESIZE_TURRET_ANGLES, unit, model);
    let orbitals = sa(h, EE_UNITS_CU_MODEL_ORBITAL_COUNT, unit, model) as u32;
    sb(h, EE_UNITS_CU_SET_ORBITAL_COUNT, unit, i64::from(orbitals));
    if orbitals == 0 {
        s(h, EE_UNITS_CU_CLEAR_ORBITALS, unit);
    } else {
        s(h, EE_UNITS_CU_NEW_ORBITALS, unit);
        for i in 0..orbitals {
            sab(h, EE_UNITS_CU_NEW_ORBITAL, unit, model, i64::from(i));
        }
    }
    let wheels = sa(h, EE_UNITS_CU_MODEL_WHEEL_COUNT, unit, model) as u32;
    sb(h, EE_UNITS_CU_SET_WHEEL_COUNT, unit, i64::from(wheels));
    if wheels == 0 {
        s(h, EE_UNITS_CU_CLEAR_WHEELS, unit);
    } else {
        s(h, EE_UNITS_CU_NEW_WHEELS, unit);
        for i in 0..wheels {
            sab(h, EE_UNITS_CU_NEW_WHEEL, unit, model, i64::from(i));
            sb(h, EE_UNITS_CU_WHEEL_ANGLE_ZERO, unit, i64::from(i));
        }
    }
}

/// `SetDisabled(away, reason)`: the flag and reason (an air base leaves or rejoins the air traffic), the time warp's
/// tint, or the tint cleared when not cloaked.
pub fn set_disabled<H: CuHost + ?Sized>(h: &mut H, unit: u64, away: bool, reason: i64) {
    if (s(h, EE_UNITS_CU_DISABLED, unit) != 0) == away {
        return;
    }
    let airport = s(h, EE_UNITS_CU_AIR_TRAFFIC_TYPE, unit);
    sb(h, EE_UNITS_CU_SET_DISABLED_FIELD, unit, i64::from(away));
    sb(h, EE_UNITS_CU_SET_DISABLED_REASON, unit, reason);
    if airport != 0 && s(h, EE_UNITS_CU_IS_AIR_BASE, unit) != 0 {
        if s(h, EE_UNITS_CU_DISABLED, unit) != 0 {
            sb(h, EE_UNITS_CU_ATC_REMOVE_AIRPORT, unit, airport);
        } else {
            sb(h, EE_UNITS_CU_ATC_ADD_AIRPORT, unit, airport);
        }
    }
    let reason = reason as c_long;
    if away {
        if reason == 0x1a || reason == 0x1c {
            sb(h, EE_UNITS_CU_SET_TINTED, unit, 1);
            s(h, EE_UNITS_CU_TIME_WARP_COLOR, unit);
        }
    } else if s(h, EE_UNITS_CU_IS_CLOAKED, unit) == 0 {
        sb(h, EE_UNITS_CU_SET_TINTED, unit, 0);
    }
}

/// `UpdateCamouflage(action)`: the camouflage power's units (without vital buildings, no camouflage: uncloaked,
/// off an enemy's minimap) start or stop their camouflage clock as they go idle or move.
pub fn update_camouflage<H: CuHost + ?Sized>(h: &mut H, unit: u64, action: i64) {
    let action = action as c_long;
    let camouflaged = s(h, EE_UNITS_CU_HAS_CAMOUFLAGE, unit) != 0 && s(h, EE_UNITS_CU_CAMOUFLAGE_SET, unit) != 0 && {
        let set = s(h, EE_UNITS_CU_CAMOUFLAGE_SET, unit) as u64;
        sa(h, EE_UNITS_CU_SET_HAS_TYPE, unit, set) != 0
    };
    if !camouflaged {
        sb(h, EE_UNITS_CU_SET_CAMOUFLAGE_TIMER, unit, 0);
        sb(h, EE_UNITS_CU_SET_U32C, unit, 0);
        return;
    }
    if (s(h, EE_UNITS_CU_VITAL_BUILDINGS, unit) as c_long) < 1 {
        let cloaked = s(h, EE_UNITS_CU_CLOAKABLE, unit) != 0 && s(h, EE_UNITS_CU_CLOAKED_FIELD, unit) != 0;
        if cloaked && s(h, EE_UNITS_CU_CLOAKED_ENEMY, unit) != 0 {
            s(h, EE_UNITS_CU_MINIMAP_REMOVE, unit);
        }
        s(h, EE_UNITS_CU_CLEAR_CLOAKABLE, unit);
        s(h, EE_UNITS_CU_CLEAR_CLOAKED, unit);
        sb(h, EE_UNITS_CU_SET_CAMOUFLAGE_TIMER, unit, 0);
        sb(h, EE_UNITS_CU_SET_U32C, unit, 0);
    } else if s(h, EE_UNITS_CU_COMMAND_TYPE, unit) == ACTION_IDLE && action != 5000 {
        sb(h, EE_UNITS_CU_SET_CAMOUFLAGE_TIMER, unit, 0);
        let timing = s(h, EE_UNITS_CU_CAMOUFLAGE_TIMING2, unit);
        sb(h, EE_UNITS_CU_SET_U32C, unit, timing);
    } else if s(h, EE_UNITS_CU_COMMAND_TYPE, unit) != ACTION_IDLE && action == 5000 {
        let timing = s(h, EE_UNITS_CU_CAMOUFLAGE_TIMING, unit);
        sb(h, EE_UNITS_CU_SET_CAMOUFLAGE_TIMER, unit, timing);
        sb(h, EE_UNITS_CU_SET_U32C, unit, 0);
    }
}

/// `SetGatheredResource(resource)`: the old resource released (a gatherer waiting for it sent there, unless this
/// one waits), the new one referenced and used.
pub fn set_gathered_resource<H: CuHost + ?Sized>(h: &mut H, unit: u64, resource: u64) {
    let old = s(h, EE_UNITS_CU_GATHERED, unit) as u64;
    if old != 0 {
        s(h, EE_UNITS_CU_RELEASE_RESOURCE_USER, unit);
        if s(h, EE_UNITS_CU_GATHERED_VALID, unit) != 0
            && s(h, EE_UNITS_CU_HAS_COMMAND, unit) != 0
            && s(h, EE_UNITS_CU_COMMAND_TYPE, unit) != ACTION_WAIT_FOR_OPEN_POSITION
        {
            let waiting = s(h, EE_UNITS_CU_WAITING_GATHERER, unit) as u64;
            if waiting != 0 {
                sab(h, EE_UNITS_CU_UNIT_START_ACTION, unit, waiting, ACTION_MOVE_TO_RESOURCE);
                sa(h, EE_UNITS_CU_UNIT_ACTION_CHANGED, unit, waiting);
            }
        }
        let old = s(h, EE_UNITS_CU_GATHERED, unit) as u64;
        sa(h, EE_UNITS_CU_DEREFERENCE, unit, old);
        sa(h, EE_UNITS_CU_SET_GATHERED_FIELD, unit, 0);
    }
    if resource == 0 {
        return;
    }
    sa(h, EE_UNITS_CU_SET_GATHERED_FIELD, unit, resource);
    sa(h, EE_UNITS_CU_REFERENCE, unit, resource);
    s(h, EE_UNITS_CU_ADD_RESOURCE_USER, unit);
}

/// `PlayEffect(technology)`: the current research's media released (a hero's slot given back), the new research
/// set and its media loaded.
pub fn play_effect<H: CuHost + ?Sized>(h: &mut H, unit: u64, technology: i64) {
    if s(h, EE_UNITS_CU_CURRENT_RESEARCH, unit) != 100000 {
        let node = s(h, crate::units::cu_cargo::EE_UNITS_CU_CURRENT_NODE, unit) as u64;
        if node != 0 {
            crate::units::cu_cargo::release_queued_hero(h, unit, node);
        }
        if s(h, EE_UNITS_CU_WORLD_CLEARING, unit) == 0 {
            s(h, EE_UNITS_CU_POST_UNLOAD_TECHNOLOGY, unit);
        } else {
            s(h, EE_UNITS_CU_UNLOAD_TECHNOLOGY_MEDIA, unit);
        }
    }
    sb(h, EE_UNITS_CU_SET_CURRENT_RESEARCH, unit, technology);
    if technology as c_long != 100000 {
        sb(h, EE_UNITS_CU_POST_LOAD_TECHNOLOGY, unit, technology);
    }
}

/// `ReleaseFireEffect()`: a citizen's, gate's or wall's fire effect destroyed; the CP market's cloak removed.
pub fn release_fire_effect<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let category = s(h, EE_UNITS_CU_CATEGORY, unit);
    if category == CATEGORY_LAND_CITIZEN || category == CATEGORY_GATE || s(h, EE_UNITS_CU_IS_WALL, unit) != 0 {
        if s(h, EE_UNITS_CU_FIRE_EFFECT, unit) as u32 > 0 {
            s(h, EE_UNITS_CU_DESTROY_FIRE_EFFECT, unit);
            s(h, EE_UNITS_CU_CLEAR_FIRE_EFFECT, unit);
        }
    } else if s(h, EE_UNITS_CU_CLASS_TYPE_FIELD, unit) == UNIT_TYPE_MARKET_CP {
        s(h, EE_UNITS_CU_REMOVE_CLOAK_EFFECT, unit);
    }
}

/// `Halt()`: the build targets, the first goal queue and the goals cleared (an air base's queue too); circling or
/// idle.
pub fn halt<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    crate::units::cu_world::clear_build_targets(h, unit);
    sb(h, EE_UNITS_CU_CLEAR_GOAL_QUEUE, unit, 0);
    s(h, crate::units::cu_cargo::EE_UNITS_CU_CLEAR_GOALS, unit);
    if s(h, EE_UNITS_CU_IS_AIR_BASE, unit) != 0 {
        sb(h, EE_UNITS_CU_ATC_CLEAR_AIRPORT_QUEUE, unit, 0);
    }
    let action = if s(h, EE_UNITS_CU_CIRCLES, unit) != 0 { ACTION_CIRCLE } else { ACTION_IDLE };
    sb(h, EE_UNITS_CU_START_ACTION, unit, action);
    s(h, EE_UNITS_CU_ACTION_CHANGED, unit);
}

/// `FleeAfterCalamity(stay)`: a computer's unit goes idle; a human's (unless `stay`, a byte) runs to a random point
/// ahead.
pub fn flee_after_calamity<H: CuHost + ?Sized>(h: &mut H, unit: u64, stay: i64) {
    if s(h, EE_UNITS_CU_HAS_COMPUTER_PLAYER, unit) != 0 {
        sb(h, EE_UNITS_CU_START_ACTION, unit, ACTION_IDLE);
    } else {
        if stay as u8 != 0 {
            return;
        }
        let mut point = [0.0f32; 4];
        if h.cu(EE_UNITS_CU_RANDOM_POINT_AHEAD, unit, 0, 0, 0, &mut point) == 0 {
            return;
        }
        h.cu(EE_UNITS_CU_FLEE_GOAL, unit, 0, 0, 0, &mut point);
        sb(h, EE_UNITS_CU_START_ACTION, unit, ACTION_PREPARE_TO_MOVE);
    }
    s(h, EE_UNITS_CU_ACTION_CHANGED, unit);
}

/// `LinkPart(previous)`: the unit unlinked from its previous part and linked after `previous` (both references
/// counted).
pub fn link_part<H: CuHost + ?Sized>(h: &mut H, unit: u64, previous: u64) {
    let old = s(h, EE_UNITS_CU_PREVIOUS_PART, unit) as u64;
    if old != 0 {
        let next = s(h, EE_UNITS_CU_PREVIOUS_NEXT, unit) as u64;
        if next != 0 {
            sa(h, EE_UNITS_CU_DEREFERENCE, unit, next);
        }
        s(h, EE_UNITS_CU_CLEAR_PREVIOUS_NEXT, unit);
        sa(h, EE_UNITS_CU_DEREFERENCE, unit, old);
    }
    sa(h, EE_UNITS_CU_SET_PREVIOUS_PART, unit, previous);
    if previous == 0 {
        return;
    }
    sa(h, EE_UNITS_CU_REFERENCE, unit, previous);
    s(h, EE_UNITS_CU_SET_PREVIOUS_NEXT_THIS, unit);
    sa(h, EE_UNITS_CU_REFERENCE, unit, unit);
}

/// `ClearGoalQueue(index)`: the first queue's goals abandoned, the queue cleared, the gathered resource released,
/// an air base's queue cleared, the goals refreshed.
pub fn clear_goal_queue<H: CuHost + ?Sized>(h: &mut H, unit: u64, index: i64) {
    let index = index as u32;
    if index == 0 {
        let mut i: i64 = 0;
        while i < s(h, EE_UNITS_CU_FIRST_QUEUE_LEN, unit) {
            sb(h, EE_UNITS_CU_ABANDON_GOAL, unit, i);
            i += 1;
        }
    }
    sb(h, EE_UNITS_CU_GOAL_QUEUE_CLEAR, unit, i64::from(index));
    s(h, EE_UNITS_CU_CLEAR_GATHERED, unit);
    if s(h, EE_UNITS_CU_HAS_CLASS, unit) != 0 {
        if s(h, EE_UNITS_CU_IS_AIR_BASE, unit) != 0 && s(h, EE_UNITS_CU_AIR_TRAFFIC_TYPE, unit) != 0 {
            sb(h, EE_UNITS_CU_ATC_CLEAR_AIRPORT_QUEUE, unit, i64::from(index));
        }
        s(h, EE_UNITS_CU_REFRESH_GOALS, unit);
        s(h, EE_UNITS_CU_REFRESH_ACTIVE_GOAL, unit);
    }
}

/// Runs body `what` (`EE_UNITS_CU_DO_*` from 40) on `unit`; `None` for another body.
pub fn run<H: CuCargoHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: i64, b: i64) -> Option<i64> {
    match what {
        EE_UNITS_CU_DO_TAKE_OFF_MAP => take_off_map(h, unit),
        EE_UNITS_CU_DO_UPDATE_WATER_STATE => update_water_state(h, unit, a != 0),
        EE_UNITS_CU_DO_LOAD_MEDIA => load_media(h, unit, a != 0),
        EE_UNITS_CU_DO_SET_DISABLED => set_disabled(h, unit, a != 0, b),
        EE_UNITS_CU_DO_UPDATE_CAMOUFLAGE => update_camouflage(h, unit, a),
        EE_UNITS_CU_DO_SET_GATHERED_RESOURCE => set_gathered_resource(h, unit, a as u64),
        EE_UNITS_CU_DO_PLAY_EFFECT => play_effect(h, unit, a),
        EE_UNITS_CU_DO_RELEASE_FIRE_EFFECT => release_fire_effect(h, unit),
        EE_UNITS_CU_DO_HALT => halt(h, unit),
        EE_UNITS_CU_DO_FLEE_AFTER_CALAMITY => flee_after_calamity(h, unit, a),
        EE_UNITS_CU_DO_LINK_PART => link_part(h, unit, a as u64),
        EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE => clear_goal_queue(h, unit, a),
        _ => return None,
    }
    Some(0)
}

#[cfg(test)]
#[path = "cu_misc_tests.rs"]
mod tests;
