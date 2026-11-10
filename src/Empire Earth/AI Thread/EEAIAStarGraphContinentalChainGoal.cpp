// C:\EEX\Empire Earth\AI Thread\EEAIAStarGraphContinentalChainGoal.cpp (file name inferred: no __FILE__; RTTI
// class EEAIAStarGraphContinentalChainGoal, vtable 0x439eb4). The continent chain goal: 0x15dae..0x16070
// (EEAIAStarFastStorage's Virtual10 ends at 0x15dae; EEAIAStarGraphStorage::FreeNodePool starts at 0x16070). ctor, the
// members the graph machine calls, then the base's ctor/Reset/??_G and the vector<long>::push_back COMDATs.
#include <windows.h>
#include <string.h>
#include <vector>
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

#include "ee/EmpireEarth.h"
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
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGraphTerrainStorage.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIAStarGraphContinentalChainGoal.h"

// FUNCTION: EXE 0x415dae
// Creates the continent chain goal with no start continent, no chain kind and no continent category allowed
EEAIAStarGraphContinentalChainGoal::EEAIAStarGraphContinentalChainGoal()
    : mStartContinent(NULL), mStartCategory(false)
{
    this->mCount = 0;
    this->mKind = kAIChainNone;
    for (long i = 0; i < 5; i++)
        this->mKinds[i] = false;
}

// FUNCTION: EXE 0x415dd5
// Gives every continent the search reached (closed and open) the next chain id of the goal's chain kind
void EEAIAStarGraphContinentalChainGoal::NumberChain()
{
    unsigned long theKind = this->mKind;
    unsigned short theID = theKind < 7 ? ++gContinentManager.mNextIDs[theKind] : 0;
    // the closed list, then the fast open list (the storage's lists are Rust's: ee_astar graph.rs)
    static const EeAStarGraphList kLists[2] = {EeAStarGraphList::Closed, EeAStarGraphList::FastOpen};
    std::vector<AStarGraphableObjectBase*> theObjects;
    for (long j = 0; j < 2; j++) {
        theObjects.clear();
        this->mStorage->ListObjects(kLists[j], theObjects);
        for (size_t i = 0; i < theObjects.size(); i++) {
            EEAIContinent* theContinent = static_cast<EEAIContinent*>(theObjects[i]);
            if (this->mKind < 7)
                theContinent->mCounts[this->mKind] = theID;
        }
    }
}

// FUNCTION: EXE 0x415e50
// Appends the found chain's node indices to outIndices
void EEAIAStarGraphContinentalChainGoal::GetPath(std::vector<long>& outIndices)
{
    EETileCoordinate theTile;
    long theIndex;
    theTile.mX = 0;
    theTile.mY = 0;
    theIndex = 0;
    bool theMore = this->mStorage->GetFirstPathNode(theTile, theIndex);
    if (theMore)
        outIndices.push_back(theIndex);
    while (theMore) {
        theTile.mX = 0;
        theTile.mY = 0;
        theIndex = 0;
        theMore = this->mStorage->GetNextPathNode(theTile, theIndex);
        if (theMore == true)
            outIndices.push_back(theIndex);
    }
}

// FUNCTION: EXE 0x415ebc
// Appends the found chain's tiles to outTiles (skipping a repeat of the last tile) and its node indices to
// outIndices
void EEAIAStarGraphContinentalChainGoal::GetPath(std::vector<EETileCoordinate>& outTiles,
                                                 std::vector<long>& outIndices)
{
    EETileCoordinate theTile;
    long theIndex;
    theTile.mX = 0;
    theTile.mY = 0;
    theIndex = 0;
    bool theMore = this->mStorage->GetFirstPathNode(theTile, theIndex);
    if (theMore) {
        if (outTiles.size() == 0 || outTiles.back() != theTile)
            outTiles.push_back(theTile);
        outIndices.push_back(theIndex);
    }
    while (theMore) {
        theTile.mX = 0;
        theTile.mY = 0;
        theIndex = 0;
        theMore = this->mStorage->GetNextPathNode(theTile, theIndex);
        if (theMore != true)
            continue;
        if (outTiles.size() == 0 || outTiles.back() != theTile)
            outTiles.push_back(theTile);
        outIndices.push_back(theIndex);
    }
}

// FUNCTION: EXE 0x415f97
// Returns whether the chain may grow onto a neighbouring continent (the same one, or one of an allowed
// category), counting each accepted step
bool EEAIAStarGraphContinentalChainGoal::AcceptNeighbor(EEAIContinent* inNeighbor, EEAIContinent* inFrom)
{
    bool theResult = inNeighbor == inFrom;
    if (!theResult)
        theResult = this->mKinds[inNeighbor->mCategory];
    if (theResult == true)
        this->mCount++;
    return theResult;
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x415fbb
// TEMPLATE: EXE 0x415ff5
// TEMPLATE: EXE 0x416027
// SYNTHETIC: EXE 0x416053
