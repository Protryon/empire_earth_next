// C:\EEX\Empire Earth\AI Thread\EEAIAStarBoundedPathGoalModifiers.cpp (file name inferred: no __FILE__; RTTI
// classes EEAIBoundedPathGoalModifierBase .. EEAIGPGModifierSpaceRoutePlanner, vtables 0x439bc4..0x439ce0). The path
// goals' modifiers: 0xf53b..0x10bc7 (EEAIAStarBoundedPathGoal.cpp's COMDATs end at 0xf53b; EEAIAStarBoundedStorage's
// ctor starts at 0x10bc7). One TU: the base classes' inline slot COMDATs (both bases, 0x10b12..0x10baa) form one
// cluster at the end. The shared tile tests of the land/sea/space modifiers, the bounded goal's modifiers, then the
// graph goal's influence-cost route planners. Classes: ee/EEAIPathGoalModifierBase.h,
// ee/EEAIBoundedPathGoalModifierBase.h, ee/EEAIPathGoalModifiers.h.
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
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "lle/UReferenceCounter.h"
#include "lle/LowLevelEngineCore.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMathInline.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETileCoordinate.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIAStarFastStorage.h"
#include "ee/EEAIAStarBoundedStorage.h"
#include "ee/EEAIAStarGraphStorage.h"
#include "ee/EEAIAStarGoalBase.h"
#include "ee/EEAIAStarGraphGoalBase.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIAStarPathfindingGoalBase.h"
#include "ee/EEAIAStarBoundedPathGoal.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEAIAStarPathGraphGoal.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIPathGoalModifiers.h"
#include "ee/EEUnitUtilities.h"

// 0x248e06 (cdecl): the tile inDistance steps from (inX, inY) in direction inDirection (0..7) (provisional name)

// .rdata 0x439c80 / 0x439c84 (next to the vtables): the land planner's influence scale and bridge cost
static const float kEEGPGInfluenceScale = 1.85f;
static const float kEEGPGBridgeCost = 800.0f;

// Whether the step (inFromX, inFromY) -> (inX, inY) is not diagonal (inline; the wall modifiers' GetCost 0x10814
// materialises it as an int: push 1; pop eax) (provisional name)
inline BOOL EEIsStraightStep(long inFromX, long inFromY, long inX, long inY)
{
    return inFromX == inX || inFromY == inY;
}

// Returns a sea (inSea) or space walker's step cost (ee_astar modifiers::step_cost over a quiet view of the world map)
long EEAIPathGoalModifierBase::GetStepCost(bool inSea, long inFromX, long inFromY, long inX, long inY, long inParentX,
                                       long inParentY)
{
    EEAIAStarView theView(gWorld.mMap, true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    EeAStarStepGoal theGoal;
    theGoal.walker = this->mGoal->mUnit;
    theGoal.avoid_units = this->mGoal->mAvoidUnits;
    theGoal.is_bounded = this->mGoal->mIsBounded;
    theGoal.goal_bounds = EEAIAStarRectOf(this->mGoal->mGoalBounds);
    return ee_astar_mod_step_cost(&theCallbacks, &theGoal, inSea, inFromX, inFromY, inX, inY, inParentX, inParentY);
}

// Returns whether the walker (inGoal's unit, else inBoundedGoal's) may enter a tile (ee_astar modifiers::is_passable)
bool EEAIPathGoalModifierBase::IsPassable(EEAIAStarPathfindingGoalBase* inGoal, EEAIAStarBoundedPathGoal* inBoundedGoal,
                                       long inX, long inY, bool inAtGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_is_passable(&theCallbacks, inGoal != NULL ? inGoal->mUnit : inBoundedGoal->mUnit, inX, inY);
}

// (0x40fa75 IsSeaBlocker and 0x40fb08 IsLandBlocker: EEAIAStarView::HasStepBlocker, with the walker as an argument)

// Creates a bounded-goal modifier attached to the given bounded path goal
EEAIBoundedPathGoalModifierBase::EEAIBoundedPathGoalModifierBase(EEAIAStarBoundedPathGoal* inGoal)
    : EEAIPathGoalModifierBase(inGoal)
{
    this->mBoundedGoal = inGoal;
}

// Attaches the modifier to a bounded path goal (also as its plain goal)
void EEAIBoundedPathGoalModifierBase::SetGoal(EEAIAStarBoundedPathGoal* inGoal)
{
    this->mBoundedGoal = inGoal;
    this->mGoal = this->mBoundedGoal;
}

// Creates the space walker's modifier for a bounded path goal
EEAIBPGModifierSpace::EEAIBPGModifierSpace(EEAIAStarBoundedPathGoal* inGoal)
    : EEAIBoundedPathGoalModifierBase(inGoal)
{
}

// Creates the sea walker's modifier for a bounded path goal
EEAIBPGModifierSea::EEAIBPGModifierSea(EEAIAStarBoundedPathGoal* inGoal)
    : EEAIBoundedPathGoalModifierBase(inGoal)
{
}

// Creates the modifier that ends a search within the unit's weapon range of the target, referencing both units
EEAIBPGModifierEnemyIsAttackableByUnit::EEAIBPGModifierEnemyIsAttackableByUnit(EEComplexUnit* inUnit,
                                                                               EEComplexUnit* inTarget)
    : mUnit(inUnit)
{
    inUnit->Reference();
    this->mTarget = inTarget;
    inTarget->Reference();
    this->mRangeSquared = UMSquare(inUnit->mStats->GetRange());
}

// Destroys the modifier, releasing its references to the unit and the target
EEAIBPGModifierEnemyIsAttackableByUnit::~EEAIBPGModifierEnemyIsAttackableByUnit()
{
    this->mUnit->Dereference();
    this->mTarget->Dereference();
}

// Returns whether a node lies within weapon range of the goal's destination; if so, moves the destination and
// the goal tile onto the node
bool EEAIBPGModifierEnemyIsAttackableByUnit::CheckNode(EEAIAStarNode* inNode)
{
    U2DPoint<long> thePoint;
    bool theResult = inNode == NULL;
    if (!theResult) {
        thePoint = U2DPoint<long>(inNode->mX, inNode->mY);
        if (thePoint.DistanceToSquaredF(this->mBoundedGoal->mDestination) <= this->mRangeSquared) {
            this->mBoundedGoal->mDestination = thePoint;
            this->mBoundedGoal->mGoalTile = thePoint;
            theResult = true;
        }
    }
    return theResult;
}

// Returns a flat step cost of 10
long EEAIBPGModifierEnemyIsAttackableByUnit::GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX,
                                                       long inParentY)
{
    return 10;
}

// Returns whether the unit may enter a tile (ee_astar modifiers::attacker_can_enter)
bool EEAIBPGModifierEnemyIsAttackableByUnit::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_attacker_can_enter(&theCallbacks, this->mUnit, inX, inY);
}

// Creates the modifier that lets a walker path through walls
EEAIBPGModifierWallsDoNotBlock::EEAIBPGModifierWallsDoNotBlock()
{
}

// +0x04 (no address: ICF-folded into EEAIGPGModifierComputerPlayerWallPather's identical 0x10814)
long EEAIBPGModifierWallsDoNotBlock::GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX,
                                               long inParentY)
{
    return EEIsStraightStep(inFromX, inFromY, inX, inY) ? 10 : 14;
}

// Returns whether the walker may enter a tile, walls passable (ee_astar modifiers::walls_can_enter)
bool EEAIBPGModifierWallsDoNotBlock::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_walls_can_enter(&theCallbacks, this->mBoundedGoal->mUnit, inX, inY);
}

// Creates an influence-cost modifier for the player against an enemy, with the given weights
EEAIGPGModiferUseInfluenceCostsBase::EEAIGPGModiferUseInfluenceCostsBase(unsigned long inPlayerID,
                                                                         unsigned long inEnemyID,
                                                                         const EECPASGWeights& inWeights)
    : mWeights(inWeights), mPlayerID(inPlayerID), mEnemyID(inEnemyID)
{
}

// Creates an influence-cost modifier for the player against an enemy, with zero weights
EEAIGPGModiferUseInfluenceCostsBase::EEAIGPGModiferUseInfluenceCostsBase(unsigned long inPlayerID,
                                                                         unsigned long inEnemyID)
    : mWeights(0.0f), mPlayerID(inPlayerID), mEnemyID(inEnemyID)
{
}

// Creates the enemy-avoidance route modifier for the player
EEAIGPModifierEnemyAvoidance::EEAIGPModifierEnemyAvoidance(unsigned long inPlayerID)
    : EEAIGPGModiferUseInfluenceCostsBase(inPlayerID, 0)
{
}

/// NEAR: the original calls std::pair<long, int>'s ctor 0xaa817 out of line (ours expands it: the Dinkum depth family)
// Returns a sector step cost: 1 inside one sector, else 10/14; +350 when enemies have influence within one region
long EEAIGPModifierEnemyAvoidance::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    U2DRectangle<long> theRect;
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    std::pair<long, int> theRegion(inTo->mLocation.GetX() / theTable->mRegionSize,
                                   inTo->mLocation.GetY() / theTable->mRegionSize);
    EEAIPathSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject)->mSector;
    long theCost;
    if (theFrom == inTo->mSector)
        theCost = 1;
    else if (theFrom->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mCenter.mY == inTo->mSector->mCenter.mY)
        theCost = 10;
    else
        theCost = 14;
    theRect.mLeft = theRegion.first - 1;
    theRect.mTop = theRegion.second - 1;
    theRect.mRight = theRegion.first + 1;
    theRect.mBottom = theRegion.second + 1;
    if (theTable->SumEnemyValue(this->mPlayerID, theRect, true) > 0)
        theCost += 350;
    if (theCost <= 1)
        theCost = 1;
    return theCost;
}

// +0x08 (no address: ICF-folded into EEAIGPGModifierSpaceRoutePlanner's identical 0x10aa1)
bool EEAIGPModifierEnemyAvoidance::CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
{
    bool theResult = this->mGoal->mPathFlags.mFlags[inSector->mKind];
    if (!theResult) {
        if (inSector->mKind != kAITileGate)
            return theResult;
        theResult = inSector->IsPassable(this->mPlayerID);
    }
    if (theResult && inFrom != NULL && inSector->mLinks.size() > 0)
        theResult = this->mGoal->HasPassableLink(inSector, inFrom);
    return theResult;
}

// Creates the computer player's wall route planner for the player against an enemy
EEAIGPGModifierComputerPlayerWallPlanner::EEAIGPGModifierComputerPlayerWallPlanner(unsigned long inPlayerID,
                                                                                   long inEnemyID)
    : EEAIGPGModiferUseInfluenceCostsBase(inPlayerID, inEnemyID)
{
}

// Returns whether the player may pass the gate sector: true when its gate is missing or destroyed, or is the
// player's or an ally's unlocked main part
bool EEAIGPGModifierComputerPlayerWallPlanner::IsGateOpen(EEAIPathSubSector* inSector, unsigned long inPlayerID)
{
    U2DSparseArrayPointContainer& theTiles = inSector->mTiles;
    bool theBlocked = false;
    EEUnitClass* theGateClass = NULL;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayerID];
    EEComplexUnit* theGate = NULL;
    for (long i = theTiles.Size() - 1; i >= 0 && !theBlocked; i--) {
        EETile* theMapTile = gWorld.GetMap()->GetTileAtLocation(theTiles[i]);
        if (!theMapTile)
            continue;
        for (RTSTileUnitNode* theNode = theMapTile->mUnits; theNode; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (!theUnit || theUnit->GetUnitClass()->mCategory != kUnitCategoryGate)
                continue;
            theGate = theUnit->mComplexUnit;
            theGateClass = EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theGate->GetUnitClass());
            break;
        }
        if (!theGate || theGate->mHitPoints <= 0) {
            theBlocked = true;
            break;
        }
        if ((thePlayer == theGate->mPlayer || theGate->mPlayer->mEnemies[thePlayer->mPlayerID] == 0) &&
            !theGateClass->IsSidePart(theGate) && !theGate->mIsLocked)
            theBlocked = true;
        else
            theBlocked = false;
    }
    return theBlocked;
}

// Returns whether a gate part is an end part (last, with a part before it) or the middle part
// (EEUCGate::IsSidePart: out of line after its first user Virtual10)
bool EEUnitClass::IsSidePart(EEComplexUnit* inGate)
{
    return inGate->mNextPart == NULL && inGate->mPreviousPart != NULL || this->IsMiddlePart(inGate);
}

// Returns whether a gate part is second to last with a part before it (the middle of a three-part gate)
bool EEUnitClass::IsMiddlePart(EEComplexUnit* inGate)
{
    return inGate->mNextPart != NULL && inGate->mNextPart->mNextPart == NULL && inGate->mPreviousPart != NULL;
}

// Returns a sector step cost: 1 inside one sector, else 10 straight or 14 diagonal
// (the path graph goal's default step cost is ICF-folded into it)
long EEAIGPGModifierComputerPlayerWallPlanner::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    EEAIPathSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject)->mSector;
    EEAIPathSector* theTo = inTo->mSector;
    if (theFrom != theTo)
        return theFrom->mCenter.mX == theTo->mCenter.mX || theFrom->mCenter.mY == theTo->mCenter.mY ? 10 : 14;
    return 1;
}

// Returns whether the planner may enter a sector: a passable kind or a gate open to the player, then a passable
// link from inFrom
bool EEAIGPGModifierComputerPlayerWallPlanner::CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
{
    bool theResult = this->mGoal->mPathFlags.mFlags[inSector->mKind];
    if (!theResult) {
        if (inSector->mKind != kAITileGate)
            return theResult;
        theResult = this->IsGateOpen(inSector, this->mPlayerID);
    }
    if (theResult && inFrom != NULL && inSector->mLinks.size() > 0)
        theResult = this->mGoal->HasPassableLink(inSector, inFrom);
    return theResult;
}

// The wall pather (ee/EEAIBoundedPathGoalModifierBase.h); its +0x04 is also EEAIBPGModifierWallsDoNotBlock's
// Returns 10 for a straight step, 14 for a diagonal one
long EEAIGPGModifierComputerPlayerWallPather::GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX,
                                                        long inParentY)
{
    return EEIsStraightStep(inFromX, inFromY, inX, inY) ? 10 : 14;
}

// Returns whether the wall pather may enter a tile, trees passable (ee_astar modifiers::wall_pather_can_enter)
bool EEAIGPGModifierComputerPlayerWallPather::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_wall_pather_can_enter(&theCallbacks, this->mBoundedGoal->mUnit, inX, inY);
}
