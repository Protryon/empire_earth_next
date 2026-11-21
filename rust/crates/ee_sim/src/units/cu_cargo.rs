//! `EEComplexUnit`'s production queue and cargo bodies over its C++ members (the units' lane, round 6; were
//! EEComplexUnit.cpp): `UpdateJITManufacturing`, `CancelQueuedProduction`, `ToggleJITManufacturing`,
//! `RemoveResearch`, `ReplaceResearch`, `QueueResearchUncaptured`, `LoadCargo`, `GarrisonUnit`, `AddGarrison`,
//! `KillCargo`, `ReleaseCargo`, `ReplaceCargo`, `UnloadCargoAt`, `UnloadCargoAtTile`, `UnloadCargoNow`,
//! `OrderRepair`, `AddBuildTarget`, `AddContact`. As cu_body.rs: each C++ statement one `EE_UNITS_CU_*` case of
//! `ee_units_cu_host`, asked in the original's order; the placement search is the unit utilities' Rust
//! ([`CuCargoHost::find_placement`], the same FFI function the C++ called, with its note).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::cu_body::CuHost;
use std::os::raw::c_long;

// ---- the statements (from 200; `a` an object, `b`, `c` values)
/// `GetPlayer()->GetTechTree()->Get(mCurrentResearch)`
pub const EE_UNITS_CU_CURRENT_NODE: i32 = 200;
/// node `a`'s `mData->mUnitClass`
pub const EE_UNITS_CU_NODE_CLASS_INDEX: i32 = 201;
/// node `a`'s `mIsAllowed = b != 0`
pub const EE_UNITS_CU_SET_NODE_ALLOWED: i32 = 202;
/// node `a`'s `mIsAllowed`
pub const EE_UNITS_CU_NODE_ALLOWED: i32 = 203;
/// class `a`'s `mAIKind`
pub const EE_UNITS_CU_CLASS_AI_KIND: i32 = 204;
/// `GetPlayer()->mNumHeroesInProduction`
pub const EE_UNITS_CU_HEROES_IN_PRODUCTION: i32 = 205;
/// `GetPlayer()->mNumHeroesInProduction = b`
pub const EE_UNITS_CU_SET_HEROES_IN_PRODUCTION: i32 = 206;
/// `mBuildQueue.size()`
pub const EE_UNITS_CU_QUEUE_SIZE: i32 = 207;
/// `mBuildQueue.back().mTechnology`
pub const EE_UNITS_CU_QUEUE_BACK_TECHNOLOGY: i32 = 208;
/// `mBuildQueue.pop_back()`
pub const EE_UNITS_CU_QUEUE_POP_BACK: i32 = 209;
/// `mPlayer->mTechnologies->Get(b)`
pub const EE_UNITS_CU_PLAYER_NODE: i32 = 210;
/// `mPlayer->RefundTechnology(node a, GetUnitClass()->mUnitType, 1.0f)`
pub const EE_UNITS_CU_REFUND_ONE: i32 = 211;
/// `SetProductionTime(b)`
pub const EE_UNITS_CU_SET_PRODUCTION_TIME_FIELD: i32 = 212;
/// `mBuildQueue.clear()`
pub const EE_UNITS_CU_QUEUE_CLEAR: i32 = 213;
/// `mBuildQueue.push_back({b, c != 0})`
pub const EE_UNITS_CU_QUEUE_PUSH_BACK: i32 = 214;
/// `mBuildQueue.insert(mBuildQueue.begin(), {b, c != 0})`
pub const EE_UNITS_CU_QUEUE_PUSH_FRONT: i32 = 215;
/// `mBuildQueue[b].mTechnology`
pub const EE_UNITS_CU_QUEUE_TECHNOLOGY_AT: i32 = 216;
/// `mBuildQueue[b].mTechnology = c`
pub const EE_UNITS_CU_QUEUE_SET_TECHNOLOGY_AT: i32 = 217;
/// `mBuildQueue.erase(mBuildQueue.begin() + b)`
pub const EE_UNITS_CU_QUEUE_ERASE_AT: i32 = 218;
/// `GetPlayer()->CanAfford(node a, GetUnitClass()->GetUnitType(), true)`
pub const EE_UNITS_CU_CAN_AFFORD: i32 = 219;
/// `gWorld.mGameOptions.mIsScenarioGame`
pub const EE_UNITS_CU_SCENARIO_GAME: i32 = 220;
/// `mPlayer->mHeroLimit`
pub const EE_UNITS_CU_HERO_LIMIT: i32 = 221;
/// `mPlayer->GetHeroCount(1, 1)`
pub const EE_UNITS_CU_HERO_COUNT: i32 = 222;
/// `mPlayer->mJITManufacturing`
pub const EE_UNITS_CU_JIT_MANUFACTURING: i32 = 223;
/// `mPlayer->Pay(node a, GetUnitClass()->GetUnitType(), 1.0f)`
pub const EE_UNITS_CU_PAY: i32 = 224;
/// node `a`'s `mData->mKind`
pub const EE_UNITS_CU_NODE_KIND: i32 = 225;
/// complex unit `a`'s `mUnitClass->mWeight`
pub const EE_UNITS_CU_UNIT_WEIGHT: i32 = 226;
/// `mCargoRoom`
pub const EE_UNITS_CU_CARGO_ROOM: i32 = 227;
/// `mCargoRoom = b`
pub const EE_UNITS_CU_SET_CARGO_ROOM: i32 = 228;
/// `mCargo.push_back(complex unit a)`
pub const EE_UNITS_CU_CARGO_PUSH: i32 = 229;
/// unit `a`'s `Reference()`
pub const EE_UNITS_CU_REFERENCE: i32 = 230;
/// complex unit `a`'s `mContainer`
pub const EE_UNITS_CU_UNIT_CONTAINER: i32 = 231;
/// complex unit `a`'s `mContainer = this`
pub const EE_UNITS_CU_SET_CONTAINER_THIS: i32 = 232;
/// complex unit `a`'s `TakeOffMap()`
pub const EE_UNITS_CU_TAKE_OFF_MAP: i32 = 233;
/// complex unit `a`'s `mPlayer->GetOffMapInfo(a)`
pub const EE_UNITS_CU_OFF_MAP_INFO: i32 = 234;
/// off-map info `a`'s `mReason = b`
pub const EE_UNITS_CU_SET_OFF_MAP_REASON: i32 = 235;
/// `GetUnitClass()->IsTransport()`
pub const EE_UNITS_CU_IS_TRANSPORT: i32 = 236;
/// `GetUnitClass()->CargoAdded(this, a, b != 0)`
pub const EE_UNITS_CU_CARGO_ADDED: i32 = 237;
/// `mPlayer->mActiveGroup->Contains(unit a)`
pub const EE_UNITS_CU_ACTIVE_GROUP_HAS: i32 = 238;
/// `gUserInterface->PostMessage424Once()`
pub const EE_UNITS_CU_POST_424: i32 = 239;
/// complex unit `a`'s `mHitPoints`
pub const EE_UNITS_CU_UNIT_HIT_POINTS: i32 = 240;
/// complex unit `a`'s `mUnitClass->GetPopulationCost()`
pub const EE_UNITS_CU_UNIT_POPULATION_COST: i32 = 241;
/// `GetUnitClass()->mGarrisonCapacity`
pub const EE_UNITS_CU_GARRISON_CAPACITY: i32 = 242;
/// `mGarrisonCount`
pub const EE_UNITS_CU_GARRISON_COUNT: i32 = 243;
/// `mGarrisonCount = b`
pub const EE_UNITS_CU_SET_GARRISON_COUNT: i32 = 244;
/// `mGarrisonPopulation`
pub const EE_UNITS_CU_GARRISON_POPULATION: i32 = 245;
/// `mGarrisonPopulation = b`
pub const EE_UNITS_CU_SET_GARRISON_POPULATION: i32 = 246;
/// complex unit `a`'s `Kill(0, 0, 0)`
pub const EE_UNITS_CU_KILL_UNIT: i32 = 247;
/// `EEAddDeadUnitStatistic(complex unit a)`
pub const EE_UNITS_CU_UNIT_DEAD_STATISTIC: i32 = 248;
/// `mCargo.size()`
pub const EE_UNITS_CU_CARGO_COUNT: i32 = 249;
/// `mCargo[b]` (`GetCargo(b)`)
pub const EE_UNITS_CU_CARGO_AT: i32 = 250;
/// complex unit `a`'s `mPreviousPart`
pub const EE_UNITS_CU_UNIT_PREVIOUS_PART: i32 = 251;
/// complex unit `a`'s `mUnitClass->GetUnitType()`
pub const EE_UNITS_CU_UNIT_UNIT_TYPE: i32 = 252;
/// complex unit `a`'s `mUnitClass->mCategory`
pub const EE_UNITS_CU_UNIT_CATEGORY: i32 = 253;
/// complex unit `a`'s `mPlayer->mSavedGroups.Reset(b, 1)`
pub const EE_UNITS_CU_UNIT_STATS_RESET: i32 = 254;
/// complex unit `a`'s `mPlayer->mSavedGroups.Lower(b, 1)`
pub const EE_UNITS_CU_UNIT_STATS_LOWER: i32 = 255;
/// `GetUnitClass()->CargoRemoved(this, a)`
pub const EE_UNITS_CU_CARGO_REMOVED: i32 = 256;
/// `mCargo.erase(mCargo.begin() + b)`
pub const EE_UNITS_CU_CARGO_ERASE_AT: i32 = 257;
/// complex unit `a`'s `mContainer->Dereference()`
pub const EE_UNITS_CU_UNIT_CONTAINER_DEREFERENCE: i32 = 258;
/// complex unit `a`'s `mContainer = NULL`
pub const EE_UNITS_CU_UNIT_CLEAR_CONTAINER: i32 = 259;
/// `mCargo[b] = complex unit a`
pub const EE_UNITS_CU_CARGO_SET_AT: i32 = 260;
/// `GetUnitClass()->CargoUnloaded(this, a)`
pub const EE_UNITS_CU_CARGO_UNLOADED: i32 = 261;
/// complex unit `a`'s `SetTransformation(the transformation at b, true)`
pub const EE_UNITS_CU_UNIT_SET_TRANSFORMATION: i32 = 262;
/// complex unit `a`'s `mUnitClass->mDomains`
pub const EE_UNITS_CU_UNIT_DOMAINS: i32 = 263;
/// complex unit `a`'s `mPlanetID = EEGetLandContinentOf(a)`
pub const EE_UNITS_CU_UNIT_SET_PLANET: i32 = 264;
/// complex unit `a`'s `mUnitClass->GetCruiseAltitude()` (its bits)
pub const EE_UNITS_CU_UNIT_CRUISE_ALTITUDE: i32 = 265;
/// complex unit `a`'s `SetTransformation(a copy of the transformation at b with Z the float of the bits c, true)`
pub const EE_UNITS_CU_UNIT_SET_TRANSFORMATION_AT_Z: i32 = 266;
/// complex unit `a`'s `GetTileX()`
pub const EE_UNITS_CU_UNIT_TILE_X: i32 = 267;
/// `gWorld.ActivateUnit(complex unit a)`
pub const EE_UNITS_CU_ACTIVATE_UNIT: i32 = 268;
/// `gWorld.DeactivateUnit(complex unit a)`
pub const EE_UNITS_CU_DEACTIVATE_UNIT: i32 = 269;
/// complex unit `a`'s `mPlayer->IsHuman()`
pub const EE_UNITS_CU_UNIT_IS_HUMAN: i32 = 270;
/// complex unit `a`'s `StartAction(b)`
pub const EE_UNITS_CU_UNIT_START_ACTION: i32 = 271;
/// complex unit `a`'s `mUnitClass->ActionChanged(a)`
pub const EE_UNITS_CU_UNIT_ACTION_CHANGED: i32 = 272;
/// complex unit `a`'s `mReferenceCount`
pub const EE_UNITS_CU_UNIT_REFERENCE_COUNT: i32 = 273;
/// `GetStats()->mRange` (its bits)
pub const EE_UNITS_CU_STATS_RANGE: i32 = 274;
/// the transport class's `HasRoom(this, complex unit a)`
pub const EE_UNITS_CU_TRANSPORT_HAS_ROOM: i32 = 275;
/// the transport class's `mUnloadRange` (its bits)
pub const EE_UNITS_CU_TRANSPORT_UNLOAD_RANGE: i32 = 276;
/// `UnloadCargoAt(b, the transformation at a)` (its answer)
pub const EE_UNITS_CU_UNLOAD_CARGO_AT: i32 = 277;
/// `CreateUnitEffect(complex unit a, kGraphicEffectCalamityTeleportIn, NULL, false, 0, 0)`
pub const EE_UNITS_CU_TELEPORT_IN_EFFECT: i32 = 278;
/// `ReleaseCargo(complex unit a)`
pub const EE_UNITS_CU_RELEASE_CARGO: i32 = 279;
/// the transport class's `GetUnloadFailedTextID()`
pub const EE_UNITS_CU_UNLOAD_FAILED_TEXT: i32 = 280;
/// `mPlayer->mEvents.AddEvent(this's tile, 2, mPlayer, b, false)`
pub const EE_UNITS_CU_ADD_EVENT: i32 = 281;
/// `gUnitSetManager.GetSet(99)->Contains(GetUnitClass()->GetUnitType())`
pub const EE_UNITS_CU_SET99_HAS_TYPE: i32 = 282;
/// the transport class's `mDomains`
pub const EE_UNITS_CU_TRANSPORT_DOMAINS: i32 = 283;
/// `mTileX`
pub const EE_UNITS_CU_TILE_X_FIELD: i32 = 284;
/// `GetTileY()`
pub const EE_UNITS_CU_TILE_Y: i32 = 285;
/// `GetUnitClass()->GetTileSize()`
pub const EE_UNITS_CU_TILE_SIZE: i32 = 286;
/// `mGoalState != NULL && mGoalState->mKind == kGoalBuildRepair`
pub const EE_UNITS_CU_GOAL_IS_BUILD: i32 = 287;
/// `EEBuildRepairGoalCast(GetGoal())->GetBuildTarget()`
pub const EE_UNITS_CU_BUILD_GOAL_TARGET: i32 = 288;
/// `mCitizenBuildQueue.push_front(unit a)`
pub const EE_UNITS_CU_BUILD_QUEUE_PUSH_FRONT: i32 = 289;
/// `mCitizenBuildQueue.push_back(unit a)`
pub const EE_UNITS_CU_BUILD_QUEUE_PUSH_BACK: i32 = 290;
/// the index of unit `a` in `mCitizenBuildQueue` (`std::find`; -1: none)
pub const EE_UNITS_CU_BUILD_QUEUE_FIND: i32 = 291;
/// `mCitizenBuildQueue.erase(mCitizenBuildQueue.begin() + b)` (the single-iterator erase)
pub const EE_UNITS_CU_BUILD_QUEUE_ERASE_AT: i32 = 292;
/// `DropFinishedGoals(0)`
pub const EE_UNITS_CU_DROP_FINISHED_GOALS: i32 = 293;
/// `ClearGoals(0)`
pub const EE_UNITS_CU_CLEAR_GOALS: i32 = 294;
/// `mHasShiftQueuedBuild = false`
pub const EE_UNITS_CU_CLEAR_SHIFT_QUEUED: i32 = 295;
/// `mContacts.Find(complex unit a) == 0`
pub const EE_UNITS_CU_CONTACT_MISSING: i32 = 296;
/// `mContacts.AddHead(complex unit a)`
pub const EE_UNITS_CU_CONTACT_ADD_HEAD: i32 = 297;
/// `this->Reference()`
pub const EE_UNITS_CU_REFERENCE_THIS: i32 = 298;
/// `EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>(GetUnitClass())` (0: not that kind)
pub const EE_UNITS_CU_TRANSPORT_CLASS: i32 = 299;

// ---- the bodies (`ee_units_cu_body`'s `what`, from 20)
pub const EE_UNITS_CU_DO_UPDATE_JIT: i32 = 20;
pub const EE_UNITS_CU_DO_CANCEL_QUEUED_PRODUCTION: i32 = 21;
pub const EE_UNITS_CU_DO_TOGGLE_JIT: i32 = 22;
pub const EE_UNITS_CU_DO_REMOVE_RESEARCH: i32 = 23;
pub const EE_UNITS_CU_DO_REPLACE_RESEARCH: i32 = 24;
pub const EE_UNITS_CU_DO_QUEUE_RESEARCH: i32 = 25;
pub const EE_UNITS_CU_DO_LOAD_CARGO: i32 = 26;
pub const EE_UNITS_CU_DO_GARRISON_UNIT: i32 = 27;
pub const EE_UNITS_CU_DO_ADD_GARRISON: i32 = 28;
pub const EE_UNITS_CU_DO_KILL_CARGO: i32 = 29;
pub const EE_UNITS_CU_DO_RELEASE_CARGO: i32 = 30;
pub const EE_UNITS_CU_DO_REPLACE_CARGO: i32 = 31;
pub const EE_UNITS_CU_DO_UNLOAD_CARGO_AT: i32 = 32;
pub const EE_UNITS_CU_DO_UNLOAD_CARGO_AT_TILE: i32 = 33;
pub const EE_UNITS_CU_DO_UNLOAD_CARGO_NOW: i32 = 34;
pub const EE_UNITS_CU_DO_ORDER_REPAIR: i32 = 35;
pub const EE_UNITS_CU_DO_ADD_BUILD_TARGET: i32 = 36;
pub const EE_UNITS_CU_DO_ADD_CONTACT: i32 = 37;

/// `kTechNone`, `kAIKindHero`, the unit types, categories and actions the bodies test
const TECH_NONE: i64 = 100000;
const AI_KIND_HERO: i64 = 0x1b;
const UNIT_TYPE_FORTRESS: i64 = 0x2a0;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_TELEPORTER: i64 = 0x313;
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_SP15_SPACE_FIGHTER: i64 = 0x2f6;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
const BUILDING_CATEGORIES: [i64; 5] = [2, 0x13, 0x19, 0x18, 0x1d];
const MOVEMENT_AIR: i64 = 1;
const ACTION_IDLE: i64 = 5000;

/// `ee_units_uu_find_placement`'s arguments (the point is the origin: the callers' `thePoint(0, 0, 0)`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Placement {
    pub unit: u64,
    pub x: u32,
    pub y: u32,
    pub size: i32,
    pub rings: i32,
    pub corners: bool,
    pub include_center: bool,
    pub face_point: bool,
    pub linked: bool,
}

/// The production and cargo bodies' host: the statements, and the placement search.
pub trait CuCargoHost: CuHost {
    /// `ee_units_uu_find_placement(&origin, p.unit, 0, p.x, p.y, p.size, p.rings, ..., m, 0)`
    fn find_placement(&mut self, p: &Placement, m: &mut [[f32; 4]; 4]) -> bool;
}

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

fn float(bits: i64) -> f32 {
    f32::from_bits(bits as u32)
}

/// A `long` member's value as the C++ holds it.
fn long(value: i64) -> c_long {
    value as c_long
}

/// The class of node `node`'s unit class index, when the index is positive and the class exists (0 else), and
/// whether it is a hero: `uc > 0 && gUnitClassTable[uc] != NULL && gUnitClassTable[uc]->mAIKind == kAIKindHero`.
fn makes_hero<H: CuHost + ?Sized>(h: &mut H, unit: u64, node: u64) -> bool {
    let index = sa(h, crate::units::cu_cargo::EE_UNITS_CU_NODE_CLASS_INDEX, unit, node);
    if long(index) <= 0 {
        return false;
    }
    let class = sb(h, crate::units::cu_body::EE_UNITS_CU_CLASS_TABLE, unit, index) as u64;
    class != 0 && sa(h, EE_UNITS_CU_CLASS_AI_KIND, unit, class) == AI_KIND_HERO
}

/// `... && mNumHeroesInProduction > 0` then `mNumHeroesInProduction--`
fn release_hero<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let heroes = long(s(h, EE_UNITS_CU_HEROES_IN_PRODUCTION, unit));
    if heroes > 0 {
        sb(h, EE_UNITS_CU_SET_HEROES_IN_PRODUCTION, unit, i64::from(heroes.wrapping_sub(1)));
    }
}

/// A node's release: a technology made allowed again (`true`: the JIT list changed), or a hero's production slot
/// given back.
fn release_node<H: CuHost + ?Sized>(h: &mut H, unit: u64, node: u64) -> bool {
    if long(sa(h, EE_UNITS_CU_NODE_CLASS_INDEX, unit, node)) <= 0 {
        sab(h, EE_UNITS_CU_SET_NODE_ALLOWED, unit, node, 1);
        return true;
    }
    if makes_hero(h, unit, node) && long(s(h, EE_UNITS_CU_HEROES_IN_PRODUCTION, unit)) > 0 {
        release_hero(h, unit);
    }
    false
}

/// `UpdateJITManufacturing()`: the current research and the whole build queue released and refunded (the queue
/// from the back).
pub fn update_jit<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    use crate::units::cu_body::*;
    let mut changed = false;
    let node = s(h, EE_UNITS_CU_CURRENT_NODE, unit) as u64;
    if node != 0 && release_node(h, unit, node) {
        changed = true;
    }
    while s(h, EE_UNITS_CU_QUEUE_SIZE, unit) != 0 {
        let technology = s(h, EE_UNITS_CU_QUEUE_BACK_TECHNOLOGY, unit);
        let node = sb(h, EE_UNITS_CU_TECH_NODE, unit, technology) as u64;
        if node != 0 && release_node(h, unit, node) {
            changed = true;
        }
        sab(h, EE_UNITS_CU_REFUND_TECHNOLOGY, unit, node, i64::from(1.0f32.to_bits()));
        s(h, EE_UNITS_CU_QUEUE_POP_BACK, unit);
    }
    if changed && s(h, EE_UNITS_CU_SHUTDOWN, unit) == 0 {
        s(h, EE_UNITS_CU_UI_UPDATE_JIT, unit);
    }
}

/// `if (uc > 0 && hero && heroes > 0) heroes--` (the queue's cancel and removal)
pub(crate) fn release_queued_hero<H: CuHost + ?Sized>(h: &mut H, unit: u64, node: u64) {
    if makes_hero(h, unit, node) && long(s(h, EE_UNITS_CU_HEROES_IN_PRODUCTION, unit)) > 0 {
        release_hero(h, unit);
    }
}

/// `CancelQueuedProduction()`: the build queue emptied from the front, each entry allowed again and refunded.
pub fn cancel_queued_production<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    use crate::units::cu_body::*;
    if s(h, EE_UNITS_CU_QUEUE_SIZE, unit) as u64 == 0 {
        return;
    }
    while s(h, EE_UNITS_CU_QUEUE_SIZE, unit) as u64 > 0 {
        let technology = s(h, EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY, unit);
        s(h, EE_UNITS_CU_QUEUE_POP_FRONT, unit);
        let node = sb(h, EE_UNITS_CU_PLAYER_NODE, unit, technology) as u64;
        sab(h, EE_UNITS_CU_SET_NODE_ALLOWED, unit, node, 1);
        release_queued_hero(h, unit, node);
        let node = sb(h, EE_UNITS_CU_PLAYER_NODE, unit, technology) as u64;
        sa(h, EE_UNITS_CU_REFUND_ONE, unit, node);
    }
}

/// `ToggleJITManufacturing()`: the current research cancelled and refunded.
pub fn toggle_jit<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    use crate::units::cu_body::*;
    let current = s(h, EE_UNITS_CU_CURRENT_RESEARCH, unit);
    if current == TECH_NONE {
        return;
    }
    let node = s(h, EE_UNITS_CU_CURRENT_NODE, unit) as u64;
    sab(h, EE_UNITS_CU_SET_NODE_ALLOWED, unit, node, 1);
    release_queued_hero(h, unit, node);
    sab(h, EE_UNITS_CU_REFUND_TECHNOLOGY, unit, node, i64::from(1.0f32.to_bits()));
    sb(h, EE_UNITS_CU_PLAY_EFFECT, unit, 100000);
    sb(h, EE_UNITS_CU_SET_PRODUCTION_TIME_FIELD, unit, 0);
}

/// `RemoveResearch(technology, remove_all)`: the matching queued entries (the first only unless `remove_all`, a
/// byte) allowed again and refunded, the rest kept in order; with none queued, the current research cancelled when
/// it matches (a land building's).
pub fn remove_research<H: CuHost + ?Sized>(h: &mut H, unit: u64, technology: i64, remove_all: i64) {
    use crate::units::cu_body::*;
    let remove_all = remove_all as u8 != 0;
    let mut kept: Vec<(i64, i64)> = Vec::new();
    let mut retained = false;
    let mut keep_next = true;
    if s(h, EE_UNITS_CU_QUEUE_SIZE, unit) as u64 > 0 {
        while s(h, EE_UNITS_CU_QUEUE_SIZE, unit) as u64 > 0 {
            let entry = (s(h, EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY, unit), s(h, EE_UNITS_CU_QUEUE_FRONT_FLAG, unit));
            s(h, EE_UNITS_CU_QUEUE_POP_FRONT, unit);
            if !crate::units::uu_reads::is_same_research(entry.0 as i32, technology as i32) || !keep_next {
                kept.push(entry);
            } else {
                let node = sb(h, EE_UNITS_CU_PLAYER_NODE, unit, entry.0) as u64;
                sab(h, EE_UNITS_CU_SET_NODE_ALLOWED, unit, node, 1);
                keep_next = remove_all;
                retained = true;
                release_queued_hero(h, unit, node);
                let node = sb(h, EE_UNITS_CU_PLAYER_NODE, unit, entry.0) as u64;
                sa(h, EE_UNITS_CU_REFUND_ONE, unit, node);
            }
        }
        s(h, EE_UNITS_CU_QUEUE_CLEAR, unit);
        for (technology, flag) in kept {
            h.cu(EE_UNITS_CU_QUEUE_PUSH_BACK, unit, 0, technology, flag, &mut [0.0; 4]);
        }
        if retained && !remove_all {
            return;
        }
    }
    if s(h, EE_UNITS_CU_IS_LAND_BUILDING, unit) != 0 {
        let current = s(h, EE_UNITS_CU_CURRENT_RESEARCH, unit);
        if crate::units::uu_reads::is_same_research(current as i32, technology as i32) {
            toggle_jit(h, unit);
        }
    }
}

/// `ReplaceResearch(technology, new)`: the current research's effect switched (a hero counted), the queued
/// entries of `technology` renamed (or, for no new technology, dropped), from the back.
pub fn replace_research<H: CuHost + ?Sized>(h: &mut H, unit: u64, technology: i64, new: i64) {
    use crate::units::cu_body::*;
    let (technology, new) = (long(technology), long(new));
    if long(s(h, EE_UNITS_CU_CURRENT_RESEARCH, unit)) == technology {
        sb(h, EE_UNITS_CU_PLAY_EFFECT, unit, i64::from(new));
        if i64::from(new) == TECH_NONE {
            sb(h, EE_UNITS_CU_SET_PRODUCTION_TIME_FIELD, unit, 0);
        } else {
            let node = sb(h, EE_UNITS_CU_TECH_NODE, unit, i64::from(new)) as u64;
            if node != 0 && makes_hero(h, unit, node) {
                let heroes = long(s(h, EE_UNITS_CU_HEROES_IN_PRODUCTION, unit));
                sb(h, EE_UNITS_CU_SET_HEROES_IN_PRODUCTION, unit, i64::from(heroes.wrapping_add(1)));
            }
        }
    }
    let size = s(h, EE_UNITS_CU_QUEUE_SIZE, unit);
    if size as u64 == 0 {
        return;
    }
    let mut i = size - 1;
    while i >= 0 {
        if long(sb(h, EE_UNITS_CU_QUEUE_TECHNOLOGY_AT, unit, i)) == technology {
            if i64::from(new) != TECH_NONE {
                h.cu(EE_UNITS_CU_QUEUE_SET_TECHNOLOGY_AT, unit, 0, i, i64::from(new), &mut [0.0; 4]);
            } else {
                sb(h, EE_UNITS_CU_QUEUE_ERASE_AT, unit, i);
            }
        }
        i -= 1;
    }
}

/// `QueueResearchUncaptured(technology, front)`: an affordable, allowed entry (a hero within the limit) queued and
/// paid; a technology (no unit class, not kind 3) is no longer allowed.
pub fn queue_research<H: CuHost + ?Sized>(h: &mut H, unit: u64, technology: i64, front: bool) {
    use crate::units::cu_body::*;
    let node = sb(h, EE_UNITS_CU_TECH_NODE, unit, technology) as u64;
    if sa(h, EE_UNITS_CU_CAN_AFFORD, unit, node) == 0 {
        return;
    }
    let mut allowed = sa(h, EE_UNITS_CU_NODE_ALLOWED, unit, node) != 0;
    if allowed && makes_hero(h, unit, node) && s(h, EE_UNITS_CU_SCENARIO_GAME, unit) == 0 {
        let limit = long(s(h, EE_UNITS_CU_HERO_LIMIT, unit));
        allowed = long(s(h, EE_UNITS_CU_HERO_COUNT, unit)) < limit;
    }
    if !allowed {
        return;
    }
    let flag = s(h, EE_UNITS_CU_JIT_MANUFACTURING, unit) != 0;
    let op = if front { EE_UNITS_CU_QUEUE_PUSH_FRONT } else { EE_UNITS_CU_QUEUE_PUSH_BACK };
    h.cu(op, unit, 0, technology, i64::from(flag), &mut [0.0; 4]);
    if makes_hero(h, unit, node) {
        let heroes = long(s(h, EE_UNITS_CU_HEROES_IN_PRODUCTION, unit));
        sb(h, EE_UNITS_CU_SET_HEROES_IN_PRODUCTION, unit, i64::from(heroes.wrapping_add(1)));
    }
    sa(h, EE_UNITS_CU_PAY, unit, node);
    let index = long(sa(h, EE_UNITS_CU_NODE_CLASS_INDEX, unit, node));
    let no_class = index < 0 || sb(h, EE_UNITS_CU_CLASS_TABLE, unit, i64::from(index)) == 0;
    if no_class && sa(h, EE_UNITS_CU_NODE_KIND, unit, node) != 3 {
        sab(h, EE_UNITS_CU_SET_NODE_ALLOWED, unit, node, 0);
        s(h, EE_UNITS_CU_UI_UPDATE_JIT, unit);
    }
}

/// `pp->mActiveGroup->Contains(...)`'s notice to the current player
fn notice_if_selected<H: CuHost + ?Sized>(h: &mut H, unit: u64, also: u64) {
    use crate::units::cu_body::*;
    if s(h, EE_UNITS_CU_PLAYER, unit) == s(h, EE_UNITS_CU_CURRENT_PLAYER, unit)
        && (sa(h, EE_UNITS_CU_ACTIVE_GROUP_HAS, unit, unit) != 0
            || (also != 0 && sa(h, EE_UNITS_CU_ACTIVE_GROUP_HAS, unit, also) != 0))
    {
        s(h, EE_UNITS_CU_POST_424, unit);
    }
}

/// `LoadCargo(cargo, ignore_room, arg3)`: 0 when the living cargo fits (or the room is ignored): its weight
/// taken, it is aboard (referenced, contained), off the map with its reason, the class told; else 7.
pub fn load_cargo<H: CuHost + ?Sized>(h: &mut H, unit: u64, cargo: u64, ignore_room: bool, arg3: bool) -> i64 {
    use crate::units::cu_body::*;
    let cost = long(sa(h, EE_UNITS_CU_UNIT_WEIGHT, unit, cargo));
    let room = long(s(h, EE_UNITS_CU_CARGO_ROOM, unit));
    if !((room >= cost || ignore_room) && long(sa(h, EE_UNITS_CU_UNIT_HIT_POINTS, unit, cargo)) > 0) {
        return 7;
    }
    sb(h, EE_UNITS_CU_SET_CARGO_ROOM, unit, i64::from(room.wrapping_sub(cost)));
    sa(h, EE_UNITS_CU_CARGO_PUSH, unit, cargo);
    sa(h, EE_UNITS_CU_REFERENCE, unit, cargo);
    if sa(h, EE_UNITS_CU_UNIT_CONTAINER, unit, cargo) as u64 != unit {
        sa(h, EE_UNITS_CU_SET_CONTAINER_THIS, unit, cargo);
        s(h, EE_UNITS_CU_REFERENCE_THIS, unit);
    }
    sa(h, EE_UNITS_CU_TAKE_OFF_MAP, unit, cargo);
    let info = sa(h, EE_UNITS_CU_OFF_MAP_INFO, unit, cargo) as u64;
    if info != 0 {
        if s(h, EE_UNITS_CU_IS_TRANSPORT, unit) != 0 {
            let reason = if s(h, EE_UNITS_CU_UNIT_TYPE, unit) != UNIT_TYPE_FORTRESS { 1 } else { 6 };
            sab(h, EE_UNITS_CU_SET_OFF_MAP_REASON, unit, info, reason);
        } else if s(h, EE_UNITS_CU_UNIT_TYPE, unit) == UNIT_TYPE_AIRPORT {
            sab(h, EE_UNITS_CU_SET_OFF_MAP_REASON, unit, info, 5);
        }
    }
    sab(h, EE_UNITS_CU_CARGO_ADDED, unit, cargo, i64::from(arg3));
    notice_if_selected(h, unit, cargo);
    0
}

/// `GarrisonUnit(cargo)`: when its weight fits the garrison, it joins (weight and population), leaves the map
/// (reason 7), dies and counts as a dead unit.
pub fn garrison_unit<H: CuHost + ?Sized>(h: &mut H, unit: u64, cargo: u64) -> bool {
    let cost = long(sa(h, EE_UNITS_CU_UNIT_WEIGHT, unit, cargo));
    let value = long(sa(h, EE_UNITS_CU_UNIT_POPULATION_COST, unit, cargo));
    let count = long(s(h, EE_UNITS_CU_GARRISON_COUNT, unit));
    if long(s(h, EE_UNITS_CU_GARRISON_CAPACITY, unit)).wrapping_sub(count) < cost {
        return false;
    }
    sb(h, EE_UNITS_CU_SET_GARRISON_COUNT, unit, i64::from(count.wrapping_add(cost)));
    let population = long(s(h, EE_UNITS_CU_GARRISON_POPULATION, unit));
    sb(h, EE_UNITS_CU_SET_GARRISON_POPULATION, unit, i64::from(population.wrapping_add(value)));
    sa(h, EE_UNITS_CU_TAKE_OFF_MAP, unit, cargo);
    let info = sa(h, EE_UNITS_CU_OFF_MAP_INFO, unit, cargo) as u64;
    if info != 0 {
        sab(h, EE_UNITS_CU_SET_OFF_MAP_REASON, unit, info, 7);
    }
    sa(h, EE_UNITS_CU_KILL_UNIT, unit, cargo);
    let series = sa(h, EE_UNITS_CU_UNIT_DEAD_STATISTIC, unit, cargo);
    if series != 0 {
        sb(h, crate::units::cu_body::EE_UNITS_CU_STATS_LOWER, unit, series);
    }
    true
}

/// `AddGarrison(amount, population)`: the weight added (never below 0) and the population set, if it fits.
pub fn add_garrison<H: CuHost + ?Sized>(h: &mut H, unit: u64, amount: i64, population: i64) -> bool {
    let amount = long(amount);
    let count = long(s(h, EE_UNITS_CU_GARRISON_COUNT, unit));
    if long(s(h, EE_UNITS_CU_GARRISON_CAPACITY, unit)).wrapping_sub(count) < amount {
        return false;
    }
    let count = count.wrapping_add(amount);
    sb(h, EE_UNITS_CU_SET_GARRISON_COUNT, unit, i64::from(count));
    if count < 0 {
        sb(h, EE_UNITS_CU_SET_GARRISON_COUNT, unit, 0);
    }
    sb(h, EE_UNITS_CU_SET_GARRISON_POPULATION, unit, population);
    true
}

/// The dead-unit statistics of a killed unit `dead` (KillCargo's; Kill's own are cu_body.rs's).
fn count_dead<H: CuHost + ?Sized>(h: &mut H, unit: u64, dead: u64, kind: i64) {
    if sa(h, EE_UNITS_CU_UNIT_UNIT_TYPE, unit, dead) != UNIT_TYPE_PATH_POINT && !(5..=8).contains(&kind) {
        let category = sa(h, EE_UNITS_CU_UNIT_CATEGORY, unit, dead);
        let series = if BUILDING_CATEGORIES.contains(&category) { 1 } else { 4 };
        sab(h, EE_UNITS_CU_UNIT_STATS_RESET, unit, dead, series);
    }
    let series = sa(h, EE_UNITS_CU_UNIT_DEAD_STATISTIC, unit, dead);
    if series != 0 {
        sab(h, EE_UNITS_CU_UNIT_STATS_LOWER, unit, dead, series);
    }
}

/// `KillCargo()`: every cargo unit killed (from the back) and counted in its player's statistics.
pub fn kill_cargo<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    use crate::units::cu_body::*;
    while s(h, EE_UNITS_CU_CARGO_COUNT, unit) != 0 {
        let last = s(h, EE_UNITS_CU_CARGO_COUNT, unit) - 1;
        let cargo = sb(h, EE_UNITS_CU_CARGO_AT, unit, last) as u64;
        sa(h, EE_UNITS_CU_KILL_UNIT, unit, cargo);
        if sa(h, EE_UNITS_CU_UNIT_PREVIOUS_PART, unit, cargo) != 0 || s(h, EE_UNITS_CU_SHUTDOWN, unit) != 0 {
            continue;
        }
        let kind = s(h, EE_UNITS_CU_SWAP_KIND, unit);
        if kind == 1 {
            continue;
        }
        count_dead(h, unit, cargo, kind);
    }
}

/// The index of `cargo` in `mCargo` from the back (-1: not aboard).
fn cargo_index<H: CuHost + ?Sized>(h: &mut H, unit: u64, cargo: u64) -> i64 {
    let mut index = s(h, EE_UNITS_CU_CARGO_COUNT, unit) - 1;
    while index >= 0 && sb(h, EE_UNITS_CU_CARGO_AT, unit, index) as u64 != cargo {
        index -= 1;
    }
    index
}

/// `ReleaseCargo(cargo)`: the cargo taken off the list (the class told, its weight given back), released.
pub fn release_cargo<H: CuHost + ?Sized>(h: &mut H, unit: u64, cargo: u64) {
    let index = cargo_index(h, unit, cargo);
    if index < 0 {
        return;
    }
    let found = sb(h, EE_UNITS_CU_CARGO_AT, unit, index) as u64;
    sa(h, EE_UNITS_CU_CARGO_REMOVED, unit, found);
    sb(h, EE_UNITS_CU_CARGO_ERASE_AT, unit, index);
    let room = long(s(h, EE_UNITS_CU_CARGO_ROOM, unit));
    let weight = long(sa(h, EE_UNITS_CU_UNIT_WEIGHT, unit, found));
    sb(h, EE_UNITS_CU_SET_CARGO_ROOM, unit, i64::from(room.wrapping_add(weight)));
    notice_if_selected(h, unit, 0);
    sa(h, crate::units::cu_body::EE_UNITS_CU_DEREFERENCE, unit, found);
    sa(h, EE_UNITS_CU_UNIT_CONTAINER_DEREFERENCE, unit, found);
    sa(h, EE_UNITS_CU_UNIT_CLEAR_CONTAINER, unit, found);
}

/// `ReplaceCargo(old, new)`: `new` takes `old`'s place aboard (the room adjusted, never below 0).
pub fn replace_cargo<H: CuHost + ?Sized>(h: &mut H, unit: u64, old: u64, new: u64) {
    use crate::units::cu_body::*;
    // std::find from the front
    let count = s(h, EE_UNITS_CU_CARGO_COUNT, unit);
    let index = (0..count).find(|&i| sb(h, EE_UNITS_CU_CARGO_AT, unit, i) as u64 == old);
    let cost = long(sa(h, EE_UNITS_CU_UNIT_WEIGHT, unit, new));
    let Some(index) = index else { return };
    let found = sb(h, EE_UNITS_CU_CARGO_AT, unit, index) as u64;
    let room = long(s(h, EE_UNITS_CU_CARGO_ROOM, unit)).wrapping_add(long(sa(h, EE_UNITS_CU_UNIT_WEIGHT, unit, found)));
    sb(h, EE_UNITS_CU_SET_CARGO_ROOM, unit, i64::from(room));
    sa(h, EE_UNITS_CU_DEREFERENCE, unit, found);
    sa(h, EE_UNITS_CU_UNIT_CONTAINER_DEREFERENCE, unit, found);
    sa(h, EE_UNITS_CU_UNIT_CLEAR_CONTAINER, unit, found);
    sab(h, EE_UNITS_CU_CARGO_SET_AT, unit, new, index);
    let room = room.wrapping_sub(cost);
    sb(h, EE_UNITS_CU_SET_CARGO_ROOM, unit, i64::from(room));
    sb(h, EE_UNITS_CU_SET_CARGO_ROOM, unit, i64::from(if room > 0 { room } else { 0 }));
    sa(h, EE_UNITS_CU_REFERENCE, unit, new);
    if sa(h, EE_UNITS_CU_UNIT_CONTAINER, unit, new) as u64 != unit {
        sa(h, EE_UNITS_CU_SET_CONTAINER_THIS, unit, new);
        s(h, EE_UNITS_CU_REFERENCE_THIS, unit);
    }
}

fn citizen(category: i64) -> bool {
    category == CATEGORY_LAND_CITIZEN || category == CATEGORY_NAVAL_CITIZEN
}

/// `if (c->mContainer) c->mContainer->Dereference(); c->mContainer = NULL;`
fn drop_container<H: CuHost + ?Sized>(h: &mut H, unit: u64, cargo: u64) {
    if sa(h, EE_UNITS_CU_UNIT_CONTAINER, unit, cargo) != 0 {
        sa(h, EE_UNITS_CU_UNIT_CONTAINER_DEREFERENCE, unit, cargo);
    }
    sa(h, EE_UNITS_CU_UNIT_CLEAR_CONTAINER, unit, cargo);
}

/// `UnloadCargoAt(index, location)` (`location` the address of its `GETransformation`): the living cargo placed
/// (an aircraft at cruise altitude unless at an air base), activated, idle; it leaves the list and its container;
/// the answer is the cargo when something else still references it, else 0.
pub fn unload_cargo_at<H: CuHost + ?Sized>(h: &mut H, unit: u64, index: i64, location: u64) -> i64 {
    use crate::units::cu_body::*;
    let cargo = sb(h, EE_UNITS_CU_CARGO_AT, unit, index) as u64;
    if long(sa(h, EE_UNITS_CU_UNIT_HIT_POINTS, unit, cargo)) > 0 {
        sa(h, EE_UNITS_CU_CARGO_UNLOADED, unit, cargo);
        sab(h, EE_UNITS_CU_UNIT_SET_TRANSFORMATION, unit, cargo, location as i64);
        if sa(h, EE_UNITS_CU_UNIT_DOMAINS, unit, cargo) & MOVEMENT_AIR != 0 {
            sa(h, EE_UNITS_CU_UNIT_SET_PLANET, unit, cargo);
            if s(h, EE_UNITS_CU_IS_AIR_BASE, unit) == 0 {
                // SAFETY: the C++ member's live transformation argument (16 floats, the translation in column 3)
                let z = unsafe { (*(location as usize as *const [[f32; 4]; 4]))[2][3] };
                let z = z + float(sa(h, EE_UNITS_CU_UNIT_CRUISE_ALTITUDE, unit, cargo));
                h.cu(
                    EE_UNITS_CU_UNIT_SET_TRANSFORMATION_AT_Z,
                    unit,
                    cargo,
                    location as i64,
                    i64::from(z.to_bits()),
                    &mut [0.0; 4],
                );
            }
        }
        if sa(h, EE_UNITS_CU_UNIT_PREVIOUS_PART, unit, cargo) == 0 {
            if long(sa(h, EE_UNITS_CU_UNIT_TILE_X, unit, cargo)) != -10000 {
                sa(h, EE_UNITS_CU_ACTIVATE_UNIT, unit, cargo);
            } else {
                sa(h, EE_UNITS_CU_DEACTIVATE_UNIT, unit, cargo);
            }
        }
    }
    let category = sa(h, EE_UNITS_CU_UNIT_CATEGORY, unit, cargo);
    if sa(h, EE_UNITS_CU_UNIT_IS_HUMAN, unit, cargo) == 0 && citizen(category) {
        drop_container(h, unit, cargo);
    }
    if long(sa(h, EE_UNITS_CU_UNIT_HIT_POINTS, unit, cargo)) > 0
        && (sa(h, EE_UNITS_CU_UNIT_IS_HUMAN, unit, cargo) != 0
            || (sa(h, EE_UNITS_CU_UNIT_DOMAINS, unit, cargo) & MOVEMENT_AIR == 0
                && sa(h, EE_UNITS_CU_UNIT_UNIT_TYPE, unit, cargo) != UNIT_TYPE_SP15_SPACE_FIGHTER))
    {
        sab(h, EE_UNITS_CU_UNIT_START_ACTION, unit, cargo, ACTION_IDLE);
        sa(h, EE_UNITS_CU_UNIT_ACTION_CHANGED, unit, cargo);
    }
    let category = sa(h, EE_UNITS_CU_UNIT_CATEGORY, unit, cargo);
    if sa(h, EE_UNITS_CU_UNIT_IS_HUMAN, unit, cargo) != 0 || !citizen(category) {
        drop_container(h, unit, cargo);
    }
    sb(h, EE_UNITS_CU_CARGO_ERASE_AT, unit, index);
    let room = long(s(h, EE_UNITS_CU_CARGO_ROOM, unit)).wrapping_add(long(sa(h, EE_UNITS_CU_UNIT_WEIGHT, unit, cargo)));
    sb(h, EE_UNITS_CU_SET_CARGO_ROOM, unit, i64::from(room));
    let references = sa(h, EE_UNITS_CU_UNIT_REFERENCE_COUNT, unit, cargo) as u32;
    notice_if_selected(h, unit, 0);
    sa(h, EE_UNITS_CU_DEREFERENCE, unit, cargo);
    if references > 1 {
        cargo as i64
    } else {
        0
    }
}

/// The identity (`GETransformation()`): the callers' placement before the search writes it.
fn identity() -> [[f32; 4]; 4] {
    let mut m = [[0.0; 4]; 4];
    for (i, row) in m.iter_mut().enumerate() {
        row[i] = 1.0;
    }
    m
}

/// `theTransportClass`: the transport kind of the class, for a transport (`IsTransport()` first)
fn transport_class<H: CuHost + ?Sized>(h: &mut H, unit: u64) -> bool {
    s(h, EE_UNITS_CU_IS_TRANSPORT, unit) != 0 && s(h, EE_UNITS_CU_TRANSPORT_CLASS, unit) != 0
}

/// The rings of an unload search: the transport class's unload range, else the stats' range (`UMFloatToLong`).
fn unload_rings<H: CuHost + ?Sized>(h: &mut H, unit: u64, transport: bool) -> i32 {
    let range = if transport {
        sa(h, EE_UNITS_CU_TRANSPORT_UNLOAD_RANGE, unit, 0)
    } else {
        s(h, EE_UNITS_CU_STATS_RANGE, unit)
    };
    ee_base::math::um::lrintf(float(range))
}

/// `UnloadCargoAtTile(x, y, corners)`: every cargo unit that fits near the tile unloaded (from the back; a
/// teleporter's with the teleport effect), dead ones released; an unload-failed event when any stay aboard.
pub fn unload_cargo_at_tile<H: CuCargoHost + ?Sized>(h: &mut H, unit: u64, x: i64, y: i64, corners: bool) {
    let mut m = identity();
    let mut event: i64 = -1;
    let transport = transport_class(h, unit);
    let mut i = s(h, EE_UNITS_CU_CARGO_COUNT, unit) - 1;
    while i >= 0 {
        let cargo = sb(h, EE_UNITS_CU_CARGO_AT, unit, i) as u64;
        if s(h, EE_UNITS_CU_IS_TRANSPORT, unit) != 0
            && (!transport || sa(h, EE_UNITS_CU_TRANSPORT_HAS_ROOM, unit, cargo) == 0)
        {
            i -= 1;
            continue;
        }
        if long(sa(h, EE_UNITS_CU_UNIT_HIT_POINTS, unit, cargo)) >= 1 {
            let rings = unload_rings(h, unit, transport);
            let p = Placement {
                unit: cargo,
                x: x as u32,
                y: y as u32,
                size: 1,
                rings,
                corners,
                include_center: true,
                face_point: false,
                linked: false,
            };
            if h.find_placement(&p, &mut m) {
                sab(h, EE_UNITS_CU_UNLOAD_CARGO_AT, unit, m.as_ptr() as u64, i);
                if s(h, crate::units::cu_body::EE_UNITS_CU_UNIT_TYPE, unit) == UNIT_TYPE_TELEPORTER {
                    sa(h, EE_UNITS_CU_TELEPORT_IN_EFFECT, unit, cargo);
                }
            }
        } else {
            sa(h, EE_UNITS_CU_RELEASE_CARGO, unit, cargo);
        }
        i -= 1;
    }
    if s(h, EE_UNITS_CU_CARGO_COUNT, unit) != 0 {
        if transport {
            event = s(h, EE_UNITS_CU_UNLOAD_FAILED_TEXT, unit);
        }
        sb(h, EE_UNITS_CU_ADD_EVENT, unit, event);
    }
}

/// `UnloadCargoNow(cargo)`: the cargo unloaded next to the transport (a dead one released): whether it was.
pub fn unload_cargo_now<H: CuCargoHost + ?Sized>(h: &mut H, unit: u64, cargo: u64) -> bool {
    let mut m = identity();
    let transport = transport_class(h, unit);
    let index = cargo_index(h, unit, cargo);
    if index < 0
        || !(s(h, EE_UNITS_CU_IS_TRANSPORT, unit) == 0
            || (transport && sa(h, EE_UNITS_CU_TRANSPORT_HAS_ROOM, unit, cargo) != 0))
    {
        return false;
    }
    if long(sa(h, EE_UNITS_CU_UNIT_HIT_POINTS, unit, cargo)) <= 0 {
        sa(h, EE_UNITS_CU_RELEASE_CARGO, unit, cargo);
        return true;
    }
    let rings = unload_rings(h, unit, transport);
    let linked = transport && (s(h, EE_UNITS_CU_TRANSPORT_DOMAINS, unit) >> 2) & 1 != 0;
    let p = Placement {
        unit: cargo,
        x: s(h, EE_UNITS_CU_TILE_X_FIELD, unit) as u32,
        y: s(h, EE_UNITS_CU_TILE_Y, unit) as u32,
        size: s(h, EE_UNITS_CU_TILE_SIZE, unit) as i32,
        rings,
        corners: s(h, EE_UNITS_CU_SET99_HAS_TYPE, unit) != 0,
        include_center: true,
        face_point: false,
        linked,
    };
    if h.find_placement(&p, &mut m) {
        sab(h, EE_UNITS_CU_UNLOAD_CARGO_AT, unit, m.as_ptr() as u64, index);
        return true;
    }
    false
}

/// `OrderRepair(target)`: the citizen's current build target kept first, its goals dropped, idle, `target` first in
/// its build queue (referenced once).
pub fn order_repair<H: CuHost + ?Sized>(h: &mut H, unit: u64, target: u64) {
    use crate::units::cu_body::*;
    if s(h, EE_UNITS_CU_GOAL_IS_BUILD, unit) != 0 {
        let current = s(h, EE_UNITS_CU_BUILD_GOAL_TARGET, unit) as u64;
        sa(h, EE_UNITS_CU_REFERENCE, unit, current);
        sa(h, EE_UNITS_CU_BUILD_QUEUE_PUSH_FRONT, unit, current);
    }
    s(h, EE_UNITS_CU_DROP_FINISHED_GOALS, unit);
    s(h, EE_UNITS_CU_CLEAR_GOALS, unit);
    sb(h, EE_UNITS_CU_START_ACTION, unit, ACTION_IDLE);
    s(h, EE_UNITS_CU_ACTION_CHANGED, unit);
    let at = sa(h, EE_UNITS_CU_BUILD_QUEUE_FIND, unit, target);
    if at < 0 {
        sa(h, EE_UNITS_CU_REFERENCE, unit, target);
    } else {
        sb(h, EE_UNITS_CU_BUILD_QUEUE_ERASE_AT, unit, at);
    }
    sa(h, EE_UNITS_CU_BUILD_QUEUE_PUSH_FRONT, unit, target);
    s(h, EE_UNITS_CU_CLEAR_SHIFT_QUEUED, unit);
}

/// `AddBuildTarget(target)`: `target` appended to the build queue unless queued.
pub fn add_build_target<H: CuHost + ?Sized>(h: &mut H, unit: u64, target: u64) {
    if sa(h, EE_UNITS_CU_BUILD_QUEUE_FIND, unit, target) < 0 {
        sa(h, EE_UNITS_CU_REFERENCE, unit, target);
        sa(h, EE_UNITS_CU_BUILD_QUEUE_PUSH_BACK, unit, target);
    }
}

/// `AddContact(other)`: `other` added to the contacts unless there.
pub fn add_contact<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64) {
    if sa(h, EE_UNITS_CU_CONTACT_MISSING, unit, other) != 0 {
        sa(h, EE_UNITS_CU_REFERENCE, unit, other);
        sa(h, EE_UNITS_CU_CONTACT_ADD_HEAD, unit, other);
    }
}

/// Runs body `what` (`EE_UNITS_CU_DO_*` from 20) on `unit`; `None` for another body.
pub fn run<H: CuCargoHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: i64, b: i64, c: i64) -> Option<i64> {
    Some(match what {
        EE_UNITS_CU_DO_UPDATE_JIT => {
            update_jit(h, unit);
            0
        }
        EE_UNITS_CU_DO_CANCEL_QUEUED_PRODUCTION => {
            cancel_queued_production(h, unit);
            0
        }
        EE_UNITS_CU_DO_TOGGLE_JIT => {
            toggle_jit(h, unit);
            0
        }
        EE_UNITS_CU_DO_REMOVE_RESEARCH => {
            remove_research(h, unit, a, b);
            0
        }
        EE_UNITS_CU_DO_REPLACE_RESEARCH => {
            replace_research(h, unit, a, b);
            0
        }
        EE_UNITS_CU_DO_QUEUE_RESEARCH => {
            queue_research(h, unit, a, b != 0);
            0
        }
        EE_UNITS_CU_DO_LOAD_CARGO => load_cargo(h, unit, a as u64, b != 0, c != 0),
        EE_UNITS_CU_DO_GARRISON_UNIT => i64::from(garrison_unit(h, unit, a as u64)),
        EE_UNITS_CU_DO_ADD_GARRISON => i64::from(add_garrison(h, unit, a, b)),
        EE_UNITS_CU_DO_KILL_CARGO => {
            kill_cargo(h, unit);
            0
        }
        EE_UNITS_CU_DO_RELEASE_CARGO => {
            release_cargo(h, unit, a as u64);
            0
        }
        EE_UNITS_CU_DO_REPLACE_CARGO => {
            replace_cargo(h, unit, a as u64, b as u64);
            0
        }
        EE_UNITS_CU_DO_UNLOAD_CARGO_AT => unload_cargo_at(h, unit, a, b as u64),
        EE_UNITS_CU_DO_UNLOAD_CARGO_AT_TILE => {
            unload_cargo_at_tile(h, unit, a, b, c != 0);
            0
        }
        EE_UNITS_CU_DO_UNLOAD_CARGO_NOW => i64::from(unload_cargo_now(h, unit, a as u64)),
        EE_UNITS_CU_DO_ORDER_REPAIR => {
            order_repair(h, unit, a as u64);
            0
        }
        EE_UNITS_CU_DO_ADD_BUILD_TARGET => {
            add_build_target(h, unit, a as u64);
            0
        }
        EE_UNITS_CU_DO_ADD_CONTACT => {
            add_contact(h, unit, a as u64);
            0
        }
        _ => return crate::units::cu_misc::run(h, what, unit, a, b),
    })
}

#[cfg(test)]
#[path = "cu_cargo_tests.rs"]
mod tests;
