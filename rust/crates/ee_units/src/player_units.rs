//! The player's unit bookkeeping and wonders (`EEPlayer`, EEPlayer.cpp; the player lane, round 2): the counts a
//! player keeps of its units as the C++ bodies in their order over the player's host ([`PlayerHost`], the C++
//! `EEPlayerHost`'s operations 25 on):
//!
//! - `AddUnit`, `UnitPlaced`, `UnitTaken`, `UnitCompleted`: the population, the end-of-game count (the vital
//!   units), the vital buildings (the camouflage power on the first, off at the last), the capitols (the capitol
//!   technologies' allowed flags on the first and the last), the heroes, the teleporters, the wonders, and the
//!   world's parts (towns, herds, walls, the exploration power's sight pulse, the statistics) as host calls;
//! - `RecountPopulation`, `RecountHeroesAndTeleporters`, `GetTotalPopulation`, `GetNumHeroes`;
//! - the wonders: `AddWonder`, `RemoveWonder` (the count per wonder type, [`EeUnitsPlayerWonders`]), the wonder
//!   powers (`AddWonderPower`, `RemoveWonderPower`: the powers a player holds, the Rust list that was the C++
//!   `mWonderPowers`), and `EEWorld::ApplyShareBonus` / `RemoveShareBonus` over every player's powers and
//!   stances ([`share_balance`]).
//!
//! Each unit read is the host's [`EeUnitsPlayerUnit`] of the unit at the statement that reads it (taken again after
//! every host call that may change a unit), so the reads are the original's at their places. The virtuals the
//! computer player overrides (`UnitPlaced`, `SuggestRecomputeAllAreaEffects`, `WonderStarted`) are host calls.
//!
//! Defined where the original was not (no game reached them): a wonder removed without a complex unit, a wall
//! without one, a missing unit set (none of its classes), a missing tech node (no flag set), a wonder power
//! removed that the list does not hold (nothing erased; `std::vector::erase(end())` before).
//!
//! Determinism: the original's integer arithmetic (wrapping `long`s) and its float products in order
//! (`(factor + 1.0f) * amount`, `bonus * (float)limit` truncated as `cvttss2si`); the type counts in a `BTreeMap`
//! (`std::map`'s order; nothing walks it); no random draw.

#![allow(non_snake_case)]
#![allow(clippy::unnecessary_cast)]

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use crate::player::{trunc_long, with, EeUnitsPlayerFields, EeUnitsPlayerRefs, Player, PlayerHost};
use crate::player_save::EeUnitsPlayerSaved;
use crate::player_tick::EE_UNITS_PLAYER_HEAL_INTERVAL;

/// The wonders' Rust state: the count of the player's wonders per unit type (`mWonderTypeCounts`) and the powers
/// they grant, in the order granted (`mWonderPowers`: a power once per wonder type that grants it).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerWonders {
    pub counts: BTreeMap<c_long, c_long>,
    pub powers: Vec<c_long>,
}

/// One unit as the bookkeeping reads it (`EE_UNITS_PLAYER_UNIT_INFO`): its class's members, its own, and its
/// complex unit's (zeros without one).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsPlayerUnit {
    /// `GetComplexUnit()` (0: none)
    pub complex: u64,
    /// the class (`mUnitClass`): `mUnitType`, `mTechnology`, `GetPopulationCost()`, `mCategory`, `mAIKind`,
    /// `mWonderBonus`, `mWonderUnitSet`
    pub unit_type: c_ulong,
    pub technology: c_long,
    pub population: c_long,
    pub category: c_ulong,
    pub ai_kind: c_ulong,
    pub wonder_bonus: c_long,
    pub wonder_unit_set: c_long,
    /// the class's `IsLandBuilding()`, `mIsVital`, `mIsInactive`, `mBlocksTile`, `mIsWall`, `mIsTree`
    pub land_building: bool,
    pub vital: bool,
    pub inactive: bool,
    pub blocks_tile: bool,
    pub wall: bool,
    pub tree: bool,
    /// the unit's `mTileNode != NULL`
    pub on_tile: bool,
    /// the complex unit's `mIsBuilt` and `mPreviousPart == NULL`
    pub built: bool,
    pub first_part: bool,
    /// the unit's `mHitPoints`, `mTileX`, `mResourceAmount`
    pub hit_points: c_long,
    pub tile_x: c_long,
    pub resource_amount: c_long,
    /// the complex unit's container's class category (-1: no container), `mCurrentResearch`, `GetBuildQueueSize()`
    pub container_category: c_long,
    pub current_research: c_long,
    pub build_queue: c_long,
}

// ---- the host's operations (the player's host, after player.rs's)
/// unit `i[0]`'s [`EeUnitsPlayerUnit`] into the record at `i[1]`
pub const EE_UNITS_PLAYER_UNIT_INFO: i32 = 25;
/// the unit after `i[0]` in the player's list (`mFirstUnit` for 0; the result: the unit, 0 at the end)
pub const EE_UNITS_PLAYER_NEXT_UNIT: i32 = 26;
/// `GetNextComplexUnit(i[0])`
pub const EE_UNITS_PLAYER_NEXT_COMPLEX: i32 = 27;
/// the game's state as bits (`EE_UNITS_PLAYER_STATE_*`)
pub const EE_UNITS_PLAYER_STATE: i32 = 28;
/// `RTSPlayer::AddUnit(i[0], i[1])`
pub const EE_UNITS_PLAYER_RTS_ADD_UNIT: i32 = 29;
/// `gWorld.ActivateUnit(i[0])`
pub const EE_UNITS_PLAYER_ACTIVATE: i32 = 30;
/// node `i[0]`'s `mIsAvailable` (the member, not the virtual)
pub const EE_UNITS_PLAYER_NODE_IS_AVAILABLE: i32 = 31;
/// `mTechnologies->SetNodeAvailable(i[0], i[1])`
pub const EE_UNITS_PLAYER_SET_NODE_AVAILABLE: i32 = 32;
/// `mTechnologies->Research(i[0], NULL)`
pub const EE_UNITS_PLAYER_RESEARCH: i32 = 33;
/// `gWorld.mNumPlayers`
pub const EE_UNITS_PLAYER_NUM_PLAYERS: i32 = 34;
/// `UnitPlaced(i[0])` (virtual)
pub const EE_UNITS_PLAYER_UNIT_PLACED: i32 = 35;
/// `GetTechTree()->Get(kTechCitizen)->Contains(i[0])`
pub const EE_UNITS_PLAYER_CITIZEN_CONTAINS: i32 = 36;
/// technology `i[0]`'s node's `GetIsObsolete()` (virtual)
pub const EE_UNITS_PLAYER_TECH_OBSOLETE: i32 = 37;
/// technology `i[0]`'s node's `mIsAllowed = i[1]`
pub const EE_UNITS_PLAYER_TECH_ALLOW: i32 = 38;
/// `gPhysics->GetUnitSwapKind(i[0])`
pub const EE_UNITS_PLAYER_SWAP_KIND: i32 = 39;
/// `EEAddDeadUnitStatistic(i[0])` (the series, 0: none)
pub const EE_UNITS_PLAYER_DEAD_STATISTIC: i32 = 40;
/// `mSavedGroups.Reset(i[0], i[1])`
pub const EE_UNITS_PLAYER_STATS_RESET: i32 = 41;
/// unit `i[0]`'s `IsFoundation()` (virtual)
pub const EE_UNITS_PLAYER_FOUNDATION: i32 = 42;
/// `AddUnitToTown(i[0])`
pub const EE_UNITS_PLAYER_ADD_TO_TOWN: i32 = 43;
/// `AddUnitToHerd(i[0])`
pub const EE_UNITS_PLAYER_ADD_TO_HERD: i32 = 44;
/// `gCivPowersManager.HasPower(this, i[0])`
pub const EE_UNITS_PLAYER_CIV_POWER: i32 = 45;
/// the exploration power's pulse: `PulseSight(true, its amount, unit i[0]'s id)`
pub const EE_UNITS_PLAYER_PULSE_EXPLORATION: i32 = 46;
/// `gContinentManager.UpdateWallUnit(i[0], i[1])`
pub const EE_UNITS_PLAYER_UPDATE_WALL: i32 = 47;
/// `EEGameLogUnitLost(this, i[0])` (the game log)
pub const EE_UNITS_PLAYER_LOG_LOST: i32 = 48;
/// unit `i[0]`'s `GetTechTreeNode()`
pub const EE_UNITS_PLAYER_UNIT_NODE: i32 = 49;
/// `RemoveUnitFromTown(i[0])`
pub const EE_UNITS_PLAYER_REMOVE_FROM_TOWN: i32 = 50;
/// `RemoveUnitFromHerd(i[0])`
pub const EE_UNITS_PLAYER_REMOVE_FROM_HERD: i32 = 51;
/// `RemoveConstructionSite(i[0])`
pub const EE_UNITS_PLAYER_REMOVE_SITE: i32 = 52;
/// `SuggestRecomputeAllAreaEffects()` (virtual)
pub const EE_UNITS_PLAYER_RECOMPUTE_AREAS: i32 = 53;
/// a tree's end: its `mResourceAmount` to 0 (when not 0), then `gContinentManager.RemoveForestTile(i[0])`
pub const EE_UNITS_PLAYER_REMOVE_FOREST: i32 = 54;
/// `EnableCamouflagePower()` (`i[0]` 1) or `DisableCamouflagePower()`
pub const EE_UNITS_PLAYER_CAMOUFLAGE: i32 = 55;
/// `i[0]->mPlayer->AddWonder(i[0], false)` (the unit's owner)
pub const EE_UNITS_PLAYER_OWNER_ADD_WONDER: i32 = 56;
/// `gWorld.mPlayers[i[0]]->WonderStarted(i[1])` (virtual) unless that player is this one
pub const EE_UNITS_PLAYER_WONDER_STARTED: i32 = 57;
/// `ClearFootprint(i[0])`
pub const EE_UNITS_PLAYER_CLEAR_FOOTPRINT: i32 = 58;
/// complex unit `i[0]`'s build queue entry `i[1]`'s technology
pub const EE_UNITS_PLAYER_BUILD_QUEUE_TECH: i32 = 59;
/// `mPendingUnits.size()`
pub const EE_UNITS_PLAYER_PENDING_COUNT: i32 = 60;
/// pending unit `i[0]`'s class's `GetPopulationCost()`
pub const EE_UNITS_PLAYER_PENDING_POPULATION: i32 = 61;
/// the wonder list: `i[1]` 1 `mWonders->Add(i[0])`, its timer (`mCloaked`) at the game time, statistic 0x10 up;
/// 0 `mWonders->Remove(i[0])`, its timer 0, statistic 0x10 down
pub const EE_UNITS_PLAYER_WONDER_LIST: i32 = 62;
/// upgrade slot `i[0]`'s unit type (-1: no upgrade, -2: past the last slot)
pub const EE_UNITS_PLAYER_UPGRADE_TYPE: i32 = 63;
/// `gUnitSetManager.GetSet(i[0])->Contains(i[1])` (0 without the set)
pub const EE_UNITS_PLAYER_SET_CONTAINS: i32 = 64;
/// unit type `i[0]`'s game variant factor into `f[0]`, the world's wonder hit point bonus into `f[1]`
pub const EE_UNITS_PLAYER_WONDER_FACTORS: i32 = 65;
/// `EEUpgradeHitPoints(upgrade slot i[0], its unit type, f[0], this)`
pub const EE_UNITS_PLAYER_UPGRADE_HIT_POINTS: i32 = 66;
/// `gWorld.RemoveShareBonus()` (`i[0]` 1) or `gWorld.ApplyShareBonus()`
pub const EE_UNITS_PLAYER_SHARE_BONUS: i32 = 67;
/// player `i[0]`'s (an `EEPlayer*`) `AddSightSharer(this)` (`i[1]` 1) or `RemoveSightSharer(this)`
pub const EE_UNITS_PLAYER_SIGHT_SHARER: i32 = 68;
/// `mHealedUnitSet = gUnitSetManager.GetSet(i[0])` (`i[1]` 1) or NULL
pub const EE_UNITS_PLAYER_HEALED_SET: i32 = 69;
/// `mPlayerID`
pub const EE_UNITS_PLAYER_ID: i32 = 70;
/// `gWorld.mPlayers[i[0]]` (0: none)
pub const EE_UNITS_PLAYER_AT: i32 = 83;

/// `EE_UNITS_PLAYER_STATE`'s bits: `gLoadingSavedGame`, `gGameScreenActive`, `gWorldClearing`,
/// `UShutdownInProgress()`, `gWorld.mVictoryEnabled`, `gUserInterface != NULL`
pub const EE_UNITS_PLAYER_STATE_LOADING: i64 = 1;
pub const EE_UNITS_PLAYER_STATE_GAME_SCREEN: i64 = 2;
pub const EE_UNITS_PLAYER_STATE_CLEARING: i64 = 4;
pub const EE_UNITS_PLAYER_STATE_SHUTDOWN: i64 = 8;
pub const EE_UNITS_PLAYER_STATE_VICTORY: i64 = 16;
pub const EE_UNITS_PLAYER_STATE_INTERFACE: i64 = 32;

/// `kAIKindHero`
const AI_KIND_HERO: c_ulong = 0x1b;
/// `kUnitTypeTeleporter`, `kUnitTypeTownCenter`, `kUnitTypeCapitol`, `kUnitTypeHouse`
const TYPE_TELEPORTER: c_ulong = 0x313;
const TYPE_TOWN_CENTER: c_ulong = 0x23;
const TYPE_CAPITOL: c_ulong = 0x29;
const TYPE_HOUSE: c_ulong = 0x25;
/// `kUnitCategoryGate`, `kUnitCategoryFortress`
const CATEGORY_GATE: c_ulong = 0x19;
const CATEGORY_FORTRESS: c_long = 0x1d;
/// `kTechSettlement`, `kTechTownCenter`, `kTechCapitol`, `kTechNone`
const TECH_SETTLEMENT: i64 = 1028;
const TECH_TOWN_CENTER: i64 = 1025;
const TECH_CAPITOL: i64 = 1053;
const TECH_NONE: c_long = 100_000;
/// `kCivPowerExploration`
const POWER_EXPLORATION: i64 = 0x2c;
/// a tech node's kind: a wonder
const NODE_KIND_WONDER: c_long = 4;
/// a tile x off the map
const OFF_MAP: c_long = -10000;

impl<'a, H: PlayerHost + ?Sized> Player<'a, H> {
    /// unit `unit`'s record now
    fn unit(&mut self, unit: i64) -> EeUnitsPlayerUnit {
        let mut u = EeUnitsPlayerUnit::default();
        self.h.ask(EE_UNITS_PLAYER_UNIT_INFO, &[unit, &mut u as *mut EeUnitsPlayerUnit as i64]);
        u
    }
    fn state(&mut self, bit: i64) -> bool {
        self.h.ask(EE_UNITS_PLAYER_STATE, &[]) & bit != 0
    }
    fn saved_mut<R>(&mut self, f: impl FnOnce(&mut EeUnitsPlayerSaved) -> R) -> R {
        // SAFETY: the player's record for the call; the reference ends before any host call
        f(unsafe { &mut *self.saved })
    }
    fn fields_mut<R>(&mut self, f: impl FnOnce(&mut EeUnitsPlayerFields) -> R) -> R {
        // SAFETY: as saved_mut
        f(unsafe { &mut *self.fields })
    }
    fn wonders<R>(&mut self, f: impl FnOnce(&mut EeUnitsPlayerWonders) -> R) -> R {
        // SAFETY: the record's wonder state, live while the player is (made by its constructors); as saved_mut
        f(unsafe { &mut *(*self.fields).mWonderState })
    }
    fn allow(&mut self, tech: i64, allowed: bool) {
        self.h.ask(EE_UNITS_PLAYER_TECH_ALLOW, &[tech, i64::from(allowed)]);
    }
    fn obsolete(&mut self, tech: i64) -> bool {
        self.h.ask(EE_UNITS_PLAYER_TECH_OBSOLETE, &[tech]) != 0
    }
    fn citizen_contains(&mut self, unit_type: c_ulong) -> bool {
        self.h.ask(EE_UNITS_PLAYER_CITIZEN_CONTAINS, &[unit_type as i64]) != 0
    }
    fn update_wall(&mut self, unit: i64, flag: bool) {
        self.h.ask(EE_UNITS_PLAYER_UPDATE_WALL, &[unit, i64::from(flag)]);
    }
    /// the exploration power's pulse at a new town center or capitol
    fn exploration_pulse(&mut self, unit: i64, unit_type: c_ulong) {
        if self.h.ask(EE_UNITS_PLAYER_CIV_POWER, &[POWER_EXPLORATION]) != 0
            && (unit_type == TYPE_TOWN_CENTER || unit_type == TYPE_CAPITOL)
        {
            self.h.ask(EE_UNITS_PLAYER_PULSE_EXPLORATION, &[unit]);
        }
    }
    fn is_wonder_node(&mut self, node: i64) -> bool {
        node != 0 && self.node_info(node).kind == NODE_KIND_WONDER
    }

    /// `EEPlayer::AddUnit`: the population, `RTSPlayer::AddUnit`, the activation, the unit's technology made
    /// available for a moment (researched when the unit is built), `UnitPlaced`, the vital units, the capitols (the
    /// first one allows the settlement or town center and no further capitol) and the dead units' statistic.
    pub fn add_unit(&mut self, x: i64, keep_id: bool) {
        let u = self.unit(x);
        let complex = u.complex;
        self.saved_mut(|s| s.mPopulationCount = s.mPopulationCount.wrapping_add(u.population));
        self.h.ask(EE_UNITS_PLAYER_RTS_ADD_UNIT, &[x, i64::from(keep_id)]);
        if complex != 0 {
            let c = self.unit(complex as i64);
            if (!c.inactive || c.hit_points <= 0) && c.first_part && c.tile_x != OFF_MAP {
                self.h.ask(EE_UNITS_PLAYER_ACTIVATE, &[complex as i64]);
            }
        }
        let tech = self.unit(x).technology;
        let node = self.node(tech);
        let was_unavailable = node != 0 && self.h.ask(EE_UNITS_PLAYER_NODE_IS_AVAILABLE, &[node]) == 0;
        self.h.ask(EE_UNITS_PLAYER_SET_NODE_AVAILABLE, &[tech as i64, 1]);
        let u = self.unit(x);
        if u.complex != 0 && u.built && self.h.ask(EE_UNITS_PLAYER_NUM_PLAYERS, &[]) > 0 {
            self.h.ask(EE_UNITS_PLAYER_RESEARCH, &[tech as i64]);
        }
        if was_unavailable {
            self.h.ask(EE_UNITS_PLAYER_SET_NODE_AVAILABLE, &[tech as i64, 0]);
        }
        self.h.ask(EE_UNITS_PLAYER_UNIT_PLACED, &[x]);
        let u = self.unit(x);
        if !u.land_building && u.vital && (u.complex == 0 || u.first_part) {
            self.saved_mut(|s| s.mEndOfGameCount = s.mEndOfGameCount.wrapping_add(1));
        }
        if self.citizen_contains(u.unit_type) {
            let capitols = self.saved_mut(|s| {
                s.mNumCapitols = s.mNumCapitols.wrapping_add(1);
                s.mNumCapitols
            });
            if capitols == 1 && !self.state(EE_UNITS_PLAYER_STATE_SHUTDOWN) {
                self.h.ask(crate::player::EE_UNITS_PLAYER_UPDATE_JIT, &[]);
                if self.obsolete(TECH_SETTLEMENT) {
                    self.allow(TECH_TOWN_CENTER, true);
                } else {
                    self.allow(TECH_SETTLEMENT, true);
                    self.allow(TECH_TOWN_CENTER, false);
                }
                self.allow(TECH_CAPITOL, false);
            }
        }
        let u = self.unit(x);
        if u.complex == 0 || self.h.ask(EE_UNITS_PLAYER_SWAP_KIND, &[u.complex as i64]) == 3 {
            return;
        }
        let series = self.h.ask(EE_UNITS_PLAYER_DEAD_STATISTIC, &[x]);
        if series != 0 {
            self.h.ask(EE_UNITS_PLAYER_STATS_RESET, &[series, 1]);
        }
    }

    /// `EEPlayer::UnitPlaced` (outside a load, on the game screen): the heroes and teleporters, the town and the
    /// herd, the exploration pulse, the walls.
    pub fn unit_placed(&mut self, x: i64) {
        if self.state(EE_UNITS_PLAYER_STATE_LOADING) || !self.state(EE_UNITS_PLAYER_STATE_GAME_SCREEN) {
            return;
        }
        let u = self.unit(x);
        if u.complex != 0 {
            if u.ai_kind == AI_KIND_HERO {
                self.fields_mut(|f| f.mNumHeroes = f.mNumHeroes.wrapping_add(1));
            }
            if u.unit_type == TYPE_TELEPORTER {
                self.fields_mut(|f| f.mNumTeleporters = f.mNumTeleporters.wrapping_add(1));
            }
            if self.h.ask(EE_UNITS_PLAYER_FOUNDATION, &[x]) == 0 {
                self.h.ask(EE_UNITS_PLAYER_ADD_TO_TOWN, &[u.complex as i64]);
            }
            self.h.ask(EE_UNITS_PLAYER_ADD_TO_HERD, &[u.complex as i64]);
            let unit_type = self.unit(x).unit_type;
            self.exploration_pulse(x, unit_type);
        }
        if self.h.ask(EE_UNITS_PLAYER_FOUNDATION, &[x]) == 0 {
            let u = self.unit(x);
            if u.blocks_tile || u.category == CATEGORY_GATE {
                self.update_wall(x, false);
            }
        }
    }

    /// `EEPlayer::UnitTaken`: the game log (unless swapped), the population, the vital units, the wonder, the vital
    /// buildings, the town, the herd, the heroes and teleporters, the construction site, the capitols (the last one
    /// allows a capitol again), the houses' area effects, the walls and the forest.
    pub fn unit_taken(&mut self, x: i64, swapped: bool) {
        if !swapped {
            self.h.ask(EE_UNITS_PLAYER_LOG_LOST, &[x]);
        }
        let u = self.unit(x);
        self.saved_mut(|s| s.mPopulationCount = s.mPopulationCount.wrapping_sub(u.population));
        if !u.land_building || u.complex == 0 || u.built {
            if u.vital && (u.complex == 0 || u.first_part && (!u.land_building || u.built)) {
                self.saved_mut(|s| s.mEndOfGameCount = s.mEndOfGameCount.wrapping_sub(1));
            }
            let node = self.h.ask(EE_UNITS_PLAYER_UNIT_NODE, &[x]);
            if self.is_wonder_node(node) && u.complex != 0 {
                self.remove_wonder(u.complex as i64);
            }
            let u = self.unit(x);
            if u.land_building && u.vital {
                let left = self.saved_mut(|s| {
                    s.mNumVitalBuildings = s.mNumVitalBuildings.wrapping_sub(1);
                    s.mNumVitalBuildings
                });
                if left == 0 {
                    self.h.ask(EE_UNITS_PLAYER_CAMOUFLAGE, &[0]);
                }
            }
        }
        let u = self.unit(x);
        if u.complex != 0 && !self.state(EE_UNITS_PLAYER_STATE_CLEARING) {
            self.h.ask(EE_UNITS_PLAYER_REMOVE_FROM_TOWN, &[u.complex as i64]);
        }
        let u = self.unit(x);
        if u.complex != 0 {
            self.h.ask(EE_UNITS_PLAYER_REMOVE_FROM_HERD, &[u.complex as i64]);
        }
        let u = self.unit(x);
        if u.complex != 0 && self.unit(u.complex as i64).ai_kind == AI_KIND_HERO {
            self.fields_mut(|f| {
                if f.mNumHeroes > 0 {
                    f.mNumHeroes -= 1;
                }
            });
        }
        if u.unit_type == TYPE_TELEPORTER {
            self.fields_mut(|f| f.mNumTeleporters = f.mNumTeleporters.wrapping_sub(1));
        }
        if u.wall && u.on_tile && u.complex != 0 && !u.built {
            self.h.ask(EE_UNITS_PLAYER_REMOVE_SITE, &[u.complex as i64]);
        }
        let u = self.unit(x);
        if self.citizen_contains(u.unit_type) {
            let capitols = self.saved_mut(|s| {
                s.mNumCapitols = s.mNumCapitols.wrapping_sub(1);
                s.mNumCapitols
            });
            if capitols == 0 {
                if self.state(EE_UNITS_PLAYER_STATE_INTERFACE) {
                    self.h.ask(crate::player::EE_UNITS_PLAYER_UPDATE_JIT, &[]);
                }
                self.allow(TECH_CAPITOL, true);
                if self.obsolete(TECH_SETTLEMENT) {
                    self.allow(TECH_TOWN_CENTER, false);
                } else {
                    self.allow(TECH_SETTLEMENT, false);
                }
            }
        }
        let u = self.unit(x);
        if u.unit_type == TYPE_HOUSE {
            self.h.ask(EE_UNITS_PLAYER_RECOMPUTE_AREAS, &[]);
        }
        let u = self.unit(x);
        if !(!self.state(EE_UNITS_PLAYER_STATE_CLEARING) && (u.blocks_tile || u.category == CATEGORY_GATE)) {
            return;
        }
        self.update_wall(x, true);
        if !self.unit(x).tree || !self.state(EE_UNITS_PLAYER_STATE_GAME_SCREEN) {
            return;
        }
        self.h.ask(EE_UNITS_PLAYER_REMOVE_FOREST, &[x]);
    }

    /// `EEPlayer::UnitCompleted`: a wonder (its owner's, the other players told when victory is on), the vital
    /// units and buildings, the exploration pulse, the footprint, the houses' area effects, a wall's construction
    /// site, then (outside a load, on the game screen) the town and the walls.
    pub fn unit_completed(&mut self, c: i64) {
        let tech = self.unit(c).technology;
        let node = self.node(tech);
        if self.is_wonder_node(node) {
            self.h.ask(EE_UNITS_PLAYER_OWNER_ADD_WONDER, &[c]);
            if self.state(EE_UNITS_PLAYER_STATE_VICTORY) {
                let mut i = 0;
                while i < self.h.ask(EE_UNITS_PLAYER_NUM_PLAYERS, &[]) {
                    self.h.ask(EE_UNITS_PLAYER_WONDER_STARTED, &[i, c]);
                    i += 1;
                }
            }
        }
        let u = self.unit(c);
        if u.land_building && u.first_part && u.vital {
            self.saved_mut(|s| s.mEndOfGameCount = s.mEndOfGameCount.wrapping_add(1));
        }
        if u.land_building && u.vital {
            let count = self.saved_mut(|s| {
                s.mNumVitalBuildings = s.mNumVitalBuildings.wrapping_add(1);
                s.mNumVitalBuildings
            });
            if count == 1 {
                self.h.ask(EE_UNITS_PLAYER_CAMOUFLAGE, &[1]);
            }
        }
        let unit_type = self.unit(c).unit_type;
        self.exploration_pulse(c, unit_type);
        self.h.ask(EE_UNITS_PLAYER_CLEAR_FOOTPRINT, &[c]);
        if self.unit(c).unit_type == TYPE_HOUSE {
            self.h.ask(EE_UNITS_PLAYER_RECOMPUTE_AREAS, &[]);
        }
        if self.unit(c).wall {
            self.h.ask(EE_UNITS_PLAYER_REMOVE_SITE, &[c]);
        }
        if self.state(EE_UNITS_PLAYER_STATE_LOADING) || !self.state(EE_UNITS_PLAYER_STATE_GAME_SCREEN) {
            return;
        }
        self.h.ask(EE_UNITS_PLAYER_ADD_TO_TOWN, &[c]);
        let u = self.unit(c);
        if u.blocks_tile || u.category == CATEGORY_GATE {
            self.update_wall(c, u.category == CATEGORY_GATE);
        }
    }

    /// `EEPlayer::RecountPopulation`: over the living complex units, the population (but the units in a fortress),
    /// the end-of-game count (the first parts of the vital units, the buildings built) and the vital buildings.
    pub fn recount_population(&mut self) {
        let (mut population, mut count, mut buildings): (c_long, c_long, c_long) = (0, 0, 0);
        let mut x = self.h.ask(EE_UNITS_PLAYER_NEXT_COMPLEX, &[0]);
        while x != 0 {
            let u = self.unit(x);
            if u.hit_points > 0 {
                if u.container_category != CATEGORY_FORTRESS {
                    population = population.wrapping_add(u.population);
                }
                if u.vital && u.first_part && (!u.land_building || u.built) {
                    count = count.wrapping_add(1);
                }
                if u.land_building && u.vital {
                    buildings = buildings.wrapping_add(1);
                }
            }
            x = self.h.ask(EE_UNITS_PLAYER_NEXT_COMPLEX, &[x]);
        }
        self.saved_mut(|s| {
            s.mPopulationCount = population;
            s.mEndOfGameCount = count;
            s.mNumVitalBuildings = buildings;
        });
    }

    /// a hero technology: the node of `tech` makes a unit class that is a hero
    fn makes_hero(&mut self, tech: c_long) -> bool {
        let node = self.node(tech);
        if node == 0 {
            return false;
        }
        let unit_class = self.node_info(node).unit_class;
        unit_class > 0 && self.class(unit_class).is_some_and(|c| c.hero)
    }

    /// `EEPlayer::RecountHeroesAndTeleporters`: over the living complex units, the heroes, the teleporters and the
    /// heroes being researched or queued.
    pub fn recount_heroes_and_teleporters(&mut self) {
        self.fields_mut(|f| {
            f.mNumHeroes = 0;
            f.mNumHeroesInProduction = 0;
            f.mNumTeleporters = 0;
        });
        let mut x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[0]);
        while x != 0 {
            let u = self.unit(x);
            let c = if u.complex != 0 { self.unit(u.complex as i64) } else { EeUnitsPlayerUnit::default() };
            if u.complex != 0 && c.hit_points > 0 {
                if c.ai_kind == AI_KIND_HERO {
                    self.fields_mut(|f| f.mNumHeroes = f.mNumHeroes.wrapping_add(1));
                }
                if u.unit_type == TYPE_TELEPORTER {
                    self.fields_mut(|f| f.mNumTeleporters = f.mNumTeleporters.wrapping_add(1));
                }
                if c.current_research != TECH_NONE && self.makes_hero(c.current_research) {
                    self.fields_mut(|f| f.mNumHeroesInProduction = f.mNumHeroesInProduction.wrapping_add(1));
                }
                let mut i = c.build_queue - 1;
                while i >= 0 {
                    let tech = self.h.ask(EE_UNITS_PLAYER_BUILD_QUEUE_TECH, &[u.complex as i64, i as i64]) as c_long;
                    if self.makes_hero(tech) {
                        self.fields_mut(|f| f.mNumHeroesInProduction = f.mNumHeroesInProduction.wrapping_add(1));
                    }
                    i -= 1;
                }
            }
            x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[x]);
        }
    }

    /// `EEPlayer::GetTotalPopulation`: the population, with `pending` the pending units', with `garrisoned` the
    /// units in a fortress.
    pub fn total_population(&mut self, pending: bool, garrisoned: bool) -> c_long {
        // SAFETY: the player's record (a field read)
        let mut result = unsafe { (*self.saved).mPopulationCount };
        if pending {
            let mut i = 0;
            while i < self.h.ask(EE_UNITS_PLAYER_PENDING_COUNT, &[]) {
                result = result.wrapping_add(self.h.ask(EE_UNITS_PLAYER_PENDING_POPULATION, &[i]) as c_long);
                i += 1;
            }
        }
        if !garrisoned {
            return result;
        }
        let mut x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[0]);
        while x != 0 {
            let u = self.unit(x);
            if u.complex != 0 && u.container_category == CATEGORY_FORTRESS {
                result = result.wrapping_add(u.population);
            }
            x = self.h.ask(EE_UNITS_PLAYER_NEXT_UNIT, &[x]);
        }
        result
    }

    /// `EEPlayer::GetNumHeroes`: the living heroes and/or those in production.
    pub fn num_heroes(&mut self, living: bool, in_production: bool) -> c_long {
        // SAFETY: the player's record (a copy)
        let f = unsafe { *self.fields };
        let mut result = 0;
        if living {
            result = f.mNumHeroes;
        }
        if in_production {
            result = result.wrapping_add(f.mNumHeroesInProduction);
        }
        result
    }

    /// `EEPlayer::AddWonder`: the wonder listed (its timer started) unless loading; the first of its type grants its
    /// power.
    pub fn add_wonder(&mut self, x: i64, loading: bool) {
        if !loading {
            self.h.ask(EE_UNITS_PLAYER_WONDER_LIST, &[x, 1]);
        }
        let unit_type = self.unit(x).unit_type as c_long;
        let first = self.wonders(|w| match w.counts.get_mut(&unit_type) {
            None => {
                w.counts.insert(unit_type, 1);
                true
            }
            Some(n) => {
                *n = n.wrapping_add(1);
                false
            }
        });
        if first {
            self.add_wonder_power(x);
        }
    }

    /// `EEPlayer::RemoveWonder`: the wonder off the list; the last of its type takes its power away. (A type never
    /// counted goes to -1 and stays, as `operator[]`'s did.)
    pub fn remove_wonder(&mut self, x: i64) {
        self.h.ask(EE_UNITS_PLAYER_WONDER_LIST, &[x, 0]);
        let unit_type = self.unit(x).unit_type as c_long;
        let last = self.wonders(|w| {
            let n = w.counts.entry(unit_type).or_insert(0);
            *n = n.wrapping_sub(1);
            *n == 0
        });
        if last {
            self.remove_wonder_power(x);
            self.wonders(|w| w.counts.remove(&unit_type));
        }
    }

    fn take_power(&mut self, power: c_long) {
        self.wonders(|w| {
            if let Some(i) = w.powers.iter().position(|&p| p == power) {
                w.powers.remove(i);
            }
        });
    }

    /// the hit point bonus of a wonder over its unit set's upgrades: `(factor + 1) * amount`, negated to take it away
    fn wonder_hit_points(&mut self, set: c_long, take: bool) {
        let mut i: i64 = 0;
        loop {
            let unit_type = self.h.ask(EE_UNITS_PLAYER_UPGRADE_TYPE, &[i]);
            if unit_type == -2 {
                break;
            }
            if unit_type >= 0 && self.h.ask(EE_UNITS_PLAYER_SET_CONTAINS, &[set as i64, unit_type]) != 0 {
                let (_, io) = self.h.info(EE_UNITS_PLAYER_WONDER_FACTORS, &[unit_type]);
                let value = (io.f[0] + 1.0) * io.f[1];
                let mut call = crate::player::EeUnitsPlayerIo::default();
                call.i[0] = i;
                call.f[0] = if take { -value } else { value };
                self.h.call(EE_UNITS_PLAYER_UPGRADE_HIT_POINTS, &mut call);
            }
            i += 1;
        }
    }

    /// each other player this player is at war with (stance 1), in id order from 1, shares its sight (`add`) or stops
    fn sight_sharers(&mut self, add: bool) {
        let me = self.h.ask(EE_UNITS_PLAYER_ID, &[]);
        let mut p: i64 = 1;
        while p < self.h.ask(EE_UNITS_PLAYER_NUM_PLAYERS, &[]) {
            // SAFETY: the player's record (a field read)
            let enemy = usize::try_from(p).ok().and_then(|q| unsafe { (*self.saved).mEnemies.get(q).copied() });
            if p != me {
                let player = self.h.ask(EE_UNITS_PLAYER_AT, &[p]);
                if player != 0 && enemy == Some(1) {
                    self.h.ask(EE_UNITS_PLAYER_SIGHT_SHARER, &[player, i64::from(add)]);
                }
            }
            p += 1;
        }
    }

    /// `EEPlayer::AddWonderPower`: by the class's wonder bonus, area conversion (1), the unit set's hit points (2),
    /// power 3, the population share (4, the world's share bonus taken off and given again around it), the
    /// enemies' sight (5: each enemy shares its buildings' sight with this player), the unit set's healing (6).
    pub fn add_wonder_power(&mut self, x: i64) {
        let u = self.unit(x);
        let push = |p: &mut Self, power: c_long| p.wonders(|w| w.powers.push(power));
        match u.wonder_bonus {
            1 => {
                push(self, 1);
                self.fields_mut(|f| f.mAreaConversion = true);
            }
            2 => {
                push(self, 2);
                self.wonder_hit_points(u.wonder_unit_set, false);
            }
            3 => push(self, 3),
            4 => {
                self.h.ask(EE_UNITS_PLAYER_SHARE_BONUS, &[1]);
                push(self, 4);
                self.h.ask(EE_UNITS_PLAYER_SHARE_BONUS, &[0]);
            }
            5 => {
                push(self, 5);
                self.sight_sharers(true);
            }
            6 => {
                self.fields_mut(|f| f.mHealsUnits = true);
                self.saved_mut(|s| s.mHealTimer = EE_UNITS_PLAYER_HEAL_INTERVAL);
                self.h.ask(EE_UNITS_PLAYER_HEALED_SET, &[u.wonder_unit_set as i64, 1]);
                push(self, 6);
            }
            _ => {}
        }
    }

    /// `EEPlayer::RemoveWonderPower`: [`Self::add_wonder_power`] undone.
    pub fn remove_wonder_power(&mut self, x: i64) {
        let u = self.unit(x);
        match u.wonder_bonus {
            1 => {
                self.take_power(1);
                self.fields_mut(|f| f.mAreaConversion = false);
            }
            2 => {
                self.take_power(2);
                self.wonder_hit_points(u.wonder_unit_set, true);
            }
            3 => self.take_power(3),
            4 => {
                self.h.ask(EE_UNITS_PLAYER_SHARE_BONUS, &[1]);
                self.take_power(4);
                self.h.ask(EE_UNITS_PLAYER_SHARE_BONUS, &[0]);
            }
            5 => {
                self.take_power(5);
                self.sight_sharers(false);
            }
            6 => {
                self.fields_mut(|f| f.mHealsUnits = false);
                self.h.ask(EE_UNITS_PLAYER_HEALED_SET, &[0, 0]);
                self.take_power(6);
            }
            _ => {}
        }
    }
}

/// One player as the share bonus sees it: whether it holds power 4 and its stances.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ShareView {
    pub holds: bool,
    pub enemies: [c_ulong; 17],
}

/// `EEWorld::ApplyShareBonus`'s balance for player `i` of `players` (indexed by player id; `None`: no player): 1
/// when it holds power 4 and no enemy (stance 1) holds it, or it holds none and only allies (stance 0) hold it; -1
/// when it holds none and only enemies hold it; else 0.
pub fn share_balance(players: &[Option<ShareView>], i: usize) -> i32 {
    let Some(me) = players[i] else { return 0 };
    let stance = |j: usize| me.enemies.get(j).copied().unwrap_or(0);
    if me.holds {
        let found = (1..players.len()).any(|j| j != i && players[j].is_some_and(|o| stance(j) == 1 && o.holds));
        return if found { 0 } else { 1 };
    }
    let mut balance = 0;
    for (j, other) in players.iter().enumerate().skip(1) {
        let Some(other) = other else { continue };
        if j == i || !other.holds {
            continue;
        }
        match stance(j) {
            0 => {
                if balance < 0 {
                    return 0;
                }
                balance = 1;
            }
            1 => {
                if balance > 0 {
                    return 0;
                }
                balance = -1;
            }
            _ => {}
        }
    }
    balance
}

/// The share bonus's change for a balance: `bonus * limit` given (balance 1) or `bonus2 * limit` taken (-1), each
/// truncated; reversed to remove it.
pub fn share_change(balance: i32, remove: bool, bonus: f32, bonus2: f32, limit: c_long) -> c_long {
    let amount = match balance {
        1 => trunc_long(bonus * limit as f32),
        -1 => trunc_long(bonus2 * limit as f32).wrapping_neg(),
        _ => 0,
    };
    if remove {
        amount.wrapping_neg()
    } else {
        amount
    }
}

// ---- the C ABI
/// The unit bookkeeping on `unit` (`a`, `b` the flags): `what` 0 `AddUnit(unit, a keep id)`, 1 `UnitPlaced`, 2
/// `UnitTaken(unit, a swapped)`, 3 `UnitCompleted`, 4 `RecountPopulation`, 5 `RecountHeroesAndTeleporters`, 6
/// `GetTotalPopulation(a, b)`, 7 `AddWonder(unit, a loading)`, 8 `RemoveWonder`, 9 `AddWonderPower`, 10
/// `RemoveWonderPower`, 11 `GetNumHeroes(a, b)` (the result: the count asked, else 0).
///
/// # Safety
/// As player.rs's `with`; the record's wonder state is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_units(
    refs: *const EeUnitsPlayerRefs,
    what: u32,
    unit: *mut c_void,
    a: bool,
    b: bool,
) -> c_long {
    let x = unit as i64;
    // SAFETY: the caller's contract
    unsafe {
        with(refs, |p| {
            match what {
                0 => p.add_unit(x, a),
                1 => p.unit_placed(x),
                2 => p.unit_taken(x, a),
                3 => p.unit_completed(x),
                4 => p.recount_population(),
                5 => p.recount_heroes_and_teleporters(),
                6 => return p.total_population(a, b),
                7 => p.add_wonder(x, a),
                8 => p.remove_wonder(x),
                9 => p.add_wonder_power(x),
                10 => p.remove_wonder_power(x),
                _ => return p.num_heroes(a, b),
            }
            0
        })
    }
}

/// `EEWorld::ApplyShareBonus` (`remove` false) and `RemoveShareBonus`: each player from 1 of the `count` in
/// `players` (by id; a null `fields` for no player) has its `mShareBonus` changed by its balance
/// ([`share_balance`], [`share_change`]).
///
/// # Safety
/// `players` holds `count` entries whose records (and the wonder states of the present ones) are live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_share_bonus(
    players: *const EeUnitsPlayerRefs,
    count: u32,
    remove: bool,
    bonus: f32,
    bonus2: f32,
    limit: c_long,
) {
    // SAFETY: the caller's contract
    let refs = unsafe { std::slice::from_raw_parts(players, count as usize) };
    let views: Vec<Option<ShareView>> = refs
        .iter()
        .map(|r| {
            (!r.fields.is_null()).then(|| {
                // SAFETY: the caller's contract (reads)
                unsafe {
                    ShareView { holds: (*(*r.fields).mWonderState).powers.contains(&4), enemies: (*r.saved).mEnemies }
                }
            })
        })
        .collect();
    for i in 1..refs.len() {
        if views[i].is_none() {
            continue;
        }
        let change = share_change(share_balance(&views, i), remove, bonus, bonus2, limit);
        // SAFETY: the caller's contract (a present player's record)
        unsafe { (*refs[i].fields).mShareBonus = (*refs[i].fields).mShareBonus.wrapping_add(change) };
    }
}

/// A new, empty wonder state (the constructors').
#[no_mangle]
pub extern "C" fn ee_units_wonders_new() -> *mut EeUnitsPlayerWonders {
    Box::into_raw(Box::default())
}

/// # Safety
/// `wonders` came from [`ee_units_wonders_new`] (or is null) and is not used after.
#[no_mangle]
pub unsafe extern "C" fn ee_units_wonders_free(wonders: *mut EeUnitsPlayerWonders) {
    if !wonders.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(wonders) });
    }
}

/// Whether the player holds wonder power `power` (`std::find` over `mWonderPowers`; SetDiplomaticStance's test).
///
/// # Safety
/// `wonders` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_units_wonders_count(wonders: *const EeUnitsPlayerWonders, power: c_long) -> u32 {
    // SAFETY: the caller's contract
    unsafe { (*wonders).powers.iter().filter(|&&p| p == power).count() as u32 }
}

#[cfg(test)]
#[path = "player_units_tests.rs"]
mod tests;
