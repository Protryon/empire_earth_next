//! `EEComplexUnit`'s life-cycle bodies over its C++ members (the units' lane, round 7; were EEComplexUnit.cpp):
//! `CopyFrom`, `ResetEEComplexUnit`, `Init`, `FreeNode`, `SwapGraphic`, `ClearGoals`, `StepAside`, `IsNextToTile`,
//! `UpdateFanaticismColor`, `GetChecksum`. Every C++ statement of the originals is one operation of
//! [`CuHost::cu`] (the game's: `ee_units_cu_host`, EEComplexUnit.cpp, one statement a case), asked in the
//! original's order; the choices, loops and arithmetic are here. `unit` is the `EEComplexUnit*`, `a` an object,
//! `b`, `c` values, `f` floats in and out.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::units::cu_body::*;
use crate::units::cu_cargo::*;
use crate::units::cu_misc::*;
use crate::units::unit_bulk::{EeUnitsComplexCopyIn, EeUnitsComplexResetIn};

// ---- the statements (`op`; `a` the other unit unless noted)
/// `mProjectileStart = a->mProjectileStart`
pub const EE_UNITS_CU_COPY_PROJECTILE_START: i32 = 500;
/// `SetAnimationFrame(a->mPhysicalModel.GetFrame())`
pub const EE_UNITS_CU_COPY_ANIMATION_FRAME: i32 = 501;
/// `SetAnimation(a->mAnimation, -1, 0)`
pub const EE_UNITS_CU_COPY_ANIMATION: i32 = 502;
/// `a->GetUnitClass()->mTileSize == GetUnitClass()->mTileSize`
pub const EE_UNITS_CU_SAME_TILE_SIZE: i32 = 503;
/// `mPhysicalModel.SetLocation(a->mPhysicalModel.GetLocation())`
pub const EE_UNITS_CU_COPY_LOCATION: i32 = 504;
/// `mAngularVelocity = a->mAngularVelocity`
pub const EE_UNITS_CU_COPY_ANGULAR_VELOCITY: i32 = 505;
/// `mOrientation = a->mOrientation`
pub const EE_UNITS_CU_COPY_ORIENTATION: i32 = 506;
/// `mInitialContactLocation = a->mInitialContactLocation`
pub const EE_UNITS_CU_COPY_CONTACT_LOCATION: i32 = 507;
/// `mInitialContactHeading = a->mInitialContactHeading`
pub const EE_UNITS_CU_COPY_CONTACT_HEADING: i32 = 508;
/// `mUnitVariables = a->mUnitVariables`
pub const EE_UNITS_CU_COPY_UNIT_VARIABLES: i32 = 509;
/// `mBehavior = a->mBehavior`
pub const EE_UNITS_CU_COPY_BEHAVIOR: i32 = 510;
/// `GetPlayer()->IsHuman()`
pub const EE_UNITS_CU_PLAYER_IS_HUMAN: i32 = 511;
/// `mBehavior == EEUnitBehavior::sTable[b]`
pub const EE_UNITS_CU_BEHAVIOR_IS: i32 = 512;
/// `mBehavior = (EEUnitBehavior*)a`
pub const EE_UNITS_CU_SET_BEHAVIOR: i32 = 513;
/// `mBehavior`
pub const EE_UNITS_CU_BEHAVIOR: i32 = 514;
/// `GetUnitClass()->mBehavior`
pub const EE_UNITS_CU_CLASS_BEHAVIOR: i32 = 515;
/// `EEUnitBehavior::sTable[b]`
pub const EE_UNITS_CU_BEHAVIOR_TABLE: i32 = 516;
/// `mHeight = a->mHeight`
pub const EE_UNITS_CU_COPY_HEIGHT: i32 = 517;
/// `a->mOwner`
pub const EE_UNITS_CU_OTHER_OWNER: i32 = 518;
/// `SetOwner(a)`
pub const EE_UNITS_CU_SET_OWNER: i32 = 519;
/// `GetUnitClass()->GetUnitType() == a->GetUnitClass()->GetUnitType()`
pub const EE_UNITS_CU_SAME_UNIT_TYPE: i32 = 520;
/// unit `a`'s `GetModel()->GetNumParts()`
pub const EE_UNITS_CU_UNIT_MODEL_PARTS: i32 = 521;
/// `mTurretAngles[b] = a->mTurretAngles[b]`
pub const EE_UNITS_CU_COPY_TURRET_ANGLE: i32 = 522;
/// unit `a`'s `mNumOrbitals`
pub const EE_UNITS_CU_UNIT_NUM_ORBITALS: i32 = 523;
/// `mOrbitals[b]->mAngle = a->mOrbitals[b]->mAngle`
pub const EE_UNITS_CU_COPY_ORBITAL_ANGLE: i32 = 524;
/// unit `a`'s `mNumWheels`
pub const EE_UNITS_CU_UNIT_NUM_WHEELS: i32 = 525;
/// `mWheels[b]->mAngle = a->mWheels[b]->mAngle`
pub const EE_UNITS_CU_COPY_WHEEL_ANGLE: i32 = 526;
/// `SetSound(a->mSound)`
pub const EE_UNITS_CU_COPY_SOUND: i32 = 527;
/// unit `a`'s `GetPlayer()->GetUpgrade(GetUnitClass()->GetUnitType())`
pub const EE_UNITS_CU_UNIT_UPGRADE: i32 = 528;
/// upgrade `a`'s `mMaxHitPoints`
pub const EE_UNITS_CU_UPGRADE_MAX_HIT_POINTS: i32 = 529;
/// upgrade `a`'s `mPower`
pub const EE_UNITS_CU_UPGRADE_POWER: i32 = 530;
/// upgrade `a`'s `mFlightTime`
pub const EE_UNITS_CU_UPGRADE_FLIGHT_TIME: i32 = 531;
/// upgrade `a`'s `mReloadTime`
pub const EE_UNITS_CU_UPGRADE_RELOAD_TIME: i32 = 532;
/// `gCivPowersManager.HasPower(GetPlayer(), kCivPowerCamouflage)`
pub const EE_UNITS_CU_HAS_CAMOUFLAGE_POWER: i32 = 533;
/// `gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)`
pub const EE_UNITS_CU_CAMOUFLAGE_POWER_SET: i32 = 534;
/// set `a`'s `Contains(unit b's GetUnitClass()->GetUnitType())`
pub const EE_UNITS_CU_SET_CONTAINS_UNIT: i32 = 535;
/// `ee_units_complex_copy(b, this, unit c, the input at a or NULL)`
pub const EE_UNITS_CU_COMPLEX_COPY: i32 = 536;
/// `UpdateCamouflage(mCommand != NULL ? mCommand->mType : 0)`
pub const EE_UNITS_CU_UPDATE_CAMOUFLAGE_COMMAND: i32 = 537;
/// `mLastMapPosition = a->mLastMapPosition`
pub const EE_UNITS_CU_COPY_LAST_MAP_POSITION: i32 = 538;
/// `mU0DC = a->mU0DC`
pub const EE_UNITS_CU_COPY_U0DC: i32 = 539;
/// `mCommand = a->mCommand`
pub const EE_UNITS_CU_COPY_COMMAND: i32 = 540;
/// `EEMovePassengers(this, a, a->GetPlayer() != GetPlayer())`
pub const EE_UNITS_CU_MOVE_PASSENGERS: i32 = 541;
/// `EETakeOverLoad(this, a)`
pub const EE_UNITS_CU_TAKE_OVER_LOAD: i32 = 542;
/// unit `a`'s `mGoalQueues.size()`
pub const EE_UNITS_CU_UNIT_GOAL_QUEUE_COUNT: i32 = 543;
/// unit `a`'s `mGoalQueues[b].mGoals.size()`
pub const EE_UNITS_CU_UNIT_GOAL_COUNT: i32 = 544;
/// unit `a`'s `mGoalQueues[b].mGoals[c]`
pub const EE_UNITS_CU_UNIT_GOAL_AT: i32 = 545;
/// goal `a`'s `mKind`
pub const EE_UNITS_CU_GOAL_KIND_OF: i32 = 546;
/// `a->GetPlayer() == GetPlayer()`
pub const EE_UNITS_CU_SAME_PLAYER: i32 = 547;
/// `EEUnloadGoalCast(goal a)->SetUnloadTransport(this)`
pub const EE_UNITS_CU_SET_UNLOAD_TRANSPORT: i32 = 548;
/// goal `a`'s `Clone()`
pub const EE_UNITS_CU_GOAL_CLONE: i32 = 549;
/// `mGoalQueues[b].mGoals.push_back(goal a)`
pub const EE_UNITS_CU_QUEUE_PUSH_GOAL: i32 = 550;
/// goal `a`'s `SetOwner(mGoalQueues[b].GetUnit())`
pub const EE_UNITS_CU_GOAL_SET_QUEUE_OWNER: i32 = 551;
/// `mWaypoints = a->mWaypoints`
pub const EE_UNITS_CU_COPY_WAYPOINTS: i32 = 552;
/// `mBuildQueue = a->mBuildQueue`
pub const EE_UNITS_CU_COPY_BUILD_QUEUE: i32 = 553;
/// `a->mAreaEffects = a->mAreaEffects` (the original's self-assignment, which clears them)
pub const EE_UNITS_CU_SELF_ASSIGN_AREA_EFFECTS: i32 = 554;
/// `a->mAffectingAreaEffects = a->mAffectingAreaEffects`
pub const EE_UNITS_CU_SELF_ASSIGN_AFFECTING: i32 = 555;
/// `mHomeBase = a->mHomeBase`
pub const EE_UNITS_CU_COPY_HOME_BASE: i32 = 556;
/// `mContainer = a->mContainer`
pub const EE_UNITS_CU_COPY_CONTAINER: i32 = 557;
/// unit `a`'s `ReplaceCargo((EEComplexUnit*)b, this)`
pub const EE_UNITS_CU_REPLACE_CARGO_WITH_THIS: i32 = 558;
/// `EEUnit::CopyFrom(a)` (`a` the `RTSUnit*`)
pub const EE_UNITS_CU_UNIT_COPY_FROM: i32 = 559;
// ResetEEComplexUnit's
/// `mBuildQueue.clear()`
pub const EE_UNITS_CU_RESET_BUILD_QUEUE: i32 = 560;
/// `mCitizenBuildQueue.clear()`
pub const EE_UNITS_CU_RESET_CITIZEN_QUEUE: i32 = 561;
/// `mBoundedPathTiles.clear()`
pub const EE_UNITS_CU_RESET_BOUNDED_PATH_TILES: i32 = 562;
/// `mCargo.clear()`
pub const EE_UNITS_CU_RESET_CARGO: i32 = 563;
/// `mWaypoints.clear()`
pub const EE_UNITS_CU_RESET_WAYPOINTS: i32 = 564;
/// `mGoalQueues.clear()`
pub const EE_UNITS_CU_RESET_GOAL_QUEUES: i32 = 565;
/// `mUnitVariables.clear()`
pub const EE_UNITS_CU_RESET_UNIT_VARIABLES: i32 = 566;
/// `mUnitVariables.resize(3, 0)`
pub const EE_UNITS_CU_RESIZE_UNIT_VARIABLES: i32 = 567;
/// `mLastMapPosition = GE3DPoint(0.0f, 0.0f, 0.0f)`
pub const EE_UNITS_CU_RESET_LAST_MAP_POSITION: i32 = 568;
/// `mU0DC.SetX(-10000.0f)`
pub const EE_UNITS_CU_RESET_U0DC_X: i32 = 569;
/// `mU0DC.SetY(-10000.0f)`
pub const EE_UNITS_CU_RESET_U0DC_Y: i32 = 570;
/// `mPlayer != NULL`
pub const EE_UNITS_CU_HAS_PLAYER: i32 = 571;
/// `GetStats()->mReloadTime`
pub const EE_UNITS_CU_STATS_RELOAD_TIME: i32 = 572;
/// `GetStats()->mPowerRegeneration`
pub const EE_UNITS_CU_STATS_POWER_REGENERATION: i32 = 573;
/// `GetUnitClass()->mInitialGarrisonCount`
pub const EE_UNITS_CU_CLASS_INITIAL_GARRISON: i32 = 574;
/// `GetUnitClass()->mIsCloakable`
pub const EE_UNITS_CU_CLASS_CLOAKABLE: i32 = 575;
/// `ee_units_complex_reset(this, the input at a)`
pub const EE_UNITS_CU_COMPLEX_RESET: i32 = 576;
/// one of the pointer members ResetEEComplexUnit clears (`b`: `EE_UNITS_CU_FIELD_*`) `= NULL`
pub const EE_UNITS_CU_CLEAR_FIELD: i32 = 577;
/// `mCollisionTile.mX = -10000`
pub const EE_UNITS_CU_RESET_COLLISION_TILE_X: i32 = 578;
/// `mCollisionTile.mY = -10000`
pub const EE_UNITS_CU_RESET_COLLISION_TILE_Y: i32 = 579;
/// `mProjectileStart = GE3DPoint(0.0f, 0.0f, 0.0f)`
pub const EE_UNITS_CU_RESET_PROJECTILE_START: i32 = 580;
/// `mConversionEffect.mValue = -1`
pub const EE_UNITS_CU_RESET_CONVERSION_EFFECT: i32 = 581;
/// `mHealEffect.mValue = -1`
pub const EE_UNITS_CU_RESET_HEAL_EFFECT: i32 = 582;
/// `mPhysicalModel.SetModel(NULL)`
pub const EE_UNITS_CU_PM_CLEAR_MODEL: i32 = 583;
/// `mPhysicalModel.SetScale(the float of the bits b)`
pub const EE_UNITS_CU_PM_SET_SCALE: i32 = 584;
/// `mPhysicalModel.SetLocation(GETransformation())`
pub const EE_UNITS_CU_PM_RESET_LOCATION: i32 = 585;
/// `mPhysicalModel.SetFrame(0)`
pub const EE_UNITS_CU_PM_RESET_FRAME: i32 = 586;
/// `mAngularVelocity.SetPitch(0.0f)`
pub const EE_UNITS_CU_RESET_PITCH: i32 = 587;
/// `mAngularVelocity.SetYaw(0.0f)`
pub const EE_UNITS_CU_RESET_YAW: i32 = 588;
/// `mAngularVelocity.SetRoll(0.0f)`
pub const EE_UNITS_CU_RESET_ROLL: i32 = 589;
/// `mOrientation = mAngularVelocity`
pub const EE_UNITS_CU_RESET_ORIENTATION: i32 = 590;
/// `mColor.SetBlue(1.0f)`
pub const EE_UNITS_CU_RESET_BLUE: i32 = 591;
/// `mColor.SetGreen(1.0f)`
pub const EE_UNITS_CU_RESET_GREEN: i32 = 592;
/// `mColor.SetRed(1.0f)`
pub const EE_UNITS_CU_RESET_RED: i32 = 593;
/// `mGoalQueues.resize(b, EEUnitVectorItem(this))`
pub const EE_UNITS_CU_RESIZE_GOAL_QUEUES: i32 = 594;
/// `mFireEffect.mValue = 0`
pub const EE_UNITS_CU_RESET_FIRE_EFFECT: i32 = 595;
// FreeNode's
/// `delete[] mTurretAngles`
pub const EE_UNITS_CU_DELETE_TURRET_ANGLES: i32 = 596;
/// `delete mOrbitals[b]`
pub const EE_UNITS_CU_DELETE_ORBITAL: i32 = 597;
/// `delete[] mOrbitals`
pub const EE_UNITS_CU_DELETE_ORBITALS: i32 = 598;
/// `mNumOrbitals = 0`
pub const EE_UNITS_CU_ZERO_NUM_ORBITALS: i32 = 599;
/// `delete mWheels[b]`
pub const EE_UNITS_CU_DELETE_WHEEL: i32 = 600;
/// `delete[] mWheels`
pub const EE_UNITS_CU_DELETE_WHEELS: i32 = 601;
/// `mNumWheels = 0`
pub const EE_UNITS_CU_ZERO_NUM_WHEELS: i32 = 602;
/// `EEUnit::FreeNode()`
pub const EE_UNITS_CU_UNIT_FREE_NODE: i32 = 603;
// ClearGoals'
/// `SetGatheredResource(NULL)`
pub const EE_UNITS_CU_CLEAR_GATHERED_RESOURCE: i32 = 604;
/// `mGoalQueues[b].mGoals.size()`
pub const EE_UNITS_CU_QUEUE_GOAL_COUNT: i32 = 605;
/// `mGoalQueues[b].GetFront()->Abandon(this)`
pub const EE_UNITS_CU_QUEUE_ABANDON_FRONT: i32 = 606;
/// `mGoalQueues[b].PopFront()`
pub const EE_UNITS_CU_QUEUE_POP_FRONT_GOAL: i32 = 607;
/// `mGoalQueues[b].GetFront()`
pub const EE_UNITS_CU_QUEUE_FRONT_GOAL: i32 = 608;
/// goal `a`'s `Suits(this)`
pub const EE_UNITS_CU_GOAL_SUITS: i32 = 609;
/// goal `a`'s `Start(this)`
pub const EE_UNITS_CU_GOAL_START: i32 = 610;
// SwapGraphic's (`a` the old graphic, `b` the new one's handle where noted)
/// graphic `a`'s `mStartFrames[b]`
pub const EE_UNITS_CU_GRAPHIC_START_FRAME: i32 = 611;
/// graphic `a`'s `mFrameCounts[b]`
pub const EE_UNITS_CU_GRAPHIC_FRAME_COUNT: i32 = 612;
/// graphic `a`'s `mLastFrames[b]`
pub const EE_UNITS_CU_GRAPHIC_LAST_FRAME: i32 = 613;
/// `UMFloatToLong(the float of the bits b)`
pub const EE_UNITS_CU_FLOAT_TO_LONG: i32 = 614;
/// `SetAnimationFrame(b)`
pub const EE_UNITS_CU_SET_ANIMATION_FRAME: i32 = 615;
/// graphic `a`'s `GetNumParts()`
pub const EE_UNITS_CU_GRAPHIC_NUM_PARTS: i32 = 616;
/// the scratch angles `= new float[b]`
pub const EE_UNITS_CU_SCRATCH_ANGLES_NEW: i32 = 617;
/// the scratch angles' `[b] = b < c ? mTurretAngles[b] : 0.0f` (c: the old part count)
pub const EE_UNITS_CU_SCRATCH_ANGLE_COPY: i32 = 618;
/// `mTurretAngles = the scratch angles`
pub const EE_UNITS_CU_TURRET_ANGLES_ASSIGN: i32 = 619;
/// graphic `a`'s `GetOrbitalCount()`
pub const EE_UNITS_CU_GRAPHIC_ORBITAL_COUNT: i32 = 620;
/// the scratch orbitals `= b == 0 ? NULL : new EEOrbital*[b]`
pub const EE_UNITS_CU_SCRATCH_ORBITALS_NEW: i32 = 621;
/// the scratch orbitals' `[b] = new EEOrbital(*graphic a->mOrbitals[b])`
pub const EE_UNITS_CU_SCRATCH_ORBITAL_NEW: i32 = 622;
/// the scratch orbitals' `[b]->mAngle = mOrbitals[b]->mAngle`
pub const EE_UNITS_CU_SCRATCH_ORBITAL_KEEP_ANGLE: i32 = 623;
/// `mNumOrbitals`
pub const EE_UNITS_CU_NUM_ORBITALS: i32 = 624;
/// `mOrbitals = the scratch orbitals`
pub const EE_UNITS_CU_ORBITALS_ASSIGN: i32 = 625;
/// `mNumOrbitals = b`
pub const EE_UNITS_CU_SET_NUM_ORBITALS: i32 = 626;
/// graphic `a`'s `GetWheelCount()`
pub const EE_UNITS_CU_GRAPHIC_WHEEL_COUNT: i32 = 627;
/// the scratch wheels `= b == 0 ? NULL : new EEWheel*[b]`
pub const EE_UNITS_CU_SCRATCH_WHEELS_NEW: i32 = 628;
/// the scratch wheels' `[b] = new EEWheel(*graphic a->mWheels[b])`
pub const EE_UNITS_CU_SCRATCH_WHEEL_NEW: i32 = 629;
/// the scratch wheels' `[b]->mAngle = mWheels[b]->mAngle`
pub const EE_UNITS_CU_SCRATCH_WHEEL_KEEP_ANGLE: i32 = 630;
/// `mNumWheels`
pub const EE_UNITS_CU_NUM_WHEELS: i32 = 631;
/// `mWheels = the scratch wheels`
pub const EE_UNITS_CU_WHEELS_ASSIGN: i32 = 632;
/// `mNumWheels = b`
pub const EE_UNITS_CU_SET_NUM_WHEELS: i32 = 633;
// StepAside's
/// `GetUnitClass()->mFamily`
pub const EE_UNITS_CU_CLASS_FAMILY: i32 = 634;
/// `GetStats()->mAttack` (its bits)
pub const EE_UNITS_CU_STATS_ATTACK: i32 = 635;
/// `GetStats()->mMaxVelocity` (its bits)
pub const EE_UNITS_CU_STATS_MAX_VELOCITY: i32 = 636;
/// `a == mPlayer || mPlayer->mEnemies[player a's mPlayerID] == 0`
pub const EE_UNITS_CU_PLAYER_IS_FRIEND: i32 = 637;
/// `EEGatherGoalCast(GetGoal())` (0: none)
pub const EE_UNITS_CU_GATHER_GOAL: i32 = 638;
/// gather goal `a`'s `FindGatherSpot(this)`
pub const EE_UNITS_CU_FIND_GATHER_SPOT: i32 = 639;
/// class `a`'s `mTileSize`
pub const EE_UNITS_CU_CLASS_TILE_SIZE_OF: i32 = 640;
/// the scratch tiles' `clear()`
pub const EE_UNITS_CU_SCRATCH_TILES_CLEAR: i32 = 641;
/// the scratch tiles' `push_back(EETileCoordinate(b, c))`
pub const EE_UNITS_CU_SCRATCH_TILES_PUSH: i32 = 642;
/// `EEAIRandomShuffle(the scratch tiles' begin(), end())`
pub const EE_UNITS_CU_SCRATCH_TILES_SHUFFLE: i32 = 643;
/// the scratch tiles' `size()`
pub const EE_UNITS_CU_SCRATCH_TILES_SIZE: i32 = 644;
/// the scratch tiles' `back()` (f[0], f[1]: its x and y as longs' bits... no: the answer is x, f is unused; y by
/// `EE_UNITS_CU_SCRATCH_TILES_BACK_Y`)
pub const EE_UNITS_CU_SCRATCH_TILES_BACK_X: i32 = 645;
/// the scratch tiles' `back().mY`
pub const EE_UNITS_CU_SCRATCH_TILES_BACK_Y: i32 = 646;
/// the scratch tiles' `pop_back()`
pub const EE_UNITS_CU_SCRATCH_TILES_POP: i32 = 647;
/// `ee_units_uu_can_stand_on(b, c, this)`
pub const EE_UNITS_CU_CAN_STAND_ON: i32 = 648;
/// `EEUGRust::NewLocation(GE2DPoint(f[0], f[1]), false, true, false, 0.0f, false, 0)`
pub const EE_UNITS_CU_NEW_LOCATION_GOAL: i32 = 649;
/// `SetGoal(goal a, b, c)`
pub const EE_UNITS_CU_SET_GOAL: i32 = 650;
// IsNextToTile's
/// `ee_units_uu_square_ring(GetTileX(), GetTileY(), the class's tile size twice, 1, the scratch points)`
pub const EE_UNITS_CU_SCRATCH_SQUARE_RING: i32 = 651;
/// `ee_units_uu_ring_corners(0, GetTileX(), GetTileY(), the class's tile size twice, 1, the scratch points)`
pub const EE_UNITS_CU_SCRATCH_RING_CORNERS: i32 = 652;
/// the scratch points' `size()`
pub const EE_UNITS_CU_SCRATCH_POINTS_SIZE: i32 = 653;
/// the scratch points' `[b].GetX()`
pub const EE_UNITS_CU_SCRATCH_POINT_X: i32 = 654;
/// the scratch points' `[b].GetY()`
pub const EE_UNITS_CU_SCRATCH_POINT_Y: i32 = 655;
/// `GetTileX()`
pub const EE_UNITS_CU_GET_TILE_X: i32 = 656;
// UpdateFanaticismColor's
/// `gPhysics->mGameTime`
pub const EE_UNITS_CU_GAME_TIME: i32 = 657;
/// `mCloaked`
pub const EE_UNITS_CU_CLOAKED: i32 = 658;
/// `mCloaked = 0`
pub const EE_UNITS_CU_CLEAR_CLOAKED_TIME: i32 = 659;
/// the fanaticism effect's (`EEGraphicEffect::Get(kGraphicEffectFanaticismEffect)`) `mIntParam1`
pub const EE_UNITS_CU_FANATICISM_PERIOD: i32 = 660;
/// the fanaticism effect's `mParam<b>` (b 1..6; its bits)
pub const EE_UNITS_CU_FANATICISM_PARAM: i32 = 661;
/// `SetColor(GEColor(f[0], f[1], f[2]))` (the function static `sColor`'s copy)
pub const EE_UNITS_CU_SET_COLOR: i32 = 662;
// GetChecksum's
/// `mComplexUnit->mTimesUpdated`
pub const EE_UNITS_CU_TIMES_UPDATED: i32 = 663;
/// `mComplexUnit->mActiveGroup != NULL`
pub const EE_UNITS_CU_HAS_ACTIVE_GROUP: i32 = 664;
/// `mComplexUnit->mCommand ? mComplexUnit->mCommand->mType : 0`
pub const EE_UNITS_CU_COMMAND_OR_ZERO: i32 = 665;
/// `mComplexUnit`'s location's x, y or z (b 0, 1, 2; its bits)
pub const EE_UNITS_CU_CHECKSUM_LOCATION: i32 = 666;
/// `mComplexUnit->mHeight` (its bits)
pub const EE_UNITS_CU_HEIGHT: i32 = 667;
/// goal `mGoalQueues[b].mGoals[c]`'s `GetOutOfSynchChecksum()`
pub const EE_UNITS_CU_GOAL_CHECKSUM: i32 = 668;
/// `mAffectingAreaEffects.mItems.size()`
pub const EE_UNITS_CU_AFFECTING_COUNT: i32 = 669;
/// `mAffectingAreaEffects.mItems[b]->Checksum()`
pub const EE_UNITS_CU_AFFECTING_CHECKSUM: i32 = 670;
/// `mAreaEffects.mItems.size()`
pub const EE_UNITS_CU_AREA_EFFECT_COUNT: i32 = 671;
/// `mAreaEffects.mItems[b]->Checksum()`
pub const EE_UNITS_CU_AREA_EFFECT_CHECKSUM: i32 = 672;
/// `EEUnit::GetChecksum(b)`
pub const EE_UNITS_CU_UNIT_CHECKSUM: i32 = 673;
// Init's
/// `EEUnit::Init(class a, player b)`
pub const EE_UNITS_CU_UNIT_INIT: i32 = 674;
/// `mIsCollidable =` class `a`'s collidable rule (collidable, not air unless it flies over, not a projectile or a
/// waypoint)
pub const EE_UNITS_CU_INIT_COLLIDABLE: i32 = 675;
/// `mCanBeHidden =` class `a`'s hiding rule
pub const EE_UNITS_CU_INIT_CAN_BE_HIDDEN: i32 = 676;
/// `UMFloor(the float of the bits b)`
pub const EE_UNITS_CU_UM_FLOOR: i32 = 677;
/// `MoveTo(b, c, false)`
pub const EE_UNITS_CU_MOVE_TO: i32 = 678;
/// `UpdateWaterState(false)`
pub const EE_UNITS_CU_UPDATE_WATER_STATE_OFF: i32 = 679;
/// `mPhysicalModel.SetLocation(the scratch placement)`
pub const EE_UNITS_CU_PM_SET_PLACEMENT: i32 = 680;
/// the scratch placement `= *(const GETransformation*)a` (the original's copy `thePlacement`)
pub const EE_UNITS_CU_PLACEMENT_COPY: i32 = 681;
/// class `a`'s `mScale` (its bits)
pub const EE_UNITS_CU_CLASS_SCALE: i32 = 682;
/// `mCargoRoom = class a's mTransportCapacity`
pub const EE_UNITS_CU_INIT_CARGO_ROOM: i32 = 683;
/// `mCollisionRadius = class a's GetInnerCollisionSphere()`
pub const EE_UNITS_CU_INIT_COLLISION_RADIUS: i32 = 684;
/// `mIsCitizen = b != 0`
pub const EE_UNITS_CU_SET_IS_CITIZEN: i32 = 685;
/// `mIsDeletable = class a's mIsDeletable`
pub const EE_UNITS_CU_INIT_DELETABLE: i32 = 686;
/// player `a`'s `GetUpgrade(GetUnitClass()->GetUnitType())->mAreaEffectTemplates[b]`
pub const EE_UNITS_CU_UPGRADE_AREA_TEMPLATE: i32 = 687;
/// `gAEManager.CreateAreaEffect(b, this)`
pub const EE_UNITS_CU_CREATE_AREA_EFFECT: i32 = 688;
/// `mAreaEffects.Add(effect a)`
pub const EE_UNITS_CU_AREA_EFFECTS_ADD: i32 = 689;
/// effect `a`'s `Dereference()`
pub const EE_UNITS_CU_EFFECT_DEREFERENCE: i32 = 690;
/// class `a`'s `mCategory`
pub const EE_UNITS_CU_CLASS_CATEGORY_OF: i32 = 691;

/// the pointer members ResetEEComplexUnit clears, in its order (`EE_UNITS_CU_CLEAR_FIELD`'s `b`)
pub const EE_UNITS_CU_FIELD_LAUNCHER_CLASS: i64 = 0;
pub const EE_UNITS_CU_FIELD_OWNER: i64 = 1;
pub const EE_UNITS_CU_FIELD_ACTIVE_GROUP: i64 = 2;
pub const EE_UNITS_CU_FIELD_PREVIOUS_PART: i64 = 3;
pub const EE_UNITS_CU_FIELD_NEXT_PART: i64 = 4;
pub const EE_UNITS_CU_FIELD_HOME_BASE: i64 = 5;
pub const EE_UNITS_CU_FIELD_CONTAINER: i64 = 6;
pub const EE_UNITS_CU_FIELD_SOUND: i64 = 7;
pub const EE_UNITS_CU_FIELD_ACTIVE_GOAL: i64 = 8;
pub const EE_UNITS_CU_FIELD_GOAL_STATE: i64 = 9;
pub const EE_UNITS_CU_FIELD_BLOCKING_UNIT: i64 = 10;
pub const EE_UNITS_CU_FIELD_PATROL_GROUP: i64 = 11;
pub const EE_UNITS_CU_FIELD_TURRET_ANGLES: i64 = 12;
pub const EE_UNITS_CU_FIELD_ORBITALS: i64 = 13;
pub const EE_UNITS_CU_FIELD_WHEELS: i64 = 14;
pub const EE_UNITS_CU_FIELD_GATHERED_RESOURCE: i64 = 15;
pub const EE_UNITS_CU_FIELD_LAST_RESOURCE: i64 = 16;
pub const EE_UNITS_CU_FIELD_COMMAND: i64 = 17;
pub const EE_UNITS_CU_FIELD_LAST_ATTACKER: i64 = 18;
pub const EE_UNITS_CU_FIELD_MINE: i64 = 19;
pub const EE_UNITS_CU_FIELD_HERD: i64 = 20;
pub const EE_UNITS_CU_FIELD_HOLDER: i64 = 21;

// ---- the bodies (`ee_units_cu_body`'s `what`)
pub const EE_UNITS_CU_DO_COPY_FROM: i32 = 60;
pub const EE_UNITS_CU_DO_RESET: i32 = 61;
pub const EE_UNITS_CU_DO_FREE_NODE: i32 = 63;
pub const EE_UNITS_CU_DO_SWAP_GRAPHIC: i32 = 64;
pub const EE_UNITS_CU_DO_CLEAR_GOALS: i32 = 65;
pub const EE_UNITS_CU_DO_STEP_ASIDE: i32 = 66;
pub const EE_UNITS_CU_DO_IS_NEXT_TO_TILE: i32 = 67;
pub const EE_UNITS_CU_DO_UPDATE_FANATICISM_COLOR: i32 = 68;
pub const EE_UNITS_CU_DO_GET_CHECKSUM: i32 = 69;

/// `kGoalUnloadTransport`
const GOAL_UNLOAD_TRANSPORT: i64 = 8;
/// the behaviour table's harass, cautious and default entries
const BEHAVIOR_HARASS: i64 = 3;
const BEHAVIOR_CAUTIOUS: i64 = 4;
const BEHAVIOR_DEFAULT: i64 = 5;
/// `kActionIdle`, `kActionWaitForOpenPosition`, `kActionGatherResource`, `kActionMoveToWaitingArea`,
/// `kActionPrepareToMove`
const ACTION_IDLE: i64 = 5000;
const ACTION_WAIT_FOR_OPEN_POSITION: i64 = 4026;
const ACTION_GATHER_RESOURCE: i64 = 4004;
const ACTION_MOVE_TO_WAITING_AREA: i64 = 4023;
const ACTION_PREPARE_TO_MOVE: i64 = 8000;
/// `kUnitFamilyAnimal`
const FAMILY_ANIMAL: i64 = 0x11;
/// `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
/// a full turn (the reset's heading)
const TWO_PI: f32 = 6.283_185_5;

struct S<'h, H: CuHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
}

impl<H: CuHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.cu(op, self.unit, 0, 0, 0, &mut [0.0; 4])
    }
    fn qa(&mut self, op: i32, a: u64) -> i64 {
        self.h.cu(op, self.unit, a, 0, 0, &mut [0.0; 4])
    }
    fn qb(&mut self, op: i32, b: i64) -> i64 {
        self.h.cu(op, self.unit, 0, b, 0, &mut [0.0; 4])
    }
    fn qab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.cu(op, self.unit, a, b, 0, &mut [0.0; 4])
    }
    fn qbc(&mut self, op: i32, b: i64, c: i64) -> i64 {
        self.h.cu(op, self.unit, 0, b, c, &mut [0.0; 4])
    }
    fn qabc(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.h.cu(op, self.unit, a, b, c, &mut [0.0; 4])
    }
    fn qf(&mut self, op: i32, a: u64, f: &mut [f32; 4]) -> i64 {
        self.h.cu(op, self.unit, a, 0, 0, f)
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
    fn obj(&mut self, op: i32) -> u64 {
        self.q(op) as u64
    }
    fn float(&mut self, op: i32) -> f32 {
        f32::from_bits(self.q(op) as u32)
    }
}

fn bits(v: f32) -> i64 {
    i64::from(v.to_bits())
}

/// `CopyFrom(other)`: a replaced unit's state into this one (position, timers, the record's scaled copies, goals,
/// cargo, links); `rts` is `other` as the `RTSUnit*` the base class's copy takes.
pub fn copy_from<H: CuHost + ?Sized>(h: &mut H, unit: u64, other: u64, rts: u64) {
    let s = &mut S { h, unit };
    s.qa(EE_UNITS_CU_COPY_PROJECTILE_START, other);
    s.qa(EE_UNITS_CU_COPY_ANIMATION_FRAME, other);
    s.qa(EE_UNITS_CU_COPY_ANIMATION, other);
    if s.qa(EE_UNITS_CU_SAME_TILE_SIZE, other) != 0 {
        s.qa(EE_UNITS_CU_COPY_LOCATION, other);
        s.qa(EE_UNITS_CU_COPY_ANGULAR_VELOCITY, other);
        s.qa(EE_UNITS_CU_COPY_ORIENTATION, other);
    }
    s.qa(EE_UNITS_CU_COPY_CONTACT_LOCATION, other);
    s.qa(EE_UNITS_CU_COPY_CONTACT_HEADING, other);
    s.qa(EE_UNITS_CU_COPY_UNIT_VARIABLES, other);
    s.qa(EE_UNITS_CU_COPY_BEHAVIOR, other);
    if s.yes(EE_UNITS_CU_PLAYER_IS_HUMAN)
        && (s.qb(EE_UNITS_CU_BEHAVIOR_IS, BEHAVIOR_HARASS) != 0 || s.qb(EE_UNITS_CU_BEHAVIOR_IS, BEHAVIOR_CAUTIOUS) != 0)
    {
        s.qa(EE_UNITS_CU_SET_BEHAVIOR, 0);
    }
    default_behavior(s);
    s.qa(EE_UNITS_CU_COPY_HEIGHT, other);
    let owner = s.qa(EE_UNITS_CU_OTHER_OWNER, other) as u64;
    if owner != 0 {
        s.qa(EE_UNITS_CU_SET_OWNER, owner);
    }
    if s.qa(EE_UNITS_CU_SAME_UNIT_TYPE, other) != 0 {
        let mut i = 0;
        while (i as i64) < s.qa(EE_UNITS_CU_UNIT_MODEL_PARTS, other) && (i as i64) < s.qa(EE_UNITS_CU_UNIT_MODEL_PARTS, unit) {
            s.qab(EE_UNITS_CU_COPY_TURRET_ANGLE, other, i);
            i += 1;
        }
        let mut i = 0;
        while i < s.qa(EE_UNITS_CU_UNIT_NUM_ORBITALS, other) && i < s.qa(EE_UNITS_CU_UNIT_NUM_ORBITALS, unit) {
            s.qab(EE_UNITS_CU_COPY_ORBITAL_ANGLE, other, i);
            i += 1;
        }
        let mut i = 0;
        while i < s.qa(EE_UNITS_CU_UNIT_NUM_WHEELS, other) && i < s.qa(EE_UNITS_CU_UNIT_NUM_WHEELS, unit) {
            s.qab(EE_UNITS_CU_COPY_WHEEL_ANGLE, other, i);
            i += 1;
        }
    }
    if s.obj(EE_UNITS_CU_SOUND) != 0 {
        s.q(EE_UNITS_CU_SOUND_STOP);
    }
    s.qa(EE_UNITS_CU_COPY_SOUND, other);
    // the record's copies and the fractions scaled to this unit's maxima (unit_bulk.rs copy_first)
    let stats = s.qa(EE_UNITS_CU_UNIT_UPGRADE, unit) as u64;
    let other_stats = s.qa(EE_UNITS_CU_UNIT_UPGRADE, other) as u64;
    let camouflage = if s.yes(EE_UNITS_CU_HAS_CAMOUFLAGE_POWER) { s.obj(EE_UNITS_CU_CAMOUFLAGE_POWER_SET) } else { 0 };
    let input = EeUnitsComplexCopyIn {
        other_max_hit_points: s.qa(EE_UNITS_CU_UPGRADE_MAX_HIT_POINTS, other_stats) as c_long,
        max_hit_points: s.qa(EE_UNITS_CU_UPGRADE_MAX_HIT_POINTS, stats) as c_long,
        other_power: s.qa(EE_UNITS_CU_UPGRADE_POWER, other_stats) as c_long,
        power: s.qa(EE_UNITS_CU_UPGRADE_POWER, stats) as c_long,
        other_flight_time: s.qa(EE_UNITS_CU_UPGRADE_FLIGHT_TIME, other_stats) as c_long,
        flight_time: s.qa(EE_UNITS_CU_UPGRADE_FLIGHT_TIME, stats) as c_long,
        other_reload_time: s.qa(EE_UNITS_CU_UPGRADE_RELOAD_TIME, other_stats) as c_long,
        reload_time: s.qa(EE_UNITS_CU_UPGRADE_RELOAD_TIME, stats) as c_long,
        copy_cloakable: camouflage != 0
            && s.qab(EE_UNITS_CU_SET_CONTAINS_UNIT, camouflage, other as i64) != 0
            && s.qab(EE_UNITS_CU_SET_CONTAINS_UNIT, camouflage, unit as i64) != 0,
    };
    s.qabc(EE_UNITS_CU_COMPLEX_COPY, &input as *const EeUnitsComplexCopyIn as usize as u64, 0, other as i64);
    s.q(EE_UNITS_CU_UPDATE_CAMOUFLAGE_COMMAND);
    s.qa(EE_UNITS_CU_COPY_LAST_MAP_POSITION, other);
    s.qa(EE_UNITS_CU_COPY_U0DC, other);
    s.qa(EE_UNITS_CU_COPY_COMMAND, other);
    s.qabc(EE_UNITS_CU_COMPLEX_COPY, 0, 1, other as i64);
    s.qa(EE_UNITS_CU_MOVE_PASSENGERS, other);
    s.qa(EE_UNITS_CU_TAKE_OVER_LOAD, other);
    let mut i = 0;
    while i < s.qa(EE_UNITS_CU_UNIT_GOAL_QUEUE_COUNT, other) && i < s.qa(EE_UNITS_CU_UNIT_GOAL_QUEUE_COUNT, unit) {
        s.qb(EE_UNITS_CU_GOAL_QUEUE_CLEAR, i);
        let mut j = 0;
        while j < s.qab(EE_UNITS_CU_UNIT_GOAL_COUNT, other, i) {
            let mut goal = s.qabc(EE_UNITS_CU_UNIT_GOAL_AT, other, i, j) as u64;
            if s.qa(EE_UNITS_CU_GOAL_KIND_OF, goal) == GOAL_UNLOAD_TRANSPORT && s.qa(EE_UNITS_CU_SAME_PLAYER, other) != 0 {
                s.qa(EE_UNITS_CU_SET_UNLOAD_TRANSPORT, goal);
            }
            goal = s.qa(EE_UNITS_CU_GOAL_CLONE, goal) as u64;
            s.qab(EE_UNITS_CU_QUEUE_PUSH_GOAL, goal, i);
            s.qab(EE_UNITS_CU_GOAL_SET_QUEUE_OWNER, goal, i);
            j += 1;
        }
        i += 1;
    }
    s.qa(EE_UNITS_CU_COPY_WAYPOINTS, other);
    s.qa(EE_UNITS_CU_COPY_BUILD_QUEUE, other);
    let research = s.qa(EE_UNITS_CU_UNIT_CURRENT_RESEARCH, other);
    s.qb(EE_UNITS_CU_PLAY_EFFECT, research);
    // the original assigns each of the other unit's containers to itself, which clears them
    s.qa(EE_UNITS_CU_SELF_ASSIGN_AREA_EFFECTS, other);
    s.qa(EE_UNITS_CU_SELF_ASSIGN_AFFECTING, other);
    s.qabc(EE_UNITS_CU_COMPLEX_COPY, 0, 2, other as i64);
    s.qa(EE_UNITS_CU_COPY_HOME_BASE, other);
    let base = s.obj(EE_UNITS_CU_HOME_BASE);
    if base != 0 {
        s.qa(EE_UNITS_CU_REFERENCE, base);
    }
    s.qa(EE_UNITS_CU_COPY_CONTAINER, other);
    let container = s.obj(EE_UNITS_CU_CONTAINER);
    if container != 0 {
        s.qa(EE_UNITS_CU_REFERENCE, container);
        s.qab(EE_UNITS_CU_REPLACE_CARGO_WITH_THIS, container, other as i64);
    }
    s.q(EE_UNITS_CU_REFRESH_GOALS);
    s.q(EE_UNITS_CU_REFRESH_ACTIVE_GOAL);
    s.qa(EE_UNITS_CU_UNIT_COPY_FROM, rts);
}

/// unit `a`'s `mCurrentResearch`
pub const EE_UNITS_CU_UNIT_CURRENT_RESEARCH: i32 = 692;

/// A unit without a behaviour takes its class's, else the table's default.
fn default_behavior<H: CuHost + ?Sized>(s: &mut S<'_, H>) {
    if s.obj(EE_UNITS_CU_BEHAVIOR) == 0 {
        if s.yes(EE_UNITS_CU_HAS_CLASS) {
            let b = s.obj(EE_UNITS_CU_CLASS_BEHAVIOR);
            s.qa(EE_UNITS_CU_SET_BEHAVIOR, b);
        }
        if s.obj(EE_UNITS_CU_BEHAVIOR) == 0 {
            let b = s.qb(EE_UNITS_CU_BEHAVIOR_TABLE, BEHAVIOR_DEFAULT) as u64;
            s.qa(EE_UNITS_CU_SET_BEHAVIOR, b);
        }
    }
}

/// `ResetEEComplexUnit(goal_queues, random_heading)`: every mobile-unit field to its default (the record's through
/// unit_bulk.rs, the heading drawn when asked), `goal_queues` empty goal queues.
pub fn reset<H: CuHost + ?Sized>(h: &mut H, unit: u64, goal_queues: i64, random_heading: bool) {
    let s = &mut S { h, unit };
    for op in [
        EE_UNITS_CU_RESET_BUILD_QUEUE,
        EE_UNITS_CU_RESET_CITIZEN_QUEUE,
        EE_UNITS_CU_RESET_BOUNDED_PATH_TILES,
        EE_UNITS_CU_RESET_CARGO,
        EE_UNITS_CU_RESET_WAYPOINTS,
        EE_UNITS_CU_RESET_GOAL_QUEUES,
        EE_UNITS_CU_AREA_EFFECTS_CLEAR,
        EE_UNITS_CU_AREA_EFFECTS_ITEMS_CLEAR,
        EE_UNITS_CU_AFFECTING_CLEAR,
        EE_UNITS_CU_AFFECTING_ITEMS_CLEAR,
        EE_UNITS_CU_RESET_UNIT_VARIABLES,
        EE_UNITS_CU_RESIZE_UNIT_VARIABLES,
        EE_UNITS_CU_RESET_LAST_MAP_POSITION,
        EE_UNITS_CU_RESET_U0DC_X,
        EE_UNITS_CU_RESET_U0DC_Y,
    ] {
        s.q(op);
    }
    // the record's members, from the class and stats as the original read them (the heading's draw first)
    let has_class = s.yes(EE_UNITS_CU_HAS_CLASS);
    let has_stats = has_class && s.yes(EE_UNITS_CU_HAS_PLAYER);
    let heading = if random_heading { s.float(EE_UNITS_CU_RANDOM) * TWO_PI } else { TWO_PI };
    let input = EeUnitsComplexResetIn {
        heading,
        reload_time: if has_stats { s.q(EE_UNITS_CU_STATS_RELOAD_TIME) as c_long } else { 0 },
        power_regeneration: if has_stats { s.q(EE_UNITS_CU_STATS_POWER_REGENERATION) as c_long } else { 0 },
        garrison_count: if has_class { s.q(EE_UNITS_CU_CLASS_INITIAL_GARRISON) as c_long } else { 0 },
        cloakable: has_class && s.yes(EE_UNITS_CU_CLASS_CLOAKABLE),
        paid_for: has_class && !s.yes(EE_UNITS_CU_IS_LAND_BUILDING),
    };
    s.qa(EE_UNITS_CU_COMPLEX_RESET, &input as *const EeUnitsComplexResetIn as usize as u64);
    let behavior = if has_class {
        s.obj(EE_UNITS_CU_CLASS_BEHAVIOR)
    } else {
        s.qb(EE_UNITS_CU_BEHAVIOR_TABLE, BEHAVIOR_DEFAULT) as u64
    };
    s.qa(EE_UNITS_CU_SET_BEHAVIOR, behavior);
    for field in EE_UNITS_CU_FIELD_LAUNCHER_CLASS..=EE_UNITS_CU_FIELD_SOUND {
        s.qb(EE_UNITS_CU_CLEAR_FIELD, field);
    }
    s.q(EE_UNITS_CU_RESET_COLLISION_TILE_X);
    s.q(EE_UNITS_CU_RESET_COLLISION_TILE_Y);
    s.q(EE_UNITS_CU_RESET_PROJECTILE_START);
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_ACTIVE_GOAL);
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_GOAL_STATE);
    s.q(EE_UNITS_CU_RESET_CONVERSION_EFFECT);
    s.q(EE_UNITS_CU_RESET_HEAL_EFFECT);
    for field in EE_UNITS_CU_FIELD_BLOCKING_UNIT..=EE_UNITS_CU_FIELD_LAST_ATTACKER {
        s.qb(EE_UNITS_CU_CLEAR_FIELD, field);
    }
    s.q(EE_UNITS_CU_PM_CLEAR_MODEL);
    s.qb(EE_UNITS_CU_PM_SET_SCALE, bits(1.0));
    s.q(EE_UNITS_CU_PM_RESET_LOCATION);
    s.q(EE_UNITS_CU_PM_RESET_FRAME);
    for op in [
        EE_UNITS_CU_RESET_PITCH,
        EE_UNITS_CU_RESET_YAW,
        EE_UNITS_CU_RESET_ROLL,
        EE_UNITS_CU_RESET_ORIENTATION,
        EE_UNITS_CU_RESET_BLUE,
        EE_UNITS_CU_RESET_GREEN,
        EE_UNITS_CU_RESET_RED,
    ] {
        s.q(op);
    }
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_MINE);
    s.qb(EE_UNITS_CU_RESIZE_GOAL_QUEUES, goal_queues);
    crate::units::cu_world::release_held_units(s.h, unit);
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_HERD);
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_HOLDER);
    s.q(EE_UNITS_CU_RESET_FIRE_EFFECT);
}

/// `Init(class, placement, player, goal_queues)`: a new unit from its class (collision, hiding, tile, scale, area
/// effects); `placement` the caller's transformation (read through the host).
pub fn init<H: CuHost + ?Sized>(h: &mut H, unit: u64, class: u64, placement: u64, player: u64, goal_queues: i64) {
    let s = &mut S { h, unit };
    s.qa(EE_UNITS_CU_PLACEMENT_COPY, placement);
    let x = s.float(EE_UNITS_CU_PLACEMENT_X);
    let y = s.float(EE_UNITS_CU_PLACEMENT_Y);
    s.qab(EE_UNITS_CU_UNIT_INIT, class, player as i64);
    reset(s.h, unit, goal_queues, true);
    s.qa(EE_UNITS_CU_INIT_COLLIDABLE, class);
    s.qa(EE_UNITS_CU_INIT_CAN_BE_HIDDEN, class);
    let tx = s.qb(EE_UNITS_CU_UM_FLOOR, bits(x));
    let ty = s.qb(EE_UNITS_CU_UM_FLOOR, bits(y));
    s.qbc(EE_UNITS_CU_MOVE_TO, tx, ty);
    s.q(EE_UNITS_CU_UPDATE_WATER_STATE_OFF);
    if s.q(EE_UNITS_CU_TILE_SIZE) > 1 {
        let offset = s.qa(EE_UNITS_CU_CLASS_TILE_SIZE_OF, class) as f32 * 0.5 - 0.5;
        let x = s.float(EE_UNITS_CU_PLACEMENT_X) + offset;
        s.qb(EE_UNITS_CU_PLACEMENT_SET_X, bits(x));
        let y = s.float(EE_UNITS_CU_PLACEMENT_Y) + offset;
        s.qb(EE_UNITS_CU_PLACEMENT_SET_Y, bits(y));
    }
    s.q(EE_UNITS_CU_PM_SET_PLACEMENT);
    let scale = s.qa(EE_UNITS_CU_CLASS_SCALE, class);
    s.qb(EE_UNITS_CU_PM_SET_SCALE, scale);
    s.qa(EE_UNITS_CU_INIT_CARGO_ROOM, class);
    s.qa(EE_UNITS_CU_INIT_COLLISION_RADIUS, class);
    let category = s.qa(EE_UNITS_CU_CLASS_CATEGORY_OF, class);
    s.qb(EE_UNITS_CU_SET_IS_CITIZEN, i64::from(category == CATEGORY_LAND_CITIZEN || category == CATEGORY_NAVAL_CITIZEN));
    s.qa(EE_UNITS_CU_INIT_DELETABLE, class);
    for i in 0..4 {
        let template = s.qab(EE_UNITS_CU_UPGRADE_AREA_TEMPLATE, player, i);
        if template <= 0 {
            continue;
        }
        let effect = s.qb(EE_UNITS_CU_CREATE_AREA_EFFECT, template) as u64;
        s.qa(EE_UNITS_CU_AREA_EFFECTS_ADD, effect);
        s.qa(EE_UNITS_CU_EFFECT_DEREFERENCE, effect);
    }
}

/// `FreeNode()`: the turret angles, orbitals, wheels and model freed, then the base unit's data.
pub fn free_node<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let s = &mut S { h, unit };
    s.q(EE_UNITS_CU_DELETE_TURRET_ANGLES);
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_TURRET_ANGLES);
    let mut i = 0;
    while i < s.q(EE_UNITS_CU_NUM_ORBITALS) {
        s.qb(EE_UNITS_CU_DELETE_ORBITAL, i);
        i += 1;
    }
    s.q(EE_UNITS_CU_DELETE_ORBITALS);
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_ORBITALS);
    s.q(EE_UNITS_CU_ZERO_NUM_ORBITALS);
    let mut j = 0;
    while j < s.q(EE_UNITS_CU_NUM_WHEELS) {
        s.qb(EE_UNITS_CU_DELETE_WHEEL, j);
        j += 1;
    }
    s.q(EE_UNITS_CU_DELETE_WHEELS);
    s.qb(EE_UNITS_CU_CLEAR_FIELD, EE_UNITS_CU_FIELD_WHEELS);
    s.q(EE_UNITS_CU_ZERO_NUM_WHEELS);
    s.q(EE_UNITS_CU_PM_CLEAR_MODEL);
    s.q(EE_UNITS_CU_UNIT_FREE_NODE);
}

/// `SwapGraphic(old, new)`: the unit on another graphic, keeping its animation progress and turret, orbital and
/// wheel angles.
pub fn swap_graphic<H: CuHost + ?Sized>(h: &mut H, unit: u64, old: u64, new: u64) {
    let s = &mut S { h, unit };
    let state = s.q(EE_UNITS_CU_ANIMATION);
    let frame = s.q(EE_UNITS_CU_PM_FRAME) as u32;
    let old_start = s.qab(EE_UNITS_CU_GRAPHIC_START_FRAME, old, state) as u32;
    // (float)(unsigned long)(frame - start) / (float)count
    let mut fraction = frame.wrapping_sub(old_start) as f32 / s.qab(EE_UNITS_CU_GRAPHIC_FRAME_COUNT, old, state) as u32 as f32;
    if fraction < 0.0 {
        fraction = 0.0;
    } else if fraction > 1.0 {
        fraction = 1.0;
    }
    let count = s.qab(EE_UNITS_CU_GRAPHIC_FRAME_COUNT, new, state) as u32 as f32;
    let start = s.qab(EE_UNITS_CU_GRAPHIC_START_FRAME, new, state) as u32;
    let mut frame = (s.qb(EE_UNITS_CU_FLOAT_TO_LONG, bits(count * fraction)) as u32).wrapping_add(start);
    let last = s.qab(EE_UNITS_CU_GRAPHIC_LAST_FRAME, new, state) as u32;
    if frame > last {
        frame = last;
    }
    s.qb(EE_UNITS_CU_SET_ANIMATION_FRAME, i64::from(frame));
    frame = frame.wrapping_add(1);
    if frame > s.qab(EE_UNITS_CU_GRAPHIC_LAST_FRAME, new, state) as u32 {
        frame = s.qab(EE_UNITS_CU_GRAPHIC_START_FRAME, new, state) as u32;
    }
    s.qb(EE_UNITS_CU_SET_NEXT_FRAME, i64::from(frame));
    let parts = s.qa(EE_UNITS_CU_GRAPHIC_NUM_PARTS, new);
    s.qb(EE_UNITS_CU_SCRATCH_ANGLES_NEW, parts);
    for i in 0..parts {
        let old_parts = s.qa(EE_UNITS_CU_GRAPHIC_NUM_PARTS, old);
        s.qbc(EE_UNITS_CU_SCRATCH_ANGLE_COPY, i, old_parts);
    }
    s.q(EE_UNITS_CU_DELETE_TURRET_ANGLES);
    s.q(EE_UNITS_CU_TURRET_ANGLES_ASSIGN);
    let orbitals = s.qa(EE_UNITS_CU_GRAPHIC_ORBITAL_COUNT, new);
    s.qb(EE_UNITS_CU_SCRATCH_ORBITALS_NEW, orbitals);
    for i in 0..orbitals {
        s.qab(EE_UNITS_CU_SCRATCH_ORBITAL_NEW, new, i);
        if i < s.q(EE_UNITS_CU_NUM_ORBITALS) {
            s.qb(EE_UNITS_CU_SCRATCH_ORBITAL_KEEP_ANGLE, i);
        }
    }
    let mut i = 0;
    while i < s.q(EE_UNITS_CU_NUM_ORBITALS) {
        s.qb(EE_UNITS_CU_DELETE_ORBITAL, i);
        i += 1;
    }
    s.q(EE_UNITS_CU_DELETE_ORBITALS);
    s.q(EE_UNITS_CU_ORBITALS_ASSIGN);
    s.qb(EE_UNITS_CU_SET_NUM_ORBITALS, orbitals);
    let wheels = s.qa(EE_UNITS_CU_GRAPHIC_WHEEL_COUNT, new);
    s.qb(EE_UNITS_CU_SCRATCH_WHEELS_NEW, wheels);
    for i in 0..wheels {
        s.qab(EE_UNITS_CU_SCRATCH_WHEEL_NEW, new, i);
        if i < s.q(EE_UNITS_CU_NUM_WHEELS) {
            s.qb(EE_UNITS_CU_SCRATCH_WHEEL_KEEP_ANGLE, i);
        }
    }
    let mut i = 0;
    while i < s.q(EE_UNITS_CU_NUM_WHEELS) {
        s.qb(EE_UNITS_CU_DELETE_WHEEL, i);
        i += 1;
    }
    s.q(EE_UNITS_CU_DELETE_WHEELS);
    s.q(EE_UNITS_CU_WHEELS_ASSIGN);
    s.qb(EE_UNITS_CU_SET_NUM_WHEELS, wheels);
}

/// `ClearGoals(index)`: the queue's goals abandoned from the front until one that suits the unit starts.
pub fn clear_goals<H: CuHost + ?Sized>(h: &mut H, unit: u64, index: i64) {
    let s = &mut S { h, unit };
    s.q(EE_UNITS_CU_CLEAR_GATHERED_RESOURCE);
    // `mGoalQueues.size() <= (unsigned long)inIndex`
    if (s.q(EE_UNITS_CU_GOAL_QUEUE_COUNT) as u64) <= index as c_long as std::os::raw::c_ulong as u64 {
        return;
    }
    loop {
        if s.qb(EE_UNITS_CU_QUEUE_GOAL_COUNT, index) <= 0 {
            break;
        }
        s.qb(EE_UNITS_CU_QUEUE_ABANDON_FRONT, index);
        s.qb(EE_UNITS_CU_QUEUE_POP_FRONT_GOAL, index);
        if (index as c_long as std::os::raw::c_ulong) < 1 {
            s.q(EE_UNITS_CU_CLEAR_ACTIVE_GOAL);
        }
        s.q(EE_UNITS_CU_REFRESH_GOALS);
        s.q(EE_UNITS_CU_REFRESH_ACTIVE_GOAL);
        if s.qb(EE_UNITS_CU_QUEUE_GOAL_COUNT, index) <= 0 {
            continue;
        }
        let goal = s.qb(EE_UNITS_CU_QUEUE_FRONT_GOAL, index) as u64;
        if s.qa(EE_UNITS_CU_GOAL_SUITS, goal) != 0 {
            s.qa(EE_UNITS_CU_GOAL_START, goal);
            break;
        }
    }
    s.q(EE_UNITS_CU_REFRESH_GOALS);
    s.q(EE_UNITS_CU_REFRESH_ACTIVE_GOAL);
}

/// `StepAside(player, class, x, y, force)`: an idle or gathering unit sent to a random free tile around a
/// footprint being placed (a waiting gatherer looks for another spot); true if it was sent away.
pub fn step_aside<H: CuHost + ?Sized>(h: &mut H, unit: u64, player: u64, class: u64, x: i64, y: i64, force: bool) -> bool {
    let s = &mut S { h, unit };
    let command = if s.yes(EE_UNITS_CU_HAS_COMMAND) { s.q(EE_UNITS_CU_COMMAND_KIND) } else { 0 };
    let unarmed = s.q(EE_UNITS_CU_CLASS_FAMILY) == FAMILY_ANIMAL && s.float(EE_UNITS_CU_STATS_ATTACK) == 0.0;
    if !((command == ACTION_IDLE || command == ACTION_WAIT_FOR_OPEN_POSITION || command == ACTION_GATHER_RESOURCE)
        && s.float(EE_UNITS_CU_STATS_MAX_VELOCITY) > 0.0
        && player != 0
        && (s.qa(EE_UNITS_CU_PLAYER_IS_FRIEND, player) != 0 || unarmed || force))
    {
        return false;
    }
    if command == ACTION_WAIT_FOR_OPEN_POSITION {
        // (the port: a waiting unit whose goal is no longer a gather - the original dereferenced the null cast)
        let gather = s.obj(EE_UNITS_CU_GATHER_GOAL);
        if gather != 0 && s.qa(EE_UNITS_CU_FIND_GATHER_SPOT, gather) != 0 {
            s.qb(EE_UNITS_CU_START_ACTION, ACTION_MOVE_TO_WAITING_AREA);
            s.q(EE_UNITS_CU_ACTION_CHANGED);
        }
        return false;
    }
    let size = s.qa(EE_UNITS_CU_CLASS_TILE_SIZE_OF, class);
    let (min_x, max_x) = (x - size - 1, x + size + 1);
    let (min_y, max_y) = (y - size - 1, y + size + 1);
    s.q(EE_UNITS_CU_SCRATCH_TILES_CLEAR);
    for tx in min_x..=max_x {
        if tx != min_x && tx != max_x {
            s.qbc(EE_UNITS_CU_SCRATCH_TILES_PUSH, tx, min_y);
            s.qbc(EE_UNITS_CU_SCRATCH_TILES_PUSH, tx, max_y);
        } else {
            for ty in min_y..=max_y {
                s.qbc(EE_UNITS_CU_SCRATCH_TILES_PUSH, tx, ty);
            }
        }
    }
    s.q(EE_UNITS_CU_SCRATCH_TILES_SHUFFLE);
    let mut found = false;
    let (mut fx, mut fy) = (0i64, 0i64);
    while s.q(EE_UNITS_CU_SCRATCH_TILES_SIZE) != 0 {
        fx = s.q(EE_UNITS_CU_SCRATCH_TILES_BACK_X);
        fy = s.q(EE_UNITS_CU_SCRATCH_TILES_BACK_Y);
        s.q(EE_UNITS_CU_SCRATCH_TILES_POP);
        found = s.qbc(EE_UNITS_CU_CAN_STAND_ON, fx, fy) != 0;
        if found {
            break;
        }
    }
    if !found {
        return false;
    }
    let mut f = [fx as c_long as f32 + 0.5, fy as c_long as f32 + 0.5, 0.0, 0.0];
    let goal = s.qf(EE_UNITS_CU_NEW_LOCATION_GOAL, 0, &mut f) as u64;
    s.qabc(EE_UNITS_CU_SET_GOAL, goal, i64::from(command == ACTION_GATHER_RESOURCE), 0);
    s.qb(EE_UNITS_CU_START_ACTION, ACTION_PREPARE_TO_MOVE);
    s.q(EE_UNITS_CU_ACTION_CHANGED);
    true
}

/// `mCommand->mType`
pub const EE_UNITS_CU_COMMAND_KIND: i32 = 693;
/// the scratch placement's `GetX()` (its bits)
pub const EE_UNITS_CU_PLACEMENT_X: i32 = 694;
/// the scratch placement's `GetY()` (its bits)
pub const EE_UNITS_CU_PLACEMENT_Y: i32 = 695;
/// the scratch placement's `SetX(the float of the bits b)`
pub const EE_UNITS_CU_PLACEMENT_SET_X: i32 = 696;
/// the scratch placement's `SetY(the float of the bits b)`
pub const EE_UNITS_CU_PLACEMENT_SET_Y: i32 = 697;
/// the scratch points' `clear()` (the original's fresh local)
pub const EE_UNITS_CU_SCRATCH_POINTS_CLEAR: i32 = 698;

/// `IsNextToTile(x, y)`: the unit's tile, or one of the side or corner tiles around its footprint.
pub fn is_next_to_tile<H: CuHost + ?Sized>(h: &mut H, unit: u64, x: i64, y: i64) -> bool {
    let s = &mut S { h, unit };
    let tx = s.q(EE_UNITS_CU_GET_TILE_X);
    let ty = s.q(EE_UNITS_CU_TILE_Y);
    if tx == x && ty == y {
        return true;
    }
    s.q(EE_UNITS_CU_SCRATCH_POINTS_CLEAR);
    for ring in [EE_UNITS_CU_SCRATCH_SQUARE_RING, EE_UNITS_CU_SCRATCH_RING_CORNERS] {
        s.q(ring);
        let mut k = 0;
        while k < s.q(EE_UNITS_CU_SCRATCH_POINTS_SIZE) {
            if x == s.qb(EE_UNITS_CU_SCRATCH_POINT_X, k) && y == s.qb(EE_UNITS_CU_SCRATCH_POINT_Y, k) {
                return true;
            }
            k += 1;
        }
    }
    false
}

/// `UpdateFanaticismColor`'s function statics: the effect's two colours, the colour, the time last computed
/// (the original's file static `sU563238`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct FanaticismColors {
    pub cloaked: [f32; 3],
    pub base: [f32; 3],
    pub color: [f32; 3],
    pub time: u32,
}

impl FanaticismColors {
    /// the originals' first values
    pub const START: Self =
        Self { cloaked: [0.9, 0.0, 0.0], base: [0.6, 0.5, 0.5], color: [0.5, 0.5, 0.5], time: 0 };
}

/// `UpdateFanaticismColor()`: a fanatic unit's colour pulses between the effect's two colours; it ends when the
/// time runs out.
pub fn update_fanaticism_color<H: CuHost + ?Sized>(h: &mut H, unit: u64, c: &mut FanaticismColors) {
    let s = &mut S { h, unit };
    if !s.yes(EE_UNITS_CU_IS_CLOAKED) {
        return;
    }
    let now = s.q(EE_UNITS_CU_GAME_TIME) as u32;
    if now > s.q(EE_UNITS_CU_CLOAKED) as u32 {
        s.q(EE_UNITS_CU_CLEAR_TINT);
        s.q(EE_UNITS_CU_CLEAR_CLOAKED_TIME);
        return;
    }
    if now != c.time {
        let period = s.q(EE_UNITS_CU_FANATICISM_PERIOD) as i32;
        let mut p = [0.0f32; 6];
        for (k, v) in p.iter_mut().enumerate() {
            *v = f32::from_bits(s.qb(EE_UNITS_CU_FANATICISM_PARAM, k as i64 + 1) as u32);
        }
        c.cloaked = [p[0], p[1], p[2]];
        c.base = [p[3], p[4], p[5]];
        // (mCloaked - mGameTime) % (unsigned long)(period * 2), as a long
        let left = (s.q(EE_UNITS_CU_CLOAKED) as u32).wrapping_sub(s.q(EE_UNITS_CU_GAME_TIME) as u32);
        let mut phase = (left % (period.wrapping_mul(2) as u32)) as i32;
        if phase > period {
            phase = period * 2 - phase;
        }
        c.time = now;
        let fraction = phase as f32 / period as f32;
        let rest = 1.0 - fraction;
        for k in 0..3 {
            c.color[k] = c.base[k] * rest + c.cloaked[k] * fraction;
        }
    }
    let mut f = [c.color[0], c.color[1], c.color[2], 0.0];
    s.qf(EE_UNITS_CU_SET_COLOR, 0, &mut f);
}

/// `GetChecksum(flags)`: the unit's out-of-sync checksum of the selected parts (the tile and hit points; the
/// physics, the floats as their bits; the goals; the area effects), plus the base unit's; summed as the `long`s
/// they were.
pub fn get_checksum<H: CuHost + ?Sized>(h: &mut H, unit: u64, flags: u32) -> i64 {
    let s = &mut S { h, unit };
    let mut sum: c_long = 0;
    let add = |sum: &mut c_long, v: i64| *sum = sum.wrapping_add(v as c_long);
    if flags & 1 != 0 {
        let hp = s.q(EE_UNITS_CU_HIT_POINTS) as c_long;
        let ty = s.q(EE_UNITS_CU_TILE_Y) as c_long;
        let tx = s.q(EE_UNITS_CU_GET_TILE_X) as c_long;
        sum = hp.wrapping_add(ty).wrapping_add(tx);
    }
    if flags & 2 != 0 {
        add(&mut sum, s.q(EE_UNITS_CU_TIMES_UPDATED));
        let x = s.qb(EE_UNITS_CU_CHECKSUM_LOCATION, 0) as u32 as i32;
        let group = s.q(EE_UNITS_CU_HAS_ACTIVE_GROUP) as c_long;
        let command = s.q(EE_UNITS_CU_COMMAND_OR_ZERO) as c_long;
        sum = sum.wrapping_add(group.wrapping_add(command).wrapping_add(x as c_long));
        add(&mut sum, i64::from(s.qb(EE_UNITS_CU_CHECKSUM_LOCATION, 1) as u32 as i32));
        add(&mut sum, i64::from(s.qb(EE_UNITS_CU_CHECKSUM_LOCATION, 2) as u32 as i32));
        add(&mut sum, i64::from(s.q(EE_UNITS_CU_HEIGHT) as u32 as i32));
    }
    if flags & 0x20 != 0 {
        let mut i = 0;
        while i < s.q(EE_UNITS_CU_GOAL_QUEUE_COUNT) {
            let count = s.qb(EE_UNITS_CU_QUEUE_GOAL_COUNT, i);
            for j in 0..count {
                add(&mut sum, s.qbc(EE_UNITS_CU_GOAL_CHECKSUM, i, j));
            }
            i += 1;
        }
    }
    if flags & 8 != 0 {
        let mut k = 0;
        while k < s.q(EE_UNITS_CU_AFFECTING_COUNT) {
            add(&mut sum, s.qb(EE_UNITS_CU_AFFECTING_CHECKSUM, k));
            k += 1;
        }
        let mut k = 0;
        while k < s.q(EE_UNITS_CU_AREA_EFFECT_COUNT) {
            add(&mut sum, s.qb(EE_UNITS_CU_AREA_EFFECT_CHECKSUM, k));
            k += 1;
        }
    }
    (s.qb(EE_UNITS_CU_UNIT_CHECKSUM, i64::from(flags)) as c_long).wrapping_add(sum) as i64
}

/// The process's `UpdateFanaticismColor` statics (the simulation thread's alone).
static FANATICISM: std::sync::Mutex<FanaticismColors> = std::sync::Mutex::new(FanaticismColors::START);

/// Runs body `what` (`EE_UNITS_CU_DO_*` of this module) on `unit`; None for another module's.
pub fn run<H: CuHost + ?Sized>(h: &mut H, what: i32, unit: u64, a: i64, b: i64, _c: i64) -> Option<i64> {
    Some(match what {
        // a: the other unit as an EEComplexUnit*, b: as the RTSUnit*
        EE_UNITS_CU_DO_COPY_FROM => {
            copy_from(h, unit, a as u64, b as u64);
            0
        }
        // a: the goal queue count, b: the random heading
        EE_UNITS_CU_DO_RESET => {
            reset(h, unit, a, b != 0);
            0
        }
        EE_UNITS_CU_DO_FREE_NODE => {
            free_node(h, unit);
            0
        }
        EE_UNITS_CU_DO_SWAP_GRAPHIC => {
            swap_graphic(h, unit, a as u64, b as u64);
            0
        }
        EE_UNITS_CU_DO_CLEAR_GOALS => {
            clear_goals(h, unit, a);
            0
        }
        EE_UNITS_CU_DO_IS_NEXT_TO_TILE => i64::from(is_next_to_tile(h, unit, a, b)),
        EE_UNITS_CU_DO_UPDATE_FANATICISM_COLOR => {
            let mut colors = *FANATICISM.lock().unwrap_or_else(|e| e.into_inner());
            update_fanaticism_color(h, unit, &mut colors);
            *FANATICISM.lock().unwrap_or_else(|e| e.into_inner()) = colors;
            0
        }
        EE_UNITS_CU_DO_GET_CHECKSUM => get_checksum(h, unit, a as u32),
        _ => return crate::units::cu_world::run(h, what, unit, a, b, _c),
    })
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::cu_body::game::Game;
    use std::os::raw::c_void;

    /// `EEComplexUnit::Init(class, placement, player, goal_queues)` on `unit` (more arguments than
    /// `ee_units_cu_body` carries).
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`; `class`, `placement` (a `GETransformation*`) and `player` live, on the
    /// simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_init(
        unit: *mut c_void,
        class: *mut c_void,
        placement: *const c_void,
        player: *mut c_void,
        goal_queues: u32,
    ) {
        init(&mut Game, unit as usize as u64, class as usize as u64, placement as usize as u64, player as usize as u64, i64::from(goal_queues));
    }

    /// `EEComplexUnit::StepAside(player, class, x, y, force)` on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`, `class` a live class, `player` a live player or null.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_step_aside(
        unit: *mut c_void,
        player: *mut c_void,
        class: *mut c_void,
        x: i64,
        y: i64,
        force: u8,
    ) -> u8 {
        u8::from(step_aside(&mut Game, unit as usize as u64, player as usize as u64, class as usize as u64, x, y, force != 0))
    }
}

#[cfg(test)]
#[path = "cu_life_tests.rs"]
mod tests;
