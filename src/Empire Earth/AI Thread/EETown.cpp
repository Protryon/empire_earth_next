// C:\EEX\Empire Earth\AI Thread\EETown.cpp (file name inferred: no __FILE__; RTTI class EETown > EEAIArea,
// vtable 0x43c57c). A player's town: 0xabbaf..0xac7b9 (EEPathManager.cpp's COMDATs end at 0xabbaf, EEAEExtraDamageFunctor's
// ctor starts at 0xac7b9).
#include <windows.h>
#include <vector>
#include <set>
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

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "lle/UReferenceCounter.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EETown.h"
#include "LostDebug.h"   // NOT original source: EE_LOST_DEBUG stand-ins (docs/LOST_DEBUG.md)

// NEAR: only the register of the two argument copies differs (ecx vs eax); store order right
// FUNCTION: EXE 0x4abbaf
// Creates a player's town with the given id: an empty town area with no patrol group, not near water
EETown::EETown(EEPlayer* inPlayer, long inTownID)
    : mPlayer(inPlayer), mTownID(inTownID), mNearMapEdge(false)
{
    this->mAreaType = kAIAreaTown;
    this->mContinent = 0;
    this->mPatrolGroup = NULL;
    this->mNearWater = false;
    this->Reset();
}

// SYNTHETIC: EXE 0x4abbf4
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4abc10
// Destroys the town, releasing its patrol group
EETown::~EETown()
{
    this->Reset();
}

// NEAR: the original refuses GetPoint's operator+ out of line and copies Virtual50's result field by field (a U2DPoint<long>
// return type? EEUnit.h is shared); registers otherwise
// FUNCTION: EXE 0x4abc27
// Adds a unit's building to the town when it is the first or the town can take it: adds its tile, sets the patrol
// group's points to the hull and counts it (walls at a tenth of their value); returns whether it was added
bool EETown::AddUnit(EEUnit* inUnit)
{
    std::vector<U2DPoint<long> > thePoints;
    U2DPoint<long> theTile;
    EEUnitClass* theClass = inUnit->mUnitClass;
    EETechTreeNode* theNode = inUnit->GetTechTreeNode();
    unsigned long theValue = theClass->mInfluence;
    long theCount = 1;
    bool theResult = false;
    if (this->mTiles.Size() != 0 || this->CanTake(inUnit)) {
        theTile = inUnit->FindAreaTile(true, 1);
        theResult = true;
        this->AddTile(theTile);
        if (!gLoadingSavedGame) {
            if (this->mPatrolGroup == NULL && this->mHull.GetPointCount() > 1) {
                this->FindPatrolGroup();
                if (this->mPatrolGroup == NULL) {
                    this->mPatrolGroup = new EEPPatrolGroup;
                    this->mPatrolGroup->mIsTownPatrol = true;
                    this->mPlayer->AddPatrolGroup(this->mPatrolGroup);
                }
            }
            if (this->mPatrolGroup != NULL) {
                for (unsigned long i = 0; i < this->mHull.GetPointCount(); i++)
                    thePoints.push_back(*this->mHull.GetPoint(i));
                this->mPatrolGroup->SetPatrolPoints(thePoints, this->mPlayer->mPlayerID);
            }
        }
        if (theClass->mIsWall) {
            this->mNumWalls++;
            theValue = theValue * 0.1f;
            theCount = 0;
        }
        switch (theClass->mUnitType) {
        case kUnitTypeAirport:
            this->mNumAirports++;
            break;
        case kUnitTypeDock:
        case kUnitTypeNavyYard:
            this->mNumDocks++;
            break;
        }
        if (theNode != NULL && theNode->mData->mKind == 4) {
            this->mNumWonders++;
            theCount--;
        }
        this->mNumUnits++;
        if (theCount > 0)
            this->mL80++;
        this->mValue += theValue;
    }
    return theResult;
}

// NEAR: the original calls GetPoint's begin()/operator+ out of line (as 0xac573 does here); ours expands them
// FUNCTION: EXE 0x4abdf3
// Adds a tile (the first sets the continent); rebuilds the hull and the near-water flags unless the tile is inside
// the hull and not a vertex; returns true
bool EETown::AddTile(const U2DPoint<long>& inTile)
{
    std::vector<U2DPoint<long> > thePoints;
    bool theOnHull = false;
    if (this->mContinent == 0)
        this->mContinent = gContinentManager.GetContinentID(inTile.GetX(), inTile.GetY());
    if (this->mHull.BoundedContains(inTile)) {
        for (unsigned long i = 0; i < this->mHull.GetPointCount(); i++) {
            if (*this->mHull.GetPoint(i) == inTile) {
                theOnHull = true;
                break;
            }
        }
        this->mTiles.Insert(inTile);
        if (theOnHull) {
            this->mTiles.BuildPointList(thePoints, 1);
            this->Create(thePoints);
            if (!gLoadingSavedGame)
                this->UpdateNearWater();
        }
    } else {
        this->mTiles.Insert(inTile);
        this->mTiles.BuildPointList(thePoints, 1);
        this->Create(thePoints);
        if (!gLoadingSavedGame)
            this->UpdateNearWater();
    }
    return true;
    EE_LOST_DEBUG(6);   // LOST-DEBUG: none: exact only (tools lostdebug_search)
}

// FUNCTION: EXE 0x4abede
// Adds another town's counters and tiles to this one (the hull is not rebuilt)
void EETown::Merge(EETown* inOther)
{
    std::vector<U2DPoint<long> > thePoints;
    inOther->mTiles.BuildPointList(thePoints, 1);
    this->mNumAirports += inOther->mNumAirports;
    this->mNumUnits += inOther->mNumUnits;
    this->mL80 += inOther->mL80;
    this->mNumDocks += inOther->mNumDocks;
    this->mValue += inOther->mValue;
    this->mNumWalls += inOther->mNumWalls;
    this->mNumWonders += inOther->mNumWonders;
    for (long i = thePoints.size() - 1; i >= 0; i--)
        this->mTiles.Insert(thePoints[i]);
}

// FUNCTION: EXE 0x4abf8f
// Zeroes the town's counters and continent, sets its reach to 256 and releases its patrol group
void EETown::Reset()
{
    this->mNumAirports = 0;
    this->mNumUnits = 0;
    this->mL80 = 0;
    this->mNumDocks = 0;
    this->mValue = 0;
    this->mNumWalls = 0;
    this->mNumWonders = 0;
    this->mContinent = 0;
    this->mReach = 0x100;
    if (this->mPatrolGroup != NULL) {
        this->mPatrolGroup->Dereference();
        this->mPatrolGroup = NULL;
    }
}

// NEAR: the original calls both vectors' _Ptrit::operator+ out of line; ours expands them (inline budget)
// FUNCTION: EXE 0x4abfda
// Recomputes whether the town is within 10 tiles of a connected water continent (and of a space one: the map-edge
// flag); returns the water flag
bool EETown::UpdateNearWater()
{
    std::vector<unsigned short> theIDs;
    U2DPoint<long> theThis;
    U2DPoint<long> theOther;
    this->mNearWater = false;
    this->mNearMapEdge = false;
    if (this->mContinent != 0 && this->mTiles.Size() > 0) {
        gContinentManager.GetConnectedIDs(theIDs, this->mContinent);
        for (long i = theIDs.size() - 1; i >= 0 && !this->mNearWater; i--) {
            EEAIContinent* theContinent = gContinentManager.mContinents[theIDs[i]];
            if (theContinent == NULL)
                continue;
            if (theContinent->GetCategory() == kContinentCategoryWater) {
                if (theContinent->GetClosestPoints(this, theThis, theOther))
                    this->mNearWater = theThis.DistanceToSquared(theOther) <= 100;
            } else if (theContinent->GetCategory() == kContinentCategorySpace) {
                if (theContinent->GetClosestPoints(this, theThis, theOther))
                    this->mNearMapEdge = theThis.DistanceToSquared(theOther) <= 100;
            }
        }
    }
    return this->mNearWater;
}

// NEAR: the original calls GetPoint's begin()/operator+ out of line (index in memory); ours expands them
// FUNCTION: EXE 0x4ac116
// Adopts (references) the patrol group of one of the owner's path-point units on a hull vertex, if any
void EETown::FindPatrolGroup()
{
    for (unsigned long i = 0; i < this->mHull.GetPointCount() && this->mPatrolGroup == NULL; i++) {
        EEMapTile* theTile = gWorld.mMap->GetTile(this->mHull.GetPoint(i)->mX, this->mHull.GetPoint(i)->mY);
        if (theTile == NULL)
            continue;
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && this->mPatrolGroup == NULL;
             theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (theUnit != NULL && theUnit->mUnitClass->mUnitType == kUnitTypePathPoint && theUnit->mPlayer == this->mPlayer &&
                theUnit->mComplexUnit->mPatrolGroup != NULL) {
                this->mPatrolGroup = theUnit->mComplexUnit->mPatrolGroup;
                this->mPatrolGroup->Reference();
            }
        }
    }
}

// NEAR: register order of `left + GetRandom(..)` (the original adds into ecx after the call)
// FUNCTION: EXE 0x4ac203
// Returns a random map tile inside the town's hull (16 tries within its bounds), else the area's random point
U2DPoint<long> EETown::GetRandomPoint()
{
    U2DPoint<long> theResult;
    bool theFound = false;
    EEMap* theMap = gWorld.mMap;
    if (theMap != NULL) {
        for (long i = 15; i >= 0 && !theFound; i--) {
            long theX = gPhysics->GetRandom(0L, this->mHull.GetBounds().mRight - this->mHull.GetBounds().mLeft);
            theResult.mX = this->mHull.GetBounds().mLeft + theX;
            long theY = gPhysics->GetRandom(0L, this->mHull.GetBounds().mBottom - this->mHull.GetBounds().mTop);
            theResult.mY = this->mHull.GetBounds().mTop + theY;
            if (theMap->GetTileAtLocation(theResult))
                theFound = this->mHull.BoundedContains(theResult);
        }
    }
    if (!theFound)
        theResult = EEAIArea::GetRandomPoint();
    return theResult;
}

// NEAR: theResult is kept in memory where the original keeps it in bl (the zero register of the NULL tests)
// FUNCTION: EXE 0x4ac2b6
// Returns whether a unit is a placed, immobile, non-ambient land building on a land continent (walls only when
// a human player owns them)
bool EETown::IsTownUnit(EEUnit* inUnit)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    bool theResult = false;
    if (inUnit->mTile != NULL && theClass->mFamily != kUnitFamilyAmbient && (theClass->IsLandBuilding() || theClass->mCategory == kUnitCategoryBuilding19) &&
        theClass->mMaxVelocity == 0.0f && (!theClass->mIsWall || inUnit->mPlayer->mHumanPlayer != NULL)) {
        const EETileCoordinate& theTile = inUnit->FindAreaTile(true, 1);
        if (theTile.mX < gMapWidth && theTile.mY < gMapHeight) {
            EEAIContinent* theContinent = gContinentManager.GetContinentAt(theTile.mX, theTile.mY);
            if (theContinent != NULL)
                theResult = theContinent->mCategory == kContinentCategoryLand;
        }
    }
    return theResult;
}

// NEAR: Virtual50's result copied field by field in the original (see AddUnit)
// FUNCTION: EXE 0x4ac351
// Removes a unit's building from the town (the mirror of AddUnit), counting its counters down; returns whether its tile was in the town (true if not taken)
bool EETown::RemoveUnit(EEUnit* inUnit)
{
    U2DPoint<long> theTile;
    EEUnitClass* theClass = inUnit->mUnitClass;
    EETechTreeNode* theNode = inUnit->GetTechTreeNode();
    unsigned long theValue = theClass->mInfluence;
    long theCount = 1;
    bool theResult = true;
    if (this->CanTake(inUnit)) {
        theTile = inUnit->FindAreaTile(true, 1);
        theResult = this->RemoveHullTile(theTile);
        if (theClass->mIsWall) {
            this->mNumWalls = this->mNumWalls > 0 ? this->mNumWalls - 1 : 0;
            theCount = 0;
            theValue = theValue * 0.1f;
        }
        switch (theClass->mUnitType) {
        case kUnitTypeAirport:
            this->mNumAirports = this->mNumAirports > 0 ? this->mNumAirports - 1 : 0;
            break;
        case kUnitTypeDock:
        case kUnitTypeNavyYard:
            this->mNumDocks = this->mNumDocks > 0 ? this->mNumDocks - 1 : 0;
            break;
        }
        if (theNode != NULL && theNode->mData->mKind == 4) {
            this->mNumWonders = this->mNumWonders > 0 ? this->mNumWonders - 1 : 0;
            theCount--;
        }
        this->mNumUnits = this->mNumUnits > 0 ? this->mNumUnits - 1 : 0;
        if (theCount > 0)
            this->mL80 = this->mL80 > 0 ? this->mL80 - 1 : 0;
        if (this->mValue > theValue)
            this->mValue -= theValue;
        else
            this->mValue = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x4ac4b6
// Finds or creates the town's patrol group (once the hull has two points) and sets its patrol points to the hull
void EETown::RefreshPatrolGroup()
{
    std::vector<U2DPoint<long> > thePoints;
    if (this->mPatrolGroup == NULL && this->mHull.GetPointCount() > 1) {
        this->FindPatrolGroup();
        if (this->mPatrolGroup == NULL) {
            this->mPatrolGroup = new EEPPatrolGroup;
            this->mPatrolGroup->mIsTownPatrol = true;
            this->mPlayer->AddPatrolGroup(this->mPatrolGroup);
        }
    }
    if (this->mPatrolGroup == NULL)
        return;
    for (long i = 0; i < (long)this->mHull.GetPointCount(); i++)
        thePoints.push_back(*this->mHull.GetPoint(i));
    this->mPatrolGroup->SetPatrolPoints(thePoints, this->mPlayer->mPlayerID);
}

// FUNCTION: EXE 0x4ac573
// Removes a tile from the town; when it was a hull vertex, rebuilds the hull (cleared when no tile is left) and the
// patrol points; returns whether the tile was in the town
bool EETown::RemoveHullTile(const U2DPoint<long>& inTile)
{
    std::vector<U2DPoint<long> > thePatrol;
    std::vector<U2DPoint<long> > thePoints;
    bool theOnHull = false;
    bool theResult = false;
    for (unsigned long i = 0; i < this->mHull.GetPointCount(); i++) {
        if (*this->mHull.GetPoint(i) == inTile) {
            theOnHull = true;
            break;
        }
    }
    if (this->mTiles.Contains(inTile)) {
        theResult = true;
        this->mTiles.Remove(inTile);
    }
    if (theOnHull) {
        this->mTiles.BuildPointList(thePoints, 1);
        if (thePoints.size() > 0) {
            this->mHull.SetPoints(thePoints, false);
            if (!gLoadingSavedGame && !gWorldClearing)
                this->UpdateNearWater();
            this->mHull.GetPointCount();
        } else
            this->mHull.Clear();
        if (!gLoadingSavedGame) {
            if (this->mPatrolGroup == NULL)
                this->FindPatrolGroup();
            if (this->mPatrolGroup != NULL) {
                for (unsigned long j = 0; j < this->mHull.GetPointCount(); j++)
                    thePatrol.push_back(*this->mHull.GetPoint(j));
                this->mPatrolGroup->SetPatrolPoints(thePatrol, this->mPlayer->mPlayerID);
            }
        }
    }
    return theResult;
}

// NEAR: theResult kept in memory where the original keeps it in bl, U2DPoint zero stores sunk below the first test
// FUNCTION: EXE 0x4ac6f7
// Returns whether an own town unit belongs to the town: always when the town is empty, else when it stands on the
// town's continent inside the hull or within the reach (squared distance) of its closest hull point
bool EETown::CanTake(EEUnit* inUnit)
{
    U2DPoint<long> theTile;
    U2DPoint<long> theClosest;
    bool theResult = false;
    if (inUnit->mPlayer == this->mPlayer && EETown::IsTownUnit(inUnit)) {
        theResult = this->mTiles.Size() == 0;
        if (!theResult) {
            theTile = inUnit->FindAreaTile(true, 1);
            unsigned short theID = gContinentManager.GetContinentID(theTile.mX, theTile.mY);
            if (theID == this->mContinent || this->mContinent == 0) {
                theResult = this->mHull.BoundedContains(theTile);
                if (!theResult) {
                    theClosest = this->GetClosestHullPoint(theTile);
                    theResult = theClosest.DistanceToSquared(theTile) <= this->mReach;
                }
            } else
                theResult = false;
        }
    }
    return theResult;
}
