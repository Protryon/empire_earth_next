//! The computer player's block of a saved game (`EEComputerPlayer::Load` 0x2fd50 and `Save` 0x3d677) as one
//! sequence each over the record ([`EeCpFields`]) and a host ([`EeUnitsPlayerSaveFn`]): the file's primitives
//! (`EESavePrimitive`'s operations, passed through) and the class's C++ parts by operation (the combat manager's and
//! the placement's blocks, the resource centres, the favourite types' history).
//!
//! Load and Save share their field order: the `seg_*` lists below are used by both. Between them sit the parts the
//! two do differently: the enable-time flag (saved as false: a loaded game schedules its next manager-enable check
//! afresh), the next attack and expansion times (saved relative to the game clock; `RestoreManagers` rebases them),
//! the centres and the history. A retail file (to 0.250) holds the deleted C++ managers' blocks, stepped over
//! ([`legacy_load`]); a port save of 0.251-0.280 holds them with the port's additions and does not load. The
//! original's 0.229-0.231 block (its seventh) lacks the four fields `mLF14`.. (read from 0.232), the space dock pair
//! is read from 0.236 and the favourite space type from 0.237.
//!
//! Kept quirks: a field read leaves what the file left when it fails (the host's read-into-the-old-value); the
//! history's first value of each entry is read even after a failed read (its second is not); the running state
//! (0.256 on) is read into temporaries the caller applies at the first tick (`EEAfterLoad`), and `favorites_ok`
//! tells the caller whether the original would have registered the favourite types' restore before those reads.
//!
//! The record is read and written through its raw pointer field by field, never by a reference held across a host
//! call.

#![allow(non_snake_case)]
// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts to and from the 32-bit file words are
// needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;
use std::os::raw::{c_long, c_ulong};

use super::ffi::EE_CP_LEGACY_LAST_VERSION;
use super::fields::EeCpFields;
use super::file::{SaveFile, FAILED, OK};
use super::legacy_load;
use crate::units::player_save::{EeUnitsPlayerSaveFn, EeUnitsPlayerSaveIo};

// ---- the host's operations beyond the file's primitives
/// `mCombatManager->Load(major, minor, file)`: its result.
pub const EE_CP_SAVE_COMBAT_LOAD: i32 = 400;
/// `mPlacement->Load(file)` (the town centre tile): its result.
pub const EE_CP_SAVE_PLACEMENT_LOAD: i32 = 401;
/// The placement's town centre tile's x (`index` 0) or y (1) into `io.i`.
pub const EE_CP_SAVE_TOWN_TILE: i32 = 402;
/// Sets the placement's town centre tile's x (`index` 0) or y (1) to `io.i`.
pub const EE_CP_SAVE_SET_TOWN_TILE: i32 = 403;
/// A new resource centre (no unit) loaded and pushed, loaded or not: the load's result.
pub const EE_CP_SAVE_CENTER_LOAD: i32 = 404;
/// `mFavoriteTypeHistory.resize(io.i, make_pair(0, 0))`.
pub const EE_CP_SAVE_HISTORY_RESIZE: i32 = 405;
/// `mFavoriteTypeHistory[index].first` (`HISTORY_FIRST`) or `.second` into `io.i`; the `SET_` ones store `io.i`.
pub const EE_CP_SAVE_HISTORY_FIRST: i32 = 406;
pub const EE_CP_SAVE_HISTORY_SECOND: i32 = 407;
pub const EE_CP_SAVE_HISTORY_SET_FIRST: i32 = 408;
pub const EE_CP_SAVE_HISTORY_SET_SECOND: i32 = 409;
/// `mCombatManager->Save(major, minor, file)`: its result.
pub const EE_CP_SAVE_COMBAT_SAVE: i32 = 410;
/// `mPlacement->Save(file)`: its result.
pub const EE_CP_SAVE_PLACEMENT_SAVE: i32 = 411;
/// `mResourceCenters.size()` into `io.i`.
pub const EE_CP_SAVE_CENTERS: i32 = 412;
/// `mResourceCenters[index]->Save(major, minor, file)`: its result.
pub const EE_CP_SAVE_CENTER_SAVE: i32 = 413;
/// `mFavoriteTypeHistory.size()` into `io.i`.
pub const EE_CP_SAVE_HISTORY_COUNT: i32 = 414;

/// What a load read past the fields: the running state of a 0.256-or-later save (the caller puts it back at the
/// first tick) and whether the fields read without an error before it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCpLoaded {
    /// the result was kErrorOK after the favourite space type (the original registered the favourites' restore
    /// there, for a 0.254-or-later save)
    pub favorites_ok: bool,
    /// the running state below was read
    pub running: bool,
    pub population_cap: c_long,
    pub enable_time_set: bool,
    pub regroup_pending: bool,
    pub next_population_time: c_long,
    pub last_damaged_time: c_long,
}

/// The save versions' predicates (ee/EESaveGameFiles.h).
pub fn has_live_state(major: u32, minor: u32) -> bool {
    major > 0 || minor >= 254
}
pub fn has_running_state(major: u32, minor: u32) -> bool {
    major > 0 || minor >= 256
}
pub fn has_lua_only_computers(major: u32, minor: u32) -> bool {
    major > 0 || minor >= 281
}

/// A field's `FSFile` overload: 4-byte `long`/`unsigned long`, `float`, `bool`.
pub trait Prim: Copy {
    fn load(f: &mut SaveFile, old: Self) -> (i32, Self);
    fn save(f: &mut SaveFile, v: Self) -> i32;
}
impl Prim for c_long {
    fn load(f: &mut SaveFile, old: Self) -> (i32, Self) {
        let (r, v) = f.read_long(old as i32);
        (r, v as c_long)
    }
    fn save(f: &mut SaveFile, v: Self) -> i32 {
        f.write_long(v as i32)
    }
}
impl Prim for c_ulong {
    fn load(f: &mut SaveFile, old: Self) -> (i32, Self) {
        let (r, v) = f.read_ulong(old as u32);
        (r, v as c_ulong)
    }
    fn save(f: &mut SaveFile, v: Self) -> i32 {
        f.write_ulong(v as u32)
    }
}
impl Prim for f32 {
    fn load(f: &mut SaveFile, old: Self) -> (i32, Self) {
        f.read_float(old)
    }
    fn save(f: &mut SaveFile, v: Self) -> i32 {
        f.write_float(v)
    }
}
impl Prim for bool {
    fn load(f: &mut SaveFile, old: Self) -> (i32, Self) {
        f.read_bool(old)
    }
    fn save(f: &mut SaveFile, v: Self) -> i32 {
        f.write_bool(v)
    }
}

/// `if (theResult == kErrorOK) theResult = inFile.Read(field)` per field: the value read (or left) stored back.
macro_rules! load_fields {
    ($f:expr, $r:expr, $p:expr; $($n:ident),* $(,)?) => {
        $(
            if $r == OK {
                // SAFETY: the caller's live record; no reference is kept across the host call
                let field = unsafe { &raw mut (*$p).$n };
                // SAFETY: as above
                let (res, v) = Prim::load($f, unsafe { field.read() });
                // SAFETY: as above
                unsafe { field.write(v) };
                $r = res;
            }
        )*
    };
}

/// `if (theResult == kErrorOK) theResult = outFile.Write(field)` per field.
macro_rules! save_fields {
    ($f:expr, $r:expr, $p:expr; $($n:ident),* $(,)?) => {
        $(
            if $r == OK {
                // SAFETY: the caller's live record, read by copy
                $r = Prim::save($f, unsafe { (*$p).$n });
            }
        )*
    };
}

// The field order the two share (the original's `Load` and `Save`, in their order)
macro_rules! seg_a {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mLBD4, mFavoriteLandType, mNextEnableTime,
        )
    };
}
macro_rules! seg_b {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mBuildingDamaged, mCitizenDamaged, mIsStarted,
        )
    };
}
macro_rules! seg_c {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mActive,
        )
    };
}
macro_rules! seg_d {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mUsesFishing, mUnitCostMedium, mCarrierMaxPerGroup, mCarrierPopPercentage, mCarrierChance,
            mNumAirExplorers, mComputerAggression, mAttackEnemyChance, mAttackToClaimResourceChance,
            mAttackForRevengeChance, mAttackGroupSizeMax, mAttackGroupSizeMin, mAttackFrequencyPerEpoch,
            mAttackFrequencyCeiling, mAttackFrequencyFloor, mAttackFrequencyMax, mAttackFrequencyMin,
            mInitialAttackDelayPerEpoch, mAttackDelayCeiling, mAttackDelayFloor, mInitialAttackDelayMax,
            mInitialAttackDelayMin, mSpecializedUnitPercentage, mTownGuardPercentage, mLD78, mLD7C,
            mCitizenPercentage, mDefenseGroupSizeMax, mDefenseGroupSizeMin, mEpochFrequencyMax,
            mEpochFrequencyMin, mGuardContinentPercentage, mGuardChokepointAirPercentage,
            mGuardChokepointPercentage, mGuardHighGroundPercentage, mFC38, mFC3C, mFC40, mFC44, mFC48, mFC4C,
            mFC50, mFC54, mFC58, mGuardTownPercentage, mGuardTownLandPercentage, mGuardWaterwayPercentage,
            mHeroMaxPerGroup, mHeroPopPercentage, mHeroChance, mNumImprovedUnitTypes, mNumImprovedAttributes,
            mReplenishDelayMax, mReplenishDelayMin, mNumLandExplorers, mNumChokePointsToGuard, mAirportMax,
            mArcheryRangeMax, mBarracksMax, mDockMax, mGranaryMax, mHospitalMax, mCyberFactoryMax, mCyberLabMax,
            mNavalYardMax, mUniversityMax, mSiegeFactoryMax, mStableMax, mTempleMax, mDefensiveUnitCap,
            mMaxAttackResponseRange, mMilitaryPopPercentage, mAidThreatThreshold, mPriestMaxPerGroup,
            mPriestPopPercentage, mPriestChance, mProphetMaxPerGroup, mProphetPopPercentage, mProphetChance,
            mResearchFrequency, mDropOffDistance, mNumSeaExplorers, mMaxEnabledManagers, mManagerAttackWait,
            mManagerEpochAdvanceWait, mEnableStockProduction, mEnableStockResource, mEnableStockCivilization,
            mEnableStockExploration, mEnableStockDefense, mEnableStockBuild, mLE80, mEnableStockResearch,
            mEnableStockCombat, mEnableInterval, mMilitaryCountWeight, mWorkerCountWeight, mUnitsPerAirport,
            mUnitsPerArcheryRange, mUnitsPerBarracks, mUnitsPerDock, mUnitsPerGranary, mUnitsPerHospital,
            mUnitsPerCyberFactory, mUnitsPerCyberLab, mUnitsPerNavalYard, mUnitsPerUniversity,
            mUnitsPerSiegeFactory, mUnitsPerStable, mUnitsPerTemple, mCanBuild, mIsCheating, mCanDefend, mBF4E,
            mCanExplore, mCanGather, mIsCitizenPercentageSet, mCanAttack, mIgnoreOthers, mDifficulty,
            mAttackFrequencyModifierEasy, mAttackFrequencyModifierHard, mAttackDelayModifierEasy,
            mAttackDelayModifierHard, mAttackGroupModifierEasy, mAttackGroupModifierHard,
            mUnitCostReductionEasy, mUnitCostReductionHard, mEpochFrequencyModifierEasy,
            mEpochFrequencyModifierHard, mPopLimitAdjustmentEasy, mPopLimitAdjustmentHard,
            mResearchFrequencyModifierEasy, mResearchFrequencyModifierHard,
        )
    };
}
macro_rules! seg_e {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mUnitsPerTankFactory, mTankFactoryMax, mMinMilitaryBuildingsEasy, mMinMilitaryBuildingsEasyDM,
            mMinMilitaryBuildingsMedium, mMinMilitaryBuildingsMediumDM, mMinMilitaryBuildingsHard,
            mMinMilitaryBuildingsHardDM, mRPSSelectionNonIslands, mRPSSelectionIslands, mSpecializedUnitChance,
            mDMEarlyAttackTime, mDMEarlyAttackScale, mNavalUnitWeight, mAirUnitWeight,
        )
    };
}
macro_rules! seg_f {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mMaxFlameThrowers, mAttackStarted, mDefensiveUnitUpdateEasy, mDefensiveUnitUpdateMedium,
            mDefensiveUnitUpdateHard, mDMDefensiveUnitUpdateEasy, mDMDefensiveUnitUpdateMedium,
            mDMDefensiveUnitUpdateHard, mTownGuardUpdateEasy, mTownGuardUpdateMedium, mTownGuardUpdateHard,
            mDMTownGuardUpdateEasy, mDMTownGuardUpdateMedium, mDMTownGuardUpdateHard, mPopLimitAdjustmentMedium,
            mAttackGroupModifierMedium, mAssistAlliesDelayEasy, mAssistAlliesDelayMedium,
            mAssistAlliesDelayHard, mShipsToMaintainDMEasy, mShipsToMaintainDMMedium, mShipsToMaintainDMHard,
            mShipsToMaintainEasy, mShipsToMaintainMedium, mShipsToMaintainHard, mShipMaintenanceTimer,
            mSiegeWeaponPercentage, mBonzaiAttackLevel, mFavoriteSeaType, mFavoriteAirType,
            mNextIslandExpansionTime, mExpansionTimerEasy, mExpansionTimerMedium, mExpansionTimerHard,
        )
    };
}
macro_rules! seg_g {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mLF14, mLF1C, mLF18, mLF20,
        )
    };
}
macro_rules! seg_h {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mUnitsPerSpaceDock, mSpaceDockMax,
        )
    };
}
macro_rules! seg_i {
    ($m:ident, $($a:tt)*) => {
        $m!($($a)*;
            mFavoriteSpaceType,
        )
    };
}
fn op(f: &mut SaveFile, op: i32, index: u32) -> (i32, i64) {
    let mut io = EeUnitsPlayerSaveIo::default();
    let r = f.call_at(op, index, &mut io);
    (r, io.i)
}

fn set(f: &mut SaveFile, op: i32, index: u32, v: i64) {
    let mut io = EeUnitsPlayerSaveIo { i: v, ..Default::default() };
    f.call_at(op, index, &mut io);
}

/// `EEComputerPlayer::Load` after `EEPlayer::Load` (whose result is `r`): the refusals, the managers' blocks (or a
/// retail file's legacy blocks), the fields, the centres, the history; `out` gets the running state.
///
/// # Safety
/// `p` is the computer player's live record; `host` serves the file's primitives and the operations above on `ctx`.
pub unsafe fn load(
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    p: *mut EeCpFields,
    major: u32,
    minor: u32,
    r: i32,
    out: &mut EeCpLoaded,
) -> i32 {
    // SAFETY: the caller's contract
    let f = &mut unsafe { SaveFile::new(ctx, host) };
    let mut r = r;
    *out = EeCpLoaded::default();
    // only the original's last two blocks load (from 0.229: the Learning Campaign's scenarios are 0.231 files)
    if r == OK && major == 0 && minor < 0xe5 {
        return FAILED;
    }
    let legacy = major == 0 && minor <= EE_CP_LEGACY_LAST_VERSION;
    if r == OK && !legacy && !has_lua_only_computers(major, minor) {
        return FAILED;
    }
    if r == OK {
        r = if legacy { legacy_load::skip_combat_manager(f, major, minor) } else { op(f, EE_CP_SAVE_COMBAT_LOAD, 0).0 };
    }
    if r == OK && legacy {
        let mut tile = [op(f, EE_CP_SAVE_TOWN_TILE, 0).1 as i32, op(f, EE_CP_SAVE_TOWN_TILE, 1).1 as i32];
        r = legacy_load::skip_civilization_manager(f, major, minor, &mut tile);
        set(f, EE_CP_SAVE_SET_TOWN_TILE, 0, i64::from(tile[0]));
        set(f, EE_CP_SAVE_SET_TOWN_TILE, 1, i64::from(tile[1]));
        if r == OK {
            r = legacy_load::skip_exploration_manager(f, major, minor);
        }
    } else if r == OK {
        r = op(f, EE_CP_SAVE_PLACEMENT_LOAD, 0).0; // (stage 6: the managers' block is the town centre tile)
    }
    seg_a!(load_fields, f, r, p);
    load_fields!(f, r, p; mIsEnableTimeSet);
    seg_b!(load_fields, f, r, p);
    load_fields!(f, r, p; mNextAttackTime);
    seg_c!(load_fields, f, r, p);
    let mut count = 0u32;
    if r == OK {
        (r, count) = f.read_ulong(count);
        if r == OK {
            // a resource center's read error is reported (it used to be discarded)
            let mut i = 0;
            while i < count && r == OK {
                r = op(f, EE_CP_SAVE_CENTER_LOAD, 0).0;
                i += 1;
            }
        }
    }
    seg_d!(load_fields, f, r, p);
    load_fields!(f, r, p; mNextExpansionTime);
    seg_e!(load_fields, f, r, p);
    if r == OK {
        (r, count) = f.read_ulong(count);
        if r == OK {
            set(f, EE_CP_SAVE_HISTORY_RESIZE, 0, i64::from(count));
            for i in 0..count {
                let old = op(f, EE_CP_SAVE_HISTORY_FIRST, i).1 as u32;
                let (rf, first) = f.read_ulong(old);
                set(f, EE_CP_SAVE_HISTORY_SET_FIRST, i, i64::from(first));
                r = rf;
                if r == OK {
                    let old = op(f, EE_CP_SAVE_HISTORY_SECOND, i).1 as i32;
                    let (rs, second) = f.read_long(old);
                    set(f, EE_CP_SAVE_HISTORY_SET_SECOND, i, i64::from(second));
                    r = rs;
                }
            }
        }
    }
    seg_f!(load_fields, f, r, p);
    // (0.229-0.231, the original's seventh block: the same fields up to here, without these four)
    if major > 0 || minor >= 0xe8 {
        seg_g!(load_fields, f, r, p);
    }
    if major > 0 || minor >= 0xec {
        seg_h!(load_fields, f, r, p);
    }
    if major > 0 || minor >= 0xed {
        seg_i!(load_fields, f, r, p);
    }
    out.favorites_ok = r == OK;
    // port (0.256): the running values the original saved otherwise or not at all, put back at the first tick
    if r == OK && has_running_state(major, minor) {
        let mut v = EeCpLoaded { favorites_ok: true, ..Default::default() };
        let mut word = 0i32;
        (r, word) = f.read_long(word);
        v.population_cap = word as c_long;
        if r == OK {
            (r, v.enable_time_set) = f.read_bool(false);
        }
        if r == OK {
            (r, v.regroup_pending) = f.read_bool(false);
        }
        if r == OK {
            (r, word) = f.read_long(0);
            v.next_population_time = word as c_long;
        }
        if r == OK {
            (r, word) = f.read_long(0);
            v.last_damaged_time = word as c_long;
        }
        if r == OK {
            v.running = true;
            *out = v;
        }
    }
    r
}

/// `EEComputerPlayer::Save` after `EEPlayer::Save` (whose result is `r`): the managers' blocks, the fields (the
/// enable-time flag as false, the next attack and expansion times relative to `game_time`), the centres, the
/// history and the running state.
///
/// # Safety
/// As [`load`].
pub unsafe fn save(
    ctx: *mut c_void,
    host: EeUnitsPlayerSaveFn,
    p: *const EeCpFields,
    major: u32,
    minor: u32,
    game_time: u32,
    r: i32,
) -> i32 {
    // SAFETY: the caller's contract
    let f = &mut unsafe { SaveFile::new(ctx, host) };
    let mut r = r;
    if r == OK {
        r = op(f, EE_CP_SAVE_COMBAT_SAVE, 0).0;
    }
    if r == OK {
        r = op(f, EE_CP_SAVE_PLACEMENT_SAVE, 0).0;
    }
    seg_a!(save_fields, f, r, p);
    if r == OK {
        r = f.write_bool(false);
    }
    seg_b!(save_fields, f, r, p);
    if r == OK {
        // SAFETY: the caller's live record, read by copy
        r = f.write_ulong((unsafe { (*p).mNextAttackTime } as u32).wrapping_sub(game_time));
    }
    seg_c!(save_fields, f, r, p);
    if r == OK {
        let count = op(f, EE_CP_SAVE_CENTERS, 0).1 as u32;
        r = f.write_ulong(count);
        // a resource center's write error is reported (it used to be discarded)
        let mut i = 0;
        while i < count && r == OK {
            r = op(f, EE_CP_SAVE_CENTER_SAVE, i).0;
            i += 1;
        }
    }
    seg_d!(save_fields, f, r, p);
    if r == OK {
        // SAFETY: as above
        r = f.write_ulong((unsafe { (*p).mNextExpansionTime } as u32).wrapping_sub(game_time));
    }
    seg_e!(save_fields, f, r, p);
    if r == OK {
        let count = op(f, EE_CP_SAVE_HISTORY_COUNT, 0).1 as u32;
        r = f.write_ulong(count);
        for i in 0..count {
            if r == OK {
                let first = op(f, EE_CP_SAVE_HISTORY_FIRST, i).1 as u32;
                r = f.write_ulong(first);
            }
            if r == OK {
                let second = op(f, EE_CP_SAVE_HISTORY_SECOND, i).1 as i32;
                r = f.write_long(second);
            }
        }
    }
    seg_f!(save_fields, f, r, p);
    seg_g!(save_fields, f, r, p);
    seg_h!(save_fields, f, r, p);
    seg_i!(save_fields, f, r, p);
    // port (0.256): the running values, without changing the running game
    if r == OK && has_running_state(major, minor) {
        save_fields!(f, r, p; mPopulationCap, mIsEnableTimeSet, mRegroupPending, mNextPopulationTime, mLastDamagedTime);
    }
    r
}

#[cfg(test)]
#[path = "save_tests.rs"]
mod tests;
