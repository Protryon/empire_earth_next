// New (port; docs/RUST_PLAN.md R5b.4, rounds 16 and 17): the goals whose class moved whole into Rust
// (ee/EEUGRust.h). Each virtual is one call of the Rust body (ee_units rust_goal.rs, by mKind) over the host below:
// one operation callback whose cases are the reads and world calls the bodies make (the units, the patrol group's
// walk, the calamity classes and manager, the explore goal's map, path and continent queries, the transport
// classes, the garrisons, the typed file calls, the texts). The base goal's own fields, save and checksum stay
// EEUnitGoal's.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
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
#include "ee/EEUCTransport.h"
#include "ee/EEUCAirplane.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
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
#include "ee/EEEvent.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUCBuilding.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/GESurface.h"
#include "lle/GEViewport.h"
#include "lle/GEScreen.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGGather.h"
#include "ee/EEUCGate.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPWallManager.h"
#include "ee/EECPBuildManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEStringIDs.h"
#include "lle/SESoundManager.h"
#include "lle/UResourceFile.h"
#include "ee/EEUnitKind.h"
#include "rust/ee_units.h"

long gSpaceUnloadSoundID = 498;             // 0x4c8fa8 (.data = 0x1f2; the teleporter's unload sound)

// ---- the build/repair goal's world work (EEUGBuildRepair.cpp's helpers; the goal itself is build.rs)

// FUNCTION: EXE 0x61e104
// Orders units farthest from the reference tile first, so a sort leaves the nearest unit last
inline long EETileDistanceLess::DistanceSquared(EEUnit* inUnit) const
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return this->DistanceSquared(theTile);
}

bool EETileDistanceLess::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    long theLeft = this->DistanceSquared(inFirst);
    long theRight = this->DistanceSquared(inSecond);
    bool theResult = theLeft > theRight;
    return theResult;
}

namespace {

// the build/repair search radius (0x447404; provisional name)
const float kBuildRepairRadius = 4.0f;

// 0x61eb31: the unit helps build the first unbuilt, paid-for land building of its player within 4 tiles
void EEHelpBuildNearby(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    std::vector<EEUnit*> theUnits;
    GE2DPoint theCenter;
    U2DPoint<long> theTile(inTarget->mTileX, inTarget->mTileY);
    theCenter = GE2DPoint((float)theTile.GetX(), (float)theTile.GetY());
    if (EEGetUnitsInRadius(theCenter, kBuildRepairRadius, theUnits) > 0) {
        std::vector<EEUnit*>::iterator i;
        for (i = theUnits.begin(); i != theUnits.end(); ++i) {
            EEComplexUnit* theUnit = (EEComplexUnit*)*i;
            if (!theUnit->mIsBuilt && theUnit != inTarget && theUnit->mHitPoints > 0
                && theUnit->mUnitClass->IsLandBuilding() && theUnit->mIsPaidFor && theUnit->GetPlayer() == inUnit->GetPlayer())
                break;
        }
        if (i != theUnits.end()) {
            inUnit->SetGoal(EEUGRust::NewBuildRepair((*i)->GetPlayer(), *i, false), 0, 0);
            inUnit->StartAction(kActionMoveToRepairSite);
            inUnit->mUnitClass->ActionChanged(inUnit);
        }
    }
}

// 0x61ec58: the unit works the nearest free farm around the farm's drop sites
void EETendNextFarm(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    std::vector<EEUnit*> theFreeUnits;
    std::vector<EEUnit*> theUnits;
    long theCount = 1;
    if (EEGetFarmDropSites(inTarget, theUnits)) {
        for (std::vector<EEUnit*>::iterator i = theUnits.begin(); i != theUnits.end(); ++i) {
            while ((inTarget = EEFindFreeFarm((*i)->GetComplexUnit(), 1, &theCount)) != 0) {
                EEUnit* theFree = inTarget;
                theFreeUnits.push_back(theFree);
            }
        }
        if (theFreeUnits.size() > 0) {
            std::sort(theFreeUnits.begin(), theFreeUnits.end(), EETileDistanceLess(inUnit->GetTile()));
            EEWorkFarm(theFreeUnits.back()->mComplexUnit, inUnit);
        }
    }
}

// 0x61ed5e: the unit gathers a resource near a drop site, passing over mines at the player's worker limit (else a
// random one of those). The original repeats the identical first search when it returns null: both calls are kept.
void EEGatherNearBuilding(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    std::vector<EEUnit*> theRejected;
    bool theResult = false;
    EEUCBasicCitizen* theClass = EEUnitClassCast<EEUCBasicCitizen>(inUnit->GetUnitClass());
    EEUnit* theResource = theClass->FindResourceOfKind(inUnit, inTarget, NULL, 6, 1, &theRejected, false);
    if (theResource == NULL)
        theResource = theClass->FindResourceOfKind(inUnit, inTarget, NULL, 6, 1, &theRejected, false);
    while (!theResult && theResource != NULL) {
        if (!theResource->mUnitClass->mIsTree && theResource->mUnitClass->mAnimalID == 1000
            && ::gWorld.GetResourceUsers(theResource) >= inUnit->mPlayer->GetMiningLimit()) {
            theRejected.push_back(theResource);
            theResource = theClass->FindResourceOfKind(inUnit, inTarget, NULL, 6, 1, &theRejected, false);
        } else {
            inUnit->SetGoal(new EEUGGather(theResource, inTarget, false), 0, 0);
            inUnit->StartAction(kActionMoveToWaitingArea);
            inUnit->mUnitClass->ActionChanged(inUnit);
            theResult = true;
        }
    }
    if (!theResult && theRejected.size() > 0) {
        EEAIRandomShuffle(theRejected.begin(), theRejected.end());
        inUnit->SetGoal(new EEUGGather(theRejected[0], inTarget, false), 0, 0);
        inUnit->StartAction(kActionMoveToWaitingArea);
        inUnit->mUnitClass->ActionChanged(inUnit);
    }
}

// 0x61f4f5: units other than the building block a tile of its footprint
bool EEBuildSiteTaken(EEUnit* inTarget, EEPlayer* inPlayer, long inType)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    bool theResult = false;
    EEGetFootprintTiles(inTarget, theTiles);
    for (unsigned long i = 0; i < theTiles.size() && !theResult; ++i) {
        EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles[i].GetX(), theTiles[i].GetY());
        EEGetBlockingUnits(inPlayer, inType, theTile, 0, theUnits);
        if (theUnits.size() > 1 || (theUnits.size() == 1 && theUnits[0] != inTarget))
            theResult = true;
    }
    return theResult;
}

// a gate's own parts (the first part and every next one)
void EEGateParts(EEUnit* inTarget, std::vector<EEUnit*>& outParts)
{
    if (inTarget->GetUnitClass()->GetCategory() != kUnitCategoryGate)
        return;
    EEComplexUnit* theUnit = inTarget->GetComplexUnit();
    while (theUnit->mPreviousPart != NULL)
        theUnit = theUnit->mPreviousPart;
    for (; theUnit != NULL; theUnit = theUnit->mNextPart)
        outParts.push_back(theUnit);
}

// 0x61f5fc: no blocking or movable units stand on the footprint (the building, a runway under type 0x42 and a gate's
// own parts excepted); farms always pass
bool EEBuildFootprintClear(EEUnit* inTarget, EEPlayer* inPlayer, long inType)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theLinked;
    bool theResult = true;
    if (inType != 0x2a && inType != 0x33a) {
        EEGateParts(inTarget, theLinked);
        EEGetFootprintTiles(inTarget, theTiles);
        for (unsigned long i = 0; i < theTiles.size() && theResult; ++i) {
            EEMapTile* theTile = ::gWorld.GetMap()->GetTile(theTiles[i].GetX(), theTiles[i].GetY());
            if (theTile == NULL)   // port: a footprint tile off the map (Small Islands seed 11): nothing stands there
                continue;
            EEGetBlockingUnits(inPlayer, inType, theTile, 0, theUnits);
            if (theUnits.empty() || (theUnits.size() == 1 && theUnits[0] == inTarget)
                || (theUnits.size() == 1 && theUnits[0]->GetUnitClass()->GetUnitType() == kUnitTypeAirportRunway && inType == 0x42))
                theResult = !EEGetMovableTileUnits(inPlayer, inType, theTile, 0, theUnits);
            else
                theResult = false;
            if (!theResult && theLinked.size() > 0) {
                for (unsigned long j = 0; j < theUnits.size() && !theResult; ++j)
                    theResult = std::find(theLinked.begin(), theLinked.end(), theUnits[j]) != theLinked.end();
            }
        }
    }
    return theResult;
}

// 0x61f85e: no unit on the footprint stands in the way (standing 2) and no foundation above 1 hit point is there
// (the building, the player's runway under type 0x42 and a gate's own parts excepted)
bool EEBuildSiteFree(EEUnit* inTarget, EEPlayer* inPlayer, long inType)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theLinked;
    bool theResult = true;
    EEGetFootprintTiles(inTarget, theTiles);
    EEGetUnitsOn(theTiles, theUnits);
    EEGateParts(inTarget, theLinked);
    unsigned long theCount = theUnits.size();
    for (unsigned long i = 0; i < theCount && theResult; ++i) {
        if (theUnits[i] == inTarget
            || (theUnits[i]->mUnitClass->GetUnitType() == kUnitTypeAirportRunway && inType == 0x42 && theUnits[i]->mPlayer == inPlayer)
            || ((theLinked.size() > 0) && std::find(theLinked.begin(), theLinked.end(), theUnits[i]) != theLinked.end()))
            continue;
        theResult = EEGetTileUnitStanding(theUnits[i], inPlayer, inType, false) != 2;
        if (theUnits[i]->IsFoundation() && theUnits[i]->mHitPoints > 1)
            theResult = false;
    }
    return theResult;
}

}  // namespace

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

// the tiles of an argument block (i[2k], i[2k + 1]) in and out
void TilesIn(const EeUnitsGoalIo* inArgs, U2DPoint<long>* outTiles, int inCount)
{
    for (int k = 0; k < inCount; k++)
        outTiles[k].SetXY((long)inArgs->i[2 * k], (long)inArgs->i[2 * k + 1]);
}

void TilesOut(EeUnitsGoalIo* ioArgs, const U2DPoint<long>* inTiles, int inCount)
{
    for (int k = 0; k < inCount; k++) {
        ioArgs->i[2 * k] = inTiles[k].mX;
        ioArgs->i[2 * k + 1] = inTiles[k].mY;
    }
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
        if (ioArgs->i[1] == 0)
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
        if (ioArgs->i[0] == 3) {
            uint32_t theBits = (uint32_t)ioArgs->i[1];
            float theValue;
            memcpy(&theValue, &theBits, sizeof(theValue));
            return theContext->mFile->Write(theValue);
        }
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
            } else if (ioArgs->i[0] == 3) {
                uint32_t theBits = (uint32_t)ioArgs->i[1];
                float theValue;
                memcpy(&theValue, &theBits, sizeof(theValue));
                theResult = theContext->mFile->Read(theValue);
                memcpy(&theBits, &theValue, sizeof(theBits));
                ioArgs->i[1] = theBits;
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

    // the unload transport's
    case EE_UNITS_GOAL_HOST_UNLOAD_RANGE:
        ioArgs->f[0] = theComplex->mUnitClass->mUnloadRange;
        return 0;
    case EE_UNITS_GOAL_HOST_CLASS_TYPE:
        return theComplex->mUnitClass->mUnitType;
    case EE_UNITS_GOAL_HOST_CARGO_COUNT:
        return (uint32_t)theComplex->GetNumCargoUnits();
    case EE_UNITS_GOAL_HOST_CARGO_CATEGORY:
        return theComplex->mCargo[0]->mUnitClass->mCategory;
    case EE_UNITS_GOAL_HOST_TRANSPORT_KIND:
        return (EEUnitClassCast<EEUCTransport>(theComplex->GetUnitClass()) != NULL ? 1 : 0)
            | (EEUnitClassCast<EEUCAirplane>(theComplex->GetUnitClass()) != NULL ? 2 : 0);
    case EE_UNITS_GOAL_HOST_CAN_UNLOAD_AT: {
            EEUCTransport* theTransport = EEUnitClassCast<EEUCTransport>(theComplex->GetUnitClass());
            if (theTransport != NULL)
                return theTransport->CanUnloadAt(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
            return EEIsTileFree(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
        }
    case EE_UNITS_GOAL_HOST_FIND_UNLOAD_TILES: {
            U2DPoint<long> theTiles[4];
            TilesIn(ioArgs, theTiles, 4);
            EEUCTransport* theTransport = EEUnitClassCast<EEUCTransport>(theComplex->GetUnitClass());
            if (theTransport != NULL)
                theTransport->FindUnloadTiles(theComplex, &theTiles[0], &theTiles[1], &theTiles[2], &theTiles[3]);
            else
                EEUnitClassCast<EEUCAirplane>(theComplex->GetUnitClass())
                    ->FindUnloadTiles(theComplex, &theTiles[0], &theTiles[1], &theTiles[2], &theTiles[3]);
            TilesOut(ioArgs, theTiles, 4);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_CAN_UNLOAD_NEAR: {
            U2DPoint<long> theTiles[3];
            TilesIn(ioArgs, theTiles, 3);
            bool theResult = EEUnitClassCast<EEUCTransport>(theComplex->GetUnitClass())
                                 ->CanUnloadNear(theComplex, theComplex->GetCargo(0), &theTiles[0], &theTiles[1],
                                                 &theTiles[2]);
            TilesOut(ioArgs, theTiles, 3);
            return theResult;
        }
    case EE_UNITS_GOAL_HOST_SAME_PLAYER:
        return theComplex->mPlayer == ObjectOf<EEComplexUnit>(inB)->mPlayer;
    case EE_UNITS_GOAL_HOST_FIRE_CARGO:
        theComplex->mUnitClass->FireShot(theComplex);
        return 0;
    case EE_UNITS_GOAL_HOST_UNLOAD_AT:
        EEUnitClassCast<EEUCTransport>(theComplex->GetUnitClass())
            ->UnloadAt(theComplex, (long)ioArgs->i[0], (long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_UNLOAD_SOUND:
        ioArgs->i[0] = ioArgs->i[0] != 0 ? gSpaceUnloadSoundID : 0x184;
        // fall through
    case EE_UNITS_GOAL_HOST_PLAY_SOUND: {
            EESoundObject* theSound = EESoundObject::Get((long)ioArgs->i[0]);
            if (theSound != NULL) {
                float theVolume;
                float thePan;
                float thePitch;
                theComplex->mUnitClass->GetSoundParameters(theComplex, theSound, &theVolume, &thePan, &thePitch);
                theSound->Play(theVolume, thePan, thePitch);
            }
            return 0;
        }

    // the load goals' (the unit and class reads give zeros for no unit)
    case EE_UNITS_GOAL_HOST_LOAD_CLASS:
        if (theComplex != NULL) {
            EEUnitClass* theClass = theComplex->mUnitClass;
            int64_t theFields[7] = {theClass->mGarrisonCapacity, theClass->mTransportCapacity, theClass->mWeight,
                                    theClass->mGarrisonTechnology, theClass->mUnitType, theClass->mCanGarrison,
                                    theClass->mTileSize};
            memcpy(ioArgs->i, theFields, sizeof(theFields));
            ioArgs->f[0] = theClass->mLoadRange;
            ioArgs->f[1] = theClass->mInnerCollisionSphere;
        }
        return 0;
    case EE_UNITS_GOAL_HOST_LOAD_UNIT:
        if (theComplex != NULL) {
            int64_t theFields[6] = {theComplex->mGarrisonCount, theComplex->mCargoRoom, theComplex->mHitPoints,
                                    theComplex->mIsDisabled, theComplex->mWaypoints.empty(), theComplex->mAnimation};
            memcpy(ioArgs->i, theFields, sizeof(theFields));
        }
        return 0;
    case EE_UNITS_GOAL_HOST_UNIT_LOCATION:
        PointOut(ioArgs, GE3DPoint(theComplex->mPhysicalModel.GetLocation()));
        return 0;
    case EE_UNITS_GOAL_HOST_LINE_BLOCKED: {
            EEUnit* theBlocker;
            long theX = 0;
            long theY = 0;
            return EEIsLineBlocked((long)ioArgs->i[0], (long)ioArgs->i[1], (long)ioArgs->i[2], (long)ioArgs->i[3],
                                   &theBlocker, &theX, &theY);
        }
    case EE_UNITS_GOAL_HOST_CAN_LOAD: {
            EEUCTransport* theTransport = EEUnitClassCast<EEUCTransport>(theComplex->GetUnitClass());
            return theTransport != NULL && theTransport->CanLoad(theComplex, ObjectOf<EEComplexUnit>(inB));
        }
    case EE_UNITS_GOAL_HOST_BOARDING: {
            EEComplexUnit* theContainer = ObjectOf<EEComplexUnit>(inB);
            GE3DPoint theOut(ioArgs->f[3], ioArgs->f[4], ioArgs->f[5]);
            EEUnitClassCast<EEUCTransport>(theContainer->GetUnitClass())
                ->FindBoardingLocation(theComplex, theContainer, thePoint, theOut);
            ioArgs->f[3] = theOut.GetX();
            ioArgs->f[4] = theOut.GetY();
            ioArgs->f[5] = theOut.GetZ();
            return 0;
        }
    case EE_UNITS_GOAL_HOST_PICK_TILE_NEAR: {
            std::vector<EETileCoordinate> theTiles;
            EEGetTilesAroundUnit(theComplex, ObjectOf<EEComplexUnit>(inB), 1, 0, 0, theTiles);
            bool theFound = EEPickTileNear(theTiles, theComplex, ObjectOf<EEComplexUnit>(inB), 0, thePoint);
            PointOut(ioArgs, thePoint);
            return theFound;
        }
    case EE_UNITS_GOAL_HOST_RENDEZVOUS: {
            std::vector<EEComplexUnit*> theUnits;
            for (size_t k = 0; k < ioArgs->list_len; k++)
                theUnits.push_back(ObjectOf<EEComplexUnit>(ioArgs->list[k]));
            U2DPoint<long> theTiles[2];
            TilesIn(ioArgs, theTiles, 2);
            EEUCTransport* theClass = EEUnitClassCast<EEUCTransport>(theComplex->GetUnitClass());
            if (ioArgs->i[4] == 0)
                theClass->FindRendezvousTiles(&theUnits, theComplex, &theTiles[0], &theTiles[1]);
            else
                theClass->EEUCTransport::FindRendezvousTiles(&theUnits, theComplex, &theTiles[0], &theTiles[1]);
            TilesOut(ioArgs, theTiles, 2);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_CENTER_OFFSET:
        theComplex->AddCenterOffset(ioArgs->f[0], ioArgs->f[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_GARRISON_UPGRADE: {
            EETechTreeNode* theNode = theComplex->mPlayer->GetTechTree()->Get((long)ioArgs->i[0]);
            ioArgs->i[0] = theNode != NULL;
            if (theNode == NULL)
                return 0;
            long theUpgrade = EEEvent::GetEventEffect(EEEvent::Get(theNode->mData->mResearchedEvent)->GetEffect(0))->mNewUnitClass;
            EEUnitClass* theClass = gUnitClassTable[theUpgrade];
            EETechTreeNode* theAvailable = theClass != NULL ? theComplex->mPlayer->GetTechTree()->Get(theClass->GetTechnology()) : NULL;
            ioArgs->i[1] = theUpgrade;
            ioArgs->i[2] = theAvailable != NULL && theAvailable->GetIsAvailable();
            return 0;
        }
    case EE_UNITS_GOAL_HOST_UNIT_GOAL: {
            EEUnitGoal* theGoal = inB != 0 ? theComplex->GetGoal() : theComplex->GetLoadGoal();
            ioArgs->i[0] = theGoal != NULL ? theGoal->mKind : -1;
            return Handle(theGoal);
        }
    case EE_UNITS_GOAL_HOST_QUEUED_GOAL_KIND: {
            EEUnitGoal* theGoal = theComplex != NULL ? theComplex->GetQueuedGoal((long)inB) : NULL;
            return theGoal != NULL ? theGoal->mKind : -1;
        }
    case EE_UNITS_GOAL_HOST_CLEAR_GOALS:
        theComplex->ClearGoals((long)inB);
        return 0;
    case EE_UNITS_GOAL_HOST_DEPOSIT:
        if (theComplex->mResourceAmount > 0 && ObjectOf<EEComplexUnit>(inB)->CanTake(theComplex, theComplex->mResourceType))
            EEDepositLoad(theComplex, ObjectOf<EEComplexUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_GARRISON_UNIT:
        return theComplex->GarrisonUnit(ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_LOAD_CARGO:
        return theComplex->LoadCargo(ObjectOf<EEComplexUnit>(inB), 0, 0);
    case EE_UNITS_GOAL_HOST_HAS_CARGO:
        return theComplex->HasCargo(ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_REPLACE_CLASS:
        if (gUnitClassTable[(long)ioArgs->i[0]] != NULL)
            EEReplaceUnit(theComplex, theComplex->mPlayer->mPlayerID, gUnitClassTable[(long)ioArgs->i[0]]);
        return 0;
    case EE_UNITS_GOAL_HOST_GENERATE:
        ioArgs->f[0] = gPhysics->mRandom.Generate();
        return 0;
    case EE_UNITS_GOAL_HOST_FIRES_OVER_WALLS:
        return theComplex->mStats->mCanFireOverWalls;
    // the build/repair goal's
    case EE_UNITS_GOAL_HOST_BUILD_UNIT: {
            EeUnitsBuildUnit* theFields = static_cast<EeUnitsBuildUnit*>(ioArgs->sink);
            EEUnitClass* theClass = theUnit->mUnitClass;
            EEPlayer* thePlayer = theUnit->mPlayer;
            EEComplexUnit* theOwn = EEUnitCast<EEComplexUnit>(theUnit);
            EEComplexUnit* theFirst = theUnit->mComplexUnit;
            if (theFirst != NULL)
                while (theFirst->mPreviousPart != NULL)
                    theFirst = theFirst->mPreviousPart;
            theFields->complex = Handle(theUnit->mComplexUnit.Get());
            theFields->unit_class = Handle(theClass);
            theFields->player = Handle(thePlayer);
            theFields->first = Handle(theFirst);
            theFields->unit_type = theClass->mUnitType;
            theFields->tile_size = theClass->mTileSize;
            theFields->category = theClass->mCategory;
            theFields->table_technology =
                gUnitClassTable[theClass->mUnitType] ? gUnitClassTable[theClass->mUnitType]->mTechnology : 100000;
            theFields->hit_points = theUnit->mHitPoints;
            theFields->max_hit_points = theUnit->mStats->mMaxHitPoints;
            theFields->tile_x = theUnit->mTileX;
            theFields->tile_y = theUnit->mTileY;
            theFields->repair_rate = theUnit->mStats->mRepairRate;
            theFields->work_rate = theUnit->mStats->mWorkRate;
            theFields->build_time_scale = theUnit->mStats->mBuildTimeScale;
            theFields->tile_node = theUnit->mTileNode != NULL;
            theFields->tile = theUnit->mTile != NULL;
            theFields->valid = theUnit->mIsValid;
            theFields->foundation = theUnit->IsFoundation();
            theFields->wall = theClass->mIsWall;
            theFields->tower = theClass->mIsTower;
            theFields->drop_site = theClass->IsDropSite();
            if (thePlayer != NULL) {
                theFields->instant_build = thePlayer->mInstantBuild;
                theFields->human = thePlayer->IsHuman();
                theFields->computer = thePlayer->mComputerPlayer != NULL;
            }
            if (theOwn != NULL) {
                theFields->next = Handle(theOwn->mNextPart);
                theFields->builders = theOwn->mNumBuilders;
                theFields->command = theOwn->mCommand ? (int32_t)theOwn->mCommand->mType : 0;
                theFields->line_of_sight = theOwn->GetLineOfSight();
                const GETransformation& theLocation = theOwn->mPhysicalModel.GetLocation();
                theFields->location[0] = theLocation.GetX();
                theFields->location[1] = theLocation.GetY();
                theFields->location[2] = theLocation.GetZ();
                theFields->built = theOwn->mIsBuilt;
                theFields->paid_for = theOwn->mIsPaidFor;
                theFields->shift_queued_build = theOwn->mHasShiftQueuedBuild;
            }
            return 0;
        }
    case EE_UNITS_GOAL_HOST_TECH_NODE:
        return Handle(ObjectOf<EEPlayer>(inA)->GetTechTree()->Get((long)ioArgs->i[0]));
    case EE_UNITS_GOAL_HOST_NODE_INFO: {
            EETechTreeNode* theNode = ObjectOf<EETechTreeNode>(inA);
            if (theNode != NULL) {
                ioArgs->i[0] = theNode->mData->mID;
                ioArgs->i[1] = theNode->mData->mKind;
                ioArgs->i[2] = theNode->GetBuildTime();
            }
            return 0;
        }
    case EE_UNITS_GOAL_HOST_PLAYER_ID:
        return inA != 0 ? (int64_t)ObjectOf<EEPlayer>(inA)->mPlayerID : 0;
    case EE_UNITS_GOAL_HOST_CLASS_UNIT_TYPE:
        return inA != 0 ? (int64_t)ObjectOf<EEUnitClass>(inA)->mUnitType : 0;
    case EE_UNITS_GOAL_HOST_BUILDERS_ADD:
        EEUnitCast<EEComplexUnit>(theUnit)->mNumBuilders += (long)ioArgs->i[0];
        return 0;
    case EE_UNITS_GOAL_HOST_ATTACK_PART:
        return Handle(EEGetAttackPart(theComplex, ObjectOf<EEComplexUnit>(inB)));
    case EE_UNITS_GOAL_HOST_IN_WORK_REACH:
        return EEIsInWorkReach(theComplex, ObjectOf<EEComplexUnit>(inB));
    case EE_UNITS_GOAL_HOST_SURROUNDING_TILES: {
            std::vector<EETileCoordinate> theTiles;
            theComplex->mUnitClass->GetSurroundingTiles(theComplex, theTiles, 1, 0);
            for (size_t i = 0; i < theTiles.size(); i++)
                ee_units_goal_push_tile(ioArgs->sink, (int32_t)theTiles[i].GetX(), (int32_t)theTiles[i].GetY());
            return 0;
        }
    case EE_UNITS_GOAL_HOST_COMPLETE_BUILDING: {
            // FinishBuilding's completion of an unbuilt building by the worker's player
            EEPlayer* thePlayer = theComplex->mPlayer;
            EEComplexUnit* theTarget = ObjectOf<EEComplexUnit>(inB);
            if (thePlayer != NULL && thePlayer->mTechnologies != NULL)
                thePlayer->mTechnologies->Research(theTarget->mUnitClass->GetTechnology(), NULL);
            thePlayer->CheckButtonsPreloaded((long)ioArgs->i[0]);
            theTarget->SetBuilt(1, 1, 1);
            if (theTarget->mUnitClass->GetUnitType() == kUnitTypeWallSpace
                && thePlayer->mTechnologies->Get(kTechWallSpace)->GetIsAvailable())
                EECreateSpaceWallEnergyWall(theTarget);
            if (thePlayer->mComputerPlayer != NULL)
                thePlayer->mComputerPlayer->mCivilizationManager->mWallManager->AddGates(theTarget);
            if (thePlayer == ::gWorld.GetCurrentPlayer()) {
                EESoundObject* theSound = theTarget->mStats->mCreationSound;
                if (theSound != NULL)
                    gSoundManager->Play(theSound->mSampleID, theSound->mVolume, 0.0f, 1.0f, false, NULL);
            }
            if (gUIManager->GetMainGameForm() != NULL)
                gUIManager->GetMainGameForm()->mRefreshGroup = true;
            return 0;
        }
    case EE_UNITS_GOAL_HOST_PLAY_EFFECT:
        theComplex->PlayEffect((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_SET_PRODUCTION_TIME:
        theComplex->SetProductionTime((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_SET_WORK_TIMER:
        theComplex->SetWorkTimer((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_FACE_TARGET:
        theComplex->FaceTarget(ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_FILL_PARTS:
        for (EEComplexUnit* thePart = theComplex->mNextPart; thePart != NULL; thePart = thePart->mNextPart)
            thePart->SetHitPoints(thePart->mStats->mMaxHitPoints);
        return 0;
    case EE_UNITS_GOAL_HOST_NEXT_FREE_TILE: {
            bool theFound = EEFindNextFreeTile(theComplex, ObjectOf<EEComplexUnit>(inB), thePoint);
            PointOut(ioArgs, thePoint);
            return theFound;
        }
    case EE_UNITS_GOAL_HOST_CANCEL_FOUNDATION: {
            // CancelFoundation: the foundation killed, its cost refunded to the worker's player
            EEComplexUnit* theWorker = ObjectOf<EEComplexUnit>(inB);
            EETechTreeNode* theNode = ObjectOf<EETechTreeNode>((uint64_t)ioArgs->i[0]);
            theComplex->Kill(0, 0, 0);
            for (unsigned long i = 2; i < 7; i++) {
                long theValue = theWorker->GetPlayer()->ScaleCost(theNode->GetCost(i, 0x20, 1.0f));
                theWorker->GetPlayer()->AddResource(i, theValue);
            }
            return 0;
        }
    case EE_UNITS_GOAL_HOST_LOCATION_GOAL:
        theComplex->SetGoal(new EEUGLocation(&thePoint, false, true, false, 0.0f, false, false), 0, 0);
        return 0;
    case EE_UNITS_GOAL_HOST_HELP_BUILD:
        EEHelpBuildNearby(theComplex, ObjectOf<EEUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_TEND_FARM:
        EETendNextFarm(theComplex, ObjectOf<EEComplexUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_GATHER_NEAR:
        EEGatherNearBuilding(theComplex, ObjectOf<EEComplexUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_GATE_SIDE_PART:
        return EEUnitClassCast<EEUCGate>(theComplex->mUnitClass)->IsSidePart(theComplex);
    case EE_UNITS_GOAL_HOST_BUILD_EVENT: {
            EEPlayer* thePlayer = ObjectOf<EEPlayer>(inA);
            EEUnit* theTarget = ObjectOf<EEUnit>(inB);
            U2DPoint<long> theTile(theTarget->mTileX, theTarget->mTileY);
            thePlayer->mEvents.AddEvent(theTile, 2, thePlayer, ioArgs->i[0] != 0 ? 0x7c42 : 0x273a, false);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_ABANDON_BUILDING:
        theComplex->mPlayer->mComputerPlayer->mCivilizationManager->mBuildManager->BuildingAbandoned(
            ObjectOf<EEComplexUnit>(inB));
        return 0;
    case EE_UNITS_GOAL_HOST_HALT:
        theComplex->SetCurrentPhysicsAction((long)ioArgs->i[0]);
        theComplex->SetVelocity(0.0f);
        theComplex->mAngularVelocity = GEOrientation();
        return 0;
    case EE_UNITS_GOAL_HOST_SITE_TAKEN:
        return EEBuildSiteTaken(theUnit, ObjectOf<EEPlayer>(inB), (long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_FOOTPRINT_CLEAR:
        return EEBuildFootprintClear(theUnit, ObjectOf<EEPlayer>(inB), (long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_SITE_FREE:
        return EEBuildSiteFree(theUnit, ObjectOf<EEPlayer>(inB), (long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_OTHER_GOAL_QUEUED: {
            // IsShiftQueueNext's queue: another goal than this one that is no automatic waypoint
            const EEGoalDeque& theGoals = theComplex->GetQueueGoals(0);
            for (EEGoalDeque::const_iterator i = theGoals.begin(); i != theGoals.end(); ++i) {
                if (*i == ObjectOf<EEUnitGoal>(inB))
                    continue;
                EEUGLocation* theLocation = (*i)->mKind == kGoalLocation ? static_cast<EEUGLocation*>(*i) : NULL;
                if (theLocation == NULL || !theLocation->GetIsAutoWaypoint())
                    return 1;
            }
            return 0;
        }
    case EE_UNITS_GOAL_HOST_RESOURCE_USERS:
        return ::gWorld.GetResourceUsers(theUnit);
    case EE_UNITS_GOAL_HOST_NEW_GATHER:
        return Handle(new EEUGGather(theUnit, ObjectOf<EEUnit>(inB), ioArgs->i[0] != 0));
    case EE_UNITS_GOAL_HOST_SET_GOAL:
        theComplex->SetGoal(ObjectOf<EEUnitGoal>(inB), (long)ioArgs->i[0], (long)ioArgs->i[1]);
        return 0;
    case EE_UNITS_GOAL_HOST_NEAREST_SITE:
        return Handle(theComplex->mPlayer->GetNearestConstructionSite(theComplex));
    case EE_UNITS_GOAL_HOST_CLEAR_SITE: {
            EETileCoordinate theTile(theUnit->mTileX, theUnit->mTileY);
            EEStepAsideFromSite(theUnit, theUnit->mUnitClass, theUnit->GetPlayer(), theTile, 0);
            return 0;
        }
    case EE_UNITS_GOAL_HOST_CAN_PRODUCE:
        return theUnit->mPlayer->CanProduce(ObjectOf<EEUnit>(inB)->mUnitClass->GetTechnology(), 0x20, 1, true, 3, NULL);
    case EE_UNITS_GOAL_HOST_CAN_AFFORD_REPAIR:
        return theComplex->mPlayer->CanAffordRepair(ObjectOf<EEUnit>(inB), theComplex, (unsigned long)ioArgs->i[0]);
    case EE_UNITS_GOAL_HOST_FRIENDLY: {
            EEPlayer* thePlayer = ObjectOf<EEPlayer>(inA);
            EEComplexUnit* theNew = ObjectOf<EEComplexUnit>(inB);
            return theNew->mPlayer == thePlayer || thePlayer->mEnemies[theNew->mPlayer->mPlayerID] == 0;
        }
    case EE_UNITS_GOAL_HOST_IN_CINEMATIC:
        return ::gWorld.mInCinematic;
    case EE_UNITS_GOAL_HOST_SET_PAID:
        theComplex->mIsPaidFor = true;
        return 0;
    case EE_UNITS_GOAL_HOST_PAY:
        ObjectOf<EEPlayer>(inA)->Pay(ObjectOf<EETechTreeNode>(inB), 0x20, ioArgs->f[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_SET_HIT_POINTS:
        theComplex->SetHitPoints((long)ioArgs->i[0]);
        return 0;
    case EE_UNITS_GOAL_HOST_FOUNDATION_STARTED: {
            // Work's foundation become a building site: the wonder's notice and clock, the media, the parts
            EEUnit* theTarget = ObjectOf<EEUnit>(inB);
            if (ioArgs->i[0] != 0) {
                if (theComplex->mPlayer != ::gWorld.GetCurrentPlayer()) {
                    UWideString theFormat;
                    UWideString theName;
                    UWideString theText;
                    gResource->GetString(IDS_HAS_STARTED_CONSTRUCTING_WONDER, theFormat);
                    gResource->GetString(theTarget->mUnitClass->mNameStringID, theName);
                    sprintf(theText, (const char*)theFormat, (const char*)theComplex->mPlayer->mName,
                            (const char*)theName);
                    gUserInterface->AddChatLogEntry(theText, 1);
                }
                EESoundObject* theSound = EESoundObject::Get(kSoundWonderClock);
                if (theSound != NULL)
                    theSound->Play();
            }
            theComplex->mPlayer->LoadUnitClassMedia(theTarget->mUnitClass->GetUnitType());
            theComplex->LoadMedia(false);
            gDataPreLoading->PostUnloadUnitClass(theComplex->mPlayer->mPlayerID, theComplex->mUnitClass->GetUnitType());
            for (EEComplexUnit* thePart = theComplex->mNextPart; thePart != NULL; thePart = thePart->mNextPart) {
                thePart->SetHitPoints(2);
                thePart->mPlayer->LoadUnitClassMedia(thePart->mUnitClass->GetUnitType());
                thePart->LoadMedia(false);
                gDataPreLoading->PostUnloadUnitClass(thePart->mPlayer->mPlayerID, thePart->mUnitClass->GetUnitType());
                thePart->PartFinished();
            }
            theComplex->PartFinished();
            return 0;
        }
    case EE_UNITS_GOAL_HOST_GOAL_ARG:
        return ObjectOf<EEUnitGoal>(inA)->GetB1C();
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

EEUGRust* EEUGRust::NewUnloadTransport(EEComplexUnit* inTransport, long inX, long inY, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalUnloadTransport, inArg);
    EeUnitsGoalIo theArgs = {{inX, inY}};
    Goal(EE_UNITS_GOAL_NEW_UNLOAD, theGoal, Handle(inTransport), 0, &theArgs);
    return theGoal;
}

EEUGRust* EEUGRust::NewUnloadTransport(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalUnloadTransport, inArg);
    Goal(EE_UNITS_GOAL_UNLOAD_NEW_EMPTY, theGoal);
    return theGoal;
}

EEComplexUnit* EEUGRust::GetUnloadTransport(long* outX, long* outY)
{
    EeUnitsGoalIo theArgs = {};
    EEComplexUnit* theTransport =
        ObjectOf<EEComplexUnit>((uint64_t)Goal(EE_UNITS_GOAL_UNLOAD_FIELDS, this, 0, 0, &theArgs));
    if (outX != NULL)
        *outX = (long)theArgs.i[0];
    if (outY != NULL)
        *outY = (long)theArgs.i[1];
    return theTransport;
}

void EEUGRust::SetUnloadTransport(EEComplexUnit* inTransport)
{
    Goal(EE_UNITS_GOAL_UNLOAD_SET_TRANSPORT, this, Handle(inTransport));
}

bool EEUGRust::CanUnloadAtDestination()
{
    return Goal(EE_UNITS_GOAL_UNLOAD_CAN_UNLOAD, this) != 0;
}

void EEUGRust::ForgetUnloadTransport()
{
    Goal(EE_UNITS_GOAL_UNLOAD_FORGET_TRANSPORT, this);
}

void EEUGRust::SetUnloadDestination(long inX, long inY)
{
    EeUnitsGoalIo theArgs = {{inX, inY}};
    Goal(EE_UNITS_GOAL_UNLOAD_SET_DESTINATION, this, 0, 0, &theArgs);
}

EEUGRust* EEUGRust::NewBuildRepair(EEPlayer* inPlayer, EEUnit* inTarget, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalBuildRepair, inArg);
    Goal(EE_UNITS_GOAL_NEW_BUILD, theGoal, Handle(inPlayer), Handle(inTarget));
    return theGoal;
}

EEUGRust* EEUGRust::NewBuildRepair(bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(kGoalBuildRepair, inArg);
    Goal(EE_UNITS_GOAL_BUILD_NEW_EMPTY, theGoal);
    return theGoal;
}

EEUnit* EEUGRust::GetBuildTarget()
{
    return ObjectOf<EEUnit>((uint64_t)Goal(EE_UNITS_GOAL_BUILD_FIELDS, this));
}

long EEUGRust::GetBuildTargetType()
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_BUILD_FIELDS, this, 0, 0, &theArgs);
    return (long)theArgs.i[0];
}

long EEUGRust::GetBuildTechTreeID()
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_BUILD_FIELDS, this, 0, 0, &theArgs);
    return (long)theArgs.i[1];
}

EETechTreeNode* EEUGRust::GetBuildTechNode()
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_BUILD_FIELDS, this, 0, 0, &theArgs);
    return ObjectOf<EETechTreeNode>((uint64_t)theArgs.i[2]);
}

EEUnitClass* EEUGRust::GetBuildTargetClass()
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_BUILD_FIELDS, this, 0, 0, &theArgs);
    return ObjectOf<EEUnitClass>((uint64_t)theArgs.i[3]);
}

bool EEUGRust::IsBuildSiteTaken()
{
    return Goal(EE_UNITS_GOAL_BUILD_SITE_TAKEN, this) != 0;
}

void EEUGRust::Work(EEComplexUnit* inUnit)
{
    Goal(EE_UNITS_GOAL_WORK, this, Handle(inUnit));
}

EEUGRust* EEUGRust::NewLoad(unsigned char inKind, EEComplexUnit* inContainer, std::vector<EEComplexUnit*>& inUnits,
                            bool inArg)
{
    std::sort(inUnits.begin(), inUnits.end(),
              [](EEComplexUnit* inFirst, EEComplexUnit* inSecond) { return inFirst->mUnitID < inSecond->mUnitID; });
    inUnits.erase(std::unique(inUnits.begin(), inUnits.end()), inUnits.end());
    EEUGRust* theGoal = new EEUGRust(inKind, inArg);
    std::vector<uint64_t> theUnits;
    for (size_t i = 0; i < inUnits.size(); i++)
        theUnits.push_back(Handle(inUnits[i]));
    EeUnitsGoalIo theArgs = {};
    theArgs.list = theUnits.data();
    theArgs.list_len = theUnits.size();
    Goal(EE_UNITS_GOAL_NEW_LOAD, theGoal, Handle(inContainer), 0, &theArgs);
    return theGoal;
}

EEUGRust* EEUGRust::NewLoad(unsigned char inKind, bool inArg)
{
    EEUGRust* theGoal = new EEUGRust(inKind, inArg);
    Goal(EE_UNITS_GOAL_LOAD_NEW_EMPTY, theGoal);
    return theGoal;
}

EEComplexUnit* EEUGRust::GetLoadContainer()
{
    return ObjectOf<EEComplexUnit>((uint64_t)Goal(EE_UNITS_GOAL_LOAD_FIELDS, this));
}

U2DPoint<long> EEUGRust::GetLoadContainerTile()
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_LOAD_FIELDS, this, 0, 0, &theArgs);
    return U2DPoint<long>((long)theArgs.i[0], (long)theArgs.i[1]);
}

U2DPoint<long> EEUGRust::GetLoadUnitTile()
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_LOAD_FIELDS, this, 0, 0, &theArgs);
    return U2DPoint<long>((long)theArgs.i[2], (long)theArgs.i[3]);
}

bool EEUGRust::IsLoadGarrisonValid()
{
    EeUnitsGoalIo theArgs = {};
    Goal(EE_UNITS_GOAL_LOAD_FIELDS, this, 0, 0, &theArgs);
    return theArgs.i[4] != 0;
}

unsigned long EEUGRust::GetNumLoadUnits()
{
    return (unsigned long)Goal(EE_UNITS_GOAL_LOAD_UNIT_COUNT, this);
}

EEComplexUnit* EEUGRust::GetLoadUnitAt(unsigned long inIndex)
{
    return ObjectOf<EEComplexUnit>((uint64_t)Goal(EE_UNITS_GOAL_LOAD_UNIT_AT, this, inIndex));
}

bool EEUGRust::HasLoadUnit(EEComplexUnit* inUnit)
{
    return Goal(EE_UNITS_GOAL_LOAD_HAS_UNIT, this, Handle(inUnit)) != 0;
}

void EEUGRust::AddLoadUnit(EEComplexUnit* inUnit)
{
    Goal(EE_UNITS_GOAL_LOAD_ADD_UNIT, this, Handle(inUnit));
}

void EEUGRust::AddLoadUnits(const std::vector<EEComplexUnit*>& inUnits)
{
    for (size_t i = 0; i < inUnits.size(); i++)
        this->AddLoadUnit(inUnits[i]);
}

void EEUGRust::AddCreatedUnit(EEComplexUnit* inUnit)
{
    Goal(EE_UNITS_GOAL_ADD_CREATED_UNIT, this, Handle(inUnit));
}

bool EEUGRust::IsLoadGoal()
{
    return this->mKind == kGoalLoadTransport || this->mKind == kGoalLoadGarrison;
}

void EEUGRust::Abandon(EEComplexUnit* inUnit)
{
    Goal(EE_UNITS_GOAL_ABANDON, this, Handle(inUnit));
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

long EEUGRust::GetMaxPathAttempts()
{
    return (long)Goal(EE_UNITS_GOAL_MAX_PATH_ATTEMPTS, this);
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
    return (long)Goal(EE_UNITS_GOAL_LOAD, this, inMajorVersion, inMinorVersion, NULL, &inFile);
}

long EEUGRust::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return (long)Goal(EE_UNITS_GOAL_REPAIR, this, inMajorVersion, inMinorVersion);
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
    return (long)Goal(EE_UNITS_GOAL_SAVE, this, inMajorVersion, inMinorVersion, NULL, &outFile);
}

bool EEUGRust::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    return Goal(EE_UNITS_GOAL_EVALUATE, this, Handle(inUnit), inEvent) != 0;
}

void EEUGRust::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit* inUnit)
{
    EeUnitsGoalIo theArgs = {{(int64_t)Handle(inUnit)}};
    Goal(EE_UNITS_GOAL_REPLACE_UNIT, this, Handle(inOld), Handle(inNew), &theArgs);
}

bool EEUGRust::Suits(EEUnit* inUnit)
{
    return Goal(EE_UNITS_GOAL_SUITS, this, Handle(inUnit)) != 0;
}
