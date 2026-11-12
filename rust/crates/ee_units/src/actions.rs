//! The classes' `ActionChanged` as Rust decisions (RUST_PLAN R5b.4, the thirteenth round): what a unit does when its
//! AI script's command changes, decided over the unit store (the command, the goal, the herd, the class's domains and
//! category, the container, the resource amount, the unit type) and the host's reads, each consequence one host
//! operation in the original's order. `EEUnitClass::ActionChanged` (0x62db0f, the common handling `HandleActionChanged`
//! calls) and the overrides of `EEUCLand` (0x6138c0), `EEUCNaval` (0x61815b), `EEUCSubmergedNaval` (0x61b0f0),
//! `EEUCSpaceship` (0x61a531) and `EEUCBasicPriest` (0x60dd2a); the host is EEUnitUpdate.cpp's (one table with the
//! classes' `Update`s).
//!
//! The heading arithmetic of the contact location (`Apply`, `UMArcTan2`), the objects made (the location and heal
//! goals, the paratroop plane), the sounds and effects stay host operations. An operation may run another unit's (or
//! this one's) `ActionChanged` again (the next part's rotting, the idle restart): the record is read again after every
//! operation and never held across one.
//!
//! Determinism (RUST_PLAN 2.1): the idle animation's random draw is a read made where the original draws it (and the
//! animation time's draw an operation), so the one stream keeps its order.

use crate::store::{with_store, EeUnitRecord};

// ---- the classes (`ee_units_action_changed`'s `kind`)
/// `EEUnitClass::ActionChanged` (every class's `HandleActionChanged`)
pub const EE_UNITS_ACTION_BASE: i32 = 1;
pub const EE_UNITS_ACTION_LAND: i32 = 2;
pub const EE_UNITS_ACTION_NAVAL: i32 = 3;
pub const EE_UNITS_ACTION_SUBMERGED_NAVAL: i32 = 4;
pub const EE_UNITS_ACTION_SPACESHIP: i32 = 5;
pub const EE_UNITS_ACTION_BASIC_PRIEST: i32 = 6;
/// the fourteenth round's: `EEUCBasicCitizen` (and the naval citizen, whose forwarder is gone), `EEUCBuilding` (and
/// the naval and space buildings'), `EEUCBuildingPriest`, `EEUCGate`, `EEUCMiningBuilding`, `EEUCHelicopter`,
/// `EEUCHelicopterTransport`, `EEUCHelicopterPriest`, `EEUCAirAnimal`, `EEUCLandable`, `EEUCAirplane`,
/// `EEUCPlanetSpaceship`
pub const EE_UNITS_ACTION_CITIZEN: i32 = 7;
pub const EE_UNITS_ACTION_BUILDING: i32 = 8;
pub const EE_UNITS_ACTION_BUILDING_PRIEST: i32 = 9;
pub const EE_UNITS_ACTION_GATE: i32 = 10;
pub const EE_UNITS_ACTION_MINING_BUILDING: i32 = 11;
pub const EE_UNITS_ACTION_HELICOPTER: i32 = 12;
pub const EE_UNITS_ACTION_HELICOPTER_TRANSPORT: i32 = 13;
pub const EE_UNITS_ACTION_HELICOPTER_PRIEST: i32 = 14;
pub const EE_UNITS_ACTION_AIR_ANIMAL: i32 = 15;
pub const EE_UNITS_ACTION_LANDABLE: i32 = 16;
pub const EE_UNITS_ACTION_AIRPLANE: i32 = 17;
pub const EE_UNITS_ACTION_PLANET_SPACESHIP: i32 = 18;

// ---- the host's reads (the update host's `read`; the store's fields as `EE_UNITS_UPD_R_*` for a unit with no record)
/// `GetAnimation()`, `EEGetAttackFrameSet(inUnit)`, `IsSwimming()`, the class's `IsLandBuilding()`, the class's
/// `mAttack > 0`, `mStats->mCanFireOverWalls`
pub const EE_UNITS_ACT_R_ANIMATION: i32 = 130;
pub const EE_UNITS_ACT_R_ATTACK_FRAME_SET: i32 = 131;
pub const EE_UNITS_ACT_R_SWIMMING: i32 = 132;
pub const EE_UNITS_ACT_R_LAND_BUILDING: i32 = 133;
pub const EE_UNITS_ACT_R_CLASS_ARMED: i32 = 134;
pub const EE_UNITS_ACT_R_FIRE_OVER_WALLS: i32 = 135;
/// `gPhysics->GetRandom()` (a draw: the float's bits)
pub const EE_UNITS_ACT_R_RANDOM: i32 = 136;
/// `mUnitClass->mIsWall`, the class's `mRottingTime`, `mNextPart != NULL` (the update host's), the goal's
/// `(unsigned long)mPathAttempts > (unsigned long)GetMaxPathAttempts()`, `mUnitClass->mResourceAmount`,
/// `mInitialContactHeading == kUnitAINoHeading`, `gWorldClearing`, `GetQueueGoals(0).size() > 1`, the class's
/// `mDeathEffect != NULL`, `mStats->mDeathSound != NULL`, `mUnitClass->mBloodStainEffect != NULL`
pub const EE_UNITS_ACT_R_IS_WALL: i32 = 137;
pub const EE_UNITS_ACT_R_ROTTING_TIME: i32 = 138;
pub const EE_UNITS_ACT_R_PATH_ATTEMPTS_EXCEEDED: i32 = 139;
pub const EE_UNITS_ACT_R_CLASS_RESOURCE_AMOUNT: i32 = 140;
pub const EE_UNITS_ACT_R_NO_HEADING: i32 = 141;
pub const EE_UNITS_ACT_R_WORLD_CLEARING: i32 = 142;
pub const EE_UNITS_ACT_R_QUEUED_GOALS: i32 = 143;
pub const EE_UNITS_ACT_R_DEATH_EFFECT: i32 = 144;
pub const EE_UNITS_ACT_R_DEATH_SOUND: i32 = 145;
pub const EE_UNITS_ACT_R_BLOOD_STAIN: i32 = 146;
/// the store's fields for a unit with no record: the class's `mDomains`, `mCategory`, `mContainer != NULL`,
/// `mResourceAmount`
pub const EE_UNITS_ACT_R_DOMAINS: i32 = 150;
pub const EE_UNITS_ACT_R_CATEGORY: i32 = 151;
pub const EE_UNITS_ACT_R_CONTAINER: i32 = 152;
pub const EE_UNITS_ACT_R_RESOURCE_AMOUNT: i32 = 153;
/// the fourteenth round's: `mPlayer->IsHuman()`, `mCitizenBuildQueue.size() == 0`, the class's
/// `mAIKind == kAIKindAircraftCarrier`, `mExitingUnitID`, the class's `mAirTrafficType`, `(long)mWaypoints.size()`,
/// `mHomeBase != NULL`, `mIsClearanceGranted == true`, the class's `mProjectileClass` a bomb, `mIsCommandable`
pub const EE_UNITS_ACT_R_IS_HUMAN: i32 = 154;
pub const EE_UNITS_ACT_R_BUILD_QUEUE_EMPTY: i32 = 155;
pub const EE_UNITS_ACT_R_AIRCRAFT_CARRIER: i32 = 156;
pub const EE_UNITS_ACT_R_EXITING_UNIT: i32 = 157;
pub const EE_UNITS_ACT_R_AIR_TRAFFIC_TYPE: i32 = 158;
pub const EE_UNITS_ACT_R_WAYPOINTS: i32 = 159;
pub const EE_UNITS_ACT_R_HOME_BASE: i32 = 160;
pub const EE_UNITS_ACT_R_CLEARANCE_GRANTED: i32 = 161;
pub const EE_UNITS_ACT_R_BOMBER: i32 = 162;
pub const EE_UNITS_ACT_R_COMMANDABLE: i32 = 163;

// ---- the host's operations (the update host's `act`)
/// `ReleaseHealEffect()`; the herd's `AttackBack(inUnit, true)`, `HandleUnitHit(inUnit)`
pub const EE_UNITS_ACT_RELEASE_HEAL_EFFECT: i32 = 200;
pub const EE_UNITS_ACT_HERD_ATTACK_BACK: i32 = 201;
pub const EE_UNITS_ACT_HERD_UNIT_HIT: i32 = 202;
/// the initial contact location and heading from the unit's location (`kActionCheckRange`'s writes;
/// `kActionShouldIFollowEnemyUnit`'s through `SetInitialContactLocation`)
pub const EE_UNITS_ACT_CONTACT_CHECK_RANGE: i32 = 203;
pub const EE_UNITS_ACT_CONTACT_FOLLOW: i32 = 204;
/// the class's `Attacked(inUnit)`, `FleeAttacker(inUnit)`
pub const EE_UNITS_ACT_ATTACKED: i32 = 205;
pub const EE_UNITS_ACT_FLEE_ATTACKER: i32 = 206;
/// `SetCurrentPhysicsAction(a)`, `SetAnimation(a, -1, b)`
pub const EE_UNITS_ACT_SET_PHYSICS_ACTION: i32 = 207;
pub const EE_UNITS_ACT_SET_ANIMATION: i32 = 208;
/// a new location goal to the initial contact location, `SetGoal(goal, a, 0)`
pub const EE_UNITS_ACT_RETURN_TO_CONTACT: i32 = 209;
/// `SetTargetSearchTimer(a)`, `FleeAfterCalamity(0)`, `mNextTriggerTime = a`
pub const EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER: i32 = 210;
pub const EE_UNITS_ACT_FLEE_AFTER_CALAMITY: i32 = 211;
pub const EE_UNITS_ACT_SET_NEXT_TRIGGER_TIME: i32 = 212;
/// `SetIsAnimationPaused(true)`; `SetAnimationTime(gPhysics->GetRandom(0, the hold time of GetAnimation()))`
pub const EE_UNITS_ACT_PAUSE_ANIMATION: i32 = 213;
pub const EE_UNITS_ACT_RANDOM_ANIMATION_TIME: i32 = 214;
/// `DropPath()`, `UpdateWaterState(false)`, `mPlayer->UnitGoalDone(inUnit)`, `ClearGoals(a)`,
/// `mInitialContactHeading = kUnitAINoHeading`, `gPathManager.CancelPaths(inUnit, true)`
pub const EE_UNITS_ACT_DROP_PATH: i32 = 215;
pub const EE_UNITS_ACT_UPDATE_WATER_STATE: i32 = 216;
pub const EE_UNITS_ACT_UNIT_GOAL_DONE: i32 = 217;
pub const EE_UNITS_ACT_CLEAR_GOALS: i32 = 218;
pub const EE_UNITS_ACT_CLEAR_HEADING: i32 = 219;
pub const EE_UNITS_ACT_CANCEL_PATHS: i32 = 220;
/// `SetRottingAwayTime(a)`; the next part's `StartAction(a)` and its class's `ActionChanged`
pub const EE_UNITS_ACT_SET_ROTTING: i32 = 221;
pub const EE_UNITS_ACT_NEXT_PART_ACTION: i32 = 222;
/// the class's `UnitDied(inUnit)`, `mGoalState->Step(inUnit)`, `mPlayer->DeselectUnit(inUnit)`
pub const EE_UNITS_ACT_UNIT_DIED: i32 = 223;
pub const EE_UNITS_ACT_GOAL_STEP: i32 = 224;
pub const EE_UNITS_ACT_DESELECT: i32 = 225;
/// `SetCloakedChain(true)`, `SetOpacityChain(1.0f)`, `Kill(a != 0, false, false)`, `mPlayer->KillUnit` of the unit
/// and its next parts
pub const EE_UNITS_ACT_CLOAK_CHAIN: i32 = 226;
pub const EE_UNITS_ACT_OPAQUE_CHAIN: i32 = 227;
pub const EE_UNITS_ACT_KILL: i32 = 228;
pub const EE_UNITS_ACT_KILL_UNIT_AND_PARTS: i32 = 229;
/// `StartAction(kActionIdle)` and the class's `ActionChanged`
pub const EE_UNITS_ACT_START_IDLE: i32 = 230;
/// the class's `PathToGoal`, `MoveToTileCentre`, `Flee`, `NextWaypoint`, `FindSafeLocation`
pub const EE_UNITS_ACT_PATH_TO_GOAL: i32 = 231;
pub const EE_UNITS_ACT_MOVE_TO_TILE_CENTRE: i32 = 232;
pub const EE_UNITS_ACT_FLEE: i32 = 233;
pub const EE_UNITS_ACT_NEXT_WAYPOINT: i32 = 234;
pub const EE_UNITS_ACT_FIND_SAFE_LOCATION: i32 = 235;
/// `SetHasArrived(false)`; the location goal's `FaceYaw(inUnit)` (its result)
pub const EE_UNITS_ACT_CLEAR_ARRIVED: i32 = 236;
pub const EE_UNITS_ACT_FACE_YAW: i32 = 237;
/// the class's `FindPath`, `Heal`; `FindTargetNear(inUnit, line of sight, &target)` (its result; the host keeps the
/// target), a new heal goal on it `SetGoal(goal, a, 0)`, `SetHealSearchTimer(the class's mHealDelay)`
pub const EE_UNITS_ACT_FIND_PATH: i32 = 238;
pub const EE_UNITS_ACT_HEAL: i32 = 239;
pub const EE_UNITS_ACT_FIND_HEALEE: i32 = 240;
pub const EE_UNITS_ACT_SET_HEAL_GOAL: i32 = 241;
pub const EE_UNITS_ACT_RESET_HEAL_SEARCH: i32 = 242;
/// `mGoalState->SetStartTime(gPhysics->mGameTime)`
pub const EE_UNITS_ACT_GOAL_START_TIME: i32 = 243;
/// the land class's `CallParatroopPlane(inUnit)`, `EEPlaceTrackDecal(inUnit)`
pub const EE_UNITS_ACT_CALL_PARATROOP_PLANE: i32 = 244;
pub const EE_UNITS_ACT_PLACE_TRACK_DECAL: i32 = 245;
/// the class's death effect on the unit, the stats' death sound played where it is
pub const EE_UNITS_ACT_DEATH_EFFECT: i32 = 246;
pub const EE_UNITS_ACT_DEATH_SOUND: i32 = 247;
/// `mAngularVelocity = 0`, `SetVelocity(0.0f)`
pub const EE_UNITS_ACT_STOP: i32 = 248;
/// `ReleaseConversionEffect()`, the priest class's `HideConversionEffects(inUnit)`, `mConversionStartTime = a` (when
/// it differs), `SetWorkTimer(a)`
pub const EE_UNITS_ACT_RELEASE_CONVERSION_EFFECT: i32 = 249;
pub const EE_UNITS_ACT_HIDE_CONVERSION_EFFECTS: i32 = 250;
pub const EE_UNITS_ACT_SET_CONVERSION_START: i32 = 251;
pub const EE_UNITS_ACT_SET_WORK_TIMER: i32 = 252;
// the fourteenth round's
/// the citizen's: `FindStorage(inUnit, a != 0)` and a gather goal to the storage found; `SetResourceAmount(a)`,
/// `SetProductionTime(a)`, `mHasShiftQueuedBuild = false`
pub const EE_UNITS_ACT_FIND_STORAGE_GOAL: i32 = 253;
pub const EE_UNITS_ACT_SET_RESOURCE_AMOUNT: i32 = 254;
pub const EE_UNITS_ACT_SET_PRODUCTION_TIME: i32 = 255;
pub const EE_UNITS_ACT_CLEAR_SHIFT_QUEUED: i32 = 256;
/// the building's: the attack goal retargeted to the nearest building of its target, `SetExitingUnitID(a)`, the
/// exiting plane's takeoff request, `ReleaseUnit(inUnit)`
pub const EE_UNITS_ACT_RETARGET_NEAREST_BUILDING: i32 = 257;
pub const EE_UNITS_ACT_SET_EXITING_UNIT: i32 = 258;
pub const EE_UNITS_ACT_QUEUE_TAKEOFF: i32 = 259;
pub const EE_UNITS_ACT_RELEASE_UNIT: i32 = 260;
/// the gate's: the gate sound played, the next two parts' `StartAction(a)`
pub const EE_UNITS_ACT_GATE_SOUND: i32 = 261;
pub const EE_UNITS_ACT_GATE_PARTS_ACTION: i32 = 262;
/// `SetCurrentPhysicsOrder(a)`, `SetF220(0.0f)`, `SetHeight(GetHeightAt(inUnit, the unit's location))`, the next
/// waypoint's height (`GetNextWaypointPoint`, then `SetHeight(GetHeightAt)` when there is one), `inUnit->NextWaypoint()`
pub const EE_UNITS_ACT_SET_PHYSICS_ORDER: i32 = 263;
pub const EE_UNITS_ACT_CLEAR_F220: i32 = 264;
pub const EE_UNITS_ACT_HEIGHT_HERE: i32 = 265;
pub const EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT: i32 = 266;
pub const EE_UNITS_ACT_UNIT_NEXT_WAYPOINT: i32 = 267;
/// `ReturnToAirport(inUnit)`, `SetFlightTime(mStats->mFlightTime)`, `SetAmmunition(the class's mAmmunition)`
pub const EE_UNITS_ACT_RETURN_TO_AIRPORT: i32 = 268;
pub const EE_UNITS_ACT_SET_FLIGHT_TIME_STATS: i32 = 269;
pub const EE_UNITS_ACT_SET_AMMUNITION: i32 = 270;
/// a falling flyer's roll (a: 0 the air animal's model, 1 the airplane's): `SetF220(ComputeRoll(yaw rate))`, the roll
/// rate set to the yaw rate
pub const EE_UNITS_ACT_DEATH_ROLL: i32 = 271;
/// the helicopters' path to the goal (a: 0 helicopter, 1 transport, 2 priest), the air animal's, the landable's
/// (destination, landing point, `PathToward`)
pub const EE_UNITS_ACT_HELICOPTER_PATH: i32 = 272;
pub const EE_UNITS_ACT_AIR_ANIMAL_PATH: i32 = 273;
pub const EE_UNITS_ACT_LANDABLE_PATH: i32 = 274;
/// the airplane's circling side (the space map's reach tests and the draws), the holding pattern (a: 0 airplane,
/// 1 planet spaceship), `FlyOffMap(inUnit)`
pub const EE_UNITS_ACT_CIRCLE_SIDE: i32 = 275;
pub const EE_UNITS_ACT_HOLDING_PATTERN: i32 = 276;
pub const EE_UNITS_ACT_FLY_OFF_MAP: i32 = 277;
/// the landable's: `ApproachCarrier(inUnit, a != 0)`, boarding the home carrier, `TakeOffFromCarrier(inUnit)`, the
/// flight's path check (path start, continent, landing point), `FindNearestAirport` (its result; the host keeps it)
/// and `SetHomeBase` of it, `StartAction(a)`, the class's `ActionChanged` again, the landing approach (a: landing),
/// the secondary target, the takeoff, the landing request (`QueueRequest` of the home base: its result, else 9)
pub const EE_UNITS_ACT_APPROACH_CARRIER: i32 = 278;
pub const EE_UNITS_ACT_BOARD_CARRIER: i32 = 279;
pub const EE_UNITS_ACT_TAKE_OFF_FROM_CARRIER: i32 = 280;
pub const EE_UNITS_ACT_FLY_CHECK_PATH: i32 = 281;
pub const EE_UNITS_ACT_FIND_NEAREST_AIRPORT: i32 = 282;
pub const EE_UNITS_ACT_SET_HOME_BASE_FOUND: i32 = 283;
pub const EE_UNITS_ACT_START_ACTION: i32 = 284;
pub const EE_UNITS_ACT_CLASS_ACTION_CHANGED: i32 = 285;
pub const EE_UNITS_ACT_APPROACH_LANDING: i32 = 286;
pub const EE_UNITS_ACT_SECONDARY_TARGET: i32 = 287;
pub const EE_UNITS_ACT_TAKEOFF: i32 = 288;
pub const EE_UNITS_ACT_QUEUE_LANDING: i32 = 289;
/// a bomber's attack: `SetIsCommandable(a != 0)`, an attack ground goal at the drop location; `mIsCommandable = true`;
/// the initial attack state's cruise height; the contact location here with heading 0 (a: through the setter)
pub const EE_UNITS_ACT_SET_COMMANDABLE: i32 = 290;
pub const EE_UNITS_ACT_BOMBER_GROUND_GOAL: i32 = 291;
pub const EE_UNITS_ACT_MAKE_COMMANDABLE: i32 = 292;
pub const EE_UNITS_ACT_INITIAL_ATTACK_HEIGHT: i32 = 293;
pub const EE_UNITS_ACT_CONTACT_HERE: i32 = 294;

// the update host's codes this module reads
use crate::update::{
    EE_UNITS_UPD_R_COMMAND, EE_UNITS_UPD_R_CONVERSION_START, EE_UNITS_UPD_R_FLIGHT_TIME, EE_UNITS_UPD_R_GAME_TIME,
    EE_UNITS_UPD_R_GATHER_TIME_2, EE_UNITS_UPD_R_GOAL_KIND, EE_UNITS_UPD_R_HAS_NEXT_PART, EE_UNITS_UPD_R_HERD,
    EE_UNITS_UPD_R_UNIT_TYPE,
};

// the original's constants (ee/EEUnitAction.h, ee/EEUnitGoal.h, ee/EEUnitClassIDs.h)
const ACTION_CONVERT_ENEMY_UNIT: i64 = 2004;
const ACTION_FACE_ENEMY_UNIT: i64 = 2012;
const ACTION_CHECK_RANGE: i64 = 2013;
const ACTION_CAST_CALAMITY: i64 = 2017;
const ACTION_RUN_FROM_ATTACKER: i64 = 2019;
const ACTION_RETALIATE_AGAINST_ATTACKER: i64 = 2020;
const ACTION_WAIT_FOR_RELOAD: i64 = 2021;
const ACTION_ANIMAL_RETALIATION: i64 = 2022;
const ACTION_ANIMAL_PARENTAL_VENGANCE: i64 = 2023;
const ACTION_SHOULD_I_FOLLOW_ENEMY_UNIT: i64 = 2024;
const ACTION_RETURN_TO_INITIAL_CONTACT_LOCATION: i64 = 2026;
const ACTION_FACE_LOCATION: i64 = 2027;
const ACTION_PREPARE_TO_CAST_CALAMITY: i64 = 2028;
const ACTION_POST_CALAMITY_FLEE: i64 = 2029;
const ACTION_CALAMITY_PRE_CAST: i64 = 2030;
const ACTION_CHANGE_TARGET_FOR_SPECIAL_ENEMY: i64 = 2031;
const ACTION_SPECIAL_LOCATION: i64 = 2032;
const ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
const ACTION_ATTACK_LOCATION: i64 = 2002;
const ACTION_INSUFFICIENT_RESOURCES: i64 = 4030;
const ACTION_CHANGE_TARGET_FOR_SPECIAL_REPAIR: i64 = 4031;
const ACTION_IDLE: i64 = 5000;
const ACTION_REACQUIRE_GOAL: i64 = 5001;
const ACTION_REACQUIRE_ENEMY_UNIT: i64 = 5002;
const ACTION_DYING: i64 = 6000;
const ACTION_RESOURCES_ROTTING: i64 = 6001;
const ACTION_UNIT_ROTTING: i64 = 6002;
const ACTION_DEAD: i64 = 6003;
const ACTION_RESOURCES_DEPLETING: i64 = 6004;
const ACTION_KILL: i64 = 6005;
const ACTION_KILL_SILENTLY: i64 = 6008;
const ACTION_FADING_OUT_IN_SPACE: i64 = 6009;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
const ACTION_FLEE_DANGER: i64 = 8019;
const ACTION_GET_NEXT_PATROL_POINT: i64 = 8020;
const ACTION_GET_NEXT_AUTO_WAYPOINT: i64 = 8021;
const ACTION_MOVE_TO_VALID_MAP_LOCATION: i64 = 8022;
const ACTION_GET_NEXT_MOVE_WAYPOINT: i64 = 9000;
const ACTION_REPATH_AROUND_OBSTACLE: i64 = 9002;
const ACTION_TURN_TO_UNIT_FACING: i64 = 9006;
const ACTION_ADVANCE: i64 = 10003;
const ACTION_FIND_SAFE_LOCATION: i64 = 11001;
const ACTION_FIND_UNEXPLORED_AREA: i64 = 11002;
const ACTION_LOAD_INTO_CONTAINER: i64 = 12000;
const ACTION_UNLOAD_TRANSPORT: i64 = 12001;
const ACTION_MOVE_TO_RENDEZVOUS: i64 = 12002;
const ACTION_PICKED_UP_LOADABLE_UNITS: i64 = 12003;
const ACTION_GET_CONTAINER_PROGRESS: i64 = 12004;
const ACTION_GET_NEXT_TRANSPORT_WAYPOINT: i64 = 12005;
const ACTION_BUMP_EXTRA_LOADABLE_UNITS: i64 = 12006;
const ACTION_GARRISON_UPGRADE: i64 = 12007;
const ACTION_WAIT_FOR_GARRISON_COST: i64 = 12008;
const ACTION_WAIT_FOR_PASSENGERS: i64 = 12010;
const ACTION_WAIT_FOR_TRANSPORT: i64 = 12011;
const ACTION_CANNOT_REACH_CONTAINER: i64 = 12012;
const ACTION_LOOK_FOR_HEALEE: i64 = 16000;
const ACTION_HEAL_UNIT: i64 = 16001;
const ACTION_TURN_TO_FACE_HEALEE: i64 = 16002;
const ACTION_REPAIR_BUILDING: i64 = 4002;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const ACTION_ADD_RESOURCE_TO_STORAGE: i64 = 4005;
const ACTION_FIND_NEW_RESOURCE: i64 = 4006;
const ACTION_WAIT_FOR_BUILD_SITE_TO_CLEAR: i64 = 4012;
const ACTION_CONVERT_TO_LOCATION_GOAL: i64 = 4019;
const ACTION_MOVE_TO_WAITING_AREA: i64 = 4023;
const ACTION_NOTIFY_WAITING_CITIZEN: i64 = 4024;
const ACTION_CHECK_FOR_OPEN_POSITION: i64 = 4025;
const ACTION_WAIT_FOR_OPEN_POSITION: i64 = 4026;
const ACTION_FIND_FARM: i64 = 4029;
const ACTION_MOVE_TO_RESOURCE: i64 = 8008;
const ACTION_MOVE_TO_STORAGE: i64 = 8009;
const ACTION_MOVE_TO_REPAIR_SITE: i64 = 8017;
const ACTION_FIND_NEW_STORAGE: i64 = 4007;
const ACTION_FIND_STORAGE: i64 = 4021;
const ACTION_TASK_SMART_CITIZEN: i64 = 4028;
const ACTION_SEARCH_FOR_SECONDARY_TARGET: i64 = 1017;
const ACTION_TRACK_ENEMY_UNIT: i64 = 2011;
const ACTION_AIRPORT_OPEN: i64 = 13000;
const ACTION_WAIT_FOR_TAKEOFF_CLEARANCE: i64 = 13001;
const ACTION_LAUNCH_NEXT_AIRPLANE: i64 = 13002;
const ACTION_OPEN: i64 = 15000;
const ACTION_CLOSED: i64 = 15001;
const ACTION_OPENING: i64 = 15002;
const ACTION_CLOSING: i64 = 15003;
const ACTION_TAXI: i64 = 1002;
const ACTION_CLIMB: i64 = 1004;
const ACTION_FLY: i64 = 1006;
const ACTION_DESCEND: i64 = 1012;
const ACTION_BRAKE: i64 = 1014;
const ACTION_CIRCLE: i64 = 1008;
const ACTION_CONTINUE_TO_TAXI: i64 = 1003;
const ACTION_CONTINUE_TO_CLIMB: i64 = 1005;
const ACTION_CONTINUE_TO_FLY: i64 = 1007;
const ACTION_CONTINUE_TO_DESCEND: i64 = 1013;
const ACTION_CONTINUE_TO_BRAKE: i64 = 1015;
const ACTION_CONTINUE_TO_WAIT_FOR_LANDING_CLEARANCE: i64 = 1020;
const ACTION_LAND: i64 = 1011;
const ACTION_RECALCULATE_LANDING: i64 = 1019;
const ACTION_TAKEOFF: i64 = 1000;
const ACTION_HOLDING_PATTERN: i64 = 1010;
const ACTION_CIRCLE_AIRTRAFFIC_CONTROLLER: i64 = 1022;
const ACTION_SEND_PARATROOPER_PLANE_OFF_MAP: i64 = 1023;
const ACTION_BOARD_TRANSPORT: i64 = 1018;
const ACTION_EMERGENCY_TAKEOFF: i64 = 1016;
const ACTION_REQUEST_LANDING: i64 = 1021;
const ACTION_WAIT_FOR_LANDING_CLEARANCE: i64 = 1009;
const ACTION_INITIAL_ATTACK_STATE: i64 = 2016;
const GOAL_NONE: i64 = -1;
const GOAL_ATTACK_GROUND: i64 = 3;
const GOAL_LOCATION: i64 = 1;
const GOAL_LOAD_TRANSPORT: i64 = 7;
const GOAL_PATROL: i64 = 9;
const GOAL_EXPLORE: i64 = 10;
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
const MOVEMENT_AIR: i64 = 1;
/// `kMovementSea | kMovementSpace` (`kMovementLand` is 2)
const MOVEMENT_SEA_OR_SPACE: i64 = 4 | 8;
const UNIT_TYPE_RADIO_MAN: i64 = 0x2ef;
const UNIT_TYPE_SP15_SPACE_CARRIER: i64 = 0x2f3;

/// The host's calls (the update host's table: `EeUnitsUpdateHost`), as a trait (the FFI table; the tests' mock).
pub use crate::update::UpdateHost as ActionHost;

/// A unit's store fields at a point of the decision: the record (a copy) or the host's.
struct ActionReads<'a> {
    host: &'a mut dyn ActionHost,
    record: &'a mut dyn FnMut() -> Option<EeUnitRecord>,
}

impl ActionReads<'_> {
    fn field(&mut self, code: i32) -> i64 {
        match (self.record)() {
            Some(r) => match code {
                EE_UNITS_UPD_R_COMMAND => r.command,
                EE_UNITS_UPD_R_GOAL_KIND => i64::from(r.goal_kind),
                EE_UNITS_UPD_R_HERD => i64::from(r.herd),
                EE_UNITS_UPD_R_UNIT_TYPE => r.unit_type,
                EE_UNITS_ACT_R_DOMAINS => i64::from(r.domains),
                EE_UNITS_ACT_R_CATEGORY => r.category,
                EE_UNITS_ACT_R_CONTAINER => i64::from(r.container),
                EE_UNITS_ACT_R_RESOURCE_AMOUNT => r.resource_amount,
                _ => self.host.read(code),
            },
            None => self.host.read(code),
        }
    }
    fn on(&mut self, code: i32) -> bool {
        self.host.read(code) != 0
    }
    /// a read of the unit's stats: the stats table's record of its stats object (stats.rs), else the host's
    fn stat(&mut self, code: i32) -> i64 {
        match (self.record)().and_then(|r| crate::stats::stats_of(r.stats)) {
            Some(s) => crate::update::stat_field(&s, code).unwrap_or_else(|| self.host.read(code)),
            None => self.host.read(code),
        }
    }
    fn act(&mut self, code: i32) -> i64 {
        self.host.act(code, 0, 0)
    }
    fn act_with(&mut self, code: i32, a: i64) -> i64 {
        self.host.act(code, a, 0)
    }
    fn animation(&mut self, animation: i64, flag: i64) {
        self.host.act(EE_UNITS_ACT_SET_ANIMATION, animation, flag);
    }
    fn command(&mut self) -> i64 {
        self.field(EE_UNITS_UPD_R_COMMAND)
    }
    fn goal(&mut self) -> i64 {
        self.field(EE_UNITS_UPD_R_GOAL_KIND)
    }
    /// a goal of a kind the unit returns to after the new one (patrol, location, explore): queued behind it (1)
    fn queue_mode(&mut self) -> i64 {
        let goal = self.goal();
        if goal == GOAL_PATROL || goal == GOAL_LOCATION || goal == GOAL_EXPLORE {
            1
        } else {
            0
        }
    }
    /// a land citizen carrying resources of a class that carries none
    fn carrying(&mut self) -> bool {
        self.field(EE_UNITS_ACT_R_CATEGORY) == CATEGORY_LAND_CITIZEN
            && self.field(EE_UNITS_ACT_R_RESOURCE_AMOUNT) > 0
            && self.host.read(EE_UNITS_ACT_R_CLASS_RESOURCE_AMOUNT) == 0
    }
}

/// `EEUnitClass::ActionChanged`: the heal effect released, then the command's handling; the goal's start time
/// restarted unless the command is not handled (or the world is being cleared, or there is no goal).
fn base(u: &mut ActionReads<'_>) {
    u.act(EE_UNITS_ACT_RELEASE_HEAL_EFFECT);
    match u.command() {
        ACTION_ANIMAL_PARENTAL_VENGANCE => {
            u.act(EE_UNITS_ACT_HERD_ATTACK_BACK);
        }
        ACTION_ANIMAL_RETALIATION => {
            if u.field(EE_UNITS_UPD_R_HERD) != 0 {
                u.act(EE_UNITS_ACT_HERD_UNIT_HIT);
            }
        }
        ACTION_CHECK_RANGE => {
            if u.on(EE_UNITS_ACT_R_NO_HEADING) {
                u.act(EE_UNITS_ACT_CONTACT_CHECK_RANGE);
            }
        }
        ACTION_RETALIATE_AGAINST_ATTACKER => {
            u.act(EE_UNITS_ACT_ATTACKED);
        }
        ACTION_RUN_FROM_ATTACKER => {
            u.act(EE_UNITS_ACT_FLEE_ATTACKER);
        }
        ACTION_SHOULD_I_FOLLOW_ENEMY_UNIT => {
            if u.on(EE_UNITS_ACT_R_NO_HEADING) {
                u.act(EE_UNITS_ACT_CONTACT_FOLLOW);
            }
        }
        ACTION_WAIT_FOR_RELOAD => {
            if (u.field(EE_UNITS_ACT_R_DOMAINS) & MOVEMENT_AIR) == 0
                && u.field(EE_UNITS_ACT_R_CATEGORY) != CATEGORY_PLANET_SPACESHIP
                && u.host.read(EE_UNITS_ACT_R_ANIMATION) as u32 != u.host.read(EE_UNITS_ACT_R_ATTACK_FRAME_SET) as u32
            {
                u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3);
                if u.host.read(EE_UNITS_ACT_R_ANIMATION) != 0 {
                    u.animation(0, 1);
                }
            }
        }
        ACTION_RETURN_TO_INITIAL_CONTACT_LOCATION => {
            let mode = u.queue_mode();
            u.act_with(EE_UNITS_ACT_RETURN_TO_CONTACT, mode);
            u.act_with(EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER, 0);
        }
        ACTION_POST_CALAMITY_FLEE => {
            u.act(EE_UNITS_ACT_FLEE_AFTER_CALAMITY);
        }
        ACTION_IDLE => idle(u),
        ACTION_RESOURCES_ROTTING => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 6);
            if !u.on(EE_UNITS_ACT_R_IS_WALL) {
                u.animation(5, 0);
            }
            u.act_with(EE_UNITS_ACT_SET_ROTTING, 0);
            u.act_with(EE_UNITS_ACT_CLEAR_GOALS, 0);
            if u.on(EE_UNITS_UPD_R_HAS_NEXT_PART) {
                u.act_with(EE_UNITS_ACT_NEXT_PART_ACTION, ACTION_RESOURCES_ROTTING);
            }
        }
        ACTION_DYING => {
            u.act(EE_UNITS_ACT_UNIT_DIED);
        }
        ACTION_REACQUIRE_GOAL => {
            if u.goal() == GOAL_LOAD_TRANSPORT {
                u.act(EE_UNITS_ACT_GOAL_STEP);
            }
        }
        ACTION_UNIT_ROTTING => {
            u.act(EE_UNITS_ACT_DESELECT);
            let time = u.host.read(EE_UNITS_ACT_R_ROTTING_TIME);
            u.act_with(EE_UNITS_ACT_SET_ROTTING, time);
            if u.on(EE_UNITS_UPD_R_HAS_NEXT_PART) {
                u.act_with(EE_UNITS_ACT_NEXT_PART_ACTION, ACTION_UNIT_ROTTING);
            }
        }
        ACTION_FADING_OUT_IN_SPACE => {
            u.act(EE_UNITS_ACT_CLOAK_CHAIN);
            u.act(EE_UNITS_ACT_OPAQUE_CHAIN);
        }
        ACTION_KILL_SILENTLY => {
            u.act_with(EE_UNITS_ACT_KILL, 0);
        }
        ACTION_KILL => {
            u.act_with(EE_UNITS_ACT_KILL, 1);
        }
        ACTION_RESOURCES_DEPLETING => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3);
        }
        ACTION_DEAD => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 6);
            u.act(EE_UNITS_ACT_KILL_UNIT_AND_PARTS);
        }
        ACTION_PREPARE_TO_MOVE => {
            if u.on(EE_UNITS_ACT_R_PATH_ATTEMPTS_EXCEEDED) {
                u.act(EE_UNITS_ACT_START_IDLE);
            } else {
                u.act(EE_UNITS_ACT_PATH_TO_GOAL);
            }
        }
        ACTION_MOVE_TO_VALID_MAP_LOCATION => {
            u.act(EE_UNITS_ACT_MOVE_TO_TILE_CENTRE);
        }
        ACTION_GET_NEXT_AUTO_WAYPOINT => {
            u.act_with(EE_UNITS_ACT_CLEAR_GOALS, 0);
        }
        ACTION_GET_NEXT_PATROL_POINT => {
            u.act(EE_UNITS_ACT_CLEAR_HEADING);
            if u.goal() == GOAL_PATROL {
                u.act(EE_UNITS_ACT_GOAL_STEP);
            }
        }
        ACTION_FLEE_DANGER => {
            u.act(EE_UNITS_ACT_FLEE);
        }
        ACTION_GET_NEXT_MOVE_WAYPOINT => {
            u.act(EE_UNITS_ACT_NEXT_WAYPOINT);
        }
        ACTION_FIND_SAFE_LOCATION => {
            u.act(EE_UNITS_ACT_FIND_SAFE_LOCATION);
        }
        ACTION_ADVANCE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.act(EE_UNITS_ACT_CLEAR_ARRIVED);
            if u.carrying() || u.on(EE_UNITS_ACT_R_SWIMMING) {
                u.animation(2, 0);
            } else {
                u.animation(0xd, 0);
            }
        }
        ACTION_TURN_TO_UNIT_FACING => {
            if !(u.goal() == GOAL_LOCATION && u.act(EE_UNITS_ACT_FACE_YAW) != 0) {
                u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 5);
                // (the original's two tests in order: a loaded citizen not walking loaded, a swimmer)
                if (u.carrying() && u.host.read(EE_UNITS_ACT_R_ANIMATION) != 2) || u.on(EE_UNITS_ACT_R_SWIMMING) {
                    u.animation(2, 0);
                } else if u.host.read(EE_UNITS_ACT_R_ANIMATION) != 0xd {
                    u.animation(0xd, 0);
                }
            }
        }
        ACTION_REPATH_AROUND_OBSTACLE => {
            u.act(EE_UNITS_ACT_FIND_PATH);
        }
        ACTION_HEAL_UNIT => {
            u.act(EE_UNITS_ACT_HEAL);
        }
        ACTION_LOOK_FOR_HEALEE => {
            // the goal as it was before the search
            let mode = u.queue_mode();
            if u.act(EE_UNITS_ACT_FIND_HEALEE) != 0 {
                u.act_with(EE_UNITS_ACT_SET_HEAL_GOAL, mode);
            }
            u.act(EE_UNITS_ACT_RESET_HEAL_SEARCH);
        }
        ACTION_ATTACK_ENEMY_UNIT
        | ACTION_ATTACK_LOCATION
        | ACTION_FACE_ENEMY_UNIT
        | ACTION_CAST_CALAMITY
        | ACTION_FACE_LOCATION
        | ACTION_PREPARE_TO_CAST_CALAMITY
        | ACTION_CALAMITY_PRE_CAST
        | ACTION_CHANGE_TARGET_FOR_SPECIAL_ENEMY
        | ACTION_INSUFFICIENT_RESOURCES
        | ACTION_CHANGE_TARGET_FOR_SPECIAL_REPAIR
        | ACTION_REACQUIRE_ENEMY_UNIT
        | ACTION_FIND_UNEXPLORED_AREA
        | ACTION_LOAD_INTO_CONTAINER
        | ACTION_UNLOAD_TRANSPORT
        | ACTION_MOVE_TO_RENDEZVOUS
        | ACTION_PICKED_UP_LOADABLE_UNITS
        | ACTION_GET_CONTAINER_PROGRESS
        | ACTION_GET_NEXT_TRANSPORT_WAYPOINT
        | ACTION_BUMP_EXTRA_LOADABLE_UNITS
        | ACTION_GARRISON_UPGRADE
        | ACTION_WAIT_FOR_GARRISON_COST
        | ACTION_WAIT_FOR_PASSENGERS
        | ACTION_WAIT_FOR_TRANSPORT
        | ACTION_CANNOT_REACH_CONTAINER
        | ACTION_TURN_TO_FACE_HEALEE => {
            if u.goal() != GOAL_NONE {
                u.act(EE_UNITS_ACT_GOAL_STEP);
            }
        }
        ACTION_SPECIAL_LOCATION => {}
        _ => return,
    }
    if u.goal() != GOAL_NONE && !u.on(EE_UNITS_ACT_R_WORLD_CLEARING) {
        u.act(EE_UNITS_ACT_GOAL_START_TIME);
    }
}

/// The idle command: physics action 3, no next trigger; a swimmer swims, a unit that may fight (no land building or
/// an armed one) not already idling idles in the stance 7 or (by a draw, unless it fires over walls) 8 with a random
/// hold; the path dropped, a ship's or spaceship's water state, the player told, the goals cleared (not a citizen's
/// queue in a container), no heading, the paths cancelled without a goal.
fn idle(u: &mut ActionReads<'_>) {
    u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3);
    u.act_with(EE_UNITS_ACT_SET_NEXT_TRIGGER_TIME, -1);
    let state = u.host.read(EE_UNITS_ACT_R_ANIMATION) as u32;
    if u.on(EE_UNITS_ACT_R_SWIMMING) {
        u.animation(2, 0);
    } else if (!u.on(EE_UNITS_ACT_R_LAND_BUILDING) || u.on(EE_UNITS_ACT_R_CLASS_ARMED))
        && state != 7
        && ((u.stat(EE_UNITS_ACT_R_FIRE_OVER_WALLS) != 0) || state != 8)
    {
        let draw = f32::from_bits(u.host.read(EE_UNITS_ACT_R_RANDOM) as u32);
        if draw < 0.5 || (u.stat(EE_UNITS_ACT_R_FIRE_OVER_WALLS) != 0) {
            u.animation(7, 0);
        } else {
            u.animation(8, 0);
        }
        u.act(EE_UNITS_ACT_PAUSE_ANIMATION);
        u.act(EE_UNITS_ACT_RANDOM_ANIMATION_TIME);
    }
    u.act(EE_UNITS_ACT_DROP_PATH);
    if u.field(EE_UNITS_ACT_R_DOMAINS) & MOVEMENT_SEA_OR_SPACE != 0 {
        u.act(EE_UNITS_ACT_UPDATE_WATER_STATE);
    }
    u.act(EE_UNITS_ACT_UNIT_GOAL_DONE);
    let category = u.field(EE_UNITS_ACT_R_CATEGORY);
    if !((category == CATEGORY_LAND_CITIZEN || category == CATEGORY_NAVAL_CITIZEN)
        && u.field(EE_UNITS_ACT_R_CONTAINER) != 0
        && u.on(EE_UNITS_ACT_R_QUEUED_GOALS))
    {
        u.act_with(EE_UNITS_ACT_CLEAR_GOALS, 0);
    }
    u.act(EE_UNITS_ACT_CLEAR_HEADING);
    if u.goal() == GOAL_NONE {
        u.act(EE_UNITS_ACT_CANCEL_PATHS);
    }
}

/// `EEUCLand::ActionChanged`: a radio man's special location calls the paratroop plane, a rotting body with a blood
/// stain leaves its decal; then the common handling.
fn land(u: &mut ActionReads<'_>) {
    match u.command() {
        ACTION_SPECIAL_LOCATION => {
            if u.field(EE_UNITS_UPD_R_UNIT_TYPE) == UNIT_TYPE_RADIO_MAN {
                u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 1);
                u.animation(0xb, 0);
                u.act(EE_UNITS_ACT_CALL_PARATROOP_PLANE);
            }
        }
        ACTION_RESOURCES_ROTTING if u.on(EE_UNITS_ACT_R_BLOOD_STAIN) => {
            u.act(EE_UNITS_ACT_PLACE_TRACK_DECAL);
        }
        _ => {}
    }
    base(u);
}

/// A ship's death (`EEUCNaval`'s and `EEUCSubmergedNaval`'s): the death effect and sound, physics action 2, stopped,
/// the sinking animation, the path dropped. Any other command: the common handling.
fn ship(u: &mut ActionReads<'_>) {
    if u.command() != ACTION_DYING {
        base(u);
        return;
    }
    if u.on(EE_UNITS_ACT_R_DEATH_EFFECT) {
        u.act(EE_UNITS_ACT_DEATH_EFFECT);
    }
    if u.on(EE_UNITS_ACT_R_DEATH_SOUND) {
        u.act(EE_UNITS_ACT_DEATH_SOUND);
    }
    u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 2);
    u.act(EE_UNITS_ACT_STOP);
    u.animation(4, 0);
    u.act(EE_UNITS_ACT_DROP_PATH);
}

/// `EEUCSpaceship::ActionChanged`: idle as any unit, then the flying animation; dying: the sound, then the effect,
/// physics action 2, the flying animation, the path dropped; else the common handling.
fn spaceship(u: &mut ActionReads<'_>) {
    match u.command() {
        ACTION_IDLE => {
            base(u);
            u.animation(0xd, 0);
        }
        ACTION_DYING => {
            if u.on(EE_UNITS_ACT_R_DEATH_SOUND) {
                u.act(EE_UNITS_ACT_DEATH_SOUND);
            }
            if u.on(EE_UNITS_ACT_R_DEATH_EFFECT) {
                u.act(EE_UNITS_ACT_DEATH_EFFECT);
            }
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 2);
            u.animation(0xd, 0);
            u.act(EE_UNITS_ACT_DROP_PATH);
        }
        _ => base(u),
    }
}

/// `EEUCBasicPriest::ActionChanged`: the conversion's and heal's effects released and hidden; converting: physics
/// action 1, the search timer, the conversion animation, the start time and work timer when not started, the path
/// dropped; waiting to reload: physics action 3; else the common handling.
fn basic_priest(u: &mut ActionReads<'_>) {
    u.act(EE_UNITS_ACT_RELEASE_CONVERSION_EFFECT);
    u.act(EE_UNITS_ACT_RELEASE_HEAL_EFFECT);
    u.act(EE_UNITS_ACT_HIDE_CONVERSION_EFFECTS);
    match u.command() {
        ACTION_CONVERT_ENEMY_UNIT => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 1);
            u.act_with(EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER, 0);
            u.animation(9, 0);
            if u.host.read(EE_UNITS_UPD_R_CONVERSION_START) == 0 {
                let now = u.host.read(EE_UNITS_UPD_R_GAME_TIME);
                if now as u32 != 0 {
                    u.act_with(EE_UNITS_ACT_SET_CONVERSION_START, now);
                }
                u.act_with(EE_UNITS_ACT_SET_WORK_TIMER, 0);
            }
            u.act(EE_UNITS_ACT_DROP_PATH);
        }
        ACTION_WAIT_FOR_RELOAD => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3);
        }
        _ => base(u),
    }
}

/// the helpers of the fourteenth round's overrides
impl ActionReads<'_> {
    /// `SetCurrentPhysicsAction(a)`, `SetCurrentPhysicsOrder(5)`, `SetHasArrived(false)`, `SetF220(0.0f)`
    fn moving(&mut self, action: i64) {
        self.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, action);
        self.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
        self.arrived_cleared();
    }
    fn arrived_cleared(&mut self) {
        self.act(EE_UNITS_ACT_CLEAR_ARRIVED);
        self.act(EE_UNITS_ACT_CLEAR_F220);
    }
    /// `SetCurrentPhysicsOrder(5)`, `inUnit->NextWaypoint()`
    fn continue_flying(&mut self) {
        self.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
        self.act(EE_UNITS_ACT_UNIT_NEXT_WAYPOINT);
    }
    /// a random side to circle (`gPhysics->GetRandom() < 0.5f`: order 0, else 1)
    fn random_side(&mut self) {
        let draw = f32::from_bits(self.host.read(EE_UNITS_ACT_R_RANDOM) as u32);
        self.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, if draw < 0.5 { 0 } else { 1 });
    }
    /// a goal other than exploring dropped (`ClearGoals(0)`)
    fn drop_unless_exploring(&mut self) {
        let goal = self.goal();
        if goal != GOAL_NONE && goal != GOAL_EXPLORE {
            self.act_with(EE_UNITS_ACT_CLEAR_GOALS, 0);
        }
    }
    fn death_sound(&mut self) {
        if self.on(EE_UNITS_ACT_R_DEATH_SOUND) {
            self.act(EE_UNITS_ACT_DEATH_SOUND);
        }
    }
    /// a carrier's plane (air traffic types 2 and 4)
    fn carrier_traffic(&mut self) -> bool {
        let kind = self.host.read(EE_UNITS_ACT_R_AIR_TRAFFIC_TYPE);
        kind == 2 || kind == 4
    }
}

/// `EEUCBasicCitizen::ActionChanged`: the work actions step the goal and end; the storage searches step it or, with
/// no goal and a load, look for a storage; the smart task steps a human's goal; rotting (with the blood stain's decal)
/// and dying empty the load, idling ends the production and the shift-queued build; then the common handling.
fn citizen(u: &mut ActionReads<'_>) {
    let command = u.command();
    match command {
        ACTION_REPAIR_BUILDING
        | ACTION_GATHER_RESOURCE
        | ACTION_ADD_RESOURCE_TO_STORAGE
        | ACTION_FIND_NEW_RESOURCE
        | ACTION_WAIT_FOR_BUILD_SITE_TO_CLEAR
        | ACTION_CONVERT_TO_LOCATION_GOAL
        | ACTION_MOVE_TO_WAITING_AREA
        | ACTION_NOTIFY_WAITING_CITIZEN
        | ACTION_CHECK_FOR_OPEN_POSITION
        | ACTION_WAIT_FOR_OPEN_POSITION
        | ACTION_FIND_FARM
        | ACTION_MOVE_TO_RESOURCE
        | ACTION_MOVE_TO_STORAGE
        | ACTION_MOVE_TO_REPAIR_SITE => {
            if u.goal() != GOAL_NONE {
                u.act(EE_UNITS_ACT_GOAL_STEP);
            }
            return;
        }
        ACTION_FIND_NEW_STORAGE | ACTION_FIND_STORAGE => {
            if u.goal() != GOAL_NONE {
                u.act(EE_UNITS_ACT_GOAL_STEP);
            } else if u.field(EE_UNITS_ACT_R_RESOURCE_AMOUNT) > 0 {
                u.act_with(EE_UNITS_ACT_FIND_STORAGE_GOAL, i64::from(command == ACTION_FIND_NEW_STORAGE));
            }
            return;
        }
        ACTION_TASK_SMART_CITIZEN => {
            if u.goal() != GOAL_NONE && u.on(EE_UNITS_ACT_R_IS_HUMAN) {
                u.act(EE_UNITS_ACT_GOAL_STEP);
            }
            return;
        }
        ACTION_RESOURCES_ROTTING => {
            if u.on(EE_UNITS_ACT_R_BLOOD_STAIN) {
                u.act(EE_UNITS_ACT_PLACE_TRACK_DECAL);
            }
            u.act_with(EE_UNITS_ACT_SET_RESOURCE_AMOUNT, 0);
        }
        ACTION_DYING => {
            u.act_with(EE_UNITS_ACT_SET_RESOURCE_AMOUNT, 0);
        }
        ACTION_IDLE => {
            u.act_with(EE_UNITS_ACT_SET_PRODUCTION_TIME, 0);
            if u.on(EE_UNITS_ACT_R_BUILD_QUEUE_EMPTY) {
                u.act(EE_UNITS_ACT_CLEAR_SHIFT_QUEUED);
            }
        }
        _ => {}
    }
    base(u);
}

/// `EEUCBuilding::ActionChanged`: the secondary target's retarget, no tracking, dying with the next part, rotting
/// empty, moving only for carriers, the airport's exits; then the common handling.
fn building(u: &mut ActionReads<'_>) {
    match u.command() {
        ACTION_SEARCH_FOR_SECONDARY_TARGET => {
            u.act(EE_UNITS_ACT_RETARGET_NEAREST_BUILDING);
            return;
        }
        ACTION_TRACK_ENEMY_UNIT => return,
        ACTION_DYING => {
            u.act_with(EE_UNITS_ACT_SET_RESOURCE_AMOUNT, 0);
            if u.on(EE_UNITS_UPD_R_HAS_NEXT_PART) {
                u.act_with(EE_UNITS_ACT_NEXT_PART_ACTION, ACTION_DYING);
            }
        }
        ACTION_RESOURCES_ROTTING => {
            u.act_with(EE_UNITS_ACT_SET_RESOURCE_AMOUNT, 0);
        }
        ACTION_PREPARE_TO_MOVE => {
            if !u.on(EE_UNITS_ACT_R_AIRCRAFT_CARRIER)
                && u.field(EE_UNITS_UPD_R_UNIT_TYPE) != UNIT_TYPE_SP15_SPACE_CARRIER
            {
                return;
            }
        }
        ACTION_AIRPORT_OPEN => {
            u.act_with(EE_UNITS_ACT_SET_EXITING_UNIT, -1);
            return;
        }
        ACTION_WAIT_FOR_TAKEOFF_CLEARANCE => {
            if u.host.read(EE_UNITS_ACT_R_EXITING_UNIT) != -1 {
                u.act(EE_UNITS_ACT_QUEUE_TAKEOFF);
            }
            return;
        }
        ACTION_LAUNCH_NEXT_AIRPLANE => {
            u.act(EE_UNITS_ACT_RELEASE_UNIT);
            return;
        }
        _ => {}
    }
    base(u);
}

/// `EEUCBuildingPriest::ActionChanged`: the building's actions as a building, the rest as a priest (its
/// `PriestVirtual74`, the basic priest's body).
fn building_priest(u: &mut ActionReads<'_>) {
    match u.command() {
        ACTION_SEARCH_FOR_SECONDARY_TARGET
        | ACTION_TRACK_ENEMY_UNIT
        | ACTION_DYING
        | ACTION_RESOURCES_ROTTING
        | ACTION_PREPARE_TO_MOVE
        | ACTION_AIRPORT_OPEN
        | ACTION_WAIT_FOR_TAKEOFF_CLEARANCE
        | ACTION_LAUNCH_NEXT_AIRPLANE => building(u),
        _ => basic_priest(u),
    }
}

/// `EEUCGate::ActionChanged`: the next two pieces open, close, open or close moving (with the gate's sound); else as
/// a building.
fn gate(u: &mut ActionReads<'_>) {
    let action = match u.command() {
        ACTION_OPEN => ACTION_OPEN,
        ACTION_CLOSED => ACTION_CLOSED,
        ACTION_OPENING => {
            u.act(EE_UNITS_ACT_GATE_SOUND);
            ACTION_OPENING
        }
        ACTION_CLOSING => {
            u.act(EE_UNITS_ACT_GATE_SOUND);
            ACTION_CLOSING
        }
        _ => {
            building(u);
            return;
        }
    };
    u.act_with(EE_UNITS_ACT_GATE_PARTS_ACTION, action);
}

/// `EEUCMiningBuilding::ActionChanged`: gathering starts the work timer at the stats' gather time 2; else as a
/// building.
fn mining_building(u: &mut ActionReads<'_>) {
    if u.command() != ACTION_GATHER_RESOURCE {
        building(u);
    } else {
        let time = u.stat(EE_UNITS_UPD_R_GATHER_TIME_2);
        u.act_with(EE_UNITS_ACT_SET_WORK_TIMER, time);
    }
}

/// The helicopters' (`variant`: 0 `EEUCHelicopter`, 1 `EEUCHelicopterTransport`, 2 `EEUCHelicopterPriest`): idling
/// as any unit, dying with the sound and falling, preparing to move a path to the goal, each then flying; the rest
/// the common (the priest's: the priest's) handling.
fn helicopter(u: &mut ActionReads<'_>, variant: i64) {
    match u.command() {
        ACTION_IDLE => base(u),
        ACTION_DYING => {
            u.death_sound();
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 2);
            u.animation(0xd, 0);
            u.act(EE_UNITS_ACT_DROP_PATH);
            return;
        }
        ACTION_PREPARE_TO_MOVE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 6);
            u.act(EE_UNITS_ACT_DROP_PATH);
            u.act_with(EE_UNITS_ACT_HELICOPTER_PATH, variant);
        }
        _ => {
            if variant == 2 {
                basic_priest(u);
            } else {
                base(u);
            }
            return;
        }
    }
    u.animation(0xd, 0);
}

/// `EEUCAirAnimal::ActionChanged`: flying, circling (a goal other than exploring dropped, a random side, the height
/// here), the waypoints, landing, taking off, dying (falling with the roll at the yaw rate), a path to the goal; the
/// rest the common handling. After landing, taking off or a path: flying, the next waypoint's height.
fn air_animal(u: &mut ActionReads<'_>) {
    match u.command() {
        ACTION_TAXI | ACTION_CLIMB | ACTION_FLY | ACTION_DESCEND | ACTION_BRAKE => {
            u.moving(0);
            return;
        }
        ACTION_CIRCLE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.arrived_cleared();
            u.drop_unless_exploring();
            u.random_side();
            u.act(EE_UNITS_ACT_HEIGHT_HERE);
            return;
        }
        ACTION_CONTINUE_TO_TAXI
        | ACTION_CONTINUE_TO_CLIMB
        | ACTION_CONTINUE_TO_FLY
        | ACTION_CONTINUE_TO_DESCEND
        | ACTION_CONTINUE_TO_BRAKE
        | ACTION_CONTINUE_TO_WAIT_FOR_LANDING_CLEARANCE => {
            u.continue_flying();
            return;
        }
        ACTION_LAND | ACTION_RECALCULATE_LANDING => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
        }
        ACTION_TAKEOFF => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
            u.act(EE_UNITS_ACT_RETURN_TO_AIRPORT);
            u.act(EE_UNITS_ACT_SET_FLIGHT_TIME_STATS);
            u.act(EE_UNITS_ACT_SET_AMMUNITION);
        }
        ACTION_DYING => {
            u.death_sound();
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 2);
            u.animation(4, 0);
            u.act(EE_UNITS_ACT_DROP_PATH);
            u.act_with(EE_UNITS_ACT_DEATH_ROLL, 0);
            return;
        }
        ACTION_PREPARE_TO_MOVE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
            u.act(EE_UNITS_ACT_DROP_PATH);
            u.act(EE_UNITS_ACT_AIR_ANIMAL_PATH);
        }
        _ => {
            base(u);
            return;
        }
    }
    u.animation(0xd, 0);
    u.act(EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT);
}

/// `EEUCLandable::ActionChanged`: a plane's taxiing, takeoffs and landings (runway or carrier), landing requests,
/// emergencies, secondary targets, bombing runs, its initial contact and its death; the rest the common handling.
fn landable(u: &mut ActionReads<'_>) {
    let command = u.command();
    match command {
        ACTION_BRAKE => {
            if u.carrier_traffic() {
                u.act_with(EE_UNITS_ACT_APPROACH_CARRIER, 0);
            }
            u.moving(0);
        }
        ACTION_BOARD_TRANSPORT => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 6);
            if u.on(EE_UNITS_ACT_R_HOME_BASE) {
                u.act(EE_UNITS_ACT_BOARD_CARRIER);
            }
        }
        ACTION_CONTINUE_TO_TAXI if u.carrier_traffic() && u.on(EE_UNITS_ACT_R_CLEARANCE_GRANTED) => {
            u.act(EE_UNITS_ACT_TAKE_OFF_FROM_CARRIER);
            u.moving(0);
        }
        ACTION_CONTINUE_TO_TAXI
        | ACTION_CONTINUE_TO_CLIMB
        | ACTION_CONTINUE_TO_DESCEND
        | ACTION_CONTINUE_TO_BRAKE
        | ACTION_CONTINUE_TO_WAIT_FOR_LANDING_CLEARANCE => u.continue_flying(),
        ACTION_TAXI | ACTION_CLIMB | ACTION_DESCEND => u.moving(0),
        ACTION_FLY | ACTION_CONTINUE_TO_FLY => {
            if command == ACTION_FLY {
                u.moving(0);
            } else {
                u.continue_flying();
            }
            u.act(EE_UNITS_ACT_FLY_CHECK_PATH);
        }
        ACTION_EMERGENCY_TAKEOFF => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            let found = u.act(EE_UNITS_ACT_FIND_NEAREST_AIRPORT) != 0;
            if u.host.read(EE_UNITS_ACT_R_WAYPOINTS) <= 3 || !found {
                u.act_with(EE_UNITS_ACT_KILL, 1);
                return;
            }
            u.act(EE_UNITS_ACT_SET_HOME_BASE_FOUND);
            u.act_with(EE_UNITS_ACT_START_ACTION, ACTION_REQUEST_LANDING);
            u.act(EE_UNITS_ACT_CLASS_ACTION_CHANGED);
        }
        ACTION_LAND | ACTION_RECALCULATE_LANDING => {
            u.act_with(EE_UNITS_ACT_APPROACH_LANDING, i64::from(command == ACTION_LAND));
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
            u.animation(0xd, 0);
            u.act(EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT);
        }
        ACTION_SEARCH_FOR_SECONDARY_TARGET => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 6);
            u.act(EE_UNITS_ACT_SECONDARY_TARGET);
        }
        ACTION_TAKEOFF => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
            u.act(EE_UNITS_ACT_TAKEOFF);
        }
        ACTION_REQUEST_LANDING => {
            let result = if u.on(EE_UNITS_ACT_R_HOME_BASE) { u.act(EE_UNITS_ACT_QUEUE_LANDING) } else { 9 };
            if result != 0 {
                if u.act(EE_UNITS_ACT_FIND_NEAREST_AIRPORT) == 0 {
                    u.act_with(EE_UNITS_ACT_KILL, 1);
                } else {
                    u.act(EE_UNITS_ACT_SET_HOME_BASE_FOUND);
                    u.act_with(EE_UNITS_ACT_START_ACTION, ACTION_REQUEST_LANDING);
                    u.act(EE_UNITS_ACT_CLASS_ACTION_CHANGED);
                }
            }
            u.act(EE_UNITS_ACT_CLEAR_HEADING);
        }
        ACTION_WAIT_FOR_LANDING_CLEARANCE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.arrived_cleared();
        }
        ACTION_ATTACK_ENEMY_UNIT | ACTION_FACE_ENEMY_UNIT => {
            // (the attack falls through to facing)
            if command == ACTION_ATTACK_ENEMY_UNIT && u.on(EE_UNITS_ACT_R_BOMBER) {
                u.act_with(EE_UNITS_ACT_SET_COMMANDABLE, 0);
                let goal = u.goal();
                if goal != GOAL_NONE && goal != GOAL_ATTACK_GROUND {
                    u.act(EE_UNITS_ACT_BOMBER_GROUND_GOAL);
                }
            }
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
            base(u);
        }
        ACTION_RUN_FROM_ATTACKER => {
            if u.on(EE_UNITS_ACT_R_BOMBER)
                && !u.on(EE_UNITS_ACT_R_COMMANDABLE)
                && u.host.read(EE_UNITS_UPD_R_FLIGHT_TIME) as u32 != 0
            {
                u.act(EE_UNITS_ACT_MAKE_COMMANDABLE);
            }
            base(u);
        }
        ACTION_INITIAL_ATTACK_STATE => {
            u.act(EE_UNITS_ACT_INITIAL_ATTACK_HEIGHT);
            base(u);
        }
        ACTION_CHECK_RANGE | ACTION_SHOULD_I_FOLLOW_ENEMY_UNIT => {
            if u.on(EE_UNITS_ACT_R_NO_HEADING) {
                u.act_with(EE_UNITS_ACT_CONTACT_HERE, i64::from(command == ACTION_SHOULD_I_FOLLOW_ENEMY_UNIT));
            }
        }
        ACTION_PREPARE_TO_MOVE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 5);
            u.act(EE_UNITS_ACT_DROP_PATH);
            if u.on(EE_UNITS_ACT_R_NO_HEADING) {
                u.act_with(EE_UNITS_ACT_CONTACT_HERE, 1);
            }
            u.act(EE_UNITS_ACT_LANDABLE_PATH);
            u.animation(0xd, 0);
            u.act(EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT);
        }
        ACTION_DYING => {
            u.death_sound();
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 2);
            u.animation(4, 0);
            u.act(EE_UNITS_ACT_DROP_PATH);
            u.act_with(EE_UNITS_ACT_DEATH_ROLL, 1);
        }
        _ => base(u),
    }
}

/// `EEUCAirplane::ActionChanged`: circling (its side, the height here, no heading, a goal other than exploring
/// dropped), the holding pattern over its home airport, circling the controller, the paratroopers' plane off the
/// map; the rest the landable's.
fn airplane(u: &mut ActionReads<'_>) {
    match u.command() {
        ACTION_CIRCLE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.arrived_cleared();
            if u.host.read(EE_UNITS_ACT_R_WAYPOINTS) > 0 {
                u.act(EE_UNITS_ACT_DROP_PATH);
            }
            u.act(EE_UNITS_ACT_CIRCLE_SIDE);
            u.act(EE_UNITS_ACT_HEIGHT_HERE);
            u.act(EE_UNITS_ACT_CLEAR_HEADING);
            u.drop_unless_exploring();
        }
        ACTION_HOLDING_PATTERN => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.act_with(EE_UNITS_ACT_HOLDING_PATTERN, 0);
        }
        ACTION_CIRCLE_AIRTRAFFIC_CONTROLLER => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ORDER, 0);
            u.arrived_cleared();
        }
        ACTION_SEND_PARATROOPER_PLANE_OFF_MAP => {
            u.act(EE_UNITS_ACT_FLY_OFF_MAP);
        }
        _ => landable(u),
    }
}

/// `EEUCPlanetSpaceship::ActionChanged`: circling (a random side), the holding pattern, circling the controller and
/// dying (the sound, falling, the effect) of a planet spaceship; the rest the landable's.
fn planet_spaceship(u: &mut ActionReads<'_>) {
    match u.command() {
        ACTION_CIRCLE => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0);
            u.arrived_cleared();
            if u.host.read(EE_UNITS_ACT_R_WAYPOINTS) > 0 {
                u.act(EE_UNITS_ACT_DROP_PATH);
            }
            u.random_side();
            u.act(EE_UNITS_ACT_HEIGHT_HERE);
            u.act(EE_UNITS_ACT_CLEAR_HEADING);
            u.drop_unless_exploring();
        }
        ACTION_HOLDING_PATTERN => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3);
            u.act_with(EE_UNITS_ACT_HOLDING_PATTERN, 1);
        }
        ACTION_CIRCLE_AIRTRAFFIC_CONTROLLER => {
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3);
            u.arrived_cleared();
        }
        ACTION_DYING => {
            u.death_sound();
            u.act_with(EE_UNITS_ACT_SET_PHYSICS_ACTION, 2);
            u.animation(4, 0);
            u.act(EE_UNITS_ACT_DROP_PATH);
            if u.on(EE_UNITS_ACT_R_DEATH_EFFECT) {
                u.act(EE_UNITS_ACT_DEATH_EFFECT);
            }
        }
        _ => landable(u),
    }
}

/// A class's `ActionChanged` (`kind`: `EE_UNITS_ACTION_*`) over `record` (the unit's record now, or none). False for
/// a kind it does not know (nothing called).
pub fn action_changed(kind: i32, host: &mut impl ActionHost, record: &mut dyn FnMut() -> Option<EeUnitRecord>) -> bool {
    let mut reads = ActionReads { host, record };
    let u = &mut reads;
    match kind {
        EE_UNITS_ACTION_BASE => base(u),
        EE_UNITS_ACTION_LAND => land(u),
        EE_UNITS_ACTION_NAVAL | EE_UNITS_ACTION_SUBMERGED_NAVAL => ship(u),
        EE_UNITS_ACTION_SPACESHIP => spaceship(u),
        EE_UNITS_ACTION_BASIC_PRIEST => basic_priest(u),
        EE_UNITS_ACTION_CITIZEN => citizen(u),
        EE_UNITS_ACTION_BUILDING => building(u),
        EE_UNITS_ACTION_BUILDING_PRIEST => building_priest(u),
        EE_UNITS_ACTION_GATE => gate(u),
        EE_UNITS_ACTION_MINING_BUILDING => mining_building(u),
        EE_UNITS_ACTION_HELICOPTER => helicopter(u, 0),
        EE_UNITS_ACTION_HELICOPTER_TRANSPORT => helicopter(u, 1),
        EE_UNITS_ACTION_HELICOPTER_PRIEST => helicopter(u, 2),
        EE_UNITS_ACTION_AIR_ANIMAL => air_animal(u),
        EE_UNITS_ACTION_LANDABLE => landable(u),
        EE_UNITS_ACTION_AIRPLANE => airplane(u),
        EE_UNITS_ACTION_PLANET_SPACESHIP => planet_spaceship(u),
        _ => return false,
    }
    true
}

/// A class's `ActionChanged` of the unit `id` (`object`: its address, the record's check) over the store and `host`.
///
/// # Safety
/// `host` points to a valid table for the call.
#[no_mangle]
pub unsafe extern "C" fn ee_units_action_changed(
    kind: i32,
    id: u32,
    object: u64,
    host: *const crate::update::EeUnitsUpdateHost,
) -> bool {
    // SAFETY: the caller's contract
    let mut h = unsafe { &*host };
    let mut record = || with_store(|s| crate::step::record_of(s, id, object).copied());
    action_changed(kind, &mut h, &mut record)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    /// A host that logs its operations and answers reads from a map (0 for others); `answers` per operation.
    #[derive(Default)]
    struct Mock {
        reads: BTreeMap<i32, i64>,
        answers: BTreeMap<i32, i64>,
        log: Vec<(i32, i64, i64)>,
        /// reads made, in order (the random draw's place)
        asked: Vec<i32>,
    }

    impl ActionHost for Mock {
        fn read(&mut self, code: i32) -> i64 {
            self.asked.push(code);
            *self.reads.get(&code).unwrap_or(&0)
        }
        fn act(&mut self, code: i32, a: i64, b: i64) -> i64 {
            self.log.push((code, a, b));
            *self.answers.get(&code).unwrap_or(&0)
        }
    }

    fn unit(command: i64) -> Mock {
        let mut m = Mock::default();
        m.reads.insert(EE_UNITS_UPD_R_COMMAND, command);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_NONE);
        m
    }

    fn run(kind: i32, m: &mut Mock) -> Vec<(i32, i64, i64)> {
        assert!(action_changed(kind, m, &mut || None));
        m.log.clone()
    }

    fn ops(log: &[(i32, i64, i64)]) -> Vec<i32> {
        log.iter().map(|l| l.0).collect()
    }

    #[test]
    fn unhandled_commands_skip_the_start_time() {
        let mut m = unit(12345);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_LOCATION);
        assert_eq!(ops(&run(EE_UNITS_ACTION_BASE, &mut m)), vec![EE_UNITS_ACT_RELEASE_HEAL_EFFECT]);
        // a handled one with a goal restarts it, not while the world is cleared
        let mut m = unit(ACTION_SPECIAL_LOCATION);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_LOCATION);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BASE, &mut m)),
            vec![EE_UNITS_ACT_RELEASE_HEAL_EFFECT, EE_UNITS_ACT_GOAL_START_TIME]
        );
        m.log.clear();
        m.reads.insert(EE_UNITS_ACT_R_WORLD_CLEARING, 1);
        assert_eq!(ops(&run(EE_UNITS_ACTION_BASE, &mut m)), vec![EE_UNITS_ACT_RELEASE_HEAL_EFFECT]);
        assert!(!action_changed(99, &mut unit(0), &mut || None));
    }

    #[test]
    fn idle() {
        // a soldier not idling: the draw picks stance 8, then the hold's draw
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_RANDOM, i64::from(0.75f32.to_bits()));
        m.reads.insert(EE_UNITS_ACT_R_DOMAINS, 4);
        let log = run(EE_UNITS_ACTION_BASE, &mut m);
        assert_eq!(
            log,
            vec![
                (EE_UNITS_ACT_RELEASE_HEAL_EFFECT, 0, 0),
                (EE_UNITS_ACT_SET_PHYSICS_ACTION, 3, 0),
                (EE_UNITS_ACT_SET_NEXT_TRIGGER_TIME, -1, 0),
                (EE_UNITS_ACT_SET_ANIMATION, 8, 0),
                (EE_UNITS_ACT_PAUSE_ANIMATION, 0, 0),
                (EE_UNITS_ACT_RANDOM_ANIMATION_TIME, 0, 0),
                (EE_UNITS_ACT_DROP_PATH, 0, 0),
                (EE_UNITS_ACT_UPDATE_WATER_STATE, 0, 0),
                (EE_UNITS_ACT_UNIT_GOAL_DONE, 0, 0),
                (EE_UNITS_ACT_CLEAR_GOALS, 0, 0),
                (EE_UNITS_ACT_CLEAR_HEADING, 0, 0),
                (EE_UNITS_ACT_CANCEL_PATHS, 0, 0),
            ]
        );
        // the draw is made once, after the stance tests
        assert_eq!(m.asked.iter().filter(|&&c| c == EE_UNITS_ACT_R_RANDOM).count(), 1);
        // a land unit (kMovementLand 2) has no water state
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_DOMAINS, 2);
        assert!(!ops(&run(EE_UNITS_ACTION_BASE, &mut m)).contains(&EE_UNITS_ACT_UPDATE_WATER_STATE));
        // below 0.5 or firing over walls: stance 7
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_RANDOM, i64::from(0.25f32.to_bits()));
        assert!(run(EE_UNITS_ACTION_BASE, &mut m).contains(&(EE_UNITS_ACT_SET_ANIMATION, 7, 0)));
        // already in stance 7, or 8 without firing over walls, or an unarmed land building: no stance, no draw
        for (code, value) in
            [(EE_UNITS_ACT_R_ANIMATION, 7), (EE_UNITS_ACT_R_ANIMATION, 8), (EE_UNITS_ACT_R_LAND_BUILDING, 1)]
        {
            let mut m = unit(ACTION_IDLE);
            m.reads.insert(code, value);
            let log = run(EE_UNITS_ACTION_BASE, &mut m);
            assert!(!ops(&log).contains(&EE_UNITS_ACT_SET_ANIMATION));
            assert!(!m.asked.contains(&EE_UNITS_ACT_R_RANDOM));
        }
        // a swimmer swims; a citizen with queued goals in a container keeps them; a goal: no paths cancelled
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_SWIMMING, 1);
        m.reads.insert(EE_UNITS_ACT_R_CATEGORY, CATEGORY_NAVAL_CITIZEN);
        m.reads.insert(EE_UNITS_ACT_R_CONTAINER, 1);
        m.reads.insert(EE_UNITS_ACT_R_QUEUED_GOALS, 1);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_PATROL);
        let log = run(EE_UNITS_ACTION_BASE, &mut m);
        assert!(log.contains(&(EE_UNITS_ACT_SET_ANIMATION, 2, 0)));
        assert!(!ops(&log).contains(&EE_UNITS_ACT_CLEAR_GOALS));
        assert!(!ops(&log).contains(&EE_UNITS_ACT_CANCEL_PATHS));
        assert_eq!(*ops(&log).last().unwrap(), EE_UNITS_ACT_GOAL_START_TIME);
    }

    #[test]
    fn rotting_dying_and_killing() {
        let mut m = unit(ACTION_RESOURCES_ROTTING);
        m.reads.insert(EE_UNITS_UPD_R_HAS_NEXT_PART, 1);
        assert_eq!(
            run(EE_UNITS_ACTION_BASE, &mut m)[1..].to_vec(),
            vec![
                (EE_UNITS_ACT_SET_PHYSICS_ACTION, 6, 0),
                (EE_UNITS_ACT_SET_ANIMATION, 5, 0),
                (EE_UNITS_ACT_SET_ROTTING, 0, 0),
                (EE_UNITS_ACT_CLEAR_GOALS, 0, 0),
                (EE_UNITS_ACT_NEXT_PART_ACTION, ACTION_RESOURCES_ROTTING, 0),
            ]
        );
        let mut m = unit(ACTION_RESOURCES_ROTTING);
        m.reads.insert(EE_UNITS_ACT_R_IS_WALL, 1);
        assert!(!ops(&run(EE_UNITS_ACTION_BASE, &mut m)).contains(&EE_UNITS_ACT_SET_ANIMATION));
        let mut m = unit(ACTION_UNIT_ROTTING);
        m.reads.insert(EE_UNITS_ACT_R_ROTTING_TIME, 30_000);
        assert_eq!(
            run(EE_UNITS_ACTION_BASE, &mut m)[1..].to_vec(),
            vec![(EE_UNITS_ACT_DESELECT, 0, 0), (EE_UNITS_ACT_SET_ROTTING, 30_000, 0)]
        );
        assert_eq!(run(EE_UNITS_ACTION_BASE, &mut unit(ACTION_KILL))[1], (EE_UNITS_ACT_KILL, 1, 0));
        assert_eq!(run(EE_UNITS_ACTION_BASE, &mut unit(ACTION_KILL_SILENTLY))[1], (EE_UNITS_ACT_KILL, 0, 0));
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BASE, &mut unit(ACTION_DEAD))),
            vec![EE_UNITS_ACT_RELEASE_HEAL_EFFECT, EE_UNITS_ACT_SET_PHYSICS_ACTION, EE_UNITS_ACT_KILL_UNIT_AND_PARTS]
        );
        let mut m = unit(ACTION_PREPARE_TO_MOVE);
        assert_eq!(ops(&run(EE_UNITS_ACTION_BASE, &mut m))[1], EE_UNITS_ACT_PATH_TO_GOAL);
        let mut m = unit(ACTION_PREPARE_TO_MOVE);
        m.reads.insert(EE_UNITS_ACT_R_PATH_ATTEMPTS_EXCEEDED, 1);
        assert_eq!(ops(&run(EE_UNITS_ACTION_BASE, &mut m))[1], EE_UNITS_ACT_START_IDLE);
    }

    #[test]
    fn moving_and_turning() {
        // advancing: a citizen carrying resources of an empty class walks loaded
        let mut m = unit(ACTION_ADVANCE);
        m.reads.insert(EE_UNITS_ACT_R_CATEGORY, CATEGORY_LAND_CITIZEN);
        m.reads.insert(EE_UNITS_ACT_R_RESOURCE_AMOUNT, 5);
        assert_eq!(
            run(EE_UNITS_ACTION_BASE, &mut m)[1..].to_vec(),
            vec![
                (EE_UNITS_ACT_SET_PHYSICS_ACTION, 0, 0),
                (EE_UNITS_ACT_CLEAR_ARRIVED, 0, 0),
                (EE_UNITS_ACT_SET_ANIMATION, 2, 0)
            ]
        );
        m.log.clear();
        m.reads.insert(EE_UNITS_ACT_R_CLASS_RESOURCE_AMOUNT, 10);
        assert!(run(EE_UNITS_ACTION_BASE, &mut m).contains(&(EE_UNITS_ACT_SET_ANIMATION, 0xd, 0)));
        // turning: the location goal's yaw ends it; a carrier already walking loaded falls to the next tests
        let mut m = unit(ACTION_TURN_TO_UNIT_FACING);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_LOCATION);
        m.answers.insert(EE_UNITS_ACT_FACE_YAW, 1);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BASE, &mut m)),
            vec![EE_UNITS_ACT_RELEASE_HEAL_EFFECT, EE_UNITS_ACT_FACE_YAW, EE_UNITS_ACT_GOAL_START_TIME]
        );
        let mut m = unit(ACTION_TURN_TO_UNIT_FACING);
        m.reads.insert(EE_UNITS_ACT_R_CATEGORY, CATEGORY_LAND_CITIZEN);
        m.reads.insert(EE_UNITS_ACT_R_RESOURCE_AMOUNT, 5);
        m.reads.insert(EE_UNITS_ACT_R_ANIMATION, 2);
        let log = run(EE_UNITS_ACTION_BASE, &mut m);
        assert_eq!(log[1], (EE_UNITS_ACT_SET_PHYSICS_ACTION, 5, 0));
        assert!(log.contains(&(EE_UNITS_ACT_SET_ANIMATION, 0xd, 0)));
        let mut m = unit(ACTION_TURN_TO_UNIT_FACING);
        m.reads.insert(EE_UNITS_ACT_R_ANIMATION, 0xd);
        assert!(!ops(&run(EE_UNITS_ACTION_BASE, &mut m)).contains(&EE_UNITS_ACT_SET_ANIMATION));
        // waiting to reload on the ground, not in the attack set: stop and stand
        let mut m = unit(ACTION_WAIT_FOR_RELOAD);
        m.reads.insert(EE_UNITS_ACT_R_ANIMATION, 3);
        m.reads.insert(EE_UNITS_ACT_R_ATTACK_FRAME_SET, 6);
        assert_eq!(
            run(EE_UNITS_ACTION_BASE, &mut m)[1..].to_vec(),
            vec![(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3, 0), (EE_UNITS_ACT_SET_ANIMATION, 0, 1)]
        );
        let mut m = unit(ACTION_WAIT_FOR_RELOAD);
        m.reads.insert(EE_UNITS_ACT_R_DOMAINS, MOVEMENT_AIR);
        assert_eq!(run(EE_UNITS_ACTION_BASE, &mut m).len(), 1);
    }

    #[test]
    fn goals_contacts_and_healing() {
        // returning to the contact location behind a patrol
        let mut m = unit(ACTION_RETURN_TO_INITIAL_CONTACT_LOCATION);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_PATROL);
        assert_eq!(
            run(EE_UNITS_ACTION_BASE, &mut m)[1..].to_vec(),
            vec![
                (EE_UNITS_ACT_RETURN_TO_CONTACT, 1, 0),
                (EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER, 0, 0),
                (EE_UNITS_ACT_GOAL_START_TIME, 0, 0)
            ]
        );
        // a healer finding a healee: the heal goal in front of an attack goal
        let mut m = unit(ACTION_LOOK_FOR_HEALEE);
        m.answers.insert(EE_UNITS_ACT_FIND_HEALEE, 1);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BASE, &mut m))[1..].to_vec(),
            vec![EE_UNITS_ACT_FIND_HEALEE, EE_UNITS_ACT_SET_HEAL_GOAL, EE_UNITS_ACT_RESET_HEAL_SEARCH]
        );
        let mut m = unit(ACTION_LOOK_FOR_HEALEE);
        assert!(!ops(&run(EE_UNITS_ACTION_BASE, &mut m)).contains(&EE_UNITS_ACT_SET_HEAL_GOAL));
        // the contact location only without a heading
        let mut m = unit(ACTION_CHECK_RANGE);
        assert_eq!(run(EE_UNITS_ACTION_BASE, &mut m).len(), 1);
        m.reads.insert(EE_UNITS_ACT_R_NO_HEADING, 1);
        assert_eq!(*ops(&run(EE_UNITS_ACTION_BASE, &mut m)).last().unwrap(), EE_UNITS_ACT_CONTACT_CHECK_RANGE);
        // the goal steps: only with a goal
        let mut m = unit(ACTION_UNLOAD_TRANSPORT);
        assert_eq!(run(EE_UNITS_ACTION_BASE, &mut m).len(), 1);
        m.log.clear();
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, 7);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BASE, &mut m))[1..].to_vec(),
            vec![EE_UNITS_ACT_GOAL_STEP, EE_UNITS_ACT_GOAL_START_TIME]
        );
        // the herd's retaliation needs a herd
        let mut m = unit(ACTION_ANIMAL_RETALIATION);
        assert_eq!(run(EE_UNITS_ACTION_BASE, &mut m).len(), 1);
        m.log.clear();
        m.reads.insert(EE_UNITS_UPD_R_HERD, 1);
        assert_eq!(ops(&run(EE_UNITS_ACTION_BASE, &mut m))[1], EE_UNITS_ACT_HERD_UNIT_HIT);
    }

    #[test]
    fn the_overrides() {
        // a radio man calls the plane, then the common handling
        let mut m = unit(ACTION_SPECIAL_LOCATION);
        m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_RADIO_MAN);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_LAND, &mut m)),
            vec![
                EE_UNITS_ACT_SET_PHYSICS_ACTION,
                EE_UNITS_ACT_SET_ANIMATION,
                EE_UNITS_ACT_CALL_PARATROOP_PLANE,
                EE_UNITS_ACT_RELEASE_HEAL_EFFECT
            ]
        );
        let mut m = unit(ACTION_RESOURCES_ROTTING);
        m.reads.insert(EE_UNITS_ACT_R_BLOOD_STAIN, 1);
        assert_eq!(ops(&run(EE_UNITS_ACTION_LAND, &mut m))[0], EE_UNITS_ACT_PLACE_TRACK_DECAL);
        // a dying ship sinks; a dying spaceship plays its sound before its effect
        let mut m = unit(ACTION_DYING);
        m.reads.insert(EE_UNITS_ACT_R_DEATH_EFFECT, 1);
        m.reads.insert(EE_UNITS_ACT_R_DEATH_SOUND, 1);
        assert_eq!(
            run(EE_UNITS_ACTION_NAVAL, &mut m),
            vec![
                (EE_UNITS_ACT_DEATH_EFFECT, 0, 0),
                (EE_UNITS_ACT_DEATH_SOUND, 0, 0),
                (EE_UNITS_ACT_SET_PHYSICS_ACTION, 2, 0),
                (EE_UNITS_ACT_STOP, 0, 0),
                (EE_UNITS_ACT_SET_ANIMATION, 4, 0),
                (EE_UNITS_ACT_DROP_PATH, 0, 0)
            ]
        );
        m.log.clear();
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_SPACESHIP, &mut m))[..2].to_vec(),
            vec![EE_UNITS_ACT_DEATH_SOUND, EE_UNITS_ACT_DEATH_EFFECT]
        );
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_SUBMERGED_NAVAL, &mut unit(ACTION_KILL)))[0],
            EE_UNITS_ACT_RELEASE_HEAL_EFFECT
        );
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_ANIMATION, 7);
        assert_eq!(*run(EE_UNITS_ACTION_SPACESHIP, &mut m).last().unwrap(), (EE_UNITS_ACT_SET_ANIMATION, 0xd, 0));
        // a priest converting starts its conversion once
        let mut m = unit(ACTION_CONVERT_ENEMY_UNIT);
        m.reads.insert(EE_UNITS_UPD_R_GAME_TIME, 5000);
        assert_eq!(
            run(EE_UNITS_ACTION_BASIC_PRIEST, &mut m)[3..].to_vec(),
            vec![
                (EE_UNITS_ACT_SET_PHYSICS_ACTION, 1, 0),
                (EE_UNITS_ACT_SET_TARGET_SEARCH_TIMER, 0, 0),
                (EE_UNITS_ACT_SET_ANIMATION, 9, 0),
                (EE_UNITS_ACT_SET_CONVERSION_START, 5000, 0),
                (EE_UNITS_ACT_SET_WORK_TIMER, 0, 0),
                (EE_UNITS_ACT_DROP_PATH, 0, 0)
            ]
        );
        let mut m = unit(ACTION_CONVERT_ENEMY_UNIT);
        m.reads.insert(EE_UNITS_UPD_R_CONVERSION_START, 4000);
        assert!(!ops(&run(EE_UNITS_ACTION_BASIC_PRIEST, &mut m)).contains(&EE_UNITS_ACT_SET_WORK_TIMER));
        let mut m = unit(ACTION_WAIT_FOR_RELOAD);
        assert_eq!(run(EE_UNITS_ACTION_BASIC_PRIEST, &mut m).len(), 4);
    }

    #[test]
    fn the_stats_reads_come_from_the_table() {
        let object = 0x5157_0000_0001u64;
        let mut stats = crate::stats::EeUnitStatsRecord { object, can_fire_over_walls: 1, ..Default::default() };
        stats.gather_times[2] = 1234;
        // SAFETY: a valid record
        unsafe { crate::stats::ee_units_stats_report(&stats) };
        let record =
            EeUnitRecord { command: ACTION_GATHER_RESOURCE, goal_kind: -1, stats: object, ..Default::default() };
        // the mine's work timer: the table's gather time, not the host's (0)
        let mut m = unit(ACTION_GATHER_RESOURCE);
        assert!(action_changed(EE_UNITS_ACTION_MINING_BUILDING, &mut m, &mut || Some(record)));
        assert_eq!(m.log, vec![(EE_UNITS_ACT_SET_WORK_TIMER, 1234, 0)]);
        assert!(!m.asked.contains(&EE_UNITS_UPD_R_GATHER_TIME_2));
        // idling with fire over walls (the table's): stance 7 whatever the draw
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_RANDOM, i64::from(0.75f32.to_bits()));
        let idle = EeUnitRecord { command: ACTION_IDLE, ..record };
        assert!(action_changed(EE_UNITS_ACTION_BASE, &mut m, &mut || Some(idle)));
        assert!(m.log.contains(&(EE_UNITS_ACT_SET_ANIMATION, 7, 0)));
        crate::stats::ee_units_stats_remove(object);
    }

    #[test]
    fn the_citizen() {
        // a work action steps the goal and ends (no common handling)
        let mut m = unit(ACTION_GATHER_RESOURCE);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, 6);
        assert_eq!(ops(&run(EE_UNITS_ACTION_CITIZEN, &mut m)), vec![EE_UNITS_ACT_GOAL_STEP]);
        assert!(run(EE_UNITS_ACTION_CITIZEN, &mut unit(ACTION_MOVE_TO_REPAIR_SITE)).is_empty());
        // the storage search with no goal and a load: the new storage flag
        let mut m = unit(ACTION_FIND_NEW_STORAGE);
        m.reads.insert(EE_UNITS_ACT_R_RESOURCE_AMOUNT, 3);
        assert_eq!(run(EE_UNITS_ACTION_CITIZEN, &mut m), vec![(EE_UNITS_ACT_FIND_STORAGE_GOAL, 1, 0)]);
        let mut m = unit(ACTION_FIND_STORAGE);
        m.reads.insert(EE_UNITS_ACT_R_RESOURCE_AMOUNT, 3);
        assert_eq!(run(EE_UNITS_ACTION_CITIZEN, &mut m), vec![(EE_UNITS_ACT_FIND_STORAGE_GOAL, 0, 0)]);
        assert!(run(EE_UNITS_ACTION_CITIZEN, &mut unit(ACTION_FIND_STORAGE)).is_empty());
        // the smart task: a human's goal only
        let mut m = unit(ACTION_TASK_SMART_CITIZEN);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, 4);
        assert!(run(EE_UNITS_ACTION_CITIZEN, &mut m).is_empty());
        m.reads.insert(EE_UNITS_ACT_R_IS_HUMAN, 1);
        assert_eq!(ops(&run(EE_UNITS_ACTION_CITIZEN, &mut m)), vec![EE_UNITS_ACT_GOAL_STEP]);
        // rotting: the decal, the load emptied, then the common handling
        let mut m = unit(ACTION_RESOURCES_ROTTING);
        m.reads.insert(EE_UNITS_ACT_R_BLOOD_STAIN, 1);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_CITIZEN, &mut m))[..3].to_vec(),
            vec![EE_UNITS_ACT_PLACE_TRACK_DECAL, EE_UNITS_ACT_SET_RESOURCE_AMOUNT, EE_UNITS_ACT_RELEASE_HEAL_EFFECT]
        );
        // idling with an empty build queue
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_BUILD_QUEUE_EMPTY, 1);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_CITIZEN, &mut m))[..3].to_vec(),
            vec![EE_UNITS_ACT_SET_PRODUCTION_TIME, EE_UNITS_ACT_CLEAR_SHIFT_QUEUED, EE_UNITS_ACT_RELEASE_HEAL_EFFECT]
        );
    }

    #[test]
    fn the_buildings() {
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BUILDING, &mut unit(ACTION_SEARCH_FOR_SECONDARY_TARGET))),
            vec![EE_UNITS_ACT_RETARGET_NEAREST_BUILDING]
        );
        assert!(run(EE_UNITS_ACTION_BUILDING, &mut unit(ACTION_TRACK_ENEMY_UNIT)).is_empty());
        // dying: the load, the next part, then the common handling
        let mut m = unit(ACTION_DYING);
        m.reads.insert(EE_UNITS_UPD_R_HAS_NEXT_PART, 1);
        assert_eq!(
            run(EE_UNITS_ACTION_BUILDING, &mut m)[..3].to_vec(),
            vec![
                (EE_UNITS_ACT_SET_RESOURCE_AMOUNT, 0, 0),
                (EE_UNITS_ACT_NEXT_PART_ACTION, ACTION_DYING, 0),
                (EE_UNITS_ACT_RELEASE_HEAL_EFFECT, 0, 0)
            ]
        );
        // moving: carriers only
        assert!(run(EE_UNITS_ACTION_BUILDING, &mut unit(ACTION_PREPARE_TO_MOVE)).is_empty());
        let mut m = unit(ACTION_PREPARE_TO_MOVE);
        m.reads.insert(EE_UNITS_UPD_R_UNIT_TYPE, UNIT_TYPE_SP15_SPACE_CARRIER);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BUILDING, &mut m))[..2].to_vec(),
            vec![EE_UNITS_ACT_RELEASE_HEAL_EFFECT, EE_UNITS_ACT_PATH_TO_GOAL]
        );
        // the airport
        assert_eq!(
            run(EE_UNITS_ACTION_BUILDING, &mut unit(ACTION_AIRPORT_OPEN)),
            vec![(EE_UNITS_ACT_SET_EXITING_UNIT, -1, 0)]
        );
        let mut m = unit(ACTION_WAIT_FOR_TAKEOFF_CLEARANCE);
        m.reads.insert(EE_UNITS_ACT_R_EXITING_UNIT, -1);
        assert!(run(EE_UNITS_ACTION_BUILDING, &mut m).is_empty());
        m.reads.insert(EE_UNITS_ACT_R_EXITING_UNIT, 7);
        assert_eq!(ops(&run(EE_UNITS_ACTION_BUILDING, &mut m)), vec![EE_UNITS_ACT_QUEUE_TAKEOFF]);
        // the temple: a building's action as a building, the rest as a priest
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BUILDING_PRIEST, &mut unit(ACTION_LAUNCH_NEXT_AIRPLANE))),
            vec![EE_UNITS_ACT_RELEASE_UNIT]
        );
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_BUILDING_PRIEST, &mut unit(ACTION_WAIT_FOR_RELOAD)))[0],
            EE_UNITS_ACT_RELEASE_CONVERSION_EFFECT
        );
        // the gate: moving pieces with the sound; other actions as a building
        assert_eq!(
            run(EE_UNITS_ACTION_GATE, &mut unit(ACTION_OPENING)),
            vec![(EE_UNITS_ACT_GATE_SOUND, 0, 0), (EE_UNITS_ACT_GATE_PARTS_ACTION, ACTION_OPENING, 0)]
        );
        assert_eq!(
            run(EE_UNITS_ACTION_GATE, &mut unit(ACTION_CLOSED)),
            vec![(EE_UNITS_ACT_GATE_PARTS_ACTION, ACTION_CLOSED, 0)]
        );
        assert!(run(EE_UNITS_ACTION_GATE, &mut unit(ACTION_TRACK_ENEMY_UNIT)).is_empty());
        // the mine: gathering's work timer
        let mut m = unit(ACTION_GATHER_RESOURCE);
        m.reads.insert(EE_UNITS_UPD_R_GATHER_TIME_2, 2500);
        assert_eq!(run(EE_UNITS_ACTION_MINING_BUILDING, &mut m), vec![(EE_UNITS_ACT_SET_WORK_TIMER, 2500, 0)]);
    }

    #[test]
    fn the_helicopters() {
        let mut m = unit(ACTION_DYING);
        m.reads.insert(EE_UNITS_ACT_R_DEATH_SOUND, 1);
        assert_eq!(
            run(EE_UNITS_ACTION_HELICOPTER, &mut m),
            vec![
                (EE_UNITS_ACT_DEATH_SOUND, 0, 0),
                (EE_UNITS_ACT_SET_PHYSICS_ACTION, 2, 0),
                (EE_UNITS_ACT_SET_ANIMATION, 0xd, 0),
                (EE_UNITS_ACT_DROP_PATH, 0, 0)
            ]
        );
        for (kind, variant) in [
            (EE_UNITS_ACTION_HELICOPTER, 0),
            (EE_UNITS_ACTION_HELICOPTER_TRANSPORT, 1),
            (EE_UNITS_ACTION_HELICOPTER_PRIEST, 2),
        ] {
            assert_eq!(
                run(kind, &mut unit(ACTION_PREPARE_TO_MOVE)),
                vec![
                    (EE_UNITS_ACT_SET_PHYSICS_ACTION, 6, 0),
                    (EE_UNITS_ACT_DROP_PATH, 0, 0),
                    (EE_UNITS_ACT_HELICOPTER_PATH, variant, 0),
                    (EE_UNITS_ACT_SET_ANIMATION, 0xd, 0)
                ]
            );
        }
        // idling: the common handling, then flying; others: the common (the priest's) handling only
        let mut m = unit(ACTION_IDLE);
        m.reads.insert(EE_UNITS_ACT_R_ANIMATION, 7);
        assert_eq!(
            *run(EE_UNITS_ACTION_HELICOPTER_PRIEST, &mut m).last().unwrap(),
            (EE_UNITS_ACT_SET_ANIMATION, 0xd, 0)
        );
        assert_eq!(ops(&run(EE_UNITS_ACTION_HELICOPTER, &mut unit(ACTION_KILL)))[0], EE_UNITS_ACT_RELEASE_HEAL_EFFECT);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_HELICOPTER_PRIEST, &mut unit(ACTION_KILL)))[0],
            EE_UNITS_ACT_RELEASE_CONVERSION_EFFECT
        );
    }

    #[test]
    fn the_air_animal() {
        // circling: the goal dropped unless exploring, the side by the draw, the height
        let mut m = unit(ACTION_CIRCLE);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_PATROL);
        m.reads.insert(EE_UNITS_ACT_R_RANDOM, i64::from(0.75f32.to_bits()));
        assert_eq!(
            run(EE_UNITS_ACTION_AIR_ANIMAL, &mut m),
            vec![
                (EE_UNITS_ACT_SET_PHYSICS_ACTION, 0, 0),
                (EE_UNITS_ACT_CLEAR_ARRIVED, 0, 0),
                (EE_UNITS_ACT_CLEAR_F220, 0, 0),
                (EE_UNITS_ACT_CLEAR_GOALS, 0, 0),
                (EE_UNITS_ACT_SET_PHYSICS_ORDER, 1, 0),
                (EE_UNITS_ACT_HEIGHT_HERE, 0, 0)
            ]
        );
        let mut m = unit(ACTION_CIRCLE);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, GOAL_EXPLORE);
        assert!(!ops(&run(EE_UNITS_ACTION_AIR_ANIMAL, &mut m)).contains(&EE_UNITS_ACT_CLEAR_GOALS));
        // taking off: then flying and the waypoint's height
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_AIR_ANIMAL, &mut unit(ACTION_TAKEOFF))),
            vec![
                EE_UNITS_ACT_SET_PHYSICS_ORDER,
                EE_UNITS_ACT_RETURN_TO_AIRPORT,
                EE_UNITS_ACT_SET_FLIGHT_TIME_STATS,
                EE_UNITS_ACT_SET_AMMUNITION,
                EE_UNITS_ACT_SET_ANIMATION,
                EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT
            ]
        );
        assert_eq!(
            *run(EE_UNITS_ACTION_AIR_ANIMAL, &mut unit(ACTION_DYING)).last().unwrap(),
            (EE_UNITS_ACT_DEATH_ROLL, 0, 0)
        );
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_AIR_ANIMAL, &mut unit(ACTION_CONTINUE_TO_BRAKE))),
            vec![EE_UNITS_ACT_SET_PHYSICS_ORDER, EE_UNITS_ACT_UNIT_NEXT_WAYPOINT]
        );
        assert_eq!(ops(&run(EE_UNITS_ACTION_AIR_ANIMAL, &mut unit(ACTION_KILL)))[0], EE_UNITS_ACT_RELEASE_HEAL_EFFECT);
    }

    #[test]
    fn the_landable() {
        // braking on a carrier approaches it first
        let mut m = unit(ACTION_BRAKE);
        m.reads.insert(EE_UNITS_ACT_R_AIR_TRAFFIC_TYPE, 4);
        assert_eq!(run(EE_UNITS_ACTION_LANDABLE, &mut m)[0], (EE_UNITS_ACT_APPROACH_CARRIER, 0, 0));
        // taxiing: a cleared carrier plane takes off, else the next waypoint
        let mut m = unit(ACTION_CONTINUE_TO_TAXI);
        m.reads.insert(EE_UNITS_ACT_R_AIR_TRAFFIC_TYPE, 2);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_LANDABLE, &mut m)),
            vec![EE_UNITS_ACT_SET_PHYSICS_ORDER, EE_UNITS_ACT_UNIT_NEXT_WAYPOINT]
        );
        m.log.clear();
        m.reads.insert(EE_UNITS_ACT_R_CLEARANCE_GRANTED, 1);
        assert_eq!(ops(&run(EE_UNITS_ACTION_LANDABLE, &mut m))[0], EE_UNITS_ACT_TAKE_OFF_FROM_CARRIER);
        // an emergency: killed with few waypoints or no airport, else a landing request handled again
        let mut m = unit(ACTION_EMERGENCY_TAKEOFF);
        m.answers.insert(EE_UNITS_ACT_FIND_NEAREST_AIRPORT, 1);
        m.reads.insert(EE_UNITS_ACT_R_WAYPOINTS, 3);
        assert_eq!(*run(EE_UNITS_ACTION_LANDABLE, &mut m).last().unwrap(), (EE_UNITS_ACT_KILL, 1, 0));
        m.log.clear();
        m.reads.insert(EE_UNITS_ACT_R_WAYPOINTS, 4);
        assert_eq!(
            run(EE_UNITS_ACTION_LANDABLE, &mut m)[2..].to_vec(),
            vec![
                (EE_UNITS_ACT_SET_HOME_BASE_FOUND, 0, 0),
                (EE_UNITS_ACT_START_ACTION, ACTION_REQUEST_LANDING, 0),
                (EE_UNITS_ACT_CLASS_ACTION_CHANGED, 0, 0)
            ]
        );
        // a landing request: no home base is a refusal (9); then the nearest airport or death; no heading after
        let mut m = unit(ACTION_REQUEST_LANDING);
        assert_eq!(
            run(EE_UNITS_ACTION_LANDABLE, &mut m),
            vec![
                (EE_UNITS_ACT_FIND_NEAREST_AIRPORT, 0, 0),
                (EE_UNITS_ACT_KILL, 1, 0),
                (EE_UNITS_ACT_CLEAR_HEADING, 0, 0)
            ]
        );
        let mut m = unit(ACTION_REQUEST_LANDING);
        m.reads.insert(EE_UNITS_ACT_R_HOME_BASE, 1);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_LANDABLE, &mut m)),
            vec![EE_UNITS_ACT_QUEUE_LANDING, EE_UNITS_ACT_CLEAR_HEADING]
        );
        // a bomber's attack: not commandable, a ground goal, then facing's order and the common handling
        let mut m = unit(ACTION_ATTACK_ENEMY_UNIT);
        m.reads.insert(EE_UNITS_ACT_R_BOMBER, 1);
        m.reads.insert(EE_UNITS_UPD_R_GOAL_KIND, 2);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_LANDABLE, &mut m))[..4].to_vec(),
            vec![
                EE_UNITS_ACT_SET_COMMANDABLE,
                EE_UNITS_ACT_BOMBER_GROUND_GOAL,
                EE_UNITS_ACT_SET_PHYSICS_ORDER,
                EE_UNITS_ACT_RELEASE_HEAL_EFFECT
            ]
        );
        let mut m = unit(ACTION_FACE_ENEMY_UNIT);
        m.reads.insert(EE_UNITS_ACT_R_BOMBER, 1);
        assert_eq!(ops(&run(EE_UNITS_ACTION_LANDABLE, &mut m))[0], EE_UNITS_ACT_SET_PHYSICS_ORDER);
        // running: a flying bomber becomes commandable
        let mut m = unit(ACTION_RUN_FROM_ATTACKER);
        m.reads.insert(EE_UNITS_ACT_R_BOMBER, 1);
        m.reads.insert(EE_UNITS_UPD_R_FLIGHT_TIME, 100);
        assert_eq!(ops(&run(EE_UNITS_ACTION_LANDABLE, &mut m))[0], EE_UNITS_ACT_MAKE_COMMANDABLE);
        // the contact: only without a heading, no common handling
        assert!(run(EE_UNITS_ACTION_LANDABLE, &mut unit(ACTION_CHECK_RANGE)).is_empty());
        let mut m = unit(ACTION_SHOULD_I_FOLLOW_ENEMY_UNIT);
        m.reads.insert(EE_UNITS_ACT_R_NO_HEADING, 1);
        assert_eq!(run(EE_UNITS_ACTION_LANDABLE, &mut m), vec![(EE_UNITS_ACT_CONTACT_HERE, 1, 0)]);
        // moving: the contact, the path, flying, the height
        let mut m = unit(ACTION_PREPARE_TO_MOVE);
        m.reads.insert(EE_UNITS_ACT_R_NO_HEADING, 1);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_LANDABLE, &mut m)),
            vec![
                EE_UNITS_ACT_SET_PHYSICS_ACTION,
                EE_UNITS_ACT_SET_PHYSICS_ORDER,
                EE_UNITS_ACT_DROP_PATH,
                EE_UNITS_ACT_CONTACT_HERE,
                EE_UNITS_ACT_LANDABLE_PATH,
                EE_UNITS_ACT_SET_ANIMATION,
                EE_UNITS_ACT_HEIGHT_AT_NEXT_WAYPOINT
            ]
        );
        assert_eq!(run(EE_UNITS_ACTION_LANDABLE, &mut unit(ACTION_LAND))[0], (EE_UNITS_ACT_APPROACH_LANDING, 1, 0));
        assert_eq!(
            *run(EE_UNITS_ACTION_LANDABLE, &mut unit(ACTION_DYING)).last().unwrap(),
            (EE_UNITS_ACT_DEATH_ROLL, 1, 0)
        );
        assert_eq!(ops(&run(EE_UNITS_ACTION_LANDABLE, &mut unit(ACTION_IDLE)))[0], EE_UNITS_ACT_RELEASE_HEAL_EFFECT);
    }

    #[test]
    fn the_planes() {
        // the airplane circles to its side; the planet spaceship by a draw
        let mut m = unit(ACTION_CIRCLE);
        m.reads.insert(EE_UNITS_ACT_R_WAYPOINTS, 2);
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_AIRPLANE, &mut m)),
            vec![
                EE_UNITS_ACT_SET_PHYSICS_ACTION,
                EE_UNITS_ACT_CLEAR_ARRIVED,
                EE_UNITS_ACT_CLEAR_F220,
                EE_UNITS_ACT_DROP_PATH,
                EE_UNITS_ACT_CIRCLE_SIDE,
                EE_UNITS_ACT_HEIGHT_HERE,
                EE_UNITS_ACT_CLEAR_HEADING
            ]
        );
        let mut m = unit(ACTION_CIRCLE);
        m.reads.insert(EE_UNITS_ACT_R_RANDOM, i64::from(0.25f32.to_bits()));
        assert!(run(EE_UNITS_ACTION_PLANET_SPACESHIP, &mut m).contains(&(EE_UNITS_ACT_SET_PHYSICS_ORDER, 0, 0)));
        assert_eq!(
            run(EE_UNITS_ACTION_AIRPLANE, &mut unit(ACTION_HOLDING_PATTERN)),
            vec![(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0, 0), (EE_UNITS_ACT_HOLDING_PATTERN, 0, 0)]
        );
        assert_eq!(
            run(EE_UNITS_ACTION_PLANET_SPACESHIP, &mut unit(ACTION_HOLDING_PATTERN)),
            vec![(EE_UNITS_ACT_SET_PHYSICS_ACTION, 3, 0), (EE_UNITS_ACT_HOLDING_PATTERN, 1, 0)]
        );
        assert_eq!(
            run(EE_UNITS_ACTION_AIRPLANE, &mut unit(ACTION_CIRCLE_AIRTRAFFIC_CONTROLLER))[..2].to_vec(),
            vec![(EE_UNITS_ACT_SET_PHYSICS_ACTION, 0, 0), (EE_UNITS_ACT_SET_PHYSICS_ORDER, 0, 0)]
        );
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_AIRPLANE, &mut unit(ACTION_SEND_PARATROOPER_PLANE_OFF_MAP))),
            vec![EE_UNITS_ACT_FLY_OFF_MAP]
        );
        // a dying planet spaceship: the sound first, the effect last; the rest the landable's
        let mut m = unit(ACTION_DYING);
        m.reads.insert(EE_UNITS_ACT_R_DEATH_SOUND, 1);
        m.reads.insert(EE_UNITS_ACT_R_DEATH_EFFECT, 1);
        let log = ops(&run(EE_UNITS_ACTION_PLANET_SPACESHIP, &mut m));
        assert_eq!((log[0], *log.last().unwrap()), (EE_UNITS_ACT_DEATH_SOUND, EE_UNITS_ACT_DEATH_EFFECT));
        assert_eq!(
            ops(&run(EE_UNITS_ACTION_AIRPLANE, &mut unit(ACTION_TAKEOFF))),
            vec![EE_UNITS_ACT_SET_PHYSICS_ORDER, EE_UNITS_ACT_TAKEOFF]
        );
    }
}
