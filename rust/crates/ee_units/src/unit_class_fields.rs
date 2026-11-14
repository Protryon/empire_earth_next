//! `EEUnitClass`'s plain members as a Rust record the C++ class derives from (R5b.4, the twenty-seventh round):
//! `class EEUnitClass : public UReferenceCounter, public EeUnitsUnitClassFields` (ee/EEUnit.h), as the units'
//! records of unit_fields.rs. The fields keep their C++ names and types (`c_long`/`c_ulong` are the C++ `long`/
//! `unsigned long` on every data model) and their order; `mAirTrafficType` and `mCommands` (+0x28, +0x3c in the
//! original, between the names and the research list) come first, then the block from `mCruiseAltitude` (+0x11c)
//! to `mIsParatrooperBomber` (+0x326). The volatile `mIsMediaLoaded` stays a C++ member; the never-read `mB329` and
//! `mS32A` are gone.
//!
//! The constructor's copies from the database row (`EEUnitClassRecord`, ee_data's generated record) are [`fill`]:
//! the original's statements in order, with what the C++ looks up elsewhere (the tech tree's epoch, the calamity
//! technology, the animal id) passed in [`EeUnitsUnitClassFillIn`] and the patrol action found in the class's
//! Rust script (script.rs).

#![allow(non_snake_case)]

use ee_data::generated::specs_gen::records::EEUnitClassRecord;
use std::os::raw::{c_long, c_ulong};

/// `EEUnitClass`'s plain members (ee/EEUnit.h), in the class's order.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUnitClassFields {
    pub mAirTrafficType: c_long,
    /// the command buttons (30: formation)
    pub mCommands: [c_ulong; 9],
    pub mCruiseAltitude: f32,
    pub mMinRange: f32,
    pub mRange: f32,
    pub mAreaEffect: f32,
    pub mAreaEffectScale: f32,
    pub mScale: f32,
    pub mMaxVelocity: f32,
    pub mF138: f32,
    pub mAcceleration: f32,
    pub mAngularAcceleration: f32,
    pub mMaxAngularVelocity: f32,
    pub mCostScale: f32,
    pub mTurretMaxAngularVelocity: f32,
    pub mBuildTimeScale: f32,
    pub mGarrisonBonus: f32,
    pub mMaxRoll: f32,
    pub mFacingOffset: f32,
    pub mFiringArc: f32,
    pub mLineOfSight: f32,
    pub mSelectionSize: f32,
    pub mShadowScale: f32,
    pub mL170: f32,
    pub mOuterCollisionSphere: f32,
    pub mInnerCollisionSphere: f32,
    pub mLaunchPitch: f32,
    pub mL180: c_long,
    pub mConversionResistance: f32,
    pub mConversionRate: f32,
    pub mSlopeSpeedPenalty: f32,
    pub mBobHeight: f32,
    pub mBobPitch: f32,
    pub mFriendlyDamageFactor: f32,
    pub mF19C: f32,
    pub mHealThreshold: f32,
    pub mHealRange: f32,
    pub mHealDelay: f32,
    pub mLoadRange: f32,
    pub mUnloadRange: f32,
    pub mIsComplex: c_long,
    pub mCategory: c_ulong,
    pub mUnitType: c_ulong,
    pub mCombatReaction: c_long,
    pub mWeapon: c_long,
    pub mFamily: c_ulong,
    pub mAmmoType: c_long,
    pub mAIKind: c_ulong,
    pub mCounterKind1: c_ulong,
    pub mCounterKind2: c_ulong,
    pub mCounterKind3: c_ulong,
    pub mKindFlags: c_long,
    pub mTechnology: c_long,
    pub mGarrisonTechnology: c_long,
    pub mWaterKind: c_long,
    /// the base armour per weapon kind
    pub mArmor: [c_long; 6],
    pub mL208: c_long,
    pub mEpoch: c_long,
    pub mReloadTime: c_long,
    pub mAttack: c_long,
    pub mWorkRate: c_long,
    pub mGatherTimes: [c_long; 7],
    pub mFlightTime: c_long,
    pub mLeadership: c_long,
    pub mMaxObjectCarry: c_long,
    pub mInfluence: c_long,
    pub mRottingTime: c_long,
    pub mTransportCapacity: c_long,
    pub mGarrisonCapacity: c_long,
    pub mRepairRate: c_long,
    pub mWeight: c_long,
    pub mHitPoints: c_long,
    pub mNameStringID: c_long,
    pub mTargetSearchInterval: c_long,
    pub mResourceType: c_long,
    pub mResourceAmount: c_long,
    pub mCarryAmount: c_long,
    pub mHealingPoints: c_long,
    pub mRegeneration: c_long,
    pub mPowerRegeneration: c_long,
    pub mPower: c_long,
    pub mL284: c_long,
    pub mMissileDelayTime: c_long,
    pub mTileSize: c_long,
    pub mCloakRadius: c_long,
    pub mL294: c_long,
    pub mOverlapUnitSet: c_long,
    pub mAmmunition: c_long,
    pub mNumUnits: c_long,
    pub mLastUnitGoneTime: c_long,
    pub mMiniMapColor: c_long,
    pub mInitialGarrisonCount: c_long,
    pub mWonderBonus: c_long,
    pub mWonderUnitSet: c_long,
    pub mMinConversionTime: c_long,
    /// the area-effect template ids
    pub mAreaEffectTemplates: [c_long; 4],
    pub mAnimalID: c_long,
    /// the movement domains (land 2, air 1, sea 4, space 8)
    pub mDomains: u8,
    pub mB2D1: bool,
    /// per terrain type, whether the class may stand on it
    pub mTerrainTypes: [bool; 17],
    pub mIsAffectedByGravity: bool,
    pub mUsesAmmunition: bool,
    pub mShootsHigh: bool,
    pub mUpgradesAreaEffect: bool,
    pub mUpgradesRepair: bool,
    pub mUpgradesShockArmor: bool,
    pub mUpgradesPierceArmor: bool,
    pub mUpgradesArrowArmor: bool,
    pub mUpgradesLaserArmor: bool,
    pub mUpgradesGunArmor: bool,
    pub mUpgradesMissileArmor: bool,
    pub mUpgradesRange: bool,
    pub mPitchesWithTerrain: bool,
    pub mRollsWithTerrain: bool,
    pub mFiresBroadside: bool,
    pub mCanAttackGround: bool,
    pub mCanFireOverWalls: bool,
    pub mIsScenarioSelectable: bool,
    pub mIsEnabled: bool,
    pub mFacesCamera: bool,
    pub mIsDecoration: bool,
    pub mFiresWhileMoving: bool,
    pub mCanFly: bool,
    pub mB2FA: bool,
    pub mBuildsOnElevation: bool,
    pub mBlocksTile: bool,
    pub mCanEnterAirTransports: bool,
    pub mCanEnterLandTransports: bool,
    pub mCanEnterSeaTransports: bool,
    pub mCanEnterSpaceTransports: bool,
    pub mCanEnterFortresses: bool,
    pub mCanGarrison: bool,
    pub mIsCollidable: bool,
    pub mCanSetRallyPoint: bool,
    pub mB305: bool,
    pub mIsGaiaPlaceable: bool,
    pub mConvertsBuildings: bool,
    pub mConvertsPriests: bool,
    pub mIsPlayerPlaceable: bool,
    pub mIsVital: bool,
    pub mIsCloakable: bool,
    pub mIsExpansionUnit: bool,
    pub mCanEnterAirports: bool,
    pub mCanEnterCarriers: bool,
    pub mCanEnterNavyYards: bool,
    pub mIsWall: bool,
    /// the contagion functors copy it to the unit's
    pub mIsDeletable: bool,
    pub mIsInactive: bool,
    pub mIsAttackUnit: bool,
    pub mIsDefenseUnit: bool,
    pub mCanCrossForest: bool,
    pub mIsObstacle: bool,
    pub mIsTree: bool,
    pub mIsMelee: bool,
    pub mCanDisband: bool,
    pub mIsHuman: bool,
    pub mIsHorse: bool,
    pub mIsTower: bool,
    pub mHasCalamityPowers: bool,
    pub mIsProjectile: bool,
    pub mCanChangeBehavior: bool,
    pub mIsCyber: bool,
    pub mCanPatrol: bool,
    /// set by `CreateUnitClasses` (unit set 0x81)
    pub mIsMedic: bool,
    pub mSkipsLineOfFireCheck: bool,
    /// set by `CreateUnitClasses` (unit sets 0x8a, 0x97, 0x96)
    pub mIsFlare: bool,
    pub mIsBoobyTrap: bool,
    pub mIsParatrooperBomber: bool,
    pub mHasSpecialAttack: bool,
    /// port (not in the original): the class's `IsLandBuilding()`, set by `CreateUnitClasses` once the class's
    /// vtable is final; the unit store reads it (store.rs)
    pub mIsLandBuildingClass: bool,
}

/// What the constructor looks up outside the row: the C++ fills it before [`fill`].
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUnitClassFillIn {
    /// the constructor's first argument
    pub is_complex: c_long,
    /// the row's technology is in the tech tree: its node's epoch wins over the row's
    pub has_tech_node: bool,
    pub tech_epoch: c_long,
    /// one of the research technologies is a kind-5 (calamity) technology
    pub has_calamity_powers: bool,
    /// `EEAnimal::Find(unit type)` found a record: its animal id (else 1000)
    pub has_animal: bool,
    pub animal_id: c_long,
    /// the class's AI script (a script.rs handle; 0: none)
    pub ai_script: u64,
}

const CATEGORY_AMBIENT: c_ulong = 0;
const CATEGORY_RESOURCE: c_ulong = 1;
const CATEGORY_BUILDING: c_ulong = 2;
const CATEGORY_AIRPLANE: c_ulong = 3;
const CATEGORY_NAVAL: c_ulong = 4;
const CATEGORY_LAND: c_ulong = 5;
const CATEGORY_LAND_CITIZEN: c_ulong = 6;
const CATEGORY_HELICOPTER: c_ulong = 7;
const CATEGORY_LAND8: c_ulong = 8;
const CATEGORY_PROJECTILE: c_ulong = 9;
const CATEGORY_SUBMERGED_NAVAL: c_ulong = 0xa;
const CATEGORY_BOMB: c_ulong = 0xb;
const CATEGORY_MISSILE: c_ulong = 0xc;
const CATEGORY_LAND_PRIEST: c_ulong = 0xd;
const CATEGORY_NAVAL_PRIEST: c_ulong = 0xe;
const CATEGORY_LAND15: c_ulong = 0xf;
const CATEGORY_LAND_TRANSPORT: c_ulong = 0x10;
const CATEGORY_HELICOPTER_TRANSPORT: c_ulong = 0x11;
const CATEGORY_NAVAL_TRANSPORT: c_ulong = 0x12;
const CATEGORY_BUILDING19: c_ulong = 0x13;
const CATEGORY_TORPEDO: c_ulong = 0x14;
const CATEGORY_NAVAL_BUILDING: c_ulong = 0x15;
const CATEGORY_NAVAL_CITIZEN: c_ulong = 0x16;
const CATEGORY_LAND_MILITARY_HERO: c_ulong = 0x17;
const CATEGORY_MINING_BUILDING: c_ulong = 0x18;
const CATEGORY_GATE: c_ulong = 0x19;
const CATEGORY_HELICOPTER_PRIEST: c_ulong = 0x1a;
const CATEGORY_SEA_ANIMAL: c_ulong = 0x1b;
const CATEGORY_AIR_ANIMAL: c_ulong = 0x1c;
const CATEGORY_FORTRESS: c_ulong = 0x1d;
const CATEGORY_BUILDING_PRIEST: c_ulong = 0x1e;
const CATEGORY_SPACESHIP: c_ulong = 0x1f;
const CATEGORY_SPACE_BUILDING: c_ulong = 0x20;
const CATEGORY_SPACE_TRANSPORT: c_ulong = 0x21;
const CATEGORY_PLANET_SPACESHIP: c_ulong = 0x22;

const UNIT_TYPE_GUARD_TOWER_PALEO: c_ulong = 0x2f;
const UNIT_TYPE_AMB_STARFISH3: c_ulong = 0x1cc;
const UNIT_TYPE_GUARD_TOWER_DIGITAL: c_ulong = 0x1d2;
const UNIT_TYPE_GUARD_TOWER_SPACE: c_ulong = 0x2fd;
const UNIT_TYPE_SPACE_TURRET: c_ulong = 0x302;
const UNIT_TYPE_GUARD_TOWER_PALISADES: c_ulong = 0x312;
const UNIT_TYPE_GUARD_TOWER_BAMBOO: c_ulong = 0x322;
const UNIT_TYPE_MISSILE_BASE: c_ulong = 0x43;
const UNIT_TYPE_CP_MISSILE_BASE: c_ulong = 0x2ed;
const UNIT_TYPE_RES_SHARK: c_ulong = 0x13a;
const UNIT_TYPE_RES_DOLPHIN: c_ulong = 0x295;
const UNIT_TYPE_CATALINA: c_ulong = 0x2db;
const UNIT_TYPE_PATH_POINT: c_ulong = 0xdf;
const UNIT_TYPE_AIRPORT: c_ulong = 0x42;
const UNIT_TYPE_PILLBOX: c_ulong = 0x25e;
const UNIT_TYPE_PROPHET: c_ulong = 0xd8;

const MOVEMENT_AIR: u8 = 1;
const MOVEMENT_LAND: u8 = 2;
const MOVEMENT_SEA: u8 = 4;
const MOVEMENT_SPACE: u8 = 8;

const RESOURCE_WOOD: c_long = 3;

const TERRAIN_DEEP_WATER: usize = 1;
const TERRAIN_SNOW: usize = 5;
const TERRAIN_CLIFF: usize = 9;
const TERRAIN_SHALLOW_WATER: usize = 10;
const TERRAIN_SPACE: usize = 12;

const UNIT_KIND_CITIZEN: c_long = 1;
const UNIT_KIND_MILITARY: c_long = 2;
const UNIT_KIND_PRIEST: c_long = 4;
const UNIT_KIND_RESOURCE: c_long = 8;
const UNIT_KIND_BUILDING: c_long = 0x10;
const UNIT_KIND_BUILDING_PRIEST: c_long = 0x14;
const UNIT_KIND_PATH_POINT: c_long = 0x20;
const UNIT_KIND_NAVAL_CITIZEN: c_long = 0x40;
const UNIT_KIND_HELICOPTER_TRANSPORT: c_long = 0x80;
const UNIT_KIND_LAND_TRANSPORT: c_long = 0x100;
const UNIT_KIND_NAVAL_TRANSPORT: c_long = 0x200;
const UNIT_KIND_CAN_ENTER_AIR_TRANSPORTS: c_long = 0x400;
const UNIT_KIND_CAN_ENTER_LAND_TRANSPORTS: c_long = 0x800;
const UNIT_KIND_CAN_ENTER_SEA_TRANSPORTS: c_long = 0x1000;
const UNIT_KIND_CAN_ENTER_FORTRESSES: c_long = 0x2000;
const UNIT_KIND_FORTRESS: c_long = 0x4000;
const UNIT_KIND_LONG_RANGE_MILITARY: c_long = 0x8002;
const UNIT_KIND_SPACE_TRANSPORT: c_long = 0x10000;
const UNIT_KIND_CAN_ENTER_SPACE_TRANSPORTS: c_long = 0x20000;

/// the command that makes a class change behaviour, and the patrol command
const COMMAND_BEHAVIOR: c_ulong = 0x1d;
const COMMAND_PATROL: c_ulong = 0x12;
/// `kActionGetNextPatrolPoint`
const ACTION_GET_NEXT_PATROL_POINT: i32 = 8020;

/// the largest line of sight (0x447ac8)
const MAX_LINE_OF_SIGHT: f32 = 32.0;
/// the default firing arc (20 degrees)
const DEFAULT_FIRING_ARC: f32 = 0.349_065_87;
/// degrees to radians (the original's constant)
const DEGREES_TO_RADIANS: f32 = 0.017_453_292;

fn flag(b: u8) -> bool {
    b != 0
}

/// A `[u8; 4]` field of the row (the generator's form of a field no table names) as the C++ `int32_t`.
fn raw_i32(b: [u8; 4]) -> i32 {
    i32::from_ne_bytes(b)
}

/// The C++ `(long)` of a float: truncation toward zero (in range; the original's out-of-range conversion is UB).
#[allow(clippy::cast_possible_truncation)]
fn trunc(f: f32) -> c_long {
    f as c_long
}

/// The constructor's plain stores from `row` (0x22f317), in the original's order where one store reads another.
/// The four flags `CreateUnitClasses` sets afterwards (`mIsMedic`, `mIsFlare`, `mIsBoobyTrap`,
/// `mIsParatrooperBomber`) start false here (the original left them unset until then).
#[must_use]
pub fn fill(row: &EEUnitClassRecord, input: &EeUnitsUnitClassFillIn) -> EeUnitsUnitClassFields {
    let mut f = EeUnitsUnitClassFields {
        mIsComplex: input.is_complex,
        mCategory: row.category as c_ulong,
        mUnitType: row.unit_type as c_ulong,
        mResourceType: row.resource_type as c_long,
        mFamily: row.family as c_ulong,
        mL208: raw_i32(row.unused_unit_group) as c_long,
        mL294: raw_i32(row.unused_selection_value) as c_long,
        mTechnology: row.technology as c_long,
        mMinRange: row.min_range,
        mRange: row.range,
        mLineOfSight: row.line_of_sight,
        ..EeUnitsUnitClassFields::default()
    };
    if f.mLineOfSight > MAX_LINE_OF_SIGHT {
        f.mLineOfSight = MAX_LINE_OF_SIGHT;
    }
    f.mAreaEffect = row.area_effect;
    f.mScale = if row.scale > 0.0001 { row.scale } else { 0.0 };
    f.mAcceleration = row.acceleration;
    f.mMaxVelocity = row.max_velocity;
    f.mF138 = 0.0;
    f.mMaxAngularVelocity = row.max_angular_velocity;
    f.mAngularAcceleration = row.angular_acceleration;
    f.mTurretMaxAngularVelocity = row.turret_max_angular_velocity;
    f.mResourceAmount = trunc(row.resource_amount);
    f.mCarryAmount = trunc(row.carry_amount);
    f.mWorkRate = trunc(row.work_rate);
    f.mGatherTimes = [
        trunc(row.wood_gather_time),
        trunc(row.stone_gather_time),
        trunc(row.gold_gather_time),
        trunc(row.iron_gather_time),
        trunc(row.hunt_gather_time),
        trunc(row.forage_gather_time),
        trunc(row.farm_gather_time),
    ];
    f.mReloadTime = trunc(row.reload_time * 1000.0);
    f.mFacingOffset = row.facing_offset;
    f.mFiringArc = if row.firing_arc == 0.0 { DEFAULT_FIRING_ARC } else { row.firing_arc };
    f.mWeapon = row.weapon as c_long;
    f.mAttack = row.attack as c_long;
    f.mArmor = [
        row.shock_armor as c_long,
        row.arrow_armor as c_long,
        row.pierce_armor as c_long,
        row.gun_armor as c_long,
        row.laser_armor as c_long,
        row.missile_armor as c_long,
    ];
    // an int32 product (the original's `int` multiply), then widened
    f.mFlightTime = row.flight_time.wrapping_mul(1000) as c_long;
    f.mLeadership = row.leadership as c_long;
    f.mMaxObjectCarry = row.max_object_carry as c_long;
    f.mRottingTime = trunc(row.rotting_time * 1000.0);
    f.mBuildTimeScale = 1.0;
    f.mCostScale = 1.0;
    f.mAreaEffectScale = 1.0;
    f.mAmmoType = row.ammo_type as c_long;
    f.mRepairRate = row.repair_rate as c_long;
    f.mHitPoints = row.hit_points as c_long;
    f.mCombatReaction = 0;
    f.mIsPlayerPlaceable = flag(row.is_player_placeable);
    f.mIsVital = flag(row.is_vital);
    f.mTileSize = row.tile_size as c_long;
    f.mWaterKind = row.water_kind as c_long;
    f.mLoadRange = row.load_range;
    f.mUnloadRange = row.unload_range;
    f.mCanCrossForest = flag(row.can_cross_forest);
    let t = &mut f.mTerrainTypes;
    t[0] = flag(row.can_move_on_land);
    t[TERRAIN_DEEP_WATER] = flag(row.can_move_on_deep_water);
    t[2] = flag(row.can_move_on_rock);
    t[3] = flag(row.can_move_on_terrain_type_3);
    t[4] = flag(row.can_move_on_mud);
    t[TERRAIN_SNOW] = flag(row.can_move_on_land);
    t[6] = flag(row.can_move_on_rough_grass);
    t[7] = flag(row.can_move_on_terrain_type_7);
    t[8] = flag(row.can_move_on_pavement);
    t[TERRAIN_CLIFF] = flag(row.can_move_on_cliffs);
    t[TERRAIN_SHALLOW_WATER] = flag(row.can_move_on_shallow_water);
    t[11] = flag(row.can_move_on_sand);
    t[TERRAIN_SPACE] = flag(row.can_move_in_space);
    t[14] = flag(row.can_cross_forest);
    f.mIsDecoration = flag(row.is_decoration);
    f.mFiresBroadside = flag(row.fires_broadside);
    f.mCanFireOverWalls = flag(row.can_fire_over_walls);
    f.mCanAttackGround = flag(row.can_attack_ground);
    f.mIsMelee = flag(row.is_melee);
    f.mCanDisband = flag(row.can_disband);
    f.mIsScenarioSelectable = flag(row.is_scenario_selectable);
    f.mIsEnabled = flag(row.is_enabled);
    f.mFacesCamera = flag(row.faces_camera);
    f.mFiresWhileMoving = flag(row.fires_while_moving);
    f.mCanFly = flag(row.can_fly);
    f.mB2FA = flag(row.unused_flag_6[0]);
    f.mBuildsOnElevation = flag(row.builds_on_elevation);
    f.mTransportCapacity = row.transport_capacity as c_long;
    f.mWeight = row.transport_size as c_long;
    f.mPitchesWithTerrain = flag(row.pitches_with_terrain);
    f.mRollsWithTerrain = flag(row.rolls_with_terrain);
    f.mUpgradesAreaEffect = flag(row.can_upgrade_area_effect);
    f.mUpgradesArrowArmor = flag(row.can_upgrade_arrow_armor);
    f.mUpgradesGunArmor = flag(row.can_upgrade_gun_armor);
    f.mUpgradesMissileArmor = flag(row.can_upgrade_missile_armor);
    f.mUpgradesRange = flag(row.can_upgrade_range);
    f.mUpgradesLaserArmor = flag(row.can_upgrade_laser_armor);
    f.mUpgradesPierceArmor = flag(row.can_upgrade_pierce_armor);
    f.mUpgradesRepair = flag(row.can_upgrade_repair);
    f.mUpgradesShockArmor = flag(row.can_upgrade_shock_armor);
    f.mF19C = f32::from_ne_bytes(row.unused_defaulted_value);
    if f.mF19C == 0.0 {
        f.mF19C = -20.0;
    }
    f.mMaxRoll = row.max_roll;
    f.mCruiseAltitude = row.cruise_altitude;
    f.mTargetSearchInterval = row.target_search_interval as c_long;
    f.mNameStringID = row.name_string_id as c_long;
    f.mHealingPoints = row.healing_points as c_long;
    f.mRegeneration = row.regeneration as c_long;
    f.mCanEnterAirTransports = flag(row.can_enter_air_transports);
    f.mCanEnterLandTransports = flag(row.can_enter_land_transports);
    f.mCanEnterSeaTransports = flag(row.can_enter_sea_transports);
    f.mCanEnterSpaceTransports = f.mCanEnterSeaTransports;
    f.mCanEnterFortresses = flag(row.can_enter_fortresses);
    f.mSelectionSize = row.selection_size;
    f.mL170 = f32::from_ne_bytes(row.unused_shadow_offset);
    f.mShadowScale = row.shadow_scale;
    f.mOuterCollisionSphere = row.outer_collision_sphere;
    f.mInnerCollisionSphere = row.inner_collision_sphere;
    f.mIsCollidable = flag(row.is_collidable);
    f.mAmmunition = row.ammunition as c_long;
    f.mUsesAmmunition = flag(row.uses_ammunition);
    f.mIsAffectedByGravity = flag(row.is_affected_by_gravity);
    f.mShootsHigh = flag(row.shoots_high);
    f.mL284 = raw_i32(row.unused_reference) as c_long;
    f.mL180 = raw_i32(row.unused_value) as c_long;
    f.mMissileDelayTime = trunc(row.missile_delay_time * 1000.0);
    f.mLaunchPitch = row.launch_pitch * DEGREES_TO_RADIANS;
    f.mCanSetRallyPoint = flag(row.can_set_rally_point);
    f.mAIKind = row.ai_kind as c_ulong;
    f.mCounterKind1 = row.counter_kind_1 as c_ulong;
    f.mCounterKind2 = row.counter_kind_2 as c_ulong;
    f.mCounterKind3 = row.counter_kind_3 as c_ulong;
    f.mB305 = flag(row.unused_ground_flag[0]);
    f.mIsGaiaPlaceable = flag(row.is_gaia_placeable);
    f.mConvertsBuildings = false;
    f.mConvertsPriests = false;
    f.mConversionResistance = row.conversion_resistance;
    #[allow(clippy::cast_precision_loss)]
    let rate = row.conversion_rate as f32;
    f.mConversionRate = rate * 0.01;
    f.mSlopeSpeedPenalty = row.slope_speed_penalty;
    f.mBobHeight = row.bob_height;
    f.mBobPitch = row.bob_pitch * DEGREES_TO_RADIANS;
    f.mIsCloakable = flag(row.is_cloakable);
    f.mCloakRadius = row.cloak_radius as c_long;
    f.mIsExpansionUnit = flag(row.is_expansion_unit);
    f.mCanEnterCarriers = flag(row.can_enter_carriers);
    f.mCanEnterAirports = flag(row.can_enter_airports);
    f.mCanEnterNavyYards = f.mUnitType == UNIT_TYPE_CATALINA;
    f.mMiniMapColor = row.mini_map_color as c_long;
    f.mAirTrafficType = row.air_traffic_type as c_long;
    f.mIsDeletable = flag(row.is_deletable);
    f.mWonderBonus = row.wonder_bonus as c_long;
    f.mWonderUnitSet = row.wonder_unit_set as c_long;
    f.mIsInactive = flag(row.is_inactive);
    f.mIsDefenseUnit = flag(row.is_defense_unit);
    f.mIsAttackUnit = flag(row.is_attack_unit);
    f.mPowerRegeneration = row.power_regeneration as c_long;
    f.mPower = row.power as c_long;
    f.mHealThreshold = row.heal_threshold;
    f.mHealRange = row.heal_range;
    f.mHealDelay = row.heal_delay;
    f.mMinConversionTime = row.min_conversion_time as c_long;
    f.mSkipsLineOfFireCheck = false;
    f.mHasSpecialAttack = flag(row.has_special_attack);
    let ty = f.mUnitType;
    f.mIsTower = ty == UNIT_TYPE_GUARD_TOWER_PALEO
        || (ty > UNIT_TYPE_AMB_STARFISH3
            && (ty <= UNIT_TYPE_GUARD_TOWER_DIGITAL
                || ty == UNIT_TYPE_GUARD_TOWER_SPACE
                || ty == UNIT_TYPE_SPACE_TURRET
                || ty == UNIT_TYPE_GUARD_TOWER_PALISADES
                || ty == UNIT_TYPE_GUARD_TOWER_BAMBOO));
    f.mDomains = domains(f.mCategory, ty, f.mIsTower);
    if ty == UNIT_TYPE_RES_SHARK || ty == UNIT_TYPE_RES_DOLPHIN {
        f.mIsCollidable = false;
    }
    f.mAnimalID = if input.has_animal { input.animal_id } else { 1000 };
    f.mInfluence = row.influence as c_long;
    if f.mInfluence == 0 && row.attack > 0 {
        f.mInfluence = 1;
    }
    f.mCanGarrison = flag(row.can_garrison);
    f.mGarrisonCapacity = row.garrison_capacity as c_long;
    f.mInitialGarrisonCount = row.initial_garrison_count as c_long;
    f.mGarrisonTechnology = row.garrison_technology as c_long;
    f.mGarrisonBonus = row.garrison_bonus;
    f.mEpoch = if input.has_tech_node { input.tech_epoch } else { row.epoch as c_long };
    f.mHasCalamityPowers = input.has_calamity_powers;
    let cat = f.mCategory;
    f.mIsProjectile =
        cat == CATEGORY_PROJECTILE || cat == CATEGORY_BOMB || cat == CATEGORY_MISSILE || cat == CATEGORY_TORPEDO;
    f.mAreaEffectTemplates = [
        row.area_effect_template_0 as c_long,
        row.area_effect_template_1 as c_long,
        row.area_effect_template_2 as c_long,
        row.area_effect_template_3 as c_long,
    ];
    f.mOverlapUnitSet = row.overlap_unit_set as c_long;
    f.mKindFlags = kind_flags(&f);
    f.mFriendlyDamageFactor = row.friendly_damage_factor;
    f.mB2D1 = true;
    f.mBlocksTile = flag(row.blocks_tile);
    f.mCommands = [
        row.command_0 as c_ulong,
        row.command_1 as c_ulong,
        row.command_2 as c_ulong,
        row.command_3 as c_ulong,
        row.command_4 as c_ulong,
        row.command_5 as c_ulong,
        row.command_6 as c_ulong,
        row.command_7 as c_ulong,
        row.command_8 as c_ulong,
    ];
    // the first eight commands only, as the original's loops
    f.mCanChangeBehavior = f.mCommands[..8].contains(&COMMAND_BEHAVIOR);
    f.mCanPatrol = f.mCommands[..8].contains(&COMMAND_PATROL)
        && crate::script::find_action(input.ai_script, ACTION_GET_NEXT_PATROL_POINT) != 0;
    f.mIsObstacle = flag(row.is_obstacle);
    f.mIsTree = f.mResourceType == RESOURCE_WOOD;
    f
}

/// The movement domains by category (towers and the missile bases are land buildings).
fn domains(cat: c_ulong, ty: c_ulong, is_tower: bool) -> u8 {
    let mut d = 0;
    if matches!(
        cat,
        CATEGORY_LAND
            | CATEGORY_LAND_CITIZEN
            | CATEGORY_LAND_PRIEST
            | CATEGORY_LAND_TRANSPORT
            | CATEGORY_LAND_MILITARY_HERO
    ) || (cat == CATEGORY_BUILDING && (is_tower || ty == UNIT_TYPE_MISSILE_BASE || ty == UNIT_TYPE_CP_MISSILE_BASE))
    {
        d = MOVEMENT_LAND;
    }
    if matches!(
        cat,
        CATEGORY_AIRPLANE
            | CATEGORY_HELICOPTER
            | CATEGORY_HELICOPTER_TRANSPORT
            | CATEGORY_HELICOPTER_PRIEST
            | CATEGORY_AIR_ANIMAL
    ) {
        d |= MOVEMENT_AIR;
    }
    if matches!(
        cat,
        CATEGORY_NAVAL
            | CATEGORY_NAVAL_CITIZEN
            | CATEGORY_LAND8
            | CATEGORY_SUBMERGED_NAVAL
            | CATEGORY_NAVAL_PRIEST
            | CATEGORY_NAVAL_TRANSPORT
            | CATEGORY_NAVAL_BUILDING
            | CATEGORY_SEA_ANIMAL
    ) {
        d |= MOVEMENT_SEA;
    }
    if matches!(
        cat,
        CATEGORY_SPACESHIP | CATEGORY_SPACE_TRANSPORT | CATEGORY_SPACE_BUILDING | CATEGORY_PLANET_SPACESHIP
    ) {
        d |= MOVEMENT_SPACE;
    }
    d
}

/// The kind flags: the category's kind, then the transports the class may enter.
fn kind_flags(f: &EeUnitsUnitClassFields) -> c_long {
    let ty = f.mUnitType;
    let mut k = match f.mCategory {
        CATEGORY_AMBIENT => {
            if ty == UNIT_TYPE_PATH_POINT {
                UNIT_KIND_PATH_POINT
            } else {
                0
            }
        }
        CATEGORY_LAND_CITIZEN => UNIT_KIND_CITIZEN,
        CATEGORY_NAVAL_CITIZEN => UNIT_KIND_NAVAL_CITIZEN,
        CATEGORY_BUILDING
        | CATEGORY_BUILDING19
        | CATEGORY_NAVAL_BUILDING
        | CATEGORY_MINING_BUILDING
        | CATEGORY_GATE
        | CATEGORY_SPACE_BUILDING => {
            if f.mIsTower
                || ty == UNIT_TYPE_MISSILE_BASE
                || ty == UNIT_TYPE_CP_MISSILE_BASE
                || ((f.mCategory == CATEGORY_NAVAL_BUILDING || f.mCategory == CATEGORY_SPACE_BUILDING)
                    && f.mAirTrafficType != 0)
                || ty == UNIT_TYPE_AIRPORT
                || ty == UNIT_TYPE_PILLBOX
            {
                UNIT_KIND_MILITARY
            } else {
                UNIT_KIND_BUILDING
            }
        }
        CATEGORY_AIRPLANE
        | CATEGORY_NAVAL
        | CATEGORY_LAND
        | CATEGORY_HELICOPTER
        | CATEGORY_LAND8
        | CATEGORY_SUBMERGED_NAVAL
        | CATEGORY_LAND15
        | CATEGORY_LAND_MILITARY_HERO
        | CATEGORY_SPACESHIP
        | CATEGORY_PLANET_SPACESHIP => {
            if f.mRange > f.mLineOfSight {
                UNIT_KIND_LONG_RANGE_MILITARY
            } else {
                UNIT_KIND_MILITARY
            }
        }
        CATEGORY_LAND_PRIEST | CATEGORY_NAVAL_PRIEST | CATEGORY_HELICOPTER_PRIEST => {
            if ty == UNIT_TYPE_PROPHET {
                UNIT_KIND_MILITARY
            } else {
                UNIT_KIND_PRIEST
            }
        }
        CATEGORY_BUILDING_PRIEST => UNIT_KIND_BUILDING_PRIEST,
        CATEGORY_LAND_TRANSPORT => UNIT_KIND_LAND_TRANSPORT,
        CATEGORY_FORTRESS => UNIT_KIND_FORTRESS,
        CATEGORY_HELICOPTER_TRANSPORT => UNIT_KIND_HELICOPTER_TRANSPORT,
        CATEGORY_NAVAL_TRANSPORT => UNIT_KIND_NAVAL_TRANSPORT,
        CATEGORY_SPACE_TRANSPORT => UNIT_KIND_SPACE_TRANSPORT,
        CATEGORY_RESOURCE => UNIT_KIND_RESOURCE,
        _ => 0,
    };
    if f.mCanEnterAirTransports {
        k |= UNIT_KIND_CAN_ENTER_AIR_TRANSPORTS;
    }
    if f.mCanEnterLandTransports {
        k |= UNIT_KIND_CAN_ENTER_LAND_TRANSPORTS;
    }
    if f.mCanEnterSeaTransports {
        k |= UNIT_KIND_CAN_ENTER_SEA_TRANSPORTS;
    }
    if f.mCanEnterFortresses {
        k |= UNIT_KIND_CAN_ENTER_FORTRESSES;
    }
    if f.mCanEnterSpaceTransports {
        k |= UNIT_KIND_CAN_ENTER_SPACE_TRANSPORTS;
    }
    k
}

/// `EEUnitClass::EEUnitClass`'s plain stores: `out` (the class's record base) set from the db row `row`
/// (an `EEUnitClassRecord`, ee/EEUnitClassRecord.h, the layout ee_data's generated record asserts).
///
/// # Safety
/// `out` is a writable `EeUnitsUnitClassFields`; `row` a readable `EEUnitClassRecord`; `input` readable.
#[no_mangle]
pub unsafe extern "C" fn ee_units_unit_class_fill(
    out: *mut EeUnitsUnitClassFields,
    row: *const std::ffi::c_void,
    input: *const EeUnitsUnitClassFillIn,
) {
    // SAFETY: the caller's contract; the row's bytes are any values of its fields' types (ints, floats, bytes)
    let (row, input) = unsafe { (&*row.cast::<EEUnitClassRecord>(), &*input) };
    let value = fill(row, input);
    // SAFETY: the caller's contract (a write, never a read of the old, possibly unset, bytes)
    unsafe { out.write(value) };
}

#[cfg(test)]
#[path = "unit_class_fields_tests.rs"]
mod tests;
