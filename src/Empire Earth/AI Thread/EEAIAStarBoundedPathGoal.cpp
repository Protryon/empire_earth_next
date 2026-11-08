// C:\EEX\Empire Earth\AI Thread\EEAIAStarBoundedPathGoal.cpp (file name inferred: no __FILE__; RTTI class
// EEAIAStarBoundedPathGoal, vtables 0x439b64 / 0x439b8c). The bounded path search's goal: 0xe9a1..0xf53b (EEAIArea's
// COMDATs end at 0xe9a1; the modifiers' unit starts with EEAIPathGoalModifierBase::GetStepCost at 0xf53b). ctor, dtor,
// the members the machine calls, then the COMDATs of their first users: vector reserve/push_back/insert, the goal
// base ctor/??_G, the second base's thunks, EEAIAStarPathfindingGoalBase::ClearSearch, AcceptTile. The twin of
// EEAIAStarPathfindingGoal.cpp (its AcceptDefault/AcceptTile(EETile*) are ICF-folded into that unit's copies).
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
#include "ee/EEAIAStarBoundedStorage.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIAStarBoundedPathGoal.h"
#include "ee/EEAIAStarMachine.h"
#include "ee/EEUnitUtilities.h"

// 0x248e06 (cdecl): the tile inDistance steps from (inX, inY) in direction inDirection (0..7) (provisional name)

// .rdata 0x439b5c (next to the vtables): a unit's full hit point fraction
static const float kEEBPGFull = 1.0f;

// FUNCTION: EXE 0x40e9a1
// Creates a bounded path goal with no bounded modifier and the large-bounds flag set
EEAIAStarBoundedPathGoal::EEAIAStarBoundedPathGoal()
    : mBoundedModifier(NULL), mB8(true)
{
}

// SYNTHETIC: EXE 0x40e9e5
// (the scalar deleting destructor)

// FUNCTION: EXE 0x40ea01
// Destroys the goal (base members only)
EEAIAStarBoundedPathGoal::~EEAIAStarBoundedPathGoal()
{
}

// FUNCTION: EXE 0x40ea26
// Clears the search and the goal base, empties the search bounds and sets the large-bounds flag again
void EEAIAStarBoundedPathGoal::Reset()
{
    this->ClearSearch();
    EEAIAStarGoalBase<EEAIAStarBoundedStorage, EEMap>::Reset();
    this->mBounds.Clear();
    this->mB8 = true;
}

// FUNCTION: EXE 0x40ea54
// Drops the reference to the bounded modifier, then releases the base goal's modifier
void EEAIAStarBoundedPathGoal::ReleaseModifier()
{
    if (this->mBoundedModifier)
        this->mBoundedModifier->Dereference();
    this->mBoundedModifier = NULL;
    EEAIAStarPathfindingGoalBase::ReleaseModifier();
}

// FUNCTION: EXE 0x40ea7b
// Marks the search failed when the open lists ran empty, when the goal next to the current node cannot be
// entered, or when the machine used up its revolutions; a failure takes the nearest tile if allowed
void EEAIAStarBoundedPathGoal::SearchDone(EEAIAStarMachineBase* inMachine)
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

// NEAR: the original materialises the false result right after the distance tests
// FUNCTION: EXE 0x40eb14
// Returns whether a node ends the search: no node, the goal tile, or a neighbour of a blocked goal (a diagonal
// neighbour also needs both corner tiles enterable); lets the bounded modifier inspect the node
bool EEAIAStarBoundedPathGoal::IsGoalNode(EEAIAStarNode* inNode)
{
    bool theResult = inNode == NULL;
    if (!theResult) {
        bool theFar = UMAbs(inNode->mX - this->mGoalTile.mX) > 1 || UMAbs(inNode->mY - this->mGoalTile.mY) > 1;
        if (theFar)
            theResult = false;
        else if (inNode->mX == this->mGoalTile.mX && inNode->mY == this->mGoalTile.mY)
            theResult = true;
        else {
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
    }
    if (this->mModifier)
        this->mBoundedModifier->CheckNode(inNode);
    return theResult;
}

// FUNCTION: EXE 0x40ec0e
// Appends the found path's tiles to outPath
void EEAIAStarBoundedPathGoal::GetPath(std::vector<U2DPoint<long> >& outPath)
{
    long theX;
    long theY;
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

// FUNCTION: EXE 0x40ec7d
// Appends the found path to outPath and the tile of every closed and open search node to outVisited
void EEAIAStarBoundedPathGoal::GetPath(std::vector<U2DPoint<long> >& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    EEAIAStarNode* theNode;
    this->GetPath(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    for (theNode = this->mStorage->mFastClosedList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    for (theNode = this->mStorage->mFastOpenList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
}

// NEAR: the original computes the point before the loop head (rotated loop)
// FUNCTION: EXE 0x40ecf0
// Appends the found path to outPath as tile-centre points
void EEAIAStarBoundedPathGoal::GetPoints(std::vector<GE3DPoint>& outPath)
{
    long theY;
    long theX;
    if (!this->mStorage->GetFirstPathTile(theX, theY))
        return;
    GE3DPoint thePoint(theX + 0.5f, theY + 0.5f, 0.0f);
    for (;;) {
        outPath.push_back(thePoint);
        if (!this->mStorage->GetNextPathTile(theX, theY))
            break;
        thePoint = GE3DPoint(theX + 0.5f, theY + 0.5f, 0.0f);
    }
}

// FUNCTION: EXE 0x40ed71
// Appends the found path to outPath as tile-centre points and every closed and open node's tile to outVisited
void EEAIAStarBoundedPathGoal::GetPoints(std::vector<GE3DPoint>& outPath, std::vector<U2DPoint<long> >& outVisited)
{
    EEAIAStarNode* theNode;
    this->GetPoints(outPath);
    outVisited.reserve(this->mMachine->mRevs);
    for (theNode = this->mStorage->mFastClosedList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
    for (theNode = this->mStorage->mFastOpenList; theNode; theNode = theNode->mNext)
        outVisited.push_back(U2DPoint<long>(theNode->mX, theNode->mY));
}

// NEAR: the original loads the parent tile into registers before the vtable
// FUNCTION: EXE 0x40ede4
// Returns the modifier's step cost (given the current node's parent tile, or -10000,-10000 without one),
// else the default cost
long EEAIAStarBoundedPathGoal::GetCost(long inFromX, long inFromY, long inX, long inY)
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

// NEAR: the first GetTile is the out-of-line RTSMap::GetTile 0x2f85 in the original; theRange lives in inFromY's slot
// FUNCTION: EXE 0x40ee51
// Returns a step's cost: 10 straight or 14 diagonal, +50 (+150 if big) per idle or attacking unit on the tile
// off the goal; doubled for a big ranked unit near a blocked tile (continent test uses the entered tile, not the probe)
long EEAIAStarBoundedPathGoal::GetDefaultCost(long inFromX, long inFromY, long inX, long inY)
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
    if (inX != this->mGoalTile.GetX() || inY != this->mGoalTile.GetY()) {
        for (RTSTileUnitNode* theNode = theTile->GetUnits(); theNode; theNode = theNode->GetNext()) {
            EEComplexUnit* theUnit = theNode->GetUnit()->GetComplexUnit();
            if (!theUnit || theUnit == this->GetUnit() || !theUnit->GetUnitClass()->GetIsCollidable() || theUnit->IsFoundation())
                continue;
            if (theUnit->GetActiveGroup() && theUnit->GetActiveGroup() == this->GetUnit()->GetActiveGroup() && theUnit->GetCollisionRadius() < kEEBPGFull)
                continue;
            if ((theUnit->GetCommand() ? theUnit->GetCommand()->GetType() : 0) != kActionIdle
                && (theUnit->GetCommand() ? theUnit->GetCommand()->GetType() : 0) != kActionAttackEnemyUnit)
                continue;
            if (theUnit->GetCollisionRadius() >= kEEBPGFull && this->GetUnit()->GetUnitClass()->GetInnerCollisionSphere() < kEEBPGFull)
                theCost += 150;
            else
                theCost += 50;
        }
    }
    if (!(this->GetUnit()->GetUnitClass()->GetInnerCollisionSphere() >= kEEBPGFull))
        return theCost;
    theRange = this->GetUnit()->GetFormationRank();
    if (!theRange || ((this->GetUnit()->GetStats()->GetTerrainMask() >> 14) & 1) != 0)
        return theCost;
    theMap = gWorld.GetMap();
    theX = 0;
    theY = 0;
    EEGetTileInDirection(inX, inY, &theX, &theY, 4, theRange);
    theNeighbor = theMap->GetTile(theX, theY);
    if (theNeighbor && (theNeighbor->GetAIValue() || !this->GetUnit()->GetUnitClass()->CanMoveOnTerrain(this->GetUnit(), theNeighbor->GetTerrainType())
                        || this->GetContinentID() != gContinentManager.GetContinentID(theX, theY)))
        theCost *= 2;
    else {
        EEGetTileInDirection(inX, inY, &theX, &theY, 3, theRange);
        theNeighbor = theMap->GetTile(theX, theY);
        if (theNeighbor && (theNeighbor->GetAIValue() || !this->GetUnit()->GetUnitClass()->CanMoveOnTerrain(this->GetUnit(), theNeighbor->GetTerrainType())
                            || this->GetContinentID() != gContinentManager.GetContinentID(theX, theY)))
            theCost *= 2;
        else {
            EEGetTileInDirection(inX, inY, &theX, &theY, 1, theRange);
            theNeighbor = theMap->GetTile(theX, theY);
            if (theNeighbor && (theNeighbor->GetAIValue()
                                || !this->GetUnit()->GetUnitClass()->CanMoveOnTerrain(this->GetUnit(), theNeighbor->GetTerrainType())
                                || this->GetContinentID() != gContinentManager.GetContinentID(theX, theY)))
                theCost *= 2;
            else {
                EEGetTileInDirection(inX, inY, &theX, &theY, 6, theRange);
                theNeighbor = theMap->GetTile(theX, theY);
                if (theNeighbor && (theNeighbor->GetAIValue()
                                    || !this->GetUnit()->GetUnitClass()->CanMoveOnTerrain(this->GetUnit(), theNeighbor->GetTerrainType())
                                    || this->GetContinentID() != gContinentManager.GetContinentID(theX, theY)))
                    theCost *= 2;
            }
        }
    }
    return theCost;
}

// FUNCTION: EXE 0x40f1c8
// Prepares the search: bounds it to the start and goal tiles plus 3, accepts the nearest tile, and sets the
// large-bounds flag when the bounds exceed 20 tiles both ways; returns true
bool EEAIAStarBoundedPathGoal::Prepare()
{
    EEAIAStarPathfindingGoalBase::Prepare();
    this->SetStartBounds(this->mStartTile, 3);
    this->SetGoalBounds(this->mGoalTile, 3);
    this->mAcceptNearest = true;
    this->mStorage->mSearchBounds = this->mBounds;
    this->mB8 = this->mBounds.mRight - this->mBounds.mLeft > 20 && this->mBounds.mBottom - this->mBounds.mTop > 20;
    return true;
}

// FUNCTION: EXE 0x40f251
// Replaces the goal's modifier and keeps a reference to it as the bounded modifier (it must be one)
void EEAIAStarBoundedPathGoal::SetModifier(EEAIPathGoalModifierBase* inModifier)
{
    this->ReleaseModifier();
    EEAIAStarPathfindingGoalBase::SetModifier(inModifier);
    this->mBoundedModifier = dynamic_cast<EEAIBoundedPathGoalModifierBase*>(inModifier);
    this->mBoundedModifier->Reference();
    this->mBoundedModifier->SetGoal(this);
}

// FUNCTION: EXE 0x40f298
// Collects the blocked tiles around the goal and marks each one closed in the search storage
void EEAIAStarBoundedPathGoal::MarkBlockedTiles()
{
    this->CollectBlockedTiles(this->mGoalTile);
    for (long i = this->mBlockedTiles.size() - 1; i >= 0; i--)
        this->mStorage->MarkTile(this->mBlockedTiles[i].mX, this->mBlockedTiles[i].mY, false);
}

// Returns whether the unit can move on the tile's terrain and pass every unit on it (false on a map edge)
// (no address: ICF-folded into EEAIAStarPathfindingGoal's identical 0x1990c)
bool EEAIAStarBoundedPathGoal::AcceptDefault(EEMapTile* inTile, long inX, long inY, bool inGoal)
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

// Returns whether the unit may enter the tile: the modifier decides, else the default test
// (no address: ICF-folded into EEAIAStarPathfindingGoal's identical 0x19a3f)
bool EEAIAStarBoundedPathGoal::AcceptTile(EEMapTile* inTile, long inX, long inY, bool inGoal)
{
    if (this->mModifier == NULL)
        return this->AcceptDefault(inTile, inX, inY, inGoal);
    return this->mModifier->CanEnter(inTile, inX, inY, inGoal);
}

// FUNCTION: EXE 0x40f4bc
// Returns whether the unit may enter a map tile that lies inside the search bounds
inline bool EEAIAStarBoundedPathGoal::AcceptTile(long inX, long inY, bool inGoal)
{
    EEMapTile* theTile = this->mMap->GetTile(inX, inY);
    if (theTile && this->mBounds.Contains(inX, inY))
        return this->AcceptTile(theTile, inX, inY, inGoal);
    return false;
}

// Compiler-generated for this unit (first users here): vector<U2DPoint<long> >::reserve 0xf2dc, vector<GE3DPoint>::
// push_back 0xf38e / insert 0xf3f4 / _Ufill 0xf44e, the goal base ctor 0xf3ba and ??_G 0xf51e, the second base's
// thunks 0xf479 (??_E) / 0xf481 (Virtual0C), EEAIAStarPathfindingGoalBase::ClearSearch 0xf489.
// TEMPLATE: EXE 0x40f2dc
// TEMPLATE: EXE 0x40f38e
// TEMPLATE: EXE 0x40f3ba
// TEMPLATE: EXE 0x40f3f4
// TEMPLATE: EXE 0x40f44e
// TEMPLATE: EXE 0x40f479
// TEMPLATE: EXE 0x40f481
// TEMPLATE: EXE 0x40f489
// TEMPLATE: EXE 0x40f51e
