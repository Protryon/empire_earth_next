// New (port): the tile A* searches' view of the world (ee/EEAIAStarView.h). Each query is the expression the goals
// evaluated in place before (EEAIAStarPathfindingGoal.cpp, EEAIAStarBoundedPathGoal.cpp, the flood goals), moved here
// unchanged.
#include <windows.h>
#include <string.h>
#include <vector>
#include <map>
#include <set>
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
#include "lle/U2DRectangle.h"

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
#include "ee/EEAIAStarPathAreaStorage.h"
#include "ee/EEPlayer.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIAStarView.h"
#include "ee/EEAIAStarCapture.h"

// EEAIContinentManager.cpp's (out of line at 0x29ced): non-zero when a resource unit stands on inTile
long EEAIHasResourceUnit(EETile* inTile);

// the path goals' full hit point fraction (.rdata 0x439ff4 / 0x439b5c)
static const float kEEAStarViewFull = 1.0f;

// The capture's note of a query (ee/EEAIAStarCapture.h; a no-op unless a search is being captured): the query's
// number, the tile, up to two more arguments, the answer
enum {
    kViewHasTile = 1, kViewTerrain, kViewEdge, kViewHasUnits, kViewAIValue, kViewHeightLevel, kViewContinentID,
    kViewContinentAt, kViewTileFlag, kViewTileValue, kViewPathAreaAt, kViewPathArea, kViewStoredPathAreaID,
    kViewChokeTile, kViewResourceKind, kViewHasBlocker, kViewFirstBlockerID, kViewFirstBlocker, kViewFirstObstacleID,
    kViewOwnWallFirst, kViewAreaGateOwner, kViewSectorGateOwner, kViewLiveDock, kViewLiveSpaceDock, kViewCanMoveOn,
    kViewArePassable, kViewCrowdCost, kViewIsWide, kViewFormationRank, kViewIgnoresFormation, kViewRankProbe,
    kViewCanEnter, kViewModifierCost
};

static inline long Note(long inQuery, long inX, long inY, long inAnswer, long inArg = 0, long inArg2 = 0)
{
    return EEAIAStarNote(kAStarEventView, inQuery, inX, inY, inArg, inArg2, inAnswer);
}

// the walker as the capture names it (its unit id)
static inline long WalkerID(EEComplexUnit* inWalker)
{
    return (long)inWalker->GetUnitID();
}

// the area fills' blocker test (EEAIAStarPathAreaGoal / EEAIAStarPathSectorGoal wrote it out in every case)
static inline bool EEAStarIsBlocker(EEUnit* inUnit)
{
    return (inUnit->GetUnitClass()->mBlocksTile || inUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
           !inUnit->IsFoundation() &&
           (inUnit->GetHitPoints() > 0 || inUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource);
}

bool EEAIAStarView::HasTile(long inX, long inY)
{
    return Note(kViewHasTile, inX, inY, this->mMap->GetTile(inX, inY) != NULL);
}

long EEAIAStarView::GetTerrain(long inX, long inY)
{
    return Note(kViewTerrain, inX, inY, this->mMap->GetTile(inX, inY)->mTerrainType);
}

bool EEAIAStarView::IsEdgeTile(long inX, long inY)
{
    return Note(kViewEdge, inX, inY, this->mMap->IsEdgeTile(inX, inY));
}

bool EEAIAStarView::HasUnits(long inX, long inY)
{
    return Note(kViewHasUnits, inX, inY, this->mMap->GetTile(inX, inY)->mUnits != NULL);
}

long EEAIAStarView::GetAIValue(long inX, long inY)
{
    return Note(kViewAIValue, inX, inY, this->mMap->GetTile(inX, inY)->mAIValue);
}

long EEAIAStarView::GetHeightLevel(long inX, long inY)
{
    EEMapTile* theTile = this->mMap->GetTile(inX, inY);
    return Note(kViewHeightLevel, inX, inY, UMFloor(theTile->GetHeight() / (this->mMap->GetF1955F4() * 0.5f) + 0.00001f));
}

unsigned short EEAIAStarView::GetContinentID(long inX, long inY)
{
    return (unsigned short)Note(kViewContinentID, inX, inY, gContinentManager.GetContinentID(inX, inY));
}

EEAIAStarContinentInfo EEAIAStarView::GetContinentAt(long inX, long inY)
{
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(inX, inY);
    EEAIAStarContinentInfo theInfo;
    theInfo.mID = theContinent->mID;
    theInfo.mCategory = theContinent->mCategory;
    Note(kViewContinentAt, inX, inY, theInfo.mID, theInfo.mCategory);
    return theInfo;
}

long EEAIAStarView::GetTileFlag(long inX, long inY)
{
    return Note(kViewTileFlag, inX, inY, gContinentManager.GetTileFlag(inX, inY));
}

long EEAIAStarView::GetTileValue(long inX, long inY)
{
    if (gContinentManager.mTileValues)
        return Note(kViewTileValue, inX, inY, gContinentManager.mTileValues[gContinentManager.mWidth * inX + inY]);
    return Note(kViewTileValue, inX, inY, gContinentManager.GetTileFlag(inX, inY));
}

EEAIAStarPathAreaInfo EEAIAStarView::GetPathAreaAt(long inX, long inY)
{
    EETileLocation theTile(inX, inY);
    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(theTile);
    EEAIAStarPathAreaInfo theInfo;
    theInfo.mFound = theArea != NULL;
    theInfo.mID = theArea ? theArea->mID : 0;
    theInfo.mContinentID = theArea ? theArea->GetContinentID() : 0;
    theInfo.mKind = theArea ? theArea->mKind : 0;
    Note(kViewPathAreaAt, inX, inY, theInfo.mFound, theInfo.mID, theInfo.mContinentID);
    Note(kViewPathAreaAt, inX, inY, theInfo.mKind);
    return theInfo;
}

EEAIAStarPathAreaInfo EEAIAStarView::GetPathArea(unsigned short inID)
{
    EEAIPathArea* theArea = gContinentManager.mPathAreas[inID];
    EEAIAStarPathAreaInfo theInfo;
    theInfo.mFound = true;
    theInfo.mID = theArea->mID;
    theInfo.mContinentID = theArea->GetContinentID();
    theInfo.mKind = theArea->GetKind();
    Note(kViewPathArea, inID, 0, theInfo.mID, theInfo.mContinentID, theInfo.mKind);
    return theInfo;
}

unsigned short EEAIAStarView::GetStoredPathAreaID(const EEAIAStarPathAreaStorage* inStorage, long inX, long inY)
{
    return (unsigned short)Note(kViewStoredPathAreaID, inX, inY, inStorage->mPathAreaIDs[inStorage->GetHeight() * inX + inY]);
}

bool EEAIAStarView::IsChokeTile(long inIndex, unsigned long* outSerial)
{
    if (gEEAIChokeTileFlags[inIndex] && gEEAIChokeTileKinds[inIndex] != 0x36) {
        *outSerial = gEEAIChokeTileSerials[inIndex];
        return Note(kViewChokeTile, inIndex, 0, true, (long)*outSerial);
    }
    *outSerial = 0;
    return Note(kViewChokeTile, inIndex, 0, false);
}

long EEAIAStarView::GetResourceKind(long inX, long inY)
{
    return Note(kViewResourceKind, inX, inY, EEAIHasResourceUnit(this->mMap->GetTile(inX, inY)));
}

bool EEAIAStarView::HasBlocker(long inX, long inY)
{
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        if (EEAStarIsBlocker(theNode->mUnit))
            return Note(kViewHasBlocker, inX, inY, true);
    }
    return Note(kViewHasBlocker, inX, inY, false);
}

long EEAIAStarView::GetFirstBlockerID(long inX, long inY)
{
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        if (EEAStarIsBlocker(theNode->mUnit))
            return Note(kViewFirstBlockerID, inX, inY, theNode->mUnit->GetUnitID());
    }
    return Note(kViewFirstBlockerID, inX, inY, -1);
}

EEAIAStarBlocker EEAIAStarView::GetFirstBlocker(long inX, long inY)
{
    EEAIAStarBlocker theBlocker;
    theBlocker.mFound = false;
    theBlocker.mUnitID = -1;
    theBlocker.mPlayerID = 0;
    theBlocker.mIsGate = false;
    theBlocker.mIsWall = false;
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (!EEAStarIsBlocker(theUnit))
            continue;
        theBlocker.mFound = true;
        theBlocker.mUnitID = theUnit->GetUnitID();
        theBlocker.mPlayerID = theUnit->GetPlayer()->GetPlayerID();
        theBlocker.mIsGate = theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate;
        theBlocker.mIsWall = theUnit->GetUnitClass()->mIsWall;
        break;
    }
    Note(kViewFirstBlocker, inX, inY, theBlocker.mFound, theBlocker.mUnitID, theBlocker.mPlayerID);
    Note(kViewFirstBlocker, inX, inY, theBlocker.mIsGate, theBlocker.mIsWall);
    return theBlocker;
}

long EEAIAStarView::GetFirstObstacleID(long inX, long inY)
{
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if ((theUnit->GetHitPoints() > 0 || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryResource) &&
            ((theUnit->GetUnitClass()->mBlocksTile && !theUnit->IsFoundation()) ||
             theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate))
            return Note(kViewFirstObstacleID, inX, inY, theUnit->GetUnitID());
    }
    return Note(kViewFirstObstacleID, inX, inY, -1);
}

bool EEAIAStarView::HasOwnWallFirst(long inX, long inY, long inPlayerID)
{
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (theUnit->GetUnitClass()->mIsWall && theUnit->GetHitPoints() > 0 &&
            theUnit->GetUnitClass()->GetCategory() != kUnitCategoryGate &&
            theUnit->GetPlayer()->GetPlayerID() == inPlayerID && !theUnit->IsFoundation())
            return Note(kViewOwnWallFirst, inX, inY, true, inPlayerID);
        if (EEAStarIsBlocker(theUnit))
            return Note(kViewOwnWallFirst, inX, inY, false, inPlayerID);
    }
    return Note(kViewOwnWallFirst, inX, inY, false, inPlayerID);
}

long EEAIAStarView::GetAreaGateOwner(long inX, long inY, long inPlayerID)
{
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if (!(theUnit->GetHitPoints() > 0) && theUnit->GetUnitClass()->GetCategory() != kUnitCategoryResource)
            continue;
        if (theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate && theUnit->GetPlayer()->GetPlayerID() == inPlayerID)
            return Note(kViewAreaGateOwner, inX, inY, theUnit->GetUnitID(), inPlayerID);
        if ((theUnit->GetUnitClass()->mBlocksTile || theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate) &&
            !theUnit->IsFoundation())
            return Note(kViewAreaGateOwner, inX, inY, theUnit->GetUnitID(), inPlayerID);
    }
    return Note(kViewAreaGateOwner, inX, inY, -1, inPlayerID);
}

long EEAIAStarView::GetSectorGateOwner(long inX, long inY, long inPlayerID)
{
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        if ((theUnit->GetUnitClass()->GetCategory() == kUnitCategoryGate && theUnit->GetHitPoints() > 0 &&
             theUnit->mPlayer->mPlayerID == inPlayerID) ||
            EEAStarIsBlocker(theUnit))
            return Note(kViewSectorGateOwner, inX, inY, theUnit->GetUnitID(), inPlayerID);
    }
    return Note(kViewSectorGateOwner, inX, inY, -1, inPlayerID);
}

bool EEAIAStarView::HasLiveDock(long inX, long inY)
{
    bool theResult = false;
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode && !theResult; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        theResult = (theUnit->mUnitClass->mUnitType == kUnitTypeDock || theUnit->mUnitClass->mUnitType == kUnitTypeNavyYard) &&
                    theUnit->mHitPoints > 0 && !theUnit->IsFoundation();
    }
    return Note(kViewLiveDock, inX, inY, theResult);
}

bool EEAIAStarView::HasLiveSpaceDock(long inX, long inY)
{
    bool theResult = false;
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode && !theResult; theNode = theNode->mNext) {
        EEUnit* theUnit = theNode->mUnit;
        unsigned long theType = theUnit->mUnitClass->mUnitType;
        theResult = (theType == kUnitTypeSpaceDock || theType == kUnitTypeSpaceTurret || theType == kUnitTypeOrbitalSpaceStation) &&
                    theUnit->mHitPoints > 0 && !theUnit->IsFoundation();
    }
    return Note(kViewLiveSpaceDock, inX, inY, theResult);
}

bool EEAIAStarView::CanMoveOn(EEComplexUnit* inWalker, long inTerrain)
{
    return Note(kViewCanMoveOn, inTerrain, 0, inWalker->GetUnitClass()->CanMoveOnTerrain(inWalker, inTerrain), WalkerID(inWalker));
}

bool EEAIAStarView::ArePassable(EEComplexUnit* inWalker, long inX, long inY)
{
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        if (!theNode->mUnit->IsPassableBy(inWalker))
            return Note(kViewArePassable, inX, inY, false, WalkerID(inWalker));
    }
    return Note(kViewArePassable, inX, inY, true, WalkerID(inWalker));
}

long EEAIAStarView::GetCrowdCost(EEComplexUnit* inWalker, long inX, long inY)
{
    long theCost = 0;
    for (RTSTileUnitNode* theNode = this->mMap->GetTile(inX, inY)->mUnits; theNode; theNode = theNode->mNext) {
        EEComplexUnit* theUnit = theNode->mUnit->mComplexUnit;
        if (!theUnit || theUnit == inWalker || !theUnit->GetUnitClass()->mIsCollidable || theUnit->IsFoundation())
            continue;
        if (theUnit->mActiveGroup && theUnit->mActiveGroup == inWalker->mActiveGroup && theUnit->mCollisionRadius < kEEAStarViewFull)
            continue;
        if ((theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionIdle
            && (theUnit->mCommand ? theUnit->mCommand->mType : 0) != kActionAttackEnemyUnit)
            continue;
        if (theUnit->mCollisionRadius >= kEEAStarViewFull && inWalker->GetUnitClass()->mInnerCollisionSphere < kEEAStarViewFull)
            theCost += 150;
        else
            theCost += 50;
    }
    return Note(kViewCrowdCost, inX, inY, theCost, WalkerID(inWalker));
}

bool EEAIAStarView::IsWide(EEComplexUnit* inWalker)
{
    return Note(kViewIsWide, 0, 0, inWalker->GetUnitClass()->mInnerCollisionSphere >= kEEAStarViewFull, WalkerID(inWalker));
}

long EEAIAStarView::GetFormationRank(EEComplexUnit* inWalker)
{
    return Note(kViewFormationRank, 0, 0, inWalker->mFormationRank, WalkerID(inWalker));
}

bool EEAIAStarView::IgnoresFormation(EEComplexUnit* inWalker)
{
    return Note(kViewIgnoresFormation, 0, 0, (bool)((inWalker->mStats->mTerrainMask >> 14) & 1), WalkerID(inWalker));
}

bool EEAIAStarView::IsRankProbeBlocked(EEComplexUnit* inWalker, unsigned short inContinentID, long inX, long inY)
{
    EEMapTile* theNeighbor = gWorld.mMap->GetTile(inX, inY);
    return Note(kViewRankProbe, inX, inY,
                theNeighbor && (theNeighbor->mAIValue || !inWalker->GetUnitClass()->CanMoveOnTerrain(inWalker, theNeighbor->GetTerrainType())
                                || inContinentID != gContinentManager.GetContinentID(inX, inY)),
                WalkerID(inWalker), inContinentID);
}

bool EEAIAStarView::CanEnter(EEAIPathGoalModifierBase* inModifier, long inX, long inY, bool inGoal)
{
    return Note(kViewCanEnter, inX, inY, inModifier->CanEnter(this->mMap->GetTile(inX, inY), inX, inY, inGoal), inGoal);
}

long EEAIAStarView::GetModifierCost(EEAIPathGoalModifierBase* inModifier, long inFromX, long inFromY, long inX, long inY,
                                    long inParentX, long inParentY)
{
    long theCost = inModifier->GetCost(inFromX, inFromY, inX, inY, inParentX, inParentY);
    Note(kViewModifierCost, inX, inY, theCost, inFromX, inFromY);
    return Note(kViewModifierCost, inX, inY, theCost, inParentX, inParentY);
}
