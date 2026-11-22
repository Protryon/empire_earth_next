//! The unit utilities' formula fills over their C++ statements (the units' lane, round 8; were
//! EEFOFindNewTargetNearOldTarget.cpp): `EEGetBallisticPitch` (CBBP), `EECanHeal` (CBCH), `EEApproachAttachPoint`
//! (CBAP: the weapon tag point's model chain) and the location screen's `Screen`. The records' reads are one
//! `EE_UNITS_LF_*` statement each of `ee_units_lf_host` (uu_line.rs's host), asked in the original's order; the
//! formulas are ee_combat's (`ballistic`, `can_heal`, `launch`).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::app::capture_record::Record;
use crate::combat::decide::{can_heal, EeCombatCanHealIn};
use crate::combat::launch::{launch, EeCombatLaunchIn, EeCombatLaunchOut, EE_COMBAT_LAUNCH_KIND_ATTACH};
use crate::combat::{ballistic, EeCombatBallisticIn, EeCombatBallisticOut};
use crate::units::uu_line::*;

/// the shot's launch speed: `f[0] =` stats `a`'s `mLaunchSpeed`
pub const EE_UNITS_LF_LAUNCH_SPEED: i32 = 29;
/// `f[0] =` stats `a`'s `mUnitClass->mProjectileClass->GetAcceleration()`
pub const EE_UNITS_LF_PROJECTILE_ACCELERATION: i32 = 30;
/// complex unit `a`'s `mUnitClass->mShootsHigh`
pub const EE_UNITS_LF_SHOOTS_HIGH: i32 = 31;
/// stats `a`'s `mCanFireOverWalls`
pub const EE_UNITS_LF_STATS_OVER_WALLS: i32 = 32;
/// complex unit `a`'s `mIsFiringHigh`
pub const EE_UNITS_LF_FIRING_HIGH: i32 = 33;
/// complex unit `a`'s `mAnimation`
pub const EE_UNITS_LF_ANIMATION: i32 = 34;
/// unit `a`'s `mStats->mWonderUnitSet`
pub const EE_UNITS_LF_WONDER_SET: i32 = 35;
/// complex unit `a`'s `IsRepairable()`
pub const EE_UNITS_LF_REPAIRABLE: i32 = 36;
/// `f[0] =` unit `a`'s `mUnitClass->GetHealThreshold()`
pub const EE_UNITS_LF_HEAL_THRESHOLD: i32 = 37;
/// unit `a`'s `mPlayer`
pub const EE_UNITS_LF_PLAYER: i32 = 38;
/// unit `a`'s `mPlayer->mEnemies[unit b's mPlayer->GetPlayerID()] == 0`
pub const EE_UNITS_LF_ALLIED: i32 = 39;
/// `gUnitSetManager.GetSize()`
pub const EE_UNITS_LF_SET_COUNT: i32 = 40;
/// `gUnitSetManager.Contains(complex unit a's mUnitClass->mUnitType, c)`
pub const EE_UNITS_LF_SET_CONTAINS: i32 = 41;
/// unit `a`'s `GetModel()->mSound1Frames[0]`
pub const EE_UNITS_LF_SOUND_FRAME0: i32 = 42;
/// tag point list `a`'s `GetNumTagPoints()`
pub const EE_UNITS_LF_TAG_POINT_COUNT: i32 = 43;
/// screen `a`'s `mB08`
pub const EE_UNITS_LF_SCREEN_ON_MAP: i32 = 44;
/// `::gWorld.mMap->GetTileAtLocation(U2DPoint<long>(c, d)) != NULL`
pub const EE_UNITS_LF_TILE_AT_LOCATION: i32 = 45;
/// screen `a`'s `mContinentID`
pub const EE_UNITS_LF_SCREEN_CONTINENT: i32 = 46;
/// `gMapWidth`
pub const EE_UNITS_LF_MAP_WIDTH: i32 = 47;
/// `gMapHeight`
pub const EE_UNITS_LF_MAP_HEIGHT: i32 = 48;
/// `gContinentManager.GetContinentID(c, d)`
pub const EE_UNITS_LF_CONTINENT_ID: i32 = 49;
/// screen `a`'s `mAreaID`
pub const EE_UNITS_LF_SCREEN_AREA: i32 = 50;
/// screen `a`'s `mUseWaterAreas`
pub const EE_UNITS_LF_SCREEN_WATER: i32 = 51;
/// `gContinentManager.GetContinent(U2DPoint<long>(c, d), b)`
pub const EE_UNITS_LF_CONTINENT_OF: i32 = 52;
/// the 16 floats at `c` = complex unit `a`'s `mPhysicalModel.GetLocation().mMatrix` (EE_UNITS_LF_LOCATION's
/// pointer in `c` too)
const LOCATION: i32 = EE_UNITS_LF_LOCATION;

pub(crate) const TAG_CBBP: u32 = u32::from_le_bytes(*b"CBBP");
pub(crate) const TAG_CBCH: u32 = u32::from_le_bytes(*b"CBCH");
pub(crate) const TAG_CBAP: u32 = u32::from_le_bytes(*b"CBAP");

fn op<H: LfHost + ?Sized>(h: &mut H, code: i32, a: u64) -> i64 {
    h.lf(code, a, 0, 0, 0, &mut [0.0; 4])
}

fn opf<H: LfHost + ?Sized>(h: &mut H, code: i32, a: u64) -> f32 {
    let mut f = [0.0; 4];
    h.lf(code, a, 0, 0, 0, &mut f);
    f[0]
}

/// A record's bytes (a repr(C) record of 4-byte fields: no padding), as the C++'s `Bytes(&in, sizeof(in))`.
fn bytes<T: Copy>(v: &T) -> &[u8] {
    // SAFETY: a plain record read as its bytes; the callers' records have no padding
    unsafe { std::slice::from_raw_parts((v as *const T).cast::<u8>(), std::mem::size_of::<T>()) }
}

/// `EEGetBallisticPitch(unit, stats, from, to, &angle)`: the launch angle at the stats' speed under the projectile
/// class's gravity (the high arc for a class that shoots high, or one firing high over walls in animation 8);
/// whether it reaches. CBBP records it.
pub fn ballistic_pitch<H: LfHost + ?Sized>(
    h: &mut H,
    unit: u64,
    stats: u64,
    from: [f32; 3],
    to: [f32; 3],
) -> (bool, f32) {
    let i = EeCombatBallisticIn {
        from_x: from[0],
        from_y: from[1],
        from_z: from[2],
        to_x: to[0],
        to_y: to[1],
        to_z: to[2],
        speed: opf(h, EE_UNITS_LF_LAUNCH_SPEED, stats),
        acceleration: opf(h, EE_UNITS_LF_PROJECTILE_ACCELERATION, stats),
        high: i32::from(
            op(h, EE_UNITS_LF_SHOOTS_HIGH, unit) != 0
                || (op(h, EE_UNITS_LF_STATS_OVER_WALLS, stats) != 0
                    && op(h, EE_UNITS_LF_FIRING_HIGH, unit) != 0
                    && op(h, EE_UNITS_LF_ANIMATION, unit) as c_ulong == 8),
        ),
    };
    let out: EeCombatBallisticOut = ballistic(&i);
    if h.capture() {
        let mut r = Record::new(TAG_CBBP);
        r.bytes(bytes(&i));
        r.bytes(bytes(&out));
        h.commit(&r);
    }
    (out.reached != 0, out.angle)
}

/// `EECanHeal(unit, target)`: wounded enough, not an enemy, in the healer's set (looked up only for a set in the
/// manager's range); CBCH records it.
pub fn can_heal_target<H: LfHost + ?Sized>(h: &mut H, unit: u64, target: u64) -> bool {
    let set = op(h, EE_UNITS_LF_WONDER_SET, unit) as c_ulong;
    let i = EeCombatCanHealIn {
        has_set: i32::from(set > 0),
        repairable: i32::from(op(h, EE_UNITS_LF_REPAIRABLE, target) != 0),
        hit_points: op(h, EE_UNITS_LF_HIT_POINTS, target) as c_long as i32,
        maximum: op(h, EE_UNITS_LF_MAX_HIT_POINTS, target) as c_long as i32,
        threshold: opf(h, EE_UNITS_LF_HEAL_THRESHOLD, unit),
        same_unit: i32::from(target == unit),
        same_player: i32::from(op(h, EE_UNITS_LF_PLAYER, unit) == op(h, EE_UNITS_LF_PLAYER, target)),
        allied: i32::from(h.lf(EE_UNITS_LF_ALLIED, target, unit, 0, 0, &mut [0.0; 4]) != 0),
        in_set: i32::from(
            set != 0
                && (set as u64) < op(h, EE_UNITS_LF_SET_COUNT, 0) as u64
                && h.lf(EE_UNITS_LF_SET_CONTAINS, target, 0, set as i64, 0, &mut [0.0; 4]) != 0,
        ),
    };
    let result = can_heal(&i);
    if h.capture() {
        let mut r = Record::new(TAG_CBCH);
        r.bytes(bytes(&i));
        r.i32(result);
        h.commit(&r);
    }
    result != 0
}

/// `EEApproachAttachPoint(unit, &point)`: the world position of the unit's current tag point (its model part's
/// chain, the unit's frame), scaled by its size; false (and the point half a tile up) when the part has none.
/// CBAP records it.
pub fn approach_attach_point<H: LfHost + ?Sized>(h: &mut H, unit: u64) -> (bool, [f32; 3]) {
    let model = op(h, EE_UNITS_LF_MODEL, unit) as u64;
    let list = h.lf(EE_UNITS_LF_TAG_LIST, model, unit, 0, 0, &mut [0.0; 4]) as u64;
    let mut i = EeCombatLaunchIn { kind: EE_COMBAT_LAUNCH_KIND_ATTACH, ..Default::default() };
    let mut chain: Vec<[f32; 16]> = Vec::new();
    i.point_count = if list != 0 { op(h, EE_UNITS_LF_TAG_POINT_COUNT, list) as c_ulong as i32 } else { 0 };
    i.weapon_index = op(h, EE_UNITS_LF_WEAPON_INDEX, unit) as c_ulong as i32;
    h.lf(LOCATION, unit, 0, i.location.as_mut_ptr() as i64, 0, &mut [0.0; 4]);
    i.scale = opf(h, EE_UNITS_LF_STATS_SCALE, unit);
    if i.point_count != 0 {
        let index = op(h, EE_UNITS_LF_WEAPON_INDEX, unit);
        let point = h.lf(EE_UNITS_LF_LIST_TAG_POINT, list, 0, index, 0, &mut [0.0; 4]) as u64;
        let frame = op(h, EE_UNITS_LF_SOUND_FRAME0, unit) as c_ulong;
        h.lf(EE_UNITS_LF_POINT_LOCAL, point, 0, frame as i64, i.point.as_mut_ptr() as i64, &mut [0.0; 4]);
        let mut link = op(h, EE_UNITS_LF_POINT_MODEL, point) as u64;
        while link != 0 {
            let mut step = [0.0f32; 16];
            h.lf(EE_UNITS_LF_INTERNAL_TRANSFORM, link, 0, frame as i64, step.as_mut_ptr() as i64, &mut [0.0; 4]);
            chain.push(step);
            i.chain += 1;
            link = op(h, EE_UNITS_LF_PARENT, link) as u64;
        }
    }
    let out: EeCombatLaunchOut = launch(&i, &chain);
    if h.capture() {
        let mut r = Record::new(TAG_CBAP);
        r.bytes(bytes(&i));
        for step in &chain {
            step.iter().for_each(|&v| r.f32(v));
        }
        r.bytes(bytes(&out));
        h.commit(&r);
    }
    (out.result != 0, out.point)
}

/// `EEFOLocationScreenEnsureContinentAndPathOverlay::Screen(tile)`: the tile is on the map (when asked) and on the
/// screen's continent and path area (when set).
pub fn location_screen<H: LfHost + ?Sized>(h: &mut H, screen: u64, x: c_long, y: c_long) -> bool {
    let at = |h: &mut H, code: i32, kind: u64| h.lf(code, 0, kind, x as i64, y as i64, &mut [0.0; 4]);
    if op(h, EE_UNITS_LF_SCREEN_ON_MAP, screen) != 0 && at(h, EE_UNITS_LF_TILE_AT_LOCATION, 0) == 0 {
        return false;
    }
    let on_grid = |h: &mut H| {
        (x as c_ulong) < op(h, EE_UNITS_LF_MAP_WIDTH, 0) as c_ulong
            && (y as c_ulong) < op(h, EE_UNITS_LF_MAP_HEIGHT, 0) as c_ulong
    };
    let continent = op(h, EE_UNITS_LF_SCREEN_CONTINENT, screen) as u16;
    if continent != 0 {
        if !on_grid(h) {
            return false;
        }
        if at(h, EE_UNITS_LF_CONTINENT_ID, 0) as u16 != continent {
            return false;
        }
    }
    let area = op(h, EE_UNITS_LF_SCREEN_AREA, screen) as u16;
    if area == 0 {
        return true;
    }
    if !on_grid(h) {
        return false;
    }
    let kind = if op(h, EE_UNITS_LF_SCREEN_WATER, screen) != 0 { 2 } else { 1 };
    at(h, EE_UNITS_LF_CONTINENT_OF, kind) as u16 == area
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::uu_line::game_host;

    /// `EEGetBallisticPitch` (`from`, `to` three floats; `angle` written)
    ///
    /// # Safety
    /// The pointers are the caller's live points and float.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_lf_ballistic_pitch(
        unit: u64,
        stats: u64,
        from: *const [f32; 3],
        to: *const [f32; 3],
        angle: *mut f32,
    ) -> u8 {
        // SAFETY: the caller's contract
        let (reached, a) = unsafe { ballistic_pitch(&mut game_host(), unit, stats, *from, *to) };
        // SAFETY: as above
        unsafe { *angle = a };
        u8::from(reached)
    }

    /// `EECanHeal`
    #[no_mangle]
    pub extern "C" fn ee_units_lf_can_heal(unit: u64, target: u64) -> u8 {
        u8::from(can_heal_target(&mut game_host(), unit, target))
    }

    /// `EEApproachAttachPoint` (`point` three floats written)
    ///
    /// # Safety
    /// `point` holds three floats.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_lf_attach_point(unit: u64, point: *mut [f32; 3]) -> u8 {
        let (found, p) = approach_attach_point(&mut game_host(), unit);
        // SAFETY: the caller's contract
        unsafe { *point = p };
        u8::from(found)
    }

    /// `EEFOLocationScreenEnsureContinentAndPathOverlay::Screen`
    #[no_mangle]
    pub extern "C" fn ee_units_lf_location_screen(screen: u64, x: c_long, y: c_long) -> u8 {
        u8::from(location_screen(&mut game_host(), screen, x, y))
    }
}

#[cfg(test)]
#[path = "uu_fill_tests.rs"]
mod tests;
