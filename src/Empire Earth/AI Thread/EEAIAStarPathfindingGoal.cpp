// C:\EEX\Empire Earth\AI Thread\EEAIAStarPathfindingGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarPathfindingGoal, vtables 0x439ffc / 0x43a014). The unbounded path search's goal: 0x190bf..0x19a93
// (EEAIAStarPathAreaStorage's dtor ends at 0x190bf; EEAIAStarPathfindingGoalBase's ctor starts at 0x19a93). ctor, the
// members the machine calls, then the COMDATs: the goal base ctor/??_G and the second base's adjustor thunks. The
// twin of EEAIAStarBoundedPathGoal.cpp (same bodies over the same storage layout; Virtual10 ICF-folded into its
// 0xf298).
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
#include "ee/EEMathInline.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarTerrainStorage.h"
#include "ee/EEPlayer.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"
#include "ee/EEAIAStarPathfindingGoal.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEUnitUtilities.h"

// 0x248e06 (cdecl): the tile inDistance steps from (inX, inY) in direction inDirection (0..7) (provisional name)

// .rdata 0x439ff4 (next to the vtables): a unit's full hit point fraction
static const float kEEPFGFull = 1.0f;

// FUNCTION: EXE 0x4190bf
// Creates the unbounded unit path search goal (bases only)
EEAIAStarPathfindingGoal::EEAIAStarPathfindingGoal()
{
}

// SYNTHETIC: EXE 0x4190e3
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4190ff
// Destroys the goal (bases only)
EEAIAStarPathfindingGoal::~EEAIAStarPathfindingGoal()
{
}

// FUNCTION: EXE 0x419124
// Clears the last search's state and resets the goal base
void EEAIAStarPathfindingGoal::Reset()
{
    this->ClearSearch();
    EEAIAStarGoalBase<EEAIAStarStorage, EEMap>::Reset();
}

// FUNCTION: EXE 0x419138
// Marks the search failed when the open lists run dry, when the reached tile next to the goal is not enterable,
// or when the machine has used up its revolutions
void EEAIAStarPathfindingGoal::SearchDone(EEAIAStarMachineBase* inMachine)
{
    EEAIAStarNode* theNode = this->mStorage->mCurrentNode;
    if (theNode == NULL) {
        bool theOpen = this->mStorage->mFastOpenList != NULL || this->mStorage->mSortedOpen != NULL;
        for (long i = this->mStorage->mBucketCount - 1; i >= 0 && !theOpen; i--)
            theOpen = this->mStorage->mOpenTable[i] != NULL;
        if (!theOpen) {
            this->mSearchFailed = true;
            this->mTookNearest = this->mAcceptNearest;
        }
    } else if (UMAbs(theNode->mX - this->mGoalTile.mX) <= 1 && UMAbs(theNode->mY - this->mGoalTile.mY) <= 1) {
        this->mSearchFailed = !this->AcceptTile(theNode->mX, theNode->mY, true);
        this->mTookNearest = this->mAcceptNearest;
    }
    if (!this->mSearchFailed) {
        long theRevs = inMachine->mRevs;
        this->mSearchFailed = theRevs >= inMachine->GetMaxRevs();
    }
}

// FUNCTION: EXE 0x4191d1
// Returns true for no node, the goal tile, or a tile next to an unenterable goal (diagonals only when both corner
// tiles are enterable while corner checks are on)
bool EEAIAStarPathfindingGoal::IsGoalNode(EEAIAStarNode* inNode)
{
    bool theResult = inNode == NULL;
    if (theResult)
        return theResult;
    if (UMAbs(inNode->mX - this->mGoalTile.mX) > 1)
        return false;
    if (UMAbs(inNode->mY - this->mGoalTile.mY) > 1)
        return false;
    if (inNode->mX == this->mGoalTile.mX && inNode->mY == this->mGoalTile.mY)
        return true;
    {
        theResult = !this->AcceptTile(this->mGoalTile.mX, this->mGoalTile.mY, true);
        if (theResult) {
            long theDX = inNode->mX - this->mGoalTile.mX;
            long theDY = inNode->mY - this->mGoalTile.mY;
            if (this->mMachine->mCheckCorners && UMAbs(theDX) == 1 && UMAbs(theDY) == 1) {
                theResult = this->AcceptTile(inNode->mX + theDX, inNode->mY + theDY, false);
                if (theResult)
                    theResult = this->AcceptTile(inNode->mX - theDX, inNode->mY - theDY, false);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x41929e
// Appends the found path's tiles in order
void EEAIAStarPathfindingGoal::GetPath(std::vector<U2DPoint<long> >& outPath)
{
    long theX = 0;
    long theY = 0;
    if (!this->mStorage->GetFirstPathTile(theX, theY))
        return;
    outPath.push_back(U2DPoint<long>(theX, theY));
    bool theMore;
    do {
        theMore = this->mStorage->GetNextPathTile(theX, theY);
        if (theMore == true)
            outPath.push_back(U2DPoint<long>(theX, theY));
    } while (theMore);
}

// FUNCTION: EXE 0x419315
// Appends the found path's tiles, and every tile on the fast closed and open lists to outVisited
void EEAIAStarPathfindingGoal::GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    EEAIAStarNode* theNode;
    this->GetPath(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    for (theNode = this->mStorage->mFastClosedList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    for (theNode = this->mStorage->mFastOpenList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
}

// NEAR: the original adds the offsets as fld member + faddp
// FUNCTION: EXE 0x419388
// Appends the found path as world points (tile plus the goal's offset, height 0)
void EEAIAStarPathfindingGoal::GetPoints(std::vector<GE3DPoint>& outPath)
{
    long theX = 0;
    long theY = 0;
    if (!this->mStorage->GetFirstPathTile(theX, theY))
        return;
    outPath.push_back(GE3DPoint(theX + this->mOffsetX, theY + this->mOffsetY, 0.0f));
    bool theMore;
    do {
        theMore = this->mStorage->GetNextPathTile(theX, theY);
        if (theMore == true)
            outPath.push_back(GE3DPoint(theX + this->mOffsetX, theY + this->mOffsetY, 0.0f));
    } while (theMore);
}

// FUNCTION: EXE 0x419429
// Appends the found path as world points, and every tile on all open and closed lists to outVisited
void EEAIAStarPathfindingGoal::GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    EEAIAStarNode* theNode;
    long i;
    this->GetPoints(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    for (theNode = this->mStorage->mSortedOpen; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    for (theNode = this->mStorage->mFastClosedList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    for (i = this->mStorage->mBucketCount - 1; i >= 0; i--)
        for (theNode = this->mStorage->mClosedTable[i]; theNode; theNode = theNode->mNext)
            outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    for (theNode = this->mStorage->mFastOpenList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    for (i = this->mStorage->mBucketCount - 1; i >= 0; i--)
        for (theNode = this->mStorage->mOpenTable[i]; theNode; theNode = theNode->mNext)
            outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
}

// NEAR: the original loads the parent tile into registers before the vtable
// FUNCTION: EXE 0x419528
// Returns the step cost between two tiles: the modifier's cost (given the current node's parent, or -10000s)
// when one is set, else the default cost
long EEAIAStarPathfindingGoal::GetCost(long inFromX, long inFromY, long inX, long inY)
{
    if (this->mModifier == NULL)
        return this->GetDefaultCost(inFromX, inFromY, inX, inY);
    EEAIAStarNode* theNode = this->mStorage->mCurrentNode;
    if (theNode && theNode->mParent) {
        long theX = theNode->mParent->mX;
        long theY = theNode->mParent->mY;
        return this->mModifier->GetCost(inFromX, inFromY, inX, inY, theX, theY);
    }
    return this->mModifier->GetCost(inFromX, inFromY, inX, inY, -10000, -10000);
}

// NEAR: the first GetTile is the out-of-line RTSMap::GetTile 0x2f85 in the original; theRange lives in inFromY's slot.
// The continent test of each side tile checks (inX, inY), not the side tile, as in the original (bytes 0x41976a)
// FUNCTION: EXE 0x419595
// Returns the default step cost: 10 straight or 14 diagonal, plus 50/150 per idle or attacking unit in the way, doubled
// for wide formation units when a tile a rank away is blocked, impassable or off-continent (tests (inX, inY))
long EEAIAStarPathfindingGoal::GetDefaultCost(long inFromX, long inFromY, long inX, long inY)
{
    EEMapTile* theTile;
    long theCost;
    long theRange;
    RTSMap* theMap;
    long theX;
    long theY;
    EEMapTile* theNeighbor;
    theTile = this->mMap->GetTile(inX, inY);
    if (inFromX == inX || inFromY == inY)
        theCost = 10;
    else
        theCost = 14;
    if (inX != this->mGoalTile.mX || inY != this->mGoalTile.mY) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode; theNode = theNode->mNext) {
            EEComplexUnit* theUnit = theNode->mUnit->mComplexUnit;
            if (!theUnit || theUnit == this->mUnit || !theUnit->GetUnitClass()->mIsCollidable || theUnit->IsFoundation())
                continue;
            if (theUnit->mActiveGroup && theUnit->mActiveGroup == this->mUnit->mActiveGroup && theUnit->mCollisionRadius < kEEPFGFull)
                continue;
            if ((theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionIdle
                && (theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionAttackEnemyUnit)
                continue;
            if (theUnit->mCollisionRadius >= kEEPFGFull && this->mUnit->GetUnitClass()->mInnerCollisionSphere < kEEPFGFull)
                theCost += 150;
            else
                theCost += 50;
        }
    }
    if (!(this->mUnit->GetUnitClass()->mInnerCollisionSphere >= kEEPFGFull))
        return theCost;
    theRange = this->mUnit->mFormationRank;
    if (!theRange || ((this->mUnit->mStats->mTerrainMask >> 14) & 1) != 0)
        return theCost;
    theMap = gWorld.mMap;
    theX = 0;
    theY = 0;
    EEGetTileInDirection(inX, inY, &theX, &theY, 4, theRange);
    theNeighbor = theMap->GetTile(theX, theY);
    if (theNeighbor && (theNeighbor->mAIValue || !this->mUnit->GetUnitClass()->CanMoveOnTerrain(this->mUnit, theNeighbor->mTerrainType)
                        || this->mContinentID != gContinentManager.GetContinentID(theX, theY)))
        theCost *= 2;
    else {
        EEGetTileInDirection(inX, inY, &theX, &theY, 3, theRange);
        theNeighbor = theMap->GetTile(theX, theY);
        if (theNeighbor && (theNeighbor->mAIValue || !this->mUnit->GetUnitClass()->CanMoveOnTerrain(this->mUnit, theNeighbor->mTerrainType)
                            || this->mContinentID != gContinentManager.GetContinentID(theX, theY)))
            theCost *= 2;
        else {
            EEGetTileInDirection(inX, inY, &theX, &theY, 1, theRange);
            theNeighbor = theMap->GetTile(theX, theY);
            if (theNeighbor && (theNeighbor->mAIValue
                                || !this->mUnit->GetUnitClass()->CanMoveOnTerrain(this->mUnit, theNeighbor->mTerrainType)
                                || this->mContinentID != gContinentManager.GetContinentID(theX, theY)))
                theCost *= 2;
            else {
                EEGetTileInDirection(inX, inY, &theX, &theY, 6, theRange);
                theNeighbor = theMap->GetTile(theX, theY);
                if (theNeighbor && (theNeighbor->mAIValue
                                    || !this->mUnit->GetUnitClass()->CanMoveOnTerrain(this->mUnit, theNeighbor->mTerrainType)
                                    || this->mContinentID != gContinentManager.GetContinentID(theX, theY)))
                    theCost *= 2;
            }
        }
    }
    return theCost;
}

// FUNCTION: EXE 0x41990c
// Returns whether the unit can enter a tile: not a map edge tile, terrain it can move on, and every unit on it
// passable to it
bool EEAIAStarPathfindingGoal::AcceptDefault(EEMapTile* inTile, long inX, long inY, bool inGoal)
{
    bool theResult;
    if (!this->mMap->IsEdgeTile(inX, inY)) {
        long theType = inTile->mTerrainType;
        if (this->mUnit->GetUnitClass()->CanMoveOnTerrain(this->mUnit, theType)) {
            theResult = true;
            for (RTSTileUnitNode* theNode = inTile->mUnits; theNode; theNode = theNode->mNext) {
                if (!theNode->mUnit->IsPassableBy(this->mUnit)) {
                    theResult = false;
                    break;
                }
            }
        } else
            theResult = false;
    } else
        theResult = false;
    return theResult;
}

// FUNCTION: EXE 0x419983
// Prepares the base and sets 3-tile start and goal bounds around the start and goal tiles; always returns true
bool EEAIAStarPathfindingGoal::Prepare()
{
    EEAIAStarPathfindingGoalBase::Prepare();
    this->SetStartBounds(this->mStartTile, 3);
    this->SetGoalBounds(this->mGoalTile, 3);
    return true;
}

// FUNCTION: EXE 0x4199fa
// Returns whether the unit can enter tile (inX, inY); false off the map
bool EEAIAStarPathfindingGoal::AcceptTile(long inX, long inY, bool inGoal)
{
    EEMapTile* theTile = this->mMap->GetTile(inX, inY);
    if (theTile)
        return this->AcceptTile(theTile, inX, inY, inGoal);
    return false;
}

// FUNCTION: EXE 0x419a3f
// Returns whether the unit can enter a tile: the modifier decides when one is set, else the default test
bool EEAIAStarPathfindingGoal::AcceptTile(EEMapTile* inTile, long inX, long inY, bool inGoal)
{
    if (this->mModifier == NULL)
        return this->AcceptDefault(inTile, inX, inY, inGoal);
    return this->mModifier->CanEnter(inTile, inX, inY, inGoal);
}

// Second base +0x10 (no address: ICF-folded into EEAIAStarBoundedPathGoal's identical 0xf298)
// Collects the tiles blocked around the goal and marks each one in the search storage
void EEAIAStarPathfindingGoal::MarkBlockedTiles()
{
    this->CollectBlockedTiles(this->mGoalTile);
    for (long i = this->mBlockedTiles.size() - 1; i >= 0; i--)
        this->mStorage->MarkTile(this->mBlockedTiles[i].mX, this->mBlockedTiles[i].mY, false);
}

// Compiler-generated for this unit: EEAIAStarGoalBase<EEAIAStarStorage, EEMap>'s ctor
// 0x199b0 and ??_G 0x19a76, the second base's adjustor thunks 0x199ea (??_E) / 0x199f2 (Virtual0C).
// TEMPLATE: EXE 0x4199b0
// TEMPLATE: EXE 0x4199ea
// TEMPLATE: EXE 0x4199f2
// TEMPLATE: EXE 0x419a76
