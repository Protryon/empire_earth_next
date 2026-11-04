// C:\EEX\Empire Earth\Units\EEUnitAction.cpp (file name inferred: no __FILE__; it sorts between EEUnit.cpp and
// EEUnitAI.cpp). The unit AI scripts: Unit AI Scripts\<name>.tai, parsed into EEUnitAction states whose
// EEUnitTriggers name the next action; the trigger and action name tables. 0x22a509..0x22d95f (its std template
// COMDATs close it at 0x22ce36..0x22d941).
#include <windows.h>
#include <string.h>
#include <stdlib.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include <map>

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitTrigger.h"
#include "ee/EEUnitAction.h"

extern FSPath* gDataPath;                   // 0x531908

std::map<UString, std::map<long, EEUnitAction*>*> EEUnitAction::sScripts;
// SYNTHETIC: EXE 0x62a509   (wrapper)
// SYNTHETIC: EXE 0x62a513   (ctor)
// SYNTHETIC: EXE 0x62a533   (registrar)
// SYNTHETIC: EXE 0x62a53f   (dtor)
std::map<UString, long> EEUnitAction::sActionIDs;
// SYNTHETIC: EXE 0x62a55a   (wrapper)
// SYNTHETIC: EXE 0x62a564   (ctor)
// SYNTHETIC: EXE 0x62a584   (registrar)
// SYNTHETIC: EXE 0x62a590   (dtor)
std::map<unsigned long, UString> EEUnitAction::sActionNames;
// SYNTHETIC: EXE 0x62a5ab   (wrapper)
// SYNTHETIC: EXE 0x62a5b5   (ctor)
// SYNTHETIC: EXE 0x62a5d5   (registrar)
// SYNTHETIC: EXE 0x62a5e1   (dtor)
std::map<UString, long> EEUnitTrigger::sTriggerIDs;
// SYNTHETIC: EXE 0x62a5fc   (wrapper)
// SYNTHETIC: EXE 0x62a606   (ctor)
// SYNTHETIC: EXE 0x62a626   (registrar)
// SYNTHETIC: EXE 0x62a632   (dtor)
std::map<unsigned long, UString> EEUnitTrigger::sTriggerNames;
// SYNTHETIC: EXE 0x62a64d   (wrapper)
// SYNTHETIC: EXE 0x62a657   (ctor)
// SYNTHETIC: EXE 0x62a677   (registrar)
// SYNTHETIC: EXE 0x62a683   (dtor)

// FUNCTION: EXE 0x62a69e
// Reads the next token of the script text up to one of inDelimiters (outside quotes); false at the end of the text
bool EEGetScriptToken(const char* inDelimiters, char*& ioCursor, UString& outToken)
{
    bool theInQuotes = false;
    outToken = "";
    while (*ioCursor != 0) {
        if (strchr(inDelimiters, *ioCursor) != NULL && !theInQuotes) {
            ioCursor++;
            break;
        }
        if (*ioCursor == '"')
            theInQuotes = !theInQuotes;
        outToken.Copy(ioCursor, 0, 1, outToken.GetLength(), false);
        ioCursor++;
    }
    return outToken.GetLength() > 0;
}

// FUNCTION: EXE 0x62a70f
// Strips a script's comments (a slash to the end of the line), folds white space into single blanks and puts a
// comma before the closing bracket of each anyof(/allof( list
void EEStripScript(char* ioText)
{
    char* theCopy = _strdup(ioText);
    bool theComment = false;
    bool theBlank = false;
    bool theInList = false;
    char* theRead = theCopy;
    char* theWrite = ioText;
    while (*theRead != 0) {
        if (theComment) {
            if (*theRead == '\n')
                theComment = false;
            theRead++;
        } else if (*theRead == '/') {
            theComment = true;
            theRead++;
        } else if (*theRead == '\r' || *theRead == '\n' || *theRead == ' ' || *theRead == '\t') {
            if (!theBlank && theWrite != ioText) {
                *theWrite++ = ' ';
                theBlank = true;
            }
            theRead++;
        } else {
            if (strncmp(theRead, "anyof(", 6) == 0 || strncmp(theRead, "allof(", 6) == 0)
                theInList = true;
            if (*theRead == ')' && theInList) {
                *theWrite++ = ',';
                theInList = false;
            }
            *theWrite++ = *theRead++;
            theBlank = false;
        }
    }
    *theWrite = *theRead;
    free(theCopy);
}

// FUNCTION: EXE 0x62a7e6
// Destroys the trigger (members only)
EEUnitTrigger::~EEUnitTrigger()
{
}

// SYNTHETIC: EXE 0x62a801
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x62a81d
// Adds a trigger name and its id to both trigger lookup tables
void EEUnitTrigger::Register(const char* inName, long inTrigger)
{
    std::pair<UString, long> theID;
    std::pair<unsigned long, UString> theName;
    theID.first = UString(inName);
    theID.second = inTrigger;
    EEUnitTrigger::sTriggerIDs.insert(theID);
    theName.first = inTrigger;
    theName.second = UString(inName);
    EEUnitTrigger::sTriggerNames.insert(theName);
}

// FUNCTION: EXE 0x62a8e5
// Registers the name of every AI-script trigger
void EEUnitTrigger::RegisterAll()
{
    EEUnitTrigger::Register("AirplaneAvailableToLaunch", kTriggerAirplaneAvailableToLaunch);
    EEUnitTrigger::Register("AirtrafficControllerMoved", kTriggerAirtrafficControllerMoved);
    EEUnitTrigger::Register("AlwaysTrue", kTriggerAlwaysTrue);
    EEUnitTrigger::Register("AirportDestroyed", kTriggerAirportDestroyed);
    EEUnitTrigger::Register("AmIUnderAttack", kTriggerAmIUnderAttack);
    EEUnitTrigger::Register("AmmoExhausted", kTriggerAmmoExhausted);
    EEUnitTrigger::Register("ArrivedAtBuilding", kTriggerArrivedAtBuilding);
    EEUnitTrigger::Register("ArrivedAtLocation", kTriggerArrivedAtLocation);
    EEUnitTrigger::Register("ArrivedAtMoveWaypoint", kTriggerArrivedAtMoveWaypoint);
    EEUnitTrigger::Register("ArrivedAtPatrolPoint", kTriggerArrivedAtPatrolPoint);
    EEUnitTrigger::Register("ArrivedAtResource", kTriggerArrivedAtResource);
    EEUnitTrigger::Register("ArrivedAtSafeLocation", kTriggerArrivedAtSafeLocation);
    EEUnitTrigger::Register("ArrivedAtStorage", kTriggerArrivedAtStorage);
    EEUnitTrigger::Register("ArrivedAtTownCenter", kTriggerArrivedAtTownCenter);
    EEUnitTrigger::Register("ArrivedAtUnexploredArea", kTriggerArrivedAtUnexploredArea);
    EEUnitTrigger::Register("ArrivedAtUnit", kTriggerArrivedAtUnit);
    EEUnitTrigger::Register("AttackerIsLegalConversionTarget", kTriggerAttackerIsLegalConversionTarget);
    EEUnitTrigger::Register("AttackMoveEnabled", kTriggerAttackMoveEnabled);
    EEUnitTrigger::Register("AttackerInWeaponRange", kTriggerAttackerInWeaponRange);
    EEUnitTrigger::Register("AttackerIsNotBuilding", kTriggerAttackerIsNotBuilding);
    EEUnitTrigger::Register("AttackerIsRangedUnit", kTriggerAttackerIsRangedUnit);
    EEUnitTrigger::Register("AttackerIsReachable", kTriggerAttackerIsReachable);
    EEUnitTrigger::Register("AtMaximumVelocity", kTriggerAtMaximumVelocity);
    EEUnitTrigger::Register("AtZeroVelocity", kTriggerAtZeroVelocity);
    EEUnitTrigger::Register("BlockedByWall", kTriggerBlockedByWall);
    EEUnitTrigger::Register("BoardedTransport", kTriggerBoardedTransport);
    EEUnitTrigger::Register("BuildSiteClear", kTriggerBuildSiteClear);
    EEUnitTrigger::Register("BuildSiteInRange", kTriggerBuildSiteInRange);
    EEUnitTrigger::Register("BuildSiteNotInRange", kTriggerBuildSiteNotInRange);
    EEUnitTrigger::Register("BuildSiteValid", kTriggerBuildSiteValid);
    EEUnitTrigger::Register("BuildSiteVisible", kTriggerBuildSiteVisible);
    EEUnitTrigger::Register("BuildingBuilt", kTriggerBuildingBuilt);
    EEUnitTrigger::Register("BuildingDestroyed", kTriggerBuildingDestroyed);
    EEUnitTrigger::Register("BuildingInBuildQueue", kTriggerBuildingInBuildQueue);
    EEUnitTrigger::Register("BuildingInWeaponRange", kTriggerBuildingInWeaponRange);
    EEUnitTrigger::Register("BuildingNotInWeaponRange", kTriggerBuildingNotInWeaponRange);
    EEUnitTrigger::Register("BuildingNotPaidFor", kTriggerBuildingNotPaidFor);
    EEUnitTrigger::Register("BuildingRepaired", kTriggerBuildingRepaired);
    EEUnitTrigger::Register("BuildQueueEmpty", kTriggerBuildQueueEmpty);
    EEUnitTrigger::Register("CalamityTargetStillValid", kTriggerCalamityTargetStillValid);
    EEUnitTrigger::Register("CanDamageAttacker", kTriggerCanDamageAttacker);
    EEUnitTrigger::Register("CanFlee", kTriggerCanFlee);
    EEUnitTrigger::Register("CanIMove", kTriggerCanIMove);
    EEUnitTrigger::Register("CanIMoveFreely", kTriggerCanIMoveFreely);
    EEUnitTrigger::Register("CanIPursuePastInitialContactLOS", kTriggerCanIPursuePastInitialContactLOS);
    EEUnitTrigger::Register("CanITargetEnemies", kTriggerCanITargetEnemies);
    EEUnitTrigger::Register("CannotAffordRepair", kTriggerCannotAffordRepair);
    EEUnitTrigger::Register("CannotPathToTarget", kTriggerCannotPathToTarget);
    EEUnitTrigger::Register("CanISeeEnemy", kTriggerCanISeeEnemy);
    EEUnitTrigger::Register("CanUseThisTileAsWaitingArea", kTriggerCanUseThisTileAsWaitingArea);
    EEUnitTrigger::Register("CargoIsEmpty", kTriggerCargoIsEmpty);
    EEUnitTrigger::Register("CeaseFire", kTriggerCeaseFire);
    EEUnitTrigger::Register("ChooseRandomVolcanoTarget", kTriggerChooseRandomVolcanoTarget);
    EEUnitTrigger::Register("IsClosed", kTriggerIsClosed);
    EEUnitTrigger::Register("CompletedUnitFacing", kTriggerCompletedUnitFacing);
    EEUnitTrigger::Register("ContainerHasLostGoal", kTriggerContainerHasLostGoal);
    EEUnitTrigger::Register("ContainerHasNoMoreWaypoints", kTriggerContainerHasNoMoreWaypoints);
    EEUnitTrigger::Register("ContainerInRange", kTriggerContainerInRange);
    EEUnitTrigger::Register("ContainerIsDead", kTriggerContainerIsDead);
    EEUnitTrigger::Register("ContainerIsFull", kTriggerContainerIsFull);
    EEUnitTrigger::Register("ContainerIsIdle", kTriggerContainerIsIdle);
    EEUnitTrigger::Register("ConvertGoalStillValid", kTriggerConvertGoalStillValid);
    EEUnitTrigger::Register("DamagedBuildingSpotted", kTriggerDamagedBuildingSpotted);
    EEUnitTrigger::Register("DefendedBuildingDestroyed", kTriggerDefendedBuildingDestroyed);
    EEUnitTrigger::Register("DefendedBuildingTakingHeavyDamage", kTriggerDefendedBuildingTakingHeavyDamage);
    EEUnitTrigger::Register("DefendedBuildingTakingLightDamage", kTriggerDefendedBuildingTakingLightDamage);
    EEUnitTrigger::Register("DefendedUnitDestroyed", kTriggerDefendedUnitDestroyed);
    EEUnitTrigger::Register("DefendedUnitTakingHeavyDamage", kTriggerDefendedUnitTakingHeavyDamage);
    EEUnitTrigger::Register("DefendedUnitTakingLightDamage", kTriggerDefendedUnitTakingLightDamage);
    EEUnitTrigger::Register("DyingAnimationFinished", kTriggerDyingAnimationFinished);
    EEUnitTrigger::Register("EnemyBuildingConverted", kTriggerEnemyBuildingConverted);
    EEUnitTrigger::Register("EnemyBuildingDestroyed", kTriggerEnemyBuildingDestroyed);
    EEUnitTrigger::Register("EnemyBuildingSpotted", kTriggerEnemyBuildingSpotted);
    EEUnitTrigger::Register("EnemyInsideInitialContactLOS", kTriggerEnemyInsideInitialContactLOS);
    EEUnitTrigger::Register("EnemyIsBuilding", kTriggerEnemyIsBuilding);
    EEUnitTrigger::Register("EnemyUnitConverted", kTriggerEnemyUnitConverted);
    EEUnitTrigger::Register("EnemyUnitDestroyed", kTriggerEnemyUnitDestroyed);
    EEUnitTrigger::Register("EnemyUnitInLOS", kTriggerEnemyUnitInLOS);
    EEUnitTrigger::Register("EnemyUnitInsideFiringArch", kTriggerEnemyUnitInsideFiringArch);
    EEUnitTrigger::Register("EnemyUnitLeftLOS", kTriggerEnemyUnitLeftLOS);
    EEUnitTrigger::Register("EnemyUnitMoving", kTriggerEnemyUnitMoving);
    EEUnitTrigger::Register("EnemyUnitMoved", kTriggerEnemyUnitMoved);
    EEUnitTrigger::Register("EnemyUnitHasNotMovedOneTile", kTriggerEnemyUnitHasNotMovedOneTile);
    EEUnitTrigger::Register("EnemyUnitNoLongerVisible", kTriggerEnemyUnitNoLongerVisible);
    EEUnitTrigger::Register("EnemyUnitReachable", kTriggerEnemyUnitReachable);
    EEUnitTrigger::Register("EnemyUnitSpotted", kTriggerEnemyUnitSpotted);
    EEUnitTrigger::Register("EnemyUnitUndetectable", kTriggerEnemyUnitUndetectable);
    EEUnitTrigger::Register("EnemyWithinMinimumRange", kTriggerEnemyWithinMinimumRange);
    EEUnitTrigger::Register("ExtraLoadableUnitsBumped", kTriggerExtraLoadableUnitsBumped);
    EEUnitTrigger::Register("FacingEnemyUnit", kTriggerFacingEnemyUnit);
    EEUnitTrigger::Register("FacingHealee", kTriggerFacingHealee);
    EEUnitTrigger::Register("FarmNotFound", kTriggerFarmNotFound);
    EEUnitTrigger::Register("FighterHasGoal", kTriggerFighterHasGoal);
    EEUnitTrigger::Register("FlightTimeAlarm", kTriggerFlightTimeAlarm);
    EEUnitTrigger::Register("FlyOffMapAfterAttack", kTriggerFlyOffMapAfterAttack);
    EEUnitTrigger::Register("GarrisonedAtBuilding", kTriggerGarrisonedAtBuilding);
    EEUnitTrigger::Register("GarrisonCostMet", kTriggerGarrisonCostMet);
    EEUnitTrigger::Register("GarrisonUpgrade", 12007);
    EEUnitTrigger::Register("GoalIsAttackLocation", kTriggerGoalIsAttackLocation);
    EEUnitTrigger::Register("GoalIsBuild", kTriggerGoalIsBuild);
    EEUnitTrigger::Register("GoalIsBuildSite", kTriggerGoalIsBuildSite);
    EEUnitTrigger::Register("GoalIsCalamity", kTriggerGoalIsCalamity);
    EEUnitTrigger::Register("GoalIsConversion", kTriggerGoalIsConversion);
    EEUnitTrigger::Register("GoalIsExplore", kTriggerGoalIsExplore);
    EEUnitTrigger::Register("GoalIsHeal", kTriggerGoalIsHeal);
    EEUnitTrigger::Register("GoalIsLoadContainer", kTriggerGoalIsLoadContainer);
    EEUnitTrigger::Register("GoalIsLoadGarrison", kTriggerGoalIsLoadGarrison);
    EEUnitTrigger::Register("GoalIsLocation", kTriggerGoalIsLocation);
    EEUnitTrigger::Register("GoalIsNotConversion", kTriggerGoalIsNotConversion);
    EEUnitTrigger::Register("GoalIsNotPlayerInitiated", kTriggerGoalIsNotPlayerInitiated);
    EEUnitTrigger::Register("GoalIsNotAttack", kTriggerGoalIsNotAttack);
    EEUnitTrigger::Register("GoalIsNotCalamity", kTriggerGoalIsNotCalamity);
    EEUnitTrigger::Register("GoalIsPatrol", kTriggerGoalIsPatrol);
    EEUnitTrigger::Register("GoalIsPlayerInitiated", kTriggerGoalIsPlayerInitiated);
    EEUnitTrigger::Register("GoalIsRepairSite", kTriggerGoalIsRepairSite);
    EEUnitTrigger::Register("GoalIsResource", kTriggerGoalIsResource);
    EEUnitTrigger::Register("GoalIsStorage", kTriggerGoalIsStorage);
    EEUnitTrigger::Register("GoalIsUnit", kTriggerGoalIsUnit);
    EEUnitTrigger::Register("GoalIsUnloadTransport", kTriggerGoalIsUnloadTransport);
    EEUnitTrigger::Register("GoalIsWaitingArea", kTriggerGoalIsWaitingArea);
    EEUnitTrigger::Register("GoalIsWall", kTriggerGoalIsWall);
    EEUnitTrigger::Register("GuardedBuildingDestroyed", kTriggerGuardedBuildingDestroyed);
    EEUnitTrigger::Register("GuardedBuildingTakingHeavyDamage", kTriggerGuardedBuildingTakingHeavyDamage);
    EEUnitTrigger::Register("GuardedBuildingTakingLightDamage", kTriggerGuardedBuildingTakingLightDamage);
    EEUnitTrigger::Register("GuardedUnitDestroyed", kTriggerGuardedUnitDestroyed);
    EEUnitTrigger::Register("GuardedUnitTakingHeavyDamage", kTriggerGuardedUnitTakingHeavyDamage);
    EEUnitTrigger::Register("GuardedUnitTakingLightDamage", kTriggerGuardedUnitTakingLightDamage);
    EEUnitTrigger::Register("HasEnoughManaForConversion", kTriggerHasEnoughManaForConversion);
    EEUnitTrigger::Register("HasGroupAttack", kTriggerHasGroupAttack);
    EEUnitTrigger::Register("HasGroupFlee", kTriggerHasGroupFlee);
    EEUnitTrigger::Register("HasParentalVengance", kTriggerHasParentalVengance);
    EEUnitTrigger::Register("HasValidInitialContactLocation", kTriggerHasValidInitialContactLocation);
    EEUnitTrigger::Register("HealSearchTimeElapsed", kTriggerHealSearchTimeElapsed);
    EEUnitTrigger::Register("HealeeDestroyed", kTriggerHealeeDestroyed);
    EEUnitTrigger::Register("HealeeInHealingRange", kTriggerHealeeInHealingRange);
    EEUnitTrigger::Register("HealeeNoLongerVisible", kTriggerHealeeNoLongerVisible);
    EEUnitTrigger::Register("HoldingPatternComputed", kTriggerHoldingPatternComputed);
    EEUnitTrigger::Register("ImAFarmer", kTriggerImAFarmer);
    EEUnitTrigger::Register("InitalObstacleDetected", kTriggerInitalObstacleDetected);
    EEUnitTrigger::Register("Invalid", kTriggerInvalid);
    EEUnitTrigger::Register("IsAttackerInRetaliationRange", kTriggerIsAttackerInRetaliationRange);
    EEUnitTrigger::Register("IsAnimal", kTriggerIsAnimal);
    EEUnitTrigger::Register("IsAnimalLeader", kTriggerIsAnimalLeader);
    EEUnitTrigger::Register("IsAdult", kTriggerIsAdult);
    EEUnitTrigger::Register("IsBaby", kTriggerIsBaby);
    EEUnitTrigger::Register("IsNotAnimal", kTriggerIsNotAnimal);
    EEUnitTrigger::Register("IsPartOfHerd", kTriggerIsPartOfHerd);
    EEUnitTrigger::Register("LandingClearanceGranted", kTriggerLandingClearanceGranted);
    EEUnitTrigger::Register("LaunchedAirplane", kTriggerLaunchedAirplane);
    EEUnitTrigger::Register("LocationInsideFiringArch", kTriggerLocationInsideFiringArch);
    EEUnitTrigger::Register("LocationInSpace", kTriggerLocationInSpace);
    EEUnitTrigger::Register("LocationInWeaponRange", kTriggerLocationInWeaponRange);
    EEUnitTrigger::Register("LocationOffMap", kTriggerLocationOffMap);
    EEUnitTrigger::Register("LocationUnderAttack", kTriggerLocationUnderAttack);
    EEUnitTrigger::Register("LocationWayOffMap", kTriggerLocationWayOffMap);
    EEUnitTrigger::Register("Locked", kTriggerLocked);
    EEUnitTrigger::Register("MoreWallSectionsToBuild", kTriggerMoreWallSectionsToBuild);
    EEUnitTrigger::Register("MoreLoadableUnitsToPickUp", kTriggerMoreLoadableUnitsToPickUp);
    EEUnitTrigger::Register("MovePreparationComplete", kTriggerMovePreparationComplete);
    EEUnitTrigger::Register("MoveWaypointCollisionDetected", kTriggerMoveWaypointCollisionDetected);
    EEUnitTrigger::Register("MovedTooFarFromCharge", kTriggerMovedTooFarFromCharge);
    EEUnitTrigger::Register("NewResourceFound", kTriggerNewResourceFound);
    EEUnitTrigger::Register("NewTargetSelected", kTriggerNewTargetSelected);
    EEUnitTrigger::Register("NextPatrolPointRetrieved", kTriggerNextPatrolPointRetrieved);
    EEUnitTrigger::Register("NextWaypointRetrieved", kTriggerNextWaypointRetrieved);
    EEUnitTrigger::Register("NoLaunchRequests", kTriggerNoLaunchRequests);
    EEUnitTrigger::Register("NoLongerUnderAttack", kTriggerNoLongerUnderAttack);
    EEUnitTrigger::Register("NoMoreLoadableUnitsToPickUp", kTriggerNoMoreLoadableUnitsToPickUp);
    EEUnitTrigger::Register("NoMoreWallSectionsToBuild", kTriggerNoMoreWallSectionsToBuild);
    EEUnitTrigger::Register("NoMoreWaypoints", kTriggerNoMoreWaypoints);
    EEUnitTrigger::Register("NoTargetUnit", kTriggerNoTargetUnit);
    EEUnitTrigger::Register("NotLocked", kTriggerNotLocked);
    EEUnitTrigger::Register("NoUnexploredAreasLeft", kTriggerNoUnexploredAreasLeft);
    EEUnitTrigger::Register("ObstacleClearedPath", kTriggerObstacleClearedPath);
    EEUnitTrigger::Register("ObstacleDetected", kTriggerObstacleDetected);
    EEUnitTrigger::Register("OneSecondElapsed", kTriggerOneSecondElapsed);
    EEUnitTrigger::Register("OneAndAHalfSecondsElapsed", kTriggerOneAndAHalfSecondsElapsed);
    EEUnitTrigger::Register("TwoSecondsElapsed", kTriggerTwoSecondsElapsed);
    EEUnitTrigger::Register("ThreeSecondsElapsed", kTriggerThreeSecondsElapsed);
    EEUnitTrigger::Register("OneWaypointRemaining", kTriggerOneWaypointRemaining);
    EEUnitTrigger::Register("OpenPositionAvailable", kTriggerOpenPositionAvailable);
    EEUnitTrigger::Register("IsOpen", kTriggerIsOpen);
    EEUnitTrigger::Register("OwnerIsComputer", kTriggerOwnerIsComputer);
    EEUnitTrigger::Register("OwnerIsHuman", kTriggerOwnerIsHuman);
    EEUnitTrigger::Register("LoadableUnitLoaded", 12007);
    EEUnitTrigger::Register("ProjectileHitGround", kTriggerProjectileHitGround);
    EEUnitTrigger::Register("ProjectileHitUnit", kTriggerProjectileHitUnit);
    EEUnitTrigger::Register("ProjectileHitWater", kTriggerProjectileHitWater);
    EEUnitTrigger::Register("HaveReachedRendezvousPoint", kTriggerHaveReachedRendezvousPoint);
    EEUnitTrigger::Register("HaveNotReachedRendezvousPoint", kTriggerHaveNotReachedRendezvousPoint);
    EEUnitTrigger::Register("ReachedBottomOfSpace", kTriggerReachedBottomOfSpace);
    EEUnitTrigger::Register("ReadyToFireAgain", kTriggerReadyToFireAgain);
    EEUnitTrigger::Register("Reloaded", kTriggerReloaded);
    EEUnitTrigger::Register("RepairSiteInRange", kTriggerRepairSiteInRange);
    EEUnitTrigger::Register("RequestStillValid", kTriggerRequestStillValid);
    EEUnitTrigger::Register("RepairIsSpecialUnit", kTriggerRepairIsSpecialUnit);
    EEUnitTrigger::Register("RepathCompleted", kTriggerRepathCompleted);
    EEUnitTrigger::Register("ResourceAccessible", kTriggerResourceAccessible);
    EEUnitTrigger::Register("ResourceCapacityEmpty", kTriggerResourceCapacityEmpty);
    EEUnitTrigger::Register("ResourceCapacityFilled", kTriggerResourceCapacityFilled);
    EEUnitTrigger::Register("ResourceDepleted", kTriggerResourceDepleted);
    EEUnitTrigger::Register("ResourceInRange", kTriggerResourceInRange);
    EEUnitTrigger::Register("ResourcesRottedAway", kTriggerResourcesRottedAway);
    EEUnitTrigger::Register("ResourceTargetIsTree", kTriggerResourceTargetIsTree);
    EEUnitTrigger::Register("ReturnsToInitialContactLocation", kTriggerReturnsToInitialContactLocation);
    EEUnitTrigger::Register("SafeLocationFound", kTriggerSafeLocationFound);
    EEUnitTrigger::Register("SearchingForAreaToExplore", kTriggerSearchingForAreaToExplore);
    EEUnitTrigger::Register("SecondaryTargetFound", kTriggerSecondaryTargetFound);
    EEUnitTrigger::Register("SecondaryTargetNotFound", kTriggerSecondaryTargetNotFound);
    EEUnitTrigger::Register("ShouldIRun", kTriggerShouldIRun);
    EEUnitTrigger::Register("ShouldIRetaliate", kTriggerShouldIRetaliate);
    EEUnitTrigger::Register("ShouldMoveToNextAutoWaypoint", kTriggerShouldMoveToNextAutoWaypoint);
    EEUnitTrigger::Register("SmartTaskNotAssigned", kTriggerSmartTaskNotAssigned);
    EEUnitTrigger::Register("SpecialUnitInOpeningRange", kTriggerSpecialUnitInOpeningRange);
    EEUnitTrigger::Register("SpecialUnitNotInOpeningRange", kTriggerSpecialUnitNotInOpeningRange);
    EEUnitTrigger::Register("StorageDestroyed", kTriggerStorageDestroyed);
    EEUnitTrigger::Register("StorageFound", kTriggerStorageFound);
    EEUnitTrigger::Register("StorageInRange", kTriggerStorageInRange);
    EEUnitTrigger::Register("TakeoffClearanceGranted", kTriggerTakeoffClearanceGranted);
    EEUnitTrigger::Register("TakingHeavyDamage", kTriggerTakingHeavyDamage);
    EEUnitTrigger::Register("TakingLightDamage", kTriggerTakingLightDamage);
    EEUnitTrigger::Register("TargetNeedsHealing", kTriggerTargetNeedsHealing);
    EEUnitTrigger::Register("TargetIsNotDefensiveBuilding", kTriggerTargetIsNotDefensiveBuilding);
    EEUnitTrigger::Register("TargetIsSpecialEnemy", kTriggerTargetIsSpecialEnemy);
    EEUnitTrigger::Register("TurnComplete", kTriggerTurnComplete);
    EEUnitTrigger::Register("TurnParametersCalculated", kTriggerTurnParametersCalculated);
    EEUnitTrigger::Register("UnexploredAreaFound", kTriggerUnexploredAreaFound);
    EEUnitTrigger::Register("UnitDestroyed", kTriggerUnitDestroyed);
    EEUnitTrigger::Register("UnitFacingNotRequired", kTriggerUnitFacingNotRequired);
    EEUnitTrigger::Register("UnitHasGoal", kTriggerUnitHasGoal);
    EEUnitTrigger::Register("UnitHitSpace", kTriggerUnitHitSpace);
    EEUnitTrigger::Register("UnitIdleOrAttackingDefenselessBuilding", kTriggerUnitIdleOrAttackingDefenselessBuilding);
    EEUnitTrigger::Register("UnitInOpeningRange", kTriggerUnitInOpeningRange);
    EEUnitTrigger::Register("UnitInWeaponRange", kTriggerUnitInWeaponRange);
    EEUnitTrigger::Register("UnitNotInOpeningRange", kTriggerUnitNotInOpeningRange);
    EEUnitTrigger::Register("UnitNotInWeaponRange", kTriggerUnitNotInWeaponRange);
    EEUnitTrigger::Register("UnitNotOnMap", kTriggerUnitNotOnMap);
    EEUnitTrigger::Register("UnitOnTileBorderingTarget", kTriggerUnitOnTileBorderingTarget);
    EEUnitTrigger::Register("UnitRepaired", kTriggerUnitRepaired);
    EEUnitTrigger::Register("UnitRottedAway", kTriggerUnitRottedAway);
    EEUnitTrigger::Register("UnitTargetNoLongerValid", kTriggerUnitTargetNoLongerValid);
    EEUnitTrigger::Register("WaitingAreaInRange", kTriggerWaitingAreaInRange);
    EEUnitTrigger::Register("WallSectionBuilt", kTriggerWallSectionBuilt);
    EEUnitTrigger::Register("WallSectionInRange", kTriggerWallSectionInRange);
    EEUnitTrigger::Register("WallSectionNotInRange", kTriggerWallSectionNotInRange);
    EEUnitTrigger::Register("WaypointInAir", kTriggerWaypointInAir);
    EEUnitTrigger::Register("WaypointIsHigherElevation", kTriggerWaypointIsHigherElevation);
    EEUnitTrigger::Register("WaypointIsLowerElevation", kTriggerWaypointIsLowerElevation);
    EEUnitTrigger::Register("WaypointIsSameElevation", kTriggerWaypointIsSameElevation);
    EEUnitTrigger::Register("WaypointOnGround", kTriggerWaypointOnGround);
}

// FUNCTION: EXE 0x62b7e8
// Returns the trigger id for a script name (0 if unknown), registering the names on first use
long EEUnitTrigger::GetTriggerID(const char* inName)
{
    UString theName(inName);
    long theResult = 0;
    if (EEUnitTrigger::sTriggerIDs.size() == 0)
        EEUnitTrigger::RegisterAll();
    std::map<UString, long>::iterator it = EEUnitTrigger::sTriggerIDs.find(theName);
    if (it != EEUnitTrigger::sTriggerIDs.end())
        theResult = it->second;
    return theResult;
}

// FUNCTION: EXE 0x62b837
// Returns the name of a trigger id (an empty name for an unknown id, e.g. a misspelt name in a unit AI script)
UString& EEUnitTrigger::GetTriggerName(unsigned long inTrigger)
{
    static UString sUnknown;
    std::map<unsigned long, UString>::iterator it = EEUnitTrigger::sTriggerNames.find(inTrigger);
    if (it == EEUnitTrigger::sTriggerNames.end())
        return sUnknown;
    return it->second;
}

// FUNCTION: EXE 0x62b854
// Resolves the trigger's true and false action ids in the script, adding empty actions for missing ones
void EEUnitTrigger::Link(std::map<long, EEUnitAction*>*& ioActions)
{
    if (this->mTrueActionID != 0) {
        std::map<long, EEUnitAction*>::iterator it = ioActions->find(this->mTrueActionID);
        if (it != ioActions->end())
            this->mTrueAction = it->second;
        else {
            this->mTrueAction = new EEUnitAction(this->mTrueActionID);
            (*ioActions)[this->mTrueActionID] = this->mTrueAction;
            EEUnitAction::GetActionName(this->mTrueActionID);
        }
    }
    if (this->mFalseActionID == 0)
        return;
    std::map<long, EEUnitAction*>::iterator it = ioActions->find(this->mFalseActionID);
    if (it != ioActions->end())
        this->mFalseAction = it->second;
    else {
        this->mFalseAction = new EEUnitAction(this->mFalseActionID);
        (*ioActions)[this->mFalseActionID] = this->mFalseAction;
        EEUnitAction::GetActionName(this->mFalseActionID);
    }
}

// FUNCTION: EXE 0x62b93f
// Creates an action of the given id with no triggers, named from the action table
EEUnitAction::EEUnitAction(long inAction)
{
    this->mType = inAction;
    this->mName = EEUnitAction::GetActionName(inAction);
}

// SYNTHETIC: EXE 0x62b984
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x62b9a0
// Tests the trigger's events (all of or any of them) and picks its true or false action; returns 0 for no action,
// else 1, or 2 if a goal answered an event
long EEUnitTrigger::Evaluate(EEComplexUnit* inUnit, EEUnitAction*& outAction)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    unsigned long theCount = this->mTriggers.size();
    long theResult = 0;
    long theStatus = 0;
    unsigned long i;
    if (this->mAllOf) {
        for (i = 0; i < theCount; i++) {
            long theTrigger = this->mTriggers[i];
            long theEvent = theClass->EvaluateCondition(inUnit, theTrigger);
            if (theEvent == 0)
                break;
            if (theEvent == 2)
                theStatus = 2;
        }
        if (i == theCount) {
            if (this->mTrueActionID != 0) {
                outAction = this->mTrueAction;
                theResult = theStatus == 0 ? 1 : theStatus;


            }
        } else if (this->mFalseActionID != 0) {
            outAction = this->mFalseAction;
            theResult = theStatus == 0 ? 1 : theStatus;


        }
    } else if (this->mAnyOf) {
        for (i = 0; i < theCount; i++) {
            long theTrigger = this->mTriggers[i];
            long theEvent = theClass->EvaluateCondition(inUnit, theTrigger);
            if (theEvent == 0)
                continue;
            if (theEvent == 2)
                theStatus = 2;
            break;
        }
        if (i < theCount) {
            if (this->mTrueActionID != 0) {
                outAction = this->mTrueAction;
                theResult = theStatus == 0 ? 1 : theStatus;


            }
        } else if (this->mFalseActionID != 0) {
            outAction = this->mFalseAction;
            theResult = theStatus == 0 ? 1 : theStatus;


        }
    }
    return theResult;
}

// FUNCTION: EXE 0x62ba95
// Destroys the action and the triggers it owns
EEUnitAction::~EEUnitAction()
{
    for (unsigned long i = 0; i < this->mTriggers.size(); i++)
        delete this->mTriggers[i];
    this->mTriggers.clear();
}

// FUNCTION: EXE 0x62bae6
// Adds an action name and its id to both action lookup tables
void EEUnitAction::Register(const char* inName, long inAction)
{
    std::pair<UString, long> theID;
    std::pair<unsigned long, UString> theName;
    theID.first = UString(inName);
    theID.second = inAction;
    EEUnitAction::sActionIDs.insert(theID);
    theName.first = inAction;
    theName.second = UString(inName);
    EEUnitAction::sActionNames.insert(theName);
}

// FUNCTION: EXE 0x62bbae
// Adds a trigger to the action; a single-event trigger replaces the action's trigger on that event (or, with
// no actions, removes it)
void EEUnitAction::AddTrigger(EEUnitTrigger* inTrigger)
{
    bool theReplaced = false;
    bool theRemoved = false;
    if (inTrigger->mAnyOf && inTrigger->GetTriggerCount() == 1) {
        for (long i = 0; i < (long)this->mTriggers.size(); i++) {
            EEUnitTrigger* theTrigger = this->mTriggers[i];
            if (!theTrigger->mAnyOf || theTrigger->GetTriggerCount() != 1
                || theTrigger->mTriggers[0] != inTrigger->mTriggers[0])
                continue;
            if (inTrigger->mTrueActionID == 0 && inTrigger->mFalseActionID == 0) {
                delete theTrigger;
                this->mTriggers.erase(this->mTriggers.begin() + i);
                theReplaced = true;
                theRemoved = true;
                i--;
            } else {
                this->mTriggers[i] = inTrigger;
                delete theTrigger;
                theReplaced = true;
            }
        }
        if (theRemoved)
            delete inTrigger;
    }
    if (!theReplaced)
        this->mTriggers.push_back(inTrigger);
}

// FUNCTION: EXE 0x62bd05
// Registers the name of every AI-script action
void EEUnitAction::RegisterAll()
{
    EEUnitAction::Register("Accelerate", kActionAccelerate);
    EEUnitAction::Register("AddResourceToStorage", kActionAddResourceToStorage);
    EEUnitAction::Register("Advance", kActionAdvance);
    EEUnitAction::Register("AirportOpen", kActionAirportOpen);
    EEUnitAction::Register("AnimalRetaliation", kActionAnimalRetaliation);
    EEUnitAction::Register("AnimalParentalVengance", kActionAnimalParentalVengance);
    EEUnitAction::Register("AttackEnemyBuilding", kActionAttackEnemyBuilding);
    EEUnitAction::Register("AttackEnemyUnit", kActionAttackEnemyUnit);
    EEUnitAction::Register("AttackLocation", kActionAttackLocation);
    EEUnitAction::Register("BoardTransport", kActionBoardTransport);
    EEUnitAction::Register("Brake", kActionBrake);
    EEUnitAction::Register("BuildBuilding", kActionBuildBuilding);
    EEUnitAction::Register("BuildListOfBuildings", kActionBuildListOfBuildings);
    EEUnitAction::Register("BuildWall", kActionBuildWall);
    EEUnitAction::Register("BumpExtraLoadableUnits", kActionBumpExtraLoadableUnits);
    EEUnitAction::Register("CalamityPreCast", kActionCalamityPreCast);
    EEUnitAction::Register("CannotReachContainer", kActionCannotReachContainer);
    EEUnitAction::Register("CastCalamity", kActionCastCalamity);
    EEUnitAction::Register("ChangeTargetForSpecialEnemy", kActionChangeTargetForSpecialEnemy);
    EEUnitAction::Register("ChangeTargetForSpecialRepair", kActionChangeTargetForSpecialRepair);
    EEUnitAction::Register("CheckBuildSiteValidity", kActionCheckBuildSiteValidity);
    EEUnitAction::Register("CheckBuildSiteVisibility", kActionCheckBuildSiteVisibility);
    EEUnitAction::Register("CheckForMoreWallSections", kActionCheckForMoreWallSections);
    EEUnitAction::Register("CheckForOpenPosition", kActionCheckForOpenPosition);
    EEUnitAction::Register("CheckOwner", kActionCheckOwner);
    EEUnitAction::Register("CheckPreviousResource", kActionCheckPreviousResource);
    EEUnitAction::Register("CheckRange", kActionCheckRange);
    EEUnitAction::Register("CheckRepairSiteAccessibility", kActionCheckRepairSiteAccessibility);
    EEUnitAction::Register("CheckResourceAccessibility", kActionCheckResourceAccessibility);
    EEUnitAction::Register("CheckResourceCapacity", kActionCheckResourceCapacity);
    EEUnitAction::Register("CheckSafety", kActionCheckSafety);
    EEUnitAction::Register("CheckStorageAccessibility", kActionCheckStorageAccessibility);
    EEUnitAction::Register("Circle", kActionCircle);
    EEUnitAction::Register("CircleAirtrafficController", kActionCircleAirtrafficController);
    EEUnitAction::Register("Climb", kActionClimb);
    EEUnitAction::Register("Closed", kActionClosed);
    EEUnitAction::Register("Closing", kActionClosing);
    EEUnitAction::Register("ContinueToBrake", kActionContinueToBrake);
    EEUnitAction::Register("ContinueToClimb", kActionContinueToClimb);
    EEUnitAction::Register("ContinueToDescend", kActionContinueToDescend);
    EEUnitAction::Register("ContinueToFly", kActionContinueToFly);
    EEUnitAction::Register("ContinueToTaxi", kActionContinueToTaxi);
    EEUnitAction::Register("ContinueToWaitForLandingClearance", kActionContinueToWaitForLandingClearance);
    EEUnitAction::Register("ConvertEnemyBuilding", kActionConvertEnemyBuilding);
    EEUnitAction::Register("ConvertEnemyUnit", kActionConvertEnemyUnit);
    EEUnitAction::Register("ConvertToLocationGoal", kActionConvertToLocationGoal);
    EEUnitAction::Register("Dead", kActionDead);
    EEUnitAction::Register("Decelerate", kActionDecelerate);
    EEUnitAction::Register("DefendBuilding", kActionDefendBuilding);
    EEUnitAction::Register("DefendLocation", kActionDefendLocation);
    EEUnitAction::Register("DefendUnit", kActionDefendUnit);
    EEUnitAction::Register("Descend", kActionDescend);
    EEUnitAction::Register("Disband", kActionDisband);
    EEUnitAction::Register("DropoffResource", kActionDropoffResource);
    EEUnitAction::Register("Dying", kActionDying);
    EEUnitAction::Register("EmergencyTakeoff", kActionEmergencyTakeoff);
    EEUnitAction::Register("Engage", kActionEngage);
    EEUnitAction::Register("FaceEnemyUnit", kActionFaceEnemyUnit);
    EEUnitAction::Register("FaceLocation", kActionFaceLocation);
    EEUnitAction::Register("FadingOutInSpace", kActionFadingOutInSpace);
    EEUnitAction::Register("FindFarm", kActionFindFarm);
    EEUnitAction::Register("FindNewResource", kActionFindNewResource);
    EEUnitAction::Register("FindNewStorage", kActionFindNewStorage);
    EEUnitAction::Register("FindSafeLocation", kActionFindSafeLocation);
    EEUnitAction::Register("FindStorage", kActionFindStorage);
    EEUnitAction::Register("FindUnexploredArea", kActionFindUnexploredArea);
    EEUnitAction::Register("FleeDanger", kActionFleeDanger);
    EEUnitAction::Register("Fly", kActionFly);
    EEUnitAction::Register("GarrisonAtBuilding", kActionGarrisonAtBuilding);
    EEUnitAction::Register("GarrisonUpgrade", kActionGarrisonUpgrade);
    EEUnitAction::Register("GatherResource", kActionGatherResource);
    EEUnitAction::Register("GetNextAutoWaypoint", kActionGetNextAutoWaypoint);
    EEUnitAction::Register("GetNextPatrolPoint", kActionGetNextPatrolPoint);
    EEUnitAction::Register("GetNextMoveWaypoint", kActionGetNextMoveWaypoint);
    EEUnitAction::Register("GetNextTransportWaypoint", kActionGetNextTransportWaypoint);
    EEUnitAction::Register("GetContainerProgress", kActionGetContainerProgress);
    EEUnitAction::Register("GuardBuilding", kActionGuardBuilding);
    EEUnitAction::Register("GuardUnit", kActionGuardUnit);
    EEUnitAction::Register("HealUnit", kActionHealUnit);
    EEUnitAction::Register("HitTarget", kActionHitTarget);
    EEUnitAction::Register("HoldingPattern", kActionHoldingPattern);
    EEUnitAction::Register("HasResourceGoal", kActionHasResourceGoal);
    EEUnitAction::Register("Idle", kActionIdle);
    EEUnitAction::Register("InitialAttackState", kActionInitialAttackState);
    EEUnitAction::Register("InitialCollision", kActionInitialCollision);
    EEUnitAction::Register("InitialCollisionResolution", kActionInitialCollisionResolution);
    EEUnitAction::Register("InsufficientResources", kActionInsufficientResources);
    EEUnitAction::Register("Invalid", kActionInvalid);
    EEUnitAction::Register("Kill", kActionKill);
    EEUnitAction::Register("KillSilently", kActionKillSilently);
    EEUnitAction::Register("Land", kActionLand);
    EEUnitAction::Register("LaunchNextAirplane", kActionLaunchNextAirplane);
    EEUnitAction::Register("LoadIntoContainer", kActionLoadIntoContainer);
    EEUnitAction::Register("LookForEnemies", kActionLookForEnemies);
    EEUnitAction::Register("LookForHealee", kActionLookForHealee);
    EEUnitAction::Register("MoveToBuildSite", kActionMoveToBuildSite);
    EEUnitAction::Register("MoveToBuilding", kActionMoveToBuilding);
    EEUnitAction::Register("MoveToConversionRange", kActionMoveToConversionRange);
    EEUnitAction::Register("MoveToDefendedBuilding", kActionMoveToDefendedBuilding);
    EEUnitAction::Register("MoveToDefendedLocation", kActionMoveToDefendedLocation);
    EEUnitAction::Register("MoveToDefendedUnit", kActionMoveToDefendedUnit);
    EEUnitAction::Register("MoveToGuardedBuilding", kActionMoveToGuardedBuilding);
    EEUnitAction::Register("MoveToGuardedUnit", kActionMoveToGuardedUnit);
    EEUnitAction::Register("MoveToLocation", kActionMoveToLocation);
    EEUnitAction::Register("MoveToNextBuildSite", kActionMoveToNextBuildSite);
    EEUnitAction::Register("MoveToNextPatrolPoint", kActionMoveToNextPatrolPoint);
    EEUnitAction::Register("MoveToRendezvous", kActionMoveToRendezvous);
    EEUnitAction::Register("MoveToRepairSite", kActionMoveToRepairSite);
    EEUnitAction::Register("MoveToResource", kActionMoveToResource);
    EEUnitAction::Register("MoveToSafeLocation", kActionMoveToSafeLocation);
    EEUnitAction::Register("MoveToStorage", kActionMoveToStorage);
    EEUnitAction::Register("MoveToTownCenter", kActionMoveToTownCenter);
    EEUnitAction::Register("MoveToUnexploredArea", kActionMoveToUnexploredArea);
    EEUnitAction::Register("MoveToUnit", kActionMoveToUnit);
    EEUnitAction::Register("MoveToValidMapLocation", kActionMoveToValidMapLocation);
    EEUnitAction::Register("MoveToWaitingArea", kActionMoveToWaitingArea);
    EEUnitAction::Register("MoveToWallSection", kActionMoveToWallSection);
    EEUnitAction::Register("NotifyWaitingCitizen", kActionNotifyWaitingCitizen);
    EEUnitAction::Register("Open", kActionOpen);
    EEUnitAction::Register("Opening", kActionOpening);
    EEUnitAction::Register("PickedUpLoadableUnits", kActionPickedUpLoadableUnits);
    EEUnitAction::Register("PostCollisionNextWaypoint", kActionPostCollisionNextWaypoint);
    EEUnitAction::Register("PostCalamityFlee", kActionPostCalamityFlee);
    EEUnitAction::Register("PrepareToCastCalamity", kActionPrepareToCastCalamity);
    EEUnitAction::Register("PrepareToMove", kActionPrepareToMove);
    EEUnitAction::Register("PrepareToStop", kActionPrepareToStop);
    EEUnitAction::Register("PrepareToTurn", kActionPrepareToTurn);
    EEUnitAction::Register("QueueNewBuilding", kActionQueueNewBuilding);
    EEUnitAction::Register("ReacquireEnemyUnit", kActionReacquireEnemyUnit);
    EEUnitAction::Register("ReacquireGoal", kActionReacquireGoal);
    EEUnitAction::Register("ReacquireLoadContainer", kActionReacquireLoadContainer);
    EEUnitAction::Register("RecalculateLanding", kActionRecalculateLanding);
    EEUnitAction::Register("RepairBuilding", kActionRepairBuilding);
    EEUnitAction::Register("RepairUnit", kActionRepairUnit);
    EEUnitAction::Register("RepathAroundObstacle", kActionRepathAroundObstacle);
    EEUnitAction::Register("RequestLanding", kActionRequestLanding);
    EEUnitAction::Register("ResourcesDepleting", kActionResourcesDepleting);
    EEUnitAction::Register("ResourcesRotting", kActionResourcesRotting);
    EEUnitAction::Register("RetaliateAgainstAttacker", kActionRetaliateAgainstAttacker);
    EEUnitAction::Register("ReturnToInitialContactLocation", kActionReturnToInitialContactLocation);
    EEUnitAction::Register("RunFromAttacker", kActionRunFromAttacker);
    EEUnitAction::Register("SearchForSecondaryTarget", kActionSearchForSecondaryTarget);
    EEUnitAction::Register("SelectNewTarget", kActionSelectNewTarget);
    EEUnitAction::Register("SendParatrooperPlaneOffMap", kActionSendParatrooperPlaneOffMap);
    EEUnitAction::Register("ShouldIFollowEnemyUnit", kActionShouldIFollowEnemyUnit);
    EEUnitAction::Register("ShouldIReturnToInitialContactLocation", kActionShouldIReturnToInitialContactLocation);
    EEUnitAction::Register("SpecialLocation", kActionSpecialLocation);
    EEUnitAction::Register("SwitchToAttackGround", kActionSwitchToAttackGround);
    EEUnitAction::Register("TaskSmartCitizen", kActionTaskSmartCitizen);
    EEUnitAction::Register("Takeoff", kActionTakeoff);
    EEUnitAction::Register("Taxi", kActionTaxi);
    EEUnitAction::Register("TrackEnemyUnit", kActionTrackEnemyUnit);
    EEUnitAction::Register("Turn", kActionTurn);
    EEUnitAction::Register("TurnToFaceHealee", kActionTurnToFaceHealee);
    EEUnitAction::Register("TurnToUnitFacing", kActionTurnToUnitFacing);
    EEUnitAction::Register("UnderAttack", kActionUnderAttack);
    EEUnitAction::Register("UnitRotting", kActionUnitRotting);
    EEUnitAction::Register("UnloadTransport", kActionUnloadTransport);
    EEUnitAction::Register("WaitForBuildSiteToClear", kActionWaitForBuildSiteToClear);
    EEUnitAction::Register("WaitForGarrisonCost", kActionWaitForGarrisonCost);
    EEUnitAction::Register("WaitForLandingClearance", kActionWaitForLandingClearance);
    EEUnitAction::Register("WaitForObstacleToClear", kActionWaitForObstacleToClear);
    EEUnitAction::Register("WaitForOpenPosition", kActionWaitForOpenPosition);
    EEUnitAction::Register("WaitForPassengers", kActionWaitForPassengers);
    EEUnitAction::Register("WaitForTransport", kActionWaitForTransport);
    EEUnitAction::Register("WaitForReload", kActionWaitForReload);
    EEUnitAction::Register("WaitForTakeoffClearance", kActionWaitForTakeoffClearance);
}

// FUNCTION: EXE 0x62c70b
// Returns the action id for a script name (0 if unknown), registering the names on first use
long EEUnitAction::GetActionID(const char* inName)
{
    UString theName(inName);
    long theResult = 0;
    if (EEUnitAction::sActionIDs.size() == 0)
        EEUnitAction::RegisterAll();
    std::map<UString, long>::iterator it = EEUnitAction::sActionIDs.find(theName);
    if (it != EEUnitAction::sActionIDs.end())
        theResult = it->second;
    return theResult;
}

// FUNCTION: EXE 0x62c75a
// Returns the name of an action id (an empty name for an unknown id, e.g. a misspelt name in a unit AI script)
UString& EEUnitAction::GetActionName(unsigned long inAction)
{
    static UString sUnknown;
    std::map<unsigned long, UString>::iterator it = EEUnitAction::sActionNames.find(inAction);
    if (it == EEUnitAction::sActionNames.end())
        return sUnknown;
    return it->second;
}

// FUNCTION: EXE 0x62c777
// Resolves the next actions of every trigger of the action
void EEUnitAction::Link(std::map<long, EEUnitAction*>*& ioActions)
{
    for (unsigned long i = 0; i < this->mTriggers.size(); i++)
        this->mTriggers[i]->Link(ioActions);
}

// FUNCTION: EXE 0x62c7a4
// Resolves the next actions of every action in a script
void EELinkScript(std::map<long, EEUnitAction*>* inActions)
{
    std::map<long, EEUnitAction*>::iterator it;
    for (it = inActions->begin(); it != inActions->end(); ++it)
        it->second->Link(inActions);
}

static void EELoadScript(const UString& inName, std::map<long, EEUnitAction*>*& ioActions);

// FUNCTION: EXE 0x62c7e0
// Returns the named unit AI script, loading, linking and caching it on first use (with an Idle action if it has none)
std::map<long, EEUnitAction*>* EEUnitAction::GetScript(const UString& inName)
{
    std::map<long, EEUnitAction*>* theActions = NULL;
    EEUnitAction* theAction = NULL;
    std::map<UString, std::map<long, EEUnitAction*>*>::iterator it = EEUnitAction::sScripts.find(inName);
    if (it != EEUnitAction::sScripts.end())
        return it->second;
    theActions = new std::map<long, EEUnitAction*>;
    EELoadScript(inName, theActions);
    EELinkScript(theActions);
    std::map<long, EEUnitAction*>::iterator theFound = theActions->find(5000);
    if (theFound == theActions->end()) {
        theAction = new EEUnitAction(kActionIdle);
        (*theActions)[5000] = theAction;
    }
    EEUnitAction::sScripts[inName] = theActions;
    return theActions;
}

// FUNCTION: EXE 0x62c8bd
// Parses Unit AI Scripts\<name>.tai (and the scripts it includes) into the script's actions and triggers
// Format: `#include(<name>)` lines, then per action `<Action> { anyof(A, B) true(X) false(Y) ... }`
static void EELoadScript(const UString& inName, std::map<long, EEUnitAction*>*& ioActions)
{
    UString theInclude;
    UString thePending(0x100);
    UString theToken(0x100);
    UString theName;
    FSFileSpec* theFile = NULL;
    EEUnitTrigger* theTrigger = NULL;
    EEUnitAction* theAction = NULL;
    char* theText = NULL;
    const char* theDelimiters = " ";
    long theState = 0;
    char* theCursor;
    unsigned long theLength;
    unsigned long theRead;
    unsigned long thePosition;
    long theID;
    long theResult = gDataPath->FindFile("Unit AI Scripts\\" + inName + ".tai", theFile);
    if (theResult == kErrorOK && theFile != NULL && theFile->Open(kFileAccessRead) == kErrorOK) {
        theFile->GetLength(theLength);
        theCursor = new char[theLength + 1];
        theFile->Read(theCursor, theLength, theRead);
        theCursor[theLength] = 0;
        theText = theCursor;
        EEStripScript(theCursor);
        while (true) {
            if (thePending.GetLength() > 0) {
                theToken = thePending;
                thePending = "";
            } else if (!EEGetScriptToken(theDelimiters, theCursor, theToken))
                break;
            switch (theState) {
            case 0:
                if (theToken.Find("#include(", 0) < theToken.GetLength()) {
                    theInclude.Copy(theToken, 10, theToken.GetLength() - 16, 0, false);
                    EELoadScript(theInclude, ioActions);
                } else {
                    theID = EEUnitAction::GetActionID(theToken);
                    std::map<long, EEUnitAction*>::iterator it = ioActions->find(theID);
                    if (it == ioActions->end()) {
                        theAction = new EEUnitAction(theID);
                        (*ioActions)[theID] = theAction;
                    } else
                        theAction = it->second;
                    theState = 1;
                }
                break;
            case 1:
                if (theToken[0] == '{') {
                    theTrigger = new EEUnitTrigger;
                    theState = 2;
                }
                break;
            case 2:
                theTrigger->mAnyOf = theToken.Find("anyof(", 0) < theToken.GetLength();
                theTrigger->mAllOf = theToken.Find("allof(", 0) < theToken.GetLength();
                if (!theTrigger->mAnyOf && !theTrigger->mAllOf) {
                    theTrigger->mAnyOf = true;
                    theTrigger->AddTrigger(EEUnitTrigger::GetTriggerID(theToken));
                    theState = 4;
                } else {
                    theDelimiters = ", ";
                    theCursor += 5 - theToken.GetLength();
                    theState = 3;
                }
                break;
            case 3:
                if (theToken[0] == ')') {
                    theDelimiters = " ";
                    theState = 4;
                } else
                    theTrigger->AddTrigger(EEUnitTrigger::GetTriggerID(theToken));
                break;
            case 4:
                thePosition = theToken.Find("true(", 0);
                if (thePosition < theToken.GetLength()) {
                    theName.Copy(theToken, thePosition + 5, theToken.GetLength() - 6, 0, false);
                    theID = EEUnitAction::GetActionID(theName);
                    theTrigger->mTrueActionID = theID;
                    theTrigger->mTrueWeight = 100.0f;
                } else if ((thePosition = theToken.Find("false(", 0)) < theToken.GetLength()) {
                    theName.Copy(theToken, thePosition + 6, theToken.GetLength() - 7, 0, false);
                    theID = EEUnitAction::GetActionID(theName);
                    theTrigger->mFalseActionID = theID;
                    theTrigger->mFalseWeight = 100.0f;
                } else if ((thePosition = theToken.Find("remove()", 0)) >= theToken.GetLength()) {
                    theAction->AddTrigger(theTrigger);
                    if (theToken[0] == '}')
                        theState = 0;
                    else {
                        theTrigger = new EEUnitTrigger;
                        thePending = theToken;
                        theState = 2;
                    }
                }
                break;
            }
        }
    }
    if (theFile != NULL) {
        theFile->Close();
        delete theFile;
    }
    if (theText != NULL)
        delete[] theText;
}

// FUNCTION: EXE 0x62cd15
// Creates an empty any-of trigger with no events or actions
EEUnitTrigger::EEUnitTrigger()
{
    this->mTrueActionID = 0;
    this->mTrueWeight = 0.0f;
    this->mFalseActionID = 0;
    this->mFalseWeight = 0.0f;
    this->mAnyOf = true;
    this->mAllOf = false;
    this->mTrueAction = NULL;
    this->mFalseAction = NULL;
}

// FUNCTION: EXE 0x62cd64
// Adds an event to the trigger, keeping its name for debugging
void EEUnitTrigger::AddTrigger(long inTrigger)
{
    this->mTriggers.push_back(inTrigger);
    this->mTriggerNames.push_back(EEUnitTrigger::GetTriggerName(inTrigger));
}

// FUNCTION: EXE 0x62cd8b
// Evaluates the action's triggers in order; returns the first non-zero result and its next action
long EEUnitAction::Evaluate(EEComplexUnit* inUnit, EEUnitAction*& outAction)
{
    long theResult = 0;
    for (std::vector<EEUnitTrigger*>::iterator it = this->mTriggers.begin(); it != this->mTriggers.end() && theResult == 0; ++it)
        theResult = (*it)->Evaluate(inUnit, outAction);
    return theResult;
}

// FUNCTION: EXE 0x62cdb6
// Deletes every loaded unit AI script with its actions
void EEUnitAction::DeleteScripts()
{
    std::map<UString, std::map<long, EEUnitAction*>*>::iterator it;
    std::map<long, EEUnitAction*>::iterator theAction;
    for (it = EEUnitAction::sScripts.begin(); it != EEUnitAction::sScripts.end(); ++it) {
        std::map<long, EEUnitAction*>* theActions = (*it).second;
        for (theAction = theActions->begin(); theAction != theActions->end(); ++theAction)
            delete (*theAction).second;
        delete theActions;
    }
    EEUnitAction::sScripts.clear();
}

// TEMPLATE: EXE 0x62ce36
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62ce61
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62ce9c
// ~_Tree of sScripts' map<UString, map<long, EEUnitAction*>*>

// TEMPLATE: EXE 0x62cec7
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62cedb
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62cf1f
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62cf4b
// ~_Tree of map<unsigned long, UString>

// TEMPLATE: EXE 0x62cf76
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d02c
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d09d
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d16d
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d1de
// _Tree<unsigned long, UString>::erase(iterator, iterator)

// TEMPLATE: EXE 0x62d24f
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d3e3
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d415
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d450
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d5e4
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d61a
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d8d1
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d90c
// (template/inline COMDAT emitted by this unit)

// TEMPLATE: EXE 0x62d941
// (template/inline COMDAT emitted by this unit)
