//! More of `EEPlayer`'s bodies (the world lane, round 2: were EEPlayer.cpp's): `GetNearestArea`, `GetChecksum`,
//! `GetHerdsOfType`, `RegisterTowns`/`RegisterHerds`, `UnitRemoved`, `RemoveAlly`, `RemovePatrolGroup`,
//! `FinishUnitsLoad`, `LoadAllClassMedia`/`UnloadAllClassMedia`, `UnitGoalDone`, `RefreshTownGroups`, the wonders'
//! timers, `SetAllResources`, `UpdateAirTraffic`, `RemovingUnit`, `SetGameResult` and `AdoptTownPatrol`. Each C++
//! statement is one operation of the player's body host ([`RestHost::body`]: `ee_units_player_body_host`,
//! EEPlayer.cpp, one statement a case; the operations from [`EE_UNITS_PLAYER_BODY_CONTINENT_ID`] on), in the
//! original's order; the plain reads go through the world views, the player's records in place.

#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use std::os::raw::{c_long, c_ulong};

use crate::units::player_body::{
    EE_UNITS_PLAYER_BODY_CLASS_COUNT, EE_UNITS_PLAYER_BODY_CLASS_STATS, EE_UNITS_PLAYER_BODY_IS_HUMAN,
    EE_UNITS_PLAYER_BODY_IS_LOCAL, EE_UNITS_PLAYER_BODY_NEXT_UNIT, EE_UNITS_PLAYER_BODY_TILE_X,
    EE_UNITS_PLAYER_BODY_TILE_Y, EE_UNITS_PLAYER_BODY_TOWN_PATROL, EE_UNITS_PLAYER_BODY_TOWN_REFRESH,
};
use crate::units::player_lists::{
    ListOp, EE_UNITS_LISTS_ALLIES, EE_UNITS_LISTS_HERDS, EE_UNITS_LISTS_PATROLS, EE_UNITS_LISTS_SOUND_CLASSES,
    EE_UNITS_LISTS_TOWNS, EE_UNITS_LISTS_WONDERS,
};
use crate::units::player_save::EeUnitsPlayerSaved;

// ---- the body host's operations (EEPlayer.cpp's `ee_units_player_body_host`; `a` an object, `b`, `c` values)
/// `gContinentManager.GetContinentID(b, c)`
pub const EE_UNITS_PLAYER_BODY_CONTINENT_ID: i32 = 200;
/// town `a`'s `mContinent`
pub const EE_UNITS_PLAYER_BODY_TOWN_CONTINENT: i32 = 201;
/// town `a`'s `GetClosestPoint((b, c))`, packed (x high, y low 32 bits)
pub const EE_UNITS_PLAYER_BODY_TOWN_CLOSEST: i32 = 202;
/// `RTSPlayer::GetChecksum(*a)`
pub const EE_UNITS_PLAYER_BODY_RTS_CHECKSUM: i32 = 203;
/// `mTechnologies->GetChecksum(*a)`
pub const EE_UNITS_PLAYER_BODY_TECH_CHECKSUM: i32 = 204;
/// `a->AddChecksum(UString("EEPlayer"), b)`
pub const EE_UNITS_PLAYER_BODY_ADD_CHECKSUM: i32 = 205;
/// `EEAnimal::Find(b)` (0 none), and animal `a`'s `mAnimalID`
pub const EE_UNITS_PLAYER_BODY_ANIMAL_OF_TYPE: i32 = 206;
pub const EE_UNITS_PLAYER_BODY_ANIMAL_ID: i32 = 241;
/// herd `a`'s `mAnimalID`
pub const EE_UNITS_PLAYER_BODY_HERD_ANIMAL: i32 = 207;
/// `((std::vector<EEHerd*>*)a)->push_back(herd b)`
pub const EE_UNITS_PLAYER_BODY_HERDS_PUSH: i32 = 208;
/// town `a`'s `SetSerial(gContinentManager.NewSerial())`, `gContinentManager.AddArea(town a)`; the herds' two
pub const EE_UNITS_PLAYER_BODY_TOWN_SERIAL: i32 = 209;
pub const EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA: i32 = 210;
pub const EE_UNITS_PLAYER_BODY_HERD_SERIAL: i32 = 242;
pub const EE_UNITS_PLAYER_BODY_HERD_ADD_AREA: i32 = 243;
/// complex unit `a`'s `GetNumGoalQueues()`
pub const EE_UNITS_PLAYER_BODY_GOAL_QUEUES: i32 = 211;
/// complex unit `a`'s `ClearGoalQueue(b)`
pub const EE_UNITS_PLAYER_BODY_CLEAR_GOAL_QUEUE: i32 = 212;
/// unit `a`'s `FinishLoad(b, c)`
pub const EE_UNITS_PLAYER_BODY_FINISH_LOAD: i32 = 215;
/// `gUnitClassTable[b]->UnloadClassMedia()`
pub const EE_UNITS_PLAYER_BODY_CLASS_UNLOAD_MEDIA: i32 = 216;
/// upgrade `a`'s `GetMediaCount()`
pub const EE_UNITS_PLAYER_BODY_STATS_MEDIA_COUNT: i32 = 217;
/// upgrade `a`'s `mUnitClass->LoadClassMedia()`
pub const EE_UNITS_PLAYER_BODY_STATS_LOAD_MEDIA: i32 = 218;
/// upgrade `a`'s `mUnitClass->mUnitType`
pub const EE_UNITS_PLAYER_BODY_STATS_TYPE: i32 = 219;
/// complex unit `a`'s `mIsDetachedFromAI`, and its store (b)
pub const EE_UNITS_PLAYER_BODY_DETACHED: i32 = 220;
pub const EE_UNITS_PLAYER_BODY_SET_DETACHED: i32 = 221;
/// complex unit `a`'s `mGoalQueues[0].mGoals.size()`, `mCitizenBuildQueue.size()`, `mActiveGroup`
pub const EE_UNITS_PLAYER_BODY_GOALS0_SIZE: i32 = 222;
pub const EE_UNITS_PLAYER_BODY_BUILD_QUEUE_SIZE: i32 = 223;
pub const EE_UNITS_PLAYER_BODY_ACTIVE_GROUP: i32 = 224;
/// group `a`'s `RemoveUnit(b)`
pub const EE_UNITS_PLAYER_BODY_GROUP_REMOVE: i32 = 225;
/// complex unit `a`'s `mCloaked = b`, `mCloaked`
pub const EE_UNITS_PLAYER_BODY_SET_CLOAKED: i32 = 228;
pub const EE_UNITS_PLAYER_BODY_CLOAKED: i32 = 230;
/// `gPhysics->mGameTime`
pub const EE_UNITS_PLAYER_BODY_GAME_TIME: i32 = 229;
/// `ee_units_atc_update(mAirTraffic[b])`
pub const EE_UNITS_PLAYER_BODY_ATC_UPDATE: i32 = 231;
/// unit `a`'s class's `RemoveFromTile(a)`, `RemoveFromRegion(a)`
pub const EE_UNITS_PLAYER_BODY_REMOVE_FROM_TILE: i32 = 232;
pub const EE_UNITS_PLAYER_BODY_REMOVE_FROM_REGION: i32 = 233;
/// `gWorld.mInfluenceTable->RemoveUnit(a, b, c)`
pub const EE_UNITS_PLAYER_BODY_INFLUENCE_REMOVE: i32 = 234;
/// `gPlatSimNoHumanDefeat`, and `gPlatSimNoHumanDefeatHits++` (TEST ONLY: the headless simulation)
pub const EE_UNITS_PLAYER_BODY_NO_HUMAN_DEFEAT: i32 = 235;
pub const EE_UNITS_PLAYER_BODY_NO_DEFEAT_HIT: i32 = 236;
// (237: the result line's chat entry, now `announce_result`)
/// town `a`'s `GetHullPointCount()`, `GetHullPoint(b)` (packed as the closest point)
pub const EE_UNITS_PLAYER_BODY_HULL_COUNT: i32 = 238;
pub const EE_UNITS_PLAYER_BODY_HULL_POINT: i32 = 239;
/// town `a`'s `mPatrolGroup = b`
pub const EE_UNITS_PLAYER_BODY_TOWN_SET_PATROL: i32 = 240;

/// `ClearFootprint`'s: `new std::vector<EEUnit*>`, `new std::vector<EETileCoordinate>`, their `delete`, sizes, the
/// unit `b` (an `RTSUnit` address) and the tile `b` (packed as the closest point)
pub const EE_UNITS_PLAYER_BODY_UNITS_NEW: i32 = 260;
pub const EE_UNITS_PLAYER_BODY_TILES_NEW: i32 = 261;
pub const EE_UNITS_PLAYER_BODY_UNITS_DELETE: i32 = 262;
pub const EE_UNITS_PLAYER_BODY_TILES_DELETE: i32 = 263;
pub const EE_UNITS_PLAYER_BODY_UNITS_SIZE: i32 = 264;
pub const EE_UNITS_PLAYER_BODY_TILES_SIZE: i32 = 265;
pub const EE_UNITS_PLAYER_BODY_UNITS_AT: i32 = 266;
pub const EE_UNITS_PLAYER_BODY_TILES_AT: i32 = 267;
/// `gUnitSetManager.Contains(b, c)`
pub const EE_UNITS_PLAYER_BODY_SET_MANAGER_CONTAINS: i32 = 268;
/// complex unit `a`'s `mWaypoints.size()`, `NextWaypoint()`, `GetPathStart(*(GE3DPoint*)b, *(float*)c)`
pub const EE_UNITS_PLAYER_BODY_WAYPOINTS: i32 = 269;
pub const EE_UNITS_PLAYER_BODY_NEXT_WAYPOINT: i32 = 270;
pub const EE_UNITS_PLAYER_BODY_PATH_START: i32 = 271;
/// complex unit `a` moved to the point `*(float(*)[3])b` keeping its orientation (`SetTransformation(.., true)`)
pub const EE_UNITS_PLAYER_BODY_MOVE_TO: i32 = 272;
/// `std::sort` of the tiles `a` by `EETileDistanceLess(b, c)`
pub const EE_UNITS_PLAYER_BODY_SORT_TILES: i32 = 273;
/// the map's tile (b, c) is there with no units on it
pub const EE_UNITS_PLAYER_BODY_TILE_EMPTY: i32 = 274;
/// the terrain mesh's `GetIsValidTile(b, c)`, its `GetMeshZ` of the two floats at `a` (the result's bits)
pub const EE_UNITS_PLAYER_BODY_MESH_VALID: i32 = 275;
pub const EE_UNITS_PLAYER_BODY_MESH_Z: i32 = 276;
/// complex unit `a`'s `DropPath()`, `StartAction(kActionIdle)`, its class's `ActionChanged(a)`
pub const EE_UNITS_PLAYER_BODY_DROP_PATH: i32 = 277;
pub const EE_UNITS_PLAYER_BODY_START_IDLE: i32 = 278;
pub const EE_UNITS_PLAYER_BODY_ACTION_CHANGED: i32 = 279;
/// `ReloadTechTree`'s: a new `FSMemoryFile(1000, true, 1)` and its `delete`, the tech tree's `Save` into file `a`,
/// `gWorld.RemoveShareBonus()`, the file's rewind, the tree's `Load` from file `a` (the result), its
/// `ResetEpochRange(b, c)`, its `ApplySavedState(true, b)`, `UnloadUnitClassMedia(b)`
pub const EE_UNITS_PLAYER_BODY_MEMORY_FILE_NEW: i32 = 280;
pub const EE_UNITS_PLAYER_BODY_MEMORY_FILE_DELETE: i32 = 281;
pub const EE_UNITS_PLAYER_BODY_TECH_SAVE: i32 = 282;
pub const EE_UNITS_PLAYER_BODY_SHARE_REMOVE: i32 = 283;
pub const EE_UNITS_PLAYER_BODY_MEMORY_FILE_REWIND: i32 = 284;
pub const EE_UNITS_PLAYER_BODY_TECH_LOAD: i32 = 285;
pub const EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE: i32 = 286;
pub const EE_UNITS_PLAYER_BODY_TECH_APPLY_SAVED: i32 = 287;
pub const EE_UNITS_PLAYER_BODY_UNLOAD_CLASS_MEDIA: i32 = 288;
/// `(const char*)gUnitClassTable[b]->mName`
pub const EE_UNITS_PLAYER_BODY_CLASS_NAME: i32 = 289;
/// `delete` herd `a` (the areas' delete: `EEDeletePointers`)
pub const EE_UNITS_PLAYER_BODY_HERD_DELETE: i32 = 292;
/// herd `a`'s `GetUnits(*(std::vector<EEComplexUnit*>*)b)`; `new`/`delete`/size/entry of such a vector; complex unit
/// `a`'s `Kill(b, 0, 0)`
pub const EE_UNITS_PLAYER_BODY_HERD_UNITS: i32 = 294;
pub const EE_UNITS_PLAYER_BODY_COMPLEXES_NEW: i32 = 295;
pub const EE_UNITS_PLAYER_BODY_COMPLEXES_DELETE: i32 = 296;
pub const EE_UNITS_PLAYER_BODY_COMPLEXES_SIZE: i32 = 297;
pub const EE_UNITS_PLAYER_BODY_COMPLEXES_AT: i32 = 298;
pub const EE_UNITS_PLAYER_BODY_KILL_KILLED: i32 = 299;
/// `DumpMedia(*(FSFileSpec*)a)`'s: the header line, `mUnitClassUpgrades.size()`, `mUnitClassUpgrades[b]`, upgrade
/// `b`'s `DumpMediaCount(file a)`, the text `b` (a C string)
pub const EE_UNITS_PLAYER_BODY_DUMP_HEADER: i32 = 300;
pub const EE_UNITS_PLAYER_BODY_UPGRADE_SLOTS: i32 = 301;
pub const EE_UNITS_PLAYER_BODY_UPGRADE_AT: i32 = 302;
pub const EE_UNITS_PLAYER_BODY_DUMP_COUNT: i32 = 303;
pub const EE_UNITS_PLAYER_BODY_DUMP_TEXT: i32 = 304;
/// the result line's texts: `new UWideString`, `delete (UWideString*)a`, `gResource->GetString(b, *(UWideString*)a)`
pub const EE_UNITS_PLAYER_BODY_TEXT_NEW: i32 = 307;
pub const EE_UNITS_PLAYER_BODY_TEXT_DELETE: i32 = 308;
pub const EE_UNITS_PLAYER_BODY_TEXT_RESOURCE: i32 = 309;
/// `sprintf(*(UWideString*)a, *(UWideString*)b, (const char*)*(UWideString*)c)`
pub const EE_UNITS_PLAYER_BODY_TEXT_FORMAT: i32 = 310;
/// `*(UWideString*)a = mName`; `sprintf(*(UWideString*)a, "%s - (%s)", mCivilization.mName, mName)`
pub const EE_UNITS_PLAYER_BODY_TEXT_NAME: i32 = 311;
pub const EE_UNITS_PLAYER_BODY_TEXT_CIV_NAME: i32 = 312;
/// `gUserInterface->AddChatLogEntry(GEColor(1, 1, 1), *(UWideString*)a, *(UWideString*)b,
/// gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0)`
pub const EE_UNITS_PLAYER_BODY_CHAT_RESULT: i32 = 313;

/// IDS_MSG_GAME, IDS_PLAYER_HAS_BEEN_DEFEATED, IDS_PLAYER_IS_VICTORIOUS (ee/EEStringIDs.h)
const IDS_MSG_GAME: i64 = 31813;
const IDS_PLAYER_HAS_BEEN_DEFEATED: i64 = 31814;
const IDS_PLAYER_IS_VICTORIOUS: i64 = 31815;

// ---- the members (`ee_units_player_rest`'s `what`)
pub const EE_UNITS_PLAYER_REST_NEAREST_AREA: u32 = 1;
pub const EE_UNITS_PLAYER_REST_CHECKSUM: u32 = 2;
pub const EE_UNITS_PLAYER_REST_HERDS_OF_TYPE: u32 = 3;
pub const EE_UNITS_PLAYER_REST_REGISTER: u32 = 4;
pub const EE_UNITS_PLAYER_REST_UNIT_REMOVED: u32 = 5;
pub const EE_UNITS_PLAYER_REST_REMOVE_ALLY: u32 = 6;
pub const EE_UNITS_PLAYER_REST_REMOVE_PATROL: u32 = 7;
pub const EE_UNITS_PLAYER_REST_FINISH_UNITS_LOAD: u32 = 8;
pub const EE_UNITS_PLAYER_REST_UNLOAD_ALL_CLASS_MEDIA: u32 = 9;
pub const EE_UNITS_PLAYER_REST_LOAD_ALL_CLASS_MEDIA: u32 = 10;
pub const EE_UNITS_PLAYER_REST_UNIT_GOAL_DONE: u32 = 11;
pub const EE_UNITS_PLAYER_REST_REFRESH_TOWN_GROUPS: u32 = 12;
pub const EE_UNITS_PLAYER_REST_RESET_WONDER_TIMERS: u32 = 13;
pub const EE_UNITS_PLAYER_REST_DELAY_WONDER_TIMERS: u32 = 14;
pub const EE_UNITS_PLAYER_REST_WONDER_TIME: u32 = 15;
pub const EE_UNITS_PLAYER_REST_SET_ALL_RESOURCES: u32 = 16;
pub const EE_UNITS_PLAYER_REST_UPDATE_AIR_TRAFFIC: u32 = 17;
pub const EE_UNITS_PLAYER_REST_REMOVING_UNIT: u32 = 18;
pub const EE_UNITS_PLAYER_REST_SET_GAME_RESULT: u32 = 19;
pub const EE_UNITS_PLAYER_REST_ADOPT_TOWN_PATROL: u32 = 20;
pub const EE_UNITS_PLAYER_REST_CLEAR_FOOTPRINT: u32 = 21;
pub const EE_UNITS_PLAYER_REST_RELOAD_TECH_TREE: u32 = 22;
/// `CheckButtonsPreloaded(class)` (b 1) and `LoadUnitClassMedia(class)` (b 0): a media warning
pub const EE_UNITS_PLAYER_REST_MEDIA_CHECK: u32 = 23;
pub const EE_UNITS_PLAYER_REST_KILL_HERD_UNITS: u32 = 24;
pub const EE_UNITS_PLAYER_REST_DUMP_MEDIA: u32 = 25;
pub const EE_UNITS_PLAYER_REST_ADD_TO_REPATROL: u32 = 26;

/// The C++ and the views these bodies read.
pub trait RestHost {
    /// one operation of the player's body host on player `p`
    fn body(&mut self, op: i32, p: u64, a: u64, b: i64, c: i64) -> i64;
    /// the player's lists (player_lists.rs): one operation
    fn list_op(&mut self, p: u64, list: u32, op: ListOp) -> u64;
    /// the player's list `list`'s entries
    fn list_all(&mut self, p: u64, list: u32) -> Vec<u64> {
        (0..self.list_len(p, list)).map(|i| self.list_at(p, list, i)).collect()
    }
    /// the player's `mTowns`, `mHerds`, `mAllies` (null entries kept)
    fn towns(&mut self, p: u64) -> Vec<u64> {
        self.list_all(p, EE_UNITS_LISTS_TOWNS)
    }
    fn herds(&mut self, p: u64) -> Vec<u64> {
        self.list_all(p, EE_UNITS_LISTS_HERDS)
    }
    fn allies(&mut self, p: u64) -> Vec<u64> {
        self.list_all(p, EE_UNITS_LISTS_ALLIES)
    }
    /// the player's media warnings (`mMediaWarnings`): one pushed, all of them
    fn media_warning(&mut self, p: u64, text: Vec<u8>);
    fn media_warnings(&mut self, p: u64) -> Vec<std::ffi::CString>;
    /// the player's saved record (`EeUnitsPlayerSaved`, read and written in place)
    fn saved(&mut self, p: u64) -> *mut EeUnitsPlayerSaved;
    /// unit `u` is an `EEComplexUnit` (`EEUnitCast<EEComplexUnit>`), its `mTileNode` is set, its tile, its class's
    /// `mInfluence`
    fn is_complex(&mut self, u: u64) -> bool;
    fn on_tile(&mut self, u: u64) -> bool;
    fn tile(&mut self, u: u64) -> (c_long, c_long);
    fn influence(&mut self, u: u64) -> c_long;
    /// the player's list `list`: length, entry, push, cleared
    fn list_len(&mut self, p: u64, list: u32) -> usize {
        self.list_op(p, list, ListOp::Len) as usize
    }
    fn list_at(&mut self, p: u64, list: u32, i: usize) -> u64 {
        self.list_op(p, list, ListOp::At(i))
    }
    fn list_add(&mut self, p: u64, list: u32, v: u64) {
        self.list_op(p, list, ListOp::Add(v));
    }
    fn list_clear(&mut self, p: u64, list: u32) {
        self.list_op(p, list, ListOp::Clear(false));
    }
    /// patrol group `g` (patrol_group.rs): referenced, dereferenced, a town's patrol, its points
    fn pg_reference(&mut self, g: u64);
    fn pg_dereference(&mut self, g: u64);
    fn pg_is_town_patrol(&mut self, g: u64) -> bool;
    fn pg_points(&mut self, g: u64) -> Vec<u64>;
    /// unit `u`'s class's `mTileSize` and `mUnitType`, its `mComplexUnit`, its location's z
    fn class_size_type(&mut self, u: u64) -> (c_long, c_long);
    fn complex_of(&mut self, u: u64) -> u64;
    fn location_z(&mut self, u: u64) -> f32;
    /// the unit utilities (uu_ffi.rs, with their captures' notes): `EEUnitsInRectangle` into the vector `units`,
    /// `EEBlockRing(0, ..)` into the vector `tiles`, `EECanStandOn`, `EEIsUnitMovable`
    fn units_in_rectangle(&mut self, bounds: [f32; 4], units: u64);
    fn block_ring(&mut self, x: c_long, y: c_long, size: c_long, tiles: u64);
    fn can_stand_on(&mut self, x: c_long, y: c_long, unit: u64) -> bool;
    fn movable(&mut self, unit: u64) -> bool;
    /// the unit classes of the tech tree's nodes whose unit media are loaded (each epoch's nodes in id order), its
    /// epoch range
    fn loaded_media_classes(&mut self, p: u64) -> Vec<c_long>;
    fn epoch_range(&mut self, p: u64) -> (c_long, c_long);
    /// `ResetTechTree` (player_body.rs) and `RefreshUpgrades` (selection.rs)
    fn reset_tech_tree(&mut self, p: u64);
    fn refresh_upgrades(&mut self, p: u64);
    /// the player's `mPlayerID`; the C string at `text`
    fn player_id(&mut self, p: u64) -> u32;
    /// unit `u`'s `mPlayer`
    fn unit_player(&mut self, u: u64) -> u64;
    fn c_string(&mut self, text: u64) -> Vec<u8>;
}

fn unpack(v: i64) -> (c_long, c_long) {
    ((v >> 32) as i32 as c_long, v as i32 as c_long)
}

/// `GetNearestArea(tile, same continent)`: the town nearest to the tile (the last of equals in the towns' order,
/// walked from the end), 0 for none.
pub fn nearest_area<H: RestHost + ?Sized>(h: &mut H, p: u64, x: c_long, y: c_long, same: bool) -> u64 {
    let continent = h.body(EE_UNITS_PLAYER_BODY_CONTINENT_ID, p, 0, x as i64, y as i64);
    let mut best: c_long = 0;
    let mut area = 0u64;
    for &town in h.towns(p).iter().rev() {
        if town == 0 || (same && h.body(EE_UNITS_PLAYER_BODY_TOWN_CONTINENT, p, town, 0, 0) != continent) {
            continue;
        }
        let (cx, cy) = unpack(h.body(EE_UNITS_PLAYER_BODY_TOWN_CLOSEST, p, town, x as i64, y as i64));
        let (dy, dx) = (cy.wrapping_sub(y), cx.wrapping_sub(x));
        let distance = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy));
        if area == 0 || distance < best {
            best = distance;
            area = town;
        }
    }
    area
}

/// `GetChecksum(io)`: `RTSPlayer`'s, then the resources and population summed (unsigned), the tech tree's, and the
/// sum as "EEPlayer".
pub fn checksum<H: RestHost + ?Sized>(h: &mut H, p: u64, io: u64) {
    h.body(EE_UNITS_PLAYER_BODY_RTS_CHECKSUM, p, io, 0, 0);
    // SAFETY: the live player's record (a copy of its values)
    let (caps, population) = unsafe {
        let s = h.saved(p);
        ((*s).mResourceCaps, (*s).mPopulationCount)
    };
    let mut sum: c_ulong = 0;
    for c in caps {
        sum = sum.wrapping_add(c as c_ulong);
    }
    sum = sum.wrapping_add(population as c_ulong);
    h.body(EE_UNITS_PLAYER_BODY_TECH_CHECKSUM, p, io, 0, 0);
    h.body(EE_UNITS_PLAYER_BODY_ADD_CHECKSUM, p, io, sum as i64, 0);
}

/// `GetHerdsOfType(out, unit type)`: the herds of the type's animal, the last first.
pub fn herds_of_type<H: RestHost + ?Sized>(h: &mut H, p: u64, out: u64, unit_type: c_long) {
    let found = h.body(EE_UNITS_PLAYER_BODY_ANIMAL_OF_TYPE, p, 0, unit_type as i64, 0) as u64;
    if found == 0 {
        return;
    }
    let animal = h.body(EE_UNITS_PLAYER_BODY_ANIMAL_ID, p, found, 0, 0);
    for &herd in h.herds(p).iter().rev() {
        if h.body(EE_UNITS_PLAYER_BODY_HERD_ANIMAL, p, herd, 0, 0) == animal {
            h.body(EE_UNITS_PLAYER_BODY_HERDS_PUSH, p, out, herd as i64, 0);
        }
    }
}

/// `RegisterTowns` (kind 0) and `RegisterHerds` (1): each area, the last first, under a new serial.
pub fn register<H: RestHost + ?Sized>(h: &mut H, p: u64, kind: i64) {
    let areas = if kind == 0 { h.towns(p) } else { h.herds(p) };
    for &area in areas.iter().rev() {
        if area == 0 {
            continue;
        }
        let (serial, add) = if kind == 0 {
            (EE_UNITS_PLAYER_BODY_TOWN_SERIAL, EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA)
        } else {
            (EE_UNITS_PLAYER_BODY_HERD_SERIAL, EE_UNITS_PLAYER_BODY_HERD_ADD_AREA)
        };
        h.body(serial, p, area, 0, 0);
        h.body(add, p, area, 0, 0);
    }
}

/// `UnitRemoved(unit)`: a complex unit's goal queues cleared.
pub fn unit_removed<H: RestHost + ?Sized>(h: &mut H, p: u64, unit: u64) {
    if unit == 0 || !h.is_complex(unit) {
        return;
    }
    let mut i = 0;
    while i < h.body(EE_UNITS_PLAYER_BODY_GOAL_QUEUES, p, unit, 0, 0) as c_ulong {
        h.body(EE_UNITS_PLAYER_BODY_CLEAR_GOAL_QUEUE, p, unit, i as i64, 0);
        i += 1;
    }
}

/// `RemoveAlly(player)`: its first entry out of `mAllies`.
pub fn remove_ally<H: RestHost + ?Sized>(h: &mut H, p: u64, other: u64) {
    h.list_op(p, EE_UNITS_LISTS_ALLIES, ListOp::Remove(other));
}

/// `RemovePatrolGroup(group)`: out of `mPatrolGroups`, dereferenced.
pub fn remove_patrol_group<H: RestHost + ?Sized>(h: &mut H, p: u64, group: u64) {
    let n = h.list_len(p, EE_UNITS_LISTS_PATROLS);
    if let Some(i) = (0..n).find(|&i| h.list_at(p, EE_UNITS_LISTS_PATROLS, i) == group) {
        h.list_op(p, EE_UNITS_LISTS_PATROLS, ListOp::EraseAt(i));
    }
    h.pg_dereference(group);
}

/// `FinishUnitsLoad(major, minor)`: each unit's `FinishLoad`; 0.
pub fn finish_units_load<H: RestHost + ?Sized>(h: &mut H, p: u64, major: i64, minor: i64) -> i64 {
    let mut u = h.body(EE_UNITS_PLAYER_BODY_NEXT_UNIT, p, 0, 0, 0) as u64;
    while u != 0 {
        h.body(EE_UNITS_PLAYER_BODY_FINISH_LOAD, p, u, major, minor);
        u = h.body(EE_UNITS_PLAYER_BODY_NEXT_UNIT, p, u, 0, 0) as u64;
    }
    0
}

/// `LoadAllClassMedia`: each class with media loads its class media and is recorded for the unload.
pub fn load_all_class_media<H: RestHost + ?Sized>(h: &mut H, p: u64) {
    let mut i = 0;
    while i < h.body(EE_UNITS_PLAYER_BODY_CLASS_COUNT, p, 0, 0, 0) {
        let stats = h.body(EE_UNITS_PLAYER_BODY_CLASS_STATS, p, 0, i, 0) as u64;
        if stats != 0 && h.body(EE_UNITS_PLAYER_BODY_STATS_MEDIA_COUNT, p, stats, 0, 0) != 0 {
            h.body(EE_UNITS_PLAYER_BODY_STATS_LOAD_MEDIA, p, stats, 0, 0);
            let unit_type = h.body(EE_UNITS_PLAYER_BODY_STATS_TYPE, p, stats, 0, 0) as u64;
            h.list_add(p, EE_UNITS_LISTS_SOUND_CLASSES, unit_type);
        }
        i += 1;
    }
}

/// `UnloadAllClassMedia`: the recorded classes' class media unloaded, the record emptied.
pub fn unload_all_class_media<H: RestHost + ?Sized>(h: &mut H, p: u64) {
    let mut i = 0;
    while i < h.list_len(p, EE_UNITS_LISTS_SOUND_CLASSES) {
        let unit_type = h.list_at(p, EE_UNITS_LISTS_SOUND_CLASSES, i);
        h.body(EE_UNITS_PLAYER_BODY_CLASS_UNLOAD_MEDIA, p, 0, unit_type as i64, 0);
        i += 1;
    }
    h.list_clear(p, EE_UNITS_LISTS_SOUND_CLASSES);
}

/// `UnitGoalDone(unit)`: back to the AI; out of its group without further goals or builds.
pub fn unit_goal_done<H: RestHost + ?Sized>(h: &mut H, p: u64, unit: u64) {
    if h.body(EE_UNITS_PLAYER_BODY_DETACHED, p, unit, 0, 0) != 0 {
        h.body(EE_UNITS_PLAYER_BODY_SET_DETACHED, p, unit, 0, 0);
    }
    if h.body(EE_UNITS_PLAYER_BODY_GOALS0_SIZE, p, unit, 0, 0) as u64 <= 1
        && h.body(EE_UNITS_PLAYER_BODY_BUILD_QUEUE_SIZE, p, unit, 0, 0) == 0
    {
        let group = h.body(EE_UNITS_PLAYER_BODY_ACTIVE_GROUP, p, unit, 0, 0) as u64;
        if group != 0 {
            h.body(EE_UNITS_PLAYER_BODY_GROUP_REMOVE, p, group, unit as i64, 0);
        }
    }
}

/// `RefreshTownGroups`: each town without a patrol group gets one, the last first.
pub fn refresh_town_groups<H: RestHost + ?Sized>(h: &mut H, p: u64) {
    let n = h.towns(p).len();
    for i in (0..n).rev() {
        let town = h.list_at(p, EE_UNITS_LISTS_TOWNS, i);
        if town != 0 && h.body(EE_UNITS_PLAYER_BODY_TOWN_PATROL, p, town, 0, 0) == 0 {
            h.body(EE_UNITS_PLAYER_BODY_TOWN_REFRESH, p, town, 0, 0);
        }
    }
}

fn wonders<H: RestHost + ?Sized>(h: &mut H, p: u64) -> Vec<u64> {
    h.list_all(p, EE_UNITS_LISTS_WONDERS)
}

/// `ResetWonderTimers(other)`: both players' wonders (the other's first) restart at the game time.
pub fn reset_wonder_timers<H: RestHost + ?Sized>(h: &mut H, p: u64, other: u64) {
    for player in [other, p] {
        for w in wonders(h, player) {
            let now = h.body(EE_UNITS_PLAYER_BODY_GAME_TIME, p, 0, 0, 0);
            h.body(EE_UNITS_PLAYER_BODY_SET_CLOAKED, p, w, now, 0);
        }
    }
}

/// `DelayWonderTimers(amount)`: every wonder's timer start moved on.
pub fn delay_wonder_timers<H: RestHost + ?Sized>(h: &mut H, p: u64, amount: c_long) {
    for w in wonders(h, p) {
        let cloaked = h.body(EE_UNITS_PLAYER_BODY_CLOAKED, p, w, 0, 0) as c_ulong;
        h.body(EE_UNITS_PLAYER_BODY_SET_CLOAKED, p, w, cloaked.wrapping_add(amount as c_ulong) as i64, 0);
    }
}

/// `SetAllResources(value)`: resources 2..6.
pub fn set_all_resources<H: RestHost + ?Sized>(h: &mut H, p: u64, value: c_long) {
    let saved = h.saved(p);
    // SAFETY: the live player's record (no reference kept)
    let caps = unsafe { &mut (*saved).mResourceCaps };
    caps[2..7].fill(value);
}

/// `RemovingUnit(unit)`: off its tile; when on one, out of its region and the influence table.
pub fn removing_unit<H: RestHost + ?Sized>(h: &mut H, p: u64, unit: u64) {
    h.body(EE_UNITS_PLAYER_BODY_REMOVE_FROM_TILE, p, unit, 0, 0);
    if !h.on_tile(unit) {
        return;
    }
    h.body(EE_UNITS_PLAYER_BODY_REMOVE_FROM_REGION, p, unit, 0, 0);
    if h.influence(unit) != 0 {
        let (x, y) = h.tile(unit);
        h.body(EE_UNITS_PLAYER_BODY_INFLUENCE_REMOVE, p, unit, x as i64, y as i64);
    }
}

/// `SetGameResult(state)`: a new victory (1) or defeat (2) of a player other than the local one is announced; the
/// state stored (TEST ONLY: the headless simulation never defeats the idle local human).
pub fn set_game_result<H: RestHost + ?Sized>(h: &mut H, p: u64, state: c_long) {
    let local = h.body(EE_UNITS_PLAYER_BODY_IS_LOCAL, p, 0, 0, 0) != 0;
    if state == 2 && h.body(EE_UNITS_PLAYER_BODY_NO_HUMAN_DEFEAT, p, 0, 0, 0) != 0 && local {
        h.body(EE_UNITS_PLAYER_BODY_NO_DEFEAT_HIT, p, 0, 0, 0);
        return;
    }
    // SAFETY: the live player's record
    let old = unsafe { (*h.saved(p)).mGameResult };
    let announce = if state == 1 { old != 1 } else { state == 2 && old != 2 };
    if announce && !local {
        announce_result(h, p, state == 1);
    }
    // SAFETY: as above
    unsafe { (*h.saved(p)).mGameResult = state as c_ulong };
}

/// The chat log's line of a player's victory or defeat ("--GAME--", the reason with the player's name; a computer
/// player's civilization before its name), white, shown twice the chat log's display time.
pub fn announce_result<H: RestHost + ?Sized>(h: &mut H, p: u64, victory: bool) {
    let mut text = || h.body(EE_UNITS_PLAYER_BODY_TEXT_NEW, p, 0, 0, 0) as u64;
    let (line, reason, title, name) = (text(), text(), text(), text());
    let id = if victory { IDS_PLAYER_IS_VICTORIOUS } else { IDS_PLAYER_HAS_BEEN_DEFEATED };
    h.body(EE_UNITS_PLAYER_BODY_TEXT_RESOURCE, p, reason, id, 0);
    h.body(EE_UNITS_PLAYER_BODY_TEXT_RESOURCE, p, title, IDS_MSG_GAME, 0);
    let named = if h.body(EE_UNITS_PLAYER_BODY_IS_HUMAN, p, 0, 0, 0) != 0 {
        EE_UNITS_PLAYER_BODY_TEXT_NAME
    } else {
        EE_UNITS_PLAYER_BODY_TEXT_CIV_NAME
    };
    h.body(named, p, name, 0, 0);
    h.body(EE_UNITS_PLAYER_BODY_TEXT_FORMAT, p, line, reason as i64, name as i64);
    h.body(EE_UNITS_PLAYER_BODY_CHAT_RESULT, p, title, line as i64, 0);
    for t in [line, reason, title, name] {
        h.body(EE_UNITS_PLAYER_BODY_TEXT_DELETE, p, t, 0, 0);
    }
}

/// `AdoptTownPatrol(town)`: a town patrol no other town holds, with a point on one of the town's hull vertices,
/// becomes the town's (referenced); whether one was found.
pub fn adopt_town_patrol<H: RestHost + ?Sized>(h: &mut H, p: u64, town: u64) -> bool {
    let groups = h.list_len(p, EE_UNITS_LISTS_PATROLS);
    for g in 0..groups {
        let group = h.list_at(p, EE_UNITS_LISTS_PATROLS, g);
        if group == 0 || !h.pg_is_town_patrol(group) {
            continue;
        }
        let towns = h.towns(p);
        if towns.iter().any(|&t| t != 0 && h.body(EE_UNITS_PLAYER_BODY_TOWN_PATROL, p, t, 0, 0) as u64 == group) {
            continue;
        }
        for point in h.pg_points(group) {
            let hulls = h.body(EE_UNITS_PLAYER_BODY_HULL_COUNT, p, town, 0, 0) as c_ulong;
            for v in 0..hulls {
                let (hx, hy) = unpack(h.body(EE_UNITS_PLAYER_BODY_HULL_POINT, p, town, v as i64, 0));
                if h.body(EE_UNITS_PLAYER_BODY_TILE_X, p, point, 0, 0) as c_long == hx
                    && h.body(EE_UNITS_PLAYER_BODY_TILE_Y, p, point, 0, 0) as c_long == hy
                {
                    h.body(EE_UNITS_PLAYER_BODY_TOWN_SET_PATROL, p, town, group as i64, 0);
                    h.pg_reference(group);
                    return true;
                }
            }
        }
    }
    false
}

/// `kUnitTypeRoboticFarm`, the unit set of buildings whose footprint is never cleared
const TYPE_ROBOTIC_FARM: c_long = 0x33a;
const SET_NO_FOOTPRINT: i64 = 0x4c;

/// `ClearFootprint(building)`: each movable unit standing in the building's tiles (not the building) goes to the first
/// waypoint of its path outside them, else to every free tile of the ring around the building it can stand on, the
/// nearest to the building's top right first (the last one taken wins), its path dropped and idle there.
pub fn clear_footprint<H: RestHost + ?Sized>(h: &mut H, p: u64, building: u64) {
    let (x, y) = h.tile(building);
    let (size, unit_type) = h.class_size_type(building);
    if unit_type == TYPE_ROBOTIC_FARM
        || h.body(EE_UNITS_PLAYER_BODY_SET_MANAGER_CONTAINS, p, 0, unit_type as i64, SET_NO_FOOTPRINT) != 0
    {
        return;
    }
    let right = (size - 1).wrapping_add(x);
    // the GERectangle: bottom, left, right, top
    let (bottom, top, left, right_f) = ((size - 1).wrapping_add(y) as f32, y as f32, x as f32, right as f32);
    let units = h.body(EE_UNITS_PLAYER_BODY_UNITS_NEW, p, 0, 0, 0) as u64;
    let tiles = h.body(EE_UNITS_PLAYER_BODY_TILES_NEW, p, 0, 0, 0) as u64;
    h.units_in_rectangle([left, top, right_f, bottom], units);
    let count = h.body(EE_UNITS_PLAYER_BODY_UNITS_SIZE, p, units, 0, 0);
    for i in 0..count {
        let u = h.body(EE_UNITS_PLAYER_BODY_UNITS_AT, p, units, i, 0) as u64;
        let unit = h.complex_of(u);
        if unit == 0 || unit == building || !h.movable(unit) {
            continue;
        }
        let mut moved = false;
        if h.body(EE_UNITS_PLAYER_BODY_WAYPOINTS, p, unit, 0, 0) > 0 {
            while h.body(EE_UNITS_PLAYER_BODY_NEXT_WAYPOINT, p, unit, 0, 0) == 0 {
                let mut point = [0f32; 3];
                let mut length = 0f32;
                let (at, len) = (point.as_mut_ptr() as i64, &mut length as *mut f32 as i64);
                if h.body(EE_UNITS_PLAYER_BODY_PATH_START, p, unit, at, len) != 0 {
                    continue;
                }
                // GERectangle::Contains: the edges included
                if point[0] >= left && point[0] <= right_f && point[1] >= top && point[1] <= bottom {
                    continue;
                }
                h.body(EE_UNITS_PLAYER_BODY_MOVE_TO, p, unit, point.as_ptr() as i64, 0);
                moved = true;
                break;
            }
        }
        if moved {
            continue;
        }
        h.block_ring(x, y, size, tiles);
        h.body(EE_UNITS_PLAYER_BODY_SORT_TILES, p, tiles, right as i64, y as i64);
        let n = h.body(EE_UNITS_PLAYER_BODY_TILES_SIZE, p, tiles, 0, 0);
        for j in 0..n {
            let (tx, ty) = unpack(h.body(EE_UNITS_PLAYER_BODY_TILES_AT, p, tiles, j, 0));
            if h.body(EE_UNITS_PLAYER_BODY_TILE_EMPTY, p, 0, tx as i64, ty as i64) == 0 || !h.can_stand_on(tx, ty, unit)
            {
                continue;
            }
            let mut z = h.location_z(unit);
            let xy = [tx as f32 + 0.5, ty as f32 + 0.5];
            if h.body(EE_UNITS_PLAYER_BODY_MESH_VALID, p, 0, tx as i64, ty as i64) != 0 {
                z = f32::from_bits(h.body(EE_UNITS_PLAYER_BODY_MESH_Z, p, xy.as_ptr() as u64, 0, 0) as u32);
            }
            let point = [xy[0], xy[1], z];
            h.body(EE_UNITS_PLAYER_BODY_MOVE_TO, p, unit, point.as_ptr() as i64, 0);
            h.body(EE_UNITS_PLAYER_BODY_DROP_PATH, p, unit, 0, 0);
            h.body(EE_UNITS_PLAYER_BODY_START_IDLE, p, unit, 0, 0);
            h.body(EE_UNITS_PLAYER_BODY_ACTION_CHANGED, p, unit, 0, 0);
        }
    }
    h.body(EE_UNITS_PLAYER_BODY_UNITS_DELETE, p, units, 0, 0);
    h.body(EE_UNITS_PLAYER_BODY_TILES_DELETE, p, tiles, 0, 0);
}

/// `ReloadTechTree(flag)`: the tree saved into a memory file, made again from the database's with the upgrades,
/// loaded back within its epoch range (its research applied unless `flag`), and the unit media its nodes had loaded
/// unloaded.
pub fn reload_tech_tree<H: RestHost + ?Sized>(h: &mut H, p: u64, flag: bool) {
    let (start, end) = h.epoch_range(p);
    let file = h.body(EE_UNITS_PLAYER_BODY_MEMORY_FILE_NEW, p, 0, 0, 0) as u64;
    let media = h.loaded_media_classes(p);
    h.body(EE_UNITS_PLAYER_BODY_TECH_SAVE, p, file, 0, 0);
    h.body(EE_UNITS_PLAYER_BODY_SHARE_REMOVE, p, 0, 0, 0);
    h.reset_tech_tree(p);
    h.refresh_upgrades(p);
    h.body(EE_UNITS_PLAYER_BODY_MEMORY_FILE_REWIND, p, file, 0, 0);
    if h.body(EE_UNITS_PLAYER_BODY_TECH_LOAD, p, file, 0, 0) == 0 {
        h.body(EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE, p, 0, start as i64, end as i64);
        h.body(EE_UNITS_PLAYER_BODY_TECH_APPLY_SAVED, p, 0, i64::from(!flag), 0);
    }
    for class in media {
        h.body(EE_UNITS_PLAYER_BODY_UNLOAD_CLASS_MEDIA, p, 0, class as i64, 0);
    }
    h.body(EE_UNITS_PLAYER_BODY_MEMORY_FILE_DELETE, p, file, 0, 0);
}

/// `CheckButtonsPreloaded(class)` (`buttons`) and `LoadUnitClassMedia(class)` (which loads nothing): a warning for
/// the media dump when the class's buttons (media) were not preloaded for the player.
pub fn media_check<H: RestHost + ?Sized>(h: &mut H, p: u64, class: c_long, buttons: bool) {
    use crate::units::player_body::{EE_UNITS_PLAYER_BODY_BUTTON_LOADS, EE_UNITS_PLAYER_BODY_CLASS_STATS};
    let stats = h.body(EE_UNITS_PLAYER_BODY_CLASS_STATS, p, 0, class as i64, 0) as u64;
    if stats == 0 {
        return;
    }
    let loads = if buttons {
        h.body(EE_UNITS_PLAYER_BODY_BUTTON_LOADS, p, stats, 0, 0)
    } else {
        h.body(EE_UNITS_PLAYER_BODY_STATS_MEDIA_COUNT, p, stats, 0, 0)
    };
    if loads != 0 {
        return;
    }
    let name = h.body(EE_UNITS_PLAYER_BODY_CLASS_NAME, p, 0, class as i64, 0) as u64;
    let name = h.c_string(name);
    let text = media_warning(&name, buttons, h.player_id(p));
    h.media_warning(p, text);
}

/// The warning's text, NUL-terminated (the original's `sprintf`; the id as `%ld`).
pub fn media_warning(name: &[u8], buttons: bool, id: u32) -> Vec<u8> {
    let mut text = b"The unit ".to_vec();
    text.extend_from_slice(name);
    let kind: &[u8] = if buttons { b" BUTTONS might" } else { b" might" };
    text.extend_from_slice(kind);
    let id = id as i32;
    text.extend(format!(" not have been preloaded for player {id} - have a programmer verify this.\n\0").bytes());
    text
}

/// A complex unit's cloak flags (`mIsCloakable`: 0, `mIsCloaked`: 1) and its next part (`mNextPart`, 0 none).
pub trait CloakHost {
    fn flag(&mut self, unit: u64, which: u32) -> bool;
    fn set_flag(&mut self, unit: u64, which: u32, value: bool);
    fn next_part(&mut self, unit: u64) -> u64;
}

const CLOAKABLE: u32 = 0;
const CLOAKED: u32 = 1;

/// `EEComplexUnit::SetCloakableChain(value)`: the unit and its following parts cloakable or not (a part already so
/// stops the walk); turning it off uncloaks them.
pub fn set_cloakable_chain<H: CloakHost + ?Sized>(h: &mut H, unit: u64, value: bool) {
    if h.flag(unit, CLOAKABLE) != value {
        h.set_flag(unit, CLOAKABLE, value);
        let next = h.next_part(unit);
        if next != 0 {
            set_cloakable_chain(h, next, value);
        }
    }
    if !value {
        set_cloaked_chain(h, unit, false);
    }
}

/// `EEComplexUnit::SetCloakedChain(value)`: the unit and its following parts cloaked or not (a part already so stops
/// the walk).
pub fn set_cloaked_chain<H: CloakHost + ?Sized>(h: &mut H, unit: u64, value: bool) {
    if h.flag(unit, CLOAKED) == value {
        return;
    }
    h.set_flag(unit, CLOAKED, value);
    let next = h.next_part(unit);
    if next != 0 {
        set_cloaked_chain(h, next, value);
    }
}

/// `KillHerdUnits(herd, killed)`: the herd's units, the last first, killed.
pub fn kill_herd_units<H: RestHost + ?Sized>(h: &mut H, p: u64, herd: u64, killed: bool) {
    let units = h.body(EE_UNITS_PLAYER_BODY_COMPLEXES_NEW, p, 0, 0, 0) as u64;
    h.body(EE_UNITS_PLAYER_BODY_HERD_UNITS, p, herd, units as i64, 0);
    let n = h.body(EE_UNITS_PLAYER_BODY_COMPLEXES_SIZE, p, units, 0, 0);
    for i in (0..n).rev() {
        let u = h.body(EE_UNITS_PLAYER_BODY_COMPLEXES_AT, p, units, i, 0) as u64;
        h.body(EE_UNITS_PLAYER_BODY_KILL_KILLED, p, u, i64::from(killed), 0);
    }
    h.body(EE_UNITS_PLAYER_BODY_COMPLEXES_DELETE, p, units, 0, 0);
}

/// `DumpMedia(file)`: the player's line, each upgrade's media counts, the media warnings.
pub fn dump_media<H: RestHost + ?Sized>(h: &mut H, p: u64, file: u64) {
    h.body(EE_UNITS_PLAYER_BODY_DUMP_HEADER, p, file, 0, 0);
    let n = h.body(EE_UNITS_PLAYER_BODY_UPGRADE_SLOTS, p, 0, 0, 0);
    for i in 0..n {
        let up = h.body(EE_UNITS_PLAYER_BODY_UPGRADE_AT, p, 0, i, 0);
        if up != 0 {
            h.body(EE_UNITS_PLAYER_BODY_DUMP_COUNT, p, file, up, 0);
        }
    }
    let title = c"\n\n\t\tMedia that might not have been loaded correctly:\n";
    h.body(EE_UNITS_PLAYER_BODY_DUMP_TEXT, p, file, title.as_ptr() as i64, 0);
    for w in h.media_warnings(p) {
        let mut line = b"\t\t".to_vec();
        line.extend_from_slice(w.as_bytes_with_nul());
        h.body(EE_UNITS_PLAYER_BODY_DUMP_TEXT, p, file, line.as_ptr() as i64, 0);
    }
}

/// `AddUnitToRepatrol(unit)`: one of the player's own units into the repatrol list once (referenced).
pub fn add_to_repatrol<H: RestHost + ?Sized>(h: &mut H, p: u64, unit: u64) {
    if unit == 0 || h.unit_player(unit) != p {
        return;
    }
    h.list_add(p, crate::units::player_lists::EE_UNITS_LISTS_REPATROL, unit);
}

/// `RTSPlayer::SetColor(from)`: the color, and its shade at 0.6 of each component (GEColor: red, green, blue).
///
/// # Safety
/// The three point to live colors (`from` may be `color`).
#[no_mangle]
pub unsafe extern "C" fn ee_units_player_set_color(color: *mut [f32; 3], shade: *mut [f32; 3], from: *const [f32; 3]) {
    // SAFETY: the caller's contract
    unsafe {
        let c = *from;
        *color = c;
        *shade = [c[0] * 0.6, c[1] * 0.6, c[2] * 0.6];
    }
}

/// `EEAddDeadUnitStatistic`: the statistics series a dead unit counts in (14 a citizen, 15 a hero, 13 another moving
/// unit that is not a shot; 0 none: a resource, an ambient object, an animal, a path point) from its class's category,
/// family and type and its upgrade's maximum velocity.
pub fn dead_unit_statistic(category: u64, family: u64, unit_type: u64, max_velocity: f32) -> c_long {
    const AMBIENT: u64 = 0;
    const RESOURCE: u64 = 1;
    const LAND_CITIZEN: u64 = 6;
    const PROJECTILE: u64 = 9;
    const BOMB: u64 = 0xb;
    const MISSILE: u64 = 0xc;
    const TORPEDO: u64 = 0x14;
    const NAVAL_CITIZEN: u64 = 0x16;
    const FAMILY_HERO: u64 = 8;
    const FAMILY_ANIMAL: u64 = 0x11;
    const TYPE_PATH_POINT: u64 = 0xdf;
    if category == RESOURCE || category == AMBIENT || family == FAMILY_ANIMAL || unit_type == TYPE_PATH_POINT {
        return 0;
    }
    if category == LAND_CITIZEN || category == NAVAL_CITIZEN {
        14
    } else if family == FAMILY_HERO {
        15
    } else if ![PROJECTILE, MISSILE, BOMB, TORPEDO].contains(&category) && max_velocity > 0.0 {
        13
    } else {
        0
    }
}

/// Runs member `what` on player `p` (`a` an object, `b`, `c` values; the answer for those that have one).
pub fn run<H: RestHost + ?Sized>(h: &mut H, p: u64, what: u32, a: u64, b: i64, c: i64) -> i64 {
    match what {
        EE_UNITS_PLAYER_REST_NEAREST_AREA => return nearest_area(h, p, b as c_long, c as c_long, a != 0) as i64,
        EE_UNITS_PLAYER_REST_CHECKSUM => checksum(h, p, a),
        EE_UNITS_PLAYER_REST_HERDS_OF_TYPE => herds_of_type(h, p, a, b as c_long),
        EE_UNITS_PLAYER_REST_REGISTER => register(h, p, b),
        EE_UNITS_PLAYER_REST_UNIT_REMOVED => unit_removed(h, p, a),
        EE_UNITS_PLAYER_REST_REMOVE_ALLY => remove_ally(h, p, a),
        EE_UNITS_PLAYER_REST_REMOVE_PATROL => remove_patrol_group(h, p, a),
        EE_UNITS_PLAYER_REST_FINISH_UNITS_LOAD => return finish_units_load(h, p, b, c),
        EE_UNITS_PLAYER_REST_UNLOAD_ALL_CLASS_MEDIA => unload_all_class_media(h, p),
        EE_UNITS_PLAYER_REST_LOAD_ALL_CLASS_MEDIA => load_all_class_media(h, p),
        EE_UNITS_PLAYER_REST_UNIT_GOAL_DONE => unit_goal_done(h, p, a),
        EE_UNITS_PLAYER_REST_REFRESH_TOWN_GROUPS => refresh_town_groups(h, p),
        EE_UNITS_PLAYER_REST_RESET_WONDER_TIMERS => reset_wonder_timers(h, p, a),
        EE_UNITS_PLAYER_REST_DELAY_WONDER_TIMERS => delay_wonder_timers(h, p, b as c_long),
        EE_UNITS_PLAYER_REST_WONDER_TIME => {
            let w = h.list_at(p, EE_UNITS_LISTS_WONDERS, b as usize);
            return h.body(EE_UNITS_PLAYER_BODY_CLOAKED, p, w, 0, 0) as c_ulong as c_long as i64;
        }
        EE_UNITS_PLAYER_REST_SET_ALL_RESOURCES => set_all_resources(h, p, b as c_long),
        EE_UNITS_PLAYER_REST_UPDATE_AIR_TRAFFIC => {
            for i in 0..4 {
                h.body(EE_UNITS_PLAYER_BODY_ATC_UPDATE, p, 0, i, 0);
            }
        }
        EE_UNITS_PLAYER_REST_REMOVING_UNIT => removing_unit(h, p, a),
        EE_UNITS_PLAYER_REST_SET_GAME_RESULT => set_game_result(h, p, b as c_long),
        EE_UNITS_PLAYER_REST_ADOPT_TOWN_PATROL => return i64::from(adopt_town_patrol(h, p, a)),
        EE_UNITS_PLAYER_REST_CLEAR_FOOTPRINT => clear_footprint(h, p, a),
        EE_UNITS_PLAYER_REST_RELOAD_TECH_TREE => reload_tech_tree(h, p, b != 0),
        EE_UNITS_PLAYER_REST_MEDIA_CHECK => media_check(h, p, a as c_long, b != 0),
        EE_UNITS_PLAYER_REST_KILL_HERD_UNITS => kill_herd_units(h, p, a, b != 0),
        EE_UNITS_PLAYER_REST_DUMP_MEDIA => dump_media(h, p, a),
        EE_UNITS_PLAYER_REST_ADD_TO_REPATROL => add_to_repatrol(h, p, a),
        _ => {}
    }
    0
}

#[cfg(not(test))]
pub mod game {
    use super::*;
    use crate::units::patrol_group::{
        ee_units_pg_dereference, ee_units_pg_is_town_patrol, ee_units_pg_point, ee_units_pg_point_count,
        ee_units_pg_reference, EeUnitsPatrolGroup,
    };
    use crate::world::layout::layout;
    use crate::world::{PlayerView, UnitView, WorldRead};
    use std::ffi::c_void;

    struct Game;

    fn unit<R>(u: u64, f: impl Fn(&UnitView) -> R, none: R) -> R {
        // SAFETY: the C++ passes a live unit and holds the world for the call
        let t = unsafe { WorldRead::begin() };
        unsafe { UnitView::of(&t, u) }.map_or(none, |v| f(&v))
    }

    fn g(group: u64) -> *mut EeUnitsPatrolGroup {
        group as usize as *mut EeUnitsPatrolGroup
    }

    impl RestHost for Game {
        fn body(&mut self, op: i32, p: u64, a: u64, b: i64, c: i64) -> i64 {
            crate::units::player_body::body_host(op, p, a, b, c)
        }
        fn list_op(&mut self, p: u64, list: u32, op: ListOp) -> u64 {
            crate::units::player_lists::player_op(p, list, op)
        }
        fn media_warning(&mut self, p: u64, text: Vec<u8>) {
            // SAFETY: the live player's lists
            let l = unsafe { &mut *crate::units::player_lists::player_lists(p) };
            l.warnings.push(std::ffi::CString::from_vec_with_nul(text).unwrap_or_default());
        }
        fn media_warnings(&mut self, p: u64) -> Vec<std::ffi::CString> {
            // SAFETY: as above
            unsafe { &*crate::units::player_lists::player_lists(p) }.warnings.clone()
        }
        fn saved(&mut self, p: u64) -> *mut EeUnitsPlayerSaved {
            (p as usize + layout().player_saved as usize) as *mut EeUnitsPlayerSaved
        }
        fn is_complex(&mut self, u: u64) -> bool {
            unit(u, |v| v.is_complex(), false)
        }
        fn on_tile(&mut self, u: u64) -> bool {
            unit(u, |v| v.has_tile_node(), false)
        }
        fn tile(&mut self, u: u64) -> (c_long, c_long) {
            unit(u, |v| (v.tile_xy().0 as c_long, v.tile_xy().1 as c_long), (0, 0))
        }
        fn influence(&mut self, u: u64) -> c_long {
            unit(u, |v| v.class().map_or(0, |c| crate::world_field!(c.fields(), mInfluence)), 0)
        }
        // SAFETY (the patrol groups'): the player's live patrol group
        fn pg_reference(&mut self, group: u64) {
            unsafe { ee_units_pg_reference(g(group)) }
        }
        fn pg_dereference(&mut self, group: u64) {
            unsafe { ee_units_pg_dereference(g(group)) }
        }
        fn pg_is_town_patrol(&mut self, group: u64) -> bool {
            unsafe { ee_units_pg_is_town_patrol(g(group)) }
        }
        fn pg_points(&mut self, group: u64) -> Vec<u64> {
            let n = unsafe { ee_units_pg_point_count(g(group)) } as usize;
            (0..n).map(|i| unsafe { ee_units_pg_point(g(group), i) } as u64).collect()
        }
        fn class_size_type(&mut self, u: u64) -> (c_long, c_long) {
            unit(u, |v| v.class().map_or((0, 0), |c| (c.tile_size() as c_long, c.unit_type() as c_long)), (0, 0))
        }
        fn complex_of(&mut self, u: u64) -> u64 {
            unit(u, |v| v.complex_unit_handle(), 0)
        }
        fn location_z(&mut self, u: u64) -> f32 {
            unit(u, |v| v.location()[2], 0.0)
        }
        // SAFETY (the unit utilities'): the vectors the body made, the live units
        fn units_in_rectangle(&mut self, b: [f32; 4], units: u64) {
            unsafe {
                crate::units::uu_ffi::ee_units_uu_units_in_rectangle(b[0], b[1], b[2], b[3], units as *mut c_void)
            };
        }
        fn block_ring(&mut self, x: c_long, y: c_long, size: c_long, tiles: u64) {
            let (x, y, size) = (x as i32, y as i32, size as i32);
            unsafe { crate::units::uu_ffi::ee_units_uu_block_ring(0, x, y, size, size, tiles as *mut c_void, 0, 1) };
        }
        fn can_stand_on(&mut self, x: c_long, y: c_long, u: u64) -> bool {
            crate::units::uu_ffi::ee_units_uu_can_stand_on(x as i32, y as i32, u) != 0
        }
        fn movable(&mut self, u: u64) -> bool {
            crate::units::uu_ffi::ee_units_uu_is_unit_movable(u) != 0
        }
        fn loaded_media_classes(&mut self, p: u64) -> Vec<c_long> {
            // SAFETY: the live player and its tree; the world is held
            let t = unsafe { WorldRead::begin() };
            let Some(tree) = (unsafe { PlayerView::of(&t, p) }).and_then(|v| v.tech_tree()) else { return Vec::new() };
            let mut ids = Vec::new();
            for e in 0..tree.epochs() {
                tree.epoch_node_ids(e, &mut ids);
            }
            ids.iter()
                .filter_map(|&id| tree.node(id as i64))
                .filter(|n| crate::world_field!(n.fields(), mIsUnitMediaLoaded))
                .filter_map(|n| n.data().map(|d| crate::world_field!(d, mUnitClass)))
                .collect()
        }
        fn epoch_range(&mut self, p: u64) -> (c_long, c_long) {
            // SAFETY: as above
            let t = unsafe { WorldRead::begin() };
            unsafe { PlayerView::of(&t, p) }.and_then(|v| v.tech_tree()).map_or((0, 0), |tree| {
                let f = tree.fields();
                (crate::world_field!(f, mStartEpoch), crate::world_field!(f, mEndEpoch))
            })
        }
        fn reset_tech_tree(&mut self, p: u64) {
            use crate::units::player_body::{run, EE_UNITS_PLAYER_DO_RESET_TECH_TREE};
            run(&mut crate::units::group_host::GameGroups, p, EE_UNITS_PLAYER_DO_RESET_TECH_TREE, 0, 0, 0, 0.0);
        }
        fn unit_player(&mut self, u: u64) -> u64 {
            unit(u, |v| v.player().map_or(0, |p| p.handle()), 0)
        }
        fn player_id(&mut self, p: u64) -> u32 {
            // SAFETY: the live player
            let t = unsafe { WorldRead::begin() };
            unsafe { PlayerView::of(&t, p) }.map_or(0, |v| v.id())
        }
        fn c_string(&mut self, text: u64) -> Vec<u8> {
            if text == 0 {
                return Vec::new();
            }
            // SAFETY: the class's live name (a NUL-terminated string)
            unsafe { std::ffi::CStr::from_ptr(text as usize as *const std::ffi::c_char) }.to_bytes().to_vec()
        }
        fn refresh_upgrades(&mut self, p: u64) {
            // SAFETY: the live player (selection.rs's refresh_upgrades)
            unsafe {
                crate::units::group_host::ee_units_selection(
                    p as *mut c_void,
                    10,
                    std::ptr::null_mut(),
                    0,
                    std::ptr::null(),
                    0,
                    false,
                    false,
                )
            };
        }
    }

    /// `EEAddDeadUnitStatistic(unit)` (an `EEComplexUnit*` as its `RTSUnit` address; see [`dead_unit_statistic`]).
    #[no_mangle]
    pub extern "C" fn ee_units_dead_unit_statistic(unit: u64) -> c_long {
        // SAFETY: the C++ passes a live unit and holds the world for the call
        let t = unsafe { WorldRead::begin() };
        let Some(u) = (unsafe { UnitView::of(&t, unit) }) else { return 0 };
        let Some(c) = u.class() else { return 0 };
        let velocity: f32 = u.stats().map_or(0.0, |s| crate::world_field!(s.fields(), mMaxVelocity));
        dead_unit_statistic(c.category(), c.family(), c.unit_type(), velocity)
    }

    struct GameCloak;

    impl CloakHost for GameCloak {
        fn flag(&mut self, u: u64, which: u32) -> bool {
            unit(u, |v| if which == CLOAKABLE { v.cloakable() } else { v.cloaked() }, false)
        }
        fn set_flag(&mut self, u: u64, which: u32, value: bool) {
            let record = (u as usize + layout().complex_unit_fields as usize)
                as *mut crate::world::unit_fields::EeUnitsComplexUnitFields;
            // SAFETY: a live complex unit's record (the C++ passes one; no reference kept)
            unsafe {
                if which == CLOAKABLE {
                    (*record).mIsCloakable = value;
                } else {
                    (*record).mIsCloaked = value;
                }
            }
        }
        fn next_part(&mut self, u: u64) -> u64 {
            unit(u, |v| v.next_part_handle(), 0)
        }
    }

    /// `EEComplexUnit::SetCloakableChain` (`which` 0) and `SetCloakedChain` (1) of complex unit `unit` (its
    /// `RTSUnit` address).
    #[no_mangle]
    pub extern "C" fn ee_units_cloak_chain(unit: u64, which: u32, value: bool) {
        if which == CLOAKABLE {
            set_cloakable_chain(&mut GameCloak, unit, value);
        } else {
            set_cloaked_chain(&mut GameCloak, unit, value);
        }
    }

    /// The player's member `what` (`EE_UNITS_PLAYER_REST_*`) on `player`.
    ///
    /// # Safety
    /// `player` a live player; `a` the member's object where it takes one.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_player_rest(player: *mut c_void, what: u32, a: u64, b: i64, c: i64) -> i64 {
        run(&mut Game, player as u64, what, a, b, c)
    }
}

#[cfg(test)]
#[path = "player_rest_tests.rs"]
mod tests;
