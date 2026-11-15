// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathGraphGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarPathGraphGoal, vtable 0x43a06c). The sub sector graph search's goal: 0x1a64b..0x1a9ec
// (EEAIAStarPathfindingGoalBase.cpp's COMDATs end at 0x1a64b; EEAIAStarPathSectorGoal's ctor starts at 0x1a9ec). ctor,
// dtor, the members the machine calls, then the COMDATs of their first users: the goal base ctor/Virtual10/??_G and
// EEAIPathSubSector::IsNeighbor.
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <deque>
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

#include "ee/EmpireEarth.h"
#include "ee/EEMathInline.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIAStarPathGraphGoal.h"
#include "ee/EEAIAStarMachine.h"

// FUNCTION: EXE 0x41a64b
// Creates the sub-sector graph path goal with no unit, no modifier and no endpoints
EEAIAStarPathGraphGoal::EEAIAStarPathGraphGoal()
{
    this->mUnit = NULL;
    this->mModifier = NULL;
    this->Reset();
}

// SYNTHETIC: EXE 0x41a68c
// (the scalar deleting destructor)

// FUNCTION: EXE 0x41a6a8
// Destroys the goal, releasing its unit and its modifier
EEAIAStarPathGraphGoal::~EEAIAStarPathGraphGoal()
{
    this->SetUnit(NULL);
    this->ReleaseModifier();
}

// FUNCTION: EXE 0x41a6c7
// Marks the search failed when the open lists run dry or the machine's revolution limit is hit before the current
// node, next to the goal tile, is the start sector
void EEAIAStarPathGraphGoal::SearchDone(EEAIAStarGraphMachineBase* inMachine)
{
    // the current node and whether an open node is left, as the machine's hook noted them (the storage's lists are
    // Rust's: ee_astar graph.rs)
    bool theFound = false;
    if (!this->mStorage->mHookHasCurrent) {
        if (!this->mStorage->mHookHasOpen) {
            this->mSearchFailed = true;
            this->mTookNearest = this->mAcceptNearest;
        }
    } else {
        const EEAIAStarGraphNode& theNode = this->mStorage->mHookCurrent;
        if (UMAbs(theNode.mTile.mX - this->mGoalTile.mX) <= 1 && UMAbs(theNode.mTile.mY - this->mGoalTile.mY) <= 1) {
            this->mSearchFailed = false;
            this->mTookNearest = this->mAcceptNearest;
            theFound = theNode.mIndex == this->mStartSector;
        }
    }
    if (!this->mSearchFailed) {
        long theRevs;
        this->mSearchFailed = !theFound && (theRevs = inMachine->mRevs) >= inMachine->GetMaxRevs();
    }
}

// FUNCTION: EXE 0x41a76f
// Returns true for no node, the target sub-sector, or a neighbour of it at its sector centre when the unit cannot
// enter the target's kind
bool EEAIAStarPathGraphGoal::IsGoalNode(EEAIAStarGraphNode* inNode)
{
    bool theResult = inNode == NULL;
    if (!theResult) {
        EEAIPathSubSector* theSector = static_cast<EEAIPathSubSector*>(inNode->mObject);
        theResult = theSector == this->mTo;
        if (!theResult && UMAbs(theSector->mSector->mCenter.mX - this->mPoint40.mX) <= 1
            && UMAbs(theSector->mSector->mCenter.mY - this->mPoint40.mY) <= 1 && theSector->IsNeighbor(this->mTo))
            theResult = !this->mPathFlags.mFlags[this->mTo->mKind];
    }
    return theResult;
}

// FUNCTION: EXE 0x41a7d0
// Fills the goal's passable-terrain flags from the unit's class (no unit: unchanged)
void EEAIAStarPathGraphGoal::SetFlags(EEComplexUnit* inUnit)
{
    if (inUnit)
        inUnit->mUnitClass->GetTerrainFlags(inUnit, this->mPathFlags);
}

// FUNCTION: EXE 0x41a7ec
// Returns whether the search may enter a sub-sector: its kind is passable (or it is a gate open to the player)
// and, coming from a linked sub-sector, a passable link joins them
bool EEAIAStarPathGraphGoal::AcceptNeighbor(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
{
    bool theResult = this->mPathFlags.mFlags[inSector->mKind];
    if (!theResult && inSector->mKind == kAITileGate)
        theResult = inSector->IsPassable(this->mPlayerID);
    if (theResult && inFrom && inSector->mLinks.size() > 0)
        theResult = this->HasPassableLink(inSector, inFrom);
    return theResult;
}

// FUNCTION: EXE 0x41a830
// Returns true when the sub-sector has no links to inFrom or any such link has a passable end
bool EEAIAStarPathGraphGoal::HasPassableLink(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    inSector->GetLinks(inFrom->mID, theLinks);
    bool theResult = theLinks.size() == 0;
    for (long i = theLinks.size() - 1; i >= 0 && !theResult; i--) {
        if (this->mPathFlags.mFlags[theLinks[i].first->mKind] == true || this->mPathFlags.mFlags[theLinks[i].second->mKind] == true)
            theResult = true;
    }
    return theResult;
}

// NEAR: the original copies the sector location as one 8-byte point (EEAIPathSector +0x40 is a U2DPoint<long>?)
// FUNCTION: EXE 0x41a8c2
// Loads the unit's terrain flags and caches the target sector's centre; always returns true
bool EEAIAStarPathGraphGoal::Prepare()
{
    this->SetFlags(this->mUnit);
    this->mPoint40 = this->mTo->mSector->mCenter;
    return true;
}

// FUNCTION: EXE 0x41a8e6
// Resets the goal: releases the unit and modifier, clears both endpoints and the terrain flags
void EEAIAStarPathGraphGoal::Reset()
{
    EEAIAStarGraphGoalBase<EEAIAStarGraphStorage>::Reset();
    this->SetUnit(NULL);
    this->ReleaseModifier();
    this->mL70 = 0;
    this->mFrom = NULL;
    this->mTo = NULL;
    memset(this->mPathFlags.mFlags, 0, sizeof(this->mPathFlags.mFlags));
    this->mPathFlags.mPlayerID = 0;
}

// (no address: ICF-folded into EECPAttackSubGroupUnit::ReleaseSubGroup 0x65d0c)
// Dereferences the goal's modifier and clears it
void EEAIAStarPathGraphGoal::ReleaseModifier()
{
    if (this->mModifier)
        this->mModifier->Dereference();
    this->mModifier = NULL;
}

// FUNCTION: EXE 0x41a91e
// Replaces the goal's modifier, referencing the new one and pointing it back at this goal
void EEAIAStarPathGraphGoal::SetModifier(EEAIGraphPathGoalModifierBase* inModifier)
{
    this->ReleaseModifier();
    this->mModifier = inModifier;
    inModifier->Reference();
    this->mModifier->mGoal = this;
}

// FUNCTION: EXE 0x41a93a
// Sets the unit to path for, moving the reference from the old unit to the new one
void EEAIAStarPathGraphGoal::SetUnit(EEComplexUnit* inUnit)
{
    if (inUnit == this->mUnit)
        return;
    if (this->mUnit)
        this->mUnit->Dereference();
    this->mUnit = inUnit;
    if (inUnit)
        inUnit->Reference();
}

// (no address: ICF-folded into EEAIAStarGraphContinentalChainGoal's identical 0x15e50; EEPathManager calls it)
// Appends the found path's sub-sector indices in order
void EEAIAStarPathGraphGoal::GetPath(std::vector<long>& outIndices)
{
    EETileCoordinate theTile;
    long theIndex;
    theTile.mX = 0;
    theTile.mY = 0;
    theIndex = 0;
    if (!this->mStorage->GetFirstPathNode(theTile, theIndex))
        return;
    outIndices.push_back(theIndex);
    bool theMore;
    do {
        theTile.mX = 0;
        theTile.mY = 0;
        theIndex = 0;
        theMore = this->mStorage->GetNextPathNode(theTile, theIndex);
        if (theMore == true)
            outIndices.push_back(theIndex);
    } while (theMore);
}

// (no address: ICF-folded into EEAIAStarGraphContinentalChainGoal's identical 0x15ebc; EEPathManager calls it)
// Appends the found path's tiles (skipping repeats of the last tile) and sub-sector indices in order
void EEAIAStarPathGraphGoal::GetPath(std::vector<EETileCoordinate>& outTiles, std::vector<long>& outIndices)
{
    EETileCoordinate theTile;
    long theIndex;
    theTile.mX = 0;
    theTile.mY = 0;
    theIndex = 0;
    if (!this->mStorage->GetFirstPathNode(theTile, theIndex))
        return;
    if (outTiles.size() == 0 || outTiles.back().mX != theTile.mX || outTiles.back().mY != theTile.mY)
        outTiles.push_back(theTile);
    outIndices.push_back(theIndex);
    bool theMore;
    do {
        theTile.mX = 0;
        theTile.mY = 0;
        theIndex = 0;
        theMore = this->mStorage->GetNextPathNode(theTile, theIndex);
        if (theMore == true) {
            if (outTiles.size() == 0 || outTiles.back().mX != theTile.mX || outTiles.back().mY != theTile.mY)
                outTiles.push_back(theTile);
            outIndices.push_back(theIndex);
        }
    } while (theMore);
}

// Compiler-generated for this unit (first users here): the graph goal base ctor 0x1a961, EEAIPathSubSector::IsNeighbor
// 0x1a99b, the base's Virtual10 0x1a9be (the kept copy) and ??_G 0x1a9cf.
// TEMPLATE: EXE 0x41a961
// TEMPLATE: EXE 0x41a99b
// TEMPLATE: EXE 0x41a9be
// TEMPLATE: EXE 0x41a9cf
