//! The unit host's world calls whose C++ was more than one statement (the units' lane, round 8; were EEUnitHost.cpp's
//! `ee_units_host_*` bodies): the map height, the town patrol point, the build and repair orders, the patrol routes,
//! the path's point, the wall plan, the building, the farms and their drop sites, the calamity class and its casters,
//! the ghost test, the garrison's rally load, the load goals, the transport filter, the research node and the cheat's
//! units. Every C++ statement of the originals is one `EE_UNITS_UH_*` case of `ee_units_uh_host` (EEUnitHost.cpp),
//! asked in the original's order; units are ids (resolved through the registry by the statement), other objects
//! handles. The lists the originals made as locals are the host's file statics, cleared where they were declared.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::units::host::{
    EE_UNITS_LOAD_CAN_GARRISON, EE_UNITS_LOAD_MOVES, EE_UNITS_LOAD_TRANSPORT, EE_UNITS_RESEARCH_BUNDESWEHR,
    EE_UNITS_RESEARCH_EPOCH, EE_UNITS_RESEARCH_FOUND, EE_UNITS_RESEARCH_HERO, EE_UNITS_RESEARCH_MAKES_UNIT,
};

// ---- the statements (`ee_units_uh_host`'s `op`; `a` an id or object, `b` an object, `c`, `d` values, `f` floats)
/// `f[0] = gWorld.GetMap()->GetHeight(GE3DPoint(f[0], f[1], 0))`
pub const EE_UNITS_UH_MAP_HEIGHT: i32 = 1;
/// unit `a`'s `FindNearestTown()`
pub const EE_UNITS_UH_NEAREST_TOWN: i32 = 2;
/// town `b`'s `mPatrolGroup`
pub const EE_UNITS_UH_TOWN_PATROL_GROUP: i32 = 3;
/// unit `a`'s `GetTileX()`, `GetTileY()`
pub const EE_UNITS_UH_TILE_X: i32 = 4;
pub const EE_UNITS_UH_TILE_Y: i32 = 5;
/// object `b`'s `mUnitID` (an `RTSUnit`)
pub const EE_UNITS_UH_OBJECT_ID: i32 = 6;
/// `UnitOf(a)->mComplexUnit->AddBuildTarget(UnitOf(b))`
pub const EE_UNITS_UH_ADD_BUILD_TARGET: i32 = 7;
/// `UnitOf(a)->mComplexUnit->mHasShiftQueuedBuild = true`
pub const EE_UNITS_UH_SET_SHIFT_QUEUED: i32 = 8;
/// `UnitOf(a)->mComplexUnit->OrderRepair(UnitOf(b))`
pub const EE_UNITS_UH_ORDER_REPAIR: i32 = 9;
/// `UnitOf(a)->mComplexUnit->StartAction(c)`
pub const EE_UNITS_UH_START_ACTION: i32 = 10;
/// `UnitOf(a)->mComplexUnit`'s class's `ActionChanged`
pub const EE_UNITS_UH_ACTION_CHANGED: i32 = 11;
/// `ComplexOf(a)->StartAction(c)`
pub const EE_UNITS_UH_C_START_ACTION: i32 = 12;
/// `ComplexOf(a)`'s class's `ActionChanged`
pub const EE_UNITS_UH_C_ACTION_CHANGED: i32 = 13;
/// player `a`'s `AddPatrolGroup(group b)`
pub const EE_UNITS_UH_ADD_PATROL_GROUP: i32 = 14;
/// player `a`'s `mPlayerID`
pub const EE_UNITS_UH_PLAYER_ID: i32 = 15;
/// `EEPatrolGoalCast(ComplexOf(a)->GetGoal())`
pub const EE_UNITS_UH_PATROL_GOAL: i32 = 16;
/// goal `b`'s `GetPatrolGroup()`
pub const EE_UNITS_UH_GOAL_PATROL_GROUP: i32 = 17;
/// `gWorld.GetMap()->GetTile(c, d)`
pub const EE_UNITS_UH_MAP_TILE: i32 = 18;
/// `gWorld.GetMap()->GetClosestTile(&(` the point `= GE3DPoint(f[0], f[1], 0)), &(` x `= c), &(` y `= d))`
pub const EE_UNITS_UH_CLOSEST_TILE: i32 = 19;
/// the shared x, y
pub const EE_UNITS_UH_X: i32 = 20;
pub const EE_UNITS_UH_Y: i32 = 21;
/// tile `b`'s `mUnits`
pub const EE_UNITS_UH_TILE_UNITS: i32 = 22;
/// tile node `b`'s `mNext`
pub const EE_UNITS_UH_NODE_NEXT: i32 = 23;
/// tile node `b`'s unit is a building unit `a` cannot pass
pub const EE_UNITS_UH_NODE_BLOCKS: i32 = 24;
/// `ComplexOf(a)->GetNearestFootprintTile(` node `b`'s unit`, &(` x `= c), &(` y `= d))`
pub const EE_UNITS_UH_NEAREST_FOOTPRINT: i32 = 25;
/// `f[0] = ` tile `b`'s `mHeight`
pub const EE_UNITS_UH_TILE_HEIGHT: i32 = 26;
/// `new std::vector<EEUnit*>()`
pub const EE_UNITS_UH_NEW_PIECES: i32 = 27;
/// `gUnitClassTable[` player `a`'s `mTechnologies->Get(c)->mData->mUnitClass]`
pub const EE_UNITS_UH_TECH_CLASS: i32 = 28;
/// list `b`'s `size()`
pub const EE_UNITS_UH_PIECES_SIZE: i32 = 29;
/// list `b`'s `[c]->mUnitID`
pub const EE_UNITS_UH_PIECE_ID: i32 = 30;
/// `delete` list `b`
pub const EE_UNITS_UH_DELETE_PIECES: i32 = 31;
/// `EEMakeBuilding(` player `a, b, c, d)`
pub const EE_UNITS_UH_MAKE_BUILDING: i32 = 32;
/// `ComplexOf(a)`
pub const EE_UNITS_UH_COMPLEX: i32 = 33;
/// the ring `= 1`
pub const EE_UNITS_UH_RESET_RING: i32 = 34;
/// `EEFindFreeFarm(` complex unit `b, 1, &` the ring `)`
pub const EE_UNITS_UH_FIND_FREE_FARM: i32 = 35;
/// the shared complex unit list's `clear()`, `push_back(ComplexOf(a))`, `size()`, `[c]->mUnitID`
pub const EE_UNITS_UH_UNITS_CLEAR: i32 = 36;
pub const EE_UNITS_UH_UNITS_PUSH: i32 = 37;
pub const EE_UNITS_UH_UNITS_SIZE: i32 = 38;
pub const EE_UNITS_UH_UNITS_ID: i32 = 39;
/// `EESortFarms(` the list`, c, d, b != 0)`
pub const EE_UNITS_UH_SORT_FARMS: i32 = 40;
/// `EEGatherGoalCast(ComplexOf(a)->GetLoadGoal())`
pub const EE_UNITS_UH_LOAD_GATHER_GOAL: i32 = 42;
/// goal `b`'s `GetGatherResource()`
pub const EE_UNITS_UH_GATHER_RESOURCE: i32 = 43;
/// unit `b`'s `mUnitClass->mUnitType`
pub const EE_UNITS_UH_UNIT_TYPE_OF: i32 = 44;
/// unit `b`'s `mComplexUnit`
pub const EE_UNITS_UH_COMPLEX_OF_UNIT: i32 = 45;
/// `ComplexOf(a)->CanTake(ComplexOf(b), ComplexOf(b)->mResourceType)`
pub const EE_UNITS_UH_CAN_TAKE: i32 = 46;
/// player `a`'s `mTechnologies->Get(c)`
pub const EE_UNITS_UH_TECH_NODE: i32 = 47;
/// the calamity class of node `b`'s `mData->mCalamity`
pub const EE_UNITS_UH_NODE_CALAMITY_CLASS: i32 = 48;
/// calamity class `b`'s `mClassEnum`, `mIsCastByAllSelected`
pub const EE_UNITS_UH_CLASS_ENUM: i32 = 49;
pub const EE_UNITS_UH_CAST_BY_ALL: i32 = 50;
/// calamity class `b`'s `CanCastAt(ComplexOf(a), ` the point`, ` c's low and high words`, true, ` unit d or NULL`)`
pub const EE_UNITS_UH_CAN_CAST_AT: i32 = 51;
/// the point `= GE3DPoint()`
pub const EE_UNITS_UH_RESET_POINT: i32 = 52;
/// active group `a`'s `SplitCalamityCasters(` class `b, ` node `c, ` unit d or NULL`)`
pub const EE_UNITS_UH_SPLIT_CASTERS: i32 = 53;
/// `UnitOf(a) != NULL ? UnitOf(a)->mTile : NULL`
pub const EE_UNITS_UH_UNIT_TILE: i32 = 54;
/// tile `b`'s `GetNumDoppelgangers()`, its doppelganger `c`'s `mUnit`
pub const EE_UNITS_UH_TILE_DOPPELGANGERS: i32 = 55;
pub const EE_UNITS_UH_TILE_DOPPELGANGER_UNIT: i32 = 56;
/// `UnitOf(a)`
pub const EE_UNITS_UH_UNIT_OF: i32 = 57;
/// unit `a`'s class can be attacked and takes damage of kind `c`
pub const EE_UNITS_UH_ATTACKABLE: i32 = 58;
/// `ee_units_atc_clear_airport_queue(` unit `a`'s air traffic`, ComplexOf(a), 0)`
pub const EE_UNITS_UH_CLEAR_AIR_TRAFFIC: i32 = 59;
/// `EEUGRust::NewLoad(kGoalLoadGarrison, ComplexOf(a), ` the list`, true)`
pub const EE_UNITS_UH_NEW_GARRISON_LOAD: i32 = 60;
/// goal `b`'s `IsLoadGarrisonValid()`
pub const EE_UNITS_UH_LOAD_VALID: i32 = 61;
/// `delete` goal `b`
pub const EE_UNITS_UH_DELETE_GOAL: i32 = 62;
/// `ComplexOf(a)->SetGoal(` goal `b, 0, 0)`
pub const EE_UNITS_UH_SET_GOAL: i32 = 63;
/// `f[0] = ComplexOf(a)->mStats->mMaxVelocity`
pub const EE_UNITS_UH_MAX_VELOCITY: i32 = 64;
/// goal `b`'s `Clone()`
pub const EE_UNITS_UH_GOAL_CLONE: i32 = 65;
/// `ComplexOf(a)->mGoalState`, `->GetLoadGoal()`, goal `b`'s `mKind`
pub const EE_UNITS_UH_GOAL_STATE: i32 = 67;
pub const EE_UNITS_UH_LOAD_GOAL: i32 = 68;
pub const EE_UNITS_UH_GOAL_KIND: i32 = 69;
/// goal `b`'s `AddLoadUnits(` the list `)`
pub const EE_UNITS_UH_ADD_LOAD_UNITS: i32 = 70;
/// `EEUGRust::NewLoad(c, ComplexOf(a), ` the list`, d != 0)`
pub const EE_UNITS_UH_NEW_LOAD: i32 = 71;
/// the tile `= ` goal `b`'s `GetLoadUnitTile()`; its x, y
pub const EE_UNITS_UH_LOAD_UNIT_TILE: i32 = 72;
pub const EE_UNITS_UH_TILE_POINT_X: i32 = 73;
pub const EE_UNITS_UH_TILE_POINT_Y: i32 = 74;
/// the rejected list's `clear()`
pub const EE_UNITS_UH_REJECTED_CLEAR: i32 = 75;
/// unit `a`'s transport class's `FilterLoadable(&` the list`, ComplexOf(a), &` the rejected `)`
pub const EE_UNITS_UH_FILTER: i32 = 76;
/// the rejected list's `size()`, `[c]->mUnitID`
pub const EE_UNITS_UH_REJECTED_SIZE: i32 = 77;
pub const EE_UNITS_UH_REJECTED_ID: i32 = 78;
/// player `a`'s `GetTechTree()->Get(c)`
pub const EE_UNITS_UH_TREE_NODE: i32 = 79;
/// node `b`'s `mData->mKind`
pub const EE_UNITS_UH_NODE_KIND: i32 = 80;
/// node `b`'s unit class is a hero's
pub const EE_UNITS_UH_NODE_HERO: i32 = 81;
/// the Bundeswehr power's technology is node `b`'s first prerequisite
pub const EE_UNITS_UH_NODE_BUNDESWEHR: i32 = 82;
/// node `b`'s `mData->mMakesUnit`
pub const EE_UNITS_UH_NODE_MAKES_UNIT: i32 = 83;
/// `EEIsEpochID(c)`
pub const EE_UNITS_UH_IS_EPOCH: i32 = 84;
/// `gWorld.GetMap()->GetGroundPoint(GE3DPoint(f[0], f[1], 0), ` the ground `)`
pub const EE_UNITS_UH_GROUND_POINT: i32 = 86;
/// the transformation `= GETransformation()`
pub const EE_UNITS_UH_RESET_TRANSFORMATION: i32 = 87;
/// the transformation's `SetOrientationYPR(3.1415927f, 0, 0)`
pub const EE_UNITS_UH_FACE_PI: i32 = 88;
/// the transformation's `SetPosition(` the ground `)`
pub const EE_UNITS_UH_POSITION_AT_GROUND: i32 = 89;
/// `gUnitClassTable[c]->CreateUnitAt(` the transformation`, d, false, NULL)`
pub const EE_UNITS_UH_CREATE_AT: i32 = 90;
/// unit `b`'s `mTileNode != NULL`
pub const EE_UNITS_UH_HAS_TILE_NODE: i32 = 91;
/// unit `b`'s `mComplexUnit->SetReloadTimer(0)`
pub const EE_UNITS_UH_CLEAR_RELOAD: i32 = 92;
/// unit `b`'s `mComplexUnit->SetPower(` its stats' power `)`
pub const EE_UNITS_UH_FULL_POWER: i32 = 93;
/// `gWorld.mPlayers[d]->mUnitClassUpgrades[gUnitClassTable[c]->mUnitType]->mCreationSound`
pub const EE_UNITS_UH_UPGRADE_CREATION_SOUND: i32 = 94;
/// `gWorld.GetCurrentPlayer()->mPlayerID`
pub const EE_UNITS_UH_CURRENT_PLAYER_ID: i32 = 95;
/// `gSoundManager->Play(` sound `b`'s sample and volume`, 0, 1, false, NULL)`
pub const EE_UNITS_UH_PLAY_SOUND: i32 = 96;
/// `gUnitClassTable[a]->CreateUnit(c, d, b, false, NULL)`
pub const EE_UNITS_UH_CREATE_BUILDING: i32 = 97;
/// unit `b`'s `mComplexUnit->SetBuilt(1, 0, 1)`
pub const EE_UNITS_UH_SET_BUILT: i32 = 98;
/// unit `b`'s `mStats->mCreationSound`
pub const EE_UNITS_UH_STATS_CREATION_SOUND: i32 = 99;
/// `ComplexOf(a)->mUnitClass->mCanGarrison`, `->IsTransport()`
pub const EE_UNITS_UH_CAN_GARRISON: i32 = 100;
pub const EE_UNITS_UH_IS_TRANSPORT: i32 = 101;

const NONE: u32 = u32::MAX;
const UNIT_TYPE_FARM: i64 = 0x2a;
const GOAL_LOAD_GARRISON: u8 = 12;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
const ACTION_PICKED_UP_LOADABLE_UNITS: i64 = 12003;

/// The C++ statements and the Rust the bodies call.
pub trait UhHost {
    /// one statement (`EE_UNITS_UH_*`)
    fn uh(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64;
    /// `ee_units_pg_nearest(group, x, y)`
    fn pg_nearest(&mut self, group: u64, x: i32, y: i32) -> u64;
    /// `ee_units_pg_new()`
    fn pg_new(&mut self) -> u64;
    /// `ee_units_pg_add_point(group, x, y, player)`
    fn pg_add_point(&mut self, group: u64, x: f32, y: f32, player: u32);
    /// `ee_units_pg_dereference(group)`
    fn pg_dereference(&mut self, group: u64);
    /// `ee_units_uu_plan_wall(start, end, class, player, 1, 0, pieces)`
    fn plan_wall(&mut self, start: (f32, f32), end: (f32, f32), class: u64, player: u64, pieces: u64);
    /// `ee_units_uu_farm_drop_sites(farm, sites)`
    fn farm_drop_sites(&mut self, farm: u64, sites: u64) -> bool;
}

/// A statement over values.
fn q<H: UhHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
    h.uh(op, a, b, c, d, &mut [0.0; 4])
}

fn qa<H: UhHost + ?Sized>(h: &mut H, op: i32, a: u64) -> i64 {
    q(h, op, a, 0, 0, 0)
}

fn qb<H: UhHost + ?Sized>(h: &mut H, op: i32, b: u64) -> i64 {
    q(h, op, 0, b, 0, 0)
}

/// A statement's float answer (`f[0]`), the floats `f` in.
fn qf<H: UhHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, f: [f32; 4]) -> f32 {
    let mut f = f;
    h.uh(op, a, b, 0, 0, &mut f);
    f[0]
}

fn id(unit: u32) -> u64 {
    u64::from(unit)
}

/// `ee_units_host_map_height`: the map's height at (x, y).
pub fn map_height<H: UhHost + ?Sized>(h: &mut H, x: f32, y: f32) -> f32 {
    qf(h, EE_UNITS_UH_MAP_HEIGHT, 0, 0, [x, y, 0.0, 0.0])
}

/// `ee_units_host_town_patrol_point`: the unit's nearest town; when it has a patrol group, that group's point
/// nearest to the unit's tile (its id, or none).
pub fn town_patrol_point<H: UhHost + ?Sized>(h: &mut H, unit: u32) -> Option<u32> {
    let site = qa(h, EE_UNITS_UH_NEAREST_TOWN, id(unit)) as u64;
    if site == 0 || qb(h, EE_UNITS_UH_TOWN_PATROL_GROUP, site) == 0 {
        return None;
    }
    let group = qb(h, EE_UNITS_UH_TOWN_PATROL_GROUP, site) as u64;
    let x = qa(h, EE_UNITS_UH_TILE_X, id(unit)) as i32;
    let y = qa(h, EE_UNITS_UH_TILE_Y, id(unit)) as i32;
    let nearest = h.pg_nearest(group, x, y);
    Some(if nearest != 0 { qb(h, EE_UNITS_UH_OBJECT_ID, nearest) as u32 } else { NONE })
}

/// `ee_units_host_unit_add_build_target`: the target to the unit's build targets; no smart task after it.
pub fn unit_add_build_target<H: UhHost + ?Sized>(h: &mut H, unit: u32, target: u32) {
    q(h, EE_UNITS_UH_ADD_BUILD_TARGET, id(unit), id(target), 0, 0);
    qa(h, EE_UNITS_UH_SET_SHIFT_QUEUED, id(unit));
}

/// `ee_units_host_unit_order_repair`: the repair ordered, the unit idle.
pub fn unit_order_repair<H: UhHost + ?Sized>(h: &mut H, unit: u32, target: u32) {
    q(h, EE_UNITS_UH_ORDER_REPAIR, id(unit), id(target), 0, 0);
    q(h, EE_UNITS_UH_START_ACTION, id(unit), 0, 5000, 0);
    qa(h, EE_UNITS_UH_ACTION_CHANGED, id(unit));
}

/// `ee_units_host_unit_start_action`
pub fn unit_start_action<H: UhHost + ?Sized>(h: &mut H, unit: u32, action: i32) {
    q(h, EE_UNITS_UH_C_START_ACTION, id(unit), 0, i64::from(action), 0);
    qa(h, EE_UNITS_UH_C_ACTION_CHANGED, id(unit));
}

/// `ee_units_host_patrol_new_lone`: a lone patrol group with the point, registered with the player.
pub fn patrol_new_lone<H: UhHost + ?Sized>(h: &mut H, player: u64, x: f32, y: f32) {
    let group = h.pg_new();
    let player_id = qa(h, EE_UNITS_UH_PLAYER_ID, player) as u32;
    h.pg_add_point(group, x, y, player_id);
    q(h, EE_UNITS_UH_ADD_PATROL_GROUP, player, group, 0, 0);
    h.pg_dereference(group);
}

/// `ee_units_host_unit_goal_patrol_route`: the route of the unit's current patrol goal (0: none).
pub fn unit_goal_patrol_route<H: UhHost + ?Sized>(h: &mut H, unit: u32) -> u64 {
    let goal = qa(h, EE_UNITS_UH_PATROL_GOAL, id(unit)) as u64;
    if goal != 0 {
        qb(h, EE_UNITS_UH_GOAL_PATROL_GROUP, goal) as u64
    } else {
        0
    }
}

/// `ee_units_host_goal_patrol_add_point`: the point added to a new patrol goal's route, the route registered.
pub fn goal_patrol_add_point<H: UhHost + ?Sized>(h: &mut H, goal: u64, x: f32, y: f32, player: u64) {
    let group = qb(h, EE_UNITS_UH_GOAL_PATROL_GROUP, goal) as u64;
    let player_id = qa(h, EE_UNITS_UH_PLAYER_ID, player) as u32;
    h.pg_add_point(group, x, y, player_id);
    let group = qb(h, EE_UNITS_UH_GOAL_PATROL_GROUP, goal) as u64;
    q(h, EE_UNITS_UH_ADD_PATROL_GROUP, player, group, 0, 0);
}

/// `ee_units_host_path_to_point` (EEMPathTo's point): a point off the map to the closest tile's centre; a tile under a
/// building the first unit cannot pass to the building's nearest footprint tile; the height the final tile's (at
/// least 0). Returns (x, y, z).
pub fn path_to_point<H: UhHost + ?Sized>(h: &mut H, first: u32, x: f32, y: f32) -> (f32, f32, f32) {
    let (mut px, mut py) = (x, y);
    let mut tx = px as c_long;
    let mut ty = py as c_long;
    let mut tile = q(h, EE_UNITS_UH_MAP_TILE, 0, 0, i64::from(tx), i64::from(ty)) as u64;
    if tile == 0 {
        let mut f = [tx as f32 + 0.5, ty as f32 + 0.5, 0.0, 0.0];
        tile = h.uh(EE_UNITS_UH_CLOSEST_TILE, 0, 0, i64::from(tx), i64::from(ty), &mut f) as u64;
        tx = q(h, EE_UNITS_UH_X, 0, 0, 0, 0) as c_long;
        ty = q(h, EE_UNITS_UH_Y, 0, 0, 0, 0) as c_long;
        px = tx as f32 + 0.5;
        py = ty as f32 + 0.5;
    }
    let mut node = qb(h, EE_UNITS_UH_TILE_UNITS, tile) as u64;
    while node != 0 {
        if q(h, EE_UNITS_UH_NODE_BLOCKS, id(first), node, 0, 0) != 0 {
            q(h, EE_UNITS_UH_NEAREST_FOOTPRINT, id(first), node, i64::from(tx), i64::from(ty));
            tx = q(h, EE_UNITS_UH_X, 0, 0, 0, 0) as c_long;
            ty = q(h, EE_UNITS_UH_Y, 0, 0, 0, 0) as c_long;
            tile = q(h, EE_UNITS_UH_MAP_TILE, 0, 0, i64::from(tx), i64::from(ty)) as u64;
            if tile != 0 {
                px = tx as f32 + 0.5;
                py = ty as f32 + 0.5;
                break;
            }
            tx = px as c_long;
            ty = py as c_long;
            tile = q(h, EE_UNITS_UH_MAP_TILE, 0, 0, i64::from(tx), i64::from(ty)) as u64;
        }
        node = qb(h, EE_UNITS_UH_NODE_NEXT, node) as u64;
    }
    let height = qf(h, EE_UNITS_UH_TILE_HEIGHT, 0, tile, [0.0; 4]);
    (px, py, if height >= 0.0 { height } else { 0.0 })
}

/// `ee_units_host_wall_plan`: the wall of the technology's class from start to end as paid foundations, in a new
/// list (the caller releases it); (the list, its length).
pub fn wall_plan<H: UhHost + ?Sized>(
    h: &mut H,
    player: u64,
    technology: i32,
    start: (f32, f32),
    end: (f32, f32),
) -> (u64, usize) {
    let pieces = q(h, EE_UNITS_UH_NEW_PIECES, 0, 0, 0, 0) as u64;
    let class = q(h, EE_UNITS_UH_TECH_CLASS, player, 0, i64::from(technology), 0) as u64;
    h.plan_wall(start, end, class, player, pieces);
    (pieces, qb(h, EE_UNITS_UH_PIECES_SIZE, pieces) as usize)
}

/// `ee_units_host_make_building`: the building's id (none: no building).
pub fn make_building<H: UhHost + ?Sized>(h: &mut H, player: u64, unit_type: i64, x: i64, y: i64) -> u32 {
    let building = q(h, EE_UNITS_UH_MAKE_BUILDING, player, unit_type as u64, x, y) as u64;
    if building != 0 {
        qb(h, EE_UNITS_UH_OBJECT_ID, building) as u32
    } else {
        NONE
    }
}

/// `ee_units_host_free_farms`: `EEFindFreeFarm` from ring 1 until it finds none (none for a unit no id resolves).
pub fn free_farms<H: UhHost + ?Sized>(h: &mut H, unit: u32, out: &mut Vec<u32>) {
    out.clear();
    let complex = qa(h, EE_UNITS_UH_COMPLEX, id(unit)) as u64;
    if complex == 0 {
        return;
    }
    q(h, EE_UNITS_UH_RESET_RING, 0, 0, 0, 0);
    loop {
        let farm = qb(h, EE_UNITS_UH_FIND_FREE_FARM, complex) as u64;
        if farm == 0 {
            break;
        }
        out.push(qb(h, EE_UNITS_UH_OBJECT_ID, farm) as u32);
    }
}

/// `ee_units_host_sort_farms`: the farms sorted by `EESortFarms` (unchanged when one is not known).
pub fn sort_farms<H: UhHost + ?Sized>(h: &mut H, farms: &mut [u32], x: i64, y: i64, greater: bool) {
    for &farm in farms.iter() {
        if qa(h, EE_UNITS_UH_COMPLEX, id(farm)) == 0 {
            return;
        }
    }
    units_in(h, farms);
    q(h, EE_UNITS_UH_SORT_FARMS, 0, u64::from(greater), x, y);
    for (k, farm) in farms.iter_mut().enumerate() {
        *farm = q(h, EE_UNITS_UH_UNITS_ID, 0, 0, k as i64, 0) as u32;
    }
}

/// The shared complex unit list from ids.
fn units_in<H: UhHost + ?Sized>(h: &mut H, units: &[u32]) {
    q(h, EE_UNITS_UH_UNITS_CLEAR, 0, 0, 0, 0);
    for &unit in units {
        qa(h, EE_UNITS_UH_UNITS_PUSH, id(unit));
    }
}

/// `ee_units_host_unit_load_farm`: the farm of the unit's gather goal (none: not a farm).
pub fn unit_load_farm<H: UhHost + ?Sized>(h: &mut H, unit: u32) -> u32 {
    let goal = qa(h, EE_UNITS_UH_LOAD_GATHER_GOAL, id(unit)) as u64;
    if goal == 0 {
        return NONE;
    }
    let resource = qb(h, EE_UNITS_UH_GATHER_RESOURCE, goal) as u64;
    if resource == 0 || qb(h, EE_UNITS_UH_UNIT_TYPE_OF, resource) != UNIT_TYPE_FARM {
        return NONE;
    }
    let complex = qb(h, EE_UNITS_UH_COMPLEX_OF_UNIT, resource) as u64;
    if complex != 0 {
        qb(h, EE_UNITS_UH_OBJECT_ID, complex) as u32
    } else {
        NONE
    }
}

/// `ee_units_host_farm_drop_sites`: `EEGetFarmDropSites`' sites (ids) and whether it found one.
pub fn farm_drop_sites<H: UhHost + ?Sized>(h: &mut H, farm: u32, out: &mut Vec<u32>) -> bool {
    out.clear();
    let sites = q(h, EE_UNITS_UH_NEW_PIECES, 0, 0, 0, 0) as u64;
    let complex = qa(h, EE_UNITS_UH_COMPLEX, id(farm)) as u64;
    let found = h.farm_drop_sites(complex, sites);
    let n = qb(h, EE_UNITS_UH_PIECES_SIZE, sites);
    for k in 0..n {
        out.push(q(h, EE_UNITS_UH_PIECE_ID, 0, sites, k, 0) as u32);
    }
    qb(h, EE_UNITS_UH_DELETE_PIECES, sites);
    found
}

/// `ee_units_host_calamity_of`: the technology's calamity class (EEMCastCalamity), its enum and cast-by-all flag.
pub fn calamity_of<H: UhHost + ?Sized>(h: &mut H, player: u64, technology: i32) -> (u64, i32, bool) {
    let node = q(h, EE_UNITS_UH_TECH_NODE, player, 0, i64::from(technology), 0) as u64;
    let class = if node != 0 { qb(h, EE_UNITS_UH_NODE_CALAMITY_CLASS, node) as u64 } else { 0 };
    if class == 0 {
        return (0, 0, false);
    }
    let class_enum = qb(h, EE_UNITS_UH_CLASS_ENUM, class) as i32;
    let all = qb(h, EE_UNITS_UH_CAST_BY_ALL, class) != 0;
    (class, class_enum, all)
}

/// `ee_units_host_calamity_can_cast_at`: `CanCastAt` with the check (the location its unused out-value).
pub fn calamity_can_cast_at<H: UhHost + ?Sized>(h: &mut H, class: u64, unit: u32, x: i32, y: i32, target: u32) -> i64 {
    q(h, EE_UNITS_UH_RESET_POINT, 0, 0, 0, 0);
    let xy = i64::from(x as u32) | i64::from(y as u32) << 32;
    q(h, EE_UNITS_UH_CAN_CAST_AT, id(unit), class, xy, i64::from(target))
}

/// `ee_units_host_group_split_calamity_casters`
pub fn group_split_calamity_casters<H: UhHost + ?Sized>(
    h: &mut H,
    group: u64,
    class: u64,
    player: u64,
    technology: i32,
    target: u32,
) {
    let node = q(h, EE_UNITS_UH_TECH_NODE, player, 0, i64::from(technology), 0);
    q(h, EE_UNITS_UH_SPLIT_CASTERS, group, class, node, i64::from(target));
}

/// `ee_units_host_unit_ghost_seen`: the unit's tile has its doppelganger (a unit off the tiles has none).
pub fn unit_ghost_seen<H: UhHost + ?Sized>(h: &mut H, unit: u32) -> bool {
    let tile = qa(h, EE_UNITS_UH_UNIT_TILE, id(unit)) as u64;
    if tile == 0 {
        return false;
    }
    let object = qa(h, EE_UNITS_UH_UNIT_OF, id(unit));
    let n = qb(h, EE_UNITS_UH_TILE_DOPPELGANGERS, tile);
    (0..n).any(|i| q(h, EE_UNITS_UH_TILE_DOPPELGANGER_UNIT, 0, tile, i, 0) == object)
}

/// `ee_units_host_garrison_rally_load` (EEMSetRallyGoal's garrison): a new load goal the garrison takes when it is
/// valid; its clone (none when not valid).
pub fn garrison_rally_load<H: UhHost + ?Sized>(h: &mut H, garrison: u32) -> u64 {
    q(h, EE_UNITS_UH_UNITS_CLEAR, 0, 0, 0, 0);
    let load = qa(h, EE_UNITS_UH_NEW_GARRISON_LOAD, id(garrison)) as u64;
    if qb(h, EE_UNITS_UH_LOAD_VALID, load) == 0 {
        qb(h, EE_UNITS_UH_DELETE_GOAL, load);
        return 0;
    }
    q(h, EE_UNITS_UH_SET_GOAL, id(garrison), load, 0, 0);
    let action = if qf(h, EE_UNITS_UH_MAX_VELOCITY, id(garrison), 0, [0.0; 4]) > 0.0 {
        ACTION_PREPARE_TO_MOVE
    } else {
        ACTION_PICKED_UP_LOADABLE_UNITS
    };
    q(h, EE_UNITS_UH_C_START_ACTION, id(garrison), 0, action, 0);
    qa(h, EE_UNITS_UH_C_ACTION_CHANGED, id(garrison));
    qb(h, EE_UNITS_UH_GOAL_CLONE, load) as u64
}

/// `ee_units_host_unit_load_traits`: can garrison, is a transport, moves.
pub fn unit_load_traits<H: UhHost + ?Sized>(h: &mut H, unit: u32) -> u8 {
    let garrison = qa(h, EE_UNITS_UH_CAN_GARRISON, id(unit)) != 0;
    let transport = qa(h, EE_UNITS_UH_IS_TRANSPORT, id(unit)) != 0;
    let moves = qf(h, EE_UNITS_UH_MAX_VELOCITY, id(unit), 0, [0.0; 4]) > 0.0;
    (if garrison { EE_UNITS_LOAD_CAN_GARRISON } else { 0 })
        | (if transport { EE_UNITS_LOAD_TRANSPORT } else { 0 })
        | (if moves { EE_UNITS_LOAD_MOVES } else { 0 })
}

/// `ee_units_host_unit_load_goal`: a garrison's `mGoalState`, a transport's `GetLoadGoal()`, when of the kind.
pub fn unit_load_goal<H: UhHost + ?Sized>(h: &mut H, unit: u32, kind: u8) -> u64 {
    let op = if kind == GOAL_LOAD_GARRISON { EE_UNITS_UH_GOAL_STATE } else { EE_UNITS_UH_LOAD_GOAL };
    let goal = qa(h, op, id(unit)) as u64;
    if goal != 0 && qb(h, EE_UNITS_UH_GOAL_KIND, goal) != i64::from(kind) {
        return 0;
    }
    goal
}

/// `ee_units_host_load_goal_add_units`
pub fn load_goal_add_units<H: UhHost + ?Sized>(h: &mut H, goal: u64, units: &[u32]) {
    units_in(h, units);
    qb(h, EE_UNITS_UH_ADD_LOAD_UNITS, goal);
}

/// `ee_units_host_load_goal_new`: `EEUGRust::NewLoad` (the ids sorted, none twice).
pub fn load_goal_new<H: UhHost + ?Sized>(h: &mut H, kind: u8, carrier: u32, units: &[u32], flag: bool) -> u64 {
    units_in(h, units);
    q(h, EE_UNITS_UH_NEW_LOAD, id(carrier), 0, i64::from(kind), i64::from(flag)) as u64
}

/// `ee_units_host_load_goal_fields`: `IsLoadGarrisonValid`, `GetLoadUnitTile`.
pub fn load_goal_fields<H: UhHost + ?Sized>(h: &mut H, goal: u64) -> (bool, i64, i64) {
    qb(h, EE_UNITS_UH_LOAD_UNIT_TILE, goal);
    let x = q(h, EE_UNITS_UH_TILE_POINT_X, 0, 0, 0, 0);
    let y = q(h, EE_UNITS_UH_TILE_POINT_Y, 0, 0, 0, 0);
    (qb(h, EE_UNITS_UH_LOAD_VALID, goal) != 0, x, y)
}

/// `ee_units_host_transport_filter`: `FilterLoadable`; (the units kept, the rejected).
pub fn transport_filter<H: UhHost + ?Sized>(h: &mut H, transport: u32, units: &[u32]) -> (Vec<u32>, Vec<u32>) {
    units_in(h, units);
    q(h, EE_UNITS_UH_REJECTED_CLEAR, 0, 0, 0, 0);
    qa(h, EE_UNITS_UH_FILTER, id(transport));
    let kept = q(h, EE_UNITS_UH_UNITS_SIZE, 0, 0, 0, 0);
    let kept: Vec<u32> = (0..kept).map(|k| q(h, EE_UNITS_UH_UNITS_ID, 0, 0, k, 0) as u32).collect();
    let n = q(h, EE_UNITS_UH_REJECTED_SIZE, 0, 0, 0, 0);
    let rejected: Vec<u32> = (0..n).map(|k| q(h, EE_UNITS_UH_REJECTED_ID, 0, 0, k, 0) as u32).collect();
    (kept, rejected)
}

/// `ee_units_host_research_node`: the node's bits (`EE_UNITS_RESEARCH_*`; 0: no node) and kind.
pub fn research_node<H: UhHost + ?Sized>(h: &mut H, player: u64, technology: i32) -> (u8, i32) {
    let node = q(h, EE_UNITS_UH_TREE_NODE, player, 0, i64::from(technology), 0) as u64;
    if node == 0 {
        return (0, 0);
    }
    let mut bits = EE_UNITS_RESEARCH_FOUND;
    let kind = qb(h, EE_UNITS_UH_NODE_KIND, node) as i32;
    if qb(h, EE_UNITS_UH_NODE_HERO, node) != 0 {
        bits |= EE_UNITS_RESEARCH_HERO;
    }
    if qb(h, EE_UNITS_UH_NODE_BUNDESWEHR, node) != 0 {
        bits |= EE_UNITS_RESEARCH_BUNDESWEHR;
    }
    if qb(h, EE_UNITS_UH_NODE_MAKES_UNIT, node) != 0 {
        bits |= EE_UNITS_RESEARCH_MAKES_UNIT;
    }
    if q(h, EE_UNITS_UH_IS_EPOCH, 0, 0, i64::from(technology), 0) != 0 {
        bits |= EE_UNITS_RESEARCH_EPOCH;
    }
    (bits, kind)
}

/// `ee_units_host_hack_create_unit` (the cheat's unit): at the point's ground facing pi, its reload cleared and its
/// power full; the creation sound for the current player.
pub fn hack_create_unit<H: UhHost + ?Sized>(h: &mut H, unit_type: i32, x: f32, y: f32, player_id: i64) {
    let t = i64::from(unit_type);
    q(h, EE_UNITS_UH_RESET_TRANSFORMATION, 0, 0, 0, 0);
    h.uh(EE_UNITS_UH_GROUND_POINT, 0, 0, 0, 0, &mut [x, y, 0.0, 0.0]);
    q(h, EE_UNITS_UH_FACE_PI, 0, 0, 0, 0);
    q(h, EE_UNITS_UH_POSITION_AT_GROUND, 0, 0, 0, 0);
    let unit = q(h, EE_UNITS_UH_CREATE_AT, 0, 0, t, player_id) as u64;
    if qb(h, EE_UNITS_UH_HAS_TILE_NODE, unit) != 0 {
        qb(h, EE_UNITS_UH_CLEAR_RELOAD, unit);
        qb(h, EE_UNITS_UH_FULL_POWER, unit);
    }
    let sound = q(h, EE_UNITS_UH_UPGRADE_CREATION_SOUND, 0, 0, t, player_id) as u64;
    if player_id == q(h, EE_UNITS_UH_CURRENT_PLAYER_ID, 0, 0, 0, 0) && sound != 0 {
        qb(h, EE_UNITS_UH_PLAY_SOUND, sound);
    }
}

/// `ee_units_host_hack_create_building` (the cheat's building): at its lower left corner, built; its creation sound
/// for the current player.
pub fn hack_create_building<H: UhHost + ?Sized>(h: &mut H, unit_type: i32, x: i64, y: i64, player_id: i64) {
    let unit = q(h, EE_UNITS_UH_CREATE_BUILDING, unit_type as i64 as u64, player_id as u64, x, y) as u64;
    if qb(h, EE_UNITS_UH_HAS_TILE_NODE, unit) != 0 {
        qb(h, EE_UNITS_UH_SET_BUILT, unit);
    }
    if player_id == q(h, EE_UNITS_UH_CURRENT_PLAYER_ID, 0, 0, 0, 0) {
        let sound = qb(h, EE_UNITS_UH_STATS_CREATION_SOUND, unit) as u64;
        if sound != 0 {
            qb(h, EE_UNITS_UH_PLAY_SOUND, sound);
        }
    }
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::ffi::c_void;

    extern "C" {
        fn ee_units_uh_host(op: i32, a: u64, b: u64, c: i64, d: i64, f: *mut f32) -> i64;
    }

    /// The game's: `ee_units_uh_host` and the Rust patrol groups and unit utilities.
    pub(crate) struct UhGame;

    impl UhHost for UhGame {
        fn uh(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::tech_cases::uh(op, a, b, c) {
                return v;
            }
            // SAFETY: one statement over the world's live objects (the decision runs on the simulation thread)
            unsafe { ee_units_uh_host(op, a, b, c, d, f.as_mut_ptr()) }
        }
        fn pg_nearest(&mut self, group: u64, x: i32, y: i32) -> u64 {
            // SAFETY: a town's live patrol group
            unsafe { crate::units::patrol_group::ee_units_pg_nearest(group as usize as *mut _, x, y) as usize as u64 }
        }
        fn pg_new(&mut self) -> u64 {
            crate::units::patrol_group::ee_units_pg_new() as usize as u64
        }
        fn pg_add_point(&mut self, group: u64, x: f32, y: f32, player: u32) {
            // SAFETY: a live patrol group
            unsafe { crate::units::patrol_group::ee_units_pg_add_point(group as usize as *mut _, x, y, player) }
        }
        fn pg_dereference(&mut self, group: u64) {
            // SAFETY: as above
            unsafe { crate::units::patrol_group::ee_units_pg_dereference(group as usize as *mut _) }
        }
        fn plan_wall(&mut self, start: (f32, f32), end: (f32, f32), class: u64, player: u64, pieces: u64) {
            // SAFETY: a live class and player, the host's new list
            unsafe {
                crate::units::uu_ffi::ee_units_uu_plan_wall(
                    start.0,
                    start.1,
                    end.0,
                    end.1,
                    class,
                    player,
                    1,
                    0,
                    pieces as usize as *mut c_void,
                )
            }
        }
        fn farm_drop_sites(&mut self, farm: u64, sites: u64) -> bool {
            // SAFETY: a live farm, the host's new list
            unsafe { crate::units::uu_ffi::ee_units_uu_farm_drop_sites(farm, sites as usize as *mut c_void) != 0 }
        }
    }
}

#[cfg(test)]
#[path = "uh_body_tests.rs"]
mod tests;
