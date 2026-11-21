//! An effect run's world calls that were statement sequences in the C++ host (`EESMEffectWorld`, `SMEUnitsWorld` in
//! Scenario Manager/EESMEffect.cpp), composed here of single statements (`EE_SCENARIO_WX_*`, each one C++ statement on
//! the run's context: the scenario editor lane's round 3). [`compose`] answers an `EE_SCENARIO_WORLD_*` operation by
//! its statements in the C++'s order, or `None` for an operation the C++ answers itself; the effect logic
//! (effect.rs, task.rs) and its tests see the same operations as before.
//!
//! The C++ keeps the objects a sequence builds (the run's group, goals, transformation, calamity, the gate part, the
//! town and patrol target, the tile node, the sound's sample), so no Rust reference to them exists.

use std::os::raw::c_long;

use ee_base::math::types::GeVector;
use ee_base::math::um::um_arctan2;

use crate::scenario::effect::*;
use crate::scenario::task::*;

// ---- the statements (`a` the arguments, ids of units, indices of players; `f(x)` a float's bits)
/// `SMEUnit(a)->GetPlayer()->DetachUnitFromAI(SMEUnit(a)->GetComplexUnit())`.
pub const EE_SCENARIO_WX_DETACH: u32 = 400;
/// `inRun->mGroup = new EEActiveGroup(SMEPlayer(a))`.
pub const EE_SCENARIO_WX_GROUP_CREATE: u32 = 401;
/// `SMEPlayer(a)->mPendingGroups.push_back(inRun->mGroup)`.
pub const EE_SCENARIO_WX_GROUP_PEND: u32 = 402;
/// `SMEComplex(a)->GetComplexUnit() != NULL`.
pub const EE_SCENARIO_WX_HAS_COMPLEX: u32 = 403;
/// `SMEComplex(a)->GetComplexUnit()->SetFlightTime(SMEComplex(a)->GetStats()->mFlightTime)`.
pub const EE_SCENARIO_WX_REFUEL: u32 = 404;
/// `SMEComplex(a)->GetComplexUnit()->SetAmmunition(SMEComplex(a)->GetUnitClass()->mAmmunition)`.
pub const EE_SCENARIO_WX_REARM: u32 = 405;
/// `inRun->mFound[0] = theGroup->FindUnitWithFlags(kUnitKindCitizen)`, `[1]` the naval citizen's; `inRun->mFound[a]
/// != NULL`; `inRun->mFound[a]->CanGather(SMEUnit(b), SMEUnit(b)->GetTileX(), SMEUnit(b)->GetTileY(), true, false)`.
pub const EE_SCENARIO_WX_GROUP_FIND: u32 = 406;
pub const EE_SCENARIO_WX_FOUND_CAN_GATHER: u32 = 407;
pub const EE_SCENARIO_WX_GROUP_FIND_NAVAL: u32 = 560;
pub const EE_SCENARIO_WX_FOUND: u32 = 561;
/// `theGroup->SpreadAroundTile(SMEUnit(a)->GetTileX(), SMEUnit(a)->GetTileY())`.
pub const EE_SCENARIO_WX_GROUP_SPREAD_AT_UNIT: u32 = 408;
/// `theGroup->SpreadAroundUnit(SMEUnit(a), SMEUnit(a)->GetTileX(), SMEUnit(a)->GetTileY())`.
pub const EE_SCENARIO_WX_GROUP_SPREAD_AROUND: u32 = 409;
/// `theGroup->ClearGoal()`.
pub const EE_SCENARIO_WX_GROUP_CLEAR_GOAL: u32 = 410;
/// The group's goal (`inRun->mGroupGoal`): `NewGather(SMEUnit(a), NULL, true)`, `NewAttackUnit(theGroup->GetFirstUnit(),
/// SMEUnit(a), true, false)`, `NewGather(NULL, SMEUnit(a)->GetComplexUnit(), true)`, `NewLocation(GE2DPoint((float)(long)a,
/// (float)(long)b), true, true, false, 0, false, 0)`, `NewPatrol(SMEUnit(a)->GetComplexUnit()->mPatrolGroup,
/// SMEUnit(a)->GetComplexUnit(), true)`, `NewPatrol(true)`, `NewLocation(GE2DPoint(f(a), f(b)), true, c != 0, false, 0,
/// false, 0)`, `NewExplore(true)`, `NewPatrol(inRun->mPatrol, inRun->mPatrolTarget, true)`.
pub const EE_SCENARIO_WX_GOAL_GATHER: u32 = 411;
pub const EE_SCENARIO_WX_GOAL_ATTACK: u32 = 412;
pub const EE_SCENARIO_WX_GOAL_DROPOFF: u32 = 413;
pub const EE_SCENARIO_WX_GOAL_TILE: u32 = 414;
pub const EE_SCENARIO_WX_GOAL_PATROL_POINT: u32 = 415;
pub const EE_SCENARIO_WX_GOAL_PATROL_NEW: u32 = 416;
pub const EE_SCENARIO_WX_GOAL_MOVE: u32 = 417;
pub const EE_SCENARIO_WX_GOAL_EXPLORE: u32 = 418;
pub const EE_SCENARIO_WX_GOAL_TOWN_PATROL: u32 = 419;
/// `theGroup->SetGoal(inRun->mGroupGoal, false)`, `theGroup->mAction = a`, `theGroup->Plan()` (`SMEGroupGoal`).
pub const EE_SCENARIO_WX_GROUP_SET_GOAL: u32 = 420;
pub const EE_SCENARIO_WX_GROUP_ACTION: u32 = 421;
pub const EE_SCENARIO_WX_GROUP_PLAN: u32 = 422;
/// A unit's goal (`inRun->mUnitGoal`): `NewHealUnit(SMEComplex(a), true)`, `NewLocation(GE2DPoint(f(a), f(b)), true,
/// c != 0, false, 0, false, 0)`, `NewCastCalamity(SMEComplex(a), SMEUnitOrNull(b), c, d, inRun->mCalamities[e],
/// true)`, `NewUnloadTransport(SMEComplex(a), (long)f(b), (long)f(c), true)`, `NewLocation(f(a), f(b), 0, true, true,
/// false, 0, false, 0)`, `NewLocation(&GE3DPoint(SMEComplex(a)'s location), true, true, true, f(b), false, false)`.
pub const EE_SCENARIO_WX_UGOAL_HEAL: u32 = 423;
pub const EE_SCENARIO_WX_UGOAL_LOCATION: u32 = 424;
pub const EE_SCENARIO_WX_UGOAL_CAST: u32 = 425;
pub const EE_SCENARIO_WX_UGOAL_UNLOAD: u32 = 426;
pub const EE_SCENARIO_WX_UGOAL_POINT: u32 = 427;
pub const EE_SCENARIO_WX_UGOAL_FACE: u32 = 428;
/// `SMEComplex(a)->SetGoal(inRun->mUnitGoal, 0, 0)`; `inRun->mUnitGoal->CanUnloadAtDestination()`; `delete
/// inRun->mUnitGoal`.
pub const EE_SCENARIO_WX_UNIT_SET_GOAL: u32 = 429;
pub const EE_SCENARIO_WX_UGOAL_CAN_UNLOAD: u32 = 430;
pub const EE_SCENARIO_WX_UGOAL_DELETE: u32 = 431;
/// `SMEComplex(a)->GetGoal()->mKind`.
pub const EE_SCENARIO_WX_GOAL_KIND: u32 = 432;
/// `inRun->mGoal = EELoadGarrisonGoalCast(SMEComplex(a)->GetGoal())`, the transport's, `inRun->mGoal->AddLoadUnits(
/// inRun->mLoad)`, `inRun->mGoal = EEUGRust::NewLoad(a, SMEComplex(b), inRun->mLoad, true)`.
pub const EE_SCENARIO_WX_GOAL_CAST_GARRISON: u32 = 433;
pub const EE_SCENARIO_WX_GOAL_CAST_TRANSPORT: u32 = 434;
pub const EE_SCENARIO_WX_GOAL_ADD_LOAD: u32 = 435;
pub const EE_SCENARIO_WX_GOAL_NEW_LOAD: u32 = 436;
/// `SMEComplex(a)->SetGoal(inRun->mGoal, 0, 0)` and with `inRun->mGoal->Clone()`.
pub const EE_SCENARIO_WX_SET_RUN_GOAL: u32 = 437;
pub const EE_SCENARIO_WX_SET_RUN_GOAL_CLONE: u32 = 438;
/// `theGroup->GetUnit(a) != SMEUnit(b)`; `inRun->mLoad.push_back(theGroup->GetUnit(a))`; the target's class's
/// `FilterLoadable(&inRun->mLoad, SMEUnit(a)->GetComplexUnit(), &inRun->mRejected)`.
pub const EE_SCENARIO_WX_GROUP_UNIT_IS_NOT: u32 = 439;
pub const EE_SCENARIO_WX_LOAD_PUSH_GROUP_UNIT: u32 = 440;
pub const EE_SCENARIO_WX_FILTER_LOADABLE: u32 = 441;
/// `theGroup->Finish()`, `theGroup->AddMember(inRun->mRejected[a], true)`.
pub const EE_SCENARIO_WX_GROUP_FINISH: u32 = 442;
pub const EE_SCENARIO_WX_GROUP_ADD_REJECTED: u32 = 443;
/// `SMEComplex(a)->OrderRepair(SMEUnit(b))`.
pub const EE_SCENARIO_WX_ORDER_REPAIR: u32 = 444;
/// `EEPlayForLocalPlayer(SMEPlayer(a), theGroup->GetFirstUnit()->GetStats()->GetAcknowledgeSound(-1))`.
pub const EE_SCENARIO_WX_ACKNOWLEDGE: u32 = 445;
/// The group's first unit's `GetTileX()`, `GetTileY()`; `ee_units_pg_add_point` of the group goal's patrol group at
/// the first unit's tile (`a` the kind) and at `f(b)`, `f(c)`; the first unit's player's `AddPatrolGroup` of it.
pub const EE_SCENARIO_WX_FIRST_TILE_X: u32 = 446;
pub const EE_SCENARIO_WX_FIRST_TILE_Y: u32 = 447;
pub const EE_SCENARIO_WX_PATROL_ADD_FIRST: u32 = 448;
pub const EE_SCENARIO_WX_PATROL_ADD: u32 = 449;
pub const EE_SCENARIO_WX_FIRST_ADD_PATROL: u32 = 450;
/// `SMEComplex(a)->QueueGoal(EEUGRust::NewLocation(GE2DPoint(f(b), f(c)), true, d != 0, false, 0, false, 0), e)`;
/// `SMEComplex(a)->mGoalQueues[b].AddTime(c)`.
pub const EE_SCENARIO_WX_QUEUE_LOCATION: u32 = 451;
pub const EE_SCENARIO_WX_QUEUE_ADD_TIME: u32 = 452;
/// The calamity nodes: `inRun->mCalamities.clear()`, `SMEPlayer(a)->GetTechTree()->GetNodeIDs(inRun->mNodeIDs)`,
/// `inRun->mNodeIDs.size()`, node `b`'s `mData->mCalamity`, its `GetIsAvailable()`, `inRun->mCalamities.push_back(
/// inRun->mNodeIDs[a])`, `inRun->mCalamities.size()`, whether `SMEPlayer(b)->GetTechTree()->Get(mCalamities[c])`
/// contains `SMEComplex(a)`'s class's type.
pub const EE_SCENARIO_WX_CAL_CLEAR: u32 = 453;
pub const EE_SCENARIO_WX_NODE_IDS: u32 = 454;
pub const EE_SCENARIO_WX_NODE_COUNT: u32 = 455;
pub const EE_SCENARIO_WX_NODE_CALAMITY: u32 = 456;
pub const EE_SCENARIO_WX_NODE_AVAILABLE: u32 = 457;
pub const EE_SCENARIO_WX_CAL_PUSH: u32 = 458;
pub const EE_SCENARIO_WX_CAL_COUNT: u32 = 459;
pub const EE_SCENARIO_WX_CAL_CONTAINS: u32 = 460;
/// `inRun->mCalamity != NULL`, its `GetTargetKind()`, `IsValidTarget(SMEComplex(a), SMEUnit(b))`,
/// `IsValidLocation(SMEComplex(a), b, c)`.
pub const EE_SCENARIO_WX_CAL_HAS: u32 = 461;
pub const EE_SCENARIO_WX_CAL_KIND: u32 = 462;
pub const EE_SCENARIO_WX_CAL_IS_VALID_TARGET: u32 = 463;
pub const EE_SCENARIO_WX_CAL_IS_VALID_LOCATION: u32 = 464;
/// `inRun->mPlace = GE2DPoint(f(a), f(b))`.
pub const EE_SCENARIO_WX_PLACE_SET: u32 = 465;
/// An area's `GetArea(a) != NULL`, `IsUnset()`, `IsOffMap()`; an object's `!= NULL`, `Evaluate(false)`.
pub const EE_SCENARIO_WX_AREA_EXISTS: u32 = 466;
pub const EE_SCENARIO_WX_AREA_UNSET: u32 = 467;
pub const EE_SCENARIO_WX_AREA_OFF_MAP: u32 = 468;
pub const EE_SCENARIO_WX_OBJECT_EXISTS: u32 = 469;
pub const EE_SCENARIO_WX_OBJECT_EVALUATE: u32 = 470;
/// The map tile (a, b): `!= NULL`, `mHeight`'s bits.
pub const EE_SCENARIO_WX_TILE_EXISTS: u32 = 471;
pub const EE_SCENARIO_WX_TILE_HEIGHT: u32 = 472;
/// `inRun->mHeld = SMEUnit(a)`; its `Kill(a != 0, false, false)`, `mTileNode != NULL`, `GetPlayer()->KillUnit(it,
/// false)`.
pub const EE_SCENARIO_WX_HOLD: u32 = 473;
pub const EE_SCENARIO_WX_HELD_KILL: u32 = 474;
pub const EE_SCENARIO_WX_HELD_ON_NODE: u32 = 475;
pub const EE_SCENARIO_WX_HELD_PLAYER_KILL: u32 = 476;
/// `SMEComplex(a)`'s location's element (b, c), bits.
pub const EE_SCENARIO_WX_LOCATION_AT: u32 = 477;
/// `inRun->mFace = SMEComplex(a)->mPhysicalModel.GetLocation()`, `inRun->mFace.SetOrientationYPR(f(a), f(b), f(c))`,
/// `SMEComplex(a)->SetTransformation(inRun->mFace, true)`, `inRun->mFacePoint = GE3DPoint(mFace's x, y, z)`,
/// `SMEComplex(a)->PlaceOnGround(inRun->mFacePoint)`.
pub const EE_SCENARIO_WX_FACE_FROM_UNIT: u32 = 479;
pub const EE_SCENARIO_WX_FACE_ORIENT: u32 = 480;
pub const EE_SCENARIO_WX_FACE_APPLY: u32 = 481;
pub const EE_SCENARIO_WX_FACE_POINT: u32 = 482;
pub const EE_SCENARIO_WX_PLACE_ON_GROUND: u32 = 483;
/// `gUIManager->GetMainGameForm() != NULL`; its `mRefreshGroup = true`.
pub const EE_SCENARIO_WX_MAIN_FORM: u32 = 484;
pub const EE_SCENARIO_WX_REFRESH_GROUP: u32 = 485;
/// `inRun->mGroup = new EEActiveGroup(SMEComplex(a)->mPlayer)`, `SMEComplex(a)->mPlayer->mPendingGroups.push_back(
/// inRun->mGroup)` (a stance's group).
pub const EE_SCENARIO_WX_UNIT_GROUP_CREATE: u32 = 486;
pub const EE_SCENARIO_WX_UNIT_GROUP_PEND: u32 = 487;
/// `theGroup->AddMember(SMEComplex(a), true)`.
pub const EE_SCENARIO_WX_GROUP_ADD_COMPLEX: u32 = 488;
/// `SMEComplex(a)->mIsLocked`, `= b != 0`; the gate's second and fourth parts into `inRun->mPart` (`!= NULL`), its
/// `mIsLocked`, `= a != 0`.
pub const EE_SCENARIO_WX_LOCKED: u32 = 489;
pub const EE_SCENARIO_WX_SET_LOCKED: u32 = 490;
pub const EE_SCENARIO_WX_GATE_SECOND: u32 = 491;
pub const EE_SCENARIO_WX_GATE_FOURTH: u32 = 492;
pub const EE_SCENARIO_WX_PART_LOCKED: u32 = 493;
pub const EE_SCENARIO_WX_PART_SET_LOCKED: u32 = 494;
/// `(inRun->mTown = SMEComplex(a)->FindNearestTown()) != NULL`, `(inRun->mPatrol = inRun->mTown->mPatrolGroup) !=
/// NULL`, `(inRun->mPatrolTarget = ee_units_pg_nearest(inRun->mPatrol, SMEComplex(a)'s tile)) != NULL`.
pub const EE_SCENARIO_WX_TOWN_FIND: u32 = 495;
pub const EE_SCENARIO_WX_TOWN_PATROL: u32 = 496;
pub const EE_SCENARIO_WX_PATROL_NEAREST: u32 = 497;
/// `SMEComplex(a)->mBehavior == EEUnitBehavior::sTable[b]`; `= sTable[b]`.
pub const EE_SCENARIO_WX_BEHAVIOR_IS: u32 = 498;
pub const EE_SCENARIO_WX_SET_BEHAVIOR: u32 = 499;
/// `SMEPlayer(a)->SetGameResult(b)`; `gWorld.RemovePlayerFromGame(a != 0, b)`.
pub const EE_SCENARIO_WX_SET_GAME_RESULT: u32 = 500;
pub const EE_SCENARIO_WX_REMOVE_PLAYER: u32 = 501;
/// `gGraphics->PostFadeIn(3000)`, `PostFadeOut(3000)`.
pub const EE_SCENARIO_WX_FADE_IN: u32 = 502;
pub const EE_SCENARIO_WX_FADE_OUT: u32 = 503;
/// Player `a`'s node of the effect's technology: `mData->mMakesUnit`, `gUnitClassTable[mData->mUnitClass] != NULL`,
/// `EEReplaceQueuedTechnology(player, that class, 100000)`, `GetTechTree()->MakeObsolete(mTechnology)`.
pub const EE_SCENARIO_WX_TECH_MAKES_UNIT: u32 = 504;
pub const EE_SCENARIO_WX_TECH_CLASS_EXISTS: u32 = 505;
pub const EE_SCENARIO_WX_TECH_REPLACE_QUEUED: u32 = 506;
pub const EE_SCENARIO_WX_TECH_OBSOLETE: u32 = 507;
/// `SMEPlayer(a)->mSeesAll = b != 0`; `SMEPlayer(a) == gWorld.GetPlayer(gWorld.mCurrentPlayer)`;
/// `gMiniMap->Redraw()`.
pub const EE_SCENARIO_WX_SET_SEES_ALL: u32 = 508;
pub const EE_SCENARIO_WX_PLAYER_IS_CURRENT: u32 = 509;
pub const EE_SCENARIO_WX_MINIMAP_REDRAW: u32 = 510;
/// `SMEPlayer(a)->mCivilization.AddPoints(b)`; `EESetCivBuilderButtonBlinking(true)`; the trumpets' sound object
/// `!= NULL`, its `Play()`.
pub const EE_SCENARIO_WX_CIV_ADD: u32 = 511;
pub const EE_SCENARIO_WX_CIV_BLINK: u32 = 512;
pub const EE_SCENARIO_WX_TRUMPETS: u32 = 513;
pub const EE_SCENARIO_WX_TRUMPETS_PLAY: u32 = 514;
/// A unit's raw tile (`mTile != NULL`, `mTileX`, `mTileY`) and its container's.
pub const EE_SCENARIO_WX_UNIT_ON_TILE: u32 = 515;
pub const EE_SCENARIO_WX_UNIT_TILE_X: u32 = 516;
pub const EE_SCENARIO_WX_UNIT_TILE_Y: u32 = 517;
pub const EE_SCENARIO_WX_UNIT_HAS_COMPLEX: u32 = 518;
pub const EE_SCENARIO_WX_UNIT_IN_CONTAINER: u32 = 519;
pub const EE_SCENARIO_WX_CONTAINER_ON_TILE: u32 = 520;
pub const EE_SCENARIO_WX_CONTAINER_TILE_X: u32 = 521;
pub const EE_SCENARIO_WX_CONTAINER_TILE_Y: u32 = 522;
/// `GetTrigger(a)->mConditionStartTime = b`, `mIsOn = b != 0`, `mHasFired = b != 0`.
pub const EE_SCENARIO_WX_TRIGGER_START: u32 = 523;
pub const EE_SCENARIO_WX_TRIGGER_ON: u32 = 524;
pub const EE_SCENARIO_WX_TRIGGER_FIRED: u32 = 525;
/// The calamity's tile walk: `(inRun->mMapTile = gWorld.mMap->GetTile(a, b)) != NULL`, `(inRun->mNode =
/// mMapTile->mUnits) != NULL`, `(inRun->mNode = mNode->mNext) != NULL`, `mNode->mUnit == NULL`, the unit in
/// `inRun->mSeen`, `inRun->mCalamity->CanCastAt(NULL, inRun->mPoint, its tile, true, it) != 0`, `CreateUnitCalamity(a,
/// -1, its id, 0)`, `inRun->mSeen.push_back(it)`.
pub const EE_SCENARIO_WX_MAP_TILE: u32 = 526;
pub const EE_SCENARIO_WX_NODE_FIRST: u32 = 527;
pub const EE_SCENARIO_WX_NODE_NEXT: u32 = 528;
pub const EE_SCENARIO_WX_NODE_NO_UNIT: u32 = 529;
pub const EE_SCENARIO_WX_NODE_SEEN: u32 = 530;
pub const EE_SCENARIO_WX_NODE_REFUSED: u32 = 531;
pub const EE_SCENARIO_WX_NODE_CREATE: u32 = 532;
pub const EE_SCENARIO_WX_NODE_MARK: u32 = 533;
/// `(inRun->mGraphic = EEGraphicEffect::Get(a)) != NULL`, its `Load(false)`, its `mKind`.
pub const EE_SCENARIO_WX_GFX_GET: u32 = 534;
pub const EE_SCENARIO_WX_GFX_LOAD: u32 = 535;
pub const EE_SCENARIO_WX_GFX_KIND: u32 = 536;
/// The weather's tiles: `inRun->mTiles.clear()`, `.push_back(U2DPoint<long>(a, b))`,
/// `gEffectManager->CreateWeatherEffect(inRun->mTiles, inRun->mGraphic, NULL)`.
pub const EE_SCENARIO_WX_TILES_CLEAR: u32 = 537;
pub const EE_SCENARIO_WX_TILES_PUSH: u32 = 538;
pub const EE_SCENARIO_WX_WEATHER_CREATE: u32 = 539;
/// The map tile (a, b)'s `GetHeight()` bits, whether its terrain is deep or shallow water; `inRun->mPoint =
/// GE3DPoint(f(a), f(b), f(c))`; `gEffectManager->CreatePlacedEffect(inRun->mPoint, inRun->mGraphic, NULL)`.
pub const EE_SCENARIO_WX_TILE_GET_HEIGHT: u32 = 540;
pub const EE_SCENARIO_WX_TILE_WATER: u32 = 541;
pub const EE_SCENARIO_WX_POINT_SET: u32 = 542;
pub const EE_SCENARIO_WX_PLACED_CREATE: u32 = 543;
/// `gWorld.mEnvironmentManager != NULL`, its `SetEnabled(a != 0)`.
pub const EE_SCENARIO_WX_ENV_EXISTS: u32 = 544;
pub const EE_SCENARIO_WX_ENV_SET: u32 = 545;
/// Computer player `a`'s toggles `SetActive` .. `SetCanGather` (`EE_SCENARIO_WX_AI_TOGGLE + n`, n 0..7: active,
/// cheating, attack, defend, build, explore, ignore others, gather) with `b != 0`.
pub const EE_SCENARIO_WX_AI_TOGGLE: u32 = 546;
/// Computer player `a`'s `SetFavoriteAirType(b)`, `SetFavoriteLandType(b)`, `SetFavoriteSeaType(b)`.
pub const EE_SCENARIO_WX_AI_FAVORITE_AIR: u32 = 554;
pub const EE_SCENARIO_WX_AI_FAVORITE_LAND: u32 = 555;
pub const EE_SCENARIO_WX_AI_FAVORITE_SEA: u32 = 556;
/// `theCamera.SetZoom(f(a))`.
pub const EE_SCENARIO_WX_CAMERA_SET_ZOOM: u32 = 557;
/// `SMEComplex(a)->StartAction(b)`; `SMEComplex(a)->GetUnitClass()->ActionChanged(SMEComplex(a))` (`SMEAction`).
pub const EE_SCENARIO_WX_START_ACTION: u32 = 558;
pub const EE_SCENARIO_WX_ACTION_CHANGED: u32 = 559;

/// Task-Area's placement (`SMETakeOffMap`, `SMEPlaceInArea`) on unit `a` (`SMEUnit(a)->GetComplexUnit()` its complex
/// unit) and the run's transformation `mT` (`inRun->mTransformation`): the complex class's `IsLandBuilding()`;
/// `mT = location`; `Set2DPosition(U2DPoint<float>(mT.GetX(), mT.GetY()))`; the unit class's air traffic type and
/// the complex class's `IsAirBase()`; `ee_units_atc_remove_airport` / `_add_airport` of the player's air traffic;
/// the local player's selection holds it; `gUserInterface->PostMessage424Once()`; `TakeOffMap()`; the player's
/// off-map info `!= NULL`, its `mReason = 2`; `mLastMapPosition = GE3DPoint(mT)`.
pub const EE_SCENARIO_WX_LAND_BUILDING: u32 = 562;
pub const EE_SCENARIO_WX_T_FROM_UNIT: u32 = 563;
pub const EE_SCENARIO_WX_SET_2D_FROM_T: u32 = 564;
pub const EE_SCENARIO_WX_AIRPORT: u32 = 565;
pub const EE_SCENARIO_WX_AIRPORT_REMOVE: u32 = 566;
pub const EE_SCENARIO_WX_AIRPORT_ADD: u32 = 567;
pub const EE_SCENARIO_WX_SELECTED_LOCALLY: u32 = 568;
pub const EE_SCENARIO_WX_POST_424: u32 = 569;
pub const EE_SCENARIO_WX_TAKE_OFF_MAP: u32 = 570;
pub const EE_SCENARIO_WX_OFF_MAP_INFO: u32 = 571;
pub const EE_SCENARIO_WX_OFF_MAP_REASON: u32 = 572;
pub const EE_SCENARIO_WX_LAST_MAP_POSITION: u32 = 573;
/// The effect's object's `mInAreaCheck`, `mArea`, `mNearCheck` (`_SET_*`: `= a`); the effect's `mTaskArea`, the task
/// area's `IsOffMap()`; the object's `FindPlacement(SMEUnit(a)'s class's type, inRun->mPlace, true)`; `mT.SetX(f(a))`,
/// `SetY`, `mMatrix[2][3] = f(a)`, `mT.GetX()`, `GetY()`; the complex unit's `mU0DC` x and y; `inRun->mPlace`'s x and
/// y; `(inRun->mMapTile = GetTile((long)mPlace.x, (long)mPlace.y)) != NULL`, its `GetHeight()`; the unit class's air
/// domain, the complex unit's location's z, the class's `GetCruiseAltitude()`; the ground transformation
/// `inRun->mGround`'s `MakeIdentity()` and `SetPosition(GE3DPoint(mT))`; the class's `mPitchesWithTerrain` /
/// `mRollsWithTerrain`, `inRun->mPitch = gWorld.mMap->GetTerrainPitch(inRun->mGround)` / `mRoll = GetTerrainRoll`;
/// `mT.SetOrientationYPR(UMArcTan2(location (1, 0), (0, 0)), inRun->mPitch, inRun->mRoll)`; `mContainer == NULL`,
/// `SetTransformation(mT, true)`, `mContainer->UnloadCargoByID(mUnitID, mT)`; the class's `GetTileSize()`;
/// `mPhysicalModel.SetLocation(mT)`; the class's category, the complex unit's `mFlightTime` and `mHomeBase != NULL`.
pub const EE_SCENARIO_WX_OBJ_IN_AREA_CHECK: u32 = 574;
pub const EE_SCENARIO_WX_OBJ_AREA: u32 = 575;
pub const EE_SCENARIO_WX_OBJ_NEAR_CHECK: u32 = 576;
pub const EE_SCENARIO_WX_OBJ_SET_IN_AREA_CHECK: u32 = 577;
pub const EE_SCENARIO_WX_OBJ_SET_AREA: u32 = 578;
pub const EE_SCENARIO_WX_OBJ_SET_NEAR_CHECK: u32 = 579;
pub const EE_SCENARIO_WX_TASK_AREA_OFF_MAP: u32 = 580;
pub const EE_SCENARIO_WX_TASK_FIND_PLACEMENT: u32 = 581;
pub const EE_SCENARIO_WX_T_SET_X: u32 = 582;
pub const EE_SCENARIO_WX_T_SET_Y: u32 = 583;
pub const EE_SCENARIO_WX_T_SET_Z: u32 = 584;
pub const EE_SCENARIO_WX_T_X: u32 = 585;
pub const EE_SCENARIO_WX_U0DC_X: u32 = 586;
pub const EE_SCENARIO_WX_U0DC_Y: u32 = 587;
pub const EE_SCENARIO_WX_PLACE_X: u32 = 588;
pub const EE_SCENARIO_WX_PLACE_Y: u32 = 589;
pub const EE_SCENARIO_WX_PLACE_TILE: u32 = 590;
pub const EE_SCENARIO_WX_PLACE_TILE_HEIGHT: u32 = 591;
pub const EE_SCENARIO_WX_UNIT_AIR: u32 = 592;
pub const EE_SCENARIO_WX_UNIT_Z: u32 = 593;
pub const EE_SCENARIO_WX_CRUISE: u32 = 594;
pub const EE_SCENARIO_WX_GROUND_IDENTITY: u32 = 595;
pub const EE_SCENARIO_WX_GROUND_POSITION: u32 = 596;
pub const EE_SCENARIO_WX_PITCHES: u32 = 597;
pub const EE_SCENARIO_WX_ROLLS: u32 = 598;
pub const EE_SCENARIO_WX_TERRAIN_PITCH: u32 = 599;
pub const EE_SCENARIO_WX_TERRAIN_ROLL: u32 = 600;
pub const EE_SCENARIO_WX_T_ORIENT_HEADING: u32 = 601;
pub const EE_SCENARIO_WX_CONTAINER_NULL: u32 = 602;
pub const EE_SCENARIO_WX_T_APPLY: u32 = 603;
pub const EE_SCENARIO_WX_UNLOAD_CARGO: u32 = 604;
pub const EE_SCENARIO_WX_TILE_SIZE: u32 = 605;
pub const EE_SCENARIO_WX_T_SET_LOCATION: u32 = 606;
pub const EE_SCENARIO_WX_CATEGORY: u32 = 607;
pub const EE_SCENARIO_WX_FLIGHT_TIME: u32 = 608;
pub const EE_SCENARIO_WX_HOME_BASE: u32 = 609;
pub const EE_SCENARIO_WX_T_Y: u32 = 635;
pub const EE_SCENARIO_WX_EFFECT_TASK_AREA: u32 = 636;
/// Modify Tech Tree's add (`SMETechAdd`) for player `a` and the effect's technology `t`: `t`, `EEEpochCount()`, the
/// tree's `mEndEpoch`, `ResetEpochRange(mStartEpoch, t)`, node `t`'s `MakeAvailable()`, `mEpoch = 0`, `mObsoleteEpoch
/// = EEEpochCount()`, `SetScenEditorObsoleted(false, false)`, `SetObsolete(false, false)`, `NeedsUnitMedia() == true
/// && !mIsUnitMediaLoaded`, the player's `SetUnitClassMedia(its class, 0x7f)`, `mIsUnitMediaLoaded = true`,
/// `mData->mKind`, the tree's `mEpochsBeforeWonders = 0`, `Research(t, NULL)`.
pub const EE_SCENARIO_WX_EFFECT_TECH: u32 = 610;
pub const EE_SCENARIO_WX_EPOCH_COUNT: u32 = 611;
pub const EE_SCENARIO_WX_TREE_END_EPOCH: u32 = 612;
pub const EE_SCENARIO_WX_TREE_RESET_RANGE: u32 = 613;
pub const EE_SCENARIO_WX_TECH_MAKE_AVAILABLE: u32 = 614;
pub const EE_SCENARIO_WX_TECH_EPOCH_ZERO: u32 = 615;
pub const EE_SCENARIO_WX_TECH_OBSOLETE_EPOCH: u32 = 616;
pub const EE_SCENARIO_WX_TECH_SCEN_OBSOLETED: u32 = 617;
pub const EE_SCENARIO_WX_TECH_SET_OBSOLETE: u32 = 618;
pub const EE_SCENARIO_WX_TECH_NEEDS_MEDIA: u32 = 619;
pub const EE_SCENARIO_WX_TECH_LOAD_MEDIA: u32 = 620;
pub const EE_SCENARIO_WX_TECH_MEDIA_LOADED: u32 = 621;
pub const EE_SCENARIO_WX_TECH_KIND: u32 = 622;
pub const EE_SCENARIO_WX_TREE_WONDERS_ZERO: u32 = 623;
pub const EE_SCENARIO_WX_TECH_RESEARCH: u32 = 624;
/// Set Favorite Units' class tests (`SMEClassValue`, class `a`): `EEUnitClassCount()`, `gUnitClassTable[a] !=
/// NULL`, its air and sea domains, `GetIsProjectile()`, `IsLandBuilding()`, `GetAttack() > 0`, `GetMaxVelocity() >
/// 0`, `FliesOver()`, `GetCategory()`.
pub const EE_SCENARIO_WX_CLASS_COUNT: u32 = 625;
pub const EE_SCENARIO_WX_CLASS_EXISTS: u32 = 626;
pub const EE_SCENARIO_WX_CLASS_AIR: u32 = 627;
pub const EE_SCENARIO_WX_CLASS_SEA: u32 = 628;
pub const EE_SCENARIO_WX_CLASS_PROJECTILE: u32 = 629;
pub const EE_SCENARIO_WX_CLASS_LAND_BUILDING: u32 = 630;
pub const EE_SCENARIO_WX_CLASS_ATTACKS: u32 = 631;
pub const EE_SCENARIO_WX_CLASS_MOVES: u32 = 632;
pub const EE_SCENARIO_WX_CLASS_FLIES_OVER: u32 = 633;
pub const EE_SCENARIO_WX_CLASS_CATEGORY: u32 = 634;

/// Create Units' unit (`EESMCreateUnit`; the context a plain `EESMHostContext`, `L` its `*mLocation`; a class `c` and
/// a unit `u` are handles the earlier statements answered): `gUnitClassTable[a]`, the class of the object's unit
/// `a`, the object's `FindPlacement(a, L, true)`, the player's node of the class's technology `!= NULL`, its
/// `mData->mKind`, `L`'s x and y (bits), `L = GE2DPoint(f(a), f(b))`, `ee_units_uu_plan_wall` at `L` into the
/// context's wall pieces, the first piece, their `clear()`, the class's `IsLandBuilding()`, `CreateUnit((long)L.x,
/// (long)L.y, player, false, NULL)`; the unit's `mTileNode != NULL`, `SetBuilt(1, 0, 1)`, its class's air domain,
/// the refuel and rearm, `gWorld.mMap->mSize`, the context's transformation from the unit's location, its
/// `SetOrientationYPR(f(a), 0, 0)`, `SetTransformation(it, true)`, the stats' power, `SetPower`, the player's
/// `AddPendingUnits(false, true)`, `gWorld.FlushQueuedPlacements()`, `PlaceUnitOnGround(u)`, the object's
/// `mHasAttributeCheck`, `EEChangeUnit(the object's attribute, ..., u, ...)`, `mCreated->push_back(its id)`, the
/// creation sound `!= NULL`, the unit's player is the local one, the sound's `Play`.
pub const EE_SCENARIO_WC_CLASS: u32 = 700;
pub const EE_SCENARIO_WC_OBJECT_UNIT_CLASS: u32 = 701;
pub const EE_SCENARIO_WC_FIND_PLACEMENT: u32 = 702;
pub const EE_SCENARIO_WC_TECH_NODE: u32 = 703;
pub const EE_SCENARIO_WC_TECH_KIND: u32 = 704;
pub const EE_SCENARIO_WC_LOCATION_X: u32 = 705;
pub const EE_SCENARIO_WC_LOCATION_Y: u32 = 706;
pub const EE_SCENARIO_WC_LOCATION_SET: u32 = 707;
pub const EE_SCENARIO_WC_PLAN_WALL: u32 = 708;
pub const EE_SCENARIO_WC_PIECES_FIRST: u32 = 709;
pub const EE_SCENARIO_WC_PIECES_CLEAR: u32 = 710;
pub const EE_SCENARIO_WC_IS_LAND_BUILDING: u32 = 711;
pub const EE_SCENARIO_WC_CREATE: u32 = 712;
pub const EE_SCENARIO_WC_ON_NODE: u32 = 713;
pub const EE_SCENARIO_WC_SET_BUILT: u32 = 714;
pub const EE_SCENARIO_WC_AIR: u32 = 715;
pub const EE_SCENARIO_WC_REFUEL: u32 = 716;
pub const EE_SCENARIO_WC_REARM: u32 = 717;
pub const EE_SCENARIO_WC_MAP_SIZE: u32 = 718;
pub const EE_SCENARIO_WC_T_FROM: u32 = 719;
pub const EE_SCENARIO_WC_T_ORIENT: u32 = 720;
pub const EE_SCENARIO_WC_T_APPLY: u32 = 721;
pub const EE_SCENARIO_WC_COMPLEX_POWER: u32 = 722;
pub const EE_SCENARIO_WC_SET_POWER: u32 = 723;
pub const EE_SCENARIO_WC_PENDING: u32 = 724;
pub const EE_SCENARIO_WC_FLUSH: u32 = 725;
pub const EE_SCENARIO_WC_PLACE_ON_GROUND: u32 = 726;
pub const EE_SCENARIO_WC_ATTRIBUTE_CHECK: u32 = 727;
pub const EE_SCENARIO_WC_CHANGE_UNIT: u32 = 728;
pub const EE_SCENARIO_WC_PUSH_CREATED: u32 = 729;
pub const EE_SCENARIO_WC_SOUND: u32 = 730;
pub const EE_SCENARIO_WC_SOUND_LOCAL: u32 = 731;
pub const EE_SCENARIO_WC_SOUND_PLAY: u32 = 732;

/// The categories (ee/EEUnitClassIDs.h): an airplane, a planet spaceship; and those Set Favorite Units never takes
/// as the land unit (`SMENotLand`).
const CATEGORY_AIRPLANE: i64 = 3;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
const NOT_LAND: [i64; 22] = [0, 1, 2, 3, 4, 9, 10, 11, 12, 14, 17, 18, 19, 20, 21, 22, 24, 25, 31, 32, 33, 34];
/// `kActionCircle`
const ACTION_CIRCLE: i64 = 1008;
/// The tech node kinds: a wall (`mKind == 6`: the wall planner places it), a wonder (4).
const NODE_WALL: i64 = 6;
const NODE_WONDER: i64 = 4;

/// The ids the C++ casts to its goal kinds (`kGoalLoadGarrison`, `kGoalLoadTransport`).
const GOAL_LOAD_GARRISON: i64 = 12;
const GOAL_LOAD_TRANSPORT: i64 = 7;
/// The action numbers the sequences end with (`kAction*`, ee/EEUnitAction.h).
const ACTION_TAKEOFF: i64 = 1000;
const ACTION_ATTACK_ENEMY_UNIT: i64 = 2001;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const ACTION_DROPOFF_RESOURCE: i64 = 4011;
const ACTION_IDLE: i64 = 5000;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
const ACTION_MOVE_TO_NEXT_PATROL_POINT: i64 = 8013;
const ACTION_FIND_UNEXPLORED_AREA: i64 = 11002;
const ACTION_TURN_TO_FACE_HEALEE: i64 = 16002;
/// The graphic effects' kinds (ee/EEEffectKindIDs.h).
const EFFECT_SMOKE_MUZZLE_BLAST: i64 = 0x14;
const EFFECT_WINTER_BREATH: i64 = 0x16;
const EFFECT_TRACERS: i64 = 0x21;
const EFFECT_SHELLS: i64 = 0x26;
const EFFECT_RAIN: i64 = 0x2a;
const EFFECT_MARCHING_LINE: i64 = 0x2d;
const EFFECT_SNOW: i64 = 0x3d;
/// `kSMEOneDegree` (0x446090, read by the facing test).
const ONE_DEGREE: f32 = 0.017_453_292;
/// The calamity target kinds (`EE_CALAMITY_TARGET_UNIT`, `_LOCATION`).
const CALAMITY_TARGET_UNIT: i64 = crate::tech::calamity_class::EE_CALAMITY_TARGET_UNIT as i64;
const CALAMITY_TARGET_LOCATION: i64 = crate::tech::calamity_class::EE_CALAMITY_TARGET_LOCATION as i64;

fn bits(v: f32) -> i64 {
    v.to_bits() as i64
}

fn float(v: i64) -> f32 {
    f32::from_bits(v as u32)
}

fn pack(x: i64, y: i64) -> i64 {
    (x as u32 as i64) | ((y as u32 as u64) << 32) as i64
}

/// The world operation `op` with `a` composed of the C++ statements `cpp` runs, or `None` when the C++ answers it.
pub fn compose(cpp: &mut dyn FnMut(u32, [i64; 6]) -> i64, op: u32, a: [i64; 6]) -> Option<i64> {
    let mut w = |op: u32, args: &[i64]| -> i64 {
        let mut v = [0i64; 6];
        v[..args.len()].copy_from_slice(args);
        cpp(op, v)
    };
    // SMEAction: the action, then the class's ActionChanged
    let action = |w: &mut dyn FnMut(u32, &[i64]) -> i64, unit: i64, act: i64| {
        w(EE_SCENARIO_WX_START_ACTION, &[unit, act]);
        w(EE_SCENARIO_WX_ACTION_CHANGED, &[unit]);
    };
    // SMEGroupGoal: the group goal set, the action, the plan
    let group_goal = |w: &mut dyn FnMut(u32, &[i64]) -> i64, act: i64| {
        w(EE_SCENARIO_WX_GROUP_SET_GOAL, &[]);
        w(EE_SCENARIO_WX_GROUP_ACTION, &[act]);
        w(EE_SCENARIO_WX_GROUP_PLAN, &[]);
    };
    let unit = |w: &mut dyn FnMut(u32, &[i64]) -> i64, id: i64, field: i64| w(EE_SCENARIO_WORLD_UNIT, &[id, field]);
    Some(match op {
        EE_SCENARIO_WORLD_ACTION => {
            action(&mut w, a[0], a[1]);
            0
        }
        EE_SCENARIO_WORLD_TASK_PREPARE => {
            w(EE_SCENARIO_WX_DETACH, &[a[0]]);
            w(EE_SCENARIO_WORLD_SET_DISABLED, &[a[0], 0]);
            w(EE_SCENARIO_WORLD_SET_COMMANDABLE, &[a[0], 1]);
            let deletable = unit(&mut w, a[0], EE_SCENARIO_UV_CLASS_DELETABLE);
            w(EE_SCENARIO_WORLD_SET_DELETABLE, &[a[0], deletable]);
            0
        }
        EE_SCENARIO_WORLD_GROUP_NEW => {
            w(EE_SCENARIO_WX_GROUP_CREATE, &[a[0]]);
            w(EE_SCENARIO_WX_GROUP_PEND, &[a[0]]);
            0
        }
        EE_SCENARIO_WORLD_GROUP_ADD_TASKED => {
            if w(EE_SCENARIO_WX_HAS_COMPLEX, &[a[0]]) != 0
                && unit(&mut w, a[0], EE_SCENARIO_UV_AIR) != 0
                && unit(&mut w, a[0], EE_SCENARIO_UV_COMMAND) == ACTION_IDLE
            {
                w(EE_SCENARIO_WX_REFUEL, &[a[0]]);
                w(EE_SCENARIO_WX_REARM, &[a[0]]);
            }
            w(EE_SCENARIO_WORLD_GROUP_ADD, &[a[0]]);
            0
        }
        EE_SCENARIO_WORLD_GROUP_CAN_GATHER => {
            // both searches first, then the gatherer's test and the carrier's
            w(EE_SCENARIO_WX_GROUP_FIND, &[]);
            w(EE_SCENARIO_WX_GROUP_FIND_NAVAL, &[]);
            let can = |w: &mut dyn FnMut(u32, &[i64]) -> i64, slot: i64| {
                w(EE_SCENARIO_WX_FOUND, &[slot]) != 0 && w(EE_SCENARIO_WX_FOUND_CAN_GATHER, &[slot, a[0]]) != 0
            };
            (can(&mut w, 0) || can(&mut w, 1)) as i64
        }
        EE_SCENARIO_WORLD_GROUP_GATHER | EE_SCENARIO_WORLD_GROUP_DROPOFF => {
            w(EE_SCENARIO_WX_GROUP_SPREAD_AT_UNIT, &[a[0]]);
            w(EE_SCENARIO_WX_GROUP_CLEAR_GOAL, &[]);
            let gather = op == EE_SCENARIO_WORLD_GROUP_GATHER;
            w(if gather { EE_SCENARIO_WX_GOAL_GATHER } else { EE_SCENARIO_WX_GOAL_DROPOFF }, &[a[0]]);
            group_goal(&mut w, if gather { ACTION_GATHER_RESOURCE } else { ACTION_DROPOFF_RESOURCE });
            0
        }
        EE_SCENARIO_WORLD_GROUP_ATTACK => {
            w(EE_SCENARIO_WX_GROUP_SPREAD_AROUND, &[a[0]]);
            w(EE_SCENARIO_WX_GROUP_CLEAR_GOAL, &[]);
            w(EE_SCENARIO_WX_GOAL_ATTACK, &[a[0]]);
            group_goal(&mut w, ACTION_ATTACK_ENEMY_UNIT);
            0
        }
        EE_SCENARIO_WORLD_HEAL => {
            w(EE_SCENARIO_WX_UGOAL_HEAL, &[a[1]]);
            w(EE_SCENARIO_WORLD_CLEAR_QUEUE, &[a[0], 0]);
            w(EE_SCENARIO_WX_UNIT_SET_GOAL, &[a[0]]);
            action(&mut w, a[0], ACTION_TURN_TO_FACE_HEALEE);
            0
        }
        EE_SCENARIO_WORLD_LOAD_GOAL => {
            let kind = if a[0] == 0 { GOAL_LOAD_GARRISON } else { GOAL_LOAD_TRANSPORT };
            if unit(&mut w, a[1], EE_SCENARIO_UV_HAS_GOAL) != 0 && w(EE_SCENARIO_WX_GOAL_KIND, &[a[1]]) == kind {
                let cast =
                    if a[0] == 0 { EE_SCENARIO_WX_GOAL_CAST_GARRISON } else { EE_SCENARIO_WX_GOAL_CAST_TRANSPORT };
                w(cast, &[a[1]]);
                w(EE_SCENARIO_WX_GOAL_ADD_LOAD, &[]);
                return Some(0);
            }
            w(EE_SCENARIO_WX_GOAL_NEW_LOAD, &[kind, a[1]]);
            1
        }
        EE_SCENARIO_WORLD_SET_GOAL | EE_SCENARIO_WORLD_SET_GOAL_CLONE => {
            w(EE_SCENARIO_WORLD_CLEAR_GOALS, &[a[0], 0]);
            let set = if op == EE_SCENARIO_WORLD_SET_GOAL {
                EE_SCENARIO_WX_SET_RUN_GOAL
            } else {
                EE_SCENARIO_WX_SET_RUN_GOAL_CLONE
            };
            w(set, &[a[0]]);
            action(&mut w, a[0], a[1]);
            0
        }
        EE_SCENARIO_WORLD_LOAD_FILTER => {
            w(EE_SCENARIO_WORLD_LOAD_CLEAR, &[]);
            let n = w(EE_SCENARIO_WORLD_GROUP_COUNT, &[]) as u32;
            for u in 0..n {
                if w(EE_SCENARIO_WX_GROUP_UNIT_IS_NOT, &[u as i64, a[0]]) != 0 {
                    w(EE_SCENARIO_WX_LOAD_PUSH_GROUP_UNIT, &[u as i64]);
                }
            }
            w(EE_SCENARIO_WX_FILTER_LOADABLE, &[a[0]]);
            0
        }
        EE_SCENARIO_WORLD_REJECTED_MOVE => {
            w(EE_SCENARIO_WX_GROUP_FINISH, &[]);
            let n = w(EE_SCENARIO_WORLD_REJECTED_COUNT, &[]) as u32;
            for i in 0..n {
                w(EE_SCENARIO_WX_GROUP_ADD_REJECTED, &[i as i64]);
            }
            w(EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE, &[]);
            w(EE_SCENARIO_WX_GOAL_TILE, &[a[0], a[1]]);
            group_goal(&mut w, ACTION_PREPARE_TO_MOVE);
            0
        }
        EE_SCENARIO_WORLD_REPAIR => {
            w(EE_SCENARIO_WX_ORDER_REPAIR, &[a[0], a[1]]);
            action(&mut w, a[0], ACTION_IDLE);
            0
        }
        EE_SCENARIO_WORLD_ACKNOWLEDGE => {
            if w(EE_SCENARIO_WORLD_GROUP_FIRST, &[]) >= 0 {
                w(EE_SCENARIO_WX_ACKNOWLEDGE, &[a[0]]);
            }
            0
        }
        EE_SCENARIO_WORLD_GROUP_PATROL_POINT => {
            w(EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE, &[]);
            w(EE_SCENARIO_WX_GOAL_PATROL_POINT, &[a[0]]);
            w(EE_SCENARIO_WX_GROUP_CLEAR_GOAL, &[]);
            group_goal(&mut w, ACTION_MOVE_TO_NEXT_PATROL_POINT);
            0
        }
        EE_SCENARIO_WORLD_GROUP_PATROL_TO => {
            w(EE_SCENARIO_WX_GOAL_PATROL_NEW, &[]);
            if w(EE_SCENARIO_WORLD_GROUP_FIRST, &[]) >= 0
                && (a[3] == 0
                    || (w(EE_SCENARIO_WX_FIRST_TILE_X, &[]) != -10000 && w(EE_SCENARIO_WX_FIRST_TILE_Y, &[]) != -10000))
            {
                w(EE_SCENARIO_WX_PATROL_ADD_FIRST, &[a[0]]);
                w(EE_SCENARIO_WX_PATROL_ADD, &[a[0], a[1], a[2]]);
                w(EE_SCENARIO_WX_FIRST_ADD_PATROL, &[]);
            }
            w(EE_SCENARIO_WX_GROUP_CLEAR_GOAL, &[]);
            group_goal(&mut w, ACTION_MOVE_TO_NEXT_PATROL_POINT);
            0
        }
        EE_SCENARIO_WORLD_AIR_QUEUE => {
            w(EE_SCENARIO_WORLD_CLEAR_QUEUE, &[a[0], 0]);
            w(EE_SCENARIO_WORLD_CLEAR_QUEUE, &[a[0], 1]);
            w(EE_SCENARIO_WX_QUEUE_LOCATION, &[a[0], a[1], a[2], a[3], 0]);
            w(EE_SCENARIO_WX_QUEUE_LOCATION, &[a[0], a[1], a[2], a[3], 1]);
            w(EE_SCENARIO_WX_QUEUE_ADD_TIME, &[a[0], 1, 1]);
            0
        }
        EE_SCENARIO_WORLD_LOCATION_GOAL => {
            w(EE_SCENARIO_WX_UGOAL_LOCATION, &[a[1], a[2], a[3]]);
            w(EE_SCENARIO_WORLD_CLEAR_GOALS, &[a[0], 0]);
            w(EE_SCENARIO_WX_UNIT_SET_GOAL, &[a[0]]);
            if a[4] == 0 || unit(&mut w, a[0], EE_SCENARIO_UV_CONTAINER) < 0 {
                action(&mut w, a[0], ACTION_TAKEOFF);
            }
            0
        }
        EE_SCENARIO_WORLD_GROUP_MOVE => {
            w(EE_SCENARIO_WX_GROUP_CLEAR_GOAL, &[]);
            w(EE_SCENARIO_WX_GOAL_MOVE, &[a[0], a[1], a[2]]);
            group_goal(&mut w, ACTION_PREPARE_TO_MOVE);
            0
        }
        EE_SCENARIO_WORLD_CAL_NODES => {
            w(EE_SCENARIO_WX_CAL_CLEAR, &[]);
            w(EE_SCENARIO_WX_NODE_IDS, &[a[0]]);
            let n = w(EE_SCENARIO_WX_NODE_COUNT, &[]);
            for i in 0..n {
                if w(EE_SCENARIO_WX_NODE_CALAMITY, &[a[0], i]) as u32 == a[1] as u32
                    && w(EE_SCENARIO_WX_NODE_AVAILABLE, &[a[0], i]) != 0
                {
                    w(EE_SCENARIO_WX_CAL_PUSH, &[i]);
                }
            }
            0
        }
        EE_SCENARIO_WORLD_CAL_VALID_TARGET | EE_SCENARIO_WORLD_CAL_VALID_LOCATION => {
            let target = op == EE_SCENARIO_WORLD_CAL_VALID_TARGET;
            let kind = if target { CALAMITY_TARGET_UNIT } else { CALAMITY_TARGET_LOCATION };
            (w(EE_SCENARIO_WX_CAL_HAS, &[]) != 0
                && w(EE_SCENARIO_WX_CAL_KIND, &[]) == kind
                && if target {
                    w(EE_SCENARIO_WX_CAL_IS_VALID_TARGET, &[a[0], a[1]]) != 0
                } else {
                    w(EE_SCENARIO_WX_CAL_IS_VALID_LOCATION, &[a[0], a[1], a[2]]) != 0
                }) as i64
        }
        EE_SCENARIO_WORLD_CAST => {
            let n = w(EE_SCENARIO_WX_CAL_COUNT, &[]);
            for i in 0..n {
                if w(EE_SCENARIO_WX_CAL_CONTAINS, &[a[0], a[3], i]) == 0 {
                    continue;
                }
                w(EE_SCENARIO_WX_UGOAL_CAST, &[a[0], a[1], a[2], a[4], i]);
                w(EE_SCENARIO_WORLD_CLEAR_GOALS, &[a[0], 0]);
                w(EE_SCENARIO_WX_UNIT_SET_GOAL, &[a[0]]);
                action(&mut w, a[0], ACTION_PREPARE_TO_MOVE);
                break;
            }
            0
        }
        EE_SCENARIO_WORLD_UNLOAD => {
            w(EE_SCENARIO_WX_UGOAL_UNLOAD, &[a[0], a[1], a[2]]);
            if unit(&mut w, a[0], EE_SCENARIO_UV_TRANSPORT) != 0
                && unit(&mut w, a[0], EE_SCENARIO_UV_CARGO) > 0
                && w(EE_SCENARIO_WX_UGOAL_CAN_UNLOAD, &[]) != 0
            {
                w(EE_SCENARIO_WORLD_CLEAR_GOALS, &[a[0], 0]);
                w(EE_SCENARIO_WX_UNIT_SET_GOAL, &[a[0]]);
            } else {
                w(EE_SCENARIO_WX_UGOAL_DELETE, &[]);
                w(EE_SCENARIO_WORLD_CLEAR_GOALS, &[a[0], 0]);
                w(EE_SCENARIO_WX_UGOAL_POINT, &[a[1], a[2]]);
                w(EE_SCENARIO_WX_UNIT_SET_GOAL, &[a[0]]);
            }
            action(&mut w, a[0], ACTION_PREPARE_TO_MOVE);
            0
        }
        EE_SCENARIO_WORLD_AREA_LOCATION => w(EE_SCENARIO_WX_PLACE_SET, &[a[0], a[1]]),
        EE_SCENARIO_WORLD_AREA_STATE => {
            if w(EE_SCENARIO_WX_AREA_EXISTS, &[a[0]]) == 0 {
                return Some(-1);
            }
            let unset = w(EE_SCENARIO_WX_AREA_UNSET, &[a[0]]) != 0;
            let off = w(EE_SCENARIO_WX_AREA_OFF_MAP, &[a[0]]) != 0;
            unset as i64 | ((off as i64) << 1)
        }
        EE_SCENARIO_WORLD_OBJECT_EVALUATE => {
            if w(EE_SCENARIO_WX_OBJECT_EXISTS, &[a[0]]) == 0 {
                return Some(-1);
            }
            w(EE_SCENARIO_WX_OBJECT_EVALUATE, &[a[0]])
        }
        EE_SCENARIO_WORLD_TILE_HEIGHT => {
            if w(EE_SCENARIO_WX_TILE_EXISTS, &[a[0], a[1]]) == 0 {
                return Some(-1);
            }
            w(EE_SCENARIO_WX_TILE_HEIGHT, &[a[0], a[1]])
        }
        EE_SCENARIO_WORLD_KILL => {
            w(EE_SCENARIO_WX_HOLD, &[a[0]]);
            w(EE_SCENARIO_WX_HELD_KILL, &[a[1]]);
            if w(EE_SCENARIO_WX_HELD_ON_NODE, &[]) == 0 {
                w(EE_SCENARIO_WX_HELD_PLAYER_KILL, &[]);
            }
            0
        }
        EE_SCENARIO_WORLD_FACE_TEST => {
            let y = w(EE_SCENARIO_WX_LOCATION_AT, &[a[0], 1, 0]);
            let x = w(EE_SCENARIO_WX_LOCATION_AT, &[a[0], 0, 0]);
            let heading = um_arctan2(float(y), float(x));
            ((float(a[1]) - heading).abs() >= ONE_DEGREE) as i64
        }
        EE_SCENARIO_WORLD_FACE_SET => {
            w(EE_SCENARIO_WX_FACE_FROM_UNIT, &[a[0]]);
            w(EE_SCENARIO_WX_FACE_ORIENT, &[a[1], bits(0.0), bits(0.0)]);
            w(EE_SCENARIO_WX_FACE_APPLY, &[a[0]]);
            w(EE_SCENARIO_WX_FACE_POINT, &[]);
            w(EE_SCENARIO_WX_PLACE_ON_GROUND, &[a[0]]);
            0
        }
        EE_SCENARIO_WORLD_FACE_GOAL => {
            w(EE_SCENARIO_WORLD_CLEAR_GOALS, &[a[0], 0]);
            w(EE_SCENARIO_WX_UGOAL_FACE, &[a[0], a[1]]);
            w(EE_SCENARIO_WX_UNIT_SET_GOAL, &[a[0]]);
            action(&mut w, a[0], ACTION_PREPARE_TO_MOVE);
            0
        }
        EE_SCENARIO_WORLD_REFRESH_PANEL => {
            if w(EE_SCENARIO_WX_MAIN_FORM, &[]) != 0 {
                w(EE_SCENARIO_WX_REFRESH_GROUP, &[]);
            }
            0
        }
        EE_SCENARIO_WORLD_STANCE_EXPLORE => {
            stance_group(&mut w, a[0]);
            w(EE_SCENARIO_WX_GROUP_CLEAR_GOAL, &[]);
            w(EE_SCENARIO_WX_GOAL_EXPLORE, &[]);
            group_goal(&mut w, ACTION_FIND_UNEXPLORED_AREA);
            0
        }
        EE_SCENARIO_WORLD_LOCK => {
            // a single-piece unit (a non-gate, or a gate created by a Create effect) has no second or fourth part
            if (w(EE_SCENARIO_WX_LOCKED, &[a[0]]) != 0) != (a[1] != 0) {
                w(EE_SCENARIO_WX_SET_LOCKED, &[a[0], a[1]]);
            }
            for part in [EE_SCENARIO_WX_GATE_SECOND, EE_SCENARIO_WX_GATE_FOURTH] {
                if w(part, &[a[0]]) != 0 && (w(EE_SCENARIO_WX_PART_LOCKED, &[]) != 0) != (a[1] != 0) {
                    w(EE_SCENARIO_WX_PART_SET_LOCKED, &[a[1]]);
                }
            }
            0
        }
        EE_SCENARIO_WORLD_STANCE_TOWN_PATROL => {
            if w(EE_SCENARIO_WX_TOWN_FIND, &[a[0]]) == 0
                || w(EE_SCENARIO_WX_TOWN_PATROL, &[]) == 0
                || w(EE_SCENARIO_WX_PATROL_NEAREST, &[a[0]]) == 0
            {
                return Some(0);
            }
            stance_group(&mut w, a[0]);
            w(EE_SCENARIO_WX_GOAL_TOWN_PATROL, &[]);
            w(EE_SCENARIO_WX_GROUP_CLEAR_GOAL, &[]);
            group_goal(&mut w, ACTION_MOVE_TO_NEXT_PATROL_POINT);
            0
        }
        EE_SCENARIO_WORLD_BEHAVIOR => {
            if w(EE_SCENARIO_WX_BEHAVIOR_IS, &[a[0], a[1]]) == 0 {
                w(EE_SCENARIO_WX_SET_BEHAVIOR, &[a[0], a[1]]);
            }
            0
        }
        EE_SCENARIO_WORLD_PLAYER_RESULT => {
            w(EE_SCENARIO_WX_SET_GAME_RESULT, &[a[0], if a[1] != 0 { 1 } else { 2 }]);
            w(EE_SCENARIO_WX_REMOVE_PLAYER, &[(a[1] != 0) as i64, a[0]]);
            0
        }
        EE_SCENARIO_WORLD_FADE => w(if a[0] != 0 { EE_SCENARIO_WX_FADE_IN } else { EE_SCENARIO_WX_FADE_OUT }, &[]),
        EE_SCENARIO_WORLD_TECH_REMOVE => {
            if w(EE_SCENARIO_WX_TECH_MAKES_UNIT, &[a[0]]) != 0 && w(EE_SCENARIO_WX_TECH_CLASS_EXISTS, &[a[0]]) != 0 {
                w(EE_SCENARIO_WX_TECH_REPLACE_QUEUED, &[a[0]]);
            }
            w(EE_SCENARIO_WX_TECH_OBSOLETE, &[a[0]]);
            0
        }
        EE_SCENARIO_WORLD_SEES_ALL => {
            w(EE_SCENARIO_WX_SET_SEES_ALL, &[a[0], a[1]]);
            if w(EE_SCENARIO_WX_PLAYER_IS_CURRENT, &[a[0]]) != 0 {
                w(EE_SCENARIO_WX_MINIMAP_REDRAW, &[]);
            }
            0
        }
        EE_SCENARIO_WORLD_CIV_POINTS => {
            w(EE_SCENARIO_WX_CIV_ADD, &[a[0], a[1]]);
            w(EE_SCENARIO_WX_CIV_BLINK, &[]);
            trumpets(&mut w);
            0
        }
        EE_SCENARIO_WORLD_FLARE_UNIT_TILE => {
            if w(EE_SCENARIO_WX_UNIT_HAS_COMPLEX, &[a[0]]) != 0 && w(EE_SCENARIO_WX_UNIT_IN_CONTAINER, &[a[0]]) == 0 {
                if w(EE_SCENARIO_WX_UNIT_ON_TILE, &[a[0]]) == 0 {
                    return Some(-1);
                }
                let (x, y) = (w(EE_SCENARIO_WX_UNIT_TILE_X, &[a[0]]), w(EE_SCENARIO_WX_UNIT_TILE_Y, &[a[0]]));
                return Some(pack(x, y));
            }
            if w(EE_SCENARIO_WX_CONTAINER_ON_TILE, &[a[0]]) == 0 {
                return Some(-1);
            }
            let (x, y) = (w(EE_SCENARIO_WX_CONTAINER_TILE_X, &[a[0]]), w(EE_SCENARIO_WX_CONTAINER_TILE_Y, &[a[0]]));
            pack(x, y)
        }
        EE_SCENARIO_WORLD_TRIGGER_SET => {
            let set = match a[1] {
                0 => EE_SCENARIO_WX_TRIGGER_START,
                1 => EE_SCENARIO_WX_TRIGGER_ON,
                _ => EE_SCENARIO_WX_TRIGGER_FIRED,
            };
            w(set, &[a[0], a[2]]);
            0
        }
        EE_SCENARIO_WORLD_CAL_TILE_UNITS => {
            if w(EE_SCENARIO_WX_MAP_TILE, &[a[1], a[2]]) == 0 {
                return Some(0);
            }
            let mut node = w(EE_SCENARIO_WX_NODE_FIRST, &[]) != 0;
            while node {
                if w(EE_SCENARIO_WX_NODE_NO_UNIT, &[]) == 0
                    && w(EE_SCENARIO_WX_NODE_SEEN, &[]) == 0
                    && w(EE_SCENARIO_WX_NODE_REFUSED, &[]) == 0
                {
                    w(EE_SCENARIO_WX_NODE_CREATE, &[a[0]]);
                    w(EE_SCENARIO_WX_NODE_MARK, &[]);
                }
                node = w(EE_SCENARIO_WX_NODE_NEXT, &[]) != 0;
            }
            0
        }
        EE_SCENARIO_WORLD_GFX_BEGIN => {
            if w(EE_SCENARIO_WX_GFX_GET, &[a[0]]) == 0 {
                return Some(-1);
            }
            w(EE_SCENARIO_WX_GFX_LOAD, &[]);
            let kind = w(EE_SCENARIO_WX_GFX_KIND, &[]);
            let weather = matches!(kind, EFFECT_RAIN | EFFECT_SNOW);
            let pair =
                matches!(kind, EFFECT_TRACERS | EFFECT_SHELLS | EFFECT_WINTER_BREATH | EFFECT_SMOKE_MUZZLE_BLAST);
            (if weather { EE_SCENARIO_GFX_WEATHER } else { 0 })
                | (if pair { EE_SCENARIO_GFX_PAIR } else { 0 })
                | (if kind == EFFECT_MARCHING_LINE { EE_SCENARIO_GFX_MARCHING } else { 0 })
        }
        EE_SCENARIO_WORLD_GFX_WEATHER => {
            w(EE_SCENARIO_WX_TILES_CLEAR, &[]);
            let xy = a[0] as usize as *const i32;
            for i in 0..a[1].max(0) as usize {
                // SAFETY: the caller's contract (effect.rs): `a[0]` points to `a[1]` x, y pairs for the call
                let (x, y) = unsafe { (*xy.add(2 * i), *xy.add(2 * i + 1)) };
                w(EE_SCENARIO_WX_TILES_PUSH, &[x as i64, y as i64]);
            }
            w(EE_SCENARIO_WX_WEATHER_CREATE, &[]);
            0
        }
        EE_SCENARIO_WORLD_GFX_PLACED => {
            let x = (a[0] as i32) as f32 + 0.5;
            let y = (a[1] as i32) as f32 + 0.5;
            let mut z = float(w(EE_SCENARIO_WX_TILE_GET_HEIGHT, &[a[0], a[1]]));
            if w(EE_SCENARIO_WX_TILE_WATER, &[a[0], a[1]]) != 0 {
                z = 0.0;
            }
            w(EE_SCENARIO_WX_POINT_SET, &[bits(x), bits(y), bits(z)]);
            w(EE_SCENARIO_WX_PLACED_CREATE, &[]);
            0
        }
        EE_SCENARIO_WORLD_WEATHER => {
            if w(EE_SCENARIO_WX_ENV_EXISTS, &[]) != 0 {
                w(EE_SCENARIO_WX_ENV_SET, &[a[0]]);
            }
            0
        }
        EE_SCENARIO_WORLD_AI_TOGGLE => w(EE_SCENARIO_WX_AI_TOGGLE + a[1] as u32, &[a[0], a[2]]),
        EE_SCENARIO_WORLD_AI_FAVORITE => {
            let set = match a[1] {
                0 => EE_SCENARIO_WX_AI_FAVORITE_AIR,
                1 => EE_SCENARIO_WX_AI_FAVORITE_LAND,
                _ => EE_SCENARIO_WX_AI_FAVORITE_SEA,
            };
            w(set, &[a[0], a[2]])
        }
        EE_SCENARIO_WORLD_CAMERA_ZOOM => {
            w(EE_SCENARIO_WORLD_CAMERA_MODE, &[]);
            w(EE_SCENARIO_WX_CAMERA_SET_ZOOM, &[a[0]]);
            0
        }
        EE_SCENARIO_WORLD_AREA_TAKE_OFF_MAP => {
            take_off_map(&mut w, a[0]);
            0
        }
        EE_SCENARIO_WORLD_AREA_PLACE => {
            place_in_area(&mut w, a[0]);
            0
        }
        EE_SCENARIO_WORLD_TECH_ADD => {
            tech_add(&mut w, a[0]);
            0
        }
        EE_SCENARIO_WORLD_CLASS => class_value(&mut w, a[0]),
        _ => return None,
    })
}

/// `SMETakeOffMap`: Task-Area with an unset area takes unit `id` off the map (its 2D position kept, out of its
/// airport, out of the selection).
fn take_off_map(w: &mut dyn FnMut(u32, &[i64]) -> i64, id: i64) {
    let airport_out = |w: &mut dyn FnMut(u32, &[i64]) -> i64| {
        if w(EE_SCENARIO_WX_AIRPORT, &[id]) != 0 {
            w(EE_SCENARIO_WX_AIRPORT_REMOVE, &[id]);
        }
        if w(EE_SCENARIO_WX_SELECTED_LOCALLY, &[id]) != 0 {
            w(EE_SCENARIO_WX_POST_424, &[]);
        }
    };
    if w(EE_SCENARIO_WX_LAND_BUILDING, &[id]) == 0 {
        // (the original tests IsLandBuilding again here: its linked parts' branch is never taken)
        w(EE_SCENARIO_WX_LAND_BUILDING, &[id]);
        w(EE_SCENARIO_WX_T_FROM_UNIT, &[id]);
        w(EE_SCENARIO_WX_SET_2D_FROM_T, &[id]);
        airport_out(w);
        w(EE_SCENARIO_WX_TAKE_OFF_MAP, &[id]);
        if w(EE_SCENARIO_WX_OFF_MAP_INFO, &[id]) != 0 {
            w(EE_SCENARIO_WX_OFF_MAP_REASON, &[id]);
        }
    } else {
        w(EE_SCENARIO_WX_T_FROM_UNIT, &[id]);
        w(EE_SCENARIO_WX_LAST_MAP_POSITION, &[id]);
        w(EE_SCENARIO_WX_SET_2D_FROM_T, &[id]);
    }
    airport_out(w);
}

/// `SMEPlaceInArea`: Task-Area's off-map unit `id` placed in the task area (the object's area check pointed at it
/// for FindPlacement), on the ground at the location's tile, turned with the terrain, idle or circling.
fn place_in_area(w: &mut dyn FnMut(u32, &[i64]) -> i64, id: i64) {
    let saved = [
        w(EE_SCENARIO_WX_OBJ_IN_AREA_CHECK, &[]),
        w(EE_SCENARIO_WX_OBJ_AREA, &[]),
        w(EE_SCENARIO_WX_OBJ_NEAR_CHECK, &[]),
    ];
    w(EE_SCENARIO_WX_OBJ_SET_IN_AREA_CHECK, &[1]);
    let area = w(EE_SCENARIO_WX_EFFECT_TASK_AREA, &[]);
    w(EE_SCENARIO_WX_OBJ_SET_AREA, &[area]);
    w(EE_SCENARIO_WX_OBJ_SET_NEAR_CHECK, &[0]);
    if w(EE_SCENARIO_WX_TASK_AREA_OFF_MAP, &[]) != 0 || w(EE_SCENARIO_WX_TASK_FIND_PLACEMENT, &[id]) != 0 {
        w(EE_SCENARIO_WX_T_FROM_UNIT, &[id]);
        if w(EE_SCENARIO_WX_TASK_AREA_OFF_MAP, &[]) != 0 {
            let x = w(EE_SCENARIO_WX_U0DC_X, &[id]);
            w(EE_SCENARIO_WX_T_SET_X, &[x]);
            let y = w(EE_SCENARIO_WX_U0DC_Y, &[id]);
            w(EE_SCENARIO_WX_T_SET_Y, &[y]);
        } else if float(w(EE_SCENARIO_WX_PLACE_X, &[])) != -10000.0 {
            let x = float(w(EE_SCENARIO_WX_PLACE_X, &[]));
            w(EE_SCENARIO_WX_T_SET_X, &[bits(x + 0.5)]);
            let y = float(w(EE_SCENARIO_WX_PLACE_Y, &[]));
            w(EE_SCENARIO_WX_T_SET_Y, &[bits(y + 0.5)]);
            if w(EE_SCENARIO_WX_PLACE_TILE, &[]) != 0 {
                if w(EE_SCENARIO_WX_UNIT_AIR, &[id]) == 0 {
                    let height = w(EE_SCENARIO_WX_PLACE_TILE_HEIGHT, &[]);
                    w(EE_SCENARIO_WX_T_SET_Z, &[height]);
                } else if float(w(EE_SCENARIO_WX_UNIT_Z, &[id])) < float(w(EE_SCENARIO_WX_PLACE_TILE_HEIGHT, &[])) {
                    let cruise = w(EE_SCENARIO_WX_CRUISE, &[id]);
                    w(EE_SCENARIO_WX_T_SET_Z, &[cruise]);
                }
            }
        } else {
            w(EE_SCENARIO_WX_T_SET_X, &[bits(-10000.0)]);
            w(EE_SCENARIO_WX_T_SET_Y, &[bits(-10000.0)]);
        }
        if float(w(EE_SCENARIO_WX_T_X, &[])) != -10000.0 {
            w(EE_SCENARIO_WX_GROUND_IDENTITY, &[]);
            w(EE_SCENARIO_WX_GROUND_POSITION, &[]);
            if w(EE_SCENARIO_WX_PITCHES, &[id]) != 0 {
                w(EE_SCENARIO_WX_TERRAIN_PITCH, &[]);
            }
            if w(EE_SCENARIO_WX_ROLLS, &[id]) != 0 {
                w(EE_SCENARIO_WX_TERRAIN_ROLL, &[]);
            }
            w(EE_SCENARIO_WX_T_ORIENT_HEADING, &[id]);
            if w(EE_SCENARIO_WX_CONTAINER_NULL, &[id]) != 0 {
                w(EE_SCENARIO_WX_T_APPLY, &[id]);
            } else {
                w(EE_SCENARIO_WX_UNLOAD_CARGO, &[id]);
            }
            let size = w(EE_SCENARIO_WX_TILE_SIZE, &[id]) as c_long;
            if size > 1 {
                let offset = size as f32 * 0.5;
                let tx = float(w(EE_SCENARIO_WX_T_X, &[]));
                w(EE_SCENARIO_WX_T_SET_X, &[bits(tx + offset)]);
                let ty = float(w(EE_SCENARIO_WX_T_Y, &[]));
                w(EE_SCENARIO_WX_T_SET_Y, &[bits(ty + offset)]);
                w(EE_SCENARIO_WX_T_SET_LOCATION, &[id]);
            }
            if w(EE_SCENARIO_WX_AIRPORT, &[id]) != 0 {
                w(EE_SCENARIO_WX_AIRPORT_ADD, &[id]);
            }
            let category = w(EE_SCENARIO_WX_CATEGORY, &[id]);
            let circles = (category == CATEGORY_AIRPLANE || category == CATEGORY_PLANET_SPACESHIP)
                && (w(EE_SCENARIO_WX_FLIGHT_TIME, &[id]) != 0 || w(EE_SCENARIO_WX_HOME_BASE, &[id]) != 0);
            w(EE_SCENARIO_WX_START_ACTION, &[id, if circles { ACTION_CIRCLE } else { ACTION_IDLE }]);
            w(EE_SCENARIO_WX_ACTION_CHANGED, &[id]);
        }
    }
    w(EE_SCENARIO_WX_OBJ_SET_IN_AREA_CHECK, &[saved[0]]);
    w(EE_SCENARIO_WX_OBJ_SET_AREA, &[saved[1]]);
    w(EE_SCENARIO_WX_OBJ_SET_NEAR_CHECK, &[saved[2]]);
}

/// `SMETechAdd`: Modify Tech Tree's add for player `p`: an epoch's node made available (the range raised), any other
/// node researched past the epochs, its unit media loaded.
fn tech_add(w: &mut dyn FnMut(u32, &[i64]) -> i64, p: i64) {
    let tech = w(EE_SCENARIO_WX_EFFECT_TECH, &[]) as c_long;
    if tech < w(EE_SCENARIO_WX_EPOCH_COUNT, &[]) as c_long {
        if (w(EE_SCENARIO_WX_TREE_END_EPOCH, &[p]) as c_long) < tech {
            w(EE_SCENARIO_WX_TREE_RESET_RANGE, &[p]);
        }
        w(EE_SCENARIO_WX_TECH_MAKE_AVAILABLE, &[p]);
        return;
    }
    for op in [
        EE_SCENARIO_WX_TECH_MAKE_AVAILABLE,
        EE_SCENARIO_WX_TECH_EPOCH_ZERO,
        EE_SCENARIO_WX_TECH_OBSOLETE_EPOCH,
        EE_SCENARIO_WX_TECH_SCEN_OBSOLETED,
        EE_SCENARIO_WX_TECH_SET_OBSOLETE,
    ] {
        w(op, &[p]);
    }
    if w(EE_SCENARIO_WX_TECH_NEEDS_MEDIA, &[p]) != 0 {
        w(EE_SCENARIO_WX_TECH_LOAD_MEDIA, &[p]);
        w(EE_SCENARIO_WX_TECH_MEDIA_LOADED, &[p]);
    }
    if w(EE_SCENARIO_WX_TECH_KIND, &[p]) == NODE_WONDER {
        w(EE_SCENARIO_WX_TREE_WONDERS_ZERO, &[p]);
    }
    w(EE_SCENARIO_WX_TECH_RESEARCH, &[p]);
}

/// `SMEClassValue`: class `c`'s tests for Set Favorite Units (`EE_SCENARIO_CLASS_*`; 0 for no class).
fn class_value(w: &mut dyn FnMut(u32, &[i64]) -> i64, c: i64) -> i64 {
    if c < 0 || c as u64 >= w(EE_SCENARIO_WX_CLASS_COUNT, &[]) as u64 || w(EE_SCENARIO_WX_CLASS_EXISTS, &[c]) == 0 {
        return 0;
    }
    let mut v = EE_SCENARIO_CLASS_EXISTS;
    for (op, flag) in [
        (EE_SCENARIO_WX_CLASS_AIR, EE_SCENARIO_CLASS_AIR),
        (EE_SCENARIO_WX_CLASS_SEA, EE_SCENARIO_CLASS_SEA),
        (EE_SCENARIO_WX_CLASS_PROJECTILE, EE_SCENARIO_CLASS_PROJECTILE),
        (EE_SCENARIO_WX_CLASS_LAND_BUILDING, EE_SCENARIO_CLASS_LAND_BUILDING),
        (EE_SCENARIO_WX_CLASS_ATTACKS, EE_SCENARIO_CLASS_ATTACKS),
        (EE_SCENARIO_WX_CLASS_MOVES, EE_SCENARIO_CLASS_MOVES),
        (EE_SCENARIO_WX_CLASS_FLIES_OVER, EE_SCENARIO_CLASS_FLIES_OVER),
    ] {
        if w(op, &[c]) != 0 {
            v |= flag;
        }
    }
    if NOT_LAND.contains(&w(EE_SCENARIO_WX_CLASS_CATEGORY, &[c])) {
        v |= EE_SCENARIO_CLASS_NOT_LAND;
    }
    v
}

/// `EESMCreateUnit`: Create Units' unit for player `player` (create.rs `EE_SCENARIO_WRITE_CREATE_UNIT`): of class
/// `gUnitClassTable[unit_type]` when `create`, else of the object's unit `index`'s class; placed, finished and kept
/// when created.
pub fn create_unit(cpp: &mut dyn FnMut(u32, [i64; 6]) -> i64, player: i64, unit_type: i64, create: bool, index: i64) {
    let mut w = |op: u32, args: &[i64]| -> i64 {
        let mut v = [0i64; 6];
        v[..args.len()].copy_from_slice(args);
        cpp(op, v)
    };
    let class =
        if create { w(EE_SCENARIO_WC_CLASS, &[unit_type]) } else { w(EE_SCENARIO_WC_OBJECT_UNIT_CLASS, &[index]) };
    if w(EE_SCENARIO_WC_FIND_PLACEMENT, &[unit_type]) == 0 {
        return;
    }
    let unit = if w(EE_SCENARIO_WC_TECH_NODE, &[player, class]) != 0
        && w(EE_SCENARIO_WC_TECH_KIND, &[player, class]) == NODE_WALL
    {
        let x = float(w(EE_SCENARIO_WC_LOCATION_X, &[])) + 0.5;
        let y = float(w(EE_SCENARIO_WC_LOCATION_Y, &[])) + 0.5;
        w(EE_SCENARIO_WC_LOCATION_SET, &[bits(x), bits(y)]);
        w(EE_SCENARIO_WC_PLAN_WALL, &[class, player]);
        let unit = w(EE_SCENARIO_WC_PIECES_FIRST, &[]);
        w(EE_SCENARIO_WC_PIECES_CLEAR, &[]);
        unit
    } else {
        w(EE_SCENARIO_WC_IS_LAND_BUILDING, &[class]);
        w(EE_SCENARIO_WC_CREATE, &[class, player])
    };
    if w(EE_SCENARIO_WC_ON_NODE, &[unit]) != 0 {
        w(EE_SCENARIO_WC_SET_BUILT, &[unit]);
    }
    if w(EE_SCENARIO_WC_AIR, &[unit]) != 0 {
        w(EE_SCENARIO_WC_REFUEL, &[unit]);
        w(EE_SCENARIO_WC_REARM, &[unit]);
        // towards the map centre
        let size = w(EE_SCENARIO_WC_MAP_SIZE, &[]) as f32;
        let (x, y) = (float(w(EE_SCENARIO_WC_LOCATION_X, &[])), float(w(EE_SCENARIO_WC_LOCATION_Y, &[])));
        let mut direction = GeVector { x: size * 0.5 - x, y: size * 0.5 - y, z: 0.0 };
        direction.normalize();
        let heading = um_arctan2(-direction.x, direction.y);
        w(EE_SCENARIO_WC_T_FROM, &[unit]);
        w(EE_SCENARIO_WC_T_ORIENT, &[bits(heading)]);
        w(EE_SCENARIO_WC_T_APPLY, &[unit]);
    }
    if w(EE_SCENARIO_WC_ON_NODE, &[unit]) != 0 && w(EE_SCENARIO_WC_COMPLEX_POWER, &[unit]) as c_long > 0 {
        w(EE_SCENARIO_WC_SET_POWER, &[unit]);
    }
    w(EE_SCENARIO_WC_PENDING, &[player]);
    w(EE_SCENARIO_WC_FLUSH, &[]);
    w(EE_SCENARIO_WC_PLACE_ON_GROUND, &[unit]);
    if w(EE_SCENARIO_WC_ATTRIBUTE_CHECK, &[]) != 0 {
        w(EE_SCENARIO_WC_CHANGE_UNIT, &[unit]);
    }
    if create {
        w(EE_SCENARIO_WC_PUSH_CREATED, &[unit]);
    }
    if w(EE_SCENARIO_WC_SOUND, &[unit]) != 0 && w(EE_SCENARIO_WC_SOUND_LOCAL, &[unit]) != 0 {
        w(EE_SCENARIO_WC_SOUND_PLAY, &[unit]);
    }
}

/// A stance's group of one unit: made, the unit added, pending on its player, kept mobile.
fn stance_group(w: &mut dyn FnMut(u32, &[i64]) -> i64, id: i64) {
    w(EE_SCENARIO_WX_UNIT_GROUP_CREATE, &[id]);
    w(EE_SCENARIO_WX_GROUP_ADD_COMPLEX, &[id]);
    w(EE_SCENARIO_WX_UNIT_GROUP_PEND, &[id]);
    w(EE_SCENARIO_WORLD_GROUP_KEEP_MOBILE, &[]);
}

/// `SMEPlayTrumpets`: the trumpets' sound played when it exists.
fn trumpets(w: &mut dyn FnMut(u32, &[i64]) -> i64) {
    if w(EE_SCENARIO_WX_TRUMPETS, &[]) != 0 {
        w(EE_SCENARIO_WX_TRUMPETS_PLAY, &[]);
    }
}

#[cfg(test)]
#[path = "world_compose_tests.rs"]
mod tests;
