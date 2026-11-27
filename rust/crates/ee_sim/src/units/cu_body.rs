//! `EEComplexUnit`'s bodies over its C++ members (the units' lane, round 6; were EEComplexUnit.cpp): `Kill`,
//! `Invalidate`, `StartProduction`, `SetBuilt`, `SetAnimation`, `PayInsurance`, `FindContacts`, `StartAction`. Every
//! C++ statement of the originals is one operation of [`CuHost::cu`] (the game's: `ee_units_cu_host`,
//! EEComplexUnit.cpp, one statement a case), asked in the original's order; the choices, loops and arithmetic are
//! here. `unit` is the `EEComplexUnit*` the body runs on, `a` an object, `b`, `c` values, `f` floats in and out.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

// ---- the statements (`op`; `a` an object, `b`, `c` values; the answer)
/// `mHitPoints`
pub const EE_UNITS_CU_HIT_POINTS: i32 = 1;
/// `mPlayer->UnitTaken(this, b != 0)`
pub const EE_UNITS_CU_UNIT_TAKEN: i32 = 2;
/// `gWorldClearing`
pub const EE_UNITS_CU_WORLD_CLEARING: i32 = 3;
/// `gUnitSetManager.GetSet(b)`
pub const EE_UNITS_CU_UNIT_SET: i32 = 4;
/// set `a`'s `Contains(GetUnitClass()->GetUnitType())`
pub const EE_UNITS_CU_SET_HAS_TYPE: i32 = 5;
/// `gWorld.mNumPlayers`
pub const EE_UNITS_CU_NUM_PLAYERS: i32 = 6;
/// `gWorld.mPlayers[(unsigned short)b]`
pub const EE_UNITS_CU_PLAYER_AT: i32 = 7;
/// player `a`'s `mEnemies[mPlayer->mPlayerID]`
pub const EE_UNITS_CU_ENEMY_STATE: i32 = 8;
/// player `a`'s `UpdateFreeUnitPower()`
pub const EE_UNITS_CU_UPDATE_FREE_UNIT_POWER: i32 = 9;
/// the dead citizen's debug line
pub const EE_UNITS_CU_DEBUG_DEAD_CITIZEN: i32 = 10;
/// `GetUnitClass()->mIsInactive`
pub const EE_UNITS_CU_CLASS_INACTIVE: i32 = 11;
/// `gWorld.ActivateUnit(this)`
pub const EE_UNITS_CU_ACTIVATE: i32 = 12;
/// `mPreviousPart`
pub const EE_UNITS_CU_PREVIOUS_PART: i32 = 13;
/// `UShutdownInProgress()`
pub const EE_UNITS_CU_SHUTDOWN: i32 = 14;
/// `gPhysics->GetUnitSwapKind(this)`
pub const EE_UNITS_CU_SWAP_KIND: i32 = 15;
/// `GetUnitClass()->GetUnitType()`
pub const EE_UNITS_CU_UNIT_TYPE: i32 = 16;
/// `GetUnitClass()->mCategory`
pub const EE_UNITS_CU_CATEGORY: i32 = 17;
/// `mPlayer->mSavedGroups.Reset(b, 1)`
pub const EE_UNITS_CU_STATS_RESET: i32 = 18;
/// `EEAddDeadUnitStatistic(this)`
pub const EE_UNITS_CU_ADD_DEAD_STATISTIC: i32 = 19;
/// `mPlayer->mSavedGroups.Lower(b, 1)`
pub const EE_UNITS_CU_STATS_LOWER: i32 = 20;
/// `gCivPowersManager.HasPower(GetPlayer(), kCivPowerInsurance)`
pub const EE_UNITS_CU_HAS_INSURANCE: i32 = 21;
/// `gCivPowersManager.GetPower(kCivPowerInsurance, the power)` (the power kept for the next two)
pub const EE_UNITS_CU_GET_INSURANCE: i32 = 22;
/// `gUnitSetManager.Contains(GetUnitClass()->GetUnitType(), the power's mUnitSet)`
pub const EE_UNITS_CU_INSURANCE_COVERS: i32 = 23;
/// the power's `mAmount` (its bits)
pub const EE_UNITS_CU_INSURANCE_AMOUNT: i32 = 24;
/// `mIsBuilt`
pub const EE_UNITS_CU_IS_BUILT: i32 = 25;
/// `PayInsurance(float of the bits b)`
pub const EE_UNITS_CU_PAY_INSURANCE: i32 = 26;
/// `mLastAttacker`
pub const EE_UNITS_CU_LAST_ATTACKER: i32 = 27;
/// unit `a`'s `Dereference()`
pub const EE_UNITS_CU_DEREFERENCE: i32 = 28;
/// `mLastAttacker = NULL`
pub const EE_UNITS_CU_CLEAR_LAST_ATTACKER: i32 = 29;
/// `mOwner`
pub const EE_UNITS_CU_OWNER: i32 = 30;
/// `mOwner = NULL`
pub const EE_UNITS_CU_CLEAR_OWNER: i32 = 31;
/// `UpdateJITManufacturing()`
pub const EE_UNITS_CU_UPDATE_JIT: i32 = 32;
/// `ClearContacts()`
pub const EE_UNITS_CU_CLEAR_CONTACTS: i32 = 33;
/// `KillCargo()`
pub const EE_UNITS_CU_KILL_CARGO: i32 = 34;
/// `ClearGarrison()`
pub const EE_UNITS_CU_CLEAR_GARRISON: i32 = 35;
/// `mHolder`
pub const EE_UNITS_CU_HOLDER: i32 = 36;
/// `mHolder->ReleaseHeld(this)`
pub const EE_UNITS_CU_HOLDER_RELEASE: i32 = 37;
/// `mHolder = NULL`
pub const EE_UNITS_CU_CLEAR_HOLDER_FIELD: i32 = 38;
/// `ReleaseHeldUnits()`
pub const EE_UNITS_CU_RELEASE_HELD_UNITS: i32 = 39;
/// `gPathManager.CancelPaths(this, true)`
pub const EE_UNITS_CU_CANCEL_PATHS: i32 = 40;
/// `DropPath()`
pub const EE_UNITS_CU_DROP_PATH: i32 = 41;
/// `GetUnitClass()->mAirTrafficType`
pub const EE_UNITS_CU_AIR_TRAFFIC_TYPE: i32 = 42;
/// `GetUnitClass()->IsAirBase()`
pub const EE_UNITS_CU_IS_AIR_BASE: i32 = 43;
/// `ee_units_atc_remove_airport(mPlayer->mAirTraffic[b - 1], this)`
pub const EE_UNITS_CU_ATC_REMOVE_AIRPORT: i32 = 44;
/// `ee_units_atc_remove_cargo_weight(mPlayer->mAirTraffic[b - 1], this)`
pub const EE_UNITS_CU_ATC_REMOVE_CARGO: i32 = 45;
/// `ClearCollisionState()`
pub const EE_UNITS_CU_CLEAR_COLLISION: i32 = 46;
/// `StartAction(b)`
pub const EE_UNITS_CU_START_ACTION: i32 = 47;
/// `GetUnitClass()->ActionChanged(this)`
pub const EE_UNITS_CU_ACTION_CHANGED: i32 = 48;
/// `mNextPart`
pub const EE_UNITS_CU_NEXT_PART: i32 = 49;
/// complex unit `a`'s `Kill(b != 0, false, false)`
pub const EE_UNITS_CU_KILL_PART: i32 = 50;
/// `GetUnitClass()->mDeathSpawnClass != NULL`
pub const EE_UNITS_CU_HAS_DEATH_SPAWN: i32 = 51;
/// `gFreeCamera`
pub const EE_UNITS_CU_FREE_CAMERA: i32 = 52;
/// `GetUnitClass()->CreateDeathUnit(this)`
pub const EE_UNITS_CU_CREATE_DEATH_UNIT: i32 = 53;
/// `GetUnitClass()->mRemainsClass != NULL`
pub const EE_UNITS_CU_HAS_REMAINS: i32 = 54;
/// `GetUnitClass()->mIsProjectile`
pub const EE_UNITS_CU_IS_PROJECTILE: i32 = 55;
/// `GetUnitClass()->CreateRemainsUnit(this)`
pub const EE_UNITS_CU_CREATE_REMAINS_UNIT: i32 = 56;
/// `mContainer`
pub const EE_UNITS_CU_CONTAINER: i32 = 57;
/// `mContainer->ReleaseCargo(this)`
pub const EE_UNITS_CU_CONTAINER_RELEASE: i32 = 58;
/// `SetGatheredResource(NULL)`
pub const EE_UNITS_CU_CLEAR_GATHERED: i32 = 59;
/// `SetBlockingUnit(NULL)`
pub const EE_UNITS_CU_CLEAR_BLOCKING: i32 = 60;
/// `SetLastResource(NULL)`
pub const EE_UNITS_CU_CLEAR_LAST_RESOURCE: i32 = 61;
/// `CancelQueuedProduction()`
pub const EE_UNITS_CU_CANCEL_QUEUED_PRODUCTION: i32 = 62;
/// `mGoalQueues.size()`
pub const EE_UNITS_CU_GOAL_QUEUE_COUNT: i32 = 63;
/// `ClearGoalQueue(b)`
pub const EE_UNITS_CU_CLEAR_GOAL_QUEUE: i32 = 64;
/// `mAffectingAreaEffects.KeepStrongest()`
pub const EE_UNITS_CU_AFFECTING_KEEP_STRONGEST: i32 = 65;
/// `mAffectingAreaEffects.Clear()`
pub const EE_UNITS_CU_AFFECTING_CLEAR: i32 = 66;
/// `mAffectingAreaEffects.mItems.clear()`
pub const EE_UNITS_CU_AFFECTING_ITEMS_CLEAR: i32 = 67;
/// `mAreaEffects.Clear()`
pub const EE_UNITS_CU_AREA_EFFECTS_CLEAR: i32 = 68;
/// `mAreaEffects.mItems.clear()`
pub const EE_UNITS_CU_AREA_EFFECTS_ITEMS_CLEAR: i32 = 69;
/// `ReleaseConversionEffect()`
pub const EE_UNITS_CU_RELEASE_CONVERSION_EFFECT: i32 = 70;
/// `ReleaseHealEffect()`
pub const EE_UNITS_CU_RELEASE_HEAL_EFFECT: i32 = 71;
/// `ClearTint()`
pub const EE_UNITS_CU_CLEAR_TINT: i32 = 72;
/// `ReleaseFireEffect()`
pub const EE_UNITS_CU_RELEASE_FIRE_EFFECT: i32 = 73;
/// `mGoalState = NULL`
pub const EE_UNITS_CU_CLEAR_GOAL_STATE: i32 = 74;
/// `mActiveGoal = NULL`
pub const EE_UNITS_CU_CLEAR_ACTIVE_GOAL: i32 = 75;
/// `EEUnit::Kill(b & 1, b & 2, b & 4)`
pub const EE_UNITS_CU_UNIT_KILL: i32 = 76;
/// `mCurrentResearch`
pub const EE_UNITS_CU_CURRENT_RESEARCH: i32 = 77;
/// `PlayEffect(b)`
pub const EE_UNITS_CU_PLAY_EFFECT: i32 = 78;
/// `gSoundManager->Stop(mSound, false)`
pub const EE_UNITS_CU_SOUND_STOP: i32 = 79;
/// `mSound`
pub const EE_UNITS_CU_SOUND: i32 = 80;
/// `mSound->Dereference()`
pub const EE_UNITS_CU_SOUND_DEREFERENCE: i32 = 81;
/// `mSound = NULL`
pub const EE_UNITS_CU_CLEAR_SOUND: i32 = 82;
/// `mCitizenBuildQueue.size()`
pub const EE_UNITS_CU_BUILD_TARGET_COUNT: i32 = 83;
/// `mCitizenBuildQueue[b]->Dereference()`
pub const EE_UNITS_CU_BUILD_TARGET_DEREFERENCE: i32 = 84;
/// `mCitizenBuildQueue.clear()`
pub const EE_UNITS_CU_BUILD_TARGETS_CLEAR: i32 = 85;
/// `mPatrolGroup`
pub const EE_UNITS_CU_PATROL_GROUP: i32 = 86;
/// `ee_units_pg_remove(mPatrolGroup, this)`
pub const EE_UNITS_CU_PATROL_REMOVE: i32 = 87;
/// `ee_units_pg_dereference(mPatrolGroup)`
pub const EE_UNITS_CU_PATROL_DEREFERENCE: i32 = 88;
/// `mPatrolGroup = NULL`
pub const EE_UNITS_CU_CLEAR_PATROL_GROUP: i32 = 89;
/// `mPreviousPart = NULL`
pub const EE_UNITS_CU_CLEAR_PREVIOUS_PART: i32 = 90;
/// `mNextPart = NULL`
pub const EE_UNITS_CU_CLEAR_NEXT_PART: i32 = 91;
/// `mHomeBase`
pub const EE_UNITS_CU_HOME_BASE: i32 = 92;
/// `mHomeBase = NULL`
pub const EE_UNITS_CU_CLEAR_HOME_BASE: i32 = 93;
/// `mContainer = NULL`
pub const EE_UNITS_CU_CLEAR_CONTAINER: i32 = 94;
/// `mMine`
pub const EE_UNITS_CU_MINE: i32 = 95;
/// `mMine = NULL`
pub const EE_UNITS_CU_CLEAR_MINE: i32 = 96;
/// `ClearHolder()`
pub const EE_UNITS_CU_CLEAR_HOLDER: i32 = 97;
/// `mIgnoredTargets.clear()`
pub const EE_UNITS_CU_IGNORED_CLEAR: i32 = 98;
/// `ReleaseSound()`
pub const EE_UNITS_CU_RELEASE_SOUND: i32 = 99;
/// `EEUnit::Remove(mIsFoundation)`
pub const EE_UNITS_CU_UNIT_REMOVE: i32 = 100;
/// `GetUnitClass()->IsLandBuilding()`
pub const EE_UNITS_CU_IS_LAND_BUILDING: i32 = 101;
/// `mIsFoundation`
pub const EE_UNITS_CU_FOUNDATION_FIELD: i32 = 102;
/// `mPlayer->UnloadUnitClassButtons(GetUnitClass()->GetUnitType())`
pub const EE_UNITS_CU_UNLOAD_BUTTONS: i32 = 103;
/// `mIsFoundation = false`
pub const EE_UNITS_CU_CLEAR_FOUNDATION: i32 = 104;
/// `mBuildQueue.front().mTechnology`
pub const EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY: i32 = 105;
/// `mBuildQueue.front().mFlag`
pub const EE_UNITS_CU_QUEUE_FRONT_FLAG: i32 = 106;
/// `GetPlayer()->GetTechTree()->Get(b)`
pub const EE_UNITS_CU_TECH_NODE: i32 = 107;
/// node `a`'s `GetIsAvailable()`
pub const EE_UNITS_CU_NODE_AVAILABLE: i32 = 108;
/// `GetPlayer()->CanProduce(b, GetUnitClass()->GetUnitType(), 1, false, 2, NULL)`
pub const EE_UNITS_CU_CAN_PRODUCE: i32 = 109;
/// node `a`'s `mData->mMakesUnit`
pub const EE_UNITS_CU_NODE_MAKES_UNIT: i32 = 110;
/// node `a`'s `mData->mUnitClass`
pub const EE_UNITS_CU_NODE_UNIT_CLASS: i32 = 111;
/// `gUnitClassTable[b]`
pub const EE_UNITS_CU_CLASS_TABLE: i32 = 112;
/// `GetPlayer()->GetUpgrade(b)->mBuildTimeScale` (its bits)
pub const EE_UNITS_CU_BUILD_TIME_SCALE: i32 = 113;
/// class `a`'s `IsAircraft()`
pub const EE_UNITS_CU_CLASS_IS_AIRCRAFT: i32 = 114;
/// class `a`'s `mWeight`
pub const EE_UNITS_CU_CLASS_WEIGHT: i32 = 115;
/// `ee_units_atc_cargo(GetPlayer()->mAirTraffic[class a's mAirTrafficType - 1], b)`
pub const EE_UNITS_CU_ATC_CARGO: i32 = 116;
/// class `a`'s `mUnitType`
pub const EE_UNITS_CU_CLASS_UNIT_TYPE: i32 = 117;
/// `gPhysics->mRandom.Generate()` (its bits; a draw)
pub const EE_UNITS_CU_RANDOM: i32 = 118;
/// class `a`'s `GetTechnology()`
pub const EE_UNITS_CU_CLASS_TECHNOLOGY: i32 = 119;
/// `GetPlayer()->mInstantBuild`
pub const EE_UNITS_CU_INSTANT_BUILD: i32 = 120;
/// node `a`'s `GetBuildTime()`
pub const EE_UNITS_CU_NODE_BUILD_TIME: i32 = 121;
/// `mProductionTime = b`
pub const EE_UNITS_CU_SET_PRODUCTION_TIME: i32 = 122;
/// `gUserInterface->UpdateJITManufacturing()`
pub const EE_UNITS_CU_UI_UPDATE_JIT: i32 = 123;
/// `mBuildQueue.erase(mBuildQueue.begin())`
pub const EE_UNITS_CU_QUEUE_POP_FRONT: i32 = 124;
/// `GetPlayer()->RefundTechnology(node a, GetUnitClass()->GetUnitType(), float of the bits b)`
pub const EE_UNITS_CU_REFUND_TECHNOLOGY: i32 = 125;
/// `mBuildQueue.empty()`
pub const EE_UNITS_CU_QUEUE_EMPTY: i32 = 126;
/// `dynamic_cast<EEPlayer*>(mPlayer)` (unit `a`'s: SetBuilt's parts)
pub const EE_UNITS_CU_EE_PLAYER: i32 = 127;
/// unit `a`'s `mIsBuilt`
pub const EE_UNITS_CU_PART_IS_BUILT: i32 = 128;
/// unit `a`'s `mUnitClass->IsLandBuilding()`
pub const EE_UNITS_CU_PART_IS_LAND_BUILDING: i32 = 129;
/// unit `a`'s `SightOff(its tile)`
pub const EE_UNITS_CU_PART_SIGHT_OFF: i32 = 130;
/// unit `a`'s `mIsBuilt = b != 0`
pub const EE_UNITS_CU_PART_SET_BUILT: i32 = 131;
/// unit `a`'s `SightOn(its tile)`
pub const EE_UNITS_CU_PART_SIGHT_ON: i32 = 132;
/// unit `a`'s `mUnitClass->mAirTrafficType`
pub const EE_UNITS_CU_PART_AIR_TRAFFIC_TYPE: i32 = 133;
/// `ee_units_atc_add_airport(player b's mAirTraffic[c - 1], unit a)`
pub const EE_UNITS_CU_PART_ATC_ADD_AIRPORT: i32 = 134;
/// player `b`'s `UnitCompleted(unit a)`
pub const EE_UNITS_CU_PART_UNIT_COMPLETED: i32 = 135;
/// unit `a`'s `mUnitClass->GetUnitType()`
pub const EE_UNITS_CU_PART_UNIT_TYPE: i32 = 136;
/// the replant message of unit `a` (`kMessageReplantFarms`)
pub const EE_UNITS_CU_PART_REPLANT: i32 = 137;
/// unit `a`'s `mPlayer->LoadUnitClassButtons(its type)`
pub const EE_UNITS_CU_PART_LOAD_BUTTONS: i32 = 138;
/// unit `a`'s `mIsPaidFor = true`
pub const EE_UNITS_CU_PART_PAID: i32 = 139;
/// unit `a`'s `mNextPart`
pub const EE_UNITS_CU_PART_NEXT: i32 = 140;
/// `mCommand != NULL`
pub const EE_UNITS_CU_HAS_COMMAND: i32 = 141;
/// `GetCommandType()`
pub const EE_UNITS_CU_COMMAND_TYPE: i32 = 142;
/// `UpdateCamouflage(b)`
pub const EE_UNITS_CU_UPDATE_CAMOUFLAGE: i32 = 143;
/// `mCommand = GetUnitClass()->FindAction(b)`
pub const EE_UNITS_CU_SET_COMMAND: i32 = 144;
/// `GetModel()`
pub const EE_UNITS_CU_MODEL: i32 = 145;
/// model `a`'s `mStartFrames[b]`
pub const EE_UNITS_CU_MODEL_START_FRAME: i32 = 146;
/// model `a`'s `mLastFrames[b]`
pub const EE_UNITS_CU_MODEL_LAST_FRAME: i32 = 147;
/// `mAnimation`
pub const EE_UNITS_CU_ANIMATION: i32 = 148;
/// `mAnimation = b`
pub const EE_UNITS_CU_SET_ANIMATION_FIELD: i32 = 149;
/// `mPhysicalModel.SetModel(model a's mModels[b])`
pub const EE_UNITS_CU_PM_SET_MODEL: i32 = 150;
/// `mPhysicalModel.SetFrame(b)`
pub const EE_UNITS_CU_PM_SET_FRAME: i32 = 151;
/// `mAnimationTime = 0`
pub const EE_UNITS_CU_CLEAR_ANIMATION_TIME: i32 = 152;
/// `mIsAnimationPaused = b != 0`
pub const EE_UNITS_CU_SET_PAUSED: i32 = 153;
/// `SetNextFrame(b)`
pub const EE_UNITS_CU_SET_NEXT_FRAME: i32 = 154;
/// `gSoundManager->Stop(mSound, !gSoundManager->GetIsLooping(mSound))`
pub const EE_UNITS_CU_SOUND_STOP_UNLESS_LOOPING: i32 = 155;
/// `mPhysicalModel.GetFrame()`
pub const EE_UNITS_CU_PM_FRAME: i32 = 156;
/// model `a`'s `mSound1Frames[b]`
pub const EE_UNITS_CU_MODEL_SOUND1_FRAME: i32 = 157;
/// model `a`'s `mSound2Frames[b]`
pub const EE_UNITS_CU_MODEL_SOUND2_FRAME: i32 = 158;
/// model `a`'s `mSounds1[b]`
pub const EE_UNITS_CU_MODEL_SOUND1: i32 = 159;
/// model `a`'s `mSounds2[b]`
pub const EE_UNITS_CU_MODEL_SOUND2: i32 = 160;
/// `GetUnitClass()->GetSoundParameters(this, sound a, &f[0], &f[1], &f[2])`
pub const EE_UNITS_CU_SOUND_PARAMETERS: i32 = 161;
/// `SetSound(gSoundManager->PlayAndReturnReferencedNode(sound a, f[0], f[1], f[2], its looping, the class name))`
pub const EE_UNITS_CU_PLAY_FRAME_SOUND: i32 = 162;
/// `mPlayer`
pub const EE_UNITS_CU_PLAYER: i32 = 163;
/// player `a`'s `mTechnologies->Get(GetUnitClass()->mTechnology)`
pub const EE_UNITS_CU_CLASS_TECH_NODE: i32 = 164;
/// player `a`'s `mInsuranceRefunds[b]`
pub const EE_UNITS_CU_REFUND_TOTAL: i32 = 165;
/// player `a`'s `mResourceCaps[b]`
pub const EE_UNITS_CU_RESOURCE: i32 = 166;
/// `mPlayer->RefundTechnology(node a, GetUnitClass()->GetUnitType(), float of the bits b)`
pub const EE_UNITS_CU_PLAYER_REFUND_TECHNOLOGY: i32 = 167;
/// player `a`'s `mInsuranceRefunds[b] = c`
pub const EE_UNITS_CU_SET_REFUND_TOTAL: i32 = 168;
/// `gWorld.GetCurrentPlayer()`
pub const EE_UNITS_CU_CURRENT_PLAYER: i32 = 169;
/// the disband emitter on the unit (`CreateUnitEffect(this, kGraphicEffectDisbandUnitEmitter, NULL, true, 0, 0)`)
pub const EE_UNITS_CU_DISBAND_EFFECT: i32 = 170;
/// `EESoundObject::Get(kSoundInsuranceCollected)`
pub const EE_UNITS_CU_INSURANCE_SOUND: i32 = 172;
/// sound `a`'s `Play(its GetVolume(), 0.0f, 1.0f)`
pub const EE_UNITS_CU_PLAY_AT_VOLUME: i32 = 173;
/// `GetUnitClass()->GetInnerCollisionSphere()` (bits)
pub const EE_UNITS_CU_INNER_SPHERE: i32 = 174;
/// class `a`'s `FliesOver()` (0: the unit's class)
pub const EE_UNITS_CU_FLIES_OVER: i32 = 175;
/// class `a`'s `IsAirCategory()` (0: the unit's class)
pub const EE_UNITS_CU_IS_AIR_CATEGORY: i32 = 176;
/// `mPhysicalModel.GetLocation().GetX()` (bits)
pub const EE_UNITS_CU_LOCATION_X: i32 = 177;
/// `mPhysicalModel.GetLocation().GetY()` (bits)
pub const EE_UNITS_CU_LOCATION_Y: i32 = 178;
/// `gWorld.mMap->GetTile(b, c)`
pub const EE_UNITS_CU_MAP_TILE: i32 = 179;
/// tile `a`'s `mUnits`
pub const EE_UNITS_CU_TILE_FIRST_NODE: i32 = 180;
/// tile node `a`'s `mNext`
pub const EE_UNITS_CU_NODE_NEXT: i32 = 181;
/// tile node `a`'s `mUnit->GetComplexUnit()`
pub const EE_UNITS_CU_NODE_COMPLEX: i32 = 182;
/// complex unit `a`'s `mIsCollidable`
pub const EE_UNITS_CU_OTHER_COLLIDABLE: i32 = 183;
/// complex unit `a`'s `mHitPoints`
pub const EE_UNITS_CU_OTHER_HIT_POINTS: i32 = 184;
/// complex unit `a`'s `IsFoundation()`
pub const EE_UNITS_CU_OTHER_IS_FOUNDATION: i32 = 185;
/// complex unit `a`'s `mUnitClass`
pub const EE_UNITS_CU_OTHER_CLASS: i32 = 186;
/// class `a`'s `mDomains` (0: the unit's class)
pub const EE_UNITS_CU_DOMAINS: i32 = 187;
/// class `a`'s `mCategory` (0: the unit's class)
pub const EE_UNITS_CU_CLASS_CATEGORY: i32 = 188;
/// complex unit `a`'s `AddContact(complex unit b)`
pub const EE_UNITS_CU_ADD_CONTACT: i32 = 189;

// ---- the bodies (`ee_units_cu_body`'s `what`)
pub const EE_UNITS_CU_DO_KILL: i32 = 1;
pub const EE_UNITS_CU_DO_INVALIDATE: i32 = 2;
pub const EE_UNITS_CU_DO_START_PRODUCTION: i32 = 3;
pub const EE_UNITS_CU_DO_SET_BUILT: i32 = 4;
pub const EE_UNITS_CU_DO_START_ACTION: i32 = 5;
pub const EE_UNITS_CU_DO_SET_ANIMATION: i32 = 6;
pub const EE_UNITS_CU_DO_PAY_INSURANCE: i32 = 7;
pub const EE_UNITS_CU_DO_FIND_CONTACTS: i32 = 8;

/// `kUnitTypePathPoint`, `kUnitTypeFarm`, `kUnitTypeCitizen`, `kUnitTypeFemaleCitizen`, the granaries
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_FARM: i64 = 0x2a;
const UNIT_TYPE_CITIZEN: i64 = 0x20;
const UNIT_TYPE_FEMALE_CITIZEN: i64 = 0x1e0;
const UNIT_TYPE_GRANARY: i64 = 0x34;
const UNIT_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
/// the building categories of the dead-unit statistics (building, building 19, gate, mining building, fortress)
const BUILDING_CATEGORIES: [i64; 5] = [2, 0x13, 0x19, 0x18, 0x1d];
/// `kUnitCategoryPlanetSpaceship`
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
/// `kMovementSpace`
const MOVEMENT_SPACE: i64 = 8;
/// `kTechNone`
const TECH_NONE: i64 = 100000;
/// `kActionDying`, `kActionKill` (the dying actions' range)
const ACTION_DYING: u32 = 6000;
const ACTION_KILL: u32 = 6005;
/// `kResourceRefundThresholds` (PayInsurance's notices)
const REFUND_THRESHOLDS: [i32; 11] = [100, 200, 300, 400, 500, 1000, 2000, 3000, 4000, 5000, 10000];
/// `kMinimumFootprint` (FindContacts)
const MINIMUM_FOOTPRINT: f32 = 1.0;

/// The C++ statements of the bodies.
pub trait CuHost {
    /// one statement (`EE_UNITS_CU_*`) on the complex unit `unit`
    fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64;
}

fn s<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64) -> i64 {
    h.cu(op, unit, 0, 0, 0, &mut [0.0; 4])
}

fn sa<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64) -> i64 {
    h.cu(op, unit, a, 0, 0, &mut [0.0; 4])
}

fn sb<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, b: i64) -> i64 {
    h.cu(op, unit, 0, b, 0, &mut [0.0; 4])
}

fn sab<H: CuHost + ?Sized>(h: &mut H, op: i32, unit: u64, a: u64, b: i64) -> i64 {
    h.cu(op, unit, a, b, 0, &mut [0.0; 4])
}

fn float(bits: i64) -> f32 {
    f32::from_bits(bits as u32)
}

fn bits(value: f32) -> i64 {
    i64::from(value.to_bits())
}

/// `UMFloor`: the nearest integer (`lrintf`), one lower when that is above `value` (compared as a float)
fn floor(value: f32) -> i32 {
    let rounded = ee_base::math::um::lrintf(value);
    if rounded as f32 > value {
        rounded.wrapping_sub(1)
    } else {
        rounded
    }
}

/// `Kill(killed, keep_research, other)`: the dying unit's world released (the statistics, the insurance, its
/// references, cargo, holder, paths, air traffic, parts, death and remains units, goals and effects), then
/// `EEUnit::Kill`.
pub fn kill<H: CuHost + ?Sized>(h: &mut H, unit: u64, killed: bool, keep_research: bool, other: bool) {
    let suppress = keep_research || other;
    if s(h, EE_UNITS_CU_HIT_POINTS, unit) == 0 {
        return;
    }
    sb(h, EE_UNITS_CU_UNIT_TAKEN, unit, i64::from(suppress));
    if killed && s(h, EE_UNITS_CU_WORLD_CLEARING, unit) == 0 {
        let set = sb(h, EE_UNITS_CU_UNIT_SET, unit, 0x92) as u64;
        if set != 0 && sa(h, EE_UNITS_CU_SET_HAS_TYPE, unit, set) != 0 {
            let mut i: u32 = 0;
            while i64::from(i as u16) < s(h, EE_UNITS_CU_NUM_PLAYERS, unit) {
                let player = sb(h, EE_UNITS_CU_PLAYER_AT, unit, i64::from(i)) as u64;
                if player != 0 && sa(h, EE_UNITS_CU_ENEMY_STATE, unit, player) == 1 {
                    sa(h, EE_UNITS_CU_UPDATE_FREE_UNIT_POWER, unit, player);
                }
                i = i.wrapping_add(1);
            }
            s(h, EE_UNITS_CU_DEBUG_DEAD_CITIZEN, unit);
        }
    }
    if s(h, EE_UNITS_CU_CLASS_INACTIVE, unit) != 0 {
        s(h, EE_UNITS_CU_ACTIVATE, unit);
    }
    let mut refund = true;
    if s(h, EE_UNITS_CU_PREVIOUS_PART, unit) == 0 && s(h, EE_UNITS_CU_SHUTDOWN, unit) == 0 {
        let state = s(h, EE_UNITS_CU_SWAP_KIND, unit) as i32;
        let special = (5..=8).contains(&state);
        if state != 1 {
            if killed || special {
                if s(h, EE_UNITS_CU_UNIT_TYPE, unit) != UNIT_TYPE_PATH_POINT && !special {
                    let category = s(h, EE_UNITS_CU_CATEGORY, unit);
                    let series = if BUILDING_CATEGORIES.contains(&category) { 1 } else { 4 };
                    sb(h, EE_UNITS_CU_STATS_RESET, unit, series);
                }
                let series = s(h, EE_UNITS_CU_ADD_DEAD_STATISTIC, unit);
                if series != 0 {
                    sb(h, EE_UNITS_CU_STATS_LOWER, unit, series);
                }
            } else {
                refund = false;
            }
        }
    }
    if refund
        && killed
        && !suppress
        && s(h, EE_UNITS_CU_HAS_INSURANCE, unit) != 0
        && s(h, EE_UNITS_CU_GET_INSURANCE, unit) != 0
        && s(h, EE_UNITS_CU_INSURANCE_COVERS, unit) != 0
        && (s(h, EE_UNITS_CU_UNIT_TYPE, unit) != UNIT_TYPE_FARM || s(h, EE_UNITS_CU_IS_BUILT, unit) != 0)
    {
        let amount = s(h, EE_UNITS_CU_INSURANCE_AMOUNT, unit);
        sb(h, EE_UNITS_CU_PAY_INSURANCE, unit, amount);
    }
    let attacker = s(h, EE_UNITS_CU_LAST_ATTACKER, unit) as u64;
    if attacker != 0 {
        sa(h, EE_UNITS_CU_DEREFERENCE, unit, attacker);
    }
    s(h, EE_UNITS_CU_CLEAR_LAST_ATTACKER, unit);
    let owner = s(h, EE_UNITS_CU_OWNER, unit) as u64;
    if owner != 0 {
        sa(h, EE_UNITS_CU_DEREFERENCE, unit, owner);
    }
    s(h, EE_UNITS_CU_CLEAR_OWNER, unit);
    s(h, EE_UNITS_CU_UPDATE_JIT, unit);
    crate::units::cu_world::clear_contacts(h, unit);
    s(h, EE_UNITS_CU_KILL_CARGO, unit);
    s(h, EE_UNITS_CU_CLEAR_GARRISON, unit);
    if s(h, EE_UNITS_CU_HOLDER, unit) != 0 {
        s(h, EE_UNITS_CU_HOLDER_RELEASE, unit);
        let holder = s(h, EE_UNITS_CU_HOLDER, unit) as u64;
        if holder != 0 {
            sa(h, EE_UNITS_CU_DEREFERENCE, unit, holder);
        }
        s(h, EE_UNITS_CU_CLEAR_HOLDER_FIELD, unit);
    } else {
        crate::units::cu_world::release_held_units(h, unit);
    }
    s(h, EE_UNITS_CU_CANCEL_PATHS, unit);
    s(h, EE_UNITS_CU_DROP_PATH, unit);
    let traffic = s(h, EE_UNITS_CU_AIR_TRAFFIC_TYPE, unit);
    if traffic != 0 {
        if s(h, EE_UNITS_CU_IS_AIR_BASE, unit) != 0 {
            sb(h, EE_UNITS_CU_ATC_REMOVE_AIRPORT, unit, traffic);
        } else {
            sb(h, EE_UNITS_CU_ATC_REMOVE_CARGO, unit, traffic);
        }
    }
    s(h, EE_UNITS_CU_CLEAR_COLLISION, unit);
    sb(h, EE_UNITS_CU_START_ACTION, unit, if killed { 6000 } else { 6003 });
    s(h, EE_UNITS_CU_ACTION_CHANGED, unit);
    let next = s(h, EE_UNITS_CU_NEXT_PART, unit) as u64;
    if next != 0 {
        sab(h, EE_UNITS_CU_KILL_PART, unit, next, i64::from(killed));
    }
    if !suppress {
        if s(h, EE_UNITS_CU_HAS_DEATH_SPAWN, unit) != 0
            && s(h, EE_UNITS_CU_WORLD_CLEARING, unit) == 0
            && s(h, EE_UNITS_CU_FREE_CAMERA, unit) == 0
        {
            s(h, EE_UNITS_CU_CREATE_DEATH_UNIT, unit);
        }
        if s(h, EE_UNITS_CU_HAS_REMAINS, unit) != 0
            && s(h, EE_UNITS_CU_IS_PROJECTILE, unit) == 0
            && s(h, EE_UNITS_CU_WORLD_CLEARING, unit) == 0
            && s(h, EE_UNITS_CU_FREE_CAMERA, unit) == 0
        {
            s(h, EE_UNITS_CU_CREATE_REMAINS_UNIT, unit);
        }
    }
    if s(h, EE_UNITS_CU_CONTAINER, unit) != 0 {
        s(h, EE_UNITS_CU_CONTAINER_RELEASE, unit);
    }
    s(h, EE_UNITS_CU_CLEAR_GATHERED, unit);
    crate::units::cu_world::set_blocking_unit(h, unit, 0);
    crate::units::cu_world::set_last_resource(h, unit, 0);
    if !keep_research {
        s(h, EE_UNITS_CU_CANCEL_QUEUED_PRODUCTION, unit);
    }
    clear_goal_queues(h, unit);
    for op in [
        EE_UNITS_CU_AFFECTING_KEEP_STRONGEST,
        EE_UNITS_CU_AFFECTING_CLEAR,
        EE_UNITS_CU_AFFECTING_ITEMS_CLEAR,
        EE_UNITS_CU_AREA_EFFECTS_CLEAR,
        EE_UNITS_CU_AREA_EFFECTS_ITEMS_CLEAR,
        EE_UNITS_CU_RELEASE_CONVERSION_EFFECT,
        EE_UNITS_CU_RELEASE_HEAL_EFFECT,
        EE_UNITS_CU_CLEAR_TINT,
        EE_UNITS_CU_RELEASE_FIRE_EFFECT,
        EE_UNITS_CU_CLEAR_GOAL_STATE,
        EE_UNITS_CU_CLEAR_ACTIVE_GOAL,
    ] {
        s(h, op, unit);
    }
    let flags = i64::from(killed) | i64::from(keep_research) << 1 | i64::from(other) << 2;
    sb(h, EE_UNITS_CU_UNIT_KILL, unit, flags);
}

/// `for (i = 0; i < mGoalQueues.size(); i++) ClearGoalQueue(i)` (the size asked each pass)
fn clear_goal_queues<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let mut i: i64 = 0;
    while i < s(h, EE_UNITS_CU_GOAL_QUEUE_COUNT, unit) {
        sb(h, EE_UNITS_CU_CLEAR_GOAL_QUEUE, unit, i);
        i += 1;
    }
}

/// A pointer member's `if (m) m->Dereference(); m = NULL;`.
fn release<H: CuHost + ?Sized>(h: &mut H, unit: u64, get: i32, clear: i32) {
    let object = s(h, get, unit) as u64;
    if object != 0 {
        sa(h, EE_UNITS_CU_DEREFERENCE, unit, object);
    }
    s(h, clear, unit);
}

/// A pointer member's `if (m) { m->Dereference(); m = NULL; }`.
fn release_set<H: CuHost + ?Sized>(h: &mut H, unit: u64, get: i32, clear: i32) {
    let object = s(h, get, unit) as u64;
    if object != 0 {
        sa(h, EE_UNITS_CU_DEREFERENCE, unit, object);
        s(h, clear, unit);
    }
}

/// `Invalidate()`: the unit removed from play (its path, goals, queues, sound and every unit reference it holds),
/// then `EEUnit::Remove` and the class's buttons.
pub fn invalidate<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    if s(h, EE_UNITS_CU_CURRENT_RESEARCH, unit) != TECH_NONE {
        sb(h, EE_UNITS_CU_PLAY_EFFECT, unit, 100000);
    }
    s(h, EE_UNITS_CU_DROP_PATH, unit);
    s(h, EE_UNITS_CU_SOUND_STOP, unit);
    if s(h, EE_UNITS_CU_SOUND, unit) != 0 {
        s(h, EE_UNITS_CU_SOUND_DEREFERENCE, unit);
    }
    s(h, EE_UNITS_CU_CLEAR_SOUND, unit);
    clear_goal_queues(h, unit);
    let mut j: i64 = 0;
    while j < s(h, EE_UNITS_CU_BUILD_TARGET_COUNT, unit) {
        sb(h, EE_UNITS_CU_BUILD_TARGET_DEREFERENCE, unit, j);
        j += 1;
    }
    s(h, EE_UNITS_CU_BUILD_TARGETS_CLEAR, unit);
    release(h, unit, EE_UNITS_CU_OWNER, EE_UNITS_CU_CLEAR_OWNER);
    release(h, unit, EE_UNITS_CU_LAST_ATTACKER, EE_UNITS_CU_CLEAR_LAST_ATTACKER);
    if s(h, EE_UNITS_CU_PATROL_GROUP, unit) != 0 {
        s(h, EE_UNITS_CU_PATROL_REMOVE, unit);
        if s(h, EE_UNITS_CU_PATROL_GROUP, unit) != 0 {
            s(h, EE_UNITS_CU_PATROL_DEREFERENCE, unit);
            s(h, EE_UNITS_CU_CLEAR_PATROL_GROUP, unit);
        }
    }
    release_set(h, unit, EE_UNITS_CU_PREVIOUS_PART, EE_UNITS_CU_CLEAR_PREVIOUS_PART);
    release_set(h, unit, EE_UNITS_CU_NEXT_PART, EE_UNITS_CU_CLEAR_NEXT_PART);
    release_set(h, unit, EE_UNITS_CU_HOME_BASE, EE_UNITS_CU_CLEAR_HOME_BASE);
    release_set(h, unit, EE_UNITS_CU_CONTAINER, EE_UNITS_CU_CLEAR_CONTAINER);
    release_set(h, unit, EE_UNITS_CU_MINE, EE_UNITS_CU_CLEAR_MINE);
    release(h, unit, EE_UNITS_CU_OWNER, EE_UNITS_CU_CLEAR_OWNER);
    release(h, unit, EE_UNITS_CU_LAST_ATTACKER, EE_UNITS_CU_CLEAR_LAST_ATTACKER);
    crate::units::cu_world::clear_contacts(h, unit);
    s(h, EE_UNITS_CU_CLEAR_ACTIVE_GOAL, unit);
    s(h, EE_UNITS_CU_CLEAR_GOAL_STATE, unit);
    if s(h, EE_UNITS_CU_HOLDER, unit) != 0 {
        s(h, EE_UNITS_CU_HOLDER_RELEASE, unit);
        s(h, EE_UNITS_CU_CLEAR_HOLDER, unit);
    } else {
        crate::units::cu_world::release_held_units(h, unit);
    }
    s(h, EE_UNITS_CU_IGNORED_CLEAR, unit);
    if s(h, EE_UNITS_CU_SOUND, unit) != 0 {
        s(h, EE_UNITS_CU_SOUND_STOP, unit);
        s(h, EE_UNITS_CU_RELEASE_SOUND, unit);
    }
    s(h, EE_UNITS_CU_UNIT_REMOVE, unit);
    if s(h, EE_UNITS_CU_IS_LAND_BUILDING, unit) == 0
        || s(h, EE_UNITS_CU_IS_BUILT, unit) != 0
        || ((s(h, EE_UNITS_CU_WORLD_CLEARING, unit) != 0 || s(h, EE_UNITS_CU_SHUTDOWN, unit) != 0)
            && s(h, EE_UNITS_CU_FOUNDATION_FIELD, unit) != 0)
    {
        s(h, EE_UNITS_CU_UNLOAD_BUTTONS, unit);
    }
    s(h, EE_UNITS_CU_CLEAR_FOUNDATION, unit);
}

/// `StartProduction()`: the build queue's first available entry started (its effect, the production time, the
/// JIT notice); unavailable entries are dropped and refunded.
pub fn start_production<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let mut time_scale: f32 = 1.0;
    loop {
        if s(h, EE_UNITS_CU_IS_BUILT, unit) == 0 {
            return;
        }
        let mut technology = s(h, EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY, unit);
        let flags = s(h, EE_UNITS_CU_QUEUE_FRONT_FLAG, unit);
        let mut node = sb(h, EE_UNITS_CU_TECH_NODE, unit, technology) as u64;
        if sa(h, EE_UNITS_CU_NODE_AVAILABLE, unit, node) != 0 {
            let mut allowed = sb(h, EE_UNITS_CU_CAN_PRODUCE, unit, technology) != 0;
            if !allowed {
                return;
            }
            if sa(h, EE_UNITS_CU_NODE_MAKES_UNIT, unit, node) != 0 {
                let kind = sa(h, EE_UNITS_CU_NODE_UNIT_CLASS, unit, node);
                let class = sb(h, EE_UNITS_CU_CLASS_TABLE, unit, kind) as u64;
                time_scale = float(sb(h, EE_UNITS_CU_BUILD_TIME_SCALE, unit, kind));
                if sa(h, EE_UNITS_CU_CLASS_IS_AIRCRAFT, unit, class) != 0 {
                    let weight = sa(h, EE_UNITS_CU_CLASS_WEIGHT, unit, class) as i32;
                    let capacity = sab(h, EE_UNITS_CU_ATC_CARGO, unit, class, 0) as i32;
                    let used = sab(h, EE_UNITS_CU_ATC_CARGO, unit, class, 1) as i32;
                    allowed = weight <= capacity.wrapping_sub(used);
                }
            }
            if !allowed {
                return;
            }
            if sa(h, EE_UNITS_CU_NODE_MAKES_UNIT, unit, node) != 0 {
                let kind = sa(h, EE_UNITS_CU_NODE_UNIT_CLASS, unit, node);
                let class = sb(h, EE_UNITS_CU_CLASS_TABLE, unit, kind) as u64;
                if sa(h, EE_UNITS_CU_CLASS_UNIT_TYPE, unit, class) == UNIT_TYPE_CITIZEN
                    && float(s(h, EE_UNITS_CU_RANDOM, unit)) >= 0.5
                {
                    let other = sb(h, EE_UNITS_CU_CLASS_TABLE, unit, UNIT_TYPE_FEMALE_CITIZEN) as u64;
                    if other != 0 {
                        let female = sa(h, EE_UNITS_CU_CLASS_TECHNOLOGY, unit, other);
                        if sb(h, EE_UNITS_CU_TECH_NODE, unit, female) != 0 {
                            let female = sa(h, EE_UNITS_CU_CLASS_TECHNOLOGY, unit, other);
                            let available = sb(h, EE_UNITS_CU_TECH_NODE, unit, female) as u64;
                            if sa(h, EE_UNITS_CU_NODE_AVAILABLE, unit, available) != 0 {
                                technology = sa(h, EE_UNITS_CU_CLASS_TECHNOLOGY, unit, other);
                            }
                        }
                    }
                }
            }
            sb(h, EE_UNITS_CU_PLAY_EFFECT, unit, technology);
            let time = if s(h, EE_UNITS_CU_INSTANT_BUILD, unit) == 0 {
                if flags & 1 == 0 || sa(h, EE_UNITS_CU_NODE_MAKES_UNIT, unit, node) == 0 {
                    let build = sa(h, EE_UNITS_CU_NODE_BUILD_TIME, unit, node) as i32;
                    ee_base::math::um::lrintf(build as f32 * time_scale)
                } else {
                    2
                }
            } else {
                1
            };
            sb(h, EE_UNITS_CU_SET_PRODUCTION_TIME, unit, i64::from(time));
            if sa(h, EE_UNITS_CU_NODE_UNIT_CLASS, unit, node) as i32 <= 0 {
                s(h, EE_UNITS_CU_UI_UPDATE_JIT, unit);
            }
            s(h, EE_UNITS_CU_QUEUE_POP_FRONT, unit);
            return;
        }
        while sa(h, EE_UNITS_CU_NODE_AVAILABLE, unit, node) == 0 {
            // every dropped entry is refunded, the last one too
            s(h, EE_UNITS_CU_QUEUE_POP_FRONT, unit);
            if sa(h, EE_UNITS_CU_NODE_UNIT_CLASS, unit, node) as i32 <= 0 {
                s(h, EE_UNITS_CU_UI_UPDATE_JIT, unit);
            }
            sab(h, EE_UNITS_CU_REFUND_TECHNOLOGY, unit, node, bits(1.0));
            if s(h, EE_UNITS_CU_QUEUE_EMPTY, unit) != 0 {
                break;
            }
            let front = s(h, EE_UNITS_CU_QUEUE_FRONT_TECHNOLOGY, unit);
            node = sb(h, EE_UNITS_CU_TECH_NODE, unit, front) as u64;
        }
        if s(h, EE_UNITS_CU_QUEUE_EMPTY, unit) != 0 {
            return;
        }
    }
}

/// `SetBuilt(built, update_sight, load_buttons)` over the unit and its parts (its `long` arguments as the C++ takes
/// them: tested for zero, `load_buttons` against 1).
pub fn set_built<H: CuHost + ?Sized>(h: &mut H, unit: u64, built: i64, update_sight: i64, load_buttons: i64) {
    let (built, update_sight) = (built as std::os::raw::c_long != 0, update_sight as std::os::raw::c_long != 0);
    let mut part = unit;
    loop {
        let player = sa(h, EE_UNITS_CU_EE_PLAYER, unit, part);
        if built && sa(h, EE_UNITS_CU_PART_IS_BUILT, unit, part) == 0 {
            if sa(h, EE_UNITS_CU_PART_IS_LAND_BUILDING, unit, part) != 0 && update_sight {
                sa(h, EE_UNITS_CU_PART_SIGHT_OFF, unit, part);
            }
            sab(h, EE_UNITS_CU_PART_SET_BUILT, unit, part, i64::from(built));
            if update_sight {
                sa(h, EE_UNITS_CU_PART_SIGHT_ON, unit, part);
            }
            if player != 0 {
                let traffic = sa(h, EE_UNITS_CU_PART_AIR_TRAFFIC_TYPE, unit, part);
                if traffic != 0 {
                    h.cu(EE_UNITS_CU_PART_ATC_ADD_AIRPORT, unit, part, player, traffic, &mut [0.0; 4]);
                }
                sab(h, EE_UNITS_CU_PART_UNIT_COMPLETED, unit, part, player);
                let kind = sa(h, EE_UNITS_CU_PART_UNIT_TYPE, unit, part);
                if kind == UNIT_TYPE_GRANARY_IMPROVED || kind == UNIT_TYPE_GRANARY {
                    sa(h, EE_UNITS_CU_PART_REPLANT, unit, part);
                }
            }
            if load_buttons == 1 {
                sa(h, EE_UNITS_CU_PART_LOAD_BUTTONS, unit, part);
            }
        } else {
            sab(h, EE_UNITS_CU_PART_SET_BUILT, unit, part, i64::from(built));
        }
        if built {
            sa(h, EE_UNITS_CU_PART_PAID, unit, part);
        }
        part = sa(h, EE_UNITS_CU_PART_NEXT, unit, part) as u64;
        if part == 0 {
            return;
        }
    }
}

/// `StartAction(action)`: a dying unit (not a land building) refuses any non-dying action; else the camouflage
/// and the class's action.
pub fn start_action<H: CuHost + ?Sized>(h: &mut H, unit: u64, action: i64) {
    if s(h, EE_UNITS_CU_HAS_COMMAND, unit) != 0 && s(h, EE_UNITS_CU_IS_LAND_BUILDING, unit) == 0 {
        let current = s(h, EE_UNITS_CU_COMMAND_TYPE, unit) as u32;
        let wanted = action as u32;
        if (ACTION_DYING..=ACTION_KILL).contains(&current) && !(ACTION_DYING..=ACTION_KILL).contains(&wanted) {
            return;
        }
    }
    sb(h, EE_UNITS_CU_UPDATE_CAMOUFLAGE, unit, action);
    sb(h, EE_UNITS_CU_SET_COMMAND, unit, action);
}

/// `SetAnimation(part, frame, paused)`: the animation and frame switched (not twice the same death or idle), the
/// sound stopped, the new frame's sound played.
pub fn set_animation<H: CuHost + ?Sized>(h: &mut H, unit: u64, part: i64, frame: i64, paused: bool) {
    let model = s(h, EE_UNITS_CU_MODEL, unit) as u64;
    let frame = if frame == -1 { sab(h, EE_UNITS_CU_MODEL_START_FRAME, unit, model, part) } else { frame };
    let animation = s(h, EE_UNITS_CU_ANIMATION, unit);
    if animation == 0xd && part == animation {
        return;
    }
    if animation == 2 && part == 2 {
        return;
    }
    sb(h, EE_UNITS_CU_SET_ANIMATION_FIELD, unit, part);
    if s(h, EE_UNITS_CU_FOUNDATION_FIELD, unit) == 0 {
        sab(h, EE_UNITS_CU_PM_SET_MODEL, unit, model, part);
        sb(h, EE_UNITS_CU_PM_SET_FRAME, unit, frame);
    }
    s(h, EE_UNITS_CU_CLEAR_ANIMATION_TIME, unit);
    sb(h, EE_UNITS_CU_SET_PAUSED, unit, i64::from(paused));
    let next = if frame == sab(h, EE_UNITS_CU_MODEL_LAST_FRAME, unit, model, part) {
        sab(h, EE_UNITS_CU_MODEL_START_FRAME, unit, model, part)
    } else {
        frame.wrapping_add(1)
    };
    sb(h, EE_UNITS_CU_SET_NEXT_FRAME, unit, next);
    if s(h, EE_UNITS_CU_SOUND, unit) != 0 {
        s(h, EE_UNITS_CU_SOUND_STOP_UNLESS_LOOPING, unit);
        s(h, EE_UNITS_CU_RELEASE_SOUND, unit);
    }
    if paused {
        return;
    }
    let current = s(h, EE_UNITS_CU_PM_FRAME, unit);
    let sound = if current == sab(h, EE_UNITS_CU_MODEL_SOUND1_FRAME, unit, model, part) {
        sab(h, EE_UNITS_CU_MODEL_SOUND1, unit, model, part)
    } else if current == sab(h, EE_UNITS_CU_MODEL_SOUND2_FRAME, unit, model, part) {
        sab(h, EE_UNITS_CU_MODEL_SOUND2, unit, model, part)
    } else {
        return;
    } as u64;
    if sound == 0 {
        return;
    }
    let mut f = [0.0; 4];
    h.cu(EE_UNITS_CU_SOUND_PARAMETERS, unit, sound, 0, 0, &mut f);
    h.cu(EE_UNITS_CU_PLAY_FRAME_SOUND, unit, sound, 0, 0, &mut f);
    if s(h, EE_UNITS_CU_SOUND, unit) != 0 {
        s(h, EE_UNITS_CU_SOUND_DEREFERENCE, unit);
    }
}

/// `PayInsurance(fraction)`: the class's technology refunded to the player by `fraction`, its five resources'
/// insurance totals raised by what came back; the current player sees the emitter and a notice per resource
/// whose total grew (past 20, only when it passes one of the thresholds, the highest passed shown).
#[allow(clippy::needless_range_loop)] // the original's index loops over its two copies
pub fn pay_insurance<H: CuHost + ?Sized>(h: &mut H, unit: u64, fraction: f32) {
    let player = s(h, EE_UNITS_CU_PLAYER, unit) as u64;
    let node = sa(h, EE_UNITS_CU_CLASS_TECH_NODE, unit, player) as u64;
    if node == 0 {
        return;
    }
    let mut old_totals = [0i32; 5];
    let mut old_resources = [0i32; 5];
    for i in 0..5 {
        old_totals[i] = sab(h, EE_UNITS_CU_REFUND_TOTAL, unit, player, i as i64) as i32;
    }
    for i in 0..5 {
        old_resources[i] = sab(h, EE_UNITS_CU_RESOURCE, unit, player, i as i64 + 2) as i32;
    }
    sab(h, EE_UNITS_CU_PLAYER_REFUND_TECHNOLOGY, unit, node, bits(fraction));
    for i in 0..5 {
        let total = sab(h, EE_UNITS_CU_REFUND_TOTAL, unit, player, i as i64) as i32;
        let resource = sab(h, EE_UNITS_CU_RESOURCE, unit, player, i as i64 + 2) as i32;
        let value = total.wrapping_add(resource.wrapping_sub(old_resources[i]));
        h.cu(EE_UNITS_CU_SET_REFUND_TOTAL, unit, player, i as i64, i64::from(value), &mut [0.0; 4]);
    }
    let player = s(h, EE_UNITS_CU_PLAYER, unit) as u64;
    if player != s(h, EE_UNITS_CU_CURRENT_PLAYER, unit) as u64 {
        return;
    }
    s(h, EE_UNITS_CU_DISBAND_EFFECT, unit);
    for resource in 2..7i64 {
        let k = (resource - 2) as usize;
        let value = sab(h, EE_UNITS_CU_REFUND_TOTAL, unit, player, resource - 2) as i32;
        if value == old_totals[k] {
            continue;
        }
        let mut shown = value;
        if value >= 20 {
            let mut notify = false;
            for threshold in REFUND_THRESHOLDS {
                if old_totals[k] < threshold && threshold <= value {
                    notify = true;
                    shown = threshold;
                }
            }
            if !notify {
                continue;
            }
        }
        crate::units::cu_small::insurance_chat(h, unit, player, resource, i64::from(shown));
        let sound = s(h, EE_UNITS_CU_INSURANCE_SOUND, unit) as u64;
        if sound != 0 {
            sa(h, EE_UNITS_CU_PLAY_AT_VOLUME, unit, sound);
        }
    }
}

/// `FindContacts()`: the living collidable units under the unit's footprint in the same layer (fliers with
/// fliers, the air category with itself, the same sea and space domains, no planet spaceship in space) become
/// mutual contacts.
pub fn find_contacts<H: CuHost + ?Sized>(h: &mut H, unit: u64) {
    let size = float(s(h, EE_UNITS_CU_INNER_SPHERE, unit));
    let mut radius = size;
    let flies = sa(h, EE_UNITS_CU_FLIES_OVER, unit, 0) != 0;
    let air = sa(h, EE_UNITS_CU_IS_AIR_CATEGORY, unit, 0) != 0;
    let x = float(s(h, EE_UNITS_CU_LOCATION_X, unit));
    let y = float(s(h, EE_UNITS_CU_LOCATION_Y, unit));
    if size >= MINIMUM_FOOTPRINT {
        radius = size + size;
    }
    // `theRadius * 0.5` and `theRadius < 0.5` in double, stored back to the float
    radius = (f64::from(radius) * 0.5) as f32;
    if f64::from(radius) < 0.5 {
        radius = 0.5;
    }
    let (min_x, max_x) = (floor(x - radius), floor(x + radius));
    let (min_y, max_y) = (floor(y - radius), floor(y + radius));
    let domains = sa(h, EE_UNITS_CU_DOMAINS, unit, 0);
    let category = sa(h, EE_UNITS_CU_CLASS_CATEGORY, unit, 0);
    for tx in min_x..=max_x {
        for ty in min_y..=max_y {
            let tile = h.cu(EE_UNITS_CU_MAP_TILE, unit, 0, i64::from(tx), i64::from(ty), &mut [0.0; 4]) as u64;
            if tile == 0 {
                continue;
            }
            let mut node = sa(h, EE_UNITS_CU_TILE_FIRST_NODE, unit, tile) as u64;
            while node != 0 {
                let other = sa(h, EE_UNITS_CU_NODE_COMPLEX, unit, node) as u64;
                let next = |h: &mut H| sa(h, EE_UNITS_CU_NODE_NEXT, unit, node) as u64;
                if other == 0
                    || other == unit
                    || sa(h, EE_UNITS_CU_OTHER_COLLIDABLE, unit, other) == 0
                    || sa(h, EE_UNITS_CU_OTHER_HIT_POINTS, unit, other) <= 0
                    || sa(h, EE_UNITS_CU_OTHER_IS_FOUNDATION, unit, other) != 0
                {
                    node = next(h);
                    continue;
                }
                let class = sa(h, EE_UNITS_CU_OTHER_CLASS, unit, other) as u64;
                let same_layer = flies == (sa(h, EE_UNITS_CU_FLIES_OVER, unit, class) != 0)
                    && air == (sa(h, EE_UNITS_CU_IS_AIR_CATEGORY, unit, class) != 0)
                    && ((domains ^ sa(h, EE_UNITS_CU_DOMAINS, unit, class)) & 0xc) == 0
                    && !((domains & MOVEMENT_SPACE) != 0
                        && (category == CATEGORY_PLANET_SPACESHIP
                            || sa(h, EE_UNITS_CU_CLASS_CATEGORY, unit, class) == CATEGORY_PLANET_SPACESHIP));
                if same_layer {
                    sab(h, EE_UNITS_CU_ADD_CONTACT, unit, unit, other as i64);
                    sab(h, EE_UNITS_CU_ADD_CONTACT, unit, other, unit as i64);
                }
                node = next(h);
            }
        }
    }
}

/// Runs body `what` on the complex unit `unit` (`a`, `b`, `c` its arguments as its C++ member takes them; a float
/// as its bits).
pub fn run<H: crate::units::cu_cargo::CuCargoHost + ?Sized>(
    h: &mut H,
    what: i32,
    unit: u64,
    a: i64,
    b: i64,
    c: i64,
) -> i64 {
    match what {
        EE_UNITS_CU_DO_KILL => kill(h, unit, a & 1 != 0, a & 2 != 0, a & 4 != 0),
        EE_UNITS_CU_DO_INVALIDATE => invalidate(h, unit),
        EE_UNITS_CU_DO_START_PRODUCTION => start_production(h, unit),
        EE_UNITS_CU_DO_SET_BUILT => set_built(h, unit, a, b, c),
        EE_UNITS_CU_DO_START_ACTION => start_action(h, unit, a),
        EE_UNITS_CU_DO_SET_ANIMATION => set_animation(h, unit, a, b, c != 0),
        EE_UNITS_CU_DO_PAY_INSURANCE => pay_insurance(h, unit, float(a)),
        EE_UNITS_CU_DO_FIND_CONTACTS => find_contacts(h, unit),
        _ => {
            return crate::units::cu_cargo::run(h, what, unit, a, b, c)
                .or_else(|| crate::units::cu_life::run(h, what, unit, a, b, c))
                .unwrap_or(0)
        }
    }
    0
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_cu_host(op: i32, unit: *mut c_void, a: u64, b: i64, c: i64, f: *mut [f32; 4]) -> i64;
    }

    /// the game's host (the other cu_* modules' extern functions use it too)
    pub(crate) struct Game;

    impl crate::units::cu_cargo::CuCargoHost for Game {
        fn find_placement(&mut self, p: &crate::units::cu_cargo::Placement, m: &mut [[f32; 4]; 4]) -> bool {
            let origin = [0.0f32; 3];
            // SAFETY: the buffers are live for the call; the unit is the body's cargo, live
            unsafe {
                crate::units::uu_ffi::ee_units_uu_find_placement(
                    &origin,
                    p.unit,
                    0,
                    p.x,
                    p.y,
                    p.size,
                    p.rings,
                    u8::from(p.corners),
                    u8::from(p.include_center),
                    u8::from(p.face_point),
                    u8::from(p.linked),
                    m,
                    0,
                ) != 0
            }
        }
    }

    impl CuHost for Game {
        fn cu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) = crate::units::tech_cases::cu(op, unit, a, b) {
                return v;
            }
            // SAFETY: the body runs inside the C++ member that handed over its live unit (the plain record reads
            // and stores first: cu_view.rs)
            unsafe {
                if let Some(v) = crate::units::cu_view::cu_view(op, unit, a, b, c, f) {
                    return v;
                }
                ee_units_cu_host(op, unit as usize as *mut c_void, a, b, c, f)
            }
        }
    }

    /// `EEComplexUnit`'s body `what` (`EE_UNITS_CU_DO_*`) on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit`, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_cu_body(what: i32, unit: *mut c_void, a: i64, b: i64, c: i64) -> i64 {
        run(&mut Game, what, unit as usize as u64, a, b, c)
    }
}

#[cfg(test)]
#[path = "cu_body_tests.rs"]
mod tests;
