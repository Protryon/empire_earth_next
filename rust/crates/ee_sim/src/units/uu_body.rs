//! The unit utilities' bodies over their C++ statements (the units' lane, round 7; were
//! EEFOFindNewTargetNearOldTarget.cpp): `EEConvertUnit` (with the adaptation power's grant and its upgrade text,
//! `EEFn24E68F`), `EEReplaceUnit`, `EEMakeUnitCopy`, `EEMakeBuilding`, `EEReplaceQueuedTechnology`, `EEWorkFarm`,
//! `EERetargetAttack`, `EEFindNewTargetNearOld`, `EEGetHelicopterPriestDestination`, `EEUpdateUnitTile`,
//! `EEGetWaitingGatherer`, `EEUpgradeHitPoints` and its functor's `Apply`, `EEForEachUnitOf`,
//! `EEForEachPlayersUnit`, `EEMovePassengers`, `EETakeOverLoad`, `EEGetPlayerClassGraphic`, `EEFindTargetNear`,
//! `EEFindNearbyTarget`, `EECanBallisticHit`, `EECanPriestConvert`, `EEPreviewWall`, the space walls' energy walls,
//! `EEHasCivilizationUpgrade`. Every C++ statement of the originals is one operation of [`UbHost::ub`] (the game's:
//! `ee_units_ub_host`, EEFOFindNewTargetNearOldTarget.cpp, one statement a case), asked in the original's order; the
//! choices, loops and arithmetic are here. `a`, `b` objects (a unit as its `EEUnit*`), `c`, `d`, `e` values, `f`
//! floats in and out.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uu_note::Note;

// ---- the statements (`op`; the answer)
/// `gWorld.mPlayers[c]`
pub const EE_UNITS_UB_PLAYER_AT: i32 = 1;
/// unit `a`'s `GetUnitClass()`
pub const EE_UNITS_UB_UNIT_CLASS: i32 = 2;
/// class `a`'s `mUnitType`
pub const EE_UNITS_UB_CLASS_TYPE: i32 = 3;
/// `gCivPowersManager.HasPower(player a, c)`
pub const EE_UNITS_UB_HAS_POWER: i32 = 4;
/// unit `a`'s `GetComplexUnit()->mIsBuilt`
pub const EE_UNITS_UB_IS_BUILT: i32 = 5;
/// the adaptation's strings (format, message, name) emptied
pub const EE_UNITS_UB_STRINGS_RESET: i32 = 6;
/// `gCivPowersManager.GetNumPowers()`
pub const EE_UNITS_UB_NUM_POWERS: i32 = 7;
/// `gCivPowersManager.HasPower(unit a's GetPlayer(), c)`
pub const EE_UNITS_UB_UNIT_HAS_POWER: i32 = 8;
/// `gCivilizationManager.MakePowerUpgrade(EECivilizationManager::PowerLevel(c))`
pub const EE_UNITS_UB_MAKE_POWER_UPGRADE: i32 = 9;
/// `gCivPowersManager.GetName(c, the name)`
pub const EE_UNITS_UB_POWER_NAME: i32 = 10;
/// the upgrade list = unit `a`'s player's `mCivilization.GetPurchasedUpgrades()` (a copy)
pub const EE_UNITS_UB_UPGRADES_COPY: i32 = 11;
/// `EEAIRandomShuffle` of the upgrade list
pub const EE_UNITS_UB_UPGRADES_SHUFFLE: i32 = 12;
/// the upgrade list's size
pub const EE_UNITS_UB_UPGRADES_COUNT: i32 = 13;
/// `EEHasCivilizationUpgrade(&player a->mCivilization, c)`
pub const EE_UNITS_UB_HAS_UPGRADE: i32 = 14;
/// the upgrade list[c]
pub const EE_UNITS_UB_UPGRADE_AT: i32 = 15;
/// `EEFn24E68F(c, the name)`
pub const EE_UNITS_UB_UPGRADE_NAME: i32 = 16;
/// player `a`'s `mCivilization.GetAvailablePoints()`
pub const EE_UNITS_UB_AVAILABLE_POINTS: i32 = 17;
/// player `a`'s `mCivilization.CanAfford(c, &the cost, NULL)`
pub const EE_UNITS_UB_CAN_AFFORD: i32 = 18;
/// player `a`'s `mCivilization.SetPointsAndMax(GetMaxPoints() + the cost)`
pub const EE_UNITS_UB_SET_POINTS_AND_MAX: i32 = 19;
/// player `a`'s `mCivilization.Buy(c, GetPlayerID(), true)`
pub const EE_UNITS_UB_BUY: i32 = 20;
/// player `a`'s `mCivilization.SetPoints(c)`
pub const EE_UNITS_UB_SET_POINTS: i32 = 21;
/// `gWorld.GetCurrentPlayer()`
pub const EE_UNITS_UB_CURRENT_PLAYER: i32 = 22;
/// `gResource->GetString(IDS_ACQUIRED_THROUGH_ADAPTATION, the format)`
pub const EE_UNITS_UB_ADAPTATION_FORMAT: i32 = 23;
/// `sprintf(the message, the format, the name)`
pub const EE_UNITS_UB_ADAPTATION_MESSAGE: i32 = 24;
/// the adaptation's chat line in unit `a`'s player's colour and name
pub const EE_UNITS_UB_ADAPTATION_CHAT: i32 = 25;
/// `gUnitClassTable[c]`
pub const EE_UNITS_UB_CLASS_OF_TYPE: i32 = 26;
/// class `a`'s `IsCrusader()`
pub const EE_UNITS_UB_IS_CRUSADER: i32 = 27;
/// the bits of `(float)unit a's GetHitPoints() / (float)its mStats->mMaxHitPoints`
pub const EE_UNITS_UB_HIT_FRACTION: i32 = 28;
/// unit `a`'s `GetPlayer()->DeselectUnit(GetComplexUnit())`
pub const EE_UNITS_UB_DESELECT: i32 = 29;
/// unit `a`'s `GetPlayer()->mGroups[c]->Contains(GetComplexUnit())`
pub const EE_UNITS_UB_GROUP_CONTAINS: i32 = 30;
/// unit `a`'s `GetPlayer()->mGroups[c]->RemoveUnit(GetComplexUnit())`
pub const EE_UNITS_UB_GROUP_REMOVE: i32 = 31;
/// player `a`'s `SetUnitClassMedia(unit b's GetUnitClass()->mUnitType, 0x7f)`
pub const EE_UNITS_UB_SET_CLASS_MEDIA: i32 = 32;
/// player `a`'s `UnloadUnitClassMedia(unit b's GetUnitClass()->mUnitType)`
pub const EE_UNITS_UB_UNLOAD_CLASS_MEDIA: i32 = 33;
/// unit `a`'s `GetComplexUnit()`
pub const EE_UNITS_UB_COMPLEX_OF: i32 = 34;
/// complex unit `a`'s `ClearGarrison()`
pub const EE_UNITS_UB_CLEAR_GARRISON: i32 = 35;
/// class `a`'s `IsLandBuilding()`
pub const EE_UNITS_UB_IS_LAND_BUILDING: i32 = 36;
/// class `a`'s `mIsWall`
pub const EE_UNITS_UB_IS_WALL: i32 = 37;
/// class `a`'s `mIsTower`
pub const EE_UNITS_UB_IS_TOWER: i32 = 38;
/// complex unit `a`'s `StartAction(c)`
pub const EE_UNITS_UB_START_ACTION: i32 = 39;
/// complex unit `a`'s `GetBuildQueueSize()`
pub const EE_UNITS_UB_BUILD_QUEUE_SIZE: i32 = 40;
/// complex unit `a`'s `mBuildQueue.clear()`
pub const EE_UNITS_UB_BUILD_QUEUE_CLEAR: i32 = 41;
/// class `a`'s `mCategory`
pub const EE_UNITS_UB_CATEGORY: i32 = 42;
/// complex unit `a`'s `SetAnimation(unit b's GetComplexUnit()->mAnimation, -1, 0)`
pub const EE_UNITS_UB_SET_ANIMATION_OF: i32 = 43;
/// complex unit `a`'s `StartAction` of unit b's complex unit's command (0 without one)
pub const EE_UNITS_UB_START_COMMAND_OF: i32 = 44;
/// `gPhysics->QueueUnitSwap(complex a, complex b)`
pub const EE_UNITS_UB_QUEUE_SWAP: i32 = 45;
/// complex unit `a`'s `mPreviousPart`
pub const EE_UNITS_UB_PREVIOUS_PART: i32 = 46;
/// unit `a`'s `GetUnitClass()->GateReplaced(a, complex b)`
pub const EE_UNITS_UB_GATE_REPLACED: i32 = 47;
/// complex unit `a`'s `ClearAllGoalQueues()`
pub const EE_UNITS_UB_CLEAR_ALL_GOAL_QUEUES: i32 = 48;
/// complex unit `a`'s `ClearBuildTargets()`
pub const EE_UNITS_UB_CLEAR_BUILD_TARGETS: i32 = 49;
/// complex unit `a`'s `PlayEffect(c)`
pub const EE_UNITS_UB_PLAY_EFFECT: i32 = 50;
/// complex unit `a`'s `mProductionTime`
pub const EE_UNITS_UB_PRODUCTION_TIME: i32 = 51;
/// complex unit `a`'s `mProductionTime = 0`
pub const EE_UNITS_UB_CLEAR_PRODUCTION_TIME: i32 = 52;
/// class `a`'s `ActionChanged(complex b)`
pub const EE_UNITS_UB_ACTION_CHANGED: i32 = 53;
/// complex unit `a`'s `Kill(c & 1, c & 2, c & 4)`
pub const EE_UNITS_UB_KILL: i32 = 54;
/// player `a`'s `mSavedGroups.Reset(c, 1)`
pub const EE_UNITS_UB_STATS_RESET: i32 = 55;
/// `EEAddDeadUnitStatistic(complex a)`
pub const EE_UNITS_UB_ADD_DEAD_STATISTIC: i32 = 56;
/// unit `a`'s `GetPlayer()->mSavedGroups.Lower(c, 1)`
pub const EE_UNITS_UB_UNIT_STATS_LOWER: i32 = 57;
/// unit `a`'s `GetPlayer()->mSavedGroups.Reset(c, 1)`
pub const EE_UNITS_UB_UNIT_STATS_RESET: i32 = 58;
/// class `a`'s `mDamageEffect != NULL`
pub const EE_UNITS_UB_HAS_DAMAGE_EFFECT: i32 = 59;
/// the bits of class `a`'s `mDamageEffect->mParam2`
pub const EE_UNITS_UB_DAMAGE_PARAM2: i32 = 60;
/// `gWorld.GetEffectManager()->CreateUnitEffect(complex a, its class's mDamageEffect, NULL, 0, 0, 0)`
pub const EE_UNITS_UB_CREATE_DAMAGE_EFFECT: i32 = 61;
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_UB_TILE_X: i32 = 62;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_UB_TILE_Y: i32 = 63;
/// unit `a`'s `IsFoundation()`
pub const EE_UNITS_UB_IS_FOUNDATION: i32 = 64;
/// class `a`'s `CreateUnit(c, d, e, b != 0, NULL)`
pub const EE_UNITS_UB_CREATE_UNIT: i32 = 65;
/// unit `a`'s `CopyFrom(unit b)`
pub const EE_UNITS_UB_COPY_FROM: i32 = 66;
/// unit `a`'s `mTileNode`
pub const EE_UNITS_UB_TILE_NODE: i32 = 67;
/// complex unit `a`'s `SetBuilt(c, d, e)`
pub const EE_UNITS_UB_SET_BUILT: i32 = 68;
/// unit `a`'s `SetHitPoints(c)`
pub const EE_UNITS_UB_SET_HIT_POINTS: i32 = 69;
/// unit `a`'s `mStats->mMaxHitPoints`
pub const EE_UNITS_UB_MAX_HIT_POINTS: i32 = 70;
/// unit `a`'s `GetPlayer()`
pub const EE_UNITS_UB_PLAYER_OF: i32 = 71;
/// player `a`'s `mGroups[c]`
pub const EE_UNITS_UB_GROUP_AT: i32 = 72;
/// group `a`'s `GetNumUnits()`
pub const EE_UNITS_UB_GROUP_NUM_UNITS: i32 = 73;
/// group `a`'s `GetUnit(c)`
pub const EE_UNITS_UB_GROUP_UNIT: i32 = 74;
/// group `a`'s `Add(complex b)`
pub const EE_UNITS_UB_GROUP_ADD: i32 = 75;
/// complex unit `a`'s `DropUnsuitedGoals(0)`
pub const EE_UNITS_UB_DROP_UNSUITED_GOALS: i32 = 76;
/// player `a`'s technology `c` `GetIsAvailable()` (`mTechnologies->Get(c)`)
pub const EE_UNITS_UB_TECH_AVAILABLE: i32 = 77;
/// complex unit `a`'s `mNextPart`
pub const EE_UNITS_UB_NEXT_PART: i32 = 78;
/// complex unit `a`'s `mFireEffect` = a new unit effect of the space wall's (c 0) or gate's (c 1) energy wall
pub const EE_UNITS_UB_ENERGY_WALL: i32 = 79;
/// unit `a`'s `mHitPoints`
pub const EE_UNITS_UB_HIT_POINTS: i32 = 80;
/// unit `a`'s `mTile->GetVisibility(player b's GetPlayerID())`
pub const EE_UNITS_UB_TILE_VISIBILITY: i32 = 81;
/// `gDoppelgangerManager.AddForUnseenUnit(complex a)`
pub const EE_UNITS_UB_ADD_FOR_UNSEEN: i32 = 82;
/// player `a`'s `Pay(its GetTechTree()->Get(class b's GetTechnology()), 0x20, 1.0f)`
pub const EE_UNITS_UB_PAY_CLASS: i32 = 83;
/// complex unit `a`'s `mIsPaidFor`
pub const EE_UNITS_UB_IS_PAID_FOR: i32 = 84;
/// complex unit `a`'s `mIsPaidFor = true`
pub const EE_UNITS_UB_SET_PAID_FOR: i32 = 85;
/// player `a`'s `AddPendingUnits(false, true)`
pub const EE_UNITS_UB_ADD_PENDING_UNITS: i32 = 86;
/// `gWorld.FlushQueuedPlacements()`
pub const EE_UNITS_UB_FLUSH_PLACEMENTS: i32 = 87;
/// the replant-farms message of unit `a` to the local input
pub const EE_UNITS_UB_REPLANT_FARMS: i32 = 88;
/// player `a`'s `GetPlayerID()`
pub const EE_UNITS_UB_PLAYER_ID: i32 = 89;
/// class `a`'s `mTechnology`
pub const EE_UNITS_UB_CLASS_TECHNOLOGY: i32 = 90;
/// player `a`'s `mTechnologies->Get(c)`
pub const EE_UNITS_UB_TECHNOLOGY: i32 = 91;
/// technology `a`'s `GetNumUnitClasses()`
pub const EE_UNITS_UB_TECH_NUM_CLASSES: i32 = 92;
/// technology `a`'s `GetUnitClass(c)`
pub const EE_UNITS_UB_TECH_CLASS: i32 = 93;
/// the unit list = player `a`'s living units of type `c` (`ee_units_uu_player_units_of_type`); its size
pub const EE_UNITS_UB_UNITS_OF_TYPE: i32 = 94;
/// the unit list[c]
pub const EE_UNITS_UB_LIST_UNIT: i32 = 95;
/// complex unit `a`'s `ReplaceResearch(c, d)`
pub const EE_UNITS_UB_REPLACE_RESEARCH: i32 = 96;
/// complex unit `a`'s `SetGoal(EEUGRust::NewGather(complex b, NULL, false), 0, 0)`
pub const EE_UNITS_UB_SET_GATHER_GOAL: i32 = 97;
/// complex unit `a`'s `SetGoal(EEUGRust::NewBuildRepair(complex b's GetPlayer(), b, false), 0, 0)`
pub const EE_UNITS_UB_SET_REPAIR_GOAL: i32 = 98;
/// goal `a`'s attack target's `mComplexUnit`
pub const EE_UNITS_UB_ATTACK_TARGET: i32 = 99;
/// goal `a`'s `ReplaceUnit(complex b, complex c, NULL)`
pub const EE_UNITS_UB_GOAL_REPLACE_UNIT: i32 = 100;
/// complex unit `a`'s `mCollisionResolutionCount = 0.0f`
pub const EE_UNITS_UB_CLEAR_COLLISION_COUNT: i32 = 101;
/// complex unit `a`'s `SetBlockingUnit(NULL)`
pub const EE_UNITS_UB_CLEAR_BLOCKING: i32 = 102;
/// complex unit `a`'s `mCollisionState`
pub const EE_UNITS_UB_COLLISION_STATE: i32 = 103;
/// complex unit `a`'s `mCollisionState = 0`
pub const EE_UNITS_UB_CLEAR_COLLISION_STATE: i32 = 104;
/// complex unit `a`'s `DropPath()`
pub const EE_UNITS_UB_DROP_PATH: i32 = 105;
/// complex unit `a`'s `IgnoreTarget(complex b)`
pub const EE_UNITS_UB_IGNORE_TARGET: i32 = 106;
/// `EEFindNewTarget(the near-old screen of complex a (25), unit b's GetLoadGoal(), b, (c, d), e, true, 0)`
pub const EE_UNITS_UB_FIND_NEW_TARGET: i32 = 107;
/// the "could not find a target" trace of complex unit `a`'s class
pub const EE_UNITS_UB_NO_TARGET_TRACE: i32 = 108;
/// complex unit `a`'s `GetOutermostCarrier()`
pub const EE_UNITS_UB_OUTERMOST_CARRIER: i32 = 109;
/// class `a`'s `IsPartLeftOut(complex b)`
pub const EE_UNITS_UB_PART_LEFT_OUT: i32 = 110;
/// complex unit `a`'s `IsTargetIgnored(complex b, c != 0)`
pub const EE_UNITS_UB_TARGET_IGNORED: i32 = 111;
/// `EEAttackUnitCast(complex a's GetLoadGoal())`
pub const EE_UNITS_UB_LOAD_ATTACK_GOAL: i32 = 112;
/// the gate class `a`'s `GetLastLinkedCarrier(complex b)`
pub const EE_UNITS_UB_LAST_LINKED_CARRIER: i32 = 113;
/// the gate class `a`'s `GetLastLinked(complex b)`
pub const EE_UNITS_UB_LAST_LINKED: i32 = 114;
/// the bits of complex unit `a`'s `DistanceSquaredTo(complex b)`
pub const EE_UNITS_UB_DISTANCE_SQUARED_TO: i32 = 115;
/// `EECastCalamityGoalCast(complex a's GetGoal())`
pub const EE_UNITS_UB_CALAMITY_GOAL: i32 = 116;
/// goal `a`'s `GetGoalLocation(f)` (three floats)
pub const EE_UNITS_UB_GOAL_LOCATION: i32 = 117;
/// goal `a`'s `GetCalamityType()`
pub const EE_UNITS_UB_CALAMITY_TYPE: i32 = 118;
/// unit `a`'s player's `mTechnologies->Get(c)`
pub const EE_UNITS_UB_UNIT_TECHNOLOGY: i32 = 119;
/// the calamity class of technology `a`'s `mData->mCalamity`'s `mRange`
pub const EE_UNITS_UB_CALAMITY_RANGE: i32 = 120;
/// complex unit `a`'s `GetPosition(f)` (three floats)
pub const EE_UNITS_UB_POSITION: i32 = 121;
/// the direction vector set to `f` (three floats)
pub const EE_UNITS_UB_DIRECTION_SET: i32 = 122;
/// the direction vector's `Normalize()`
pub const EE_UNITS_UB_DIRECTION_NORMALIZE: i32 = 123;
/// the direction vector's `Scale(f[0], f)` (three floats out)
pub const EE_UNITS_UB_DIRECTION_SCALE: i32 = 124;
/// the point at `a` (a `GE3DPoint*`) set to `f`
pub const EE_UNITS_UB_SET_POINT: i32 = 125;
/// the point at `a` into `f`
pub const EE_UNITS_UB_POINT: i32 = 126;
/// transformation `a`'s position into `f`
pub const EE_UNITS_UB_TRANSFORMATION_POSITION: i32 = 127;
/// transformation `a`'s matrix into the matrix at `c`
pub const EE_UNITS_UB_TRANSFORMATION_MATRIX: i32 = 128;
/// complex unit `a`'s location's matrix into the matrix at `c`
pub const EE_UNITS_UB_LOCATION_MATRIX: i32 = 129;
/// `*(long*)a = c`
pub const EE_UNITS_UB_SET_LONG: i32 = 130;
/// `gWorld.mMap->GetTile(c, d) != NULL`
pub const EE_UNITS_UB_TILE_EXISTS: i32 = 131;
/// complex unit `a`'s `SetTransformation(transformation b, true)`
pub const EE_UNITS_UB_SET_TRANSFORMATION: i32 = 132;
/// `gWorld.mMap->GetClosestTile(the point at f, the longs at c, d)` (f a `GE3DPoint`)
pub const EE_UNITS_UB_CLOSEST_TILE: i32 = 133;
/// complex unit `a`'s `mPhysicalModel.SetLocation(transformation b)`
pub const EE_UNITS_UB_SET_LOCATION: i32 = 134;
/// complex unit `a`'s `MoveTo(c, d, true)`
pub const EE_UNITS_UB_MOVE_TO: i32 = 135;
/// complex unit `a`'s `mWaypoints.size()`
pub const EE_UNITS_UB_WAYPOINT_COUNT: i32 = 136;
/// class `a`'s `mDomains`
pub const EE_UNITS_UB_DOMAINS: i32 = 137;
/// complex unit `a`'s `GetNextWaypointPoint(the scratch point)`
pub const EE_UNITS_UB_NEXT_WAYPOINT_POINT: i32 = 138;
/// the bits of complex unit `a`'s class's `GetHeightAt(a, the scratch point)`
pub const EE_UNITS_UB_HEIGHT_AT: i32 = 139;
/// the bits of complex unit `a`'s `mHeight`
pub const EE_UNITS_UB_HEIGHT: i32 = 140;
/// complex unit `a`'s `mHeight = the float of the bits c`
pub const EE_UNITS_UB_SET_HEIGHT: i32 = 141;
/// `UShutdownInProgress()`
pub const EE_UNITS_UB_SHUTDOWN: i32 = 142;
/// `ee_units_uu_unit_tiles(1, unit a, the tile list)`
pub const EE_UNITS_UB_UNIT_TILES: i32 = 143;
/// `ee_units_uu_units_on(0, the tile list, the unit list)`
pub const EE_UNITS_UB_UNITS_ON_TILES: i32 = 144;
/// the bits of `gPhysics->mRandom.Generate()`
pub const EE_UNITS_UB_RANDOM_GENERATE: i32 = 145;
/// `PermuteValues(the unit list, c)`
pub const EE_UNITS_UB_PERMUTE_UNITS: i32 = 146;
/// the unit list's size
pub const EE_UNITS_UB_LIST_SIZE: i32 = 147;
/// complex unit `a`'s command's `mType` (0 without one)
pub const EE_UNITS_UB_COMMAND: i32 = 148;
/// complex unit `a`'s `mGoalState`
pub const EE_UNITS_UB_GOAL_STATE: i32 = 149;
/// goal `a`'s `mKind`
pub const EE_UNITS_UB_GOAL_KIND: i32 = 150;
/// `EEGatherGoalCast(goal a)`
pub const EE_UNITS_UB_GATHER_GOAL: i32 = 151;
/// gather goal `a`'s `GetGatherResource()`
pub const EE_UNITS_UB_GATHER_RESOURCE: i32 = 152;
/// upgrade `a`'s `mUnitClass->mHitPoints`
pub const EE_UNITS_UB_UPGRADE_BASE_HIT_POINTS: i32 = 153;
/// upgrade `a`'s `GetMaxHitPoints()`
pub const EE_UNITS_UB_UPGRADE_MAX_HIT_POINTS: i32 = 154;
/// the hit point functor = `EEHitPointUpgrader(c, the float of the bits d)`; its address
pub const EE_UNITS_UB_MAKE_UPGRADER: i32 = 155;
/// upgrade `a`'s `mMaxHitPoints = c`
pub const EE_UNITS_UB_SET_UPGRADE_MAX_HIT_POINTS: i32 = 156;
/// `gLoadingSavedGame`
pub const EE_UNITS_UB_LOADING_SAVED_GAME: i32 = 157;
/// player `a`'s `mFirstUnit`
pub const EE_UNITS_UB_FIRST_UNIT: i32 = 158;
/// unit `a`'s `mNext`
pub const EE_UNITS_UB_NEXT_UNIT: i32 = 159;
/// functor `a`'s `ApplyToUnit(unit b)`
pub const EE_UNITS_UB_APPLY_TO_UNIT: i32 = 160;
/// functor `a`'s `Apply(complex b)`
pub const EE_UNITS_UB_APPLY: i32 = 161;
/// player `a`'s `GetNumPendingUnits()`
pub const EE_UNITS_UB_NUM_PENDING: i32 = 162;
/// player `a`'s `GetPendingUnit(c)`
pub const EE_UNITS_UB_PENDING_UNIT: i32 = 163;
/// `gWorld.mNumPlayers`
pub const EE_UNITS_UB_NUM_PLAYERS: i32 = 164;
/// functor `a`'s (an `EEHitPointUpgrader`) `mUnitType`
pub const EE_UNITS_UB_UPGRADER_TYPE: i32 = 165;
/// the bits of functor `a`'s (an `EEHitPointUpgrader`) `mRatio`
pub const EE_UNITS_UB_UPGRADER_RATIO: i32 = 166;
/// complex unit `a`'s `GetNumCargoUnits()`
pub const EE_UNITS_UB_NUM_CARGO: i32 = 167;
/// complex unit `a`'s `UnloadCargoAt(c, the off-map transformation)`
pub const EE_UNITS_UB_UNLOAD_CARGO_AT: i32 = 168;
/// complex unit `a`'s `LoadCargo(complex b, false, c != 0)`
pub const EE_UNITS_UB_LOAD_CARGO: i32 = 169;
/// complex unit `a`'s `AddGarrison(complex b's mGarrisonCount - a's class's mInitialGarrisonCount, b's mGarrisonPopulation)`
pub const EE_UNITS_UB_TAKE_GARRISON: i32 = 170;
/// class `a`'s `mTileSize`
pub const EE_UNITS_UB_TILE_SIZE: i32 = 171;
/// class `gUnitClassTable[c]`'s `mModel`
pub const EE_UNITS_UB_TABLE_MODEL: i32 = 172;
/// player `a`'s `mUnitClassUpgrades[c]->mModel`
pub const EE_UNITS_UB_UPGRADE_MODEL: i32 = 173;
/// the bits of unit `a`'s `mStats->GetRange()`
pub const EE_UNITS_UB_RANGE: i32 = 174;
/// the bits of complex unit `a`'s `GetLineOfSight()`
pub const EE_UNITS_UB_LINE_OF_SIGHT: i32 = 175;
/// the bits of unit `a`'s `mStats->GetAttack()`
pub const EE_UNITS_UB_ATTACK: i32 = 176;
/// class `a`'s `mAmmoType`
pub const EE_UNITS_UB_AMMO_TYPE: i32 = 177;
/// `gWorld.mInfluenceTable->SumOthersValueOnTiles(c, the search area)`
pub const EE_UNITS_UB_SUM_OTHERS_INFLUENCE: i32 = 178;
/// `EESearchTarget(complex a, &the search area, c, d, the float of the bits e)`
pub const EE_UNITS_UB_SEARCH_TARGET: i32 = 179;
/// `EESearchTargetAlongList(complex a, c, d, the float of the bits e)`
pub const EE_UNITS_UB_SEARCH_ALONG_LIST: i32 = 180;
/// `EEApproachAttachPoint(complex a, the point at b)`
pub const EE_UNITS_UB_APPROACH_ATTACH_POINT: i32 = 181;
/// complex unit `a`'s `IsOnOtherContinentThan(complex b)`
pub const EE_UNITS_UB_OTHER_CONTINENT: i32 = 182;
/// `EECombatFindDropLocation(complex a, unit b, the point at c)`
pub const EE_UNITS_UB_FIND_DROP_LOCATION: i32 = 183;
/// `EEGetBallisticPitch(complex a, its GetStats(), the point at b, the point at c, the float at d)`
pub const EE_UNITS_UB_BALLISTIC_PITCH: i32 = 184;
/// class `a`'s `mShootsHigh`
pub const EE_UNITS_UB_SHOOTS_HIGH: i32 = 185;
/// complex unit `a`'s `GetStats()->mCanFireOverWalls`
pub const EE_UNITS_UB_FIRES_OVER_WALLS: i32 = 186;
/// `EEIsLineToUnitBlocked(complex a, unit b, the unit at c, the longs at d, e)`
pub const EE_UNITS_UB_LINE_TO_UNIT_BLOCKED: i32 = 187;
/// class `a`'s `IsPriest()`
pub const EE_UNITS_UB_IS_PRIEST: i32 = 188;
/// `EECanConvert(complex a, complex b, the bool at c)`
pub const EE_UNITS_UB_CAN_CONVERT: i32 = 189;
/// `ee_units_uu_drop_tiles(c, the tiles at a)`
pub const EE_UNITS_UB_DROP_TILES: i32 = 190;
/// `EEPickWallPieces(the tiles at a, the corner list, gWorld.mMap)`
pub const EE_UNITS_UB_PICK_WALL_PIECES: i32 = 191;
/// `EEBuildWallGhosts(the tiles at a, the corner list, c, player b, 0.0f)`
pub const EE_UNITS_UB_BUILD_WALL_GHOSTS: i32 = 192;
/// the corner list emptied
pub const EE_UNITS_UB_CORNERS_CLEAR: i32 = 193;
/// civilization `a`'s purchased upgrade count
pub const EE_UNITS_UB_PURCHASED_COUNT: i32 = 194;
/// civilization `a`'s purchased upgrade `c`
pub const EE_UNITS_UB_PURCHASED_AT: i32 = 195;
/// `gCivilizationManager.SplitAttribute(c, &the category, &the index)`
pub const EE_UNITS_UB_SPLIT_ATTRIBUTE: i32 = 196;
/// `gCivilizationManager.LookupLevel(c, &f[0], &f[1])`
pub const EE_UNITS_UB_LOOKUP_LEVEL: i32 = 197;
/// `gCivilizationManager.GetCategoryNameID(the category, &the string id)`
pub const EE_UNITS_UB_CATEGORY_NAME_ID: i32 = 198;
/// `gResource->GetString(the string id, the category string)`
pub const EE_UNITS_UB_CATEGORY_STRING: i32 = 199;
/// the string id = `EECivilizationManager::LevelNameID(the index)`
pub const EE_UNITS_UB_LEVEL_NAME_ID: i32 = 200;
/// `gResource->GetString(the string id, the attribute string)`
pub const EE_UNITS_UB_ATTRIBUTE_STRING: i32 = 201;
/// `EECivilizationManager::IsPowerLevel(the index)`
pub const EE_UNITS_UB_IS_POWER_LEVEL: i32 = 202;
/// `sprintf(text a, "%s: %s %ld%%", the category string, the attribute string, c)`
pub const EE_UNITS_UB_LEVEL_TEXT: i32 = 203;
/// `sprintf(text a, "%s", the attribute string)`
pub const EE_UNITS_UB_POWER_TEXT: i32 = 204;
/// the adaptation's name string's address
pub const EE_UNITS_UB_NAME_TEXT: i32 = 205;
/// the search area's `mTop = c`
pub const EE_UNITS_UB_AREA_TOP: i32 = 206;
/// the search area's `mBottom = c`
pub const EE_UNITS_UB_AREA_BOTTOM: i32 = 207;
/// `*(long*)a`
pub const EE_UNITS_UB_READ_LONG: i32 = 208;
/// the unit list and the tile list emptied (the original's were new locals: the searches append to them)
pub const EE_UNITS_UB_LISTS_CLEAR: i32 = 211;
/// the search area's `mLeft = c`
pub const EE_UNITS_UB_AREA_LEFT: i32 = 209;
/// the search area's `mRight = c`
pub const EE_UNITS_UB_AREA_RIGHT: i32 = 210;

// ---- the bodies (`ee_units_ub_body`'s `what`)
pub const EE_UNITS_UB_DO_CONVERT_UNIT: i32 = 1;
pub const EE_UNITS_UB_DO_UPGRADE_TEXT: i32 = 2;
pub const EE_UNITS_UB_DO_MAKE_UNIT_COPY: i32 = 3;
pub const EE_UNITS_UB_DO_REPLACE_UNIT: i32 = 4;
pub const EE_UNITS_UB_DO_MAKE_BUILDING: i32 = 5;
pub const EE_UNITS_UB_DO_REPLACE_QUEUED_TECHNOLOGY: i32 = 6;
pub const EE_UNITS_UB_DO_WORK_FARM: i32 = 7;
pub const EE_UNITS_UB_DO_RETARGET_ATTACK: i32 = 8;
pub const EE_UNITS_UB_DO_FIND_NEW_TARGET_NEAR_OLD: i32 = 9;
pub const EE_UNITS_UB_DO_HELICOPTER_PRIEST_DESTINATION: i32 = 10;
pub const EE_UNITS_UB_DO_UPDATE_UNIT_TILE: i32 = 11;
pub const EE_UNITS_UB_DO_WAITING_GATHERER: i32 = 12;
pub const EE_UNITS_UB_DO_UPGRADE_HIT_POINTS: i32 = 13;
pub const EE_UNITS_UB_DO_HIT_POINT_APPLY: i32 = 14;
pub const EE_UNITS_UB_DO_FOR_EACH_UNIT_OF: i32 = 15;
pub const EE_UNITS_UB_DO_FOR_EACH_PLAYERS_UNIT: i32 = 16;
pub const EE_UNITS_UB_DO_MOVE_PASSENGERS: i32 = 17;
pub const EE_UNITS_UB_DO_TAKE_OVER_LOAD: i32 = 18;
pub const EE_UNITS_UB_DO_PLAYER_CLASS_GRAPHIC: i32 = 19;
pub const EE_UNITS_UB_DO_FIND_TARGET_NEAR: i32 = 20;
pub const EE_UNITS_UB_DO_FIND_NEARBY_TARGET: i32 = 21;
pub const EE_UNITS_UB_DO_CAN_BALLISTIC_HIT: i32 = 22;
pub const EE_UNITS_UB_DO_CAN_PRIEST_CONVERT: i32 = 23;
pub const EE_UNITS_UB_DO_PREVIEW_WALL: i32 = 24;
pub const EE_UNITS_UB_DO_SPACE_ENERGY_WALL: i32 = 25;
pub const EE_UNITS_UB_DO_HAS_CIVILIZATION_UPGRADE: i32 = 26;

// ---- the original's constants
const UNIT_TYPE_SETTLEMENT: i64 = 0x2b;
const UNIT_TYPE_TOWN_CENTER: i64 = 0x23;
const UNIT_TYPE_ARCH03_SIMPLE_BOWMAN_FLAMING: i64 = 0x2d6;
const UNIT_TYPE_ARCH05_COMPOSITE_BOW_FLAMING: i64 = 0x2d9;
const UNIT_TYPE_ARCH06_LONG_BOW_FLAMING: i64 = 0x2dd;
const UNIT_TYPE_ARCH03_SIMPLE_BOWMAN: i64 = 0xa;
const UNIT_TYPE_ARCH05_COMPOSITE_BOW: i64 = 0xf;
const UNIT_TYPE_ARCH06_LONG_BOW: i64 = 0x11;
const UNIT_TYPE_INF04_SHORT_SWORD_CRUSADER: i64 = 0x2e6;
const UNIT_TYPE_INF06_LONG_SWORD_CRUSADER: i64 = 0x2e7;
const UNIT_TYPE_CAV04_BRONZE_SPEAR_CAVALRY_CRUSADER: i64 = 0x2e8;
const UNIT_TYPE_CAV06_KNIGHT_CRUSADER: i64 = 0x2e9;
const UNIT_TYPE_INF04_SHORT_SWORD: i64 = 0x16;
const UNIT_TYPE_INF06_LONG_SWORD: i64 = 0x17;
const UNIT_TYPE_CAV04_BRONZE_SPEAR_CAVALRY: i64 = 0x89;
const UNIT_TYPE_CAV06_KNIGHT: i64 = 0x8b;
const UNIT_TYPE_EMISSARY: i64 = 0x2d4;
const UNIT_TYPE_PRIEST: i64 = 1;
const UNIT_TYPE_PATH_POINT: i64 = 0xdf;
const UNIT_TYPE_WALL_SPACE: i64 = 0x2fe;
const UNIT_TYPE_GATE_PALISADES: i64 = 0x311;
const UNIT_TYPE_ROBOTIC_FARM: i64 = 0x33a;
const UNIT_TYPE_SPACE_FALLOW_FARM: i64 = 0x33b;
const UNIT_TYPE_AIRPORT: i64 = 0x42;
const UNIT_TYPE_FOUNDATION6X6: i64 = 0x168;
const UNIT_TYPE_FOUNDATION_AIRPORT: i64 = 0x2cd;
const UNIT_TYPE_FARM: i64 = 0x2a;
const UNIT_TYPE_GRANARY: i64 = 0x34;
const UNIT_TYPE_GRANARY_IMPROVED: i64 = 0x2ba;
const CIV_POWER_FLAMING_ARROWS: i64 = 0x26;
const CIV_POWER_CRUSADERS: i64 = 0x29;
const CIV_POWER_EMISSARIES: i64 = 0x2b;
const CIV_POWER_ADAPTATION: i64 = 0x39;
const CATEGORY_GATE: i64 = 0x19;
const CATEGORY_AIRPLANE: i64 = 3;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
const CATEGORY_BUILDING: i64 = 2;
const CATEGORY_BUILDING19: i64 = 0x13;
const CATEGORY_MINING_BUILDING: i64 = 0x18;
const CATEGORY_FORTRESS: i64 = 0x1d;
const CATEGORY_BUILDING_PRIEST: i64 = 0x1e;
const ACTION_IDLE: i64 = 5000;
const ACTION_CIRCLE: i64 = 1008;
const ACTION_CHECK_RANGE: i64 = 2013;
const ACTION_MOVE_TO_RESOURCE: i64 = 8008;
const ACTION_MOVE_TO_REPAIR_SITE: i64 = 8017;
const ACTION_WAIT_FOR_OPEN_POSITION: i64 = 4026;
const GOAL_GATHER: i64 = 6;
const MOVEMENT_AIR: i64 = 1;
const TECH_WALL_SPACE: i64 = 15013;
const TECH_GATE_SPACE: i64 = 15014;
/// `kTechNone` (PlayEffect's none)
const TECH_NONE: i64 = 100000;
/// the anti-air ammunition (`mAmmoType` 0x3c): its searches walk the unit list
const AMMO_ANTI_AIR: i64 = 0x3c;
/// the foundation upgrades of the sizes 1, 2 and 3-4 (`mUnitClassUpgrades[0x136 / 0x132 / 0x133]`)
const FOUNDATION_1: i64 = 0x136;
const FOUNDATION_2: i64 = 0x132;
const FOUNDATION_3: i64 = 0x133;

/// The C++ statements of the bodies.
pub trait UbHost {
    /// one statement (`EE_UNITS_UB_*`)
    fn ub(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, e: i64, f: &mut [f32; 4]) -> i64;
}

fn s<H: UbHost + ?Sized>(h: &mut H, op: i32) -> i64 {
    h.ub(op, 0, 0, 0, 0, 0, &mut [0.0; 4])
}
fn sa<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64) -> i64 {
    h.ub(op, a, 0, 0, 0, 0, &mut [0.0; 4])
}
fn sab<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64) -> i64 {
    h.ub(op, a, b, 0, 0, 0, &mut [0.0; 4])
}
fn sac<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64, c: i64) -> i64 {
    h.ub(op, a, 0, c, 0, 0, &mut [0.0; 4])
}
fn sabc<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, c: i64) -> i64 {
    h.ub(op, a, b, c, 0, 0, &mut [0.0; 4])
}
fn sc<H: UbHost + ?Sized>(h: &mut H, op: i32, c: i64) -> i64 {
    h.ub(op, 0, 0, c, 0, 0, &mut [0.0; 4])
}
/// an object answer
fn obj<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64) -> u64 {
    sa(h, op, a) as u64
}
fn flt(bits: i64) -> f32 {
    f32::from_bits(bits as u32)
}
fn bits(value: f32) -> i64 {
    i64::from(value.to_bits())
}
fn yes(v: i64) -> bool {
    v != 0
}

/// `UMCeil`: the nearest integer, one higher when that is below `value`
fn ceil(value: f32) -> i32 {
    let r = ee_base::math::um::lrintf(value);
    if (r as f32) < value {
        r.wrapping_add(1)
    } else {
        r
    }
}

/// `EEFn24E68F(upgrade, text)`: the upgrade's text into the `UWideString` at `text` ("category: attribute percent%",
/// or the attribute alone for a power level); false when a string is missing.
pub fn upgrade_text<H: UbHost + ?Sized>(h: &mut H, upgrade: i64, text: u64) -> bool {
    sc(h, EE_UNITS_UB_SPLIT_ATTRIBUTE, upgrade);
    let mut f = [0.0f32; 4];
    h.ub(EE_UNITS_UB_LOOKUP_LEVEL, 0, 0, upgrade, 0, 0, &mut f);
    let value = f[0];
    if !yes(s(h, EE_UNITS_UB_CATEGORY_NAME_ID)) {
        return false;
    }
    if s(h, EE_UNITS_UB_CATEGORY_STRING) != 0 {
        return false;
    }
    s(h, EE_UNITS_UB_LEVEL_NAME_ID);
    if s(h, EE_UNITS_UB_ATTRIBUTE_STRING) != 0 {
        return false;
    }
    if !yes(s(h, EE_UNITS_UB_IS_POWER_LEVEL)) {
        // `(long)(theValue * 100.0f + 0.5f)`: a truncation
        let percent = (value * 100.0f32 + 0.5f32) as i32;
        sac(h, EE_UNITS_UB_LEVEL_TEXT, text, i64::from(percent));
    } else {
        sa(h, EE_UNITS_UB_POWER_TEXT, text);
    }
    true
}

/// `EEHasCivilizationUpgrade(civilization, value)`: whether the civilization bought the upgrade.
pub fn has_civilization_upgrade<H: UbHost + ?Sized>(h: &mut H, civilization: u64, value: i64) -> bool {
    let n = sa(h, EE_UNITS_UB_PURCHASED_COUNT, civilization);
    (0..n).any(|i| sac(h, EE_UNITS_UB_PURCHASED_AT, civilization, i) == value)
}

/// `EEConvertUnit(unit, player, conversion)`: the unit converted to the player (the adaptation's grant, the class
/// the new owner may have, the new unit's state and the statistics); the new complex unit.
pub fn convert_unit<H: UbHost + ?Sized>(h: &mut H, unit: u64, player_index: i64, conversion: bool) -> u64 {
    let note = Note::begin(86);
    note.u(unit).i(player_index).b(conversion);
    let player = obj_c(h, EE_UNITS_UB_PLAYER_AT, player_index);
    let mut old = 0u64;
    let mut class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    let class_type = sa(h, EE_UNITS_UB_CLASS_TYPE, class);
    if (class_type == UNIT_TYPE_SETTLEMENT || class_type == UNIT_TYPE_TOWN_CENTER)
        && yes(sac(h, EE_UNITS_UB_HAS_POWER, player, CIV_POWER_ADAPTATION))
        && yes(sa(h, EE_UNITS_UB_IS_BUILT, unit))
    {
        adaptation(h, unit, player);
        if class_type == UNIT_TYPE_TOWN_CENTER {
            class = obj_c(h, EE_UNITS_UB_CLASS_OF_TYPE, UNIT_TYPE_SETTLEMENT);
        }
    }
    if !yes(sac(h, EE_UNITS_UB_HAS_POWER, player, CIV_POWER_FLAMING_ARROWS)) {
        let plain = match sa(h, EE_UNITS_UB_CLASS_TYPE, class) {
            UNIT_TYPE_ARCH03_SIMPLE_BOWMAN_FLAMING => Some(UNIT_TYPE_ARCH03_SIMPLE_BOWMAN),
            UNIT_TYPE_ARCH05_COMPOSITE_BOW_FLAMING => Some(UNIT_TYPE_ARCH05_COMPOSITE_BOW),
            UNIT_TYPE_ARCH06_LONG_BOW_FLAMING => Some(UNIT_TYPE_ARCH06_LONG_BOW),
            _ => None,
        };
        if let Some(t) = plain {
            class = obj_c(h, EE_UNITS_UB_CLASS_OF_TYPE, t);
        }
    }
    if !yes(sac(h, EE_UNITS_UB_HAS_POWER, player, CIV_POWER_CRUSADERS)) && yes(sa(h, EE_UNITS_UB_IS_CRUSADER, class)) {
        let plain = match sa(h, EE_UNITS_UB_CLASS_TYPE, class) {
            UNIT_TYPE_INF04_SHORT_SWORD_CRUSADER => Some(UNIT_TYPE_INF04_SHORT_SWORD),
            UNIT_TYPE_INF06_LONG_SWORD_CRUSADER => Some(UNIT_TYPE_INF06_LONG_SWORD),
            UNIT_TYPE_CAV04_BRONZE_SPEAR_CAVALRY_CRUSADER => Some(UNIT_TYPE_CAV04_BRONZE_SPEAR_CAVALRY),
            UNIT_TYPE_CAV06_KNIGHT_CRUSADER => Some(UNIT_TYPE_CAV06_KNIGHT),
            _ => None,
        };
        if let Some(t) = plain {
            class = obj_c(h, EE_UNITS_UB_CLASS_OF_TYPE, t);
        }
    }
    if !yes(sac(h, EE_UNITS_UB_HAS_POWER, player, CIV_POWER_EMISSARIES))
        && sa(h, EE_UNITS_UB_CLASS_TYPE, class) == UNIT_TYPE_EMISSARY
    {
        class = obj_c(h, EE_UNITS_UB_CLASS_OF_TYPE, UNIT_TYPE_PRIEST);
    }
    let fraction = flt(sa(h, EE_UNITS_UB_HIT_FRACTION, unit));
    sa(h, EE_UNITS_UB_DESELECT, unit);
    for i in 0..20 {
        if yes(sac(h, EE_UNITS_UB_GROUP_CONTAINS, unit, i)) {
            sac(h, EE_UNITS_UB_GROUP_REMOVE, unit, i);
        }
    }
    sab(h, EE_UNITS_UB_SET_CLASS_MEDIA, player, unit);
    let new_unit = make_unit_copy(h, unit, player_index, class);
    sab(h, EE_UNITS_UB_UNLOAD_CLASS_MEDIA, player, unit);
    if obj(h, EE_UNITS_UB_UNIT_CLASS, unit) != class && sa(h, EE_UNITS_UB_CLASS_TYPE, class) == UNIT_TYPE_SETTLEMENT {
        let c = obj(h, EE_UNITS_UB_COMPLEX_OF, new_unit);
        sa(h, EE_UNITS_UB_CLEAR_GARRISON, c);
    }
    let new = obj(h, EE_UNITS_UB_COMPLEX_OF, new_unit);
    if new != 0 {
        let new_class = obj(h, EE_UNITS_UB_UNIT_CLASS, new);
        if yes(sa(h, EE_UNITS_UB_IS_LAND_BUILDING, new_class)) && yes(sa(h, EE_UNITS_UB_IS_BUILT, new)) {
            if !yes(sa(h, EE_UNITS_UB_IS_WALL, new_class)) || yes(sa(h, EE_UNITS_UB_IS_TOWER, new_class)) {
                sac(h, EE_UNITS_UB_START_ACTION, new, ACTION_IDLE);
            }
            if sa(h, EE_UNITS_UB_BUILD_QUEUE_SIZE, new) as u32 > 0 {
                sa(h, EE_UNITS_UB_BUILD_QUEUE_CLEAR, new);
            }
            if yes(sa(h, EE_UNITS_UB_IS_WALL, class)) && sa(h, EE_UNITS_UB_CATEGORY, class) != CATEGORY_GATE {
                sab(h, EE_UNITS_UB_SET_ANIMATION_OF, new, unit);
            } else if sa(h, EE_UNITS_UB_CATEGORY, class) == CATEGORY_GATE {
                sab(h, EE_UNITS_UB_START_COMMAND_OF, new, unit);
            }
        } else {
            let category = sa(h, EE_UNITS_UB_CATEGORY, class);
            let action = if category == CATEGORY_AIRPLANE || category == CATEGORY_PLANET_SPACESHIP {
                ACTION_CIRCLE
            } else {
                ACTION_IDLE
            };
            sac(h, EE_UNITS_UB_START_ACTION, new, action);
        }
        old = obj(h, EE_UNITS_UB_COMPLEX_OF, unit);
        sab(h, EE_UNITS_UB_QUEUE_SWAP, old, new);
        if obj(h, EE_UNITS_UB_PREVIOUS_PART, old) == 0 {
            sab(h, EE_UNITS_UB_GATE_REPLACED, unit, new);
        }
        sa(h, EE_UNITS_UB_CLEAR_ALL_GOAL_QUEUES, new);
        sa(h, EE_UNITS_UB_BUILD_QUEUE_CLEAR, new);
        sa(h, EE_UNITS_UB_CLEAR_BUILD_TARGETS, new);
        sac(h, EE_UNITS_UB_PLAY_EFFECT, new, TECH_NONE);
        if sa(h, EE_UNITS_UB_PRODUCTION_TIME, new) != 0 {
            sa(h, EE_UNITS_UB_CLEAR_PRODUCTION_TIME, new);
        }
        let new_class = obj(h, EE_UNITS_UB_UNIT_CLASS, new);
        if !yes(sa(h, EE_UNITS_UB_IS_WALL, new_class)) {
            sab(h, EE_UNITS_UB_ACTION_CHANGED, class, new);
        }
    }
    sac(h, EE_UNITS_UB_KILL, unit, 4);
    if conversion {
        sac(h, EE_UNITS_UB_STATS_RESET, player, 5);
        let series = sa(h, EE_UNITS_UB_ADD_DEAD_STATISTIC, unit);
        if series != 0 {
            sac(h, EE_UNITS_UB_UNIT_STATS_LOWER, unit, series);
            sac(h, EE_UNITS_UB_STATS_RESET, player, series);
        }
        let unit_class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
        if sa(h, EE_UNITS_UB_CLASS_TYPE, unit_class) != UNIT_TYPE_PATH_POINT {
            let category = sa(h, EE_UNITS_UB_CATEGORY, unit_class);
            let series =
                if [CATEGORY_BUILDING, CATEGORY_BUILDING19, CATEGORY_GATE, CATEGORY_MINING_BUILDING, CATEGORY_FORTRESS]
                    .contains(&category)
                {
                    1
                } else {
                    4
                };
            sac(h, EE_UNITS_UB_UNIT_STATS_RESET, unit, series);
        }
    }
    damage_effect(h, unit, new, old, fraction, true);
    note.u(new).c(if new != 0 { obj(h, EE_UNITS_UB_UNIT_CLASS, new) } else { 0 });
    new
}

fn obj_c<H: UbHost + ?Sized>(h: &mut H, op: i32, c: i64) -> u64 {
    sc(h, op, c) as u64
}

/// The replaced unit's damage effect on the new one (EEConvertUnit's `theFraction < mParam2`; EEReplaceUnit's
/// `mParam2 > theFraction`: the same test).
fn damage_effect<H: UbHost + ?Sized>(h: &mut H, unit: u64, new: u64, old: u64, fraction: f32, _convert: bool) {
    let unit_class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    if !yes(sa(h, EE_UNITS_UB_HAS_DAMAGE_EFFECT, unit_class)) {
        return;
    }
    let new_class = obj(h, EE_UNITS_UB_UNIT_CLASS, new);
    if !yes(sa(h, EE_UNITS_UB_HAS_DAMAGE_EFFECT, new_class)) || old == 0 || !yes(sa(h, EE_UNITS_UB_IS_BUILT, old)) {
        return;
    }
    if fraction < flt(sa(h, EE_UNITS_UB_DAMAGE_PARAM2, unit_class)) {
        sa(h, EE_UNITS_UB_CREATE_DAMAGE_EFFECT, new);
    }
}

/// EEConvertUnit's adaptation power: a civilization power the unit's owner has and the new one lacks, else a random
/// upgrade it bought that the new one lacks, bought for the new owner at no cost (the chat line for the local player).
fn adaptation<H: UbHost + ?Sized>(h: &mut H, unit: u64, player: u64) {
    s(h, EE_UNITS_UB_STRINGS_RESET);
    let mut upgrade = 0i64;
    let mut granted = false;
    let first = 0x26u32;
    let mut i = first;
    while i < (s(h, EE_UNITS_UB_NUM_POWERS) as u32).wrapping_add(first) {
        if yes(sac(h, EE_UNITS_UB_UNIT_HAS_POWER, unit, i64::from(i)))
            && !yes(sac(h, EE_UNITS_UB_HAS_POWER, player, i64::from(i)))
        {
            upgrade = sc(h, EE_UNITS_UB_MAKE_POWER_UPGRADE, i64::from(i));
            sc(h, EE_UNITS_UB_POWER_NAME, i64::from(i));
            granted = true;
            break;
        }
        i += 1;
    }
    if !granted {
        sa(h, EE_UNITS_UB_UPGRADES_COPY, unit);
        s(h, EE_UNITS_UB_UPGRADES_SHUFFLE);
        let n = s(h, EE_UNITS_UB_UPGRADES_COUNT);
        let list: Vec<i64> = (0..n).map(|k| sc(h, EE_UNITS_UB_UPGRADE_AT, k)).collect();
        for value in list {
            if !yes(sac(h, EE_UNITS_UB_HAS_UPGRADE, player, value)) {
                upgrade = value;
                sc(h, EE_UNITS_UB_UPGRADE_NAME, upgrade);
                granted = true;
                break;
            }
        }
    }
    if !granted {
        return;
    }
    let points = sa(h, EE_UNITS_UB_AVAILABLE_POINTS, player);
    sac(h, EE_UNITS_UB_CAN_AFFORD, player, upgrade);
    sa(h, EE_UNITS_UB_SET_POINTS_AND_MAX, player);
    sac(h, EE_UNITS_UB_BUY, player, upgrade);
    sac(h, EE_UNITS_UB_SET_POINTS, player, points);
    if obj_c(h, EE_UNITS_UB_CURRENT_PLAYER, 0) == player {
        s(h, EE_UNITS_UB_ADAPTATION_FORMAT);
        s(h, EE_UNITS_UB_ADAPTATION_MESSAGE);
        sa(h, EE_UNITS_UB_ADAPTATION_CHAT, unit);
    }
}

/// `EEMakeUnitCopy(unit, player, class)`: a copy of the unit (or a unit of `class`) for the player on its tile, in
/// its state (a built land building set built again); the new `EEUnit*`.
pub fn make_unit_copy<H: UbHost + ?Sized>(h: &mut H, unit: u64, player: i64, class: u64) -> u64 {
    let class = if class == 0 { obj(h, EE_UNITS_UB_UNIT_CLASS, unit) } else { class };
    let y = sa(h, EE_UNITS_UB_TILE_Y, unit);
    let x = sa(h, EE_UNITS_UB_TILE_X, unit);
    let foundation = sa(h, EE_UNITS_UB_IS_FOUNDATION, unit);
    let new = h.ub(EE_UNITS_UB_CREATE_UNIT, class, foundation as u64, x, y, player, &mut [0.0; 4]) as u64;
    sab(h, EE_UNITS_UB_COPY_FROM, new, unit);
    if obj(h, EE_UNITS_UB_TILE_NODE, new) != 0 {
        let new_class = obj(h, EE_UNITS_UB_UNIT_CLASS, new);
        if yes(sa(h, EE_UNITS_UB_IS_LAND_BUILDING, new_class)) {
            let complex = obj(h, EE_UNITS_UB_COMPLEX_OF, new);
            if yes(sa(h, EE_UNITS_UB_IS_BUILT, complex)) {
                h.ub(EE_UNITS_UB_SET_BUILT, complex, 0, 0, 0, 1, &mut [0.0; 4]);
                h.ub(EE_UNITS_UB_SET_BUILT, complex, 0, 1, 0, 1, &mut [0.0; 4]);
            }
        }
    }
    new
}

/// `EEReplaceUnit(unit, player, class)` (pack/unpack): a new unit of `class` with the hit point share, the groups and
/// the effects; the new complex unit.
pub fn replace_unit<H: UbHost + ?Sized>(h: &mut H, unit: u64, player: i64, class: u64) -> u64 {
    let note = Note::begin(85);
    note.u(unit).i(player).c(class);
    let new_unit = make_unit_copy(h, unit, player, class);
    let new = obj(h, EE_UNITS_UB_COMPLEX_OF, new_unit);
    let fraction = flt(sa(h, EE_UNITS_UB_HIT_FRACTION, unit));
    let max = sa(h, EE_UNITS_UB_MAX_HIT_POINTS, new);
    let scaled = ee_base::math::um::lrintf(max as i32 as f32 * fraction);
    let hit_points = if scaled > 1 { scaled } else { 1 };
    sac(h, EE_UNITS_UB_SET_HIT_POINTS, new, i64::from(hit_points));
    let old = obj(h, EE_UNITS_UB_COMPLEX_OF, unit);
    sab(h, EE_UNITS_UB_QUEUE_SWAP, old, new);
    if obj(h, EE_UNITS_UB_PREVIOUS_PART, old) == 0 {
        sab(h, EE_UNITS_UB_GATE_REPLACED, unit, new);
    }
    let new_player = obj(h, EE_UNITS_UB_PLAYER_OF, new);
    if obj(h, EE_UNITS_UB_PLAYER_OF, old) == new_player {
        for i in 0..20 {
            let group = obj_ac(h, EE_UNITS_UB_GROUP_AT, new_player, i);
            if group == 0 {
                continue;
            }
            let n = sa(h, EE_UNITS_UB_GROUP_NUM_UNITS, group) as u32;
            for j in 0..n {
                if obj_ac(h, EE_UNITS_UB_GROUP_UNIT, group, i64::from(j)) == old {
                    sab(h, EE_UNITS_UB_GROUP_ADD, group, new);
                    break;
                }
            }
        }
    }
    sac(h, EE_UNITS_UB_KILL, unit, 2);
    sa(h, EE_UNITS_UB_DROP_UNSUITED_GOALS, new);
    damage_effect(h, unit, new, old, fraction, false);
    let new_class = obj(h, EE_UNITS_UB_UNIT_CLASS, new);
    if sa(h, EE_UNITS_UB_CLASS_TYPE, new_class) == UNIT_TYPE_WALL_SPACE
        && !yes(sa(h, EE_UNITS_UB_IS_FOUNDATION, new))
        && {
            let player = obj(h, EE_UNITS_UB_PLAYER_OF, new);
            yes(sac(h, EE_UNITS_UB_TECH_AVAILABLE, player, TECH_WALL_SPACE))
        }
    {
        space_energy_wall(h, new, false);
    } else if sa(h, EE_UNITS_UB_CATEGORY, new_class) == CATEGORY_GATE
        && obj(h, EE_UNITS_UB_PREVIOUS_PART, new) == 0
        && obj(h, EE_UNITS_UB_NEXT_PART, new) != 0
        && {
            let player = obj(h, EE_UNITS_UB_PLAYER_OF, new);
            yes(sac(h, EE_UNITS_UB_TECH_AVAILABLE, player, TECH_GATE_SPACE))
        }
        && sa(h, EE_UNITS_UB_CLASS_TYPE, new_class) != UNIT_TYPE_GATE_PALISADES
    {
        space_energy_wall(h, new, true);
    }
    note.u(new).i(sa(h, EE_UNITS_UB_HIT_POINTS, new));
    new
}

fn obj_ac<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64, c: i64) -> u64 {
    sac(h, op, a, c) as u64
}

/// `EECreateSpaceWallEnergyWall` / `EECreateSpaceGateEnergyWall(unit)`: the energy wall effect, kept as the unit's
/// fire effect.
pub fn space_energy_wall<H: UbHost + ?Sized>(h: &mut H, unit: u64, gate: bool) {
    sac(h, EE_UNITS_UB_ENERGY_WALL, unit, i64::from(gate));
}

/// `EEMakeBuilding(player, type, x, y)`: a paid building site (one hit point) of the type for the player at the
/// tile; the new `EEUnit*`.
pub fn make_building<H: UbHost + ?Sized>(h: &mut H, player: u64, unit_type: i64, x: i64, y: i64) -> u64 {
    let class = obj_c(h, EE_UNITS_UB_CLASS_OF_TYPE, unit_type);
    let player_id = sa(h, EE_UNITS_UB_PLAYER_ID, player);
    let unit = h.ub(EE_UNITS_UB_CREATE_UNIT, class, 1, x, y, player_id, &mut [0.0; 4]) as u64;
    sac(h, EE_UNITS_UB_SET_HIT_POINTS, unit, 1);
    let visibility = sab(h, EE_UNITS_UB_TILE_VISIBILITY, unit, player) as u32;
    if visibility > 0 && sab(h, EE_UNITS_UB_TILE_VISIBILITY, unit, player) as u32 <= 1 {
        sa(h, EE_UNITS_UB_ADD_FOR_UNSEEN, unit);
    }
    if unit_type == UNIT_TYPE_AIRPORT {
        // `dynamic_cast<EEComplexUnit*>`: the created unit itself (a complex unit, at the same address)
        let mut link = unit;
        loop {
            link = obj(h, EE_UNITS_UB_NEXT_PART, link);
            if link == 0 {
                break;
            }
            sac(h, EE_UNITS_UB_SET_HIT_POINTS, link, 1);
        }
    }
    let unit_class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    let t = sa(h, EE_UNITS_UB_CLASS_TYPE, unit_class);
    if t != UNIT_TYPE_FARM && t != UNIT_TYPE_ROBOTIC_FARM {
        sab(h, EE_UNITS_UB_PAY_CLASS, player, class);
        if obj(h, EE_UNITS_UB_TILE_NODE, unit) != 0 {
            let complex = obj(h, EE_UNITS_UB_COMPLEX_OF, unit);
            if !yes(sa(h, EE_UNITS_UB_IS_PAID_FOR, complex)) {
                sa(h, EE_UNITS_UB_SET_PAID_FOR, complex);
            }
        }
    }
    sa(h, EE_UNITS_UB_ADD_PENDING_UNITS, player);
    s(h, EE_UNITS_UB_FLUSH_PLACEMENTS);
    let unit_class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    let t = sa(h, EE_UNITS_UB_CLASS_TYPE, unit_class);
    if t == UNIT_TYPE_GRANARY_IMPROVED || t == UNIT_TYPE_GRANARY {
        sa(h, EE_UNITS_UB_REPLANT_FARMS, unit);
    }
    unit
}

/// `EEReplaceQueuedTechnology(player, class, new)`: every living unit of the player whose class makes the class's
/// technology replaces it in its queue.
pub fn replace_queued_technology<H: UbHost + ?Sized>(h: &mut H, player: u64, class: u64, new_technology: i64) {
    let id = sa(h, EE_UNITS_UB_CLASS_TECHNOLOGY, class);
    let technology = obj_ac(h, EE_UNITS_UB_TECHNOLOGY, player, id);
    if technology == 0 {
        return;
    }
    // one unit list for every class: the search appends to it, as the original's did
    s(h, EE_UNITS_UB_LISTS_CLEAR);
    let mut i = 0u32;
    while i < sa(h, EE_UNITS_UB_TECH_NUM_CLASSES, technology) as u32 {
        let unit_class = sac(h, EE_UNITS_UB_TECH_CLASS, technology, i64::from(i));
        if sac(h, EE_UNITS_UB_UNITS_OF_TYPE, player, unit_class) as i32 > 0 {
            // the list copied before any call (a nested body reuses the host's list)
            for u in unit_list(h) {
                let complex = obj(h, EE_UNITS_UB_COMPLEX_OF, u);
                if sa(h, EE_UNITS_UB_HIT_POINTS, complex) as i32 > 0 {
                    h.ub(EE_UNITS_UB_REPLACE_RESEARCH, complex, 0, id, new_technology, 0, &mut [0.0; 4]);
                }
            }
        }
        i += 1;
    }
}

/// `EEWorkFarm(target, unit)`: the unit gathers from a built, whole farm, else builds or repairs it.
pub fn work_farm<H: UbHost + ?Sized>(h: &mut H, target: u64, unit: u64) {
    let complete = yes(sa(h, EE_UNITS_UB_IS_BUILT, target))
        && sa(h, EE_UNITS_UB_HIT_POINTS, target) == sa(h, EE_UNITS_UB_MAX_HIT_POINTS, target);
    if complete {
        sab(h, EE_UNITS_UB_SET_GATHER_GOAL, unit, target);
        sac(h, EE_UNITS_UB_START_ACTION, unit, ACTION_MOVE_TO_RESOURCE);
    } else {
        sab(h, EE_UNITS_UB_SET_REPAIR_GOAL, unit, target);
        sac(h, EE_UNITS_UB_START_ACTION, unit, ACTION_MOVE_TO_REPAIR_SITE);
    }
    let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    sab(h, EE_UNITS_UB_ACTION_CHANGED, class, unit);
}

/// `EERetargetAttack(unit, goal, target)`: the unit's attack goal moves on to the target and the attack restarts.
pub fn retarget_attack<H: UbHost + ?Sized>(h: &mut H, unit: u64, goal: u64, target: u64) {
    let old = obj(h, EE_UNITS_UB_ATTACK_TARGET, goal);
    if target != old {
        h.ub(EE_UNITS_UB_GOAL_REPLACE_UNIT, goal, old, target as i64, 0, 0, &mut [0.0; 4]);
    }
    sa(h, EE_UNITS_UB_CLEAR_COLLISION_COUNT, unit);
    sa(h, EE_UNITS_UB_CLEAR_BLOCKING, unit);
    if sa(h, EE_UNITS_UB_COLLISION_STATE, unit) != 0 {
        sa(h, EE_UNITS_UB_CLEAR_COLLISION_STATE, unit);
    }
    sa(h, EE_UNITS_UB_DROP_PATH, unit);
    sac(h, EE_UNITS_UB_START_ACTION, unit, ACTION_CHECK_RANGE);
    let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    sab(h, EE_UNITS_UB_ACTION_CHANGED, class, unit);
}

/// `EEFindNewTargetNearOld(unit, target)`: another piece of a linked target (wall, gate), else the nearest living
/// enemy near it.
pub fn find_new_target_near_old<H: UbHost + ?Sized>(h: &mut H, unit: u64, target: u64) {
    sab(h, EE_UNITS_UB_IGNORE_TARGET, unit, target);
    if obj(h, EE_UNITS_UB_PREVIOUS_PART, target) == 0 && obj(h, EE_UNITS_UB_NEXT_PART, target) == 0 {
        let x = sa(h, EE_UNITS_UB_TILE_X, target);
        let y = sa(h, EE_UNITS_UB_TILE_Y, target);
        if !yes(h.ub(EE_UNITS_UB_FIND_NEW_TARGET, target, unit, x, y, 2, &mut [0.0; 4])) {
            sa(h, EE_UNITS_UB_NO_TARGET_TRACE, target);
        }
        return;
    }
    let carrier = obj(h, EE_UNITS_UB_OUTERMOST_CARRIER, target);
    let mut link = obj(h, EE_UNITS_UB_NEXT_PART, carrier);
    while obj(h, EE_UNITS_UB_NEXT_PART, link) != 0 {
        let class = obj(h, EE_UNITS_UB_UNIT_CLASS, link);
        if !yes(sab(h, EE_UNITS_UB_PART_LEFT_OUT, class, link))
            && !yes(sabc(h, EE_UNITS_UB_TARGET_IGNORED, unit, link, 0))
        {
            let goal = obj(h, EE_UNITS_UB_LOAD_ATTACK_GOAL, unit);
            retarget_attack(h, unit, goal, link);
            return;
        }
        link = obj(h, EE_UNITS_UB_NEXT_PART, link);
    }
    let target_class = obj(h, EE_UNITS_UB_UNIT_CLASS, target);
    if sa(h, EE_UNITS_UB_CATEGORY, target_class) == CATEGORY_GATE {
        let carrier = obj(h, EE_UNITS_UB_OUTERMOST_CARRIER, link);
        let first = sab(h, EE_UNITS_UB_LAST_LINKED_CARRIER, target_class, carrier) as u64;
        let first_carrier = obj(h, EE_UNITS_UB_OUTERMOST_CARRIER, first);
        let second = sab(h, EE_UNITS_UB_LAST_LINKED, target_class, first_carrier) as u64;
        let distance = flt(sab(h, EE_UNITS_UB_DISTANCE_SQUARED_TO, unit, first));
        link = first;
        if flt(sab(h, EE_UNITS_UB_DISTANCE_SQUARED_TO, unit, second)) < distance {
            link = second;
        }
    }
    let x = sa(h, EE_UNITS_UB_TILE_X, link);
    let y = sa(h, EE_UNITS_UB_TILE_Y, link);
    if !yes(h.ub(EE_UNITS_UB_FIND_NEW_TARGET, link, unit, x, y, 1, &mut [0.0; 4])) {
        sa(h, EE_UNITS_UB_NO_TARGET_TRACE, link);
    }
}

fn point<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64) -> [f32; 3] {
    let mut f = [0.0f32; 4];
    h.ub(op, a, 0, 0, 0, 0, &mut f);
    [f[0], f[1], f[2]]
}

/// `EEGetHelicopterPriestDestination(unit, out)`: where the unit casts its calamity from, 90% of the calamity's range
/// from the target along the line to the unit (the point at `out` untouched without the technology).
pub fn helicopter_priest_destination<H: UbHost + ?Sized>(h: &mut H, unit: u64, out: u64) {
    let note = Note::begin(64);
    note.u(unit);
    if note.on() {
        note.p(point(h, EE_UNITS_UB_POINT, out));
    }
    let goal = obj(h, EE_UNITS_UB_CALAMITY_GOAL, unit);
    let target = point(h, EE_UNITS_UB_GOAL_LOCATION, goal);
    let calamity = sa(h, EE_UNITS_UB_CALAMITY_TYPE, goal);
    let technology = obj_ac(h, EE_UNITS_UB_UNIT_TECHNOLOGY, unit, calamity);
    if technology == 0 {
        if note.on() {
            note.p(point(h, EE_UNITS_UB_POINT, out));
        }
        return;
    }
    let range = sa(h, EE_UNITS_UB_CALAMITY_RANGE, technology);
    let position = point(h, EE_UNITS_UB_POSITION, unit);
    let mut f = [position[0] - target[0], position[1] - target[1], position[2] - target[2], 0.0];
    h.ub(EE_UNITS_UB_DIRECTION_SET, 0, 0, 0, 0, 0, &mut f);
    s(h, EE_UNITS_UB_DIRECTION_NORMALIZE);
    let mut f = [(range as u32) as f32 * 0.9f32, 0.0, 0.0, 0.0];
    h.ub(EE_UNITS_UB_DIRECTION_SCALE, 0, 0, 0, 0, 0, &mut f);
    let mut p = [target[0] + f[0], target[1] + f[1], target[2] + f[2], 0.0];
    h.ub(EE_UNITS_UB_SET_POINT, out, 0, 0, 0, 0, &mut p);
    if note.on() {
        note.p(point(h, EE_UNITS_UB_POINT, out));
    }
}

fn matrix<H: UbHost + ?Sized>(h: &mut H, op: i32, a: u64) -> [[f32; 4]; 4] {
    let mut m = [[0.0f32; 4]; 4];
    h.ub(op, a, 0, &mut m as *mut _ as usize as i64, 0, 0, &mut [0.0; 4]);
    m
}

/// `EEUpdateUnitTile(transformation, unit, out_x, out_y)`: the unit moved to the transformation; off the map it
/// snaps to the closest tile (an aircraft's height follows its next waypoint). `out_x`, `out_y` are `long*`.
pub fn update_unit_tile<H: UbHost + ?Sized>(h: &mut H, transformation: u64, unit: u64, out_x: u64, out_y: u64) {
    let note = Note::begin(81);
    if note.on() {
        note.m(&matrix(h, EE_UNITS_UB_TRANSFORMATION_MATRIX, transformation));
    }
    note.u(unit);
    let p = point(h, EE_UNITS_UB_TRANSFORMATION_POSITION, transformation);
    let x = i64::from(ee_base::math::um::floor(p[0]));
    sac(h, EE_UNITS_UB_SET_LONG, out_x, x);
    let y = i64::from(ee_base::math::um::floor(p[1]));
    sac(h, EE_UNITS_UB_SET_LONG, out_y, y);
    let (mut x, mut y) = (x, y);
    if yes(h.ub(EE_UNITS_UB_TILE_EXISTS, 0, 0, x, y, 0, &mut [0.0; 4])) {
        sab(h, EE_UNITS_UB_SET_TRANSFORMATION, unit, transformation);
    } else {
        let mut f = [p[0], p[1], p[2], 0.0];
        h.ub(EE_UNITS_UB_CLOSEST_TILE, 0, 0, out_x as i64, out_y as i64, 0, &mut f);
        sab(h, EE_UNITS_UB_SET_LOCATION, unit, transformation);
        x = read_long(h, out_x);
        y = read_long(h, out_y);
        h.ub(EE_UNITS_UB_MOVE_TO, unit, 0, x, y, 0, &mut [0.0; 4]);
        let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
        if sa(h, EE_UNITS_UB_WAYPOINT_COUNT, unit) as i32 > 0 && sa(h, EE_UNITS_UB_DOMAINS, class) & MOVEMENT_AIR != 0 {
            sa(h, EE_UNITS_UB_NEXT_WAYPOINT_POINT, unit);
            let height = sa(h, EE_UNITS_UB_HEIGHT_AT, unit);
            if flt(height) != flt(sa(h, EE_UNITS_UB_HEIGHT, unit)) {
                sac(h, EE_UNITS_UB_SET_HEIGHT, unit, height);
            }
        }
    }
    if note.on() {
        note.i(x).i(y).m(&matrix(h, EE_UNITS_UB_LOCATION_MATRIX, unit));
    }
}

fn read_long<H: UbHost + ?Sized>(h: &mut H, at: u64) -> i64 {
    sa(h, EE_UNITS_UB_READ_LONG, at)
}

/// `EEGetWaitingGatherer(unit)`: a unit around the given one (searched in a random rotation) whose gather goal
/// waits to work on it.
pub fn waiting_gatherer<H: UbHost + ?Sized>(h: &mut H, unit: u64) -> u64 {
    if yes(s(h, EE_UNITS_UB_SHUTDOWN)) {
        return 0;
    }
    s(h, EE_UNITS_UB_LISTS_CLEAR);
    sa(h, EE_UNITS_UB_UNIT_TILES, unit);
    s(h, EE_UNITS_UB_UNITS_ON_TILES);
    let random = flt(s(h, EE_UNITS_UB_RANDOM_GENERATE));
    sc(h, EE_UNITS_UB_PERMUTE_UNITS, i64::from(ee_base::math::um::lrintf(random)));
    for candidate in unit_list(h) {
        let complex = obj(h, EE_UNITS_UB_COMPLEX_OF, candidate);
        if complex == 0 {
            continue;
        }
        if sa(h, EE_UNITS_UB_COMMAND, complex) != ACTION_WAIT_FOR_OPEN_POSITION {
            continue;
        }
        let state = obj(h, EE_UNITS_UB_GOAL_STATE, complex);
        if state == 0 || sa(h, EE_UNITS_UB_GOAL_KIND, state) != GOAL_GATHER {
            continue;
        }
        let gather = obj(h, EE_UNITS_UB_GATHER_GOAL, state);
        if gather != 0 && obj(h, EE_UNITS_UB_GATHER_RESOURCE, gather) == unit {
            return complex;
        }
    }
    0
}

/// The host's unit list, copied (its size, then each unit).
fn unit_list<H: UbHost + ?Sized>(h: &mut H) -> Vec<u64> {
    let n = s(h, EE_UNITS_UB_LIST_SIZE);
    (0..n).map(|k| obj_c(h, EE_UNITS_UB_LIST_UNIT, k)).collect()
}

/// `EEUpgradeHitPoints(upgrade, type, factor, player)`: the upgrade's hit points raised by `factor` of its class's
/// base; the player's units of the type follow (not while a saved game loads).
pub fn upgrade_hit_points<H: UbHost + ?Sized>(h: &mut H, upgrade: u64, unit_type: i64, factor: f32, player: u64) {
    let base = sa(h, EE_UNITS_UB_UPGRADE_BASE_HIT_POINTS, upgrade) as i32;
    let max = sa(h, EE_UNITS_UB_UPGRADE_MAX_HIT_POINTS, upgrade) as i32;
    let max2 = sa(h, EE_UNITS_UB_UPGRADE_MAX_HIT_POINTS, upgrade) as i32;
    let ratio = (base as f32 * factor + max as f32) / max2 as f32;
    let functor = h.ub(EE_UNITS_UB_MAKE_UPGRADER, 0, 0, unit_type, bits(ratio), 0, &mut [0.0; 4]) as u64;
    let bonus = sa(h, EE_UNITS_UB_UPGRADE_BASE_HIT_POINTS, upgrade) as i32;
    if bonus <= 0 {
        return;
    }
    let factor = factor * bonus as f32;
    let max = sa(h, EE_UNITS_UB_UPGRADE_MAX_HIT_POINTS, upgrade) as i32;
    let hit_points = ee_base::math::um::lrintf(max as f32 + factor).max(1);
    sac(h, EE_UNITS_UB_SET_UPGRADE_MAX_HIT_POINTS, upgrade, i64::from(hit_points.max(1)));
    if !yes(s(h, EE_UNITS_UB_LOADING_SAVED_GAME)) {
        for_each_unit_of(h, player, functor, true);
    }
}

/// `EEHitPointUpgrader::Apply(unit)`: a living, finished unit of the upgraded type scaled by the ratio (at least 1).
pub fn hit_point_apply<H: UbHost + ?Sized>(h: &mut H, functor: u64, unit: u64) {
    let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    if sa(h, EE_UNITS_UB_CLASS_TYPE, class) as u32 != sa(h, EE_UNITS_UB_UPGRADER_TYPE, functor) as u32 {
        return;
    }
    if sa(h, EE_UNITS_UB_HIT_POINTS, unit) as i32 <= 0 || yes(sa(h, EE_UNITS_UB_IS_FOUNDATION, unit)) {
        return;
    }
    let hit_points = sa(h, EE_UNITS_UB_HIT_POINTS, unit) as i32;
    if hit_points > 0 {
        let ratio = flt(sa(h, EE_UNITS_UB_UPGRADER_RATIO, functor));
        let scaled = ee_base::math::um::lrintf(hit_points as f32 * ratio).max(1);
        sac(h, EE_UNITS_UB_SET_HIT_POINTS, unit, i64::from(scaled));
    }
}

/// `EEForEachUnitOf(player, functor, pending)`: the functor over the player's units (each unit, then its complex
/// unit) and, when asked, its pending units.
pub fn for_each_unit_of<H: UbHost + ?Sized>(h: &mut H, player: u64, functor: u64, pending: bool) {
    if player == 0 {
        return;
    }
    let mut unit = obj(h, EE_UNITS_UB_FIRST_UNIT, player);
    while unit != 0 {
        sab(h, EE_UNITS_UB_APPLY_TO_UNIT, functor, unit);
        let complex = obj(h, EE_UNITS_UB_COMPLEX_OF, unit);
        if complex != 0 {
            sab(h, EE_UNITS_UB_APPLY, functor, complex);
        }
        unit = obj(h, EE_UNITS_UB_NEXT_UNIT, unit);
    }
    if !pending {
        return;
    }
    let mut i = 0u32;
    while i < sa(h, EE_UNITS_UB_NUM_PENDING, player) as u32 {
        let u = obj_ac(h, EE_UNITS_UB_PENDING_UNIT, player, i64::from(i));
        sab(h, EE_UNITS_UB_APPLY_TO_UNIT, functor, u);
        let u = obj_ac(h, EE_UNITS_UB_PENDING_UNIT, player, i64::from(i));
        let complex = obj(h, EE_UNITS_UB_COMPLEX_OF, u);
        if complex != 0 {
            sab(h, EE_UNITS_UB_APPLY, functor, complex);
        }
        i += 1;
    }
}

/// `EEForEachPlayersUnit(functor)`: the functor over every player's units but nature's.
pub fn for_each_players_unit<H: UbHost + ?Sized>(h: &mut H, functor: u64) {
    let mut i = 1u32;
    while i < s(h, EE_UNITS_UB_NUM_PLAYERS) as u32 {
        let player = obj_c(h, EE_UNITS_UB_PLAYER_AT, i64::from(i));
        for_each_unit_of(h, player, functor, false);
        i += 1;
    }
}

/// `EEMovePassengers(unit, container, conversion)`: the container's cargo into the unit (converted to its owner for
/// a conversion).
pub fn move_passengers<H: UbHost + ?Sized>(h: &mut H, unit: u64, container: u64, conversion: bool) {
    let player = obj(h, EE_UNITS_UB_PLAYER_OF, unit);
    if container == 0 || sa(h, EE_UNITS_UB_NUM_CARGO, container) as u32 == 0 {
        return;
    }
    while sa(h, EE_UNITS_UB_NUM_CARGO, container) as u32 != 0 {
        let last = sa(h, EE_UNITS_UB_NUM_CARGO, container) - 1;
        let mut cargo = obj_ac(h, EE_UNITS_UB_UNLOAD_CARGO_AT, container, last);
        if conversion {
            let id = sa(h, EE_UNITS_UB_PLAYER_ID, player);
            cargo = convert_unit(h, cargo, id, true);
        }
        sabc(h, EE_UNITS_UB_LOAD_CARGO, unit, cargo, i64::from(conversion));
    }
}

/// `EEGetPlayerClassGraphic(player, class)`: the foundation model the player uses for a building class of its size.
pub fn player_class_graphic<H: UbHost + ?Sized>(h: &mut H, player: u64, class: u64) -> u64 {
    let t = sa(h, EE_UNITS_UB_CLASS_TYPE, class);
    if t == UNIT_TYPE_ROBOTIC_FARM {
        return obj_c(h, EE_UNITS_UB_TABLE_MODEL, UNIT_TYPE_SPACE_FALLOW_FARM);
    }
    let upgrade = if t != UNIT_TYPE_AIRPORT {
        match sa(h, EE_UNITS_UB_TILE_SIZE, class) as i32 {
            1 => FOUNDATION_1,
            2 => FOUNDATION_2,
            3 | 4 => FOUNDATION_3,
            _ => UNIT_TYPE_FOUNDATION6X6,
        }
    } else {
        UNIT_TYPE_FOUNDATION_AIRPORT
    };
    obj_ac(h, EE_UNITS_UB_UPGRADE_MODEL, player, upgrade)
}

/// `EEFindTargetNear(unit, x, y, scale)`: the best target around the tile when the influence table shows enemies
/// within the unit's reach (anti-air ammunition always searches).
pub fn find_target_near<H: UbHost + ?Sized>(h: &mut H, unit: u64, x: i64, y: i64, scale: f32) -> u64 {
    let range = flt(sa(h, EE_UNITS_UB_RANGE, unit)) * scale;
    let reach = flt(sa(h, EE_UNITS_UB_LINE_OF_SIGHT, unit));
    let player = obj(h, EE_UNITS_UB_PLAYER_OF, unit);
    let player_id = sa(h, EE_UNITS_UB_PLAYER_ID, player);
    let radius = i64::from(if range > reach { ceil(range) } else { ceil(reach) });
    let x = x as i32 as i64;
    let y = y as i32 as i64;
    let r = radius as i32;
    sc(h, EE_UNITS_UB_AREA_TOP, i64::from((y as i32).wrapping_sub(r)));
    sc(h, EE_UNITS_UB_AREA_BOTTOM, i64::from(r.wrapping_add(y as i32)));
    sc(h, EE_UNITS_UB_AREA_LEFT, i64::from((x as i32).wrapping_sub(r)));
    sc(h, EE_UNITS_UB_AREA_RIGHT, i64::from(r.wrapping_add(x as i32)));
    let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    let attacks =
        flt(sa(h, EE_UNITS_UB_ATTACK, unit)) != 0.0 || sa(h, EE_UNITS_UB_CATEGORY, class) == CATEGORY_BUILDING_PRIEST;
    if !attacks {
        return 0;
    }
    if sa(h, EE_UNITS_UB_AMMO_TYPE, class) == AMMO_ANTI_AIR
        || sc(h, EE_UNITS_UB_SUM_OTHERS_INFLUENCE, player_id) as i32 > 0
    {
        return h.ub(EE_UNITS_UB_SEARCH_TARGET, unit, 0, x, y, bits(scale), &mut [0.0; 4]) as u64;
    }
    0
}

/// `EEFindNearbyTarget(unit)`: a target near the unit (along the unit list for anti-air ammunition).
pub fn find_nearby_target<H: UbHost + ?Sized>(h: &mut H, unit: u64) -> u64 {
    let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    let x = sa(h, EE_UNITS_UB_TILE_X, unit);
    let y = sa(h, EE_UNITS_UB_TILE_Y, unit);
    if sa(h, EE_UNITS_UB_AMMO_TYPE, class) != AMMO_ANTI_AIR {
        return find_target_near(h, unit, x, y, 1.0);
    }
    h.ub(EE_UNITS_UB_SEARCH_ALONG_LIST, unit, 0, x, y, bits(1.0), &mut [0.0; 4]) as u64
}

/// `EECanBallisticHit(unit, target)`: whether the unit's shot from its attach point reaches the target's drop
/// location (a clear line unless it shoots high or over walls).
pub fn can_ballistic_hit<H: UbHost + ?Sized>(h: &mut H, unit: u64, target: u64) -> bool {
    let mut drop = [0.0f32; 3];
    let mut tag = [0.0f32; 3];
    let mut blocker = 0u64;
    let mut b = 0i64;
    let mut c = 0i64;
    let mut angle = 0.0f32;
    let at = |p: &mut [f32; 3]| p as *mut _ as usize as u64;
    sab(h, EE_UNITS_UB_APPROACH_ATTACH_POINT, unit, at(&mut tag));
    let mut result = !yes(sab(h, EE_UNITS_UB_OTHER_CONTINENT, unit, target));
    if result {
        sabc(h, EE_UNITS_UB_FIND_DROP_LOCATION, unit, target, at(&mut drop) as i64);
        result = yes(h.ub(
            EE_UNITS_UB_BALLISTIC_PITCH,
            unit,
            at(&mut tag),
            at(&mut drop) as i64,
            &mut angle as *mut f32 as usize as i64,
            0,
            &mut [0.0; 4],
        ));
        let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
        if result && !yes(sa(h, EE_UNITS_UB_SHOOTS_HIGH, class)) && !yes(sa(h, EE_UNITS_UB_FIRES_OVER_WALLS, unit)) {
            result = !yes(h.ub(
                EE_UNITS_UB_LINE_TO_UNIT_BLOCKED,
                unit,
                target,
                &mut blocker as *mut u64 as usize as i64,
                &mut b as *mut i64 as usize as i64,
                &mut c as *mut i64 as usize as i64,
                &mut [0.0; 4],
            ));
        }
    }
    result
}

/// `EECanPriestConvert(unit, target)`: a priest that can convert the target.
pub fn can_priest_convert<H: UbHost + ?Sized>(h: &mut H, unit: u64, target: u64) -> bool {
    let class = obj(h, EE_UNITS_UB_UNIT_CLASS, unit);
    let mut flag = 0u8;
    target != 0
        && yes(sa(h, EE_UNITS_UB_IS_PRIEST, class))
        && yes(sabc(h, EE_UNITS_UB_CAN_CONVERT, unit, target, &mut flag as *mut u8 as usize as i64))
}

/// `EEPreviewWall(tiles, technology, player)`: the wall ghosts of a wall line.
pub fn preview_wall<H: UbHost + ?Sized>(h: &mut H, tiles: u64, technology: i64, player_index: i64) {
    let player = obj_c(h, EE_UNITS_UB_PLAYER_AT, player_index);
    s(h, EE_UNITS_UB_CORNERS_CLEAR);
    sac(h, EE_UNITS_UB_DROP_TILES, tiles, 1);
    sac(h, EE_UNITS_UB_DROP_TILES, tiles, 0);
    sa(h, EE_UNITS_UB_PICK_WALL_PIECES, tiles);
    sabc(h, EE_UNITS_UB_BUILD_WALL_GHOSTS, tiles, player, technology);
}

/// `EETakeOverLoad(unit, other)`: the other's garrison into the unit (as much as its class allows), the other emptied.
pub fn take_over_load<H: UbHost + ?Sized>(h: &mut H, unit: u64, other: u64) {
    sab(h, EE_UNITS_UB_TAKE_GARRISON, unit, other);
    sa(h, EE_UNITS_UB_CLEAR_GARRISON, other);
}

/// Runs body `what` (`EE_UNITS_UB_DO_*`; a float argument as its bits).
pub fn run<H: UbHost + ?Sized>(h: &mut H, what: i32, a: u64, b: u64, c: i64, d: i64, e: i64) -> i64 {
    match what {
        EE_UNITS_UB_DO_CONVERT_UNIT => convert_unit(h, a, c, c_bool(d)) as i64,
        EE_UNITS_UB_DO_UPGRADE_TEXT => i64::from(upgrade_text(h, c, b)),
        EE_UNITS_UB_DO_MAKE_UNIT_COPY => make_unit_copy(h, a, c, b) as i64,
        EE_UNITS_UB_DO_REPLACE_UNIT => replace_unit(h, a, c, b) as i64,
        EE_UNITS_UB_DO_MAKE_BUILDING => make_building(h, a, c, d, e) as i64,
        EE_UNITS_UB_DO_REPLACE_QUEUED_TECHNOLOGY => {
            replace_queued_technology(h, a, b, c);
            0
        }
        EE_UNITS_UB_DO_WORK_FARM => {
            work_farm(h, a, b);
            0
        }
        EE_UNITS_UB_DO_RETARGET_ATTACK => {
            retarget_attack(h, a, b, c as u64);
            0
        }
        EE_UNITS_UB_DO_FIND_NEW_TARGET_NEAR_OLD => {
            find_new_target_near_old(h, a, b);
            0
        }
        EE_UNITS_UB_DO_HELICOPTER_PRIEST_DESTINATION => {
            helicopter_priest_destination(h, a, b);
            0
        }
        EE_UNITS_UB_DO_UPDATE_UNIT_TILE => {
            update_unit_tile(h, a, b, c as u64, d as u64);
            0
        }
        EE_UNITS_UB_DO_WAITING_GATHERER => waiting_gatherer(h, a) as i64,
        EE_UNITS_UB_DO_UPGRADE_HIT_POINTS => {
            upgrade_hit_points(h, a, c, flt(d), b);
            0
        }
        EE_UNITS_UB_DO_HIT_POINT_APPLY => {
            hit_point_apply(h, a, b);
            0
        }
        EE_UNITS_UB_DO_FOR_EACH_UNIT_OF => {
            for_each_unit_of(h, a, b, c_bool(c));
            0
        }
        EE_UNITS_UB_DO_FOR_EACH_PLAYERS_UNIT => {
            for_each_players_unit(h, a);
            0
        }
        EE_UNITS_UB_DO_MOVE_PASSENGERS => {
            move_passengers(h, a, b, c_bool(c));
            0
        }
        EE_UNITS_UB_DO_TAKE_OVER_LOAD => {
            take_over_load(h, a, b);
            0
        }
        EE_UNITS_UB_DO_PLAYER_CLASS_GRAPHIC => player_class_graphic(h, a, b) as i64,
        EE_UNITS_UB_DO_FIND_TARGET_NEAR => find_target_near(h, a, c, d, flt(e)) as i64,
        EE_UNITS_UB_DO_FIND_NEARBY_TARGET => find_nearby_target(h, a) as i64,
        EE_UNITS_UB_DO_CAN_BALLISTIC_HIT => i64::from(can_ballistic_hit(h, a, b)),
        EE_UNITS_UB_DO_CAN_PRIEST_CONVERT => i64::from(can_priest_convert(h, a, b)),
        EE_UNITS_UB_DO_PREVIEW_WALL => {
            preview_wall(h, a, c, d);
            0
        }
        EE_UNITS_UB_DO_SPACE_ENERGY_WALL => {
            space_energy_wall(h, a, c_bool(c));
            0
        }
        EE_UNITS_UB_DO_HAS_CIVILIZATION_UPGRADE => i64::from(has_civilization_upgrade(h, a, c)),
        _ => 0,
    }
}

fn c_bool(v: i64) -> bool {
    v & 0xff != 0
}

#[cfg(not(test))]
mod game {
    use super::*;

    extern "C" {
        fn ee_units_ub_host(op: i32, a: u64, b: u64, c: i64, d: i64, e: i64, f: *mut [f32; 4]) -> i64;
    }

    struct Game;

    impl UbHost for Game {
        fn ub(&mut self, op: i32, a: u64, b: u64, c: i64, d: i64, e: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the body runs inside the C++ function that handed over its live objects
            unsafe { ee_units_ub_host(op, a, b, c, d, e, f) }
        }
    }

    /// The unit utility `what` (`EE_UNITS_UB_DO_*`) over its arguments as the C++ function takes them.
    ///
    /// # Safety
    /// The objects are live game objects, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ub_body(what: i32, a: u64, b: u64, c: i64, d: i64, e: i64) -> i64 {
        run(&mut Game, what, a, b, c, d, e)
    }
}

#[cfg(test)]
#[path = "uu_body_tests.rs"]
mod tests;
