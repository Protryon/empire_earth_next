//! The patrol group's bodies (`EEPPatrolGroup`, Physics Thread/EEPPatrolGroup.cpp 0x16066d..0x16100f; the physics
//! models' lane, round 2): the patrol points a group of units walks. The C++ class stays the type its holders use
//! (a reference-counted object whose members, the point list, the id and the two flags, they read); every member
//! function's body is here, over [`PatrolGroupHost`]: the group's fields, the points' units, the map's tiles, the
//! path point's creation and death, the player's selection and the saved game's typed calls.
//!
//! The id counter (`0x531d8c`) is Rust's: [`next_id`], [`reset_ids`], the saved game's raise.
//!
//! Points are handles (the units' addresses). Between `Load` and `RepairPointers` a point is the saved id in the
//! pointer's place, as the original kept it (`USavedID` reads it back as its low 32 bits).

use std::sync::atomic::{AtomicU32, Ordering};

/// The patrol group's fields that the bodies read and write (`mID`, `mIsLoop`, `mIsTownPatrol`).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct GroupFields {
    pub id: u32,
    pub is_loop: bool,
    pub is_town_patrol: bool,
}

/// The C++ the bodies call (the FFI adapter; the tests' mock).
pub trait PatrolGroupHost {
    /// the group's `mPatrolPoints` and its replacement
    fn points(&mut self, group: u64) -> Vec<u64>;
    fn set_points(&mut self, group: u64, points: &[u64]);
    /// the group's `mID`, `mIsLoop`, `mIsTownPatrol` and their store
    fn fields(&mut self, group: u64) -> GroupFields;
    fn set_fields(&mut self, group: u64, fields: GroupFields);
    /// a point unit's `GetTileX()`, `GetTileY()` (its `GetTile()`)
    fn unit_tile(&mut self, unit: u64) -> (i64, i64);
    /// a unit's `DistanceSquaredToTileXY(x, y)`
    fn distance_squared_to_tile(&mut self, unit: u64, x: i64, y: i64) -> f32;
    /// the map's tile at `((long)x, (long)y)`: its `GetHeight()` and whether it `HasUnit(probe)` (probe 0: not asked)
    fn tile_at(&mut self, x: f32, y: f32, probe: u64) -> (f32, bool);
    /// a new path point unit (`gUnitClassTable[kUnitTypePathPoint]->CreateUnit((long)x, (long)y, player, 0, NULL)`)
    /// with `SetPatrolGroup(group)`
    fn create_point(&mut self, group: u64, x: f32, y: f32, player: u32) -> u64;
    /// a unit's `SetPatrolGroup(group)` (0: NULL) and its `GetPatrolGroup()`
    fn set_patrol_group(&mut self, unit: u64, group: u64);
    fn patrol_group_of(&mut self, unit: u64) -> u64;
    /// the unit's `Kill(1, 0, 0)` (when `kill`), then its player's `KillUnit(unit, false)`
    fn kill_unit(&mut self, unit: u64, kill: bool);
    /// the unit's player's `RemovePatrolGroup(group)` (may free the group: nothing reads it after)
    fn remove_from_player(&mut self, unit: u64, group: u64);
    /// the player's `TrySelectUnit(unit, flag)`
    fn try_select(&mut self, player: u64, unit: u64, flag: bool) -> bool;
    /// a point's `mUnitID`; the world's complex unit of an id (`EEUnitCast<EEComplexUnit>(GetUnit(id))`, 0: none)
    fn unit_id(&mut self, unit: u64) -> u32;
    fn find_unit(&mut self, id: u32) -> u64;
    /// `EESaveHasLiveState(major, minor)`
    fn live_state(&mut self, major: u32, minor: u32) -> bool;
    /// the file's typed calls (`unsigned long`, `bool`): the result code; a read gives back what the file left in
    /// the value it was handed
    fn write_u32(&mut self, value: u32) -> i32;
    fn write_bool(&mut self, value: bool) -> i32;
    fn read_u32(&mut self, value: &mut u32) -> i32;
    fn read_bool(&mut self, value: &mut bool) -> i32;
}

/// `kErrorOK`
const OK: i32 = 0;

static NEXT_ID: AtomicU32 = AtomicU32::new(0);

/// The constructor's id: the counter's next value (`mID = sPatrolGroupID++`).
pub fn next_id() -> u32 {
    NEXT_ID.fetch_add(1, Ordering::Relaxed)
}

/// `ResetIDs` (0x160cdc): ids restart at 0.
pub fn reset_ids() {
    NEXT_ID.store(0, Ordering::Relaxed);
}

/// The counter (the saved game's copy; the tests).
pub fn current_id() -> u32 {
    NEXT_ID.load(Ordering::Relaxed)
}

/// `~EEPPatrolGroup` (0x1606cc): every point is detached and killed, the list emptied.
pub fn destroy(group: u64, host: &mut dyn PatrolGroupHost) {
    let points = host.points(group);
    for unit in points {
        host.set_patrol_group(unit, 0);
        host.kill_unit(unit, false);
    }
    host.set_points(group, &[]);
}

/// `AddPoint` (0x160727): a point at the end at the tile's height; a point on the first point's tile closes the loop
/// instead.
pub fn add_point(group: u64, x: f32, y: f32, player: u32, host: &mut dyn PatrolGroupHost) {
    let points = host.points(group);
    let probe = points.first().copied().unwrap_or(0);
    let (height, has) = host.tile_at(x, y, probe);
    if !points.is_empty() && has {
        let mut f = host.fields(group);
        f.is_loop = true;
        host.set_fields(group, f);
    } else {
        append_point(group, [x, y, height], player, host);
    }
}

/// `AppendPatrolPoint` (0x1607c8): after the last point.
pub fn append_point(group: u64, point: [f32; 3], player: u32, host: &mut dyn PatrolGroupHost) {
    let last = host.points(group).last().copied().unwrap_or(0);
    insert_point(group, point, player, last, host);
}

/// `GetNearest` (0x1607f4): the point nearest to the tile, the first of equals; 0 when there is none.
pub fn nearest(group: u64, x: i64, y: i64, host: &mut dyn PatrolGroupHost) -> u64 {
    let points = host.points(group);
    nearest_of(&points, x, y, host)
}

fn nearest_of(points: &[u64], x: i64, y: i64, host: &mut dyn PatrolGroupHost) -> u64 {
    let Some(&first) = points.first() else {
        return 0;
    };
    let mut result = first;
    for &unit in &points[1..] {
        let distance = host.distance_squared_to_tile(unit, x, y);
        if host.distance_squared_to_tile(result, x, y) > distance {
            result = unit;
        }
    }
    result
}

/// `IndexOf` (0x1608db): the point's index, else the index of the point nearest to the unit's tile (the list's size
/// when there is none).
pub fn index_of(group: u64, unit: u64, host: &mut dyn PatrolGroupHost) -> usize {
    let points = host.points(group);
    index_in(&points, unit, host)
}

fn index_in(points: &[u64], unit: u64, host: &mut dyn PatrolGroupHost) -> usize {
    if let Some(i) = points.iter().position(|&p| p == unit) {
        return i;
    }
    let (x, y) = host.unit_tile(unit);
    let near = nearest_of(points, x, y, host);
    if near != 0 {
        if let Some(i) = points.iter().position(|&p| p == near) {
            return i;
        }
    }
    points.len()
}

/// `GetNext` (0x16085d): the point after the unit's (the first for none); the first again in a loop or a one-point
/// patrol, else 0 past the end.
pub fn next(group: u64, unit: u64, host: &mut dyn PatrolGroupHost) -> u64 {
    let points = host.points(group);
    if points.is_empty() {
        return 0;
    }
    if unit == 0 {
        return points[0];
    }
    let index = index_in(&points, unit, host);
    if index + 1 < points.len() {
        points[index + 1]
    } else if host.fields(group).is_loop || points.len() == 1 {
        points[0]
    } else {
        0
    }
}

/// `GetPrevious` (0x16098a): the point before the unit's (the first for none); the last in a loop, else 0 at the
/// start.
pub fn previous(group: u64, unit: u64, host: &mut dyn PatrolGroupHost) -> u64 {
    let points = host.points(group);
    if points.is_empty() {
        return 0;
    }
    if unit == 0 {
        return points[0];
    }
    let index = index_in(&points, unit, host);
    if index > 0 {
        points[index - 1]
    } else if host.fields(group).is_loop {
        points[points.len() - 1]
    } else {
        0
    }
}

/// `GetLast` (0x16363e): the last point, 0 when there is none.
pub fn last(group: u64, host: &mut dyn PatrolGroupHost) -> u64 {
    host.points(group).last().copied().unwrap_or(0)
}

/// `InsertPatrolPointAt` (0x1609e4): a point at the map position, at its tile's height, after the unit's.
pub fn insert_point_at(group: u64, x: f32, y: f32, player: u32, after: u64, host: &mut dyn PatrolGroupHost) {
    let (height, _) = host.tile_at(x, y, 0);
    insert_point(group, [x, y, height], player, after, host);
}

/// `InsertPatrolPoint` (0x160a58): a new path point unit at the spot, after the given point (at the end for none).
pub fn insert_point(group: u64, point: [f32; 3], player: u32, after: u64, host: &mut dyn PatrolGroupHost) {
    let unit = host.create_point(group, point[0], point[1], player);
    let mut points = host.points(group);
    let at = if after != 0 { (index_in(&points, after, host) + 1).min(points.len()) } else { points.len() };
    points.insert(at, unit);
    host.set_points(group, &points);
}

/// `RemoveUnit` (0x160b72): drops the point; the player releases the group once it has none.
pub fn remove_unit(group: u64, unit: u64, host: &mut dyn PatrolGroupHost) {
    let mut points = host.points(group);
    if let Some(i) = points.iter().position(|&p| p == unit) {
        points.remove(i);
    }
    host.set_points(group, &points);
    if points.is_empty() {
        host.remove_from_player(unit, group);
    }
}

/// `Select` (0x160d70): the points selected for the player, up to the first that cannot be.
pub fn select(group: u64, player: u64, flag: bool, host: &mut dyn PatrolGroupHost) {
    let points = host.points(group);
    for unit in points {
        if !host.try_select(player, unit, flag) {
            break;
        }
    }
}

/// `SetPatrolPoints` (0x160da9): the route replaced by the tiles: points on no listed tile are killed, missing ones
/// added, the points ordered as listed, as a closed loop.
pub fn set_points(group: u64, tiles: &[(i64, i64)], player: u32, host: &mut dyn PatrolGroupHost) {
    let mut points = host.points(group);
    let mut i = 0;
    while i < points.len() {
        let unit = points[i];
        let tile = host.unit_tile(unit);
        if !tiles.contains(&tile) {
            host.set_patrol_group(unit, 0);
            host.kill_unit(unit, true);
            points.remove(i);
            host.set_points(group, &points);
        } else {
            i += 1;
        }
    }
    for &(x, y) in tiles {
        let points = host.points(group);
        if find_on_tile(&points, (x, y), host).is_none() {
            add_point(group, x as f32, y as f32, player, host);
        }
    }
    let points = host.points(group);
    let mut kept = Vec::with_capacity(tiles.len());
    for &tile in tiles {
        // the original dereferences find_if's result unchecked; every listed tile has its point by now
        if let Some(unit) = find_on_tile(&points, tile, host) {
            kept.push(unit);
        }
    }
    host.set_points(group, &kept);
    let mut f = host.fields(group);
    f.is_loop = true;
    host.set_fields(group, f);
}

fn find_on_tile(points: &[u64], tile: (i64, i64), host: &mut dyn PatrolGroupHost) -> Option<u64> {
    points.iter().copied().find(|&unit| host.unit_tile(unit) == tile)
}

/// `Load` (0x160ade): the id, the point ids (kept in the points' places for `RepairPointers`) and the loop flag;
/// from save 0.254 the town flag and the id counter (raised to it). The first read error.
pub fn load(group: u64, major: u32, minor: u32, host: &mut dyn PatrolGroupHost) -> i32 {
    let mut f = host.fields(group);
    let mut count = 0u32;
    let mut result = host.read_u32(&mut f.id);
    if result == OK {
        result = host.read_u32(&mut count);
        if result == OK {
            let mut points = Vec::new();
            host.set_points(group, &points);
            let mut i = 0;
            while i < count && result == OK {
                let mut id = u32::MAX;
                result = host.read_u32(&mut id);
                if result == OK {
                    points.push(u64::from(id));
                }
                i += 1;
            }
            host.set_points(group, &points);
            if result == OK {
                result = host.read_bool(&mut f.is_loop);
            }
            if result == OK && host.live_state(major, minor) {
                result = host.read_bool(&mut f.is_town_patrol);
                let mut next = 0u32;
                if result == OK {
                    result = host.read_u32(&mut next);
                }
                if result == OK && next > current_id() {
                    NEXT_ID.store(next, Ordering::Relaxed);
                }
            }
        }
    }
    host.set_fields(group, f);
    result
}

/// `RepairPointers` (0x160bf2): the loaded ids back to point units, claiming those of no group or another; 3 at the
/// first id with no unit.
pub fn repair_pointers(group: u64, host: &mut dyn PatrolGroupHost) -> i32 {
    let ids: Vec<u32> = host.points(group).iter().map(|&p| p as u32).collect();
    let mut points = Vec::new();
    host.set_points(group, &points);
    let mut result = 0;
    for id in ids {
        if result != 0 {
            break;
        }
        let unit = host.find_unit(id);
        if unit != 0 {
            if host.patrol_group_of(unit) != group {
                host.set_patrol_group(unit, group);
            }
            points.push(unit);
            host.set_points(group, &points);
        } else {
            result = 3;
        }
    }
    result
}

/// `Save` (0x160ce4): the id, the point unit ids, the loop flag; from save 0.254 the town flag and the id counter.
/// The first write error.
pub fn save(group: u64, major: u32, minor: u32, host: &mut dyn PatrolGroupHost) -> i32 {
    let f = host.fields(group);
    let mut result = host.write_u32(f.id);
    if result != OK {
        return result;
    }
    let points = host.points(group);
    result = host.write_u32(points.len() as u32);
    if result != OK {
        return result;
    }
    for &unit in &points {
        if result != OK {
            break;
        }
        let id = host.unit_id(unit);
        result = host.write_u32(id);
    }
    if result == OK {
        result = host.write_bool(f.is_loop);
    }
    if result == OK && host.live_state(major, minor) {
        result = host.write_bool(f.is_town_patrol);
        if result == OK {
            result = host.write_u32(current_id());
        }
    }
    result
}

// ---- the C++ side (Physics Thread/EEPPatrolGroup.cpp: ee_units_pg_host_*)

/// The selectors of `ee_units_pg_host_unit` (one unit-side call each).
pub const EE_UNITS_PG_SET_GROUP: i32 = 0;
pub const EE_UNITS_PG_GROUP_OF: i32 = 1;
pub const EE_UNITS_PG_KILL: i32 = 2;
pub const EE_UNITS_PG_KILL_UNIT: i32 = 3;
pub const EE_UNITS_PG_REMOVE_GROUP: i32 = 4;
pub const EE_UNITS_PG_UNIT_ID: i32 = 5;
pub const EE_UNITS_PG_FIND_UNIT: i32 = 6;
pub const EE_UNITS_PG_SELECT: i32 = 7;
pub const EE_UNITS_PG_SELECT_FLAG: i32 = 8;
/// The selectors of `ee_units_pg_host_file`
pub const EE_UNITS_PG_WRITE_U32: i32 = 0;
pub const EE_UNITS_PG_WRITE_BOOL: i32 = 1;
pub const EE_UNITS_PG_READ_U32: i32 = 2;
pub const EE_UNITS_PG_READ_BOOL: i32 = 3;

/// The fields' record (`mID`, `mIsLoop`, `mIsTownPatrol`) across the C ABI.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeUnitsPgFields {
    pub id: u32,
    pub is_loop: u8,
    pub is_town_patrol: u8,
}

#[cfg(not(test))]
extern "C" {
    fn ee_units_pg_host_points(group: u64, out: *mut u64, capacity: u32) -> u32;
    fn ee_units_pg_host_set_points(group: u64, points: *const u64, count: u32);
    fn ee_units_pg_host_fields(group: u64, set: u8, io: *mut EeUnitsPgFields);
    fn ee_units_pg_host_unit_tile(unit: u64, out_x: *mut i32, out_y: *mut i32);
    fn ee_units_pg_host_distance(unit: u64, x: i32, y: i32) -> f32;
    fn ee_units_pg_host_tile_at(x: f32, y: f32, probe: u64, out_height: *mut f32) -> u8;
    fn ee_units_pg_host_create_point(group: u64, x: f32, y: f32, player: u32) -> u64;
    fn ee_units_pg_host_unit(which: i32, unit: u64, other: u64) -> u64;
    fn ee_units_pg_host_live_state(major: u32, minor: u32) -> u8;
    fn ee_units_pg_host_file(file: u64, which: i32, io: *mut u32) -> i32;
}

/// The FFI adapter: the C++ calls, the file the call was handed (an `FSFile*`, 0 when none).
#[cfg(not(test))]
struct Ffi {
    file: u64,
}

#[cfg(not(test))]
impl PatrolGroupHost for Ffi {
    fn points(&mut self, group: u64) -> Vec<u64> {
        let n = unsafe { ee_units_pg_host_points(group, std::ptr::null_mut(), 0) };
        let mut v = vec![0u64; n as usize];
        if n != 0 {
            unsafe { ee_units_pg_host_points(group, v.as_mut_ptr(), n) };
        }
        v
    }
    fn set_points(&mut self, group: u64, points: &[u64]) {
        unsafe { ee_units_pg_host_set_points(group, points.as_ptr(), points.len() as u32) }
    }
    fn fields(&mut self, group: u64) -> GroupFields {
        let mut f = EeUnitsPgFields::default();
        unsafe { ee_units_pg_host_fields(group, 0, &mut f) };
        GroupFields { id: f.id, is_loop: f.is_loop != 0, is_town_patrol: f.is_town_patrol != 0 }
    }
    fn set_fields(&mut self, group: u64, fields: GroupFields) {
        let mut f = EeUnitsPgFields {
            id: fields.id,
            is_loop: fields.is_loop as u8,
            is_town_patrol: fields.is_town_patrol as u8,
        };
        unsafe { ee_units_pg_host_fields(group, 1, &mut f) }
    }
    fn unit_tile(&mut self, unit: u64) -> (i64, i64) {
        let (mut x, mut y) = (0i32, 0i32);
        unsafe { ee_units_pg_host_unit_tile(unit, &mut x, &mut y) };
        (i64::from(x), i64::from(y))
    }
    fn distance_squared_to_tile(&mut self, unit: u64, x: i64, y: i64) -> f32 {
        unsafe { ee_units_pg_host_distance(unit, x as i32, y as i32) }
    }
    fn tile_at(&mut self, x: f32, y: f32, probe: u64) -> (f32, bool) {
        let mut height = 0.0f32;
        let has = unsafe { ee_units_pg_host_tile_at(x, y, probe, &mut height) };
        (height, has != 0)
    }
    fn create_point(&mut self, group: u64, x: f32, y: f32, player: u32) -> u64 {
        unsafe { ee_units_pg_host_create_point(group, x, y, player) }
    }
    fn set_patrol_group(&mut self, unit: u64, group: u64) {
        unsafe { ee_units_pg_host_unit(EE_UNITS_PG_SET_GROUP, unit, group) };
    }
    fn patrol_group_of(&mut self, unit: u64) -> u64 {
        unsafe { ee_units_pg_host_unit(EE_UNITS_PG_GROUP_OF, unit, 0) }
    }
    fn kill_unit(&mut self, unit: u64, kill: bool) {
        let which = if kill { EE_UNITS_PG_KILL } else { EE_UNITS_PG_KILL_UNIT };
        unsafe { ee_units_pg_host_unit(which, unit, 0) };
    }
    fn remove_from_player(&mut self, unit: u64, group: u64) {
        unsafe { ee_units_pg_host_unit(EE_UNITS_PG_REMOVE_GROUP, unit, group) };
    }
    fn try_select(&mut self, player: u64, unit: u64, flag: bool) -> bool {
        let which = if flag { EE_UNITS_PG_SELECT_FLAG } else { EE_UNITS_PG_SELECT };
        unsafe { ee_units_pg_host_unit(which, unit, player) != 0 }
    }
    fn unit_id(&mut self, unit: u64) -> u32 {
        unsafe { ee_units_pg_host_unit(EE_UNITS_PG_UNIT_ID, unit, 0) as u32 }
    }
    fn find_unit(&mut self, id: u32) -> u64 {
        unsafe { ee_units_pg_host_unit(EE_UNITS_PG_FIND_UNIT, 0, u64::from(id)) }
    }
    fn live_state(&mut self, major: u32, minor: u32) -> bool {
        unsafe { ee_units_pg_host_live_state(major, minor) != 0 }
    }
    fn write_u32(&mut self, value: u32) -> i32 {
        let mut v = value;
        unsafe { ee_units_pg_host_file(self.file, EE_UNITS_PG_WRITE_U32, &mut v) }
    }
    fn write_bool(&mut self, value: bool) -> i32 {
        let mut v = value as u32;
        unsafe { ee_units_pg_host_file(self.file, EE_UNITS_PG_WRITE_BOOL, &mut v) }
    }
    fn read_u32(&mut self, value: &mut u32) -> i32 {
        unsafe { ee_units_pg_host_file(self.file, EE_UNITS_PG_READ_U32, value) }
    }
    fn read_bool(&mut self, value: &mut bool) -> i32 {
        let mut v = *value as u32;
        let r = unsafe { ee_units_pg_host_file(self.file, EE_UNITS_PG_READ_BOOL, &mut v) };
        *value = v != 0;
        r
    }
}

/// The members' operations of [`ee_units_patrol_group`].
pub const EE_UNITS_PG_OP_NEW_ID: i32 = 0;
pub const EE_UNITS_PG_OP_RESET_IDS: i32 = 1;
pub const EE_UNITS_PG_OP_DESTROY: i32 = 2;
pub const EE_UNITS_PG_OP_ADD_POINT: i32 = 3;
pub const EE_UNITS_PG_OP_APPEND: i32 = 4;
pub const EE_UNITS_PG_OP_NEAREST: i32 = 5;
pub const EE_UNITS_PG_OP_INDEX_OF: i32 = 6;
pub const EE_UNITS_PG_OP_NEXT: i32 = 7;
pub const EE_UNITS_PG_OP_PREVIOUS: i32 = 8;
pub const EE_UNITS_PG_OP_LAST: i32 = 9;
pub const EE_UNITS_PG_OP_INSERT_AT: i32 = 10;
pub const EE_UNITS_PG_OP_INSERT: i32 = 11;
pub const EE_UNITS_PG_OP_REMOVE: i32 = 12;
pub const EE_UNITS_PG_OP_SELECT: i32 = 13;
pub const EE_UNITS_PG_OP_LOAD: i32 = 14;
pub const EE_UNITS_PG_OP_REPAIR: i32 = 15;
pub const EE_UNITS_PG_OP_SAVE: i32 = 16;

/// One call's arguments: a unit (or player, or file), a point, two integers (a player id, tile x/y, a flag, a save's
/// version).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeUnitsPgArgs {
    pub unit: u64,
    pub file: u64,
    pub point: [f32; 3],
    pub a: i32,
    pub b: i32,
}

/// Every `EEPPatrolGroup` member's body by its operation (`EE_UNITS_PG_OP_*`) on the group object; the result is a
/// unit handle, an index, an id or an error code as the member returns it.
#[cfg(not(test))]
#[no_mangle]
pub extern "C" fn ee_units_patrol_group(op: i32, group: u64, args: EeUnitsPgArgs) -> u64 {
    let mut host = Ffi { file: args.file };
    run(op, group, args, &mut host)
}

/// The dispatch of [`ee_units_patrol_group`] over a host (the tests drive it with their mock).
pub fn run(op: i32, group: u64, args: EeUnitsPgArgs, host: &mut dyn PatrolGroupHost) -> u64 {
    let [x, y, z] = args.point;
    match op {
        EE_UNITS_PG_OP_NEW_ID => u64::from(next_id()),
        EE_UNITS_PG_OP_RESET_IDS => {
            reset_ids();
            0
        }
        EE_UNITS_PG_OP_DESTROY => {
            destroy(group, host);
            0
        }
        EE_UNITS_PG_OP_ADD_POINT => {
            add_point(group, x, y, args.a as u32, host);
            0
        }
        EE_UNITS_PG_OP_APPEND => {
            append_point(group, args.point, args.a as u32, host);
            0
        }
        EE_UNITS_PG_OP_NEAREST => nearest(group, i64::from(args.a), i64::from(args.b), host),
        EE_UNITS_PG_OP_INDEX_OF => index_of(group, args.unit, host) as u64,
        EE_UNITS_PG_OP_NEXT => next(group, args.unit, host),
        EE_UNITS_PG_OP_PREVIOUS => previous(group, args.unit, host),
        EE_UNITS_PG_OP_LAST => last(group, host),
        EE_UNITS_PG_OP_INSERT_AT => {
            insert_point_at(group, x, y, args.a as u32, args.unit, host);
            0
        }
        EE_UNITS_PG_OP_INSERT => {
            insert_point(group, [x, y, z], args.a as u32, args.unit, host);
            0
        }
        EE_UNITS_PG_OP_REMOVE => {
            remove_unit(group, args.unit, host);
            0
        }
        EE_UNITS_PG_OP_SELECT => {
            select(group, args.unit, args.a != 0, host);
            0
        }
        EE_UNITS_PG_OP_LOAD => load(group, args.a as u32, args.b as u32, host) as u32 as u64,
        EE_UNITS_PG_OP_REPAIR => repair_pointers(group, host) as u32 as u64,
        EE_UNITS_PG_OP_SAVE => save(group, args.a as u32, args.b as u32, host) as u32 as u64,
        _ => 0,
    }
}

/// `SetPatrolPoints`'s tiles (`inPoints`, U2DPoint<long> pairs) for the group of player `player`.
///
/// # Safety
/// `tiles` holds `count` (x, y) pairs of 32-bit integers (or is null with `count` 0), valid for the call.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_patrol_group_set_points(group: u64, tiles: *const i32, count: u32, player: u32) {
    let pairs: Vec<(i64, i64)> = if tiles.is_null() || count == 0 {
        Vec::new()
    } else {
        // SAFETY: the function's contract
        let raw = unsafe { std::slice::from_raw_parts(tiles, count as usize * 2) };
        raw.chunks(2).map(|c| (i64::from(c[0]), i64::from(c[1]))).collect()
    };
    set_points(group, &pairs, player, &mut Ffi { file: 0 });
}

#[cfg(test)]
#[path = "patrol_group_tests.rs"]
mod tests;
