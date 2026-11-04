// C:\EEX\Empire Earth\Units\EEUGExplore.cpp (file name inferred: no __FILE__; the key "EEUGExplore-Data").
// Exploring: 0x2213dd..0x221d98. Unit flags: the exe's module set. Data: the key 0x563568 (guard byte 0x563560);
// vtable 0x44750c. The inline virtuals (+0x80, +0x84, +0x30) follow their first user, the ctor.

#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGExplore.h"
#include "ee/EEAreaUtilities.h"
#include <math.h>
#include "ee/EETileLocation.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include <map>
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEPathManager.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEMapDimensions.h"

const UString EEUGExplore::sData("EEUGExplore-Data");
// SYNTHETIC: EXE 0x6213dd   (wrapper)
// SYNTHETIC: EXE 0x6213e7   (ctor)
// SYNTHETIC: EXE 0x6213f8   (registrar)
// SYNTHETIC: EXE 0x621404   (dtor)

// FUNCTION: EXE 0x621420
// Creates an explore goal with no tile searched yet
EEUGExplore::EEUGExplore(bool inArg)
    : EEUnitGoal(kGoalExplore, inArg)
{
    this->mNoUnexploredAreas = false;
    this->mNoNonVisibleAreas = false;
    this->mSearchRadius = 0;
}

// TEMPLATE: EXE 0x621454
// (inline: GetNoUnexploredAreas, emitted with the ctor)

// TEMPLATE: EXE 0x621458
// (inline: GetNoNonVisibleAreas)

// TEMPLATE: EXE 0x62145c
// (inline: Virtual30)

// SYNTHETIC: EXE 0x621462
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x62147e
// Destroys the goal (members only)
EEUGExplore::~EEUGExplore()
{
}

// FUNCTION: EXE 0x6214ab
// Writes the bad-tile count, current tile, search radius and both no-areas flags to the out-of-synch dump
void EEUGExplore::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGExplore Bad Tile Array Size %d\n", this->mBadTiles.Size());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGExplore Tile Location X: %d\n", this->mTile.GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGExplore Tile Location Y: %d\n", this->mTile.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGExplore Search Radius %d\n", this->mSearchRadius);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGExplore No UnExplored Areas flag %d\n", this->mNoUnexploredAreas == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGExplore No NonVisible Areas flag %d\n", this->mNoNonVisibleAreas == true);
}

// FUNCTION: EXE 0x621513
// Returns the bad-tile count plus the current tile, search radius and both no-areas flags (without the base goal's)
long EEUGExplore::GetOutOfSynchChecksum()
{
    return this->mBadTiles.Size() + this->mTile.GetX() + this->mTile.GetY() + this->mSearchRadius + (this->mNoUnexploredAreas == true)
        + (this->mNoNonVisibleAreas == true);
}

// FUNCTION: EXE 0x621534
// Returns a fresh explore goal (the search state is not copied)
EEUnitGoal* EEUGExplore::Clone()
{
    return new EEUGExplore(this->GetB1C());
}

// FUNCTION: EXE 0x62155e
// Picks the next tile to explore: an unseen, reachable tile on rings round the unit that widen by 5 per call up to
// the path area's size (false while rings remain), else a random continent point; notices foreigners, tells the group
bool EEUGExplore::FindNextTile(EEComplexUnit* inUnit)
{
    EECPASGFlags theFlags;

    std::vector<U2DPoint<long> > theTiles;
    EEMap* theMap = gWorld.GetMap();
    bool theFound = false;
    U2DPoint<long> theCenter;
    EEUnit* theTarget;
    if (!inUnit->mPlayer->IsHuman())
        this->mNoUnexploredAreas = true;
    if (!this->mNoNonVisibleAreas) {
        this->mNoNonVisibleAreas = inUnit->mPlayer->mSeesAll;
    }
    if (!this->mNoNonVisibleAreas && !this->mNoUnexploredAreas) {
        theCenter.mX = inUnit->mTileX;
        theCenter.mY = inUnit->mTileY;
        if ((theCenter.GetX() == -10000 || theCenter.mY == -10000) && inUnit->mContainer != NULL) {
            theCenter.mX = inUnit->mContainer->mTileX;
            theCenter.mY = inUnit->mContainer->mTileY;
        }
        if (theCenter != this->mTile)
            this->mBadTiles.Insert(this->mTile);
        EEPlayer* thePlayer = inUnit->mPlayer;
        unsigned long theMaximum;
        EEAIPathArea* theArea;
        if ((inUnit->mActiveGroup == NULL || inUnit->mActiveGroup->GetNumUnits() < 5) &&
            (theArea = gContinentManager.GetPathAreaAt(inUnit)) != 0) {
            const U2DRectangle<long>& theBounds = theArea->mHull.GetBounds();
            long theA = theBounds.mBottom - theBounds.mTop;
            long theB = theBounds.mRight - theBounds.mLeft;
            float theDistance = (float)(sqrt((double)(theB * theB) + (double)(theA * theA)));
            theMaximum = UMFloatToLong(theDistance);
        } else {
            theMaximum = 30;
        }
        long theRadius = UMFloatToLong(inUnit->GetLineOfSight());
        this->mSearchRadius += 5;
        if (this->mSearchRadius + theRadius > theMaximum)
            this->mSearchRadius = theMaximum - theRadius;
        unsigned long theRing = this->mSearchRadius - 5 + theRadius;
        for (; !theFound && theRing < this->mSearchRadius + theRadius; ++theRing) {
            Fn2492C2(theCenter.GetX(), theCenter.mY, theRing, theTiles);
            EEAIRandomShuffle(theTiles.begin(), theTiles.end());
            for (long i = (long)theTiles.size() - 1; i >= 0 && !theFound; --i) {
                this->mTile.mX = theTiles[i].GetX();
                this->mTile.mY = theTiles[i].mY;
                EEMapTile* theTile = theMap->GetTile(this->mTile.GetX(), this->mTile.mY);
                if (theTile == NULL || theMap->IsEdgeTile(this->mTile.GetX(), this->mTile.GetY()))
                    continue;
                // the rings are searched only while mNoUnexploredAreas is false, so the test is always "visible
                // now" (the "ever seen" arm for mNoUnexploredAreas could never run and is gone)
                bool theSeen = theTile->IsVisibleTo(thePlayer);
                if (theSeen)
                    continue;
                if ((inUnit->mUnitClass->mDomains & kMovementAir) != 0 &&
                    (!theMap->mTerrainMesh.GetIsSpaceMap() || inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 12)))
                    continue;
                GE3DPoint thePoint((float)this->mTile.GetX() + 0.5f, (float)this->mTile.mY + 0.5f, 0.0f);
                if (this->mBadTiles.Contains(this->mTile) || (inUnit->mActiveGroup != NULL && inUnit->mActiveGroup->IsCellTaken(thePoint)))
                    continue;
                EETileCoordinate theDestination;
                theDestination.mX = this->mTile.GetX();
                theDestination.mY = this->mTile.mY;
                if (!EECanStandOn(theDestination, inUnit)) {
                    this->mBadTiles.Insert(this->mTile);
                    continue;
                }
                inUnit->mUnitClass->GetTerrainFlags(inUnit, theFlags);
                EETileCoordinate theFrom;
                theFrom.mX = theCenter.GetX();
                theFrom.mY = theCenter.mY;
                EEAutoPathRequest* thePath = gPathManager.RequestPath(thePlayer->GetPlayerID(),
                    theFrom, theDestination, &theFlags, 1, 10, NULL, 1);
                bool theReachable = false;
                if (thePath->mPath.size() > 0) {
                    EETileCoordinate theLast = thePath->mPath.back();
                    EEAIPathSubSector* theFirstSector = gContinentManager.GetPathSubSectorAt(theLast.GetX(), theLast.mY);
                    EEAIPathSubSector* theLastSector = gContinentManager.GetPathSubSectorAt(this->mTile.GetX(), this->mTile.mY);
                    theReachable = theFirstSector == theLastSector;
                    if (!theReachable && theFirstSector != NULL && theLastSector != NULL && theFirstSector->IsNeighbor(theLastSector))
                        theReachable = theFirstSector->AreLinksBlocked(theLastSector, theFlags);
                }
                thePath->Dereference();
                if (!theReachable) {
                    EEAIPathArea* theFirstArea = gContinentManager.GetPathAreaAt(this->mTile);
                    unsigned short theFirstID = theFirstArea != NULL ? theFirstArea->mID : 0;
                    EEAIPathArea* theLastArea = gContinentManager.GetPathAreaAt(theCenter);
                    unsigned short theLastID = theLastArea != NULL ? theLastArea->mID : 0;
                    if (theFirstID != theLastID) {
                        this->mBadTiles.Insert(this->mTile);
                        continue;
                    }
                }
                theFound = true;
            }
        }
        if (!theFound && theRing < theMaximum)
            goto done;
        if (theFound)
            goto selected;
    }
    if (inUnit->mActiveGroup == NULL || inUnit->mActiveGroup->GetNumUnits() < 5)
        this->mNoUnexploredAreas = true;
    if ((unsigned long)inUnit->mTileX < gMapWidth && (unsigned long)inUnit->mTileY < gMapHeight) {
        EEAIArea* theArea;
        if (((inUnit->mUnitClass->mDomains & kMovementAir) == 0 ||
            (theMap->mTerrainMesh.GetIsSpaceMap() && !inUnit->mUnitClass->CanMoveOnTerrain(inUnit, 12))) &&
            inUnit->mUnitClass->mUnitType != kUnitTypeSp15SpaceFighter)
            theArea = gContinentManager.GetContinentAt(inUnit);
        else
            theArea = dynamic_cast<EEAIContinent*>(gContinentManager.GetRandomContinent(kContinentCategoryNone, gPhysics->mRandom));
        if (theArea != NULL)
            this->mTile = theArea->GetRandomPoint();
    } else {
        long theX = gPhysics->GetRandom(0L, theMap->mL195614 - 1);
        long theY = gPhysics->GetRandom(0L, theMap->mL195610 - 1);
        this->mTile.mX = theY + theX;
        this->mTile.mY = theY + (theMap->mL195610 - theX);
    }
    this->mSearchRadius = 0;
    theFound = true;
selected:
    this->mSearchRadius = 0;
    theTarget = EEFindNearbyTarget(inUnit);
    if (theTarget != NULL)
        inUnit->mPlayer->NoticeForeignUnit(theTarget);
    if (inUnit->mActiveGroup != NULL) {
        GE3DPoint thePoint((float)this->mTile.GetX() + 0.5f, (float)this->mTile.mY + 0.5f, 0.0f);
        inUnit->mActiveGroup->TellGroup(inUnit, thePoint);
    }
done:
    return theFound;
}







// FUNCTION: EXE 0x621af3
// Appends "Explore:" to the one-line debug text
void EEUGExplore::AppendDebugLabel(UWideString& ioText)
{
    ioText += "Explore:";
}

// FUNCTION: EXE 0x621b05
// Appends "Exploring" to the debug text
void EEUGExplore::GetDebugText(UWideString& ioText)
{
    ioText += "Exploring";
}

// FUNCTION: EXE 0x621b17
// Returns the centre of the tile being explored (x and y only)
void EEUGExplore::GetTileCenter(GE3DPoint& outLocation)
{
    outLocation.SetX((float)this->mTile.GetX() + 0.5f);
    outLocation.SetY((float)this->mTile.GetY() + 0.5f);
}

// FUNCTION: EXE 0x621b35
// Returns the location of the tile being explored
void EEUGExplore::GetGoalLocation(GE3DPoint& outLocation)
{
    gWorld.GetMap()->GetTileLocation(this->mTile.GetX(), this->mTile.GetY(), outLocation);
}

// FUNCTION: EXE 0x621b4d
// On the find-unexplored-area action picks the next tile and resets the path attempts; a ship whose player owns the
// Lighthouse at Alexandria skips the ring search and goes to random points
void EEUGExplore::Step(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    if ((inUnit->mCommand ? inUnit->mCommand->mType : 0) != kActionFindUnexploredArea)
        return;
    if (!this->mNoUnexploredAreas && inUnit->mUnitClass->mFamily == kUnitFamilyShip) {
        EEGetPlayerUnitsOfType(kUnitTypeLighthouseAtAlexandria, inUnit->mPlayer->GetPlayerID(), theUnits);
        this->mNoUnexploredAreas = theUnits.size() > 0;
        this->mNoNonVisibleAreas = theUnits.size() > 0;
    }
    this->FindNextTile(inUnit);
    this->SetPathAttempts(0);
}

// FUNCTION: EXE 0x621bfc
// Reads the bad tiles, current tile, both no-areas flags and search radius from the saved game; returns the first
// read error
long EEUGExplore::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    U2DPoint<long> thePoint;
    unsigned long theCount = 0;
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0) {
        theResult = inFile.Read(theCount);
        this->mBadTiles.Clear();
        for (unsigned long i = 0; i < theCount; i++) {
            if (theResult != 0)
                return theResult;
            theResult = thePoint.Load(inFile);
            this->mBadTiles.Insert(thePoint);
        }
        if (theResult == 0) {
            theResult = this->mTile.Load(inFile);
            if (theResult == 0) {
                theResult = inFile.Read(this->mNoUnexploredAreas);
                if (theResult == 0) {
                    theResult = inFile.Read(this->mNoNonVisibleAreas);
                    if (theResult == 0)
                        theResult = inFile.Read(this->mSearchRadius);
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x621cbd
// Writes the bad tiles (count and points), the current tile, both no-areas flags and the search
// radius; returns the first write error
long EEUGExplore::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    std::vector<U2DPoint<long> > thePoints;
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    this->mBadTiles.BuildPointList(thePoints, 1);
    if (theResult == 0) {
        // the bad tiles Load reads back (the count was always written as 0, so they were lost; old saves hold 0)
        unsigned long theCount = thePoints.size();
        theResult = outFile.Write(theCount);
        for (unsigned long i = 0; i < theCount && theResult == 0; i++)
            theResult = thePoints[i].Save(outFile);
        if (theResult == 0) {
            theResult = this->mTile.Save(outFile);
            if (theResult == 0) {
                theResult = outFile.Write(this->mNoUnexploredAreas);
                if (theResult == 0) {
                    theResult = outFile.Write(this->mNoNonVisibleAreas);
                    if (theResult == 0)
                        theResult = outFile.Write(this->mSearchRadius);
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x621d67
// Answers the explore triggers (no unexplored areas left: never; searching: while the search radius is not 0);
// others go to the common goal
bool EEUGExplore::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    switch (inEvent) {
    case kTriggerNoUnexploredAreasLeft:
        return false;
    case kTriggerSearchingForAreaToExplore:
        return this->mSearchRadius != 0 ? true : false;
    }
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

// The slots ICF-folded into other units' identical bodies:
// ICF-FOLDED: EXE 0x6907a3
// Does nothing: the goal holds no units
void EEUGExplore::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
}

// ICF-FOLDED: EXE 0x4b3257
// Returns true: the goal suits any unit
bool EEUGExplore::Suits(EEUnit* inUnit)
{
    return true;
}
