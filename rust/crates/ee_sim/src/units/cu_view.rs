//! The unit host's plain reads and stores (the units' lane, round 13): the `EE_UNITS_CU_*` statements of
//! `ee_units_cu_host` (EEComplexUnit.cpp) that only read or write a member of the unit's, another unit's, a class's,
//! a class upgrade's or a player's Rust record (or a pointer member the world layout registers) are answered here,
//! before the game's host is asked ([`cu_view`], from cu_body.rs `Game::cu`). Each answer has the C++ statement's
//! type conversion: a `long` sign-extends, an `unsigned long`, a `uint32_t` or a float's bits zero-extend, a pointer
//! is its address, a `bool` 0 or 1.

use crate::units::cu_body::*;
use crate::units::cu_cargo::*;
use crate::units::cu_dump::*;
use crate::units::cu_life::*;
use crate::units::cu_misc::*;
use crate::units::cu_more::*;
use crate::units::cu_small::*;
use crate::units::cu_world::*;
use crate::units::eu_body::{EE_UNITS_EU_DO_MOVE_TO, EE_UNITS_EU_DO_SET_HIT_POINTS};
use crate::units::goals;
use crate::units::u12a_small::*;
use crate::units::uc_is::{EE_UNITS_UC_IS_AIR_BASE, EE_UNITS_UC_IS_TRANSPORT};
use crate::world::layout::{layout, EeWorldLayout};
use crate::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved, EeUnitsRtsPlayerFields};
use crate::world::stats_fields::{EeUnitsUnitClassUpgradeFields, EeUnitsUnitClassUpgradeLinks};
use crate::world::unit_class_fields::{EeUnitsUnitClassFields, EeUnitsUnitClassLinks};
use crate::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsRtsUnitFields, EeUnitsUnitFields};
use std::os::raw::{c_long, c_ulong};
use std::ptr::addr_of;

/// `kGoalBuildRepair` (an `EEUnitGoal`'s `mKind`)
const CU_VIEW_GOAL_BUILD_REPAIR: u8 = 4;
/// `kUnitCategoryAirplane`, `kUnitCategoryPlanetSpaceship`
const CU_VIEW_CATEGORY_AIRPLANE: c_ulong = 3;
const CU_VIEW_CATEGORY_PLANET_SPACESHIP: c_ulong = 34;

/// The record at `object + offset`.
fn at<T>(object: u64, offset: u64) -> *mut T {
    (object as usize + offset as usize) as *mut T
}

/// The pointer member at `object + offset`.
///
/// # Safety
/// The bytes there are a live pointer member.
unsafe fn word(object: u64, offset: u64) -> u64 {
    // SAFETY: the caller's contract
    unsafe { *at::<usize>(object, offset) as u64 }
}

/// A `bool` member read as its byte (a byte other than 0 or 1 is a C++ `true`).
macro_rules! flag {
    ($record:expr, $field:ident) => {
        *(addr_of!((*$record).$field) as *const u8) != 0
    };
}

/// A pointer as the host's answer (its address).
fn h<T>(p: *mut T) -> i64 {
    p as usize as i64
}

fn long(v: c_long) -> i64 {
    v as i64
}

fn ulong(v: c_ulong) -> i64 {
    v as u64 as i64
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

/// The low 32 bits of an `unsigned long` (the C++ `(uint32_t)` cast; a no-op where `long` is 32-bit)
#[allow(clippy::unnecessary_cast)]
fn low32(v: c_ulong) -> u32 {
    v as u32
}

fn float(b: i64) -> f32 {
    f32::from_bits(b as u32)
}

/// A unit object's records, by its handle.
#[derive(Clone, Copy)]
struct U(u64, &'static EeWorldLayout);

impl U {
    fn rts(self) -> *mut EeUnitsRtsUnitFields {
        at(self.0, self.1.rts_unit_fields)
    }
    fn uf(self) -> *mut EeUnitsUnitFields {
        at(self.0, self.1.unit_fields)
    }
    fn cx(self) -> *mut EeUnitsComplexUnitFields {
        at(self.0, self.1.complex_unit_fields)
    }
    /// # Safety
    /// A live `EEUnit` with a class.
    unsafe fn class(self) -> *mut EeUnitsUnitClassFields {
        // SAFETY: the caller's contract
        at(unsafe { (*self.uf()).mUnitClass } as u64, self.1.unit_class_fields)
    }
    /// # Safety
    /// As [`U::class`].
    unsafe fn links(self) -> *mut EeUnitsUnitClassLinks {
        // SAFETY: the caller's contract
        at(unsafe { (*self.uf()).mUnitClass } as u64, self.1.class_links)
    }
    /// # Safety
    /// A live `EEUnit` with stats.
    unsafe fn stats(self) -> *mut EeUnitsUnitClassUpgradeFields {
        // SAFETY: the caller's contract
        at(unsafe { (*self.uf()).mStats } as u64, self.1.stats_fields)
    }
    /// `mPlayer` (an `EEPlayer*`)
    ///
    /// # Safety
    /// A live unit.
    unsafe fn player(self) -> u64 {
        // SAFETY: the caller's contract
        unsafe { word(self.0, self.1.unit_player) }
    }
    /// `mComplexUnit`
    ///
    /// # Safety
    /// A live `EEUnit`.
    unsafe fn complex(self) -> U {
        // SAFETY: the caller's contract
        U(unsafe { (*self.uf()).mComplexUnit } as u64, self.1)
    }
    /// The `std::vector`'s begin and end at `offset` (its first two words).
    ///
    /// # Safety
    /// A live unit whose vector member is there.
    unsafe fn vector(self, offset: u64) -> (u64, u64) {
        // SAFETY: the caller's contract
        unsafe { (word(self.0, offset), word(self.0, offset + std::mem::size_of::<usize>() as u64)) }
    }
    /// `mCargo.size()`
    ///
    /// # Safety
    /// A live complex unit.
    unsafe fn cargo_len(self) -> u64 {
        // SAFETY: the caller's contract
        let (begin, end) = unsafe { self.vector(self.1.complex_cargo) };
        (end - begin) / std::mem::size_of::<usize>() as u64
    }
    /// `mCargo[i]`
    ///
    /// # Safety
    /// A live complex unit; `i` below the size.
    unsafe fn cargo(self, i: u64) -> u64 {
        // SAFETY: the caller's contract
        unsafe { word(self.vector(self.1.complex_cargo).0, i * std::mem::size_of::<usize>() as u64) }
    }
    /// `mBuildQueue.size()`
    ///
    /// # Safety
    /// A live complex unit.
    unsafe fn queue_len(self) -> u64 {
        // SAFETY: the caller's contract
        let (begin, end) = unsafe { self.vector(self.1.complex_build_queue) };
        (end - begin) / self.1.build_entry_stride.max(1)
    }
    /// `&mBuildQueue[i]`
    ///
    /// # Safety
    /// A live complex unit.
    unsafe fn entry(self, i: u64) -> u64 {
        // SAFETY: the caller's contract
        unsafe { self.vector(self.1.complex_build_queue).0 + i * self.1.build_entry_stride }
    }
    /// `mCommand ? mCommand->mType : 0`
    ///
    /// # Safety
    /// A live complex unit.
    unsafe fn command_type(self) -> i64 {
        // SAFETY: the caller's contract: the action is live while the unit points to it
        unsafe {
            let action = (*self.cx()).mCommand as u64;
            if action == 0 {
                0
            } else {
                i64::from(*at::<u32>(action, self.1.action_type))
            }
        }
    }
    /// `mGoalState` (0: none) and its `mKind`
    ///
    /// # Safety
    /// A live complex unit.
    unsafe fn goal_kind(self) -> Option<u8> {
        // SAFETY: the caller's contract: the goal is live while the unit points to it
        unsafe {
            let goal = (*self.cx()).mGoalState as u64;
            (goal != 0).then(|| *at::<u8>(goal, self.1.goal_kind))
        }
    }
    /// `mPhysicalModel.GetLocation()`'s x, y, z (the transformation's last column)
    ///
    /// # Safety
    /// A live complex unit.
    unsafe fn location(self, i: usize) -> f32 {
        // SAFETY: the caller's contract
        unsafe { (*at::<[[f32; 4]; 4]>(self.0, self.1.complex_location))[i][3] }
    }
}

/// A player's records, by its handle.
#[derive(Clone, Copy)]
struct P(u64, &'static EeWorldLayout);

impl P {
    fn rts(self) -> *mut EeUnitsRtsPlayerFields {
        at(self.0, self.1.rts_player_fields)
    }
    fn saved(self) -> *mut EeUnitsPlayerSaved {
        at(self.0, self.1.player_saved)
    }
    fn fields(self) -> *mut EeUnitsPlayerFields {
        at(self.0, self.1.player_fields)
    }
    /// `mEnemies[other's mPlayerID]`
    ///
    /// # Safety
    /// Both live players.
    unsafe fn enemy_of(self, other: P) -> c_ulong {
        // SAFETY: the caller's contract
        unsafe { (*self.saved()).mEnemies[(*other.rts()).mPlayerID as usize] }
    }
}

/// `kUnitCategorySubmergedNaval`, `kUnitCategorySeaAnimal`, `kActionUnderAttack`
const CU_VIEW_CATEGORY_SUBMERGED_NAVAL: c_ulong = 10;
const CU_VIEW_CATEGORY_SEA_ANIMAL: c_ulong = 27;
const CU_VIEW_ACTION_UNDER_ATTACK: i64 = 2018;

/// `ee_units_cu_body(what, unit, a, b, c)` (an `EEComplexUnit` member's Rust body); the host's 0
///
/// # Safety
/// `unit` a live complex unit.
unsafe fn body(what: i32, unit: u64, a: i64, b: i64, c: i64) -> i64 {
    // SAFETY: the caller's contract
    unsafe { crate::units::cu_body::game::ee_units_cu_body(what, unit as usize as _, a, b, c) };
    0
}

/// `ee_units_cu_small(what, unit, a, b)`; the host's 0
///
/// # Safety
/// As [`body`].
unsafe fn small(what: i32, unit: u64, a: u64, b: i64) -> i64 {
    // SAFETY: the caller's contract
    unsafe { crate::units::cu_small::game::ee_units_cu_small(what, unit as usize as _, a as usize as _, b) };
    0
}

/// `ee_units_cu_tiny(what, unit, a, 0, 0, 0, 0, 0)`; the host's 0
///
/// # Safety
/// As [`body`].
unsafe fn tiny(what: i32, unit: u64, a: u64) -> i64 {
    // SAFETY: the caller's contract
    unsafe {
        crate::units::u12a_small::game::ee_units_cu_tiny(what, unit as usize as _, a as usize as _, 0, 0, 0.0, 0.0, 0.0)
    };
    0
}

/// `mIsDisabled && !IsProtected()` of a complex unit (protected: disabled, by reason 0x1a or 0x1c)
///
/// # Safety
/// A live complex unit.
unsafe fn disabled_unprotected(u: U) -> bool {
    // SAFETY: the caller's contract
    unsafe {
        let disabled = flag!(u.cx(), mIsDisabled);
        let reason = (*u.cx()).mDisabledReason;
        disabled && !(disabled && (reason == 0x1a || reason == 0x1c))
    }
}

/// The answer of `ee_units_cu_host`'s statement `op` when it is a plain record read or store (None: the game's
/// host answers it).
///
/// # Safety
/// As the host's: `unit` is the live complex unit of the body (0 for the pool's statements), `a` the statement's
/// object, on the simulation thread inside the C++ member that handed over the unit.
pub(crate) unsafe fn cu_view(op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let l = layout();
    let u = U(unit, l);
    let o = U(a, l);
    // the host's `theClass`: the class `a`, else the unit's
    let class = |l: &EeWorldLayout| -> *mut EeUnitsUnitClassFields {
        if a != 0 {
            at(a, l.unit_class_fields)
        } else {
            // SAFETY: the unit's class (the host's preamble reads it too)
            unsafe { u.class() }
        }
    };
    // SAFETY: the host's contract: each statement reads or writes a member of live objects, as its C++ did
    let v = unsafe {
        match op {
            // the unit's own members
            EE_UNITS_CU_T_WEAPON_TAG => i64::from(low32((*u.cx()).mWeaponTagPoint)),
            EE_UNITS_CU_T_SET_OWNER_FIELD => {
                (*u.cx()).mOwner = a as usize as _;
                0
            }
            EE_UNITS_CU_S_HAS_SOUND => i64::from(!(*u.cx()).mSound.is_null()),
            EE_UNITS_CU_S_SET_SOUND => {
                (*u.cx()).mSound = a as usize as _;
                0
            }
            EE_UNITS_CU_S_HEAL_EFFECT_NONE => i64::from((*u.cx()).mHealEffect.mValue == u32::MAX),
            EE_UNITS_CU_S_CLEAR_HEAL_EFFECT | EE_UNITS_CU_RESET_HEAL_EFFECT => {
                (*u.cx()).mHealEffect.mValue = u32::MAX;
                0
            }
            EE_UNITS_CU_CONVERSION_IS_NONE => i64::from((*u.cx()).mConversionEffect.mValue == u32::MAX),
            EE_UNITS_CU_RESET_CONVERSION_EFFECT => {
                (*u.cx()).mConversionEffect.mValue = u32::MAX;
                0
            }
            EE_UNITS_CU_FIRE_EFFECT => i64::from((*u.cx()).mFireEffect.mValue),
            EE_UNITS_CU_CLEAR_FIRE_EFFECT | EE_UNITS_CU_RESET_FIRE_EFFECT => {
                (*u.cx()).mFireEffect.mValue = 0;
                0
            }
            EE_UNITS_CU_S_SET_INVALID => {
                *(addr_of!((*u.rts()).mIsValid) as *mut u8) = 0;
                0
            }
            EE_UNITS_CU_HIT_POINTS | EE_UNITS_CUM_HIT_POINTS => long((*u.uf()).mHitPoints),
            EE_UNITS_CU_PREVIOUS_PART => h((*u.cx()).mPreviousPart),
            EE_UNITS_CU_NEXT_PART => h((*u.cx()).mNextPart),
            EE_UNITS_CU_IS_BUILT | EE_UNITS_CUM_IS_BUILT => i64::from(flag!(u.cx(), mIsBuilt)),
            EE_UNITS_CU_LAST_ATTACKER => h((*u.cx()).mLastAttacker),
            EE_UNITS_CU_CLEAR_LAST_ATTACKER => {
                (*u.cx()).mLastAttacker = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_OWNER => h((*u.cx()).mOwner),
            EE_UNITS_CU_CLEAR_OWNER => {
                (*u.cx()).mOwner = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_HOLDER => h((*u.cx()).mHolder),
            EE_UNITS_CU_CLEAR_HOLDER_FIELD => {
                (*u.cx()).mHolder = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_CONTAINER => h((*u.cx()).mContainer),
            EE_UNITS_CU_CLEAR_CONTAINER => {
                (*u.cx()).mContainer = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_CLEAR_GOAL_STATE => {
                (*u.cx()).mGoalState = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_CLEAR_ACTIVE_GOAL => {
                (*u.cx()).mActiveGoal = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_CURRENT_RESEARCH => long((*u.cx()).mCurrentResearch),
            EE_UNITS_CU_SET_CURRENT_RESEARCH => {
                (*u.cx()).mCurrentResearch = b as c_long;
                0
            }
            EE_UNITS_CU_SOUND => h((*u.cx()).mSound),
            EE_UNITS_CU_CLEAR_SOUND => {
                (*u.cx()).mSound = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_PATROL_GROUP => word(unit, l.complex_patrol_group) as i64,
            EE_UNITS_CU_CLEAR_PATROL_GROUP => {
                *at::<usize>(unit, l.complex_patrol_group) = 0;
                0
            }
            EE_UNITS_CU_SET_PATROL_GROUP_FIELD => {
                *at::<usize>(unit, l.complex_patrol_group) = a as usize;
                0
            }
            EE_UNITS_CU_CLEAR_PREVIOUS_PART => {
                (*u.cx()).mPreviousPart = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_CLEAR_NEXT_PART => {
                (*u.cx()).mNextPart = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_HOME_BASE => h((*u.cx()).mHomeBase),
            EE_UNITS_CU_CLEAR_HOME_BASE => {
                (*u.cx()).mHomeBase = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_MINE => h((*u.cx()).mMine),
            EE_UNITS_CU_CLEAR_MINE => {
                (*u.cx()).mMine = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_FOUNDATION_FIELD => i64::from(flag!(u.cx(), mIsFoundation)),
            EE_UNITS_CU_CLEAR_FOUNDATION => {
                (*u.cx()).mIsFoundation = false;
                0
            }
            EE_UNITS_CU_SET_FOUNDATION => {
                (*u.cx()).mIsFoundation = b != 0;
                0
            }
            EE_UNITS_CU_SET_PRODUCTION_TIME => {
                (*u.cx()).mProductionTime = b as c_long;
                0
            }
            EE_UNITS_CU_HAS_COMMAND => i64::from(!(*u.cx()).mCommand.is_null()),
            EE_UNITS_CU_COMMAND_TYPE | EE_UNITS_CU_COMMAND_KIND => u.command_type(),
            EE_UNITS_CU_ANIMATION => ulong((*u.cx()).mAnimation),
            EE_UNITS_CU_SET_ANIMATION_FIELD => {
                (*u.cx()).mAnimation = b as c_long as c_ulong;
                0
            }
            EE_UNITS_CU_CLEAR_ANIMATION_TIME => {
                (*u.cx()).mAnimationTime = 0;
                0
            }
            EE_UNITS_CU_SET_PAUSED => {
                (*u.cx()).mIsAnimationPaused = b != 0;
                0
            }
            EE_UNITS_CU_PM_FRAME => long(*at::<c_ulong>(unit, l.complex_frame) as c_long),
            EE_UNITS_CU_DUMP_FRAME => ulong(*at::<c_ulong>(unit, l.complex_frame)),
            EE_UNITS_CU_PLAYER => u.player() as i64,
            EE_UNITS_CU_HAS_PLAYER => i64::from(u.player() != 0),
            EE_UNITS_CU_LOCATION_X => bits(u.location(0)),
            EE_UNITS_CU_LOCATION_Y => bits(u.location(1)),
            EE_UNITS_CU_QUEUE_SIZE | EE_UNITS_CUM_BUILD_QUEUE_SIZE => u.queue_len() as i64,
            EE_UNITS_CU_QUEUE_EMPTY => i64::from(u.queue_len() == 0),
            EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY => long(*at::<c_long>(u.entry(0), l.build_entry_technology)),
            EE_UNITS_CU_QUEUE_FRONT_FLAG => long(*at::<c_long>(u.entry(0), l.build_entry_flag)),
            EE_UNITS_CU_QUEUE_BACK_TECHNOLOGY => {
                long(*at::<c_long>(u.entry(u.queue_len().wrapping_sub(1)), l.build_entry_technology))
            }
            EE_UNITS_CU_QUEUE_TECHNOLOGY_AT => long(*at::<c_long>(u.entry(b as u64), l.build_entry_technology)),
            EE_UNITS_CU_QUEUE_SET_TECHNOLOGY_AT => {
                *at::<c_long>(u.entry(b as u64), l.build_entry_technology) = c as c_long;
                0
            }
            EE_UNITS_CU_CARGO_ROOM => long((*u.cx()).mCargoRoom),
            EE_UNITS_CU_SET_CARGO_ROOM => {
                (*u.cx()).mCargoRoom = b as c_long;
                0
            }
            EE_UNITS_CU_GARRISON_COUNT => long((*u.cx()).mGarrisonCount),
            EE_UNITS_CU_SET_GARRISON_COUNT => {
                (*u.cx()).mGarrisonCount = b as c_long;
                0
            }
            EE_UNITS_CU_GARRISON_POPULATION => long((*u.cx()).mGarrisonPopulation),
            EE_UNITS_CU_SET_GARRISON_POPULATION => {
                (*u.cx()).mGarrisonPopulation = b as c_long;
                0
            }
            EE_UNITS_CU_CARGO_COUNT | EE_UNITS_CU_CARGO_SIZE | EE_UNITS_CU_NUM_CARGO_UNITS => u.cargo_len() as i64,
            EE_UNITS_CU_CARGO_AT => u.cargo(b as c_ulong as u64) as i64,
            EE_UNITS_CU_CARGO_ID_AT => ulong((*U(u.cargo(b as c_ulong as u64), l).rts()).mUnitID),
            EE_UNITS_CU_TILE_X_FIELD | EE_UNITS_CU_GET_TILE_X => long((*u.rts()).mTileX),
            EE_UNITS_CU_TILE_Y => long((*u.rts()).mTileY),
            EE_UNITS_CU_DUMP_TILE => long(if b == 0 { (*u.rts()).mTileX } else { (*u.rts()).mTileY }),
            EE_UNITS_CU_DUMP_COLLISION_TILE => {
                long(if b == 0 { (*u.cx()).mCollisionTile.mX } else { (*u.cx()).mCollisionTile.mY })
            }
            EE_UNITS_CU_RESET_COLLISION_TILE_X => {
                (*u.cx()).mCollisionTile.mX = -10000;
                0
            }
            EE_UNITS_CU_RESET_COLLISION_TILE_Y => {
                (*u.cx()).mCollisionTile.mY = -10000;
                0
            }
            EE_UNITS_CU_COLLISION_STATE => i64::from((*u.cx()).mCollisionState),
            EE_UNITS_CU_TILE | EE_UNITS_CU_TILE_POINTER => word(unit, l.unit_tile) as i64,
            EE_UNITS_CU_GOAL_IS_BUILD => i64::from(u.goal_kind() == Some(CU_VIEW_GOAL_BUILD_REPAIR)),
            EE_UNITS_CU_HAS_GOAL_STATE => i64::from(!(*u.cx()).mGoalState.is_null()),
            EE_UNITS_CU_GOAL_KIND => i64::from(u.goal_kind()?),
            EE_UNITS_CU_CLEAR_SHIFT_QUEUED => {
                (*u.cx()).mHasShiftQueuedBuild = false;
                0
            }
            EE_UNITS_CU_SET_WATER_STATE => {
                (*u.cx()).mWaterState = b as c_long;
                0
            }
            EE_UNITS_CU_CLEAR_TURRET_ANGLES | EE_UNITS_CUM_TURRETS_NULL => {
                (*u.cx()).mTurretAngles = std::ptr::null_mut();
                0
            }
            EE_UNITS_CUM_TURRET_ZERO => {
                *(*u.cx()).mTurretAngles.add(b as c_ulong as usize) = 0.0;
                0
            }
            EE_UNITS_CU_SET_ORBITAL_COUNT | EE_UNITS_CU_SET_NUM_ORBITALS => {
                (*u.cx()).mNumOrbitals = b as c_ulong;
                0
            }
            EE_UNITS_CU_ZERO_NUM_ORBITALS => {
                (*u.cx()).mNumOrbitals = 0;
                0
            }
            EE_UNITS_CU_NUM_ORBITALS => ulong((*u.cx()).mNumOrbitals),
            EE_UNITS_CU_CLEAR_ORBITALS => {
                (*u.cx()).mOrbitals = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_SET_WHEEL_COUNT | EE_UNITS_CU_SET_NUM_WHEELS => {
                (*u.cx()).mNumWheels = b as c_ulong;
                0
            }
            EE_UNITS_CU_ZERO_NUM_WHEELS => {
                (*u.cx()).mNumWheels = 0;
                0
            }
            EE_UNITS_CU_NUM_WHEELS => ulong((*u.cx()).mNumWheels),
            EE_UNITS_CU_CLEAR_WHEELS => {
                (*u.cx()).mWheels = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_DISABLED => i64::from(flag!(u.cx(), mIsDisabled)),
            EE_UNITS_CU_SET_DISABLED_FIELD => {
                (*u.cx()).mIsDisabled = b != 0;
                0
            }
            EE_UNITS_CU_SET_DISABLED_REASON => {
                (*u.cx()).mDisabledReason = b as c_long as c_ulong;
                0
            }
            EE_UNITS_CU_IS_CLOAKED => i64::from((*u.cx()).mCloaked > 0),
            EE_UNITS_CU_CLOAKED => ulong((*u.cx()).mCloaked),
            EE_UNITS_CU_CLEAR_CLOAKED_TIME => {
                (*u.cx()).mCloaked = 0;
                0
            }
            EE_UNITS_CU_CLOAKABLE => i64::from(flag!(u.cx(), mIsCloakable)),
            EE_UNITS_CU_CLOAKED_FIELD => i64::from(flag!(u.cx(), mIsCloaked)),
            EE_UNITS_CU_CLEAR_CLOAKABLE => {
                (*u.cx()).mIsCloakable = false;
                0
            }
            EE_UNITS_CU_CLEAR_CLOAKED => {
                (*u.cx()).mIsCloaked = false;
                0
            }
            EE_UNITS_CU_SET_CAMOUFLAGE_TIMER => {
                (*u.cx()).mCamouflageTimer = b as c_ulong;
                0
            }
            EE_UNITS_CU_SET_U32C => {
                (*u.cx()).mU32C = b as c_ulong;
                0
            }
            EE_UNITS_CU_GATHERED => h((*u.cx()).mGatheredResource),
            EE_UNITS_CU_GATHERED_VALID => i64::from(flag!(U((*u.cx()).mGatheredResource as u64, l).rts(), mIsValid)),
            EE_UNITS_CU_SET_GATHERED_FIELD => {
                (*u.cx()).mGatheredResource = a as usize as _;
                0
            }
            EE_UNITS_CU_PREVIOUS_NEXT => h((*U((*u.cx()).mPreviousPart as u64, l).cx()).mNextPart),
            EE_UNITS_CU_CLEAR_PREVIOUS_NEXT => {
                (*U((*u.cx()).mPreviousPart as u64, l).cx()).mNextPart = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_SET_PREVIOUS_PART => {
                (*u.cx()).mPreviousPart = a as usize as _;
                0
            }
            EE_UNITS_CU_SET_PREVIOUS_NEXT_THIS => {
                (*U((*u.cx()).mPreviousPart as u64, l).cx()).mNextPart = unit as usize as _;
                0
            }
            EE_UNITS_CU_HAS_CLASS => i64::from(!(*u.uf()).mUnitClass.is_null()),
            EE_UNITS_CU_IS_LOCKED => i64::from(flag!(u.cx(), mIsLocked)),
            EE_UNITS_CU_IS_COLLIDABLE => i64::from((*u.cx()).mIsCollidable != 0),
            EE_UNITS_CU_GROWTH => bits((*u.cx()).mGrowth),
            EE_UNITS_CU_SET_GROWTH => {
                (*u.cx()).mGrowth = float(b);
                0
            }
            EE_UNITS_CU_COLLISION_RADIUS => bits((*u.cx()).mCollisionRadius),
            EE_UNITS_CU_SET_COLLISION_RADIUS => {
                (*u.cx()).mCollisionRadius = float(b);
                0
            }
            EE_UNITS_CU_ZERO_NUM_AREA_EFFECTS => {
                (*u.cx()).mNumAreaEffects = 0;
                0
            }
            EE_UNITS_CU_COUNT_AREA_EFFECT => {
                (*u.cx()).mNumAreaEffects = (*u.cx()).mNumAreaEffects.wrapping_add(1);
                0
            }
            EE_UNITS_CU_IS_DELETABLE => i64::from(flag!(u.cx(), mIsDeletable)),
            EE_UNITS_CU_BLOCKING_UNIT => h((*u.cx()).mBlockingUnit),
            EE_UNITS_CU_SET_BLOCKING_FIELD => {
                (*u.cx()).mBlockingUnit = a as usize as _;
                0
            }
            EE_UNITS_CU_LAST_RESOURCE => h((*u.cx()).mLastResource),
            EE_UNITS_CU_SET_LAST_RESOURCE_FIELD => {
                (*u.cx()).mLastResource = a as usize as _;
                0
            }
            EE_UNITS_CU_SET_HOLDER_FIELD => {
                (*u.cx()).mHolder = a as usize as _;
                0
            }
            EE_UNITS_CU_SET_MINE_FIELD => {
                (*u.cx()).mMine = a as usize as _;
                0
            }
            EE_UNITS_CU_SET_HOME_BASE_FIELD => {
                (*u.cx()).mHomeBase = a as usize as _;
                0
            }
            EE_UNITS_CU_BEHAVIOR => h((*u.cx()).mBehavior),
            EE_UNITS_CU_SET_BEHAVIOR => {
                (*u.cx()).mBehavior = a as usize as _;
                0
            }
            EE_UNITS_CU_CLEAR_FIELD => {
                let cx = u.cx();
                match b {
                    EE_UNITS_CU_FIELD_LAUNCHER_CLASS => (*cx).mLauncherClass = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_OWNER => (*cx).mOwner = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_ACTIVE_GROUP => (*cx).mActiveGroup = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_PREVIOUS_PART => (*cx).mPreviousPart = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_NEXT_PART => (*cx).mNextPart = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_HOME_BASE => (*cx).mHomeBase = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_CONTAINER => (*cx).mContainer = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_SOUND => (*cx).mSound = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_ACTIVE_GOAL => (*cx).mActiveGoal = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_GOAL_STATE => (*cx).mGoalState = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_BLOCKING_UNIT => (*cx).mBlockingUnit = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_PATROL_GROUP => *at::<usize>(unit, l.complex_patrol_group) = 0,
                    EE_UNITS_CU_FIELD_TURRET_ANGLES => (*cx).mTurretAngles = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_ORBITALS => (*cx).mOrbitals = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_WHEELS => (*cx).mWheels = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_GATHERED_RESOURCE => (*cx).mGatheredResource = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_LAST_RESOURCE => (*cx).mLastResource = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_COMMAND => (*cx).mCommand = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_LAST_ATTACKER => (*cx).mLastAttacker = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_MINE => (*cx).mMine = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_HERD => (*cx).mHerd = std::ptr::null_mut(),
                    EE_UNITS_CU_FIELD_HOLDER => (*cx).mHolder = std::ptr::null_mut(),
                    _ => {}
                }
                0
            }
            // the Rust calls the C++ statements forwarded to
            EE_UNITS_CU_T_IN_REACH => i64::from(crate::combat::cg_query::ee_units_cgq_in_reach(unit, a)),
            EE_UNITS_CU_T_READ_ENTER => i64::from(crate::combat::cg_query::ee_combat_host_read_enter(0)),
            EE_UNITS_CU_T_READ_LEAVE => {
                crate::combat::cg_query::ee_combat_host_read_leave(0);
                0
            }
            EE_UNITS_CU_PATROL_REMOVE => {
                crate::units::patrol_group::ee_units_pg_remove(
                    word(unit, l.complex_patrol_group) as usize as _,
                    unit as usize as _,
                );
                0
            }
            EE_UNITS_CU_PATROL_DEREFERENCE => {
                crate::units::patrol_group::ee_units_pg_dereference(word(unit, l.complex_patrol_group) as usize as _);
                0
            }
            EE_UNITS_CU_PATROL_REFERENCE => {
                crate::units::patrol_group::ee_units_pg_reference(a as usize as _);
                0
            }
            EE_UNITS_CU_CAN_STAND_ON => {
                i64::from(crate::units::uu_ffi::ee_units_uu_can_stand_on(b as i32, c as i32, unit))
            }
            EE_UNITS_CU_CLOAKED_ENEMY => {
                i64::from(crate::units::uu_ffi::ee_units_uu_cloaked_enemy(unit, current_player()?))
            }
            EE_UNITS_CU_COMPLEX_RESET => {
                crate::units::unit_bulk::ee_units_complex_reset(u.cx(), a as usize as _);
                0
            }
            EE_UNITS_CU_COMPLEX_COPY => {
                crate::units::unit_bulk::ee_units_complex_copy(b as u32, u.cx(), U(c as u64, l).cx(), a as usize as _);
                0
            }
            EE_UNITS_CU_S_NOT_CURRENT_PLAYER => i64::from(current_player()? != u.player()),
            EE_UNITS_CU_CURRENT_PLAYER => current_player()? as i64,
            EE_UNITS_CU_NUM_PLAYERS => ulong(*at::<c_ulong>(l.world_address, l.world_num_players)),
            EE_UNITS_CU_PLAYER_AT => world_player(Some(u64::from(b as u16)).filter(|&i| i < 17)?) as i64,
            EE_UNITS_CUM_SCALE_OF => {
                f[0] = *at::<f32>(o.complex().0, l.complex_scale);
                0
            }
            // the members that are one Rust call (ee/EEUnit.h's inline forwarders), as the C++ statement made it
            EE_UNITS_CU_START_ACTION => body(EE_UNITS_CU_DO_START_ACTION, unit, b as c_long as i64, 0, 0),
            EE_UNITS_CU_UNIT_START_ACTION => body(EE_UNITS_CU_DO_START_ACTION, a, b as c_long as i64, 0, 0),
            EE_UNITS_CU_PLAY_EFFECT => body(EE_UNITS_CU_DO_PLAY_EFFECT, unit, b as c_long as i64, 0, 0),
            EE_UNITS_CU_CLEAR_GOALS => body(EE_UNITS_CU_DO_CLEAR_GOALS, unit, 0, 0, 0),
            EE_UNITS_CU_UPDATE_JIT => body(EE_UNITS_CU_DO_UPDATE_JIT, unit, 0, 0, 0),
            EE_UNITS_CU_KILL_CARGO => body(EE_UNITS_CU_DO_KILL_CARGO, unit, 0, 0, 0),
            EE_UNITS_CU_CANCEL_QUEUED_PRODUCTION => body(EE_UNITS_CU_DO_CANCEL_QUEUED_PRODUCTION, unit, 0, 0, 0),
            EE_UNITS_CU_RELEASE_FIRE_EFFECT => body(EE_UNITS_CU_DO_RELEASE_FIRE_EFFECT, unit, 0, 0, 0),
            EE_UNITS_CU_RELEASE_CONVERSION_EFFECT => body(EE_UNITS_CU_DO_RELEASE_CONVERSION_EFFECT, unit, 0, 0, 0),
            EE_UNITS_CU_UPDATE_CAMOUFLAGE => body(EE_UNITS_CU_DO_UPDATE_CAMOUFLAGE, unit, b as c_long as i64, 0, 0),
            EE_UNITS_CU_UPDATE_CAMOUFLAGE_COMMAND => {
                body(EE_UNITS_CU_DO_UPDATE_CAMOUFLAGE, unit, u.command_type() as c_long as i64, 0, 0)
            }
            EE_UNITS_CU_CLEAR_GATHERED | EE_UNITS_CU_CLEAR_GATHERED_RESOURCE => {
                body(EE_UNITS_CU_DO_SET_GATHERED_RESOURCE, unit, 0, 0, 0)
            }
            EE_UNITS_CU_LINK_PART_TO => body(EE_UNITS_CU_DO_LINK_PART, unit, a as i64, 0, 0),
            EE_UNITS_CU_ADD_BUILD_TARGET_UNIT => body(EE_UNITS_CU_DO_ADD_BUILD_TARGET, unit, a as i64, 0, 0),
            EE_UNITS_CU_RELEASE_CARGO => body(EE_UNITS_CU_DO_RELEASE_CARGO, unit, a as i64, 0, 0),
            EE_UNITS_CU_TAKE_OFF_MAP => body(EE_UNITS_CU_DO_TAKE_OFF_MAP, a, 0, 0, 0),
            EE_UNITS_CU_ADD_CONTACT => body(EE_UNITS_CU_DO_ADD_CONTACT, a, b, 0, 0),
            EE_UNITS_CU_CLEAR_GOAL_QUEUE | EE_UNITS_CU_S_CLEAR_GOAL_QUEUE => {
                body(EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE, unit, b as c_ulong as i64, 0, 0)
            }
            EE_UNITS_CU_PAY_INSURANCE => body(EE_UNITS_CU_DO_PAY_INSURANCE, unit, i64::from(b as u32), 0, 0),
            EE_UNITS_CU_T_MOVE_TO_LOCATION => {
                body(EE_UNITS_CU_DO_MOVE_TO_LOCATION, unit, bits(f[0]), bits(f[1]), i64::from(b != 0))
            }
            EE_UNITS_CU_SET_ANIMATION_STATE => body(EE_UNITS_CU_DO_SET_ANIMATION, unit, b as c_long as i64, -1, 0),
            EE_UNITS_CU_COPY_ANIMATION => {
                body(EE_UNITS_CU_DO_SET_ANIMATION, unit, (*o.cx()).mAnimation as c_long as i64, -1, 0)
            }
            EE_UNITS_CU_UPDATE_WATER_STATE_OFF => body(EE_UNITS_CU_DO_UPDATE_WATER_STATE, unit, 0, 0, 0),
            EE_UNITS_CU_SET_HIT_POINTS => {
                crate::units::eu_body::game::ee_units_eu_body(
                    EE_UNITS_EU_DO_SET_HIT_POINTS,
                    unit as usize as _,
                    b as c_long as i64,
                    0,
                    0,
                );
                0
            }
            EE_UNITS_CU_MOVE_TO | EE_UNITS_CU_MOVE_TO_UPDATE => {
                let update = op == EE_UNITS_CU_MOVE_TO_UPDATE && a != 0;
                crate::units::eu_body::game::ee_units_eu_body(
                    EE_UNITS_EU_DO_MOVE_TO,
                    unit as usize as _,
                    b as c_long as i64,
                    c as c_long as i64,
                    i64::from(update),
                );
                0
            }
            EE_UNITS_CU_CLEAR_HOLDER => tiny(EE_UNITS_CU_T_DO_CLEAR_HOLDER, unit, 0),
            EE_UNITS_CU_HELD_CLEAR_HOLDER => {
                let held = word(u.vector(l.complex_held_units).0, b as u64 * std::mem::size_of::<usize>() as u64);
                tiny(EE_UNITS_CU_T_DO_CLEAR_HOLDER, held, 0)
            }
            EE_UNITS_CU_SET_OWNER => tiny(EE_UNITS_CU_T_DO_SET_OWNER, unit, a),
            EE_UNITS_CU_CLEAR_GARRISON => {
                crate::units::u12a_small::clear_garrison(&mut *u.cx());
                0
            }
            EE_UNITS_CU_CLEAR_COLLISION => {
                crate::units::u12a_small::clear_collision_state(&mut *u.cx());
                0
            }
            EE_UNITS_CU_RELEASE_HEAL_EFFECT => small(EE_UNITS_CU_S_DO_RELEASE_HEAL_EFFECT, unit, 0, 0),
            EE_UNITS_CU_RELEASE_SOUND => small(EE_UNITS_CU_S_DO_RELEASE_SOUND, unit, 0, 0),
            EE_UNITS_CU_COPY_SOUND => small(EE_UNITS_CU_S_DO_SET_SOUND, unit, (*o.cx()).mSound as u64, 0),
            EE_UNITS_CU_SET_ANIMATION_FRAME => {
                small(EE_UNITS_CU_S_DO_SET_ANIMATION_FRAME, unit, 0, b as c_ulong as i64)
            }
            EE_UNITS_CU_COPY_ANIMATION_FRAME => {
                small(EE_UNITS_CU_S_DO_SET_ANIMATION_FRAME, unit, 0, ulong(*at::<c_ulong>(a, l.complex_frame)))
            }
            EE_UNITS_CU_DROP_PATH => {
                crate::units::waypoints::ee_units_waypoints_drop(unit);
                0
            }
            EE_UNITS_CU_DROP_FINISHED_GOALS => {
                goals::ee_units_unit_drop_finished_goals(unit, false);
                0
            }
            EE_UNITS_CU_STOP => {
                crate::units::cu_inline::ee_units_cu_set_velocity(u.uf(), u.cx(), 0.0);
                0
            }
            EE_UNITS_CU_SET_PRODUCTION_TIME_FIELD => {
                (*u.cx()).mProductionTime = b as c_long;
                0
            }
            EE_UNITS_CU_SET_RELOAD_TIMER => {
                (*u.cx()).mReloadTimer = b as c_long;
                0
            }
            EE_UNITS_CU_SET_TINTED => {
                (*u.cx()).mIsTinted = b != 0;
                0
            }
            EE_UNITS_CU_CLEAR_TINT => {
                (*u.cx()).mIsTinted = false;
                0
            }
            EE_UNITS_CU_UNIT_DISABLED_UNPROTECTED => i64::from(disabled_unprotected(o)),
            EE_UNITS_CU_IS_AIR_CATEGORY => i64::from(matches!(
                (*class(l)).mCategory,
                CU_VIEW_CATEGORY_SUBMERGED_NAVAL | CU_VIEW_CATEGORY_SEA_ANIMAL
            )),
            EE_UNITS_CU_IS_TRANSPORT => {
                i64::from(crate::units::uc_is::ee_units_uc_is((*u.uf()).mUnitClass as u64, EE_UNITS_UC_IS_TRANSPORT))
            }
            EE_UNITS_CU_IS_AIR_BASE => {
                i64::from(crate::units::uc_is::ee_units_uc_is((*u.uf()).mUnitClass as u64, EE_UNITS_UC_IS_AIR_BASE))
            }
            // the held units, the cargo and the contacts (a vector's elements, the contact list's nodes)
            EE_UNITS_CU_HELD_COUNT => {
                let (begin, end) = u.vector(l.complex_held_units);
                ((end - begin) / std::mem::size_of::<usize>() as u64) as i64
            }
            EE_UNITS_CU_HELD_FIND => {
                let (begin, end) = u.vector(l.complex_held_units);
                let w = std::mem::size_of::<usize>() as u64;
                (0..(end - begin) / w).find(|&i| word(begin, i * w) == a).map_or(-1, |i| i as i64)
            }
            EE_UNITS_CU_HELD_HOLDER_IS_THIS => {
                let held = word(u.vector(l.complex_held_units).0, b as u64 * std::mem::size_of::<usize>() as u64);
                i64::from((*U(held, l).cx()).mHolder as u64 == unit)
            }
            EE_UNITS_CU_CARGO_SET_AT => {
                *at::<usize>(u.vector(l.complex_cargo).0, b as u64 * std::mem::size_of::<usize>() as u64) = a as usize;
                0
            }
            EE_UNITS_CU_CONTACTS_HEAD => word(unit, l.complex_contacts_head) as i64,
            EE_UNITS_CU_CONTACT_NEXT => word(a, l.contact_node_next) as i64,
            EE_UNITS_CU_CONTACTS_FIND | EE_UNITS_CU_CONTACT_MISSING => {
                let node = ee_base::util::pool::ee_util_list_find(at(unit, l.complex_contacts_head), a as usize as _);
                if op == EE_UNITS_CU_CONTACT_MISSING {
                    i64::from(node.is_null())
                } else {
                    node as usize as i64
                }
            }
            EE_UNITS_CU_CLASS_TABLE => {
                let t = crate::world::view::WorldRead::begin();
                usize::try_from(b as c_long)
                    .ok()
                    .and_then(|i| crate::world::view::ClassView::by_index(&t, i))
                    .map_or(0, |k| k.handle() as i64)
            }
            // the goal queues (Rust's: goals.rs; past the queues the C++ indexed unchecked)
            EE_UNITS_CU_S_NUM_GOAL_QUEUES | EE_UNITS_CU_GOAL_QUEUE_COUNT => goals::unit_queues(unit).len() as i64,
            EE_UNITS_CU_UNIT_GOAL_QUEUE_COUNT => goals::unit_queues(a).len() as i64,
            EE_UNITS_CU_FIRST_QUEUE_LEN => goals::queue_len(unit, 0)? as i64,
            EE_UNITS_CU_QUEUE_GOAL_COUNT => goals::queue_len(unit, b as usize)? as i64,
            EE_UNITS_CU_UNIT_GOAL_COUNT => goals::queue_len(a, b as usize)? as i64,
            EE_UNITS_CU_QUEUE_GOAL_AT => goal_at(unit, b, c as usize)? as i64,
            EE_UNITS_CU_UNIT_GOAL_AT => goal_at(a, b, c as usize)? as i64,
            EE_UNITS_CU_QUEUE_FRONT_GOAL => goal_at(unit, b, 0)? as i64,
            EE_UNITS_CU_GOAL_CHANGED => {
                let len = goals::queue_len(unit, b as usize)?;
                i64::from(len <= c as usize || goal_at(unit, b, c as usize)? != a)
            }
            EE_UNITS_CU_QUEUED_GOAL => goals::queued_goal(unit, b as c_ulong as usize) as i64,
            EE_UNITS_CU_REFRESH_GOALS => {
                goals::refresh_goals(unit);
                0
            }
            EE_UNITS_CU_REFRESH_ACTIVE_GOAL => {
                goals::refresh_active_goal(unit);
                0
            }
            EE_UNITS_CU_SET_IS_CITIZEN => {
                (*u.cx()).mIsCitizen = b != 0;
                0
            }
            EE_UNITS_CUM_UNIT_ID => long((*u.rts()).mUnitID as c_long),
            // through the unit's mComplexUnit (GetChecksum's, the dump's)
            EE_UNITS_CU_TIMES_UPDATED => ulong((*u.complex().cx()).mTimesUpdated),
            EE_UNITS_CU_HAS_ACTIVE_GROUP => i64::from(!(*u.complex().cx()).mActiveGroup.is_null()),
            EE_UNITS_CU_NO_ACTIVE_GROUP => i64::from((*u.complex().cx()).mActiveGroup.is_null()),
            EE_UNITS_CU_HAS_GOAL_STATE_NOW => i64::from(!(*u.complex().cx()).mGoalState.is_null()),
            EE_UNITS_CU_COMMAND_OR_ZERO => u.complex().command_type(),
            EE_UNITS_CU_CHECKSUM_LOCATION => bits(u.complex().location(match b {
                0 => 0,
                1 => 1,
                _ => 2,
            })),
            EE_UNITS_CU_HEIGHT => bits((*u.complex().cx()).mHeight),
            // the unit's class and stats
            EE_UNITS_CU_CLASS_INACTIVE => i64::from(flag!(u.class(), mIsInactive)),
            EE_UNITS_CU_UNIT_TYPE | EE_UNITS_CUM_UNIT_TYPE => long((*u.class()).mUnitType as c_long),
            EE_UNITS_CU_CLASS_TYPE_FIELD => ulong((*u.class()).mUnitType),
            EE_UNITS_CU_CATEGORY => ulong((*u.class()).mCategory),
            EE_UNITS_CU_CIRCLES => i64::from(matches!(
                (*u.class()).mCategory,
                CU_VIEW_CATEGORY_AIRPLANE | CU_VIEW_CATEGORY_PLANET_SPACESHIP
            )),
            EE_UNITS_CU_AIR_TRAFFIC_TYPE => long((*u.class()).mAirTrafficType),
            EE_UNITS_CU_HAS_DEATH_SPAWN => i64::from(!(*u.links()).mDeathSpawnClass.is_null()),
            EE_UNITS_CU_HAS_REMAINS => i64::from(!(*u.links()).mRemainsClass.is_null()),
            EE_UNITS_CU_PROJECTILE_CLASS => h((*u.links()).mProjectileClass),
            EE_UNITS_CU_CLASS_BEHAVIOR => h((*u.links()).mBehavior),
            EE_UNITS_CU_IS_PROJECTILE => i64::from(flag!(u.class(), mIsProjectile)),
            EE_UNITS_CU_GARRISON_CAPACITY => long((*u.class()).mGarrisonCapacity),
            EE_UNITS_CU_TILE_SIZE | EE_UNITS_CUM_TILE_SIZE => long((*u.class()).mTileSize),
            EE_UNITS_CU_CLASS_DOMAINS => i64::from((*u.class()).mDomains),
            EE_UNITS_CU_WATER_KIND => long((*u.class()).mWaterKind),
            EE_UNITS_CU_IS_WALL => i64::from(flag!(u.class(), mIsWall)),
            EE_UNITS_CU_SHOOTS_HIGH => i64::from(flag!(u.class(), mShootsHigh)),
            EE_UNITS_CU_BLOCKS_TILE => i64::from(flag!(u.class(), mBlocksTile)),
            EE_UNITS_CU_IS_MELEE => i64::from(flag!(u.class(), mIsMelee)),
            EE_UNITS_CU_INNER_SPHERE => bits((*u.class()).mInnerCollisionSphere),
            EE_UNITS_CU_CLASS_INITIAL_GARRISON => long((*u.class()).mInitialGarrisonCount),
            EE_UNITS_CU_CLASS_CLOAKABLE => i64::from(flag!(u.class(), mIsCloakable)),
            EE_UNITS_CU_CLASS_FAMILY => ulong((*u.class()).mFamily),
            EE_UNITS_CUM_PITCHES => i64::from(flag!(u.class(), mPitchesWithTerrain)),
            EE_UNITS_CUM_ROLLS => i64::from(flag!(u.class(), mRollsWithTerrain)),
            EE_UNITS_CU_STATS_RANGE => bits((*u.stats()).mRange),
            EE_UNITS_CU_STATS_SCALE => bits((*u.stats()).mScale),
            EE_UNITS_CUM_STATS_SCALE => {
                f[0] = (*u.stats()).mScale;
                0
            }
            EE_UNITS_CU_FIRES_OVER_WALLS => i64::from(flag!(u.stats(), mCanFireOverWalls)),
            EE_UNITS_CU_STATS_AREA_TEMPLATE => long(*(*u.stats()).mAreaEffectTemplates.get(b as usize)?),
            EE_UNITS_CU_STATS_RELOAD_TIME => long((*u.stats()).mReloadTime),
            EE_UNITS_CU_STATS_POWER_REGENERATION => long((*u.stats()).mPowerRegeneration),
            EE_UNITS_CU_STATS_ATTACK => bits((*u.stats()).mAttack),
            EE_UNITS_CU_STATS_MAX_VELOCITY => bits((*u.stats()).mMaxVelocity),
            EE_UNITS_CUM_MAX_HIT_POINTS => long((*u.stats()).mMaxHitPoints),
            // `theClass` (the class `a`, else the unit's)
            EE_UNITS_CU_CLASS_WEIGHT => long((*class(l)).mWeight),
            EE_UNITS_CU_CLASS_UNIT_TYPE => ulong((*class(l)).mUnitType),
            EE_UNITS_CU_CLASS_TECHNOLOGY => long((*class(l)).mTechnology),
            EE_UNITS_CU_DOMAINS => i64::from((*class(l)).mDomains),
            EE_UNITS_CU_CLASS_CATEGORY | EE_UNITS_CU_CLASS_CATEGORY_OF => ulong((*class(l)).mCategory),
            EE_UNITS_CU_CLASS_AI_KIND => ulong((*class(l)).mAIKind),
            EE_UNITS_CU_CLASS_TILE_SIZE_OF => long((*class(l)).mTileSize),
            EE_UNITS_CU_CLASS_SCALE => bits((*class(l)).mScale),
            EE_UNITS_CU_INIT_CARGO_ROOM => {
                (*u.cx()).mCargoRoom = (*class(l)).mTransportCapacity;
                0
            }
            EE_UNITS_CU_INIT_COLLISION_RADIUS => {
                (*u.cx()).mCollisionRadius = (*class(l)).mInnerCollisionSphere;
                0
            }
            EE_UNITS_CU_INIT_DELETABLE => {
                (*u.cx()).mIsDeletable = flag!(class(l), mIsDeletable);
                0
            }
            // the other unit `a`'s
            EE_UNITS_CU_PART_IS_BUILT => i64::from(flag!(o.cx(), mIsBuilt)),
            EE_UNITS_CU_PART_SET_BUILT => {
                (*o.cx()).mIsBuilt = b != 0;
                0
            }
            EE_UNITS_CU_PART_AIR_TRAFFIC_TYPE => long((*o.class()).mAirTrafficType),
            EE_UNITS_CU_PART_UNIT_TYPE | EE_UNITS_CU_UNIT_UNIT_TYPE => long((*o.class()).mUnitType as c_long),
            EE_UNITS_CU_PART_PAID => {
                (*o.cx()).mIsPaidFor = true;
                0
            }
            EE_UNITS_CU_PART_NEXT => h((*o.cx()).mNextPart),
            EE_UNITS_CU_OTHER_COLLIDABLE => long((*o.cx()).mIsCollidable),
            EE_UNITS_CU_OTHER_HIT_POINTS | EE_UNITS_CU_UNIT_HIT_POINTS => long((*o.uf()).mHitPoints),
            EE_UNITS_CU_OTHER_CLASS => h((*o.uf()).mUnitClass),
            EE_UNITS_CU_UNIT_WEIGHT | EE_UNITS_CU_UNIT_POPULATION_COST => long((*o.class()).mWeight),
            EE_UNITS_CU_UNIT_CONTAINER => h((*o.cx()).mContainer),
            EE_UNITS_CU_SET_CONTAINER_THIS => {
                (*o.cx()).mContainer = unit as usize as _;
                0
            }
            EE_UNITS_CU_UNIT_CLEAR_CONTAINER => {
                (*o.cx()).mContainer = std::ptr::null_mut();
                0
            }
            EE_UNITS_CU_UNIT_PREVIOUS_PART => h((*o.cx()).mPreviousPart),
            EE_UNITS_CU_UNIT_CATEGORY => ulong((*o.class()).mCategory),
            EE_UNITS_CU_UNIT_DOMAINS => i64::from((*o.class()).mDomains),
            EE_UNITS_CU_UNIT_CRUISE_ALTITUDE => bits((*o.class()).mCruiseAltitude),
            EE_UNITS_CU_UNIT_TILE_X | EE_UNITS_CUM_TILE_X_OF => long((*o.rts()).mTileX),
            EE_UNITS_CU_UNIT_TILE_Y | EE_UNITS_CUM_TILE_Y_OF => long((*o.rts()).mTileY),
            EE_UNITS_CU_UNIT_TILE => long(if b == 0 { (*o.rts()).mTileX } else { (*o.rts()).mTileY }),
            EE_UNITS_CU_UNIT_TILE_SIZE | EE_UNITS_CUM_TILE_SIZE_OF => long((*o.class()).mTileSize),
            EE_UNITS_CU_UNIT_PLANET => i64::from((*o.complex().cx()).mPlanetID),
            EE_UNITS_CU_AREA_EFFECT_OF => bits((*o.stats()).mAreaEffect),
            EE_UNITS_CU_UNIT_VALID => i64::from(flag!(o.rts(), mIsValid)),
            EE_UNITS_CU_OTHER_OWNER => h((*o.cx()).mOwner),
            EE_UNITS_CU_UNIT_NUM_ORBITALS => ulong((*o.cx()).mNumOrbitals),
            EE_UNITS_CU_UNIT_NUM_WHEELS => ulong((*o.cx()).mNumWheels),
            EE_UNITS_CU_UNIT_CURRENT_RESEARCH => long((*o.cx()).mCurrentResearch),
            EE_UNITS_CU_SAME_UNIT_TYPE => i64::from((*u.class()).mUnitType == (*o.class()).mUnitType),
            EE_UNITS_CU_SAME_TILE_SIZE => i64::from((*o.class()).mTileSize == (*u.class()).mTileSize),
            EE_UNITS_CU_SAME_PLAYER => i64::from(o.player() == u.player()),
            EE_UNITS_CU_COPY_CONTACT_HEADING => {
                (*u.cx()).mInitialContactHeading = (*o.cx()).mInitialContactHeading;
                0
            }
            EE_UNITS_CU_COPY_BEHAVIOR => {
                (*u.cx()).mBehavior = (*o.cx()).mBehavior;
                0
            }
            EE_UNITS_CU_COPY_HEIGHT => {
                (*u.cx()).mHeight = (*o.cx()).mHeight;
                0
            }
            EE_UNITS_CU_COPY_HOME_BASE => {
                (*u.cx()).mHomeBase = (*o.cx()).mHomeBase;
                0
            }
            EE_UNITS_CU_COPY_CONTAINER => {
                (*u.cx()).mContainer = (*o.cx()).mContainer;
                0
            }
            EE_UNITS_CU_COPY_COMMAND => {
                (*u.cx()).mCommand = (*o.cx()).mCommand;
                0
            }
            EE_UNITS_CU_COPY_TURRET_ANGLE => {
                *(*u.cx()).mTurretAngles.add(b as usize) = *(*o.cx()).mTurretAngles.add(b as usize);
                0
            }
            // the class upgrade `a`'s
            EE_UNITS_CU_UPGRADE_MAX_HIT_POINTS => {
                long((*at::<EeUnitsUnitClassUpgradeFields>(a, l.stats_fields)).mMaxHitPoints)
            }
            EE_UNITS_CU_UPGRADE_POWER => long((*at::<EeUnitsUnitClassUpgradeFields>(a, l.stats_fields)).mPower),
            EE_UNITS_CU_UPGRADE_FLIGHT_TIME => {
                long((*at::<EeUnitsUnitClassUpgradeFields>(a, l.stats_fields)).mFlightTime)
            }
            EE_UNITS_CU_UPGRADE_RELOAD_TIME => {
                long((*at::<EeUnitsUnitClassUpgradeFields>(a, l.stats_fields)).mReloadTime)
            }
            // the players'
            EE_UNITS_CU_ENEMY_STATE => ulong(P(a, l).enemy_of(P(u.player(), l))),
            EE_UNITS_CU_HOSTILE => {
                let (mine, theirs) = (P(u.player(), l), P(o.player(), l));
                i64::from(theirs.0 != mine.0 && mine.enemy_of(theirs) != 0)
            }
            EE_UNITS_CU_PLAYER_IS_FRIEND => {
                let (mine, them) = (P(u.player(), l), P(a, l));
                i64::from(them.0 == mine.0 || mine.enemy_of(them) == 0)
            }
            EE_UNITS_CU_REFUND_TOTAL => long(*(*P(a, l).saved()).mInsuranceRefunds.get(b as usize)?),
            EE_UNITS_CU_SET_REFUND_TOTAL => {
                *(*P(a, l).saved()).mInsuranceRefunds.get_mut(b as usize)? = c as c_long;
                0
            }
            EE_UNITS_CU_RESOURCE => long(*(*P(a, l).saved()).mResourceCaps.get(b as usize)?),
            EE_UNITS_CU_VITAL_BUILDINGS => long((*P(u.player(), l).saved()).mNumVitalBuildings),
            EE_UNITS_CU_NUM_CAPITOLS => long((*P(u.player(), l).saved()).mNumCapitols),
            EE_UNITS_CU_HAS_COMPUTER_PLAYER => i64::from(word(u.player(), l.player_computer_player) != 0),
            EE_UNITS_CU_PLAYER_ID | EE_UNITS_CU_DUMP_PLAYER_ID | EE_UNITS_CUM_PLAYER_ID => {
                ulong((*P(u.player(), l).rts()).mPlayerID)
            }
            EE_UNITS_CU_HEROES_IN_PRODUCTION => long((*P(u.player(), l).fields()).mNumHeroesInProduction),
            EE_UNITS_CU_SET_HEROES_IN_PRODUCTION => {
                (*P(u.player(), l).fields()).mNumHeroesInProduction = b as c_long;
                0
            }
            EE_UNITS_CU_HERO_LIMIT => long((*P(u.player(), l).fields()).mHeroLimit),
            EE_UNITS_CU_JIT_MANUFACTURING => i64::from(flag!(P(u.player(), l).fields(), mJITManufacturing)),
            EE_UNITS_CU_INSTANT_BUILD => i64::from(flag!(P(u.player(), l).fields(), mInstantBuild)),
            _ => return None,
        }
    };
    Some(v)
}

/// `kActionIdle`, `kHiddenFlagMoved`, `kMovementAir`, `kResourceNone`
const CU_VIEW_ACTION_IDLE: u32 = 5000;
const CU_VIEW_HIDDEN_FLAG_MOVED: u8 = 16;
const CU_VIEW_MOVEMENT_AIR: u8 = 1;
const CU_VIEW_RESOURCE_NONE: c_ulong = 0;

/// `gWorld`'s pointer member at `offset`.
///
/// # Safety
/// The world is live.
unsafe fn world_word(offset: u64) -> u64 {
    // SAFETY: the caller's contract
    unsafe { word(layout().world_address, offset) }
}

/// `gWorld.mPlayers[index]`
///
/// # Safety
/// The world is live; `index` below its 17 slots.
unsafe fn world_player(index: u64) -> u64 {
    // SAFETY: the caller's contract
    unsafe { world_word(layout().world_players + index * std::mem::size_of::<usize>() as u64) }
}

/// The answer of `ee_units_eu_host`'s statement `op` (EEUnit.cpp; the statements of eu_hit.rs and eu_body.rs) when
/// it is a plain record read or store (None: the game's host answers it).
///
/// # Safety
/// As the host's: `unit` a live `EEUnit`, `a` the statement's object (the other unit: the unit itself for 0).
pub(crate) unsafe fn eu_view(op: i32, unit: u64, a: u64, b: i64) -> Option<i64> {
    use crate::units::eu_body::*;
    use crate::units::eu_hit::*;
    let l = layout();
    let u = U(unit, l);
    let o = U(if a != 0 { a } else { unit }, l);
    // SAFETY: the host's contract: each statement reads or writes a member of live objects, as its C++ did
    let v = unsafe {
        match op {
            EE_UNITS_EU_COMPLEX => u.complex().0 as i64,
            EE_UNITS_EU_DISABLED_UNPROTECTED => i64::from(disabled_unprotected(u.complex())),
            EE_UNITS_EU_START_UNDER_ATTACK => {
                body(EE_UNITS_CU_DO_START_ACTION, u.complex().0, CU_VIEW_ACTION_UNDER_ATTACK, 0, 0)
            }
            EE_UNITS_EU_SET_LAST_ATTACKER => {
                crate::units::eu_more::game::ee_units_eu_more(
                    crate::units::eu_more::EE_UNITS_EUM_DO_SET_LAST_ATTACKER,
                    u.complex().0 as usize as _,
                    a as usize as _,
                    0,
                    0,
                );
                0
            }
            EE_UNITS_EU_TARGETABLE => {
                i64::from(crate::units::uu_ffi::ee_units_uu_targetable(unit, (*o.uf()).mUnitClass as u64, 0))
            }
            EE_UNITS_EU_CLOAKED_ENEMY => {
                i64::from(crate::units::uu_ffi::ee_units_uu_cloaked_enemy(unit, current_player()?))
            }
            EE_UNITS_EU_REGIONS_SIZE => i64::from(crate::combat::regions::ee_combat_regions_size(a as usize as _)),
            EE_UNITS_EU_PREVIOUS_PART => h((*u.complex().cx()).mPreviousPart),
            EE_UNITS_EU_NO_PREVIOUS_PART => i64::from((*u.complex().cx()).mPreviousPart.is_null()),
            EE_UNITS_EU_HIT_POINTS => long((*u.uf()).mHitPoints),
            EE_UNITS_EU_SET_HIT_POINTS => {
                (*u.uf()).mHitPoints = b as c_long;
                0
            }
            EE_UNITS_EU_HAS_SPARKS => i64::from(!(*u.links()).mImpactSparksEffect.is_null()),
            EE_UNITS_EU_HAS_BLOOD => i64::from(!(*u.links()).mImpactBloodEffect.is_null()),
            EE_UNITS_EU_HAS_DEATH_EFFECT => i64::from(!(*u.links()).mDeathEffect.is_null()),
            EE_UNITS_EU_DAMAGE_EFFECT => h((*u.links()).mDamageEffect),
            EE_UNITS_EU_ON_MAP => i64::from(word(o.0, l.unit_tile_node) != 0),
            EE_UNITS_EU_HAS_TILE_NODE => i64::from(word(unit, l.unit_tile_node) != 0),
            EE_UNITS_EU_SHOWS_DAMAGE => {
                i64::from(word(unit, l.unit_tile_node) == 0 || flag!(u.complex().cx(), mIsBuilt))
            }
            EE_UNITS_EU_IS_ENEMY => {
                let (mine, theirs) = (P(u.player(), l), P(o.player(), l));
                i64::from(theirs.0 != mine.0 && mine.enemy_of(theirs) != 0)
            }
            EE_UNITS_EU_ENEMY_OF => i64::from(P(u.player(), l).enemy_of(P(o.player(), l)) != 0),
            EE_UNITS_EU_SAME_PLAYER => i64::from(o.player() == u.player()),
            EE_UNITS_EU_OWNER => h((*o.complex().cx()).mOwner),
            EE_UNITS_EU_COMPLEX_OF => o.complex().0 as i64,
            EE_UNITS_EU_IS_IDLE => {
                let action = (*u.complex().cx()).mCommand as u64;
                i64::from(action != 0 && *at::<u32>(action, l.action_type) == CU_VIEW_ACTION_IDLE)
            }
            EE_UNITS_EU_NUM_PLAYERS => ulong(*at::<c_ulong>(l.world_address, l.world_num_players)),
            EE_UNITS_EU_FIRST_PLAYER_ID => ulong((*P(world_player(0), l).rts()).mPlayerID),
            EE_UNITS_EU_IS_CURRENT_PLAYER => {
                let current = *at::<c_long>(l.world_address, l.world_current_player);
                if !(0..17).contains(&current) {
                    return None;
                }
                i64::from(u.player() == world_player(current as i64 as u64))
            }
            EE_UNITS_EU_HAS_MAP => i64::from(world_word(l.world_map) != 0),
            EE_UNITS_EU_TILE_X => long((*o.rts()).mTileX),
            EE_UNITS_EU_TILE_Y => long((*o.rts()).mTileY),
            EE_UNITS_EU_SET_TILE_X => {
                (*u.rts()).mTileX = b as c_long;
                0
            }
            EE_UNITS_EU_SET_TILE_Y => {
                (*u.rts()).mTileY = b as c_long;
                0
            }
            EE_UNITS_EU_TILE => word(unit, l.unit_tile) as i64,
            EE_UNITS_EU_CONTAINER => h((*U(a, l).cx()).mContainer),
            EE_UNITS_EU_CATEGORY => ulong((*u.class()).mCategory),
            EE_UNITS_EU_UNIT_TYPE => ulong((*u.class()).mUnitType),
            EE_UNITS_EU_CLASS_TILE_SIZE => long((*u.class()).mTileSize),
            EE_UNITS_EU_CLASS_INACTIVE => i64::from(flag!(u.class(), mIsInactive)),
            EE_UNITS_EU_HAS_INFLUENCE => i64::from((*u.class()).mInfluence != 0),
            EE_UNITS_EU_CLASS_HIDES => {
                let k = u.class();
                i64::from(
                    flag!(k, mIsObstacle)
                        && (*k).mDomains & CU_VIEW_MOVEMENT_AIR == 0
                        && !flag!(k, mIsProjectile)
                        && (*k).mMaxVelocity == 0.0,
                )
            }
            EE_UNITS_EU_COMPLEX_CATEGORY => ulong((*u.complex().class()).mCategory),
            EE_UNITS_EU_TARGET_SEARCH_TIMER => long((*u.complex().cx()).mTargetSearchTimer),
            EE_UNITS_EU_CLEAR_TARGET_SEARCH_TIMER => {
                (*u.complex().cx()).mTargetSearchTimer = 0;
                0
            }
            EE_UNITS_EU_AMMUNITION => ulong((*u.complex().cx()).mAmmunition),
            EE_UNITS_EU_FLIGHT_TIME => long((*u.complex().cx()).mFlightTime),
            EE_UNITS_EU_CLEAR_FLIGHT_TIME => {
                (*u.complex().cx()).mFlightTime = 0;
                0
            }
            EE_UNITS_EU_KEEP_COLLISION_COUNT => {
                let cx = u.complex().cx();
                (*cx).mF24C = (*cx).mCollisionResolutionCount;
                0
            }
            EE_UNITS_EU_CLEAR_COLLISION_COUNT => {
                (*u.complex().cx()).mCollisionResolutionCount = 0.0;
                0
            }
            EE_UNITS_EU_PLAYER_ID => ulong((*P(u.player(), l).rts()).mPlayerID),
            EE_UNITS_EU_MARK_MOVED => {
                (*u.uf()).mHiddenFlags |= CU_VIEW_HIDDEN_FLAG_MOVED;
                0
            }
            EE_UNITS_EU_HAS_DEATH_SOUND => i64::from(
                !(*at::<EeUnitsUnitClassUpgradeLinks>((*u.uf()).mStats as u64, l.stats_links)).mDeathSound.is_null(),
            ),
            EE_UNITS_EU_CARRIES => i64::from((*u.stats()).mCarryAmount > 0),
            EE_UNITS_EU_SET_RESOURCE_AMOUNT => {
                (*u.uf()).mResourceAmount = b as c_long;
                0
            }
            EE_UNITS_EU_TAKE_RESOURCE => {
                (*u.uf()).mResourceAmount = (*u.uf()).mResourceAmount.wrapping_sub(b as c_long);
                0
            }
            EE_UNITS_EU_RESOURCE_AMOUNT => long((*u.uf()).mResourceAmount),
            EE_UNITS_EU_RESOURCE_AMOUNT_OF => long((*o.uf()).mResourceAmount),
            EE_UNITS_EU_RESOURCE_TYPE_OF => ulong((*o.uf()).mResourceType),
            EE_UNITS_EU_CATEGORY_OF => ulong((*o.class()).mCategory),
            EE_UNITS_EU_UNIT_TYPE_OF => long((*o.class()).mUnitType as c_long),
            EE_UNITS_EU_TILE_SIZE_OF => long((*o.class()).mTileSize),
            EE_UNITS_EU_IS_BUILT_OF => i64::from(flag!(o.complex().cx(), mIsBuilt)),
            EE_UNITS_EU_HIT_POINTS_OF => long((*o.uf()).mHitPoints),
            EE_UNITS_EU_FULL_HEALTH_OF => i64::from((*o.uf()).mHitPoints == (*o.stats()).mMaxHitPoints),
            _ => return None,
        }
    };
    Some(v)
}

/// The answer of `ee_units_eum_host`'s statement `op` (EEUnit.cpp; eu_more.rs's and eu_describe.rs's statements)
/// when it is a plain record read or store (None: the game's host answers it).
///
/// # Safety
/// As the host's: `unit` a live `EEUnit` (or the unit being made), `a` the statement's object.
pub(crate) unsafe fn eum_view(op: i32, unit: u64, a: u64, b: i64) -> Option<i64> {
    use crate::units::eu_describe::*;
    use crate::units::eu_more::*;
    let l = layout();
    let u = U(unit, l);
    let o = U(a, l);
    // SAFETY: the host's contract: each statement reads or writes a member of live objects, as its C++ did
    let v = unsafe {
        match op {
            EE_UNITS_EUM_ZERO_HIT_POINTS => {
                (*u.uf()).mHitPoints = 0;
                0
            }
            EE_UNITS_EUM_SET_HIT_POINTS_FIELD => {
                (*u.uf()).mHitPoints = b as c_long;
                0
            }
            EE_UNITS_EUM_HIT_POINTS_OF => long((*o.uf()).mHitPoints),
            EE_UNITS_EUM_HAS_COMPLEX => i64::from(!(*u.uf()).mComplexUnit.is_null()),
            EE_UNITS_EUM_COPY_RESOURCE_TYPE => {
                (*u.uf()).mResourceType = (*o.uf()).mResourceType;
                0
            }
            EE_UNITS_EUM_COPY_RESOURCE_AMOUNT => {
                (*u.uf()).mResourceAmount = (*o.uf()).mResourceAmount;
                0
            }
            EE_UNITS_EUM_ZERO_RESOURCE_AMOUNT => {
                (*u.uf()).mResourceAmount = 0;
                0
            }
            EE_UNITS_EUM_RESOURCE_TYPE_NONE => {
                (*u.uf()).mResourceType = CU_VIEW_RESOURCE_NONE;
                0
            }
            EE_UNITS_EUM_HAS_RESOURCE => i64::from((*u.uf()).mResourceType != CU_VIEW_RESOURCE_NONE),
            EE_UNITS_EUM_SET_PLAYER => {
                *at::<usize>(unit, l.unit_player) = a as usize;
                0
            }
            EE_UNITS_EUM_SET_CLASS => {
                (*u.uf()).mUnitClass = a as usize as _;
                0
            }
            EE_UNITS_EUM_SET_COMPLEX_SELF => {
                (*u.uf()).mComplexUnit = unit as usize as _;
                0
            }
            EE_UNITS_EUM_CLEAR_CLASS => {
                (*u.uf()).mUnitClass = std::ptr::null_mut();
                0
            }
            EE_UNITS_EUM_CLEAR_COMPLEX => {
                (*u.uf()).mComplexUnit = std::ptr::null_mut();
                0
            }
            EE_UNITS_EUM_CLEAR_HIDDEN => {
                (*u.uf()).mHiddenFlags = 0;
                0
            }
            EE_UNITS_EUM_CLEAR_STATS => {
                (*u.uf()).mStats = std::ptr::null_mut();
                0
            }
            EE_UNITS_EUM_CATEGORY_OF => ulong((*o.class()).mCategory),
            EE_UNITS_EUM_LAST_ATTACKER => h((*u.cx()).mLastAttacker),
            EE_UNITS_EUM_SET_LAST_ATTACKER => {
                (*u.cx()).mLastAttacker = a as usize as _;
                0
            }
            EE_UNITS_EUM_CX_ZERO_DISABLED_REASON => {
                (*u.cx()).mDisabledReason = 0;
                0
            }
            EE_UNITS_EUM_SET_OBJECT_KIND => {
                *at::<u8>(unit, l.unit_object_kind) = b as u8;
                0
            }
            EE_UNITS_EUM_SET_INVALID => {
                *(addr_of!((*u.rts()).mIsValid) as *mut u8) = 0;
                0
            }
            _ => return None,
        }
    };
    Some(v)
}

/// The answer of `ee_units_uh_host`'s statement `op` (EEUnitHost.cpp; uh_body.rs's statements: units by id, other
/// objects by address) when it is a plain record read or store (None: the game's host answers it).
///
/// # Safety
/// As the host's: the ids name units of the world, the objects are live, on the simulation thread.
pub(crate) unsafe fn uh_view(op: i32, a: u64, b: u64, f: &mut [f32; 4]) -> Option<i64> {
    use crate::units::goal_spec::resolve;
    use crate::units::uh_body::*;
    let l = layout();
    // `ComplexOf(a)` / `UnitOf(a)` (the registry's unit of the id a), the object b
    let id = || U(resolve(a as u32), l);
    let o = U(b, l);
    // SAFETY: the host's contract: each statement reads or writes a member of live objects, as its C++ did
    let v = unsafe {
        match op {
            EE_UNITS_UH_TILE_X => long((*id().rts()).mTileX),
            EE_UNITS_UH_TILE_Y => long((*id().rts()).mTileY),
            EE_UNITS_UH_OBJECT_ID => i64::from(low32((*o.rts()).mUnitID)),
            EE_UNITS_UH_SET_SHIFT_QUEUED => {
                (*id().complex().cx()).mHasShiftQueuedBuild = true;
                0
            }
            EE_UNITS_UH_PLAYER_ID => ulong((*P(a, l).rts()).mPlayerID),
            EE_UNITS_UH_UNIT_TYPE_OF => ulong((*o.class()).mUnitType),
            EE_UNITS_UH_COMPLEX_OF_UNIT => o.complex().0 as i64,
            EE_UNITS_UH_COMPLEX | EE_UNITS_UH_UNIT_OF => id().0 as i64,
            EE_UNITS_UH_UNIT_TILE => {
                let unit = id().0;
                if unit == 0 {
                    0
                } else {
                    word(unit, l.unit_tile) as i64
                }
            }
            EE_UNITS_UH_MAX_VELOCITY => {
                f[0] = (*id().stats()).mMaxVelocity;
                0
            }
            EE_UNITS_UH_GOAL_STATE => h((*id().cx()).mGoalState),
            EE_UNITS_UH_GOAL_KIND => i64::from(*at::<u8>(b, l.goal_kind)),
            EE_UNITS_UH_HAS_TILE_NODE => i64::from(word(b, l.unit_tile_node) != 0),
            EE_UNITS_UH_STATS_CREATION_SOUND => {
                h((*at::<EeUnitsUnitClassUpgradeLinks>((*o.uf()).mStats as u64, l.stats_links)).mCreationSound)
            }
            EE_UNITS_UH_CAN_GARRISON => i64::from(flag!(id().class(), mCanGarrison)),
            EE_UNITS_UH_CURRENT_PLAYER_ID => {
                let current = *at::<c_long>(l.world_address, l.world_current_player);
                if !(0..17).contains(&current) {
                    return None;
                }
                ulong((*P(world_player(current as i64 as u64), l).rts()).mPlayerID)
            }
            _ => return None,
        }
    };
    Some(v)
}

/// `gWorld.GetCurrentPlayer()` (None for an index outside the 17 slots, which the C++ read past)
///
/// # Safety
/// The world is live.
unsafe fn current_player() -> Option<u64> {
    let l = layout();
    // SAFETY: the caller's contract
    unsafe {
        let current = *at::<c_long>(l.world_address, l.world_current_player);
        (0..17).contains(&current).then(|| world_player(current as i64 as u64))
    }
}

/// The answer of `ee_units_ai_host`'s statement `op` (EEUnitAction.cpp; ai.rs's statements) when it is a plain
/// record read or store (None: the game's host answers it).
///
/// # Safety
/// As the host's: `unit` the live complex unit stepped, `a` the statement's object.
pub(crate) unsafe fn ai_view(op: i32, unit: u64, a: u64, b: i64) -> Option<i64> {
    use crate::units::ai::*;
    let l = layout();
    let u = U(unit, l);
    // SAFETY: the host's contract: each statement reads or writes a member of live objects, as its C++ did
    let v = unsafe {
        match op {
            EE_UNITS_AI_H_COMMAND_TYPE => u.command_type(),
            EE_UNITS_AI_H_NEXT_TRIGGER_TIME => ulong((*u.cx()).mNextTriggerTime),
            EE_UNITS_AI_H_GOAL_KIND => u.goal_kind().map_or(-1, i64::from),
            EE_UNITS_AI_H_COMMAND => h((*u.cx()).mCommand),
            EE_UNITS_AI_H_HAS_GOAL => i64::from(!(*u.cx()).mGoalState.is_null()),
            EE_UNITS_AI_H_ACTION_TYPE => i64::from(*at::<u32>(a, l.action_type)),
            EE_UNITS_AI_H_SET_COMMAND => {
                (*u.cx()).mCommand = a as usize as _;
                0
            }
            EE_UNITS_AI_H_SET_NEXT_TRIGGER_TIME => {
                (*u.cx()).mNextTriggerTime = b as c_ulong;
                0
            }
            EE_UNITS_AI_H_UNIT_ID => ulong((*u.rts()).mUnitID),
            EE_UNITS_AI_H_IS_CITIZEN => i64::from(flag!(u.cx(), mIsCitizen)),
            EE_UNITS_AI_H_GAME_TIME => {
                let physics = word(l.physics_pointer, 0);
                long((*at::<crate::units::physics_save::EeUnitsPhysicsSaved>(physics, l.physics_saved)).mGameTime)
            }
            _ => return None,
        }
    };
    Some(v)
}

/// The answer of `ee_units_ck_host`'s statement `op` (EECloakingUtilities.cpp; uu_cloak.rs's statements) when it is
/// a plain record read or store (None: the game's host answers it).
///
/// # Safety
/// As the host's: `a` the statement's live object.
pub(crate) unsafe fn ck_view(op: i32, a: u64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    use crate::units::uu_cloak::*;
    let l = layout();
    let o = U(a, l);
    // SAFETY: the host's contract: each statement reads or writes a member of live objects, as its C++ did
    let v = unsafe {
        match op {
            EE_UNITS_CK_COMMAND_TYPE => o.command_type(),
            EE_UNITS_CK_ROT_FRACTION => {
                bits((*o.complex().cx()).mRottingAwayTime as f32 / (*o.class()).mRottingTime as f32)
            }
            EE_UNITS_CK_HAS_ROTTING_TIME => i64::from((*o.class()).mRottingTime != 0),
            EE_UNITS_CK_IS_CLOAKABLE => i64::from(flag!(o.cx(), mIsCloakable)),
            EE_UNITS_CK_IS_CLOAKED => i64::from(flag!(o.cx(), mIsCloaked)),
            EE_UNITS_CK_OPACITY => bits((*o.cx()).mOpacity),
            EE_UNITS_CK_CURRENT_PLAYER => current_player()? as i64,
            EE_UNITS_CK_PLAYER => o.player() as i64,
            EE_UNITS_CK_ENEMY_OF_VIEWER => ulong(P(o.player(), l).enemy_of(P(current_player()?, l))),
            EE_UNITS_CK_OBSERVER => i64::from(flag!(P(current_player()?, l).rts(), mObserverMode)),
            EE_UNITS_CK_SET_FLOAT => {
                *at::<f32>(a, 0) = f[0];
                0
            }
            EE_UNITS_CK_CLOAKED => long((*o.cx()).mCloaked as c_long),
            EE_UNITS_CK_CLEAR_CLOAKED => {
                (*o.cx()).mCloaked = 0;
                0
            }
            EE_UNITS_CK_TILE_X => long((*o.rts()).mTileX),
            EE_UNITS_CK_TILE_Y => long((*o.rts()).mTileY),
            EE_UNITS_CK_COMPLEX_OF => o.complex().0 as i64,
            EE_UNITS_CK_DOMAINS => i64::from((*o.class()).mDomains),
            EE_UNITS_CK_UNIT_TYPE => ulong((*o.class()).mUnitType),
            EE_UNITS_CK_SET_PLANET => {
                (*o.cx()).mPlanetID = c as u16;
                0
            }
            _ => return None,
        }
    };
    Some(v)
}

/// The answer of `ee_units_us_host`'s statement `op` (EEUnitStore.cpp; us_check.rs's and us_fill.rs's statements)
/// when it is a plain record read (None: the game's host answers it).
///
/// # Safety
/// As the host's: `a` the statement's live unit or player.
pub(crate) unsafe fn us_view(op: i32, a: u64, b: i64) -> Option<i64> {
    use crate::units::us_check::*;
    use crate::units::us_fill::*;
    let l = layout();
    let o = U(a, l);
    // SAFETY: the host's contract: each statement reads a member of live objects, as its C++ did
    let v = unsafe {
        match op {
            EE_UNITS_US_PLAYER => world_player(u64::try_from(b).ok().filter(|&i| i < 17)?) as i64,
            EE_UNITS_US_PLAYER_ID => ulong((*P(a, l).rts()).mPlayerID),
            EE_UNITS_US_PLAYER_RTS => a as i64,
            EE_UNITS_US_FIRST_UNIT => word(a, l.player_first_unit) as i64,
            EE_UNITS_US_NEXT_UNIT => word(a, l.unit_next) as i64,
            EE_UNITS_US_UNIT_PLAYER => {
                if *at::<u8>(a, l.unit_object_kind) >= crate::units::kinds::EE_OBJECT_UNIT {
                    o.player() as i64
                } else {
                    0
                }
            }
            EE_UNITS_US_UNIT_ID => i64::from(low32((*o.rts()).mUnitID)),
            EE_UNITS_US_F_OBJECT_KIND => i64::from(*at::<u8>(a, l.unit_object_kind)),
            EE_UNITS_US_F_STATS => h((*o.uf()).mStats),
            EE_UNITS_US_F_LOCATION => bits(o.location(match b {
                0 => 0,
                1 => 1,
                _ => 2,
            })),
            EE_UNITS_US_F_FIELDS => match b {
                0 => o.rts() as i64,
                1 => o.uf() as i64,
                _ => o.cx() as i64,
            },
            EE_UNITS_US_B_RAW_PLAYER => o.player() as i64,
            EE_UNITS_US_B_ENEMY => i64::from(low32(*(*P(a, l).saved()).mEnemies.get(b as usize)?)),
            _ => return None,
        }
    };
    Some(v)
}

/// `mGoalQueues[queue].mGoals[index]` of a unit (None past the queues; 0 past the goals)
fn goal_at(unit: u64, queue: i64, index: usize) -> Option<u64> {
    let q = *goals::unit_queues(unit).get(queue as usize)?;
    // SAFETY: a live queue of the unit
    Some(unsafe { crate::units::goals::ee_units_gq_get(q, index) })
}

/// The answer of `ee_units_wp_host`'s statement `op` (EEComplexUnit.cpp; waypoints.rs's and the path manager's
/// statements on the unit's goals) when the goal queues answer it (None: the game's host answers it).
pub(crate) fn wp_view(op: i32, unit: u64) -> Option<i64> {
    use crate::units::waypoints::*;
    Some(match op {
        EE_UNITS_WP_QUEUE_LENGTH => goals::queue_len(unit, 0)? as i64,
        EE_UNITS_WP_QUEUE_BACK => goal_at(unit, 0, goals::queue_len(unit, 0)?.wrapping_sub(1))? as i64,
        EE_UNITS_WP_REFRESH_GOALS => {
            goals::refresh_goals(unit);
            0
        }
        EE_UNITS_WP_REFRESH_ACTIVE_GOAL => {
            goals::refresh_active_goal(unit);
            0
        }
        _ => return None,
    })
}
