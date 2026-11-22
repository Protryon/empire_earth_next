//! More of `EEUnit`'s bodies over its C++ statements (the units' lane, round 8; were EEUnit.cpp): `Remove`,
//! `CopyFrom`, both `Init`s, `ResetEEUnit`, `LoadMedia`, `RegrowForest` and `EEComplexUnit::SetLastAttacker`. Every
//! C++ statement of the originals is one operation of [`EumHost::eum`] (`ee_units_eum_host`, EEUnit.cpp; from 200),
//! asked in the original's order.
//!
//! Determinism: `CopyFrom`'s health fraction is the original's `float` expression and `UMFloatToLong` (`lrintf`);
//! `RegrowForest`'s draws are the host's `GetRandom`s in the original's order, the renewal time a wrapping `long` sum.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use ee_base::math::um::lrintf;

// ---- the statements (`op`; `a` an object, `b`, `c` values)
/// `mUnitClass->UnitRemoved()`
pub const EE_UNITS_EUM_UNIT_REMOVED: i32 = 200;
/// `UShutdownInProgress()`
pub const EE_UNITS_EUM_SHUTDOWN: i32 = 202;
/// `gWorld.mClearing`
pub const EE_UNITS_EUM_CLEARING: i32 = 203;
/// `gDataPreLoading->PostUnloadUnitClass(mPlayer->mPlayerID, mUnitClass->mUnitType)`
pub const EE_UNITS_EUM_POST_UNLOAD: i32 = 204;
/// `mPlayer->UnloadUnitClassMedia(mUnitClass->mUnitType)`
pub const EE_UNITS_EUM_UNLOAD_MEDIA: i32 = 205;
/// `mHitPoints = 0`
pub const EE_UNITS_EUM_ZERO_HIT_POINTS: i32 = 206;
/// `mComplexUnit != NULL`
pub const EE_UNITS_EUM_HAS_COMPLEX: i32 = 207;
/// `gWorld.DeactivateUnit(mComplexUnit)`
pub const EE_UNITS_EUM_DEACTIVATE: i32 = 208;
/// `RTSUnit::Invalidate()`
pub const EE_UNITS_EUM_RTS_INVALIDATE: i32 = 209;
/// `dynamic_cast<EEUnit*>(` unit `a)`
pub const EE_UNITS_EUM_AS_UNIT: i32 = 210;
/// `mResourceType = ` unit `a`'s
pub const EE_UNITS_EUM_COPY_RESOURCE_TYPE: i32 = 211;
/// unit `a`'s player's upgrade of its class's `mMaxHitPoints`
pub const EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS: i32 = 212;
/// unit `a`'s `GetHitPoints()`
pub const EE_UNITS_EUM_HIT_POINTS_OF: i32 = 213;
/// `mHitPoints = b`
pub const EE_UNITS_EUM_SET_HIT_POINTS_FIELD: i32 = 214;
/// `mResourceAmount = ` unit `a`'s
pub const EE_UNITS_EUM_COPY_RESOURCE_AMOUNT: i32 = 215;
/// `RTSUnit::CopyFrom(` unit `a)`
pub const EE_UNITS_EUM_RTS_COPY_FROM: i32 = 216;
/// `Initialize(b)`
pub const EE_UNITS_EUM_INITIALIZE: i32 = 217;
/// `mReferenceCount`
pub const EE_UNITS_EUM_REFERENCE_COUNT: i32 = 219;
/// `mReferenceCount = 1`
pub const EE_UNITS_EUM_REFERENCE_ONE: i32 = 220;
/// `mPlayer = ` player `a`
pub const EE_UNITS_EUM_SET_PLAYER: i32 = 221;
/// `mUnitClass = ` class `a`
pub const EE_UNITS_EUM_SET_CLASS: i32 = 222;
/// `mStats = mPlayer->mUnitClassUpgrades[mUnitClass->mUnitType]`
pub const EE_UNITS_EUM_SET_STATS: i32 = 223;
/// `MoveTo(b, c, 0)`
pub const EE_UNITS_EUM_MOVE_TO_TILE: i32 = 224;
/// `mComplexUnit = static_cast<EEComplexUnit*>(this)`
pub const EE_UNITS_EUM_SET_COMPLEX_SELF: i32 = 225;
/// `mResourceAmount = 0`
pub const EE_UNITS_EUM_ZERO_RESOURCE_AMOUNT: i32 = 226;
/// `mResourceType = kResourceNone`
pub const EE_UNITS_EUM_RESOURCE_TYPE_NONE: i32 = 227;
/// `mUnitClass = NULL`
pub const EE_UNITS_EUM_CLEAR_CLASS: i32 = 228;
/// `mComplexUnit = NULL`
pub const EE_UNITS_EUM_CLEAR_COMPLEX: i32 = 229;
/// `mHiddenFlags = 0`
pub const EE_UNITS_EUM_CLEAR_HIDDEN: i32 = 230;
/// `mStats = NULL`
pub const EE_UNITS_EUM_CLEAR_STATS: i32 = 231;
/// `EEUnit::IsCarbonRock(mUnitClass->mUnitType)`
pub const EE_UNITS_EUM_CARBON_ROCK: i32 = 232;
/// `gContinentManager.GetForestAt(this)`
pub const EE_UNITS_EUM_FOREST: i32 = 233;
/// forest `a`'s `GetNextRenewal() <= gPhysics->mGameTime`
pub const EE_UNITS_EUM_FOREST_DUE: i32 = 236;
/// forest `a`'s `GetNumTiles()`
pub const EE_UNITS_EUM_FOREST_TILES: i32 = 237;
/// unit `a`'s `mUnitClass->mCategory`
pub const EE_UNITS_EUM_CATEGORY_OF: i32 = 238;
/// `gPhysics->GetRandom((long)b, (long)c)`
pub const EE_UNITS_EUM_RANDOM: i32 = 239;
/// forest `a`'s `RegrowTrees(U2DPoint<long>(mTileX, mTileY), b, c)`
pub const EE_UNITS_EUM_REGROW: i32 = 240;
/// forest `a`'s `SetNextRenewal(b)`
pub const EE_UNITS_EUM_SET_NEXT_RENEWAL: i32 = 241;
/// `gPhysics->mGameTime`
pub const EE_UNITS_EUM_GAME_TIME: i32 = 235;
/// `mPlayer->SetUnitClassMedia(mUnitClass->mUnitType, 0x7f)`
pub const EE_UNITS_EUM_SET_CLASS_MEDIA: i32 = 242;
/// `gDataPreLoading->PostLoadUnitClass(mPlayer->mPlayerID, mUnitClass->mUnitType)`
pub const EE_UNITS_EUM_POST_LOAD: i32 = 243;
/// the complex unit's `mLastAttacker`
pub const EE_UNITS_EUM_LAST_ATTACKER: i32 = 244;
/// the complex unit's `mLastAttacker->Dereference()`
pub const EE_UNITS_EUM_DEREFERENCE_LAST_ATTACKER: i32 = 245;
/// the complex unit's `mLastAttacker = ` complex unit `a`
pub const EE_UNITS_EUM_SET_LAST_ATTACKER: i32 = 246;
/// complex unit `a`'s `Reference()`
pub const EE_UNITS_EUM_REFERENCE_OF: i32 = 247;

// ---- the bodies (`ee_units_eu_more`'s `what`)
pub const EE_UNITS_EUM_DO_REMOVE: i32 = 1;
pub const EE_UNITS_EUM_DO_COPY_FROM: i32 = 2;
pub const EE_UNITS_EUM_DO_INIT_AT: i32 = 3;
pub const EE_UNITS_EUM_DO_INIT_COMPLEX: i32 = 4;
pub const EE_UNITS_EUM_DO_RESET: i32 = 5;
pub const EE_UNITS_EUM_DO_LOAD_MEDIA: i32 = 6;
pub const EE_UNITS_EUM_DO_REGROW_FOREST: i32 = 7;
pub const EE_UNITS_EUM_DO_SET_LAST_ATTACKER: i32 = 8;

/// The C++ statements of the bodies.
pub trait EumHost {
    /// one statement (`EE_UNITS_EUM_*`) on the unit `unit`
    fn eum(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64;
}

const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;

struct S<'h, H: EumHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
}

impl<H: EumHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.eum(op, self.unit, 0, 0, 0)
    }
    fn qa(&mut self, op: i32, a: u64) -> i64 {
        self.h.eum(op, self.unit, a, 0, 0)
    }
    fn qabc(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.h.eum(op, self.unit, a, b, c)
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
}

/// `Remove(in_background)`: the class media unloaded (queued to the preloader if asked), no hit points, deactivated.
pub fn remove<H: EumHost + ?Sized>(h: &mut H, unit: u64, background: bool) {
    let s = &mut S { h, unit };
    s.q(EE_UNITS_EUM_UNIT_REMOVED);
    if background && !s.yes(EE_UNITS_EUM_SHUTDOWN) && !s.yes(EE_UNITS_EUM_CLEARING) {
        s.q(EE_UNITS_EUM_POST_UNLOAD);
    } else {
        s.q(EE_UNITS_EUM_UNLOAD_MEDIA);
    }
    s.q(EE_UNITS_EUM_ZERO_HIT_POINTS);
    if s.yes(EE_UNITS_EUM_HAS_COMPLEX) {
        s.q(EE_UNITS_EUM_DEACTIVATE);
    }
    s.q(EE_UNITS_EUM_RTS_INVALIDATE);
}

/// `CopyFrom(other)`: the other's resource type and amount, its health fraction scaled to this unit's maximum.
pub fn copy_from<H: EumHost + ?Sized>(h: &mut H, unit: u64, other: u64) {
    let s = &mut S { h, unit };
    let other = s.qa(EE_UNITS_EUM_AS_UNIT, other) as u64;
    s.qa(EE_UNITS_EUM_COPY_RESOURCE_TYPE, other);
    let other_max = s.qa(EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS, other) as c_long;
    let max = s.qa(EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS, unit) as c_long;
    let hit_points = s.qa(EE_UNITS_EUM_HIT_POINTS_OF, other) as c_long;
    let fraction = hit_points as f32 / other_max as f32;
    let value = lrintf(max as f32 * fraction);
    s.qabc(EE_UNITS_EUM_SET_HIT_POINTS_FIELD, 0, i64::from(value), 0);
    s.qa(EE_UNITS_EUM_COPY_RESOURCE_AMOUNT, other);
    s.qa(EE_UNITS_EUM_RTS_COPY_FROM, other);
}

/// `ResetEEUnit()`: no class, complex unit, resources, hit points, hidden flags or stats.
pub fn reset<H: EumHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    for op in [
        EE_UNITS_EUM_CLEAR_CLASS,
        EE_UNITS_EUM_CLEAR_COMPLEX,
        EE_UNITS_EUM_ZERO_RESOURCE_AMOUNT,
        EE_UNITS_EUM_RESOURCE_TYPE_NONE,
        EE_UNITS_EUM_ZERO_HIT_POINTS,
        EE_UNITS_EUM_CLEAR_HIDDEN,
        EE_UNITS_EUM_CLEAR_STATS,
    ] {
        s.q(op);
    }
}

/// Both `Init`s' start: `Initialize(linked)`, the reset, a first reference, the player and class (and stats).
fn init_start<H: EumHost + ?Sized>(h: &mut H, unit: u64, linked: i64, class: u64, player: u64, complex: bool) {
    h.eum(EE_UNITS_EUM_INITIALIZE, unit, 0, linked, 0);
    reset(h, unit);
    let s = &mut S { h, unit };
    if s.q(EE_UNITS_EUM_REFERENCE_COUNT) == 0 {
        s.q(EE_UNITS_EUM_REFERENCE_ONE);
    }
    s.qa(EE_UNITS_EUM_SET_PLAYER, player);
    s.qa(EE_UNITS_EUM_SET_CLASS, class);
    if complex {
        s.q(EE_UNITS_EUM_SET_COMPLEX_SELF);
    }
    if class != 0 {
        s.q(EE_UNITS_EUM_SET_STATS);
    }
}

/// `Init(class, x, y, player)`: a simple unit of the class for the player, placed at tile (x, y).
pub fn init_at<H: EumHost + ?Sized>(h: &mut H, unit: u64, class: u64, x: i64, y: i64, player: u64) {
    init_start(h, unit, 0, class, player, false);
    h.eum(EE_UNITS_EUM_MOVE_TO_TILE, unit, 0, x, y);
}

/// `Init(class, player)`: the unit part of a complex unit, with no resources or hit points yet.
pub fn init_complex<H: EumHost + ?Sized>(h: &mut H, unit: u64, class: u64, player: u64) {
    init_start(h, unit, 1, class, player, true);
    let s = &mut S { h, unit };
    s.q(EE_UNITS_EUM_ZERO_RESOURCE_AMOUNT);
    s.q(EE_UNITS_EUM_RESOURCE_TYPE_NONE);
    s.q(EE_UNITS_EUM_ZERO_HIT_POINTS);
}

/// `LoadMedia(foundation)`: the class media at once, or for a foundation queued to the preloader.
pub fn load_media<H: EumHost + ?Sized>(h: &mut H, unit: u64, foundation: bool) {
    h.eum(if foundation { EE_UNITS_EUM_POST_LOAD } else { EE_UNITS_EUM_SET_CLASS_MEDIA }, unit, 0, 0, 0);
}

/// `RegrowForest(by)`: a due forest of more than one tile regrows trees round the unit (a citizen: a random radius
/// 0..2 and count 1..2, else 3 and 3) and its next renewal is drawn 125..275 s on.
pub fn regrow_forest<H: EumHost + ?Sized>(h: &mut H, unit: u64, by: u64) {
    let s = &mut S { h, unit };
    if s.yes(EE_UNITS_EUM_CARBON_ROCK) {
        return;
    }
    let forest = s.q(EE_UNITS_EUM_FOREST) as u64;
    let mut count: i64 = 3;
    let mut radius: i64 = 3;
    if forest == 0 || s.qa(EE_UNITS_EUM_FOREST_DUE, forest) == 0 || s.qa(EE_UNITS_EUM_FOREST_TILES, forest) <= 1 {
        return;
    }
    if by != 0 && matches!(s.qa(EE_UNITS_EUM_CATEGORY_OF, by), CATEGORY_LAND_CITIZEN | CATEGORY_NAVAL_CITIZEN) {
        radius = s.qabc(EE_UNITS_EUM_RANDOM, 0, 0, 2);
        count = s.qabc(EE_UNITS_EUM_RANDOM, 0, 1, 2);
    }
    s.qabc(EE_UNITS_EUM_REGROW, forest, radius, count);
    let time = s.q(EE_UNITS_EUM_GAME_TIME) as c_long;
    let next = time.wrapping_add(s.qabc(EE_UNITS_EUM_RANDOM, 0, 125_000, 275_000) as c_long);
    s.qabc(EE_UNITS_EUM_SET_NEXT_RENEWAL, forest, i64::from(next), 0);
}

/// `EEComplexUnit::SetLastAttacker(attacker)`: the attacker referenced, the last one released.
pub fn set_last_attacker<H: EumHost + ?Sized>(h: &mut H, unit: u64, attacker: u64) {
    let s = &mut S { h, unit };
    let last = s.q(EE_UNITS_EUM_LAST_ATTACKER) as u64;
    if last == attacker {
        return;
    }
    if last != 0 {
        s.q(EE_UNITS_EUM_DEREFERENCE_LAST_ATTACKER);
    }
    s.qa(EE_UNITS_EUM_SET_LAST_ATTACKER, attacker);
    s.qa(EE_UNITS_EUM_REFERENCE_OF, attacker);
}

/// Runs body `what` on `unit`.
pub fn run<H: EumHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: u64, b: i64, c: i64) {
    match what {
        EE_UNITS_EUM_DO_REMOVE => remove(h, unit, b != 0),
        EE_UNITS_EUM_DO_COPY_FROM => copy_from(h, unit, a),
        EE_UNITS_EUM_DO_INIT_AT => {
            // (the player in `c`)
            let (x, y) = (b & 0xffff_ffff, b >> 32);
            init_at(h, unit, a, i64::from(x as u32 as i32), i64::from(y as i32), c as u64)
        }
        EE_UNITS_EUM_DO_INIT_COMPLEX => init_complex(h, unit, a, c as u64),
        EE_UNITS_EUM_DO_RESET => reset(h, unit),
        EE_UNITS_EUM_DO_LOAD_MEDIA => load_media(h, unit, b != 0),
        EE_UNITS_EUM_DO_REGROW_FOREST => regrow_forest(h, unit, a),
        EE_UNITS_EUM_DO_SET_LAST_ATTACKER => set_last_attacker(h, unit, a),
        _ => {}
    }
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_eum_host(op: i32, unit: *mut c_void, a: u64, b: i64, c: i64) -> i64;
    }

    struct Game;

    impl EumHost for Game {
        fn eum(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
            // SAFETY: the body runs inside the C++ member that handed over its unit
            unsafe { ee_units_eum_host(op, unit as usize as *mut c_void, a, b, c) }
        }
    }

    /// `EEUnit`'s body `what` (`EE_UNITS_EUM_DO_*`) on `unit`; `a` an object, `b`, `c` values.
    ///
    /// # Safety
    /// `unit` is a live `EEUnit` (or the unit being made), on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_eu_more(what: i32, unit: *mut c_void, a: *mut c_void, b: i64, c: i64) {
        run(&mut Game, what, unit as usize as u64, a as usize as u64, b, c)
    }
}

#[cfg(test)]
#[path = "eu_more_tests.rs"]
mod tests;
