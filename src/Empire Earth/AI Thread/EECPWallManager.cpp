// C:\EEX\Empire Earth\AI Thread\EECPWallManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPWallManager, vtable 0x43c2c8; the class-static key "EECPWallManager-Data"). A computer player's wall builder:
// 0x9c833..0xa1910 (the key chain first; three sort predicates built without /Og, then the members; the vector / sort /
// find COMDATs close the unit; EEHerd.cpp starts at 0xa1910). The wall pather's base ctor 0x9f9d3 is an out-of-line
// COMDAT here (its first user PlanWall).
#include <windows.h>
#include <string.h>
#include <vector>
#include <set>
#include <deque>
#include <map>
#include <math.h>
#include <algorithm>

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
#include "lle/GERectangle.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/LowLevelEngineCore.h"
#include "lle/FSRestorable.h"
#include "lle/U2DRectangle.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEPathManager.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPWallManager.h"

// The area utilities this unit calls (cdecl; outside its range; argument meanings inferred) (provisional names)
// 0x24b794: the tiles on the line inFrom..inTo
void EEGetWallLine(const U2DPoint<long>& inFrom, const U2DPoint<long>& inTo, std::vector<U2DPoint<long> >& outTiles);
// 0x2492c2 (ee/EECPIntelligence.h declares it with vector<EETileCoordinate>): the tiles around (inX, inY) at inRadius
void Fn2492C2(long inX, long inY, long inRadius, std::vector<U2DPoint<long> >& outTiles);
// 0x24c3df: the units on tile (inX, inY)
long EEGetUnitsOnTile(long inX, long inY, std::vector<EEUnit*>& outUnits);
// 0x24c433: the units on inTiles
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits);
// 0x210eee: the units of inUnitType at side inSide of inUnit
bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits);
// 0x211d00: whether every tile of inTiles is free
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);

const UString EECPWallManager::sData("EECPWallManager-Data");
// SYNTHETIC: EXE 0x49c833   (wrapper)
// SYNTHETIC: EXE 0x49c83d   (ctor)
// SYNTHETIC: EXE 0x49c84e   (registrar)
// SYNTHETIC: EXE 0x49c85a   (dtor)

// The wall sizes by mSize (9 entries, 0x4e5db8; Update fills them once, by the map and the number of computer players)
struct EECPWMSize {
    float mStartFraction;
    float mEndFraction;
    long mLength;
};
static EECPWMSize sWallSizes[9];

// Orders enemy players by the distance of their centers from mUnit, the farther first (AddWalls) (provisional name)
struct EECPWMPlayerFarther {
    EECPWMPlayerFarther(EECPWallManager* inManager, EEComplexUnit* inUnit) : mManager(inManager), mUnit(inUnit) {}
    bool operator()(EEPlayer* inFirst, EEPlayer* inSecond) const;

    EECPWallManager* mManager;
    EEComplexUnit* mUnit;
};

// Orders runs by their middle tiles' distance from mUnit; runs about as near (within mTolerance) by their lengths
struct EECPWMRunLess {
    EECPWMRunLess(EEComplexUnit* inUnit, float inTolerance) : mUnit(inUnit), mTolerance(inTolerance) {}
    bool operator()(const RTSMapTileRun& inFirst, const RTSMapTileRun& inSecond) const;

    EEComplexUnit* mUnit;
    float mTolerance;
};

// Finds the walls of mUnit (UnitLost's find_if; the COMDAT 0xa009e takes it by value)
struct EECPWMWallIs {
    EECPWMWallIs(EEComplexUnit* inUnit) : mUnit(inUnit) {}
    bool operator()(const EECPWMWall& inWall) const { return inWall.mUnit == this->mUnit; }

    EEComplexUnit* mUnit;
};

// The three predicates were built without /Og (memory-resident locals, `leave`, the bool through a frame slot) while
// the rest of the unit is normal -O1.
// NEAR: built without /Og (memory-resident locals, leave); the pragma alone does not make them exact (the original calls DistanceSquaredTo/GetCenter out of line), so it is not used
// FUNCTION: EXE 0x49c876
// Orders two players by the squared distance of their centers from the unit, the farther first
bool EECPWMPlayerFarther::operator()(EEPlayer* inFirst, EEPlayer* inSecond) const
{
    EEComplexUnit* theFirst = this->mManager->GetCenter(inFirst);
    EEComplexUnit* theSecond = this->mManager->GetCenter(inSecond);
    float theFirstDistance = this->mUnit->DistanceSquaredTo(theFirst);
    float theSecondDistance = this->mUnit->DistanceSquaredTo(theSecond);
    return theFirstDistance > theSecondDistance ? true : false;
}

// FUNCTION: EXE 0x49c8e6
// Orders two tile runs by their middle tiles' squared distance from the unit, nearer first; runs within the
// tolerance of each other by tile count, shorter first
bool EECPWMRunLess::operator()(const RTSMapTileRun& inFirst, const RTSMapTileRun& inSecond) const
{
    std::vector<U2DPoint<long> > theTiles;
    U2DPoint<long> theFirstMiddle;
    U2DPoint<long> theSecondMiddle;
    EEGetWallLine(inFirst.mStart, inFirst.mEnd, theTiles);
    unsigned long theFirstSize = theTiles.size();
    theFirstMiddle = theTiles[theFirstSize / 2];
    EETileDistanceLess theLess(this->mUnit->GetTile());
    long theFirstDistance = theLess.DistanceSquared(theFirstMiddle);
    EEGetWallLine(inSecond.mStart, inSecond.mEnd, theTiles);
    unsigned long theSecondSize = theTiles.size();
    theSecondMiddle = theTiles[theSecondSize / 2];
    EETileDistanceLess theOtherLess(this->mUnit->GetTile());
    long theSecondDistance = theOtherLess.DistanceSquared(theSecondMiddle);
    long theDifference = theSecondDistance - theFirstDistance;
    bool theResult;
    if ((float)abs(theDifference) < this->mTolerance)
        theResult = theFirstSize < theSecondSize;
    else
        theResult = theFirstDistance < theSecondDistance;
    return theResult;
}

// FUNCTION: EXE 0x49cad8
// Orders two tile runs by their middle tiles' squared distance from the center tile, nearer first
bool EETileDistanceLess::operator()(const RTSMapTileRun& inFirst, const RTSMapTileRun& inSecond) const
{
    std::vector<U2DPoint<long> > theTiles;
    U2DPoint<long> theFirstMiddle;
    U2DPoint<long> theSecondMiddle;
    EEGetWallLine(inFirst.mStart, inFirst.mEnd, theTiles);
    theFirstMiddle = theTiles[theTiles.size() / 2];
    long theFirstDistance = this->DistanceSquared(theFirstMiddle);
    EEGetWallLine(inSecond.mStart, inSecond.mEnd, theTiles);
    theSecondMiddle = theTiles.begin()[theTiles.size() / 2];
    long theSecondDistance = this->DistanceSquared(theSecondMiddle);
    return theFirstDistance < theSecondDistance;
}

// FUNCTION: EXE 0x49cc24
// Creates the wall manager of a civilization manager's player, with no walls, gates or path request
EECPWallManager::EECPWallManager(EECPCivilizationManager* inManager)
{
    this->mManager = inManager;
    this->mPlayer = inManager->mPlayer;
    this->mNextTime = 0;
    this->mRequest = NULL;
    this->mFrom = NULL;
    this->mTo = NULL;
    this->mPathing = false;
    this->mGateTile.SetXY(0, 0);
    this->mF70 = 0.0f;
    this->mBuildingPlannedWall = false;
    this->mL7C = 0;
    this->mF74 = 0.0f;
    this->mSize = 0;
}

// FUNCTION: EXE 0x49ccde
// Destroys the wall manager, releasing its planned walls, wall pieces and path request
EECPWallManager::~EECPWallManager()
{
    this->Clear();
}

// FUNCTION: EXE 0x49cd0e
// Returns whether a land building stands within two tiles of the line, ignoring the player's own wall pieces
// (not gates) on the line itself
bool EECPWallManager::IsPathBlocked(std::vector<U2DPoint<long> >& inTiles)
{
    std::vector<U2DPoint<long> > theAll;
    std::vector<U2DPoint<long> > theAround;
    std::vector<EEUnit*> theUnits;
    bool theResult = false;
    for (std::vector<U2DPoint<long> >::iterator it = inTiles.begin(); it != inTiles.end(); it++) {
        theAll.push_back(*it);
        for (unsigned long theRadius = 1; theRadius <= 2; theRadius++) {
            Fn2492C2(it->mX, it->mY, theRadius, theAround);
            theAll.insert(theAll.end(), theAround.begin(), theAround.end());
        }
    }
    std::sort(theAll.begin(), theAll.end());
    std::vector<U2DPoint<long> >::iterator theEnd = std::unique(theAll.begin(), theAll.end());
    theAll.erase(theEnd, theAll.end());
    EEGetUnitsOn(theAll, theUnits);
    for (std::vector<EEUnit*>::iterator theUnit = theUnits.begin(); theUnit != theUnits.end() && !theResult; theUnit++) {
        theResult = (*theUnit)->mUnitClass->IsLandBuilding();
        if (!theResult || (*theUnit)->mPlayer != this->mPlayer || !(*theUnit)->mUnitClass->mIsWall ||
            (*theUnit)->mUnitClass->mCategory == kUnitCategoryGate)
            continue;
        U2DPoint<long> theTile((*theUnit)->mTileX, (*theUnit)->mTileY);
        std::vector<U2DPoint<long> >::iterator theFound = std::find(inTiles.begin(), inTiles.end(), theTile);
        if (theFound != inTiles.end())
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x49cf2a
// Replaces the town's wall plans with one per enemy that has a center (the farthest first) and waits 30 s
// before the next wall
void EECPWallManager::AddWalls(EEComplexUnit* inUnit)
{
    std::vector<EEPlayer*> theEnemies;
    std::vector<EECPWMWall>::iterator it = this->mWalls.begin();
    while (it != this->mWalls.end()) {
        if (it->mUnit == inUnit) {
            it->mUnit->Dereference();
            it = this->mWalls.erase(it);
        } else
            it++;
    }
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer != NULL && thePlayer != this->mPlayer && this->mPlayer->mEnemies[thePlayer->mPlayerID] != 0 &&
            this->GetCenter(thePlayer) != NULL)
            theEnemies.push_back(thePlayer);
    }
    if (theEnemies.size() > 0) {
        std::sort(theEnemies.begin(), theEnemies.end(), EECPWMPlayerFarther(this, inUnit));
        for (std::vector<EEPlayer*>::iterator theEnemy = theEnemies.begin(); theEnemy != theEnemies.end(); theEnemy++) {
            EECPWMWall theWall;
            inUnit->Reference();
            theWall.mUnit = inUnit;
            theWall.mEnemy = *theEnemy;
            this->mWalls.push_back(theWall);
        }
    }
    this->mNextTime = gPhysics->mGameTime + 30000;
}

// FUNCTION: EXE 0x49d05f
// Selects up to one builder per 10 tiles and orders a wall from inFrom to inTo; while the order runs, new wall
// foundations count as the planned wall's (inBuildingPlannedWall)
void EECPWallManager::BuildWall(U2DPoint<long>& inFrom, U2DPoint<long>& inTo, long inUnitClass, bool inBuildingPlannedWall)
{
    GE2DPoint theStart((float)inFrom.GetX(), (float)inFrom.mY);
    GE2DPoint theEnd((float)inTo.GetX(), (float)inTo.mY);
    long theTechnology;
    if (gUnitClassTable[inUnitClass] != NULL)
        theTechnology = gUnitClassTable[inUnitClass]->mTechnology;
    else
        theTechnology = 100000;
    float theLength = theStart.DistanceTo(theEnd) / 10.0f;
    unsigned long theCount = UMFloatToLong(theLength);
    if ((float)theCount < theLength)
        theCount++;
    unsigned long theNum = theCount;
    if (theNum <= 1)
        theNum = 1;
    for (unsigned long i = theNum; i > 0; i--) {
        EEComplexUnit* theBuilder = this->mManager->GetUnitManager()->FindBuilder(theStart, inUnitClass);
        if (theBuilder == NULL)
            continue;
        this->mPlayer->TrySelectUnit(theBuilder, false);
        theBuilder->ClearGoalQueue(0);
        theBuilder->StartAction(kActionIdle);
        theBuilder->mUnitClass->ActionChanged(theBuilder);
        if (theBuilder->mIsDetachedFromAI != true)
            theBuilder->mIsDetachedFromAI = true;
    }
    unsigned long j = 0;
    while (true) {
        if (j >= this->mPlayer->mActiveGroup->GetNumUnits())
            break;
        bool& theFlag = this->mPlayer->mActiveGroup->GetUnit(j)->mIsDetachedFromAI;
        if (theFlag != false)
            theFlag = false;
        j++;
    }
    if (!(this->mPlayer->mActiveGroup->GetNumUnits() > 0))
        return;
    this->mBuildingPlannedWall = inBuildingPlannedWall;
    EEMBuildWall* theMessage = new EEMBuildWall(theStart, theEnd, theTechnology, this->mPlayer->mPlayerID);
    theMessage->Execute();
    theMessage->Dereference();
    this->mPlayer->UnselectAllUnits();
    this->mPlayer->AddPendingUnits(false, true);
    gWorld.FlushQueuedPlacements();
    this->mBuildingPlannedWall = false;
}

// FUNCTION: EXE 0x49d1e7
// Returns whether a wall line stays 0.75 drop-off distances from every resource center (unless ignored) and
// crosses the enemy path (unless the path check is skipped)
bool EECPWallManager::IsRunClear(U2DPoint<long>& inFrom, U2DPoint<long>& inTo, bool inIgnoreResourceCenters, bool inSkipPathCheck)
{
    std::vector<U2DPoint<long> > theTiles;
    EETileDistanceLess theCenter(0, 0);
    long theRange = UMFloatToLong(UMSquare(this->mPlayer->mDropOffDistance * 0.75f));
    bool theResult = true;
    EEGetWallLine(inFrom, inTo, theTiles);
    if (!inIgnoreResourceCenters) {
        for (unsigned long i = 0, theCount = theTiles.size(); i < theCount && theResult; i++) {
            for (unsigned long j = 0; theResult && j < this->mPlayer->GetNumResourceCenters(); j++) {
                this->mPlayer->GetResourceCenter(j)->GetCenter(theCenter.mCenter);
                EETileCoordinate theTile;
                theTile.mX = (theTiles.begin() + i)->mX;
                theTile.mY = (theTiles.begin() + i)->mY;
                theResult = theCenter.DistanceSquared(theTile) > theRange;
            }
        }
    }
    if (!inSkipPathCheck && theResult) {
        unsigned long theSize = theTiles.size();
        bool theFound = false;
        for (unsigned long k = 0; k < theSize && !theFound; k++)
            theFound = std::find(this->mPath.begin(), this->mPath.end(), *(theTiles.begin() + k)) != this->mPath.end();
        theResult = theFound;
    }
    return theResult;
}

// FUNCTION: EXE 0x49d371
// Releases every planned wall, pending wall piece and the path request
void EECPWallManager::Clear()
{
    if (this->mWalls.size() > 0) {
        for (std::vector<EECPWMWall>::iterator it = this->mWalls.begin(); it != this->mWalls.end(); it++) {
            it->mUnit->Dereference();
            it->mUnit = NULL;
            it->mEnemy = NULL;
        }
        this->mWalls.clear();
    }
    if (this->mGates.size() > 0) {
        for (unsigned long i = 0, theCount = this->mGates.size(); i < theCount; i++)
            this->mGates[i]->Dereference();
        this->mGates.clear();
    }
    this->ClearRequest();
}

// FUNCTION: EXE 0x49d3f2
// Releases the path request and its two end units and clears the path
void EECPWallManager::ClearRequest()
{
    if (this->mRequest != NULL) {
        this->mRequest->Dereference();
        this->mRequest = NULL;
    }
    if (this->mFrom != NULL) {
        this->mFrom->Dereference();
        this->mFrom = NULL;
    }
    if (this->mTo != NULL) {
        this->mTo->Dereference();
        this->mTo = NULL;
    }
    this->mPathing = false;
    this->mPath.clear();
}

// FUNCTION: EXE 0x49d437
// Drops a finished piece of the planned wall from the pending ones; after the last, orders a gate on the wall
// tile nearest the middle (else remembers the middle); a new town center or capitol gets walls planned
void EECPWallManager::AddGates(EEComplexUnit* inUnit)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<U2DPoint<long> > theSorted;
    GE2DPoint theLocation(0.0f, 0.0f);
    EETileCoordinate theMiddle;
    theMiddle.mX = 0;
    theMiddle.mY = 0;
    long theUnitClass = this->mPlayer->mCivilizationManager->GetGateClass();
    long theTechnology;
    if (gUnitClassTable[theUnitClass] != NULL)
        theTechnology = gUnitClassTable[theUnitClass]->mTechnology;
    else
        theTechnology = 100000;
    long theSide = 0;
    long theIndex;
    if (inUnit->mUnitClass->mIsWall && !inUnit->mUnitClass->mIsTower) {
        std::vector<EEComplexUnit*>::iterator it = std::find(this->mGates.begin(), this->mGates.end(), inUnit);
        if (it != this->mGates.end()) {
            this->mGates.erase(it);
            inUnit->Dereference();
            if (this->mGates.size() == 0) {
                EEGetWallLine(this->mWallStart, this->mWallEnd, theTiles);
                U2DPoint<long> theTile = theTiles[theTiles.size() / 2];
                theMiddle.mX = theTile.mX;
                theMiddle.mY = theTile.mY;
                theSorted = theTiles;
                std::sort(theSorted.begin(), theSorted.end(), EETileDistanceLess(theMiddle));
                if (this->FindGate(theSorted, theSide, theIndex)) {
                    this->mGateTile = theSorted[theIndex];
                    theLocation.SetX((float)theSorted[theIndex].mX);
                    theLocation.SetY((float)theSorted[theIndex].mY);
                    EEMBuildGate* theMessage = new EEMBuildGate(theLocation, theTechnology, theSide, this->mPlayer->GetPlayerID());
                    theMessage->Execute();
                    theMessage->Dereference();
                } else {
                    this->mGateTile.mX = theMiddle.mX;
                    this->mGateTile.mY = theMiddle.mY;
                }
                this->mWallEnd.SetXY(0, 0);
                this->mWallStart.SetXY(0, 0);
            }
        }
    } else if (inUnit->mUnitClass->mUnitType == kUnitTypeTownCenter || inUnit->mUnitClass->mUnitType == kUnitTypeCapitol)
        this->AddWalls(inUnit);
}

// FUNCTION: EXE 0x49d6cc
// Takes the nearest planned wall and requests a wall-planner path from that enemy's center to the town
void EECPWallManager::StartNextWall()
{
    EECPASGFlags theFlags;
    EEUnitClass* theClass = gUnitClassTable[kUnitTypeCitizen];
    EEAIGPGModifierComputerPlayerWallPlanner* theModifier = NULL;
    if (this->mWalls.size() <= 0)
        return;
    EECPWMWall& theWall = this->mWalls.back();
    EEComplexUnit* theUnit = theWall.mUnit;
    EEPlayer* theEnemy = theWall.mEnemy;
    this->mWalls.pop_back();
    if (theEnemy != NULL) {
        EEComplexUnit* theCenter = this->GetCenter(theEnemy);
        if (theCenter != NULL) {
            this->ClearRequest();
            theClass->GetTerrainFlags(NULL, theFlags);
            theModifier = new EEAIGPGModifierComputerPlayerWallPlanner(theEnemy->GetPlayerID(), 0);
            EETileCoordinate theTo = theUnit->GetTile();
            EETileCoordinate theFrom = theCenter->GetTile();
            this->mRequest = gPathManager.RequestPath(theEnemy->GetPlayerID(), theFrom, theTo, &theFlags, 2, -1,
                                                theModifier, 1);
            if (this->mRequest != NULL) {
                this->mPath.clear();
                this->mPathing = true;
                this->mFrom = theUnit;
                theUnit->Reference();
                this->mTo = theCenter;
                theCenter->Reference();
                this->mWallEnd.SetXY(0, 0);
                this->mWallStart.SetXY(0, 0);
                this->mGates.clear();
            }
        }
    }
    theUnit->Dereference();
    if (theModifier != NULL)
        theModifier->Dereference();
}

// FUNCTION: EXE 0x49d815
// Returns the player's capitol, else its first unit (NULL without units)
EEComplexUnit* EECPWallManager::GetCenter(EEPlayer* inPlayer)
{
    EEComplexUnit* theResult = NULL;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL && theResult == NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mUnitClass->mUnitType == kUnitTypeCapitol)
            theResult = theUnit->mComplexUnit;
    }
    if (theResult == NULL && inPlayer->mFirstUnit != NULL)
        theResult = inPlayer->mFirstUnit->mComplexUnit;
    return theResult;
}

// FUNCTION: EXE 0x49d851
// Finds the first listed tile with a live wall piece where a gate fits on some side; returns the side and the
// tile's index
bool EECPWallManager::FindGate(std::vector<U2DPoint<long> >& inTiles, long& outSide, long& outIndex)
{
    std::vector<EEUnit*> theUnits;
    EEMap* theMap = gWorld.mMap;
    bool theFound = false;
    unsigned long theSize = inTiles.size();
    for (unsigned long i = 0; i < theSize && !theFound; i++) {
        if (theMap->GetTile(inTiles[i].GetX(), inTiles[i].GetY()) == NULL)
            continue;
        EEGetUnitsOnTile(inTiles[i].GetX(), inTiles[i].GetY(), theUnits);
        EEUnit* theWall = NULL;
        for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end() && theWall == NULL; it++) {
            if ((*it)->mUnitClass->mIsWall && (*it)->mHitPoints > 0)
                theWall = *it;
        }
        if (theWall == NULL)
            continue;
        long theType = theWall->mUnitClass->mUnitType;
        theUnits.clear();
        long theSide;
        if (EEDoesWallGateFitOnUnit(static_cast<EEComplexUnit*>(theWall), 1, theType, theUnits))
            theSide = 1;
        else if (EEDoesWallGateFitOnUnit(static_cast<EEComplexUnit*>(theWall), 2, theType, theUnits))
            theSide = 2;
        else if (EEDoesWallGateFitOnUnit(static_cast<EEComplexUnit*>(theWall), 3, theType, theUnits))
            theSide = 3;
        else if (EEDoesWallGateFitOnUnit(static_cast<EEComplexUnit*>(theWall), 4, theType, theUnits))
            theSide = 4;
        else
            continue;
        outSide = theSide;
        outIndex = i;
        theFound = true;
    }
    return theFound;
}

// FUNCTION: EXE 0x49d9e9
// Lays out the enemy path and searches its middle stretch for a run across it between obstacles, on level
// ground away from gates; picks an affordable run (false if none); unsuitable clear runs are kept for later
bool EECPWallManager::PlanWall()
{
    std::vector<RTSMapTileRun> theRuns;
    std::vector<EEUnit*> theUnits;
    std::vector<U2DPoint<long> > theFree;
    std::vector<EETileCoordinate> thePath(this->mRequest->mPath);
    std::vector<U2DPoint<long> > theSegment;
    std::vector<U2DPoint<long> > theStretches;
    std::vector<U2DPoint<long> > theLine;
    std::vector<EEUnit*> theLineUnits;
    U2DPoint<long> theFrom;
    U2DPoint<long> theTo;
    U2DPoint<long> theStart;
    U2DPoint<long> theEnd;
    EEAIAStarBounds theBounds;
    EEMap* theMap = gWorld.GetMap();
    EEAIGPGModifierComputerPlayerWallPather* thePather = new EEAIGPGModifierComputerPlayerWallPather;
    EECPBuildManager* theBuildManager = this->mManager->mBuildManager;
    unsigned long theRunStart = 0;
    unsigned long theBest = 0;
    bool theResult = false;
    long theHalf = sWallSizes[this->mSize].mLength / 2;
    std::reverse(thePath.begin(), thePath.end());
    thePath[0] = this->mFrom->GetTile();
    thePath[thePath.size() - 1] = this->mTo->GetTile();
    unsigned long theCount = thePath.size() - 1;
    for (unsigned long i = 0; i < theCount; ) {
        theFrom.SetXY(thePath[i].mX, thePath[i].GetY());
        i++;
        theTo.SetXY(thePath[i].mX, thePath[i].GetY());
        EETileCoordinate theTile;
        theTile.mX = theFrom.mX;
        theTile.mY = theFrom.GetY();
        theBounds.Set(theTile);
        theTile.mX = theTo.mX;
        theTile.mY = theTo.GetY();
        theBounds.Extend(theTile);
        // each step is searched backwards, from the later path tile to the earlier one (the original passes theTo first)
        gPathManager.FindPath(this->mFrom, theMap, theTo, theFrom, theBounds, theSegment, 1, 0x32, thePather, NULL);
        this->mPath.insert(this->mPath.end(), theSegment.begin(), theSegment.end());
        this->mPath.pop_back();
        theSegment.clear();
    }
    unsigned long theFirst = UMFloatToLong(this->mPath.size() * sWallSizes[this->mSize].mStartFraction);
    unsigned long theLast = UMFloatToLong(this->mPath.size() * sWallSizes[this->mSize].mEndFraction);
    while (true) {
        bool theBlocked;
        do {
            if (theFirst >= theLast) {
                if (theRuns.size() > 0)
                    theResult = this->ChooseRun(theRuns, this->mWallStart, this->mWallEnd);
                if (thePather != NULL)
                    thePather->Dereference();
                return theResult;
            }
            theFrom = this->mPath[theFirst];
            theFirst++;
            theTo = this->mPath[theFirst];
            if (theFrom.mX == theTo.mX) {
                theStart.mX = theFrom.mX - theHalf;
                theEnd.mX = theHalf + theFrom.mX;
                theStart.mY = theFrom.GetY();
                theEnd.mY = theFrom.GetY();
            } else if (theFrom.GetY() == theTo.GetY()) {
                theStart.mY = theFrom.GetY() - theHalf;
                theStart.mX = theFrom.mX;
                theEnd.mX = theFrom.mX;
                theEnd.mY = theFrom.GetY() + theHalf;
            } else {
                long theOffset;
                if ((theFrom.mX < theTo.mX && theFrom.GetY() < theTo.GetY()) ||
                    (theTo.mX < theFrom.mX && theTo.GetY() < theFrom.GetY())) {
                    theEnd.mX = theHalf;
                    theOffset = -theHalf;
                } else {
                    theEnd.mX = -theHalf;
                    theOffset = theHalf;
                }
                theStart.mY = theFrom.GetY() + theHalf;
                theStart.mX = theFrom.mX + theOffset;
                theEnd.mX = theFrom.mX + theEnd.mX;
                theEnd.mY = theFrom.GetY() - theHalf;
            }
            EEGetWallLine(theStart, theEnd, thePath);
            theBlocked = this->IsPathBlocked(thePath);
        } while (theBlocked);
        theStretches.clear();
        unsigned long theSize = thePath.size();
        bool theInRun = false;
        bool theSeenBlock = false;
        for (unsigned long j = 0; j < theSize; j++) {
            bool theBlock;
            if (theMap->GetTile(thePath[j].mX, thePath[j].GetY()) == NULL)
                theBlock = true;
            else if (gContinentManager.GetForestAt(thePath[j]) != NULL)
                theBlock = true;
            else {
                unsigned char theKind = gContinentManager.GetContinentAt(thePath[j].mX, thePath[j].GetY())->mCategory;
                theBlock = theKind == kContinentCategoryCliff || theKind == kContinentCategoryWater || theKind == kContinentCategorySpace;
            }
            if (theBlock) {
                if (theInRun) {
                    theInRun = false;
                    theStretches.push_back(U2DPoint<long>(theRunStart, j - 1));
                }
                theSeenBlock = true;
            } else if (theSeenBlock && !theInRun) {
                theInRun = true;
                theRunStart = j;
            }
        }
        unsigned long theStretchCount = theStretches.size();
        bool theFound = false;
        unsigned long theLimit = 99999;
        for (unsigned long k = 0; k < theStretchCount; k++) {
            unsigned long theA = theStretches[k].mX;
            long theB = theStretches[k].GetY();
            unsigned long theLength = theB - theA + 1;
            if (theLength <= 4 || theLength >= theLimit ||
                !this->IsRunClear(thePath[theA], thePath[theB], false, false) ||
                theBuildManager->HasGateNear(thePath[theA].mX, thePath[theA].GetY(), 10.0f) ||
                theBuildManager->HasGateNear(thePath[theB].mX, thePath[theB].GetY(), 10.0f))
                continue;
            for (unsigned long m = 0; m < theB - theA - 4; m++) {
                theFree.clear();
                theFree.insert(theFree.end(), thePath.begin() + theA + m, thePath.begin() + theA + m + 5);
                if (!EEAreTilesLevel(theFree))
                    continue;
                theBest = k;
                theFound = true;
                theLimit = theLength;
                break;
            }
        }
        if (theFound) {
            theRuns.push_back(RTSMapTileRun(thePath[theStretches[theBest].mX], thePath[theStretches[theBest].GetY()]));
            this->mRuns.clear();
        } else {
            for (unsigned long n = 0; n < theStretchCount; n++) {
                U2DPoint<long> theA = thePath[theStretches[n].mX];
                U2DPoint<long> theB = thePath[theStretches[n].GetY()];
                EEGetWallLine(theA, theB, theLine);
                EEGetUnitsOn(theLine, theLineUnits);
                if (!this->IsRunClear(theA, theB, true, false) || theLineUnits.size() != 0 ||
                    std::find(this->mRuns.begin(), this->mRuns.end(), RTSMapTileRun(theA, theB)) != this->mRuns.end())
                    continue;
                bool theNear = false;
                for (std::vector<U2DPoint<long> >::iterator it = theLine.begin(); it != theLine.end() && !theNear; it++) {
                    if (theBuildManager->HasWallNear(it->mX, it->mY, 100.0f) ||
                        theBuildManager->HasGateNear(it->mX, it->mY, 10.0f))
                        theNear = true;
                }
                if (!theNear)
                    this->mRuns.push_back(RTSMapTileRun(theA, theB));
            }
        }
    }
}

// FUNCTION: EXE 0x49e58e
// Returns whether the path request has finished (false without one)
bool EECPWallManager::IsRequestDone()
{
    if (this->mRequest != NULL)
        return this->mRequest->mIsDone;
    return false;
}

// FUNCTION: EXE 0x49e59c
// Reads the planned walls and pending wall pieces (as unit and player IDs), the gate and wall tiles and the
// timers from the saved game; returns the read error
long EECPWallManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EECPWMWall theWall;
    theWall.mUnit = NULL;
    theWall.mEnemy = NULL;
    unsigned long theCount;
    unsigned long theValue;
    unsigned long theOther;
    EEComplexUnit* theGate;
    unsigned long i;
    this->Clear();
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (i = 0; i < theCount && theResult == kErrorOK; i++) {
            theResult = inFile.Read(theValue);
            if (theResult != kErrorOK)
                continue;
            theWall.mUnit = reinterpret_cast<EEComplexUnit*>(theValue);
            theResult = inFile.Read(theOther);
            if (theResult == kErrorOK) {
                theWall.mEnemy = reinterpret_cast<EEPlayer*>(theOther);
                this->mWalls.push_back(theWall);
            }
        }
    }
    theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
        theResult = inFile.Read(theValue);
        if (theResult == kErrorOK) {
            theGate = reinterpret_cast<EEComplexUnit*>(theValue);
            this->mGates.push_back(theGate);
        }
    }
    if (theResult == kErrorOK)
        theResult = this->mGateTile.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mWallStart.Load(inFile);
    if (theResult == kErrorOK)
        theResult = this->mWallEnd.Load(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mF70);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mF74);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mL7C);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mBuildingPlannedWall);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mNextTime);
    return theResult;
}

// FUNCTION: EXE 0x49e6fe
// Returns whether the path's end lies in the town's path subsector or a neighbor, with no link between them
// through a kind 0xe subsector
bool EECPWallManager::IsPathUsable()
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    bool theResult = false;
    if (this->mRequest != NULL && this->mFrom != NULL) {
        EETileCoordinate& theEnd = this->mRequest->mPath.back();
        EEAIPathSubSector* theFirst = gContinentManager.GetPathSubSectorAt(theEnd.mX, theEnd.mY);
        EEAIPathSubSector* theSecond = gContinentManager.GetPathSubSectorAt(this->mFrom->GetTileX(), this->mFrom->mTileY);
        if (theFirst != theSecond && !theFirst->IsNeighbor(theSecond))
            theResult = false;
        else {
            theResult = true;
            theFirst->GetLinks(theSecond->mID, theLinks);
            for (unsigned long i = 0, theCount = theLinks.size(); i < theCount && theResult; i++) {
                if ((*(theLinks.begin() + i)).first->mKind != 0xe && (*(theLinks.begin() + i)).second->mKind != 0xe)
                    theResult = true;
                else
                    theResult = false;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x49e7fe
// Picks the first affordable run, nearest the town first (similar distances: shorter first); returns its ends
bool EECPWallManager::ChooseRun(std::vector<RTSMapTileRun>& inRuns, U2DPoint<long>& outFrom, U2DPoint<long>& outTo)
{
    std::vector<U2DPoint<long> > theTiles;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    long theUnitClass = this->mManager->GetWallClass();
    bool theResult = false;
    if (theTree != NULL) {
        EETechTreeNode* theNode = theTree->Get(gUnitClassTable[theUnitClass] != NULL ?
                                               gUnitClassTable[theUnitClass]->mTechnology : 100000);
        if (theNode != NULL) {
            long theLength = sWallSizes[this->mSize].mLength;
            std::sort(inRuns.begin(), inRuns.end(), EECPWMRunLess(this->mFrom, (float)(theLength * theLength)));
            for (unsigned long i = 0; i < inRuns.size() && !theResult; i++) {
                EEGetWallLine(inRuns[i].mStart, inRuns[i].mEnd, theTiles);
                if (!this->mPlayer->CanAfford(theNode, 0x20, theTiles.size()))
                    continue;
                theResult = true;
                outFrom = inRuns[i].mStart;
                outTo = inRuns[i].mEnd;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x49e99b
// Picks the affordable run nearest one of the player's gates, else nearest the town center; returns its ends
bool EECPWallManager::ChooseRunNearGates(std::vector<RTSMapTileRun>& inRuns, U2DPoint<long>& outFrom, U2DPoint<long>& outTo)
{
    std::vector<RTSMapTileRun> theRuns(inRuns);
    std::vector<EEUnit*> theUnits;
    std::vector<U2DPoint<long> > theTiles;
    EETechTree* theTree = this->mPlayer->mTechnologies;
    bool theResult = false;
    long theUnitClass = this->mManager->GetWallClass();
    EETechTreeNode* theNode = theTree->Get(gUnitClassTable[theUnitClass] != NULL ?
                                           gUnitClassTable[theUnitClass]->mTechnology : 100000);
    EEGetPlayerUnitsOfType(this->mManager->GetGateClass(), this->mPlayer->GetPlayerID(), theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end() && !theResult; it++) {
        std::sort(theRuns.begin(), theRuns.end(), EETileDistanceLess((*it)->GetTile()));
        for (std::vector<RTSMapTileRun>::iterator theRun = theRuns.begin(); theRun != theRuns.end() && !theResult; theRun++) {
            EEGetWallLine(theRun->mStart, theRun->mEnd, theTiles);
            theResult = this->mPlayer->CanAfford(theNode, 0x20, theTiles.size());
            if (theResult) {
                outFrom = theRun->mStart;
                outTo = theRun->mEnd;
            }
        }
    }
    U2DPoint<long> theCenter = this->mManager->mBuildManager->mTownCenterTile;
    if (theResult || theCenter.mX == -10000 || theCenter.mY == -10000)
        return theResult;
    theRuns = inRuns;
    std::sort(theRuns.begin(), theRuns.end(), EETileDistanceLess(theCenter.mX, theCenter.mY));
    for (std::vector<RTSMapTileRun>::iterator theRun = theRuns.begin(); theRun != theRuns.end() && !theResult; theRun++) {
        EEGetWallLine(theRun->mStart, theRun->mEnd, theTiles);
        theResult = this->mPlayer->CanAfford(theNode, 0x20, theTiles.size());
        if (theResult) {
            outFrom = theRun->mStart;
            outTo = theRun->mEnd;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x49ec12
// Replaces swapped units in the pending wall pieces, moving the references
void EECPWallManager::UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps)
{
    for (std::vector<EEPUnitSwap>::iterator it = inSwaps.begin(); it != inSwaps.end(); it++) {
        std::vector<EEComplexUnit*>::iterator theGate = std::find(this->mGates.begin(), this->mGates.end(), it->mOld);
        if (theGate == this->mGates.end())
            continue;
        theGate = this->mGates.erase(theGate);
        this->mGates.insert(theGate, it->mNew);
        it->mOld->Dereference();
        it->mNew->Reference();
    }
}

// FUNCTION: EXE 0x49ecc6
// Releases every planned wall, pending wall piece and the path request
void EECPWallManager::Reset()
{
    this->Clear();
}

// FUNCTION: EXE 0x49eccb
// Turns the loaded unit and player IDs back into references, dropping missing ones, and makes the next-wall
// time absolute
long EECPWallManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    EECPWMWall theWall;
    theWall.mUnit = NULL;
    theWall.mEnemy = NULL;
    std::vector<EECPWMWall> theWalls;
    std::vector<EEComplexUnit*> theGates;
    std::vector<EEComplexUnit*> theUnused;
    unsigned long i;
    EEComplexUnit* theUnit;
    theWalls = this->mWalls;
    this->mWalls.clear();
    for (i = 0; i < theWalls.size(); i++) {
        theWall = theWalls[i];
        if (USavedID(theWall.mUnit) == -1)
            continue;
        theUnit = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(USavedID(theWall.mUnit))));
        if (theUnit == NULL)
            continue;
        EEPlayer* thePlayer = gWorld.mPlayers[USavedID(theWall.mEnemy)];
        EEPlayer* theEnemy = dynamic_cast<EEPlayer*>(thePlayer);
        if (theEnemy == NULL)
            continue;
        theWall.mUnit = theUnit;
        theUnit->Reference();
        theWall.mEnemy = theEnemy;
        this->mWalls.push_back(theWall);
    }
    theGates = this->mGates;
    this->mGates.clear();
    for (i = 0; i < theGates.size(); i++) {
        theUnit = theGates[i];
        if (USavedID(theUnit) == -1)
            continue;
        theUnit = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(USavedID(theUnit))));
        if (theUnit != NULL) {
            theUnit->Reference();
            this->mGates.push_back(theUnit);
        }
    }
    this->mNextTime += gPhysics->mGameTime;
    return 0;
}

// FUNCTION: EXE 0x49ee89
// Zeroes the wall size table so the next Update fills it again
void EECPWallManager::ResetSizes()
{
    for (int i = 0; i < 9; i++) {
        sWallSizes[i].mEndFraction = 0.0f;
        sWallSizes[i].mStartFraction = 0.0f;
        sWallSizes[i].mLength = 0;
    }
}

// FUNCTION: EXE 0x49eea6
// Writes the planned walls, pending wall pieces (unit and player IDs), gate and wall tiles and the time to the
// next wall; an unfinished path request is planned again first; returns the write error
long EECPWallManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (this->mPathing && !this->IsRequestDone()) {
        this->AddWalls(this->mFrom);
        this->ClearRequest();
    }
    unsigned long theCount = this->mWalls.size();
    unsigned long theID;
    unsigned long thePlayerID;
    unsigned long theTime;
    unsigned long i;
    long theResult = outFile.Write(theCount);
    if (theResult == 0) {
        for (i = 0; i < theCount && theResult == 0; i++) {
            EECPWMWall theWall = this->mWalls[i];
            theID = theWall.mUnit != NULL ? theWall.mUnit->GetUnitID() : -1;
            theResult = outFile.Write(theID);
            if (theResult == 0) {
                thePlayerID = theWall.mEnemy->mPlayerID;
                theResult = outFile.Write(thePlayerID);
            }
        }
    }
    theCount = this->mGates.size();
    theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (i = 0; i < theCount && theResult == 0; i++) {
        theID = this->mGates[i] != NULL ? this->mGates[i]->GetUnitID() : -1;
        theResult = outFile.Write(theID);
    }
    if (theResult == 0)
        theResult = this->mGateTile.Save(outFile);
    if (theResult == 0)
        theResult = this->mWallStart.Save(outFile);
    if (theResult == 0)
        theResult = this->mWallEnd.Save(outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mF70);
    if (theResult == 0)
        theResult = outFile.Write(this->mF74);
    if (theResult == 0)
        theResult = outFile.Write(this->mL7C);
    if (theResult == 0)
        theResult = outFile.Write(this->mBuildingPlannedWall);
    if (theResult == 0) {
        theTime = this->mNextTime - gPhysics->mGameTime;
        theResult = outFile.Write(theTime);
    }
    return theResult;
}

// FUNCTION: EXE 0x49f058
// Records a new wall foundation as a pending piece while a planned wall is being ordered
void EECPWallManager::UnitAdded(EEComplexUnit* inUnit)
{
    if (!inUnit->mUnitClass->mIsWall || !inUnit->IsFoundation() || !this->mBuildingPlannedWall)
        return;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    inUnit->Reference();
    this->mGates.push_back(theUnit);
}

// FUNCTION: EXE 0x49f09d
// Queues a lost wall piece off the planned wall for rebuilding (one on it abandons the pending pieces);
// drops the wall plans of any other lost unit
void EECPWallManager::UnitLost(EEComplexUnit* inUnit, bool inSwapped)
{
    std::vector<U2DPoint<long> > theTiles;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (!inSwapped && theUnit != NULL) {
        if (theUnit->mUnitClass->mIsWall && !theUnit->mUnitClass->mIsTower && theUnit->mUnitClass->mCategory != kUnitCategoryGate) {
            if ((this->mWallStart.mX == 0 && this->mWallStart.mY == 0) || (this->mWallEnd.mX == 0 && this->mWallEnd.mY == 0)) {
                this->mManager->mBuildManager->QueueBuildingAt(inUnit->mUnitClass->mUnitType, inUnit->mTileX, inUnit->mTileY);
                return;
            }
            EEGetWallLine(this->mWallStart, this->mWallEnd, theTiles);
            if (std::find(theTiles.begin(), theTiles.end(), U2DPoint<long>(inUnit->mTileX, inUnit->mTileY)) == theTiles.end()) {
                this->mManager->mBuildManager->QueueBuildingAt(inUnit->mUnitClass->mUnitType, inUnit->mTileX, inUnit->mTileY);
                return;
            }
            for (unsigned long i = 0, n = this->mGates.size(); i < n; i++)
                this->mGates[i]->Dereference();
            this->mGates.clear();
        } else {
            std::vector<EECPWMWall>::iterator it;
            while ((it = std::find_if(this->mWalls.begin(), this->mWalls.end(), EECPWMWallIs(theUnit))) != this->mWalls.end()) {
                it->mUnit->Dereference();
                this->mWalls.erase(it);
            }
        }
    }
}

// FUNCTION: EXE 0x49f244
// Fills the wall size table once; from the Copper age on turns a finished path request into a wall (larger
// size on failure), or every 30 s starts the next planned wall once the last gate stands
void EECPWallManager::Update(unsigned long inTime)
{
    std::vector<EEUnit*> theUnits;
    U2DPoint<long> theTile;
    EEMap* theMap = gWorld.mMap;
    long theUnitClass = this->mManager->GetWallClass();
    bool theSmall = theMap != NULL && (theMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap;
    if (sWallSizes[0].mLength == 0) {
        if (!theSmall && gArtificialIntelligence->GetNumComputerPlayers() <= 2) {
            sWallSizes[0].mStartFraction = 0.45f;  sWallSizes[0].mEndFraction = 0.55f;  sWallSizes[0].mLength = 20;
            sWallSizes[1].mStartFraction = 0.45f;  sWallSizes[1].mEndFraction = 0.55f;  sWallSizes[1].mLength = 30;
            sWallSizes[2].mStartFraction = 0.45f;  sWallSizes[2].mEndFraction = 0.55f;  sWallSizes[2].mLength = 40;
            sWallSizes[3].mStartFraction = 0.3f;   sWallSizes[3].mEndFraction = 0.4f;   sWallSizes[3].mLength = 20;
            sWallSizes[4].mStartFraction = 0.3f;   sWallSizes[4].mEndFraction = 0.4f;   sWallSizes[4].mLength = 30;
            sWallSizes[5].mStartFraction = 0.3f;   sWallSizes[5].mEndFraction = 0.4f;   sWallSizes[5].mLength = 40;
            sWallSizes[6].mStartFraction = 0.15f;  sWallSizes[6].mEndFraction = 0.25f;  sWallSizes[6].mLength = 20;
            sWallSizes[7].mStartFraction = 0.15f;  sWallSizes[7].mEndFraction = 0.25f;  sWallSizes[7].mLength = 30;
            sWallSizes[8].mStartFraction = 0.15f;  sWallSizes[8].mEndFraction = 0.25f;  sWallSizes[8].mLength = 40;
        } else {
            sWallSizes[0].mStartFraction = 0.15f;  sWallSizes[0].mEndFraction = 0.25f;  sWallSizes[0].mLength = 20;
            sWallSizes[1].mStartFraction = 0.15f;  sWallSizes[1].mEndFraction = 0.25f;  sWallSizes[1].mLength = 30;
            sWallSizes[2].mStartFraction = 0.15f;  sWallSizes[2].mEndFraction = 0.25f;  sWallSizes[2].mLength = 40;
            sWallSizes[3].mStartFraction = 0.3f;   sWallSizes[3].mEndFraction = 0.4f;   sWallSizes[3].mLength = 20;
            sWallSizes[4].mStartFraction = 0.3f;   sWallSizes[4].mEndFraction = 0.4f;   sWallSizes[4].mLength = 30;
            sWallSizes[5].mStartFraction = 0.3f;   sWallSizes[5].mEndFraction = 0.4f;   sWallSizes[5].mLength = 40;
            sWallSizes[6].mStartFraction = 0.45f;  sWallSizes[6].mEndFraction = 0.55f;  sWallSizes[6].mLength = 20;
            sWallSizes[7].mStartFraction = 0.45f;  sWallSizes[7].mEndFraction = 0.55f;  sWallSizes[7].mLength = 30;
            sWallSizes[8].mStartFraction = 0.45f;  sWallSizes[8].mEndFraction = 0.55f;  sWallSizes[8].mLength = 40;
        }
    }
    if (this->mPlayer->mTechnologies->mCurrentEpoch < kTechEpochCopper)
        return;
    if (this->mPathing) {
        if (!this->IsRequestDone())
            return;
        if (this->IsPathUsable()) {
            unsigned short theID = this->mPlayer->GetObjectRegion(this->mFrom);
            theTile = gContinentManager.GetPathAreaByID(theID)->GetRandomPoint();
            if (this->mManager->GetUnitManager()->FindBuilderForTile(theTile, theUnitClass) != NULL) {
                if (this->PlanWall()) {
                    this->BuildWall(this->mWallStart, this->mWallEnd, theUnitClass, true);
                    this->mSize = 0;
                    this->mRuns.clear();
                    this->AddWalls(this->mFrom);
                } else if (++this->mSize == 9) {
                    this->mSize = 0;
                    if (this->ChooseRunNearGates(this->mRuns, this->mWallStart, this->mWallEnd)) {
                        this->BuildWall(this->mWallStart, this->mWallEnd, theUnitClass, true);
                        this->mRuns.clear();
                        this->AddWalls(this->mFrom);
                    }
                } else {
                    EECPWMWall theWall;
                    this->mFrom->Reference();
                    theWall = EECPWMWall(this->mFrom, this->mTo->mPlayer);
                    this->mWalls.push_back(theWall);
                }
            } else {
                EECPWMWall theWall;
                this->mFrom->Reference();
                theWall = EECPWMWall(this->mFrom, this->mTo->mPlayer);
                this->mWalls.push_back(theWall);
                this->mNextTime = inTime + 5000;
            }
        }
        this->ClearRequest();
        return;
    }
    if (!this->mPlayer->mCanBuild)
        return;
    if (this->mGates.size() != 0 || inTime < this->mNextTime)
        return;
    if (this->mWalls.size() > 0) {
        this->mNextTime = inTime + 30000;
        if (this->mNextTime > 0) {
            if (this->mGateTile.GetX() == 0 && this->mGateTile.GetY() == 0)
                this->StartNextWall();
            else {
                EEGetUnitsOnTile(this->mGateTile.GetX(), this->mGateTile.GetY(), theUnits);
                bool theFound = false;
                for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end() && !theFound; it++) {
                    if (((*it)->GetUnitClass()->mCategory == kUnitCategoryGate || (*it)->GetUnitClass()->mIsWall) && (*it)->mHitPoints > 0)
                        theFound = true;
                    else
                        theFound = false;
                }
                EEAIPathSubSector* theSubSector = gContinentManager.GetPathSubSectorAt(this->mGateTile.GetX(), this->mGateTile.GetY());
                if (theFound && theSubSector->mKind != kAITileGate && theSubSector->mKind != kAITileWall)
                    this->mNextTime = inTime + 10000;
                else
                    this->StartNextWall();
            }
        }
    } else
        this->mNextTime = inTime + 300000;
}

// FUNCTION: EXE 0x49fec3
// Returns whether both runs have the same start and end tiles
// RTSMapTileRun (ee/EEMap.h) declares both members out of line. Their bodies sit among this unit's COMDATs (the
// find 0x9fe9d calls ==; Fn9D9E9 0x9e2bd is the ctor's first caller; RTSMap.cpp calls it too), so they are defined
// here. Placement: the original's are COMDATs between the templates (inline in the header?); ours follow the
// unit's last function
bool RTSMapTileRun::operator==(const RTSMapTileRun& inOther) const
{
    return inOther.mStart.mX == this->mStart.mX && inOther.mStart.mY == this->mStart.mY && inOther.mEnd.mX == this->mEnd.mX &&
        inOther.mEnd.mY == this->mEnd.mY;
}

// FUNCTION: EXE 0x4a016f
// Creates a tile run from inStart to inEnd
RTSMapTileRun::RTSMapTileRun(const U2DPoint<long>& inStart, const U2DPoint<long>& inEnd)
    : mStart(inStart), mEnd(inEnd)
{
}

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x49f6d0
// TEMPLATE: EXE 0x49f717
// TEMPLATE: EXE 0x49f869
// TEMPLATE: EXE 0x49f87c
// TEMPLATE: EXE 0x49f8a8
// TEMPLATE: EXE 0x49f8e3
// TEMPLATE: EXE 0x49f8f6
// TEMPLATE: EXE 0x49f983
// TEMPLATE: EXE 0x49f9d3
// TEMPLATE: EXE 0x49fcc9
// TEMPLATE: EXE 0x49fd8f
// TEMPLATE: EXE 0x49fe75
// TEMPLATE: EXE 0x49fe9d
// TEMPLATE: EXE 0x49feef
// TEMPLATE: EXE 0x49ffb8
// TEMPLATE: EXE 0x4a009e
// TEMPLATE: EXE 0x4a00bc
// TEMPLATE: EXE 0x4a00ed
// TEMPLATE: EXE 0x4a0115
// TEMPLATE: EXE 0x4a014c
// TEMPLATE: EXE 0x4a0191
// TEMPLATE: EXE 0x4a01be
// TEMPLATE: EXE 0x4a029a
// TEMPLATE: EXE 0x4a02ce
// TEMPLATE: EXE 0x4a03db
// TEMPLATE: EXE 0x4a0410
// TEMPLATE: EXE 0x4a043d
// TEMPLATE: EXE 0x4a0564
// TEMPLATE: EXE 0x4a0597
// TEMPLATE: EXE 0x4a06a4
// TEMPLATE: EXE 0x4a06ef
// TEMPLATE: EXE 0x4a0702
// TEMPLATE: EXE 0x4a07a2
// TEMPLATE: EXE 0x4a07df
// TEMPLATE: EXE 0x4a0823
// TEMPLATE: EXE 0x4a0943
// TEMPLATE: EXE 0x4a0976
// TEMPLATE: EXE 0x4a09ba
// TEMPLATE: EXE 0x4a0a07
// TEMPLATE: EXE 0x4a0a43
// TEMPLATE: EXE 0x4a0b15
// TEMPLATE: EXE 0x4a0b1b
// TEMPLATE: EXE 0x4a0b58
// TEMPLATE: EXE 0x4a0b9c
// TEMPLATE: EXE 0x4a0cc2
// TEMPLATE: EXE 0x4a0d16
// TEMPLATE: EXE 0x4a0df1
// TEMPLATE: EXE 0x4a0e35
// TEMPLATE: EXE 0x4a0e82
// TEMPLATE: EXE 0x4a0fa8
// TEMPLATE: EXE 0x4a0ffc
// TEMPLATE: EXE 0x4a1043
// TEMPLATE: EXE 0x4a1070
// TEMPLATE: EXE 0x4a10a3
// TEMPLATE: EXE 0x4a1100
// TEMPLATE: EXE 0x4a112d
// TEMPLATE: EXE 0x4a1152
// TEMPLATE: EXE 0x4a11b8
// TEMPLATE: EXE 0x4a11eb
// TEMPLATE: EXE 0x4a125c
// TEMPLATE: EXE 0x4a134c
// TEMPLATE: EXE 0x4a13c4
// TEMPLATE: EXE 0x4a14f0
// TEMPLATE: EXE 0x4a1536
// TEMPLATE: EXE 0x4a15fa
// TEMPLATE: EXE 0x4a1673
// TEMPLATE: EXE 0x4a16c2
// TEMPLATE: EXE 0x4a16f9
// TEMPLATE: EXE 0x4a174d
// TEMPLATE: EXE 0x4a17cf
// TEMPLATE: EXE 0x4a1827
// TEMPLATE: EXE 0x4a18a9
// TEMPLATE: EXE 0x4a190a
// SYNTHETIC: EXE 0x49ccc2
// (EECPWallManager's scalar deleting destructor)
// Named COMDATs of this unit (compiled here; names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x49f922
// TEMPLATE: EXE 0x49f9eb
// TEMPLATE: EXE 0x4a12b2
// TEMPLATE: EXE 0x4a147f
// COMDATs named by matching symbol names (names in analysis/exe/names.txt)
// TEMPLATE: EXE 0x49fca2
// TEMPLATE: EXE 0x4a0522
