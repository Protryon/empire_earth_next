// New (port; docs/RUST_PLAN.md R5b.4, rounds 16 and 17): the goals whose class moved whole into Rust
// (ee/EEUGRust.h). Each virtual is one call of the Rust body (ee_units rust_goal.rs, by mKind) over the host below:
// one operation callback whose cases are the reads and world calls the bodies make (the units, the patrol group's
// walk, the calamity classes and manager, the explore goal's map, path and continent queries, the typed file
// calls, the texts). The base goal's own fields, save and checksum stay EEUnitGoal's.
#include <windows.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/U2DPolygon.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EETileLocation.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
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
#include "ee/EEUnitKind.h"
#include "rust/ee_units.h"

namespace {

// where a body's file calls and text go
struct EEGoalContext {
    FSFile* mFile;
    UWideString* mText;
    FILE* mDump;
};

uint64_t Handle(const void* inObject)
{
    return reinterpret_cast<uintptr_t>(inObject);
}

template <class T> T* ObjectOf(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

void PointOut(EeUnitsGoalIo* ioArgs, const GE3DPoint& inPoint)
{
    ioArgs->f[0] = inPoint.GetX();
    ioArgs->f[1] = inPoint.GetY();
    ioArgs->f[2] = inPoint.GetZ();
}

void PointIn(const EeUnitsGoalIo& inArgs, GE3DPoint& outLocation)
{
    outLocation.SetX(inArgs.f[0]);
    outLocation.SetY(inArgs.f[1]);
    outLocation.SetZ(inArgs.f[2]);
}

// one operation (EE_UNITS_GOAL_HOST_*) of a body: inA, inB the handles it names, ioArgs the rest
int64_t GoalOp(void* inContext, int32_t inOp, uint64_t inA, uint64_t inB, EeUnitsGoalIo* ioArgs)
{
    EEGoalContext* theContext = static_cast<EEGoalContext*>(inContext);
    EEUnit* theUnit = ObjectOf<EEUnit>(inA);
    EEComplexUnit* theComplex = ObjectOf<EEComplexUnit>(inA);
    EEPPatrolGroup* theGroup = ObjectOf<EEPPatrolGroup>(inA);
    EECalamityClass* theClass = ObjectOf<EECalamityClass>(inA);
    GE3DPoint thePoint(ioArgs->f[0], ioArgs->f[1], ioArgs->f[2]);
    switch (inOp) {
    case EE_UNITS_GOAL_HOST_REFERENCE:
        if (inB != 0)
            theGroup->Reference();
        else
            theUnit->Reference();
        return 0;
    case EE_UNITS_GOAL_HOST_DEREFERENCE:
        if (inB != 0)
            theGroup->Dereference();
        else
            theUnit->Dereference();
        return 0;
    case EE_UNITS_GOAL_HOST_UNIT_ID:
        return (uint32_t)theUnit->mUnitID;
    case EE_UNITS_GOAL_HOST_UNIT_TILE:
        ioArgs->i[0] = theUnit->mTileX.Get();
        ioArgs->i[1] = theUnit->mTileY.Get();
        return 0;
    case EE_UNITS_GOAL_HOST_UNIT_VALID:
        return theUnit->mIsValid ? 1 : 0;
    case EE_UNITS_GOAL_HOST_UNIT_COMMAND:
        return theComplex->mCommand ? (int64_t)theComplex->mCommand->mType : 0;
    case EE_UNITS_GOAL_HOST_RESET_PATH_ATTEMPTS:
        ObjectOf<EEUnitGoal>(inA)->SetPathAttempts(0);
        return 0;
    case EE_UNITS_GOAL_HOST_START_ACTION:
        theComplex->StartAction((unsigned long)ioArgs->i[0]);
        theComplex->mUnitClass->ActionChanged(theComplex);
        return 0;
    case EE_UNITS_GOAL_HOST_FIND_UNIT:
        if (inB != 0)
            return Handle(EEUnitCast<EEComplexUnit>(::gWorld.GetUnit((unsigned long)inA)));
        return Handle(static_cast<EEUnit*>(::gWorld.GetUnit((unsigned long)inA)));
    case EE_UNITS_GOAL_HOST_UNIT_POSITION:
        theUnit->GetPosition(thePoint);
        PointOut(ioArgs, thePoint);
        return 0;
    case EE_UNITS_GOAL_HOST_TILE_LOCATION:
        ::gWorld.mMap->GetTileLocation((long)ioArgs->i[0], (long)ioArgs->i[1], thePoint);
        PointOut(ioArgs, thePoint);
        return 0;
    case EE_UNITS_GOAL_HOST_TELL_GROUP:
        if (theComplex->mActiveGroup != NULL)
            theComplex->mActiveGroup->TellGroup(theComplex, thePoint);
        return 0;
    case EE_UNITS_GOAL_HOST_SET_PHYSICS_ACTION:
        theComplex->SetCurrentPhysicsAction((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_SET_ANIMATION:
        theComplex->SetAnimation((long)ioArgs->i[0], -1, 0);
        return 0;
    case EE_UNITS_GOAL_HOST_DROP_PATH:
        theComplex->DropPath();
        return 0;
    case EE_UNITS_GOAL_HOST_NEW_GOAL:
        return Handle(new EEUGRust((unsigned char)inA, inB != 0));
    case EE_UNITS_GOAL_HOST_FILE_OPEN:
        return theContext->mFile->GetIsOpen() ? 1 : 0;
    case EE_UNITS_GOAL_HOST_WRITE:
        if (ioArgs->i[0] == 0)
            return theContext->mFile->Write((uint32_t)ioArgs->i[1]);
        if (ioArgs->i[0] == 1)
            return theContext->mFile->Write((int32_t)ioArgs->i[1]);
        return theContext->mFile->Write(ioArgs->i[1] != 0);
    case EE_UNITS_GOAL_HOST_READ: {
            long theResult;
            if (ioArgs->i[0] == 0) {
                uint32_t theValue = (uint32_t)ioArgs->i[1];
                theResult = theContext->mFile->Read(theValue);
                ioArgs->i[1] = theValue;
            } else if (ioArgs->i[0] == 1) {
                int32_t theValue = (int32_t)ioArgs->i[1];
                theResult = theContext->mFile->Read(theValue);
                ioArgs->i[1] = theValue;
            } else {
                bool theValue = ioArgs->i[1] != 0;
                theResult = theContext->mFile->Read(theValue);
                ioArgs->i[1] = theValue;
            }
            return theResult;
        }
    case EE_UNITS_GOAL_HOST_APPEND: {
            std::string theText((const char*)ioArgs->text, ioArgs->len);
            if (theContext->mText != NULL)
                *theContext->mText += theText.c_str();
            else
                fputs(theText.c_str(), theContext->mDump);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_APPEND_CLASS_NAME:
        *theContext->mText += (const char*)theUnit->mUnitClass->mName;
        return 0;

    // the patrol's
    case EE_UNITS_GOAL_HOST_NEW_PATROL_GROUP:
        return Handle(new EEPPatrolGroup);
    case EE_UNITS_GOAL_HOST_GROUP_NEXT:
        return Handle(theGroup->GetNext(ObjectOf<EEComplexUnit>(inB)));
    case EE_UNITS_GOAL_HOST_GROUP_PREVIOUS:
        return Handle(theGroup->GetPrevious(ObjectOf<EEComplexUnit>(inB)));
    case EE_UNITS_GOAL_HOST_GROUP_COUNT:
        return (uint32_t)theGroup->GetNumPoints();
    case EE_UNITS_GOAL_HOST_GROUP_ID:
        return (uint32_t)theGroup->mID;
    case EE_UNITS_GOAL_HOST_GROUP_NEAREST:
        return Handle(theGroup->GetNearest((long)ioArgs->i[0], (long)ioArgs->i[1]));
    case EE_UNITS_GOAL_HOST_PLAYER_GROUP:
        return Handle(theComplex->GetPlayer()->GetPatrolGroup((unsigned long)inB));
    case EE_UNITS_GOAL_HOST_GROUP_POINTS:
        return (int64_t)theGroup->mPatrolPoints.size();
    case EE_UNITS_GOAL_HOST_GROUP_POINT:
        return Handle(theGroup->mPatrolPoints[(size_t)inB]);

    // the cast calamity's
    case EE_UNITS_GOAL_HOST_CALAMITY_CLASS: {
            EETechTreeNode* theNode = theComplex->mPlayer->mTechnologies->Get((long)ioArgs->i[0]);
            if (theNode == NULL)
                return 0;
            ioArgs->i[1] = ioArgs->i[2] = theNode->mData->mCalamity;
            return Handle(
                EECalamityManager::GetInstance()->GetCalamityClass((EECalamityEnum)theNode->mData->mCalamity));
        }
    case EE_UNITS_GOAL_HOST_CALAMITY_PLACE:
        theClass->Place(ObjectOf<EEComplexUnit>(inB), (unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_CALAMITY_INFO:
        ioArgs->i[0] = theClass->GetTargetKind();
        ioArgs->i[1] = theClass->GetCost();
        ioArgs->i[2] = theClass->mRange;
        return 0;
    case EE_UNITS_GOAL_HOST_CREATE_CALAMITY: {
            EECalamityManager* theManager = EECalamityManager::GetInstance();
            unsigned long theCalamity = (unsigned long)ioArgs->i[1];
            unsigned long theSource = theComplex->GetUnitID();
            unsigned long thePlayer = ObjectOf<EEComplexUnit>((uint64_t)ioArgs->i[4])->mPlayer->GetPlayerID();
            if (ioArgs->i[0] == 3)
                return theManager->CreateUnitCalamity(theCalamity, theSource, ObjectOf<EEUnit>(inB)->GetUnitID(),
                                                      thePlayer);
            if (ioArgs->i[0] == 2)
                return theManager->CreateLocationCalamity(theCalamity, theSource, (long)ioArgs->i[2],
                                                          (long)ioArgs->i[3], thePlayer);
            return theManager->CreateGlobalCalamity(theCalamity, theSource, thePlayer);
        }
    case EE_UNITS_GOAL_HOST_RESET_SAVED_GROUP:
        theComplex->mPlayer->mSavedGroups.Reset(6, 1);
        return 0;
    case EE_UNITS_GOAL_HOST_SPEND_POWER:
        theComplex->SetPower(theComplex->GetPower() - (long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_CAN_CAST_AT: {
            GE3DPoint theLocation;
            return theClass->EECalamityClass::CanCastAt(ObjectOf<EEComplexUnit>(inB), theLocation,
                                                        (unsigned long)ioArgs->i[0], (unsigned long)ioArgs->i[1],
                                                        true, NULL) == 0;
        }
    case EE_UNITS_GOAL_HOST_FACES_TARGET:
        return theComplex->GetUnitClass()->FacesTarget(theComplex, ObjectOf<EEUnit>(inB), false);
    case EE_UNITS_GOAL_HOST_FACES_POINT:
        return theComplex->GetUnitClass()->FacesPoint(theComplex, thePoint, false);
    case EE_UNITS_GOAL_HOST_UNIT_IN_SET: {
            EEUnitSet* theSet = gUnitSetManager.GetSet((long)ioArgs->i[0]);
            return theSet != NULL && theSet->Contains(theComplex->mUnitClass->mUnitType);
        }
    case EE_UNITS_GOAL_HOST_STATS_RANGE:
        ioArgs->f[0] = theComplex->mStats->mRange;
        return 0;
    case EE_UNITS_GOAL_HOST_CONTACT_RANGE: {
            GE3DPoint theTarget;
            ObjectOf<EEUnit>(inB)->GetPosition(theTarget);
            theComplex->GetPosition(thePoint);
            return EEIsInContactRange(theComplex, ObjectOf<EEUnit>(inB), thePoint.DistanceToSquared(theTarget));
        }
    case EE_UNITS_GOAL_HOST_DISTANCE_TO_UNIT:
        ioArgs->f[0] = theComplex->DistanceSquaredToUnit(ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_UNIT_HEIGHT:
        ioArgs->f[0] = theComplex->mHeight;
        return 0;
    case EE_UNITS_GOAL_HOST_HIT_POINTS:
        return theUnit->mHitPoints;
    case EE_UNITS_GOAL_HOST_UNDETECTABLE: {
            EEUnit* theTarget = ObjectOf<EEUnit>(inB);
            return theTarget->mComplexUnit->mIsCloakable && theTarget->mComplexUnit->GetIsCloaked()
                && theTarget->mPlayer != theComplex->mPlayer
                && theComplex->mPlayer->mEnemies[theTarget->mPlayer->mPlayerID] != 0
                && theTarget->mUnitClass->mCloakRadius == 0;
        }
    case EE_UNITS_GOAL_HOST_COMPLEX_UNIT:
        return Handle(theUnit->mComplexUnit);

    // the explore goal's
    case EE_UNITS_GOAL_HOST_PLAYER_HUMAN:
        return theComplex->mPlayer->IsHuman() ? 1 : 0;
    case EE_UNITS_GOAL_HOST_PLAYER_SEES_ALL:
        return theComplex->mPlayer->mSeesAll ? 1 : 0;
    case EE_UNITS_GOAL_HOST_CONTAINER:
        return Handle(theComplex->mContainer);
    case EE_UNITS_GOAL_HOST_GROUP_UNITS:
        return theComplex->mActiveGroup == NULL ? -1 : (int64_t)theComplex->mActiveGroup->GetNumUnits();
    case EE_UNITS_GOAL_HOST_PATH_AREA_BOUNDS: {
            EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(theComplex);
            if (theArea == NULL)
                return 0;
            const U2DRectangle<long>& theBounds = theArea->mHull.GetBounds();
            ioArgs->i[0] = theBounds.mTop;
            ioArgs->i[1] = theBounds.mBottom;
            ioArgs->i[2] = theBounds.mLeft;
            ioArgs->i[3] = theBounds.mRight;
            return 1;
        }
    case EE_UNITS_GOAL_HOST_LINE_OF_SIGHT:
        ioArgs->f[0] = theComplex->GetLineOfSight();
        return 0;
    case EE_UNITS_GOAL_HOST_RING_TILES: {
            std::vector<U2DPoint<long> > theTiles;
            EEGetSquareRingTiles((long)ioArgs->i[0], (long)ioArgs->i[1], (long)ioArgs->i[2], theTiles);
            EEAIRandomShuffle(theTiles.begin(), theTiles.end());
            for (size_t i = 0; i < theTiles.size(); i++)
                ee_units_goal_push_tile(ioArgs->sink, (int32_t)theTiles[i].mX, (int32_t)theTiles[i].mY);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_TILE_SCREEN: {
            EEMap* theMap = ::gWorld.mMap;
            long theX = (long)ioArgs->i[0];
            long theY = (long)ioArgs->i[1];
            EEMapTile* theTile = theMap->GetTile(theX, theY);
            if (theTile == NULL || theMap->IsEdgeTile(theX, theY) || theTile->IsVisibleTo(theComplex->mPlayer))
                return 1;
            return (theComplex->mUnitClass->mDomains & kMovementAir) != 0
                && (!theMap->mTerrainMesh.GetIsSpaceMap() || theComplex->mUnitClass->CanMoveOnTerrain(theComplex, 12));
        }
    case EE_UNITS_GOAL_HOST_CELL_TAKEN: {
            GE3DPoint theCell((float)(long)ioArgs->i[0] + 0.5f, (float)(long)ioArgs->i[1] + 0.5f, 0.0f);
            return theComplex->mActiveGroup != NULL && theComplex->mActiveGroup->IsCellTaken(theCell);
        }
    case EE_UNITS_GOAL_HOST_CAN_STAND: {
            EETileCoordinate theTile;
            theTile.mX = (long)ioArgs->i[0];
            theTile.mY = (long)ioArgs->i[1];
            return EECanStandOn(theTile, theComplex);
        }
    case EE_UNITS_GOAL_HOST_PATH_REACHABLE: {
            EECPASGFlags theFlags;
            theComplex->mUnitClass->GetTerrainFlags(theComplex, theFlags);
            EETileCoordinate theFrom;
            theFrom.mX = (long)ioArgs->i[0];
            theFrom.mY = (long)ioArgs->i[1];
            EETileCoordinate theTo;
            theTo.mX = (long)ioArgs->i[2];
            theTo.mY = (long)ioArgs->i[3];
            EEAutoPathRequest* thePath =
                gPathManager.RequestPath(theComplex->mPlayer->GetPlayerID(), theFrom, theTo, &theFlags, 1, 10, NULL, 1);
            bool theReachable = false;
            if (thePath->mPath.size() > 0) {
                EETileCoordinate theLast = thePath->mPath.back();
                EEAIPathSubSector* theFirst = gContinentManager.GetPathSubSectorAt(theLast.GetX(), theLast.mY);
                EEAIPathSubSector* theSecond = gContinentManager.GetPathSubSectorAt(theTo.GetX(), theTo.mY);
                theReachable = theFirst == theSecond;
                if (!theReachable && theFirst != NULL && theSecond != NULL && theFirst->IsNeighbor(theSecond))
                    theReachable = theFirst->AreLinksBlocked(theSecond, theFlags);
            }
            thePath->Dereference();
            return theReachable;
        }
    case EE_UNITS_GOAL_HOST_PATH_AREA_ID: {
            U2DPoint<long> theTile;
            theTile.mX = (long)ioArgs->i[0];
            theTile.mY = (long)ioArgs->i[1];
            EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(theTile);
            return theArea != NULL ? theArea->mID : 0;
        }
    case EE_UNITS_GOAL_HOST_MAP_DIMENSIONS:
        ioArgs->i[0] = gMapWidth;
        ioArgs->i[1] = gMapHeight;
        ioArgs->i[2] = ::gWorld.mMap->mL195614;
        ioArgs->i[3] = ::gWorld.mMap->mL195610;
        return 0;
    case EE_UNITS_GOAL_HOST_CONTINENT_POINT: {
            EEAIArea* theArea;
            if (((theComplex->mUnitClass->mDomains & kMovementAir) == 0
                 || (::gWorld.mMap->mTerrainMesh.GetIsSpaceMap() && !theComplex->mUnitClass->CanMoveOnTerrain(theComplex, 12)))
                && theComplex->mUnitClass->mUnitType != kUnitTypeSp15SpaceFighter)
                theArea = gContinentManager.GetContinentAt(theComplex);
            else
                theArea = dynamic_cast<EEAIContinent*>(
                    gContinentManager.GetRandomContinent(kContinentCategoryNone, gPhysics->mRandom));
            if (theArea == NULL)
                return 0;
            U2DPoint<long> thePoint = theArea->GetRandomPoint();
            ioArgs->i[0] = thePoint.mX;
            ioArgs->i[1] = thePoint.mY;
            return 1;
        }
    case EE_UNITS_GOAL_HOST_RANDOM:
        return gPhysics->GetRandom((long)ioArgs->i[0], (long)ioArgs->i[1]);
    case EE_UNITS_GOAL_HOST_NOTICE_FOREIGN: {
            EEUnit* theTarget = EEFindNearbyTarget(theComplex);
            if (theTarget != NULL)
                theComplex->mPlayer->NoticeForeignUnit(theTarget);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_UNIT_FAMILY:
        return theComplex->mUnitClass->mFamily;
    case EE_UNITS_GOAL_HOST_OWNS_LIGHTHOUSE: {
            std::vector<EEUnit*> theUnits;
            EEGetPlayerUnitsOfType(kUnitTypeLighthouseAtAlexandria, theComplex->mPlayer->GetPlayerID(), theUnits);
            return theUnits.size() > 0;
        }
    }
    return 0;
}

// one body (EE_UNITS_GOAL_*) of inGoal; inA, inB, ioArgs as it takes them; the file or the text it writes
int64_t Goal(int inOperation, const EEUGRust* inGoal, uint64_t inA = 0, uint64_t inB = 0,
             EeUnitsGoalIo* ioArgs = NULL, FSFile* inFile = NULL, UWideString* inText = NULL, FILE* inDump = NULL)
{
    EEGoalContext theContext = {inFile, inText, inDump};
    EeUnitsGoalHost theHost = {&theContext, GoalOp};
    EeUnitsGoalIo theArgs = {};
    return ee_units_rust_goal(inOperation, inGoal->mKind, Handle(inGoal), inA, inB,
                              ioArgs != NULL ? ioArgs : &theArgs, &theHost);
}

}  // namespace

EEUGRust::EEUGRust(unsigned char inKind, bool inArg)
    : EEUnitGoal(inKind, inArg)
{
}

EEUGRust* EEUGRust::NewPatrol(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalPatrol, inArg);
    Goal(EE_UNITS_GOAL_PATROL_NEW_EMPTY, theGoal);
    return theGoal;
}

EEUGRust* EEUGRust::NewPatrol(EEPPatrolGroup* inGroup, EEComplexUnit* inTarget, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalPatrol, inArg);
    Goal(EE_UNITS_GOAL_NEW_PATROL, theGoal, Handle(inGroup), Handle(inTarget));
    return theGoal;
}

EEPPatrolGroup* EEUGRust::GetPatrolGroup()
{
    return ObjectOf<EEPPatrolGroup>((uint64_t)Goal(EE_UNITS_GOAL_PATROL_GROUP, this));
}

EEComplexUnit* EEUGRust::GetPatrolTarget()
{
    return ObjectOf<EEComplexUnit>((uint64_t)Goal(EE_UNITS_GOAL_PATROL_TARGET, this));
}

void EEUGRust::SetPatrolGroup(EEPPatrolGroup* inGroup)
{
    Goal(EE_UNITS_GOAL_PATROL_SET_GROUP, this, Handle(inGroup));
}

EEUGRust* EEUGRust::NewCastCalamity(EEComplexUnit* inSource, EEUnit* inTarget, long inTileX, long inTileY,
                                    long inCalamityType, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalCastCalamity, inArg);
    EeUnitsGoalIo theArgs = {{inTileX, inTileY, inCalamityType}};
    Goal(EE_UNITS_GOAL_NEW_CALAMITY, theGoal, Handle(inSource), Handle(inTarget), &theArgs);
    return theGoal;
}

EEUGRust* EEUGRust::NewCastCalamity(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalCastCalamity, inArg);
    Goal(EE_UNITS_GOAL_CALAMITY_NEW_EMPTY, theGoal);
    return theGoal;
}

long EEUGRust::GetCalamityType(long* outTileX, long* outTileY, EEUnit** outTarget)
{
    EeUnitsGoalIo theArgs = {};
    EEUnit* theTarget = ObjectOf<EEUnit>((uint64_t)Goal(EE_UNITS_GOAL_CALAMITY_FIELDS, this, 0, 0, &theArgs));
    if (outTileX != NULL)
        *outTileX = (long)theArgs.i[1];
    if (outTileY != NULL)
        *outTileY = (long)theArgs.i[2];
    if (outTarget != NULL)
        *outTarget = theTarget;
    return (long)theArgs.i[0];
}

void EEUGRust::GetCalamityTargetLocation(GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_CALAMITY_TARGET_LOCATION, this, 0, 0, &theArgs);
    PointIn(theArgs, outLocation);
}

EEUGRust* EEUGRust::NewExplore(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalExplore, inArg);
    Goal(EE_UNITS_GOAL_NEW_EXPLORE, theGoal);
    return theGoal;
}

void EEUGRust::GetExploreTileCenter(GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_EXPLORE_TILE, this, 0, 0, &theArgs);
    outLocation.SetX((float)(long)theArgs.i[0] + 0.5f);
    outLocation.SetY((float)(long)theArgs.i[1] + 0.5f);
}

void EEUGRust::SetExploreTile(long inX, long inY)
{
    EeUnitsGoalIo theArgs = {{inX, inY}};
    Goal(EE_UNITS_GOAL_EXPLORE_SET_TILE, this, 0, 0, &theArgs);
}

EEUGRust::~EEUGRust()
{
    Goal(EE_UNITS_GOAL_DESTROY, this);
}

void EEUGRust::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    PointOut(&theArgs, outLocation);
    Goal(EE_UNITS_GOAL_DESTINATION, this, Handle(inUnit), 0, &theArgs);
    PointIn(theArgs, outLocation);
}

EEUnitGoal* EEUGRust::Clone()
{
    return ObjectOf<EEUGRust>((uint64_t)Goal(EE_UNITS_GOAL_CLONE, this, 0, this->GetB1C()));
}

void EEUGRust::DumpOutOfSynch(FILE* inFile)
{
    Goal(EE_UNITS_GOAL_DUMP, this, 0, 0, NULL, NULL, NULL, inFile);
}

long EEUGRust::GetOutOfSynchChecksum()
{
    return EEUnitGoal::GetOutOfSynchChecksum() + (long)Goal(EE_UNITS_GOAL_CHECKSUM, this);
}

void EEUGRust::GetDebugLine(UWideString& ioText)
{
    Goal(EE_UNITS_GOAL_DEBUG_LINE, this, 0, 0, NULL, NULL, &ioText);
}

void EEUGRust::GetDebugText(UWideString& ioText)
{
    Goal(EE_UNITS_GOAL_DEBUG_TEXT, this, 0, 0, NULL, NULL, &ioText);
}

void EEUGRust::GetGoalLocation(GE3DPoint& outLocation)
{
    EeUnitsGoalIo theArgs = {};
    PointOut(&theArgs, outLocation);
    Goal(EE_UNITS_GOAL_LOCATION, this, 0, 0, &theArgs);
    PointIn(theArgs, outLocation);
}

long EEUGRust::GetStartAction()
{
    return (long)Goal(EE_UNITS_GOAL_START_ACTION, this);
}

void EEUGRust::SetOwner(EEComplexUnit* inUnit)
{
    Goal(EE_UNITS_GOAL_SET_OWNER, this, Handle(inUnit));
}

void EEUGRust::Step(EEComplexUnit* inUnit)
{
    Goal(EE_UNITS_GOAL_STEP, this, Handle(inUnit));
}

long EEUGRust::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult != 0)
        return theResult;
    return (long)Goal(EE_UNITS_GOAL_LOAD, this, 0, 0, NULL, &inFile);
}

long EEUGRust::RepairPointers(unsigned long, unsigned long)
{
    return (long)Goal(EE_UNITS_GOAL_REPAIR, this);
}

void EEUGRust::Start(EEComplexUnit* inUnit)
{
    Goal(EE_UNITS_GOAL_START, this, Handle(inUnit));
}

long EEUGRust::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != 0)
        return theResult;
    return (long)Goal(EE_UNITS_GOAL_SAVE, this, 0, 0, NULL, &outFile);
}

bool EEUGRust::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    return Goal(EE_UNITS_GOAL_EVALUATE, this, Handle(inUnit), inEvent) != 0;
}

void EEUGRust::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    Goal(EE_UNITS_GOAL_REPLACE_UNIT, this, Handle(inOld), Handle(inNew));
}

bool EEUGRust::Suits(EEUnit*)
{
    return true;
}
