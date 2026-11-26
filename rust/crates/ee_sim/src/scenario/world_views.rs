//! The world channel's reads the world views answer (game core lane, round 17): the effect run's, Create Units', the
//! class attributes', the unit change's and the run host's single-statement reads of a unit, its complex unit, class
//! and stats, a player, a class by index, the map's tiles and the world, which were cases of EESMEffect.cpp's
//! `ee_scenariocpp_world_*` hosts. entry.rs's `world` asks [`answer`] first; None hands the operation on to the C++
//! host world_route.rs names (its statements on C++ objects, the run's own state).
//!
//! Each answer is the C++'s `int64_t` conversion of its expression: a `long` sign-extended, an `unsigned long`
//! zero-extended, a `bool` 0 or 1, a `float` its bits ([`ee_scenario_f32_bits`]), a pointer its address. A unit by id
//! is `gWorld.GetUnit(id)` (the host's [`Lookups::unit`]); where the C++ would have read through a NULL pointer the
//! answer is 0.

// the records keep the C++ member names; c_long/c_ulong are 32 bits on Windows
#![allow(non_snake_case, clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::game_ids::EETerrainType::{kTerrainDeepWater, kTerrainShallowWater};
use crate::scenario::ca_compose::*;
use crate::scenario::effect::*;
use crate::scenario::effect_text::EE_SCENARIO_WY_UNIT_ID;
use crate::scenario::entry::EeScenarioHostContext;
use crate::scenario::fields::EeScenarioEffectFields;
use crate::scenario::unit_change::*;
use crate::scenario::world_compose::*;
use crate::scenario::world_route::ee_scenario_f32_bits as bits;
use crate::unit_ids::EEUnitActionID::kActionGatherResource;
use crate::unit_ids::EEUnitMovementFlag::{kMovementAir, kMovementSea};
use crate::world::view::{ClassView, PlayerView, StatsView, UnitView, WorldRead, WorldView};
use crate::world_field as get;

/// What the C++ host still looks up for the views: a unit by id, an effect's record.
pub trait Lookups {
    /// `gWorld.GetUnit((unsigned long)id)`'s handle (0: none).
    fn unit(&self, id: i64) -> u64;
    /// The effect `effect`'s record (`static_cast<EeScenarioEffectFields*>`).
    fn effect(&self, effect: *mut c_void) -> *const EeScenarioEffectFields;
}

/// `EEUnitClass::Is*()` (`EE_UNITS_UC_IS_*`) of the class `class`.
#[cfg(not(test))]
fn class_is(class: u64, which: i32) -> bool {
    crate::units::uc_is::ee_units_uc_is(class, which)
}
#[cfg(test)]
fn class_is(class: u64, which: i32) -> bool {
    class != 0 && which == crate::units::uc_is::EE_UNITS_UC_IS_TRANSPORT
}

/// `EEComplexUnit::GetMaxAngularVelocity()` of the complex unit `unit`.
#[cfg(not(test))]
fn max_angular_velocity(unit: u64) -> f32 {
    // SAFETY: a live complex unit of the running game, on the simulation thread
    unsafe {
        crate::units::pm_flight::ee_units_pm_unit_rate(
            unit as usize as *const c_void,
            crate::units::pm_flight::EE_UNITS_PM_RATE_MAX_ANGULAR_VELOCITY,
        )
    }
}
#[cfg(test)]
fn max_angular_velocity(unit: u64) -> f32 {
    (unit & 1) as f32
}

/// `SMEID(unit)`: its id as 32 bits, -1 for none.
fn id_or_none(u: Option<UnitView>) -> i64 {
    u.map_or(-1, |u| u.id() as i64)
}

/// `GetUnitType()` (a `long`).
fn unit_type(k: ClassView) -> i64 {
    k.unit_type() as c_long as i64
}

/// `(mDomains & flag) != 0`.
fn domains(k: ClassView, flag: u8) -> bool {
    get!(k.fields(), mDomains) & flag != 0
}

/// `gWorld.GetPlayerIndex(player)`: the last world slot below `mNumPlayers` holding it, -1 for none.
fn player_index(w: &WorldView, player: u64) -> i64 {
    let mut result = -1;
    for i in 0..w.num_players() as usize {
        if w.player(i).map_or(0, |p| p.handle()) == player {
            result = i as i64;
        }
    }
    result
}

/// `SMEUnitValue(id, field)` (`EE_SCENARIO_UV_*`) of the unit `u` (None: no such unit).
fn unit_value(w: &WorldView, u: Option<UnitView>, field: i64) -> i64 {
    let Some(u) = u else { return 0 };
    if field == EE_SCENARIO_UV_EXISTS {
        return 1;
    }
    let c = u.complex_unit();
    let k = u.class();
    let v = || -> Option<i64> {
        Some(match field {
            EE_SCENARIO_UV_COMPLEX => c.is_some() as i64,
            EE_SCENARIO_UV_HIT_POINTS => u.hit_points(),
            EE_SCENARIO_UV_DAMAGED => (u.hit_points() < u.stats()?.max_hit_points()) as i64,
            EE_SCENARIO_UV_TILE_X => u.tile_xy().0,
            EE_SCENARIO_UV_TILE_Y => u.tile_xy().1,
            EE_SCENARIO_UV_ON_TILE => u.tile().is_some() as i64,
            EE_SCENARIO_UV_PLAYER => u.player().map_or(-1, |p| p.id() as i64),
            EE_SCENARIO_UV_PLAYER_INDEX => player_index(w, u.player().map_or(0, |p| p.handle())),
            EE_SCENARIO_UV_TYPE => unit_type(k?),
            EE_SCENARIO_UV_CATEGORY => k?.category() as i64,
            EE_SCENARIO_UV_COMMAND => c?.command_type() as i64,
            EE_SCENARIO_UV_DISABLED => get!(c?.complex(), mIsDisabled) as i64,
            EE_SCENARIO_UV_COMMANDABLE => get!(c?.complex(), mIsCommandable) as i64,
            EE_SCENARIO_UV_CLASS_DELETABLE => get!(k?.fields(), mIsDeletable) as i64,
            EE_SCENARIO_UV_WALL => k?.wall() as i64,
            EE_SCENARIO_UV_HAS_GOAL => (c?.goal_handle() != 0) as i64,
            EE_SCENARIO_UV_TURNS => (max_angular_velocity(c?.handle()) > 0.0) as i64,
            EE_SCENARIO_UV_TREE => k?.tree() as i64,
            EE_SCENARIO_UV_ID => u.id() as i64,
            EE_SCENARIO_UV_CONTAINER => id_or_none(c?.container()),
            EE_SCENARIO_UV_CONTAINER_TYPE => unit_type(c?.container()?.class()?),
            EE_SCENARIO_UV_AIR => domains(k?, kMovementAir as u8) as i64,
            EE_SCENARIO_UV_LAND_BUILDING => k?.land_building() as i64,
            EE_SCENARIO_UV_TRANSPORT => class_is(k?.handle(), crate::units::uc_is::EE_UNITS_UC_IS_TRANSPORT) as i64,
            EE_SCENARIO_UV_DROP_SITE => class_is(k?.handle(), crate::units::uc_is::EE_UNITS_UC_IS_DROP_SITE) as i64,
            EE_SCENARIO_UV_GARRISON => get!(k?.fields(), mGarrisonCapacity) as i64,
            EE_SCENARIO_UV_DETACHED => get!(c?.complex(), mIsDetachedFromAI) as i64,
            EE_SCENARIO_UV_CARGO => c?.cargo_len() as i64,
            _ => 0,
        })
    };
    v().unwrap_or(0)
}

/// EEChangeUnit's read of field `field` (`EE_SCENARIO_CU_*`, the three unit variables after them) of the context's
/// unit `u` (`EE_SCENARIO_WORLD_CU_VALUE`).
fn change_unit_value(u: Option<UnitView>, field: i64) -> i64 {
    let Some(u) = u else { return 0 };
    let c = u.complex_unit();
    let k = u.class();
    let s = u.stats();
    let v = || -> Option<i64> {
        Some(match field {
            EE_SCENARIO_CU_EXISTS => 1,
            EE_SCENARIO_CU_COMPLEX => c.is_some() as i64,
            EE_SCENARIO_CU_HIT_POINTS => u.hit_points(),
            EE_SCENARIO_CU_MAX_HIT_POINTS => s?.max_hit_points(),
            EE_SCENARIO_CU_CATEGORY => k?.category() as i64,
            EE_SCENARIO_CU_CARRY => get!(s?.fields(), mCarryAmount) as i64,
            EE_SCENARIO_CU_RESOURCE_AMOUNT => u.resource_amount(),
            EE_SCENARIO_CU_GATHERING => (c?.command_type() == kActionGatherResource as u32) as i64,
            EE_SCENARIO_CU_RESOURCE_TYPE => c?.resource_type() as i64,
            EE_SCENARIO_CU_STATS_FLIGHT_TIME => get!(s?.fields(), mFlightTime) as i64,
            EE_SCENARIO_CU_FLIGHT_TIME => get!(c?.complex(), mFlightTime) as i64,
            EE_SCENARIO_CU_GARRISON_CAPACITY => get!(k?.fields(), mGarrisonCapacity) as i64,
            EE_SCENARIO_CU_GARRISON => get!(c?.complex(), mGarrisonCount) as i64,
            EE_SCENARIO_CU_CLASS_AMMUNITION => get!(k?.fields(), mAmmunition) as i64,
            EE_SCENARIO_CU_AMMUNITION => get!(c?.complex(), mAmmunition) as c_ulong as i64,
            EE_SCENARIO_CU_CLASS_SCALE => bits(get!(k?.fields(), mScale)),
            EE_SCENARIO_CU_SCALE => bits(c?.model_scale()),
            EE_SCENARIO_CU_STATS_RELOAD => get!(s?.fields(), mReloadTime) as i64,
            EE_SCENARIO_CU_RELOAD_TIMER => get!(c?.complex(), mReloadTimer) as i64,
            EE_SCENARIO_CU_STATS_POWER => get!(s?.fields(), mPower) as i64,
            EE_SCENARIO_CU_POWER => get!(c?.complex(), mPower) as i64,
            EE_SCENARIO_CU_CLASS_ROTTING => get!(k?.fields(), mRottingTime) as i64,
            EE_SCENARIO_CU_ROTTING => get!(c?.complex(), mRottingAwayTime) as i64,
            f if (EE_SCENARIO_CU_VARIABLE..EE_SCENARIO_CU_VARIABLE + 3).contains(&f) => {
                *c?.unit_variables().get((f - EE_SCENARIO_CU_VARIABLE) as usize)?
            }
            _ => 0,
        })
    };
    v().unwrap_or(0)
}

/// The answer to world operation `op` with the arguments `a` on the context `ctx` (an `EESMHostContext*`, an
/// `EESMEffectRun*` for the effect run's), or None for the C++ host's.
///
/// # Safety
/// Inside a call from C++ on the simulation thread: `ctx` the live context the operation names, the handles in `a`
/// live objects of the operation's kinds, `look` the running game's.
pub unsafe fn answer(ctx: *mut c_void, op: u32, a: &[i64; 6], look: &dyn Lookups) -> Option<i64> {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    // SAFETY (each view below): the handles are the operation's live objects and a unit by id is the world's
    // (the caller's contract); the views live no longer than the token
    let by = |handle: u64| unsafe { UnitView::of(&t, handle) };
    let unit = |id: i64| by(look.unit(id));
    let complex = |id: i64| unit(id).and_then(|u| u.complex_unit());
    let stats_of = |handle: i64| unsafe { StatsView::of(&t, handle as u64) };
    let class_of = |handle: i64| unsafe { ClassView::of(&t, handle as u64) };
    let player_of = |handle: i64| unsafe { PlayerView::of(&t, handle as u64) };
    let class_at = |i: i64| if i < 0 { None } else { ClassView::by_index(&t, i as usize) };
    let h = |v: Option<u64>| v.unwrap_or(0) as i64;
    let effect = || {
        // SAFETY: the effect run's context, whose effect and its record are live
        unsafe { look.effect((*(ctx as *const EeScenarioHostContext)).effect).as_ref() }
    };
    let b = |v: bool| Some(v as i64);
    if op == EE_SCENARIO_WORLD_CU_VALUE {
        // EEChangeUnit's reads of the context's unit (always answered here: the C++ host has only the writes)
        // SAFETY: the host context the operation names
        return Some(change_unit_value(by(unsafe { (*(ctx as *const EeScenarioHostContext)).unit } as u64), a[0]));
    }
    let w = WorldView::global(&t)?;
    let player = |i: i64| if i < 0 { None } else { w.player(i as usize) };
    let tile = |x: i64, y: i64| w.map().and_then(|m| m.tile(x as c_long as i64, y as c_long as i64));
    match op {
        // a class attribute's and Set AI Variable's reads (ca_compose.rs, ai_store.rs; on handles)
        EE_SCENARIO_WK_PLAYER => Some(h(player(a[0]).map(|p| p.handle()))),
        EE_SCENARIO_WK_UPGRADE_FIELDS => Some(h(stats_of(a[0]).map(|s| s.fields().ptr() as u64))),
        EE_SCENARIO_WK_CLASS => Some(h(stats_of(a[0]).map(|s| s.class_handle()))),
        EE_SCENARIO_WK_CLASS_FIELDS => Some(h(class_of(a[0]).map(|k| k.fields().ptr() as u64))),
        EE_SCENARIO_WK_PLAYER_ID => Some(h(player_of(a[0]).map(|p| p.id() as u64))),
        EE_SCENARIO_WK_FIRST_UNIT => Some(h(player_of(a[0]).and_then(|p| p.first_unit()).map(|u| u.handle()))),
        EE_SCENARIO_WK_NEXT_UNIT => Some(h(by(a[0] as u64).and_then(|u| u.next()).map(|u| u.handle()))),
        EE_SCENARIO_WK_UNIT_COMPLEX => b(by(a[0] as u64).is_some_and(|u| u.complex_unit_handle() != 0)),
        EE_SCENARIO_WK_UNIT_CLASS => Some(h(by(a[0] as u64).and_then(|u| u.class()).map(|k| k.handle()))),
        // the run host's (effect.rs)
        EE_SCENARIO_WORLD_UNIT => Some(unit_value(&w, unit(a[0]), a[1])),
        EE_SCENARIO_WORLD_CURRENT_PLAYER => Some(w.current_player_index()),
        EE_SCENARIO_WORLD_IN_CINEMATIC => b(w.in_cinematic()),
        EE_SCENARIO_WORLD_PLAYER_OBSERVER => b(player(a[0]).is_some_and(|p| p.observer())),
        // Create Units' unit's (world_compose.rs create_unit; on handles)
        EE_SCENARIO_WC_CLASS => Some(h(class_at(a[0]).map(|k| k.handle()))),
        EE_SCENARIO_WC_IS_LAND_BUILDING => b(class_of(a[0]).is_some_and(|k| k.land_building())),
        EE_SCENARIO_WC_ON_NODE => b(by(a[0] as u64).is_some_and(|u| u.has_tile_node())),
        EE_SCENARIO_WC_AIR => {
            b(by(a[0] as u64).and_then(|u| u.class()).is_some_and(|k| domains(k, kMovementAir as u8)))
        }
        EE_SCENARIO_WC_MAP_SIZE => Some(w.map().map_or(0, |m| m.size())),
        EE_SCENARIO_WC_COMPLEX_POWER => {
            Some(by(a[0] as u64).and_then(|u| u.complex_unit()?.stats()).map_or(0, |s| get!(s.fields(), mPower) as i64))
        }
        EE_SCENARIO_WC_SOUND_LOCAL => {
            b(by(a[0] as u64).and_then(|u| u.player()).is_some_and(|p| p.id() == w.current_player_index() as u32))
        }
        // the effect run's texts' (effect_text.rs)
        EE_SCENARIO_WY_UNIT_ID => Some(by(a[0] as u64).map_or(0, |u| u.id() as i64)),
        // the effect run's single statements' (world_compose.rs; units by id, players by index)
        EE_SCENARIO_WX_HAS_COMPLEX => b(complex(a[0]).is_some_and(|c| c.complex_unit_handle() != 0)),
        EE_SCENARIO_WX_GOAL_KIND => Some(complex(a[0]).and_then(|c| c.goal()).map_or(0, |g| g.0 as i64)),
        EE_SCENARIO_WX_LOCATION_AT => {
            let m = complex(a[0])?.transform();
            Some(bits(*m.get(a[1] as usize)?.get(a[2] as usize)?))
        }
        EE_SCENARIO_WX_TILE_EXISTS => b(tile(a[0], a[1]).is_some()),
        EE_SCENARIO_WX_TILE_HEIGHT | EE_SCENARIO_WX_TILE_GET_HEIGHT => Some(bits(tile(a[0], a[1])?.height())),
        EE_SCENARIO_WX_TILE_WATER => {
            let terrain = tile(a[0], a[1])?.terrain_type();
            b(terrain == kTerrainDeepWater as i64 || terrain == kTerrainShallowWater as i64)
        }
        EE_SCENARIO_WX_LOCKED => b(complex(a[0]).is_some_and(|c| get!(c.complex(), mIsLocked))),
        EE_SCENARIO_WX_PLAYER_IS_CURRENT => {
            b(h(player(a[0]).map(|p| p.handle())) == h(w.current_player().map(|p| p.handle())))
        }
        EE_SCENARIO_WX_UNIT_ON_TILE => b(unit(a[0]).is_some_and(|u| u.tile().is_some())),
        EE_SCENARIO_WX_UNIT_TILE_X => Some(unit(a[0]).map_or(0, |u| u.tile_xy().0)),
        EE_SCENARIO_WX_UNIT_TILE_Y => Some(unit(a[0]).map_or(0, |u| u.tile_xy().1)),
        EE_SCENARIO_WX_UNIT_HAS_COMPLEX => b(unit(a[0]).is_some_and(|u| u.complex_unit_handle() != 0)),
        EE_SCENARIO_WX_UNIT_IN_CONTAINER => b(complex(a[0]).is_some_and(|c| c.container_handle() != 0)),
        EE_SCENARIO_WX_CONTAINER_ON_TILE => {
            b(complex(a[0]).and_then(|c| c.container()).is_some_and(|k| k.tile().is_some()))
        }
        EE_SCENARIO_WX_CONTAINER_TILE_X => Some(complex(a[0]).and_then(|c| c.container()).map_or(0, |k| k.tile_xy().0)),
        EE_SCENARIO_WX_CONTAINER_TILE_Y => Some(complex(a[0]).and_then(|c| c.container()).map_or(0, |k| k.tile_xy().1)),
        EE_SCENARIO_WX_LAND_BUILDING => b(complex(a[0]).and_then(|c| c.class()).is_some_and(|k| k.land_building())),
        EE_SCENARIO_WX_AIRPORT => {
            let traffic = unit(a[0]).and_then(|u| u.class()).is_some_and(|k| get!(k.fields(), mAirTrafficType) != 0);
            let base = || {
                complex(a[0])
                    .and_then(|c| c.class())
                    .is_some_and(|k| class_is(k.handle(), crate::units::uc_is::EE_UNITS_UC_IS_AIR_BASE))
            };
            b(traffic && base())
        }
        EE_SCENARIO_WX_UNIT_AIR => {
            b(unit(a[0]).and_then(|u| u.class()).is_some_and(|k| domains(k, kMovementAir as u8)))
        }
        EE_SCENARIO_WX_UNIT_Z => Some(bits(complex(a[0])?.location()[2])),
        EE_SCENARIO_WX_CRUISE => Some(bits(get!(unit(a[0])?.class()?.fields(), mCruiseAltitude))),
        EE_SCENARIO_WX_PITCHES => {
            b(unit(a[0]).and_then(|u| u.class()).is_some_and(|k| get!(k.fields(), mPitchesWithTerrain)))
        }
        EE_SCENARIO_WX_ROLLS => {
            b(unit(a[0]).and_then(|u| u.class()).is_some_and(|k| get!(k.fields(), mRollsWithTerrain)))
        }
        EE_SCENARIO_WX_CONTAINER_NULL => b(complex(a[0]).is_some_and(|c| c.container_handle() == 0)),
        EE_SCENARIO_WX_TILE_SIZE => Some(unit(a[0]).and_then(|u| u.class()).map_or(0, |k| k.tile_size())),
        EE_SCENARIO_WX_CATEGORY => Some(unit(a[0]).and_then(|u| u.class()).map_or(0, |k| k.category() as i64)),
        EE_SCENARIO_WX_FLIGHT_TIME => Some(complex(a[0]).map_or(0, |c| get!(c.complex(), mFlightTime) as i64)),
        EE_SCENARIO_WX_HOME_BASE => b(complex(a[0]).is_some_and(|c| c.home_base_handle() != 0)),
        EE_SCENARIO_WX_EFFECT_TECH => Some(effect()?.mTechnology as i64),
        EE_SCENARIO_WX_EFFECT_TASK_AREA => Some(effect()?.mTaskArea as i64),
        EE_SCENARIO_WX_CLASS_COUNT => Some(ClassView::count(&t) as i64),
        EE_SCENARIO_WX_CLASS_EXISTS => b(class_at(a[0]).is_some()),
        EE_SCENARIO_WX_CLASS_AIR => b(class_at(a[0]).is_some_and(|k| domains(k, kMovementAir as u8))),
        EE_SCENARIO_WX_CLASS_SEA => b(class_at(a[0]).is_some_and(|k| domains(k, kMovementSea as u8))),
        EE_SCENARIO_WX_CLASS_PROJECTILE => b(class_at(a[0]).is_some_and(|k| get!(k.fields(), mIsProjectile))),
        EE_SCENARIO_WX_CLASS_LAND_BUILDING => b(class_at(a[0]).is_some_and(|k| k.land_building())),
        EE_SCENARIO_WX_CLASS_ATTACKS => b(class_at(a[0]).is_some_and(|k| get!(k.fields(), mAttack) > 0)),
        EE_SCENARIO_WX_CLASS_MOVES => b(class_at(a[0]).is_some_and(|k| get!(k.fields(), mMaxVelocity) > 0.0)),
        EE_SCENARIO_WX_CLASS_CATEGORY => Some(class_at(a[0]).map_or(0, |k| k.category() as i64)),
        _ => None,
    }
}

#[cfg(test)]
#[path = "world_views_tests.rs"]
mod tests;
