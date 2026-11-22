//! `EEDoppelgangerManager`'s bodies (the units' lane, round 7; were EEDoppelgangerManager.cpp): the saved record's
//! `Load`, `Free`, the doppelganger finds, `AddToTiles` / `RemoveFromTiles`, `TileHidden`, `IsOutOfSight`,
//! `AddForUnseenUnit`, `TileShown`, `UnregisterPlayer`. Every C++ statement of the originals is one operation of
//! [`DmHost::dm`] (the game's: `ee_units_dm_host`, EEDoppelgangerManager.cpp, one statement a case), asked in the
//! original's order; the loops, the tile arithmetic and the choices are here. `a` is an object (a doppelganger, a
//! tile, a unit, a player), `b`, `c` values or a second object.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

// ---- the statements (`op`; the answer)
/// the saved game's file `= (FSFile*)a` (the loads' file)
pub const EE_UNITS_DM_SET_FILE: i32 = 1;
/// the file's `Read(an unsigned long)`
pub const EE_UNITS_DM_READ_ULONG: i32 = 2;
/// the file's `Read(a long)`
pub const EE_UNITS_DM_READ_LONG: i32 = 3;
/// the file's `Read(a bool)` (kept: `EE_UNITS_DM_FLAG`)
pub const EE_UNITS_DM_READ_BOOL: i32 = 4;
/// the bool read last
pub const EE_UNITS_DM_FLAG: i32 = 5;
/// `NewDoppelganger(b != 0)`
pub const EE_UNITS_DM_NEW: i32 = 6;
/// doppelganger `a`'s `Load(b, c, the file)`
pub const EE_UNITS_DM_LOAD: i32 = 7;
/// `gGameState.mRestorables.push_back(doppelganger a)`
pub const EE_UNITS_DM_RESTORABLE: i32 = 8;
/// doppelganger `a`'s `IsComplex()`
pub const EE_UNITS_DM_IS_COMPLEX: i32 = 9;
/// `dynamic_cast<EEComplexDoppelganger*>(a)->Clear()`
pub const EE_UNITS_DM_COMPLEX_CLEAR: i32 = 10;
/// `mComplexPool.Delete(dynamic_cast<EEComplexDoppelganger*>(a))`
pub const EE_UNITS_DM_COMPLEX_POOL_DELETE: i32 = 11;
/// doppelganger `a`'s `Clear()`
pub const EE_UNITS_DM_CLEAR: i32 = 12;
/// `mPool.Delete(a)`
pub const EE_UNITS_DM_POOL_DELETE: i32 = 13;
/// `gWorld.mPlayers[b]`
pub const EE_UNITS_DM_PLAYER_AT: i32 = 14;
/// tile `a`'s `GetNumDoppelgangers()`
pub const EE_UNITS_DM_NUM_DOPPELGANGERS: i32 = 15;
/// tile `a`'s `mDoppelgangers[b]`
pub const EE_UNITS_DM_DOPPELGANGER_AT: i32 = 16;
/// doppelganger `a`'s `mPlayer`
pub const EE_UNITS_DM_DOPPELGANGER_PLAYER: i32 = 17;
/// doppelganger `a`'s `mUnit`
pub const EE_UNITS_DM_DOPPELGANGER_UNIT: i32 = 18;
/// doppelganger `a`'s `mUnit->mUnitClass->mTileSize`
pub const EE_UNITS_DM_DOPPELGANGER_TILE_SIZE: i32 = 19;
/// `gWorld.mMap->GetTile(b, c)`
pub const EE_UNITS_DM_MAP_TILE: i32 = 20;
/// tile `a`'s `AddDoppelganger((EEDoppelganger*)b)`
pub const EE_UNITS_DM_TILE_ADD: i32 = 21;
/// tile `a`'s `RemoveDoppelganger((EEDoppelganger*)b)`
pub const EE_UNITS_DM_TILE_REMOVE: i32 = 22;
/// doppelganger `a`'s `mUnit->mComplexUnit->mFacing`
pub const EE_UNITS_DM_DOPPELGANGER_FACING: i32 = 23;
/// `gWorld.mPlayers[gWorld.mCurrentPlayer] == player a`
pub const EE_UNITS_DM_IS_CURRENT: i32 = 24;
/// player `a`'s `mPlayerID`
pub const EE_UNITS_DM_PLAYER_ID: i32 = 25;
/// tile `a`'s `mUnits`
pub const EE_UNITS_DM_TILE_FIRST_NODE: i32 = 26;
/// tile `a`'s `GetVisibility(b)`
pub const EE_UNITS_DM_TILE_VISIBILITY: i32 = 27;
/// node `a`'s `mUnit`
pub const EE_UNITS_DM_NODE_UNIT: i32 = 28;
/// node `a`'s `mNext`
pub const EE_UNITS_DM_NODE_NEXT: i32 = 29;
/// unit `a`'s `mUnitClass->mUnitType`
pub const EE_UNITS_DM_UNIT_TYPE: i32 = 30;
/// unit `a`'s class is a flare or a booby trap (`mIsFlare`, `mIsBoobyTrap`: b 0, 1)
pub const EE_UNITS_DM_CLASS_FLAG: i32 = 31;
/// unit `a`'s `mUnitClass->mCategory`
pub const EE_UNITS_DM_CATEGORY: i32 = 32;
/// unit `a`'s `mUnitClass->mFamily`
pub const EE_UNITS_DM_FAMILY: i32 = 33;
/// unit `a`'s `mUnitClass->IsLandBuilding()`
pub const EE_UNITS_DM_IS_LAND_BUILDING: i32 = 34;
/// unit `a`'s `mHitPoints`
pub const EE_UNITS_DM_HIT_POINTS: i32 = 35;
/// unit `a`'s `mComplexUnit->IsFoundation()`
pub const EE_UNITS_DM_IS_FOUNDATION: i32 = 36;
/// unit `a`'s `mPlayer`
pub const EE_UNITS_DM_UNIT_PLAYER: i32 = 37;
/// unit `a`'s `mTile`
pub const EE_UNITS_DM_UNIT_TILE: i32 = 38;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_DM_UNIT_COMPLEX: i32 = 39;
/// complex unit `a`'s `mPreviousPart`
pub const EE_UNITS_DM_PREVIOUS_PART: i32 = 40;
/// complex unit `a`'s `mNextPart`
pub const EE_UNITS_DM_NEXT_PART: i32 = 41;
/// unit `a`'s `mUnitClass->mScale != 0.0f`
pub const EE_UNITS_DM_HAS_SCALE: i32 = 42;
/// unit `a`'s `mUnitClass->mTileSize`
pub const EE_UNITS_DM_UNIT_TILE_SIZE: i32 = 43;
/// doppelganger `a`'s `Initialize((EEUnit*)b, (EEPlayer*)c, the unit's mTile)`
pub const EE_UNITS_DM_INITIALIZE: i32 = 44;
/// `gMiniMap->AddDoppelganger(doppelganger a, unit b's mTileX, mTileY, c != 0)`
pub const EE_UNITS_DM_MINIMAP_ADD: i32 = 45;
/// unit `a`'s `mTileX`, `mTileY` (b 0, 1)
pub const EE_UNITS_DM_UNIT_TILE_XY: i32 = 46;
/// complex unit `a`'s tech tree node is a wonder's (`GetTechTreeNode() != NULL && ...->mData->mKind == 4`)
pub const EE_UNITS_DM_IS_WONDER: i32 = 47;
/// complex unit `a`'s `mIsCloakable ? mIsCloaked : false`
pub const EE_UNITS_DM_IS_CLOAKED: i32 = 48;
/// player `a`'s `mSaveVisMap == true`
pub const EE_UNITS_DM_SAVES_VIS_MAP: i32 = 49;
/// `EECombatIsSeenBy(unit a, (EEPlayer*)b, true)`
pub const EE_UNITS_DM_IS_SEEN_BY: i32 = 50;
/// unit `a`'s `mComplexUnit->mIsCloakable`
pub const EE_UNITS_DM_IS_CLOAKABLE: i32 = 51;
/// unit `a`'s `GetPlayer()`
pub const EE_UNITS_DM_GET_PLAYER: i32 = 52;
/// doppelganger `a`'s `GetComplexDoppelganger()`
pub const EE_UNITS_DM_COMPLEX_OF: i32 = 53;
/// complex doppelganger `a`'s `mTileX`, `mTileY` (b 0, 1)
pub const EE_UNITS_DM_COMPLEX_TILE: i32 = 54;
/// `gMiniMap->RemoveDoppelganger(doppelganger a, b, c, false)`
pub const EE_UNITS_DM_MINIMAP_REMOVE: i32 = 55;
/// `gMiniMap->RemoveDoppelganger(doppelganger a, b, c, true)`
pub const EE_UNITS_DM_MINIMAP_REMOVE_WALL: i32 = 56;
/// `gWorld.mMap != NULL`
pub const EE_UNITS_DM_HAS_MAP: i32 = 57;
/// `gWorld.mMap->mSize`
pub const EE_UNITS_DM_MAP_SIZE: i32 = 58;
/// doppelganger `a`'s `GetTile()`
pub const EE_UNITS_DM_DOPPELGANGER_TILE: i32 = 59;
/// doppelganger `a`'s `GetPlayer()`
pub const EE_UNITS_DM_DOPPELGANGER_GET_PLAYER: i32 = 60;
/// doppelganger `a`'s `mUnit->mUnitClass->mUnitType`
pub const EE_UNITS_DM_DOPPELGANGER_UNIT_TYPE: i32 = 61;

/// `kUnitTypePathPoint`, `kUnitTypeWaypoint`, `kUnitTypeAirport`
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_WAYPOINT: i64 = 0x104;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
/// `kUnitCategoryResource`, `kUnitCategoryAmbient`
const CATEGORY_RESOURCE: i64 = 1;
const CATEGORY_AMBIENT: i64 = 0;
/// `kUnitFamilyFish`
const FAMILY_FISH: i64 = 0x12;
/// a tile out of a player's sight (`GetVisibility` at most this)
const OUT_OF_SIGHT: i64 = 1;

/// The C++ statements of the bodies.
pub trait DmHost {
    /// one statement (`EE_UNITS_DM_*`) of the manager
    fn dm(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64;
}

fn q<H: DmHost + ?Sized>(h: &mut H, op: i32, a: u64) -> i64 {
    h.dm(op, a, 0, 0)
}

fn qb<H: DmHost + ?Sized>(h: &mut H, op: i32, a: u64, b: i64) -> i64 {
    h.dm(op, a, b, 0)
}

/// `Load(major, minor, file)`: one saved doppelganger read into a new pooled one and queued for pointer repair; an
/// old record (minor < 0xbd) skipped. The doppelganger or 0.
pub fn load<H: DmHost + ?Sized>(h: &mut H, file: u64, major: i64, minor: i64) -> u64 {
    q(h, EE_UNITS_DM_SET_FILE, file);
    if major == 0 && minor < 0xbd {
        // old saves: the record is skipped
        let six = [
            EE_UNITS_DM_READ_ULONG,
            EE_UNITS_DM_READ_LONG,
            EE_UNITS_DM_READ_LONG,
            EE_UNITS_DM_READ_LONG,
            EE_UNITS_DM_READ_ULONG,
        ];
        if minor <= 0x18 {
            for op in six {
                q(h, op, 0);
            }
            q(h, EE_UNITS_DM_READ_BOOL, 0);
        } else {
            for op in six {
                q(h, op, 0);
            }
            if minor <= 0x64 {
                q(h, EE_UNITS_DM_READ_BOOL, 0);
                if minor < 0x27 {
                    return 0;
                }
            }
            q(h, EE_UNITS_DM_READ_ULONG, 0);
            q(h, EE_UNITS_DM_READ_ULONG, 0);
        }
        return 0;
    }
    if q(h, EE_UNITS_DM_READ_BOOL, 0) != 0 {
        return 0;
    }
    let complex = q(h, EE_UNITS_DM_FLAG, 0);
    let d = qb(h, EE_UNITS_DM_NEW, 0, complex) as u64;
    h.dm(EE_UNITS_DM_LOAD, d, major, minor);
    q(h, EE_UNITS_DM_RESTORABLE, d);
    d
}

/// `Free(doppelganger)`: cleared and returned to its pool.
pub fn free<H: DmHost + ?Sized>(h: &mut H, d: u64) {
    if q(h, EE_UNITS_DM_IS_COMPLEX, d) != 0 {
        q(h, EE_UNITS_DM_COMPLEX_CLEAR, d);
        q(h, EE_UNITS_DM_COMPLEX_POOL_DELETE, d);
    } else {
        q(h, EE_UNITS_DM_CLEAR, d);
        q(h, EE_UNITS_DM_POOL_DELETE, d);
    }
}

/// `HasDoppelganger(tile, unit, player)`: whether the tile holds the player's doppelganger of a unit.
pub fn has_doppelganger_of<H: DmHost + ?Sized>(h: &mut H, tile: u64, unit: u64, player: i64) -> bool {
    let p = qb(h, EE_UNITS_DM_PLAYER_AT, 0, player);
    let mut found = false;
    if tile != 0 {
        let mut i = 0;
        while i < q(h, EE_UNITS_DM_NUM_DOPPELGANGERS, tile) && !found {
            let d = qb(h, EE_UNITS_DM_DOPPELGANGER_AT, tile, i) as u64;
            found =
                q(h, EE_UNITS_DM_DOPPELGANGER_PLAYER, d) == p && unit == q(h, EE_UNITS_DM_DOPPELGANGER_UNIT, d) as u64;
            i += 1;
        }
    }
    found
}

/// `HasDoppelganger(tile, unit)`: whether the tile holds any player's doppelganger of a unit.
pub fn has_doppelganger<H: DmHost + ?Sized>(h: &mut H, tile: u64, unit: u64) -> bool {
    let mut i = 0;
    while i < q(h, EE_UNITS_DM_NUM_DOPPELGANGERS, tile) {
        let d = qb(h, EE_UNITS_DM_DOPPELGANGER_AT, tile, i) as u64;
        if unit == q(h, EE_UNITS_DM_DOPPELGANGER_UNIT, d) as u64 {
            return true;
        }
        i += 1;
    }
    false
}

/// `GetDoppelganger(tile, unit, player)`: the player's doppelganger of a unit on a tile, or 0.
pub fn get_doppelganger<H: DmHost + ?Sized>(h: &mut H, tile: u64, unit: u64, player: i64) -> u64 {
    let p = qb(h, EE_UNITS_DM_PLAYER_AT, 0, player);
    let mut i = 0;
    while i < q(h, EE_UNITS_DM_NUM_DOPPELGANGERS, tile) {
        let d = qb(h, EE_UNITS_DM_DOPPELGANGER_AT, tile, i) as u64;
        if q(h, EE_UNITS_DM_DOPPELGANGER_PLAYER, d) == p && unit == q(h, EE_UNITS_DM_DOPPELGANGER_UNIT, d) as u64 {
            return d;
        }
        i += 1;
    }
    0
}

/// `AddToTiles` (`add`) / `RemoveFromTiles`: a doppelganger on (off) every tile of its unit's square, for an
/// airport also the 3 runway squares ahead.
pub fn tiles<H: DmHost + ?Sized>(h: &mut H, d: u64, x: i64, y: i64, runways: bool, add: bool) {
    let size = q(h, EE_UNITS_DM_DOPPELGANGER_TILE_SIZE, d) as c_ulong as i64;
    let op = if add { EE_UNITS_DM_TILE_ADD } else { EE_UNITS_DM_TILE_REMOVE };
    for i in 0..size {
        for j in 0..size {
            let tile = h.dm(EE_UNITS_DM_MAP_TILE, 0, (x + j) as c_long as i64, (y + i) as c_long as i64) as u64;
            if tile != 0 {
                qb(h, op, tile, d as i64);
            }
        }
    }
    if !runways {
        return;
    }
    let facing = q(h, EE_UNITS_DM_DOPPELGANGER_FACING, d);
    for k in 1..=3 {
        let (dx, dy) = match facing {
            0 => (0, k * size),
            1 => (0, -k * size),
            2 => (k * size, 0),
            _ => (-k * size, 0),
        };
        tiles(h, d, (x + dx) as c_long as i64, (y + dy) as c_long as i64, false, add);
    }
}

/// `TileHidden(tile, player)`: as a tile drops out of a player's sight, he is left doppelgangers of the resources,
/// ambients, fish and built land buildings there (foundations only his own) wholly out of sight; all parts of a
/// complex one.
pub fn tile_hidden<H: DmHost + ?Sized>(h: &mut H, tile: u64, player: u64) {
    let current = q(h, EE_UNITS_DM_IS_CURRENT, player) != 0;
    let id = q(h, EE_UNITS_DM_PLAYER_ID, player);
    if tile == 0 {
        return;
    }
    let mut node = q(h, EE_UNITS_DM_TILE_FIRST_NODE, tile) as u64;
    if node == 0 || qb(h, EE_UNITS_DM_TILE_VISIBILITY, tile, id) > OUT_OF_SIGHT {
        return;
    }
    loop {
        let mut unit = q(h, EE_UNITS_DM_NODE_UNIT, node) as u64;
        if unit != 0 && remembered(h, unit, player, id) {
            let complex = q(h, EE_UNITS_DM_UNIT_COMPLEX, unit) as u64;
            if complex != 0 {
                let mut carrier = complex;
                loop {
                    let previous = q(h, EE_UNITS_DM_PREVIOUS_PART, carrier) as u64;
                    if previous == 0 {
                        break;
                    }
                    carrier = previous;
                }
                unit = carrier;
            }
            let wall = q(h, EE_UNITS_DM_UNIT_TYPE, unit) == UNIT_TYPE_AIRPORT;
            while unit != 0 {
                let t = q(h, EE_UNITS_DM_UNIT_TILE, unit);
                if t != 0 && q(h, EE_UNITS_DM_HAS_SCALE, unit) != 0 {
                    add_doppelganger(h, unit, player, current, wall);
                }
                let complex = q(h, EE_UNITS_DM_UNIT_COMPLEX, unit) as u64;
                unit = if complex != 0 && !wall { q(h, EE_UNITS_DM_NEXT_PART, complex) as u64 } else { 0 };
            }
        }
        node = q(h, EE_UNITS_DM_NODE_NEXT, node) as u64;
        if node == 0 {
            break;
        }
    }
}

/// `TileHidden`'s test of a unit: a kind the player remembers, no doppelganger of his yet, and wholly out of sight.
fn remembered<H: DmHost + ?Sized>(h: &mut H, unit: u64, player: u64, id: i64) -> bool {
    let kind = q(h, EE_UNITS_DM_UNIT_TYPE, unit);
    let category = q(h, EE_UNITS_DM_CATEGORY, unit);
    kind != UNIT_TYPE_PATH_POINT
        && kind != UNIT_TYPE_WAYPOINT
        && qb(h, EE_UNITS_DM_CLASS_FLAG, unit, 0) == 0
        && qb(h, EE_UNITS_DM_CLASS_FLAG, unit, 1) == 0
        && (category == CATEGORY_RESOURCE
            || category == CATEGORY_AMBIENT
            || q(h, EE_UNITS_DM_FAMILY, unit) == FAMILY_FISH
            || (q(h, EE_UNITS_DM_IS_LAND_BUILDING, unit) != 0
                && q(h, EE_UNITS_DM_HIT_POINTS, unit) > 0
                && (q(h, EE_UNITS_DM_IS_FOUNDATION, unit) == 0
                    || q(h, EE_UNITS_DM_UNIT_PLAYER, unit) as u64 == player)))
        && {
            let t = q(h, EE_UNITS_DM_UNIT_TILE, unit) as u64;
            !has_doppelganger_of(h, t, unit, id)
        }
        && is_out_of_sight(h, unit, player)
}

/// A new doppelganger of a unit for a player (complex for a complex or large unit), on the minimap for the current
/// player and on its tiles.
fn add_doppelganger<H: DmHost + ?Sized>(h: &mut H, unit: u64, player: u64, current: bool, wall: bool) {
    let complex = q(h, EE_UNITS_DM_UNIT_COMPLEX, unit) != 0 || q(h, EE_UNITS_DM_UNIT_TILE_SIZE, unit) > 1;
    let d = qb(h, EE_UNITS_DM_NEW, 0, i64::from(complex)) as u64;
    h.dm(EE_UNITS_DM_INITIALIZE, d, unit as i64, player as i64);
    if current {
        h.dm(EE_UNITS_DM_MINIMAP_ADD, d, unit as i64, i64::from(wall));
    }
    let x = qb(h, EE_UNITS_DM_UNIT_TILE_XY, unit, 0);
    let y = qb(h, EE_UNITS_DM_UNIT_TILE_XY, unit, 1);
    tiles(h, d, x, y, wall, true);
}

/// `IsOutOfSight(unit, player)`: whether every tile under the unit (all parts of a complex one) is out of the
/// player's sight; false for a cloaked unit or a wonder.
pub fn is_out_of_sight<H: DmHost + ?Sized>(h: &mut H, unit: u64, player: u64) -> bool {
    let mut unit = unit;
    let mut result = true;
    let complex = q(h, EE_UNITS_DM_UNIT_COMPLEX, unit) as u64;
    if complex != 0 {
        let mut carrier = complex;
        loop {
            let previous = q(h, EE_UNITS_DM_PREVIOUS_PART, carrier) as u64;
            if previous == 0 {
                break;
            }
            carrier = previous;
        }
        unit = carrier;
        result = q(h, EE_UNITS_DM_IS_WONDER, carrier) == 0;
        result &= q(h, EE_UNITS_DM_IS_CLOAKED, carrier) == 0;
    }
    while unit != 0 && result {
        let mut size = q(h, EE_UNITS_DM_UNIT_TILE_SIZE, unit) as c_long;
        if size <= 1 {
            size = 1;
        }
        let x = qb(h, EE_UNITS_DM_UNIT_TILE_XY, unit, 0) as c_long as c_ulong;
        let y = qb(h, EE_UNITS_DM_UNIT_TILE_XY, unit, 1) as c_long as c_ulong;
        let (x_end, y_end) = (x.wrapping_add(size as c_ulong), y.wrapping_add(size as c_ulong));
        let id = q(h, EE_UNITS_DM_PLAYER_ID, player);
        let mut i = x;
        while i < x_end && result {
            let mut j = y;
            while j < y_end && result {
                let tile = h.dm(EE_UNITS_DM_MAP_TILE, 0, i as c_long as i64, j as c_long as i64) as u64;
                result = tile != 0 && qb(h, EE_UNITS_DM_TILE_VISIBILITY, tile, id) <= OUT_OF_SIGHT;
                j = j.wrapping_add(1);
            }
            i = i.wrapping_add(1);
        }
        let complex = q(h, EE_UNITS_DM_UNIT_COMPLEX, unit) as u64;
        unit = if complex != 0 { q(h, EE_UNITS_DM_NEXT_PART, complex) as u64 } else { 0 };
    }
    result
}

/// `AddForUnseenUnit(unit)`: a resource, ambient, fish or building its own player cannot see (when he keeps a
/// visibility map) remembered: his doppelgangers on its tile cleared, a new one there and on the minimap.
pub fn add_for_unseen_unit<H: DmHost + ?Sized>(h: &mut H, unit: u64) {
    let tile = q(h, EE_UNITS_DM_UNIT_TILE, unit) as u64;
    let player = q(h, EE_UNITS_DM_UNIT_PLAYER, unit) as u64;
    if tile == 0
        || q(h, EE_UNITS_DM_SAVES_VIS_MAP, player) == 0
        || qb(h, EE_UNITS_DM_IS_SEEN_BY, unit, player as i64) != 0
    {
        return;
    }
    let family = q(h, EE_UNITS_DM_FAMILY, unit);
    let category = q(h, EE_UNITS_DM_CATEGORY, unit);
    let skip = ((q(h, EE_UNITS_DM_IS_LAND_BUILDING, unit) == 0
        || q(h, EE_UNITS_DM_HIT_POINTS, unit) <= 0
        || q(h, EE_UNITS_DM_IS_CLOAKABLE, unit) != 0
        || (q(h, EE_UNITS_DM_IS_FOUNDATION, unit) != 0 && q(h, EE_UNITS_DM_UNIT_PLAYER, unit) as u64 != player))
        && category != CATEGORY_RESOURCE
        && category != CATEGORY_AMBIENT
        && family != FAMILY_FISH)
        || q(h, EE_UNITS_DM_HAS_SCALE, unit) == 0;
    if skip {
        return;
    }
    let x = qb(h, EE_UNITS_DM_UNIT_TILE_XY, unit, 0);
    let y = qb(h, EE_UNITS_DM_UNIT_TILE_XY, unit, 1);
    let owner = q(h, EE_UNITS_DM_GET_PLAYER, unit) as u64;
    tile_shown(h, tile, x, y, owner);
    let wall = q(h, EE_UNITS_DM_UNIT_TYPE, unit) == UNIT_TYPE_AIRPORT;
    let current = q(h, EE_UNITS_DM_IS_CURRENT, player) != 0;
    add_doppelganger(h, unit, player, current, wall);
}

/// The minimap removal of a doppelganger at (x, y).
fn minimap_remove<H: DmHost + ?Sized>(h: &mut H, d: u64, x: i64, y: i64, wall: bool) {
    h.dm(if wall { EE_UNITS_DM_MINIMAP_REMOVE_WALL } else { EE_UNITS_DM_MINIMAP_REMOVE }, d, x, y);
}

/// `TileShown(tile, x, y, player)`: as a tile comes into a player's sight, his doppelgangers on it are taken off the
/// map and the minimap and freed.
pub fn tile_shown<H: DmHost + ?Sized>(h: &mut H, tile: u64, x: i64, y: i64, player: u64) {
    let current = q(h, EE_UNITS_DM_IS_CURRENT, player) != 0;
    let mut i = 0;
    while i < q(h, EE_UNITS_DM_NUM_DOPPELGANGERS, tile) {
        let d = qb(h, EE_UNITS_DM_DOPPELGANGER_AT, tile, i) as u64;
        let complex = q(h, EE_UNITS_DM_COMPLEX_OF, d) as u64;
        if q(h, EE_UNITS_DM_DOPPELGANGER_PLAYER, d) as u64 == player {
            let wall = q(h, EE_UNITS_DM_DOPPELGANGER_UNIT_TYPE, d) == UNIT_TYPE_AIRPORT;
            let (tx, ty) = if complex != 0 {
                (qb(h, EE_UNITS_DM_COMPLEX_TILE, complex, 0), qb(h, EE_UNITS_DM_COMPLEX_TILE, complex, 1))
            } else {
                (x, y)
            };
            if current {
                minimap_remove(h, d, tx, ty, wall);
            }
            tiles(h, d, tx, ty, wall, false);
            free(h, d);
        } else {
            i += 1;
        }
    }
}

/// `UnregisterPlayer(player)`: every doppelganger the player sees or owns the unit of removed and freed, over the
/// whole map.
pub fn unregister_player<H: DmHost + ?Sized>(h: &mut H, player: u64) {
    let current = q(h, EE_UNITS_DM_IS_CURRENT, player) != 0;
    if q(h, EE_UNITS_DM_HAS_MAP, 0) == 0 {
        return;
    }
    let size = q(h, EE_UNITS_DM_MAP_SIZE, 0);
    for x in 0..size {
        for y in 0..size {
            let tile = h.dm(EE_UNITS_DM_MAP_TILE, 0, x, y) as u64;
            if tile == 0 {
                continue;
            }
            let mut i = 0;
            while i < q(h, EE_UNITS_DM_NUM_DOPPELGANGERS, tile) {
                let d = qb(h, EE_UNITS_DM_DOPPELGANGER_AT, tile, i) as u64;
                let at = q(h, EE_UNITS_DM_DOPPELGANGER_TILE, d) as u64;
                if (at == 0 || at == tile)
                    && (q(h, EE_UNITS_DM_DOPPELGANGER_PLAYER, d) as u64 == player
                        || q(h, EE_UNITS_DM_DOPPELGANGER_GET_PLAYER, d) as u64 == player)
                {
                    let wall = q(h, EE_UNITS_DM_DOPPELGANGER_UNIT_TYPE, d) == UNIT_TYPE_AIRPORT;
                    if current && q(h, EE_UNITS_DM_DOPPELGANGER_PLAYER, d) as u64 == player {
                        minimap_remove(h, d, x, y, wall);
                    }
                    tiles(h, d, x, y, wall, false);
                    free(h, d);
                } else {
                    i += 1;
                }
            }
        }
    }
}

// ---- the bodies (`ee_units_dm_body`'s `what`)
pub const EE_UNITS_DM_DO_LOAD: i32 = 1;
pub const EE_UNITS_DM_DO_FREE: i32 = 2;
pub const EE_UNITS_DM_DO_HAS_OF_PLAYER: i32 = 3;
pub const EE_UNITS_DM_DO_HAS: i32 = 4;
pub const EE_UNITS_DM_DO_GET: i32 = 5;
pub const EE_UNITS_DM_DO_ADD_TO_TILES: i32 = 6;
pub const EE_UNITS_DM_DO_REMOVE_FROM_TILES: i32 = 7;
pub const EE_UNITS_DM_DO_TILE_HIDDEN: i32 = 8;
pub const EE_UNITS_DM_DO_IS_OUT_OF_SIGHT: i32 = 9;
pub const EE_UNITS_DM_DO_ADD_FOR_UNSEEN: i32 = 10;
pub const EE_UNITS_DM_DO_TILE_SHOWN: i32 = 11;
pub const EE_UNITS_DM_DO_UNREGISTER_PLAYER: i32 = 12;

/// Runs body `what` (its arguments: `a` an object, `b`, `c`, `d` values or objects).
pub fn run<H: DmHost + ?Sized>(h: &mut H, what: i32, a: u64, b: i64, c: i64, d: i64) -> i64 {
    match what {
        EE_UNITS_DM_DO_LOAD => return load(h, a, b, c) as i64,
        EE_UNITS_DM_DO_FREE => free(h, a),
        EE_UNITS_DM_DO_HAS_OF_PLAYER => return i64::from(has_doppelganger_of(h, a, b as u64, c)),
        EE_UNITS_DM_DO_HAS => return i64::from(has_doppelganger(h, a, b as u64)),
        EE_UNITS_DM_DO_GET => return get_doppelganger(h, a, b as u64, c) as i64,
        EE_UNITS_DM_DO_ADD_TO_TILES => tiles(h, a, b, c, d != 0, true),
        EE_UNITS_DM_DO_REMOVE_FROM_TILES => tiles(h, a, b, c, d != 0, false),
        EE_UNITS_DM_DO_TILE_HIDDEN => tile_hidden(h, a, b as u64),
        EE_UNITS_DM_DO_IS_OUT_OF_SIGHT => return i64::from(is_out_of_sight(h, a, b as u64)),
        EE_UNITS_DM_DO_ADD_FOR_UNSEEN => add_for_unseen_unit(h, a),
        EE_UNITS_DM_DO_TILE_SHOWN => tile_shown(h, a, b, c, d as u64),
        EE_UNITS_DM_DO_UNREGISTER_PLAYER => unregister_player(h, a),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_dm_host(op: i32, a: u64, b: i64, c: i64) -> i64;
    }

    struct Game;

    impl DmHost for Game {
        fn dm(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
            // SAFETY: the manager's objects, live for the body (the C++ member's caller holds them)
            unsafe { ee_units_dm_host(op, a, b, c) }
        }
    }

    /// `EEDoppelgangerManager`'s body `what` (`EE_UNITS_DM_DO_*`).
    ///
    /// # Safety
    /// The objects named are live, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_dm_body(what: i32, a: *mut c_void, b: i64, c: i64, d: i64) -> i64 {
        run(&mut Game, what, a as usize as u64, b, c, d)
    }
}

#[cfg(test)]
#[path = "dm_body_tests.rs"]
mod tests;
