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
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEAIAStarPathGraphGoal.h"
#include "ee/EEAIPathSector.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
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

// FUNCTION: EXE 0x40f53b
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

// FUNCTION: EXE 0x40f9bb
// Returns whether the walker (inGoal's unit, else inBoundedGoal's) may enter a tile (ee_astar modifiers::is_passable)
bool EEAIPathGoalModifierBase::IsPassable(EEAIAStarPathfindingGoalBase* inGoal, EEAIAStarBoundedPathGoal* inBoundedGoal,
                                       long inX, long inY, bool inAtGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_is_passable(&theCallbacks, inGoal != NULL ? inGoal->mUnit : inBoundedGoal->mUnit, inX, inY);
}

// (0x40fa75 IsSeaBlocker and 0x40fb08 IsLandBlocker: EEAIAStarView::HasStepBlocker, with the walker as an argument)

// FUNCTION: EXE 0x40fb57
// Creates a bounded-goal modifier attached to the given bounded path goal
EEAIBoundedPathGoalModifierBase::EEAIBoundedPathGoalModifierBase(EEAIAStarBoundedPathGoal* inGoal)
    : EEAIPathGoalModifierBase(inGoal)
{
    this->mBoundedGoal = inGoal;
}

// FUNCTION: EXE 0x40fb80
// Attaches the modifier to a bounded path goal (also as its plain goal)
void EEAIBoundedPathGoalModifierBase::SetGoal(EEAIAStarBoundedPathGoal* inGoal)
{
    this->mBoundedGoal = inGoal;
    this->mGoal = this->mBoundedGoal;
}

// FUNCTION: EXE 0x40fb98
// Creates the space walker's modifier for a bounded path goal
EEAIBPGModifierSpace::EEAIBPGModifierSpace(EEAIAStarBoundedPathGoal* inGoal)
    : EEAIBoundedPathGoalModifierBase(inGoal)
{
}

// FUNCTION: EXE 0x40fbb0
// Creates the sea walker's modifier for a bounded path goal
EEAIBPGModifierSea::EEAIBPGModifierSea(EEAIAStarBoundedPathGoal* inGoal)
    : EEAIBoundedPathGoalModifierBase(inGoal)
{
}

// SYNTHETIC: EXE 0x40fbc8
// (the scalar deleting destructor)

// FUNCTION: EXE 0x40fbe4
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

// FUNCTION: EXE 0x40fc3e
// Destroys the modifier, releasing its references to the unit and the target
EEAIBPGModifierEnemyIsAttackableByUnit::~EEAIBPGModifierEnemyIsAttackableByUnit()
{
    this->mUnit->Dereference();
    this->mTarget->Dereference();
}

// FUNCTION: EXE 0x40fc6e
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

// FUNCTION: EXE 0x40fcdf
// Returns a flat step cost of 10
long EEAIBPGModifierEnemyIsAttackableByUnit::GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX,
                                                       long inParentY)
{
    return 10;
}

// FUNCTION: EXE 0x40fce5
// Returns whether the unit may enter a tile (ee_astar modifiers::attacker_can_enter)
bool EEAIBPGModifierEnemyIsAttackableByUnit::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_attacker_can_enter(&theCallbacks, this->mUnit, inX, inY);
}

// FUNCTION: EXE 0x40fdab
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

// FUNCTION: EXE 0x40fdc3
// Returns whether the walker may enter a tile, walls passable (ee_astar modifiers::walls_can_enter)
bool EEAIBPGModifierWallsDoNotBlock::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_walls_can_enter(&theCallbacks, this->mBoundedGoal->mUnit, inX, inY);
}

// FUNCTION: EXE 0x40fe55
// Creates an influence-cost modifier for the player against an enemy, with the given weights
EEAIGPGModiferUseInfluenceCostsBase::EEAIGPGModiferUseInfluenceCostsBase(unsigned long inPlayerID,
                                                                         unsigned long inEnemyID,
                                                                         const EECPASGWeights& inWeights)
    : mWeights(inWeights), mPlayerID(inPlayerID), mEnemyID(inEnemyID)
{
}

// FUNCTION: EXE 0x40fe8f
// Creates an influence-cost modifier for the player against an enemy, with zero weights
EEAIGPGModiferUseInfluenceCostsBase::EEAIGPGModiferUseInfluenceCostsBase(unsigned long inPlayerID,
                                                                         unsigned long inEnemyID)
    : mWeights(0.0f), mPlayerID(inPlayerID), mEnemyID(inEnemyID)
{
}

// FUNCTION: EXE 0x40fec1
// Returns 1 plus the influence in the target sector's region: enemy influence against the player (inEnemies),
// else the enemy player's own
unsigned long EEAIGPGModiferUseInfluenceCostsBase::GetInfluenceCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo,
                                                             bool inEnemies)
{
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    U2DPoint<long> theRegion(inTo->mLocation.GetX() / theTable->mRegionSize, inTo->mLocation.GetY() / theTable->mRegionSize);
    unsigned long theInfluence;
    if (inEnemies)
        theInfluence = theTable->GetEnemyValue(this->mPlayerID, theRegion, true);
    else
        theInfluence = theTable->GetValue(this->mEnemyID, theRegion);
    return theInfluence + 1;
}

// TEMPLATE: EXE 0x40ff19
// EEInfluenceTable::GetValue(unsigned long, const U2DPoint<long>&) (inline, after its first user)

// FUNCTION: EXE 0x40ff38
// Creates the enemy-avoidance route modifier for the player
EEAIGPModifierEnemyAvoidance::EEAIGPModifierEnemyAvoidance(unsigned long inPlayerID)
    : EEAIGPGModiferUseInfluenceCostsBase(inPlayerID, 0)
{
}

/// NEAR: the original calls std::pair<long, int>'s ctor 0xaa817 out of line (ours expands it: the Dinkum depth family)
// FUNCTION: EXE 0x40ff52
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

// FUNCTION: EXE 0x41001a
// Creates the land route planner for an attack subgroup, its two continents and weights; it avoids friendly walls
EEAIGPGModifierLandAttackSubGroupRoutePlanner::EEAIGPGModifierLandAttackSubGroupRoutePlanner(
    EECPAttackSubGroup* inSubGroup, unsigned short inContinent1, unsigned short inContinent2, bool inAnyContinent,
    const EECPASGWeights& inWeights)
    : EEAIGPGModiferUseInfluenceCostsBase(inSubGroup->mPlayer2->GetPlayerID(), inSubGroup->mTask.GetPlayerID(), inWeights),
      mSubGroup(inSubGroup), mContinent1(inContinent1), mContinent2(inContinent2), mAnyContinent(inAnyContinent)
{
    this->mAvoidFriendlyBlockers = true;
}

/// NEAR: the original reaches theLinks[i] through the out-of-line _Ptrit::operator+ 0x13dc48 in the first tests (Dinkum depth family)
// FUNCTION: EXE 0x410074
// Returns a land attack step cost: 10/14 plus weighted wall (750, 3750 if friendly), gate (800) and influence costs,
// scaled by terrain weights; x10 into a wall from water or space, x50 on a continent flagged against the first
long EEAIGPGModifierLandAttackSubGroupRoutePlanner::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theKind = inTo->mKind;
    long theCost = theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY ?
        10 : 14;
    long thePenalty;
    if (theKind == kAITileWall) {
        thePenalty = 750;
        EEPlayer* thePlayer = gWorld.mPlayers[inTo->mPlayerID];
        if (thePlayer != NULL && thePlayer->mPlayerID != 0 &&
            (thePlayer == this->mSubGroup->mPlayer2 || this->mSubGroup->mPlayer2->mEnemies[thePlayer->mPlayerID] == 0))
            thePenalty = 3750;
        if (this->mWeights.mValues[3] != 1.0f)
            theCost += UMFloatToLong(thePenalty * this->mWeights.mValues[3]);
        else
            theCost += thePenalty;
    } else if (theKind == kAITileGate) {
        if (this->mWeights.mValues[2] != 1.0f)
            theCost += UMFloatToLong(this->mWeights.mValues[2] * 800.0f);
        else
            theCost += 800;
    }
    float theInfluence = (float)this->GetInfluenceCost(inFrom, inTo, false);
    if (theInfluence != 0.0f)
        theCost += UMFloatToLong(this->mWeights.mValues[4] * (theInfluence * 1.85f));

    inTo->GetLinks(theFrom->mID, theLinks);
    for (long i = theLinks.size() - 1; i >= 0; i--) {
        if (theLinks[i].first->mKind == 13 || theLinks[i].second->mKind == 13) {
            thePenalty = 750;
            EEPlayer* thePlayer = gWorld.mPlayers[theLinks[i].first->mPlayerID];
            if (thePlayer == NULL || thePlayer->mPlayerID == 0)
                thePlayer = gWorld.mPlayers[theLinks[i].second->mPlayerID];
            if (thePlayer != NULL && thePlayer->mPlayerID != 0 &&
                (thePlayer == this->mSubGroup->mPlayer2 || this->mSubGroup->mPlayer2->mEnemies[thePlayer->mPlayerID] == 0))
                thePenalty = 3750;
            if (this->mWeights.mValues[3] != 1.0f)
                theCost += UMFloatToLong(thePenalty * this->mWeights.mValues[3]);
            else
                theCost += thePenalty;
            break;
        }
        if (theLinks[i].first->mKind != 14 && theLinks[i].second->mKind != 14)
            continue;
        if (this->mWeights.mValues[2] != 1.0f)
            theCost += UMFloatToLong(this->mWeights.mValues[2] * 800.0f);
        else
            theCost += 800;
        break;
    }

    switch (theKind) {
    case kAITileWater:
    case kAITileShallows:
    case kAITileShallowsShore:
    case kAITileShallowsNearShore:
    case kAITileShore:
    case kAITileNearShore:
        if (this->mWeights.mValues[1] != 1.0f)
            theCost = UMFloatToLong(theCost * this->mWeights.mValues[1]);
        break;
    case kAITileCliff:
        if (this->mWeights.mValues[5] != 1.0f)
            theCost = UMFloatToLong(theCost * this->mWeights.mValues[5]);
        break;
    case kAITileForest:
        if (this->mWeights.mValues[0] != 1.0f)
            theCost = UMFloatToLong(theCost * this->mWeights.mValues[0]);
        break;
    case kAITileWall:
        if (this->mGoal->mFrom->mID != theFrom->mID) {
            long theFromKind = theFrom->mKind;
            if (theFromKind == kAITileWater || (theFromKind > kAITileMapEdge && theFromKind <= kAITileNearShore) || theFromKind == kAITileSpace)
                theCost *= 10;
        }
        break;
    case kAITileSpace:
        if (this->mWeights.mValues[10] != 1.0f)
            theCost = UMFloatToLong(theCost * this->mWeights.mValues[10]);
        break;
    }

    if (!this->mAnyContinent) {
        unsigned short theContinent = gContinentManager.GetContinentID(inTo->mLocation.GetX(), inTo->mLocation.GetY());
        if (theContinent != this->mContinent2 && theContinent != this->mContinent1 &&
            gContinentManager.mTileFlags[this->mContinent1 * gContinentManager.mFlagsWidth + theContinent])
            theCost *= 50;
    }
    if (theCost <= 1)
        theCost = 1;
    return theCost;
}

/// NEAR: the result byte lives in inSector's dead slot; RTSMap::GetTile 0x2f85 out of line twice
// FUNCTION: EXE 0x410465
// Returns whether the planner may enter a sector: a passable kind or open gate, no friendly wall, a passable link
// from inFrom, and water to water only across a water-type link when the water weight is set
bool EEAIGPGModifierLandAttackSubGroupRoutePlanner::CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
{
    bool theResult = this->mGoal->mPathFlags.mFlags[inSector->mKind];
    if (!theResult) {
        if (inSector->mKind != kAITileGate)
            return theResult;
        theResult = inSector->IsPassable(this->mPlayerID);
    }
    if (theResult == true && inSector->mKind == kAITileWall && this->mAvoidFriendlyBlockers) {
        EEComplexUnit* theUnit = inSector->GetBlockingUnit();
        if (theUnit != NULL && (theUnit->mPlayer == this->mSubGroup->mPlayer2 ||
                                this->mSubGroup->mPlayer2->mEnemies[theUnit->mPlayer->mPlayerID] == 0)) {
            theResult = false;
            return theResult;
        }
        theResult = true;
    }
    if (!theResult)
        return theResult;
    if (inFrom != NULL && inSector->mLinks.size() > 0) {
        theResult = this->mGoal->HasPassableLink(inSector, inFrom);
        if (theResult && this->mWeights.mValues[1] != 0.0f) {
            EEMap* theMap = gWorld.mMap;
            if (theMap->GetTile(inSector->mLocation.GetX(), inSector->mLocation.GetY()) != NULL) {
                long theType = theMap->GetTile(inSector->mLocation.GetX(), inSector->mLocation.GetY())->mTerrainType;
                if (theType == kTerrainDeepWater || theType == kTerrainShallowWater) {
                    if (theMap->GetTileAtLocation(inFrom->mLocation) != NULL) {
                        theType = theMap->GetTileAtLocation(inFrom->mLocation)->mTerrainType;
                        if (theType == kTerrainDeepWater || theType == kTerrainShallowWater) {
                            std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
                            inSector->GetLinks(inFrom->mID, theLinks);
                            theResult = theLinks.size() == 0;
                            for (long i = theLinks.size() - 1; i >= 0 && !theResult; i--) {
                                long theLinkKind = theLinks[i].first->mKind;
                                if (theLinkKind == 2 || (theLinkKind > 6 && theLinkKind <= 11) || theLinkKind == 15)
                                    theResult = true;
                                else {
                                    theLinkKind = theLinks[i].second->mKind;
                                    if (theLinkKind == 2 || (theLinkKind > 6 && theLinkKind <= 11) ||
                                        theLinkKind == 15)
                                        theResult = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x410639
// Creates the computer player's wall route planner for the player against an enemy
EEAIGPGModifierComputerPlayerWallPlanner::EEAIGPGModifierComputerPlayerWallPlanner(unsigned long inPlayerID,
                                                                                   long inEnemyID)
    : EEAIGPGModiferUseInfluenceCostsBase(inPlayerID, inEnemyID)
{
}

// FUNCTION: EXE 0x410655
// Returns whether the player may pass the gate sector: true when its gate is missing or destroyed, or is the
// player's or an ally's unlocked main part
bool EEAIGPGModifierComputerPlayerWallPlanner::IsGateOpen(EEAIPathSubSector* inSector, unsigned long inPlayerID)
{
    U2DSparseArrayPointContainer& theTiles = inSector->mTiles;
    bool theBlocked = false;
    EEUCGate* theGateClass = NULL;
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
            theGateClass = dynamic_cast<EEUCGate*>(theGate->GetUnitClass());
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

// FUNCTION: EXE 0x410745
// Returns whether a gate part is an end part (last, with a part before it) or the middle part
// (EEUCGate::IsSidePart: out of line after its first user Virtual10)
bool EEUCGate::IsSidePart(EEComplexUnit* inGate)
{
    return inGate->mNextPart == NULL && inGate->mPreviousPart != NULL || this->IsMiddlePart(inGate);
}

// FUNCTION: EXE 0x41076f
// Returns whether a gate part is second to last with a part before it (the middle of a three-part gate)
bool EEUCGate::IsMiddlePart(EEComplexUnit* inGate)
{
    return inGate->mNextPart != NULL && inGate->mNextPart->mNextPart == NULL && inGate->mPreviousPart != NULL;
}

// FUNCTION: EXE 0x410799
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

// FUNCTION: EXE 0x4107cb
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
// FUNCTION: EXE 0x410814
// Returns 10 for a straight step, 14 for a diagonal one
long EEAIGPGModifierComputerPlayerWallPather::GetCost(long inFromX, long inFromY, long inX, long inY, long inParentX,
                                                        long inParentY)
{
    return EEIsStraightStep(inFromX, inFromY, inX, inY) ? 10 : 14;
}

// FUNCTION: EXE 0x41083c
// Returns whether the wall pather may enter a tile, trees passable (ee_astar modifiers::wall_pather_can_enter)
bool EEAIGPGModifierComputerPlayerWallPather::CanEnter(EETile* inTile, long inX, long inY, bool inGoal)
{
    EEAIAStarView theView(gWorld.GetMap(), true);
    EeAStarViewCallbacks theCallbacks = theView.Callbacks();
    return ee_astar_mod_wall_pather_can_enter(&theCallbacks, this->mBoundedGoal->mUnit, inX, inY);
}

// FUNCTION: EXE 0x4108d6
// Creates the sea route planner for an attack subgroup with the given weights
EEAIGPGModifierSeaRoutePlanner::EEAIGPGModifierSeaRoutePlanner(unsigned long inPlayerID, EECPAttackSubGroup* inSubGroup,
                                                               const EECPASGWeights& inWeights)
    : EEAIGPGModiferUseInfluenceCostsBase(inPlayerID, inSubGroup->mTask.GetPlayerID(), inWeights), mSubGroup(inSubGroup)
{
}

// +0x04 (no address: ICF-folded into EEAIGPGModifierSpaceRoutePlanner's identical 0x10a20)
long EEAIGPGModifierSeaRoutePlanner::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theCost = theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY ?
        10 : 14;
    unsigned long theInfluence = this->GetInfluenceCost(inFrom, inTo, false);
    if (theInfluence != 0)
        theCost += UMFloatToLong(theInfluence * this->mWeights.mValues[4]);
    if (theCost <= 1)
        theCost = 1;
    return theCost;
}

// +0x08 (no address: ICF-folded into EEAIGPGModifierSpaceRoutePlanner's identical 0x10aa1)
bool EEAIGPGModifierSeaRoutePlanner::CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
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

// FUNCTION: EXE 0x410909
// Creates the helicopter route planner for an attack subgroup with the given weights
EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner::EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner(
    EECPAttackSubGroup* inSubGroup, const EECPASGWeights& inWeights)
    : EEAIGPGModiferUseInfluenceCostsBase(inSubGroup->mPlayer2->GetPlayerID(), inSubGroup->mTask.GetPlayerID(), inWeights),
      mSubGroup(inSubGroup)
{
}

// +0x04 (no address: ICF-folded into EEAIGPGModifierSpaceRoutePlanner's identical 0x10a20)
long EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theCost = theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY ?
        10 : 14;
    unsigned long theInfluence = this->GetInfluenceCost(inFrom, inTo, false);
    if (theInfluence != 0)
        theCost += UMFloatToLong(theInfluence * this->mWeights.mValues[4]);
    if (theCost <= 1)
        theCost = 1;
    return theCost;
}

// +0x08 (no address: ICF-folded into EEAIGPGModifierSpaceRoutePlanner's identical 0x10aa1)
bool EEAIGPGModifierHelicopterAttackSubGroupRoutePlanner::CanEnter(EEAIPathSubSector* inSector,
                                                                   EEAIPathSubSector* inFrom)
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

// FUNCTION: EXE 0x410947
// Creates the airplane route planner for an attack subgroup with the given weights
EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner::EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner(
    EECPAttackSubGroup* inSubGroup, const EECPASGWeights& inWeights)
    : EEAIGPGModiferUseInfluenceCostsBase(inSubGroup->mPlayer2->GetPlayerID(), inSubGroup->mTask.GetPlayerID(), inWeights),
      mSubGroup(inSubGroup)
{
}

// FUNCTION: EXE 0x410985
// Returns a sector step cost: 10 straight or 14 diagonal plus the weighted enemy influence; at least 1
long EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theCost = theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY ?
        10 : 14;
    unsigned long theInfluence = this->GetInfluenceCost(inFrom, inTo, false);
    if (theInfluence != 0)
        theCost += UMFloatToLong(theInfluence * this->mWeights.mValues[4]);
    if (theCost <= 1)
        theCost = 1;
    return theCost;
}

// +0x08 (no address: ICF-folded into EEAIGPGModifierSpaceRoutePlanner's identical 0x10aa1)
bool EEAIGPGModifierAirplaneAttackSubGroupRoutePlanner::CanEnter(EEAIPathSubSector* inSector,
                                                                 EEAIPathSubSector* inFrom)
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

// FUNCTION: EXE 0x4109ed
// Creates the space route planner for an attack subgroup with the given weights
EEAIGPGModifierSpaceRoutePlanner::EEAIGPGModifierSpaceRoutePlanner(unsigned long inPlayerID,
                                                                   EECPAttackSubGroup* inSubGroup,
                                                                   const EECPASGWeights& inWeights)
    : EEAIGPGModiferUseInfluenceCostsBase(inPlayerID, inSubGroup->mTask.GetPlayerID(), inWeights), mSubGroup(inSubGroup)
{
}

// FUNCTION: EXE 0x410a20
// Returns a sector step cost: 10 straight or 14 diagonal plus the weighted enemy influence; at least 1
long EEAIGPGModifierSpaceRoutePlanner::GetCost(EEAIAStarGraphNode* inFrom, EEAIPathSubSector* inTo)
{
    std::vector<std::pair<EEAIPathSubSector*, EEAIPathSubSector*> > theLinks;
    EEAIPathSubSector* theFrom = static_cast<EEAIPathSubSector*>(inFrom->mObject);
    long theCost = theFrom->mSector->mCenter.mX == inTo->mSector->mCenter.mX || theFrom->mSector->mCenter.mY == inTo->mSector->mCenter.mY ?
        10 : 14;
    unsigned long theInfluence = this->GetInfluenceCost(inFrom, inTo, false);
    if (theInfluence != 0)
        theCost += UMFloatToLong(theInfluence * this->mWeights.mValues[4]);
    if (theCost <= 1)
        theCost = 1;
    return theCost;
}

// FUNCTION: EXE 0x410aa1
// Returns whether the planner may enter a sector: a passable kind or a gate open to the player, then a passable
// link from inFrom
bool EEAIGPGModifierSpaceRoutePlanner::CanEnter(EEAIPathSubSector* inSector, EEAIPathSubSector* inFrom)
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

// The inline COMDATs of their first users: RTSRegionTable::GetRegion(const U2DPoint<long>&) (GetStepCost), then the
// bases' inline slots (EEAIPathGoalModifierBase +0x04/+0x08/??_G, EEAIGraphPathGoalModifierBase +0x04/+0x08/??_G,
// EEAIBoundedPathGoalModifierBase +0x0c, EEAIBPGModifierSpace/Sea +0x04/+0x08 ICF-shared)
// TEMPLATE: EXE 0x410ae9
// TEMPLATE: EXE 0x410b12
// TEMPLATE: EXE 0x410b18
// TEMPLATE: EXE 0x410b24
// TEMPLATE: EXE 0x410b48
// TEMPLATE: EXE 0x410b4e
// TEMPLATE: EXE 0x410b5a
// TEMPLATE: EXE 0x410b7e
// TEMPLATE: EXE 0x410b8a
// TEMPLATE: EXE 0x410baa
