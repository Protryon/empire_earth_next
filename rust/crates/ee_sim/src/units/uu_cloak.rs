//! The cloaking and space dock utilities over their C++ statements (the units' lane, round 7; were
//! EECloakingUtilities.cpp): `Fn23CEE2` (a unit's translucency and opacity), `EEGetSpaceDockYaw`,
//! `EEIsSpaceDockSiteOpen`, `EECountTilesAround`, `EEAddCloakEffect`, `EERemoveCloakEffect`, `EEGetLandContinentOf`,
//! `EEUpdateLandContinents`. Every C++ statement of the originals is one operation of [`CkHost::ck`] (the game's:
//! `ee_units_ck_host`, EECloakingUtilities.cpp), asked in the original's order; the loops, choices and arithmetic are
//! here.

#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

// ---- the statements (`op`; `a` an object, `c`, `d` values, `f` floats; the answer)
/// the terrain type of `gWorld.mMap->GetTile(c, d)` (-1: no tile)
pub const EE_UNITS_CK_TERRAIN: i32 = 1;
/// `gWorld.mMap->IsEdgeTile(c, d)`
pub const EE_UNITS_CK_IS_EDGE: i32 = 2;
/// `EEIsLandTerrain(c)`
pub const EE_UNITS_CK_IS_LAND: i32 = 3;
/// `EECanPlace(player a, kUnitTypeSpaceDock, &the point f, c, d, 0, 0)`
pub const EE_UNITS_CK_CAN_PLACE_DOCK: i32 = 4;
/// `EEGetDockFacing(c, d)`
pub const EE_UNITS_CK_DOCK_FACING: i32 = 5;
/// unit `a`'s `GetCommandType()`
pub const EE_UNITS_CK_COMMAND_TYPE: i32 = 6;
/// the bits of `(float)` unit `a`'s complex unit's `mRottingAwayTime / ` its class's `mRottingTime` (0: no rotting
/// time... asked only when it is not 0)
pub const EE_UNITS_CK_ROT_FRACTION: i32 = 7;
/// unit `a`'s class's `mRottingTime != 0`
pub const EE_UNITS_CK_HAS_ROTTING_TIME: i32 = 8;
/// unit `a`'s `mIsCloakable`
pub const EE_UNITS_CK_IS_CLOAKABLE: i32 = 9;
/// unit `a`'s `mIsCloaked`
pub const EE_UNITS_CK_IS_CLOAKED: i32 = 10;
/// `gFreeCamera`
pub const EE_UNITS_CK_FREE_CAMERA: i32 = 11;
/// the bits of unit `a`'s `mOpacity`
pub const EE_UNITS_CK_OPACITY: i32 = 12;
/// `gWorld.GetCurrentPlayer()`
pub const EE_UNITS_CK_CURRENT_PLAYER: i32 = 13;
/// unit `a`'s `mPlayer`
pub const EE_UNITS_CK_PLAYER: i32 = 14;
/// unit `a`'s `mPlayer->mEnemies[the current player's mPlayerID]`
pub const EE_UNITS_CK_ENEMY_OF_VIEWER: i32 = 15;
/// `gWorld.GetCurrentPlayer()->mObserverMode`
pub const EE_UNITS_CK_OBSERVER: i32 = 16;
/// `*(float*)a = f[0]`
pub const EE_UNITS_CK_SET_FLOAT: i32 = 17;
/// the cloak effect created on unit `a` (`kGraphicEffectMarketTicketTape`, the handle kept); the result
pub const EE_UNITS_CK_CREATE_CLOAK: i32 = 18;
/// unit `a`'s complex unit's `mCloaked = the kept handle's mValue`
pub const EE_UNITS_CK_KEEP_CLOAK: i32 = 19;
/// unit `a`'s complex unit's `(long)mCloaked`
pub const EE_UNITS_CK_CLOAKED: i32 = 20;
/// the effect handle `c` destroyed
pub const EE_UNITS_CK_DESTROY_EFFECT: i32 = 21;
/// unit `a`'s complex unit's `mCloaked = 0`
pub const EE_UNITS_CK_CLEAR_CLOAKED: i32 = 22;
/// `gWorld.mMap->mTerrainMesh.GetIsSpaceMap()`
pub const EE_UNITS_CK_SPACE_MAP: i32 = 23;
/// complex unit `a`'s `mTileX`
pub const EE_UNITS_CK_TILE_X: i32 = 24;
/// complex unit `a`'s `mTileY`
pub const EE_UNITS_CK_TILE_Y: i32 = 25;
/// `gContinentManager.GetContinent((c, d), kAIPathNonSpace)`
pub const EE_UNITS_CK_CONTINENT: i32 = 26;
/// `gWorld.mUnits.mFirst`
pub const EE_UNITS_CK_FIRST_UNIT: i32 = 27;
/// unit `a`'s `mTileNode->mNext`
pub const EE_UNITS_CK_NEXT_UNIT: i32 = 28;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_CK_COMPLEX_OF: i32 = 29;
/// complex unit `a`'s class's `mDomains`
pub const EE_UNITS_CK_DOMAINS: i32 = 30;
/// complex unit `a`'s class's `mUnitType`
pub const EE_UNITS_CK_UNIT_TYPE: i32 = 31;
/// complex unit `a`'s `mPlanetID = c`
pub const EE_UNITS_CK_SET_PLANET: i32 = 32;

// ---- the bodies (`ee_units_ck_body`'s `what`)
pub const EE_UNITS_CK_DO_TRANSLUCENT: i32 = 1;
pub const EE_UNITS_CK_DO_SPACE_DOCK_YAW: i32 = 2;
pub const EE_UNITS_CK_DO_SPACE_DOCK_SITE_OPEN: i32 = 3;
pub const EE_UNITS_CK_DO_COUNT_TILES_AROUND: i32 = 4;
pub const EE_UNITS_CK_DO_ADD_CLOAK_EFFECT: i32 = 5;
pub const EE_UNITS_CK_DO_REMOVE_CLOAK_EFFECT: i32 = 6;
pub const EE_UNITS_CK_DO_LAND_CONTINENT_OF: i32 = 7;
pub const EE_UNITS_CK_DO_UPDATE_LAND_CONTINENTS: i32 = 8;

const TERRAIN_SPACE: i64 = 12;
const ACTION_UNIT_ROTTING: i64 = 6002;
const MOVEMENT_AIR: i64 = 1;
const UNIT_TYPE_SPACE_DOCK: i64 = 0x2d8;
const UNIT_TYPE_SPACE_TURRET: i64 = 0x302;
const UNIT_TYPE_ORBITAL_SPACE_STATION: i64 = 0x31f;
/// the opacity range of a cloaked unit seen by an enemy (the original's dynamic initializer subtracts the two)
const CLOAKED_FULL_OPACITY: f32 = 1.0;
const CLOAKED_OPACITY: f32 = 0.6;
/// the wall pieces' yaws (`kWallRotations`)
const WALL_ROTATIONS: [f32; 4] = [0.0, 3.1415927, -1.5707964, 1.5707964];

/// The C++ statements of the bodies.
pub trait CkHost {
    /// one statement (`EE_UNITS_CK_*`)
    fn ck(&mut self, op: i32, a: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64;
}

fn sa<H: CkHost + ?Sized>(h: &mut H, op: i32, a: u64) -> i64 {
    h.ck(op, a, 0, 0, &mut [0.0; 4])
}
fn scd<H: CkHost + ?Sized>(h: &mut H, op: i32, c: i64, d: i64) -> i64 {
    h.ck(op, 0, c, d, &mut [0.0; 4])
}
fn s<H: CkHost + ?Sized>(h: &mut H, op: i32) -> i64 {
    h.ck(op, 0, 0, 0, &mut [0.0; 4])
}
fn yes(v: i64) -> bool {
    v != 0
}
fn flt(bits: i64) -> f32 {
    f32::from_bits(bits as u32)
}
/// `(long)` of a small float: a truncation
fn trunc(v: f32) -> c_long {
    v as c_long
}

/// `Fn23CEE2(unit, translucent, opacity)`: whether the unit is drawn translucent; its opacity (at the `float*`
/// `opacity`): the rotting fade, or its cloak opacity (dimmed and clamped for an enemy viewer). A translucent unit
/// that is not cloaked is left as it is.
pub fn translucent<H: CkHost + ?Sized>(h: &mut H, unit: u64, translucent: bool, opacity: u64) -> bool {
    let mut result = translucent;
    if translucent && !(yes(sa(h, EE_UNITS_CK_IS_CLOAKABLE, unit)) && yes(sa(h, EE_UNITS_CK_IS_CLOAKED, unit))) {
        return result;
    }
    let set = |h: &mut H, v: f32| {
        h.ck(EE_UNITS_CK_SET_FLOAT, opacity, 0, 0, &mut [v, 0.0, 0.0, 0.0]);
    };
    if sa(h, EE_UNITS_CK_COMMAND_TYPE, unit) == ACTION_UNIT_ROTTING {
        if yes(sa(h, EE_UNITS_CK_HAS_ROTTING_TIME, unit)) {
            let v = flt(sa(h, EE_UNITS_CK_ROT_FRACTION, unit));
            set(h, v);
        } else {
            set(h, 0.0);
        }
    } else if !yes(sa(h, EE_UNITS_CK_IS_CLOAKABLE, unit)) || yes(s(h, EE_UNITS_CK_FREE_CAMERA)) {
        set(h, 1.0);
    } else {
        let value = flt(sa(h, EE_UNITS_CK_OPACITY, unit));
        set(h, value);
        if value < 1.0 {
            result = true;
        }
        let viewer = s(h, EE_UNITS_CK_CURRENT_PLAYER);
        if viewer != sa(h, EE_UNITS_CK_PLAYER, unit)
            && sa(h, EE_UNITS_CK_ENEMY_OF_VIEWER, unit) != 0
            && !yes(s(h, EE_UNITS_CK_OBSERVER))
        {
            let range = CLOAKED_FULL_OPACITY - CLOAKED_OPACITY;
            let mut v = (value - CLOAKED_OPACITY) / range * CLOAKED_OPACITY;
            if v < 0.0 {
                v = 0.0;
            } else if v > CLOAKED_OPACITY {
                v = CLOAKED_OPACITY;
            }
            set(h, v);
        }
    }
    result
}

fn terrain<H: CkHost + ?Sized>(h: &mut H, x: c_long, y: c_long) -> Option<i64> {
    let t = scd(h, EE_UNITS_CK_TERRAIN, x as i64, y as i64);
    if t < 0 {
        None
    } else {
        Some(t)
    }
}

/// `EEGetSpaceDockYaw(x, y, size)`: a space dock's yaw from which of the 2 x 2 tiles at (x, y) are space (one or
/// two space tiles), else the dock facing's.
pub fn space_dock_yaw<H: CkHost + ?Sized>(h: &mut H, x: c_long, y: c_long) -> f32 {
    let left = trunc(x as f32 - 1.5) + 1;
    let bottom = trunc(y as f32 - 1.5) + 1;
    let right = trunc(x as f32 + 1.5);
    let top = trunc(y as f32 + 1.5);
    let mut count = 0u16;
    for tx in left..right {
        for ty in bottom..top {
            if let Some(t) = terrain(h, tx, ty) {
                if t != TERRAIN_SPACE {
                    count = count.wrapping_add(1);
                }
            }
        }
    }
    if count == 3 {
        let mut water = 0u32;
        let mut index = 0u32;
        for tx in left..right {
            for ty in bottom..top {
                let t = terrain(h, tx, ty);
                if t.is_none() || t == Some(TERRAIN_SPACE) {
                    water = index;
                }
                index += 1;
            }
        }
        match water as u16 {
            3 => -0.78539819,
            2 => -2.3561945,
            1 => 0.78539819,
            0 => 2.3561945,
            _ => 0.0,
        }
    } else if count == 2 {
        let mut kinds = [0i64; 4];
        let mut index = 0usize;
        for tx in left..right {
            for ty in bottom..top {
                kinds[(index as u16) as usize] = terrain(h, tx, ty).unwrap_or(TERRAIN_SPACE);
                index += 1;
            }
        }
        let piece = if kinds[1] != TERRAIN_SPACE && kinds[3] != TERRAIN_SPACE {
            1
        } else if kinds[0] != TERRAIN_SPACE && kinds[1] != TERRAIN_SPACE {
            2
        } else if kinds[2] != TERRAIN_SPACE && kinds[3] != TERRAIN_SPACE {
            3
        } else {
            0
        };
        WALL_ROTATIONS[piece]
    } else {
        WALL_ROTATIONS[scd(h, EE_UNITS_CK_DOCK_FACING, x as i64, y as i64) as usize]
    }
}

/// `EECountTilesAround(x, y, any)`: the 3 x 3 tiles around (x, y) that are land (`any`) or space; 0 when the centre
/// is off the map.
pub fn count_tiles_around<H: CkHost + ?Sized>(h: &mut H, x: c_long, y: c_long, any: bool) -> u16 {
    if terrain(h, x, y).is_none() {
        return 0;
    }
    let mut count: c_long = 0;
    for tx in x - 1..=x + 1 {
        for ty in y - 1..=y + 1 {
            let Some(t) = terrain(h, tx, ty) else { continue };
            if any {
                if yes(h.ck(EE_UNITS_CK_IS_LAND, 0, t, 0, &mut [0.0; 4])) {
                    count += 1;
                }
            } else if t == TERRAIN_SPACE {
                count += 1;
            }
        }
    }
    count as u16
}

/// `EEIsSpaceDockSiteOpen(x, y, player)`: whether a space dock fits at (x, y): its inner tiles placeable, its border
/// on the map with at least two land tiles, its inner land and space fitting a dock.
pub fn space_dock_site_open<H: CkHost + ?Sized>(h: &mut H, x: c_long, y: c_long, player: u64) -> bool {
    let mut ok = true;
    let mut blocking: c_long = 0;
    let mut inner = 0u16;
    let left = trunc(x as f32 - 1.5);
    let bottom = trunc(y as f32 - 1.5);
    let right = trunc(x as f32 + 1.5);
    let top = trunc(y as f32 + 1.5);
    let point = [x as f32, y as f32, 0.0, 0.0];
    let mut tx = left;
    while tx <= right && ok {
        let mut ty = bottom;
        while ty <= top && ok {
            let t = terrain(h, tx, ty);
            if tx == left || ty == bottom || tx == right || ty == top {
                if t.is_none() || yes(scd(h, EE_UNITS_CK_IS_EDGE, tx as i64, ty as i64)) {
                    ok = false;
                } else if yes(h.ck(EE_UNITS_CK_IS_LAND, 0, t.unwrap(), 0, &mut [0.0; 4])) {
                    blocking += 1;
                }
            } else {
                let placeable = match t {
                    Some(t) => {
                        let mut f = point;
                        let p = yes(h.ck(EE_UNITS_CK_CAN_PLACE_DOCK, player, tx as i64, ty as i64, &mut f));
                        if yes(h.ck(EE_UNITS_CK_IS_LAND, 0, t, 0, &mut [0.0; 4])) {
                            inner = inner.wrapping_add(1);
                        }
                        p
                    }
                    None => false,
                };
                if !placeable {
                    ok = false;
                }
            }
            ty += 1;
        }
        tx += 1;
    }
    if ok {
        if inner == 3 {
            let mut found = false;
            let mut fx = left + 1;
            let mut fy = bottom + 1;
            while fx < right && !found {
                fy = bottom + 1;
                while fy < top && !found {
                    if terrain(h, fx, fy) == Some(TERRAIN_SPACE) {
                        found = true;
                    }
                    fy += 1;
                }
                fx += 1;
            }
            if count_tiles_around(h, fx - 1, fy - 1, false) < 5 {
                ok = false;
            }
        } else if inner == 2 {
            // two blocking tiles inside: accepted as is
        } else if inner == 1 {
            let mut most = 0u16;
            for cx in left + 1..right {
                for cy in bottom + 1..top {
                    if terrain(h, cx, cy) != Some(TERRAIN_SPACE) {
                        continue;
                    }
                    let n = count_tiles_around(h, cx, cy, true);
                    if n > most {
                        most = n;
                    }
                }
            }
            if most < 5 {
                ok = false;
            }
        } else {
            ok = false;
        }
    }
    if blocking < 2 {
        ok = false;
    }
    ok
}

/// `EEAddCloakEffect(unit)`: the cloaking effect attached to the unit, its handle kept in the complex unit.
pub fn add_cloak_effect<H: CkHost + ?Sized>(h: &mut H, unit: u64) {
    if sa(h, EE_UNITS_CK_CREATE_CLOAK, unit) == 0 {
        sa(h, EE_UNITS_CK_KEEP_CLOAK, unit);
    }
}

/// `EERemoveCloakEffect(unit)`: the unit's cloaking effect (when it has one) destroyed and its handle cleared.
pub fn remove_cloak_effect<H: CkHost + ?Sized>(h: &mut H, unit: u64) {
    let handle = sa(h, EE_UNITS_CK_CLOAKED, unit);
    if handle as c_long <= 0 {
        return;
    }
    h.ck(EE_UNITS_CK_DESTROY_EFFECT, 0, handle, 0, &mut [0.0; 4]);
    sa(h, EE_UNITS_CK_CLEAR_CLOAKED, unit);
}

fn land_at<H: CkHost + ?Sized>(h: &mut H, x: c_long, y: c_long) -> bool {
    match terrain(h, x, y) {
        Some(t) => t != TERRAIN_SPACE && !yes(scd(h, EE_UNITS_CK_IS_EDGE, x as i64, y as i64)),
        None => false,
    }
}

/// `EEGetLandContinentOf(unit)`: the planet under the unit or nearest it within 6 tiles; 1 on a non-space map, 0
/// when none.
pub fn land_continent_of<H: CkHost + ?Sized>(h: &mut H, unit: u64) -> u16 {
    if !yes(s(h, EE_UNITS_CK_SPACE_MAP)) {
        return 1;
    }
    let ux = sa(h, EE_UNITS_CK_TILE_X, unit) as c_long;
    let uy = sa(h, EE_UNITS_CK_TILE_Y, unit) as c_long;
    let (mut x, mut y) = (ux, uy);
    let mut found = land_at(h, x, y);
    let mut step = 1;
    while step <= 6 && !found {
        let mut i = -1;
        while i <= 1 && !found {
            let mut j = -1;
            while j <= 1 && !found {
                if !(i == 0 && j == 0) {
                    x = i * step + ux;
                    y = j * step + uy;
                    if land_at(h, x, y) {
                        found = true;
                    }
                }
                j += 1;
            }
            i += 1;
        }
        step += 1;
    }
    if found {
        scd(h, EE_UNITS_CK_CONTINENT, x as i64, y as i64) as u16
    } else {
        0
    }
}

/// `EEUpdateLandContinents()`: the planet of every complex air unit, space dock, space turret and orbital space
/// station on the map.
pub fn update_land_continents<H: CkHost + ?Sized>(h: &mut H) {
    let mut unit = s(h, EE_UNITS_CK_FIRST_UNIT) as u64;
    while unit != 0 {
        let complex = sa(h, EE_UNITS_CK_COMPLEX_OF, unit) as u64;
        if complex != 0 {
            let t = sa(h, EE_UNITS_CK_UNIT_TYPE, complex);
            if sa(h, EE_UNITS_CK_DOMAINS, complex) & MOVEMENT_AIR != 0
                || t == UNIT_TYPE_SPACE_DOCK
                || t == UNIT_TYPE_SPACE_TURRET
                || t == UNIT_TYPE_ORBITAL_SPACE_STATION
            {
                let planet = land_continent_of(h, complex);
                h.ck(EE_UNITS_CK_SET_PLANET, complex, i64::from(planet), 0, &mut [0.0; 4]);
            }
        }
        unit = sa(h, EE_UNITS_CK_NEXT_UNIT, unit) as u64;
    }
}

/// Runs body `what` (`EE_UNITS_CK_DO_*`).
pub fn run<H: CkHost + ?Sized>(h: &mut H, what: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
    match what {
        EE_UNITS_CK_DO_TRANSLUCENT => i64::from(translucent(h, a, c & 0xff != 0, b)),
        EE_UNITS_CK_DO_SPACE_DOCK_YAW => i64::from(space_dock_yaw(h, c as c_long, d as c_long).to_bits()),
        EE_UNITS_CK_DO_SPACE_DOCK_SITE_OPEN => i64::from(space_dock_site_open(h, c as c_long, d as c_long, a)),
        EE_UNITS_CK_DO_COUNT_TILES_AROUND => i64::from(count_tiles_around(h, c as c_long, d as c_long, a & 0xff != 0)),
        EE_UNITS_CK_DO_ADD_CLOAK_EFFECT => {
            add_cloak_effect(h, a);
            0
        }
        EE_UNITS_CK_DO_REMOVE_CLOAK_EFFECT => {
            remove_cloak_effect(h, a);
            0
        }
        EE_UNITS_CK_DO_LAND_CONTINENT_OF => i64::from(land_continent_of(h, a)),
        EE_UNITS_CK_DO_UPDATE_LAND_CONTINENTS => {
            update_land_continents(h);
            0
        }
        _ => 0,
    }
}

#[cfg(not(test))]
mod game {
    use super::*;

    extern "C" {
        fn ee_units_ck_host(op: i32, a: u64, c: i64, d: i64, f: *mut [f32; 4]) -> i64;
    }

    struct Game;

    impl CkHost for Game {
        fn ck(&mut self, op: i32, a: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the body runs inside the C++ function that handed over its live objects
            unsafe { ee_units_ck_host(op, a, c, d, f) }
        }
    }

    /// The cloaking utility `what` (`EE_UNITS_CK_DO_*`).
    ///
    /// # Safety
    /// The objects are live game objects.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ck_body(what: i32, a: u64, b: u64, c: i64, d: i64) -> i64 {
        run(&mut Game, what, a, b, c, d)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// A map of terrain types (absent: no tile) and a recording host.
    #[derive(Default)]
    struct Mock {
        tiles: BTreeMap<(i64, i64), i64>,
        answers: BTreeMap<(i32, u64), i64>,
        log: Vec<(i32, u64, i64, i64, f32)>,
    }

    impl CkHost for Mock {
        fn ck(&mut self, op: i32, a: u64, c: i64, d: i64, f: &mut [f32; 4]) -> i64 {
            self.log.push((op, a, c, d, f[0]));
            match op {
                EE_UNITS_CK_TERRAIN => *self.tiles.get(&(c, d)).unwrap_or(&-1),
                EE_UNITS_CK_IS_LAND => i64::from(c == 1),
                EE_UNITS_CK_CAN_PLACE_DOCK => 1,
                _ => *self.answers.get(&(op, a)).unwrap_or(&0),
            }
        }
    }

    fn grid(f: impl Fn(i64, i64) -> i64) -> Mock {
        let mut m = Mock::default();
        for x in 0..20 {
            for y in 0..20 {
                m.tiles.insert((x, y), f(x, y));
            }
        }
        m
    }

    #[test]
    fn a_dock_with_one_space_tile_faces_it() {
        // the 2 x 2 at (5, 5) is tiles 4..5 x 4..5; (5, 5) is space (index 3)
        let mut h = grid(|x, y| if (x, y) == (5, 5) { TERRAIN_SPACE } else { 1 });
        assert_eq!(space_dock_yaw(&mut h, 5, 5), -0.78539819);
    }

    #[test]
    fn a_dock_with_two_space_tiles_takes_a_wall_rotation() {
        // tiles (4, 4) (4, 5) land, (5, 4) (5, 5) space: kinds [land, land, space, space] -> piece 2
        let mut h = grid(|x, _| if x == 5 { TERRAIN_SPACE } else { 1 });
        assert_eq!(space_dock_yaw(&mut h, 5, 5), WALL_ROTATIONS[2]);
    }

    #[test]
    fn a_dock_on_land_asks_the_facing() {
        let mut h = grid(|_, _| 1);
        h.answers.insert((EE_UNITS_CK_DOCK_FACING, 0), 3);
        assert_eq!(space_dock_yaw(&mut h, 5, 5), WALL_ROTATIONS[3]);
    }

    #[test]
    fn tiles_around_count_land_or_space() {
        let mut h = grid(|x, _| if x < 5 { 1 } else { TERRAIN_SPACE });
        assert_eq!(count_tiles_around(&mut h, 5, 5, true), 3);
        assert_eq!(count_tiles_around(&mut h, 5, 5, false), 6);
        assert_eq!(count_tiles_around(&mut h, 50, 5, true), 0);
    }

    #[test]
    fn a_site_needs_land_on_its_border() {
        // all space: no blocking border tile
        let mut h = grid(|_, _| TERRAIN_SPACE);
        assert!(!space_dock_site_open(&mut h, 5, 5, 9));
        // inner (4..5, 4..5) one land tile, the border land, the rest space
        let mut h = grid(|x, y| if (3..=6).contains(&x) && (3..=6).contains(&y) && !((4..=5).contains(&x) && (4..=5).contains(&y)) { 1 } else if (x, y) == (4, 4) { 1 } else { TERRAIN_SPACE });
        // a space inner tile's 3 x 3 has at least five land tiles
        assert!(space_dock_site_open(&mut h, 5, 5, 9));
        let placed = h.log.iter().filter(|e| e.0 == EE_UNITS_CK_CAN_PLACE_DOCK).count();
        assert_eq!(placed, 4);
    }

    #[test]
    fn an_enemy_sees_a_cloaked_unit_dimmed() {
        let mut h = Mock::default();
        h.answers.insert((EE_UNITS_CK_IS_CLOAKABLE, 1), 1);
        h.answers.insert((EE_UNITS_CK_IS_CLOAKED, 1), 1);
        h.answers.insert((EE_UNITS_CK_OPACITY, 1), i64::from(0.8f32.to_bits()));
        h.answers.insert((EE_UNITS_CK_CURRENT_PLAYER, 0), 2);
        h.answers.insert((EE_UNITS_CK_PLAYER, 1), 3);
        h.answers.insert((EE_UNITS_CK_ENEMY_OF_VIEWER, 1), 1);
        assert!(translucent(&mut h, 1, true, 77));
        let sets: Vec<f32> = h.log.iter().filter(|e| e.0 == EE_UNITS_CK_SET_FLOAT).map(|e| e.4).collect();
        let range = CLOAKED_FULL_OPACITY - CLOAKED_OPACITY;
        assert_eq!(sets, vec![0.8, ((0.8f32 - 0.6) / range * 0.6).clamp(0.0, 0.6)]);
    }

    #[test]
    fn a_translucent_uncloaked_unit_is_left_alone() {
        let mut h = Mock::default();
        assert!(translucent(&mut h, 1, true, 77));
        assert!(h.log.iter().all(|e| e.0 != EE_UNITS_CK_SET_FLOAT));
    }

    #[test]
    fn the_land_continent_is_searched_in_rings() {
        let mut h = grid(|x, y| if (x, y) == (7, 5) { 1 } else { TERRAIN_SPACE });
        h.answers.insert((EE_UNITS_CK_SPACE_MAP, 0), 1);
        h.answers.insert((EE_UNITS_CK_TILE_X, 1), 5);
        h.answers.insert((EE_UNITS_CK_TILE_Y, 1), 5);
        h.answers.insert((EE_UNITS_CK_CONTINENT, 0), 4);
        assert_eq!(land_continent_of(&mut h, 1), 4);
        let asked = h.log.iter().find(|e| e.0 == EE_UNITS_CK_CONTINENT).unwrap();
        assert_eq!((asked.2, asked.3), (7, 5));
    }

    #[test]
    fn the_cloak_effect_is_kept_and_removed() {
        let mut h = Mock::default();
        add_cloak_effect(&mut h, 1);
        assert!(h.log.iter().any(|e| e.0 == EE_UNITS_CK_KEEP_CLOAK));
        let mut h = Mock::default();
        h.answers.insert((EE_UNITS_CK_CLOAKED, 1), 5);
        remove_cloak_effect(&mut h, 1);
        assert!(h.log.iter().any(|e| e.0 == EE_UNITS_CK_DESTROY_EFFECT && e.2 == 5));
        assert!(h.log.iter().any(|e| e.0 == EE_UNITS_CK_CLEAR_CLOAKED));
    }
}
