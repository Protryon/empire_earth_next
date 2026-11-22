//! The class bodies round 8 left in EEUnitClass.cpp and its kind files (the units' lane, round 9), over their C++
//! statements: `GetElevationFactor`, `BaseGetSoundParameters` and the priests' and spaceships' sound parameters,
//! `FindTargetNear`, `GetContainerContinent`, `BaseAddToTile`/`BaseRemoveFromTile`, `RegisterUnitClass`,
//! `ReleaseUnitClass(es)`, `UnitRemoved`, `EEComplexUnit::GetInverseMaxVelocity`, `SetCamouflaged`,
//! `UpdateOrbitals`, `DistanceSquared3D`; the people's `FindNextResource`, `EEHelicopterPriestPathToGoal`,
//! `HelicopterPriestUnitCreated`, the basic priest's media; the buildings' `EruptVolcano` and the gates' part
//! links; the air's `EEIsTileFree`. Each C++ statement one `EE_UNITS_UC_*` case of `ee_units_uc_host`
//! (EEUnitClass.cpp; `EruptVolcano`'s through `ee_units_ucb_host`, where its assert names its file), in the
//! original's order.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::combat::EeCombatElevationIn;
use crate::units::uc_body::*;
use crate::units::uc_create::*;
use crate::units::uc_produce::*;
use ee_base::math::types::GeTransformation;
use ee_base::math::um::lrintf;

// ---- the statements (3100..3199)
/// `unit->GetLineOfSight()` in `f[0]`
pub const EE_UNITS_UC_R_LINE_OF_SIGHT: i32 = 3100;
/// `static_cast<EEUGRust*>(goal a)->GetGatherResource()`
pub const EE_UNITS_UC_R_GATHER_RESOURCE: i32 = 3101;
/// `gContinentManager.CanGetToUnit(unit a, unit)`
pub const EE_UNITS_UC_R_CAN_GET_TO_UNIT: i32 = 3102;
/// `class->FindResourceOfKind(unit, unit a, unit b, the bits of f[0], the bits of f[1], the vector c, true)`
pub const EE_UNITS_UC_R_FIND_RESOURCE_OF_KIND: i32 = 3103;
/// `EECombatFindDropLocation(unit, NULL, f[0..3])`
pub const EE_UNITS_UC_R_FIND_DROP_LOCATION: i32 = 3104;
/// goal `a`'s `GetDestination(unit, f[0..3])`
pub const EE_UNITS_UC_R_GOAL_DESTINATION: i32 = 3105;
/// `EEGetHelicopterPriestDestination(unit, f[0..3])`
pub const EE_UNITS_UC_R_PRIEST_DESTINATION: i32 = 3106;
/// `f[0] = class->mCruiseAltitude`
pub const EE_UNITS_UC_R_CRUISE_ALTITUDE: i32 = 3107;
/// `EEFindLandingPoint(unit, f[0..3])`
pub const EE_UNITS_UC_R_LANDING_POINT: i32 = 3108;
/// `f[0] = class->GetHeightAt(unit, f[0..3])`
pub const EE_UNITS_UC_R_HEIGHT_AT: i32 = 3109;
/// `unit->SetTransformation(the transformation at a, true)`
pub const EE_UNITS_UC_R_SET_TRANSFORMATION: i32 = 3110;
/// `unit->SetHeight(f[0])`
pub const EE_UNITS_UC_R_SET_HEIGHT: i32 = 3111;
/// `class->mCategory`
pub const EE_UNITS_UC_R_CATEGORY: i32 = 3112;
/// unit `a`'s `mUnitClass->mCategory`
pub const EE_UNITS_UC_R_CATEGORY_OF: i32 = 3113;
/// `EEGetRules().mCombat.IsElevationLauncher(b)`
pub const EE_UNITS_UC_R_ELEVATION_LAUNCHER: i32 = 3114;
/// `unit->mLauncherClass->mCategory`
pub const EE_UNITS_UC_R_LAUNCHER_CATEGORY: i32 = 3115;
/// `EEGetRules().mCombat.IsElevationAttacker(b)`
pub const EE_UNITS_UC_R_ELEVATION_ATTACKER: i32 = 3116;
/// `EEGetRules().mCombat.IsElevationTarget(b)`
pub const EE_UNITS_UC_R_ELEVATION_TARGET: i32 = 3117;
/// `f[0] = unit->mHeight`
pub const EE_UNITS_UC_R_HEIGHT: i32 = 3118;
/// `f[0] = gWorldTable.GetFloatValue(b)`
pub const EE_UNITS_UC_R_WORLD_FLOAT: i32 = 3119;
/// `f[0] = unit->mPlayer->mHighGroundBonus`
pub const EE_UNITS_UC_R_HIGH_GROUND_BONUS: i32 = 3120;
/// `f[0] = EECombatElevation(the record at a)` (its CBEL record)
pub const EE_UNITS_UC_R_ELEVATION: i32 = 3121;
/// unit `a`'s `mTileX`
pub const EE_UNITS_UC_R_TILE_X_OF: i32 = 3122;
/// unit `a`'s `mTileY`
pub const EE_UNITS_UC_R_TILE_Y_OF: i32 = 3123;
/// `f[0] = ` unit `a`'s `mTile->mHeight`
pub const EE_UNITS_UC_R_TILE_HEIGHT_OF: i32 = 3124;
/// `f[0] = ` sound `a`'s `mVolume`
pub const EE_UNITS_UC_R_SOUND_VOLUME: i32 = 3125;
/// `EEComputeSoundParameters(f[3], f[0..3], f[0], f[1], f[2], 4.0f)`
pub const EE_UNITS_UC_R_COMPUTE_SOUND: i32 = 3126;
/// `EEIsSeen(unit a)`
pub const EE_UNITS_UC_R_IS_SEEN: i32 = 3127;
/// `EEGetDopplerPitch(unit a, sound b, &f[0], &f[1], &f[2])`
pub const EE_UNITS_UC_R_DOPPLER_PITCH: i32 = 3128;
/// unit `a`'s `mTileNode != NULL`
pub const EE_UNITS_UC_R_HAS_TILE_NODE: i32 = 3129;
/// `EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Load(false)`
pub const EE_UNITS_UC_R_LOAD_INDICATOR: i32 = 3130;
/// `EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Unload()`
pub const EE_UNITS_UC_R_UNLOAD_INDICATOR: i32 = 3131;
/// `class->BaseLoadMedia(player a)`
pub const EE_UNITS_UC_R_BASE_LOAD_MEDIA: i32 = 3132;
/// `class->BaseUnloadMedia(player a)`
pub const EE_UNITS_UC_R_BASE_UNLOAD_MEDIA: i32 = 3133;
/// `unit->mNumOrbitals`
pub const EE_UNITS_UC_R_ORBITAL_COUNT: i32 = 3134;
/// `unit->mOrbitals[b]->Update()`
pub const EE_UNITS_UC_R_ORBITAL_UPDATE: i32 = 3135;
/// unit `a`'s `mTile`
pub const EE_UNITS_UC_R_RTS_TILE_OF: i32 = 3136;
/// RTS tile `a`'s `mUnits = AddUnit(unit b, mUnits)`
pub const EE_UNITS_UC_R_RTS_TILE_ADD: i32 = 3137;
/// RTS tile `a`'s `RemoveUnit(unit b)`
pub const EE_UNITS_UC_R_RTS_TILE_REMOVE: i32 = 3138;
/// class `a`'s `Reference()`
pub const EE_UNITS_UC_R_CLASS_REFERENCE: i32 = 3139;
/// class `a`'s `Dereference()`
pub const EE_UNITS_UC_R_CLASS_DEREFERENCE: i32 = 3140;
/// `gUnitClassTable[b]`
pub const EE_UNITS_UC_R_TABLE_GET: i32 = 3141;
/// `gUnitClassTable[b] = class a`
pub const EE_UNITS_UC_R_TABLE_SET: i32 = 3142;
/// `EEUnitClass::sUnitTypes.push_back(b)`
pub const EE_UNITS_UC_R_TYPES_PUSH: i32 = 3143;
/// `EEUnitClassCount()`
pub const EE_UNITS_UC_R_CLASS_COUNT: i32 = 3144;
/// `--class->mNumUnits` (its new value)
pub const EE_UNITS_UC_R_UNIT_REMOVED: i32 = 3145;
/// `class->mLastUnitGoneTime = TMGetTime()`
pub const EE_UNITS_UC_R_SET_GONE_TIME: i32 = 3146;
/// `unit->mActiveGroup`
pub const EE_UNITS_UC_R_ACTIVE_GROUP: i32 = 3147;
/// `f[0] = ` group `a`'s `GetInverseMaxVelocity(unit)`
pub const EE_UNITS_UC_R_GROUP_INVERSE_VELOCITY: i32 = 3148;
/// `f[0] = unit->mStats->mInverseMaxVelocity`
pub const EE_UNITS_UC_R_STATS_INVERSE_VELOCITY: i32 = 3149;
/// `unit->mCamouflageTimer`
pub const EE_UNITS_UC_R_CAMOUFLAGE_TIMER: i32 = 3150;
/// `unit->mCamouflageTimer = b`
pub const EE_UNITS_UC_R_SET_CAMOUFLAGE_TIMER: i32 = 3151;
/// unit `a`'s `mNextPart`
pub const EE_UNITS_UC_R_NEXT_PART: i32 = 3152;
/// unit `a`'s `mPreviousPart`
pub const EE_UNITS_UC_R_PREVIOUS_PART: i32 = 3153;
/// `EECanHeal(unit, complex unit a)`
pub const EE_UNITS_UC_R_CAN_HEAL: i32 = 3154;
/// `unit->CanReachUnit(unit a)`
pub const EE_UNITS_UC_R_CAN_REACH: i32 = 3155;
/// `gContinentManager.GetContinentAt(unit a)`
pub const EE_UNITS_UC_R_CONTINENT_OF: i32 = 3156;
/// `sUcTiles9.clear()` (the surrounding tiles' list, a local of the original)
pub const EE_UNITS_UC_R_TILES_CLEAR: i32 = 3157;
/// `class->GetSurroundingTiles(unit, sUcTiles9, 1, false)`
pub const EE_UNITS_UC_R_SURROUNDING_TILES: i32 = 3158;
/// `sUcTiles9.size()`
pub const EE_UNITS_UC_R_TILES_SIZE: i32 = 3159;
/// `gContinentManager.GetContinentAt(sUcTiles9[b].GetX(), sUcTiles9[b].GetY())`
pub const EE_UNITS_UC_R_CONTINENT_AT_TILE: i32 = 3160;
/// `gWorld.mMap->GetTileAtLocation(sUcTiles9[b])`
pub const EE_UNITS_UC_R_MAP_TILE_AT_TILE: i32 = 3161;
/// continent `a`'s `GetCategory() == kContinentCategoryLand`
pub const EE_UNITS_UC_R_CONTINENT_IS_LAND: i32 = 3162;
/// tile `a`'s `mTerrainType`
pub const EE_UNITS_UC_R_TERRAIN_OF: i32 = 3163;
/// `::gWorld.mMap->IsEdgeTile(b, c)`
pub const EE_UNITS_UC_R_EDGE_TILE: i32 = 3164;
/// `class->FacesPoint(unit, f[0..3], b)` (its own CBQR record)
pub const EE_UNITS_UC_R_FACES_POINT: i32 = 3165;
/// tile vector `a`'s `size()`
pub const EE_UNITS_UC_R_VEC_TILES_SIZE: i32 = 3166;
/// tile vector `a`'s `clear()`
pub const EE_UNITS_UC_R_VEC_TILES_CLEAR: i32 = 3167;
/// upgrade `a`'s `mButton = mUnitClass->mButton`
pub const EE_UNITS_UC_R_UPGRADE_BUTTON: i32 = 3168;
/// upgrade `a`'s `SetModel(mUnitClass->mModel)`
pub const EE_UNITS_UC_R_UPGRADE_MODEL: i32 = 3169;
/// upgrade `a`'s `SetAcknowledgeSound(mUnitClass->mAcknowledgeSounds[b], b)`
pub const EE_UNITS_UC_R_UPGRADE_ACKNOWLEDGE: i32 = 3170;
/// upgrade `a`'s `SetCreationSound(mUnitClass->mCreationSound)`
pub const EE_UNITS_UC_R_UPGRADE_CREATION: i32 = 3171;
/// upgrade `a`'s `SetDeathSound(mUnitClass->mDeathSound)`
pub const EE_UNITS_UC_R_UPGRADE_DEATH: i32 = 3172;
/// upgrade `a`'s `SetSelectionSound(mUnitClass->mSelectionSounds[b], b)`
pub const EE_UNITS_UC_R_UPGRADE_SELECTION: i32 = 3173;
/// upgrade `a`'s `SetRange(mUnitClass->mRange)`
pub const EE_UNITS_UC_R_UPGRADE_RANGE: i32 = 3174;
/// `ee_units_upgrade_init(upgrade a, its mUnitClass)` (the copies from the class's record)
pub const EE_UNITS_UC_R_UPGRADE_INIT: i32 = 3175;
/// upgrade `a`'s `mImprovement.Reapply(mUnitClass, upgrade)`
pub const EE_UNITS_UC_R_UPGRADE_REAPPLY: i32 = 3176;
/// `ee_units_upgrade_init_finish(upgrade a, its mUnitClass)`
pub const EE_UNITS_UC_R_UPGRADE_INIT_FINISH: i32 = 3177;

// (the buildings' host, EEUnitClassBuildings.cpp: 3180..3199)
/// `USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(9, the class pointer at a)`
pub const EE_UNITS_UCB_R_VOLCANO_CLASS: i32 = 3180;
/// the original's `ESVerify` of the volcano class
pub const EE_UNITS_UCB_R_VOLCANO_VERIFY: i32 = 3181;
/// calamity class `a`'s `GetRandomLocation(f[0..3], f[0..3])`
pub const EE_UNITS_UCB_R_RANDOM_LOCATION: i32 = 3182;
/// `unit->SetGoal(EEUGRust::NewAttackGround(f[0..3], false), 0, 0)`
pub const EE_UNITS_UCB_R_ATTACK_GROUND: i32 = 3183;

// ---- the bodies (`ee_units_uc_rest`'s `what`)
pub const EE_UNITS_UC_DO_R_HELICOPTER_PRIEST_PATH: i32 = 3200;
pub const EE_UNITS_UC_DO_R_HELICOPTER_PRIEST_CREATED: i32 = 3201;
pub const EE_UNITS_UC_DO_R_FIND_NEXT_RESOURCE: i32 = 3202;
pub const EE_UNITS_UC_DO_R_ELEVATION_FACTOR: i32 = 3203;
pub const EE_UNITS_UC_DO_R_UPDATE_ORBITALS: i32 = 3204;
pub const EE_UNITS_UC_DO_R_PRIEST_LOAD_MEDIA: i32 = 3205;
pub const EE_UNITS_UC_DO_R_PRIEST_UNLOAD_MEDIA: i32 = 3206;
pub const EE_UNITS_UC_DO_R_BASE_ADD_TO_TILE: i32 = 3207;
pub const EE_UNITS_UC_DO_R_BASE_REMOVE_FROM_TILE: i32 = 3208;
pub const EE_UNITS_UC_DO_R_REGISTER_CLASS: i32 = 3209;
pub const EE_UNITS_UC_DO_R_RELEASE_CLASS: i32 = 3210;
pub const EE_UNITS_UC_DO_R_RELEASE_CLASSES: i32 = 3211;
pub const EE_UNITS_UC_DO_R_UNIT_REMOVED: i32 = 3212;
pub const EE_UNITS_UC_DO_R_INVERSE_MAX_VELOCITY: i32 = 3213;
pub const EE_UNITS_UC_DO_R_SET_CAMOUFLAGED: i32 = 3214;
pub const EE_UNITS_UC_DO_R_ERUPT_VOLCANO: i32 = 3215;
pub const EE_UNITS_UC_DO_R_LAST_LINKED: i32 = 3216;
pub const EE_UNITS_UC_DO_R_FOURTH_PART: i32 = 3217;
pub const EE_UNITS_UC_DO_R_SECOND_PART: i32 = 3218;
pub const EE_UNITS_UC_DO_R_FIRST_PART: i32 = 3219;
pub const EE_UNITS_UC_DO_R_FIND_TARGET_NEAR: i32 = 3220;
pub const EE_UNITS_UC_DO_R_CONTAINER_CONTINENT: i32 = 3221;
pub const EE_UNITS_UC_DO_R_DISTANCE_3D: i32 = 3222;
pub const EE_UNITS_UC_DO_R_TILE_FREE: i32 = 3223;
pub const EE_UNITS_UC_DO_R_FACES_TARGET: i32 = 3224;
pub const EE_UNITS_UC_DO_R_BASE_SURROUNDING: i32 = 3225;
pub const EE_UNITS_UC_DO_R_UPGRADE_INIT: i32 = 3226;

// ---- the sound bodies (`ee_units_uc_rest_sound`'s `what`)
pub const EE_UNITS_UC_SOUND_BASE: i32 = 3240;
pub const EE_UNITS_UC_SOUND_LAND_PRIEST: i32 = 3241;
pub const EE_UNITS_UC_SOUND_HELICOPTER_PRIEST: i32 = 3242;
pub const EE_UNITS_UC_SOUND_SPACESHIP: i32 = 3243;

const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_GATHER: i64 = 6;
const GOAL_EXPLORE: i64 = 10;
const GOAL_CAST_CALAMITY: i64 = 11;
const GOAL_HEAL_UNIT: i64 = 15;
const TERRAIN_DEEP_WATER: i64 = 1;
const TERRAIN_CLIFF: i64 = 9;
const TERRAIN_SPACE: i64 = 12;
/// `kWorldHighGroundHeight`, `kWorldLowGroundHeight`, `kWorldHighGroundAttackFactor`, `kWorldLowGroundAttackFactor`
/// (the world table's rows, ee/EEWorldConstants.h)
pub const WORLD_ELEVATION_ROWS: [i64; 4] = [53, 54, 55, 56];

/// The Rust these bodies call (as the C++ called it).
pub trait UcRestRust {
    /// `ee_units_mv_path_toward(unit, from, to, snap)` (`PathToward`'s one call)
    fn path_toward(&mut self, unit: u64, from: [f32; 3], to: [f32; 3], snap: bool);
    /// `ee_units_uu_aim_point(unit, target, out)` (its UUTL note is the original call's)
    fn aim_point(&mut self, unit: u64, target: [f32; 3]) -> [f32; 3];
    /// `ee_units_uu_units_near(center, radius, units)`: the units
    fn units_near(&mut self, center: [f32; 3], radius: f32) -> Vec<u64>;
    /// `ee_units_uu_block_ring(corners, x, y, w, h, tiles, append, 1)`
    fn block_ring(&mut self, corners: i32, ring: [i32; 4], tiles: u64, append: bool);
}

fn location<H: UcHost + ?Sized>(s: &mut S<'_, H>, unit: u64) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    s.ab(EE_UNITS_UC_LOCATION_MATRIX, unit, addr(&mut t.m) as i64);
    t
}

fn position_of(t: &GeTransformation) -> [f32; 3] {
    [t.m[0][3], t.m[1][3], t.m[2][3]]
}

/// `EEHelicopterPriestPathToGoal(class, unit)`: a path toward the goal's point (an attack's aim point, a cast's
/// priest destination, a heal's location; cruise altitude over an exploration), at its landing point.
pub fn helicopter_priest_path<H: UcHost + UcRestRust + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h: &mut *h, class, unit };
    let position = position_of(&location(&mut s, unit));
    let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
    let mut target = [0.0f32; 3];
    let kind = s.a(EE_UNITS_UC_GOAL_KIND, goal);
    if kind == GOAL_ATTACK_UNIT {
        let mut enemy = [0.0f32; 3];
        s.point(EE_UNITS_UC_R_FIND_DROP_LOCATION, 0, 0, 0, &mut enemy);
        target = h.aim_point(unit, enemy);
    } else if kind == GOAL_CAST_CALAMITY {
        s.point(EE_UNITS_UC_R_GOAL_DESTINATION, goal, 0, 0, &mut target);
        s.point(EE_UNITS_UC_R_PRIEST_DESTINATION, 0, 0, 0, &mut target);
    } else if kind == GOAL_HEAL_UNIT {
        s.point(EE_UNITS_UC_GOAL_LOCATION, goal, 0, 0, &mut target);
    } else {
        s.point(EE_UNITS_UC_R_GOAL_DESTINATION, goal, 0, 0, &mut target);
    }
    let mut s = S { h: &mut *h, class, unit };
    if s.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_EXPLORE {
        target[2] = s.float(EE_UNITS_UC_R_CRUISE_ALTITUDE, 0, 0);
    }
    s.point(EE_UNITS_UC_R_LANDING_POINT, 0, 0, 0, &mut target);
    h.path_toward(unit, position, target, false);
}

/// `HelicopterPriestUnitCreated(unit)`: a new helicopter priest at its height over its point.
pub fn helicopter_priest_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut t = location(&mut S { h: &mut *h, class, unit }, unit);
    base_unit_created(&mut *h, class, unit);
    let mut s = S { h, class, unit };
    let mut f = [t.m[0][3], t.m[1][3], t.m[2][3], 0.0];
    s.f(EE_UNITS_UC_R_HEIGHT_AT, 0, 0, 0, &mut f);
    let z = f[0];
    t.m[2][3] = z;
    s.a(EE_UNITS_UC_R_SET_TRANSFORMATION, addr(&mut t.m));
    s.with_float(EE_UNITS_UC_R_SET_HEIGHT, 0, z);
}

fn find_resource_of_kind<H: UcHost + ?Sized>(
    s: &mut S<'_, H>,
    center: u64,
    resource: u64,
    range: i32,
    kind: u32,
    units: u64,
) -> u64 {
    let mut f = [f32::from_bits(range as u32), f32::from_bits(kind), 0.0, 0.0];
    s.f(EE_UNITS_UC_R_FIND_RESOURCE_OF_KIND, center, resource as i64, units as i64, &mut f) as u64
}

/// `FindNextResource(unit, resource, kind, units)`: a resource of the kind within five lines of sight, around the
/// gather goal's storage first when the unit can get to it.
pub fn find_next_resource<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    unit: u64,
    resource: u64,
    kind: u32,
    units: u64,
) -> u64 {
    let mut s = S { h, class, unit };
    let goal = s.s(EE_UNITS_UC_GOAL_STATE_SELF) as u64;
    let range = lrintf(s.float(EE_UNITS_UC_R_LINE_OF_SIGHT, 0, 0) * 5.0);
    if goal != 0 && s.a(EE_UNITS_UC_GOAL_KIND, goal) == GOAL_GATHER {
        let storage = s.a(EE_UNITS_UC_R_GATHER_RESOURCE, goal) as u64;
        if storage != 0 && s.a(EE_UNITS_UC_R_CAN_GET_TO_UNIT, storage) != 0 {
            let found = find_resource_of_kind(&mut s, storage, resource, range, kind, units);
            if found != 0 {
                return found;
            }
        }
    }
    find_resource_of_kind(&mut s, unit, resource, range, kind, units)
}

/// `GetElevationFactor(unit, target)`: the attack's high or low ground factor (the rules' attacker, launcher and
/// target categories; the launcher's height for a launched shot).
pub fn elevation_factor<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, target: u64) -> f32 {
    let mut s = S { h, class, unit };
    let mut category = s.s(EE_UNITS_UC_R_CATEGORY);
    let target_category = s.a(EE_UNITS_UC_R_CATEGORY_OF, target);
    let own_height = if s.b(EE_UNITS_UC_R_ELEVATION_LAUNCHER, category) != 0 {
        category = s.s(EE_UNITS_UC_R_LAUNCHER_CATEGORY);
        false
    } else {
        true
    };
    let mut input = EeCombatElevationIn {
        applies: i32::from(
            s.b(EE_UNITS_UC_R_ELEVATION_ATTACKER, category) != 0
                && s.b(EE_UNITS_UC_R_ELEVATION_TARGET, target_category) != 0
                && target != 0,
        ),
        ..Default::default()
    };
    if input.applies != 0 {
        input.own_z =
            if own_height { position_of(&location(&mut s, unit))[2] } else { s.float(EE_UNITS_UC_R_HEIGHT, 0, 0) };
        input.target_z = position_of(&location(&mut s, target))[2];
        input.high_height = s.float(EE_UNITS_UC_R_WORLD_FLOAT, 0, WORLD_ELEVATION_ROWS[0]);
        input.low_height = s.float(EE_UNITS_UC_R_WORLD_FLOAT, 0, WORLD_ELEVATION_ROWS[1]);
        input.high_bonus = s.float(EE_UNITS_UC_R_HIGH_GROUND_BONUS, 0, 0);
        input.high_factor = s.float(EE_UNITS_UC_R_WORLD_FLOAT, 0, WORLD_ELEVATION_ROWS[2]);
        input.low_factor = s.float(EE_UNITS_UC_R_WORLD_FLOAT, 0, WORLD_ELEVATION_ROWS[3]);
    }
    s.float(EE_UNITS_UC_R_ELEVATION, addr(&mut input), 0)
}

/// The sound parameters' outs (`float* outVolume, float* outPan, float* outPitch`)
#[derive(Clone, Copy)]
pub struct SoundOuts {
    pub volume: *mut f32,
    pub pan: *mut f32,
    pub pitch: *mut f32,
}

impl SoundOuts {
    /// # Safety
    /// The three are live floats
    unsafe fn get(&self) -> [f32; 4] {
        // SAFETY: the caller's contract
        unsafe { [*self.volume, *self.pan, *self.pitch, 0.0] }
    }
    /// # Safety
    /// The three are live floats
    unsafe fn set(&self, f: &[f32; 4]) {
        // SAFETY: the caller's contract
        unsafe {
            *self.volume = f[0];
            *self.pan = f[1];
            *self.pitch = f[2];
        }
    }
}

/// `BaseGetSoundParameters(unit, sound, outs)`: the sound's parameters at the unit's point (its tile's centre
/// corner and height for a simple unit), with the class's distance factor 4.
///
/// # Safety
/// `outs` are live floats
pub unsafe fn base_sound<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, sound: u64, outs: SoundOuts) {
    let mut s = S { h, class, unit: 0 };
    let owner = s.a(EE_UNITS_UC_COMPLEX_OF, unit) as u64;
    let point = if owner != 0 {
        position_of(&location(&mut s, owner))
    } else {
        [
            s.a(EE_UNITS_UC_R_TILE_X_OF, unit) as i32 as f32,
            s.a(EE_UNITS_UC_R_TILE_Y_OF, unit) as i32 as f32,
            s.float(EE_UNITS_UC_R_TILE_HEIGHT_OF, unit, 0),
        ]
    };
    let volume = if sound != 0 { s.float(EE_UNITS_UC_R_SOUND_VOLUME, sound, 0) } else { 1.0 };
    let mut f = [point[0], point[1], point[2], volume];
    s.f(EE_UNITS_UC_R_COMPUTE_SOUND, 0, 0, 0, &mut f);
    // SAFETY: the caller's contract
    unsafe { outs.set(&f) };
}

/// `EEGetDopplerPitch(unit, sound, outs)` (its note, as the original called it)
///
/// # Safety
/// `outs` are live floats
unsafe fn doppler<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, sound: u64, outs: SoundOuts) {
    // SAFETY: the caller's contract
    unsafe {
        let mut f = outs.get();
        S { h, class, unit: 0 }.f(EE_UNITS_UC_R_DOPPLER_PITCH, unit, sound as i64, 0, &mut f);
        outs.set(&f);
    }
}

/// The sound body `what` (`EE_UNITS_UC_SOUND_*`) of `class` for `unit` and `sound`.
///
/// # Safety
/// `outs` are live floats
pub unsafe fn sound<H: UcHost + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, sound: u64, outs: SoundOuts) {
    // SAFETY: the caller's contract
    unsafe {
        base_sound(h, class, unit, sound, outs);
        match what {
            EE_UNITS_UC_SOUND_LAND_PRIEST => {
                let mut s = S { h, class, unit: 0 };
                let owner = s.a(EE_UNITS_UC_COMPLEX_OF, unit) as u64;
                if owner != 0 && s.a(EE_UNITS_UC_R_IS_SEEN, owner) == 0 {
                    *outs.volume = 0.0;
                }
            }
            EE_UNITS_UC_SOUND_HELICOPTER_PRIEST => {
                doppler(h, class, unit, sound, outs);
                let mut s = S { h, class, unit: 0 };
                if s.a(EE_UNITS_UC_R_HAS_TILE_NODE, unit) != 0 {
                    let owner = s.a(EE_UNITS_UC_COMPLEX_OF, unit) as u64;
                    if s.a(EE_UNITS_UC_R_IS_SEEN, owner) == 0 {
                        *outs.volume = 0.0;
                    }
                }
            }
            EE_UNITS_UC_SOUND_SPACESHIP => doppler(h, class, unit, sound, outs),
            _ => {}
        }
    }
}

/// `EEComplexUnit::UpdateOrbitals()`: each orbital's update.
pub fn update_orbitals<H: UcHost + ?Sized>(h: &mut H, unit: u64) {
    let mut s = S { h, class: 0, unit };
    let count = s.s(EE_UNITS_UC_R_ORBITAL_COUNT) as u32;
    for i in 0..count {
        s.b(EE_UNITS_UC_R_ORBITAL_UPDATE, i64::from(i));
    }
}

/// `BasicPriestLoadMedia(player)` (`load`) / `BasicPriestUnloadMedia(player)`: the conversion indicator, then the
/// class's media.
pub fn priest_media<H: UcHost + ?Sized>(h: &mut H, class: u64, player: u64, load: bool) {
    let mut s = S { h, class, unit: 0 };
    if load {
        s.s(EE_UNITS_UC_R_LOAD_INDICATOR);
        s.a(EE_UNITS_UC_R_BASE_LOAD_MEDIA, player);
    } else {
        s.s(EE_UNITS_UC_R_UNLOAD_INDICATOR);
        s.a(EE_UNITS_UC_R_BASE_UNLOAD_MEDIA, player);
    }
}

/// `BaseAddToTile(unit)` (`add`) / `BaseRemoveFromTile(unit)`: the unit on (off) its tile's list.
pub fn base_tile<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, add: bool) {
    let mut s = S { h, class, unit: 0 };
    let tile = s.a(EE_UNITS_UC_R_RTS_TILE_OF, unit) as u64;
    if tile != 0 {
        s.ab(if add { EE_UNITS_UC_R_RTS_TILE_ADD } else { EE_UNITS_UC_R_RTS_TILE_REMOVE }, tile, unit as i64);
    }
}

/// `RegisterUnitClass(class)`: the class (referenced) in the table under its unit type, the type in the list.
pub fn register_class<H: UcHost + ?Sized>(h: &mut H, class: u64) {
    let mut s = S { h, class: 0, unit: 0 };
    if class == 0 {
        return;
    }
    s.a(EE_UNITS_UC_R_CLASS_REFERENCE, class);
    let unit_type = s.a(EE_UNITS_UC_CLASS_UNIT_TYPE, class);
    s.ab(EE_UNITS_UC_R_TABLE_SET, class, unit_type);
    let unit_type = s.a(EE_UNITS_UC_CLASS_UNIT_TYPE, class);
    s.b(EE_UNITS_UC_R_TYPES_PUSH, unit_type);
}

/// `ReleaseUnitClass(type)`: the table's class for the type dereferenced and cleared.
pub fn release_class<H: UcHost + ?Sized>(h: &mut H, unit_type: i64) {
    let mut s = S { h, class: 0, unit: 0 };
    let entry = s.b(EE_UNITS_UC_R_TABLE_GET, unit_type) as u64;
    if entry != 0 {
        s.a(EE_UNITS_UC_R_CLASS_DEREFERENCE, entry);
    }
    s.ab(EE_UNITS_UC_R_TABLE_SET, 0, unit_type);
}

/// `ReleaseUnitClasses()`: every table slot's class released.
pub fn release_classes<H: UcHost + ?Sized>(h: &mut H) {
    let mut i: u32 = 0;
    loop {
        let count = S { h: &mut *h, class: 0, unit: 0 }.s(EE_UNITS_UC_R_CLASS_COUNT) as u32;
        if i >= count {
            break;
        }
        release_class(&mut *h, i64::from(i));
        i += 1;
    }
}

/// `UnitRemoved()`: one unit of the class fewer; the time the last one went.
pub fn unit_removed<H: UcHost + ?Sized>(h: &mut H, class: u64) {
    let mut s = S { h, class, unit: 0 };
    if s.s(EE_UNITS_UC_R_UNIT_REMOVED) as i32 == 0 {
        s.s(EE_UNITS_UC_R_SET_GONE_TIME);
    }
}

/// `EEComplexUnit::GetInverseMaxVelocity()`: the active group's, or the unit's stats'.
pub fn inverse_max_velocity<H: UcHost + ?Sized>(h: &mut H, unit: u64) -> f32 {
    let mut s = S { h, class: 0, unit };
    let group = s.s(EE_UNITS_UC_R_ACTIVE_GROUP) as u64;
    if group != 0 {
        return s.float(EE_UNITS_UC_R_GROUP_INVERSE_VELOCITY, group, 0);
    }
    s.float(EE_UNITS_UC_R_STATS_INVERSE_VELOCITY, 0, 0)
}

/// `EEComplexUnit::SetCamouflaged(value)`: the camouflage timer's top bit.
pub fn set_camouflaged<H: UcHost + ?Sized>(h: &mut H, unit: u64, value: bool) {
    let mut s = S { h, class: 0, unit };
    let timer = s.s(EE_UNITS_UC_R_CAMOUFLAGE_TIMER) as u32;
    let timer = if value { timer | 0x8000_0000 } else { timer & 0x7fff_ffff };
    s.b(EE_UNITS_UC_R_SET_CAMOUFLAGE_TIMER, i64::from(timer));
}

/// `EruptVolcano(unit)`: an attack on a random point of the volcano calamity's around the unit (statements of the
/// buildings' host).
pub fn erupt_volcano<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let mut volcano: u64 = 0;
    if s.a(EE_UNITS_UCB_R_VOLCANO_CLASS, addr(&mut volcano)) == 0 {
        s.s(EE_UNITS_UCB_R_VOLCANO_VERIFY);
    }
    let mut point = [0.0f32; 3];
    s.point(EE_UNITS_UC_POSITION, unit, 0, 0, &mut point);
    s.point(EE_UNITS_UCB_R_RANDOM_LOCATION, volcano, 0, 0, &mut point);
    s.point(EE_UNITS_UCB_R_ATTACK_GROUND, 0, 0, 0, &mut point);
    true
}

/// `GetLastLinked(unit)`: the last of the unit's linked parts.
pub fn last_linked<H: UcHost + ?Sized>(h: &mut H, mut unit: u64) -> u64 {
    let mut s = S { h, class: 0, unit: 0 };
    loop {
        let next = s.a(EE_UNITS_UC_R_NEXT_PART, unit) as u64;
        if next == 0 {
            return unit;
        }
        unit = next;
    }
}

/// `GetFourthPart(gate)`: the part before the last linked one's previous (`GetLastLinkedCarrier`'s).
pub fn fourth_part<H: UcHost + ?Sized>(h: &mut H, gate: u64) -> u64 {
    let last = last_linked(&mut *h, gate);
    let mut s = S { h, class: 0, unit: 0 };
    let carrier = s.a(EE_UNITS_UC_R_PREVIOUS_PART, last) as u64;
    if carrier != 0 {
        s.a(EE_UNITS_UC_R_PREVIOUS_PART, carrier) as u64
    } else {
        0
    }
}

/// `GetSecondPart(gate)`: the part before the fourth.
pub fn second_part<H: UcHost + ?Sized>(h: &mut H, gate: u64) -> u64 {
    let fourth = fourth_part(&mut *h, gate);
    if fourth != 0 {
        S { h, class: 0, unit: 0 }.a(EE_UNITS_UC_R_PREVIOUS_PART, fourth) as u64
    } else {
        0
    }
}

/// `EEGetFirstPart(unit)`: the first of the unit's parts.
pub fn first_part<H: UcHost + ?Sized>(h: &mut H, mut unit: u64) -> u64 {
    let mut s = S { h, class: 0, unit: 0 };
    loop {
        let previous = s.a(EE_UNITS_UC_R_PREVIOUS_PART, unit) as u64;
        if previous == 0 {
            return unit;
        }
        unit = previous;
    }
}

/// `FindTargetNear(unit, range, out)`: the last unit within the range (the line of sight for 0) the unit can heal
/// and reach.
///
/// # Safety
/// `out` is a writable `EEUnit*`
pub unsafe fn find_target_near<H: UcHost + UcRestRust + ?Sized>(
    h: &mut H,
    class: u64,
    unit: u64,
    range: f32,
    out: *mut u64,
) -> bool {
    let mut s = S { h: &mut *h, class, unit };
    let range = if range == 0.0 { s.float(EE_UNITS_UC_R_LINE_OF_SIGHT, 0, 0) } else { range };
    let mut position = [0.0f32; 3];
    s.point(EE_UNITS_UC_POSITION, unit, 0, 0, &mut position);
    // SAFETY: the caller's contract
    unsafe { *out = 0 };
    let units = h.units_near(position, range);
    let mut s = S { h, class, unit };
    for &candidate in units.iter().rev() {
        if s.a(EE_UNITS_UC_R_CAN_HEAL, candidate) == 0 || s.a(EE_UNITS_UC_R_CAN_REACH, candidate) == 0 {
            continue;
        }
        // SAFETY: the caller's contract
        unsafe { *out = candidate };
        return true;
    }
    false
}

/// `GetContainerContinent(unit, target)`: the continent of the last surrounding tile (asked from the end) that is
/// land, no cliff, and the target's continent when there is a target; else the first tile's.
pub fn container_continent<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, target: u64) -> u64 {
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UC_R_TILES_CLEAR);
    let mut found = false;
    let mut continent = 0u64;
    let target_continent = if target != 0 { s.a(EE_UNITS_UC_R_CONTINENT_OF, target) as u64 } else { 0 };
    s.s(EE_UNITS_UC_R_SURROUNDING_TILES);
    let size = s.s(EE_UNITS_UC_R_TILES_SIZE);
    let mut i = size - 1;
    while i >= 0 && !found {
        continent = s.b(EE_UNITS_UC_R_CONTINENT_AT_TILE, i) as u64;
        let tile = s.b(EE_UNITS_UC_R_MAP_TILE_AT_TILE, i) as u64;
        found = continent != 0
            && tile != 0
            && s.a(EE_UNITS_UC_R_CONTINENT_IS_LAND, continent) != 0
            && s.a(EE_UNITS_UC_R_TERRAIN_OF, tile) != TERRAIN_CLIFF
            && (target_continent == 0 || continent == target_continent);
        i -= 1;
    }
    continent
}

/// `EEComplexUnit::DistanceSquared3D(point)`: the squared distance from the unit's point.
pub fn distance_3d<H: UcHost + ?Sized>(h: &mut H, unit: u64, point: [f32; 3]) -> f32 {
    let p = position_of(&location(&mut S { h, class: 0, unit }, unit));
    let dz = p[2] - point[2];
    let dy = p[1] - point[1];
    let dx = p[0] - point[0];
    dx * dx + dy * dy + dz * dz
}

/// `EEIsTileFree(unit, x, y)`: a map tile that is no cliff, deep water or space, and not on the map's edge.
pub fn tile_free<H: UcHost + ?Sized>(h: &mut H, x: u32, y: u32) -> bool {
    let mut s = S { h, class: 0, unit: 0 };
    let tile = s.abc(EE_UNITS_UC_GET_TILE, 0, i64::from(x), i64::from(y)) as u64;
    tile != 0
        && {
            let terrain = s.a(EE_UNITS_UC_R_TERRAIN_OF, tile);
            terrain != TERRAIN_CLIFF && terrain != TERRAIN_DEEP_WATER && terrain != TERRAIN_SPACE
        }
        && s.abc(EE_UNITS_UC_R_EDGE_TILE, 0, i64::from(x), i64::from(y)) == 0
}

/// `FacesTarget(unit, target, tolerance)`'s answer (its CBQR record stays the C++ member's): `FacesPoint` toward
/// the target's point (its tile's centre for a simple unit).
pub fn faces_target<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, target: u64, tolerance: bool) -> bool {
    let mut s = S { h, class, unit };
    let owner = s.a(EE_UNITS_UC_COMPLEX_OF, target) as u64;
    let mut f = [0.0f32; 4];
    if owner != 0 {
        let p = position_of(&location(&mut s, owner));
        f[0] = p[0];
        f[1] = p[1];
    } else {
        f[0] = s.a(EE_UNITS_UC_R_TILE_X_OF, target) as i32 as f32 + 0.5;
        f[1] = s.a(EE_UNITS_UC_R_TILE_Y_OF, target) as i32 as f32 + 0.5;
    }
    s.f(EE_UNITS_UC_R_FACES_POINT, 0, i64::from(tolerance), 0, &mut f) != 0
}

/// `BaseGetSurroundingTiles(unit, tiles, corners, append)`: the ring around the class's footprint at the unit's
/// tile (the list emptied first unless appending).
pub fn base_surrounding<H: UcHost + UcRestRust + ?Sized>(
    h: &mut H,
    class: u64,
    unit: u64,
    tiles: u64,
    corners: i32,
    append: bool,
) {
    let mut s = S { h: &mut *h, class, unit };
    if !append && s.a(EE_UNITS_UC_R_VEC_TILES_SIZE, tiles) > 0 {
        s.a(EE_UNITS_UC_R_VEC_TILES_CLEAR, tiles);
    }
    let x = s.a(EE_UNITS_UC_R_TILE_X_OF, unit) as i32;
    let y = s.a(EE_UNITS_UC_R_TILE_Y_OF, unit) as i32;
    let size = s.s(EE_UNITS_UC_TILE_SIZE) as i32;
    let size2 = s.s(EE_UNITS_UC_TILE_SIZE) as i32;
    h.block_ring(corners, [x, y, size, size2], tiles, append);
}

/// `EEUnitClassUpgrade::Init()`: the copy reset to its class (media, button, stats), then its improvements
/// reapplied.
pub fn upgrade_init<H: UcHost + ?Sized>(h: &mut H, upgrade: u64) {
    let mut s = S { h, class: 0, unit: 0 };
    s.a(EE_UNITS_UC_R_UPGRADE_BUTTON, upgrade);
    s.a(EE_UNITS_UC_R_UPGRADE_MODEL, upgrade);
    for i in 0..2 {
        s.ab(EE_UNITS_UC_R_UPGRADE_ACKNOWLEDGE, upgrade, i);
    }
    s.a(EE_UNITS_UC_R_UPGRADE_CREATION, upgrade);
    s.a(EE_UNITS_UC_R_UPGRADE_DEATH, upgrade);
    for i in 0..2 {
        s.ab(EE_UNITS_UC_R_UPGRADE_SELECTION, upgrade, i);
    }
    s.a(EE_UNITS_UC_R_UPGRADE_RANGE, upgrade);
    s.a(EE_UNITS_UC_R_UPGRADE_INIT, upgrade);
    s.a(EE_UNITS_UC_R_UPGRADE_REAPPLY, upgrade);
    s.a(EE_UNITS_UC_R_UPGRADE_INIT_FINISH, upgrade);
}

/// Runs body `what` (`EE_UNITS_UC_DO_R_*`) of `class` on `unit`: `a`, `b`, `c` its arguments, `f` its floats (a
/// point in, a float out); the answer, or 0.
///
/// # Safety
/// `f` is a writable `[f32; 4]`; `c` the out target of `FindTargetNear` (`b` its range's bits).
#[allow(clippy::too_many_arguments)]
pub unsafe fn run<H: UcHost + UcRestRust + ?Sized>(
    h: &mut H,
    what: i32,
    class: u64,
    unit: u64,
    a: u64,
    b: i64,
    c: i64,
    f: *mut [f32; 4],
) -> i64 {
    // SAFETY: the caller's contract
    let f = unsafe { &mut *f };
    match what {
        EE_UNITS_UC_DO_R_HELICOPTER_PRIEST_PATH => helicopter_priest_path(h, class, unit),
        EE_UNITS_UC_DO_R_HELICOPTER_PRIEST_CREATED => helicopter_priest_created(h, class, unit),
        EE_UNITS_UC_DO_R_FIND_NEXT_RESOURCE => return find_next_resource(h, class, unit, a, b as u32, c as u64) as i64,
        EE_UNITS_UC_DO_R_ELEVATION_FACTOR => f[0] = elevation_factor(h, class, unit, a),
        EE_UNITS_UC_DO_R_UPDATE_ORBITALS => update_orbitals(h, unit),
        EE_UNITS_UC_DO_R_PRIEST_LOAD_MEDIA => priest_media(h, class, a, true),
        EE_UNITS_UC_DO_R_PRIEST_UNLOAD_MEDIA => priest_media(h, class, a, false),
        EE_UNITS_UC_DO_R_BASE_ADD_TO_TILE => base_tile(h, class, a, true),
        EE_UNITS_UC_DO_R_BASE_REMOVE_FROM_TILE => base_tile(h, class, a, false),
        EE_UNITS_UC_DO_R_REGISTER_CLASS => register_class(h, a),
        EE_UNITS_UC_DO_R_RELEASE_CLASS => release_class(h, b),
        EE_UNITS_UC_DO_R_RELEASE_CLASSES => release_classes(h),
        EE_UNITS_UC_DO_R_UNIT_REMOVED => unit_removed(h, class),
        EE_UNITS_UC_DO_R_INVERSE_MAX_VELOCITY => f[0] = inverse_max_velocity(h, unit),
        EE_UNITS_UC_DO_R_SET_CAMOUFLAGED => set_camouflaged(h, unit, b != 0),
        EE_UNITS_UC_DO_R_ERUPT_VOLCANO => return i64::from(erupt_volcano(h, class, unit)),
        EE_UNITS_UC_DO_R_LAST_LINKED => return last_linked(h, a) as i64,
        EE_UNITS_UC_DO_R_FOURTH_PART => return fourth_part(h, a) as i64,
        EE_UNITS_UC_DO_R_SECOND_PART => return second_part(h, a) as i64,
        EE_UNITS_UC_DO_R_FIRST_PART => return first_part(h, a) as i64,
        // SAFETY: the caller's contract
        EE_UNITS_UC_DO_R_FIND_TARGET_NEAR => {
            return i64::from(unsafe {
                find_target_near(h, class, unit, f32::from_bits(b as u32), c as usize as *mut u64)
            })
        }
        EE_UNITS_UC_DO_R_CONTAINER_CONTINENT => return container_continent(h, class, unit, a) as i64,
        EE_UNITS_UC_DO_R_DISTANCE_3D => f[0] = distance_3d(h, unit, [f[0], f[1], f[2]]),
        EE_UNITS_UC_DO_R_TILE_FREE => return i64::from(tile_free(h, b as u32, c as u32)),
        EE_UNITS_UC_DO_R_FACES_TARGET => return i64::from(faces_target(h, class, unit, a, b != 0)),
        EE_UNITS_UC_DO_R_BASE_SURROUNDING => base_surrounding(h, class, unit, a, b as i32, c != 0),
        EE_UNITS_UC_DO_R_UPGRADE_INIT => upgrade_init(h, a),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    /// The class host (EEUnitClass.cpp), or the buildings' for `EruptVolcano` (its statements pass on to the class
    /// host's)
    struct GameR {
        buildings: bool,
    }

    impl UcHost for GameR {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if self.buildings {
                // SAFETY: the body runs inside the C++ member that handed over its live class and unit
                unsafe {
                    crate::units::ucb_release::game::ee_units_ucb_host(
                        op,
                        class as usize as *mut c_void,
                        unit as usize as *mut c_void,
                        a,
                        b,
                        c,
                        f,
                    )
                }
            } else {
                crate::units::uc_body::game::Game.uc(op, class, unit, a, b, c, f)
            }
        }
    }

    impl UcRestRust for GameR {
        fn path_toward(&mut self, unit: u64, from: [f32; 3], to: [f32; 3], snap: bool) {
            crate::units::cls_move_host::ee_units_mv_path_toward(
                unit,
                from[0],
                from[1],
                from[2],
                to[0],
                to[1],
                to[2],
                u8::from(snap),
            );
        }
        fn aim_point(&mut self, unit: u64, target: [f32; 3]) -> [f32; 3] {
            let mut out = [0.0f32; 3];
            // SAFETY: the unit is live; the points are locals
            unsafe { crate::units::uu_ffi::ee_units_uu_aim_point(unit, &target, &mut out) };
            out
        }
        fn units_near(&mut self, center: [f32; 3], radius: f32) -> Vec<u64> {
            crate::units::uu_ffi::units_near(center, radius)
        }
        fn block_ring(&mut self, corners: i32, ring: [i32; 4], tiles: u64, append: bool) {
            // SAFETY: the caller's live tile vector
            unsafe {
                crate::units::uu_ffi::ee_units_uu_block_ring(
                    corners,
                    ring[0],
                    ring[1],
                    ring[2],
                    ring[3],
                    tiles as usize as *mut std::ffi::c_void,
                    u8::from(append),
                    1,
                )
            }
        }
    }

    /// The class body `what` (`EE_UNITS_UC_DO_R_*`) of `class` on `unit` (see [`run`]).
    ///
    /// # Safety
    /// `class` a live `EEUnitClass` (or NULL for the table's bodies), `unit` a live `EEComplexUnit` (or NULL), `a`
    /// a live object of the body's, `f` four writable floats, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uc_rest(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        f: *mut [f32; 4],
    ) -> i64 {
        let mut local = [0.0f32; 4];
        let f = if f.is_null() { &mut local as *mut [f32; 4] } else { f };
        let mut h = GameR { buildings: what == EE_UNITS_UC_DO_R_ERUPT_VOLCANO };
        // SAFETY: the caller's contract
        unsafe { run(&mut h, what, class as usize as u64, unit as usize as u64, a, b, c, f) }
    }

    /// The class body `what` (`EE_UNITS_UC_DO_R_*`) of `class` on `unit` that answers a float (`a` its object,
    /// (`x`, `y`, `z`) its point).
    ///
    /// # Safety
    /// As [`ee_units_uc_rest`].
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uc_rest_f(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        x: f32,
        y: f32,
        z: f32,
    ) -> f32 {
        let mut f = [x, y, z, 0.0];
        // SAFETY: the caller's contract
        unsafe { ee_units_uc_rest(what, class, unit, a, 0, 0, &mut f) };
        f[0]
    }

    /// The sound body `what` (`EE_UNITS_UC_SOUND_*`) of `class` for `unit` (an `EEUnit`) and `sound`.
    ///
    /// # Safety
    /// `class`, `unit` live; `sound` a live `EESoundObject` or NULL; the outs live floats.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uc_rest_sound(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        sound: *mut c_void,
        volume: *mut f32,
        pan: *mut f32,
        pitch: *mut f32,
    ) {
        let outs = SoundOuts { volume, pan, pitch };
        // SAFETY: the caller's contract
        unsafe {
            super::sound(
                &mut GameR { buildings: false },
                what,
                class as usize as u64,
                unit as usize as u64,
                sound as usize as u64,
                outs,
            )
        }
    }
}

#[cfg(test)]
#[path = "uc_rest_tests.rs"]
mod tests;
