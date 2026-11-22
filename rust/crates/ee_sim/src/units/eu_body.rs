//! `EEUnit`'s bodies over its C++ statements (the units' lane, round 7; were EEUnit.cpp): `MoveTo`, `Kill`,
//! `CanGather`, `FindAreaTile`, `GetNearestFootprintTile`, `SetHitPoints`, `DepleteResource`. Every C++ statement of
//! the originals is one operation of [`EuHost::eu`] (the game's: `ee_units_eu_host`, EEUnit.cpp, after eu_hit.rs's
//! statements), asked in the original's order. `unit` is the `EEUnit*`, `a` an object (0: the unit itself where
//! noted), `b`, `c` values.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::units::eu_hit::*;

// ---- the statements (`op`; the answer), after eu_hit.rs's
/// `gWorld.mMap ? gWorld.mMap->mUnitRegions : 0`
pub const EE_UNITS_EU_REGIONS: i32 = 100;
/// `mTile`
pub const EE_UNITS_EU_TILE: i32 = 101;
/// `mPlayer->AddOffMapUnit(this)`
pub const EE_UNITS_EU_ADD_OFF_MAP: i32 = 102;
/// `mPlayer->RemoveOffMapUnit(this)`
pub const EE_UNITS_EU_REMOVE_OFF_MAP: i32 = 103;
/// `mComplexUnit->mF24C = mComplexUnit->mCollisionResolutionCount`
pub const EE_UNITS_EU_KEEP_COLLISION_COUNT: i32 = 104;
/// `mComplexUnit->mCollisionResolutionCount = 0.0f`
pub const EE_UNITS_EU_CLEAR_COLLISION_COUNT: i32 = 105;
/// `ee_units_uu_cloaked_enemy(this, gWorld.GetCurrentPlayer())`
pub const EE_UNITS_EU_CLOAKED_ENEMY: i32 = 106;
/// `IsFoundation()`
pub const EE_UNITS_EU_IS_FOUNDATION: i32 = 107;
/// `gMiniMap->MoveUnit(this, mTileX, mTileY, b, c)`
pub const EE_UNITS_EU_MINIMAP_MOVE: i32 = 108;
/// `SightMoved(mTileX, mTileY, b, c)`
pub const EE_UNITS_EU_SIGHT_MOVED: i32 = 109;
/// `mUnitClass->mTileSize`
pub const EE_UNITS_EU_CLASS_TILE_SIZE: i32 = 110;
/// `ee_combat_regions_size(regions a)`
pub const EE_UNITS_EU_REGIONS_SIZE: i32 = 111;
/// `gWorld.mMap != NULL`
pub const EE_UNITS_EU_HAS_MAP: i32 = 112;
/// `gWorld.mMap->GetTile(b, c) != NULL`
pub const EE_UNITS_EU_MAP_TILE_EXISTS: i32 = 113;
/// `mUnitClass->RemoveFromRegion(this)`
pub const EE_UNITS_EU_REMOVE_FROM_REGION: i32 = 114;
/// `mUnitClass->mInfluence != 0`
pub const EE_UNITS_EU_HAS_INFLUENCE: i32 = 115;
/// `gWorld.mInfluenceTable->RemoveUnit(this, mTileX, mTileY)`
pub const EE_UNITS_EU_INFLUENCE_REMOVE: i32 = 116;
/// `mUnitClass->RemoveFromTile(this)`
pub const EE_UNITS_EU_REMOVE_FROM_TILE: i32 = 117;
/// `mHiddenFlags |= kHiddenFlagMoved`
pub const EE_UNITS_EU_MARK_MOVED: i32 = 118;
/// `mTileY = b`
pub const EE_UNITS_EU_SET_TILE_Y: i32 = 119;
/// `mTileX = b`
pub const EE_UNITS_EU_SET_TILE_X: i32 = 120;
/// the class hides its units (an obstacle, not air, not a projectile, standing: `mMaxVelocity == 0.0f`)
pub const EE_UNITS_EU_CLASS_HIDES: i32 = 121;
/// `UpdateHidden()`
pub const EE_UNITS_EU_UPDATE_HIDDEN: i32 = 122;
/// `mTile = gWorld.mMap->GetTile(b, c)`
pub const EE_UNITS_EU_SET_TILE: i32 = 123;
/// `mUnitClass->AddToTile(this)`
pub const EE_UNITS_EU_ADD_TO_TILE: i32 = 124;
/// `mUnitClass->AddToRegion(this)`
pub const EE_UNITS_EU_ADD_TO_REGION: i32 = 125;
/// `gWorld.mInfluenceTable->AddUnit(this, b, c)`
pub const EE_UNITS_EU_INFLUENCE_ADD: i32 = 126;
/// `mUnitClass->mIsInactive`
pub const EE_UNITS_EU_CLASS_INACTIVE: i32 = 127;
/// `gWorld.ActivateUnit(mComplexUnit)`
pub const EE_UNITS_EU_ACTIVATE_COMPLEX: i32 = 128;
/// `gWorld.ResortUnit(this)`
pub const EE_UNITS_EU_RESORT: i32 = 129;
/// `SightOff(mTileX, mTileY)`
pub const EE_UNITS_EU_SIGHT_OFF_HERE: i32 = 130;
/// `gDoppelgangerManager.GetDoppelganger((EETile*)a, this, mPlayer->mPlayerID) != NULL`
pub const EE_UNITS_EU_HAS_DOPPELGANGER: i32 = 131;
/// `gDoppelgangerManager.TileShown((EETile*)a, mTileX, mTileY, mPlayer)`
pub const EE_UNITS_EU_DOPPELGANGER_TILE_SHOWN: i32 = 132;
/// `mUnitClass->mDeathEffect != NULL`
pub const EE_UNITS_EU_HAS_DEATH_EFFECT: i32 = 133;
/// `gEffectManager->CreateUnitEffect(this, mUnitClass->mDeathEffect, NULL, 0, 0, 0)`
pub const EE_UNITS_EU_DEATH_EFFECT: i32 = 134;
/// `mStats->mDeathSound != NULL`
pub const EE_UNITS_EU_HAS_DEATH_SOUND: i32 = 135;
/// `mUnitClass->GetSoundParameters(this, mStats->mDeathSound, the volume, pan, pitch)` (kept for the next)
pub const EE_UNITS_EU_DEATH_SOUND_PARAMETERS: i32 = 136;
/// `gSoundManager->Play(the death sound's mSampleID, the volume, pan, pitch, its mLooping, NULL)`
pub const EE_UNITS_EU_DEATH_SOUND_PLAY: i32 = 137;
/// `mResourceAmount = b`
pub const EE_UNITS_EU_SET_RESOURCE_AMOUNT: i32 = 138;
/// `SightOn(mTileX, mTileY)`
pub const EE_UNITS_EU_SIGHT_ON_HERE: i32 = 139;
/// `gWorld.mCameraControl.mUnitA == this` (b 0) / `mUnitB` (b 1)
pub const EE_UNITS_EU_CAMERA_UNIT_IS_THIS: i32 = 140;
/// `gWorld.mCameraControl.ClearUnitA()`
pub const EE_UNITS_EU_CLEAR_CAMERA_UNIT: i32 = 141;
/// `gWorld.mCameraControl.ClearUnitB()`
pub const EE_UNITS_EU_CLEAR_CAMERA_UNIT_B: i32 = 167;
/// `mPlayer->UnitTaken(this, b != 0)`
pub const EE_UNITS_EU_UNIT_TAKEN: i32 = 142;
/// `gWorld.mMap->GetTile(b, c)->IsVisibleTo(GetPlayer())`
pub const EE_UNITS_EU_TILE_VISIBLE: i32 = 143;
/// `GetStats()->mCarryAmount > 0`
pub const EE_UNITS_EU_CARRIES: i32 = 144;
/// unit `a`'s `mResourceAmount`
pub const EE_UNITS_EU_RESOURCE_AMOUNT_OF: i32 = 145;
/// unit `a`'s `mResourceType`
pub const EE_UNITS_EU_RESOURCE_TYPE_OF: i32 = 146;
/// unit `a`'s `GetUnitClass()->GetCategory()` (0: this unit's)
pub const EE_UNITS_EU_CATEGORY_OF: i32 = 147;
/// unit `a`'s `GetUnitClass()->GetUnitType()`
pub const EE_UNITS_EU_UNIT_TYPE_OF: i32 = 148;
/// unit `a`'s `GetPlayer() == GetPlayer()`
pub const EE_UNITS_EU_SAME_PLAYER: i32 = 149;
/// unit `a`'s `GetComplexUnit()->mIsBuilt`
pub const EE_UNITS_EU_IS_BUILT_OF: i32 = 150;
/// unit `a`'s `GetHitPoints() == GetStats()->mMaxHitPoints`
pub const EE_UNITS_EU_FULL_HEALTH_OF: i32 = 151;
/// unit `a`'s `mTile->mTerrainType`
pub const EE_UNITS_EU_TERRAIN_OF: i32 = 152;
/// `GetPlayer()->GetEnemy(unit a's GetPlayer()->mPlayerID) != 0`
pub const EE_UNITS_EU_ENEMY_OF: i32 = 153;
/// unit `a`'s `GetHitPoints()`
pub const EE_UNITS_EU_HIT_POINTS_OF: i32 = 154;
/// `gContinentManager.GetContinentAt(this)`
pub const EE_UNITS_EU_CONTINENT_AT: i32 = 155;
/// continent `a`'s `GetCategory()`
pub const EE_UNITS_EU_CONTINENT_CATEGORY: i32 = 156;
/// `gMapWidth` (b 0) / `gMapHeight` (b 1)
pub const EE_UNITS_EU_MAP_SIZE: i32 = 157;
/// `gContinentManager.GetContinentID(b, c)`
pub const EE_UNITS_EU_CONTINENT_ID: i32 = 158;
/// `gContinentManager.GetContinentByID(b)`
pub const EE_UNITS_EU_CONTINENT_BY_ID: i32 = 159;
/// `GetTileDistanceSquared(unit a)` (its bits)
pub const EE_UNITS_EU_TILE_DISTANCE_SQUARED: i32 = 160;
/// unit `a`'s `mUnitClass->mTileSize`
pub const EE_UNITS_EU_TILE_SIZE_OF: i32 = 161;
/// `mTileNode != NULL`
pub const EE_UNITS_EU_HAS_TILE_NODE: i32 = 162;
/// `mPlayer->KillUnit(this, false)`
pub const EE_UNITS_EU_KILL_UNIT: i32 = 163;
/// `mResourceAmount -= b`
pub const EE_UNITS_EU_TAKE_RESOURCE: i32 = 164;
/// `mResourceAmount`
pub const EE_UNITS_EU_RESOURCE_AMOUNT: i32 = 165;
/// `mComplexUnit->mPreviousPart == NULL`
pub const EE_UNITS_EU_NO_PREVIOUS_PART: i32 = 166;

/// the off-map tile
const OFF_MAP: i64 = -10000;
/// `kUnitTypeFarm`, `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`, `kUnitCategoryNaval`,
/// `kUnitCategoryResource`
const UNIT_TYPE_FARM: i64 = 0x2a;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
const CATEGORY_NAVAL: i64 = 4;
const CATEGORY_RESOURCE: i64 = 1;
/// `kResourceAny`, the resources' end
const RESOURCE_ANY: i64 = 1;
const RESOURCES_END: i64 = 7;
/// `kTerrainDeepWater`
const TERRAIN_DEEP_WATER: i64 = 1;

struct S<'h, H: EuHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
}

impl<H: EuHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.eu(op, self.unit, 0, 0, 0)
    }
    fn qa(&mut self, op: i32, a: u64) -> i64 {
        self.h.eu(op, self.unit, a, 0, 0)
    }
    fn qb(&mut self, op: i32, b: i64) -> i64 {
        self.h.eu(op, self.unit, 0, b, 0)
    }
    fn qbc(&mut self, op: i32, b: i64, c: i64) -> i64 {
        self.h.eu(op, self.unit, 0, b, c)
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
}

/// `MoveTo(x, y, update_tiles)`: the unit to a tile; with `update_tiles` also the off-map list, the minimap, sight,
/// the tile, region and influence tables, activation and the world's sort order.
pub fn move_to<H: EuHost + ?Sized>(h: &mut H, unit: u64, x: i64, y: i64, update: bool) {
    let s = &mut S { h, unit };
    let old_x = s.qa(EE_UNITS_EU_TILE_X, 0);
    let old_y = s.qa(EE_UNITS_EU_TILE_Y, 0);
    let regions = s.q(EE_UNITS_EU_REGIONS) as u64;
    let old_tile = s.q(EE_UNITS_EU_TILE);
    let mut same_region = false;
    if old_x == x && old_y == y {
        return;
    }
    let complex = s.q(EE_UNITS_EU_COMPLEX);
    if update {
        if x == OFF_MAP {
            s.q(EE_UNITS_EU_ADD_OFF_MAP);
        } else if old_x == OFF_MAP {
            s.q(EE_UNITS_EU_REMOVE_OFF_MAP);
        }
        if complex != 0 {
            s.q(EE_UNITS_EU_KEEP_COLLISION_COUNT);
            s.q(EE_UNITS_EU_CLEAR_COLLISION_COUNT);
        }
        if s.q(EE_UNITS_EU_HIT_POINTS) != 0
            && !s.yes(EE_UNITS_EU_CLOAKED_ENEMY)
            && (s.yes(EE_UNITS_EU_IS_CURRENT_PLAYER) || !s.yes(EE_UNITS_EU_IS_FOUNDATION))
        {
            s.qbc(EE_UNITS_EU_MINIMAP_MOVE, x, y);
        }
        s.qbc(EE_UNITS_EU_SIGHT_MOVED, x, y);
        if s.q(EE_UNITS_EU_TILE) != 0 {
            if s.q(EE_UNITS_EU_CLASS_TILE_SIZE) <= 1 {
                let size = s.qa(EE_UNITS_EU_REGIONS_SIZE, regions) as c_long;
                let (ox, oy, nx, ny) = (old_x as c_long, old_y as c_long, x as c_long, y as c_long);
                same_region = ox.wrapping_div(size) == nx.wrapping_div(size)
                    && oy.wrapping_div(size) == ny.wrapping_div(size)
                    && s.yes(EE_UNITS_EU_HAS_MAP)
                    && s.qbc(EE_UNITS_EU_MAP_TILE_EXISTS, x, y) != 0;
            }
            if !same_region {
                s.q(EE_UNITS_EU_REMOVE_FROM_REGION);
                if s.yes(EE_UNITS_EU_HAS_INFLUENCE) {
                    s.q(EE_UNITS_EU_INFLUENCE_REMOVE);
                }
            }
            s.q(EE_UNITS_EU_REMOVE_FROM_TILE);
        }
    }
    s.q(EE_UNITS_EU_MARK_MOVED);
    s.qb(EE_UNITS_EU_SET_TILE_Y, y);
    s.qb(EE_UNITS_EU_SET_TILE_X, x);
    if x != OFF_MAP && s.yes(EE_UNITS_EU_CLASS_HIDES) {
        s.q(EE_UNITS_EU_UPDATE_HIDDEN);
    }
    if !s.yes(EE_UNITS_EU_HAS_MAP) {
        return;
    }
    s.qbc(EE_UNITS_EU_SET_TILE, x, y);
    if !update {
        return;
    }
    if s.q(EE_UNITS_EU_TILE) != 0 {
        s.q(EE_UNITS_EU_ADD_TO_TILE);
        if !same_region {
            s.q(EE_UNITS_EU_ADD_TO_REGION);
            if s.yes(EE_UNITS_EU_HAS_INFLUENCE) {
                s.qbc(EE_UNITS_EU_INFLUENCE_ADD, x, y);
            }
        }
        if old_tile == 0
            && complex != 0
            && s.yes(EE_UNITS_EU_NO_PREVIOUS_PART)
            && (!s.yes(EE_UNITS_EU_CLASS_INACTIVE) || s.q(EE_UNITS_EU_HIT_POINTS) <= 0)
        {
            s.q(EE_UNITS_EU_ACTIVATE_COMPLEX);
        }
    }
    if (old_tile != 0 || old_x == OFF_MAP) && old_x != x {
        s.q(EE_UNITS_EU_RESORT);
    }
}

/// `Kill(killed, keep_research, other)`: sight and doppelgangers updated, a simple unit's death effect and sound,
/// farms and citizens emptied, off the minimap and the camera, a simple unit reported lost.
pub fn kill<H: EuHost + ?Sized>(h: &mut H, unit: u64, killed: bool, keep_research: bool, other: bool) {
    let s = &mut S { h, unit };
    let tile = s.q(EE_UNITS_EU_TILE) as u64;
    if s.q(EE_UNITS_EU_HIT_POINTS) == 0 {
        return;
    }
    if s.qa(EE_UNITS_EU_TILE_X, 0) == OFF_MAP {
        s.q(EE_UNITS_EU_REMOVE_OFF_MAP);
    }
    s.q(EE_UNITS_EU_SIGHT_OFF_HERE);
    if s.yes(EE_UNITS_EU_IS_FOUNDATION) && s.qa(EE_UNITS_EU_HAS_DOPPELGANGER, tile) != 0 {
        s.qa(EE_UNITS_EU_DOPPELGANGER_TILE_SHOWN, tile);
    }
    s.qb(EE_UNITS_EU_SET_HIT_POINTS, 0);
    let complex = s.q(EE_UNITS_EU_COMPLEX);
    if complex == 0 && killed {
        if s.yes(EE_UNITS_EU_HAS_DEATH_EFFECT) {
            s.q(EE_UNITS_EU_DEATH_EFFECT);
        }
        if s.yes(EE_UNITS_EU_HAS_DEATH_SOUND) {
            s.q(EE_UNITS_EU_DEATH_SOUND_PARAMETERS);
            s.q(EE_UNITS_EU_DEATH_SOUND_PLAY);
        }
    }
    let category = s.q(EE_UNITS_EU_CATEGORY);
    if s.q(EE_UNITS_EU_UNIT_TYPE) == UNIT_TYPE_FARM
        || category == CATEGORY_LAND_CITIZEN
        || category == CATEGORY_NAVAL_CITIZEN
    {
        s.qb(EE_UNITS_EU_SET_RESOURCE_AMOUNT, 0);
    }
    s.q(EE_UNITS_EU_SIGHT_ON_HERE);
    if !s.yes(EE_UNITS_EU_CLOAKED_ENEMY) {
        s.qbc(EE_UNITS_EU_MINIMAP_MOVE, OFF_MAP, OFF_MAP);
    }
    for camera in 0..2 {
        if s.qb(EE_UNITS_EU_CAMERA_UNIT_IS_THIS, camera) != 0 {
            s.q(if camera == 0 { EE_UNITS_EU_CLEAR_CAMERA_UNIT } else { EE_UNITS_EU_CLEAR_CAMERA_UNIT_B });
        }
    }
    if complex == 0 {
        s.qb(EE_UNITS_EU_UNIT_TAKEN, i64::from(keep_research | other));
    }
}

/// `CanGather(resource, x, y, visible, ignore_amount)`: whether the unit may gather from another (a visible tile,
/// resource left unless ignored): a land citizen takes its own fully repaired farm or a dead or enemy non-citizen
/// off deep water; a naval citizen enemy naval units.
pub fn can_gather<H: EuHost + ?Sized>(
    h: &mut H,
    unit: u64,
    other: u64,
    x: i64,
    y: i64,
    visible: bool,
    ignore: bool,
) -> bool {
    let s = &mut S { h, unit };
    let mut visible = visible;
    if !visible && s.yes(EE_UNITS_EU_HAS_MAP) && s.qbc(EE_UNITS_EU_MAP_TILE_EXISTS, x, y) != 0 {
        visible = s.qbc(EE_UNITS_EU_TILE_VISIBLE, x, y) != 0;
    }
    if !s.yes(EE_UNITS_EU_CARRIES) || !visible {
        return false;
    }
    if !ignore && s.qa(EE_UNITS_EU_RESOURCE_AMOUNT_OF, other) <= 0 {
        return false;
    }
    let kind = s.qa(EE_UNITS_EU_RESOURCE_TYPE_OF, other);
    if kind <= RESOURCE_ANY || kind >= RESOURCES_END {
        return false;
    }
    let category = s.qa(EE_UNITS_EU_CATEGORY_OF, 0);
    if category == CATEGORY_LAND_CITIZEN {
        if s.qa(EE_UNITS_EU_UNIT_TYPE_OF, other) == UNIT_TYPE_FARM {
            return s.qa(EE_UNITS_EU_SAME_PLAYER, other) != 0
                && s.qa(EE_UNITS_EU_IS_BUILT_OF, other) != 0
                && s.qa(EE_UNITS_EU_FULL_HEALTH_OF, other) != 0;
        }
        if s.qa(EE_UNITS_EU_TERRAIN_OF, other) == TERRAIN_DEEP_WATER {
            return false;
        }
        let other_category = s.qa(EE_UNITS_EU_CATEGORY_OF, other);
        if other_category == CATEGORY_LAND_CITIZEN || other_category == CATEGORY_NAVAL_CITIZEN {
            return false;
        }
        return (s.qa(EE_UNITS_EU_SAME_PLAYER, other) == 0 && s.qa(EE_UNITS_EU_ENEMY_OF, other) != 0)
            || s.qa(EE_UNITS_EU_HIT_POINTS_OF, other) == 0;
    }
    category == CATEGORY_NAVAL_CITIZEN
        && s.qa(EE_UNITS_EU_CATEGORY_OF, other) == CATEGORY_NAVAL
        && s.qa(EE_UNITS_EU_SAME_PLAYER, other) == 0
        && s.qa(EE_UNITS_EU_ENEMY_OF, other) != 0
}

/// `FindAreaTile(land, range)`: the unit's tile, or for a multi-tile unit off a continent of the wanted kind the
/// first tile within `range` of its footprint on one. (x, y).
pub fn find_area_tile<H: EuHost + ?Sized>(h: &mut H, unit: u64, land: bool, range: i64) -> (i64, i64) {
    let s = &mut S { h, unit };
    let continent = s.q(EE_UNITS_EU_CONTINENT_AT) as u64;
    let (tx, ty) = (s.qa(EE_UNITS_EU_TILE_X, 0), s.qa(EE_UNITS_EU_TILE_Y, 0));
    let mut tile = (tx, ty);
    let size = s.q(EE_UNITS_EU_CLASS_TILE_SIZE);
    let flag = i64::from(land);
    if size > 1 && (continent == 0 || s.qa(EE_UNITS_EU_CONTINENT_CATEGORY, continent) != flag) {
        let end_y = ty + size + range;
        let end_x = tx + size + range;
        let mut x = tx - range;
        while x < end_x {
            let mut y = ty - range;
            while y < end_y {
                if (x as c_ulong as u64) >= s.qb(EE_UNITS_EU_MAP_SIZE, 0) as c_ulong as u64
                    || (y as c_ulong as u64) >= s.qb(EE_UNITS_EU_MAP_SIZE, 1) as c_ulong as u64
                {
                    y += 1;
                    continue;
                }
                let id = s.qbc(EE_UNITS_EU_CONTINENT_ID, x, y);
                let other = if id != 0 { s.qb(EE_UNITS_EU_CONTINENT_BY_ID, id) as u64 } else { 0 };
                if other == 0 || s.qa(EE_UNITS_EU_CONTINENT_CATEGORY, other) != flag {
                    y += 1;
                    continue;
                }
                tile = (x, y);
                x = end_x;
                break;
            }
            x += 1;
        }
    }
    tile
}

/// `GetNearestFootprintTile(other)`: the squared tile distance to the nearest edge tile of the other unit's
/// footprint, and that tile.
pub fn nearest_footprint_tile<H: EuHost + ?Sized>(h: &mut H, unit: u64, other: u64) -> (f32, i64, i64) {
    let s = &mut S { h, unit };
    let mut best = f32::from_bits(s.qa(EE_UNITS_EU_TILE_DISTANCE_SQUARED, other) as u32);
    let size = s.qa(EE_UNITS_EU_TILE_SIZE_OF, other) as c_long;
    let (tx, ty) = (s.qa(EE_UNITS_EU_TILE_X, other) as c_long, s.qa(EE_UNITS_EU_TILE_Y, other) as c_long);
    let mut out = (i64::from(tx), i64::from(ty));
    if size <= 1 {
        return (best, out.0, out.1);
    }
    let (mx, my) = (s.qa(EE_UNITS_EU_TILE_X, 0) as c_long, s.qa(EE_UNITS_EU_TILE_Y, 0) as c_long);
    let mut x = tx;
    while x < tx + size {
        let mut y = ty;
        while y < ty + size {
            let dx = mx.wrapping_sub(x);
            let dy = my.wrapping_sub(y);
            let distance = dx.wrapping_mul(dx).wrapping_add(dy.wrapping_mul(dy)) as f32;
            if distance < best {
                best = distance;
                out = (i64::from(x), i64::from(y));
            }
            if x > tx && x < tx + size - 1 {
                y += size - 2;
            }
            y += 1;
        }
        x += 1;
    }
    (best, out.0, out.1)
}

/// `SetHitPoints(hit_points)`: the hit points; a dead unit of an inactive class activated; the icon bar refreshed
/// if the unit is selected.
pub fn set_hit_points<H: EuHost + ?Sized>(h: &mut H, unit: u64, hit_points: i64) {
    let s = &mut S { h, unit };
    if s.q(EE_UNITS_EU_HIT_POINTS) != hit_points {
        s.qb(EE_UNITS_EU_SET_HIT_POINTS, hit_points);
    }
    if s.yes(EE_UNITS_EU_CLASS_INACTIVE) && s.q(EE_UNITS_EU_HIT_POINTS) <= 0 && s.q(EE_UNITS_EU_COMPLEX) != 0 {
        s.q(EE_UNITS_EU_ACTIVATE_COMPLEX);
    }
    if s.yes(EE_UNITS_EU_HAS_TILE_NODE) && s.yes(EE_UNITS_EU_IS_CURRENT_PLAYER) && s.yes(EE_UNITS_EU_SELECTED) {
        s.q(EE_UNITS_EU_POST_424);
    }
}

/// `DepleteResource(amount)`: the amount taken from the resource; an exhausted resource unit leaves the minimap and
/// the game.
pub fn deplete_resource<H: EuHost + ?Sized>(h: &mut H, unit: u64, amount: i64) {
    let s = &mut S { h, unit };
    s.qb(EE_UNITS_EU_TAKE_RESOURCE, amount);
    if s.q(EE_UNITS_EU_RESOURCE_AMOUNT) > 0 {
        return;
    }
    s.qb(EE_UNITS_EU_SET_RESOURCE_AMOUNT, 0);
    if s.q(EE_UNITS_EU_CATEGORY) != CATEGORY_RESOURCE {
        return;
    }
    if !s.yes(EE_UNITS_EU_CLOAKED_ENEMY) {
        s.qbc(EE_UNITS_EU_MINIMAP_MOVE, OFF_MAP, OFF_MAP);
    }
    s.qb(EE_UNITS_EU_UNIT_TAKEN, 0);
    s.q(EE_UNITS_EU_KILL_UNIT);
}

// ---- the bodies (`ee_units_eu_body`'s `what`)
pub const EE_UNITS_EU_DO_MOVE_TO: i32 = 1;
pub const EE_UNITS_EU_DO_KILL: i32 = 2;
pub const EE_UNITS_EU_DO_FIND_AREA_TILE: i32 = 3;
pub const EE_UNITS_EU_DO_SET_HIT_POINTS: i32 = 4;
pub const EE_UNITS_EU_DO_DEPLETE_RESOURCE: i32 = 5;

/// Runs body `what` on `unit` (its arguments `a`, `b`, `c` as the member takes them).
pub fn run<H: EuHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: i64, b: i64, c: i64) -> i64 {
    match what {
        EE_UNITS_EU_DO_MOVE_TO => move_to(h, unit, a, b, c != 0),
        EE_UNITS_EU_DO_KILL => kill(h, unit, a & 1 != 0, a & 2 != 0, a & 4 != 0),
        EE_UNITS_EU_DO_FIND_AREA_TILE => {
            let (x, y) = find_area_tile(h, unit, a != 0, b);
            return i64::from(x as u32) | i64::from(y as u32) << 32;
        }
        EE_UNITS_EU_DO_SET_HIT_POINTS => set_hit_points(h, unit, a),
        EE_UNITS_EU_DO_DEPLETE_RESOURCE => deplete_resource(h, unit, a),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::eu_hit::game::Game;
    use std::os::raw::c_void;

    /// `EEUnit`'s body `what` (`EE_UNITS_EU_DO_*`) on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_eu_body(what: i32, unit: *mut c_void, a: i64, b: i64, c: i64) -> i64 {
        run(&mut Game, what, unit as usize as u64, a, b, c)
    }

    /// `EEUnit::CanGather(other, x, y, visible, ignore_amount)` on `unit`.
    ///
    /// # Safety
    /// `unit` and `other` are live `EEUnit`s, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_eu_can_gather(
        unit: *mut c_void,
        other: *mut c_void,
        x: i64,
        y: i64,
        visible: u8,
        ignore: u8,
    ) -> u8 {
        u8::from(can_gather(&mut Game, unit as usize as u64, other as usize as u64, x, y, visible != 0, ignore != 0))
    }

    /// `EEUnit::GetNearestFootprintTile(other, out_x, out_y)` on `unit`: the squared distance, the tile written.
    ///
    /// # Safety
    /// `unit` and `other` are live `EEUnit`s, the outs writable.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_eu_nearest_footprint_tile(
        unit: *mut c_void,
        other: *mut c_void,
        out_x: *mut c_long,
        out_y: *mut c_long,
    ) -> f32 {
        let (d, x, y) = nearest_footprint_tile(&mut Game, unit as usize as u64, other as usize as u64);
        // SAFETY: the caller's outs
        unsafe {
            *out_x = x as c_long;
            *out_y = y as c_long;
        }
        d
    }
}

#[cfg(test)]
#[path = "eu_body_tests.rs"]
mod tests;
