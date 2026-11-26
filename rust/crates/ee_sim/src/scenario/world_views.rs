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
use crate::scenario::area_fields::EeScenarioAreaFields;
use crate::scenario::ca_compose::*;
use crate::scenario::cinematic::*;
use crate::scenario::effect::*;
use crate::scenario::effect_text::*;
use crate::scenario::entry::EE_SCENARIO_EN_MESSAGE_FIELDS;
use crate::scenario::entry::{
    EeScenarioHostContext, EE_SCENARIO_EN_CONDITION_FIELDS, EE_SCENARIO_EN_OBJECT_FIELDS, EE_SCENARIO_EN_TRIGGER_FIELDS,
};
use crate::scenario::event::*;
use crate::scenario::fields::{
    EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioMessageFields, EeScenarioObjectFields,
    EeScenarioTriggerFields,
};
use crate::scenario::player::*;

use crate::scenario::host::*;
use crate::scenario::object_reads::*;
use crate::scenario::objeval::*;
use crate::scenario::placement::*;
use crate::scenario::read_compose::*;
use crate::scenario::round::*;
use crate::scenario::unit_change::*;
use crate::scenario::world_compose::*;
use crate::scenario::world_route::ee_scenario_f32_bits as bits;
use crate::unit_ids::EEUnitActionID::kActionGatherResource;
use crate::unit_ids::EEUnitMovementFlag::{kMovementAir, kMovementSea};
use crate::world::view::{ClassView, PlayerView, Record, StatsView, TileView, UnitView, WorldRead, WorldView};
use crate::world::NodeView;
use crate::world_field as get;

/// `record.member = value` through a view's record (the C++'s assignment), answering the value.
macro_rules! put {
    ($record:expr, $member:ident, $value:expr) => {{
        let value = $value;
        // SAFETY: a view's record is a live object's (the views' contract); the simulation thread writes it inside
        // the C++ call, as the C++ statement did
        unsafe { (*record_mut($record)).$member = value };
        value
    }};
}

/// A view's record as the C++ object's member it is, for [`put!`].
fn record_mut<T>(record: Record<T>) -> *mut T {
    record.ptr() as *mut T
}

/// What the C++ host still looks up for the views: a unit by id, an effect's record.
pub trait Lookups {
    /// `gWorld.GetUnit((unsigned long)id)`'s handle (0: none).
    fn unit(&self, id: i64) -> u64;
    /// The effect `effect`'s record (`static_cast<EeScenarioEffectFields*>`).
    fn effect(&self, effect: *mut c_void) -> *const EeScenarioEffectFields;
    /// The record entry.rs's `EE_SCENARIO_EN_*_FIELDS` operation `op` names of the element `element`.
    fn fields(&self, op: u32, element: *mut c_void) -> *const c_void;
    /// `gWorld.mScenarioManager->GetArea(index)->GetCenterTile()`, packed (x low, y high).
    fn area_center(&self, index: i64) -> i64;
    /// `GetArea(index)`'s record (null: none).
    fn area(&self, index: i64) -> *const EeScenarioAreaFields;
    /// `GetScenarioObject(index)`'s record (null: none).
    fn object(&self, index: i64) -> *const EeScenarioObjectFields;
    /// `GetTrigger(index)`'s record (null: none).
    fn trigger(&self, index: i64) -> *const EeScenarioTriggerFields;
}

/// `EEEpochCount()`.
fn epoch_count() -> i64 {
    crate::tech::tech_ids::ee_tech_epoch_count() as i64
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
    if op == EE_SCENARIO_WORLD_CU_SET {
        // EEChangeUnit's plain writes of the context's complex unit (the others are the C++ host's)
        // SAFETY: the host context the operation names
        let c = by(unsafe { (*(ctx as *const EeScenarioHostContext)).unit } as u64).and_then(|u| u.complex_unit());
        let r = || c.map(|c| c.complex());
        match a[0] {
            EE_SCENARIO_CU_AMMUNITION => {
                put!(r()?, mAmmunition, a[1] as c_ulong);
            }
            EE_SCENARIO_CU_RELOAD_TIMER => {
                put!(r()?, mReloadTimer, a[1] as c_long);
            }
            EE_SCENARIO_CU_POWER => {
                put!(r()?, mPower, a[1] as c_long);
            }
            EE_SCENARIO_CU_ROTTING => {
                put!(r()?, mRottingAwayTime, a[1] as c_long);
            }
            _ => return None,
        }
        return Some(0);
    }
    if crate::scenario::run_state::is_op(op) {
        // the effect run's own values (run_state.rs), its record right after the host context (EESMEffect.cpp
        // asserts it)
        // SAFETY: the operation is an effect run's: `ctx` is the run
        let state = unsafe {
            &mut *((ctx as usize + std::mem::size_of::<EeScenarioHostContext>())
                as *mut crate::scenario::run_state::EeScenarioRunState)
        };
        let mut location = |o: u32, id: i64| {
            let u = if o == EE_SCENARIO_WC_T_FROM { by(id as u64) } else { unit(id) };
            Some(ee_base::math::types::GeTransformation { m: u?.complex_unit()?.transform() })
        };
        return crate::scenario::run_state::op(state, op, a, &mut location);
    }
    let w = WorldView::global(&t)?;
    let node_of = |handle: i64| unsafe { NodeView::of(&t, handle as u64) };
    let tile_of = |handle: i64| unsafe { TileView::of(&t, handle as u64) };
    // an element's record by its entry operation (None for none)
    let record = |op: u32, element: i64| -> *const c_void { look.fields(op, element as usize as *mut c_void) };
    // SAFETY: the records are the live elements' (the handles' contract)
    let trigger =
        |e: i64| unsafe { (record(EE_SCENARIO_EN_TRIGGER_FIELDS, e) as *const EeScenarioTriggerFields).as_ref() };
    let condition =
        |e: i64| unsafe { (record(EE_SCENARIO_EN_CONDITION_FIELDS, e) as *const EeScenarioConditionFields).as_ref() };
    let object =
        |e: i64| unsafe { (record(EE_SCENARIO_EN_OBJECT_FIELDS, e) as *const EeScenarioObjectFields).as_ref() };
    // the context's unit (an `EESMHostContext`'s `mUnit`) and its complex unit
    // SAFETY: the operation's context
    let context = || unsafe { &mut *(ctx as *mut EeScenarioHostContext) };
    let context_complex = || by(context().unit as u64).and_then(|u| u.complex_unit());
    let player = |i: i64| if i < 0 { None } else { w.player(i as usize) };
    let tile = |x: i64, y: i64| w.map().and_then(|m| m.tile(x as c_long as i64, y as c_long as i64));
    // the effect's scenario object's record, an area's
    // SAFETY (each record): the manager's live element
    let effect_object = || unsafe { (look.object(effect()?.mObject as i64) as *mut EeScenarioObjectFields).as_mut() };
    let area = |i: i64| unsafe { look.area(i).as_ref() };
    let message =
        |m: i64| unsafe { (record(EE_SCENARIO_EN_MESSAGE_FIELDS, m) as *mut EeScenarioMessageFields).as_mut() };
    let trigger_at = |i: i64| unsafe { (look.trigger(i) as *mut EeScenarioTriggerFields).as_mut() };
    // the effect's technology's node in player slot `i`'s tree
    let effect_node = |i: i64| player(i)?.tech_tree()?.node(effect()?.mTechnology as i64);
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
        EE_SCENARIO_WORLD_AREA_CENTER => Some(look.area_center(a[0])),
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
        EE_SCENARIO_WC_TECH_NODE => Some(h(w
            .player(a[0] as c_ulong as usize)
            .and_then(|p| p.tech_tree()?.node(class_of(a[1])?.technology()))
            .map(|n| n.handle()))),
        EE_SCENARIO_WC_TECH_KIND => Some(
            w.player(a[0] as c_ulong as usize)
                .and_then(|p| p.tech_tree()?.node(class_of(a[1])?.technology())?.data())
                .map_or(0, |d| get!(d, mKind) as i64),
        ),
        EE_SCENARIO_WC_ATTRIBUTE_CHECK => {
            // SAFETY: the run's object, live
            let o = unsafe {
                (record(EE_SCENARIO_EN_OBJECT_FIELDS, context().object as i64) as *const EeScenarioObjectFields)
                    .as_ref()
            };
            Some(o?.mHasAttributeCheck as i64)
        }
        EE_SCENARIO_WK_EFFECT_FIELDS => Some(look.effect(a[0] as usize as *mut c_void) as i64),
        EE_SCENARIO_WY_EFFECT_FIELDS => Some(look.effect(context().effect) as i64),
        EE_SCENARIO_WY_MSG_DURATION => {
            let m = message(a[0])?;
            m.mDuration = a[1] as c_ulong;
            Some(m.mDuration as i64)
        }
        EE_SCENARIO_WY_MSG_CINEMATIC => {
            let m = message(a[0])?;
            m.mIsCinematic = a[1] != 0;
            b(m.mIsCinematic)
        }
        EE_SCENARIO_WY_MSG_UNIT_TYPE => {
            let m = message(a[0])?;
            m.mUnitType = a[1] as c_ulong;
            Some(m.mUnitType as i64)
        }
        EE_SCENARIO_WY_MSG_SCROLL => {
            let m = message(a[0])?;
            m.mScrollSpeed = a[1] as c_long;
            Some(m.mScrollSpeed as i64)
        }
        EE_SCENARIO_WY_MSG_UNIT_ID => {
            let m = message(a[0])?;
            m.mUnitID = a[1] as c_ulong;
            Some(m.mUnitID as i64)
        }
        EE_SCENARIO_WORLD_TIME_OF_DAY_RUNS => {
            put!(w.physics()?, mTimeOfDayRuns, a[0] != 0);
            Some(0)
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
        EE_SCENARIO_WX_TECH_MAKES_UNIT => {
            b(effect_node(a[0]).and_then(|n| n.data()).is_some_and(|d| get!(d, mMakesUnit)))
        }
        EE_SCENARIO_WX_TECH_CLASS_EXISTS => {
            b(effect_node(a[0]).and_then(|n| n.data()).is_some_and(|d| class_at(get!(d, mUnitClass) as i64).is_some()))
        }
        EE_SCENARIO_WX_TECH_KIND => Some(effect_node(a[0]).and_then(|n| n.data()).map_or(0, |d| get!(d, mKind) as i64)),
        EE_SCENARIO_WX_TREE_END_EPOCH => {
            Some(player(a[0]).and_then(|p| p.tech_tree()).map_or(0, |k| get!(k.fields(), mEndEpoch) as i64))
        }
        EE_SCENARIO_WX_EPOCH_COUNT => Some(epoch_count()),
        EE_SCENARIO_WX_TECH_EPOCH_ZERO => Some(put!(effect_node(a[0])?.fields(), mEpoch, 0) as i64),
        EE_SCENARIO_WX_TECH_OBSOLETE_EPOCH => {
            Some(put!(effect_node(a[0])?.fields(), mObsoleteEpoch, epoch_count() as c_long) as i64)
        }
        EE_SCENARIO_WX_TECH_MEDIA_LOADED => b(put!(effect_node(a[0])?.fields(), mIsUnitMediaLoaded, true)),
        EE_SCENARIO_WX_TREE_WONDERS_ZERO => {
            Some(put!(player(a[0])?.tech_tree()?.fields(), mEpochsBeforeWonders, 0) as i64)
        }
        EE_SCENARIO_WX_SET_LOCKED => b(put!(complex(a[0])?.complex(), mIsLocked, a[1] != 0)),
        EE_SCENARIO_WX_SET_SEES_ALL => b(put!(player(a[0])?.rts(), mSeesAll, a[1] != 0)),
        EE_SCENARIO_WX_TRIGGER_START => {
            let r = trigger_at(a[0])?;
            r.mConditionStartTime = a[1] as c_ulong;
            Some(r.mConditionStartTime as i64)
        }
        EE_SCENARIO_WX_TRIGGER_ON => {
            let r = trigger_at(a[0])?;
            r.mIsOn = a[1] != 0;
            b(r.mIsOn)
        }
        EE_SCENARIO_WX_TRIGGER_FIRED => {
            let r = trigger_at(a[0])?;
            r.mHasFired = a[1] != 0;
            b(r.mHasFired)
        }
        EE_SCENARIO_WX_OBJECT_EXISTS => b(!look.object(a[0]).is_null()),
        EE_SCENARIO_WX_OBJ_IN_AREA_CHECK => Some(effect_object()?.mInAreaCheck as i64),
        EE_SCENARIO_WX_OBJ_AREA => Some(effect_object()?.mArea as i64),
        EE_SCENARIO_WX_OBJ_NEAR_CHECK => Some(effect_object()?.mNearCheck as i64),
        EE_SCENARIO_WX_OBJ_SET_IN_AREA_CHECK => {
            let o = effect_object()?;
            o.mInAreaCheck = a[0] as c_ulong;
            Some(o.mInAreaCheck as i64)
        }
        EE_SCENARIO_WX_OBJ_SET_AREA => {
            let o = effect_object()?;
            o.mArea = a[0] as c_ulong;
            Some(o.mArea as i64)
        }
        EE_SCENARIO_WX_OBJ_SET_NEAR_CHECK => {
            let o = effect_object()?;
            o.mNearCheck = a[0] as c_ulong;
            Some(o.mNearCheck as i64)
        }
        EE_SCENARIO_WX_AREA_EXISTS => b(area(a[0]).is_some()),
        // SAFETY (the three): an area's live record
        EE_SCENARIO_WX_AREA_UNSET => b(unsafe { crate::scenario::entry::ee_scenario_area_is_unset(area(a[0])?) }),
        EE_SCENARIO_WX_AREA_OFF_MAP => b(unsafe { crate::scenario::entry::ee_scenario_area_is_off_map(area(a[0])?) }),
        EE_SCENARIO_WX_TASK_AREA_OFF_MAP => {
            b(unsafe { crate::scenario::entry::ee_scenario_area_is_off_map(area(effect()?.mTaskArea as i64)?) })
        }
        EE_SCENARIO_WX_CLASS_COUNT => Some(ClassView::count(&t) as i64),
        EE_SCENARIO_WX_CLASS_EXISTS => b(class_at(a[0]).is_some()),
        EE_SCENARIO_WX_CLASS_AIR => b(class_at(a[0]).is_some_and(|k| domains(k, kMovementAir as u8))),
        EE_SCENARIO_WX_CLASS_SEA => b(class_at(a[0]).is_some_and(|k| domains(k, kMovementSea as u8))),
        EE_SCENARIO_WX_CLASS_PROJECTILE => b(class_at(a[0]).is_some_and(|k| get!(k.fields(), mIsProjectile))),
        EE_SCENARIO_WX_CLASS_LAND_BUILDING => b(class_at(a[0]).is_some_and(|k| k.land_building())),
        EE_SCENARIO_WX_CLASS_ATTACKS => b(class_at(a[0]).is_some_and(|k| get!(k.fields(), mAttack) > 0)),
        EE_SCENARIO_WX_CLASS_MOVES => b(class_at(a[0]).is_some_and(|k| get!(k.fields(), mMaxVelocity) > 0.0)),
        EE_SCENARIO_WX_CLASS_CATEGORY => Some(class_at(a[0]).map_or(0, |k| k.category() as i64)),
        // the host's reads' (read_compose.rs; on handles)
        EE_SCENARIO_RX_TECH_NODE => {
            Some(h(player(a[0]).and_then(|p| p.tech_tree()?.node(a[1] as c_long as i64)).map(|n| n.handle())))
        }
        EE_SCENARIO_RX_NODE_MAKES_UNIT => b(node_of(a[0]).and_then(|n| n.data()).is_some_and(|d| get!(d, mMakesUnit))),
        EE_SCENARIO_RX_NODE_RESEARCHED => b(node_of(a[0]).is_some_and(|n| n.researched())),
        EE_SCENARIO_RX_NODE_SET_RESEARCHED => b(put!(node_of(a[0])?.fields(), mIsResearched, a[1] != 0)),
        EE_SCENARIO_RX_OBJECT_HAS_ATTRIBUTE_CHECK => Some(object(a[0]).map_or(0, |o| o.mHasAttributeCheck as i64)),
        EE_SCENARIO_RX_UNIT => Some(look.unit(a[0]) as i64),
        EE_SCENARIO_RX_UNIT_TYPE => Some(by(a[0] as u64).and_then(|u| u.class()).map_or(0, unit_type)),
        EE_SCENARIO_RX_TRIGGER_FIRED => b(trigger(a[0]).is_some_and(|r| r.mHasFired)),
        EE_SCENARIO_RX_TRIGGER_ON => b(trigger(a[0]).is_some_and(|r| r.mIsOn)),
        EE_SCENARIO_RX_CONDITION_TYPE => Some(condition(a[0]).map_or(0, |r| r.mType as i64)),
        EE_SCENARIO_RX_CONDITION_PLAYER => Some(condition(a[0]).map_or(0, |r| r.mPlayer as i64)),
        EE_SCENARIO_RX_CONDITION_ATTRIBUTE => Some(condition(a[0]).map_or(0, |r| r.mAttribute as i64)),
        // the object reads' (object_reads.rs; units by 32-bit id or handle)
        EE_SCENARIO_WZ_UNIT => Some(look.unit(a[0] as i32 as u32 as i64) as i64),
        EE_SCENARIO_WZ_UNIT_HIT_POINTS => Some(by(a[0] as u64).map_or(0, |u| u.hit_points())),
        EE_SCENARIO_WZ_UNIT_IS_TREE => b(by(a[0] as u64).and_then(|u| u.class()).is_some_and(|k| k.tree())),
        EE_SCENARIO_WZ_UNIT_COMPLEX => b(by(a[0] as u64).is_some_and(|u| u.complex_unit_handle() != 0)),
        EE_SCENARIO_WZ_UNIT_CARRIED => {
            b(by(a[0] as u64).and_then(|u| u.complex_unit()).is_some_and(|c| c.previous_part_handle() != 0))
        }
        EE_SCENARIO_WZ_PLAYER_SLOT => Some(h(player(a[0]).map(|p| p.handle()))),
        EE_SCENARIO_WZ_VIEWED_UNIT => b(player_of(a[0]).is_some_and(|p| p.viewed_unit_handle() == a[1] as u64)),
        EE_SCENARIO_WZ_FIRST_UNIT => Some(h(player_of(a[0]).and_then(|p| p.first_unit()).map(|u| u.handle()))),
        EE_SCENARIO_WZ_NEXT_UNIT => Some(h(by(a[0] as u64).and_then(|u| u.next()).map(|u| u.handle()))),
        EE_SCENARIO_WZ_CURSOR => Some(context().cursor as i64),
        EE_SCENARIO_WZ_SET_CURSOR => {
            context().cursor = a[0] as usize as *mut c_void;
            Some(0)
        }
        EE_SCENARIO_WZ_CHAIN_UNIT => Some(
            by(a[0] as u64)
                .map_or(-1, |u| u.id() as i64 | ((u.class().map_or(0, |k| k.unit_type()) as u32 as i64) << 32)),
        ),
        EE_SCENARIO_WZ_TILE => Some(h(tile(a[0], a[1]).map(|k| k.handle()))),
        EE_SCENARIO_WZ_TILE_TERRAIN => Some(tile_of(a[0]).map_or(0, |k| k.terrain_type())),
        EE_SCENARIO_WZ_UNIT_VARIABLE => {
            Some(context_complex().and_then(|c| c.unit_variables().get(a[0] as usize).copied()).unwrap_or(0))
        }
        EE_SCENARIO_WZ_UNIT_SCALE => Some(context_complex().map_or(0, |c| bits(c.model_scale()))),
        _ => None,
    }
}

/// The answer to the host's read `op` (`EE_SCENARIO_READ_*`; the conditions', objects' and placements' reads that
/// the C++ host answered from the world: EEScenarioHost.cpp's and EESMObject.cpp's cases), or None for the C++
/// host's. `unit` is `gWorld.GetUnit(id)`'s handle, `object` the scenario object `GetScenarioObject(i)`'s record.
///
/// # Safety
/// As [`answer`].
pub unsafe fn read(
    op: u32,
    a: i32,
    b: i32,
    unit: &mut dyn FnMut(i64) -> u64,
    object: &dyn Fn(i32) -> *const EeScenarioObjectFields,
) -> Option<i64> {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let w = WorldView::global(&t)?;
    let pack = |x: i64, y: i64| crate::scenario::world_route::ee_scenario_pack_xy(x as c_long, y as c_long);
    // the world's player slot a (none outside the array, as no original index reached)
    let slot = || if a < 0 { None } else { w.player(a as usize) };
    // SAFETY: a scenario object's record lives with the manager
    let obj = || unsafe { object(a).as_ref() };
    let fl = |v: bool| Some(v as i64);
    match op {
        EE_SCENARIO_READ_PLAYER_COMPUTER => return fl(slot().is_some_and(|p| p.has_computer_player())),
        EE_SCENARIO_READ_EPOCH_COUNT => return Some(epoch_count()),
        EE_SCENARIO_READ_EPOCH => return Some(slot().and_then(|p| p.tech_tree()).map_or(0, |k| k.current_epoch())),
        EE_SCENARIO_READ_TOWN_COUNT => return Some(slot().map_or(0, |p| p.towns_len() as i64)),
        EE_SCENARIO_READ_OBJECT_EXISTS => return fl(obj().is_some()),
        EE_SCENARIO_READ_OBJECT_SELECT_ON_MAP => return fl(obj().is_some_and(|o| o.mIsSelectOnMap)),
        EE_SCENARIO_READ_OBJECT_SELECTED_BY_CHECK => return Some(obj().map_or(0, |o| o.mSelectedByCheck as i64)),
        EE_SCENARIO_READ_MAP_SIZE => {
            let m = w.map()?;
            return Some(pack(get!(m.rts(), mL195610) as i64, get!(m.rts(), mL195614) as i64));
        }
        EE_SCENARIO_READ_CLASS_TILE_SIZE => {
            return Some(if a < 0 { None } else { ClassView::by_index(&t, a as usize) }.map_or(0, |k| k.tile_size()))
        }
        EE_SCENARIO_READ_MAP_TILE_EXISTS => {
            return fl(w.map().and_then(|m| m.tile(a as c_ulong as i64, b as c_ulong as i64)).is_some())
        }
        _ => {}
    }
    // the reads of the unit of 32-bit id a
    let id = a as u32 as i64;
    let of_unit = |u: UnitView| -> Option<i64> {
        Some(match op {
            EE_SCENARIO_READ_UNIT_CLASS_TYPE => u.class()?.unit_type() as i64,
            EE_SCENARIO_READ_UNIT_PLAYER => u.player()?.id() as i64,
            EE_SCENARIO_READ_UNIT_COMMAND => u.complex_unit()?.command_type() as i64,
            EE_SCENARIO_READ_UNIT_TILE_OF => pack(u.tile_xy().0, u.tile_xy().1),
            EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT => {
                let c = u.complex_unit()?;
                bits(get!(c.stats()?.fields(), mLineOfSight) + c.sight())
            }
            EE_SCENARIO_READ_UNIT_RANGE => bits(u.stats()?.range()),
            EE_SCENARIO_READ_UNIT_TILE_SIZE => u.class()?.tile_size(),
            _ => return None,
        })
    };
    match op {
        EE_SCENARIO_READ_UNIT_CLASS_TYPE
        | EE_SCENARIO_READ_UNIT_PLAYER
        | EE_SCENARIO_READ_UNIT_COMMAND
        | EE_SCENARIO_READ_UNIT_TILE_OF
        | EE_SCENARIO_READ_UNIT_LINE_OF_SIGHT
        | EE_SCENARIO_READ_UNIT_RANGE
        | EE_SCENARIO_READ_UNIT_TILE_SIZE => {
            // SAFETY: a unit by id is the world's
            Some(unsafe { UnitView::of(&t, unit(id)) }.and_then(of_unit).unwrap_or(0))
        }
        _ => None,
    }
}

/// `gWorld.mInCinematic` (false before the world is registered).
pub fn in_cinematic() -> bool {
    // SAFETY: a read of the world's flag on the simulation thread (the trigger's statement's)
    let t = unsafe { WorldRead::begin() };
    WorldView::global(&t).is_some_and(|w| w.in_cinematic())
}

/// The host's write `op` (`EE_SCENARIO_WRITE_*`, player.rs) of player slot `player` the records take (the player's
/// plain members: EEScenarioHost.cpp's cases); false for the C++ host's.
///
/// # Safety
/// As [`answer`].
pub unsafe fn write(op: u32, player: i64, b: i64, c: i64) -> bool {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let Some(w) = WorldView::global(&t) else { return false };
    let Some(p) = (if player < 0 { None } else { w.player(player as usize) }) else { return false };
    // SAFETY: the player's records are live (the views' contract), written on the simulation thread
    let saved = || unsafe { &mut *(p.saved().ptr() as *mut crate::world::player_fields::EeUnitsPlayerSaved) };
    let fields = || unsafe { &mut *(p.fields().ptr() as *mut crate::world::player_fields::EeUnitsPlayerFields) };
    match op {
        EE_SCENARIO_WRITE_RESOURCE => saved().mResourceCaps[b as usize] = c as c_long,
        EE_SCENARIO_WRITE_SHARE_BONUS => fields().mShareBonus = b as c_long,
        EE_SCENARIO_WRITE_VARIABLE => saved().mScenarioVariables[b as usize] = c as c_ulong,
        EE_SCENARIO_WRITE_TRIBUTE => saved().mTributes[b as usize] = c as c_ulong,
        EE_SCENARIO_WRITE_MINING_LIMIT => fields().mMiningLimit = b as c_long,
        _ => return false,
    }
    true
}

/// The answer to the events' statement `op` (`EE_SCENARIO_EV_*`, event.rs; pointers as integers) that reads the
/// world (EEEvent.cpp's cases), or None for the C++ host's.
///
/// # Safety
/// As [`answer`]: the handles in `a` the operation's live objects.
pub unsafe fn event(op: u32, a: &[i64; 4]) -> Option<i64> {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let w = WorldView::global(&t)?;
    // SAFETY (each view): the operation's live objects
    let by = |handle: i64| unsafe { UnitView::of(&t, handle as u64) };
    let player_of = |handle: i64| unsafe { PlayerView::of(&t, handle as u64) };
    let stats_of = |handle: i64| unsafe { StatsView::of(&t, handle as u64) };
    let class_of = |handle: i64| unsafe { ClassView::of(&t, handle as u64) };
    let node_of = |handle: i64| unsafe { NodeView::of(&t, handle as u64) };
    let h = |v: Option<u64>| v.unwrap_or(0) as i64;
    let complex = |handle: i64| by(handle).filter(|u| u.is_complex());
    Some(match op {
        EE_SCENARIO_EV_LOADING => w.loading_saved_game() as i64,
        EE_SCENARIO_EV_PLAYER => h(if a[0] < 0 { None } else { w.player(a[0] as usize) }.map(|p| p.handle())),
        EE_SCENARIO_EV_CURRENT_PLAYER => h(w.current_player().map(|p| p.handle())),
        EE_SCENARIO_EV_PLAYER_ID => player_of(a[0]).map_or(0, |p| p.id() as i64),
        EE_SCENARIO_EV_PLAYER_FIELDS => h(player_of(a[0]).map(|p| p.fields().ptr() as u64)),
        EE_SCENARIO_EV_CLASS => {
            h(if a[0] < 0 { None } else { ClassView::by_index(&t, a[0] as usize) }.map(|k| k.handle()))
        }
        EE_SCENARIO_EV_CLASS_FIELDS => h(class_of(a[0]).map(|k| k.fields().ptr() as u64)),
        EE_SCENARIO_EV_CLASS_COUNT => ClassView::count(&t) as i64,
        EE_SCENARIO_EV_UPGRADE_FIELDS => h(stats_of(a[0]).map(|s| s.fields().ptr() as u64)),
        EE_SCENARIO_EV_UPGRADE_CLASS => h(stats_of(a[0]).map(|s| s.class_handle())),
        EE_SCENARIO_EV_FIRST_UNIT => h(player_of(a[0]).and_then(|p| p.first_unit()).map(|u| u.handle())),
        EE_SCENARIO_EV_NEXT_UNIT => h(by(a[0]).and_then(|u| u.next()).map(|u| u.handle())),
        EE_SCENARIO_EV_PENDING_COUNT => player_of(a[0]).map_or(0, |p| p.pending_units_len() as i64),
        EE_SCENARIO_EV_PENDING_UNIT => h(player_of(a[0]).map(|p| p.pending_unit_handle(a[1] as c_ulong as usize))),
        EE_SCENARIO_EV_UNIT_TYPE => by(a[0]).and_then(|u| u.class()).map_or(0, |k| k.unit_type() as i64),
        EE_SCENARIO_EV_UNIT_HIT_POINTS => by(a[0]).map_or(0, |u| u.hit_points()),
        EE_SCENARIO_EV_UNIT_COMPLEX | EE_SCENARIO_EV_GET_COMPLEX => h(by(a[0]).map(|u| u.complex_unit_handle())),
        EE_SCENARIO_EV_COMPLEX_FLIGHT => complex(a[0]).map_or(0, |c| get!(c.complex(), mFlightTime) as i64),
        EE_SCENARIO_EV_COMPLEX_BUILT => complex(a[0]).is_some_and(|c| c.is_built()) as i64,
        EE_SCENARIO_EV_UNIT_ON_TILE => by(a[0]).is_some_and(|u| u.has_tile_node()) as i64,
        EE_SCENARIO_EV_COMPLEX_CARRIED => complex(a[0]).is_some_and(|c| c.previous_part_handle() != 0) as i64,
        EE_SCENARIO_EV_UNIT_ID => by(a[0]).map_or(0, |u| u.id() as i64),
        EE_SCENARIO_EV_COMPLEX_CLASS | EE_SCENARIO_EV_UNIT_CLASS => {
            h(by(a[0]).and_then(|u| u.class()).map(|k| k.handle()))
        }
        EE_SCENARIO_EV_COMPLEX_CLASS_TYPE => by(a[0]).and_then(|u| u.class()).map_or(0, |k| k.unit_type() as i64),
        EE_SCENARIO_EV_COMPLEX_AS_UNIT => a[0],
        EE_SCENARIO_EV_TREE_NODE => h(player_of(a[0]).and_then(|p| p.tech_tree()?.node(a[1])).map(|n| n.handle())),
        EE_SCENARIO_EV_NODE_UNIT_CLASS => {
            node_of(a[0]).and_then(|n| n.data()).map_or(0, |d| get!(d, mUnitClass) as i64)
        }
        EE_SCENARIO_EV_NODE_KIND => node_of(a[0]).and_then(|n| n.data()).map_or(0, |d| get!(d, mKind) as i64),
        EE_SCENARIO_EV_NODE_DELAY => {
            let n = node_of(a[0])?.fields();
            let delay: c_long = get!(n, mResearchDelay);
            put!(n, mResearchDelay, delay.wrapping_add(1));
            delay as i64
        }
        EE_SCENARIO_EV_TREE_SET_MEDIA => put!(player_of(a[0])?.tech_tree()?.fields(), mMediaID, a[1] as c_long) as i64,
        _ => return None,
    })
}

/// The answer to the scenario capture's read `op` (`EE_SCENARIO_CAP_*`, sm_capture.rs; `p` an object's address)
/// that reads the world or an element's record (EEScenarioCapture.cpp's cases), or None for the C++ host's.
/// `record(op, element)` is entry.rs's `EE_SCENARIO_EN_*_FIELDS` operation `op` on `element`.
///
/// # Safety
/// As [`answer`]: `p` the operation's live object.
pub unsafe fn capture(op: u32, p: u64, a: i64, record: &dyn Fn(u32, u64) -> u64) -> Option<i64> {
    use crate::scenario::area_fields::EeScenarioAreaFields;
    use crate::scenario::entry::{
        EE_SCENARIO_EN_AREA_FIELDS, EE_SCENARIO_EN_EFFECT_FIELDS, EE_SCENARIO_EN_MANAGER_FIELDS,
    };
    use crate::scenario::sm_capture::*;
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let w = WorldView::global(&t)?;
    // SAFETY (each view): the operation's live object
    let pl = || unsafe { PlayerView::of(&t, p) };
    let u = || unsafe { UnitView::of(&t, p) };
    let c = || u().filter(|u| u.is_complex());
    // SAFETY: an area's record lives with it
    let area = || unsafe { (record(EE_SCENARIO_EN_AREA_FIELDS, p) as *const EeScenarioAreaFields).as_ref() };
    let at = |v: &[c_ulong]| v.get(a as usize).map_or(0, |&x| x as i64);
    Some(match op {
        EE_SCENARIO_CAP_IN_CINEMATIC => w.in_cinematic() as i64,
        EE_SCENARIO_CAP_NUM_PLAYERS => w.num_players() as i64,
        EE_SCENARIO_CAP_PLAYER => w.player(a as usize).map_or(0, |p| p.handle() as i64),
        EE_SCENARIO_CAP_PLAYER_RESULT => pl().map_or(0, |p| get!(p.saved(), mGameResult) as i64),
        EE_SCENARIO_CAP_PLAYER_SEES_ALL => pl().is_some_and(|p| p.sees_all()) as i64,
        EE_SCENARIO_CAP_PLAYER_OBSERVER => pl().is_some_and(|p| p.observer()) as i64,
        EE_SCENARIO_CAP_PLAYER_ENEMY => pl().map_or(0, |p| p.enemy(a as usize) as i64),
        EE_SCENARIO_CAP_PLAYER_RESOURCE => {
            pl().map_or(0, |p| get!(p.saved(), mResourceCaps).get(a as usize).map_or(0, |&x| x as i64))
        }
        EE_SCENARIO_CAP_PLAYER_SHARE_BONUS => pl().map_or(0, |p| get!(p.fields(), mShareBonus) as i64),
        EE_SCENARIO_CAP_PLAYER_VARIABLE => pl().map_or(0, |p| at(&get!(p.saved(), mScenarioVariables))),
        EE_SCENARIO_CAP_PLAYER_TRIBUTE => pl().map_or(0, |p| at(&get!(p.saved(), mTributes))),
        EE_SCENARIO_CAP_PLAYER_MINING_LIMIT => pl().map_or(0, |p| get!(p.fields(), mMiningLimit) as i64),
        EE_SCENARIO_CAP_PLAYER_ID => pl().map_or(0, |p| p.id() as i64),
        EE_SCENARIO_CAP_UNIT_PLAYER => u().and_then(|u| u.player()).map_or(0, |p| p.handle() as i64),
        EE_SCENARIO_CAP_UNIT_HIT_POINTS => u().map_or(0, |u| u.hit_points()),
        EE_SCENARIO_CAP_UNIT_TILE_X => u().map_or(0, |u| u.tile_xy().0),
        EE_SCENARIO_CAP_UNIT_TILE_Y => u().map_or(0, |u| u.tile_xy().1),
        EE_SCENARIO_CAP_UNIT_COMPLEX => u().map_or(0, |u| u.complex_unit_handle() as i64),
        EE_SCENARIO_CAP_UNIT_ID => u().map_or(0, |u| u.id() as i64),
        EE_SCENARIO_CAP_UNIT_RESOURCE_AMOUNT => u().map_or(0, |u| u.resource_amount()),
        EE_SCENARIO_CAP_UNIT_RESOURCE_TYPE => u().map_or(0, |u| u.resource_type() as i64),
        EE_SCENARIO_CAP_COMMAND => c().map_or(0, |c| c.command_type() as i64),
        EE_SCENARIO_CAP_GOAL => c().map_or(0, |c| c.goal_handle() as i64),
        EE_SCENARIO_CAP_QUEUES => c().map_or(0, |c| c.goal_queues().len() as i64),
        EE_SCENARIO_CAP_DISABLED => c().is_some_and(|c| get!(c.complex(), mIsDisabled)) as i64,
        EE_SCENARIO_CAP_COMMANDABLE => c().is_some_and(|c| get!(c.complex(), mIsCommandable)) as i64,
        EE_SCENARIO_CAP_DELETABLE => c().is_some_and(|c| get!(c.complex(), mIsDeletable)) as i64,
        EE_SCENARIO_CAP_LOCKED => c().is_some_and(|c| get!(c.complex(), mIsLocked)) as i64,
        EE_SCENARIO_CAP_DETACHED => c().is_some_and(|c| get!(c.complex(), mIsDetachedFromAI)) as i64,
        EE_SCENARIO_CAP_BEHAVIOR => c().map_or(0, |c| c.behavior_handle() as i64),
        EE_SCENARIO_CAP_CONTAINER => c().map_or(0, |c| c.container_handle() as i64),
        EE_SCENARIO_CAP_FLIGHT_TIME => c().map_or(0, |c| get!(c.complex(), mFlightTime) as i64),
        EE_SCENARIO_CAP_AMMUNITION => c().map_or(0, |c| get!(c.complex(), mAmmunition) as i64),
        EE_SCENARIO_CAP_CARGO => c().map_or(0, |c| c.cargo_len() as i64),
        EE_SCENARIO_CAP_GARRISON => c().map_or(0, |c| get!(c.complex(), mGarrisonCount) as i64),
        EE_SCENARIO_CAP_RELOAD_TIMER => c().map_or(0, |c| get!(c.complex(), mReloadTimer) as i64),
        EE_SCENARIO_CAP_POWER => c().map_or(0, |c| get!(c.complex(), mPower) as i64),
        EE_SCENARIO_CAP_ROTTING => c().map_or(0, |c| get!(c.complex(), mRottingAwayTime) as i64),
        EE_SCENARIO_CAP_BUILT => c().is_some_and(|c| get!(c.complex(), mIsBuilt)) as i64,
        EE_SCENARIO_CAP_PAUSED => c().is_some_and(|c| get!(c.complex(), mIsAnimationPaused)) as i64,
        EE_SCENARIO_CAP_VARIABLES => c().map_or(0, |c| c.unit_variables().len() as i64),
        EE_SCENARIO_CAP_VARIABLE_AT => c().and_then(|c| c.unit_variables().get(a as usize).copied()).unwrap_or(0),
        // SAFETY: as above
        EE_SCENARIO_CAP_UPGRADE_FIELDS => unsafe { StatsView::of(&t, p) }.map_or(0, |s| s.fields().ptr() as i64),
        EE_SCENARIO_CAP_MANAGER_FIELDS => record(EE_SCENARIO_EN_MANAGER_FIELDS, p) as i64,
        EE_SCENARIO_CAP_CONDITION_FIELDS => record(EE_SCENARIO_EN_CONDITION_FIELDS, p) as i64,
        EE_SCENARIO_CAP_TRIGGER_FIELDS => record(EE_SCENARIO_EN_TRIGGER_FIELDS, p) as i64,
        EE_SCENARIO_CAP_OBJECT_FIELDS => record(EE_SCENARIO_EN_OBJECT_FIELDS, p) as i64,
        EE_SCENARIO_CAP_EFFECT_FIELDS => record(EE_SCENARIO_EN_EFFECT_FIELDS, p) as i64,
        EE_SCENARIO_CAP_AREA_ID => area().map_or(0, |r| r.mID as i64),
        EE_SCENARIO_CAP_AREA_SELECT => area().map_or(0, |r| r.mSelectMode as i64),
        EE_SCENARIO_CAP_AREA_ANCHOR_X => area().map_or(0, |r| r.mAnchor.mX as i64),
        EE_SCENARIO_CAP_AREA_ANCHOR_Y => area().map_or(0, |r| r.mAnchor.mY as i64),
        _ => return None,
    })
}

/// The scenario capture's float read `op` (`ee_scenariocpp_capture_f`'s) the views and records answer, or None.
///
/// # Safety
/// As [`capture`].
pub unsafe fn capture_f(op: u32, p: u64, record: &dyn Fn(u32, u64) -> u64) -> Option<f32> {
    use crate::scenario::area_fields::EeScenarioAreaFields;
    use crate::scenario::entry::EE_SCENARIO_EN_AREA_FIELDS;
    use crate::scenario::sm_capture::*;
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    // SAFETY: an area's record lives with it
    let area = || unsafe { (record(EE_SCENARIO_EN_AREA_FIELDS, p) as *const EeScenarioAreaFields).as_ref() };
    Some(match op {
        EE_SCENARIO_CAP_AREA_LEFT => area()?.mRect.mLeft,
        EE_SCENARIO_CAP_AREA_TOP => area()?.mRect.mTop,
        EE_SCENARIO_CAP_AREA_RIGHT => area()?.mRect.mRight,
        EE_SCENARIO_CAP_AREA_BOTTOM => area()?.mRect.mBottom,
        EE_SCENARIO_CAP_GAME_SPEED => WorldView::global(&t)?.game_speed(),
        // SAFETY: the operation's live complex unit
        EE_SCENARIO_CAP_SCALE => unsafe { UnitView::of(&t, p) }?.model_scale(),
        _ => return None,
    })
}

/// The answer to the round's operation `op` (`EE_SCENARIO_ROUND_*`, round.rs) that reads the world
/// (EEScenarioManager.cpp's cases), or None for the C++ host's.
///
/// # Safety
/// As [`answer`].
pub unsafe fn round(op: u32, a: i64) -> Option<i64> {
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let w = WorldView::global(&t)?;
    let player = || if a < 0 { None } else { w.player(a as c_ulong as usize) };
    Some(match op {
        EE_SCENARIO_ROUND_GAME_SPEED => bits(w.game_speed()),
        EE_SCENARIO_ROUND_IN_CINEMATIC => w.in_cinematic() as i64,
        EE_SCENARIO_ROUND_NUM_PLAYERS => w.num_players() as i64,
        EE_SCENARIO_ROUND_PLAYER_EXISTS => player().is_some() as i64,
        EE_SCENARIO_ROUND_PLAYER_OBSERVER => player().is_some_and(|p| p.observer()) as i64,
        EE_SCENARIO_ROUND_SET_IN_CINEMATIC => {
            let at = w.handle() as usize + crate::world::layout::layout().world_in_cinematic as usize;
            // SAFETY: the world's bool member (the layout's), written on the simulation thread as the C++ did
            unsafe { *(at as *mut bool) = a != 0 };
            (a != 0) as i64
        }
        _ => return None,
    })
}

#[cfg(test)]
#[path = "world_views_tests.rs"]
mod tests;
