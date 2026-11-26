//! The people's host's statements answered over the world views (the units' lane, round 14): EEUnitClassPeople.cpp's
//! reads and stores of plain members (the goal queue, the timers, the conversion's handles, the can-convert reads),
//! its scratch values (the effect handle, the area conversion's units) kept here as this thread's cells, and the
//! statements whose callee is Rust. A statement that still calls C++ and fills the handle is passed on to
//! `ee_units_uc_host` with the cell's address in `c` (an operand the original left unused). Asked by
//! [`kind_view`](super::uc_kind_view::kind_view) for the people's kind after its plain reads.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::cell::{Cell, RefCell};
use std::os::raw::{c_long, c_ulong, c_void};

use crate::units::uc_body::game::ee_units_uc_host;
use crate::units::uc_view::{complex_mut, Ctx};
use crate::units::ucp_people::*;
use crate::world::unit_fields::{EeUnitsEffectHandle, EeUnitsUnitFields};
use crate::world::view::{ClassView, UnitView};
use crate::world_field;

thread_local! {
    /// the people's effect handle (the effect manager fills it; 0 when it makes none)
    static HANDLE: Cell<EeUnitsEffectHandle> = const { Cell::new(EeUnitsEffectHandle { mValue: 0 }) };
    /// the area conversion's units (`EEGetUnitsNear`'s)
    static UNITS: RefCell<Vec<u64>> = const { RefCell::new(Vec::new()) };
}

/// `kWorldAreaConversionRadius` (rust/data/world_constants.h)
const WORLD_AREA_CONVERSION_RADIUS: u32 = 35;
/// `EEComplexUnit::IsProtected`'s disabled reasons
const DISABLED_PROTECTED: [c_ulong; 2] = [0x1a, 0x1c];
/// `HasEffect(4)`: the shield
const EFFECT_SHIELD: i64 = 4;

fn host(x: &Ctx<'_>, op: i32, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
    let p = |h: u64| h as usize as *mut c_void;
    // SAFETY: the C++ host's statement with the live class, unit and objects the body handed over
    unsafe { ee_units_uc_host(op, p(x.class), p(x.unit), a, b, c, f) }
}

fn handle_cell() -> i64 {
    HANDLE.with(|h| h.as_ptr() as usize as i64)
}

/// A unit's player pointer (0: none), compared as the C++ compares `mPlayer`s.
fn player_of(u: &UnitView<'_>) -> u64 {
    u.player().map_or(0, |p| p.handle())
}

fn unit_mut(u: &UnitView<'_>) -> *mut EeUnitsUnitFields {
    u.fields().ptr() as *mut EeUnitsUnitFields
}

fn class_of<'t>(u: &UnitView<'t>) -> Option<ClassView<'t>> {
    u.class()
}

/// The people's statement `op` when it is one of these.
pub(crate) fn people_view(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let _ = c;
    let (u, o) = (|| x.u(), || x.o());
    Some(match op {
        // `theUnit->mGoalQueues[0].mGoals.size()` and its goal b's kind (an `unsigned char`)
        EE_UNITS_UCP_QUEUE0_LEN | EE_UNITS_UCP_QUEUE0_KIND => {
            let q = u()?.goal_queues().first().copied().unwrap_or(0);
            let goals: Vec<u64> = if q == 0 {
                Vec::new()
            } else {
                // SAFETY: the unit's live Rust goal queue
                unsafe { (*(q as usize as *const crate::units::goals::EeUnitsGoalQueue)).goals.clone() }
            };
            if op == EE_UNITS_UCP_QUEUE0_LEN {
                goals.len() as i64
            } else {
                let goal = goals[b as usize];
                // SAFETY: a live goal's kind byte
                i64::from(unsafe {
                    std::ptr::read((goal as usize + crate::world::layout::layout().goal_kind as usize) as *const u8)
                })
            }
        }
        EE_UNITS_UCP_CLEAR_RESOURCE_AMOUNT => {
            // SAFETY: the unit's member
            unsafe { (*unit_mut(&u()?)).mResourceAmount = 0 };
            0
        }
        EE_UNITS_UCP_SET_WORK_TIMER => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mWorkTimer = b as c_long };
            0
        }
        EE_UNITS_UCP_WORK_TIMER_INT => i64::from(world_field!(u()?.complex(), mWorkTimer) as i32),
        // the tech node a's record's `mMakesUnit`, `mKind`
        EE_UNITS_UCP_NODE_MAKES_UNIT | EE_UNITS_UCP_NODE_KIND => {
            // SAFETY: the live node the body handed over
            let data = unsafe { crate::world::NodeView::of(x.t, x.a) }?.data()?;
            if op == EE_UNITS_UCP_NODE_MAKES_UNIT {
                i64::from(world_field!(data, mMakesUnit))
            } else {
                world_field!(data, mKind) as i64
            }
        }
        // the effect handle: reset, filled by a C++ creation (its address in c), compared, kept
        EE_UNITS_UCP_RESET_HANDLE => {
            HANDLE.set(EeUnitsEffectHandle { mValue: 0 });
            0
        }
        EE_UNITS_UCP_REMOVAL_EFFECT | EE_UNITS_UCP_CREATE_INDICATOR | EE_UNITS_UCP_CREATE_EFFECT => {
            host(x, op, x.a, b, handle_cell(), f)
        }
        EE_UNITS_UCP_HANDLE_DIFFERS => i64::from(u()?.conversion_effect() != HANDLE.get().mValue),
        EE_UNITS_UCP_KEEP_HANDLE => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mConversionEffect = HANDLE.get() };
            0
        }
        EE_UNITS_UCP_CLOAKED_HANDLE => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mCloaked = HANDLE.get().mValue as c_ulong };
            0
        }
        EE_UNITS_UCP_GROW_HANDLE => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mGrowTime = HANDLE.get().mValue as c_ulong };
            0
        }
        EE_UNITS_UCP_NO_CONVERSION_EFFECT => i64::from(u()?.conversion_effect() == 0xffff_ffff),
        EE_UNITS_UCP_SAME_PLAYER => i64::from(player_of(&o()?) == player_of(&u()?)),
        // `gPhysics->mGameTime - mConversionStartTime < (unsigned long)theClass->mMinConversionTime`
        EE_UNITS_UCP_TOO_SOON => {
            let now = x.world()?.game_time() as c_long as c_ulong;
            let start: c_ulong = world_field!(u()?.complex(), mConversionStartTime);
            let min: c_long = world_field!(x.cls()?.fields(), mMinConversionTime);
            i64::from(now.wrapping_sub(start) < min as c_ulong)
        }
        EE_UNITS_UCP_IS_CRUSADER => {
            let class = u()?.class()?.handle();
            i64::from(crate::units::uc_is::ee_units_uc_is(class, crate::units::uc_is::EE_UNITS_UC_IS_CRUSADER))
        }
        EE_UNITS_UCP_UNIT_PLAYER_CURRENT => i64::from(player_of(&u()?) == x.current_player()?),
        EE_UNITS_UCP_PLAYER_CURRENT => i64::from(x.a == x.current_player()?),
        EE_UNITS_UCP_AREA_CONVERSION => i64::from(world_field!(u()?.player()?.fields(), mAreaConversion)),
        // the area conversion's units around f (`ee_units_uu_units_near` with the world table's radius)
        EE_UNITS_UCP_UNITS_CLEAR => {
            UNITS.with(|v| v.borrow_mut().clear());
            0
        }
        EE_UNITS_UCP_UNITS_NEAR => {
            let radius = ee_data::tables::ee_data_world_entry(WORLD_AREA_CONVERSION_RADIUS).mFloatValue;
            let near = crate::units::uu_ffi::units_near([f[0], f[1], f[2]], radius);
            UNITS.with(|v| *v.borrow_mut() = near);
            0
        }
        EE_UNITS_UCP_UNITS_LEN => UNITS.with(|v| v.borrow().len()) as i64,
        EE_UNITS_UCP_UNITS_AT => UNITS.with(|v| v.borrow()[b as usize]) as i64,
        EE_UNITS_UCP_HAS_CLOAKED_EFFECT => i64::from(world_field!(u()?.complex(), mCloaked) as c_long > 0),
        EE_UNITS_UCP_HAS_GROW_EFFECT => i64::from(world_field!(u()?.complex(), mGrowTime) > 0),
        EE_UNITS_UCP_CLEAR_GROW_TIME => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mGrowTime = 0 };
            0
        }
        EE_UNITS_UCP_CAPTURE_ACTIVE => i64::from(crate::app::capture::ee_app_capture_active()),
        EE_UNITS_UCP_TIMER => timer(&u()?, b, f)?,
        EE_UNITS_UCP_SET_TIMER => {
            set_timer(&u()?, b, c, f);
            0
        }
        EE_UNITS_UCP_CAN_CONVERT_READ => can_convert_read(x, b, f)?,
        _ => return None,
    })
}

/// The step timers by ucp_people.rs's index (0 reload, 1 target search, 2 rotting, 3 resource amount, 4 heal search
/// into `f[0]`).
fn timer(u: &UnitView<'_>, which: i64, f: &mut [f32; 4]) -> Option<i64> {
    let c = u.complex();
    Some(match which {
        0 => world_field!(c, mReloadTimer) as i64,
        1 => world_field!(c, mTargetSearchTimer) as i64,
        2 => world_field!(c, mRottingAwayTime) as i64,
        3 => u.resource_amount(),
        4 => {
            f[0] = world_field!(c, mHealSearchTimer);
            0
        }
        _ => 0,
    })
}

fn set_timer(u: &UnitView<'_>, which: i64, value: i64, f: &[f32; 4]) {
    let c = complex_mut(u);
    // SAFETY: the unit's members
    unsafe {
        match which {
            0 => (*c).mReloadTimer = value as c_long,
            1 => (*c).mTargetSearchTimer = value as c_long,
            2 => (*c).mRottingAwayTime = value as c_long,
            3 => (*unit_mut(u)).mResourceAmount = value as c_long,
            4 => (*c).mHealSearchTimer = f[0],
            _ => {}
        }
    }
}

/// `EECanConvert`'s reads of the priest (the unit) and the target `a` (floats in `f[0]`).
fn can_convert_read(x: &Ctx<'_>, which: i64, f: &mut [f32; 4]) -> Option<i64> {
    let (priest, target) = (x.u()?, x.o()?);
    let float = |f: &mut [f32; 4], v: f32| {
        f[0] = v;
        0
    };
    Some(match which {
        EE_UNITS_UCP_CC_CRUSADER => people_view(x, EE_UNITS_UCP_IS_CRUSADER, 0, 0, f)?,
        EE_UNITS_UCP_CC_ADAPTATION => host(x, EE_UNITS_UCP_HAS_ADAPTATION, x.a, 0, 0, f),
        EE_UNITS_UCP_CC_SAME_PLAYER => i64::from(player_of(&target) == player_of(&priest)),
        EE_UNITS_UCP_CC_ENEMY => i64::from(priest.player()?.enemy(target.player()?.id() as usize) != 0),
        EE_UNITS_UCP_CC_CONVERSION_RATE => float(f, world_field!(priest.stats()?.fields(), mConversionRate)),
        EE_UNITS_UCP_CC_TARGET_DISABLED => i64::from(world_field!(target.complex(), mIsDisabled)),
        EE_UNITS_UCP_CC_TARGET_PROTECTED => {
            let reason: c_ulong = world_field!(target.complex(), mDisabledReason);
            i64::from(world_field!(target.complex(), mIsDisabled) && DISABLED_PROTECTED.contains(&reason))
        }
        EE_UNITS_UCP_CC_RESISTANCE => float(f, world_field!(target.stats()?.fields(), mConversionResistance)),
        EE_UNITS_UCP_CC_TARGET_TYPE => class_of(&target)?.unit_type() as i64,
        EE_UNITS_UCP_CC_TARGET_HIT_POINTS => i64::from(target.hit_points() as i32),
        EE_UNITS_UCP_CC_ATTACKABLE => i64::from(!world_field!(class_of(&target)?.fields(), mIsFlare)),
        EE_UNITS_UCP_CC_AMMO_TYPE => world_field!(class_of(&priest)?.fields(), mAmmoType) as i64,
        EE_UNITS_UCP_CC_DAMAGE => {
            let family = class_of(&target)?.family() as c_long as i32;
            let ammo: c_long = world_field!(class_of(&priest)?.fields(), mAmmoType);
            float(f, crate::combat::damage_table::ee_combat_damage_factor(family, ammo as i32))
        }
        EE_UNITS_UCP_CC_TARGET_CATEGORY => class_of(&target)?.category() as i64,
        EE_UNITS_UCP_CC_PRIEST_CATEGORY => class_of(&priest)?.category() as i64,
        EE_UNITS_UCP_CC_CONVERTS_PRIESTS => i64::from(world_field!(priest.stats()?.fields(), mConvertsPriests)),
        EE_UNITS_UCP_CC_POWER => i64::from(priest.power() as i32),
        EE_UNITS_UCP_CC_CONVERTS_BUILDINGS => i64::from(world_field!(priest.stats()?.fields(), mConvertsBuildings)),
        // `IsLandBuilding()`: the kinds' dispatcher's row (uck_dispatch.rs)
        EE_UNITS_UCP_CC_TARGET_LAND_BUILDING => {
            let class = class_of(&target)?.handle() as usize as *mut c_void;
            // SAFETY: the target's live class, as `EEUnitClass::IsLandBuilding` passes it
            i64::from(
                unsafe {
                    crate::units::uck_dispatch::game::ee_units_uck_dispatch(
                        crate::units::cls_dispatch::EE_UNITS_BODY_IS_LAND_BUILDING,
                        class,
                        std::ptr::null_mut(),
                        0,
                        0,
                        0,
                        std::ptr::null_mut(),
                    )
                } != 0,
            )
        }
        EE_UNITS_UCP_CC_EPOCH => priest.player()?.tech_tree()?.current_epoch(),
        EE_UNITS_UCP_CC_RANGE => float(f, priest.stats()?.range()),
        EE_UNITS_UCP_CC_PRIEST_CYBER => i64::from(world_field!(class_of(&priest)?.fields(), mIsCyber)),
        EE_UNITS_UCP_CC_TARGET_CYBER => i64::from(world_field!(class_of(&target)?.fields(), mIsCyber)),
        // `HasEffect(4)` (u12a_small.rs)
        EE_UNITS_UCP_CC_SHIELDED => {
            let p = target.handle() as usize as *mut c_void;
            // SAFETY: the member's entry point on the live target
            i64::from(
                unsafe {
                    crate::units::u12a_small::game::ee_units_cu_tiny(
                        crate::units::u12a_small::EE_UNITS_CU_T_DO_HAS_EFFECT,
                        p,
                        std::ptr::null(),
                        EFFECT_SHIELD,
                        0,
                        0.0,
                        0.0,
                        0.0,
                    )
                } != 0,
            )
        }
        _ => 0,
    })
}
