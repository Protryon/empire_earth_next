//! The class host's plain statements over the world views (the units' lane, round 13): each `EE_UNITS_UC_*`
//! statement that reads or stores one member of a Rust record (the class's `EeUnitsUnitClassFields` and
//! `EeUnitsUnitClassLinks`, the db row's `DbUnitClass`, the unit's records, its stats', its player's, its tile's,
//! the world's and the physics'), or that calls a Rust function, answered here before the C++ host is asked. Their
//! cases left EEUnitClass.cpp's `ee_units_uc_host`. Every `UcHost` of the game (the class host's and the kinds'
//! hosts', whose own cases never take an `EE_UNITS_UC_*` value) asks [`uc_view`] first.
//!
//! The conversions are the C++ returns': a `long` sign-extends, an `unsigned long` zero-extends, a bool is 0 or 1,
//! a pointer is its address, a float goes out in `f`.

#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_anim::*;
use crate::units::uc_body::*;
use crate::units::uc_class::*;
use crate::units::uc_create::*;
use crate::units::uc_media::*;
use crate::units::uc_produce::*;
use crate::units::uc_rest::*;
use crate::world::layout::layout;
use crate::world::stats_fields::EeUnitsUnitClassUpgradeLinks;
use crate::world::unit_class_fields::{EeUnitsUnitClassFields, EeUnitsUnitClassLinks};
use crate::world::unit_fields::EeUnitsComplexUnitFields;
use crate::world::view::{ClassView, PlayerView, Record, StatsView, TileView, UnitView, WorldRead, WorldView};
use crate::world_field;
use ee_data::tables::DbUnitClass;
use std::os::raw::{c_long, c_ulong};

/// `EEUnitClass`'s third base (its pointer members).
pub(crate) fn links<'t>(class: &ClassView<'t>) -> Record<'t, EeUnitsUnitClassLinks> {
    Record::at_offset(class.handle() as usize, layout().class_links)
}

/// `EEUnitClassUpgrade`'s first base (its pointer members).
pub(crate) fn stats_links<'t>(stats: &StatsView<'t>) -> Record<'t, EeUnitsUnitClassUpgradeLinks> {
    Record::at_offset(stats.handle() as usize, layout().stats_links)
}

/// The pointer member at `object + offset` (0 for no object).
///
/// # Safety
/// `object` is 0 or a live object with a pointer member there.
pub(crate) unsafe fn word(object: u64, offset: u64) -> u64 {
    if object == 0 {
        return 0;
    }
    // SAFETY: the caller's contract
    unsafe { std::ptr::read((object as usize + offset as usize) as *const usize) as u64 }
}

/// A complex unit's record, for a store.
pub(crate) fn complex_mut(u: &UnitView<'_>) -> *mut EeUnitsComplexUnitFields {
    u.complex().ptr() as *mut EeUnitsComplexUnitFields
}

/// A class's record, for a store.
pub(crate) fn class_mut(c: &ClassView<'_>) -> *mut EeUnitsUnitClassFields {
    c.fields().ptr() as *mut EeUnitsUnitClassFields
}

/// A class's pointer members, for a store.
pub(crate) fn links_mut(c: &ClassView<'_>) -> *mut EeUnitsUnitClassLinks {
    links(c).ptr() as *mut EeUnitsUnitClassLinks
}

pub(crate) fn long(v: c_long) -> i64 {
    v as i64
}

pub(crate) fn ulong(v: c_ulong) -> i64 {
    v as u64 as i64
}

pub(crate) fn obj<T>(p: *mut T) -> i64 {
    p as usize as i64
}

/// The host a body's statements go to: the class host's, or a kind's (whose own statements are tried first).
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub(crate) enum Kind {
    Class,
    Air,
    Buildings,
    Ground,
    Kinds,
    People,
    Shots,
    Transports,
}

/// A statement's objects over one world read: the class, its unit and the object `a`.
pub(crate) struct Ctx<'t> {
    pub(crate) t: &'t WorldRead,
    pub(crate) class: u64,
    pub(crate) unit: u64,
    pub(crate) a: u64,
}

impl<'t> Ctx<'t> {
    pub(crate) fn cls(&self) -> Option<ClassView<'t>> {
        // SAFETY: the live class the C++ member handed over (the world held)
        unsafe { ClassView::of(self.t, self.class) }
    }
    pub(crate) fn u(&self) -> Option<UnitView<'t>> {
        // SAFETY: its live unit
        unsafe { UnitView::of(self.t, self.unit) }
    }
    /// `a` as a unit
    pub(crate) fn o(&self) -> Option<UnitView<'t>> {
        // SAFETY: the live unit the body handed over
        unsafe { UnitView::of(self.t, self.a) }
    }
    /// `a` as a class
    pub(crate) fn oc(&self) -> Option<ClassView<'t>> {
        // SAFETY: the live class the body handed over
        unsafe { ClassView::of(self.t, self.a) }
    }
    /// `a` as a player
    pub(crate) fn op(&self) -> Option<PlayerView<'t>> {
        // SAFETY: the live player the body handed over
        unsafe { PlayerView::of(self.t, self.a) }
    }
    pub(crate) fn world(&self) -> Option<WorldView<'t>> {
        WorldView::global(self.t)
    }
    /// `gWorld.GetCurrentPlayer()`'s handle (`mPlayers[mCurrentPlayer]`)
    pub(crate) fn current_player(&self) -> Option<u64> {
        let w = self.world()?;
        Some(w.player(w.current_player_index() as usize).map_or(0, |p| p.handle()))
    }
}

/// `u->mPlayer->mAirTraffic[u->mUnitClass->mAirTrafficType - 1]` (an `EeUnitsAtc*`).
pub(crate) fn air_traffic(u: &UnitView<'_>) -> Option<u64> {
    let player = u.player()?.handle();
    let kind: c_long = world_field!(u.class()?.fields(), mAirTrafficType);
    let at = layout().player_air_traffic.wrapping_add(((kind as i64 - 1) * std::mem::size_of::<usize>() as i64) as u64);
    // SAFETY: the player's air traffic pointer for the class's kind (1..4: an airport class's)
    Some(unsafe { word(player, at) })
}

/// The statement `op` of a body's host `kind` when it is a plain one (`class` the class, `unit` its unit, `a` an
/// object, `b` and `c` values, `f` floats in and out); None for the C++ host's.
#[allow(clippy::too_many_arguments)]
pub(crate) fn uc_view(
    kind: Kind,
    op: i32,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    f: &mut [f32; 4],
) -> Option<i64> {
    // SAFETY: the class bodies run inside the C++ member that handed over its live class, unit and objects (the
    // world held); every handle below is one of those or a member of one
    let t = unsafe { WorldRead::begin() };
    let x = Ctx { t: &t, class, unit, a };
    if kind != Kind::Class {
        if let Some(v) = crate::units::uc_kind_view::kind_view(kind, &x, op, b, c, f) {
            return Some(v);
        }
    }
    if let Some(v) = crate::units::uc_fwd_view::fwd_view(kind, &x, op, b, c, f) {
        return Some(v);
    }
    if let Some(v) = crate::units::uc_scratch_view::scratch_view(&x, op, b, c, f) {
        return Some(v);
    }
    base(&x, op, b, c, f)
}

/// The class host's own statements.
fn base(x: &Ctx<'_>, op: i32, b: i64, c: i64, f: &mut [f32; 4]) -> Option<i64> {
    let (unit, a) = (x.unit, x.a);
    let t = x.t;
    let cls = || x.cls();
    let u = || x.u();
    let o = || x.o();
    let oc = || x.oc();
    let row = |p: u64| p as usize as *const DbUnitClass;
    let world = || x.world();
    Some(match op {
        // the constructor's (the unit is the class's db row)
        EE_UNITS_UC_SET_PHYSICS_MODEL => {
            // SAFETY: the class under construction's member
            unsafe { (*links_mut(&cls()?)).mPhysicsModel = a as usize as *mut _ };
            0
        }
        // SAFETY: the row the constructor was handed
        EE_UNITS_UC_RECORD_ATTRIBUTES => i64::from(unsafe { (*row(unit)).mNumAttributes }),
        // SAFETY: as above (kMaxLineOfSight)
        EE_UNITS_UC_LOS_TOO_FAR => i64::from(unsafe { (*row(unit)).mLineOfSight } > 32.0),
        EE_UNITS_UC_SET_EFFECT => {
            // SAFETY: the class's effect slot b (below 29: the constructor's loop)
            unsafe { *(*links_mut(&cls()?)).effect_slot(b as usize) = a as usize as *mut _ };
            0
        }
        EE_UNITS_UC_AI_SCRIPT => obj(world_field!(links(&cls()?), mAIScript)),
        EE_UNITS_UC_CLASS_FILL => {
            // SAFETY: the class's record base, its db row and the fill's inputs (the constructor's)
            unsafe {
                crate::units::unit_class_fields::ee_units_unit_class_fill(
                    class_mut(&cls()?),
                    unit as usize as *const std::ffi::c_void,
                    a as usize as *const _,
                )
            };
            0
        }
        // CreateUnitClasses' (a the row or the class)
        // SAFETY: a dbobjects row
        EE_UNITS_UC_RECORD_ENABLED => i64::from(unsafe { (*row(a)).mIsEnabled }),
        // SAFETY: as above
        EE_UNITS_UC_RECORD_TYPE => i64::from(unsafe { (*row(a)).mUnitType }),
        // SAFETY: as above
        EE_UNITS_UC_RECORD_CATEGORY => i64::from(unsafe { (*row(a)).mCategory }),
        // SAFETY: as above (k below 6)
        EE_UNITS_UC_RECORD_RELATED => {
            i64::from(unsafe { crate::world::unit_class_fields::ee_units_uc_record_related(row(a), b as i32) })
        }
        EE_UNITS_UC_SET_RELATED => {
            let to = ClassView::by_index(t, c as usize).map_or(0, |k| k.handle());
            // SAFETY: class a's related slot b (below 6)
            unsafe { *(*links_mut(&oc()?)).related_slot(b as usize) = to as usize as *mut _ };
            0
        }
        // Heal's
        EE_UNITS_UC_HEAL_EFFECT => obj(world_field!(links(&u()?.class()?), mHealEffect)),
        EE_UNITS_UC_HEAL_HANDLE => i64::from(u()?.heal_effect()),
        EE_UNITS_UC_SET_HEAL_HANDLE => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mHealEffect.mValue = b as u32 };
            0
        }
        // ProduceUnit's (a a class or a unit)
        EE_UNITS_UC_CLASS_UNIT_TYPE => long(oc()?.unit_type() as c_ulong as c_long),
        EE_UNITS_UC_CLASS_POPULATION_COST => long(world_field!(oc()?.fields(), mWeight)),
        EE_UNITS_UC_PRODUCER_TYPE => long(u()?.class()?.unit_type() as c_ulong as c_long),
        EE_UNITS_UC_CLASS_CATEGORY | EE_UNITS_UC_CLASS_CATEGORY_FIELD => oc()?.category() as i64,
        EE_UNITS_UC_CLASS_AIR_TRAFFIC => long(world_field!(oc()?.fields(), mAirTrafficType)),
        EE_UNITS_UC_CLASS_WEIGHT => long(world_field!(oc()?.fields(), mWeight)),
        EE_UNITS_UC_TRANSPORT_CAPACITY => long(world_field!(u()?.class()?.fields(), mTransportCapacity)),
        EE_UNITS_UC_CARGO_ROOM => long(world_field!(u()?.complex(), mCargoRoom)),
        EE_UNITS_UC_PARATROOPER_BOMBER => i64::from(world_field!(o()?.class()?.fields(), mIsParatrooperBomber)),
        EE_UNITS_UC_CLASS_FAMILY => oc()?.family() as i64,
        EE_UNITS_UC_THIS_UNIT_TYPE => cls()?.unit_type() as i64,
        // round 9's bodies'
        EE_UNITS_UC_R_LINE_OF_SIGHT => {
            let u = u()?;
            let sight: f32 = world_field!(u.stats()?.fields(), mLineOfSight);
            f[0] = sight + u.sight();
            0
        }
        EE_UNITS_UC_R_CRUISE_ALTITUDE => {
            f[0] = world_field!(cls()?.fields(), mCruiseAltitude);
            0
        }
        EE_UNITS_UC_R_CATEGORY => cls()?.category() as i64,
        EE_UNITS_UC_R_CATEGORY_OF | EE_UNITS_UC_UNIT_CATEGORY => o()?.class()?.category() as i64,
        EE_UNITS_UC_R_LAUNCHER_CATEGORY => {
            // SAFETY: the unit's launcher class
            unsafe { ClassView::of(t, u()?.launcher_class_handle()) }?.category() as i64
        }
        EE_UNITS_UC_R_HEIGHT | EE_UNITS_UC_HEIGHT_FIELD => {
            f[0] = u()?.height();
            0
        }
        EE_UNITS_UC_R_HIGH_GROUND_BONUS => {
            f[0] = world_field!(u()?.player()?.saved(), mHighGroundBonus);
            0
        }
        EE_UNITS_UC_R_TILE_X_OF | EE_UNITS_UC_UNIT_TILE_X => o()?.tile_xy().0,
        EE_UNITS_UC_R_TILE_Y_OF | EE_UNITS_UC_UNIT_TILE_Y => o()?.tile_xy().1,
        EE_UNITS_UC_R_TILE_HEIGHT_OF => {
            f[0] = o()?.tile()?.height();
            0
        }
        EE_UNITS_UC_R_HAS_TILE_NODE => i64::from(o()?.has_tile_node()),
        EE_UNITS_UC_R_ORBITAL_COUNT => ulong(world_field!(u()?.complex(), mNumOrbitals)),
        EE_UNITS_UC_R_RTS_TILE_OF => o()?.tile().map_or(0, |k| k.handle() as i64),
        EE_UNITS_UC_R_TABLE_GET | EE_UNITS_UC_CLASS_AT => {
            ClassView::by_index(t, b as usize).map_or(0, |k| k.handle() as i64)
        }
        EE_UNITS_UC_R_CLASS_COUNT | EE_UNITS_UC_CLASS_COUNT => ClassView::count(t) as c_ulong as i64,
        EE_UNITS_UC_R_UNIT_REMOVED => {
            let p = class_mut(&cls()?);
            // SAFETY: the class's member
            unsafe {
                (*p).mNumUnits -= 1;
                long((*p).mNumUnits)
            }
        }
        EE_UNITS_UC_NUM_UNITS_INC => {
            // SAFETY: as above
            unsafe { (*class_mut(&cls()?)).mNumUnits += 1 };
            0
        }
        EE_UNITS_UC_R_ACTIVE_GROUP => u()?.active_group_handle() as i64,
        EE_UNITS_UC_R_STATS_INVERSE_VELOCITY => {
            f[0] = world_field!(u()?.stats()?.fields(), mInverseMaxVelocity);
            0
        }
        EE_UNITS_UC_R_CAMOUFLAGE_TIMER => ulong(world_field!(u()?.complex(), mCamouflageTimer)),
        EE_UNITS_UC_R_SET_CAMOUFLAGE_TIMER => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mCamouflageTimer = b as c_ulong };
            0
        }
        EE_UNITS_UC_R_NEXT_PART => o()?.next_part_handle() as i64,
        EE_UNITS_UC_R_PREVIOUS_PART => o()?.previous_part_handle() as i64,
        // SAFETY: an EETile
        EE_UNITS_UC_R_TERRAIN_OF | EE_UNITS_UC_TILE_TERRAIN => unsafe { TileView::of(t, a) }?.terrain_type(),
        EE_UNITS_UC_COMPLEX_OF => o()?.complex_unit_handle() as i64,
        EE_UNITS_UC_CONTAINER_OF => o()?.container_handle() as i64,
        EE_UNITS_UC_CLASS_DOMAINS => i64::from(world_field!(oc()?.fields(), mDomains)),
        EE_UNITS_UC_TILE_SIZE => cls()?.tile_size(),
        EE_UNITS_UC_CAN_GARRISON => i64::from(world_field!(o()?.class()?.fields(), mCanGarrison)),
        EE_UNITS_UC_ACTIVE_GOAL => o()?.active_goal_handle() as i64,
        EE_UNITS_UC_OUTER_SPHERE => {
            let r: f32 = world_field!(o()?.class()?.fields(), mOuterCollisionSphere);
            i64::from(r.to_bits())
        }
        EE_UNITS_UC_GOAL_STATE => o()?.goal_handle() as i64,
        EE_UNITS_UC_CREATION_SOUND => obj(world_field!(stats_links(&o()?.stats()?), mCreationSound)),
        EE_UNITS_UC_PRODUCER_TILE_X | EE_UNITS_UC_TILE_X => u()?.tile_xy().0,
        EE_UNITS_UC_PRODUCER_TILE_Y => u()?.tile_xy().1,
        // UpdateBuilding's
        EE_UNITS_UC_DISABLED => i64::from(world_field!(u()?.complex(), mIsDisabled)),
        EE_UNITS_UC_HIT_POINTS | EE_UNITS_UC_GET_HIT_POINTS => u()?.hit_points(),
        EE_UNITS_UC_PRODUCTION_TIME => long(world_field!(u()?.complex(), mProductionTime)),
        EE_UNITS_UC_TICK_MS => world()?.tick_ms(),
        EE_UNITS_UC_INSTANT_BUILD => i64::from(world_field!(u()?.player()?.fields(), mInstantBuild)),
        EE_UNITS_UC_COMMAND_KIND | EE_UNITS_UC_COMMAND_TYPE => i64::from(u()?.command_type()),
        EE_UNITS_UC_NEXT_PART => (if a != 0 { o() } else { u() })?.next_part_handle() as i64,
        // UpdateAnimal's
        EE_UNITS_UC_FRAME => u()?.model_frame() as i64,
        EE_UNITS_UC_HAS_PHYSICS_MODEL => i64::from(!world_field!(links(&cls()?), mPhysicsModel).is_null()),
        // RefreshSound's
        EE_UNITS_UC_HAS_SOUND => i64::from(!world_field!(u()?.complex(), mSound).is_null()),
        EE_UNITS_UC_TICKS_DONE => ulong(world_field!(world()?.physics()?, mNumTicksDone)),
        EE_UNITS_UC_UNIT_ID => i64::from(u()?.id()),
        // CreateDeathUnit's
        EE_UNITS_UC_DEATH_SPAWN_CLASS => obj(world_field!(links(&cls()?), mDeathSpawnClass)),
        EE_UNITS_UC_CONTAINER => u()?.container_handle() as i64,
        EE_UNITS_UC_LOCATION => {
            let p = o()?.location();
            f[..3].copy_from_slice(&p);
            0
        }
        EE_UNITS_UC_MUZZLE_FLASH => obj(world_field!(links(&o()?.class()?), mMuzzleFlashEffect)),
        // the creations', launches' and deaths'
        EE_UNITS_UC_IS_COMPLEX => long(world_field!(cls()?.fields(), mIsComplex)),
        EE_UNITS_UC_PITCHES => i64::from(world_field!(cls()?.fields(), mPitchesWithTerrain)),
        EE_UNITS_UC_ROLLS => i64::from(world_field!(cls()?.fields(), mRollsWithTerrain)),
        EE_UNITS_UC_WORLD_MAP => world()?.map().map_or(0, |m| m.handle() as i64),
        EE_UNITS_UC_PLAYER_AT => world()?.player(b as usize).map_or(0, |p| p.handle() as i64),
        EE_UNITS_UC_ATTACHED_EFFECT => obj(world_field!(links(&cls()?), mAttachedEffect)),
        EE_UNITS_UC_CLASS_TILE_SIZE => oc()?.tile_size(),
        EE_UNITS_UC_PLAYER_ID => i64::from(x.op()?.id()),
        EE_UNITS_UC_REMAINS_CLASS => obj(world_field!(links(&cls()?), mRemainsClass)),
        EE_UNITS_UC_UNIT_PLAYER_ID => i64::from(u()?.player()?.id()),
        EE_UNITS_UC_PROJECTILE_CLASS => cls()?.projectile_class_handle() as i64,
        EE_UNITS_UC_CREATION_EFFECT => obj(world_field!(links(&cls()?), mCreationEffect)),
        EE_UNITS_UC_VELOCITY_FIELD | EE_UNITS_UC_GET_VELOCITY => {
            f[0] = world_field!(u()?.complex(), mVelocity);
            0
        }
        EE_UNITS_UC_SPECIAL_ATTACK => i64::from(world_field!(u()?.complex(), mIsSpecialAttack)),
        EE_UNITS_UC_HAS_SPECIAL_ATTACK => i64::from(world_field!(cls()?.fields(), mHasSpecialAttack)),
        EE_UNITS_UC_WEAPON_TAG => ulong(world_field!(u()?.complex(), mWeaponTagPoint)),
        EE_UNITS_UC_STATS_SCALE | EE_UNITS_UC_STATS_SCALE_FIELD => {
            f[0] = u()?.stats()?.scale();
            0
        }
        EE_UNITS_UC_CLEAR_SPECIAL => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mIsSpecialAttack = false };
            0
        }
        EE_UNITS_UC_CLASS_OF => o()?.class().map_or(0, |k| k.handle() as i64),
        EE_UNITS_UC_TURRET_ANGLE => {
            let angles: *mut f32 = world_field!(u()?.complex(), mTurretAngles);
            // SAFETY: the unit's turret angle b (one per turret part)
            f[0] = unsafe { *angles.add(b as usize) };
            0
        }
        EE_UNITS_UC_DEATH_CLASS => obj(world_field!(links(&cls()?), mDeathClass)),
        EE_UNITS_UC_UNIT_TILE => u()?.tile().map_or(0, |k| k.handle() as i64),
        EE_UNITS_UC_DEATH_EFFECT => obj(world_field!(links(&cls()?), mDeathEffect)),
        EE_UNITS_UC_DEATH_SOUND => obj(world_field!(stats_links(&u()?.stats()?), mDeathSound)),
        EE_UNITS_UC_UNIT_IS_WALL => i64::from(u()?.class()?.wall()),
        // SAFETY: the unit's player's pointer member
        EE_UNITS_UC_COMPUTER_PLAYER => unsafe { word(u()?.player()?.handle(), layout().player_computer_player) as i64 },
        EE_UNITS_UC_GOAL_STATE_SELF => u()?.goal_handle() as i64,
        EE_UNITS_UC_CAN_FLY => i64::from(world_field!(u()?.class()?.fields(), mCanFly)),
        EE_UNITS_UC_TILE_HEIGHT => {
            f[0] = u()?.tile()?.height();
            0
        }
        // the per-tick bodies'
        EE_UNITS_UC_ANIMATION => ulong(world_field!(u()?.complex(), mAnimation)),
        EE_UNITS_UC_PAUSED => i64::from(world_field!(u()?.complex(), mIsAnimationPaused)),
        EE_UNITS_UC_CAN_FIRE_OVER_WALLS => i64::from(world_field!(u()?.stats()?.fields(), mCanFireOverWalls)),
        EE_UNITS_UC_ANIMATION_TIME => ulong(world_field!(u()?.complex(), mAnimationTime)),
        EE_UNITS_UC_MAX_VELOCITY => {
            f[0] = world_field!(cls()?.fields(), mMaxVelocity);
            0
        }
        EE_UNITS_UC_MAX_ANGULAR_VELOCITY => {
            f[0] = world_field!(cls()?.fields(), mMaxAngularVelocity);
            0
        }
        EE_UNITS_UC_SOUND_INSTANCE => obj(world_field!(u()?.complex(), mSound)),
        EE_UNITS_UC_IS_HORSE => i64::from(world_field!(cls()?.fields(), mIsHorse)),
        EE_UNITS_UC_FRACTION => {
            f[0] = world_field!(u()?.complex(), mAnimationFraction);
            0
        }
        EE_UNITS_UC_SET_FRACTION => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mAnimationFraction = f[0] };
            0
        }
        EE_UNITS_UC_TURRET_RATE => {
            f[0] = world_field!(u()?.stats()?.fields(), mTurretMaxAngularVelocity);
            0
        }
        EE_UNITS_UC_TICK_SECONDS => {
            f[0] = world_field!(world()?.physics()?, mTickSeconds);
            0
        }
        EE_UNITS_UC_TRAIL_TIMER => long(world_field!(u()?.complex(), mTrailEffectTimer)),
        EE_UNITS_UC_WAKE_TIMER => long(world_field!(u()?.complex(), mWakeTimer)),
        EE_UNITS_UC_SET_WAKE_TIMER => {
            // SAFETY: the unit's member
            unsafe { (*complex_mut(&u()?)).mWakeTimer = b as c_long };
            0
        }
        EE_UNITS_UC_HEALING_POINTS => long(world_field!(u()?.stats()?.fields(), mHealingPoints)),
        EE_UNITS_UC_CARGO_COUNT => u()?.cargo_len() as c_ulong as i64,
        EE_UNITS_UC_WORLD_IN_CINEMATIC => i64::from(world()?.in_cinematic()),
        EE_UNITS_UC_IS_CURRENT_PLAYER | EE_UNITS_UC_PLAYER_IS_CURRENT | EE_UNITS_UC_PRODUCER_IS_CURRENT => {
            let current = x.current_player()?;
            i64::from(u()?.player().map_or(0, |p| p.handle()) == current)
        }
        // SAFETY: the unit's pointer member
        EE_UNITS_UC_TILE_NODE => unsafe { word(o()?.handle(), layout().unit_tile_node) as i64 },
        EE_UNITS_UC_UNIT_REGIONS => world()?.map()?.unit_regions() as usize as i64,
        EE_UNITS_UC_TILE_HEIGHT_OF => {
            // SAFETY: an EEMapTile
            f[0] = unsafe { TileView::of(t, a) }?.height();
            0
        }
        // the media and lifetime bodies'
        EE_UNITS_UC_ACKNOWLEDGE_SOUND => {
            let s = world_field!(links(&cls()?), mAcknowledgeSounds);
            obj(s[b as usize])
        }
        EE_UNITS_UC_SELECTION_SOUND => {
            let s = world_field!(links(&cls()?), mSelectionSounds);
            obj(s[b as usize])
        }
        EE_UNITS_UC_CLASS_DEATH_SOUND => obj(world_field!(links(&cls()?), mDeathSound)),
        EE_UNITS_UC_CLASS_CREATION_SOUND => obj(world_field!(links(&cls()?), mCreationSound)),
        EE_UNITS_UC_CLASS_MODEL => obj(world_field!(links(&cls()?), mModel)),
        EE_UNITS_UC_CLASS_BUTTON => obj(world_field!(links(&cls()?), mButton)),
        EE_UNITS_UC_COMMAND => {
            let commands: [c_ulong; 9] = world_field!(cls()?.fields(), mCommands);
            ulong(commands[b as usize])
        }
        EE_UNITS_UC_RELATED_CLASS => {
            // SAFETY: the class's related slot b (below 6)
            obj(unsafe { *(*links_mut(&cls()?)).related_slot(b as usize) })
        }
        EE_UNITS_UC_CLASS_TYPE_FIELD => oc()?.unit_type() as i64,
        EE_UNITS_UC_EFFECT_AT => cls()?.effect_handle(b as usize) as i64,
        EE_UNITS_UC_CLEAR_AI_SCRIPT => {
            // SAFETY: the class's member
            unsafe { (*links_mut(&cls()?)).mAIScript = std::ptr::null_mut() };
            0
        }
        EE_UNITS_UC_CLEAR_RELATED => {
            // SAFETY: the class's related slot b (below 6)
            unsafe { *(*links_mut(&cls()?)).related_slot(b as usize) = std::ptr::null_mut() };
            0
        }
        _ => return None,
    })
}
