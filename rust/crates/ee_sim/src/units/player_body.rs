//! `EEPlayer`'s bodies over its C++ members (the groups lane, round 2; were EEPlayer.cpp): the destructor, `Reset`,
//! `Clear`, `RepairPointers`, `ExecuteSelectionMessage`, `SetSelectedViewedUnit`, `UnitsSwapped`,
//! `UnselectAllUnits`, `ClearSavedGroup`, `RestorePatrols`, `KillStrandedUnits`, the player effects' and the loaded
//! media's loads and unloads. The groups are the group objects (group_obj.rs), the selection selection.rs's; every
//! other C++ statement is one operation of [`PlayerBodyHost::body`] (the game's: `ee_units_player_body_host`,
//! EEPlayer.cpp, one statement a case), in the original's order.

use crate::units::group_obj::{self as go, EE_UNITS_GROUP_KIND_SAVED, EE_UNITS_GROUP_KIND_SELECTED, G};
use crate::units::player_lists::{
    ListOp, EE_UNITS_LISTS_HERDS, EE_UNITS_LISTS_LOADED_IDS, EE_UNITS_LISTS_MEDIA, EE_UNITS_LISTS_PATROLS,
    EE_UNITS_LISTS_REPATROL, EE_UNITS_LISTS_SITES, EE_UNITS_LISTS_SITE_IDS, EE_UNITS_LISTS_STRANDED,
    EE_UNITS_LISTS_TOWNS, EE_UNITS_LISTS_WONDERS, EE_UNITS_LISTS_WONDER_IDS,
};
use crate::units::selection::{self as sel, SelectionHost};
use std::os::raw::c_long;

// ---- the operations (`op`; `a` an object or index, `b`, `c` values; the answer)
/// `reinterpret_cast<RTSUnit*>(a)->Reference()`
pub const EE_UNITS_PLAYER_BODY_UNIT_REFERENCE: i32 = 1;
/// `reinterpret_cast<RTSUnit*>(a)->Dereference()`
pub const EE_UNITS_PLAYER_BODY_UNIT_DEREFERENCE: i32 = 2;
/// `mActiveGroup = a`
pub const EE_UNITS_PLAYER_BODY_SET_ACTIVE: i32 = 3;
/// `mSelectedGroup = a`
pub const EE_UNITS_PLAYER_BODY_SET_SELECTED: i32 = 4;
/// `mGroups[b] = a`
pub const EE_UNITS_PLAYER_BODY_SET_HOT_KEY: i32 = 5;
/// `gWorld.mExecutingLocalSelection = b != 0`
pub const EE_UNITS_PLAYER_BODY_EXECUTING_LOCAL: i32 = 6;
/// `reinterpret_cast<NEMessage*>(a)->Execute()`
pub const EE_UNITS_PLAYER_BODY_EXECUTE: i32 = 7;
/// `mSelectedViewedUnit` (no reference counted)
pub const EE_UNITS_PLAYER_BODY_SELECTED_VIEWED: i32 = 8;
/// `mSelectedViewedUnit = a`
pub const EE_UNITS_PLAYER_BODY_SET_SELECTED_VIEWED: i32 = 9;
/// `mViewedUnit = a`
pub const EE_UNITS_PLAYER_BODY_SET_VIEWED: i32 = 10;
/// `GetNextComplexUnit(a)` (0: the first)
pub const EE_UNITS_PLAYER_BODY_NEXT_COMPLEX: i32 = 11;
/// complex unit `a`'s `ReplaceUnits(swaps)` (the swaps' vector: `io.out`)
pub const EE_UNITS_PLAYER_BODY_REPLACE_UNITS: i32 = 12;
/// `EEATCReplaceUnits(mAirTraffic[b], swaps)`
pub const EE_UNITS_PLAYER_BODY_ATC_REPLACE: i32 = 13;
/// unit `a` is an unbuilt wall (`mUnitClass->mIsWall && !mIsBuilt`)
pub const EE_UNITS_PLAYER_BODY_UNBUILT_WALL: i32 = 14;
/// `gDoppelgangerManager.UnregisterPlayer(this)`
pub const EE_UNITS_PLAYER_BODY_DOPPELGANGERS: i32 = 17;
/// `mEvents.Clear(b ? this : NULL)`
pub const EE_UNITS_PLAYER_BODY_EVENTS_CLEAR: i32 = 18;
/// `mSavedGroups.Clear()` (the statistics)
pub const EE_UNITS_PLAYER_BODY_STATS_CLEAR: i32 = 20;
/// `mSavedGroups.SetName(GetName())`
pub const EE_UNITS_PLAYER_BODY_STATS_NAME: i32 = 21;
/// `EEUnitStorePlayerTable(this)`
pub const EE_UNITS_PLAYER_BODY_STORE_TABLE: i32 = 22;
/// `EEUnitClassCount()`
pub const EE_UNITS_PLAYER_BODY_CLASS_COUNT: i32 = 23;
/// `mUnitClassUpgrades.resize(b)`
pub const EE_UNITS_PLAYER_BODY_UPGRADES_RESIZE: i32 = 24;
/// `mUnitClassUpgrades[b]` = a new upgrade of `gUnitClassTable[b]` (NULL without the class)
pub const EE_UNITS_PLAYER_BODY_UPGRADE_NEW: i32 = 25;
/// `delete mUnitClassUpgrades.begin()[b]`
pub const EE_UNITS_PLAYER_BODY_UPGRADE_DELETE: i32 = 26;
/// `mUnitClassUpgrades.clear()`
pub const EE_UNITS_PLAYER_BODY_UPGRADES_CLEAR: i32 = 27;
/// `mTechnologies = new EETechTree`
pub const EE_UNITS_PLAYER_BODY_TECH_NEW: i32 = 28;
/// `delete mTechnologies`
pub const EE_UNITS_PLAYER_BODY_TECH_DELETE: i32 = 29;
/// `EEATCNew()`
pub const EE_UNITS_PLAYER_BODY_ATC_NEW: i32 = 30;
/// `mAirTraffic[b] = a`
pub const EE_UNITS_PLAYER_BODY_ATC_SET: i32 = 31;
/// `ee_units_atc_delete(mAirTraffic[b])`
pub const EE_UNITS_PLAYER_BODY_ATC_DELETE: i32 = 32;
/// `ee_units_atc_repair_pointers(mAirTraffic[b])`
pub const EE_UNITS_PLAYER_BODY_ATC_REPAIR: i32 = 33;
/// the world table's long `b`
pub const EE_UNITS_PLAYER_BODY_WORLD_LONG: i32 = 34;
/// `mHealedUnitSet = NULL`
pub const EE_UNITS_PLAYER_BODY_HEALED_CLEAR: i32 = 35;
/// `EEGraphicEffect::Get(b)` as an `EEMedia*`
pub const EE_UNITS_PLAYER_BODY_EFFECT: i32 = 36;
/// media `a`'s `LoadMedia()`
pub const EE_UNITS_PLAYER_BODY_MEDIA_LOAD: i32 = 37;
/// media `a`'s `UnloadMedia()`
pub const EE_UNITS_PLAYER_BODY_MEDIA_UNLOAD: i32 = 38;
/// `ee_units_queued_free(mQueued)`
pub const EE_UNITS_PLAYER_BODY_QUEUED_FREE: i32 = 43;
/// `RTSPlayer::Clear()`
pub const EE_UNITS_PLAYER_BODY_RTS_CLEAR: i32 = 44;
/// `mCivilization.Reapply(this, b, c)`
pub const EE_UNITS_PLAYER_BODY_CIV_REAPPLY: i32 = 45;
/// `RTSPlayer::RepairPointers(b, c)`
pub const EE_UNITS_PLAYER_BODY_RTS_REPAIR: i32 = 46;
/// `EEMakeSavedTowns(this)`
pub const EE_UNITS_PLAYER_BODY_MAKE_SAVED_TOWNS: i32 = 49;
/// `mFirstUnit` (a 0) or unit `a`'s `mNext`
pub const EE_UNITS_PLAYER_BODY_NEXT_UNIT: i32 = 50;
/// `EESaveHasLiveState(b, c)`
pub const EE_UNITS_PLAYER_BODY_LIVE_STATE: i32 = 51;
/// `AddUnitToTown(a)`
pub const EE_UNITS_PLAYER_BODY_ADD_TO_TOWN: i32 = 52;
/// the civilization builder's button is due to blink (points to add)
pub const EE_UNITS_PLAYER_BODY_CIV_POINTS: i32 = 53;
/// `EESetCivBuilderButtonBlinking(true)`
pub const EE_UNITS_PLAYER_BODY_CIV_BLINK: i32 = 54;
/// `RecountPopulation()`
pub const EE_UNITS_PLAYER_BODY_RECOUNT_POPULATION: i32 = 56;
/// herd `a`'s `RepairPointers(b, c)`
pub const EE_UNITS_PLAYER_BODY_HERD_REPAIR: i32 = 58;
/// `RegisterHerds()`
pub const EE_UNITS_PLAYER_BODY_REGISTER_HERDS: i32 = 59;
/// `mEvents.RepairEvents(this, b, c)`
pub const EE_UNITS_PLAYER_BODY_EVENTS_REPAIR: i32 = 60;
/// the queued improvements queued again (`EEPlayerImprovement(this, 10, 0, 0)`)
pub const EE_UNITS_PLAYER_BODY_REQUEUE: i32 = 61;
/// `mNumCapitols`
pub const EE_UNITS_PLAYER_BODY_CAPITOLS: i32 = 62;
/// tech `b`'s `GetIsObsolete()`
pub const EE_UNITS_PLAYER_BODY_TECH_OBSOLETE: i32 = 63;
/// tech `b`'s `mIsAllowed = c != 0`
pub const EE_UNITS_PLAYER_BODY_TECH_ALLOW: i32 = 64;
/// `EEApplySavedTowns(this)`
pub const EE_UNITS_PLAYER_BODY_APPLY_SAVED_TOWNS: i32 = 65;
/// town `a`'s `RefreshPatrolGroup()`
pub const EE_UNITS_PLAYER_BODY_TOWN_REFRESH: i32 = 69;
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_PLAYER_BODY_TILE_X: i32 = 70;
/// `AddOffMapUnit(a)`
pub const EE_UNITS_PLAYER_BODY_ADD_OFF_MAP: i32 = 71;
/// `RecountHeroesAndTeleporters()`
pub const EE_UNITS_PLAYER_BODY_RECOUNT_HEROES: i32 = 72;
/// `UnloadAllMedia()` (virtual)
pub const EE_UNITS_PLAYER_BODY_UNLOAD_ALL_MEDIA: i32 = 75;
/// complex unit `a`'s `mIsDisabled`
pub const EE_UNITS_PLAYER_BODY_DISABLED: i32 = 77;
/// complex unit `a`'s `FindNearestTown()` (an `EETown*`)
pub const EE_UNITS_PLAYER_BODY_NEAREST_TOWN: i32 = 78;
/// `ee_units_pg_nearest(patrol a, b, c)` (an `EEComplexUnit*`)
pub const EE_UNITS_PLAYER_BODY_PATROL_NEAREST: i32 = 79;
/// complex unit `a`'s `mPatrolGroup`
pub const EE_UNITS_PLAYER_BODY_PATROL_OF: i32 = 80;
/// `EEUGRust::NewPatrol(patrol b, point a, true)` (the goal, an `EEUnitGoal*`)
pub const EE_UNITS_PLAYER_BODY_NEW_PATROL_GOAL: i32 = 81;
/// active group `a`'s `KeepMobileUnits()`
pub const EE_UNITS_PLAYER_BODY_KEEP_MOBILE: i32 = 82;
/// active group `a`'s `Plan()`
pub const EE_UNITS_PLAYER_BODY_PLAN: i32 = 83;
/// unit `a`'s `Kill(0, 0, 0)`
pub const EE_UNITS_PLAYER_BODY_KILL: i32 = 84;
/// town `a`'s `mPatrolGroup`
pub const EE_UNITS_PLAYER_BODY_TOWN_PATROL: i32 = 85;
/// `*mTechnologies = gTechTree`
pub const EE_UNITS_PLAYER_BODY_TECH_COPY: i32 = 87;
/// `mTechnologies = NULL`
pub const EE_UNITS_PLAYER_BODY_TECH_NULL: i32 = 88;
/// `ee_units_wonders_free(mWonderState)`
pub const EE_UNITS_PLAYER_BODY_WONDER_STATE_FREE: i32 = 92;
/// `ee_units_lists_free(mLists)`
pub const EE_UNITS_PLAYER_BODY_LISTS_FREE: i32 = 93;
/// `ee_units_player_groups_free(mGroupLists)`
pub const EE_UNITS_PLAYER_BODY_GROUP_LISTS_FREE: i32 = 94;
/// `KillUnit(a, false)`
pub const EE_UNITS_PLAYER_BODY_KILL_UNIT: i32 = 95;

/// unit `a`'s stats' `GetLineOfSight()` (its bits)
pub const EE_UNITS_PLAYER_BODY_LINE_OF_SIGHT: i32 = 100;
/// unit `a`'s complex unit's `SightOffHere()`
pub const EE_UNITS_PLAYER_BODY_COMPLEX_SIGHT_OFF: i32 = 101;
/// unit `a`'s complex unit's `SetSight(b's float bits)`
pub const EE_UNITS_PLAYER_BODY_SET_SIGHT: i32 = 102;
/// unit `a`'s `SightOnHere()`
pub const EE_UNITS_PLAYER_BODY_SIGHT_ON: i32 = 103;
/// unit `a`'s complex unit's `mSight = 0`
pub const EE_UNITS_PLAYER_BODY_SIGHT_ZERO: i32 = 104;
/// `gWorld.mPlayers[gWorld.mCurrentPlayer] == this`
pub const EE_UNITS_PLAYER_BODY_IS_LOCAL: i32 = 105;
/// the main game form's Ctrl+D hot key removed
pub const EE_UNITS_PLAYER_BODY_HOT_KEY_REMOVE: i32 = 106;
/// the map is a waterless one
pub const EE_UNITS_PLAYER_BODY_WATERLESS: i32 = 107;
/// the map is no space map
pub const EE_UNITS_PLAYER_BODY_NOT_SPACE: i32 = 108;
/// the main game form's Ctrl+D hot key: select the units of type `b`
pub const EE_UNITS_PLAYER_BODY_HOT_KEY_ADD: i32 = 109;
/// `gCivPowersManager.HasPower(this, kCivPowerCamouflage)`
pub const EE_UNITS_PLAYER_BODY_HAS_CAMOUFLAGE: i32 = 110;
/// the camouflage power's unit set (an `EEUnitSet*`)
pub const EE_UNITS_PLAYER_BODY_CAMOUFLAGE_SET: i32 = 111;
/// unit set `a`'s `Contains(b)`
pub const EE_UNITS_PLAYER_BODY_SET_HAS: i32 = 112;
/// complex unit `a`'s command's type (0 none)
pub const EE_UNITS_PLAYER_BODY_COMMAND_TYPE: i32 = 113;
/// complex unit `a`'s `mCamouflageTimer = b`
pub const EE_UNITS_PLAYER_BODY_SET_CAMOUFLAGE_TIMER: i32 = 114;
/// complex unit `a`'s `SetCloakableChain(false)`
pub const EE_UNITS_PLAYER_BODY_CLOAKABLE_OFF: i32 = 115;
/// complex unit `a`'s `SetCloakedChain(false)`
pub const EE_UNITS_PLAYER_BODY_CLOAKED_OFF: i32 = 116;
/// complex unit `a`'s `mU32C = 0`
pub const EE_UNITS_PLAYER_BODY_CLEAR_U32C: i32 = 117;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_PLAYER_BODY_TILE_Y: i32 = 118;
/// the upgrade of unit class `b` (`EEPlayerClassStats`: NULL past the classes)
pub const EE_UNITS_PLAYER_BODY_CLASS_STATS: i32 = 121;
/// upgrade `a`'s `mTerrainMask`
pub const EE_UNITS_PLAYER_BODY_TERRAIN_MASK: i32 = 122;
/// `gWorld.GetNumTeams()`
pub const EE_UNITS_PLAYER_BODY_TEAM_COUNT: i32 = 123;
/// `gWorld.mTeams[b].ContainsPlayer(c)`
pub const EE_UNITS_PLAYER_BODY_TEAM_HAS: i32 = 124;
/// upgrade `a`'s `mButtonLoadCount`
pub const EE_UNITS_PLAYER_BODY_BUTTON_LOADS: i32 = 125;
/// `IsHuman()` (virtual)
pub const EE_UNITS_PLAYER_BODY_IS_HUMAN: i32 = 126;
/// button `a`'s `Release()`
pub const EE_UNITS_PLAYER_BODY_BUTTON_RELEASE: i32 = 127;
/// button `a`'s `Acquire()`
pub const EE_UNITS_PLAYER_BODY_BUTTON_ACQUIRE: i32 = 128;
/// `gWorld.mMap->mSize`
pub const EE_UNITS_PLAYER_BODY_MAP_SIZE: i32 = 129;
/// `gWorld.mMap->GetTile(b, c) != NULL`
pub const EE_UNITS_PLAYER_BODY_TILE_PRESENT: i32 = 130;
/// tile (b, c)'s visibility for the player into the terrain mesh
pub const EE_UNITS_PLAYER_BODY_TILE_VISIBILITY: i32 = 131;
/// `mDamageFrom[b] += c`
pub const EE_UNITS_PLAYER_BODY_DAMAGE_ADD: i32 = 133;
/// `gWorld.mPlayers[b] == this`
pub const EE_UNITS_PLAYER_BODY_IS_SELF: i32 = 134;
/// `mEnemies[gWorld.mPlayers[b]->mPlayerID]`
pub const EE_UNITS_PLAYER_BODY_ENEMY_OF: i32 = 135;
/// `mCombatState = b`
pub const EE_UNITS_PLAYER_BODY_SET_COMBAT: i32 = 136;
/// `mTechnologies->SetPlayerIndex(b)`
pub const EE_UNITS_PLAYER_BODY_TECH_SET_INDEX: i32 = 137;
/// `mTechnologies->SetPlayer(this)`
pub const EE_UNITS_PLAYER_BODY_TECH_SET_PLAYER: i32 = 138;
/// `mBB44 = false`
pub const EE_UNITS_PLAYER_BODY_BB44_CLEAR: i32 = 139;
/// `mPopulationLimitBonus = 0`
pub const EE_UNITS_PLAYER_BODY_POPULATION_BONUS_CLEAR: i32 = 140;
/// `mTributeFee = EE_UNITS_PLAYER_TRIBUTE_FEE`
pub const EE_UNITS_PLAYER_BODY_TRIBUTE_RESET: i32 = 141;
/// `mLastUnit` (an `RTSUnit*`)
pub const EE_UNITS_PLAYER_BODY_LAST_UNIT: i32 = 142;
/// unit `a`'s `GetComplexUnit()`
pub const EE_UNITS_PLAYER_BODY_COMPLEX_OF: i32 = 143;
/// `mQueued = ee_units_queued_new()`
pub const EE_UNITS_PLAYER_BODY_QUEUED_NEW: i32 = 144;
/// `mWonderState = ee_units_wonders_new()`
pub const EE_UNITS_PLAYER_BODY_WONDER_STATE_NEW: i32 = 145;
/// `mLists = ee_units_lists_new()`
pub const EE_UNITS_PLAYER_BODY_LISTS_NEW: i32 = 146;
/// `mGroupLists = ee_units_player_groups_new()`
pub const EE_UNITS_PLAYER_BODY_GROUP_LISTS_NEW: i32 = 147;
/// `mComputerPlayer = NULL`
pub const EE_UNITS_PLAYER_BODY_COMPUTER_CLEAR: i32 = 148;
/// `mHumanPlayer = NULL`
pub const EE_UNITS_PLAYER_BODY_HUMAN_CLEAR: i32 = 149;

/// `kActionMoveToNextPatrolPoint`
const ACTION_MOVE_TO_NEXT_PATROL_POINT: c_long = 8013;
/// `kWorldStartingCitizensDefault`, `kWorldGatherersPerResourceMax` (the world table's rows)
const WORLD_STARTING_CITIZENS: i64 = 48;
const WORLD_GATHERERS_MAX: i64 = 22;
/// the effects every player loads (`sPlayerEffects`, 0x4429c0)
const PLAYER_EFFECTS: [i64; 3] = [271, 274, 275];
/// `kTechTownCenter`, `kTechSettlement`, `kTechCapitol`
const TECH_TOWN_CENTER: i64 = 1025;
const TECH_SETTLEMENT: i64 = 1028;
const TECH_CAPITOL: i64 = 1053;
/// `kUnitTypeFarm`
const TYPE_FARM: i64 = 0x2a;
const OK: c_long = 0;
const OFF_MAP: i64 = -10000;

/// The C++ the player's bodies call beyond the groups and the selection.
pub trait PlayerBodyHost: SelectionHost {
    /// one operation (`EE_UNITS_PLAYER_BODY_*`) on player `player`; `swaps` the swaps' vector where one is named
    fn body(&mut self, op: i32, player: u64, a: u64, b: i64, c: i64) -> i64;
    /// `UnitsSwapped`'s operations with the swaps' vector
    fn body_swaps(&mut self, op: i32, player: u64, a: u64, b: i64, swaps: u64);
    /// `ee_units_player_reset(record, id, part)` (player_tick.rs)
    fn reset_record(&mut self, player: u64, part: u32);
    /// `ee_units_player_fields_reset(fields, citizens, mining)` (player.rs)
    fn reset_fields(&mut self, player: u64, starting_citizens: c_long, mining_limit: c_long);
    /// the player's lists (player_lists.rs): one operation; length, entry, cleared (released)
    fn list_op(&mut self, player: u64, list: u32, op: ListOp) -> u64;
    fn list_len(&mut self, player: u64, list: u32) -> usize {
        self.list_op(player, list, ListOp::Len) as usize
    }
    fn list_at(&mut self, player: u64, list: u32, i: usize) -> u64 {
        self.list_op(player, list, ListOp::At(i))
    }
    fn list_clear(&mut self, player: u64, list: u32, release: bool) {
        self.list_op(player, list, ListOp::Clear(release));
    }
    /// `ee_units_pg_id(group)`, `ee_units_pg_repair(group)` (patrol_group.rs)
    fn patrol_id(&mut self, group: u64) -> u64;
    fn patrol_repair(&mut self, group: u64);
    /// the off-map records (player_lists.rs `ee_units_off_map`): `what` 2 deleted, 3 repaired
    fn off_map(&mut self, player: u64, what: u32);
    /// `AdoptTownPatrol(town)` (player_rest.rs): whether it took a patrol
    fn adopt_town_patrol(&mut self, player: u64, town: u64) -> bool;
}

fn op<H: PlayerBodyHost>(h: &mut H, op: i32, p: u64, a: u64, b: i64, c: i64) -> i64 {
    h.body(op, p, a, b, c)
}

fn set_viewed<H: PlayerBodyHost>(h: &mut H, p: u64, unit: u64) {
    op(h, EE_UNITS_PLAYER_BODY_SET_VIEWED, p, unit, 0, 0);
}

/// `this->mViewedUnit` dereferenced and cleared (the destructor's, `Reset`'s, `Clear`'s).
fn drop_viewed<H: PlayerBodyHost>(h: &mut H, p: u64) {
    let v = h.viewed_unit(p);
    if v != 0 {
        op(h, EE_UNITS_PLAYER_BODY_UNIT_DEREFERENCE, p, v, 0, 0);
        set_viewed(h, p, 0);
    }
}

/// `SetSelectedViewedUnit(unit)`: the reference moves.
pub fn set_selected_viewed_unit<H: PlayerBodyHost>(h: &mut H, p: u64, unit: u64) {
    let old = op(h, EE_UNITS_PLAYER_BODY_SELECTED_VIEWED, p, 0, 0, 0) as u64;
    if old != 0 {
        op(h, EE_UNITS_PLAYER_BODY_UNIT_DEREFERENCE, p, old, 0, 0);
    }
    op(h, EE_UNITS_PLAYER_BODY_SET_SELECTED_VIEWED, p, unit, 0, 0);
    if unit != 0 {
        op(h, EE_UNITS_PLAYER_BODY_UNIT_REFERENCE, p, unit, 0, 0);
    }
}

/// `ExecuteSelectionMessage(message)`: the message runs on the network selection (swapped in as the active one,
/// the local selection flag up); the viewed unit is kept across it.
pub fn execute_selection_message<H: PlayerBodyHost>(h: &mut H, p: u64, message: u64) {
    let unit = h.viewed_unit(p);
    let group = h.player_selection(p, 0);
    if unit != 0 {
        op(h, EE_UNITS_PLAYER_BODY_UNIT_REFERENCE, p, unit, 0, 0);
    }
    let selected = h.player_selection(p, 1);
    op(h, EE_UNITS_PLAYER_BODY_SET_ACTIVE, p, selected, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_EXECUTING_LOCAL, p, 0, 1, 0);
    op(h, EE_UNITS_PLAYER_BODY_EXECUTE, p, message, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_EXECUTING_LOCAL, p, 0, 0, 0);
    let active = h.player_selection(p, 0);
    op(h, EE_UNITS_PLAYER_BODY_SET_SELECTED, p, active, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_SET_ACTIVE, p, group, 0, 0);
    let viewed = h.viewed_unit(p);
    set_selected_viewed_unit(h, p, viewed);
    sel::update_selection(h, p, unit, false);
    if unit != 0 {
        op(h, EE_UNITS_PLAYER_BODY_UNIT_DEREFERENCE, p, unit, 0, 0);
    }
}

/// `UnitsSwapped(swaps)`: every complex unit and air traffic controller replaces the old units by the new ones; each
/// (old, new) pair (both there) moves out of the selection and the hot-key groups (the new unit in when it is of
/// the old one's player), the interface is told for the local player, an unbuilt wall becomes a construction site
/// and a wonder is replaced. `vector` the swaps' `std::vector<EEPUnitSwap>`.
pub fn units_swapped<H: PlayerBodyHost>(h: &mut H, p: u64, swaps: &[(u64, u64)], vector: u64) {
    let group = h.player_selection(p, 0) as G;
    let mut u = op(h, EE_UNITS_PLAYER_BODY_NEXT_COMPLEX, p, 0, 0, 0) as u64;
    while u != 0 {
        h.body_swaps(EE_UNITS_PLAYER_BODY_REPLACE_UNITS, p, u, 0, vector);
        u = op(h, EE_UNITS_PLAYER_BODY_NEXT_COMPLEX, p, u, 0, 0) as u64;
    }
    for a in 0..4 {
        h.body_swaps(EE_UNITS_PLAYER_BODY_ATC_REPLACE, p, 0, a, vector);
    }
    for &(old, new) in swaps {
        if old == 0 || new == 0 {
            continue;
        }
        let same_player = h.unit(old).player == h.unit(new).player;
        // SAFETY (below): the player's live groups
        if unsafe { go::contains(group, old) } {
            unsafe { go::remove_unit(h, group, old) };
            if same_player {
                unsafe { go::add(h, group, new) };
            }
            let (selected, active) = (h.player_selection(p, 1) as G, h.player_selection(p, 0) as G);
            unsafe { go::copy_selection(h, selected, active) };
        }
        for i in 0..20 {
            let g = h.player_group(p, i) as G;
            if !unsafe { go::contains(g, old) } {
                continue;
            }
            unsafe { go::remove_unit(h, g, old) };
            if same_player {
                unsafe { go::add(h, g, new) };
            }
        }
        if p == h.current_player() {
            h.post_selection();
        }
        if op(h, EE_UNITS_PLAYER_BODY_UNBUILT_WALL, p, new, 0, 0) != 0 {
            add_construction_site(h, p, new);
        }
        h.list_op(p, EE_UNITS_LISTS_WONDERS, ListOp::Replace(old, new));
    }
}

/// `AddConstructionSite(unit)`: into the construction sites (referenced) unless they hold it.
pub fn add_construction_site<H: PlayerBodyHost>(h: &mut H, p: u64, unit: u64) {
    if h.list_op(p, EE_UNITS_LISTS_SITES, ListOp::Has(unit)) == 0 {
        h.list_op(p, EE_UNITS_LISTS_SITES, ListOp::Add(unit));
    }
}

/// `UnselectAllUnits()`: no viewed unit, the selection emptied.
pub fn unselect_all_units<H: PlayerBodyHost>(h: &mut H, p: u64) {
    sel::update_selection(h, p, 0, true);
    let g = h.player_selection(p, 0) as G;
    // SAFETY: the player's live selection
    unsafe { go::clear(h, g) };
}

/// `ClearSavedGroup(index)`: the hot-key group emptied, the interface told (index + 1).
pub fn clear_saved_group<H: PlayerBodyHost>(h: &mut H, p: u64, index: c_long) {
    let g = h.player_group(p, index as i64) as G;
    // SAFETY: the player's live group
    unsafe { go::clear(h, g) };
    h.post_saved_group(index + 1);
}

/// `LoadPlayerEffects()`: the three effects loaded and recorded as the player's media.
pub fn load_player_effects<H: PlayerBodyHost>(h: &mut H, p: u64) {
    for id in PLAYER_EFFECTS {
        let media = op(h, EE_UNITS_PLAYER_BODY_EFFECT, p, 0, id, 0) as u64;
        if media == 0 {
            continue;
        }
        op(h, EE_UNITS_PLAYER_BODY_MEDIA_LOAD, p, media, 0, 0);
        h.list_op(p, EE_UNITS_LISTS_MEDIA, ListOp::Add(media));
    }
}

/// `UnloadPlayerEffects()`: the effects the player's media holds, unloaded.
pub fn unload_player_effects<H: PlayerBodyHost>(h: &mut H, p: u64) {
    for id in PLAYER_EFFECTS {
        let media = op(h, EE_UNITS_PLAYER_BODY_EFFECT, p, 0, id, 0) as u64;
        if media == 0 {
            continue;
        }
        if h.list_op(p, EE_UNITS_LISTS_MEDIA, ListOp::Remove(media)) != 0 {
            op(h, EE_UNITS_PLAYER_BODY_MEDIA_UNLOAD, p, media, 0, 0);
        }
    }
}

/// `UnloadAllMedia()`: the player's media unloaded, the last first; the list emptied.
pub fn unload_all_media<H: PlayerBodyHost>(h: &mut H, p: u64) {
    for i in (0..h.list_len(p, EE_UNITS_LISTS_MEDIA)).rev() {
        let media = h.list_at(p, EE_UNITS_LISTS_MEDIA, i);
        if media != 0 {
            op(h, EE_UNITS_PLAYER_BODY_MEDIA_UNLOAD, p, media, 0, 0);
        }
    }
    h.list_clear(p, EE_UNITS_LISTS_MEDIA, false);
}

/// `KillStrandedUnits()`: each stranded unit (the last first) killed, taken from the player and released; the list
/// emptied.
pub fn kill_stranded_units<H: PlayerBodyHost>(h: &mut H, p: u64) {
    for i in (0..h.list_len(p, EE_UNITS_LISTS_STRANDED)).rev() {
        let unit = h.list_at(p, EE_UNITS_LISTS_STRANDED, i);
        if unit == 0 {
            continue;
        }
        op(h, EE_UNITS_PLAYER_BODY_KILL, p, unit, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_KILL_UNIT, p, unit, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_UNIT_DEREFERENCE, p, unit, 0, 0);
    }
    h.list_clear(p, EE_UNITS_LISTS_STRANDED, false);
}

/// `RestorePatrols()`: each enabled unit waiting to repatrol (the last first) goes to the nearest point of its
/// nearest town's patrol in a new group; the list emptied (released).
pub fn restore_patrols<H: PlayerBodyHost>(h: &mut H, p: u64) {
    for i in (0..h.list_len(p, EE_UNITS_LISTS_REPATROL)).rev() {
        let unit = h.list_at(p, EE_UNITS_LISTS_REPATROL, i);
        if op(h, EE_UNITS_PLAYER_BODY_DISABLED, p, unit, 0, 0) != 0 {
            continue;
        }
        let town = op(h, EE_UNITS_PLAYER_BODY_NEAREST_TOWN, p, unit, 0, 0) as u64;
        if town == 0 {
            continue;
        }
        let patrol = op(h, EE_UNITS_PLAYER_BODY_TOWN_PATROL, p, town, 0, 0) as u64;
        if patrol == 0 {
            continue;
        }
        let u = h.unit(unit);
        let point = op(h, EE_UNITS_PLAYER_BODY_PATROL_NEAREST, p, patrol, u.tile_x, u.tile_y) as u64;
        if point == 0 {
            continue;
        }
        let point_patrol = op(h, EE_UNITS_PLAYER_BODY_PATROL_OF, p, point, 0, 0) as u64;
        if point_patrol == 0 {
            continue;
        }
        let g = go::new(h, crate::units::group_obj::EE_UNITS_GROUP_KIND_ACTIVE, p, 0);
        // SAFETY (below): the new live group
        unsafe { go::add(h, g, unit) };
        h.player_groups_push(p, crate::units::group_obj::EE_UNITS_PLAYER_GROUPS_PENDING, g as u64);
        op(h, EE_UNITS_PLAYER_BODY_KEEP_MOBILE, p, g as u64, 0, 0);
        let goal = op(h, EE_UNITS_PLAYER_BODY_NEW_PATROL_GOAL, p, point, point_patrol as i64, 0) as u64;
        unsafe {
            go::clear_goal(h, g);
            go::set_goal(h, g, goal, false);
            (*g).mFields.mAction = ACTION_MOVE_TO_NEXT_PATROL_POINT;
        }
        op(h, EE_UNITS_PLAYER_BODY_PLAN, p, g as u64, 0, 0);
    }
    h.list_clear(p, EE_UNITS_LISTS_REPATROL, true);
}

/// `Reset()`: new selections and hot-key groups, the running groups released, the record reset, the statistics
/// cleared, an upgrade per unit class, the tech tree copied from the game's, the air traffic, the fields and the
/// events reset, the player effects loaded.
pub fn reset<H: PlayerBodyHost>(h: &mut H, p: u64) {
    let active = go::new(h, EE_UNITS_GROUP_KIND_SELECTED, p, 0);
    op(h, EE_UNITS_PLAYER_BODY_SET_ACTIVE, p, active as u64, 0, 0);
    let selected = go::new(h, EE_UNITS_GROUP_KIND_SELECTED, p, 0);
    op(h, EE_UNITS_PLAYER_BODY_SET_SELECTED, p, selected as u64, 0, 0);
    for i in 0..20 {
        let g = go::new(h, EE_UNITS_GROUP_KIND_SAVED, p, i + 1);
        op(h, EE_UNITS_PLAYER_BODY_SET_HOT_KEY, p, g as u64, i as i64, 0);
    }
    go::release_running(h, p);
    go::clear_selections(h, p);
    drop_viewed(h, p);
    h.reset_record(p, 0);
    op(h, EE_UNITS_PLAYER_BODY_STATS_CLEAR, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_STATS_NAME, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_STORE_TABLE, p, 0, 0, 0);
    let classes = op(h, EE_UNITS_PLAYER_BODY_CLASS_COUNT, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_UPGRADES_RESIZE, p, 0, classes, 0);
    for c in 0..op(h, EE_UNITS_PLAYER_BODY_CLASS_COUNT, p, 0, 0, 0) {
        op(h, EE_UNITS_PLAYER_BODY_UPGRADE_NEW, p, 0, c, 0);
    }
    op(h, EE_UNITS_PLAYER_BODY_TECH_NEW, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_TECH_COPY, p, 0, 0, 0);
    for a in 0..4 {
        let atc = op(h, EE_UNITS_PLAYER_BODY_ATC_NEW, p, 0, 0, 0) as u64;
        op(h, EE_UNITS_PLAYER_BODY_ATC_SET, p, atc, a, 0);
    }
    let citizens = op(h, EE_UNITS_PLAYER_BODY_WORLD_LONG, p, 0, WORLD_STARTING_CITIZENS, 0) as c_long;
    let mining = op(h, EE_UNITS_PLAYER_BODY_WORLD_LONG, p, 0, WORLD_GATHERERS_MAX, 0) as c_long;
    h.reset_fields(p, citizens, mining);
    op(h, EE_UNITS_PLAYER_BODY_HEALED_CLEAR, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_EVENTS_CLEAR, p, 0, 1, 0);
    h.reset_record(p, 1);
    load_player_effects(h, p);
}

/// The towns' (`towns`) or the herds' `EEDeletePointers`: each area deleted in order, then the list cleared.
fn delete_areas<H: PlayerBodyHost>(h: &mut H, p: u64, towns: bool) {
    use crate::units::player_rest::EE_UNITS_PLAYER_BODY_HERD_DELETE;
    use crate::units::town_split::EE_UNITS_PLAYER_BODY_TOWN_DELETE;
    let (list, delete) = if towns {
        (EE_UNITS_LISTS_TOWNS, EE_UNITS_PLAYER_BODY_TOWN_DELETE)
    } else {
        (EE_UNITS_LISTS_HERDS, EE_UNITS_PLAYER_BODY_HERD_DELETE)
    };
    for i in 0..h.list_len(p, list) {
        let area = h.list_at(p, list, i);
        op(h, delete, p, area, 0, 0);
    }
    h.list_clear(p, list, false);
}

/// The private lists, the towns, the herds, the off-map records and the patrols dropped (the destructor's and
/// `Clear`'s middle; `destroying`: the destructor's order, the repatrol list after the towns).
fn drop_areas<H: PlayerBodyHost>(h: &mut H, p: u64, destroying: bool) {
    delete_areas(h, p, true);
    if destroying {
        h.list_clear(p, EE_UNITS_LISTS_REPATROL, true);
        delete_areas(h, p, false);
    } else {
        delete_areas(h, p, false);
        h.list_clear(p, EE_UNITS_LISTS_REPATROL, true);
    }
    h.off_map(p, 2);
    h.list_clear(p, EE_UNITS_LISTS_PATROLS, true);
}

/// `~EEPlayer()`: the groups, the construction sites, the viewed units, the doppelgangers, the media, the tech tree,
/// the air traffic, the upgrades, the areas, the containers, the effects and the Rust objects released.
pub fn destroy<H: PlayerBodyHost>(h: &mut H, p: u64) {
    go::release_running(h, p);
    op(h, EE_UNITS_PLAYER_BODY_EVENTS_CLEAR, p, 0, 0, 0);
    go::release_others(h, p);
    h.list_clear(p, EE_UNITS_LISTS_SITES, true);
    drop_viewed(h, p);
    let sv = op(h, EE_UNITS_PLAYER_BODY_SELECTED_VIEWED, p, 0, 0, 0) as u64;
    if sv != 0 {
        op(h, EE_UNITS_PLAYER_BODY_UNIT_DEREFERENCE, p, sv, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_SET_SELECTED_VIEWED, p, 0, 0, 0);
    }
    op(h, EE_UNITS_PLAYER_BODY_DOPPELGANGERS, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_UNLOAD_ALL_MEDIA, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_TECH_DELETE, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_TECH_NULL, p, 0, 0, 0);
    for a in 0..4 {
        op(h, EE_UNITS_PLAYER_BODY_ATC_DELETE, p, 0, a, 0);
        op(h, EE_UNITS_PLAYER_BODY_ATC_SET, p, 0, a, 0);
    }
    for c in 0..op(h, EE_UNITS_PLAYER_BODY_CLASS_COUNT, p, 0, 0, 0) {
        op(h, EE_UNITS_PLAYER_BODY_UPGRADE_DELETE, p, 0, c, 0);
    }
    op(h, EE_UNITS_PLAYER_BODY_UPGRADES_CLEAR, p, 0, 0, 0);
    drop_areas(h, p, true);
    h.list_clear(p, EE_UNITS_LISTS_WONDERS, true);
    h.list_clear(p, EE_UNITS_LISTS_SITES, true);
    unload_player_effects(h, p);
    for o in [
        EE_UNITS_PLAYER_BODY_QUEUED_FREE,
        EE_UNITS_PLAYER_BODY_WONDER_STATE_FREE,
        EE_UNITS_PLAYER_BODY_LISTS_FREE,
        EE_UNITS_PLAYER_BODY_GROUP_LISTS_FREE,
    ] {
        op(h, o, p, 0, 0, 0);
    }
}

/// `Clear()`: the doppelgangers, the running groups, the selection and hot-key groups, the viewed unit, the events,
/// the areas and the patrols, then `RTSPlayer::Clear`.
pub fn clear<H: PlayerBodyHost>(h: &mut H, p: u64) {
    op(h, EE_UNITS_PLAYER_BODY_DOPPELGANGERS, p, 0, 0, 0);
    go::release_running(h, p);
    go::clear_selections(h, p);
    drop_viewed(h, p);
    op(h, EE_UNITS_PLAYER_BODY_EVENTS_CLEAR, p, 0, 1, 0);
    drop_areas(h, p, false);
    op(h, EE_UNITS_PLAYER_BODY_RTS_CLEAR, p, 0, 0, 0);
}

/// The player's units from `mFirstUnit` by `mNext`, each `mNext` read after `f` ran on the unit.
fn each_unit<H: PlayerBodyHost>(h: &mut H, p: u64, mut f: impl FnMut(&mut H, u64)) {
    let mut u = op(h, EE_UNITS_PLAYER_BODY_NEXT_UNIT, p, 0, 0, 0) as u64;
    while u != 0 {
        f(h, u);
        u = op(h, EE_UNITS_PLAYER_BODY_NEXT_UNIT, p, u, 0, 0) as u64;
    }
}

/// `RepairPointers(first, second)` after a load: the civilization, the base player, the patrols, the air traffic,
/// the towns (as saved, else rebuilt from the buildings), the construction sites and wonders by id, the groups, the
/// herds, the events, the queued improvements, the town buildings' technologies, the towns' patrols and the off-map
/// units; the air traffic's last answer, the blink's, then the events'.
pub fn repair_pointers<H: PlayerBodyHost>(h: &mut H, p: u64, first: c_long, second: c_long) -> c_long {
    let (b, c) = (first as i64, second as i64);
    let mut result: c_long = OK;
    op(h, EE_UNITS_PLAYER_BODY_CIV_REAPPLY, p, 0, b, c);
    op(h, EE_UNITS_PLAYER_BODY_RTS_REPAIR, p, 0, b, c);
    let mut i = 0;
    while i < h.list_len(p, EE_UNITS_LISTS_PATROLS) {
        let g = h.list_at(p, EE_UNITS_LISTS_PATROLS, i);
        h.patrol_repair(g);
        i += 1;
    }
    for a in 0..4 {
        result = op(h, EE_UNITS_PLAYER_BODY_ATC_REPAIR, p, 0, a, 0) as c_long;
    }
    h.list_clear(p, EE_UNITS_LISTS_LOADED_IDS, false);
    if h.player_id(p) >= 1 && op(h, EE_UNITS_PLAYER_BODY_MAKE_SAVED_TOWNS, p, 0, 0, 0) == 0 {
        let live = op(h, EE_UNITS_PLAYER_BODY_LIVE_STATE, p, 0, b, c) != 0;
        each_unit(h, p, |h, u| {
            let v = h.unit(u);
            if v.complex != 0 && !(live && v.land_building && !v.built && v.unit_type != TYPE_FARM) {
                op(h, EE_UNITS_PLAYER_BODY_ADD_TO_TOWN, p, v.complex, 0, 0);
            }
        });
    }
    if op(h, EE_UNITS_PLAYER_BODY_CIV_POINTS, p, 0, 0, 0) != 0 {
        result = op(h, EE_UNITS_PLAYER_BODY_CIV_BLINK, p, 0, 0, 0) as c_long;
    }
    for (list, container) in
        [(EE_UNITS_LISTS_SITE_IDS, EE_UNITS_LISTS_SITES), (EE_UNITS_LISTS_WONDER_IDS, EE_UNITS_LISTS_WONDERS)]
    {
        let mut j = 0;
        while j < h.list_len(p, list) {
            let id = h.list_at(p, list, j);
            let unit = h.world_unit(id as c_long as i64);
            if unit != 0 && h.unit(unit).has_tile_node {
                let complex = h.unit(unit).complex;
                h.list_op(p, container, ListOp::Add(complex));
            }
            j += 1;
        }
        h.list_clear(p, list, false);
    }
    op(h, EE_UNITS_PLAYER_BODY_RECOUNT_POPULATION, p, 0, 0, 0);
    if result == OK {
        let g = h.player_selection(p, 0) as G;
        // SAFETY: the player's live selection
        unsafe { go::repair_pointers(h, g) };
    }
    for i in 0..20 {
        let g = h.player_group(p, i) as G;
        // SAFETY: the player's live hot-key group
        unsafe { go::repair_pointers(h, g) };
    }
    let mut i = 0;
    while i < h.list_len(p, EE_UNITS_LISTS_HERDS) {
        let herd = h.list_at(p, EE_UNITS_LISTS_HERDS, i);
        op(h, EE_UNITS_PLAYER_BODY_HERD_REPAIR, p, herd, b, c);
        i += 1;
    }
    op(h, EE_UNITS_PLAYER_BODY_REGISTER_HERDS, p, 0, 0, 0);
    result = op(h, EE_UNITS_PLAYER_BODY_EVENTS_REPAIR, p, 0, b, c) as c_long;
    op(h, EE_UNITS_PLAYER_BODY_REQUEUE, p, 0, 0, 0);
    let (selected, active) = (h.player_selection(p, 1) as G, h.player_selection(p, 0) as G);
    // SAFETY: the player's live selections
    unsafe { go::copy_selection(h, selected, active) };
    let capitols = op(h, EE_UNITS_PLAYER_BODY_CAPITOLS, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_TECH_ALLOW, p, 0, TECH_CAPITOL, i64::from(capitols == 0));
    if op(h, EE_UNITS_PLAYER_BODY_TECH_OBSOLETE, p, 0, TECH_SETTLEMENT, 0) != 0 {
        let capitols = op(h, EE_UNITS_PLAYER_BODY_CAPITOLS, p, 0, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_TECH_ALLOW, p, 0, TECH_TOWN_CENTER, i64::from(capitols > 0));
    } else {
        let capitols = op(h, EE_UNITS_PLAYER_BODY_CAPITOLS, p, 0, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_TECH_ALLOW, p, 0, TECH_SETTLEMENT, i64::from(capitols > 0));
        op(h, EE_UNITS_PLAYER_BODY_TECH_ALLOW, p, 0, TECH_TOWN_CENTER, 0);
    }
    op(h, EE_UNITS_PLAYER_BODY_APPLY_SAVED_TOWNS, p, 0, 0, 0);
    let live = op(h, EE_UNITS_PLAYER_BODY_LIVE_STATE, p, 0, b, c) != 0;
    for t in (0..h.list_len(p, EE_UNITS_LISTS_TOWNS)).rev() {
        let town = h.list_at(p, EE_UNITS_LISTS_TOWNS, t);
        if town != 0
            && op(h, EE_UNITS_PLAYER_BODY_TOWN_PATROL, p, town, 0, 0) == 0
            && !(live && h.adopt_town_patrol(p, town))
        {
            op(h, EE_UNITS_PLAYER_BODY_TOWN_REFRESH, p, town, 0, 0);
        }
    }
    if first == 0 && (second as u32) < 0xd7 {
        each_unit(h, p, |h, u| {
            if op(h, EE_UNITS_PLAYER_BODY_TILE_X, p, u, 0, 0) == OFF_MAP {
                op(h, EE_UNITS_PLAYER_BODY_ADD_OFF_MAP, p, u, 0, 0);
            }
        });
    } else {
        h.off_map(p, 3);
    }
    op(h, EE_UNITS_PLAYER_BODY_RECOUNT_HEROES, p, 0, 0, 0);
    result
}

// ---- the second batch (the groups lane, round 2): the sight pulse, the map restrictions, the camouflage power, the
// construction site and team searches, the citizens' passability, the class button swap, the terrain's visibility,
// the patrol search, the damage, the tech tree's reset, the complex-unit walk and the constructors

/// `kTechDock`, `kTechNavyYard`, `kTechLighthouseOfAlexandria`, `kTechHurricane` (off on a waterless map)
const WATER_TECHS: [i64; 4] = [2023, 10007, 3047, 1022];
/// `kTechSpaceDock`, `kTechSpaceTurret`, `kTechMeteorStorm`, `kTechOrbitalSpaceStation` (off off a space map)
const SPACE_TECHS: [i64; 4] = [15000, 15009, 1089, 15029];
/// `kUnitTypeDock`, `kUnitTypeSpaceDock` (the Ctrl+D selection's unit type)
const TYPE_DOCK: i64 = 0x24;
const TYPE_SPACE_DOCK: i64 = 0x2d8;
/// `kUnitTypeCitizen`
const TYPE_CITIZEN: i64 = 0x20;
/// `kActionIdle`
const ACTION_IDLE: i64 = 5000;
/// `kWorldCamouflageTiming`
const WORLD_CAMOUFLAGE_TIMING: i64 = 85;

/// `PulseSight(by id, sight, id)`: the unit of the id (or every placed unit of type `id`) shows `sight` around it
/// for a moment (its sight raised by the difference to its line of sight, on, then back to 0, on).
pub fn pulse_sight<H: PlayerBodyHost>(h: &mut H, p: u64, by_id: bool, sight: f32, id: c_long) {
    let mut units = Vec::new();
    if by_id {
        if h.world_unit(id as i64) != 0 {
            units.push(h.world_unit(id as i64));
        }
    } else {
        each_unit(h, p, |h, u| {
            let v = h.unit(u);
            if v.has_tile_node && v.unit_type == id as i64 {
                units.push(u);
            }
        });
    }
    for u in units {
        let los = f32::from_bits(op(h, EE_UNITS_PLAYER_BODY_LINE_OF_SIGHT, p, u, 0, 0) as u32);
        let s = sight - los;
        op(h, EE_UNITS_PLAYER_BODY_COMPLEX_SIGHT_OFF, p, u, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_SET_SIGHT, p, u, s.to_bits() as i64, 0);
        op(h, EE_UNITS_PLAYER_BODY_SIGHT_ON, p, u, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_COMPLEX_SIGHT_OFF, p, u, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_SIGHT_ZERO, p, u, 0, 0);
        op(h, EE_UNITS_PLAYER_BODY_SIGHT_ON, p, u, 0, 0);
    }
}

/// `ApplyMapRestrictions()`: the local player loses the Ctrl+D hot key; a waterless map forbids the water
/// technologies (else the local player's Ctrl+D selects its docks), a map off space the space ones (else its space
/// docks).
pub fn apply_map_restrictions<H: PlayerBodyHost>(h: &mut H, p: u64) {
    if op(h, EE_UNITS_PLAYER_BODY_IS_LOCAL, p, 0, 0, 0) != 0 {
        op(h, EE_UNITS_PLAYER_BODY_HOT_KEY_REMOVE, p, 0, 0, 0);
    }
    for (forbid, techs, dock) in [
        (EE_UNITS_PLAYER_BODY_WATERLESS, WATER_TECHS, TYPE_DOCK),
        (EE_UNITS_PLAYER_BODY_NOT_SPACE, SPACE_TECHS, TYPE_SPACE_DOCK),
    ] {
        if op(h, forbid, p, 0, 0, 0) != 0 {
            for t in techs {
                op(h, EE_UNITS_PLAYER_BODY_TECH_ALLOW, p, 0, t, 0);
            }
        } else if op(h, EE_UNITS_PLAYER_BODY_IS_LOCAL, p, 0, 0, 0) != 0 {
            op(h, EE_UNITS_PLAYER_BODY_HOT_KEY_ADD, p, 0, dock, 0);
        }
    }
}

/// The camouflage power's unit set (0 without the power or its set).
fn camouflage_set<H: PlayerBodyHost>(h: &mut H, p: u64) -> u64 {
    if op(h, EE_UNITS_PLAYER_BODY_HAS_CAMOUFLAGE, p, 0, 0, 0) == 0
        || op(h, EE_UNITS_PLAYER_BODY_CAMOUFLAGE_SET, p, 0, 0, 0) == 0
    {
        return 0;
    }
    op(h, EE_UNITS_PLAYER_BODY_CAMOUFLAGE_SET, p, 0, 0, 0) as u64
}

/// `EnableCamouflagePower()` (`on`) / `DisableCamouflagePower()`: with the power, each complex unit of its set gets
/// the cloak timer when idle (on), or is uncloaked and made uncloakable with its timers cleared (off).
pub fn camouflage<H: PlayerBodyHost>(h: &mut H, p: u64, on: bool) {
    let set = camouflage_set(h, p);
    if set == 0 {
        return;
    }
    each_unit(h, p, |h, u| {
        let c = op(h, EE_UNITS_PLAYER_BODY_COMPLEX_OF, p, u, 0, 0) as u64;
        if c == 0 {
            return;
        }
        let unit_type = h.unit(u).unit_type;
        if op(h, EE_UNITS_PLAYER_BODY_SET_HAS, p, set, unit_type, 0) == 0 {
            return;
        }
        if on {
            if op(h, EE_UNITS_PLAYER_BODY_COMMAND_TYPE, p, c, 0, 0) == ACTION_IDLE {
                let timing = op(h, EE_UNITS_PLAYER_BODY_WORLD_LONG, p, 0, WORLD_CAMOUFLAGE_TIMING, 0);
                op(h, EE_UNITS_PLAYER_BODY_SET_CAMOUFLAGE_TIMER, p, c, timing, 0);
            }
        } else {
            op(h, EE_UNITS_PLAYER_BODY_CLOAKABLE_OFF, p, c, 0, 0);
            op(h, EE_UNITS_PLAYER_BODY_CLOAKED_OFF, p, c, 0, 0);
            op(h, EE_UNITS_PLAYER_BODY_SET_CAMOUFLAGE_TIMER, p, c, 0, 0);
            op(h, EE_UNITS_PLAYER_BODY_CLEAR_U32C, p, c, 0, 0);
        }
    });
}

/// A unit's tile (`mTileX`, `mTileY`).
fn tile<H: PlayerBodyHost>(h: &mut H, p: u64, u: u64) -> (c_long, c_long) {
    (op(h, EE_UNITS_PLAYER_BODY_TILE_X, p, u, 0, 0) as c_long, op(h, EE_UNITS_PLAYER_BODY_TILE_Y, p, u, 0, 0) as c_long)
}

/// `EETileDistanceLess::DistanceSquared` (wrapping as the `long` arithmetic did).
fn distance_squared(a: (c_long, c_long), b: (c_long, c_long)) -> c_long {
    let (dx, dy) = (a.0.wrapping_sub(b.0), a.1.wrapping_sub(b.1));
    dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy))
}

/// `GetNearestConstructionSite(unit)`: the first nearest site within a squared distance below 100 (0 none).
pub fn nearest_construction_site<H: PlayerBodyHost>(h: &mut H, p: u64, unit: u64) -> u64 {
    let mut result = 0;
    let mut best: c_long = 100;
    let center = tile(h, p, unit);
    for i in 0..h.list_len(p, EE_UNITS_LISTS_SITES) {
        let site = h.list_at(p, EE_UNITS_LISTS_SITES, i);
        let d = distance_squared(center, tile(h, p, site));
        if d < best {
            result = site;
            best = d;
        }
    }
    result
}

/// `GetCitizenPassability()`: the citizens' terrain mask as passability flags (1 land 4, 0x200 1, 2 2, 0x1000 8).
pub fn citizen_passability<H: PlayerBodyHost>(h: &mut H, p: u64) -> c_long {
    let stats = op(h, EE_UNITS_PLAYER_BODY_CLASS_STATS, p, 0, TYPE_CITIZEN, 0) as u64;
    let mut result = 0;
    if stats != 0 {
        let flags = op(h, EE_UNITS_PLAYER_BODY_TERRAIN_MASK, p, stats, 0, 0);
        if flags & 1 != 0 {
            result = 4;
        }
        if flags & 0x200 != 0 {
            result |= 1;
        }
        if flags & 2 != 0 {
            result |= 2;
        }
        if flags & 0x1000 != 0 {
            result |= 8;
        }
    }
    result
}

/// `GetTeamIndex()`: the first team holding the player's id (0 none).
pub fn team_index<H: PlayerBodyHost>(h: &mut H, p: u64) -> i64 {
    let id = h.player_id(p);
    for i in 0..op(h, EE_UNITS_PLAYER_BODY_TEAM_COUNT, p, 0, 0, 0) {
        if op(h, EE_UNITS_PLAYER_BODY_TEAM_HAS, p, 0, i, id) != 0 {
            return i;
        }
    }
    0
}

/// `SwapClassButtonMedia(class, first, second)`: a human player whose class's buttons are loaded releases the
/// first button and acquires the second.
pub fn swap_class_button_media<H: PlayerBodyHost>(h: &mut H, p: u64, class: i64, first: u64, second: u64) {
    let stats = op(h, EE_UNITS_PLAYER_BODY_CLASS_STATS, p, 0, class, 0) as u64;
    if stats == 0
        || op(h, EE_UNITS_PLAYER_BODY_BUTTON_LOADS, p, stats, 0, 0) as c_long <= 0
        || op(h, EE_UNITS_PLAYER_BODY_IS_HUMAN, p, 0, 0, 0) == 0
    {
        return;
    }
    if first != 0 {
        op(h, EE_UNITS_PLAYER_BODY_BUTTON_RELEASE, p, first, 0, 0);
    }
    if second != 0 {
        op(h, EE_UNITS_PLAYER_BODY_BUTTON_ACQUIRE, p, second, 0, 0);
    }
}

/// `RefreshTerrainVisibility()`: every tile's visibility for the player into the terrain mesh, by column.
pub fn refresh_terrain_visibility<H: PlayerBodyHost>(h: &mut H, p: u64) {
    let size = op(h, EE_UNITS_PLAYER_BODY_MAP_SIZE, p, 0, 0, 0) as u32;
    for x in 0..size {
        for y in 0..size {
            if op(h, EE_UNITS_PLAYER_BODY_TILE_PRESENT, p, 0, x.into(), y.into()) != 0 {
                op(h, EE_UNITS_PLAYER_BODY_TILE_VISIBILITY, p, 0, x.into(), y.into());
            }
        }
    }
}

/// `GetPatrolGroup(id)`: the first patrol group of the id (0 none).
pub fn patrol_group<H: PlayerBodyHost>(h: &mut H, p: u64, id: u64) -> u64 {
    let mut i = 0;
    while i < h.list_len(p, EE_UNITS_LISTS_PATROLS) {
        let g = h.list_at(p, EE_UNITS_LISTS_PATROLS, i);
        if h.patrol_id(g) == id {
            return g;
        }
        i += 1;
    }
    0
}

/// `DamageTaken(from, amount, unit)`: the damage counted against its player; an enemy's puts the player in combat.
pub fn damage_taken<H: PlayerBodyHost>(h: &mut H, p: u64, from: i64, amount: i64) {
    op(h, EE_UNITS_PLAYER_BODY_DAMAGE_ADD, p, 0, from, amount);
    if from == 0 {
        return;
    }
    if op(h, EE_UNITS_PLAYER_BODY_IS_SELF, p, 0, from, 0) == 0
        && op(h, EE_UNITS_PLAYER_BODY_ENEMY_OF, p, 0, from, 0) != 0
    {
        op(h, EE_UNITS_PLAYER_BODY_SET_COMBAT, p, 0, 2, 0);
    }
}

/// `ResetTechTree()`: the game's tree copied in and bound to the player, the population bonus and tribute fee reset.
pub fn reset_tech_tree<H: PlayerBodyHost>(h: &mut H, p: u64) {
    op(h, EE_UNITS_PLAYER_BODY_TECH_COPY, p, 0, 0, 0);
    let id = h.player_id(p);
    op(h, EE_UNITS_PLAYER_BODY_TECH_SET_INDEX, p, 0, id, 0);
    op(h, EE_UNITS_PLAYER_BODY_TECH_SET_PLAYER, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_BB44_CLEAR, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_POPULATION_BONUS_CLEAR, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_TRIBUTE_RESET, p, 0, 0, 0);
}

/// `GetNextComplexUnit(unit)`: the next unit after `unit` (the first for 0) with a complex unit, its complex unit (0
/// at the end, or for a player whose last unit is none).
pub fn next_complex_unit<H: PlayerBodyHost>(h: &mut H, p: u64, unit: u64) -> u64 {
    let mut result = 0;
    let mut u = unit;
    if op(h, EE_UNITS_PLAYER_BODY_LAST_UNIT, p, 0, 0, 0) != 0 {
        if u == 0 {
            u = op(h, EE_UNITS_PLAYER_BODY_NEXT_UNIT, p, 0, 0, 0) as u64;
            result = op(h, EE_UNITS_PLAYER_BODY_COMPLEX_OF, p, u, 0, 0) as u64;
        }
        while u != 0 && result == 0 {
            u = op(h, EE_UNITS_PLAYER_BODY_NEXT_UNIT, p, u, 0, 0) as u64;
            if u != 0 {
                result = op(h, EE_UNITS_PLAYER_BODY_COMPLEX_OF, p, u, 0, 0) as u64;
            }
        }
    }
    result
}

/// The constructors' members (`named`: `EEPlayer(name, id)`, which binds the tech tree to player id `id` after
/// `Reset`).
pub fn construct<H: PlayerBodyHost>(h: &mut H, p: u64, named: bool, id: i64) {
    op(h, EE_UNITS_PLAYER_BODY_SET_VIEWED, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_SET_SELECTED_VIEWED, p, 0, 0, 0);
    for o in [
        EE_UNITS_PLAYER_BODY_QUEUED_NEW,
        EE_UNITS_PLAYER_BODY_WONDER_STATE_NEW,
        EE_UNITS_PLAYER_BODY_LISTS_NEW,
        EE_UNITS_PLAYER_BODY_GROUP_LISTS_NEW,
    ] {
        op(h, o, p, 0, 0, 0);
    }
    reset(h, p);
    if named {
        op(h, EE_UNITS_PLAYER_BODY_TECH_SET_INDEX, p, 0, id, 0);
        op(h, EE_UNITS_PLAYER_BODY_TECH_SET_PLAYER, p, 0, 0, 0);
    }
    op(h, EE_UNITS_PLAYER_BODY_COMPUTER_CLEAR, p, 0, 0, 0);
    op(h, EE_UNITS_PLAYER_BODY_HUMAN_CLEAR, p, 0, 0, 0);
}

// ---- the entry point
/// `ee_units_player_body`'s `what`: the members above.
pub const EE_UNITS_PLAYER_DO_DESTROY: u32 = 0;
pub const EE_UNITS_PLAYER_DO_RESET: u32 = 1;
pub const EE_UNITS_PLAYER_DO_CLEAR: u32 = 2;
/// `RepairPointers(a, b)` (the answer)
pub const EE_UNITS_PLAYER_DO_REPAIR_POINTERS: u32 = 3;
/// `ExecuteSelectionMessage(message a)`
pub const EE_UNITS_PLAYER_DO_EXECUTE_SELECTION: u32 = 4;
/// `SetSelectedViewedUnit(unit a)`
pub const EE_UNITS_PLAYER_DO_SET_SELECTED_VIEWED: u32 = 5;
pub const EE_UNITS_PLAYER_DO_UNSELECT_ALL: u32 = 6;
/// `ClearSavedGroup(a)`
pub const EE_UNITS_PLAYER_DO_CLEAR_SAVED_GROUP: u32 = 7;
pub const EE_UNITS_PLAYER_DO_LOAD_EFFECTS: u32 = 8;
pub const EE_UNITS_PLAYER_DO_UNLOAD_EFFECTS: u32 = 9;
pub const EE_UNITS_PLAYER_DO_UNLOAD_ALL_MEDIA: u32 = 10;
pub const EE_UNITS_PLAYER_DO_KILL_STRANDED: u32 = 11;
pub const EE_UNITS_PLAYER_DO_RESTORE_PATROLS: u32 = 12;

/// `PulseSight(by id b, sight f, id a)`
pub const EE_UNITS_PLAYER_DO_PULSE_SIGHT: u32 = 13;
pub const EE_UNITS_PLAYER_DO_MAP_RESTRICTIONS: u32 = 14;
/// `EnableCamouflagePower()` (b 1) / `DisableCamouflagePower()`
pub const EE_UNITS_PLAYER_DO_CAMOUFLAGE: u32 = 15;
/// `GetNearestConstructionSite(unit a)` (the answer)
pub const EE_UNITS_PLAYER_DO_NEAREST_SITE: u32 = 16;
pub const EE_UNITS_PLAYER_DO_CITIZEN_PASSABILITY: u32 = 17;
pub const EE_UNITS_PLAYER_DO_TEAM_INDEX: u32 = 18;
/// `SwapClassButtonMedia(class b, first a, second c)`
pub const EE_UNITS_PLAYER_DO_SWAP_BUTTONS: u32 = 19;
pub const EE_UNITS_PLAYER_DO_REFRESH_TERRAIN: u32 = 20;
/// `GetPatrolGroup(id a)` (the answer)
pub const EE_UNITS_PLAYER_DO_PATROL_GROUP: u32 = 21;
/// `DamageTaken(from b, amount c, unit)`
pub const EE_UNITS_PLAYER_DO_DAMAGE: u32 = 22;
pub const EE_UNITS_PLAYER_DO_RESET_TECH_TREE: u32 = 23;
/// `GetNextComplexUnit(unit a)` (the answer)
pub const EE_UNITS_PLAYER_DO_NEXT_COMPLEX: u32 = 24;
/// the constructors (b 1: `EEPlayer(name, id c)`)
pub const EE_UNITS_PLAYER_DO_CONSTRUCT: u32 = 25;

/// Runs member `what` on `player` (`a`, `b`, `c`, `f` its arguments; the answer for those that have one).
pub fn run<H: PlayerBodyHost>(h: &mut H, player: u64, what: u32, a: u64, b: i64, c: i64, f: f32) -> i64 {
    match what {
        EE_UNITS_PLAYER_DO_DESTROY => destroy(h, player),
        EE_UNITS_PLAYER_DO_RESET => reset(h, player),
        EE_UNITS_PLAYER_DO_CLEAR => clear(h, player),
        EE_UNITS_PLAYER_DO_REPAIR_POINTERS => return repair_pointers(h, player, a as c_long, b as c_long) as i64,
        EE_UNITS_PLAYER_DO_EXECUTE_SELECTION => execute_selection_message(h, player, a),
        EE_UNITS_PLAYER_DO_SET_SELECTED_VIEWED => set_selected_viewed_unit(h, player, a),
        EE_UNITS_PLAYER_DO_UNSELECT_ALL => unselect_all_units(h, player),
        EE_UNITS_PLAYER_DO_CLEAR_SAVED_GROUP => clear_saved_group(h, player, a as c_long),
        EE_UNITS_PLAYER_DO_LOAD_EFFECTS => load_player_effects(h, player),
        EE_UNITS_PLAYER_DO_UNLOAD_EFFECTS => unload_player_effects(h, player),
        EE_UNITS_PLAYER_DO_UNLOAD_ALL_MEDIA => unload_all_media(h, player),
        EE_UNITS_PLAYER_DO_KILL_STRANDED => kill_stranded_units(h, player),
        EE_UNITS_PLAYER_DO_RESTORE_PATROLS => restore_patrols(h, player),
        EE_UNITS_PLAYER_DO_PULSE_SIGHT => pulse_sight(h, player, b != 0, f, a as c_long),
        EE_UNITS_PLAYER_DO_MAP_RESTRICTIONS => apply_map_restrictions(h, player),
        EE_UNITS_PLAYER_DO_CAMOUFLAGE => camouflage(h, player, b != 0),
        EE_UNITS_PLAYER_DO_NEAREST_SITE => return nearest_construction_site(h, player, a) as i64,
        EE_UNITS_PLAYER_DO_CITIZEN_PASSABILITY => return citizen_passability(h, player) as i64,
        EE_UNITS_PLAYER_DO_TEAM_INDEX => return team_index(h, player),
        EE_UNITS_PLAYER_DO_SWAP_BUTTONS => swap_class_button_media(h, player, b, a, c as u64),
        EE_UNITS_PLAYER_DO_REFRESH_TERRAIN => refresh_terrain_visibility(h, player),
        EE_UNITS_PLAYER_DO_PATROL_GROUP => return patrol_group(h, player, a) as i64,
        EE_UNITS_PLAYER_DO_DAMAGE => damage_taken(h, player, b, c),
        EE_UNITS_PLAYER_DO_RESET_TECH_TREE => reset_tech_tree(h, player),
        EE_UNITS_PLAYER_DO_NEXT_COMPLEX => return next_complex_unit(h, player, a) as i64,
        EE_UNITS_PLAYER_DO_CONSTRUCT => construct(h, player, b != 0, c),
        _ => {}
    }
    0
}

/// The C++ body host (EEPlayer.cpp's `ee_units_player_body_host`) for the other bodies' modules (player_rest.rs,
/// town_split.rs): one statement of the player's C++ on player `player`.
#[cfg(not(test))]
pub(crate) fn body_host(op: i32, player: u64, a: u64, b: i64, c: i64) -> i64 {
    // SAFETY: the player's own operations (player_native.rs), else the C++ switch over a live player and the objects
    // the bodies hand it
    unsafe {
        crate::units::player_native::body(op, player, a, b, c)
            .or_else(|| crate::units::player_upgrades::body(op, player, b))
            .unwrap_or_else(|| {
                game::ee_units_player_body_host(op, player as *mut std::ffi::c_void, a, b, c, std::ptr::null_mut())
            })
    }
}

#[cfg(not(test))]
pub mod game {
    use super::*;
    use crate::units::group_host::GameGroups;
    use crate::units::group_obj::GroupHost;
    use crate::world::layout::layout;
    use std::ffi::c_void;

    extern "C" {
        pub(super) fn ee_units_player_body_host(
            op: i32,
            player: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            swaps: *mut c_void,
        ) -> i64;
    }

    impl PlayerBodyHost for GameGroups {
        fn body(&mut self, op: i32, player: u64, a: u64, b: i64, c: i64) -> i64 {
            body_host(op, player, a, b, c)
        }
        fn body_swaps(&mut self, op: i32, player: u64, a: u64, b: i64, swaps: u64) {
            // SAFETY: as above; `swaps` the caller's vector
            unsafe { ee_units_player_body_host(op, player as *mut c_void, a, b, 0, swaps as *mut c_void) };
        }
        fn reset_record(&mut self, player: u64, part: u32) {
            let id = self.player_id(player) as u32;
            let rec = (player as usize + layout().player_saved as usize)
                as *mut crate::units::player_save::EeUnitsPlayerSaved;
            // SAFETY: the live player's record
            unsafe { crate::units::player_tick::ee_units_player_reset(rec, id, part) }
        }
        fn reset_fields(&mut self, player: u64, starting_citizens: c_long, mining_limit: c_long) {
            let rec = (player as usize + layout().player_fields as usize)
                as *mut crate::world::player_fields::EeUnitsPlayerFields;
            // SAFETY: the live player's record
            unsafe { crate::units::player::ee_units_player_fields_reset(rec, starting_citizens, mining_limit) }
        }
        fn list_op(&mut self, player: u64, list: u32, op: ListOp) -> u64 {
            crate::units::player_lists::player_op(player, list, op)
        }
        fn patrol_id(&mut self, group: u64) -> u64 {
            // SAFETY: the player's live patrol group
            unsafe { crate::units::patrol_group::ee_units_pg_id(group as *mut _) as u64 }
        }
        fn off_map(&mut self, player: u64, what: u32) {
            let lists = crate::units::player_lists::player_lists(player);
            // SAFETY: the live player's lists
            unsafe { crate::units::player_lists::ee_units_off_map(lists, what, 0, 0, 0, 0) };
        }
        fn patrol_repair(&mut self, group: u64) {
            // SAFETY: as above
            unsafe { crate::units::patrol_group::ee_units_pg_repair(group as *mut _) };
        }
        fn adopt_town_patrol(&mut self, player: u64, town: u64) -> bool {
            use crate::units::player_rest::{game::ee_units_player_rest, EE_UNITS_PLAYER_REST_ADOPT_TOWN_PATROL};
            // SAFETY: the live player and its town
            unsafe {
                ee_units_player_rest(player as *mut c_void, EE_UNITS_PLAYER_REST_ADOPT_TOWN_PATROL, town, 0, 0) != 0
            }
        }
    }

    /// The player's members (`what`, `EE_UNITS_PLAYER_DO_*`) over the game; `swaps`/`n` `UnitsSwapped`'s pairs
    /// (`EEPUnitSwap`: old, new), `vector` their `std::vector`.
    ///
    /// # Safety
    /// `player` a live player; `a` the member's object where it takes one; `swaps` `n` pairs.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_player_body(
        player: *mut c_void,
        what: u32,
        a: u64,
        b: i64,
        c: i64,
        f: f32,
    ) -> i64 {
        run(&mut GameGroups, player as u64, what, a, b, c, f)
    }

    /// `UnitsSwapped(swaps)`.
    ///
    /// # Safety
    /// `player` a live player; `swaps` `n` (old, new) unit pairs, the data of `vector`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_player_units_swapped(
        player: *mut c_void,
        swaps: *const [*mut c_void; 2],
        n: usize,
        vector: *mut c_void,
    ) {
        // SAFETY: the caller's contract
        let pairs: Vec<(u64, u64)> = if n == 0 || swaps.is_null() {
            Vec::new()
        } else {
            unsafe { std::slice::from_raw_parts(swaps, n) }.iter().map(|s| (s[0] as u64, s[1] as u64)).collect()
        };
        units_swapped(&mut GameGroups, player as u64, &pairs, vector as u64)
    }
}
