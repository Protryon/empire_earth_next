// C:\EEX\Empire Earth\AI Thread\EECPCombatManager.cpp (RECONSTRUCTION). The combat manager's military class tests
// (ee/EECPCombatManager.h); stage 5 deleted the combat tree the rest of it ran (docs/AI_RESEARCH.md §8.90).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECPCombatManager.h"

// FUNCTION: EXE 0x471d74
EECPCombatManager::EECPCombatManager(EEComputerPlayer* inPlayer) : mPlayer(inPlayer)
{
}

// FUNCTION: EXE 0x471ea6
EECPCombatManager::~EECPCombatManager()
{
}

// ---- the deleted C++ combat tree's saved block (docs/AI_RESEARCH.md §8.90) ----
// Stage 5 made the combat manager's saved state empty, but a file written before the port's own format (0.254) still
// carries the whole block: the personnel and its requests, the attack and defense commanders with their groups, tasks
// and aid packages, the wonder race and the threat counts. A campaign scenario (0.250) or a retail save is such a
// file, so its computer players cannot be read without consuming it. The readers below only consume the fields, in
// the deleted classes' order (git c41244c7d^); nothing is restored, as the port's AI rebuilds its state from the map.
namespace {

// EECPILocation: the tile, the area serial, its kind and the player
static long EELoadLegacyLocation(FSFile& inFile)
{
    long theTileX = 0;
    long theTileY = 0;
    unsigned long theSerial = 0;
    long theType = 0;
    unsigned long thePlayer = 0;
    long theResult = inFile.Read(theTileX);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTileY);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theSerial);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(thePlayer);
    return theResult;
}

// EECPAGTask: the location, the unit and player ids, then the task's fields
static long EELoadLegacyTask(FSFile& inFile)
{
    unsigned long theUnit = 0;
    unsigned long thePlayer = 0;
    float theAggression = 0.0f;
    long theType = 0;
    long theMode = 0;
    bool theIsActive = false;
    bool theB31 = false;
    long theResult = EELoadLegacyLocation(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theUnit);
    if (theResult == kErrorOK)
        theResult = inFile.Read(thePlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theAggression);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theMode);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theIsActive);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theB31);
    return theResult;
}

// EECPAGOrder: the location, the two sub group ids, the unit id and the order's fields
static long EELoadLegacyOrder(FSFile& inFile)
{
    unsigned long theSubGroup = 0;
    unsigned long theOtherSubGroup = 0;
    unsigned long theUnitID = 0;
    long theL28 = 0;
    long theType = 0;
    unsigned long theState = 0;
    bool theIsActive = false;
    long theSplitKind = 0;
    long theResult = EELoadLegacyLocation(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theSubGroup);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theOtherSubGroup);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theUnitID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theL28);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theIsActive);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theSplitKind);
    return theResult;
}

// EECPAttackSubGroupUnit: the location, the unit, target and escort ids, then the unit's fields
static long EELoadLegacySubGroupUnit(FSFile& inFile)
{
    unsigned long theUnit = 0;
    unsigned long theTarget = 0;
    unsigned long theNextRetargetTime = 0;
    unsigned long theImportance = 0;
    bool theIsImportant = false;
    bool theNeedsOrders = false;
    bool theIsOutOfFight = false;
    long theType = 0;
    float theF6C = 0.0f;
    float theAggression = 0.0f;
    float theCourage = 0.0f;
    float theF68 = 0.0f;
    unsigned long theEscort = 0;
    long theState = 0;
    long thePostAttackMode = 0;
    long theResult = EELoadLegacyLocation(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theUnit);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTarget);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextRetargetTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theImportance);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theIsImportant);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNeedsOrders);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theIsOutOfFight);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theF6C);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theAggression);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCourage);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theF68);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theEscort);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(thePostAttackMode);
    return theResult;
}

// EECPAttackSubGroup: the units, the path, the area, the waypoints, the task and the sub group's fields
static long EELoadLegacySubGroup(FSFile& inFile)
{
    long theCount = 0;
    long i;
    long theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        long theKind = 0;
        theResult = inFile.Read(theKind);
        if (theResult == kErrorOK)
            theResult = EELoadLegacySubGroupUnit(inFile);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        long theTileX = 0;
        long theTileY = 0;
        long theSectorKind = 0;
        theResult = inFile.Read(theTileX);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theTileY);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theSectorKind);
    }
    long theLeft = 0;
    long theTop = 0;
    long theRight = 0;
    long theBottom = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theLeft);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTop);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theRight);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theBottom);
    if (theResult == kErrorOK)
        theResult = EELoadLegacyLocation(inFile);   // the next waypoint
    if (theResult == kErrorOK)
        theResult = EELoadLegacyLocation(inFile);   // the rally point
    if (theResult == kErrorOK)
        theResult = EELoadLegacyTask(inFile);
    long theRequest = 0;
    long theScanRange = 0;
    float theAggression = 0.0f;
    unsigned long theID = 0;
    unsigned long theNextScanTime = 0;
    unsigned long theNextUpdateTime = 0;
    unsigned long theU1B0 = 0;
    unsigned long theU1B4 = 0;
    unsigned long theNextFollowTime = 0;
    unsigned long theU1BC = 0;
    unsigned long theNextTaskCheckTime = 0;
    long theNumSplits = 0;
    unsigned long theState = 0;
    long theKind = 0;
    bool theB1E0 = false;
    bool theB1E1 = false;
    bool theB1E2 = false;
    bool theIsOutOfFight = false;
    long theMission = 0;
    long theSplitKind = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theRequest);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theScanRange);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theAggression);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextScanTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextUpdateTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theU1B0);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theU1B4);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextFollowTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theU1BC);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextTaskCheckTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNumSplits);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theKind);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theB1E0);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theB1E1);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theB1E2);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theIsOutOfFight);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theMission);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theSplitKind);
    return theResult;
}

// EECPAttackGroup: the task, the sub groups, the orders, then the id, continent and finished flag
static long EELoadLegacyAttackGroup(FSFile& inFile)
{
    long theCount = 0;
    long i;
    long theResult = EELoadLegacyTask(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        long theKind = 0;
        theResult = inFile.Read(theKind);
        if (theResult == kErrorOK)
            theResult = EELoadLegacySubGroup(inFile);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++)
        theResult = EELoadLegacyOrder(inFile);
    unsigned long theID = 0;
    unsigned short theContinentID = 0;
    bool theIsFinished = false;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theContinentID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theIsFinished);
    return theResult;
}

// EECPPersonnelRequest: the slots, then the fill, id, continent, town, tile and times
static long EELoadLegacyPersonnelRequest(FSFile& inFile)
{
    long theCount = 0;
    long i;
    long theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theUnit = 0;
        unsigned long theClass = 0;
        unsigned long theUnitID = 0;
        bool theB0C = false;
        theResult = inFile.Read(theUnit);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theClass);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theUnitID);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theB0C);
    }
    float theFill = 0.0f;
    long theID = 0;
    unsigned short theContinent = 0;
    long theTownID = 0;
    long theTileX = 0;
    long theTileY = 0;
    unsigned long theStartTime = 0;
    unsigned long theDeadline = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theFill);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theContinent);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTownID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTileX);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTileY);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theStartTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theDeadline);
    return theResult;
}

// EECPPersonnel: the units, the requests and the deferred unit ids
static long EELoadLegacyPersonnel(FSFile& inFile)
{
    unsigned long theCount = 0;
    unsigned long i;
    long theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theUnitID = 0;
        unsigned long theValue = 0;
        theResult = inFile.Read(theUnitID);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theValue);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        long theRequestID = 0;
        theResult = inFile.Read(theRequestID);
        if (theResult == kErrorOK)
            theResult = EELoadLegacyPersonnelRequest(inFile);
    }
    unsigned long theNextID = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theValue = 0;
        theResult = inFile.Read(theValue);
    }
    return theResult;
}

// EECPAttackCommander: the groups and the one timer it saves
static long EELoadLegacyAttackCommander(FSFile& inFile)
{
    unsigned long theCount = 0;
    unsigned long i;
    long theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++)
        theResult = EELoadLegacyAttackGroup(inFile);
    long theL28 = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theL28);
    return theResult;
}

// EECPMilitaryAidPackage: the sub group ids, the location, the target and the package's fields
static long EELoadLegacyAidPackage(FSFile& inFile)
{
    unsigned long theCount = 0;
    unsigned long i;
    long theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theValue = 0;
        theResult = inFile.Read(theValue);
    }
    if (theResult == kErrorOK)
        theResult = EELoadLegacyLocation(inFile);
    unsigned long theTarget = 0;
    float theThreat = 0.0f;
    long theTownID = 0;
    long theState = 0;
    unsigned long theNextUpdateTime = 0;
    unsigned long thePlayerID = 0;
    unsigned long theNextSendTime = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTarget);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theThreat);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theTownID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextUpdateTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(thePlayerID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextSendTime);
    return theResult;
}

// EECPDefenseCommander: the timers, the aid packages and the loose sub group ids
static long EELoadLegacyDefenseCommander(FSFile& inFile, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    unsigned long theNextUpdateTime = 0;
    bool theEnabled = false;
    unsigned long theCount = 0;
    unsigned long i;
    long theResult = inFile.Read(theNextUpdateTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theEnabled);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        bool theValid = false;
        theResult = inFile.Read(theValid);
        if (theResult == kErrorOK && theValid)
            theResult = EELoadLegacyAidPackage(inFile);
    }
    unsigned long theNextAidTime = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextAidTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theID = 0;
        theResult = inFile.Read(theID);
    }
    unsigned long theNextNavalTime = 0;
    unsigned long theNextSpaceTime = 0;
    unsigned long theNextSpecialTime = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextNavalTime);
    if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 0xed))
        theResult = inFile.Read(theNextSpaceTime);
    if (theResult == kErrorOK && (inMajorVersion > 0 || inMinorVersion >= 0xf0))
        theResult = inFile.Read(theNextSpecialTime);
    return theResult;
}

}   // namespace

// Reads and discards the deleted combat tree's saved block (from 0.228 on: the format retail files carry)
static long EELoadLegacyCombatBlock(FSFile& inFile, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theCount = 0;
    long i;
    long theResult = EELoadLegacyPersonnel(inFile);
    if (theResult == kErrorOK)
        theResult = EELoadLegacyAttackCommander(inFile);
    if (theResult == kErrorOK)
        theResult = EELoadLegacyDefenseCommander(inFile, inMajorVersion, inMinorVersion);
    unsigned long theNextGroupID = 0;
    unsigned long theNextSubGroupID = 0;
    bool theWasAttacked = false;
    bool theImprovementsEnabled = false;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextGroupID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextSubGroupID);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theWasAttacked);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theImprovementsEnabled);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++)
        theResult = EELoadLegacyTask(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++)
        theResult = EELoadLegacyTask(inFile);
    bool theIsWonderRace = false;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theIsWonderRace);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theTime = 0;
        theResult = inFile.Read(theTime);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(theCount);
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        long theTileX = 0;
        long theTileY = 0;
        long theValue = 0;
        theResult = inFile.Read(theTileX);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theTileY);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theValue);
    }
    return theResult;
}

long EECPCombatManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    // stage 5 (docs/AI_RESEARCH.md §8.90): the combat tree is gone and its saved block with it. Files older than the
    // port's own format (0.254) still carry it -- the retail game's saves and its campaign scenarios -- so consume it
    // first, or every computer player after the first desynchronises.
    if (inMajorVersion == 0 && inMinorVersion < 0xfe)
        return EELoadLegacyCombatBlock(inFile, inMajorVersion, inMinorVersion);
    return 0;
}

long EECPCombatManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    (void)inMajorVersion;
    (void)inMinorVersion;
    (void)outFile;
    return 0;
}

// FUNCTION: EXE 0x47377e
// Returns true for a live, AI-controlled unit of a military class that can move
bool EECPCombatManager::IsMilitaryUnit(EEComplexUnit* inUnit)
{
    if (!inUnit->mIsDetachedFromAI && this->IsMilitaryClass(inUnit->mUnitClass) && inUnit->mStats && inUnit->mHitPoints > 0 && !inUnit->mPreviousPart
        && inUnit->GetMaxVelocity() > 0.0f)
        return true;
    return false;
}

// FUNCTION: EXE 0x4737c9
// Returns true for a mobile military class (not a citizen, building, projectile or naval mine)
bool EECPCombatManager::IsMilitaryClass(EEUnitClass* inClass)
{
    bool theResult = false;
    if (inClass && inClass->mMaxVelocity > 0.0f && inClass->mCategory != kUnitCategoryLandCitizen && inClass->mCategory != kUnitCategoryNavalCitizen && !inClass->mIsProjectile) {
        switch (inClass->mCategory) {
        case kUnitCategoryAmbient:
        case kUnitCategoryResource:
        case kUnitCategoryBuilding:
        case kUnitCategoryLandCitizen:
        case kUnitCategoryProjectile:
        case kUnitCategoryBomb:
        case kUnitCategoryMissile:
        case kUnitCategoryBuilding19:
        case kUnitCategoryTorpedo:
        case kUnitCategoryNavalCitizen:
        case kUnitCategoryMiningBuilding:
        case kUnitCategoryGate:
        case kUnitCategoryFortress:
        case kUnitCategoryBuildingPriest:
            theResult = false;
            break;
        case kUnitCategoryAirplane:
        case kUnitCategoryNaval:
        case kUnitCategoryLand:
        case kUnitCategoryHelicopter:
        case kUnitCategoryLand8:
        case kUnitCategorySubmergedNaval:
        case kUnitCategoryLandPriest:
        case kUnitCategoryNavalPriest:
        case kUnitCategoryLand15:
        case kUnitCategoryLandTransport:
        case kUnitCategoryHelicopterTransport:
        case kUnitCategoryNavalTransport:
        case kUnitCategoryNavalBuilding:
        case kUnitCategoryLandMilitaryHero:
        case kUnitCategoryHelicopterPriest:
        case kUnitCategorySeaAnimal:
        case kUnitCategoryAirAnimal:
        case kUnitCategorySpaceship:
        case kUnitCategorySpaceBuilding:
        case kUnitCategorySpaceTransport:
        case kUnitCategoryPlanetSpaceship:
            theResult = inClass->mUnitType != kUnitTypeS10NavalMine;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x475122
// Returns true when the unit type's class is in a military category
bool EECPCombatManager::IsMilitaryType(unsigned long inUnitType)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    bool theResult = false;
    if (theClass) {
        switch (theClass->mCategory) {
        case kUnitCategoryAirplane:
        case kUnitCategoryNaval:
        case kUnitCategoryLand:
        case kUnitCategoryHelicopter:
        case kUnitCategoryLand8:
        case kUnitCategorySubmergedNaval:
        case kUnitCategoryLandPriest:
        case kUnitCategoryNavalPriest:
        case kUnitCategoryLand15:
        case kUnitCategoryLandTransport:
        case kUnitCategoryHelicopterTransport:
        case kUnitCategoryNavalTransport:
        case kUnitCategoryNavalBuilding:
        case kUnitCategoryLandMilitaryHero:
        case kUnitCategoryHelicopterPriest:
        case kUnitCategorySeaAnimal:
        case kUnitCategoryAirAnimal:
        case kUnitCategorySpaceship:
        case kUnitCategorySpaceBuilding:
        case kUnitCategorySpaceTransport:
        case kUnitCategoryPlanetSpaceship:
            theResult = true;
            break;
        case kUnitCategoryAmbient:
        case kUnitCategoryResource:
        case kUnitCategoryBuilding:
        case kUnitCategoryLandCitizen:
        case kUnitCategoryProjectile:
        case kUnitCategoryBomb:
        case kUnitCategoryMissile:
        case kUnitCategoryBuilding19:
        case kUnitCategoryTorpedo:
        case kUnitCategoryNavalCitizen:
        case kUnitCategoryMiningBuilding:
        case kUnitCategoryGate:
        case kUnitCategoryFortress:
        case kUnitCategoryBuildingPriest:
            theResult = false;
            break;
        default:
            theResult = false;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x47517d
// Returns true when the unit's type is military
bool EECPCombatManager::IsMilitaryTypeUnit(EEUnit* inUnit)
{
    if (inUnit)
        return EECPCombatManager::IsMilitaryType(inUnit->mUnitClass->mUnitType);
    return false;
}
