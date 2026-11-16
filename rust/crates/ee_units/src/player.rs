//! The player's economy and improvements (`EEPlayer`, EEPlayer.cpp): the members the C++ class kept after the saved
//! record (`EeUnitsPlayerFields`, a record `EEPlayer` derives from: their C++ names and types, so every reader, the
//! classic AI's among them, is unchanged), the queued improvements by unit type as a Rust map the record points to
//! (`mQueued`), and the decisions over them as the C++ bodies in their order over one host ([`PlayerHost`], the
//! C++ `EEPlayerHost`'s operations):
//!
//! - the costs: `CanAffordScaled`, `CanAffordSingleResource` (one resource, or iron and gold together with the
//!   metallurgy power, and the required statistic), `CanAfford`, `CanAffordTechnology`, `CanProduce` (the
//!   population, the teleporter and the hero limits, the cost, a researched producer), `Pay`, `PayImprovement`,
//!   `RefundTechnology`, `RefundImprovement`;
//! - the improvements: `GetImprovementLevel` (the upgrade's level and the queued ones), `CountQueuedImprovements`,
//!   `CanImprove`, `CanBuy`, `IsImprovementMaxed`, `QueueImprovement`, `RemoveQueuedImprovement`,
//!   `AdvanceImprovement`, the queue's erase and move, the portrait's progress, `RepairPointers`' requeue, the tick's
//!   walk (player_tick.rs) and the saved game's entries (player_save.rs's operations answered here).
//!
//! The virtuals the computer player overrides (`ScaleCost`, `TakeResource`, `AddResource`, `CanAfford`,
//! `GetExtraPopulation`, `GetPopulationLimit`, `GetHeroCount`, `PayImprovement`) are host calls, so its overrides run
//! as before; the tech nodes' `GetCost` (the TCST capture's) is called in the original's order and count.
//!
//! Kept: the `+ 0.0001f` of the scaled and improvement costs (none in `Pay`'s), the costs' `long` products wrapping,
//! `CanBuy`'s unsigned compare with the improvement points' maximum where `CanImprove` and `IsImprovementMaxed`
//! compare signed, `PayImprovement`'s gold-and-iron cover without the metallurgy test, `RemoveQueuedImprovement`'s
//! refund at the full cost. Defined where the original was not: a node, class or upgrade missing where the
//! original dereferenced it answers "cannot" (a crash before), the portrait's improvement of an empty queue is 0x10
//! (none) where the original read past it.
//!
//! Determinism: the original's `float` operations in order (no FMA), `(long)` of a float as `cvttss2si`, no random
//! draw; the map in key order (`std::map`'s). No Rust reference to the record or the queue lives across a host call
//! that can reach them (the requeue and the tick take theirs again after each call).

#![allow(non_snake_case)]
#![allow(clippy::unnecessary_cast)]

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::player_save::{
    EeUnitsPlayerSaveIo, EeUnitsPlayerSaved, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};
use crate::player_stats::EeUnitsPlayerStats;

/// The queued improvements: per unit type the running timer (`first`) and the improvements in order.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeUnitsQueued {
    pub map: BTreeMap<c_ulong, (f32, Vec<c_ulong>)>,
}

impl EeUnitsQueued {
    /// `operator[]`: the entry, made (0, empty) if absent.
    pub fn entry(&mut self, key: c_ulong) -> &mut (f32, Vec<c_ulong>) {
        self.map.entry(key).or_insert((0.0, Vec::new()))
    }
}

/// `EEPlayer`'s other members (their C++ names and types).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsPlayerFields {
    /// `kNoUnitClass` (0xffffffff): none; a unit class table index
    pub mHackUnitClass: c_ulong,
    pub mConversionFactor: f32,
    /// the tribute fee (0.4)
    pub mTributeFee: f32,
    pub mMaxStartingCitizens: c_long,
    /// the damage taken from each player
    pub mDamageFrom: [c_long; 17],
    pub mNumAttacks: c_long,
    pub mLastAcknowledgementTime: c_long,
    pub mLastAcknowledgedGroup: c_long,
    pub mHeroLimit: c_long,
    pub mMiningLimit: c_long,
    pub mLAF0: c_long,
    pub mShareBonus: c_long,
    pub mPopulationLimitBonus: c_long,
    pub mNumHeroes: c_long,
    pub mNumHeroesInProduction: c_long,
    pub mNumTeleporters: c_long,
    pub mAreaConversion: bool,
    pub mBB42: bool,
    pub mBB44: bool,
    pub mInstantBuild: bool,
    pub mHealsUnits: bool,
    pub mBehindFirewall: bool,
    pub mEarthquakeShaken: bool,
    pub mShiftMode: bool,
    pub mJITManufacturing: bool,
    /// the queued improvements (made by the constructors, freed by the destructor)
    pub mQueued: *mut EeUnitsQueued,
    /// the wonders' types and powers (player_units.rs; made by the constructors, freed by the destructor)
    pub mWonderState: *mut crate::player_units::EeUnitsPlayerWonders,
}

impl Default for EeUnitsPlayerFields {
    /// zeros and no queue (`Reset` sets the C++ values)
    fn default() -> Self {
        EeUnitsPlayerFields {
            mHackUnitClass: 0,
            mConversionFactor: 0.0,
            mTributeFee: 0.0,
            mMaxStartingCitizens: 0,
            mDamageFrom: [0; 17],
            mNumAttacks: 0,
            mLastAcknowledgementTime: 0,
            mLastAcknowledgedGroup: 0,
            mHeroLimit: 0,
            mMiningLimit: 0,
            mLAF0: 0,
            mShareBonus: 0,
            mPopulationLimitBonus: 0,
            mNumHeroes: 0,
            mNumHeroesInProduction: 0,
            mNumTeleporters: 0,
            mAreaConversion: false,
            mBB42: false,
            mBB44: false,
            mInstantBuild: false,
            mHealsUnits: false,
            mBehindFirewall: false,
            mEarthquakeShaken: false,
            mShiftMode: false,
            mJITManufacturing: false,
            mQueued: std::ptr::null_mut(),
            mWonderState: std::ptr::null_mut(),
        }
    }
}

/// `kTributeFee`
pub const EE_UNITS_PLAYER_TRIBUTE_FEE: f32 = 0.4;
/// `kNoUnitClass`
pub const EE_UNITS_PLAYER_NO_UNIT_CLASS: c_ulong = 0xffff_ffff;
/// `kTechNone`
pub const EE_UNITS_PLAYER_TECH_NONE: c_long = 100_000;
const GOLD: c_long = 5;
const IRON: c_long = 6;

/// `Reset`'s stores into the record (the queue untouched).
pub fn reset_fields(r: &mut EeUnitsPlayerFields, starting_citizens: c_long, mining_limit: c_long) {
    r.mConversionFactor = 1.0;
    r.mNumHeroes = 0;
    r.mHeroLimit = 1;
    r.mNumHeroesInProduction = 0;
    r.mTributeFee = EE_UNITS_PLAYER_TRIBUTE_FEE;
    r.mShareBonus = 0;
    r.mPopulationLimitBonus = 0;
    r.mBB44 = false;
    r.mInstantBuild = false;
    r.mJITManufacturing = false;
    r.mLAF0 = 0;
    r.mHealsUnits = false;
    r.mMaxStartingCitizens = starting_citizens;
    r.mMiningLimit = mining_limit;
    r.mAreaConversion = false;
    r.mBehindFirewall = false;
    r.mBB42 = false;
    r.mShiftMode = false;
    r.mLastAcknowledgementTime = 0;
    r.mLastAcknowledgedGroup = 0;
    r.mEarthquakeShaken = false;
    r.mNumTeleporters = 0;
    r.mDamageFrom = [0; 17];
    r.mNumAttacks = 0;
    r.mHackUnitClass = EE_UNITS_PLAYER_NO_UNIT_CLASS;
}

/// One host call's values: the arguments in `i`/`f`, the answers back in them.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerIo {
    pub i: [i64; 8],
    pub f: [f32; 2],
}

// ---- the host's operations
/// `mTechnologies->Get(i[0])` (the result: the node, 0 none)
pub const EE_UNITS_PLAYER_NODE: i32 = 1;
/// node `i[0]`'s data into `i`: makes a unit, its unit class, the cost resource, the required statistic and value,
/// the kind, the unit classes' count, researched
pub const EE_UNITS_PLAYER_NODE_INFO: i32 = 2;
/// node `i[0]`'s `GetIsAvailable()` (virtual)
pub const EE_UNITS_PLAYER_NODE_AVAILABLE: i32 = 3;
/// node `i[0]`'s `GetUnitClass(i[1])`
pub const EE_UNITS_PLAYER_NODE_UNIT_CLASS: i32 = 4;
/// node `i[0]`'s `GetCost(i[1], i[2], 1.0f)` (virtual; the TCST capture's)
pub const EE_UNITS_PLAYER_NODE_COST: i32 = 5;
/// `ScaleCost(i[0])` (virtual)
pub const EE_UNITS_PLAYER_SCALE_COST: i32 = 6;
/// `gCivPowersManager.HasPower(this, kCivPowerMetallurgy)`
pub const EE_UNITS_PLAYER_METALLURGY: i32 = 7;
/// `TakeResource(i[0], i[1])` (virtual)
pub const EE_UNITS_PLAYER_TAKE: i32 = 8;
/// `AddResource(i[0], i[1])` (virtual)
pub const EE_UNITS_PLAYER_ADD: i32 = 9;
/// `gUnitClassTable[i[0]]` (the result 1 when there is one) into `i`: its technology, weight, population cost,
/// whether it is the teleporter, a hero (its AI kind), whether it has improvements
pub const EE_UNITS_PLAYER_CLASS: i32 = 10;
/// `GetExtraPopulation(i[0])` (virtual)
pub const EE_UNITS_PLAYER_EXTRA_POPULATION: i32 = 11;
/// `GetPopulationLimit(0)` (virtual)
pub const EE_UNITS_PLAYER_POPULATION_LIMIT: i32 = 12;
/// `GetHeroCount(i[0], i[1])` (virtual)
pub const EE_UNITS_PLAYER_HERO_COUNT: i32 = 13;
/// `gWorld.mGameOptions.mIsScenarioGame`
pub const EE_UNITS_PLAYER_SCENARIO: i32 = 14;
/// `CanAfford(node i[0], i[1], i[2])` (virtual)
pub const EE_UNITS_PLAYER_CAN_AFFORD: i32 = 15;
/// the reason text `i[1]` (1 the maximum built, 2 the hero limit, 3 the population limit, 4 the construction's
/// resources, 5 the research's, 6 the improvements' maximum) into the `UWideString*` `i[0]`
pub const EE_UNITS_PLAYER_REASON: i32 = 16;
/// `mSavedGroups.Lower(i[0], i[1])`
pub const EE_UNITS_PLAYER_STATS_LOWER: i32 = 17;
/// unit type `i[0]`'s upgrade and class for improvement `i[1]` (the result 1 when the upgrade is there): `i` its
/// `CanUpgrade`, `GetCost(i[1])`, `mTotalCost`, its level, the table's maximum level, the world's improvement
/// points' maximum; `f` the step factor (0 for improvement 12 or without a table) and the cost factor
pub const EE_UNITS_PLAYER_IMPROVEMENT: i32 = 18;
/// unit type `i[0]`'s upgrade's `GetCost(i[1])`
pub const EE_UNITS_PLAYER_IMPROVEMENT_COST: i32 = 19;
/// unit type `i[0]`'s upgrade's level of improvement `i[1]` (the result 1 when the upgrade is there; the level in
/// `i[0]`)
pub const EE_UNITS_PLAYER_LEVEL: i32 = 20;
/// improvement `i[0]`'s time (`(long)` of the world table's `GetLongValue2` when `i[1]`, else `GetLongValue`)
pub const EE_UNITS_PLAYER_IMPROVEMENT_TIME: i32 = 21;
/// whether the player is the local player
pub const EE_UNITS_PLAYER_IS_LOCAL: i32 = 22;
/// `gUserInterface->UpdateJITManufacturing()`
pub const EE_UNITS_PLAYER_UPDATE_JIT: i32 = 23;
/// `PayImprovement(gUnitClassTable[i[0]], i[1])` (virtual)
pub const EE_UNITS_PLAYER_PAY_IMPROVEMENT: i32 = 24;

const REASON_MAXIMUM_BUILT: i64 = 1;
const REASON_HERO_LIMIT: i64 = 2;
const REASON_POPULATION: i64 = 3;
const REASON_CONSTRUCTION: i64 = 4;
const REASON_RESEARCH: i64 = 5;
const REASON_IMPROVEMENTS: i64 = 6;

/// The player's C++ side: one call per operation.
pub trait PlayerHost {
    fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64;

    fn ask(&mut self, op: i32, args: &[i64]) -> i64 {
        let mut io = EeUnitsPlayerIo::default();
        io.i[..args.len()].copy_from_slice(args);
        self.call(op, &mut io)
    }
    fn info(&mut self, op: i32, args: &[i64]) -> (i64, EeUnitsPlayerIo) {
        let mut io = EeUnitsPlayerIo::default();
        io.i[..args.len()].copy_from_slice(args);
        (self.call(op, &mut io), io)
    }
}

/// `(long)` of a float: `cvttss2si` (the integer indefinite, `c_long::MIN`, for NaN and out of range).
pub fn trunc_long(v: f32) -> c_long {
    let min = c_long::MIN as f32;
    if v.is_nan() || v < min || v >= -min {
        c_long::MIN
    } else {
        v as c_long
    }
}

/// A node's data (`EE_UNITS_PLAYER_NODE_INFO`).
#[derive(Clone, Copy, Debug, Default)]
pub(crate) struct Node {
    makes_unit: bool,
    pub(crate) unit_class: i64,
    cost_resource: c_long,
    required_statistic: c_long,
    required_value: c_long,
    pub(crate) kind: c_long,
    unit_classes: i64,
    researched: bool,
}

/// A unit class (`EE_UNITS_PLAYER_CLASS`).
#[derive(Clone, Copy, Debug, Default)]
pub(crate) struct Class {
    tech: c_long,
    weight: i64,
    population: c_long,
    teleporter: bool,
    pub(crate) hero: bool,
    improvements: bool,
}

/// An improvement's numbers (`EE_UNITS_PLAYER_IMPROVEMENT`).
#[derive(Clone, Copy, Debug, Default)]
struct Improvement {
    can_upgrade: bool,
    cost: c_long,
    total: c_long,
    level: c_long,
    max_level: c_long,
    points_max: c_long,
    step: f32,
    factor: f32,
}

/// The decisions over the host and the player's records.
pub struct Player<'a, H: PlayerHost + ?Sized> {
    pub h: &'a mut H,
    pub saved: *mut EeUnitsPlayerSaved,
    pub fields: *mut EeUnitsPlayerFields,
    pub stats: *const EeUnitsPlayerStats,
}

impl<'a, H: PlayerHost + ?Sized> Player<'a, H> {
    fn cap(&self, resource: c_long) -> c_long {
        // SAFETY: `saved` is the player's record for the call (a field read, no reference kept)
        usize::try_from(resource).ok().and_then(|r| unsafe { (*self.saved).mResourceCaps.get(r).copied() }).unwrap_or(0)
    }
    fn fields(&self) -> EeUnitsPlayerFields {
        // SAFETY: as cap (a copy)
        unsafe { *self.fields }
    }
    fn queued(&mut self) -> &mut EeUnitsQueued {
        // SAFETY: the record's queue, live while the player is; the callers take it again after each host call
        unsafe { &mut *(*self.fields).mQueued }
    }

    pub(crate) fn node(&mut self, tech: c_long) -> i64 {
        self.h.ask(EE_UNITS_PLAYER_NODE, &[tech as i64])
    }
    pub(crate) fn node_info(&mut self, node: i64) -> Node {
        let (_, io) = self.h.info(EE_UNITS_PLAYER_NODE_INFO, &[node]);
        Node {
            makes_unit: io.i[0] != 0,
            unit_class: io.i[1],
            cost_resource: io.i[2] as c_long,
            required_statistic: io.i[3] as c_long,
            required_value: io.i[4] as c_long,
            kind: io.i[5] as c_long,
            unit_classes: io.i[6],
            researched: io.i[7] != 0,
        }
    }
    pub(crate) fn class(&mut self, unit_type: i64) -> Option<Class> {
        let (found, io) = self.h.info(EE_UNITS_PLAYER_CLASS, &[unit_type]);
        (found != 0).then_some(Class {
            tech: io.i[0] as c_long,
            weight: io.i[1],
            population: io.i[2] as c_long,
            teleporter: io.i[3] != 0,
            hero: io.i[4] != 0,
            improvements: io.i[5] != 0,
        })
    }
    fn improvement(&mut self, unit_type: c_ulong, improvement: c_long) -> Option<Improvement> {
        let (found, io) = self.h.info(EE_UNITS_PLAYER_IMPROVEMENT, &[unit_type as i64, improvement as i64]);
        (found != 0).then_some(Improvement {
            can_upgrade: io.i[0] != 0,
            cost: io.i[1] as c_long,
            total: io.i[2] as c_long,
            level: io.i[3] as c_long,
            max_level: io.i[4] as c_long,
            points_max: io.i[5] as c_long,
            step: io.f[0],
            factor: io.f[1],
        })
    }
    fn cost(&mut self, node: i64, resource: c_long, unit_class: c_long) -> c_long {
        self.h.ask(EE_UNITS_PLAYER_NODE_COST, &[node, resource as i64, unit_class as i64]) as c_long
    }
    fn scale_cost(&mut self, cost: c_long) -> c_long {
        self.h.ask(EE_UNITS_PLAYER_SCALE_COST, &[cost as i64]) as c_long
    }
    fn metallurgy(&mut self) -> bool {
        self.h.ask(EE_UNITS_PLAYER_METALLURGY, &[]) != 0
    }
    fn take(&mut self, resource: c_long, amount: c_long) {
        self.h.ask(EE_UNITS_PLAYER_TAKE, &[resource as i64, amount as i64]);
    }
    fn add(&mut self, resource: c_long, amount: c_long) {
        self.h.ask(EE_UNITS_PLAYER_ADD, &[resource as i64, amount as i64]);
    }
    fn reason(&mut self, reason: i64, kind: i64) {
        if reason != 0 {
            self.h.ask(EE_UNITS_PLAYER_REASON, &[reason, kind]);
        }
    }
    fn tech_of(&mut self, unit_type: i64) -> (Option<Class>, c_long) {
        let class = self.class(unit_type);
        let tech = class.map_or(EE_UNITS_PLAYER_TECH_NONE, |c| c.tech);
        (class, tech)
    }

    /// `ScaleCost((long)(GetCost(r) * scale + 0.0001f)) * count`
    fn scaled_cost(&mut self, node: i64, resource: c_long, unit_class: c_long, scale: f32, count: c_long) -> c_long {
        let cost = self.cost(node, resource, unit_class) as f32 * scale + 0.0001;
        self.scale_cost(trunc_long(cost)).wrapping_mul(count)
    }

    /// `CanAffordScaled`: every resource covers its scaled cost (with metallurgy iron and gold together).
    pub fn can_afford_scaled(&mut self, node: i64, unit_class: c_long, scale: f32, count: c_long) -> bool {
        if node == 0 {
            return false;
        }
        let mut result = true;
        let mut i: c_long = 2;
        while i < 7 && result {
            let amount = self.cap(i);
            result = amount >= self.scaled_cost(node, i, unit_class, scale, count);
            if !result && (i == 6 || i == 5) && self.metallurgy() {
                let cost = self
                    .scaled_cost(node, IRON, unit_class, scale, count)
                    .wrapping_add(self.scaled_cost(node, GOLD, unit_class, scale, count));
                if self.cap(IRON).wrapping_add(self.cap(GOLD)) >= cost {
                    result = true;
                }
            }
            i += 1;
        }
        result
    }

    /// `CanAffordSingleResource`: the node's cost resource alone (or any one of the five, or iron and gold together
    /// with metallurgy) covers the cost, and the required statistic is reached.
    pub fn can_afford_single(&mut self, node: i64, unit_class: c_long, count: c_long) -> bool {
        if node == 0 {
            return false;
        }
        let n = self.node_info(node);
        let mut result = false;
        if n.cost_resource == 0 || n.required_value == 0 {
            return false;
        }
        if n.cost_resource != 1 && {
            let have = self.cap(n.cost_resource);
            let cost = self.cost(node, n.cost_resource, unit_class);
            have >= self.scale_cost(cost).wrapping_mul(count)
        } {
            result = true;
        } else {
            let mut i: c_long = 2;
            while i < 7 && !result {
                let amount = self.cap(i);
                let cost = self.cost(node, i, unit_class);
                result = amount >= self.scale_cost(cost).wrapping_mul(count);
                if !result && (i == 6 || i == 5) && self.metallurgy() {
                    let iron = self.cost(node, IRON, unit_class) as f32 + 0.0001;
                    let iron = self.scale_cost(trunc_long(iron)).wrapping_mul(count);
                    let gold = self.cost(node, GOLD, unit_class) as f32 + 0.0001;
                    let cost = iron.wrapping_add(self.scale_cost(trunc_long(gold)).wrapping_mul(count));
                    if self.cap(IRON).wrapping_add(self.cap(GOLD)) >= cost {
                        result = true;
                    }
                }
                i += 1;
            }
        }
        if result {
            // SAFETY: the player's statistics for the call
            let last = usize::try_from(n.required_statistic).map_or(0, |s| unsafe { (*self.stats).last(s) });
            if last < n.required_value as c_ulong {
                result = false;
            }
        }
        result
    }

    /// `EEPlayer::CanAfford`: the scaled cost at 1, else a single resource.
    pub fn can_afford(&mut self, node: i64, unit_type: c_long, count: c_long) -> bool {
        self.can_afford_scaled(node, unit_type, 1.0, count) || self.can_afford_single(node, unit_type, count)
    }

    /// `CanAffordTechnology`
    pub fn can_afford_technology(&mut self, tech: c_long, scale: f32, reason: i64) -> bool {
        let node = self.node(tech);
        let result = self.can_afford_scaled(node, 0, scale, 1);
        if !result {
            self.reason(reason, REASON_CONSTRUCTION);
        }
        result
    }

    /// `EEPlayer::CanProduce`: an available node; a unit within the population limit (the computer player's extra),
    /// one teleporter, the hero limit by `wonder_mode`; the cost when `check_cost` (the virtual `CanAfford`); a
    /// researched unit class to make it.
    pub fn can_produce(
        &mut self,
        tech: c_long,
        unit_class: c_long,
        count: c_long,
        check_cost: bool,
        wonder_mode: c_long,
        reason: i64,
    ) -> bool {
        let node = self.node(tech);
        if node == 0 || self.h.ask(EE_UNITS_PLAYER_NODE_AVAILABLE, &[node]) == 0 {
            return false;
        }
        let n = self.node_info(node);
        let mut allowed = true;
        if n.makes_unit {
            if let Some(class) = self.class(n.unit_class) {
                let saved = self.saved;
                let population = || {
                    // SAFETY: as cap
                    unsafe { (*saved).mPopulationCount }
                };
                let fits = class.weight == 0
                    || {
                        let need = class.population.wrapping_add(population());
                        let extra = self.h.ask(EE_UNITS_PLAYER_EXTRA_POPULATION, &[n.unit_class]) as c_long;
                        need <= extra.wrapping_add(self.h.ask(EE_UNITS_PLAYER_POPULATION_LIMIT, &[]) as c_long)
                    }
                    || (population() == (self.h.ask(EE_UNITS_PLAYER_POPULATION_LIMIT, &[]) as c_long).wrapping_sub(1)
                        && {
                            let need = class.population.wrapping_add(population());
                            let extra = self.h.ask(EE_UNITS_PLAYER_EXTRA_POPULATION, &[n.unit_class]) as c_long;
                            need <= extra.wrapping_add(self.h.ask(EE_UNITS_PLAYER_POPULATION_LIMIT, &[]) as c_long)
                        });
                if fits {
                    if class.teleporter {
                        allowed = self.fields().mNumTeleporters < 1;
                        if !allowed {
                            self.reason(reason, REASON_MAXIMUM_BUILT);
                        }
                    }
                    if allowed && class.hero && self.h.ask(EE_UNITS_PLAYER_SCENARIO, &[]) == 0 && wonder_mode != 0 {
                        let heroes = |me: &mut Self, a: bool, b: bool| {
                            me.h.ask(EE_UNITS_PLAYER_HERO_COUNT, &[i64::from(a), i64::from(b)]) as c_long
                        };
                        match wonder_mode {
                            3 => allowed = heroes(self, true, true) < self.fields().mHeroLimit,
                            1 => allowed = heroes(self, false, true) < self.fields().mHeroLimit,
                            2 => allowed = heroes(self, true, false) < self.fields().mHeroLimit,
                            _ => {}
                        }
                        if !allowed {
                            self.reason(reason, REASON_HERO_LIMIT);
                        }
                    }
                } else {
                    allowed = false;
                    self.reason(reason, REASON_POPULATION);
                }
            }
        }
        if check_cost && allowed {
            allowed = self.h.ask(EE_UNITS_PLAYER_CAN_AFFORD, &[node, unit_class as i64, count as i64]) != 0;
            if !allowed {
                self.reason(reason, if n.makes_unit { REASON_CONSTRUCTION } else { REASON_RESEARCH });
            }
        }
        if allowed {
            allowed = n.unit_classes > 0;
            let mut i = n.unit_classes - 1;
            while i >= 0 && !allowed {
                let unit_type = self.h.ask(EE_UNITS_PLAYER_NODE_UNIT_CLASS, &[node, i]);
                let (_, tech) = self.tech_of(unit_type);
                let producer = self.node(tech);
                allowed = producer != 0 && self.node_info(producer).researched;
                i -= 1;
            }
        }
        allowed
    }

    /// `TakeResource` of iron and gold for `cost`, the larger of the two paying what the other cannot.
    fn take_gold_and_iron(&mut self, cost: c_long) {
        if self.cap(GOLD) >= self.cap(IRON) {
            let iron = self.cap(IRON);
            self.take(GOLD, cost.wrapping_sub(iron));
            let iron = self.cap(IRON);
            self.take(IRON, iron);
        } else {
            let gold = self.cap(GOLD);
            self.take(IRON, cost.wrapping_sub(gold));
            let gold = self.cap(GOLD);
            self.take(GOLD, gold);
        }
    }

    /// `ScaleCost((long)(GetCost(r) * fraction))` (`Pay`'s: no `+ 0.0001f`)
    fn pay_cost(&mut self, node: i64, resource: c_long, unit_type: c_long, fraction: f32) -> c_long {
        let cost = self.cost(node, resource, unit_type) as f32 * fraction;
        self.scale_cost(trunc_long(cost))
    }

    /// `EEPlayer::Pay`: a single resource's cost when it can be paid so (and the required statistic lowered), else
    /// each resource's, iron and gold covering each other with metallurgy.
    pub fn pay(&mut self, node: i64, unit_type: c_long, fraction: f32) {
        if node == 0 {
            return;
        }
        if self.can_afford_single(node, unit_type, 1) {
            let n = self.node_info(node);
            if n.cost_resource == 1 {
                for i in 2..7 {
                    let cost = self.pay_cost(node, i, unit_type, fraction);
                    if self.cap(i) >= cost {
                        self.take(i, cost);
                        break;
                    }
                }
            } else {
                let cost = self.pay_cost(node, n.cost_resource, unit_type, fraction);
                let resource = n.cost_resource;
                if cost > self.cap(resource) {
                    if (resource == 5 || resource == 6)
                        && self.cap(GOLD).wrapping_add(self.cap(IRON)) >= cost
                        && self.metallurgy()
                    {
                        self.take_gold_and_iron(cost);
                    }
                } else {
                    self.take(resource, cost);
                }
            }
            self.h.ask(EE_UNITS_PLAYER_STATS_LOWER, &[n.required_statistic as i64, n.required_value as i64]);
            return;
        }
        let mut insufficient = false;
        let mut only_interchangeable = true;
        for i in 2..7 {
            let cost = self.pay_cost(node, i, unit_type, fraction);
            if cost <= self.cap(i) {
                continue;
            }
            insufficient = true;
            if i != 6 && i != 5 {
                only_interchangeable = false;
                break;
            }
        }
        if insufficient {
            if only_interchangeable && self.metallurgy() {
                let gold = self.pay_cost(node, GOLD, unit_type, fraction);
                let iron = self.pay_cost(node, IRON, unit_type, fraction);
                let cost = gold.wrapping_add(iron);
                if self.cap(GOLD).wrapping_add(self.cap(IRON)) >= cost {
                    self.take_gold_and_iron(cost);
                    for i in 2..7 {
                        if i != 5 && i != 6 {
                            let other = self.pay_cost(node, i, unit_type, fraction);
                            self.take(i, other);
                        }
                    }
                }
            }
        } else {
            for i in 2..7 {
                let cost = self.pay_cost(node, i, unit_type, fraction);
                if cost > self.cap(i) {
                    return;
                }
                self.take(i, cost);
            }
        }
    }

    /// `RefundTechnology`: each resource's cost times `fraction` added back.
    pub fn refund_technology(&mut self, node: i64, unit_type: c_long, fraction: f32) {
        for i in 2..7 {
            let cost = self.cost(node, i, unit_type);
            let amount = trunc_long(self.scale_cost(cost) as f32 * fraction);
            self.add(i, amount);
        }
    }

    /// The node and the cost scale of unit type `unit_type`'s improvement at its queued level (`PayImprovement`'s
    /// and `RefundImprovement`'s start), or None.
    fn improvement_scale(&mut self, unit_type: c_ulong, improvement: c_long) -> Option<(i64, f32)> {
        let (class, tech) = self.tech_of(unit_type as i64);
        if class.is_none() || tech == EE_UNITS_PLAYER_TECH_NONE {
            return None;
        }
        let node = self.node(tech);
        if node == 0 || !class.is_some_and(|c| c.improvements) {
            return None;
        }
        let factor = self.improvement(unit_type, improvement).map_or(0.0, |i| i.factor);
        Some((node, factor * self.level(unit_type, improvement) as f32))
    }

    /// `ScaleCost((long)(GetCost(r, 0) * scale + 0.0001f))`
    fn improvement_cost(&mut self, node: i64, resource: c_long, scale: f32) -> c_long {
        let cost = self.cost(node, resource, 0) as f32 * scale + 0.0001;
        self.scale_cost(trunc_long(cost))
    }

    /// `EEPlayer::PayImprovement`: each resource's cost at the queued level; a short gold or iron paid by the other.
    pub fn pay_improvement(&mut self, unit_type: c_ulong, improvement: c_long) {
        let Some((node, scale)) = self.improvement_scale(unit_type, improvement) else { return };
        for i in 2..7 {
            let cost = self.improvement_cost(node, i, scale);
            if self.cap(i).wrapping_sub(cost) < 0 {
                if i == 6 {
                    let iron = self.cap(IRON);
                    self.take(GOLD, cost.wrapping_sub(iron));
                    let iron = self.cap(IRON);
                    self.take(i, iron);
                } else if i == 5 {
                    let gold = self.cap(GOLD);
                    self.take(IRON, cost.wrapping_sub(gold));
                    let gold = self.cap(GOLD);
                    self.take(i, gold);
                }
            } else {
                self.take(i, cost);
            }
        }
    }

    /// `RefundImprovement`: each resource's cost at the queued level times `fraction` added back.
    pub fn refund_improvement(&mut self, unit_type: c_ulong, improvement: c_long, fraction: f32) {
        let Some((node, scale)) = self.improvement_scale(unit_type, improvement) else { return };
        for i in 2..7 {
            let cost = trunc_long(self.cost(node, i, 0) as f32 * scale + 0.0001);
            let amount = trunc_long(self.scale_cost(cost) as f32 * fraction);
            self.add(i, amount);
        }
    }

    // ---- the improvements
    /// The upgrade's level of an improvement (None without an upgrade).
    fn upgrade_level(&mut self, unit_type: c_ulong, improvement: c_long) -> Option<c_long> {
        let (found, io) = self.h.info(EE_UNITS_PLAYER_LEVEL, &[unit_type as i64, improvement as i64]);
        (found != 0).then_some(io.i[0] as c_long)
    }

    /// `GetImprovementLevel`: the upgrade's level plus the queued ones (-1 without an upgrade).
    pub fn level(&mut self, unit_type: c_ulong, improvement: c_long) -> c_long {
        let Some(level) = self.upgrade_level(unit_type, improvement) else { return -1 };
        let queued = self
            .queued()
            .map
            .get(&unit_type)
            .map_or(0, |e| e.1.iter().filter(|&&x| x == improvement as c_ulong).count());
        level.wrapping_add(queued as c_long)
    }

    /// `CountQueuedImprovements`: the queued ones past the first.
    pub fn count_queued(&mut self, unit_type: c_ulong, improvement: c_long) -> c_long {
        if self.upgrade_level(unit_type, improvement).is_none() {
            return 0;
        }
        let q = self.queued();
        q.map.get(&unit_type).map_or(0, |e| e.1.iter().skip(1).filter(|&&x| x == improvement as c_ulong).count())
            as c_long
    }

    /// The improvement points of the upgrade and its queue (`mTotalCost` plus each queued improvement's cost).
    fn points(&mut self, unit_type: c_ulong, total: c_long) -> c_long {
        let queue = self.queued().map.get(&unit_type).map(|e| e.1.clone()).unwrap_or_default();
        let mut points = total;
        for x in queue {
            points = points
                .wrapping_add(self.h.ask(EE_UNITS_PLAYER_IMPROVEMENT_COST, &[unit_type as i64, x as i64]) as c_long);
        }
        points
    }

    /// `CanImprove`: allowed, upgradable, within the improvement points and the maximum level (the queue not counted).
    pub fn can_improve(&mut self, unit_type: c_ulong, improvement: c_long) -> bool {
        let (class, tech) = self.tech_of(unit_type as i64);
        if class.is_none() || tech == EE_UNITS_PLAYER_TECH_NONE {
            return false;
        }
        let Some(i) = self.improvement(unit_type, improvement) else { return false };
        i.step > 0.0 && i.can_upgrade && i.cost.wrapping_add(i.total) <= i.points_max && i.level <= i.max_level
    }

    /// `CanBuy`: as `CanImprove` with the queue counted (unsigned), then the cost at the queued level affordable.
    pub fn can_buy(&mut self, unit_type: c_ulong, improvement: c_long, reason: i64) -> bool {
        let (class, tech) = self.tech_of(unit_type as i64);
        if class.is_none() || tech == EE_UNITS_PLAYER_TECH_NONE {
            return false;
        }
        let Some(i) = self.improvement(unit_type, improvement) else { return false };
        if !(i.step > 0.0 && i.can_upgrade) {
            return false;
        }
        let points = self.points(unit_type, i.total);
        if (i.cost.wrapping_add(points) as c_ulong) <= i.points_max as c_ulong
            && self.level(unit_type, improvement) <= i.max_level
        {
            let level = self.level(unit_type, improvement);
            return self.can_afford_technology(tech, level as f32 * i.factor, reason);
        }
        self.reason(reason, REASON_IMPROVEMENTS);
        false
    }

    /// `IsImprovementMaxed`: past the improvement points or the maximum level with the queue counted.
    pub fn is_improvement_maxed(&mut self, unit_type: c_ulong, improvement: c_long) -> bool {
        let (class, tech) = self.tech_of(unit_type as i64);
        if class.is_none() || tech == EE_UNITS_PLAYER_TECH_NONE {
            return true;
        }
        let Some(i) = self.improvement(unit_type, improvement) else { return true };
        let points = self.points(unit_type, i.total);
        !(i.cost.wrapping_add(points) <= i.points_max && self.level(unit_type, improvement) <= i.max_level)
    }

    /// `QueueImprovement`: when it can be bought, paid (unless already) and queued at the back or the front.
    pub fn queue_improvement(&mut self, unit_type: c_ulong, improvement: c_ulong, front: bool, paid: bool) {
        if self.class(unit_type as i64).is_none() || !self.can_buy(unit_type, improvement as c_long, 0) {
            return;
        }
        if !paid {
            self.h.ask(EE_UNITS_PLAYER_PAY_IMPROVEMENT, &[unit_type as i64, improvement as i64]);
        }
        let list = &mut self.queued().entry(unit_type).1;
        if front {
            list.insert(0, improvement);
        } else {
            list.push(improvement);
        }
    }

    /// `RemoveQueuedImprovement`: the first queued `improvement` (past the running one with `not_running`) dropped and
    /// refunded; the timer stopped when it was the running one, the entry erased when empty.
    pub fn remove_queued(&mut self, unit_type: c_ulong, improvement: c_long, not_running: bool) {
        let Some(entry) = self.queued().map.get_mut(&unit_type) else { return };
        let skip = usize::from(not_running);
        let Some(at) = entry.1.iter().skip(skip).position(|&x| x == improvement as c_ulong).map(|p| p + skip) else {
            return;
        };
        entry.1.remove(at);
        self.refund_improvement(unit_type, improvement, 1.0);
        let entry = self.queued().entry(unit_type);
        if at == 0 {
            entry.0 = 0.0;
        }
        if entry.1.is_empty() {
            self.queued().map.remove(&unit_type);
        }
    }

    /// `AdvanceImprovement`: the queue's first improvement started when it can be done (instantly with instant build,
    /// else the world's time for its level); true when the queue is empty.
    pub fn advance(&mut self, unit_type: c_ulong) -> bool {
        let Some(first) = self.queued().map.get(&unit_type).map(|e| e.1.first().copied()) else { return false };
        let Some(improvement) = first.filter(|_| self.class(unit_type as i64).is_some()) else { return true };
        if !self.can_improve(unit_type, improvement as c_long) {
            return false;
        }
        let time = if self.fields().mInstantBuild {
            1.0
        } else {
            let second = self.upgrade_level(unit_type, improvement as c_long) != Some(1);
            self.h.ask(EE_UNITS_PLAYER_IMPROVEMENT_TIME, &[improvement as i64, i64::from(second)]) as c_long as f32
        };
        self.queued().entry(unit_type).0 = time;
        if self.h.ask(EE_UNITS_PLAYER_IS_LOCAL, &[]) != 0 {
            self.h.ask(EE_UNITS_PLAYER_UPDATE_JIT, &[]);
        }
        false
    }

    /// `RepairPointers`' requeue: each entry's improvements queued again (already paid), an entry left empty erased.
    pub fn requeue(&mut self) {
        let keys: Vec<c_ulong> = self.queued().map.keys().copied().collect();
        for key in keys {
            let Some(list) = self.queued().map.get_mut(&key).map(|e| std::mem::take(&mut e.1)) else { continue };
            for improvement in list {
                self.queue_improvement(key, improvement, false, true);
            }
            if self.queued().map.get(&key).is_some_and(|e| e.1.is_empty()) {
                self.queued().map.remove(&key);
            }
        }
    }
}

// ---- the saved game's queue (player_save.rs's QUEUED operations) and the tick's walk
/// `FSLoadVector<unsigned long>` into `list`: the size, then each element read into the last one's value.
pub fn load_list<F: FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32>(list: &mut Vec<c_ulong>, mut prim: F) -> i32 {
    let mut io = EeUnitsPlayerSaveIo::default();
    let mut result = prim(EE_UNITS_PLAYER_SAVE_READ_ULONG, &mut io);
    let size = io.i as c_ulong;
    let mut element = EeUnitsPlayerSaveIo::default();
    for _ in 0..size {
        result = prim(EE_UNITS_PLAYER_SAVE_READ_ULONG, &mut element);
        list.push(element.i as c_ulong);
    }
    result
}

/// `FSSaveVector<unsigned long>`: the size, then each element until a write fails.
pub fn save_list<F: FnMut(i32, &mut EeUnitsPlayerSaveIo) -> i32>(list: &[c_ulong], mut prim: F) -> i32 {
    let mut result =
        prim(EE_UNITS_PLAYER_SAVE_WRITE_ULONG, &mut EeUnitsPlayerSaveIo { i: list.len() as i64, ..Default::default() });
    for &x in list {
        if result != 0 {
            break;
        }
        result = prim(EE_UNITS_PLAYER_SAVE_WRITE_ULONG, &mut EeUnitsPlayerSaveIo { i: x as i64, ..Default::default() });
    }
    result
}

// ---- the C ABI
/// The C++ host: `(player, op, io) -> the answer`.
pub type EeUnitsPlayerFn = unsafe extern "C" fn(*mut c_void, i32, *mut EeUnitsPlayerIo) -> i64;

/// What a call needs: the player (the host's context), the host, the records and the statistics.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeUnitsPlayerRefs {
    pub player: *mut c_void,
    pub host: EeUnitsPlayerFn,
    pub saved: *mut EeUnitsPlayerSaved,
    pub fields: *mut EeUnitsPlayerFields,
    pub stats: *const EeUnitsPlayerStats,
}

pub(crate) struct FnHost {
    player: *mut c_void,
    f: EeUnitsPlayerFn,
}

impl PlayerHost for FnHost {
    fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64 {
        // SAFETY: the C++ callback's contract
        unsafe { (self.f)(self.player, op, io) }
    }
}

/// Runs `body` over the player of `refs`.
///
/// # Safety
/// `refs` and its pointers are live for the call; the host handles every operation.
pub(crate) unsafe fn with<R>(refs: *const EeUnitsPlayerRefs, body: impl FnOnce(&mut Player<FnHost>) -> R) -> R {
    // SAFETY: the caller's contract
    let r = unsafe { *refs };
    let mut h = FnHost { player: r.player, f: r.host };
    body(&mut Player { h: &mut h, saved: r.saved, fields: r.fields, stats: r.stats })
}

/// The cost decisions: `what` 0 `CanAffordScaled(node, a, f, b)`, 1 `CanAffordSingleResource(node, a, b)`, 2
/// `EEPlayer::CanAfford(node, a, b)`, 3 `CanAffordTechnology(a, f, reason)`, 4 `EEPlayer::Pay(node, a, f)`, 5
/// `RefundTechnology(node, a, f)`.
///
/// # Safety
/// As [`with`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_cost(
    refs: *const EeUnitsPlayerRefs,
    what: u32,
    node: *mut c_void,
    a: c_long,
    b: c_long,
    f: f32,
    reason: *mut c_void,
) -> bool {
    let node = node as i64;
    // SAFETY: the caller's contract
    unsafe {
        with(refs, |p| match what {
            0 => p.can_afford_scaled(node, a, f, b),
            1 => p.can_afford_single(node, a, b),
            2 => p.can_afford(node, a, b),
            3 => p.can_afford_technology(a, f, reason as i64),
            4 => {
                p.pay(node, a, f);
                false
            }
            _ => {
                p.refund_technology(node, a, f);
                false
            }
        })
    }
}

/// `EEPlayer::CanProduce` (see [`Player::can_produce`]).
///
/// # Safety
/// As [`with`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_can_produce(
    refs: *const EeUnitsPlayerRefs,
    tech: c_long,
    unit_class: c_long,
    count: c_long,
    check_cost: bool,
    wonder_mode: c_long,
    reason: *mut c_void,
) -> bool {
    // SAFETY: the caller's contract
    unsafe { with(refs, |p| p.can_produce(tech, unit_class, count, check_cost, wonder_mode, reason as i64)) }
}

/// The improvement decisions on unit type `unit_type` and improvement `improvement`: `what` 0
/// `GetImprovementLevel`, 1 `CountQueuedImprovements`, 2 `CanImprove`, 3 `CanBuy(reason)`, 4 `IsImprovementMaxed`,
/// 5 `QueueImprovement(flag front, flag2 paid)`, 6 `RemoveQueuedImprovement(flag)`, 7 `AdvanceImprovement`, 8
/// `PayImprovement`, 9 `RefundImprovement(f)`, 10 `RepairPointers`' requeue (the arguments unread).
///
/// # Safety
/// As [`with`].
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_improvement(
    refs: *const EeUnitsPlayerRefs,
    what: u32,
    unit_type: c_ulong,
    improvement: c_long,
    flag: bool,
    flag2: bool,
    f: f32,
    reason: *mut c_void,
) -> c_long {
    // SAFETY: the caller's contract
    unsafe {
        with(refs, |p| match what {
            0 => p.level(unit_type, improvement),
            1 => p.count_queued(unit_type, improvement),
            2 => c_long::from(p.can_improve(unit_type, improvement)),
            3 => c_long::from(p.can_buy(unit_type, improvement, reason as i64)),
            4 => c_long::from(p.is_improvement_maxed(unit_type, improvement)),
            5 => {
                p.queue_improvement(unit_type, improvement as c_ulong, flag, flag2);
                0
            }
            6 => {
                p.remove_queued(unit_type, improvement, flag);
                0
            }
            7 => c_long::from(p.advance(unit_type)),
            8 => {
                p.pay_improvement(unit_type, improvement);
                0
            }
            9 => {
                p.refund_improvement(unit_type, improvement, f);
                0
            }
            _ => {
                p.requeue();
                0
            }
        })
    }
}

/// A new, empty queue (the constructors').
#[no_mangle]
pub extern "C" fn ee_units_queued_new() -> *mut EeUnitsQueued {
    Box::into_raw(Box::default())
}

/// # Safety
/// `queued` came from [`ee_units_queued_new`] (or is null) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_units_queued_free(queued: *mut EeUnitsQueued) {
    if !queued.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(queued) });
    }
}

/// The queue's own operations on unit type `a`: `what` 0 `EraseQueuedImprovements`, 1 `MoveQueuedImprovements(a, b)`,
/// 2 the portrait's improvement (the result: the first queued, 0x10 none).
///
/// # Safety
/// `queued` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_queued_op(queued: *mut EeUnitsQueued, what: u32, a: c_ulong, b: c_ulong) -> c_ulong {
    // SAFETY: the caller's contract
    let q = unsafe { &mut *queued };
    match what {
        0 => {
            q.map.remove(&a);
        }
        1 => {
            if let Some(entry) = q.map.remove(&a) {
                q.map.insert(b, entry);
            }
        }
        _ => return q.map.get(&a).map_or(0x10, |e| e.1.first().copied().unwrap_or(0x10)),
    }
    0
}

/// The portrait's progress of unit type `a` (its timer; 0 without an entry).
///
/// # Safety
/// `queued` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_queued_timer(queued: *const EeUnitsQueued, a: c_ulong) -> f32 {
    // SAFETY: the caller's contract
    unsafe { (*queued).map.get(&a).map_or(0.0, |e| e.0) }
}

/// `Reset`'s stores into the record (see [`reset_fields`]).
///
/// # Safety
/// `r` points to the record.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_fields_reset(
    r: *mut EeUnitsPlayerFields,
    starting_citizens: c_long,
    mining_limit: c_long,
) {
    // SAFETY: the caller's contract
    unsafe { reset_fields(&mut *r, starting_citizens, mining_limit) };
}

/// TEST ONLY: the capture's digest of a queue (PLST): FNV-1a 64 of the count, then per entry its key, timer, count and
/// improvements as words.
///
/// # Safety
/// `queued` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_queued_digest(queued: *const EeUnitsQueued) -> u64 {
    // SAFETY: the caller's contract
    let q = unsafe { &*queued };
    let mut h: u64 = 0xcbf29ce484222325;
    let mut word = |w: u32| {
        for b in w.to_le_bytes() {
            h = (h ^ u64::from(b)).wrapping_mul(0x100000001b3);
        }
    };
    word(q.map.len() as u32);
    for (&key, (time, list)) in &q.map {
        word(key as u32);
        word(time.to_bits());
        word(list.len() as u32);
        for &x in list {
            word(x as u32);
        }
    }
    h
}

#[cfg(test)]
#[path = "player_tests.rs"]
mod tests;
